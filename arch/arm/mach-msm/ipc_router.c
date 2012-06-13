/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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

#define DEBUG

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/wakelock.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>

#include <asm/uaccess.h>
#include <asm/byteorder.h>

#include <mach/smem_log.h>
#include <mach/subsystem_notif.h>

#include "ipc_router.h"
#include "modem_notifier.h"

enum {
	SMEM_LOG = 1U << 0,
	RTR_DBG = 1U << 1,
	R2R_MSG = 1U << 2,
	R2R_RAW = 1U << 3,
	NTFY_MSG = 1U << 4,
	R2R_RAW_HDR = 1U << 5,
};

static int msm_ipc_router_debug_mask;
module_param_named(debug_mask, msm_ipc_router_debug_mask,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);

#define DIAG(x...) pr_info("[RR] ERROR " x)

#if defined(DEBUG)
#define D(x...) do { \
if (msm_ipc_router_debug_mask & RTR_DBG) \
	pr_info(x); \
} while (0)

#define RR(x...) do { \
if (msm_ipc_router_debug_mask & R2R_MSG) \
	pr_info("[RR] "x); \
} while (0)

#define RAW(x...) do { \
if (msm_ipc_router_debug_mask & R2R_RAW) \
	pr_info("[RAW] "x); \
} while (0)

#define NTFY(x...) do { \
if (msm_ipc_router_debug_mask & NTFY_MSG) \
	pr_info("[NOTIFY] "x); \
} while (0)

#define RAW_HDR(x...) do { \
if (msm_ipc_router_debug_mask & R2R_RAW_HDR) \
	pr_info("[HDR] "x); \
} while (0)
#else
#define D(x...) do { } while (0)
#define RR(x...) do { } while (0)
#define RAW(x...) do { } while (0)
#define RAW_HDR(x...) do { } while (0)
#define NTFY(x...) do { } while (0)
#endif

#define IPC_ROUTER_LOG_EVENT_ERROR      0x10
#define IPC_ROUTER_LOG_EVENT_TX         0x11
#define IPC_ROUTER_LOG_EVENT_RX         0x12

static LIST_HEAD(control_ports);
static DEFINE_MUTEX(control_ports_lock);

#define LP_HASH_SIZE 32
static struct list_head local_ports[LP_HASH_SIZE];
static DEFINE_MUTEX(local_ports_lock);

#define SRV_HASH_SIZE 32
static struct list_head server_list[SRV_HASH_SIZE];
static DEFINE_MUTEX(server_list_lock);
static wait_queue_head_t newserver_wait;

struct msm_ipc_server {
	struct list_head list;
	struct msm_ipc_port_name name;
	struct list_head server_port_list;
};

struct msm_ipc_server_port {
	struct list_head list;
	struct msm_ipc_port_addr server_addr;
	struct msm_ipc_router_xprt_info *xprt_info;
};

#define RP_HASH_SIZE 32
struct msm_ipc_router_remote_port {
	struct list_head list;
	uint32_t node_id;
	uint32_t port_id;
	uint32_t restart_state;
	wait_queue_head_t quota_wait;
	uint32_t tx_quota_cnt;
	struct mutex quota_lock;
};

struct msm_ipc_router_xprt_info {
	struct list_head list;
	struct msm_ipc_router_xprt *xprt;
	uint32_t remote_node_id;
	uint32_t initialized;
	struct list_head pkt_list;
	wait_queue_head_t read_wait;
	struct wake_lock wakelock;
	struct mutex rx_lock;
	struct mutex tx_lock;
	uint32_t need_len;
	uint32_t abort_data_read;
	struct work_struct read_data;
	struct workqueue_struct *workqueue;
};

#define RT_HASH_SIZE 4
struct msm_ipc_routing_table_entry {
	struct list_head list;
	uint32_t node_id;
	uint32_t neighbor_node_id;
	struct list_head remote_port_list[RP_HASH_SIZE];
	struct msm_ipc_router_xprt_info *xprt_info;
	struct mutex lock;
	unsigned long num_tx_bytes;
	unsigned long num_rx_bytes;
};

static struct list_head routing_table[RT_HASH_SIZE];
static DEFINE_MUTEX(routing_table_lock);
static int routing_table_inited;

static LIST_HEAD(msm_ipc_board_dev_list);
static DEFINE_MUTEX(msm_ipc_board_dev_list_lock);

static void do_read_data(struct work_struct *work);

#define RR_STATE_IDLE    0
#define RR_STATE_HEADER  1
#define RR_STATE_BODY    2
#define RR_STATE_ERROR   3

#define RESTART_NORMAL 0
#define RESTART_PEND 1

/* State for remote ep following restart */
#define RESTART_QUOTA_ABORT  1

static LIST_HEAD(xprt_info_list);
static DEFINE_MUTEX(xprt_info_list_lock);

DECLARE_COMPLETION(msm_ipc_remote_router_up);
static DECLARE_COMPLETION(msm_ipc_local_router_up);
#define IPC_ROUTER_INIT_TIMEOUT (10 * HZ)

static uint32_t next_port_id;
static DEFINE_MUTEX(next_port_id_lock);
static atomic_t pending_close_count = ATOMIC_INIT(0);
static wait_queue_head_t subsystem_restart_wait;
static struct workqueue_struct *msm_ipc_router_workqueue;

enum {
	CLIENT_PORT,
	SERVER_PORT,
	CONTROL_PORT,
};

enum {
	DOWN,
	UP,
};

static void init_routing_table(void)
{
	int i;
	for (i = 0; i < RT_HASH_SIZE; i++)
		INIT_LIST_HEAD(&routing_table[i]);
}

static struct msm_ipc_routing_table_entry *alloc_routing_table_entry(
	uint32_t node_id)
{
	int i;
	struct msm_ipc_routing_table_entry *rt_entry;

	rt_entry = kmalloc(sizeof(struct msm_ipc_routing_table_entry),
			   GFP_KERNEL);
	if (!rt_entry) {
		pr_err("%s: rt_entry allocation failed for %d\n",
			__func__, node_id);
		return NULL;
	}

	for (i = 0; i < RP_HASH_SIZE; i++)
		INIT_LIST_HEAD(&rt_entry->remote_port_list[i]);

	mutex_init(&rt_entry->lock);
	rt_entry->node_id = node_id;
	rt_entry->xprt_info = NULL;
	return rt_entry;
}

/*Please take routing_table_lock before calling this function*/
static int add_routing_table_entry(
	struct msm_ipc_routing_table_entry *rt_entry)
{
	uint32_t key;

	if (!rt_entry)
		return -EINVAL;

	key = (rt_entry->node_id % RT_HASH_SIZE);
	list_add_tail(&rt_entry->list, &routing_table[key]);
	return 0;
}

/*Please take routing_table_lock before calling this function*/
static struct msm_ipc_routing_table_entry *lookup_routing_table(
	uint32_t node_id)
{
	uint32_t key = (node_id % RT_HASH_SIZE);
	struct msm_ipc_routing_table_entry *rt_entry;

	list_for_each_entry(rt_entry, &routing_table[key], list) {
		if (rt_entry->node_id == node_id)
			return rt_entry;
	}
	return NULL;
}

struct rr_packet *rr_read(struct msm_ipc_router_xprt_info *xprt_info)
{
	struct rr_packet *temp_pkt;

	if (!xprt_info)
		return NULL;

	mutex_lock(&xprt_info->rx_lock);
	while (!(xprt_info->abort_data_read) &&
		list_empty(&xprt_info->pkt_list)) {
		mutex_unlock(&xprt_info->rx_lock);
		wait_event(xprt_info->read_wait,
			   ((xprt_info->abort_data_read) ||
			   !list_empty(&xprt_info->pkt_list)));
		mutex_lock(&xprt_info->rx_lock);
	}
	if (xprt_info->abort_data_read) {
		mutex_unlock(&xprt_info->rx_lock);
		return NULL;
	}

	temp_pkt = list_first_entry(&xprt_info->pkt_list,
				    struct rr_packet, list);
	list_del(&temp_pkt->list);
	if (list_empty(&xprt_info->pkt_list))
		wake_unlock(&xprt_info->wakelock);
	mutex_unlock(&xprt_info->rx_lock);
	return temp_pkt;
}

struct rr_packet *clone_pkt(struct rr_packet *pkt)
{
	struct rr_packet *cloned_pkt;
	struct sk_buff *temp_skb, *cloned_skb;
	struct sk_buff_head *pkt_fragment_q;

	cloned_pkt = kzalloc(sizeof(struct rr_packet), GFP_KERNEL);
	if (!cloned_pkt) {
		pr_err("%s: failure\n", __func__);
		return NULL;
	}

	pkt_fragment_q = kmalloc(sizeof(struct sk_buff_head), GFP_KERNEL);
	if (!pkt_fragment_q) {
		pr_err("%s: pkt_frag_q alloc failure\n", __func__);
		kfree(cloned_pkt);
		return NULL;
	}
	skb_queue_head_init(pkt_fragment_q);

	skb_queue_walk(pkt->pkt_fragment_q, temp_skb) {
		cloned_skb = skb_clone(temp_skb, GFP_KERNEL);
		if (!cloned_skb)
			goto fail_clone;
		skb_queue_tail(pkt_fragment_q, cloned_skb);
	}
	cloned_pkt->pkt_fragment_q = pkt_fragment_q;
	cloned_pkt->length = pkt->length;
	return cloned_pkt;

fail_clone:
	while (!skb_queue_empty(pkt_fragment_q)) {
		temp_skb = skb_dequeue(pkt_fragment_q);
		kfree_skb(temp_skb);
	}
	kfree(pkt_fragment_q);
	kfree(cloned_pkt);
	return NULL;
}

struct rr_packet *create_pkt(struct sk_buff_head *data)
{
	struct rr_packet *pkt;
	struct sk_buff *temp_skb;

	pkt = kzalloc(sizeof(struct rr_packet), GFP_KERNEL);
	if (!pkt) {
		pr_err("%s: failure\n", __func__);
		return NULL;
	}

	pkt->pkt_fragment_q = data;
	skb_queue_walk(pkt->pkt_fragment_q, temp_skb)
		pkt->length += temp_skb->len;
	return pkt;
}

void release_pkt(struct rr_packet *pkt)
{
	struct sk_buff *temp_skb;

	if (!pkt)
		return;

	if (!pkt->pkt_fragment_q) {
		kfree(pkt);
		return;
	}

	while (!skb_queue_empty(pkt->pkt_fragment_q)) {
		temp_skb = skb_dequeue(pkt->pkt_fragment_q);
		kfree_skb(temp_skb);
	}
	kfree(pkt->pkt_fragment_q);
	kfree(pkt);
	return;
}

static int post_control_ports(struct rr_packet *pkt)
{
	struct msm_ipc_port *port_ptr;
	struct rr_packet *cloned_pkt;

	if (!pkt)
		return -EINVAL;

	mutex_lock(&control_ports_lock);
	list_for_each_entry(port_ptr, &control_ports, list) {
		mutex_lock(&port_ptr->port_rx_q_lock);
		cloned_pkt = clone_pkt(pkt);
		wake_lock(&port_ptr->port_rx_wake_lock);
		list_add_tail(&cloned_pkt->list, &port_ptr->port_rx_q);
		wake_up(&port_ptr->port_rx_wait_q);
		mutex_unlock(&port_ptr->port_rx_q_lock);
	}
	mutex_unlock(&control_ports_lock);
	return 0;
}

