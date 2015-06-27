/* Copyright (c) 2010-2015, The Linux Foundation. All rights reserved.
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

#include <linux/io.h>
#include <linux/list.h>
#include <linux/types.h>
#include <linux/stat.h>
#include <linux/slab.h>
#include <linux/device.h>

#include "mdss_hdmi_cec.h"
#include "mdss_panel.h"

#define CEC_STATUS_WR_ERROR	BIT(0)
#define CEC_STATUS_WR_DONE	BIT(1)
#define CEC_INTR		(BIT(1) | BIT(3) | BIT(7))

#define CEC_SUPPORTED_HW_VERSION 0x30000001

/* Reference: HDMI 1.4a Specification section 7.1 */

struct hdmi_cec_ctrl {
	bool cec_enabled;

	u32 cec_msg_wr_status;
	spinlock_t lock;
	struct work_struct cec_read_work;
	struct completion cec_msg_wr_done;
	struct hdmi_cec_init_data init_data;
};

static int hdmi_cec_msg_send(void *data, struct cec_msg *msg)
{
	int i, line_check_retry = 10, rc = 0;
	u32 frame_retransmit = RETRANSMIT_MAX_NUM;
	bool frame_type;
	unsigned long flags;
	struct dss_io_data *io = NULL;
	struct hdmi_cec_ctrl *cec_ctrl = (struct hdmi_cec_ctrl *)data;

	if (!cec_ctrl || !cec_ctrl->init_data.io || !msg) {
		DEV_ERR("%s: Invalid input\n", __func__);
		return -EINVAL;
	}

	io = cec_ctrl->init_data.io;

	INIT_COMPLETION(cec_ctrl->cec_msg_wr_done);
	cec_ctrl->cec_msg_wr_status = 0;
	frame_type = (msg->recvr_id == 15 ? BIT(0) : 0);
	if (msg->retransmit > 0 && msg->retransmit < RETRANSMIT_MAX_NUM)
		frame_retransmit = msg->retransmit;

	/* toggle cec in order to flush out bad hw state, if any */
	DSS_REG_W(io, HDMI_CEC_CTRL, 0);
	DSS_REG_W(io, HDMI_CEC_CTRL, BIT(0));

	frame_retransmit = (frame_retransmit & 0xF) << 4;
	DSS_REG_W(io, HDMI_CEC_RETRANSMIT, BIT(0) | frame_retransmit);

	/* header block */
	DSS_REG_W_ND(io, HDMI_CEC_WR_DATA,
		(((msg->sender_id << 4) | msg->recvr_id) << 8) | frame_type);

	/* data block 0 : opcode */
	DSS_REG_W_ND(io, HDMI_CEC_WR_DATA,
		((msg->frame_size < 2 ? 0 : msg->opcode) << 8) | frame_type);

	/* data block 1-14 : operand 0-13 */
	for (i = 0; i < msg->frame_size - 2; i++)
		DSS_REG_W_ND(io, HDMI_CEC_WR_DATA,
			(msg->operand[i] << 8) | frame_type);

	while ((DSS_REG_R(io, HDMI_CEC_STATUS) & BIT(0)) &&
		line_check_retry) {
		line_check_retry--;
		DEV_DBG("%s: CEC line is busy(%d)\n", __func__,
			line_check_retry);
		schedule();
	}

	if (!line_check_retry && (DSS_REG_R(io, HDMI_CEC_STATUS) & BIT(0))) {
		DEV_ERR("%s: CEC line is busy. Retry\n", __func__);
		return -EAGAIN;
	}

	/* start transmission */
	DSS_REG_W(io, HDMI_CEC_CTRL, BIT(0) | BIT(1) |
		((msg->frame_size & 0x1F) << 4) | BIT(9));

	if (!wait_for_completion_timeout(
		&cec_ctrl->cec_msg_wr_done, HZ)) {
		DEV_ERR("%s: timedout", __func__);
		return -ETIMEDOUT;
	}

	spin_lock_irqsave(&cec_ctrl->lock, flags);
	if (cec_ctrl->cec_msg_wr_status == CEC_STATUS_WR_ERROR) {
		rc = -ENXIO;
		DEV_ERR("%s: msg write failed.\n", __func__);
	} else {
		DEV_DBG("%s: CEC write frame done (frame len=%d)", __func__,
			msg->frame_size);
	}
	spin_unlock_irqrestore(&cec_ctrl->lock, flags);

	return rc;
} /* hdmi_cec_msg_send */

