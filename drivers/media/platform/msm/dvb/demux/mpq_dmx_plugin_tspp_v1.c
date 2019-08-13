// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/vmalloc.h>
#include <linux/dma-buf.h>
#include <linux/ion_kernel.h>
#include <linux/qcom_tspp.h>
#include <linux/platform_device.h>
#include "mpq_dvb_debug.h"
#include "mpq_dmx_plugin_common.h"

#define TSIF_COUNT			2

/* Max number of PID filters */
#define TSPP_MAX_PID_FILTER_NUM		128

/* Max number of user-defined HW PID filters */
#define TSPP_MAX_HW_PID_FILTER_NUM	15

/* HW index  of the last entry in the TSPP HW filter table */
#define TSPP_LAST_HW_FILTER_INDEX	15

/* Number of filters required to accept all packets except NULL packets */
#define TSPP_BLOCK_NULLS_FILTERS_NUM	13

/* Max number of section filters */
#define TSPP_MAX_SECTION_FILTER_NUM	128

/* For each TSIF we use a single pipe holding the data after PID filtering */
#define TSPP_CHANNEL			0

/* the channel_id set to TSPP driver based on TSIF number and channel type */
#define TSPP_CHANNEL_ID(tsif, ch)		((tsif << 1) + ch)
#define TSPP_GET_TSIF_NUM(ch_id)		(ch_id >> 1)

/* mask that set to care for all bits in pid filter */
#define TSPP_PID_MASK			0x1FFF

/* dvb-demux defines pid 0x2000 as full capture pid */
#define TSPP_PASS_THROUGH_PID		0x2000

/* NULL packets pid */
#define TSPP_NULL_PACKETS_PID		0x1FFF

#define TSPP_RAW_TTS_SIZE		192
#define TSPP_RAW_SIZE			188

#define MAX_BAM_DESCRIPTOR_SIZE	(32 * 1024 - 1)

#define MAX_BAM_DESCRIPTOR_COUNT	(8 * 1024 - 2)

#define TSPP_BUFFER_SIZE		(500 * 1024) /* 500KB */

#define TSPP_DEFAULT_DESCRIPTOR_SIZE	(TSPP_RAW_TTS_SIZE)

#define TSPP_BUFFER_COUNT(buffer_size)	\
	((buffer_size) / tspp_desc_size)

/* When TSPP notifies demux that new packets are received.
 * Using max descriptor size (170 packets).
 * Assuming 20MBit/sec stream, with 170 packets
 * per descriptor there would be about 82 descriptors,
 * Meaning about 82 notifications per second.
 */
#define TSPP_NOTIFICATION_SIZE(desc_size)		\
	(MAX_BAM_DESCRIPTOR_SIZE / (desc_size))

/* Channel timeout in msec */
#define TSPP_CHANNEL_TIMEOUT			100

enum mem_buffer_allocation_mode {
	MPQ_DMX_TSPP_INTERNAL_ALLOC = 0,
	MPQ_DMX_TSPP_CONTIGUOUS_PHYS_ALLOC = 1
};

/* TODO: Convert below module parameters to sysfs tunables */
/* module parameters for load time configuration */
static int allocation_mode = MPQ_DMX_TSPP_INTERNAL_ALLOC;
static int tspp_out_buffer_size = TSPP_BUFFER_SIZE;
static int tspp_desc_size = TSPP_DEFAULT_DESCRIPTOR_SIZE;
static int tspp_notification_size =
	TSPP_NOTIFICATION_SIZE(TSPP_DEFAULT_DESCRIPTOR_SIZE);
static int tspp_channel_timeout = TSPP_CHANNEL_TIMEOUT;

/* The following structure hold singleton information
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

		/* flag to indicate control operation is in progress */
		atomic_t control_op;

		/* ION handle used for TSPP data buffer allocation */
		struct ion_handle *ch_mem_heap_handle;

		/* TSPP data buffer heap virtual base address */
		void *ch_mem_heap_virt_base;

		/* TSPP data buffer heap physical base address */
		phys_addr_t ch_mem_heap_phys_base;

		/* Buffer allocation index */
		int buff_index;

		/* Number of buffers */
		u32 buffer_count;

		/*
		 * Array holding the IDs of the TSPP buffer descriptors in the
		 * current aggregate, in order to release these descriptors at
		 * the end of processing.
		 */
		int *aggregate_ids;

		/*
		 * Holds PIDs of allocated filters along with
		 * how many feeds are opened on the same PID. For
		 * TSPP HW filters, holds also the filter table index.
		 * When pid == -1, the entry is free.
		 */
		struct {
			int pid;
			int ref_count;
			int hw_index;
		} filters[TSPP_MAX_PID_FILTER_NUM];

		/* Indicates available/allocated filter table indexes */
		int hw_indexes[TSPP_MAX_HW_PID_FILTER_NUM];

		/* Number of currently allocated PID filters */
		u16 current_filter_count;

		/*
		 * Flag to indicate whether the user added a filter to accept
		 * NULL packets (PID = 0x1FFF)
		 */
		int pass_nulls_flag;

		/*
		 * Flag to indicate whether the user added a filter to accept
		 * all packets (PID = 0x2000)
		 */
		int pass_all_flag;

		/*
		 * Flag to indicate whether the filter that accepts
		 * all packets has already been added and is
		 * currently enabled
		 */
		int accept_all_filter_exists_flag;

		/* Thread processing TS packets from TSPP */
		struct task_struct *thread;
		wait_queue_head_t wait_queue;

		/* TSIF alias */
		char name[TSIF_NAME_LENGTH];

		/* Pointer to the demux connected to this TSIF */
		struct mpq_demux *mpq_demux;

		/* Mutex protecting the data-structure */
		struct mutex mutex;

		/* ion dma buffer mapping structure */
		struct tspp_ion_dma_buf_info ch_ion_dma_buf;

	} tsif[TSIF_COUNT];

	/* ION client used for TSPP data buffer allocation */
	struct ion_client *ion_client;
} mpq_dmx_tspp_info;

static void *tspp_mem_allocator(int channel_id, u32 size,
				phys_addr_t *phys_base, dma_addr_t *dma_base,
				void *user)
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
			void *virt_base, phys_addr_t phys_base, void *user)
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
 * Returns a free HW filter index that can be used.
 *
 * @tsif: The TSIF to allocate filter from
 *
 * Return  HW filter index or -ENOMEM if no filters available
 */
static int mpq_tspp_allocate_hw_filter_index(int tsif)
{
	int i;

	for (i = 0; i < TSPP_MAX_HW_PID_FILTER_NUM; i++) {
		if (mpq_dmx_tspp_info.tsif[tsif].hw_indexes[i] == 0) {
			mpq_dmx_tspp_info.tsif[tsif].hw_indexes[i] = 1;
			return i;
		}
	}

	return -ENOMEM;
}

/**
 * Releases a HW filter index for future reuse.
 *
 * @tsif: The TSIF from which the filter should be released
 * @hw_index: The HW index to release
 *
 */
static inline void mpq_tspp_release_hw_filter_index(int tsif, int hw_index)
{
	if ((hw_index >= 0) && (hw_index < TSPP_MAX_HW_PID_FILTER_NUM))
		mpq_dmx_tspp_info.tsif[tsif].hw_indexes[hw_index] = 0;
}


