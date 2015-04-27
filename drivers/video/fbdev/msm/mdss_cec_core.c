/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/io.h>
#include <linux/list.h>
#include <linux/types.h>
#include <linux/stat.h>
#include <linux/slab.h>
#include <linux/device.h>

#include "mdss_fb.h"
#include "mdss_cec_core.h"

#define CEC_ENABLE_MASK		BIT(0)
#define CEC_WAKEUP_ENABLE_MASK	BIT(1)

struct cec_msg_node {
	struct cec_msg msg;
	struct list_head list;
};

struct cec_ctl {
	bool enabled;
	bool compliance_enabled;
	bool cec_wakeup_en;

	u8 logical_addr;

	spinlock_t lock;
	struct list_head msg_head;
	struct cec_abstract_init_data init_data;

};

static struct cec_ctl *cec_get_ctl(struct device *dev)
{
	struct fb_info *fbi;
	struct msm_fb_data_type *mfd;
	struct mdss_panel_info *pinfo;

	if (!dev) {
		pr_err("invalid input\n");
		goto error;
	}

	fbi = dev_get_drvdata(dev);
	if (!fbi) {
		pr_err("invalid fbi\n");
		goto error;
	}

	mfd = fbi->par;
	if (!mfd) {
		pr_err("invalid mfd\n");
		goto error;
	}

	pinfo = mfd->panel_info;
	if (!pinfo) {
		pr_err("invalid pinfo\n");
		goto error;
	}

	return pinfo->cec_data;

error:
	return NULL;
}

static int cec_msg_send(struct cec_ctl *ctl, struct cec_msg *msg)
{
	int ret = -EINVAL;
	struct cec_ops *ops;

	if (!ctl || !msg) {
		pr_err("invalid input\n");
		goto end;
	}

	ops = ctl->init_data.ops;

	if (ops && ops->send_msg)
		ret = ops->send_msg(ops->data, msg);
end:
	return ret;
}

static void cec_dump_msg(struct cec_ctl *ctl, struct cec_msg *msg)
{
	int i;
	unsigned long flags;

	if (!ctl || !msg) {
		pr_err("invalid input\n");
		return;
	}

	spin_lock_irqsave(&ctl->lock, flags);
	pr_debug("==%pS dump start ==\n",
		__builtin_return_address(0));

	pr_debug("cec: sender_id: %d\n", msg->sender_id);
	pr_debug("cec: recvr_id:  %d\n", msg->recvr_id);

	if (msg->frame_size < 2) {
		pr_debug("cec: polling message\n");
		spin_unlock_irqrestore(&ctl->lock, flags);
		return;
	}

	pr_debug("cec: opcode: %02x\n", msg->opcode);
	for (i = 0; i < msg->frame_size - 2; i++)
		pr_debug("cec: operand(%2d) : %02x\n", i + 1, msg->operand[i]);

	pr_debug("==%pS dump end ==\n",
		__builtin_return_address(0));
	spin_unlock_irqrestore(&ctl->lock, flags);
}

static int cec_disable(struct cec_ctl *ctl)
{
	unsigned long flags;
	int ret = -EINVAL;
	struct cec_msg_node *msg_node, *tmp;
	struct cec_ops *ops;

	if (!ctl) {
		pr_err("Invalid input\n");
		goto end;
	}

	spin_lock_irqsave(&ctl->lock, flags);
	list_for_each_entry_safe(msg_node, tmp, &ctl->msg_head, list) {
		list_del(&msg_node->list);
		kfree(msg_node);
	}
	spin_unlock_irqrestore(&ctl->lock, flags);

	ops = ctl->init_data.ops;

	if (ops && ops->enable)
		ret = ops->enable(ops->data, false);

	if (!ret)
		ctl->enabled = false;

end:
	return ret;
}

static int cec_enable(struct cec_ctl *ctl)
{
	int ret = -EINVAL;
	struct cec_ops *ops;

	if (!ctl) {
		pr_err("Invalid input\n");
		goto end;
	}

	INIT_LIST_HEAD(&ctl->msg_head);

	ops = ctl->init_data.ops;

	if (ops && ops->enable)
		ret = ops->enable(ops->data, true);

	if (!ret)
		ctl->enabled = true;

end:
	return ret;
}

