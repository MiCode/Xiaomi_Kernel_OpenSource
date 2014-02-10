/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/completion.h>
#include "mpq_dmx_plugin_tspp_v2.h"
#include "mpq_dvb_debug.h"
#include "mpq_dmx_plugin_common.h"

#define TSPP2_DEVICE_ID			0
#define TSPP2_MAX_REC_PATTERN_INDEXING	1

/* Below are TSIF parameters only the TSPPv2 plugin uses */
static int data_inverse;
module_param(data_inverse, int, S_IRUGO | S_IWUSR);

static int sync_inverse;
module_param(sync_inverse, int, S_IRUGO | S_IWUSR);

static int enable_inverse;
module_param(enable_inverse, int, S_IRUGO | S_IWUSR);

static int tspp2_buff_heap = ION_IOMMU_HEAP_ID;
module_param(tspp2_buff_heap, int, S_IRUGO | S_IWUSR);

/**
 * mpq_dmx_tspp2_info - TSPPv2 demux singleton information
 * @filters:			TSPP2 filters info
 * @pipes:			TSPP2 pipes info
 * @source:			TSPP2 demuxing sources info.
 *				Indexs 0 & 1 are reserved for TSIF0 & TSIF1,
 *				the rest for DVR devices accordingly.
 *				(See enum mpq_dmx_tspp2_source)
 * @ts_insertion_source:	Shared singleton special source for TS insertion
 * @index_tables:		TSPP2 indexing tables info
 * @polling_timer:		Section/Recording pipe polling timer
 * @mutex:			Mutex protecting access to this structure
 * @user_count:			tspp2 device reference count
 * @debugfs_dmx_dir:		'tspp2_demux' debug-fs root directory object
 * @debugfs_pipes_file:		Pipes information
 * @debugfs_filters_file:	Filters information
 * @debugfs_sources_file:	Sources information
 * @debugfs_index_tables_file:	Indexing tables information
 */
static struct
{
	struct mpq_dmx_tspp2_filter filters[TSPP2_DMX_MAX_PID_FILTER_NUM];
	struct pipe_info pipes[TSPP2_NUM_PIPES];
	struct source_info source[TSPP2_DMX_SOURCE_COUNT];
	struct source_info ts_insertion_source;
	struct mpq_tspp2_index_table index_tables[TSPP2_NUM_INDEXING_TABLES];
	struct {
		struct timer_list handle;
		u16 ref_count; /* number of pipes using the timer */
	} polling_timer;
	struct mutex mutex;
	u32 user_count;

	/* debug-fs */
	struct dentry *debugfs_dmx_dir;
	struct dentry *debugfs_pipes_file;
	struct dentry *debugfs_filters_file;
	struct dentry *debugfs_sources_file;
	struct dentry *debugfs_index_tables_file;
} mpq_dmx_tspp2_info;

/**
 * Initialized a pipe work queue
 *
 * @queue: pipe work queue to initialize
 */
static void pipe_work_queue_init(struct pipe_work_queue *queue)
{
	int i;

	spin_lock_init(&queue->lock);
	INIT_LIST_HEAD(&queue->work_list);
	INIT_LIST_HEAD(&queue->free_list);

	for (i = 0; i < TSPP2_DMX_PIPE_WORK_POOL_SIZE; i++)
		list_add_tail(&queue->work_pool[i].next, &queue->free_list);
}

/**
 * Allocates a pipe work element
 *
 * @queue: pipe work queue
 *
 * Return pipe_work pointer on success, NULL otherwise.
 */
static struct pipe_work *pipe_work_queue_allocate(struct pipe_work_queue *queue)
{
	struct pipe_work *work;
	unsigned long flags;

	if (queue == NULL)
		return NULL;

	spin_lock_irqsave(&queue->lock, flags);

	if (list_empty(&queue->free_list)) {
		spin_unlock_irqrestore(&queue->lock, flags);
		return NULL;
	}

	work = list_first_entry(&queue->free_list, struct pipe_work, next);
	list_del(&work->next);

	spin_unlock_irqrestore(&queue->lock, flags);

	return work;
}

/**
 * Queue up a pipe work element on the queue
 *
 * @queue: pipe work queue
 * @work: pipe work element for queuing
 */
static void pipe_work_queue_push(struct pipe_work_queue *queue,
	struct pipe_work *work)
{
	unsigned long flags;

	if (queue == NULL || work == NULL)
		return;

	spin_lock_irqsave(&queue->lock, flags);
	list_add_tail(&work->next, &queue->work_list);
	spin_unlock_irqrestore(&queue->lock, flags);
}

/**
 * Pops out the first pipe work element from the queue
 *
 * @queue: pipe work queue
 *
 * Return pipe_work pointer on success, NULL otherwise.
 */
static struct pipe_work *pipe_work_queue_pop(struct pipe_work_queue *queue)
{
	struct pipe_work *work = NULL;
	unsigned long flags;

	if (queue == NULL)
		return NULL;

	spin_lock_irqsave(&queue->lock, flags);
	if (list_empty(&queue->work_list)) {
		spin_unlock_irqrestore(&queue->lock, flags);
		return NULL;
	}

	work = list_first_entry(&queue->work_list, struct pipe_work, next);
	list_del(&work->next);

	spin_unlock_irqrestore(&queue->lock, flags);
	return work;
}

/**
 * Return a pipe work element to the queue pool
 *
 * @queue: pipe work queue
 * @work: pipe work element
 */
static void pipe_work_queue_release(struct pipe_work_queue *queue,
	struct pipe_work *work)
{
	unsigned long flags;

	if (queue == NULL || work == NULL)
		return;

	spin_lock_irqsave(&queue->lock, flags);
	list_add_tail(&work->next, &queue->free_list);
	spin_unlock_irqrestore(&queue->lock, flags);
}

/**
 * Test if a pipe work queue is empty
 *
 * @queue: pipe work queue
 *
 * Return 1 if pipe work queue is empty, 0 otherwise
 */
static int pipe_work_queue_empty(struct pipe_work_queue *queue)
{
	int empty;
	unsigned long flags;

	spin_lock_irqsave(&queue->lock, flags);
	empty = list_empty(&queue->work_list);
	spin_unlock_irqrestore(&queue->lock, flags);

	return empty;
}

/**
 * Remove all pending pipe work elements for the specified pipe from the
 * work queue, and return them to the queue pool.
 *
 * @queue: pipe work queue
 * @pipe_info: pipe object
 */
static void pipe_work_queue_cancel_work(struct pipe_work_queue *queue,
	struct pipe_info *pipe_info)
{
	struct pipe_work *pos, *tmp;
	unsigned long flags;

	if (queue == NULL || pipe_info == NULL)
		return;

	spin_lock_irqsave(&queue->lock, flags);

	if (list_empty(&queue->work_list)) {
		spin_unlock_irqrestore(&queue->lock, flags);
		return;
	}

	list_for_each_entry_safe(pos, tmp, &queue->work_list, next) {
		if (pos->pipe_info == pipe_info) {
			list_del(&pos->next);
			list_add_tail(&pos->next, &queue->free_list);
		}
	}

	spin_unlock_irqrestore(&queue->lock, flags);
}

/*
 * mpq_dmx_tspp2_get_stc() - extract TSPPv2 STC value from buffer
 *
 * @buf:	7 bytes buffer containing the STC value
 * @size:	Number STC data bytes in buffer. Can be either 7 or 4.
 *
 * TSPPv2 STC value in buffer are in big endian format (msb in low address)
 */
static inline u64 mpq_dmx_tspp2_get_stc(const u8 *buf, size_t size)
{
	u64 stc = 0;

	if (!buf)
		return 0;

	if (size == 7) {
		stc += ((u64)buf[0]) << 48;
		stc += ((u64)buf[1]) << 40;
		stc += ((u64)buf[2]) << 32;
		stc += ((u64)buf[3]) << 24;
		stc += ((u64)buf[4]) << 16;
		stc += ((u64)buf[5]) << 8;
		stc += buf[6];
	} else if (size == 4) {
		stc += ((u64)buf[0]) << 24;
		stc += ((u64)buf[1]) << 16;
		stc += ((u64)buf[2]) << 8;
		stc += buf[3];
	}

	return stc;
}

/**
 * mpq_dmx_tspp2_convert_ts() - Convert a 4 byte timestamp recorded by TSPPv2
 * to 27Mhz format.
 *
 * @feed:		dvb demux feed object
 * @timestamp:		4 byte timestamp buffer
 * @timestampIn27Mhz:	convertion result
 */
static void mpq_dmx_tspp2_convert_ts(struct dvb_demux_feed *feed,
			const u8 timestamp[TIMESTAMP_LEN],
			u64 *timestampIn27Mhz)
{
	if (unlikely(!timestampIn27Mhz))
		return;

	*timestampIn27Mhz = mpq_dmx_tspp2_get_stc(timestamp, 4);
}

/**
 * mpq_dmx_tspp2_reuse_decoder_buffer() - release decoder buffer for reuse
 *
 * @feed:	dvb demux object
 * @cookie:	cookie identifying buffer for reuse
 *
 * Return error status
 */
static int mpq_dmx_tspp2_reuse_decoder_buffer(struct dvb_demux_feed *feed,
			int cookie)
{
	struct mpq_demux *mpq_demux = feed->demux->priv;

	MPQ_DVB_DBG_PRINT("%s: cookie=%d\n", __func__, cookie);

	if (cookie < 0) {
		MPQ_DVB_ERR_PRINT("%s: invalid cookie parameter\n", __func__);
		return -EINVAL;
	}

	if (dvb_dmx_is_video_feed(feed)) {
		struct mpq_video_feed_info *feed_data;
		struct mpq_feed *mpq_feed;
		struct mpq_streambuffer *stream_buffer;
		int ret;

		if (mutex_lock_interruptible(&mpq_dmx_tspp2_info.mutex))
			return -ERESTARTSYS;

		mpq_feed = feed->priv;
		feed_data = &mpq_feed->video_info;

		stream_buffer = feed_data->video_buffer;

		if (stream_buffer == NULL) {
			MPQ_DVB_ERR_PRINT(
				"%s: invalid feed, feed_data->video_buffer is NULL\n",
				__func__);
			mutex_unlock(&mpq_demux->mutex);
			return -EINVAL;
		}

		ret = mpq_streambuffer_pkt_dispose(stream_buffer, cookie, 1);
		mutex_unlock(&mpq_dmx_tspp2_info.mutex);

		return ret;
	}

	/* else */
	MPQ_DVB_ERR_PRINT("%s: Invalid feed type %d\n",
			__func__, feed->pes_type);

	return -EINVAL;
}

static int mpq_dmx_tspp2_init_idx_table(enum dmx_video_codec codec,
	const struct dvb_dmx_video_patterns *patterns[], int num_patterns)
{
	struct mpq_tspp2_index_table *table;
	u8 table_id = codec;
	u32 values[TSPP2_NUM_INDEXING_PATTERNS];
	u32 masks[TSPP2_NUM_INDEXING_PATTERNS];
	int ret;
	int i;
	int j;

	table = &mpq_dmx_tspp2_info.index_tables[codec];

	ret = tspp2_indexing_patterns_clear(TSPP2_DEVICE_ID, table_id);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: tspp2_indexing_patterns_clear(table_id=%u) failed, ret=%d\n",
			__func__, table_id, ret);
		return ret;
	}
	table->num_patterns = 0;

	ret = tspp2_indexing_prefix_set(TSPP2_DEVICE_ID, table_id,
		INDEX_TABLE_PREFIX_VALUE, INDEX_TABLE_PREFIX_MASK);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: tspp2_indexing_prefix_set(table_id=%u) failed, ret=%d\n",
			__func__, table_id, ret);
		return ret;
	}

	MPQ_DVB_DBG_PRINT(
		"%s: Initialize new index table #%d, prefix=0x%X, mask=0x%X\n",
		__func__, table_id, INDEX_TABLE_PREFIX_VALUE,
		INDEX_TABLE_PREFIX_MASK);

	for (i = 0; i < num_patterns; i++) {
		values[i] = 0;
		masks[i] = 0;

		for (j = 0; j < patterns[i]->size - INDEX_TABLE_PREFIX_LENGTH;
			j++) {
			values[i] |= patterns[i]->
				pattern[INDEX_TABLE_PREFIX_LENGTH + j] <<
				(sizeof(u32) - 1 - j) * 8;
			masks[i] |= patterns[i]->
				mask[INDEX_TABLE_PREFIX_LENGTH + j] <<
				(sizeof(u32) - 1 - j) * 8;
		}

		table->patterns[i].value = values[i];
		table->patterns[i].mask = masks[i];
		table->patterns[i].type = patterns[i]->type;
		MPQ_DVB_DBG_PRINT(
			"%s: Index pattern: value=0x%X, mask=0x%X, type=0x%llX\n",
			__func__, values[i], masks[i], patterns[i]->type);
		table->num_patterns++;
	}

	ret = tspp2_indexing_patterns_add(TSPP2_DEVICE_ID, table_id, values,
		masks, table->num_patterns);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: tspp2_indexing_patterns_add (%u patterns) failed, ret=%d\n",
			__func__, table->num_patterns, ret);
		table->num_patterns = 0;
		return ret;
	}

	return 0;
}

static int mpq_dmx_tspp2_init_codec_tables(void)
{
	const struct dvb_dmx_video_patterns
		*patterns[DVB_DMX_MAX_SEARCH_PATTERN_NUM];
	int ret;

	patterns[0] = dvb_dmx_get_pattern(DMX_IDX_MPEG_SEQ_HEADER);
	patterns[1] = dvb_dmx_get_pattern(DMX_IDX_MPEG_GOP);
	patterns[2] = dvb_dmx_get_pattern(DMX_IDX_MPEG_I_FRAME_START);
	patterns[3] = dvb_dmx_get_pattern(DMX_IDX_MPEG_P_FRAME_START);
	patterns[4] = dvb_dmx_get_pattern(DMX_IDX_MPEG_B_FRAME_START);
	ret = mpq_dmx_tspp2_init_idx_table(DMX_VIDEO_CODEC_MPEG2, patterns, 5);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: Failed to init MPEG2 codec indexing table, ret=%d\n",
			__func__, ret);
		return ret;
	}

	patterns[0] = dvb_dmx_get_pattern(DMX_IDX_H264_SPS);
	patterns[1] = dvb_dmx_get_pattern(DMX_IDX_H264_PPS);
	patterns[2] = dvb_dmx_get_pattern(DMX_IDX_H264_IDR_START);
	patterns[3] = dvb_dmx_get_pattern(DMX_IDX_H264_NON_IDR_START);
	ret = mpq_dmx_tspp2_init_idx_table(DMX_VIDEO_CODEC_H264, patterns, 4);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: Failed to init H264 codec indexing table, ret=%d\n",
			__func__, ret);
		return ret;
	}

	patterns[0] = dvb_dmx_get_pattern(DMX_IDX_VC1_SEQ_HEADER);
	patterns[1] = dvb_dmx_get_pattern(DMX_IDX_VC1_ENTRY_POINT);
	patterns[2] = dvb_dmx_get_pattern(DMX_IDX_VC1_FRAME_START);
	ret = mpq_dmx_tspp2_init_idx_table(DMX_VIDEO_CODEC_VC1, patterns, 3);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: Failed to init VC1 codec indexing table, ret=%d\n",
			__func__, ret);
		return ret;
	}

	return 0;
}

/**
 * Set TSPPv2 global configuration parameters for the specified device id
 *
 * @id: TSPP2 device id
 *
 * Return 0 if successful, error code otherwise
 */
static int mpq_dmx_tspp2_set_global_config(int id)
{
	struct tspp2_config cfg;
	int ret;

	cfg.min_pcr_interval = 0;
	cfg.pcr_on_discontinuity = 1;
	cfg.stc_byte_offset = 0; /* Use 4 lsb bytes out of the 7 -> stc @27MHz*/
	ret = tspp2_config_set(id, &cfg);
	if (ret) {
		MPQ_DVB_ERR_PRINT("%s: tspp2_config_set(%d) failed. ret=%d\n",
			__func__, id, ret);
		return ret;
	}

	MPQ_DVB_DBG_PRINT(
		"%s: Global config(%d): min_pcr=%d, pcr_on_disc=%d, stc_offset=%d\n",
		__func__, id, cfg.min_pcr_interval,
		cfg.pcr_on_discontinuity, cfg.stc_byte_offset);

	ret = mpq_dmx_tspp2_init_codec_tables();
	if (ret)
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_dmx_tspp2_init_codec_tables failed, ret=%d\n",
			__func__, ret);

	return ret;
}

/**
 * mpq_dmx_tspp2_open_device() - opens a TSPP2 device instance and set global
 * configuration according to reference count.
 * mpq_dmx_tspp2_info.mutex should be acquired by caller.
 *
 * Return error status
 */
static int mpq_dmx_tspp2_open_device(void)
{
	int ret;

	if (mpq_dmx_tspp2_info.user_count == 0) {
		ret = tspp2_device_open(TSPP2_DEVICE_ID);
		if (ret) {
			MPQ_DVB_ERR_PRINT(
				"%s: tspp2_device_open failed, ret=%d\n",
				__func__, ret);
			return ret;
		}
		MPQ_DVB_DBG_PRINT("%s: tspp2_device_open OK\n", __func__);

		ret = mpq_dmx_tspp2_set_global_config(TSPP2_DEVICE_ID);
		if (ret) {
			MPQ_DVB_ERR_PRINT(
				"%s: mpq_dmx_tspp2_set_global_config failed, ret=%d\n",
				__func__, ret);
			tspp2_device_close(TSPP2_DEVICE_ID);
			return ret;
		}
		MPQ_DVB_DBG_PRINT("%s: mpq_dmx_tspp2_set_global_config OK\n",
			__func__);
	}

	mpq_dmx_tspp2_info.user_count++;
	MPQ_DVB_DBG_PRINT("%s: tspp2 device use count = %u\n",
		__func__, mpq_dmx_tspp2_info.user_count);

	return 0;
}

/**
 * mpq_dmx_tspp2_close_device() - closes an open instance of a TSPP2 device
 * according to reference count.
 * mpq_dmx_tspp2_info.mutex should be acquired by caller.
 *
 * Return error status
 */
static int mpq_dmx_tspp2_close_device(void)
{
	int ret;

	if (mpq_dmx_tspp2_info.user_count == 0) {
		MPQ_DVB_DBG_PRINT(
			"%s: tspp2 device is not open\n", __func__);
		return 0;
	}

	mpq_dmx_tspp2_info.user_count--;
	if (mpq_dmx_tspp2_info.user_count == 0) {
		ret = tspp2_device_close(TSPP2_DEVICE_ID);
		if (ret) {
			MPQ_DVB_ERR_PRINT(
				"%s: tspp2_device_close failed, ret=%d\n",
				__func__, ret);
			return ret;
		}
		MPQ_DVB_DBG_PRINT("%s: tspp2 device closed\n", __func__);
	}
	MPQ_DVB_DBG_PRINT("%s: tspp2 device use count = %u\n",
		__func__, mpq_dmx_tspp2_info.user_count);

	return 0;
}

/**
 * Returns the source-info object for the specified source type
 *
 * @source: the demux source of this pipe
 *
 * Return pointer to source_info object on success, NULL otherwise
 */
static struct source_info *mpq_dmx_get_source(dmx_source_t source)
{
	int index;

	if (source >= DMX_SOURCE_DVR0) {
		index = source - DMX_SOURCE_DVR0;
		if (index >= TSPP2_NUM_MEM_INPUTS)
			return NULL;

		index += BAM0_SOURCE;
	} else {
		index = source - DMX_SOURCE_FRONT0;
		if (index >= TSPP2_NUM_TSIF_INPUTS)
			return NULL;

		index += TSIF0_SOURCE;
	}

	return &mpq_dmx_tspp2_info.source[index];
}

/**
 * Return a free pipe
 *
 * Return pointer to pipe_info object on success, NULL otherwise
 */
static inline struct pipe_info *mpq_dmx_get_free_pipe(void)
{
	int i;

	for (i = 0; i < TSPP2_NUM_PIPES; i++)
		if (mpq_dmx_tspp2_info.pipes[i].ref_count == 0)
			return &mpq_dmx_tspp2_info.pipes[i];

	return NULL;
}

static inline u32 mpq_dmx_tspp2_addr_to_offset(struct pipe_info *pipe_info,
	u32 addr)
{
	return ((addr - pipe_info->buffer.iova) + 1) % pipe_info->buffer.size;
}

/**
 * mpq_dmx_tspp2_calc_pipe_data() - Calculate number of new data bytes in the
 * specified output pipe. Calculation is based on the pipe's last address
 * written to as TSPP2 reports relative to the previous last address value.
 *
 * @pipe_info:	Output pipe info
 * @last_addr:	Current last address reported for the pipe
 *
 * Return number of new data bytes
 */
static inline size_t mpq_dmx_tspp2_calc_pipe_data(struct pipe_info *pipe_info,
	u32 last_addr)
{
	size_t data_size;
	u32 prev_last_addr = pipe_info->tspp_last_addr;

	if (last_addr == 0 || last_addr == prev_last_addr) {
		MPQ_DVB_DBG_PRINT(
			"%s: buf_size=%u, last_addr=0x%x, prev_last_addr=0x%x -> data size=0\n",
			__func__, pipe_info->buffer.size, last_addr,
			pipe_info->tspp_last_addr);
		return 0;
	}

	if (prev_last_addr == 0)
		data_size = last_addr - pipe_info->buffer.iova + 1;
	else
		data_size = last_addr > prev_last_addr ?
			last_addr - prev_last_addr :
			pipe_info->buffer.size + last_addr - prev_last_addr;

	MPQ_DVB_DBG_PRINT(
		"%s: buf_size=%u, last_addr=0x%x, prev_last_addr=0x%x -> data size=%u\n",
		__func__, pipe_info->buffer.size, last_addr,
		prev_last_addr, data_size);

	return data_size;
}

/**
 * mpq_dmx_tspp2_ts_event_check() - check sufficient space in dmxdev filter
 * event queue or block until space is available.
 * Note that the function assumes that the pipe's mutex is locked.
 *
 * @feed:	dvb demux feed object
 * @pipe_info:	output pipe associated with feed
 *
 * return errors status
 */
static int mpq_dmx_tspp2_ts_event_check(struct dvb_demux_feed *feed,
	struct pipe_info *pipe_info)
{
	int ret;
	u32 session_id;

	if (feed->demux->playback_mode != DMX_PB_MODE_PULL)
		return 0;

	if (!mutex_is_locked(&pipe_info->mutex))
		return -EINVAL;

	/*
	 * Trigger buffer control callback with 0 bytes required space to
	 * verify event queue in the demux device filter is not overflowed.
	 * Data buffer is guaranteed not to overflow by TSPPv2 HW.
	 * Since we may block waiting for the event queue, need to release the
	 * pipe mutex, and when returning verify the pipe was not closed /
	 * re-opened in the meantime.
	 */
	session_id = pipe_info->session_id;
	mutex_unlock(&pipe_info->mutex);
	ret = feed->demux->buffer_ctrl.ts(&feed->feed.ts, 0, 1);
	mutex_lock(&pipe_info->mutex);
	if (ret) {
		MPQ_DVB_DBG_PRINT(
			"%s: buffer_ctrl.ts callback failed, ret=%d!\n",
			__func__, ret);
		return ret;
	}
	if (pipe_info->session_id != session_id || !pipe_info->ref_count) {
		MPQ_DVB_DBG_PRINT(
			"%s: pipe was closed / reopened: ref. count=%u, session_id=%u (expected %u)\n",
			__func__, pipe_info->ref_count,
			pipe_info->session_id, session_id);
		return -ENODEV;
	}

	return 0;
}

static int mpq_dmx_tspp2_stream_buffer_event_check(struct dvb_demux_feed *feed,
	struct pipe_info *pipe_info)
{
	int ret;
	u32 session_id;

	if (feed->demux->playback_mode != DMX_PB_MODE_PULL)
		return 0;

	if (!mutex_is_locked(&pipe_info->mutex))
		return -EINVAL;

	/*
	 * For pull mode need to wait for sufficient room to write the
	 * meta-data packet in the mpq_streambuffer object.
	 * Data itself was already written by TSPPv2 hardware (so required_space
	 * argument is 0).
	 * Since this may block waiting for the metadata buffer, pipe mutex
	 * needs to be released, and when returning verify the pipe was not
	 * closed / re-opened in the meantime.
	 */
	session_id = pipe_info->session_id;
	mutex_unlock(&pipe_info->mutex);
	ret = mpq_dmx_decoder_fullness_wait(feed, 0);
	mutex_lock(&pipe_info->mutex);
	if (ret) {
		MPQ_DVB_DBG_PRINT(
			"%s: mpq_dmx_decoder_fullness_wait failed, ret=%d\n",
			__func__, ret);
		return ret;
	}
	if (pipe_info->session_id != session_id || !pipe_info->ref_count) {
		MPQ_DVB_DBG_PRINT(
			"%s: pipe was closed / re-opened: ref. count=%u, session_id=%u (expected %u)\n",
			__func__, pipe_info->ref_count,
			pipe_info->session_id, session_id);
		return -ENODEV;
	}

	return 0;
}

/**
 * mpq_dmx_tspp2_sbm_work() - filter scrambling bit monitor
 *
 * @worker:		work object of filter delayed work
 */
static void mpq_dmx_tspp2_sbm_work(struct work_struct *worker)
{
	struct mpq_dmx_tspp2_filter *filter =
		container_of(to_delayed_work(worker),
			struct mpq_dmx_tspp2_filter, dwork);
	struct dvb_demux *dvb_demux;
	struct dvb_demux_feed *feed;
	struct mpq_feed *mpq_feed;
	struct mpq_tspp2_feed *tspp2_feed;
	struct dmx_data_ready event;
	u8 scramble_bits;
	int ret;

	if (mutex_lock_interruptible(&mpq_dmx_tspp2_info.mutex))
		return;

	/* Check filter was not closed */
	if (filter->handle == TSPP2_INVALID_HANDLE)
		goto end;

	ret = tspp2_filter_current_scrambling_bits_get(filter->handle,
		&scramble_bits);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: tspp2_filter_current_scrambling_bits_get failed, pid=%u, ret=%d\n",
			__func__, filter->pid, ret);
		goto end;
	}

	if (scramble_bits != filter->scm_prev_val) {
		filter->scm_count = 0;
		filter->scm_prev_val = scramble_bits;
	}

	/* Prevent overflow to the counter */
	if (filter->scm_count >= TSPP2_DMX_SB_MONITOR_THRESHOLD)
		goto end;

	filter->scm_count++;

	/* Scrambling bit status not stable enough yet */
	if (filter->scm_count < TSPP2_DMX_SB_MONITOR_THRESHOLD)
		goto end;

	/* Scrambling bit change is stable enough and can be reported */
	event.status = DMX_OK_SCRAMBLING_STATUS;
	event.data_length = 0;

	dvb_demux = &filter->source_info->demux_src.mpq_demux->demux;
	spin_lock(&dvb_demux->lock);
	list_for_each_entry(feed, &dvb_demux->feed_list, list_head) {
		mpq_feed = feed->priv;
		tspp2_feed = mpq_feed->plugin_priv;
		if ((feed->type == DMX_TYPE_TS &&
			!feed->feed.ts.is_filtering) ||
			(feed->type == DMX_TYPE_SEC &&
				!feed->feed.sec.is_filtering) ||
			tspp2_feed->filter != filter ||
			feed->scrambling_bits == scramble_bits)
			continue;

		/*
		 * Notify on scrambling status change only when we move from
		 * clear (0) to non-clear and vise-versa.
		 */
		if ((!feed->scrambling_bits && scramble_bits) ||
			(feed->scrambling_bits && !scramble_bits)) {
			event.scrambling_bits.pid = feed->pid;
			event.scrambling_bits.old_value = feed->scrambling_bits;
			event.scrambling_bits.new_value = scramble_bits;
			if (feed->type == DMX_TYPE_TS)
				feed->data_ready_cb.ts(&feed->feed.ts, &event);
			else
				dvb_dmx_notify_section_event(feed, &event, 0);
		}

		/* Update current state */
		feed->scrambling_bits = scramble_bits;
	}
	spin_unlock(&dvb_demux->lock);

end:
	if (filter->handle != TSPP2_INVALID_HANDLE)
		schedule_delayed_work(&filter->dwork,
			msecs_to_jiffies(TSPP2_DMX_SB_MONITOR_INTERVAL));

	mutex_unlock(&mpq_dmx_tspp2_info.mutex);
	return;
}

static void mpq_dmx_tspp2_start_scramble_bit_monitor(
	struct mpq_dmx_tspp2_filter *filter)
{
	/* Monitor only filters for specific PID */
	if (filter->pid == 0x2000)
		return;

	MPQ_DVB_DBG_PRINT(
		"%s: started scrambling bit monitor (pid=%u)\n",
		__func__, filter->pid);

	filter->scm_prev_val = 0;
	filter->scm_count = 0;
	INIT_DELAYED_WORK(&filter->dwork, mpq_dmx_tspp2_sbm_work);
	schedule_delayed_work(&filter->dwork,
		msecs_to_jiffies(TSPP2_DMX_SB_MONITOR_INTERVAL));
}

/**
 * Returns an existing or create a new filter in the TSPP2 driver for the
 * specified pid.
 *
 * @pid: pid of filter to lookup / create
 * @source_info: source associated with the filter

 * Return  pointer to filter object on success, NULL otherwise
 */
static struct mpq_dmx_tspp2_filter *mpq_dmx_tspp2_get_filter(u16 pid,
	struct source_info *source_info)
{
	int i;
	int ret;
	struct mpq_dmx_tspp2_filter *filter;

	for (i = 0; i < TSPP2_DMX_MAX_PID_FILTER_NUM; i++) {
		filter = &mpq_dmx_tspp2_info.filters[i];
		if (filter->handle != TSPP2_INVALID_HANDLE &&
			filter->source_info == source_info &&
			filter->pid == pid)
			return filter;
	}

	for (i = 0; i < TSPP2_DMX_MAX_PID_FILTER_NUM; i++) {
		filter = &mpq_dmx_tspp2_info.filters[i];
		if (filter->handle == TSPP2_INVALID_HANDLE)
			break;
	}

	if (i == TSPP2_DMX_MAX_PID_FILTER_NUM)
		return NULL;

	ret = tspp2_filter_open(source_info->handle,
		(pid == 0x2000) ? 0 : pid,
		(pid == 0x2000) ? 0 : 0x1FFF,
		&filter->handle);
	if (ret) {
		MPQ_DVB_ERR_PRINT("%s: tspp2_filter_open failed, ret=%d\n",
				__func__, ret);
		return NULL;
	}

	MPQ_DVB_DBG_PRINT(
		"%s: tspp2_filter_open(pid=%u) success, filter handle=0x%x\n",
		__func__, pid, filter->handle);

	filter->pid = pid;
	filter->num_ops = 0;
	filter->num_pes_ops = 0;
	filter->indexing_enabled = 0;
	filter->source_info = source_info;
	INIT_LIST_HEAD(&filter->operations_list);

	return filter;
}

/**
 * Close the specified filter in the TSPP2 driver
 *
 * @filter: filter object
 *
 * Return  0 on success, error code otherwise
 */
static int mpq_dmx_tspp2_close_filter(struct mpq_dmx_tspp2_filter *filter)
{
	int ret;
	bool cancelled;

	/* Filter is still being used */
	if (filter->num_ops)
		return 0;

	ret = tspp2_filter_disable(filter->handle);
	if (ret)
		MPQ_DVB_ERR_PRINT(
			"%s: tspp2_filter_disable(0x%0x) failed, ret=%d\n",
			__func__, filter->handle, ret);
	else
		MPQ_DVB_DBG_PRINT("%s: tspp2_filter_disable(0x%0x) success\n",
			__func__, filter->handle);

	ret = tspp2_filter_operations_clear(filter->handle);
	if (ret)
		MPQ_DVB_ERR_PRINT(
			"%s: tspp2_filter_operations_clear(0x%0x) failed, ret=%d\n",
			__func__, filter->handle, ret);
	else
		MPQ_DVB_DBG_PRINT(
			"%s: tspp2_filter_operations_clear(0x%0x) success\n",
			__func__, filter->handle);

	ret = tspp2_filter_close(filter->handle);
	if (ret)
		MPQ_DVB_ERR_PRINT(
			"%s: tspp2_filter_close(0x%0x) failed, ret=%d\n",
			__func__, filter->handle, ret);
	else
		MPQ_DVB_DBG_PRINT("%s: tspp2_filter_close(0x%0x) success\n",
			__func__, filter->handle);

	filter->handle = TSPP2_INVALID_HANDLE;
	filter->source_info = NULL;

	if (filter->pid != 0x2000) {
		cancelled = cancel_delayed_work(&filter->dwork);
		MPQ_DVB_DBG_PRINT(
			"%s: canceling scrambling bit monitor work (canceled=%d)\n",
			__func__, cancelled);
	}

	return ret;
}

