/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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
#include <linux/rwsem.h>

#include <asm/uaccess.h>
#include <asm/byteorder.h>

#include <mach/smem_log.h>
#include <mach/subsystem_notif.h>
#include <mach/msm_ipc_router.h>
#include <mach/msm_ipc_logging.h>

#include "ipc_router.h"
#include "modem_notifier.h"
#include "msm_ipc_router_security.h"

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

static void *ipc_rtr_log_ctxt;
#define IPC_RTR_LOG_PAGES 5
#define DIAG(x...) pr_info("[RR] ERROR " x)

#if defined(DEBUG)
#define D(x...) do { \
if (ipc_rtr_log_ctxt) \
	ipc_log_string(ipc_rtr_log_ctxt, x); \
if (msm_ipc_router_debug_mask & RTR_DBG) \
	pr_info(x); \
} while (0)

#define RR(x...) do { \
if (ipc_rtr_log_ctxt) \
	ipc_log_string(ipc_rtr_log_ctxt, x); \
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

#define IPC_ROUTER_LOG_EVENT_ERROR      0x00
#define IPC_ROUTER_LOG_EVENT_TX         0x01
#define IPC_ROUTER_LOG_EVENT_RX         0x02
#define IPC_ROUTER_DUMMY_DEST_NODE	0xFFFFFFFF

static LIST_HEAD(control_ports);
static DECLARE_RWSEM(control_ports_lock_lha5);

#define LP_HASH_SIZE 32
static struct list_head local_ports[LP_HASH_SIZE];
static DECLARE_RWSEM(local_ports_lock_lha2);

/*
 * Server info is organized as a hash table. The server's service ID is
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
	struct msm_ipc_port_name name;
	char pdev_name[32];
	int next_pdev_id;
	int synced_sec_rule;
	struct list_head server_port_list;
};

struct msm_ipc_server_port {
	struct list_head list;
	struct platform_device pdev;
	struct msm_ipc_port_addr server_addr;
	struct msm_ipc_router_xprt_info *xprt_info;
};

struct msm_ipc_resume_tx_port {
	struct list_head list;
	uint32_t port_id;
	uint32_t node_id;
};

#define RP_HASH_SIZE 32
struct msm_ipc_router_remote_port {
	struct list_head list;
	uint32_t node_id;
	uint32_t port_id;
	uint32_t tx_quota_cnt;
	struct mutex quota_lock_lhb2;
	struct list_head resume_tx_port_list;
	void *sec_rule;
	struct msm_ipc_server *server;
};

struct msm_ipc_router_xprt_info {
	struct list_head list;
	struct msm_ipc_router_xprt *xprt;
	uint32_t remote_node_id;
	uint32_t initialized;
	struct list_head pkt_list;
	struct wake_lock wakelock;
	struct mutex rx_lock_lhb2;
	struct mutex tx_lock_lhb2;
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
	struct rw_semaphore lock_lha4;
	unsigned long num_tx_bytes;
	unsigned long num_rx_bytes;
};

static struct list_head routing_table[RT_HASH_SIZE];
static DECLARE_RWSEM(routing_table_lock_lha3);
static int routing_table_inited;

static void do_read_data(struct work_struct *work);

static LIST_HEAD(xprt_info_list);
static DECLARE_RWSEM(xprt_info_list_lock_lha5);

static DECLARE_COMPLETION(msm_ipc_local_router_up);
#define IPC_ROUTER_INIT_TIMEOUT (10 * HZ)

static uint32_t next_port_id;
static DEFINE_MUTEX(next_port_id_lock_lha1);
static struct workqueue_struct *msm_ipc_router_workqueue;

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

	init_rwsem(&rt_entry->lock_lha4);
	rt_entry->node_id = node_id;
	rt_entry->xprt_info = NULL;
	return rt_entry;
}

/* Must be called with routing_table_lock_lha3 locked. */
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

struct rr_packet *rr_read(struct msm_ipc_router_xprt_info *xprt_info)
{
	struct rr_packet *temp_pkt;

	if (!xprt_info)
		return NULL;

	mutex_lock(&xprt_info->rx_lock_lhb2);
	if (xprt_info->abort_data_read) {
		mutex_unlock(&xprt_info->rx_lock_lhb2);
		pr_err("%s detected SSR & exiting now\n",
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
		wake_unlock(&xprt_info->wakelock);
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
		pr_err("%s: failure\n", __func__);
		return NULL;
	}
	memcpy(&(cloned_pkt->hdr), &(pkt->hdr), sizeof(struct rr_header_v1));
	/* TODO: Copy optional headers, if available */

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
	/* TODO: Free optional headers, if present */
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
	/* TODO: Free Optional headers, if present */
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
		pr_err("%s: Couldnot allocate skb_head\n", __func__);
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
				pr_err("%s: cannot allocate skb\n", __func__);
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
		pr_err("%s: NULL skb_head\n", __func__);
		return NULL;
	}

