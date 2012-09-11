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
#include <media/v4l2-event.h>
#include <linux/regulator/consumer.h>
#include <mach/clk.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>
#include <mach/iommu_domains.h>

#include <media/vcap_v4l2.h>
#include <media/vcap_fmt.h>
#include "vcap_vc.h"
#include "vcap_vp.h"

#define NUM_INPUTS 1
#define MSM_VCAP_DRV_NAME "msm_vcap"

static struct vcap_dev *vcap_ctrl;

static unsigned debug;

#define dprintk(level, fmt, arg...)					\
	do {								\
		if (debug >= level)					\
			printk(KERN_DEBUG "VCAP: " fmt, ## arg);	\
	} while (0)

int vcap_reg_powerup(struct vcap_dev *dev)
{
	dev->fs_vcap = regulator_get(NULL, "fs_vcap");
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

	dprintk(4, "GPIO config start\n");
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
	dprintk(4, "GPIO config exit\n");
	return 0;
gpio_failed:
	for (i--; i >= 0; i--)
		gpio_free(gpios[i]);
	return -EINVAL;
}

int vcap_clk_powerup(struct vcap_dev *dev, struct device *ddev,
		unsigned long rate)
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

	rate = clk_round_rate(dev->vcap_clk, rate);
	if (rate < 0) {
		pr_err("%s: Failed core rnd_rate\n", __func__);
		goto fail_vcap_clk;
	}
	ret = clk_set_rate(dev->vcap_clk, rate);
	if (ret < 0) {
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

int vcap_enable(struct vcap_dev *dev, struct device *ddev,
		unsigned long rate)
{
	int rc;

	rc = vcap_reg_powerup(dev);
	if (rc < 0)
		goto reg_failed;
	rc = vcap_clk_powerup(dev, ddev, rate);
	if (rc < 0)
		goto clk_failed;
	rc = vcap_get_bus_client_handle(dev);
	if (rc < 0)
		goto bus_r_failed;
	rc = config_gpios(1, dev->vcap_pdata);
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

enum vcap_op_mode determine_mode(struct vcap_client_data *cd)
{
	if (cd->set_cap == 1 && cd->set_vp_o == 0 &&
			cd->set_decode == 0)
		return VC_VCAP_OP;
	else if (cd->set_cap == 1 && cd->set_vp_o == 1 &&
			cd->set_decode == 0)
		return VC_AND_VP_VCAP_OP;
	else if (cd->set_cap == 0 && cd->set_vp_o == 1 &&
			cd->set_decode == 1)
		return VP_VCAP_OP;
	else
		return UNKNOWN_VCAP_OP;
}

void dealloc_resources(struct vcap_client_data *cd)
{
	cd->set_cap = false;
	cd->set_decode = false;
	cd->set_vp_o = false;
}

/* VCAP Internal QBUF and DQBUF for VC + VP */
int vcvp_qbuf(struct vb2_queue *q, struct v4l2_buffer *b)
{
	struct vb2_buffer *vb;

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

	if (b->memory != q->memory) {
		dprintk(1, "%s: invalid memory type\n", __func__);
		return -EINVAL;
	}

	if (vb->state != VB2_BUF_STATE_DEQUEUED &&
			vb->state != VB2_BUF_STATE_PREPARED) {
		dprintk(1, "%s: buffer already in use\n", __func__);
		return -EINVAL;
	}

	list_add_tail(&vb->queued_entry, &q->queued_list);
	vb->state = VB2_BUF_STATE_QUEUED;

	if (q->streaming) {
		vb->state = VB2_BUF_STATE_ACTIVE;
		atomic_inc(&q->queued_count);
		q->ops->buf_queue(vb);
	}
	return 0;
}

int vcvp_dqbuf(struct vb2_queue *q, struct v4l2_buffer *b)
{
	struct vb2_buffer *vb = NULL;
	unsigned long flags;

	if (q->fileio) {
		dprintk(1, "%s: file io in progress\n", __func__);
		return -EBUSY;
	}

	if (b->type != q->type) {
		dprintk(1, "%s: invalid buffer type\n", __func__);
		return -EINVAL;
	}

	if (!q->streaming) {
		dprintk(1, "Streaming off, will not wait for buffers\n");
		return -EINVAL;
	}

	if (!list_empty(&q->done_list)) {
		spin_lock_irqsave(&q->done_lock, flags);
		vb = list_first_entry(&q->done_list, struct vb2_buffer,
				done_entry);
		list_del(&vb->done_entry);
		spin_unlock_irqrestore(&q->done_lock, flags);

		switch (vb->state) {
		case VB2_BUF_STATE_DONE:
			dprintk(3, "%s: Returning done buffer\n", __func__);
			break;
		case VB2_BUF_STATE_ERROR:
			dprintk(3, "%s: Ret done buf with err\n", __func__);
			break;
		default:
			dprintk(1, "%s: Invalid buffer state\n", __func__);
			return -EINVAL;
		}

		memcpy(b, &vb->v4l2_buf, offsetof(struct v4l2_buffer, m));

		list_del(&vb->queued_entry);

		vb->state = VB2_BUF_STATE_DEQUEUED;
		return 0;
	}

	dprintk(1, "No buffers to dequeue\n");
	return -EAGAIN;
}

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

	buf->ion_handle = ion_import_dma_buf(dev->ion_client, b->m.userptr);
	if (IS_ERR((void *)buf->ion_handle)) {
		pr_err("%s: Could not alloc memory\n", __func__);
		buf->ion_handle = NULL;
		return -ENOMEM;
	}
	rc = ion_phys(dev->ion_client, buf->ion_handle,
			&buf->paddr, (size_t *)&len);
	if (rc < 0) {
		pr_err("%s: Could not get phys addr\n", __func__);
		ion_free(dev->ion_client, buf->ion_handle);
		buf->ion_handle = NULL;
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

/* VC Videobuf operations */

static int capture_queue_setup(struct vb2_queue *vq,
			       const struct v4l2_format *fmt,
			       unsigned int *nbuffers,
			       unsigned int *nplanes, unsigned int sizes[],
			       void *alloc_ctxs[])
{
	*nbuffers += vcap_ctrl->vc_tot_buf;
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
	struct vc_action *vc_action = &c_data->vc_action;
	struct vb2_queue *q = vb->vb2_queue;
	unsigned long flags = 0;

	spin_lock_irqsave(&c_data->cap_slock, flags);
	list_add_tail(&buf->list, &vc_action->active);
	spin_unlock_irqrestore(&c_data->cap_slock, flags);

	if (atomic_read(&c_data->dev->vc_enabled) == 0) {
		if (atomic_read(&q->queued_count) >= c_data->vc_action.tot_buf)
			if (vc_hw_kick_off(c_data) == 0)
				atomic_set(&c_data->dev->vc_enabled, 1);
	}
}

static int capture_start_streaming(struct vb2_queue *vq, unsigned int count)
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

	while (!list_empty(&c_data->vc_action.active)) {
		struct vcap_buffer *buf;
		buf = list_entry(c_data->vc_action.active.next,
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

/* VP I/P Videobuf operations */

static int vp_in_queue_setup(struct vb2_queue *vq,
			     const struct v4l2_format *fmt,
			     unsigned int *nbuffers,
			     unsigned int *nplanes, unsigned int sizes[],
			     void *alloc_ctxs[])
{
	if (*nbuffers >= VIDEO_MAX_FRAME && *nbuffers < 5)
		*nbuffers = 5;

	*nplanes = 1;
	return 0;
}

static int vp_in_buffer_init(struct vb2_buffer *vb)
{
	return 0;
}

static int vp_in_buffer_prepare(struct vb2_buffer *vb)
{
	return 0;
}

static void vp_in_buffer_queue(struct vb2_buffer *vb)
{
	struct vcap_client_data *cd = vb2_get_drv_priv(vb->vb2_queue);
	struct vcap_buffer *buf = container_of(vb, struct vcap_buffer, vb);
	struct vp_action *vp_act = &cd->vp_action;
	struct vb2_queue *q = vb->vb2_queue;
	unsigned long flags = 0;

	spin_lock_irqsave(&cd->cap_slock, flags);
	list_add_tail(&buf->list, &vp_act->in_active);
	spin_unlock_irqrestore(&cd->cap_slock, flags);

	if (atomic_read(&cd->dev->vp_enabled) == 0) {
		if (cd->vp_action.vp_state == VP_FRAME1) {
			if (atomic_read(&q->queued_count) > 1 &&
				atomic_read(&cd->vp_out_vidq.queued_count) > 0)
				/* Valid code flow for VC-VP mode */
				kickoff_vp(cd);
		} else {
			/* VP has already kicked off just needs cont */
			continue_vp(cd);
		}
	}
}

static int vp_in_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	dprintk(2, "VP IN start streaming\n");
	return 0;
}

static int vp_in_stop_streaming(struct vb2_queue *vq)
{
	struct vcap_client_data *c_data = vb2_get_drv_priv(vq);
	struct vb2_buffer *vb;

	dprintk(2, "VP stop streaming\n");

	while (!list_empty(&c_data->vp_action.in_active)) {
		struct vcap_buffer *buf;
		buf = list_entry(c_data->vp_action.in_active.next,
			struct vcap_buffer, list);
		list_del(&buf->list);
		vb2_buffer_done(&buf->vb, VB2_BUF_STATE_ERROR);
	}

	/* clean ion handles */
	list_for_each_entry(vb, &vq->queued_list, queued_entry)
		free_ion_handle_work(c_data->dev, vb);
	return 0;
}

static int vp_in_buffer_finish(struct vb2_buffer *vb)
{
	return 0;
}

static void vp_in_buffer_cleanup(struct vb2_buffer *vb)
{
}

static struct vb2_ops vp_in_video_qops = {
	.queue_setup		= vp_in_queue_setup,
	.buf_init			= vp_in_buffer_init,
	.buf_prepare		= vp_in_buffer_prepare,
	.buf_queue			= vp_in_buffer_queue,
	.start_streaming	= vp_in_start_streaming,
	.stop_streaming		= vp_in_stop_streaming,
	.buf_finish			= vp_in_buffer_finish,
	.buf_cleanup		= vp_in_buffer_cleanup,
};


/* VP O/P Videobuf operations */

static int vp_out_queue_setup(struct vb2_queue *vq,
			      const struct v4l2_format *fmt,
			      unsigned int *nbuffers,
			      unsigned int *nplanes, unsigned int sizes[],
			      void *alloc_ctxs[])
{
	if (*nbuffers >= VIDEO_MAX_FRAME && *nbuffers < 3)
		*nbuffers = 3;

	*nplanes = 1;
	return 0;
}

static int vp_out_buffer_init(struct vb2_buffer *vb)
{
	return 0;
}

static int vp_out_buffer_prepare(struct vb2_buffer *vb)
{
	return 0;
}

static void vp_out_buffer_queue(struct vb2_buffer *vb)
{
	struct vcap_client_data *cd = vb2_get_drv_priv(vb->vb2_queue);
	struct vcap_buffer *buf = container_of(vb, struct vcap_buffer, vb);
	struct vp_action *vp_act = &cd->vp_action;
	struct vb2_queue *q = vb->vb2_queue;
	unsigned long flags = 0;

	spin_lock_irqsave(&cd->cap_slock, flags);
	list_add_tail(&buf->list, &vp_act->out_active);
	spin_unlock_irqrestore(&cd->cap_slock, flags);

	if (atomic_read(&cd->dev->vp_enabled) == 0) {
		if (cd->vp_action.vp_state == VP_FRAME1) {
			if (atomic_read(&q->queued_count) > 0 &&
				atomic_read(&
					cd->vp_in_vidq.queued_count) > 1)
				kickoff_vp(cd);
		} else {
			/* VP has already kicked off just needs cont */
			continue_vp(cd);
		}
	}
}

static int vp_out_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	return 0;
}

static int vp_out_stop_streaming(struct vb2_queue *vq)
{
	struct vcap_client_data *c_data = vb2_get_drv_priv(vq);
	struct vb2_buffer *vb;

	dprintk(2, "VP out q stop streaming\n");
	vp_stop_capture(c_data);

	while (!list_empty(&c_data->vp_action.out_active)) {
		struct vcap_buffer *buf;
		buf = list_entry(c_data->vp_action.out_active.next,
			struct vcap_buffer, list);
		list_del(&buf->list);
		vb2_buffer_done(&buf->vb, VB2_BUF_STATE_ERROR);
	}

	/* clean ion handles */
	list_for_each_entry(vb, &vq->queued_list, queued_entry)
		free_ion_handle_work(c_data->dev, vb);
	return 0;
}

static int vp_out_buffer_finish(struct vb2_buffer *vb)
{
	return 0;
}

static void vp_out_buffer_cleanup(struct vb2_buffer *vb)
{
}

static struct vb2_ops vp_out_video_qops = {
	.queue_setup		= vp_out_queue_setup,
	.buf_init			= vp_out_buffer_init,
	.buf_prepare		= vp_out_buffer_prepare,
	.buf_queue			= vp_out_buffer_queue,
	.start_streaming	= vp_out_start_streaming,
	.stop_streaming		= vp_out_stop_streaming,
	.buf_finish			= vp_out_buffer_finish,
	.buf_cleanup		= vp_out_buffer_cleanup,
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
	struct vcap_client_data *c_data = to_client_data(file->private_data);

	priv_fmt = (struct vcap_priv_fmt *) f->fmt.raw_data;

	switch (priv_fmt->type) {
	case VC_TYPE:
		vc_format = (struct v4l2_format_vc_ext *) &priv_fmt->u.timing;
		c_data->vc_format = *vc_format;

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
		c_data->set_cap = true;
		break;
	case VP_IN_TYPE:
		c_data->vp_in_fmt.width = priv_fmt->u.pix.width;
		c_data->vp_in_fmt.height = priv_fmt->u.pix.height;
		c_data->vp_in_fmt.pixfmt = priv_fmt->u.pix.pixelformat;

		size = c_data->vp_in_fmt.width * c_data->vp_in_fmt.height;
		if (c_data->vp_in_fmt.pixfmt == V4L2_PIX_FMT_NV16)
			size = size * 2;
		else
			size = size / 2 * 3;
		priv_fmt->u.pix.sizeimage = size;
		c_data->set_decode = true;
		break;
	case VP_OUT_TYPE:
		c_data->vp_out_fmt.width = priv_fmt->u.pix.width;
		c_data->vp_out_fmt.height = priv_fmt->u.pix.height;
		c_data->vp_out_fmt.pixfmt = priv_fmt->u.pix.pixelformat;

		size = c_data->vp_out_fmt.width * c_data->vp_out_fmt.height;
		if (c_data->vp_out_fmt.pixfmt == V4L2_PIX_FMT_NV16)
			size = size * 2;
		else
			size = size / 2 * 3;
		priv_fmt->u.pix.sizeimage = size;
		c_data->set_vp_o = true;
		break;
	default:
		break;
	}

	return 0;
}

static int vidioc_reqbufs(struct file *file, void *priv,
			  struct v4l2_requestbuffers *rb)
{
	struct vcap_client_data *c_data = to_client_data(file->private_data);
	struct vcap_dev *dev = c_data->dev;
	int rc;

	dprintk(3, "In Req Buf %08x\n", (unsigned int)rb->type);
	c_data->op_mode = determine_mode(c_data);
	if (c_data->op_mode == UNKNOWN_VCAP_OP) {
		pr_err("VCAP Error: %s: VCAP in unknown mode\n", __func__);
		return -ENOTRECOVERABLE;
	}

	switch (rb->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		if (c_data->op_mode == VC_AND_VP_VCAP_OP) {
			if (c_data->vc_format.color_space) {
				pr_err("VCAP Err: %s: VP No RGB support\n",
					__func__);
				return -ENOTRECOVERABLE;
			}
			if (!c_data->vc_format.mode) {
				pr_err("VCAP Err: VP No prog support\n");
				return -ENOTRECOVERABLE;
			}
			if (rb->count <= VCAP_VP_MIN_BUF) {
				pr_err("VCAP Err: Not enough buf for VC_VP\n");
				return -EINVAL;
			}
			rc = vb2_reqbufs(&c_data->vc_vidq, rb);
			if (rc < 0)
				return rc;

			c_data->vp_in_fmt.width =
				(c_data->vc_format.hactive_end -
				c_data->vc_format.hactive_start);
			c_data->vp_in_fmt.height =
				(c_data->vc_format.vactive_end -
				c_data->vc_format.vactive_start);
			/* VC outputs YCbCr 4:2:2 */
			c_data->vp_in_fmt.pixfmt = V4L2_PIX_FMT_NV16;
			rb->type = V4L2_BUF_TYPE_INTERLACED_IN_DECODER;
			rc = vb2_reqbufs(&c_data->vp_in_vidq, rb);
			rb->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			c_data->vc_action.tot_buf = dev->vc_tot_buf;
			return rc;

		} else {
			rc = vb2_reqbufs(&c_data->vc_vidq, rb);
			c_data->vc_action.tot_buf = dev->vc_tot_buf;
			return rc;
		}
	case V4L2_BUF_TYPE_INTERLACED_IN_DECODER:
		return vb2_reqbufs(&c_data->vp_in_vidq, rb);
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		return vb2_reqbufs(&c_data->vp_out_vidq, rb);
	default:
		pr_err("VCAP Error: %s: Unknown buffer type\n", __func__);
		return -EINVAL;
	}
	return 0;
}

static int vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct vcap_client_data *c_data = to_client_data(file->private_data);

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
	struct vcap_client_data *c_data = to_client_data(file->private_data);
	struct vb2_buffer *vb;
	struct vb2_queue *q;
	int rc;

	dprintk(3, "In Q Buf %08x\n", (unsigned int)p->type);
	switch (p->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		if (c_data->op_mode == VC_AND_VP_VCAP_OP) {
			/* If buffer in vp_in_q it will be coming back */
			q = &c_data->vp_in_vidq;
			if (p->index >= q->num_buffers) {
				dprintk(1, "qbuf: buffer index out of range\n");
				return -EINVAL;
			}

			vb = q->bufs[p->index];
			if (NULL == vb) {
				dprintk(1, "qbuf: buffer is NULL\n");
				return -EINVAL;
			}

			if (vb->state != VB2_BUF_STATE_DEQUEUED) {
				dprintk(1, "qbuf: buffer already in use\n");
				return -EINVAL;
			}
			rc = get_phys_addr(c_data->dev, &c_data->vc_vidq, p);
			if (rc < 0)
				return rc;
			rc = vcvp_qbuf(&c_data->vc_vidq, p);
			if (rc < 0)
				free_ion_handle(c_data->dev,
					&c_data->vc_vidq, p);
			return rc;
		}
		rc = get_phys_addr(c_data->dev, &c_data->vc_vidq, p);
		if (rc < 0)
			return rc;
		rc = vb2_qbuf(&c_data->vc_vidq, p);
		if (rc < 0)
			free_ion_handle(c_data->dev, &c_data->vc_vidq, p);
		return rc;
	case V4L2_BUF_TYPE_INTERLACED_IN_DECODER:
		if (c_data->op_mode == VC_AND_VP_VCAP_OP)
			return -EINVAL;
		rc = get_phys_addr(c_data->dev, &c_data->vp_in_vidq, p);
		if (rc < 0)
			return rc;
		rc = vb2_qbuf(&c_data->vp_in_vidq, p);
		if (rc < 0)
			free_ion_handle(c_data->dev, &c_data->vp_in_vidq, p);
		return rc;
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		rc = get_phys_addr(c_data->dev, &c_data->vp_out_vidq, p);
		if (rc < 0)
			return rc;
		rc = vb2_qbuf(&c_data->vp_out_vidq, p);
		if (rc < 0)
			free_ion_handle(c_data->dev, &c_data->vp_out_vidq, p);
		return rc;
	default:
		pr_err("VCAP Error: %s: Unknown buffer type\n", __func__);
		return -EINVAL;
	}
	return 0;
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct vcap_client_data *c_data = to_client_data(file->private_data);
	int rc;

	dprintk(3, "In DQ Buf %08x\n", (unsigned int)p->type);
	switch (p->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		if (c_data->op_mode == VC_AND_VP_VCAP_OP)
			return -EINVAL;
		rc = vb2_dqbuf(&c_data->vc_vidq, p, file->f_flags & O_NONBLOCK);
		if (rc < 0)
			return rc;
		return free_ion_handle(c_data->dev, &c_data->vc_vidq, p);
	case V4L2_BUF_TYPE_INTERLACED_IN_DECODER:
		if (c_data->op_mode == VC_AND_VP_VCAP_OP)
			return -EINVAL;
		rc = vb2_dqbuf(&c_data->vp_in_vidq, p, file->f_flags &
				O_NONBLOCK);
		if (rc < 0)
			return rc;
		return free_ion_handle(c_data->dev, &c_data->vp_in_vidq, p);
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		rc = vb2_dqbuf(&c_data->vp_out_vidq, p, file->f_flags &
				O_NONBLOCK);
		if (rc < 0)
			return rc;
		return free_ion_handle(c_data->dev, &c_data->vp_out_vidq, p);
	default:
		pr_err("VCAP Error: %s: Unknown buffer type", __func__);
		return -EINVAL;
	}
	return 0;
}

/*
 * When calling streamon on multiple queues there is a need to first verify
 * that the steamon will succeed on all queues, similarly for streamoff
 */
int streamon_validate_q(struct vb2_queue *q)
{
	if (q->fileio) {
		dprintk(1, "streamon: file io in progress\n");
		return -EBUSY;
	}

	if (q->streaming) {
		dprintk(1, "streamon: already streaming\n");
		return -EBUSY;
	}

	if (V4L2_TYPE_IS_OUTPUT(q->type)) {
		if (list_empty(&q->queued_list)) {
			dprintk(1, "streamon: no output buffers queued\n");
			return -EINVAL;
		}
	}
	return 0;
}

int request_bus_bw(struct vcap_dev *dev, unsigned long rate)
{
	struct msm_bus_paths *bus_vectors;
	int idx, length;
	bus_vectors = dev->vcap_pdata->bus_client_pdata->usecase;
	length = dev->vcap_pdata->bus_client_pdata->num_usecases;
	idx = 0;
	do {
		if (rate <= bus_vectors[idx].vectors[0].ab)
			break;
		idx++;
	} while (idx < length);
	if (idx == length) {
		pr_err("VCAP: Defaulting to highest BW request\n");
		idx--;
	}
	msm_bus_scale_client_update_request(dev->bus_client_handle, idx);
	return 0;
}

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct vcap_client_data *c_data = to_client_data(file->private_data);
	struct vcap_dev *dev = c_data->dev;
	int rc;
	unsigned long rate;
	long rate_rc;

	dprintk(3, "In Stream ON\n");
	if (determine_mode(c_data) != c_data->op_mode) {
		pr_err("VCAP Error: %s: s_fmt called after req_buf", __func__);
		return -ENOTRECOVERABLE;
	}

	if (!dev->vp_dummy_complete) {
		pr_err("VCAP Err: %s: VP dummy read not complete",
			__func__);
		return -EINVAL;
	}

	switch (c_data->op_mode) {
	case VC_VCAP_OP:
		mutex_lock(&dev->dev_mutex);
		if (dev->vc_resource) {
			pr_err("VCAP Err: %s: VC resource taken", __func__);
			mutex_unlock(&dev->dev_mutex);
			return -EBUSY;
		}
		dev->vc_resource = 1;
		mutex_unlock(&dev->dev_mutex);

		c_data->dev->vc_client = c_data;

		if (!c_data->vc_format.clk_freq) {
			rc = -EINVAL;
			goto free_res;
		}

		rate = c_data->vc_format.clk_freq / 100 * 102;
		rate_rc = clk_round_rate(dev->vcap_clk, rate);
		if (rate_rc <= 0) {
			pr_err("%s: Failed core rnd_rate\n", __func__);
			rc = -EINVAL;
			goto free_res;
		}
		rate = (unsigned long)rate_rc;
		rc = clk_set_rate(dev->vcap_clk, rate);
		if (rc < 0)
			goto free_res;

		rate = (c_data->vc_format.hactive_end -
			c_data->vc_format.hactive_start);

		if (c_data->vc_format.color_space)
			rate *= 3;
		else
			rate *= 2;

		rate *= (c_data->vc_format.vactive_end -
			c_data->vc_format.vactive_start);
		rate *= c_data->vc_format.frame_rate;
		if (rate == 0)
			goto free_res;

		rc = request_bus_bw(dev, rate);
		if (rc < 0)
			goto free_res;

		config_vc_format(c_data);
		c_data->streaming = 1;
		rc = vb2_streamon(&c_data->vc_vidq, i);
		if (rc < 0)
			goto free_res;
		break;
	case VP_VCAP_OP:
		mutex_lock(&dev->dev_mutex);
		if (dev->vp_resource) {
			pr_err("VCAP Err: %s: VP resource taken", __func__);
			mutex_unlock(&dev->dev_mutex);
			return -EBUSY;
		}
		dev->vp_resource = 1;
		mutex_unlock(&dev->dev_mutex);
		c_data->dev->vp_client = c_data;

		rate = 160000000;
		rate_rc = clk_round_rate(dev->vcap_clk, rate);
		if (rate_rc <= 0) {
			pr_err("%s: Failed core rnd_rate\n", __func__);
			rc = -EINVAL;
			goto free_res;
		}
		rate = (unsigned long)rate_rc;
		rc = clk_set_rate(dev->vcap_clk, rate);
		if (rc < 0)
			goto free_res;

		rate = c_data->vp_out_fmt.width *
			c_data->vp_out_fmt.height * 240;
		rc = request_bus_bw(dev, rate);
		if (rc < 0)
			goto free_res;

		rc = streamon_validate_q(&c_data->vp_in_vidq);
		if (rc < 0)
			goto free_res;
		rc = streamon_validate_q(&c_data->vp_out_vidq);
		if (rc < 0)
			goto free_res;

		rc = config_vp_format(c_data);
		if (rc < 0)
			goto free_res;
		rc = init_motion_buf(c_data);
		if (rc < 0)
			goto free_res;
		if (dev->nr_param.mode) {
			rc = init_nr_buf(c_data);
			if (rc < 0)
				goto s_on_deinit_m_buf;
		}

		c_data->vp_action.vp_state = VP_FRAME1;
		c_data->streaming = 1;

		rc = vb2_streamon(&c_data->vp_in_vidq,
				V4L2_BUF_TYPE_INTERLACED_IN_DECODER);
		if (rc < 0)
			goto s_on_deinit_nr_buf;

		rc = vb2_streamon(&c_data->vp_out_vidq,
				V4L2_BUF_TYPE_VIDEO_OUTPUT);
		if (rc < 0)
			goto s_on_deinit_nr_buf;
		break;
	case VC_AND_VP_VCAP_OP:
		mutex_lock(&dev->dev_mutex);
		if (dev->vc_resource || dev->vp_resource) {
			pr_err("VCAP Err: %s: VC/VP resource taken",
				__func__);
			mutex_unlock(&dev->dev_mutex);
			return -EBUSY;
		}
		dev->vc_resource = 1;
		dev->vp_resource = 1;
		mutex_unlock(&dev->dev_mutex);
		c_data->dev->vc_client = c_data;
		c_data->dev->vp_client = c_data;

		if (!c_data->vc_format.clk_freq) {
			rc = -EINVAL;
			goto free_res;
		}

		rate = c_data->vc_format.clk_freq / 100 * 102;
		rate_rc = clk_round_rate(dev->vcap_clk, rate);
		if (rate_rc <= 0) {
			pr_err("%s: Failed core rnd_rate\n", __func__);
			rc = -EINVAL;
			goto free_res;
		}
		rate = (unsigned long)rate_rc;
		rc = clk_set_rate(dev->vcap_clk, rate);
		if (rc < 0)
			goto free_res;

		rate = (c_data->vc_format.hactive_end -
			c_data->vc_format.hactive_start);

		if (c_data->vc_format.color_space)
			rate *= 3;
		else
			rate *= 2;

		rate *= (c_data->vc_format.vactive_end -
			c_data->vc_format.vactive_start);
		rate *= c_data->vc_format.frame_rate;
		rate *= 2;
		if (rate == 0)
			goto free_res;

		rc = request_bus_bw(dev, rate);
		if (rc < 0)
			goto free_res;

		rc = streamon_validate_q(&c_data->vc_vidq);
		if (rc < 0)
			return rc;
		rc = streamon_validate_q(&c_data->vp_in_vidq);
		if (rc < 0)
			goto free_res;
		rc = streamon_validate_q(&c_data->vp_out_vidq);
		if (rc < 0)
			goto free_res;

		rc = config_vc_format(c_data);
		if (rc < 0)
			goto free_res;
		rc = config_vp_format(c_data);
		if (rc < 0)
			goto free_res;
		rc = init_motion_buf(c_data);
		if (rc < 0)
			goto free_res;

		if (dev->nr_param.mode) {
			rc = init_nr_buf(c_data);
			if (rc < 0)
				goto s_on_deinit_m_buf;
		}

		c_data->dev->vc_to_vp_work.cd = c_data;
		c_data->vp_action.vp_state = VP_FRAME1;
		c_data->streaming = 1;

		/* These stream on calls should not fail */
		rc = vb2_streamon(&c_data->vc_vidq,
				V4L2_BUF_TYPE_VIDEO_CAPTURE);
		if (rc < 0)
			goto s_on_deinit_nr_buf;

		rc = vb2_streamon(&c_data->vp_in_vidq,
				V4L2_BUF_TYPE_INTERLACED_IN_DECODER);
		if (rc < 0)
			goto s_on_deinit_nr_buf;

		rc = vb2_streamon(&c_data->vp_out_vidq,
				V4L2_BUF_TYPE_VIDEO_OUTPUT);
		if (rc < 0)
			goto s_on_deinit_nr_buf;
		break;
	default:
		pr_err("VCAP Error: %s: Operation Mode type", __func__);
		return -ENOTRECOVERABLE;
	}
	return 0;

s_on_deinit_nr_buf:
	if (dev->nr_param.mode)
		deinit_nr_buf(c_data);
s_on_deinit_m_buf:
	deinit_motion_buf(c_data);
free_res:
	mutex_lock(&dev->dev_mutex);
	if (c_data->op_mode == VC_VCAP_OP) {
		dev->vc_resource = 0;
		c_data->dev->vc_client = NULL;
	} else if (c_data->op_mode == VP_VCAP_OP) {
		dev->vp_resource = 0;
		c_data->dev->vp_client = NULL;
	} else if (c_data->op_mode == VC_AND_VP_VCAP_OP) {
		c_data->dev->vc_client = NULL;
		c_data->dev->vp_client = NULL;
		dev->vc_resource = 0;
		dev->vp_resource = 0;
	}
	mutex_unlock(&dev->dev_mutex);
	return rc;
}

int streamoff_validate_q(struct vb2_queue *q)
{
	if (q->fileio) {
		dprintk(1, "streamoff: file io in progress\n");
		return -EBUSY;
	}

	if (!q->streaming) {
		dprintk(1, "streamoff: not streaming\n");
		return -EINVAL;
	}
	return 0;
}

int streamoff_work(struct vcap_client_data *c_data)
{
	struct vcap_dev *dev = c_data->dev;
	int rc;
	switch (c_data->op_mode) {
	case VC_VCAP_OP:
		if (c_data != dev->vc_client) {
			pr_err("VCAP Err: %s: VC held by other client",
				__func__);
			return -EBUSY;
		}
		mutex_lock(&dev->dev_mutex);
		if (!dev->vc_resource) {
			pr_err("VCAP Err: %s: VC res not acquired", __func__);
			mutex_unlock(&dev->dev_mutex);
			return -EBUSY;
		}
		dev->vc_resource = 0;
		mutex_unlock(&dev->dev_mutex);
		c_data->streaming = 0;
		rc = vb2_streamoff(&c_data->vc_vidq,
				V4L2_BUF_TYPE_VIDEO_CAPTURE);
		if (rc >= 0)
			atomic_set(&c_data->dev->vc_enabled, 0);
		return rc;
	case VP_VCAP_OP:
		if (c_data != dev->vp_client) {
			pr_err("VCAP Err: %s: VP held by other client",
				__func__);
			return -EBUSY;
		}
		mutex_lock(&dev->dev_mutex);
		if (!dev->vp_resource) {
			pr_err("VCAP Err: %s: VP res not acquired", __func__);
			mutex_unlock(&dev->dev_mutex);
			return -EBUSY;
		}
		dev->vp_resource = 0;
		mutex_unlock(&dev->dev_mutex);
		rc = streamoff_validate_q(&c_data->vp_in_vidq);
		if (rc < 0)
			return rc;
		rc = streamoff_validate_q(&c_data->vp_out_vidq);
		if (rc < 0)
			return rc;
		c_data->streaming = 0;

		/* These stream on calls should not fail */
		rc = vb2_streamoff(&c_data->vp_in_vidq,
				V4L2_BUF_TYPE_INTERLACED_IN_DECODER);
		if (rc < 0)
			return rc;

		rc = vb2_streamoff(&c_data->vp_out_vidq,
				V4L2_BUF_TYPE_VIDEO_OUTPUT);
		if (rc < 0)
			return rc;

		deinit_motion_buf(c_data);
		if (dev->nr_param.mode)
			deinit_nr_buf(c_data);
		atomic_set(&c_data->dev->vp_enabled, 0);
		return rc;
	case VC_AND_VP_VCAP_OP:
		if (c_data != dev->vp_client || c_data != dev->vc_client) {
			pr_err("VCAP Err: %s: VC/VP held by other client",
				__func__);
			return -EBUSY;
		}
		mutex_lock(&dev->dev_mutex);
		if (!(dev->vc_resource || dev->vp_resource)) {
			pr_err("VCAP Err: %s: VC or VP res not acquired",
				__func__);
			mutex_unlock(&dev->dev_mutex);
			return -EBUSY;
		}
		dev->vc_resource = 0;
		dev->vp_resource = 0;
		mutex_unlock(&dev->dev_mutex);
		rc = streamoff_validate_q(&c_data->vc_vidq);
		if (rc < 0)
			return rc;
		rc = streamoff_validate_q(&c_data->vp_in_vidq);
		if (rc < 0)
			return rc;
		rc = streamoff_validate_q(&c_data->vp_out_vidq);
		if (rc < 0)
			return rc;

		/* These stream on calls should not fail */
		c_data->streaming = 0;
		rc = vb2_streamoff(&c_data->vc_vidq,
				V4L2_BUF_TYPE_VIDEO_CAPTURE);
		if (rc < 0)
			return rc;

		rc = vb2_streamoff(&c_data->vp_in_vidq,
				V4L2_BUF_TYPE_INTERLACED_IN_DECODER);
		if (rc < 0)
			return rc;

		rc = vb2_streamoff(&c_data->vp_out_vidq,
				V4L2_BUF_TYPE_VIDEO_OUTPUT);
		if (rc < 0)
			return rc;

		deinit_motion_buf(c_data);
		if (dev->nr_param.mode)
			deinit_nr_buf(c_data);
		atomic_set(&c_data->dev->vc_enabled, 0);
		atomic_set(&c_data->dev->vp_enabled, 0);
		return rc;
	default:
		pr_err("VCAP Error: %s: Unknown Operation mode", __func__);
		return -ENOTRECOVERABLE;
	}
}

static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{
	struct vcap_client_data *c_data = to_client_data(file->private_data);
	return streamoff_work(c_data);
}

static int vidioc_subscribe_event(struct v4l2_fh *fh,
			struct v4l2_event_subscription *sub)
{
	int rc;
	if (sub->type == V4L2_EVENT_ALL) {
		sub->type = V4L2_EVENT_PRIVATE_START +
				VCAP_GENERIC_NOTIFY_EVENT;
		sub->id = 0;
		do {
			rc = v4l2_event_subscribe(fh, sub, 16);
			if (rc < 0) {
				sub->type = V4L2_EVENT_ALL;
				v4l2_event_unsubscribe(fh, sub);
				return rc;
			}
			sub->type++;
		} while (sub->type !=
			V4L2_EVENT_PRIVATE_START + VCAP_MAX_NOTIFY_EVENT);
	} else {
		rc = v4l2_event_subscribe(fh, sub, 16);
	}
	return rc;
}

static int vidioc_unsubscribe_event(struct v4l2_fh *fh,
			struct v4l2_event_subscription *sub)
{
	return v4l2_event_unsubscribe(fh, sub);
}

static long vidioc_default(struct file *file, void *fh, bool valid_prio,
						int cmd, void *arg)
{
	struct vcap_client_data *c_data = to_client_data(file->private_data);
	struct vcap_dev *dev = c_data->dev;
	struct nr_param *param;
	int	val;
	unsigned long flags = 0;
	int ret;

	switch (cmd) {
	case VCAPIOC_NR_S_PARAMS:

		if (c_data->streaming != 0 &&
				(!(!((struct nr_param *) arg)->mode) !=
				!(!(dev->nr_param.mode)))) {
			pr_err("ERR: Trying to toggle on/off while VP is already running");
			return -EBUSY;
		}


		spin_lock_irqsave(&c_data->cap_slock, flags);
		ret = nr_s_param(c_data, (struct nr_param *) arg);
		if (ret < 0) {
			spin_unlock_irqrestore(&c_data->cap_slock, flags);
			return ret;
		}
		param = (struct nr_param *) arg;
		dev->nr_param = *param;
		if (param->mode == NR_AUTO)
			s_default_nr_val(&dev->nr_param);
		dev->nr_update = true;
		spin_unlock_irqrestore(&c_data->cap_slock, flags);
		break;
	case VCAPIOC_NR_G_PARAMS:
		*((struct nr_param *)arg) = dev->nr_param;
		if (dev->nr_param.mode != NR_DISABLE) {
			if (c_data->streaming)
				nr_g_param(c_data, (struct nr_param *) arg);
			else
				(*(struct nr_param *) arg) =
					dev->nr_param;
		}
		break;
	case VCAPIOC_S_NUM_VC_BUF:
		val = (*(int *) arg);
		if (val < VCAP_VC_MIN_BUF || val > VCAP_VC_MAX_BUF)
			return -EINVAL;
		dev->vc_tot_buf = (uint8_t) val;
		break;
	default:
		return -EINVAL;
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
	if (!dev)
		return -EINVAL;
	c_data = kzalloc(sizeof(*c_data), GFP_KERNEL);
	if (!c_data)
		return -ENOMEM;

	c_data->dev = dev;

	spin_lock_init(&c_data->cap_slock);

	/* initialize vc queue */
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
		goto vc_q_failed;

	/* initialize vp in queue */
	q = &c_data->vp_in_vidq;
	memset(q, 0, sizeof(c_data->vp_in_vidq));
	q->type = V4L2_BUF_TYPE_INTERLACED_IN_DECODER;
	q->io_modes = VB2_USERPTR;
	q->drv_priv = c_data;
	q->buf_struct_size = sizeof(struct vcap_buffer);
	q->ops = &vp_in_video_qops;
	q->mem_ops = &vcap_mem_ops;
	ret = vb2_queue_init(q);
	if (ret < 0)
		goto vp_in_q_failed;

	/* initialize vp out queue */
	q = &c_data->vp_out_vidq;
	memset(q, 0, sizeof(c_data->vp_out_vidq));
	q->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	q->io_modes = VB2_USERPTR;
	q->drv_priv = c_data;
	q->buf_struct_size = sizeof(struct vcap_buffer);
	q->ops = &vp_out_video_qops;
	q->mem_ops = &vcap_mem_ops;

	ret = vb2_queue_init(q);
	if (ret < 0)
		goto vp_out_q_failed;

	INIT_LIST_HEAD(&c_data->vc_action.active);
	INIT_LIST_HEAD(&c_data->vp_action.in_active);
	INIT_LIST_HEAD(&c_data->vp_action.out_active);

	v4l2_fh_init(&c_data->vfh, dev->vfd);
	v4l2_fh_add(&c_data->vfh);

	mutex_lock(&dev->dev_mutex);
	atomic_inc(&dev->open_clients);
	ret = atomic_read(&dev->open_clients);
	if (ret == 1) {
		ret = vcap_enable(dev, dev->ddev, 54860000);
		if (ret < 0) {
			pr_err("Err: %s: Power on vcap failed", __func__);
			mutex_unlock(&dev->dev_mutex);
			goto vcap_power_failed;
		}

		ret = vp_dummy_event(c_data);
		if (ret < 0) {
			pr_err("Err: %s: Dummy Event failed", __func__);
			mutex_unlock(&dev->dev_mutex);
			vcap_disable(dev);
			goto vcap_power_failed;
		}
	}
	mutex_unlock(&dev->dev_mutex);

	file->private_data = &c_data->vfh;
	return 0;

vcap_power_failed:
	atomic_dec(&dev->open_clients);

	v4l2_fh_del(&c_data->vfh);
	v4l2_fh_exit(&c_data->vfh);
	vb2_queue_release(&c_data->vp_out_vidq);
vp_out_q_failed:
	vb2_queue_release(&c_data->vp_in_vidq);
vp_in_q_failed:
	vb2_queue_release(&c_data->vc_vidq);
vc_q_failed:
	kfree(c_data);
	return ret;
}

static int vcap_close(struct file *file)
{
	struct vcap_dev *dev = video_drvdata(file);
	struct vcap_client_data *c_data = to_client_data(file->private_data);
	int ret;

	if (c_data == NULL)
		return 0;

	if (c_data->streaming)
		streamoff_work(c_data);

	mutex_lock(&dev->dev_mutex);
	atomic_dec(&dev->open_clients);
	ret = atomic_read(&dev->open_clients);
	mutex_unlock(&dev->dev_mutex);
	if (ret == 0) {
		vcap_disable(dev);
		dev->vc_tot_buf = 2;
		dev->vp_dummy_complete = false;
	}
	v4l2_fh_del(&c_data->vfh);
	v4l2_fh_exit(&c_data->vfh);
	vb2_queue_release(&c_data->vp_out_vidq);
	vb2_queue_release(&c_data->vp_in_vidq);
	vb2_queue_release(&c_data->vc_vidq);
	if (c_data->dev->vc_client == c_data)
		c_data->dev->vc_client = NULL;
	if (c_data->dev->vp_client == c_data)
		c_data->dev->vp_client = NULL;
	kfree(c_data);
	return 0;
}

unsigned int poll_work(struct vb2_queue *q, struct file *file,
	poll_table *wait, bool write_q)
{
	unsigned long flags;
	struct vb2_buffer *vb = NULL;

	if (q->num_buffers == 0)
		return POLLERR;

	if (list_empty(&q->queued_list))
		return POLLERR;

	poll_wait(file, &q->done_wq, wait);

	spin_lock_irqsave(&q->done_lock, flags);
	if (!list_empty(&q->done_list))
		vb = list_first_entry(&q->done_list, struct vb2_buffer,
					done_entry);
	spin_unlock_irqrestore(&q->done_lock, flags);

	if (vb && (vb->state == VB2_BUF_STATE_DONE
			|| vb->state == VB2_BUF_STATE_ERROR)) {
		return (write_q) ? POLLOUT | POLLWRNORM :
			POLLIN | POLLRDNORM;
	}
	return 0;
}

static unsigned int vcap_poll(struct file *file,
				  struct poll_table_struct *wait)
{
	struct vcap_client_data *c_data = to_client_data(file->private_data);
	struct vb2_queue *q;
	unsigned int mask = 0;

	dprintk(1, "Enter slect/poll\n");

	switch (c_data->op_mode) {
	case VC_VCAP_OP:
		q = &c_data->vc_vidq;
		mask = vb2_poll(q, file, wait);
		break;
	case VP_VCAP_OP:
		q = &c_data->vp_in_vidq;
		mask = poll_work(q, file, wait, 0);
		q = &c_data->vp_out_vidq;
		mask |= poll_work(q, file, wait, 1);
		break;
	case VC_AND_VP_VCAP_OP:
		q = &c_data->vp_out_vidq;
		mask = poll_work(q, file, wait, 0);
		break;
	default:
		pr_err("VCAP Error: %s: Unknown operation mode", __func__);
		return POLLERR;
	}
	if (v4l2_event_pending(&c_data->vfh))
		mask |= POLLPRI;
	poll_wait(file, &(c_data->vfh.wait), wait);
	return mask;
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
	.vidioc_s_fmt_vid_out_mplane	= vidioc_s_fmt_vid_cap,
	.vidioc_g_fmt_vid_out_mplane	= vidioc_g_fmt_vid_cap,
	.vidioc_reqbufs       = vidioc_reqbufs,
	.vidioc_querybuf      = vidioc_querybuf,
	.vidioc_qbuf          = vidioc_qbuf,
	.vidioc_dqbuf         = vidioc_dqbuf,
	.vidioc_streamon      = vidioc_streamon,
	.vidioc_streamoff     = vidioc_streamoff,

	.vidioc_subscribe_event = vidioc_subscribe_event,
	.vidioc_unsubscribe_event = vidioc_unsubscribe_event,
	.vidioc_default = vidioc_default,
};

static struct video_device vcap_template = {
	.name		= "vcap",
	.fops		= &vcap_fops,
	.ioctl_ops	= &vcap_ioctl_ops,
	.release	= video_device_release,
};

static irqreturn_t vcap_vp_handler(int irq_num, void *data)
{
	return vp_handler(vcap_ctrl);
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

	dev->vcirq = platform_get_resource_byname(pdev,
					IORESOURCE_IRQ, "vc_irq");
	if (!dev->vcirq) {
		pr_err("%s: no vc irq resource?\n", __func__);
		ret = -ENODEV;
		goto free_resource;
	}
	dev->vpirq = platform_get_resource_byname(pdev,
					IORESOURCE_IRQ, "vp_irq");
	if (!dev->vpirq) {
		pr_err("%s: no vp irq resource?\n", __func__);
		ret = -ENODEV;
		goto free_resource;
	}


	ret = request_irq(dev->vcirq->start, vcap_vc_handler,
		IRQF_TRIGGER_RISING, "vc_irq", 0);
	if (ret < 0) {
		pr_err("%s: vc irq request fail\n", __func__);
		ret = -EBUSY;
		goto free_resource;
	}
	disable_irq(dev->vcirq->start);

	ret = request_irq(dev->vpirq->start, vcap_vp_handler,
		IRQF_TRIGGER_RISING, "vp_irq", 0);

	if (ret < 0) {
		pr_err("%s: vp irq request fail\n", __func__);
		ret = -EBUSY;
		goto free_resource;
	}
	disable_irq(dev->vpirq->start);

	snprintf(dev->v4l2_dev.name, sizeof(dev->v4l2_dev.name),
			"%s", MSM_VCAP_DRV_NAME);

	ret = v4l2_device_register(NULL, &dev->v4l2_dev);
	if (ret)
		goto free_resource;

	ret = vcap_enable(dev, &pdev->dev, 54860000);
	if (ret)
		goto unreg_dev;
	msm_bus_scale_client_update_request(dev->bus_client_handle, 0);

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

	dev->vcap_wq = create_workqueue("vcap");
	if (!dev->vcap_wq) {
		pr_err("Could not create workqueue");
		goto rel_vdev;
	}

	dev->ion_client = msm_ion_client_create(-1, "vcap");
	if (IS_ERR((void *)dev->ion_client)) {
		pr_err("could not get ion client");
		goto rel_vcap_wq;
	}

	dev->vc_tot_buf = 2;
	atomic_set(&dev->vc_enabled, 0);
	atomic_set(&dev->vp_enabled, 0);
	atomic_set(&dev->open_clients, 0);
	dev->ddev = &pdev->dev;
	mutex_init(&dev->dev_mutex);
	init_waitqueue_head(&dev->vp_dummy_waitq);
	vcap_disable(dev);

	dprintk(1, "Exit probe succesfully");
	return 0;
rel_vcap_wq:
	destroy_workqueue(dev->vcap_wq);
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
	flush_workqueue(dev->vcap_wq);
	destroy_workqueue(dev->vcap_wq);
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
