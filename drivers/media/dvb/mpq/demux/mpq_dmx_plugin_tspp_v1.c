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
#include <linux/workqueue.h>
#include <mach/msm_tspp.h>
#include "mpq_dvb_debug.h"
#include "mpq_dmx_plugin_common.h"


#define TSIF_COUNT					2

#define TSPP_MAX_PID_FILTER_NUM		16

/* Max number of section filters */
#define TSPP_MAX_SECTION_FILTER_NUM		64

/* For each TSIF we allocate two pipes, one for PES and one for sections */
#define TSPP_PES_CHANNEL				0
#define TSPP_SECTION_CHANNEL			1

/* the channel_id set to TSPP driver based on TSIF number and channel type */
#define TSPP_CHANNEL_ID(tsif, ch)		((tsif << 1) + ch)
#define TSPP_IS_PES_CHANNEL(ch_id)		((ch_id & 0x1) == 0)
#define TSPP_GET_TSIF_NUM(ch_id)		(ch_id >> 1)

/* mask that set to care for all bits in pid filter */
#define TSPP_PID_MASK					0x1FFF

/* dvb-demux defines pid 0x2000 as full capture pid */
#define TSPP_PASS_THROUGH_PID			0x2000

/* TODO - NEED TO SET THESE PROPERLY
 * once TSPP driver is ready, reduce TSPP_BUFFER_SIZE
 * to single packet and set TSPP_BUFFER_COUNT accordingly
 */

#define TSPP_RAW_TTS_SIZE				192

/* Size of single descriptor. Using max descriptor size (170 packets).
 * Assuming 20MBit/sec stream, with 170 packets
 * per descriptor there would be about 82 descriptors,
 * Meanning about 82 notifications per second.
 */
#define MAX_BAM_DESCRIPTOR_SIZE		(32*1024 - 1)
#define TSPP_BUFFER_SIZE			\
	((MAX_BAM_DESCRIPTOR_SIZE / TSPP_RAW_TTS_SIZE) * TSPP_RAW_TTS_SIZE)

/* Number of descriptors, total size: TSPP_BUFFER_SIZE*TSPP_BUFFER_COUNT */
#define TSPP_BUFFER_COUNT				(32)

/* When TSPP notifies demux that new packets are received */
#define TSPP_NOTIFICATION_SIZE			1

/* Channel timeout in msec */
#define TSPP_CHANNEL_TIMEOUT			16

/* module parameters for load time configuration */
static int tsif0_mode = TSPP_TSIF_MODE_2;
static int tsif1_mode = TSPP_TSIF_MODE_2;
module_param(tsif0_mode, int, S_IRUGO);
module_param(tsif1_mode, int, S_IRUGO);

/*
 * Work scheduled each time TSPP notifies dmx
 * of new TS packet in some channel
 */
struct tspp_work {
	struct work_struct work;
	int channel_id;
};

/* The following structure hold singelton information
 * required for dmx implementation on top of TSPP.
 */
static struct
{
	/* Information for each TSIF input processing */
	struct {
		/*
		 * TSPP pipe holding all TS packets with PES data.
		 * The following is reference count for number of feeds
		 * allocated on that pipe.
		 */
		int pes_channel_ref;

		/* work used to submit to workqueue to process pes channel */
		struct tspp_work pes_work;

		/*
		 * TSPP pipe holding all TS packets with section data.
		 * The following is reference count for number of feeds
		 * allocated on that pipe.
		 */
		int section_channel_ref;

		/* work used to submit to workqueue to process pes channel */
		struct tspp_work section_work;

		/*
		 * Holds PIDs of allocated TSPP filters along with
		 * how many feeds are opened on same PID.
		 */
		struct {
			int pid;
			int ref_count;
		} filters[TSPP_MAX_PID_FILTER_NUM];

		/* workqueue that processes TS packets from specific TSIF */
		struct workqueue_struct *workqueue;