/**
 * Returns a free filter slot that can be used.
 *
 * @tsif: The TSIF to allocate filter from
 *
 * Return  filter index or -ENOMEM if no filters available
 */
static int mpq_tspp_get_free_filter_slot(int tsif)
{
	int slot;

	for (slot = 0; slot < TSPP_MAX_PID_FILTER_NUM; slot++)
		if (mpq_dmx_tspp_info.tsif[tsif].filters[slot].pid == -1)
			return slot;

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
	int slot;

	for (slot = 0; slot < TSPP_MAX_PID_FILTER_NUM; slot++)
		if (mpq_dmx_tspp_info.tsif[tsif].filters[slot].pid == pid)
			return slot;

	return -EINVAL;
}

/**
 * mpq_dmx_tspp_swfilter_desc - helper function
 *
 * Takes a tspp buffer descriptor and send it to the SW filter for demuxing,
 * one TS packet at a time.
 *
 * @mpq_demux - mpq demux object
 * @tspp_data_desc - tspp buffer descriptor
 */
static inline void mpq_dmx_tspp_swfilter_desc(struct mpq_demux *mpq_demux,
	const struct tspp_data_descriptor *tspp_data_desc)
{
	u32 notif_size;
	int i;

	notif_size = tspp_data_desc->size / TSPP_RAW_TTS_SIZE;
	for (i = 0; i < notif_size; i++)
		dvb_dmx_swfilter_packet(&mpq_demux->demux,
			((u8 *)tspp_data_desc->virt_base) +
			i * TSPP_RAW_TTS_SIZE,
			((u8 *)tspp_data_desc->virt_base) +
			i * TSPP_RAW_TTS_SIZE + TSPP_RAW_SIZE);
}

/**
 * Demux TS packets from TSPP by secure-demux.
 * The function assumes the buffer is physically contiguous
 * and that TSPP descriptors are continuous in memory.
 *
 * @tsif: The TSIF interface to process its packets
 * @channel_id: the TSPP output pipe with the TS packets
 */
static void mpq_dmx_tspp_aggregated_process(int tsif, int channel_id)
{
	const struct tspp_data_descriptor *tspp_data_desc;
	struct mpq_demux *mpq_demux = mpq_dmx_tspp_info.tsif[tsif].mpq_demux;
	struct sdmx_buff_descr input;
	size_t aggregate_len = 0;
	size_t aggregate_count = 0;
	phys_addr_t buff_start_addr_phys;
	phys_addr_t buff_current_addr_phys = 0;
	u32 notif_size;
	int i;

	while ((tspp_data_desc = tspp_get_buffer(0, channel_id)) != NULL) {
		if (aggregate_count == 0)
			buff_current_addr_phys = tspp_data_desc->phys_base;
		notif_size = tspp_data_desc->size / TSPP_RAW_TTS_SIZE;
		mpq_dmx_tspp_info.tsif[tsif].aggregate_ids[aggregate_count] =
			tspp_data_desc->id;
		aggregate_len += tspp_data_desc->size;
		aggregate_count++;
		mpq_demux->hw_notification_size += notif_size;

		/* Let SW filter process only if it might be relevant */
		if (mpq_demux->num_active_feeds > mpq_demux->num_secure_feeds)
			mpq_dmx_tspp_swfilter_desc(mpq_demux, tspp_data_desc);

	}

	if (!aggregate_count)
		return;

	buff_start_addr_phys =
		mpq_dmx_tspp_info.tsif[tsif].ch_mem_heap_phys_base;

	input.base_addr = (u64)buff_start_addr_phys;
	input.size = mpq_dmx_tspp_info.tsif[tsif].buffer_count * tspp_desc_size;

	if (mpq_sdmx_is_loaded() && mpq_demux->sdmx_filter_count) {
		MPQ_DVB_DBG_PRINT(
			"%s: SDMX Processing %zu descriptors: %zu bytes at start address 0x%llx, read offset %d\n",
			__func__, aggregate_count, aggregate_len,
			input.base_addr,
			(int)(buff_current_addr_phys - buff_start_addr_phys));

		mpq_sdmx_process(mpq_demux, &input, aggregate_len,
			buff_current_addr_phys - buff_start_addr_phys,
			TSPP_RAW_TTS_SIZE);
	}

	for (i = 0; i < aggregate_count; i++)
		tspp_release_buffer(0, channel_id,
			mpq_dmx_tspp_info.tsif[tsif].aggregate_ids[i]);
}


/**
 * Demux thread function handling data from specific TSIF.
 *
 * @arg: TSIF number
 */
