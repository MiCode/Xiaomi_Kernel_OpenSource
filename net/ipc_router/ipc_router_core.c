/* Copyright (c) 2011-2016, 2018, The Linux Foundation. All rights reserved.
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
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/rwsem.h>
#include <linux/ipc_logging.h>
#include <linux/uaccess.h>
#include <linux/ipc_router.h>
#include <linux/ipc_router_xprt.h>
#include <linux/kref.h>
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/subsystem_restart.h>

#include <asm/byteorder.h>

#include <soc/qcom/smem_log.h>

#include "ipc_router_private.h"
#include "ipc_router_security.h"

enum {
	SMEM_LOG = 1U << 0,
	RTR_DBG = 1U << 1,
};

static int msm_ipc_router_debug_mask;
module_param_named(debug_mask, msm_ipc_router_debug_mask,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);
#define MODULE_NAME "ipc_router"

#define IPC_RTR_INFO_PAGES 6

#define IPC_RTR_INFO(log_ctx, x...) do { \
if (log_ctx) \
	ipc_log_string(log_ctx, x); \
if (msm_ipc_router_debug_mask & RTR_DBG) \
	pr_info("[IPCRTR] "x); \
} while (0)

#define IPC_ROUTER_LOG_EVENT_TX         0x01
#define IPC_ROUTER_LOG_EVENT_RX         0x02
#define IPC_ROUTER_LOG_EVENT_TX_ERR     0x03
#define IPC_ROUTER_LOG_EVENT_RX_ERR     0x04
#define IPC_ROUTER_DUMMY_DEST_NODE	0xFFFFFFFF

#define ipc_port_sk(port) ((struct sock *)(port))

static LIST_HEAD(control_ports);
static DECLARE_RWSEM(control_ports_lock_lha5);

#define LP_HASH_SIZE 32
static struct list_head local_ports[LP_HASH_SIZE];
static DECLARE_RWSEM(local_ports_lock_lhc2);

/* Server info is organized as a hash table. The server's service ID is
 * used to index into the hash table. The instance ID of most of the servers
 * are 1 or 2. The service IDs are well distributed compared to the instance
 * IDs and hence choosing service ID to index into this hash table optimizes
 * the hash table operations like add, lookup, destroy.
 */
#define SRV_HASH_SIZE 32
static struct list_head server_list[SRV_HASH_SIZE];
static DECLARE_RWSEM(server_list_lock_lha2);

struct msm_ipc_server {
	struct list_head list;
	struct kref ref;
	struct msm_ipc_port_name name;
	char pdev_name[32];
	int next_pdev_id;
	int synced_sec_rule;
	struct list_head server_port_list;
};

struct msm_ipc_server_port {
	struct list_head list;
	struct platform_device *pdev;
	struct msm_ipc_port_addr server_addr;
	struct msm_ipc_router_xprt_info *xprt_info;
};

struct msm_ipc_resume_tx_port {
	struct list_head list;
	uint32_t port_id;
	uint32_t node_id;
};

struct ipc_router_conn_info {
	struct list_head list;
	uint32_t port_id;
};

enum {
	RESET = 0,
	VALID = 1,
};

#define RP_HASH_SIZE 32
struct msm_ipc_router_remote_port {
	struct list_head list;
	struct kref ref;
	struct mutex rport_lock_lhb2;
	uint32_t node_id;
	uint32_t port_id;
	int status;
	uint32_t tx_quota_cnt;
	struct list_head resume_tx_port_list;
	struct list_head conn_info_list;
	void *sec_rule;
	struct msm_ipc_server *server;
};

struct msm_ipc_router_xprt_info {
	struct list_head list;
	struct msm_ipc_router_xprt *xprt;
	uint32_t remote_node_id;
	uint32_t initialized;
	struct list_head pkt_list;
	struct wakeup_source ws;
	struct mutex rx_lock_lhb2;
	struct mutex tx_lock_lhb2;
	uint32_t need_len;
	uint32_t abort_data_read;
	struct work_struct read_data;
	struct workqueue_struct *workqueue;
	void *log_ctx;
	struct kref ref;
	struct completion ref_complete;
};

#define RT_HASH_SIZE 4
struct msm_ipc_routing_table_entry {
	struct list_head list;
	struct kref ref;
	uint32_t node_id;
	uint32_t neighbor_node_id;
	struct list_head remote_port_list[RP_HASH_SIZE];
	struct msm_ipc_router_xprt_info *xprt_info;
	struct rw_semaphore lock_lha4;
	unsigned long num_tx_bytes;
	unsigned long num_rx_bytes;
};

#define LOG_CTX_NAME_LEN 32
struct ipc_rtr_log_ctx {
	struct list_head list;
	char log_ctx_name[LOG_CTX_NAME_LEN];
	void *log_ctx;
};

static struct list_head routing_table[RT_HASH_SIZE];
static DECLARE_RWSEM(routing_table_lock_lha3);
static int routing_table_inited;

static void do_read_data(struct work_struct *work);

static LIST_HEAD(xprt_info_list);
static DECLARE_RWSEM(xprt_info_list_lock_lha5);

static DEFINE_MUTEX(log_ctx_list_lock_lha0);
static LIST_HEAD(log_ctx_list);
static DEFINE_MUTEX(ipc_router_init_lock);
static bool is_ipc_router_inited;
static int ipc_router_core_init(void);
#define IPC_ROUTER_INIT_TIMEOUT (10 * HZ)

static uint32_t next_port_id;
static DEFINE_MUTEX(next_port_id_lock_lhc1);
static struct workqueue_struct *msm_ipc_router_workqueue;

static void *local_log_ctx;
static void *ipc_router_get_log_ctx(char *sub_name);
static int process_resume_tx_msg(union rr_control_msg *msg,
				 struct rr_packet *pkt);
static void ipc_router_reset_conn(struct msm_ipc_router_remote_port *rport_ptr);
static int ipc_router_get_xprt_info_ref(
		struct msm_ipc_router_xprt_info *xprt_info);
static void ipc_router_put_xprt_info_ref(
		struct msm_ipc_router_xprt_info *xprt_info);
static void ipc_router_release_xprt_info_ref(struct kref *ref);

struct pil_vote_info {
	void *pil_handle;
	struct work_struct load_work;
	struct work_struct unload_work;
};

#define PIL_SUBSYSTEM_NAME_LEN 32
static char default_peripheral[PIL_SUBSYSTEM_NAME_LEN];

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

/**
 * ipc_router_calc_checksum() - compute the checksum for extended HELLO message
 * @msg:	Reference to the IPC Router HELLO message.
 *
 * Return: Computed checksum value, 0 if msg is NULL.
 */
static uint32_t ipc_router_calc_checksum(union rr_control_msg *msg)
{
	uint32_t checksum = 0;
	int i, len;
	uint16_t upper_nb;
	uint16_t lower_nb;
	void *hello;

	if (!msg)
		return checksum;
	hello = msg;
	len = sizeof(*msg);

	for (i = 0; i < len/IPCR_WORD_SIZE; i++) {
		lower_nb = (*((uint32_t *)hello)) & IPC_ROUTER_CHECKSUM_MASK;
		upper_nb = ((*((uint32_t *)hello)) >> 16) &
				IPC_ROUTER_CHECKSUM_MASK;
		checksum = checksum + upper_nb + lower_nb;
		hello = ((uint32_t *)hello) + 1;
	}
	while (checksum > 0xFFFF)
		checksum = (checksum & IPC_ROUTER_CHECKSUM_MASK) +
				((checksum >> 16) & IPC_ROUTER_CHECKSUM_MASK);

	checksum = ~checksum & IPC_ROUTER_CHECKSUM_MASK;
	return checksum;
}

/**
 * skb_copy_to_log_buf() - copies the required number bytes from the skb_queue
 * @skb_head:	skb_queue head that contains the data.
 * @pl_len:	length of payload need to be copied.
 * @hdr_offset:	length of the header present in first skb
 * @log_buf:	The output buffer which will contain the formatted log string
 *
 * This function copies the first specified number of bytes from the skb_queue
 * to a new buffer and formats them to a string for logging.
 */
static void skb_copy_to_log_buf(struct sk_buff_head *skb_head,
				unsigned int pl_len, unsigned int hdr_offset,
				unsigned char *log_buf)
{
	struct sk_buff *temp_skb;
	unsigned int copied_len = 0, copy_len = 0;
	int remaining;

	if (!skb_head) {
		IPC_RTR_ERR("%s: NULL skb_head\n", __func__);
		return;
	}
	temp_skb = skb_peek(skb_head);
	if (unlikely(!temp_skb || !temp_skb->data)) {
		IPC_RTR_ERR("%s: No SKBs in skb_queue\n", __func__);
		return;
	}

	remaining = temp_skb->len - hdr_offset;
	skb_queue_walk(skb_head, temp_skb) {
		copy_len = remaining < pl_len ? remaining : pl_len;
		memcpy(log_buf + copied_len,
			temp_skb->data + hdr_offset, copy_len);
		copied_len += copy_len;
		hdr_offset = 0;
		if (copied_len == pl_len)
			break;
		remaining = pl_len - remaining;
	}
	return;
}

/**
 * ipc_router_log_msg() - log all data messages exchanged
 * @log_ctx:	IPC Logging context specfic to each transport
 * @xchng_type:	Identifies the data to be a receive or send.
 * @data:	IPC Router data packet or control msg recieved or to be send.
 * @hdr:	Reference to the router header
 * @port_ptr:	Local IPC Router port.
 * @rport_ptr:	Remote IPC Router port
 *
 * This function builds the log message that would be passed on to the IPC
 * logging framework. The data messages that would be passed corresponds to
 * the information that is exchanged between the IPC Router and it's clients.
 */
static void ipc_router_log_msg(void *log_ctx, uint32_t xchng_type,
			void *data, struct rr_header_v1 *hdr,
			struct msm_ipc_port *port_ptr,
			struct msm_ipc_router_remote_port *rport_ptr)
{
	struct sk_buff_head *skb_head = NULL;
	union rr_control_msg *msg = NULL;
	struct rr_packet *pkt = NULL;
	uint64_t pl_buf = 0;
	struct sk_buff *skb;
	uint32_t buf_len = 8;
	uint32_t svcId = 0;
	uint32_t svcIns = 0;
	unsigned int hdr_offset = 0;
	uint32_t port_type = 0;

	if (!log_ctx || !hdr || !data)
		return;

	if (hdr->type == IPC_ROUTER_CTRL_CMD_DATA) {
		pkt = (struct rr_packet *)data;
		skb_head = pkt->pkt_fragment_q;
		skb = skb_peek(skb_head);
		if (!skb || !skb->data) {
			IPC_RTR_ERR("%s: No SKBs in skb_queue\n", __func__);
			return;
		}

		if (skb_queue_len(skb_head) == 1 && skb->len < 8)
			buf_len = skb->len;
		if (xchng_type == IPC_ROUTER_LOG_EVENT_TX && hdr->dst_node_id
				!= IPC_ROUTER_NID_LOCAL) {
			if (hdr->version == IPC_ROUTER_V1)
				hdr_offset = sizeof(struct rr_header_v1);
			else if (hdr->version == IPC_ROUTER_V2)
				hdr_offset = sizeof(struct rr_header_v2);
		}
		skb_copy_to_log_buf(skb_head, buf_len, hdr_offset,
				    (unsigned char *)&pl_buf);

		if (port_ptr && rport_ptr && (port_ptr->type == CLIENT_PORT)
				&& (rport_ptr->server != NULL)) {
			svcId = rport_ptr->server->name.service;
			svcIns = rport_ptr->server->name.instance;
			port_type = CLIENT_PORT;
		} else if (port_ptr && (port_ptr->type == SERVER_PORT)) {
			svcId = port_ptr->port_name.service;
			svcIns = port_ptr->port_name.instance;
			port_type = SERVER_PORT;
		}
		IPC_RTR_INFO(log_ctx,
			"%s %s %s Len:0x%x T:0x%x CF:0x%x SVC:<0x%x:0x%x> SRC:<0x%x:0x%x> DST:<0x%x:0x%x> DATA: %08x %08x",
			(xchng_type == IPC_ROUTER_LOG_EVENT_RX ? "" :
			(xchng_type == IPC_ROUTER_LOG_EVENT_TX ?
			 current->comm : "")),
			(port_type == CLIENT_PORT ? "CLI" : "SRV"),
			(xchng_type == IPC_ROUTER_LOG_EVENT_RX ? "RX" :
			(xchng_type == IPC_ROUTER_LOG_EVENT_TX ? "TX" :
			(xchng_type == IPC_ROUTER_LOG_EVENT_TX_ERR ? "TX_ERR" :
			(xchng_type == IPC_ROUTER_LOG_EVENT_RX_ERR ? "RX_ERR" :
			 "UNKNOWN")))),
			hdr->size, hdr->type, hdr->control_flag,
			svcId, svcIns, hdr->src_node_id, hdr->src_port_id,
			hdr->dst_node_id, hdr->dst_port_id,
			(unsigned int)pl_buf, (unsigned int)(pl_buf>>32));

	} else {
		msg = (union rr_control_msg *)data;
		if (msg->cmd == IPC_ROUTER_CTRL_CMD_NEW_SERVER ||
			msg->cmd == IPC_ROUTER_CTRL_CMD_REMOVE_SERVER)
			IPC_RTR_INFO(log_ctx,
			"CTL MSG: %s cmd:0x%x SVC:<0x%x:0x%x> ADDR:<0x%x:0x%x>",
			(xchng_type == IPC_ROUTER_LOG_EVENT_RX ? "RX" :
			(xchng_type == IPC_ROUTER_LOG_EVENT_TX ? "TX" :
			(xchng_type == IPC_ROUTER_LOG_EVENT_TX_ERR ? "TX_ERR" :
			(xchng_type == IPC_ROUTER_LOG_EVENT_RX_ERR ? "RX_ERR" :
			 "UNKNOWN")))),
			msg->cmd, msg->srv.service, msg->srv.instance,
			msg->srv.node_id, msg->srv.port_id);
		else if (msg->cmd == IPC_ROUTER_CTRL_CMD_REMOVE_CLIENT ||
				msg->cmd == IPC_ROUTER_CTRL_CMD_RESUME_TX)
			IPC_RTR_INFO(log_ctx,
			"CTL MSG: %s cmd:0x%x ADDR: <0x%x:0x%x>",
			(xchng_type == IPC_ROUTER_LOG_EVENT_RX ? "RX" :
			(xchng_type == IPC_ROUTER_LOG_EVENT_TX ? "TX" : "ERR")),
			msg->cmd, msg->cli.node_id, msg->cli.port_id);
		else if (msg->cmd == IPC_ROUTER_CTRL_CMD_HELLO && hdr)
			IPC_RTR_INFO(log_ctx, "CTL MSG %s cmd:0x%x ADDR:0x%x",
			(xchng_type == IPC_ROUTER_LOG_EVENT_RX ? "RX" :
			(xchng_type == IPC_ROUTER_LOG_EVENT_TX ? "TX" : "ERR")),
			msg->cmd, hdr->src_node_id);
		else
			IPC_RTR_INFO(log_ctx, "%s UNKNOWN cmd:0x%x",
			(xchng_type == IPC_ROUTER_LOG_EVENT_RX ? "RX" :
			(xchng_type == IPC_ROUTER_LOG_EVENT_TX ? "TX" : "ERR")),
			msg->cmd);
	}
}

/* Must be called with routing_table_lock_lha3 locked. */
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

/**
 * create_routing_table_entry() - Lookup and create a routing table entry
 * @node_id: Node ID of the routing table entry to be created.
 * @xprt_info: XPRT through which the node ID is reachable.
 *
 * @return: a reference to the routing table entry on success, NULL on failure.
 */
static struct msm_ipc_routing_table_entry *create_routing_table_entry(
	uint32_t node_id, struct msm_ipc_router_xprt_info *xprt_info)
{
	int i;
	struct msm_ipc_routing_table_entry *rt_entry;
	uint32_t key;

	down_write(&routing_table_lock_lha3);
	rt_entry = lookup_routing_table(node_id);
	if (rt_entry)
		goto out_create_rtentry1;

	rt_entry = kmalloc(sizeof(struct msm_ipc_routing_table_entry),
			   GFP_KERNEL);
	if (!rt_entry) {
		IPC_RTR_ERR("%s: rt_entry allocation failed for %d\n",
			__func__, node_id);
		goto out_create_rtentry2;
	}

	for (i = 0; i < RP_HASH_SIZE; i++)
		INIT_LIST_HEAD(&rt_entry->remote_port_list[i]);
	init_rwsem(&rt_entry->lock_lha4);
	kref_init(&rt_entry->ref);
	rt_entry->node_id = node_id;
	rt_entry->xprt_info = xprt_info;
	if (xprt_info)
		rt_entry->neighbor_node_id = xprt_info->remote_node_id;

	key = (node_id % RT_HASH_SIZE);
	list_add_tail(&rt_entry->list, &routing_table[key]);
out_create_rtentry1:
	kref_get(&rt_entry->ref);
out_create_rtentry2:
	up_write(&routing_table_lock_lha3);
	return rt_entry;
}

/**
 * ipc_router_get_rtentry_ref() - Get a reference to the routing table entry
 * @node_id: Node ID of the routing table entry.
 *
 * @return: a reference to the routing table entry on success, NULL on failure.
 *
 * This function is used to obtain a reference to the rounting table entry
 * corresponding to a node id.
 */
static struct msm_ipc_routing_table_entry *ipc_router_get_rtentry_ref(
	uint32_t node_id)
{
	struct msm_ipc_routing_table_entry *rt_entry;

	down_read(&routing_table_lock_lha3);
	rt_entry = lookup_routing_table(node_id);
	if (rt_entry)
		kref_get(&rt_entry->ref);
	up_read(&routing_table_lock_lha3);
	return rt_entry;
}

/**
 * ipc_router_release_rtentry() - Cleanup and release the routing table entry
 * @ref: Reference to the entry.
 *
 * This function is called when all references to the routing table entry are
 * released.
 */
void ipc_router_release_rtentry(struct kref *ref)
{
	struct msm_ipc_routing_table_entry *rt_entry =
		container_of(ref, struct msm_ipc_routing_table_entry, ref);

	/*
	 * All references to a routing entry will be put only under SSR.
	 * As part of SSR, all the internals of the routing table entry
	 * are cleaned. So just free the routing table entry.
	 */
	kfree(rt_entry);
}

struct rr_packet *rr_read(struct msm_ipc_router_xprt_info *xprt_info)
{
	struct rr_packet *temp_pkt;

	if (!xprt_info)
		return NULL;

	mutex_lock(&xprt_info->rx_lock_lhb2);
	if (xprt_info->abort_data_read) {
		mutex_unlock(&xprt_info->rx_lock_lhb2);
		IPC_RTR_ERR("%s detected SSR & exiting now\n",
			xprt_info->xprt->name);
		return NULL;
	}

	if (list_empty(&xprt_info->pkt_list)) {
		mutex_unlock(&xprt_info->rx_lock_lhb2);
		return NULL;
	}

	temp_pkt = list_first_entry(&xprt_info->pkt_list,
				    struct rr_packet, list);
	list_del(&temp_pkt->list);
	if (list_empty(&xprt_info->pkt_list))
		__pm_relax(&xprt_info->ws);
	mutex_unlock(&xprt_info->rx_lock_lhb2);
	return temp_pkt;
}

