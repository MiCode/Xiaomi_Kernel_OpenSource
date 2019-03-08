/*
 * Copyright (c) 2015, Sony Mobile Communications Inc.
 * Copyright (c) 2013, 2018-2019 The Linux Foundation. All rights reserved.
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
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/netlink.h>
#include <linux/qrtr.h>
#include <linux/termios.h>	/* For TIOCINQ/OUTQ */
#include <linux/wait.h>
#include <linux/rwsem.h>
#include <linux/ipc_logging.h>
#include <linux/uidgid.h>

#include <net/sock.h>

#include "qrtr.h"

#define QRTR_LOG_PAGE_CNT 4
#define QRTR_INFO(ctx, x, ...)				\
	ipc_log_string(ctx, x, ##__VA_ARGS__)

#define QRTR_PROTO_VER_1 1
#define QRTR_PROTO_VER_2 3

/* auto-bind range */
#define QRTR_MIN_EPH_SOCKET 0x4000
#define QRTR_MAX_EPH_SOCKET 0x7fff

#define QRTR_PORT_CTRL_LEGACY 0xffff

/* qrtr socket states */
#define QRTR_STATE_MULTI	-2
#define QRTR_STATE_INIT	-1

#define AID_VENDOR_QRTR	KGIDT_INIT(2906)

/**
 * struct qrtr_hdr_v1 - (I|R)PCrouter packet header version 1
 * @version: protocol version
 * @type: packet type; one of QRTR_TYPE_*
 * @src_node_id: source node
 * @src_port_id: source port
 * @confirm_rx: boolean; whether a resume-tx packet should be send in reply
 * @size: length of packet, excluding this header
 * @dst_node_id: destination node
 * @dst_port_id: destination port
 */
struct qrtr_hdr_v1 {
	__le32 version;
	__le32 type;
	__le32 src_node_id;
	__le32 src_port_id;
	__le32 confirm_rx;
	__le32 size;
	__le32 dst_node_id;
	__le32 dst_port_id;
} __packed;

/**
 * struct qrtr_hdr_v2 - (I|R)PCrouter packet header later versions
 * @version: protocol version
 * @type: packet type; one of QRTR_TYPE_*
 * @flags: bitmask of QRTR_FLAGS_*
 * @optlen: length of optional header data
 * @size: length of packet, excluding this header and optlen
 * @src_node_id: source node
 * @src_port_id: source port
 * @dst_node_id: destination node
 * @dst_port_id: destination port
 */
struct qrtr_hdr_v2 {
	u8 version;
	u8 type;
	u8 flags;
	u8 optlen;
	__le32 size;
	__le16 src_node_id;
	__le16 src_port_id;
	__le16 dst_node_id;
	__le16 dst_port_id;
} __packed;

#define QRTR_FLAGS_CONFIRM_RX	BIT(0)

struct qrtr_cb {
	u32 src_node;
	u32 src_port;
	u32 dst_node;
	u32 dst_port;

	u8 type;
	u8 confirm_rx;
};

#define QRTR_HDR_MAX_SIZE max_t(size_t, sizeof(struct qrtr_hdr_v1), \
					sizeof(struct qrtr_hdr_v2))

struct qrtr_sock {
	/* WARNING: sk must be the first member */
	struct sock sk;
	struct sockaddr_qrtr us;
	struct sockaddr_qrtr peer;

	int state;
};

static inline struct qrtr_sock *qrtr_sk(struct sock *sk)
{
	BUILD_BUG_ON(offsetof(struct qrtr_sock, sk) != 0);
	return container_of(sk, struct qrtr_sock, sk);
}

static unsigned int qrtr_local_nid = 1;

/* for node ids */
static RADIX_TREE(qrtr_nodes, GFP_KERNEL);
/* broadcast list */
static LIST_HEAD(qrtr_all_epts);
/* lock for qrtr_nodes, qrtr_all_epts and node reference */
static DECLARE_RWSEM(qrtr_node_lock);

/* local port allocation management */
static DEFINE_IDR(qrtr_ports);
static DEFINE_MUTEX(qrtr_port_lock);

/**
 * struct qrtr_node - endpoint node
 * @ep_lock: lock for endpoint management and callbacks
 * @ep: endpoint
 * @ref: reference count for node
 * @nid: node id
 * @net_id: network cluster identifer
 * @hello_sent: hello packet sent to endpoint
 * @qrtr_tx_flow: remote port tx flow control list
 * @resume_tx: wait until remote port acks control flag
 * @qrtr_tx_lock: lock for qrtr_tx_flow
 * @rx_queue: receive queue
 * @item: list item for broadcast list
 * @kworker: worker thread for recv work
 * @task: task to run the worker thread
 * @read_data: scheduled work for recv work
 * @ilc: ipc logging context reference
 */
struct qrtr_node {
	struct mutex ep_lock;
	struct qrtr_endpoint *ep;
	struct kref ref;
	unsigned int nid;
	unsigned int net_id;
	atomic_t hello_sent;

	struct radix_tree_root qrtr_tx_flow;
	struct wait_queue_head resume_tx;
	struct mutex qrtr_tx_lock;	/* for qrtr_tx_flow */

	struct sk_buff_head rx_queue;
	struct list_head item;

	struct kthread_worker kworker;
	struct task_struct *task;
	struct kthread_work read_data;

	void *ilc;
};

struct qrtr_tx_flow_waiter {
	struct list_head node;
	struct sock *sk;
};

struct qrtr_tx_flow {
	atomic_t pending;
	struct list_head waiters;
};

#define QRTR_TX_FLOW_HIGH	10
#define QRTR_TX_FLOW_LOW	5

static struct sk_buff *qrtr_alloc_ctrl_packet(struct qrtr_ctrl_pkt **pkt);
static int qrtr_local_enqueue(struct qrtr_node *node, struct sk_buff *skb,
			      int type, struct sockaddr_qrtr *from,
			      struct sockaddr_qrtr *to, unsigned int flags);
static int qrtr_bcast_enqueue(struct qrtr_node *node, struct sk_buff *skb,
			      int type, struct sockaddr_qrtr *from,
			      struct sockaddr_qrtr *to, unsigned int flags);

static void qrtr_log_tx_msg(struct qrtr_node *node, struct qrtr_hdr_v1 *hdr,
			    struct sk_buff *skb)
{
	const struct qrtr_ctrl_pkt *pkt;
	u64 pl_buf = 0;

	if (!hdr || !skb || !skb->data)
		return;

	if (hdr->type == QRTR_TYPE_DATA) {
		pl_buf = *(u64 *)(skb->data + QRTR_HDR_MAX_SIZE);
		QRTR_INFO(node->ilc,
			  "TX DATA: Len:0x%x CF:0x%x src[0x%x:0x%x] dst[0x%x:0x%x] [%08x %08x] [%s]\n",
			  hdr->size, hdr->confirm_rx,
			  hdr->src_node_id, hdr->src_port_id,
			  hdr->dst_node_id, hdr->dst_port_id,
			  (unsigned int)pl_buf, (unsigned int)(pl_buf >> 32),
			  current->comm);
	} else {
		pkt = (struct qrtr_ctrl_pkt *)(skb->data + QRTR_HDR_MAX_SIZE);
		if (hdr->type == QRTR_TYPE_NEW_SERVER ||
		    hdr->type == QRTR_TYPE_DEL_SERVER)
			QRTR_INFO(node->ilc,
				  "TX CTRL: cmd:0x%x SVC[0x%x:0x%x] addr[0x%x:0x%x]\n",
				  hdr->type, le32_to_cpu(pkt->server.service),
				  le32_to_cpu(pkt->server.instance),
				  le32_to_cpu(pkt->server.node),
				  le32_to_cpu(pkt->server.port));
		else if (hdr->type == QRTR_TYPE_DEL_CLIENT ||
			 hdr->type == QRTR_TYPE_RESUME_TX)
			QRTR_INFO(node->ilc,
				  "TX CTRL: cmd:0x%x addr[0x%x:0x%x]\n",
				  hdr->type, le32_to_cpu(pkt->client.node),
				  le32_to_cpu(pkt->client.port));
		else if (hdr->type == QRTR_TYPE_HELLO ||
			 hdr->type == QRTR_TYPE_BYE)
			QRTR_INFO(node->ilc,
				  "TX CTRL: cmd:0x%x node[0x%x]\n",
				  hdr->type, hdr->src_node_id);
		else if (hdr->type == QRTR_TYPE_DEL_PROC)
			QRTR_INFO(node->ilc,
				  "TX CTRL: cmd:0x%x node[0x%x]\n",
				  hdr->type, pkt->proc.node);
	}
}

