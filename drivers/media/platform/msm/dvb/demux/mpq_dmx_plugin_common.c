// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/file.h>
#include <linux/scatterlist.h>
#include <linux/platform_device.h>
#include "mpq_dvb_debug.h"
#include "mpq_dmx_plugin_common.h"
#include "mpq_sdmx.h"
#include <linux/dma-buf.h>
#include <linux/ion_kernel.h>
#include <linux/sched/signal.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include <soc/qcom/qtee_shmbridge.h>

#define SDMX_MAJOR_VERSION_MATCH	(8)

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

/* TODO: Convert below parameters to sysfs tunables */
/* Number of demux devices, has default of linux configuration */
static int mpq_demux_device_num = CONFIG_DVB_MPQ_NUM_DMX_DEVICES;
static int video_secure_ion_heap = ION_CP_MM_HEAP_ID;
static int video_nonsecure_ion_heap = ION_SYSTEM_HEAP_ID;
/* Value of TS packet scramble bits field for even key */
static int mpq_sdmx_scramble_even = 0x2;
/* Value of TS packet scramble bits field for odd key */
static int mpq_sdmx_scramble_odd = 0x3;

/*
 * Default action (discard or pass) taken when scramble bit is not one of the
 * pass-through / odd / even values.
 * When set packets will be discarded, otherwise passed through.
 */
static int mpq_sdmx_scramble_default_discard = 1;

/* Max number of TS packets allowed as input for a single sdmx process */
static int mpq_sdmx_proc_limit = MAX_TS_PACKETS_FOR_SDMX_PROCESS;

/* Debug flag for secure demux process */
static int mpq_sdmx_debug;

/*
 * Indicates whether the demux should search for frame boundaries
 * and notify on video packets on frame-basis or whether to provide
 * only video PES packet payloads as-is.
 */
static int video_framing = 1;

/* TSIF operation mode: 1 = TSIF_MODE_1,  2 = TSIF_MODE_2, 3 = TSIF_LOOPBACK */
static int tsif_mode = 2;

/* Inverse TSIF clock signal */
static int clock_inv;

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


int mpq_dmx_get_param_scramble_odd(void)
{
	return mpq_sdmx_scramble_odd;
}

int mpq_dmx_get_param_scramble_even(void)
{
	return mpq_sdmx_scramble_even;
}

int mpq_dmx_get_param_scramble_default_discard(void)
{
	return mpq_sdmx_scramble_default_discard;
}

int mpq_dmx_get_param_tsif_mode(void)
{
	return tsif_mode;
}

int mpq_dmx_get_param_clock_inv(void)
{
	return clock_inv;
}

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

static int mpq_dmx_dmabuf_map(int ion_fd, struct sg_table **sgt,
				struct dma_buf_attachment **attach,
				struct dma_buf **dmabuf)
{
	struct dma_buf *new_dma_buf = NULL;
	struct dma_buf_attachment *new_attach = NULL;
	struct sg_table *new_sgt = NULL;
	int ret = 0;

	new_dma_buf = dma_buf_get(ion_fd);
	if (new_dma_buf == NULL) {
		MPQ_DVB_ERR_PRINT("%s: dma_buf_get() for ion_fd %d failed\n",
							__func__, ion_fd);
		ret = -ENOMEM;
		goto err;
	}

	new_attach = dma_buf_attach(new_dma_buf,
				    &mpq_dmx_info.devices[0].pdev->dev);

	if (IS_ERR(new_attach)) {
		MPQ_DVB_ERR_PRINT("%s: dma_buf_attach() for ion_fd %d failed\n",
							__func__, ion_fd);
		ret = -ENOMEM;
		goto err_put;
	}

	new_sgt = dma_buf_map_attachment(new_attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(new_sgt)) {
		ret = PTR_ERR(new_sgt);
		MPQ_DVB_ERR_PRINT(
		"%s: dma_buf_map_attachment for ion_fd %d failed ret = %d\n",
		__func__, ion_fd, ret);
		goto err_detach;
	}
	*sgt = new_sgt;
	*attach = new_attach;
	*dmabuf = new_dma_buf;

	return ret;

err_detach:
	dma_buf_detach(new_dma_buf, new_attach);
err_put:
	dma_buf_put(new_dma_buf);
err:
	return ret;
}

/*
 * CR-2864017: Deregister dma buffers from shmbridge here, because
 * mpq_sdmx_destroy_shm_bridge_callback will not be called if the OMX
 * application does't release buffers but reuse them even after switching
 * PES filters.
 */
static void mpq_dmx_dmabuf_unmap(struct sg_table *sgt,
		struct dma_buf_attachment *attach,
		struct dma_buf *dmabuf)
{
	int ret = 0;
	uint64_t handle = 0;

	dma_buf_unmap_attachment(attach, sgt, DMA_BIDIRECTIONAL);
	dma_buf_detach(dmabuf, attach);

	handle = (uint64_t)dmabuf->dtor_data;
	MPQ_DVB_DBG_PRINT("%s: to destroy shm bridge %lld\n",
			__func__, handle);
	ret = qtee_shmbridge_deregister(handle);
	if (ret) {
		MPQ_DVB_ERR_PRINT("%s: failed to destroy shm bridge %lld\n",
				__func__, handle);
	}

	dma_buf_set_destructor(dmabuf, NULL, NULL);
	dma_buf_put(dmabuf);
}

/* convert ion_fd to phys_adds and virt_addr*/
static int mpq_dmx_vaddr_map(int ion_fd,
			phys_addr_t *paddr, void **vaddr,
			struct sg_table **sgt,
			struct dma_buf_attachment **attach,
			size_t *sb_length, struct dma_buf **dmabuf)
{
	struct dma_buf *new_dma_buf = NULL;
	struct dma_buf_attachment *new_attach = NULL;
	struct sg_table *new_sgt = NULL;
	void *new_va = NULL;
	int ret = 0;

	ret = mpq_dmx_dmabuf_map(ion_fd, &new_sgt, &new_attach, &new_dma_buf);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
				"%s: qseecom_dmabuf_map for ion_fd %d failed ret = %d\n",
				__func__, ion_fd, ret);
		goto err;
	}

	*paddr = sg_dma_address(new_sgt->sgl);
	*sb_length = new_sgt->sgl->length;

	dma_buf_begin_cpu_access(new_dma_buf, DMA_BIDIRECTIONAL);
	/* TODO: changing kmap to vmap otherwise you need to map
	 * each 4K chunk need to be mapped and stored its address;
	 */
	new_va = dma_buf_vmap(new_dma_buf);
	if (!new_va) {
		MPQ_DVB_ERR_PRINT("%s: dma_buf_kmap failed\n", __func__);
		ret = -ENOMEM;
		goto err_unmap;
	}
	*dmabuf = new_dma_buf;
	*attach = new_attach;
	*sgt = new_sgt;
	*vaddr = new_va;
	return ret;

err_unmap:
	MPQ_DVB_ERR_PRINT("%s Map fail\n", __func__);
	dma_buf_end_cpu_access(new_dma_buf, DMA_BIDIRECTIONAL);
	mpq_dmx_dmabuf_unmap(new_sgt, new_attach, new_dma_buf);
err:
	MPQ_DVB_ERR_PRINT("%s Init fail\n", __func__);
	return ret;
}

static void mpq_dmx_vaddr_unmap(void *vaddr, struct sg_table *sgt,
		struct dma_buf_attachment *attach,
		struct dma_buf *dmabuf)
{
	dma_buf_vunmap(dmabuf, vaddr);
	dma_buf_end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
	mpq_dmx_dmabuf_unmap(sgt, attach, dmabuf);
}

static int mpq_dmx_paddr_map(int ion_fd,
			phys_addr_t *paddr,
			struct sg_table **sgt,
			struct dma_buf_attachment **attach,
			size_t *sb_length, struct dma_buf **dmabuf)
{
	struct dma_buf *new_dma_buf = NULL;
	struct dma_buf_attachment *new_attach = NULL;
	struct sg_table *new_sgt = NULL;
	int ret = 0;

	ret = mpq_dmx_dmabuf_map(ion_fd, &new_sgt, &new_attach, &new_dma_buf);
	if (ret) {
		MPQ_DVB_ERR_PRINT(
				"%s: qseecom_dmabuf_map for ion_fd %d failed ret = %d\n",
				__func__, ion_fd, ret);
		goto err;
	}

	*paddr = sg_dma_address(new_sgt->sgl);
	*sb_length = new_sgt->sgl->length;

	dma_buf_begin_cpu_access(new_dma_buf, DMA_BIDIRECTIONAL);
	*dmabuf = new_dma_buf;
	*attach = new_attach;
	*sgt = new_sgt;
	return ret;

err:
	return ret;
}

