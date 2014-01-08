/* Copyright (c) 2010-2013, The Linux Foundation. All rights reserved.
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
#include <mach/board.h>

#include "mdss_hdmi_cec.h"

#define CEC_STATUS_WR_ERROR	BIT(0)
#define CEC_STATUS_WR_DONE	BIT(1)

/* Reference: HDMI 1.4a Specification section 7.1 */
#define RETRANSMIT_MAX_NUM	5
#define MAX_OPERAND_SIZE	15

/*
 * Ref. HDMI 1.4a: Supplement-1 CEC Section 6, 7
 */
struct hdmi_cec_msg {
	u8 sender_id;
	u8 recvr_id;
	u8 opcode;
	u8 operand[MAX_OPERAND_SIZE];
	u8 frame_size;
	u8 retransmit;
};

struct hdmi_cec_msg_node {
	struct hdmi_cec_msg msg;
	struct list_head list;
};

struct hdmi_cec_ctrl {
	bool cec_enabled;
	bool compliance_response_enabled;
	bool cec_engine_configed;

	u8 cec_logical_addr;
	u32 cec_msg_wr_status;

	spinlock_t lock;
	struct list_head msg_head;
	struct work_struct cec_read_work;
	struct completion cec_msg_wr_done;
	struct hdmi_cec_init_data init_data;
};

static int hdmi_cec_msg_send(struct hdmi_cec_ctrl *cec_ctrl,
	struct hdmi_cec_msg *msg);

static void hdmi_cec_dump_msg(struct hdmi_cec_ctrl *cec_ctrl,
	struct hdmi_cec_msg *msg)
{
	int i;
	unsigned long flags;

	if (!cec_ctrl || !msg) {
		DEV_ERR("%pS->%s: invalid input\n",
			__builtin_return_address(0), __func__);
		return;
	}

	spin_lock_irqsave(&cec_ctrl->lock, flags);
	DEV_DBG("=================%pS dump start =====================\n",
		__builtin_return_address(0));

	DEV_DBG("sender_id     : %d", msg->sender_id);
	DEV_DBG("recvr_id      : %d", msg->recvr_id);

	if (msg->frame_size < 2) {
		DEV_DBG("polling message");
		spin_unlock_irqrestore(&cec_ctrl->lock, flags);
		return;
	}

	DEV_DBG("opcode        : %02x", msg->opcode);
	for (i = 0; i < msg->frame_size - 2; i++)
		DEV_DBG("operand(%2d) : %02x", i + 1, msg->operand[i]);

	DEV_DBG("=================%pS dump end =====================\n",
		__builtin_return_address(0));
	spin_unlock_irqrestore(&cec_ctrl->lock, flags);
} /* hdmi_cec_dump_msg */

static inline void hdmi_cec_write_logical_addr(struct hdmi_cec_ctrl *cec_ctrl,
	u8 addr)
{
	if (!cec_ctrl || !cec_ctrl->init_data.io) {
		DEV_ERR("%s: Invalid input\n", __func__);
		return;
	}

	DSS_REG_W(cec_ctrl->init_data.io, HDMI_CEC_ADDR, addr & 0xF);
} /* hdmi_cec_write_logical_addr */

static void hdmi_cec_disable(struct hdmi_cec_ctrl *cec_ctrl)
{
	u32 reg_val;
	unsigned long flags;
	struct dss_io_data *io = NULL;
	struct hdmi_cec_msg_node *msg_node, *tmp;

	if (!cec_ctrl || !cec_ctrl->init_data.io) {
		DEV_ERR("%s: Invalid input\n", __func__);
		return;
	}

	io = cec_ctrl->init_data.io;

	/* Disable Engine */
	DSS_REG_W(io, HDMI_CEC_CTRL, 0);

	/* Disable CEC interrupts */
	reg_val = DSS_REG_R(io, HDMI_CEC_INT);
	DSS_REG_W(io, HDMI_CEC_INT, reg_val & !BIT(1) & !BIT(3) & !BIT(7));

	spin_lock_irqsave(&cec_ctrl->lock, flags);
	list_for_each_entry_safe(msg_node, tmp, &cec_ctrl->msg_head, list) {
		list_del(&msg_node->list);
		kfree(msg_node);
	}
	spin_unlock_irqrestore(&cec_ctrl->lock, flags);
} /* hdmi_cec_disable */

