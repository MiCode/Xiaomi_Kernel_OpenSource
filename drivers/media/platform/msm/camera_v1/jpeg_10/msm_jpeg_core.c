/* Copyright (c) 2012,The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/sched.h>
#include "msm_jpeg_hw.h"
#include "msm_jpeg_core.h"
#include "msm_jpeg_platform.h"
#include "msm_jpeg_common.h"

static struct msm_jpeg_hw_pingpong fe_pingpong_buf;
static struct msm_jpeg_hw_pingpong we_pingpong_buf;
static int we_pingpong_index;
static int reset_done_ack;
static spinlock_t reset_lock;
static wait_queue_head_t reset_wait;

int msm_jpeg_core_reset(uint8_t op_mode, void *base, int size)
{
	unsigned long flags;
	int rc = 0;
	int tm = 500; /*500ms*/
	memset(&fe_pingpong_buf, 0, sizeof(fe_pingpong_buf));
	fe_pingpong_buf.is_fe = 1;
	we_pingpong_index = 0;
	memset(&we_pingpong_buf, 0, sizeof(we_pingpong_buf));
	spin_lock_irqsave(&reset_lock, flags);
	reset_done_ack = 0;
	msm_jpeg_hw_reset(base, size);
	spin_unlock_irqrestore(&reset_lock, flags);
	rc = wait_event_interruptible_timeout(
			reset_wait,
			reset_done_ack,
			msecs_to_jiffies(tm));

	if (!reset_done_ack) {
		JPEG_DBG("%s: reset ACK failed %d", __func__, rc);
		return -EBUSY;
	}

	JPEG_DBG("%s: reset_done_ack rc %d", __func__, rc);
	spin_lock_irqsave(&reset_lock, flags);
	reset_done_ack = 0;
	spin_unlock_irqrestore(&reset_lock, flags);

	return 0;
}

void msm_jpeg_core_release(int release_buf, int domain_num)
{
	int i = 0;
	for (i = 0; i < 2; i++) {
		if (we_pingpong_buf.buf_status[i] && release_buf)
			msm_jpeg_platform_p2v(we_pingpong_buf.buf[i].file,
				&we_pingpong_buf.buf[i].handle, domain_num);
		we_pingpong_buf.buf_status[i] = 0;
	}
}

void msm_jpeg_core_init(void)
{
	init_waitqueue_head(&reset_wait);
	spin_lock_init(&reset_lock);
}

int msm_jpeg_core_fe_start(void)
{
	msm_jpeg_hw_fe_start();
	return 0;
}

/* fetch engine */
int msm_jpeg_core_fe_buf_update(struct msm_jpeg_core_buf *buf)
{
	JPEG_DBG("%s:%d] 0x%08x %d 0x%08x %d\n", __func__, __LINE__,
		(int) buf->y_buffer_addr, buf->y_len,
		(int) buf->cbcr_buffer_addr, buf->cbcr_len);
	return msm_jpeg_hw_pingpong_update(&fe_pingpong_buf, buf);
}

void *msm_jpeg_core_fe_pingpong_irq(int jpeg_irq_status, void *context)
{
	return msm_jpeg_hw_pingpong_irq(&fe_pingpong_buf);
}

/* write engine */
int msm_jpeg_core_we_buf_update(struct msm_jpeg_core_buf *buf)
{
	JPEG_DBG("%s:%d] 0x%08x 0x%08x %d\n", __func__, __LINE__,
		(int) buf->y_buffer_addr, (int) buf->cbcr_buffer_addr,
		buf->y_len);
	we_pingpong_buf.buf[0] = *buf;
	we_pingpong_buf.buf_status[0] = 1;
	msm_jpeg_hw_we_buffer_update(
			&we_pingpong_buf.buf[0], 0);

	return 0;
}

int msm_jpeg_core_we_buf_reset(struct msm_jpeg_hw_buf *buf)
{
	int i = 0;
	for (i = 0; i < 2; i++) {
		if (we_pingpong_buf.buf[i].y_buffer_addr
					== buf->y_buffer_addr)
			we_pingpong_buf.buf_status[i] = 0;
	}
	return 0;
}

