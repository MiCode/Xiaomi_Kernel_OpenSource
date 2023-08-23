/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2016-2017, Linaro Ltd
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

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/rpmsg.h>
#include <linux/idr.h>
#include <linux/sizes.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/workqueue.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/component.h>
#include <linux/ipc_logging.h>
#include "../soc/qcom/bgcom.h"

#include <linux/rpmsg/qcom_glink.h>

#include "rpmsg_internal.h"
#include "qcom_glink_native.h"

#define GLINK_LOG_PAGE_CNT 2
#define GLINK_INFO(ctxt, x, ...)					       \
do {									       \
	if (ctxt->ilc)							       \
		ipc_log_string(ctxt->ilc, "[%s]: "x, __func__, ##__VA_ARGS__); \
} while (0)

#define CH_INFO(ch, x, ...)						     \
do {									     \
	if (ch->glink && ch->glink->ilc)				     \
		ipc_log_string(ch->glink->ilc, "%s[%d:%d] %s: "x, ch->name,  \
			       ch->lcid, ch->rcid, __func__, ##__VA_ARGS__); \
} while (0)


#define GLINK_ERR(ctxt, x, ...)						       \
do {									       \
	pr_err_ratelimited("[%s]: "x, __func__, ##__VA_ARGS__);		       \
	if (ctxt->ilc)							       \
		ipc_log_string(ctxt->ilc, "[%s]: "x, __func__, ##__VA_ARGS__); \
} while (0)

#define BGCOM_ALIGNMENT 16
#define TX_BLOCKED_CMD_RESERVE 16
#define DEFAULT_FIFO_SIZE 1024
#define SHORT_SIZE 16
#define XPRT_ALIGNMENT 4

#define ACTIVE_TX BIT(0)
#define ACTIVE_RX BIT(1)

#define ID_MASK 0xFFFFFF

#define GLINK_NAME_SIZE		32
#define GLINK_VERSION_1		1

#define BGCOM_GLINK_CID_MIN	1
#define BGCOM_GLINK_CID_MAX	65536

#define TX_WAIT_US 500
#define BGCOM_RESET 0x00000000
#define BGCOM_APPLICATION_RUNNING 0x00000001
#define BGCOM_TO_SLAVE_FIFO_READY 0x00000002
#define BGCOM_TO_MASTER_FIFO_READY 0x00000004
#define BGCOM_AHB_READY 0x00000008
#define WORD_SIZE 4
#define TX_BLOCKED_CMD_RESERVE 16
#define FIFO_FULL_RESERVE (TX_BLOCKED_CMD_RESERVE/WORD_SIZE)
#define BGCOM_LINKUP (BGCOM_APPLICATION_RUNNING \
			| BGCOM_TO_SLAVE_FIFO_READY \
			| BGCOM_TO_MASTER_FIFO_READY \
			| BGCOM_AHB_READY)

struct glink_bgcom_msg {
	__le16 cmd;
	__le16 param1;
	__le32 param2;
	__le32 param3;
	__le32 param4;
	u8 data[];
} __packed;

/**
 * struct glink_bgcom_defer_cmd - deferred incoming control message
 * @node:	list node
 * @msg:	message header
 * data:	payload of the message
 *
 * Copy of a received control message, to be added to @rx_queue and processed
 * by @rx_work of @glink_bgcom.
 */
struct glink_bgcom_defer_cmd {
	struct list_head node;

	struct glink_bgcom_msg msg;
	u8 data[];
};

/**
 * struct glink_bgcom_rx_intent - RX intent
 * RX intent
 *
 * @data:	pointer to the data (may be NULL for zero-copy)
 * @id:		remote or local intent ID
 * @size:	size of the original intent (do not modify)
 * @addr:	addr to read/write the data from
 * @reuse:	To mark if the intent can be reused after first use
 * @in_use:	To mark if intent is already in use for the channel
 * @offset:	next write offset (initially 0)
 */
struct glink_bgcom_rx_intent {
	void *data;
	u32 id;
	size_t size;
	u32 addr;
	bool reuse;
	bool in_use;
	u32 offset;

	struct list_head node;
};

struct bgcom_fifo_size {
	uint32_t to_master:16;
	uint32_t to_slave:16;
};

struct bgcom_fifo_fill {
	uint32_t rx_avail:16;
	uint32_t tx_avail:16;
};

/**
 * struct glink_bgcom - driver context, relates to one remote subsystem
 * @dev:	reference to the associated struct device
 * @name:	name of this edge
 * @rx_pipe:	pipe object for receive FIFO
 * @rx_worker:	worker struct for handling received control messages
 * @rx_task:	task that runs the rx_worker
 * @rx_lock:	protects the @rx_queue
 * @rx_queue:	queue of received control messages to be processed in @rx_work
 * @tx_lock:	synchronizes operations on the tx fifo
 * @idr_lock:	synchronizes @lcids and @rcids modifications
 * @lcids:	idr of all channels with a known local channel id
 * @rcids:	idr of all channels with a known remote channel id
 * @spi_ops:	spi ops for sending data to the remote
 * @cmpnt:	component to be registered with the wdsp component manager
 * @in_reset	indicates that remote processor is in reset
 * @ilc:	ipc logging context reference
 * @sent_read_notify:	flag to check cmd sent or not
 */
struct glink_bgcom {
	struct device *dev;

	const char *name;

	struct kthread_worker rx_worker;
	struct task_struct *rx_task;

	spinlock_t rx_lock;
	struct list_head rx_queue;
	struct work_struct rx_defer_work;

	struct mutex tx_lock;

	struct mutex idr_lock;
	struct idr lcids;
	struct idr rcids;
	u32 features;

	atomic_t activity_cnt;
	atomic_t in_reset;

	void *ilc;
	bool sent_read_notify;

	struct bgcom_fifo_fill fifo_fill;
	struct bgcom_fifo_size fifo_size;
	struct mutex tx_avail_lock;
	struct kthread_worker kworker;

	uint32_t bgcom_status;
	struct bgcom_open_config_type bgcom_config;
	void *bgcom_handle;
	bool water_mark_reached;
};

enum {
	GLINK_STATE_CLOSED,
	GLINK_STATE_OPENING,
	GLINK_STATE_OPEN,
	GLINK_STATE_CLOSING,
};

/**
 * struct glink_bgcom_channel - internal representation of a channel
 * @rpdev:	rpdev reference, only used for primary endpoints
 * @ept:	rpmsg endpoint this channel is associated with
 * @glink:	glink_bgcom context handle
 * @refcount:	refcount for the channel object
 * @recv_lock:	guard for @ept.cb
 * @name:	unique channel name/identifier
 * @lcid:	channel id, in local space
 * @rcid:	channel id, in remote space
 * @intent_lock: lock for protection of @liids, @riids
 * @liids:	idr of all local intents
 * @riids:	idr of all remote intents
 * @open_ack:	completed once remote has acked the open-request
 * @open_req:	completed once open-request has been received
 * @intent_req_lock: Synchronises multiple intent requests
 * @intent_req_result: Result of intent request
 * @intent_req_comp: Completion for intent_req signalling
 */
struct glink_bgcom_channel {
	struct rpmsg_endpoint ept;

	struct rpmsg_device *rpdev;
	struct glink_bgcom *glink;

	struct kref refcount;

	spinlock_t recv_lock;

	char *name;
	unsigned int lcid;
	unsigned int rcid;

	struct mutex intent_lock;
	struct idr liids;
	struct idr riids;

	unsigned int lsigs;
	unsigned int rsigs;

	struct completion open_ack;
	struct completion open_req;

	struct mutex intent_req_lock;
	bool intent_req_result;
	struct completion intent_req_comp;
	struct completion intent_alloc_comp;
};

struct rx_pkt {
	void *rx_buf;
	uint32_t rx_len;
	struct glink_bgcom *glink;
	struct kthread_work kwork;
};

#define to_glink_channel(_ept) container_of(_ept, \
			struct glink_bgcom_channel, ept)

static const struct rpmsg_endpoint_ops glink_endpoint_ops;

#define BGCOM_CMD_VERSION			0
#define BGCOM_CMD_VERSION_ACK			1
#define BGCOM_CMD_OPEN				2
#define BGCOM_CMD_CLOSE				3
#define BGCOM_CMD_OPEN_ACK			4
#define BGCOM_CMD_CLOSE_ACK			5
#define BGCOM_CMD_INTENT			6
#define BGCOM_CMD_RX_DONE			7
#define BGCOM_CMD_RX_DONE_W_REUSE		8
#define BGCOM_CMD_RX_INTENT_REQ			9
#define BGCOM_CMD_RX_INTENT_REQ_ACK		10
#define BGCOM_CMD_TX_DATA			11
#define BGCOM_CMD_TX_DATA_CONT			12
#define BGCOM_CMD_READ_NOTIF			13
#define BGCOM_CMD_SIGNALS			14
#define BGCOM_CMD_TX_SHORT_DATA			17

static struct glink_bgcom_channel *
glink_bgcom_alloc_channel(struct glink_bgcom *glink, const char *name)
{
	struct glink_bgcom_channel *channel;

	channel = kzalloc(sizeof(*channel), GFP_KERNEL);
	if (!channel)
		return ERR_PTR(-ENOMEM);

	/* Setup glink internal glink_spi_channel data */
	spin_lock_init(&channel->recv_lock);
	mutex_init(&channel->intent_lock);
	mutex_init(&channel->intent_req_lock);

	channel->glink = glink;
	channel->name = kstrdup(name, GFP_KERNEL);

	init_completion(&channel->open_req);
	init_completion(&channel->open_ack);
	init_completion(&channel->intent_req_comp);
	init_completion(&channel->intent_alloc_comp);

	idr_init(&channel->liids);
	idr_init(&channel->riids);
	kref_init(&channel->refcount);

	return channel;
}

static void glink_bgcom_channel_release(struct kref *ref)
{
	struct glink_bgcom_channel *channel;
	struct glink_bgcom_rx_intent *tmp;
	int iid;

	channel = container_of(ref, struct glink_bgcom_channel, refcount);
	CH_INFO(channel, "\n");

	channel->intent_req_result = 0;
	complete(&channel->intent_req_comp);
	complete(&channel->intent_alloc_comp);

	mutex_lock(&channel->intent_lock);
	idr_for_each_entry(&channel->liids, tmp, iid) {
		kfree(tmp->data);
		kfree(tmp);
	}
	idr_destroy(&channel->liids);

	idr_for_each_entry(&channel->riids, tmp, iid)
		kfree(tmp);
	idr_destroy(&channel->riids);
	mutex_unlock(&channel->intent_lock);

	kfree(channel->name);
	kfree(channel);
}

static struct glink_bgcom_rx_intent *
glink_bgcom_alloc_intent(struct glink_bgcom *glink,
		       struct glink_bgcom_channel *channel,
		       size_t size,
		       bool reuseable)
{
	struct glink_bgcom_rx_intent *intent;
	int ret;

	intent = kzalloc(sizeof(*intent), GFP_KERNEL);
	if (!intent)
		return NULL;

	intent->data = kzalloc(size, GFP_KERNEL);
	if (!intent->data)
		goto free_intent;

	mutex_lock(&channel->intent_lock);
	ret = idr_alloc_cyclic(&channel->liids, intent, 1, -1, GFP_ATOMIC);
	if (ret < 0) {
		mutex_unlock(&channel->intent_lock);
		goto free_data;
	}
	mutex_unlock(&channel->intent_lock);

	intent->id = ret;
	intent->size = size;
	intent->reuse = reuseable;

	return intent;

free_data:
	kfree(intent->data);
free_intent:
	kfree(intent);
	return NULL;
}

/**
 * tx_wakeup_worker() - worker function to wakeup tx blocked thread
 * @work:	kwork associated with the edge to process commands on.
 */
static void tx_wakeup_worker(struct glink_bgcom *glink)
{
	struct bgcom_fifo_fill fifo_fill;

	mutex_lock(&glink->tx_avail_lock);
	bgcom_reg_read(glink->bgcom_handle, BGCOM_REG_FIFO_FILL, 1,
						&fifo_fill);
	glink->fifo_fill.tx_avail = fifo_fill.tx_avail;
	if (glink->fifo_fill.tx_avail > glink->fifo_size.to_slave/2)
		glink->water_mark_reached = false;
	mutex_unlock(&glink->tx_avail_lock);

	if (atomic_read(&glink->in_reset))
		return;
}

static void glink_bgcom_update_tx_avail(struct glink_bgcom *glink,
							uint32_t size)
{
	mutex_lock(&glink->tx_avail_lock);
	glink->fifo_fill.tx_avail -= size;
	if (glink->fifo_fill.tx_avail < glink->fifo_size.to_slave/2)
		glink->water_mark_reached = true;
	mutex_unlock(&glink->tx_avail_lock);
}

size_t glink_bgcom_tx_avail(struct glink_bgcom *glink)
{
	u32 tx_avail;

	mutex_lock(&glink->tx_avail_lock);
	tx_avail = glink->fifo_fill.tx_avail;
	if (tx_avail < FIFO_FULL_RESERVE)
		tx_avail = 0;
	else
		tx_avail -= FIFO_FULL_RESERVE;

	mutex_unlock(&glink->tx_avail_lock);
	return tx_avail;
}

static int glink_bgcom_tx_write_one(struct glink_bgcom *glink, void *src,
					uint32_t size)
{
	u32 tx_avail = glink_bgcom_tx_avail(glink);
	int ret;
	uint32_t size_in_words = size/WORD_SIZE;

	if (size_in_words > tx_avail) {
		GLINK_ERR(glink, "%s: No Space in Fifo\n", __func__);
		return -ENOSPC;
	}

	ret = bgcom_fifo_write(glink->bgcom_handle, size_in_words, src);
	if (ret < 0) {
		GLINK_ERR(glink, "%s: Error %d writing data\n",
							__func__, ret);
		return ret;
	}

	glink_bgcom_update_tx_avail(glink, size_in_words);
	return ret;
}

static void glink_bgcom_tx_write(struct glink_bgcom *glink,
					void *data, size_t dlen)
{
	int ret;

	if (dlen) {
		ret = glink_bgcom_tx_write_one(glink, data, dlen);
		if (ret < 0)
			GLINK_ERR(glink, "Error %d writing tx data\n", ret);
	}
}

static void glink_bgcom_send_read_notify(struct glink_bgcom *glink)
{
	struct glink_bgcom_msg msg = { 0 };
	int ret;

	msg.cmd = cpu_to_le16(BGCOM_CMD_READ_NOTIF);
	msg.param1 = 0;
	msg.param2 = 0;

	GLINK_INFO(glink, "Cmd size in words = %d\n", sizeof(msg)/WORD_SIZE);

	ret = bgcom_fifo_write(glink->bgcom_handle, sizeof(msg)/WORD_SIZE,
								&msg);
	if (ret < 0) {
		GLINK_ERR(glink, "%s: Error %d writing data\n",
							__func__, ret);
		return;
	}

	glink_bgcom_update_tx_avail(glink, sizeof(msg)/WORD_SIZE);
}

static int glink_bgcom_tx(struct glink_bgcom *glink, void *data,
						size_t dlen, bool wait)
{
	int ret = 0;

	mutex_lock(&glink->tx_lock);

	while (glink_bgcom_tx_avail(glink) < dlen/WORD_SIZE) {
		if (!wait) {
			ret = -EAGAIN;
			goto out;
		}

		if (atomic_read(&glink->in_reset)) {
			ret = -ENXIO;
			goto out;
		}

		if (!glink->sent_read_notify) {
			glink->sent_read_notify = true;
			glink_bgcom_send_read_notify(glink);
		}
		/* Wait without holding the tx_lock */
		mutex_unlock(&glink->tx_lock);

		usleep_range(TX_WAIT_US, TX_WAIT_US + 50);

		mutex_lock(&glink->tx_lock);

		if (glink_bgcom_tx_avail(glink) >= dlen/WORD_SIZE)
			glink->sent_read_notify = false;
	}

	glink_bgcom_tx_write(glink, data, dlen);

out:
	mutex_unlock(&glink->tx_lock);

	return ret;
}

/**
 * glink_bgcom_send_intent_req_ack() - convert an rx intent request ack cmd to
				      wire format and transmit
 * @glink:	The transport to transmit on.
 * @channel:	The glink channel
 * @granted:	The request response to encode.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int glink_bgcom_send_intent_req_ack(struct glink_bgcom *glink,
					 struct glink_bgcom_channel *channel,
					 bool granted)
{
	struct glink_bgcom_msg msg = { 0 };

	msg.cmd = cpu_to_le16(BGCOM_CMD_RX_INTENT_REQ_ACK);
	msg.param1 = cpu_to_le16(channel->lcid);
	msg.param2 = cpu_to_le32(granted);

	CH_INFO(channel, "\n");
	glink_bgcom_tx(glink, &msg, sizeof(msg), true);

	return 0;
}

static int glink_bgcom_send_data(struct glink_bgcom_channel *channel,
			       void *data, int chunk_size, int left_size,
			       struct glink_bgcom_rx_intent *intent, bool wait)
{
	struct glink_bgcom *glink = channel->glink;
	struct {
		struct glink_bgcom_msg msg;
		__le32 chunk_size;
		__le32 left_size;
		uint64_t addr;
	} __packed req;

	CH_INFO(channel, "chunk:%d, left:%d\n", chunk_size, left_size);

	memset(&req, 0, sizeof(req));
	if (intent->offset)
		req.msg.cmd = cpu_to_le16(BGCOM_CMD_TX_DATA_CONT);
	else
		req.msg.cmd = cpu_to_le16(BGCOM_CMD_TX_DATA);

	req.msg.param1 = cpu_to_le16(channel->lcid);
	req.msg.param2 = cpu_to_le32(intent->id);
	req.chunk_size = cpu_to_le32(chunk_size);
	req.left_size = cpu_to_le32(left_size);
	req.addr = 0;

	mutex_lock(&glink->tx_lock);
	while (glink_bgcom_tx_avail(glink) < sizeof(req)/WORD_SIZE) {
		if (!wait) {
			mutex_unlock(&glink->tx_lock);
			return -EAGAIN;
		}

		if (atomic_read(&glink->in_reset)) {
			mutex_unlock(&glink->tx_lock);
			return -EINVAL;
		}

		if (!glink->sent_read_notify) {
			glink->sent_read_notify = true;
			glink_bgcom_send_read_notify(glink);
		}

		/* Wait without holding the tx_lock */
		mutex_unlock(&glink->tx_lock);

		usleep_range(TX_WAIT_US, TX_WAIT_US + 50);

		mutex_lock(&glink->tx_lock);

		if (glink_bgcom_tx_avail(glink) >= sizeof(req)/WORD_SIZE)
			glink->sent_read_notify = false;
	}

	bgcom_ahb_write(glink->bgcom_handle,
	(uint32_t)(size_t)(intent->addr + intent->offset),
	ALIGN(chunk_size, WORD_SIZE)/WORD_SIZE, data);

	intent->offset += chunk_size;
	glink_bgcom_tx_write(glink, &req, sizeof(req));

	mutex_unlock(&glink->tx_lock);
	return 0;
}