static void qrtr_log_rx_msg(struct qrtr_node *node, struct sk_buff *skb)
{
	const struct qrtr_ctrl_pkt *pkt;
	struct qrtr_cb *cb;
	u64 pl_buf = 0;

	if (!skb || !skb->data)
		return;

	cb = (struct qrtr_cb *)skb->cb;

	if (cb->type == QRTR_TYPE_DATA) {
		pl_buf = *(u64 *)(skb->data);
		QRTR_INFO(node->ilc,
			  "RX DATA: Len:0x%x CF:0x%x src[0x%x:0x%x] dst[0x%x:0x%x] [%08x %08x]\n",
			  skb->len, cb->confirm_rx, cb->src_node, cb->src_port,
			  cb->dst_node, cb->dst_port,
			  (unsigned int)pl_buf, (unsigned int)(pl_buf >> 32));
	} else {
		pkt = (struct qrtr_ctrl_pkt *)(skb->data);
		if (cb->type == QRTR_TYPE_NEW_SERVER ||
		    cb->type == QRTR_TYPE_DEL_SERVER)
			QRTR_INFO(node->ilc,
				  "RX CTRL: cmd:0x%x SVC[0x%x:0x%x] addr[0x%x:0x%x]\n",
				  cb->type, le32_to_cpu(pkt->server.service),
				  le32_to_cpu(pkt->server.instance),
				  le32_to_cpu(pkt->server.node),
				  le32_to_cpu(pkt->server.port));
		else if (cb->type == QRTR_TYPE_DEL_CLIENT ||
			 cb->type == QRTR_TYPE_RESUME_TX)
			QRTR_INFO(node->ilc,
				  "RX CTRL: cmd:0x%x addr[0x%x:0x%x]\n",
				  cb->type, le32_to_cpu(pkt->client.node),
				  le32_to_cpu(pkt->client.port));
		else if (cb->type == QRTR_TYPE_HELLO ||
			 cb->type == QRTR_TYPE_BYE)
			QRTR_INFO(node->ilc,
				  "RX CTRL: cmd:0x%x node[0x%x]\n",
				  cb->type, cb->src_node);
	}
}

static bool refcount_dec_and_rwsem_lock(refcount_t *r,
					struct rw_semaphore *sem)
{
	if (refcount_dec_not_one(r))
		return false;

	down_write(sem);
	if (!refcount_dec_and_test(r)) {
		up_write(sem);
		return false;
	}

	return true;
}

static inline int kref_put_rwsem_lock(struct kref *kref,
				      void (*release)(struct kref *kref),
				      struct rw_semaphore *sem)
{
	if (refcount_dec_and_rwsem_lock(&kref->refcount, sem)) {
		release(kref);
		return 1;
	}
	return 0;
}

/* Release node resources and free the node.
 *
 * Do not call directly, use qrtr_node_release.  To be used with
 * kref_put_mutex.  As such, the node mutex is expected to be locked on call.
 */
static void __qrtr_node_release(struct kref *kref)
{
	struct qrtr_tx_flow_waiter *waiter;
	struct qrtr_tx_flow_waiter *temp;
	struct radix_tree_iter iter;
	struct qrtr_tx_flow *flow;
	struct qrtr_node *node = container_of(kref, struct qrtr_node, ref);
	void __rcu **slot;

	if (node->nid != QRTR_EP_NID_AUTO) {
		radix_tree_for_each_slot(slot, &qrtr_nodes, &iter, 0) {
			if (node == *slot)
				radix_tree_delete(&qrtr_nodes, iter.index);
		}
	}

	list_del(&node->item);
	up_write(&qrtr_node_lock);

	/* Free tx flow counters */
	mutex_lock(&node->qrtr_tx_lock);
	radix_tree_for_each_slot(slot, &node->qrtr_tx_flow, &iter, 0) {
		flow = *slot;
		list_for_each_entry_safe(waiter, temp, &flow->waiters, node) {
			list_del(&waiter->node);
			sock_put(waiter->sk);
			kfree(waiter);
		}
		kfree(flow);
		radix_tree_delete(&node->qrtr_tx_flow, iter.index);
	}
	mutex_unlock(&node->qrtr_tx_lock);

	kthread_flush_worker(&node->kworker);
	kthread_stop(node->task);

	skb_queue_purge(&node->rx_queue);
	kfree(node);
}

/* Increment reference to node. */
static struct qrtr_node *qrtr_node_acquire(struct qrtr_node *node)
{
	if (node)
		kref_get(&node->ref);
	return node;
}

/* Decrement reference to node and release as necessary. */
static void qrtr_node_release(struct qrtr_node *node)
{
	if (!node)
		return;
	kref_put_rwsem_lock(&node->ref, __qrtr_node_release, &qrtr_node_lock);
}

/**
 * qrtr_tx_resume() - reset flow control counter
 * @node:	qrtr_node that the QRTR_TYPE_RESUME_TX packet arrived on
 * @skb:	skb for resume tx control packet
 */
static void qrtr_tx_resume(struct qrtr_node *node, struct sk_buff *skb)
{
	struct qrtr_tx_flow_waiter *waiter;
	struct qrtr_tx_flow_waiter *temp;
	struct qrtr_ctrl_pkt *pkt;
	struct qrtr_tx_flow *flow;
	struct sockaddr_qrtr src;
	struct qrtr_sock *ipc;
	struct sk_buff *skbn;
	unsigned long key;

	pkt = (struct qrtr_ctrl_pkt *)skb->data;
	if (le32_to_cpu(pkt->cmd) != QRTR_TYPE_RESUME_TX)
		return;

	src.sq_family = AF_QIPCRTR;
	src.sq_node = le32_to_cpu(pkt->client.node);
	src.sq_port = le32_to_cpu(pkt->client.port);
	key = (u64)src.sq_node << 32 | src.sq_port;

	flow = radix_tree_lookup(&node->qrtr_tx_flow, key);
	if (!flow)
		return;

	mutex_lock(&node->qrtr_tx_lock);
	atomic_set(&flow->pending, 0);
	wake_up_interruptible_all(&node->resume_tx);

	list_for_each_entry_safe(waiter, temp, &flow->waiters, node) {
		list_del(&waiter->node);
		skbn = alloc_skb(0, GFP_KERNEL);
		if (skbn) {
			ipc = qrtr_sk(waiter->sk);
			qrtr_local_enqueue(NULL, skbn, QRTR_TYPE_RESUME_TX,
					   &src, &ipc->us, 0);
		}
		sock_put(waiter->sk);
		kfree(waiter);
	}
	mutex_unlock(&node->qrtr_tx_lock);
}

