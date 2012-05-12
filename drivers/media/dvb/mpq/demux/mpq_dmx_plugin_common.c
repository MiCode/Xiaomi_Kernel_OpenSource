/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include "mpq_dvb_debug.h"
#include "mpq_dmx_plugin_common.h"


/* Length of mandatory fields that must exist in header of video PES */
#define PES_MANDATORY_FIELDS_LEN			9


/*
 * 500 PES header packets in the meta-data buffer,
 * should be more than enough
 */
#define VIDEO_NUM_OF_PES_PACKETS			500

#define VIDEO_META_DATA_BUFFER_SIZE              \
	(VIDEO_NUM_OF_PES_PACKETS *                  \
	  (DVB_RINGBUFFER_PKTHDRSIZE +               \
	   sizeof(struct mpq_streambuffer_packet_header) + \
	   sizeof(struct mpq_adapter_video_meta_data)))

/*
 * The following threshold defines gap from end of ring-buffer
 * from which new PES payload will not be written to make
 * sure that the PES payload does not wrap-around at end of the
 * buffer. Instead, padding will be inserted and the new PES will
 * be written from the beginning of the buffer.
 * Setting this to 0 means no padding will be added.
 */
#define VIDEO_WRAP_AROUND_THRESHOLD			(1024*1024+512*1024)

/*
 * PCR/STC information length saved in ring-buffer.
 * PCR / STC are saved in ring-buffer in the following form:
 * <8 bit flags><64 bits of STC> <64bits of PCR>
 * STC and PCR values are in 27MHz.
 * The current flags that are defined:
 * 0x00000001: discontinuity_indicator
 */
#define PCR_STC_LEN							17


/* Number of demux devices, has default of linux configuration */
static int mpq_demux_device_num = CONFIG_DVB_MPQ_NUM_DMX_DEVICES;
module_param(mpq_demux_device_num, int, S_IRUGO);


/* Global data-structure for managing demux devices */
static struct
{
	/* ION demux client used for memory allocation */
	struct ion_client *ion_client;

	/* demux devices array */
	struct mpq_demux *devices;

	/* Stream buffers objects used for tunneling to decoders */
	struct mpq_streambuffer
		decoder_buffers[MPQ_ADAPTER_MAX_NUM_OF_INTERFACES];

	/*
	 * Indicates whether we allow decoder's data to
	 * wrap-around in the output buffer or padding is
	 * inserted in such case.
	 */
	int decoder_data_wrap;
} mpq_dmx_info;


/* Check that PES header is valid and that it is a video PES */
static int mpq_dmx_is_valid_video_pes(struct pes_packet_header *pes_header)
{
	/* start-code valid? */
	if ((pes_header->packet_start_code_prefix_1 != 0) ||
		(pes_header->packet_start_code_prefix_2 != 0) ||
		(pes_header->packet_start_code_prefix_3 != 1))
		return -EINVAL;

	/* stream_id is video? */
	if ((pes_header->stream_id & 0xF0) != 0xE0)
		return -EINVAL;

	return 0;
}


/* Extend dvb-demux debugfs with HW statistics */
void mpq_dmx_init_hw_statistics(struct mpq_demux *mpq_demux)
{
	/*
	 * Extend dvb-demux debugfs with HW statistics.
	 * Note that destruction of debugfs directory is done
	 * when dvb-demux is terminated.
	 */
	mpq_demux->hw_notification_count = 0;
	mpq_demux->hw_notification_rate = 0;
	mpq_demux->hw_notification_size = 0;
	mpq_demux->decoder_tsp_drop_count = 0;

	if (mpq_demux->demux.debugfs_demux_dir != NULL) {
		debugfs_create_u32(
			"hw_notification_rate",
			S_IRUGO|S_IWUGO,
			mpq_demux->demux.debugfs_demux_dir,
			&mpq_demux->hw_notification_rate);

		debugfs_create_u32(
			"hw_notification_count",
			S_IRUGO|S_IWUGO,
			mpq_demux->demux.debugfs_demux_dir,
			&mpq_demux->hw_notification_count);

		debugfs_create_u32(
			"hw_notification_size",
			S_IRUGO|S_IWUGO,
			mpq_demux->demux.debugfs_demux_dir,
			&mpq_demux->hw_notification_size);

		debugfs_create_u32(
			"decoder_tsp_drop_count",
			S_IRUGO|S_IWUGO,
			mpq_demux->demux.debugfs_demux_dir,
			&mpq_demux->decoder_tsp_drop_count);
	}
}
EXPORT_SYMBOL(mpq_dmx_init_hw_statistics);


