/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#define WFD_VERSION KERNEL_VERSION(0, 0, 1)
#define DEFAULT_WFD_WIDTH 640
#define DEFAULT_WFD_HEIGHT 480
#define MIN_BUF_COUNT 2

struct wfd_device {
	struct platform_device *pdev;
	struct v4l2_device v4l2_dev;
	struct video_device *pvdev;
	struct v4l2_subdev mdp_sdev;
};

struct mem_info {
	u32 fd;
	u32 offset;
};

struct wfd_inst {
	struct vb2_queue vid_bufq;
	spinlock_t buflock;
	spinlock_t inst_lock;
	u32 buf_count;
	struct task_struct *mdp_task;
	void *mdp_inst;
	u32 height;
	u32 width;
	u32 pixelformat;
	struct mem_info **minfo;
	bool streamoff;
};

struct wfd_vid_buffer {
	struct vb2_buffer    vidbuf;
};

static int wfd_vidbuf_queue_setup(struct vb2_queue *q,
		unsigned int *num_buffers, unsigned int *num_planes,
		unsigned long sizes[], void *alloc_ctxs[])
{
	WFD_MSG_DBG("In %s\n", __func__);

	if (num_buffers == NULL || num_planes == NULL)
		return -EINVAL;

	*num_planes = 1;
	/*MDP outputs in RGB for now;i
	 * make sure it's smaller than VIDEO_MAX_PLANES*/
	sizes[0] = 800*480*2;

	return 0;
}

void wfd_vidbuf_wait_prepare(struct vb2_queue *q)
{
}
void wfd_vidbuf_wait_finish(struct vb2_queue *q)
{
}

int wfd_vidbuf_buf_init(struct vb2_buffer *vb)
{
	int rc = 0;
	struct vb2_queue *q = vb->vb2_queue;
	struct file *priv_data = (struct file *)(q->drv_priv);
	struct wfd_inst *inst = (struct wfd_inst *)priv_data->private_data;
	struct wfd_device *wfd_dev =
		(struct wfd_device *)video_drvdata(priv_data);
	struct mdp_buf_info buf = {
					inst->mdp_inst,
					vb,
					inst->minfo[vb->v4l2_buf.index]->fd,
					inst->minfo[vb->v4l2_buf.index]->offset
					};

	if (inst && !inst->vid_bufq.streaming) {
		rc = v4l2_subdev_call(&wfd_dev->mdp_sdev, core,
				ioctl, MDP_PREPARE_BUF, (void *)&buf);
		if (rc)
			WFD_MSG_ERR("Unable to prepare/register the buffer\n");
	}
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
	struct mdp_buf_info buf = {
					inst->mdp_inst,
					vb,
					inst->minfo[vb->v4l2_buf.index]->fd,
					inst->minfo[vb->v4l2_buf.index]->offset
					};
	WFD_MSG_DBG("Releasing buffer\n");
	rc = v4l2_subdev_call(&wfd_dev->mdp_sdev, core, ioctl,
			 MDP_RELEASE_BUF, (void *)&buf);
	if (rc)
		WFD_MSG_ERR("Failed to release the buffer\n");
}

