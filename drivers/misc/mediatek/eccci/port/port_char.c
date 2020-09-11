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
#include "ccci_config.h"
#include "ccci_bm.h"
#include "port_proxy.h"
#include "port_char.h"
#include "port_smem.h"
#include "ccci_fsm.h"
#include "ccci_ipc_task_ID.h"

#define MAX_QUEUE_LENGTH 32

unsigned int port_char_dev_poll(struct file *fp,
	struct poll_table_struct *poll)
{
	struct port_t *port = fp->private_data;
	unsigned int mask = 0;
	int md_id = port->md_id;
	int md_state = ccci_fsm_get_md_state(md_id);

	CCCI_DEBUG_LOG(md_id, CHAR, "poll on %s\n", port->name);
	poll_wait(fp, &port->rx_wq, poll);
	/* TODO: lack of poll wait for Tx */
	if (!skb_queue_empty(&port->rx_skb_list))
		mask |= POLLIN | POLLRDNORM;
	if (port_write_room_to_md(port) > 0)
		mask |= POLLOUT | POLLWRNORM;
	if (port->rx_ch == CCCI_UART1_RX &&
	    md_state != READY &&
		md_state != EXCEPTION) {
		/* notify MD logger to save its log
		 * before md_init kills it
		 */
		mask |= POLLERR;
		CCCI_NORMAL_LOG(md_id, CHAR,
			"poll error for MD logger at state %d,mask=%d\n",
			md_state, mask);
	}

	return mask;
}

static const struct file_operations char_dev_fops = {
	.owner = THIS_MODULE,
	.open = &port_dev_open, /*use default API*/
	.read = &port_dev_read, /*use default API*/
	.write = &port_dev_write, /*use default API*/
	.release = &port_dev_close,/*use default API*/
	.unlocked_ioctl = &port_dev_ioctl,/*use default API*/
#ifdef CONFIG_COMPAT
	.compat_ioctl = &port_dev_compat_ioctl,/*use default API*/
#endif
	.poll = &port_char_dev_poll,/*use port char self API*/
	.mmap = &port_dev_mmap,
};
static int port_char_init(struct port_t *port)
{
	struct cdev *dev;
	int ret = 0;
	int md_id = port->md_id;

	CCCI_DEBUG_LOG(md_id, CHAR,
		"char port %s is initializing\n", port->name);
	port->rx_length_th = MAX_QUEUE_LENGTH;
	port->skb_from_pool = 1;
	port->interception = 0;
	if (port->flags & PORT_F_WITH_CHAR_NODE) {
		dev = kmalloc(sizeof(struct cdev), GFP_KERNEL);
		if (unlikely(!dev)) {
			CCCI_ERROR_LOG(port->md_id, CHAR,
				"alloc char dev fail!!\n");
			return -1;
		}
		cdev_init(dev, &char_dev_fops);
		dev->owner = THIS_MODULE;
		ret = cdev_add(dev, MKDEV(port->major,
				port->minor_base + port->minor), 1);
		ret = ccci_register_dev_node(port->name, port->major,
				port->minor_base + port->minor);
		port->flags |= PORT_F_ADJUST_HEADER;
	}
#ifndef DPMAIF_DEBUG_LOG
	if (port->rx_ch == CCCI_UART2_RX ||
		port->rx_ch == CCCI_C2K_AT ||
		port->rx_ch == CCCI_C2K_AT2 ||
		port->rx_ch == CCCI_C2K_AT3 ||
		port->rx_ch == CCCI_C2K_AT4 ||
		port->rx_ch == CCCI_C2K_AT5 ||
		port->rx_ch == CCCI_C2K_AT6 ||
		port->rx_ch == CCCI_C2K_AT7 ||
		port->rx_ch == CCCI_C2K_AT8)
		port->flags |= PORT_F_CH_TRAFFIC;
	else if (port->rx_ch == CCCI_FS_RX)
		port->flags |= (PORT_F_CH_TRAFFIC | PORT_F_DUMP_RAW_DATA);
#endif
	return ret;
}