		/* TSIF alias */
		char name[TSIF_NAME_LENGTH];

		/* Pointer to the demux connected to this TSIF */
		struct mpq_demux *mpq_demux;

		/* mutex protecting the data-structure */
		struct mutex mutex;
	} tsif[TSIF_COUNT];
} mpq_dmx_tspp_info;


/**
 * Returns a free filter slot that can be used.
 *
 * @tsif: The TSIF to allocate filter from
 * @channel_id: The channel allocating filter to
 *
 * Return  filter index or -1 if no filters available
 *
 * To give priority to PES data, for pes filters
 * the table is scanned from high to low priority,
 * and sections from low to high priority. This way TSPP
 * would get a match on PES data filters faster as they
 * are scanned first.
 */
static int mpq_tspp_get_free_filter_slot(int tsif, int channel_id)
{
	int i;

	if (TSPP_IS_PES_CHANNEL(channel_id)) {
		for (i = 0; i < TSPP_MAX_PID_FILTER_NUM; i++)
			if (mpq_dmx_tspp_info.tsif[tsif].filters[i].pid == -1)
				return i;
	} else {
		for (i = TSPP_MAX_PID_FILTER_NUM-1; i >= 0; i--)
			if (mpq_dmx_tspp_info.tsif[tsif].filters[i].pid == -1)
				return i;
	}

	return -ENOMEM;
}

/**
 * Returns filter index of specific pid.
 *
 * @tsif: The TSIF to which the pid is allocated
 * @pid: The pid to search for
 *
 * Return  filter index or -1 if no filter available
 */
static int mpq_tspp_get_filter_slot(int tsif, int pid)
{
	int i;

	for (i = 0; i < TSPP_MAX_PID_FILTER_NUM; i++)
		if (mpq_dmx_tspp_info.tsif[tsif].filters[i].pid == pid)
			return i;

	return -EINVAL;
}

/**
 * Worker function that processes the TS packets notified by TSPP.
 *
 * @worker: the executed work
 */
static void mpq_dmx_tspp_work(struct work_struct *worker)
{
	struct tspp_work *tspp_work =
		container_of(worker, struct tspp_work, work);
	struct mpq_demux *mpq_demux;
	int channel_id = tspp_work->channel_id;
	int tsif = TSPP_GET_TSIF_NUM(channel_id);
	const struct tspp_data_descriptor *tspp_data_desc;
	int ref_count;

	mpq_demux = mpq_dmx_tspp_info.tsif[tsif].mpq_demux;

	/* Lock against the TSPP filters data-structure */
	if (mutex_lock_interruptible(&mpq_dmx_tspp_info.tsif[tsif].mutex))
		return;

	/* Make sure channel is still active */
	if (TSPP_IS_PES_CHANNEL(channel_id))
		ref_count = mpq_dmx_tspp_info.tsif[tsif].pes_channel_ref;
	else
		ref_count = mpq_dmx_tspp_info.tsif[tsif].section_channel_ref;

	if (ref_count == 0) {
		mutex_unlock(&mpq_dmx_tspp_info.tsif[tsif].mutex);
		return;
	}

	mpq_demux->hw_notification_size = 0;

	/* Go through all filled descriptors and perform demuxing on them */
	while ((tspp_data_desc = tspp_get_buffer(0, channel_id)) != NULL) {
		mpq_demux->hw_notification_size +=
			(tspp_data_desc->size / TSPP_RAW_TTS_SIZE);

		dvb_dmx_swfilter_format(
				&mpq_demux->demux,
				tspp_data_desc->virt_base,
				tspp_data_desc->size,
				DMX_TSP_FORMAT_192_TAIL);

		/* Notify TSPP that the buffer is no longer needed */
		tspp_release_buffer(0, channel_id, tspp_data_desc->id);
	}

	mutex_unlock(&mpq_dmx_tspp_info.tsif[tsif].mutex);
}