/* Update dvb-demux debugfs with HW notification statistics */
void mpq_dmx_update_hw_statistics(struct mpq_demux *mpq_demux)
{
	struct timespec curr_time, delta_time;
	u64 delta_time_ms;

	curr_time = current_kernel_time();
	if (likely(mpq_demux->hw_notification_count)) {
		/* calculate time-delta between notifications */
		delta_time =
			timespec_sub(
					curr_time,
					mpq_demux->last_notification_time);

		delta_time_ms = (u64)timespec_to_ns(&delta_time);
		delta_time_ms = div64_u64(delta_time_ms, 1000000); /* ns->ms */

		mpq_demux->hw_notification_rate = delta_time_ms;
	}

	mpq_demux->hw_notification_count++;
	mpq_demux->last_notification_time = curr_time;
}
EXPORT_SYMBOL(mpq_dmx_update_hw_statistics);


int mpq_dmx_plugin_init(mpq_dmx_init dmx_init_func)
{
	int i;
	int result;
	struct mpq_demux *mpq_demux;
	struct dvb_adapter *mpq_adapter;

	MPQ_DVB_DBG_PRINT("%s executed, device num %d\n",
					  __func__,
					  mpq_demux_device_num);

	mpq_adapter = mpq_adapter_get();

	if (mpq_adapter == NULL) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_adapter is not valid\n",
			__func__);
		result = -EPERM;
		goto init_failed;
	}

	if (mpq_demux_device_num == 0) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_demux_device_num set to 0\n",
			__func__);

		result = -EPERM;
		goto init_failed;
	}

	mpq_dmx_info.devices = NULL;
	mpq_dmx_info.ion_client = NULL;

	/* TODO: the following should be set based on the decoder */
	mpq_dmx_info.decoder_data_wrap = 0;

	/* Allocate memory for all MPQ devices */
	mpq_dmx_info.devices =
		vmalloc(mpq_demux_device_num*sizeof(struct mpq_demux));

	if (!mpq_dmx_info.devices) {
		MPQ_DVB_ERR_PRINT(
				"%s: failed to allocate devices memory\n",
				__func__);

		result = -ENOMEM;
		goto init_failed;
	}

	/* Zero allocated memory */
	memset(mpq_dmx_info.devices,
		   0,
		   mpq_demux_device_num*sizeof(struct mpq_demux));

	/*
	 * Create a new ION client used by demux to allocate memory
	 * for decoder's buffers.
	 */
	mpq_dmx_info.ion_client =
		msm_ion_client_create(UINT_MAX, "demux client");

	if (IS_ERR_OR_NULL(mpq_dmx_info.ion_client)) {
		MPQ_DVB_ERR_PRINT(
				"%s: msm_ion_client_create\n",
				__func__);

		result = PTR_ERR(mpq_dmx_info.ion_client);
		mpq_dmx_info.ion_client = NULL;
		goto init_failed_free_demux_devices;
	}

	/* Initialize and register all demux devices to the system */
	for (i = 0; i < mpq_demux_device_num; i++) {
		mpq_demux = mpq_dmx_info.devices+i;

		/* initialize demux source to memory by default */
		mpq_demux->source = DMX_SOURCE_DVR0 + i;

		/*
		 * Give the plugin pointer to the ion client so
		 * that it can allocate memory from ION if it requires so
		 */
		mpq_demux->ion_client = mpq_dmx_info.ion_client;

		spin_lock_init(&mpq_demux->feed_lock);

		/*
		 * mpq_demux_plugin_hw_init should be implemented
		 * by the specific plugin
		 */
		result = dmx_init_func(mpq_adapter, mpq_demux);
		if (result < 0) {
			MPQ_DVB_ERR_PRINT(
				"%s: dmx_init_func (errno=%d)\n",
				__func__,
				result);

			goto init_failed_free_demux_devices;
		}

		mpq_demux->is_initialized = 1;

		/*
		 * Add capability of receiving input from memory.
		 * Every demux in our system may be connected to memory input,
		 * or any live input.
		 */
		mpq_demux->fe_memory.source = DMX_MEMORY_FE;
		result =
			mpq_demux->demux.dmx.add_frontend(
					&mpq_demux->demux.dmx,
					&mpq_demux->fe_memory);

		if (result < 0) {
			MPQ_DVB_ERR_PRINT(
				"%s: add_frontend (mem) failed (errno=%d)\n",
				__func__,
				result);

			goto init_failed_free_demux_devices;
		}
	}

	return 0;

init_failed_free_demux_devices:
	mpq_dmx_plugin_exit();
init_failed:
	return result;
}
EXPORT_SYMBOL(mpq_dmx_plugin_init);