/**
 * qrtr_tx_wait() - flow control for outgoing packets
 * @node:	qrtr_node that the packet is to be send to
 * @dest_node:	node id of the destination
 * @dest_port:	port number of the destination
 * @type:	type of message
 *
 * The flow control scheme is based around the low and high "watermarks". When
 * the low watermark is passed the confirm_rx flag is set on the outgoing
 * message, which will trigger the remote to send a control message of the type
 * QRTR_TYPE_RESUME_TX to reset the counter. If the high watermark is hit
 * further transmision should be paused.
 *
 * Return: 1 if confirm_rx should be set, 0 otherwise or errno failure
 */
static int qrtr_tx_wait(struct qrtr_node *node, struct sockaddr_qrtr *to,
			struct sock *sk, int type, unsigned int flags)
{
	struct qrtr_tx_flow_waiter *waiter;
	struct qrtr_tx_flow *flow;
	unsigned long key = (u64)to->sq_node << 32 | to->sq_port;
	int confirm_rx = 0;
	long timeo;
	long ret;

	/* Never set confirm_rx on non-data packets */
	if (type != QRTR_TYPE_DATA)
		return 0;

	/* Assume sk is set correctly for all data type packets */
	timeo = sock_sndtimeo(sk, flags & MSG_DONTWAIT);

	mutex_lock(&node->qrtr_tx_lock);
	flow = radix_tree_lookup(&node->qrtr_tx_flow, key);
	if (!flow) {
		flow = kzalloc(sizeof(*flow), GFP_KERNEL);
		if (!flow)
			return 1;
		INIT_LIST_HEAD(&flow->waiters);
		radix_tree_insert(&node->qrtr_tx_flow, key, flow);
	}
	mutex_unlock(&node->qrtr_tx_lock);

	ret = timeo;
	for (;;) {
		mutex_lock(&node->qrtr_tx_lock);
		if (atomic_read(&flow->pending) < QRTR_TX_FLOW_HIGH) {
			atomic_inc(&flow->pending);
			confirm_rx = atomic_read(&flow->pending) ==
				     QRTR_TX_FLOW_LOW;
			mutex_unlock(&node->qrtr_tx_lock);
			break;
		}
		if (!ret) {
			waiter = kzalloc(sizeof(*waiter), GFP_KERNEL);
			if (!waiter) {
				mutex_unlock(&node->qrtr_tx_lock);
				return -ENOMEM;
			}
			waiter->sk = sk;
			sock_hold(sk);
			list_add_tail(&waiter->node, &flow->waiters);
			mutex_unlock(&node->qrtr_tx_lock);
			return -EAGAIN;
		}
		mutex_unlock(&node->qrtr_tx_lock);

		ret = wait_event_interruptible_timeout(
				node->resume_tx,
				!node->ep ||
				atomic_read(&flow->pending) < QRTR_TX_FLOW_HIGH,
				timeo);
		if (ret < 0)
			return ret;
		if (!node->ep)
			return -EPIPE;
	}
	return confirm_rx;
}

/* Pass an outgoing packet socket buffer to the endpoint driver. */
static int qrtr_node_enqueue(struct qrtr_node *node, struct sk_buff *skb,
			     int type, struct sockaddr_qrtr *from,
			     struct sockaddr_qrtr *to, unsigned int flags)
{
	struct qrtr_hdr_v1 *hdr;
	int confirm_rx;
	size_t len = skb->len;
	int rc = -ENODEV;

	if (!atomic_read(&node->hello_sent) && type != QRTR_TYPE_HELLO) {
		kfree_skb(skb);
		return rc;
	}

	/* If sk is null, this is a forwarded packet and should not wait */
	if (!skb->sk) {
		struct qrtr_cb *cb = (struct qrtr_cb *)skb->cb;

		confirm_rx = cb->confirm_rx;
	} else {
		confirm_rx = qrtr_tx_wait(node, to, skb->sk, type, flags);
		if (confirm_rx < 0) {
			kfree_skb(skb);
			return confirm_rx;
		}
	}

	hdr = skb_push(skb, sizeof(*hdr));
	hdr->version = cpu_to_le32(QRTR_PROTO_VER_1);
	hdr->type = cpu_to_le32(type);
	hdr->src_node_id = cpu_to_le32(from->sq_node);
	hdr->src_port_id = cpu_to_le32(from->sq_port);
	if (to->sq_node == QRTR_NODE_BCAST)
		hdr->dst_node_id = cpu_to_le32(node->nid);
	else
		hdr->dst_node_id = cpu_to_le32(to->sq_node);

	hdr->dst_port_id = cpu_to_le32(to->sq_port);
	hdr->size = cpu_to_le32(len);
	hdr->confirm_rx = !!confirm_rx;

	skb_put_padto(skb, ALIGN(len, 4) + sizeof(*hdr));
	qrtr_log_tx_msg(node, hdr, skb);

	mutex_lock(&node->ep_lock);
	if (node->ep)
		rc = node->ep->xmit(node->ep, skb);
	else
		kfree_skb(skb);
	mutex_unlock(&node->ep_lock);

	if (!rc && type == QRTR_TYPE_HELLO)
		atomic_inc(&node->hello_sent);

	if (rc) {
		struct qrtr_tx_flow *flow;
		unsigned long key = (u64)to->sq_node << 32 | to->sq_port;

		mutex_lock(&node->qrtr_tx_lock);
		flow = radix_tree_lookup(&node->qrtr_tx_flow, key);
		if (flow)
			atomic_dec(&flow->pending);
		mutex_unlock(&node->qrtr_tx_lock);
	}

	return rc;
}

/* Lookup node by id.
 *
 * callers must release with qrtr_node_release()
 */
static struct qrtr_node *qrtr_node_lookup(unsigned int nid)
{
	struct qrtr_node *node;

	down_read(&qrtr_node_lock);
	node = radix_tree_lookup(&qrtr_nodes, nid);
	node = qrtr_node_acquire(node);
	up_read(&qrtr_node_lock);

	return node;
}

/* Assign node id to node.
 *
 * This is mostly useful for automatic node id assignment, based on
 * the source id in the incoming packet.
 */
static void qrtr_node_assign(struct qrtr_node *node, unsigned int nid)
{
	struct qrtr_node *tnode = NULL;
	char name[32] = {0,};

	if (nid == QRTR_EP_NID_AUTO)
		return;
	if (nid == node->nid)
		return;

	down_read(&qrtr_node_lock);
	tnode = radix_tree_lookup(&qrtr_nodes, nid);
	up_read(&qrtr_node_lock);
	if (tnode)
		return;

	down_write(&qrtr_node_lock);
	radix_tree_insert(&qrtr_nodes, nid, node);

	if (node->nid == QRTR_EP_NID_AUTO)
		node->nid = nid;
	up_write(&qrtr_node_lock);

	if (!node->ilc) {
		snprintf(name, sizeof(name), "qrtr_%d", nid);
		node->ilc = ipc_log_context_create(QRTR_LOG_PAGE_CNT, name, 0);
	}
}

/**
 * qrtr_peek_pkt_size() - Peek into the packet header to get potential pkt size
 *
 * @data: Starting address of the packet which points to router header.
 *
 * @returns: potential packet size on success, < 0 on error.
 *
 * This function is used by the underlying transport abstraction layer to
 * peek into the potential packet size of an incoming packet. This information
 * is used to perform link layer fragmentation and re-assembly
 */