/**
 * Callback function from TSPP when new data is ready.
 *
 * @channel_id: Channel with new TS packets
 * @user: user-data holding TSIF number
 */
static void mpq_tspp_callback(int channel_id, void *user)
{
	int tsif = (int)user;
	struct work_struct *work;
	struct mpq_demux *mpq_demux;

	/* Save statistics on TSPP notifications */
	mpq_demux = mpq_dmx_tspp_info.tsif[tsif].mpq_demux;
	mpq_dmx_update_hw_statistics(mpq_demux);

	if (TSPP_IS_PES_CHANNEL(channel_id))
		work = &mpq_dmx_tspp_info.tsif[tsif].pes_work.work;
	else
		work = &mpq_dmx_tspp_info.tsif[tsif].section_work.work;

	/* Scheudle a new work to demux workqueue */
	if (!work_pending(work))
		queue_work(mpq_dmx_tspp_info.tsif[tsif].workqueue, work);
}

/**
 * Configure TSPP channel to filter the PID of new feed.
 *
 * @feed: The feed to configure the channel with
 *
 * Return  error status
 *
 * The function checks if the new PID can be added to an already
 * allocated channel, if not, a new channel is allocated and configured.
 */
static int mpq_tspp_dmx_add_channel(struct dvb_demux_feed *feed)
{
	struct mpq_demux *mpq_demux = feed->demux->priv;
	enum tspp_source tspp_source;
	struct tspp_filter tspp_filter;
	int tsif;
	int ret;
	int channel_id;
	int *channel_ref_count;
	enum tspp_tsif_mode mode;

	/* determine the TSIF we are reading from */
	if (mpq_demux->source == DMX_SOURCE_FRONT0) {
		tsif = 0;
		tspp_source = TSPP_SOURCE_TSIF0;
		mode = (enum tspp_tsif_mode)tsif0_mode;
	} else if (mpq_demux->source == DMX_SOURCE_FRONT1) {
		tsif = 1;
		tspp_source = TSPP_SOURCE_TSIF1;
		mode = (enum tspp_tsif_mode)tsif1_mode;
	} else {
		/* invalid source */
		MPQ_DVB_ERR_PRINT(
			"%s: invalid input source (%d)\n",
			__func__,
			mpq_demux->source);

		return -EINVAL;
	}

	if (mutex_lock_interruptible(&mpq_dmx_tspp_info.tsif[tsif].mutex))
		return -ERESTARTSYS;

	/*
	 * It is possible that this PID was already requested before.
	 * Can happen if we play and record same PES or PCR
	 * piggypacked on video packet.
	 */
	ret = mpq_tspp_get_filter_slot(tsif, feed->pid);
	if (ret >= 0) {
		/* PID already configured */
		mpq_dmx_tspp_info.tsif[tsif].filters[ret].ref_count++;
		mutex_unlock(&mpq_dmx_tspp_info.tsif[tsif].mutex);
		return 0;
	}

	/* determine to which pipe the feed should be routed: section or pes */
	if ((feed->type == DMX_TYPE_PES) || (feed->type == DMX_TYPE_TS)) {
		channel_id = TSPP_CHANNEL_ID(tsif, TSPP_PES_CHANNEL);
		channel_ref_count =
			&mpq_dmx_tspp_info.tsif[tsif].pes_channel_ref;
	} else {
		channel_id = TSPP_CHANNEL_ID(tsif, TSPP_SECTION_CHANNEL);
		channel_ref_count =
			&mpq_dmx_tspp_info.tsif[tsif].section_channel_ref;
	}

	/* check if required TSPP pipe is already allocated or not */
	if (*channel_ref_count == 0) {
		ret = tspp_open_channel(0, channel_id);

		if (ret < 0) {
			MPQ_DVB_ERR_PRINT(
				"%s: tspp_open_channel(%d) failed (%d)\n",
				__func__,
				channel_id,
				ret);

			goto add_channel_failed;
		}

		/* set TSPP source */
		ret = tspp_open_stream(0, channel_id, tspp_source, mode);
		if (ret < 0) {
			MPQ_DVB_ERR_PRINT(
				"%s: tspp_select_source(%d,%d) failed (%d)\n",
				__func__,
				channel_id,
				tspp_source,
				ret);

			goto add_channel_close_ch;
		}

		/* register notification on TS packets */
		tspp_register_notification(0,
					   channel_id,
					   mpq_tspp_callback,
					   (void *)tsif,
					   TSPP_CHANNEL_TIMEOUT);

		/* TODO: register allocater and provide allocation function
		 * that allocate from continous memory so that we can have
		 * big notification size, smallest descriptor, and still provide
		 * TZ with single big buffer based on notification size.
		 */

		/* set buffer/descriptor size and count */
		ret = tspp_allocate_buffers(0,
					    channel_id,
					    TSPP_BUFFER_COUNT,
					    TSPP_BUFFER_SIZE,
					    TSPP_NOTIFICATION_SIZE,
					    NULL,
					    NULL);
		if (ret < 0) {
			MPQ_DVB_ERR_PRINT(
				"%s: tspp_allocate_buffers(%d) failed (%d)\n",
				__func__,
				channel_id,
				ret);

			goto add_channel_unregister_notif;
		}

		mpq_dmx_tspp_info.tsif[tsif].mpq_demux = mpq_demux;
	}

	/* add new PID to the existing pipe */
	ret = mpq_tspp_get_free_filter_slot(tsif, channel_id);
	if (ret < 0) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_allocate_filter_slot(%d, %d) failed\n",
			__func__,
			tsif,
			channel_id);

		goto add_channel_unregister_notif;
	}

	mpq_dmx_tspp_info.tsif[tsif].filters[ret].pid = feed->pid;
	mpq_dmx_tspp_info.tsif[tsif].filters[ret].ref_count++;

	tspp_filter.priority = ret;
	if (feed->pid == TSPP_PASS_THROUGH_PID) {
		/* pass all pids */
		tspp_filter.pid = 0;
		tspp_filter.mask = 0;
	} else {
		tspp_filter.pid = feed->pid;
		tspp_filter.mask = TSPP_PID_MASK;
	}

	/*
	 * Include TTS in RAW packets, if you change this to
	 * TSPP_MODE_RAW_NO_SUFFIX you must also change TSPP_RAW_TTS_SIZE
	 * accordingly.
	 */
	tspp_filter.mode = TSPP_MODE_RAW;
	tspp_filter.source = tspp_source;
	tspp_filter.decrypt = 0;
	ret = tspp_add_filter(0, channel_id, &tspp_filter);
	if (ret < 0) {
		MPQ_DVB_ERR_PRINT(
			"%s: tspp_add_filter(%d) failed (%d)\n",
			__func__,
			channel_id,
			ret);

		goto add_channel_free_filter_slot;
	}

	(*channel_ref_count)++;

	mutex_unlock(&mpq_dmx_tspp_info.tsif[tsif].mutex);
	return 0;