void mpq_dmx_plugin_exit(void)
{
	int i;
	struct mpq_demux *mpq_demux;

	MPQ_DVB_DBG_PRINT("%s executed\n", __func__);

	if (mpq_dmx_info.ion_client != NULL) {
		ion_client_destroy(mpq_dmx_info.ion_client);
		mpq_dmx_info.ion_client = NULL;
	}

	if (mpq_dmx_info.devices != NULL) {
		for (i = 0; i < mpq_demux_device_num; i++) {
			mpq_demux = mpq_dmx_info.devices+i;

			if (mpq_demux->is_initialized) {
				mpq_demux->demux.dmx.remove_frontend(
							&mpq_demux->demux.dmx,
							&mpq_demux->fe_memory);

				dvb_dmxdev_release(&mpq_demux->dmxdev);
				dvb_dmx_release(&mpq_demux->demux);
			}
		}

		vfree(mpq_dmx_info.devices);
		mpq_dmx_info.devices = NULL;
	}
}
EXPORT_SYMBOL(mpq_dmx_plugin_exit);


int mpq_dmx_set_source(
		struct dmx_demux *demux,
		const dmx_source_t *src)
{
	int i;
	int dvr_index;
	int dmx_index;
	struct mpq_demux *mpq_demux = (struct mpq_demux *)demux->priv;

	if ((mpq_dmx_info.devices == NULL) || (mpq_demux == NULL)) {
		MPQ_DVB_ERR_PRINT("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	/*
	 * For dvr sources,
	 * verify that this source is connected to the respective demux
	 */
	dmx_index = mpq_demux - mpq_dmx_info.devices;

	if (*src >= DMX_SOURCE_DVR0) {
		dvr_index = *src - DMX_SOURCE_DVR0;

		if (dvr_index != dmx_index) {
			MPQ_DVB_ERR_PRINT(
				"%s: can't connect demux%d to dvr%d\n",
				__func__,
				dmx_index,
				dvr_index);
			return -EINVAL;
		}
	}

	/*
	 * For front-end sources,
	 * verify that this source is not already set to different demux
	 */
	for (i = 0; i < mpq_demux_device_num; i++) {
		if ((&mpq_dmx_info.devices[i] != mpq_demux) &&
			(mpq_dmx_info.devices[i].source == *src)) {
			MPQ_DVB_ERR_PRINT(
				"%s: demux%d source can't be set,\n"
				"demux%d occupies this source already\n",
				__func__,
				dmx_index,
				i);
			return -EBUSY;
		}
	}

	mpq_demux->source = *src;
	return 0;
}
EXPORT_SYMBOL(mpq_dmx_set_source);


int mpq_dmx_init_video_feed(struct dvb_demux_feed *feed)
{
	int ret;
	void *packet_buffer;
	void *payload_buffer;
	struct mpq_video_feed_info *feed_data;
	struct mpq_demux *mpq_demux =
		(struct mpq_demux *)feed->demux->priv;
	struct mpq_streambuffer *stream_buffer;
	int actual_buffer_size;

	/* Allocate memory for private feed data */
	feed_data = vmalloc(sizeof(struct mpq_video_feed_info));

	if (feed_data == NULL) {
		MPQ_DVB_ERR_PRINT(
			"%s: FAILED to private video feed data\n",
			__func__);

		ret = -ENOMEM;
		goto init_failed;
	}

	/* Allocate packet buffer holding the meta-data */
	packet_buffer = vmalloc(VIDEO_META_DATA_BUFFER_SIZE);

	if (packet_buffer == NULL) {
		MPQ_DVB_ERR_PRINT(
			"%s: FAILED to allocate packets buffer\n",
			__func__);

		ret = -ENOMEM;
		goto init_failed_free_priv_data;
	}

	/*
	 * Allocate payload buffer through ION.
	 * TODO: for scrambling support, need to check if the
	 * stream is scrambled and allocate the buffer with secure
	 * flag set.
	 */

	if (mpq_dmx_info.decoder_data_wrap)
		actual_buffer_size =
			feed->buffer_size;
	else
		actual_buffer_size =
			feed->buffer_size + VIDEO_WRAP_AROUND_THRESHOLD;

	actual_buffer_size += (SZ_4K - 1);
	actual_buffer_size &= ~(SZ_4K - 1);

	feed_data->payload_buff_handle =
		ion_alloc(mpq_demux->ion_client,
				  actual_buffer_size,
				  SZ_4K,
				  ION_HEAP(ION_CP_MM_HEAP_ID));

	if (IS_ERR_OR_NULL(feed_data->payload_buff_handle)) {
		ret = PTR_ERR(feed_data->payload_buff_handle);

		MPQ_DVB_ERR_PRINT(
			"%s: FAILED to allocate payload buffer %d\n",
			__func__,
			ret);

		goto init_failed_free_packet_buffer;
	}

	payload_buffer =
		ion_map_kernel(mpq_demux->ion_client,
					   feed_data->payload_buff_handle,
					   0);

	if (IS_ERR_OR_NULL(payload_buffer)) {
		ret = PTR_ERR(payload_buffer);

		MPQ_DVB_ERR_PRINT(
			"%s: FAILED to map payload buffer %d\n",
			__func__,
			ret);

		goto init_failed_free_payload_buffer;
	}

	/* Register the new stream-buffer interface to MPQ adapter */
	switch (feed->pes_type) {
	case DMX_TS_PES_VIDEO0:
		feed_data->stream_interface =
			MPQ_ADAPTER_VIDEO0_STREAM_IF;
		break;

	case DMX_TS_PES_VIDEO1:
		feed_data->stream_interface =
			MPQ_ADAPTER_VIDEO1_STREAM_IF;
		break;

	case DMX_TS_PES_VIDEO2:
		feed_data->stream_interface =
			MPQ_ADAPTER_VIDEO2_STREAM_IF;
		break;

	case DMX_TS_PES_VIDEO3:
		feed_data->stream_interface =
			MPQ_ADAPTER_VIDEO3_STREAM_IF;
		break;

	default:
		MPQ_DVB_ERR_PRINT(
			"%s: Invalid pes type %d\n",
			__func__,
			feed->pes_type);
		ret = -EINVAL;
		goto init_failed_unmap_payload_buffer;
	}

	/* make sure not occupied already */
	stream_buffer = NULL;
	mpq_adapter_get_stream_if(
			feed_data->stream_interface,
			&stream_buffer);
	if (stream_buffer != NULL) {
		MPQ_DVB_ERR_PRINT(
			"%s: Video interface %d already occupied!\n",
			__func__,
			feed_data->stream_interface);
		ret = -EBUSY;
		goto init_failed_unmap_payload_buffer;
	}

	feed_data->video_buffer =
		&mpq_dmx_info.decoder_buffers[feed_data->stream_interface];

	mpq_streambuffer_init(
			feed_data->video_buffer,
			payload_buffer,
			actual_buffer_size,
			packet_buffer,
			VIDEO_META_DATA_BUFFER_SIZE);

	ret =
		mpq_adapter_register_stream_if(
			feed_data->stream_interface,
			feed_data->video_buffer);

	if (ret < 0) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_adapter_register_stream_if failed, "
			"err = %d\n",
			__func__,
			ret);
		goto init_failed_unmap_payload_buffer;
	}

	feed->buffer_size = actual_buffer_size;
	feed_data->pes_payload_address =
		(u32)feed_data->video_buffer->raw_data.data;

	feed_data->pes_header_left_bytes = PES_MANDATORY_FIELDS_LEN;
	feed_data->pes_header_offset = 0;
	feed->pusi_seen = 0;
	feed->peslen = 0;
	feed_data->fullness_wait_cancel = 0;

	spin_lock(&mpq_demux->feed_lock);
	feed->priv = (void *)feed_data;
	spin_unlock(&mpq_demux->feed_lock);

	return 0;

init_failed_unmap_payload_buffer:
	ion_unmap_kernel(mpq_demux->ion_client,
					 feed_data->payload_buff_handle);
init_failed_free_payload_buffer:
	ion_free(mpq_demux->ion_client,
			feed_data->payload_buff_handle);
init_failed_free_packet_buffer:
	vfree(packet_buffer);
init_failed_free_priv_data:
	vfree(feed_data);
	feed->priv = NULL;
init_failed:

	return ret;
}
EXPORT_SYMBOL(mpq_dmx_init_video_feed);


