/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/types.h>
#include <linux/list.h>
#include <linux/ioctl.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/android_pmem.h>
#include <linux/sched.h>
#include <linux/kthread.h>

#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-msm-mem.h>
#include "wfd-util.h"
#include "mdp-subdev.h"
#include "enc-subdev.h"

#define WFD_VERSION KERNEL_VERSION(0, 0, 1)
#define DEFAULT_WFD_WIDTH 640
#define DEFAULT_WFD_HEIGHT 480
#define MIN_BUF_COUNT 2
#define VENC_INPUT_BUFFERS 3

struct wfd_device {
	struct platform_device *pdev;
	struct v4l2_device v4l2_dev;
	struct video_device *pvdev;
	struct v4l2_subdev mdp_sdev;
	struct v4l2_subdev enc_sdev;
};

struct mem_info {
	u32 fd;
	u32 offset;
};

struct mem_info_entry {
	struct list_head list;
	unsigned long userptr;
	struct mem_info minfo;
};
struct wfd_inst {
	struct vb2_queue vid_bufq;
	spinlock_t inst_lock;
	u32 buf_count;
	struct task_struct *mdp_task;
	void *mdp_inst;
	void *venc_inst;
	u32 height;
	u32 width;
	u32 pixelformat;
	struct list_head minfo_list;
	bool streamoff;
	u32 input_bufs_allocated;
	u32 input_buf_size;
	u32 out_buf_size;
	struct list_head input_mem_list;
};

struct wfd_vid_buffer {
	struct vb2_buffer    vidbuf;
};

static int wfd_vidbuf_queue_setup(struct vb2_queue *q,
		unsigned int *num_buffers, unsigned int *num_planes,
		unsigned long sizes[], void *alloc_ctxs[])
{
	struct file *priv_data = (struct file *)(q->drv_priv);
	struct wfd_inst *inst = (struct wfd_inst *)priv_data->private_data;
	unsigned long flags;
	int i;

	WFD_MSG_DBG("In %s\n", __func__);
	if (num_buffers == NULL || num_planes == NULL)
		return -EINVAL;

	*num_planes = 1;
	spin_lock_irqsave(&inst->inst_lock, flags);
	for (i = 0; i < *num_planes; ++i) {
		sizes[i] = inst->out_buf_size;
		alloc_ctxs[i] = inst;
	}
	spin_unlock_irqrestore(&inst->inst_lock, flags);

	return 0;
}

void wfd_vidbuf_wait_prepare(struct vb2_queue *q)
{
}
void wfd_vidbuf_wait_finish(struct vb2_queue *q)
{
}

int wfd_allocate_input_buffers(struct wfd_device *wfd_dev,
			struct wfd_inst *inst)
{
	int i;
	struct mem_region *mregion;
	int rc;
	unsigned long flags;
	struct mdp_buf_info buf = {0};
	spin_lock_irqsave(&inst->inst_lock, flags);
	if (inst->input_bufs_allocated) {
		spin_unlock_irqrestore(&inst->inst_lock, flags);
		return 0;
	}
	inst->input_bufs_allocated = true;
	spin_unlock_irqrestore(&inst->inst_lock, flags);

	for (i = 0; i < VENC_INPUT_BUFFERS; ++i) {
		mregion = kzalloc(sizeof(struct mem_region), GFP_KERNEL);
		mregion->size = inst->input_buf_size;
		rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, ioctl,
				ALLOC_INPUT_BUFFER, (void *)mregion);
		if (rc) {
			WFD_MSG_ERR("Failed to allocate input memory."
				" This error causes memory leak!!!\n");
			break;
		}
		WFD_MSG_DBG("NOTE: paddr = %p, kvaddr = %p\n", mregion->paddr,
					mregion->kvaddr);
		list_add_tail(&mregion->list, &inst->input_mem_list);
		buf.inst = inst->mdp_inst;
		buf.cookie = mregion;
		buf.kvaddr = (u32) mregion->kvaddr;
		buf.paddr = (u32) mregion->paddr;
		rc = v4l2_subdev_call(&wfd_dev->mdp_sdev, core, ioctl,
				MDP_Q_BUFFER, (void *)&buf);
		if (rc) {
			WFD_MSG_ERR("Unable to queue the buffer to mdp\n");
			break;
		}
	}
	rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, ioctl,
			ALLOC_RECON_BUFFERS, NULL);
	if (rc)
		WFD_MSG_ERR("Failed to allocate recon buffers\n");

	return rc;
}
void wfd_free_input_buffers(struct wfd_device *wfd_dev,
			struct wfd_inst *inst)
{
	struct list_head *ptr, *next;
	struct mem_region *mregion;
	unsigned long flags;
	int rc = 0;
	spin_lock_irqsave(&inst->inst_lock, flags);
	if (!inst->input_bufs_allocated) {
		spin_unlock_irqrestore(&inst->inst_lock, flags);
		return;
	}
	inst->input_bufs_allocated = false;
	spin_unlock_irqrestore(&inst->inst_lock, flags);
	if (!list_empty(&inst->input_mem_list)) {
		list_for_each_safe(ptr, next,
				&inst->input_mem_list) {
			mregion = list_entry(ptr, struct mem_region,
						list);
			rc = v4l2_subdev_call(&wfd_dev->enc_sdev,
					core, ioctl, FREE_INPUT_BUFFER,
					(void *)mregion);
			if (rc)
				WFD_MSG_ERR("TODO: SOMETHING IS WRONG!!!\n");

			list_del(&mregion->list);
			kfree(mregion);
		}
	}
	rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, ioctl,
			FREE_RECON_BUFFERS, NULL);
	if (rc)
		WFD_MSG_ERR("Failed to free recon buffers\n");
}

