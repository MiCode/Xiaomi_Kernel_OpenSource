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
 *
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <linux/platform_device.h>
#include <linux/memory_alloc.h>

#include <mach/msm_subsystem_map.h>
#include <mach/board.h>
#include <mach/gpio.h>
#include <mach/irqs.h>

#include <media/videobuf2-msm-mem.h>

#include <media/videobuf2-vmalloc.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-common.h>
#include <linux/regulator/consumer.h>
#include <mach/clk.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>

#include <media/vcap_v4l2.h>
#include <media/vcap_fmt.h>
#include "vcap_vc.h"

#define NUM_INPUTS 1
#define MSM_VCAP_DRV_NAME "msm_vcap"

static struct vcap_dev *vcap_ctrl;

static unsigned debug;

#define dprintk(level, fmt, arg...)					\
	do {								\
		if (debug >= level)					\
			printk(KERN_DEBUG "VCAP: " fmt, ## arg);	\
	} while (0)

int get_phys_addr(struct vcap_dev *dev, struct vb2_queue *q,
				  struct v4l2_buffer *b)
{
	struct vb2_buffer *vb;
	struct vcap_buffer *buf;
	unsigned long len, offset;
	int rc;

	if (q->fileio) {
		dprintk(1, "%s: file io in progress\n", __func__);
		return -EBUSY;
	}

	if (b->type != q->type) {
		dprintk(1, "%s: invalid buffer type\n", __func__);
		return -EINVAL;
	}

	if (b->index >= q->num_buffers) {
		dprintk(1, "%s: buffer index out of range\n", __func__);
		return -EINVAL;
	}

	vb = q->bufs[b->index];
	if (NULL == vb) {
		dprintk(1, "%s: buffer is NULL\n", __func__);
		return -EINVAL;
	}

	if (vb->state != VB2_BUF_STATE_DEQUEUED) {
		dprintk(1, "%s: buffer already in use\n", __func__);
		return -EINVAL;
	}

	buf = container_of(vb, struct vcap_buffer, vb);

	buf->ion_handle = ion_import_fd(dev->ion_client, b->m.userptr);
	if (IS_ERR((void *)buf->ion_handle)) {
		pr_err("%s: Could not alloc memory\n", __func__);
		buf->ion_handle = NULL;
		return -ENOMEM;
	}
	rc = ion_phys(dev->ion_client, buf->ion_handle,
			&buf->paddr, (size_t *)&len);
	if (rc < 0) {
		pr_err("%s: Could not get phys addr\n", __func__);
		return -EFAULT;
	}

	offset = b->reserved;
	buf->paddr += offset;
	return 0;
}

void free_ion_handle_work(struct vcap_dev *dev, struct vb2_buffer *vb)
{
	struct vcap_buffer *buf;

	buf = container_of(vb, struct vcap_buffer, vb);
	if (buf->ion_handle == NULL) {
		dprintk(1, "%s: no ION handle to free\n", __func__);
		return;
	}
	buf->paddr = 0;
	ion_free(dev->ion_client, buf->ion_handle);
	buf->ion_handle = NULL;
	return;
}

int free_ion_handle(struct vcap_dev *dev, struct vb2_queue *q,
					 struct v4l2_buffer *b)
{
	struct vb2_buffer *vb;

	if (q->fileio)
		return -EBUSY;

	if (b->type != q->type)
		return -EINVAL;

	if (b->index >= q->num_buffers)
		return -EINVAL;

	vb = q->bufs[b->index];
	if (NULL == vb)
		return -EINVAL;

	free_ion_handle_work(dev, vb);
	return 0;
}

/* Videobuf operations */

static int capture_queue_setup(struct vb2_queue *vq, unsigned int *nbuffers,
				unsigned int *nplanes, unsigned long sizes[],
				void *alloc_ctxs[])
{
	*nbuffers += 2;
	if (*nbuffers > VIDEO_MAX_FRAME)
		return -EINVAL;

	*nplanes = 1;
	return 0;
}

static int capture_buffer_init(struct vb2_buffer *vb)
{
	return 0;
}

static int capture_buffer_prepare(struct vb2_buffer *vb)
{
	return 0;
}

static void capture_buffer_queue(struct vb2_buffer *vb)
{
	struct vcap_client_data *c_data = vb2_get_drv_priv(vb->vb2_queue);
	struct vcap_buffer *buf = container_of(vb, struct vcap_buffer, vb);
	struct vcap_action *vid_vc_action = &c_data->vid_vc_action;
	struct vb2_queue *q = vb->vb2_queue;
	unsigned long flags = 0;

	spin_lock_irqsave(&c_data->cap_slock, flags);
	list_add_tail(&buf->list, &vid_vc_action->active);
	spin_unlock_irqrestore(&c_data->cap_slock, flags);

	if (atomic_read(&c_data->dev->vc_enabled) == 0) {

		if (atomic_read(&q->queued_count) > 1)
			if (vc_hw_kick_off(c_data) == 0)
				atomic_set(&c_data->dev->vc_enabled, 1);
	}
}

static int capture_start_streaming(struct vb2_queue *vq)
{
	struct vcap_client_data *c_data = vb2_get_drv_priv(vq);
	dprintk(2, "VC start streaming\n");
	return vc_start_capture(c_data);
}

static int capture_stop_streaming(struct vb2_queue *vq)
{
	struct vcap_client_data *c_data = vb2_get_drv_priv(vq);
	struct vb2_buffer *vb;

	vc_stop_capture(c_data);

	while (!list_empty(&c_data->vid_vc_action.active)) {
		struct vcap_buffer *buf;
		buf = list_entry(c_data->vid_vc_action.active.next,
			struct vcap_buffer, list);
		list_del(&buf->list);
		vb2_buffer_done(&buf->vb, VB2_BUF_STATE_ERROR);
	}

	/* clean ion handles */
	list_for_each_entry(vb, &vq->queued_list, queued_entry)
		free_ion_handle_work(c_data->dev, vb);
	return 0;
}

static int capture_buffer_finish(struct vb2_buffer *vb)
{
	return 0;
}

static void capture_buffer_cleanup(struct vb2_buffer *vb)
{
}

static struct vb2_ops capture_video_qops = {
	.queue_setup		= capture_queue_setup,
	.buf_init			= capture_buffer_init,
	.buf_prepare		= capture_buffer_prepare,
	.buf_queue			= capture_buffer_queue,
	.start_streaming	= capture_start_streaming,
	.stop_streaming		= capture_stop_streaming,
	.buf_finish			= capture_buffer_finish,
	.buf_cleanup		= capture_buffer_cleanup,
};

/* IOCTL vidioc handling */

static int vidioc_querycap(struct file *file, void  *priv,
					struct v4l2_capability *cap)
{
	struct vcap_dev *dev = video_drvdata(file);

	strlcpy(cap->driver, MSM_VCAP_DRV_NAME, sizeof(cap->driver));
	strlcpy(cap->card, MSM_VCAP_DRV_NAME, sizeof(cap->card));
	strlcpy(cap->bus_info, dev->v4l2_dev.name, sizeof(cap->bus_info));
	cap->version = 0x10000000;
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
	return 0;
}

static int vidioc_enum_input(struct file *file, void *priv,
				struct v4l2_input *inp)
{
	if (inp->index >= NUM_INPUTS)
		return -EINVAL;
	return 0;
}

static int vidioc_g_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *f)
{
	return 0;
}

