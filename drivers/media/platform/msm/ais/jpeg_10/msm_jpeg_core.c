/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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

int msm_jpeg_core_reset(struct msm_jpeg_device *pgmn_dev, uint8_t op_mode,
	void *base, int size) {
	unsigned long flags;
	int rc = 0;
	int tm = 500; /*500ms*/

	JPEG_DBG("%s:%d] reset", __func__, __LINE__);
	memset(&pgmn_dev->fe_pingpong_buf, 0,
		sizeof(pgmn_dev->fe_pingpong_buf));
	pgmn_dev->fe_pingpong_buf.is_fe = 1;
	memset(&pgmn_dev->we_pingpong_buf, 0,
		sizeof(pgmn_dev->we_pingpong_buf));
	spin_lock_irqsave(&pgmn_dev->reset_lock, flags);
	pgmn_dev->reset_done_ack = 0;
	if (pgmn_dev->core_type == MSM_JPEG_CORE_CODEC)
		msm_jpeg_hw_reset(base, size);
	else
		msm_jpeg_hw_reset_dma(base, size);

	spin_unlock_irqrestore(&pgmn_dev->reset_lock, flags);
	rc = wait_event_timeout(
			pgmn_dev->reset_wait,
			pgmn_dev->reset_done_ack,
			msecs_to_jiffies(tm));

	if (!pgmn_dev->reset_done_ack) {
		JPEG_DBG("%s: reset ACK failed %d", __func__, rc);
		return -EBUSY;
	}

	JPEG_DBG("%s: reset_done_ack rc %d", __func__, rc);
	spin_lock_irqsave(&pgmn_dev->reset_lock, flags);
	pgmn_dev->reset_done_ack = 0;
	pgmn_dev->state = MSM_JPEG_RESET;
	spin_unlock_irqrestore(&pgmn_dev->reset_lock, flags);

	return 0;
}

void msm_jpeg_core_release(struct msm_jpeg_device *pgmn_dev)
{
	int i = 0;

	for (i = 0; i < 2; i++) {
		if (pgmn_dev->we_pingpong_buf.buf_status[i] &&
			pgmn_dev->release_buf)
			msm_jpeg_platform_p2v(pgmn_dev->iommu_hdl,
				pgmn_dev->we_pingpong_buf.buf[i].ion_fd);
		pgmn_dev->we_pingpong_buf.buf_status[i] = 0;
	}
}

void msm_jpeg_core_init(struct msm_jpeg_device *pgmn_dev)
{
	init_waitqueue_head(&pgmn_dev->reset_wait);
	spin_lock_init(&pgmn_dev->reset_lock);
}

int msm_jpeg_core_fe_start(struct msm_jpeg_device *pgmn_dev)
{
	msm_jpeg_hw_fe_start(pgmn_dev->base);
	return 0;
}

/* fetch engine */
int msm_jpeg_core_fe_buf_update(struct msm_jpeg_device *pgmn_dev,
	struct msm_jpeg_core_buf *buf)
{
	int rc = 0;

	if (buf->cbcr_len == 0)
		buf->cbcr_buffer_addr = 0x0;

	JPEG_DBG("%s:%d] 0x%08x %d 0x%08x %d\n", __func__, __LINE__,
		(int) buf->y_buffer_addr, buf->y_len,
		(int) buf->cbcr_buffer_addr, buf->cbcr_len);

	if (pgmn_dev->core_type == MSM_JPEG_CORE_CODEC) {
		rc = msm_jpeg_hw_pingpong_update(&pgmn_dev->fe_pingpong_buf,
			buf, pgmn_dev->base);
		if (rc < 0)
			return rc;
		msm_jpeg_hw_fe_mmu_prefetch(buf, pgmn_dev->base,
			pgmn_dev->decode_flag);
	} else {
		rc = msm_jpegdma_hw_pingpong_update(
			&pgmn_dev->fe_pingpong_buf, buf, pgmn_dev->base);
		if (rc < 0)
			return rc;
		msm_jpegdma_hw_fe_mmu_prefetch(buf, pgmn_dev->base);
	}

	return rc;
}

