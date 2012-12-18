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

void config_buffer(struct vcap_client_data *c_data,
			struct vcap_buffer *buf,
			void __iomem *y_addr,
			void __iomem *c_addr)
{
	if (c_data->vc_format.color_space == HAL_VCAP_RGB) {
		writel_relaxed(buf->paddr, y_addr);
	} else {
		int size = (c_data->vc_format.hactive_end -
				c_data->vc_format.hactive_start);
		if (c_data->stride == VC_STRIDE_32)
			size = VCAP_STRIDE_CALC(size, VCAP_STRIDE_ALIGN_32);
		else
			size = VCAP_STRIDE_CALC(size, VCAP_STRIDE_ALIGN_16);
		size *= (c_data->vc_format.vactive_end -
				c_data->vc_format.vactive_start);
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
	p.memory = V4L2_MEMORY_USERPTR;
	while (1) {
		p.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (!vp_work->cd->streaming)
			return;
		rc = vcvp_dqbuf(&vp_work->cd->vc_vidq, &p);
		if (rc < 0)
			return;

		vb_vc = vp_work->cd->vc_vidq.bufs[p.index];
		if (NULL == vb_vc) {
			pr_debug("%s: buffer is NULL\n", __func__);
			vcvp_qbuf(&vp_work->cd->vc_vidq, &p);
			return;
		}
		buf_vc = container_of(vb_vc, struct vcap_buffer, vb);

		vb_vp = vp_work->cd->vp_in_vidq.bufs[p.index];
		if (NULL == vb_vp) {
			pr_debug("%s: buffer is NULL\n", __func__);
			vcvp_qbuf(&vp_work->cd->vc_vidq, &p);
			return;
		}
		buf_vp = container_of(vb_vp, struct vcap_buffer, vb);
		buf_vp->ion_handle = buf_vc->ion_handle;
		buf_vp->paddr = buf_vc->paddr;
		buf_vc->ion_handle = NULL;
		buf_vc->paddr = 0;

		p.type = V4L2_BUF_TYPE_INTERLACED_IN_DECODER;

		/* This call should not fail */
		rc = vcvp_qbuf(&vp_work->cd->vp_in_vidq, &p);
		if (rc < 0) {
			pr_err("%s: qbuf to vp_in failed\n", __func__);
			buf_vc->ion_handle = buf_vp->ion_handle;
			buf_vc->paddr = buf_vp->paddr;
			buf_vp->ion_handle = NULL;
			buf_vp->paddr = 0;
			p.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			vcvp_qbuf(&vp_work->cd->vc_vidq, &p);
		}
	}
}

static uint8_t correct_buf_num(uint32_t reg)
{
	int i;
	bool block_found = false;
	for (i = 0; i < VCAP_VC_MAX_BUF; i++) {
		if (reg & (0x2 << i)) {
			block_found = true;
			continue;
		}
		if (block_found)
			return i;
	}
	return 0;
}

static struct timeval interpolate_ts(struct timeval tv, uint32_t delta)
{
	if (tv.tv_usec < delta) {
		tv.tv_sec--;
		tv.tv_usec += VCAP_USEC - delta;
	} else {
		tv.tv_usec -= delta;
	}
	return tv;
}

inline void vc_isr_error_checking(struct vcap_dev *dev,
		struct v4l2_event v4l2_evt, uint32_t irq)
{
	if (irq & 0x200) {
		if (irq & 0x80000000) {
			writel_iowmb(0x00000102, VCAP_VC_NPL_CTRL);
			v4l2_evt.type = V4L2_EVENT_PRIVATE_START +
				VCAP_VC_PIX_ERR_EVENT;
			v4l2_event_queue(dev->vfd, &v4l2_evt);
		}
		if (irq & 0x40000000) {
			writel_iowmb(0x00000102, VCAP_VC_NPL_CTRL);
			v4l2_evt.type = V4L2_EVENT_PRIVATE_START +
				VCAP_VC_LINE_ERR_EVENT;
			v4l2_event_queue(dev->vfd, &v4l2_evt);
		}
		if (irq & 0x20000000) {
			writel_iowmb(0x00000102, VCAP_VC_NPL_CTRL);
			v4l2_evt.type = V4L2_EVENT_PRIVATE_START +
				VCAP_VC_VSYNC_ERR_EVENT;
			v4l2_event_queue(dev->vfd, &v4l2_evt);
		}
	}
	if (irq & 0x00001000) {
		writel_iowmb(0x00000102, VCAP_VC_NPL_CTRL);
		v4l2_evt.type = V4L2_EVENT_PRIVATE_START +
			VCAP_VC_VSYNC_SEQ_ERR;
		v4l2_event_queue(dev->vfd, &v4l2_evt);
	}
	if (irq & 0x00000800) {
		writel_iowmb(0x00000102, VCAP_VC_NPL_CTRL);
		v4l2_evt.type = V4L2_EVENT_PRIVATE_START +
			VCAP_VC_NPL_OFLOW_ERR_EVENT;
		v4l2_event_queue(dev->vfd, &v4l2_evt);
	}
	if (irq & 0x00000400) {
		writel_iowmb(0x00000102, VCAP_VC_NPL_CTRL);
		v4l2_evt.type = V4L2_EVENT_PRIVATE_START +
			VCAP_VC_LBUF_OFLOW_ERR_EVENT;
		v4l2_event_queue(dev->vfd, &v4l2_evt);
	}
}

inline uint8_t vc_isr_buffer_done_count(struct vcap_dev *dev,
		struct vcap_client_data *c_data, uint32_t irq)
{
	int i;
	uint8_t done_count = 0;
	for (i = 0; i < VCAP_VC_MAX_BUF; i++) {
		if (0x2 & (irq >> i))
			done_count++;
	}
	return done_count;
}

inline bool vc_isr_verify_expect_buf_rdy(struct vcap_dev *dev,
		struct vcap_client_data *c_data, struct v4l2_event v4l2_evt,
		uint32_t irq, uint8_t done_count, uint8_t tot, uint8_t buf_num)
{
	int i;
	/* Double check expected buffers are done */
	for (i = 0; i < done_count; i++) {
		if (!(irq & (0x1 << (((buf_num + i) % tot) + 1)))) {
			v4l2_evt.type = V4L2_EVENT_PRIVATE_START +
				VCAP_VC_UNEXPECT_BUF_DONE;
			v4l2_event_queue(dev->vfd, &v4l2_evt);
			pr_debug("Unexpected buffer done\n");
			c_data->vc_action.buf_num =
				correct_buf_num(irq) % tot;
			return true;
		}
	}
	return false;
}

inline void vc_isr_update_timestamp(struct vcap_dev *dev,
		struct vcap_client_data *c_data)
{
	uint32_t timestamp;

	timestamp = readl_relaxed(VCAP_VC_TIMESTAMP);
	if (timestamp < c_data->vc_action.last_ts) {
		c_data->vc_action.vc_ts.tv_usec +=
			(0xFFFFFFFF - c_data->vc_action.last_ts) +
			timestamp + 1;
	} else {
		c_data->vc_action.vc_ts.tv_usec +=
			timestamp - c_data->vc_action.last_ts;
	}

	c_data->vc_action.vc_ts.tv_sec +=
		c_data->vc_action.vc_ts.tv_usec / VCAP_USEC;
	c_data->vc_action.vc_ts.tv_usec =
		c_data->vc_action.vc_ts.tv_usec % VCAP_USEC;
	c_data->vc_action.last_ts = timestamp;
}

inline void vc_isr_no_new_buffer(struct vcap_dev *dev,
		struct vcap_client_data *c_data, struct v4l2_event v4l2_evt)
{
	v4l2_evt.type = V4L2_EVENT_PRIVATE_START +
		VCAP_VC_BUF_OVERWRITE_EVENT;
	v4l2_event_queue(dev->vfd, &v4l2_evt);

	c_data->vc_action.field_dropped =
		!c_data->vc_action.field_dropped;

	c_data->vc_action.field1 =
		!c_data->vc_action.field1;
	atomic_inc(&dev->dbg_p.vc_drop_count);
}

inline void vc_isr_switch_buffers(struct vcap_dev *dev,
		struct vcap_client_data *c_data, struct vcap_buffer *buf,
		struct vb2_buffer *vb, uint8_t idx, int done_count, int i)
{
	/* Config vc with this new buffer */
	config_buffer(c_data, buf, VCAP_VC_Y_ADDR_1 + 0x8 * idx,
			VCAP_VC_C_ADDR_1 + 0x8 * idx);
	vb->v4l2_buf.timestamp = interpolate_ts(
		c_data->vc_action.vc_ts,
		1000000 / c_data->vc_format.frame_rate *
		(done_count - 1 - i));
	if (c_data->vc_format.mode == HAL_VCAP_MODE_INT) {
		if (c_data->vc_action.field1)
			vb->v4l2_buf.field = V4L2_FIELD_TOP;
		else
			vb->v4l2_buf.field = V4L2_FIELD_BOTTOM;

		c_data->vc_action.field1 =
			!c_data->vc_action.field1;
	}
	vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
	c_data->vc_action.buf[idx] = buf;
}

inline bool vc_isr_change_buffers(struct vcap_dev *dev,
		struct vcap_client_data *c_data, struct v4l2_event v4l2_evt,
		int done_count, uint8_t tot, uint8_t buf_num)
{
	struct vb2_buffer *vb = NULL;
	struct vcap_buffer *buf;
	bool schedule_work = false;
	uint8_t idx;
	int i;

	for (i = 0; i < done_count; i++) {
		idx = (buf_num + i) % tot;
		vb = &c_data->vc_action.buf[idx]->vb;
		spin_lock(&c_data->cap_slock);
		if (list_empty(&c_data->vc_action.active)) {
			spin_unlock(&c_data->cap_slock);
			vc_isr_no_new_buffer(dev, c_data, v4l2_evt);
			continue;
		}
		if (c_data->vc_format.mode == HAL_VCAP_MODE_INT &&
				c_data->vc_action.field_dropped) {
			spin_unlock(&c_data->cap_slock);
			vc_isr_no_new_buffer(dev, c_data, v4l2_evt);
			continue;
		}
		buf = list_entry(c_data->vc_action.active.next,
				struct vcap_buffer, list);
		list_del(&buf->list);
		spin_unlock(&c_data->cap_slock);
		vc_isr_switch_buffers(dev, c_data, buf, vb, idx, done_count, i);
		schedule_work = true;
	}
	return schedule_work;
}

irqreturn_t vc_handler(struct vcap_dev *dev)
{
	uint32_t irq;
	struct vcap_client_data *c_data;
	struct v4l2_event v4l2_evt;
	uint8_t done_count = 0, buf_num, tot;
	bool schedule_work = false;

	v4l2_evt.id = 0;
	irq = readl_relaxed(VCAP_VC_INT_STATUS);
	writel_relaxed(irq, VCAP_VC_INT_CLEAR);

	pr_debug("%s: irq=0x%08x\n", __func__, irq);

	if (dev->vc_client == NULL) {
		/* This should never happen */
		pr_err("VC: There is no active vc client\n");
		return IRQ_HANDLED;
	}

	c_data = dev->vc_client;
	if (!c_data->streaming) {
		pr_err("VC no longer streaming\n");
		return IRQ_HANDLED;
	}

	if (irq == VC_VSYNC_MASK) {
		if (c_data->vc_format.mode == HAL_VCAP_MODE_INT)
			c_data->vc_action.field1 = irq & 0x1;
		return IRQ_HANDLED;
	}

	if (irq & VC_ERR_MASK) {
		vc_isr_error_checking(dev, v4l2_evt, irq);
		return IRQ_HANDLED;
	}

	if (!(irq & VC_BUFFER_MASK)) {
		pr_debug("No frames done\n");
		return IRQ_HANDLED;
	}

	done_count = vc_isr_buffer_done_count(dev, c_data, irq);
	buf_num = c_data->vc_action.buf_num;
	tot = c_data->vc_action.tot_buf;

	if (vc_isr_verify_expect_buf_rdy(dev, c_data,
			v4l2_evt, irq, done_count, tot, buf_num))
		return IRQ_HANDLED;

	vc_isr_update_timestamp(dev, c_data);

	c_data->vc_action.buf_num = (buf_num + done_count) % tot;

	schedule_work = vc_isr_change_buffers(dev, c_data, v4l2_evt,
		done_count, tot, buf_num);

	if (schedule_work && c_data->op_mode == VC_AND_VP_VCAP_OP)
		queue_work(dev->vcap_wq, &dev->vc_to_vp_work.work);

	return IRQ_HANDLED;
}

int vc_start_capture(struct vcap_client_data *c_data)
{
	return 0;
}

int vc_hw_kick_off(struct vcap_client_data *c_data)
{
	struct vc_action *vc_action = &c_data->vc_action;
	struct vcap_dev *dev;
	struct timeval tv;
	unsigned long flags = 0;
	int rc, i, counter = 0;
	struct vcap_buffer *buf;

	dev = c_data->dev;
	pr_debug("Start Kickoff\n");

	if (dev->vc_client == NULL) {
		pr_err("No active vc client\n");
		return -ENODEV;
	}
	c_data->vc_action.buf_num = 0;
	spin_lock_irqsave(&dev->vc_client->cap_slock, flags);
	if (list_empty(&dev->vc_client->vc_action.active)) {
		spin_unlock_irqrestore(&dev->vc_client->cap_slock, flags);
		pr_err("%s: VC We have no more avilable buffers\n",
				__func__);
		return -EINVAL;
	}

	list_for_each_entry(buf, &vc_action->active, list)
		counter++;

	if (counter < c_data->vc_action.tot_buf) {
		/* not enough buffers have been queued */
		spin_unlock_irqrestore(&dev->vc_client->cap_slock, flags);
		return -EINVAL;
	}

	for (i = 0; i < c_data->vc_action.tot_buf; i++) {
		vc_action->buf[i] = list_entry(vc_action->active.next,
			struct vcap_buffer, list);
		list_del(&vc_action->buf[i]->list);
	}
	spin_unlock_irqrestore(&dev->vc_client->cap_slock, flags);

	for (i = 0; i < c_data->vc_action.tot_buf; i++) {
		config_buffer(c_data, vc_action->buf[i],
			VCAP_VC_Y_ADDR_1 + i * 8,
			VCAP_VC_C_ADDR_1 + i * 8);
	}

	c_data->vc_action.last_ts = readl_relaxed(VCAP_VC_TIMESTAMP);
	c_data->vc_action.vc_ts.tv_sec =
		c_data->vc_action.last_ts / VCAP_USEC;
	c_data->vc_action.vc_ts.tv_usec =
		c_data->vc_action.last_ts % VCAP_USEC;

	atomic_set(&dev->dbg_p.vc_drop_count, 0);
	do_gettimeofday(&tv);
	dev->dbg_p.vc_timestamp = (uint32_t) (tv.tv_sec * VCAP_USEC +
		tv.tv_usec);

	rc = 0;
	for (i = 0; i < c_data->vc_action.tot_buf; i++)
		rc = rc << 1 | 0x2;
	rc |= VC_ERR_MASK;
	rc |= VC_VSYNC_MASK;
	writel_relaxed(rc, VCAP_VC_INT_MASK);

	enable_irq(dev->vcirq->start);
	rc = readl_relaxed(VCAP_VC_CTRL);
	writel_iowmb(rc | 0x1, VCAP_VC_CTRL);

	return 0;
}

void vc_stop_capture(struct vcap_client_data *c_data)
{
	struct vcap_dev *dev = c_data->dev;
	unsigned int reg;
	int timeout;

	writel_iowmb(0x00000102, VCAP_VC_NPL_CTRL);
	writel_iowmb(0x0, VCAP_VC_INT_MASK);
	flush_workqueue(dev->vcap_wq);
	if (atomic_read(&dev->vc_enabled) == 1)
		disable_irq_nosync(dev->vcirq->start);

	writel_iowmb(0x00000000, VCAP_VC_CTRL);
	writel_iowmb(0x00000001, VCAP_SW_RESET_REQ);
	timeout = 10000;
	while (1) {
		reg = (readl_relaxed(VCAP_SW_RESET_STATUS) & 0x1);
		if (!reg)
			break;
		timeout--;
		if (timeout == 0) {
			/* This should not happen */
			pr_err("VC is not resetting properly\n");
			writel_iowmb(0x00000000, VCAP_SW_RESET_REQ);
			break;
		}
	}

	reg = readl_relaxed(VCAP_VC_NPL_CTRL);
	reg = readl_relaxed(VCAP_VC_NPL_CTRL);
	writel_iowmb(0x00000002, VCAP_VC_NPL_CTRL);
}

int config_vc_format(struct vcap_client_data *c_data)
{
	struct vcap_dev *dev;
	unsigned int rc;
	int timeout;
	struct v4l2_format_vc_ext *vc_format = &c_data->vc_format;
	dev = c_data->dev;

	/* restart VC */
	writel_iowmb(0x00000102, VCAP_VC_NPL_CTRL);
	writel_iowmb(0x00000001, VCAP_SW_RESET_REQ);
	timeout = 10000;
	while (1) {
		if (!(readl_relaxed(VCAP_SW_RESET_STATUS) & 0x1))
			break;
		timeout--;
		if (timeout == 0) {
			pr_err("VC is not resetting properly\n");
			writel_iowmb(0x00000002, VCAP_VC_NPL_CTRL);
			return -EINVAL;
		}
	}

	rc = readl_relaxed(VCAP_VC_NPL_CTRL);
	rc = readl_relaxed(VCAP_VC_NPL_CTRL);
	writel_iowmb(0x00000002, VCAP_VC_NPL_CTRL);

	pr_debug("%s: Starting VC configuration\n", __func__);
	writel_iowmb(0x00000002, VCAP_VC_NPL_CTRL);
	writel_iowmb(0x00000004 | vc_format->color_space << 1 |
			vc_format->mode << 3 |
			(c_data->vc_action.tot_buf - 2) << 4 |
			vc_format->mode << 10,
			VCAP_VC_CTRL);

	writel_relaxed(vc_format->d_polar << 8 |
			vc_format->h_polar << 4 |
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
	if (c_data->stride == VC_STRIDE_32)
		rc = VCAP_STRIDE_CALC(rc, VCAP_STRIDE_ALIGN_32);
	else
		rc = VCAP_STRIDE_CALC(rc, VCAP_STRIDE_ALIGN_16);
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
	pr_debug("%s: Done VC configuration\n", __func__);

	return 0;
}

int detect_vc(struct vcap_dev *dev)
{
	int result;
	result = readl_relaxed(VCAP_HARDWARE_VERSION_REG);
	pr_debug("Hardware version: %08x\n", result);
	if (result != VCAP_HARDWARE_VERSION)
		return -ENODEV;
	INIT_WORK(&dev->vc_to_vp_work.work, mov_buf_to_vp);
	return 0;
}

int deinit_vc(void)
{
	return 0;
}