struct mem_info *wfd_get_mem_info(struct wfd_inst *inst,
			unsigned long userptr)
{
	struct mem_info_entry *temp;
	struct mem_info *ret = NULL;
	unsigned long flags;
	spin_lock_irqsave(&inst->inst_lock, flags);
	if (!list_empty(&inst->minfo_list)) {
		list_for_each_entry(temp, &inst->minfo_list, list) {
			if (temp && temp->userptr == userptr) {
				ret = &temp->minfo;
				break;
			}
		}
	}
	spin_unlock_irqrestore(&inst->inst_lock, flags);
	return ret;
}
void wfd_put_mem_info(struct wfd_inst *inst,
			struct mem_info *minfo)
{
	struct list_head *ptr, *next;
	struct mem_info_entry *temp;
	unsigned long flags;
	spin_lock_irqsave(&inst->inst_lock, flags);
	if (!list_empty(&inst->minfo_list)) {
		list_for_each_safe(ptr, next,
				&inst->minfo_list) {
			temp = list_entry(ptr, struct mem_info_entry,
						list);
			if (temp && (&temp->minfo == minfo)) {
				list_del(&temp->list);
				kfree(temp);
			}
		}
	}
	spin_unlock_irqrestore(&inst->inst_lock, flags);
}
static void wfd_unregister_out_buf(struct wfd_inst *inst,
		struct mem_info *minfo)
{
	if (!minfo || !inst) {
		WFD_MSG_ERR("Invalid arguments\n");
		return;
	}
	wfd_put_mem_info(inst, minfo);
}
int wfd_vidbuf_buf_init(struct vb2_buffer *vb)
{
	int rc = 0;
	struct vb2_queue *q = vb->vb2_queue;
	struct file *priv_data = (struct file *)(q->drv_priv);
	struct wfd_inst *inst = (struct wfd_inst *)priv_data->private_data;
	struct wfd_device *wfd_dev =
		(struct wfd_device *)video_drvdata(priv_data);
	struct mem_info *minfo = vb2_plane_cookie(vb, 0);
	struct mem_region mregion;
	mregion.fd = minfo->fd;
	mregion.offset = minfo->offset;
	mregion.cookie = (u32)vb;
	/*TODO: should be fixed in kernel 3.2*/
	mregion.size =  inst->out_buf_size;

	if (inst && !inst->vid_bufq.streaming) {
		rc = wfd_allocate_input_buffers(wfd_dev, inst);
		if (rc) {
			WFD_MSG_ERR("Failed to allocate input buffers\n");
			goto err;
		}
		rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, ioctl,
				SET_OUTPUT_BUFFER, (void *)&mregion);
		if (rc) {
			WFD_MSG_ERR("Failed to set output buffer\n");
			goto free_input_bufs;
		}
	}
	return rc;
free_input_bufs:
	wfd_free_input_buffers(wfd_dev, inst);
err:
	return rc;
}

int wfd_vidbuf_buf_prepare(struct vb2_buffer *vb)
{
	return 0;
}

int wfd_vidbuf_buf_finish(struct vb2_buffer *vb)
{
	return 0;
}

