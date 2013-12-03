/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/file.h>
#include <linux/scatterlist.h>
#include "mpq_dvb_debug.h"
#include "mpq_dmx_plugin_common.h"
#include "mpq_sdmx.h"

#define SDMX_MAJOR_VERSION_MATCH	(4)

#define TS_PACKET_HEADER_LENGTH (4)

/* Length of mandatory fields that must exist in header of video PES */
#define PES_MANDATORY_FIELDS_LEN			9

/* Index of first byte in TS packet holding STC */
#define STC_LOCATION_IDX			188

#define MAX_PES_LENGTH	(SZ_64K)

#define MAX_TS_PACKETS_FOR_SDMX_PROCESS	(500)

/*
 * PES header length field is 8 bits so PES header length after this field
 * can be up to 256 bytes.
 * Preceding fields of the PES header total to 9 bytes
 * (including the PES header length field).
 */
#define MAX_PES_HEADER_LENGTH	(256 + PES_MANDATORY_FIELDS_LEN)

/* TS packet with adaptation field only can take up the entire TSP */
#define MAX_TSP_ADAPTATION_LENGTH (184)

#define MAX_SDMX_METADATA_LENGTH	\
	(TS_PACKET_HEADER_LENGTH +	\
	MAX_TSP_ADAPTATION_LENGTH +	\
	MAX_PES_HEADER_LENGTH)

#define SDMX_METADATA_BUFFER_SIZE	(64*1024)
#define SDMX_SECTION_BUFFER_SIZE	(64*1024)
#define SDMX_PCR_BUFFER_SIZE		(64*1024)

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


/* Number of demux devices, has default of linux configuration */
static int mpq_demux_device_num = CONFIG_DVB_MPQ_NUM_DMX_DEVICES;
module_param(mpq_demux_device_num, int, S_IRUGO);