int qrtr_peek_pkt_size(const void *data)
{
	const struct qrtr_hdr_v1 *v1;
	const struct qrtr_hdr_v2 *v2;
	unsigned int hdrlen;
	unsigned int size;
	unsigned int ver;

	/* Version field in v1 is little endian, so this works for both cases */
	ver = *(u8 *)data;

	switch (ver) {
	case QRTR_PROTO_VER_1:
		v1 = data;
		hdrlen = sizeof(*v1);
		size = le32_to_cpu(v1->size);
		break;
	case QRTR_PROTO_VER_2:
		v2 = data;
		hdrlen = sizeof(*v2) + v2->optlen;
		size = le32_to_cpu(v2->size);
		break;
	default:
		pr_err("qrtr: Invalid version %d\n", ver);
		return -EINVAL;
	}

	return ALIGN(size, 4) + hdrlen;
}
EXPORT_SYMBOL(qrtr_peek_pkt_size);

/**
 * qrtr_endpoint_post() - post incoming data
 * @ep: endpoint handle
 * @data: data pointer
 * @len: size of data in bytes
 *
 * Return: 0 on success; negative error code on failure
 */
int qrtr_endpoint_post(struct qrtr_endpoint *ep, const void *data, size_t len)
{
	struct qrtr_node *node = ep->node;
	const struct qrtr_hdr_v1 *v1;
	const struct qrtr_hdr_v2 *v2;
	struct sk_buff *skb;
	struct qrtr_cb *cb;
	unsigned int size;
	unsigned int ver;
	size_t hdrlen;

	if (len & 3)
		return -EINVAL;

	skb = netdev_alloc_skb(NULL, len);
	if (!skb)
		return -ENOMEM;

	cb = (struct qrtr_cb *)skb->cb;

	/* Version field in v1 is little endian, so this works for both cases */
	ver = *(u8 *)data;

	switch (ver) {
	case QRTR_PROTO_VER_1:
		v1 = data;
		hdrlen = sizeof(*v1);

		cb->type = le32_to_cpu(v1->type);
		cb->src_node = le32_to_cpu(v1->src_node_id);
		cb->src_port = le32_to_cpu(v1->src_port_id);
		cb->confirm_rx = !!v1->confirm_rx;
		cb->dst_node = le32_to_cpu(v1->dst_node_id);
		cb->dst_port = le32_to_cpu(v1->dst_port_id);

		size = le32_to_cpu(v1->size);
		break;
	case QRTR_PROTO_VER_2:
		v2 = data;
		hdrlen = sizeof(*v2) + v2->optlen;

		cb->type = v2->type;
		cb->confirm_rx = !!(v2->flags & QRTR_FLAGS_CONFIRM_RX);
		cb->src_node = le16_to_cpu(v2->src_node_id);
		cb->src_port = le16_to_cpu(v2->src_port_id);
		cb->dst_node = le16_to_cpu(v2->dst_node_id);
		cb->dst_port = le16_to_cpu(v2->dst_port_id);

		if (cb->src_port == (u16)QRTR_PORT_CTRL)
			cb->src_port = QRTR_PORT_CTRL;
		if (cb->dst_port == (u16)QRTR_PORT_CTRL)
			cb->dst_port = QRTR_PORT_CTRL;

		size = le32_to_cpu(v2->size);
		break;
	default:
		pr_err("qrtr: Invalid version %d\n", ver);
		goto err;
	}

	if (cb->dst_port == QRTR_PORT_CTRL_LEGACY)
		cb->dst_port = QRTR_PORT_CTRL;

	if (len != ALIGN(size, 4) + hdrlen)
		goto err;

	if (cb->dst_port != QRTR_PORT_CTRL && cb->type != QRTR_TYPE_DATA &&
	    cb->type != QRTR_TYPE_RESUME_TX)
		goto err;

	skb_put_data(skb, data + hdrlen, size);
	qrtr_log_rx_msg(node, skb);

	skb_queue_tail(&node->rx_queue, skb);
	kthread_queue_work(&node->kworker, &node->read_data);

	return 0;

err:
	kfree_skb(skb);
	return -EINVAL;

}
EXPORT_SYMBOL_GPL(qrtr_endpoint_post);

/**
 * qrtr_alloc_ctrl_packet() - allocate control packet skb
 * @pkt: reference to qrtr_ctrl_pkt pointer
 *
 * Returns newly allocated sk_buff, or NULL on failure
 *
 * This function allocates a sk_buff large enough to carry a qrtr_ctrl_pkt and
 * on success returns a reference to the control packet in @pkt.
 */
static struct sk_buff *qrtr_alloc_ctrl_packet(struct qrtr_ctrl_pkt **pkt)
{
	const int pkt_len = sizeof(struct qrtr_ctrl_pkt);
	struct sk_buff *skb;

	skb = alloc_skb(QRTR_HDR_MAX_SIZE + pkt_len, GFP_KERNEL);
	if (!skb)
		return NULL;

	skb_reserve(skb, QRTR_HDR_MAX_SIZE);
	*pkt = skb_put_zero(skb, pkt_len);

	return skb;
}

static struct qrtr_sock *qrtr_port_lookup(int port);
static void qrtr_port_put(struct qrtr_sock *ipc);

static bool qrtr_must_forward(struct qrtr_node *src,
			      struct qrtr_node *dst, u32 type)
{
	/* Node structure is not maintained for local processor.
	 * Hence src is null in that case.
	 */
	if (!src)
		return true;

	if (!dst)
		return false;

	if (type == QRTR_TYPE_HELLO || type == QRTR_TYPE_RESUME_TX)
		return false;

	if (dst == src || dst->nid == QRTR_EP_NID_AUTO)
		return false;

	if (abs(dst->net_id - src->net_id) > 1)
		return true;

	return false;
}

static void qrtr_fwd_ctrl_pkt(struct sk_buff *skb)
{
	struct qrtr_node *node;
	struct qrtr_node *src;
	struct qrtr_cb *cb = (struct qrtr_cb *)skb->cb;

	src = qrtr_node_lookup(cb->src_node);
	down_read(&qrtr_node_lock);
	list_for_each_entry(node, &qrtr_all_epts, item) {
		struct sockaddr_qrtr from;
		struct sockaddr_qrtr to;
		struct sk_buff *skbn;

		if (!qrtr_must_forward(src, node, cb->type))
			continue;

		skbn = skb_clone(skb, GFP_KERNEL);
		if (!skbn)
			break;

		from.sq_family = AF_QIPCRTR;
		from.sq_node = cb->src_node;
		from.sq_port = cb->src_port;

		to.sq_family = AF_QIPCRTR;
		to.sq_node = node->nid;
		to.sq_port = QRTR_PORT_CTRL;

		qrtr_node_enqueue(node, skbn, cb->type, &from, &to, 0);
	}
	up_read(&qrtr_node_lock);
	qrtr_node_release(src);
}

static void qrtr_fwd_pkt(struct sk_buff *skb, struct qrtr_cb *cb)
{
	struct sockaddr_qrtr from = {AF_QIPCRTR, cb->src_node, cb->src_port};
	struct sockaddr_qrtr to = {AF_QIPCRTR, cb->dst_node, cb->dst_port};
	struct qrtr_node *node;

	node = qrtr_node_lookup(cb->dst_node);
	if (!node)
		return;

	qrtr_node_enqueue(node, skb, cb->type, &from, &to, 0);
	qrtr_node_release(node);
}
/* Handle and route a received packet.
 *
 * This will auto-reply with resume-tx packet as necessary.
 */