static void glink_bgcom_handle_intent_req_ack(struct glink_bgcom *glink,
					    unsigned int cid, bool granted)
{
	struct glink_bgcom_channel *channel;

	mutex_lock(&glink->idr_lock);
	channel = idr_find(&glink->rcids, cid);
	mutex_unlock(&glink->idr_lock);
	if (!channel) {
		dev_err(glink->dev, "unable to find channel\n");
		return;
	}

	channel->intent_req_result = granted;
	complete(&channel->intent_req_comp);
	CH_INFO(channel, "\n");
}

/**
 * glink_bgcom_advertise_intent - convert an rx intent cmd to wire format and
 *			   transmit
 * @glink:	The transport to transmit on.
 * @channel:	The local channel
 * @size:	The intent to pass on to remote.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int glink_bgcom_advertise_intent(struct glink_bgcom *glink,
				      struct glink_bgcom_channel *channel,
				      struct glink_bgcom_rx_intent *intent)
{
	struct command {
		struct glink_bgcom_msg msg;
		__le32 size;
		__le32 liid;
		__le64 addr;
	} __packed;
	struct command cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.msg.cmd = cpu_to_le16(BGCOM_CMD_INTENT);
	cmd.msg.param1 = cpu_to_le16(channel->lcid);
	cmd.msg.param2 = cpu_to_le32(1);
	cmd.size = cpu_to_le32(intent->size);
	cmd.liid = cpu_to_le32(intent->id);

	glink_bgcom_tx(glink, &cmd, sizeof(cmd),  true);

	CH_INFO(channel, "count:%d size:%lu liid:%d\n", 1,
		intent->size, intent->id);

	return 0;
}

/**
 * glink_bgcom_handle_intent_req() - Receive a request for rx_intent
 *					    from remote side
 * if_ptr:      Pointer to the transport interface
 * rcid:	Remote channel ID
 * size:	size of the intent
 *
 * The function searches for the local channel to which the request for
 * rx_intent has arrived and allocates and notifies the remote back
 */