/* ION heap IDs used for allocating video output buffer */
static int video_secure_ion_heap = ION_CP_MM_HEAP_ID;
module_param(video_secure_ion_heap , int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(video_secure_ion_heap, "ION heap for secure video buffer allocation");

static int video_nonsecure_ion_heap = ION_IOMMU_HEAP_ID;
module_param(video_nonsecure_ion_heap, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(video_nonsecure_ion_heap, "ION heap for non-secure video buffer allocation");

/* Value of TS packet scramble bits field for even key */
static int mpq_sdmx_scramble_even = 0x2;
module_param(mpq_sdmx_scramble_even, int, S_IRUGO | S_IWUSR);

/* Value of TS packet scramble bits field for odd key */
static int mpq_sdmx_scramble_odd = 0x3;
module_param(mpq_sdmx_scramble_odd, int, S_IRUGO | S_IWUSR);

/* Whether to use secure demux or bypass it. Use for debugging */
static int mpq_bypass_sdmx = 1;
module_param(mpq_bypass_sdmx, int, S_IRUGO | S_IWUSR);

/* Max number of TS packets allowed as input for a single sdmx process */
static int mpq_sdmx_proc_limit = MAX_TS_PACKETS_FOR_SDMX_PROCESS;
module_param(mpq_sdmx_proc_limit, int, S_IRUGO | S_IWUSR);

/* Debug flag for secure demux process */
static int mpq_sdmx_debug;
module_param(mpq_sdmx_debug, int, S_IRUGO | S_IWUSR);

/*
 * Indicates whether the demux should search for frame boundaries
 * and notify on video packets on frame-basis or whether to provide
 * only video PES packet payloads as-is.
 */
static int video_framing = 1;
module_param(video_framing, int, S_IRUGO | S_IWUSR);

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

	/* Indicates whether secure demux TZ application is available */
	int secure_demux_app_loaded;
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

/* Check if a framing pattern is a video frame pattern or a header pattern */
static inline int mpq_dmx_is_video_frame(
				enum dmx_video_codec codec,
				u64 pattern_type)
{
	switch (codec) {
	case DMX_VIDEO_CODEC_MPEG2:
		if ((pattern_type == DMX_IDX_MPEG_I_FRAME_START) ||
			(pattern_type == DMX_IDX_MPEG_P_FRAME_START) ||
			(pattern_type == DMX_IDX_MPEG_B_FRAME_START))
			return 1;
		return 0;

	case DMX_VIDEO_CODEC_H264:
		if ((pattern_type == DMX_IDX_H264_IDR_START) ||
			(pattern_type == DMX_IDX_H264_NON_IDR_START))
			return 1;
		return 0;

	case DMX_VIDEO_CODEC_VC1:
		if (pattern_type == DMX_IDX_VC1_FRAME_START)
			return 1;
		return 0;

	default:
		return -EINVAL;
	}
}

/*
 * mpq_dmx_get_pattern_params - Returns the required video
 * patterns for framing operation based on video codec.
 *
 * @video_codec: the video codec.
 * @patterns: a pointer to the pattern parameters, updated by this function.
 * @patterns_num: number of patterns, updated by this function.
 */
static inline int mpq_dmx_get_pattern_params(
	enum dmx_video_codec video_codec,
	const struct dvb_dmx_video_patterns
		 *patterns[DVB_DMX_MAX_SEARCH_PATTERN_NUM],
	int *patterns_num)
{
	switch (video_codec) {
	case DMX_VIDEO_CODEC_MPEG2:
		patterns[0] = dvb_dmx_get_pattern(DMX_IDX_MPEG_SEQ_HEADER);
		patterns[1] = dvb_dmx_get_pattern(DMX_IDX_MPEG_GOP);
		patterns[2] = dvb_dmx_get_pattern(DMX_IDX_MPEG_I_FRAME_START);
		patterns[3] = dvb_dmx_get_pattern(DMX_IDX_MPEG_P_FRAME_START);
		patterns[4] = dvb_dmx_get_pattern(DMX_IDX_MPEG_B_FRAME_START);
		*patterns_num = 5;
		break;

	case DMX_VIDEO_CODEC_H264:
		patterns[0] = dvb_dmx_get_pattern(DMX_IDX_H264_SPS);
		patterns[1] = dvb_dmx_get_pattern(DMX_IDX_H264_PPS);
		patterns[2] = dvb_dmx_get_pattern(DMX_IDX_H264_IDR_START);
		patterns[3] = dvb_dmx_get_pattern(DMX_IDX_H264_NON_IDR_START);
		patterns[4] = dvb_dmx_get_pattern(DMX_IDX_H264_SEI);
		*patterns_num = 5;
		break;

	case DMX_VIDEO_CODEC_VC1:
		patterns[0] = dvb_dmx_get_pattern(DMX_IDX_VC1_SEQ_HEADER);
		patterns[1] = dvb_dmx_get_pattern(DMX_IDX_VC1_ENTRY_POINT);
		patterns[2] = dvb_dmx_get_pattern(DMX_IDX_VC1_FRAME_START);
		*patterns_num = 3;
		break;

	default:
		MPQ_DVB_ERR_PRINT("%s: invalid parameters\n", __func__);
		*patterns_num = 0;
		return -EINVAL;
	}

	return 0;
}

/*
 * mpq_dmx_calc_time_delta -
 * Calculate delta in msec between two time snapshots.
 *
 * @curr_time: value of current time
 * @prev_time: value of previous time
 *
 * Return	time-delta in msec
 */
static inline u32 mpq_dmx_calc_time_delta(struct timespec *curr_time,
	struct timespec *prev_time)
{
	struct timespec delta_time;
	u64 delta_time_ms;

	delta_time = timespec_sub(*curr_time, *prev_time);

	delta_time_ms = ((u64)delta_time.tv_sec * MSEC_PER_SEC) +
		delta_time.tv_nsec / NSEC_PER_MSEC;

	return (u32)delta_time_ms;
}

/*
 * mpq_dmx_update_decoder_stat -
 * Update decoder output statistics in debug-fs.
 *
 * @mpq_demux: mpq_demux object
 */
static inline void mpq_dmx_update_decoder_stat(struct mpq_demux *mpq_demux)
{
	struct timespec curr_time;
	u64 delta_time_ms;

	curr_time = current_kernel_time();
	if (unlikely(!mpq_demux->decoder_out_count)) {
		mpq_demux->decoder_out_last_time = curr_time;
		mpq_demux->decoder_out_count++;
		return;
	}

	/* calculate time-delta between frame */
	delta_time_ms = mpq_dmx_calc_time_delta(&curr_time,
		&mpq_demux->decoder_out_last_time);

	mpq_demux->decoder_out_interval_sum += (u32)delta_time_ms;

	mpq_demux->decoder_out_interval_average =
	  mpq_demux->decoder_out_interval_sum /
	  mpq_demux->decoder_out_count;

	if (delta_time_ms > mpq_demux->decoder_out_interval_max)
		mpq_demux->decoder_out_interval_max = delta_time_ms;

	mpq_demux->decoder_out_last_time = curr_time;
	mpq_demux->decoder_out_count++;
}

/*
 * mpq_dmx_update_sdmx_stat -
 * Update SDMX statistics in debug-fs.
 *
 * @mpq_demux: mpq_demux object
 * @bytes_processed: number of bytes processed by sdmx
 * @process_start_time: time before sdmx process was triggered
 * @process_end_time: time after sdmx process finished
 */
static inline void mpq_dmx_update_sdmx_stat(struct mpq_demux *mpq_demux,
		u32 bytes_processed, struct timespec *process_start_time,
		struct timespec *process_end_time)
{
	u32 packets_num;
	u64 process_time;

	mpq_demux->sdmx_process_count++;
	packets_num = bytes_processed / mpq_demux->demux.ts_packet_size;
	mpq_demux->sdmx_process_packets_sum += packets_num;
	mpq_demux->sdmx_process_packets_average =
		mpq_demux->sdmx_process_packets_sum /
		mpq_demux->sdmx_process_count;

	process_time =
		mpq_dmx_calc_time_delta(process_end_time, process_start_time);

	mpq_demux->sdmx_process_time_sum += process_time;
	mpq_demux->sdmx_process_time_average =
		mpq_demux->sdmx_process_time_sum /
		mpq_demux->sdmx_process_count;

	if ((mpq_demux->sdmx_process_count == 1) ||
		(packets_num < mpq_demux->sdmx_process_packets_min))
		mpq_demux->sdmx_process_packets_min = packets_num;

	if ((mpq_demux->sdmx_process_count == 1) ||
		(process_time > mpq_demux->sdmx_process_time_max))
		mpq_demux->sdmx_process_time_max = process_time;
}

static int mpq_sdmx_log_level_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t mpq_sdmx_log_level_read(struct file *fp,
	char __user *user_buffer, size_t count, loff_t *position)
{
	char user_str[16];
	struct mpq_demux *mpq_demux = fp->private_data;
	int ret;

	ret = scnprintf(user_str, 16, "%d", mpq_demux->sdmx_log_level);
	ret = simple_read_from_buffer(user_buffer, count, position,
		user_str, ret+1);

	return ret;
}

static ssize_t mpq_sdmx_log_level_write(struct file *fp,
	const char __user *user_buffer, size_t count, loff_t *position)
{
	char user_str[16];
	int ret;
	int ret_count;
	int level;
	struct mpq_demux *mpq_demux = fp->private_data;

	if (count >= 16)
		return -EINVAL;

	ret_count = simple_write_to_buffer(user_str, 16, position, user_buffer,
		count);
	if (ret_count < 0)
		return ret_count;

	ret = sscanf(user_str, "%d", &level);
	if (ret != 1)
		return -EINVAL;

	if (level < SDMX_LOG_NO_PRINT || level > SDMX_LOG_VERBOSE)
		return -EINVAL;

	mutex_lock(&mpq_demux->mutex);
	mpq_demux->sdmx_log_level = level;
	if (mpq_demux->sdmx_session_handle != SDMX_INVALID_SESSION_HANDLE) {
		ret = sdmx_set_log_level(mpq_demux->sdmx_session_handle,
			mpq_demux->sdmx_log_level);
		if (ret) {
			MPQ_DVB_ERR_PRINT(
				"%s: Could not set sdmx log level. ret = %d\n",
				__func__, ret);
			mutex_unlock(&mpq_demux->mutex);
			return -EINVAL;
		}
	}

	mutex_unlock(&mpq_demux->mutex);
	return ret_count;
}

static const struct file_operations sdmx_debug_fops = {
	.open = mpq_sdmx_log_level_open,
	.read = mpq_sdmx_log_level_read,
	.write = mpq_sdmx_log_level_write,
	.owner = THIS_MODULE,
};

/* Extend dvb-demux debugfs with common plug-in entries */
void mpq_dmx_init_debugfs_entries(struct mpq_demux *mpq_demux)
{
	/*
	 * Extend dvb-demux debugfs with HW statistics.
	 * Note that destruction of debugfs directory is done
	 * when dvb-demux is terminated.
	 */
	mpq_demux->hw_notification_count = 0;
	mpq_demux->hw_notification_interval = 0;
	mpq_demux->hw_notification_size = 0;
	mpq_demux->hw_notification_min_size = 0xFFFFFFFF;

	if (mpq_demux->demux.dmx.debugfs_demux_dir == NULL)
		return;

	debugfs_create_u32(
		"hw_notification_interval",
		S_IRUGO | S_IWUSR | S_IWGRP,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		&mpq_demux->hw_notification_interval);

	debugfs_create_u32(
		"hw_notification_min_interval",
		S_IRUGO | S_IWUSR | S_IWGRP,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		&mpq_demux->hw_notification_min_interval);

	debugfs_create_u32(
		"hw_notification_count",
		S_IRUGO | S_IWUSR | S_IWGRP,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		&mpq_demux->hw_notification_count);

	debugfs_create_u32(
		"hw_notification_size",
		S_IRUGO | S_IWUSR | S_IWGRP,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		&mpq_demux->hw_notification_size);

	debugfs_create_u32(
		"hw_notification_min_size",
		S_IRUGO | S_IWUSR | S_IWGRP,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		&mpq_demux->hw_notification_min_size);

	debugfs_create_u32(
		"decoder_drop_count",
		S_IRUGO | S_IWUSR | S_IWGRP,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		&mpq_demux->decoder_drop_count);

	debugfs_create_u32(
		"decoder_out_count",
		S_IRUGO | S_IWUSR | S_IWGRP,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		&mpq_demux->decoder_out_count);

	debugfs_create_u32(
		"decoder_out_interval_sum",
		S_IRUGO | S_IWUSR | S_IWGRP,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		&mpq_demux->decoder_out_interval_sum);

	debugfs_create_u32(
		"decoder_out_interval_average",
		S_IRUGO | S_IWUSR | S_IWGRP,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		&mpq_demux->decoder_out_interval_average);

	debugfs_create_u32(
		"decoder_out_interval_max",
		S_IRUGO | S_IWUSR | S_IWGRP,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		&mpq_demux->decoder_out_interval_max);

	debugfs_create_u32(
		"decoder_ts_errors",
		S_IRUGO | S_IWUSR | S_IWGRP,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		&mpq_demux->decoder_ts_errors);

	debugfs_create_u32(
		"decoder_cc_errors",
		S_IRUGO | S_IWUSR | S_IWGRP,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		&mpq_demux->decoder_cc_errors);

	debugfs_create_u32(
		"sdmx_process_count",
		S_IRUGO | S_IWUSR | S_IWGRP,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		&mpq_demux->sdmx_process_count);

	debugfs_create_u32(
		"sdmx_process_time_sum",
		S_IRUGO | S_IWUSR | S_IWGRP,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		&mpq_demux->sdmx_process_time_sum);

	debugfs_create_u32(
		"sdmx_process_time_average",
		S_IRUGO | S_IWUSR | S_IWGRP,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		&mpq_demux->sdmx_process_time_average);

	debugfs_create_u32(
		"sdmx_process_time_max",
		S_IRUGO | S_IWUSR | S_IWGRP,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		&mpq_demux->sdmx_process_time_max);

	debugfs_create_u32(
		"sdmx_process_packets_sum",
		S_IRUGO | S_IWUSR | S_IWGRP,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		&mpq_demux->sdmx_process_packets_sum);

	debugfs_create_u32(
		"sdmx_process_packets_average",
		S_IRUGO | S_IWUSR | S_IWGRP,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		&mpq_demux->sdmx_process_packets_average);

	debugfs_create_u32(
		"sdmx_process_packets_min",
		S_IRUGO | S_IWUSR | S_IWGRP,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		&mpq_demux->sdmx_process_packets_min);

	debugfs_create_file("sdmx_log_level",
		S_IRUGO | S_IWUSR | S_IWGRP,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		mpq_demux,
		&sdmx_debug_fops);
}
EXPORT_SYMBOL(mpq_dmx_init_debugfs_entries);

/* Update dvb-demux debugfs with HW notification statistics */
void mpq_dmx_update_hw_statistics(struct mpq_demux *mpq_demux)
{
	struct timespec curr_time;
	u64 delta_time_ms;

	curr_time = current_kernel_time();
	if (likely(mpq_demux->hw_notification_count)) {
		/* calculate time-delta between notifications */
		delta_time_ms = mpq_dmx_calc_time_delta(&curr_time,
			&mpq_demux->last_notification_time);

		mpq_demux->hw_notification_interval = delta_time_ms;

		if ((mpq_demux->hw_notification_count == 1) ||
			(mpq_demux->hw_notification_interval &&
			 mpq_demux->hw_notification_interval <
				mpq_demux->hw_notification_min_interval))
			mpq_demux->hw_notification_min_interval =
				mpq_demux->hw_notification_interval;
	}

	mpq_demux->hw_notification_count++;
	mpq_demux->last_notification_time = curr_time;
}
EXPORT_SYMBOL(mpq_dmx_update_hw_statistics);

static void mpq_sdmx_check_app_loaded(void)
{
	int session;
	u32 version;
	int ret;

	ret = sdmx_open_session(&session);
	if (ret != SDMX_SUCCESS) {
		MPQ_DVB_ERR_PRINT(
			"%s: Could not initialize session with SDMX. ret = %d\n",
			__func__, ret);
		mpq_dmx_info.secure_demux_app_loaded = 0;
		return;
	}

	/* Check proper sdmx major version */
	ret = sdmx_get_version(session, &version);
	if (ret != SDMX_SUCCESS) {
		MPQ_DVB_ERR_PRINT(
			"%s: Could not get sdmx version. ret = %d\n",
			__func__, ret);
	} else {
		if ((version >> 8) != SDMX_MAJOR_VERSION_MATCH)
			MPQ_DVB_ERR_PRINT(
				"%s: sdmx major version does not match. expected=%d, actual=%d\n",
				__func__, SDMX_MAJOR_VERSION_MATCH,
				(version >> 8));
		else
			MPQ_DVB_DBG_PRINT(
				"%s: sdmx major version is ok = %d\n",
				__func__, SDMX_MAJOR_VERSION_MATCH);
	}

	mpq_dmx_info.secure_demux_app_loaded = 1;
	sdmx_close_session(session);
}

int mpq_dmx_plugin_init(mpq_dmx_init dmx_init_func)
{
	int i;
	int j;
	int result;
	struct mpq_demux *mpq_demux;
	struct dvb_adapter *mpq_adapter;
	struct mpq_feed *feed;

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

	mpq_dmx_info.secure_demux_app_loaded = 0;

	/* Allocate memory for all MPQ devices */
	mpq_dmx_info.devices =
		vzalloc(mpq_demux_device_num*sizeof(struct mpq_demux));

	if (!mpq_dmx_info.devices) {
		MPQ_DVB_ERR_PRINT(
				"%s: failed to allocate devices memory\n",
				__func__);

		result = -ENOMEM;
		goto init_failed;
	}

	/*
	 * Create a new ION client used by demux to allocate memory
	 * for decoder's buffers.
	 */
	mpq_dmx_info.ion_client =
		msm_ion_client_create(UINT_MAX, "demux_client");

	if (IS_ERR_OR_NULL(mpq_dmx_info.ion_client)) {
		MPQ_DVB_ERR_PRINT(
				"%s: msm_ion_client_create\n",
				__func__);

		result = PTR_ERR(mpq_dmx_info.ion_client);
		if (!result)
			result = -ENOMEM;
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

		mutex_init(&mpq_demux->mutex);

		mpq_demux->num_secure_feeds = 0;
		mpq_demux->num_active_feeds = 0;
		mpq_demux->sdmx_filter_count = 0;
		mpq_demux->sdmx_session_handle = SDMX_INVALID_SESSION_HANDLE;
		mpq_demux->sdmx_eos = 0;
		mpq_demux->sdmx_log_level = SDMX_LOG_NO_PRINT;

		if (mpq_demux->demux.feednum > MPQ_MAX_DMX_FILES) {
			MPQ_DVB_ERR_PRINT(
				"%s: err - actual feednum (%d) larger than max, enlarge MPQ_MAX_DMX_FILES!\n",
				__func__,
				mpq_demux->demux.feednum);
			result = -EINVAL;
			goto init_failed_free_demux_devices;
		}

		/* Initialize private feed info */
		for (j = 0; j < MPQ_MAX_DMX_FILES; j++) {
			feed = &mpq_demux->feeds[j];
			memset(feed, 0, sizeof(*feed));
			feed->sdmx_filter_handle = SDMX_INVALID_FILTER_HANDLE;
			feed->mpq_demux = mpq_demux;
			feed->session_id = 0;
		}

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
		 * dvb-demux is now initialized,
		 * update back-pointers of private feeds
		 */
		for (j = 0; j < MPQ_MAX_DMX_FILES; j++) {
			feed = &mpq_demux->feeds[j];
			feed->dvb_demux_feed = &mpq_demux->demux.feed[j];
			mpq_demux->demux.feed[j].priv = feed;
		}

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
			mpq_demux = mpq_dmx_info.devices + i;

			if (mpq_demux->is_initialized) {
				mpq_demux->demux.dmx.remove_frontend(
							&mpq_demux->demux.dmx,
							&mpq_demux->fe_memory);

				if (mpq_sdmx_is_loaded())
					mpq_sdmx_close_session(mpq_demux);
				mutex_destroy(&mpq_demux->mutex);
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
	struct dvb_demux *dvb_demux = demux->priv;
	struct mpq_demux *mpq_demux;

	if ((mpq_dmx_info.devices == NULL) || (dvb_demux == NULL)) {
		MPQ_DVB_ERR_PRINT("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	mpq_demux = dvb_demux->priv;
	if (mpq_demux == NULL) {
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

/**
 * Takes an ION allocated buffer's file descriptor and handles the details of
 * mapping it into kernel memory and obtaining an ION handle for it.
 * Internal helper function.
 *
 * @client: ION client
 * @handle: ION file descriptor to map
 * @priv_handle: returned ION handle. Must be freed when no longer needed
 * @kernel_mem: returned kernel mapped pointer
 *
 * Note: mapping might not be possible in secured heaps/buffers, and so NULL
 * might be returned in kernel_mem
 *
 * Return errors status
 */
static int mpq_map_buffer_to_kernel(
	struct ion_client *client,
	int handle,
	struct ion_handle **priv_handle,
	void **kernel_mem)
{
	struct ion_handle *ion_handle;
	unsigned long ionflag = 0;
	int ret;

	if (client == NULL || priv_handle == NULL || kernel_mem == NULL) {
		MPQ_DVB_ERR_PRINT("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	ion_handle = ion_import_dma_buf(client, handle);
	if (IS_ERR_OR_NULL(ion_handle)) {
		ret = PTR_ERR(ion_handle);
		MPQ_DVB_ERR_PRINT("%s: ion_import_dma_buf failed %d\n",
			__func__, ret);
		if (!ret)
			ret = -ENOMEM;

		goto map_buffer_failed;
	}

	ret = ion_handle_get_flags(client, ion_handle, &ionflag);
	if (ret) {
		MPQ_DVB_ERR_PRINT("%s: ion_handle_get_flags failed %d\n",
			__func__, ret);
		goto map_buffer_failed_free_buff;
	}

	if (ionflag & ION_FLAG_SECURE) {
		MPQ_DVB_DBG_PRINT("%s: secured buffer\n", __func__);
		*kernel_mem = NULL;
	} else {
		unsigned long tmp;
		*kernel_mem = ion_map_kernel(client, ion_handle);
		if (*kernel_mem == NULL) {
			MPQ_DVB_ERR_PRINT("%s: ion_map_kernel failed\n",
				__func__);
			ret = -ENOMEM;
			goto map_buffer_failed_free_buff;
		}
		ion_handle_get_size(client, ion_handle, &tmp);
		MPQ_DVB_DBG_PRINT(
			"%s: mapped to address 0x%p, size=%lu\n",
			__func__, *kernel_mem, tmp);
	}

	*priv_handle = ion_handle;
	return 0;

map_buffer_failed_free_buff:
	ion_free(client, ion_handle);
map_buffer_failed:
	return ret;
}

int mpq_dmx_map_buffer(struct dmx_demux *demux, struct dmx_buffer *dmx_buffer,
		void **priv_handle, void **kernel_mem)
{
	struct dvb_demux *dvb_demux = demux->priv;
	struct mpq_demux *mpq_demux;

	if ((mpq_dmx_info.devices == NULL) || (dvb_demux == NULL) ||
		(priv_handle == NULL) || (kernel_mem == NULL)) {
		MPQ_DVB_ERR_PRINT("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	mpq_demux = dvb_demux->priv;
	if (mpq_demux == NULL) {
		MPQ_DVB_ERR_PRINT("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	return mpq_map_buffer_to_kernel(
		mpq_demux->ion_client,
		dmx_buffer->handle,
		(struct ion_handle **)priv_handle, kernel_mem);
}
EXPORT_SYMBOL(mpq_dmx_map_buffer);

int mpq_dmx_unmap_buffer(struct dmx_demux *demux,
		void *priv_handle)
{
	struct dvb_demux *dvb_demux = demux->priv;
	struct ion_handle *ion_handle = priv_handle;
	struct mpq_demux *mpq_demux;
	unsigned long ionflag = 0;
	int ret;

	if ((mpq_dmx_info.devices == NULL) || (dvb_demux == NULL) ||
		(priv_handle == NULL)) {
		MPQ_DVB_ERR_PRINT("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	mpq_demux = dvb_demux->priv;
	if (mpq_demux == NULL) {
		MPQ_DVB_ERR_PRINT("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	ret = ion_handle_get_flags(mpq_demux->ion_client, ion_handle, &ionflag);
	if (ret) {
		MPQ_DVB_ERR_PRINT("%s: ion_handle_get_flags failed %d\n",
			__func__, ret);
		return -EINVAL;
	}

	if (!(ionflag & ION_FLAG_SECURE))
		ion_unmap_kernel(mpq_demux->ion_client, ion_handle);

	ion_free(mpq_demux->ion_client, ion_handle);

	return 0;
}
EXPORT_SYMBOL(mpq_dmx_unmap_buffer);

int mpq_dmx_reuse_decoder_buffer(struct dvb_demux_feed *feed, int cookie)
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

		mutex_lock(&mpq_demux->mutex);
		mpq_feed = feed->priv;
		feed_data = &mpq_feed->video_info;

		spin_lock(&feed_data->video_buffer_lock);
		stream_buffer = feed_data->video_buffer;
		if (stream_buffer == NULL) {
			MPQ_DVB_ERR_PRINT(
				"%s: invalid feed, feed_data->video_buffer is NULL\n",
				__func__);
			spin_unlock(&feed_data->video_buffer_lock);
			mutex_unlock(&mpq_demux->mutex);
			return -EINVAL;
		}

		ret = mpq_streambuffer_pkt_dispose(stream_buffer, cookie, 1);
		spin_unlock(&feed_data->video_buffer_lock);
		mutex_unlock(&mpq_demux->mutex);

		return ret;
	}

	/* else */
	MPQ_DVB_ERR_PRINT("%s: Invalid feed type %d\n",
			__func__, feed->pes_type);

	return -EINVAL;
}
EXPORT_SYMBOL(mpq_dmx_reuse_decoder_buffer);

/**
 * Handles the details of internal decoder buffer allocation via ION.
 * Internal helper function.
 * @feed_data: decoder feed object
 * @dec_buffs: buffer information
 * @client: ION client
 *
 * Return error status
 */
static int mpq_dmx_init_internal_buffers(
	struct mpq_video_feed_info *feed_data,
	struct dmx_decoder_buffers *dec_buffs,
	struct ion_client *client)
{
	struct ion_handle *temp_handle = NULL;
	void *payload_buffer = NULL;
	int actual_buffer_size = 0;
	int ret = 0;

	MPQ_DVB_DBG_PRINT("%s: Internal decoder buffer allocation\n", __func__);

	actual_buffer_size = dec_buffs->buffers_size;
	actual_buffer_size += (SZ_4K - 1);
	actual_buffer_size &= ~(SZ_4K - 1);

	temp_handle = ion_alloc(client, actual_buffer_size, SZ_4K,
		ION_HEAP(video_secure_ion_heap) |
		ION_HEAP(video_nonsecure_ion_heap),
		ION_FLAG_CACHED);

	if (IS_ERR_OR_NULL(temp_handle)) {
		ret = PTR_ERR(temp_handle);
		MPQ_DVB_ERR_PRINT("%s: FAILED to allocate payload buffer %d\n",
			__func__, ret);
		if (!ret)
			ret = -ENOMEM;
		goto end;
	}

	payload_buffer = ion_map_kernel(client, temp_handle);

	if (IS_ERR_OR_NULL(payload_buffer)) {
		ret = PTR_ERR(payload_buffer);
		MPQ_DVB_ERR_PRINT(
			"%s: FAILED to map payload buffer %d\n",
			__func__, ret);
		if (!ret)
			ret = -ENOMEM;
		goto init_failed_free_payload_buffer;
	}
	feed_data->buffer_desc.decoder_buffers_num = 1;
	feed_data->buffer_desc.ion_handle[0] = temp_handle;
	feed_data->buffer_desc.desc[0].base = payload_buffer;
	feed_data->buffer_desc.desc[0].size = actual_buffer_size;
	feed_data->buffer_desc.desc[0].read_ptr = 0;
	feed_data->buffer_desc.desc[0].write_ptr = 0;
	feed_data->buffer_desc.desc[0].handle =
		ion_share_dma_buf_fd(client, temp_handle);
	if (IS_ERR_VALUE(feed_data->buffer_desc.desc[0].handle)) {
		MPQ_DVB_ERR_PRINT(
			"%s: FAILED to share payload buffer %d\n",
			__func__, ret);
		ret = -ENOMEM;
		goto init_failed_unmap_payload_buffer;
	}

	return 0;

init_failed_unmap_payload_buffer:
	ion_unmap_kernel(client, temp_handle);
	feed_data->buffer_desc.desc[0].base = NULL;
init_failed_free_payload_buffer:
	ion_free(client, temp_handle);
	feed_data->buffer_desc.ion_handle[0] = NULL;
	feed_data->buffer_desc.desc[0].size = 0;
	feed_data->buffer_desc.decoder_buffers_num = 0;
end:
	return ret;
}

/**
 * Handles the details of external decoder buffers allocated by user.
 * Each buffer is mapped into kernel memory and an ION handle is obtained, and
 * decoder feed object is updated with related information.
 * Internal helper function.
 * @feed_data: decoder feed object
 * @dec_buffs: buffer information
 * @client: ION client
 *
 * Return error status
 */
static int mpq_dmx_init_external_buffers(
	struct mpq_video_feed_info *feed_data,
	struct dmx_decoder_buffers *dec_buffs,
	struct ion_client *client)
{
	struct ion_handle *temp_handle = NULL;
	void *payload_buffer = NULL;
	int actual_buffer_size = 0;
	int ret = 0;
	int i;

	/*
	 * Payload buffer was allocated externally (through ION).
	 * Map the ion handles to kernel memory
	 */
	MPQ_DVB_DBG_PRINT("%s: External decoder buffer allocation\n", __func__);

	actual_buffer_size = dec_buffs->buffers_size;
	if (!dec_buffs->is_linear) {
		MPQ_DVB_DBG_PRINT("%s: Ex. Ring-buffer\n", __func__);
		feed_data->buffer_desc.decoder_buffers_num = 1;
	} else {
		MPQ_DVB_DBG_PRINT("%s: Ex. Linear\n", __func__);
		feed_data->buffer_desc.decoder_buffers_num =
			dec_buffs->buffers_num;
	}

	for (i = 0; i < feed_data->buffer_desc.decoder_buffers_num; i++) {
		ret = mpq_map_buffer_to_kernel(
			client,
			dec_buffs->handles[i],
			&temp_handle,
			&payload_buffer);
		if (ret < 0) {
			MPQ_DVB_ERR_PRINT(
				"%s: Failed mapping buffer %d\n",
				__func__, i);
			goto init_failed;
		}
		feed_data->buffer_desc.ion_handle[i] = temp_handle;
		feed_data->buffer_desc.desc[i].base = payload_buffer;
		feed_data->buffer_desc.desc[i].handle =
			dec_buffs->handles[i];
		feed_data->buffer_desc.desc[i].size =
			dec_buffs->buffers_size;
		feed_data->buffer_desc.desc[i].read_ptr = 0;
		feed_data->buffer_desc.desc[i].write_ptr = 0;

		MPQ_DVB_DBG_PRINT(
			"%s: Buffer #%d: base=0x%p, handle=%d, size=%d\n",
			__func__, i ,
			feed_data->buffer_desc.desc[i].base,
			feed_data->buffer_desc.desc[i].handle,
			feed_data->buffer_desc.desc[i].size);
	}

	return 0;

init_failed:
	for (i = 0; i < feed_data->buffer_desc.decoder_buffers_num; i++) {
		if (feed_data->buffer_desc.ion_handle[i]) {
			if (feed_data->buffer_desc.desc[i].base) {
				ion_unmap_kernel(client,
					feed_data->buffer_desc.ion_handle[i]);
				feed_data->buffer_desc.desc[i].base = NULL;
			}
			ion_free(client, feed_data->buffer_desc.ion_handle[i]);
			feed_data->buffer_desc.ion_handle[i] = NULL;
			feed_data->buffer_desc.desc[i].size = 0;
		}
	}
	return ret;
}

/**
 * Handles the details of initializing the mpq_streambuffer object according
 * to the user decoder buffer configuration: External/Internal buffers and
 * ring/linear buffering mode.
 * Internal helper function.
 * @feed:  dvb demux feed object, contains the buffers configuration
 * @feed_data: decoder feed object
 * @stream_buffer: stream buffer object to initialize
 *
 * Return error status
 */
static int mpq_dmx_init_streambuffer(
	struct mpq_feed *feed,
	struct mpq_video_feed_info *feed_data,
	struct mpq_streambuffer *stream_buffer)
{
	int ret;
	void *packet_buffer = NULL;
	struct mpq_demux *mpq_demux = feed->mpq_demux;
	struct ion_client *client = mpq_demux->ion_client;
	struct dmx_decoder_buffers *dec_buffs = NULL;
	enum mpq_streambuffer_mode mode;

	dec_buffs = feed->dvb_demux_feed->feed.ts.decoder_buffers;

	/* Allocate packet buffer holding the meta-data */
	packet_buffer = vmalloc(VIDEO_META_DATA_BUFFER_SIZE);

	if (packet_buffer == NULL) {
		MPQ_DVB_ERR_PRINT(
			"%s: FAILED to allocate packets buffer\n",
			__func__);

		ret = -ENOMEM;
		goto end;
	}

	MPQ_DVB_DBG_PRINT("%s: dec_buffs: num=%d, size=%d, linear=%d\n",
			__func__,
			dec_buffs->buffers_num,
			dec_buffs->buffers_size,
			dec_buffs->is_linear);

	if (0 == dec_buffs->buffers_num)
		ret = mpq_dmx_init_internal_buffers(
			feed_data, dec_buffs, client);
	else
		ret = mpq_dmx_init_external_buffers(
			feed_data, dec_buffs, client);

	if (ret != 0)
		goto init_failed_free_packet_buffer;

	mode = dec_buffs->is_linear ? MPQ_STREAMBUFFER_BUFFER_MODE_LINEAR :
		MPQ_STREAMBUFFER_BUFFER_MODE_RING;
	ret = mpq_streambuffer_init(
			feed_data->video_buffer,
			mode,
			feed_data->buffer_desc.desc,
			feed_data->buffer_desc.decoder_buffers_num,
			packet_buffer,
			VIDEO_META_DATA_BUFFER_SIZE);

	if (ret != 0)
		goto init_failed_free_packet_buffer;

	goto end;


init_failed_free_packet_buffer:
	vfree(packet_buffer);
end:
	return ret;
}

static void mpq_dmx_release_streambuffer(
	struct mpq_feed *feed,
	struct mpq_video_feed_info *feed_data,
	struct mpq_streambuffer *video_buffer,
	struct ion_client *client)
{
	int buf_num = 0;
	int i;
	struct dmx_decoder_buffers *dec_buffs =
		feed->dvb_demux_feed->feed.ts.decoder_buffers;

	mpq_adapter_unregister_stream_if(feed_data->stream_interface);

	mpq_streambuffer_terminate(video_buffer);

	vfree(video_buffer->packet_data.data);

	buf_num = feed_data->buffer_desc.decoder_buffers_num;

	for (i = 0; i < buf_num; i++) {
		if (feed_data->buffer_desc.ion_handle[i]) {
			if (feed_data->buffer_desc.desc[i].base) {
				ion_unmap_kernel(client,
					feed_data->buffer_desc.ion_handle[i]);
				feed_data->buffer_desc.desc[i].base = NULL;
			}

			/*
			 * Un-share the buffer if kernel it the one that
			 * shared it.
			 */
			if (0 == dec_buffs->buffers_num) {
				struct file *shared_file = fget(
					feed_data->buffer_desc.desc[i].handle);

				if (shared_file)
					fput(shared_file);
				else
					MPQ_DVB_ERR_PRINT(
						"%s: failed to get shared-file handle\n",
						__func__);
			}

			ion_free(client, feed_data->buffer_desc.ion_handle[i]);
			feed_data->buffer_desc.ion_handle[i] = NULL;
			feed_data->buffer_desc.desc[i].size = 0;
		}
	}
}

/**
 * mpq_dmx_init_video_feed - Initializes of video feed information
 * used to pass data directly to decoder.
 *
 * @mpq_feed: The mpq feed object
 *
 * Return     error code.
 */
static int mpq_dmx_init_video_feed(struct mpq_feed *mpq_feed)
{
	int ret;
	struct mpq_video_feed_info *feed_data = &mpq_feed->video_info;
	struct mpq_demux *mpq_demux = mpq_feed->mpq_demux;
	struct mpq_streambuffer *stream_buffer;

	/* get and store framing information if required */
	if (video_framing) {
		mpq_dmx_get_pattern_params(
			mpq_feed->dvb_demux_feed->video_codec,
			feed_data->patterns, &feed_data->patterns_num);
		if (!feed_data->patterns_num) {
			MPQ_DVB_ERR_PRINT(
				"%s: FAILED to get framing pattern parameters\n",
				__func__);

			ret = -EINVAL;
			goto init_failed_free_priv_data;
		}
	}

	/* Register the new stream-buffer interface to MPQ adapter */
	switch (mpq_feed->dvb_demux_feed->pes_type) {
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
			mpq_feed->dvb_demux_feed->pes_type);
		ret = -EINVAL;
		goto init_failed_free_priv_data;
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
		goto init_failed_free_priv_data;
	}

	feed_data->video_buffer =
		&mpq_dmx_info.decoder_buffers[feed_data->stream_interface];

	ret = mpq_dmx_init_streambuffer(
		mpq_feed, feed_data, feed_data->video_buffer);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_dmx_init_streambuffer failed, err = %d\n",
			__func__, ret);
		goto init_failed_free_priv_data;
	}

	ret = mpq_adapter_register_stream_if(
			feed_data->stream_interface,
			feed_data->video_buffer);

	if (ret < 0) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_adapter_register_stream_if failed, "
			"err = %d\n",
			__func__, ret);
		goto init_failed_free_stream_buffer;
	}

	spin_lock_init(&feed_data->video_buffer_lock);

	feed_data->pes_header_left_bytes = PES_MANDATORY_FIELDS_LEN;
	feed_data->pes_header_offset = 0;
	mpq_feed->dvb_demux_feed->pusi_seen = 0;
	mpq_feed->dvb_demux_feed->peslen = 0;
	feed_data->fullness_wait_cancel = 0;
	mpq_streambuffer_get_data_rw_offset(feed_data->video_buffer, NULL,
		&feed_data->frame_offset);
	feed_data->last_pattern_offset = 0;
	feed_data->pending_pattern_len = 0;
	feed_data->last_framing_match_type = 0;
	feed_data->found_sequence_header_pattern = 0;
	memset(&feed_data->prefix_size, 0,
			sizeof(struct dvb_dmx_video_prefix_size_masks));
	feed_data->first_prefix_size = 0;
	feed_data->saved_pts_dts_info.pts_exist = 0;
	feed_data->saved_pts_dts_info.dts_exist = 0;
	feed_data->new_pts_dts_info.pts_exist = 0;
	feed_data->new_pts_dts_info.dts_exist = 0;
	feed_data->saved_info_used = 1;
	feed_data->new_info_exists = 0;
	feed_data->first_pts_dts_copy = 1;
	feed_data->tei_errs = 0;
	feed_data->last_continuity = -1;
	feed_data->continuity_errs = 0;
	feed_data->ts_packets_num = 0;
	feed_data->ts_dropped_bytes = 0;
	feed_data->last_pkt_index = -1;

	mpq_demux->decoder_drop_count = 0;
	mpq_demux->decoder_out_count = 0;
	mpq_demux->decoder_out_interval_sum = 0;
	mpq_demux->decoder_out_interval_max = 0;
	mpq_demux->decoder_ts_errors = 0;
	mpq_demux->decoder_cc_errors = 0;

	return 0;

init_failed_free_stream_buffer:
	mpq_dmx_release_streambuffer(mpq_feed, feed_data,
		feed_data->video_buffer, mpq_demux->ion_client);
	mpq_adapter_unregister_stream_if(feed_data->stream_interface);
init_failed_free_priv_data:
	feed_data->video_buffer = NULL;
	return ret;
}

/**
 * mpq_dmx_terminate_video_feed - terminate video feed information
 * that was previously initialized in mpq_dmx_init_video_feed
 *
 * @mpq_feed: The mpq feed used for the video TS packets
 *
 * Return     error code.
 */
static int mpq_dmx_terminate_video_feed(struct mpq_feed *mpq_feed)
{
	struct mpq_streambuffer *video_buffer;
	struct mpq_video_feed_info *feed_data;
	struct mpq_demux *mpq_demux = mpq_feed->mpq_demux;

	if (mpq_feed == NULL)
		return -EINVAL;

	feed_data = &mpq_feed->video_info;

	spin_lock(&feed_data->video_buffer_lock);
	video_buffer = feed_data->video_buffer;
	feed_data->video_buffer = NULL;
	wake_up_all(&video_buffer->raw_data.queue);
	spin_unlock(&feed_data->video_buffer_lock);

	mpq_dmx_release_streambuffer(mpq_feed, feed_data,
		video_buffer, mpq_demux->ion_client);

	return 0;
}

/**
 * mpq_sdmx_lookup_feed() - Search for a feed object that shares the same
 * filter of the specified feed object, and return it
 *
 * @feed: dvb demux feed object
 *
 * Return the mpq_feed sharing the same filter's buffer or NULL if no
 * such is found.
 */
static struct mpq_feed *mpq_sdmx_lookup_feed(struct dvb_demux_feed *feed)
{
	int i;
	struct dvb_demux_feed *tmp;
	struct mpq_demux *mpq_demux = feed->demux->priv;

	for (i = 0; i < MPQ_MAX_DMX_FILES; i++) {
		tmp = mpq_demux->feeds[i].dvb_demux_feed;
		if ((tmp->state == DMX_STATE_GO) &&
			(tmp != feed) &&
			(tmp->feed.ts.buffer.ringbuff ==
			feed->feed.ts.buffer.ringbuff)) {
			MPQ_DVB_DBG_PRINT(
				"%s: main feed pid=%d, secondary feed pid=%d\n",
				__func__, tmp->pid, feed->pid);
			return &mpq_demux->feeds[i];
		}
	}

	return NULL;
}

static int mpq_sdmx_alloc_data_buf(struct mpq_feed *mpq_feed, size_t size)
{
	struct mpq_demux *mpq_demux = mpq_feed->mpq_demux;
	void *buf_base;
	int ret;

	mpq_feed->sdmx_buf_handle = ion_alloc(mpq_demux->ion_client,
		size,
		SZ_4K,
		ION_HEAP(ION_QSECOM_HEAP_ID),
		0);
	if (IS_ERR_OR_NULL(mpq_feed->sdmx_buf_handle)) {
		ret = PTR_ERR(mpq_feed->sdmx_buf_handle);
		MPQ_DVB_ERR_PRINT(
			"%s: FAILED to allocate sdmx buffer %d\n",
			__func__, ret);
		if (!ret)
			ret = -ENOMEM;
		goto end;
	}

	buf_base = ion_map_kernel(mpq_demux->ion_client,
		mpq_feed->sdmx_buf_handle);
	if (IS_ERR_OR_NULL(buf_base)) {
		ret = PTR_ERR(buf_base);
		MPQ_DVB_ERR_PRINT(
			"%s: FAILED to map sdmx buffer %d\n",
			__func__, ret);
		if (!ret)
			ret = -ENOMEM;
		goto failed_free_buf;
	}

	dvb_ringbuffer_init(&mpq_feed->sdmx_buf, buf_base, size);

	return 0;

failed_free_buf:
	ion_free(mpq_demux->ion_client, mpq_feed->sdmx_buf_handle);
	mpq_feed->sdmx_buf_handle = NULL;
end:
	return ret;
}

static int mpq_sdmx_free_data_buf(struct mpq_feed *mpq_feed)
{
	struct mpq_demux *mpq_demux = mpq_feed->mpq_demux;

	if (mpq_feed->sdmx_buf_handle) {
		ion_unmap_kernel(mpq_demux->ion_client,
			mpq_feed->sdmx_buf_handle);
		mpq_feed->sdmx_buf.data = NULL;
		ion_free(mpq_demux->ion_client,
			mpq_feed->sdmx_buf_handle);
		mpq_feed->sdmx_buf_handle = NULL;
	}

	return 0;
}

static int mpq_sdmx_init_metadata_buffer(struct mpq_demux *mpq_demux,
	struct mpq_feed *feed, struct sdmx_buff_descr *metadata_buff_desc)
{
	void *metadata_buff_base;
	ion_phys_addr_t temp;
	int ret;

	feed->metadata_buf_handle = ion_alloc(mpq_demux->ion_client,
		SDMX_METADATA_BUFFER_SIZE,
		SZ_4K,
		ION_HEAP(ION_QSECOM_HEAP_ID),
		0);
	if (IS_ERR_OR_NULL(feed->metadata_buf_handle)) {
		ret = PTR_ERR(feed->metadata_buf_handle);
		MPQ_DVB_ERR_PRINT(
			"%s: FAILED to allocate metadata buffer %d\n",
			__func__, ret);
		if (!ret)
			ret = -ENOMEM;
		goto end;
	}

	metadata_buff_base = ion_map_kernel(mpq_demux->ion_client,
		feed->metadata_buf_handle);
	if (IS_ERR_OR_NULL(metadata_buff_base)) {
		ret = PTR_ERR(metadata_buff_base);
		MPQ_DVB_ERR_PRINT(
			"%s: FAILED to map metadata buffer %d\n",
			__func__, ret);
		if (!ret)
			ret = -ENOMEM;
		goto failed_free_metadata_buf;
	}

	ret = ion_phys(mpq_demux->ion_client,
		feed->metadata_buf_handle,
		&temp,
		&metadata_buff_desc->size);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: FAILED to get physical address %d\n",
			__func__, ret);
		goto failed_unmap_metadata_buf;
	}

	/*
	 * NOTE: the following casting to u32 must be done
	 * as long as TZ does not support LPAE. Once TZ supports
	 * LPAE SDMX interface needs to be updated accordingly.
	 */
	if (temp > 0xFFFFFFFF)
		MPQ_DVB_ERR_PRINT(
			"%s: WARNNING - physical address %pa is larger than 32bits!\n",
			__func__, &temp);
	metadata_buff_desc->base_addr = (void *)(u32)temp;

	dvb_ringbuffer_init(&feed->metadata_buf, metadata_buff_base,
		SDMX_METADATA_BUFFER_SIZE);

	return 0;

failed_unmap_metadata_buf:
	ion_unmap_kernel(mpq_demux->ion_client, feed->metadata_buf_handle);
failed_free_metadata_buf:
	ion_free(mpq_demux->ion_client, feed->metadata_buf_handle);
	feed->metadata_buf_handle = NULL;
end:
	return ret;
}

static int mpq_sdmx_terminate_metadata_buffer(struct mpq_feed *mpq_feed)
{
	struct mpq_demux *mpq_demux = mpq_feed->mpq_demux;

	if (mpq_feed->metadata_buf_handle) {
		ion_unmap_kernel(mpq_demux->ion_client,
			mpq_feed->metadata_buf_handle);
		mpq_feed->metadata_buf.data = NULL;
		ion_free(mpq_demux->ion_client,
			mpq_feed->metadata_buf_handle);
		mpq_feed->metadata_buf_handle = NULL;
	}

	return 0;
}

int mpq_dmx_terminate_feed(struct dvb_demux_feed *feed)
{
	int ret = 0;
	struct mpq_demux *mpq_demux;
	struct mpq_feed *mpq_feed;
	struct mpq_feed *main_rec_feed;

	if (feed == NULL)
		return -EINVAL;

	mpq_demux = feed->demux->priv;

	mutex_lock(&mpq_demux->mutex);
	mpq_feed = feed->priv;

	if (mpq_feed->sdmx_filter_handle != SDMX_INVALID_FILTER_HANDLE) {
		if (mpq_feed->filter_type == SDMX_RAW_FILTER)
			main_rec_feed = mpq_sdmx_lookup_feed(feed);
		else
			main_rec_feed = NULL;

		if (main_rec_feed) {
			/* This feed is part of a recording filter */
			MPQ_DVB_DBG_PRINT(
				"%s: Removing raw pid %d from filter %d\n",
				__func__, feed->pid,
				mpq_feed->sdmx_filter_handle);
			ret = sdmx_remove_raw_pid(
				mpq_demux->sdmx_session_handle,
				mpq_feed->sdmx_filter_handle, feed->pid);
			if (ret)
				MPQ_DVB_ERR_PRINT(
					"%s: SDMX_remove_raw_pid failed. ret = %d\n",
					__func__, ret);

			/* If this feed that we are removing was set as primary,
			 * now other feeds should be set as primary
			 */
			if (!mpq_feed->secondary_feed)
				main_rec_feed->secondary_feed = 0;
		} else {
			MPQ_DVB_DBG_PRINT("%s: Removing filter %d, pid %d\n",
				__func__, mpq_feed->sdmx_filter_handle,
				feed->pid);
			ret = sdmx_remove_filter(mpq_demux->sdmx_session_handle,
				mpq_feed->sdmx_filter_handle);
			if (ret) {
				MPQ_DVB_ERR_PRINT(
					"%s: SDMX_remove_filter failed. ret = %d\n",
					__func__, ret);
			}

			mpq_demux->sdmx_filter_count--;
			mpq_feed->sdmx_filter_handle =
				SDMX_INVALID_FILTER_HANDLE;
		}

		mpq_sdmx_close_session(mpq_demux);
		mpq_demux->num_secure_feeds--;
	}

	if (dvb_dmx_is_video_feed(feed)) {
		ret = mpq_dmx_terminate_video_feed(mpq_feed);
		if (ret)
			MPQ_DVB_ERR_PRINT(
				"%s: mpq_dmx_terminate_video_feed failed. ret = %d\n",
				__func__, ret);
	}

	if (mpq_feed->sdmx_buf_handle) {
		wake_up_all(&mpq_feed->sdmx_buf.queue);
		mpq_sdmx_free_data_buf(mpq_feed);
	}

	mpq_sdmx_terminate_metadata_buffer(mpq_feed);
	mpq_demux->num_active_feeds--;

	mutex_unlock(&mpq_demux->mutex);

	return ret;
}
EXPORT_SYMBOL(mpq_dmx_terminate_feed);

int mpq_dmx_decoder_fullness_init(struct dvb_demux_feed *feed)
{
	if (dvb_dmx_is_video_feed(feed)) {
		struct mpq_feed *mpq_feed;
		struct mpq_video_feed_info *feed_data;

		mpq_feed = feed->priv;
		feed_data = &mpq_feed->video_info;
		feed_data->fullness_wait_cancel = 0;

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

/**
 * Returns whether the free space of decoder's output
 * buffer is larger than specific number of bytes.
 *
 * @sbuff: MPQ stream buffer used for decoder data.
 * @required_space: number of required free bytes in the buffer
 *
 * Return 1 if required free bytes are available, 0 otherwise.
 */
static inline int mpq_dmx_check_decoder_fullness(
	struct mpq_streambuffer *sbuff,
	size_t required_space)
{
	u32 free = mpq_streambuffer_data_free(sbuff);

	/*
	 * For linear buffers, verify there's enough space for this TSP
	 * and an additional buffer is free, as framing might required one
	 * more buffer to be available.
	 */
	if (MPQ_STREAMBUFFER_BUFFER_MODE_LINEAR == sbuff->mode)
		return (free >= required_space &&
			sbuff->pending_buffers_count < sbuff->buffers_num-1);
	else
		/* Ring buffer mode */
		return (free >= required_space);
}

/**
 * Checks whether decoder's output buffer has free space
 * for specific number of bytes, if not, the function waits
 * until the amount of free-space is available.
 *
 * @feed: decoder's feed object
 * @required_space: number of required free bytes in the buffer
 * @lock_feed: indicates whether mutex should be held before
 * accessing the feed information. If the caller of this function
 * already holds a mutex then this should be set to 0 and 1 otherwise.
 *
 * Return 0 if required space is available and error code
 * in case waiting on buffer fullness was aborted.
 */
static int mpq_dmx_decoder_fullness_check(
		struct dvb_demux_feed *feed,
		size_t required_space,
		int lock_feed)
{
	struct mpq_demux *mpq_demux = feed->demux->priv;
	struct mpq_streambuffer *sbuff = NULL;
	struct mpq_video_feed_info *feed_data;
	struct mpq_feed *mpq_feed;
	int ret = 0;

	if (!dvb_dmx_is_video_feed(feed)) {
		MPQ_DVB_DBG_PRINT("%s: Invalid feed type %d\n",
			__func__,
			feed->pes_type);
		return -EINVAL;
	}

	if (lock_feed) {
		mutex_lock(&mpq_demux->mutex);
	} else if (!mutex_is_locked(&mpq_demux->mutex)) {
		MPQ_DVB_ERR_PRINT(
				"%s: Mutex should have been locked\n",
				__func__);
		return -EINVAL;
	}

	mpq_feed = feed->priv;
	feed_data = &mpq_feed->video_info;

	sbuff = feed_data->video_buffer;
	if (sbuff == NULL) {
		if (lock_feed)
			mutex_unlock(&mpq_demux->mutex);
		MPQ_DVB_ERR_PRINT("%s: mpq_streambuffer object is NULL\n",
			__func__);
		return -EINVAL;
	}

	if ((feed_data->video_buffer != NULL) &&
		(!feed_data->fullness_wait_cancel) &&
		(!mpq_dmx_check_decoder_fullness(sbuff, required_space))) {
		DEFINE_WAIT(__wait);
		for (;;) {
			prepare_to_wait(&sbuff->raw_data.queue,
				&__wait,
				TASK_INTERRUPTIBLE);
			if (!feed_data->video_buffer ||
				feed_data->fullness_wait_cancel ||
				mpq_dmx_check_decoder_fullness(sbuff,
					required_space))
				break;

			if (!signal_pending(current)) {
				mutex_unlock(&mpq_demux->mutex);
				schedule();
				mutex_lock(&mpq_demux->mutex);
				continue;
			}

			ret = -ERESTARTSYS;
			break;
		}
		finish_wait(&sbuff->raw_data.queue, &__wait);
	}

	if (ret < 0) {
		if (lock_feed)
			mutex_unlock(&mpq_demux->mutex);
		return ret;
	}

	if ((feed_data->fullness_wait_cancel) ||
		(feed_data->video_buffer == NULL)) {
		if (lock_feed)
			mutex_unlock(&mpq_demux->mutex);
		return -EINVAL;
	}

	if (lock_feed)
		mutex_unlock(&mpq_demux->mutex);
	return 0;
}

int mpq_dmx_decoder_fullness_wait(
		struct dvb_demux_feed *feed,
		size_t required_space)
{
	return mpq_dmx_decoder_fullness_check(feed, required_space, 1);
}
EXPORT_SYMBOL(mpq_dmx_decoder_fullness_wait);

int mpq_dmx_decoder_fullness_abort(struct dvb_demux_feed *feed)
{
	if (dvb_dmx_is_video_feed(feed)) {
		struct mpq_feed *mpq_feed;
		struct mpq_video_feed_info *feed_data;
		struct dvb_ringbuffer *video_buff;

		mpq_feed = feed->priv;
		feed_data = &mpq_feed->video_info;

		feed_data->fullness_wait_cancel = 1;

		spin_lock(&feed_data->video_buffer_lock);
		if (feed_data->video_buffer == NULL) {
			MPQ_DVB_DBG_PRINT(
				"%s: video_buffer released\n",
				__func__);
			spin_unlock(&feed_data->video_buffer_lock);
			return 0;
		}

		video_buff = &feed_data->video_buffer->raw_data;
		wake_up_all(&video_buff->queue);
		spin_unlock(&feed_data->video_buffer_lock);

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


static inline int mpq_dmx_parse_mandatory_pes_header(
				struct dvb_demux_feed *feed,
				struct mpq_video_feed_info *feed_data,
				struct pes_packet_header *pes_header,
				const u8 *buf,
				u32 *ts_payload_offset,
				int *bytes_avail)
{
	int left_size, copy_len;

	if (feed_data->pes_header_offset < PES_MANDATORY_FIELDS_LEN) {
		left_size =
			PES_MANDATORY_FIELDS_LEN -
			feed_data->pes_header_offset;

		copy_len = (left_size > *bytes_avail) ?
					*bytes_avail :
					left_size;

		memcpy((u8 *)((u8 *)pes_header + feed_data->pes_header_offset),
				(buf + *ts_payload_offset),
				copy_len);

		feed_data->pes_header_offset += copy_len;

		if (left_size > *bytes_avail)
			return -EINVAL;

		/* else - we have beginning of PES header */
		*bytes_avail -= left_size;
		*ts_payload_offset += left_size;

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

			return -EINVAL;
		}

		feed_data->pes_header_left_bytes =
			pes_header->pes_header_data_length;
	}

	return 0;
}

static inline void mpq_dmx_save_pts_dts(struct mpq_video_feed_info *feed_data)
{
	if (feed_data->new_info_exists) {
		feed_data->saved_pts_dts_info.pts_exist =
			feed_data->new_pts_dts_info.pts_exist;
		feed_data->saved_pts_dts_info.pts =
			feed_data->new_pts_dts_info.pts;
		feed_data->saved_pts_dts_info.dts_exist =
			feed_data->new_pts_dts_info.dts_exist;
		feed_data->saved_pts_dts_info.dts =
			feed_data->new_pts_dts_info.dts;

		feed_data->new_info_exists = 0;
		feed_data->saved_info_used = 0;
	}
}

static inline void mpq_dmx_write_pts_dts(struct mpq_video_feed_info *feed_data,
					struct dmx_pts_dts_info *info)
{
	if (!feed_data->saved_info_used) {
		info->pts_exist = feed_data->saved_pts_dts_info.pts_exist;
		info->pts = feed_data->saved_pts_dts_info.pts;
		info->dts_exist = feed_data->saved_pts_dts_info.dts_exist;
		info->dts = feed_data->saved_pts_dts_info.dts;

		feed_data->saved_info_used = 1;
	} else {
		info->pts_exist = 0;
		info->dts_exist = 0;
	}
}

static inline void mpq_dmx_get_pts_dts(struct mpq_video_feed_info *feed_data,
				struct pes_packet_header *pes_header)
{
	struct dmx_pts_dts_info *info = &(feed_data->new_pts_dts_info);

	/* Get PTS/DTS information from PES header */

	if ((pes_header->pts_dts_flag == 2) ||
		(pes_header->pts_dts_flag == 3)) {
		info->pts_exist = 1;

		info->pts =
			((u64)pes_header->pts_1 << 30) |
			((u64)pes_header->pts_2 << 22) |
			((u64)pes_header->pts_3 << 15) |
			((u64)pes_header->pts_4 << 7) |
			(u64)pes_header->pts_5;
	} else {
		info->pts_exist = 0;
		info->pts = 0;
	}

	if (pes_header->pts_dts_flag == 3) {
		info->dts_exist = 1;

		info->dts =
			((u64)pes_header->dts_1 << 30) |
			((u64)pes_header->dts_2 << 22) |
			((u64)pes_header->dts_3 << 15) |
			((u64)pes_header->dts_4 << 7) |
			(u64)pes_header->dts_5;
	} else {
		info->dts_exist = 0;
		info->dts = 0;
	}

	feed_data->new_info_exists = 1;
}

static inline int mpq_dmx_parse_remaining_pes_header(
				struct dvb_demux_feed *feed,
				struct mpq_video_feed_info *feed_data,
				struct pes_packet_header *pes_header,
				const u8 *buf,
				u32 *ts_payload_offset,
				int *bytes_avail)
{
	int left_size, copy_len;

	/* Remaining header bytes that need to be processed? */
	if (!feed_data->pes_header_left_bytes)
		return 0;

	/* Did we capture the PTS value (if exists)? */
	if ((*bytes_avail != 0) &&
		(feed_data->pes_header_offset <
		 (PES_MANDATORY_FIELDS_LEN+5)) &&
		((pes_header->pts_dts_flag == 2) ||
		 (pes_header->pts_dts_flag == 3))) {

		/* 5 more bytes should be there */
		left_size =
			PES_MANDATORY_FIELDS_LEN + 5 -
			feed_data->pes_header_offset;

		copy_len = (left_size > *bytes_avail) ?
					*bytes_avail :
					left_size;

		memcpy((u8 *)((u8 *)pes_header + feed_data->pes_header_offset),
			(buf + *ts_payload_offset),
			copy_len);

		feed_data->pes_header_offset += copy_len;
		feed_data->pes_header_left_bytes -= copy_len;

		if (left_size > *bytes_avail)
			return -EINVAL;

		/* else - we have the PTS */
		*bytes_avail -= copy_len;
		*ts_payload_offset += copy_len;
	}

	/* Did we capture the DTS value (if exist)? */
	if ((*bytes_avail != 0) &&
		(feed_data->pes_header_offset <
		 (PES_MANDATORY_FIELDS_LEN+10)) &&
		(pes_header->pts_dts_flag == 3)) {

		/* 5 more bytes should be there */
		left_size =
			PES_MANDATORY_FIELDS_LEN + 10 -
			feed_data->pes_header_offset;

		copy_len = (left_size > *bytes_avail) ?
					*bytes_avail :
					left_size;

		memcpy((u8 *)((u8 *)pes_header + feed_data->pes_header_offset),
			(buf + *ts_payload_offset),
			copy_len);

		feed_data->pes_header_offset += copy_len;
		feed_data->pes_header_left_bytes -= copy_len;

		if (left_size > *bytes_avail)
			return -EINVAL;

		/* else - we have the DTS */
		*bytes_avail -= copy_len;
		*ts_payload_offset += copy_len;
	}

	/* Any more header bytes?! */
	if (feed_data->pes_header_left_bytes >= *bytes_avail) {
		feed_data->pes_header_left_bytes -= *bytes_avail;
		return -EINVAL;
	}

	/* get PTS/DTS information from PES header to be written later */
	mpq_dmx_get_pts_dts(feed_data, pes_header);

	/* Got PES header, process payload */
	*bytes_avail -= feed_data->pes_header_left_bytes;
	*ts_payload_offset += feed_data->pes_header_left_bytes;
	feed_data->pes_header_left_bytes = 0;

	return 0;
}

static void mpq_dmx_check_continuity(struct mpq_video_feed_info *feed_data,
					int current_continuity,
					int discontinuity_indicator)
{
	const int max_continuity = 0x0F; /* 4 bits in the TS packet header */

	/* sanity check */
	if (unlikely((current_continuity < 0) ||
			(current_continuity > max_continuity))) {
		MPQ_DVB_DBG_PRINT(
			"%s: received invalid continuity counter value %d\n",
					__func__, current_continuity);
		return;
	}

	/* reset last continuity */
	if ((feed_data->last_continuity == -1) ||
		(discontinuity_indicator)) {
		feed_data->last_continuity = current_continuity;
		return;
	}

	/* check for continuity errors */
	if (current_continuity !=
			((feed_data->last_continuity + 1) & max_continuity))
		feed_data->continuity_errs++;

	/* save for next time */
	feed_data->last_continuity = current_continuity;
}

static inline void mpq_dmx_prepare_es_event_data(
			struct mpq_streambuffer_packet_header *packet,
			struct mpq_adapter_video_meta_data *meta_data,
			struct mpq_video_feed_info *feed_data,
			struct mpq_streambuffer *stream_buffer,
			struct dmx_data_ready *data)
{
	size_t len = 0;
	struct dmx_pts_dts_info *pts_dts;

	if (meta_data->packet_type == DMX_PES_PACKET) {
		pts_dts = &meta_data->info.pes.pts_dts_info;
		data->buf.stc = meta_data->info.pes.stc;
	} else {
		pts_dts = &meta_data->info.framing.pts_dts_info;
		data->buf.stc = meta_data->info.framing.stc;
	}

	pts_dts = meta_data->packet_type == DMX_PES_PACKET ?
		&meta_data->info.pes.pts_dts_info :
		&meta_data->info.framing.pts_dts_info;

	data->data_length = 0;
	data->buf.handle = packet->raw_data_handle;

	/* this has to succeed when called here, after packet was written */
	data->buf.cookie = mpq_streambuffer_pkt_next(stream_buffer,
				feed_data->last_pkt_index, &len);
	if (data->buf.cookie < 0)
		MPQ_DVB_DBG_PRINT(
			"%s: received invalid packet index %d\n",
			__func__, data->buf.cookie);

	data->buf.offset = packet->raw_data_offset;
	data->buf.len = packet->raw_data_len;
	data->buf.pts_exists = pts_dts->pts_exist;
	data->buf.pts = pts_dts->pts;
	data->buf.dts_exists = pts_dts->dts_exist;
	data->buf.dts = pts_dts->dts;
	data->buf.tei_counter = feed_data->tei_errs;
	data->buf.cont_err_counter = feed_data->continuity_errs;
	data->buf.ts_packets_num = feed_data->ts_packets_num;
	data->buf.ts_dropped_bytes = feed_data->ts_dropped_bytes;
	data->status = DMX_OK_DECODER_BUF;

	/* save for next time: */
	feed_data->last_pkt_index = data->buf.cookie;

	MPQ_DVB_DBG_PRINT("%s: cookie=%d\n", __func__, data->buf.cookie);

	/* reset counters */
	feed_data->ts_packets_num = 0;
	feed_data->ts_dropped_bytes = 0;
	feed_data->tei_errs = 0;
	feed_data->continuity_errs = 0;
}

static int mpq_sdmx_dvr_buffer_desc(struct mpq_demux *mpq_demux,
	struct sdmx_buff_descr *buf_desc)
{
	struct dvb_ringbuffer *rbuf = (struct dvb_ringbuffer *)
				mpq_demux->demux.dmx.dvr_input.ringbuff;
	struct ion_handle *ion_handle =
		mpq_demux->demux.dmx.dvr_input.priv_handle;
	ion_phys_addr_t phys_addr;
	size_t len;
	int ret;

	ret = ion_phys(mpq_demux->ion_client, ion_handle, &phys_addr, &len);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: Failed to obtain physical address of input buffer. ret = %d\n",
			__func__, ret);
		return ret;
	}

	/*
	 * NOTE: the following casting to u32 must be done
	 * as long as TZ does not support LPAE. Once TZ supports
	 * LPAE SDMX interface needs to be updated accordingly.
	 */
	if (phys_addr > 0xFFFFFFFF)
		MPQ_DVB_ERR_PRINT(
			"%s: WARNNING - physical address %pa is larger than 32bits!\n",
			__func__, &phys_addr);
	buf_desc->base_addr = (void *)(u32)phys_addr;
	buf_desc->size = rbuf->size;

	return 0;
}

/**
 * mpq_dmx_decoder_frame_closure - Helper function to handle closing current
 * pending frame upon reaching EOS.
 *
 * @mpq_demux - mpq demux instance
 * @mpq_feed - mpq feed object
 */
static void mpq_dmx_decoder_frame_closure(struct mpq_demux *mpq_demux,
		struct mpq_feed *mpq_feed)
{
	struct mpq_streambuffer_packet_header packet;
	struct mpq_streambuffer *stream_buffer;
	struct mpq_adapter_video_meta_data meta_data;
	struct mpq_video_feed_info *feed_data;
	struct dvb_demux_feed *feed = mpq_feed->dvb_demux_feed;
	struct dmx_data_ready data;

	feed_data = &mpq_feed->video_info;

	/*
	 * spin-lock is taken to protect against manipulation of video
	 * output buffer by the API (terminate video feed, re-use of video
	 * buffers).
	 */
	spin_lock(&feed_data->video_buffer_lock);
	stream_buffer = feed_data->video_buffer;

	if (stream_buffer == NULL) {
		MPQ_DVB_DBG_PRINT("%s: video_buffer released\n", __func__);
		spin_unlock(&feed_data->video_buffer_lock);
		return;
	}

	/* Report last pattern found */
	if ((feed_data->pending_pattern_len) &&
		mpq_dmx_is_video_frame(feed->video_codec,
			feed_data->last_framing_match_type)) {
		meta_data.packet_type = DMX_FRAMING_INFO_PACKET;
		mpq_dmx_write_pts_dts(feed_data,
			&(meta_data.info.framing.pts_dts_info));
		mpq_dmx_save_pts_dts(feed_data);
		packet.user_data_len =
			sizeof(struct mpq_adapter_video_meta_data);
		packet.raw_data_len = feed_data->pending_pattern_len;
		packet.raw_data_offset = feed_data->frame_offset;
		meta_data.info.framing.pattern_type =
			feed_data->last_framing_match_type;
		meta_data.info.framing.stc = feed_data->last_framing_match_stc;
		meta_data.info.framing.continuity_error_counter =
			feed_data->continuity_errs;
		meta_data.info.framing.transport_error_indicator_counter =
			feed_data->tei_errs;
		meta_data.info.framing.ts_dropped_bytes =
			feed_data->ts_dropped_bytes;
		meta_data.info.framing.ts_packets_num =
			feed_data->ts_packets_num;

		mpq_streambuffer_get_buffer_handle(stream_buffer,
			0, /* current write buffer handle */
			&packet.raw_data_handle);

		mpq_dmx_update_decoder_stat(mpq_demux);

		/* Writing meta-data that includes the framing information */
		if (mpq_streambuffer_pkt_write(stream_buffer, &packet,
			(u8 *)&meta_data) < 0)
			MPQ_DVB_ERR_PRINT("%s: Couldn't write packet\n",
				__func__);

		mpq_dmx_prepare_es_event_data(&packet, &meta_data, feed_data,
			stream_buffer, &data);
		feed->data_ready_cb.ts(&feed->feed.ts, &data);
	}

	spin_unlock(&feed_data->video_buffer_lock);
}

/**
 * mpq_dmx_decoder_pes_closure - Helper function to handle closing current PES
 * upon reaching EOS.
 *
 * @mpq_demux - mpq demux instance
 * @mpq_feed - mpq feed object
 */
static void mpq_dmx_decoder_pes_closure(struct mpq_demux *mpq_demux,
	struct mpq_feed *mpq_feed)
{
	struct mpq_streambuffer_packet_header packet;
	struct mpq_streambuffer *stream_buffer;
	struct mpq_adapter_video_meta_data meta_data;
	struct mpq_video_feed_info *feed_data;
	struct dvb_demux_feed *feed = mpq_feed->dvb_demux_feed;
	struct dmx_data_ready data;

	feed_data = &mpq_feed->video_info;

	/*
	 * spin-lock is taken to protect against manipulation of video
	 * output buffer by the API (terminate video feed, re-use of video
	 * buffers).
	 */
	spin_lock(&feed_data->video_buffer_lock);
	stream_buffer = feed_data->video_buffer;

	if (stream_buffer == NULL) {
		MPQ_DVB_DBG_PRINT("%s: video_buffer released\n", __func__);
		spin_unlock(&feed_data->video_buffer_lock);
		return;
	}

	/*
	 * Close previous PES.
	 * Push new packet to the meta-data buffer.
	 */
	if ((feed->pusi_seen) && (0 == feed_data->pes_header_left_bytes)) {
		packet.raw_data_len = feed->peslen;
		mpq_streambuffer_get_buffer_handle(stream_buffer,
			0, /* current write buffer handle */
			&packet.raw_data_handle);
		packet.raw_data_offset = feed_data->frame_offset;
		packet.user_data_len =
			sizeof(struct mpq_adapter_video_meta_data);

		mpq_dmx_write_pts_dts(feed_data,
			&(meta_data.info.pes.pts_dts_info));

		meta_data.packet_type = DMX_PES_PACKET;
		meta_data.info.pes.stc = feed_data->prev_stc;

		mpq_dmx_update_decoder_stat(mpq_demux);

		if (mpq_streambuffer_pkt_write(stream_buffer, &packet,
			(u8 *)&meta_data) < 0)
			MPQ_DVB_ERR_PRINT("%s: Couldn't write packet\n",
				__func__);

		/* Save write offset where new PES will begin */
		mpq_streambuffer_get_data_rw_offset(stream_buffer, NULL,
			&feed_data->frame_offset);

		mpq_dmx_prepare_es_event_data(&packet, &meta_data, feed_data,
			stream_buffer, &data);
		feed->data_ready_cb.ts(&feed->feed.ts, &data);
	}
	/* Reset PES info */
	feed->peslen = 0;
	feed_data->pes_header_offset = 0;
	feed_data->pes_header_left_bytes = PES_MANDATORY_FIELDS_LEN;

	spin_unlock(&feed_data->video_buffer_lock);
}

static int mpq_dmx_process_video_packet_framing(
			struct dvb_demux_feed *feed,
			const u8 *buf,
			u64 curr_stc)
{
	int bytes_avail;
	u32 ts_payload_offset;
	struct mpq_video_feed_info *feed_data;
	const struct ts_packet_header *ts_header;
	struct mpq_streambuffer *stream_buffer;
	struct pes_packet_header *pes_header;
	struct mpq_demux *mpq_demux;
	struct mpq_feed *mpq_feed;

	struct dvb_dmx_video_patterns_results framing_res;
	struct mpq_streambuffer_packet_header packet;
	struct mpq_adapter_video_meta_data meta_data;
	int bytes_written = 0;
	int bytes_to_write = 0;
	int found_patterns = 0;
	int first_pattern = 0;
	int i;
	int is_video_frame = 0;
	int pending_data_len = 0;
	int ret = 0;
	int discontinuity_indicator = 0;
	struct dmx_data_ready data;

	mpq_demux = feed->demux->priv;

	mpq_feed = feed->priv;
	feed_data = &mpq_feed->video_info;

	/*
	 * spin-lock is taken to protect against manipulation of video
	 * output buffer by the API (terminate video feed, re-use of video
	 * buffers). Mutex on the video-feed cannot be held here
	 * since SW demux holds a spin-lock while calling write_to_decoder
	 */
	spin_lock(&feed_data->video_buffer_lock);
	stream_buffer = feed_data->video_buffer;

	if (stream_buffer == NULL) {
		MPQ_DVB_DBG_PRINT(
			"%s: video_buffer released\n",
			__func__);
		spin_unlock(&feed_data->video_buffer_lock);
		return 0;
	}

	ts_header = (const struct ts_packet_header *)buf;

	pes_header = &feed_data->pes_header;

	/* Make sure this TS packet has a payload and not scrambled */
	if ((ts_header->sync_byte != 0x47) ||
		(ts_header->adaptation_field_control == 0) ||
		(ts_header->adaptation_field_control == 2) ||
		(ts_header->transport_scrambling_control)) {
		/* continue to next packet */
		spin_unlock(&feed_data->video_buffer_lock);
		return 0;
	}

	if (ts_header->payload_unit_start_indicator) { /* PUSI? */
		if (feed->pusi_seen) { /* Did we see PUSI before? */
			/*
			 * Double check that we are not in middle of
			 * previous PES header parsing.
			 */
			if (feed_data->pes_header_left_bytes != 0) {
				MPQ_DVB_ERR_PRINT(
					"%s: received PUSI"
					"while handling PES header"
					"of previous PES\n",
					__func__);
			}

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
		spin_unlock(&feed_data->video_buffer_lock);
		return 0; /* drop and wait for next packets */
	}

	ts_payload_offset = sizeof(struct ts_packet_header);

	/*
	 * Skip adaptation field if exists.
	 * Save discontinuity indicator if exists.
	 */
	if (ts_header->adaptation_field_control == 3) {
		const struct ts_adaptation_field *adaptation_field;
		adaptation_field = (const struct ts_adaptation_field *)
			(buf + ts_payload_offset);
		discontinuity_indicator =
			adaptation_field->discontinuity_indicator;
		ts_payload_offset += buf[ts_payload_offset] + 1;
	}

	/* 188 bytes: the size of a TS packet including the TS packet header */
	bytes_avail = 188 - ts_payload_offset;

	/* Get the mandatory fields of the video PES header */
	if (mpq_dmx_parse_mandatory_pes_header(feed, feed_data,
						pes_header, buf,
						&ts_payload_offset,
						&bytes_avail)) {
		spin_unlock(&feed_data->video_buffer_lock);
		return 0;
	}

	if (mpq_dmx_parse_remaining_pes_header(feed, feed_data,
						pes_header, buf,
						&ts_payload_offset,
						&bytes_avail)) {
		spin_unlock(&feed_data->video_buffer_lock);
		return 0;
	}

	/*
	 * If we reached here,
	 * then we are now at the PES payload data
	 */
	if (bytes_avail == 0) {
		spin_unlock(&feed_data->video_buffer_lock);
		return 0;
	}

	/*
	 * the decoder requires demux to do framing,
	 * so search for the patterns now.
	 */
	found_patterns = dvb_dmx_video_pattern_search(
				feed_data->patterns,
				feed_data->patterns_num,
				(buf + ts_payload_offset),
				bytes_avail,
				&feed_data->prefix_size,
				&framing_res);

	if (!feed_data->found_sequence_header_pattern) {
		for (i = 0; i < found_patterns; i++) {
			if ((framing_res.info[i].type ==
				DMX_IDX_MPEG_SEQ_HEADER) ||
			    (framing_res.info[i].type ==
				DMX_IDX_H264_SPS) ||
				(framing_res.info[i].type ==
				DMX_IDX_VC1_SEQ_HEADER)) {

				MPQ_DVB_DBG_PRINT(
					"%s: Found Sequence Pattern, buf %p, i = %d, offset = %d, type = %lld\n",
					__func__, buf, i,
					framing_res.info[i].offset,
					framing_res.info[i].type);

				first_pattern = i;
				feed_data->found_sequence_header_pattern = 1;
				ts_payload_offset +=
					framing_res.info[i].offset;
				bytes_avail -= framing_res.info[i].offset;

				if (framing_res.info[i].used_prefix_size) {
					feed_data->first_prefix_size =
						framing_res.info[i].
							used_prefix_size;
				}
				break;
			}
		}
	}

	/*
	 * If decoder requires demux to do framing,
	 * pass data to decoder only after sequence header
	 * or equivalent is found. Otherwise the data is dropped.
	 */
	if (!feed_data->found_sequence_header_pattern) {
		feed_data->prev_stc = curr_stc;
		spin_unlock(&feed_data->video_buffer_lock);
		return 0;
	}

	/* Update error counters based on TS header */
	feed_data->ts_packets_num++;
	feed_data->tei_errs += ts_header->transport_error_indicator;
	mpq_demux->decoder_ts_errors += ts_header->transport_error_indicator;
	mpq_dmx_check_continuity(feed_data,
				ts_header->continuity_counter,
				discontinuity_indicator);
	mpq_demux->decoder_cc_errors += feed_data->continuity_errs;

	/* Need to back-up the PTS information of the very first frame */
	if (feed_data->first_pts_dts_copy) {
		for (i = first_pattern; i < found_patterns; i++) {
			is_video_frame = mpq_dmx_is_video_frame(
					feed->video_codec,
					framing_res.info[i].type);

			if (is_video_frame == 1) {
				mpq_dmx_save_pts_dts(feed_data);
				feed_data->first_pts_dts_copy = 0;
				break;
			}
		}
	}

	/*
	 * write prefix used to find first Sequence pattern, if needed.
	 * feed_data->patterns[0]->pattern always contains the sequence
	 * header pattern.
	 */
	if (feed_data->first_prefix_size) {
		if (mpq_streambuffer_data_write(stream_buffer,
					(feed_data->patterns[0]->pattern),
					feed_data->first_prefix_size) < 0) {
			mpq_demux->decoder_drop_count +=
				feed_data->first_prefix_size;
			feed_data->ts_dropped_bytes +=
				feed_data->first_prefix_size;
			MPQ_DVB_DBG_PRINT("%s: could not write prefix\n",
				__func__);
		} else {
			MPQ_DVB_DBG_PRINT(
				"%s: Writing pattern prefix of size %d\n",
				__func__, feed_data->first_prefix_size);
			/*
			 * update the length of the data we report
			 * to include the size of the prefix that was used.
			 */
			feed_data->pending_pattern_len +=
				feed_data->first_prefix_size;
		}
	}

	feed->peslen += bytes_avail;
	pending_data_len += bytes_avail;

	meta_data.packet_type = DMX_FRAMING_INFO_PACKET;
	packet.user_data_len = sizeof(struct mpq_adapter_video_meta_data);

	/*
	 * Go over all the patterns that were found in this packet.
	 * For each pattern found, write the relevant data to the data
	 * buffer, then write the respective meta-data.
	 * Each pattern can only be reported when the next pattern is found
	 * (in order to know the data length).
	 * There are three possible cases for each pattern:
	 * 1. This is the very first pattern we found in any TS packet in this
	 *    feed.
	 * 2. This is the first pattern found in this TS packet, but we've
	 *    already found patterns in previous packets.
	 * 3. This is not the first pattern in this packet, i.e., we've
	 *    already found patterns in this TS packet.
	 */
	for (i = first_pattern; i < found_patterns; i++) {
		if (i == first_pattern) {
			/*
			 * The way to identify the very first pattern:
			 * 1. It's the first pattern found in this packet.
			 * 2. The pending_pattern_len, which indicates the
			 *    data length of the previous pattern that has
			 *    not yet been reported, is usually 0. However,
			 *    it may be larger than 0 if a prefix was used
			 *    to find this pattern (i.e., the pattern was
			 *    split over two TS packets). In that case,
			 *    pending_pattern_len equals first_prefix_size.
			 *    first_prefix_size is set to 0 later in this
			 *    function.
			 */
			if (feed_data->first_prefix_size ==
				feed_data->pending_pattern_len) {
				/*
				 * This is the very first pattern, so no
				 * previous pending frame data exists.
				 * Update frame info and skip to the
				 * next frame.
				 */
				feed_data->last_framing_match_type =
					framing_res.info[i].type;
				feed_data->last_pattern_offset =
					framing_res.info[i].offset;
				if (framing_res.info[i].used_prefix_size)
					feed_data->last_framing_match_stc =
						feed_data->prev_stc;
				else
					feed_data->last_framing_match_stc =
						curr_stc;
				continue;
			}
			/*
			 * This is the first pattern in this
			 * packet and previous frame from
			 * previous packet is pending for report
			 */
			bytes_to_write = framing_res.info[i].offset;
		} else {
			/*
			 * Previous pending frame is in
			 * the same packet
			 */
			bytes_to_write =
				framing_res.info[i].offset -
				feed_data->last_pattern_offset;
		}

		if (mpq_streambuffer_data_write(
			stream_buffer,
			(buf + ts_payload_offset + bytes_written),
			bytes_to_write) < 0) {
			mpq_demux->decoder_drop_count += bytes_to_write;
			feed_data->ts_dropped_bytes += bytes_to_write;
			MPQ_DVB_DBG_PRINT(
				"%s: Couldn't write %d bytes to data buffer\n",
				__func__, bytes_to_write);
		} else {
			bytes_written += bytes_to_write;
			pending_data_len -= bytes_to_write;
			feed_data->pending_pattern_len += bytes_to_write;
		}

		is_video_frame = mpq_dmx_is_video_frame(
				feed->video_codec,
				feed_data->last_framing_match_type);
		if (is_video_frame == 1) {
			mpq_dmx_write_pts_dts(feed_data,
				&(meta_data.info.framing.pts_dts_info));
			mpq_dmx_save_pts_dts(feed_data);

			packet.raw_data_len = feed_data->pending_pattern_len -
				framing_res.info[i].used_prefix_size;
			packet.raw_data_offset = feed_data->frame_offset;
			meta_data.info.framing.pattern_type =
				feed_data->last_framing_match_type;
			meta_data.info.framing.stc =
				feed_data->last_framing_match_stc;
			meta_data.info.framing.continuity_error_counter =
				feed_data->continuity_errs;
			meta_data.info.framing.
				transport_error_indicator_counter =
				 feed_data->tei_errs;
			meta_data.info.framing.ts_dropped_bytes =
				feed_data->ts_dropped_bytes;
			meta_data.info.framing.ts_packets_num =
				feed_data->ts_packets_num;

			mpq_streambuffer_get_buffer_handle(
				stream_buffer,
				0,	/* current write buffer handle */
				&packet.raw_data_handle);

			mpq_dmx_update_decoder_stat(mpq_demux);

			/*
			 * writing meta-data that includes
			 * the framing information
			 */
			if (mpq_streambuffer_pkt_write(stream_buffer,
				&packet,
				(u8 *)&meta_data) < 0) {
				MPQ_DVB_ERR_PRINT(
					"%s: "
					"Couldn't write packet. "
					"Should never happen\n",
					__func__);
			}

			mpq_dmx_prepare_es_event_data(
				&packet, &meta_data, feed_data,
				stream_buffer, &data);

			feed->data_ready_cb.ts(&feed->feed.ts, &data);

			mpq_streambuffer_get_data_rw_offset(
				feed_data->video_buffer,
				NULL,
				&feed_data->frame_offset);

			/*
			 * In linear buffers, after writing the packet
			 * we switched over to a new linear buffer for the new
			 * frame. In that case, we should re-write the prefix
			 * of the existing frame if any exists.
			 */
			if ((MPQ_STREAMBUFFER_BUFFER_MODE_LINEAR ==
				 feed_data->video_buffer->mode) &&
				framing_res.info[i].used_prefix_size) {
				ret = mpq_streambuffer_data_write(stream_buffer,
					feed_data->prev_pattern +
					 DVB_DMX_MAX_PATTERN_LEN -
					 framing_res.info[i].used_prefix_size,
					framing_res.info[i].used_prefix_size);

				if (ret < 0) {
					feed_data->pending_pattern_len = 0;
					mpq_demux->decoder_drop_count +=
					 framing_res.info[i].used_prefix_size;
					feed_data->ts_dropped_bytes +=
					 framing_res.info[i].used_prefix_size;
				} else {
					feed_data->pending_pattern_len =
					 framing_res.info[i].used_prefix_size;
				}
			} else {
				s32 offset = (s32)feed_data->frame_offset;
				u32 buff_size =
				 feed_data->video_buffer->buffers[0].size;

				offset -= framing_res.info[i].used_prefix_size;
				offset += (offset < 0) ? buff_size : 0;
				feed_data->pending_pattern_len =
					framing_res.info[i].used_prefix_size;

				if (MPQ_STREAMBUFFER_BUFFER_MODE_RING ==
					feed_data->video_buffer->mode) {
					feed_data->frame_offset = (u32)offset;
				}
			}
		}

		/* save the last match for next time */
		feed_data->last_framing_match_type =
			framing_res.info[i].type;
		feed_data->last_pattern_offset =
			framing_res.info[i].offset;
		if (framing_res.info[i].used_prefix_size)
			feed_data->last_framing_match_stc = feed_data->prev_stc;
		else
			feed_data->last_framing_match_stc = curr_stc;
	}

	feed_data->prev_stc = curr_stc;
	feed_data->first_prefix_size = 0;

	/*
	 * Save the trailing of the TS packet as we might have a pattern
	 * split that we need to re-use when closing the next
	 * video linear buffer.
	 */
	if (MPQ_STREAMBUFFER_BUFFER_MODE_LINEAR ==
		feed_data->video_buffer->mode)
		memcpy(feed_data->prev_pattern,
			buf + 188 - DVB_DMX_MAX_PATTERN_LEN,
			DVB_DMX_MAX_PATTERN_LEN);

	if (pending_data_len) {
		ret = mpq_streambuffer_data_write(
			stream_buffer,
			(buf + ts_payload_offset + bytes_written),
			pending_data_len);

		if (ret < 0) {
			mpq_demux->decoder_drop_count += pending_data_len;
			feed_data->ts_dropped_bytes += pending_data_len;
			MPQ_DVB_DBG_PRINT(
				"%s: Couldn't write %d bytes to data buffer\n",
				__func__, pending_data_len);
		} else {
			feed_data->pending_pattern_len += pending_data_len;
		}
	}

	spin_unlock(&feed_data->video_buffer_lock);
	return 0;
}

static int mpq_dmx_process_video_packet_no_framing(
			struct dvb_demux_feed *feed,
			const u8 *buf,
			u64 curr_stc)
{
	int bytes_avail;
	u32 ts_payload_offset;
	struct mpq_video_feed_info *feed_data;
	const struct ts_packet_header *ts_header;
	struct mpq_streambuffer *stream_buffer;
	struct pes_packet_header *pes_header;
	struct mpq_demux *mpq_demux;
	struct mpq_feed *mpq_feed;
	int discontinuity_indicator = 0;
	struct dmx_data_ready data;

	mpq_demux = feed->demux->priv;
	mpq_feed = feed->priv;
	feed_data = &mpq_feed->video_info;

	/*
	 * spin-lock is taken to protect against manipulation of video
	 * output buffer by the API (terminate video feed, re-use of video
	 * buffers). Mutex on the video-feed cannot be held here
	 * since SW demux holds a spin-lock while calling write_to_decoder
	 */
	spin_lock(&feed_data->video_buffer_lock);
	stream_buffer = feed_data->video_buffer;
	if (stream_buffer == NULL) {
		MPQ_DVB_DBG_PRINT(
			"%s: video_buffer released\n",
			__func__);
		spin_unlock(&feed_data->video_buffer_lock);
		return 0;
	}

	ts_header = (const struct ts_packet_header *)buf;

	pes_header = &feed_data->pes_header;

	/* Make sure this TS packet has a payload and not scrambled */
	if ((ts_header->sync_byte != 0x47) ||
		(ts_header->adaptation_field_control == 0) ||
		(ts_header->adaptation_field_control == 2) ||
		(ts_header->transport_scrambling_control)) {
		/* continue to next packet */
		spin_unlock(&feed_data->video_buffer_lock);
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
				packet.raw_data_len = feed->peslen;
				mpq_streambuffer_get_buffer_handle(
					stream_buffer,
					0, /* current write buffer handle */
					&packet.raw_data_handle);
				packet.raw_data_offset =
					feed_data->frame_offset;
				packet.user_data_len =
					sizeof(struct
						mpq_adapter_video_meta_data);

				mpq_dmx_write_pts_dts(feed_data,
					&(meta_data.info.pes.pts_dts_info));

				/* Mark that we detected start of new PES */
				feed_data->first_pts_dts_copy = 1;

				meta_data.packet_type = DMX_PES_PACKET;
				meta_data.info.pes.stc = feed_data->prev_stc;

				mpq_dmx_update_decoder_stat(mpq_demux);

				if (mpq_streambuffer_pkt_write(
						stream_buffer,
						&packet,
						(u8 *)&meta_data) < 0)
					MPQ_DVB_ERR_PRINT(
						"%s: "
						"Couldn't write packet. "
						"Should never happen\n",
						__func__);

				/* Save write offset where new PES will begin */
				mpq_streambuffer_get_data_rw_offset(
					stream_buffer,
					NULL,
					&feed_data->frame_offset);

				mpq_dmx_prepare_es_event_data(
					&packet, &meta_data,
					feed_data,
					stream_buffer, &data);

				feed->data_ready_cb.ts(
					&feed->feed.ts, &data);
			} else {
				MPQ_DVB_ERR_PRINT(
					"%s: received PUSI"
					"while handling PES header"
					"of previous PES\n",
					__func__);
			}

			/* Reset PES info */
			feed->peslen = 0;
			feed_data->pes_header_offset = 0;
			feed_data->pes_header_left_bytes =
				PES_MANDATORY_FIELDS_LEN;
		} else {
			feed->pusi_seen = 1;
		}

		feed_data->prev_stc = curr_stc;
	}

	/*
	 * Parse PES data only if PUSI was encountered,
	 * otherwise the data is dropped
	 */
	if (!feed->pusi_seen) {
		spin_unlock(&feed_data->video_buffer_lock);
		return 0; /* drop and wait for next packets */
	}

	ts_payload_offset = sizeof(struct ts_packet_header);

	/*
	 * Skip adaptation field if exists.
	 * Save discontinuity indicator if exists.
	 */
	if (ts_header->adaptation_field_control == 3) {
		const struct ts_adaptation_field *adaptation_field;
		adaptation_field = (const struct ts_adaptation_field *)
			(buf + ts_payload_offset);
		discontinuity_indicator =
			adaptation_field->discontinuity_indicator;
		ts_payload_offset += buf[ts_payload_offset] + 1;
	}

	/* 188 bytes: size of a TS packet including the TS packet header */
	bytes_avail = 188 - ts_payload_offset;

	/* Get the mandatory fields of the video PES header */
	if (mpq_dmx_parse_mandatory_pes_header(feed, feed_data,
						pes_header, buf,
						&ts_payload_offset,
						&bytes_avail)) {
		spin_unlock(&feed_data->video_buffer_lock);
		return 0;
	}

	if (mpq_dmx_parse_remaining_pes_header(feed, feed_data,
						pes_header, buf,
						&ts_payload_offset,
						&bytes_avail)) {
		spin_unlock(&feed_data->video_buffer_lock);
		return 0;
	}

	/*
	 * If we reached here,
	 * then we are now at the PES payload data
	 */
	if (bytes_avail == 0) {
		spin_unlock(&feed_data->video_buffer_lock);
		return 0;
	}

	/*
	 * Need to back-up the PTS information
	 * of the start of new PES
	 */
	if (feed_data->first_pts_dts_copy) {
		mpq_dmx_save_pts_dts(feed_data);
		feed_data->first_pts_dts_copy = 0;
	}

	/* Update error counters based on TS header */
	feed_data->ts_packets_num++;
	feed_data->tei_errs += ts_header->transport_error_indicator;
	mpq_demux->decoder_ts_errors += ts_header->transport_error_indicator;
	mpq_dmx_check_continuity(feed_data,
				ts_header->continuity_counter,
				discontinuity_indicator);
	mpq_demux->decoder_cc_errors += feed_data->continuity_errs;

	if (mpq_streambuffer_data_write(
				stream_buffer,
				buf+ts_payload_offset,
				bytes_avail) < 0) {
		mpq_demux->decoder_drop_count += bytes_avail;
		feed_data->ts_dropped_bytes += bytes_avail;
	} else {
		feed->peslen += bytes_avail;
	}

	spin_unlock(&feed_data->video_buffer_lock);

	return 0;
}

int mpq_dmx_decoder_buffer_status(struct dvb_demux_feed *feed,
		struct dmx_buffer_status *dmx_buffer_status)
{
	struct mpq_demux *mpq_demux = feed->demux->priv;
	struct mpq_video_feed_info *feed_data;
	struct mpq_streambuffer *video_buff;
	struct mpq_feed *mpq_feed;

	if (!dvb_dmx_is_video_feed(feed)) {
		MPQ_DVB_ERR_PRINT(
			"%s: Invalid feed type %d\n",
			__func__,
			feed->pes_type);
		return -EINVAL;
	}

	mutex_lock(&mpq_demux->mutex);

	mpq_feed = feed->priv;
	feed_data = &mpq_feed->video_info;
	video_buff = feed_data->video_buffer;
	if (!video_buff) {
		mutex_unlock(&mpq_demux->mutex);
		return -EINVAL;
	}

	dmx_buffer_status->error = video_buff->raw_data.error;

	if (MPQ_STREAMBUFFER_BUFFER_MODE_LINEAR == video_buff->mode) {
		dmx_buffer_status->fullness =
			video_buff->buffers[0].size *
			video_buff->pending_buffers_count;
		dmx_buffer_status->free_bytes =
			video_buff->buffers[0].size *
			(video_buff->buffers_num -
			video_buff->pending_buffers_count);
		dmx_buffer_status->size =
			video_buff->buffers[0].size *
			video_buff->buffers_num;
	} else {
		dmx_buffer_status->fullness =
			mpq_streambuffer_data_avail(video_buff);
		dmx_buffer_status->free_bytes =
			mpq_streambuffer_data_free(video_buff);
		dmx_buffer_status->size = video_buff->buffers[0].size;
	}

	mpq_streambuffer_get_data_rw_offset(
		video_buff,
		&dmx_buffer_status->read_offset,
		&dmx_buffer_status->write_offset);

	mutex_unlock(&mpq_demux->mutex);

	return 0;
}
EXPORT_SYMBOL(mpq_dmx_decoder_buffer_status);

int mpq_dmx_process_video_packet(
			struct dvb_demux_feed *feed,
			const u8 *buf)
{
	u64 curr_stc;
	struct mpq_demux *mpq_demux = feed->demux->priv;

	if ((mpq_demux->source >= DMX_SOURCE_DVR0) &&
		(mpq_demux->demux.tsp_format != DMX_TSP_FORMAT_192_TAIL)) {
		curr_stc = 0;
	} else {
		curr_stc = buf[STC_LOCATION_IDX + 2] << 16;
		curr_stc += buf[STC_LOCATION_IDX + 1] << 8;
		curr_stc += buf[STC_LOCATION_IDX];
		curr_stc *= 256; /* convert from 105.47 KHZ to 27MHz */
	}

	if (!video_framing)
		return mpq_dmx_process_video_packet_no_framing(feed, buf,
				curr_stc);
	else
		return mpq_dmx_process_video_packet_framing(feed, buf,
				curr_stc);
}
EXPORT_SYMBOL(mpq_dmx_process_video_packet);

/*
 * Extract the PCR field and discontinuity indicator from a TS packet buffer
 * @buf: TSP buffer
 * @pcr: returned PCR value
 * @dci: returned discontinuity indicator
 * Returns 1 if PCR was extracted, 0 otherwise.
 */
static int mpq_dmx_extract_pcr_and_dci(const u8 *buf, u64 *pcr, int *dci)
{
	const struct ts_packet_header *ts_header;
	const struct ts_adaptation_field *adaptation_field;

	if (buf == NULL || pcr == NULL || dci == NULL)
		return 0;

	ts_header = (const struct ts_packet_header *)buf;

	/* Make sure this TS packet has a adaptation field */
	if ((ts_header->sync_byte != 0x47) ||
		(ts_header->adaptation_field_control == 0) ||
		(ts_header->adaptation_field_control == 1) ||
		ts_header->transport_error_indicator)
		return 0;

	adaptation_field = (const struct ts_adaptation_field *)
			(buf + sizeof(struct ts_packet_header));

	if ((!adaptation_field->adaptation_field_length) ||
		(!adaptation_field->PCR_flag))
		return 0; /* 0 adaptation field or no PCR */

	*pcr = ((u64)adaptation_field->program_clock_reference_base_1) << 25;
	*pcr += ((u64)adaptation_field->program_clock_reference_base_2) << 17;
	*pcr += ((u64)adaptation_field->program_clock_reference_base_3) << 9;
	*pcr += ((u64)adaptation_field->program_clock_reference_base_4) << 1;
	*pcr += adaptation_field->program_clock_reference_base_5;
	*pcr *= 300;
	*pcr += (((u64)adaptation_field->program_clock_reference_ext_1) << 8) +
		adaptation_field->program_clock_reference_ext_2;

	*dci = adaptation_field->discontinuity_indicator;

	return 1;
}

int mpq_dmx_process_pcr_packet(
			struct dvb_demux_feed *feed,
			const u8 *buf)
{
	u64 stc;
	struct dmx_data_ready data;
	struct mpq_demux *mpq_demux = feed->demux->priv;

	if (0 == mpq_dmx_extract_pcr_and_dci(buf, &data.pcr.pcr,
		&data.pcr.disc_indicator_set))
		return 0;

	/*
	 * When we play from front-end, we configure HW
	 * to output the extra timestamp, if we are playing
	 * from DVR, we don't have a timestamp if the packet
	 * format is not 192-tail.
	 */
	if ((mpq_demux->source >= DMX_SOURCE_DVR0) &&
		(mpq_demux->demux.tsp_format != DMX_TSP_FORMAT_192_TAIL)) {
		stc = 0;
	} else {
		stc = buf[STC_LOCATION_IDX + 2] << 16;
		stc += buf[STC_LOCATION_IDX + 1] << 8;
		stc += buf[STC_LOCATION_IDX];
		stc *= 256; /* convert from 105.47 KHZ to 27MHz */
	}

	data.data_length = 0;
	data.pcr.stc = stc;
	data.status = DMX_OK_PCR;
	feed->data_ready_cb.ts(&feed->feed.ts, &data);

	return 0;
}
EXPORT_SYMBOL(mpq_dmx_process_pcr_packet);

static int mpq_dmx_decoder_eos_cmd(struct mpq_feed *mpq_feed)
{
	struct mpq_video_feed_info *feed_data = &mpq_feed->video_info;
	struct mpq_streambuffer *stream_buffer;
	struct mpq_streambuffer_packet_header oob_packet;
	struct mpq_adapter_video_meta_data oob_meta_data;
	int ret;

	spin_lock(&feed_data->video_buffer_lock);
	stream_buffer = feed_data->video_buffer;

	if (stream_buffer == NULL) {
		MPQ_DVB_DBG_PRINT("%s: video_buffer released\n", __func__);
		spin_unlock(&feed_data->video_buffer_lock);
		return 0;
	}

	memset(&oob_packet, 0, sizeof(oob_packet));
	oob_packet.user_data_len = sizeof(oob_meta_data);
	oob_meta_data.packet_type = DMX_EOS_PACKET;

	ret = mpq_streambuffer_pkt_write(stream_buffer, &oob_packet,
					(u8 *)&oob_meta_data);

	spin_unlock(&feed_data->video_buffer_lock);
	return ret;
}

void mpq_dmx_convert_tts(struct dvb_demux_feed *feed,
			const u8 timestamp[TIMESTAMP_LEN],
			u64 *timestampIn27Mhz)
{
	if (unlikely(!timestampIn27Mhz))
		return;

	*timestampIn27Mhz = timestamp[2] << 16;
	*timestampIn27Mhz += timestamp[1] << 8;
	*timestampIn27Mhz += timestamp[0];
	*timestampIn27Mhz *= 256; /* convert from 105.47 KHZ to 27MHz */
}
EXPORT_SYMBOL(mpq_dmx_convert_tts);

int mpq_sdmx_open_session(struct mpq_demux *mpq_demux)
{
	enum sdmx_status ret = SDMX_SUCCESS;
	enum sdmx_proc_mode proc_mode;
	enum sdmx_pkt_format pkt_format;

	MPQ_DVB_DBG_PRINT("%s: ref_count %d\n",
		__func__, mpq_demux->sdmx_session_ref_count);

	if (mpq_demux->sdmx_session_ref_count) {
		/* session is already open */
		mpq_demux->sdmx_session_ref_count++;
		return ret;
	}

	proc_mode = (mpq_demux->demux.playback_mode == DMX_PB_MODE_PUSH) ?
		SDMX_PUSH_MODE : SDMX_PULL_MODE;
	MPQ_DVB_DBG_PRINT(
		"%s: Proc mode = %s\n",
		__func__, SDMX_PUSH_MODE == proc_mode ? "Push" : "Pull");

	if (mpq_demux->source < DMX_SOURCE_DVR0) {
		pkt_format = SDMX_192_BYTE_PKT;
	} else if (DMX_TSP_FORMAT_188 == mpq_demux->demux.tsp_format) {
		pkt_format = SDMX_188_BYTE_PKT;
	} else if (DMX_TSP_FORMAT_192_TAIL == mpq_demux->demux.tsp_format) {
		pkt_format = SDMX_192_BYTE_PKT;
	} else {
		MPQ_DVB_ERR_PRINT("%s: invalid tsp format\n", __func__);
		return -EINVAL;
	}

	MPQ_DVB_DBG_PRINT("%s: (%s) source, packet format: %d\n",
		 __func__,
		 (mpq_demux->source < DMX_SOURCE_DVR0) ?
		 "frontend" : "DVR", pkt_format);

	/* open session and set configuration */
	ret = sdmx_open_session(&mpq_demux->sdmx_session_handle);
	if (ret != SDMX_SUCCESS) {
		MPQ_DVB_ERR_PRINT("%s: Could not open session. ret=%d\n",
			__func__ , ret);
		return ret;
	}

	MPQ_DVB_DBG_PRINT("%s: new session_handle = %d\n",
		__func__ , mpq_demux->sdmx_session_handle);

	ret = sdmx_set_session_cfg(mpq_demux->sdmx_session_handle,
		proc_mode,
		SDMX_PKT_ENC_MODE,
		pkt_format,
		mpq_sdmx_scramble_odd,
		mpq_sdmx_scramble_even);
	if (ret != SDMX_SUCCESS) {
		MPQ_DVB_ERR_PRINT("%s: Could not set session config. ret=%d\n",
			__func__, ret);
		sdmx_close_session(mpq_demux->sdmx_session_handle);
		mpq_demux->sdmx_session_handle = SDMX_INVALID_SESSION_HANDLE;
		return -EINVAL;
	}

	ret = sdmx_set_log_level(mpq_demux->sdmx_session_handle,
		mpq_demux->sdmx_log_level);
	if (ret != SDMX_SUCCESS) {
		MPQ_DVB_ERR_PRINT("%s: Could not set log level. ret=%d\n",
				__func__, ret);
		/* Don't fail open session if just log level setting failed */
		ret = 0;
	}

	mpq_demux->sdmx_process_count = 0;
	mpq_demux->sdmx_process_time_sum = 0;
	mpq_demux->sdmx_process_time_average = 0;
	mpq_demux->sdmx_process_time_max = 0;
	mpq_demux->sdmx_process_packets_sum = 0;
	mpq_demux->sdmx_process_packets_average = 0;
	mpq_demux->sdmx_process_packets_min = 0;

	mpq_demux->sdmx_session_ref_count++;
	return ret;
}
EXPORT_SYMBOL(mpq_sdmx_open_session);

int mpq_sdmx_close_session(struct mpq_demux *mpq_demux)
{
	int ret = 0;
	enum sdmx_status status;

	MPQ_DVB_DBG_PRINT("%s: session_handle = %d, ref_count %d\n",
			__func__,
			mpq_demux->sdmx_session_handle,
			mpq_demux->sdmx_session_ref_count);

	if (!mpq_demux->sdmx_session_ref_count)
		return -EINVAL;

	if (mpq_demux->sdmx_session_ref_count == 1) {
		status = sdmx_close_session(mpq_demux->sdmx_session_handle);
		if (status != SDMX_SUCCESS) {
			MPQ_DVB_ERR_PRINT("%s: sdmx_close_session failed %d\n",
				__func__, status);
		}
		mpq_demux->sdmx_eos = 0;
		mpq_demux->sdmx_session_handle = SDMX_INVALID_SESSION_HANDLE;
	}

	mpq_demux->sdmx_session_ref_count--;

	return ret;
}
EXPORT_SYMBOL(mpq_sdmx_close_session);

static int mpq_sdmx_get_buffer_chunks(struct mpq_demux *mpq_demux,
	struct ion_handle *buff_handle,
	u32 actual_buff_size,
	struct sdmx_buff_descr buff_chunks[SDMX_MAX_PHYSICAL_CHUNKS])
{
	int i;
	struct sg_table *sg_ptr;
	struct scatterlist *sg;
	u32 chunk_size;

	memset(buff_chunks, 0,
		sizeof(struct sdmx_buff_descr) * SDMX_MAX_PHYSICAL_CHUNKS);

	sg_ptr = ion_sg_table(mpq_demux->ion_client, buff_handle);
	if (sg_ptr == NULL) {
		MPQ_DVB_ERR_PRINT("%s: ion_sg_table failed\n",
			__func__);
		return -EINVAL;
	}

	if (sg_ptr->nents == 0) {
		MPQ_DVB_ERR_PRINT("%s: num of scattered entries is 0\n",
			__func__);
		return -EINVAL;
	}

	if (sg_ptr->nents > SDMX_MAX_PHYSICAL_CHUNKS) {
		MPQ_DVB_ERR_PRINT(
			"%s: num of scattered entries %d greater than max supported %d\n",
			__func__, sg_ptr->nents, SDMX_MAX_PHYSICAL_CHUNKS);
		return -EINVAL;
	}

	sg = sg_ptr->sgl;
	for (i = 0; i < sg_ptr->nents; i++) {
		/*
		 * NOTE: the following casting to u32 must be done
		 * as long as TZ does not support LPAE. Once TZ supports
		 * LPAE SDMX interface needs to be updated accordingly.
		 */
		if (sg_dma_address(sg) > 0xFFFFFFFF)
			MPQ_DVB_ERR_PRINT(
				"%s: WARNNING - physical address %pa is larger than 32bits!\n",
				__func__, &sg_dma_address(sg));

		buff_chunks[i].base_addr =
			(void *)(u32)sg_dma_address(sg);

		if (sg->length > actual_buff_size)
			chunk_size = actual_buff_size;
		else
			chunk_size = sg->length;

		buff_chunks[i].size = chunk_size;
		sg = sg_next(sg);
		actual_buff_size -= chunk_size;
	}

	return 0;
}

static int mpq_sdmx_init_data_buffer(struct mpq_demux *mpq_demux,
	struct mpq_feed *feed, u32 *num_buffers,
	struct sdmx_data_buff_descr buf_desc[DMX_MAX_DECODER_BUFFER_NUM],
	enum sdmx_buf_mode *buf_mode)
{
	struct dvb_demux_feed *dvbdmx_feed = feed->dvb_demux_feed;
	struct dvb_ringbuffer *buffer;
	struct mpq_video_feed_info *feed_data = &feed->video_info;
	struct ion_handle *sdmx_buff;
	int ret;
	int i;

	*buf_mode = SDMX_RING_BUF;

	if (dvb_dmx_is_video_feed(feed->dvb_demux_feed)) {
		if (feed_data->buffer_desc.decoder_buffers_num > 1)
			*buf_mode = SDMX_LINEAR_GROUP_BUF;
		*num_buffers = feed_data->buffer_desc.decoder_buffers_num;

		for (i = 0; i < *num_buffers; i++) {
			buf_desc[i].length =
				feed_data->buffer_desc.desc[i].size;

			ret = mpq_sdmx_get_buffer_chunks(mpq_demux,
					feed_data->buffer_desc.ion_handle[i],
					buf_desc[i].length,
					buf_desc[i].buff_chunks);
			if (ret) {
				MPQ_DVB_ERR_PRINT(
					"%s: mpq_sdmx_get_buffer_chunks failed\n",
					__func__);
				return ret;
			}
		}

		return 0;
	}

	*num_buffers = 1;
	if (dvb_dmx_is_sec_feed(dvbdmx_feed) ||
		dvb_dmx_is_pcr_feed(dvbdmx_feed)) {
		buffer = &feed->sdmx_buf;
		sdmx_buff = feed->sdmx_buf_handle;
	} else {
		buffer = (struct dvb_ringbuffer *)
			dvbdmx_feed->feed.ts.buffer.ringbuff;
		sdmx_buff = dvbdmx_feed->feed.ts.buffer.priv_handle;
	}

	if (sdmx_buff == NULL) {
		MPQ_DVB_ERR_PRINT(
			"%s: Invalid buffer allocation\n",
			__func__);
		return -ENOMEM;
	}

	buf_desc[0].length = buffer->size;
	ret = mpq_sdmx_get_buffer_chunks(mpq_demux, sdmx_buff,
		buf_desc[0].length,
		buf_desc[0].buff_chunks);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_sdmx_get_buffer_chunks failed\n",
			__func__);
		return ret;
	}

	return 0;
}

static int mpq_sdmx_filter_setup(struct mpq_demux *mpq_demux,
	struct dvb_demux_feed *dvbdmx_feed)
{
	int ret = 0;
	struct mpq_feed *feed;
	struct mpq_feed *main_rec_feed;
	struct sdmx_buff_descr metadata_buff_desc;
	struct sdmx_data_buff_descr *data_buff_desc = NULL;
	u32 data_buf_num = DMX_MAX_DECODER_BUFFER_NUM;
	enum sdmx_buf_mode buf_mode;
	enum sdmx_raw_out_format ts_out_format = SDMX_188_OUTPUT;
	u32 filter_flags = 0;

	feed = dvbdmx_feed->priv;

	if (dvb_dmx_is_sec_feed(dvbdmx_feed)) {
		feed->filter_type = SDMX_SECTION_FILTER;
		if (dvbdmx_feed->feed.sec.check_crc)
			filter_flags |= SDMX_FILTER_FLAG_VERIFY_SECTION_CRC;
		MPQ_DVB_DBG_PRINT("%s: SDMX_SECTION_FILTER\n", __func__);
	} else if (dvb_dmx_is_pcr_feed(dvbdmx_feed)) {
		feed->filter_type = SDMX_PCR_FILTER;
		MPQ_DVB_DBG_PRINT("%s: SDMX_PCR_FILTER\n", __func__);
	} else if (dvb_dmx_is_video_feed(dvbdmx_feed)) {
		feed->filter_type = SDMX_SEPARATED_PES_FILTER;
		MPQ_DVB_DBG_PRINT("%s: SDMX_SEPARATED_PES_FILTER\n", __func__);
	} else if (dvb_dmx_is_rec_feed(dvbdmx_feed)) {
		feed->filter_type = SDMX_RAW_FILTER;
		switch (dvbdmx_feed->tsp_out_format) {
		case (DMX_TSP_FORMAT_188):
			ts_out_format = SDMX_188_OUTPUT;
			break;
		case (DMX_TSP_FORMAT_192_HEAD):
			ts_out_format = SDMX_192_HEAD_OUTPUT;
			break;
		case (DMX_TSP_FORMAT_192_TAIL):
			ts_out_format = SDMX_192_TAIL_OUTPUT;
			break;
		default:
			MPQ_DVB_ERR_PRINT(
				"%s: Unsupported TS output format %d\n",
				__func__, dvbdmx_feed->tsp_out_format);
			return -EINVAL;
		}
		MPQ_DVB_DBG_PRINT("%s: SDMX_RAW_FILTER\n", __func__);
	} else {
		feed->filter_type = SDMX_PES_FILTER;
		MPQ_DVB_DBG_PRINT("%s: SDMX_PES_FILTER\n", __func__);
	}

	data_buff_desc = vmalloc(
			sizeof(*data_buff_desc)*DMX_MAX_DECODER_BUFFER_NUM);
	if (!data_buff_desc) {
		MPQ_DVB_ERR_PRINT(
			"%s: failed to allocate memory for data buffer\n",
			__func__);
		return -ENOMEM;
	}

	/*
	 * Recording feed sdmx filter handle lookup:
	 * In case this is a recording filter with multiple feeds,
	 * this feed is either the first feed of a new recording filter,
	 * or it is another feed of an existing filter for which a filter was
	 * already opened with sdmx. In such case, we need to look up in the
	 * feed pool for a allocated feed with same output buffer (meaning they
	 * belong to the same filter) and to use the already allocated sdmx
	 * filter handle.
	 */
	if (feed->filter_type == SDMX_RAW_FILTER)
		main_rec_feed = mpq_sdmx_lookup_feed(dvbdmx_feed);
	else
		main_rec_feed = NULL;

	/*
	 * If this PID is not part of existing recording filter,
	 * configure a new filter to SDMX.
	 */
	if (!main_rec_feed) {
		feed->secondary_feed = 0;

		MPQ_DVB_DBG_PRINT(
			"%s: Adding new sdmx filter, pid %d, flags=0x%X, ts_out_format=%d\n",
			__func__, dvbdmx_feed->pid, filter_flags,
			ts_out_format);

		/* Meta-data initialization,
		 * Recording filters do no need meta-data buffers.
		 */
		if (dvb_dmx_is_rec_feed(dvbdmx_feed)) {
			metadata_buff_desc.base_addr = 0;
			metadata_buff_desc.size = 0;
		} else {
			ret = mpq_sdmx_init_metadata_buffer(mpq_demux, feed,
				&metadata_buff_desc);
			if (ret) {
				MPQ_DVB_ERR_PRINT(
					"%s: Failed to initialize metadata buffer. ret=%d\n",
					__func__, ret);
				goto sdmx_filter_setup_failed;
			}
		}

		ret = mpq_sdmx_init_data_buffer(mpq_demux, feed, &data_buf_num,
			data_buff_desc, &buf_mode);
		if (ret) {
			MPQ_DVB_ERR_PRINT(
				"%s: Failed to initialize data buffer. ret=%d\n",
				__func__, ret);
			mpq_sdmx_terminate_metadata_buffer(feed);
			goto sdmx_filter_setup_failed;
		}
		ret = sdmx_add_filter(mpq_demux->sdmx_session_handle,
			dvbdmx_feed->pid,
			feed->filter_type,
			&metadata_buff_desc,
			buf_mode,
			data_buf_num,
			data_buff_desc,
			&feed->sdmx_filter_handle,
			ts_out_format,
			filter_flags);
		if (ret) {
			MPQ_DVB_ERR_PRINT(
				"%s: SDMX_add_filter failed. ret = %d\n",
				__func__, ret);
			ret = -ENODEV;
			mpq_sdmx_terminate_metadata_buffer(feed);
			goto sdmx_filter_setup_failed;
		}

		MPQ_DVB_DBG_PRINT(
			"%s: feed=0x%p, filter pid=%d, handle=%d, data buffer(s)=%d, size=%d\n",
			__func__, feed, dvbdmx_feed->pid,
			feed->sdmx_filter_handle,
			data_buf_num, data_buff_desc[0].length);

		mpq_demux->sdmx_filter_count++;
	} else {
		MPQ_DVB_DBG_PRINT(
			"%s: Adding RAW pid to sdmx, pid %d\n",
			__func__, dvbdmx_feed->pid);

		feed->secondary_feed = 1;
		feed->sdmx_filter_handle = main_rec_feed->sdmx_filter_handle;
		ret = sdmx_add_raw_pid(mpq_demux->sdmx_session_handle,
			feed->sdmx_filter_handle, dvbdmx_feed->pid);
		if (ret) {
			MPQ_DVB_ERR_PRINT(
				"%s: FAILED to add raw pid, ret=%d\n",
				__func__, ret);
			ret = -ENODEV;
			goto sdmx_filter_setup_failed;
		}
	}

	/*
	 * If pid has a key ladder id associated, we need to
	 * set it to SDMX.
	 */
	if (dvbdmx_feed->secure_mode.is_secured &&
		dvbdmx_feed->cipher_ops.operations_count) {
		MPQ_DVB_DBG_PRINT(
			"%s: set key-ladder %d to PID %d\n",
			__func__,
			dvbdmx_feed->cipher_ops.operations[0].key_ladder_id,
			dvbdmx_feed->cipher_ops.pid);

		ret = sdmx_set_kl_ind(mpq_demux->sdmx_session_handle,
			dvbdmx_feed->cipher_ops.pid,
			dvbdmx_feed->cipher_ops.operations[0].key_ladder_id);

		if (ret) {
			MPQ_DVB_ERR_PRINT(
				"%s: FAILED to set key ladder, ret=%d\n",
				__func__, ret);
		}
	}

	vfree(data_buff_desc);
	return 0;

sdmx_filter_setup_failed:
	vfree(data_buff_desc);
	return ret;
}

/**
 * mpq_sdmx_init_feed - initialize secure demux related elements of mpq feed
 *
 * @mpq_demux: mpq_demux object
 * @mpq_feed: mpq_feed object
 *
 * Note: the function assumes mpq_demux->mutex locking is done by caller.
 */
static int mpq_sdmx_init_feed(struct mpq_demux *mpq_demux,
	struct mpq_feed *mpq_feed)
{
	int ret;

	ret = mpq_sdmx_open_session(mpq_demux);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_sdmx_open_session failed, ret=%d\n",
			__func__, ret);

		ret = -ENODEV;
		goto init_sdmx_feed_failed;
	}

	/* PCR and sections have internal buffer for SDMX */
	if (dvb_dmx_is_pcr_feed(mpq_feed->dvb_demux_feed))
		ret = mpq_sdmx_alloc_data_buf(mpq_feed, SDMX_PCR_BUFFER_SIZE);
	else if (dvb_dmx_is_sec_feed(mpq_feed->dvb_demux_feed))
		ret = mpq_sdmx_alloc_data_buf(mpq_feed,
			SDMX_SECTION_BUFFER_SIZE);
	else
		ret = 0;

	if (ret) {
		MPQ_DVB_ERR_PRINT("%s: init buffer failed, ret=%d\n",
			__func__, ret);
		goto init_sdmx_feed_failed_free_sdmx;
	}

	ret = mpq_sdmx_filter_setup(mpq_demux, mpq_feed->dvb_demux_feed);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_sdmx_filter_setup failed, ret=%d\n",
			__func__, ret);
		goto init_sdmx_feed_failed_free_data_buff;
	}

	mpq_demux->num_secure_feeds++;
	return 0;

init_sdmx_feed_failed_free_data_buff:
	mpq_sdmx_free_data_buf(mpq_feed);
init_sdmx_feed_failed_free_sdmx:
	mpq_sdmx_close_session(mpq_demux);
init_sdmx_feed_failed:
	return ret;
}

int mpq_dmx_init_mpq_feed(struct dvb_demux_feed *feed)
{
	int ret = 0;
	struct mpq_demux *mpq_demux = feed->demux->priv;
	struct mpq_feed *mpq_feed = feed->priv;

	mutex_lock(&mpq_demux->mutex);

	mpq_feed->sdmx_buf_handle = NULL;
	mpq_feed->metadata_buf_handle = NULL;
	mpq_feed->sdmx_filter_handle = SDMX_INVALID_FILTER_HANDLE;

	if (dvb_dmx_is_video_feed(feed)) {
		ret = mpq_dmx_init_video_feed(mpq_feed);
		if (ret) {
			MPQ_DVB_ERR_PRINT(
				"%s: mpq_dmx_init_video_feed failed, ret=%d\n",
				__func__, ret);
			goto init_mpq_feed_end;
		}
	}

	/*
	 * sdmx is not relevant for recording filters, which always use
	 * regular filters (non-sdmx)
	 */
	if (!mpq_sdmx_is_loaded() || !feed->secure_mode.is_secured ||
		dvb_dmx_is_rec_feed(feed)) {
		if (!mpq_sdmx_is_loaded())
			mpq_demux->sdmx_session_handle =
				SDMX_INVALID_SESSION_HANDLE;
		goto init_mpq_feed_end;
	}

	 /* Initialization of secure demux filters (PES/PCR/Video/Section) */
	ret = mpq_sdmx_init_feed(mpq_demux, mpq_feed);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: mpq_sdmx_init_feed failed, ret=%d\n",
			__func__, ret);
		if (dvb_dmx_is_video_feed(feed))
			mpq_dmx_terminate_video_feed(mpq_feed);
	}

init_mpq_feed_end:
	if (!ret) {
		mpq_demux->num_active_feeds++;
		mpq_feed->session_id++;
	}
	mutex_unlock(&mpq_demux->mutex);
	return ret;
}
EXPORT_SYMBOL(mpq_dmx_init_mpq_feed);