static void hdmi_cec_enable(struct hdmi_cec_ctrl *cec_ctrl)
{
	struct dss_io_data *io = NULL;

	if (!cec_ctrl || !cec_ctrl->init_data.io) {
		DEV_ERR("%s: Invalid input\n", __func__);
		return;
	}

	io = cec_ctrl->init_data.io;

	INIT_LIST_HEAD(&cec_ctrl->msg_head);

	/* Enable CEC interrupts */
	DSS_REG_W(io, HDMI_CEC_INT, BIT(1) | BIT(3) | BIT(7));

	/* Enable Engine */
	DSS_REG_W(io, HDMI_CEC_CTRL, BIT(0));
} /* hdmi_cec_enable */

static int hdmi_cec_send_abort_opcode(struct hdmi_cec_ctrl *cec_ctrl,
	struct hdmi_cec_msg *in_msg, u8 reason_operand)
{
	int i = 0;
	struct hdmi_cec_msg out_msg;

	if (!cec_ctrl || !in_msg) {
		DEV_ERR("%s: Invalid input\n", __func__);
		return -EINVAL;
	}

	out_msg.sender_id = 0x4;
	out_msg.recvr_id = in_msg->sender_id;
	out_msg.opcode = 0x0; /* opcode for feature abort */
	out_msg.operand[i++] = in_msg->opcode;
	out_msg.operand[i++] = reason_operand;
	out_msg.frame_size = i + 2;

	return hdmi_cec_msg_send(cec_ctrl, &out_msg);
} /* hdmi_cec_send_abort_opcode */

static int hdmi_cec_msg_parser(struct hdmi_cec_ctrl *cec_ctrl,
	struct hdmi_cec_msg *in_msg)
{
	int rc = 0, i = 0;
	struct hdmi_cec_msg out_msg;

	if (!cec_ctrl || !in_msg) {
		DEV_ERR("%s: Invalid input\n", __func__);
		return -EINVAL;
	}

