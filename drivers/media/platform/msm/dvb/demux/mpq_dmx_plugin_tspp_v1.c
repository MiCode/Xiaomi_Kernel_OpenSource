/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#include <linux/kthread.h>
#include <mach/msm_tspp.h>
#include "mpq_dvb_debug.h"
#include "mpq_dmx_plugin_common.h"

#define TSIF_COUNT			2

#define TSPP_MAX_PID_FILTER_NUM		16

/* Max number of section filters */
#define TSPP_MAX_SECTION_FILTER_NUM	64

/* For each TSIF we use a single pipe holding the data after PID filtering */
#define TSPP_CHANNEL			0

/* the channel_id set to TSPP driver based on TSIF number and channel type */
#define TSPP_CHANNEL_ID(tsif, ch)		((tsif << 1) + ch)
#define TSPP_GET_TSIF_NUM(ch_id)		(ch_id >> 1)

/* mask that set to care for all bits in pid filter */
#define TSPP_PID_MASK			0x1FFF

/* dvb-demux defines pid 0x2000 as full capture pid */
#define TSPP_PASS_THROUGH_PID		0x2000

#define TSPP_RAW_TTS_SIZE		192
#define TSPP_RAW_SIZE			188

#define MAX_BAM_DESCRIPTOR_SIZE	(32*1024 - 1)

#define TSPP_BUFFER_SIZE		(500 * 1024) /* 500KB */

#define TSPP_DESCRIPTOR_SIZE	(TSPP_RAW_TTS_SIZE)

#define TSPP_BUFFER_COUNT(buffer_size)	\
	((buffer_size) / TSPP_RAW_TTS_SIZE)

/* When TSPP notifies demux that new packets are received.
 * Using max descriptor size (170 packets).
 * Assuming 20MBit/sec stream, with 170 packets
 * per descriptor there would be about 82 descriptors,
 * Meanning about 82 notifications per second.
 */
#define TSPP_NOTIFICATION_SIZE(desc_size)		\
	(MAX_BAM_DESCRIPTOR_SIZE / (desc_size))

/* Channel timeout in msec */
#define TSPP_CHANNEL_TIMEOUT			100

enum mem_buffer_allocation_mode {
	MPQ_DMX_TSPP_INTERNAL_ALLOC = 0,
	MPQ_DMX_TSPP_CONTIGUOUS_PHYS_ALLOC = 1
};

/* module parameters for load time configuration */
static int clock_inv;
static int tsif_mode = 2;
static int allocation_mode = MPQ_DMX_TSPP_INTERNAL_ALLOC;
static int tspp_out_buffer_size = TSPP_BUFFER_SIZE;
static int tspp_notification_size =
	TSPP_NOTIFICATION_SIZE(TSPP_DESCRIPTOR_SIZE);
static int tspp_channel_timeout = TSPP_CHANNEL_TIMEOUT;
static int tspp_out_ion_heap = ION_QSECOM_HEAP_ID;

module_param(tsif_mode, int, S_IRUGO | S_IWUSR);
module_param(clock_inv, int, S_IRUGO | S_IWUSR);
module_param(allocation_mode, int, S_IRUGO | S_IWUSR);
module_param(tspp_out_buffer_size, int, S_IRUGO);
module_param(tspp_notification_size, int, S_IRUGO | S_IWUSR);
module_param(tspp_channel_timeout, int, S_IRUGO | S_IWUSR);
module_param(tspp_out_ion_heap, int, S_IRUGO | S_IWUSR);

/* The following structure hold singelton information
 * required for dmx implementation on top of TSPP.
 */
static struct
{
	/* Information for each TSIF input processing */
	struct {
		/*
		 * TSPP pipe holding all TS packets after PID filtering.
		 * The following is reference count for number of feeds
		 * allocated on that pipe.
		 */
		int channel_ref;

		/* Counter for data notifications on the pipe */
		atomic_t data_cnt;

		/* ION handle used for TSPP data buffer allocation */
		struct ion_handle *ch_mem_heap_handle;

		/* TSPP data buffer heap virtual base address */
		void *ch_mem_heap_virt_base;

		/* TSPP data buffer heap physical base address */
		ion_phys_addr_t ch_mem_heap_phys_base;