int wfd_vidbuf_start_streaming(struct vb2_queue *q)
{
	struct file *priv_data = (struct file *)(q->drv_priv);
	struct wfd_device *wfd_dev =
		(struct wfd_device *)video_drvdata(priv_data);
	struct wfd_inst *inst = (struct wfd_inst *)priv_data->private_data;
	int rc = 0;
	rc = v4l2_subdev_call(&wfd_dev->mdp_sdev, core, ioctl,
			 MDP_START, (void *)inst->mdp_inst);
	if (rc)
		WFD_MSG_ERR("Failed to start MDP\n");

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
	struct mdp_buf_info buf = {
					inst->mdp_inst,
					vb,
					inst->minfo[vb->v4l2_buf.index]->fd,
					inst->minfo[vb->v4l2_buf.index]->offset
					};

	WFD_MSG_DBG("Inside wfd_vidbuf_queue\n");
	rc = v4l2_subdev_call(&wfd_dev->mdp_sdev, core, ioctl,
			MDP_Q_BUFFER, (void *)&buf);
	if (rc)
		WFD_MSG_ERR("Failed to call fill this buffer\n");
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
	fmt->fmt.pix.sizeimage = inst->width * inst->height * 2;
	fmt->fmt.pix.bytesperline = inst->width * 2; /*TODO: Needs
							discussion */
	fmt->fmt.pix.field = V4L2_FIELD_NONE;
	fmt->fmt.pix.colorspace = V4L2_COLORSPACE_SRGB; /*TODO: Needs
							discussion*/
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
	if (!fmt) {
		WFD_MSG_ERR("Invalid argument\n");
		return -EINVAL;
	}

	if (fmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE ||
		fmt->fmt.pix.pixelformat != V4L2_PIX_FMT_RGB565) {
		WFD_MSG_ERR("Only V4L2_BUF_TYPE_VIDEO_CAPTURE and "
				"V4L2_PIX_FMT_RGB565 are supported\n");
		return -EINVAL;
	}
	spin_lock_irqsave(&inst->inst_lock, flags);
	prop.height = inst->height = fmt->fmt.pix.height;
	prop.width = inst->width = fmt->fmt.pix.width;
	prop.inst = inst->mdp_inst;
	fmt->fmt.pix.sizeimage = inst->height * inst->width * 2;
	fmt->fmt.pix.field = V4L2_FIELD_NONE;
	fmt->fmt.pix.bytesperline = inst->width * 2; /*TODO: Needs
							 discussion */
	fmt->fmt.pix.colorspace = V4L2_COLORSPACE_SRGB; /*TODO: Needs
						      discussion*/
	spin_unlock_irqrestore(&inst->inst_lock, flags);
	rc = v4l2_subdev_call(&wfd_dev->mdp_sdev, core, ioctl, MDP_SET_PROP,
				(void *)&prop);
	if (rc)
		WFD_MSG_ERR("Failed to set height/width property on mdp\n");
	return rc;
}
static int wfdioc_reqbufs(struct file *filp, void *fh,
		struct v4l2_requestbuffers *b)
{
	int rc = 0;
	struct wfd_inst *inst = filp->private_data;
	unsigned long flags;
	int i;
	if (b->type != V4L2_CAP_VIDEO_CAPTURE ||
		b->memory != V4L2_MEMORY_USERPTR) {
		WFD_MSG_ERR("Only V4L2_CAP_VIDEO_CAPTURE and "
		"V4L2_CAP_VIDEO_CAPTURE are supported\n");
		return -EINVAL;
	}
	if (b->count < MIN_BUF_COUNT)
		b->count = MIN_BUF_COUNT;
	spin_lock_irqsave(&inst->inst_lock, flags);
	if (inst->minfo) {
		for (i = 0; i < inst->buf_count; ++i)
			kfree(inst->minfo[i]);
	}
	kfree(inst->minfo);
	inst->buf_count = b->count;
	inst->minfo = kzalloc(sizeof(struct mem_info *) * inst->buf_count,
						GFP_KERNEL);
	for (i = 0; i < inst->buf_count; ++i)
		inst->minfo[i] = kzalloc(sizeof(struct mem_info), GFP_KERNEL);
	spin_unlock_irqrestore(&inst->inst_lock, flags);
	rc = vb2_reqbufs(&inst->vid_bufq, b);
	if (rc) {
		WFD_MSG_ERR("Failed in videobuf_reqbufs, rc = %d\n", rc);
		spin_lock_irqsave(&inst->inst_lock, flags);
		if (inst->minfo) {
			for (i = 0; i < inst->buf_count; ++i)
				kfree(inst->minfo[i]);
		}
		kfree(inst->minfo);
		inst->minfo = NULL;
		spin_unlock_irqrestore(&inst->inst_lock, flags);
	}
	return rc;
}
static int wfdioc_qbuf(struct file *filp, void *fh,
		struct v4l2_buffer *b)
{
	int rc = 0;
	struct wfd_inst *inst = filp->private_data;