void wfd_vidbuf_buf_cleanup(struct vb2_buffer *vb)
{
	int rc = 0;
	struct vb2_queue *q = vb->vb2_queue;
	struct file *priv_data = (struct file *)(q->drv_priv);
	struct wfd_device *wfd_dev =
		(struct wfd_device *)video_drvdata(priv_data);
	struct wfd_inst *inst = (struct wfd_inst *)priv_data->private_data;
	struct mem_info *minfo = vb2_plane_cookie(vb, 0);
	struct mem_region mregion;
	mregion.fd = minfo->fd;
	mregion.offset = minfo->offset;
	mregion.cookie = (u32)vb;
	mregion.size =  inst->out_buf_size;

	rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, ioctl,
			FREE_OUTPUT_BUFFER, (void *)&mregion);
	if (rc)
		WFD_MSG_ERR("Failed to free output buffer\n");
	wfd_unregister_out_buf(inst, minfo);
	wfd_free_input_buffers(wfd_dev, inst);
}
static int mdp_output_thread(void *data)
{
	int rc = 0;
	struct file *filp = (struct file *)data;
	struct wfd_inst *inst = filp->private_data;
	struct wfd_device *wfd_dev =
		(struct wfd_device *)video_drvdata(filp);
	struct mdp_buf_info obuf = {inst->mdp_inst, 0, 0, 0};
	while (!kthread_should_stop()) {
		WFD_MSG_DBG("waiting for mdp output\n");
		rc = v4l2_subdev_call(&wfd_dev->mdp_sdev,
			core, ioctl, MDP_DQ_BUFFER, (void *)&obuf);

		if (rc) {
			WFD_MSG_ERR("Either streamoff called or"
						" MDP REPORTED ERROR\n");
			break;
		}

		rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, ioctl,
			ENCODE_FRAME, obuf.cookie);
		if (rc) {
			WFD_MSG_ERR("Failed to encode frame\n");
			break;
		}
	}
	WFD_MSG_DBG("Exiting the thread\n");
	return rc;
}

int wfd_vidbuf_start_streaming(struct vb2_queue *q)
{
	struct file *priv_data = (struct file *)(q->drv_priv);
	struct wfd_device *wfd_dev =
		(struct wfd_device *)video_drvdata(priv_data);
	struct wfd_inst *inst = (struct wfd_inst *)priv_data->private_data;
	int rc = 0;

	rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, ioctl,
			ENCODE_START, (void *)inst->venc_inst);
	if (rc) {
		WFD_MSG_ERR("Failed to start encoder\n");
		goto err;
	}

	inst->mdp_task = kthread_run(mdp_output_thread, priv_data,
				"mdp_output_thread");
	if (IS_ERR(inst->mdp_task)) {
		rc = PTR_ERR(inst->mdp_task);
		goto err;
	}
	rc = v4l2_subdev_call(&wfd_dev->mdp_sdev, core, ioctl,
			 MDP_START, (void *)inst->mdp_inst);
	if (rc)
		WFD_MSG_ERR("Failed to start MDP\n");
err:
	return rc;
}

int wfd_vidbuf_stop_streaming(struct vb2_queue *q)
{
	struct file *priv_data = (struct file *)(q->drv_priv);
	struct wfd_device *wfd_dev =
		(struct wfd_device *)video_drvdata(priv_data);
	struct wfd_inst *inst = (struct wfd_inst *)priv_data->private_data;
	int rc = 0;
	rc = v4l2_subdev_call(&wfd_dev->mdp_sdev, core, ioctl,
			 MDP_STOP, (void *)inst->mdp_inst);
	if (rc)
		WFD_MSG_ERR("Failed to stop MDP\n");

	kthread_stop(inst->mdp_task);
	rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, ioctl,
			ENCODE_STOP, (void *)inst->venc_inst);
	if (rc)
		WFD_MSG_ERR("Failed to stop encoder\n");

	return rc;
}

void wfd_vidbuf_buf_queue(struct vb2_buffer *vb)
{
	int rc = 0;
	struct vb2_queue *q = vb->vb2_queue;
	struct file *priv_data = (struct file *)(q->drv_priv);
	struct wfd_device *wfd_dev =
		(struct wfd_device *)video_drvdata(priv_data);
	struct wfd_inst *inst = (struct wfd_inst *)priv_data->private_data;
	struct mem_region mregion;
	struct mem_info *minfo = vb2_plane_cookie(vb, 0);
	mregion.fd = minfo->fd;
	mregion.offset = minfo->offset;
	mregion.cookie = (u32)vb;
	mregion.size =  inst->out_buf_size;
	rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, ioctl,
			FILL_OUTPUT_BUFFER, (void *)&mregion);
	if (rc) {
		WFD_MSG_ERR("Failed to fill output buffer\n");
	}
}