struct rr_packet *clone_pkt(struct rr_packet *pkt)
{
	struct rr_packet *cloned_pkt;
	struct sk_buff *temp_skb, *cloned_skb;
	struct sk_buff_head *pkt_fragment_q;

	cloned_pkt = kzalloc(sizeof(struct rr_packet), GFP_KERNEL);
	if (!cloned_pkt) {
		IPC_RTR_ERR("%s: failure\n", __func__);
		return NULL;
	}
	memcpy(&(cloned_pkt->hdr), &(pkt->hdr), sizeof(struct rr_header_v1));
	if (pkt->opt_hdr.len > 0) {
		cloned_pkt->opt_hdr.data = kmalloc(pkt->opt_hdr.len,
							GFP_KERNEL);
		if (!cloned_pkt->opt_hdr.data) {
			IPC_RTR_ERR("%s: Memory allocation Failed\n", __func__);
		} else {
			cloned_pkt->opt_hdr.len = pkt->opt_hdr.len;
			memcpy(cloned_pkt->opt_hdr.data, pkt->opt_hdr.data,
			       pkt->opt_hdr.len);
		}
	}

	pkt_fragment_q = kmalloc(sizeof(struct sk_buff_head), GFP_KERNEL);
	if (!pkt_fragment_q) {
		IPC_RTR_ERR("%s: pkt_frag_q alloc failure\n", __func__);
		kfree(cloned_pkt);
		return NULL;
	}
	skb_queue_head_init(pkt_fragment_q);
	kref_init(&cloned_pkt->ref);

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
	if (cloned_pkt->opt_hdr.len > 0)
		kfree(cloned_pkt->opt_hdr.data);
	kfree(cloned_pkt);
	return NULL;
}

/**
 * create_pkt() - Create a Router packet
 * @data: SKB queue to be contained inside the packet.
 *
 * @return: pointer to packet on success, NULL on failure.
 */
struct rr_packet *create_pkt(struct sk_buff_head *data)
{
	struct rr_packet *pkt;
	struct sk_buff *temp_skb;

	pkt = kzalloc(sizeof(struct rr_packet), GFP_KERNEL);
	if (!pkt) {
		IPC_RTR_ERR("%s: failure\n", __func__);
		return NULL;
	}

	if (data) {
		pkt->pkt_fragment_q = data;
		skb_queue_walk(pkt->pkt_fragment_q, temp_skb)
			pkt->length += temp_skb->len;
	} else {
		pkt->pkt_fragment_q = kmalloc(sizeof(struct sk_buff_head),
					      GFP_KERNEL);
		if (!pkt->pkt_fragment_q) {
			IPC_RTR_ERR("%s: Couldn't alloc pkt_fragment_q\n",
				    __func__);
			kfree(pkt);
			return NULL;
		}
		skb_queue_head_init(pkt->pkt_fragment_q);
	}
	kref_init(&pkt->ref);
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
	if (pkt->opt_hdr.len > 0)
		kfree(pkt->opt_hdr.data);
	kfree(pkt);
	return;
}

static struct sk_buff_head *msm_ipc_router_buf_to_skb(void *buf,
						unsigned int buf_len)
{
	struct sk_buff_head *skb_head;
	struct sk_buff *skb;
	int first = 1, offset = 0;
	int skb_size, data_size;
	void *data;
	int last = 1;
	int align_size;

	skb_head = kmalloc(sizeof(struct sk_buff_head), GFP_KERNEL);
	if (!skb_head) {
		IPC_RTR_ERR("%s: Couldnot allocate skb_head\n", __func__);
		return NULL;
	}
	skb_queue_head_init(skb_head);

	data_size = buf_len;
	align_size = ALIGN_SIZE(data_size);
	while (offset != buf_len) {
		skb_size = data_size;
		if (first)
			skb_size += IPC_ROUTER_HDR_SIZE;
		if (last)
			skb_size += align_size;

		skb = alloc_skb(skb_size, GFP_KERNEL);
		if (!skb) {
			if (skb_size <= (PAGE_SIZE/2)) {
				IPC_RTR_ERR("%s: cannot allocate skb\n",
								__func__);
				goto buf_to_skb_error;
			}
			data_size = data_size / 2;
			last = 0;
			continue;
		}

		if (first) {
			skb_reserve(skb, IPC_ROUTER_HDR_SIZE);
			first = 0;
		}

		data = skb_put(skb, data_size);
		memcpy(skb->data, buf + offset, data_size);
		skb_queue_tail(skb_head, skb);
		offset += data_size;
		data_size = buf_len - offset;
		last = 1;
	}
	return skb_head;

buf_to_skb_error:
	while (!skb_queue_empty(skb_head)) {
		skb = skb_dequeue(skb_head);
		kfree_skb(skb);
	}
	kfree(skb_head);
	return NULL;
}

static void *msm_ipc_router_skb_to_buf(struct sk_buff_head *skb_head,
				       unsigned int len)
{
	struct sk_buff *temp;
	unsigned int offset = 0, buf_len = 0, copy_len;
	void *buf;

	if (!skb_head) {
		IPC_RTR_ERR("%s: NULL skb_head\n", __func__);
		return NULL;
	}

	temp = skb_peek(skb_head);
	buf_len = len;
	buf = kmalloc(buf_len, GFP_KERNEL);
	if (!buf) {
		IPC_RTR_ERR("%s: cannot allocate buf\n", __func__);
		return NULL;
	}
	skb_queue_walk(skb_head, temp) {
		copy_len = buf_len < temp->len ? buf_len : temp->len;
		memcpy(buf + offset, temp->data, copy_len);
		offset += copy_len;
		buf_len -= copy_len;
	}
	return buf;
}

void msm_ipc_router_free_skb(struct sk_buff_head *skb_head)
{
	struct sk_buff *temp_skb;

	if (!skb_head)
		return;

	while (!skb_queue_empty(skb_head)) {
		temp_skb = skb_dequeue(skb_head);
		kfree_skb(temp_skb);
	}
	kfree(skb_head);
}

/**
 * extract_optional_header() - Extract the optional header from skb
 * @pkt:	Packet structure into which the header has to be extracted.
 * @opt_len:	The optional header length in word size.
 *
 * @return:	Length of optional header in bytes if success, zero otherwise.
 */
static int extract_optional_header(struct rr_packet *pkt, uint8_t opt_len)
{
	size_t offset = 0, buf_len = 0, copy_len, opt_hdr_len;
	struct sk_buff *temp;
	struct sk_buff_head *skb_head;

	opt_hdr_len = opt_len * IPCR_WORD_SIZE;
	pkt->opt_hdr.data = kmalloc(opt_hdr_len, GFP_KERNEL);
	if (!pkt->opt_hdr.data) {
		IPC_RTR_ERR("%s: Memory allocation Failed\n", __func__);
		return 0;
	}
	skb_head = pkt->pkt_fragment_q;
	buf_len = opt_hdr_len;
	skb_queue_walk(skb_head, temp) {
		copy_len = buf_len < temp->len ? buf_len : temp->len;
		memcpy(pkt->opt_hdr.data + offset, temp->data, copy_len);
		offset += copy_len;
		buf_len -= copy_len;
		skb_pull(temp, copy_len);
		if (temp->len == 0) {
			skb_dequeue(skb_head);
			kfree_skb(temp);
		}
	}
	pkt->opt_hdr.len = opt_hdr_len;
	return opt_hdr_len;
}

/**
 * extract_header_v1() - Extract IPC Router header of version 1
 * @pkt: Packet structure into which the header has to be extraced.
 * @skb: SKB from which the header has to be extracted.
 *
 * @return: 0 on success, standard Linux error codes on failure.
 */
static int extract_header_v1(struct rr_packet *pkt, struct sk_buff *skb)
{
	if (!pkt || !skb) {
		IPC_RTR_ERR("%s: Invalid pkt or skb\n", __func__);
		return -EINVAL;
	}

	memcpy(&pkt->hdr, skb->data, sizeof(struct rr_header_v1));
	skb_pull(skb, sizeof(struct rr_header_v1));
	pkt->length -= sizeof(struct rr_header_v1);
	return 0;
}

/**
 * extract_header_v2() - Extract IPC Router header of version 2
 * @pkt: Packet structure into which the header has to be extraced.
 * @skb: SKB from which the header has to be extracted.
 *
 * @return: 0 on success, standard Linux error codes on failure.
 */
static int extract_header_v2(struct rr_packet *pkt, struct sk_buff *skb)
{
	struct rr_header_v2 *hdr;
	uint8_t opt_len;
	size_t opt_hdr_len;
	size_t total_hdr_size = sizeof(*hdr);

	if (!pkt || !skb) {
		IPC_RTR_ERR("%s: Invalid pkt or skb\n", __func__);
		return -EINVAL;
	}

	hdr = (struct rr_header_v2 *)skb->data;
	pkt->hdr.version = (uint32_t)hdr->version;
	pkt->hdr.type = (uint32_t)hdr->type;
	pkt->hdr.src_node_id = (uint32_t)hdr->src_node_id;
	pkt->hdr.src_port_id = (uint32_t)hdr->src_port_id;
	pkt->hdr.size = (uint32_t)hdr->size;
	pkt->hdr.control_flag = (uint32_t)hdr->control_flag;
	pkt->hdr.dst_node_id = (uint32_t)hdr->dst_node_id;
	pkt->hdr.dst_port_id = (uint32_t)hdr->dst_port_id;
	opt_len = hdr->opt_len;
	skb_pull(skb, total_hdr_size);
	if (opt_len > 0) {
		opt_hdr_len = extract_optional_header(pkt, opt_len);
		total_hdr_size += opt_hdr_len;
	}
	pkt->length -= total_hdr_size;
	return 0;
}

/**
 * extract_header() - Extract IPC Router header
 * @pkt: Packet from which the header has to be extraced.
 *
 * @return: 0 on success, standard Linux error codes on failure.
 *
 * This function will check if the header version is v1 or v2 and invoke
 * the corresponding helper function to extract the IPC Router header.
 */
static int extract_header(struct rr_packet *pkt)
{
	struct sk_buff *temp_skb;
	int ret;

	if (!pkt) {
		IPC_RTR_ERR("%s: NULL PKT\n", __func__);
		return -EINVAL;
	}

	temp_skb = skb_peek(pkt->pkt_fragment_q);
	if (!temp_skb || !temp_skb->data) {
		IPC_RTR_ERR("%s: No SKBs in skb_queue\n", __func__);
		return -EINVAL;
	}

	if (temp_skb->data[0] == IPC_ROUTER_V1) {
		ret = extract_header_v1(pkt, temp_skb);
	} else if (temp_skb->data[0] == IPC_ROUTER_V2) {
		ret = extract_header_v2(pkt, temp_skb);
	} else {
		IPC_RTR_ERR("%s: Invalid Header version %02x\n",
			__func__, temp_skb->data[0]);
		print_hex_dump(KERN_ERR, "Header: ", DUMP_PREFIX_ADDRESS,
			       16, 1, temp_skb->data, pkt->length, true);
		return -EINVAL;
	}
	return ret;
}

/**
 * calc_tx_header_size() - Calculate header size to be reserved in SKB
 * @pkt: Packet in which the space for header has to be reserved.
 * @dst_xprt_info: XPRT through which the destination is reachable.
 *
 * @return: required header size on success,
 *          starndard Linux error codes on failure.
 *
 * This function is used to calculate the header size that has to be reserved
 * in a transmit SKB. The header size is calculated based on the XPRT through
 * which the destination node is reachable.
 */
static int calc_tx_header_size(struct rr_packet *pkt,
			       struct msm_ipc_router_xprt_info *dst_xprt_info)
{
	int hdr_size = 0;
	int xprt_version = 0;
	struct msm_ipc_router_xprt_info *xprt_info = dst_xprt_info;

	if (!pkt) {
		IPC_RTR_ERR("%s: NULL PKT\n", __func__);
		return -EINVAL;
	}

	if (xprt_info)
		xprt_version = xprt_info->xprt->get_version(xprt_info->xprt);

	if (xprt_version == IPC_ROUTER_V1) {
		pkt->hdr.version = IPC_ROUTER_V1;
		hdr_size = sizeof(struct rr_header_v1);
	} else if (xprt_version == IPC_ROUTER_V2) {
		pkt->hdr.version = IPC_ROUTER_V2;
		hdr_size = sizeof(struct rr_header_v2) + pkt->opt_hdr.len;
	} else {
		IPC_RTR_ERR("%s: Invalid xprt_version %d\n",
			__func__, xprt_version);
		hdr_size = -EINVAL;
	}

	return hdr_size;
}

/**
 * calc_rx_header_size() - Calculate the RX header size
 * @xprt_info: XPRT info of the received message.
 *
 * @return: valid header size on success, INT_MAX on failure.
 */
static int calc_rx_header_size(struct msm_ipc_router_xprt_info *xprt_info)
{
	int xprt_version = 0;
	int hdr_size = INT_MAX;

	if (xprt_info)
		xprt_version = xprt_info->xprt->get_version(xprt_info->xprt);

	if (xprt_version == IPC_ROUTER_V1)
		hdr_size = sizeof(struct rr_header_v1);
	else if (xprt_version == IPC_ROUTER_V2)
		hdr_size = sizeof(struct rr_header_v2);
	return hdr_size;
}

/**
 * prepend_header_v1() - Prepend IPC Router header of version 1
 * @pkt: Packet structure which contains the header info to be prepended.
 * @hdr_size: Size of the header
 *
 * @return: 0 on success, standard Linux error codes on failure.
 */
static int prepend_header_v1(struct rr_packet *pkt, int hdr_size)
{
	struct sk_buff *temp_skb;
	struct rr_header_v1 *hdr;

	if (!pkt || hdr_size <= 0) {
		IPC_RTR_ERR("%s: Invalid input parameters\n", __func__);
		return -EINVAL;
	}

	temp_skb = skb_peek(pkt->pkt_fragment_q);
	if (!temp_skb || !temp_skb->data) {
		IPC_RTR_ERR("%s: No SKBs in skb_queue\n", __func__);
		return -EINVAL;
	}

	if (skb_headroom(temp_skb) < hdr_size) {
		temp_skb = alloc_skb(hdr_size, GFP_KERNEL);
		if (!temp_skb) {
			IPC_RTR_ERR("%s: Could not allocate SKB of size %d\n",
				__func__, hdr_size);
			return -ENOMEM;
		}
		skb_reserve(temp_skb, hdr_size);
	}

	hdr = (struct rr_header_v1 *)skb_push(temp_skb, hdr_size);
	memcpy(hdr, &pkt->hdr, hdr_size);
	if (temp_skb != skb_peek(pkt->pkt_fragment_q))
		skb_queue_head(pkt->pkt_fragment_q, temp_skb);
	pkt->length += hdr_size;
	return 0;
}

/**
 * prepend_header_v2() - Prepend IPC Router header of version 2
 * @pkt: Packet structure which contains the header info to be prepended.
 * @hdr_size: Size of the header
 *
 * @return: 0 on success, standard Linux error codes on failure.
 */
static int prepend_header_v2(struct rr_packet *pkt, int hdr_size)
{
	struct sk_buff *temp_skb;
	struct rr_header_v2 *hdr;

	if (!pkt || hdr_size <= 0) {
		IPC_RTR_ERR("%s: Invalid input parameters\n", __func__);
		return -EINVAL;
	}

	temp_skb = skb_peek(pkt->pkt_fragment_q);
	if (!temp_skb || !temp_skb->data) {
		IPC_RTR_ERR("%s: No SKBs in skb_queue\n", __func__);
		return -EINVAL;
	}

	if (skb_headroom(temp_skb) < hdr_size) {
		temp_skb = alloc_skb(hdr_size, GFP_KERNEL);
		if (!temp_skb) {
			IPC_RTR_ERR("%s: Could not allocate SKB of size %d\n",
				__func__, hdr_size);
			return -ENOMEM;
		}
		skb_reserve(temp_skb, hdr_size);
	}

	hdr = (struct rr_header_v2 *)skb_push(temp_skb, hdr_size);
	hdr->version = (uint8_t)pkt->hdr.version;
	hdr->type = (uint8_t)pkt->hdr.type;
	hdr->control_flag = (uint8_t)pkt->hdr.control_flag;
	hdr->size = (uint32_t)pkt->hdr.size;
	hdr->src_node_id = (uint16_t)pkt->hdr.src_node_id;
	hdr->src_port_id = (uint16_t)pkt->hdr.src_port_id;
	hdr->dst_node_id = (uint16_t)pkt->hdr.dst_node_id;
	hdr->dst_port_id = (uint16_t)pkt->hdr.dst_port_id;
	if (pkt->opt_hdr.len > 0) {
		hdr->opt_len = pkt->opt_hdr.len/IPCR_WORD_SIZE;
		memcpy(hdr + sizeof(*hdr), pkt->opt_hdr.data, pkt->opt_hdr.len);
	} else {
		hdr->opt_len = 0;
	}
	if (temp_skb != skb_peek(pkt->pkt_fragment_q))
		skb_queue_head(pkt->pkt_fragment_q, temp_skb);
	pkt->length += hdr_size;
	return 0;
}

/**
 * prepend_header() - Prepend IPC Router header
 * @pkt: Packet structure which contains the header info to be prepended.
 * @xprt_info: XPRT through which the packet is transmitted.
 *
 * @return: 0 on success, standard Linux error codes on failure.
 *
 * This function prepends the header to the packet to be transmitted. The
 * IPC Router header version to be prepended depends on the XPRT through
 * which the destination is reachable.
 */
static int prepend_header(struct rr_packet *pkt,
			  struct msm_ipc_router_xprt_info *xprt_info)
{
	int hdr_size;
	struct sk_buff *temp_skb;

	if (!pkt) {
		IPC_RTR_ERR("%s: NULL PKT\n", __func__);
		return -EINVAL;
	}

	temp_skb = skb_peek(pkt->pkt_fragment_q);
	if (!temp_skb || !temp_skb->data) {
		IPC_RTR_ERR("%s: No SKBs in skb_queue\n", __func__);
		return -EINVAL;
	}

	hdr_size = calc_tx_header_size(pkt, xprt_info);
	if (hdr_size <= 0)
		return hdr_size;

	if (pkt->hdr.version == IPC_ROUTER_V1)
		return prepend_header_v1(pkt, hdr_size);
	else if (pkt->hdr.version == IPC_ROUTER_V2)
		return prepend_header_v2(pkt, hdr_size);
	else
		return -EINVAL;
}

/**
 * defragment_pkt() - Defragment and linearize the packet
 * @pkt: Packet to be linearized.
 *
 * @return: 0 on success, standard Linux error codes on failure.
 *
 * Some packets contain fragments of data over multiple SKBs. If an XPRT
 * does not supported fragmented writes, linearize multiple SKBs into one
 * single SKB.
 */
