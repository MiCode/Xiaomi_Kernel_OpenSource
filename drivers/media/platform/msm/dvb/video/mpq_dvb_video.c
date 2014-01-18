/* Copyright (c) 2010-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/poll.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/dvb/video.h>
#include <linux/clk.h>
#include <linux/timer.h>
#include <linux/msm_iommu_domains.h>
#include <media/msm_vidc.h>
#include "dvbdev.h"
#include "mpq_adapter.h"
#include "mpq_stream_buffer.h"
#include "mpq_dvb_video_internal.h"
#define V4L2_PORT_CHANGE \
V4L2_EVENT_MSM_VIDC_PORT_SETTINGS_CHANGED_INSUFFICIENT
#define V4L2_PORT_REDO \
V4L2_EVENT_MSM_VIDC_PORT_SETTINGS_CHANGED_SUFFICIENT

struct smem_client {
	int mem_type;
	void *clnt;
	void *res;
};

static int mpq_dvb_input_data_handler(void *pparam);
static int mpq_dvb_vidc_event_handler(void *pparam);
static int mpq_dvb_demux_data_handler(void *pparam);
static int mpq_dvb_flush(struct mpq_dvb_video_instance *dvb_video_inst);
static int mpq_dvb_set_input_dmx_buf(
	struct v4l2_instance *v4l2_inst,
	struct video_data_buffer *pvideo_buf);

/*only enable error output to kernel msg*/
int mpq_debug = 0x00001;
static int default_height = 1088;
static int default_width = 1920;
static int default_input_buf_size = 1024*1024;
static int default_buf_num_for_ringbuf = 4;
static struct mpq_dvb_video_device *dvb_video_device;

static char vid_thread_names[DVB_MPQ_NUM_VIDEO_DEVICES][10] = {
				"dvb-vid-0",
				"dvb-vid-1",
				"dvb-vid-2",
				"dvb-vid-3",
};

static char vid_data_thread_names[DVB_MPQ_NUM_VIDEO_DEVICES][10] = {
				"dvb-dat-0",
				"dvb-dat-1",
				"dvb-dat-2",
				"dvb-dat-3",
};

static char vid_dmx_thread_names[DVB_MPQ_NUM_VIDEO_DEVICES][10] = {
				"dvb-dmx-0",
				"dvb-dmx-1",
				"dvb-dmx-2",
				"dvb-dmx-3",
};

static int v4l2_sub_event[] = {
	V4L2_EVENT_MSM_VIDC_FLUSH_DONE,
	V4L2_EVENT_MSM_VIDC_PORT_SETTINGS_CHANGED_SUFFICIENT,
	V4L2_EVENT_MSM_VIDC_PORT_SETTINGS_CHANGED_INSUFFICIENT,
	V4L2_EVENT_MSM_VIDC_CLOSE_DONE,
	V4L2_EVENT_MSM_VIDC_SYS_ERROR
};

#define V4L2_MPEG_VIDC_EXTRADATA_MB_QUANTIZATION_BIT 0x00000001
#define V4L2_MPEG_VIDC_EXTRADATA_INTERLACE_VIDEO_BIT 0x00000002
#define V4L2_MPEG_VIDC_EXTRADATA_VC1_FRAMEDISP_BIT 0x00000004
#define V4L2_MPEG_VIDC_EXTRADATA_VC1_SEQDISP_BIT 0x00000008
#define V4L2_MPEG_VIDC_EXTRADATA_TIMESTAMP_BIT 0x00000010
#define V4L2_MPEG_VIDC_EXTRADATA_S3D_FRAME_PACKING_BIT 0x00000020
#define V4L2_MPEG_VIDC_EXTRADATA_FRAME_RATE_BIT	0x00000040
#define V4L2_MPEG_VIDC_EXTRADATA_PANSCAN_WINDOW_BIT	0x00000080
#define V4L2_MPEG_VIDC_EXTRADATA_RECOVERY_POINT_SEI_BIT	0x00000100
#define V4L2_MPEG_VIDC_EXTRADATA_CLOSED_CAPTION_UD_BIT 0x00000200
#define V4L2_MPEG_VIDC_EXTRADATA_AFD_UD_BIT	0x00000400
#define V4L2_MPEG_VIDC_EXTRADATA_MULTISLICE_INFO_BIT 0x00000800
#define V4L2_MPEG_VIDC_EXTRADATA_NUM_CONCEALED_MB_BIT 0x00001000
#define V4L2_MPEG_VIDC_EXTRADATA_METADATA_FILLER_BIT 0x00002000
#define V4L2_MPEG_VIDC_EXTRADATA_INPUT_CROP_BIT 0x00004000
#define V4L2_MPEG_VIDC_EXTRADATA_DIGITAL_ZOOM_BIT	0x00008000
#define V4L2_MPEG_VIDC_EXTRADATA_ASPECT_RATIO_BIT	0x00010000
#define V4L2_MPEG_VIDC_EXTRADATA_MPEG2_SEQDISP_BIT 0x00020000

struct v4l2_extradata_type {
	int type;
	int mask;
};

static struct v4l2_extradata_type mpq_v4l2_extradata[] = {
{
V4L2_MPEG_VIDC_EXTRADATA_MB_QUANTIZATION,
V4L2_MPEG_VIDC_EXTRADATA_MB_QUANTIZATION_BIT
},
{
V4L2_MPEG_VIDC_EXTRADATA_INTERLACE_VIDEO,
V4L2_MPEG_VIDC_EXTRADATA_INTERLACE_VIDEO_BIT
},
{
V4L2_MPEG_VIDC_EXTRADATA_VC1_FRAMEDISP,
V4L2_MPEG_VIDC_EXTRADATA_VC1_FRAMEDISP_BIT
},
{
V4L2_MPEG_VIDC_EXTRADATA_VC1_SEQDISP,
V4L2_MPEG_VIDC_EXTRADATA_VC1_SEQDISP_BIT
},
{

V4L2_MPEG_VIDC_EXTRADATA_TIMESTAMP,
V4L2_MPEG_VIDC_EXTRADATA_TIMESTAMP_BIT
},
{
V4L2_MPEG_VIDC_EXTRADATA_S3D_FRAME_PACKING,
V4L2_MPEG_VIDC_EXTRADATA_S3D_FRAME_PACKING_BIT
},
{
V4L2_MPEG_VIDC_EXTRADATA_FRAME_RATE,
V4L2_MPEG_VIDC_EXTRADATA_FRAME_RATE_BIT
},
{
V4L2_MPEG_VIDC_EXTRADATA_PANSCAN_WINDOW,
V4L2_MPEG_VIDC_EXTRADATA_PANSCAN_WINDOW_BIT
},
{
V4L2_MPEG_VIDC_EXTRADATA_RECOVERY_POINT_SEI,
V4L2_MPEG_VIDC_EXTRADATA_RECOVERY_POINT_SEI_BIT
},
/*
{
V4L2_MPEG_VIDC_EXTRADATA_CLOSED_CAPTION_UD,
V4L2_MPEG_VIDC_EXTRADATA_CLOSED_CAPTION_UD_BIT
},
{
V4L2_MPEG_VIDC_EXTRADATA_AFD_UD,
V4L2_MPEG_VIDC_EXTRADATA_AFD_UD_BIT
},
*/
{
V4L2_MPEG_VIDC_EXTRADATA_MULTISLICE_INFO,
V4L2_MPEG_VIDC_EXTRADATA_MULTISLICE_INFO_BIT
},
{
V4L2_MPEG_VIDC_EXTRADATA_NUM_CONCEALED_MB,
V4L2_MPEG_VIDC_EXTRADATA_NUM_CONCEALED_MB_BIT
},
{
V4L2_MPEG_VIDC_EXTRADATA_METADATA_FILLER,
V4L2_MPEG_VIDC_EXTRADATA_METADATA_FILLER_BIT
},
{
V4L2_MPEG_VIDC_EXTRADATA_INPUT_CROP,
V4L2_MPEG_VIDC_EXTRADATA_INPUT_CROP_BIT
},
{
V4L2_MPEG_VIDC_EXTRADATA_DIGITAL_ZOOM,
V4L2_MPEG_VIDC_EXTRADATA_DIGITAL_ZOOM_BIT
},
{
V4L2_MPEG_VIDC_EXTRADATA_ASPECT_RATIO,
V4L2_MPEG_VIDC_EXTRADATA_ASPECT_RATIO_BIT
},
{
V4L2_MPEG_VIDC_EXTRADATA_MPEG2_SEQDISP,
V4L2_MPEG_VIDC_EXTRADATA_MPEG2_SEQDISP_BIT
}
};

static int mpq_map_kernel(void *hndl, struct msm_smem *mem)
{
	struct ion_handle *handle = (struct ion_handle *)mem->smem_priv;
	struct smem_client *client = (struct smem_client *)hndl;
	if (client && client->clnt) {
		mem->kvaddr = ion_map_kernel(client->clnt, handle);
		if (!(mem->kvaddr))
			return -EFAULT;
		DBG("Map Kernel address from(hnd:%u) to %u",
			(u32)handle, (u32)mem->kvaddr);
		return 0;
	}
	return -EINVAL;
}

static int mpq_unmap_kernel(void *hndl, struct msm_smem *mem)
{
	struct ion_handle *handle = (struct ion_handle *)mem->smem_priv;
	struct smem_client *client = (struct smem_client *)hndl;
	if (client && client->clnt)
		ion_unmap_kernel(client->clnt, handle);
		return 0;

	return -EINVAL;
}

static int mpq_ring_buffer_create(
	struct mpq_ring_buffer **ppringbuf,
	struct buffer_info binfo)
{
	struct mpq_ring_buffer *pbuf;
	DBG("ENTER mpq_ring_buffer_create\n");
	if (binfo.index != 0 || binfo.offset != 0)
		return -EINVAL;
	pbuf = kzalloc(sizeof(struct mpq_ring_buffer), GFP_KERNEL);
	if (!pbuf)
		return -ENOMEM;
	pbuf->buf = binfo;
	pbuf->len = binfo.size;
	pbuf->write_idx = 0;
	pbuf->read_idx = 0;
	pbuf->release_idx = 0;
	pbuf->flush_buffer = 0;

	sema_init(&(pbuf->sem), 1);
	init_waitqueue_head(&(pbuf->write_wait));
	init_waitqueue_head(&(pbuf->read_wait));
	*ppringbuf = pbuf;
	DBG("LEAVE mpq_ring_buffer_create\n");
	return 0;
}

static int mpq_ring_buffer_delete(
	struct mpq_ring_buffer *pbuf)
{
	DBG("ENTER mpq_ring_buffer_delete\n");
	kfree(pbuf);
	DBG("LEAVE mpq_ring_buffer_delete\n");
	return 0;
}

static int mpq_ring_buffer_flush(
	struct mpq_ring_buffer *pbuf)
{
	DBG("ENTER mpq_ring_buffer_flush\n");
	if (!pbuf) {
		ERR("Invalid Ring Buffer\n");
		return -EINVAL;
	}
	if (down_interruptible(&pbuf->sem))
		return -ERESTARTSYS;
	pbuf->flush_buffer = 1;
	up(&pbuf->sem);
	wake_up(&pbuf->write_wait);
	wake_up(&pbuf->read_wait);
	DBG("LEAVE mpq_ring_buffer_flush\n");
	return 0;
}

static int mpq_ring_buffer_reset(
struct mpq_ring_buffer *pbuf)
{
	DBG("ENTER mpq_ring_buffer_reset\n");
	if (!pbuf) {
		ERR("Invalid Ring Buffer\n");
		return -EINVAL;
	}
	if (down_interruptible(&pbuf->sem))
		return -ERESTARTSYS;
	pbuf->read_idx = 0;
	pbuf->release_idx = 0;
	pbuf->write_idx = 0;
	pbuf->flush_buffer = 0;
	up(&pbuf->sem);
	DBG("LEAVE mpq_ring_buffer_reset\n");
	return 0;
}

static inline size_t mpq_ring_buffer_get_write_size(
	struct mpq_ring_buffer *pbuf)
{
	u32 free_data_size = 0;
	if (pbuf->write_idx == pbuf->release_idx)
		free_data_size = pbuf->len;
	if (pbuf->write_idx > pbuf->release_idx)
		free_data_size =
			pbuf->len - (pbuf->write_idx - pbuf->release_idx);
	if (pbuf->write_idx < pbuf->release_idx)
		free_data_size = pbuf->release_idx - pbuf->write_idx;
	if (free_data_size > SAFE_GAP)
		free_data_size = free_data_size - SAFE_GAP;
	else
		free_data_size = 0;
	return free_data_size;
}

static inline size_t mpq_ring_buffer_get_read_size(
	struct mpq_ring_buffer *pbuf)
{
	u32 data_size;
	if (pbuf->write_idx == pbuf->read_idx)
		data_size = 0;
	if (pbuf->write_idx > pbuf->read_idx)
		data_size = pbuf->write_idx - pbuf->read_idx;
	if (pbuf->write_idx < pbuf->read_idx)
		data_size = pbuf->write_idx + pbuf->len - pbuf->read_idx;
	return data_size;
}

static inline size_t mpq_ring_buffer_get_release_size(
	struct mpq_ring_buffer *pbuf)
{
	u32 data_size;
	if (pbuf->release_idx == pbuf->read_idx)
		data_size = 0;
	if (pbuf->release_idx < pbuf->read_idx)
		data_size = pbuf->read_idx - pbuf->release_idx;
	if (pbuf->release_idx > pbuf->read_idx)
		data_size = pbuf->read_idx + pbuf->len - pbuf->release_idx;
	return data_size;
}

static int mpq_ring_buffer_release(
	struct mpq_ring_buffer *pbuf,
	size_t release_offset)
{
	size_t release_size;
	DBG("ENTER mpq_ring_buffer_release\n");
	if (down_interruptible(&pbuf->sem))
		return -ERESTARTSYS;
	release_offset %= pbuf->len;
	if (release_offset > pbuf->len)
		goto release_err;
	if (release_offset > pbuf->release_idx)
		release_size = release_offset - pbuf->release_idx;
	if (release_offset < pbuf->release_idx)
		release_size = release_offset + pbuf->len - pbuf->release_idx;
	if (release_offset == pbuf->release_idx)
		release_size = 0;
	pbuf->release_idx = release_offset % pbuf->len;
	INF("RINGBUFFFER(len %d) Status write:%d read:%d release:%d\n",
		pbuf->len, pbuf->write_idx, pbuf->read_idx, pbuf->release_idx);
	up(&pbuf->sem);
	wake_up(&pbuf->write_wait);
	DBG("LEAVE mpq_ring_buffer_release\n");
	return 0;
release_err:
	INF("RINGBUFFFER(len %d) Status write:%d read:%d release:%d\n",
		pbuf->len, pbuf->write_idx, pbuf->read_idx, pbuf->release_idx);
	ERR("RINGBUFFER Release Error: %d\n", release_offset);
	up(&pbuf->sem);
	return -EINVAL;
}

static int mpq_ring_buffer_write(
	struct mpq_ring_buffer *pbuf,
	const u8 *wr_buf,
	size_t wr_buf_size)
{
	/*sanity check*/
	int tm_jiffies = 32;
	int rc;
	DBG("ENTER mpq_ring_buffer_write\n");
	if (wr_buf == NULL || wr_buf_size > pbuf->len)
		return -EINVAL;
	if (pbuf->flush_buffer)
		return 0;
	if (down_interruptible(&pbuf->sem))
		return -ERESTARTSYS;
	while (mpq_ring_buffer_get_write_size(pbuf) < wr_buf_size) {
		up(&pbuf->sem);
		rc = wait_event_interruptible_timeout(pbuf->write_wait,
			mpq_ring_buffer_get_write_size(pbuf) >= wr_buf_size ||
			pbuf->flush_buffer,
			tm_jiffies);
		if (rc < 0)
			return -ERESTARTSYS;
		if (pbuf->flush_buffer)
			return 0;
		if (down_interruptible(&pbuf->sem))
			return -ERESTARTSYS;
		if (rc == 0)
			continue;
	}
	/*
	 * sem locked
	 * handle wrap around case
	 */
	if ((pbuf->write_idx + wr_buf_size) >= pbuf->len) {
		size_t wr_sz1 = pbuf->len - pbuf->write_idx;
		size_t wr_sz2 = wr_buf_size - wr_sz1;
		if (copy_from_user(
		   (u8 *)pbuf->buf.kernel_vaddr + pbuf->write_idx,
			wr_buf, wr_sz1))
			goto copy_err;
		if (copy_from_user(
		   (u8 *)pbuf->buf.kernel_vaddr,
			wr_buf + wr_sz1, wr_sz2))
			goto copy_err;
		pbuf->write_idx = wr_sz2;
	} else {
		if (copy_from_user(
		   (u8 *)pbuf->buf.kernel_vaddr + pbuf->write_idx,
			wr_buf, wr_buf_size))
			goto copy_err;
		pbuf->write_idx += wr_buf_size;
	}
	INF("RINGBUFFFER(len %d) Status write:%d read:%d release:%d\n",
		pbuf->len, pbuf->write_idx, pbuf->read_idx, pbuf->release_idx);
	up(&pbuf->sem);
	wake_up(&pbuf->read_wait);
	DBG("LEAVE mpq_ring_buffer_write\n");
	return 0;
copy_err:
	up(&pbuf->sem);
	return -EFAULT;
}

static int mpq_ring_buffer_read_v4l2(
	struct mpq_ring_buffer *pbuf,
	struct buffer_info *pbinfo,
	size_t buf_size)
{
	int timeout_jiffies = 32;
	int rc;
	if (pbinfo == NULL || buf_size > pbuf->len)
		return -EINVAL;
	DBG("ENTER mpq_ring_buffer_read_v4l2()\n");
	if (pbuf->flush_buffer)
		return 0;
	if (down_interruptible(&pbuf->sem))
		return -ERESTARTSYS;
	while (!mpq_ring_buffer_get_read_size(pbuf)) {
		up(&pbuf->sem);
		rc = wait_event_interruptible_timeout(
		   pbuf->read_wait,
			mpq_ring_buffer_get_read_size(pbuf) ||
		   pbuf->flush_buffer ||
		   kthread_should_stop(),
			timeout_jiffies);
		if (rc < 0)
			return -ERESTARTSYS;
		if (kthread_should_stop())
			return -ERESTARTSYS;
		if (pbuf->flush_buffer)
			return 0;
		/* TIMEOUT Queue the empty buffer to VIDC anyway*/
		if (rc == 0) {
			INF("HXU_DEBUG: Queue EMPTY ETB to vidc\n");
			pbinfo->offset = pbuf->read_idx;
			pbinfo->bytesused = 0;
			return 0;
		}
		if (down_interruptible(&pbuf->sem))
			return -ERESTARTSYS;
	}
	if (mpq_ring_buffer_get_read_size(pbuf) < buf_size)
		buf_size = mpq_ring_buffer_get_read_size(pbuf);
	pbinfo->offset = pbuf->read_idx;
	pbinfo->bytesused = buf_size;
	if ((pbuf->read_idx + buf_size) >= pbuf->len)
		pbuf->read_idx = pbuf->read_idx + buf_size - pbuf->len;
	else
		pbuf->read_idx += buf_size;
	INF("RINGBUFFFER(len %d) Status write:%d read:%d release:%d\n",
		pbuf->len, pbuf->write_idx, pbuf->read_idx, pbuf->release_idx);
	up(&pbuf->sem);
	DBG("LEAVE mpq_ring_buffer_read_v4l2()\n");
	return buf_size;
}