static struct vb2_ops wfd_vidbuf_ops = {
	.queue_setup = wfd_vidbuf_queue_setup,

	.wait_prepare = wfd_vidbuf_wait_prepare,
	.wait_finish = wfd_vidbuf_wait_finish,

	.buf_init = wfd_vidbuf_buf_init,
	.buf_prepare = wfd_vidbuf_buf_prepare,
	.buf_finish = wfd_vidbuf_buf_finish,
	.buf_cleanup = wfd_vidbuf_buf_cleanup,

	.start_streaming = wfd_vidbuf_start_streaming,
	.stop_streaming = wfd_vidbuf_stop_streaming,

	.buf_queue = wfd_vidbuf_buf_queue,
};

static const struct v4l2_subdev_core_ops mdp_subdev_core_ops = {
	.init = mdp_init,
	.ioctl = mdp_ioctl,
};

static const struct v4l2_subdev_ops mdp_subdev_ops = {
	.core = &mdp_subdev_core_ops,
};

static const struct v4l2_subdev_core_ops enc_subdev_core_ops = {
	.init = venc_init,
	.load_fw = venc_load_fw,
	.ioctl = venc_ioctl,
};

static const struct v4l2_subdev_ops enc_subdev_ops = {
	.core = &enc_subdev_core_ops,
}
;
static int wfdioc_querycap(struct file *filp, void *fh,
		struct v4l2_capability *cap) {
	WFD_MSG_DBG("wfdioc_querycap: E\n");
	memset(cap, 0, sizeof(struct v4l2_capability));
	strlcpy(cap->driver, "wifi-display", sizeof(cap->driver));
	strlcpy(cap->card, "msm", sizeof(cap->card));
	cap->version = WFD_VERSION;
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
	WFD_MSG_DBG("wfdioc_querycap: X\n");
	return 0;
}
static int wfdioc_g_fmt(struct file *filp, void *fh,
			struct v4l2_format *fmt)
{
	struct wfd_inst *inst = filp->private_data;
	unsigned long flags;
	if (!fmt) {
		WFD_MSG_ERR("Invalid argument\n");
		return -EINVAL;
	}
	if (fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		WFD_MSG_ERR("Only V4L2_BUF_TYPE_VIDEO_CAPTURE is supported\n");
		return -EINVAL;
	}
	spin_lock_irqsave(&inst->inst_lock, flags);
	fmt->fmt.pix.width = inst->width;
	fmt->fmt.pix.height = inst->height;
	fmt->fmt.pix.pixelformat = inst->pixelformat;
	fmt->fmt.pix.sizeimage = inst->out_buf_size;
	fmt->fmt.pix.priv = 0;
	spin_unlock_irqrestore(&inst->inst_lock, flags);
	return 0;
}
static int wfdioc_s_fmt(struct file *filp, void *fh,
			struct v4l2_format *fmt)
{
	int rc = 0;
	struct wfd_inst *inst = filp->private_data;
	struct wfd_device *wfd_dev = video_drvdata(filp);
	struct mdp_prop prop;
	unsigned long flags;
	struct bufreq breq;
	if (!fmt) {
		WFD_MSG_ERR("Invalid argument\n");
		return -EINVAL;
	}
	if (fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE ||
		fmt->fmt.pix.pixelformat != V4L2_PIX_FMT_NV12) {
		WFD_MSG_ERR("Only V4L2_BUF_TYPE_VIDEO_CAPTURE and "
				"V4L2_PIX_FMT_NV12 are supported\n");
		return -EINVAL;
	}
	rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, ioctl, SET_FORMAT,
				(void *)fmt);
	if (rc) {
		WFD_MSG_ERR("Failed to set format on encoder, rc = %d\n", rc);
		goto err;
	}
	breq.count = VENC_INPUT_BUFFERS;
	breq.height = fmt->fmt.pix.height;
	breq.width = fmt->fmt.pix.width;
	rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, ioctl,
			SET_BUFFER_REQ, (void *)&breq);
	if (rc) {
		WFD_MSG_ERR("Failed to set buffer reqs on encoder\n");
		goto err;
	}
	spin_lock_irqsave(&inst->inst_lock, flags);
	inst->input_buf_size = breq.size;
	inst->out_buf_size = fmt->fmt.pix.sizeimage;
	prop.height = inst->height = fmt->fmt.pix.height;
	prop.width = inst->width = fmt->fmt.pix.width;
	prop.inst = inst->mdp_inst;
	spin_unlock_irqrestore(&inst->inst_lock, flags);
	rc = v4l2_subdev_call(&wfd_dev->mdp_sdev, core, ioctl, MDP_SET_PROP,
				(void *)&prop);
	if (rc)
		WFD_MSG_ERR("Failed to set height/width property on mdp\n");