/**
 * Sets all current filter operations in the TSPP2 driver for the specified
 * filter.
 *
 * @filter: filter object
 *
 * Return  0 on success, error code otherwise
 */
static int mpq_dmx_tspp2_set_filter_ops(struct mpq_dmx_tspp2_filter *filter)
{
	struct tspp2_operation ops[TSPP2_MAX_OPS_PER_FILTER];
	struct mpq_dmx_tspp2_filter_op *filter_op;
	int i = 0;
	int ret;

	list_for_each_entry(filter_op, &filter->operations_list, next) {
		ops[i] = filter_op->op;
		i++;
	}

	ret = tspp2_filter_operations_set(filter->handle, ops, filter->num_ops);
	if (ret)
		MPQ_DVB_ERR_PRINT(
			"%s: tspp2_filter_operations_set(0x%0x) failed, ret=%d\n",
			__func__, filter->handle, ret);
	else
		MPQ_DVB_DBG_PRINT(
			"%s: tspp2_filter_operations_set(0x%0x) success\n",
			__func__, filter->handle);

	return ret;
}

/**
 * Initializes a filter object for recording
 *
 * @feed:		dvb demux feed object
 * @source_info:	source to associate the filter with
 * @tsp_out_format:	required TS packet output format for recording
 *
 * Return  0 on success, error code otherwise
 */
static int mpq_dmx_tspp2_init_raw_filter(struct dvb_demux_feed *feed,
	struct mpq_dmx_tspp2_filter *filter,
	enum dmx_tsp_format_t tsp_out_format)
{
	struct mpq_feed *mpq_feed = feed->priv;
	struct mpq_tspp2_feed *mpq_tspp2_feed = mpq_feed->plugin_priv;
	struct source_info *source_info = filter->source_info;
	struct pipe_info *pipe_info;
	struct mpq_dmx_tspp2_filter_op *rec_op;
	enum tspp2_operation_timestamp_mode timestamp_mode;
	int ret;

	if (mpq_tspp2_feed->main_pipe == NULL) {
		MPQ_DVB_ERR_PRINT("%s: NULL pipe\n", __func__);
		return -EINVAL;
	}

	pipe_info = mpq_tspp2_feed->main_pipe;

	mpq_tspp2_feed->op_count = 0;
	rec_op = &mpq_tspp2_feed->ops[mpq_tspp2_feed->op_count];
	mpq_tspp2_feed->op_count++;

	rec_op->op.type = TSPP2_OP_RAW_TRANSMIT;
	rec_op->op.params.raw_transmit.input = TSPP2_OP_BUFFER_A;

	if ((source_info->demux_src.mpq_demux->source >= DMX_SOURCE_DVR0) &&
		(source_info->tsp_format == TSPP2_PACKET_FORMAT_188_RAW))
		timestamp_mode = TSPP2_OP_TIMESTAMP_ZERO;
	else
		timestamp_mode = TSPP2_OP_TIMESTAMP_STC;

	if (tsp_out_format == DMX_TSP_FORMAT_188) {
		rec_op->op.params.raw_transmit.timestamp_mode =
			TSPP2_OP_TIMESTAMP_NONE;
		rec_op->op.params.raw_transmit.timestamp_position =
			TSPP2_PACKET_FORMAT_188_RAW;
	} else if (tsp_out_format == DMX_TSP_FORMAT_192_HEAD) {
		rec_op->op.params.raw_transmit.timestamp_mode =
			timestamp_mode;
		rec_op->op.params.raw_transmit.timestamp_position =
			TSPP2_PACKET_FORMAT_192_HEAD;
	} else if (tsp_out_format == DMX_TSP_FORMAT_192_TAIL) {
		rec_op->op.params.raw_transmit.timestamp_mode =
			timestamp_mode;
		rec_op->op.params.raw_transmit.timestamp_position =
			TSPP2_PACKET_FORMAT_192_TAIL;
	} else {
		MPQ_DVB_ERR_PRINT(
			"%s: unsupported ts packet output format %d\n",
			__func__, tsp_out_format);
		ret = -EINVAL;
		goto release_op;
	}

	rec_op->op.params.raw_transmit.support_indexing =
		feed->idx_params.enable;
	rec_op->op.params.raw_transmit.skip_ts_errs = 0;
	rec_op->op.params.raw_transmit.output_pipe_handle = pipe_info->handle;


	/* Append RAW_TX operation to filter operations list */
	MPQ_DVB_DBG_PRINT("%s: Appending RAW_TX, TS mode=%d, TS pos=%d\n",
		__func__,
		 rec_op->op.params.raw_transmit.timestamp_mode,
		 rec_op->op.params.raw_transmit.timestamp_position);
	list_add_tail(&rec_op->next, &filter->operations_list);
	filter->num_ops++;

	ret = mpq_dmx_tspp2_set_filter_ops(filter);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_dmx_tspp2_set_filter_ops failed, ret=%d\n",
			__func__, ret);
		goto remove_op;
	}

	mpq_tspp2_feed->filter = filter;
	return 0;

remove_op:
	list_del(&rec_op->next);
	filter->num_ops--;
release_op:
	mpq_tspp2_feed->op_count = 0;

	return ret;
}

/**
 * Initializes a filter object for PCR filtering
 *
 * @feed: dvb demux feed object
 * @source_info: source to associate the filter with
 *
 * Return  0 on success, error code otherwise
 */
static int mpq_dmx_tspp2_init_pcr_filter(struct dvb_demux_feed *feed,
	struct mpq_dmx_tspp2_filter *filter)
{
	struct mpq_feed *mpq_feed = feed->priv;
	struct mpq_tspp2_feed *mpq_tspp2_feed = mpq_feed->plugin_priv;
	struct pipe_info *pipe_info;
	struct mpq_dmx_tspp2_filter_op *pcr_op;
	int ret;

	if (mpq_tspp2_feed->main_pipe == NULL) {
		MPQ_DVB_ERR_PRINT("%s: invalid pipe\n", __func__);
		return -EINVAL;
	}

	pipe_info = mpq_tspp2_feed->main_pipe;

	mpq_tspp2_feed->op_count = 0;
	pcr_op = &mpq_tspp2_feed->ops[mpq_tspp2_feed->op_count];
	mpq_tspp2_feed->op_count++;

	pcr_op->op.type = TSPP2_OP_PCR_EXTRACTION;
	pcr_op->op.params.pcr_extraction.input = TSPP2_OP_BUFFER_A;
	pcr_op->op.params.pcr_extraction.skip_ts_errs = 0;
	pcr_op->op.params.pcr_extraction.extract_pcr = 1;
	pcr_op->op.params.pcr_extraction.extract_opcr = 0;
	pcr_op->op.params.pcr_extraction.extract_splicing_point = 0;
	pcr_op->op.params.pcr_extraction.extract_transport_private_data = 0;
	pcr_op->op.params.pcr_extraction.extract_af_extension = 0;
	pcr_op->op.params.pcr_extraction.extract_all_af = 0;
	pcr_op->op.params.pcr_extraction.output_pipe_handle = pipe_info->handle;

	/* Add PCR_TX operation at beginning of filter operations list */
	list_add(&pcr_op->next, &filter->operations_list);
	filter->num_ops++;

	ret = mpq_dmx_tspp2_set_filter_ops(filter);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_dmx_tspp2_set_filter_ops failed, ret=%d\n",
			__func__, ret);
		goto remove_op;
	}

	mpq_tspp2_feed->filter = filter;
	return 0;

remove_op:
	list_del(&pcr_op->next);
	filter->num_ops--;
	mpq_tspp2_feed->op_count = 0;
	return ret;
}

/**
 * Add the special PES Analysis operation to the specified filter
 * This operation should appear only once in a filter's operations list,
 * so a reference count is kept for it.
 *
 * @filter: filter object
 */
static void mpq_dmx_tspp2_add_pes_analysis_op(
	struct mpq_dmx_tspp2_filter *filter)
{
	struct tspp2_op_pes_analysis_params *op_params =
		&filter->pes_analysis_op.op.params.pes_analysis;

	if (!filter->num_pes_ops) {
		filter->pes_analysis_op.op.type = TSPP2_OP_PES_ANALYSIS;
		op_params->input = TSPP2_OP_BUFFER_A;
		op_params->skip_ts_errs = 0;

		list_add_tail(&filter->pes_analysis_op.next,
			&filter->operations_list);
		filter->num_ops++;
	}

	filter->num_pes_ops++;
}

/**
 * Remove the special PES Analysis operation from the specified filter
 * This operation should appear only once in a filter's operations list,
 * so a reference count is kept for it.
 *
 * @filter: filter object
 */
static void mpq_dmx_tspp2_del_pes_analysis_op(
	struct mpq_dmx_tspp2_filter *filter)
{
	if (!filter->num_pes_ops)
		return;

	filter->num_pes_ops--;
	if (!filter->num_pes_ops) {
		list_del(&filter->pes_analysis_op.next);
		filter->num_ops--;
	}
}

/**
 * mpq_dmx_tspp2_init_index_filter() - initialize a filter with indexing
 *
 * @feed: dvb demux feed object
 * @source_info: source associated with the filter
 *
 * Return  0 on success, error code otherwise
 */
static int mpq_dmx_tspp2_init_index_filter(struct dvb_demux_feed *feed,
	struct mpq_dmx_tspp2_filter *filter)
{
	struct mpq_feed *mpq_feed = feed->priv;
	struct mpq_tspp2_feed *tspp2_feed = mpq_feed->plugin_priv;
	struct pipe_info *pipe_info;
	struct mpq_dmx_tspp2_filter_op *index_op;
	struct mpq_dmx_tspp2_filter_op *rec_op;
	int ret;

	if (tspp2_feed->secondary_pipe == NULL) {
		MPQ_DVB_ERR_PRINT("%s(pid=%u): NULL secondary pipe\n",
			__func__, feed->pid);
		return -EINVAL;
	}

	if (filter->indexing_enabled) {
		MPQ_DVB_ERR_PRINT(
			"%s(pid=%u): Indexing can be done only once\n",
			__func__, feed->pid);
		return -EPERM;
	}

	pipe_info = tspp2_feed->secondary_pipe;
	index_op = &filter->index_op;
	rec_op = &tspp2_feed->ops[0];

	/*
	 * PES analysis operation must precede indexing operation.
	 * RAW operation already exists in the filter and is always the first
	 * operation of the feed, verify addressing enable bit in the RAW op.
	 */
	rec_op->op.params.raw_transmit.support_indexing = 1;
	mpq_dmx_tspp2_add_pes_analysis_op(filter);

	index_op->op.type = TSPP2_OP_INDEXING;
	index_op->op.params.indexing.input = TSPP2_OP_BUFFER_A;
	index_op->op.params.indexing.random_access_indicator_indexing = 0;
	index_op->op.params.indexing.indexing_table_id =
		tspp2_feed->index_table;
	index_op->op.params.indexing.skip_ts_errs = 0;
	index_op->op.params.indexing.output_pipe_handle = pipe_info->handle;

	MPQ_DVB_DBG_PRINT("%s: Appending Indexing\n", __func__);
	list_add_tail(&index_op->next, &filter->operations_list);
	filter->indexing_enabled = 1;
	filter->num_ops++;

	ret = mpq_dmx_tspp2_set_filter_ops(filter);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_dmx_tspp2_set_filter_ops failed, ret=%d\n",
			__func__, ret);
		goto remove_ops;
	}

	return 0;

remove_ops:
	list_del(&index_op->next);
	filter->num_ops--;
	filter->indexing_enabled = 0;
	mpq_dmx_tspp2_del_pes_analysis_op(filter);

	return ret;
}

/**
 * Terminates a filter object
 *
 * @feed: dvb demux feed object
 * @source_info: source associated with the filter
 *
 * Return  0 on success, error code otherwise
 */
static int mpq_dmx_tspp2_terminate_filter(struct dvb_demux_feed *feed,
	struct source_info *source_info)
{
	struct mpq_feed *mpq_feed = feed->priv;
	struct mpq_tspp2_feed *mpq_tspp2_feed = mpq_feed->plugin_priv;
	int ret;
	int i;

	if (!mpq_tspp2_feed->filter)
		return -EINVAL;

	/* Remove all operations owned by feed */
	for (i = 0; i < mpq_tspp2_feed->op_count; i++) {
		list_del(&mpq_tspp2_feed->ops[i].next);
		mpq_tspp2_feed->filter->num_ops--;
	}
	mpq_tspp2_feed->op_count = 0;

	/* Remove PES analysis operation */
	if ((feed->ts_type & (TS_PAYLOAD_ONLY | TS_DECODER) &&
		!dvb_dmx_is_pcr_feed(feed)) || feed->idx_params.enable)
		mpq_dmx_tspp2_del_pes_analysis_op(mpq_tspp2_feed->filter);

	/* Remove indexing operation */
	if (mpq_tspp2_feed->filter->indexing_enabled) {
		list_del(&mpq_tspp2_feed->filter->index_op.next);
		mpq_tspp2_feed->filter->num_ops--;
		mpq_tspp2_feed->filter->indexing_enabled = 0;
	}

	if (mpq_tspp2_feed->filter->num_ops) {
		ret = mpq_dmx_tspp2_set_filter_ops(mpq_tspp2_feed->filter);
		if (ret)
			MPQ_DVB_ERR_PRINT(
				"%s: mpq_dmx_tspp2_set_filter_ops failed, ret=%d\n",
				__func__, ret);
	} else {
		ret = mpq_dmx_tspp2_close_filter(mpq_tspp2_feed->filter);
		if (ret)
			MPQ_DVB_ERR_PRINT(
				"%s: mpq_dmx_tspp2_close_filter failed, ret=%d\n",
				__func__, ret);
	}

	mpq_tspp2_feed->filter = NULL;
	return ret;
}

/**
 * Add the specified PES operation to the specified filter's operations list
 *
 * @filter: filter object
 * @pes_op: filter operation configured to PES
 *
 * Return 0 on success, error code otherwise
 */
static int mpq_dmx_tspp2_setup_pes_op(struct mpq_dmx_tspp2_filter *filter,
	struct mpq_dmx_tspp2_filter_op *pes_op)
{
	int ret;

	/* Append PES analysis operation to filter operations list */
	mpq_dmx_tspp2_add_pes_analysis_op(filter);

	/* Append PES operation to filter operations list */
	list_add_tail(&pes_op->next, &filter->operations_list);
	filter->num_ops++;

	ret = mpq_dmx_tspp2_set_filter_ops(filter);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_dmx_tspp2_set_filter_ops failed, ret=%d\n",
			__func__, ret);
		goto remove_op;
	}

	return 0;

remove_op:
	mpq_dmx_tspp2_del_pes_analysis_op(filter);
	list_del(&pes_op->next);
	filter->num_ops--;

	return ret;
}

/**
 * Initializes a filter object for full-PES filtering
 *
 * @feed: dvb demux feed object
 * @source_info: source to associate the filter with
 *
 * Return  0 on success, error code otherwise
 */
static int mpq_dmx_tspp2_init_pes_filter(struct dvb_demux_feed *feed,
	struct mpq_dmx_tspp2_filter *filter)
{
	struct mpq_feed *mpq_feed = feed->priv;
	struct mpq_tspp2_feed *mpq_tspp2_feed = mpq_feed->plugin_priv;
	struct pipe_info *pipe_info;
	struct mpq_dmx_tspp2_filter_op *pes_op;
	int ret;

	if (mpq_tspp2_feed->main_pipe == NULL) {
		MPQ_DVB_ERR_PRINT("%s: invalid pipe\n", __func__);
		return -EINVAL;
	}

	pipe_info = mpq_tspp2_feed->main_pipe;

	mpq_tspp2_feed->op_count = 0;
	pes_op = &mpq_tspp2_feed->ops[mpq_tspp2_feed->op_count];

	pes_op->op.type = TSPP2_OP_PES_TRANSMIT;
	pes_op->op.params.pes_transmit.input = TSPP2_OP_BUFFER_A;
	pes_op->op.params.pes_transmit.mode = TSPP2_OP_PES_TRANSMIT_FULL;
	pes_op->op.params.pes_transmit.enable_sw_indexing = 0;
	pes_op->op.params.pes_transmit.attach_stc_flags = 1;
	pes_op->op.params.pes_transmit.disable_tx_on_pes_discontinuity = 0;
	pes_op->op.params.pes_transmit.output_pipe_handle = pipe_info->handle;
	pes_op->op.params.pes_transmit.header_output_pipe_handle =
		TSPP2_INVALID_HANDLE;

	ret = mpq_dmx_tspp2_setup_pes_op(filter, pes_op);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_dmx_tspp2_setup_pes_op(0x%0x) failed, ret=%d\n",
			__func__, filter->handle, ret);

		return ret;
	}

	mpq_tspp2_feed->op_count++;
	mpq_tspp2_feed->filter = filter;

	return 0;
}

/**
 * Initializes a filter object for section filtering
 *
 * @feed: dvb demux feed object
 * @source_info: source to associate the filter with
 *
 * Return  0 on success, error code otherwise
 */
static int mpq_dmx_tspp2_init_sec_filter(struct dvb_demux_feed *feed,
	struct mpq_dmx_tspp2_filter *filter)
{
	return mpq_dmx_tspp2_init_raw_filter(feed, filter,
		DMX_TSP_FORMAT_188);
}

static int mpq_dmx_tspp2_init_rec_filter(struct dvb_demux_feed *feed,
	struct mpq_dmx_tspp2_filter *filter)
{
	int ret;

	ret = mpq_dmx_tspp2_init_raw_filter(feed, filter, feed->tsp_out_format);
	if (!ret && feed->idx_params.enable && feed->pattern_num)
		ret = mpq_dmx_tspp2_init_index_filter(feed, filter);

	return ret;
}

/**
 * Initializes a filter object for video decoder (separated PES) filtering
 *
 * @feed: dvb demux feed object
 * @source_info: source to associate the filter with
 *
 * Return  0 on success, error code otherwise
 */
static int mpq_dmx_tspp2_init_decoder_filter(struct dvb_demux_feed *feed,
	struct mpq_dmx_tspp2_filter *filter)
{
	struct mpq_feed *mpq_feed = feed->priv;
	struct mpq_tspp2_feed *mpq_tspp2_feed = mpq_feed->plugin_priv;
	struct pipe_info *pipe_info;
	struct pipe_info *header_pipe_info;
	struct mpq_dmx_tspp2_filter_op *spes_op;
	int ret;

	if (mpq_tspp2_feed->main_pipe == NULL ||
		mpq_tspp2_feed->secondary_pipe == NULL) {
		MPQ_DVB_ERR_PRINT("%s: invalid pipe(s)\n", __func__);
		return -EINVAL;
	}

	pipe_info = mpq_tspp2_feed->main_pipe;
	header_pipe_info = mpq_tspp2_feed->secondary_pipe;

	mpq_tspp2_feed->op_count = 0;
	spes_op = &mpq_tspp2_feed->ops[mpq_tspp2_feed->op_count];

	spes_op->op.type = TSPP2_OP_PES_TRANSMIT;
	spes_op->op.params.pes_transmit.input = TSPP2_OP_BUFFER_A;
	spes_op->op.params.pes_transmit.mode = TSPP2_OP_PES_TRANSMIT_SEPARATED;
	spes_op->op.params.pes_transmit.enable_sw_indexing = 0;
	spes_op->op.params.pes_transmit.attach_stc_flags = 1;
	spes_op->op.params.pes_transmit.disable_tx_on_pes_discontinuity = 0;
	spes_op->op.params.pes_transmit.output_pipe_handle = pipe_info->handle;
	spes_op->op.params.pes_transmit.header_output_pipe_handle =
		header_pipe_info->handle;

	ret = mpq_dmx_tspp2_setup_pes_op(filter, spes_op);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_dmx_tspp2_setup_pes_op(0x%0x) failed, ret=%d\n",
			__func__, filter->handle, ret);

		return ret;
	}

	mpq_tspp2_feed->op_count++;
	mpq_tspp2_feed->filter = filter;

	return 0;
}

/**
 * Initializes a filter object for the specified filter feed
 *
 * @feed: dvb demux feed object
 * @source_info: source to associate the filter with
 *
 * Return  0 on success, error code otherwise
 */
static int mpq_dmx_tspp2_init_filter(struct dvb_demux_feed *feed,
	struct source_info *source_info)
{
	int ret;
	struct mpq_feed *mpq_feed = feed->priv;
	struct mpq_tspp2_feed *tspp2_feed = mpq_feed->plugin_priv;
	struct mpq_dmx_tspp2_filter *filter;
	bool start_sb_monitor;

	filter = mpq_dmx_tspp2_get_filter(feed->pid, source_info);
	if (filter == NULL || filter->num_ops == TSPP2_MAX_OPS_PER_FILTER)
		return -ENOMEM;

	/* Start the scrambling bit monitor once per filter */
	start_sb_monitor = (filter->num_ops == 0);

	if (feed->type == DMX_TYPE_SEC)
		ret = mpq_dmx_tspp2_init_sec_filter(feed, filter);
	else if (dvb_dmx_is_pcr_feed(feed))
		ret = mpq_dmx_tspp2_init_pcr_filter(feed, filter);
	else if (dvb_dmx_is_video_feed(feed))
		ret = mpq_dmx_tspp2_init_decoder_filter(feed, filter);
	else if (feed->ts_type & TS_PAYLOAD_ONLY)
		ret = mpq_dmx_tspp2_init_pes_filter(feed, filter);
	else /* Recording case */
		ret = mpq_dmx_tspp2_init_rec_filter(feed, filter);

	if (!ret && start_sb_monitor)
		mpq_dmx_tspp2_start_scramble_bit_monitor(tspp2_feed->filter);

	if (ret)
		mpq_dmx_tspp2_close_filter(filter);

	return ret;
}

static int mpq_dmx_tspp2_remove_indexing_op(struct mpq_dmx_tspp2_filter *filter)
{
	if (!filter->indexing_enabled)
		return 0;

	/* Remove indexing operation */
	list_del(&filter->index_op.next);
	filter->num_ops--;
	filter->indexing_enabled = 0;

	mpq_dmx_tspp2_del_pes_analysis_op(filter);

	return mpq_dmx_tspp2_set_filter_ops(filter);
}

/**
 * mpq_dmx_init_out_pipe - Initialize a new output pipe with TSPP2 driver
 *
 * In case pipe buffer is already allocated (and mapped to kernel memory),
 * pipe_info->buffer.handle must be initialized with that buffer's handle.
 * Or, memory can be allocated by this function for internal buffers.
 *
 * If successful pipe_info->buffer.iova is updated with proper mapping of the
 * buffer with TSPP2. In case internal allocation was requested
 * pipe_info->buffer.handle is also updated.
 *
 * @mpq_demux: mpq demux instance
 * @buffer_size: Size of the pipe buffer
 * @sps_cfg: Pipe sps configuration
 * @pull_cfg: Pipe pull mode configuration
 * @allocate_mem: Indicates whether memory should be allocated for the pipe,
 * or if it was already allocated externally. If set, will allocate buffer
 * with size 'buffer_size'.
 * @is_secure: Indicates whether allocation should be for secured buffer
 *
 * Return  0 on success, error status otherwise
 */
static int mpq_dmx_init_out_pipe(struct mpq_demux *mpq_demux,
	struct pipe_info *pipe_info, u32 buffer_size,
	struct tspp2_pipe_sps_params *sps_cfg,
	struct tspp2_pipe_pull_mode_params *pull_cfg,
	int allocate_mem, unsigned int ion_heap, int is_secure)
{
	int ret;
	size_t align;
	unsigned int ion_flags = 0;
	struct mpq_tspp2_demux *mpq_tspp2_demux = mpq_demux->plugin_priv;
	struct source_info *source_info;
	int kernel_map = 0;
	struct tspp2_pipe_config_params *pipe_cfg;

	if (pipe_info->handle != TSPP2_INVALID_HANDLE) {
		MPQ_DVB_ERR_PRINT(
			"%s: pipe already opened! handle=0x%x\n",
			__func__, pipe_info->handle);
		return -EINVAL;
	}

	source_info = mpq_tspp2_demux->source_info;
	if (source_info == NULL) {
		MPQ_DVB_ERR_PRINT("%s: invalid demux source %d\n", __func__,
			mpq_demux->source);
		return -ENODEV;
	}

	/* Allocate pipe buffer */
	if (allocate_mem) {
		if (is_secure) {
			align = SZ_1M;
			ion_flags |= ION_FLAG_SECURE;
		} else {
			align = SZ_4K;
		}

		pipe_info->buffer.handle = ion_alloc(mpq_demux->ion_client,
			buffer_size, align, ion_heap, ion_flags);
		if (IS_ERR_OR_NULL(pipe_info->buffer.handle)) {
			ret = PTR_ERR(pipe_info->buffer.handle);
			MPQ_DVB_ERR_PRINT(
				"%s: failed to allocate buffer, %d\n",
				__func__, ret);
			if (!ret)
				ret = -ENOMEM;
			goto end;
		}

		if (!is_secure) {
			pipe_info->buffer.mem =
				ion_map_kernel(mpq_demux->ion_client,
					pipe_info->buffer.handle);
			if (IS_ERR_OR_NULL(pipe_info->buffer.mem)) {
				ret = PTR_ERR(pipe_info->buffer.mem);
				MPQ_DVB_ERR_PRINT(
					"%s: failed mapping buffer to kernel, %d\n",
					__func__, ret);
				if (!ret)
					ret = -ENOMEM;
				goto free_mem;
			}
			kernel_map = 1;
		} else {
			pipe_info->buffer.mem = NULL;
		}
	} else {
		if (pipe_info->buffer.handle == NULL) {
			MPQ_DVB_ERR_PRINT(
				"%s: pipe buffer handle is null\n", __func__);
			return -EINVAL;
		}
	}

	pipe_cfg = &pipe_info->pipe_cfg;
	pipe_cfg->ion_client = mpq_demux->ion_client;
	pipe_cfg->buffer_handle = pipe_info->buffer.handle;
	pipe_cfg->buffer_size = buffer_size;
	pipe_cfg->pipe_mode = TSPP2_SRC_PIPE_OUTPUT;
	pipe_cfg->is_secure = 0;
	pipe_cfg->sps_cfg = *sps_cfg;

	ret = tspp2_pipe_open(TSPP2_DEVICE_ID, pipe_cfg,
		&pipe_info->buffer.iova, &pipe_info->handle);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: tspp2_pipe_open failed, ret=%d\n",
			__func__, ret);
		goto unmap_mem;
	}
	MPQ_DVB_DBG_PRINT(
		"%s: tspp2_pipe_open(): handle=0x%x, iova=0x%0x, mem=%p\n",
		__func__, pipe_info->handle, pipe_info->buffer.iova,
		pipe_info->buffer.mem);

	ret = tspp2_src_pipe_attach(source_info->handle, pipe_info->handle,
		pull_cfg);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: tspp2_src_pipe_attach(src=0x%0x, pipe=0x%0x) failed, ret=%d\n",
			__func__, source_info->handle, pipe_info->handle, ret);
		goto close_pipe;
	}
	source_info->ref_count++;
	MPQ_DVB_DBG_PRINT(
		"%s: tspp2_src_pipe_attach(src=0x%0x, pipe=0x%0x) success, new source ref. count=%u\n",
		__func__, source_info->handle, pipe_info->handle,
		source_info->ref_count);

	pipe_info->buffer.internal_mem = allocate_mem;
	pipe_info->buffer.kernel_map = kernel_map;
	pipe_info->buffer.size = buffer_size;
	pipe_info->tspp_last_addr = 0;
	pipe_info->tspp_write_offset = 0;
	pipe_info->tspp_read_offset = 0;
	pipe_info->bam_read_offset = 0;
	pipe_info->source_info = source_info;
	pipe_info->eos_pending = 0;
	pipe_info->session_id++;
	pipe_info->hw_missed_notif = 0;
	pipe_info->handler_count = 0;
	pipe_info->overflow = 0;
	return 0;

close_pipe:
	tspp2_pipe_close(pipe_info->handle);
unmap_mem:
	if (kernel_map)
		ion_unmap_kernel(mpq_demux->ion_client,
			pipe_info->buffer.handle);
free_mem:
	if (allocate_mem)
		ion_free(mpq_demux->ion_client, pipe_info->buffer.handle);
end:
	return ret;
}

/**
 * Terminate specified output pipe with TSPP2 driver
 *
 * Detaches & closes the pipe. If pipe buffer was internally allocated, unmap
 * and free it.
 *
 * @pipe: the pipe to terminate its memory
 *
 * Return  0 on success, error status otherwise
 */
static int mpq_dmx_terminate_out_pipe(struct pipe_info *pipe_info)
{
	struct mpq_demux *mpq_demux;
	struct source_info *source_info;
	int ref_count;
	unsigned long flags;

	if (pipe_info == NULL)
		return -EINVAL;

	spin_lock_irqsave(&pipe_info->lock, flags);
	ref_count = pipe_info->ref_count;
	spin_unlock_irqrestore(&pipe_info->lock, flags);
	if (ref_count) {
		MPQ_DVB_ERR_PRINT("%s: pipe still in use, ref=%d\n", __func__,
			ref_count);
		return -EBUSY;
	}

	if (pipe_info->handle == TSPP2_INVALID_HANDLE) {
		MPQ_DVB_ERR_PRINT("%s: pipe not open\n", __func__);
		return -EINVAL;
	}

	mpq_demux = pipe_info->parent->mpq_feed->mpq_demux;
	source_info = mpq_dmx_get_source(mpq_demux->source);
	if (source_info == NULL) {
		MPQ_DVB_ERR_PRINT("%s: invalid source %d\n", __func__,
			mpq_demux->source);
		return -ENODEV;
	}

	/* Must first detach and close pipe before freeing it's buffer */
	tspp2_src_pipe_detach(source_info->handle, pipe_info->handle);
	source_info->ref_count--;
	MPQ_DVB_DBG_PRINT("%s: detached output pipe, source ref. count=%u\n",
		__func__, source_info->ref_count);
	tspp2_pipe_close(pipe_info->handle);
	pipe_info->handle = TSPP2_INVALID_HANDLE;

	pipe_work_queue_cancel_work(&pipe_info->work_queue, pipe_info);

	if (pipe_info->buffer.kernel_map)
		ion_unmap_kernel(mpq_demux->ion_client,
			pipe_info->buffer.handle);

	if (pipe_info->buffer.internal_mem)
		ion_free(mpq_demux->ion_client, pipe_info->buffer.handle);

	pipe_info->buffer.mem = NULL;
	pipe_info->buffer.iova = 0;
	pipe_info->buffer.handle = NULL;

	return 0;
}

/**
 * Setup configuration parameters for the specified source
 *
 * @source_info: source_info object
 *
 * Return 0 on success, error code otherwise
 */
static int mpq_dmx_tspp2_source_setup(struct mpq_demux *mpq_demux,
	struct source_info *source_info)
{
	struct tspp2_src_scrambling_config scramble_cfg;
	enum tspp2_packet_format tsp_format;
	enum tspp2_src_scrambling_ctrl scramble_default;
	int scramble_even = mpq_dmx_get_param_scramble_even();
	int scramble_odd = mpq_dmx_get_param_scramble_odd();
	int ret;

	/* From demod we always get 188 TS packets */
	if (mpq_demux->source < DMX_SOURCE_DVR0) {
		tsp_format = TSPP2_PACKET_FORMAT_188_RAW;
	} else {
		switch (mpq_demux->demux.tsp_format) {
		case DMX_TSP_FORMAT_188:
			tsp_format = TSPP2_PACKET_FORMAT_188_RAW;
			break;
		case DMX_TSP_FORMAT_192_TAIL:
			tsp_format = TSPP2_PACKET_FORMAT_192_TAIL;
			break;
		case DMX_TSP_FORMAT_192_HEAD:
			tsp_format = TSPP2_PACKET_FORMAT_192_HEAD;
			break;
		case DMX_TSP_FORMAT_204:
		default:
			MPQ_DVB_ERR_PRINT(
				"%s: unsupported TS packet format %d\n",
				__func__,
				mpq_demux->demux.tsp_format);
			return -EINVAL;
		}
	}

