/* Copyright (c) 2010-2013, The Linux Foundation. All rights reserved.
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
#include <mach/iommu_domains.h>
#include <media/msm_vidc.h>
#include <media/msm_smem.h>
#include "dvbdev.h"
#include "mpq_adapter.h"
#include "mpq_stream_buffer.h"
#include "mpq_dvb_video_internal.h"
/*
 * Allow dev->read() to get the data from the ringbuffer
 * You must not call ioctl(VIDEO_PLAY) if you use the
 * read() for debugging
 */
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
int mpq_debug = 0x000001;/* print out err msg */
static int default_height = 1920;
static int default_width = 1080;
static int default_input_buf_size = 1024*1024;

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
	if (pbuf) {
		pbuf->read_idx = 0;
		pbuf->release_idx = 0;
		pbuf->write_idx = 0;
	}
	DBG("LEAVE mpq_ring_buffer_flush\n");
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
	if (release_offset > pbuf->len)
		goto release_err;
	if (release_offset > pbuf->release_idx)
		release_size = release_offset - pbuf->release_idx;
	if (release_offset < pbuf->release_idx)
		release_size = release_offset + pbuf->len - pbuf->release_idx;
	if (release_offset == pbuf->release_idx)
		release_size = 0;
	if (release_size > mpq_ring_buffer_get_release_size(pbuf))
		goto release_err;
	pbuf->release_idx = release_offset;
	INF("RINGBUFFFER(len %d) Status write:%d read:%d release:%d\n",
		pbuf->len, pbuf->write_idx, pbuf->read_idx, pbuf->release_idx);
	up(&pbuf->sem);
	wake_up(&pbuf->write_wait);
	DBG("LEAVE mpq_ring_buffer_release\n");
	return 0;