int mpq_dmx_terminate_video_feed(struct dvb_demux_feed *feed)
{
	struct mpq_video_feed_info *feed_data;
	struct mpq_demux *mpq_demux;

	if (feed->priv == NULL) {
		MPQ_DVB_ERR_PRINT(
			"%s: invalid feed, feed->priv is NULL\n",
			__func__);

		return -EINVAL;
	}

	mpq_demux =
		(struct mpq_demux *)feed->demux->priv;

	feed_data =
		(struct mpq_video_feed_info *)feed->priv;

	spin_lock(&mpq_demux->feed_lock);
	feed->priv = NULL;
	spin_unlock(&mpq_demux->feed_lock);

	wake_up_all(&feed_data->video_buffer->raw_data.queue);

	mpq_adapter_unregister_stream_if(
		feed_data->stream_interface);

	vfree(feed_data->video_buffer->packet_data.data);

	ion_unmap_kernel(mpq_demux->ion_client,
					 feed_data->payload_buff_handle);

	ion_free(mpq_demux->ion_client,
			 feed_data->payload_buff_handle);

	vfree(feed_data);

	return 0;
}
EXPORT_SYMBOL(mpq_dmx_terminate_video_feed);

int mpq_dmx_decoder_fullness_init(struct dvb_demux_feed *feed)
{
	struct mpq_demux *mpq_demux;

	mpq_demux =
		(struct mpq_demux *)feed->demux->priv;

	if (mpq_dmx_is_video_feed(feed)) {
		struct mpq_video_feed_info *feed_data;

		spin_lock(&mpq_demux->feed_lock);

		if (feed->priv == NULL) {
			MPQ_DVB_ERR_PRINT(
				"%s: invalid feed, feed->priv is NULL\n",
				__func__);
			spin_unlock(&mpq_demux->feed_lock);
			return -EINVAL;
		}

		feed_data =
			(struct mpq_video_feed_info *)feed->priv;

		feed_data->fullness_wait_cancel = 0;

		spin_unlock(&mpq_demux->feed_lock);

		return 0;
	}

	/* else */
	MPQ_DVB_DBG_PRINT(
		"%s: Invalid feed type %d\n",
		__func__,
		feed->pes_type);

	return -EINVAL;
}
EXPORT_SYMBOL(mpq_dmx_decoder_fullness_init);