add_channel_free_filter_slot:
	mpq_dmx_tspp_info.tsif[tsif].filters[tspp_filter.priority].pid = -1;
	mpq_dmx_tspp_info.tsif[tsif].filters[tspp_filter.priority].ref_count--;
add_channel_unregister_notif:
	tspp_unregister_notification(0, channel_id);
add_channel_close_ch:
	tspp_close_channel(0, channel_id);
add_channel_failed:
	mutex_unlock(&mpq_dmx_tspp_info.tsif[tsif].mutex);
	return ret;
}

/**
 * Removes filter from TSPP.
 *
 * @feed: The feed to remove
 *
 * Return  error status
 *
 * The function checks if this is the only PID allocated within
 * the channel, if so, the channel is closed as well.
 */
static int mpq_tspp_dmx_remove_channel(struct dvb_demux_feed *feed)
{
	int tsif;
	int ret;
	int channel_id;
	int *channel_ref_count;
	struct tspp_filter tspp_filter;
	struct mpq_demux *mpq_demux = feed->demux->priv;

	/* determine the TSIF we are reading from */
	if (mpq_demux->source == DMX_SOURCE_FRONT0) {
		tsif = 0;
	} else if (mpq_demux->source == DMX_SOURCE_FRONT1) {
		tsif = 1;
	} else {
		/* invalid source */
		MPQ_DVB_ERR_PRINT(
			"%s: invalid input source (%d)\n",
			__func__,
			mpq_demux->source);

		return -EINVAL;
	}

	if (mutex_lock_interruptible(&mpq_dmx_tspp_info.tsif[tsif].mutex))
		return -ERESTARTSYS;

	/* determine to which pipe the feed should be routed: section or pes */
	if ((feed->type == DMX_TYPE_PES) || (feed->type == DMX_TYPE_TS)) {
		channel_id = TSPP_CHANNEL_ID(tsif, TSPP_PES_CHANNEL);
		channel_ref_count =
			&mpq_dmx_tspp_info.tsif[tsif].pes_channel_ref;
	} else {
		channel_id = TSPP_CHANNEL_ID(tsif, TSPP_SECTION_CHANNEL);
		channel_ref_count =
			&mpq_dmx_tspp_info.tsif[tsif].section_channel_ref;
	}

	/* check if required TSPP pipe is already allocated or not */
	if (*channel_ref_count == 0) {
		/* invalid feed provided as the channel is not allocated */
		MPQ_DVB_ERR_PRINT(
			"%s: invalid feed (%d)\n",
			__func__,
			channel_id);

		ret = -EINVAL;
		goto remove_channel_failed;
	}

	tspp_filter.priority = mpq_tspp_get_filter_slot(tsif, feed->pid);

	if (tspp_filter.priority < 0) {
		/* invalid feed provided as it has no filter allocated */
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_tspp_get_filter_slot failed (%d,%d)\n",
			__func__,
			feed->pid,
			tsif);

		ret = -EINVAL;
		goto remove_channel_failed;
	}

	mpq_dmx_tspp_info.tsif[tsif].filters[tspp_filter.priority].ref_count--;

	if (mpq_dmx_tspp_info.tsif[tsif].
		filters[tspp_filter.priority].ref_count) {
		/*
		 * there are still references to this pid, do not
		 * remove the filter yet
		 */
		mutex_unlock(&mpq_dmx_tspp_info.tsif[tsif].mutex);
		return 0;
	}

	ret = tspp_remove_filter(0, channel_id, &tspp_filter);
	if (ret < 0) {
		/* invalid feed provided as it has no filter allocated */
		MPQ_DVB_ERR_PRINT(
			"%s: tspp_remove_filter failed (%d,%d)\n",
			__func__,
			channel_id,
			tspp_filter.priority);

		goto remove_channel_failed_restore_count;
	}

	mpq_dmx_tspp_info.tsif[tsif].filters[tspp_filter.priority].pid = -1;
	(*channel_ref_count)--;

	if (*channel_ref_count == 0) {
		/* channel is not used any more, release it */
		tspp_unregister_notification(0, channel_id);
		tspp_close_channel(0, channel_id);
	}

	mutex_unlock(&mpq_dmx_tspp_info.tsif[tsif].mutex);
	return 0;