static uint32_t allocate_port_id(void)
{
	uint32_t port_id = 0, prev_port_id, key;
	struct msm_ipc_port *port_ptr;

	mutex_lock(&next_port_id_lock);
	prev_port_id = next_port_id;
	mutex_lock(&local_ports_lock);
	do {
		next_port_id++;
		if ((next_port_id & 0xFFFFFFFE) == 0xFFFFFFFE)
			next_port_id = 1;

		key = (next_port_id & (LP_HASH_SIZE - 1));
		if (list_empty(&local_ports[key])) {
			port_id = next_port_id;
			break;
		}
		list_for_each_entry(port_ptr, &local_ports[key], list) {
			if (port_ptr->this_port.port_id == next_port_id) {
				port_id = next_port_id;
				break;
			}
		}
		if (!port_id) {
			port_id = next_port_id;
			break;
		}
		port_id = 0;
	} while (next_port_id != prev_port_id);
	mutex_unlock(&local_ports_lock);
	mutex_unlock(&next_port_id_lock);

	return port_id;
}

void msm_ipc_router_add_local_port(struct msm_ipc_port *port_ptr)
{
	uint32_t key;

	if (!port_ptr)
		return;

	key = (port_ptr->this_port.port_id & (LP_HASH_SIZE - 1));
	mutex_lock(&local_ports_lock);
	list_add_tail(&port_ptr->list, &local_ports[key]);
	mutex_unlock(&local_ports_lock);
}

struct msm_ipc_port *msm_ipc_router_create_raw_port(void *endpoint,
		void (*notify)(unsigned event, void *data,
			       void *addr, void *priv),
		void *priv)
{
	struct msm_ipc_port *port_ptr;

	port_ptr = kzalloc(sizeof(struct msm_ipc_port), GFP_KERNEL);
	if (!port_ptr)
		return NULL;

	port_ptr->this_port.node_id = IPC_ROUTER_NID_LOCAL;
	port_ptr->this_port.port_id = allocate_port_id();
	if (!port_ptr->this_port.port_id) {
		pr_err("%s: All port ids are in use\n", __func__);
		kfree(port_ptr);
		return NULL;
	}

	spin_lock_init(&port_ptr->port_lock);
	INIT_LIST_HEAD(&port_ptr->incomplete);
	mutex_init(&port_ptr->incomplete_lock);
	INIT_LIST_HEAD(&port_ptr->port_rx_q);
	mutex_init(&port_ptr->port_rx_q_lock);
	init_waitqueue_head(&port_ptr->port_rx_wait_q);
	snprintf(port_ptr->rx_wakelock_name, MAX_WAKELOCK_NAME_SZ,
		 "msm_ipc_read%08x:%08x",
		 port_ptr->this_port.node_id,
		 port_ptr->this_port.port_id);
	wake_lock_init(&port_ptr->port_rx_wake_lock,
			WAKE_LOCK_SUSPEND, port_ptr->rx_wakelock_name);

	port_ptr->endpoint = endpoint;
	port_ptr->notify = notify;
	port_ptr->priv = priv;

	msm_ipc_router_add_local_port(port_ptr);
	return port_ptr;
}

/*
 * Should be called with local_ports_lock locked
 */
static struct msm_ipc_port *msm_ipc_router_lookup_local_port(uint32_t port_id)
{
	int key = (port_id & (LP_HASH_SIZE - 1));
	struct msm_ipc_port *port_ptr;

	list_for_each_entry(port_ptr, &local_ports[key], list) {
		if (port_ptr->this_port.port_id == port_id) {
			return port_ptr;
		}
	}
	return NULL;
}

static struct msm_ipc_router_remote_port *msm_ipc_router_lookup_remote_port(
						uint32_t node_id,
						uint32_t port_id)
{
	struct msm_ipc_router_remote_port *rport_ptr;
	struct msm_ipc_routing_table_entry *rt_entry;
	int key = (port_id & (RP_HASH_SIZE - 1));

	mutex_lock(&routing_table_lock);
	rt_entry = lookup_routing_table(node_id);
	if (!rt_entry) {
		mutex_unlock(&routing_table_lock);
		pr_err("%s: Node is not up\n", __func__);
		return NULL;
	}

	mutex_lock(&rt_entry->lock);
	list_for_each_entry(rport_ptr,
			    &rt_entry->remote_port_list[key], list) {
		if (rport_ptr->port_id == port_id) {
			if (rport_ptr->restart_state != RESTART_NORMAL)
				rport_ptr = NULL;
			mutex_unlock(&rt_entry->lock);
			mutex_unlock(&routing_table_lock);
			return rport_ptr;
		}
	}
	mutex_unlock(&rt_entry->lock);
	mutex_unlock(&routing_table_lock);
	return NULL;
}

static struct msm_ipc_router_remote_port *msm_ipc_router_create_remote_port(
						uint32_t node_id,
						uint32_t port_id)
{
	struct msm_ipc_router_remote_port *rport_ptr;
	struct msm_ipc_routing_table_entry *rt_entry;
	int key = (port_id & (RP_HASH_SIZE - 1));

	mutex_lock(&routing_table_lock);
	rt_entry = lookup_routing_table(node_id);
	if (!rt_entry) {
		mutex_unlock(&routing_table_lock);
		pr_err("%s: Node is not up\n", __func__);
		return NULL;
	}

	mutex_lock(&rt_entry->lock);
	rport_ptr = kmalloc(sizeof(struct msm_ipc_router_remote_port),
			    GFP_KERNEL);
	if (!rport_ptr) {
		mutex_unlock(&rt_entry->lock);
		mutex_unlock(&routing_table_lock);
		pr_err("%s: Remote port alloc failed\n", __func__);
		return NULL;
	}
	rport_ptr->port_id = port_id;
	rport_ptr->node_id = node_id;
	rport_ptr->restart_state = RESTART_NORMAL;
	rport_ptr->tx_quota_cnt = 0;
	init_waitqueue_head(&rport_ptr->quota_wait);
	mutex_init(&rport_ptr->quota_lock);
	list_add_tail(&rport_ptr->list,
		      &rt_entry->remote_port_list[key]);
	mutex_unlock(&rt_entry->lock);
	mutex_unlock(&routing_table_lock);
	return rport_ptr;
}

static void msm_ipc_router_destroy_remote_port(
	struct msm_ipc_router_remote_port *rport_ptr)
{
	uint32_t node_id;
	struct msm_ipc_routing_table_entry *rt_entry;

	if (!rport_ptr)
		return;

	node_id = rport_ptr->node_id;
	mutex_lock(&routing_table_lock);
	rt_entry = lookup_routing_table(node_id);
	if (!rt_entry) {
		mutex_unlock(&routing_table_lock);
		pr_err("%s: Node %d is not up\n", __func__, node_id);
		return;
	}

	mutex_lock(&rt_entry->lock);
	list_del(&rport_ptr->list);
	kfree(rport_ptr);
	mutex_unlock(&rt_entry->lock);
	mutex_unlock(&routing_table_lock);
	return;
}

static struct msm_ipc_server *msm_ipc_router_lookup_server(
				uint32_t service,
				uint32_t instance,
				uint32_t node_id,
				uint32_t port_id)
{
	struct msm_ipc_server *server;
	struct msm_ipc_server_port *server_port;
	int key = (instance & (SRV_HASH_SIZE - 1));

	mutex_lock(&server_list_lock);
	list_for_each_entry(server, &server_list[key], list) {
		if ((server->name.service != service) ||
		    (server->name.instance != instance))
			continue;
		if ((node_id == 0) && (port_id == 0)) {
			mutex_unlock(&server_list_lock);
			return server;
		}
		list_for_each_entry(server_port, &server->server_port_list,
				    list) {
			if ((server_port->server_addr.node_id == node_id) &&
			    (server_port->server_addr.port_id == port_id)) {
				mutex_unlock(&server_list_lock);
				return server;
			}
		}
	}
	mutex_unlock(&server_list_lock);
	return NULL;
}

static struct msm_ipc_server *msm_ipc_router_create_server(
					uint32_t service,
					uint32_t instance,
					uint32_t node_id,
					uint32_t port_id,
		struct msm_ipc_router_xprt_info *xprt_info)
{
	struct msm_ipc_server *server = NULL;
	struct msm_ipc_server_port *server_port;
	int key = (instance & (SRV_HASH_SIZE - 1));

	mutex_lock(&server_list_lock);
	list_for_each_entry(server, &server_list[key], list) {
		if ((server->name.service == service) &&
		    (server->name.instance == instance))
			goto create_srv_port;
	}

	server = kmalloc(sizeof(struct msm_ipc_server), GFP_KERNEL);
	if (!server) {
		mutex_unlock(&server_list_lock);
		pr_err("%s: Server allocation failed\n", __func__);
		return NULL;
	}
	server->name.service = service;
	server->name.instance = instance;
	INIT_LIST_HEAD(&server->server_port_list);
	list_add_tail(&server->list, &server_list[key]);

create_srv_port:
	server_port = kmalloc(sizeof(struct msm_ipc_server_port), GFP_KERNEL);
	if (!server_port) {
		if (list_empty(&server->server_port_list)) {
			list_del(&server->list);
			kfree(server);
		}
		mutex_unlock(&server_list_lock);
		pr_err("%s: Server Port allocation failed\n", __func__);
		return NULL;
	}
	server_port->server_addr.node_id = node_id;
	server_port->server_addr.port_id = port_id;
	server_port->xprt_info = xprt_info;
	list_add_tail(&server_port->list, &server->server_port_list);
	mutex_unlock(&server_list_lock);

	return server;
}

static void msm_ipc_router_destroy_server(struct msm_ipc_server *server,
					  uint32_t node_id, uint32_t port_id)
{
	struct msm_ipc_server_port *server_port;

	if (!server)
		return;

	mutex_lock(&server_list_lock);
	list_for_each_entry(server_port, &server->server_port_list, list) {
		if ((server_port->server_addr.node_id == node_id) &&
		    (server_port->server_addr.port_id == port_id))
			break;
	}
	if (server_port) {
		list_del(&server_port->list);
		kfree(server_port);
	}
	if (list_empty(&server->server_port_list)) {
		list_del(&server->list);
		kfree(server);
	}
	mutex_unlock(&server_list_lock);
	return;
}

static int msm_ipc_router_send_control_msg(
		struct msm_ipc_router_xprt_info *xprt_info,
		union rr_control_msg *msg)
{
	struct rr_packet *pkt;
	struct sk_buff *ipc_rtr_pkt;
	struct rr_header *hdr;
	int pkt_size;
	void *data;
	struct sk_buff_head *pkt_fragment_q;
	int ret;

	if (!xprt_info || ((msg->cmd != IPC_ROUTER_CTRL_CMD_HELLO) &&
	    !xprt_info->initialized)) {
		pr_err("%s: xprt_info not initialized\n", __func__);
		return -EINVAL;
	}

	if (xprt_info->remote_node_id == IPC_ROUTER_NID_LOCAL)
		return 0;

	pkt = kzalloc(sizeof(struct rr_packet), GFP_KERNEL);
	if (!pkt) {
		pr_err("%s: pkt alloc failed\n", __func__);
		return -ENOMEM;
	}