static int mpq_dmx_tspp_thread(void *arg)
{
	int tsif = (int)(uintptr_t)arg;
	struct mpq_demux *mpq_demux;
	const struct tspp_data_descriptor *tspp_data_desc;
	atomic_t *data_cnt;
	u32 notif_size;
	int channel_id;
	int ref_count;
	int ret;

	do {
		ret = wait_event_interruptible(
			mpq_dmx_tspp_info.tsif[tsif].wait_queue,
			(atomic_read(&mpq_dmx_tspp_info.tsif[tsif].data_cnt) &&
			!atomic_read(&mpq_dmx_tspp_info.tsif[tsif].control_op))
			|| kthread_should_stop());

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

		if (allocation_mode != MPQ_DMX_TSPP_CONTIGUOUS_PHYS_ALLOC &&
			mpq_sdmx_is_loaded())
			pr_err_once(
				"%s: TSPP Allocation mode does not support secure demux.\n",
				__func__);

		if (allocation_mode == MPQ_DMX_TSPP_CONTIGUOUS_PHYS_ALLOC &&
			mpq_sdmx_is_loaded()) {
			mpq_dmx_tspp_aggregated_process(tsif, channel_id);
		} else {
			/*
			 * Go through all filled descriptors
			 * and perform demuxing on them
			 */
			do {
				if (atomic_read(
				  &mpq_dmx_tspp_info.tsif[tsif].control_op)) {
					/* restore for next iteration */
					atomic_inc(data_cnt);
					break;
				}
				tspp_data_desc = tspp_get_buffer(0, channel_id);
				if (!tspp_data_desc)
					break;

				notif_size = tspp_data_desc->size /
					TSPP_RAW_TTS_SIZE;
				mpq_demux->hw_notification_size += notif_size;

				mpq_dmx_tspp_swfilter_desc(mpq_demux,
					tspp_data_desc);
				/*
				 * Notify TSPP that the buffer
				 * is no longer needed
				 */
				tspp_release_buffer(0, channel_id,
					tspp_data_desc->id);
			} while (1);
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
	int tsif = (int)(uintptr_t)user;
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
	int size = 0;

	size = (mpq_dmx_tspp_info.tsif[tsif].buffer_count * tspp_desc_size);
	size = ALIGN(size, SZ_4K);


	tspp_free_dma_buffer(0, size,
	    (void *)mpq_dmx_tspp_info.tsif[tsif].ch_mem_heap_virt_base,
	    (dma_addr_t)mpq_dmx_tspp_info.tsif[tsif].ch_mem_heap_phys_base);

	mpq_dmx_tspp_info.tsif[tsif].ch_mem_heap_phys_base = 0;
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
	int size = 0;

	size = (mpq_dmx_tspp_info.tsif[tsif].buffer_count * tspp_desc_size);

	size = ALIGN(size, SZ_4K);

	mpq_dmx_tspp_info.tsif[tsif].ch_mem_heap_virt_base
		= tspp_allocate_dma_buffer(0, size,
			&mpq_dmx_tspp_info.tsif[tsif].ch_mem_heap_phys_base);

	if (IS_ERR_OR_NULL(
	     mpq_dmx_tspp_info.tsif[tsif].ch_mem_heap_virt_base)) {
		MPQ_DVB_ERR_PRINT("%s: ion_map_kernel() failed\n", __func__);
		mpq_dmx_channel_mem_free(tsif);
		return -ENOMEM;
	}
	return 0;
}

/**
 * Add a filter to accept all packets as the last entry
 * of the TSPP HW filter table.
 *
 * @channel_id: Channel ID number.
 * @source: TSPP source.
 *
 * Return  error status
 */
static int mpq_tspp_add_accept_all_filter(int channel_id,
				enum tspp_source source)
{
	struct tspp_filter tspp_filter;
	int tsif = TSPP_GET_TSIF_NUM(channel_id);
	int ret;

	MPQ_DVB_DBG_PRINT("%s: executed, channel id = %d, source = %d\n",
		__func__, channel_id, source);

	if (mpq_dmx_tspp_info.tsif[tsif].accept_all_filter_exists_flag) {
		MPQ_DVB_DBG_PRINT("%s: accept all filter already exists\n",
				__func__);
		return 0;
	}

	/* This filter will be the last entry in the table */
	tspp_filter.priority = TSPP_LAST_HW_FILTER_INDEX;
	/* Pass all pids - set mask to 0 */
	tspp_filter.pid = 0;
	tspp_filter.mask = 0;
	/*
	 * Include TTS in RAW packets, if you change this to
	 * TSPP_MODE_RAW_NO_SUFFIX you must also change TSPP_RAW_TTS_SIZE
	 * accordingly.
	 */
	tspp_filter.mode = TSPP_MODE_RAW;
	tspp_filter.source = source;
	tspp_filter.decrypt = 0;

	ret = tspp_add_filter(0, channel_id, &tspp_filter);
	if (!ret) {
		mpq_dmx_tspp_info.tsif[tsif].accept_all_filter_exists_flag = 1;
		MPQ_DVB_DBG_PRINT(
				"%s: accept all filter added successfully\n",
				__func__);
	}

	return ret;
}

/**
 * Remove the filter that accepts all packets from the last entry
 * of the TSPP HW filter table.
 *
 * @channel_id: Channel ID number.
 * @source: TSPP source.
 *
 * Return  error status
 */
static int mpq_tspp_remove_accept_all_filter(int channel_id,
				enum tspp_source source)
{
	struct tspp_filter tspp_filter;
	int tsif = TSPP_GET_TSIF_NUM(channel_id);
	int ret;

	MPQ_DVB_DBG_PRINT("%s: executed, channel id = %d, source = %d\n",
		__func__, channel_id, source);

	if (mpq_dmx_tspp_info.tsif[tsif].accept_all_filter_exists_flag == 0) {
		MPQ_DVB_DBG_PRINT("%s: accept all filter doesn't exist\n",
				__func__);
		return 0;
	}

	tspp_filter.priority = TSPP_LAST_HW_FILTER_INDEX;

	ret = tspp_remove_filter(0, channel_id, &tspp_filter);
	if (!ret) {
		mpq_dmx_tspp_info.tsif[tsif].accept_all_filter_exists_flag = 0;
		MPQ_DVB_DBG_PRINT(
			"%s: accept all filter removed successfully\n",
			__func__);
	}

	return ret;
}

/**
 * Add filters designed to accept all packets except NULL packets, i.e.
 * packets with PID = 0x1FFF.
 * This function is called after user-defined filters were removed,
 * so it assumes that the first 13 HW filters in the TSPP filter
 * table are free for use.
 *
 * @channel_id: Channel ID number.
 * @source: TSPP source.
 *
 * Return  0 on success, -1 otherwise
 */
static int mpq_tspp_add_null_blocking_filters(int channel_id,
				enum tspp_source source)
{
	struct tspp_filter tspp_filter;
	int ret = 0;
	int i, j;
	u16 full_pid_mask = 0x1FFF;
	u8 mask_shift;
	u8 pid_shift;
	int tsif = TSPP_GET_TSIF_NUM(channel_id);

	MPQ_DVB_DBG_PRINT("%s: executed, channel id = %d, source = %d\n",
		__func__, channel_id, source);

	/*
	 * Add a total of 13 filters that will accept packets with
	 * every PID other than 0x1FFF, which is the NULL PID.
	 *
	 * Filter 0: accept all PIDs with bit 12 clear, i.e.
	 * PID = 0x0000 .. 0x0FFF (4096 PIDs in total):
	 * Mask = 0x1000, PID = 0x0000.
	 *
	 * Filter 12: Accept PID 0x1FFE:
	 * Mask = 0x1FFF, PID = 0x1FFE.
	 *
	 * In general: For N = 0 .. 12,
	 * Filter <N>: accept all PIDs with <N> MSBits set and bit <N-1> clear.
	 * Filter <N> Mask = N+1 MSBits set, others clear.
	 * Filter <N> PID = <N> MSBits set, others clear.
	 */

	/*
	 * Include TTS in RAW packets, if you change this to
	 * TSPP_MODE_RAW_NO_SUFFIX you must also change TSPP_RAW_TTS_SIZE
	 * accordingly.
	 */
	tspp_filter.mode = TSPP_MODE_RAW;
	tspp_filter.source = source;
	tspp_filter.decrypt = 0;

	for (i = 0; i < TSPP_BLOCK_NULLS_FILTERS_NUM; i++) {
		tspp_filter.priority = mpq_tspp_allocate_hw_filter_index(tsif);
		if (tspp_filter.priority != i) {
			MPQ_DVB_ERR_PRINT(
				"%s: got unexpected HW index %d, expected %d\n",
				__func__, tspp_filter.priority, i);
			ret = -1;
			break;
		}
		mask_shift = (TSPP_BLOCK_NULLS_FILTERS_NUM - 1 - i);
		pid_shift = (TSPP_BLOCK_NULLS_FILTERS_NUM - i);
		tspp_filter.mask =
			((full_pid_mask >> mask_shift) << mask_shift);
		tspp_filter.pid = ((full_pid_mask >> pid_shift) << pid_shift);

		if (tspp_add_filter(0, channel_id, &tspp_filter)) {
			ret = -1;
			break;
		}
	}

	if (ret) {
		/* cleanup on failure */
		for (j = 0; j < i; j++) {
			tspp_filter.priority = j;
			mpq_tspp_release_hw_filter_index(tsif, j);
			tspp_remove_filter(0, channel_id, &tspp_filter);
		}
	} else {
		MPQ_DVB_DBG_PRINT(
			"%s: NULL blocking filters added successfully\n",
			__func__);
	}

	return ret;
}

/**
 * Remove filters designed to accept all packets except NULL packets, i.e.
 * packets with PID = 0x1FFF.
 *
 * @channel_id: Channel ID number.
 *
 * @source: TSPP source.
 *
 * Return  0 on success, -1 otherwise
 */
static int mpq_tspp_remove_null_blocking_filters(int channel_id,
				enum tspp_source source)
{
	struct tspp_filter tspp_filter;
	int tsif = TSPP_GET_TSIF_NUM(channel_id);
	int ret = 0;
	int i;

	MPQ_DVB_DBG_PRINT("%s: executed, channel id = %d, source = %d\n",
		__func__, channel_id, source);

	for (i = 0; i < TSPP_BLOCK_NULLS_FILTERS_NUM; i++) {
		tspp_filter.priority = i;
		if (tspp_remove_filter(0, channel_id, &tspp_filter)) {
			MPQ_DVB_ERR_PRINT("%s: failed to remove filter %d\n",
				__func__, i);
			ret = -1;
		}

		mpq_tspp_release_hw_filter_index(tsif, i);
	}

	return ret;
}

/**
 * Add all current user-defined filters (up to 15) as HW filters
 *
 * @channel_id: Channel ID number.
 *
 * @source: TSPP source.
 *
 * Return  0 on success, -1 otherwise
 */
static int mpq_tspp_add_all_user_filters(int channel_id,
				enum tspp_source source)
{
	struct tspp_filter tspp_filter;
	int tsif = TSPP_GET_TSIF_NUM(channel_id);
	int slot;
	u16 added_count = 0;
	u16 total_filters_count = 0;

	MPQ_DVB_DBG_PRINT("%s: executed\n", __func__);

	/*
	 * Include TTS in RAW packets, if you change this to
	 * TSPP_MODE_RAW_NO_SUFFIX you must also change TSPP_RAW_TTS_SIZE
	 * accordingly.
	 */
	tspp_filter.mode = TSPP_MODE_RAW;
	tspp_filter.source = source;
	tspp_filter.decrypt = 0;

	for (slot = 0; slot < TSPP_MAX_PID_FILTER_NUM; slot++) {
		if (mpq_dmx_tspp_info.tsif[tsif].filters[slot].pid == -1)
			continue;

		/*
		 * count total number of user filters to verify that it is
		 * exactly TSPP_MAX_HW_PID_FILTER_NUM as expected.
		 */
		total_filters_count++;

		if (added_count > TSPP_MAX_HW_PID_FILTER_NUM)
			continue;

		tspp_filter.priority = mpq_tspp_allocate_hw_filter_index(tsif);

		if (mpq_dmx_tspp_info.tsif[tsif].filters[slot].pid ==
				TSPP_PASS_THROUGH_PID) {
			/* pass all pids */
			tspp_filter.pid = 0;
			tspp_filter.mask = 0;
		} else {
			tspp_filter.pid =
				mpq_dmx_tspp_info.tsif[tsif].filters[slot].pid;
			tspp_filter.mask = TSPP_PID_MASK;
		}

		MPQ_DVB_DBG_PRINT(
			"%s: adding HW filter, PID = %d, mask = 0x%X, index = %d\n",
				__func__, tspp_filter.pid, tspp_filter.mask,
				tspp_filter.priority);

		if (!tspp_add_filter(0, channel_id, &tspp_filter)) {
			mpq_dmx_tspp_info.tsif[tsif].filters[slot].hw_index =
				tspp_filter.priority;
			added_count++;
		} else {
			MPQ_DVB_ERR_PRINT("%s: tspp_add_filter failed\n",
						__func__);
		}
	}

	if ((added_count != TSPP_MAX_HW_PID_FILTER_NUM) ||
		(added_count != total_filters_count))
		return -EINVAL;

	return 0;
}

/**
 * Remove all user-defined HW filters
 *
 * @channel_id: Channel ID number.
 *
 * @source: TSPP source.
 *
 * Return  0 on success, -1 otherwise
 */
static int mpq_tspp_remove_all_user_filters(int channel_id,
				enum tspp_source source)
{
	struct tspp_filter tspp_filter;
	int ret = 0;
	int tsif = TSPP_GET_TSIF_NUM(channel_id);
	int i;

	MPQ_DVB_DBG_PRINT("%s: executed\n", __func__);

	for (i = 0; i < TSPP_MAX_HW_PID_FILTER_NUM; i++) {
		tspp_filter.priority = i;
		MPQ_DVB_DBG_PRINT("%s: Removing HW filter %d\n",
			__func__, tspp_filter.priority);
		if (tspp_remove_filter(0, channel_id, &tspp_filter))
			ret = -1;

		mpq_tspp_release_hw_filter_index(tsif, i);
		mpq_dmx_tspp_info.tsif[tsif].filters[i].hw_index = -1;
	}

	return ret;
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
	int tsif_mode = mpq_dmx_get_param_tsif_mode();
	int ret = 0;
	int slot;
	int channel_id;
	int *channel_ref_count;
	u32 buffer_size;
	int restore_user_filters = 0;
	int remove_accept_all_filter = 0;
	int remove_null_blocking_filters = 0;
	size_t agg_size;

	tspp_source.clk_inverse = mpq_dmx_get_param_clock_inv();
	tspp_source.data_inverse = 0;
	tspp_source.sync_inverse = 0;
	tspp_source.enable_inverse = 0;

	MPQ_DVB_DBG_PRINT("%s: executed, PID = %d\n", __func__, feed->pid);

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

	atomic_inc(&mpq_dmx_tspp_info.tsif[tsif].control_op);
	if (mutex_lock_interruptible(&mpq_dmx_tspp_info.tsif[tsif].mutex)) {
		atomic_dec(&mpq_dmx_tspp_info.tsif[tsif].control_op);
		return -ERESTARTSYS;
	}

	/*
	 * It is possible that this PID was already requested before.
	 * Can happen if we play and record same PES or PCR
	 * piggypacked on video packet.
	 */
	slot = mpq_tspp_get_filter_slot(tsif, feed->pid);
	if (slot >= 0) {
		/* PID already configured */
		mpq_dmx_tspp_info.tsif[tsif].filters[slot].ref_count++;
		goto out;
	}


	channel_id = TSPP_CHANNEL_ID(tsif, TSPP_CHANNEL);
	channel_ref_count = &mpq_dmx_tspp_info.tsif[tsif].channel_ref;

	/*
	 * Recalculate 'tspp_notification_size' and buffer count in case
	 * 'tspp_desc_size' or 'tspp_out_buffer_size' parameters have changed.
	 */
	buffer_size = tspp_desc_size;
	tspp_notification_size = TSPP_NOTIFICATION_SIZE(tspp_desc_size);
	mpq_dmx_tspp_info.tsif[tsif].buffer_count =
			TSPP_BUFFER_COUNT(tspp_out_buffer_size);
	if (mpq_dmx_tspp_info.tsif[tsif].buffer_count >
			MAX_BAM_DESCRIPTOR_COUNT)
		mpq_dmx_tspp_info.tsif[tsif].buffer_count =
			MAX_BAM_DESCRIPTOR_COUNT;

	/* check if required TSPP pipe is already allocated or not */
	if (*channel_ref_count == 0) {
		if (allocation_mode == MPQ_DMX_TSPP_CONTIGUOUS_PHYS_ALLOC) {
			agg_size = mpq_dmx_tspp_info.tsif[tsif].buffer_count *
				sizeof(int);
			mpq_dmx_tspp_info.tsif[tsif].aggregate_ids =
					vzalloc(agg_size);
			if (!mpq_dmx_tspp_info.tsif[tsif].aggregate_ids) {
				MPQ_DVB_ERR_PRINT(
					"%s: Failed to allocate memory for buffer descriptors aggregation\n",
					__func__);
				ret = -ENOMEM;
				goto out;
			}

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
					   (void *)(uintptr_t)tsif,
					   tspp_channel_timeout);

		/*
		 * Register allocator and provide allocation function
		 * that allocates from contiguous memory so that we can have
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
	slot = mpq_tspp_get_free_filter_slot(tsif);
	if (slot < 0) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_tspp_get_free_filter_slot(%d) failed\n",
			__func__, tsif);

		goto add_channel_unregister_notif;
	}

	if (feed->pid == TSPP_PASS_THROUGH_PID)
		mpq_dmx_tspp_info.tsif[tsif].pass_all_flag = 1;
	else if (feed->pid == TSPP_NULL_PACKETS_PID)
		mpq_dmx_tspp_info.tsif[tsif].pass_nulls_flag = 1;

	mpq_dmx_tspp_info.tsif[tsif].filters[slot].pid = feed->pid;
	mpq_dmx_tspp_info.tsif[tsif].filters[slot].ref_count++;

	tspp_filter.priority = -1;

	if (mpq_dmx_tspp_info.tsif[tsif].current_filter_count <
					TSPP_MAX_HW_PID_FILTER_NUM) {
		/* HW filtering mode */
		tspp_filter.priority = mpq_tspp_allocate_hw_filter_index(tsif);
		if (tspp_filter.priority < 0)
			goto add_channel_free_filter_slot;

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
		 * TSPP_MODE_RAW_NO_SUFFIX you must also change
		 * TSPP_RAW_TTS_SIZE accordingly.
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
		mpq_dmx_tspp_info.tsif[tsif].filters[slot].hw_index =
			tspp_filter.priority;

		MPQ_DVB_DBG_PRINT(
			"%s: HW filtering mode: added TSPP HW filter, PID = %d, mask = 0x%X, index = %d\n",
			__func__, tspp_filter.pid, tspp_filter.mask,
			tspp_filter.priority);
	} else if (mpq_dmx_tspp_info.tsif[tsif].current_filter_count ==
					TSPP_MAX_HW_PID_FILTER_NUM) {
		/* Crossing the threshold - from HW to SW filtering mode */

		/* Add a temporary filter to accept all packets */
		ret = mpq_tspp_add_accept_all_filter(channel_id,
					tspp_source.source);
		if (ret < 0) {
			MPQ_DVB_ERR_PRINT(
				"%s: mpq_tspp_add_accept_all_filter(%d, %d) failed\n",
				__func__, channel_id, tspp_source.source);

			goto add_channel_free_filter_slot;
		}

		/* Remove all existing user filters */
		ret = mpq_tspp_remove_all_user_filters(channel_id,
					tspp_source.source);
		if (ret < 0) {
			MPQ_DVB_ERR_PRINT(
				"%s: mpq_tspp_remove_all_user_filters(%d, %d) failed\n",
				__func__, channel_id, tspp_source.source);

			restore_user_filters = 1;
			remove_accept_all_filter = 1;

			goto add_channel_free_filter_slot;
		}

		/* Add HW filters to block NULL packets */
		ret = mpq_tspp_add_null_blocking_filters(channel_id,
					tspp_source.source);
		if (ret < 0) {
			MPQ_DVB_ERR_PRINT(
				"%s: mpq_tspp_add_null_blocking_filters(%d, %d) failed\n",
				__func__, channel_id, tspp_source.source);

			restore_user_filters = 1;
			remove_accept_all_filter = 1;

			goto add_channel_free_filter_slot;
		}

		/* Remove filters that accepts all packets, if necessary */
		if ((mpq_dmx_tspp_info.tsif[tsif].pass_all_flag == 0) &&
			(mpq_dmx_tspp_info.tsif[tsif].pass_nulls_flag == 0)) {

			ret = mpq_tspp_remove_accept_all_filter(channel_id,
						tspp_source.source);
			if (ret < 0) {
				MPQ_DVB_ERR_PRINT(
					"%s: mpq_tspp_remove_accept_all_filter(%d, %d) failed\n",
					__func__, channel_id,
					tspp_source.source);

				remove_null_blocking_filters = 1;
				restore_user_filters = 1;
				remove_accept_all_filter = 1;

				goto add_channel_free_filter_slot;
			}
		}
	} else {
		/* Already working in SW filtering mode */
		if (mpq_dmx_tspp_info.tsif[tsif].pass_all_flag ||
			mpq_dmx_tspp_info.tsif[tsif].pass_nulls_flag) {

			ret = mpq_tspp_add_accept_all_filter(channel_id,
						tspp_source.source);
			if (ret < 0) {
				MPQ_DVB_ERR_PRINT(
					"%s: mpq_tspp_add_accept_all_filter(%d, %d) failed\n",
					__func__, channel_id,
					tspp_source.source);

				goto add_channel_free_filter_slot;
			}
		}
	}

	(*channel_ref_count)++;
	mpq_dmx_tspp_info.tsif[tsif].current_filter_count++;

	MPQ_DVB_DBG_PRINT("%s: success, current_filter_count = %d\n",
		__func__, mpq_dmx_tspp_info.tsif[tsif].current_filter_count);

	goto out;

add_channel_free_filter_slot:
	/* restore internal database state */
	mpq_dmx_tspp_info.tsif[tsif].filters[slot].pid = -1;
	mpq_dmx_tspp_info.tsif[tsif].filters[slot].ref_count--;

	/* release HW index if we allocated one */
	if (tspp_filter.priority >= 0) {
		mpq_dmx_tspp_info.tsif[tsif].filters[slot].hw_index = -1;
		mpq_tspp_release_hw_filter_index(tsif, tspp_filter.priority);
	}

	/* restore HW filter table state if necessary */
	if (remove_null_blocking_filters)
		mpq_tspp_remove_null_blocking_filters(channel_id,
						tspp_source.source);

	if (restore_user_filters)
		mpq_tspp_add_all_user_filters(channel_id, tspp_source.source);

	if (remove_accept_all_filter)
		mpq_tspp_remove_accept_all_filter(channel_id,
						tspp_source.source);

	/* restore flags. we can only get here if we changed the flags. */
	if (feed->pid == TSPP_PASS_THROUGH_PID)
		mpq_dmx_tspp_info.tsif[tsif].pass_all_flag = 0;
	else if (feed->pid == TSPP_NULL_PACKETS_PID)
		mpq_dmx_tspp_info.tsif[tsif].pass_nulls_flag = 0;

add_channel_unregister_notif:
	if (*channel_ref_count == 0) {
		tspp_unregister_notification(0, channel_id);
		tspp_close_stream(0, channel_id);
	}
add_channel_close_ch:
	if (*channel_ref_count == 0)
		tspp_close_channel(0, channel_id);
add_channel_failed:
	if (*channel_ref_count == 0)
		if (allocation_mode == MPQ_DMX_TSPP_CONTIGUOUS_PHYS_ALLOC) {
			vfree(mpq_dmx_tspp_info.tsif[tsif].aggregate_ids);
			mpq_dmx_tspp_info.tsif[tsif].aggregate_ids = NULL;
			mpq_dmx_channel_mem_free(tsif);
		}

out:
	mutex_unlock(&mpq_dmx_tspp_info.tsif[tsif].mutex);
	atomic_dec(&mpq_dmx_tspp_info.tsif[tsif].control_op);
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
	int ret = 0;
	int channel_id;
	int slot;
	atomic_t *data_cnt;
	int *channel_ref_count;
	enum tspp_source tspp_source;
	struct tspp_filter tspp_filter;
	struct mpq_demux *mpq_demux = feed->demux->priv;
	int restore_null_blocking_filters = 0;
	int remove_accept_all_filter = 0;
	int remove_user_filters = 0;
	int accept_all_filter_existed = 0;

	MPQ_DVB_DBG_PRINT("%s: executed, PID = %d\n", __func__, feed->pid);

	/* determine the TSIF we are reading from */
	if (mpq_demux->source == DMX_SOURCE_FRONT0) {
		tsif = 0;
		tspp_source = TSPP_SOURCE_TSIF0;
	} else if (mpq_demux->source == DMX_SOURCE_FRONT1) {
		tsif = 1;
		tspp_source = TSPP_SOURCE_TSIF1;
	} else {
		/* invalid source */
		MPQ_DVB_ERR_PRINT(
			"%s: invalid input source (%d)\n",
			__func__,
			mpq_demux->source);

		return -EINVAL;
	}

	atomic_inc(&mpq_dmx_tspp_info.tsif[tsif].control_op);
	if (mutex_lock_interruptible(&mpq_dmx_tspp_info.tsif[tsif].mutex)) {
		atomic_dec(&mpq_dmx_tspp_info.tsif[tsif].control_op);
		return -ERESTARTSYS;
	}

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
		goto out;
	}

	slot = mpq_tspp_get_filter_slot(tsif, feed->pid);

	if (slot < 0) {
		/* invalid feed provided as it has no filter allocated */
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_tspp_get_filter_slot failed (%d,%d)\n",
			__func__,
			feed->pid,
			tsif);

		ret = -EINVAL;
		goto out;
	}

	/* since filter was found, ref_count > 0 so it's ok to decrement it */
	mpq_dmx_tspp_info.tsif[tsif].filters[slot].ref_count--;

	if (mpq_dmx_tspp_info.tsif[tsif].filters[slot].ref_count) {
		/*
		 * there are still references to this pid, do not
		 * remove the filter yet
		 */
		goto out;
	}

	if (feed->pid == TSPP_PASS_THROUGH_PID)
		mpq_dmx_tspp_info.tsif[tsif].pass_all_flag = 0;
	else if (feed->pid == TSPP_NULL_PACKETS_PID)
		mpq_dmx_tspp_info.tsif[tsif].pass_nulls_flag = 0;

	mpq_dmx_tspp_info.tsif[tsif].filters[slot].pid = -1;

	if (mpq_dmx_tspp_info.tsif[tsif].current_filter_count <=
					TSPP_MAX_HW_PID_FILTER_NUM) {
		/* staying in HW filtering mode */
		tspp_filter.priority =
			mpq_dmx_tspp_info.tsif[tsif].filters[slot].hw_index;
		ret = tspp_remove_filter(0, channel_id, &tspp_filter);
		if (ret < 0) {
			MPQ_DVB_ERR_PRINT(
				"%s: tspp_remove_filter failed (%d,%d)\n",
				__func__,
				channel_id,
				tspp_filter.priority);

			goto remove_channel_failed_restore_count;
		}
		mpq_tspp_release_hw_filter_index(tsif, tspp_filter.priority);
		mpq_dmx_tspp_info.tsif[tsif].filters[slot].hw_index = -1;

		MPQ_DVB_DBG_PRINT(
			"%s: HW filtering mode: Removed TSPP HW filter, PID = %d, index = %d\n",
			__func__, feed->pid, tspp_filter.priority);
	} else  if (mpq_dmx_tspp_info.tsif[tsif].current_filter_count ==
					(TSPP_MAX_HW_PID_FILTER_NUM + 1)) {
		/* Crossing the threshold - from SW to HW filtering mode */

		accept_all_filter_existed =
		   mpq_dmx_tspp_info.tsif[tsif].accept_all_filter_exists_flag;

		/* Add a temporary filter to accept all packets */
		ret = mpq_tspp_add_accept_all_filter(channel_id,
					tspp_source);
		if (ret < 0) {
			MPQ_DVB_ERR_PRINT(
				"%s: mpq_tspp_add_accept_all_filter(%d, %d) failed\n",
				__func__, channel_id, tspp_source);

			goto remove_channel_failed_restore_count;
		}

		ret = mpq_tspp_remove_null_blocking_filters(channel_id,
					tspp_source);
		if (ret < 0) {
			MPQ_DVB_ERR_PRINT(
				"%s: mpq_tspp_remove_null_blocking_filters(%d, %d) failed\n",
				__func__, channel_id, tspp_source);

			restore_null_blocking_filters = 1;
			if (!accept_all_filter_existed)
				remove_accept_all_filter = 1;

			goto remove_channel_failed_restore_count;
		}

		ret = mpq_tspp_add_all_user_filters(channel_id,
					tspp_source);
		if (ret < 0) {
			MPQ_DVB_ERR_PRINT(
				"%s: mpq_tspp_add_all_user_filters(%d, %d) failed\n",
				__func__, channel_id, tspp_source);

			remove_user_filters = 1;
			restore_null_blocking_filters = 1;
			if (!accept_all_filter_existed)
				remove_accept_all_filter = 1;

			goto remove_channel_failed_restore_count;
		}

		ret = mpq_tspp_remove_accept_all_filter(channel_id,
					tspp_source);
		if (ret < 0) {
			MPQ_DVB_ERR_PRINT(
				"%s: mpq_tspp_remove_accept_all_filter(%d, %d) failed\n",
				__func__, channel_id, tspp_source);

			remove_user_filters = 1;
			restore_null_blocking_filters = 1;
			if (!accept_all_filter_existed)
				remove_accept_all_filter = 1;

			goto remove_channel_failed_restore_count;
		}
	} else {
		/* staying in SW filtering mode */
		if ((mpq_dmx_tspp_info.tsif[tsif].pass_all_flag == 0) &&
			(mpq_dmx_tspp_info.tsif[tsif].pass_nulls_flag == 0)) {

			ret = mpq_tspp_remove_accept_all_filter(channel_id,
						tspp_source);
			if (ret < 0) {
				MPQ_DVB_ERR_PRINT(
					"%s: mpq_tspp_remove_accept_all_filter(%d, %d) failed\n",
					__func__, channel_id,
					tspp_source);

				goto remove_channel_failed_restore_count;
			}
		}
	}

	mpq_dmx_tspp_info.tsif[tsif].current_filter_count--;
	(*channel_ref_count)--;

	MPQ_DVB_DBG_PRINT("%s: success, current_filter_count = %d\n",
		__func__, mpq_dmx_tspp_info.tsif[tsif].current_filter_count);

	if (*channel_ref_count == 0) {
		/* channel is not used any more, release it */
		tspp_unregister_notification(0, channel_id);
		tspp_close_stream(0, channel_id);
		tspp_close_channel(0, channel_id);
		atomic_set(data_cnt, 0);

		if (allocation_mode == MPQ_DMX_TSPP_CONTIGUOUS_PHYS_ALLOC) {
			vfree(mpq_dmx_tspp_info.tsif[tsif].aggregate_ids);
			mpq_dmx_tspp_info.tsif[tsif].aggregate_ids = NULL;
			mpq_dmx_channel_mem_free(tsif);
		}
	}

	goto out;

remove_channel_failed_restore_count:
	/* restore internal database state */
	mpq_dmx_tspp_info.tsif[tsif].filters[slot].pid = feed->pid;
	mpq_dmx_tspp_info.tsif[tsif].filters[slot].ref_count++;

	if (remove_user_filters)
		mpq_tspp_remove_all_user_filters(channel_id, tspp_source);

	if (restore_null_blocking_filters)
		mpq_tspp_add_null_blocking_filters(channel_id, tspp_source);

	if (remove_accept_all_filter)
		mpq_tspp_remove_accept_all_filter(channel_id, tspp_source);

	/* restore flags. we can only get here if we changed the flags. */
	if (feed->pid == TSPP_PASS_THROUGH_PID)
		mpq_dmx_tspp_info.tsif[tsif].pass_all_flag = 1;
	else if (feed->pid == TSPP_NULL_PACKETS_PID)
		mpq_dmx_tspp_info.tsif[tsif].pass_nulls_flag = 1;

out:
	mutex_unlock(&mpq_dmx_tspp_info.tsif[tsif].mutex);
	atomic_dec(&mpq_dmx_tspp_info.tsif[tsif].control_op);
	return ret;
}