remove_channel_failed_restore_count:
	mpq_dmx_tspp_info.tsif[tsif].filters[tspp_filter.priority].ref_count++;

remove_channel_failed:
	mutex_unlock(&mpq_dmx_tspp_info.tsif[tsif].mutex);
	return ret;
}

static int mpq_tspp_dmx_start_filtering(struct dvb_demux_feed *feed)
{
	int ret;
	struct mpq_demux *mpq_demux = feed->demux->priv;

	MPQ_DVB_DBG_PRINT(
		"%s(%d) executed\n",
		__func__,
		feed->pid);

	if (mpq_demux == NULL) {
		MPQ_DVB_ERR_PRINT(
			"%s: invalid mpq_demux handle\n",
			__func__);

		return -EINVAL;
	}

	if (mpq_demux->source < DMX_SOURCE_DVR0) {
		/* source from TSPP, need to configure tspp pipe */
		ret = mpq_tspp_dmx_add_channel(feed);

		if (ret < 0) {
			MPQ_DVB_DBG_PRINT(
				"%s: mpq_tspp_dmx_add_channel failed(%d)\n",
				__func__,
				ret);
			return ret;
		}
	}

	/*
	 * Always feed sections/PES starting from a new one and
	 * do not partial transfer data from older one
	 */
	feed->pusi_seen = 0;

	/*
	 * For video PES, data is tunneled to the decoder,
	 * initialize tunneling and pes parsing.
	 */
	if (mpq_dmx_is_video_feed(feed)) {
		ret = mpq_dmx_init_video_feed(feed);

		if (ret < 0) {
			MPQ_DVB_DBG_PRINT(
				"%s: mpq_dmx_init_video_feed failed(%d)\n",
				__func__,
				ret);

			if (mpq_demux->source < DMX_SOURCE_DVR0)
				mpq_tspp_dmx_remove_channel(feed);

			return ret;
		}
	}

	return 0;
}