	ret = tspp2_src_packet_format_set(source_info->handle, tsp_format);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: tspp2_src_packet_format_set failed for source=0x%0x, ret=%d\n",
			__func__, source_info->handle, ret);
		return ret;
	}
	source_info->tsp_format = tsp_format;

	ret = tspp2_src_parsing_option_set(source_info->handle,
		TSPP2_SRC_PARSING_OPT_CHECK_CONTINUITY, 1);

	if (!ret)
		ret = tspp2_src_parsing_option_set(source_info->handle,
		TSPP2_SRC_PARSING_OPT_IGNORE_DISCONTINUITY, 0);

	if (!ret)
		ret = tspp2_src_parsing_option_set(source_info->handle,
		TSPP2_SRC_PARSING_OPT_ASSUME_DUPLICATE_PACKETS, 0);

	if (!ret)
		ret = tspp2_src_parsing_option_set(source_info->handle,
		TSPP2_SRC_PARSING_OPT_DISCARD_INVALID_AF_PACKETS, 0);

	if (!ret)
		ret = tspp2_src_parsing_option_set(source_info->handle,
		TSPP2_SRC_PARSING_OPT_VERIFY_PES_START, 0);

	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: tspp2_src_parsing_option_set failed for source=0x%0x, ret=%d",
			__func__, source_info->handle, ret);
		return ret;
	}

	ret = tspp2_src_sync_byte_config_set(source_info->handle, 1,
		TS_PACKET_SYNC_BYTE);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: tspp2_src_sync_byte_config_set failed for source=0x%0x, ret=%d",
			__func__, source_info->handle, ret);
		return ret;
	}

	ret = tspp2_src_scrambling_config_get(source_info->handle,
		&scramble_cfg);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: tspp2_src_scrambling_config_get failed for source=0x%0x, ret=%d",
			__func__, source_info->handle, ret);
		return ret;
	}
	/*
	 * Scramble bits configuration:
	 * 0 is always pass through (clear packet),
	 * Odd & even are set according to the configured values,
	 * the remaining value is set according to the default action set.
	 */
	if (mpq_dmx_get_param_scramble_default_discard())
		scramble_default = TSPP2_SRC_SCRAMBLING_CTRL_DISCARD;
	else
		scramble_default = TSPP2_SRC_SCRAMBLING_CTRL_PASSTHROUGH;
	scramble_cfg.scrambling_0_ctrl = TSPP2_SRC_SCRAMBLING_CTRL_PASSTHROUGH;
	scramble_cfg.scrambling_1_ctrl = scramble_default;
	scramble_cfg.scrambling_2_ctrl = scramble_default;
	scramble_cfg.scrambling_3_ctrl = scramble_default;
	if (scramble_even == 1)
		scramble_cfg.scrambling_1_ctrl = TSPP2_SRC_SCRAMBLING_CTRL_EVEN;
	if (scramble_odd == 1)
		scramble_cfg.scrambling_1_ctrl = TSPP2_SRC_SCRAMBLING_CTRL_ODD;
	if (scramble_even == 2)
		scramble_cfg.scrambling_2_ctrl = TSPP2_SRC_SCRAMBLING_CTRL_EVEN;
	if (scramble_odd == 2)
		scramble_cfg.scrambling_2_ctrl = TSPP2_SRC_SCRAMBLING_CTRL_ODD;
	if (scramble_even == 3)
		scramble_cfg.scrambling_3_ctrl = TSPP2_SRC_SCRAMBLING_CTRL_EVEN;
	if (scramble_odd == 3)
		scramble_cfg.scrambling_3_ctrl = TSPP2_SRC_SCRAMBLING_CTRL_ODD;
	scramble_cfg.scrambling_bits_monitoring =
		TSPP2_SRC_SCRAMBLING_MONITOR_TS_ONLY;

	ret = tspp2_src_scrambling_config_set(source_info->handle,
		&scramble_cfg);
	if (ret)
		MPQ_DVB_ERR_PRINT(
			"%s: tspp2_src_scrambling_config_set failed for source=0x%0x, ret=%d",
			__func__, source_info->handle, ret);

	return ret;
}

/**
 * Initializes a new source in the TSPP2 driver for the specified demux instance
 *
 * @mpq_demux: mpq demux instance
 * @source_info: source_info object
 *
 * Return 0 on success, error code otherwise
 */
static int mpq_dmx_tspp2_init_source(struct mpq_demux *mpq_demux,
	struct source_info *source_info)
{
	int ret;
	int tsif_mode = mpq_dmx_get_param_tsif_mode();
	struct tspp2_src_cfg src_cfg;

	ret = mpq_dmx_tspp2_open_device();
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_dmx_tspp2_open_device failed, ret=%d\n",
			__func__, ret);
		goto end;
	}

	src_cfg.input = TSPP2_INPUT_MEMORY;
	if (mpq_demux->source == DMX_SOURCE_FRONT0 ||
		mpq_demux->source == DMX_SOURCE_FRONT1) {

		src_cfg.input = mpq_demux->source == DMX_SOURCE_FRONT0 ?
			TSPP2_INPUT_TSIF0 : TSPP2_INPUT_TSIF1;

		/* Below are only relevant for TSIF sources */
		switch (tsif_mode) {
		case 1:
			src_cfg.params.tsif_params.tsif_mode =
				TSPP2_TSIF_MODE_1;
			break;
		case 2:
			src_cfg.params.tsif_params.tsif_mode =
				TSPP2_TSIF_MODE_2;
			break;
		default:
			/*
			 * Keep compatibility with TSIF driver where 3 is
			 * loopback mode.
			 */
			src_cfg.params.tsif_params.tsif_mode =
				TSPP2_TSIF_MODE_LOOPBACK;
			break;
		}
		src_cfg.params.tsif_params.clock_inverse =
			mpq_dmx_get_param_clock_inv();
		src_cfg.params.tsif_params.data_inverse = data_inverse;
		src_cfg.params.tsif_params.sync_inverse = sync_inverse;
		src_cfg.params.tsif_params.enable_inverse = enable_inverse;
	}

	if (source_info->type == DEMUXING_SOURCE)
		source_info->demux_src.mpq_demux = mpq_demux;
	source_info->ref_count = 0;
	ret = tspp2_src_open(TSPP2_DEVICE_ID, &src_cfg,
		&source_info->handle);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: tspp2_src_open failed, ret=%d\n",
			__func__, ret);
		goto close_device;
	}
	MPQ_DVB_DBG_PRINT(
		"%s: tspp2_src_open success, source handle=0x%0x\n",
		__func__, source_info->handle);

	ret = mpq_dmx_tspp2_source_setup(mpq_demux, source_info);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_dmx_tspp2_source_setup failed, ret=%d\n",
			__func__, ret);
		goto close_source;
	}
	return 0;

close_source:
	tspp2_src_close(source_info->handle);
	source_info->handle = TSPP2_INVALID_HANDLE;
close_device:
	mpq_dmx_tspp2_close_device();
end:
	return ret;
}

/**
 * Terminate the specified source in the TSPP2 driver
 *
 * @source_info: source_info object
 *
 * Return 0 on success, error code otherwise
 */
static int mpq_dmx_tspp2_terminate_source(struct source_info *source_info)
{
	int ret;

	if (source_info->enabled) {
		ret = tspp2_src_disable(source_info->handle);
		if (ret)
			MPQ_DVB_ERR_PRINT(
				"%s: tspp2_src_disable failed, ret=%d\n",
				__func__, ret);
		source_info->enabled = 0;
	}

	ret = tspp2_src_close(source_info->handle);
	if (ret)
		MPQ_DVB_ERR_PRINT(
			"%s: tspp2_src_close failed, ret=%d\n",
			__func__, ret);

	source_info->handle = TSPP2_INVALID_HANDLE;
	source_info->demux_src.mpq_demux = NULL;

	ret = mpq_dmx_tspp2_close_device();
	if (ret)
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_dmx_tspp2_close_device failed, ret=%d\n",
			__func__, ret);

	return ret;
}

static int mpq_dmx_tspp2_open_source(struct mpq_demux *mpq_demux,
	struct source_info *source_info)
{
	int ret;
	struct pipe_info *pipe_info;

	if (source_info->handle == TSPP2_INVALID_HANDLE) {
		ret = mpq_dmx_tspp2_init_source(mpq_demux, source_info);
		if (ret) {
			MPQ_DVB_ERR_PRINT(
				"%s: mpq_dmx_tspp2_init_source failed, ret=%d\n",
				__func__, ret);
			goto end;
		}

		pipe_info = source_info->input_pipe;
		if (pipe_info && pipe_info->handle != TSPP2_INVALID_HANDLE) {
			/* Attach the existing input pipe to the source */
			ret = tspp2_src_pipe_attach(source_info->handle,
				pipe_info->handle, NULL);
			if (ret) {
				MPQ_DVB_ERR_PRINT(
					"%s: tspp2_src_pipe_attach failed, ret=%d\n",
					__func__, ret);
				goto terminate_source;
			}
			source_info->ref_count++;
			pipe_info->source_info = source_info;
			MPQ_DVB_DBG_PRINT(
				"%s: tspp2_src_pipe_attach input pipe, new source ref. count=%u\n",
				__func__, source_info->ref_count);
		}
	}

	return 0;

terminate_source:
	if (!source_info->ref_count)
		mpq_dmx_tspp2_terminate_source(source_info);
end:
	return ret;
}

static int mpq_dmx_tspp2_close_source(struct source_info *source_info)
{
	int ret = 0;

	if (!source_info->ref_count)
		ret = mpq_dmx_tspp2_terminate_source(source_info);

	return ret;
}

static void mpq_dmx_tspp2_update_pipe_stats(struct pipe_info *pipe_info)
{
	struct timespec curr_time;
	u32 delta_time_ms;

	curr_time = current_kernel_time();
	if (unlikely(!pipe_info->hw_notif_count)) {
		pipe_info->hw_notif_last_time = curr_time;
		pipe_info->hw_notif_count++;
		return;
	}

	/* Calculate time-delta between notifications */
	delta_time_ms = mpq_dmx_calc_time_delta(&curr_time,
			&pipe_info->hw_notif_last_time);
	if (delta_time_ms) {
		pipe_info->hw_notif_last_time = curr_time;
		pipe_info->hw_notif_rate_hz =
			(pipe_info->hw_notif_count * 1000) /
			delta_time_ms;
	}
	pipe_info->hw_notif_count++;
}

static int mpq_dmx_tspp2_queue_pipe_handler(struct pipe_info *pipe_info,
	enum mpq_dmx_tspp2_pipe_event event)
{
	struct source_info *src = pipe_info->source_info;
	struct pipe_work_queue *queue = &pipe_info->work_queue;
	struct pipe_work *pipe_work;
	unsigned long flags;

	/* Try to merge data events into 1 pipe work object */
	spin_lock_irqsave(&queue->lock, flags);
	if (!list_empty(&queue->work_list)) {
		pipe_work = list_first_entry(&queue->work_list,
			struct pipe_work, next);
		if (event == PIPE_DATA_EVENT &&
			pipe_work->event == PIPE_DATA_EVENT &&
			pipe_work->event_count &&
			pipe_work->pipe_info == pipe_info &&
			pipe_work->session_id == pipe_info->session_id) {
			pipe_work->event_count++;
			spin_unlock_irqrestore(&queue->lock, flags);
			wake_up_all(&src->demux_src.wait_queue);
			return 0;
		}
	}
	spin_unlock_irqrestore(&queue->lock, flags);

	pipe_work = pipe_work_queue_allocate(&pipe_info->work_queue);
	if (pipe_work == NULL) {
		int pipe_idx = (pipe_info - mpq_dmx_tspp2_info.pipes) /
			sizeof(*pipe_info);
		MPQ_DVB_ERR_PRINT(
			"%s: Cannot allocate pipe work for pipe %d, type %d\n",
			__func__, pipe_idx, pipe_info->type);
		return -ENOSPC;
	}

	pipe_work->pipe_info = pipe_info;
	pipe_work->event = event;
	pipe_work->session_id = pipe_info->session_id;
	pipe_work->event_count = 1;

	pipe_work_queue_push(&pipe_info->work_queue, pipe_work);
	wake_up_all(&src->demux_src.wait_queue);

	return 0;
}

/**
 * Event callback function from SPS driver for output (producer) pipes.
 *
 * @notify: event's information
 */
static void mpq_dmx_sps_producer_cb(struct sps_event_notify *notify)
{
	struct pipe_info *pipe_info = notify->user;
	int ret;

	if (unlikely(pipe_info == NULL))
		return;

	mpq_dmx_tspp2_update_pipe_stats(pipe_info);

	if (notify->event_id != SPS_EVENT_EOT) {
		MPQ_DVB_ERR_PRINT(
			"%s: unexpected sps event id=%d (expected=%d)\n",
			__func__, notify->event_id, SPS_EVENT_EOT);
		return;
	}

	/* Schedule a new work to relevant pipe workqueue */
	ret = mpq_dmx_tspp2_queue_pipe_handler(pipe_info, PIPE_DATA_EVENT);
	if (ret)
		pipe_info->hw_missed_notif++;
}

/**
 * Event callback function from SPS driver for input (consumer) pipes.
 *
 * @notify: event's information
 */
static void mpq_dmx_sps_consumer_cb(struct sps_event_notify *notify)
{
	struct pipe_info *pipe_info = notify->user;
	struct source_info *source_info;

	MPQ_DVB_DBG_PRINT("%s: Notification event id=%d\n",
		__func__, notify->event_id);

	if (unlikely(pipe_info == NULL))
		return;

	mpq_dmx_tspp2_update_pipe_stats(pipe_info);

	source_info = pipe_info->source_info;
	if (unlikely(source_info == NULL))
		return;

	complete(&source_info->completion);
}

/**
 * Polling timer callback. Used to poll for section
 * and recording pipes.
 */
static void mpq_dmx_timer_cb(unsigned long param)
{
	int i;
	struct pipe_info *pipe_info;
	struct pipe_work *pipe_work;
	struct source_info *source_info;

	MPQ_DVB_DBG_PRINT("%s: entry\n", __func__);

	for (i = 0; i < TSPP2_NUM_PIPES; i++) {
		pipe_info = &mpq_dmx_tspp2_info.pipes[i];

		spin_lock(&pipe_info->lock);
		if (!pipe_info->ref_count ||
			(pipe_info->type != CLEAR_SECTION_PIPE &&
			pipe_info->type != SCRAMBLED_SECTION_PIPE &&
			pipe_info->type != REC_PIPE)) {
			spin_unlock(&pipe_info->lock);
			continue;
		}

		source_info = pipe_info->source_info;
		if (source_info == NULL) {
			MPQ_DVB_ERR_PRINT("%s: invalid source\n", __func__);
			spin_unlock(&pipe_info->lock);
			continue;
		}

		/* Schedule a new work to relevant source workqueue */
		pipe_work = pipe_work_queue_allocate(
			&pipe_info->work_queue);
		if (pipe_work != NULL) {
			pipe_work->session_id = pipe_info->session_id;
			pipe_work->pipe_info = pipe_info;
			pipe_work->event = PIPE_DATA_EVENT;
			pipe_work->event_count = 1;
			pipe_work_queue_push(&pipe_info->work_queue, pipe_work);
			wake_up_all(&source_info->demux_src.wait_queue);
		} else {
			MPQ_DVB_ERR_PRINT(
				"%s: Cannot allocate pipe work\n", __func__);
		}
		spin_unlock(&pipe_info->lock);

	}

	mod_timer(&mpq_dmx_tspp2_info.polling_timer.handle, jiffies +
		msecs_to_jiffies(TSPP2_DMX_POLL_TIMER_INTERVAL_MSEC));
}

/**
 * Starts polling timer if not already started.
 */
static void mpq_dmx_start_polling_timer(void)
{
	if (!mpq_dmx_tspp2_info.polling_timer.ref_count) {
		MPQ_DVB_DBG_PRINT("%s: Starting the timer\n", __func__);
		mod_timer(&mpq_dmx_tspp2_info.polling_timer.handle, jiffies +
			msecs_to_jiffies(TSPP2_DMX_POLL_TIMER_INTERVAL_MSEC));
	}

	mpq_dmx_tspp2_info.polling_timer.ref_count++;
	MPQ_DVB_DBG_PRINT("%s: ref_count=%d\n",
		__func__, mpq_dmx_tspp2_info.polling_timer.ref_count);
}

/**
 * Stop polling timer. The timer is stoped only
 * when all users asked to stop the timer.
 */
static void mpq_dmx_stop_polling_timer(void)
{
	if (mpq_dmx_tspp2_info.polling_timer.ref_count) {
		mpq_dmx_tspp2_info.polling_timer.ref_count--;
		MPQ_DVB_DBG_PRINT("%s: ref_count=%d\n",
				__func__,
				mpq_dmx_tspp2_info.polling_timer.ref_count);
		if (!mpq_dmx_tspp2_info.polling_timer.ref_count) {
			MPQ_DVB_DBG_PRINT("%s: Stopping the timer\n", __func__);
			del_timer_sync(
				&mpq_dmx_tspp2_info.polling_timer.handle);
		}
	}
}

/**
 * Calculates the gap between write and read offsets in a given buffer.
 *
 * @write_offset: write offset
 * @read_offset: read offset
 * @buffer_size: buffer size
 *
 * Function returns (write_offset - read_offset) while taking
 * wrap-around into account.
 */
static inline size_t mpq_dmx_calc_fullness(u32 write_offset,
	u32 read_offset,
	u32 buffer_size)
{
	return (write_offset >= read_offset) ?
		(write_offset - read_offset) :
		buffer_size - (read_offset - write_offset);
}

/**
 * Releases data from pipe output buffer through SPS.
 *
 * @pipe_info: the pipe to release its descriptors
 * @data_length: Length of data to release
 *
 * Return  0 on success, error status otherwise
 *
 * The function updates the read-offset based on the
 * data length, and then releases respective number
 * of descriptors based respectively.
 *
 * In case the specific pipe works in polling mode,
 * the descriptor is first read from the BAM and only
 * then it is released back, in other cases it is assumed
 * BAM descriptors were already received from the BAM.
 */
static int mpq_dmx_release_data(struct pipe_info *pipe_info, u32 data_length)
{
	struct sps_iovec desc;
	u32 desc_num;
	int ret;

	pipe_info->tspp_read_offset += data_length;
	if (pipe_info->tspp_read_offset >= pipe_info->buffer.size)
		pipe_info->tspp_read_offset -= pipe_info->buffer.size;

	desc_num = mpq_dmx_calc_fullness(pipe_info->tspp_read_offset,
			pipe_info->bam_read_offset, pipe_info->buffer.size);
	desc_num /= pipe_info->pipe_cfg.sps_cfg.descriptor_size;

	/*
	 * In case pipe is completely full and we release all the data,
	 * desc_num will be 0 if calculated according to pipe offsets.
	 * So if this is the case calculate according to length of data to
	 * release.
	 */
	if (desc_num == 0)
		desc_num = data_length /
			pipe_info->pipe_cfg.sps_cfg.descriptor_size;

	while (desc_num) {
		/*
		 * Pipes with the SPS_O_HYBRID option set (recording pipes,
		 * the clear section pipe and video payload pipe) are pipes
		 * that are polled directly for data so their descriptors were
		 * not yet read; read them now, and put them back.
		 */
		if (pipe_info->pipe_cfg.sps_cfg.setting & SPS_O_HYBRID) {
			ret = tspp2_pipe_descriptor_get(pipe_info->handle,
				&desc);
			if (ret) {
				MPQ_DVB_ERR_PRINT(
					"%s: tspp2_pipe_descriptor_get failed, ret=%d\n",
					__func__, ret);
				break;
			}

			if (!desc.size) {
				MPQ_DVB_DBG_PRINT(
					"%s: tspp2_pipe_descriptor_get failed, 0 descriptor size\n",
					__func__);
				break;
			}
		} else {
			/*
			 * For other pipe types, descriptor were already read.
			 * Put descriptors back with full descriptor size.
			 */
			desc.addr = pipe_info->buffer.iova +
				pipe_info->bam_read_offset;
			desc.size = pipe_info->pipe_cfg.sps_cfg.descriptor_size;
			desc.flags =
				pipe_info->pipe_cfg.sps_cfg.descriptor_flags;
		}

		ret = tspp2_pipe_descriptor_put(pipe_info->handle, desc.addr,
			desc.size, desc.flags);
		if (ret) {
			MPQ_DVB_ERR_PRINT(
				"%s: tspp2_pipe_descriptor_put failed, ret=%d\n",
				__func__, ret);
			break;
		}
		pipe_info->overflow = 0;
		desc_num--;
		pipe_info->bam_read_offset += desc.size;
		if (pipe_info->bam_read_offset >= pipe_info->buffer.size)
			pipe_info->bam_read_offset -= pipe_info->buffer.size;
	}

	return ret;
}

/**
 * Translate TSPP virtual address to kernel-mapped address.
 *
 * @pipe_info: the pipe with the buffer used to translate the addresses
 * @tspp_address: virtual TSPP address to be translated
 *
 * Return  0 in case of failure, kernel mapped address otherwise
 */
static inline u8 *mpq_dmx_get_kernel_addr(struct pipe_info *pipe_info,
					unsigned long tspp_address)
{
	u8 *kernel_address;

	if (tspp_address < pipe_info->buffer.iova) {
		MPQ_DVB_ERR_PRINT("%s: Invalid TSPP address 0x%0x < 0x%0x\n",
			__func__, (u32)tspp_address,
			pipe_info->buffer.iova);
		return 0;
	}

	if (!pipe_info->buffer.mem) {
		MPQ_DVB_ERR_PRINT("%s: Buffer is not mapped to kernel mem\n",
			__func__);
		return 0;
	}

	kernel_address = (u8 *)pipe_info->buffer.mem;
	kernel_address += (tspp_address - pipe_info->buffer.iova);

	return kernel_address;
}

static void mpq_dmx_tspp2_check_pipe_overflow(struct pipe_info *pipe_info)
{
	struct dmx_data_ready data;
	struct dvb_demux_feed *feed =
		pipe_info->parent->mpq_feed->dvb_demux_feed;

	if (feed->demux->playback_mode == DMX_PB_MODE_PUSH &&
		dvb_ringbuffer_free(feed->feed.ts.buffer.ringbuff) == 0 &&
		!pipe_info->overflow) {
		/* Output buffer is completely full, report overflow */
		MPQ_DVB_ERR_PRINT(
			"%s: pipe overflow (type=%d, pid=%u) overflow (pipe: bam=%u, rd=%u wr=%u)!\n",
			__func__, pipe_info->type, feed->pid,
			pipe_info->bam_read_offset, pipe_info->tspp_read_offset,
			pipe_info->tspp_write_offset);
		data.status = DMX_OVERRUN_ERROR;
		data.data_length = 0;
		feed->data_ready_cb.ts(&feed->feed.ts, &data);
		pipe_info->overflow = 1;
	}
}

/**
 * mpq_dmx_tspp2_pcr_pipe_handler() - Handler for PCR pipe notifications
 *
 * @pipe_info:		pipe_info for the PCR pipe
 * @event:		Notification event type
 *
 * Return error status
 */
static int mpq_dmx_tspp2_pcr_pipe_handler(struct pipe_info *pipe_info,
	enum mpq_dmx_tspp2_pipe_event event)
{
	int ret;
	u64 pcr;
	u64 stc;
	int disc_indicator;
	struct sps_iovec iovec;
	u8 *data_buffer;
	struct dmx_data_ready data;
	struct dvb_demux_feed *feed;

	feed = pipe_info->parent->mpq_feed->dvb_demux_feed;
	if (unlikely(!feed)) {
		MPQ_DVB_ERR_PRINT("%s: invalid feed!\n", __func__);
		return -EINVAL;
	}

	/* Read all descriptors */
	while (1) {
		ret = tspp2_pipe_descriptor_get(pipe_info->handle, &iovec);
		if (ret || !iovec.size) {
			MPQ_DVB_DBG_PRINT(
				"%s: tspp2_pipe_descriptor_get failed %d\n",
				__func__, ret);
			break;
		}

		data_buffer = mpq_dmx_get_kernel_addr(pipe_info,
						iovec.addr);
		if (unlikely(!data_buffer)) {
			MPQ_DVB_ERR_PRINT(
				"%s: mpq_dmx_get_kernel_addr failed\n",
				__func__);
			tspp2_pipe_descriptor_put(pipe_info->handle,
				iovec.addr, iovec.size, SPS_IOVEC_FLAG_INT);
			continue;
		}

		if (unlikely(iovec.size < TSPP2_DMX_SPS_PCR_DESC_SIZE)) {
			MPQ_DVB_ERR_PRINT(
				"%s: Invalid size of PCR descriptor %d\n",
				__func__, iovec.size);
			tspp2_pipe_descriptor_put(pipe_info->handle,
				iovec.addr, iovec.size, SPS_IOVEC_FLAG_INT);
			continue;
		}

		ret = mpq_dmx_extract_pcr_and_dci(data_buffer,
				&pcr, &disc_indicator);

		/* 7 bytes STC@27MHz at end of the packet */
		stc = mpq_dmx_tspp2_get_stc(
			&data_buffer[TS_PACKET_SIZE], 7);

		/* can re-queue the buffer now */
		ret = tspp2_pipe_descriptor_put(pipe_info->handle,
			iovec.addr, iovec.size, SPS_IOVEC_FLAG_INT);
		if (unlikely(ret))
			MPQ_DVB_ERR_PRINT(
				"%s: tspp2_pipe_descriptor_put failed %d\n",
				__func__, ret);

		data.data_length = 0;
		data.pcr.pcr = pcr;
		data.pcr.stc = stc;
		data.pcr.disc_indicator_set = disc_indicator;
		data.status = DMX_OK_PCR;

		feed->data_ready_cb.ts(&feed->feed.ts, &data);

		pipe_info->tspp_write_offset += iovec.size;
		if (pipe_info->tspp_write_offset >=
			pipe_info->buffer.size)
			pipe_info->tspp_write_offset -=
			pipe_info->buffer.size;

		pipe_info->tspp_read_offset =
			pipe_info->tspp_write_offset;
		pipe_info->bam_read_offset =
			pipe_info->tspp_write_offset;
	}

	if (event == PIPE_EOS_EVENT) {
		data.status = DMX_OK_EOS;
		data.data_length = 0;
		feed->data_ready_cb.ts(&feed->feed.ts, &data);
	}

	return 0;
}

/*
 * Handles the details of processing a TS packet of a clear section
 *
 * @feed: dvb demux feed with matching pid of this ts packet
 * @buf:  the ts packet buffer, assumed to always contain 188 bytes
 *
 * Return 1 if packet was processed, 0 otherwise
 */
static int mpq_dmx_tspp2_process_clear_section_packet(
	struct dvb_demux_feed *feed, u8 *buf)
{
	struct dvb_demux_filter *f;
	int ret;

	if (feed->demux->playback_mode == DMX_PB_MODE_PULL) {
		/*
		 * Check that all the section filters associated with this feed
		 * have enough free space in the final output buffer &
		 * event queue, but don't block if there isn't.
		 */
		f = feed->filter;
		while (f && feed->feed.sec.is_filtering) {
			ret = feed->demux->buffer_ctrl.sec(&f->filter,
				feed->feed.sec.tsfeedp + 188, 0);
			if (ret)
				return 0;
			f = f->next;
		}
	}

	/* Demux spin lock was already locked at this point */
	if (dvb_dmx_swfilter_section_packet(feed, buf, 0) < 0)
		feed->feed.sec.seclen = feed->feed.sec.secbufp = 0;

	return 1;
}

static void mpq_dmx_tspp2_notify_section_eos(struct dvb_demux *dvb_demux)
{
	struct dvb_demux_feed *feed;
	struct dmx_data_ready data;
	struct dvb_demux_filter *f;

	data.status = DMX_OK_EOS;
	data.data_length = 0;

	/* Notify EOS event in every section filter */
	spin_lock(&dvb_demux->lock);
	list_for_each_entry(feed, &dvb_demux->feed_list, list_head) {
		if (feed->type != DMX_TYPE_SEC)
			continue;

		f = feed->filter;
		while (f && feed->feed.sec.is_filtering) {
			feed->data_ready_cb.sec(&f->filter, &data);
			f = f->next;
		}
	}
	spin_unlock(&dvb_demux->lock);
}

/**
 * mpq_dmx_tspp2_section_pipe_handler() - Handler for section pipe polling
 *
 * @pipe_info:		pipe_info for the section pipe
 * @event:		Notification event type
 *
 * Return error status
 */
static int mpq_dmx_tspp2_section_pipe_handler(struct pipe_info *pipe_info,
	enum mpq_dmx_tspp2_pipe_event event)
{
	int i;
	int ret;
	int num_packets;
	int should_stall = 0;
	u8 *curr_pkt;
	u16 curr_pid;
	ssize_t data_size;
	size_t packet_offset;
	u32 tspp_last_addr = 0;
	struct dvb_demux *dvb_demux =
		&pipe_info->source_info->demux_src.mpq_demux->demux;
	struct dvb_demux_feed *feed;

	tspp2_pipe_last_address_used_get(pipe_info->handle, &tspp_last_addr);
	data_size = mpq_dmx_tspp2_calc_pipe_data(pipe_info, tspp_last_addr);
	if (data_size) {
		pipe_info->tspp_write_offset += data_size;
		if (pipe_info->tspp_write_offset >= pipe_info->buffer.size)
			pipe_info->tspp_write_offset -= pipe_info->buffer.size;
	}
	data_size = mpq_dmx_calc_fullness(pipe_info->tspp_write_offset,
		pipe_info->tspp_read_offset, pipe_info->buffer.size);
	if (data_size == 0)
		return 0;

	/* Warn if buffer is near overflow, which should never happen */
	if (data_size > TSPP2_DMX_SECTION_BUFFER_THRESHOLD)
		MPQ_DVB_WARN_PRINT(
			"%s: Section buffer is over threshold (size=%u > threshold=%u)\n",
			__func__, data_size,
			TSPP2_DMX_SECTION_BUFFER_THRESHOLD);

	num_packets = data_size / TSPP2_DMX_SPS_SECTION_DESC_SIZE;

	/*
	 * Processing of the clear sections pipe occurs 1 TS packet at a time
	 * since it may contain TS packets with different PIDs.
	 * When stalling is needed for pull mode, processing is stopped without
	 * releasing all the descriptors, so that at a certain point the TSPP2
	 * will stall because there are no free descriptors.
	 */
	spin_lock(&dvb_demux->lock);
	for (i = 0; i < num_packets && !should_stall;) {
		packet_offset = pipe_info->tspp_read_offset +
			i * TSPP2_DMX_SPS_SECTION_DESC_SIZE;
		if (packet_offset >= pipe_info->buffer.size)
			packet_offset -= pipe_info->buffer.size;

		curr_pkt = pipe_info->buffer.mem + packet_offset;
		curr_pid = ts_pid(curr_pkt);

		list_for_each_entry(feed, &dvb_demux->feed_list, list_head) {
			if (feed->pid != curr_pid ||
				feed->type != DMX_TYPE_SEC ||
				feed->state != DMX_STATE_GO)
				continue;

			ret = mpq_dmx_tspp2_process_clear_section_packet(feed,
				curr_pkt);

			if (ret == 0) {
				should_stall = 1;
				break;
			}
		}
		if (!should_stall)
			i++;
	}
	spin_unlock(&dvb_demux->lock);

	pipe_info->tspp_last_addr = tspp_last_addr;

	/* Release the data for the packets that were processed */
	ret = mpq_dmx_release_data(pipe_info,
		i * TSPP2_DMX_SPS_SECTION_DESC_SIZE);

	if (event == PIPE_EOS_EVENT && should_stall)
		pipe_info->eos_pending = 1;
	else if (event == PIPE_EOS_EVENT ||
		(pipe_info->eos_pending && !should_stall))
		mpq_dmx_tspp2_notify_section_eos(dvb_demux);

	return ret;
}

static int mpq_dmx_tspp2_process_full_pes_desc(struct pipe_info *pipe_info,
	struct dvb_demux_feed *feed, struct sps_iovec *iovec)
{
	u8 pes_status;
	u8 *data_buffer = NULL;
	struct dmx_data_ready data;
	int ret;