static int mpq_tspp_dmx_start_filtering(struct dvb_demux_feed *feed)
{
	int ret;
	struct mpq_demux *mpq_demux = feed->demux->priv;

	MPQ_DVB_DBG_PRINT(
		"%s(pid=%d) executed\n",
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

	ret = mpq_dmx_init_mpq_feed(feed);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_dmx_init_mpq_feed failed(%d)\n",
			__func__,
			ret);
		if (mpq_demux->source < DMX_SOURCE_DVR0)
			mpq_tspp_dmx_remove_channel(feed);

		return ret;
	}

	return 0;
}

static int mpq_tspp_dmx_stop_filtering(struct dvb_demux_feed *feed)
{
	int ret = 0;
	struct mpq_demux *mpq_demux = feed->demux->priv;

	MPQ_DVB_DBG_PRINT("%s(%d) executed\n", __func__, feed->pid);

	mpq_dmx_terminate_feed(feed);

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

	if (dvb_dmx_is_video_feed(feed))
		return mpq_dmx_process_video_packet(feed, buf);

	if (dvb_dmx_is_pcr_feed(feed))
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

	caps->caps = DMX_CAP_PULL_MODE | DMX_CAP_VIDEO_DECODER_DATA |
		DMX_CAP_TS_INSERTION | DMX_CAP_VIDEO_INDEXING |
		DMX_CAP_AUTO_BUFFER_FLUSH;
	caps->recording_max_video_pids_indexed = 0;
	caps->num_decoders = MPQ_ADAPTER_MAX_NUM_OF_INTERFACES;
	caps->num_demux_devices = CONFIG_DVB_MPQ_NUM_DMX_DEVICES;
	caps->num_pid_filters = TSPP_MAX_PID_FILTER_NUM;
	caps->num_section_filters = dvb_demux->filternum;
	caps->num_section_filters_per_pid = dvb_demux->filternum;
	caps->section_filter_length = DMX_FILTER_SIZE;
	caps->num_demod_inputs = TSIF_COUNT;
	caps->num_memory_inputs = CONFIG_DVB_MPQ_NUM_DMX_DEVICES;
	caps->max_bitrate = 192;
	caps->demod_input_max_bitrate = 96;
	caps->memory_input_max_bitrate = 96;
	caps->num_cipher_ops = 1;

	/* TSIF reports 3 bytes STC at unit of 27MHz/256 */
	caps->max_stc = (u64)0xFFFFFF * 256;

	/* Buffer requirements */
	caps->section.flags =
		DMX_BUFFER_EXTERNAL_SUPPORT	|
		DMX_BUFFER_INTERNAL_SUPPORT |
		DMX_BUFFER_CACHED;
	caps->section.max_buffer_num = 1;
	caps->section.max_size = 0xFFFFFFFF;
	caps->section.size_alignment = 0;
	caps->pes.flags =
		DMX_BUFFER_EXTERNAL_SUPPORT	|
		DMX_BUFFER_INTERNAL_SUPPORT |
		DMX_BUFFER_CACHED;
	caps->pes.max_buffer_num = 1;
	caps->pes.max_size = 0xFFFFFFFF;
	caps->pes.size_alignment = 0;
	caps->recording_188_tsp.flags =
		DMX_BUFFER_EXTERNAL_SUPPORT	|
		DMX_BUFFER_INTERNAL_SUPPORT |
		DMX_BUFFER_CACHED;
	caps->recording_188_tsp.max_buffer_num = 1;
	caps->recording_188_tsp.max_size = 0xFFFFFFFF;
	caps->recording_188_tsp.size_alignment = 0;
	caps->recording_192_tsp.flags =
		DMX_BUFFER_EXTERNAL_SUPPORT	|
		DMX_BUFFER_INTERNAL_SUPPORT |
		DMX_BUFFER_CACHED;
	caps->recording_192_tsp.max_buffer_num = 1;
	caps->recording_192_tsp.max_size = 0xFFFFFFFF;
	caps->recording_192_tsp.size_alignment = 0;
	caps->playback_188_tsp.flags =
		DMX_BUFFER_EXTERNAL_SUPPORT	|
		DMX_BUFFER_INTERNAL_SUPPORT |
		DMX_BUFFER_CACHED;
	caps->playback_188_tsp.max_buffer_num = 1;
	caps->playback_188_tsp.max_size = 0xFFFFFFFF;
	caps->playback_188_tsp.size_alignment = 188;
	caps->playback_192_tsp.flags =
		DMX_BUFFER_EXTERNAL_SUPPORT	|
		DMX_BUFFER_INTERNAL_SUPPORT |
		DMX_BUFFER_CACHED;
	caps->playback_192_tsp.max_buffer_num = 1;
	caps->playback_192_tsp.max_size = 0xFFFFFFFF;
	caps->playback_192_tsp.size_alignment = 192;
	caps->decoder.flags =
		DMX_BUFFER_SECURED_IF_DECRYPTED	|
		DMX_BUFFER_EXTERNAL_SUPPORT	|
		DMX_BUFFER_INTERNAL_SUPPORT	|
		DMX_BUFFER_LINEAR_GROUP_SUPPORT |
		DMX_BUFFER_CACHED;
	caps->decoder.max_buffer_num = DMX_MAX_DECODER_BUFFER_NUM;
	caps->decoder.max_size = 0xFFFFFFFF;
	caps->decoder.size_alignment = SZ_4K;

	return 0;
}