static int mpq_tspp_dmx_stop_filtering(struct dvb_demux_feed *feed)
{
	int ret = 0;
	struct mpq_demux *mpq_demux = feed->demux->priv;

	MPQ_DVB_DBG_PRINT(
		"%s(%d) executed\n",
		__func__,
		feed->pid);

	/*
	 * For video PES, data is tunneled to the decoder,
	 * terminate tunnel and pes parsing.
	 */
	if (mpq_dmx_is_video_feed(feed))
		mpq_dmx_terminate_video_feed(feed);

	if (mpq_demux->source < DMX_SOURCE_DVR0) {
		/* source from TSPP, need to configure tspp pipe */
		ret = mpq_tspp_dmx_remove_channel(feed);
	}

	return ret;
}

static int mpq_tspp_dmx_write_to_decoder(
					struct dvb_demux_feed *feed,
					const u8 *buf,
					size_t len)
{

	/*
	 * It is assumed that this function is called once for each
	 * TS packet of the relevant feed.
	 */
	if (len > TSPP_RAW_TTS_SIZE)
		MPQ_DVB_DBG_PRINT(
				"%s: warnning - len larger than one packet\n",
				__func__);

	if (mpq_dmx_is_video_feed(feed))
		return mpq_dmx_process_video_packet(feed, buf);

	if (mpq_dmx_is_pcr_feed(feed))
		return mpq_dmx_process_pcr_packet(feed, buf);

	return 0;
}

/**
 * Returns demux capabilities of TSPPv1 plugin
 *
 * @demux: demux device
 * @caps: Returned capbabilities
 *
 * Return     error code
 */
static int mpq_tspp_dmx_get_caps(struct dmx_demux *demux,
				struct dmx_caps *caps)
{
	struct dvb_demux *dvb_demux = demux->priv;

	if ((dvb_demux == NULL) || (caps == NULL)) {
		MPQ_DVB_ERR_PRINT(
			"%s: invalid parameters\n",
			__func__);

		return -EINVAL;
	}

	caps->caps = DMX_CAP_PULL_MODE | DMX_CAP_VIDEO_DECODER_DATA;
	caps->num_decoders = MPQ_ADAPTER_MAX_NUM_OF_INTERFACES;
	caps->num_demux_devices = CONFIG_DVB_MPQ_NUM_DMX_DEVICES;
	caps->num_pid_filters = TSPP_MAX_PID_FILTER_NUM;
	caps->num_section_filters = dvb_demux->filternum;
	caps->num_section_filters_per_pid = dvb_demux->filternum;
	caps->section_filter_length = DMX_FILTER_SIZE;
	caps->num_demod_inputs = TSIF_COUNT;
	caps->num_memory_inputs = CONFIG_DVB_MPQ_NUM_DMX_DEVICES;
	caps->max_bitrate = 144;
	caps->demod_input_max_bitrate = 72;
	caps->memory_input_max_bitrate = 72;