static int cec_send_abort_opcode(struct cec_ctl *ctl,
	struct cec_msg *in_msg, u8 reason_operand)
{
	int i = 0;
	struct cec_msg out_msg;

	if (!ctl || !in_msg) {
		pr_err("Invalid input\n");
		return -EINVAL;
	}

	out_msg.sender_id = 0x4;
	out_msg.recvr_id = in_msg->sender_id;
	out_msg.opcode = 0x0; /* opcode for feature abort */
	out_msg.operand[i++] = in_msg->opcode;
	out_msg.operand[i++] = reason_operand;
	out_msg.frame_size = i + 2;

	return cec_msg_send(ctl, &out_msg);
}

static int cec_msg_parser(struct cec_ctl *ctl, struct cec_msg *in_msg)
{
	int rc = 0, i = 0;
	struct cec_msg out_msg;

	if (!ctl || !in_msg) {
		pr_err("Invalid input\n");
		rc = -EINVAL;
		goto end;
	}

	pr_debug("in_msg->opcode = 0x%x\n", in_msg->opcode);
	switch (in_msg->opcode) {
	case CEC_MSG_SET_OSD_STRING:
		/* Set OSD String */
		pr_debug("Recvd OSD Str=[0x%x]\n",
			in_msg->operand[3]);
		break;
	case CEC_MSG_GIVE_PHYS_ADDR:
		/* Give Phy Addr */
		pr_debug("Recvd a Give Phy Addr cmd\n");

		out_msg.sender_id = 0x4;
		/* Broadcast */
		out_msg.recvr_id = 0xF;
		out_msg.opcode = 0x84;
		out_msg.operand[i++] = 0x10;
		out_msg.operand[i++] = 0x0;
		out_msg.operand[i++] = 0x04;
		out_msg.frame_size = i + 2;

		rc = cec_msg_send(ctl, &out_msg);
		break;
	case CEC_MSG_ABORT:
		/* Abort */
		pr_debug("Recvd an abort cmd.\n");

		/* reason = "Refused" */
		rc = cec_send_abort_opcode(ctl, in_msg, 0x04);
		break;
	case CEC_MSG_GIVE_OSD_NAME:
		/* Give OSD name */
		pr_debug("Recvd 'Give OSD name' cmd.\n");

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

		rc = cec_msg_send(ctl, &out_msg);
		break;
	case CEC_MSG_GIVE_POWER_STATUS:
		/* Give Device Power status */
		pr_debug("Recvd a Power status message\n");

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

		rc = cec_msg_send(ctl, &out_msg);
		break;
	case CEC_MSG_ROUTE_CHANGE_CMD:
		/* Routing Change cmd */
	case CEC_MSG_SET_STREAM_PATH:
		/* Set Stream Path */
		pr_debug("Recvd Set Stream or Routing Change cmd\n");

		out_msg.sender_id = 0x4;
		out_msg.recvr_id = 0xF; /* broadcast this message */
		out_msg.opcode = 0x82; /* Active Source */
		out_msg.operand[i++] = 0x10;
		out_msg.operand[i++] = 0x0;
		out_msg.frame_size = i + 2;

		rc = cec_msg_send(ctl, &out_msg);
		if (rc)
			goto end;

		/* sending <Image View On> message */
		memset(&out_msg, 0x0, sizeof(struct cec_msg));
		i = 0;
		out_msg.sender_id = 0x4;
		out_msg.recvr_id = in_msg->sender_id;
		out_msg.opcode = 0x04; /* opcode for Image View On */
		out_msg.frame_size = i + 2;

		rc = cec_msg_send(ctl, &out_msg);
		break;
	case CEC_MSG_USER_CTRL_PRESS:
		/* User Control Pressed */
		pr_debug("User Control Pressed\n");
		break;
	case CEC_MSG_USER_CTRL_RELEASE:
		/* User Control Released */
		pr_debug("User Control Released\n");
		break;
	default:
		pr_debug("Recvd an unknown cmd = [%u]\n",
			in_msg->opcode);

		/* reason = "Unrecognized opcode" */
		rc = cec_send_abort_opcode(ctl, in_msg, 0x0);
		break;
	}
end:
	return rc;
}