static int defragment_pkt(struct rr_packet *pkt)
{
	struct sk_buff *dst_skb, *src_skb, *temp_skb;
	int offset = 0, buf_len = 0, copy_len;
	void *buf;
	int align_size;

	if (!pkt || pkt->length <= 0) {
		IPC_RTR_ERR("%s: Invalid PKT\n", __func__);
		return -EINVAL;
	}

	if (skb_queue_len(pkt->pkt_fragment_q) == 1)
		return 0;

	align_size = ALIGN_SIZE(pkt->length);
	dst_skb = alloc_skb(pkt->length + align_size, GFP_KERNEL);
	if (!dst_skb) {
		IPC_RTR_ERR("%s: could not allocate one skb of size %d\n",
			__func__, pkt->length);
		return -ENOMEM;
	}
	buf = skb_put(dst_skb, pkt->length);
	buf_len = pkt->length;

	skb_queue_walk(pkt->pkt_fragment_q, src_skb) {
		copy_len =  buf_len < src_skb->len ? buf_len : src_skb->len;
		memcpy(buf + offset, src_skb->data, copy_len);
		offset += copy_len;
		buf_len -= copy_len;
	}

	while (!skb_queue_empty(pkt->pkt_fragment_q)) {
		temp_skb = skb_dequeue(pkt->pkt_fragment_q);
		kfree_skb(temp_skb);
	}
	skb_queue_tail(pkt->pkt_fragment_q, dst_skb);
	return 0;
}

static int post_pkt_to_port(struct msm_ipc_port *port_ptr,
			    struct rr_packet *pkt, int clone)
{
	struct rr_packet *temp_pkt = pkt;
	void (*notify)(unsigned event, void *oob_data,
		       size_t oob_data_len, void *priv);
	void (*data_ready)(struct sock *sk) = NULL;
	struct sock *sk;
	uint32_t pkt_type;

	if (unlikely(!port_ptr || !pkt))
		return -EINVAL;

	if (clone) {
		temp_pkt = clone_pkt(pkt);
		if (!temp_pkt) {
			IPC_RTR_ERR(
			"%s: Error cloning packet for port %08x:%08x\n",
				__func__, port_ptr->this_port.node_id,
				port_ptr->this_port.port_id);
			return -ENOMEM;
		}
	}

	mutex_lock(&port_ptr->port_rx_q_lock_lhc3);
	__pm_stay_awake(port_ptr->port_rx_ws);
	list_add_tail(&temp_pkt->list, &port_ptr->port_rx_q);
	wake_up(&port_ptr->port_rx_wait_q);
	notify = port_ptr->notify;
	pkt_type = temp_pkt->hdr.type;
	sk = (struct sock *)port_ptr->endpoint;
	if (sk) {
		read_lock(&sk->sk_callback_lock);
		data_ready = sk->sk_data_ready;
		read_unlock(&sk->sk_callback_lock);
	}
	mutex_unlock(&port_ptr->port_rx_q_lock_lhc3);
	if (notify)
		notify(pkt_type, NULL, 0, port_ptr->priv);
	else if (sk && data_ready)
		data_ready(sk);

	return 0;
}

/**
 * ipc_router_peek_pkt_size() - Peek into the packet header to get potential packet size
 * @data: Starting address of the packet which points to router header.
 *
 * @returns: potential packet size on success, < 0 on error.
 *
 * This function is used by the underlying transport abstraction layer to
 * peek into the potential packet size of an incoming packet. This information
 * is used to perform link layer fragmentation and re-assembly
 */
int ipc_router_peek_pkt_size(char *data)
{
	int size;

	if (!data) {
		pr_err("%s: NULL PKT\n", __func__);
		return -EINVAL;
	}

	if (data[0] == IPC_ROUTER_V1)
		size = ((struct rr_header_v1 *)data)->size +
			sizeof(struct rr_header_v1);
	else if (data[0] == IPC_ROUTER_V2)
		size = ((struct rr_header_v2 *)data)->size +
			((struct rr_header_v2 *)data)->opt_len * IPCR_WORD_SIZE
			+ sizeof(struct rr_header_v2);
	else
		return -EINVAL;

	size += ALIGN_SIZE(size);
	return size;
}

static int post_control_ports(struct rr_packet *pkt)
{
	struct msm_ipc_port *port_ptr;

	if (!pkt)
		return -EINVAL;

	down_read(&control_ports_lock_lha5);
	list_for_each_entry(port_ptr, &control_ports, list)
		post_pkt_to_port(port_ptr, pkt, 1);
	up_read(&control_ports_lock_lha5);
	return 0;
}