int mpq_dmx_decoder_fullness_wait(
		struct dvb_demux_feed *feed,
		size_t required_space)
{
	struct mpq_demux *mpq_demux;

	mpq_demux =
		(struct mpq_demux *)feed->demux->priv;

	if (mpq_dmx_is_video_feed(feed)) {
		int ret;
		int gap;
		struct mpq_video_feed_info *feed_data;
		struct dvb_ringbuffer *video_buff;

		spin_lock(&mpq_demux->feed_lock);

		if (feed->priv == NULL) {
			spin_unlock(&mpq_demux->feed_lock);
			return -EINVAL;
		}

		feed_data =
			(struct mpq_video_feed_info *)feed->priv;

		video_buff =
			&feed_data->video_buffer->raw_data;

		/*
		 * If we are now starting new PES and the
		 * PES payload may wrap-around, extra padding
		 * needs to be pushed into the buffer.
		 */
		gap = video_buff->size - video_buff->pwrite;
		if ((!mpq_dmx_info.decoder_data_wrap) &&
			(gap < VIDEO_WRAP_AROUND_THRESHOLD))
			required_space += gap;

		ret = 0;
		if ((feed_data != NULL) &&
			(!feed_data->fullness_wait_cancel) &&
			(dvb_ringbuffer_free(video_buff) < required_space)) {
			DEFINE_WAIT(__wait);
			for (;;) {
				prepare_to_wait(
					&video_buff->queue,
					&__wait,
					TASK_INTERRUPTIBLE);

				if ((feed->priv == NULL) ||
					(feed_data->fullness_wait_cancel) ||
					(dvb_ringbuffer_free(video_buff) >=
					required_space))
					break;

				if (!signal_pending(current)) {
					spin_unlock(&mpq_demux->feed_lock);
					schedule();
					spin_lock(&mpq_demux->feed_lock);
					continue;
				}
				ret = -ERESTARTSYS;
				break;
			}
			finish_wait(&video_buff->queue, &__wait);
		}

		if (ret < 0) {
			spin_unlock(&mpq_demux->feed_lock);
			return ret;
		}

		if ((feed->priv == NULL) ||
			(feed_data->fullness_wait_cancel)) {
			spin_unlock(&mpq_demux->feed_lock);
			return -EINVAL;
		}

		spin_unlock(&mpq_demux->feed_lock);
		return 0;
	}

	/* else */
	MPQ_DVB_DBG_PRINT(
		"%s: Invalid feed type %d\n",
		__func__,
		feed->pes_type);

	return -EINVAL;
}
EXPORT_SYMBOL(mpq_dmx_decoder_fullness_wait);

int mpq_dmx_decoder_fullness_abort(struct dvb_demux_feed *feed)
{
	struct mpq_demux *mpq_demux;

	mpq_demux =
		(struct mpq_demux *)feed->demux->priv;

	if (mpq_dmx_is_video_feed(feed)) {
		struct mpq_video_feed_info *feed_data;
		struct dvb_ringbuffer *video_buff;

		spin_lock(&mpq_demux->feed_lock);

		if (feed->priv == NULL) {
			MPQ_DVB_ERR_PRINT(
				"%s: invalid feed, feed->priv is NULL\n",
				__func__);
			spin_unlock(&mpq_demux->feed_lock);
			return -EINVAL;
		}

		feed_data =
			(struct mpq_video_feed_info *)feed->priv;

		video_buff =
			&feed_data->video_buffer->raw_data;

		feed_data->fullness_wait_cancel = 1;
		spin_unlock(&mpq_demux->feed_lock);

		wake_up_all(&video_buff->queue);

		return 0;
	}

	/* else */
	MPQ_DVB_ERR_PRINT(
		"%s: Invalid feed type %d\n",
		__func__,
		feed->pes_type);

	return -EINVAL;
}
EXPORT_SYMBOL(mpq_dmx_decoder_fullness_abort);