static int mpq_ring_buffer_read_user(
	struct mpq_ring_buffer *pbuf,
	u8 *p_user_buf,
	size_t buf_size)
{
	size_t rd_sz1;
	size_t rd_sz2;
	int rc;
	DBG("ENTER mpq_ring_buffer_read_user\n");
	if (pbuf == NULL || buf_size > pbuf->len)
		return -EINVAL;
	if (pbuf->flush_buffer)
		return 0;
	if (down_interruptible(&pbuf->sem))
		return -ERESTARTSYS;
	while (mpq_ring_buffer_get_read_size(pbuf) < buf_size) {
		up(&pbuf->sem);
		if (wait_event_interruptible(
		   pbuf->read_wait,
			mpq_ring_buffer_get_read_size(pbuf) >= buf_size))
			return -ERESTARTSYS;
		if (down_interruptible(&pbuf->sem))
			return -ERESTARTSYS;
	}
	if ((pbuf->read_idx + buf_size) >= pbuf->len) {
		rd_sz1 = pbuf->len - pbuf->read_idx;
		rd_sz2 = buf_size - rd_sz1;
		rc = copy_to_user(
		   p_user_buf,
			(u8 *)pbuf->buf.kernel_vaddr + pbuf->read_idx,
			rd_sz1);
		if (rc)
			return -EINVAL;
		rc = copy_to_user(p_user_buf + rd_sz1,
						  (u8 *)pbuf->buf.kernel_vaddr,
						  rd_sz2);
		if (rc)
			return -EINVAL;
		pbuf->read_idx = rd_sz2;
	} else {
		rc = copy_to_user(
		   p_user_buf,
			(u8 *)pbuf->buf.kernel_vaddr + pbuf->read_idx,
			buf_size);
		if (rc)
			return -EINVAL;
		pbuf->read_idx += buf_size;
	}
	INF("RINGBUFFFER(len %d) Status write:%d read:%d release:%d\n",
		pbuf->len, pbuf->write_idx, pbuf->read_idx, pbuf->release_idx);
	up(&pbuf->sem);
	mpq_ring_buffer_release(pbuf, pbuf->read_idx);
	DBG("LEAVE mpq_ring_buffer_read_user\n");
	return buf_size;
}

static void mpq_dvb_get_stream_if(
	enum mpq_adapter_stream_if interface_id,
	void *arg)
{
	struct mpq_dvb_video_instance *dvb_video_inst = arg;
	struct mpq_streambuffer *sbuff;
	struct mpq_dmx_source *dmx_data;
	DBG("In mpq_dvb_video_get_stream_if : %d\n", interface_id);
	if (!dvb_video_inst || !dvb_video_inst->dmx_src_data) {
		ERR("Passed NULL pointer!!\n");
		return;
	}
	dmx_data = dvb_video_inst->dmx_src_data;


	mpq_adapter_get_stream_if(interface_id,
			&dvb_video_inst->dmx_src_data->stream_buffer);


	DBG("Receive StreamBuffer from Adapter card:%u\n",
		(u32)dmx_data->stream_buffer);
	sbuff = dmx_data->stream_buffer;
	if (!sbuff) {
		ERR("Receive NULL stream buffer\n");
		return;
	}
	DBG("RAW DATA Size:%u and Pkt Data Size: %u\n",
		(u32)sbuff->raw_data.size,
		(u32)sbuff->packet_data.size);
	DBG("BUFFER number:%u and buffer_desc: %u; handle:%d base:%u\n",
		(u32)sbuff->buffers_num,
		(u32)sbuff->buffers[0].size,
		sbuff->buffers[0].handle,
		(u32)sbuff->buffers[0].base);
		dmx_data->dmx_video_buf.bufferaddr = sbuff->buffers[0].base;
		dmx_data->dmx_video_buf.buffer_len = sbuff->buffers[0].size;
		dmx_data->dmx_video_buf.ion_fd = sbuff->buffers[0].handle;
		dmx_data->dmx_video_buf.offset = 0;
		dmx_data->dmx_video_buf.mmaped_size = sbuff->buffers[0].size;
		DBG("HXU_DEBUG: dmx_buf fd:%d size:%u\n",
			dmx_data->dmx_video_buf.ion_fd,
			dmx_data->dmx_video_buf.buffer_len);
	wake_up(&dvb_video_inst->dmx_src_data->dmx_wait);
}

static int mpq_dvb_init_dmx_src(
	struct mpq_dvb_video_instance *dvb_video_inst,
	int device_id)
{
	int rc = 0;
	struct mpq_dmx_source *dmx_data;
	DBG("ENTER mpq_dvb_init_dmx_src\n");
	dvb_video_inst->dmx_src_data =  kzalloc(sizeof(struct mpq_dmx_source),
						GFP_KERNEL);
	if (dvb_video_inst->dmx_src_data == NULL) {
		ERR("Error allocate memory\n");
		return -ENOMEM;
	}
	dmx_data = dvb_video_inst->dmx_src_data;
	dvb_video_inst->dmx_src_data->device_id = device_id;
	init_waitqueue_head(&dvb_video_inst->dmx_src_data->dmx_wait);
	INIT_LIST_HEAD(&dvb_video_inst->dmx_src_data->pkt_queue);
	sema_init(&dvb_video_inst->dmx_src_data->pkt_sem, 1);
	init_waitqueue_head(&dvb_video_inst->dmx_src_data->pkt_wait);
	rc = mpq_adapter_get_stream_if(
		(enum mpq_adapter_stream_if)dmx_data->device_id,
		&dmx_data->stream_buffer);

	if (rc) {
		ERR("Error in mpq_adapter_get_stream_if()");
		return -ENODEV;
	} else if (dmx_data->stream_buffer == NULL) {
		DBG("Stream Buffer is NULL. Resigtering Notifier.\n");
		rc = mpq_adapter_notify_stream_if(
			(enum mpq_adapter_stream_if)dmx_data->device_id,
			mpq_dvb_get_stream_if,
			(void *)dvb_video_inst);
		if (rc)
			return -ENODEV;
	} else {
		struct mpq_streambuffer *sbuff = dmx_data->stream_buffer;
		DBG("Receive StreamBuffer from Adapter card:%u\n",
			(u32)dmx_data->stream_buffer);
		DBG("RAW DATA Size:%u and Pkt Data Size: %u\n",
			(u32)sbuff->raw_data.size,
			(u32)sbuff->packet_data.size);
		DBG("BUFFER number:%u and buffer_desc: %u; handle:%d base:%u\n",
			(u32)sbuff->buffers_num,
			(u32)sbuff->buffers[0].size,
			sbuff->buffers[0].handle,
			(u32)sbuff->buffers[0].base);
		dmx_data->dmx_video_buf.bufferaddr = sbuff->buffers[0].base;
		dmx_data->dmx_video_buf.buffer_len = sbuff->buffers[0].size;
		dmx_data->dmx_video_buf.ion_fd = sbuff->buffers[0].handle;
		dmx_data->dmx_video_buf.offset = 0;
		dmx_data->dmx_video_buf.mmaped_size = sbuff->buffers[0].size;
		DBG("HXU_DEBUG: dmx_buf fd:%d size:%u\n",
			dmx_data->dmx_video_buf.ion_fd,
			dmx_data->dmx_video_buf.buffer_len);
	}
	DBG("LEAVE mpq_dvb_init_dmx_src\n");
	return rc;
}

static int mpq_dvb_start_dmx_src(
	struct mpq_dvb_video_instance *dvb_video_inst)
{
	struct mpq_dmx_source *dmx_data = dvb_video_inst->dmx_src_data;
	DBG("ENTER mpq_dvb_start_dmx_src\n");
	if (NULL == dmx_data)
		return 0;
	dvb_video_inst->demux_task = kthread_run(
			mpq_dvb_demux_data_handler,	(void *)dvb_video_inst,
			vid_dmx_thread_names[dmx_data->device_id]);
	DBG("LEAVE mpq_dvb_start_dmx_src\n");
	return 0;
}

static int mpq_dvb_stop_dmx_src(
	struct mpq_dvb_video_instance *dvb_video_inst)
{
	struct mpq_dmx_source *dmx_data = dvb_video_inst->dmx_src_data;
	if (NULL == dmx_data)
		return 0;
	if (dvb_video_inst->demux_task) {
		kthread_stop(dvb_video_inst->demux_task);
		dvb_video_inst->demux_task = NULL;
	}
	return 0;
}

static int mpq_dvb_term_dmx_src(
	struct mpq_dvb_video_instance *dvb_video_inst)
{
	struct mpq_dmx_source *dmx_data = dvb_video_inst->dmx_src_data;
	if (NULL == dmx_data)
		return 0;
	if (dvb_video_inst->demux_task)
		kthread_stop(dvb_video_inst->demux_task);
	kfree(dmx_data);
	dvb_video_inst->dmx_src_data = NULL;
	return 0;
}

/*
 *	V4L2 VIDC Driver API Wrapper Implementation
 */
static int mpq_v4l2_queue_input_buffer(
	struct v4l2_instance *v4l2_inst,
	struct buffer_info *pbinfo)
{
	int rc;
	struct v4l2_buffer buf;
	struct v4l2_plane plane[VIDEO_MAX_PLANES];
	u32 lo_pts;
	u32 hi_pts;
	if (!v4l2_inst || !pbinfo)
		return -EINVAL;
	DBG("ENTER mpq_v4l2_queue_input_buffer [%d]\n", pbinfo->index);
	memset(&buf, 0, sizeof(buf));
	memset(&plane, 0, sizeof(plane));
	INF("Queue Input Buf[%d]\n", pbinfo->index);
	buf.index = pbinfo->index;
	buf.type = pbinfo->buf_type;
	buf.memory = V4L2_MEMORY_USERPTR;
	buf.length = 1;
	lo_pts = pbinfo->pts & 0xffffffff;
	hi_pts = pbinfo->pts >> 32;
	buf.timestamp.tv_sec = lo_pts/90000;
	buf.timestamp.tv_usec = (lo_pts % 90000)*100/9;
	DBG("Queue Buffer with timestamp %u-%u\n",
		(u32)buf.timestamp.tv_sec,
		(u32)buf.timestamp.tv_usec/1000);
	plane[0].bytesused = pbinfo->bytesused;
	plane[0].length = pbinfo->size;
	plane[0].m.userptr = (unsigned long)pbinfo->dev_addr;
	plane[0].reserved[0] = pbinfo->fd;
	plane[0].reserved[1] = 0;
	plane[0].data_offset = pbinfo->offset;
	buf.m.planes = plane;
	if (pbinfo->handle) {
		rc = msm_vidc_smem_cache_operations(v4l2_inst->vidc_inst,
			pbinfo->handle, SMEM_CACHE_CLEAN);
		if (rc)
			ERR("[%s] Failed to clean caches: %d\n", __func__, rc);
	}
	rc = msm_vidc_qbuf(v4l2_inst->vidc_inst, &buf);
	if (!rc)
		v4l2_inst->vidc_etb++;
	if (rc)
		ERR("[%s]msm_vidc_qbuf error\n", __func__);
	DBG("LEAVE mpq_v4l2_queue_input_buffer\n");
	return rc;
}

static int mpq_v4l2_queue_output_buffer(
	struct v4l2_instance *v4l2_inst,
	struct buffer_info *pbinfo)
{
	int rc;
	int extra_idx;
	struct v4l2_buffer buf;
	struct v4l2_plane plane[VIDEO_MAX_PLANES];

	if (!v4l2_inst || !pbinfo)
		return -EINVAL;
	DBG("ENTER mpq_v4l2_queue_output_buffer [%d]\n", pbinfo->index);
	memset(&buf, 0, sizeof(buf));
	memset(&plane, 0, sizeof(plane));
	buf.index = pbinfo->index;
	buf.type = pbinfo->buf_type;
	buf.memory = V4L2_MEMORY_USERPTR;
	plane[0].bytesused = 0;
	plane[0].length = pbinfo->size;
	plane[0].m.userptr = (unsigned long)pbinfo->dev_addr;
	plane[0].reserved[0] = pbinfo->fd;
	plane[0].reserved[1] = 0;
	plane[0].data_offset = pbinfo->offset;
	if (v4l2_inst->fmt[CAPTURE_PORT].fmt.pix_mp.num_planes > 1) {
		extra_idx = EXTRADATA_IDX(
			v4l2_inst->fmt[CAPTURE_PORT].fmt.pix_mp.num_planes);
		if (v4l2_inst->extradata_handle) {
			plane[extra_idx].length = pbinfo->extradata.length;
			plane[extra_idx].m.userptr =
				v4l2_inst->extradata_handle->device_addr +
				buf.index * pbinfo->extradata.length;
			plane[extra_idx].reserved[0] =
				pbinfo->extradata.ion_fd;
			plane[extra_idx].reserved[1] =
				pbinfo->extradata.fd_offset;
			plane[extra_idx].data_offset = 0;
			DBG("EX:[%d]-[%d]:addr:%u len:%u fd:%d off:%d\n",
				buf.index,
				extra_idx,
				(u32)plane[extra_idx].m.userptr,
				(u32)plane[extra_idx].length,
				(int)plane[extra_idx].reserved[0],
				(int)plane[extra_idx].reserved[1]);
		} else {
			plane[extra_idx].length = 0;
			plane[extra_idx].reserved[0] = 0;
			plane[extra_idx].reserved[1] = 0;
			plane[extra_idx].data_offset = 0;
		}
		buf.length = v4l2_inst->fmt[CAPTURE_PORT].fmt.pix_mp.num_planes;
	} else {
		buf.length = 1;
	}
	buf.m.planes = plane;

	rc = msm_vidc_qbuf(v4l2_inst->vidc_inst, &buf);
	if (!rc)
		v4l2_inst->vidc_ftb++;
	if (rc)
		ERR("[%s]msm_vidc_qbuf error\n", __func__);
	DBG("LEAVE mpq_v4l2_queue_output_buffer\n");
	return rc;
}

static int mpq_v4l2_deque_input_buffer(
	struct mpq_dvb_video_instance *dvb_video_inst,
	int *p_buf_index)
{
	struct v4l2_instance *v4l2_inst;
	struct v4l2_buffer buf;
	struct v4l2_plane plane[VIDEO_MAX_PLANES];
	int rc = 0;
	u32 release_offset = 0;
	v4l2_inst = dvb_video_inst->v4l2_inst;
	if (!v4l2_inst)
		return -EINVAL;
	*p_buf_index = -1;
	memset(&buf, 0, sizeof(buf));
	memset(&plane, 0, sizeof(plane));
	buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	buf.memory = V4L2_MEMORY_USERPTR;
	buf.m.planes = plane;
	buf.length = 1;
	rc = msm_vidc_dqbuf(v4l2_inst->vidc_inst, &buf);
	if (!rc)
		v4l2_inst->vidc_ebd++;
	if (rc)
		return rc;
	if (v4l2_inst->input_mode == INPUT_MODE_RING) {
		release_offset = buf.m.planes[0].data_offset;
		INF("dequeue input buffer[%d] with data offset %u\n",
			buf.index,  release_offset);
		if (dvb_video_inst->source == VIDEO_SOURCE_DEMUX) {
			u32 rd_offset;
			u32 wr_offset;
			u32 skip_len;
			struct mpq_streambuffer *sbuff;
			struct mpq_dmx_source *dmx_data;
			dmx_data = dvb_video_inst->dmx_src_data;
			if (!dmx_data) {
				ERR("NULL pointer for DMX source\n");
				return -EINVAL;
			}
			sbuff = dmx_data->stream_buffer;
			if (!sbuff) {
				ERR("NULL pointer for DMX stream buffer\n");
				return -EINVAL;
			}
			rc = mpq_streambuffer_get_data_rw_offset(
			   sbuff, &rd_offset, &wr_offset);
			if (rc) {
				ERR("Error get offset from Demux\n");
				return rc;
			}
			if (dmx_data->dmx_video_buf.buffer_len) {
				release_offset = release_offset %
					dmx_data->dmx_video_buf.buffer_len;
			} else {
				ERR("zero length buffer for DMX\n");
				return -EINVAL;
			}
			if (release_offset >= rd_offset)
				skip_len = release_offset - rd_offset;
			else
				skip_len = release_offset +
				dmx_data->dmx_video_buf.buffer_len - rd_offset;
			DBG("Dispose: len:%u, offset:%u rd:%u wr:%u size:%u\n",
				skip_len, release_offset, rd_offset, wr_offset,
				dmx_data->dmx_video_buf.buffer_len);
			if (skip_len > mpq_streambuffer_data_avail(sbuff))
				ERR("Error: the release offset is wrong\n");
			if (skip_len > 0) {
				rc = mpq_streambuffer_data_read_dispose(
				   sbuff, skip_len);
				if (rc) {
					ERR("Error in dispose data\n");
					return rc;
				}
			}
		} else {
			rc = mpq_ring_buffer_release(
				v4l2_inst->ringbuf, release_offset);
			if (rc)
				return rc;
}
	}
	*p_buf_index = buf.index;
	return 0;
}

static int mpq_v4l2_deque_output_buffer(
	struct v4l2_instance *v4l2_inst,
	int *p_buf_index)
{
	struct v4l2_buffer buf;
	struct v4l2_plane plane[VIDEO_MAX_PLANES];
	struct buffer_info *pbuf;
	int rc = 0;
	int extra_idx;
	if (!v4l2_inst)
		return -EINVAL;
	*p_buf_index = -1;
	memset(&buf, 0, sizeof(buf));
	memset(&plane, 0, sizeof(plane));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	buf.memory = V4L2_MEMORY_USERPTR;
	buf.m.planes = plane;
	buf.length = v4l2_inst->fmt[CAPTURE_PORT].fmt.pix_mp.num_planes;

	rc = msm_vidc_dqbuf(v4l2_inst->vidc_inst, &buf);
	if (!rc)
		v4l2_inst->vidc_fbd++;
	if (rc)
		return rc;
	DBG("DEQUEUE OUTPUT Buffer with timestamp %u-%u\n",
		(u32)buf.timestamp.tv_sec,
		(u32)buf.timestamp.tv_usec/1000);
	if (buf.flags & 0x80000)
		DBG("v4l2_buf IDR Frame\n");
	else if (buf.flags & 0x8)
		DBG("v4l2_buf I Frame\n");
	else if (buf.flags & 0x10)
		DBG("v4l2_buf P Frame\n");
	else if (buf.flags & 0x20)
		DBG("v4l2_buf B Frame\n");

	*p_buf_index = buf.index;
	pbuf = &v4l2_inst->buf_info[CAPTURE_PORT][buf.index];
	pbuf->bytesused = buf.m.planes[0].bytesused;
	pbuf->pts = buf.timestamp.tv_sec*90000 +
		buf.timestamp.tv_usec*9/100;

	if (pbuf->handle) {
		rc = msm_vidc_smem_cache_operations(v4l2_inst->vidc_inst,
			pbuf->handle,
			SMEM_CACHE_INVALIDATE);
		if (rc)
			ERR("[%s]Failed to invalidate caches: %d\n",
				 __func__, rc);
	}
	if (buf.length > 1) {
		extra_idx = EXTRADATA_IDX(buf.length);
		pbuf->extradata.bytesused =
			buf.m.planes[extra_idx].bytesused;
		if (v4l2_inst->extradata_handle) {
			rc = msm_vidc_smem_cache_operations(
				v4l2_inst->vidc_inst,
				v4l2_inst->extradata_handle,
				SMEM_CACHE_INVALIDATE);
			if (rc)
				ERR("[%s]Failed to invalidate caches: %d\n",
					 __func__, rc);
		}
	} else {
		pbuf->extradata.bytesused = 0;
	}
	DBG("EXTRADATA: Buf[%d] get bytesused: %u\n",
		buf.index,
		pbuf->extradata.bytesused);
	return rc;
}