release_err:
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
	if (down_interruptible(&pbuf->sem))
		return -ERESTARTSYS;
	while (mpq_ring_buffer_get_write_size(pbuf) < wr_buf_size) {
		up(&pbuf->sem);
		rc = wait_event_interruptible_timeout(pbuf->write_wait,
			mpq_ring_buffer_get_write_size(pbuf) >= wr_buf_size,
			tm_jiffies);
		if (rc < 0)
			return -ERESTARTSYS;
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

static int mpq_ring_buffer_write_demux(
	struct mpq_ring_buffer *pbuf,
	struct mpq_streambuffer *streambuff,
	size_t wr_buf_size)
{
	int timeout_jiffies = 32;
	int rc;
	DBG("ENTER mpq_ring_buffer_write_demux\n");
	if (streambuff == NULL || wr_buf_size > pbuf->len)
		return -EINVAL;
	if (down_interruptible(&pbuf->sem))
		return -ERESTARTSYS;
	while (mpq_ring_buffer_get_write_size(pbuf) < wr_buf_size) {
		up(&pbuf->sem);
		rc = wait_event_interruptible_timeout(
		   pbuf->write_wait,
			mpq_ring_buffer_get_write_size(pbuf) >= wr_buf_size,
			timeout_jiffies);
		if (rc < 0)
			return -ERESTARTSYS;
		if (down_interruptible(&pbuf->sem))
			return -ERESTARTSYS;
	}
	if ((pbuf->write_idx + wr_buf_size) >= pbuf->len) {
		size_t wr_sz1 = pbuf->len - pbuf->write_idx;
		size_t wr_sz2 = wr_buf_size - wr_sz1;
		ssize_t bytes_read = mpq_streambuffer_data_read(
		   streambuff,
			(u8 *)pbuf->buf.kernel_vaddr + pbuf->write_idx,
			wr_sz1);
		if (bytes_read != wr_sz1) {
			WRN("Read StreamBuffer bytes not match\n");
			goto copy_err;
		}
		bytes_read = mpq_streambuffer_data_read(streambuff,
					(u8 *)pbuf->buf.kernel_vaddr,
					wr_sz2);
		if (bytes_read != wr_sz2) {
			WRN("Read StreamBuffer bytes not match\n");
			goto copy_err;
		}
		pbuf->write_idx = wr_sz2;
	} else {
		ssize_t bytes_read = mpq_streambuffer_data_read(
		   streambuff,
			(u8 *)pbuf->buf.kernel_vaddr + pbuf->write_idx,
			wr_buf_size);

		if (bytes_read != wr_buf_size) {
			WRN("Read StreamBuffer bytes not match\n");
			goto copy_err;
		}
		pbuf->write_idx += wr_buf_size;
	}
	INF("RINGBUFFFER(len %d) Status write:%d read:%d release:%d\n",
		pbuf->len, pbuf->write_idx, pbuf->read_idx, pbuf->release_idx);
	up(&pbuf->sem);
	wake_up(&pbuf->read_wait);
	DBG("LEAVE mpq_ring_buffer_write_demux\n");
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
	if (down_interruptible(&pbuf->sem))
		return -ERESTARTSYS;
	while (!mpq_ring_buffer_get_read_size(pbuf)) {
		up(&pbuf->sem);
		rc = wait_event_interruptible_timeout(
		   pbuf->read_wait,
			mpq_ring_buffer_get_read_size(pbuf) ||
		   kthread_should_stop(),
			timeout_jiffies);
		if (rc < 0)
			return -ERESTARTSYS;
		if (kthread_should_stop())
			return -ERESTARTSYS;
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
	if (!v4l2_inst || !pbinfo)
		return -EINVAL;
	DBG("ENTER mpq_v4l2_queue_input_buffer [%d]\n", pbinfo->index);
	memset(&buf, 0, sizeof(buf));
	memset(&plane, 0, sizeof(plane));
	buf.index = pbinfo->index;
	buf.type = pbinfo->buf_type;
	buf.memory = V4L2_MEMORY_USERPTR;
	buf.length = 1;
	plane[0].bytesused = pbinfo->bytesused;
	plane[0].length = pbinfo->size;
	plane[0].m.userptr = (unsigned long)pbinfo->dev_addr;
	plane[0].reserved[0] = pbinfo->fd;
	plane[0].reserved[1] = 0;
	plane[0].data_offset = pbinfo->offset;
	buf.m.planes = plane;
	if (pbinfo->handle) {
		rc = msm_smem_cache_operations(v4l2_inst->mem_client,
			pbinfo->handle, SMEM_CACHE_CLEAN);
		if (rc)
			ERR("[%s] Failed to clean caches: %d\n", __func__, rc);
	}
	rc = msm_vidc_qbuf(v4l2_inst->vidc_inst, &buf);
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
		plane[extra_idx].length = 0;
		plane[extra_idx].reserved[0] = 0;
		plane[extra_idx].reserved[1] = 0;
		plane[extra_idx].data_offset = 0;
		buf.length = v4l2_inst->fmt[CAPTURE_PORT].fmt.pix_mp.num_planes;
	} else {
		buf.length = 1;
	}
	buf.m.planes = plane;

	rc = msm_vidc_qbuf(v4l2_inst->vidc_inst, &buf);
	if (rc)
		ERR("[%s]msm_vidc_qbuf error\n", __func__);
	DBG("LEAVE mpq_v4l2_queue_output_buffer\n");
	return rc;
}

static int mpq_v4l2_deque_input_buffer(
	struct v4l2_instance *v4l2_inst,
	int *p_buf_index)
{
	struct v4l2_buffer buf;
	struct v4l2_plane plane[VIDEO_MAX_PLANES];
	int rc = 0;
	u32 release_offset = 0;
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

	if (rc)
		return rc;
	if (v4l2_inst->input_mode == INPUT_MODE_RING) {
		release_offset = buf.m.planes[0].data_offset;
		rc = mpq_ring_buffer_release(
		   v4l2_inst->ringbuf, release_offset);
		if (rc)
			return rc;
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
	int rc = 0;
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
	if (rc)
		return rc;
	*p_buf_index = buf.index;
	v4l2_inst->buf_info[CAPTURE_PORT][buf.index].bytesused =
		buf.m.planes[0].bytesused;
	if (v4l2_inst->buf_info[CAPTURE_PORT][buf.index].handle) {
		rc = msm_smem_cache_operations(v4l2_inst->mem_client,
			v4l2_inst->buf_info[CAPTURE_PORT][buf.index].handle,
			SMEM_CACHE_INVALIDATE);
		if (rc)
			ERR("[%s]Failed to invalidate caches: %d\n",
				 __func__, rc);
	}
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
	int extra_idx;
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
		extra_idx = EXTRADATA_IDX(
			v4l2_inst->fmt[CAPTURE_PORT].fmt.pix_mp.num_planes);
		plane[extra_idx].length = 0;
		plane[extra_idx].reserved[0] = 0;
		plane[extra_idx].reserved[1] = 0;
		plane[extra_idx].data_offset = 0;
		buf.length = v4l2_inst->fmt[CAPTURE_PORT].fmt.pix_mp.num_planes;
	} else {
		buf.length = 1;
	}
	buf.m.planes = plane;
	if (pbinfo->handle) {
		rc = msm_smem_cache_operations(v4l2_inst->mem_client,
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
	DBG("ENTER mpq_translate_from_dvb_buf\n");
	if (!p_dvb_buf || buf_index >= MAX_NUM_BUFS || port_num >= MAX_PORTS) {
		ERR("[%s]Input parameter is NULL or invalid\n", __func__);
		return -EINVAL;
	}
	pbinfo = &v4l2_inst->buf_info[port_num][buf_index];
	memset(pbinfo, 0, sizeof(*pbinfo));
	pbinfo->index = buf_index;
	pbinfo->vaddr = (u32)p_dvb_buf->bufferaddr;
	pbinfo->buf_offset = p_dvb_buf->offset;
	pbinfo->size = p_dvb_buf->buffer_len;
	pbinfo->fd = p_dvb_buf->ion_fd;
	if (port_num == OUTPUT_PORT)
		pbinfo->buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	else
		pbinfo->buf_type =  V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	handle = msm_smem_user_to_kernel(v4l2_inst->mem_client,
				pbinfo->fd, pbinfo->buf_offset,
				(port_num == OUTPUT_PORT) ?
				HAL_BUFFER_OUTPUT : HAL_BUFFER_INPUT);

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
	return 0;
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
		v4l2_inst->buf_info[OUTPUT_PORT][0].kernel_vaddr =
			(u32)v4l2_inst->buf_info[OUTPUT_PORT][0].handle->kvaddr;
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
				v4l2_inst->state = MPQ_STATE_READY;
			default_input_buf_size = pvideo_buf->buffer_len /
				v4l2_inst->num_input_buffers;
		}
	} else if (v4l2_inst->input_buf_count == v4l2_inst->num_input_buffers) {
		v4l2_inst->flag |= MPQ_DVB_INPUT_BUF_SETUP_BIT;
		if (v4l2_inst->flag & MPQ_DVB_OUTPUT_BUF_SETUP_BIT)
			v4l2_inst->state = MPQ_STATE_READY;
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
				v4l2_inst->state = MPQ_STATE_READY;
		}
	}
	DBG("LEAVE mpq_dvb_set_output_buf\n");
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
	return rc;
}