static void qrtr_node_rx_work(struct kthread_work *work)
{
	struct qrtr_node *node = container_of(work, struct qrtr_node,
					      read_data);
	struct qrtr_ctrl_pkt *pkt;
	struct sk_buff *skb;

	while ((skb = skb_dequeue(&node->rx_queue)) != NULL) {
		struct qrtr_sock *ipc;
		struct qrtr_cb *cb;

		cb = (struct qrtr_cb *)skb->cb;
		qrtr_node_assign(node, cb->src_node);

		if (cb->type != QRTR_TYPE_DATA)
			qrtr_fwd_ctrl_pkt(skb);

		if (cb->type == QRTR_TYPE_NEW_SERVER &&
		    skb->len == sizeof(*pkt)) {
			pkt = (void *)skb->data;
			qrtr_node_assign(node, le32_to_cpu(pkt->server.node));
		}

		if (cb->type == QRTR_TYPE_RESUME_TX) {
			if (cb->dst_node != qrtr_local_nid) {
				qrtr_fwd_pkt(skb, cb);
				continue;
			}
			qrtr_tx_resume(node, skb);
			consume_skb(skb);
		} else if (cb->dst_node != qrtr_local_nid &&
			   cb->type == QRTR_TYPE_DATA) {
			qrtr_fwd_pkt(skb, cb);
		} else {
			ipc = qrtr_port_lookup(cb->dst_port);
			if (!ipc) {
				kfree_skb(skb);
			} else {
				if (sock_queue_rcv_skb(&ipc->sk, skb))
					kfree_skb(skb);

				qrtr_port_put(ipc);
			}
		}
	}
}

/**
 * qrtr_endpoint_register() - register a new endpoint
 * @ep: endpoint to register
 * @nid: desired node id; may be QRTR_EP_NID_AUTO for auto-assignment
 * Return: 0 on success; negative error code on failure
 *
 * The specified endpoint must have the xmit function pointer set on call.
 */
int qrtr_endpoint_register(struct qrtr_endpoint *ep, unsigned int net_id)
{
	struct qrtr_node *node;

	if (!ep || !ep->xmit)
		return -EINVAL;

	node = kzalloc(sizeof(*node), GFP_KERNEL);
	if (!node)
		return -ENOMEM;

	kref_init(&node->ref);
	mutex_init(&node->ep_lock);
	skb_queue_head_init(&node->rx_queue);
	node->nid = QRTR_EP_NID_AUTO;
	node->ep = ep;
	atomic_set(&node->hello_sent, 0);

	kthread_init_work(&node->read_data, qrtr_node_rx_work);
	kthread_init_worker(&node->kworker);
	node->task = kthread_run(kthread_worker_fn, &node->kworker, "qrtr_rx");
	if (IS_ERR(node->task)) {
		kfree(node);
		return -ENOMEM;
	}

	mutex_init(&node->qrtr_tx_lock);
	INIT_RADIX_TREE(&node->qrtr_tx_flow, GFP_KERNEL);
	init_waitqueue_head(&node->resume_tx);

	qrtr_node_assign(node, node->nid);
	node->net_id = net_id;

	down_write(&qrtr_node_lock);
	list_add(&node->item, &qrtr_all_epts);
	up_write(&qrtr_node_lock);
	ep->node = node;

	return 0;
}
EXPORT_SYMBOL_GPL(qrtr_endpoint_register);

static u32 qrtr_calc_checksum(struct qrtr_ctrl_pkt *pkt)
{
	u32 checksum = 0;
	u32 mask = 0xffff;
	u16 upper_nb;
	u16 lower_nb;
	u32 *msg;
	int i;

	if (!pkt)
		return checksum;
	msg = (u32 *)pkt;

	for (i = 0; i < sizeof(*pkt) / sizeof(*msg); i++) {
		lower_nb = *msg & mask;
		upper_nb = (*msg >> 16) & mask;
		checksum += (upper_nb + lower_nb);
		msg++;
	}
	while (checksum > 0xffff)
		checksum = (checksum & mask) + ((checksum >> 16) & mask);

	checksum = ~checksum & mask;

	return checksum;
}

static void qrtr_fwd_del_proc(struct qrtr_node *src, unsigned int nid)
{
	struct sockaddr_qrtr from = {AF_QIPCRTR, 0, QRTR_PORT_CTRL};
	struct sockaddr_qrtr to = {AF_QIPCRTR, 0, QRTR_PORT_CTRL};
	struct qrtr_ctrl_pkt *pkt;
	struct qrtr_node *dst;
	struct sk_buff *skb;

	list_for_each_entry(dst, &qrtr_all_epts, item) {
		if (!qrtr_must_forward(src, dst, QRTR_TYPE_DEL_PROC))
			continue;

		skb = qrtr_alloc_ctrl_packet(&pkt);
		if (!skb)
			return;

		pkt->cmd = cpu_to_le32(QRTR_TYPE_DEL_PROC);
		pkt->proc.rsvd = QRTR_DEL_PROC_MAGIC;
		pkt->proc.node = cpu_to_le32(nid);
		pkt->proc.rsvd = cpu_to_le32(qrtr_calc_checksum(pkt));

		from.sq_node = src->nid;
		to.sq_node = dst->nid;
		qrtr_node_enqueue(dst, skb, QRTR_TYPE_DEL_PROC, &from, &to, 0);
	}
}

/**
 * qrtr_endpoint_unregister - unregister endpoint
 * @ep: endpoint to unregister
 */
void qrtr_endpoint_unregister(struct qrtr_endpoint *ep)
{
	struct radix_tree_iter iter;
	struct qrtr_node *node = ep->node;
	struct sockaddr_qrtr src = {AF_QIPCRTR, node->nid, QRTR_PORT_CTRL};
	struct sockaddr_qrtr dst = {AF_QIPCRTR, qrtr_local_nid, QRTR_PORT_CTRL};
	struct qrtr_ctrl_pkt *pkt;
	struct sk_buff *skb;
	void __rcu **slot;

	mutex_lock(&node->ep_lock);
	node->ep = NULL;
	mutex_unlock(&node->ep_lock);

	/* Notify the local controller about the event */
	down_read(&qrtr_node_lock);
	radix_tree_for_each_slot(slot, &qrtr_nodes, &iter, 0) {
		if (node != *slot)
			continue;

		skb = qrtr_alloc_ctrl_packet(&pkt);
		if (!skb)
			continue;

		src.sq_node = iter.index;
		pkt->cmd = cpu_to_le32(QRTR_TYPE_BYE);
		qrtr_local_enqueue(NULL, skb, QRTR_TYPE_BYE, &src, &dst, 0);

		qrtr_fwd_del_proc(node, iter.index);
	}
	up_read(&qrtr_node_lock);

	/* Wake up any transmitters waiting for resume-tx from the node */
	wake_up_interruptible_all(&node->resume_tx);

	qrtr_node_release(node);
	ep->node = NULL;
}
EXPORT_SYMBOL_GPL(qrtr_endpoint_unregister);

/* Lookup socket by port.
 *
 * Callers must release with qrtr_port_put()
 */
static struct qrtr_sock *qrtr_port_lookup(int port)
{
	struct qrtr_sock *ipc;

	if (port == QRTR_PORT_CTRL)
		port = 0;

	mutex_lock(&qrtr_port_lock);
	ipc = idr_find(&qrtr_ports, port);
	if (ipc)
		sock_hold(&ipc->sk);
	mutex_unlock(&qrtr_port_lock);

	return ipc;
}

/* Release acquired socket. */
static void qrtr_port_put(struct qrtr_sock *ipc)
{
	sock_put(&ipc->sk);
}