void *msm_jpeg_core_fe_pingpong_irq(int jpeg_irq_status,
	struct msm_jpeg_device *pgmn_dev)
{
	return msm_jpeg_hw_pingpong_irq(&pgmn_dev->fe_pingpong_buf);
}

/* write engine */
int msm_jpeg_core_we_buf_update(struct msm_jpeg_device *pgmn_dev,
	struct msm_jpeg_core_buf *buf) {

	JPEG_DBG("%s:%d] 0x%08x 0x%08x %d\n", __func__, __LINE__,
		(int) buf->y_buffer_addr, (int) buf->cbcr_buffer_addr,
		buf->y_len);

	pgmn_dev->we_pingpong_buf.buf[0] = *buf;
	pgmn_dev->we_pingpong_buf.buf_status[0] = 1;

	if (pgmn_dev->core_type == MSM_JPEG_CORE_CODEC) {
		msm_jpeg_hw_we_buffer_update(
			&pgmn_dev->we_pingpong_buf.buf[0], 0, pgmn_dev->base);
		msm_jpeg_hw_we_mmu_prefetch(buf, pgmn_dev->base,
			pgmn_dev->decode_flag);
	} else {
		msm_jpegdma_hw_we_buffer_update(
			&pgmn_dev->we_pingpong_buf.buf[0], 0, pgmn_dev->base);
		msm_jpegdma_hw_we_mmu_prefetch(buf, pgmn_dev->base);
	}

	return 0;
}

int msm_jpeg_core_we_buf_reset(struct msm_jpeg_device *pgmn_dev,
	struct msm_jpeg_hw_buf *buf)
{
	int i = 0;

	for (i = 0; i < 2; i++) {
		if (pgmn_dev->we_pingpong_buf.buf[i].y_buffer_addr
			== buf->y_buffer_addr)
			pgmn_dev->we_pingpong_buf.buf_status[i] = 0;
	}
	return 0;
}

void *msm_jpeg_core_we_pingpong_irq(int jpeg_irq_status,
	struct msm_jpeg_device *pgmn_dev)
{
	JPEG_DBG("%s:%d]\n", __func__, __LINE__);

	return msm_jpeg_hw_pingpong_irq(&pgmn_dev->we_pingpong_buf);
}

void *msm_jpeg_core_framedone_irq(int jpeg_irq_status,
	struct msm_jpeg_device *pgmn_dev)
{
	struct msm_jpeg_hw_buf *buf_p;

	JPEG_DBG("%s:%d]\n", __func__, __LINE__);

	buf_p = msm_jpeg_hw_pingpong_active_buffer(
		&pgmn_dev->we_pingpong_buf);
	if (buf_p && !pgmn_dev->decode_flag) {
		buf_p->framedone_len =
			msm_jpeg_hw_encode_output_size(pgmn_dev->base);
		JPEG_DBG("%s:%d] framedone_len %d\n", __func__, __LINE__,
			buf_p->framedone_len);
	}

	return buf_p;
}

void *msm_jpeg_core_reset_ack_irq(int jpeg_irq_status,
	struct msm_jpeg_device *pgmn_dev)
{
	/* @todo return the status back to msm_jpeg_core_reset */
	JPEG_DBG("%s:%d]\n", __func__, __LINE__);
	return NULL;
}

void *msm_jpeg_core_err_irq(int jpeg_irq_status,
	struct msm_jpeg_device *pgmn_dev)
{
	JPEG_PR_ERR("%s: Error %x\n", __func__, jpeg_irq_status);
	return NULL;
}

static int (*msm_jpeg_irq_handler)(int, void *, void *);

void msm_jpeg_core_return_buffers(struct msm_jpeg_device *pgmn_dev,
	 int jpeg_irq_status)
{
	void *data = NULL;

	data = msm_jpeg_core_fe_pingpong_irq(jpeg_irq_status,
		pgmn_dev);
	if (msm_jpeg_irq_handler)
		msm_jpeg_irq_handler(MSM_JPEG_HW_MASK_COMP_FE,
			pgmn_dev, data);
	data = msm_jpeg_core_we_pingpong_irq(jpeg_irq_status,
		pgmn_dev);
	if (msm_jpeg_irq_handler)
		msm_jpeg_irq_handler(MSM_JPEG_HW_MASK_COMP_WE,
			pgmn_dev, data);
}