/**
 * Note: Called only when filter is in "GO" state - after feed has been started.
 */
int mpq_dmx_set_cipher_ops(struct dvb_demux_feed *feed,
		struct dmx_cipher_operations *cipher_ops)
{
	struct mpq_feed *mpq_feed;
	struct mpq_demux *mpq_demux;
	int ret = 0;

	if (!feed || !feed->priv || !cipher_ops) {
		MPQ_DVB_ERR_PRINT(
			"%s: invalid parameters\n",
			__func__);
		return -EINVAL;
	}

	MPQ_DVB_DBG_PRINT("%s(%d, %d, %d)\n",
		__func__, cipher_ops->pid,
		cipher_ops->operations_count,
		cipher_ops->operations[0].key_ladder_id);

	if ((cipher_ops->operations_count > 1) ||
		(cipher_ops->operations_count &&
		 cipher_ops->operations[0].encrypt)) {
		MPQ_DVB_ERR_PRINT(
			"%s: Invalid cipher operations, count=%d, encrypt=%d\n",
			__func__, cipher_ops->operations_count,
			cipher_ops->operations[0].encrypt);
		return -EINVAL;
	}

	if (!feed->secure_mode.is_secured) {
		/*
		 * Filter is not configured as secured, setting cipher
		 * operations is not allowed.
		 */
		MPQ_DVB_ERR_PRINT(
			"%s: Cannot set cipher operations to non-secure filter\n",
			__func__);
		return -EPERM;
	}