	DEV_DBG("%s: in_msg->opcode = 0x%x\n", __func__, in_msg->opcode);
	switch (in_msg->opcode) {
	case 0x64:
		/* Set OSD String */
		DEV_INFO("%s: Recvd OSD Str=[0x%x]\n", __func__,
			in_msg->operand[3]);
		break;
	case 0x83:
		/* Give Phy Addr */
		DEV_INFO("%s: Recvd a Give Phy Addr cmd\n", __func__);

		out_msg.sender_id = 0x4;
		out_msg.recvr_id = 0xF; /* Broadcast */
		out_msg.opcode = 0x84;
		out_msg.operand[i++] = 0x10;
		out_msg.operand[i++] = 0x0;
		out_msg.operand[i++] = 0x04;
		out_msg.frame_size = i + 2;

		rc = hdmi_cec_msg_send(cec_ctrl, &out_msg);
		break;
	case 0xFF:
		/* Abort */
		DEV_INFO("%s: Recvd an abort cmd.\n", __func__);

		/* reason = "Refused" */
		rc = hdmi_cec_send_abort_opcode(cec_ctrl, in_msg, 0x04);
		break;
	case 0x46:
		/* Give OSD name */
		DEV_INFO("%s: Recvd 'Give OSD name' cmd.\n", __func__);

		out_msg.sender_id = 0x4;
		out_msg.recvr_id = in_msg->sender_id;
		out_msg.opcode = 0x47; /* OSD Name */
		/* Display control byte */
		out_msg.operand[i++] = 0x0;
		out_msg.operand[i++] = 'H';
		out_msg.operand[i++] = 'e';
		out_msg.operand[i++] = 'l';
		out_msg.operand[i++] = 'l';
		out_msg.operand[i++] = 'o';
		out_msg.operand[i++] = ' ';
		out_msg.operand[i++] = 'W';
		out_msg.operand[i++] = 'o';
		out_msg.operand[i++] = 'r';
		out_msg.operand[i++] = 'l';
		out_msg.operand[i++] = 'd';
		out_msg.frame_size = i + 2;

		rc = hdmi_cec_msg_send(cec_ctrl, &out_msg);
		break;
	case 0x8F:
		/* Give Device Power status */
		DEV_INFO("%s: Recvd a Power status message\n", __func__);

		out_msg.sender_id = 0x4;
		out_msg.recvr_id = in_msg->sender_id;
		out_msg.opcode = 0x90; /* OSD String */
		out_msg.operand[i++] = 'H';
		out_msg.operand[i++] = 'e';
		out_msg.operand[i++] = 'l';
		out_msg.operand[i++] = 'l';
		out_msg.operand[i++] = 'o';
		out_msg.operand[i++] = ' ';
		out_msg.operand[i++] = 'W';
		out_msg.operand[i++] = 'o';
		out_msg.operand[i++] = 'r';
		out_msg.operand[i++] = 'l';
		out_msg.operand[i++] = 'd';
		out_msg.frame_size = i + 2;

		rc = hdmi_cec_msg_send(cec_ctrl, &out_msg);
		break;
	case 0x80:
		/* Routing Change cmd */
	case 0x86:
		/* Set Stream Path */
		DEV_INFO("%s: Recvd Set Stream or Routing Change cmd\n",
			__func__);

		out_msg.sender_id = 0x4;
		out_msg.recvr_id = 0xF; /* broadcast this message */
		out_msg.opcode = 0x82; /* Active Source */
		out_msg.operand[i++] = 0x10;
		out_msg.operand[i++] = 0x0;
		out_msg.frame_size = i + 2;

		rc = hdmi_cec_msg_send(cec_ctrl, &out_msg);

		/* todo: check if need to wait for msg response from sink */

		/* sending <Image View On> message */
		memset(&out_msg, 0x0, sizeof(struct hdmi_cec_msg));
		i = 0;
		out_msg.sender_id = 0x4;
		out_msg.recvr_id = in_msg->sender_id;
		out_msg.opcode = 0x04; /* opcode for Image View On */
		out_msg.frame_size = i + 2;

		rc = hdmi_cec_msg_send(cec_ctrl, &out_msg);
		break;
	case 0x44:
		/* User Control Pressed */
		DEV_INFO("%s: User Control Pressed\n", __func__);
		break;
	case 0x45:
		/* User Control Released */
		DEV_INFO("%s: User Control Released\n", __func__);
		break;
	default:
		DEV_INFO("%s: Recvd an unknown cmd = [%u]\n", __func__,
			in_msg->opcode);

		/* reason = "Unrecognized opcode" */
		rc = hdmi_cec_send_abort_opcode(cec_ctrl, in_msg, 0x0);
		break;
	}

	return rc;
} /* hdmi_cec_msg_parser */

static int hdmi_cec_msg_send(struct hdmi_cec_ctrl *cec_ctrl,
	struct hdmi_cec_msg *msg)
{
	int i, line_check_retry = 10;
	u32 frame_retransmit = RETRANSMIT_MAX_NUM;
	bool frame_type;
	unsigned long flags;
	struct dss_io_data *io = NULL;

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
		hdmi_cec_dump_msg(cec_ctrl, msg);
		return -ETIMEDOUT;
	}

	spin_lock_irqsave(&cec_ctrl->lock, flags);
	if (cec_ctrl->cec_msg_wr_status == CEC_STATUS_WR_ERROR)
		DEV_ERR("%s: msg write failed.\n", __func__);
	else
		DEV_DBG("%s: CEC write frame done (frame len=%d)", __func__,
			msg->frame_size);
	spin_unlock_irqrestore(&cec_ctrl->lock, flags);
	hdmi_cec_dump_msg(cec_ctrl, msg);

	return 0;
} /* hdmi_cec_msg_send */