static int vidioc_try_fmt_vid_cap(struct file *file, void *priv,
			struct v4l2_format *f)
{
	return 0;
}

static int vidioc_s_fmt_vid_cap(struct file *file, void *priv,
					struct v4l2_format *f)
{
	int size;
	struct vcap_priv_fmt *priv_fmt;
	struct v4l2_format_vc_ext *vc_format;
	struct vcap_client_data *c_data = file->private_data;

	priv_fmt = (struct vcap_priv_fmt *) f->fmt.raw_data;

	switch (priv_fmt->type) {
	case VC_TYPE:
		vc_format = (struct v4l2_format_vc_ext *) &priv_fmt->u.timing;
		c_data->vc_format = *vc_format;

		config_vc_format(c_data);

		size = (c_data->vc_format.hactive_end -
			c_data->vc_format.hactive_start);

		if (c_data->vc_format.color_space)
			size *= 3;
		else
			size *= 2;

		priv_fmt->u.timing.bytesperline = size;
		size *= (c_data->vc_format.vactive_end -
			c_data->vc_format.vactive_start);
		priv_fmt->u.timing.sizeimage = size;
		vcap_ctrl->vc_client = c_data;
		break;
	case VP_IN_TYPE:
		c_data->vp_buf_type_field = V4L2_BUF_TYPE_INTERLACED_IN_DECODER;
		c_data->vp_format.field = f->fmt.pix.field;
		c_data->vp_format.height = f->fmt.pix.height;
		c_data->vp_format.width = f->fmt.pix.width;
		c_data->vp_format.pixelformat = f->fmt.pix.pixelformat;
		break;
	case VP_OUT_TYPE:
		break;
	default:
		break;
	}