int mpq_dmx_process_video_packet(
			struct dvb_demux_feed *feed,
			const u8 *buf)
{
	int bytes_avail;
	int left_size;
	int copy_len;
	u32 ts_payload_offset;
	struct mpq_video_feed_info *feed_data;
	const struct ts_packet_header *ts_header;
	struct mpq_streambuffer *stream_buffer;
	struct pes_packet_header *pes_header;
	struct mpq_demux *mpq_demux;

	mpq_demux =
		(struct mpq_demux *)feed->demux->priv;

	spin_lock(&mpq_demux->feed_lock);

	feed_data =
		(struct mpq_video_feed_info *)feed->priv;

	if (unlikely(feed_data == NULL)) {
		spin_unlock(&mpq_demux->feed_lock);
		return 0;
	}

	ts_header = (const struct ts_packet_header *)buf;

	stream_buffer =
			feed_data->video_buffer;

	pes_header =
			&feed_data->pes_header;

/*	printk("TS packet: %X %X %X %X %X%X %X %X %X\n",
		ts_header->sync_byte,
		ts_header->transport_error_indicator,
		ts_header->payload_unit_start_indicator,
		ts_header->transport_priority,
		ts_header->pid_msb,
		ts_header->pid_lsb,
		ts_header->transport_scrambling_control,
		ts_header->adaptation_field_control,
		ts_header->continuity_counter);*/

	/* Make sure this TS packet has a payload and not scrambled */
	if ((ts_header->sync_byte != 0x47) ||
		(ts_header->adaptation_field_control == 0) ||
		(ts_header->adaptation_field_control == 2) ||
		(ts_header->transport_scrambling_control)) {
		/* continue to next packet */
		spin_unlock(&mpq_demux->feed_lock);
		return 0;
	}

	if (ts_header->payload_unit_start_indicator) { /* PUSI? */
		if (feed->pusi_seen) { /* Did we see PUSI before? */
			struct mpq_streambuffer_packet_header packet;
			struct mpq_adapter_video_meta_data meta_data;

			/*
			 * Close previous PES.
			 * Push new packet to the meta-data buffer.
			 * Double check that we are not in middle of
			 * previous PES header parsing.
			 */

			if (0 == feed_data->pes_header_left_bytes) {
				packet.raw_data_addr =
					feed_data->pes_payload_address;

				packet.raw_data_len = feed->peslen;

				if ((!mpq_dmx_info.decoder_data_wrap) &&
					((feed_data->pes_payload_address +
					feed->peslen) >
					((u32)stream_buffer->raw_data.data +
					stream_buffer->raw_data.size)))
					MPQ_DVB_ERR_PRINT(
						"%s: "
						"Video data has wrapped-around!\n",
						__func__);

				packet.user_data_len =
					sizeof(struct
						mpq_adapter_video_meta_data);

				if ((pes_header->pts_dts_flag == 2) ||
					(pes_header->pts_dts_flag == 3))
					meta_data.pts_exist = 1;
				else
					meta_data.pts_exist = 0;

				meta_data.pts =
					((u64)pes_header->pts_1 << 30) |
					((u64)pes_header->pts_2 << 22) |
					((u64)pes_header->pts_3 << 15) |
					((u64)pes_header->pts_4 << 7) |
					(u64)pes_header->pts_5;

				if (pes_header->pts_dts_flag == 3)
					meta_data.dts_exist = 1;
				else
					meta_data.dts_exist = 0;

				meta_data.dts =
					((u64)pes_header->dts_1 << 30) |
					((u64)pes_header->dts_2 << 22) |
					((u64)pes_header->dts_3 << 15) |
					((u64)pes_header->dts_4 << 7) |
					(u64)pes_header->dts_5;

				meta_data.is_padding = 0;

				if (mpq_streambuffer_pkt_write(
						stream_buffer,
						&packet,
						(u8 *)&meta_data) < 0)
					MPQ_DVB_ERR_PRINT(
						"%s: "
						"Couldn't write packet. "
						"Should never happen\n",
						__func__);
			} else {
				MPQ_DVB_ERR_PRINT(
					"%s: received PUSI"
					"while handling PES header"
					"of previous PES\n",
					__func__);
			}

			/* Reset PES info */
			feed_data->pes_payload_address =
				(u32)stream_buffer->raw_data.data +
				stream_buffer->raw_data.pwrite;

			feed->peslen = 0;
			feed_data->pes_header_offset = 0;
			feed_data->pes_header_left_bytes =
				PES_MANDATORY_FIELDS_LEN;
		} else {
			feed->pusi_seen = 1;
		}
	}

	/*
	 * Parse PES data only if PUSI was encountered,
	 * otherwise the data is dropped
	 */
	if (!feed->pusi_seen) {
		spin_unlock(&mpq_demux->feed_lock);
		return 0; /* drop and wait for next packets */
	}

	ts_payload_offset = sizeof(struct ts_packet_header);

	/* Skip adaptation field if exists */
	if (ts_header->adaptation_field_control == 3)
		ts_payload_offset += buf[ts_payload_offset] + 1;

	bytes_avail = 188 - ts_payload_offset;

	/* Got the mandatory fields of the video PES header? */
	if (feed_data->pes_header_offset < PES_MANDATORY_FIELDS_LEN) {
		left_size =
			PES_MANDATORY_FIELDS_LEN -
			feed_data->pes_header_offset;

		copy_len = (left_size > bytes_avail) ?
					bytes_avail :
					left_size;

		memcpy((u8 *)pes_header+feed_data->pes_header_offset,
				buf+ts_payload_offset,
				copy_len);

		feed_data->pes_header_offset += copy_len;

		if (left_size > bytes_avail) {
			spin_unlock(&mpq_demux->feed_lock);
			return 0;
		}

		/* else - we have beginning of PES header */
		bytes_avail -= left_size;
		ts_payload_offset += left_size;

		/* Make sure the PES packet is valid */
		if (mpq_dmx_is_valid_video_pes(pes_header) < 0) {
			/*
			 * Since the new PES header parsing
			 * failed, reset pusi_seen to drop all
			 * data until next PUSI
			 */
			feed->pusi_seen = 0;
			feed_data->pes_header_offset = 0;

			MPQ_DVB_ERR_PRINT(
				"%s: invalid packet\n",
				__func__);

			spin_unlock(&mpq_demux->feed_lock);
			return 0;
		}

		feed_data->pes_header_left_bytes =
			pes_header->pes_header_data_length;
	}

	/* Remainning header bytes that need to be processed? */
	if (feed_data->pes_header_left_bytes) {
		/* Did we capture the PTS value (if exist)? */
		if ((bytes_avail != 0) &&
			(feed_data->pes_header_offset <
			 (PES_MANDATORY_FIELDS_LEN+5)) &&
			((pes_header->pts_dts_flag == 2) ||
			 (pes_header->pts_dts_flag == 3))) {

			/* 5 more bytes should be there */
			left_size =
				PES_MANDATORY_FIELDS_LEN +
				5 -
				feed_data->pes_header_offset;

			copy_len = (left_size > bytes_avail) ?
						bytes_avail :
						left_size;

			memcpy((u8 *)pes_header+
				feed_data->pes_header_offset,
				buf+ts_payload_offset,
				copy_len);

			feed_data->pes_header_offset += copy_len;
			feed_data->pes_header_left_bytes -= copy_len;

			if (left_size > bytes_avail) {
				spin_unlock(&mpq_demux->feed_lock);
				return 0;
			}

			/* else - we have the PTS */
			bytes_avail -= copy_len;
			ts_payload_offset += copy_len;
		}

		/* Did we capture the DTS value (if exist)? */
		if ((bytes_avail != 0) &&
			(feed_data->pes_header_offset <
			 (PES_MANDATORY_FIELDS_LEN+10)) &&
			(pes_header->pts_dts_flag == 3)) {

			/* 5 more bytes should be there */
			left_size =
				PES_MANDATORY_FIELDS_LEN +
				10 -
				feed_data->pes_header_offset;

			copy_len = (left_size > bytes_avail) ?
						bytes_avail :
						left_size;

			memcpy((u8 *)pes_header+
				feed_data->pes_header_offset,
				buf+ts_payload_offset,
				copy_len);

			feed_data->pes_header_offset += copy_len;
			feed_data->pes_header_left_bytes -= copy_len;

			if (left_size > bytes_avail) {
				spin_unlock(&mpq_demux->feed_lock);
				return 0;
			}

			/* else - we have the DTS */
			bytes_avail -= copy_len;
			ts_payload_offset += copy_len;
		}

		/* Any more header bytes?! */
		if (feed_data->pes_header_left_bytes >= bytes_avail) {
			feed_data->pes_header_left_bytes -= bytes_avail;
			spin_unlock(&mpq_demux->feed_lock);
			return 0;
		}

		/* Got PES header, process payload */
		bytes_avail -= feed_data->pes_header_left_bytes;
		ts_payload_offset += feed_data->pes_header_left_bytes;
		feed_data->pes_header_left_bytes = 0;
	}

	/*
	 * If we reached here,
	 * then we are now at the PES payload data
	 */
	if (bytes_avail == 0) {
		spin_unlock(&mpq_demux->feed_lock);
		return 0;
	}

	if (feed->peslen == 0) { /* starting new PES */
		/* gap till end of the buffer */
		int gap =
			stream_buffer->raw_data.size -
			stream_buffer->raw_data.pwrite;

		if ((!mpq_dmx_info.decoder_data_wrap) &&
			(gap < VIDEO_WRAP_AROUND_THRESHOLD)) {
			struct mpq_streambuffer_packet_header packet;
			struct mpq_adapter_video_meta_data meta_data;

			/*
			 * Do not start writting new PES from
			 * this location to prevent possible
			 * wrap-around of the payload, fill padding instead.
			 */

			/* push a packet with padding indication */
			meta_data.is_padding = 1;

			packet.raw_data_len = gap;
			packet.user_data_len =
				sizeof(struct mpq_adapter_video_meta_data);
			packet.raw_data_addr =
				feed_data->pes_payload_address;

			if (mpq_streambuffer_data_write_deposit(
						stream_buffer,
						gap) < 0) {
				MPQ_DVB_ERR_PRINT(
					"%s: mpq_streambuffer_data_write_deposit "
					"failed!\n",
					__func__);
			} else if (mpq_streambuffer_pkt_write(
							stream_buffer,
							&packet,
							(u8 *)&meta_data) < 0) {
				MPQ_DVB_ERR_PRINT(
					"%s: "
					"Couldn't write packet. "
					"Should never happen\n",
					__func__);
			} else {
				feed_data->pes_payload_address =
					(u32)stream_buffer->raw_data.data +
					stream_buffer->raw_data.pwrite;
			}
		}
	}

	if (mpq_streambuffer_data_write(
				stream_buffer,
				buf+ts_payload_offset,
				bytes_avail) < 0)
		mpq_demux->decoder_tsp_drop_count++;
	else
		feed->peslen += bytes_avail;

	spin_unlock(&mpq_demux->feed_lock);

	return 0;
}
EXPORT_SYMBOL(mpq_dmx_process_video_packet);