	mpq_feed = feed->priv;
	mpq_demux = mpq_feed->mpq_demux;

	mutex_lock(&mpq_demux->mutex);

	/*
	 * Feed is running in secure mode, this secure mode request is to
	 * update the key ladder id
	 */
	if ((mpq_demux->sdmx_session_handle != SDMX_INVALID_SESSION_HANDLE) &&
		cipher_ops->operations_count) {
		ret = sdmx_set_kl_ind(mpq_demux->sdmx_session_handle,
			cipher_ops->pid,
			cipher_ops->operations[0].key_ladder_id);
		if (ret) {
			MPQ_DVB_ERR_PRINT(
				"%s: FAILED to set key ladder, ret=%d\n",
				__func__, ret);
			ret = -ENODEV;
		}
	}

	mutex_unlock(&mpq_demux->mutex);

	return ret;
}
EXPORT_SYMBOL(mpq_dmx_set_cipher_ops);

static void mpq_sdmx_prepare_filter_status(struct mpq_demux *mpq_demux,
	struct sdmx_filter_status *filter_sts,
	struct mpq_feed *mpq_feed)
{
	struct dvb_demux_feed *feed = mpq_feed->dvb_demux_feed;
	struct mpq_video_feed_info *feed_data;
	struct mpq_streambuffer *sbuff;