err:
	return rc;
}
static int wfdioc_reqbufs(struct file *filp, void *fh,
		struct v4l2_requestbuffers *b)
{
	struct wfd_inst *inst = filp->private_data;
	struct wfd_device *wfd_dev = video_drvdata(filp);
	unsigned long flags;
	int rc = 0;

	if (b->type != V4L2_CAP_VIDEO_CAPTURE ||
		b->memory != V4L2_MEMORY_USERPTR) {
		WFD_MSG_ERR("Only V4L2_CAP_VIDEO_CAPTURE and "
		"V4L2_MEMORY_USERPTR are supported\n");
		return -EINVAL;
	}
	rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, ioctl,
			GET_BUFFER_REQ, (void *)b);
	if (rc) {
		WFD_MSG_ERR("Failed to get buf reqs from encoder\n");
		goto err;
	}
	spin_lock_irqsave(&inst->inst_lock, flags);
	inst->buf_count = b->count;
	spin_unlock_irqrestore(&inst->inst_lock, flags);
	rc = vb2_reqbufs(&inst->vid_bufq, b);
err:
	return rc;
}
static int wfd_register_out_buf(struct wfd_inst *inst,
		struct v4l2_buffer *b)
{
	struct mem_info_entry *minfo_entry;
	struct mem_info *minfo;
	unsigned long flags;
	if (!b || !inst || !b->reserved) {
		WFD_MSG_ERR("Invalid arguments\n");
		return -EINVAL;
	}
	minfo = wfd_get_mem_info(inst, b->m.userptr);
	if (!minfo) {
		minfo_entry = kzalloc(sizeof(struct mem_info_entry),
				GFP_KERNEL);
		if (copy_from_user(&minfo_entry->minfo, (void *)b->reserved,
					sizeof(struct mem_info))) {
			WFD_MSG_ERR(" copy_from_user failed. Populate"
					" v4l2_buffer->reserved with meminfo\n");
			return -EINVAL;
		}
		minfo_entry->userptr = b->m.userptr;
		spin_lock_irqsave(&inst->inst_lock, flags);
		list_add_tail(&minfo_entry->list, &inst->minfo_list);
		spin_unlock_irqrestore(&inst->inst_lock, flags);
	} else
		WFD_MSG_INFO("Buffer already registered\n");

	return 0;
}
static int wfdioc_qbuf(struct file *filp, void *fh,
		struct v4l2_buffer *b)
{
	int rc = 0;
	struct wfd_inst *inst = filp->private_data;
	if (!inst || !b ||
			(b->index < 0 || b->index >= inst->buf_count)) {
		WFD_MSG_ERR("Invalid input parameters to QBUF IOCTL\n");
		return -EINVAL;
	}
	rc = wfd_register_out_buf(inst, b);
	if (rc) {
		WFD_MSG_ERR("Failed to register buffer\n");
		goto err;
	}
	rc = vb2_qbuf(&inst->vid_bufq, b);
	if (rc)
		WFD_MSG_ERR("Failed to queue buffer\n");
err:
	return rc;
}

static int wfdioc_streamon(struct file *filp, void *fh,
		enum v4l2_buf_type i)
{
	int rc = 0;
	struct wfd_inst *inst = filp->private_data;
	unsigned long flags;
	if (i != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		WFD_MSG_ERR("stream on for buffer type = %d is not "
			"supported.\n", i);
		return -EINVAL;
	}

	spin_lock_irqsave(&inst->inst_lock, flags);
	inst->streamoff = false;
	spin_unlock_irqrestore(&inst->inst_lock, flags);

