/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/poll.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include "ccci_config.h"

#ifdef FEATURE_RF_CLK_BUF
#include <mtk_clkbuf_ctl.h>
#endif

#include "ccci_core.h"
#include "ccci_bm.h"
#include "ccci_modem.h"
#include "port_misc.h"

#define MAX_QUEUE_LENGTH 16

struct ccci_misc_cb_func_info ccci_misc_cb_table[MAX_MD_NUM][ID_USER_MAX];

int register_ccci_func_call_back(int md_id, unsigned int id,
	ccci_misc_cb_func_t func)
{
	int ret = 0;
	struct ccci_misc_cb_func_info *info_ptr;

	if (md_id >= MAX_MD_NUM) {
		CCCI_ERROR_LOG(md_id, MISC,
			"register_sys_call_back fail: invalid md id\n");
		return -EINVAL;
	}

	info_ptr = &(ccci_misc_cb_table[md_id][id]);

	if (info_ptr->func == NULL) {
		info_ptr->id = id;
		info_ptr->func = func;
	} else {
		CCCI_ERROR_LOG(md_id, MISC,
			"register_ccci_misc_call_back fail: func(0x%x) registered! %ps\n",
			id, info_ptr->func);
	}

	return ret;
}

void exec_ccci_misc_call_back(int md_id, int cb_id, void *data, int data_len)
{
	ccci_misc_cb_func_t func;
	int id;
	struct ccci_misc_cb_func_info *curr_table;

	if (md_id >= MAX_MD_NUM) {
		CCCI_ERROR_LOG(md_id, MISC,
			"exec_ccci_misc_cb fail: invalid md id\n");
		return;
	}

	id = cb_id & 0xFF;
	if (id >= ID_USER_MAX) {
		CCCI_ERROR_LOG(md_id, MISC,
			"exec_sys_cb fail: invalid func id(0x%x)\n", cb_id);
		return;
	}

	curr_table = ccci_misc_cb_table[md_id];

	func = curr_table[id].func;
	if (func != NULL)
		func(md_id, data, data_len);
	else
		CCCI_ERROR_LOG(md_id, MISC,
			"exec_ccci_misc_cb fail: func id(0x%x) not register!\n",
			cb_id);
}

/*
 * define character device operation for misc_u
 */
static const struct file_operations ccci_misc_dev_fops = {
	.owner = THIS_MODULE,
	.open = &port_dev_open, /*use default API*/
	.read = &port_dev_read, /*use default API*/
	.write = &port_dev_write, /*use default API*/
	.release = &port_dev_close,/*use default API*/
};

static int port_misc_kernel_thread(void *arg)
{
	struct port_t *port = arg;
	struct sk_buff *skb;
	struct ccci_header *ccci_h;
	unsigned long flags;
	int ret = 0;
	/* struct rmmi_camera_arfcn_info_struct *recv_msg; */

	CCCI_DEBUG_LOG(port->md_id, MISC,
		"port %s's thread running\n", port->name);

	while (1) {
retry:
		if (skb_queue_empty(&port->rx_skb_list)) {
			ret = wait_event_interruptible(port->rx_wq,
				!skb_queue_empty(&port->rx_skb_list));
			if (ret == -ERESTARTSYS)
				continue;	/* FIXME */
		}
		if (kthread_should_stop())
			break;
		CCCI_DEBUG_LOG(port->md_id, MISC,
			"read on %s\n", port->name);
		/* 1. dequeue */
		spin_lock_irqsave(&port->rx_skb_list.lock, flags);
		skb = __skb_dequeue(&port->rx_skb_list);
		if (port->rx_skb_list.qlen == 0)
			port_ask_more_req_to_md(port);
		spin_unlock_irqrestore(&port->rx_skb_list.lock, flags);
		if (skb == NULL)
			goto retry;
		/* 2. process the request */
		/* ccci header */
		ccci_h = (struct ccci_header *)skb->data;
		skb_pull(skb, sizeof(struct ccci_header));

		switch (ccci_h->channel) {
		case CCCI_MIPI_CHANNEL_RX:
			exec_ccci_misc_call_back(port->md_id,
				ID_MD_CAMERA, skb->data, skb->len);
			break;
		default:
			CCCI_ERROR_LOG(port->md_id, MISC,
				"recv unknown channel %d\n",
				ccci_h->channel);
			break;
		}
		CCCI_DEBUG_LOG(port->md_id, MISC,
			"read done on %s\n",
			port->name);
		ccci_free_skb(skb);
	}
	return 0;
}

static int port_misc_init(struct port_t *port)
{
	struct cdev *dev;
	int ret = 0;

	CCCI_DEBUG_LOG(port->md_id, MISC,
		"ccci misc port %s is initializing\n", port->name);
	port->rx_length_th = MAX_QUEUE_LENGTH;
	port->skb_from_pool = 1;
	port->interception = 0;
	if (port->flags & PORT_F_WITH_CHAR_NODE) {
		dev = kmalloc(sizeof(struct cdev), GFP_KERNEL);
		if (unlikely(!dev)) {
			CCCI_ERROR_LOG(port->md_id, MISC,
				"alloc misc char dev fail!!\n");
			return -1;
		}
		cdev_init(dev, &ccci_misc_dev_fops);
		dev->owner = THIS_MODULE;
		ret = cdev_add(dev, MKDEV(port->major,
			port->minor_base + port->minor), 1);
		ret = ccci_register_dev_node(port->name, port->major,
			port->minor_base + port->minor);
		port->flags |= PORT_F_ADJUST_HEADER;
	} else {
		kthread_run(port_misc_kernel_thread, port, "%s", port->name);
	}

	return 0;
}

struct port_ops ccci_misc_port_ops = {
	.init = &port_misc_init,
	.recv_skb = &port_recv_skb,
};