static void hdmi_cec_msg_recv(struct work_struct *work)
{
	int i;
	u32 data;
	unsigned long flags;
	struct hdmi_cec_ctrl *cec_ctrl = NULL;
	struct dss_io_data *io = NULL;
	struct hdmi_cec_msg_node *msg_node = NULL;

	cec_ctrl = container_of(work, struct hdmi_cec_ctrl, cec_read_work);
	if (!cec_ctrl || !cec_ctrl->cec_enabled || !cec_ctrl->init_data.io) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	io = cec_ctrl->init_data.io;

	msg_node = kzalloc(sizeof(*msg_node), GFP_KERNEL);
	if (!msg_node) {
		DEV_ERR("%s: FAILED: out of memory\n", __func__);
		return;
	}

	data = DSS_REG_R(io, HDMI_CEC_RD_DATA);

	msg_node->msg.recvr_id   = (data & 0x000F);
	msg_node->msg.sender_id  = (data & 0x00F0) >> 4;
	msg_node->msg.frame_size = (data & 0x1F00) >> 8;
	DEV_DBG("%s: Recvd init=[%u] dest=[%u] size=[%u]\n", __func__,
		msg_node->msg.sender_id, msg_node->msg.recvr_id,
		msg_node->msg.frame_size);

	if (msg_node->msg.frame_size < 1) {
		DEV_ERR("%s: invalid message (frame length = %d)",
			__func__, msg_node->msg.frame_size);
		kfree(msg_node);
		return;
	} else if (msg_node->msg.frame_size == 1) {
		DEV_DBG("%s: polling message (dest[%x] <- init[%x])", __func__,
			msg_node->msg.recvr_id, msg_node->msg.sender_id);
		kfree(msg_node);
		return;
	}

	/* data block 0 : opcode */
	data = DSS_REG_R_ND(io, HDMI_CEC_RD_DATA);
	msg_node->msg.opcode = data & 0xFF;

	/* data block 1-14 : operand 0-13 */
	for (i = 0; i < msg_node->msg.frame_size - 2; i++) {
		data = DSS_REG_R_ND(io, HDMI_CEC_RD_DATA);
		msg_node->msg.operand[i] = data & 0xFF;
	}

	for (; i < 14; i++)
		msg_node->msg.operand[i] = 0;

	DEV_DBG("%s: CEC read frame done\n", __func__);
	hdmi_cec_dump_msg(cec_ctrl, &msg_node->msg);

	spin_lock_irqsave(&cec_ctrl->lock, flags);
	if (cec_ctrl->compliance_response_enabled) {
		spin_unlock_irqrestore(&cec_ctrl->lock, flags);

		if (hdmi_cec_msg_parser(cec_ctrl, &msg_node->msg) != 0) {
			DEV_ERR("%s: cec_msg_parser fail. Sending abort msg\n",
				__func__);
			/* reason = "Unrecognized opcode" */
			hdmi_cec_send_abort_opcode(cec_ctrl,
				&msg_node->msg, 0x0);
		}
		kfree(msg_node);
	} else {
		list_add_tail(&msg_node->list, &cec_ctrl->msg_head);
		spin_unlock_irqrestore(&cec_ctrl->lock, flags);

		/* wake-up sysfs read_msg context */
		sysfs_notify(cec_ctrl->init_data.sysfs_kobj, "cec", "rd_msg");
	}
} /* hdmi_cec_msg_recv*/