	data_buffer = mpq_dmx_get_kernel_addr(pipe_info, iovec->addr);
	if (unlikely(!data_buffer)) {
		/* Should NEVER happen! */
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_dmx_get_kernel_addr failed\n",
			__func__);
		return -EFAULT;
	}

	/*
	 * Extract STC value for this PES:
	 * Descriptor data starts with 8 bytes of STC:
	 * 7 bytes STC@27MHz and 1 zero byte for padding.
	 */
	if (feed->peslen < PES_STC_FIELD_LENGTH) {
		if (iovec->size < PES_STC_FIELD_LENGTH) {
			/*
			 * Descriptor too small to even hold STC info,
			 * report this descriptor as an empty PES.
			 */
			MPQ_DVB_DBG_PRINT(
				"%s: descriptor size %d is too small (peslen=%d)\n",
				__func__, iovec->size, feed->peslen);

			data.status = DMX_OK;
			data.data_length = TSPP2_DMX_SPS_NON_VID_PES_DESC_SIZE;
			feed->data_ready_cb.ts(&feed->feed.ts, &data);

			memset(&data, 0, sizeof(data));
			data.status = DMX_OK_PES_END;
			data.data_length = 0;
			data.pes_end.start_gap =
				TSPP2_DMX_SPS_NON_VID_PES_DESC_SIZE;
			data.pes_end.actual_length = 0;
			data.pes_end.stc = feed->prev_stc;
			ret = mpq_dmx_tspp2_ts_event_check(feed, pipe_info);
			if (ret)
				return ret;
			feed->data_ready_cb.ts(&feed->feed.ts, &data);

			/* Reset accumulated PES length for next iteration */
			feed->peslen = 0;
			return 0;
		}

		feed->prev_stc = mpq_dmx_tspp2_get_stc(&data_buffer[0], 7);
	}

	/*
	 * Report the whole descriptor allocated size to dmxdev so that
	 * DMX_OK_PES_END event total size will be correct.
	 */
	data.status = DMX_OK;
	data.data_length = TSPP2_DMX_SPS_NON_VID_PES_DESC_SIZE;
	feed->data_ready_cb.ts(&feed->feed.ts, &data);

	if (iovec->flags & SPS_IOVEC_FLAG_EOT) {
		/* PES assembly status is the last byte in the descriptor */
		pes_status =
			data_buffer[iovec->size - PES_ASM_STATUS_FIELD_LENGTH];

		/*
		 * EOT descriptor might not have been used completely.
		 * The next PES will begin on the next descriptor and
		 * not immediately following this PES. Need to account
		 * for this gap when PES is reported.
		 */
		feed->peslen += iovec->size;

		data.status = DMX_OK_PES_END;
		data.data_length = 0;
		data.pes_end.start_gap = PES_STC_FIELD_LENGTH;
		data.pes_end.actual_length =
			feed->peslen - PES_STC_FIELD_LENGTH -
			PES_ASM_STATUS_FIELD_LENGTH;
		data.pes_end.stc = feed->prev_stc;
		data.pes_end.disc_indicator_set =
			pes_status & PES_ASM_STATUS_DCI;
		data.pes_end.pes_length_mismatch =
			pes_status & PES_ASM_STATUS_SIZE_MISMATCH;
		/* TSPPv2 does not report the following */
		data.pes_end.tei_counter = 0;
		data.pes_end.cont_err_counter = 0;
		data.pes_end.ts_packets_num = 0;

		ret = mpq_dmx_tspp2_ts_event_check(feed, pipe_info);
		if (ret)
			return ret;
		feed->data_ready_cb.ts(&feed->feed.ts, &data);

		/* Reset accumulated PES length for next iteration */
		feed->peslen = 0;

		return 0;
	}

	/* DESC_DONE case */
	feed->peslen += iovec->size;

	return 0;
}

/**
 * mpq_dmx_tspp2_pes_pipe_handler() - Handler for non-video full PES
 * pipe notifications.
 *
 * @pipe_info:		pipe_info for the section pipe
 * @event:		Notification event type
 *
 * Return error status
 */
static int mpq_dmx_tspp2_pes_pipe_handler(struct pipe_info *pipe_info,
	enum mpq_dmx_tspp2_pipe_event event)
{
	int ret;
	u32 tspp_write_offset = 0;
	u32 tspp_last_addr;
	size_t pes_leftover = 0;
	struct sps_iovec iovec;
	u8 *data_buffer = NULL;
	struct dmx_data_ready data;
	struct dvb_demux_feed *feed;

	feed = pipe_info->parent->mpq_feed->dvb_demux_feed;
	if (unlikely(!feed)) {
		MPQ_DVB_ERR_PRINT("%s: invalid feed!\n", __func__);
		return -EINVAL;
	}

	/*
	 * Read pending descriptors (DESC_DONE and EOT).
	 * In case PES is very short and fits in 1 descriptor, only EOT will
	 * be received.
	 */
	while (1) {
		ret = tspp2_pipe_descriptor_get(pipe_info->handle, &iovec);
		if (ret) {
			/* should NEVER happen! */
			MPQ_DVB_ERR_PRINT(
				"%s: tspp2_pipe_descriptor_get failed %d\n",
				__func__, ret);
			return -EINVAL;
		}

		/* No more descriptors */
		if (!iovec.size)
			break;

		pipe_info->tspp_write_offset +=
			TSPP2_DMX_SPS_NON_VID_PES_DESC_SIZE;
		if (pipe_info->tspp_write_offset >= pipe_info->buffer.size)
			pipe_info->tspp_write_offset -= pipe_info->buffer.size;

		ret = mpq_dmx_tspp2_process_full_pes_desc(pipe_info, feed,
			&iovec);
		if (ret) {
			MPQ_DVB_DBG_PRINT(
				"%s: mpq_dmx_tspp2_process_full_pes_desc failed, ret=%d\n",
				__func__, ret);
			return ret;
		}

		if (iovec.flags & SPS_IOVEC_FLAG_EOT)
			break;
	}

	if (event == PIPE_EOS_EVENT) {
		/*
		 * There are 3 cases for End-of-Stream:
		 * 1. No new data so just need to notify on end of partial PES
		 *    consisting of previous DESC_DONE descriptors.
		 *    (feed->peslen > 0, pes_leftover = 0)
		 * 2. New PES begins in this partial descriptor with stc
		 *    (feed->peslen = 0, pes_leftover < 256)
		 * 3. New PES began is a previous descriptors, stc is valid
		 *    (feed->peslen > 0, pes_leftover < 256)
		 */
		tspp2_pipe_last_address_used_get(pipe_info->handle,
			&tspp_last_addr);

		if (tspp_last_addr) {
			tspp_write_offset = mpq_dmx_tspp2_addr_to_offset(
				pipe_info, tspp_last_addr);
			pes_leftover = mpq_dmx_calc_fullness(tspp_write_offset,
				pipe_info->tspp_write_offset,
				pipe_info->buffer.size);
		}


		if (feed->peslen < PES_STC_FIELD_LENGTH &&
			pes_leftover < PES_STC_FIELD_LENGTH) {
			/* Insufficient data to report, just report EOS event */
			MPQ_DVB_DBG_PRINT(
				"%s: PES leftover too small = %d bytes\n",
				__func__, pes_leftover);

			data.status = DMX_OK_EOS;
			data.data_length = 0;
			feed->data_ready_cb.ts(&feed->feed.ts, &data);

			return 0;
		}

		if (pes_leftover) {
			MPQ_DVB_DBG_PRINT("%s: PES leftover %d bytes\n",
				__func__, pes_leftover);
			data_buffer = pipe_info->buffer.mem + tspp_write_offset;

			/* Notify there is more data */
			data.status = DMX_OK;
			data.data_length = pes_leftover;
			feed->data_ready_cb.ts(&feed->feed.ts, &data);
		}

		if (feed->peslen < PES_STC_FIELD_LENGTH)
			feed->prev_stc =
				mpq_dmx_tspp2_get_stc(&data_buffer[0], 7);

		feed->peslen += pes_leftover;

		/* Notify PES has ended */
		data.status = DMX_OK_PES_END;
		data.data_length = 0;
		data.pes_end.start_gap = PES_STC_FIELD_LENGTH;
		/* In EOS case there is no PES assembly status byte */
		data.pes_end.actual_length =
			feed->peslen - PES_STC_FIELD_LENGTH;
		data.pes_end.stc = feed->prev_stc;
		data.pes_end.disc_indicator_set = 0;
		data.pes_end.pes_length_mismatch = 0;
		data.pes_end.tei_counter = 0;
		data.pes_end.cont_err_counter = 0;
		data.pes_end.ts_packets_num = 0;
		pipe_info->tspp_write_offset = tspp_write_offset;

		ret = mpq_dmx_tspp2_ts_event_check(feed, pipe_info);
		if (ret)
			return ret;
		feed->data_ready_cb.ts(&feed->feed.ts, &data);

		feed->peslen = 0;

		data.status = DMX_OK_EOS;
		data.data_length = 0;
		feed->data_ready_cb.ts(&feed->feed.ts, &data);
	} else {
		mpq_dmx_tspp2_check_pipe_overflow(pipe_info);
	}

	return 0;
}

/**
 * Parses extra-info provided in a TS packet holding video-PES header
 * when working in separated PES mode.
 *
 * @data_buffer: Buffer holding the TS packet with the extra-info data output
 * from for video PES header. It is assumed the
 * length of this buffer is TSPP SPS_VPES_HEADER_DESC_SIZE
 * @stc: Output of STC attached to the TS packet
 * @start_addr: Output of start-address of the respective PES payload
 * @end_addr: Output of end-address of the respective PES payload
 * @status_flags: Output of the status flags attached to this TS packet
 */
static inline void mpq_dmx_parse_video_header_suffix(u8 *data_buffer,
	int partial_header, u64 *stc, u32 *start_addr,
	u32 *end_addr, u8 *status_flags)
{
	*stc = mpq_dmx_tspp2_get_stc(&data_buffer[VPES_HEADER_STC_OFFSET], 7);

	*start_addr = data_buffer[VPES_HEADER_SA_OFFSET] << 24;
	*start_addr += data_buffer[VPES_HEADER_SA_OFFSET + 1] << 16;
	*start_addr += data_buffer[VPES_HEADER_SA_OFFSET + 2] << 8;
	*start_addr += data_buffer[VPES_HEADER_SA_OFFSET + 3];

	if (!partial_header) {
		*end_addr = data_buffer[VPES_HEADER_EA_OFFSET] << 24;
		*end_addr += data_buffer[VPES_HEADER_EA_OFFSET + 1] << 16;
		*end_addr += data_buffer[VPES_HEADER_EA_OFFSET + 2] << 8;
		*end_addr += data_buffer[VPES_HEADER_EA_OFFSET + 3];

		*status_flags = data_buffer[VPES_HEADER_STATUS_OFFSET] << 24;
	}
}

/**
 * mpq_dmx_tspp2_process_video_headers -
 *
 * @buffer:		Buffer holding the video PES header data
 * @partial_header:	Specifies if video PES headers are partial
 *			(such in the case of EOS)
 * @header_pipe:	Header pipe info
 * @payload_pipe:	Payload pipe info
 *
 * Returns 1 if video PES was detected, 0 if no video PES detected yet,
 * and error code otherwise.
 */
static int mpq_dmx_tspp2_process_video_headers(struct mpq_feed *mpq_feed,
	u8 *buffer, int partial_header,
	struct pipe_info *header_pipe, struct pipe_info *payload_pipe)
{
	int ret;
	u64 stc;
	u8 status_flags;
	u32 ts_payload_offset;
	u32 pes_payload_ea = 0;
	u32 pes_payload_sa;
	int bytes_avail;
	struct dvb_demux_feed *feed;
	struct pes_packet_header *pes_header;
	struct mpq_video_feed_info *feed_data;
	struct ts_packet_header *ts_header;
	struct mpq_streambuffer_packet_header packet;
	struct mpq_adapter_video_meta_data meta_data;
	struct mpq_streambuffer *stream_buffer;
	struct dmx_pts_dts_info *pts_dts_info;
	struct dmx_data_ready data;

	feed = mpq_feed->dvb_demux_feed;
	feed_data = &mpq_feed->video_info;
	pes_header = &feed_data->pes_header;
	stream_buffer = feed_data->video_buffer;

	if (stream_buffer == NULL) {
		MPQ_DVB_NOTICE_PRINT(
			"%s: PES detected but video_buffer was released\n",
			__func__);
		return 1;
	}

	mpq_dmx_parse_video_header_suffix(buffer, partial_header,
		&stc, &pes_payload_sa, &pes_payload_ea,
		&status_flags);
	if (status_flags)
		MPQ_DVB_DBG_PRINT(
			"%s: status_flags=0x%x, sa=%u, ea=%u\n", __func__,
			status_flags, pes_payload_sa, pes_payload_ea);

	if (pes_payload_sa == ULONG_MAX || pes_payload_ea == ULONG_MAX) {
		/*
		 * Several video headers may indicate overflow so set an
		 * overflow error indication in the mpq_streambuffer to report
		 * overflow just once.
		 * The overflow error indication will be reset when
		 * mpq_streambuffer is flushed, or when the next non-overflow
		 * video header is read.
		 */
		if (!stream_buffer->packet_data.error) {
			stream_buffer->packet_data.error = -EOVERFLOW;
			MPQ_DVB_DBG_PRINT(
				"%s: S-PES payload overflow (sa=0x%x, ea=0x%x, sts=0x%x)\n",
				__func__, pes_payload_sa, pes_payload_ea,
				status_flags);
			data.status = DMX_OVERRUN_ERROR;
			data.data_length = 0;
			feed->data_ready_cb.ts(&feed->feed.ts, &data);
			return 1;
		}
	} else if (stream_buffer->packet_data.error == -EOVERFLOW) {
		stream_buffer->packet_data.error = 0;
	}

	ts_header = (struct ts_packet_header *)buffer;

	if (ts_header->payload_unit_start_indicator) {
		feed->peslen = 0;
		feed_data->pes_header_offset = 0;
		feed_data->pes_header_left_bytes =
			PES_MANDATORY_FIELDS_LEN;
		/*
		 * PES header can span over more than 1 TS
		 * packet, so save the STC of the TS packet with
		 * the PUSI flag set.
		 */
		feed->prev_stc = stc;
	}

	if (ts_header->adaptation_field_control == 0 ||
		ts_header->adaptation_field_control == 2)
		return 0; /* TS packet without payload */
	ts_payload_offset = sizeof(struct ts_packet_header);

	/*
	 * Skip adaptation field if exists.
	 * Save discontinuity indicator (dci) if exists.
	 */
	if (ts_header->adaptation_field_control == 3) {
		struct ts_adaptation_field *af;
		af =  (struct ts_adaptation_field *)
			(buffer + ts_payload_offset);
		ts_payload_offset +=
			buffer[ts_payload_offset] + 1;
	}

	bytes_avail = TS_PACKET_SIZE - ts_payload_offset;

	/* Get the mandatory fields of the video PES header */
	mpq_dmx_parse_mandatory_pes_header(feed,
		feed_data, pes_header, buffer,
		&ts_payload_offset, &bytes_avail);

	/* Get the PTS/DTS fields of the video PES header */
	mpq_dmx_parse_remaining_pes_header(feed,
		feed_data, pes_header, buffer,
		&ts_payload_offset, &bytes_avail);

	/*
	 * PES header continues on the next TS packet which should also
	 * be parsed before reporting the PES.
	 */
	if (pes_payload_sa == 0)
		return 0;

	/*
	 * Reached last packet that holds PES header.
	 * Verify we have received at least the mandatory
	 * fields within the PES header.
	 */
	if (unlikely(feed_data->pes_header_offset < PES_MANDATORY_FIELDS_LEN)) {
		MPQ_DVB_ERR_PRINT(
			"%s: Invalid header size %d\n",
			__func__, feed_data->pes_header_offset);
		return -EINVAL;
	}

	meta_data.packet_type = DMX_PES_PACKET;
	pts_dts_info = &meta_data.info.pes.pts_dts_info;
	mpq_dmx_save_pts_dts(feed_data);
	mpq_dmx_write_pts_dts(feed_data, pts_dts_info);
	meta_data.info.pes.stc = feed->prev_stc;

	ret = mpq_streambuffer_get_buffer_handle(stream_buffer,
		0, &packet.raw_data_handle);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_streambuffer_get_buffer_handle failed, ret=%d\n",
			__func__, ret);
		return ret;
	}

	packet.raw_data_offset = pes_payload_sa - payload_pipe->buffer.iova;

	if (partial_header) {
		tspp2_pipe_last_address_used_get(payload_pipe->handle,
			&pes_payload_ea);
	}

	packet.raw_data_len =
		mpq_dmx_calc_fullness(pes_payload_ea,
			pes_payload_sa, payload_pipe->buffer.size);
	packet.raw_data_len++;
	packet.user_data_len = sizeof(meta_data);

	payload_pipe->tspp_write_offset =
		mpq_dmx_tspp2_addr_to_offset(payload_pipe, pes_payload_ea);

	ret = mpq_streambuffer_data_write_deposit(stream_buffer,
		packet.raw_data_len);
	if (ret) {
		MPQ_DVB_DBG_PRINT(
			"%s: mpq_streambuffer_data_write_deposit failed, ret=%d\n",
			__func__, ret);
		return ret;
	}

	ret = mpq_dmx_tspp2_stream_buffer_event_check(feed, header_pipe);
	if (ret) {
		MPQ_DVB_DBG_PRINT(
			"%s: mpq_dmx_tspp2_stream_buffer_event_check failed, ret=%d\n",
			__func__, ret);
		return ret;
	}

	ret = mpq_streambuffer_pkt_write(stream_buffer, &packet,
		(u8 *)&meta_data);
	if (ret < 0) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_streambuffer_pkt_write failed, ret=%d\n",
			__func__, ret);
	} else {
		struct dmx_data_ready data;

		mpq_dmx_update_decoder_stat(mpq_feed);

		data.data_length = 0;
		data.buf.handle = packet.raw_data_handle;

		/*
		 * The following has to succeed when called here,
		 * after packet was written
		 */
		data.buf.cookie = ret;
		data.buf.offset = packet.raw_data_offset;
		data.buf.len = packet.raw_data_len;
		data.buf.pts_exists = pts_dts_info->pts_exist;
		data.buf.pts = pts_dts_info->pts;
		data.buf.dts_exists = pts_dts_info->dts_exist;
		data.buf.dts = pts_dts_info->dts;
		data.buf.tei_counter = 0;
		data.buf.cont_err_counter = 0;
		data.buf.ts_packets_num = 0;
		data.buf.ts_dropped_bytes = 0;
		data.status = DMX_OK_DECODER_BUF;

		MPQ_DVB_DBG_PRINT("%s: cookie=%d\n", __func__, data.buf.cookie);

		ret = mpq_dmx_tspp2_ts_event_check(feed, header_pipe);
		if (!ret)
			feed->data_ready_cb.ts(&feed->feed.ts, &data);
	}

	return ret ? ret : 1;
}

/**
 * mpq_dmx_tspp2_video_pipe_handler() - Handler for video separated PES
 * pipe notifications.
 *
 * @pipe_info:		pipe_info for the section pipe
 * @event:		Notification event type
 *
 * Return error status
 */
static int mpq_dmx_tspp2_video_pipe_handler(struct pipe_info *pipe_info,
	enum mpq_dmx_tspp2_pipe_event event)
{
	int ret;
	u32 tspp_write_offset = 0;
	u32 tspp_last_addr = 0;
	u32 pes_leftover = 0;
	u8 *data_buffer = NULL;
	struct mpq_tspp2_feed *tspp2_feed;
	struct mpq_feed *mpq_feed;
	struct dvb_demux_feed *feed;
	struct sps_iovec iovec;
	struct mpq_video_feed_info *feed_data;
	struct pipe_info *main_pipe;
	struct dmx_data_ready eos_event;

	if (pipe_info->type == VPES_PAYLOAD_PIPE)
		return 0;

	/* Video header pipe. Read descriptors until EOT */
	tspp2_feed = pipe_info->parent;
	if (unlikely(!tspp2_feed)) {
		MPQ_DVB_ERR_PRINT("%s: invalid feed!\n", __func__);
		return -EINVAL;
	}

	main_pipe = tspp2_feed->main_pipe;
	mpq_feed = tspp2_feed->mpq_feed;
	feed = mpq_feed->dvb_demux_feed;
	feed_data = &mpq_feed->video_info;

	if (event == PIPE_EOS_EVENT) {
		tspp2_pipe_last_address_used_get(pipe_info->handle,
			&tspp_last_addr);
		if (tspp_last_addr) {
			tspp_write_offset =
				mpq_dmx_tspp2_addr_to_offset(pipe_info,
					tspp_last_addr);
			pes_leftover = mpq_dmx_calc_fullness(
				tspp_write_offset,
				pipe_info->tspp_write_offset,
				pipe_info->buffer.size);
		}

		MPQ_DVB_DBG_PRINT("%s: VPES EOS - pes header leftover=%d\n",
			__func__, pes_leftover);

		if (pes_leftover) {
			data_buffer = pipe_info->buffer.mem +
				pipe_info->tspp_read_offset;
			ret = mpq_dmx_tspp2_process_video_headers(
				mpq_feed, data_buffer,
				1, /* partial header */
				pipe_info, main_pipe);
			if (ret < 0) {
				if (ret == -ENODEV) {
					MPQ_DVB_DBG_PRINT(
						"%s: header pipe was closed\n",
						__func__);
					return ret;
				}
				MPQ_DVB_ERR_PRINT(
					"%s: mpq_dmx_tspp2_process_video_pes failed, ret=%d\n",
					__func__, ret);
			}
			pipe_info->tspp_write_offset = tspp_write_offset;
			pipe_info->tspp_read_offset = tspp_write_offset;
			pipe_info->bam_read_offset = tspp_write_offset;
		}

		/* Notify EOS event */
		ret = mpq_dmx_decoder_eos_cmd(mpq_feed);
		if (ret < 0) {
			MPQ_DVB_ERR_PRINT(
				"%s: mpq_dmx_tspp2_process_video_pes failed, ret=%d\n",
				__func__, ret);
		}

		eos_event.data_length = 0;
		eos_event.status = DMX_OK_EOS;
		feed->data_ready_cb.ts(&feed->feed.ts, &eos_event);

		return 0;
	}

	/*
	 * PIPE_DATA_EVENT case:
	 * Read exactly one EOT descriptor containing 1 or 2 s-pes headers.
	 */
	ret = tspp2_pipe_descriptor_get(pipe_info->handle, &iovec);
	if (ret) {
		/* should NEVER happen! */
		MPQ_DVB_ERR_PRINT(
			"%s: tspp2_pipe_descriptor_get failed, ret=%d\n",
			__func__, ret);
		return -EINVAL;
	}

	if (!(iovec.flags & SPS_IOVEC_FLAG_EOT)) {
		MPQ_DVB_ERR_PRINT(
			"%s: not EOT descriptor (flags=0x%x)\n",
			__func__, iovec.flags);
		ret = -EINVAL;
		goto put_desc;
	}

	/* Descriptor must contain either 1 or 2 s-pes headers */
	if (iovec.size != VPES_HEADER_DATA_SIZE &&
		iovec.size != 2*VPES_HEADER_DATA_SIZE) {
		MPQ_DVB_DBG_PRINT("%s: invalid descriptor size %d\n",
			__func__, iovec.size);
		ret = -EINVAL;
		goto put_desc;
	}

	data_buffer = mpq_dmx_get_kernel_addr(pipe_info, iovec.addr);
	if (unlikely(!data_buffer)) {
		/* should NEVER happen! */
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_dmx_get_kernel_addr failed (addr=0x%x)\n",
			__func__, iovec.addr);
		/* Do not put back a descriptor with invalid address */
		return -EFAULT;
	}

	ret = mpq_dmx_tspp2_process_video_headers(mpq_feed,
		data_buffer, 0, pipe_info, main_pipe);
	if (ret == 0 && iovec.size > VPES_HEADER_DATA_SIZE) {
		/*
		 * PES header spreads across more than 1 TS packet so
		 * it has two headers that need to be parsed.
		 */
		ret = mpq_dmx_tspp2_process_video_headers(mpq_feed,
			&data_buffer[VPES_HEADER_DATA_SIZE], 0,
			pipe_info, main_pipe);
	}

	if (ret < 0) {
		MPQ_DVB_DBG_PRINT(
			"%s: mpq_dmx_tspp2_process_video_pes failed, ret=%d\n",
			__func__, ret);
		/* Exit handler if filter was stopped */
		if (ret == -ENODEV)
			return ret;
	}

	/* re-queue buffer holding TS packet of PES header */
	ret = tspp2_pipe_descriptor_put(pipe_info->handle, iovec.addr,
		TSPP2_DMX_SPS_VPES_HEADER_DESC_SIZE, 0);
	if (unlikely(ret))
		MPQ_DVB_ERR_PRINT(
			"%s: tspp2_pipe_descriptor_put failed %d\n",
			__func__, ret);

	pipe_info->tspp_write_offset += TSPP2_DMX_SPS_VPES_HEADER_DESC_SIZE;
	if (pipe_info->tspp_write_offset >= pipe_info->buffer.size)
		pipe_info->tspp_write_offset -= pipe_info->buffer.size;

	pipe_info->tspp_read_offset = pipe_info->tspp_write_offset;
	pipe_info->bam_read_offset = pipe_info->tspp_write_offset;

	return 0;

put_desc:
	if (tspp2_pipe_descriptor_put(pipe_info->handle, iovec.addr,
		TSPP2_DMX_SPS_VPES_HEADER_DESC_SIZE, 0))
		MPQ_DVB_ERR_PRINT(
			"%s: tspp2_pipe_descriptor_put failed\n", __func__);
	return ret;
}

/**
 * mpq_dmx_tspp2_calc_tsp_num_delta() - Calculates the number of TS packets
 * relative to the chunk start for a matched packet.
 *
 * @match:		offset of the match to calculate
 * @chunk_start:	offset where recording chunk starts
 * @chunk_size:		recording chunk size in bytes
 * @buffer_size:	buffer size in bytes
 * @packet_size:	TS packet size
 *
 * Return number of TS packets relative to the chunk start.
 * Result can be negative, meaning match packet occurred in some previous
 * recording chunk.
 */
static int mpq_dmx_tspp2_calc_tsp_num_delta(
	u32 match, u32 chunk_start, size_t chunk_size, size_t buffer_size,
	size_t packet_size)
{
	int tsp_num = match - chunk_start;
	u32 chunk_end = (chunk_start + chunk_size) % buffer_size;

	if ((chunk_start + chunk_size < buffer_size) &&
		(match > chunk_start + chunk_size) && (match < buffer_size)) {
		tsp_num -= buffer_size;
		MPQ_DVB_DBG_PRINT(
			"%s: 1: match=%u, CS=%u, CE=%u, CSize=%u, BS=%u, tsp_num=%d\n",
			__func__, match, chunk_start, chunk_end, chunk_size,
			buffer_size, tsp_num);
	} else if ((chunk_start + chunk_size) >= buffer_size &&
		match < chunk_end) {
		tsp_num += buffer_size;
		MPQ_DVB_DBG_PRINT(
			"%s: 2: match=%u, CS=%u, CE=%u, CSize=%u, BS=%u, tsp_num=%d\n",
			__func__, match, chunk_start, chunk_end, chunk_size,
			buffer_size, tsp_num);
	} else {
		MPQ_DVB_DBG_PRINT(
			"%s: 3: match=%u, CS=%u, CE=%u, CSize=%u, BS=%u, tsp_num=%d\n",
			__func__, match, chunk_start, chunk_end, chunk_size,
			buffer_size, tsp_num);
	}

	return tsp_num / (int)packet_size;
}

/**
 * mpq_dmx_tspp2_offset_in_range() - return whether some buffer offset is in
 * the given offsets range, taking wrap-around into consideration.
 *
 * @offset:	offset to check
 * @from:	range "left" boundary
 * @to:		range "right"boundary
 *
 * Return true if offset is in range, false otherwise
 */
static bool mpq_dmx_tspp2_offset_in_range(u32 offset, u32 from, u32 to)
{
	if (from <= to)
		return (offset >= from && offset < to);

	return !(offset >= to && offset < from);
}

/**
 * mpq_dmx_tspp2_match_after_pusi() - check if PUSI offset precedes the
 * TSP match offset in the recording chunk.
 *
 * @pusi:		pusi tsp offset
 * @match:		match tsp offset
 * @chunk_start:	recording chunk start offset
 * @chunk_size:		recording chunk size
 * @buffer_size:	recording buffer size
 *
 * Return true if PUSI offset precedes match offset, false otherwise
 */
static bool mpq_dmx_tspp2_match_after_pusi(u32 pusi, u32 match,
	u32 chunk_start, size_t chunk_size, size_t buffer_size)
{
	u32 end_offset = (chunk_start + chunk_size) % buffer_size;

	if ((chunk_start + chunk_size) >= buffer_size) {
		if ((pusi >= chunk_start && match >= chunk_start) ||
			(pusi < end_offset && match < end_offset))
			return (pusi <= match);

		return (pusi > match);
	}

	return (pusi <= match);
}

/**
 * mpq_dmx_tspp2_process_index_desc() - process one indexing descriptor
 * (descriptor might be partial)
 *
 * @feed:		dvb_demux feed object
 * @rec_pipe:		recording pipe info
 * @idx_desc:		indexing descriptor to process
 * @rec_data_size:	recording chunk size
 * @ts_pkt_size:	TS packet size
 */
static void mpq_dmx_tspp2_process_index_desc(struct dvb_demux_feed *feed,
	struct pipe_info *rec_pipe, struct mpq_tspp2_index_desc *idx_desc,
	size_t rec_data_size, size_t ts_pkt_size)
{
	struct dmx_index_event_info idx_event;
	struct mpq_tspp2_index_table *index_table;
	struct dvb_dmx_video_patterns_results pattern;
	u8 pattern_id;
	u8 table_id;
	int tsp_delta;
	u32 match_tsp_offset;
	u32 pusi_tsp_offset;
	u32 from = rec_pipe->tspp_write_offset;
	u32 to = (rec_pipe->tspp_write_offset + rec_data_size) %
		rec_pipe->buffer.size;

	/* Convert addresses in the indexing desc. from big-endian */
	idx_desc->matched_tsp_addr = be32_to_cpu(idx_desc->matched_tsp_addr);
	idx_desc->pusi_tsp_addr = be32_to_cpu(idx_desc->pusi_tsp_addr);
	idx_desc->last_tsp_addr = 0;	/* unused */

	idx_event.pid = feed->pid;
	idx_event.stc = mpq_dmx_tspp2_get_stc(idx_desc->stc, 7);

	pattern_id = idx_desc->pattern_id & INDEX_DESC_PATTERN_ID_MASK;
	table_id = (idx_desc->pattern_id & INDEX_DESC_TABLE_ID_MASK) >> 5;
	index_table = &mpq_dmx_tspp2_info.index_tables[table_id];
	idx_event.type = index_table->patterns[pattern_id].type;

	MPQ_DVB_DBG_PRINT(
		"%s: Index info: pattern_id=0x%x, pusi=0x%x, match=0x%x\n",
		__func__, idx_desc->pattern_id, idx_desc->pusi_tsp_addr,
		idx_desc->matched_tsp_addr);

	if (idx_desc->matched_tsp_addr &&
		idx_desc->matched_tsp_addr != ULONG_MAX) {
		match_tsp_offset =
			idx_desc->matched_tsp_addr - ts_pkt_size + 1 -
			rec_pipe->buffer.iova;
		pusi_tsp_offset =
			idx_desc->pusi_tsp_addr - rec_pipe->buffer.iova;

		tsp_delta = mpq_dmx_tspp2_calc_tsp_num_delta(
			pusi_tsp_offset, rec_pipe->tspp_write_offset,
			rec_data_size, rec_pipe->buffer.size,
			ts_pkt_size);

		/*
		 * PUSI address and match address are both in the chunk,
		 * but if PUSI address is after the match address then
		 * it is really from previous chunk.
		 */
		if (mpq_dmx_tspp2_offset_in_range(pusi_tsp_offset, from, to)
			&& !mpq_dmx_tspp2_match_after_pusi(pusi_tsp_offset,
				match_tsp_offset, rec_pipe->tspp_write_offset,
				rec_data_size, rec_pipe->buffer.size))
			idx_event.last_pusi_tsp_num =
				feed->rec_info->ts_output_count -
				((rec_pipe->buffer.size / ts_pkt_size) -
				tsp_delta);
		else
			idx_event.last_pusi_tsp_num =
				feed->rec_info->ts_output_count + tsp_delta;

		idx_event.match_tsp_num = feed->rec_info->ts_output_count +
			mpq_dmx_tspp2_calc_tsp_num_delta(
				match_tsp_offset,
				rec_pipe->tspp_write_offset, rec_data_size,
				rec_pipe->buffer.size, ts_pkt_size);
		feed->last_pattern_tsp_num = idx_event.match_tsp_num;

		MPQ_DVB_DBG_PRINT(
			"%s: PUSI tsp num=%llu, Match tsp num=%llu (tsp_delta=%d)\n",
			__func__, idx_event.last_pusi_tsp_num,
			idx_event.match_tsp_num, tsp_delta);

		pattern.info[0].type =
			index_table->patterns[pattern_id].type;
		pattern.info[0].offset = 0;
		pattern.info[0].used_prefix_size = 0;

		dvb_dmx_process_idx_pattern(feed, &pattern, 0, idx_event.stc,
			0, idx_event.match_tsp_num, 0,
			idx_event.last_pusi_tsp_num, 0);
	}
}

/**
 * mpq_dmx_tspp2_index_pipe_handler() - Handler for index pipe notifications
 *
 * @rec_pipe:			pipe_info for the recording payload pipe
 * @index_pipe:			pipe_info for the indexing pipe
 * @ts_packet_size:		Recording TS packet size
 * @event:			Notification event type
 * @rec_data_size:		Recording chunk size
 * @tspp_index_last_addr:	Index pipe last write address sampled
 *
 * Return error status
 */