static void hdmi_cec_msg_recv(struct work_struct *work)
{
	int i;
	u32 data;
	struct hdmi_cec_ctrl *cec_ctrl = NULL;
	struct dss_io_data *io = NULL;
	struct cec_msg msg;
	struct cec_cbs *cbs;

	cec_ctrl = container_of(work, struct hdmi_cec_ctrl, cec_read_work);
	if (!cec_ctrl || !cec_ctrl->init_data.io) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	if (!cec_ctrl->cec_enabled) {
		DEV_ERR("%s: cec not enabled\n", __func__);
		return;
	}

	io = cec_ctrl->init_data.io;
	cbs = cec_ctrl->init_data.cbs;

	data = DSS_REG_R(io, HDMI_CEC_RD_DATA);

	msg.recvr_id   = (data & 0x000F);
	msg.sender_id  = (data & 0x00F0) >> 4;
	msg.frame_size = (data & 0x1F00) >> 8;
	DEV_DBG("%s: Recvd init=[%u] dest=[%u] size=[%u]\n", __func__,
		msg.sender_id, msg.recvr_id,
		msg.frame_size);

	if (msg.frame_size < 1) {
		DEV_ERR("%s: invalid message (frame length = %d)\n",
			__func__, msg.frame_size);
		return;
	} else if (msg.frame_size == 1) {
		DEV_DBG("%s: polling message (dest[%x] <- init[%x])\n",
			__func__, msg.recvr_id, msg.sender_id);
		return;
	}

	/* data block 0 : opcode */
	data = DSS_REG_R_ND(io, HDMI_CEC_RD_DATA);
	msg.opcode = data & 0xFF;

	/* data block 1-14 : operand 0-13 */
	for (i = 0; i < msg.frame_size - 2; i++) {
		data = DSS_REG_R_ND(io, HDMI_CEC_RD_DATA);
		msg.operand[i] = data & 0xFF;
	}

	for (; i < 14; i++)
		msg.operand[i] = 0;

	if (cbs && cbs->msg_recv_notify)
		cbs->msg_recv_notify(cbs->data, &msg);
}

int hdmi_cec_isr(void *input)
{
	int rc = 0;
	u32 cec_intr, cec_status;
	unsigned long flags;
	struct dss_io_data *io = NULL;
	struct hdmi_cec_ctrl *cec_ctrl = (struct hdmi_cec_ctrl *)input;

	if (!cec_ctrl || !cec_ctrl->init_data.io) {
		DEV_ERR("%s: Invalid input\n", __func__);
		return -EPERM;
	}

	io = cec_ctrl->init_data.io;

	cec_intr = DSS_REG_R_ND(io, HDMI_CEC_INT);

	if (!cec_ctrl->cec_enabled) {
		DSS_REG_W(io, HDMI_CEC_INT, cec_intr);
		return 0;
	}

	cec_status = DSS_REG_R_ND(io, HDMI_CEC_STATUS);

	if ((cec_intr & BIT(0)) && (cec_intr & BIT(1))) {
		DEV_DBG("%s: CEC_IRQ_FRAME_WR_DONE\n", __func__);
		DSS_REG_W(io, HDMI_CEC_INT, cec_intr | BIT(0));

		spin_lock_irqsave(&cec_ctrl->lock, flags);
		cec_ctrl->cec_msg_wr_status |= CEC_STATUS_WR_DONE;
		spin_unlock_irqrestore(&cec_ctrl->lock, flags);

		if (!completion_done(&cec_ctrl->cec_msg_wr_done))
			complete_all(&cec_ctrl->cec_msg_wr_done);
	}

	if ((cec_intr & BIT(2)) && (cec_intr & BIT(3))) {
		DEV_DBG("%s: CEC_IRQ_FRAME_ERROR\n", __func__);
		DSS_REG_W(io, HDMI_CEC_INT, cec_intr | BIT(2));

		spin_lock_irqsave(&cec_ctrl->lock, flags);
		cec_ctrl->cec_msg_wr_status |= CEC_STATUS_WR_ERROR;
		spin_unlock_irqrestore(&cec_ctrl->lock, flags);

		if (!completion_done(&cec_ctrl->cec_msg_wr_done))
			complete_all(&cec_ctrl->cec_msg_wr_done);
	}

	if ((cec_intr & BIT(6)) && (cec_intr & BIT(7))) {
		DEV_DBG("%s: CEC_IRQ_FRAME_RD_DONE\n", __func__);

		DSS_REG_W(io, HDMI_CEC_INT, cec_intr | BIT(6));
		queue_work(cec_ctrl->init_data.workq, &cec_ctrl->cec_read_work);
	}

	return rc;
}

