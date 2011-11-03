/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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
#include "msm_gemini_hw.h"
#include "msm_gemini_core.h"
#include "msm_gemini_platform.h"
#include "msm_gemini_common.h"

static struct msm_gemini_hw_pingpong fe_pingpong_buf;
static struct msm_gemini_hw_pingpong we_pingpong_buf;
static int we_pingpong_index;
static int reset_done_ack;
static spinlock_t reset_lock;
static wait_queue_head_t reset_wait;

int msm_gemini_core_reset(uint8_t op_mode, void *base, int size)
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
	msm_gemini_hw_reset(base, size);
	spin_unlock_irqrestore(&reset_lock, flags);
	rc = wait_event_interruptible_timeout(
			reset_wait,
			reset_done_ack,
			msecs_to_jiffies(tm));

	if (!reset_done_ack) {
		GMN_DBG("%s: reset ACK failed %d", __func__, rc);
		return -EBUSY;
	}

	GMN_DBG("%s: reset_done_ack rc %d", __func__, rc);
	spin_lock_irqsave(&reset_lock, flags);
	reset_done_ack = 0;
	spin_unlock_irqrestore(&reset_lock, flags);

	if (op_mode == MSM_GEMINI_MODE_REALTIME_ENCODE) {
		/* Nothing needed for fe buffer cfg, config we only */
		msm_gemini_hw_we_buffer_cfg(1);
	} else {
		/* Nothing needed for fe buffer cfg, config we only */
		msm_gemini_hw_we_buffer_cfg(0);
	}

	/* @todo wait for reset done irq */

	return 0;
}

void msm_gemini_core_release(int release_buf)
{
	int i = 0;
	for (i = 0; i < 2; i++) {
		if (we_pingpong_buf.buf_status[i] && release_buf)
			msm_gemini_platform_p2v(we_pingpong_buf.buf[i].file,
					&we_pingpong_buf.buf[i].msm_buffer,
					&we_pingpong_buf.buf[i].handle);
		we_pingpong_buf.buf_status[i] = 0;
	}
}

void msm_gemini_core_init(void)
{
	init_waitqueue_head(&reset_wait);
	spin_lock_init(&reset_lock);
}

int msm_gemini_core_fe_start(void)
{
	msm_gemini_hw_fe_start();
	return 0;
}

/* fetch engine */
int msm_gemini_core_fe_buf_update(struct msm_gemini_core_buf *buf)
{
	GMN_DBG("%s:%d] 0x%08x %d 0x%08x %d\n", __func__, __LINE__,
		(int) buf->y_buffer_addr, buf->y_len,
		(int) buf->cbcr_buffer_addr, buf->cbcr_len);
	return msm_gemini_hw_pingpong_update(&fe_pingpong_buf, buf);
}

void *msm_gemini_core_fe_pingpong_irq(int gemini_irq_status, void *context)
{
	return msm_gemini_hw_pingpong_irq(&fe_pingpong_buf);
}

/* write engine */
int msm_gemini_core_we_buf_update(struct msm_gemini_core_buf *buf)
{
	int rc;
	GMN_DBG("%s:%d] 0x%08x 0x%08x %d\n", __func__, __LINE__,
		(int) buf->y_buffer_addr, (int) buf->cbcr_buffer_addr,
		buf->y_len);
	we_pingpong_buf.buf_status[we_pingpong_index] = 0;
	we_pingpong_index = (we_pingpong_index + 1)%2;
	rc = msm_gemini_hw_pingpong_update(&we_pingpong_buf, buf);
	return 0;
}

int msm_gemini_core_we_buf_reset(struct msm_gemini_hw_buf *buf)
{
	int i = 0;
	for (i = 0; i < 2; i++) {
		if (we_pingpong_buf.buf[i].y_buffer_addr
					== buf->y_buffer_addr)
			we_pingpong_buf.buf_status[i] = 0;
	}
	return 0;
}

void *msm_gemini_core_we_pingpong_irq(int gemini_irq_status, void *context)
{
	GMN_DBG("%s:%d]\n", __func__, __LINE__);

	return msm_gemini_hw_pingpong_irq(&we_pingpong_buf);
}