static void glink_bgcom_handle_intent_req(struct glink_bgcom *glink,
					u32 cid, size_t size)
{
	struct glink_bgcom_rx_intent *intent;
	struct glink_bgcom_channel *channel;

	mutex_lock(&glink->idr_lock);
	channel = idr_find(&glink->rcids, cid);
	mutex_unlock(&glink->idr_lock);

	if (!channel) {
		pr_err("%s channel not found for cid %d\n", __func__, cid);
		return;
	}

	intent = glink_bgcom_alloc_intent(glink, channel, size, false);
	if (intent)
		glink_bgcom_advertise_intent(glink, channel, intent);

	glink_bgcom_send_intent_req_ack(glink, channel, !!intent);
}

static int glink_bgcom_send_short(struct glink_bgcom_channel *channel,
				void *data, int len,
				struct glink_bgcom_rx_intent *intent,
				bool wait)
{
	struct glink_bgcom *glink = channel->glink;
	struct {
		struct glink_bgcom_msg msg;
		u8 data[SHORT_SIZE];
	} __packed req;

	CH_INFO(channel, "intent offset:%d len:%d\n", intent->offset, len);

	req.msg.cmd = cpu_to_le16(BGCOM_CMD_TX_SHORT_DATA);
	req.msg.param1 = cpu_to_le16(channel->lcid);
	req.msg.param2 = cpu_to_le32(intent->id);
	req.msg.param3 = cpu_to_le32(len);
	req.msg.param4 = cpu_to_be32(0);
	memcpy(req.data, data, len);

	mutex_lock(&glink->tx_lock);
	while (glink_bgcom_tx_avail(glink) < sizeof(req)/WORD_SIZE) {

		if (!wait) {
			mutex_unlock(&glink->tx_lock);
			return -EAGAIN;
		}

		if (atomic_read(&glink->in_reset)) {
			mutex_unlock(&glink->tx_lock);
			return -EINVAL;
		}

		if (!glink->sent_read_notify) {
			glink->sent_read_notify = true;
			glink_bgcom_send_read_notify(glink);
		}

		/* Wait without holding the tx_lock */
		mutex_unlock(&glink->tx_lock);

		usleep_range(TX_WAIT_US, TX_WAIT_US + 50);

		mutex_lock(&glink->tx_lock);

		if (glink_bgcom_tx_avail(glink) >= sizeof(req)/WORD_SIZE)
			glink->sent_read_notify = false;
	}
	glink_bgcom_tx_write(glink, &req, sizeof(req));

	mutex_unlock(&glink->tx_lock);
	return 0;
}

static int glink_bgcom_request_intent(struct glink_bgcom *glink,
				    struct glink_bgcom_channel *channel,
				    size_t size)
{
	struct glink_bgcom_msg req = { 0 };
	int ret;

	kref_get(&channel->refcount);
	mutex_lock(&channel->intent_req_lock);

	reinit_completion(&channel->intent_req_comp);
	reinit_completion(&channel->intent_alloc_comp);

	req.cmd = cpu_to_le16(BGCOM_CMD_RX_INTENT_REQ);
	req.param1 = cpu_to_le16(channel->lcid);
	req.param2 = cpu_to_le32(size);

	CH_INFO(channel, "size:%lu\n", size);

	ret = glink_bgcom_tx(glink, &req, sizeof(req), true);
	if (ret)
		goto unlock;

	ret = wait_for_completion_timeout(&channel->intent_req_comp, 10 * HZ);
	if (!ret) {
		dev_err(glink->dev, "intent request ack timed out\n");
		ret = -ETIMEDOUT;
	}

	ret = wait_for_completion_timeout(&channel->intent_alloc_comp, 10 * HZ);
	if (!ret) {
		dev_err(glink->dev, "intent request alloc timed out\n");
		ret = -ETIMEDOUT;
	} else {
		ret = channel->intent_req_result ? 0 : -ECANCELED;
	}
unlock:
	mutex_unlock(&channel->intent_req_lock);
	kref_put(&channel->refcount, glink_bgcom_channel_release);
	return ret;
}

static int __glink_bgcom_send(struct glink_bgcom_channel *channel,
			    void *data, int len, bool wait)
{
	struct glink_bgcom *glink = channel->glink;
	struct glink_bgcom_rx_intent *intent = NULL;
	struct glink_bgcom_rx_intent *tmp;
	int size = len;
	int iid = 0;
	int ret = 0;

	CH_INFO(channel, "size:%d, wait:%d\n", len, wait);

	atomic_inc(&glink->activity_cnt);
	bgcom_resume(glink->bgcom_handle);
	while (!intent) {
		mutex_lock(&channel->intent_lock);
		idr_for_each_entry(&channel->riids, tmp, iid) {
			if (tmp->size >= len && !tmp->in_use) {
				if (!intent)
					intent = tmp;
				else if (intent->size > tmp->size)
					intent = tmp;
				if (intent->size == len)
					break;
			}
		}
		if (intent)
			intent->in_use = true;
		mutex_unlock(&channel->intent_lock);

		/* We found an available intent */
		if (intent)
			break;

		if (!wait) {
			ret = -EBUSY;
			goto tx_exit;
		}

		ret = glink_bgcom_request_intent(glink, channel, len);
		if (ret < 0)
			goto tx_exit;
	}

	if (len <= SHORT_SIZE)
		size = 0;
	else if (size & (XPRT_ALIGNMENT - 1))
		size = ALIGN(len - SHORT_SIZE, XPRT_ALIGNMENT);

	if (size) {
		ret = glink_bgcom_send_data(channel, data, size, len - size,
					  intent, wait);
		if (ret)
			goto tx_exit;
	}

	data = (char *)data + size;
	size = len - size;
	if (size)
		ret = glink_bgcom_send_short(channel, data, size, intent, wait);

tx_exit:
	/* Mark intent available if we failed */
	if (ret && intent)
		intent->in_use = false;

	atomic_dec(&glink->activity_cnt);

	return ret;
}