static void hdmi_cec_write_logical_addr(void *input, u8 addr)
{
	struct hdmi_cec_ctrl *cec_ctrl = (struct hdmi_cec_ctrl *)input;

	if (!cec_ctrl || !cec_ctrl->init_data.io) {
		DEV_ERR("%s: Invalid input\n", __func__);
		return;
	}

	if (cec_ctrl->cec_enabled)
		DSS_REG_W(cec_ctrl->init_data.io, HDMI_CEC_ADDR, addr & 0xF);
}

static int hdmi_cec_enable(void *input, bool enable)
{
	int ret = 0;
	u32 hdmi_hw_version, reg_val;
	struct dss_io_data *io = NULL;
	struct hdmi_cec_ctrl *cec_ctrl = (struct hdmi_cec_ctrl *)input;
	struct mdss_panel_info *pinfo;

	if (!cec_ctrl || !cec_ctrl->init_data.io) {
		DEV_ERR("%s: Invalid input\n", __func__);
		ret = -EPERM;
		goto end;
	}

	io = cec_ctrl->init_data.io;
	pinfo = cec_ctrl->init_data.pinfo;

	if (!pinfo) {
		DEV_ERR("%s: invalid pinfo\n", __func__);
		goto end;
	}

	if (enable) {
		/* 19.2Mhz * 0.00005 us = 950 = 0x3B6 */
		DSS_REG_W(io, HDMI_CEC_REFTIMER, (0x3B6 & 0xFFF) | BIT(16));

		hdmi_hw_version = DSS_REG_R(io, HDMI_VERSION);
		if (hdmi_hw_version >= CEC_SUPPORTED_HW_VERSION) {
			DSS_REG_W(io, HDMI_CEC_RD_RANGE, 0x30AB9888);
			DSS_REG_W(io, HDMI_CEC_WR_RANGE, 0x888AA888);

			DSS_REG_W(io, HDMI_CEC_RD_START_RANGE, 0x88888888);
			DSS_REG_W(io, HDMI_CEC_RD_TOTAL_RANGE, 0x99);
			DSS_REG_W(io, HDMI_CEC_COMPL_CTL, 0xF);
			DSS_REG_W(io, HDMI_CEC_WR_CHECK_CONFIG, 0x4);
		} else {
			DEV_DBG("%s: CEC version %d is not supported.\n",
				__func__, hdmi_hw_version);
			ret = -EPERM;
			goto end;
		}

		DSS_REG_W(io, HDMI_CEC_RD_FILTER, BIT(0) | (0x7FF << 4));
		DSS_REG_W(io, HDMI_CEC_TIME, BIT(0) | ((7 * 0x30) << 7));

		/* Enable CEC interrupts */
		DSS_REG_W(io, HDMI_CEC_INT, CEC_INTR);

		/* Enable Engine */
		DSS_REG_W(io, HDMI_CEC_CTRL, BIT(0));
	} else {
		/* Disable Engine */
		DSS_REG_W(io, HDMI_CEC_CTRL, 0);

		/* Disable CEC interrupts */
		reg_val = DSS_REG_R(io, HDMI_CEC_INT);
		DSS_REG_W(io, HDMI_CEC_INT, reg_val & ~CEC_INTR);
	}

	cec_ctrl->cec_enabled = enable;
end:
	return ret;
}

void *hdmi_cec_init(struct hdmi_cec_init_data *init_data)
{
	struct hdmi_cec_ctrl *cec_ctrl;
	struct cec_ops *ops;
	int ret = 0;

	if (!init_data) {
		DEV_ERR("%s: Invalid input\n", __func__);
		ret = -EINVAL;
		goto error;
	}

	ops = init_data->ops;
	if (ops) {
		DEV_ERR("%s: no ops provided\n", __func__);
		ret = -EINVAL;
		goto error;
	}

	cec_ctrl = kzalloc(sizeof(*cec_ctrl), GFP_KERNEL);
	if (!cec_ctrl) {
		DEV_ERR("%s: FAILED: out of memory\n", __func__);
		ret = -EINVAL;
		goto error;
	}

	/* keep a copy of init data */
	cec_ctrl->init_data = *init_data;

	spin_lock_init(&cec_ctrl->lock);
	INIT_WORK(&cec_ctrl->cec_read_work, hdmi_cec_msg_recv);
	init_completion(&cec_ctrl->cec_msg_wr_done);

	/* populate hardware specific operations to client */
	ops->send_msg = hdmi_cec_msg_send;
	ops->wt_logical_addr = hdmi_cec_write_logical_addr;
	ops->enable = hdmi_cec_enable;
	ops->data = cec_ctrl;

	return cec_ctrl;
error:
	return ERR_PTR(ret);
}

void hdmi_cec_deinit(void *data)
{
	struct hdmi_cec_ctrl *cec_ctrl = (struct hdmi_cec_ctrl *)data;

	kfree(cec_ctrl);
}