	return 0;
}

static int vidioc_reqbufs(struct file *file, void *priv,
			  struct v4l2_requestbuffers *rb)
{
	struct vcap_client_data *c_data = file->private_data;
	switch (rb->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		return vb2_reqbufs(&c_data->vc_vidq, rb);
	default:
		pr_err("VCAP Error: %s: Unknown buffer type\n", __func__);
		return -EINVAL;
	}
	return 0;
}

static int vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct vcap_client_data *c_data = file->private_data;

	switch (p->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		return vb2_querybuf(&c_data->vc_vidq, p);
	default:
		pr_err("VCAP Error: %s: Unknown buffer type\n", __func__);
		return -EINVAL;
	}
	return 0;
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct vcap_client_data *c_data = file->private_data;
	int rc;

	switch (p->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		if (get_phys_addr(c_data->dev, &c_data->vc_vidq, p))
			return -EINVAL;
		rc = vb2_qbuf(&c_data->vc_vidq, p);
		if (rc < 0)
			free_ion_handle(c_data->dev, &c_data->vc_vidq, p);
		return rc;
	default:
		pr_err("VCAP Error: %s: Unknown buffer type\n", __func__);
		return -EINVAL;
	}
	return 0;
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct vcap_client_data *c_data = file->private_data;
	int rc;

	switch (p->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		rc = vb2_dqbuf(&c_data->vc_vidq, p, file->f_flags & O_NONBLOCK);
		if (rc < 0)
			return rc;
		return free_ion_handle(c_data->dev, &c_data->vc_vidq, p);
	default:
		pr_err("VCAP Error: %s: Unknown buffer type", __func__);
		return -EINVAL;
	}
	return 0;
}

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct vcap_client_data *c_data = file->private_data;

	switch (i) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		return vb2_streamon(&c_data->vc_vidq, i);
	default:
		pr_err("VCAP Error: %s: Unknown buffer type", __func__);
		return -EINVAL;
	}
	return 0;
}

static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct vcap_client_data *c_data = file->private_data;
	int rc;

	switch (i) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		rc = vb2_streamoff(&c_data->vc_vidq, i);
		if (rc >= 0)
			atomic_set(&c_data->dev->vc_enabled, 0);
		return rc;
	default:
		pr_err("VCAP Error: %s: Unknown buffer type", __func__);
		break;
	}
	return 0;
}

/* VCAP fops */