static int mpq_dvb_get_buffer_req(
	struct v4l2_instance *v4l2_inst,
	struct video_buffer_req *vdec_buf_req)
{
	int rc = 0;
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
		fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_H264;
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
		if (v4l2_inst->input_mode == INPUT_MODE_RING) {
			control.id = V4L2_CID_MPEG_VIDC_VIDEO_ALLOC_MODE;
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
		fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_H264;
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
		v4l2_inst->fmt[CAPTURE_PORT].fmt.pix_mp.height =
			fmt.fmt.pix_mp.height;
		v4l2_inst->fmt[CAPTURE_PORT].fmt.pix_mp.width =
			fmt.fmt.pix_mp.width;
		v4l2_inst->fmt[CAPTURE_PORT].fmt.pix_mp.pixelformat =
			fmt.fmt.pix_mp.pixelformat;
		v4l2_inst->fmt[CAPTURE_PORT].fmt.pix_mp.plane_fmt[0].sizeimage =
			fmt.fmt.pix_mp.plane_fmt[0].sizeimage;

		memset(&v4l2_buffer_req, 0, sizeof(v4l2_buffer_req));
		v4l2_buffer_req.count = v4l2_inst->num_input_buffers;
		v4l2_buffer_req.memory = V4L2_MEMORY_USERPTR;
		v4l2_buffer_req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		rc = msm_vidc_reqbufs(v4l2_inst->vidc_inst, &v4l2_buffer_req);
		if (rc) {
			ERR("ERROR in msm_vidc_reqbufs for OUTPUT_PORT");
			return rc;
		}
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
		vdec_buf_req->num_input_buffers		 =
			v4l2_inst->num_input_buffers;


		vdec_buf_req->output_buf_prop.alignment  = 4096;
		vdec_buf_req->output_buf_prop.buf_poolid = 0;
		vdec_buf_req->output_buf_prop.buf_size	=
		v4l2_inst->fmt[CAPTURE_PORT].fmt.pix_mp.plane_fmt[0].sizeimage;
		vdec_buf_req->num_output_buffers		 =
			v4l2_inst->num_output_buffers;

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
	int device_id = 0;
	struct mpq_inq_msg *p_inq_msg;
	struct v4l2_instance *v4l2_inst;

	if (!dvb_video_inst || !dvb_video_inst->v4l2_inst) {
		ERR("Input parameter is NULL or invalid\n");
		return -EINVAL;
	}
	v4l2_inst = dvb_video_inst->v4l2_inst;
	if ((v4l2_inst->state != MPQ_STATE_READY) &&
		(v4l2_inst->state != MPQ_STATE_IDLE))
		return -EINVAL;
	if (v4l2_inst->state == MPQ_STATE_READY) {
		dvb_video_inst->event_task = kthread_run(
				mpq_dvb_vidc_event_handler,
				(void *)dvb_video_inst,
				vid_thread_names[0]);
		if (!dvb_video_inst->event_task)
			return -ENOMEM;
		if (v4l2_inst->input_mode == INPUT_MODE_RING) {
			dvb_video_inst->input_task = kthread_run(
					mpq_dvb_input_data_handler,
					(void *)dvb_video_inst,
					vid_data_thread_names[device_id]);
			if (!dvb_video_inst->input_task)
				return -ENOMEM;
		}
		rc = msm_vidc_streamon(v4l2_inst->vidc_inst,
				V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
		if (rc) {
			ERR("MSM Vidc streamon->CAPTURE_PORT error\n");
			return rc;
		}
		v4l2_inst->flag |= MPQ_DVB_OUTPUT_STREAMON_BIT;
		v4l2_inst->flag &= ~MPQ_DVB_INPUT_STREAMON_BIT;
	}

	if (down_interruptible(&v4l2_inst->inq_sem))
		return -ERESTARTSYS;
	for (i = 0; i < v4l2_inst->num_input_buffers; i++) {
		p_inq_msg = kzalloc(sizeof(struct mpq_inq_msg), GFP_KERNEL);
		if (!p_inq_msg)
			return -ENOMEM;
		p_inq_msg->buf_index = i;
		list_add_tail(&p_inq_msg->list, &v4l2_inst->inq);
	}
	up(&v4l2_inst->inq_sem);
	wake_up(&v4l2_inst->inq_wait);

	/* maybe we should let user queue the capture_port buffers */
	for (i = 0; i < v4l2_inst->output_buf_count; i++) {
		rc = mpq_v4l2_queue_output_buffer(v4l2_inst,
					&v4l2_inst->buf_info[CAPTURE_PORT][i]);
		if (rc) {
			ERR("Queue Output Buffer error\n");
			return rc;
		}
	}
	v4l2_inst->state = MPQ_STATE_RUNNING;
	return rc;
}

static int mpq_dvb_stop_play(
	struct mpq_dvb_video_instance *dvb_video_inst)
{
	int rc = 0;
	struct v4l2_instance *v4l2_inst;
	struct list_head *ptr, *next, *head;
	struct mpq_inq_msg	*inq_entry;
	struct mpq_outq_msg	*outq_entry;
	struct v4l2_decoder_cmd cmd;
	DBG("ENTER mpq_dvb_stop_play\n");
	if (!dvb_video_inst || !dvb_video_inst->v4l2_inst) {
		ERR("Input parameter is NULL or invalid\n");
		return -EINVAL;
	}
	v4l2_inst = dvb_video_inst->v4l2_inst;
	if (v4l2_inst->state != MPQ_STATE_RUNNING)
		return 0;

	v4l2_inst->flag &= ~MPQ_DVB_FLUSH_DONE_BIT;
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd = V4L2_DEC_QCOM_CMD_FLUSH;
	cmd.flags = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	rc = msm_vidc_decoder_cmd(v4l2_inst->vidc_inst, &cmd);
	if (rc) {
		ERR("DECODER_QCOM_CMD_FLUSH failed\n");
		return rc;
	}
	DBG("Decoder Flush INPUT Success\n");
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd = V4L2_DEC_QCOM_CMD_FLUSH;
	cmd.flags = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	rc = msm_vidc_decoder_cmd(v4l2_inst->vidc_inst, &cmd);
	if (rc) {
		ERR("DECODER_QCOM_CMD_FLUSH failed\n");
		return rc;
	}
	DBG("Decoder Flush OUTPUT Success\n");
	if (down_interruptible(&v4l2_inst->inq_sem))
		return -ERESTARTSYS;
	if (!list_empty(&v4l2_inst->inq)) {
		head = &v4l2_inst->inq;
		list_for_each_safe(ptr, next, head) {
			inq_entry = list_entry(ptr, struct mpq_inq_msg,
					list);
			list_del(&inq_entry->list);
			kfree(inq_entry);
		}
	}
	up(&v4l2_inst->inq_sem);
	if (v4l2_inst->input_mode == INPUT_MODE_RING)
		mpq_ring_buffer_flush(v4l2_inst->ringbuf);
	v4l2_inst->state = MPQ_STATE_IDLE;
	if (down_interruptible(&v4l2_inst->outq_sem))
		return -ERESTARTSYS;
	if (!list_empty(&v4l2_inst->outq)) {
		head = &v4l2_inst->outq;
		list_for_each_safe(ptr, next, head) {
			outq_entry = list_entry(ptr, struct mpq_outq_msg,
					list);
			list_del(&outq_entry->list);
			kfree(outq_entry);
		}
	}
	up(&v4l2_inst->outq_sem);
	DBG("LEAVE mpq_dvb_stop_play\n");
	return rc;
}

static int mpq_dvb_stop_decoder(
	struct mpq_dvb_video_instance *dvb_video_inst)
{
	int rc = 0;
	struct v4l2_instance *v4l2_inst;
	struct list_head *ptr, *next, *head;
	struct mpq_inq_msg	*inq_entry;
	struct mpq_outq_msg	*outq_entry;
	struct v4l2_decoder_cmd cmd;
	DBG("ENTER mpq_dvb_stop_decoder\n");
	if (!dvb_video_inst || !dvb_video_inst->v4l2_inst) {
		ERR("Input parameter is NULL or invalid\n");
		return -EINVAL;
	}

	v4l2_inst = dvb_video_inst->v4l2_inst;
	if ((v4l2_inst->state != MPQ_STATE_RUNNING) &&
		(v4l2_inst->state != MPQ_STATE_IDLE))
		return 0;

	if (dvb_video_inst->source == VIDEO_SOURCE_DEMUX) {
		DBG("Kthread[DEMUX] try to stop...\n");
		if (dvb_video_inst->demux_task)
			kthread_stop(dvb_video_inst->demux_task);
		DBG("Kthread[DEMUX] stops\n");
	}

	if (v4l2_inst->input_mode == INPUT_MODE_RING) {
		DBG("Kthread[Input] try to stop...\n");
		if (dvb_video_inst->input_task)
			kthread_stop(dvb_video_inst->input_task);
		DBG("Kthread[Input] stops\n");
		mpq_ring_buffer_flush(v4l2_inst->ringbuf);
	}

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd = V4L2_DEC_QCOM_CMD_FLUSH;
	cmd.flags = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	rc = msm_vidc_decoder_cmd(v4l2_inst->vidc_inst, &cmd);
	if (rc) {
		ERR("DECODER_QCOM_CMD_FLUSH failed\n");
		return rc;
	}
	DBG("Decoder Flush INPUT Success\n");
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd = V4L2_DEC_QCOM_CMD_FLUSH;
	cmd.flags = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	rc = msm_vidc_decoder_cmd(v4l2_inst->vidc_inst, &cmd);
	if (rc) {
		ERR("DECODER_QCOM_CMD_FLUSH failed\n");
		return rc;
	}
	DBG("Decoder Flush OUTPUT Success\n");
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd = V4L2_DEC_CMD_STOP;
	rc = msm_vidc_decoder_cmd(v4l2_inst->vidc_inst, &cmd);
	if (rc) {
		ERR("DECODER_CMD_STOP failed\n");
		return rc;
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

	if (down_interruptible(&v4l2_inst->inq_sem))
		return -ERESTARTSYS;
	if (!list_empty(&v4l2_inst->inq)) {
		head = &v4l2_inst->inq;
		list_for_each_safe(ptr, next, head) {
			inq_entry = list_entry(ptr, struct mpq_inq_msg,
					list);
			list_del(&inq_entry->list);
			kfree(inq_entry);
		}
	}
	up(&v4l2_inst->inq_sem);
	if (down_interruptible(&v4l2_inst->outq_sem))
		return -ERESTARTSYS;
	if (!list_empty(&v4l2_inst->outq)) {
		head = &v4l2_inst->outq;
		list_for_each_safe(ptr, next, head) {
			outq_entry = list_entry(ptr, struct mpq_outq_msg,
					list);
			list_del(&outq_entry->list);
			kfree(outq_entry);
		}
	}
	up(&v4l2_inst->outq_sem);
	v4l2_inst->state = MPQ_STATE_CLOSED;
	DBG("LEAVE mpq_dvb_stop_decoder\n");
	return rc;
}

static int mpq_dvb_get_event(
	struct mpq_dvb_video_instance *dvb_video_inst,
	struct video_event *p_ev)
{
	struct v4l2_instance *v4l2_inst;
	struct mpq_outq_msg	*p_outq_msg;
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
	return 0;
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
	return mpq_v4l2_queue_output_buffer(v4l2_inst, pbuf);
}

static int mpq_dvb_open_v4l2(
	struct mpq_dvb_video_instance *dvb_video_inst)
{
	int rc = 0;
	struct v4l2_instance *v4l2_inst;
	struct v4l2_event_subscription sub;
	struct v4l2_fmtdesc fdesc;
	struct v4l2_capability cap;
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
	v4l2_inst->vidc_inst = msm_vidc_open(MSM_VIDC_CORE_0, MSM_VIDC_DECODER);
	if (!v4l2_inst->vidc_inst) {
		ERR("Failed to open VIDC driver\n");
		rc = -EFAULT;
		goto fail_open_vidc;
	}
	memset(&sub, 0, sizeof(sub));
	sub.type = V4L2_EVENT_MSM_VIDC_CLOSE_DONE;
	rc = msm_vidc_subscribe_event(v4l2_inst->vidc_inst, &sub);
	if (rc) {
		ERR("Unable to subscribe events for VIDC\n");
		goto fail_subscribe_event;
	}
	sub.type = V4L2_EVENT_MSM_VIDC_PORT_SETTINGS_CHANGED_INSUFFICIENT;
	rc = msm_vidc_subscribe_event(v4l2_inst->vidc_inst, &sub);
	if (rc) {
		ERR("Unable to subscribe events for VIDC\n");
		goto fail_subscribe_event;
	}
	sub.type = V4L2_EVENT_MSM_VIDC_PORT_SETTINGS_CHANGED_SUFFICIENT;
	rc = msm_vidc_subscribe_event(v4l2_inst->vidc_inst, &sub);
	if (rc) {
		ERR("Unable to subscribe events for VIDC\n");
		goto fail_subscribe_event;
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
	v4l2_inst->mem_client = msm_smem_new_client(SMEM_ION,
		msm_vidc_get_resources(v4l2_inst->vidc_inst));
	if (!v4l2_inst->mem_client) {
		ERR("Failed to create SMEM Client\n");
		rc = -ENOMEM;
		goto fail_create_mem_client;
	}
	mutex_init(&v4l2_inst->lock);
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
	struct list_head *ptr, *next;
	struct mpq_msg_q_msg *msg_q_entry;
	struct mpq_inq_msg *inq_entry;
	struct mpq_outq_msg *outq_entry;
	struct v4l2_instance *v4l2_inst;
	int not_run = 0;
	if (!dvb_video_inst || !dvb_video_inst->v4l2_inst) {
		ERR("Input parameter is NULL or invalid\n");
		return -EINVAL;
	}
	v4l2_inst = dvb_video_inst->v4l2_inst;
	mpq_dvb_stop_decoder(dvb_video_inst);
	if (not_run) {
		mutex_lock(&v4l2_inst->lock);
		if (!list_empty(&v4l2_inst->msg_queue)) {
			list_for_each_safe(ptr, next, &v4l2_inst->msg_queue) {
				msg_q_entry = list_entry(ptr,
					struct mpq_msg_q_msg,
					list);
				list_del(&msg_q_entry->list);
				kfree(msg_q_entry);
			}
		}
		if (!list_empty(&v4l2_inst->inq)) {
			list_for_each_safe(ptr, next, &v4l2_inst->inq) {
				inq_entry = list_entry(ptr, struct mpq_inq_msg,
						list);
				list_del(&inq_entry->list);
				kfree(inq_entry);
			}
		}
		if (!list_empty(&v4l2_inst->outq)) {
			list_for_each_safe(ptr, next, &v4l2_inst->outq) {
				outq_entry = list_entry(
				   ptr,
				   struct mpq_outq_msg,
					list);
				list_del(&inq_entry->list);
				kfree(outq_entry);
			}
		}
		mutex_unlock(&v4l2_inst->lock);
	}
	if (v4l2_inst->input_mode == INPUT_MODE_RING) {
		if (v4l2_inst->ringbuf) {
			mpq_ring_buffer_delete(v4l2_inst->ringbuf);
			mpq_unmap_kernel(v4l2_inst->mem_client,
				v4l2_inst->buf_info[OUTPUT_PORT][0].handle);
		}
	}
	DBG("Close MSM MEM Client\n");
	msm_smem_delete_client(v4l2_inst->mem_client);
	DBG("Close MSM VIDC Decoder\n");
	rc = msm_vidc_close(v4l2_inst->vidc_inst);
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

	rc = mpq_v4l2_deque_input_buffer(v4l2_inst, &buf_index);
	if (rc) {
		ERR("Error dequeue input buffer (EBD)\n");
		return -EFAULT;
	}
	if (buf_index == -1)
		return -EFAULT;
	p_inq_msg = kzalloc(sizeof(*p_inq_msg), GFP_KERNEL);
	if (!p_inq_msg)
		return -ENOMEM;
	p_inq_msg->buf_index = buf_index;
	if (down_interruptible(&v4l2_inst->inq_sem))
		return -ERESTARTSYS;
	list_add_tail(&p_inq_msg->list, &v4l2_inst->inq);
	up(&v4l2_inst->inq_sem);
	wake_up(&v4l2_inst->inq_wait);
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
	DBG("ENTER mpq_dvb_fill_buffer_done\n");
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
		return 0;
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

static void mpq_dvb_get_stream_if(
	enum mpq_adapter_stream_if interface_id,
	void *arg)
{
	struct mpq_dvb_video_instance *dvb_video_inst = arg;

	DBG("In mpq_dvb_video_get_stream_if : %d\n", interface_id);

	mpq_adapter_get_stream_if(interface_id,
			&dvb_video_inst->dmx_src_data->stream_buffer);
}

static int mpq_dvb_init_dmx_src(
	struct mpq_dvb_video_instance *dvb_video_inst,
	int device_id)
{
	int rc;
	struct mpq_streambuffer *sbuff;
	DBG("ENTER mpq_dvb_init_dmx_src\n");
	dvb_video_inst->dmx_src_data =  kzalloc(sizeof(struct mpq_dmx_source),
						GFP_KERNEL);
	if (dvb_video_inst->dmx_src_data == NULL)
		return -ENOMEM;

	rc = mpq_adapter_get_stream_if(
		(enum mpq_adapter_stream_if)device_id,
		&dvb_video_inst->dmx_src_data->stream_buffer);

	if (rc) {
		kfree(dvb_video_inst->dmx_src_data);
		ERR("Error in mpq_adapter_get_stream_if()");
		return -ENODEV;
	} else if (dvb_video_inst->dmx_src_data->stream_buffer == NULL) {
		DBG("Stream Buffer is NULL. Resigtering Notifier.\n");
		rc = mpq_adapter_notify_stream_if(
			(enum mpq_adapter_stream_if)device_id,
			mpq_dvb_get_stream_if,
			(void *)dvb_video_inst);
		if (rc) {
			kfree(dvb_video_inst->dmx_src_data);
			return -ENODEV;
		}
	} else {
		DBG("Receive StreamBuffer from Adapter card:%u\n",
			(u32)dvb_video_inst->dmx_src_data->stream_buffer);
		sbuff = dvb_video_inst->dmx_src_data->stream_buffer;
		DBG("RAW DATA Size:%u and Pkt Data Size: %u\n",
			(u32)sbuff->raw_data.size,
			(u32)sbuff->packet_data.size);
	}

	dvb_video_inst->demux_task = kthread_run(
			mpq_dvb_demux_data_handler,	(void *)dvb_video_inst,
			vid_dmx_thread_names[device_id]);

	DBG("LEAVE mpq_dvb_init_dmx_src\n");
	return 0;
}

static int mpq_dvb_term_dmx_src(
	struct mpq_dvb_video_instance *dvb_video_inst,
	int device_id)
{

	struct mpq_dmx_source *dmx_data = dvb_video_inst->dmx_src_data;

	if (NULL == dmx_data)
		return 0;

	kthread_stop(dvb_video_inst->demux_task);
	kfree(dmx_data);
	dvb_video_inst->dmx_src_data = NULL;
	return 0;

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
		rc = mpq_dvb_term_dmx_src(dvb_video_inst, device->id);
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
	int buf_index = -1;
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
			kfree(p_inq_msg);
			count = mpq_ring_buffer_read_v4l2(
				v4l2_inst->ringbuf,
				&v4l2_inst->buf_info[OUTPUT_PORT][buf_index],
				default_input_buf_size);
			if (count < 0) {
				ERR("Error in reading ring_buffer\n");
				rc = -EAGAIN;
				goto thread_exit;
			}
			mpq_v4l2_queue_input_buffer(
				v4l2_inst,
				&v4l2_inst->buf_info[OUTPUT_PORT][buf_index]);
			if (!(v4l2_inst->flag & MPQ_DVB_INPUT_STREAMON_BIT)) {
				rc = msm_vidc_streamon(v4l2_inst->vidc_inst,
					V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
				if (rc) {
					ERR("ERROR in Streamon OUTPUT_PORT\n");
					goto thread_exit;
				}
				v4l2_inst->flag |= MPQ_DVB_INPUT_STREAMON_BIT;
			}
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
	struct v4l2_instance *v4l2_inst = dvb_video_inst->v4l2_inst;
	struct mpq_dmx_source *dmx_data = dvb_video_inst->dmx_src_data;
	struct mpq_streambuffer *streambuff = dmx_data->stream_buffer;
	struct mpq_streambuffer_packet_header pkt_hdr;
	struct mpq_adapter_video_meta_data meta_data;
	ssize_t indx = -1;
	ssize_t bytes_read = 0;
	size_t pktlen = 0;
	DBG("START mpq_dvb_demux_data_handler\n");
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
			DBG("RAW Data Read: %d\n", pkt_hdr.raw_data_len);
			mpq_ring_buffer_write_demux(v4l2_inst->ringbuf,
					streambuff,
					pkt_hdr.raw_data_len);

			mpq_streambuffer_pkt_dispose(streambuff, indx, 0);
			break;
		case DMX_EOS_PACKET:
			break;
		case DMX_PES_PACKET:
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
			DBG("mpq_dvb_empty_buffer_done()\n");
			mpq_dvb_empty_buffer_done(dvb_video_inst);
		}
		if ((mask & POLLIN) || (mask & POLLRDNORM)) {
				DBG("mpq_dvb_fill_buffer_done()\n");
				mpq_dvb_fill_buffer_done(dvb_video_inst);

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
		break;
	case VIDEO_CMD_FREE_OUTPUT_BUFFERS:
		DBG("cmd : VIDEO_CMD_FREE_OUTPUT_BUFFERS\n");
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
		if (cmd->raw.data[0]) {
			rc = mpq_ring_buffer_release(v4l2_inst->ringbuf,
					cmd->raw.data[0]);
		}
		break;
	case VIDEO_CMD_CLEAR_OUTPUT_BUFFER:
		DBG("cmd : VIDEO_CMD_CLEAR_OUTPUT_BUFFER\n");
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


	rc = mpq_dvb_open_v4l2(dvb_video_inst);
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
	int rc;
	struct mpq_dvb_video_instance *dvb_video_inst	= NULL;
	struct dvb_device		 *device	 = NULL;

	device	= (struct dvb_device *)file->private_data;
	if (device == NULL)
		return -EINVAL;
	dvb_video_inst = (struct mpq_dvb_video_instance *)device->priv;
	if (dvb_video_inst == NULL)
		return -EINVAL;

	rc = mpq_dvb_close_v4l2(dvb_video_inst);
	kfree(dvb_video_inst->v4l2_inst);
	dvb_video_inst->v4l2_inst = NULL;
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
		rc = mpq_dvb_start_play(dvb_video_inst);
		break;
	case VIDEO_STOP:
		DBG("ioctl : VIDEO_STOP\n");
		rc = mpq_dvb_stop_play(dvb_video_inst);
		break;
	case VIDEO_FREEZE:
		DBG("ioctl : VIDEO_FREEZE\n");
		break;
	case VIDEO_CONTINUE:
		DBG("ioctl : VIDEO_CONTINUE\n");
		break;
	case VIDEO_CLEAR_BUFFER:
		DBG("ioctl : VIDEO_CLEAR_BUFFER\n");
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