	filter_sts->filter_handle = mpq_feed->sdmx_filter_handle;
	filter_sts->metadata_fill_count =
		dvb_ringbuffer_avail(&mpq_feed->metadata_buf);
	filter_sts->metadata_write_offset = mpq_feed->metadata_buf.pwrite;
	filter_sts->error_indicators = 0;
	filter_sts->status_indicators = 0;

	MPQ_DVB_DBG_PRINT(
		"%s: Filter meta-data buffer status: fill count = %d, write_offset = %d\n",
		__func__, filter_sts->metadata_fill_count,
		filter_sts->metadata_write_offset);

	if (!dvb_dmx_is_video_feed(feed)) {
		struct dvb_ringbuffer *buffer;

		if (dvb_dmx_is_sec_feed(feed) ||
			dvb_dmx_is_pcr_feed(feed)) {
			buffer = (struct dvb_ringbuffer *)
				&mpq_feed->sdmx_buf;
		} else {
			buffer = (struct dvb_ringbuffer *)
				feed->feed.ts.buffer.ringbuff;
		}

		filter_sts->data_fill_count = dvb_ringbuffer_avail(buffer);
		filter_sts->data_write_offset = buffer->pwrite;

		MPQ_DVB_DBG_PRINT(
			"%s: Filter buffers status: fill count = %d, write_offset = %d\n",
			__func__, filter_sts->data_fill_count,
			filter_sts->data_write_offset);

		return;
	}