static uint32_t allocate_port_id(void)
{
	uint32_t port_id = 0, prev_port_id, key;
	struct msm_ipc_port *port_ptr;

	mutex_lock(&next_port_id_lock_lhc1);
	prev_port_id = next_port_id;
	down_read(&local_ports_lock_lhc2);
	do {
		next_port_id++;
		if ((next_port_id & IPC_ROUTER_ADDRESS) == IPC_ROUTER_ADDRESS)
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
	up_read(&local_ports_lock_lhc2);
	mutex_unlock(&next_port_id_lock_lhc1);

	return port_id;
}

void msm_ipc_router_add_local_port(struct msm_ipc_port *port_ptr)
{
	uint32_t key;

	if (!port_ptr)
		return;

	key = (port_ptr->this_port.port_id & (LP_HASH_SIZE - 1));
	down_write(&local_ports_lock_lhc2);
	list_add_tail(&port_ptr->list, &local_ports[key]);
	up_write(&local_ports_lock_lhc2);
}

/**
 * msm_ipc_router_create_raw_port() - Create an IPC Router port
 * @endpoint: User-space space socket information to be cached.
 * @notify: Function to notify incoming events on the port.
 *   @event: Event ID to be handled.
 *   @oob_data: Any out-of-band data associated with the event.
 *   @oob_data_len: Size of the out-of-band data, if valid.
 *   @priv: Private data registered during the port creation.
 * @priv: Private Data to be passed during the event notification.
 *
 * @return: Valid pointer to port on success, NULL on failure.
 *
 * This function is used to create an IPC Router port. The port is used for
 * communication locally or outside the subsystem.
 */
struct msm_ipc_port *msm_ipc_router_create_raw_port(void *endpoint,
	void (*notify)(unsigned event, void *oob_data,
		       size_t oob_data_len, void *priv),
	void *priv)
{
	struct msm_ipc_port *port_ptr;

	port_ptr = kzalloc(sizeof(struct msm_ipc_port), GFP_KERNEL);
	if (!port_ptr)
		return NULL;

	port_ptr->this_port.node_id = IPC_ROUTER_NID_LOCAL;
	port_ptr->this_port.port_id = allocate_port_id();
	if (!port_ptr->this_port.port_id) {
		IPC_RTR_ERR("%s: All port ids are in use\n", __func__);
		kfree(port_ptr);
		return NULL;
	}

	mutex_init(&port_ptr->port_lock_lhc3);
	INIT_LIST_HEAD(&port_ptr->port_rx_q);
	mutex_init(&port_ptr->port_rx_q_lock_lhc3);
	init_waitqueue_head(&port_ptr->port_rx_wait_q);
	snprintf(port_ptr->rx_ws_name, MAX_WS_NAME_SZ,
		 "ipc%08x_%s",
		 port_ptr->this_port.port_id,
		 current->comm);
	port_ptr->port_rx_ws = wakeup_source_register(port_ptr->rx_ws_name);
	if (!port_ptr->port_rx_ws) {
		kfree(port_ptr);
		return NULL;
	}
	init_waitqueue_head(&port_ptr->port_tx_wait_q);
	kref_init(&port_ptr->ref);

	port_ptr->endpoint = endpoint;
	port_ptr->notify = notify;
	port_ptr->priv = priv;

	msm_ipc_router_add_local_port(port_ptr);
	if (endpoint)
		sock_hold(ipc_port_sk(endpoint));
	return port_ptr;
}

/**
 * ipc_router_get_port_ref() - Get a reference to the local port
 * @port_id: Port ID of the local port for which reference is get.
 *
 * @return: If port is found, a reference to the port is returned.
 *          Else NULL is returned.
 */
static struct msm_ipc_port *ipc_router_get_port_ref(uint32_t port_id)
{
	int key = (port_id & (LP_HASH_SIZE - 1));
	struct msm_ipc_port *port_ptr;

	down_read(&local_ports_lock_lhc2);
	list_for_each_entry(port_ptr, &local_ports[key], list) {
		if (port_ptr->this_port.port_id == port_id) {
			kref_get(&port_ptr->ref);
			up_read(&local_ports_lock_lhc2);
			return port_ptr;
		}
	}
	up_read(&local_ports_lock_lhc2);
	return NULL;
}

/**
 * ipc_router_release_port() - Cleanup and release the port
 * @ref: Reference to the port.
 *
 * This function is called when all references to the port are released.
 */
void ipc_router_release_port(struct kref *ref)
{
	struct rr_packet *pkt, *temp_pkt;
	struct msm_ipc_port *port_ptr =
		container_of(ref, struct msm_ipc_port, ref);

	mutex_lock(&port_ptr->port_rx_q_lock_lhc3);
	list_for_each_entry_safe(pkt, temp_pkt, &port_ptr->port_rx_q, list) {
		list_del(&pkt->list);
		release_pkt(pkt);
	}
	mutex_unlock(&port_ptr->port_rx_q_lock_lhc3);
	wakeup_source_unregister(port_ptr->port_rx_ws);
	if (port_ptr->endpoint)
		sock_put(ipc_port_sk(port_ptr->endpoint));
	kfree(port_ptr);
}

/**
 * ipc_router_get_rport_ref()- Get reference to the remote port
 * @node_id: Node ID corresponding to the remote port.
 * @port_id: Port ID corresponding to the remote port.
 *
 * @return: a reference to the remote port on success, NULL on failure.
 */
static struct msm_ipc_router_remote_port *ipc_router_get_rport_ref(
		uint32_t node_id, uint32_t port_id)
{
	struct msm_ipc_router_remote_port *rport_ptr;
	struct msm_ipc_routing_table_entry *rt_entry;
	int key = (port_id & (RP_HASH_SIZE - 1));

	rt_entry = ipc_router_get_rtentry_ref(node_id);
	if (!rt_entry) {
		IPC_RTR_ERR("%s: Node is not up\n", __func__);
		return NULL;
	}

	down_read(&rt_entry->lock_lha4);
	list_for_each_entry(rport_ptr,
			    &rt_entry->remote_port_list[key], list) {
		if (rport_ptr->port_id == port_id) {
			kref_get(&rport_ptr->ref);
			goto out_lookup_rmt_port1;
		}
	}
	rport_ptr = NULL;
out_lookup_rmt_port1:
	up_read(&rt_entry->lock_lha4);
	kref_put(&rt_entry->ref, ipc_router_release_rtentry);
	return rport_ptr;
}

/**
 * ipc_router_create_rport() - Create a remote port
 * @node_id: Node ID corresponding to the remote port.
 * @port_id: Port ID corresponding to the remote port.
 * @xprt_info: XPRT through which the concerned node is reachable.
 *
 * @return: a reference to the remote port on success, NULL on failure.
 */
static struct msm_ipc_router_remote_port *ipc_router_create_rport(
				uint32_t node_id, uint32_t port_id,
				struct msm_ipc_router_xprt_info *xprt_info)
{
	struct msm_ipc_router_remote_port *rport_ptr;
	struct msm_ipc_routing_table_entry *rt_entry;
	int key = (port_id & (RP_HASH_SIZE - 1));

	rt_entry = create_routing_table_entry(node_id, xprt_info);
	if (!rt_entry) {
		IPC_RTR_ERR("%s: Node cannot be created\n", __func__);
		return NULL;
	}

	down_write(&rt_entry->lock_lha4);
	list_for_each_entry(rport_ptr,
			    &rt_entry->remote_port_list[key], list) {
		if (rport_ptr->port_id == port_id)
			goto out_create_rmt_port1;
	}

	rport_ptr = kmalloc(sizeof(struct msm_ipc_router_remote_port),
			    GFP_KERNEL);
	if (!rport_ptr) {
		IPC_RTR_ERR("%s: Remote port alloc failed\n", __func__);
		goto out_create_rmt_port2;
	}
	rport_ptr->port_id = port_id;
	rport_ptr->node_id = node_id;
	rport_ptr->status = VALID;
	rport_ptr->sec_rule = NULL;
	rport_ptr->server = NULL;
	rport_ptr->tx_quota_cnt = 0;
	kref_init(&rport_ptr->ref);
	mutex_init(&rport_ptr->rport_lock_lhb2);
	INIT_LIST_HEAD(&rport_ptr->resume_tx_port_list);
	INIT_LIST_HEAD(&rport_ptr->conn_info_list);
	list_add_tail(&rport_ptr->list,
		      &rt_entry->remote_port_list[key]);
out_create_rmt_port1:
	kref_get(&rport_ptr->ref);
out_create_rmt_port2:
	up_write(&rt_entry->lock_lha4);
	kref_put(&rt_entry->ref, ipc_router_release_rtentry);
	return rport_ptr;
}

/**
 * msm_ipc_router_free_resume_tx_port() - Free the resume_tx ports
 * @rport_ptr: Pointer to the remote port.
 *
 * This function deletes all the resume_tx ports associated with a remote port
 * and frees the memory allocated to each resume_tx port.
 *
 * Must be called with rport_ptr->rport_lock_lhb2 locked.
 */
static void msm_ipc_router_free_resume_tx_port(
	struct msm_ipc_router_remote_port *rport_ptr)
{
	struct msm_ipc_resume_tx_port *rtx_port, *tmp_rtx_port;

	list_for_each_entry_safe(rtx_port, tmp_rtx_port,
			&rport_ptr->resume_tx_port_list, list) {
		list_del(&rtx_port->list);
		kfree(rtx_port);
	}
}

/**
 * msm_ipc_router_lookup_resume_tx_port() - Lookup resume_tx port list
 * @rport_ptr: Remote port whose resume_tx port list needs to be looked.
 * @port_id: Port ID which needs to be looked from the list.
 *
 * return 1 if the port_id is found in the list, else 0.
 *
 * This function is used to lookup the existence of a local port in
 * remote port's resume_tx list. This function is used to ensure that
 * the same port is not added to the remote_port's resume_tx list repeatedly.
 *
 * Must be called with rport_ptr->rport_lock_lhb2 locked.
 */
static int msm_ipc_router_lookup_resume_tx_port(
	struct msm_ipc_router_remote_port *rport_ptr, uint32_t port_id)
{
	struct msm_ipc_resume_tx_port *rtx_port;

	list_for_each_entry(rtx_port, &rport_ptr->resume_tx_port_list, list) {
		if (port_id == rtx_port->port_id)
			return 1;
	}
	return 0;
}

/**
 * ipc_router_dummy_write_space() - Dummy write space available callback
 * @sk:	Socket pointer for which the callback is called.
 */
void ipc_router_dummy_write_space(struct sock *sk)
{
}

/**
 * post_resume_tx() - Post the resume_tx event
 * @rport_ptr: Pointer to the remote port
 * @pkt : The data packet that is received on a resume_tx event
 * @msg: Out of band data to be passed to kernel drivers
 *
 * This function informs about the reception of the resume_tx message from a
 * remote port pointed by rport_ptr to all the local ports that are in the
 * resume_tx_ports_list of this remote port. On posting the information, this
 * function sequentially deletes each entry in the resume_tx_port_list of the
 * remote port.
 *
 * Must be called with rport_ptr->rport_lock_lhb2 locked.
 */
static void post_resume_tx(struct msm_ipc_router_remote_port *rport_ptr,
			   struct rr_packet *pkt, union rr_control_msg *msg)
{
	struct msm_ipc_resume_tx_port *rtx_port, *tmp_rtx_port;
	struct msm_ipc_port *local_port;
	struct sock *sk;
	void (*write_space)(struct sock *sk) = NULL;

	list_for_each_entry_safe(rtx_port, tmp_rtx_port,
				&rport_ptr->resume_tx_port_list, list) {
		local_port = ipc_router_get_port_ref(rtx_port->port_id);
		if (local_port && local_port->notify) {
			wake_up(&local_port->port_tx_wait_q);
			local_port->notify(IPC_ROUTER_CTRL_CMD_RESUME_TX, msg,
					   sizeof(*msg), local_port->priv);
		} else if (local_port) {
			wake_up(&local_port->port_tx_wait_q);
			sk = ipc_port_sk(local_port->endpoint);
			if (sk) {
				read_lock(&sk->sk_callback_lock);
				write_space = sk->sk_write_space;
				read_unlock(&sk->sk_callback_lock);
			}
			if (write_space &&
			    write_space != ipc_router_dummy_write_space)
				write_space(sk);
			else
				post_pkt_to_port(local_port, pkt, 1);
		} else {
			IPC_RTR_ERR("%s: Local Port %d not Found",
				__func__, rtx_port->port_id);
		}
		if (local_port)
			kref_put(&local_port->ref, ipc_router_release_port);
		list_del(&rtx_port->list);
		kfree(rtx_port);
	}
}

/**
 * signal_rport_exit() - Signal the local ports of remote port exit
 * @rport_ptr: Remote port that is exiting.
 *
 * This function is used to signal the local ports that are waiting
 * to resume transmission to a remote port that is exiting.
 */
static void signal_rport_exit(struct msm_ipc_router_remote_port *rport_ptr)
{
	struct msm_ipc_resume_tx_port *rtx_port, *tmp_rtx_port;
	struct msm_ipc_port *local_port;

	mutex_lock(&rport_ptr->rport_lock_lhb2);
	rport_ptr->status = RESET;
	list_for_each_entry_safe(rtx_port, tmp_rtx_port,
				 &rport_ptr->resume_tx_port_list, list) {
		local_port = ipc_router_get_port_ref(rtx_port->port_id);
		if (local_port) {
			wake_up(&local_port->port_tx_wait_q);
			kref_put(&local_port->ref, ipc_router_release_port);
		}
		list_del(&rtx_port->list);
		kfree(rtx_port);
	}
	mutex_unlock(&rport_ptr->rport_lock_lhb2);
}

/**
 * ipc_router_release_rport() - Cleanup and release the remote port
 * @ref: Reference to the remote port.
 *
 * This function is called when all references to the remote port are released.
 */
static void ipc_router_release_rport(struct kref *ref)
{
	struct msm_ipc_router_remote_port *rport_ptr =
		container_of(ref, struct msm_ipc_router_remote_port, ref);

	mutex_lock(&rport_ptr->rport_lock_lhb2);
	msm_ipc_router_free_resume_tx_port(rport_ptr);
	mutex_unlock(&rport_ptr->rport_lock_lhb2);
	kfree(rport_ptr);
}

/**
 * ipc_router_destroy_rport() - Destroy the remote port
 * @rport_ptr: Pointer to the remote port to be destroyed.
 */
static void ipc_router_destroy_rport(
	struct msm_ipc_router_remote_port *rport_ptr)
{
	uint32_t node_id;
	struct msm_ipc_routing_table_entry *rt_entry;

	if (!rport_ptr)
		return;

	node_id = rport_ptr->node_id;
	rt_entry = ipc_router_get_rtentry_ref(node_id);
	if (!rt_entry) {
		IPC_RTR_ERR("%s: Node %d is not up\n", __func__, node_id);
		return;
	}
	down_write(&rt_entry->lock_lha4);
	list_del(&rport_ptr->list);
	up_write(&rt_entry->lock_lha4);
	signal_rport_exit(rport_ptr);
	kref_put(&rport_ptr->ref, ipc_router_release_rport);
	kref_put(&rt_entry->ref, ipc_router_release_rtentry);
	return;
}

/**
 * msm_ipc_router_lookup_server() - Lookup server information
 * @service: Service ID of the server info to be looked up.
 * @instance: Instance ID of the server info to be looked up.
 * @node_id: Node/Processor ID in which the server is hosted.
 * @port_id: Port ID within the node in which the server is hosted.
 *
 * @return: If found Pointer to server structure, else NULL.
 *
 * Note1: Lock the server_list_lock_lha2 before accessing this function.
 * Note2: If the <node_id:port_id> are <0:0>, then the lookup is restricted
 *        to <service:instance>. Used only when a client wants to send a
 *        message to any QMI server.
 */
static struct msm_ipc_server *msm_ipc_router_lookup_server(
				uint32_t service,
				uint32_t instance,
				uint32_t node_id,
				uint32_t port_id)
{
	struct msm_ipc_server *server;
	struct msm_ipc_server_port *server_port;
	int key = (service & (SRV_HASH_SIZE - 1));

	list_for_each_entry(server, &server_list[key], list) {
		if ((server->name.service != service) ||
		    (server->name.instance != instance))
			continue;
		if ((node_id == 0) && (port_id == 0))
			return server;
		list_for_each_entry(server_port, &server->server_port_list,
				    list) {
			if ((server_port->server_addr.node_id == node_id) &&
			    (server_port->server_addr.port_id == port_id))
				return server;
		}
	}
	return NULL;
}

/**
 * ipc_router_get_server_ref() - Get reference to the server
 * @svc: Service ID for which the reference is required.
 * @ins: Instance ID for which the reference is required.
 * @node_id: Node/Processor ID in which the server is hosted.
 * @port_id: Port ID within the node in which the server is hosted.
 *
 * @return: If found return reference to server, else NULL.
 */
static struct msm_ipc_server *ipc_router_get_server_ref(
	uint32_t svc, uint32_t ins, uint32_t node_id, uint32_t port_id)
{
	struct msm_ipc_server *server;

	down_read(&server_list_lock_lha2);
	server = msm_ipc_router_lookup_server(svc, ins, node_id, port_id);
	if (server)
		kref_get(&server->ref);
	up_read(&server_list_lock_lha2);
	return server;
}

/**
 * ipc_router_release_server() - Cleanup and release the server
 * @ref: Reference to the server.
 *
 * This function is called when all references to the server are released.
 */
static void ipc_router_release_server(struct kref *ref)
{
	struct msm_ipc_server *server =
		container_of(ref, struct msm_ipc_server, ref);

	kfree(server);
}

/**
 * msm_ipc_router_create_server() - Add server info to hash table
 * @service: Service ID of the server info to be created.
 * @instance: Instance ID of the server info to be created.
 * @node_id: Node/Processor ID in which the server is hosted.
 * @port_id: Port ID within the node in which the server is hosted.
 * @xprt_info: XPRT through which the node hosting the server is reached.
 *
 * @return: Pointer to server structure on success, else NULL.
 *
 * This function adds the server info to the hash table. If the same
 * server(i.e. <service_id:instance_id>) is hosted in different nodes,
 * they are maintained as list of "server_port" under "server" structure.
 */
static struct msm_ipc_server *msm_ipc_router_create_server(
					uint32_t service,
					uint32_t instance,
					uint32_t node_id,
					uint32_t port_id,
		struct msm_ipc_router_xprt_info *xprt_info)
{
	struct msm_ipc_server *server = NULL;
	struct msm_ipc_server_port *server_port;
	struct platform_device *pdev;
	int key = (service & (SRV_HASH_SIZE - 1));

	down_write(&server_list_lock_lha2);
	server = msm_ipc_router_lookup_server(service, instance, 0, 0);
	if (server) {
		list_for_each_entry(server_port, &server->server_port_list,
				    list) {
			if ((server_port->server_addr.node_id == node_id) &&
			    (server_port->server_addr.port_id == port_id))
				goto return_server;
		}
		goto create_srv_port;
	}

	server = kzalloc(sizeof(struct msm_ipc_server), GFP_KERNEL);
	if (!server) {
		up_write(&server_list_lock_lha2);
		IPC_RTR_ERR("%s: Server allocation failed\n", __func__);
		return NULL;
	}
	server->name.service = service;
	server->name.instance = instance;
	server->synced_sec_rule = 0;
	INIT_LIST_HEAD(&server->server_port_list);
	kref_init(&server->ref);
	list_add_tail(&server->list, &server_list[key]);
	scnprintf(server->pdev_name, sizeof(server->pdev_name),
		  "SVC%08x:%08x", service, instance);
	server->next_pdev_id = 1;

create_srv_port:
	server_port = kzalloc(sizeof(struct msm_ipc_server_port), GFP_KERNEL);
	pdev = platform_device_alloc(server->pdev_name, server->next_pdev_id);
	if (!server_port || !pdev) {
		kfree(server_port);
		if (pdev)
			platform_device_put(pdev);
		if (list_empty(&server->server_port_list)) {
			list_del(&server->list);
			kfree(server);
		}
		up_write(&server_list_lock_lha2);
		IPC_RTR_ERR("%s: Server Port allocation failed\n", __func__);
		return NULL;
	}
	server_port->pdev = pdev;
	server_port->server_addr.node_id = node_id;
	server_port->server_addr.port_id = port_id;
	server_port->xprt_info = xprt_info;
	list_add_tail(&server_port->list, &server->server_port_list);
	server->next_pdev_id++;
	platform_device_add(server_port->pdev);

return_server:
	/* Add a reference so that the caller can put it back */
	kref_get(&server->ref);
	up_write(&server_list_lock_lha2);
	return server;
}

/**
 * ipc_router_destroy_server_nolock() - Remove server info from hash table
 * @server: Server info to be removed.
 * @node_id: Node/Processor ID in which the server is hosted.
 * @port_id: Port ID within the node in which the server is hosted.
 *
 * This function removes the server_port identified using <node_id:port_id>
 * from the server structure. If the server_port list under server structure
 * is empty after removal, then remove the server structure from the server
 * hash table. This function must be called with server_list_lock_lha2 locked.
 */
static void ipc_router_destroy_server_nolock(struct msm_ipc_server *server,
					  uint32_t node_id, uint32_t port_id)
{
	struct msm_ipc_server_port *server_port;
	bool server_port_found = false;

	if (!server)
		return;

	list_for_each_entry(server_port, &server->server_port_list, list) {
		if ((server_port->server_addr.node_id == node_id) &&
		    (server_port->server_addr.port_id == port_id)) {
			server_port_found = true;
			break;
		}
	}
	if (server_port_found && server_port) {
		platform_device_unregister(server_port->pdev);
		list_del(&server_port->list);
		kfree(server_port);
	}
	if (list_empty(&server->server_port_list)) {
		list_del(&server->list);
		kref_put(&server->ref, ipc_router_release_server);
	}
	return;
}

/**
 * ipc_router_destroy_server() - Remove server info from hash table
 * @server: Server info to be removed.
 * @node_id: Node/Processor ID in which the server is hosted.
 * @port_id: Port ID within the node in which the server is hosted.
 *
 * This function removes the server_port identified using <node_id:port_id>
 * from the server structure. If the server_port list under server structure
 * is empty after removal, then remove the server structure from the server
 * hash table.
 */
static void ipc_router_destroy_server(struct msm_ipc_server *server,
				      uint32_t node_id, uint32_t port_id)
{
	down_write(&server_list_lock_lha2);
	ipc_router_destroy_server_nolock(server, node_id, port_id);
	up_write(&server_list_lock_lha2);
	return;
}

static int ipc_router_send_ctl_msg(
		struct msm_ipc_router_xprt_info *xprt_info,
		union rr_control_msg *msg,
		uint32_t dst_node_id)
{
	struct rr_packet *pkt;
	struct sk_buff *ipc_rtr_pkt;
	struct rr_header_v1 *hdr;
	int pkt_size;
	void *data;
	int ret = -EINVAL;

	pkt = create_pkt(NULL);
	if (!pkt) {
		IPC_RTR_ERR("%s: pkt alloc failed\n", __func__);
		return -ENOMEM;
	}

	pkt_size = IPC_ROUTER_HDR_SIZE + sizeof(*msg);
	ipc_rtr_pkt = alloc_skb(pkt_size, GFP_KERNEL);
	if (!ipc_rtr_pkt) {
		IPC_RTR_ERR("%s: ipc_rtr_pkt alloc failed\n", __func__);
		release_pkt(pkt);
		return -ENOMEM;
	}

	skb_reserve(ipc_rtr_pkt, IPC_ROUTER_HDR_SIZE);
	data = skb_put(ipc_rtr_pkt, sizeof(*msg));
	memcpy(data, msg, sizeof(*msg));
	skb_queue_tail(pkt->pkt_fragment_q, ipc_rtr_pkt);
	pkt->length = sizeof(*msg);

	hdr = &(pkt->hdr);
	hdr->version = IPC_ROUTER_V1;
	hdr->type = msg->cmd;
	hdr->src_node_id = IPC_ROUTER_NID_LOCAL;
	hdr->src_port_id = IPC_ROUTER_ADDRESS;
	hdr->control_flag = 0;
	hdr->size = sizeof(*msg);
	if (hdr->type == IPC_ROUTER_CTRL_CMD_RESUME_TX ||
	    (!xprt_info && dst_node_id == IPC_ROUTER_NID_LOCAL))
		hdr->dst_node_id = dst_node_id;
	else if (xprt_info)
		hdr->dst_node_id = xprt_info->remote_node_id;
	hdr->dst_port_id = IPC_ROUTER_ADDRESS;

	if (dst_node_id == IPC_ROUTER_NID_LOCAL &&
	    msg->cmd != IPC_ROUTER_CTRL_CMD_RESUME_TX) {
		ipc_router_log_msg(local_log_ctx,
				IPC_ROUTER_LOG_EVENT_TX, msg, hdr, NULL, NULL);
		ret = post_control_ports(pkt);
	} else if (dst_node_id == IPC_ROUTER_NID_LOCAL &&
		   msg->cmd == IPC_ROUTER_CTRL_CMD_RESUME_TX) {
		ipc_router_log_msg(local_log_ctx,
				IPC_ROUTER_LOG_EVENT_TX, msg, hdr, NULL, NULL);
		ret = process_resume_tx_msg(msg, pkt);
	} else if (xprt_info && (msg->cmd == IPC_ROUTER_CTRL_CMD_HELLO ||
		   xprt_info->initialized)) {
		mutex_lock(&xprt_info->tx_lock_lhb2);
		ipc_router_log_msg(xprt_info->log_ctx,
				IPC_ROUTER_LOG_EVENT_TX, msg, hdr, NULL, NULL);
		ret = prepend_header(pkt, xprt_info);
		if (ret < 0) {
			mutex_unlock(&xprt_info->tx_lock_lhb2);
			IPC_RTR_ERR("%s: Prepend Header failed\n", __func__);
			release_pkt(pkt);
			return ret;
		}

		ret = xprt_info->xprt->write(pkt, pkt->length, xprt_info->xprt);
		mutex_unlock(&xprt_info->tx_lock_lhb2);
	}

	release_pkt(pkt);
	return ret;
}

static int msm_ipc_router_send_server_list(uint32_t node_id,
		struct msm_ipc_router_xprt_info *xprt_info)
{
	union rr_control_msg ctl;
	struct msm_ipc_server *server;
	struct msm_ipc_server_port *server_port;
	int i;

	if (!xprt_info || !xprt_info->initialized) {
		IPC_RTR_ERR("%s: Xprt info not initialized\n", __func__);
		return -EINVAL;
	}

	memset(&ctl, 0, sizeof(ctl));
	ctl.cmd = IPC_ROUTER_CTRL_CMD_NEW_SERVER;

	for (i = 0; i < SRV_HASH_SIZE; i++) {
		list_for_each_entry(server, &server_list[i], list) {
			ctl.srv.service = server->name.service;
			ctl.srv.instance = server->name.instance;
			list_for_each_entry(server_port,
					    &server->server_port_list, list) {
				if (server_port->server_addr.node_id !=
				    node_id)
					continue;

				ctl.srv.node_id =
					server_port->server_addr.node_id;
				ctl.srv.port_id =
					server_port->server_addr.port_id;
				ipc_router_send_ctl_msg(xprt_info,
					&ctl, IPC_ROUTER_DUMMY_DEST_NODE);
			}
		}
	}

	return 0;
}

static int broadcast_ctl_msg_locally(union rr_control_msg *msg)
{
	return ipc_router_send_ctl_msg(NULL, msg, IPC_ROUTER_NID_LOCAL);
}

static int broadcast_ctl_msg(union rr_control_msg *ctl)
{
	struct msm_ipc_router_xprt_info *xprt_info;

	down_read(&xprt_info_list_lock_lha5);
	list_for_each_entry(xprt_info, &xprt_info_list, list) {
		ipc_router_send_ctl_msg(xprt_info, ctl,
					IPC_ROUTER_DUMMY_DEST_NODE);
	}
	up_read(&xprt_info_list_lock_lha5);
	broadcast_ctl_msg_locally(ctl);

	return 0;
}

static int relay_ctl_msg(struct msm_ipc_router_xprt_info *xprt_info,
			 union rr_control_msg *ctl)
{
	struct msm_ipc_router_xprt_info *fwd_xprt_info;

	if (!xprt_info || !ctl)
		return -EINVAL;

	down_read(&xprt_info_list_lock_lha5);
	list_for_each_entry(fwd_xprt_info, &xprt_info_list, list) {
		if (xprt_info->xprt->link_id != fwd_xprt_info->xprt->link_id)
			ipc_router_send_ctl_msg(fwd_xprt_info, ctl,
						IPC_ROUTER_DUMMY_DEST_NODE);
	}
	up_read(&xprt_info_list_lock_lha5);

	return 0;
}

static int forward_msg(struct msm_ipc_router_xprt_info *xprt_info,
		       struct rr_packet *pkt)
{
	struct rr_header_v1 *hdr;
	struct msm_ipc_router_xprt_info *fwd_xprt_info;
	struct msm_ipc_routing_table_entry *rt_entry;
	int ret = 0;
	int fwd_xprt_option;

	if (!xprt_info || !pkt)
		return -EINVAL;

	hdr = &(pkt->hdr);
	rt_entry = ipc_router_get_rtentry_ref(hdr->dst_node_id);
	if (!(rt_entry) || !(rt_entry->xprt_info)) {
		IPC_RTR_ERR("%s: Routing table not initialized\n", __func__);
		ret = -ENODEV;
		goto fm_error1;
	}

	down_read(&rt_entry->lock_lha4);
	fwd_xprt_info = rt_entry->xprt_info;
	ret = ipc_router_get_xprt_info_ref(fwd_xprt_info);
	if (ret < 0) {
		IPC_RTR_ERR("%s: Abort invalid xprt\n", __func__);
		goto fm_error_xprt;
	}
	ret = prepend_header(pkt, fwd_xprt_info);
	if (ret < 0) {
		IPC_RTR_ERR("%s: Prepend Header failed\n", __func__);
		goto fm_error2;
	}
	fwd_xprt_option = fwd_xprt_info->xprt->get_option(fwd_xprt_info->xprt);
	if (!(fwd_xprt_option & FRAG_PKT_WRITE_ENABLE)) {
		ret = defragment_pkt(pkt);
		if (ret < 0)
			goto fm_error2;
	}

	mutex_lock(&fwd_xprt_info->tx_lock_lhb2);
	if (xprt_info->remote_node_id == fwd_xprt_info->remote_node_id) {
		IPC_RTR_ERR("%s: Discarding Command to route back\n", __func__);
		ret = -EINVAL;
		goto fm_error3;
	}

	if (xprt_info->xprt->link_id == fwd_xprt_info->xprt->link_id) {
		IPC_RTR_ERR("%s: DST in the same cluster\n", __func__);
		ret = 0;
		goto fm_error3;
	}
	fwd_xprt_info->xprt->write(pkt, pkt->length, fwd_xprt_info->xprt);
	IPC_RTR_INFO(fwd_xprt_info->log_ctx,
		"%s %s Len:0x%x T:0x%x CF:0x%x SRC:<0x%x:0x%x> DST:<0x%x:0x%x>\n",
		"FWD", "TX", hdr->size, hdr->type, hdr->control_flag,
		hdr->src_node_id, hdr->src_port_id,
		hdr->dst_node_id, hdr->dst_port_id);

fm_error3:
	mutex_unlock(&fwd_xprt_info->tx_lock_lhb2);
fm_error2:
	ipc_router_put_xprt_info_ref(fwd_xprt_info);
fm_error_xprt:
	up_read(&rt_entry->lock_lha4);
fm_error1:
	if (rt_entry)
		kref_put(&rt_entry->ref, ipc_router_release_rtentry);
	return ret;
}

static int msm_ipc_router_send_remove_client(struct comm_mode_info *mode_info,
					uint32_t node_id, uint32_t port_id)
{
	union rr_control_msg msg;
	struct msm_ipc_router_xprt_info *tmp_xprt_info;
	int mode;
	void *xprt_info;
	int rc = 0;

	if (!mode_info) {
		IPC_RTR_ERR("%s: NULL mode_info\n", __func__);
		return -EINVAL;
	}
	mode = mode_info->mode;
	xprt_info = mode_info->xprt_info;

	memset(&msg, 0, sizeof(msg));
	msg.cmd = IPC_ROUTER_CTRL_CMD_REMOVE_CLIENT;
	msg.cli.node_id = node_id;
	msg.cli.port_id = port_id;

	if ((mode == SINGLE_LINK_MODE) && xprt_info) {
		down_read(&xprt_info_list_lock_lha5);
		list_for_each_entry(tmp_xprt_info, &xprt_info_list, list) {
			if (tmp_xprt_info != xprt_info)
				continue;
			ipc_router_send_ctl_msg(tmp_xprt_info, &msg,
						IPC_ROUTER_DUMMY_DEST_NODE);
			break;
		}
		up_read(&xprt_info_list_lock_lha5);
	} else if ((mode == SINGLE_LINK_MODE) && !xprt_info) {
		broadcast_ctl_msg_locally(&msg);
	} else if (mode == MULTI_LINK_MODE) {
		broadcast_ctl_msg(&msg);
	} else if (mode != NULL_MODE) {
		IPC_RTR_ERR(
		"%s: Invalid mode(%d) + xprt_inf(%p) for %08x:%08x\n",
			__func__, mode, xprt_info, node_id, port_id);
		rc = -EINVAL;
	}
	return rc;
}

static void update_comm_mode_info(struct comm_mode_info *mode_info,
				  struct msm_ipc_router_xprt_info *xprt_info)
{
	if (!mode_info) {
		IPC_RTR_ERR("%s: NULL mode_info\n", __func__);
		return;
	}

	if (mode_info->mode == NULL_MODE) {
		mode_info->xprt_info = xprt_info;
		mode_info->mode = SINGLE_LINK_MODE;
	} else if (mode_info->mode == SINGLE_LINK_MODE &&
		   mode_info->xprt_info != xprt_info) {
		mode_info->mode = MULTI_LINK_MODE;
	}

	return;
}

/**
 * cleanup_rmt_server() - Cleanup server hosted in the remote port
 * @xprt_info: XPRT through which this cleanup event is handled.
 * @rport_ptr: Remote port that is being cleaned up.
 * @server: Server that is hosted in the remote port.
 */
static void cleanup_rmt_server(struct msm_ipc_router_xprt_info *xprt_info,
			       struct msm_ipc_router_remote_port *rport_ptr,
			       struct msm_ipc_server *server)
{
	union rr_control_msg ctl;

	memset(&ctl, 0, sizeof(ctl));
	ctl.cmd = IPC_ROUTER_CTRL_CMD_REMOVE_SERVER;
	ctl.srv.service = server->name.service;
	ctl.srv.instance = server->name.instance;
	ctl.srv.node_id = rport_ptr->node_id;
	ctl.srv.port_id = rport_ptr->port_id;
	if (xprt_info)
		relay_ctl_msg(xprt_info, &ctl);
	broadcast_ctl_msg_locally(&ctl);
	ipc_router_destroy_server_nolock(server,
			rport_ptr->node_id, rport_ptr->port_id);
}

static void cleanup_rmt_ports(struct msm_ipc_router_xprt_info *xprt_info,
			      struct msm_ipc_routing_table_entry *rt_entry)
{
	struct msm_ipc_router_remote_port *rport_ptr, *tmp_rport_ptr;
	struct msm_ipc_server *server;
	union rr_control_msg ctl;
	int j;

	memset(&ctl, 0, sizeof(ctl));
	for (j = 0; j < RP_HASH_SIZE; j++) {
		list_for_each_entry_safe(rport_ptr, tmp_rport_ptr,
				&rt_entry->remote_port_list[j], list) {
			list_del(&rport_ptr->list);
			mutex_lock(&rport_ptr->rport_lock_lhb2);
			server = rport_ptr->server;
			rport_ptr->server = NULL;
			mutex_unlock(&rport_ptr->rport_lock_lhb2);
			ipc_router_reset_conn(rport_ptr);
			if (server) {
				cleanup_rmt_server(xprt_info, rport_ptr,
						   server);
				server = NULL;
			}

			ctl.cmd = IPC_ROUTER_CTRL_CMD_REMOVE_CLIENT;
			ctl.cli.node_id = rport_ptr->node_id;
			ctl.cli.port_id = rport_ptr->port_id;
			kref_put(&rport_ptr->ref, ipc_router_release_rport);

			relay_ctl_msg(xprt_info, &ctl);
			broadcast_ctl_msg_locally(&ctl);
		}
	}
}

static void msm_ipc_cleanup_routing_table(
	struct msm_ipc_router_xprt_info *xprt_info)
{
	int i;
	struct msm_ipc_routing_table_entry *rt_entry, *tmp_rt_entry;

	if (!xprt_info) {
		IPC_RTR_ERR("%s: Invalid xprt_info\n", __func__);
		return;
	}

	down_write(&server_list_lock_lha2);
	down_write(&routing_table_lock_lha3);
	for (i = 0; i < RT_HASH_SIZE; i++) {
		list_for_each_entry_safe(rt_entry, tmp_rt_entry,
					 &routing_table[i], list) {
			down_write(&rt_entry->lock_lha4);
			if (rt_entry->xprt_info != xprt_info) {
				up_write(&rt_entry->lock_lha4);
				continue;
			}
			cleanup_rmt_ports(xprt_info, rt_entry);
			rt_entry->xprt_info = NULL;
			up_write(&rt_entry->lock_lha4);
			list_del(&rt_entry->list);
			kref_put(&rt_entry->ref, ipc_router_release_rtentry);
		}
	}
	up_write(&routing_table_lock_lha3);
	up_write(&server_list_lock_lha2);
}

/**
 * sync_sec_rule() - Synchrnoize the security rule into the server structure
 * @server: Server structure where the rule has to be synchronized.
 * @rule: Security tule to be synchronized.
 *
 * This function is used to update the server structure with the security
 * rule configured for the <service:instance> corresponding to that server.
 */
static void sync_sec_rule(struct msm_ipc_server *server, void *rule)
{
	struct msm_ipc_server_port *server_port;
	struct msm_ipc_router_remote_port *rport_ptr = NULL;

	list_for_each_entry(server_port, &server->server_port_list, list) {
		rport_ptr = ipc_router_get_rport_ref(
				server_port->server_addr.node_id,
				server_port->server_addr.port_id);
		if (!rport_ptr)
			continue;
		rport_ptr->sec_rule = rule;
		kref_put(&rport_ptr->ref, ipc_router_release_rport);
	}
	server->synced_sec_rule = 1;
}

/**
 * msm_ipc_sync_sec_rule() - Sync the security rule to the service
 * @service: Service for which the rule has to be synchronized.
 * @instance: Instance for which the rule has to be synchronized.
 * @rule: Security rule to be synchronized.
 *
 * This function is used to syncrhonize the security rule with the server
 * hash table, if the user-space script configures the rule after the service
 * has come up. This function is used to synchronize the security rule to a
 * specific service and optionally a specific instance.
 */
void msm_ipc_sync_sec_rule(uint32_t service, uint32_t instance, void *rule)
{
	int key = (service & (SRV_HASH_SIZE - 1));
	struct msm_ipc_server *server;

	down_write(&server_list_lock_lha2);
	list_for_each_entry(server, &server_list[key], list) {
		if (server->name.service != service)
			continue;

		if (server->name.instance != instance &&
		    instance != ALL_INSTANCE)
			continue;

		/* If the rule applies to all instances and if the specific
		 * instance of a service has a rule synchronized already,
		 * do not apply the rule for that specific instance.
		 */
		if (instance == ALL_INSTANCE && server->synced_sec_rule)
			continue;

		sync_sec_rule(server, rule);
	}
	up_write(&server_list_lock_lha2);
}

/**
 * msm_ipc_sync_default_sec_rule() - Default security rule to all services
 * @rule: Security rule to be synchronized.
 *
 * This function is used to syncrhonize the security rule with the server
 * hash table, if the user-space script configures the rule after the service
 * has come up. This function is used to synchronize the security rule that
 * applies to all services, if the concerned service do not have any rule
 * defined.
 */
void msm_ipc_sync_default_sec_rule(void *rule)
{
	int key;
	struct msm_ipc_server *server;

	down_write(&server_list_lock_lha2);
	for (key = 0; key < SRV_HASH_SIZE; key++) {
		list_for_each_entry(server, &server_list[key], list) {
			if (server->synced_sec_rule)
				continue;

			sync_sec_rule(server, rule);
		}
	}
	up_write(&server_list_lock_lha2);
}

/**
 * ipc_router_reset_conn() - Reset the connection to remote port
 * @rport_ptr: Pointer to the remote port to be disconnected.
 *
 * This function is used to reset all the local ports that are connected to
 * the remote port being passed.
 */
static void ipc_router_reset_conn(struct msm_ipc_router_remote_port *rport_ptr)
{
	struct msm_ipc_port *port_ptr;
	struct ipc_router_conn_info *conn_info, *tmp_conn_info;

	mutex_lock(&rport_ptr->rport_lock_lhb2);
	list_for_each_entry_safe(conn_info, tmp_conn_info,
				&rport_ptr->conn_info_list, list) {
		port_ptr = ipc_router_get_port_ref(conn_info->port_id);
		if (port_ptr) {
			mutex_lock(&port_ptr->port_lock_lhc3);
			port_ptr->conn_status = CONNECTION_RESET;
			mutex_unlock(&port_ptr->port_lock_lhc3);
			wake_up(&port_ptr->port_rx_wait_q);
			kref_put(&port_ptr->ref, ipc_router_release_port);
		}

		list_del(&conn_info->list);
		kfree(conn_info);
	}
	mutex_unlock(&rport_ptr->rport_lock_lhb2);
}

/**
 * ipc_router_set_conn() - Set the connection by initializing dest address
 * @port_ptr: Local port in which the connection has to be set.
 * @addr: Destination address of the connection.
 *
 * @return: 0 on success, standard Linux error codes on failure.
 */
int ipc_router_set_conn(struct msm_ipc_port *port_ptr,
			struct msm_ipc_addr *addr)
{
	struct msm_ipc_router_remote_port *rport_ptr;
	struct ipc_router_conn_info *conn_info;

	if (unlikely(!port_ptr || !addr))
		return -EINVAL;

	if (addr->addrtype != MSM_IPC_ADDR_ID) {
		IPC_RTR_ERR("%s: Invalid Address type\n", __func__);
		return -EINVAL;
	}

	if (port_ptr->type == SERVER_PORT) {
		IPC_RTR_ERR("%s: Connection refused on a server port\n",
			    __func__);
		return -ECONNREFUSED;
	}

	if (port_ptr->conn_status == CONNECTED) {
		IPC_RTR_ERR("%s: Port %08x already connected\n",
			    __func__, port_ptr->this_port.port_id);
		return -EISCONN;
	}

	conn_info = kzalloc(sizeof(struct ipc_router_conn_info), GFP_KERNEL);
	if (!conn_info) {
		IPC_RTR_ERR("%s: Error allocating conn_info\n", __func__);
		return -ENOMEM;
	}
	INIT_LIST_HEAD(&conn_info->list);
	conn_info->port_id = port_ptr->this_port.port_id;

	rport_ptr = ipc_router_get_rport_ref(addr->addr.port_addr.node_id,
					     addr->addr.port_addr.port_id);
	if (!rport_ptr) {
		IPC_RTR_ERR("%s: Invalid remote endpoint\n", __func__);
		kfree(conn_info);
		return -ENODEV;
	}
	mutex_lock(&rport_ptr->rport_lock_lhb2);
	list_add_tail(&conn_info->list, &rport_ptr->conn_info_list);
	mutex_unlock(&rport_ptr->rport_lock_lhb2);

	mutex_lock(&port_ptr->port_lock_lhc3);
	memcpy(&port_ptr->dest_addr, &addr->addr.port_addr,
	       sizeof(struct msm_ipc_port_addr));
	port_ptr->conn_status = CONNECTED;
	mutex_unlock(&port_ptr->port_lock_lhc3);
	kref_put(&rport_ptr->ref, ipc_router_release_rport);
	return 0;
}

/**
 * do_version_negotiation() - perform a version negotiation and set the version
 * @xprt_info:	Pointer to the IPC Router transport info structure.
 * @msg:	Pointer to the IPC Router HELLO message.
 *
 * This function performs the version negotiation by verifying the computed
 * checksum first. If the checksum matches with the magic number, it sets the
 * negotiated IPC Router version in transport.
 */
static void do_version_negotiation(struct msm_ipc_router_xprt_info *xprt_info,
				   union rr_control_msg *msg)
{
	uint32_t magic;
	unsigned version;

	if (!xprt_info)
		return;
	magic = ipc_router_calc_checksum(msg);
	if (magic == IPC_ROUTER_HELLO_MAGIC) {
		version = fls(msg->hello.versions & IPC_ROUTER_VER_BITMASK) - 1;
		/*Bit 0 & 31 are reserved for future usage*/
		if ((version > 0) &&
		    (version != (sizeof(version) * BITS_PER_BYTE - 1)) &&
			xprt_info->xprt->set_version)
			xprt_info->xprt->set_version(xprt_info->xprt, version);
	}
}

static int process_hello_msg(struct msm_ipc_router_xprt_info *xprt_info,
				union rr_control_msg *msg,
				struct rr_header_v1 *hdr)
{
	int i, rc = 0;
	union rr_control_msg ctl;
	struct msm_ipc_routing_table_entry *rt_entry;

	if (!hdr)
		return -EINVAL;

	xprt_info->remote_node_id = hdr->src_node_id;
	rt_entry = create_routing_table_entry(hdr->src_node_id, xprt_info);
	if (!rt_entry) {
		IPC_RTR_ERR("%s: rt_entry allocation failed\n", __func__);
		return -ENOMEM;
	}
	kref_put(&rt_entry->ref, ipc_router_release_rtentry);

	do_version_negotiation(xprt_info, msg);
	/* Send a reply HELLO message */
	memset(&ctl, 0, sizeof(ctl));
	ctl.hello.cmd = IPC_ROUTER_CTRL_CMD_HELLO;
	ctl.hello.checksum = IPC_ROUTER_HELLO_MAGIC;
	ctl.hello.versions = (uint32_t)IPC_ROUTER_VER_BITMASK;
	ctl.hello.checksum = ipc_router_calc_checksum(&ctl);
	rc = ipc_router_send_ctl_msg(xprt_info, &ctl,
				     IPC_ROUTER_DUMMY_DEST_NODE);
	if (rc < 0) {
		IPC_RTR_ERR("%s: Error sending reply HELLO message\n",
								__func__);
		return rc;
	}
	xprt_info->initialized = 1;

	/* Send list of servers from the local node and from nodes
	 * outside the mesh network in which this XPRT is part of.
	 */
	down_read(&server_list_lock_lha2);
	down_read(&routing_table_lock_lha3);
	for (i = 0; i < RT_HASH_SIZE; i++) {
		list_for_each_entry(rt_entry, &routing_table[i], list) {
			if ((rt_entry->node_id != IPC_ROUTER_NID_LOCAL) &&
			    (!rt_entry->xprt_info ||
			     (rt_entry->xprt_info->xprt->link_id ==
			      xprt_info->xprt->link_id)))
				continue;
			rc = msm_ipc_router_send_server_list(rt_entry->node_id,
							     xprt_info);
			if (rc < 0) {
				up_read(&routing_table_lock_lha3);
				up_read(&server_list_lock_lha2);
				return rc;
			}
		}
	}
	up_read(&routing_table_lock_lha3);
	up_read(&server_list_lock_lha2);
	return rc;
}

static int process_resume_tx_msg(union rr_control_msg *msg,
				 struct rr_packet *pkt)
{
	struct msm_ipc_router_remote_port *rport_ptr;


	rport_ptr = ipc_router_get_rport_ref(msg->cli.node_id,
					     msg->cli.port_id);
	if (!rport_ptr) {
		IPC_RTR_ERR("%s: Unable to resume client\n", __func__);
		return -ENODEV;
	}
	mutex_lock(&rport_ptr->rport_lock_lhb2);
	rport_ptr->tx_quota_cnt = 0;
	post_resume_tx(rport_ptr, pkt, msg);
	mutex_unlock(&rport_ptr->rport_lock_lhb2);
	kref_put(&rport_ptr->ref, ipc_router_release_rport);
	return 0;
}

static int process_new_server_msg(struct msm_ipc_router_xprt_info *xprt_info,
			union rr_control_msg *msg, struct rr_packet *pkt)
{
	struct msm_ipc_routing_table_entry *rt_entry;
	struct msm_ipc_server *server;
	struct msm_ipc_router_remote_port *rport_ptr;

	if (msg->srv.instance == 0) {
		IPC_RTR_ERR("%s: Server %08x create rejected, version = 0\n",
			__func__, msg->srv.service);
		return -EINVAL;
	}

	rt_entry = ipc_router_get_rtentry_ref(msg->srv.node_id);
	if (!rt_entry) {
		rt_entry = create_routing_table_entry(msg->srv.node_id,
						      xprt_info);
		if (!rt_entry) {
			IPC_RTR_ERR("%s: rt_entry allocation failed\n",
								__func__);
			return -ENOMEM;
		}
	}
	kref_put(&rt_entry->ref, ipc_router_release_rtentry);

	/* If the service already exists in the table, create_server returns
	 * a reference to it.
	 */
	rport_ptr = ipc_router_create_rport(msg->srv.node_id,
				msg->srv.port_id, xprt_info);
	if (!rport_ptr)
		return -ENOMEM;

	server = msm_ipc_router_create_server(
			msg->srv.service, msg->srv.instance,
			msg->srv.node_id, msg->srv.port_id, xprt_info);
	if (!server) {
		IPC_RTR_ERR("%s: Server %08x:%08x Create failed\n",
			    __func__, msg->srv.service, msg->srv.instance);
		kref_put(&rport_ptr->ref, ipc_router_release_rport);
		ipc_router_destroy_rport(rport_ptr);
		return -ENOMEM;
	}
	mutex_lock(&rport_ptr->rport_lock_lhb2);
	rport_ptr->server = server;
	mutex_unlock(&rport_ptr->rport_lock_lhb2);
	rport_ptr->sec_rule = msm_ipc_get_security_rule(
					msg->srv.service, msg->srv.instance);
	kref_put(&rport_ptr->ref, ipc_router_release_rport);
	kref_put(&server->ref, ipc_router_release_server);

	/* Relay the new server message to other subsystems that do not belong
	 * to the cluster from which this message is received. Notify the
	 * local clients waiting for this service.
	 */
	relay_ctl_msg(xprt_info, msg);
	post_control_ports(pkt);
	return 0;
}

static int process_rmv_server_msg(struct msm_ipc_router_xprt_info *xprt_info,
			union rr_control_msg *msg, struct rr_packet *pkt)
{
	struct msm_ipc_server *server;
	struct msm_ipc_router_remote_port *rport_ptr;

	server = ipc_router_get_server_ref(msg->srv.service, msg->srv.instance,
					   msg->srv.node_id, msg->srv.port_id);
	rport_ptr = ipc_router_get_rport_ref(msg->srv.node_id,
					     msg->srv.port_id);
	if (rport_ptr) {
		mutex_lock(&rport_ptr->rport_lock_lhb2);
		if (rport_ptr->server == server)
			rport_ptr->server = NULL;
		mutex_unlock(&rport_ptr->rport_lock_lhb2);
		kref_put(&rport_ptr->ref, ipc_router_release_rport);
	}

	if (server) {
		kref_put(&server->ref, ipc_router_release_server);
		ipc_router_destroy_server(server, msg->srv.node_id,
					  msg->srv.port_id);
		/*
		 * Relay the new server message to other subsystems that do not
		 * belong to the cluster from which this message is received.
		 * Notify the local clients communicating with the service.
		 */
		relay_ctl_msg(xprt_info, msg);
		post_control_ports(pkt);
	}
	return 0;
}

static int process_rmv_client_msg(struct msm_ipc_router_xprt_info *xprt_info,
			union rr_control_msg *msg, struct rr_packet *pkt)
{
	struct msm_ipc_router_remote_port *rport_ptr;
	struct msm_ipc_server *server;

	rport_ptr = ipc_router_get_rport_ref(msg->cli.node_id,
					     msg->cli.port_id);
	if (rport_ptr) {
		mutex_lock(&rport_ptr->rport_lock_lhb2);
		server = rport_ptr->server;
		rport_ptr->server = NULL;
		mutex_unlock(&rport_ptr->rport_lock_lhb2);
		ipc_router_reset_conn(rport_ptr);
		down_write(&server_list_lock_lha2);
		if (server)
			cleanup_rmt_server(NULL, rport_ptr, server);
		up_write(&server_list_lock_lha2);
		kref_put(&rport_ptr->ref, ipc_router_release_rport);
		ipc_router_destroy_rport(rport_ptr);
	}

	relay_ctl_msg(xprt_info, msg);
	post_control_ports(pkt);
	return 0;
}

static int process_control_msg(struct msm_ipc_router_xprt_info *xprt_info,
			       struct rr_packet *pkt)
{
	union rr_control_msg *msg;
	int rc = 0;
	struct rr_header_v1 *hdr;

	if (pkt->length != sizeof(*msg)) {
		IPC_RTR_ERR("%s: r2r msg size %d != %zu\n",
				__func__, pkt->length, sizeof(*msg));
		return -EINVAL;
	}

	hdr = &(pkt->hdr);
	msg = msm_ipc_router_skb_to_buf(pkt->pkt_fragment_q, sizeof(*msg));
	if (!msg) {
		IPC_RTR_ERR("%s: Error extracting control msg\n", __func__);
		return -ENOMEM;
	}

	ipc_router_log_msg(xprt_info->log_ctx, IPC_ROUTER_LOG_EVENT_RX,
					msg, hdr, NULL, NULL);

	switch (msg->cmd) {
	case IPC_ROUTER_CTRL_CMD_HELLO:
		rc = process_hello_msg(xprt_info, msg, hdr);
		break;
	case IPC_ROUTER_CTRL_CMD_RESUME_TX:
		rc = process_resume_tx_msg(msg, pkt);
		break;
	case IPC_ROUTER_CTRL_CMD_NEW_SERVER:
		rc = process_new_server_msg(xprt_info, msg, pkt);
		break;
	case IPC_ROUTER_CTRL_CMD_REMOVE_SERVER:
		rc = process_rmv_server_msg(xprt_info, msg, pkt);
		break;
	case IPC_ROUTER_CTRL_CMD_REMOVE_CLIENT:
		rc = process_rmv_client_msg(xprt_info, msg, pkt);
		break;
	default:
		rc = -ENOSYS;
	}
	kfree(msg);
	return rc;
}

static void do_read_data(struct work_struct *work)
{
	struct rr_header_v1 *hdr;
	struct rr_packet *pkt = NULL;
	struct msm_ipc_port *port_ptr;
	struct msm_ipc_router_remote_port *rport_ptr;
	int ret;

	struct msm_ipc_router_xprt_info *xprt_info =
		container_of(work,
			     struct msm_ipc_router_xprt_info,
			     read_data);

	while ((pkt = rr_read(xprt_info)) != NULL) {
		if (pkt->length < calc_rx_header_size(xprt_info) ||
		    pkt->length > MAX_IPC_PKT_SIZE) {
			IPC_RTR_ERR("%s: Invalid pkt length %d\n",
				__func__, pkt->length);
			goto read_next_pkt1;
		}

		ret = extract_header(pkt);
		if (ret < 0)
			goto read_next_pkt1;
		hdr = &(pkt->hdr);

		if ((hdr->dst_node_id != IPC_ROUTER_NID_LOCAL) &&
		    ((hdr->type == IPC_ROUTER_CTRL_CMD_RESUME_TX) ||
		     (hdr->type == IPC_ROUTER_CTRL_CMD_DATA))) {
			IPC_RTR_INFO(xprt_info->log_ctx,
			"%s %s Len:0x%x T:0x%x CF:0x%x SRC:<0x%x:0x%x> DST:<0x%x:0x%x>\n",
			"FWD", "RX", hdr->size, hdr->type, hdr->control_flag,
			hdr->src_node_id, hdr->src_port_id,
			hdr->dst_node_id, hdr->dst_port_id);
			forward_msg(xprt_info, pkt);
			goto read_next_pkt1;
		}

		if (hdr->type != IPC_ROUTER_CTRL_CMD_DATA) {
			process_control_msg(xprt_info, pkt);
			goto read_next_pkt1;
		}

		if (msm_ipc_router_debug_mask & SMEM_LOG) {
			smem_log_event((SMEM_LOG_PROC_ID_APPS |
				SMEM_LOG_IPC_ROUTER_EVENT_BASE |
				IPC_ROUTER_LOG_EVENT_RX),
				(hdr->src_node_id << 24) |
				(hdr->src_port_id & 0xffffff),
				(hdr->dst_node_id << 24) |
				(hdr->dst_port_id & 0xffffff),
				(hdr->type << 24) | (hdr->control_flag << 16) |
				(hdr->size & 0xffff));
		}

		port_ptr = ipc_router_get_port_ref(hdr->dst_port_id);
		if (!port_ptr) {
			IPC_RTR_ERR("%s: No local port id %08x\n", __func__,
				hdr->dst_port_id);
			goto read_next_pkt1;
		}

		rport_ptr = ipc_router_get_rport_ref(hdr->src_node_id,
						     hdr->src_port_id);
		if (!rport_ptr) {
			rport_ptr = ipc_router_create_rport(hdr->src_node_id,
						hdr->src_port_id, xprt_info);
			if (!rport_ptr) {
				IPC_RTR_ERR(
				"%s: Rmt Prt %08x:%08x create failed\n",
				__func__, hdr->src_node_id, hdr->src_port_id);
				goto read_next_pkt2;
			}
		}

		ipc_router_log_msg(xprt_info->log_ctx, IPC_ROUTER_LOG_EVENT_RX,
				pkt, hdr, port_ptr, rport_ptr);
		kref_put(&rport_ptr->ref, ipc_router_release_rport);
		post_pkt_to_port(port_ptr, pkt, 0);
		kref_put(&port_ptr->ref, ipc_router_release_port);
		continue;
read_next_pkt2:
		kref_put(&port_ptr->ref, ipc_router_release_port);
read_next_pkt1:
		release_pkt(pkt);
	}
}

int msm_ipc_router_register_server(struct msm_ipc_port *port_ptr,
				   struct msm_ipc_addr *name)
{
	struct msm_ipc_server *server;
	union rr_control_msg ctl;
	struct msm_ipc_router_remote_port *rport_ptr;

	if (!port_ptr || !name)
		return -EINVAL;

	if (port_ptr->type != CLIENT_PORT)
		return -EINVAL;

	if (name->addrtype != MSM_IPC_ADDR_NAME)
		return -EINVAL;

	rport_ptr = ipc_router_create_rport(IPC_ROUTER_NID_LOCAL,
			port_ptr->this_port.port_id, NULL);
	if (!rport_ptr) {
		IPC_RTR_ERR("%s: RPort %08x:%08x creation failed\n", __func__,
			    IPC_ROUTER_NID_LOCAL, port_ptr->this_port.port_id);
		return -ENOMEM;
	}

	server = msm_ipc_router_create_server(name->addr.port_name.service,
					      name->addr.port_name.instance,
					      IPC_ROUTER_NID_LOCAL,
					      port_ptr->this_port.port_id,
					      NULL);
	if (!server) {
		IPC_RTR_ERR("%s: Server %08x:%08x Create failed\n",
			    __func__, name->addr.port_name.service,
			    name->addr.port_name.instance);
		kref_put(&rport_ptr->ref, ipc_router_release_rport);
		ipc_router_destroy_rport(rport_ptr);
		return -ENOMEM;
	}

	memset(&ctl, 0, sizeof(ctl));
	ctl.cmd = IPC_ROUTER_CTRL_CMD_NEW_SERVER;
	ctl.srv.service = server->name.service;
	ctl.srv.instance = server->name.instance;
	ctl.srv.node_id = IPC_ROUTER_NID_LOCAL;
	ctl.srv.port_id = port_ptr->this_port.port_id;
	broadcast_ctl_msg(&ctl);
	mutex_lock(&port_ptr->port_lock_lhc3);
	port_ptr->type = SERVER_PORT;
	port_ptr->mode_info.mode = MULTI_LINK_MODE;
	port_ptr->port_name.service = server->name.service;
	port_ptr->port_name.instance = server->name.instance;
	port_ptr->rport_info = rport_ptr;
	mutex_unlock(&port_ptr->port_lock_lhc3);
	kref_put(&rport_ptr->ref, ipc_router_release_rport);
	kref_put(&server->ref, ipc_router_release_server);
	return 0;
}

int msm_ipc_router_unregister_server(struct msm_ipc_port *port_ptr)
{
	struct msm_ipc_server *server;
	union rr_control_msg ctl;
	struct msm_ipc_router_remote_port *rport_ptr;

	if (!port_ptr)
		return -EINVAL;

	if (port_ptr->type != SERVER_PORT) {
		IPC_RTR_ERR("%s: Trying to unregister a non-server port\n",
			__func__);
		return -EINVAL;
	}

	if (port_ptr->this_port.node_id != IPC_ROUTER_NID_LOCAL) {
		IPC_RTR_ERR(
		"%s: Trying to unregister a remote server locally\n",
			__func__);
		return -EINVAL;
	}

	server = ipc_router_get_server_ref(port_ptr->port_name.service,
					   port_ptr->port_name.instance,
					   port_ptr->this_port.node_id,
					   port_ptr->this_port.port_id);
	if (!server) {
		IPC_RTR_ERR("%s: Server lookup failed\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&port_ptr->port_lock_lhc3);
	port_ptr->type = CLIENT_PORT;
	rport_ptr = (struct msm_ipc_router_remote_port *)port_ptr->rport_info;
	mutex_unlock(&port_ptr->port_lock_lhc3);
	if (rport_ptr)
		ipc_router_reset_conn(rport_ptr);
	memset(&ctl, 0, sizeof(ctl));
	ctl.cmd = IPC_ROUTER_CTRL_CMD_REMOVE_SERVER;
	ctl.srv.service = server->name.service;
	ctl.srv.instance = server->name.instance;
	ctl.srv.node_id = IPC_ROUTER_NID_LOCAL;
	ctl.srv.port_id = port_ptr->this_port.port_id;
	kref_put(&server->ref, ipc_router_release_server);
	ipc_router_destroy_server(server, port_ptr->this_port.node_id,
				  port_ptr->this_port.port_id);
	broadcast_ctl_msg(&ctl);
	mutex_lock(&port_ptr->port_lock_lhc3);
	port_ptr->type = CLIENT_PORT;
	mutex_unlock(&port_ptr->port_lock_lhc3);
	return 0;
}

static int loopback_data(struct msm_ipc_port *src,
			uint32_t port_id,
			struct rr_packet *pkt)
{
	struct msm_ipc_port *port_ptr;
	struct sk_buff *temp_skb;
	int align_size;

	if (!pkt) {
		IPC_RTR_ERR("%s: Invalid pkt pointer\n", __func__);
		return -EINVAL;
	}

	temp_skb = skb_peek_tail(pkt->pkt_fragment_q);
	align_size = ALIGN_SIZE(pkt->length);
	skb_put(temp_skb, align_size);
	pkt->length += align_size;

	port_ptr = ipc_router_get_port_ref(port_id);
	if (!port_ptr) {
		IPC_RTR_ERR("%s: Local port %d not present\n",
						__func__, port_id);
		return -ENODEV;
	}
	post_pkt_to_port(port_ptr, pkt, 1);
	update_comm_mode_info(&src->mode_info, NULL);
	kref_put(&port_ptr->ref, ipc_router_release_port);

	return pkt->hdr.size;
}

static int ipc_router_tx_wait(struct msm_ipc_port *src,
			      struct msm_ipc_router_remote_port *rport_ptr,
			      uint32_t *set_confirm_rx,
			      long timeout)
{
	struct msm_ipc_resume_tx_port *resume_tx_port;
	int ret;

	if (unlikely(!src || !rport_ptr))
		return -EINVAL;

	for (;;) {
		mutex_lock(&rport_ptr->rport_lock_lhb2);
		if (rport_ptr->status == RESET) {
			mutex_unlock(&rport_ptr->rport_lock_lhb2);
			IPC_RTR_ERR("%s: RPort %08x:%08x is in reset state\n",
			    __func__, rport_ptr->node_id, rport_ptr->port_id);
			return -ENETRESET;
		}

		if (rport_ptr->tx_quota_cnt < IPC_ROUTER_HIGH_RX_QUOTA)
			break;

		if (msm_ipc_router_lookup_resume_tx_port(
			rport_ptr, src->this_port.port_id))
			goto check_timeo;

		resume_tx_port =
			kzalloc(sizeof(struct msm_ipc_resume_tx_port),
				GFP_KERNEL);
		if (!resume_tx_port) {
			IPC_RTR_ERR("%s: Resume_Tx port allocation failed\n",
				    __func__);
			mutex_unlock(&rport_ptr->rport_lock_lhb2);
			return -ENOMEM;
		}
		INIT_LIST_HEAD(&resume_tx_port->list);
		resume_tx_port->port_id = src->this_port.port_id;
		resume_tx_port->node_id = src->this_port.node_id;
		list_add_tail(&resume_tx_port->list,
			      &rport_ptr->resume_tx_port_list);
check_timeo:
		mutex_unlock(&rport_ptr->rport_lock_lhb2);
		if (!timeout) {
			return -EAGAIN;
		} else if (timeout < 0) {
			ret = wait_event_interruptible(src->port_tx_wait_q,
					(rport_ptr->tx_quota_cnt !=
					 IPC_ROUTER_HIGH_RX_QUOTA ||
					 rport_ptr->status == RESET));
			if (ret)
				return ret;
		} else {
			ret = wait_event_interruptible_timeout(
					src->port_tx_wait_q,
					(rport_ptr->tx_quota_cnt !=
					 IPC_ROUTER_HIGH_RX_QUOTA ||
					 rport_ptr->status == RESET),
					msecs_to_jiffies(timeout));
			if (ret < 0) {
				return ret;
			} else if (ret == 0) {
				IPC_RTR_ERR("%s: Resume_tx Timeout %08x:%08x\n",
					__func__, rport_ptr->node_id,
					rport_ptr->port_id);
				return -ETIMEDOUT;
			}
		}
	}
	rport_ptr->tx_quota_cnt++;
	if (rport_ptr->tx_quota_cnt == IPC_ROUTER_LOW_RX_QUOTA)
		*set_confirm_rx = 1;
	mutex_unlock(&rport_ptr->rport_lock_lhb2);
	return 0;
}

static int msm_ipc_router_write_pkt(struct msm_ipc_port *src,
				struct msm_ipc_router_remote_port *rport_ptr,
				struct rr_packet *pkt,
				long timeout)
{
	struct rr_header_v1 *hdr;
	struct msm_ipc_router_xprt_info *xprt_info;
	struct msm_ipc_routing_table_entry *rt_entry;
	struct sk_buff *temp_skb;
	int xprt_option;
	int ret;
	int align_size;
	uint32_t set_confirm_rx = 0;

	if (!rport_ptr || !src || !pkt)
		return -EINVAL;

	hdr = &(pkt->hdr);
	hdr->version = IPC_ROUTER_V1;
	hdr->type = IPC_ROUTER_CTRL_CMD_DATA;
	hdr->src_node_id = src->this_port.node_id;
	hdr->src_port_id = src->this_port.port_id;
	hdr->size = pkt->length;
	hdr->control_flag = 0;
	hdr->dst_node_id = rport_ptr->node_id;
	hdr->dst_port_id = rport_ptr->port_id;

	ret = ipc_router_tx_wait(src, rport_ptr, &set_confirm_rx, timeout);
	if (ret < 0)
		return ret;
	if (set_confirm_rx)
		hdr->control_flag |= CONTROL_FLAG_CONFIRM_RX;

	if (hdr->dst_node_id == IPC_ROUTER_NID_LOCAL) {
		ipc_router_log_msg(local_log_ctx,
		IPC_ROUTER_LOG_EVENT_TX, pkt, hdr, src, rport_ptr);
		ret = loopback_data(src, hdr->dst_port_id, pkt);
		return ret;
	}

	rt_entry = ipc_router_get_rtentry_ref(hdr->dst_node_id);
	if (!rt_entry) {
		IPC_RTR_ERR("%s: Remote node %d not up\n",
			__func__, hdr->dst_node_id);
		return -ENODEV;
	}
	down_read(&rt_entry->lock_lha4);
	xprt_info = rt_entry->xprt_info;
	ret = ipc_router_get_xprt_info_ref(xprt_info);
	if (ret < 0) {
		IPC_RTR_ERR("%s: Abort invalid xprt\n", __func__);
		up_read(&rt_entry->lock_lha4);
		kref_put(&rt_entry->ref, ipc_router_release_rtentry);
		return ret;
	}
	ret = prepend_header(pkt, xprt_info);
	if (ret < 0) {
		IPC_RTR_ERR("%s: Prepend Header failed\n", __func__);
		goto out_write_pkt;
	}
	xprt_option = xprt_info->xprt->get_option(xprt_info->xprt);
	if (!(xprt_option & FRAG_PKT_WRITE_ENABLE)) {
		ret = defragment_pkt(pkt);
		if (ret < 0)
			goto out_write_pkt;
	}

	temp_skb = skb_peek_tail(pkt->pkt_fragment_q);
	align_size = ALIGN_SIZE(pkt->length);
	skb_put(temp_skb, align_size);
	pkt->length += align_size;
	mutex_lock(&xprt_info->tx_lock_lhb2);
	ret = xprt_info->xprt->write(pkt, pkt->length, xprt_info->xprt);
	mutex_unlock(&xprt_info->tx_lock_lhb2);
out_write_pkt:
	up_read(&rt_entry->lock_lha4);
	kref_put(&rt_entry->ref, ipc_router_release_rtentry);

	if (ret < 0) {
		IPC_RTR_ERR("%s: Write on XPRT failed\n", __func__);
		ipc_router_log_msg(xprt_info->log_ctx,
			IPC_ROUTER_LOG_EVENT_TX_ERR, pkt, hdr, src, rport_ptr);

		ipc_router_put_xprt_info_ref(xprt_info);
		return ret;
	}
	update_comm_mode_info(&src->mode_info, xprt_info);
	ipc_router_log_msg(xprt_info->log_ctx,
		IPC_ROUTER_LOG_EVENT_TX, pkt, hdr, src, rport_ptr);
	if (msm_ipc_router_debug_mask & SMEM_LOG) {
		smem_log_event((SMEM_LOG_PROC_ID_APPS |
			SMEM_LOG_IPC_ROUTER_EVENT_BASE |
			IPC_ROUTER_LOG_EVENT_TX),
			(hdr->src_node_id << 24) |
			(hdr->src_port_id & 0xffffff),
			(hdr->dst_node_id << 24) |
			(hdr->dst_port_id & 0xffffff),
			(hdr->type << 24) | (hdr->control_flag << 16) |
			(hdr->size & 0xffff));
	}

	ipc_router_put_xprt_info_ref(xprt_info);
	return hdr->size;
}

int msm_ipc_router_send_to(struct msm_ipc_port *src,
			   struct sk_buff_head *data,
			   struct msm_ipc_addr *dest,
			   long timeout)
{
	uint32_t dst_node_id = 0, dst_port_id = 0;
	struct msm_ipc_server *server;
	struct msm_ipc_server_port *server_port;
	struct msm_ipc_router_remote_port *rport_ptr = NULL;
	struct msm_ipc_router_remote_port *src_rport_ptr = NULL;
	struct rr_packet *pkt;
	int ret;

	if (!src || !data || !dest) {
		IPC_RTR_ERR("%s: Invalid Parameters\n", __func__);
		return -EINVAL;
	}

	/* Resolve Address*/
	if (dest->addrtype == MSM_IPC_ADDR_ID) {
		dst_node_id = dest->addr.port_addr.node_id;
		dst_port_id = dest->addr.port_addr.port_id;
	} else if (dest->addrtype == MSM_IPC_ADDR_NAME) {
		server = ipc_router_get_server_ref(
					dest->addr.port_name.service,
					dest->addr.port_name.instance,
					0, 0);
		if (!server) {
			IPC_RTR_ERR("%s: Destination not reachable\n",
								__func__);
			return -ENODEV;
		}
		server_port = list_first_entry(&server->server_port_list,
					       struct msm_ipc_server_port,
					       list);
		dst_node_id = server_port->server_addr.node_id;
		dst_port_id = server_port->server_addr.port_id;
		kref_put(&server->ref, ipc_router_release_server);
	}

	rport_ptr = ipc_router_get_rport_ref(dst_node_id, dst_port_id);
	if (!rport_ptr) {
		IPC_RTR_ERR("%s: Remote port not found\n", __func__);
		return -ENODEV;
	}

	if (src->check_send_permissions) {
		ret = src->check_send_permissions(rport_ptr->sec_rule);
		if (ret <= 0) {
			kref_put(&rport_ptr->ref, ipc_router_release_rport);
			IPC_RTR_ERR("%s: permission failure for %s\n",
				__func__, current->comm);
			return -EPERM;
		}
	}

	if (dst_node_id == IPC_ROUTER_NID_LOCAL && !src->rport_info) {
		src_rport_ptr = ipc_router_create_rport(IPC_ROUTER_NID_LOCAL,
					src->this_port.port_id, NULL);
		if (!src_rport_ptr) {
			kref_put(&rport_ptr->ref, ipc_router_release_rport);
			IPC_RTR_ERR("%s: RPort creation failed\n", __func__);
			return -ENOMEM;
		}
		mutex_lock(&src->port_lock_lhc3);
		src->rport_info = src_rport_ptr;
		mutex_unlock(&src->port_lock_lhc3);
		kref_put(&src_rport_ptr->ref, ipc_router_release_rport);
	}

	pkt = create_pkt(data);
	if (!pkt) {
		kref_put(&rport_ptr->ref, ipc_router_release_rport);
		IPC_RTR_ERR("%s: Pkt creation failed\n", __func__);
		return -ENOMEM;
	}

	ret = msm_ipc_router_write_pkt(src, rport_ptr, pkt, timeout);
	kref_put(&rport_ptr->ref, ipc_router_release_rport);
	if (ret < 0)
		pkt->pkt_fragment_q = NULL;
	release_pkt(pkt);

	return ret;
}

int msm_ipc_router_send_msg(struct msm_ipc_port *src,
			    struct msm_ipc_addr *dest,
			    void *data, unsigned int data_len)
{
	struct sk_buff_head *out_skb_head;
	int ret;

	out_skb_head = msm_ipc_router_buf_to_skb(data, data_len);
	if (!out_skb_head) {
		IPC_RTR_ERR("%s: SKB conversion failed\n", __func__);
		return -EFAULT;
	}

	ret = msm_ipc_router_send_to(src, out_skb_head, dest, 0);
	if (ret < 0) {
		if (ret != -EAGAIN)
			IPC_RTR_ERR(
			"%s: msm_ipc_router_send_to failed - ret: %d\n",
				__func__, ret);
		msm_ipc_router_free_skb(out_skb_head);
		return ret;
	}
	return 0;
}

/**
 * msm_ipc_router_send_resume_tx() - Send Resume_Tx message
 * @data: Pointer to received data packet that has confirm_rx bit set
 *
 * @return: On success, number of bytes transferred is returned, else
 *	    standard linux error code is returned.
 *
 * This function sends the Resume_Tx event to the remote node that
 * sent the data with confirm_rx field set. In case of a multi-hop
 * scenario also, this function makes sure that the destination node_id
 * to which the resume_tx event should reach is right.
 */
static int msm_ipc_router_send_resume_tx(void *data)
{
	union rr_control_msg msg;
	struct rr_header_v1 *hdr = (struct rr_header_v1 *)data;
	struct msm_ipc_routing_table_entry *rt_entry;
	int ret;

	memset(&msg, 0, sizeof(msg));
	msg.cmd = IPC_ROUTER_CTRL_CMD_RESUME_TX;
	msg.cli.node_id = hdr->dst_node_id;
	msg.cli.port_id = hdr->dst_port_id;
	rt_entry = ipc_router_get_rtentry_ref(hdr->src_node_id);
	if (!rt_entry) {
		IPC_RTR_ERR("%s: %d Node is not present",
				__func__, hdr->src_node_id);
		return -ENODEV;
	}
	ret = ipc_router_get_xprt_info_ref(rt_entry->xprt_info);
	if (ret < 0) {
		IPC_RTR_ERR("%s: Abort invalid xprt\n", __func__);
		kref_put(&rt_entry->ref, ipc_router_release_rtentry);
		return ret;
	}
	ret = ipc_router_send_ctl_msg(rt_entry->xprt_info, &msg,
				      hdr->src_node_id);
	ipc_router_put_xprt_info_ref(rt_entry->xprt_info);
	kref_put(&rt_entry->ref, ipc_router_release_rtentry);
	if (ret < 0)
		IPC_RTR_ERR(
		"%s: Send Resume_Tx Failed SRC_NODE: %d SRC_PORT: %d DEST_NODE: %d",
			__func__, hdr->dst_node_id, hdr->dst_port_id,
			hdr->src_node_id);

	return ret;
}

int msm_ipc_router_read(struct msm_ipc_port *port_ptr,
			struct rr_packet **read_pkt,
			size_t buf_len)
{
	struct rr_packet *pkt;

	if (!port_ptr || !read_pkt)
		return -EINVAL;

	mutex_lock(&port_ptr->port_rx_q_lock_lhc3);
	if (list_empty(&port_ptr->port_rx_q)) {
		mutex_unlock(&port_ptr->port_rx_q_lock_lhc3);
		return -EAGAIN;
	}

	pkt = list_first_entry(&port_ptr->port_rx_q, struct rr_packet, list);
	if ((buf_len) && (pkt->hdr.size > buf_len)) {
		mutex_unlock(&port_ptr->port_rx_q_lock_lhc3);
		return -ETOOSMALL;
	}
	list_del(&pkt->list);
	if (list_empty(&port_ptr->port_rx_q))
		__pm_relax(port_ptr->port_rx_ws);
	*read_pkt = pkt;
	mutex_unlock(&port_ptr->port_rx_q_lock_lhc3);
	if (pkt->hdr.control_flag & CONTROL_FLAG_CONFIRM_RX)
		msm_ipc_router_send_resume_tx(&pkt->hdr);

	return pkt->length;
}

/**
 * msm_ipc_router_rx_data_wait() - Wait for new message destined to a local port.
 * @port_ptr: Pointer to the local port
 * @timeout: < 0 timeout indicates infinite wait till a message arrives.
 *	     > 0 timeout indicates the wait time.
 *	     0 indicates that we do not wait.
 * @return: 0 if there are pending messages to read,
 *	    standard Linux error code otherwise.
 *
 * Checks for the availability of messages that are destined to a local port.
 * If no messages are present then waits as per @timeout.
 */
int msm_ipc_router_rx_data_wait(struct msm_ipc_port *port_ptr, long timeout)
{
	int ret = 0;

	mutex_lock(&port_ptr->port_rx_q_lock_lhc3);
	while (list_empty(&port_ptr->port_rx_q)) {
		mutex_unlock(&port_ptr->port_rx_q_lock_lhc3);
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
			return -ENOMSG;
		mutex_lock(&port_ptr->port_rx_q_lock_lhc3);
	}
	mutex_unlock(&port_ptr->port_rx_q_lock_lhc3);

	return ret;
}

/**
 * msm_ipc_router_recv_from() - Recieve messages destined to a local port.
 * @port_ptr: Pointer to the local port
 * @pkt : Pointer to the router-to-router packet
 * @src: Pointer to local port address
 * @timeout: < 0 timeout indicates infinite wait till a message arrives.
 *	     > 0 timeout indicates the wait time.
 *	     0 indicates that we do not wait.
 * @return: = Number of bytes read(On successful read operation).
 *	    = -ENOMSG (If there are no pending messages and timeout is 0).
 *	    = -EINVAL (If either of the arguments, port_ptr or data is invalid)
 *	    = -EFAULT (If there are no pending messages when timeout is > 0
 *	      and the wait_event_interruptible_timeout has returned value > 0)
 *	    = -ERESTARTSYS (If there are no pending messages when timeout
 *	      is < 0 and wait_event_interruptible was interrupted by a signal)
 *
 * This function reads the messages that are destined for a local port. It
 * is used by modules that exist with-in the kernel and use IPC Router for
 * transport. The function checks if there are any messages that are already
 * received. If yes, it reads them, else it waits as per the timeout value.
 * On a successful read, the return value of the function indicates the number
 * of bytes that are read.
 */
int msm_ipc_router_recv_from(struct msm_ipc_port *port_ptr,
			     struct rr_packet **pkt,
			     struct msm_ipc_addr *src,
			     long timeout)
{
	int ret, data_len, align_size;
	struct sk_buff *temp_skb;
	struct rr_header_v1 *hdr = NULL;

	if (!port_ptr || !pkt) {
		IPC_RTR_ERR("%s: Invalid pointers being passed\n", __func__);
		return -EINVAL;
	}

	*pkt = NULL;

	ret = msm_ipc_router_rx_data_wait(port_ptr, timeout);
	if (ret)
		return ret;

	ret = msm_ipc_router_read(port_ptr, pkt, 0);
	if (ret <= 0 || !(*pkt))
		return ret;

	hdr = &((*pkt)->hdr);
	if (src) {
		src->addrtype = MSM_IPC_ADDR_ID;
		src->addr.port_addr.node_id = hdr->src_node_id;
		src->addr.port_addr.port_id = hdr->src_port_id;
	}

	data_len = hdr->size;
	align_size = ALIGN_SIZE(data_len);
	if (align_size) {
		temp_skb = skb_peek_tail((*pkt)->pkt_fragment_q);
		skb_trim(temp_skb, (temp_skb->len - align_size));
	}
	return data_len;
}

int msm_ipc_router_read_msg(struct msm_ipc_port *port_ptr,
			    struct msm_ipc_addr *src,
			    unsigned char **data,
			    unsigned int *len)
{
	struct rr_packet *pkt;
	int ret;

	ret = msm_ipc_router_recv_from(port_ptr, &pkt, src, 0);
	if (ret < 0) {
		if (ret != -ENOMSG)
			IPC_RTR_ERR(
			"%s: msm_ipc_router_recv_from failed - ret: %d\n",
				__func__, ret);
		return ret;
	}

	*data = msm_ipc_router_skb_to_buf(pkt->pkt_fragment_q, ret);
	if (!(*data)) {
		IPC_RTR_ERR("%s: Buf conversion failed\n", __func__);
		release_pkt(pkt);
		return -ENOMEM;
	}

	*len = ret;
	release_pkt(pkt);
	return 0;
}

/**
 * msm_ipc_router_create_port() - Create a IPC Router port/endpoint
 * @notify: Callback function to notify any event on the port.
 *   @event: Event ID to be handled.
 *   @oob_data: Any out-of-band data associated with the event.
 *   @oob_data_len: Size of the out-of-band data, if valid.
 *   @priv: Private data registered during the port creation.
 * @priv: Private info to be passed while the notification is generated.
 *
 * @return: Pointer to the port on success, NULL on error.
 */
struct msm_ipc_port *msm_ipc_router_create_port(
	void (*notify)(unsigned event, void *oob_data,
		       size_t oob_data_len, void *priv),
	void *priv)
{
	struct msm_ipc_port *port_ptr;
	int ret;

	ret = ipc_router_core_init();
	if (ret < 0) {
		IPC_RTR_ERR("%s: Error %d initializing IPC Router\n",
			    __func__, ret);
		return NULL;
	}

	port_ptr = msm_ipc_router_create_raw_port(NULL, notify, priv);
	if (!port_ptr)
		IPC_RTR_ERR("%s: port_ptr alloc failed\n", __func__);

	return port_ptr;
}

int msm_ipc_router_close_port(struct msm_ipc_port *port_ptr)
{
	union rr_control_msg msg;
	struct msm_ipc_server *server;
	struct msm_ipc_router_remote_port *rport_ptr;

	if (!port_ptr)
		return -EINVAL;

	if (port_ptr->type == SERVER_PORT || port_ptr->type == CLIENT_PORT) {
		down_write(&local_ports_lock_lhc2);
		list_del(&port_ptr->list);
		up_write(&local_ports_lock_lhc2);

		mutex_lock(&port_ptr->port_lock_lhc3);
		rport_ptr = (struct msm_ipc_router_remote_port *)
						port_ptr->rport_info;
		port_ptr->rport_info = NULL;
		mutex_unlock(&port_ptr->port_lock_lhc3);
		if (rport_ptr) {
			ipc_router_reset_conn(rport_ptr);
			ipc_router_destroy_rport(rport_ptr);
		}

		if (port_ptr->type == SERVER_PORT) {
			memset(&msg, 0, sizeof(msg));
			msg.cmd = IPC_ROUTER_CTRL_CMD_REMOVE_SERVER;
			msg.srv.service = port_ptr->port_name.service;
			msg.srv.instance = port_ptr->port_name.instance;
			msg.srv.node_id = port_ptr->this_port.node_id;
			msg.srv.port_id = port_ptr->this_port.port_id;
			broadcast_ctl_msg(&msg);
		}

		/* Server port could have been a client port earlier.
		 * Send REMOVE_CLIENT message in either case.
		 */
		msm_ipc_router_send_remove_client(&port_ptr->mode_info,
			port_ptr->this_port.node_id,
			port_ptr->this_port.port_id);
	} else if (port_ptr->type == CONTROL_PORT) {
		down_write(&control_ports_lock_lha5);
		list_del(&port_ptr->list);
		up_write(&control_ports_lock_lha5);
	} else if (port_ptr->type == IRSC_PORT) {
		down_write(&local_ports_lock_lhc2);
		list_del(&port_ptr->list);
		up_write(&local_ports_lock_lhc2);
		signal_irsc_completion();
	}

	if (port_ptr->type == SERVER_PORT) {
		server = ipc_router_get_server_ref(
				port_ptr->port_name.service,
				port_ptr->port_name.instance,
				port_ptr->this_port.node_id,
				port_ptr->this_port.port_id);
		if (server) {
			kref_put(&server->ref, ipc_router_release_server);
			ipc_router_destroy_server(server,
				port_ptr->this_port.node_id,
				port_ptr->this_port.port_id);
		}
	}

	mutex_lock(&port_ptr->port_lock_lhc3);
	rport_ptr = (struct msm_ipc_router_remote_port *)port_ptr->rport_info;
	port_ptr->rport_info = NULL;
	mutex_unlock(&port_ptr->port_lock_lhc3);
	if (rport_ptr)
		ipc_router_destroy_rport(rport_ptr);

	kref_put(&port_ptr->ref, ipc_router_release_port);
	return 0;
}

int msm_ipc_router_get_curr_pkt_size(struct msm_ipc_port *port_ptr)
{
	struct rr_packet *pkt;
	int rc = 0;

	if (!port_ptr)
		return -EINVAL;

	mutex_lock(&port_ptr->port_rx_q_lock_lhc3);
	if (!list_empty(&port_ptr->port_rx_q)) {
		pkt = list_first_entry(&port_ptr->port_rx_q,
					struct rr_packet, list);
		rc = pkt->hdr.size;
	}
	mutex_unlock(&port_ptr->port_rx_q_lock_lhc3);

	return rc;
}

int msm_ipc_router_bind_control_port(struct msm_ipc_port *port_ptr)
{
	if (unlikely(!port_ptr || port_ptr->type != CLIENT_PORT))
		return -EINVAL;

	down_write(&local_ports_lock_lhc2);
	list_del(&port_ptr->list);
	up_write(&local_ports_lock_lhc2);
	port_ptr->type = CONTROL_PORT;
	down_write(&control_ports_lock_lha5);
	list_add_tail(&port_ptr->list, &control_ports);
	up_write(&control_ports_lock_lha5);

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
		IPC_RTR_ERR("%s: Invalid srv_name\n", __func__);
		return -EINVAL;
	}

	if (num_entries_in_array && !srv_info) {
		IPC_RTR_ERR("%s: srv_info NULL\n", __func__);
		return -EINVAL;
	}

	down_read(&server_list_lock_lha2);
	key = (srv_name->service & (SRV_HASH_SIZE - 1));
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
				srv_info[i].service = server->name.service;
				srv_info[i].instance = server->name.instance;
			}
			i++;
		}
	}
	up_read(&server_list_lock_lha2);

	return i;
}