static void *vcap_ops_get_userptr(void *alloc_ctx, unsigned long vaddr,
					unsigned long size, int write)
{
	struct vcap_buf_info *mem;
	mem = kzalloc(sizeof(*mem), GFP_KERNEL);
	if (!mem)
		return ERR_PTR(-ENOMEM);
	mem->vaddr = vaddr;
	mem->size = size;
	return mem;
}

static void vcap_ops_put_userptr(void *buf_priv)
{
	kfree(buf_priv);
}

const struct vb2_mem_ops vcap_mem_ops = {
	.get_userptr =		vcap_ops_get_userptr,
	.put_userptr =		vcap_ops_put_userptr,
};

static int vcap_open(struct file *file)
{
	struct vcap_dev *dev = video_drvdata(file);
	struct vcap_client_data *c_data;
	struct vb2_queue *q;
	int ret;
	c_data = kzalloc(sizeof(*c_data), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	c_data->dev = dev;

	spin_lock_init(&c_data->cap_slock);

	/* initialize queue */
	q = &c_data->vc_vidq;
	memset(q, 0, sizeof(c_data->vc_vidq));
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	q->io_modes = VB2_USERPTR;
	q->drv_priv = c_data;
	q->buf_struct_size = sizeof(struct vcap_buffer);
	q->ops = &capture_video_qops;
	q->mem_ops = &vcap_mem_ops;

	ret = vb2_queue_init(q);
	if (ret < 0)
		goto open_failed;

	INIT_LIST_HEAD(&c_data->vid_vc_action.active);
	file->private_data = c_data;

	return 0;

open_failed:
	kfree(c_data);
	return ret;
}

static int vcap_close(struct file *file)
{
	struct vcap_client_data *c_data = file->private_data;
	vb2_queue_release(&c_data->vc_vidq);
	c_data->dev->vc_client = NULL;
	c_data->dev->vp_client = NULL;
	kfree(c_data);
	return 0;
}

static unsigned int vcap_poll(struct file *file,
				  struct poll_table_struct *wait)
{
	struct vcap_client_data *c_data = file->private_data;
	struct vb2_queue *q = &c_data->vc_vidq;

	return vb2_poll(q, file, wait);
}
/* V4L2 and video device structures */

static const struct v4l2_file_operations vcap_fops = {
	.owner		= THIS_MODULE,
	.open		= vcap_open,
	.release	= vcap_close,
	.poll		= vcap_poll,
	.unlocked_ioctl = video_ioctl2, /* V4L2 ioctl handler */
};

static const struct v4l2_ioctl_ops vcap_ioctl_ops = {
	.vidioc_querycap      = vidioc_querycap,
	.vidioc_enum_input    = vidioc_enum_input,
	.vidioc_g_fmt_vid_cap     = vidioc_g_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap   = vidioc_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap     = vidioc_s_fmt_vid_cap,
	.vidioc_s_fmt_type_private     = vidioc_s_fmt_vid_cap,
	.vidioc_g_fmt_type_private     = vidioc_g_fmt_vid_cap,
	.vidioc_reqbufs       = vidioc_reqbufs,
	.vidioc_querybuf      = vidioc_querybuf,
	.vidioc_qbuf          = vidioc_qbuf,
	.vidioc_dqbuf         = vidioc_dqbuf,
	.vidioc_streamon      = vidioc_streamon,
	.vidioc_streamoff     = vidioc_streamoff,
};

static struct video_device vcap_template = {
	.name		= "vcap",
	.fops		= &vcap_fops,
	.ioctl_ops	= &vcap_ioctl_ops,
	.release	= video_device_release,
};

int vcap_reg_powerup(struct vcap_dev *dev, struct device *ddev)
{
	dev->fs_vcap = regulator_get(ddev, "vdd");
	if (IS_ERR(dev->fs_vcap)) {
		pr_err("%s: Regulator FS_VCAP get failed %ld\n", __func__,
			PTR_ERR(dev->fs_vcap));
		dev->fs_vcap = NULL;
		return -EINVAL;
	} else if (regulator_enable(dev->fs_vcap)) {
		pr_err("%s: Regulator FS_VCAP enable failed\n", __func__);
		regulator_put(dev->fs_vcap);
		return -EINVAL;
	}
	return 0;
}

void vcap_reg_powerdown(struct vcap_dev *dev)
{
	if (dev->fs_vcap == NULL)
		return;
	regulator_disable(dev->fs_vcap);
	regulator_put(dev->fs_vcap);
	dev->fs_vcap = NULL;
	return;
}

int config_gpios(int on, struct vcap_platform_data *pdata)
{
	int i, ret;
	int num_gpios = pdata->num_gpios;
	unsigned *gpios = pdata->gpios;

	if (on) {
		for (i = 0; i < num_gpios; i++) {
			ret = gpio_request(gpios[i], "vcap:vc");
			if (ret) {
				pr_err("VCAP: failed at GPIO %d to request\n",
						gpios[i]);
				goto gpio_failed;
			}
			ret = gpio_direction_input(gpios[i]);
			if (ret) {
				pr_err("VCAP: failed at GPIO %d to set to input\n",
					gpios[i]);
				i++;
				goto gpio_failed;
			}
		}
	} else {
		for (i = 0; i < num_gpios; i++)
			gpio_free(gpios[i]);
	}
	dprintk(2, "GPIO config done\n");
	return 0;
gpio_failed:
	for (i--; i >= 0; i--)
		gpio_free(gpios[i]);
	return -EINVAL;
}

int vcap_clk_powerup(struct vcap_dev *dev, struct device *ddev)
{
	int ret = 0;

	dev->vcap_clk = clk_get(ddev, "core_clk");
	if (IS_ERR(dev->vcap_clk)) {
		dev->vcap_clk = NULL;
		pr_err("%s: Could not clk_get core_clk\n", __func__);
		clk_put(dev->vcap_clk);
		dev->vcap_clk = NULL;
		return -EINVAL;
	}

	clk_prepare(dev->vcap_clk);
	ret = clk_enable(dev->vcap_clk);
	if (ret) {
		pr_err("%s: Failed core clk_enable %d\n", __func__, ret);
		goto fail_vcap_clk_unprep;
	}

	clk_set_rate(dev->vcap_clk, 160000000);
	if (ret) {
		pr_err("%s: Failed core set_rate %d\n", __func__, ret);
		goto fail_vcap_clk;
	}

	dev->vcap_npl_clk = clk_get(ddev, "vcap_npl_clk");
	if (IS_ERR(dev->vcap_npl_clk)) {
		dev->vcap_npl_clk = NULL;
		pr_err("%s: Could not clk_get npl\n", __func__);
		clk_put(dev->vcap_npl_clk);
		dev->vcap_npl_clk = NULL;
		goto fail_vcap_clk;
	}

	clk_prepare(dev->vcap_npl_clk);
	ret = clk_enable(dev->vcap_npl_clk);
	if (ret) {
		pr_err("%s:Failed npl clk_enable %d\n", __func__, ret);
		goto fail_vcap_npl_clk_unprep;
	}

	dev->vcap_p_clk = clk_get(ddev, "iface_clk");
	if (IS_ERR(dev->vcap_p_clk)) {
		dev->vcap_p_clk = NULL;
		pr_err("%s: Could not clk_get pix(AHB)\n", __func__);
		clk_put(dev->vcap_p_clk);
		dev->vcap_p_clk = NULL;
		goto fail_vcap_npl_clk;
	}

	clk_prepare(dev->vcap_p_clk);
	ret = clk_enable(dev->vcap_p_clk);
	if (ret) {
		pr_err("%s: Failed pix(AHB) clk_enable %d\n", __func__, ret);
		goto fail_vcap_p_clk_unprep;
	}
	return 0;

fail_vcap_p_clk_unprep:
	clk_unprepare(dev->vcap_p_clk);
	clk_put(dev->vcap_p_clk);
	dev->vcap_p_clk = NULL;

fail_vcap_npl_clk:
	clk_disable(dev->vcap_npl_clk);
fail_vcap_npl_clk_unprep:
	clk_unprepare(dev->vcap_npl_clk);
	clk_put(dev->vcap_npl_clk);
	dev->vcap_npl_clk = NULL;

fail_vcap_clk:
	clk_disable(dev->vcap_clk);
fail_vcap_clk_unprep:
	clk_unprepare(dev->vcap_clk);
	clk_put(dev->vcap_clk);
	dev->vcap_clk = NULL;
	return -EINVAL;
}

void vcap_clk_powerdown(struct vcap_dev *dev)
{
	if (dev->vcap_p_clk != NULL) {
		clk_disable(dev->vcap_p_clk);
		clk_unprepare(dev->vcap_p_clk);
		clk_put(dev->vcap_p_clk);
		dev->vcap_p_clk = NULL;
	}

	if (dev->vcap_npl_clk != NULL) {
		clk_disable(dev->vcap_npl_clk);
		clk_unprepare(dev->vcap_npl_clk);
		clk_put(dev->vcap_npl_clk);
		dev->vcap_npl_clk = NULL;
	}

	if (dev->vcap_clk != NULL) {
		clk_disable(dev->vcap_clk);
		clk_unprepare(dev->vcap_clk);
		clk_put(dev->vcap_clk);
		dev->vcap_clk = NULL;
	}
}

int vcap_get_bus_client_handle(struct vcap_dev *dev)
{
	struct msm_bus_scale_pdata *vcap_axi_client_pdata =
			dev->vcap_pdata->bus_client_pdata;
	dev->bus_client_handle =
			msm_bus_scale_register_client(vcap_axi_client_pdata);

	return 0;
}

int vcap_enable(struct vcap_dev *dev, struct device *ddev)
{
	int rc;

	rc = vcap_reg_powerup(dev, ddev);
	if (rc < 0)
		goto reg_failed;
	rc = vcap_clk_powerup(dev, ddev);
	if (rc < 0)
		goto clk_failed;
	rc = vcap_get_bus_client_handle(dev);
	if (rc < 0)
		goto bus_r_failed;
	config_gpios(1, dev->vcap_pdata);
	if (rc < 0)
		goto gpio_failed;
	return 0;

gpio_failed:
	msm_bus_scale_unregister_client(dev->bus_client_handle);
	dev->bus_client_handle = 0;
bus_r_failed:
	vcap_clk_powerdown(dev);
clk_failed:
	vcap_reg_powerdown(dev);
reg_failed:
	return rc;
}

int vcap_disable(struct vcap_dev *dev)
{
	config_gpios(0, dev->vcap_pdata);

	msm_bus_scale_unregister_client(dev->bus_client_handle);
	dev->bus_client_handle = 0;
	vcap_clk_powerdown(dev);
	vcap_reg_powerdown(dev);
	return 0;
}

static irqreturn_t vcap_vc_handler(int irq_num, void *data)
{
	return vc_handler(vcap_ctrl);
}

static int __devinit vcap_probe(struct platform_device *pdev)
{
	struct vcap_dev *dev;
	struct video_device *vfd;
	int ret;

	dprintk(1, "Probe started\n");
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;
	vcap_ctrl = dev;
	dev->vcap_pdata = pdev->dev.platform_data;

	dev->vcapmem = platform_get_resource_byname(pdev,
			IORESOURCE_MEM, "vcap");
	if (!dev->vcapmem) {
		pr_err("VCAP: %s: no mem resource?\n", __func__);
		ret = -ENODEV;
		goto free_dev;
	}

	dev->vcapio = request_mem_region(dev->vcapmem->start,
		resource_size(dev->vcapmem), pdev->name);
	if (!dev->vcapio) {
		pr_err("VCAP: %s: no valid mem region\n", __func__);
		ret = -EBUSY;
		goto free_dev;
	}

	dev->vcapbase = ioremap(dev->vcapmem->start,
		resource_size(dev->vcapmem));
	if (!dev->vcapbase) {
		ret = -ENOMEM;
		pr_err("VCAP: %s: vcap ioremap failed\n", __func__);
		goto free_resource;
	}

	dev->vcapirq = platform_get_resource_byname(pdev,
					IORESOURCE_IRQ, "vcap");
	if (!dev->vcapirq) {
		pr_err("%s: no irq resource?\n", __func__);
		ret = -ENODEV;
		goto free_resource;
	}

	ret = request_irq(dev->vcapirq->start, vcap_vc_handler,
		IRQF_TRIGGER_RISING, "vcap", 0);
	if (ret < 0) {
		pr_err("%s: irq request fail\n", __func__);
		ret = -EBUSY;
		goto free_resource;
	}

	disable_irq(dev->vcapirq->start);

	snprintf(dev->v4l2_dev.name, sizeof(dev->v4l2_dev.name),
			"%s", MSM_VCAP_DRV_NAME);
	ret = v4l2_device_register(NULL, &dev->v4l2_dev);
	if (ret)
		goto free_resource;

	ret = vcap_enable(dev, &pdev->dev);
	if (ret)
		goto unreg_dev;
	msm_bus_scale_client_update_request(dev->bus_client_handle, 3);

	ret = detect_vc(dev);

	if (ret)
		goto power_down;

	/* init video device*/
	vfd = video_device_alloc();
	if (!vfd)
		goto deinit_vc;

	*vfd = vcap_template;
	vfd->v4l2_dev = &dev->v4l2_dev;

	ret = video_register_device(vfd, VFL_TYPE_GRABBER, -1);
	if (ret < 0)
		goto rel_vdev;

	dev->vfd = vfd;
	video_set_drvdata(vfd, dev);

	dev->ion_client = msm_ion_client_create(-1, "vcap");
	if (IS_ERR((void *)dev->ion_client)) {
		pr_err("could not get ion client");
		goto rel_vdev;
	}

	atomic_set(&dev->vc_enabled, 0);

	dprintk(1, "Exit probe succesfully");
	return 0;

rel_vdev:
	video_device_release(vfd);
deinit_vc:
	deinit_vc();
power_down:
	vcap_disable(dev);
unreg_dev:
	v4l2_device_unregister(&dev->v4l2_dev);
free_resource:
	iounmap(dev->vcapbase);
	release_mem_region(dev->vcapmem->start, resource_size(dev->vcapmem));
free_dev:
	vcap_ctrl = NULL;
	kfree(dev);
	return ret;
}

static int __devexit vcap_remove(struct platform_device *pdev)
{
	struct vcap_dev *dev = vcap_ctrl;
	ion_client_destroy(dev->ion_client);
	video_device_release(dev->vfd);
	deinit_vc();
	vcap_disable(dev);
	v4l2_device_unregister(&dev->v4l2_dev);
	iounmap(dev->vcapbase);
	release_mem_region(dev->vcapmem->start, resource_size(dev->vcapmem));
	vcap_ctrl = NULL;
	kfree(dev);

	return 0;
}

struct platform_driver vcap_platform_driver = {
	.driver		= {
		.name	= MSM_VCAP_DRV_NAME,
		.owner	= THIS_MODULE,
	},
	.probe		= vcap_probe,
	.remove		= vcap_remove,
};

static int __init vcap_init_module(void)
{
	return platform_driver_register(&vcap_platform_driver);
}

static void __exit vcap_exit_module(void)
{
	platform_driver_unregister(&vcap_platform_driver);
}

module_init(vcap_init_module);
module_exit(vcap_exit_module);
MODULE_DESCRIPTION("VCAP driver");
MODULE_LICENSE("GPL v2");