	/* Video feed - decoder buffers */
	feed_data = &mpq_feed->video_info;

	spin_lock(&mpq_feed->video_info.video_buffer_lock);
	sbuff = feed_data->video_buffer;
	if (sbuff == NULL) {
		MPQ_DVB_DBG_PRINT(
			"%s: video_buffer released\n",
			__func__);
		spin_unlock(&feed_data->video_buffer_lock);
		return;
	}

	if (feed_data->buffer_desc.decoder_buffers_num > 1) {
		/* linear mode */
		filter_sts->data_fill_count = sbuff->pending_buffers_count;
		filter_sts->data_write_offset =
			sbuff->raw_data.pwrite /
			sizeof(struct mpq_streambuffer_buffer_desc);
	} else {
		/* ring buffer mode */
		filter_sts->data_fill_count =
			mpq_streambuffer_data_avail(sbuff);
		mpq_streambuffer_get_data_rw_offset(sbuff, NULL,
			&filter_sts->data_write_offset);

	}

	spin_unlock(&mpq_feed->video_info.video_buffer_lock);

	MPQ_DVB_DBG_PRINT(
		"%s: Decoder buffers filter status: fill count = %d, write_offset = %d\n",
		__func__, filter_sts->data_fill_count,
		filter_sts->data_write_offset);
}