static void mpq_dmx_paddr_unmap(struct sg_table *sgt,
		struct dma_buf_attachment *attach,
		struct dma_buf *dmabuf)
{
	dma_buf_end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
	mpq_dmx_dmabuf_unmap(sgt, attach, dmabuf);
}

/*
 * mpq_dmx_update_decoder_stat -
 * Update decoder output statistics in debug-fs.
 *
 * @mpq_feed: decoder feed object
 */
void mpq_dmx_update_decoder_stat(struct mpq_feed *mpq_feed)
{
	ktime_t curr_time;
	u32 delta_time_ms;
	struct mpq_demux *mpq_demux = mpq_feed->mpq_demux;
	enum mpq_adapter_stream_if idx;

	if (!dvb_dmx_is_video_feed(mpq_feed->dvb_demux_feed))
		return;

	if (mpq_feed->video_info.stream_interface <=
			MPQ_ADAPTER_VIDEO3_STREAM_IF)
		idx = mpq_feed->video_info.stream_interface;
	else
		return;

	curr_time = ktime_get();
	if (unlikely(!mpq_demux->decoder_stat[idx].out_count)) {
		mpq_demux->decoder_stat[idx].out_last_time = curr_time;
		mpq_demux->decoder_stat[idx].out_count++;
		return;
	}

	/* calculate time-delta between frame */
	delta_time_ms = mpq_dmx_calc_time_delta(curr_time,
		mpq_demux->decoder_stat[idx].out_last_time);

	mpq_demux->decoder_stat[idx].out_interval_sum += delta_time_ms;

	mpq_demux->decoder_stat[idx].out_interval_average =
	  mpq_demux->decoder_stat[idx].out_interval_sum /
	  mpq_demux->decoder_stat[idx].out_count;

	if (delta_time_ms > mpq_demux->decoder_stat[idx].out_interval_max)
		mpq_demux->decoder_stat[idx].out_interval_max = delta_time_ms;

	mpq_demux->decoder_stat[idx].out_last_time = curr_time;
	mpq_demux->decoder_stat[idx].out_count++;
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
		u32 bytes_processed, ktime_t process_start_time,
		ktime_t process_end_time)
{
	u32 packets_num;
	u32 process_time;

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

	if (count == 0 || count >= 16)
		return -EINVAL;

	memset(user_str, '\0', sizeof(user_str));

	ret_count = simple_write_to_buffer(user_str, 15, position, user_buffer,
		count);
	if (ret_count < 0)
		return ret_count;
	else if (ret_count == 0)
		return -EINVAL;

	ret = kstrtoint(user_str, 0, &level);
	if (ret)
		return ret;

	if (level < SDMX_LOG_NO_PRINT || level > SDMX_LOG_VERBOSE)
		return -EINVAL;

	mutex_lock_interruptible(&mpq_demux->mutex);
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
	.open = simple_open,
	.read = mpq_sdmx_log_level_read,
	.write = mpq_sdmx_log_level_write,
	.owner = THIS_MODULE,
};

/* Extend dvb-demux debugfs with common plug-in entries */
void mpq_dmx_init_debugfs_entries(struct mpq_demux *mpq_demux)
{
	int i;
	char file_name[50];
	struct dentry *debugfs_decoder_dir;

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
		0664,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		&mpq_demux->hw_notification_interval);

	debugfs_create_u32(
		"hw_notification_min_interval",
		0664,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		&mpq_demux->hw_notification_min_interval);

	debugfs_create_u32(
		"hw_notification_count",
		0664,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		&mpq_demux->hw_notification_count);

	debugfs_create_u32(
		"hw_notification_size",
		0664,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		&mpq_demux->hw_notification_size);

	debugfs_create_u32(
		"hw_notification_min_size",
		0664,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		&mpq_demux->hw_notification_min_size);

	debugfs_decoder_dir = debugfs_create_dir("decoder",
		mpq_demux->demux.dmx.debugfs_demux_dir);

	for (i = 0;
		 debugfs_decoder_dir &&
		 (i < MPQ_ADAPTER_MAX_NUM_OF_INTERFACES);
		 i++) {
		snprintf(file_name, 50, "decoder%d_drop_count", i);
		debugfs_create_u32(
			file_name,
			0444,
			debugfs_decoder_dir,
			&mpq_demux->decoder_stat[i].drop_count);

		snprintf(file_name, 50, "decoder%d_out_count", i);
		debugfs_create_u32(
			file_name,
			0444,
			debugfs_decoder_dir,
			&mpq_demux->decoder_stat[i].out_count);

		snprintf(file_name, 50, "decoder%d_out_interval_sum", i);
		debugfs_create_u32(
			file_name,
			0444,
			debugfs_decoder_dir,
			&mpq_demux->decoder_stat[i].out_interval_sum);

		snprintf(file_name, 50, "decoder%d_out_interval_average", i);
		debugfs_create_u32(
			file_name,
			0444,
			debugfs_decoder_dir,
			&mpq_demux->decoder_stat[i].out_interval_average);

		snprintf(file_name, 50, "decoder%d_out_interval_max", i);
		debugfs_create_u32(
			file_name,
			0444,
			debugfs_decoder_dir,
			&mpq_demux->decoder_stat[i].out_interval_max);

		snprintf(file_name, 50, "decoder%d_ts_errors", i);
		debugfs_create_u32(
			file_name,
			0444,
			debugfs_decoder_dir,
			&mpq_demux->decoder_stat[i].ts_errors);

		snprintf(file_name, 50, "decoder%d_cc_errors", i);
		debugfs_create_u32(
			file_name,
			0444,
			debugfs_decoder_dir,
			&mpq_demux->decoder_stat[i].cc_errors);
	}

	debugfs_create_u32(
		"sdmx_process_count",
		0664,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		&mpq_demux->sdmx_process_count);

	debugfs_create_u32(
		"sdmx_process_time_sum",
		0664,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		&mpq_demux->sdmx_process_time_sum);

	debugfs_create_u32(
		"sdmx_process_time_average",
		0664,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		&mpq_demux->sdmx_process_time_average);

	debugfs_create_u32(
		"sdmx_process_time_max",
		0664,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		&mpq_demux->sdmx_process_time_max);

	debugfs_create_u32(
		"sdmx_process_packets_sum",
		0664,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		&mpq_demux->sdmx_process_packets_sum);

	debugfs_create_u32(
		"sdmx_process_packets_average",
		0664,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		&mpq_demux->sdmx_process_packets_average);

	debugfs_create_u32(
		"sdmx_process_packets_min",
		0664,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		&mpq_demux->sdmx_process_packets_min);

	debugfs_create_file("sdmx_log_level",
		0664,
		mpq_demux->demux.dmx.debugfs_demux_dir,
		mpq_demux,
		&sdmx_debug_fops);
}

/* Update dvb-demux debugfs with HW notification statistics */
void mpq_dmx_update_hw_statistics(struct mpq_demux *mpq_demux)
{
	ktime_t curr_time;
	u32 delta_time_ms;

	curr_time = ktime_get();
	if (likely(mpq_demux->hw_notification_count)) {
		/* calculate time-delta between notifications */
		delta_time_ms = mpq_dmx_calc_time_delta(curr_time,
			mpq_demux->last_notification_time);

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
		if ((version >> 8) != SDMX_MAJOR_VERSION_MATCH) {
			MPQ_DVB_ERR_PRINT(
				"%s: sdmx major version does not match",
				__func__);
			MPQ_DVB_ERR_PRINT(
				"%s: version : expected=%d, actual=%d\n",
				__func__, SDMX_MAJOR_VERSION_MATCH,
				(version >> 8));
		} else {
			MPQ_DVB_DBG_PRINT(
				"%s: sdmx major version is ok = %d\n",
				__func__, SDMX_MAJOR_VERSION_MATCH);
		}
	}

	mpq_dmx_info.secure_demux_app_loaded = 1;
	sdmx_close_session(session);
}

int mpq_dmx_plugin_init(mpq_dmx_init dmx_init_func,
			struct platform_device *pdev)
{
	int i;
	int j;
	int result;
	struct mpq_demux *mpq_demux;
	struct dvb_adapter *mpq_adapter;
	struct mpq_feed *feed;