void *msm_jpeg_core_we_pingpong_irq(int jpeg_irq_status, void *context)
{
	JPEG_DBG("%s:%d]\n", __func__, __LINE__);

	return msm_jpeg_hw_pingpong_irq(&we_pingpong_buf);
}

void *msm_jpeg_core_framedone_irq(int jpeg_irq_status, void *context)
{
	struct msm_jpeg_hw_buf *buf_p;

	JPEG_DBG("%s:%d]\n", __func__, __LINE__);

	buf_p = msm_jpeg_hw_pingpong_active_buffer(&we_pingpong_buf);
	if (buf_p) {
		buf_p->framedone_len = msm_jpeg_hw_encode_output_size();
		JPEG_DBG("%s:%d] framedone_len %d\n", __func__, __LINE__,
			buf_p->framedone_len);
	}

	return buf_p;
}

void *msm_jpeg_core_reset_ack_irq(int jpeg_irq_status, void *context)
{
	/* @todo return the status back to msm_jpeg_core_reset */
	JPEG_DBG("%s:%d]\n", __func__, __LINE__);
	return NULL;
}

void *msm_jpeg_core_err_irq(int jpeg_irq_status, void *context)
{
	JPEG_PR_ERR("%s:%d]\n", __func__, jpeg_irq_status);
	return NULL;
}

static int (*msm_jpeg_irq_handler) (int, void *, void *);

irqreturn_t msm_jpeg_core_irq(int irq_num, void *context)
{
	void *data = NULL;
	unsigned long flags;
	int jpeg_irq_status;

	JPEG_DBG("%s:%d] irq_num = %d\n", __func__, __LINE__, irq_num);

	jpeg_irq_status = msm_jpeg_hw_irq_get_status();

	JPEG_DBG("%s:%d] jpeg_irq_status = %0x\n", __func__, __LINE__,
		jpeg_irq_status);

	/*For reset and framedone IRQs, clear all bits*/
	if (jpeg_irq_status & 0x10000000) {
		msm_jpeg_hw_irq_clear(JPEG_IRQ_CLEAR_BMSK,
			JPEG_IRQ_CLEAR_ALL);
	} else if (jpeg_irq_status & 0x1) {
		msm_jpeg_hw_irq_clear(JPEG_IRQ_CLEAR_BMSK,
			JPEG_IRQ_CLEAR_ALL);
	} else {
		msm_jpeg_hw_irq_clear(JPEG_IRQ_CLEAR_BMSK,
			jpeg_irq_status);
	}

	if (msm_jpeg_hw_irq_is_frame_done(jpeg_irq_status)) {
		data = msm_jpeg_core_framedone_irq(jpeg_irq_status,
			context);
		if (msm_jpeg_irq_handler)
			msm_jpeg_irq_handler(
				MSM_JPEG_HW_MASK_COMP_FRAMEDONE,
				context, data);
	}
	if (msm_jpeg_hw_irq_is_reset_ack(jpeg_irq_status)) {
		data = msm_jpeg_core_reset_ack_irq(jpeg_irq_status,
			context);
		spin_lock_irqsave(&reset_lock, flags);
		reset_done_ack = 1;
		spin_unlock_irqrestore(&reset_lock, flags);
		wake_up(&reset_wait);
		if (msm_jpeg_irq_handler)
			msm_jpeg_irq_handler(
				MSM_JPEG_HW_MASK_COMP_RESET_ACK,
				context, data);
	}

	/* Unexpected/unintended HW interrupt */
	if (msm_jpeg_hw_irq_is_err(jpeg_irq_status)) {
		data = msm_jpeg_core_err_irq(jpeg_irq_status, context);
		if (msm_jpeg_irq_handler)
			msm_jpeg_irq_handler(MSM_JPEG_HW_MASK_COMP_ERR,
				context, data);
	}

	return IRQ_HANDLED;
}

void msm_jpeg_core_irq_install(int (*irq_handler) (int, void *, void *))
{
	msm_jpeg_irq_handler = irq_handler;
}

void msm_jpeg_core_irq_remove(void)
{
	msm_jpeg_irq_handler = NULL;
}
