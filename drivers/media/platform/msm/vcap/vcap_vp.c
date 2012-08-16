/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <mach/camera.h>
#include <linux/io.h>
#include <mach/clk.h>
#include <linux/clk.h>

#include <media/v4l2-event.h>
#include <media/vcap_v4l2.h>
#include <media/vcap_fmt.h>
#include "vcap_vp.h"

static unsigned debug;

#define dprintk(level, fmt, arg...)					\
	do {								\
		if (debug >= level)					\
			printk(KERN_DEBUG "VP: " fmt, ## arg);		\
	} while (0)

void config_nr_buffer(struct vcap_client_data *c_data,
			struct vcap_buffer *buf)
{
	struct vcap_dev *dev = c_data->dev;
	int size = c_data->vp_in_fmt.height * c_data->vp_in_fmt.width;

	writel_relaxed(buf->paddr, VCAP_VP_NR_T2_Y_BASE_ADDR);
	writel_relaxed(buf->paddr + size, VCAP_VP_NR_T2_C_BASE_ADDR);
}

void config_in_buffer(struct vcap_client_data *c_data,
			struct vcap_buffer *buf)
{
	struct vcap_dev *dev = c_data->dev;
	int size = c_data->vp_in_fmt.height * c_data->vp_in_fmt.width;

	writel_relaxed(buf->paddr, VCAP_VP_T2_Y_BASE_ADDR);
	writel_relaxed(buf->paddr + size, VCAP_VP_T2_C_BASE_ADDR);
}

void config_out_buffer(struct vcap_client_data *c_data,
			struct vcap_buffer *buf)
{
	struct vcap_dev *dev = c_data->dev;
	int size;
	size = c_data->vp_out_fmt.height * c_data->vp_out_fmt.width;
	writel_relaxed(buf->paddr, VCAP_VP_OUT_Y_BASE_ADDR);
	writel_relaxed(buf->paddr + size, VCAP_VP_OUT_C_BASE_ADDR);
}

int vp_setup_buffers(struct vcap_client_data *c_data)
{
	struct vp_action *vp_act;
	struct vcap_dev *dev;
	unsigned long flags = 0;

	if (!c_data->streaming)
		return -ENOEXEC;
	dev = c_data->dev;
	dprintk(2, "Start setup buffers\n");

	if (dev->vp_shutdown) {
		dprintk(1, "%s: VP shutting down, no buf setup\n",
			__func__);
		return -EPERM;
	}

	/* No need to verify vp_client is not NULL caller does so */
	vp_act = &dev->vp_client->vp_action;

	spin_lock_irqsave(&dev->vp_client->cap_slock, flags);
	if (list_empty(&vp_act->in_active)) {
		spin_unlock_irqrestore(&dev->vp_client->cap_slock, flags);
		dprintk(1, "%s: VP We have no more input buffers\n",
				__func__);
		return -EAGAIN;
	}

	if (list_empty(&vp_act->out_active)) {
		spin_unlock_irqrestore(&dev->vp_client->cap_slock,
			flags);
		dprintk(1, "%s: VP We have no more output buffers\n",
		   __func__);
		return -EAGAIN;
	}

	vp_act->bufT2 = list_entry(vp_act->in_active.next,
			struct vcap_buffer, list);
	list_del(&vp_act->bufT2->list);

	vp_act->bufOut = list_entry(vp_act->out_active.next,
			struct vcap_buffer, list);
	list_del(&vp_act->bufOut->list);
	spin_unlock_irqrestore(&dev->vp_client->cap_slock, flags);

	config_in_buffer(c_data, vp_act->bufT2);
	config_out_buffer(c_data, vp_act->bufOut);
	return 0;
}

static void mov_buf_to_vc(struct work_struct *work)
{
	struct vp_work_t *vp_work = container_of(work, struct vp_work_t, work);
	struct v4l2_buffer p;
	struct vb2_buffer *vb_vc;
	struct vcap_buffer *buf_vc;
	struct vb2_buffer *vb_vp;
	struct vcap_buffer *buf_vp;
	int rc;

	p.memory = V4L2_MEMORY_USERPTR;

	/* This loop exits when there is no more buffers left */
	while (1) {
		p.type = V4L2_BUF_TYPE_INTERLACED_IN_DECODER;
		if (!vp_work->cd->streaming)
			return;
		rc = vcvp_dqbuf(&vp_work->cd->vp_in_vidq, &p);
		if (rc < 0)
			return;

		vb_vc = vp_work->cd->vc_vidq.bufs[p.index];
		if (NULL == vb_vc) {
			dprintk(1, "%s: buffer is NULL\n", __func__);
			vcvp_qbuf(&vp_work->cd->vp_in_vidq, &p);
			return;
		}
		buf_vc = container_of(vb_vc, struct vcap_buffer, vb);

		vb_vp = vp_work->cd->vp_in_vidq.bufs[p.index];
		if (NULL == vb_vp) {
			dprintk(1, "%s: buffer is NULL\n", __func__);
			vcvp_qbuf(&vp_work->cd->vp_in_vidq, &p);
			return;
		}
		buf_vp = container_of(vb_vp, struct vcap_buffer, vb);
		buf_vc->ion_handle = buf_vp->ion_handle;
		buf_vc->paddr = buf_vp->paddr;
		buf_vp->ion_handle = NULL;
		buf_vp->paddr = 0;

		p.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		/* This call should not fail */
		rc = vcvp_qbuf(&vp_work->cd->vc_vidq, &p);
		if (rc < 0) {
			dprintk(1, "%s: qbuf to vc failed\n", __func__);
			buf_vp->ion_handle = buf_vc->ion_handle;
			buf_vp->paddr = buf_vc->paddr;
			buf_vc->ion_handle = NULL;
			buf_vc->paddr = 0;
			p.type = V4L2_BUF_TYPE_INTERLACED_IN_DECODER;
			vcvp_qbuf(&vp_work->cd->vp_in_vidq, &p);
		}
	}
}

void update_nr_value(struct vcap_dev *dev)
{
	struct nr_param *par;
	par = &dev->nr_param;
	if (par->mode == NR_MANUAL) {
		writel_relaxed(par->window << 24 | par->decay_ratio << 20,
			VCAP_VP_NR_CONFIG);
		writel_relaxed(par->luma.max_blend_ratio << 24 |
			par->luma.scale_diff_ratio << 12 |
			par->luma.diff_limit_ratio << 8  |
			par->luma.scale_motion_ratio << 4 |
			par->luma.blend_limit_ratio << 0,
			VCAP_VP_NR_LUMA_CONFIG);
		writel_relaxed(par->chroma.max_blend_ratio << 24 |
			par->chroma.scale_diff_ratio << 12 |
			par->chroma.diff_limit_ratio << 8  |
			par->chroma.scale_motion_ratio << 4 |
			par->chroma.blend_limit_ratio << 0,
			VCAP_VP_NR_CHROMA_CONFIG);
	}
	dev->nr_update = false;
}

static void vp_wq_fnc(struct work_struct *work)
{
	struct vp_work_t *vp_work = container_of(work, struct vp_work_t, work);
	struct vcap_dev *dev;
	struct vp_action *vp_act;
	unsigned long flags = 0;
	uint32_t irq;
	int rc;
#ifndef TOP_FIELD_FIX
	bool top_field;
#endif

	if (vp_work && vp_work->cd && vp_work->cd->dev)
		dev = vp_work->cd->dev;
	else
		return;

	vp_act = &dev->vp_client->vp_action;

	rc = readl_relaxed(VCAP_OFFSET(0x048));
	while (!(rc & 0x00000100))
		rc = readl_relaxed(VCAP_OFFSET(0x048));

	irq = readl_relaxed(VCAP_VP_INT_STATUS);

	writel_relaxed(0x00000000, VCAP_VP_BAL_VMOTION_STATE);
	writel_relaxed(0x40000000, VCAP_VP_REDUCT_AVG_MOTION2);

	spin_lock_irqsave(&dev->vp_client->cap_slock, flags);
	if (dev->nr_update == true)
		update_nr_value(dev);
	spin_unlock_irqrestore(&dev->vp_client->cap_slock, flags);

	/* Queue the done buffers */
	if (vp_act->vp_state == VP_NORMAL &&
			vp_act->bufNR.nr_pos != TM1_BUF) {
		vb2_buffer_done(&vp_act->bufTm1->vb, VB2_BUF_STATE_DONE);
		if (vp_work->cd->op_mode == VC_AND_VP_VCAP_OP)
			queue_work(dev->vcap_wq, &dev->vp_to_vc_work.work);
	}

	vb2_buffer_done(&vp_act->bufOut->vb, VB2_BUF_STATE_DONE);

	/* Cycle to next state */
	if (vp_act->vp_state != VP_NORMAL)
		vp_act->vp_state++;
#ifdef TOP_FIELD_FIX
	vp_act->top_field = !vp_act->top_field;
#endif

	/* Cycle Buffers*/
	if (dev->nr_param.mode) {
		if (vp_act->bufNR.nr_pos == TM1_BUF)
			vp_act->bufNR.nr_pos = BUF_NOT_IN_USE;

		if (vp_act->bufNR.nr_pos != BUF_NOT_IN_USE)
			vp_act->bufNR.nr_pos++;

		vp_act->bufTm1 = vp_act->bufT0;
		vp_act->bufT0 = vp_act->bufT1;
		vp_act->bufT1 = vp_act->bufNRT2;
		vp_act->bufNRT2 = vp_act->bufT2;
		config_nr_buffer(vp_work->cd, vp_act->bufNRT2);
	} else {
		vp_act->bufTm1 = vp_act->bufT0;
		vp_act->bufT0 = vp_act->bufT1;
		vp_act->bufT1 = vp_act->bufT2;
	}

	rc = vp_setup_buffers(vp_work->cd);
	if (rc < 0) {
		/* setup_buf failed because we are waiting for buffers */
		writel_relaxed(0x00000000, VCAP_VP_INTERRUPT_ENABLE);
		writel_iowmb(irq, VCAP_VP_INT_CLEAR);
		atomic_set(&dev->vp_enabled, 0);
		if (dev->vp_shutdown)
			wake_up(&dev->vp_dummy_waitq);
		return;
	}

	/* Config VP */
#ifndef TOP_FIELD_FIX
	if (vp_act->bufT2->vb.v4l2_buf.field == V4L2_FIELD_TOP)
		top_field = 1;
#endif

#ifdef TOP_FIELD_FIX
	writel_iowmb(0x00000000 | vp_act->top_field << 0, VCAP_VP_CTRL);
	writel_iowmb(0x00010000 | vp_act->top_field << 0, VCAP_VP_CTRL);
#else
	writel_iowmb(0x00000000 | top_field, VCAP_VP_CTRL);
	writel_iowmb(0x00010000 | top_field, VCAP_VP_CTRL);
#endif
	enable_irq(dev->vpirq->start);
	writel_iowmb(irq, VCAP_VP_INT_CLEAR);
}

irqreturn_t vp_handler(struct vcap_dev *dev)
{
	struct vcap_client_data *c_data;
	struct vp_action *vp_act;
	struct v4l2_event v4l2_evt;
	uint32_t irq;
	int rc;

	irq = readl_relaxed(VCAP_VP_INT_STATUS);
	if (dev->vp_dummy_event == true) {
		writel_relaxed(irq, VCAP_VP_INT_CLEAR);
		dev->vp_dummy_complete = true;
		wake_up(&dev->vp_dummy_waitq);
		return IRQ_HANDLED;
	}

	if (irq & 0x02000000) {
		v4l2_evt.type = V4L2_EVENT_PRIVATE_START +
			VCAP_VP_REG_R_ERR_EVENT;
		v4l2_event_queue(dev->vfd, &v4l2_evt);
	}
	if (irq & 0x01000000) {
		v4l2_evt.type = V4L2_EVENT_PRIVATE_START +
			VCAP_VC_LINE_ERR_EVENT;
		v4l2_event_queue(dev->vfd, &v4l2_evt);
	}
	if (irq & 0x00020000) {
		v4l2_evt.type = V4L2_EVENT_PRIVATE_START +
			VCAP_VP_IN_HEIGHT_ERR_EVENT;
		v4l2_event_queue(dev->vfd, &v4l2_evt);
	}
	if (irq & 0x00010000) {
		v4l2_evt.type = V4L2_EVENT_PRIVATE_START +
			VCAP_VP_IN_WIDTH_ERR_EVENT;
		v4l2_event_queue(dev->vfd, &v4l2_evt);
	}

	dprintk(1, "%s: irq=0x%08x\n", __func__, irq);
	if (!(irq & (VP_PIC_DONE | VP_MODE_CHANGE))) {
		writel_relaxed(irq, VCAP_VP_INT_CLEAR);
		pr_err("VP IRQ shows some error\n");
		return IRQ_HANDLED;
	}

	if (dev->vp_client == NULL) {
		writel_relaxed(irq, VCAP_VP_INT_CLEAR);
		pr_err("VC: There is no active vp client\n");
		return IRQ_HANDLED;
	}

	vp_act = &dev->vp_client->vp_action;
	c_data = dev->vp_client;

	if (vp_act->vp_state == VP_UNKNOWN) {
		writel_relaxed(irq, VCAP_VP_INT_CLEAR);
		pr_err("%s: VP is in an unknown state\n",
				__func__);
		return -EAGAIN;
	}

	INIT_WORK(&dev->vp_work.work, vp_wq_fnc);
	dev->vp_work.cd = c_data;
	rc = queue_work(dev->vcap_wq, &dev->vp_work.work);

	disable_irq_nosync(dev->vpirq->start);
	return IRQ_HANDLED;
}

int vp_sw_reset(struct vcap_dev *dev)
{
	int timeout;
	writel_iowmb(0x00000010, VCAP_SW_RESET_REQ);
	timeout = 10000;
	while (1) {
		if (!(readl_relaxed(VCAP_SW_RESET_STATUS) & 0x10))
			break;
		timeout--;
		if (timeout == 0) {
			/* This should not happen */
			pr_err("VP is not resetting properly\n");
			writel_iowmb(0x00000000, VCAP_SW_RESET_REQ);
			return -EINVAL;
		}
	}
	return 0;
}

void vp_stop_capture(struct vcap_client_data *c_data)
{
	struct vcap_dev *dev = c_data->dev;
	int rc;

	dev->vp_shutdown = true;
	flush_workqueue(dev->vcap_wq);

	if (atomic_read(&dev->vp_enabled) == 1) {
		rc = wait_event_interruptible_timeout(dev->vp_dummy_waitq,
				!atomic_read(&dev->vp_enabled),
				msecs_to_jiffies(50));
		if (rc == 0 && atomic_read(&dev->vp_enabled) == 1) {
			/* This should not happen, if it does hw is stuck */
			pr_err("%s: VP Timeout and VP still running\n",
				__func__);
		}
	}

	vp_sw_reset(dev);
	dev->vp_shutdown = false;
}

int config_vp_format(struct vcap_client_data *c_data)
{
	struct vcap_dev *dev = c_data->dev;
	int rc;

	INIT_WORK(&dev->vp_to_vc_work.work, mov_buf_to_vc);
	dev->vp_to_vc_work.cd = c_data;

	/* SW restart VP */
	rc = vp_sw_reset(dev);
	if (rc < 0)
		return rc;

	/* Film Mode related settings */
	writel_iowmb(0x00000000, VCAP_VP_FILM_PROJECTION_T0);
	writel_relaxed(0x00000000, VCAP_VP_FILM_PROJECTION_T2);
	writel_relaxed(0x00000000, VCAP_VP_FILM_PAST_MAX_PROJ);
	writel_relaxed(0x00000000, VCAP_VP_FILM_PAST_MIN_PROJ);
	writel_relaxed(0x00000000, VCAP_VP_FILM_SEQUENCE_HIST);
	writel_relaxed(0x00000000, VCAP_VP_FILM_MODE_STATE);

	writel_relaxed(0x00000000, VCAP_VP_BAL_VMOTION_STATE);
	writel_relaxed(0x00000010, VCAP_VP_REDUCT_AVG_MOTION);
	writel_relaxed(0x40000000, VCAP_VP_REDUCT_AVG_MOTION2);
	writel_relaxed(0x40000000, VCAP_VP_NR_AVG_LUMA);
	writel_relaxed(0x40000000, VCAP_VP_NR_AVG_CHROMA);
	writel_relaxed(0x40000000, VCAP_VP_NR_CTRL_LUMA);
	writel_relaxed(0x40000000, VCAP_VP_NR_CTRL_CHROMA);
	writel_relaxed(0x00000000, VCAP_VP_BAL_AVG_BLEND);
	writel_relaxed(0x00000000, VCAP_VP_VMOTION_HIST);
	writel_relaxed(0x05047D19, VCAP_VP_FILM_ANALYSIS_CONFIG);
	writel_relaxed(0x20260200, VCAP_VP_FILM_STATE_CONFIG);
	writel_relaxed(0x23A60114, VCAP_VP_FVM_CONFIG);
	writel_relaxed(0x03043210, VCAP_VP_FILM_ANALYSIS_CONFIG2);
	writel_relaxed(0x04DB7A51, VCAP_VP_MIXED_ANALYSIS_CONFIG);
	writel_relaxed(0x14224916, VCAP_VP_SPATIAL_CONFIG);
	writel_relaxed(0x83270400, VCAP_VP_SPATIAL_CONFIG2);
	writel_relaxed(0x0F000F92, VCAP_VP_SPATIAL_CONFIG3);
	writel_relaxed(0x00000000, VCAP_VP_TEMPORAL_CONFIG);
	writel_relaxed(0x00000000, VCAP_VP_PIXEL_DIFF_CONFIG);
	writel_relaxed(0x0C090511, VCAP_VP_H_FREQ_CONFIG);
	writel_relaxed(0x0A000000, VCAP_VP_NR_CONFIG);
	writel_relaxed(0x008F4149, VCAP_VP_NR_LUMA_CONFIG);
	writel_relaxed(0x008F4149, VCAP_VP_NR_CHROMA_CONFIG);
	writel_relaxed(0x43C0FD0C, VCAP_VP_BAL_CONFIG);
	writel_relaxed(0x00000255, VCAP_VP_BAL_MOTION_CONFIG);
	writel_relaxed(0x24154252, VCAP_VP_BAL_LIGHT_COMB);
	writel_relaxed(0x10024414, VCAP_VP_BAL_VMOTION_CONFIG);
	writel_relaxed(0x00000002, VCAP_VP_NR_CONFIG2);
	writel_relaxed((c_data->vp_out_fmt.height-1)<<16 |
			(c_data->vp_out_fmt.width - 1), VCAP_VP_FRAME_SIZE);
	writel_relaxed(0x00000000, VCAP_VP_SPLIT_SCRN_CTRL);

	return 0;
}

int init_motion_buf(struct vcap_client_data *c_data)
{
	int rc;
	struct vcap_dev *dev = c_data->dev;
	struct ion_handle *handle = NULL;
	unsigned long paddr, ionflag = 0;
	void *vaddr;
	size_t len;
	size_t size = ((c_data->vp_out_fmt.width + 63) >> 6) *
		((c_data->vp_out_fmt.height + 7) >> 3) * 16;

	if (c_data->vp_action.motionHandle) {
		pr_err("Motion buffer has already been created");
		return -ENOEXEC;
	}

	handle = ion_alloc(dev->ion_client, size, SZ_4K,
			ION_HEAP(ION_CP_MM_HEAP_ID));
	if (IS_ERR_OR_NULL(handle)) {
		pr_err("%s: ion_alloc failed\n", __func__);
		return -ENOMEM;
	}
	rc = ion_phys(dev->ion_client, handle, &paddr, &len);
	if (rc < 0) {
		pr_err("%s: ion_phys failed\n", __func__);
		ion_free(dev->ion_client, handle);
		return rc;
	}

	rc = ion_handle_get_flags(dev->ion_client, handle, &ionflag);
	if (rc) {
		pr_err("%s: get flags ion handle failed\n", __func__);
		ion_free(dev->ion_client, handle);
		return rc;
	}

	vaddr = ion_map_kernel(dev->ion_client, handle, ionflag);
	if (IS_ERR(vaddr)) {
		pr_err("%s: Map motion buffer failed\n", __func__);
		ion_free(dev->ion_client, handle);
		rc = -ENOMEM;
		return rc;
	}

	memset(vaddr, 0, size);
	c_data->vp_action.motionHandle = handle;

	vaddr = NULL;
	ion_unmap_kernel(dev->ion_client, handle);

	writel_iowmb(paddr, VCAP_VP_MOTION_EST_ADDR);
	return 0;
}

void deinit_motion_buf(struct vcap_client_data *c_data)
{
	struct vcap_dev *dev = c_data->dev;
	if (!c_data->vp_action.motionHandle) {
		pr_err("Motion buffer has not been created");
		return;
	}

	writel_iowmb(0x00000000, VCAP_VP_MOTION_EST_ADDR);
	ion_free(dev->ion_client, c_data->vp_action.motionHandle);
	c_data->vp_action.motionHandle = NULL;
	return;
}

int init_nr_buf(struct vcap_client_data *c_data)
{
	struct vcap_dev *dev = c_data->dev;
	struct ion_handle *handle = NULL;
	size_t frame_size, tot_size, len;
	unsigned long paddr;
	int rc;

	if (c_data->vp_action.bufNR.nr_handle) {
		pr_err("NR buffer has already been created");
		return -ENOEXEC;
	}

	frame_size = c_data->vp_in_fmt.width * c_data->vp_in_fmt.height;
	if (c_data->vp_in_fmt.pixfmt == V4L2_PIX_FMT_NV16)
		tot_size = frame_size * 2;
	else
		tot_size = frame_size / 2 * 3;

	handle = ion_alloc(dev->ion_client, tot_size, SZ_4K,
			ION_HEAP(ION_CP_MM_HEAP_ID));
	if (IS_ERR_OR_NULL(handle)) {
		pr_err("%s: ion_alloc failed\n", __func__);
		return -ENOMEM;
	}

	rc = ion_phys(dev->ion_client, handle, &paddr, &len);
	if (rc < 0) {
		pr_err("%s: ion_phys failed\n", __func__);
		ion_free(dev->ion_client, handle);
		return rc;
	}

	c_data->vp_action.bufNR.nr_handle = handle;
	update_nr_value(dev);

	c_data->vp_action.bufNR.paddr = paddr;
	rc = readl_relaxed(VCAP_VP_NR_CONFIG2);
	rc |= (((c_data->vp_out_fmt.width / 16) << 20) | 0x1);
	writel_relaxed(rc, VCAP_VP_NR_CONFIG2);
	writel_relaxed(paddr, VCAP_VP_NR_T2_Y_BASE_ADDR);
	writel_relaxed(paddr + frame_size, VCAP_VP_NR_T2_C_BASE_ADDR);
	c_data->vp_action.bufNR.nr_pos = NRT2_BUF;
	return 0;
}

void deinit_nr_buf(struct vcap_client_data *c_data)
{
	struct vcap_dev *dev = c_data->dev;
	struct nr_buffer *buf;
	uint32_t rc;

	if (!c_data->vp_action.bufNR.nr_handle) {
		pr_err("NR buffer has not been created");
		return;
	}
	buf = &c_data->vp_action.bufNR;

	rc = readl_relaxed(VCAP_VP_NR_CONFIG2);
	rc &= !(0x0FF00001);
	writel_relaxed(rc, VCAP_VP_NR_CONFIG2);

	ion_free(dev->ion_client, buf->nr_handle);
	buf->nr_handle = NULL;
	buf->paddr = 0;
	return;
}

int nr_s_param(struct vcap_client_data *c_data, struct nr_param *param)
{
	if (param->mode != NR_MANUAL)
		return 0;

	/* Verify values in range */
	if (param->window > VP_NR_MAX_WINDOW)
		return -EINVAL;
	if (param->luma.max_blend_ratio > VP_NR_MAX_RATIO)
		return -EINVAL;
	if (param->luma.scale_diff_ratio > VP_NR_MAX_RATIO)
		return -EINVAL;
	if (param->luma.diff_limit_ratio > VP_NR_MAX_RATIO)
		return -EINVAL;
	if (param->luma.scale_motion_ratio > VP_NR_MAX_RATIO)
		return -EINVAL;
	if (param->luma.blend_limit_ratio > VP_NR_MAX_RATIO)
		return -EINVAL;
	if (param->chroma.max_blend_ratio > VP_NR_MAX_RATIO)
		return -EINVAL;
	if (param->chroma.scale_diff_ratio > VP_NR_MAX_RATIO)
		return -EINVAL;
	if (param->chroma.diff_limit_ratio > VP_NR_MAX_RATIO)
		return -EINVAL;
	if (param->chroma.scale_motion_ratio > VP_NR_MAX_RATIO)
		return -EINVAL;
	if (param->chroma.blend_limit_ratio > VP_NR_MAX_RATIO)
		return -EINVAL;
	return 0;
}

void nr_g_param(struct vcap_client_data *c_data, struct nr_param *param)
{
	struct vcap_dev *dev = c_data->dev;
	uint32_t rc;
	rc = readl_relaxed(VCAP_VP_NR_CONFIG);
	param->window = BITS_VALUE(rc, 24, 4);
	param->decay_ratio = BITS_VALUE(rc, 20, 3);

	rc = readl_relaxed(VCAP_VP_NR_LUMA_CONFIG);
	param->luma.max_blend_ratio = BITS_VALUE(rc, 24, 4);
	param->luma.scale_diff_ratio = BITS_VALUE(rc, 12, 4);
	param->luma.diff_limit_ratio = BITS_VALUE(rc, 8, 4);
	param->luma.scale_motion_ratio = BITS_VALUE(rc, 4, 4);
	param->luma.blend_limit_ratio = BITS_VALUE(rc, 0, 4);

	rc = readl_relaxed(VCAP_VP_NR_CHROMA_CONFIG);
	param->chroma.max_blend_ratio = BITS_VALUE(rc, 24, 4);
	param->chroma.scale_diff_ratio = BITS_VALUE(rc, 12, 4);
	param->chroma.diff_limit_ratio = BITS_VALUE(rc, 8, 4);
	param->chroma.scale_motion_ratio = BITS_VALUE(rc, 4, 4);
	param->chroma.blend_limit_ratio = BITS_VALUE(rc, 0, 4);
}

void s_default_nr_val(struct nr_param *param)
{
	param->window = 10;
	param->decay_ratio = 0;
	param->luma.max_blend_ratio = 0;
	param->luma.scale_diff_ratio = 4;
	param->luma.diff_limit_ratio = 1;
	param->luma.scale_motion_ratio = 4;
	param->luma.blend_limit_ratio = 9;
	param->chroma.max_blend_ratio = 0;
	param->chroma.scale_diff_ratio = 4;
	param->chroma.diff_limit_ratio = 1;
	param->chroma.scale_motion_ratio = 4;
	param->chroma.blend_limit_ratio = 9;
}

int vp_dummy_event(struct vcap_client_data *c_data)
{
	struct vcap_dev *dev = c_data->dev;
	unsigned int width, height;
	struct ion_handle *handle = NULL;
	unsigned long paddr;
	size_t len;
	uint32_t reg;
	int rc = 0;

	dprintk(2, "%s: Start VP dummy event\n", __func__);
	handle = ion_alloc(dev->ion_client, 0x1200, SZ_4K,
			ION_HEAP(ION_CP_MM_HEAP_ID));
	if (IS_ERR_OR_NULL(handle)) {
		pr_err("%s: ion_alloc failed\n", __func__);
		return -ENOMEM;
	}

	rc = ion_phys(dev->ion_client, handle, &paddr, &len);
	if (rc < 0) {
		pr_err("%s: ion_phys failed\n", __func__);
		ion_free(dev->ion_client, handle);
		return rc;
	}

	width = c_data->vp_out_fmt.width;
	height = c_data->vp_out_fmt.height;

	c_data->vp_out_fmt.width = 0x3F;
	c_data->vp_out_fmt.height = 0x16;

	config_vp_format(c_data);
	writel_relaxed(paddr, VCAP_VP_T1_Y_BASE_ADDR);
	writel_relaxed(paddr + 0x2C0, VCAP_VP_T1_C_BASE_ADDR);
	writel_relaxed(paddr + 0x440, VCAP_VP_T2_Y_BASE_ADDR);
	writel_relaxed(paddr + 0x700, VCAP_VP_T2_C_BASE_ADDR);
	writel_relaxed(paddr + 0x880, VCAP_VP_OUT_Y_BASE_ADDR);
	writel_relaxed(paddr + 0xB40, VCAP_VP_OUT_C_BASE_ADDR);
	writel_iowmb(paddr + 0x1100, VCAP_VP_MOTION_EST_ADDR);
	writel_relaxed(4 << 20 | 0x2 << 4, VCAP_VP_IN_CONFIG);
	writel_relaxed(4 << 20 | 0x1 << 4, VCAP_VP_OUT_CONFIG);

	dev->vp_dummy_event = true;

	enable_irq(dev->vpirq->start);
	writel_relaxed(0x01100101, VCAP_VP_INTERRUPT_ENABLE);
	writel_iowmb(0x00000000, VCAP_VP_CTRL);
	writel_iowmb(0x00010000, VCAP_VP_CTRL);

	rc = wait_event_interruptible_timeout(dev->vp_dummy_waitq,
		dev->vp_dummy_complete, msecs_to_jiffies(50));
	if (!rc && !dev->vp_dummy_complete) {
		pr_err("%s: VP dummy event timeout", __func__);
		rc = -ETIME;

		vp_sw_reset(dev);
		dev->vp_dummy_complete = false;
	}

	writel_relaxed(0x00000000, VCAP_VP_INTERRUPT_ENABLE);
	disable_irq(dev->vpirq->start);
	dev->vp_dummy_event = false;

	reg = readl_relaxed(VCAP_OFFSET(0x0D94));
	writel_relaxed(reg, VCAP_OFFSET(0x0D9C));

	c_data->vp_out_fmt.width = width;
	c_data->vp_out_fmt.height = height;
	ion_free(dev->ion_client, handle);

	dprintk(2, "%s: Exit VP dummy event\n", __func__);
	return rc;
}

int kickoff_vp(struct vcap_client_data *c_data)
{
	struct vcap_dev *dev;
	struct vp_action *vp_act;
	unsigned long flags = 0;
	unsigned int chroma_fmt = 0;
	int size;
#ifndef TOP_FIELD_FIX
	bool top_field;
#endif

	if (!c_data->streaming)
		return -ENOEXEC;

	dev = c_data->dev;
	dprintk(2, "Start Kickoff\n");

	if (dev->vp_client == NULL) {
		pr_err("No active vp client\n");
		return -ENODEV;
	}
	vp_act = &dev->vp_client->vp_action;

	spin_lock_irqsave(&dev->vp_client->cap_slock, flags);
	if (list_empty(&vp_act->in_active)) {
		spin_unlock_irqrestore(&dev->vp_client->cap_slock, flags);
		pr_err("%s: VP We have no more input buffers\n",
				__func__);
		return -EAGAIN;
	}

	vp_act->bufT1 = list_entry(vp_act->in_active.next,
			struct vcap_buffer, list);
	list_del(&vp_act->bufT1->list);

	if (list_empty(&vp_act->in_active)) {
		spin_unlock_irqrestore(&dev->vp_client->cap_slock, flags);
		list_add(&vp_act->bufT1->list, &vp_act->in_active);
		pr_err("%s: VP We have no more input buffers\n",
				__func__);
		return -EAGAIN;
	}

	vp_act->bufT2 = list_entry(vp_act->in_active.next,
			struct vcap_buffer, list);
	list_del(&vp_act->bufT2->list);

	if (list_empty(&vp_act->out_active)) {
		spin_unlock_irqrestore(&dev->vp_client->cap_slock, flags);
		list_add(&vp_act->bufT2->list, &vp_act->in_active);
		list_add(&vp_act->bufT1->list, &vp_act->in_active);
		pr_err("%s: VP We have no more output buffers\n",
				__func__);
		return -EAGAIN;
	}

	vp_act->bufOut = list_entry(vp_act->out_active.next,
			struct vcap_buffer, list);
	list_del(&vp_act->bufOut->list);
	spin_unlock_irqrestore(&dev->vp_client->cap_slock, flags);

	size = c_data->vp_in_fmt.height * c_data->vp_in_fmt.width;
	writel_relaxed(vp_act->bufT1->paddr, VCAP_VP_T1_Y_BASE_ADDR);
	writel_relaxed(vp_act->bufT1->paddr + size, VCAP_VP_T1_C_BASE_ADDR);

	config_in_buffer(c_data, vp_act->bufT2);
	config_out_buffer(c_data, vp_act->bufOut);

	/* Config VP */
	if (c_data->vp_in_fmt.pixfmt == V4L2_PIX_FMT_NV16)
		chroma_fmt = 1;
	writel_relaxed((c_data->vp_in_fmt.width / 16) << 20 |
			chroma_fmt << 11 | 0x2 << 4, VCAP_VP_IN_CONFIG);

	chroma_fmt = 0;
	if (c_data->vp_out_fmt.pixfmt == V4L2_PIX_FMT_NV16)
		chroma_fmt = 1;

	writel_relaxed((c_data->vp_out_fmt.width / 16) << 20 |
			chroma_fmt << 11 | 0x1 << 4, VCAP_VP_OUT_CONFIG);

	/* Enable Interrupt */
#ifdef TOP_FIELD_FIX
	vp_act->top_field = 1;
#else
	if (vp_act->bufT2->vb.v4l2_buf.field == V4L2_FIELD_TOP)
		top_field = 1;
#endif
	vp_act->vp_state = VP_FRAME2;
	writel_relaxed(0x01100001, VCAP_VP_INTERRUPT_ENABLE);
#ifdef TOP_FIELD_FIX
	writel_iowmb(0x00000000 | vp_act->top_field << 0, VCAP_VP_CTRL);
	writel_iowmb(0x00010000 | vp_act->top_field << 0, VCAP_VP_CTRL);
#else
	writel_iowmb(0x00000000 | top_field, VCAP_VP_CTRL);
	writel_iowmb(0x00010000 | top_field, VCAP_VP_CTRL);
#endif
	atomic_set(&c_data->dev->vp_enabled, 1);
	enable_irq(dev->vpirq->start);
	return 0;
}

int continue_vp(struct vcap_client_data *c_data)
{
	struct vcap_dev *dev;
	struct vp_action *vp_act;
	int rc;
#ifndef TOP_FIELD_FIX
	bool top_field;
#endif

	dprintk(2, "Start Continue\n");
	dev = c_data->dev;

	if (dev->vp_client == NULL) {
		pr_err("No active vp client\n");
		return -ENODEV;
	}
	vp_act = &dev->vp_client->vp_action;

	if (vp_act->vp_state == VP_UNKNOWN) {
		pr_err("%s: VP is in an unknown state\n",
				__func__);
		return -EAGAIN;
	}

	rc = vp_setup_buffers(c_data);
	if (rc < 0)
		return rc;

#ifndef TOP_FIELD_FIX
	if (vp_act->bufT2->vb.v4l2_buf.field == V4L2_FIELD_TOP)
		top_field = 1;
#endif

	/* Config VP & Enable Interrupt */
	writel_relaxed(0x01100001, VCAP_VP_INTERRUPT_ENABLE);
#ifdef TOP_FIELD_FIX
	writel_iowmb(0x00000000 | vp_act->top_field << 0, VCAP_VP_CTRL);
	writel_iowmb(0x00010000 | vp_act->top_field << 0, VCAP_VP_CTRL);
#else
	writel_iowmb(0x00000000 | top_field, VCAP_VP_CTRL);
	writel_iowmb(0x00010000 | top_field, VCAP_VP_CTRL);
#endif

	atomic_set(&c_data->dev->vp_enabled, 1);
	enable_irq(dev->vpirq->start);
	return 0;
}