/**
 * glink_spi_send_signals() - convert a signal  cmd to wire format and transmit
 * @glink:	The transport to transmit on.
 * @channel:	The glink channel
 * @sigs:	The signals to encode.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int glink_bgcom_send_signals(struct glink_bgcom *glink,
				  struct glink_bgcom_channel *channel,
				  u32 sigs)
{
	struct glink_bgcom_msg msg;

	msg.cmd = cpu_to_le16(BGCOM_CMD_SIGNALS);
	msg.param1 = cpu_to_le16(channel->lcid);
	msg.param2 = cpu_to_le32(sigs);

	GLINK_INFO(glink, "sigs:%d\n", sigs);
	return glink_bgcom_tx(glink, &msg, sizeof(msg), true);
}

static int glink_bgcom_handle_signals(struct glink_bgcom *glink,
				    unsigned int rcid, unsigned int signals)
{
	struct glink_bgcom_channel *channel;
	u32 old;

	mutex_lock(&glink->idr_lock);
	channel = idr_find(&glink->rcids, rcid);
	mutex_unlock(&glink->idr_lock);
	if (!channel) {
		dev_err(glink->dev, "signal for non-existing channel\n");
		return -EINVAL;
	}

	old = channel->rsigs;
	channel->rsigs = signals;

	if (channel->ept.sig_cb)
		channel->ept.sig_cb(channel->ept.rpdev, old, channel->rsigs);

	CH_INFO(channel, "old:%d new:%d\n", old, channel->rsigs);

	return 0;
}

static int glink_bgcom_send_version(struct glink_bgcom *glink)
{
	struct glink_bgcom_msg msg = { 0 };

	msg.cmd = cpu_to_le16(BGCOM_CMD_VERSION);
	msg.param1 = cpu_to_le16(GLINK_VERSION_1);
	msg.param2 = cpu_to_le32(glink->features);

	GLINK_INFO(glink, "vers:%d features:%d\n", msg.param1, msg.param2);
	return glink_bgcom_tx(glink, &msg, sizeof(msg), true);
}

static void glink_bgcom_send_version_ack(struct glink_bgcom *glink)
{
	struct glink_bgcom_msg msg = { 0 };

	msg.cmd = cpu_to_le16(BGCOM_CMD_VERSION_ACK);
	msg.param1 = cpu_to_le16(GLINK_VERSION_1);
	msg.param2 = cpu_to_le32(glink->features);

	GLINK_INFO(glink, "vers:%d features:%d\n", msg.param1, msg.param2);
	glink_bgcom_tx(glink, &msg, sizeof(msg), true);
}

static void glink_bgcom_send_close_req(struct glink_bgcom *glink,
				     struct glink_bgcom_channel *channel)
{
	struct glink_bgcom_msg req = { 0 };

	req.cmd = cpu_to_le16(BGCOM_CMD_CLOSE);
	req.param1 = cpu_to_le16(channel->lcid);

	CH_INFO(channel, "\n");
	glink_bgcom_tx(glink, &req, sizeof(req), true);
}

/**
 * glink_bgcom_send_open_req() - send a BGCOM_CMD_OPEN request to the remote
 * @glink: Ptr to the glink edge
 * @channel: Ptr to the channel that the open req is sent
 *
 * Allocates a local channel id and sends a BGCOM_CMD_OPEN message to the
 * remote. Will return with refcount held, regardless of outcome.
 *
 * Returns 0 on success, negative errno otherwise.
 */
static int glink_bgcom_send_open_req(struct glink_bgcom *glink,
				   struct glink_bgcom_channel *channel)
{

	struct cmd_msg {
		__le16 cmd;
		__le16 lcid;
		__le16 length;
		__le16 req_xprt;
		__le64 reserved;
	};
	struct {
		struct cmd_msg msg;
		u8 name[GLINK_NAME_SIZE];
	} __packed req;
	int name_len = strlen(channel->name) + 1;
	int req_len = ALIGN(sizeof(req.msg) + name_len, BGCOM_ALIGNMENT);
	int ret;

	kref_get(&channel->refcount);

	mutex_lock(&glink->idr_lock);
	ret = idr_alloc_cyclic(&glink->lcids, channel,
			       BGCOM_GLINK_CID_MIN, BGCOM_GLINK_CID_MAX,
			       GFP_ATOMIC);
	mutex_unlock(&glink->idr_lock);
	if (ret < 0)
		return ret;

	channel->lcid = ret;
	CH_INFO(channel, "\n");

	memset(&req, 0, sizeof(req));
	req.msg.cmd = cpu_to_le16(BGCOM_CMD_OPEN);
	req.msg.lcid = cpu_to_le16(channel->lcid);
	req.msg.length = cpu_to_le16(name_len);
	strlcpy(req.name, channel->name, GLINK_NAME_SIZE);

	ret = glink_bgcom_tx(glink, &req, req_len, true);
	if (ret)
		goto remove_idr;

	return 0;

remove_idr:
	CH_INFO(channel, "remove_idr\n");

	mutex_lock(&glink->idr_lock);
	idr_remove(&glink->lcids, channel->lcid);
	channel->lcid = 0;
	mutex_unlock(&glink->idr_lock);

	return ret;
}

static void glink_bgcom_send_open_ack(struct glink_bgcom *glink,
				    struct glink_bgcom_channel *channel)
{
	struct glink_bgcom_msg msg = { 0 };

	msg.cmd = cpu_to_le16(BGCOM_CMD_OPEN_ACK);
	msg.param1 = cpu_to_le16(channel->rcid);

	CH_INFO(channel, "\n");
	glink_bgcom_tx(glink, &msg, sizeof(msg), true);
}

static int glink_bgcom_rx_open_ack(struct glink_bgcom *glink,
							unsigned int lcid)
{
	struct glink_bgcom_channel *channel;

	mutex_lock(&glink->idr_lock);
	channel = idr_find(&glink->lcids, lcid);
	mutex_unlock(&glink->idr_lock);
	if (!channel) {
		GLINK_ERR(glink, "Invalid open ack packet %d\n", lcid);
		return -EINVAL;
	}

	CH_INFO(channel, "\n");
	complete_all(&channel->open_ack);

	return 0;
}

/* Remote initiated rpmsg_create_ept */
static int glink_bgcom_create_remote(struct glink_bgcom *glink,
				   struct glink_bgcom_channel *channel)
{
	int ret;

	CH_INFO(channel, "\n");

	glink_bgcom_send_open_ack(glink, channel);

	ret = glink_bgcom_send_open_req(glink, channel);
	if (ret)
		goto close_link;

	ret = wait_for_completion_timeout(&channel->open_ack, 5 * HZ);
	if (!ret) {
		ret = -ETIMEDOUT;
		goto close_link;
	}

	return 0;

close_link:
	CH_INFO(channel, "close_link %d\n", ret);

	/*
	 * Send a close request to "undo" our open-ack. The close-ack will
	 * release glink_bgcom_send_open_req() reference and the last reference
	 * will be release after rx_close or transport unregister by calling
	 * glink_bgcom_remove().
	 */
	glink_bgcom_send_close_req(glink, channel);

	return ret;
}

/* Locally initiated rpmsg_create_ept */
static struct glink_bgcom_channel
*glink_bgcom_create_local(struct glink_bgcom *glink, const char *name)
{
	struct glink_bgcom_channel *channel;
	int ret;

	channel = glink_bgcom_alloc_channel(glink, name);
	if (IS_ERR(channel))
		return ERR_CAST(channel);

	CH_INFO(channel, "\n");
	ret = glink_bgcom_send_open_req(glink, channel);
	if (ret)
		goto release_channel;

	ret = wait_for_completion_timeout(&channel->open_ack, 5 * HZ);
	if (!ret)
		goto err_timeout;

	ret = wait_for_completion_timeout(&channel->open_req, 5 * HZ);
	if (!ret)
		goto err_timeout;

	glink_bgcom_send_open_ack(glink, channel);

	return channel;

err_timeout:
	CH_INFO(channel, "err_timeout\n");

	/* glink_bgcom_send_open_req() did register the channel in lcids*/
	mutex_lock(&glink->idr_lock);
	idr_remove(&glink->lcids, channel->lcid);
	mutex_unlock(&glink->idr_lock);

release_channel:
	CH_INFO(channel, "release_channel\n");
	/* Release glink_bgcom_send_open_req() reference */
	kref_put(&channel->refcount, glink_bgcom_channel_release);
	/* Release glink_bgcom_alloc_channel() reference */
	kref_put(&channel->refcount, glink_bgcom_channel_release);

	return ERR_PTR(-ETIMEDOUT);
}

static struct rpmsg_endpoint *
glink_bgcom_create_ept(struct rpmsg_device *rpdev, rpmsg_rx_cb_t cb,
		void *priv, struct rpmsg_channel_info chinfo)
{
	struct glink_bgcom_channel *parent = to_glink_channel(rpdev->ept);
	struct glink_bgcom_channel *channel;
	struct glink_bgcom *glink = parent->glink;
	struct rpmsg_endpoint *ept;
	const char *name = chinfo.name;
	int cid;
	int ret;

	mutex_lock(&glink->idr_lock);
	idr_for_each_entry(&glink->rcids, channel, cid) {
		if (!strcmp(channel->name, name))
			break;
	}
	mutex_unlock(&glink->idr_lock);

	if (!channel) {
		channel = glink_bgcom_create_local(glink, name);
		if (IS_ERR(channel))
			return NULL;
	} else {
		ret = glink_bgcom_create_remote(glink, channel);
		if (ret)
			return NULL;
	}

	ept = &channel->ept;
	ept->rpdev = rpdev;
	ept->cb = cb;
	ept->priv = priv;
	ept->ops = &glink_endpoint_ops;

	return ept;
}