static int cec_msg_recv(void *data, struct cec_msg *msg)
{
	unsigned long flags;
	struct cec_ctl *ctl = data;
	struct cec_msg_node *msg_node;
	int ret = 0;

	if (!ctl) {
		pr_err("invalid input\n");
		ret = -EINVAL;
		goto end;
	}

	if (!ctl->enabled) {
		pr_err("cec not enabled\n");
		ret = -ENODEV;
		goto end;
	}

	msg_node = kzalloc(sizeof(*msg_node), GFP_KERNEL);
	if (!msg_node) {
		ret = -ENOMEM;
		goto end;
	}

	msg_node->msg = *msg;

	pr_debug("CEC read frame done\n");
	cec_dump_msg(ctl, &msg_node->msg);

	spin_lock_irqsave(&ctl->lock, flags);
	if (ctl->compliance_enabled) {
		spin_unlock_irqrestore(&ctl->lock, flags);

		ret = cec_msg_parser(ctl, &msg_node->msg);
		if (ret)
			pr_err("msg parsing failed\n");

		kfree(msg_node);
	} else {
		list_add_tail(&msg_node->list, &ctl->msg_head);
		spin_unlock_irqrestore(&ctl->lock, flags);

		/* wake-up sysfs read_msg context */
		sysfs_notify(ctl->init_data.kobj, "cec", "rd_msg");
	}
end:
	return ret;
}

static ssize_t cec_rda_enable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	unsigned long flags;
	struct cec_ctl *ctl = cec_get_ctl(dev);

	if (!ctl) {
		pr_err("Invalid input\n");
		ret = -EINVAL;
		goto end;
	}

	spin_lock_irqsave(&ctl->lock, flags);
	if (ctl->enabled) {
		pr_debug("cec is enabled\n");
		ret = snprintf(buf, PAGE_SIZE, "%d\n", 1);
	} else {
		pr_err("cec is disabled\n");
		ret = snprintf(buf, PAGE_SIZE, "%d\n", 0);
	}
	spin_unlock_irqrestore(&ctl->lock, flags);
end:
	return ret;
}

static ssize_t cec_wta_enable(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int val;
	bool cec_en;
	ssize_t ret;
	struct cec_ctl *ctl = cec_get_ctl(dev);
	struct cec_ops *ops;

	if (!ctl) {
		pr_err("Invalid input\n");
		ret = -EINVAL;
		goto end;
	}

	ops = ctl->init_data.ops;

	ret = kstrtoint(buf, 10, &val);
	if (ret) {
		pr_err("kstrtoint failed.\n");
		goto end;
	}

	cec_en = (val & CEC_ENABLE_MASK) ? true : false;

	/* bit 1 is used for wakeup feature */
	if ((val & CEC_ENABLE_MASK) && (val & CEC_WAKEUP_ENABLE_MASK))
		ctl->cec_wakeup_en = true;
	else
		ctl->cec_wakeup_en = false;

	if (ops && ops->wakeup_en)
		ret = ops->wakeup_en(ops->data, ctl->cec_wakeup_en);

	if (ret)
		goto end;

	if (ctl->enabled == cec_en) {
		pr_debug("cec is already %s\n",
			cec_en ? "enabled" : "disabled");
		goto bail;
	}

	if (cec_en)
		ret = cec_enable(ctl);
	else
		ret = cec_disable(ctl);

	if (ret)
		goto end;

bail:
	ret = strnlen(buf, PAGE_SIZE);
end:
	return ret;
}