	pkt_fragment_q = kmalloc(sizeof(struct sk_buff_head), GFP_KERNEL);
	if (!pkt_fragment_q) {
		pr_err("%s: pkt_fragment_q alloc failed\n", __func__);
		kfree(pkt);
		return -ENOMEM;
	}
	skb_queue_head_init(pkt_fragment_q);

	pkt_size = IPC_ROUTER_HDR_SIZE + sizeof(*msg);
	ipc_rtr_pkt = alloc_skb(pkt_size, GFP_KERNEL);
	if (!ipc_rtr_pkt) {
		pr_err("%s: ipc_rtr_pkt alloc failed\n", __func__);
		kfree(pkt_fragment_q);
		kfree(pkt);
		return -ENOMEM;
	}

	skb_reserve(ipc_rtr_pkt, IPC_ROUTER_HDR_SIZE);
	data = skb_put(ipc_rtr_pkt, sizeof(*msg));
	memcpy(data, msg, sizeof(*msg));
	hdr = (struct rr_header *)skb_push(ipc_rtr_pkt, IPC_ROUTER_HDR_SIZE);
	if (!hdr) {
		pr_err("%s: skb_push failed\n", __func__);
		kfree_skb(ipc_rtr_pkt);
		kfree(pkt_fragment_q);
		kfree(pkt);
		return -ENOMEM;
	}

	hdr->version = IPC_ROUTER_VERSION;
	hdr->type = msg->cmd;
	hdr->src_node_id = IPC_ROUTER_NID_LOCAL;
	hdr->src_port_id = IPC_ROUTER_ADDRESS;
	hdr->confirm_rx = 0;
	hdr->size = sizeof(*msg);
	hdr->dst_node_id = xprt_info->remote_node_id;
	hdr->dst_port_id = IPC_ROUTER_ADDRESS;
	skb_queue_tail(pkt_fragment_q, ipc_rtr_pkt);
	pkt->pkt_fragment_q = pkt_fragment_q;
	pkt->length = pkt_size;

	mutex_lock(&xprt_info->tx_lock);
	ret = xprt_info->xprt->write(pkt, pkt_size, xprt_info->xprt);
	mutex_unlock(&xprt_info->tx_lock);

	release_pkt(pkt);
	return ret;
}

static int msm_ipc_router_send_server_list(
		struct msm_ipc_router_xprt_info *xprt_info)
{
	union rr_control_msg ctl;
	struct msm_ipc_server *server;
	struct msm_ipc_server_port *server_port;
	int i;

	if (!xprt_info || !xprt_info->initialized) {
		pr_err("%s: Xprt info not initialized\n", __func__);
		return -EINVAL;
	}

	ctl.cmd = IPC_ROUTER_CTRL_CMD_NEW_SERVER;

	mutex_lock(&server_list_lock);
	for (i = 0; i < SRV_HASH_SIZE; i++) {
		list_for_each_entry(server, &server_list[i], list) {
			ctl.srv.service = server->name.service;
			ctl.srv.instance = server->name.instance;
			list_for_each_entry(server_port,
					    &server->server_port_list, list) {
				if (server_port->server_addr.node_id ==
				    xprt_info->remote_node_id)
					continue;

				ctl.srv.node_id =
					server_port->server_addr.node_id;
				ctl.srv.port_id =
					server_port->server_addr.port_id;
				msm_ipc_router_send_control_msg(xprt_info,
								&ctl);
			}
		}
	}
	mutex_unlock(&server_list_lock);

	return 0;
}

#if defined(DEBUG)
static char *type_to_str(int i)
{
	switch (i) {
	case IPC_ROUTER_CTRL_CMD_DATA:
		return "data    ";
	case IPC_ROUTER_CTRL_CMD_HELLO:
		return "hello   ";
	case IPC_ROUTER_CTRL_CMD_BYE:
		return "bye     ";
	case IPC_ROUTER_CTRL_CMD_NEW_SERVER:
		return "new_srvr";
	case IPC_ROUTER_CTRL_CMD_REMOVE_SERVER:
		return "rmv_srvr";
	case IPC_ROUTER_CTRL_CMD_REMOVE_CLIENT:
		return "rmv_clnt";
	case IPC_ROUTER_CTRL_CMD_RESUME_TX:
		return "resum_tx";
	case IPC_ROUTER_CTRL_CMD_EXIT:
		return "cmd_exit";
	default:
		return "invalid";
	}
}
#endif

static int broadcast_ctl_msg_locally(union rr_control_msg *msg)
{
	struct rr_packet *pkt;
	struct sk_buff *ipc_rtr_pkt;
	struct rr_header *hdr;
	int pkt_size;
	void *data;
	struct sk_buff_head *pkt_fragment_q;
	int ret;

	pkt = kzalloc(sizeof(struct rr_packet), GFP_KERNEL);
	if (!pkt) {
		pr_err("%s: pkt alloc failed\n", __func__);
		return -ENOMEM;
	}

	pkt_fragment_q = kmalloc(sizeof(struct sk_buff_head), GFP_KERNEL);
	if (!pkt_fragment_q) {
		pr_err("%s: pkt_fragment_q alloc failed\n", __func__);
		kfree(pkt);
		return -ENOMEM;
	}
	skb_queue_head_init(pkt_fragment_q);

	pkt_size = IPC_ROUTER_HDR_SIZE + sizeof(*msg);
	ipc_rtr_pkt = alloc_skb(pkt_size, GFP_KERNEL);
	if (!ipc_rtr_pkt) {
		pr_err("%s: ipc_rtr_pkt alloc failed\n", __func__);
		kfree(pkt_fragment_q);
		kfree(pkt);
		return -ENOMEM;
	}

	skb_reserve(ipc_rtr_pkt, IPC_ROUTER_HDR_SIZE);
	data = skb_put(ipc_rtr_pkt, sizeof(*msg));
	memcpy(data, msg, sizeof(*msg));
	hdr = (struct rr_header *)skb_push(ipc_rtr_pkt, IPC_ROUTER_HDR_SIZE);
	if (!hdr) {
		pr_err("%s: skb_push failed\n", __func__);
		kfree_skb(ipc_rtr_pkt);
		kfree(pkt_fragment_q);
		kfree(pkt);
		return -ENOMEM;
	}
	hdr->version = IPC_ROUTER_VERSION;
	hdr->type = msg->cmd;
	hdr->src_node_id = IPC_ROUTER_NID_LOCAL;
	hdr->src_port_id = IPC_ROUTER_ADDRESS;
	hdr->confirm_rx = 0;
	hdr->size = sizeof(*msg);
	hdr->dst_node_id = IPC_ROUTER_NID_LOCAL;
	hdr->dst_port_id = IPC_ROUTER_ADDRESS;
	skb_queue_tail(pkt_fragment_q, ipc_rtr_pkt);
	pkt->pkt_fragment_q = pkt_fragment_q;
	pkt->length = pkt_size;

	ret = post_control_ports(pkt);
	release_pkt(pkt);
	return ret;
}

static int broadcast_ctl_msg(union rr_control_msg *ctl)
{
	struct msm_ipc_router_xprt_info *xprt_info;

	mutex_lock(&xprt_info_list_lock);
	list_for_each_entry(xprt_info, &xprt_info_list, list) {
		msm_ipc_router_send_control_msg(xprt_info, ctl);
	}
	mutex_unlock(&xprt_info_list_lock);

	return 0;
}

static int relay_ctl_msg(struct msm_ipc_router_xprt_info *xprt_info,
			 union rr_control_msg *ctl)
{
	struct msm_ipc_router_xprt_info *fwd_xprt_info;

	if (!xprt_info || !ctl)
		return -EINVAL;

	mutex_lock(&xprt_info_list_lock);
	list_for_each_entry(fwd_xprt_info, &xprt_info_list, list) {
		if (xprt_info->xprt->link_id != fwd_xprt_info->xprt->link_id)
			msm_ipc_router_send_control_msg(fwd_xprt_info, ctl);
	}
	mutex_unlock(&xprt_info_list_lock);

	return 0;
}

static int relay_msg(struct msm_ipc_router_xprt_info *xprt_info,
		     struct rr_packet *pkt)
{
	struct msm_ipc_router_xprt_info *fwd_xprt_info;

	if (!xprt_info || !pkt)
		return -EINVAL;

	mutex_lock(&xprt_info_list_lock);
	list_for_each_entry(fwd_xprt_info, &xprt_info_list, list) {
		mutex_lock(&fwd_xprt_info->tx_lock);
		if (xprt_info->xprt->link_id != fwd_xprt_info->xprt->link_id)
			fwd_xprt_info->xprt->write(pkt, pkt->length,
						   fwd_xprt_info->xprt);
		mutex_unlock(&fwd_xprt_info->tx_lock);
	}
	mutex_unlock(&xprt_info_list_lock);
	return 0;
}

static int forward_msg(struct msm_ipc_router_xprt_info *xprt_info,
		       struct rr_packet *pkt)
{
	uint32_t dst_node_id;
	struct sk_buff *head_pkt;
	struct rr_header *hdr;
	struct msm_ipc_router_xprt_info *fwd_xprt_info;
	struct msm_ipc_routing_table_entry *rt_entry;

	if (!xprt_info || !pkt)
		return -EINVAL;

	head_pkt = skb_peek(pkt->pkt_fragment_q);
	if (!head_pkt)
		return -EINVAL;

