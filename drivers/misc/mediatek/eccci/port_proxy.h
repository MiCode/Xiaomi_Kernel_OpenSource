/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __PORT_PROXY_H__
#define __PORT_PROXY_H__
#include <mt-plat/mt_ccci_common.h>
#include "ccci_core.h"

#define PORT_F_ALLOW_DROP	(1<<0)	/* packet will be dropped if port's Rx buffer full */
#define PORT_F_RX_FULLED	(1<<1)	/* rx buffer has been full once */
#define PORT_F_USER_HEADER	(1<<2)	/* CCCI header will be provided by user, but not by CCCI */
#define PORT_F_RX_EXCLUSIVE	(1<<3)	/* Rx queue only has this one port */
#define PORT_F_ADJUST_HEADER	(1<<4)	/* Check whether need remove ccci header while recv skb*/
#define PORT_F_CH_TRAFFIC	(1<<5)	/* Enable port channel traffic*/
#define PORT_F_DUMP_RAW_DATA	(1<<6)	/* Dump raw data if CH_TRAFFIC set*/

enum {
	PORT_DBG_DUMP_RILD = 0,
	PORT_DBG_DUMP_AUDIO,
	PORT_DBG_DUMP_IMS,
};
struct ccci_port_ops {
	/* must-have */
	int (*init)(struct ccci_port *port);
	int (*recv_skb)(struct ccci_port *port, struct sk_buff *skb);
	/* optional */
	int (*recv_match)(struct ccci_port *port, struct sk_buff *skb);
	void (*md_state_notice)(struct ccci_port *port, MD_STATE state);
	void (*dump_info)(struct ccci_port *port, unsigned flag);
};
typedef void (*port_skb_handler)(struct ccci_port *port, struct sk_buff *skb);
struct ccci_port {
	/* don't change the sequence unless you modified modem drivers as well */
	/* identity */
	CCCI_CH tx_ch;
	CCCI_CH rx_ch;
	/*
	 *
	 * here is a nasty trick, we assume no modem provide more than 0xF0 queues, so we use
	 * the lower 4 bit to smuggle info for network ports.
	 * Attention, in this trick we assume hardware queue index for net port will not exceed 0xF.
	 * check NET_ACK_TXQ_INDEX@port_net.c
	 */
	unsigned char txq_index;
	unsigned char rxq_index;
	unsigned char txq_exp_index;
	unsigned char rxq_exp_index;
	unsigned char flags;
	struct ccci_port_ops *ops;
	/* device node related */
	unsigned int minor;
	char *name;
	/* un-initiallized in defination, always put them at the end */
	int md_id;
	struct ccci_modem *modem;
	void *port_proxy;
	void *private_data;
	atomic_t usage_cnt;
	struct list_head entry;
	unsigned int major; /*dynamic alloc*/
	unsigned int minor_base;
	/*
	 * the Tx and Rx flow are asymmetric due to ports are mutilplexed on queues.
	 * Tx: data block are sent directly to queue's list, so port won't maitain a Tx list. It only
	 provide a wait_queue_head for blocking write.
	 * Rx: due to modem needs to dispatch Rx packet as quickly as possible, so port needs a
	 *      Rx list to hold packets.
	 */
	struct sk_buff_head rx_skb_list;
	unsigned char skb_from_pool;
	spinlock_t rx_req_lock;
	wait_queue_head_t rx_wq;	/* for uplayer user */
	int rx_length;
	int rx_length_th;
	struct wake_lock rx_wakelock;
	unsigned int tx_busy_count;
	unsigned int rx_busy_count;
	int interception;
	unsigned int rx_pkg_cnt;
	unsigned int rx_drop_cnt;
	unsigned int tx_pkg_cnt;
	port_skb_handler skb_handler;
};

struct port_proxy {
	int md_id;
	int port_number;
	unsigned int major;
	unsigned int minor_base;
	unsigned int md_capability;
	unsigned int napi_queue_mask;
	unsigned int sim_type;
	int dtr_state; /* only for usb bypass */
	unsigned int critical_user_active;
	unsigned int md_img_exist[MAX_IMG_NUM];
	unsigned int md_img_type_is_set;
	unsigned int mdlog_dump_done;
	unsigned int traffic_dump_flag;
	/*do NOT use this manner, otherwise spinlock inside private_data will trigger alignment exception */
	char wakelock_name[32];
	struct wake_lock wakelock;
	/* refer to ccci_modem obj
	* port sub class no need to reference, if really want, then add delegant API in port_proxy
	*/
	void *md_obj;
	struct ccci_port *sys_port;
	struct ccci_port *ctl_port;
	struct ccci_port *ports;
	struct ccci_port *napi_port[8];
	struct list_head rx_ch_ports[CCCI_MAX_CH_NUM];	/* port list of each Rx channel, for Rx dispatching */
	unsigned long long latest_rx_thread_time;
};

/****************************************************************************************************************/
/* API Region called by ccci modem object */
/****************************************************************************************************************/
struct port_proxy *port_proxy_alloc(int md_id, int md_capability, int nap_queue_mask, void *md);
void port_proxy_free(struct port_proxy *proxy_p);
void port_proxy_dump_status(struct port_proxy *proxy_p);
void port_proxy_md_status_notice(struct port_proxy *proxy_p, DIRECTION dir,
							int filter_ch_no, int filter_queue_idx, MD_STATE state);