int msm_ipc_router_close(void)
{
	struct msm_ipc_router_xprt_info *xprt_info, *tmp_xprt_info;

	down_write(&xprt_info_list_lock_lha5);
	list_for_each_entry_safe(xprt_info, tmp_xprt_info,
				 &xprt_info_list, list) {
		xprt_info->xprt->close(xprt_info->xprt);
		list_del(&xprt_info->list);
		kfree(xprt_info);
	}
	up_write(&xprt_info_list_lock_lha5);
	return 0;
}

/**
 * pil_vote_load_worker() - Process vote to load the modem
 *
 * @work: Work item to process
 *
 * This function is called to process votes to load the modem that have been
 * queued by msm_ipc_load_default_node().
 */
static void pil_vote_load_worker(struct work_struct *work)
{
	struct pil_vote_info *vote_info;

	vote_info = container_of(work, struct pil_vote_info, load_work);
	if (strlen(default_peripheral)) {
		vote_info->pil_handle = subsystem_get(default_peripheral);
		if (IS_ERR(vote_info->pil_handle)) {
			IPC_RTR_ERR("%s: Failed to load %s\n",
				    __func__, default_peripheral);
			vote_info->pil_handle = NULL;
		}
	} else {
		vote_info->pil_handle = NULL;
	}
}