static void qrtr_send_del_client(struct qrtr_sock *ipc)
{
	struct qrtr_ctrl_pkt *pkt;
	struct sockaddr_qrtr to;
	struct qrtr_node *node;
	struct sk_buff *skbn;
	struct sk_buff *skb;
	int type = QRTR_TYPE_DEL_CLIENT;

	skb = qrtr_alloc_ctrl_packet(&pkt);
	if (!skb)
		return;

	to.sq_family = AF_QIPCRTR;
	to.sq_node = QRTR_NODE_BCAST;
	to.sq_port = QRTR_PORT_CTRL;

	pkt->cmd = cpu_to_le32(QRTR_TYPE_DEL_CLIENT);
	pkt->client.node = cpu_to_le32(ipc->us.sq_node);
	pkt->client.port = cpu_to_le32(ipc->us.sq_port);

	skb_set_owner_w(skb, &ipc->sk);

	if (ipc->state == QRTR_STATE_MULTI) {
		qrtr_bcast_enqueue(NULL, skb, type, &ipc->us, &to, 0);
		return;
	}

	if (ipc->state > QRTR_STATE_INIT) {
		node = qrtr_node_lookup(ipc->state);
		if (!node)
			goto exit;

		skbn = skb_clone(skb, GFP_KERNEL);
		if (!skbn) {
			qrtr_node_release(node);
			goto exit;
		}

		skb_set_owner_w(skbn, &ipc->sk);
		qrtr_node_enqueue(node, skbn, type, &ipc->us, &to, 0);
		qrtr_node_release(node);
	}
exit:
	qrtr_local_enqueue(NULL, skb, type, &ipc->us, &to, 0);
}

/* Remove port assignment. */
static void qrtr_port_remove(struct qrtr_sock *ipc)
{
	int port = ipc->us.sq_port;

	qrtr_send_del_client(ipc);
	if (port == QRTR_PORT_CTRL)
		port = 0;

	__sock_put(&ipc->sk);

	mutex_lock(&qrtr_port_lock);
	idr_remove(&qrtr_ports, port);
	mutex_unlock(&qrtr_port_lock);
}

/* Assign port number to socket.
 *
 * Specify port in the integer pointed to by port, and it will be adjusted
 * on return as necesssary.
 *
 * Port may be:
 *   0: Assign ephemeral port in [QRTR_MIN_EPH_SOCKET, QRTR_MAX_EPH_SOCKET]
 *   <QRTR_MIN_EPH_SOCKET: Specified; requires CAP_NET_ADMIN
 *   >QRTR_MIN_EPH_SOCKET: Specified; available to all
 */
static int qrtr_port_assign(struct qrtr_sock *ipc, int *port)
{
	int rc;

	if (!*port) {
		rc = idr_alloc_cyclic(&qrtr_ports, ipc, QRTR_MIN_EPH_SOCKET,
				      QRTR_MAX_EPH_SOCKET + 1, GFP_ATOMIC);
		if (rc >= 0)
			*port = rc;
	} else if (*port < QRTR_MIN_EPH_SOCKET &&
		   !(capable(CAP_NET_ADMIN) ||
		   in_egroup_p(AID_VENDOR_QRTR) ||
		   in_egroup_p(GLOBAL_ROOT_GID))) {
		rc = -EACCES;
	} else if (*port == QRTR_PORT_CTRL) {
		rc = idr_alloc(&qrtr_ports, ipc, 0, 1, GFP_ATOMIC);
	} else {
		rc = idr_alloc_cyclic(&qrtr_ports, ipc, *port, *port + 1,
				      GFP_ATOMIC);
		if (rc >= 0)
			*port = rc;
	}

	if (rc == -ENOSPC)
		return -EADDRINUSE;
	else if (rc < 0)
		return rc;

	sock_hold(&ipc->sk);

	return 0;
}

/* Reset all non-control ports */
static void qrtr_reset_ports(void)
{
	struct qrtr_sock *ipc;
	int id;

	idr_for_each_entry(&qrtr_ports, ipc, id) {
		/* Don't reset control port */
		if (id == 0)
			continue;

		sock_hold(&ipc->sk);
		ipc->sk.sk_err = ENETRESET;
		if (ipc->sk.sk_error_report)
			ipc->sk.sk_error_report(&ipc->sk);
		sock_put(&ipc->sk);
	}
}

/* Bind socket to address.
 *
 * Socket should be locked upon call.
 */
static int __qrtr_bind(struct socket *sock,
		       const struct sockaddr_qrtr *addr, int zapped)
{
	struct qrtr_sock *ipc = qrtr_sk(sock->sk);
	struct sock *sk = sock->sk;
	int port;
	int rc;

	/* rebinding ok */
	if (!zapped && addr->sq_port == ipc->us.sq_port)
		return 0;

	mutex_lock(&qrtr_port_lock);
	port = addr->sq_port;
	rc = qrtr_port_assign(ipc, &port);
	if (rc) {
		mutex_unlock(&qrtr_port_lock);
		return rc;
	}
	/* Notify all open ports about the new controller */
	if (port == QRTR_PORT_CTRL)
		qrtr_reset_ports();
	mutex_unlock(&qrtr_port_lock);

	/* unbind previous, if any */
	if (!zapped)
		qrtr_port_remove(ipc);
	ipc->us.sq_port = port;
	sock_reset_flag(sk, SOCK_ZAPPED);

	return 0;
}

/* Auto bind to an ephemeral port. */
static int qrtr_autobind(struct socket *sock)
{
	struct sock *sk = sock->sk;
	struct sockaddr_qrtr addr;

	if (!sock_flag(sk, SOCK_ZAPPED))
		return 0;

	addr.sq_family = AF_QIPCRTR;
	addr.sq_node = qrtr_local_nid;
	addr.sq_port = 0;

	return __qrtr_bind(sock, &addr, 1);
}

/* Bind socket to specified sockaddr. */
static int qrtr_bind(struct socket *sock, struct sockaddr *saddr, int len)
{
	DECLARE_SOCKADDR(struct sockaddr_qrtr *, addr, saddr);
	struct qrtr_sock *ipc = qrtr_sk(sock->sk);
	struct sock *sk = sock->sk;
	int rc;

	if (len < sizeof(*addr) || addr->sq_family != AF_QIPCRTR)
		return -EINVAL;

	if (addr->sq_node != ipc->us.sq_node)
		return -EINVAL;

	lock_sock(sk);
	rc = __qrtr_bind(sock, addr, sock_flag(sk, SOCK_ZAPPED));
	release_sock(sk);

	return rc;
}

/* Queue packet to local peer socket. */
static int qrtr_local_enqueue(struct qrtr_node *node, struct sk_buff *skb,
			      int type, struct sockaddr_qrtr *from,
			      struct sockaddr_qrtr *to, unsigned int flags)
{
	struct qrtr_sock *ipc;
	struct qrtr_cb *cb;
	struct sock *sk = skb->sk;

	ipc = qrtr_port_lookup(to->sq_port);
	if (!ipc && to->sq_port == QRTR_PORT_CTRL) {
		kfree_skb(skb);
		return 0;
	}
	if (!ipc || &ipc->sk == skb->sk) { /* do not send to self */
		kfree_skb(skb);
		return -ENODEV;
	}
	/* Keep resetting NETRESET until socket is closed */
	if (sk && sk->sk_err == ENETRESET) {
		sock_hold(sk);
		sk->sk_err = ENETRESET;
		if (sk->sk_error_report)
			sk->sk_error_report(sk);
		sock_put(sk);
		kfree_skb(skb);
		return 0;
	}

	cb = (struct qrtr_cb *)skb->cb;
	cb->src_node = from->sq_node;
	cb->src_port = from->sq_port;

	if (sock_queue_rcv_skb(&ipc->sk, skb)) {
		qrtr_port_put(ipc);
		kfree_skb(skb);
		return -ENOSPC;
	}