irqreturn_t msm_jpeg_core_irq(int irq_num, void *context)
{
	void *data = NULL;
	unsigned long flags;
	int jpeg_irq_status;
	struct msm_jpeg_device *pgmn_dev = (struct msm_jpeg_device *)context;

	JPEG_DBG("%s:%d] irq_num = %d\n", __func__, __LINE__, irq_num);

	jpeg_irq_status = msm_jpeg_hw_irq_get_status(pgmn_dev->base);

	JPEG_DBG("%s:%d] jpeg_irq_status = %0x\n", __func__, __LINE__,
		jpeg_irq_status);

	/* For reset and framedone IRQs, clear all bits */
	if (pgmn_dev->state == MSM_JPEG_IDLE) {
		JPEG_DBG_HIGH("%s %d ] Error IRQ received state %d",
		__func__, __LINE__, pgmn_dev->state);
		JPEG_DBG_HIGH("%s %d ] Ignoring the Error", __func__,
		__LINE__);
		msm_jpeg_hw_irq_clear(JPEG_IRQ_CLEAR_BMSK,
			JPEG_IRQ_CLEAR_ALL, pgmn_dev->base);
		return IRQ_HANDLED;
	} else if (jpeg_irq_status & 0x10000000) {
		msm_jpeg_hw_irq_clear(JPEG_IRQ_CLEAR_BMSK,
			JPEG_IRQ_CLEAR_ALL, pgmn_dev->base);
	} else if (jpeg_irq_status & 0x1) {
		msm_jpeg_hw_irq_clear(JPEG_IRQ_CLEAR_BMSK,
			JPEG_IRQ_CLEAR_ALL, pgmn_dev->base);
		if (pgmn_dev->decode_flag)
			msm_jpeg_decode_status(pgmn_dev->base);
	} else {
		msm_jpeg_hw_irq_clear(JPEG_IRQ_CLEAR_BMSK,
			jpeg_irq_status, pgmn_dev->base);
	}

	if (msm_jpeg_hw_irq_is_frame_done(jpeg_irq_status)) {
		/* send fe ping pong irq */
		JPEG_DBG_HIGH("%s:%d] Session done\n", __func__, __LINE__);
		data = msm_jpeg_core_fe_pingpong_irq(jpeg_irq_status,
			pgmn_dev);
		if (msm_jpeg_irq_handler)
			msm_jpeg_irq_handler(MSM_JPEG_HW_MASK_COMP_FE,
				context, data);
		data = msm_jpeg_core_framedone_irq(jpeg_irq_status,
			pgmn_dev);
		if (msm_jpeg_irq_handler)
			msm_jpeg_irq_handler(
				MSM_JPEG_HW_MASK_COMP_FRAMEDONE,
				context, data);
		pgmn_dev->state = MSM_JPEG_INIT;
	}
	if (msm_jpeg_hw_irq_is_reset_ack(jpeg_irq_status)) {
		data = msm_jpeg_core_reset_ack_irq(jpeg_irq_status,
			pgmn_dev);
		spin_lock_irqsave(&pgmn_dev->reset_lock, flags);
		pgmn_dev->reset_done_ack = 1;
		spin_unlock_irqrestore(&pgmn_dev->reset_lock, flags);
		wake_up(&pgmn_dev->reset_wait);
		if (msm_jpeg_irq_handler)
			msm_jpeg_irq_handler(
				MSM_JPEG_HW_MASK_COMP_RESET_ACK,
				context, data);
	}

	/* Unexpected/unintended HW interrupt */
	if (msm_jpeg_hw_irq_is_err(jpeg_irq_status)) {
		if (pgmn_dev->state != MSM_JPEG_EXECUTING) {
			/* Clear all the bits and ignore the IRQ */
			JPEG_DBG_HIGH("%s %d ] Error IRQ received state %d",
			__func__, __LINE__, pgmn_dev->state);
			JPEG_DBG_HIGH("%s %d ] Ignoring the Error", __func__,
			__LINE__);
			msm_jpeg_hw_irq_clear(JPEG_IRQ_CLEAR_BMSK,
			JPEG_IRQ_CLEAR_ALL, pgmn_dev->base);
			return IRQ_HANDLED;
		}
		if (pgmn_dev->decode_flag)
			msm_jpeg_decode_status(pgmn_dev->base);
		msm_jpeg_core_return_buffers(pgmn_dev, jpeg_irq_status);
		data = msm_jpeg_core_err_irq(jpeg_irq_status, pgmn_dev);
		if (msm_jpeg_irq_handler) {
			msm_jpeg_irq_handler(MSM_JPEG_HW_MASK_COMP_ERR,
				context, data);
		}
	}

	return IRQ_HANDLED;
}

