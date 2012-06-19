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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <mach/camera.h>
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <mach/clk.h>
#include <linux/clk.h>

#include <media/v4l2-event.h>
#include <media/vcap_v4l2.h>
#include <media/vcap_fmt.h>
#include "vcap_vc.h"

static unsigned debug;

#define dprintk(level, fmt, arg...)					\
	do {								\
		if (debug >= level)					\
			printk(KERN_DEBUG "VC: " fmt, ## arg);		\
	} while (0)

void config_buffer(struct vcap_client_data *c_data,
			struct vcap_buffer *buf,
			void __iomem *y_addr,
			void __iomem *c_addr)
{
	if (c_data->vc_format.color_space == HAL_VCAP_RGB) {
		writel_relaxed(buf->paddr, y_addr);
	} else {
		int size = ((c_data->vc_format.hactive_end -
				c_data->vc_format.hactive_start) *
				(c_data->vc_format.vactive_end -
				c_data->vc_format.vactive_start));
		writel_relaxed(buf->paddr, y_addr);
		writel_relaxed(buf->paddr + size, c_addr);
	}
}

static void mov_buf_to_vp(struct work_struct *work)
{
	struct vp_work_t *vp_work = container_of(work, struct vp_work_t, work);
	struct v4l2_buffer p;
	struct vb2_buffer *vb_vc;
	struct vcap_buffer *buf_vc;
	struct vb2_buffer *vb_vp;
	struct vcap_buffer *buf_vp;

	int rc;
	p.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	p.memory = V4L2_MEMORY_USERPTR;
	while (1) {
		if (!vp_work->cd->streaming)
			return;
		rc = vb2_dqbuf(&vp_work->cd->vc_vidq, &p, O_NONBLOCK);
		if (rc < 0)
			return;

		vb_vc = vp_work->cd->vc_vidq.bufs[p.index];
		if (NULL == vb_vc) {
			dprintk(1, "%s: buffer is NULL\n", __func__);
			vb2_qbuf(&vp_work->cd->vc_vidq, &p);
			return;
		}
		buf_vc = container_of(vb_vc, struct vcap_buffer, vb);

		vb_vp = vp_work->cd->vp_in_vidq.bufs[p.index];
		if (NULL == vb_vp) {
			dprintk(1, "%s: buffer is NULL\n", __func__);
			vb2_qbuf(&vp_work->cd->vc_vidq, &p);
			return;
		}
		buf_vp = container_of(vb_vp, struct vcap_buffer, vb);
		buf_vp->ion_handle = buf_vc->ion_handle;
		buf_vp->paddr = buf_vc->paddr;
		buf_vc->ion_handle = NULL;
		buf_vc->paddr = 0;

		p.type = V4L2_BUF_TYPE_INTERLACED_IN_DECODER;

		/* This call should not fail */
		rc = vb2_qbuf(&vp_work->cd->vp_in_vidq, &p);
		if (rc < 0) {
			pr_err("%s: qbuf to vp_in failed\n", __func__);
			buf_vc->ion_handle = buf_vp->ion_handle;
			buf_vc->paddr = buf_vp->paddr;
			buf_vp->ion_handle = NULL;
			buf_vp->paddr = 0;
			p.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			vb2_qbuf(&vp_work->cd->vc_vidq, &p);
		}
	}
}

irqreturn_t vc_handler(struct vcap_dev *dev)
{
	uint32_t irq, timestamp;
	enum rdy_buf vc_buf_status, buf_ind;
	struct vcap_buffer *buf;
	struct vb2_buffer *vb = NULL;
	struct vcap_client_data *c_data;
	struct v4l2_event v4l2_evt;

	irq = readl_relaxed(VCAP_VC_INT_STATUS);

	dprintk(1, "%s: irq=0x%08x\n", __func__, irq);

	v4l2_evt.id = 0;
	if (irq & 0x8000200) {
		v4l2_evt.type = V4L2_EVENT_PRIVATE_START +
			VCAP_VC_PIX_ERR_EVENT;
		v4l2_event_queue(dev->vfd, &v4l2_evt);
	}
	if (irq & 0x40000200) {
		v4l2_evt.type = V4L2_EVENT_PRIVATE_START +
			VCAP_VC_LINE_ERR_EVENT;
		v4l2_event_queue(dev->vfd, &v4l2_evt);
	}
	if (irq & 0x20000200) {
		v4l2_evt.type = V4L2_EVENT_PRIVATE_START +
			VCAP_VC_VSYNC_ERR_EVENT;
		v4l2_event_queue(dev->vfd, &v4l2_evt);
	}
	if (irq & 0x00000800) {
		v4l2_evt.type = V4L2_EVENT_PRIVATE_START +
			VCAP_VC_NPL_OFLOW_ERR_EVENT;
		v4l2_event_queue(dev->vfd, &v4l2_evt);
	}
	if (irq & 0x00000400) {
		v4l2_evt.type = V4L2_EVENT_PRIVATE_START +
			VCAP_VC_LBUF_OFLOW_ERR_EVENT;
		v4l2_event_queue(dev->vfd, &v4l2_evt);
	}

	vc_buf_status = irq & VC_BUFFER_WRITTEN;
	dprintk(1, "Done buf status = %d\n", vc_buf_status);

	if (vc_buf_status == VC_NO_BUF) {
		writel_relaxed(irq, VCAP_VC_INT_CLEAR);
		pr_err("VC IRQ shows some error\n");
		return IRQ_HANDLED;
	}

	if (dev->vc_client == NULL) {
		writel_relaxed(irq, VCAP_VC_INT_CLEAR);
		pr_err("VC: There is no active vc client\n");
		return IRQ_HANDLED;
	}
	c_data = dev->vc_client;

	spin_lock(&dev->vc_client->cap_slock);
	if (list_empty(&dev->vc_client->vid_vc_action.active)) {
		/* Just leave we have no new queued buffers */
		spin_unlock(&dev->vc_client->cap_slock);
		writel_relaxed(irq, VCAP_VC_INT_CLEAR);
		v4l2_evt.type = V4L2_EVENT_PRIVATE_START +
			VCAP_VC_BUF_OVERWRITE_EVENT;
		v4l2_event_queue(dev->vfd, &v4l2_evt);
		dprintk(1, "We have no more avilable buffers\n");
		return IRQ_HANDLED;
	}
	spin_unlock(&dev->vc_client->cap_slock);

	timestamp = readl_relaxed(VCAP_VC_TIMESTAMP);

	buf_ind = dev->vc_client->vid_vc_action.buf_ind;

	if (vc_buf_status == VC_BUF1N2) {
		/* There are 2 buffer ready */
		writel_relaxed(irq, VCAP_VC_INT_CLEAR);
		return IRQ_HANDLED;
	} else if (buf_ind != vc_buf_status) {
		/* buffer is out of sync */
		writel_relaxed(irq, VCAP_VC_INT_CLEAR);
		return IRQ_HANDLED;
	}

	if (buf_ind == VC_BUF1) {
		dprintk(1, "Got BUF1\n");
		vb = &dev->vc_client->vid_vc_action.buf1->vb;
		spin_lock(&dev->vc_client->cap_slock);
		if (list_empty(&dev->vc_client->vid_vc_action.active)) {
			spin_unlock(&dev->vc_client->cap_slock);
			v4l2_evt.type = V4L2_EVENT_PRIVATE_START +
				VCAP_VC_BUF_OVERWRITE_EVENT;
			v4l2_event_queue(dev->vfd, &v4l2_evt);
			writel_relaxed(irq, VCAP_VC_INT_CLEAR);
			return IRQ_HANDLED;
		}
		buf = list_entry(dev->vc_client->vid_vc_action.active.next,
				struct vcap_buffer, list);
		list_del(&buf->list);
		spin_unlock(&dev->vc_client->cap_slock);
		/* Config vc with this new buffer */
		config_buffer(c_data, buf, VCAP_VC_Y_ADDR_1,
				VCAP_VC_C_ADDR_1);

		vb->v4l2_buf.timestamp.tv_usec = timestamp;
		vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
		dev->vc_client->vid_vc_action.buf1 = buf;
		dev->vc_client->vid_vc_action.buf_ind = VC_BUF2;
		irq = VC_BUF1;
	} else {
		dprintk(1, "Got BUF2\n");
		spin_lock(&dev->vc_client->cap_slock);
		vb = &dev->vc_client->vid_vc_action.buf2->vb;
		if (list_empty(&dev->vc_client->vid_vc_action.active)) {
			spin_unlock(&dev->vc_client->cap_slock);
			writel_relaxed(irq, VCAP_VC_INT_CLEAR);
			v4l2_evt.type = V4L2_EVENT_PRIVATE_START +
				VCAP_VC_BUF_OVERWRITE_EVENT;
			v4l2_event_queue(dev->vfd, &v4l2_evt);
			return IRQ_HANDLED;
		}
		buf = list_entry(dev->vc_client->vid_vc_action.active.next,
						 struct vcap_buffer, list);
		list_del(&buf->list);
		spin_unlock(&dev->vc_client->cap_slock);
		/* Config vc with this new buffer */
		config_buffer(c_data, buf, VCAP_VC_Y_ADDR_2,
				VCAP_VC_C_ADDR_2);

		vb->v4l2_buf.timestamp.tv_usec = timestamp;
		vb2_buffer_done(vb, VB2_BUF_STATE_DONE);

		dev->vc_client->vid_vc_action.buf2 = buf;
		dev->vc_client->vid_vc_action.buf_ind = VC_BUF1;
		irq = VC_BUF2;
	}

	if (c_data->op_mode == VC_AND_VP_VCAP_OP)
		queue_work(dev->vcap_wq, &dev->vc_to_vp_work.work);

	writel_relaxed(irq, VCAP_VC_INT_CLEAR);

	return IRQ_HANDLED;
}

int vc_start_capture(struct vcap_client_data *c_data)
{
	return 0;
}

int vc_hw_kick_off(struct vcap_client_data *c_data)
{
	struct vcap_action *vid_vc_action = &c_data->vid_vc_action;
	struct vcap_dev *dev;
	unsigned long flags = 0;
	int rc, counter = 0;
	struct vcap_buffer *buf;

	dev = c_data->dev;
	vid_vc_action->buf_ind = VC_BUF1;
	dprintk(2, "Start Kickoff\n");

	if (dev->vc_client == NULL) {
		pr_err("No active vc client\n");
		return -ENODEV;
	}
	spin_lock_irqsave(&dev->vc_client->cap_slock, flags);
	if (list_empty(&dev->vc_client->vid_vc_action.active)) {
		spin_unlock_irqrestore(&dev->vc_client->cap_slock, flags);
		pr_err("%s: VC We have no more avilable buffers\n",
				__func__);
		return -EINVAL;
	}

	list_for_each_entry(buf, &vid_vc_action->active, list)
		counter++;

	if (counter < 2) {
		/* not enough buffers have been queued */
		spin_unlock_irqrestore(&dev->vc_client->cap_slock, flags);
		return -EINVAL;
	}

	vid_vc_action->buf1 = list_entry(vid_vc_action->active.next,
			struct vcap_buffer, list);
	list_del(&vid_vc_action->buf1->list);

	vid_vc_action->buf2 = list_entry(vid_vc_action->active.next,
			struct vcap_buffer, list);
	list_del(&vid_vc_action->buf2->list);

	spin_unlock_irqrestore(&dev->vc_client->cap_slock, flags);

	config_buffer(c_data, vid_vc_action->buf1, VCAP_VC_Y_ADDR_1,
			VCAP_VC_C_ADDR_1);
	config_buffer(c_data, vid_vc_action->buf2, VCAP_VC_Y_ADDR_2,
			VCAP_VC_C_ADDR_2);

	rc = readl_relaxed(VCAP_VC_CTRL);
	writel_iowmb(rc | 0x1, VCAP_VC_CTRL);

	writel_relaxed(0x6, VCAP_VC_INT_MASK);

	enable_irq(dev->vcirq->start);
	return 0;
}

void vc_stop_capture(struct vcap_client_data *c_data)
{
	struct vcap_dev *dev = c_data->dev;
	int rc;

	rc = readl_relaxed(VCAP_VC_CTRL);
	writel_iowmb(rc & ~(0x1), VCAP_VC_CTRL);

	if (atomic_read(&dev->vc_enabled) == 1)
		disable_irq(dev->vcirq->start);

	flush_workqueue(dev->vcap_wq);
}

int config_vc_format(struct vcap_client_data *c_data)
{
	struct vcap_dev *dev;
	unsigned int rc;
	int timeout;
	struct v4l2_format_vc_ext *vc_format = &c_data->vc_format;
	dev = c_data->dev;

	/* restart VC */
	writel_relaxed(0x00000001, VCAP_SW_RESET_REQ);
	timeout = 10000;
	while (1) {
		rc = (readl_relaxed(VCAP_SW_RESET_STATUS) & 0x1);
		if (!rc)
			break;
		timeout--;
		if (timeout == 0) {
			pr_err("VC is not resetting properly\n");
			return -EINVAL;
		}
	}
	writel_relaxed(0x00000000, VCAP_SW_RESET_REQ);

	writel_iowmb(0x00000102, VCAP_VC_NPL_CTRL);
	rc = readl_relaxed(VCAP_VC_NPL_CTRL);
	rc = readl_relaxed(VCAP_VC_NPL_CTRL);
	writel_iowmb(0x00000002, VCAP_VC_NPL_CTRL);

	dprintk(2, "%s: Starting VC configuration\n", __func__);
	writel_iowmb(0x00000002, VCAP_VC_NPL_CTRL);
	writel_iowmb(0x00000004 | vc_format->color_space << 1 |
			vc_format->mode << 3 |
			vc_format->mode << 10, VCAP_VC_CTRL);

	writel_relaxed(vc_format->h_polar << 4 |
			vc_format->v_polar << 0, VCAP_VC_POLARITY);

	writel_relaxed(vc_format->h_polar << 4 |
			vc_format->v_polar << 0, VCAP_VC_POLARITY);
	writel_relaxed(((vc_format->htotal << 16) | vc_format->vtotal),
			VCAP_VC_V_H_TOTAL);
	writel_relaxed(((vc_format->hactive_end << 16) |
			vc_format->hactive_start), VCAP_VC_H_ACTIVE);

	writel_relaxed(((vc_format->vactive_end << 16) |
			vc_format->vactive_start), VCAP_VC_V_ACTIVE);
	writel_relaxed(((vc_format->f2_vactive_end << 16) |
			vc_format->f2_vactive_start), VCAP_VC_V_ACTIVE_F2);
	writel_relaxed(((vc_format->vsync_end << 16) | vc_format->vsync_start),
			VCAP_VC_VSYNC_VPOS);
	writel_relaxed(((vc_format->f2_vsync_v_end << 16) |
			vc_format->f2_vsync_v_start), VCAP_VC_VSYNC_F2_VPOS);
	writel_relaxed(((vc_format->hsync_end << 16) |
			vc_format->hsync_start), VCAP_VC_HSYNC_HPOS);
	writel_relaxed(((vc_format->f2_vsync_h_end << 16) |
			vc_format->f2_vsync_h_start), VCAP_VC_VSYNC_F2_HPOS);
	writel_iowmb(0x000033FF, VCAP_VC_BUF_CTRL);

	rc = vc_format->hactive_end - vc_format->hactive_start;
	if (vc_format->color_space)
		rc *= 3;

	writel_relaxed(rc, VCAP_VC_Y_STRIDE);
	writel_relaxed(rc, VCAP_VC_C_STRIDE);

	writel_relaxed(0x00010033 , VCAP_OFFSET(0x0898));
	writel_relaxed(0x00010fff , VCAP_OFFSET(0x089c));
	writel_relaxed(0x0a418820, VCAP_VC_IN_CTRL1);
	writel_relaxed(0x16a4a0e6, VCAP_VC_IN_CTRL2);
	writel_relaxed(0x2307b9ac, VCAP_VC_IN_CTRL3);
	writel_relaxed(0x2f6ad272, VCAP_VC_IN_CTRL4);
	writel_relaxed(0x00006b38, VCAP_VC_IN_CTRL5);

	writel_iowmb(0x00000001 , VCAP_OFFSET(0x0d00));
	dprintk(2, "%s: Done VC configuration\n", __func__);

	return 0;
}

int detect_vc(struct vcap_dev *dev)
{
	int result;
	result = readl_relaxed(VCAP_HARDWARE_VERSION_REG);
	dprintk(1, "Hardware version: %08x\n", result);
	if (result != VCAP_HARDWARE_VERSION)
		return -ENODEV;
	INIT_WORK(&dev->vc_to_vp_work.work, mov_buf_to_vp);
	return 0;
}

int deinit_vc(void)
{
	return 0;
}