	qrtr_port_put(ipc);

	return 0;
}

/* Queue packet for broadcast. */
static int qrtr_bcast_enqueue(struct qrtr_node *node, struct sk_buff *skb,
			      int type, struct sockaddr_qrtr *from,
			      struct sockaddr_qrtr *to, unsigned int flags)
{
	struct sk_buff *skbn;

	down_read(&qrtr_node_lock);
	list_for_each_entry(node, &qrtr_all_epts, item) {
		if (node->nid == QRTR_EP_NID_AUTO)
			continue;
		skbn = skb_clone(skb, GFP_KERNEL);
		if (!skbn)
			break;
		skb_set_owner_w(skbn, skb->sk);
		qrtr_node_enqueue(node, skbn, type, from, to, flags);
	}
	up_read(&qrtr_node_lock);

	qrtr_local_enqueue(node, skb, type, from, to, flags);

	return 0;
}

static int qrtr_sendmsg(struct socket *sock, struct msghdr *msg, size_t len)
{
	DECLARE_SOCKADDR(struct sockaddr_qrtr *, addr, msg->msg_name);
	int (*enqueue_fn)(struct qrtr_node *, struct sk_buff *, int,
			  struct sockaddr_qrtr *, struct sockaddr_qrtr *,
			  unsigned int);
	struct qrtr_sock *ipc = qrtr_sk(sock->sk);
	struct sock *sk = sock->sk;
	struct qrtr_ctrl_pkt *pkt;
	struct qrtr_node *node;
	struct qrtr_node *srv_node;
	struct sk_buff *skb;
	size_t plen;
	u32 type = QRTR_TYPE_DATA;
	int rc;

	if (msg->msg_flags & ~(MSG_DONTWAIT))
		return -EINVAL;

	if (len > 65535)
		return -EMSGSIZE;

	lock_sock(sk);

	if (addr) {
		if (msg->msg_namelen < sizeof(*addr)) {
			release_sock(sk);
			return -EINVAL;
		}

		if (addr->sq_family != AF_QIPCRTR) {
			release_sock(sk);
			return -EINVAL;
		}

		rc = qrtr_autobind(sock);
		if (rc) {
			release_sock(sk);
			return rc;
		}
	} else if (sk->sk_state == TCP_ESTABLISHED) {
		addr = &ipc->peer;
	} else {
		release_sock(sk);
		return -ENOTCONN;
	}

	node = NULL;
	srv_node = NULL;
	if (addr->sq_node == QRTR_NODE_BCAST) {
		enqueue_fn = qrtr_bcast_enqueue;
		if (addr->sq_port != QRTR_PORT_CTRL) {
			release_sock(sk);
			return -EINVAL;
		}
	} else if (addr->sq_node == ipc->us.sq_node) {
		enqueue_fn = qrtr_local_enqueue;
	} else {
		enqueue_fn = qrtr_node_enqueue;
		node = qrtr_node_lookup(addr->sq_node);
		if (!node) {
			release_sock(sk);
			return -ECONNRESET;
		}

		if (ipc->state > QRTR_STATE_INIT && ipc->state != node->nid)
			ipc->state = QRTR_STATE_MULTI;
		else if (ipc->state == QRTR_STATE_INIT)
			ipc->state = node->nid;
	}

	plen = (len + 3) & ~3;
	skb = sock_alloc_send_skb(sk, plen + QRTR_HDR_MAX_SIZE,
				  msg->msg_flags & MSG_DONTWAIT, &rc);
	if (!skb)
		goto out_node;

	skb_reserve(skb, QRTR_HDR_MAX_SIZE);

	rc = memcpy_from_msg(skb_put(skb, len), msg, len);
	if (rc) {
		kfree_skb(skb);
		goto out_node;
	}

	if (ipc->us.sq_port == QRTR_PORT_CTRL ||
	    addr->sq_port == QRTR_PORT_CTRL) {
		if (len < 4) {
			rc = -EINVAL;
			kfree_skb(skb);
			goto out_node;
		}

		/* control messages already require the type as 'command' */
		skb_copy_bits(skb, 0, &type, 4);
		type = le32_to_cpu(type);
	}
	if (addr->sq_port == QRTR_PORT_CTRL && type == QRTR_TYPE_NEW_SERVER) {
		ipc->state = QRTR_STATE_MULTI;

		/* drop new server cmds that are not forwardable to dst node*/
		pkt = (struct qrtr_ctrl_pkt *)skb->data;
		srv_node = qrtr_node_lookup(pkt->server.node);
		if (!qrtr_must_forward(srv_node, node, type)) {
			rc = 0;
			kfree_skb(skb);
			qrtr_node_release(srv_node);
			goto out_node;
		}
		qrtr_node_release(srv_node);
	}

	rc = enqueue_fn(node, skb, type, &ipc->us, addr, msg->msg_flags);
	if (rc >= 0)
		rc = len;

out_node:
	qrtr_node_release(node);
	release_sock(sk);

	return rc;
}

static int qrtr_resume_tx(struct qrtr_cb *cb)
{
	struct sockaddr_qrtr remote = { AF_QIPCRTR,
					cb->src_node, cb->src_port };
	struct sockaddr_qrtr local = { AF_QIPCRTR, cb->dst_node, cb->dst_port };
	struct qrtr_ctrl_pkt *pkt;
	struct qrtr_node *node;
	struct sk_buff *skb;
	int ret;

	node = qrtr_node_lookup(remote.sq_node);
	if (!node)
		return -EINVAL;

	skb = qrtr_alloc_ctrl_packet(&pkt);
	if (!skb)
		return -ENOMEM;

	pkt->cmd = cpu_to_le32(QRTR_TYPE_RESUME_TX);
	pkt->client.node = cpu_to_le32(cb->dst_node);
	pkt->client.port = cpu_to_le32(cb->dst_port);

	ret = qrtr_node_enqueue(node, skb, QRTR_TYPE_RESUME_TX,
				&local, &remote, 0);

	qrtr_node_release(node);

	return ret;
}

static int qrtr_recvmsg(struct socket *sock, struct msghdr *msg,
			size_t size, int flags)
{
	DECLARE_SOCKADDR(struct sockaddr_qrtr *, addr, msg->msg_name);
	struct sock *sk = sock->sk;
	struct sk_buff *skb;
	struct qrtr_cb *cb;
	int copied, rc;

	lock_sock(sk);

	if (sock_flag(sk, SOCK_ZAPPED)) {
		release_sock(sk);
		return -EADDRNOTAVAIL;
	}

	skb = skb_recv_datagram(sk, flags & ~MSG_DONTWAIT,
				flags & MSG_DONTWAIT, &rc);
	if (!skb) {
		release_sock(sk);
		return rc;
	}
	cb = (struct qrtr_cb *)skb->cb;

	copied = skb->len;
	if (copied > size) {
		copied = size;
		msg->msg_flags |= MSG_TRUNC;
	}

	rc = skb_copy_datagram_msg(skb, 0, msg, copied);
	if (rc < 0)
		goto out;
	rc = copied;

	if (addr) {
		addr->sq_family = AF_QIPCRTR;
		addr->sq_node = cb->src_node;
		addr->sq_port = cb->src_port;
		msg->msg_namelen = sizeof(*addr);
	}

out:
	if (cb->confirm_rx)
		qrtr_resume_tx(cb);

	skb_free_datagram(sk, skb);
	release_sock(sk);

	return rc;
}