	return 0;
}

static int mpq_tspp_dmx_init(
			struct dvb_adapter *mpq_adapter,
			struct mpq_demux *mpq_demux)
{
	int result;

	MPQ_DVB_DBG_PRINT("%s executed\n", __func__);

	/* Set the kernel-demux object capabilities */
	mpq_demux->demux.dmx.capabilities =
		DMX_TS_FILTERING			|
		DMX_PES_FILTERING			|
		DMX_SECTION_FILTERING		|
		DMX_MEMORY_BASED_FILTERING	|
		DMX_CRC_CHECKING			|
		DMX_TS_DESCRAMBLING;

	/* Set dvb-demux "virtual" function pointers */
	mpq_demux->demux.priv = (void *)mpq_demux;
	mpq_demux->demux.filternum = TSPP_MAX_SECTION_FILTER_NUM;
	mpq_demux->demux.feednum = MPQ_MAX_DMX_FILES;
	mpq_demux->demux.start_feed = mpq_tspp_dmx_start_filtering;
	mpq_demux->demux.stop_feed = mpq_tspp_dmx_stop_filtering;
	mpq_demux->demux.write_to_decoder = mpq_tspp_dmx_write_to_decoder;

	mpq_demux->demux.decoder_fullness_init =
		mpq_dmx_decoder_fullness_init;

	mpq_demux->demux.decoder_fullness_wait =
		mpq_dmx_decoder_fullness_wait;

	mpq_demux->demux.decoder_fullness_abort =
		mpq_dmx_decoder_fullness_abort;

	mpq_demux->demux.decoder_buffer_status =
		mpq_dmx_decoder_buffer_status;

	/* Initialize dvb_demux object */
	result = dvb_dmx_init(&mpq_demux->demux);
	if (result < 0) {
		MPQ_DVB_ERR_PRINT("%s: dvb_dmx_init failed\n", __func__);
		goto init_failed;
	}

	/* Now initailize the dmx-dev object */
	mpq_demux->dmxdev.filternum = MPQ_MAX_DMX_FILES;
	mpq_demux->dmxdev.demux = &mpq_demux->demux.dmx;
	mpq_demux->dmxdev.capabilities =
		DMXDEV_CAP_DUPLEX |
		DMXDEV_CAP_PULL_MODE |
		DMXDEV_CAP_INDEXING;

	mpq_demux->dmxdev.demux->set_source = mpq_dmx_set_source;
	mpq_demux->dmxdev.demux->get_caps = mpq_tspp_dmx_get_caps;

	result = dvb_dmxdev_init(&mpq_demux->dmxdev, mpq_adapter);
	if (result < 0) {
		MPQ_DVB_ERR_PRINT("%s: dvb_dmxdev_init failed (errno=%d)\n",
						  __func__,
						  result);
		goto init_failed_dmx_release;
	}

	/* Extend dvb-demux debugfs with TSPP statistics. */
	mpq_dmx_init_hw_statistics(mpq_demux);

	return 0;

init_failed_dmx_release:
	dvb_dmx_release(&mpq_demux->demux);
init_failed:
	return result;
}