/**
 * pil_vote_unload_worker() - Process vote to unload the modem
 *
 * @work: Work item to process
 *
 * This function is called to process votes to unload the modem that have been
 * queued by msm_ipc_unload_default_node().
 */
static void pil_vote_unload_worker(struct work_struct *work)
{
	struct pil_vote_info *vote_info;

	vote_info = container_of(work, struct pil_vote_info, unload_work);

	if (vote_info->pil_handle) {
		subsystem_put(vote_info->pil_handle);
		vote_info->pil_handle = NULL;
	}
	kfree(vote_info);
}

/**
 * msm_ipc_load_default_node() - Queue a vote to load the modem.
 *
 * @return: PIL vote info structure on success, NULL on failure.
 *
 * This function places a work item that loads the modem on the
 * single-threaded workqueue used for processing PIL votes to load
 * or unload the modem.
 */
void *msm_ipc_load_default_node(void)
{
	struct pil_vote_info *vote_info;

	vote_info = kmalloc(sizeof(*vote_info), GFP_KERNEL);
	if (!vote_info)
		return vote_info;

	INIT_WORK(&vote_info->load_work, pil_vote_load_worker);
	queue_work(msm_ipc_router_workqueue, &vote_info->load_work);

	return vote_info;
}

/**
 * msm_ipc_unload_default_node() - Queue a vote to unload the modem.
 *
 * @pil_vote: PIL vote info structure, containing the PIL handle
 * and work structure.
 *
 * This function places a work item that unloads the modem on the
 * single-threaded workqueue used for processing PIL votes to load
 * or unload the modem.
 */