static int mpq_dmx_tspp2_index_pipe_handler(struct pipe_info *rec_pipe,
	struct pipe_info *index_pipe, size_t ts_packet_size,
	enum mpq_dmx_tspp2_pipe_event event, size_t rec_data_size,
	u32 tspp_index_last_addr)
{
	struct dvb_demux_feed *feed;
	u32 desc_leftover = 0;
	struct mpq_tspp2_feed *tspp2_feed;
	struct mpq_tspp2_index_desc index_desc;
	size_t data_size;

	if (!index_pipe->ref_count) {
		MPQ_DVB_ERR_PRINT(
			"%s: invalid index pipe ref. count\n", __func__);
		return -EINVAL;
	}

	if (index_pipe->type != INDEXING_PIPE) {
		MPQ_DVB_ERR_PRINT(
			"%s: invalid pipe type: expected %d, actual %d!\n",
			__func__, INDEXING_PIPE, index_pipe->type);
		return -EINVAL;
	}

	tspp2_feed = index_pipe->parent;
	feed = tspp2_feed->mpq_feed->dvb_demux_feed;

	/* Calculate new data in indexing pipe */
	data_size = mpq_dmx_tspp2_calc_pipe_data(index_pipe,
		tspp_index_last_addr);
	index_pipe->tspp_last_addr = tspp_index_last_addr;
	index_pipe->tspp_write_offset += data_size;
	if (index_pipe->tspp_write_offset >= index_pipe->buffer.size)
		index_pipe->tspp_write_offset -= index_pipe->buffer.size;

	/*
	 * Calculate total data to process, disregarding leftover from previous
	 * partial descriptor that was processed.
	 */
	if (index_pipe->tspp_read_offset % TSPP2_DMX_SPS_INDEXING_DESC_SIZE) {
		desc_leftover = TSPP2_DMX_SPS_INDEXING_DESC_SIZE -
			index_pipe->tspp_read_offset %
			TSPP2_DMX_SPS_INDEXING_DESC_SIZE;
		if (desc_leftover <= data_size)
			mpq_dmx_release_data(index_pipe, desc_leftover);
		else
			return 0;
	}
	data_size = mpq_dmx_calc_fullness(index_pipe->tspp_write_offset,
		index_pipe->tspp_read_offset, index_pipe->buffer.size);
	if (data_size == 0)
		return 0;

	MPQ_DVB_DBG_PRINT(
		"\n%s: TS output count=%llu, desc_data=%u, desc_leftover=%u\n",
		__func__, feed->rec_info->ts_output_count, data_size,
		desc_leftover);
	MPQ_DVB_DBG_PRINT(
		"%s: Recording chunk: from=%u, to=%u, size=%u\n\n",
		__func__, rec_pipe->tspp_write_offset,
		(rec_pipe->tspp_write_offset + rec_data_size) %
		rec_pipe->buffer.size,
		rec_data_size);

	/*
	 * Loop over the indexing descriptors and process each one.
	 * The last descriptor might be a partial descriptor (24 bytes out of
	 * 28 total) which does not contain the information where the frame/PES
	 * ends, but we don't use this field anyway so we process it too.
	 */
	while (data_size >= TSPP2_DMX_MIN_INDEXING_DESC_SIZE) {
		memcpy(&index_desc,
			index_pipe->buffer.mem + index_pipe->tspp_read_offset,
			sizeof(index_desc));

		mpq_dmx_tspp2_process_index_desc(feed, rec_pipe, &index_desc,
			rec_data_size, ts_packet_size);
		/*
		 * Descriptor was processed - advance the index pipe read offset
		 */
		if (data_size >= TSPP2_DMX_SPS_INDEXING_DESC_SIZE) {
			mpq_dmx_release_data(index_pipe,
				TSPP2_DMX_SPS_INDEXING_DESC_SIZE);
			data_size -= TSPP2_DMX_SPS_INDEXING_DESC_SIZE;
		} else {
			mpq_dmx_release_data(index_pipe, data_size);
			data_size = 0;
		}
	}

	return 0;
}

/**
 * mpq_dmx_tspp2_index() - Perform RAI & PUSI indexing on recorded data.
 * Data polled from the recording pipe is scanned, and RAI and/or PUSI
 * indexing is done for each recording filter feed with indexing enabled,
 * one TS packet at a time.
 *
 * @rec_feed:		dvb_demux_feed object of the recording filter
 * @pipe_info:		pipe_info of the recording pipe
 * @size:		Size of recording data polled
 * @ts_packet_size:	Recording filter TS packet size
 *
 * Return error status
 */
static int mpq_dmx_tspp2_index(struct dvb_demux_feed *rec_feed,
	struct pipe_info *pipe_info, size_t size, size_t ts_packet_size)
{
	int i;
	struct dvb_demux_feed *feed;
	struct dvb_demux *dvb_demux = rec_feed->demux;
	struct dmx_index_event_info idx_event;
	size_t packet_count = rec_feed->rec_info->ts_output_count;
	size_t num_packets = size / ts_packet_size;
	u32 packet_offset;
	u8 *packet;
	u8 *timestamp = NULL;
	u16 pid;

	for (i = 0; i < num_packets; i++) {
		packet_offset = pipe_info->tspp_write_offset +
			i*ts_packet_size;
		if (packet_offset >= pipe_info->buffer.size)
			packet_offset -= pipe_info->buffer.size;
		packet = pipe_info->buffer.mem + packet_offset;

		if (rec_feed->tsp_out_format == DMX_TSP_FORMAT_192_HEAD) {
			timestamp = packet;
			packet += TIMESTAMP_LEN;
		} else if (rec_feed->tsp_out_format == DMX_TSP_FORMAT_192_TAIL)
			timestamp = packet + TS_PACKET_SIZE;

		pid = ts_pid(packet);

		spin_lock(&dvb_demux->lock);
		list_for_each_entry(feed, &dvb_demux->feed_list, list_head) {
			if (feed->state != DMX_STATE_GO ||
				feed->pid != pid ||
				!feed->idx_params.enable ||
				!(feed->idx_params.types &
					(DMX_IDX_PUSI | DMX_IDX_RAI)))
				continue;

			idx_event.pid = pid;
			idx_event.stc = mpq_dmx_tspp2_get_stc(timestamp, 4);
			idx_event.match_tsp_num = packet_count + i;

			/* PUSI indexing */
			if (packet[1] & 0x40) {
				feed->curr_pusi_tsp_num = packet_count + i;
				if (feed->idx_params.types & DMX_IDX_PUSI) {
					idx_event.type = DMX_IDX_PUSI;
					idx_event.last_pusi_tsp_num =
						feed->curr_pusi_tsp_num;
					dvb_demux_push_idx_event(feed,
						&idx_event, 0);
				}
			}

			/*
			 * If we still did not encounter a TS packet with PUSI
			 * indication, we cannot report index entries yet as we
			 * need to provide the TS packet number with PUSI
			 * indication preceding the TS packet pointed by the
			 * reported index entry.
			 */
			if (feed->curr_pusi_tsp_num == (u64)-1)
				continue;

			/* RAI indexing */
			if ((feed->idx_params.types & DMX_IDX_RAI) &&
				(packet[3] & 0x20) &&	/* AF exists? */
				(packet[4] > 0) &&	/* AF len > 0? */
				(packet[5] & 0x40)) {	/* RAI is set? */
				idx_event.type = DMX_IDX_RAI;
				idx_event.last_pusi_tsp_num =
					feed->curr_pusi_tsp_num;
				dvb_demux_push_idx_event(feed, &idx_event, 0);
			}
		}
		spin_unlock(&dvb_demux->lock);
	}

	return 0;
}

/**
 * mpq_dmx_tspp2_rec_pipe_handler() - Handler for recording pipe polling
 *
 * @pipe_info:	pipe_info for the recording payload pipe
 * @event:	Notification event type
 *
 * Return error status
 */
static int mpq_dmx_tspp2_rec_pipe_handler(struct pipe_info *pipe_info,
	enum mpq_dmx_tspp2_pipe_event event)
{
	ssize_t data_size;
	size_t ts_packet_size = TS_PACKET_SIZE;
	size_t num_packets;
	struct dvb_demux_feed *feed;
	u32 tspp_last_addr;
	u32 tspp_index_last_addr;
	struct dmx_data_ready data;
	struct pipe_info *index_pipe = pipe_info->parent->secondary_pipe;
	int ret = 0;

	if (!pipe_info->ref_count || pipe_info->type != REC_PIPE) {
		MPQ_DVB_ERR_PRINT("%s: invalid feed!\n", __func__);
		return -EINVAL;
	}

	feed = pipe_info->parent->mpq_feed->dvb_demux_feed;
	if (unlikely(!feed)) {
		MPQ_DVB_ERR_PRINT("%s: invalid feed!\n", __func__);
		return -EINVAL;
	}

	if (feed->tsp_out_format == DMX_TSP_FORMAT_192_HEAD ||
		feed->tsp_out_format == DMX_TSP_FORMAT_192_TAIL)
		ts_packet_size = 192;

	/*
	 * Sample indexing pipe before sampling the recording pipe.
	 * This ensures indexing data refers to the current recording chunk,
	 * or the previous recording chunk (as we might still miss indexing
	 * descriptor that was written immediately after we sampled the pipe,
	 * which will be processed in the next iteration).
	 */
	if (index_pipe)
		tspp2_pipe_last_address_used_get(index_pipe->handle,
			&tspp_index_last_addr);

	tspp2_pipe_last_address_used_get(pipe_info->handle, &tspp_last_addr);
	data_size = mpq_dmx_tspp2_calc_pipe_data(pipe_info, tspp_last_addr);

	/* Process only complete TS packets */
	data_size = (data_size / ts_packet_size) * ts_packet_size;
	num_packets = data_size / ts_packet_size;

	MPQ_DVB_DBG_PRINT("%s: new data: size=%u, num_pkts=%u\n",
		__func__, data_size, num_packets);
	if (data_size) {
		MPQ_DVB_DBG_PRINT("%s: new %d bytes, notify dmxdev\n",
			__func__, data_size);
		/*
		 * Notify dmx-dev that new data is ready - this might trigger
		 * a recording chunk notification so in pull mode verify
		 * sufficient event queue space
		 */
		data.status = DMX_OK;
		data.data_length = data_size;
		ret = mpq_dmx_tspp2_ts_event_check(feed, pipe_info);
		if (ret)
			return ret;

		feed->data_ready_cb.ts(&feed->feed.ts, &data);

		/* Report overflow if output buffer is completely full */
		mpq_dmx_tspp2_check_pipe_overflow(pipe_info);
	}

	/*
	 * Handle indexing of recorded data even if recording chunk size is 0,
	 * to process any HW indexing data not read in previous iteration.
	 */
	if (feed->rec_info->idx_info.indexing_feeds_num) {
		/* Handle PUSI / RAI indexing */
		mpq_dmx_tspp2_index(feed, pipe_info, data_size, ts_packet_size);

		/* Handle HW indexing results */
		if (index_pipe)
			mpq_dmx_tspp2_index_pipe_handler(pipe_info, index_pipe,
				ts_packet_size, event, data_size,
				tspp_index_last_addr);

		/*
		 * Limit indexing notification to the last TS packet in the
		 * current recording chunk, since pattern indexing as reported
		 * by the HW might contain entries outside this recording chunk
		 * that will be processed at this time.
		 */
		if (feed->rec_info->ts_output_count + num_packets >
			feed->last_pattern_tsp_num)
			feed->rec_info->idx_info.min_pattern_tsp_num =
				feed->last_pattern_tsp_num;
		else
			feed->rec_info->idx_info.min_pattern_tsp_num =
				feed->rec_info->ts_output_count + num_packets;

		dvb_dmx_notify_idx_events(feed, 1);
	}

	pipe_info->tspp_last_addr = tspp_last_addr;
	pipe_info->tspp_write_offset += data_size;
	if (pipe_info->tspp_write_offset >= pipe_info->buffer.size)
		pipe_info->tspp_write_offset -= pipe_info->buffer.size;
	feed->rec_info->ts_output_count += num_packets;

	if (event == PIPE_EOS_EVENT) {
		data.status = DMX_OK_EOS;
		data.data_length = 0;
		feed->data_ready_cb.ts(&feed->feed.ts, &data);
	}

	return ret;
}

/**
 * Allocate section pipe.
 *
 * @feed: the feed to allocate the pipe to.
 *
 * A single pipe is allocated for all clear TS packets holding sections
 * from specific input, and another pipe for all scrambled
 * TS packets holding sections.
 * The function checks if section pipe is already allocated for the input
 * of the provided feed, if not, a new pipe is allocated.
 *
 * Return  0 on success, error status otherwise
 */
static int mpq_dmx_allocate_sec_pipe(struct dvb_demux_feed *feed)
{
	int ret;
	unsigned long flags;
	struct mpq_feed *mpq_feed = feed->priv;
	struct mpq_tspp2_feed *tspp2_feed = mpq_feed->plugin_priv;
	struct mpq_demux *mpq_demux = feed->demux->priv;
	struct source_info *source_info;
	struct pipe_info *pipe_info;
	struct tspp2_pipe_sps_params sps_cfg;
	struct tspp2_pipe_pull_mode_params pull_cfg;

	source_info = mpq_dmx_get_source(mpq_demux->source);
	if (source_info == NULL) {
		MPQ_DVB_ERR_PRINT("%s(%d): invalid source %d\n",
				__func__, feed->pid, mpq_demux->source);
		return -ENODEV;
	}

	/* MPQ_TODO: Scrambled section support:
	 * TSPP2 secured output buffer will be allocated from the CP_MM heap
	 * (call mpq_dmx_init_out_pipe with the CM_MM heap and ION_SECURE flag)
	 * TZ clear output buffer will be allocated from QSEECOM heap via
	 * mpq_sdmx_init_feed.
	 */
	if (source_info->demux_src.clear_section_pipe == NULL) {
		pipe_info = mpq_dmx_get_free_pipe();
		if (pipe_info == NULL) {
			MPQ_DVB_ERR_PRINT(
				"%s(%d): cannot allocate pipe\n",
				__func__, feed->pid);
			return -ENOMEM;
		}

		pipe_info->source_info = source_info;
		pipe_info->type = CLEAR_SECTION_PIPE;
		pipe_info->parent = tspp2_feed;
		pipe_info->pipe_handler = mpq_dmx_tspp2_section_pipe_handler;
		pipe_info->hw_notif_count = 0;

		sps_cfg.descriptor_size = TSPP2_DMX_SPS_SECTION_DESC_SIZE;
		sps_cfg.descriptor_flags = 0;
		sps_cfg.setting = SPS_O_AUTO_ENABLE | SPS_O_HYBRID |
			SPS_O_ACK_TRANSFERS;
		sps_cfg.wakeup_events = 0;
		sps_cfg.callback = mpq_dmx_sps_producer_cb;
		sps_cfg.user_info = pipe_info;

		if (mpq_demux->demux.playback_mode == DMX_PB_MODE_PULL) {
			pull_cfg.is_stalling = 1;
			pull_cfg.threshold = 3 * TS_PACKET_SIZE;
		} else {
			pull_cfg.is_stalling = 0;
		}

		ret = mpq_dmx_init_out_pipe(mpq_demux, pipe_info,
			TSPP2_DMX_SECTION_PIPE_BUFF_SIZE, &sps_cfg, &pull_cfg,
			1, ION_HEAP(tspp2_buff_heap), 0);

		if (ret) {
			MPQ_DVB_ERR_PRINT(
				"%s: mpq_dmx_init_out_pipe failed, ret=%d\n",
				__func__, ret);
			return ret;
		}
		source_info->demux_src.clear_section_pipe = pipe_info;
		mpq_dmx_start_polling_timer();
	} else {
		pipe_info = source_info->demux_src.clear_section_pipe;
	}

	tspp2_feed->main_pipe = pipe_info;
	spin_lock_irqsave(&pipe_info->lock, flags);
	pipe_info->ref_count++;
	spin_unlock_irqrestore(&pipe_info->lock, flags);

	return 0;
}

/**
 * Release section pipe.
 *
 * @feed: the feed to release its pipe.
 *
 * The function checks if the section pipe attached to the feed is used
 * by other other feeds, if not, the pipe is released.
 *
 * Return  0 on success, error status otherwise
 */
static int mpq_dmx_release_sec_pipe(struct dvb_demux_feed *feed)
{
	struct mpq_demux *mpq_demux = feed->demux->priv;
	struct mpq_tspp2_demux *mpq_tspp2_demux = mpq_demux->plugin_priv;
	struct mpq_feed *mpq_feed = feed->priv;
	struct mpq_tspp2_feed *tspp2_feed = mpq_feed->plugin_priv;
	struct source_info *source_info;
	struct pipe_info *pipe_info;
	u32 ref_count;
	unsigned long flags;

	if (unlikely(tspp2_feed->main_pipe == NULL)) {
		MPQ_DVB_ERR_PRINT(
			"%s(%d): NULL pipe\n", __func__, feed->pid);
		return -EINVAL;
	}

	source_info = mpq_tspp2_demux->source_info;
	if (source_info == NULL) {
		MPQ_DVB_ERR_PRINT("%s(%d): invalid source %d\n",
				__func__, feed->pid, mpq_demux->source);
		return -ENODEV;
	}

	/* TODO: when scrambling is supported,
	 * need to check here scrambled pipe in case the section
	 * is scrambled or the input is a secure one */
	pipe_info = source_info->demux_src.clear_section_pipe;
	if (unlikely(pipe_info != tspp2_feed->main_pipe)) {
		MPQ_DVB_ERR_PRINT(
			"%s(%d): unmatching pipes\n", __func__, feed->pid);
		return -EINVAL;
	}

	tspp2_feed->main_pipe = NULL;

	mutex_lock(&pipe_info->mutex);

	spin_lock_irqsave(&pipe_info->lock, flags);
	pipe_info->ref_count--;
	ref_count = pipe_info->ref_count;
	spin_unlock_irqrestore(&pipe_info->lock, flags);

	if (ref_count == 0) {
		mpq_dmx_stop_polling_timer();
		mpq_dmx_terminate_out_pipe(pipe_info);
		pipe_info->parent = NULL;
		pipe_info->pipe_handler = NULL;
		source_info->demux_src.clear_section_pipe = NULL;
	}

	mutex_unlock(&pipe_info->mutex);
	return 0;
}

/**
 * Allocate PCR pipe.
 *
 * @feed: the feed to allocate the pipe to.
 *
 * Return  0 on success, error status otherwise
 */
static int mpq_dmx_allocate_pcr_pipe(struct dvb_demux_feed *feed)
{
	int ret;
	struct mpq_demux *mpq_demux = feed->demux->priv;
	struct mpq_tspp2_demux *mpq_tspp2_demux = mpq_demux->plugin_priv;
	struct mpq_feed *mpq_feed = feed->priv;
	struct mpq_tspp2_feed *tspp2_feed = mpq_feed->plugin_priv;
	struct source_info *source_info;
	struct pipe_info *pipe_info;
	struct tspp2_pipe_sps_params sps_cfg;
	struct tspp2_pipe_pull_mode_params pull_cfg;
	unsigned long flags;

	source_info = mpq_tspp2_demux->source_info;
	if (source_info == NULL) {
		MPQ_DVB_ERR_PRINT("%s: invalid demux source (%d)\n",
			__func__, mpq_demux->source);
		return -ENODEV;
	}

	pipe_info = mpq_dmx_get_free_pipe();
	if (unlikely(pipe_info == NULL)) {
		MPQ_DVB_ERR_PRINT(
			"%s(%d): cannot allocate PCR pipe\n",
			__func__, feed->pid);
		return -ENOMEM;
	}

	pipe_info->source_info = source_info;
	pipe_info->type = PCR_PIPE;
	pipe_info->parent = tspp2_feed;
	pipe_info->pipe_handler = mpq_dmx_tspp2_pcr_pipe_handler;
	pipe_info->hw_notif_count = 0;

	sps_cfg.descriptor_size = TSPP2_DMX_SPS_PCR_DESC_SIZE;
	sps_cfg.descriptor_flags = SPS_IOVEC_FLAG_INT;
	sps_cfg.setting = SPS_O_AUTO_ENABLE | SPS_O_DESC_DONE |
		SPS_O_ACK_TRANSFERS;
	sps_cfg.wakeup_events = SPS_O_DESC_DONE;
	sps_cfg.callback = mpq_dmx_sps_producer_cb;
	sps_cfg.user_info = pipe_info;

	if (mpq_demux->demux.playback_mode == DMX_PB_MODE_PULL) {
		pull_cfg.is_stalling = 1;
		pull_cfg.threshold = 3 * 195;
	} else {
		pull_cfg.is_stalling = 0;
	}

	ret = mpq_dmx_init_out_pipe(mpq_demux, pipe_info,
		TSPP2_DMX_PCR_PIPE_BUFF_SIZE, &sps_cfg, &pull_cfg, 1,
		ION_HEAP(tspp2_buff_heap), 0);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s(pid=%d): mpq_dmx_init_out_pipe failed, ret=%d\n",
			__func__, feed->pid, ret);
		return ret;
	}
	tspp2_feed->main_pipe = pipe_info;
	spin_lock_irqsave(&pipe_info->lock, flags);
	pipe_info->ref_count = 1;
	spin_unlock_irqrestore(&pipe_info->lock, flags);

	return 0;
}

/**
 * Release PCR pipe.
 *
 * @feed: the feed to release its pipe
 *
 * Return  0 on success, error status otherwise
 */
static int mpq_dmx_release_pcr_pipe(struct dvb_demux_feed *feed)
{
	struct pipe_info *pipe_info;
	struct mpq_feed *mpq_feed = feed->priv;
	struct mpq_tspp2_feed *tspp2_feed = mpq_feed->plugin_priv;
	int ret;

	pipe_info = tspp2_feed->main_pipe;

	if (unlikely(pipe_info == NULL)) {
		MPQ_DVB_ERR_PRINT(
			"%s(%d): NULL pipe\n", __func__, feed->pid);
		return -EINVAL;
	}

	/* Protect from concurrency with the pipe handler */
	mutex_lock(&pipe_info->mutex);

	tspp2_feed->main_pipe = NULL;
	if (pipe_info->ref_count != 1)
		MPQ_DVB_ERR_PRINT("%s(%d): invalid pipe ref. count = %d\n",
			__func__, feed->pid, pipe_info->ref_count);
	pipe_info->ref_count = 0;
	ret = mpq_dmx_terminate_out_pipe(pipe_info);
	if (ret)
		MPQ_DVB_ERR_PRINT(
			"%s(%d): mpq_dmx_terminate_out_pipe failed, ret=%d\n",
			__func__, feed->pid, ret);
	pipe_info->parent = NULL;
	pipe_info->pipe_handler = NULL;

	mutex_unlock(&pipe_info->mutex);
	return ret;
}

/**
 * Allocate PES pipe(s).
 *
 * @feed: the feed to allocate the pipe(s) to.
 *
 * The function allocates a single pipe for all non-video PES.
 * For video PES going to decoder, two pipes are allocated,
 * one holding PES headers and the second holding PES payload.
 *
 * Return  0 on success, error status otherwise
 */
static int mpq_dmx_allocate_pes_pipe(struct dvb_demux_feed *feed)
{
	int ret;
	int is_video;
	u32 buffer_size;
	struct mpq_demux *mpq_demux = feed->demux->priv;
	struct mpq_feed *mpq_feed = feed->priv;
	struct mpq_tspp2_feed *tspp2_feed = mpq_feed->plugin_priv;
	struct pipe_info *pipe_info;
	struct pipe_info *header_pipe = NULL;
	struct tspp2_pipe_sps_params sps_cfg;
	struct tspp2_pipe_pull_mode_params pull_cfg;
	struct mpq_decoder_buffers_desc *dec_bufs =
		&mpq_feed->video_info.buffer_desc;
	unsigned long flags;

	is_video = dvb_dmx_is_video_feed(feed);

	pipe_info = mpq_dmx_get_free_pipe();
	if (unlikely(pipe_info == NULL)) {
		MPQ_DVB_ERR_PRINT(
			"%s(%d): cannot allocate payload pipe\n",
			__func__, feed->pid);
		return -ENOMEM;
	}

	pipe_info->hw_notif_count = 0;

	if (is_video) {
		sps_cfg.descriptor_size = TSPP2_DMX_SPS_VPES_PAYLOAD_DESC_SIZE;
		sps_cfg.setting = SPS_O_AUTO_ENABLE | SPS_O_HYBRID |
			SPS_O_ACK_TRANSFERS;
		sps_cfg.wakeup_events = 0;
		pipe_info->type = VPES_PAYLOAD_PIPE;
		pipe_info->pipe_handler = mpq_dmx_tspp2_video_pipe_handler;

		/*
		 * Video payload buffer (single ringbuffer) was allocated
		 * externally and decoder buffer info was previously
		 * initialized when mpq_dmx_init_video_feed() was called.
		 */
		buffer_size = dec_bufs->desc[0].size;
		pipe_info->buffer.handle = dec_bufs->ion_handle[0];
		pipe_info->buffer.mem = dec_bufs->desc[0].base;
	} else {
		sps_cfg.descriptor_size = TSPP2_DMX_SPS_NON_VID_PES_DESC_SIZE;
		sps_cfg.setting = SPS_O_AUTO_ENABLE | SPS_O_EOT |
			SPS_O_LATE_EOT | SPS_O_ACK_TRANSFERS;
		sps_cfg.wakeup_events = SPS_O_EOT;
		pipe_info->type = PES_PIPE;
		pipe_info->pipe_handler = mpq_dmx_tspp2_pes_pipe_handler;

		/*
		 * Output buffer was allocated externally and already mapped
		 * to kernel memory.
		 */
		buffer_size = feed->feed.ts.buffer.ringbuff->size;
		pipe_info->buffer.handle = feed->feed.ts.buffer.priv_handle;
		pipe_info->buffer.mem = feed->feed.ts.buffer.ringbuff->data;
	}
	sps_cfg.descriptor_flags = 0;
	sps_cfg.callback = mpq_dmx_sps_producer_cb;
	sps_cfg.user_info = pipe_info;

	if (mpq_demux->demux.playback_mode == DMX_PB_MODE_PULL) {
		pull_cfg.is_stalling = 1;
		pull_cfg.threshold =
			3 * (TS_PACKET_SIZE - TS_PACKET_HEADER_LENGTH);
	} else {
		pull_cfg.is_stalling = 0;
	}

	ret = mpq_dmx_init_out_pipe(mpq_demux, pipe_info, buffer_size,
		&sps_cfg, &pull_cfg, 0, 0, 0);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s(pid=%d): mpq_dmx_init_out_pipe failed, ret=%d\n",
			__func__, feed->pid, ret);
		return ret;
	}

	pipe_info->parent = tspp2_feed;
	tspp2_feed->main_pipe = pipe_info;

	spin_lock_irqsave(&pipe_info->lock, flags);
	pipe_info->ref_count = 1;
	spin_unlock_irqrestore(&pipe_info->lock, flags);

	if (is_video) {
		header_pipe = mpq_dmx_get_free_pipe();
		if (unlikely(header_pipe == NULL)) {
			MPQ_DVB_ERR_PRINT(
				"%s(%d): cannot allocate header pipe\n",
				__func__, feed->pid);
			ret = -ENOMEM;
			goto free_main_pipe;
		}
		header_pipe->hw_notif_count = 0;

		sps_cfg.descriptor_size = TSPP2_DMX_SPS_VPES_HEADER_DESC_SIZE;
		sps_cfg.descriptor_flags = 0;
		sps_cfg.setting = SPS_O_AUTO_ENABLE | SPS_O_EOT |
			SPS_O_LATE_EOT | SPS_O_ACK_TRANSFERS;
		sps_cfg.wakeup_events = SPS_O_EOT;
		sps_cfg.callback = mpq_dmx_sps_producer_cb;
		sps_cfg.user_info = header_pipe;

		if (mpq_demux->demux.playback_mode == DMX_PB_MODE_PULL) {
			pull_cfg.is_stalling = 1;
			pull_cfg.threshold =
				3 * (TS_PACKET_SIZE - TS_PACKET_HEADER_LENGTH);
		} else {
			pull_cfg.is_stalling = 0;
		}

		ret = mpq_dmx_init_out_pipe(mpq_demux, header_pipe,
			TSPP2_DMX_VPES_HEADER_PIPE_BUFF_SIZE, &sps_cfg,
			&pull_cfg, 1, ION_HEAP(tspp2_buff_heap), 0);

		if (ret) {
			MPQ_DVB_ERR_PRINT(
				"%s(pid=%d): mpq_dmx_init_out_pipe for header pipe failed, ret=%d\n",
				__func__, feed->pid, ret);
			goto free_main_pipe;
		}

		header_pipe->pipe_handler = mpq_dmx_tspp2_video_pipe_handler;
		header_pipe->parent = tspp2_feed;
		header_pipe->type = VPES_HEADER_PIPE;
		spin_lock_irqsave(&header_pipe->lock, flags);
		header_pipe->ref_count = 1;
		spin_unlock_irqrestore(&header_pipe->lock, flags);

		tspp2_feed->secondary_pipe = header_pipe;
	}

	return 0;

free_main_pipe:
	spin_lock_irqsave(&pipe_info->lock, flags);
	pipe_info->ref_count--;
	spin_unlock_irqrestore(&pipe_info->lock, flags);

	mpq_dmx_terminate_out_pipe(pipe_info);

	return ret;
}

/**
 * Release PES pipe(s).
 *
 * @feed: the feed to release its pipe(s).
 *
 * Return  0 on success, error status otherwise
 *
 * In case of video PES going to decoder, two pipes are released.
 */
static int mpq_dmx_release_pes_pipe(struct dvb_demux_feed *feed)
{
	int ret;
	struct mpq_feed *mpq_feed = feed->priv;
	struct mpq_tspp2_feed *tspp2_feed = mpq_feed->plugin_priv;
	struct pipe_info *pipe_info;

	if (dvb_dmx_is_video_feed(feed)) {
		pipe_info = tspp2_feed->secondary_pipe;

		/*
		 * Remove additional pipe for PES header.
		 * Important to be done before payload-pipe removal as
		 * payload-pipe is polled on notifications from header-pipe
		 */
		if (unlikely(pipe_info == NULL)) {
			MPQ_DVB_ERR_PRINT(
				"%s(%d): NULL pipe\n", __func__, feed->pid);
			return -EINVAL;
		}

		mutex_lock(&pipe_info->mutex);

		tspp2_feed->secondary_pipe = NULL;
		pipe_info->ref_count--;
		mpq_dmx_terminate_out_pipe(pipe_info);
		pipe_info->parent = NULL;
		pipe_info->pipe_handler = NULL;

		mutex_unlock(&pipe_info->mutex);
	}

	pipe_info = tspp2_feed->main_pipe;
	if (unlikely(pipe_info == NULL)) {
		MPQ_DVB_ERR_PRINT(
			"%s(%d): NULL pipe\n", __func__, feed->pid);
		return -EINVAL;
	}

	tspp2_feed->main_pipe = NULL;

	mutex_lock(&pipe_info->mutex);

	pipe_info->ref_count--;
	ret = mpq_dmx_terminate_out_pipe(pipe_info);
	pipe_info->parent = NULL;
	pipe_info->pipe_handler = NULL;

	mutex_unlock(&pipe_info->mutex);
	return ret;
}

static int mpq_dmx_tspp2_allocate_index_pipe(struct dvb_demux_feed *feed)
{
	int ret;
	unsigned long flags;
	struct dvb_demux *dvb_demux = feed->demux;
	struct mpq_demux *mpq_demux = dvb_demux->priv;
	struct mpq_feed *mpq_feed = feed->priv;
	struct mpq_tspp2_feed *tspp2_feed = mpq_feed->plugin_priv;
	struct pipe_info *pipe_info;
	struct tspp2_pipe_sps_params sps_cfg;
	struct tspp2_pipe_pull_mode_params pull_cfg;

	pipe_info = mpq_dmx_get_free_pipe();
	if (unlikely(pipe_info == NULL)) {
		MPQ_DVB_ERR_PRINT(
			"%s(%d): cannot allocate indexing pipe\n",
			__func__, feed->pid);
		return -ENOMEM;
	}

	pipe_info->source_info = tspp2_feed->main_pipe->source_info;
	pipe_info->type = INDEXING_PIPE;
	pipe_info->parent = tspp2_feed;
	pipe_info->pipe_handler = mpq_dmx_tspp2_rec_pipe_handler;
	pipe_info->hw_notif_count = 0;

	sps_cfg.descriptor_size = TSPP2_DMX_SPS_INDEXING_DESC_SIZE;
	sps_cfg.descriptor_flags = 0;
	sps_cfg.setting = SPS_O_AUTO_ENABLE | SPS_O_HYBRID |
		SPS_O_ACK_TRANSFERS;
	sps_cfg.wakeup_events = 0;
	sps_cfg.callback = mpq_dmx_sps_producer_cb;
	sps_cfg.user_info = pipe_info;

	if (dvb_demux->playback_mode == DMX_PB_MODE_PULL) {
		pull_cfg.is_stalling = 1;
		pull_cfg.threshold = 10 * TSPP2_DMX_SPS_INDEXING_DESC_SIZE;
	} else {
		pull_cfg.is_stalling = 0;
	}

	ret = mpq_dmx_init_out_pipe(mpq_demux, pipe_info,
		TSPP2_DMX_INDEX_PIPE_BUFFER_SIZE, &sps_cfg,
		&pull_cfg, 1, ION_HEAP(tspp2_buff_heap), 0);

	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_dmx_init_out_pipe failed, ret=%d\n",
			__func__, ret);
		return ret;
	}
	tspp2_feed->last_pattern_addr = 0;
	tspp2_feed->last_pusi_addr = 0;
	tspp2_feed->secondary_pipe = pipe_info;
	/*
	 * Set the indexing feed as recording pipe parent, so that indexing
	 * pipe is immediately reachable in the recording pipe handler.
	 */
	tspp2_feed->main_pipe->parent = tspp2_feed;

	spin_lock_irqsave(&pipe_info->lock, flags);
	pipe_info->ref_count++;
	spin_unlock_irqrestore(&pipe_info->lock, flags);

	return 0;
}