		/* buffer allocation index */
		int buff_index;

		u32 buffer_count;

		/*
		 * Holds PIDs of allocated TSPP filters along with
		 * how many feeds are opened on same PID.
		 */
		struct {
			int pid;
			int ref_count;
		} filters[TSPP_MAX_PID_FILTER_NUM];

		/* thread processing TS packets from TSPP */
		struct task_struct *thread;
		wait_queue_head_t wait_queue;

		/* TSIF alias */
		char name[TSIF_NAME_LENGTH];

		/* Pointer to the demux connected to this TSIF */
		struct mpq_demux *mpq_demux;

		/* mutex protecting the data-structure */
		struct mutex mutex;
	} tsif[TSIF_COUNT];

	/* ION client used for TSPP data buffer allocation */
	struct ion_client *ion_client;
} mpq_dmx_tspp_info;

static void *tspp_mem_allocator(int channel_id, u32 size,
				u32 *phys_base, void *user)
{
	void *virt_addr = NULL;
	int i = TSPP_GET_TSIF_NUM(channel_id);

	if (mpq_dmx_tspp_info.tsif[i].buff_index ==
		mpq_dmx_tspp_info.tsif[i].buffer_count)
		return NULL;

	virt_addr =
		(mpq_dmx_tspp_info.tsif[i].ch_mem_heap_virt_base +
		(mpq_dmx_tspp_info.tsif[i].buff_index * size));

	*phys_base =
		(mpq_dmx_tspp_info.tsif[i].ch_mem_heap_phys_base +
		(mpq_dmx_tspp_info.tsif[i].buff_index * size));

	mpq_dmx_tspp_info.tsif[i].buff_index++;

	return virt_addr;
}

static void tspp_mem_free(int channel_id, u32 size,
			void *virt_base, u32 phys_base, void *user)
{
	int i = TSPP_GET_TSIF_NUM(channel_id);

	/*
	 * actual buffer heap free is done in mpq_dmx_tspp_plugin_exit().
	 * we update index here, so if this function is called repetitively
	 * for all the buffers, then afterwards tspp_mem_allocator()
	 * can be called again.
	 * Note: it would be incorrect to call tspp_mem_allocator()
	 * a few times, then call tspp_mem_free(), then call
	 * tspp_mem_allocator() again.
	 */
	if (mpq_dmx_tspp_info.tsif[i].buff_index > 0)
		mpq_dmx_tspp_info.tsif[i].buff_index--;
}

/**
 * Returns a free filter slot that can be used.
 *
 * @tsif: The TSIF to allocate filter from
 * @channel_id: The channel allocating filter to
 *
 * Return  filter index or -1 if no filters available
 */