void port_proxy_wake_up_tx_queue(struct port_proxy *proxy_p, unsigned char qno);
int port_proxy_recv_skb(struct port_proxy *proxy_p, struct sk_buff *skb);
int port_proxy_start_md(struct port_proxy *proxy_p);
int port_proxy_stop_md(struct port_proxy *proxy_p, unsigned int stop_type);
/****************************************************************************************************************/
/* API Region called by ccci port object */
/****************************************************************************************************************/
int port_kthread_handler(void *arg);
int port_recv_skb(struct ccci_port *port, struct sk_buff *skb);
void port_ch_dump(struct ccci_port *port, int dir, void *msg_buf, int len);
struct port_proxy *port_proxy_get_by_md_id(int md_id);
struct ccci_port *port_proxy_get_port(struct port_proxy *proxy_p, int minor, CCCI_CH ch);
struct ccci_port *port_proxy_get_port_by_node(int major, int minor);
void port_proxy_md_no_repsone_notify(struct port_proxy *proxy_p);
void port_proxy_poll_md_fail_notify(struct port_proxy *proxy_p, u64 latest_poll_start_time);
void port_proxy_set_md_dsp_protection(struct port_proxy *proxy_p, int is_loaded);
char *port_proxy_get_md_img_post_fix(struct port_proxy *proxy_p);
long port_proxy_user_ioctl(struct port_proxy *proxy_p, int ch, unsigned int cmd, unsigned long arg);
int port_proxy_user_register(struct port_proxy *proxy_p, struct ccci_port *port);
int port_proxy_user_unregister(struct port_proxy *proxy_p, struct ccci_port *port);
int port_proxy_get_md_state(struct port_proxy *proxy_p);
int port_proxy_send_msg_to_user(struct port_proxy *proxy_p, CCCI_CH ch, CCCI_MD_MSG msg, u32 resv);
int port_proxy_send_msg_to_md(struct port_proxy *proxy_p, CCCI_CH ch, u32 msg, u32 resv, int blocking);
int port_proxy_ask_more_req_to_md(struct port_proxy *proxy_p, struct ccci_port *port);
int port_proxy_send_skb_to_md(struct port_proxy *proxy_p, struct ccci_port *port,
				struct sk_buff *skb, int blocking);
int port_proxy_net_send_skb_to_md(struct port_proxy *proxy_p, struct ccci_port *port,
				int is_ack, struct sk_buff *skb);
int port_proxy_napi_poll(struct port_proxy *proxy_p, struct ccci_port *port,
				struct napi_struct *napi, int weight);
int port_proxy_write_room_to_md(struct port_proxy *proxy_p, struct ccci_port *port);
int port_proxy_send_ccb_tx_notify_to_md(struct port_proxy *proxy_p, int core_id);
void port_proxy_md_hs1_msg_notify(struct port_proxy *proxy_p, struct sk_buff *skb);
void port_proxy_md_hs2_msg_notify(struct port_proxy *proxy_p, struct sk_buff *skb);
void *port_proxy_get_mdee(struct port_proxy *proxy_p);
unsigned int port_proxy_get_poll_seq_num(struct port_proxy *proxy_p);
int port_proxy_check_critical_user(struct port_proxy *proxy_p);

#ifdef FEATURE_SCP_CCCI_SUPPORT
int port_proxy_ccism_shm_init_ack_hdlr(struct port_proxy *proxy_p, unsigned int data);
#endif

unsigned long long *port_proxy_get_md_net_rx_profile(struct port_proxy *proxy_p);
static inline int port_proxy_get_critical_user(struct port_proxy *proxy_p, int user_id)
{
	return ((proxy_p->critical_user_active & (1 << user_id)) >> user_id);
}
static inline void port_proxy_set_dtr_state(struct port_proxy *proxy_p, int value)
{
	proxy_p->dtr_state = value;
}
static inline void port_proxy_record_rx_sched_time(struct port_proxy *proxy_p, int ch)
{
	proxy_p->latest_rx_thread_time = local_clock();
}
static inline int port_proxy_get_capability(struct port_proxy *proxy_p)
{
	return proxy_p->md_capability;
}

static inline void port_proxy_set_sim_type(struct port_proxy *proxy_p, unsigned int sim_type)
{
	proxy_p->sim_type = sim_type;
}

static inline int port_proxy_is_napi_queue(struct port_proxy *proxy_p, int qno)
{
	return proxy_p->napi_port[qno] == NULL ? 0:1;
}

static inline int port_proxy_napi_queue_notice(struct port_proxy *proxy_p, int qno)
{
	struct ccci_port *port;

	port = proxy_p->napi_port[qno];
	if (port && port->ops->md_state_notice)
		port->ops->md_state_notice(port, RX_IRQ);
	return 0;
}

static inline int port_proxy_get_mdlog_dump_done(struct port_proxy *proxy_p)
{
	return proxy_p->mdlog_dump_done;
}
static inline void port_proxy_start_wake_lock(struct port_proxy *proxy_p, int sec)
{
	wake_lock_timeout(&proxy_p->wakelock, sec * HZ);
}
static inline struct ccci_port *port_proxy_get_port_by_minor(struct port_proxy *proxy_p, int minor)
{
	return port_proxy_get_port(proxy_p, minor, CCCI_INVALID_CH_ID);
}
static inline struct ccci_port *port_proxy_get_port_by_channel(struct port_proxy *proxy_p, CCCI_CH ch)
{
	return port_proxy_get_port(proxy_p, -1, ch);
}

static inline int port_proxy_append_fsm_event(struct port_proxy *proxy_p, CCCI_FSM_EVENT event_id,
	unsigned char *data, unsigned int length)
{
	return ccci_fsm_append_event(proxy_p->md_obj, event_id, data, length);
}
/****************************************************************************************************************/
/* External API Region called by port proxy object */
/****************************************************************************************************************/
extern int get_md_port_cfg(int md_id, struct ccci_port **ports);
extern int port_smem_cfg(int md_id, struct ccci_smem_layout *smem_layout);
extern struct ccci_modem *ccci_md_get_modem_by_id(int md_id);
#endif /* __PORT_PROXY_H__ */