/**
 * Allocate recording pipe(s).
 *
 * @feed: the feed to allocate the pipe(s) to
 *
 * The function checks if the recording buffer used by the
 * feed is also used by other feeds, if so, the pipe is already
 * allocated by the other feeds, otherwise, a new pipe is allocated.
 *
 * Return  0 on success, error status otherwise
 */
static int mpq_dmx_allocate_rec_pipe(struct dvb_demux_feed *feed)
{
	int ret;
	unsigned long flags;
	struct mpq_demux *mpq_demux = feed->demux->priv;
	struct mpq_feed *mpq_feed = feed->priv;
	struct mpq_tspp2_feed *tspp2_feed = mpq_feed->plugin_priv;
	struct dvb_demux_feed *tmp;
	struct pipe_info *pipe_info;
	struct source_info *source_info;
	struct tspp2_pipe_sps_params sps_cfg;
	struct tspp2_pipe_pull_mode_params pull_cfg;
	size_t tsp_out_size;

	source_info = mpq_dmx_get_source(mpq_demux->source);
	if (source_info == NULL) {
		MPQ_DVB_ERR_PRINT("%s: invalid source (%d)\n",
			__func__, mpq_demux->source);
		return -ENODEV;
	}

	tmp = mpq_dmx_peer_rec_feed(feed);
	if (tmp == NULL) {
		/* New recording filter, allocate new output pipe */
		pipe_info = mpq_dmx_get_free_pipe();
		if (unlikely(pipe_info == NULL)) {
			MPQ_DVB_ERR_PRINT(
				"%s(%d): cannot allocate recording pipe\n",
				__func__, feed->pid);
			return -ENOMEM;
		}

		pipe_info->source_info = source_info;
		pipe_info->type = REC_PIPE;
		pipe_info->parent = tspp2_feed;
		pipe_info->pipe_handler = mpq_dmx_tspp2_rec_pipe_handler;
		pipe_info->hw_notif_count = 0;

		/*
		 * TSPP output is mapped to rec buffer allocated by dmx-dev.
		 * The buffer is already mapped to kernel memory.
		 * TODO: In secured recording, map to CB1, set secure flag,
		 * prohibit kernel mapping in such case.
		 */
		pipe_info->buffer.handle = feed->feed.ts.buffer.priv_handle;
		pipe_info->buffer.mem = feed->feed.ts.buffer.ringbuff->data;

		if (feed->tsp_out_format == DMX_TSP_FORMAT_188) {
			sps_cfg.descriptor_size =
				TSPP2_DMX_SPS_188_RECORDING_DESC_SIZE;
			tsp_out_size = 188;
		} else {
			sps_cfg.descriptor_size =
				TSPP2_DMX_SPS_192_RECORDING_DESC_SIZE;
			tsp_out_size = 192;
		}
		sps_cfg.descriptor_flags = 0;
		sps_cfg.setting = SPS_O_AUTO_ENABLE | SPS_O_HYBRID |
			SPS_O_ACK_TRANSFERS;
		sps_cfg.wakeup_events = 0;
		sps_cfg.callback = mpq_dmx_sps_producer_cb;
		sps_cfg.user_info = pipe_info;

		if (mpq_demux->demux.playback_mode == DMX_PB_MODE_PULL) {
			pull_cfg.is_stalling = 1;
			pull_cfg.threshold = 3 * tsp_out_size;
		} else {
			pull_cfg.is_stalling = 0;
		}

		ret = mpq_dmx_init_out_pipe(mpq_demux, pipe_info,
			feed->feed.ts.buffer.ringbuff->size, &sps_cfg,
			&pull_cfg, 0, 0, 0);
		if (ret) {
			MPQ_DVB_ERR_PRINT(
				"%s: mpq_dmx_init_out_pipe failed, ret=%d\n",
				__func__, ret);
			return ret;
		}
		tspp2_feed->main_pipe = pipe_info;
		mpq_dmx_start_polling_timer();
	} else {
		struct mpq_feed *main_mpq_feed = tmp->priv;
		struct mpq_tspp2_feed *main_tspp2_feed =
			main_mpq_feed->plugin_priv;

		pipe_info = main_tspp2_feed->main_pipe;
		tspp2_feed->main_pipe = pipe_info;
	}

	spin_lock_irqsave(&pipe_info->lock, flags);
	pipe_info->ref_count++;
	spin_unlock_irqrestore(&pipe_info->lock, flags);

	return 0;
}

static int mpq_dmx_tspp2_release_index_pipe(struct pipe_info *pipe_info)
{
	int ref_count;
	unsigned long flags;

	mutex_lock(&pipe_info->mutex);

	spin_lock_irqsave(&pipe_info->lock, flags);
	pipe_info->ref_count--;
	ref_count = pipe_info->ref_count;
	spin_unlock_irqrestore(&pipe_info->lock, flags);

	if (ref_count) {
		MPQ_DVB_ERR_PRINT(
			"%s: invalid ref_count for index pipe %d, should be zero\n",
			__func__, ref_count);
		mutex_unlock(&pipe_info->mutex);
		return -EINVAL;
	}

	MPQ_DVB_DBG_PRINT("%s: terminate secondary indexing pipe\n", __func__);
	mpq_dmx_terminate_out_pipe(pipe_info);
	pipe_info->parent = NULL;
	pipe_info->pipe_handler = NULL;

	mutex_unlock(&pipe_info->mutex);

	return 0;
}

/**
 * Release recording pipe(s).
 *
 * @feed: the feed to release its pipe(s).
 *
 * Return  0 on success, error status otherwise
 *
 * The function checks if the recording pipe attached to the feed is used
 * by other other feeds, if not, the pipe is released.
 * If the feed had an indexing pipe, the indexing pipe is released as well.
 */
static int mpq_dmx_release_rec_pipe(struct dvb_demux_feed *feed)
{
	int ref_count;
	unsigned long flags;
	struct mpq_feed *mpq_feed = feed->priv;
	struct mpq_tspp2_feed *tspp2_feed = mpq_feed->plugin_priv;
	struct pipe_info *pipe_info;

	MPQ_DVB_DBG_PRINT("%s: entry\n", __func__);

	if (unlikely(tspp2_feed->main_pipe == NULL)) {
		MPQ_DVB_ERR_PRINT(
			"%s(%d): NULL pipe\n", __func__, feed->pid);
		return -EINVAL;
	}
	pipe_info = tspp2_feed->main_pipe;
	tspp2_feed->main_pipe = NULL;

	mutex_lock(&pipe_info->mutex);

	spin_lock_irqsave(&pipe_info->lock, flags);
	pipe_info->ref_count--;
	ref_count = pipe_info->ref_count;
	spin_unlock_irqrestore(&pipe_info->lock, flags);
	if (ref_count == 0) {
		mpq_dmx_stop_polling_timer();
		mpq_dmx_terminate_out_pipe(pipe_info);
		pipe_info->parent = NULL;
		pipe_info->pipe_handler = NULL;
	} else {
		/*
		 * Recording pipe may have multiple feeds associated with it.
		 * If the pipe's parent feed was released we must assign another
		 * valid feed for the pipe handler to work with.
		 */
		if (pipe_info->parent == tspp2_feed) {
			struct dvb_demux_feed *feed_tmp;
			struct mpq_feed *mpq_feed_tmp;

			feed_tmp = mpq_dmx_peer_rec_feed(feed);
			if (feed_tmp) {
				MPQ_DVB_DBG_PRINT(
					"%s: Switching pipe parent from feed(pid=%u) to feed(pid=%u)\n",
					__func__, feed->pid, feed_tmp->pid);
				mpq_feed_tmp = feed_tmp->priv;
				pipe_info->parent = mpq_feed_tmp->plugin_priv;
			}
		}
	}

	mutex_unlock(&pipe_info->mutex);

	/* Terminate indexing pipe if exists */
	pipe_info = tspp2_feed->secondary_pipe;
	if (pipe_info != NULL && !mpq_dmx_tspp2_release_index_pipe(pipe_info))
		tspp2_feed->secondary_pipe = NULL;

	MPQ_DVB_DBG_PRINT("%s: exit\n", __func__);
	return 0;
}

/**
 * Allocate pipe(s) for a new feed.
 *
 * @feed: the feed to allocate the pipe(s) to
 *
 * Return  0 on success, error status otherwise
 */
static int mpq_dmx_allocate_pipe(struct dvb_demux_feed *feed)
{
	int ret;

	if (feed->type == DMX_TYPE_SEC)
		return mpq_dmx_allocate_sec_pipe(feed);

	if (dvb_dmx_is_pcr_feed(feed))
		return mpq_dmx_allocate_pcr_pipe(feed);

	if ((feed->ts_type & TS_PAYLOAD_ONLY) ||
		dvb_dmx_is_video_feed(feed))
		return mpq_dmx_allocate_pes_pipe(feed);

	/* Recording case */
	ret = mpq_dmx_allocate_rec_pipe(feed);
	/*
	 * Allocate index pipe only to pattern search indexing feed,
	 * There can be only one.
	 */
	if (!ret && feed->idx_params.enable && feed->pattern_num)
		ret = mpq_dmx_tspp2_allocate_index_pipe(feed);

	return ret;
}

/**
 * Release pipe(s) used by a feed.
 *
 * @feed: the feed to release its pipe(s)
 *
 * Return  0 on success, error status otherwise
 */
static int mpq_dmx_release_pipe(struct dvb_demux_feed *feed)
{
	if (feed->type == DMX_TYPE_SEC)
		return mpq_dmx_release_sec_pipe(feed);

	if (dvb_dmx_is_pcr_feed(feed))
		return mpq_dmx_release_pcr_pipe(feed);

	if ((feed->ts_type & TS_PAYLOAD_ONLY) ||
		dvb_dmx_is_video_feed(feed))
		return mpq_dmx_release_pes_pipe(feed);

	/* Recording case */
	return mpq_dmx_release_rec_pipe(feed);
}

/**
 * Implementation of dmx-device notify_data_read function.
 *
 * @ts_feed: the feed its data was read by the user
 * @bytes_num: Number of bytes that have been read
 *
 * Return  0 on success, error status otherwise
 *
 * This is called by dmx-dev to notify that data was read from
 * from the output buffer of specific feed. Respective
 * BAM descriptors are released.
 */
static int mpq_dmx_tspp2_notify_data_read(struct dmx_ts_feed *ts_feed,
	u32 bytes_num)
{
	struct pipe_info *pipe_info;
	struct dvb_demux_feed *feed = (struct dvb_demux_feed *)ts_feed;
	struct mpq_feed *mpq_feed = feed->priv;
	struct mpq_tspp2_feed *tspp2_feed = mpq_feed->plugin_priv;

	if ((feed->type != DMX_TYPE_TS) ||
		(dvb_dmx_is_video_feed(feed))) {
		return -EINVAL;
	}

	if (mutex_lock_interruptible(&mpq_dmx_tspp2_info.mutex))
		return -ERESTARTSYS;

	pipe_info = tspp2_feed->main_pipe;

	if (mutex_lock_interruptible(&pipe_info->mutex)) {
		mutex_unlock(&mpq_dmx_tspp2_info.mutex);
		return -ERESTARTSYS;
	}

	if (pipe_info->ref_count)
		mpq_dmx_release_data(pipe_info, bytes_num);

	mutex_unlock(&pipe_info->mutex);
	mutex_unlock(&mpq_dmx_tspp2_info.mutex);
	return 0;
}

static void mpq_dmx_tspp2_release_video_payload(struct pipe_info *pipe_info,
	u32 offset, size_t len)
{
	int ret;
	u32 end_offset;
	u32 data_len;

	if (mutex_lock_interruptible(&pipe_info->mutex))
		return;

	if (!pipe_info->ref_count) {
		mutex_unlock(&pipe_info->mutex);
		MPQ_DVB_DBG_PRINT("%s: pipe was released\n", __func__);
		return;
	}

	end_offset = (offset + len) % pipe_info->buffer.size;
	data_len = mpq_dmx_calc_fullness(end_offset,
		pipe_info->tspp_read_offset, pipe_info->buffer.size);

	ret = mpq_dmx_release_data(pipe_info, data_len);
	if (ret)
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_dmx_release_data(data_len=%u) failed, ret=%d\n",
			__func__, data_len, ret);

	mutex_unlock(&pipe_info->mutex);
}

static void mpq_dmx_tspp2_streambuffer_cb(struct mpq_streambuffer *sbuff,
	u32 offset, size_t len, void *user_data)
{
	struct pipe_info *pipe_info = user_data;

	mpq_dmx_tspp2_release_video_payload(pipe_info, offset, len);
}

static int mpq_dmx_tspp2_eos_cmd(struct mpq_tspp2_feed *tspp2_feed)
{
	struct pipe_info *pipe_info;
	struct pipe_work *pipe_work;
	struct source_info *source_info;

	if (mutex_lock_interruptible(&mpq_dmx_tspp2_info.mutex))
		return -ERESTARTSYS;

	pipe_info = tspp2_feed->main_pipe;
	if (dvb_dmx_is_video_feed(tspp2_feed->mpq_feed->dvb_demux_feed))
		pipe_info = tspp2_feed->secondary_pipe;

	source_info = pipe_info->source_info;

	pipe_work = pipe_work_queue_allocate(&pipe_info->work_queue);
	if (pipe_work == NULL) {
		MPQ_DVB_ERR_PRINT("%s: Cannot allocate pipe work\n", __func__);
		mutex_unlock(&mpq_dmx_tspp2_info.mutex);
		return -ENOMEM;
	}

	pipe_work->pipe_info = pipe_info;
	pipe_work->event = PIPE_EOS_EVENT;
	pipe_work->event_count = 1;
	pipe_work->session_id = pipe_info->session_id;

	pipe_work_queue_push(&pipe_info->work_queue, pipe_work);
	wake_up_all(&source_info->demux_src.wait_queue);

	mutex_unlock(&mpq_dmx_tspp2_info.mutex);

	return 0;
}

static int mpq_dmx_tspp2_ts_oob_cmd(struct dmx_ts_feed *ts_feed,
		struct dmx_oob_command *cmd)
{
	struct dvb_demux_feed *feed = (struct dvb_demux_feed *)ts_feed;
	struct mpq_feed *mpq_feed = feed->priv;
	struct mpq_tspp2_feed *tspp2_feed = mpq_feed->plugin_priv;
	struct dmx_data_ready data;
	int ret;

	switch (cmd->type) {
	case DMX_OOB_CMD_EOS:
		ret = mpq_dmx_tspp2_eos_cmd(tspp2_feed);
		break;
	case DMX_OOB_CMD_MARKER:
		data.status = DMX_OK_MARKER;
		data.marker.id = cmd->params.marker.id;
		feed->data_ready_cb.ts(&feed->feed.ts, &data);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int mpq_dmx_tspp2_section_oob_cmd(struct dmx_section_feed *section_feed,
		struct dmx_oob_command *cmd)
{
	struct dvb_demux_feed *feed = (struct dvb_demux_feed *)section_feed;
	struct mpq_feed *mpq_feed = feed->priv;
	struct mpq_tspp2_feed *tspp2_feed = mpq_feed->plugin_priv;
	struct dmx_data_ready data;
	int ret;

	switch (cmd->type) {
	case DMX_OOB_CMD_EOS:
		ret = mpq_dmx_tspp2_eos_cmd(tspp2_feed);
		break;
	case DMX_OOB_CMD_MARKER:
		data.status = DMX_OK_MARKER;
		data.marker.id = cmd->params.marker.id;
		ret = dvb_dmx_notify_section_event(feed, &data, 1);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static inline int mpq_dmx_tspp2_is_mpeg2_pattern(u64 type)
{
	return type & (DMX_IDX_MPEG_SEQ_HEADER |
		DMX_IDX_MPEG_GOP | DMX_IDX_MPEG_FIRST_SEQ_FRAME_START |
		DMX_IDX_MPEG_FIRST_SEQ_FRAME_END | DMX_IDX_MPEG_I_FRAME_START |
		DMX_IDX_MPEG_I_FRAME_END | DMX_IDX_MPEG_P_FRAME_START |
		DMX_IDX_MPEG_P_FRAME_END | DMX_IDX_MPEG_B_FRAME_START |
		DMX_IDX_MPEG_B_FRAME_END);
}

static inline int mpq_dmx_tspp2_is_h264_pattern(u64 type)
{
	return type & (DMX_IDX_H264_SPS | DMX_IDX_H264_PPS |
		DMX_IDX_H264_FIRST_SPS_FRAME_START |
		DMX_IDX_H264_FIRST_SPS_FRAME_END | DMX_IDX_H264_IDR_START |
		DMX_IDX_H264_IDR_END | DMX_IDX_H264_NON_IDR_START |
		DMX_IDX_H264_NON_IDR_END | DMX_IDX_H264_ACCESS_UNIT_DEL |
		DMX_IDX_H264_SEI);
}

static inline int mpq_dmx_tspp2_is_vc1_pattern(u64 type)
{
	return type & (DMX_IDX_VC1_SEQ_HEADER | DMX_IDX_VC1_ENTRY_POINT |
		DMX_IDX_VC1_FIRST_SEQ_FRAME_START |
		DMX_IDX_VC1_FIRST_SEQ_FRAME_END | DMX_IDX_VC1_FRAME_START |
		DMX_IDX_VC1_FRAME_END);
}

static int mpq_dmx_tspp2_is_mixed_codec(enum dmx_video_codec codec, u64 type)
{
	return (codec == DMX_VIDEO_CODEC_MPEG2 &&
		(mpq_dmx_tspp2_is_h264_pattern(type) ||
		mpq_dmx_tspp2_is_vc1_pattern(type))) ||
		(codec == DMX_VIDEO_CODEC_H264 &&
		(mpq_dmx_tspp2_is_mpeg2_pattern(type) ||
		mpq_dmx_tspp2_is_vc1_pattern(type))) ||
		(codec == DMX_VIDEO_CODEC_VC1 &&
		(mpq_dmx_tspp2_is_mpeg2_pattern(type) ||
		mpq_dmx_tspp2_is_h264_pattern(type)));
}

static int mpq_dmx_tspp2_set_indexing(struct dvb_demux_feed *feed)
{
	int ret = 0;
	int i;
	struct mpq_feed *mpq_feed = feed->priv;
	struct mpq_tspp2_feed *tspp2_feed = mpq_feed->plugin_priv;
	struct mpq_dmx_tspp2_filter_op *idx_op;
	enum dmx_video_codec codec = DMX_VIDEO_CODEC_MPEG2;

	if (feed->rec_info->idx_info.pattern_search_feeds_num >
		TSPP2_MAX_REC_PATTERN_INDEXING) {
		MPQ_DVB_ERR_PRINT(
			"%s: Cannot index more than %d video pid\n",
			__func__, TSPP2_MAX_REC_PATTERN_INDEXING);
		return -EBUSY;
	}

	/*
	 * Determine codec in use according to the first patterns.
	 * Verify there is no mixing of different codecs.
	 */
	if (feed->pattern_num) {
		if (mpq_dmx_tspp2_is_mpeg2_pattern(feed->patterns[0]->type))
			codec = DMX_VIDEO_CODEC_MPEG2;
		else if (mpq_dmx_tspp2_is_h264_pattern(feed->patterns[0]->type))
			codec = DMX_VIDEO_CODEC_H264;
		else if (mpq_dmx_tspp2_is_vc1_pattern(feed->patterns[0]->type))
			codec = DMX_VIDEO_CODEC_VC1;
		else {
			MPQ_DVB_ERR_PRINT(
				"%s: Invalid codec patterns\n",
				__func__);
			return -EINVAL;
		}

		for (i = 1; i < feed->pattern_num; i++)
			if (mpq_dmx_tspp2_is_mixed_codec(codec,
				feed->patterns[i]->type)) {
				MPQ_DVB_ERR_PRINT(
					"%s: Cannot mix patterns of different codecs: Active codec is %d, but pattern %llx was requested\n",
					__func__, codec,
					feed->patterns[i]->type);
				return -EINVAL;
			}
	}

	if (mutex_lock_interruptible(&mpq_dmx_tspp2_info.mutex))
		return -ERESTARTSYS;

	if (feed->state != DMX_STATE_GO)
		goto update_index;

	if (tspp2_feed->index_table == TSPP2_NUM_INDEXING_TABLES &&
		feed->pattern_num) {
		/* New indexing */
		MPQ_DVB_DBG_PRINT(
			"%s: Initializing indexing pipe & filter op.\n",
			__func__);
		ret = mpq_dmx_tspp2_allocate_index_pipe(feed);
		if (ret) {
			MPQ_DVB_ERR_PRINT(
				"%s: mpq_dmx_tspp2_allocate_index_pipe failed, ret=%d, PID=%u\n",
				__func__, ret, feed->pid);
			goto end;
		}
		tspp2_feed->index_table = codec;
		ret = mpq_dmx_tspp2_init_index_filter(feed, tspp2_feed->filter);
		if (ret) {
			MPQ_DVB_ERR_PRINT(
				"%s: mpq_dmx_tspp2_init_index_filter failed, ret=%d, PID=%u\n",
				__func__, ret, feed->pid);
			mpq_dmx_tspp2_release_index_pipe(
				tspp2_feed->secondary_pipe);
			goto end;
		}
	} else if (tspp2_feed->index_table != TSPP2_NUM_INDEXING_TABLES) {
		if (!feed->pattern_num) {
			/* Stop indexing */
			MPQ_DVB_DBG_PRINT(
				"%s: Release indexing filter & pipe op.\n",
				__func__);

			ret = mpq_dmx_tspp2_remove_indexing_op(
				tspp2_feed->filter);
			if (ret) {
				MPQ_DVB_ERR_PRINT(
					"%s: mpq_dmx_tspp2_remove_indexing_op failed, ret=%d, PID=%u\n",
					__func__, ret, feed->pid);
				goto end;
			}

			ret = mpq_dmx_tspp2_release_index_pipe(
				tspp2_feed->secondary_pipe);
			if (ret) {
				MPQ_DVB_ERR_PRINT(
					"%s: mpq_dmx_tspp2_release_index_pipe failed, ret=%d, PID=%u\n",
					__func__, ret, feed->pid);
				goto end;
			}

			tspp2_feed->index_table = TSPP2_NUM_INDEXING_TABLES;
			goto end;
		}

		/* Update indexing */
		idx_op = &tspp2_feed->filter->index_op;
		idx_op->op.params.indexing.indexing_table_id = codec;
		ret = mpq_dmx_tspp2_set_filter_ops(tspp2_feed->filter);
		if (ret) {
			MPQ_DVB_ERR_PRINT(
				"%s: Failed to update indexing op. table id from %d to codec=%d, ret=%d\n",
				__func__, tspp2_feed->index_table,
				codec, ret);
			goto end;
		}
		MPQ_DVB_DBG_PRINT(
			"%s: Updated indexing table from %d to %d\n",
			__func__, tspp2_feed->index_table, codec);
	}

update_index:
	MPQ_DVB_DBG_PRINT("%s: Using codec %d\n", __func__, codec);
	tspp2_feed->index_table = codec;
	tspp2_feed->last_pattern_addr = 0;
	tspp2_feed->last_pusi_addr = 0;

end:
	mutex_unlock(&mpq_dmx_tspp2_info.mutex);
	return ret;
}

/**
 * mpq_dmx_tspp2_allocate_ts_insert_pipe() - Allocate an internal buffer,
 * initialize an input pipe to be used for TS buffer insertion, and attach
 * it to the TS insertion source.
 *
 * @source_info:	TS insertion source
 * @feed:		dvb demux feed object
 *
 * Return error code
 */
static int mpq_dmx_tspp2_allocate_ts_insert_pipe(
	struct source_info *source_info, struct dvb_demux_feed *feed)
{
	int ret;
	struct mpq_feed *mpq_feed = feed->priv;
	struct mpq_tspp2_feed *tspp2_feed = mpq_feed->plugin_priv;
	struct mpq_demux *mpq_demux = feed->demux->priv;
	struct pipe_info *pipe_info;
	struct tspp2_pipe_config_params *pipe_cfg;
	unsigned long flags;

	if (!source_info->input_pipe) {
		pipe_info = mpq_dmx_get_free_pipe();
		if (pipe_info == NULL) {
			MPQ_DVB_ERR_PRINT(
				"%s: cannot allocate ts insertion pipe\n",
				__func__);
			return -ENOMEM;
		}

		pipe_info->type = INPUT_PIPE;
		pipe_info->buffer.size = TSPP2_DMX_TS_INSERTION_BUFF_SIZE;
		pipe_info->buffer.handle = ion_alloc(mpq_demux->ion_client,
			TSPP2_DMX_TS_INSERTION_BUFF_SIZE, SZ_4K,
			ION_HEAP(tspp2_buff_heap), 0);

		if (IS_ERR_OR_NULL(pipe_info->buffer.handle)) {
			ret = PTR_ERR(pipe_info->buffer.handle);
			MPQ_DVB_ERR_PRINT(
				"%s: failed to allocate buffer, %d\n",
				__func__, ret);
			if (!ret)
				ret = -ENOMEM;
			return ret;
		}
		pipe_info->buffer.internal_mem = 1;

		pipe_info->buffer.mem = ion_map_kernel(mpq_demux->ion_client,
			pipe_info->buffer.handle);
		if (IS_ERR_OR_NULL(pipe_info->buffer.mem)) {
			ret = PTR_ERR(pipe_info->buffer.mem);
			MPQ_DVB_ERR_PRINT(
				"%s: failed mapping buffer to kernel, %d\n",
				__func__, ret);
			if (!ret)
				ret = -ENOMEM;
			goto free_mem;
		}
		pipe_info->buffer.kernel_map = 1;

		pipe_cfg = &pipe_info->pipe_cfg;
		pipe_cfg->ion_client = mpq_demux->ion_client;
		pipe_cfg->buffer_handle = pipe_info->buffer.handle;
		pipe_cfg->buffer_size = pipe_info->buffer.size;
		pipe_cfg->is_secure = 0;
		pipe_cfg->pipe_mode = TSPP2_SRC_PIPE_INPUT;
		pipe_cfg->sps_cfg.callback = mpq_dmx_sps_consumer_cb;
		pipe_cfg->sps_cfg.user_info = pipe_info;
		pipe_cfg->sps_cfg.descriptor_size =
			TSPP2_DMX_SPS_TS_INSERTION_DESC_SIZE;
		pipe_cfg->sps_cfg.descriptor_flags = 0;
		pipe_cfg->sps_cfg.setting = SPS_O_AUTO_ENABLE | SPS_O_EOT;
		pipe_cfg->sps_cfg.wakeup_events = SPS_O_EOT;

		ret = tspp2_pipe_open(TSPP2_DEVICE_ID, pipe_cfg,
			&pipe_info->buffer.iova, &pipe_info->handle);
		if (ret) {
			MPQ_DVB_ERR_PRINT(
				"%s: tspp2_pipe_open failed, ret=%d\n",
				__func__, ret);
			goto unmap_mem;
		}
		MPQ_DVB_DBG_PRINT(
			"%s: tspp2_pipe_open(): handle=0x%x, iova=0x%0x\n",
			__func__, pipe_info->handle, pipe_info->buffer.iova);

		ret = tspp2_src_pipe_attach(source_info->handle,
					pipe_info->handle, NULL);
		if (ret) {
			MPQ_DVB_ERR_PRINT(
				"%s: tspp2_src_pipe_attach(src=0x%0x, pipe=0x%0x) failed, ret=%d\n",
				__func__, source_info->handle,
				pipe_info->handle, ret);
			goto close_pipe;
		}
		source_info->ref_count++;
		MPQ_DVB_DBG_PRINT(
			"%s: tspp2_src_pipe_attach(src=0x%0x, pipe=0x%0x) success, new source ref. count=%u\n",
			__func__, source_info->handle, pipe_info->handle,
			source_info->ref_count);

		pipe_info->parent = tspp2_feed;
		pipe_info->tspp_last_addr = 0;
		pipe_info->tspp_write_offset = 0;
		pipe_info->tspp_read_offset = 0;
		pipe_info->bam_read_offset = 0;
		pipe_info->source_info = source_info;
		pipe_info->eos_pending = 0;
		pipe_info->session_id++;

		source_info->input_pipe = pipe_info;
	} else {
		pipe_info = source_info->input_pipe;
	}

	spin_lock_irqsave(&pipe_info->lock, flags);
	pipe_info->ref_count++;
	spin_unlock_irqrestore(&pipe_info->lock, flags);

	return 0;

close_pipe:
	tspp2_pipe_close(pipe_info->handle);
	pipe_info->handle = TSPP2_INVALID_HANDLE;
unmap_mem:
	ion_unmap_kernel(mpq_demux->ion_client, pipe_info->buffer.handle);
free_mem:
	ion_free(mpq_demux->ion_client, pipe_info->buffer.handle);

	return ret;
}

/**
 * mpq_dmx_tspp2_release_ts_insert_pipe() - Close TS buffer insertion input
 * pipe and free the internal buffer allocated.
 *
 * @source_info:	TS insertion source
 * @feed:		dvb demux feed object
 *
 * Return error code
 */
static int mpq_dmx_tspp2_release_ts_insert_pipe(
	struct source_info *source_info, struct dvb_demux_feed *feed)
{
	struct pipe_info *pipe_info;
	struct mpq_demux *mpq_demux = feed->demux->priv;
	unsigned long flags;
	u32 ref_count;

	pipe_info = source_info->input_pipe;
	spin_lock_irqsave(&pipe_info->lock, flags);
	pipe_info->ref_count--;
	ref_count = pipe_info->ref_count;
	spin_unlock_irqrestore(&pipe_info->lock, flags);

	if (ref_count == 0) {
		tspp2_src_pipe_detach(source_info->handle, pipe_info->handle);
		source_info->ref_count--;
		tspp2_pipe_close(pipe_info->handle);
		pipe_info->handle = TSPP2_INVALID_HANDLE;

		if (pipe_info->buffer.kernel_map)
			ion_unmap_kernel(mpq_demux->ion_client,
				pipe_info->buffer.handle);

		if (pipe_info->buffer.internal_mem)
			ion_free(mpq_demux->ion_client,
				pipe_info->buffer.handle);

		pipe_info->buffer.mem = NULL;
		pipe_info->buffer.iova = 0;
		pipe_info->buffer.handle = NULL;

		source_info->input_pipe = NULL;
	}

	return 0;
}

/**
 * mpq_dmx_tspp2_ts_insertion_init() - Initialize resources required for
 * TS buffer insertion: the TS insertion source, input pipe and filter.
 * This is called by dmxdev once per recording filter.
 *
 * @ts_feed: dmxdev recording filter feed object
 *
 * Return error code
 */
static int mpq_dmx_tspp2_ts_insertion_init(struct dmx_ts_feed *ts_feed)
{
	int ret;
	struct dvb_demux_feed *feed = (struct dvb_demux_feed *)ts_feed;
	struct mpq_demux *mpq_demux = feed->demux->priv;
	struct source_info *source_info;
	struct mpq_dmx_tspp2_filter *filter;

	if (mutex_lock_interruptible(&mpq_dmx_tspp2_info.mutex))
		return -ERESTARTSYS;

	source_info = &mpq_dmx_tspp2_info.ts_insertion_source;

	ret = mpq_dmx_tspp2_open_source(mpq_demux, source_info);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_dmx_tspp2_open_source failed, ret=%d\n",
			__func__, ret);
		goto end;
	}

	ret = mpq_dmx_tspp2_allocate_ts_insert_pipe(source_info, feed);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_dmx_tspp2_allocate_ts_insert_pipe failed, ret=%d\n",
			__func__, ret);
		goto close_source;
	}

	if (source_info->insert_src.filter == NULL) {
		filter = mpq_dmx_tspp2_get_filter(0x2000, source_info);
		if (!filter) {
			MPQ_DVB_ERR_PRINT(
				"%s: mpq_dmx_tspp2_get_filter\n", __func__);
			ret = -ENOMEM;
			goto release_insert_pipe;
		}
		source_info->insert_src.filter = filter;
	}

	mutex_unlock(&mpq_dmx_tspp2_info.mutex);
	return 0;

release_insert_pipe:
	mpq_dmx_tspp2_release_ts_insert_pipe(source_info, feed);
close_source:
	mpq_dmx_tspp2_close_source(source_info);
end:
	mutex_unlock(&mpq_dmx_tspp2_info.mutex);
	return ret;
}

/**
 * mpq_dmx_tspp2_ts_insertion_terminate() - Release resources used for
 * TS buffer insertion: the TS insertion source, input pipe and filter.
 * This is called by dmxdev once per recording filter.
 *
 * @ts_feed: dmxdev recording filter feed object
 *
 * Return error code
 */