	rc = vb2_streamon(&inst->vid_bufq, i);
	if (rc) {
		WFD_MSG_ERR("videobuf_streamon failed with err = %d\n", rc);
		goto vidbuf_streamon_failed;
	}
	return rc;

vidbuf_streamon_failed:
	vb2_streamoff(&inst->vid_bufq, i);
	return rc;
}
static int wfdioc_streamoff(struct file *filp, void *fh,
		enum v4l2_buf_type i)
{
	struct wfd_inst *inst = filp->private_data;
	unsigned long flags;
	spin_lock_irqsave(&inst->inst_lock, flags);
	if (inst->streamoff) {
		WFD_MSG_ERR("Module is already in streamoff state\n");
		spin_unlock_irqrestore(&inst->inst_lock, flags);
		return -EINVAL;
	}
	inst->streamoff = true;
	spin_unlock_irqrestore(&inst->inst_lock, flags);
	if (i != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		WFD_MSG_ERR("stream off for buffer type = %d is not "
			"supported.\n", i);
		return -EINVAL;
	}
	WFD_MSG_DBG("Calling videobuf_streamoff\n");
	vb2_streamoff(&inst->vid_bufq, i);
	return 0;
}
static int wfdioc_dqbuf(struct file *filp, void *fh,
		struct v4l2_buffer *b)
{
	struct wfd_inst *inst = filp->private_data;
	WFD_MSG_INFO("Waiting to dequeue buffer\n");
	return vb2_dqbuf(&inst->vid_bufq, b, 0);
}
static int wfdioc_g_ctrl(struct file *filp, void *fh,
					struct v4l2_control *a)
{
	int rc = 0;
	struct wfd_device *wfd_dev = video_drvdata(filp);
	rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core,
			ioctl, GET_PROP, a);
	if (rc)
		WFD_MSG_ERR("Failed to get encoder property\n");
	return rc;
}
static int wfdioc_s_ctrl(struct file *filp, void *fh,
					struct v4l2_control *a)
{
	int rc = 0;
	struct wfd_device *wfd_dev = video_drvdata(filp);
	rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core,
			ioctl, SET_PROP, a);
	if (rc)
		WFD_MSG_ERR("Failed to set encoder property\n");
	return rc;
}
static const struct v4l2_ioctl_ops g_wfd_ioctl_ops = {
	.vidioc_querycap = wfdioc_querycap,
	.vidioc_s_fmt_vid_cap = wfdioc_s_fmt,
	.vidioc_g_fmt_vid_cap = wfdioc_g_fmt,
	.vidioc_reqbufs = wfdioc_reqbufs,
	.vidioc_qbuf = wfdioc_qbuf,
	.vidioc_streamon = wfdioc_streamon,
	.vidioc_streamoff = wfdioc_streamoff,
	.vidioc_dqbuf = wfdioc_dqbuf,
	.vidioc_g_ctrl = wfdioc_g_ctrl,
	.vidioc_s_ctrl = wfdioc_s_ctrl,

};
static int wfd_set_default_properties(struct file *filp)
{
	unsigned long flags;
	struct v4l2_format fmt;
	struct wfd_inst *inst = filp->private_data;
	if (!inst) {
		WFD_MSG_ERR("Invalid argument\n");
		return -EINVAL;
	}
	spin_lock_irqsave(&inst->inst_lock, flags);
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.height = inst->height = DEFAULT_WFD_HEIGHT;
	fmt.fmt.pix.width = inst->width = DEFAULT_WFD_WIDTH;
	fmt.fmt.pix.pixelformat = inst->pixelformat
			= V4L2_PIX_FMT_NV12;
	spin_unlock_irqrestore(&inst->inst_lock, flags);
	wfdioc_s_fmt(filp, filp->private_data, &fmt);
	return 0;
}
void venc_op_buffer_done(void *cookie, u32 status,
			struct vb2_buffer *buf)
{
	WFD_MSG_DBG("yay!! got callback\n");
	vb2_buffer_done(buf, VB2_BUF_STATE_DONE);
}
void venc_ip_buffer_done(void *cookie, u32 status,
			struct mem_region *mregion)
{
	struct file *filp = cookie;
	struct wfd_inst *inst = filp->private_data;
	struct mdp_buf_info buf = {0};
	struct wfd_device *wfd_dev =
		(struct wfd_device *)video_drvdata(filp);
	int rc = 0;
	WFD_MSG_DBG("yay!! got ip callback\n");
	buf.inst = inst->mdp_inst;
	buf.cookie = mregion;
	buf.kvaddr = (u32) mregion->kvaddr;
	buf.paddr = (u32) mregion->paddr;
	rc = v4l2_subdev_call(&wfd_dev->mdp_sdev, core,
			ioctl, MDP_Q_BUFFER, (void *)&buf);
	if (rc)
		WFD_MSG_ERR("Failed to Q buffer to mdp\n");

}
void *wfd_vb2_mem_ops_get_userptr(void *alloc_ctx, unsigned long vaddr,
					unsigned long size, int write)
{
	return wfd_get_mem_info(alloc_ctx, vaddr);
}