static int qrtr_connect(struct socket *sock, struct sockaddr *saddr,
			int len, int flags)
{
	DECLARE_SOCKADDR(struct sockaddr_qrtr *, addr, saddr);
	struct qrtr_sock *ipc = qrtr_sk(sock->sk);
	struct sock *sk = sock->sk;
	int rc;

	if (len < sizeof(*addr) || addr->sq_family != AF_QIPCRTR)
		return -EINVAL;

	lock_sock(sk);

	sk->sk_state = TCP_CLOSE;
	sock->state = SS_UNCONNECTED;

	rc = qrtr_autobind(sock);
	if (rc) {
		release_sock(sk);
		return rc;
	}

	ipc->peer = *addr;
	sock->state = SS_CONNECTED;
	sk->sk_state = TCP_ESTABLISHED;

	release_sock(sk);

	return 0;
}

static int qrtr_getname(struct socket *sock, struct sockaddr *saddr,
			int *len, int peer)
{
	struct qrtr_sock *ipc = qrtr_sk(sock->sk);
	struct sockaddr_qrtr qaddr;
	struct sock *sk = sock->sk;

	lock_sock(sk);
	if (peer) {
		if (sk->sk_state != TCP_ESTABLISHED) {
			release_sock(sk);
			return -ENOTCONN;
		}

		qaddr = ipc->peer;
	} else {
		qaddr = ipc->us;
	}
	release_sock(sk);

	*len = sizeof(qaddr);
	qaddr.sq_family = AF_QIPCRTR;

	memcpy(saddr, &qaddr, sizeof(qaddr));

	return 0;
}

static int qrtr_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct qrtr_sock *ipc = qrtr_sk(sock->sk);
	struct sock *sk = sock->sk;
	struct sockaddr_qrtr *sq;
	struct sk_buff *skb;
	struct ifreq ifr;
	long len = 0;
	int rc = 0;

	lock_sock(sk);

	switch (cmd) {
	case TIOCOUTQ:
		len = sk->sk_sndbuf - sk_wmem_alloc_get(sk);
		if (len < 0)
			len = 0;
		rc = put_user(len, (int __user *)argp);
		break;
	case TIOCINQ:
		skb = skb_peek(&sk->sk_receive_queue);
		if (skb)
			len = skb->len;
		rc = put_user(len, (int __user *)argp);
		break;
	case SIOCGIFADDR:
		if (copy_from_user(&ifr, argp, sizeof(ifr))) {
			rc = -EFAULT;
			break;
		}

		sq = (struct sockaddr_qrtr *)&ifr.ifr_addr;
		*sq = ipc->us;
		if (copy_to_user(argp, &ifr, sizeof(ifr))) {
			rc = -EFAULT;
			break;
		}
		break;
	case SIOCGSTAMP:
		rc = sock_get_timestamp(sk, argp);
		break;
	case SIOCADDRT:
	case SIOCDELRT:
	case SIOCSIFADDR:
	case SIOCGIFDSTADDR:
	case SIOCSIFDSTADDR:
	case SIOCGIFBRDADDR:
	case SIOCSIFBRDADDR:
	case SIOCGIFNETMASK:
	case SIOCSIFNETMASK:
		rc = -EINVAL;
		break;
	default:
		rc = -ENOIOCTLCMD;
		break;
	}

	release_sock(sk);

	return rc;
}

static int qrtr_release(struct socket *sock)
{
	struct sock *sk = sock->sk;
	struct qrtr_sock *ipc;

	if (!sk)
		return 0;

	lock_sock(sk);

	ipc = qrtr_sk(sk);
	sk->sk_shutdown = SHUTDOWN_MASK;
	if (!sock_flag(sk, SOCK_DEAD))
		sk->sk_state_change(sk);

	sock_orphan(sk);
	sock->sk = NULL;

	if (!sock_flag(sk, SOCK_ZAPPED))
		qrtr_port_remove(ipc);

	skb_queue_purge(&sk->sk_receive_queue);

	release_sock(sk);
	sock_put(sk);

	return 0;
}

static const struct proto_ops qrtr_proto_ops = {
	.owner		= THIS_MODULE,
	.family		= AF_QIPCRTR,
	.bind		= qrtr_bind,
	.connect	= qrtr_connect,
	.socketpair	= sock_no_socketpair,
	.accept		= sock_no_accept,
	.listen		= sock_no_listen,
	.sendmsg	= qrtr_sendmsg,
	.recvmsg	= qrtr_recvmsg,
	.getname	= qrtr_getname,
	.ioctl		= qrtr_ioctl,
	.poll		= datagram_poll,
	.shutdown	= sock_no_shutdown,
	.setsockopt	= sock_no_setsockopt,
	.getsockopt	= sock_no_getsockopt,
	.release	= qrtr_release,
	.mmap		= sock_no_mmap,
	.sendpage	= sock_no_sendpage,
};

static struct proto qrtr_proto = {
	.name		= "QIPCRTR",
	.owner		= THIS_MODULE,
	.obj_size	= sizeof(struct qrtr_sock),
};

static int qrtr_create(struct net *net, struct socket *sock,
		       int protocol, int kern)
{
	struct qrtr_sock *ipc;
	struct sock *sk;

	if (sock->type != SOCK_DGRAM)
		return -EPROTOTYPE;

	sk = sk_alloc(net, AF_QIPCRTR, GFP_KERNEL, &qrtr_proto, kern);
	if (!sk)
		return -ENOMEM;

	sock_set_flag(sk, SOCK_ZAPPED);

	sock_init_data(sock, sk);
	sock->ops = &qrtr_proto_ops;

	ipc = qrtr_sk(sk);
	ipc->us.sq_family = AF_QIPCRTR;
	ipc->us.sq_node = qrtr_local_nid;
	ipc->us.sq_port = 0;
	ipc->state = QRTR_STATE_INIT;

	return 0;
}

static const struct nla_policy qrtr_policy[IFA_MAX + 1] = {
	[IFA_LOCAL] = { .type = NLA_U32 },
};

static int qrtr_addr_doit(struct sk_buff *skb, struct nlmsghdr *nlh,
			  struct netlink_ext_ack *extack)
{
	struct nlattr *tb[IFA_MAX + 1];
	struct ifaddrmsg *ifm;
	int rc;

	if (!netlink_capable(skb, CAP_NET_ADMIN))
		return -EPERM;

	ASSERT_RTNL();

	rc = nlmsg_parse(nlh, sizeof(*ifm), tb, IFA_MAX, qrtr_policy, extack);
	if (rc < 0)
		return rc;

	ifm = nlmsg_data(nlh);
	if (!tb[IFA_LOCAL])
		return -EINVAL;

	qrtr_local_nid = nla_get_u32(tb[IFA_LOCAL]);
	return 0;
}

static const struct net_proto_family qrtr_family = {
	.owner	= THIS_MODULE,
	.family	= AF_QIPCRTR,
	.create	= qrtr_create,
};

static int __init qrtr_proto_init(void)
{
	int rc;

	rc = proto_register(&qrtr_proto, 1);
	if (rc)
		return rc;

	rc = sock_register(&qrtr_family);
	if (rc) {
		proto_unregister(&qrtr_proto);
		return rc;
	}

	rtnl_register(PF_QIPCRTR, RTM_NEWADDR, qrtr_addr_doit, NULL, 0);

	return 0;
}
postcore_initcall(qrtr_proto_init);

static void __exit qrtr_proto_fini(void)
{
	rtnl_unregister(PF_QIPCRTR, RTM_NEWADDR);
	sock_unregister(qrtr_family.family);
	proto_unregister(&qrtr_proto);
}
module_exit(qrtr_proto_fini);

MODULE_DESCRIPTION("Qualcomm IPC-router driver");
MODULE_LICENSE("GPL v2");