static int mpq_dmx_tspp2_ts_insertion_terminate(struct dmx_ts_feed *ts_feed)
{
	struct source_info *source_info;
	struct dvb_demux_feed *feed = (struct dvb_demux_feed *)ts_feed;

	mutex_lock(&mpq_dmx_tspp2_info.mutex);

	source_info = &mpq_dmx_tspp2_info.ts_insertion_source;

	/* Must close the filter before closing the source */
	if (source_info->ref_count == 1) {
		tspp2_filter_close(source_info->insert_src.filter->handle);
		source_info->insert_src.filter->handle = TSPP2_INVALID_HANDLE;
		source_info->insert_src.filter = NULL;
	}

	mpq_dmx_tspp2_release_ts_insert_pipe(source_info, feed);
	mpq_dmx_tspp2_close_source(source_info);

	mutex_unlock(&mpq_dmx_tspp2_info.mutex);
	return 0;
}

/**
 * mpq_dmx_tspp2_ts_insertion_insert_buffer() - Insert TS buffer to recording
 * pipe.
 *
 * @ts_feed:	dmxdev recording filter feed object
 * @data:	Buffer to insert
 * @size:	Size of buffer
 *
 * Return error code
 */
static int mpq_dmx_tspp2_ts_insertion_insert_buffer(struct dmx_ts_feed *ts_feed,
	char *data, size_t size)
{
	int ret = 0;
	int is_filtering;
	struct source_info *source_info;
	struct dvb_demux_feed *feed = (struct dvb_demux_feed *)ts_feed;
	struct mpq_feed *mpq_feed = feed->priv;
	struct mpq_tspp2_feed *tspp2_feed = mpq_feed->plugin_priv;
	struct mpq_dmx_tspp2_filter_op *raw_op;
	struct mpq_dmx_tspp2_filter *filter;
	enum tspp2_packet_format src_tsp_format;
	struct tspp2_pipe_pull_mode_params pull_cfg;

	if (size > TSPP2_DMX_TS_INSERTION_BUFF_SIZE) {
		MPQ_DVB_ERR_PRINT(
			"%s: insert buffer too big (%u > %u)\n",
			__func__, size, TSPP2_DMX_TS_INSERTION_BUFF_SIZE);
		return -ENOMEM;
	}

	if (mutex_lock_interruptible(&mpq_dmx_tspp2_info.mutex))
		return -ERESTARTSYS;

	MPQ_DVB_DBG_PRINT("%s: entry\n", __func__);

	/* Check filter is still running */
	spin_lock(&feed->demux->lock);
	is_filtering = ts_feed->is_filtering;
	spin_unlock(&feed->demux->lock);

	if (!is_filtering)
		goto end;

	source_info = &mpq_dmx_tspp2_info.ts_insertion_source;
	filter = source_info->insert_src.filter;
	raw_op = &source_info->insert_src.raw_op;
	pull_cfg.is_stalling = 0;

	raw_op->op.params.raw_transmit.timestamp_mode = TSPP2_OP_TIMESTAMP_STC;
	if (feed->tsp_out_format == DMX_TSP_FORMAT_192_HEAD) {
		raw_op->op.params.raw_transmit.timestamp_position =
			TSPP2_PACKET_FORMAT_192_HEAD;
		src_tsp_format = TSPP2_PACKET_FORMAT_192_HEAD;
	} else if (feed->tsp_out_format == DMX_TSP_FORMAT_192_TAIL) {
		raw_op->op.params.raw_transmit.timestamp_position =
			TSPP2_PACKET_FORMAT_192_TAIL;
		src_tsp_format = TSPP2_PACKET_FORMAT_192_TAIL;
	} else {
		raw_op->op.params.raw_transmit.timestamp_mode =
			TSPP2_OP_TIMESTAMP_NONE;
		raw_op->op.params.raw_transmit.timestamp_position =
			TSPP2_PACKET_FORMAT_188_RAW;
		src_tsp_format = TSPP2_PACKET_FORMAT_188_RAW;
	}

	if (source_info->tsp_format != src_tsp_format) {
		ret = tspp2_src_packet_format_set(source_info->handle,
			src_tsp_format);
		if (ret) {
			MPQ_DVB_ERR_PRINT(
				"%s: tspp2_src_packet_format_set failed, ret=%d\n",
				__func__, ret);
			goto end;
		}
	}

	ret = tspp2_src_pipe_attach(source_info->handle,
		tspp2_feed->main_pipe->handle, &pull_cfg);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: tspp2_src_pipe_attach failed, ret=%d\n",
			__func__, ret);
		goto end;
	}
	source_info->ref_count++;
	MPQ_DVB_DBG_PRINT(
		"%s: tspp2_src_pipe_attach(src=0x%0x, pipe=0x%0x) success, new source ref. count=%u\n",
		__func__, source_info->handle, tspp2_feed->main_pipe->handle,
		source_info->ref_count);

	raw_op->op.type = TSPP2_OP_RAW_TRANSMIT;
	raw_op->op.params.raw_transmit.input = TSPP2_OP_BUFFER_A;
	raw_op->op.params.raw_transmit.output_pipe_handle =
		tspp2_feed->main_pipe->handle;
	raw_op->op.params.raw_transmit.support_indexing = 0;
	raw_op->op.params.raw_transmit.skip_ts_errs = 0;

	list_add_tail(&raw_op->next, &filter->operations_list);
	filter->num_ops++;
	ret = mpq_dmx_tspp2_set_filter_ops(filter);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_dmx_tspp2_set_filter_ops failed, ret=%d\n",
			__func__, ret);
		goto detach_pipe_remove_op;
	}

	ret = tspp2_filter_enable(filter->handle);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: tspp2_filter_enable failed, ret=%d\n",
			__func__, ret);
		goto clear_filter_ops;
	}

	memcpy(source_info->input_pipe->buffer.mem, data, size);
	ret = tspp2_src_enable(source_info->handle);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
		"%s: tspp2_src_enable failed, ret=%d\n",
		__func__, ret);
		goto disable_filter;
	}

	ret = tspp2_data_write(source_info->handle, 0, size);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: tspp2_data_write failed, ret=%d\n",
			__func__, ret);
		goto disable_src;
	}

	/*
	 * Mutex remains locked while waiting for completion because:
	 * 1. While waiting we do not want to enable other buffer insertions
	 * 2. While waiting output pipe should not be closed
	 *    (closing will fail since the insertion source is still attached).
	 * This should be ok since wait time for completion is short (source
	 * is not stalling).
	 */
	ret = wait_for_completion_interruptible_timeout(
		&source_info->completion, msecs_to_jiffies(5000));
	if (ret <= 0)
		MPQ_DVB_ERR_PRINT(
			"%s: wait_for_completion_interruptible %s, ret=%d\n",
			__func__, (ret == 0) ? "timedout" : "failed", ret);

disable_src:
	tspp2_src_disable(source_info->handle);
disable_filter:
	tspp2_filter_disable(filter->handle);
clear_filter_ops:
	tspp2_filter_operations_clear(filter->handle);
detach_pipe_remove_op:
	tspp2_src_pipe_detach(source_info->handle,
		tspp2_feed->main_pipe->handle);
	source_info->ref_count--;
	list_del(&raw_op->next);
	filter->num_ops = 0;
end:
	mutex_unlock(&mpq_dmx_tspp2_info.mutex);
	MPQ_DVB_DBG_PRINT("%s: exit\n", __func__);
	return ret;
}

static int mpq_dmx_tspp2_flush_index_pipe(struct pipe_info *index_pipe)
{
	u32 last_addr;
	size_t data_size;

	tspp2_pipe_last_address_used_get(index_pipe->handle, &last_addr);

	data_size = mpq_dmx_tspp2_calc_pipe_data(index_pipe, last_addr);
	index_pipe->tspp_last_addr = last_addr;
	index_pipe->tspp_write_offset += data_size;
	if (index_pipe->tspp_write_offset >= index_pipe->buffer.size)
		index_pipe->tspp_write_offset -= index_pipe->buffer.size;

	return mpq_dmx_release_data(index_pipe, data_size);
}

static int mpq_dmx_tspp2_flush_buffer(struct dmx_ts_feed *ts_feed, size_t len)
{
	struct dvb_demux_feed *feed = (struct dvb_demux_feed *)ts_feed;
	struct mpq_feed *mpq_feed = feed->priv;
	struct mpq_tspp2_feed *tspp2_feed = mpq_feed->plugin_priv;
	struct pipe_info *pipe_info;
	int ret = 0;

	if (mutex_lock_interruptible(&feed->demux->mutex))
		return -ERESTARTSYS;

	if (feed->state != DMX_STATE_GO) {
		mutex_unlock(&feed->demux->mutex);
		return -EINVAL;
	}

	if ((feed->ts_type & TS_PAYLOAD_ONLY) || dvb_dmx_is_video_feed(feed))
		dvbdmx_ts_reset_pes_state(feed);

	if (dvb_dmx_is_video_feed(feed)) {
		/* S-PES */
		pipe_info = tspp2_feed->secondary_pipe;
		if (mutex_lock_interruptible(&pipe_info->mutex)) {
			ret = -ERESTARTSYS;
			goto end;
		}

		/* Header pipe was closed */
		if (!pipe_info->ref_count) {
			ret = -ENODEV;
			goto release_pipe_mutex;
		}

		MPQ_DVB_DBG_PRINT("%s: Flush mpq_streambuffer\n", __func__);
		mpq_dmx_flush_stream_buffer(feed);

		/* Flush video payload pipe */
		MPQ_DVB_DBG_PRINT("%s: Flushing S-PES data pipe\n", __func__);

		MPQ_DVB_DBG_PRINT(
			"%s: Flushing video payload pipe till offset %u\n",
			__func__, tspp2_feed->main_pipe->tspp_write_offset);

		mpq_dmx_tspp2_release_video_payload(tspp2_feed->main_pipe,
			tspp2_feed->main_pipe->tspp_write_offset, 0);
	} else if (!dvb_dmx_is_pcr_feed(feed)) {
		pipe_info = tspp2_feed->main_pipe;
		MPQ_DVB_DBG_PRINT("%s: Flushing %s pipe\n", __func__,
			pipe_info->type == PES_PIPE ? "PES" : "REC");
		if (mutex_lock_interruptible(&pipe_info->mutex)) {
			ret = -ERESTARTSYS;
			goto end;
		}

		if (!pipe_info->ref_count) {
			ret = -ENODEV;
			goto release_pipe_mutex;
		}

		ret = mpq_dmx_release_data(pipe_info, len);

		/* Indexing pipe */
		if (dvb_dmx_is_rec_feed(feed) && feed->idx_params.enable &&
			feed->pattern_num) {
			MPQ_DVB_DBG_PRINT("%s: Flushing indexing pipe\n",
				__func__);
			ret = mpq_dmx_tspp2_flush_index_pipe(
				tspp2_feed->secondary_pipe);
		}
	}

release_pipe_mutex:
	mutex_unlock(&pipe_info->mutex);
end:
	mutex_unlock(&feed->demux->mutex);
	MPQ_DVB_DBG_PRINT("%s(%d) exit, ret=%d\n", __func__, feed->pid, ret);
	return ret;
}

/**
 * Implementation of dvb-demux start_feed function.
 *
 * @feed: the feed to start
 *
 * Return  0 on success, error status otherwise
 */
static int mpq_dmx_tspp2_start_filtering(struct dvb_demux_feed *feed)
{
	int ret = 0;
	struct source_info *source_info;
	struct mpq_feed *mpq_feed = feed->priv;
	struct mpq_tspp2_feed *tspp2_feed = mpq_feed->plugin_priv;
	struct mpq_demux *mpq_demux = mpq_feed->mpq_demux;
	struct mpq_tspp2_demux *mpq_tspp2_demux = mpq_demux->plugin_priv;

	MPQ_DVB_DBG_PRINT("%s(%d) entry\n", __func__, feed->pid);

	if (mutex_lock_interruptible(&mpq_dmx_tspp2_info.mutex))
		return -ERESTARTSYS;

	/*
	 * Always feed sections/PES starting from a new one and
	 * do not partial transfer data from older one
	 */
	feed->pusi_seen = 0;

	if (feed->type == DMX_TYPE_TS) {
		feed->feed.ts.flush_buffer = mpq_dmx_tspp2_flush_buffer;
		feed->feed.ts.notify_data_read = mpq_dmx_tspp2_notify_data_read;
		feed->feed.ts.oob_command = mpq_dmx_tspp2_ts_oob_cmd;
		if (dvb_dmx_is_rec_feed(feed)) {
			feed->feed.ts.ts_insertion_init =
				mpq_dmx_tspp2_ts_insertion_init;
			feed->feed.ts.ts_insertion_insert_buffer =
				mpq_dmx_tspp2_ts_insertion_insert_buffer;
			feed->feed.ts.ts_insertion_terminate =
				mpq_dmx_tspp2_ts_insertion_terminate;
		}
	} else {
		feed->feed.sec.flush_buffer = NULL;
		feed->feed.sec.notify_data_read = NULL;
		feed->feed.sec.oob_command = mpq_dmx_tspp2_section_oob_cmd;
	}

	source_info = mpq_tspp2_demux->source_info;
	if (source_info == NULL) {
		MPQ_DVB_ERR_PRINT(
			"%s(pid=%d): invalid source %d\n",
			__func__, feed->pid, mpq_demux->source);
		ret = -ENODEV;
		goto start_filtering_failed;
	}

	ret = mpq_dmx_tspp2_open_source(mpq_demux, source_info);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_dmx_tspp2_open_source failed, ret=%d\n",
			__func__, ret);
		goto start_filtering_failed;
	}

	if (dvb_dmx_is_video_feed(feed)) {
		ret = mpq_dmx_init_video_feed(mpq_feed);
		if (ret) {
			MPQ_DVB_ERR_PRINT(
				"%s: mpq_dmx_init_video_feed failed, ret=%d\n",
				__func__, ret);
			goto start_filtering_failed_close_source;
		}
	}

	ret = mpq_dmx_allocate_pipe(feed);
	if (ret) {
		MPQ_DVB_ERR_PRINT("%s: failed to allocate pipe, %d\n",
			__func__, ret);
		goto start_filtering_failed_terminate_video_feed;
	}

	ret = mpq_dmx_tspp2_init_filter(feed, source_info);
	if (ret) {
		MPQ_DVB_ERR_PRINT("%s: failed to init. filter, ret=%d\n",
			__func__, ret);
		goto start_filtering_failed_release_pipe;
	}

	ret = tspp2_filter_enable(tspp2_feed->filter->handle);
	if (ret) {
		MPQ_DVB_ERR_PRINT("%s: failed to enable filter, ret=%d\n",
			__func__, ret);
		goto start_filtering_failed_terminate_filter;
	}

	if (dvb_dmx_is_video_feed(feed)) {
		ret = mpq_streambuffer_register_data_dispose(
			mpq_feed->video_info.video_buffer,
			mpq_dmx_tspp2_streambuffer_cb,
			tspp2_feed->main_pipe);
		if (ret) {
			MPQ_DVB_ERR_PRINT(
				"%s: mpq_streambuffer_register_pkt_dispose failed, ret=%d\n",
				__func__, ret);
			goto start_filtering_failed_terminate_filter;
		}
	}

	if (!source_info->enabled && mpq_demux->source >= DMX_SOURCE_FRONT0 &&
		mpq_demux->source < DMX_SOURCE_FRONT0 + TSPP2_NUM_TSIF_INPUTS) {
		ret = tspp2_src_enable(source_info->handle);
		if (ret) {
			MPQ_DVB_ERR_PRINT(
				"%s: tspp2_src_enable failed, ret=%d\n",
				__func__, ret);
			goto start_filtering_failed_terminate_filter;
		}
		source_info->enabled = 1;
		MPQ_DVB_DBG_PRINT(
			"%s: Enabling TSIF source\n", __func__);
	}

	mutex_unlock(&mpq_dmx_tspp2_info.mutex);
	return 0;

start_filtering_failed_terminate_filter:
	mpq_dmx_tspp2_terminate_filter(feed, source_info);
start_filtering_failed_release_pipe:
	mpq_dmx_release_pipe(feed);
start_filtering_failed_terminate_video_feed:
	if (dvb_dmx_is_video_feed(feed))
		mpq_dmx_terminate_video_feed(mpq_feed);
start_filtering_failed_close_source:
	mpq_dmx_tspp2_close_source(source_info);
start_filtering_failed:
	mutex_unlock(&mpq_dmx_tspp2_info.mutex);

	MPQ_DVB_DBG_PRINT("%s(%d) exit\n", __func__, feed->pid);
	return ret;
}

/**
 * Implementation of dvb-demux stop_feed function.
 *
 * @feed: the feed to stop
 *
 * Return  0 on success, error status otherwise
 */
static int mpq_dmx_tspp2_stop_filtering(struct dvb_demux_feed *feed)
{
	struct source_info *source_info;
	struct mpq_feed *mpq_feed = feed->priv;
	struct mpq_tspp2_feed *tspp2_feed = mpq_feed->plugin_priv;
	struct mpq_demux *mpq_demux = mpq_feed->mpq_demux;
	struct mpq_tspp2_demux *mpq_tspp2_demux = mpq_demux->plugin_priv;

	MPQ_DVB_DBG_PRINT("%s(%d) entry\n", __func__, feed->pid);

	mutex_lock(&mpq_dmx_tspp2_info.mutex);

	source_info = mpq_tspp2_demux->source_info;
	if (source_info == NULL) {
		MPQ_DVB_ERR_PRINT(
			"%s(pid=%d): invalid demux source\n",
			__func__, feed->pid);
		mutex_unlock(&mpq_dmx_tspp2_info.mutex);
		return -EINVAL;
	}

	mpq_dmx_tspp2_terminate_filter(feed, source_info);

	/*
	 * After filter is terminated, it should be safe to close the output
	 * pipe even though the source is enabled.
	 */
	mpq_dmx_release_pipe(feed);

	mpq_dmx_tspp2_close_source(source_info);

	if (dvb_dmx_is_video_feed(feed))
		mpq_dmx_terminate_video_feed(mpq_feed);

	if (tspp2_feed->index_table < TSPP2_NUM_INDEXING_TABLES)
		tspp2_feed->index_table = TSPP2_NUM_INDEXING_TABLES;

	mutex_unlock(&mpq_dmx_tspp2_info.mutex);
	return 0;
}

/**
 * Implementation of dmx-device connect_frontend function.
 *
 * @demux: the demux device to connect new front-end to it
 * @frontend: the frontend to connect to demux
 *
 * Return  0 on success, error status otherwise
 *
 * In case of memory frontend (playback from memory), the
 * function tries to allocate a BAM consumer pipe.
 */
static int mpq_dmx_tspp2_connect_frontend(struct dmx_demux *demux,
	struct dmx_frontend *frontend)
{
	struct dvb_demux *dvbdemux = (struct dvb_demux *)demux;
	struct mpq_demux *mpq_demux = dvbdemux->priv;
	struct source_info *source_info;
	struct pipe_info *pipe_info;
	unsigned long flags;
	dmx_source_t source;
	int ret = 0;

	if (mutex_lock_interruptible(&dvbdemux->mutex))
		return -ERESTARTSYS;

	if (mutex_lock_interruptible(&mpq_dmx_tspp2_info.mutex)) {
		mutex_unlock(&dvbdemux->mutex);
		return -ERESTARTSYS;
	}

	if (!frontend) {
		demux->frontend = frontend;
		goto end;
	}

	if (demux->frontend) {
		MPQ_DVB_ERR_PRINT(
			"%s: Front-end already connected (demux->frontend=%d)\n",
			__func__, demux->frontend->source);
		ret = -EBUSY;
		goto end;
	}

	if (frontend->source != DMX_MEMORY_FE) {
		/* Nothing special to do for TSIF source */
		demux->frontend = frontend;
		goto end;
	}

	source = DMX_SOURCE_DVR0 + mpq_demux->idx;
	source_info = mpq_dmx_get_source(source);
	if (source_info == NULL) {
		MPQ_DVB_ERR_PRINT("%s: invalid source %d\n", __func__, source);
		ret = -ENODEV;
		goto end;
	}

	if (source_info->input_pipe == NULL) {
		/* Reserve input pipe for DVR source */
		pipe_info = mpq_dmx_get_free_pipe();
		if (pipe_info == NULL) {
			MPQ_DVB_ERR_PRINT(
				"%s): cannot allocate input pipe\n", __func__);
			ret = -ENOMEM;
			goto end;
		}
		spin_lock_irqsave(&pipe_info->lock, flags);
		pipe_info->type = INPUT_PIPE;
		pipe_info->ref_count++;
		spin_unlock_irqrestore(&pipe_info->lock, flags);
		source_info->input_pipe = pipe_info;
		pipe_info->source_info = source_info;
	}

	demux->frontend = frontend;

end:
	mutex_unlock(&mpq_dmx_tspp2_info.mutex);
	mutex_unlock(&dvbdemux->mutex);
	return ret;
}

/**
 * Implementation of dmx-device disconnect_frontend function.
 *
 * @demux: the demux device to disconnect its front-end
 *
 * Return  0 on success, error status otherwise
 *
 * In case of memory frontend (playback from memory), the
 * function releases the BAM consumer pipe.
 */
static int mpq_dmx_tspp2_disconnect_frontend(struct dmx_demux *demux)
{
	int ret = 0;
	struct dvb_demux *dvbdemux = (struct dvb_demux *)demux;
	struct mpq_demux *mpq_demux = dvbdemux->priv;
	struct pipe_info *pipe_info;
	struct source_info *source_info;
	enum dmx_frontend_source fe_source;
	dmx_source_t source;
	unsigned long flags;
	u32 ref_count;

	mutex_lock(&dvbdemux->mutex);
	if (!demux->frontend) {
		mutex_unlock(&dvbdemux->mutex);
		return 0;
	}

	fe_source = demux->frontend->source;
	demux->frontend = NULL;
	mutex_unlock(&dvbdemux->mutex);

	if (fe_source != DMX_MEMORY_FE)
		return 0;

	mutex_lock(&mpq_dmx_tspp2_info.mutex);

	source = DMX_SOURCE_DVR0 + mpq_demux->idx;
	source_info = mpq_dmx_get_source(source);
	if (source_info == NULL) {
		MPQ_DVB_ERR_PRINT("%s: invalid source %d\n", __func__, source);
		mutex_unlock(&mpq_dmx_tspp2_info.mutex);
		return -ENODEV;
	}

	/* Disable source before detaching the input pipe */
	if (source_info->handle != TSPP2_INVALID_HANDLE &&
		source_info->enabled) {
		ret = tspp2_src_disable(source_info->handle);
		if (ret)
			MPQ_DVB_ERR_PRINT(
				"%s: tspp2_src_disable failed, ret=%d\n",
				__func__, ret);
		source_info->enabled = 0;
	}

	pipe_info = source_info->input_pipe;
	if (pipe_info) {
		spin_lock_irqsave(&pipe_info->lock, flags);
		pipe_info->ref_count--;
		ref_count = pipe_info->ref_count;
		spin_unlock_irqrestore(&pipe_info->lock, flags);

		if (ref_count) {
			MPQ_DVB_ERR_PRINT(
				"%s: invalid input pipe ref count: expected 0, actual %d\n",
				__func__, ref_count);
			/* Fix up the ref. count */
			spin_lock_irqsave(&pipe_info->lock, flags);
			pipe_info->ref_count = 0;
			spin_unlock_irqrestore(&pipe_info->lock, flags);
		}

		if (pipe_info->handle != TSPP2_INVALID_HANDLE) {
			if (source_info->handle != TSPP2_INVALID_HANDLE) {
				ret = tspp2_src_pipe_detach(source_info->handle,
					pipe_info->handle);
				if (ret)
					MPQ_DVB_ERR_PRINT(
						"%s: tspp2_src_pipe_detach failed, ret=%d\n",
						__func__, ret);
				source_info->ref_count--;
			}

			ret = tspp2_pipe_close(pipe_info->handle);
			if (ret)
				MPQ_DVB_ERR_PRINT(
					"%s: tspp2_pipe_close failed, ret=%d\n",
					__func__, ret);
			pipe_info->handle = TSPP2_INVALID_HANDLE;
		}

		source_info->input_pipe = NULL;
	}

	/*
	 * Closing the source can close the TSPP2 device instance, so it needs
	 * to be done last, after input pipe was detached and closed properly.
	 */
	if (source_info->handle != TSPP2_INVALID_HANDLE)
		mpq_dmx_tspp2_close_source(source_info);

	mutex_unlock(&mpq_dmx_tspp2_info.mutex);
	return ret;
}

/**
 * Implementation of dmx-device write function.
 *
 * @demux: the demux device to write data to
 * @buf: buffer holding the data
 * @count: length of data
 *
 * Return number of bytes written, or error status otherwise
 *
 * The function queues respective number of descriptors
 * to the BAM consumer pipe so that TSPP can read the data
 * from memory. The function returns after TSPP has
 * fully read the data or in case data-write was aborted.
 */
static int mpq_dmx_tspp2_write(struct dmx_demux *demux,
			const char *buf, size_t count)
{
	struct dvb_demux *dvbdemux = (struct dvb_demux *)demux;
	struct mpq_demux *mpq_demux = dvbdemux->priv;
	struct mpq_tspp2_demux *mpq_tspp2_demux = mpq_demux->plugin_priv;
	struct source_info *source_info;
	struct pipe_info *pipe_info;
	struct tspp2_pipe_config_params *pipe_cfg;

	u32 data_offset;
	u32 data_length;
	u32 max_desc_len;
	int ret = 0;

	/* Process only whole TS packets */
	data_length = (count / dvbdemux->ts_packet_size) *
		dvbdemux->ts_packet_size;
	if (!data_length)
		return 0;

	if ((!demux->frontend) || (demux->frontend->source != DMX_MEMORY_FE))
		return -EINVAL;

	if (mutex_lock_interruptible(&mpq_dmx_tspp2_info.mutex))
		return -ERESTARTSYS;

	source_info = mpq_tspp2_demux->source_info;
	if (unlikely(source_info == NULL)) {
		MPQ_DVB_ERR_PRINT("%s: invalid source\n", __func__);
		mutex_unlock(&mpq_dmx_tspp2_info.mutex);
		return -EINVAL;
	}

	/*
	 * If source is not yet initialized this means no demux filter is
	 * set for this source so data can simply be discarded.
	 */
	if (source_info->handle == TSPP2_INVALID_HANDLE) {
		mutex_unlock(&mpq_dmx_tspp2_info.mutex);
		return data_length;
	}

	pipe_info = source_info->input_pipe;

	if (unlikely(pipe_info == NULL)) {
		MPQ_DVB_ERR_PRINT(
			"%s: NULL input pipe, should never happen\n", __func__);
		mutex_unlock(&mpq_dmx_tspp2_info.mutex);
		return -ENODEV;
	}

	if (pipe_info->handle == TSPP2_INVALID_HANDLE) {
		/*
		 * Allocate new input pipe. At this point it is assumed that
		 * user has already set the DVR buffer & input TS packet format
		 * & playback mode (since there are some demux filter running).
		 * Also, input pipe has already been reserved at the time
		 * of DVR open.
		 */
		if (dvbdemux->tsp_format == DMX_TSP_FORMAT_188)
			max_desc_len = TSPP2_DMX_SPS_188_INPUT_BUFF_DESC_SIZE;
		else
			max_desc_len = TSPP2_DMX_SPS_192_INPUT_BUFF_DESC_SIZE;

		/*
		 * DVR input buffer was allocated externally and already
		 * mapped to kernel memory.
		 * (in secure recording mode kernel memory mapping is NULL)
		 * TODO: In secured recording, set secure flag)
		 */
		pipe_info->buffer.handle = demux->dvr_input.priv_handle;
		pipe_info->buffer.mem = demux->dvr_input.ringbuff->data;
		pipe_info->buffer.size = demux->dvr_input.ringbuff->size;

		pipe_cfg = &pipe_info->pipe_cfg;
		pipe_cfg->is_secure = 0;
		pipe_cfg->ion_client = mpq_demux->ion_client;
		pipe_cfg->buffer_handle = pipe_info->buffer.handle;
		pipe_cfg->buffer_size = pipe_info->buffer.size;
		pipe_cfg->pipe_mode = TSPP2_SRC_PIPE_INPUT;
		pipe_cfg->sps_cfg.callback = mpq_dmx_sps_consumer_cb;
		pipe_cfg->sps_cfg.user_info = pipe_info;
		pipe_cfg->sps_cfg.descriptor_size = max_desc_len;
		pipe_cfg->sps_cfg.descriptor_flags = 0;
		pipe_cfg->sps_cfg.setting = SPS_O_AUTO_ENABLE | SPS_O_EOT;
		pipe_cfg->sps_cfg.wakeup_events = SPS_O_EOT;

		/*
		 * User needs to be aware he must allocate DVR input
		 * buffer as multiply of actual desc. size (188/192), as
		 * reflected in demux get capabilities.
		 */
		if (pipe_cfg->buffer_size % pipe_cfg->sps_cfg.descriptor_size) {
			MPQ_DVB_ERR_PRINT(
				"%s: Buffer size %d not aligned to desc size %d\n",
				__func__, pipe_cfg->buffer_size,
				pipe_cfg->sps_cfg.descriptor_size);
			mutex_unlock(&mpq_dmx_tspp2_info.mutex);
			return -EINVAL;
		}

		/*
		 * TSPP2 driver initialized a new pipe object:
		 * Allocates desc. buffer memory, and maps supplied data buffer
		 * to TSPP2's virtual address space. The mapped address is
		 * returned into the iova parameter.
		 */
		ret = tspp2_pipe_open(TSPP2_DEVICE_ID, pipe_cfg,
			&pipe_info->buffer.iova, &pipe_info->handle);
		if (ret) {
			MPQ_DVB_ERR_PRINT(
				"%s: tspp2_pipe_open failed, ret=%d\n",
				__func__, ret);
			mutex_unlock(&mpq_dmx_tspp2_info.mutex);
			return ret;
		}
		MPQ_DVB_DBG_PRINT(
			"%s: tspp2_pipe_open(): handle=0x%x, iova=0x%0x\n",
			__func__, pipe_info->handle, pipe_info->buffer.iova);

		ret = tspp2_src_pipe_attach(source_info->handle,
			pipe_info->handle, NULL);
		if (ret) {
			MPQ_DVB_ERR_PRINT(
				"%s: tspp2_src_pipe_attach(src=0x%0x, pipe=0x%0x) failed, ret=%d\n",
				__func__, source_info->handle,
				pipe_info->handle, ret);
			tspp2_pipe_close(pipe_info->handle);
			pipe_info->handle = TSPP2_INVALID_HANDLE;
			mutex_unlock(&mpq_dmx_tspp2_info.mutex);
			return ret;
		}
		pipe_info->source_info = source_info;
		source_info->ref_count++;
		MPQ_DVB_DBG_PRINT(
			"%s: tspp2_src_pipe_attach(src=0x%0x, pipe=0x%0x) success, new source ref. count=%u\n",
			__func__, source_info->handle, pipe_info->handle,
			source_info->ref_count);
		pipe_info->tspp_last_addr = 0;
		pipe_info->tspp_write_offset = 0;
		pipe_info->tspp_read_offset = 0;
		pipe_info->bam_read_offset = 0;
		pipe_info->eos_pending = 0;
		pipe_info->session_id++;
		pipe_info->handler_count = 0;
		pipe_info->hw_notif_count = 0;
		pipe_info->hw_missed_notif = 0;

		if (!source_info->enabled) {
			MPQ_DVB_DBG_PRINT(
				"%s: enabling source for memory input\n",
				__func__);
			tspp2_src_enable(source_info->handle);
			source_info->enabled = 1;
		}
	}

	INIT_COMPLETION(source_info->completion);

	data_offset = demux->dvr_input.ringbuff->pread;

	/*
	 * tspp2_data_write is not blocking. When data transfer is finished,
	 * mpq_dmx_sps_consumer_cb callback notifies on completion.
	 */
	ret = tspp2_data_write(source_info->handle, data_offset, data_length);
	mutex_unlock(&mpq_dmx_tspp2_info.mutex);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: tspp2_data_write failed, ret=%d\n",
			__func__, ret);
		return ret;
	}

	MPQ_DVB_DBG_PRINT(
		"%s: tspp2_data_write(src=0x%0x, offset=%d, length=%d) success\n",
		__func__, source_info->handle, data_offset, data_length);
	/*
	 * Wait for notification from mpq_dmx_sps_consumer_cb.
	 * In case TSPP2 is stalling in pull mode, we can block here
	 * indefinitely.
	 */
	ret = wait_for_completion_interruptible(&source_info->completion);

	return (ret < 0) ? ret : data_length;
}