	if (pdev == NULL) {
		MPQ_DVB_ERR_PRINT("%s: NULL platform device\n", __func__);
		return -EINVAL;
	}
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
		result = -ENOMEM;
		goto init_failed;
	}

	/* Initialize and register all demux devices to the system */
	for (i = 0; i < mpq_demux_device_num; i++) {
		mpq_demux = mpq_dmx_info.devices+i;
		mpq_demux->idx = i;

		/* Set platform device */
		mpq_demux->pdev = pdev;
		/* initialize demux source to memory by default */
		mpq_demux->source = DMX_SOURCE_DVR0 + i;

		mutex_init(&mpq_demux->mutex);

		dma_set_mask(&mpq_demux->pdev->dev, DMA_BIT_MASK(48));

		mpq_demux->num_secure_feeds = 0;
		mpq_demux->num_active_feeds = 0;
		mpq_demux->sdmx_filter_count = 0;
		mpq_demux->sdmx_session_handle = SDMX_INVALID_SESSION_HANDLE;
		mpq_demux->sdmx_eos = 0;
		mpq_demux->sdmx_log_level = SDMX_LOG_NO_PRINT;
		mpq_demux->ts_packet_timestamp_source = 0;
		mpq_demux->disable_cache_ops = 1;

		if (mpq_demux->demux.feednum > MPQ_MAX_DMX_FILES) {
			MPQ_DVB_ERR_PRINT(
				"%s: actual feednum (%d) larger than MPQ_MAX_DMX_FILES\n",
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

void mpq_dmx_plugin_exit(void)
{
	int i;
	struct mpq_demux *mpq_demux;

	MPQ_DVB_DBG_PRINT("%s executed\n", __func__);

	if (mpq_dmx_info.devices != NULL) {
		for (i = 0; i < mpq_demux_device_num; i++) {
			mpq_demux = mpq_dmx_info.devices + i;

			if (!mpq_demux->is_initialized)
				continue;

			if (mpq_demux->mpq_dmx_plugin_release)
				mpq_demux->mpq_dmx_plugin_release(mpq_demux);

			mpq_demux->demux.dmx.remove_frontend(
						&mpq_demux->demux.dmx,
						&mpq_demux->fe_memory);

			if (mpq_dmx_info.secure_demux_app_loaded)
				mpq_sdmx_close_session(mpq_demux);
			mutex_destroy(&mpq_demux->mutex);
			dvb_dmxdev_release(&mpq_demux->dmxdev);
			dvb_dmx_release(&mpq_demux->demux);
		}

		vfree(mpq_dmx_info.devices);
		mpq_dmx_info.devices = NULL;
	}
}

int mpq_dmx_set_source(
		struct dmx_demux *demux,
		const enum dmx_source_t *src)
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
				"%s: demux%d source can't be set\n",
				__func__, dmx_index);
			MPQ_DVB_ERR_PRINT(
				"%s: demux%d occupies this source already\n",
				__func__, i);
			return -EBUSY;
		}
	}

	mpq_demux->source = *src;
	return 0;
}