static ssize_t hdmi_rda_cec_enable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret;
	unsigned long flags;
	struct hdmi_cec_ctrl *cec_ctrl =
		hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_CEC);

	if (!cec_ctrl || !cec_ctrl->init_data.io) {
		DEV_ERR("%s: Invalid input\n", __func__);
		return -EPERM;
	}

	spin_lock_irqsave(&cec_ctrl->lock, flags);
	if (cec_ctrl->cec_enabled && cec_ctrl->cec_engine_configed) {
		DEV_DBG("%s: cec is enabled\n", __func__);
		ret = snprintf(buf, PAGE_SIZE, "%d\n", 1);
	} else if (cec_ctrl->cec_enabled && !cec_ctrl->cec_engine_configed) {
		DEV_ERR("%s: CEC will be enabled when HDMI mirroring is on\n",
			__func__);
		ret = -EPERM;
	} else {
		DEV_DBG("%s: cec is disabled\n", __func__);
		ret = snprintf(buf, PAGE_SIZE, "%d\n", 0);
	}
	spin_unlock_irqrestore(&cec_ctrl->lock, flags);

	return ret;
} /* hdmi_rda_cec_enable */

static ssize_t hdmi_wta_cec_enable(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int val;
	bool cec_en;
	unsigned long flags;
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct hdmi_cec_ctrl *cec_ctrl =
		hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_CEC);

	if (!cec_ctrl) {
		DEV_ERR("%s: Invalid input\n", __func__);
		return -EPERM;
	}

	if (kstrtoint(buf, 10, &val)) {
		DEV_ERR("%s: kstrtoint failed.\n", __func__);
		return -EPERM;
	}
	cec_en = (val == 1) ? true : false;

	spin_lock_irqsave(&cec_ctrl->lock, flags);
	if (cec_ctrl->cec_enabled == cec_en) {
		spin_unlock_irqrestore(&cec_ctrl->lock, flags);
		DEV_INFO("%s: cec is already %s\n", __func__,
			cec_en ? "enabled" : "disabled");
		return ret;
	}
	spin_unlock_irqrestore(&cec_ctrl->lock, flags);

	if (!cec_en) {
		spin_lock_irqsave(&cec_ctrl->lock, flags);
		if (!cec_ctrl->cec_engine_configed) {
			DEV_DBG("%s: hdmi is already off. disable cec\n",
				__func__);
			cec_ctrl->cec_enabled = false;
			spin_unlock_irqrestore(&cec_ctrl->lock, flags);
			return ret;
		}
		cec_ctrl->cec_enabled = false;
		spin_unlock_irqrestore(&cec_ctrl->lock, flags);

		hdmi_cec_disable(cec_ctrl);
		return ret;
	} else {
		spin_lock_irqsave(&cec_ctrl->lock, flags);
		if (!cec_ctrl->cec_engine_configed) {
			DEV_DBG("%s: CEC will be enabled on mirroring\n",
				__func__);
			cec_ctrl->cec_enabled = true;
			spin_unlock_irqrestore(&cec_ctrl->lock, flags);
			return ret;
		}
		cec_ctrl->cec_enabled = true;
		spin_unlock_irqrestore(&cec_ctrl->lock, flags);

		hdmi_cec_enable(cec_ctrl);

		return ret;
	}
} /* hdmi_wta_cec_enable */

static ssize_t hdmi_rda_cec_enable_compliance(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	unsigned long flags;
	ssize_t ret;
	struct hdmi_cec_ctrl *cec_ctrl =
		hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_CEC);

	if (!cec_ctrl) {
		DEV_ERR("%s: Invalid cec_ctrl\n", __func__);
		return -EPERM;
	}

	spin_lock_irqsave(&cec_ctrl->lock, flags);
	ret = snprintf(buf, PAGE_SIZE, "%d\n",
		cec_ctrl->compliance_response_enabled);

	cec_ctrl->cec_logical_addr = 0x4;
	hdmi_cec_write_logical_addr(cec_ctrl, cec_ctrl->cec_logical_addr);

	spin_unlock_irqrestore(&cec_ctrl->lock, flags);

	return ret;
} /* hdmi_rda_cec_enable_compliance */