static int glink_bgcom_announce_create(struct rpmsg_device *rpdev)
{
	struct glink_bgcom_channel *channel = to_glink_channel(rpdev->ept);
	struct device_node *np = rpdev->dev.of_node;
	struct glink_bgcom *glink = channel->glink;
	struct glink_bgcom_rx_intent *intent;
	const struct property *prop = NULL;
	__be32 defaults[] = { cpu_to_be32(SZ_1K), cpu_to_be32(5) };
	int num_intents;
	int num_groups = 1;
	__be32 *val = defaults;
	int size;

	if (!completion_done(&channel->open_ack))
		return 0;

	prop = of_find_property(np, "qcom,intents", NULL);
	if (prop) {
		val = prop->value;
		num_groups = prop->length / sizeof(u32) / 2;
	}

	/* Channel is now open, advertise base set of intents */
	while (num_groups--) {
		size = be32_to_cpup(val++);
		num_intents = be32_to_cpup(val++);
		while (num_intents--) {
			intent = glink_bgcom_alloc_intent(glink, channel, size,
							 true);
			if (!intent)
				break;

			glink_bgcom_advertise_intent(glink, channel, intent);
		}
	}
	return 0;
}

static void glink_bgcom_destroy_ept(struct rpmsg_endpoint *ept)
{
	struct glink_bgcom_channel *channel = to_glink_channel(ept);
	struct glink_bgcom *glink = channel->glink;
	unsigned long flags;

	spin_lock_irqsave(&channel->recv_lock, flags);
	channel->ept.cb = NULL;
	spin_unlock_irqrestore(&channel->recv_lock, flags);

	/* Decouple the potential rpdev from the channel */
	channel->rpdev = NULL;

	glink_bgcom_send_close_req(glink, channel);
}

static void glink_bgcom_send_close_ack(struct glink_bgcom *glink,
				     unsigned int rcid)
{
	struct glink_bgcom_msg req = { 0 };

	req.cmd = cpu_to_le16(BGCOM_CMD_CLOSE_ACK);
	req.param1 = cpu_to_le16(rcid);

	GLINK_INFO(glink, "rcid:%d\n", rcid);
	glink_bgcom_tx(glink, &req, sizeof(req), true);
}

static void glink_bgcom_rx_close(struct glink_bgcom *glink, unsigned int rcid)
{
	struct rpmsg_channel_info chinfo;
	struct glink_bgcom_channel *channel;

	mutex_lock(&glink->idr_lock);
	channel = idr_find(&glink->rcids, rcid);
	mutex_unlock(&glink->idr_lock);
	if (WARN(!channel, "close request on unknown channel\n"))
		return;
	CH_INFO(channel, "\n");

	if (channel->rpdev) {
		strlcpy(chinfo.name, channel->name, sizeof(chinfo.name));
		chinfo.src = RPMSG_ADDR_ANY;
		chinfo.dst = RPMSG_ADDR_ANY;

		rpmsg_unregister_device(glink->dev, &chinfo);
	}

	glink_bgcom_send_close_ack(glink, channel->rcid);

	mutex_lock(&glink->idr_lock);
	idr_remove(&glink->rcids, channel->rcid);
	channel->rcid = 0;
	mutex_unlock(&glink->idr_lock);

	kref_put(&channel->refcount, glink_bgcom_channel_release);
}

static void glink_bgcom_rx_close_ack(struct glink_bgcom *glink,
							unsigned int lcid)
{
	struct glink_bgcom_channel *channel;

	mutex_lock(&glink->idr_lock);
	channel = idr_find(&glink->lcids, lcid);
	if (WARN(!channel, "close ack on unknown channel\n")) {
		mutex_unlock(&glink->idr_lock);
		return;
	}
	CH_INFO(channel, "\n");

	idr_remove(&glink->lcids, channel->lcid);
	channel->lcid = 0;
	mutex_unlock(&glink->idr_lock);

	kref_put(&channel->refcount, glink_bgcom_channel_release);
}

static int glink_bgcom_send(struct rpmsg_endpoint *ept, void *data, int len)
{
	struct glink_bgcom_channel *channel = to_glink_channel(ept);

	return __glink_bgcom_send(channel, data, len, true);
}

static int glink_bgcom_trysend(struct rpmsg_endpoint *ept, void *data,
								int len)
{
	struct glink_bgcom_channel *channel = to_glink_channel(ept);

	return __glink_bgcom_send(channel, data, len, false);
}

/**
 * glink_bgcom_receive_version_ack() - receive negotiation ack from remote
 * system
 *
 * @glink:	pointer to transport interface
 * @r_version:	remote version response
 * @r_features:	remote features response
 *
 * This function is called in response to a local-initiated version/feature
 * negotiation sequence and is the counter-offer from the remote side based
 * upon the initial version and feature set requested.
 */
static void glink_bgcom_receive_version_ack(struct glink_bgcom *glink,
					  u32 version,
					  u32 features)
{
	GLINK_INFO(glink, "vers:%d features:%d\n", version, features);

	switch (version) {
	case 0:
		/* Version negotiation failed */
		break;
	case GLINK_VERSION_1:
		if (features == glink->features)
			break;

		glink->features &= features;
		/* FALLTHROUGH */
	default:
		glink_bgcom_send_version(glink);
		break;
	}
}

/**
 * glink_bgcom_receive_version() - receive version/features from remote system
 *
 * @glink:	pointer to transport interface
 * @r_version:	remote version
 * @r_features:	remote features
 *
 * This function is called in response to a remote-initiated version/feature
 * negotiation sequence.
 */
static void glink_bgcom_receive_version(struct glink_bgcom *glink,
				      u32 version,
				      u32 features)
{
	GLINK_INFO(glink, "vers:%d features:%d\n", version, features);

	switch (version) {
	case 0:
		break;
	case GLINK_VERSION_1:
		glink->features &= features;
		/* FALLTHROUGH */
	default:
		glink_bgcom_send_version_ack(glink);
		break;
	}
}

static const struct rpmsg_device_ops glink_device_ops = {
	.create_ept = glink_bgcom_create_ept,
	.announce_create = glink_bgcom_announce_create,
};

/*
 * Finds the device_node for the glink child interested in this channel.
 */
static struct device_node *glink_bgcom_match_channel(struct device_node *node,
						   const char *channel)
{
	struct device_node *child;
	const char *name;
	const char *key;
	int ret;

	for_each_available_child_of_node(node, child) {
		key = "qcom,glink-channels";
		ret = of_property_read_string(child, key, &name);
		if (ret)
			continue;

		if (strcmp(name, channel) == 0)
			return child;
	}

	return NULL;
}

static void glink_bgcom_rpdev_release(struct device *dev)
{
	struct rpmsg_device *rpdev = to_rpmsg_device(dev);
	struct glink_bgcom_channel *channel = to_glink_channel(rpdev->ept);

	channel->rpdev = NULL;
	kfree(rpdev);

}
static int glink_bgcom_rx_open(struct glink_bgcom *glink, unsigned int rcid,
			     char *name)
{
	struct glink_bgcom_channel *channel;
	struct rpmsg_device *rpdev;
	bool create_device = false;
	struct device_node *node;
	int lcid;
	int ret;

	mutex_lock(&glink->idr_lock);
	idr_for_each_entry(&glink->lcids, channel, lcid) {
		if (!strcmp(channel->name, name))
			break;
	}
	mutex_unlock(&glink->idr_lock);

	if (!channel) {
		channel = glink_bgcom_alloc_channel(glink, name);
		if (IS_ERR(channel))
			return PTR_ERR(channel);

		/* The opening dance was initiated by the remote */
		create_device = true;
	}

	mutex_lock(&glink->idr_lock);
	ret = idr_alloc(&glink->rcids, channel, rcid, rcid + 1, GFP_ATOMIC);
	if (ret < 0) {
		dev_err(glink->dev, "Unable to insert channel into rcid list\n");
		mutex_unlock(&glink->idr_lock);
		goto free_channel;
	}
	channel->rcid = ret;
	mutex_unlock(&glink->idr_lock);

	complete_all(&channel->open_req);

	if (create_device) {
		rpdev = kzalloc(sizeof(*rpdev), GFP_KERNEL);
		if (!rpdev) {
			ret = -ENOMEM;
			goto rcid_remove;
		}

		rpdev->ept = &channel->ept;
		strlcpy(rpdev->id.name, name, RPMSG_NAME_SIZE);
		rpdev->src = RPMSG_ADDR_ANY;
		rpdev->dst = RPMSG_ADDR_ANY;
		rpdev->ops = &glink_device_ops;

		node = glink_bgcom_match_channel(glink->dev->of_node, name);
		rpdev->dev.of_node = node;
		rpdev->dev.parent = glink->dev;
		rpdev->dev.release = glink_bgcom_rpdev_release;

		ret = rpmsg_register_device(rpdev);
		if (ret)
			goto free_rpdev;

		channel->rpdev = rpdev;
	}
	CH_INFO(channel, "\n");

	return 0;

free_rpdev:
	CH_INFO(channel, "free_rpdev\n");
	kfree(rpdev);
rcid_remove:
	CH_INFO(channel, "rcid_remove\n");
	mutex_lock(&glink->idr_lock);
	idr_remove(&glink->rcids, channel->rcid);
	channel->rcid = 0;
	mutex_unlock(&glink->idr_lock);
free_channel:
	CH_INFO(channel, "free_channel\n");
	/* Release the reference, iff we took it */
	if (create_device)
		kref_put(&channel->refcount, glink_bgcom_channel_release);

	return ret;
}