static int mpq_v4l2_prepare_buffer(
	struct v4l2_instance *v4l2_inst,
	u32 buf_index,
	u32 port_num)
{
	struct v4l2_buffer buf;
	struct v4l2_plane plane[VIDEO_MAX_PLANES];
	struct buffer_info *pbinfo;
	int rc = 0;
	if (!v4l2_inst || buf_index >= MAX_NUM_BUFS || port_num >= MAX_PORTS)
		return -EINVAL;
	pbinfo = &v4l2_inst->buf_info[port_num][buf_index];
	memset(&buf, 0, sizeof(buf));
	memset(&plane, 0, sizeof(plane));
	buf.type = pbinfo->buf_type;
	buf.index = pbinfo->index;
	buf.memory = V4L2_MEMORY_USERPTR;
	buf.length = v4l2_inst->fmt[port_num].fmt.pix_mp.num_planes;
	plane[0].length = pbinfo->size;
	plane[0].m.userptr = (unsigned long)pbinfo->dev_addr;
	plane[0].reserved[0] = pbinfo->fd;
	plane[0].reserved[1] = 0;
	plane[0].data_offset = pbinfo->offset;
	if ((port_num == CAPTURE_PORT) &&
		(v4l2_inst->fmt[CAPTURE_PORT].fmt.pix_mp.num_planes > 1)) {
		int extra_idx;
		extra_idx = EXTRADATA_IDX(
		v4l2_inst->fmt[CAPTURE_PORT].fmt.pix_mp.num_planes);
		if (v4l2_inst->extradata_handle) {
			plane[extra_idx].length = pbinfo->extradata.length;
			plane[extra_idx].m.userptr =
				v4l2_inst->extradata_handle->device_addr +
				buf_index * pbinfo->extradata.length;
			plane[extra_idx].reserved[0] =
				pbinfo->extradata.ion_fd;
			plane[extra_idx].reserved[1] =
				pbinfo->extradata.fd_offset;
			plane[extra_idx].data_offset = 0;
		} else {
			plane[extra_idx].length = 0;
			plane[extra_idx].m.userptr = 0;
			plane[extra_idx].reserved[0] = 0;
			plane[extra_idx].reserved[1] = 0;
			plane[extra_idx].data_offset = 0;
		}
		buf.length = v4l2_inst->fmt[CAPTURE_PORT].fmt.pix_mp.num_planes;
	} else {
		buf.length = 1;
	}
	buf.m.planes = plane;
	if (pbinfo->handle) {
		rc = msm_vidc_smem_cache_operations(v4l2_inst->vidc_inst,
			pbinfo->handle, SMEM_CACHE_CLEAN);
		if (rc)
			ERR("[%s]Failed to clean caches: %d\n", __func__, rc);
	}
	rc = msm_vidc_prepare_buf(v4l2_inst->vidc_inst, &buf);

	return rc;
}

static int mpq_translate_from_dvb_buf(
	struct v4l2_instance *v4l2_inst,
	struct video_data_buffer *p_dvb_buf,
	u32 buf_index,
	u32 port_num)
{
	struct buffer_info *pbinfo;
	struct msm_smem *handle = NULL;
	int rc = 0;
	DBG("ENTER mpq_translate_from_dvb_buf\n");
	if (!p_dvb_buf || buf_index >= MAX_NUM_BUFS || port_num >= MAX_PORTS) {
		ERR("[%s]Input parameter is NULL or invalid\n", __func__);
		return -EINVAL;
	}
	pbinfo = &v4l2_inst->buf_info[port_num][buf_index];
	pbinfo->index = buf_index;
	pbinfo->vaddr = (u32)p_dvb_buf->bufferaddr;
	pbinfo->buf_offset = p_dvb_buf->offset;
	pbinfo->size = p_dvb_buf->buffer_len;
	pbinfo->fd = p_dvb_buf->ion_fd;
	if (port_num == OUTPUT_PORT)
		pbinfo->buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	else
		pbinfo->buf_type =  V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	handle = msm_vidc_smem_user_to_kernel(v4l2_inst->vidc_inst,
				pbinfo->fd, pbinfo->buf_offset,
				(port_num == OUTPUT_PORT) ?
				HAL_BUFFER_INPUT : HAL_BUFFER_OUTPUT);

	if (!handle) {
		ERR("[%s]Error in msm_smem_user_to_kernel\n", __func__);
		return -EFAULT;
	}
	pbinfo->dev_addr = handle->device_addr + pbinfo->buf_offset;
	pbinfo->kernel_vaddr = (u32)handle->kvaddr;
	pbinfo->handle = handle;
	INF("BUFINFO: vaddr:%u kernel_vaddr:%u dev_addr:%u handle:%u\n",
		pbinfo->vaddr, pbinfo->kernel_vaddr, pbinfo->dev_addr,
		(unsigned int)pbinfo->handle);
	DBG("LEAVE mpq_translate_from_dvb_buf\n");
	return rc;
}

/***************************************************************************
 *  DVB Video IOCTL calls												  *
 ***************************************************************************/
static int mpq_dvb_set_input_buf_count(
	struct v4l2_instance *v4l2_inst,
	unsigned int num_of_bufs)
{
	if (!v4l2_inst) {
		ERR("[%s]Input parameter is NULL or invalid\n", __func__);
		return -EINVAL;
	}
	if (v4l2_inst->state != MPQ_STATE_INIT)
		return -EINVAL;
	if (num_of_bufs > MAX_NUM_BUFS) {
		ERR("[%s]Input parameter is NULL or invalid\n", __func__);
		return -EINVAL;
	}
	if (num_of_bufs == 1) {
		INF("Device is set for ring buffer input mode\n");
		v4l2_inst->input_mode = INPUT_MODE_RING;
	} else {
		v4l2_inst->input_mode = INPUT_MODE_LINEAR;
	}
	v4l2_inst->num_input_buffers = num_of_bufs;
	v4l2_inst->flag |= MPQ_DVB_INPUT_BUF_COUNT_BIT;
	v4l2_inst->flag &= ~MPQ_DVB_INPUT_BUF_REQ_BIT;
	v4l2_inst->flag &= ~MPQ_DVB_INPUT_BUF_SETUP_BIT;
	v4l2_inst->input_buf_count = 0;
	return 0;
}

static int mpq_dvb_set_output_buf_count(
	struct v4l2_instance *v4l2_inst,
	unsigned int num_of_bufs)
{
	int rc = 0;
	if (!v4l2_inst) {
		ERR("[%s]Input parameter is NULL or invalid\n", __func__);
		return -EINVAL;
	}
	if (v4l2_inst->state != MPQ_STATE_INIT)
		return -EINVAL;
	if (num_of_bufs > MAX_NUM_BUFS) {
		ERR("[%s]Input parameter is NULL or invalid\n", __func__);
		return -EINVAL;
	}
	v4l2_inst->num_output_buffers = num_of_bufs;
	v4l2_inst->flag |= MPQ_DVB_OUTPUT_BUF_COUNT_BIT;
	v4l2_inst->flag &= ~MPQ_DVB_OUTPUT_BUF_REQ_BIT;
	v4l2_inst->flag &= ~MPQ_DVB_OUTPUT_BUF_SETUP_BIT;
	v4l2_inst->output_buf_count = 0;
	return rc;
}

static int mpq_dvb_set_input_dmx_buf(
	struct v4l2_instance *v4l2_inst,
	struct video_data_buffer *pvideo_buf)
{
	int rc = 0;
	int i;
	DBG("ENTER mpq_dvb_set_input_dmx_buf\n");
	if (!v4l2_inst || !pvideo_buf) {
		ERR("[%s]Input parameter is NULL or invalid\n", __func__);
		return -EINVAL;
	}
	if (!(v4l2_inst->flag & MPQ_DVB_INPUT_BUF_REQ_BIT)) {
		ERR("[%s]Call mpq_dvb_get_reqbufs() before set input buffer\n",
			 __func__);
		return -EINVAL;
	}
	v4l2_inst->input_buf_count = 0;

	rc = mpq_translate_from_dvb_buf(v4l2_inst, pvideo_buf,
			v4l2_inst->input_buf_count, OUTPUT_PORT);
	rc = mpq_v4l2_prepare_buffer(v4l2_inst,
			v4l2_inst->input_buf_count, OUTPUT_PORT);
	if (rc) {
		ERR("ERROR in msm_vidc_prepare_buf for OUTPUT_PORT\n");
		return rc;
	}
	v4l2_inst->input_buf_count++;
	if (v4l2_inst->input_mode == INPUT_MODE_RING) {
			i = v4l2_inst->input_buf_count;
			while (i < v4l2_inst->num_input_buffers) {
				memcpy(&v4l2_inst->buf_info[OUTPUT_PORT][i],
					   &v4l2_inst->buf_info[OUTPUT_PORT][0],
					   sizeof(struct buffer_info));
				v4l2_inst->buf_info[OUTPUT_PORT][i].index = i;
				DBG("Internal Input Buffer NUM;%d Created\n",
					v4l2_inst->input_buf_count);
				i++;
				v4l2_inst->input_buf_count++;
			}
			v4l2_inst->flag |= MPQ_DVB_INPUT_BUF_SETUP_BIT;
			if (v4l2_inst->flag & MPQ_DVB_OUTPUT_BUF_SETUP_BIT)
				v4l2_inst->state = MPQ_STATE_IDLE;
			default_input_buf_size = pvideo_buf->buffer_len /
				v4l2_inst->num_input_buffers;
			if (v4l2_inst->state == MPQ_STATE_IDLE)
				DBG("Video decoder is in IDLE state now\n");
	} else {
		ERR("Error: input ring mode needed for demux source\n");
		return -EINVAL;
	}
	DBG("LEAVE mpq_dvb_set_input_buf\n");
	return rc;
}

static int mpq_dvb_set_input_buf(
	struct v4l2_instance *v4l2_inst,
	struct video_data_buffer *pvideo_buf)
{
	int rc = 0;
	int i;
	DBG("ENTER mpq_dvb_set_input_buf\n");
	if (!v4l2_inst || !pvideo_buf) {
		ERR("[%s]Input parameter is NULL or invalid\n", __func__);
		return -EINVAL;
	}
	if (v4l2_inst->state != MPQ_STATE_INIT)
		return -EINVAL;
	if (!(v4l2_inst->flag & MPQ_DVB_INPUT_BUF_REQ_BIT)) {
		ERR("[%s]Call mpq_dvb_get_reqbufs() before set input buffer\n",
			 __func__);
		return -EINVAL;
	}
	if (v4l2_inst->input_buf_count >= v4l2_inst->num_input_buffers) {
		ERR("[%s]Input Buffer more than specified\n", __func__);
		return -EINVAL;
	}
	rc = mpq_translate_from_dvb_buf(v4l2_inst, pvideo_buf,
			v4l2_inst->input_buf_count, OUTPUT_PORT);
	rc = mpq_v4l2_prepare_buffer(v4l2_inst,
			v4l2_inst->input_buf_count, OUTPUT_PORT);
	if (rc) {
		ERR("ERROR in msm_vidc_prepare_buf for OUTPUT_PORT\n");
		return rc;
	}
	v4l2_inst->buf_info[OUTPUT_PORT][v4l2_inst->input_buf_count].state =
		MPQ_INPUT_BUFFER_FREE;
	v4l2_inst->input_buf_count++;
	if (v4l2_inst->input_mode == INPUT_MODE_RING) {
		if (v4l2_inst->ringbuf) {

			mpq_unmap_kernel(v4l2_inst->mem_client,
				v4l2_inst->buf_info[OUTPUT_PORT][0].handle);
			rc = mpq_ring_buffer_delete(v4l2_inst->ringbuf);
			if (rc)
				return rc;
		}
		mpq_map_kernel(v4l2_inst->mem_client,
				v4l2_inst->buf_info[OUTPUT_PORT][0].handle);
		if (v4l2_inst->buf_info[OUTPUT_PORT][0].handle) {
			v4l2_inst->buf_info[OUTPUT_PORT][0].kernel_vaddr =
			(u32)v4l2_inst->buf_info[OUTPUT_PORT][0].handle->kvaddr;
		}
		rc = mpq_ring_buffer_create(&v4l2_inst->ringbuf,
				v4l2_inst->buf_info[OUTPUT_PORT][0]);
		if (!rc) {
			i = v4l2_inst->input_buf_count;
			while (i < v4l2_inst->num_input_buffers) {
				memcpy(&v4l2_inst->buf_info[OUTPUT_PORT][i],
					   &v4l2_inst->buf_info[OUTPUT_PORT][0],
					   sizeof(struct buffer_info));
				v4l2_inst->buf_info[OUTPUT_PORT][i].index = i;
				DBG("Internal Input Buffer NUM;%d Created\n",
					v4l2_inst->input_buf_count);
				i++;
				v4l2_inst->input_buf_count++;
			}
			v4l2_inst->flag |= MPQ_DVB_INPUT_BUF_SETUP_BIT;
			if (v4l2_inst->flag & MPQ_DVB_OUTPUT_BUF_SETUP_BIT)
				v4l2_inst->state = MPQ_STATE_IDLE;
			default_input_buf_size = pvideo_buf->buffer_len /
				v4l2_inst->num_input_buffers;
			if (v4l2_inst->state == MPQ_STATE_IDLE)
				DBG("Video decoder is in IDLE state now\n");
		}
	} else if (v4l2_inst->input_buf_count == v4l2_inst->num_input_buffers) {
		v4l2_inst->flag |= MPQ_DVB_INPUT_BUF_SETUP_BIT;
		if (v4l2_inst->flag & MPQ_DVB_OUTPUT_BUF_SETUP_BIT)
			v4l2_inst->state = MPQ_STATE_IDLE;
	}
	DBG("LEAVE mpq_dvb_set_input_buf\n");
	return rc;
}

static int mpq_dvb_set_output_buf(
	struct v4l2_instance *v4l2_inst,
	struct video_data_buffer *pvideo_buf)
{
	int rc = 0;
	DBG("ENTER mpq_dvb_set_output_buf\n");
	if (!v4l2_inst || !pvideo_buf) {
		ERR("[%s]Input parameter is NULL or invalid\n", __func__);
		return -EINVAL;
	}
	if (v4l2_inst->state != MPQ_STATE_INIT)
		return -EINVAL;
	if (!(v4l2_inst->flag & MPQ_DVB_OUTPUT_BUF_REQ_BIT)) {
		ERR("[%s]Call mpq_dvb_get_reqbufs() before set output buffer\n",
			 __func__);
		return -EINVAL;
	}
	if (v4l2_inst->output_buf_count >= v4l2_inst->num_output_buffers) {
		ERR("Setup output buffer more than required\n");
		return -EINVAL;
	}
	DBG("EXTRADATA: translate dvb buf\n");
	rc = mpq_translate_from_dvb_buf(v4l2_inst, pvideo_buf,
			v4l2_inst->output_buf_count, CAPTURE_PORT);
	if (!rc) {
		rc = mpq_v4l2_prepare_buffer(v4l2_inst,
				v4l2_inst->output_buf_count, CAPTURE_PORT);
		if (rc) {
			ERR("[%s]ERROR in msm_vidc_prepare_buf\n",
				__func__);
			return rc;
		}
		v4l2_inst->output_buf_count++;
		if (v4l2_inst->output_buf_count ==
			v4l2_inst->num_output_buffers) {
			v4l2_inst->flag |= MPQ_DVB_OUTPUT_BUF_SETUP_BIT;
			if (v4l2_inst->flag & MPQ_DVB_INPUT_BUF_SETUP_BIT)
				v4l2_inst->state = MPQ_STATE_IDLE;
		}
	}
	DBG("LEAVE mpq_dvb_set_output_buf\n");
	return rc;
}

static int mpq_dvb_set_extradata_buf(
	struct v4l2_instance *v4l2_inst,
	struct extradata_buffer *pextradata_buf)
{
	int rc = 0;
	int i;
	struct msm_smem     *handle;
	DBG("ENTER mpq_dvb_set_extradata_buf\n");
	if (!v4l2_inst || !pextradata_buf) {
		ERR("[%s]Input parameter is NULL or invalid\n", __func__);
		return -EINVAL;
	}
	if (v4l2_inst->state != MPQ_STATE_INIT)
		return -EINVAL;

	if (!(v4l2_inst->flag & MPQ_DVB_INPUT_BUF_REQ_BIT)) {
		ERR("[%s]Call reqbufs() before set buffer\n",
			 __func__);
		return -EINVAL;
	}
	if (!v4l2_inst->extradata_types)
		return -EINVAL;

	if (pextradata_buf->buffer_len <
		(v4l2_inst->num_output_buffers *
		 v4l2_inst->extradata_size)) {
		ERR("Buf Size is too small");
		return -EINVAL;
	}

	memcpy(&v4l2_inst->extradata,
		   pextradata_buf,
		   sizeof(v4l2_inst->extradata));
	handle = msm_vidc_smem_user_to_kernel(v4l2_inst->vidc_inst,
				v4l2_inst->extradata.ion_fd,
				v4l2_inst->extradata.offset,
				HAL_BUFFER_OUTPUT);