irqreturn_t msm_jpegdma_core_irq(int irq_num, void *context)
{
	void *data = NULL;
	unsigned long flags;
	int jpeg_irq_status;
	struct msm_jpeg_device *pgmn_dev = context;

	JPEG_DBG("%s:%d] irq_num = %d\n", __func__, __LINE__, irq_num);

	jpeg_irq_status = msm_jpegdma_hw_irq_get_status(pgmn_dev->base);

	JPEG_DBG("%s:%d] jpeg_irq_status = %0x\n", __func__, __LINE__,
		jpeg_irq_status);

	/* For reset and framedone IRQs, clear all bits */
	if (pgmn_dev->state == MSM_JPEG_IDLE) {
		JPEG_DBG_HIGH("%s %d ] Error IRQ received state %d",
		__func__, __LINE__, pgmn_dev->state);
		JPEG_DBG_HIGH("%s %d ] Ignoring the Error", __func__,
		__LINE__);
		msm_jpegdma_hw_irq_clear(JPEGDMA_IRQ_CLEAR_BMSK,
			JPEGDMA_IRQ_CLEAR_ALL, pgmn_dev->base);
		return IRQ_HANDLED;
	} else if (jpeg_irq_status & 0x00000400) {
		msm_jpegdma_hw_irq_clear(JPEGDMA_IRQ_CLEAR_BMSK,
			JPEGDMA_IRQ_CLEAR_ALL, pgmn_dev->base);
	} else if (jpeg_irq_status & 0x1) {
		msm_jpegdma_hw_irq_clear(JPEGDMA_IRQ_CLEAR_BMSK,
			JPEGDMA_IRQ_CLEAR_ALL, pgmn_dev->base);
	} else {
		msm_jpegdma_hw_irq_clear(JPEGDMA_IRQ_CLEAR_BMSK,
			jpeg_irq_status, pgmn_dev->base);
	}

	if (msm_jpegdma_hw_irq_is_frame_done(jpeg_irq_status)) {
		/* send fe ping pong irq */
		JPEG_DBG_HIGH("%s:%d] Session done\n", __func__, __LINE__);
		data = msm_jpeg_core_fe_pingpong_irq(jpeg_irq_status,
			pgmn_dev);
		if (msm_jpeg_irq_handler)
			msm_jpeg_irq_handler(MSM_JPEG_HW_MASK_COMP_FE,
				context, data);
		data = msm_jpeg_core_framedone_irq(jpeg_irq_status,
			pgmn_dev);
		if (msm_jpeg_irq_handler)
			msm_jpeg_irq_handler(
				MSM_JPEG_HW_MASK_COMP_FRAMEDONE,
				context, data);
		pgmn_dev->state = MSM_JPEG_INIT;
	}
	if (msm_jpegdma_hw_irq_is_reset_ack(jpeg_irq_status)) {
		data = msm_jpeg_core_reset_ack_irq(jpeg_irq_status,
			pgmn_dev);
		spin_lock_irqsave(&pgmn_dev->reset_lock, flags);
		pgmn_dev->reset_done_ack = 1;
		spin_unlock_irqrestore(&pgmn_dev->reset_lock, flags);
		wake_up(&pgmn_dev->reset_wait);
		if (msm_jpeg_irq_handler)
			msm_jpeg_irq_handler(
				MSM_JPEG_HW_MASK_COMP_RESET_ACK,
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