int mpq_dmx_process_pcr_packet(
			struct dvb_demux_feed *feed,
			const u8 *buf)
{
	int i;
	u64 pcr;
	u64 stc;
	u8 output[PCR_STC_LEN];
	struct mpq_demux *mpq_demux =
		(struct mpq_demux *)feed->demux->priv;
	const struct ts_packet_header *ts_header;
	const struct ts_adaptation_field *adaptation_field;

	/*
	 * When we play from front-end, we configure HW
	 * to output the extra timestamp, if we are playing
	 * from DVR, make sure the format is 192 packet.
	 */
	if ((mpq_demux->source >= DMX_SOURCE_DVR0) &&
		(mpq_demux->demux.tsp_format != DMX_TSP_FORMAT_192_TAIL)) {
		MPQ_DVB_ERR_PRINT(
			"%s: invalid packet format %d for PCR extraction\n",
			__func__,
			mpq_demux->demux.tsp_format);

		 return -EINVAL;
	}

	ts_header = (const struct ts_packet_header *)buf;

	/* Make sure this TS packet has a adaptation field */
	if ((ts_header->sync_byte != 0x47) ||
		(ts_header->adaptation_field_control == 0) ||
		(ts_header->adaptation_field_control == 1)) {
		return 0;
	}

	adaptation_field = (const struct ts_adaptation_field *)
			(buf + sizeof(struct ts_packet_header));

	if ((!adaptation_field->adaptation_field_length) ||
		(!adaptation_field->PCR_flag))
		return 0; /* 0 adaptation field or no PCR */

	pcr = ((u64)adaptation_field->program_clock_reference_base_1) << 25;
	pcr += ((u64)adaptation_field->program_clock_reference_base_2) << 17;
	pcr += ((u64)adaptation_field->program_clock_reference_base_3) << 9;
	pcr += ((u64)adaptation_field->program_clock_reference_base_4) << 1;
	pcr += adaptation_field->program_clock_reference_base_5;
	pcr *= 300;
	pcr +=
		(((u64)adaptation_field->program_clock_reference_ext_1) << 8) +
		adaptation_field->program_clock_reference_ext_2;

	stc = buf[189] << 16;
	stc += buf[190] << 8;
	stc += buf[191];
	stc *= 256; /* convert from 105.47 KHZ to 27MHz */

	output[0] = adaptation_field->discontinuity_indicator;

	for (i = 1; i <= 8; i++)
		output[i] = (stc >> ((8-i) << 3)) & 0xFF;

	for (i = 9; i <= 16; i++)
		output[i] = (pcr >> ((16-i) << 3)) & 0xFF;

	feed->cb.ts(output, PCR_STC_LEN,
				NULL, 0,
				&feed->feed.ts, DMX_OK);
	return 0;
}
EXPORT_SYMBOL(mpq_dmx_process_pcr_packet);