	if (!handle) {
		ERR("[%s]Error in msm_smem_user_to_kernel\n", __func__);
		return -EFAULT;
	}
	rc = msm_vidc_smem_cache_operations(v4l2_inst->vidc_inst,
		handle, SMEM_CACHE_CLEAN);
	if (rc) {
		ERR("[%s]Failed to clean caches: %d\n", __func__, rc);
		return rc;
	}
	v4l2_inst->extradata_handle = handle;
	DBG("EXTRADATA: map to device mem: %u\n", (u32)handle->device_addr);
	for (i = 0; i < v4l2_inst->num_output_buffers; i++) {
		struct buffer_info *pbinfo =
			&v4l2_inst->buf_info[CAPTURE_PORT][i];
		pbinfo->extradata.index = i;
		pbinfo->extradata.length =
			v4l2_inst->extradata_size;
		pbinfo->extradata.uaddr =
			(u32)v4l2_inst->extradata.bufferaddr +
			i * v4l2_inst->extradata_size;
		pbinfo->extradata.ion_fd =
			v4l2_inst->extradata.ion_fd;
		pbinfo->extradata.fd_offset =
			v4l2_inst->extradata.offset +
			i * v4l2_inst->extradata_size;
	}
	DBG("LEAVE mpq_dvb_set_extradata_buf\n");
	return rc;
}

static int mpq_dvb_free_input_buffers(
	struct v4l2_instance *v4l2_inst)
{
	int rc = 0;
	struct v4l2_requestbuffers v4l2_buffer_req;
	DBG("ENTER mpq_dvb_free_input_buf\n");
	if (!v4l2_inst) {
		ERR("[%s]Input parameter is NULL or invalid\n", __func__);
		return -EINVAL;
	}
	memset(&v4l2_buffer_req, 0, sizeof(v4l2_buffer_req));
	v4l2_buffer_req.count = 0;
	v4l2_buffer_req.memory = V4L2_MEMORY_USERPTR;
	v4l2_buffer_req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	rc = msm_vidc_reqbufs(v4l2_inst->vidc_inst, &v4l2_buffer_req);
	if (rc) {
		ERR("ERROR in msm_vidc_reqbufs for OUTPUT_PORT");
		return rc;
	}
	DBG("LEAVE mpq_dvb_free_input_buf\n");
	return rc;
}

static int mpq_dvb_free_output_buffers(
	struct v4l2_instance *v4l2_inst)
{
	int rc = 0;
	int i;
	struct v4l2_requestbuffers v4l2_buffer_req;
	struct buffer_info *pbuf;
	DBG("ENTER mpq_dvb_free_output_buf\n");
	if (!v4l2_inst) {
		ERR("[%s]Input parameter is NULL or invalid\n", __func__);
		return -EINVAL;
	}
	for (i = 0; i < v4l2_inst->num_output_buffers; i++) {
		pbuf = &v4l2_inst->buf_info[CAPTURE_PORT][i];
		if (pbuf->handle)
			msm_vidc_smem_free(v4l2_inst->vidc_inst, pbuf->handle);
	}
	rc = msm_vidc_release_buffers(
	   v4l2_inst->vidc_inst,
	   V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	if (rc) {
		ERR("ERROR in msm_vidc_reqbufs for OUTPUT_PORT");
		return rc;
	}
	memset(&v4l2_buffer_req, 0, sizeof(v4l2_buffer_req));
	v4l2_buffer_req.count = 0;
	v4l2_buffer_req.memory = V4L2_MEMORY_USERPTR;
	v4l2_buffer_req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	rc = msm_vidc_reqbufs(
	   v4l2_inst->vidc_inst,
	   &v4l2_buffer_req);
	if (rc) {
		ERR("ERROR in msm_vidc_reqbufs for CAPTURE_PORT");
		return rc;
	}
	DBG("LEAVE mpq_dvb_free_output_buf\n");
	return rc;
}

static int mpq_dvb_set_codec(
	struct v4l2_instance *v4l2_inst,
	enum video_codec_t video_codec)
{
	int rc = 0;
	if (!v4l2_inst || v4l2_inst->state != MPQ_STATE_INIT) {
		ERR("Input parameter is NULL or invalid\n");
		return -EINVAL;
	}
	if (video_codec == VIDEO_CODECTYPE_MPEG2) {
		v4l2_inst->video_codec = V4L2_PIX_FMT_MPEG2;
	} else if (video_codec == VIDEO_CODECTYPE_H264) {
		v4l2_inst->video_codec = V4L2_PIX_FMT_H264;
	} else {
		ERR("Unsupported video codec\n");
		return -EINVAL;
	}
	return rc;
}

static int mpq_dvb_get_buffer_req(
	struct v4l2_instance *v4l2_inst,
	struct video_buffer_req *vdec_buf_req)
{
	int rc = 0;
	int i;
	int extradata_num;
	if (!v4l2_inst || !vdec_buf_req)
		return -EINVAL;
	if (v4l2_inst->state != MPQ_STATE_INIT)
		return -EINVAL;
	if ((v4l2_inst->flag & MPQ_DVB_INPUT_BUF_COUNT_BIT) &&
		(v4l2_inst->flag & MPQ_DVB_OUTPUT_BUF_COUNT_BIT)) {
		struct v4l2_format fmt;
		struct v4l2_control control;
		struct v4l2_requestbuffers v4l2_buffer_req;
		memset(&fmt, 0, sizeof(fmt));
		fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		fmt.fmt.pix_mp.height = default_height;
		fmt.fmt.pix_mp.width = default_width;
		fmt.fmt.pix_mp.pixelformat = v4l2_inst->video_codec;
		rc = msm_vidc_s_fmt(v4l2_inst->vidc_inst, &fmt);
		if (rc) {
			ERR("ERROR in set format for OUTPUT_PORT\n");
			return rc;
		}

		control.id = V4L2_CID_MPEG_VIDC_VIDEO_CONTINUE_DATA_TRANSFER;
		control.value = 1;
		rc = msm_vidc_s_ctrl(v4l2_inst->vidc_inst, &control);
		if (rc) {
			ERR("ERROR:set CONTINUE_DATA_TRANSFER\n");
			return rc;
		}

		control.id = V4L2_CID_MPEG_VIDC_SET_PERF_LEVEL;
		control.value = V4L2_CID_MPEG_VIDC_PERF_LEVEL_TURBO;
		rc = msm_vidc_s_ctrl(v4l2_inst->vidc_inst, &control);
		if (rc) {
			ERR("ERROR set VIDEO_ENABLE_PICTURE_TYPE\n");
			return rc;
		}

		if (v4l2_inst->input_mode == INPUT_MODE_RING) {
			control.id = V4L2_CID_MPEG_VIDC_VIDEO_ALLOC_MODE_INPUT;
			control.value = 1;
			rc = msm_vidc_s_ctrl(v4l2_inst->vidc_inst, &control);
			if (rc) {
				ERR("ERROR set VIDEO_ALLOC_MODE\n");
				return rc;
			}
			DBG("Set Ctrl V4L2_CID_MPEG_VIDC_VIDEO_ALLOC_MODE\n");
			control.id = V4L2_CID_MPEG_VIDC_VIDEO_FRAME_ASSEMBLY;
			control.value = 1;
			rc = msm_vidc_s_ctrl(v4l2_inst->vidc_inst, &control);
			if (rc) {
				ERR("ERROR set VIDEO_FRAME_ASSEMBLY\n");
				return rc;
			}
			DBG("Set Ctrl VIDEO_FRAME_ASSEMBLY\n");
		}
		if (v4l2_inst->extradata_types) {
			control.id = V4L2_CID_MPEG_VIDC_VIDEO_EXTRADATA;
			extradata_num = sizeof(mpq_v4l2_extradata) /
				sizeof(struct v4l2_extradata_type);
			for (i = 0; i < extradata_num; i++) {
				if (v4l2_inst->extradata_types &
					mpq_v4l2_extradata[i].mask) {
					control.value =
						mpq_v4l2_extradata[i].type;
					rc = msm_vidc_s_ctrl(
					   v4l2_inst->vidc_inst, &control);
					if (rc)
						return rc;
				}
			}
		}
		memset(&v4l2_inst->fmt[OUTPUT_PORT], 0,
				sizeof(v4l2_inst->fmt[OUTPUT_PORT]));
		v4l2_inst->fmt[OUTPUT_PORT].type =
			V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		rc = msm_vidc_g_fmt(v4l2_inst->vidc_inst,
				&v4l2_inst->fmt[OUTPUT_PORT]);
		if (rc) {
			ERR("ERROR in get format for OUTPUT_PORT\n");
			return rc;
		}
		memset(&v4l2_inst->fmt[CAPTURE_PORT], 0,
			sizeof(v4l2_inst->fmt[CAPTURE_PORT]));
		v4l2_inst->fmt[CAPTURE_PORT].type =
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		rc = msm_vidc_g_fmt(v4l2_inst->vidc_inst,
				&v4l2_inst->fmt[CAPTURE_PORT]);
		if (rc) {
			ERR("ERROR in get format for CAPTURE_PORT\n");
			return rc;
		}
		memset(&fmt, 0, sizeof(fmt));
		fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		fmt.fmt.pix_mp.height = default_height;
		fmt.fmt.pix_mp.width = default_width;
		fmt.fmt.pix_mp.pixelformat = v4l2_inst->video_codec;
		rc = msm_vidc_s_fmt(v4l2_inst->vidc_inst, &fmt);
		if (rc) {
			ERR("ERROR in set format for OUTPUT_PORT\n");
			return rc;
		}
		v4l2_inst->fmt[OUTPUT_PORT].fmt.pix_mp.height =
			fmt.fmt.pix_mp.height;
		v4l2_inst->fmt[OUTPUT_PORT].fmt.pix_mp.width =
			fmt.fmt.pix_mp.width;
		v4l2_inst->fmt[OUTPUT_PORT].fmt.pix_mp.pixelformat =
			fmt.fmt.pix_mp.pixelformat;
		v4l2_inst->fmt[OUTPUT_PORT].fmt.pix_mp.plane_fmt[0].sizeimage =
			fmt.fmt.pix_mp.plane_fmt[0].sizeimage;

		memset(&fmt, 0, sizeof(fmt));
		fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		fmt.fmt.pix_mp.height = default_height;
		fmt.fmt.pix_mp.width = default_width;
		fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12;
		rc = msm_vidc_s_fmt(v4l2_inst->vidc_inst, &fmt);
		if (rc) {
			ERR("ERROR in set format for CAPTURE_PORT\n");
			return rc;
		}
		memset(&fmt, 0, sizeof(fmt));
		fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		rc = msm_vidc_g_fmt(v4l2_inst->vidc_inst, &fmt);
		if (rc) {
			ERR("ERROR in get format for CAPTURE_PORT\n");
			return rc;
		}

		v4l2_inst->fmt[CAPTURE_PORT].fmt.pix_mp.height =
			fmt.fmt.pix_mp.height;
		v4l2_inst->fmt[CAPTURE_PORT].fmt.pix_mp.width =
			fmt.fmt.pix_mp.width;
		v4l2_inst->fmt[CAPTURE_PORT].fmt.pix_mp.pixelformat =
			fmt.fmt.pix_mp.pixelformat;
		v4l2_inst->fmt[CAPTURE_PORT].fmt.pix_mp.plane_fmt[0].sizeimage =
			fmt.fmt.pix_mp.plane_fmt[0].sizeimage;
		v4l2_inst->extradata_size =
			fmt.fmt.pix_mp.plane_fmt[1].sizeimage;
		INF("EXTRADATA Buffer Size: %d\n",
			fmt.fmt.pix_mp.plane_fmt[1].sizeimage);

		memset(&v4l2_buffer_req, 0, sizeof(v4l2_buffer_req));
		v4l2_buffer_req.count = default_buf_num_for_ringbuf;
		v4l2_buffer_req.memory = V4L2_MEMORY_USERPTR;
		v4l2_buffer_req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		rc = msm_vidc_reqbufs(v4l2_inst->vidc_inst, &v4l2_buffer_req);
		if (rc) {
			ERR("ERROR in msm_vidc_reqbufs for OUTPUT_PORT");
			return rc;
		}
		if (default_buf_num_for_ringbuf > v4l2_buffer_req.count)
			v4l2_inst->num_input_buffers =
			default_buf_num_for_ringbuf;
		else
			v4l2_inst->num_input_buffers = v4l2_buffer_req.count;

		v4l2_buffer_req.count = v4l2_inst->num_output_buffers;
		v4l2_buffer_req.memory = V4L2_MEMORY_USERPTR;
		v4l2_buffer_req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		rc = msm_vidc_reqbufs(v4l2_inst->vidc_inst, &v4l2_buffer_req);
		if (rc) {
			ERR("ERROR in msm_vidc_reqbufs for CAPTURE_PORT");
			return rc;
		}
		v4l2_inst->num_output_buffers = v4l2_buffer_req.count;
		vdec_buf_req->input_buf_prop.alignment  = 4096;
		vdec_buf_req->input_buf_prop.buf_poolid = 0;
		vdec_buf_req->input_buf_prop.buf_size	=
		v4l2_inst->fmt[OUTPUT_PORT].fmt.pix_mp.plane_fmt[0].sizeimage;
		vdec_buf_req->num_input_buffers	=
			v4l2_inst->num_input_buffers;


		vdec_buf_req->output_buf_prop.alignment  = 4096;
		vdec_buf_req->output_buf_prop.buf_poolid = 0;
		vdec_buf_req->output_buf_prop.buf_size	=
		v4l2_inst->fmt[CAPTURE_PORT].fmt.pix_mp.plane_fmt[0].sizeimage;
		vdec_buf_req->num_output_buffers		 =
			v4l2_inst->num_output_buffers;
		vdec_buf_req->extradata_size = v4l2_inst->extradata_size;
		v4l2_inst->flag |= MPQ_DVB_INPUT_BUF_REQ_BIT;
		v4l2_inst->flag |= MPQ_DVB_OUTPUT_BUF_REQ_BIT;

		INF("GET BUF REQ: INPUT(%d%d) OUTPUT(%d%d)\n",
			vdec_buf_req->input_buf_prop.buf_size,
			vdec_buf_req->num_input_buffers,
			vdec_buf_req->output_buf_prop.buf_size,
			vdec_buf_req->num_output_buffers);
		return 0;
	}
	return -EINVAL;
}

static int mpq_dvb_set_output_format(
		struct v4l2_instance *v4l2_inst,
		enum video_out_format_t format)
{
	struct v4l2_format fmt;
	int rc = 0;
	if (!v4l2_inst)
		return -EINVAL;
	if (v4l2_inst->state != MPQ_STATE_INIT)
		return -EINVAL;
	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	fmt.fmt.pix_mp.height = default_height;
	fmt.fmt.pix_mp.width = default_width;
	switch (format) {
	case VIDEO_YUV_FORMAT_NV12:
		fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12;
		break;
	case VIDEO_YUV_FORMAT_TILE_4x2:
		rc = -EINVAL;
		break;
	default:
		rc = -EINVAL;
		break;
	}
	if (rc)
		ERR("ERROR in set format for CAPTURE_PORT\n");
	return rc;

}

static int mpq_playback_coarse_to_normal(struct v4l2_instance *v4l2_inst)
{
	int rc = 0;
	struct v4l2_control control;
	if (!v4l2_inst) {
		ERR("ERROR passed Null pointer\n");
		return -EINVAL;
	}
	control.id = V4L2_CID_MPEG_VIDC_VIDEO_ENABLE_PICTURE_TYPE;
	control.value = 15;
	rc = msm_vidc_s_ctrl(v4l2_inst->vidc_inst, &control);
	if (rc) {
		ERR("ERROR set VIDEO_ENABLE_PICTURE_TYPE\n");
		return rc;
	}
	DBG("Set Ctrl V4L2_CID_MPEG_VIDC_VIDEO_ENABLE_PICTURE_TYPE\n");
	control.id = V4L2_CID_MPEG_VIDC_VIDEO_OUTPUT_ORDER;
	control.value = 0;
	rc = msm_vidc_s_ctrl(v4l2_inst->vidc_inst, &control);
	if (rc) {
		ERR("ERROR set VIDEO_OUTPUT_ORDER\n");
		return rc;
	}
	DBG("Set Ctrl VIDEO_OUTPUT_ORDER\n");
	return rc;
}

static int mpq_playback_normal_to_coarse(struct v4l2_instance *v4l2_inst)
{
	int rc = 0;
	struct v4l2_control control;
	if (!v4l2_inst) {
		ERR("ERROR passed Null pointer\n");
		return -EINVAL;
	}

	control.id = V4L2_CID_MPEG_VIDC_SET_PERF_LEVEL;
	control.value = V4L2_CID_MPEG_VIDC_PERF_LEVEL_TURBO;
	rc = msm_vidc_s_ctrl(v4l2_inst->vidc_inst, &control);
	if (rc) {
		ERR("ERROR set VIDEO_ENABLE_PICTURE_TYPE\n");
		return rc;
	}

	control.id = V4L2_CID_MPEG_VIDC_VIDEO_ENABLE_PICTURE_TYPE;
	control.value = 9;
	rc = msm_vidc_s_ctrl(v4l2_inst->vidc_inst, &control);
	if (rc) {
		ERR("ERROR set VIDEO_ENABLE_PICTURE_TYPE\n");
		return rc;
	}
	DBG("Set Ctrl ENABLE_PICTURE_TYPE\n");
	control.id = V4L2_CID_MPEG_VIDC_VIDEO_OUTPUT_ORDER;
	control.value = 1;
	rc = msm_vidc_s_ctrl(v4l2_inst->vidc_inst, &control);
	if (rc) {
		ERR("ERROR set VIDEO_OUTPUT_ORDER\n");
		return rc;
	}
	DBG("Set Ctrl VIDEO_OUTPUT_ORDER\n");
	return rc;
}

static int mpq_playback_smooth_to_normal(struct v4l2_instance *v4l2_inst)
{
	return 0;
}

static int mpq_playback_normal_to_smooth(struct v4l2_instance *v4l2_inst)
{
	return 0;
}


static int mpq_playback_smooth_to_coarse(struct v4l2_instance *v4l2_inst)
{
	int rc = 0;
	struct v4l2_control control;
	if (!v4l2_inst) {
		ERR("ERROR passed Null pointer\n");
		return -EINVAL;
	}
	control.id = V4L2_CID_MPEG_VIDC_VIDEO_ENABLE_PICTURE_TYPE;
	control.value = 9;
	rc = msm_vidc_s_ctrl(v4l2_inst->vidc_inst, &control);
	if (rc) {
		ERR("ERROR set VIDEO_ENABLE_PICTURE_TYPE\n");
		return rc;
	}
	DBG("Set Ctrl ENABLE_PICTURE_TYPE\n");
	control.id = V4L2_CID_MPEG_VIDC_VIDEO_OUTPUT_ORDER;
	control.value = 1;
	rc = msm_vidc_s_ctrl(v4l2_inst->vidc_inst, &control);
	if (rc) {
		ERR("ERROR set VIDEO_OUTPUT_ORDER\n");
		return rc;
	}
	DBG("Set Ctrl VIDEO_OUTPUT_ORDER\n");
	return rc;
}


static int mpq_playback_coarse_to_smooth(struct v4l2_instance *v4l2_inst)
{
	int rc = 0;
	struct v4l2_control control;
	if (!v4l2_inst) {
		ERR("ERROR passed Null pointer\n");
		return -EINVAL;
	}
	control.id = V4L2_CID_MPEG_VIDC_VIDEO_ENABLE_PICTURE_TYPE;
	control.value = 15;
	rc = msm_vidc_s_ctrl(v4l2_inst->vidc_inst, &control);
	if (rc) {
		ERR("ERROR set VIDEO_ENABLE_PICTURE_TYPE\n");
		return rc;
	}
	DBG("Set Ctrl ENABLE_PICTURE_TYPE\n");
	control.id = V4L2_CID_MPEG_VIDC_VIDEO_OUTPUT_ORDER;
	control.value = 0;
	rc = msm_vidc_s_ctrl(v4l2_inst->vidc_inst, &control);
	if (rc) {
		ERR("ERROR set VIDEO_OUTPUT_ORDER\n");
		return rc;
	}
	DBG("Set Ctrl VIDEO_OUTPUT_ORDER\n");
	return rc;
}

static int mpq_dvb_set_decode_mode(
struct mpq_dvb_video_instance *dvb_video_inst,
int decode_mode)
{
	int rc = 0;
	struct v4l2_instance *v4l2_inst;
	if (!dvb_video_inst || !dvb_video_inst->v4l2_inst) {
		ERR("ERROR passed Null pointer\n");
		return -EINVAL;
	}
	v4l2_inst = dvb_video_inst->v4l2_inst;

	if (decode_mode == VIDEO_PLAYBACK_TRICKMODE_COARSE) {
		switch (v4l2_inst->playback_mode) {
		case VIDEO_PLAYBACK_NORMAL:
			rc = mpq_dvb_flush(dvb_video_inst);
			if (rc) {
				ERR("Error in flush buffers\n");
				return rc;
			}
			DBG("Decoder Flush DONE\n");
			rc = mpq_playback_normal_to_coarse(v4l2_inst);
			break;
		case VIDEO_PLAYBACK_TRICKMODE_SMOOTH:
			rc = mpq_dvb_flush(dvb_video_inst);
			if (rc) {
				ERR("Error in flush buffers\n");
				return rc;
			}
			DBG("Decoder Flush DONE\n");
			rc = mpq_playback_smooth_to_coarse(v4l2_inst);
			break;
		default:
			break;
		}
	}
	if (decode_mode == VIDEO_PLAYBACK_TRICKMODE_SMOOTH) {
		switch (v4l2_inst->playback_mode) {
		case VIDEO_PLAYBACK_NORMAL:
			rc = mpq_playback_normal_to_smooth(v4l2_inst);
			break;
		case VIDEO_PLAYBACK_TRICKMODE_COARSE:
			rc = mpq_dvb_flush(dvb_video_inst);
			if (rc) {
				ERR("Error in flush buffers\n");
				return rc;
			}
			DBG("Decoder Flush DONE\n");
			rc = mpq_playback_coarse_to_smooth(v4l2_inst);
			break;
		default:
			break;
		}
	}
	if (decode_mode == VIDEO_PLAYBACK_NORMAL) {
		switch (v4l2_inst->playback_mode) {
		case VIDEO_PLAYBACK_TRICKMODE_COARSE:
			rc = mpq_dvb_flush(dvb_video_inst);
			if (rc) {
				ERR("Error in flush buffers\n");
				return rc;
			}
			DBG("Decoder Flush DONE\n");
			rc = mpq_playback_coarse_to_normal(v4l2_inst);
			break;
		case VIDEO_PLAYBACK_TRICKMODE_SMOOTH:
			rc = mpq_playback_smooth_to_normal(v4l2_inst);
			break;
		default:
			break;
		}
	}
	if (!rc)
		v4l2_inst->playback_mode = decode_mode;
	else
		WRN("Error in settting decode mode\n");
	return rc;
}

static int mpq_dvb_get_output_format(
	struct v4l2_instance *v4l2_inst,
	enum video_out_format_t *format)
{
	int rc = 0;
	struct v4l2_format fmt;
	if (!v4l2_inst || !format)
		return -EINVAL;
	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	rc = msm_vidc_g_fmt(v4l2_inst->vidc_inst, &fmt);
	if (rc) {
		ERR("ERROR in get format for OUTPUT_PORT\n");
	} else {
		if (fmt.fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV12)
			*format = VIDEO_YUV_FORMAT_NV12;
	}
	return rc;
}

static int mpq_dvb_get_codec(
	struct v4l2_instance *v4l2_inst,
	enum video_codec_t *video_codec)
{
	int rc = 0;
	if (!v4l2_inst || !video_codec)
		return -EINVAL;