	temp = skb_peek(skb_head);
	buf_len = len;
	buf = kmalloc(buf_len, GFP_KERNEL);
	if (!buf) {
		pr_err("%s: cannot allocate buf\n", __func__);
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
 * extract_header_v1() - Extract IPC Router header of version 1
 * @pkt: Packet structure into which the header has to be extraced.
 * @skb: SKB from which the header has to be extracted.
 *
 * @return: 0 on success, standard Linux error codes on failure.
 */
static int extract_header_v1(struct rr_packet *pkt, struct sk_buff *skb)
{
	if (!pkt || !skb) {
		pr_err("%s: Invalid pkt or skb\n", __func__);
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

	if (!pkt || !skb) {
		pr_err("%s: Invalid pkt or skb\n", __func__);
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
	skb_pull(skb, sizeof(struct rr_header_v2));
	pkt->length -= sizeof(struct rr_header_v2);
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
		pr_err("%s: NULL PKT\n", __func__);
		return -EINVAL;
	}

	temp_skb = skb_peek(pkt->pkt_fragment_q);
	if (!temp_skb || !temp_skb->data) {
		pr_err("%s: No SKBs in skb_queue\n", __func__);
		return -EINVAL;
	}

	if (temp_skb->data[0] == IPC_ROUTER_V1) {
		ret = extract_header_v1(pkt, temp_skb);
	} else if (temp_skb->data[0] == IPC_ROUTER_V2) {
		ret = extract_header_v2(pkt, temp_skb);
		/* TODO: Extract optional headers if present */
	} else {
		pr_err("%s: Invalid Header version %02x\n",
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
	struct msm_ipc_routing_table_entry *rt_entry;
	struct msm_ipc_router_xprt_info *xprt_info = dst_xprt_info;

	if (!pkt) {
		pr_err("%s: NULL PKT\n", __func__);
		return -EINVAL;
	}

	if (!xprt_info) {
		rt_entry = lookup_routing_table(pkt->hdr.dst_node_id);
		if (!rt_entry || !(rt_entry->xprt_info)) {
			pr_err("%s: Node %d is not up\n",
				__func__, pkt->hdr.dst_node_id);
			return -ENODEV;
		}

		xprt_info = rt_entry->xprt_info;
	}
	if (xprt_info)
		xprt_version = xprt_info->xprt->get_version(xprt_info->xprt);

	if (xprt_version == IPC_ROUTER_V1) {
		pkt->hdr.version = IPC_ROUTER_V1;
		hdr_size = sizeof(struct rr_header_v1);
	} else if (xprt_version == IPC_ROUTER_V2) {
		pkt->hdr.version = IPC_ROUTER_V2;
		hdr_size = sizeof(struct rr_header_v2);
		/* TODO: Calculate optional header length, if present */
	} else {
		pr_err("%s: Invalid xprt_version %d\n",
			__func__, xprt_version);
		hdr_size = -EINVAL;
	}

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
		pr_err("%s: Invalid input parameters\n", __func__);
		return -EINVAL;
	}

	temp_skb = skb_peek(pkt->pkt_fragment_q);
	if (!temp_skb || !temp_skb->data) {
		pr_err("%s: No SKBs in skb_queue\n", __func__);
		return -EINVAL;
	}

	if (skb_headroom(temp_skb) < hdr_size) {
		temp_skb = alloc_skb(hdr_size, GFP_KERNEL);
		if (!temp_skb) {
			pr_err("%s: Could not allocate SKB of size %d\n",
				__func__, hdr_size);
			return -ENOMEM;
		}
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
		pr_err("%s: Invalid input parameters\n", __func__);
		return -EINVAL;
	}

	temp_skb = skb_peek(pkt->pkt_fragment_q);
	if (!temp_skb || !temp_skb->data) {
		pr_err("%s: No SKBs in skb_queue\n", __func__);
		return -EINVAL;
	}

	if (skb_headroom(temp_skb) < hdr_size) {
		temp_skb = alloc_skb(hdr_size, GFP_KERNEL);
		if (!temp_skb) {
			pr_err("%s: Could not allocate SKB of size %d\n",
				__func__, hdr_size);
			return -ENOMEM;
		}
	}

	hdr = (struct rr_header_v2 *)skb_push(temp_skb, hdr_size);
	hdr->version = (uint8_t)pkt->hdr.version;
	hdr->type = (uint8_t)pkt->hdr.type;
	hdr->control_flag = (uint16_t)pkt->hdr.control_flag;
	hdr->size = (uint32_t)pkt->hdr.size;
	hdr->src_node_id = (uint16_t)pkt->hdr.src_node_id;
	hdr->src_port_id = (uint16_t)pkt->hdr.src_port_id;
	hdr->dst_node_id = (uint16_t)pkt->hdr.dst_node_id;
	hdr->dst_port_id = (uint16_t)pkt->hdr.dst_port_id;
	/* TODO: Add optional headers, if present */
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
		pr_err("%s: NULL PKT\n", __func__);
		return -EINVAL;
	}

	temp_skb = skb_peek(pkt->pkt_fragment_q);
	if (!temp_skb || !temp_skb->data) {
		pr_err("%s: No SKBs in skb_queue\n", __func__);
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
		pr_err("%s: Invalid PKT\n", __func__);
		return -EINVAL;
	}

	if (skb_queue_len(pkt->pkt_fragment_q) == 1)
		return 0;

	align_size = ALIGN_SIZE(pkt->length);
	dst_skb = alloc_skb(pkt->length + align_size, GFP_KERNEL);
	if (!dst_skb) {
		pr_err("%s: could not allocate one skb of size %d\n",
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
	void (*notify)(unsigned event, void *priv);

	if (unlikely(!port_ptr || !pkt))
		return -EINVAL;

	if (clone) {
		temp_pkt = clone_pkt(pkt);
		if (!temp_pkt) {
			pr_err("%s: Error cloning packet for port %08x:%08x\n",
				__func__, port_ptr->this_port.node_id,
				port_ptr->this_port.port_id);
			return -ENOMEM;
		}
	}

	mutex_lock(&port_ptr->port_rx_q_lock_lhb3);
	wake_lock(&port_ptr->port_rx_wake_lock);
	list_add_tail(&temp_pkt->list, &port_ptr->port_rx_q);
	wake_up(&port_ptr->port_rx_wait_q);
	notify = port_ptr->notify;
	mutex_unlock(&port_ptr->port_rx_q_lock_lhb3);
	if (notify)
		notify(MSM_IPC_ROUTER_READ_CB, port_ptr->priv);
	return 0;
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

	mutex_lock(&next_port_id_lock_lha1);
	prev_port_id = next_port_id;
	down_read(&local_ports_lock_lha2);
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
	up_read(&local_ports_lock_lha2);
	mutex_unlock(&next_port_id_lock_lha1);

	return port_id;
}

void msm_ipc_router_add_local_port(struct msm_ipc_port *port_ptr)
{
	uint32_t key;

	if (!port_ptr)
		return;

	key = (port_ptr->this_port.port_id & (LP_HASH_SIZE - 1));
	down_write(&local_ports_lock_lha2);
	list_add_tail(&port_ptr->list, &local_ports[key]);
	up_write(&local_ports_lock_lha2);
}

struct msm_ipc_port *msm_ipc_router_create_raw_port(void *endpoint,
		void (*notify)(unsigned event, void *priv),
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
	INIT_LIST_HEAD(&port_ptr->port_rx_q);
	mutex_init(&port_ptr->port_rx_q_lock_lhb3);
	init_waitqueue_head(&port_ptr->port_rx_wait_q);
	snprintf(port_ptr->rx_wakelock_name, MAX_WAKELOCK_NAME_SZ,
		 "ipc%08x_%s",
		 port_ptr->this_port.port_id,
		 current->comm);
	wake_lock_init(&port_ptr->port_rx_wake_lock,
			WAKE_LOCK_SUSPEND, port_ptr->rx_wakelock_name);

	port_ptr->endpoint = endpoint;
	port_ptr->notify = notify;
	port_ptr->priv = priv;

	msm_ipc_router_add_local_port(port_ptr);
	return port_ptr;
}

/* Must be called with local_ports_lock_lha2 locked. */
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

/* Must be called with routing_table_lock_lha3 locked. */
static struct msm_ipc_router_remote_port *msm_ipc_router_lookup_remote_port(
						uint32_t node_id,
						uint32_t port_id)
{
	struct msm_ipc_router_remote_port *rport_ptr;
	struct msm_ipc_routing_table_entry *rt_entry;
	int key = (port_id & (RP_HASH_SIZE - 1));

	rt_entry = lookup_routing_table(node_id);
	if (!rt_entry) {
		pr_err("%s: Node is not up\n", __func__);
		return NULL;
	}

	down_read(&rt_entry->lock_lha4);
	list_for_each_entry(rport_ptr,
			    &rt_entry->remote_port_list[key], list) {
		if (rport_ptr->port_id == port_id) {
			up_read(&rt_entry->lock_lha4);
			return rport_ptr;
		}
	}
	up_read(&rt_entry->lock_lha4);
	return NULL;
}

/* Must be called with routing_table_lock_lha3 locked. */
static struct msm_ipc_router_remote_port *msm_ipc_router_create_remote_port(
						uint32_t node_id,
						uint32_t port_id)
{
	struct msm_ipc_router_remote_port *rport_ptr;
	struct msm_ipc_routing_table_entry *rt_entry;
	int key = (port_id & (RP_HASH_SIZE - 1));

	rt_entry = lookup_routing_table(node_id);
	if (!rt_entry) {
		pr_err("%s: Node is not up\n", __func__);
		return NULL;
	}

	rport_ptr = kmalloc(sizeof(struct msm_ipc_router_remote_port),
			    GFP_KERNEL);
	if (!rport_ptr) {
		pr_err("%s: Remote port alloc failed\n", __func__);
		return NULL;
	}
	rport_ptr->port_id = port_id;
	rport_ptr->node_id = node_id;
	rport_ptr->sec_rule = NULL;
	rport_ptr->server = NULL;
	rport_ptr->tx_quota_cnt = 0;
	mutex_init(&rport_ptr->quota_lock_lhb2);
	INIT_LIST_HEAD(&rport_ptr->resume_tx_port_list);
	down_write(&rt_entry->lock_lha4);
	list_add_tail(&rport_ptr->list,
		      &rt_entry->remote_port_list[key]);
	up_write(&rt_entry->lock_lha4);
	return rport_ptr;
}

/**
 * msm_ipc_router_free_resume_tx_port() - Free the resume_tx ports
 * @rport_ptr: Pointer to the remote port.
 *
 * This function deletes all the resume_tx ports associated with a remote port
 * and frees the memory allocated to each resume_tx port.
 *
 * Must be called with rport_ptr->quota_lock_lhb2 locked.
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
 * Must be called with rport_ptr->quota_lock_lhb2 locked.
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
 * post_resume_tx() - Post the resume_tx event
 * @rport_ptr: Pointer to the remote port
 * @pkt : The data packet that is received on a resume_tx event
 *
 * This function informs about the reception of the resume_tx message from a
 * remote port pointed by rport_ptr to all the local ports that are in the
 * resume_tx_ports_list of this remote port. On posting the information, this
 * function sequentially deletes each entry in the resume_tx_port_list of the
 * remote port.
 *
 * Must be called with rport_ptr->quota_lock_lhb2 locked.
 */
static void post_resume_tx(struct msm_ipc_router_remote_port *rport_ptr,
						   struct rr_packet *pkt)
{
	struct msm_ipc_resume_tx_port *rtx_port, *tmp_rtx_port;
	struct msm_ipc_port *local_port;

	list_for_each_entry_safe(rtx_port, tmp_rtx_port,
				&rport_ptr->resume_tx_port_list, list) {
		local_port =
			msm_ipc_router_lookup_local_port(rtx_port->port_id);
		if (local_port && local_port->notify)
			local_port->notify(MSM_IPC_ROUTER_RESUME_TX,
						local_port->priv);
		else if (local_port)
			post_pkt_to_port(local_port, pkt, 1);
		else
			pr_err("%s: Local Port %d not Found",
				__func__, rtx_port->port_id);
		list_del(&rtx_port->list);
		kfree(rtx_port);
	}
}

/* Must be called with routing_table_lock_lha3 locked. */
static void msm_ipc_router_destroy_remote_port(
	struct msm_ipc_router_remote_port *rport_ptr)
{
	uint32_t node_id;
	struct msm_ipc_routing_table_entry *rt_entry;

	if (!rport_ptr)
		return;

	node_id = rport_ptr->node_id;
	rt_entry = lookup_routing_table(node_id);
	if (!rt_entry) {
		pr_err("%s: Node %d is not up\n", __func__, node_id);
		return;
	}
	down_write(&rt_entry->lock_lha4);
	list_del(&rport_ptr->list);
	up_write(&rt_entry->lock_lha4);
	mutex_lock(&rport_ptr->quota_lock_lhb2);
	msm_ipc_router_free_resume_tx_port(rport_ptr);
	mutex_unlock(&rport_ptr->quota_lock_lhb2);
	kfree(rport_ptr);
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

static void dummy_release(struct device *dev)
{
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
 * Note: Lock the server_list_lock_lha2 before accessing this function.
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
	int key = (service & (SRV_HASH_SIZE - 1));

	list_for_each_entry(server, &server_list[key], list) {
		if ((server->name.service == service) &&
		    (server->name.instance == instance))
			goto create_srv_port;
	}

	server = kzalloc(sizeof(struct msm_ipc_server), GFP_KERNEL);
	if (!server) {
		pr_err("%s: Server allocation failed\n", __func__);
		return NULL;
	}
	server->name.service = service;
	server->name.instance = instance;
	server->synced_sec_rule = 0;
	INIT_LIST_HEAD(&server->server_port_list);
	list_add_tail(&server->list, &server_list[key]);
	scnprintf(server->pdev_name, sizeof(server->pdev_name),
		  "QMI%08x:%08x", service, instance);
	server->next_pdev_id = 1;

create_srv_port:
	server_port = kzalloc(sizeof(struct msm_ipc_server_port), GFP_KERNEL);
	if (!server_port) {
		if (list_empty(&server->server_port_list)) {
			list_del(&server->list);
			kfree(server);
		}
		pr_err("%s: Server Port allocation failed\n", __func__);
		return NULL;
	}
	server_port->server_addr.node_id = node_id;
	server_port->server_addr.port_id = port_id;
	server_port->xprt_info = xprt_info;
	list_add_tail(&server_port->list, &server->server_port_list);

	server_port->pdev.name = server->pdev_name;
	server_port->pdev.id = server->next_pdev_id++;
	server_port->pdev.dev.release = dummy_release;
	platform_device_register(&server_port->pdev);

	return server;
}

/**
 * msm_ipc_router_destroy_server() - Remove server info from hash table
 * @server: Server info to be removed.
 * @node_id: Node/Processor ID in which the server is hosted.
 * @port_id: Port ID within the node in which the server is hosted.
 *
 * This function removes the server_port identified using <node_id:port_id>
 * from the server structure. If the server_port list under server structure
 * is empty after removal, then remove the server structure from the server
 * hash table.
 * Note: Lock the server_list_lock_lha2 before accessing this function.
 */
static void msm_ipc_router_destroy_server(struct msm_ipc_server *server,
					  uint32_t node_id, uint32_t port_id)
{
	struct msm_ipc_server_port *server_port;

	if (!server)
		return;

	list_for_each_entry(server_port, &server->server_port_list, list) {
		if ((server_port->server_addr.node_id == node_id) &&
		    (server_port->server_addr.port_id == port_id))
			break;
	}
	if (server_port) {
		platform_device_unregister(&server_port->pdev);
		list_del(&server_port->list);
		kfree(server_port);
	}
	if (list_empty(&server->server_port_list)) {
		list_del(&server->list);
		kfree(server);
	}
	return;
}

static int msm_ipc_router_send_control_msg(
		struct msm_ipc_router_xprt_info *xprt_info,
		union rr_control_msg *msg,
		uint32_t dst_node_id)
{
	struct rr_packet *pkt;
	struct sk_buff *ipc_rtr_pkt;
	struct rr_header_v1 *hdr;
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
	skb_queue_tail(pkt_fragment_q, ipc_rtr_pkt);
	pkt->pkt_fragment_q = pkt_fragment_q;
	pkt->length = sizeof(*msg);

	hdr = &(pkt->hdr);
	hdr->version = IPC_ROUTER_V1;
	hdr->type = msg->cmd;
	hdr->src_node_id = IPC_ROUTER_NID_LOCAL;
	hdr->src_port_id = IPC_ROUTER_ADDRESS;
	hdr->control_flag = 0;
	hdr->size = sizeof(*msg);
	if (hdr->type == IPC_ROUTER_CTRL_CMD_RESUME_TX)
		hdr->dst_node_id = dst_node_id;
	else
		hdr->dst_node_id = xprt_info->remote_node_id;
	hdr->dst_port_id = IPC_ROUTER_ADDRESS;

	mutex_lock(&xprt_info->tx_lock_lhb2);
	ret = prepend_header(pkt, xprt_info);
	if (ret < 0) {
		mutex_unlock(&xprt_info->tx_lock_lhb2);
		pr_err("%s: Prepend Header failed\n", __func__);
		release_pkt(pkt);
		return ret;
	}

	ret = xprt_info->xprt->write(pkt, pkt->length, xprt_info->xprt);
	mutex_unlock(&xprt_info->tx_lock_lhb2);

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
		pr_err("%s: Xprt info not initialized\n", __func__);
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
				msm_ipc_router_send_control_msg(xprt_info,
					&ctl, IPC_ROUTER_DUMMY_DEST_NODE);
			}
		}
	}

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
	struct rr_header_v1 *hdr;
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

	pkt_size = sizeof(*msg);
	ipc_rtr_pkt = alloc_skb(pkt_size, GFP_KERNEL);
	if (!ipc_rtr_pkt) {
		pr_err("%s: ipc_rtr_pkt alloc failed\n", __func__);
		kfree(pkt_fragment_q);
		kfree(pkt);
		return -ENOMEM;
	}

	data = skb_put(ipc_rtr_pkt, sizeof(*msg));
	memcpy(data, msg, sizeof(*msg));
	hdr = &(pkt->hdr);
	hdr->version = IPC_ROUTER_V1;
	hdr->type = msg->cmd;
	hdr->src_node_id = IPC_ROUTER_NID_LOCAL;
	hdr->src_port_id = IPC_ROUTER_ADDRESS;
	hdr->control_flag = 0;
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

	down_read(&xprt_info_list_lock_lha5);
	list_for_each_entry(xprt_info, &xprt_info_list, list) {
		msm_ipc_router_send_control_msg(xprt_info, ctl,
					IPC_ROUTER_DUMMY_DEST_NODE);
	}
	up_read(&xprt_info_list_lock_lha5);

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
			msm_ipc_router_send_control_msg(fwd_xprt_info, ctl,
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
	down_read(&routing_table_lock_lha3);
	rt_entry = lookup_routing_table(hdr->dst_node_id);
	if (!(rt_entry) || !(rt_entry->xprt_info)) {
		pr_err("%s: Routing table not initialized\n", __func__);
		ret = -ENODEV;
		goto fm_error1;
	}

	down_read(&rt_entry->lock_lha4);
	fwd_xprt_info = rt_entry->xprt_info;
	ret = prepend_header(pkt, fwd_xprt_info);
	if (ret < 0) {
		pr_err("%s: Prepend Header failed\n", __func__);
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
		pr_err("%s: Discarding Command to route back\n", __func__);
		ret = -EINVAL;
		goto fm_error3;
	}

	if (xprt_info->xprt->link_id == fwd_xprt_info->xprt->link_id) {
		pr_err("%s: DST in the same cluster\n", __func__);
		ret = 0;
		goto fm_error3;
	}
	fwd_xprt_info->xprt->write(pkt, pkt->length, fwd_xprt_info->xprt);

fm_error3:
	mutex_unlock(&fwd_xprt_info->tx_lock_lhb2);
fm_error2:
	up_read(&rt_entry->lock_lha4);
fm_error1:
	up_read(&routing_table_lock_lha3);

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
		pr_err("%s: NULL mode_info\n", __func__);
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
			msm_ipc_router_send_control_msg(tmp_xprt_info, &msg,
						IPC_ROUTER_DUMMY_DEST_NODE);
			break;
		}
		up_read(&xprt_info_list_lock_lha5);
	} else if ((mode == SINGLE_LINK_MODE) && !xprt_info) {
		broadcast_ctl_msg_locally(&msg);
	} else if (mode == MULTI_LINK_MODE) {
		broadcast_ctl_msg(&msg);
		broadcast_ctl_msg_locally(&msg);
	} else if (mode != NULL_MODE) {
		pr_err("%s: Invalid mode(%d) + xprt_inf(%p) for %08x:%08x\n",
			__func__, mode, xprt_info, node_id, port_id);
		rc = -EINVAL;
	}
	return rc;
}

static void update_comm_mode_info(struct comm_mode_info *mode_info,
				  struct msm_ipc_router_xprt_info *xprt_info)
{
	if (!mode_info) {
		pr_err("%s: NULL mode_info\n", __func__);
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

static void cleanup_rmt_server(struct msm_ipc_router_xprt_info *xprt_info,
			       struct msm_ipc_router_remote_port *rport_ptr)
{
	union rr_control_msg ctl;
	struct msm_ipc_server *server = rport_ptr->server;

	D("Remove server %08x:%08x - %08x:%08x",
	   server->name.service, server->name.instance,
	   rport_ptr->node_id, rport_ptr->port_id);
	memset(&ctl, 0, sizeof(ctl));
	ctl.cmd = IPC_ROUTER_CTRL_CMD_REMOVE_SERVER;
	ctl.srv.service = server->name.service;
	ctl.srv.instance = server->name.instance;
	ctl.srv.node_id = rport_ptr->node_id;
	ctl.srv.port_id = rport_ptr->port_id;
	relay_ctl_msg(xprt_info, &ctl);
	broadcast_ctl_msg_locally(&ctl);
	msm_ipc_router_destroy_server(server,
			rport_ptr->node_id, rport_ptr->port_id);
}

static void cleanup_rmt_ports(struct msm_ipc_router_xprt_info *xprt_info,
			      struct msm_ipc_routing_table_entry *rt_entry)
{
	struct msm_ipc_router_remote_port *rport_ptr, *tmp_rport_ptr;
	union rr_control_msg ctl;
	int j;

	memset(&ctl, 0, sizeof(ctl));
	for (j = 0; j < RP_HASH_SIZE; j++) {
		list_for_each_entry_safe(rport_ptr, tmp_rport_ptr,
				&rt_entry->remote_port_list[j], list) {
			list_del(&rport_ptr->list);
			mutex_lock(&rport_ptr->quota_lock_lhb2);
			msm_ipc_router_free_resume_tx_port(rport_ptr);
			mutex_unlock(&rport_ptr->quota_lock_lhb2);

			if (rport_ptr->server)
				cleanup_rmt_server(xprt_info, rport_ptr);

			ctl.cmd = IPC_ROUTER_CTRL_CMD_REMOVE_CLIENT;
			ctl.cli.node_id = rport_ptr->node_id;
			ctl.cli.port_id = rport_ptr->port_id;
			relay_ctl_msg(xprt_info, &ctl);
			broadcast_ctl_msg_locally(&ctl);
			kfree(rport_ptr);
		}
	}
}

static void msm_ipc_cleanup_routing_table(
	struct msm_ipc_router_xprt_info *xprt_info)
{
	int i;
	struct msm_ipc_routing_table_entry *rt_entry, *tmp_rt_entry;

	if (!xprt_info) {
		pr_err("%s: Invalid xprt_info\n", __func__);
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
			kfree(rt_entry);
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

	down_read(&routing_table_lock_lha3);
	list_for_each_entry(server_port, &server->server_port_list, list) {
		rport_ptr = msm_ipc_router_lookup_remote_port(
				server_port->server_addr.node_id,
				server_port->server_addr.port_id);
		if (!rport_ptr)
			continue;
		rport_ptr->sec_rule = rule;
	}
	up_read(&routing_table_lock_lha3);
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

		/*
		 * If the rule applies to all instances and if the specific
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

static int process_hello_msg(struct msm_ipc_router_xprt_info *xprt_info,
			     struct rr_header_v1 *hdr)
{
	int i, rc = 0;
	union rr_control_msg ctl;
	struct msm_ipc_routing_table_entry *rt_entry;

	if (!hdr)
		return -EINVAL;

	RR("o HELLO NID %d\n", hdr->src_node_id);

	xprt_info->remote_node_id = hdr->src_node_id;
	/*
	 * Find the entry from Routing Table corresponding to Node ID.
	 * Under SSR, an entry will be found. When the system boots up
	 * for the 1st time, an entry will not be found and hence allocate
	 * an entry. Update the entry with the Node ID that it corresponds
	 * to and the XPRT through which it can be reached.
	 */
	down_write(&routing_table_lock_lha3);
	rt_entry = lookup_routing_table(hdr->src_node_id);
	if (!rt_entry) {
		rt_entry = alloc_routing_table_entry(hdr->src_node_id);
		if (!rt_entry) {
			up_write(&routing_table_lock_lha3);
			pr_err("%s: rt_entry allocation failed\n", __func__);
			return -ENOMEM;
		}
		add_routing_table_entry(rt_entry);
	}
	down_write(&rt_entry->lock_lha4);
	rt_entry->neighbor_node_id = xprt_info->remote_node_id;
	rt_entry->xprt_info = xprt_info;
	up_write(&rt_entry->lock_lha4);
	up_write(&routing_table_lock_lha3);

	/* Send a reply HELLO message */
	memset(&ctl, 0, sizeof(ctl));
	ctl.hello.cmd = IPC_ROUTER_CTRL_CMD_HELLO;
	rc = msm_ipc_router_send_control_msg(xprt_info, &ctl,
						IPC_ROUTER_DUMMY_DEST_NODE);
	if (rc < 0) {
		pr_err("%s: Error sending reply HELLO message\n", __func__);
		return rc;
	}
	xprt_info->initialized = 1;

	/*
	 * Send list of servers from the local node and from nodes
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
	RR("HELLO message processed\n");
	return rc;
}

static int process_resume_tx_msg(union rr_control_msg *msg,
				 struct rr_packet *pkt)
{
	struct msm_ipc_router_remote_port *rport_ptr;
	int ret = 0;

	RR("o RESUME_TX id=%d:%08x\n", msg->cli.node_id, msg->cli.port_id);

	down_read(&local_ports_lock_lha2);
	down_read(&routing_table_lock_lha3);
	rport_ptr = msm_ipc_router_lookup_remote_port(msg->cli.node_id,
						      msg->cli.port_id);
	if (!rport_ptr) {
		pr_err("%s: Unable to resume client\n", __func__);
		ret = -ENODEV;
		goto prtm_out;
	}
	mutex_lock(&rport_ptr->quota_lock_lhb2);
	rport_ptr->tx_quota_cnt = 0;
	post_resume_tx(rport_ptr, pkt);
	mutex_unlock(&rport_ptr->quota_lock_lhb2);
prtm_out:
	up_read(&routing_table_lock_lha3);
	up_read(&local_ports_lock_lha2);
	return 0;
}

static int process_new_server_msg(struct msm_ipc_router_xprt_info *xprt_info,
			union rr_control_msg *msg, struct rr_packet *pkt)
{
	struct msm_ipc_routing_table_entry *rt_entry;
	struct msm_ipc_server *server;
	struct msm_ipc_router_remote_port *rport_ptr;

	if (msg->srv.instance == 0) {
		pr_err("%s: Server %08x create rejected, version = 0\n",
			__func__, msg->srv.service);
		return -EINVAL;
	}

	RR("o NEW_SERVER id=%d:%08x service=%08x:%08x\n", msg->srv.node_id,
	    msg->srv.port_id, msg->srv.service, msg->srv.instance);
	/*
	 * Find the entry from Routing Table corresponding to Node ID.
	 * Under SSR, an entry will be found. When the subsystem hosting
	 * service is not adjacent, an entry will not be found and hence
	 * allocate an entry. Update the entry with the Node ID that it
	 * corresponds to and the XPRT through which it can be reached.
	 */
	down_write(&routing_table_lock_lha3);
	rt_entry = lookup_routing_table(msg->srv.node_id);
	if (!rt_entry) {
		rt_entry = alloc_routing_table_entry(msg->srv.node_id);
		if (!rt_entry) {
			up_write(&routing_table_lock_lha3);
			pr_err("%s: rt_entry allocation failed\n", __func__);
			return -ENOMEM;
		}
		down_write(&rt_entry->lock_lha4);
		rt_entry->neighbor_node_id = xprt_info->remote_node_id;
		rt_entry->xprt_info = xprt_info;
		up_write(&rt_entry->lock_lha4);
		add_routing_table_entry(rt_entry);
	}
	up_write(&routing_table_lock_lha3);

	/*
	 * If the service does not exist already in the database, create and
	 * store the service info. Create a remote port structure in which
	 * the service is hosted and cache the security rule for the service
	 * in that remote port structure.
	 */
	down_write(&server_list_lock_lha2);
	server = msm_ipc_router_lookup_server(msg->srv.service,
			msg->srv.instance, msg->srv.node_id, msg->srv.port_id);
	if (!server) {
		server = msm_ipc_router_create_server(
				msg->srv.service, msg->srv.instance,
				msg->srv.node_id, msg->srv.port_id, xprt_info);
		if (!server) {
			up_write(&server_list_lock_lha2);
			pr_err("%s: Server Create failed\n", __func__);
			return -ENOMEM;
		}

		down_read(&routing_table_lock_lha3);
		if (!msm_ipc_router_lookup_remote_port(
				msg->srv.node_id, msg->srv.port_id)) {
			rport_ptr = msm_ipc_router_create_remote_port(
					msg->srv.node_id, msg->srv.port_id);
			if (!rport_ptr) {
				up_read(&routing_table_lock_lha3);
				up_write(&server_list_lock_lha2);
				return -ENOMEM;
			}
			rport_ptr->server = server;
			rport_ptr->sec_rule = msm_ipc_get_security_rule(
						msg->srv.service,
						msg->srv.instance);
		}
		up_read(&routing_table_lock_lha3);
	}
	up_write(&server_list_lock_lha2);

	/*
	 * Relay the new server message to other subsystems that do not belong
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

	RR("o REMOVE_SERVER service=%08x:%d\n",
	    msg->srv.service, msg->srv.instance);
	down_write(&server_list_lock_lha2);
	server = msm_ipc_router_lookup_server(msg->srv.service,
			msg->srv.instance, msg->srv.node_id, msg->srv.port_id);
	if (server) {
		msm_ipc_router_destroy_server(server, msg->srv.node_id,
					      msg->srv.port_id);
		/*
		 * Relay the new server message to other subsystems that do not
		 * belong to the cluster from which this message is received.
		 * Notify the local clients communicating with the service.
		 */
		relay_ctl_msg(xprt_info, msg);
		post_control_ports(pkt);
	}
	up_write(&server_list_lock_lha2);
	return 0;
}

static int process_rmv_client_msg(struct msm_ipc_router_xprt_info *xprt_info,
			union rr_control_msg *msg, struct rr_packet *pkt)
{
	struct msm_ipc_router_remote_port *rport_ptr;

	RR("o REMOVE_CLIENT id=%d:%08x\n", msg->cli.node_id, msg->cli.port_id);
	down_write(&routing_table_lock_lha3);
	rport_ptr = msm_ipc_router_lookup_remote_port(msg->cli.node_id,
						      msg->cli.port_id);
	if (rport_ptr)
		msm_ipc_router_destroy_remote_port(rport_ptr);
	up_write(&routing_table_lock_lha3);

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
		pr_err("%s: r2r msg size %d != %d\n", __func__, pkt->length,
			sizeof(*msg));
		return -EINVAL;
	}

	hdr = &(pkt->hdr);
	msg = msm_ipc_router_skb_to_buf(pkt->pkt_fragment_q, sizeof(*msg));
	if (!msg) {
		pr_err("%s: Error extracting control msg\n", __func__);
		return -ENOMEM;
	}

	switch (msg->cmd) {
	case IPC_ROUTER_CTRL_CMD_HELLO:
		rc = process_hello_msg(xprt_info, hdr);
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
	case IPC_ROUTER_CTRL_CMD_PING:
		/* No action needed for ping messages received */
		RR("o PING\n");
		break;
	default:
		RR("o UNKNOWN(%08x)\n", msg->cmd);
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
		if (pkt->length < IPC_ROUTER_HDR_SIZE ||
		    pkt->length > MAX_IPC_PKT_SIZE) {
			pr_err("%s: Invalid pkt length %d\n",
				__func__, pkt->length);
			goto fail_data;
		}

		ret = extract_header(pkt);
		if (ret < 0)
			goto fail_data;
		hdr = &(pkt->hdr);
		RAW("ver=%d type=%d src=%d:%08x crx=%d siz=%d dst=%d:%08x\n",
		     hdr->version, hdr->type, hdr->src_node_id,
		     hdr->src_port_id, hdr->control_flag, hdr->size,
		     hdr->dst_node_id, hdr->dst_port_id);

		if ((hdr->dst_node_id != IPC_ROUTER_NID_LOCAL) &&
		    ((hdr->type == IPC_ROUTER_CTRL_CMD_RESUME_TX) ||
		     (hdr->type == IPC_ROUTER_CTRL_CMD_DATA))) {
			forward_msg(xprt_info, pkt);
			release_pkt(pkt);
			continue;
		}

		if (hdr->type != IPC_ROUTER_CTRL_CMD_DATA) {
			process_control_msg(xprt_info, pkt);
			release_pkt(pkt);
			continue;
		}
#if defined(CONFIG_MSM_SMD_LOGGING)
#if defined(DEBUG)
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
#endif
#endif

		down_read(&local_ports_lock_lha2);
		port_ptr = msm_ipc_router_lookup_local_port(hdr->dst_port_id);
		if (!port_ptr) {
			pr_err("%s: No local port id %08x\n", __func__,
				hdr->dst_port_id);
			up_read(&local_ports_lock_lha2);
			release_pkt(pkt);
			return;
		}

		down_read(&routing_table_lock_lha3);
		rport_ptr = msm_ipc_router_lookup_remote_port(hdr->src_node_id,
							hdr->src_port_id);
		if (!rport_ptr) {
			rport_ptr = msm_ipc_router_create_remote_port(
							hdr->src_node_id,
							hdr->src_port_id);
			if (!rport_ptr) {
				pr_err("%s: Rmt Prt %08x:%08x create failed\n",
					__func__, hdr->src_node_id,
					hdr->src_port_id);
				up_read(&routing_table_lock_lha3);
				up_read(&local_ports_lock_lha2);
				release_pkt(pkt);
				return;
			}
		}
		up_read(&routing_table_lock_lha3);
		post_pkt_to_port(port_ptr, pkt, 0);
		up_read(&local_ports_lock_lha2);
	}
	return;

fail_data:
	release_pkt(pkt);
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

	down_write(&server_list_lock_lha2);
	server = msm_ipc_router_lookup_server(name->addr.port_name.service,
					      name->addr.port_name.instance,
					      IPC_ROUTER_NID_LOCAL,
					      port_ptr->this_port.port_id);
	if (server) {
		up_write(&server_list_lock_lha2);
		pr_err("%s: Server already present\n", __func__);
		return -EINVAL;
	}

	server = msm_ipc_router_create_server(name->addr.port_name.service,
					      name->addr.port_name.instance,
					      IPC_ROUTER_NID_LOCAL,
					      port_ptr->this_port.port_id,
					      NULL);
	if (!server) {
		up_write(&server_list_lock_lha2);
		pr_err("%s: Server Creation failed\n", __func__);
		return -EINVAL;
	}

	memset(&ctl, 0, sizeof(ctl));
	ctl.cmd = IPC_ROUTER_CTRL_CMD_NEW_SERVER;
	ctl.srv.service = server->name.service;
	ctl.srv.instance = server->name.instance;
	ctl.srv.node_id = IPC_ROUTER_NID_LOCAL;
	ctl.srv.port_id = port_ptr->this_port.port_id;
	up_write(&server_list_lock_lha2);
	broadcast_ctl_msg(&ctl);
	broadcast_ctl_msg_locally(&ctl);
	spin_lock_irqsave(&port_ptr->port_lock, flags);
	port_ptr->type = SERVER_PORT;
	port_ptr->mode_info.mode = MULTI_LINK_MODE;
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

	down_write(&server_list_lock_lha2);
	server = msm_ipc_router_lookup_server(port_ptr->port_name.service,
					      port_ptr->port_name.instance,
					      port_ptr->this_port.node_id,
					      port_ptr->this_port.port_id);
	if (!server) {
		up_write(&server_list_lock_lha2);
		pr_err("%s: Server lookup failed\n", __func__);
		return -ENODEV;
	}

	memset(&ctl, 0, sizeof(ctl));
	ctl.cmd = IPC_ROUTER_CTRL_CMD_REMOVE_SERVER;
	ctl.srv.service = server->name.service;
	ctl.srv.instance = server->name.instance;
	ctl.srv.node_id = IPC_ROUTER_NID_LOCAL;
	ctl.srv.port_id = port_ptr->this_port.port_id;
	msm_ipc_router_destroy_server(server, port_ptr->this_port.node_id,
				      port_ptr->this_port.port_id);
	up_write(&server_list_lock_lha2);
	broadcast_ctl_msg(&ctl);
	broadcast_ctl_msg_locally(&ctl);
	spin_lock_irqsave(&port_ptr->port_lock, flags);
	port_ptr->type = CLIENT_PORT;
	spin_unlock_irqrestore(&port_ptr->port_lock, flags);
	return 0;
}

static int loopback_data(struct msm_ipc_port *src,
			uint32_t port_id,
			struct sk_buff_head *data)
{
	struct rr_header_v1 *hdr;
	struct msm_ipc_port *port_ptr;
	struct rr_packet *pkt;
	int ret_len;

	if (!data) {
		pr_err("%s: Invalid pkt pointer\n", __func__);
		return -EINVAL;
	}

	pkt = create_pkt(data);
	if (!pkt) {
		pr_err("%s: New pkt create failed\n", __func__);
		return -ENOMEM;
	}
	hdr = &(pkt->hdr);
	hdr->version = IPC_ROUTER_V1;
	hdr->type = IPC_ROUTER_CTRL_CMD_DATA;
	hdr->src_node_id = src->this_port.node_id;
	hdr->src_port_id = src->this_port.port_id;
	hdr->size = pkt->length;
	hdr->control_flag = 0;
	hdr->dst_node_id = IPC_ROUTER_NID_LOCAL;
	hdr->dst_port_id = port_id;

	down_read(&local_ports_lock_lha2);
	port_ptr = msm_ipc_router_lookup_local_port(port_id);
	if (!port_ptr) {
		pr_err("%s: Local port %d not present\n", __func__, port_id);
		up_read(&local_ports_lock_lha2);
		pkt->pkt_fragment_q = NULL;
		release_pkt(pkt);
		return -ENODEV;
	}

	ret_len = pkt->length;
	post_pkt_to_port(port_ptr, pkt, 0);
	update_comm_mode_info(&src->mode_info, NULL);
	up_read(&local_ports_lock_lha2);

	return ret_len;
}

static int msm_ipc_router_write_pkt(struct msm_ipc_port *src,
				struct msm_ipc_router_remote_port *rport_ptr,
				struct rr_packet *pkt)
{
	struct rr_header_v1 *hdr;
	struct msm_ipc_router_xprt_info *xprt_info;
	struct msm_ipc_routing_table_entry *rt_entry;
	struct msm_ipc_resume_tx_port  *resume_tx_port;
	struct sk_buff *temp_skb;
	int xprt_option;
	int ret;
	int align_size;

	if (!rport_ptr || !src || !pkt)
		return -EINVAL;

	hdr = &(pkt->hdr);
	hdr->type = IPC_ROUTER_CTRL_CMD_DATA;
	hdr->src_node_id = src->this_port.node_id;
	hdr->src_port_id = src->this_port.port_id;
	hdr->size = pkt->length;
	hdr->control_flag = 0;
	hdr->dst_node_id = rport_ptr->node_id;
	hdr->dst_port_id = rport_ptr->port_id;

	mutex_lock(&rport_ptr->quota_lock_lhb2);
	if (rport_ptr->tx_quota_cnt == IPC_ROUTER_DEFAULT_RX_QUOTA) {
		if (msm_ipc_router_lookup_resume_tx_port(
			rport_ptr, src->this_port.port_id)) {
			mutex_unlock(&rport_ptr->quota_lock_lhb2);
			return -EAGAIN;
		}
		resume_tx_port =
			kzalloc(sizeof(struct msm_ipc_resume_tx_port),
							GFP_KERNEL);
		if (!resume_tx_port) {
			pr_err("%s: Resume_Tx port allocation failed\n",
								__func__);
			mutex_unlock(&rport_ptr->quota_lock_lhb2);
			return -ENOMEM;
		}
		INIT_LIST_HEAD(&resume_tx_port->list);
		resume_tx_port->port_id = src->this_port.port_id;
		resume_tx_port->node_id = src->this_port.node_id;
		list_add_tail(&resume_tx_port->list,
				&rport_ptr->resume_tx_port_list);
		mutex_unlock(&rport_ptr->quota_lock_lhb2);
		return -EAGAIN;
	}
	rport_ptr->tx_quota_cnt++;
	if (rport_ptr->tx_quota_cnt == IPC_ROUTER_DEFAULT_RX_QUOTA)
		hdr->control_flag |= CONTROL_FLAG_CONFIRM_RX;
	mutex_unlock(&rport_ptr->quota_lock_lhb2);

	rt_entry = lookup_routing_table(hdr->dst_node_id);
	if (!rt_entry || !rt_entry->xprt_info) {
		pr_err("%s: Remote node %d not up\n",
			__func__, hdr->dst_node_id);
		return -ENODEV;
	}
	down_read(&rt_entry->lock_lha4);
	xprt_info = rt_entry->xprt_info;
	ret = prepend_header(pkt, xprt_info);
	if (ret < 0) {
		up_read(&rt_entry->lock_lha4);
		pr_err("%s: Prepend Header failed\n", __func__);
		return ret;
	}
	xprt_option = xprt_info->xprt->get_option(xprt_info->xprt);
	if (!(xprt_option & FRAG_PKT_WRITE_ENABLE)) {
		ret = defragment_pkt(pkt);
		if (ret < 0) {
			up_read(&rt_entry->lock_lha4);
			return ret;
		}
	}

	temp_skb = skb_peek_tail(pkt->pkt_fragment_q);
	align_size = ALIGN_SIZE(pkt->length);
	skb_put(temp_skb, align_size);
	pkt->length += align_size;
	mutex_lock(&xprt_info->tx_lock_lhb2);
	ret = xprt_info->xprt->write(pkt, pkt->length, xprt_info->xprt);
	mutex_unlock(&xprt_info->tx_lock_lhb2);
	up_read(&rt_entry->lock_lha4);

	if (ret < 0) {
		pr_err("%s: Write on XPRT failed\n", __func__);
		return ret;
	}
	update_comm_mode_info(&src->mode_info, xprt_info);

	RAW_HDR("[w rr_h] "
		"ver=%i,type=%s,src_nid=%08x,src_port_id=%08x,"
		"control_flag=%i,size=%3i,dst_pid=%08x,dst_cid=%08x\n",
		hdr->version, type_to_str(hdr->type),
		hdr->src_node_id, hdr->src_port_id,
		hdr->control_flag, hdr->size,
		hdr->dst_node_id, hdr->dst_port_id);

#if defined(CONFIG_MSM_SMD_LOGGING)
#if defined(DEBUG)
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
#endif
#endif

	return hdr->size;
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
		down_read(&server_list_lock_lha2);
		server = msm_ipc_router_lookup_server(
					dest->addr.port_name.service,
					dest->addr.port_name.instance,
					0, 0);
		if (!server) {
			up_read(&server_list_lock_lha2);
			pr_err("%s: Destination not reachable\n", __func__);
			return -ENODEV;
		}
		server_port = list_first_entry(&server->server_port_list,
					       struct msm_ipc_server_port,
					       list);
		dst_node_id = server_port->server_addr.node_id;
		dst_port_id = server_port->server_addr.port_id;
		up_read(&server_list_lock_lha2);
	}
	if (dst_node_id == IPC_ROUTER_NID_LOCAL) {
		ret = loopback_data(src, dst_port_id, data);
		return ret;
	}

	down_read(&routing_table_lock_lha3);
	rport_ptr = msm_ipc_router_lookup_remote_port(dst_node_id,
						      dst_port_id);
	if (!rport_ptr) {
		up_read(&routing_table_lock_lha3);
		pr_err("%s: Remote port not found\n", __func__);
		return -ENODEV;
	}

	if (src->check_send_permissions) {
		ret = src->check_send_permissions(rport_ptr->sec_rule);
		if (ret <= 0) {
			up_read(&routing_table_lock_lha3);
			pr_err("%s: permission failure for %s\n",
				__func__, current->comm);
			return -EPERM;
		}
	}

	pkt = create_pkt(data);
	if (!pkt) {
		up_read(&routing_table_lock_lha3);
		pr_err("%s: Pkt creation failed\n", __func__);
		return -ENOMEM;
	}

	ret = msm_ipc_router_write_pkt(src, rport_ptr, pkt);
	up_read(&routing_table_lock_lha3);
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
		pr_err("%s: SKB conversion failed\n", __func__);
		return -EFAULT;
	}

	ret = msm_ipc_router_send_to(src, out_skb_head, dest);
	if (ret < 0) {
		if (ret != -EAGAIN)
			pr_err("%s: msm_ipc_router_send_to failed - ret: %d\n",
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
	down_read(&routing_table_lock_lha3);
	rt_entry = lookup_routing_table(hdr->src_node_id);
	if (!rt_entry) {
		pr_err("%s: %d Node is not present",
				__func__, hdr->src_node_id);
		up_read(&routing_table_lock_lha3);
		return -ENODEV;
	}
	RR("x RESUME_TX id=%d:%08x\n",
			msg.cli.node_id, msg.cli.port_id);
	ret = msm_ipc_router_send_control_msg(rt_entry->xprt_info, &msg,
						hdr->src_node_id);
	up_read(&routing_table_lock_lha3);
	if (ret < 0)
		pr_err("%s: Send Resume_Tx Failed SRC_NODE: %d SRC_PORT: %d DEST_NODE: %d",
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

	mutex_lock(&port_ptr->port_rx_q_lock_lhb3);
	if (list_empty(&port_ptr->port_rx_q)) {
		mutex_unlock(&port_ptr->port_rx_q_lock_lhb3);
		return -EAGAIN;
	}

	pkt = list_first_entry(&port_ptr->port_rx_q, struct rr_packet, list);
	if ((buf_len) && (pkt->hdr.size > buf_len)) {
		mutex_unlock(&port_ptr->port_rx_q_lock_lhb3);
		return -ETOOSMALL;
	}
	list_del(&pkt->list);
	if (list_empty(&port_ptr->port_rx_q))
		wake_unlock(&port_ptr->port_rx_wake_lock);
	*read_pkt = pkt;
	mutex_unlock(&port_ptr->port_rx_q_lock_lhb3);
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

	mutex_lock(&port_ptr->port_rx_q_lock_lhb3);
	while (list_empty(&port_ptr->port_rx_q)) {
		mutex_unlock(&port_ptr->port_rx_q_lock_lhb3);
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
		mutex_lock(&port_ptr->port_rx_q_lock_lhb3);
	}
	mutex_unlock(&port_ptr->port_rx_q_lock_lhb3);

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
		pr_err("%s: Invalid pointers being passed\n", __func__);
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
			pr_err("%s: msm_ipc_router_recv_from failed - ret: %d\n",
				__func__, ret);
		return ret;
	}

	*data = msm_ipc_router_skb_to_buf(pkt->pkt_fragment_q, ret);
	if (!(*data))
		pr_err("%s: Buf conversion failed\n", __func__);

	*len = ret;
	release_pkt(pkt);
	return 0;
}

struct msm_ipc_port *msm_ipc_router_create_port(
	void (*notify)(unsigned event, void *priv),
	void *priv)
{
	struct msm_ipc_port *port_ptr;
	int ret;

	ret = wait_for_completion_interruptible(&msm_ipc_local_router_up);
	if (ret < 0) {
		pr_err("%s: Error waiting for local router\n", __func__);
		return NULL;
	}

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
		down_write(&local_ports_lock_lha2);
		list_del(&port_ptr->list);
		up_write(&local_ports_lock_lha2);

		if (port_ptr->type == SERVER_PORT) {
			memset(&msg, 0, sizeof(msg));
			msg.cmd = IPC_ROUTER_CTRL_CMD_REMOVE_SERVER;
			msg.srv.service = port_ptr->port_name.service;
			msg.srv.instance = port_ptr->port_name.instance;
			msg.srv.node_id = port_ptr->this_port.node_id;
			msg.srv.port_id = port_ptr->this_port.port_id;
			RR("x REMOVE_SERVER Name=%d:%08x Id=%d:%08x\n",
			   msg.srv.service, msg.srv.instance,
			   msg.srv.node_id, msg.srv.port_id);
			broadcast_ctl_msg(&msg);
			broadcast_ctl_msg_locally(&msg);
		}

		/*
		 * Server port could have been a client port earlier.
		 * Send REMOVE_CLIENT message in either case.
		 */
		RR("x REMOVE_CLIENT id=%d:%08x\n",
		   port_ptr->this_port.node_id, port_ptr->this_port.port_id);
		msm_ipc_router_send_remove_client(&port_ptr->mode_info,
			port_ptr->this_port.node_id,
			port_ptr->this_port.port_id);
	} else if (port_ptr->type == CONTROL_PORT) {
		down_write(&control_ports_lock_lha5);
		list_del(&port_ptr->list);
		up_write(&control_ports_lock_lha5);
	} else if (port_ptr->type == IRSC_PORT) {
		down_write(&local_ports_lock_lha2);
		list_del(&port_ptr->list);
		up_write(&local_ports_lock_lha2);
		signal_irsc_completion();
	}

	mutex_lock(&port_ptr->port_rx_q_lock_lhb3);
	list_for_each_entry_safe(pkt, temp_pkt, &port_ptr->port_rx_q, list) {
		list_del(&pkt->list);
		release_pkt(pkt);
	}
	mutex_unlock(&port_ptr->port_rx_q_lock_lhb3);

	if (port_ptr->type == SERVER_PORT) {
		down_write(&server_list_lock_lha2);
		server = msm_ipc_router_lookup_server(
				port_ptr->port_name.service,
				port_ptr->port_name.instance,
				port_ptr->this_port.node_id,
				port_ptr->this_port.port_id);
		if (server)
			msm_ipc_router_destroy_server(server,
				port_ptr->this_port.node_id,
				port_ptr->this_port.port_id);
		up_write(&server_list_lock_lha2);
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

	mutex_lock(&port_ptr->port_rx_q_lock_lhb3);
	if (!list_empty(&port_ptr->port_rx_q)) {
		pkt = list_first_entry(&port_ptr->port_rx_q,
					struct rr_packet, list);
		rc = pkt->length;
	}
	mutex_unlock(&port_ptr->port_rx_q_lock_lhb3);

	return rc;
}

int msm_ipc_router_bind_control_port(struct msm_ipc_port *port_ptr)
{
	if (!port_ptr)
		return -EINVAL;

	down_write(&local_ports_lock_lha2);
	list_del(&port_ptr->list);
	up_write(&local_ports_lock_lha2);
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
		pr_err("%s: Invalid srv_name\n", __func__);
		return -EINVAL;
	}

	if (num_entries_in_array && !srv_info) {
		pr_err("%s: srv_info NULL\n", __func__);
		return -EINVAL;
	}

	down_read(&server_list_lock_lha2);
	if (!lookup_mask)
		lookup_mask = 0xFFFFFFFF;
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

#if defined(CONFIG_DEBUG_FS)
static int dump_routing_table(char *buf, int max)
{
	int i = 0, j;
	struct msm_ipc_routing_table_entry *rt_entry;

	for (j = 0; j < RT_HASH_SIZE; j++) {
		down_read(&routing_table_lock_lha3);
		list_for_each_entry(rt_entry, &routing_table[j], list) {
			down_read(&rt_entry->lock_lha4);
			i += scnprintf(buf + i, max - i,
				       "Node Id: 0x%08x\n", rt_entry->node_id);
			if (rt_entry->node_id == IPC_ROUTER_NID_LOCAL) {
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
			up_read(&rt_entry->lock_lha4);
		}
		up_read(&routing_table_lock_lha3);
	}

	return i;
}

static int dump_xprt_info(char *buf, int max)
{
	int i = 0;
	struct msm_ipc_router_xprt_info *xprt_info;

	down_read(&xprt_info_list_lock_lha5);
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
	up_read(&xprt_info_list_lock_lha5);

	return i;
}

static int dump_servers(char *buf, int max)
{
	int i = 0, j;
	struct msm_ipc_server *server;
	struct msm_ipc_server_port *server_port;

	down_read(&server_list_lock_lha2);
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
	up_read(&server_list_lock_lha2);

	return i;
}

static int dump_remote_ports(char *buf, int max)
{
	int i = 0, j, k;
	struct msm_ipc_router_remote_port *rport_ptr;
	struct msm_ipc_routing_table_entry *rt_entry;

	for (j = 0; j < RT_HASH_SIZE; j++) {
		down_read(&routing_table_lock_lha3);
		list_for_each_entry(rt_entry, &routing_table[j], list) {
			down_read(&rt_entry->lock_lha4);
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
			up_read(&rt_entry->lock_lha4);
		}
		up_read(&routing_table_lock_lha3);
	}

	return i;
}

static int dump_control_ports(char *buf, int max)
{
	int i = 0;
	struct msm_ipc_port *port_ptr;

	down_read(&control_ports_lock_lha5);
	list_for_each_entry(port_ptr, &control_ports, list) {
		i += scnprintf(buf + i, max - i, "Node_id: 0x%08x\n",
			       port_ptr->this_port.node_id);
		i += scnprintf(buf + i, max - i, "Port_id: 0x%08x\n",
			       port_ptr->this_port.port_id);
		i += scnprintf(buf + i, max - i, "\n");
	}
	up_read(&control_ports_lock_lha5);

	return i;
}

static int dump_local_ports(char *buf, int max)
{
	int i = 0, j;
	unsigned long flags;
	struct msm_ipc_port *port_ptr;

	down_read(&local_ports_lock_lha2);
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
	up_read(&local_ports_lock_lha2);

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
	mutex_init(&xprt_info->rx_lock_lhb2);
	mutex_init(&xprt_info->tx_lock_lhb2);
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

	down_write(&xprt_info_list_lock_lha5);
	list_add_tail(&xprt_info->list, &xprt_info_list);
	up_write(&xprt_info_list_lock_lha5);

	down_write(&routing_table_lock_lha3);
	if (!routing_table_inited) {
		init_routing_table();
		rt_entry = alloc_routing_table_entry(IPC_ROUTER_NID_LOCAL);
		add_routing_table_entry(rt_entry);
		routing_table_inited = 1;
	}
	up_write(&routing_table_lock_lha3);

	xprt->priv = xprt_info;

	return 0;
}

static void msm_ipc_router_remove_xprt(struct msm_ipc_router_xprt *xprt)
{
	struct msm_ipc_router_xprt_info *xprt_info;

	if (xprt && xprt->priv) {
		xprt_info = xprt->priv;

		mutex_lock(&xprt_info->rx_lock_lhb2);
		xprt_info->abort_data_read = 1;
		mutex_unlock(&xprt_info->rx_lock_lhb2);

		down_write(&xprt_info_list_lock_lha5);
		list_del(&xprt_info->list);
		up_write(&xprt_info_list_lock_lha5);

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

	msm_ipc_cleanup_routing_table(xprt_work->xprt->priv);
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
		if (xprt_work) {
			xprt_work->xprt = xprt;
			INIT_WORK(&xprt_work->work, xprt_open_worker);
			queue_work(msm_ipc_router_workqueue, &xprt_work->work);
		} else {
			pr_err("%s: malloc failure - Couldn't notify OPEN event",
				__func__);
		}
		break;

	case IPC_ROUTER_XPRT_EVENT_CLOSE:
		D("close event for '%s'\n", xprt->name);
		xprt_work = kmalloc(sizeof(struct msm_ipc_router_xprt_work),
				GFP_ATOMIC);
		if (xprt_work) {
			xprt_work->xprt = xprt;
			INIT_WORK(&xprt_work->work, xprt_close_worker);
			queue_work(msm_ipc_router_workqueue, &xprt_work->work);
		} else {
			pr_err("%s: malloc failure - Couldn't notify CLOSE event",
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
	wake_lock(&xprt_info->wakelock);
	mutex_unlock(&xprt_info->rx_lock_lhb2);
	queue_work(xprt_info->workqueue, &xprt_info->read_data);
}

static int __init msm_ipc_router_init(void)
{
	int i, ret;
	struct msm_ipc_routing_table_entry *rt_entry;

	msm_ipc_router_debug_mask |= SMEM_LOG;
	ipc_rtr_log_ctxt = ipc_log_context_create(IPC_RTR_LOG_PAGES,
						  "ipc_router");
	if (!ipc_rtr_log_ctxt)
		pr_err("%s: Unable to create IPC logging for IPC RTR",
			__func__);

	msm_ipc_router_workqueue =
		create_singlethread_workqueue("msm_ipc_router");
	if (!msm_ipc_router_workqueue)
		return -ENOMEM;

	debugfs_init();

	for (i = 0; i < SRV_HASH_SIZE; i++)
		INIT_LIST_HEAD(&server_list[i]);

	for (i = 0; i < LP_HASH_SIZE; i++)
		INIT_LIST_HEAD(&local_ports[i]);

	down_write(&routing_table_lock_lha3);
	if (!routing_table_inited) {
		init_routing_table();
		rt_entry = alloc_routing_table_entry(IPC_ROUTER_NID_LOCAL);
		add_routing_table_entry(rt_entry);
		routing_table_inited = 1;
	}
	up_write(&routing_table_lock_lha3);

	ret = msm_ipc_router_init_sockets();
	if (ret < 0)
		pr_err("%s: Init sockets failed\n", __func__);

	ret = msm_ipc_router_security_init();
	if (ret < 0)
		pr_err("%s: Security Init failed\n", __func__);

	complete_all(&msm_ipc_local_router_up);
	return ret;
}

module_init(msm_ipc_router_init);
MODULE_DESCRIPTION("MSM IPC Router");
MODULE_LICENSE("GPL v2");