static void glink_bgcom_defer_work(struct work_struct *work)
{
	struct glink_bgcom *glink = container_of(work, struct glink_bgcom,
					       rx_defer_work);

	struct glink_bgcom_defer_cmd *dcmd;
	struct glink_bgcom_msg *msg;
	unsigned long flags;
	unsigned int param1;
	unsigned int param2;
	unsigned int param3;
	unsigned int param4;
	unsigned int cmd;

	atomic_inc(&glink->activity_cnt);
	bgcom_resume(glink->bgcom_handle);
	for (;;) {
		spin_lock_irqsave(&glink->rx_lock, flags);
		if (list_empty(&glink->rx_queue)) {
			spin_unlock_irqrestore(&glink->rx_lock, flags);
			break;
		}
		dcmd = list_first_entry(&glink->rx_queue,
					struct glink_bgcom_defer_cmd, node);
		list_del(&dcmd->node);
		spin_unlock_irqrestore(&glink->rx_lock, flags);

		msg = &dcmd->msg;
		cmd = le16_to_cpu(msg->cmd);
		param1 = le16_to_cpu(msg->param1);
		param2 = le32_to_cpu(msg->param2);
		param3 = le32_to_cpu(msg->param3);
		param4 = le32_to_cpu(msg->param4);

		switch (cmd) {
		case BGCOM_CMD_OPEN:
			glink_bgcom_rx_open(glink, param1, msg->data);
			break;
		case BGCOM_CMD_CLOSE:
			glink_bgcom_rx_close(glink, param1);
			break;
		case BGCOM_CMD_CLOSE_ACK:
			glink_bgcom_rx_close_ack(glink, param1);
			break;
		default:
			WARN(1, "Unknown defer object %d\n", cmd);
			break;
		}

		kfree(dcmd);
	}
	atomic_dec(&glink->activity_cnt);
}

static int glink_bgcom_rx_defer(struct glink_bgcom *glink,
			      void *rx_data, u32 rx_avail, size_t extra)
{
	struct glink_bgcom_defer_cmd *dcmd;

	extra = ALIGN(extra, BGCOM_ALIGNMENT);

	if (rx_avail < sizeof(struct glink_bgcom_msg) + extra) {
		dev_dbg(glink->dev, "Insufficient data in rx fifo");
		return -ENXIO;
	}

	dcmd = kzalloc(sizeof(*dcmd) + extra, GFP_KERNEL);
	if (!dcmd)
		return -ENOMEM;

	INIT_LIST_HEAD(&dcmd->node);

	memcpy(&dcmd->msg, rx_data, sizeof(dcmd->msg) + extra);

	spin_lock(&glink->rx_lock);
	list_add_tail(&dcmd->node, &glink->rx_queue);
	spin_unlock(&glink->rx_lock);

	schedule_work(&glink->rx_defer_work);

	return 0;
}

/**
 * glink_bgcom_send_rx_done() - send a rx done to remote side
 * glink:       The transport to transmit on
 * channel:	The glink channel
 * intent:	the intent to send rx done for
 *
 * This function assumes the intent lock is held
 */
static void glink_bgcom_send_rx_done(struct glink_bgcom *glink,
				   struct glink_bgcom_channel *channel,
				   struct glink_bgcom_rx_intent *intent)
{
	struct {
		u16 id;
		u16 lcid;
		u32 liid;
		u64 reserved;
	} __packed cmd;
	unsigned int cid = channel->lcid;
	unsigned int iid = intent->id;
	bool reuse = intent->reuse;

	cmd.id = reuse ? BGCOM_CMD_RX_DONE_W_REUSE : BGCOM_CMD_RX_DONE;
	cmd.lcid = cid;
	cmd.liid = iid;
	glink_bgcom_tx(glink, &cmd, sizeof(cmd), true);
	CH_INFO(channel, "reuse:%d liid:%d", reuse, iid);
}

/**
 * glink_bgcom_free_intent() - Reset and free intent if not reusuable
 * channel:	The glink channel
 * intent:	the intent to send rx done for
 *
 * This function assumes the intent lock is held
 */
static void glink_bgcom_free_intent(struct glink_bgcom_channel *channel,
				  struct glink_bgcom_rx_intent *intent)
{
	CH_INFO(channel, "reuse:%d liid:%d", intent->reuse, intent->id);
	intent->offset = 0;
	if (!intent->reuse) {
		idr_remove(&channel->liids, intent->id);
		kfree(intent->data);
		kfree(intent);
	}
}

static int glink_bgcom_rx_data(struct glink_bgcom *glink,
			     unsigned int rcid, unsigned int liid,
			     void *rx_data, size_t avail)
{
	struct glink_bgcom_rx_intent *intent;
	struct glink_bgcom_channel *channel;
	struct data_desc {
		__le32 chunk_size;
		__le32 left_size;
		__le64 addr;
	};
	struct data_desc *hdr;
	unsigned int chunk_size;
	unsigned int left_size;
	u32 addr;
	size_t msglen;
	unsigned long flags;
	int rc;

	msglen = sizeof(*hdr);
	if (avail < msglen) {
		dev_dbg(glink->dev, "Not enough data in fifo\n");
		return avail;
	}
	hdr = (struct data_desc *)rx_data;

	chunk_size = le32_to_cpu(hdr->chunk_size);
	left_size = le32_to_cpu(hdr->left_size);
	addr = (u32)le64_to_cpu(hdr->addr);

	mutex_lock(&glink->idr_lock);
	channel = idr_find(&glink->rcids, rcid);
	mutex_unlock(&glink->idr_lock);
	if (!channel) {
		dev_dbg(glink->dev, "Data on non-existing channel\n");
		return msglen;
	}
	CH_INFO(channel, "chunk_size:%d left_size:%d\n", chunk_size, left_size);

	mutex_lock(&channel->intent_lock);
	intent = idr_find(&channel->liids, liid);

	if (!intent) {
		dev_err(glink->dev,
			"no intent found for channel %s intent %d",
			channel->name, liid);
		mutex_unlock(&channel->intent_lock);

		return msglen;
	}

	if (intent->size - intent->offset < chunk_size) {
		dev_err(glink->dev, "Insufficient space in intent\n");
		mutex_unlock(&channel->intent_lock);

		/* The packet header lied, drop payload */
		return msglen;
	}

	rc = bgcom_ahb_read(glink->bgcom_handle, (uint32_t)(size_t)addr,
			ALIGN(chunk_size, WORD_SIZE)/WORD_SIZE,
			intent->data + intent->offset);
	if (rc < 0) {
		GLINK_ERR(glink, "%s: Error %d receiving data\n",
							__func__, rc);
	}

	intent->offset += chunk_size;

	/* Handle message when no fragments remain to be received */
	if (!left_size) {
		glink_bgcom_send_rx_done(glink, channel, intent);

		spin_lock_irqsave(&channel->recv_lock, flags);
		if (channel->ept.cb) {
			channel->ept.cb(channel->ept.rpdev,
					intent->data,
					intent->offset,
					channel->ept.priv,
					RPMSG_ADDR_ANY);
		}
		spin_unlock_irqrestore(&channel->recv_lock, flags);

		glink_bgcom_free_intent(channel, intent);
	}
	mutex_unlock(&channel->intent_lock);

	return msglen;
}

static int glink_bgcom_rx_short_data(struct glink_bgcom *glink,
				   unsigned int rcid, unsigned int liid,
				   unsigned int chunk_size,
				   unsigned int left_size,
				   void *src, size_t avail)
{
	struct glink_bgcom_rx_intent *intent;
	struct glink_bgcom_channel *channel;
	size_t msglen = SHORT_SIZE;
	unsigned long flags;

	if (avail < msglen) {
		dev_dbg(glink->dev, "Not enough data in fifo\n");
		return avail;
	}
	mutex_lock(&glink->idr_lock);
	channel = idr_find(&glink->rcids, rcid);
	mutex_unlock(&glink->idr_lock);
	if (!channel) {
		dev_dbg(glink->dev, "Data on non-existing channel\n");
		return msglen;
	}
	CH_INFO(channel, "chunk_size:%d left_size:%d\n", chunk_size, left_size);

	mutex_lock(&channel->intent_lock);
	intent = idr_find(&channel->liids, liid);

	if (!intent) {
		dev_err(glink->dev,
			"no intent found for channel %s intent %d",
			channel->name, liid);
		mutex_unlock(&channel->intent_lock);
		return msglen;
	}

	if (intent->size - intent->offset < chunk_size) {
		dev_err(glink->dev, "Insufficient space in intent\n");
		mutex_unlock(&channel->intent_lock);

		/* The packet header lied, drop payload */
		return msglen;
	}

	/* Read message from addr sent by WDSP */
	memcpy(intent->data + intent->offset, src, chunk_size);
	intent->offset += chunk_size;

	/* Handle message when no fragments remain to be received */
	if (!left_size) {
		glink_bgcom_send_rx_done(glink, channel, intent);

		spin_lock_irqsave(&channel->recv_lock, flags);
		if (channel->ept.cb) {
			channel->ept.cb(channel->ept.rpdev,
					intent->data,
					intent->offset,
					channel->ept.priv,
					RPMSG_ADDR_ANY);
		}
		spin_unlock_irqrestore(&channel->recv_lock, flags);

		glink_bgcom_free_intent(channel, intent);
	}
	mutex_unlock(&channel->intent_lock);

	return msglen;
}