int mpq_dmx_map_buffer(struct dmx_demux *demux,
		       struct dmx_buffer *dmx_buffer,
		       struct ion_dma_buff_info *dma_buffer,
		       void **kernel_mem)
{
	struct dvb_demux *dvb_demux = demux->priv;
	struct mpq_demux *mpq_demux;
	int ret;

	if ((mpq_dmx_info.devices == NULL) || (dvb_demux == NULL) ||
		(dmx_buffer == NULL) || (kernel_mem == NULL) ||
		(dmx_buffer->handle <= 0)) {
		MPQ_DVB_ERR_PRINT("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	mpq_demux = dvb_demux->priv;
	if (mpq_demux == NULL) {
		MPQ_DVB_ERR_PRINT("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	ret = mpq_dmx_vaddr_map(dmx_buffer->handle, &dma_buffer->pa,
				&dma_buffer->va, &dma_buffer->sgt,
				&dma_buffer->attach, &dma_buffer->len,
				&dma_buffer->dmabuf);
	if (ret) {
		MPQ_DVB_ERR_PRINT("%s: Failed to map vaddr for ion_fd %d\n",
						__func__, dmx_buffer->handle);
		return -ENOMEM;
	}

	if (dma_buffer->va != NULL) {
		*kernel_mem = dma_buffer->va;
	} else {
		MPQ_DVB_ERR_PRINT("%s: Fail to map buffer\n", __func__);
		return -ENOMEM;
	}

	return 0;
}

int mpq_dmx_unmap_buffer(struct dmx_demux *demux,
			struct ion_dma_buff_info *dma_buffer)
{
	struct dvb_demux *dvb_demux = demux->priv;
	struct mpq_demux *mpq_demux;

	if ((mpq_dmx_info.devices == NULL) || (dvb_demux == NULL) ||
		(dma_buffer == NULL)) {
		MPQ_DVB_ERR_PRINT("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	mpq_demux = dvb_demux->priv;
	if (mpq_demux == NULL) {
		MPQ_DVB_ERR_PRINT("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	mpq_dmx_vaddr_unmap(dma_buffer->va, dma_buffer->sgt,
			    dma_buffer->attach, dma_buffer->dmabuf);
	memset(dma_buffer, 0, sizeof(struct ion_dma_buff_info));

	return 0;
}

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

		mutex_lock_interruptible(&mpq_demux->mutex);
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
	MPQ_DVB_ERR_PRINT("%s: Invalid feed type %d\n",
			__func__, feed->pes_type);

	return -EINVAL;
}

static int mpq_sdmx_destroy_shm_bridge_callback(struct dma_buf *dmabuf,
		void *dtor_data)
{
	int ret = 0;
	uint64_t handle = (uint64_t)dtor_data;

	if (!dmabuf) {
		MPQ_DVB_DBG_PRINT("dmabuf NULL\n");
		return -EINVAL;
	}
	MPQ_DVB_DBG_PRINT("to destroy shm bridge %lld\n", handle);
	ret = qtee_shmbridge_deregister(handle);
	if (ret) {
		MPQ_DVB_DBG_PRINT("failed to destroy shm bridge %lld\n",
				handle);
		return ret;
	}
	dma_buf_set_destructor(dmabuf, NULL, NULL);
	return ret;
}

static int mpq_sdmx_create_shm_bridge(struct dma_buf *dmabuf,
		struct sg_table *sgt)
{
	int ret = 0, i;
	phys_addr_t phys;
	size_t size = 0;
	uint64_t handle = 0;
	int tz_perm = PERM_READ|PERM_WRITE;
	unsigned long dma_buf_flags = 0;
	uint32_t *vmid_list;
	uint32_t *perms_list;
	uint32_t nelems;
	struct scatterlist *sg;

	ret = dma_buf_get_flags(dmabuf, &dma_buf_flags);
	if (ret) {
		MPQ_DVB_ERR_PRINT("%s:  failed to get dmabuf flag\n",
				__func__);
		return ret;
	}

	if (!(dma_buf_flags & ION_FLAG_SECURE)) {
		MPQ_DVB_ERR_PRINT("Not a secure buffer\n");
		return 0;
	}

	nelems = ion_get_flags_num_vm_elems(dma_buf_flags);

	vmid_list = kcalloc(nelems, sizeof(*vmid_list), GFP_KERNEL);
	if (!vmid_list) {
		ret = -ENOMEM;
		MPQ_DVB_ERR_PRINT("%s: failed at %u with ret = %d\n",
				__func__, __LINE__, ret);
		goto exit;
	}

	ret = ion_populate_vm_list(dma_buf_flags, vmid_list, nelems);
	if (ret) {
		MPQ_DVB_ERR_PRINT("%s: failed at %u with ret = %d\n",
				__func__, __LINE__, ret);
		goto exit_free_vmid_list;
	}
	perms_list = kcalloc(nelems, sizeof(*perms_list), GFP_KERNEL);
	if (!perms_list) {
		ret = -ENOMEM;
		MPQ_DVB_ERR_PRINT("%s: failed at %u with ret = %d\n",
				__func__, __LINE__, ret);
		goto exit_free_vmid_list;
	}

	for (i = 0; i < nelems; i++)
		perms_list[i] = msm_secure_get_vmid_perms(vmid_list[i]);


	sg = sgt->sgl;
	for (i = 0; i < sgt->nents; i++) {
		phys = sg_phys(sg);
		size = sg->length;

		ret = qtee_shmbridge_query(phys);
		if (ret) {
			MPQ_DVB_ERR_PRINT("shm bridge exists\n");
			goto exit_free_perms_list;
		}

		ret = qtee_shmbridge_register(phys, size, vmid_list,
				perms_list, nelems,
				tz_perm, &handle);
		if (ret && ret != -EEXIST) {
			MPQ_DVB_ERR_PRINT("shm register failed: ret: %d\n",
					ret);
			goto exit_free_perms_list;
		}

		MPQ_DVB_DBG_PRINT("%s: created shm bridge %lld\n",
				__func__,  handle);
		dma_buf_set_destructor(dmabuf,
				mpq_sdmx_destroy_shm_bridge_callback,
				(void *)handle);
		sg = sg_next(sg);
	}

exit_free_perms_list:
	kfree(perms_list);
exit_free_vmid_list:
	kfree(vmid_list);
exit:
	return ret;
}

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
	struct mpq_demux *mpq_demux,
	struct mpq_video_feed_info *feed_data,
	struct dmx_decoder_buffers *dec_buffs)
{
	struct ion_dma_buff_info *dbuf = NULL;
	int size = 0;
	int ret = 0;

	MPQ_DVB_DBG_PRINT("%s: Internal decoder buffer allocation\n", __func__);

	size = dec_buffs->buffers_size;
	size = ALIGN(size, SZ_4K);

	dbuf = &feed_data->buffer_desc.buff_dma_info[0];
	memset(dbuf, 0, sizeof(struct ion_dma_buff_info));

	dbuf->dmabuf = ion_alloc(size,
		ION_HEAP(video_secure_ion_heap) |
		ION_HEAP(video_nonsecure_ion_heap),
		mpq_demux->decoder_alloc_flags);

	if (IS_ERR_OR_NULL(dbuf->dmabuf)) {
		ret = PTR_ERR(dbuf->dmabuf);
		MPQ_DVB_ERR_PRINT(
				  "%s: FAILED to allocate sdmx buffer %d\n",
				  __func__, ret);
		ret = -ENOMEM;
		goto err;
	}

	dbuf->attach = dma_buf_attach(dbuf->dmabuf,
				    &mpq_demux->pdev->dev);
	if (IS_ERR_OR_NULL(dbuf->attach)) {
		MPQ_DVB_ERR_PRINT("%s: dma_buf_attach fail\n", __func__);
		dma_buf_put(dbuf->dmabuf);
		ret = -ENOMEM;
		goto err_put;
	}

	dbuf->sgt = dma_buf_map_attachment(dbuf->attach, DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(dbuf->sgt)) {
		ret = PTR_ERR(dbuf->sgt);
		MPQ_DVB_ERR_PRINT(
			"%s: dma_buf_map_attachment failed ret = %d\n",
			__func__, ret);
		goto err_detach;
	}

	dbuf->pa = sg_dma_address(dbuf->sgt->sgl);
	dbuf->len = dbuf->sgt->sgl->length;

	dma_buf_begin_cpu_access(dbuf->dmabuf, DMA_BIDIRECTIONAL);
	dbuf->va = dma_buf_vmap(dbuf->dmabuf);
	if (!dbuf->va) {
		MPQ_DVB_ERR_PRINT("%s: dma_buf_vmap failed\n", __func__);
		dma_buf_end_cpu_access(dbuf->dmabuf, DMA_BIDIRECTIONAL);
		mpq_dmx_dmabuf_unmap(dbuf->sgt, dbuf->attach, dbuf->dmabuf);
		ret = -ENOMEM;
		goto err;
	}

	feed_data->buffer_desc.decoder_buffers_num = 1;
	feed_data->buffer_desc.desc[0].base = dbuf->va;
	feed_data->buffer_desc.desc[0].size = size;
	feed_data->buffer_desc.desc[0].read_ptr = 0;
	feed_data->buffer_desc.desc[0].write_ptr = 0;

	ret = mpq_sdmx_create_shm_bridge(dbuf->dmabuf, dbuf->sgt);
	if (ret) {
		MPQ_DVB_ERR_PRINT("%s mpq_sdmx_create_shm_bridge failed\n");
		return ret;
	}

	return 0;

err_detach:
	dma_buf_detach(dbuf->dmabuf, dbuf->attach);
err_put:
	dma_buf_put(dbuf->dmabuf);
err:
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
	bool is_secure_feed)
{
	struct ion_dma_buff_info buff;
	int actual_buffer_size = 0;
	int ret = 0;
	int i;

	/*
	 * Payload buffer was allocated externally (through ION).
	 * Map the ion handles to kernel memory
	 */
	MPQ_DVB_DBG_PRINT("%s: External decoder buffer allocation\n", __func__);
	memset(&buff, 0, sizeof(struct ion_dma_buff_info));

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
		if (!is_secure_feed) {
			mpq_dmx_vaddr_map(dec_buffs->handles[i], &buff.pa,
				&buff.va, &buff.sgt, &buff.attach,
				&buff.len, &buff.dmabuf);

			if (ret < 0) {
				MPQ_DVB_ERR_PRINT(
				"%s: Failed mapping buffer %d\n",
				__func__, i);
				goto init_failed;
			}
			memcpy(&feed_data->buffer_desc.buff_dma_info[i], &buff,
					sizeof(struct ion_dma_buff_info));

		} else {
			mpq_dmx_paddr_map(dec_buffs->handles[i], &buff.pa,
				&buff.sgt, &buff.attach,
				&buff.len, &buff.dmabuf);

			if (ret < 0) {
				MPQ_DVB_ERR_PRINT(
				"%s: Failed mapping buffer %d\n",
				__func__, i);
				goto init_failed;
			}
			memcpy(&feed_data->buffer_desc.buff_dma_info[i], &buff,
				sizeof(struct ion_dma_buff_info));

		}
		if (buff.va)
			feed_data->buffer_desc.desc[i].base = buff.va;
		else
			feed_data->buffer_desc.desc[i].base = NULL;

		feed_data->buffer_desc.desc[i].handle =
			dec_buffs->handles[i];
		feed_data->buffer_desc.desc[i].size =
			dec_buffs->buffers_size;
		feed_data->buffer_desc.desc[i].read_ptr = 0;
		feed_data->buffer_desc.desc[i].write_ptr = 0;

		memset(&buff, 0, sizeof(struct ion_dma_buff_info));

		MPQ_DVB_DBG_PRINT(
			"%s: Buffer #%d: handle=%d, size=%d\n",
			__func__, i,
			feed_data->buffer_desc.desc[i].handle,
			feed_data->buffer_desc.desc[i].size);
	}

	return 0;

init_failed:
	MPQ_DVB_DBG_PRINT("%s: Init failed\n", __func__);

	for (i = 0; i < feed_data->buffer_desc.decoder_buffers_num; i++) {
		struct ion_dma_buff_info *buff =
			&feed_data->buffer_desc.buff_dma_info[i];
		if (feed_data->buffer_desc.desc[i].base) {
			mpq_dmx_vaddr_unmap(buff->va, buff->sgt,
					    buff->attach,
					    buff->dmabuf);
			feed_data->buffer_desc.desc[i].base = NULL;
		}
		feed_data->buffer_desc.desc[i].size = 0;
		memset(buff, 0, sizeof(struct ion_dma_buff_info));
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
	struct dmx_decoder_buffers *dec_buffs = NULL;
	enum mpq_streambuffer_mode mode;
	bool is_secure_feed = false;

	dec_buffs = feed->dvb_demux_feed->feed.ts.decoder_buffers;
	is_secure_feed = feed->dvb_demux_feed->secure_mode.is_secured;

	/* Allocate packet buffer holding the meta-data */
	packet_buffer = vmalloc(VIDEO_META_DATA_BUFFER_SIZE);

	if (packet_buffer == NULL) {
		ret = -ENOMEM;
		goto end;
	}

	MPQ_DVB_DBG_PRINT("%s: dec_buffs: num=%d, size=%d, linear=%d\n",
			__func__,
			dec_buffs->buffers_num,
			dec_buffs->buffers_size,
			dec_buffs->is_linear);

	if (dec_buffs->buffers_num == 0)
		ret = mpq_dmx_init_internal_buffers(
			mpq_demux, feed_data, dec_buffs);
	else
		ret = mpq_dmx_init_external_buffers(
			feed_data, dec_buffs, is_secure_feed);

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
	struct ion_dma_buff_info *buff = NULL;

	mpq_adapter_unregister_stream_if(feed_data->stream_interface);

	mpq_streambuffer_terminate(video_buffer);

	vfree(video_buffer->packet_data.data);

	buf_num = feed_data->buffer_desc.decoder_buffers_num;

	for (i = 0; i < buf_num; i++) {
		buff = &feed_data->buffer_desc.buff_dma_info[i];
		if (buff->va) {
			if (feed_data->buffer_desc.desc[i].base) {
				mpq_dmx_vaddr_unmap(buff->va, buff->sgt,
					buff->attach, buff->dmabuf);
				feed_data->buffer_desc.desc[i].base = NULL;
			}

			/*
			 * Un-share the buffer if kernel it the one that
			 * shared it.
			 */
			if (!dec_buffs->buffers_num &&
				feed_data->buffer_desc.shared_file) {
				fput(feed_data->buffer_desc.shared_file);
				feed_data->buffer_desc.shared_file = NULL;
			}

			feed_data->buffer_desc.desc[i].size = 0;
		} else if (buff->pa) {
			mpq_dmx_paddr_unmap(buff->sgt, buff->attach,
					    buff->dmabuf);
			buff->pa = 0;
		}
		memset(buff, 0, sizeof(struct ion_dma_buff_info));
	}
}

int mpq_dmx_flush_stream_buffer(struct dvb_demux_feed *feed)
{
	struct mpq_feed *mpq_feed = feed->priv;
	struct mpq_video_feed_info *feed_data = &mpq_feed->video_info;
	struct mpq_streambuffer *sbuff;
	int ret = 0;

	if (!dvb_dmx_is_video_feed(feed)) {
		MPQ_DVB_DBG_PRINT("%s: not a video feed, feed type=%d\n",
			__func__, feed->pes_type);
		return 0;
	}

	spin_lock(&feed_data->video_buffer_lock);

	sbuff = feed_data->video_buffer;
	if (sbuff == NULL) {
		MPQ_DVB_DBG_PRINT("%s: feed_data->video_buffer is NULL\n",
			__func__);
		spin_unlock(&feed_data->video_buffer_lock);
		return -ENODEV;
	}

	feed_data->pending_pattern_len = 0;

	ret = mpq_streambuffer_flush(sbuff);
	if (ret)
		MPQ_DVB_ERR_PRINT("%s: mpq_streambuffer_flush failed, ret=%d\n",
			__func__, ret);

	spin_unlock(&feed_data->video_buffer_lock);

	return ret;
}

static int mpq_dmx_flush_buffer(struct dmx_ts_feed *ts_feed, size_t length)
{
	struct dvb_demux_feed *feed = (struct dvb_demux_feed *)ts_feed;
	struct dvb_demux *demux = feed->demux;
	int ret = 0;

	if (mutex_lock_interruptible(&demux->mutex))
		return -ERESTARTSYS;

	dvbdmx_ts_reset_pes_state(feed);

	if (dvb_dmx_is_video_feed(feed)) {
		MPQ_DVB_DBG_PRINT("%s: flushing video buffer\n", __func__);

		ret = mpq_dmx_flush_stream_buffer(feed);
	}
	mutex_unlock(&demux->mutex);
	return ret;
}

/**
 * mpq_dmx_init_video_feed - Initializes of video feed information
 * used to pass data directly to decoder.
 *
 * @mpq_feed: The mpq feed object
 *
 * Return     error code.
 */
int mpq_dmx_init_video_feed(struct mpq_feed *mpq_feed)
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
	case DMX_PES_VIDEO0:
		feed_data->stream_interface =
			MPQ_ADAPTER_VIDEO0_STREAM_IF;
		break;

	case DMX_PES_VIDEO1:
		feed_data->stream_interface =
			MPQ_ADAPTER_VIDEO1_STREAM_IF;
		break;

	case DMX_PES_VIDEO2:
		feed_data->stream_interface =
			MPQ_ADAPTER_VIDEO2_STREAM_IF;
		break;

	case DMX_PES_VIDEO3:
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
			"%s: mpq_adapter_register_stream_if failed, err = %d\n",
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

	mpq_demux->decoder_stat[feed_data->stream_interface].drop_count = 0;
	mpq_demux->decoder_stat[feed_data->stream_interface].out_count = 0;
	mpq_demux->decoder_stat[feed_data->stream_interface].out_interval_sum
									= 0;
	mpq_demux->decoder_stat[feed_data->stream_interface].out_interval_max
									= 0;
	mpq_demux->decoder_stat[feed_data->stream_interface].ts_errors = 0;
	mpq_demux->decoder_stat[feed_data->stream_interface].cc_errors = 0;

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
int mpq_dmx_terminate_video_feed(struct mpq_feed *mpq_feed)
{
	struct mpq_streambuffer *video_buffer;
	struct mpq_video_feed_info *feed_data;
	struct mpq_demux *mpq_demux;

	if (mpq_feed == NULL)
		return -EINVAL;

	mpq_demux = mpq_feed->mpq_demux;
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

struct dvb_demux_feed *mpq_dmx_peer_rec_feed(struct dvb_demux_feed *feed)
{
	struct dvb_demux_feed *tmp;
	struct dvb_demux *dvb_demux = feed->demux;

	list_for_each_entry(tmp, &dvb_demux->feed_list, list_head) {
		if (tmp != feed && tmp->state == DMX_STATE_GO &&
			tmp->feed.ts.buffer.ringbuff ==
			feed->feed.ts.buffer.ringbuff) {
			MPQ_DVB_DBG_PRINT(
				"%s: main feed pid=%d, secondary feed pid=%d\n",
				__func__, tmp->pid, feed->pid);
			return tmp;
		}
	}

	return NULL;
}

static int mpq_sdmx_alloc_data_buf(struct mpq_feed *mpq_feed, size_t size)
{
	int ret = 0;
	struct sdmx_buff_descriptor *desc = &mpq_feed->data_desc;
	struct qtee_shm *shminfo = NULL;

	shminfo = vmalloc(sizeof(struct qtee_shm));
	if (!shminfo) {
		MPQ_DVB_ERR_PRINT("%s: shminfo alloc failed\n");
		return -ENOMEM;
	}

	qtee_shmbridge_allocate_shm(size, shminfo);
	desc->size = size;
	desc->phys_base = shminfo->paddr;
	desc->virt_base = shminfo->vaddr;
	desc->user = (void *)shminfo;

	if (IS_ERR_OR_NULL(desc->virt_base)) {
		ret = PTR_ERR(desc->virt_base);
		MPQ_DVB_ERR_PRINT("%s: qtee_shmbridge_allocate_shm failed\n",
				__func__);
		return ret;
	}

	dvb_ringbuffer_init(&mpq_feed->sdmx_buf, desc->virt_base, size);
	mpq_feed->sdmx_dma_buff.va = desc->virt_base;

	return 0;
}

static int mpq_sdmx_free_data_buf(struct mpq_feed *mpq_feed)
{
	struct sdmx_buff_descriptor *desc = &mpq_feed->data_desc;

	qtee_shmbridge_free_shm((struct qtee_shm *) desc->user);
	vfree(desc->user);
	MPQ_DVB_DBG_PRINT("%s: = qtee_shmbridge_free\n", __func__);

	memset(desc, 0, sizeof(struct sdmx_buff_descriptor));
	return 0;
}

static int mpq_sdmx_init_metadata_buffer(struct mpq_demux *mpq_demux,
		struct mpq_feed *feed,
		struct sdmx_buff_descr *metadata_buff_desc)
{
	int ret = 0;
	struct sdmx_buff_descriptor *desc = &feed->metadata_desc;
	struct qtee_shm *shminfo = NULL;

	shminfo = vmalloc(sizeof(struct qtee_shm));

	if (!shminfo) {
		MPQ_DVB_ERR_PRINT("%s: shminfo alloc failed\n");
		return -ENOMEM;
	}
	qtee_shmbridge_allocate_shm(SDMX_METADATA_BUFFER_SIZE, shminfo);

	desc->phys_base = shminfo->paddr;
	desc->virt_base = shminfo->vaddr;
	desc->user = (void *)shminfo;

	if (IS_ERR_OR_NULL(desc->virt_base)) {
		ret = PTR_ERR(desc->virt_base);
		MPQ_DVB_ERR_PRINT(
			"%s: dma_buf_map_attachment failed ret = %d\n",
			__func__, ret);
		return ret;
	}
	desc->size = SDMX_METADATA_BUFFER_SIZE;

	metadata_buff_desc->size = SDMX_METADATA_BUFFER_SIZE;
	metadata_buff_desc->base_addr = desc->phys_base;
	dvb_ringbuffer_init(&feed->metadata_buf, desc->virt_base,
		SDMX_METADATA_BUFFER_SIZE);

	return 0;
}

static int mpq_sdmx_terminate_metadata_buffer(struct mpq_feed *mpq_feed)
{
	struct sdmx_buff_descriptor *desc = &mpq_feed->metadata_desc;

	qtee_shmbridge_free_shm((struct qtee_shm *) desc->user);
	vfree(desc->user);
	MPQ_DVB_DBG_PRINT("%s: = qtee_shmbridge_free\n", __func__);

	memset(desc, 0, sizeof(struct sdmx_buff_descriptor));
	return 0;
}

int mpq_dmx_terminate_feed(struct dvb_demux_feed *feed)
{
	int ret = 0;
	struct mpq_demux *mpq_demux;
	struct mpq_feed *mpq_feed;
	struct mpq_feed *main_rec_feed = NULL;
	struct dvb_demux_feed *tmp;

	if (feed == NULL)
		return -EINVAL;

	mpq_demux = feed->demux->priv;

	mutex_lock(&mpq_demux->mutex);
	mpq_feed = feed->priv;

	if (mpq_feed->sdmx_filter_handle != SDMX_INVALID_FILTER_HANDLE) {
		if (mpq_feed->filter_type == SDMX_RAW_FILTER) {
			tmp = mpq_dmx_peer_rec_feed(feed);
			if (tmp)
				main_rec_feed = tmp->priv;
		}

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
		if (mpq_demux->num_secure_feeds > 0)
			mpq_demux->num_secure_feeds--;
		else
			MPQ_DVB_DBG_PRINT("%s: Invalid secure feed count= %u\n",
				 __func__, mpq_demux->num_secure_feeds);
	}

	if (dvb_dmx_is_video_feed(feed)) {

		ret = mpq_dmx_terminate_video_feed(mpq_feed);
		if (ret)
			MPQ_DVB_ERR_PRINT(
				"%s: mpq_dmx_terminate_video_feed failed. ret = %d\n",
				__func__, ret);
	}

	if (mpq_feed->sdmx_dma_buff.va) {
		wake_up_all(&mpq_feed->sdmx_buf.queue);
		mpq_sdmx_free_data_buf(mpq_feed);
	}

	mpq_sdmx_terminate_metadata_buffer(mpq_feed);
	if (mpq_demux->num_active_feeds > 0)
		mpq_demux->num_active_feeds--;
	else
		MPQ_DVB_DBG_PRINT("%s: Invalid num_active_feeds count = %u\n",
				  __func__, mpq_demux->num_active_feeds);

	mutex_unlock(&mpq_demux->mutex);

	return ret;
}

int mpq_dmx_decoder_fullness_init(struct dvb_demux_feed *feed)
{
	struct mpq_feed *mpq_feed;

	if (dvb_dmx_is_video_feed(feed)) {
		struct mpq_video_feed_info *feed_data;

		mpq_feed = feed->priv;
		feed_data = &mpq_feed->video_info;
		feed_data->fullness_wait_cancel = 0;

		return 0;
	}

	MPQ_DVB_DBG_PRINT("%s: Invalid feed type %d\n", __func__,
			   feed->pes_type);

	return -EINVAL;
}

/**
 * Returns whether the free space of decoder's output
 * buffer is larger than specific number of bytes.
 *
 * @sbuff: MPQ stream buffer used for decoder data.
 * @required_space: number of required free bytes in the buffer
 *
 * Return 1 if required free bytes are available, 0 otherwise.
 */
static inline int mpq_dmx_check_video_decoder_fullness(
	struct mpq_streambuffer *sbuff,
	size_t required_space)
{
	ssize_t free = mpq_streambuffer_data_free(sbuff);
	ssize_t free_meta = mpq_streambuffer_metadata_free(sbuff);

	/* Verify meta-data buffer can contain at least 1 packet */
	if (free_meta < VIDEO_META_DATA_PACKET_SIZE)
		return 0;

	/*
	 * For linear buffers, verify there's enough space for this TSP
	 * and an additional buffer is free, as framing might required one
	 * more buffer to be available.
	 */
	if (sbuff->mode == MPQ_STREAMBUFFER_BUFFER_MODE_LINEAR)
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
		mutex_trylock(&mpq_demux->mutex);
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
		(!mpq_dmx_check_video_decoder_fullness(sbuff,
						       required_space))) {
		DEFINE_WAIT(__wait);

		for (;;) {
			prepare_to_wait(&sbuff->raw_data.queue,
				&__wait,
				TASK_INTERRUPTIBLE);
			if (!feed_data->video_buffer ||
				feed_data->fullness_wait_cancel ||
				mpq_dmx_check_video_decoder_fullness(sbuff,
					required_space))
				break;

			if (!signal_pending(current)) {
				mutex_unlock(&mpq_demux->mutex);
				schedule();
				mutex_trylock(&mpq_demux->mutex);
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
				"%s: video_buffer released\n", __func__);
			spin_unlock(&feed_data->video_buffer_lock);
			return 0;
		}

		video_buff = &feed_data->video_buffer->raw_data;
		wake_up_all(&video_buff->queue);
		spin_unlock(&feed_data->video_buffer_lock);

		return 0;
	}
	MPQ_DVB_ERR_PRINT(
			"%s: Invalid feed type %d\n", __func__, feed->pes_type);

	return -EINVAL;
}

int mpq_dmx_parse_mandatory_pes_header(
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

int mpq_dmx_parse_remaining_pes_header(
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
			struct dmx_data_ready *data,
			int cookie)
{
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
	data->buf.cookie = cookie;
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
	struct ion_dma_buff_info *dma_buff =
		&mpq_demux->demux.dmx.dvr_input.buff_dma_info;
	if (dma_buff->pa) {
		buf_desc->base_addr = (u64)dma_buff->pa;
		buf_desc->size = rbuf->size;
	}

	return 0;
}

static inline int mpq_dmx_notify_overflow(struct dvb_demux_feed *feed)
{
	struct dmx_data_ready data;

	data.data_length = 0;
	data.status = DMX_OVERRUN_ERROR;
	return feed->data_ready_cb.ts(&feed->feed.ts, &data);
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
	int cookie;

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

		mpq_dmx_update_decoder_stat(mpq_feed);

		/* Writing meta-data that includes the framing information */
		cookie = mpq_streambuffer_pkt_write(stream_buffer, &packet,
			(u8 *)&meta_data);
		if (cookie >= 0) {
			mpq_dmx_prepare_es_event_data(&packet, &meta_data,
				feed_data, stream_buffer, &data, cookie);
			feed->data_ready_cb.ts(&feed->feed.ts, &data);
		} else {
			MPQ_DVB_ERR_PRINT(
				"%s: mpq_streambuffer_pkt_write failed, ret=%d\n",
				__func__, cookie);
		}
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
	int cookie;

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
	if ((feed->pusi_seen) && (feed_data->pes_header_left_bytes == 0)) {
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

		mpq_dmx_update_decoder_stat(mpq_feed);

		cookie = mpq_streambuffer_pkt_write(stream_buffer, &packet,
			(u8 *)&meta_data);
		if (cookie >= 0) {
			/* Save write offset where new PES will begin */
			mpq_streambuffer_get_data_rw_offset(stream_buffer, NULL,
				&feed_data->frame_offset);
			mpq_dmx_prepare_es_event_data(&packet, &meta_data,
				feed_data, stream_buffer, &data, cookie);
			feed->data_ready_cb.ts(&feed->feed.ts, &data);
		} else {
			MPQ_DVB_ERR_PRINT(
				"%s: mpq_streambuffer_pkt_write failed, ret=%d\n",
				__func__, cookie);
		}
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
	struct dmx_framing_packet_info *framing;

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
			if (feed_data->pes_header_left_bytes != 0)
				MPQ_DVB_ERR_PRINT(
					"%s: received PUSI while handling PES header of previous PES\n",
					__func__);

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
		const struct ts_adaptation_field *adaptation_field =
			(const struct ts_adaptation_field *)(buf +
				ts_payload_offset);

		discontinuity_indicator =
			adaptation_field->discontinuity_indicator;
		ts_payload_offset += buf[ts_payload_offset] + 1;
	}

	bytes_avail = TS_PACKET_SIZE - ts_payload_offset;

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
					"%s: Found Sequence Pattern", __func__);
				MPQ_DVB_DBG_PRINT(
					"i = %d, offset = %d, type = %lld\n", i,
						framing_res.info[i].offset,
						framing_res.info[i].type);

				first_pattern = i;
				feed_data->found_sequence_header_pattern = 1;
				ts_payload_offset +=
					framing_res.info[i].offset;
				bytes_avail -= framing_res.info[i].offset;

				if (framing_res.info[i].used_prefix_size) {
					feed_data->first_prefix_size =
					 framing_res.info[i].used_prefix_size;
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
	mpq_demux->decoder_stat[feed_data->stream_interface].ts_errors +=
		ts_header->transport_error_indicator;
	mpq_dmx_check_continuity(feed_data,
				ts_header->continuity_counter,
				discontinuity_indicator);
	mpq_demux->decoder_stat[feed_data->stream_interface].cc_errors +=
		feed_data->continuity_errs;

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
		ret = mpq_streambuffer_data_write(stream_buffer,
			feed_data->patterns[0]->pattern,
			feed_data->first_prefix_size);
		if (ret < 0) {
			mpq_demux->decoder_stat
				[feed_data->stream_interface].drop_count +=
				feed_data->first_prefix_size;
			feed_data->ts_dropped_bytes +=
				feed_data->first_prefix_size;
			MPQ_DVB_DBG_PRINT("%s: could not write prefix\n",
				__func__);
			if (ret == -ENOSPC)
				mpq_dmx_notify_overflow(feed);
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
			/* Previous pending frame is in the same packet */
			bytes_to_write =
				framing_res.info[i].offset -
				feed_data->last_pattern_offset;
		}

		ret = mpq_streambuffer_data_write(
			stream_buffer,
			(buf + ts_payload_offset + bytes_written),
			bytes_to_write);
		if (ret < 0) {
			mpq_demux->decoder_stat
				[feed_data->stream_interface].drop_count +=
				bytes_to_write;
			feed_data->ts_dropped_bytes += bytes_to_write;
			MPQ_DVB_DBG_PRINT(
				"%s: Couldn't write %d bytes to data buffer, ret=%d\n",
				__func__, bytes_to_write, ret);
			if (ret == -ENOSPC)
				mpq_dmx_notify_overflow(feed);
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

			framing = &meta_data.info.framing;
			framing->pattern_type =
					feed_data->last_framing_match_type;
			framing->stc = feed_data->last_framing_match_stc;
			framing->continuity_error_counter =
					feed_data->continuity_errs;
			framing->transport_error_indicator_counter =
					feed_data->tei_errs;
			framing->ts_dropped_bytes =
					feed_data->ts_dropped_bytes;
			framing->ts_packets_num =
					feed_data->ts_packets_num;

			mpq_streambuffer_get_buffer_handle(
				stream_buffer,
				0,	/* current write buffer handle */
				&packet.raw_data_handle);

			mpq_dmx_update_decoder_stat(mpq_feed);

			/*
			 * Write meta-data that includes the framing information
			 */
			ret = mpq_streambuffer_pkt_write(stream_buffer, &packet,
				(u8 *)&meta_data);
			if (ret < 0) {
				MPQ_DVB_ERR_PRINT
					("%s: mpq_sb_pkt_write failed ret=%d\n",
					 __func__, ret);
				if (ret == -ENOSPC)
					mpq_dmx_notify_overflow(feed);
			} else {
				mpq_dmx_prepare_es_event_data(
					&packet, &meta_data, feed_data,
					stream_buffer, &data, ret);

				/* Trigger ES Data Event for VPTS */
				feed->data_ready_cb.ts(&feed->feed.ts, &data);

				if (feed_data->video_buffer->mode ==
					MPQ_STREAMBUFFER_BUFFER_MODE_LINEAR)
					feed_data->frame_offset = 0;
				else
					mpq_streambuffer_get_data_rw_offset(
						feed_data->video_buffer,
						NULL,
						&feed_data->frame_offset);
			}

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
					mpq_demux->decoder_stat
						[feed_data->stream_interface].
						drop_count += bytes_avail;
					feed_data->ts_dropped_bytes +=
					 framing_res.info[i].used_prefix_size;
					if (ret == -ENOSPC)
						mpq_dmx_notify_overflow(feed);
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
			buf + TS_PACKET_SIZE - DVB_DMX_MAX_PATTERN_LEN,
			DVB_DMX_MAX_PATTERN_LEN);

	if (pending_data_len) {
		ret = mpq_streambuffer_data_write(
			stream_buffer,
			(buf + ts_payload_offset + bytes_written),
			pending_data_len);

		if (ret < 0) {
			mpq_demux->decoder_stat
				[feed_data->stream_interface].drop_count +=
				pending_data_len;
			feed_data->ts_dropped_bytes += pending_data_len;
			MPQ_DVB_DBG_PRINT(
				"%s: Couldn't write %d pending bytes to data buffer, ret=%d\n",
				__func__, pending_data_len, ret);
			if (ret == -ENOSPC)
				mpq_dmx_notify_overflow(feed);
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
	int cookie;
	int ret;

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

			if (feed_data->pes_header_left_bytes == 0) {
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

				mpq_dmx_update_decoder_stat(mpq_feed);

				cookie = mpq_streambuffer_pkt_write(
					stream_buffer, &packet,
					(u8 *)&meta_data);
				if (cookie < 0) {
					MPQ_DVB_ERR_PRINT
						("%s: write failed, ret=%d\n",
						__func__, cookie);
				} else {
					/*
					 * Save write offset where new PES
					 * will begin
					 */
					mpq_streambuffer_get_data_rw_offset(
						stream_buffer,
						NULL,
						&feed_data->frame_offset);

					mpq_dmx_prepare_es_event_data(
						&packet, &meta_data,
						feed_data,
						stream_buffer, &data, cookie);

					feed->data_ready_cb.ts(&feed->feed.ts,
						&data);
				}
			} else {
				MPQ_DVB_ERR_PRINT(
					"%s: received PUSI while handling PES header of previous PES\n",
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
		const struct ts_adaptation_field *adaptation_field =
			(const struct ts_adaptation_field *)(buf +
				ts_payload_offset);

		discontinuity_indicator =
			adaptation_field->discontinuity_indicator;
		ts_payload_offset += buf[ts_payload_offset] + 1;
	}

	bytes_avail = TS_PACKET_SIZE - ts_payload_offset;

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
	mpq_demux->decoder_stat[feed_data->stream_interface].ts_errors +=
		ts_header->transport_error_indicator;
	mpq_dmx_check_continuity(feed_data,
				ts_header->continuity_counter,
				discontinuity_indicator);
	mpq_demux->decoder_stat[feed_data->stream_interface].cc_errors +=
		feed_data->continuity_errs;

	ret = mpq_streambuffer_data_write(stream_buffer, buf+ts_payload_offset,
		bytes_avail);
	if (ret < 0) {
		mpq_demux->decoder_stat
			[feed_data->stream_interface].drop_count += bytes_avail;
		feed_data->ts_dropped_bytes += bytes_avail;
		if (ret == -ENOSPC)
			mpq_dmx_notify_overflow(feed);
	} else {
		feed->peslen += bytes_avail;
	}

	spin_unlock(&feed_data->video_buffer_lock);

	return 0;
}

/* function ptr used in several places, handle differently */
int mpq_dmx_decoder_buffer_status(struct dvb_demux_feed *feed,
		struct dmx_buffer_status *dmx_buffer_status)
{

	if (dvb_dmx_is_video_feed(feed)) {
		struct mpq_demux *mpq_demux = feed->demux->priv;
		struct mpq_video_feed_info *feed_data;
		struct mpq_streambuffer *video_buff;
		struct mpq_feed *mpq_feed;

		mutex_lock(&mpq_demux->mutex);

		mpq_feed = feed->priv;
		feed_data = &mpq_feed->video_info;
		video_buff = feed_data->video_buffer;
		if (!video_buff) {
			mutex_unlock(&mpq_demux->mutex);
			return -EINVAL;
		}

		dmx_buffer_status->error = video_buff->raw_data.error;

		if (video_buff->mode == MPQ_STREAMBUFFER_BUFFER_MODE_LINEAR) {
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

	} else {
		MPQ_DVB_ERR_PRINT("%s: Invalid feed type %d\n",
				   __func__, feed->pes_type);
		return -EINVAL;
	}
	return 0;
}

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

int mpq_dmx_extract_pcr_and_dci(const u8 *buf, u64 *pcr, int *dci)
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

	if (mpq_dmx_extract_pcr_and_dci(buf, &data.pcr.pcr,
		&data.pcr.disc_indicator_set) == 0)
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

int mpq_dmx_decoder_eos_cmd(struct mpq_feed *mpq_feed)
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
	return (ret < 0) ? ret : 0;
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
	} else if (mpq_demux->demux.tsp_format == DMX_TSP_FORMAT_188) {
		pkt_format = SDMX_188_BYTE_PKT;
	} else if (mpq_demux->demux.tsp_format == DMX_TSP_FORMAT_192_TAIL) {
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
			__func__, ret);
		return ret;
	}

	MPQ_DVB_DBG_PRINT("%s: new session_handle = %d\n",
		__func__, mpq_demux->sdmx_session_handle);

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

static int mpq_sdmx_get_buffer_chunks(struct mpq_demux *mpq_demux,
	struct ion_dma_buff_info *buff_info,
	u32 actual_buff_size,
	struct sdmx_buff_descr buff_chunks[SDMX_MAX_PHYSICAL_CHUNKS])
{
	int i;
	struct sg_table *sg_ptr;
	struct scatterlist *sg;
	u32 chunk_size;
	int ret;

	memset(buff_chunks, 0,
		sizeof(struct sdmx_buff_descr) * SDMX_MAX_PHYSICAL_CHUNKS);

	sg_ptr = buff_info->sgt;
	if (IS_ERR_OR_NULL(sg_ptr)) {
		ret = PTR_ERR(sg_ptr);
		MPQ_DVB_ERR_PRINT("%s: ion_sg_table failed, ret=%d\n",
			__func__, ret);
		if (!ret)
			ret = -EINVAL;
		return ret;
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
		buff_chunks[i].base_addr = (u64)sg_dma_address(sg);

		if (sg->length > actual_buff_size)
			chunk_size = actual_buff_size;
		else
			chunk_size = sg->length;

		buff_chunks[i].size = chunk_size;
		sg = sg_next(sg);
		actual_buff_size -= chunk_size;
	}

	ret = mpq_sdmx_create_shm_bridge(buff_info->dmabuf, buff_info->sgt);
	if (ret) {
		MPQ_DVB_ERR_PRINT("%s mpq_sdmx_create_shm_bridge failed\n");
		return ret;
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
	struct ion_dma_buff_info *sdmx_buff;
	int ret;
	int i;

	*buf_mode = SDMX_RING_BUF;

	if (dvb_dmx_is_video_feed(feed->dvb_demux_feed)) {

		if (feed_data->buffer_desc.decoder_buffers_num > 1)
			*buf_mode = SDMX_LINEAR_GROUP_BUF;
		*num_buffers = feed_data->buffer_desc.decoder_buffers_num;

		MPQ_DVB_ERR_PRINT("%s: video feed case no of buffers=%zu\n",
				  __func__, *num_buffers);

		for (i = 0; i < *num_buffers; i++) {
			buf_desc[i].length =
				feed_data->buffer_desc.desc[i].size;

			ret = mpq_sdmx_get_buffer_chunks(mpq_demux,
				&feed_data->buffer_desc.buff_dma_info[i],
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
		sdmx_buff  = &feed->sdmx_dma_buff;

		buf_desc[0].length = buffer->size;
		buf_desc[0].buff_chunks[0].base_addr =
					feed->data_desc.phys_base;
		buf_desc[0].buff_chunks[0].size = feed->data_desc.size;
	} else {
		buffer = (struct dvb_ringbuffer *)
			dvbdmx_feed->feed.ts.buffer.ringbuff;

		sdmx_buff = &dvbdmx_feed->feed.ts.buffer.buff_dma_info;

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

	}
	return 0;
}

static int mpq_sdmx_filter_setup(struct mpq_demux *mpq_demux,
	struct dvb_demux_feed *dvbdmx_feed)
{
	int ret = 0;
	struct mpq_feed *feed;
	struct mpq_feed *main_rec_feed = NULL;
	struct dvb_demux_feed *tmp;
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
	if (feed->filter_type == SDMX_RAW_FILTER) {
		tmp = mpq_dmx_peer_rec_feed(dvbdmx_feed);
		if (tmp)
			main_rec_feed = tmp->priv;
	}

	/*
	 * If this PID is not part of existing recording filter,
	 * configure a new filter to SDMX.
	 */
	if (!main_rec_feed) {
		feed->secondary_feed = 0;

		MPQ_DVB_DBG_PRINT("%s: Adding new sdmx filter", __func__);
		MPQ_DVB_DBG_PRINT("%s: pid %d, flags=0x%X, ts_out_format=%d\n",
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
			"%s: filter pid=%d, handle=%d, data buffer(s)=%d, size=%d\n",
			__func__, dvbdmx_feed->pid,
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

	if (mutex_lock_interruptible(&mpq_demux->mutex))
		return -ERESTARTSYS;

	mpq_feed->sdmx_filter_handle = SDMX_INVALID_FILTER_HANDLE;

	if (feed->type != DMX_TYPE_SEC)
		feed->feed.ts.flush_buffer = mpq_dmx_flush_buffer;

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
		MPQ_DVB_ERR_PRINT(
			"	%s: init feed exit\n",
				__func__);
		goto init_mpq_feed_end;
	}
	MPQ_DVB_ERR_PRINT(
			"%s: mpq_sdmx_init_feed enter\n",
			__func__);
	MPQ_DVB_DBG_PRINT("%s: Init sdmx feed start\n", __func__);

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

static int mpq_sdmx_invalidate_buffer(struct mpq_feed *mpq_feed)
{
	struct dvb_demux_feed *feed = mpq_feed->dvb_demux_feed;
	struct dvb_ringbuffer *buffer;

	if (!dvb_dmx_is_video_feed(feed)) {
		if (dvb_dmx_is_sec_feed(feed) ||
			dvb_dmx_is_pcr_feed(feed)) {
			buffer = (struct dvb_ringbuffer *)
				&mpq_feed->sdmx_buf;
		} else {
			buffer = (struct dvb_ringbuffer *)
				feed->feed.ts.buffer.ringbuff;
		}
	}
	return 0;
}

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
					header->payload_length, 1);

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
			NULL, 0, &f->filter, &feed->buffer_flags);
	} else {
		int split = mpq_feed->sdmx_buf.size - mpq_feed->sdmx_buf.pread;

		feed->cb.sec(&mpq_feed->sdmx_buf.data[mpq_feed->sdmx_buf.pread],
			split,
			&mpq_feed->sdmx_buf.data[0],
			header->payload_length - split,
			&f->filter, &feed->buffer_flags);
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
		MPQ_DVB_DBG_PRINT("%s: Stalling for events and %zu bytes\n",
			__func__, req);

		mutex_unlock(&mpq_demux->mutex);

		ret = mpq_demux->demux.buffer_ctrl.ts(&feed->feed.ts, req, 1);
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

	if ((sts->metadata_fill_count == 0) &&
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
				"%s: metadata_fill_count is %d less than required %zu bytes\n",
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
		mpq_dmx_notify_overflow(feed);
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
				"%s: metadata_fill_count is %d less than required %zu bytes\n",
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
				"%s: metadata_fill_count is %d less than required %zu bytes\n",
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
				"%s: meta-data size %d larger than available meta-data %zd or max allowed %d\n",
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
		if (ts_header->adaptation_field_control == 1) {
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
		mpq_dmx_update_decoder_stat(mpq_feed);
		ret = mpq_streambuffer_pkt_write(sbuf, &packet,
			(u8 *)&meta_data);
		if (ret < 0) {
			MPQ_DVB_ERR_PRINT(
				"%s: mpq_streambuffer_pkt_write failed, ret=%d\n",
				__func__, ret);
		} else {
			mpq_dmx_prepare_es_event_data(
				&packet, &meta_data, &mpq_feed->video_info,
				sbuf, &data, ret);
			MPQ_DVB_DBG_PRINT("%s: Notify ES Event\n", __func__);
			feed->data_ready_cb.ts(&feed->feed.ts, &data);
		}

		spin_unlock(&mpq_feed->video_info.video_buffer_lock);
	}

decoder_filter_check_flags:
	if ((mpq_demux->demux.playback_mode == DMX_PB_MODE_PUSH) &&
		(sts->error_indicators & SDMX_FILTER_ERR_D_LIN_BUFS_FULL)) {
		MPQ_DVB_ERR_PRINT("%s: DMX_OVERRUN_ERROR\n", __func__);
		mpq_dmx_notify_overflow(mpq_feed->dvb_demux_feed);
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

	if (mpq_demux->demux.tsp_format == DMX_TSP_FORMAT_192_TAIL)
		stc_len = 4;

	mpq_feed->metadata_buf.pwrite = sts->metadata_write_offset;
	rbuff->pwrite = sts->data_write_offset;

	while (sts->metadata_fill_count) {
		bytes_avail = dvb_ringbuffer_avail(&mpq_feed->metadata_buf);
		if (bytes_avail < sizeof(header)) {
			MPQ_DVB_ERR_PRINT(
				"%s: metadata_fill_count is %d less than required %zu bytes\n",
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
		mpq_dmx_notify_overflow(feed);
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

		/* Invalidate output buffer before processing the results */
		if (!mpq_demux->disable_cache_ops)
			mpq_sdmx_invalidate_buffer(mpq_feed);

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
	ktime_t process_start_time;
	ktime_t process_end_time;

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

	process_start_time = ktime_get();

	prev_read_offset = read_offset;
	prev_fill_count = fill_count;
	sdmx_res = sdmx_process(mpq_demux->sdmx_session_handle, flags, input,
		&fill_count, &read_offset, &errors, &status,
		mpq_demux->sdmx_filter_count,
		mpq_demux->sdmx_filters_state.status);

	process_end_time = ktime_get();
	bytes_read = prev_fill_count - fill_count;

	mpq_dmx_update_sdmx_stat(mpq_demux, bytes_read,
			process_start_time, process_end_time);

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
		"%s: read_offset=%u, fill_count=%u, tsp_size=%zu\n",
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
		} else {
			/*
			 * ret < 0:	some error occurred
			 * ret == 0:	not enough data (less than 1 TS packet)
			 */
			if (ret < 0)
				MPQ_DVB_ERR_PRINT(
					"%s: mpq_sdmx_process_buffer failed, returned %d\n",
					__func__, ret);
			break;
		}
	}

	return total_bytes_read;
}

static int mpq_sdmx_write(struct mpq_demux *mpq_demux,
	struct ion_dma_buff_info *dvr_input_buff,
	const char *buf,
	size_t count)
{
	struct ion_dma_buff_info *buff;
	struct dvb_ringbuffer *rbuf;
	struct sdmx_buff_descr buf_desc;
	u32 read_offset;
	int ret;

	if (mpq_demux == NULL || dvr_input_buff == NULL) {
		MPQ_DVB_ERR_PRINT("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	buff = &mpq_demux->demux.dmx.dvr_input.buff_dma_info;
	rbuf = (struct dvb_ringbuffer *)mpq_demux->demux.dmx.dvr_input.ringbuff;

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
	return 0;
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
			&demux->dvr_input.buff_dma_info,
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

int mpq_sdmx_is_loaded(void)
{
	static int sdmx_load_checked;

	if (!sdmx_load_checked) {
		mpq_sdmx_check_app_loaded();
		sdmx_load_checked = 1;
	}

	return mpq_dmx_info.secure_demux_app_loaded;
}

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