static ssize_t hdmi_wta_cec_enable_compliance(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int val;
	unsigned long flags;
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct hdmi_cec_ctrl *cec_ctrl =
		hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_CEC);

	if (!cec_ctrl) {
		DEV_ERR("%s: Invalid cec_ctrl\n", __func__);
		return -EPERM;
	}

	spin_lock_irqsave(&cec_ctrl->lock, flags);
	if (cec_ctrl->cec_enabled && cec_ctrl->cec_engine_configed) {
		spin_unlock_irqrestore(&cec_ctrl->lock, flags);
		DEV_ERR("%s: Cannot en/dis compliance when CEC session is on\n",
			__func__);
		return -EPERM;
	} else {
		if (kstrtoint(buf, 10, &val)) {
			DEV_ERR("%s: kstrtoint failed.\n", __func__);
			return -EPERM;
		}
		cec_ctrl->compliance_response_enabled =
			(val == 1) ? true : false;
	}
	spin_unlock_irqrestore(&cec_ctrl->lock, flags);

	return ret;
} /* hdmi_wta_cec_enable_compliance */

static ssize_t hdmi_rda_cec_logical_addr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	unsigned long flags;
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct hdmi_cec_ctrl *cec_ctrl =
		hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_CEC);

	if (!cec_ctrl) {
		DEV_ERR("%s: Invalid cec_ctrl\n", __func__);
		return -EPERM;
	}

	spin_lock_irqsave(&cec_ctrl->lock, flags);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", cec_ctrl->cec_logical_addr);
	spin_unlock_irqrestore(&cec_ctrl->lock, flags);

	return ret;
} /* hdmi_rda_cec_logical_addr */

static ssize_t hdmi_wta_cec_logical_addr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int logical_addr;
	unsigned long flags;
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct hdmi_cec_ctrl *cec_ctrl =
	 hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_CEC);

	if (!cec_ctrl) {
		DEV_ERR("%s: Invalid cec_ctrl\n", __func__);
		return -EPERM;
	}

	if (kstrtoint(buf, 10, &logical_addr)) {
		DEV_ERR("%s: kstrtoint failed\n", __func__);
		return -EPERM;
	}

	if (logical_addr < 0 || logical_addr > 15) {
		DEV_ERR("%s: Invalid logical address\n", __func__);
		return -EINVAL;
	}

	spin_lock_irqsave(&cec_ctrl->lock, flags);
	cec_ctrl->cec_logical_addr = (u8)logical_addr;
	if (cec_ctrl->cec_enabled && cec_ctrl->cec_engine_configed)
		hdmi_cec_write_logical_addr(cec_ctrl,
			cec_ctrl->cec_logical_addr);
	spin_unlock_irqrestore(&cec_ctrl->lock, flags);

	return ret;
} /* hdmi_wta_cec_logical_addr */

static ssize_t hdmi_rda_cec_msg(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i = 0;
	unsigned long flags;
	struct hdmi_cec_msg_node *msg_node, *tmp;
	struct hdmi_cec_ctrl *cec_ctrl =
		hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_CEC);

	if (!cec_ctrl) {
		DEV_ERR("%s: Invalid cec_ctrl\n", __func__);
		return -EPERM;
	}

	spin_lock_irqsave(&cec_ctrl->lock, flags);

	if (cec_ctrl->compliance_response_enabled) {
		spin_unlock_irqrestore(&cec_ctrl->lock, flags);
		DEV_ERR("%s: Read is disabled coz compliance response is on\n",
			__func__);
		return -EPERM;
	}

	if (list_empty_careful(&cec_ctrl->msg_head)) {
		spin_unlock_irqrestore(&cec_ctrl->lock, flags);
		DEV_ERR("%s: CEC message queue is empty\n", __func__);
		return -EPERM;
	}

	list_for_each_entry_safe(msg_node, tmp, &cec_ctrl->msg_head, list) {
		if ((i+1) * sizeof(struct hdmi_cec_msg) > PAGE_SIZE) {
			DEV_DBG("%s: Overflowing PAGE_SIZE.\n", __func__);
			break;
		}

		memcpy(buf + (i * sizeof(struct hdmi_cec_msg)), &msg_node->msg,
			sizeof(struct hdmi_cec_msg));
		list_del(&msg_node->list);
		kfree(msg_node);
		i++;
	}

	spin_unlock_irqrestore(&cec_ctrl->lock, flags);

	return i * sizeof(struct hdmi_cec_msg);
} /* hdmi_rda_cec_msg */