static int glink_bgcom_handle_intent(struct glink_bgcom *glink,
				   unsigned int cid,
				   unsigned int count,
				   void *rx_data,
				   size_t avail)
{
	struct glink_bgcom_rx_intent *intent;
	struct glink_bgcom_channel *channel;
	struct intent_pair {
		__le32 size;
		__le32 iid;
		__le64 addr;
	};
	struct intent_pair *intents;
	const size_t msglen = sizeof(struct intent_pair) * count;
	int ret;
	int i;

	if (avail < msglen) {
		dev_err(glink->dev, "Not enough data in buf\n");
		return avail;
	}

	mutex_lock(&glink->idr_lock);
	channel = idr_find(&glink->rcids, cid);
	mutex_unlock(&glink->idr_lock);
	if (!channel) {
		dev_err(glink->dev, "intents for non-existing channel\n");
		return msglen;
	}

	intents = (struct intent_pair *)rx_data;
	for (i = 0; i < count; ++i) {
		intent = kzalloc(sizeof(*intent), GFP_ATOMIC);
		if (!intent)
			break;

		intent->id = le32_to_cpu(intents[i].iid);
		intent->size = le32_to_cpu(intents[i].size);
		intent->addr = (u32)le64_to_cpu(intents[i].addr);

		CH_INFO(channel, "riid:%d size:%lu\n", intent->id,
			intent->size);

		mutex_lock(&channel->intent_lock);
		ret = idr_alloc(&channel->riids, intent,
				intent->id, intent->id + 1, GFP_ATOMIC);
		mutex_unlock(&channel->intent_lock);

		if (ret < 0)
			dev_err(glink->dev, "failed to store remote intent\n");
	}

	complete(&channel->intent_alloc_comp);
	return msglen;
}

static void glink_bgcom_handle_rx_done(struct glink_bgcom *glink,
				     u32 cid, uint32_t iid,
				     bool reuse)
{
	struct glink_bgcom_rx_intent *intent;
	struct glink_bgcom_channel *channel;

	mutex_lock(&glink->idr_lock);
	channel = idr_find(&glink->rcids, cid);
	mutex_unlock(&glink->idr_lock);
	if (!channel) {
		dev_err(glink->dev, "invalid channel id received\n");
		return;
	}

	mutex_lock(&channel->intent_lock);
	intent = idr_find(&channel->riids, iid);

	if (!intent) {
		mutex_unlock(&channel->intent_lock);
		dev_err(glink->dev, "invalid intent id received\n");
		return;
	}

	intent->offset = 0;
	intent->in_use = false;
	CH_INFO(channel, "reuse:%d iid:%d\n", reuse, intent->id);

	if (!reuse) {
		idr_remove(&channel->riids, intent->id);
		kfree(intent);
	}
	mutex_unlock(&channel->intent_lock);
}

static void glink_bgcom_process_cmd(struct glink_bgcom *glink, void *rx_data,
				  u32 rx_size)
{
	struct glink_bgcom_msg *msg;
	unsigned int param1;
	unsigned int param2;
	unsigned int param3;
	unsigned int param4;
	unsigned int cmd;
	int offset = 0;
	int ret;
	u16 name_len;
	char *name;

	while (offset < rx_size) {
		msg = (struct glink_bgcom_msg *)(rx_data + offset);
		offset += sizeof(*msg);

		cmd = le16_to_cpu(msg->cmd);
		param1 = le16_to_cpu(msg->param1);
		param2 = le32_to_cpu(msg->param2);
		param3 = le32_to_cpu(msg->param3);
		param4 = le32_to_cpu(msg->param4);

		switch (cmd) {
		case BGCOM_CMD_VERSION:
			glink_bgcom_receive_version(glink, param1, param2);
			break;
		case BGCOM_CMD_VERSION_ACK:
			glink_bgcom_receive_version_ack(glink, param1, param2);
			break;
		case BGCOM_CMD_CLOSE:
		case BGCOM_CMD_CLOSE_ACK:
			glink_bgcom_rx_defer(glink,
					   rx_data + offset - sizeof(*msg),
					   rx_size + offset - sizeof(*msg), 0);
			break;
		case BGCOM_CMD_RX_INTENT_REQ:
			glink_bgcom_handle_intent_req(glink, param1, param2);
			break;
		case BGCOM_CMD_OPEN_ACK:
			ret = glink_bgcom_rx_open_ack(glink, param1);
			break;
		case BGCOM_CMD_OPEN:
			name_len = (u16)(param2 & 0xFFFF);
			name = rx_data + offset;
			glink_bgcom_rx_defer(glink,
					   rx_data + offset - sizeof(*msg),
					   rx_size + offset - sizeof(*msg),
					   ALIGN(name_len, BGCOM_ALIGNMENT));

			offset += ALIGN(name_len, BGCOM_ALIGNMENT);
			break;
		case BGCOM_CMD_TX_DATA:
		case BGCOM_CMD_TX_DATA_CONT:
			ret = glink_bgcom_rx_data(glink, param1, param2,
						rx_data + offset,
						rx_size - offset);
			offset += ALIGN(ret, BGCOM_ALIGNMENT);
			break;
		case BGCOM_CMD_TX_SHORT_DATA:
			ret = glink_bgcom_rx_short_data(glink,
						      param1, param2,
						      param3, param4,
						      rx_data + offset,
						      rx_size - offset);
			offset += ALIGN(ret, BGCOM_ALIGNMENT);
			break;
		case BGCOM_CMD_READ_NOTIF:
			break;
		case BGCOM_CMD_INTENT:
			ret = glink_bgcom_handle_intent(glink,
						      param1, param2,
						      rx_data + offset,
						      rx_size - offset);
			offset += ALIGN(ret, BGCOM_ALIGNMENT);
			break;
		case BGCOM_CMD_RX_DONE:
			glink_bgcom_handle_rx_done(glink, param1, param2,
									false);
			break;
		case BGCOM_CMD_RX_DONE_W_REUSE:
			glink_bgcom_handle_rx_done(glink, param1, param2,
									true);
			break;
		case BGCOM_CMD_RX_INTENT_REQ_ACK:
			glink_bgcom_handle_intent_req_ack(glink, param1,
								param2);
			break;
		case BGCOM_CMD_SIGNALS:
			glink_bgcom_handle_signals(glink, param1, param2);
			break;
		default:
			dev_err(glink->dev, "unhandled rx cmd: %d\n", cmd);
			break;
		}
	}
}

/**
 * __rx_worker() - Receive commands on a specific edge
 * @einfo:      Edge to process commands on.
 *
 * This function checks the size of data to be received, allocates the
 * buffer for that data and reads the data from the remote subsytem
 * into that buffer. This function then calls the glink_bgcom_process_cmd()
 * to parse the received G-Link command sequence. This function will also
 * poll for the data for a predefined duration for performance reasons.
 */
static void __rx_worker(struct rx_pkt *rx_pkt_info)
{
	struct glink_bgcom *glink = rx_pkt_info->glink;

	if (atomic_read(&glink->in_reset))
		return;

	glink_bgcom_process_cmd(glink, rx_pkt_info->rx_buf,
				rx_pkt_info->rx_len*WORD_SIZE);
	kfree(rx_pkt_info->rx_buf);
	kfree(rx_pkt_info);
}

/**
 * rx_worker() - Worker function to process received commands
 * @work:       kwork associated with the edge to process commands on.
 */
static void rx_worker(struct kthread_work *work)
{
	struct rx_pkt *rx_pkt_info;

	rx_pkt_info = container_of(work, struct rx_pkt, kwork);
	__rx_worker(rx_pkt_info);
};

static void glink_bgcom_linkup(struct glink_bgcom *glink)
{
	int ret;

	if (glink->bgcom_status != BGCOM_LINKUP)
		return;
	atomic_set(&glink->in_reset, 0);
	bgcom_reg_read(glink->bgcom_handle, BGCOM_REG_FIFO_SIZE, 1,
				&glink->fifo_size);
	mutex_lock(&glink->tx_avail_lock);
	glink->fifo_fill.tx_avail = glink->fifo_size.to_master;
	mutex_unlock(&glink->tx_avail_lock);

	ret = glink_bgcom_send_version(glink);
	if (ret)
		GLINK_ERR(glink, "Failed to link up %d\n", ret);
}

static int glink_bgcom_remove_device(struct device *dev, void *data)
{
	device_unregister(dev);

	return 0;
}