void *msm_gemini_core_framedone_irq(int gemini_irq_status, void *context)
{
	struct msm_gemini_hw_buf *buf_p;

	GMN_DBG("%s:%d]\n", __func__, __LINE__);

	buf_p = msm_gemini_hw_pingpong_active_buffer(&we_pingpong_buf);
	if (buf_p) {
		buf_p->framedone_len = msm_gemini_hw_encode_output_size();
		GMN_DBG("%s:%d] framedone_len %d\n", __func__, __LINE__,
			buf_p->framedone_len);
	}

	return buf_p;
}

void *msm_gemini_core_reset_ack_irq(int gemini_irq_status, void *context)
{
	/* @todo return the status back to msm_gemini_core_reset */
	GMN_DBG("%s:%d]\n", __func__, __LINE__);
	return NULL;
}

void *msm_gemini_core_err_irq(int gemini_irq_status, void *context)
{
	GMN_PR_ERR("%s:%d]\n", __func__, gemini_irq_status);
	return NULL;
}

static int (*msm_gemini_irq_handler) (int, void *, void *);

irqreturn_t msm_gemini_core_irq(int irq_num, void *context)
{
	void *data = NULL;
	unsigned long flags;
	int gemini_irq_status;

	GMN_DBG("%s:%d] irq_num = %d\n", __func__, __LINE__, irq_num);

	spin_lock_irqsave(&reset_lock, flags);
	reset_done_ack = 1;
	spin_unlock_irqrestore(&reset_lock, flags);
	gemini_irq_status = msm_gemini_hw_irq_get_status();

	GMN_DBG("%s:%d] gemini_irq_status = %0x\n", __func__, __LINE__,
		gemini_irq_status);

	/*For reset and framedone IRQs, clear all bits*/
	if (gemini_irq_status & 0x400) {
		wake_up(&reset_wait);
		msm_gemini_hw_irq_clear(HWIO_JPEG_IRQ_CLEAR_RMSK,
			JPEG_IRQ_CLEAR_ALL);
	} else if (gemini_irq_status & 0x1) {
		msm_gemini_hw_irq_clear(HWIO_JPEG_IRQ_CLEAR_RMSK,
			JPEG_IRQ_CLEAR_ALL);
	} else {
		msm_gemini_hw_irq_clear(HWIO_JPEG_IRQ_CLEAR_RMSK,
			gemini_irq_status);
	}

	if (msm_gemini_hw_irq_is_frame_done(gemini_irq_status)) {
		data = msm_gemini_core_framedone_irq(gemini_irq_status,
			context);
		if (msm_gemini_irq_handler)
			msm_gemini_irq_handler(
				MSM_GEMINI_HW_MASK_COMP_FRAMEDONE,
				context, data);
	}

	if (msm_gemini_hw_irq_is_fe_pingpong(gemini_irq_status)) {
		data = msm_gemini_core_fe_pingpong_irq(gemini_irq_status,
			context);
		if (msm_gemini_irq_handler)
			msm_gemini_irq_handler(MSM_GEMINI_HW_MASK_COMP_FE,
				context, data);
	}

	if (msm_gemini_hw_irq_is_we_pingpong(gemini_irq_status) &&
	    !msm_gemini_hw_irq_is_frame_done(gemini_irq_status)) {
		data = msm_gemini_core_we_pingpong_irq(gemini_irq_status,
			context);
		if (msm_gemini_irq_handler)
			msm_gemini_irq_handler(MSM_GEMINI_HW_MASK_COMP_WE,
				context, data);
	}

	if (msm_gemini_hw_irq_is_reset_ack(gemini_irq_status)) {
		data = msm_gemini_core_reset_ack_irq(gemini_irq_status,
			context);
		if (msm_gemini_irq_handler)
			msm_gemini_irq_handler(
				MSM_GEMINI_HW_MASK_COMP_RESET_ACK,
				context, data);
	}

	/* Unexpected/unintended HW interrupt */
	if (msm_gemini_hw_irq_is_err(gemini_irq_status)) {
		data = msm_gemini_core_err_irq(gemini_irq_status, context);
		if (msm_gemini_irq_handler)
			msm_gemini_irq_handler(MSM_GEMINI_HW_MASK_COMP_ERR,
				context, data);
	}

	return IRQ_HANDLED;
}

void msm_gemini_core_irq_install(int (*irq_handler) (int, void *, void *))
{
	msm_gemini_irq_handler = irq_handler;
}

void msm_gemini_core_irq_remove(void)
{
	msm_gemini_irq_handler = NULL;
}