static int mpq_tspp_get_free_filter_slot(int tsif, int channel_id)
{
	int i;

	for (i = 0; i < TSPP_MAX_PID_FILTER_NUM; i++)
		if (mpq_dmx_tspp_info.tsif[tsif].filters[i].pid == -1)
			return i;

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
 * Demux thread function handling data from specific TSIF.
 *
 * @arg: TSIF number
 */
static int mpq_dmx_tspp_thread(void *arg)
{
	int tsif = (int)arg;
	struct mpq_demux *mpq_demux;
	const struct tspp_data_descriptor *tspp_data_desc;
	atomic_t *data_cnt;
	u32 notif_size;
	int channel_id;
	int ref_count;
	int ret;
	int j;

	do {
		ret = wait_event_interruptible(
			mpq_dmx_tspp_info.tsif[tsif].wait_queue,
			atomic_read(&mpq_dmx_tspp_info.tsif[tsif].data_cnt) ||
			kthread_should_stop());

		if ((ret < 0) || kthread_should_stop()) {
			MPQ_DVB_ERR_PRINT("%s: exit\n", __func__);
			break;
		}

		/* Lock against the TSPP filters data-structure */
		if (mutex_lock_interruptible(
			&mpq_dmx_tspp_info.tsif[tsif].mutex))
			return -ERESTARTSYS;

		channel_id = TSPP_CHANNEL_ID(tsif, TSPP_CHANNEL);

		ref_count = mpq_dmx_tspp_info.tsif[tsif].channel_ref;
		data_cnt = &mpq_dmx_tspp_info.tsif[tsif].data_cnt;

		/* Make sure channel is still active */
		if (ref_count == 0) {
			mutex_unlock(&mpq_dmx_tspp_info.tsif[tsif].mutex);
			continue;
		}

		atomic_dec(data_cnt);

		mpq_demux = mpq_dmx_tspp_info.tsif[tsif].mpq_demux;
		mpq_demux->hw_notification_size = 0;

		/*
		 * Go through all filled descriptors
		 * and perform demuxing on them
		 */
		while ((tspp_data_desc = tspp_get_buffer(0, channel_id))
				!= NULL) {
			notif_size = tspp_data_desc->size / TSPP_RAW_TTS_SIZE;
			mpq_demux->hw_notification_size += notif_size;

			for (j = 0; j < notif_size; j++)
				dvb_dmx_swfilter_packet(
				 &mpq_demux->demux,
				 ((u8 *)tspp_data_desc->virt_base) +
				 j * TSPP_RAW_TTS_SIZE,
				 ((u8 *)tspp_data_desc->virt_base) +
				 j * TSPP_RAW_TTS_SIZE + TSPP_RAW_SIZE);
			/*
			 * Notify TSPP that the buffer
			 * is no longer needed
			 */
			tspp_release_buffer(0, channel_id, tspp_data_desc->id);
		}

		if (mpq_demux->hw_notification_size &&
			(mpq_demux->hw_notification_size <
			mpq_demux->hw_notification_min_size))
			mpq_demux->hw_notification_min_size =
				mpq_demux->hw_notification_size;

		mutex_unlock(&mpq_dmx_tspp_info.tsif[tsif].mutex);
	} while (1);

	return 0;
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
	struct mpq_demux *mpq_demux;

	/* Save statistics on TSPP notifications */
	mpq_demux = mpq_dmx_tspp_info.tsif[tsif].mpq_demux;
	mpq_dmx_update_hw_statistics(mpq_demux);

	atomic_inc(&mpq_dmx_tspp_info.tsif[tsif].data_cnt);
	wake_up(&mpq_dmx_tspp_info.tsif[tsif].wait_queue);
}

/**
 * Free memory of channel output of specific TSIF.
 *
 * @tsif: The TSIF id to which memory should be freed.
 */
static void mpq_dmx_channel_mem_free(int tsif)
{
	MPQ_DVB_DBG_PRINT("%s(%d)\n", __func__, tsif);

	mpq_dmx_tspp_info.tsif[tsif].ch_mem_heap_phys_base = 0;

	if (!IS_ERR_OR_NULL(mpq_dmx_tspp_info.tsif[tsif].ch_mem_heap_handle)) {
		if (!IS_ERR_OR_NULL(mpq_dmx_tspp_info.tsif[tsif].
				ch_mem_heap_virt_base))
			ion_unmap_kernel(mpq_dmx_tspp_info.ion_client,
				mpq_dmx_tspp_info.tsif[tsif].
					ch_mem_heap_handle);

		ion_free(mpq_dmx_tspp_info.ion_client,
			mpq_dmx_tspp_info.tsif[tsif].ch_mem_heap_handle);
	}

	mpq_dmx_tspp_info.tsif[tsif].ch_mem_heap_virt_base = NULL;
	mpq_dmx_tspp_info.tsif[tsif].ch_mem_heap_handle = NULL;
}

/**
 * Allocate memory for channel output of specific TSIF.
 *
 * @tsif: The TSIF id to which memory should be allocated.
 *
 * Return  error status
 */
static int mpq_dmx_channel_mem_alloc(int tsif)
{
	int result;
	size_t len;

	MPQ_DVB_DBG_PRINT("%s(%d)\n", __func__, tsif);

	mpq_dmx_tspp_info.tsif[tsif].ch_mem_heap_handle =
		ion_alloc(mpq_dmx_tspp_info.ion_client,
		 (mpq_dmx_tspp_info.tsif[tsif].buffer_count *
		  TSPP_DESCRIPTOR_SIZE),
		 SZ_4K,
		 ION_HEAP(tspp_out_ion_heap),
		 0); /* non-cached */

	if (IS_ERR_OR_NULL(mpq_dmx_tspp_info.tsif[tsif].ch_mem_heap_handle)) {
		MPQ_DVB_ERR_PRINT("%s: ion_alloc() failed\n", __func__);
		mpq_dmx_channel_mem_free(tsif);
		return -ENOMEM;
	}

	/* save virtual base address of heap */
	mpq_dmx_tspp_info.tsif[tsif].ch_mem_heap_virt_base =
		ion_map_kernel(mpq_dmx_tspp_info.ion_client,
			mpq_dmx_tspp_info.tsif[tsif].ch_mem_heap_handle);
	if (IS_ERR_OR_NULL(mpq_dmx_tspp_info.tsif[tsif].
				ch_mem_heap_virt_base)) {
		MPQ_DVB_ERR_PRINT("%s: ion_map_kernel() failed\n", __func__);
		mpq_dmx_channel_mem_free(tsif);
		return -ENOMEM;
	}

	/* save physical base address of heap */
	result = ion_phys(mpq_dmx_tspp_info.ion_client,
		mpq_dmx_tspp_info.tsif[tsif].ch_mem_heap_handle,
		&(mpq_dmx_tspp_info.tsif[tsif].ch_mem_heap_phys_base), &len);
	if (result < 0) {
		MPQ_DVB_ERR_PRINT("%s: ion_phys() failed\n", __func__);
		mpq_dmx_channel_mem_free(tsif);
		return -ENOMEM;
	}

	return 0;
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
	struct tspp_select_source tspp_source;
	struct tspp_filter tspp_filter;
	int tsif;
	int ret;
	int channel_id;
	int *channel_ref_count;
	u32 buffer_size;

	tspp_source.clk_inverse = clock_inv;
	tspp_source.data_inverse = 0;
	tspp_source.sync_inverse = 0;
	tspp_source.enable_inverse = 0;

	switch (tsif_mode) {
	case 1:
		tspp_source.mode = TSPP_TSIF_MODE_1;
		break;
	case 2:
		tspp_source.mode = TSPP_TSIF_MODE_2;
		break;
	default:
		tspp_source.mode = TSPP_TSIF_MODE_LOOPBACK;
		break;
	}

	/* determine the TSIF we are reading from */
	if (mpq_demux->source == DMX_SOURCE_FRONT0) {
		tsif = 0;
		tspp_source.source = TSPP_SOURCE_TSIF0;
	} else if (mpq_demux->source == DMX_SOURCE_FRONT1) {
		tsif = 1;
		tspp_source.source = TSPP_SOURCE_TSIF1;
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

	channel_id = TSPP_CHANNEL_ID(tsif, TSPP_CHANNEL);
	channel_ref_count = &mpq_dmx_tspp_info.tsif[tsif].channel_ref;
	buffer_size = TSPP_DESCRIPTOR_SIZE;

	/* check if required TSPP pipe is already allocated or not */
	if (*channel_ref_count == 0) {
		if (allocation_mode == MPQ_DMX_TSPP_CONTIGUOUS_PHYS_ALLOC) {
			ret = mpq_dmx_channel_mem_alloc(tsif);
			if (ret < 0) {
				MPQ_DVB_ERR_PRINT(
					"%s: mpq_dmx_channel_mem_alloc(%d) failed (%d)\n",
					__func__,
					channel_id,
					ret);

				goto add_channel_failed;
			}
		}

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
		ret = tspp_open_stream(0, channel_id, &tspp_source);
		if (ret < 0) {
			MPQ_DVB_ERR_PRINT(
				"%s: tspp_select_source(%d,%d) failed (%d)\n",
				__func__,
				channel_id,
				tspp_source.source,
				ret);

			goto add_channel_close_ch;
		}

		/* register notification on TS packets */
		tspp_register_notification(0,
					   channel_id,
					   mpq_tspp_callback,
					   (void *)tsif,
					   tspp_channel_timeout);

		/* register allocater and provide allocation function
		 * that allocates from continous memory so that we can have
		 * big notification size, smallest descriptor, and still provide
		 * TZ with single big buffer based on notification size.
		 */
		if (allocation_mode == MPQ_DMX_TSPP_CONTIGUOUS_PHYS_ALLOC) {
			ret = tspp_allocate_buffers(0, channel_id,
				   mpq_dmx_tspp_info.tsif[tsif].buffer_count,
				   buffer_size, tspp_notification_size,
				   tspp_mem_allocator, tspp_mem_free, NULL);
		} else {
			ret = tspp_allocate_buffers(0, channel_id,
				   mpq_dmx_tspp_info.tsif[tsif].buffer_count,
				   buffer_size, tspp_notification_size,
				   NULL, NULL, NULL);
		}
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
	tspp_filter.source = tspp_source.source;
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
	if (allocation_mode == MPQ_DMX_TSPP_CONTIGUOUS_PHYS_ALLOC)
		mpq_dmx_channel_mem_free(tsif);

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
	atomic_t *data_cnt;
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

	channel_id = TSPP_CHANNEL_ID(tsif, TSPP_CHANNEL);
	channel_ref_count = &mpq_dmx_tspp_info.tsif[tsif].channel_ref;
	data_cnt = &mpq_dmx_tspp_info.tsif[tsif].data_cnt;

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
		tspp_close_stream(0, channel_id);
		atomic_set(data_cnt, 0);

		if (allocation_mode == MPQ_DMX_TSPP_CONTIGUOUS_PHYS_ALLOC)
			mpq_dmx_channel_mem_free(tsif);
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
			MPQ_DVB_ERR_PRINT(
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

	/* Buffer requirements */
	caps->section.flags =
		DMX_BUFFER_EXTERNAL_SUPPORT	|
		DMX_BUFFER_INTERNAL_SUPPORT;
	caps->section.max_buffer_num = 1;
	caps->section.max_size = 0xFFFFFFFF;
	caps->section.size_alignment = 0;
	caps->pes.flags =
		DMX_BUFFER_EXTERNAL_SUPPORT	|
		DMX_BUFFER_INTERNAL_SUPPORT;
	caps->pes.max_buffer_num = 1;
	caps->pes.max_size = 0xFFFFFFFF;
	caps->pes.size_alignment = 0;
	caps->recording_188_tsp.flags =
		DMX_BUFFER_EXTERNAL_SUPPORT	|
		DMX_BUFFER_INTERNAL_SUPPORT;
	caps->recording_188_tsp.max_buffer_num = 1;
	caps->recording_188_tsp.max_size = 0xFFFFFFFF;
	caps->recording_188_tsp.size_alignment = 0;
	caps->recording_192_tsp.flags =
		DMX_BUFFER_EXTERNAL_SUPPORT	|
		DMX_BUFFER_INTERNAL_SUPPORT;
	caps->recording_192_tsp.max_buffer_num = 1;
	caps->recording_192_tsp.max_size = 0xFFFFFFFF;
	caps->recording_192_tsp.size_alignment = 0;
	caps->playback_188_tsp.flags =
		DMX_BUFFER_EXTERNAL_SUPPORT	|
		DMX_BUFFER_INTERNAL_SUPPORT;
	caps->playback_188_tsp.max_buffer_num = 1;
	caps->playback_188_tsp.max_size = 0xFFFFFFFF;
	caps->playback_188_tsp.size_alignment = 0;
	caps->playback_192_tsp.flags =
		DMX_BUFFER_EXTERNAL_SUPPORT	|
		DMX_BUFFER_INTERNAL_SUPPORT;
	caps->playback_192_tsp.max_buffer_num = 1;
	caps->playback_192_tsp.max_size = 0xFFFFFFFF;
	caps->playback_192_tsp.size_alignment = 0;
	caps->decoder.flags =
		DMX_BUFFER_CONTIGUOUS_MEM	|
		DMX_BUFFER_SECURED_IF_DECRYPTED	|
		DMX_BUFFER_EXTERNAL_SUPPORT	|
		DMX_BUFFER_INTERNAL_SUPPORT	|
		DMX_BUFFER_LINEAR_GROUP_SUPPORT;
	caps->decoder.max_buffer_num = DMX_MAX_DECODER_BUFFER_NUM;
	caps->decoder.max_size = 0xFFFFFFFF;
	caps->decoder.size_alignment = SZ_4K;

	return 0;
}

static int mpq_tspp_dmx_init(
			struct dvb_adapter *mpq_adapter,
			struct mpq_demux *mpq_demux)
{
	int result;

	MPQ_DVB_DBG_PRINT("%s executed\n", __func__);

	mpq_dmx_tspp_info.ion_client = mpq_demux->ion_client;

	/* Set the kernel-demux object capabilities */
	mpq_demux->demux.dmx.capabilities =
		DMX_TS_FILTERING			|
		DMX_PES_FILTERING			|
		DMX_SECTION_FILTERING			|
		DMX_MEMORY_BASED_FILTERING		|
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

	mpq_demux->demux.reuse_decoder_buffer =
		mpq_dmx_reuse_decoder_buffer;

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
	mpq_demux->dmxdev.demux->map_buffer = mpq_dmx_map_buffer;
	mpq_demux->dmxdev.demux->unmap_buffer = mpq_dmx_unmap_buffer;

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
		mpq_dmx_tspp_info.tsif[i].buffer_count =
				TSPP_BUFFER_COUNT(tspp_out_buffer_size);

		mpq_dmx_tspp_info.tsif[i].channel_ref = 0;
		mpq_dmx_tspp_info.tsif[i].buff_index = 0;
		mpq_dmx_tspp_info.tsif[i].ch_mem_heap_handle = NULL;
		mpq_dmx_tspp_info.tsif[i].ch_mem_heap_virt_base = NULL;
		mpq_dmx_tspp_info.tsif[i].ch_mem_heap_phys_base = 0;
		atomic_set(&mpq_dmx_tspp_info.tsif[i].data_cnt, 0);

		for (j = 0; j < TSPP_MAX_PID_FILTER_NUM; j++) {
			mpq_dmx_tspp_info.tsif[i].filters[j].pid = -1;
			mpq_dmx_tspp_info.tsif[i].filters[j].ref_count = 0;
		}

		snprintf(mpq_dmx_tspp_info.tsif[i].name,
				TSIF_NAME_LENGTH,
				"dmx_tsif%d",
				i);

		init_waitqueue_head(&mpq_dmx_tspp_info.tsif[i].wait_queue);
		mpq_dmx_tspp_info.tsif[i].thread =
			kthread_run(
				mpq_dmx_tspp_thread, (void *)i,
				mpq_dmx_tspp_info.tsif[i].name);

		if (IS_ERR(mpq_dmx_tspp_info.tsif[i].thread)) {
			for (j = 0; j < i; j++) {
				kthread_stop(mpq_dmx_tspp_info.tsif[j].thread);
				mutex_destroy(&mpq_dmx_tspp_info.tsif[j].mutex);
			}

			MPQ_DVB_ERR_PRINT(
				"%s: kthread_run failed\n",
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
			kthread_stop(mpq_dmx_tspp_info.tsif[i].thread);
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

		/*
		 * Note: tspp_close_channel will also free the TSPP buffers
		 * even if we allocated them ourselves,
		 * using our free function.
		 */
		if (mpq_dmx_tspp_info.tsif[i].channel_ref) {
			tspp_unregister_notification(0,
				TSPP_CHANNEL_ID(i, TSPP_CHANNEL));
			tspp_close_channel(0,
				TSPP_CHANNEL_ID(i, TSPP_CHANNEL));

			if (allocation_mode ==
				MPQ_DMX_TSPP_CONTIGUOUS_PHYS_ALLOC)
				mpq_dmx_channel_mem_free(i);
		}

		mutex_unlock(&mpq_dmx_tspp_info.tsif[i].mutex);
		kthread_stop(mpq_dmx_tspp_info.tsif[i].thread);
		mutex_destroy(&mpq_dmx_tspp_info.tsif[i].mutex);
	}

	mpq_dmx_plugin_exit();
}


module_init(mpq_dmx_tspp_plugin_init);
module_exit(mpq_dmx_tspp_plugin_exit);

MODULE_DESCRIPTION("Qualcomm demux TSPP version 1 HW Plugin");
MODULE_LICENSE("GPL v2");