static int glink_bgcom_cleanup(struct glink_bgcom *glink)
{
	struct glink_bgcom_channel *channel;
	int cid;
	int ret;

	GLINK_INFO(glink, "\n");

	atomic_set(&glink->in_reset, 1);

	kthread_flush_worker(&glink->kworker);
	cancel_work_sync(&glink->rx_defer_work);

	ret = device_for_each_child(glink->dev, NULL,
					glink_bgcom_remove_device);
	if (ret)
		dev_warn(glink->dev, "Can't remove GLINK devices: %d\n", ret);

	mutex_lock(&glink->idr_lock);
	/* Release any defunct local channels, waiting for close-ack */
	idr_for_each_entry(&glink->lcids, channel, cid) {
		/* Wakeup threads waiting for intent*/
		complete(&channel->intent_req_comp);
		kref_put(&channel->refcount, glink_bgcom_channel_release);
		idr_remove(&glink->lcids, cid);
	}

	/* Release any defunct local channels, waiting for close-req */
	idr_for_each_entry(&glink->rcids, channel, cid) {
		kref_put(&channel->refcount, glink_bgcom_channel_release);
		idr_remove(&glink->rcids, cid);
	}

	mutex_unlock(&glink->idr_lock);

	return ret;
}

static void glink_bgcom_event_handler(void *handle,
		void *priv_data, enum bgcom_event_type event,
		union bgcom_event_data_type *data)
{
	struct glink_bgcom *glink = (struct glink_bgcom *)priv_data;
	struct rx_pkt *rx_pkt_info;

	switch (event) {
	case BGCOM_EVENT_APPLICATION_RUNNING:
		if (data->application_running &&
				glink->bgcom_status != BGCOM_LINKUP) {
			glink->bgcom_status |= BGCOM_APPLICATION_RUNNING;
			glink_bgcom_linkup(glink);
		}
		break;
	case BGCOM_EVENT_TO_SLAVE_FIFO_READY:
		if (data->to_slave_fifo_ready &&
				glink->bgcom_status != BGCOM_LINKUP) {
			glink->bgcom_status |= BGCOM_TO_SLAVE_FIFO_READY;
			glink_bgcom_linkup(glink);
		}
		break;
	case BGCOM_EVENT_TO_MASTER_FIFO_READY:
		if (data->to_master_fifo_ready &&
				glink->bgcom_status != BGCOM_LINKUP) {
			glink->bgcom_status |= BGCOM_TO_MASTER_FIFO_READY;
			glink_bgcom_linkup(glink);
		}
		break;
	case BGCOM_EVENT_AHB_READY:
		if (data->ahb_ready &&
				glink->bgcom_status != BGCOM_LINKUP) {
			glink->bgcom_status |= BGCOM_AHB_READY;
			glink_bgcom_linkup(glink);
		}
		break;
	case BGCOM_EVENT_TO_MASTER_FIFO_USED:
		rx_pkt_info = kzalloc(sizeof(struct rx_pkt), GFP_KERNEL);
		rx_pkt_info->rx_buf = data->fifo_data.data;
		rx_pkt_info->rx_len = data->fifo_data.to_master_fifo_used;
		rx_pkt_info->glink = glink;
		kthread_init_work(&rx_pkt_info->kwork, rx_worker);
		kthread_queue_work(&glink->kworker, &rx_pkt_info->kwork);
		break;
	case BGCOM_EVENT_TO_SLAVE_FIFO_FREE:
		if (glink->water_mark_reached)
			tx_wakeup_worker(glink);
		break;
	case BGCOM_EVENT_RESET_OCCURRED:
		glink->bgcom_status = BGCOM_RESET;
		glink_bgcom_cleanup(glink);
		break;
	case BGCOM_EVENT_ERROR_WRITE_FIFO_OVERRUN:
	case BGCOM_EVENT_ERROR_WRITE_FIFO_BUS_ERR:
	case BGCOM_EVENT_ERROR_WRITE_FIFO_ACCESS:
	case BGCOM_EVENT_ERROR_READ_FIFO_UNDERRUN:
	case BGCOM_EVENT_ERROR_READ_FIFO_BUS_ERR:
	case BGCOM_EVENT_ERROR_READ_FIFO_ACCESS:
	case BGCOM_EVENT_ERROR_TRUNCATED_READ:
	case BGCOM_EVENT_ERROR_TRUNCATED_WRITE:
	case BGCOM_EVENT_ERROR_AHB_ILLEGAL_ADDRESS:
	case BGCOM_EVENT_ERROR_AHB_BUS_ERR:
		GLINK_ERR(glink, "%s: ERROR %d", __func__, event);
		break;
	default:
		GLINK_ERR(glink, "%s: unhandled event %d", __func__, event);
		break;
	}
}

static int glink_bgcom_get_sigs(struct rpmsg_endpoint *ept,
			      u32 *lsigs, u32 *rsigs)
{
	struct glink_bgcom_channel *channel = to_glink_channel(ept);

	*lsigs = channel->lsigs;
	*rsigs = channel->rsigs;

	return 0;
}

static int glink_bgcom_set_sigs(struct rpmsg_endpoint *ept, u32 sigs)
{
	struct glink_bgcom_channel *channel = to_glink_channel(ept);
	struct glink_bgcom *glink = channel->glink;

	channel->lsigs = sigs;

	return glink_bgcom_send_signals(glink, channel, sigs);
}

static const struct rpmsg_endpoint_ops glink_endpoint_ops = {
	.destroy_ept = glink_bgcom_destroy_ept,
	.send = glink_bgcom_send,
	.trysend = glink_bgcom_trysend,
	.get_sigs = glink_bgcom_get_sigs,
	.set_sigs = glink_bgcom_set_sigs,
};

static void glink_bgcom_release(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct glink_bgcom *glink = platform_get_drvdata(pdev);

	kfree(glink);
}

static int glink_bgcom_probe(struct platform_device *pdev)
{
	struct glink_bgcom *glink;
	struct device *dev;
	int ret;

	glink = kzalloc(sizeof(*glink), GFP_KERNEL);
	if (!glink)
		return -ENOMEM;

	glink->dev = &pdev->dev;
	dev = glink->dev;
	dev->of_node = pdev->dev.of_node;
	dev->release = glink_bgcom_release;
	dev_set_name(dev, "%s", dev->of_node->name);
	dev_set_drvdata(dev, glink);

	ret = of_property_read_string(dev->of_node, "label", &glink->name);
	if (ret < 0)
		glink->name = dev->of_node->name;

	glink->features = GLINK_FEATURE_INTENT_REUSE;

	mutex_init(&glink->tx_lock);
	mutex_init(&glink->tx_avail_lock);
	spin_lock_init(&glink->rx_lock);
	INIT_LIST_HEAD(&glink->rx_queue);
	INIT_WORK(&glink->rx_defer_work, glink_bgcom_defer_work);

	kthread_init_worker(&glink->kworker);

	mutex_init(&glink->idr_lock);
	idr_init(&glink->lcids);
	idr_init(&glink->rcids);

	atomic_set(&glink->in_reset, 1);
	atomic_set(&glink->activity_cnt, 0);

	glink->rx_task = kthread_run(kthread_worker_fn, &glink->kworker,
				     "bgcom_%s", glink->name);
	if (IS_ERR(glink->rx_task)) {
		ret = PTR_ERR(glink->rx_task);
		dev_err(glink->dev, "kthread run failed %d\n", ret);
		goto err_put_dev;
	}

	glink->ilc = ipc_log_context_create(GLINK_LOG_PAGE_CNT, glink->name, 0);

	glink->bgcom_config.priv = (void *)glink;
	glink->bgcom_config.bgcom_notification_cb = glink_bgcom_event_handler;
	glink->bgcom_handle = NULL;
	if (!strcmp(glink->name, "bg")) {
		glink->bgcom_handle = bgcom_open(&glink->bgcom_config);
		if (!glink->bgcom_handle) {
			GLINK_ERR(glink, "%s: bgcom open failed\n", __func__);
			ret = -ENODEV;
			goto err_bg_handle;
		}
	}

	return 0;

err_bg_handle:
	kthread_stop(glink->rx_task);
err_put_dev:
	dev_set_drvdata(dev, NULL);
	put_device(dev);

	return ret;
}
EXPORT_SYMBOL(glink_bgcom_probe);

static int glink_bgcom_remove(struct platform_device *pdev)
{
	struct glink_bgcom *glink = platform_get_drvdata(pdev);
	int ret;

	GLINK_INFO(glink, "\n");

	atomic_set(&glink->in_reset, 1);

	bgcom_close(glink->bgcom_handle);

	ret = glink_bgcom_cleanup(glink);

	kthread_stop(glink->rx_task);

	mutex_lock(&glink->idr_lock);
	idr_destroy(&glink->lcids);
	idr_destroy(&glink->rcids);
	mutex_unlock(&glink->idr_lock);

	return ret;
}
EXPORT_SYMBOL(glink_bgcom_remove);

static const struct of_device_id glink_bgcom_of_match[] = {
	{ .compatible = "qcom,glink-bgcom-xprt" },
	{}
};
MODULE_DEVICE_TABLE(of, glink_bgcom_of_match);

static struct platform_driver glink_bgcom_driver = {
	.probe = glink_bgcom_probe,
	.remove = glink_bgcom_remove,
	.driver = {
		.name = "qcom_glink_bgcom",
		.of_match_table = glink_bgcom_of_match,
	},
};

static int __init glink_bgcom_init(void)
{
	return platform_driver_register(&glink_bgcom_driver);
}
postcore_initcall(glink_bgcom_init);

static void __exit glink_bgcom_exit(void)
{
	platform_driver_unregister(&glink_bgcom_driver);
}
module_exit(glink_bgcom_exit);

MODULE_DESCRIPTION("QTI GLINK BGCOM Transport");
MODULE_LICENSE("GPL v2");