/**
 * Reads TSIF STC from TSPP
 *
 * @demux: demux device
 * @num: STC number. 0 for TSIF0 and 1 for TSIF1.
 * @stc: STC value
 * @base: divisor to get 90KHz value
 *
 * Return     error code
 */
static int mpq_tspp_dmx_get_stc(struct dmx_demux *demux, unsigned int num,
		u64 *stc, unsigned int *base)
{
	enum tspp_source source;
	u32 tcr_counter;
	u64 avtimer_stc = 0;
	int tts_source = 0;

	if (!demux || !stc || !base)
		return -EINVAL;

	if (num == 0)
		source = TSPP_SOURCE_TSIF0;
	else if (num == 1)
		source = TSPP_SOURCE_TSIF1;
	else
		return -EINVAL;

	if (tspp_get_tts_source(0, &tts_source) < 0)
		tts_source = TSIF_TTS_TCR;

	if (tts_source != TSIF_TTS_LPASS_TIMER) {
		tspp_get_ref_clk_counter(0, source, &tcr_counter);
		*stc = ((u64)tcr_counter) * 256; /* conversion to 27MHz */
		*base = 300; /* divisor to get 90KHz clock from stc value */
	} else {
		if (tspp_get_lpass_time_counter(0, source, &avtimer_stc) < 0)
			return -EINVAL;
		*stc = avtimer_stc;
	}
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
	mpq_demux->demux.decoder_fullness_init = mpq_dmx_decoder_fullness_init;
	mpq_demux->demux.decoder_fullness_wait = mpq_dmx_decoder_fullness_wait;
	mpq_demux->demux.decoder_fullness_abort =
		mpq_dmx_decoder_fullness_abort;
	mpq_demux->demux.decoder_buffer_status = mpq_dmx_decoder_buffer_status;
	mpq_demux->demux.reuse_decoder_buffer = mpq_dmx_reuse_decoder_buffer;
	mpq_demux->demux.set_cipher_op = mpq_dmx_set_cipher_ops;
	mpq_demux->demux.oob_command = mpq_dmx_oob_command;
	mpq_demux->demux.convert_ts = mpq_dmx_convert_tts;
	mpq_demux->demux.flush_decoder_buffer = NULL;

	/* Initialize dvb_demux object */
	result = dvb_dmx_init(&mpq_demux->demux);
	if (result < 0) {
		MPQ_DVB_ERR_PRINT("%s: dvb_dmx_init failed\n", __func__);
		goto init_failed;
	}

	/* Now initailize the dmx-dev object */
	mpq_demux->dmxdev.filternum = MPQ_MAX_DMX_FILES;
	mpq_demux->dmxdev.demux = &mpq_demux->demux.dmx;
	mpq_demux->dmxdev.capabilities = DMXDEV_CAP_DUPLEX;

	mpq_demux->dmxdev.demux->set_source = mpq_dmx_set_source;
	mpq_demux->dmxdev.demux->get_stc = mpq_tspp_dmx_get_stc;
	mpq_demux->dmxdev.demux->get_caps = mpq_tspp_dmx_get_caps;
	mpq_demux->dmxdev.demux->map_buffer = mpq_dmx_map_buffer;
	mpq_demux->dmxdev.demux->unmap_buffer = mpq_dmx_unmap_buffer;
	mpq_demux->dmxdev.demux->write = mpq_dmx_write;
	result = dvb_dmxdev_init(&mpq_demux->dmxdev, mpq_adapter);
	if (result < 0) {
		MPQ_DVB_ERR_PRINT("%s: dvb_dmxdev_init failed (errno=%d)\n",
						  __func__,
						  result);
		goto init_failed_dmx_release;
	}

	/* Extend dvb-demux debugfs with TSPP statistics. */
	mpq_dmx_init_debugfs_entries(mpq_demux);

	/* Get the TSIF TTS info */
	if (tspp_get_tts_source(0, &mpq_demux->ts_packet_timestamp_source) < 0)
		mpq_demux->ts_packet_timestamp_source = TSIF_TTS_TCR;

	return 0;

init_failed_dmx_release:
	dvb_dmx_release(&mpq_demux->demux);
init_failed:
	return result;
}