#ifdef CONFIG_MTK_ECCCI_C2K
static int c2k_req_push_to_usb(struct port_t *port, struct sk_buff *skb)
{
	int md_id = port->md_id;
	struct ccci_header *ccci_h = NULL;
	int read_len, read_count, ret = 0;
	int c2k_ch_id;
#if (MD_GENERATION <= 6292)
	int ppp_rx_ch = CCCI_C2K_PPP_DATA;
#else
	int ppp_rx_ch = CCCI_C2K_PPP_RX;
#endif

	if (port->rx_ch == ppp_rx_ch)
		c2k_ch_id = DATA_PPP_CH_C2K-1;
	else if (port->rx_ch == CCCI_MD_LOG_RX)
		c2k_ch_id = MDLOG_CH_C2K-2;
	else {
		ret = -ENODEV;
		CCCI_ERROR_LOG(md_id, CHAR,
			"Err: wrong ch_id(%d) from usb bypass\n", port->rx_ch);
		return ret;
	}

	/* caculate available data */
	ccci_h = (struct ccci_header *)skb->data;
	read_len = skb->len - sizeof(struct ccci_header);
	/* remove CCCI header */
	skb_pull(skb, sizeof(struct ccci_header));

retry_push:
	/* push to usb */
	read_count = rawbulk_push_upstream_buffer(c2k_ch_id,
		skb->data, read_len);
	CCCI_DEBUG_LOG(md_id, CHAR,
		"data push to usb bypass (ch%d)(%d)\n",
		port->rx_ch, read_count);

	if (read_count > 0) {
		skb_pull(skb, read_count);
		read_len -= read_count;
		if (read_len > 0)
			goto retry_push;
		else if (read_len == 0)
			ccci_free_skb(skb);
		else if (read_len < 0)
			CCCI_ERROR_LOG(md_id, CHAR,
				"read_len error, check why come here\n");
	} else {
		CCCI_NORMAL_LOG(md_id, CHAR, "usb buf full\n");
		msleep(20);
		goto retry_push;
	}

	return read_len;

}
#endif

static int port_char_recv_skb(struct port_t *port, struct sk_buff *skb)
{
	int md_id = port->md_id;

	if (!atomic_read(&port->usage_cnt) &&
		(port->rx_ch != CCCI_UART2_RX &&
		port->rx_ch != CCCI_C2K_AT &&
		port->rx_ch != CCCI_PCM_RX &&
		port->rx_ch != CCCI_FS_RX &&
		port->rx_ch != CCCI_RPC_RX &&
		port->rx_ch != CCCI_UDC_RX &&
		!(port->rx_ch == CCCI_IPC_RX &&
		port->minor ==
		AP_IPC_LWAPROXY + CCCI_IPC_MINOR_BASE)))
		return -CCCI_ERR_DROP_PACKET;

#ifdef CONFIG_MTK_ECCCI_C2K
	if (port->interception) {
		c2k_req_push_to_usb(port, skb);
		return 0;
	}
#endif
	CCCI_DEBUG_LOG(md_id, CHAR, "recv on %s, len=%d\n",
		port->name, port->rx_skb_list.qlen);
	return port_recv_skb(port, skb);
}

void port_char_dump_info(struct port_t *port, unsigned int flag)
{
	if (port == NULL) {
		CCCI_ERROR_LOG(0, CHAR, "%s: port==NULL\n", __func__);
		return;
	}
	if (atomic_read(&port->usage_cnt) == 0)
		return;
	if (port->flags & PORT_F_CH_TRAFFIC)
		CCCI_REPEAT_LOG(port->md_id, CHAR,
			"CHR:(%d):%dR(%d,%d,%d):%dT(%d)\n",
			port->flags, port->rx_ch,
			port->rx_skb_list.qlen,
			port->rx_pkg_cnt, port->rx_drop_cnt,
			port->tx_ch, port->tx_pkg_cnt);
}
struct port_ops char_port_ops = {
	.init = &port_char_init,
	.recv_skb = &port_char_recv_skb,
	.dump_info = &port_char_dump_info,
};