static ssize_t hdmi_wta_cec_msg(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int rc;
	unsigned long flags;
	struct hdmi_cec_msg *msg = (struct hdmi_cec_msg *)buf;
	struct hdmi_cec_ctrl *cec_ctrl =
		hdmi_get_featuredata_from_sysfs_dev(dev, HDMI_TX_FEAT_CEC);

	if (!cec_ctrl) {
		DEV_ERR("%s: Invalid cec_ctrl\n", __func__);
		return -EPERM;
	}

	spin_lock_irqsave(&cec_ctrl->lock, flags);
	if (cec_ctrl->compliance_response_enabled) {
		spin_unlock_irqrestore(&cec_ctrl->lock, flags);
		DEV_ERR("%s: Write disabled coz compliance response is on.\n",
			__func__);
		return -EPERM;
	}
	spin_unlock_irqrestore(&cec_ctrl->lock, flags);

	if (msg->frame_size > MAX_OPERAND_SIZE) {
		DEV_ERR("%s: msg frame too big!\n", __func__);
		return -EINVAL;
	}
	rc = hdmi_cec_msg_send(cec_ctrl, msg);
	if (rc) {
		DEV_ERR("%s: hdmi_cec_msg_send failed\n", __func__);
		return rc;
	} else {
		return sizeof(struct hdmi_cec_msg);
	}
} /* hdmi_wta_cec_msg */

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR, hdmi_rda_cec_enable,
	hdmi_wta_cec_enable);
static DEVICE_ATTR(enable_compliance, S_IRUGO | S_IWUSR,
	hdmi_rda_cec_enable_compliance, hdmi_wta_cec_enable_compliance);
static DEVICE_ATTR(logical_addr, S_IRUSR | S_IWUSR,
	hdmi_rda_cec_logical_addr, hdmi_wta_cec_logical_addr);
static DEVICE_ATTR(rd_msg, S_IRUGO, hdmi_rda_cec_msg,	NULL);
static DEVICE_ATTR(wr_msg, S_IWUSR, NULL, hdmi_wta_cec_msg);

static struct attribute *hdmi_cec_fs_attrs[] = {
	&dev_attr_enable.attr,
	&dev_attr_enable_compliance.attr,
	&dev_attr_logical_addr.attr,
	&dev_attr_rd_msg.attr,
	&dev_attr_wr_msg.attr,
	NULL,
};

static struct attribute_group hdmi_cec_fs_attr_group = {
	.name = "cec",
	.attrs = hdmi_cec_fs_attrs,
};

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
	DEV_DBG("%s: cec interrupt status is [0x%x]\n", __func__, cec_intr);

	if (!cec_ctrl->cec_enabled) {
		DEV_ERR("%s: cec is not enabled. Just clear int and return.\n",
			__func__);
		DSS_REG_W(io, HDMI_CEC_INT, cec_intr);
		return 0;
	}

	cec_status = DSS_REG_R_ND(io, HDMI_CEC_STATUS);
	DEV_DBG("%s: cec status is [0x%x]\n", __func__, cec_status);

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
} /* hdmi_cec_isr */

int hdmi_cec_deconfig(void *input)
{
	unsigned long flags;
	struct hdmi_cec_ctrl *cec_ctrl = (struct hdmi_cec_ctrl *)input;

	if (!cec_ctrl) {
		DEV_ERR("%s: Invalid input\n", __func__);
		return -EPERM;
	}

	hdmi_cec_disable(cec_ctrl);

	spin_lock_irqsave(&cec_ctrl->lock, flags);
	cec_ctrl->cec_engine_configed = false;
	spin_unlock_irqrestore(&cec_ctrl->lock, flags);

	return 0;
} /* hdmi_cec_deconfig */