	*video_codec = VIDEO_CODECTYPE_H264;

	return rc;
}

static int mpq_dvb_start_play(
	struct mpq_dvb_video_instance *dvb_video_inst)
{
	int rc = 0;
	int i = 0;
	struct mpq_inq_msg *p_inq_msg;
	struct v4l2_instance *v4l2_inst;

	if (!dvb_video_inst || !dvb_video_inst->v4l2_inst) {
		ERR("Input parameter is NULL or invalid\n");
		return -EINVAL;
	}
	v4l2_inst = dvb_video_inst->v4l2_inst;
	if (v4l2_inst->input_mode == INPUT_MODE_RING)
		mpq_ring_buffer_reset(v4l2_inst->ringbuf);
	if (down_interruptible(&v4l2_inst->inq_sem))
		return -ERESTARTSYS;
	for (i = 0; i < v4l2_inst->num_input_buffers; i++) {
		if (v4l2_inst->buf_info[OUTPUT_PORT][i].state ==
			MPQ_INPUT_BUFFER_FREE) {
			p_inq_msg = kzalloc(
			   sizeof(struct mpq_inq_msg),
			   GFP_KERNEL);
			if (!p_inq_msg)
				return -ENOMEM;
			p_inq_msg->buf_index = i;
			list_add_tail(&p_inq_msg->list, &v4l2_inst->inq);
			v4l2_inst->buf_info[OUTPUT_PORT][i].state =
				MPQ_INPUT_BUFFER_IN_USE;
		}
	}
	up(&v4l2_inst->inq_sem);
	wake_up(&v4l2_inst->inq_wait);
	if (!rc) {
		struct mpq_outq_msg *p_msg;
		v4l2_inst->state = MPQ_STATE_RUNNING;
		DBG("Send Decoder is playing\n");
		p_msg = kzalloc(sizeof(*p_msg), GFP_KERNEL);
		p_msg->type = MPQ_MSG_VIDC_EVENT;
		p_msg->vidc_event.type =
			VIDEO_EVENT_DECODER_PLAYING;
		p_msg->vidc_event.status =
			VIDEO_STATUS_SUCESS;
		if (down_interruptible(&v4l2_inst->outq_sem))
			return -ERESTARTSYS;
		list_add_tail(&p_msg->list, &v4l2_inst->outq);
		up(&v4l2_inst->outq_sem);
		wake_up(&v4l2_inst->outq_wait);
		if (v4l2_inst->input_mode == INPUT_MODE_RING) {
			int device_id = dvb_video_inst->video_dev->id;
			dvb_video_inst->input_task = kthread_run(
					mpq_dvb_input_data_handler,
					(void *)dvb_video_inst,
					vid_data_thread_names[device_id]);
			if (!dvb_video_inst->input_task)
				return -ENOMEM;
		}
		if (dvb_video_inst->source == VIDEO_SOURCE_DEMUX) {
			rc = mpq_dvb_start_dmx_src(dvb_video_inst);
			if (rc) {
				ERR("Error start dmx thread\n");
				return rc;
			}
		}
	} else {
		ERR("Unable to start play the decoder\n");
	}
	return rc;
}

static int mpq_dvb_start_decoder(
	struct mpq_dvb_video_instance *dvb_video_inst)
{
	int rc = 0;
	int device_id = 0;
	struct v4l2_instance *v4l2_inst;

	if (!dvb_video_inst || !dvb_video_inst->v4l2_inst) {
		ERR("Input parameter is NULL or invalid\n");
		return -EINVAL;
	}
	v4l2_inst = dvb_video_inst->v4l2_inst;
	if (v4l2_inst->state != MPQ_STATE_IDLE) {
		if (dvb_video_inst->source != VIDEO_SOURCE_DEMUX) {
			ERR("Decoder is not in IDLE state\n");
			return -EINVAL;
		} else {
			if (v4l2_inst->state == MPQ_STATE_INIT &&
				v4l2_inst->flag &
				MPQ_DVB_OUTPUT_BUF_SETUP_BIT) {
				struct mpq_dmx_source *dmx_data;
				dmx_data = dvb_video_inst->dmx_src_data;
				if (!dmx_data || !dmx_data->stream_buffer) {
					ERR("No DMX source is not ready\n");
					return -EINVAL;
				}
				rc = mpq_dvb_set_input_dmx_buf(
				   dvb_video_inst->v4l2_inst,
				   &dmx_data->dmx_video_buf);
				if (rc) {
					ERR("Error in set dmx input buffer\n");
					return rc;
				}
				v4l2_inst->state = MPQ_STATE_IDLE;
			} else {
				ERR("Decoder is not in IDLE state\n");
				return -EINVAL;
			}
		}
	}
	v4l2_inst->vidc_etb = 0;
	v4l2_inst->vidc_ebd = 0;
	v4l2_inst->vidc_ftb = 0;
	v4l2_inst->vidc_fbd = 0;
	if (!dvb_video_inst->event_task) {
		dvb_video_inst->event_task = kthread_run(
				mpq_dvb_vidc_event_handler,
				(void *)dvb_video_inst,
				vid_thread_names[device_id]);
		if (!dvb_video_inst->event_task) {
			ERR("Unable to start event task\n");
			return -ENOMEM;
		}
	}
	rc = msm_vidc_streamon(v4l2_inst->vidc_inst,
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	if (rc) {
		ERR("MSM Vidc streamon->CAPTURE_PORT error\n");
		return rc;
	}
	v4l2_inst->flag |= MPQ_DVB_OUTPUT_STREAMON_BIT;
	v4l2_inst->flag &= ~MPQ_DVB_INPUT_STREAMON_BIT;

	rc = mpq_dvb_start_play(dvb_video_inst);
	return rc;
}

static int mpq_dvb_flush(
	struct mpq_dvb_video_instance *dvb_video_inst)
{
	int rc = 0;
	struct v4l2_instance *v4l2_inst;
	struct v4l2_decoder_cmd cmd;
	int i;
	struct mpq_inq_msg *p_inq_msg;
	struct mpq_outq_msg *p_msg;
	DBG("ENTER mpq_dvb_flush\n");
	if (!dvb_video_inst || !dvb_video_inst->v4l2_inst) {
		ERR("Input parameter is NULL or invalid\n");
		return -EINVAL;
	}
	v4l2_inst = dvb_video_inst->v4l2_inst;
	mutex_lock(&v4l2_inst->flush_lock);
	v4l2_inst->flag &= ~MPQ_DVB_EVENT_FLUSH_DONE_BIT;
	v4l2_inst->flag |= MPQ_DVB_INPUT_FLUSH_IN_PROGRESS_BIT;
	v4l2_inst->flag |= MPQ_DVB_OUTPUT_FLUSH_IN_PROGRESS_BIT;
	mutex_unlock(&v4l2_inst->flush_lock);

	if (v4l2_inst->input_mode == INPUT_MODE_RING)
		mpq_ring_buffer_flush(v4l2_inst->ringbuf);


	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd = V4L2_DEC_QCOM_CMD_FLUSH;
	cmd.flags = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE |
		V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	rc = msm_vidc_decoder_cmd(v4l2_inst->vidc_inst, &cmd);
	if (rc) {
		ERR("DECODER_QCOM_CMD_FLUSH failed\n");
		return rc;
	}
	DBG("Decoder Flush INPUT Success\n");
	if (!((v4l2_inst->flag & MPQ_DVB_EVENT_FLUSH_DONE_BIT) &&
		  (v4l2_inst->vidc_ftb == v4l2_inst->vidc_fbd))) {
		wait_event_interruptible(v4l2_inst->msg_wait,
			(v4l2_inst->flag & MPQ_DVB_EVENT_FLUSH_DONE_BIT) &&
			(v4l2_inst->vidc_ftb == v4l2_inst->vidc_fbd));
	}
	DBG("Decoder Flush ALL Success\n");
	/* sanity check: ETB == EBD & FTB == FBD */
	if ((v4l2_inst->vidc_etb == v4l2_inst->vidc_ebd) &&
		(v4l2_inst->vidc_ftb == v4l2_inst->vidc_fbd)) {
		INF("vidc flush success: ETB(%u) EBD(%u) FTB(%u) FBD(%u)\n",
			v4l2_inst->vidc_etb,
			v4l2_inst->vidc_ebd,
			v4l2_inst->vidc_ftb,
			v4l2_inst->vidc_fbd);
	} else {
		INF("vidc flush ERROR: ETB(%u) EBD(%u) FTB(%u) FBD(%u)\n",
			v4l2_inst->vidc_etb,
			v4l2_inst->vidc_ebd,
			v4l2_inst->vidc_ftb,
			v4l2_inst->vidc_fbd);
	}
	INF("================RINGBUFFER FLUSH here================\n");
	if (v4l2_inst->input_mode == INPUT_MODE_RING)
		mpq_ring_buffer_flush(v4l2_inst->ringbuf);
	mutex_lock(&v4l2_inst->flush_lock);
	v4l2_inst->flag &= ~MPQ_DVB_INPUT_FLUSH_IN_PROGRESS_BIT;
	v4l2_inst->flag &= ~MPQ_DVB_OUTPUT_FLUSH_IN_PROGRESS_BIT;
	mutex_unlock(&v4l2_inst->flush_lock);


	INF("================RINGBUFFER Reset here================\n");
	if (v4l2_inst->input_mode == INPUT_MODE_RING)
		mpq_ring_buffer_reset(v4l2_inst->ringbuf);
	if (down_interruptible(&v4l2_inst->inq_sem))
		return -ERESTARTSYS;
	for (i = 0; i < v4l2_inst->num_input_buffers; i++) {
		if (v4l2_inst->buf_info[OUTPUT_PORT][i].state ==
			MPQ_INPUT_BUFFER_FREE) {
			p_inq_msg = kzalloc(
			   sizeof(struct mpq_inq_msg),
			   GFP_KERNEL);
			if (!p_inq_msg)
				return -ENOMEM;
			p_inq_msg->buf_index = i;
			list_add_tail(&p_inq_msg->list, &v4l2_inst->inq);
			v4l2_inst->buf_info[OUTPUT_PORT][i].state =
				MPQ_INPUT_BUFFER_IN_USE;
		}
	}
	up(&v4l2_inst->inq_sem);
	wake_up(&v4l2_inst->inq_wait);

	p_msg = kzalloc(sizeof(*p_msg), GFP_KERNEL);
	p_msg->type = MPQ_MSG_VIDC_EVENT;
	p_msg->vidc_event.type =
		VIDEO_EVENT_INPUT_FLUSH_DONE;
	p_msg->vidc_event.status =
		VIDEO_STATUS_SUCESS;
	if (down_interruptible(&v4l2_inst->outq_sem))
		return -ERESTARTSYS;
	list_add_tail(&p_msg->list, &v4l2_inst->outq);
	up(&v4l2_inst->outq_sem);

	p_msg = kzalloc(sizeof(*p_msg), GFP_KERNEL);
	p_msg->type = MPQ_MSG_VIDC_EVENT;
	p_msg->vidc_event.type =
		VIDEO_EVENT_OUTPUT_FLUSH_DONE;
	p_msg->vidc_event.status =
		VIDEO_STATUS_SUCESS;
	if (down_interruptible(&v4l2_inst->outq_sem))
		return -ERESTARTSYS;
	list_add_tail(&p_msg->list, &v4l2_inst->outq);
	up(&v4l2_inst->outq_sem);
	wake_up(&v4l2_inst->outq_wait);

	DBG("LEAVE mpq_dvb_flush\n");
	return rc;
}

static int mpq_dvb_flush_input(
	struct mpq_dvb_video_instance *dvb_video_inst)
{
	int rc = 0;
	struct v4l2_instance *v4l2_inst;
	struct v4l2_decoder_cmd cmd;
	int i;
	struct mpq_inq_msg *p_inq_msg;
	struct mpq_outq_msg *p_msg;
	DBG("ENTER mpq_dvb_flush_input\n");
	if (!dvb_video_inst || !dvb_video_inst->v4l2_inst) {
		ERR("Input parameter is NULL or invalid\n");
		return -EINVAL;
	}
	v4l2_inst = dvb_video_inst->v4l2_inst;
	mutex_lock(&v4l2_inst->flush_lock);
	v4l2_inst->flag &= ~MPQ_DVB_EVENT_FLUSH_DONE_BIT;
	v4l2_inst->flag |= MPQ_DVB_INPUT_FLUSH_IN_PROGRESS_BIT;
	mutex_unlock(&v4l2_inst->flush_lock);
	if (v4l2_inst->input_mode == INPUT_MODE_RING)
		mpq_ring_buffer_flush(v4l2_inst->ringbuf);
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd = V4L2_DEC_QCOM_CMD_FLUSH;
	cmd.flags = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	rc = msm_vidc_decoder_cmd(v4l2_inst->vidc_inst, &cmd);
	if (rc) {
		ERR("DECODER_QCOM_CMD_FLUSH failed\n");
		return rc;
	}
	DBG("Decoder Flush INPUT Success\n");
	if (!((v4l2_inst->flag & MPQ_DVB_EVENT_FLUSH_DONE_BIT) &&
		  (v4l2_inst->vidc_etb == v4l2_inst->vidc_ebd))) {
		wait_event_interruptible(v4l2_inst->msg_wait,
			(v4l2_inst->flag & MPQ_DVB_EVENT_FLUSH_DONE_BIT) &&
			(v4l2_inst->vidc_etb == v4l2_inst->vidc_ebd));
	}
	DBG("Decoder Flush INPUT Success\n");
	/* sanity check: ETB == EBD & FTB == FBD */
	if ((v4l2_inst->vidc_etb == v4l2_inst->vidc_ebd)) {
		INF("vidc flush input success: ETB(%u) EBD(%u)\n",
			v4l2_inst->vidc_etb,
			v4l2_inst->vidc_ebd);
	} else {
		INF("vidc flush INPUT ERROR: ETB(%u) EBD(%u)\n",
			v4l2_inst->vidc_etb,
			v4l2_inst->vidc_ebd);
	}
	if (v4l2_inst->input_mode == INPUT_MODE_RING)
		mpq_ring_buffer_flush(v4l2_inst->ringbuf);
	mutex_lock(&v4l2_inst->flush_lock);
	v4l2_inst->flag &= ~MPQ_DVB_INPUT_FLUSH_IN_PROGRESS_BIT;
	mutex_unlock(&v4l2_inst->flush_lock);