static ssize_t cec_rda_enable_compliance(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	unsigned long flags;
	ssize_t ret;
	struct cec_ctl *ctl = cec_get_ctl(dev);

	if (!ctl) {
		pr_err("Invalid ctl\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&ctl->lock, flags);
	ret = snprintf(buf, PAGE_SIZE, "%d\n",
		ctl->compliance_enabled);

	spin_unlock_irqrestore(&ctl->lock, flags);

	return ret;
}

static ssize_t cec_wta_enable_compliance(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int val;
	ssize_t ret;
	struct cec_ctl *ctl = cec_get_ctl(dev);
	struct cec_ops *ops;

	if (!ctl) {
		pr_err("Invalid ctl\n");
		ret = -EINVAL;
		goto end;
	}

	ops = ctl->init_data.ops;

	ret = kstrtoint(buf, 10, &val);
	if (ret) {
		pr_err("kstrtoint failed.\n");
		goto end;
	}

	ctl->compliance_enabled = (val == 1) ? true : false;

	if (ctl->compliance_enabled) {
		ret = cec_enable(ctl);
		if (ret)
			goto end;

		ctl->logical_addr = 0x4;

		if (ops && ops->wt_logical_addr)
			ops->wt_logical_addr(ops->data, ctl->logical_addr);

	} else {
		ctl->logical_addr = 0;

		ret = cec_disable(ctl);
		if (ret)
			goto end;
	}

	ret = strnlen(buf, PAGE_SIZE);
end:
	return ret;
}

static ssize_t cec_rda_logical_addr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	unsigned long flags;
	ssize_t ret;
	struct cec_ctl *ctl = cec_get_ctl(dev);

	if (!ctl) {
		pr_err("Invalid ctl\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&ctl->lock, flags);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", ctl->logical_addr);
	spin_unlock_irqrestore(&ctl->lock, flags);

	return ret;
}

static ssize_t cec_wta_logical_addr(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int logical_addr;
	unsigned long flags;
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	struct cec_ctl *ctl = cec_get_ctl(dev);
	struct cec_ops *ops;

	if (!ctl) {
		pr_err("Invalid ctl\n");
		ret = -EINVAL;
		goto end;
	}

	ops = ctl->init_data.ops;

	ret = kstrtoint(buf, 10, &logical_addr);
	if (ret) {
		pr_err("kstrtoint failed\n");
		goto end;
	}

	if (logical_addr < 0 || logical_addr > 15) {
		pr_err("Invalid logical address\n");
		ret = -EINVAL;
		goto end;
	}

	spin_lock_irqsave(&ctl->lock, flags);
	ctl->logical_addr = (u8)logical_addr;
	if (ctl->enabled) {
		if (ops && ops->wt_logical_addr)
			ops->wt_logical_addr(ops->data, ctl->logical_addr);
	}
	spin_unlock_irqrestore(&ctl->lock, flags);
end:
	return ret;
}

static ssize_t cec_rda_msg(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i = 0;
	unsigned long flags;
	struct cec_msg_node *msg_node, *tmp;
	struct cec_ctl *ctl = cec_get_ctl(dev);
	ssize_t ret;

	if (!ctl) {
		pr_err("Invalid ctl\n");
		ret = -EINVAL;
		goto end;
	}

	if (!ctl->enabled) {
		pr_err("cec not enabled\n");
		ret = -EINVAL;
		goto end;
	}

	spin_lock_irqsave(&ctl->lock, flags);

	if (ctl->compliance_enabled) {
		spin_unlock_irqrestore(&ctl->lock, flags);
		pr_err("Read no allowed in compliance mode\n");
		ret = -EPERM;
		goto end;
	}

	if (list_empty_careful(&ctl->msg_head)) {
		spin_unlock_irqrestore(&ctl->lock, flags);
		pr_err("CEC message queue is empty\n");
		ret = -EINVAL;
		goto end;
	}

	list_for_each_entry_safe(msg_node, tmp, &ctl->msg_head, list) {
		if ((i + 1) * sizeof(struct cec_msg) > PAGE_SIZE) {
			pr_debug("Overflowing PAGE_SIZE.\n");
			break;
		}

		memcpy(buf + (i * sizeof(struct cec_msg)), &msg_node->msg,
			sizeof(struct cec_msg));
		list_del(&msg_node->list);
		kfree(msg_node);
		i++;
	}

	spin_unlock_irqrestore(&ctl->lock, flags);

	ret = i * sizeof(struct cec_msg);
end:
	return ret;
}

static ssize_t cec_wta_msg(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret;
	unsigned long flags;
	struct cec_msg *msg = (struct cec_msg *)buf;
	struct cec_ctl *ctl = cec_get_ctl(dev);

	if (!ctl) {
		pr_err("Invalid ctl\n");
		ret = -EINVAL;
		goto end;
	}

	spin_lock_irqsave(&ctl->lock, flags);
	if (ctl->compliance_enabled) {
		spin_unlock_irqrestore(&ctl->lock, flags);
		pr_err("Write not allowed in compliance mode\n");
		ret = -EPERM;
		goto end;
	}

	if (!ctl->enabled) {
		spin_unlock_irqrestore(&ctl->lock, flags);
		pr_err("CEC is not configed.\n");
		ret = -EPERM;
		goto end;
	}
	spin_unlock_irqrestore(&ctl->lock, flags);

	if (msg->frame_size > MAX_OPERAND_SIZE) {
		pr_err("msg frame too big!\n");
		ret = -EINVAL;
		goto end;
	}
	ret = cec_msg_send(ctl, msg);
	if (ret) {
		pr_err("cec_msg_send failed\n");
		goto end;
	}

	ret = sizeof(struct cec_msg);
end:
	return ret;
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR, cec_rda_enable,
	cec_wta_enable);
static DEVICE_ATTR(enable_compliance, S_IRUGO | S_IWUSR,
	cec_rda_enable_compliance, cec_wta_enable_compliance);
static DEVICE_ATTR(logical_addr, S_IRUSR | S_IWUSR,
	cec_rda_logical_addr, cec_wta_logical_addr);
static DEVICE_ATTR(rd_msg, S_IRUGO, cec_rda_msg, NULL);
static DEVICE_ATTR(wr_msg, S_IWUSR | S_IRUSR, NULL, cec_wta_msg);

static struct attribute *cec_fs_attrs[] = {
	&dev_attr_enable.attr,
	&dev_attr_enable_compliance.attr,
	&dev_attr_logical_addr.attr,
	&dev_attr_rd_msg.attr,
	&dev_attr_wr_msg.attr,
	NULL,
};

static struct attribute_group cec_fs_attr_group = {
	.name = "cec",
	.attrs = cec_fs_attrs,
};

/**
 * cec_abstract_deinit() - Release CEC abstract module
 * @input: CEC abstract data
 *
 * This API release all the resources allocated for this
 * module.
 *
 * Return: 0 on success otherwise error code.
 */
int cec_abstract_deinit(void *input)
{
	struct cec_ctl *ctl = (struct cec_ctl *)input;

	if (!ctl)
		return -EINVAL;

	sysfs_remove_group(ctl->init_data.kobj, &cec_fs_attr_group);

	kfree(ctl);

	return 0;
}

/**
 * cec_abstract_init() - Initialize CEC abstract module
 * @init_data: data needed to initialize the CEC abstraction module
 *
 * This API will initialize the CEC abstract module which connects
 * CEC client with CEC hardware. It creates sysfs nodes for client
 * to read and write CEC messages. It interacts with hardware with
 * provided operation function pointers. Also provides callback
 * function pointers to let the hardware inform about incoming
 * CEC message.
 *
 * Return: pinter to cec abstract data which needs to be passed
 * as parameter with callback functions.
*/
void *cec_abstract_init(struct cec_abstract_init_data *init_data)
{
	struct cec_ctl *ctl = NULL;
	int ret = 0;

	if (!init_data) {
		pr_err("invalid input\n");
		ret = -EINVAL;
		goto end;
	}

	ctl = kzalloc(sizeof(*ctl), GFP_KERNEL);
	if (!ctl) {
		ret = -ENOMEM;
		goto end;
	}

	/* keep a copy of init data */
	ctl->init_data = *init_data;

	ret = sysfs_create_group(ctl->init_data.kobj, &cec_fs_attr_group);
	if (ret) {
		pr_err("cec sysfs group creation failed\n");
		goto end;
	}

	spin_lock_init(&ctl->lock);

	/* provide callback function pointers */
	if (init_data->cbs) {
		init_data->cbs->msg_recv_notify = cec_msg_recv;
		init_data->cbs->data = ctl;
	}

	return ctl;
end:
	kfree(ctl);
	return ERR_PTR(ret);
}

