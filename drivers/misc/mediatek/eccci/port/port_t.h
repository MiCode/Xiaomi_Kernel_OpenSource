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

#ifndef __PORT_T_H__
#define __PORT_T_H__
#include "ccci_core.h"

/* packet will be dropped if port's Rx buffer full */
#define PORT_F_ALLOW_DROP	(1<<0)
/* rx buffer has been full once */
#define PORT_F_RX_FULLED	(1<<1)
/* CCCI header will be provided by user, but not by CCCI */
#define PORT_F_USER_HEADER	(1<<2)
/* Rx queue only has this one port */
#define PORT_F_RX_EXCLUSIVE	(1<<3)
/* Check whether need remove ccci header while recv skb*/
#define PORT_F_ADJUST_HEADER	(1<<4)
/* Enable port channel traffic*/
#define PORT_F_CH_TRAFFIC	(1<<5)
/* Dump raw data if CH_TRAFFIC set*/
#define PORT_F_DUMP_RAW_DATA	(1<<6)
/* Need export char dev node for userspace*/
#define PORT_F_WITH_CHAR_NODE	(1<<7)

/* reused for net tx, Data queue, same bit as RX_FULLED */
#define PORT_F_TX_DATA_FULLED	(1<<1)
#define PORT_F_TX_ACK_FULLED	(1<<8)

/*Can be clean when MD is invalid*/
#define PORT_F_CLEAN            (1<<9)

enum {
	PORT_DBG_DUMP_RILD = 0,
	PORT_DBG_DUMP_AUDIO,
	PORT_DBG_DUMP_IMS,
};
struct port_t;
struct port_ops {
	/* must-have */
	int (*init)(struct port_t *port);
	int (*recv_skb)(struct port_t *port, struct sk_buff *skb);
	/* optional */
	int (*recv_match)(struct port_t *port, struct sk_buff *skb);
	void (*md_state_notify)(struct port_t *port, unsigned int md_state);
	void (*queue_state_notify)(struct port_t *port, int dir, int qno,
		unsigned int qstate);
	void (*dump_info)(struct port_t *port, unsigned int flag);
};
typedef void (*port_skb_handler)(struct port_t *port, struct sk_buff *skb);
struct port_t {
	/* don't change the sequence unless
	 * you modified modem drivers as well
	 */
	/* identity */
	CCCI_CH tx_ch;
	CCCI_CH rx_ch;
	/*
	 *
	 * here is a nasty trick, we assume no modem provide
	 * more than 0xF0 queues, so we use
	 * the lower 4 bit to smuggle info for network ports.
	 * Attention, in this trick we assume hardware queue index
	 * for net port will not exceed 0xF.
	 * check NET_ACK_TXQ_INDEX@port_net.c
	 */
	unsigned char txq_index;
	unsigned char rxq_index;
	unsigned char txq_exp_index;
	unsigned char rxq_exp_index;
	unsigned char hif_id;
	unsigned short flags;
	struct port_ops *ops;
	/* device node related */
	unsigned int minor;
	char *name;
	/* un-initiallized in defination, always put them at the end */
	int md_id;
	void *port_proxy;
	void *private_data;
	atomic_t usage_cnt;
	struct list_head entry;
	struct list_head exp_entry;
	struct list_head queue_entry;
	unsigned int major; /*dynamic alloc*/
	unsigned int minor_base;
	/*
	 * the Tx and Rx flow are asymmetric due to ports are
	 * mutilplexed on queues.
	 * Tx: data block are sent directly to queue's list,
	 * so port won't maitain a Tx list. It only
	 * provide a wait_queue_head for blocking write.
	 * Rx: due to modem needs to dispatch Rx packet
	 * as quickly as possible, so port needs a
	 * Rx list to hold packets.
	 */
	struct sk_buff_head rx_skb_list;
	/* add high prio rx list for udc */
	struct sk_buff_head rx_skb_list_hp;
	unsigned char skb_from_pool;
	spinlock_t rx_req_lock;
	wait_queue_head_t rx_wq;	/* for uplayer user */
	int rx_length;
	int rx_length_th;
	struct wakeup_source rx_wakelock;
	unsigned int tx_busy_count;
	unsigned int rx_busy_count;
	int interception;
	unsigned int rx_pkg_cnt;
	unsigned int rx_drop_cnt;
	unsigned int tx_pkg_cnt;
	port_skb_handler skb_handler;
};
/****************************************************************************/
/* API Region called by ccci port object */
/****************************************************************************/
/*
 *This API used to some port create kthread handle,
 *which have same kthread handle flow.
 */
int port_kthread_handler(void *arg);
/*
 *This API used to some port to receive native HIF RX data,
 *which have same RX receive flow.
 */
int port_recv_skb(struct port_t *port, struct sk_buff *skb);

int port_user_register(struct port_t *port);
int port_user_unregister(struct port_t *port);
int port_ask_more_req_to_md(struct port_t *port);
int port_write_room_to_md(struct port_t *port);
void port_ch_dump(struct port_t *port, int dir, void *msg_buf, int len);
int port_get_capability(int md_id);
struct port_t *port_get_by_node(int major, int minor);
struct port_t *port_get_by_minor(int md_id, int minor);
struct port_t *port_get_by_channel(int md_id, CCCI_CH ch);
int port_send_skb_to_md(struct port_t *port, struct sk_buff *skb,
	int blocking);
int port_net_send_skb_to_md(struct port_t *port, int is_ack,
	struct sk_buff *skb);
int port_send_msg_to_md(struct port_t *port, unsigned int msg,
		unsigned int resv, int blocking);

int port_dev_open(struct inode *inode, struct file *file);
int port_dev_close(struct inode *inode, struct file *file);
ssize_t port_dev_read(struct file *file, char *buf, size_t count,
	loff_t *ppos);
ssize_t port_dev_write(struct file *file, const char __user *buf, size_t count,
	loff_t *ppos);
long port_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
#ifdef CONFIG_COMPAT
long port_dev_compat_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg);
#endif
int port_dev_mmap(struct file *fp, struct vm_area_struct *vma);

#endif /* __PORT_T_H__ */