	if (!inst || !b || !b->reserved ||
			(b->index < 0 || b->index >= inst->buf_count)) {
		WFD_MSG_ERR("Invalid input parameters to QBUF IOCTL\n");
		return -EINVAL;
	}
	if (!inst->vid_bufq.streaming) {
		if (copy_from_user(inst->minfo[b->index], (void *)b->reserved,
				sizeof(struct mem_info))) {
			WFD_MSG_ERR(" copy_from_user failed. Populate"
						" v4l2_buffer->reserved with meminfo\n");
			return -EINVAL;
		}
	}
	rc = vb2_qbuf(&inst->vid_bufq, b);
	if (rc)
		WFD_MSG_ERR("Failed to queue buffer\n");
	return rc;
}
static int mdp_output_thread(void *data)
{
	int rc = 0;
	struct file *filp = (struct file *)data;
	struct wfd_inst *inst = filp->private_data;
	struct wfd_device *wfd_dev =
		(struct wfd_device *)video_drvdata(filp);
	struct vb2_buffer *vbuf = NULL;
	struct mdp_buf_info obuf = {inst->mdp_inst, vbuf, 0, 0};
	while (!kthread_should_stop()) {
		rc = v4l2_subdev_call(&wfd_dev->mdp_sdev,
			core, ioctl, MDP_DQ_BUFFER, (void *)&obuf);

		if (rc) {
			WFD_MSG_ERR("Either streamoff called or"
						" MDP REPORTED ERROR\n");
			break;
		} else
			WFD_MSG_DBG("Dequeued buffer successfully\n");

		vbuf = obuf.b;
		vb2_buffer_done(vbuf,
			rc ? VB2_BUF_STATE_ERROR : VB2_BUF_STATE_DONE);
	}
	WFD_MSG_DBG("Exiting the thread\n");
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
	/*TODO: Do we need to lock the instance here*/
	rc = vb2_streamon(&inst->vid_bufq, i);
	if (rc) {
		WFD_MSG_ERR("videobuf_streamon failed with err = %d\n", rc);
		goto vidbuf_streamon_failed;
	}
	inst->mdp_task = kthread_run(mdp_output_thread, filp,
				"mdp_output_thread");
	if (IS_ERR(inst->mdp_task)) {
		rc = PTR_ERR(inst->mdp_task);
		goto mdp_task_failed;
	}
	return rc;
mdp_task_failed:
	vb2_streamoff(&inst->vid_bufq, i);
vidbuf_streamon_failed:
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
	vb2_queue_release(&inst->vid_bufq);
	kthread_stop(inst->mdp_task);
	return 0;
}
static int wfdioc_dqbuf(struct file *filp, void *fh,
		struct v4l2_buffer *b)
{
	struct wfd_inst *inst = filp->private_data;
	WFD_MSG_INFO("Waiting to dequeue buffer\n");
	return vb2_dqbuf(&inst->vid_bufq, b, 0);
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
};
static int wfd_set_default_properties(struct wfd_inst *inst)
{
	unsigned long flags;
	if (!inst) {
		WFD_MSG_ERR("Invalid argument\n");
		return -EINVAL;
	}
	spin_lock_irqsave(&inst->inst_lock, flags);
	inst->height = DEFAULT_WFD_HEIGHT;
	inst->width = DEFAULT_WFD_WIDTH;
	inst->pixelformat = V4L2_PIX_FMT_RGB565;
	spin_unlock_irqrestore(&inst->inst_lock, flags);
	return 0;
}
static int wfd_open(struct file *filp)
{
	int rc = 0;
	struct wfd_inst *inst;
	struct wfd_device *wfd_dev;
	WFD_MSG_DBG("wfd_open: E\n");
	inst = kzalloc(sizeof(struct wfd_inst), GFP_KERNEL);
	if (!inst) {
		WFD_MSG_ERR("Could not allocate memory for "
			"wfd instance\n");
		return -ENOMEM;
	}
	spin_lock_init(&inst->inst_lock);
	spin_lock_init(&inst->buflock);
	wfd_dev = video_drvdata(filp);
	rc = v4l2_subdev_call(&wfd_dev->mdp_sdev, core, ioctl, MDP_OPEN,
				(void *)&inst->mdp_inst);
	if (rc) {
		WFD_MSG_ERR("Failed to open mdp subdevice: %d\n", rc);
		goto err_mdp_open;
	}

	videobuf2_queue_pmem_contig_init(&inst->vid_bufq,
				V4L2_BUF_TYPE_VIDEO_CAPTURE,
				&wfd_vidbuf_ops,
				sizeof(struct wfd_vid_buffer),
				filp);  /*TODO: Check if it needs to be freed*/
	wfd_set_default_properties(inst);
	filp->private_data = inst;
	WFD_MSG_DBG("wfd_open: X\n");
	return rc;
err_mdp_open:
	kfree(inst);
	return rc;
}
static int wfd_close(struct file *filp)
{
	struct wfd_inst *inst;
	struct wfd_device *wfd_dev;
	int rc = 0;
	int k;
	unsigned long flags;
	wfd_dev = video_drvdata(filp);
	WFD_MSG_DBG("wfd_close: E\n");
	inst = filp->private_data;
	if (inst) {
		wfdioc_streamoff(filp, NULL, V4L2_BUF_TYPE_VIDEO_CAPTURE);
		rc = v4l2_subdev_call(&wfd_dev->mdp_sdev, core, ioctl,
				MDP_CLOSE, (void *)inst->mdp_inst);
		if (rc)
			WFD_MSG_ERR("Failed to CLOSE mdp subdevice: %d\n", rc);
		spin_lock_irqsave(&inst->inst_lock, flags);
		if (inst->minfo) {
			for (k = 0; k < inst->buf_count; ++k)
				kfree(inst->minfo[k]);
		}
		kfree(inst->minfo);
		inst->minfo = NULL;
		spin_unlock_irqrestore(&inst->inst_lock, flags);
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
	wfd_dev = kzalloc(sizeof(*wfd_dev), GFP_KERNEL);  /*TODO: Free it*/
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
	WFD_MSG_DBG("__wfd_probe: X\n");
	return rc;
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