static int mpq_dmx_tspp2_write_cancel(struct dmx_demux *demux)
{
	struct dvb_demux *dvb_demux = (struct dvb_demux *)demux;
	struct mpq_demux *mpq_demux = dvb_demux->priv;
	struct mpq_tspp2_demux *mpq_tspp2_demux = mpq_demux->plugin_priv;
	struct source_info *source_info = mpq_tspp2_demux->source_info;

	MPQ_DVB_DBG_PRINT("%s\n", __func__);

	if (source_info == NULL) {
		MPQ_DVB_ERR_PRINT("%s: invalid demux source %d\n",
			__func__, mpq_demux->source);
		return -ENODEV;
	}

	complete(&source_info->completion);

	return 0;
}

static bool mpq_dmx_tspp2_pipe_do_work(struct source_info *source_info)
{
	struct pipe_info *pipe_info;
	int i;

	for (i = 0; i < TSPP2_NUM_PIPES; i++) {
		pipe_info = &mpq_dmx_tspp2_info.pipes[i];
		if (pipe_info->source_info == source_info &&
			(!pipe_work_queue_empty(&pipe_info->work_queue)))
			return true;
	}

	return false;
}

static void mpq_dmx_tspp2_call_pipe_handler(struct pipe_work *pipe_work,
	struct pipe_info *pipe_info)
{
	int i;

	for (i = 0; i < pipe_work->event_count; i++) {
		if (mutex_lock_interruptible(&pipe_info->mutex))
			break;

		/* Check pipe was not closed / reopened */
		if (!pipe_info->pipe_handler || !pipe_info->ref_count ||
			pipe_work->session_id != pipe_info->session_id) {
			mutex_unlock(&pipe_info->mutex);
			break;
		}

		/* Call pipe handler while pipe mutex is locked */
		pipe_info->pipe_handler(pipe_info, pipe_work->event);
		pipe_info->handler_count++;

		mutex_unlock(&pipe_info->mutex);
	}
}

static int mpq_dmx_tspp2_thread(void *arg)
{
	struct source_info *source_info = arg;
	struct pipe_work *pipe_work;
	struct pipe_info *pipe_info;
	int ret;
	int i;

	while (1) {
		ret = wait_event_interruptible(
			source_info->demux_src.wait_queue,
			mpq_dmx_tspp2_pipe_do_work(source_info) ||
			kthread_should_stop());
		if (ret) {
			MPQ_DVB_ERR_PRINT(
				"%s: wait_event_interruptible ret=%d\n",
				__func__, ret);
			break;
		}
		if (kthread_should_stop()) {
			MPQ_DVB_DBG_PRINT("%s: Thread should stop\n", __func__);
			break;
		}

		for (i = 0; i < TSPP2_NUM_PIPES; i++) {
			pipe_info = &mpq_dmx_tspp2_info.pipes[i];

			/*
			 * Lock pipe mutex to protect against pipe being closed
			 * during its processing
			 */
			if (mutex_lock_interruptible(&pipe_info->mutex))
				continue;

			if (pipe_info->source_info != source_info ||
				pipe_work_queue_empty(&pipe_info->work_queue)) {
				mutex_unlock(&pipe_info->mutex);
				continue;
			}

			pipe_work = pipe_work_queue_pop(&pipe_info->work_queue);

			/*
			 * The pipe work might have been flushed
			 * if filter was stopped
			 */
			if (pipe_work == NULL) {
				MPQ_DVB_DBG_PRINT(
					"%s: pipe was flushed\n", __func__);
				mutex_unlock(&pipe_info->mutex);
				continue;
			}

			mutex_unlock(&pipe_info->mutex);

			mpq_dmx_tspp2_call_pipe_handler(pipe_work, pipe_info);

			pipe_work_queue_release(&pipe_info->work_queue,
				pipe_work);
		}
	}

	/* Terminate thread gracefully */
	set_current_state(TASK_INTERRUPTIBLE);
	while (!kthread_should_stop()) {
		schedule();
		set_current_state(TASK_INTERRUPTIBLE);
	}
	set_current_state(TASK_RUNNING);

	return 0;
}

static int mpq_dmx_tspp2_map_buffer(struct dmx_demux *demux,
	struct dmx_buffer *dmx_buffer, void **priv_handle, void **kernel_mem)
{
	struct dvb_demux *dvb_demux = demux->priv;
	struct mpq_demux *mpq_demux = dvb_demux->priv;
	struct source_info *source_info;
	struct pipe_info *pipe_info;

	/*
	 * If this is the DVR input buffer verify a consumer pipe was not
	 * already allocated.
	 * MPQ_TODO: review - maybe terminate the pipe, so consumer pipe will
	 * be initialized again in the next DVR write operation.
	 */
	if (priv_handle == &demux->dvr_input.priv_handle) {
		source_info = mpq_dmx_get_source(DMX_SOURCE_DVR0 +
						mpq_demux->idx);

		if (mutex_lock_interruptible(&mpq_dmx_tspp2_info.mutex))
			return -ERESTARTSYS;

		if (!source_info->input_pipe) {
			mutex_unlock(&mpq_dmx_tspp2_info.mutex);
			MPQ_DVB_ERR_PRINT(
				"%s: invalid input pipe\n",
				__func__);
			return -EINVAL;
		}

		pipe_info = source_info->input_pipe;
		if (pipe_info->handle != TSPP2_INVALID_HANDLE) {
			mutex_unlock(&mpq_dmx_tspp2_info.mutex);
			return -EBUSY;
		}

		mutex_unlock(&mpq_dmx_tspp2_info.mutex);
	}

	return mpq_dmx_map_buffer(demux, dmx_buffer, priv_handle, kernel_mem);
}

static int mpq_dmx_tspp2_set_source(struct dmx_demux *demux,
	const dmx_source_t *src)
{
	int ret;
	struct dvb_demux *dvb_demux = demux->priv;
	struct mpq_demux *mpq_demux = dvb_demux->priv;
	struct mpq_tspp2_demux *mpq_tspp2_demux = mpq_demux->plugin_priv;
	struct dvb_demux_feed *feed;

	if (*src == mpq_demux->source)
		return 0;

	/* Verify no feed is running */
	if (mutex_lock_interruptible(&dvb_demux->mutex))
		return -ERESTARTSYS;

	list_for_each_entry(feed, &dvb_demux->feed_list, list_head) {
		if (feed->state == DMX_STATE_GO) {
			mutex_unlock(&dvb_demux->mutex);
			return -EBUSY;
		}
	}

	mutex_unlock(&dvb_demux->mutex);

	ret = mpq_dmx_set_source(demux, src);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_dmx_set_source(%d) failed, ret=%d\n",
			__func__, *src, ret);
		return ret;
	}

	mpq_tspp2_demux->source_info = mpq_dmx_get_source(mpq_demux->source);

	return ret;
}

/**
 * Returns demux capabilities of TSPPv2 plugin
 *
 * @demux: demux device
 * @caps: Returned capabilities
 *
 * Return     error code
 */
static int mpq_dmx_tspp2_get_caps(struct dmx_demux *demux,
				struct dmx_caps *caps)
{
	struct dvb_demux *dvb_demux = demux->priv;

	if ((dvb_demux == NULL) || (caps == NULL)) {
		MPQ_DVB_ERR_PRINT("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	caps->caps = DMX_CAP_PULL_MODE | DMX_CAP_VIDEO_INDEXING |
		DMX_CAP_VIDEO_DECODER_DATA | DMX_CAP_TS_INSERTION |
		DMX_CAP_SECURED_INPUT_PLAYBACK;
	caps->recording_max_video_pids_indexed = TSPP2_MAX_REC_PATTERN_INDEXING;
	caps->num_decoders = MPQ_ADAPTER_MAX_NUM_OF_INTERFACES;
	caps->num_demux_devices = CONFIG_DVB_MPQ_NUM_DMX_DEVICES;
	caps->num_pid_filters = TSPP2_DMX_MAX_PID_FILTER_NUM;
	caps->num_section_filters = dvb_demux->filternum;
	caps->num_section_filters_per_pid = dvb_demux->filternum;
	caps->section_filter_length = DMX_FILTER_SIZE;
	caps->num_demod_inputs = TSPP2_NUM_TSIF_INPUTS;
	caps->num_memory_inputs = TSPP2_NUM_MEM_INPUTS;
	caps->max_bitrate = 320;
	caps->demod_input_max_bitrate = 96;
	caps->memory_input_max_bitrate = 80;
	caps->num_cipher_ops = DMX_MAX_CIPHER_OPERATIONS_COUNT;

	/* TSIF reports 7 bytes STC at unit of 27MHz */
	caps->max_stc = 0x00FFFFFFFFFFFFFFULL;

	caps->section.flags = DMX_BUFFER_EXTERNAL_SUPPORT |
		DMX_BUFFER_INTERNAL_SUPPORT | DMX_BUFFER_CACHED;
	caps->section.max_buffer_num = 1;
	caps->section.max_size = 0xFFFFFFFF;
	caps->section.size_alignment = 0;

	caps->pes.flags = DMX_BUFFER_EXTERNAL_SUPPORT;
	caps->pes.max_buffer_num = 1;
	caps->pes.max_size = TSPP2_DMX_SPS_NON_VID_PES_MAX_BUFF_SIZE;
	caps->pes.size_alignment = TSPP2_DMX_SPS_NON_VID_PES_DESC_SIZE;

	caps->decoder.flags = DMX_BUFFER_SECURED_IF_DECRYPTED |
		DMX_BUFFER_EXTERNAL_SUPPORT | DMX_BUFFER_INTERNAL_SUPPORT;
	caps->decoder.max_buffer_num = 1;
	caps->decoder.max_size = TSPP2_DMX_VPES_PAYLOAD_MAX_BUFF_SIZE;
	caps->decoder.size_alignment = TSPP2_DMX_SPS_VPES_PAYLOAD_DESC_SIZE;

	caps->recording_188_tsp.flags = DMX_BUFFER_SECURED_IF_DECRYPTED |
		DMX_BUFFER_EXTERNAL_SUPPORT;
	caps->recording_188_tsp.max_buffer_num = 1;
	caps->recording_188_tsp.max_size =
		TSPP2_DMX_SPS_188_RECORD_MAX_BUFF_SIZE;
	caps->recording_188_tsp.size_alignment =
		TSPP2_DMX_SPS_188_RECORDING_DESC_SIZE;

	caps->recording_192_tsp.flags = DMX_BUFFER_SECURED_IF_DECRYPTED |
		DMX_BUFFER_EXTERNAL_SUPPORT;
	caps->recording_192_tsp.max_buffer_num = 1;
	caps->recording_192_tsp.max_size =
		TSPP2_DMX_SPS_192_RECORD_MAX_BUFF_SIZE;
	caps->recording_192_tsp.size_alignment =
		TSPP2_DMX_SPS_192_RECORDING_DESC_SIZE;

	caps->playback_188_tsp.flags = DMX_BUFFER_SECURED_IF_DECRYPTED |
		DMX_BUFFER_EXTERNAL_SUPPORT;
	caps->playback_188_tsp.max_buffer_num = 1;
	caps->playback_188_tsp.max_size =
		TSPP2_DMX_SPS_188_MAX_INPUT_BUFF_SIZE;
	caps->playback_188_tsp.size_alignment =
		TSPP2_DMX_SPS_188_INPUT_BUFF_DESC_SIZE;

	caps->playback_192_tsp.flags = DMX_BUFFER_SECURED_IF_DECRYPTED |
		DMX_BUFFER_EXTERNAL_SUPPORT;
	caps->playback_192_tsp.max_buffer_num = 1;
	caps->playback_192_tsp.max_size =
		TSPP2_DMX_SPS_192_MAX_INPUT_BUFF_SIZE;
	caps->playback_192_tsp.size_alignment =
		TSPP2_DMX_SPS_192_INPUT_BUFF_DESC_SIZE;

	return 0;
}

/**
 * Release a single demux device.
 *
 * @mpq_demux: The demux device to release
 *
 * Return error code
 */
static int mpq_dmx_tspp2_release(struct mpq_demux *mpq_demux)
{
	MPQ_DVB_DBG_PRINT("%s executed\n", __func__);

	if (mpq_demux->plugin_priv) {
		vfree(mpq_demux->plugin_priv);
		mpq_demux->plugin_priv = NULL;
	}

	return 0;
}

/**
 * debugfs service to print pipes information.
 */
static int mpq_dmx_tspp2_pipes_print(struct seq_file *s, void *p)
{
	int i;
	int allocated_count = 0;
	unsigned long flags;
	struct pipe_info *pipe_info;
	const char *pipe_types[] = {
		"PCR", "CLEAR_SECTION", "SCRAMBLED_SECTION",
		"FULL_PES", "VPES_HEADER", "VPES_PAYLOAD",
		"REC", "INDEXING", "INPUT_PIPE"};

	if (mutex_lock_interruptible(&mpq_dmx_tspp2_info.mutex))
		return -ERESTARTSYS;

	for (i = 0; i < TSPP2_NUM_PIPES; i++) {
		pipe_info = &mpq_dmx_tspp2_info.pipes[i];
		if (mutex_lock_interruptible(&pipe_info->mutex)) {
			mutex_unlock(&mpq_dmx_tspp2_info.mutex);
			return -ERESTARTSYS;
		}

		spin_lock_irqsave(&pipe_info->lock, flags);
		if (pipe_info->ref_count) {
			seq_printf(s, "PIPE[%d]\n", i);
			seq_puts(s, "--------\n");
			seq_printf(s, "handle         : 0x%x\n",
				pipe_info->handle);
			seq_printf(s, "ref_count      : %d\n",
				pipe_info->ref_count);
			seq_printf(s, "session_id     : %d\n",
				pipe_info->session_id);
			seq_printf(s, "type           : %s\n",
				pipe_types[pipe_info->type]);
			seq_printf(s, "interrupt rate : %d\n",
				pipe_info->hw_notif_rate_hz);
			seq_printf(s, "interrupt count: %d\n",
				pipe_info->hw_notif_count);
			seq_printf(s, "int. miss count: %d\n",
				pipe_info->hw_missed_notif);
			seq_printf(s, "handler count  : %d\n",
				pipe_info->handler_count);
			seq_printf(s, "buffer address : 0x%p(0x%p)\n",
				pipe_info->buffer.mem,
				(void *)pipe_info->buffer.iova);
			seq_printf(s, "buffer size    : %d\n",
				pipe_info->buffer.size);
			seq_printf(s, "SPS config     : 0x%08X\n",
				pipe_info->pipe_cfg.sps_cfg.setting);
			seq_printf(s, "SPS events     : 0x%08X\n",
				pipe_info->pipe_cfg.sps_cfg.wakeup_events);
			seq_printf(s, "SPS desc size  : %d\n",
				pipe_info->pipe_cfg.sps_cfg.descriptor_size);
			seq_printf(s, "TSPP wr offset : %d\n",
				pipe_info->tspp_write_offset);
			seq_printf(s, "TSPP rd offset : %d\n",
				pipe_info->tspp_read_offset);
			seq_printf(s, "BAM rd offset  : %d\n\n",
				pipe_info->bam_read_offset);

			allocated_count++;
		}
		spin_unlock_irqrestore(&pipe_info->lock, flags);
		mutex_unlock(&pipe_info->mutex);
	}

	mutex_unlock(&mpq_dmx_tspp2_info.mutex);

	if (!allocated_count)
		seq_puts(s, "No pipes allocated\n");
	else
		seq_printf(s, "Total pipes: %d\n", allocated_count);

	return 0;
}

/**
 * debugfs service to print filters information.
 */
static int mpq_dmx_tspp2_filters_print(struct seq_file *s, void *p)
{
	int i;
	int active_filters = 0;
	struct mpq_dmx_tspp2_filter *filter;
	struct mpq_dmx_tspp2_filter_op *op;

	const char *op_types[] = {
		"PES analyze", "RAW", "PES", "PCR", "CIPHER", "INDEX", "COPY"};

	if (mutex_lock_interruptible(&mpq_dmx_tspp2_info.mutex))
		return -ERESTARTSYS;

	for (i = 0; i < TSPP2_DMX_MAX_PID_FILTER_NUM; i++) {
		filter = &mpq_dmx_tspp2_info.filters[i];

		if (filter->handle == TSPP2_INVALID_HANDLE)
			continue;

		seq_printf(s, "FILTER[%d]:\n", i);
		seq_puts(s, "--------\n");
		seq_printf(s, "pid          : %d\n", filter->pid);
		seq_printf(s, "handle       : 0x%x\n", filter->handle);
		seq_printf(s, "source handle: 0x%x\n",
			filter->source_info->handle);
		seq_printf(s, "operations   : %d\n", filter->num_ops);
		seq_puts(s, "    ");
		list_for_each_entry(op, &filter->operations_list, next) {
			seq_printf(s, "-> %s", op_types[op->op.type]);
		}
		seq_puts(s, "\n");

		active_filters++;
	}

	mutex_unlock(&mpq_dmx_tspp2_info.mutex);

	if (!active_filters)
		seq_puts(s, "No active filters\n");
	else
		seq_printf(s, "Total filters: %d\n", active_filters);

	return 0;
}

/**
 * debugfs service to print indexing tables information.
 */
static int mpq_dmx_tspp2_index_tables_print(struct seq_file *s, void *p)
{
	int i;
	int j;
	struct mpq_tspp2_index_table *table;

	if (mutex_lock_interruptible(&mpq_dmx_tspp2_info.mutex))
		return -ERESTARTSYS;

	for (i = 0; i < TSPP2_NUM_INDEXING_TABLES; i++) {
		table = &mpq_dmx_tspp2_info.index_tables[i];

		seq_printf(s, "Index Table[%d] - %d patterns:\n",
			i, table->num_patterns);
		seq_puts(s, "------------------------------\n");
		for (j = 0; j < table->num_patterns; j++) {
			seq_printf(s, "type=0x%08llx value=0x0%x mask=0x%0x\n",
				table->patterns[j].type,
				table->patterns[j].value,
				table->patterns[j].mask);
		}
		seq_puts(s, "\n");
	}

	mutex_unlock(&mpq_dmx_tspp2_info.mutex);

	return 0;
}

/**
 * debugfs service to print sources information.
 */
static int mpq_dmx_tspp2_sources_print(struct seq_file *s, void *p)
{
	int i = 0;
	struct source_info *src;

	if (mutex_lock_interruptible(&mpq_dmx_tspp2_info.mutex))
		return -ERESTARTSYS;

	while (i <= TSPP2_DMX_SOURCE_COUNT) {
		if (i < TSPP2_DMX_SOURCE_COUNT)
			src = &mpq_dmx_tspp2_info.source[i];
		else
			src = &mpq_dmx_tspp2_info.ts_insertion_source;

		seq_printf(s, "Demux Source[%d]: %s\n", i, src->name);
		seq_puts(s, "-------------------------------\n");
		seq_printf(s, "Type        : %s\n",
			src->type == DEMUXING_SOURCE ?
				"demuxing" : "ts insertion");
		seq_printf(s, "TSPP2 handle: 0x%0x\n", src->handle);
		seq_printf(s, "Ref. count  : %u\n", src->ref_count);
		seq_printf(s, "Enabled     : %d\n", src->enabled);
		seq_printf(s, "Input pipe  : 0x%x\n",
			src->input_pipe == NULL ? 0 : src->input_pipe->handle);
		seq_printf(s, "TSP format  : %s\n",
			src->tsp_format == TSPP2_PACKET_FORMAT_188_RAW ? "188" :
			src->tsp_format == TSPP2_PACKET_FORMAT_192_HEAD ?
				"192 Head" : "192 Tail");
		i++;
	}
	seq_puts(s, "\n");

	mutex_unlock(&mpq_dmx_tspp2_info.mutex);

	return 0;
}

/**
 * debugfs service to print pipes, filters, index tables and sources information
 */
static int mpq_dmx_tspp2_pipes_open(struct inode *inode, struct file *file)
{
	return single_open(file, mpq_dmx_tspp2_pipes_print, inode->i_private);
}

static int mpq_dmx_tspp2_filters_open(struct inode *inode, struct file *file)
{
	return single_open(file, mpq_dmx_tspp2_filters_print, inode->i_private);
}

static int mpq_dmx_tspp2_index_tables_open(struct inode *inode,
	struct file *file)
{
	return single_open(file, mpq_dmx_tspp2_index_tables_print,
		inode->i_private);
}

static int mpq_dmx_tspp2_sources_open(struct inode *inode,
	struct file *file)
{
	return single_open(file, mpq_dmx_tspp2_sources_print, inode->i_private);
}

static const struct file_operations dbgfs_pipes_fops = {
	.open = mpq_dmx_tspp2_pipes_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static const struct file_operations dbgfs_filters_fops = {
	.open = mpq_dmx_tspp2_filters_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static const struct file_operations dbgfs_index_tables_fops = {
	.open = mpq_dmx_tspp2_index_tables_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};

static const struct file_operations dbgfs_sources_fops = {
	.open = mpq_dmx_tspp2_sources_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.owner = THIS_MODULE,
};


/**
 * Initialize a single demux device.
 *
 * @mpq_adapter: MPQ DVB adapter
 * @mpq_demux: The demux device to initialize
 *
 * Return     error code
 */
static int mpq_dmx_tsppv2_init(struct dvb_adapter *mpq_adapter,
			struct mpq_demux *mpq_demux)
{
	int result;
	int i;
	struct mpq_tspp2_demux *mpq_tspp2_demux;

	MPQ_DVB_DBG_PRINT("%s executed\n", __func__);

	/* Set the kernel-demux object capabilities */
	mpq_demux->demux.dmx.capabilities =
		DMX_TS_FILTERING		|
		DMX_PES_FILTERING		|
		DMX_SECTION_FILTERING		|
		DMX_MEMORY_BASED_FILTERING	|
		DMX_CRC_CHECKING		|
		DMX_TS_DESCRAMBLING;

	/*
	 * TSPPv2 buffers allocated internally by demux cannot be allocated
	 * with cache enabled.
	 */
	mpq_demux->decoder_alloc_flags = 0;

	/* Set dvb-demux "virtual" function pointers */
	mpq_demux->demux.priv = (void *)mpq_demux;
	mpq_demux->demux.filternum = TSPP2_DMX_MAX_PID_FILTER_NUM;
	mpq_demux->demux.feednum = MPQ_MAX_DMX_FILES;
	mpq_demux->demux.start_feed = mpq_dmx_tspp2_start_filtering;
	mpq_demux->demux.stop_feed = mpq_dmx_tspp2_stop_filtering;
	mpq_demux->demux.write_to_decoder = NULL;
	mpq_demux->demux.decoder_fullness_init = NULL;
	mpq_demux->demux.decoder_fullness_wait = NULL;
	mpq_demux->demux.decoder_fullness_abort = NULL;
	mpq_demux->demux.decoder_buffer_status = mpq_dmx_decoder_buffer_status;
	mpq_demux->demux.reuse_decoder_buffer =
		mpq_dmx_tspp2_reuse_decoder_buffer;
	mpq_demux->demux.set_cipher_op = NULL;
	mpq_demux->demux.oob_command = NULL;
	mpq_demux->demux.set_indexing = mpq_dmx_tspp2_set_indexing;
	mpq_demux->demux.convert_ts = mpq_dmx_tspp2_convert_ts;
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

	mpq_demux->dmxdev.demux->set_source = mpq_dmx_tspp2_set_source;
	mpq_demux->dmxdev.demux->get_caps = mpq_dmx_tspp2_get_caps;
	mpq_demux->dmxdev.demux->connect_frontend =
		mpq_dmx_tspp2_connect_frontend;
	mpq_demux->dmxdev.demux->disconnect_frontend =
		mpq_dmx_tspp2_disconnect_frontend;
	mpq_demux->dmxdev.demux->write = mpq_dmx_tspp2_write;
	mpq_demux->dmxdev.demux->write_cancel = mpq_dmx_tspp2_write_cancel;
	mpq_demux->dmxdev.demux->map_buffer = mpq_dmx_tspp2_map_buffer;
	mpq_demux->dmxdev.demux->unmap_buffer = mpq_dmx_unmap_buffer;
	mpq_demux->dmxdev.demux->get_stc = NULL;

	result = dvb_dmxdev_init(&mpq_demux->dmxdev, mpq_adapter);
	if (result < 0) {
		MPQ_DVB_ERR_PRINT("%s: dvb_dmxdev_init failed (errno=%d)\n",
					__func__, result);

		goto init_failed_dvbdmx_release;
	}

	mpq_tspp2_demux = vzalloc(sizeof(struct mpq_tspp2_demux));
	if (!mpq_tspp2_demux) {
		result = -ENOMEM;
		goto init_failed_dmxdev_release;
	}
	mpq_demux->mpq_dmx_plugin_release = mpq_dmx_tspp2_release;
	mpq_demux->plugin_priv = mpq_tspp2_demux;
	mpq_tspp2_demux->mpq_demux = mpq_demux;

	/* Default source is the matching DVR */
	mpq_tspp2_demux->source_info = mpq_dmx_get_source(mpq_demux->source);

	for (i = 0; i < mpq_demux->demux.feednum; i++) {
		mpq_tspp2_demux->feeds[i].main_pipe = NULL;
		mpq_tspp2_demux->feeds[i].secondary_pipe = NULL;
		mpq_tspp2_demux->feeds[i].op_count = 0;
		mpq_tspp2_demux->feeds[i].index_table =
			TSPP2_NUM_INDEXING_TABLES;
		mpq_tspp2_demux->feeds[i].mpq_feed = &mpq_demux->feeds[i];
		mpq_demux->feeds[i].plugin_priv = &mpq_tspp2_demux->feeds[i];
	}

	/* Extend dvb-demux debugfs with TSPP statistics. */
	mpq_dmx_init_debugfs_entries(mpq_demux);

	return 0;

init_failed_dmxdev_release:
	dvb_dmxdev_release(&mpq_demux->dmxdev);
init_failed_dvbdmx_release:
	dvb_dmx_release(&mpq_demux->demux);
init_failed:
	return result;
}

static void mpq_dmx_tspp2_plugin_terminate(void)
{
	int i;

	MPQ_DVB_DBG_PRINT("%s: entry\n", __func__);

	del_timer_sync(&mpq_dmx_tspp2_info.polling_timer.handle);
	for (i = 0; i < TSPP2_DMX_SOURCE_COUNT; i++) {
		MPQ_DVB_DBG_PRINT("%s: [%d] Stopping thread %s...\n",
			__func__, i, mpq_dmx_tspp2_info.source[i].name);
		kthread_stop(mpq_dmx_tspp2_info.source[i].demux_src.thread);
	}

	if (mpq_dmx_tspp2_info.debugfs_dmx_dir != NULL)
		debugfs_remove_recursive(mpq_dmx_tspp2_info.debugfs_dmx_dir);

	for (i = 0; i < TSPP2_NUM_PIPES; i++)
		mutex_destroy(&mpq_dmx_tspp2_info.pipes[i].mutex);

	mutex_destroy(&mpq_dmx_tspp2_info.mutex);

	mpq_dmx_plugin_exit();
}

static int __init mpq_dmx_tspp2_plugin_init(void)
{
	int i;
	int j;
	int ret;
	struct source_info *source_info;
	struct mpq_dmx_tspp2_filter *filter;

	init_timer(&mpq_dmx_tspp2_info.polling_timer.handle);
	mpq_dmx_tspp2_info.polling_timer.ref_count = 0;
	mpq_dmx_tspp2_info.polling_timer.handle.function = mpq_dmx_timer_cb;

	memset(mpq_dmx_tspp2_info.filters, 0, TSPP2_DMX_MAX_PID_FILTER_NUM *
		sizeof(struct mpq_dmx_tspp2_filter));
	for (i = 0; i < TSPP2_DMX_MAX_PID_FILTER_NUM; i++) {
		filter = &mpq_dmx_tspp2_info.filters[i];
		filter->handle = TSPP2_INVALID_HANDLE;
		INIT_LIST_HEAD(&filter->operations_list);
	}

	for (i = 0; i < TSPP2_DMX_SOURCE_COUNT; i++) {
		source_info = &mpq_dmx_tspp2_info.source[i];
		source_info->handle = TSPP2_INVALID_HANDLE;
		source_info->type = DEMUXING_SOURCE;
		source_info->ref_count = 0;
		source_info->input_pipe = NULL;
		source_info->tsp_format = TSPP2_PACKET_FORMAT_188_RAW;
		source_info->demux_src.clear_section_pipe = NULL;
		source_info->demux_src.scrambled_section_pipe = NULL;
		source_info->enabled = 0;

		snprintf(source_info->name, TSPP2_DMX_SOURCE_NAME_LENGTH,
			(i < TSPP2_NUM_TSIF_INPUTS) ? "tsif_%d" : "bam_%d",
			(i < TSPP2_NUM_TSIF_INPUTS) ? i :
				(i - TSPP2_NUM_TSIF_INPUTS));

		init_completion(&source_info->completion);

		init_waitqueue_head(&source_info->demux_src.wait_queue);

		/* Initialize source processing thread */
		source_info->demux_src.thread =
			kthread_run(mpq_dmx_tspp2_thread, (void *)source_info,
				source_info->name);
		if (IS_ERR(source_info->demux_src.thread)) {
			for (j = 0; j < i; j++) {
				kthread_stop(
					source_info->demux_src.thread);
			}
			MPQ_DVB_ERR_PRINT("%s: kthread_run failed\n", __func__);
			return -ENOMEM;
		}
	}

	source_info = &mpq_dmx_tspp2_info.ts_insertion_source;
	source_info->type = TS_INSERTION_SOURCE;
	snprintf(source_info->name, TSPP2_DMX_SOURCE_NAME_LENGTH, "ts_insert");
	source_info->ref_count = 0;
	source_info->handle = TSPP2_INVALID_HANDLE;
	init_completion(&source_info->completion);
	source_info->input_pipe = NULL;
	source_info->tsp_format = TSPP2_PACKET_FORMAT_188_RAW;
	source_info->insert_src.filter = NULL;

	memset(mpq_dmx_tspp2_info.pipes, 0,
		TSPP2_NUM_PIPES*sizeof(struct pipe_info));
	for (i = 0; i < TSPP2_NUM_PIPES; i++) {
		mpq_dmx_tspp2_info.pipes[i].handle = TSPP2_INVALID_HANDLE;
		mutex_init(&mpq_dmx_tspp2_info.pipes[i].mutex);
		pipe_work_queue_init(&mpq_dmx_tspp2_info.pipes[i].work_queue);
		spin_lock_init(&mpq_dmx_tspp2_info.pipes[i].lock);
	}
	mpq_dmx_tspp2_info.user_count = 0;
	mutex_init(&mpq_dmx_tspp2_info.mutex);

	ret = mpq_dmx_plugin_init(mpq_dmx_tsppv2_init);

	if (ret < 0) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_dmx_plugin_init failed (errno=%d)\n",
			__func__, ret);

		for (i = 0; i < TSPP2_NUM_PIPES; i++)
			mutex_destroy(&mpq_dmx_tspp2_info.pipes[i].mutex);

		for (i = 0; i < TSPP2_DMX_SOURCE_COUNT; i++) {
			MPQ_DVB_DBG_PRINT("%s: [%d] Stopping thread %s...\n",
				__func__, i, mpq_dmx_tspp2_info.source[i].name);
			kthread_stop(
				mpq_dmx_tspp2_info.source[i].demux_src.thread);
		}

		mutex_destroy(&mpq_dmx_tspp2_info.mutex);

		return ret;
	}

	/* new debugfs directory holding global demux info */
	mpq_dmx_tspp2_info.debugfs_dmx_dir =
		debugfs_create_dir("tspp2_demux", NULL);
	if (mpq_dmx_tspp2_info.debugfs_dmx_dir != NULL) {
		mpq_dmx_tspp2_info.debugfs_pipes_file =
			debugfs_create_file("pipes",
				S_IRUGO,
				mpq_dmx_tspp2_info.debugfs_dmx_dir,
				NULL,
				&dbgfs_pipes_fops);
		mpq_dmx_tspp2_info.debugfs_filters_file =
			debugfs_create_file("filters",
				S_IRUGO,
				mpq_dmx_tspp2_info.debugfs_dmx_dir,
				NULL,
				&dbgfs_filters_fops);
		mpq_dmx_tspp2_info.debugfs_index_tables_file =
			debugfs_create_file("index_tables",
				S_IRUGO,
				mpq_dmx_tspp2_info.debugfs_dmx_dir,
				NULL,
				&dbgfs_index_tables_fops);
		mpq_dmx_tspp2_info.debugfs_sources_file =
			debugfs_create_file("sources",
				S_IRUGO,
				mpq_dmx_tspp2_info.debugfs_dmx_dir,
				NULL,
				&dbgfs_sources_fops);
	}

	return ret;
}

static void __exit mpq_dmx_tspp2_plugin_exit(void)
{
	MPQ_DVB_DBG_PRINT("%s: entry\n", __func__);
	mpq_dmx_tspp2_plugin_terminate();
}

module_init(mpq_dmx_tspp2_plugin_init);
module_exit(mpq_dmx_tspp2_plugin_exit);

MODULE_DESCRIPTION("Qualcomm demux TSPP version2 HW Plugin");
MODULE_LICENSE("GPL v2");