static int mpq_sdmx_section_filtering(struct mpq_feed *mpq_feed,
	struct dvb_demux_filter *f,
	struct sdmx_metadata_header *header)
{
	struct dvb_demux_feed *feed = mpq_feed->dvb_demux_feed;
	int ret;
	u8 neq = 0;
	u8 xor;
	u8 tmp;
	int i;

	if (!mutex_is_locked(&mpq_feed->mpq_demux->mutex)) {
		MPQ_DVB_ERR_PRINT(
				"%s: Mutex should have been locked\n",
				__func__);
		return -EINVAL;
	}

	for (i = 0; i < DVB_DEMUX_MASK_MAX; i++) {
		tmp = DVB_RINGBUFFER_PEEK(&mpq_feed->sdmx_buf, i);
		xor = f->filter.filter_value[i] ^ tmp;

		if (f->maskandmode[i] & xor)
			return 0;

		neq |= f->maskandnotmode[i] & xor;
	}

	if (f->doneq && !neq)
		return 0;

	if (feed->demux->playback_mode == DMX_PB_MODE_PULL) {
		mutex_unlock(&mpq_feed->mpq_demux->mutex);

		ret = feed->demux->buffer_ctrl.sec(&f->filter,
					header->payload_length);

		mutex_lock(&mpq_feed->mpq_demux->mutex);

		if (ret) {
			MPQ_DVB_DBG_PRINT(
				"%s: buffer_ctrl.sec aborted\n",
				__func__);
			return ret;
		}

		if (mpq_feed->sdmx_filter_handle ==
			SDMX_INVALID_FILTER_HANDLE) {
			MPQ_DVB_DBG_PRINT("%s: filter was stopped\n",
				__func__);
			return -ENODEV;
		}
	}

	if (mpq_feed->sdmx_buf.pread + header->payload_length <
		mpq_feed->sdmx_buf.size) {
		feed->cb.sec(&mpq_feed->sdmx_buf.data[mpq_feed->sdmx_buf.pread],
			header->payload_length,
			NULL, 0, &f->filter, DMX_OK);
	} else {
		int split = mpq_feed->sdmx_buf.size - mpq_feed->sdmx_buf.pread;
		feed->cb.sec(&mpq_feed->sdmx_buf.data[mpq_feed->sdmx_buf.pread],
			split,
			&mpq_feed->sdmx_buf.data[0],
			header->payload_length - split,
			&f->filter, DMX_OK);
	}

	return 0;
}

static int mpq_sdmx_check_ts_stall(struct mpq_demux *mpq_demux,
	struct mpq_feed *mpq_feed,
	struct sdmx_filter_status *sts,
	size_t req,
	int events_only)
{
	struct dvb_demux_feed *feed = mpq_feed->dvb_demux_feed;
	int ret;

	if (!mutex_is_locked(&mpq_feed->mpq_demux->mutex)) {
		MPQ_DVB_ERR_PRINT(
				"%s: Mutex should have been locked\n",
				__func__);
		return -EINVAL;
	}

	/*
	 * For PULL mode need to verify there is enough space for the dmxdev
	 * event. Also, if data buffer is full we want to stall until some
	 * data is removed from it to prevent calling the sdmx when it cannot
	 * output data to the still full buffer.
	 */
	if (mpq_demux->demux.playback_mode == DMX_PB_MODE_PULL) {
		MPQ_DVB_DBG_PRINT("%s: Stalling for events and %d bytes\n",
			__func__, req);

		mutex_unlock(&mpq_demux->mutex);

		ret = mpq_demux->demux.buffer_ctrl.ts(&feed->feed.ts, req);
		MPQ_DVB_DBG_PRINT("%s: stall result = %d\n",
			__func__, ret);

		mutex_lock(&mpq_demux->mutex);

		if (mpq_feed->sdmx_filter_handle ==
			SDMX_INVALID_FILTER_HANDLE) {
			MPQ_DVB_DBG_PRINT("%s: filter was stopped\n",
					__func__);
			return -ENODEV;
		}

		return ret;
	}

	return 0;
}

/* Handle filter results for filters with no extra meta-data */
static void mpq_sdmx_pes_filter_results(struct mpq_demux *mpq_demux,
	struct mpq_feed *mpq_feed,
	struct sdmx_filter_status *sts)
{
	int ret;
	struct sdmx_metadata_header header;
	struct sdmx_pes_counters counters;
	struct dmx_data_ready data_event;
	struct dmx_data_ready pes_event;
	struct dvb_demux_feed *feed = mpq_feed->dvb_demux_feed;
	struct dvb_ringbuffer *buf = (struct dvb_ringbuffer *)
		feed->feed.ts.buffer.ringbuff;
	ssize_t bytes_avail;

	if ((!sts->metadata_fill_count) && (!sts->data_fill_count))
		goto pes_filter_check_overflow;

	MPQ_DVB_DBG_PRINT(
		"%s: Meta: fill=%u, write=%u. Data: fill=%u, write=%u\n",
		__func__, sts->metadata_fill_count, sts->metadata_write_offset,
		sts->data_fill_count, sts->data_write_offset);

	mpq_feed->metadata_buf.pwrite = sts->metadata_write_offset;

	if ((0 == sts->metadata_fill_count) &&
		(sts->error_indicators & SDMX_FILTER_ERR_D_BUF_FULL)) {
		ssize_t free = dvb_ringbuffer_free(buf);
		ret = 0;
		if ((free + SZ_2K) < MAX_PES_LENGTH)
			ret = mpq_sdmx_check_ts_stall(mpq_demux, mpq_feed, sts,
				free + SZ_2K, 0);
		else
			MPQ_DVB_ERR_PRINT(
				"%s: Cannot stall when free space bigger than max PES size\n",
				__func__);
		if (ret) {
			MPQ_DVB_DBG_PRINT(
				"%s: mpq_sdmx_check_ts_stall aborted\n",
				__func__);
			return;
		}
	}

	while (sts->metadata_fill_count) {
		bytes_avail = dvb_ringbuffer_avail(&mpq_feed->metadata_buf);
		if (bytes_avail < (sizeof(header) + sizeof(counters))) {
			MPQ_DVB_ERR_PRINT(
				"%s: metadata_fill_count is %d less than required %d bytes\n",
				__func__,
				sts->metadata_fill_count,
				sizeof(header) + sizeof(counters));

			/* clean-up remaining bytes to try to recover */
			DVB_RINGBUFFER_SKIP(&mpq_feed->metadata_buf,
				bytes_avail);
			sts->metadata_fill_count = 0;
			break;
		}

		dvb_ringbuffer_read(&mpq_feed->metadata_buf, (u8 *)&header,
			sizeof(header));
		MPQ_DVB_DBG_PRINT(
			"%s: metadata header: start=%u, length=%u\n",
			__func__, header.payload_start, header.payload_length);
		sts->metadata_fill_count -= sizeof(header);

		dvb_ringbuffer_read(&mpq_feed->metadata_buf, (u8 *)&counters,
			sizeof(counters));
		sts->metadata_fill_count -= sizeof(counters);

		/* Notify new data in buffer */
		data_event.status = DMX_OK;
		data_event.data_length = header.payload_length;
		ret = mpq_sdmx_check_ts_stall(mpq_demux, mpq_feed, sts,
			data_event.data_length, 0);
		if (ret) {
			MPQ_DVB_DBG_PRINT(
				"%s: mpq_sdmx_check_ts_stall aborted\n",
				__func__);
			return;
		}

		feed->data_ready_cb.ts(&feed->feed.ts, &data_event);

		/* Notify new complete PES */
		pes_event.status = DMX_OK_PES_END;
		pes_event.pes_end.actual_length = header.payload_length;
		pes_event.pes_end.start_gap = 0;
		pes_event.data_length = 0;

		/* Parse error indicators */
		if (sts->error_indicators & SDMX_FILTER_ERR_INVALID_PES_LEN)
			pes_event.pes_end.pes_length_mismatch = 1;
		else
			pes_event.pes_end.pes_length_mismatch = 0;

		pes_event.pes_end.disc_indicator_set = 0;

		pes_event.pes_end.stc = 0;
		pes_event.pes_end.tei_counter = counters.transport_err_count;
		pes_event.pes_end.cont_err_counter =
			counters.continuity_err_count;
		pes_event.pes_end.ts_packets_num =
			counters.pes_ts_count;

		ret = mpq_sdmx_check_ts_stall(mpq_demux, mpq_feed, sts, 0, 1);
		if (ret) {
			MPQ_DVB_DBG_PRINT(
				"%s: mpq_sdmx_check_ts_stall aborted\n",
				__func__);
			return;
		}
		feed->data_ready_cb.ts(&feed->feed.ts, &pes_event);
	}

pes_filter_check_overflow:
	if ((mpq_demux->demux.playback_mode == DMX_PB_MODE_PUSH) &&
		(sts->error_indicators & SDMX_FILTER_ERR_D_BUF_FULL)) {
		MPQ_DVB_ERR_PRINT("%s: DMX_OVERRUN_ERROR\n", __func__);
		data_event.status = DMX_OVERRUN_ERROR;
		data_event.data_length = 0;
		feed->data_ready_cb.ts(&feed->feed.ts, &data_event);
	}

	if (sts->status_indicators & SDMX_FILTER_STATUS_EOS) {
		data_event.data_length = 0;
		data_event.status = DMX_OK_EOS;
		feed->data_ready_cb.ts(&feed->feed.ts, &data_event);
	}
}

static void mpq_sdmx_section_filter_results(struct mpq_demux *mpq_demux,
	struct mpq_feed *mpq_feed,
	struct sdmx_filter_status *sts)
{
	struct sdmx_metadata_header header;
	struct dmx_data_ready event;
	struct dvb_demux_feed *feed = mpq_feed->dvb_demux_feed;
	struct dvb_demux_filter *f;
	struct dmx_section_feed *sec = &feed->feed.sec;
	ssize_t bytes_avail;

	/* Parse error indicators */
	if (sts->error_indicators & SDMX_FILTER_ERR_SEC_VERIF_CRC32_FAIL) {
		MPQ_DVB_DBG_PRINT("%s: Notify CRC err event\n", __func__);
		event.status = DMX_CRC_ERROR;
		event.data_length = 0;
		dvb_dmx_notify_section_event(feed, &event, 1);
	}

	if (sts->error_indicators & SDMX_FILTER_ERR_D_BUF_FULL)
		MPQ_DVB_ERR_PRINT("%s: internal section buffer overflowed!\n",
			__func__);

	if ((!sts->metadata_fill_count) && (!sts->data_fill_count))
		goto section_filter_check_eos;

	mpq_feed->metadata_buf.pwrite = sts->metadata_write_offset;
	mpq_feed->sdmx_buf.pwrite = sts->data_write_offset;

	while (sts->metadata_fill_count) {
		bytes_avail = dvb_ringbuffer_avail(&mpq_feed->metadata_buf);
		if (bytes_avail < sizeof(header)) {
			MPQ_DVB_ERR_PRINT(
				"%s: metadata_fill_count is %d less than required %d bytes\n",
				__func__,
				sts->metadata_fill_count,
				sizeof(header));

			/* clean-up remaining bytes to try to recover */
			DVB_RINGBUFFER_SKIP(&mpq_feed->metadata_buf,
				bytes_avail);
			sts->metadata_fill_count = 0;
			break;
		}

		dvb_ringbuffer_read(&mpq_feed->metadata_buf, (u8 *) &header,
			sizeof(header));
		sts->metadata_fill_count -= sizeof(header);
		MPQ_DVB_DBG_PRINT(
			"%s: metadata header: start=%u, length=%u\n",
			__func__, header.payload_start, header.payload_length);

		f = feed->filter;
		do {
			if (mpq_sdmx_section_filtering(mpq_feed, f, &header))
				return;
		} while ((f = f->next) && sec->is_filtering);

		DVB_RINGBUFFER_SKIP(&mpq_feed->sdmx_buf, header.payload_length);
	}

section_filter_check_eos:
	if (sts->status_indicators & SDMX_FILTER_STATUS_EOS) {
		event.data_length = 0;
		event.status = DMX_OK_EOS;
		dvb_dmx_notify_section_event(feed, &event, 1);
	}

}

static void mpq_sdmx_decoder_filter_results(struct mpq_demux *mpq_demux,
	struct mpq_feed *mpq_feed,
	struct sdmx_filter_status *sts)
{
	struct sdmx_metadata_header header;
	struct sdmx_pes_counters counters;
	int pes_header_offset;
	struct ts_packet_header *ts_header;
	struct ts_adaptation_field *ts_adapt;
	struct pes_packet_header *pes_header;
	u8 metadata_buf[MAX_SDMX_METADATA_LENGTH];
	struct mpq_streambuffer *sbuf;
	int ret;
	struct dmx_data_ready data_event;
	struct dmx_data_ready data;
	struct dvb_demux_feed *feed = mpq_feed->dvb_demux_feed;
	ssize_t bytes_avail;

	if ((!sts->metadata_fill_count) && (!sts->data_fill_count))
		goto decoder_filter_check_flags;

	/* Update meta data buffer write pointer */
	mpq_feed->metadata_buf.pwrite = sts->metadata_write_offset;

	if ((mpq_demux->demux.playback_mode == DMX_PB_MODE_PULL) &&
		(sts->error_indicators & SDMX_FILTER_ERR_D_LIN_BUFS_FULL)) {
		MPQ_DVB_DBG_PRINT("%s: Decoder stall...\n", __func__);

		ret = mpq_dmx_decoder_fullness_check(
			mpq_feed->dvb_demux_feed, 0, 0);
		if (ret) {
			/* we reach here if demuxing was aborted */
			MPQ_DVB_DBG_PRINT(
				"%s: mpq_dmx_decoder_fullness_check aborted\n",
				__func__);
			return;
		}
	}

	while (sts->metadata_fill_count) {
		struct mpq_streambuffer_packet_header packet;
		struct mpq_adapter_video_meta_data meta_data;

		bytes_avail = dvb_ringbuffer_avail(&mpq_feed->metadata_buf);
		if (bytes_avail < (sizeof(header) + sizeof(counters))) {
			MPQ_DVB_ERR_PRINT(
				"%s: metadata_fill_count is %d less than required %d bytes\n",
				__func__,
				sts->metadata_fill_count,
				sizeof(header) + sizeof(counters));

			/* clean-up remaining bytes to try to recover */
			DVB_RINGBUFFER_SKIP(&mpq_feed->metadata_buf,
				bytes_avail);
			sts->metadata_fill_count = 0;
			break;
		}

		/* Read metadata header */
		dvb_ringbuffer_read(&mpq_feed->metadata_buf, (u8 *)&header,
			sizeof(header));
		sts->metadata_fill_count -= sizeof(header);
		MPQ_DVB_DBG_PRINT(
			"%s: metadata header: start=%u, length=%u, metadata=%u\n",
			__func__, header.payload_start, header.payload_length,
			header.metadata_length);

		/* Read metadata - PES counters */
		dvb_ringbuffer_read(&mpq_feed->metadata_buf, (u8 *)&counters,
					sizeof(counters));
		sts->metadata_fill_count -= sizeof(counters);

		/* Read metadata - TS & PES headers */
		bytes_avail = dvb_ringbuffer_avail(&mpq_feed->metadata_buf);
		if ((header.metadata_length < MAX_SDMX_METADATA_LENGTH) &&
			(header.metadata_length >= sizeof(counters)) &&
			(bytes_avail >=
			 (header.metadata_length - sizeof(counters)))) {
			dvb_ringbuffer_read(&mpq_feed->metadata_buf,
				metadata_buf,
				header.metadata_length - sizeof(counters));
		} else {
			MPQ_DVB_ERR_PRINT(
				"%s: meta-data size %d larger than available meta-data %d or max allowed %d\n",
				__func__, header.metadata_length,
				bytes_avail,
				MAX_SDMX_METADATA_LENGTH);

			/* clean-up remaining bytes to try to recover */
			DVB_RINGBUFFER_SKIP(&mpq_feed->metadata_buf,
				bytes_avail);
			sts->metadata_fill_count = 0;
			break;
		}

		sts->metadata_fill_count -=
			(header.metadata_length - sizeof(counters));

		ts_header = (struct ts_packet_header *)&metadata_buf[0];
		if (1 == ts_header->adaptation_field_control) {
			ts_adapt = NULL;
			pes_header_offset = sizeof(*ts_header);
		} else {
			ts_adapt = (struct ts_adaptation_field *)
				&metadata_buf[sizeof(*ts_header)];
			pes_header_offset = sizeof(*ts_header) + 1 +
				ts_adapt->adaptation_field_length;
		}
		pes_header = (struct pes_packet_header *)
			&metadata_buf[pes_header_offset];
		meta_data.packet_type = DMX_PES_PACKET;
		/* TODO - set to real STC when SDMX supports it */
		meta_data.info.pes.stc = 0;

		if (pes_header->pts_dts_flag & 0x2) {
			meta_data.info.pes.pts_dts_info.pts_exist = 1;
			meta_data.info.pes.pts_dts_info.pts =
				((u64)pes_header->pts_1 << 30) |
				((u64)pes_header->pts_2 << 22) |
				((u64)pes_header->pts_3 << 15) |
				((u64)pes_header->pts_4 << 7) |
				(u64)pes_header->pts_5;
		} else {
			meta_data.info.pes.pts_dts_info.pts_exist = 0;
		}

		if (pes_header->pts_dts_flag & 0x1) {
			meta_data.info.pes.pts_dts_info.dts_exist = 1;
			meta_data.info.pes.pts_dts_info.dts =
				((u64)pes_header->dts_1 << 30) |
				((u64)pes_header->dts_2 << 22) |
				((u64)pes_header->dts_3 << 15) |
				((u64)pes_header->dts_4 << 7) |
				(u64)pes_header->dts_5;
		} else {
			meta_data.info.pes.pts_dts_info.dts_exist = 0;
		}

		spin_lock(&mpq_feed->video_info.video_buffer_lock);

		mpq_feed->video_info.tei_errs =
			counters.transport_err_count;
		mpq_feed->video_info.continuity_errs =
			counters.continuity_err_count;
		mpq_feed->video_info.ts_packets_num =
			counters.pes_ts_count;
		mpq_feed->video_info.ts_dropped_bytes =
			counters.drop_count *
			mpq_demux->demux.ts_packet_size;

		sbuf = mpq_feed->video_info.video_buffer;
		if (sbuf == NULL) {
			MPQ_DVB_DBG_PRINT(
				"%s: video_buffer released\n",
				__func__);
			spin_unlock(&mpq_feed->video_info.video_buffer_lock);
			return;
		}

		if (!header.payload_length) {
			MPQ_DVB_DBG_PRINT(
				"%s: warnning - video frame with 0 length, dropping\n",
				__func__);
			spin_unlock(&mpq_feed->video_info.video_buffer_lock);
			continue;
		}

		packet.raw_data_len = header.payload_length;
		packet.user_data_len = sizeof(meta_data);
		mpq_streambuffer_get_buffer_handle(sbuf, 0,
			&packet.raw_data_handle);
		mpq_streambuffer_get_data_rw_offset(sbuf,
			NULL, &packet.raw_data_offset);
		ret = mpq_streambuffer_data_write_deposit(sbuf,
			header.payload_length);
		if (ret) {
			MPQ_DVB_ERR_PRINT(
				"%s: mpq_streambuffer_data_write_deposit failed. ret=%d\n",
				__func__, ret);
		}
		mpq_dmx_update_decoder_stat(mpq_demux);
		if (mpq_streambuffer_pkt_write(sbuf,
				&packet,
				(u8 *)&meta_data) < 0)
			MPQ_DVB_ERR_PRINT(
				"%s: Couldn't write packet. Should never happen\n",
				__func__);

		mpq_dmx_prepare_es_event_data(
			&packet, &meta_data, &mpq_feed->video_info,
			sbuf, &data);
		MPQ_DVB_DBG_PRINT("%s: Notify ES Event\n", __func__);
		feed->data_ready_cb.ts(&feed->feed.ts, &data);

		spin_unlock(&mpq_feed->video_info.video_buffer_lock);
	}

decoder_filter_check_flags:
	if ((mpq_demux->demux.playback_mode == DMX_PB_MODE_PUSH) &&
		(sts->error_indicators & SDMX_FILTER_ERR_D_LIN_BUFS_FULL)) {
		MPQ_DVB_ERR_PRINT("%s: DMX_OVERRUN_ERROR\n", __func__);
		data_event.status = DMX_OVERRUN_ERROR;
		data_event.data_length = 0;
		mpq_feed->dvb_demux_feed->data_ready_cb.ts(
			&mpq_feed->dvb_demux_feed->feed.ts, &data_event);
	}