void wfd_vb2_mem_ops_put_userptr(void *buf_priv)
{
	/*TODO: Free the list*/
}

void *wfd_vb2_mem_ops_cookie(void *buf_priv)
{
	return buf_priv;
}


static struct vb2_mem_ops wfd_vb2_mem_ops = {
	.get_userptr = wfd_vb2_mem_ops_get_userptr,
	.put_userptr = wfd_vb2_mem_ops_put_userptr,
	.cookie = wfd_vb2_mem_ops_cookie,
};

int wfd_initialize_vb2_queue(struct vb2_queue *q, void *priv)
{
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	q->io_modes = VB2_USERPTR;
	q->ops = &wfd_vidbuf_ops;
	q->mem_ops = &wfd_vb2_mem_ops;
	q->drv_priv = priv;
	return vb2_queue_init(q);
}

static int wfd_open(struct file *filp)
{
	int rc = 0;
	struct wfd_inst *inst;
	struct wfd_device *wfd_dev;
	struct venc_msg_ops vmops;
	WFD_MSG_DBG("wfd_open: E\n");
	wfd_dev = video_drvdata(filp);
	inst = kzalloc(sizeof(struct wfd_inst), GFP_KERNEL);
	if (!inst || !wfd_dev) {
		WFD_MSG_ERR("Could not allocate memory for "
			"wfd instance\n");
		return -ENOMEM;
	}
	filp->private_data = inst;
	spin_lock_init(&inst->inst_lock);
	INIT_LIST_HEAD(&inst->input_mem_list);
	INIT_LIST_HEAD(&inst->minfo_list);
	rc = v4l2_subdev_call(&wfd_dev->mdp_sdev, core, ioctl, MDP_OPEN,
				(void *)&inst->mdp_inst);
	if (rc) {
		WFD_MSG_ERR("Failed to open mdp subdevice: %d\n", rc);
		goto err_mdp_open;
	}

	rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, load_fw);
	if (rc) {
		WFD_MSG_ERR("Failed to load video encoder firmware: %d\n", rc);
		goto err_venc;
	}
	vmops.op_buffer_done = venc_op_buffer_done;
	vmops.ip_buffer_done = venc_ip_buffer_done;
	vmops.cbdata = filp;
	rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, ioctl, OPEN,
				(void *)&vmops);
	if (rc || !vmops.cookie) {
		WFD_MSG_ERR("Failed to open encoder subdevice: %d\n", rc);
		goto err_venc;
	}
	inst->venc_inst = vmops.cookie;

	wfd_initialize_vb2_queue(&inst->vid_bufq, filp);
	wfd_set_default_properties(filp);
	WFD_MSG_DBG("wfd_open: X\n");
	return rc;
err_venc:
	v4l2_subdev_call(&wfd_dev->mdp_sdev, core, ioctl,
				MDP_CLOSE, (void *)inst->mdp_inst);
err_mdp_open:
	kfree(inst);
	return rc;
}