void msm_ipc_unload_default_node(void *pil_vote)
{
	struct pil_vote_info *vote_info;

	if (pil_vote) {
		vote_info = (struct pil_vote_info *)pil_vote;
		INIT_WORK(&vote_info->unload_work, pil_vote_unload_worker);
		queue_work(msm_ipc_router_workqueue, &vote_info->unload_work);
	}
}

#if defined(CONFIG_DEBUG_FS)
static void dump_routing_table(struct seq_file *s)
{
	int j;
	struct msm_ipc_routing_table_entry *rt_entry;

	seq_printf(s, "%-10s|%-20s|%-10s|\n",
			"Node Id", "XPRT Name", "Next Hop");
	seq_puts(s, "----------------------------------------------\n");
	for (j = 0; j < RT_HASH_SIZE; j++) {
		down_read(&routing_table_lock_lha3);
		list_for_each_entry(rt_entry, &routing_table[j], list) {
			down_read(&rt_entry->lock_lha4);
			seq_printf(s, "0x%08x|", rt_entry->node_id);
			if (rt_entry->node_id == IPC_ROUTER_NID_LOCAL)
				seq_printf(s, "%-20s|0x%08x|\n",
				       "Loopback", rt_entry->node_id);
			else
				seq_printf(s, "%-20s|0x%08x|\n",
				       rt_entry->xprt_info->xprt->name,
				       rt_entry->node_id);
			up_read(&rt_entry->lock_lha4);
		}
		up_read(&routing_table_lock_lha3);
	}
}