	INF("================RINGBUFFER Reset here================\n");
	if (v4l2_inst->input_mode == INPUT_MODE_RING)
		mpq_ring_buffer_reset(v4l2_inst->ringbuf);
	if (down_interruptible(&v4l2_inst->inq_sem))
		return -ERESTARTSYS;
	for (i = 0; i < v4l2_inst->num_input_buffers; i++) {
		if (v4l2_inst->buf_info[OUTPUT_PORT][i].state ==
			MPQ_INPUT_BUFFER_FREE) {
			p_inq_msg = kzalloc(
			   sizeof(struct mpq_inq_msg),
			   GFP_KERNEL);
			if (!p_inq_msg)
				return -ENOMEM;
			p_inq_msg->buf_index = i;
			list_add_tail(&p_inq_msg->list, &v4l2_inst->inq);
			v4l2_inst->buf_info[OUTPUT_PORT][i].state =
				MPQ_INPUT_BUFFER_IN_USE;
		}
	}
	up(&v4l2_inst->inq_sem);
	wake_up(&v4l2_inst->inq_wait);

	p_msg = kzalloc(sizeof(*p_msg), GFP_KERNEL);
	p_msg->type = MPQ_MSG_VIDC_EVENT;
	p_msg->vidc_event.type =
		VIDEO_EVENT_INPUT_FLUSH_DONE;
	p_msg->vidc_event.status =
		VIDEO_STATUS_SUCESS;
	if (down_interruptible(&v4l2_inst->outq_sem))
		return -ERESTARTSYS;
	list_add_tail(&p_msg->list, &v4l2_inst->outq);
	up(&v4l2_inst->outq_sem);
	wake_up(&v4l2_inst->outq_wait);

	DBG("LEAVE mpq_dvb_flush_input\n");
	return rc;
}

static int mpq_dvb_flush_output(
	struct mpq_dvb_video_instance *dvb_video_inst)
{
	int rc = 0;
	struct v4l2_instance *v4l2_inst;
	struct v4l2_decoder_cmd cmd;
	struct mpq_outq_msg *p_msg;
	DBG("ENTER mpq_dvb_flush_output\n");
	if (!dvb_video_inst || !dvb_video_inst->v4l2_inst) {
		ERR("Input parameter is NULL or invalid\n");
		return -EINVAL;
	}
	v4l2_inst = dvb_video_inst->v4l2_inst;
	mutex_lock(&v4l2_inst->flush_lock);
	v4l2_inst->flag &= ~MPQ_DVB_EVENT_FLUSH_DONE_BIT;
	v4l2_inst->flag |= MPQ_DVB_OUTPUT_FLUSH_IN_PROGRESS_BIT;
	mutex_unlock(&v4l2_inst->flush_lock);
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd = V4L2_DEC_QCOM_CMD_FLUSH;
	cmd.flags = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	rc = msm_vidc_decoder_cmd(v4l2_inst->vidc_inst, &cmd);
	if (rc) {
		ERR("DECODER_QCOM_CMD_FLUSH failed\n");
		return rc;
	}
	DBG("Decoder Flush OUTPUT Success\n");
	if (!((v4l2_inst->flag & MPQ_DVB_EVENT_FLUSH_DONE_BIT) &&
		  (v4l2_inst->vidc_ftb == v4l2_inst->vidc_fbd))) {
		wait_event_interruptible(v4l2_inst->msg_wait,
			(v4l2_inst->flag & MPQ_DVB_EVENT_FLUSH_DONE_BIT) &&
			(v4l2_inst->vidc_ftb == v4l2_inst->vidc_fbd));
	}
	DBG("Decoder Flush ALL Success\n");
	/* sanity check: ETB == EBD & FTB == FBD */
	if (v4l2_inst->vidc_ftb == v4l2_inst->vidc_fbd) {
		INF("vidc flush OUTPUT success:FTB(%u) FBD(%u)\n",
			v4l2_inst->vidc_ftb,
			v4l2_inst->vidc_fbd);
	} else {
		INF("vidc flush OUTPUT ERROR: FTB(%u) FBD(%u)\n",
			v4l2_inst->vidc_ftb,
			v4l2_inst->vidc_fbd);
	}
	mutex_lock(&v4l2_inst->flush_lock);
	v4l2_inst->flag &= ~MPQ_DVB_OUTPUT_FLUSH_IN_PROGRESS_BIT;
	mutex_unlock(&v4l2_inst->flush_lock);

	p_msg = kzalloc(sizeof(*p_msg), GFP_KERNEL);
	p_msg->type = MPQ_MSG_VIDC_EVENT;
	p_msg->vidc_event.type =
		VIDEO_EVENT_OUTPUT_FLUSH_DONE;
	p_msg->vidc_event.status =
		VIDEO_STATUS_SUCESS;
	if (down_interruptible(&v4l2_inst->outq_sem))
		return -ERESTARTSYS;
	list_add_tail(&p_msg->list, &v4l2_inst->outq);
	up(&v4l2_inst->outq_sem);
	wake_up(&v4l2_inst->outq_wait);

	DBG("LEAVE mpq_dvb_flush_output\n");
	return rc;
}

static int mpq_dvb_stop_play(
	struct mpq_dvb_video_instance *dvb_video_inst)
{
	int rc = 0;
	struct v4l2_instance *v4l2_inst;
	struct mpq_outq_msg *p_msg;
	DBG("ENTER mpq_dvb_stop_play\n");
	if (!dvb_video_inst || !dvb_video_inst->v4l2_inst) {
		ERR("Input parameter is NULL or invalid\n");
		return -EINVAL;
	}
	v4l2_inst = dvb_video_inst->v4l2_inst;
	if (v4l2_inst->state != MPQ_STATE_RUNNING)
		return 0;

	rc = mpq_dvb_flush(dvb_video_inst);
	if (rc) {
		ERR("Error in flush buffers\n");
		return rc;
	}
	DBG("Decoder Flush DONE\n");
	DBG("Send Decoder Stopped Msg\n");
	p_msg = kzalloc(sizeof(*p_msg), GFP_KERNEL);
	p_msg->type = MPQ_MSG_VIDC_EVENT;
	p_msg->vidc_event.type =
		VIDEO_EVENT_DECODER_STOPPED;
	p_msg->vidc_event.status =
		VIDEO_STATUS_SUCESS;
	if (down_interruptible(&v4l2_inst->outq_sem))
		return -ERESTARTSYS;
	list_add_tail(&p_msg->list, &v4l2_inst->outq);
	up(&v4l2_inst->outq_sem);
	wake_up(&v4l2_inst->outq_wait);
	if (v4l2_inst->input_mode == INPUT_MODE_RING) {
		DBG("Kthread[Input] try to stop...\n");
		if (dvb_video_inst->input_task)
			kthread_stop(dvb_video_inst->input_task);
		DBG("Kthread[Input] stops\n");
		mpq_ring_buffer_flush(v4l2_inst->ringbuf);
	}
	if (dvb_video_inst->source == VIDEO_SOURCE_DEMUX) {
		rc = mpq_dvb_stop_dmx_src(dvb_video_inst);
		if (rc) {
			ERR("Error stop dmx thread\n");
			return rc;
		}
	}
	DBG("LEAVE mpq_dvb_stop_play\n");
	return rc;
}

static int mpq_dvb_stop_decoder(
	struct mpq_dvb_video_instance *dvb_video_inst)
{
	int rc = 0;
	struct v4l2_instance *v4l2_inst;
	DBG("ENTER mpq_dvb_stop_decoder\n");
	if (!dvb_video_inst || !dvb_video_inst->v4l2_inst) {
		ERR("Input parameter is NULL or invalid\n");
		return -EINVAL;
	}

	v4l2_inst = dvb_video_inst->v4l2_inst;

	if (v4l2_inst->state != MPQ_STATE_RUNNING)
		return -EINVAL;