	hdr = (struct rr_header *)head_pkt->data;
	dst_node_id = hdr->dst_node_id;
	mutex_lock(&routing_table_lock);
	rt_entry = lookup_routing_table(dst_node_id);
	if (!(rt_entry) || !(rt_entry->xprt_info)) {
		mutex_unlock(&routing_table_lock);
		pr_err("%s: Routing table not initialized\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&rt_entry->lock);
	fwd_xprt_info = rt_entry->xprt_info;
	mutex_lock(&fwd_xprt_info->tx_lock);
	if (xprt_info->remote_node_id == fwd_xprt_info->remote_node_id) {
		mutex_unlock(&fwd_xprt_info->tx_lock);
		mutex_unlock(&rt_entry->lock);
		mutex_unlock(&routing_table_lock);
		pr_err("%s: Discarding Command to route back\n", __func__);
		return -EINVAL;
	}

	if (xprt_info->xprt->link_id == fwd_xprt_info->xprt->link_id) {
		mutex_unlock(&fwd_xprt_info->tx_lock);
		mutex_unlock(&rt_entry->lock);
		mutex_unlock(&routing_table_lock);
		pr_err("%s: DST in the same cluster\n", __func__);
		return 0;
	}
	fwd_xprt_info->xprt->write(pkt, pkt->length, fwd_xprt_info->xprt);
	mutex_unlock(&fwd_xprt_info->tx_lock);
	mutex_unlock(&rt_entry->lock);
	mutex_unlock(&routing_table_lock);

	return 0;
}

static void reset_remote_port_info(uint32_t node_id, uint32_t port_id)
{
	struct msm_ipc_router_remote_port *rport_ptr;

	rport_ptr = msm_ipc_router_lookup_remote_port(node_id, port_id);
	if (!rport_ptr) {
		pr_err("%s: No such remote port %08x:%08x\n",
			__func__, node_id, port_id);
		return;
	}
	mutex_lock(&rport_ptr->quota_lock);
	rport_ptr->restart_state = RESTART_PEND;
	wake_up(&rport_ptr->quota_wait);
	mutex_unlock(&rport_ptr->quota_lock);
	return;
}

static void msm_ipc_cleanup_remote_server_info(
		struct msm_ipc_router_xprt_info *xprt_info)
{
	struct msm_ipc_server *svr, *tmp_svr;
	struct msm_ipc_server_port *svr_port, *tmp_svr_port;
	int i;
	union rr_control_msg ctl;

	if (!xprt_info) {
		pr_err("%s: Invalid xprt_info\n", __func__);
		return;
	}

	ctl.cmd = IPC_ROUTER_CTRL_CMD_REMOVE_SERVER;
	mutex_lock(&server_list_lock);
	for (i = 0; i < SRV_HASH_SIZE; i++) {
		list_for_each_entry_safe(svr, tmp_svr, &server_list[i], list) {
			ctl.srv.service = svr->name.service;
			ctl.srv.instance = svr->name.instance;
			list_for_each_entry_safe(svr_port, tmp_svr_port,
					 &svr->server_port_list, list) {
				if (svr_port->xprt_info != xprt_info)
					continue;
				D("Remove server %08x:%08x - %08x:%08x",
				   ctl.srv.service, ctl.srv.instance,
				   svr_port->server_addr.node_id,
				   svr_port->server_addr.port_id);
				reset_remote_port_info(
					svr_port->server_addr.node_id,
					svr_port->server_addr.port_id);
				ctl.srv.node_id = svr_port->server_addr.node_id;
				ctl.srv.port_id = svr_port->server_addr.port_id;
				relay_ctl_msg(xprt_info, &ctl);
				broadcast_ctl_msg_locally(&ctl);
				list_del(&svr_port->list);
				kfree(svr_port);
			}
			if (list_empty(&svr->server_port_list)) {
				list_del(&svr->list);
				kfree(svr);
			}
		}
	}
	mutex_unlock(&server_list_lock);
}

static void msm_ipc_cleanup_remote_client_info(
		struct msm_ipc_router_xprt_info *xprt_info)
{
	struct msm_ipc_routing_table_entry *rt_entry;
	struct msm_ipc_router_remote_port *rport_ptr;
	int i, j;
	union rr_control_msg ctl;

	if (!xprt_info) {
		pr_err("%s: Invalid xprt_info\n", __func__);
		return;
	}

	ctl.cmd = IPC_ROUTER_CTRL_CMD_REMOVE_CLIENT;
	mutex_lock(&routing_table_lock);
	for (i = 0; i < RT_HASH_SIZE; i++) {
		list_for_each_entry(rt_entry, &routing_table[i], list) {
			mutex_lock(&rt_entry->lock);
			if (rt_entry->xprt_info != xprt_info) {
				mutex_unlock(&rt_entry->lock);
				continue;
			}
			for (j = 0; j < RP_HASH_SIZE; j++) {
				list_for_each_entry(rport_ptr,
					&rt_entry->remote_port_list[j], list) {
					if (rport_ptr->restart_state ==
						RESTART_PEND)
						continue;
					mutex_lock(&rport_ptr->quota_lock);
					rport_ptr->restart_state = RESTART_PEND;
					wake_up(&rport_ptr->quota_wait);
					mutex_unlock(&rport_ptr->quota_lock);
					ctl.cli.node_id = rport_ptr->node_id;
					ctl.cli.port_id = rport_ptr->port_id;
					broadcast_ctl_msg_locally(&ctl);
				}
			}
			mutex_unlock(&rt_entry->lock);
		}
	}
	mutex_unlock(&routing_table_lock);
}

static void msm_ipc_cleanup_remote_port_info(uint32_t node_id)
{
	struct msm_ipc_routing_table_entry *rt_entry, *tmp_rt_entry;
	struct msm_ipc_router_remote_port *rport_ptr, *tmp_rport_ptr;
	int i, j;

	mutex_lock(&routing_table_lock);
	for (i = 0; i < RT_HASH_SIZE; i++) {
		list_for_each_entry_safe(rt_entry, tmp_rt_entry,
					 &routing_table[i], list) {
			mutex_lock(&rt_entry->lock);
			if (rt_entry->neighbor_node_id != node_id) {
				mutex_unlock(&rt_entry->lock);
				continue;
			}
			for (j = 0; j < RP_HASH_SIZE; j++) {
				list_for_each_entry_safe(rport_ptr,
					tmp_rport_ptr,
					&rt_entry->remote_port_list[j], list) {
					list_del(&rport_ptr->list);
					kfree(rport_ptr);
				}
			}
			mutex_unlock(&rt_entry->lock);
		}
	}
	mutex_unlock(&routing_table_lock);
}

static void msm_ipc_cleanup_routing_table(
	struct msm_ipc_router_xprt_info *xprt_info)
{
	int i;
	struct msm_ipc_routing_table_entry *rt_entry;

	if (!xprt_info) {
		pr_err("%s: Invalid xprt_info\n", __func__);
		return;
	}

	mutex_lock(&routing_table_lock);
	for (i = 0; i < RT_HASH_SIZE; i++) {
		list_for_each_entry(rt_entry, &routing_table[i], list) {
			mutex_lock(&rt_entry->lock);
			if (rt_entry->xprt_info == xprt_info)
				rt_entry->xprt_info = NULL;
			mutex_unlock(&rt_entry->lock);
		}
	}
	mutex_unlock(&routing_table_lock);
}

static void modem_reset_cleanup(struct msm_ipc_router_xprt_info *xprt_info)
{

	if (!xprt_info) {
		pr_err("%s: Invalid xprt_info\n", __func__);
		return;
	}

	msm_ipc_cleanup_remote_server_info(xprt_info);
	msm_ipc_cleanup_remote_client_info(xprt_info);
	msm_ipc_cleanup_routing_table(xprt_info);
}

static int process_control_msg(struct msm_ipc_router_xprt_info *xprt_info,
			       struct rr_packet *pkt)
{
	union rr_control_msg ctl;
	union rr_control_msg *msg;
	struct msm_ipc_router_remote_port *rport_ptr;
	int rc = 0;
	static uint32_t first = 1;
	struct sk_buff *temp_ptr;
	struct rr_header *hdr;
	struct msm_ipc_server *server;
	struct msm_ipc_routing_table_entry *rt_entry;

	if (pkt->length != (IPC_ROUTER_HDR_SIZE + sizeof(*msg))) {
		pr_err("%s: r2r msg size %d != %d\n", __func__, pkt->length,
			(IPC_ROUTER_HDR_SIZE + sizeof(*msg)));
		return -EINVAL;
	}

	temp_ptr = skb_peek(pkt->pkt_fragment_q);
	if (!temp_ptr) {
		pr_err("%s: pkt_fragment_q is empty\n", __func__);
		return -EINVAL;
	}
	hdr = (struct rr_header *)temp_ptr->data;
	if (!hdr) {
		pr_err("%s: No data inside the skb\n", __func__);
		return -EINVAL;
	}
	msg = (union rr_control_msg *)((char *)hdr + IPC_ROUTER_HDR_SIZE);

	switch (msg->cmd) {
	case IPC_ROUTER_CTRL_CMD_HELLO:
		RR("o HELLO NID %d\n", hdr->src_node_id);
		xprt_info->remote_node_id = hdr->src_node_id;

		mutex_lock(&routing_table_lock);
		rt_entry = lookup_routing_table(hdr->src_node_id);
		if (!rt_entry) {
			rt_entry = alloc_routing_table_entry(hdr->src_node_id);
			if (!rt_entry) {
				mutex_unlock(&routing_table_lock);
				pr_err("%s: rt_entry allocation failed\n",
					__func__);
				return -ENOMEM;
			}
			add_routing_table_entry(rt_entry);
		}
		mutex_lock(&rt_entry->lock);
		rt_entry->neighbor_node_id = xprt_info->remote_node_id;
		rt_entry->xprt_info = xprt_info;
		mutex_unlock(&rt_entry->lock);
		mutex_unlock(&routing_table_lock);
		msm_ipc_cleanup_remote_port_info(xprt_info->remote_node_id);

		memset(&ctl, 0, sizeof(ctl));
		ctl.cmd = IPC_ROUTER_CTRL_CMD_HELLO;
		msm_ipc_router_send_control_msg(xprt_info, &ctl);

		xprt_info->initialized = 1;

		/* Send list of servers one at a time */
		msm_ipc_router_send_server_list(xprt_info);

		if (first) {
			first = 0;
			complete_all(&msm_ipc_remote_router_up);
		}
		RR("HELLO message processed\n");
		break;
	case IPC_ROUTER_CTRL_CMD_RESUME_TX:
		RR("o RESUME_TX id=%d:%08x\n",
		   msg->cli.node_id, msg->cli.port_id);

		rport_ptr = msm_ipc_router_lookup_remote_port(msg->cli.node_id,
							msg->cli.port_id);
		if (!rport_ptr) {
			pr_err("%s: Unable to resume client\n", __func__);
			break;
		}
		mutex_lock(&rport_ptr->quota_lock);
		rport_ptr->tx_quota_cnt = 0;
		mutex_unlock(&rport_ptr->quota_lock);
		wake_up(&rport_ptr->quota_wait);
		break;

	case IPC_ROUTER_CTRL_CMD_NEW_SERVER:
		if (msg->srv.instance == 0) {
			pr_err(
			"rpcrouter: Server create rejected, version = 0, "
			"service = %08x\n", msg->srv.service);
			break;
		}

		RR("o NEW_SERVER id=%d:%08x service=%08x:%08x\n",
		   msg->srv.node_id, msg->srv.port_id,
		   msg->srv.service, msg->srv.instance);

		mutex_lock(&routing_table_lock);
		rt_entry = lookup_routing_table(msg->srv.node_id);
		if (!rt_entry) {
			rt_entry = alloc_routing_table_entry(msg->srv.node_id);
			if (!rt_entry) {
				mutex_unlock(&routing_table_lock);
				pr_err("%s: rt_entry allocation failed\n",
					__func__);
				return -ENOMEM;
			}
			mutex_lock(&rt_entry->lock);
			rt_entry->neighbor_node_id = xprt_info->remote_node_id;
			rt_entry->xprt_info = xprt_info;
			mutex_unlock(&rt_entry->lock);
			add_routing_table_entry(rt_entry);
		}
		mutex_unlock(&routing_table_lock);

		server = msm_ipc_router_lookup_server(msg->srv.service,
						      msg->srv.instance,
						      msg->srv.node_id,
						      msg->srv.port_id);
		if (!server) {
			server = msm_ipc_router_create_server(
				msg->srv.service, msg->srv.instance,
				msg->srv.node_id, msg->srv.port_id, xprt_info);
			if (!server) {
				pr_err("%s: Server Create failed\n", __func__);
				return -ENOMEM;
			}

			if (!msm_ipc_router_lookup_remote_port(
					msg->srv.node_id, msg->srv.port_id)) {
				rport_ptr = msm_ipc_router_create_remote_port(
					msg->srv.node_id, msg->srv.port_id);
				if (!rport_ptr)
					pr_err("%s: Remote port create "
					       "failed\n", __func__);
			}
			wake_up(&newserver_wait);
		}

		relay_msg(xprt_info, pkt);
		post_control_ports(pkt);
		break;
	case IPC_ROUTER_CTRL_CMD_REMOVE_SERVER:
		RR("o REMOVE_SERVER service=%08x:%d\n",
		   msg->srv.service, msg->srv.instance);
		server = msm_ipc_router_lookup_server(msg->srv.service,
						      msg->srv.instance,
						      msg->srv.node_id,
						      msg->srv.port_id);
		if (server) {
			msm_ipc_router_destroy_server(server,
						      msg->srv.node_id,
						      msg->srv.port_id);
			relay_msg(xprt_info, pkt);
			post_control_ports(pkt);
		}
		break;
	case IPC_ROUTER_CTRL_CMD_REMOVE_CLIENT:
		RR("o REMOVE_CLIENT id=%d:%08x\n",
		    msg->cli.node_id, msg->cli.port_id);
		rport_ptr = msm_ipc_router_lookup_remote_port(msg->cli.node_id,
							msg->cli.port_id);
		if (rport_ptr)
			msm_ipc_router_destroy_remote_port(rport_ptr);

		relay_msg(xprt_info, pkt);
		post_control_ports(pkt);
		break;
	case IPC_ROUTER_CTRL_CMD_PING:
		/* No action needed for ping messages received */
		RR("o PING\n");
		break;
	default:
		RR("o UNKNOWN(%08x)\n", msg->cmd);
		rc = -ENOSYS;
	}

	return rc;
}

static void do_read_data(struct work_struct *work)
{
	struct rr_header *hdr;
	struct rr_packet *pkt = NULL;
	struct msm_ipc_port *port_ptr;
	struct sk_buff *head_skb;
	struct msm_ipc_port_addr *src_addr;
	struct msm_ipc_router_remote_port *rport_ptr;
	uint32_t resume_tx, resume_tx_node_id, resume_tx_port_id;

	struct msm_ipc_router_xprt_info *xprt_info =
		container_of(work,
			     struct msm_ipc_router_xprt_info,
			     read_data);

	pkt = rr_read(xprt_info);
	if (!pkt) {
		pr_err("%s: rr_read failed\n", __func__);
		goto fail_io;
	}

	if (pkt->length < IPC_ROUTER_HDR_SIZE ||
	    pkt->length > MAX_IPC_PKT_SIZE) {
		pr_err("%s: Invalid pkt length %d\n", __func__, pkt->length);
		goto fail_data;
	}

	head_skb = skb_peek(pkt->pkt_fragment_q);
	if (!head_skb) {
		pr_err("%s: head_skb is invalid\n", __func__);
		goto fail_data;
	}

	hdr = (struct rr_header *)(head_skb->data);
	RR("- ver=%d type=%d src=%d:%08x crx=%d siz=%d dst=%d:%08x\n",
	   hdr->version, hdr->type, hdr->src_node_id, hdr->src_port_id,
	   hdr->confirm_rx, hdr->size, hdr->dst_node_id, hdr->dst_port_id);
	RAW_HDR("[r rr_h] "
		"ver=%i,type=%s,src_node_id=%08x,src_port_id=%08x,"
		"confirm_rx=%i,size=%3i,dst_node_id=%08x,dst_port_id=%08x\n",
		hdr->version, type_to_str(hdr->type), hdr->src_node_id,
		hdr->src_port_id, hdr->confirm_rx, hdr->size, hdr->dst_node_id,
		hdr->dst_port_id);

	if (hdr->version != IPC_ROUTER_VERSION) {
		pr_err("version %d != %d\n", hdr->version, IPC_ROUTER_VERSION);
		goto fail_data;
	}

	if ((hdr->dst_node_id != IPC_ROUTER_NID_LOCAL) &&
	    ((hdr->type == IPC_ROUTER_CTRL_CMD_RESUME_TX) ||
	     (hdr->type == IPC_ROUTER_CTRL_CMD_DATA))) {
		forward_msg(xprt_info, pkt);
		release_pkt(pkt);
		goto done;
	}

	if ((hdr->dst_port_id == IPC_ROUTER_ADDRESS) ||
	    (hdr->type == IPC_ROUTER_CTRL_CMD_HELLO)) {
		process_control_msg(xprt_info, pkt);
		release_pkt(pkt);
		goto done;
	}
#if defined(CONFIG_MSM_SMD_LOGGING)
#if defined(DEBUG)
	if (msm_ipc_router_debug_mask & SMEM_LOG) {
		smem_log_event((SMEM_LOG_PROC_ID_APPS |
			SMEM_LOG_RPC_ROUTER_EVENT_BASE |
			IPC_ROUTER_LOG_EVENT_RX),
			(hdr->src_node_id << 24) |
			(hdr->src_port_id & 0xffffff),
			(hdr->dst_node_id << 24) |
			(hdr->dst_port_id & 0xffffff),
			(hdr->type << 24) | (hdr->confirm_rx << 16) |
			(hdr->size & 0xffff));
	}
#endif
#endif

	resume_tx = hdr->confirm_rx;
	resume_tx_node_id = hdr->dst_node_id;
	resume_tx_port_id = hdr->dst_port_id;

	rport_ptr = msm_ipc_router_lookup_remote_port(hdr->src_node_id,
						      hdr->src_port_id);

	mutex_lock(&local_ports_lock);
	port_ptr = msm_ipc_router_lookup_local_port(hdr->dst_port_id);
	if (!port_ptr) {
		pr_err("%s: No local port id %08x\n", __func__,
			hdr->dst_port_id);
		mutex_unlock(&local_ports_lock);
		release_pkt(pkt);
		goto process_done;
	}

	if (!rport_ptr) {
		rport_ptr = msm_ipc_router_create_remote_port(
							hdr->src_node_id,
							hdr->src_port_id);
		if (!rport_ptr) {
			pr_err("%s: Remote port %08x:%08x creation failed\n",
				__func__, hdr->src_node_id, hdr->src_port_id);
			mutex_unlock(&local_ports_lock);
			goto process_done;
		}
	}

	if (!port_ptr->notify) {
		mutex_lock(&port_ptr->port_rx_q_lock);
		wake_lock(&port_ptr->port_rx_wake_lock);
		list_add_tail(&pkt->list, &port_ptr->port_rx_q);
		wake_up(&port_ptr->port_rx_wait_q);
		mutex_unlock(&port_ptr->port_rx_q_lock);
		mutex_unlock(&local_ports_lock);
	} else {
		mutex_lock(&port_ptr->port_rx_q_lock);
		src_addr = kmalloc(sizeof(struct msm_ipc_port_addr),
				   GFP_KERNEL);
		if (src_addr) {
			src_addr->node_id = hdr->src_node_id;
			src_addr->port_id = hdr->src_port_id;
		}
		skb_pull(head_skb, IPC_ROUTER_HDR_SIZE);
		mutex_unlock(&local_ports_lock);
		port_ptr->notify(MSM_IPC_ROUTER_READ_CB, pkt->pkt_fragment_q,
				 src_addr, port_ptr->priv);
		mutex_unlock(&port_ptr->port_rx_q_lock);
		pkt->pkt_fragment_q = NULL;
		src_addr = NULL;
		release_pkt(pkt);
	}

process_done:
	if (resume_tx) {
		union rr_control_msg msg;

		msg.cmd = IPC_ROUTER_CTRL_CMD_RESUME_TX;
		msg.cli.node_id = resume_tx_node_id;
		msg.cli.port_id = resume_tx_port_id;

		RR("x RESUME_TX id=%d:%08x\n",
		   msg.cli.node_id, msg.cli.port_id);
		msm_ipc_router_send_control_msg(xprt_info, &msg);
	}

done:
	queue_work(xprt_info->workqueue, &xprt_info->read_data);
	return;

fail_data:
	release_pkt(pkt);
fail_io:
	pr_err("ipc_router has died\n");
}

int msm_ipc_router_register_server(struct msm_ipc_port *port_ptr,
				   struct msm_ipc_addr *name)
{
	struct msm_ipc_server *server;
	unsigned long flags;
	union rr_control_msg ctl;

	if (!port_ptr || !name)
		return -EINVAL;

	if (name->addrtype != MSM_IPC_ADDR_NAME)
		return -EINVAL;

	server = msm_ipc_router_lookup_server(name->addr.port_name.service,
					      name->addr.port_name.instance,
					      IPC_ROUTER_NID_LOCAL,
					      port_ptr->this_port.port_id);
	if (server) {
		pr_err("%s: Server already present\n", __func__);
		return -EINVAL;
	}

	server = msm_ipc_router_create_server(name->addr.port_name.service,
					      name->addr.port_name.instance,
					      IPC_ROUTER_NID_LOCAL,
					      port_ptr->this_port.port_id,
					      NULL);
	if (!server) {
		pr_err("%s: Server Creation failed\n", __func__);
		return -EINVAL;
	}

	ctl.cmd = IPC_ROUTER_CTRL_CMD_NEW_SERVER;
	ctl.srv.service = server->name.service;
	ctl.srv.instance = server->name.instance;
	ctl.srv.node_id = IPC_ROUTER_NID_LOCAL;
	ctl.srv.port_id = port_ptr->this_port.port_id;
	broadcast_ctl_msg(&ctl);
	spin_lock_irqsave(&port_ptr->port_lock, flags);
	port_ptr->type = SERVER_PORT;
	port_ptr->port_name.service = server->name.service;
	port_ptr->port_name.instance = server->name.instance;
	spin_unlock_irqrestore(&port_ptr->port_lock, flags);
	return 0;
}

int msm_ipc_router_unregister_server(struct msm_ipc_port *port_ptr)
{
	struct msm_ipc_server *server;
	unsigned long flags;
	union rr_control_msg ctl;

	if (!port_ptr)
		return -EINVAL;

	if (port_ptr->type != SERVER_PORT) {
		pr_err("%s: Trying to unregister a non-server port\n",
			__func__);
		return -EINVAL;
	}

	if (port_ptr->this_port.node_id != IPC_ROUTER_NID_LOCAL) {
		pr_err("%s: Trying to unregister a remote server locally\n",
			__func__);
		return -EINVAL;
	}

	server = msm_ipc_router_lookup_server(port_ptr->port_name.service,
					      port_ptr->port_name.instance,
					      port_ptr->this_port.node_id,
					      port_ptr->this_port.port_id);
	if (!server) {
		pr_err("%s: Server lookup failed\n", __func__);
		return -ENODEV;
	}

	ctl.cmd = IPC_ROUTER_CTRL_CMD_REMOVE_SERVER;
	ctl.srv.service = server->name.service;
	ctl.srv.instance = server->name.instance;
	ctl.srv.node_id = IPC_ROUTER_NID_LOCAL;
	ctl.srv.port_id = port_ptr->this_port.port_id;
	broadcast_ctl_msg(&ctl);
	msm_ipc_router_destroy_server(server, port_ptr->this_port.node_id,
				      port_ptr->this_port.port_id);
	spin_lock_irqsave(&port_ptr->port_lock, flags);
	port_ptr->type = CLIENT_PORT;
	spin_unlock_irqrestore(&port_ptr->port_lock, flags);
	return 0;
}

static int loopback_data(struct msm_ipc_port *src,
			uint32_t port_id,
			struct sk_buff_head *data)
{
	struct sk_buff *head_skb;
	struct rr_header *hdr;
	struct msm_ipc_port *port_ptr;
	struct rr_packet *pkt;

	if (!data) {
		pr_err("%s: Invalid pkt pointer\n", __func__);
		return -EINVAL;
	}

	pkt = create_pkt(data);
	if (!pkt) {
		pr_err("%s: New pkt create failed\n", __func__);
		return -ENOMEM;
	}

	head_skb = skb_peek(pkt->pkt_fragment_q);
	if (!head_skb) {
		pr_err("%s: pkt_fragment_q is empty\n", __func__);
		return -EINVAL;
	}
	hdr = (struct rr_header *)skb_push(head_skb, IPC_ROUTER_HDR_SIZE);
	if (!hdr) {
		pr_err("%s: Prepend Header failed\n", __func__);
		release_pkt(pkt);
		return -ENOMEM;
	}
	hdr->version = IPC_ROUTER_VERSION;
	hdr->type = IPC_ROUTER_CTRL_CMD_DATA;
	hdr->src_node_id = src->this_port.node_id;
	hdr->src_port_id = src->this_port.port_id;
	hdr->size = pkt->length;
	hdr->confirm_rx = 0;
	hdr->dst_node_id = IPC_ROUTER_NID_LOCAL;
	hdr->dst_port_id = port_id;
	pkt->length += IPC_ROUTER_HDR_SIZE;

	mutex_lock(&local_ports_lock);
	port_ptr = msm_ipc_router_lookup_local_port(port_id);
	if (!port_ptr) {
		pr_err("%s: Local port %d not present\n", __func__, port_id);
		mutex_unlock(&local_ports_lock);
		release_pkt(pkt);
		return -ENODEV;
	}

	mutex_lock(&port_ptr->port_rx_q_lock);
	wake_lock(&port_ptr->port_rx_wake_lock);
	list_add_tail(&pkt->list, &port_ptr->port_rx_q);
	wake_up(&port_ptr->port_rx_wait_q);
	mutex_unlock(&port_ptr->port_rx_q_lock);
	mutex_unlock(&local_ports_lock);

	return pkt->length;
}

static int msm_ipc_router_write_pkt(struct msm_ipc_port *src,
				struct msm_ipc_router_remote_port *rport_ptr,
				struct rr_packet *pkt)
{
	struct sk_buff *head_skb;
	struct rr_header *hdr;
	struct msm_ipc_router_xprt_info *xprt_info;
	struct msm_ipc_routing_table_entry *rt_entry;
	int ret;
	DEFINE_WAIT(__wait);

	if (!rport_ptr || !src || !pkt)
		return -EINVAL;

	head_skb = skb_peek(pkt->pkt_fragment_q);
	if (!head_skb) {
		pr_err("%s: pkt_fragment_q is empty\n", __func__);
		return -EINVAL;
	}
	hdr = (struct rr_header *)skb_push(head_skb, IPC_ROUTER_HDR_SIZE);
	if (!hdr) {
		pr_err("%s: Prepend Header failed\n", __func__);
		return -ENOMEM;
	}
	hdr->version = IPC_ROUTER_VERSION;
	hdr->type = IPC_ROUTER_CTRL_CMD_DATA;
	hdr->src_node_id = src->this_port.node_id;
	hdr->src_port_id = src->this_port.port_id;
	hdr->size = pkt->length;
	hdr->confirm_rx = 0;
	hdr->dst_node_id = rport_ptr->node_id;
	hdr->dst_port_id = rport_ptr->port_id;
	pkt->length += IPC_ROUTER_HDR_SIZE;

	for (;;) {
		prepare_to_wait(&rport_ptr->quota_wait, &__wait,
				TASK_INTERRUPTIBLE);
		mutex_lock(&rport_ptr->quota_lock);
		if (rport_ptr->restart_state != RESTART_NORMAL)
			break;
		if (rport_ptr->tx_quota_cnt <
		     IPC_ROUTER_DEFAULT_RX_QUOTA)
			break;
		if (signal_pending(current))
			break;
		mutex_unlock(&rport_ptr->quota_lock);
		schedule();
	}
	finish_wait(&rport_ptr->quota_wait, &__wait);

	if (rport_ptr->restart_state != RESTART_NORMAL) {
		mutex_unlock(&rport_ptr->quota_lock);
		return -ENETRESET;
	}
	if (signal_pending(current)) {
		mutex_unlock(&rport_ptr->quota_lock);
		return -ERESTARTSYS;
	}
	rport_ptr->tx_quota_cnt++;
	if (rport_ptr->tx_quota_cnt == IPC_ROUTER_DEFAULT_RX_QUOTA)
		hdr->confirm_rx = 1;
	mutex_unlock(&rport_ptr->quota_lock);

	mutex_lock(&routing_table_lock);
	rt_entry = lookup_routing_table(hdr->dst_node_id);
	if (!rt_entry || !rt_entry->xprt_info) {
		mutex_unlock(&routing_table_lock);
		pr_err("%s: Remote node %d not up\n",
			__func__, hdr->dst_node_id);
		return -ENODEV;
	}
	mutex_lock(&rt_entry->lock);
	xprt_info = rt_entry->xprt_info;
	mutex_lock(&xprt_info->tx_lock);
	ret = xprt_info->xprt->write(pkt, pkt->length, xprt_info->xprt);
	mutex_unlock(&xprt_info->tx_lock);
	mutex_unlock(&rt_entry->lock);
	mutex_unlock(&routing_table_lock);

	if (ret < 0) {
		pr_err("%s: Write on XPRT failed\n", __func__);
		return ret;
	}

	RAW_HDR("[w rr_h] "
		"ver=%i,type=%s,src_nid=%08x,src_port_id=%08x,"
		"confirm_rx=%i,size=%3i,dst_pid=%08x,dst_cid=%08x\n",
		hdr->version, type_to_str(hdr->type),
		hdr->src_node_id, hdr->src_port_id,
		hdr->confirm_rx, hdr->size,
		hdr->dst_node_id, hdr->dst_port_id);

#if defined(CONFIG_MSM_SMD_LOGGING)
#if defined(DEBUG)
	if (msm_ipc_router_debug_mask & SMEM_LOG) {
		smem_log_event((SMEM_LOG_PROC_ID_APPS |
			SMEM_LOG_RPC_ROUTER_EVENT_BASE |
			IPC_ROUTER_LOG_EVENT_TX),
			(hdr->src_node_id << 24) |
			(hdr->src_port_id & 0xffffff),
			(hdr->dst_node_id << 24) |
			(hdr->dst_port_id & 0xffffff),
			(hdr->type << 24) | (hdr->confirm_rx << 16) |
			(hdr->size & 0xffff));
	}
#endif
#endif

	return pkt->length;
}

int msm_ipc_router_send_to(struct msm_ipc_port *src,
			   struct sk_buff_head *data,
			   struct msm_ipc_addr *dest)
{
	uint32_t dst_node_id = 0, dst_port_id = 0;
	struct msm_ipc_server *server;
	struct msm_ipc_server_port *server_port;
	struct msm_ipc_router_remote_port *rport_ptr = NULL;
	struct rr_packet *pkt;
	int ret;

	if (!src || !data || !dest) {
		pr_err("%s: Invalid Parameters\n", __func__);
		return -EINVAL;
	}

	/* Resolve Address*/
	if (dest->addrtype == MSM_IPC_ADDR_ID) {
		dst_node_id = dest->addr.port_addr.node_id;
		dst_port_id = dest->addr.port_addr.port_id;
	} else if (dest->addrtype == MSM_IPC_ADDR_NAME) {
		server = msm_ipc_router_lookup_server(
					dest->addr.port_name.service,
					dest->addr.port_name.instance,
					0, 0);
		if (!server) {
			pr_err("%s: Destination not reachable\n", __func__);
			return -ENODEV;
		}
		mutex_lock(&server_list_lock);
		server_port = list_first_entry(&server->server_port_list,
					       struct msm_ipc_server_port,
					       list);
		dst_node_id = server_port->server_addr.node_id;
		dst_port_id = server_port->server_addr.port_id;
		mutex_unlock(&server_list_lock);
	}
	if (dst_node_id == IPC_ROUTER_NID_LOCAL) {
		ret = loopback_data(src, dst_port_id, data);
		return ret;
	}

	/* Achieve Flow control */
	rport_ptr = msm_ipc_router_lookup_remote_port(dst_node_id,
						      dst_port_id);
	if (!rport_ptr) {
		pr_err("%s: Could not create remote port\n", __func__);
		return -ENOMEM;
	}

	pkt = create_pkt(data);
	if (!pkt) {
		pr_err("%s: Pkt creation failed\n", __func__);
		return -ENOMEM;
	}

	ret = msm_ipc_router_write_pkt(src, rport_ptr, pkt);
	release_pkt(pkt);

	return ret;
}

int msm_ipc_router_read(struct msm_ipc_port *port_ptr,
			struct sk_buff_head **data,
			size_t buf_len)
{
	struct rr_packet *pkt;
	int ret;

	if (!port_ptr || !data)
		return -EINVAL;

	mutex_lock(&port_ptr->port_rx_q_lock);
	if (list_empty(&port_ptr->port_rx_q)) {
		mutex_unlock(&port_ptr->port_rx_q_lock);
		return -EAGAIN;
	}

	pkt = list_first_entry(&port_ptr->port_rx_q, struct rr_packet, list);
	if ((buf_len) && ((pkt->length - IPC_ROUTER_HDR_SIZE) > buf_len)) {
		mutex_unlock(&port_ptr->port_rx_q_lock);
		return -ETOOSMALL;
	}
	list_del(&pkt->list);
	if (list_empty(&port_ptr->port_rx_q))
		wake_unlock(&port_ptr->port_rx_wake_lock);
	*data = pkt->pkt_fragment_q;
	ret = pkt->length;
	kfree(pkt);
	mutex_unlock(&port_ptr->port_rx_q_lock);

	return ret;
}

int msm_ipc_router_recv_from(struct msm_ipc_port *port_ptr,
			     struct sk_buff_head **data,
			     struct msm_ipc_addr *src,
			     unsigned long timeout)
{
	int ret, data_len, align_size;
	struct sk_buff *temp_skb;
	struct rr_header *hdr = NULL;

	if (!port_ptr || !data) {
		pr_err("%s: Invalid pointers being passed\n", __func__);
		return -EINVAL;
	}

	*data = NULL;
	mutex_lock(&port_ptr->port_rx_q_lock);
	while (list_empty(&port_ptr->port_rx_q)) {
		mutex_unlock(&port_ptr->port_rx_q_lock);
		if (timeout < 0) {
			ret = wait_event_interruptible(
					port_ptr->port_rx_wait_q,
					!list_empty(&port_ptr->port_rx_q));
			if (ret)
				return ret;
		} else if (timeout > 0) {
			timeout = wait_event_interruptible_timeout(
					port_ptr->port_rx_wait_q,
					!list_empty(&port_ptr->port_rx_q),
					timeout);
			if (timeout < 0)
				return -EFAULT;
		}
		if (timeout == 0)
			return -ETIMEDOUT;
		mutex_lock(&port_ptr->port_rx_q_lock);
	}
	mutex_unlock(&port_ptr->port_rx_q_lock);

	ret = msm_ipc_router_read(port_ptr, data, 0);
	if (ret <= 0 || !(*data))
		return ret;

	temp_skb = skb_peek(*data);
	hdr = (struct rr_header *)(temp_skb->data);
	if (src) {
		src->addrtype = MSM_IPC_ADDR_ID;
		src->addr.port_addr.node_id = hdr->src_node_id;
		src->addr.port_addr.port_id = hdr->src_port_id;
	}

	data_len = hdr->size;
	skb_pull(temp_skb, IPC_ROUTER_HDR_SIZE);
	align_size = ALIGN_SIZE(data_len);
	if (align_size) {
		temp_skb = skb_peek_tail(*data);
		skb_trim(temp_skb, (temp_skb->len - align_size));
	}
	return data_len;
}

struct msm_ipc_port *msm_ipc_router_create_port(
	void (*notify)(unsigned event, void *data, void *addr, void *priv),
	void *priv)
{
	struct msm_ipc_port *port_ptr;

	port_ptr = msm_ipc_router_create_raw_port(NULL, notify, priv);
	if (!port_ptr)
		pr_err("%s: port_ptr alloc failed\n", __func__);

	return port_ptr;
}

int msm_ipc_router_close_port(struct msm_ipc_port *port_ptr)
{
	union rr_control_msg msg;
	struct rr_packet *pkt, *temp_pkt;
	struct msm_ipc_server *server;

	if (!port_ptr)
		return -EINVAL;

	if (port_ptr->type == SERVER_PORT || port_ptr->type == CLIENT_PORT) {
		mutex_lock(&local_ports_lock);
		list_del(&port_ptr->list);
		mutex_unlock(&local_ports_lock);

		if (port_ptr->type == SERVER_PORT) {
			msg.cmd = IPC_ROUTER_CTRL_CMD_REMOVE_SERVER;
			msg.srv.service = port_ptr->port_name.service;
			msg.srv.instance = port_ptr->port_name.instance;
			msg.srv.node_id = port_ptr->this_port.node_id;
			msg.srv.port_id = port_ptr->this_port.port_id;
			RR("x REMOVE_SERVER Name=%d:%08x Id=%d:%08x\n",
			   msg.srv.service, msg.srv.instance,
			   msg.srv.node_id, msg.srv.port_id);
		} else if (port_ptr->type == CLIENT_PORT) {
			msg.cmd = IPC_ROUTER_CTRL_CMD_REMOVE_CLIENT;
			msg.cli.node_id = port_ptr->this_port.node_id;
			msg.cli.port_id = port_ptr->this_port.port_id;
			RR("x REMOVE_CLIENT id=%d:%08x\n",
			   msg.cli.node_id, msg.cli.port_id);
		}
		broadcast_ctl_msg(&msg);
		broadcast_ctl_msg_locally(&msg);
	} else if (port_ptr->type == CONTROL_PORT) {
		mutex_lock(&control_ports_lock);
		list_del(&port_ptr->list);
		mutex_unlock(&control_ports_lock);
	}

	mutex_lock(&port_ptr->port_rx_q_lock);
	list_for_each_entry_safe(pkt, temp_pkt, &port_ptr->port_rx_q, list) {
		list_del(&pkt->list);
		release_pkt(pkt);
	}
	mutex_unlock(&port_ptr->port_rx_q_lock);

	if (port_ptr->type == SERVER_PORT) {
		server = msm_ipc_router_lookup_server(
				port_ptr->port_name.service,
				port_ptr->port_name.instance,
				port_ptr->this_port.node_id,
				port_ptr->this_port.port_id);
		if (server)
			msm_ipc_router_destroy_server(server,
				port_ptr->this_port.node_id,
				port_ptr->this_port.port_id);
	}

	wake_lock_destroy(&port_ptr->port_rx_wake_lock);
	kfree(port_ptr);
	return 0;
}

int msm_ipc_router_get_curr_pkt_size(struct msm_ipc_port *port_ptr)
{
	struct rr_packet *pkt;
	int rc = 0;

	if (!port_ptr)
		return -EINVAL;

	mutex_lock(&port_ptr->port_rx_q_lock);
	if (!list_empty(&port_ptr->port_rx_q)) {
		pkt = list_first_entry(&port_ptr->port_rx_q,
					struct rr_packet, list);
		rc = pkt->length;
	}
	mutex_unlock(&port_ptr->port_rx_q_lock);

	return rc;
}

int msm_ipc_router_bind_control_port(struct msm_ipc_port *port_ptr)
{
	if (!port_ptr)
		return -EINVAL;

	mutex_lock(&local_ports_lock);
	list_del(&port_ptr->list);
	mutex_unlock(&local_ports_lock);
	port_ptr->type = CONTROL_PORT;
	mutex_lock(&control_ports_lock);
	list_add_tail(&port_ptr->list, &control_ports);
	mutex_unlock(&control_ports_lock);

	return 0;
}

int msm_ipc_router_lookup_server_name(struct msm_ipc_port_name *srv_name,
				struct msm_ipc_server_info *srv_info,
				int num_entries_in_array,
				uint32_t lookup_mask)
{
	struct msm_ipc_server *server;
	struct msm_ipc_server_port *server_port;
	int key, i = 0; /*num_entries_found*/

	if (!srv_name) {
		pr_err("%s: Invalid srv_name\n", __func__);
		return -EINVAL;
	}

	if (num_entries_in_array && !srv_info) {
		pr_err("%s: srv_info NULL\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&server_list_lock);
	if (!lookup_mask)
		lookup_mask = 0xFFFFFFFF;
	for (key = 0; key < SRV_HASH_SIZE; key++) {
		list_for_each_entry(server, &server_list[key], list) {
			if ((server->name.service != srv_name->service) ||
			    ((server->name.instance & lookup_mask) !=
				srv_name->instance))
				continue;

			list_for_each_entry(server_port,
				&server->server_port_list, list) {
				if (i < num_entries_in_array) {
					srv_info[i].node_id =
					  server_port->server_addr.node_id;
					srv_info[i].port_id =
					  server_port->server_addr.port_id;
					srv_info[i].service =
					  server->name.service;
					srv_info[i].instance =
					  server->name.instance;
				}
				i++;
			}
		}
	}
	mutex_unlock(&server_list_lock);

	return i;
}

int msm_ipc_router_close(void)
{
	struct msm_ipc_router_xprt_info *xprt_info, *tmp_xprt_info;

	mutex_lock(&xprt_info_list_lock);
	list_for_each_entry_safe(xprt_info, tmp_xprt_info,
				 &xprt_info_list, list) {
		xprt_info->xprt->close(xprt_info->xprt);
		list_del(&xprt_info->list);
		kfree(xprt_info);
	}
	mutex_unlock(&xprt_info_list_lock);
	return 0;
}

#if defined(CONFIG_DEBUG_FS)
static int dump_routing_table(char *buf, int max)
{
	int i = 0, j;
	struct msm_ipc_routing_table_entry *rt_entry;

	for (j = 0; j < RT_HASH_SIZE; j++) {
		mutex_lock(&routing_table_lock);
		list_for_each_entry(rt_entry, &routing_table[j], list) {
			mutex_lock(&rt_entry->lock);
			i += scnprintf(buf + i, max - i,
				       "Node Id: 0x%08x\n", rt_entry->node_id);
			if (j == IPC_ROUTER_NID_LOCAL) {
				i += scnprintf(buf + i, max - i,
				       "XPRT Name: Loopback\n");
				i += scnprintf(buf + i, max - i,
				       "Next Hop: %d\n", rt_entry->node_id);
			} else {
				i += scnprintf(buf + i, max - i,
					"XPRT Name: %s\n",
					rt_entry->xprt_info->xprt->name);
				i += scnprintf(buf + i, max - i,
					"Next Hop: 0x%08x\n",
					rt_entry->xprt_info->remote_node_id);
			}
			i += scnprintf(buf + i, max - i, "\n");
			mutex_unlock(&rt_entry->lock);
		}
		mutex_unlock(&routing_table_lock);
	}

	return i;
}

static int dump_xprt_info(char *buf, int max)
{
	int i = 0;
	struct msm_ipc_router_xprt_info *xprt_info;

	mutex_lock(&xprt_info_list_lock);
	list_for_each_entry(xprt_info, &xprt_info_list, list) {
		i += scnprintf(buf + i, max - i, "XPRT Name: %s\n",
			       xprt_info->xprt->name);
		i += scnprintf(buf + i, max - i, "Link Id: %d\n",
			       xprt_info->xprt->link_id);
		i += scnprintf(buf + i, max - i, "Initialized: %s\n",
			       (xprt_info->initialized ? "Y" : "N"));
		i += scnprintf(buf + i, max - i, "Remote Node Id: 0x%08x\n",
			       xprt_info->remote_node_id);
		i += scnprintf(buf + i, max - i, "\n");
	}
	mutex_unlock(&xprt_info_list_lock);

	return i;
}

static int dump_servers(char *buf, int max)
{
	int i = 0, j;
	struct msm_ipc_server *server;
	struct msm_ipc_server_port *server_port;

	mutex_lock(&server_list_lock);
	for (j = 0; j < SRV_HASH_SIZE; j++) {
		list_for_each_entry(server, &server_list[j], list) {
			list_for_each_entry(server_port,
					    &server->server_port_list,
					    list) {
				i += scnprintf(buf + i, max - i, "Service: "
					"0x%08x\n", server->name.service);
				i += scnprintf(buf + i, max - i, "Instance: "
					"0x%08x\n", server->name.instance);
				i += scnprintf(buf + i, max - i,
					"Node_id: 0x%08x\n",
					server_port->server_addr.node_id);
				i += scnprintf(buf + i, max - i,
					"Port_id: 0x%08x\n",
					server_port->server_addr.port_id);
				i += scnprintf(buf + i, max - i, "\n");
			}
		}
	}
	mutex_unlock(&server_list_lock);

	return i;
}

static int dump_remote_ports(char *buf, int max)
{
	int i = 0, j, k;
	struct msm_ipc_router_remote_port *rport_ptr;
	struct msm_ipc_routing_table_entry *rt_entry;

	for (j = 0; j < RT_HASH_SIZE; j++) {
		mutex_lock(&routing_table_lock);
		list_for_each_entry(rt_entry, &routing_table[j], list) {
			mutex_lock(&rt_entry->lock);
			for (k = 0; k < RP_HASH_SIZE; k++) {
				list_for_each_entry(rport_ptr,
					&rt_entry->remote_port_list[k],
					list) {
					i += scnprintf(buf + i, max - i,
						"Node_id: 0x%08x\n",
						rport_ptr->node_id);
					i += scnprintf(buf + i, max - i,
						"Port_id: 0x%08x\n",
						rport_ptr->port_id);
					i += scnprintf(buf + i, max - i,
						"Quota_cnt: %d\n",
						rport_ptr->tx_quota_cnt);
				i += scnprintf(buf + i, max - i, "\n");
				}
			}
			mutex_unlock(&rt_entry->lock);
		}
		mutex_unlock(&routing_table_lock);
	}

	return i;
}

static int dump_control_ports(char *buf, int max)
{
	int i = 0;
	struct msm_ipc_port *port_ptr;

	mutex_lock(&control_ports_lock);
	list_for_each_entry(port_ptr, &control_ports, list) {
		i += scnprintf(buf + i, max - i, "Node_id: 0x%08x\n",
			       port_ptr->this_port.node_id);
		i += scnprintf(buf + i, max - i, "Port_id: 0x%08x\n",
			       port_ptr->this_port.port_id);
		i += scnprintf(buf + i, max - i, "\n");
	}
	mutex_unlock(&control_ports_lock);

	return i;
}

static int dump_local_ports(char *buf, int max)
{
	int i = 0, j;
	unsigned long flags;
	struct msm_ipc_port *port_ptr;

	mutex_lock(&local_ports_lock);
	for (j = 0; j < LP_HASH_SIZE; j++) {
		list_for_each_entry(port_ptr, &local_ports[j], list) {
			spin_lock_irqsave(&port_ptr->port_lock, flags);
			i += scnprintf(buf + i, max - i, "Node_id: 0x%08x\n",
				       port_ptr->this_port.node_id);
			i += scnprintf(buf + i, max - i, "Port_id: 0x%08x\n",
				       port_ptr->this_port.port_id);
			i += scnprintf(buf + i, max - i, "# pkts tx'd %d\n",
				       port_ptr->num_tx);
			i += scnprintf(buf + i, max - i, "# pkts rx'd %d\n",
				       port_ptr->num_rx);
			i += scnprintf(buf + i, max - i, "# bytes tx'd %ld\n",
				       port_ptr->num_tx_bytes);
			i += scnprintf(buf + i, max - i, "# bytes rx'd %ld\n",
				       port_ptr->num_rx_bytes);
			spin_unlock_irqrestore(&port_ptr->port_lock, flags);
			i += scnprintf(buf + i, max - i, "\n");
		}
	}
	mutex_unlock(&local_ports_lock);

	return i;
}

#define DEBUG_BUFMAX 4096
static char debug_buffer[DEBUG_BUFMAX];

static ssize_t debug_read(struct file *file, char __user *buf,
			  size_t count, loff_t *ppos)
{
	int (*fill)(char *buf, int max) = file->private_data;
	int bsize = fill(debug_buffer, DEBUG_BUFMAX);
	return simple_read_from_buffer(buf, count, ppos, debug_buffer, bsize);
}

static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static const struct file_operations debug_ops = {
	.read = debug_read,
	.open = debug_open,
};

static void debug_create(const char *name, mode_t mode,
			 struct dentry *dent,
			 int (*fill)(char *buf, int max))
{
	debugfs_create_file(name, mode, dent, fill, &debug_ops);
}

static void debugfs_init(void)
{
	struct dentry *dent;

	dent = debugfs_create_dir("msm_ipc_router", 0);
	if (IS_ERR(dent))
		return;

	debug_create("dump_local_ports", 0444, dent,
		      dump_local_ports);
	debug_create("dump_remote_ports", 0444, dent,
		      dump_remote_ports);
	debug_create("dump_control_ports", 0444, dent,
		      dump_control_ports);
	debug_create("dump_servers", 0444, dent,
		      dump_servers);
	debug_create("dump_xprt_info", 0444, dent,
		      dump_xprt_info);
	debug_create("dump_routing_table", 0444, dent,
		      dump_routing_table);
}

#else
static void debugfs_init(void) {}
#endif

static int msm_ipc_router_add_xprt(struct msm_ipc_router_xprt *xprt)
{
	struct msm_ipc_router_xprt_info *xprt_info;
	struct msm_ipc_routing_table_entry *rt_entry;

	xprt_info = kmalloc(sizeof(struct msm_ipc_router_xprt_info),
			    GFP_KERNEL);
	if (!xprt_info)
		return -ENOMEM;

	xprt_info->xprt = xprt;
	xprt_info->initialized = 0;
	xprt_info->remote_node_id = -1;
	INIT_LIST_HEAD(&xprt_info->pkt_list);
	init_waitqueue_head(&xprt_info->read_wait);
	mutex_init(&xprt_info->rx_lock);
	mutex_init(&xprt_info->tx_lock);
	wake_lock_init(&xprt_info->wakelock,
			WAKE_LOCK_SUSPEND, xprt->name);
	xprt_info->need_len = 0;
	xprt_info->abort_data_read = 0;
	INIT_WORK(&xprt_info->read_data, do_read_data);
	INIT_LIST_HEAD(&xprt_info->list);

	xprt_info->workqueue = create_singlethread_workqueue(xprt->name);
	if (!xprt_info->workqueue) {
		kfree(xprt_info);
		return -ENOMEM;
	}

	if (!strcmp(xprt->name, "msm_ipc_router_loopback_xprt")) {
		xprt_info->remote_node_id = IPC_ROUTER_NID_LOCAL;
		xprt_info->initialized = 1;
	}

	mutex_lock(&xprt_info_list_lock);
	list_add_tail(&xprt_info->list, &xprt_info_list);
	mutex_unlock(&xprt_info_list_lock);

	mutex_lock(&routing_table_lock);
	if (!routing_table_inited) {
		init_routing_table();
		rt_entry = alloc_routing_table_entry(IPC_ROUTER_NID_LOCAL);
		add_routing_table_entry(rt_entry);
		routing_table_inited = 1;
	}
	mutex_unlock(&routing_table_lock);

	queue_work(xprt_info->workqueue, &xprt_info->read_data);

	xprt->priv = xprt_info;

	return 0;
}

static void msm_ipc_router_remove_xprt(struct msm_ipc_router_xprt *xprt)
{
	struct msm_ipc_router_xprt_info *xprt_info;

	if (xprt && xprt->priv) {
		xprt_info = xprt->priv;

		xprt_info->abort_data_read = 1;
		wake_up(&xprt_info->read_wait);

		mutex_lock(&xprt_info_list_lock);
		list_del(&xprt_info->list);
		mutex_unlock(&xprt_info_list_lock);

		flush_workqueue(xprt_info->workqueue);
		destroy_workqueue(xprt_info->workqueue);
		wake_lock_destroy(&xprt_info->wakelock);

		xprt->priv = 0;
		kfree(xprt_info);
	}
}


struct msm_ipc_router_xprt_work {
	struct msm_ipc_router_xprt *xprt;
	struct work_struct work;
};

static void xprt_open_worker(struct work_struct *work)
{
	struct msm_ipc_router_xprt_work *xprt_work =
		container_of(work, struct msm_ipc_router_xprt_work, work);

	msm_ipc_router_add_xprt(xprt_work->xprt);
	kfree(xprt_work);
}

static void xprt_close_worker(struct work_struct *work)
{
	struct msm_ipc_router_xprt_work *xprt_work =
		container_of(work, struct msm_ipc_router_xprt_work, work);

	modem_reset_cleanup(xprt_work->xprt->priv);
	msm_ipc_router_remove_xprt(xprt_work->xprt);

	if (atomic_dec_return(&pending_close_count) == 0)
		wake_up(&subsystem_restart_wait);

	kfree(xprt_work);
}

void msm_ipc_router_xprt_notify(struct msm_ipc_router_xprt *xprt,
				unsigned event,
				void *data)
{
	struct msm_ipc_router_xprt_info *xprt_info = xprt->priv;
	struct msm_ipc_router_xprt_work *xprt_work;
	struct rr_packet *pkt;
	unsigned long ret;

	if (!msm_ipc_router_workqueue) {
		ret = wait_for_completion_timeout(&msm_ipc_local_router_up,
						  IPC_ROUTER_INIT_TIMEOUT);
		if (!ret || !msm_ipc_router_workqueue) {
			pr_err("%s: IPC Router not initialized\n", __func__);
			return;
		}
	}

	switch (event) {
	case IPC_ROUTER_XPRT_EVENT_OPEN:
		D("open event for '%s'\n", xprt->name);
		xprt_work = kmalloc(sizeof(struct msm_ipc_router_xprt_work),
				GFP_ATOMIC);
		xprt_work->xprt = xprt;
		INIT_WORK(&xprt_work->work, xprt_open_worker);
		queue_work(msm_ipc_router_workqueue, &xprt_work->work);
		break;

	case IPC_ROUTER_XPRT_EVENT_CLOSE:
		D("close event for '%s'\n", xprt->name);
		atomic_inc(&pending_close_count);
		xprt_work = kmalloc(sizeof(struct msm_ipc_router_xprt_work),
				GFP_ATOMIC);
		xprt_work->xprt = xprt;
		INIT_WORK(&xprt_work->work, xprt_close_worker);
		queue_work(msm_ipc_router_workqueue, &xprt_work->work);
		break;
	}

	if (!data)
		return;

	while (!xprt_info) {
		msleep(100);
		xprt_info = xprt->priv;
	}

	pkt = clone_pkt((struct rr_packet *)data);
	if (!pkt)
		return;

	mutex_lock(&xprt_info->rx_lock);
	list_add_tail(&pkt->list, &xprt_info->pkt_list);
	wake_lock(&xprt_info->wakelock);
	wake_up(&xprt_info->read_wait);
	mutex_unlock(&xprt_info->rx_lock);
}

static int modem_restart_notifier_cb(struct notifier_block *this,
				unsigned long code,
				void *data);
static struct notifier_block msm_ipc_router_nb = {
	.notifier_call = modem_restart_notifier_cb,
};

static int modem_restart_notifier_cb(struct notifier_block *this,
				unsigned long code,
				void *data)
{
	switch (code) {
	case SUBSYS_BEFORE_SHUTDOWN:
		D("%s: SUBSYS_BEFORE_SHUTDOWN\n", __func__);
		break;

	case SUBSYS_BEFORE_POWERUP:
		D("%s: waiting for RPC restart to complete\n", __func__);
		wait_event(subsystem_restart_wait,
			atomic_read(&pending_close_count) == 0);
		D("%s: finished restart wait\n", __func__);
		break;

	default:
		break;
	}

	return NOTIFY_DONE;
}

static void *restart_notifier_handle;
static __init int msm_ipc_router_modem_restart_late_init(void)
{
	restart_notifier_handle = subsys_notif_register_notifier("modem",
							&msm_ipc_router_nb);
	return 0;
}
late_initcall(msm_ipc_router_modem_restart_late_init);

static int __init msm_ipc_router_init(void)
{
	int i, ret;
	struct msm_ipc_routing_table_entry *rt_entry;

	msm_ipc_router_debug_mask |= SMEM_LOG;
	msm_ipc_router_workqueue =
		create_singlethread_workqueue("msm_ipc_router");
	if (!msm_ipc_router_workqueue)
		return -ENOMEM;

	debugfs_init();

	for (i = 0; i < SRV_HASH_SIZE; i++)
		INIT_LIST_HEAD(&server_list[i]);

	for (i = 0; i < LP_HASH_SIZE; i++)
		INIT_LIST_HEAD(&local_ports[i]);

	mutex_lock(&routing_table_lock);
	if (!routing_table_inited) {
		init_routing_table();
		rt_entry = alloc_routing_table_entry(IPC_ROUTER_NID_LOCAL);
		add_routing_table_entry(rt_entry);
		routing_table_inited = 1;
	}
	mutex_unlock(&routing_table_lock);

	init_waitqueue_head(&newserver_wait);
	init_waitqueue_head(&subsystem_restart_wait);
	ret = msm_ipc_router_init_sockets();
	if (ret < 0)
		pr_err("%s: Init sockets failed\n", __func__);

	complete_all(&msm_ipc_local_router_up);
	return ret;
}

module_init(msm_ipc_router_init);
MODULE_DESCRIPTION("MSM IPC Router");
MODULE_LICENSE("GPL v2");