static int __init mpq_dmx_tspp_plugin_init(void)
{
	int i;
	int j;
	int ret;

	MPQ_DVB_DBG_PRINT("%s executed\n", __func__);

	for (i = 0; i < TSIF_COUNT; i++) {
		mpq_dmx_tspp_info.tsif[i].pes_channel_ref = 0;

		mpq_dmx_tspp_info.tsif[i].pes_work.channel_id =
			TSPP_CHANNEL_ID(i, TSPP_PES_CHANNEL);

		INIT_WORK(&mpq_dmx_tspp_info.tsif[i].pes_work.work,
				  mpq_dmx_tspp_work);

		mpq_dmx_tspp_info.tsif[i].section_channel_ref = 0;

		mpq_dmx_tspp_info.tsif[i].section_work.channel_id =
			TSPP_CHANNEL_ID(i, TSPP_SECTION_CHANNEL);

		INIT_WORK(&mpq_dmx_tspp_info.tsif[i].section_work.work,
				  mpq_dmx_tspp_work);

		for (j = 0; j < TSPP_MAX_PID_FILTER_NUM; j++) {
			mpq_dmx_tspp_info.tsif[i].filters[j].pid = -1;
			mpq_dmx_tspp_info.tsif[i].filters[j].ref_count = 0;
		}

		snprintf(mpq_dmx_tspp_info.tsif[i].name,
				TSIF_NAME_LENGTH,
				"tsif_%d",
				i);

		mpq_dmx_tspp_info.tsif[i].workqueue =
			create_singlethread_workqueue(
				mpq_dmx_tspp_info.tsif[i].name);

		if (mpq_dmx_tspp_info.tsif[i].workqueue == NULL) {

			for (j = 0; j < i; j++) {
				destroy_workqueue(
					mpq_dmx_tspp_info.tsif[j].workqueue);

				mutex_destroy(&mpq_dmx_tspp_info.tsif[j].mutex);
			}

			MPQ_DVB_ERR_PRINT(
				"%s: create_singlethread_workqueue failed\n",
				__func__);

			return -ENOMEM;
		}

		mutex_init(&mpq_dmx_tspp_info.tsif[i].mutex);
	}

	ret = mpq_dmx_plugin_init(mpq_tspp_dmx_init);

	if (ret < 0) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_dmx_plugin_init failed (errno=%d)\n",
			__func__,
			ret);

		for (i = 0; i < TSIF_COUNT; i++) {
			destroy_workqueue(mpq_dmx_tspp_info.tsif[i].workqueue);
			mutex_destroy(&mpq_dmx_tspp_info.tsif[i].mutex);
		}
	}

	return ret;
}

static void __exit mpq_dmx_tspp_plugin_exit(void)
{
	int i;

	MPQ_DVB_DBG_PRINT("%s executed\n", __func__);

	for (i = 0; i < TSIF_COUNT; i++) {
		mutex_lock(&mpq_dmx_tspp_info.tsif[i].mutex);

		if (mpq_dmx_tspp_info.tsif[i].pes_channel_ref) {
			tspp_unregister_notification(0, TSPP_PES_CHANNEL);
			tspp_close_channel(0,
				TSPP_CHANNEL_ID(i, TSPP_PES_CHANNEL));
		}

		if (mpq_dmx_tspp_info.tsif[i].section_channel_ref) {
			tspp_unregister_notification(0, TSPP_SECTION_CHANNEL);
			tspp_close_channel(0,
				TSPP_CHANNEL_ID(i, TSPP_SECTION_CHANNEL));
		}

		/* TODO: if we allocate buffer
		 * to TSPP ourself, need to free those as well
		 */

		mutex_unlock(&mpq_dmx_tspp_info.tsif[i].mutex);
		flush_workqueue(mpq_dmx_tspp_info.tsif[i].workqueue);
		destroy_workqueue(mpq_dmx_tspp_info.tsif[i].workqueue);
		mutex_destroy(&mpq_dmx_tspp_info.tsif[i].mutex);
	}

	mpq_dmx_plugin_exit();
}


module_init(mpq_dmx_tspp_plugin_init);
module_exit(mpq_dmx_tspp_plugin_exit);

MODULE_DESCRIPTION("Qualcomm demux TSPP version 1 HW Plugin");
MODULE_LICENSE("GPL v2");