	if (sts->status_indicators & SDMX_FILTER_STATUS_EOS) {
		/* Notify decoder via the stream buffer */
		ret = mpq_dmx_decoder_eos_cmd(mpq_feed);
		if (ret)
			MPQ_DVB_ERR_PRINT(
				"%s: Failed to notify decoder on EOS, ret=%d\n",
				__func__, ret);

		/* Notify user filter */
		data_event.data_length = 0;
		data_event.status = DMX_OK_EOS;
		mpq_feed->dvb_demux_feed->data_ready_cb.ts(
			&mpq_feed->dvb_demux_feed->feed.ts, &data_event);
	}
}

static void mpq_sdmx_pcr_filter_results(struct mpq_demux *mpq_demux,
	struct mpq_feed *mpq_feed,
	struct sdmx_filter_status *sts)
{
	int ret;
	struct sdmx_metadata_header header;
	struct dmx_data_ready data;
	struct dvb_ringbuffer *rbuff = &mpq_feed->sdmx_buf;
	struct dvb_demux_feed *feed = mpq_feed->dvb_demux_feed;
	u8 buf[TS_PACKET_HEADER_LENGTH + MAX_TSP_ADAPTATION_LENGTH +
	       TIMESTAMP_LEN];
	size_t stc_len = 0;
	ssize_t bytes_avail;

	if (sts->error_indicators & SDMX_FILTER_ERR_D_BUF_FULL)
		MPQ_DVB_ERR_PRINT("%s: internal PCR buffer overflowed!\n",
			__func__);

	if ((!sts->metadata_fill_count) && (!sts->data_fill_count))
		goto pcr_filter_check_eos;

	if (DMX_TSP_FORMAT_192_TAIL == mpq_demux->demux.tsp_format)
		stc_len = 4;

	mpq_feed->metadata_buf.pwrite = sts->metadata_write_offset;
	rbuff->pwrite = sts->data_write_offset;

	while (sts->metadata_fill_count) {
		bytes_avail = dvb_ringbuffer_avail(&mpq_feed->metadata_buf);
		if (bytes_avail < sizeof(header)) {
			MPQ_DVB_ERR_PRINT(
				"%s: metadata_fill_count is %d less than required %d bytes\n",
				__func__,
				sts->metadata_fill_count,
				sizeof(header));

			/* clean-up remaining bytes to try to recover */
			DVB_RINGBUFFER_SKIP(&mpq_feed->metadata_buf,
				bytes_avail);
			sts->metadata_fill_count = 0;
			break;
		}

		dvb_ringbuffer_read(&mpq_feed->metadata_buf, (u8 *) &header,
			sizeof(header));
		MPQ_DVB_DBG_PRINT(
			"%s: metadata header: start=%u, length=%u\n",
			__func__, header.payload_start, header.payload_length);
		sts->metadata_fill_count -= sizeof(header);

		dvb_ringbuffer_read(rbuff, buf, header.payload_length);

		if (mpq_dmx_extract_pcr_and_dci(buf, &data.pcr.pcr,
			&data.pcr.disc_indicator_set)) {

			if (stc_len) {
				data.pcr.stc =
					buf[header.payload_length-2] << 16;
				data.pcr.stc +=
					buf[header.payload_length-3] << 8;
				data.pcr.stc += buf[header.payload_length-4];
				 /* convert from 105.47 KHZ to 27MHz */
				data.pcr.stc *= 256;
			} else {
				data.pcr.stc = 0;
			}

			data.data_length = 0;
			data.status = DMX_OK_PCR;
			ret = mpq_sdmx_check_ts_stall(
				mpq_demux, mpq_feed, sts, 0, 1);
			if (ret) {
				MPQ_DVB_DBG_PRINT(
					"%s: mpq_sdmx_check_ts_stall aborted\n",
					__func__);
				return;
			}
			feed->data_ready_cb.ts(&feed->feed.ts, &data);
		}
	}

pcr_filter_check_eos:
	if (sts->status_indicators & SDMX_FILTER_STATUS_EOS) {
		data.data_length = 0;
		data.status = DMX_OK_EOS;
		feed->data_ready_cb.ts(&feed->feed.ts, &data);
	}
}

static void mpq_sdmx_raw_filter_results(struct mpq_demux *mpq_demux,
	struct mpq_feed *mpq_feed,
	struct sdmx_filter_status *sts)
{
	int ret;
	ssize_t new_data;
	struct dmx_data_ready data_event;
	struct dvb_demux_feed *feed = mpq_feed->dvb_demux_feed;
	struct dvb_ringbuffer *buf = (struct dvb_ringbuffer *)
					feed->feed.ts.buffer.ringbuff;

	if ((!sts->metadata_fill_count) && (!sts->data_fill_count))
		goto raw_filter_check_flags;

	new_data = sts->data_write_offset -
		buf->pwrite;
	if (new_data < 0)
		new_data += buf->size;

	ret = mpq_sdmx_check_ts_stall(mpq_demux, mpq_feed, sts,
		new_data + feed->demux->ts_packet_size, 0);
	if (ret) {
		MPQ_DVB_DBG_PRINT(
			"%s: mpq_sdmx_check_ts_stall aborted\n",
			__func__);
		return;
	}

	data_event.status = DMX_OK;
	data_event.data_length = new_data;
	feed->data_ready_cb.ts(&feed->feed.ts, &data_event);
	MPQ_DVB_DBG_PRINT("%s: Callback DMX_OK, size=%d\n",
		__func__, data_event.data_length);

raw_filter_check_flags:
	if ((mpq_demux->demux.playback_mode == DMX_PB_MODE_PUSH) &&
		(sts->error_indicators & SDMX_FILTER_ERR_D_BUF_FULL)) {
		MPQ_DVB_DBG_PRINT("%s: DMX_OVERRUN_ERROR\n", __func__);
		data_event.status = DMX_OVERRUN_ERROR;
		data_event.data_length = 0;
		feed->data_ready_cb.ts(&feed->feed.ts, &data_event);
	}

	if (sts->status_indicators & SDMX_FILTER_STATUS_EOS) {
		data_event.data_length = 0;
		data_event.status = DMX_OK_EOS;
		feed->data_ready_cb.ts(&feed->feed.ts, &data_event);
	}

}

static void mpq_sdmx_process_results(struct mpq_demux *mpq_demux)
{
	int i;
	int sdmx_filters;
	struct sdmx_filter_status *sts;
	struct mpq_feed *mpq_feed;
	u8 mpq_feed_idx;

	sdmx_filters = mpq_demux->sdmx_filter_count;
	for (i = 0; i < sdmx_filters; i++) {
		sts = &mpq_demux->sdmx_filters_state.status[i];
		MPQ_DVB_DBG_PRINT(
			"%s: Filter: handle=%d, status=0x%x, errors=0x%x\n",
			__func__, sts->filter_handle, sts->status_indicators,
			sts->error_indicators);
		MPQ_DVB_DBG_PRINT("%s: Metadata fill count=%d (write=%d)\n",
			__func__, sts->metadata_fill_count,
			sts->metadata_write_offset);
		MPQ_DVB_DBG_PRINT("%s: Data fill count=%d (write=%d)\n",
			__func__, sts->data_fill_count, sts->data_write_offset);

		mpq_feed_idx = mpq_demux->sdmx_filters_state.mpq_feed_idx[i];
		mpq_feed = &mpq_demux->feeds[mpq_feed_idx];
		if ((mpq_feed->dvb_demux_feed->state != DMX_STATE_GO) ||
			(sts->filter_handle != mpq_feed->sdmx_filter_handle) ||
			mpq_feed->secondary_feed ||
			(mpq_demux->sdmx_filters_state.session_id[i] !=
			 mpq_feed->session_id))
			continue;

		if (sts->error_indicators & SDMX_FILTER_ERR_MD_BUF_FULL)
			MPQ_DVB_ERR_PRINT(
				"%s: meta-data buff for pid %d overflowed!\n",
				__func__, mpq_feed->dvb_demux_feed->pid);

		switch (mpq_feed->filter_type) {
		case SDMX_PCR_FILTER:
			mpq_sdmx_pcr_filter_results(mpq_demux, mpq_feed, sts);
			break;
		case SDMX_PES_FILTER:
			mpq_sdmx_pes_filter_results(mpq_demux, mpq_feed,
				sts);
			break;
		case SDMX_SEPARATED_PES_FILTER:
			mpq_sdmx_decoder_filter_results(mpq_demux, mpq_feed,
				sts);
			break;
		case SDMX_SECTION_FILTER:
			mpq_sdmx_section_filter_results(mpq_demux, mpq_feed,
				sts);
			break;
		case SDMX_RAW_FILTER:
			mpq_sdmx_raw_filter_results(mpq_demux, mpq_feed, sts);
			break;
		default:
			break;
		}
	}
}

static int mpq_sdmx_process_buffer(struct mpq_demux *mpq_demux,
	struct sdmx_buff_descr *input,
	u32 fill_count,
	u32 read_offset)
{
	struct sdmx_filter_status *sts;
	struct mpq_feed *mpq_feed;
	u8 flags = 0;
	u32 errors;
	u32 status;
	u32 prev_read_offset;
	u32 prev_fill_count;
	enum sdmx_status sdmx_res;
	int i;
	int filter_index = 0;
	int bytes_read;
	struct timespec process_start_time;
	struct timespec process_end_time;

	mutex_lock(&mpq_demux->mutex);

	/*
	 * All active filters may get totally closed and therefore
	 * sdmx session may get terminated, in such case nothing to process
	 */
	if (mpq_demux->sdmx_session_handle == SDMX_INVALID_SESSION_HANDLE) {
		MPQ_DVB_DBG_PRINT(
			"%s: sdmx filters aborted, filter-count %d, session %d\n",
			__func__, mpq_demux->sdmx_filter_count,
			mpq_demux->sdmx_session_handle);
		mutex_unlock(&mpq_demux->mutex);
		return 0;
	}

	/* Set input flags */
	if (mpq_demux->sdmx_eos)
		flags |= SDMX_INPUT_FLAG_EOS;
	if (mpq_sdmx_debug)
		flags |= SDMX_INPUT_FLAG_DBG_ENABLE;

	/* Build up to date filter status array */
	for (i = 0; i < MPQ_MAX_DMX_FILES; i++) {
		mpq_feed = &mpq_demux->feeds[i];
		if ((mpq_feed->sdmx_filter_handle != SDMX_INVALID_FILTER_HANDLE)
			&& (mpq_feed->dvb_demux_feed->state == DMX_STATE_GO)
			&& (!mpq_feed->secondary_feed)) {
			sts = mpq_demux->sdmx_filters_state.status +
				filter_index;
			mpq_sdmx_prepare_filter_status(mpq_demux, sts,
				mpq_feed);
			mpq_demux->sdmx_filters_state.mpq_feed_idx[filter_index]
				 = i;
			mpq_demux->sdmx_filters_state.session_id[filter_index] =
				mpq_feed->session_id;
			filter_index++;
		}
	}

	/* Sanity check */
	if (filter_index != mpq_demux->sdmx_filter_count) {
		mutex_unlock(&mpq_demux->mutex);
		MPQ_DVB_ERR_PRINT(
			"%s: Updated %d SDMX filters status but should be %d\n",
			__func__, filter_index, mpq_demux->sdmx_filter_count);
		return -ERESTART;
	}

	MPQ_DVB_DBG_PRINT(
		"%s: Before SDMX_process: input read_offset=%u, fill count=%u\n",
		__func__, read_offset, fill_count);

	process_start_time = current_kernel_time();

	prev_read_offset = read_offset;
	prev_fill_count = fill_count;
	sdmx_res = sdmx_process(mpq_demux->sdmx_session_handle, flags, input,
		&fill_count, &read_offset, &errors, &status,
		mpq_demux->sdmx_filter_count,
		mpq_demux->sdmx_filters_state.status);

	process_end_time = current_kernel_time();
	bytes_read = prev_fill_count - fill_count;

	mpq_dmx_update_sdmx_stat(mpq_demux, bytes_read,
			&process_start_time, &process_end_time);

	MPQ_DVB_DBG_PRINT(
		"%s: SDMX result=%d, input_fill_count=%u, read_offset=%u, read %d bytes from input, status=0x%X, errors=0x%X\n",
		__func__, sdmx_res, fill_count, read_offset, bytes_read,
		status, errors);

	if ((sdmx_res == SDMX_SUCCESS) ||
		(sdmx_res == SDMX_STATUS_STALLED_IN_PULL_MODE)) {
		if (sdmx_res == SDMX_STATUS_STALLED_IN_PULL_MODE)
			MPQ_DVB_DBG_PRINT("%s: SDMX stalled for PULL mode\n",
				__func__);

		mpq_sdmx_process_results(mpq_demux);
	} else {
		MPQ_DVB_ERR_PRINT(
			"%s: SDMX Process returned %d\n",
			__func__, sdmx_res);
	}

	mutex_unlock(&mpq_demux->mutex);

	return bytes_read;
}

int mpq_sdmx_process(struct mpq_demux *mpq_demux,
	struct sdmx_buff_descr *input,
	u32 fill_count,
	u32 read_offset,
	size_t tsp_size)
{
	int ret;
	int todo;
	int total_bytes_read = 0;
	int limit = mpq_sdmx_proc_limit * tsp_size;

	MPQ_DVB_DBG_PRINT(
		"\n\n%s: read_offset=%u, fill_count=%u, tsp_size=%u\n",
		__func__, read_offset, fill_count, tsp_size);

	while (fill_count >= tsp_size) {
		todo = fill_count > limit ? limit : fill_count;
		ret = mpq_sdmx_process_buffer(mpq_demux, input, todo,
			read_offset);

		if (mpq_demux->demux.sw_filter_abort) {
			MPQ_DVB_ERR_PRINT(
				"%s: Demuxing from DVR was aborted\n",
				__func__);
			return -ENODEV;
		}

		if (ret > 0) {
			total_bytes_read += ret;
			fill_count -= ret;
			read_offset += ret;
			if (read_offset >= input->size)
				read_offset -= input->size;
		} else if (ret == 0) {
			/* Not enough data to read (less than 1 TS packet) */
			break;
		} else {
			/* Some error occurred */
			MPQ_DVB_ERR_PRINT(
				"%s: mpq_sdmx_process_buffer failed, returned %d\n",
				__func__, ret);
			break;
		}
	}

	return total_bytes_read;
}
EXPORT_SYMBOL(mpq_sdmx_process);

static int mpq_sdmx_write(struct mpq_demux *mpq_demux,
	struct ion_handle *input_handle,
	const char *buf,
	size_t count)
{
	struct sdmx_buff_descr buf_desc;
	u32 read_offset;
	int ret;

	if (mpq_demux == NULL || input_handle == NULL) {
		MPQ_DVB_ERR_PRINT("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	ret = mpq_sdmx_dvr_buffer_desc(mpq_demux, &buf_desc);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
			"%s: Failed to init input buffer descriptor. ret = %d\n",
			__func__, ret);
		return ret;
	}
	read_offset = mpq_demux->demux.dmx.dvr_input.ringbuff->pread;

	return mpq_sdmx_process(mpq_demux, &buf_desc, count,
				read_offset, mpq_demux->demux.ts_packet_size);
}

int mpq_dmx_write(struct dmx_demux *demux, const char *buf, size_t count)
{
	struct dvb_demux *dvb_demux;
	struct mpq_demux *mpq_demux;
	int ret = count;

	if (demux == NULL)
		return -EINVAL;

	dvb_demux = demux->priv;
	mpq_demux = dvb_demux->priv;

	/* Route through secure demux - process secure feeds if any exist */
	if (mpq_sdmx_is_loaded() && mpq_demux->sdmx_filter_count) {
		ret = mpq_sdmx_write(mpq_demux,
			demux->dvr_input.priv_handle,
			buf,
			count);
		if (ret < 0) {
			MPQ_DVB_ERR_PRINT(
				"%s: mpq_sdmx_write failed. ret = %d\n",
				__func__, ret);
			ret = count;
		}
	}

	/*
	 * Route through sw filter - process non-secure feeds if any exist.
	 * For sw filter, should process the same amount of bytes the sdmx
	 * process managed to consume, unless some sdmx error occurred, for
	 * which should process the whole buffer
	 */
	if (mpq_demux->num_active_feeds > mpq_demux->num_secure_feeds)
		dvb_dmx_swfilter_format(dvb_demux, buf, ret,
			dvb_demux->tsp_format);

	if (signal_pending(current))
		return -EINTR;

	return ret;
}
EXPORT_SYMBOL(mpq_dmx_write);

int mpq_sdmx_is_loaded(void)
{
	static int sdmx_load_checked;

	if (mpq_bypass_sdmx)
		return 0;

	if (!sdmx_load_checked) {
		mpq_sdmx_check_app_loaded();
		sdmx_load_checked = 1;
	}

	return mpq_dmx_info.secure_demux_app_loaded;
}
EXPORT_SYMBOL(mpq_sdmx_is_loaded);

int mpq_dmx_oob_command(struct dvb_demux_feed *feed,
	struct dmx_oob_command *cmd)
{
	struct mpq_feed *mpq_feed = feed->priv;
	struct mpq_demux *mpq_demux = mpq_feed->mpq_demux;
	struct dmx_data_ready event;
	int ret = 0;

	mutex_lock(&mpq_demux->mutex);
	mpq_feed = feed->priv;

	if (!dvb_dmx_is_video_feed(feed) && !dvb_dmx_is_pcr_feed(feed) &&
		!feed->secure_mode.is_secured) {
		mutex_unlock(&mpq_demux->mutex);
		return 0;
	}

	event.data_length = 0;

	switch (cmd->type) {
	case DMX_OOB_CMD_EOS:
		event.status = DMX_OK_EOS;
		if (!feed->secure_mode.is_secured) {
			if (dvb_dmx_is_video_feed(feed)) {
				if (!video_framing)
					mpq_dmx_decoder_pes_closure(mpq_demux,
						mpq_feed);
				else
					mpq_dmx_decoder_frame_closure(mpq_demux,
						mpq_feed);
				ret = mpq_dmx_decoder_eos_cmd(mpq_feed);
				if (ret)
					MPQ_DVB_ERR_PRINT(
						"%s: Couldn't write oob eos packet\n",
						__func__);
			}
			ret = feed->data_ready_cb.ts(&feed->feed.ts, &event);
		} else if (!mpq_demux->sdmx_eos) {
			struct sdmx_buff_descr buf_desc;

			mpq_demux->sdmx_eos = 1;
			ret = mpq_sdmx_dvr_buffer_desc(mpq_demux, &buf_desc);
			if (!ret) {
				mutex_unlock(&mpq_demux->mutex);
				mpq_sdmx_process_buffer(mpq_demux, &buf_desc,
					0, 0);
				return 0;
			}
		}
		break;
	case DMX_OOB_CMD_MARKER:
		event.status = DMX_OK_MARKER;
		event.marker.id = cmd->params.marker.id;

		if (feed->type == DMX_TYPE_SEC)
			ret = dvb_dmx_notify_section_event(feed, &event, 1);
		else
			/* MPQ_TODO: Notify decoder via the stream buffer */
			ret = feed->data_ready_cb.ts(&feed->feed.ts, &event);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&mpq_demux->mutex);
	return ret;
}
EXPORT_SYMBOL(mpq_dmx_oob_command);