static int wfd_close(struct file *filp)
{
	struct wfd_inst *inst;
	struct wfd_device *wfd_dev;
	int rc = 0;
	wfd_dev = video_drvdata(filp);
	WFD_MSG_DBG("wfd_close: E\n");
	inst = filp->private_data;
	if (inst) {
		wfdioc_streamoff(filp, NULL, V4L2_BUF_TYPE_VIDEO_CAPTURE);
		vb2_queue_release(&inst->vid_bufq);
		rc = v4l2_subdev_call(&wfd_dev->mdp_sdev, core, ioctl,
				MDP_CLOSE, (void *)inst->mdp_inst);
		if (rc)
			WFD_MSG_ERR("Failed to CLOSE mdp subdevice: %d\n", rc);

		rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, ioctl,
				CLOSE, (void *)inst->venc_inst);
		if (rc)
			WFD_MSG_ERR("Failed to CLOSE enc subdev: %d\n", rc);

		kfree(inst);
	}
	WFD_MSG_DBG("wfd_close: X\n");
	return 0;
}
static const struct v4l2_file_operations g_wfd_fops = {
	.owner = THIS_MODULE,
	.open = wfd_open,
	.release = wfd_close,
	.ioctl = video_ioctl2
};
void release_video_device(struct video_device *pvdev)
{

}
static int __devinit __wfd_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct wfd_device *wfd_dev;
	WFD_MSG_DBG("__wfd_probe: E\n");
	wfd_dev = kzalloc(sizeof(*wfd_dev), GFP_KERNEL);
	if (!wfd_dev) {
		WFD_MSG_ERR("Could not allocate memory for "
				"wfd device\n");
		return -ENOMEM;
	}
	pdev->dev.platform_data = (void *) wfd_dev;
	rc = v4l2_device_register(&pdev->dev, &wfd_dev->v4l2_dev);
	if (rc) {
		WFD_MSG_ERR("Failed to register the video device\n");
		goto err_v4l2_registration;
	}
	wfd_dev->pvdev = video_device_alloc();
	if (!wfd_dev->pvdev) {
		WFD_MSG_ERR("Failed to allocate video device\n");
		goto err_video_device_alloc;
	}

	wfd_dev->pvdev->release = release_video_device;
	wfd_dev->pvdev->fops = &g_wfd_fops;
	wfd_dev->pvdev->ioctl_ops = &g_wfd_ioctl_ops;

	rc = video_register_device(wfd_dev->pvdev, VFL_TYPE_GRABBER, -1);
	if (rc) {
		WFD_MSG_ERR("Failed to register the device\n");
		goto err_video_register_device;
	}
	video_set_drvdata(wfd_dev->pvdev, wfd_dev);

	v4l2_subdev_init(&wfd_dev->mdp_sdev, &mdp_subdev_ops);
	strncpy(wfd_dev->mdp_sdev.name, "wfd-mdp", V4L2_SUBDEV_NAME_SIZE);
	rc = v4l2_device_register_subdev(&wfd_dev->v4l2_dev,
						&wfd_dev->mdp_sdev);
	if (rc) {
		WFD_MSG_ERR("Failed to register mdp subdevice: %d\n", rc);
		goto err_mdp_register_subdev;
	}

	v4l2_subdev_init(&wfd_dev->enc_sdev, &enc_subdev_ops);
	strncpy(wfd_dev->enc_sdev.name, "wfd-venc", V4L2_SUBDEV_NAME_SIZE);
	rc = v4l2_device_register_subdev(&wfd_dev->v4l2_dev,
						&wfd_dev->enc_sdev);
	if (rc) {
		WFD_MSG_ERR("Failed to register encoder subdevice: %d\n", rc);
		goto err_venc_register_subdev;
	}
	rc = v4l2_subdev_call(&wfd_dev->enc_sdev, core, init, 0);
	if (rc) {
		WFD_MSG_ERR("Failed to initiate encoder device %d\n", rc);
		goto err_venc_init;
	}

	WFD_MSG_DBG("__wfd_probe: X\n");
	return rc;

err_venc_init:
	v4l2_device_unregister_subdev(&wfd_dev->enc_sdev);
err_venc_register_subdev:
	v4l2_device_unregister_subdev(&wfd_dev->mdp_sdev);
err_mdp_register_subdev:
	video_unregister_device(wfd_dev->pvdev);
err_video_register_device:
	video_device_release(wfd_dev->pvdev);
err_video_device_alloc:
	v4l2_device_unregister(&wfd_dev->v4l2_dev);
err_v4l2_registration:
	kfree(wfd_dev);
	return rc;
}

static int __devexit __wfd_remove(struct platform_device *pdev)
{
	struct wfd_device *wfd_dev;
	wfd_dev = (struct wfd_device *)pdev->dev.platform_data;

	WFD_MSG_DBG("Inside wfd_remove\n");
	if (!wfd_dev) {
		WFD_MSG_ERR("Error removing WFD device");
		return -ENODEV;
	}

	v4l2_device_unregister_subdev(&wfd_dev->mdp_sdev);
	video_unregister_device(wfd_dev->pvdev);
	video_device_release(wfd_dev->pvdev);
	v4l2_device_unregister(&wfd_dev->v4l2_dev);
	kfree(wfd_dev);
	return 0;
}
static struct platform_driver wfd_driver = {
	.probe =  __wfd_probe,
	.remove = __wfd_remove,
	.driver = {
		.name = "msm_wfd",
		.owner = THIS_MODULE,
	}
};

static int __init wfd_init(void)
{
	int rc = 0;
	WFD_MSG_DBG("Calling init function of wfd driver\n");
	rc = platform_driver_register(&wfd_driver);
	if (rc) {
		WFD_MSG_ERR("failed to load the driver\n");
		goto err_platform_registration;
	}
err_platform_registration:
	return rc;
}

static void __exit wfd_exit(void)
{
	WFD_MSG_DBG("wfd_exit: X\n");
	platform_driver_unregister(&wfd_driver);
}

module_init(wfd_init);
module_exit(wfd_exit);