int hdmi_cec_config(void *input)
{
	unsigned long flags;
	u32 hdmi_hw_version;
	struct dss_io_data *io = NULL;
	struct hdmi_cec_ctrl *cec_ctrl = (struct hdmi_cec_ctrl *)input;

	if (!cec_ctrl || !cec_ctrl->init_data.io) {
		DEV_ERR("%s: Invalid input\n", __func__);
		return -EPERM;
	}

	io = cec_ctrl->init_data.io;

	/* 19.2Mhz * 0.00005 us = 950 = 0x3B6 */
	DSS_REG_W(io, HDMI_CEC_REFTIMER, (0x3B6 & 0xFFF) | BIT(16));

	hdmi_hw_version = DSS_REG_R(io, HDMI_VERSION);
	if (hdmi_hw_version == 0x30000001) {
		DSS_REG_W(io, HDMI_CEC_RD_RANGE, 0x30AB9888);
		DSS_REG_W(io, HDMI_CEC_WR_RANGE, 0x888AA888);

		DSS_REG_W(io, HDMI_CEC_RD_START_RANGE, 0x88888888);
		DSS_REG_W(io, HDMI_CEC_RD_TOTAL_RANGE, 0x99);
		DSS_REG_W(io, HDMI_CEC_COMPL_CTL, 0xF);
		DSS_REG_W(io, HDMI_CEC_WR_CHECK_CONFIG, 0x4);
	} else {
		DEV_INFO("%s: CEC is not supported on %d HDMI HW version.\n",
			__func__, hdmi_hw_version);
		return -EPERM;
	}

	DSS_REG_W(io, HDMI_CEC_RD_FILTER, BIT(0) | (0x7FF << 4));
	DSS_REG_W(io, HDMI_CEC_TIME, BIT(0) | ((7 * 0x30) << 7));

	if (cec_ctrl->cec_enabled)
		hdmi_cec_enable(cec_ctrl);

	spin_lock_irqsave(&cec_ctrl->lock, flags);
	cec_ctrl->cec_engine_configed = true;
	spin_unlock_irqrestore(&cec_ctrl->lock, flags);

	return 0;
} /* hdmi_cec_config */

void hdmi_cec_deinit(void *input)
{
	struct hdmi_cec_msg_node *msg_node, *tmp;
	struct hdmi_cec_ctrl *cec_ctrl = (struct hdmi_cec_ctrl *)input;

	if (cec_ctrl) {
		list_for_each_entry_safe(msg_node, tmp, &cec_ctrl->msg_head,
					list) {
			list_del(&msg_node->list);
			kfree(msg_node);
		}

		sysfs_remove_group(cec_ctrl->init_data.sysfs_kobj,
			&hdmi_cec_fs_attr_group);

		kfree(cec_ctrl);
	}
} /* hdmi_cec_deinit */

void *hdmi_cec_init(struct hdmi_cec_init_data *init_data)
{
	struct hdmi_cec_ctrl *cec_ctrl = NULL;

	if (!init_data) {
		DEV_ERR("%s: Invalid input\n", __func__);
		goto error;
	}

	cec_ctrl = kzalloc(sizeof(*cec_ctrl), GFP_KERNEL);
	if (!cec_ctrl) {
		DEV_ERR("%s: FAILED: out of memory\n", __func__);
		goto error;
	}

	cec_ctrl->init_data = *init_data;

	if (sysfs_create_group(init_data->sysfs_kobj,
				&hdmi_cec_fs_attr_group)) {
		DEV_ERR("%s: cec sysfs group creation failed\n", __func__);
		goto error;
	}

	spin_lock_init(&cec_ctrl->lock);
	INIT_LIST_HEAD(&cec_ctrl->msg_head);
	INIT_WORK(&cec_ctrl->cec_read_work, hdmi_cec_msg_recv);
	init_completion(&cec_ctrl->cec_msg_wr_done);

	goto exit;

error:
	kfree(cec_ctrl);
	cec_ctrl = NULL;
exit:
	return (void *)cec_ctrl;
} /* hdmi_cec_init */