	rc = mpq_dvb_stop_play(dvb_video_inst);
	if (rc) {
		ERR("Error in stopping play\n");
		return -EINVAL;
	}
	rc = msm_vidc_streamoff(v4l2_inst->vidc_inst,
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	if (rc) {
		ERR("Stream Off on CAPTURE_PORT failed\n");
		return rc;
	}
	rc = msm_vidc_streamoff(v4l2_inst->vidc_inst,
			V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	if (rc) {
		ERR("Stream Off on OUTPUT_PORT failed\n");
		return rc;
	}

	v4l2_inst->state = MPQ_STATE_IDLE;
	DBG("LEAVE mpq_dvb_stop_decoder\n");
	return rc;
}

static int mpq_dvb_get_event(
	struct mpq_dvb_video_instance *dvb_video_inst,
	struct video_event *p_ev)
{
	struct v4l2_instance *v4l2_inst;
	struct mpq_outq_msg	*p_outq_msg;
	int rc = 0;
	if (!dvb_video_inst || !dvb_video_inst->v4l2_inst) {
		ERR("Input parameter is NULL or invalid\n");
		return -EINVAL;
	}
	v4l2_inst = dvb_video_inst->v4l2_inst;
	if (list_empty(&v4l2_inst->outq))
		return -EAGAIN;
	if (down_interruptible(&v4l2_inst->outq_sem))
		return -ERESTARTSYS;
	p_outq_msg = list_first_entry(
	   &v4l2_inst->outq, struct mpq_outq_msg, list);
	list_del(&p_outq_msg->list);
	up(&v4l2_inst->outq_sem);
	*p_ev = p_outq_msg->vidc_event;
	kfree(p_outq_msg);
	return rc;
}

static int mpq_dvb_queue_output_buffer(
	struct v4l2_instance *v4l2_inst,
	struct video_data_buffer *p_vid_buf)
{
	int buf_index;
	int i;
	struct buffer_info *pbuf;
	if (!v4l2_inst || !p_vid_buf) {
		ERR("Input parameter is NULL or invalid\n");
		return -EINVAL;
	}
	buf_index = -1;
	for (i = 0; i < v4l2_inst->output_buf_count; i++) {
		pbuf = &(v4l2_inst->buf_info[CAPTURE_PORT][i]);
		if (((u32)p_vid_buf->bufferaddr == pbuf->vaddr) &&
			(p_vid_buf->offset == pbuf->offset)) {
			buf_index = i;
			break;
		}
	}
	if (buf_index == -1) {
		ERR("Could not find video output buffer in the list\n");
		return -EINVAL;
	}
	/* we need to short circuit the qbuf() to avoid race conditions */
	mutex_lock(&v4l2_inst->flush_lock);
	if (v4l2_inst->flag & MPQ_DVB_OUTPUT_FLUSH_IN_PROGRESS_BIT) {
		struct mpq_outq_msg *p_msg;
		mutex_unlock(&v4l2_inst->flush_lock);
		p_msg = kzalloc(sizeof(*p_msg), GFP_KERNEL);
		p_msg->type = MPQ_MSG_OUTPUT_BUFFER_DONE;
		p_msg->vidc_event.type = VIDEO_EVENT_OUTPUT_BUFFER_DONE;
		p_msg->vidc_event.status = VIDEO_STATUS_SUCESS;
		p_msg->vidc_event.timestamp = 0;
		p_msg->vidc_event.u.buffer.bufferaddr = (void *)pbuf->vaddr;
		p_msg->vidc_event.u.buffer.buffer_len = 0;
		p_msg->vidc_event.u.buffer.ion_fd = pbuf->fd;
		p_msg->vidc_event.u.buffer.offset = pbuf->offset;
		p_msg->vidc_event.u.buffer.mmaped_size = 0;
		if (down_interruptible(&v4l2_inst->outq_sem))
			return -ERESTARTSYS;
		list_add_tail(&p_msg->list, &v4l2_inst->outq);
		up(&v4l2_inst->outq_sem);
		wake_up(&v4l2_inst->outq_wait);
		return 0;
	}
	mutex_unlock(&v4l2_inst->flush_lock);
	return mpq_v4l2_queue_output_buffer(v4l2_inst, pbuf);
}

static int mpq_dvb_open_v4l2(
	struct mpq_dvb_video_instance *dvb_video_inst,
	int device_id)
{
	int rc = 0;
	struct v4l2_instance *v4l2_inst;
	struct v4l2_event_subscription sub;
	struct v4l2_fmtdesc fdesc;
	struct v4l2_capability cap;
	int num_of_events;
	int i = 0;
	if (!dvb_video_inst) {
		ERR("Input parameter is NULL or invalid\n");
		return -EINVAL;
	}
	if (dvb_video_inst->v4l2_inst)
		return -EEXIST;
	v4l2_inst = kzalloc(sizeof(struct v4l2_instance), GFP_KERNEL);
	if (!v4l2_inst) {
		ERR("Out of Memory\n");
		rc = -ENOMEM;
		goto fail_create_v4l2;
	}
	v4l2_inst->vidc_inst = msm_vidc_open(MSM_VIDC_CORE_VENUS,
					MSM_VIDC_DECODER);
	if (!v4l2_inst->vidc_inst) {
		ERR("Failed to open VIDC driver\n");
		rc = -EFAULT;
		goto fail_open_vidc;
	}
	memset(&sub, 0, sizeof(sub));
	num_of_events = sizeof(v4l2_sub_event)/sizeof(int);
	for (i = 0; i < num_of_events; i++) {
		sub.type = v4l2_sub_event[i];
		rc = msm_vidc_subscribe_event(v4l2_inst->vidc_inst, &sub);
		if (rc) {
			ERR("Unable to subscribe events for VIDC\n");
			goto fail_subscribe_event;
		}
	}
	rc = msm_vidc_querycap(v4l2_inst->vidc_inst, &cap);
	if (rc) {
		ERR("ERROR in query_cap\n");
		goto fail_open_vidc;
	}

	fdesc.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	for (i = 0;; i++) {
		fdesc.index = i;
		rc = msm_vidc_enum_fmt(v4l2_inst->vidc_inst, &fdesc);
		if (rc)
			break;
		DBG("Enum fmt: description: %s, fmt: %x, flags = %x\n",
			fdesc.description, fdesc.pixelformat, fdesc.flags);
	}
	fdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	for (i = 0;; i++) {
		fdesc.index = i;
		rc = msm_vidc_enum_fmt(v4l2_inst->vidc_inst, &fdesc);
		if (rc)
			break;
		DBG("Enum fmt: description: %s, fmt: %x, flags = %x\n",
			fdesc.description, fdesc.pixelformat, fdesc.flags);
	}
	v4l2_inst->mem_client = msm_vidc_smem_get_client(v4l2_inst->vidc_inst);
	if (!v4l2_inst->mem_client) {
		ERR("Failed to create SMEM Client\n");
		rc = -ENOMEM;
		goto fail_create_mem_client;
	}

	v4l2_inst->playback_mode = VIDEO_PLAYBACK_NORMAL;
	mutex_init(&v4l2_inst->lock);
	mutex_init(&v4l2_inst->flush_lock);
	INIT_LIST_HEAD(&v4l2_inst->msg_queue);
	sema_init(&v4l2_inst->msg_sem, 1);
	INIT_LIST_HEAD(&v4l2_inst->inq);
	sema_init(&v4l2_inst->inq_sem, 1);
	INIT_LIST_HEAD(&v4l2_inst->outq);
	sema_init(&v4l2_inst->outq_sem, 1);
	init_waitqueue_head(&v4l2_inst->msg_wait);
	init_waitqueue_head(&v4l2_inst->inq_wait);
	init_waitqueue_head(&v4l2_inst->outq_wait);
	dvb_video_inst->v4l2_inst = v4l2_inst;
	return 0;
fail_create_mem_client:
	msm_vidc_unsubscribe_event(v4l2_inst->vidc_inst, &sub);
fail_subscribe_event:
	msm_vidc_close(v4l2_inst->vidc_inst);
fail_open_vidc:
	kfree(v4l2_inst);
	v4l2_inst = NULL;
fail_create_v4l2:
	return rc;
}

static int mpq_dvb_close_v4l2(
	struct mpq_dvb_video_instance *dvb_video_inst)
{
	int rc;
	struct v4l2_instance *v4l2_inst;
	struct v4l2_decoder_cmd cmd;
	if (!dvb_video_inst || !dvb_video_inst->v4l2_inst) {
		ERR("Input parameter is NULL or invalid\n");
		return -EINVAL;
	}
	v4l2_inst = dvb_video_inst->v4l2_inst;
	if (v4l2_inst->state == MPQ_STATE_RUNNING)
		mpq_dvb_stop_decoder(dvb_video_inst);

	/* state IDLE --> LOADED */
	if (dvb_video_inst->source == VIDEO_SOURCE_DEMUX)
		mpq_dvb_term_dmx_src(dvb_video_inst);

	if (v4l2_inst->flag & MPQ_DVB_INPUT_BUF_SETUP_BIT) {
		rc = mpq_dvb_free_input_buffers(v4l2_inst);
		if (rc) {
			ERR("Release Input Buffers failed\n");
			return rc;
		}
		if (v4l2_inst->input_mode == INPUT_MODE_RING &&
			v4l2_inst->ringbuf) {
			mpq_ring_buffer_delete(v4l2_inst->ringbuf);
			mpq_unmap_kernel(v4l2_inst->mem_client,
			v4l2_inst->buf_info[OUTPUT_PORT][0].handle);
			v4l2_inst->ringbuf = NULL;
		}
	}

	if (v4l2_inst->flag & MPQ_DVB_OUTPUT_BUF_SETUP_BIT) {
		rc = mpq_dvb_free_output_buffers(v4l2_inst);
		if (rc) {
			ERR("Release Output Buffers failed\n");
			return rc;
		}
	}
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd = V4L2_DEC_CMD_STOP;
	rc = msm_vidc_decoder_cmd(v4l2_inst->vidc_inst, &cmd);
	if (rc) {
		ERR("DECODER_CMD_STOP failed\n");
		return rc;
	}
	v4l2_inst->state = MPQ_STATE_STOPPED;

	DBG("Close MSM VIDC Decoder\n");
	rc = msm_vidc_close(v4l2_inst->vidc_inst);
	kfree(dvb_video_inst->v4l2_inst);
	dvb_video_inst->v4l2_inst = NULL;
	DBG("MSM VIDC Decoder Closed\n");
	return rc;
}

static int mpq_dvb_empty_buffer_done(
	struct mpq_dvb_video_instance *dvb_video_inst)
{
	int rc = 0;
	int buf_index = -1;
	struct v4l2_instance *v4l2_inst;
	struct mpq_inq_msg *p_inq_msg;
	DBG("ENTER mpq_dvb_empty_buffer_done\n");
	if (!dvb_video_inst || !dvb_video_inst->v4l2_inst) {
		ERR("Input parameter is NULL or invalid\n");
		return -EINVAL;
	}
	v4l2_inst = dvb_video_inst->v4l2_inst;

	rc = mpq_v4l2_deque_input_buffer(dvb_video_inst, &buf_index);
	if (rc) {
		ERR("Error dequeue input buffer (EBD)\n");
		return -EFAULT;
	}
	if (buf_index == -1)
		return -EFAULT;
	INF("Empty Buffer Done[%d]\n", buf_index);
	if (v4l2_inst->flag & MPQ_DVB_INPUT_FLUSH_IN_PROGRESS_BIT) {
		v4l2_inst->buf_info[OUTPUT_PORT][buf_index].state =
			MPQ_INPUT_BUFFER_FREE;
		if (v4l2_inst->vidc_etb == v4l2_inst->vidc_ebd) {
			INF("VIDC flushed all input buffers\n");
			wake_up(&v4l2_inst->msg_wait);
		}
	} else {
		p_inq_msg = kzalloc(sizeof(*p_inq_msg), GFP_KERNEL);
		if (!p_inq_msg)
			return -ENOMEM;
		p_inq_msg->buf_index = buf_index;
		if (down_interruptible(&v4l2_inst->inq_sem))
			return -ERESTARTSYS;
		list_add_tail(&p_inq_msg->list, &v4l2_inst->inq);
		up(&v4l2_inst->inq_sem);
		wake_up(&v4l2_inst->inq_wait);
	}
	DBG("LEAVE mpq_dvb_empty_buffer_done\n");
	return 0;
}

static int mpq_dvb_fill_buffer_done(
	struct mpq_dvb_video_instance *dvb_video_inst)
{
	int rc = 0;
	int buf_index = -1;
	struct mpq_outq_msg *p_msg;
	struct buffer_info *pbuf;
	struct v4l2_instance *v4l2_inst = dvb_video_inst->v4l2_inst;
	DBG("EXTRADATA: ENTER mpq_dvb_fill_buffer_done\n");
	rc = mpq_v4l2_deque_output_buffer(v4l2_inst, &buf_index);
	if (rc) {
		ERR("Error dequeue input buffer (EBD)\n");
		return -EFAULT;
	}
	if (buf_index == -1)
		return -EFAULT;
	pbuf = &(v4l2_inst->buf_info[CAPTURE_PORT][buf_index]);
	if (!pbuf->bytesused) {
		INF("This is an empty buffer dequeued\n");
	}
	if (v4l2_inst->flag & MPQ_DVB_OUTPUT_FLUSH_IN_PROGRESS_BIT) {
		if (v4l2_inst->vidc_ftb == v4l2_inst->vidc_fbd) {
			INF("VIDC flushed all output buffers\n");
			wake_up(&v4l2_inst->msg_wait);
		}
	}
	p_msg = kzalloc(sizeof(*p_msg), GFP_KERNEL);
	p_msg->type = MPQ_MSG_OUTPUT_BUFFER_DONE;
	p_msg->vidc_event.type = VIDEO_EVENT_OUTPUT_BUFFER_DONE;
	p_msg->vidc_event.status = VIDEO_STATUS_SUCESS;
	p_msg->vidc_event.timestamp = 0;
	p_msg->vidc_event.u.buffer.bufferaddr = (void *)pbuf->vaddr;
	p_msg->vidc_event.u.buffer.buffer_len = pbuf->bytesused;
	p_msg->vidc_event.u.buffer.ion_fd = pbuf->fd;
	p_msg->vidc_event.u.buffer.offset = pbuf->offset;
	p_msg->vidc_event.u.buffer.mmaped_size = pbuf->bytesused;

	p_msg->vidc_event.u.buffer.pts = pbuf->pts;
	DBG("Video Timestamp is %llu ticks\n", p_msg->vidc_event.u.buffer.pts);
	p_msg->vidc_event.u.buffer.client_data = (void *)pbuf->extradata.uaddr;

	if (down_interruptible(&v4l2_inst->outq_sem))
		return -ERESTARTSYS;
	list_add_tail(&p_msg->list, &v4l2_inst->outq);
	up(&v4l2_inst->outq_sem);
	wake_up(&v4l2_inst->outq_wait);
	DBG("LEAVE mpq_dvb_fill_buffer_done\n");
	return 0;
}

static int mpq_dvb_get_pic_res(
	struct v4l2_instance *v4l2_inst,
	struct video_pic_res *p_pic_res)
{
	int rc;
	struct v4l2_format *pf;
	if (v4l2_inst->flag & MPQ_DVB_OUTPUT_BUF_REQ_BIT) {
		pf = &v4l2_inst->fmt[CAPTURE_PORT];
		memset(&v4l2_inst->fmt[CAPTURE_PORT], 0,
				sizeof(v4l2_inst->fmt[CAPTURE_PORT]));
		v4l2_inst->fmt[CAPTURE_PORT].type =
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		rc = msm_vidc_g_fmt(v4l2_inst->vidc_inst,
				&v4l2_inst->fmt[CAPTURE_PORT]);
		if (rc) {
			ERR("ERROR in get format for CAPTURE_PORT\n");
			return rc;
		}
		p_pic_res->width =
			pf->fmt.pix_mp.width;
		p_pic_res->height =
			pf->fmt.pix_mp.height;
		p_pic_res->stride =
			pf->fmt.pix_mp.plane_fmt[0].bytesperline;
		p_pic_res->scan_lines =
			pf->fmt.pix_mp.plane_fmt[0].reserved[0];
		return 0;
	}
	return -EINVAL;
}

static int mpq_dvb_video_play(struct mpq_dvb_video_instance *dvb_video_inst)
{
	int rc = 0;
	struct v4l2_instance *v4l2_inst;
	if (!dvb_video_inst || !dvb_video_inst->v4l2_inst) {
		ERR("Input parameter is NULL or invalid\n");
		return -EINVAL;
	}
	v4l2_inst = dvb_video_inst->v4l2_inst;
	if (v4l2_inst->state == MPQ_STATE_IDLE) {
		rc = mpq_dvb_start_decoder(dvb_video_inst);
	} else if (v4l2_inst->state == MPQ_STATE_RUNNING) {
		rc = mpq_dvb_start_play(dvb_video_inst);
	} else {
		ERR("Invalid state to start play\n");
		rc = -EINVAL;
	}
	return rc;
}

static int mpq_dvb_video_stop(struct mpq_dvb_video_instance *dvb_video_inst)
{
	return mpq_dvb_stop_play(dvb_video_inst);
}

static int mpq_dvb_clear_buffer(struct mpq_dvb_video_instance *dvb_video_inst)
{
	int rc = 0;
	struct v4l2_instance *v4l2_inst;
	DBG("ENTER mpq_dvb_clear_buffer\n");
	if (!dvb_video_inst || !dvb_video_inst->v4l2_inst) {
		ERR("Input parameter is NULL or invalid\n");
		return -EINVAL;
	}
	v4l2_inst = dvb_video_inst->v4l2_inst;
	if (v4l2_inst->state != MPQ_STATE_RUNNING)
		return 0;

	rc = mpq_dvb_flush(dvb_video_inst);
	if (rc) {
		ERR("Error in flush buffers\n");
		return rc;
	}
	DBG("Decoder Flush DONE\n");
	DBG("LEAVE mpq_dvb_clear_buffer\n");
	return rc;
}
static int mpq_dvb_set_source(
	struct dvb_device *device,
	video_stream_source_t source)
{
	int rc = 0;
	struct mpq_dvb_video_instance *dvb_video_inst =
		(struct mpq_dvb_video_instance *)device->priv;
	if ((VIDEO_SOURCE_MEMORY == source) &&
		(VIDEO_SOURCE_DEMUX == dvb_video_inst->source))
		rc = mpq_dvb_term_dmx_src(dvb_video_inst);
	if (rc) {
		ERR("ERR in mpq_dvb_term_dmx_src()\n");
		return rc;
	}

	dvb_video_inst->source = source;
	if (VIDEO_SOURCE_DEMUX == source)
		rc = mpq_dvb_init_dmx_src(dvb_video_inst, device->id);
	return rc;
}

/*
 * MPQ_DVB_Video Event Handling/Data Input Kthread Functions
 */
static int mpq_dvb_input_data_handler(void *pparam)
{
	struct mpq_dvb_video_instance *dvb_video_inst =
		(struct mpq_dvb_video_instance *)pparam;
	struct v4l2_instance *v4l2_inst;
	struct mpq_inq_msg *p_inq_msg;
	struct mpq_dmx_source *dmx_data;
	struct mpq_pkt_msg *p_pkt_msg;
	u32 next_buf_offset = 0;
	int buf_index = -1;
	struct buffer_info *pbuf;
	int timeout_jiffies = 32;
	int rc;
	int count = 0;

	DBG("START mpq_dvb_input_data_handler\n");
	if (!dvb_video_inst || !dvb_video_inst->v4l2_inst) {
		rc = -EINVAL;
		goto thread_exit;
	}
	v4l2_inst = dvb_video_inst->v4l2_inst;
	if (!v4l2_inst) {
		rc = -EINVAL;
		goto thread_exit;
	}
	do {
		rc = wait_event_interruptible_timeout(
			v4l2_inst->inq_wait,
			!list_empty(&v4l2_inst->inq) || kthread_should_stop(),
			timeout_jiffies);
		if (kthread_should_stop()) {
			DBG("STOP KThread: Input_Data_Handling\n");
			break;
		}
		if (rc < 0) {
			rc = -ERESTARTSYS;
			goto thread_exit;
		}
		if (down_interruptible(&v4l2_inst->inq_sem))
			return -ERESTARTSYS;
		while (!list_empty(&v4l2_inst->inq)) {
			p_inq_msg = list_first_entry(&v4l2_inst->inq,
						 struct mpq_inq_msg,
						 list);
			list_del(&p_inq_msg->list);
			up(&v4l2_inst->inq_sem);
			buf_index = p_inq_msg->buf_index;
			pbuf = &v4l2_inst->buf_info[OUTPUT_PORT][buf_index];
			kfree(p_inq_msg);
			if (dvb_video_inst->source == VIDEO_SOURCE_DEMUX) {
				dmx_data = dvb_video_inst->dmx_src_data;
				while (list_empty(&dmx_data->pkt_queue)) {
					rc = wait_event_interruptible(
					  dmx_data->pkt_wait,
					  !list_empty(&dmx_data->pkt_queue)
					  || kthread_should_stop());
					if (kthread_should_stop()) {
						DBG("STOP KThread:Input\n");
						goto thread_exit;
					}
					if (rc < 0) {
						rc = -ERESTARTSYS;
						goto thread_exit;
					}
				}
				if (down_interruptible(&dmx_data->pkt_sem))
					return -ERESTARTSYS;
				p_pkt_msg = list_first_entry(
				   &dmx_data->pkt_queue,
				   struct mpq_pkt_msg,
				   list);
				list_del(&p_pkt_msg->list);
				up(&dmx_data->pkt_sem);
				DBG("QBuf[%d]Dmx[Off:%u Len:%u pts:%llu]\n",
					buf_index,
					p_pkt_msg->offset,
					p_pkt_msg->len,
					p_pkt_msg->pts);
				if (p_pkt_msg->offset != next_buf_offset) {
					ERR("wrong offset %u should be %u\n",
						p_pkt_msg->offset,
						next_buf_offset);

				}
				pbuf->offset = p_pkt_msg->offset;
				pbuf->bytesused = p_pkt_msg->len;
				pbuf->pts = p_pkt_msg->pts;

				next_buf_offset =
					(p_pkt_msg->offset + p_pkt_msg->len) %
					pbuf->size;
				kfree(p_pkt_msg);
			} else {
				count = mpq_ring_buffer_read_v4l2(
					v4l2_inst->ringbuf,
					pbuf,
					default_input_buf_size);
				if (count < 0) {
					ERR("Error in reading ring_buffer\n");
					rc = -EAGAIN;
					goto thread_exit;
				}
			}
			mutex_lock(&v4l2_inst->flush_lock);
			if (v4l2_inst->flag &
			MPQ_DVB_INPUT_FLUSH_IN_PROGRESS_BIT) {
				pbuf->state = MPQ_INPUT_BUFFER_FREE;
			} else {
				rc = mpq_v4l2_queue_input_buffer(
				v4l2_inst,
				pbuf);
			if (rc) {
				ERR("Unable to queue input buffer\n");
				mutex_unlock(&v4l2_inst->flush_lock);
				goto thread_exit;
			}
			if (!(v4l2_inst->flag &
				  MPQ_DVB_INPUT_STREAMON_BIT)) {
				rc = msm_vidc_streamon(v4l2_inst->vidc_inst,
				V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
				if (rc) {
					ERR("ERROR in Streamon OUTPUT_PORT\n");
					mutex_unlock(&v4l2_inst->flush_lock);
					goto thread_exit;
				}
				v4l2_inst->flag |= MPQ_DVB_INPUT_STREAMON_BIT;
			}
			}
			mutex_unlock(&v4l2_inst->flush_lock);
			if (down_interruptible(&v4l2_inst->inq_sem))
				return -ERESTARTSYS;
		}
		up(&v4l2_inst->inq_sem);
	} while (1);
thread_exit:
	DBG("STOP mpq_dvb_input_data_handler\n");
	dvb_video_inst->input_task = NULL;
	return 0;
}

static int mpq_dvb_demux_data_handler(void *pparam)
{
	struct mpq_dvb_video_instance *dvb_video_inst =
		(struct mpq_dvb_video_instance *)pparam;
	struct mpq_dmx_source *dmx_data = dvb_video_inst->dmx_src_data;
	struct mpq_streambuffer *streambuff;
	struct mpq_streambuffer_packet_header pkt_hdr;
	struct mpq_adapter_video_meta_data meta_data;
	struct mpq_pkt_msg *p_pkt_msg;
	ssize_t indx = -1;
	ssize_t bytes_read = 0;
	size_t pktlen = 0;
	DBG("START mpq_dvb_demux_data_handler\n");
	while (!dmx_data->stream_buffer) {
		wait_event_interruptible(dmx_data->dmx_wait,
			dmx_data->stream_buffer ||
			kthread_should_stop());

		if (kthread_should_stop()) {
			DBG("STOP signal Received\n");
			goto thread_exit;
		}
	}
	streambuff = dmx_data->stream_buffer;
	do {
		wait_event_interruptible(streambuff->packet_data.queue,
			(!dvb_ringbuffer_empty(&streambuff->packet_data) ||
			streambuff->packet_data.error != 0) ||
			kthread_should_stop());
		if (kthread_should_stop()) {
			DBG("STOP signal Received\n");
			goto thread_exit;
		}
		indx = mpq_streambuffer_pkt_next(streambuff, -1, &pktlen);
		if (-1 == indx) {
			DBG("Invalid Index -1\n");
			goto thread_exit;
		}
		DBG("Receive Packets %d for len:%d from Stream Buffer\n",
			indx, pktlen);
		bytes_read = mpq_streambuffer_pkt_read(streambuff, indx,
			&pkt_hdr, (u8 *)&meta_data);
		if (bytes_read >= 0) {
			DBG("Read %d bytes of Packets\n", bytes_read);
		} else {
			WRN("No data in streambuffer read\n");
			goto thread_exit;
		}
		switch (meta_data.packet_type) {
		case DMX_FRAMING_INFO_PACKET:
			DBG("RAW Data Handle:%d Offset:%d Len: %d\n",
				pkt_hdr.raw_data_handle,
				pkt_hdr.raw_data_offset,
				pkt_hdr.raw_data_len);
			if (meta_data.info.framing.pts_dts_info.pts_exist)
				DBG("PTS is %llu\n",
				meta_data.info.framing.pts_dts_info.pts);
			p_pkt_msg = kzalloc(sizeof(*p_pkt_msg), GFP_KERNEL);
			if (!p_pkt_msg)
				return -ENOMEM;
			p_pkt_msg->offset = pkt_hdr.raw_data_offset;
			p_pkt_msg->len = pkt_hdr.raw_data_len;
			if (meta_data.info.framing.pts_dts_info.pts_exist)
				p_pkt_msg->pts =
				(meta_data.info.framing.pts_dts_info.pts);
			else
				p_pkt_msg->pts = 0;
			if (down_interruptible(&dmx_data->pkt_sem))
				return -ERESTARTSYS;
			list_add_tail(&p_pkt_msg->list, &dmx_data->pkt_queue);
			up(&dmx_data->pkt_sem);
			DBG("queue dmx packet(off:%u len:%u)\n",
				p_pkt_msg->offset, p_pkt_msg->len);
			wake_up(&dmx_data->pkt_wait);
			mpq_streambuffer_pkt_dispose(streambuff, indx, 0);
			break;
		case DMX_EOS_PACKET:
			break;
		case DMX_PES_PACKET:
			DBG("PES Payload Data Handle:%d Offset:%d Len: %d\n",
				pkt_hdr.raw_data_handle,
				pkt_hdr.raw_data_offset,
				pkt_hdr.raw_data_len);
			if (meta_data.info.pes.pts_dts_info.pts_exist) {
				DBG("PES Timestamp [STC:%llu PTS:%llu]\n",
					meta_data.info.pes.stc,
					meta_data.info.pes.pts_dts_info.pts);
			} else {
				DBG("PES Timestamp [STC:%llu PTS:N/A]\n",
					meta_data.info.pes.stc);
			}
			p_pkt_msg = kzalloc(sizeof(*p_pkt_msg), GFP_KERNEL);
			if (!p_pkt_msg)
				return -ENOMEM;
			p_pkt_msg->offset = pkt_hdr.raw_data_offset;
			p_pkt_msg->len = pkt_hdr.raw_data_len;
			if (meta_data.info.pes.pts_dts_info.pts_exist)
				p_pkt_msg->pts =
				(meta_data.info.pes.pts_dts_info.pts);
			else
				p_pkt_msg->pts = 0;
			if (down_interruptible(&dmx_data->pkt_sem))
				return -ERESTARTSYS;
			list_add_tail(&p_pkt_msg->list, &dmx_data->pkt_queue);
			up(&dmx_data->pkt_sem);
			DBG("queue dmx packet(off:%u len:%u)\n",
				p_pkt_msg->offset, p_pkt_msg->len);
			wake_up(&dmx_data->pkt_wait);
			mpq_streambuffer_pkt_dispose(streambuff, indx, 0);
			break;
		case DMX_MARKER_PACKET:
			break;
		default:
			break;
		}
	} while (1);
thread_exit:
	DBG("STOP mpq_dvb_demux_data_handler\n");
	dvb_video_inst->demux_task = NULL;
	return 0;
}

static int mpq_dvb_vidc_event_handler(void *pparam)
{
	struct mpq_dvb_video_instance *dvb_video_inst =
		(struct mpq_dvb_video_instance *)pparam;
	struct v4l2_instance *v4l2_inst;
	int mask = 0;
	struct v4l2_event ev;
	bool exit_handler = false;
	DBG("START mpq_dvb_vidc_event_handler\n");
	if (!dvb_video_inst)
		return 0;
	v4l2_inst = dvb_video_inst->v4l2_inst;
	if (!v4l2_inst || !v4l2_inst->vidc_inst)
		return 0;
	while (!exit_handler) {
		mask = msm_vidc_wait(v4l2_inst->vidc_inst);
		if ((mask & POLLOUT) || (mask & POLLWRNORM)) {
			mpq_dvb_empty_buffer_done(dvb_video_inst);
			DBG("mpq_dvb_empty_buffer_done()\n");
		}
		if ((mask & POLLIN) || (mask & POLLRDNORM)) {
			mpq_dvb_fill_buffer_done(dvb_video_inst);
			DBG("mpq_dvb_fill_buffer_done()\n");

		}
		if (mask & POLLPRI) {
			struct mpq_outq_msg *p_msg;
			msm_vidc_dqevent(v4l2_inst->vidc_inst, &ev);
			DBG("VIDC Events: type %d id:%d\n", ev.type, ev.id);
			switch (ev.type) {
			case V4L2_EVENT_MSM_VIDC_CLOSE_DONE:
				DBG("Receive VIDC Close Done\n");
				p_msg = kzalloc(sizeof(*p_msg), GFP_KERNEL);
				p_msg->type = MPQ_MSG_VIDC_EVENT;
				p_msg->vidc_event.type =
					VIDEO_EVENT_DECODER_STOPPED;
				p_msg->vidc_event.status =
					VIDEO_STATUS_SUCESS;
				if (down_interruptible(&v4l2_inst->outq_sem))
					return -ERESTARTSYS;
				list_add_tail(&p_msg->list, &v4l2_inst->outq);
				up(&v4l2_inst->outq_sem);
				wake_up(&v4l2_inst->outq_wait);
				exit_handler = true;
				break;
			case V4L2_PORT_REDO:
				DBG("Receive VIDC Port Change Insufficient\n");
				p_msg = kzalloc(sizeof(*p_msg), GFP_KERNEL);
				p_msg->type = MPQ_MSG_VIDC_EVENT;
				p_msg->vidc_event.type =
					VIDEO_EVENT_SIZE_CHANGED;
				p_msg->vidc_event.status =
					VIDEO_STATUS_SUCESS;
				if (down_interruptible(&v4l2_inst->outq_sem))
					return -ERESTARTSYS;
				list_add_tail(&p_msg->list, &v4l2_inst->outq);
				up(&v4l2_inst->outq_sem);
				wake_up(&v4l2_inst->outq_wait);
				break;
			case V4L2_PORT_CHANGE:
				DBG("Receive VIDC Port Change sufficient\n");
				p_msg = kzalloc(sizeof(*p_msg), GFP_KERNEL);
				p_msg->type = MPQ_MSG_VIDC_EVENT;
				p_msg->vidc_event.type =
					VIDEO_EVENT_SIZE_CHANGED;
				p_msg->vidc_event.status =
					VIDEO_STATUS_NORESOURCE;
				if (down_interruptible(&v4l2_inst->outq_sem))
					return -ERESTARTSYS;
				list_add_tail(&p_msg->list, &v4l2_inst->outq);
				up(&v4l2_inst->outq_sem);
				wake_up(&v4l2_inst->outq_wait);
				break;
			case V4L2_EVENT_MSM_VIDC_FLUSH_DONE:
				DBG("Receive FLUSH DONE\n");
				mutex_lock(&v4l2_inst->flush_lock);
				v4l2_inst->flag |= MPQ_DVB_EVENT_FLUSH_DONE_BIT;
				mutex_unlock(&v4l2_inst->flush_lock);
				wake_up(&v4l2_inst->msg_wait);
				break;
			default:
				WRN("Unknown VIDC event\n");
				break;
			}
		}
		if (mask & POLLERR) {
			DBG("VIDC Error\n");
			break;
		}
	}
	DBG("STOP mpq_dvb_vidc_event_handler\n");
	dvb_video_inst->event_task = NULL;
	return 0;
}

static int mpq_dvb_command_handler(
	struct mpq_dvb_video_instance *dvb_video_inst,
	void *parg)
{
	struct v4l2_instance *v4l2_inst = dvb_video_inst->v4l2_inst;
	struct video_command *cmd = parg;
	int rc = 0;

	if (cmd == NULL)
		return -EINVAL;

	switch (cmd->cmd) {
	case VIDEO_CMD_SET_CODEC:
		DBG("cmd : VIDEO_CMD_SET_CODEC\n");
		rc = mpq_dvb_set_codec(v4l2_inst, cmd->codec);
		break;
	case VIDEO_CMD_GET_CODEC:
		DBG("cmd : VIDEO_CMD_GET_CODEC\n");
		rc = mpq_dvb_get_codec(v4l2_inst, &cmd->codec);
		break;
	case VIDEO_CMD_SET_OUTPUT_FORMAT:
		DBG("cmd : VIDEO_CMD_SET_OUTPUT_FORMAT\n");
		rc = mpq_dvb_set_output_format(v4l2_inst,
							cmd->format);
		break;
	case VIDEO_CMD_GET_OUTPUT_FORMAT:
		DBG("cmd : VIDEO_CMD_GET_OUTPUT_FORMAT\n");
		rc = mpq_dvb_get_output_format(v4l2_inst,
							&cmd->format);
		break;
	case VIDEO_CMD_GET_PIC_RES:
		DBG("cmd : VIDEO_CMD_GET_PIC_RES\n");
		rc = mpq_dvb_get_pic_res(v4l2_inst,
						&cmd->frame_res);
		break;
	case VIDEO_CMD_SET_INPUT_BUFFERS:
		DBG("cmd : VIDEO_CMD_SET_INPUT_BUFFERS\n");
		rc = mpq_dvb_set_input_buf(v4l2_inst, &cmd->buffer);
		break;
	case VIDEO_CMD_SET_OUTPUT_BUFFERS:
		DBG("cmd : VIDEO_CMD_SET_OUTPUT_BUFFERS\n");
		rc = mpq_dvb_set_output_buf(v4l2_inst, &cmd->buffer);
		break;
	case VIDEO_CMD_FREE_INPUT_BUFFERS:
		DBG("cmd : VIDEO_CMD_FREE_INPUT_BUFFERS\n");
		rc = mpq_dvb_free_input_buffers(v4l2_inst);
		break;
	case VIDEO_CMD_FREE_OUTPUT_BUFFERS:
		DBG("cmd : VIDEO_CMD_FREE_OUTPUT_BUFFERS\n");
		rc = mpq_dvb_free_output_buffers(v4l2_inst);
		break;
	case VIDEO_CMD_GET_BUFFER_REQ:
		DBG("cmd : VIDEO_CMD_GET_BUFFER_REQ\n");
		rc = mpq_dvb_get_buffer_req(v4l2_inst, &cmd->buf_req);
		break;
	case VIDEO_CMD_SET_BUFFER_COUNT:
		DBG("cmd : VIDEO_CMD_SET_BUFFER_COUNT\n");
		rc = mpq_dvb_set_input_buf_count(v4l2_inst,
			cmd->buf_req.num_input_buffers);
		rc = mpq_dvb_set_output_buf_count(v4l2_inst,
				cmd->buf_req.num_output_buffers);

		break;
	case VIDEO_CMD_READ_RAW_OUTPUT:
		DBG("cmd : VIDEO_CMD_READ_RAW_OUTPUT\n");
		rc = mpq_dvb_queue_output_buffer(v4l2_inst, &cmd->buffer);
		break;
	case VIDEO_CMD_CLEAR_INPUT_BUFFER:
		DBG("cmd : VIDEO_CMD_CLEAR_INPUT_BUFFER\n");
		rc = mpq_dvb_flush_input(dvb_video_inst);
		break;
	case VIDEO_CMD_CLEAR_OUTPUT_BUFFER:
		DBG("cmd : VIDEO_CMD_CLEAR_OUTPUT_BUFFER\n");
		rc = mpq_dvb_flush_output(dvb_video_inst);
		break;
	case VIDEO_CMD_SET_EXTRADATA_TYPES:
		DBG("cmd : VIDEO_CMD_SET_EXTRADATA_TYPES\n");
		v4l2_inst->extradata_types = cmd->extradata_type;
		break;
	case VIDEO_CMD_SET_EXTRADATA_BUFFER:
		DBG("cmd : VIDEO_CMD_SET_EXTRADATA_BUFFER\n");
		rc = mpq_dvb_set_extradata_buf(
		   v4l2_inst, &cmd->extradata_buffer);
		break;
	case VIDEO_CMD_SET_PLAYBACK_MODE:
		DBG("cmd : VIDEO_CMD_SET_PLAYBACK_MODE\n");
		rc = mpq_dvb_set_decode_mode(
		   dvb_video_inst, cmd->video_mode);
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}

/*
 *	API functions for DVB VIDEO DRIVER interface
 */
static ssize_t mpq_dvb_video_dev_read(
	struct file *filp,
	char __user *buf,
	size_t count,
	loff_t *ppos)
{
	struct dvb_device *device = (struct dvb_device *)filp->private_data;
	struct mpq_dvb_video_instance *dvb_video_inst = NULL;
	struct v4l2_instance *v4l2_inst;
	DBG("ENTER mpq_dvb_video_dev_read: %d\n", count);
	if (!device) {
		ERR("No such device\n");
		return -EINVAL;
	}
	dvb_video_inst = (struct mpq_dvb_video_instance *)device->priv;
	if (!dvb_video_inst || !dvb_video_inst->v4l2_inst) {
		ERR("Invalid video instance\n");
		return -EINVAL;
	}
	v4l2_inst = dvb_video_inst->v4l2_inst;
	if (v4l2_inst->input_mode != INPUT_MODE_RING)
		return -EINVAL;
	if (!v4l2_inst->ringbuf) {
		ERR("Invalid Ring Buffer\n");
		return -EINVAL;
	}
	mpq_ring_buffer_read_user(v4l2_inst->ringbuf, buf, count);
	DBG("LEAVE mpq_dvb_video_dev_read: %d\n", count);
	return count;
}

static ssize_t mpq_dvb_video_dev_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *ppos)
{
	struct dvb_device *device = (struct dvb_device *)filp->private_data;
	struct mpq_dvb_video_instance *dvb_video_inst = NULL;
	struct v4l2_instance *v4l2_inst;
	int rc = 0;
	DBG("ENTER mpq_dvb_video_dev_write: %d\n", count);
	if (!device) {
		ERR("No such device\n");
		return -EINVAL;
	}
	dvb_video_inst = (struct mpq_dvb_video_instance *)device->priv;
	if (!dvb_video_inst || !dvb_video_inst->v4l2_inst) {
		ERR("Invalid video instance\n");
		return -EINVAL;
	}
	v4l2_inst = dvb_video_inst->v4l2_inst;
	if (v4l2_inst->input_mode == INPUT_MODE_RING) {
		if (!v4l2_inst->ringbuf) {
			ERR("Invalid Ring Buffer\n");
			return -EINVAL;
		}
		mpq_ring_buffer_write(v4l2_inst->ringbuf, buf, count);
	} else {
		struct mpq_inq_msg *p_inq_msg;
		int buf_index = 0;
		struct buffer_info *pbuf;
		if (list_empty(&v4l2_inst->inq)) {
			if (wait_event_interruptible(
					v4l2_inst->inq_wait,
					!list_empty(&v4l2_inst->inq)))
				return -ERESTARTSYS;
		}
		if (down_interruptible(&v4l2_inst->inq_sem))
			return -ERESTARTSYS;
		p_inq_msg = list_first_entry(&v4l2_inst->inq,
					 struct mpq_inq_msg,
					 list);
		list_del(&p_inq_msg->list);
		up(&v4l2_inst->inq_sem);
		buf_index = p_inq_msg->buf_index;
		kfree(p_inq_msg);
		pbuf = &(v4l2_inst->buf_info[OUTPUT_PORT][buf_index]);
		rc = copy_from_user((u8 *)pbuf->vaddr, buf, count);
		if (rc) {
			ERR("Error in copy_from_user\n");
			return -EINVAL;
		}
		pbuf->bytesused = count;

		mpq_v4l2_queue_input_buffer(v4l2_inst, pbuf);
		if (!(v4l2_inst->flag & MPQ_DVB_INPUT_STREAMON_BIT)) {
			rc = msm_vidc_streamon(v4l2_inst->vidc_inst,
				V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
			if (rc) {
				ERR("ERROR in Streamon OUTPUT_PORT\n");
				return rc;
			}
			v4l2_inst->flag |= MPQ_DVB_INPUT_STREAMON_BIT;
		}
	}
	DBG("LEAVE mpq_dvb_video_dev_write: %d\n", count);
	return count;
}

static int mpq_dvb_video_dev_open(
	struct inode *inode,
	struct file *file)
{
	int rc;
	struct mpq_dvb_video_instance *dvb_video_inst	= NULL;
	struct dvb_device		 *device	 = NULL;

	rc = dvb_generic_open(inode, file);
	if (rc)
		return rc;

	device	= (struct dvb_device *)file->private_data;
	dvb_video_inst = (struct mpq_dvb_video_instance *)device->priv;


	rc = mpq_dvb_open_v4l2(dvb_video_inst, device->id);
	if (rc) {
		dvb_generic_release(inode, file);
		return rc;
	}

	dvb_video_inst->source = VIDEO_SOURCE_MEMORY;
	return rc;
}

static int mpq_dvb_video_dev_release(
	struct inode *inode,
	struct file *file)
{
	int rc = 0;
	struct mpq_dvb_video_instance *dvb_video_inst	= NULL;
	struct dvb_device		 *device	 = NULL;

	device	= (struct dvb_device *)file->private_data;
	if (device == NULL)
		return -EINVAL;
	dvb_video_inst = (struct mpq_dvb_video_instance *)device->priv;
	if (dvb_video_inst == NULL)
		return -EINVAL;
	rc = mpq_dvb_close_v4l2(dvb_video_inst);
	if (rc)
		ERR("Error in closing V4L2 driver\n");
	dvb_generic_release(inode, file);
	return rc;
}

static unsigned int mpq_dvb_video_dev_poll(
	struct file *file,
	poll_table *wait)
{
	unsigned int mask = 0;
	struct mpq_dvb_video_instance *dvb_video_inst	= NULL;
	struct dvb_device		 *device	 = NULL;
	struct v4l2_instance		*v4l2_inst = NULL;

	device	= (struct dvb_device *)file->private_data;
	if (device == NULL)
		return -EINVAL;
	dvb_video_inst = (struct mpq_dvb_video_instance *)device->priv;
	if (dvb_video_inst == NULL)
		return -EINVAL;
	v4l2_inst = dvb_video_inst->v4l2_inst;
	if (v4l2_inst == NULL)
		return -EINVAL;

	poll_wait(file, &v4l2_inst->outq_wait, wait);
	if (!list_empty(&v4l2_inst->outq))
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static int mpq_dvb_video_dev_ioctl(
	struct file *file,
	unsigned int cmd,
	void *parg)
{
	int rc;
	struct mpq_dvb_video_instance *dvb_video_inst	= NULL;
	struct dvb_device		 *device	 = NULL;
	struct v4l2_instance		*v4l2_inst = NULL;

	device	= (struct dvb_device *)file->private_data;
	if (device == NULL)
		return -EINVAL;
	dvb_video_inst = (struct mpq_dvb_video_instance *)device->priv;
	if (dvb_video_inst == NULL)
		return -EINVAL;
	v4l2_inst = dvb_video_inst->v4l2_inst;
	if (v4l2_inst == NULL)
		return -EINVAL;

	switch (cmd) {
	case VIDEO_PLAY:
		DBG("ioctl : VIDEO_PLAY\n");
		rc = mpq_dvb_video_play(dvb_video_inst);
		break;
	case VIDEO_STOP:
		DBG("ioctl : VIDEO_STOP\n");
		rc = mpq_dvb_video_stop(dvb_video_inst);
		break;
	case VIDEO_FREEZE:
		DBG("ioctl : VIDEO_FREEZE\n");
		break;
	case VIDEO_CONTINUE:
		DBG("ioctl : VIDEO_CONTINUE\n");
		break;
	case VIDEO_CLEAR_BUFFER:
		DBG("ioctl : VIDEO_CLEAR_BUFFER\n");
		rc = mpq_dvb_clear_buffer(dvb_video_inst);
		break;
	case VIDEO_COMMAND:
	case VIDEO_TRY_COMMAND:
		DBG("ioctl : VIDEO_COMMAND\n");
		rc = mpq_dvb_command_handler(dvb_video_inst, parg);
		break;
	case VIDEO_GET_FRAME_RATE:
		DBG("ioctl : VIDEO_GET_FRAME_RATE\n");
		break;
	case VIDEO_GET_EVENT:
		DBG("ioctl : VIDEO_GET_EVENT\n");
		rc = mpq_dvb_get_event(dvb_video_inst,
				(struct video_event *)parg);
		break;
	case VIDEO_SELECT_SOURCE:
		DBG("ioctl : VIDEO_SELECT_SOURCE\n");
		rc = mpq_dvb_set_source(device,
				(video_stream_source_t)parg);
		break;
	default:
		ERR("Invalid IOCTL\n");
		rc = -EINVAL;
	}
	return rc;
}

/************************************************************
 *  DVB VIDEO Driver Registration.						  *
 ************************************************************/
static const struct file_operations mpq_dvb_video_dev_fops = {
	.owner		= THIS_MODULE,
	.read		= mpq_dvb_video_dev_read,
	.write		= mpq_dvb_video_dev_write,
	.unlocked_ioctl	= dvb_generic_ioctl,
	.open		= mpq_dvb_video_dev_open,
	.release	= mpq_dvb_video_dev_release,
	.poll		= mpq_dvb_video_dev_poll,
	.llseek		= noop_llseek,
};

static const struct dvb_device mpq_dvb_video_device_ctrl = {
	.priv		= NULL,
	.users		= 4,
	.readers	= 4,	/* arbitrary */
	.writers	= 4,
	.fops		= &mpq_dvb_video_dev_fops,
	.kernel_ioctl	= mpq_dvb_video_dev_ioctl,
};

static int __init mpq_dvb_video_init(void)
{
	int rc, i = 0, j;

	dvb_video_device  = kzalloc(
			sizeof(struct mpq_dvb_video_device),
			GFP_KERNEL);
	if (!dvb_video_device) {
		ERR("%s Unable to allocate memory for mpq_dvb_video_dev\n",
				__func__);
		return -ENOMEM;
	}
	dvb_video_device->mpq_adapter = mpq_adapter_get();
	if (!dvb_video_device->mpq_adapter) {
		ERR("%s Unable to get MPQ Adapter\n", __func__);
		rc = -ENODEV;
		goto free_region;
	}
	for (i = 0; i < DVB_MPQ_NUM_VIDEO_DEVICES; i++) {

		rc = dvb_register_device(dvb_video_device->mpq_adapter,
				&dvb_video_device->dev_inst[i].video_dev,
				&mpq_dvb_video_device_ctrl,
				&dvb_video_device->dev_inst[i],
				DVB_DEVICE_VIDEO);

		if (rc) {
			ERR("Failed in %s with at %d return value :%d\n",
				__func__, i, rc);
			goto free_region;
		}

	}
	return 0;

free_region:
	for (j = 0; j < i; j++)
		dvb_unregister_device(
			dvb_video_device->dev_inst[j].video_dev);

	kfree(dvb_video_device);
	return rc;
}

static void __exit mpq_dvb_video_exit(void)
{
	int i;

	for (i = 0; i < DVB_MPQ_NUM_VIDEO_DEVICES; i++)
		dvb_unregister_device(dvb_video_device->dev_inst[i].video_dev);

	mutex_destroy(&dvb_video_device->lock);
	kfree(dvb_video_device);
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MPQ DVB Video driver");
module_param(mpq_debug, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
module_init(mpq_dvb_video_init);
module_exit(mpq_dvb_video_exit);