static int mpq_dmx_tspp_plugin_probe(struct platform_device *pdev)
{
	int i;
	int j;
	int ret;

	MPQ_DVB_DBG_PRINT("%s executed\n", __func__);

	for (i = 0; i < TSIF_COUNT; i++) {
		mpq_dmx_tspp_info.tsif[i].aggregate_ids = NULL;
		mpq_dmx_tspp_info.tsif[i].channel_ref = 0;
		mpq_dmx_tspp_info.tsif[i].buff_index = 0;
		mpq_dmx_tspp_info.tsif[i].ch_mem_heap_handle = NULL;
		mpq_dmx_tspp_info.tsif[i].ch_mem_heap_virt_base = NULL;
		mpq_dmx_tspp_info.tsif[i].ch_mem_heap_phys_base = 0;
		atomic_set(&mpq_dmx_tspp_info.tsif[i].data_cnt, 0);
		atomic_set(&mpq_dmx_tspp_info.tsif[i].control_op, 0);

		for (j = 0; j < TSPP_MAX_PID_FILTER_NUM; j++) {
			mpq_dmx_tspp_info.tsif[i].filters[j].pid = -1;
			mpq_dmx_tspp_info.tsif[i].filters[j].ref_count = 0;
			mpq_dmx_tspp_info.tsif[i].filters[j].hw_index = -1;
		}

		for (j = 0; j < TSPP_MAX_HW_PID_FILTER_NUM; j++)
			mpq_dmx_tspp_info.tsif[i].hw_indexes[j] = 0;

		mpq_dmx_tspp_info.tsif[i].current_filter_count = 0;
		mpq_dmx_tspp_info.tsif[i].pass_nulls_flag = 0;
		mpq_dmx_tspp_info.tsif[i].pass_all_flag = 0;
		mpq_dmx_tspp_info.tsif[i].accept_all_filter_exists_flag = 0;

		snprintf(mpq_dmx_tspp_info.tsif[i].name,
				TSIF_NAME_LENGTH,
				"dmx_tsif%d",
				i);

		init_waitqueue_head(&mpq_dmx_tspp_info.tsif[i].wait_queue);
		mpq_dmx_tspp_info.tsif[i].thread =
			kthread_run(
				mpq_dmx_tspp_thread, (void *)(uintptr_t)i,
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

	ret = mpq_dmx_plugin_init(mpq_tspp_dmx_init, pdev);

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

static int mpq_dmx_tspp_plugin_remove(struct platform_device *pdev)
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
				MPQ_DMX_TSPP_CONTIGUOUS_PHYS_ALLOC) {
				vfree(mpq_dmx_tspp_info.tsif[i].aggregate_ids);
				mpq_dmx_tspp_info.tsif[i].aggregate_ids = NULL;
				mpq_dmx_channel_mem_free(i);
			}
		}

		mutex_unlock(&mpq_dmx_tspp_info.tsif[i].mutex);
		kthread_stop(mpq_dmx_tspp_info.tsif[i].thread);
		mutex_destroy(&mpq_dmx_tspp_info.tsif[i].mutex);
	}

	mpq_dmx_plugin_exit();
	return 0;
}

static const struct of_device_id msm_match_table[] = {
	{.compatible = "qcom,demux"},
	{}
};

static struct platform_driver mpq_dmx_tspp_plugin_driver = {
	.probe          = mpq_dmx_tspp_plugin_probe,
	.remove         = mpq_dmx_tspp_plugin_remove,
	.driver         = {
		.name   = "demux",
		.of_match_table = msm_match_table,
	},
};


static int __init mpq_dmx_tspp_plugin_init(void)
{
	int rc;

	/* register the driver, and check hardware */
	rc = platform_driver_register(&mpq_dmx_tspp_plugin_driver);
	if (rc)
		pr_err("%s: platform_driver_register failed: %d\n",
			__func__, rc);

	return rc;
}

static void __exit mpq_dmx_tspp_plugin_exit(void)
{
	/* delete low level driver */
	platform_driver_unregister(&mpq_dmx_tspp_plugin_driver);
}


module_init(mpq_dmx_tspp_plugin_init);
module_exit(mpq_dmx_tspp_plugin_exit);

MODULE_DESCRIPTION("Qualcomm Technologies Inc. demux TSPP version 1 HW Plugin");
MODULE_LICENSE("GPL v2");