static void dump_xprt_info(struct seq_file *s)
{
	struct msm_ipc_router_xprt_info *xprt_info;

	seq_printf(s, "%-20s|%-10s|%-12s|%-15s|\n",
			"XPRT Name", "Link ID",
			"Initialized", "Remote Node Id");
	seq_puts(s, "------------------------------------------------------------\n");
	down_read(&xprt_info_list_lock_lha5);
	list_for_each_entry(xprt_info, &xprt_info_list, list)
		seq_printf(s, "%-20s|0x%08x|%-12s|0x%08x|\n",
			       xprt_info->xprt->name,
			       xprt_info->xprt->link_id,
			       (xprt_info->initialized ? "Y" : "N"),
			       xprt_info->remote_node_id);
	up_read(&xprt_info_list_lock_lha5);
}

static void dump_servers(struct seq_file *s)
{
	int j;
	struct msm_ipc_server *server;
	struct msm_ipc_server_port *server_port;

	seq_printf(s, "%-11s|%-11s|%-11s|%-11s|\n",
			"Service", "Instance", "Node_id", "Port_id");
	seq_puts(s, "------------------------------------------------------------\n");
	down_read(&server_list_lock_lha2);
	for (j = 0; j < SRV_HASH_SIZE; j++) {
		list_for_each_entry(server, &server_list[j], list) {
			list_for_each_entry(server_port,
					    &server->server_port_list,
					    list)
				seq_printf(s, "0x%08x |0x%08x |0x%08x |0x%08x |\n",
					server->name.service,
					server->name.instance,
					server_port->server_addr.node_id,
					server_port->server_addr.port_id);
		}
	}
	up_read(&server_list_lock_lha2);
}

static void dump_remote_ports(struct seq_file *s)
{
	int j, k;
	struct msm_ipc_router_remote_port *rport_ptr;
	struct msm_ipc_routing_table_entry *rt_entry;

	seq_printf(s, "%-11s|%-11s|%-10s|\n",
			"Node_id", "Port_id", "Quota_cnt");
	seq_puts(s, "------------------------------------------------------------\n");
	for (j = 0; j < RT_HASH_SIZE; j++) {
		down_read(&routing_table_lock_lha3);
		list_for_each_entry(rt_entry, &routing_table[j], list) {
			down_read(&rt_entry->lock_lha4);
			for (k = 0; k < RP_HASH_SIZE; k++) {
				list_for_each_entry(rport_ptr,
					&rt_entry->remote_port_list[k],
					list)
					seq_printf(s, "0x%08x |0x%08x |0x%08x|\n",
						rport_ptr->node_id,
						rport_ptr->port_id,
						rport_ptr->tx_quota_cnt);
			}
			up_read(&rt_entry->lock_lha4);
		}
		up_read(&routing_table_lock_lha3);
	}
}

static void dump_control_ports(struct seq_file *s)
{
	struct msm_ipc_port *port_ptr;

	seq_printf(s, "%-11s|%-11s|\n",
			"Node_id", "Port_id");
	seq_puts(s, "------------------------------------------------------------\n");
	down_read(&control_ports_lock_lha5);
	list_for_each_entry(port_ptr, &control_ports, list)
		seq_printf(s, "0x%08x |0x%08x |\n",
			 port_ptr->this_port.node_id,
			 port_ptr->this_port.port_id);
	up_read(&control_ports_lock_lha5);
}

static void dump_local_ports(struct seq_file *s)
{
	int j;
	struct msm_ipc_port *port_ptr;

	seq_printf(s, "%-11s|%-11s|\n",
			"Node_id", "Port_id");
	seq_puts(s, "------------------------------------------------------------\n");
	down_read(&local_ports_lock_lhc2);
	for (j = 0; j < LP_HASH_SIZE; j++) {
		list_for_each_entry(port_ptr, &local_ports[j], list) {
			mutex_lock(&port_ptr->port_lock_lhc3);
			seq_printf(s, "0x%08x |0x%08x |\n",
				       port_ptr->this_port.node_id,
				       port_ptr->this_port.port_id);
			mutex_unlock(&port_ptr->port_lock_lhc3);
		}
	}
	up_read(&local_ports_lock_lhc2);
}

static int debugfs_show(struct seq_file *s, void *data)
{
	void (*show)(struct seq_file *) = s->private;
	show(s);
	return 0;
}

static int debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, debugfs_show, inode->i_private);
}

static const struct file_operations debug_ops = {
	.open = debug_open,
	.release = single_release,
	.read = seq_read,
	.llseek = seq_lseek,
};

static void debug_create(const char *name, struct dentry *dent,
			 void (*show)(struct seq_file *))
{
	debugfs_create_file(name, 0444, dent, show, &debug_ops);
}

static void debugfs_init(void)
{
	struct dentry *dent;

	dent = debugfs_create_dir("msm_ipc_router", 0);
	if (IS_ERR(dent))
		return;

	debug_create("dump_local_ports", dent, dump_local_ports);
	debug_create("dump_remote_ports", dent, dump_remote_ports);
	debug_create("dump_control_ports", dent, dump_control_ports);
	debug_create("dump_servers", dent, dump_servers);
	debug_create("dump_xprt_info", dent, dump_xprt_info);
	debug_create("dump_routing_table", dent, dump_routing_table);
}

#else
static void debugfs_init(void) {}
#endif

/**
 * ipc_router_create_log_ctx() - Create and add the log context based on transport
 * @name:	subsystem name
 *
 * Return:	a reference to the log context created
 *
 * This function creates ipc log context based on transport and adds it to a
 * global list. This log context can be reused from the list in case of a
 * subsystem restart.
 */
static void *ipc_router_create_log_ctx(char *name)
{
	struct ipc_rtr_log_ctx *sub_log_ctx;

	sub_log_ctx = kmalloc(sizeof(struct ipc_rtr_log_ctx),
				GFP_KERNEL);
	if (!sub_log_ctx)
		return NULL;
	sub_log_ctx->log_ctx = ipc_log_context_create(
				IPC_RTR_INFO_PAGES, name, 0);
	if (!sub_log_ctx->log_ctx) {
		IPC_RTR_ERR("%s: Unable to create IPC logging for [%s]",
			__func__, name);
		kfree(sub_log_ctx);
		return NULL;
	}
	strlcpy(sub_log_ctx->log_ctx_name, name,
			LOG_CTX_NAME_LEN);
	INIT_LIST_HEAD(&sub_log_ctx->list);
	list_add_tail(&sub_log_ctx->list, &log_ctx_list);
	return sub_log_ctx->log_ctx;
}

static void ipc_router_log_ctx_init(void)
{
	mutex_lock(&log_ctx_list_lock_lha0);
	local_log_ctx = ipc_router_create_log_ctx("local_IPCRTR");
	mutex_unlock(&log_ctx_list_lock_lha0);
}

/**
 * ipc_router_get_log_ctx() - Retrieves the ipc log context based on subsystem name.
 * @sub_name:	subsystem name
 *
 * Return:	a reference to the log context
 */
static void *ipc_router_get_log_ctx(char *sub_name)
{
	void *log_ctx = NULL;
	struct ipc_rtr_log_ctx *temp_log_ctx;

	mutex_lock(&log_ctx_list_lock_lha0);
	list_for_each_entry(temp_log_ctx, &log_ctx_list, list)
		if (!strcmp(temp_log_ctx->log_ctx_name, sub_name)) {
			log_ctx = temp_log_ctx->log_ctx;
			mutex_unlock(&log_ctx_list_lock_lha0);
			return log_ctx;
		}
	log_ctx = ipc_router_create_log_ctx(sub_name);
	mutex_unlock(&log_ctx_list_lock_lha0);

	return log_ctx;
}

/**
 * ipc_router_get_xprt_info_ref() - Get a reference to the xprt_info structure
 * @xprt_info: pointer to the xprt_info.
 *
 * @return: Zero on success, -ENODEV on failure.
 *
 * This function is used to obtain a reference to the xprt_info structure
 * corresponding to the requested @xprt_info pointer.
 */
static int ipc_router_get_xprt_info_ref(
		struct msm_ipc_router_xprt_info *xprt_info)
{
	int ret = -ENODEV;
	struct msm_ipc_router_xprt_info *tmp_xprt_info;

	if (!xprt_info)
		return 0;

	down_read(&xprt_info_list_lock_lha5);
	list_for_each_entry(tmp_xprt_info, &xprt_info_list, list) {
		if (tmp_xprt_info == xprt_info) {
			kref_get(&xprt_info->ref);
			ret = 0;
			break;
		}
	}
	up_read(&xprt_info_list_lock_lha5);

	return ret;
}

/**
 * ipc_router_put_xprt_info_ref() - Put a reference to the xprt_info structure
 * @xprt_info: pointer to the xprt_info.
 *
 * This function is used to put the reference to the xprt_info structure
 * corresponding to the requested @xprt_info pointer.
 */
static void ipc_router_put_xprt_info_ref(
		struct msm_ipc_router_xprt_info *xprt_info)
{
	if (xprt_info)
		kref_put(&xprt_info->ref, ipc_router_release_xprt_info_ref);
}

/**
 * ipc_router_release_xprt_info_ref() - release the xprt_info last reference
 * @ref: Reference to the xprt_info structure.
 *
 * This function is called when all references to the xprt_info structure
 * are released.
 */
static void ipc_router_release_xprt_info_ref(struct kref *ref)
{
	struct msm_ipc_router_xprt_info *xprt_info =
		container_of(ref, struct msm_ipc_router_xprt_info, ref);

	complete_all(&xprt_info->ref_complete);
}

static int msm_ipc_router_add_xprt(struct msm_ipc_router_xprt *xprt)
{
	struct msm_ipc_router_xprt_info *xprt_info;

	xprt_info = kmalloc(sizeof(struct msm_ipc_router_xprt_info),
			    GFP_KERNEL);
	if (!xprt_info)
		return -ENOMEM;

	xprt_info->xprt = xprt;
	xprt_info->initialized = 0;
	xprt_info->remote_node_id = -1;
	INIT_LIST_HEAD(&xprt_info->pkt_list);
	mutex_init(&xprt_info->rx_lock_lhb2);
	mutex_init(&xprt_info->tx_lock_lhb2);
	wakeup_source_init(&xprt_info->ws, xprt->name);
	xprt_info->need_len = 0;
	xprt_info->abort_data_read = 0;
	INIT_WORK(&xprt_info->read_data, do_read_data);
	INIT_LIST_HEAD(&xprt_info->list);
	kref_init(&xprt_info->ref);
	init_completion(&xprt_info->ref_complete);

	xprt_info->workqueue = create_singlethread_workqueue(xprt->name);
	if (!xprt_info->workqueue) {
		kfree(xprt_info);
		return -ENOMEM;
	}

	xprt_info->log_ctx = ipc_router_get_log_ctx(xprt->name);

	if (!strcmp(xprt->name, "msm_ipc_router_loopback_xprt")) {
		xprt_info->remote_node_id = IPC_ROUTER_NID_LOCAL;
		xprt_info->initialized = 1;
	}

	IPC_RTR_INFO(xprt_info->log_ctx, "Adding xprt: [%s]\n",
						xprt->name);
	down_write(&xprt_info_list_lock_lha5);
	list_add_tail(&xprt_info->list, &xprt_info_list);
	up_write(&xprt_info_list_lock_lha5);

	down_write(&routing_table_lock_lha3);
	if (!routing_table_inited) {
		init_routing_table();
		routing_table_inited = 1;
	}
	up_write(&routing_table_lock_lha3);

	xprt->priv = xprt_info;

	return 0;
}

static void msm_ipc_router_remove_xprt(struct msm_ipc_router_xprt *xprt)
{
	struct msm_ipc_router_xprt_info *xprt_info;
	struct rr_packet *temp_pkt, *pkt;

	if (xprt && xprt->priv) {
		xprt_info = xprt->priv;

		IPC_RTR_INFO(xprt_info->log_ctx, "Removing xprt: [%s]\n",
						xprt->name);
		mutex_lock(&xprt_info->rx_lock_lhb2);
		xprt_info->abort_data_read = 1;
		mutex_unlock(&xprt_info->rx_lock_lhb2);
		flush_workqueue(xprt_info->workqueue);
		destroy_workqueue(xprt_info->workqueue);
		mutex_lock(&xprt_info->rx_lock_lhb2);
		list_for_each_entry_safe(pkt, temp_pkt,
					 &xprt_info->pkt_list, list) {
			list_del(&pkt->list);
			release_pkt(pkt);
		}
		mutex_unlock(&xprt_info->rx_lock_lhb2);

		down_write(&xprt_info_list_lock_lha5);
		list_del(&xprt_info->list);
		up_write(&xprt_info_list_lock_lha5);

		msm_ipc_cleanup_routing_table(xprt_info);

		wakeup_source_trash(&xprt_info->ws);

		ipc_router_put_xprt_info_ref(xprt_info);
		wait_for_completion(&xprt_info->ref_complete);

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

	msm_ipc_router_remove_xprt(xprt_work->xprt);
	xprt_work->xprt->sft_close_done(xprt_work->xprt);
	kfree(xprt_work);
}

void msm_ipc_router_xprt_notify(struct msm_ipc_router_xprt *xprt,
				unsigned event,
				void *data)
{
	struct msm_ipc_router_xprt_info *xprt_info = xprt->priv;
	struct msm_ipc_router_xprt_work *xprt_work;
	struct rr_packet *pkt;
	int ret;

	ret = ipc_router_core_init();
	if (ret < 0) {
		IPC_RTR_ERR("%s: Error %d initializing IPC Router\n",
			    __func__, ret);
		return;
	}

	switch (event) {
	case IPC_ROUTER_XPRT_EVENT_OPEN:
		xprt_work = kmalloc(sizeof(struct msm_ipc_router_xprt_work),
				GFP_ATOMIC);
		if (xprt_work) {
			xprt_work->xprt = xprt;
			INIT_WORK(&xprt_work->work, xprt_open_worker);
			queue_work(msm_ipc_router_workqueue, &xprt_work->work);
		} else {
			IPC_RTR_ERR(
			"%s: malloc failure - Couldn't notify OPEN event",
				__func__);
		}
		break;

	case IPC_ROUTER_XPRT_EVENT_CLOSE:
		xprt_work = kmalloc(sizeof(struct msm_ipc_router_xprt_work),
				GFP_ATOMIC);
		if (xprt_work) {
			xprt_work->xprt = xprt;
			INIT_WORK(&xprt_work->work, xprt_close_worker);
			queue_work(msm_ipc_router_workqueue, &xprt_work->work);
		} else {
			IPC_RTR_ERR(
			"%s: malloc failure - Couldn't notify CLOSE event",
				__func__);
		}
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

	mutex_lock(&xprt_info->rx_lock_lhb2);
	list_add_tail(&pkt->list, &xprt_info->pkt_list);
	__pm_stay_awake(&xprt_info->ws);
	mutex_unlock(&xprt_info->rx_lock_lhb2);
	queue_work(xprt_info->workqueue, &xprt_info->read_data);
}

/**
 * parse_devicetree() - parse device tree binding
 *
 * @node: pointer to device tree node
 *
 * @return: 0 on success, -ENODEV on failure.
 */
static int parse_devicetree(struct device_node *node)
{
	char *key;
	const char *peripheral = NULL;

	key = "qcom,default-peripheral";
	peripheral = of_get_property(node, key, NULL);
	if (peripheral)
		strlcpy(default_peripheral, peripheral, PIL_SUBSYSTEM_NAME_LEN);

	return 0;
}

/**
 * ipc_router_probe() - Probe the IPC Router
 *
 * @pdev: Platform device corresponding to IPC Router.
 *
 * @return: 0 on success, standard Linux error codes on error.
 *
 * This function is called when the underlying device tree driver registers
 * a platform device, mapped to IPC Router.
 */
static int ipc_router_probe(struct platform_device *pdev)
{
	int ret = 0;

	if (pdev && pdev->dev.of_node) {
		ret = parse_devicetree(pdev->dev.of_node);
		if (ret)
			IPC_RTR_ERR("%s: Failed to parse device tree\n",
				    __func__);
	}
	return ret;
}

static struct of_device_id ipc_router_match_table[] = {
	{ .compatible = "qcom,ipc_router" },
	{},
};

static struct platform_driver ipc_router_driver = {
	.probe = ipc_router_probe,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = ipc_router_match_table,
	 },
};

/**
 * ipc_router_core_init() - Initialize all IPC Router core data structures
 *
 * Return: 0 on Success or Standard error code otherwise.
 *
 * This function only initializes all the core data structures to the IPC Router
 * module. The remaining initialization is done inside msm_ipc_router_init().
 */
static int ipc_router_core_init(void)
{
	int i;
	int ret;
	struct msm_ipc_routing_table_entry *rt_entry;

	mutex_lock(&ipc_router_init_lock);
	if (likely(is_ipc_router_inited)) {
		mutex_unlock(&ipc_router_init_lock);
		return 0;
	}

	debugfs_init();

	for (i = 0; i < SRV_HASH_SIZE; i++)
		INIT_LIST_HEAD(&server_list[i]);

	for (i = 0; i < LP_HASH_SIZE; i++)
		INIT_LIST_HEAD(&local_ports[i]);

	down_write(&routing_table_lock_lha3);
	if (!routing_table_inited) {
		init_routing_table();
		routing_table_inited = 1;
	}
	up_write(&routing_table_lock_lha3);
	rt_entry = create_routing_table_entry(IPC_ROUTER_NID_LOCAL, NULL);
	kref_put(&rt_entry->ref, ipc_router_release_rtentry);

	msm_ipc_router_workqueue =
		create_singlethread_workqueue("msm_ipc_router");
	if (!msm_ipc_router_workqueue) {
		mutex_unlock(&ipc_router_init_lock);
		return -ENOMEM;
	}

	ret = msm_ipc_router_security_init();
	if (ret < 0)
		IPC_RTR_ERR("%s: Security Init failed\n", __func__);
	else
		is_ipc_router_inited = true;
	mutex_unlock(&ipc_router_init_lock);

	return ret;
}

static int msm_ipc_router_init(void)
{
	int ret;

	ret = ipc_router_core_init();
	if (ret < 0)
		return ret;

	ret = platform_driver_register(&ipc_router_driver);
	if (ret)
		IPC_RTR_ERR(
		"%s: ipc_router_driver register failed %d\n", __func__, ret);

	ret = msm_ipc_router_init_sockets();
	if (ret < 0)
		IPC_RTR_ERR("%s: Init sockets failed\n", __func__);

	ipc_router_log_ctx_init();
	return ret;
}

module_init(msm_ipc_router_init);
MODULE_DESCRIPTION("MSM IPC Router");
MODULE_LICENSE("GPL v2");
