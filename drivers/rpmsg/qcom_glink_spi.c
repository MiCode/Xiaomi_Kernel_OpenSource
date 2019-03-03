/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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
#include <sound/wcd-dsp-mgr.h>
#include <sound/wcd-spi.h>
#include <linux/ipc_logging.h>

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

#define SPI_ALIGNMENT 16
#define FIFO_FULL_RESERVE 8
#define TX_BLOCKED_CMD_RESERVE 16
#define DEFAULT_FIFO_SIZE 1024
#define SHORT_SIZE 16
#define XPRT_ALIGNMENT 4

#define MAX_INACTIVE_CYCLES 50
#define POLL_INTERVAL_US 500
#define TX_WAIT_US 500

#define ACTIVE_TX BIT(0)
#define ACTIVE_RX BIT(1)

#define ID_MASK 0xFFFFFF

#define GLINK_NAME_SIZE		32
#define GLINK_VERSION_1		1

#define SPI_GLINK_CID_MIN	1
#define SPI_GLINK_CID_MAX	65536

struct glink_spi_msg {
	__le16 cmd;
	__le16 param1;
	__le32 param2;
	__le32 param3;
	__le32 param4;
	u8 data[];
} __packed;

/**
 * struct glink_spi_defer_cmd - deferred incoming control message
 * @node:	list node
 * @msg:	message header
 * data:	payload of the message
 *
 * Copy of a received control message, to be added to @rx_queue and processed
 * by @rx_work of @glink_spi.
 */
struct glink_spi_defer_cmd {
	struct list_head node;

	struct glink_spi_msg msg;
	u8 data[];
};

/**
 * struct glink_spi_rx_intent - RX intent
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
struct glink_spi_rx_intent {
	void *data;
	u32 id;
	size_t size;
	u32 addr;
	bool reuse;
	bool in_use;
	u32 offset;

	struct list_head node;
};

/**
 * @fifo_base:		Base Address of the RX FIFO.
 * @length:		End Address of the RX FIFO.
 * @tail_addr:		Address of the TX FIFO Read Index Register.
 * @head_addr:		Address of the TX FIFO Write Index Register.
 * @local_addr:		Address of the RX FIFO Read Index Register.
 */
struct glink_spi_pipe {
	u32 fifo_base;
	u32 length;

	u32 tail_addr;
	u32 head_addr;

	u32 local_addr;
};

/**
 * struct glink_cmpnt - Component to cache spi component and its operations
 * @master_dev:	Device structure corresponding to spi device.
 * @master_ops:	Operations supported by the spi device.
 */
struct glink_cmpnt {
	struct device *master_dev;
	struct wdsp_mgr_ops *master_ops;
};

/**
 * struct glink_spi - driver context, relates to one remote subsystem
 * @dev:	reference to the associated struct device
 * @name:	name of this edge
 * @rx_pipe:	pipe object for receive FIFO
 * @tx_pipe:	pipe object for transmit FIFO
 * @rx_work:	worker for handling received control messages
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
 */
struct glink_spi {
	struct device dev;

	const char *name;

	struct glink_spi_pipe rx_pipe;
	struct glink_spi_pipe tx_pipe;

	struct kthread_work rx_work;
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

	struct wcd_spi_ops spi_ops;
	struct glink_cmpnt cmpnt;
	atomic_t activity_cnt;
	atomic_t in_reset;

	void *ilc;
};

enum {
	GLINK_STATE_CLOSED,
	GLINK_STATE_OPENING,
	GLINK_STATE_OPEN,
	GLINK_STATE_CLOSING,
};

/**
 * struct glink_spi_channel - internal representation of a channel
 * @rpdev:	rpdev reference, only used for primary endpoints
 * @ept:	rpmsg endpoint this channel is associated with
 * @glink:	glink_spi context handle
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
struct glink_spi_channel {
	struct rpmsg_endpoint ept;

	struct rpmsg_device *rpdev;
	struct glink_spi *glink;

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
};

#define to_glink_channel(_ept) container_of(_ept, struct glink_spi_channel, ept)

static const struct rpmsg_endpoint_ops glink_endpoint_ops;

#define SPI_CMD_VERSION			0
#define SPI_CMD_VERSION_ACK		1
#define SPI_CMD_OPEN			2
#define SPI_CMD_CLOSE			3
#define SPI_CMD_OPEN_ACK		4
#define SPI_CMD_CLOSE_ACK		5
#define SPI_CMD_INTENT			6
#define SPI_CMD_RX_DONE			7
#define SPI_CMD_RX_DONE_W_REUSE		8
#define SPI_CMD_RX_INTENT_REQ		9
#define SPI_CMD_RX_INTENT_REQ_ACK	10
#define SPI_CMD_TX_DATA			11
#define SPI_CMD_TX_DATA_CONT		12
#define SPI_CMD_READ_NOTIF		13
#define SPI_CMD_SIGNALS			14
#define SPI_CMD_TX_SHORT_DATA		17

static void glink_spi_remove(struct glink_spi *glink);

/**
 * spi_resume() - Vote for the spi device resume
 * @cmpnt:	Component to identify the spi device.
 *
 * Return: 0 on success, standard Linux error codes on failure.
 */
static int spi_resume(struct glink_cmpnt *cmpnt)
{
	if (!cmpnt || !cmpnt->master_dev || !cmpnt->master_ops ||
	    !cmpnt->master_ops->resume)
		return 0;

	return cmpnt->master_ops->resume(cmpnt->master_dev);
}

/**
 * glink_spi_xprt_set_poll_mode() - Set the transport to polling mode
 * @glink:	Edge information corresponding to the transport.
 *
 * This helper function indicates the start of RX polling. This will
 * prevent the system from suspending and keeps polling for RX for a
 * pre-defined duration.
 */
static void glink_spi_xprt_set_poll_mode(struct glink_spi *glink)
{
	atomic_inc(&glink->activity_cnt);
	spi_resume(&glink->cmpnt);
}

/**
 * glink_spi_xprt_set_irq_mode() - Set the transport to IRQ mode
 * @glink:	Edge information corresponding to the transport.
 *
 * This helper indicates the end of RX polling. This will allow the
 * system to suspend and new RX data can be handled only through an IRQ.
 */
static void glink_spi_xprt_set_irq_mode(struct glink_spi *glink)
{
	atomic_dec(&glink->activity_cnt);
}

static struct glink_spi_channel *
glink_spi_alloc_channel(struct glink_spi *glink, const char *name)
{
	struct glink_spi_channel *channel;

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

	idr_init(&channel->liids);
	idr_init(&channel->riids);
	kref_init(&channel->refcount);

	return channel;
}

static void glink_spi_channel_release(struct kref *ref)
{
	struct glink_spi_channel *channel;
	struct glink_spi_rx_intent *tmp;
	int iid;

	channel = container_of(ref, struct glink_spi_channel, refcount);
	CH_INFO(channel, "\n");

	channel->intent_req_result = 0;
	complete(&channel->intent_req_comp);

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

/**
 * glink_spi_read() - Receive data over SPI bus
 * @glink:	Edge from which the data has to be received.
 * @src:	Source Address of the RX data.
 * @dst:	Address of the destination RX buffer.
 * @size:	Size of the RX data.
 *
 * This function is used to receive data or command as a byte stream from
 * the remote subsystem over the SPI bus.
 *
 * Return: 0 on success, standard Linux error codes on failure.
 */
static int glink_spi_read(struct glink_spi *glink, u32 src, void *dst,
			  size_t size)
{
	struct wcd_spi_msg spi_msg = { 0 };

	if (unlikely(!glink->spi_ops.read_dev))
		return -EINVAL;

	spi_msg.data = dst;
	spi_msg.remote_addr = src;
	spi_msg.len = size;
	return glink->spi_ops.read_dev(glink->spi_ops.spi_dev, &spi_msg);
}

/**
 * glink_spi_write() - Transmit data over SPI bus
 * @glink:	Edge from which the data has to be received.
 * @src:	Address of the TX buffer.
 * @dst:	Destination Address of the TX Data.
 * @size:	Size of the TX data.
 *
 * This function is used to transmit data or command as a byte stream to
 * the remote subsystem over the SPI bus.
 *
 * Return: 0 on success, standard Linux error codes on failure.
 */
static int glink_spi_write(struct glink_spi *glink, void *src, u32 dst,
			   size_t size)
{
	struct wcd_spi_msg spi_msg = { 0 };

	if (unlikely(!glink->spi_ops.write_dev))
		return -EINVAL;

	spi_msg.data = src;
	spi_msg.remote_addr = dst;
	spi_msg.len = size;
	return glink->spi_ops.write_dev(glink->spi_ops.spi_dev, &spi_msg);
}

/**
 * glink_spi_reg_read() - Read the TX/RX FIFO Read/Write Index registers
 * @glink:	Edge from which the registers have to be read.
 * @reg_addr:	Address of the register to be read.
 * @data:	Buffer into which the register data has to be read.
 *
 * Return: 0 on success, standard Linux error codes on failure.
 */
static int glink_spi_reg_read(struct glink_spi *glink, u32 reg_addr, u32 *data)
{
	int ret;

	ret = glink_spi_read(glink, reg_addr, data, sizeof(*data));
	if (ret)
		return ret;

	/* SPI register reads need to be masked */
	*data = *data & ID_MASK;
	return 0;
}

/**
 * glink_spi_reg_write() - Write the TX/RX FIFO Read/Write Index registers
 * @glink:	Edge to which the registers have to be written.
 * @reg_addr:	Address of the registers to be written.
 * @data:	Data to be written to the registers.
 *
 * Return: 0 on success, standard Linux error codes on failure.
 */
static int glink_spi_reg_write(struct glink_spi *glink, u32 reg_addr, u32 data)
{
	return glink_spi_write(glink, &data, reg_addr, sizeof(data));
}

static size_t glink_spi_rx_avail(struct glink_spi *glink)
{
	struct glink_spi_pipe *pipe = &glink->rx_pipe;
	u32 head;
	u32 tail;
	int ret;

	if (atomic_read(&glink->in_reset))
		return 0;

	if (unlikely(!pipe->fifo_base)) {
		ret = glink_spi_reg_read(glink, pipe->tail_addr,
					 &pipe->local_addr);
		if (ret < 0) {
			GLINK_ERR(glink, "Error %d reading rx tail\n", ret);
			return 0;
		}
		pipe->fifo_base = pipe->local_addr;
	}

	tail = pipe->local_addr;
	ret = glink_spi_reg_read(glink, pipe->head_addr, &head);
	if (ret < 0) {
		GLINK_ERR(glink, "Error %d reading rx head\n", ret);
		return 0;
	}

	if (head < tail)
		return pipe->length - (tail - head);
	else
		return head - tail;
}

static void glink_spi_rx_peak(struct glink_spi *glink,
			      void *data, unsigned int offset, size_t count)
{
	struct glink_spi_pipe *pipe = &glink->rx_pipe;
	u32 fifo_end;
	size_t len;
	u32 tail;

	fifo_end = pipe->fifo_base + pipe->length;
	tail = pipe->local_addr;
	tail += offset;
	if (tail >= fifo_end)
		tail -= pipe->length;

	len = min_t(size_t, count, fifo_end - tail);
	if (len)
		glink_spi_read(glink, tail, data, len);

	if (len != count)
		glink_spi_read(glink, pipe->fifo_base, data + len, count - len);
}

static void glink_spi_rx_advance(struct glink_spi *glink, size_t count)
{
	struct glink_spi_pipe *pipe = &glink->rx_pipe;
	u32 tail;
	int ret;

	tail = pipe->local_addr;
	tail += count;

	if (tail >= pipe->fifo_base + pipe->length)
		tail -= pipe->length;

	pipe->local_addr = tail;
	ret = glink_spi_reg_write(glink, pipe->tail_addr, tail);
	if (ret)
		GLINK_ERR(glink, "Error writing rx tail\n", ret);
}

static size_t glink_spi_tx_avail(struct glink_spi *glink)
{
	struct glink_spi_pipe *pipe = &glink->tx_pipe;
	u32 avail;
	u32 head;
	u32 tail;
	int ret;

	if (atomic_read(&glink->in_reset))
		return 0;

	if (unlikely(!pipe->fifo_base)) {
		ret = glink_spi_reg_read(glink, pipe->head_addr,
					 &pipe->local_addr);
		if (ret < 0) {
			GLINK_ERR(glink, "Error %d reading tx head\n", ret);
			return 0;
		}
		pipe->fifo_base = pipe->local_addr;
	}

	head = pipe->local_addr;
	ret = glink_spi_reg_read(glink, pipe->tail_addr, &tail);
	if (ret < 0) {
		GLINK_ERR(glink, "Error %d reading tx tail\n", ret);
		return 0;
	}

	if (tail <= head)
		avail = pipe->fifo_base + pipe->length - head + tail;
	else
		avail = tail - head;

	if (avail < (FIFO_FULL_RESERVE + TX_BLOCKED_CMD_RESERVE))
		avail = 0;
	else
		avail -= FIFO_FULL_RESERVE + TX_BLOCKED_CMD_RESERVE;

	return avail;
}

static unsigned int glink_spi_tx_write_one(struct glink_spi *glink, u32 head,
					   void *data, size_t count)
{
	struct glink_spi_pipe *pipe = &glink->tx_pipe;
	size_t len;
	int ret;

	len = min_t(size_t, count, pipe->fifo_base + pipe->length - head);
	if (len) {
		ret = glink_spi_write(glink, data, head, len);
		if (ret)
			GLINK_ERR(glink, "Error %d writing tx data\n", ret);
	}

	if (len != count) {
		ret = glink_spi_write(glink, data + len, pipe->fifo_base,
				      count - len);
		if (ret)
			GLINK_ERR(glink, "Error %d writing tx data\n", ret);
	}

	head += count;
	if (head >= pipe->fifo_base + pipe->length)
		head -= pipe->length;

	return head;
}

static void glink_spi_tx_write(struct glink_spi *glink, void *hdr, size_t hlen,
			       void *data, size_t dlen)
{
	struct glink_spi_pipe *pipe = &glink->tx_pipe;
	u32 head;
	int ret;

	head = pipe->local_addr;

	if (hlen)
		head = glink_spi_tx_write_one(glink, head, hdr, hlen);
	if (dlen)
		head = glink_spi_tx_write_one(glink, head, data, dlen);

	/* Ensure head is always aligned to 8 bytes */
	head = ALIGN(head, SPI_ALIGNMENT);
	if (head >= pipe->fifo_base + pipe->length)
		head -= pipe->length;

	pipe->local_addr = head;
	ret = glink_spi_reg_write(glink, pipe->head_addr, head);
	if (ret)
		GLINK_ERR(glink, "Error %d writing tx head\n", ret);

}

static int glink_spi_tx(struct glink_spi *glink, void *hdr, size_t hlen,
			void *data, size_t dlen, bool wait)
{
	unsigned int tlen = hlen + dlen;
	int ret = 0;

	if (tlen >= glink->tx_pipe.length)
		return -EINVAL;

	mutex_lock(&glink->tx_lock);

	while (glink_spi_tx_avail(glink) < tlen) {
		if (!wait) {
			ret = -EAGAIN;
			goto out;
		}

		if (atomic_read(&glink->in_reset)) {
			ret = -ENXIO;
			goto out;
		}

		/* Wait without holding the tx_lock */
		mutex_unlock(&glink->tx_lock);

		usleep_range(TX_WAIT_US, TX_WAIT_US + 50);

		mutex_lock(&glink->tx_lock);
	}

	glink_spi_tx_write(glink, hdr, hlen, data, dlen);

out:
	mutex_unlock(&glink->tx_lock);


	return ret;
}

static int glink_spi_send_version(struct glink_spi *glink)
{
	struct glink_spi_msg msg = { 0 };

	msg.cmd = cpu_to_le16(SPI_CMD_VERSION);
	msg.param1 = cpu_to_le16(GLINK_VERSION_1);
	msg.param2 = cpu_to_le32(glink->features);

	GLINK_INFO(glink, "vers:%d features:%d\n", msg.param1, msg.param2);
	return glink_spi_tx(glink, &msg, sizeof(msg), NULL, 0, true);
}

static void glink_spi_send_version_ack(struct glink_spi *glink)
{
	struct glink_spi_msg msg = { 0 };

	msg.cmd = cpu_to_le16(SPI_CMD_VERSION_ACK);
	msg.param1 = cpu_to_le16(GLINK_VERSION_1);
	msg.param2 = cpu_to_le32(glink->features);

	GLINK_INFO(glink, "vers:%d features:%d\n", msg.param1, msg.param2);
	glink_spi_tx(glink, &msg, sizeof(msg), NULL, 0, true);
}

/**
 * glink_spi_receive_version() - receive version/features from remote system
 *
 * @glink:	pointer to transport interface
 * @r_version:	remote version
 * @r_features:	remote features
 *
 * This function is called in response to a remote-initiated version/feature
 * negotiation sequence.
 */
static void glink_spi_receive_version(struct glink_spi *glink,
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
		glink_spi_send_version_ack(glink);
		break;
	}
}

/**
 * glink_spi_receive_version_ack() - receive negotiation ack from remote system
 *
 * @glink:	pointer to transport interface
 * @r_version:	remote version response
 * @r_features:	remote features response
 *
 * This function is called in response to a local-initiated version/feature
 * negotiation sequence and is the counter-offer from the remote side based
 * upon the initial version and feature set requested.
 */
static void glink_spi_receive_version_ack(struct glink_spi *glink,
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
		glink_spi_send_version(glink);
		break;
	}
}

/**
 * glink_spi_send_open_req() - send a SPI_CMD_OPEN request to the remote
 * @glink: Ptr to the glink edge
 * @channel: Ptr to the channel that the open req is sent
 *
 * Allocates a local channel id and sends a SPI_CMD_OPEN message to the remote.
 * Will return with refcount held, regardless of outcome.
 *
 * Returns 0 on success, negative errno otherwise.
 */
static int glink_spi_send_open_req(struct glink_spi *glink,
				   struct glink_spi_channel *channel)
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
	int req_len = ALIGN(sizeof(req.msg) + name_len, SPI_ALIGNMENT);
	int ret;

	kref_get(&channel->refcount);

	mutex_lock(&glink->idr_lock);
	ret = idr_alloc_cyclic(&glink->lcids, channel,
			       SPI_GLINK_CID_MIN, SPI_GLINK_CID_MAX,
			       GFP_ATOMIC);
	mutex_unlock(&glink->idr_lock);
	if (ret < 0)
		return ret;

	channel->lcid = ret;
	CH_INFO(channel, "\n");

	memset(&req, 0, sizeof(req));
	req.msg.cmd = cpu_to_le16(SPI_CMD_OPEN);
	req.msg.lcid = cpu_to_le16(channel->lcid);
	req.msg.length = cpu_to_le16(name_len);
	strlcpy(req.name, channel->name, GLINK_NAME_SIZE);

	ret = glink_spi_tx(glink, &req, req_len, NULL, 0, true);
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

static void glink_spi_send_open_ack(struct glink_spi *glink,
				    struct glink_spi_channel *channel)
{
	struct glink_spi_msg msg = { 0 };

	msg.cmd = cpu_to_le16(SPI_CMD_OPEN_ACK);
	msg.param1 = cpu_to_le16(channel->rcid);

	CH_INFO(channel, "\n");
	glink_spi_tx(glink, &msg, sizeof(msg), NULL, 0, true);
}

static int glink_spi_rx_open_ack(struct glink_spi *glink, unsigned int lcid)
{
	struct glink_spi_channel *channel;

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

static void glink_spi_send_close_req(struct glink_spi *glink,
				     struct glink_spi_channel *channel)
{
	struct glink_spi_msg req = { 0 };

	req.cmd = cpu_to_le16(SPI_CMD_CLOSE);
	req.param1 = cpu_to_le16(channel->lcid);

	CH_INFO(channel, "\n");
	glink_spi_tx(glink, &req, sizeof(req), NULL, 0, true);
}

static void glink_spi_send_close_ack(struct glink_spi *glink,
				     unsigned int rcid)
{
	struct glink_spi_msg req = { 0 };

	req.cmd = cpu_to_le16(SPI_CMD_CLOSE_ACK);
	req.param1 = cpu_to_le16(rcid);

	GLINK_INFO(glink, "rcid:%d\n", rcid);
	glink_spi_tx(glink, &req, sizeof(req), NULL, 0, true);
}

static int glink_spi_request_intent(struct glink_spi *glink,
				    struct glink_spi_channel *channel,
				    size_t size)
{
	struct glink_spi_msg req = { 0 };
	int ret;

	kref_get(&channel->refcount);
	mutex_lock(&channel->intent_req_lock);

	reinit_completion(&channel->intent_req_comp);

	req.cmd = cpu_to_le16(SPI_CMD_RX_INTENT_REQ);
	req.param1 = cpu_to_le16(channel->lcid);
	req.param2 = cpu_to_le32(size);

	CH_INFO(channel, "size:%d\n", size);

	ret = glink_spi_tx(glink, &req, sizeof(req), NULL, 0, true);
	if (ret)
		goto unlock;

	ret = wait_for_completion_timeout(&channel->intent_req_comp, 10 * HZ);
	if (!ret) {
		dev_err(&glink->dev, "intent request timed out\n");
		ret = -ETIMEDOUT;
	} else {
		ret = channel->intent_req_result ? 0 : -ECANCELED;
	}

unlock:
	mutex_unlock(&channel->intent_req_lock);
	kref_put(&channel->refcount, glink_spi_channel_release);
	return ret;
}

static int glink_spi_handle_intent(struct glink_spi *glink,
				   unsigned int cid,
				   unsigned int count,
				   void *rx_data,
				   size_t avail)
{
	struct glink_spi_rx_intent *intent;
	struct glink_spi_channel *channel;
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
		dev_err(&glink->dev, "Not enough data in buf\n");
		return avail;
	}

	mutex_lock(&glink->idr_lock);
	channel = idr_find(&glink->rcids, cid);
	mutex_unlock(&glink->idr_lock);
	if (!channel) {
		dev_err(&glink->dev, "intents for non-existing channel\n");
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

		CH_INFO(channel, "riid:%d size:%d\n", intent->id, intent->size);

		mutex_lock(&channel->intent_lock);
		ret = idr_alloc(&channel->riids, intent,
				intent->id, intent->id + 1, GFP_ATOMIC);
		mutex_unlock(&channel->intent_lock);

		if (ret < 0)
			dev_err(&glink->dev, "failed to store remote intent\n");
	}

	return msglen;
}

static void glink_spi_handle_intent_req_ack(struct glink_spi *glink,
					    unsigned int cid, bool granted)
{
	struct glink_spi_channel *channel;

	mutex_lock(&glink->idr_lock);
	channel = idr_find(&glink->rcids, cid);
	mutex_unlock(&glink->idr_lock);
	if (!channel) {
		dev_err(&glink->dev, "unable to find channel\n");
		return;
	}

	channel->intent_req_result = granted;
	complete(&channel->intent_req_comp);
	CH_INFO(channel, "\n");
}

/**
 * glink_spi_send_intent_req_ack() - convert an rx intent request ack cmd to
				      wire format and transmit
 * @glink:	The transport to transmit on.
 * @channel:	The glink channel
 * @granted:	The request response to encode.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int glink_spi_send_intent_req_ack(struct glink_spi *glink,
					 struct glink_spi_channel *channel,
					 bool granted)
{
	struct glink_spi_msg msg = { 0 };

	msg.cmd = cpu_to_le16(SPI_CMD_RX_INTENT_REQ_ACK);
	msg.param1 = cpu_to_le16(channel->lcid);
	msg.param2 = cpu_to_le32(granted);

	CH_INFO(channel, "\n");
	glink_spi_tx(glink, &msg, sizeof(msg), NULL, 0, true);

	return 0;
}

static struct glink_spi_rx_intent *
glink_spi_alloc_intent(struct glink_spi *glink,
		       struct glink_spi_channel *channel,
		       size_t size,
		       bool reuseable)
{
	struct glink_spi_rx_intent *intent;
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
 * glink_spi_advertise_intent - convert an rx intent cmd to wire format and
 *			   transmit
 * @glink:	The transport to transmit on.
 * @channel:	The local channel
 * @size:	The intent to pass on to remote.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int glink_spi_advertise_intent(struct glink_spi *glink,
				      struct glink_spi_channel *channel,
				      struct glink_spi_rx_intent *intent)
{
	struct command {
		struct glink_spi_msg msg;
		__le32 size;
		__le32 liid;
		__le64 addr;
	} __packed;
	struct command cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.msg.cmd = cpu_to_le16(SPI_CMD_INTENT);
	cmd.msg.param1 = cpu_to_le16(channel->lcid);
	cmd.msg.param2 = cpu_to_le32(1);
	cmd.size = cpu_to_le32(intent->size);
	cmd.liid = cpu_to_le32(intent->id);

	CH_INFO(channel, "count:%d size:%d liid:%d\n", 1,
		intent->size, intent->id);

	glink_spi_tx(glink, &cmd, sizeof(cmd), NULL, 0, true);

	return 0;
}

/**
 * glink_spi_handle_intent_req() - Receive a request for rx_intent
 *					    from remote side
 * if_ptr:      Pointer to the transport interface
 * rcid:	Remote channel ID
 * size:	size of the intent
 *
 * The function searches for the local channel to which the request for
 * rx_intent has arrived and allocates and notifies the remote back
 */
static void glink_spi_handle_intent_req(struct glink_spi *glink,
					u32 cid, size_t size)
{
	struct glink_spi_rx_intent *intent;
	struct glink_spi_channel *channel;

	mutex_lock(&glink->idr_lock);
	channel = idr_find(&glink->rcids, cid);
	mutex_unlock(&glink->idr_lock);

	if (!channel) {
		pr_err("%s channel not found for cid %d\n", __func__, cid);
		return;
	}

	intent = glink_spi_alloc_intent(glink, channel, size, false);
	if (intent)
		glink_spi_advertise_intent(glink, channel, intent);

	glink_spi_send_intent_req_ack(glink, channel, !!intent);
}

static int glink_spi_send_short(struct glink_spi_channel *channel,
				void *data, int len,
				struct glink_spi_rx_intent *intent, bool wait)
{
	struct glink_spi *glink = channel->glink;
	struct {
		struct glink_spi_msg msg;
		u8 data[SHORT_SIZE];
	} __packed req;

	CH_INFO(channel, "intent offset:%d len:%d\n", intent->offset, len);

	req.msg.cmd = cpu_to_le16(SPI_CMD_TX_SHORT_DATA);
	req.msg.param1 = cpu_to_le16(channel->lcid);
	req.msg.param2 = cpu_to_le32(intent->id);
	req.msg.param3 = cpu_to_le32(len);
	req.msg.param4 = cpu_to_be32(0);
	memcpy(req.data, data, len);

	mutex_lock(&glink->tx_lock);
	while (glink_spi_tx_avail(glink) < sizeof(req)) {
		if (!wait) {
			mutex_unlock(&glink->tx_lock);
			return -EAGAIN;
		}

		if (atomic_read(&glink->in_reset)) {
			mutex_unlock(&glink->tx_lock);
			return -EINVAL;
		}

		/* Wait without holding the tx_lock */
		mutex_unlock(&glink->tx_lock);

		usleep_range(TX_WAIT_US, TX_WAIT_US + 50);

		mutex_lock(&glink->tx_lock);
	}
	glink_spi_tx_write(glink, &req, sizeof(req), NULL, 0);

	mutex_unlock(&glink->tx_lock);
	return 0;
}

static int glink_spi_send_data(struct glink_spi_channel *channel,
			       void *data, int chunk_size, int left_size,
			       struct glink_spi_rx_intent *intent, bool wait)
{
	struct glink_spi *glink = channel->glink;
	struct {
		struct glink_spi_msg msg;
		__le32 chunk_size;
		__le32 left_size;
	} __packed req;

	CH_INFO(channel, "chunk:%d, left:%d\n", chunk_size, left_size);

	memset(&req, 0, sizeof(req));
	if (intent->offset)
		req.msg.cmd = cpu_to_le16(SPI_CMD_TX_DATA_CONT);
	else
		req.msg.cmd = cpu_to_le16(SPI_CMD_TX_DATA);

	req.msg.param1 = cpu_to_le16(channel->lcid);
	req.msg.param2 = cpu_to_le32(intent->id);
	req.chunk_size = cpu_to_le32(chunk_size);
	req.left_size = cpu_to_le32(left_size);

	mutex_lock(&glink->tx_lock);
	while (glink_spi_tx_avail(glink) < sizeof(req)) {
		if (!wait) {
			mutex_unlock(&glink->tx_lock);
			return -EAGAIN;
		}

		if (atomic_read(&glink->in_reset)) {
			mutex_unlock(&glink->tx_lock);
			return -EINVAL;
		}

		/* Wait without holding the tx_lock */
		mutex_unlock(&glink->tx_lock);

		usleep_range(TX_WAIT_US, TX_WAIT_US + 50);

		mutex_lock(&glink->tx_lock);
	}
	glink_spi_write(glink, data, intent->addr + intent->offset, chunk_size);
	intent->offset += chunk_size;
	glink_spi_tx_write(glink, &req, sizeof(req), NULL, 0);

	mutex_unlock(&glink->tx_lock);
	return 0;
}

static int __glink_spi_send(struct glink_spi_channel *channel,
			    void *data, int len, bool wait)
{
	struct glink_spi *glink = channel->glink;
	struct glink_spi_rx_intent *intent = NULL;
	struct glink_spi_rx_intent *tmp;
	int size = len;
	int iid = 0;
	int ret = 0;

	CH_INFO(channel, "size:%d, wait:%d\n", len, wait);

	atomic_inc(&glink->activity_cnt);
	spi_resume(&glink->cmpnt);
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

		ret = glink_spi_request_intent(glink, channel, len);
		if (ret < 0)
			goto tx_exit;
	}

	if (len <= SHORT_SIZE)
		size = 0;
	else if (size & (XPRT_ALIGNMENT - 1))
		size = ALIGN(len - SHORT_SIZE, XPRT_ALIGNMENT);

	if (size) {
		ret = glink_spi_send_data(channel, data, size, len - size,
					  intent, wait);
		if (ret)
			goto tx_exit;
	}

	data = (char *)data + size;
	size = len - size;
	if (size)
		ret = glink_spi_send_short(channel, data, size, intent, wait);

tx_exit:
	/* Mark intent available if we failed */
	if (ret && intent)
		intent->in_use = false;

	atomic_dec(&glink->activity_cnt);

	return ret;
}

static void glink_spi_handle_rx_done(struct glink_spi *glink,
				     u32 cid, uint32_t iid,
				     bool reuse)
{
	struct glink_spi_rx_intent *intent;
	struct glink_spi_channel *channel;

	mutex_lock(&glink->idr_lock);
	channel = idr_find(&glink->rcids, cid);
	mutex_unlock(&glink->idr_lock);
	if (!channel) {
		dev_err(&glink->dev, "invalid channel id received\n");
		return;
	}

	mutex_lock(&channel->intent_lock);
	intent = idr_find(&channel->riids, iid);

	if (!intent) {
		mutex_unlock(&channel->intent_lock);
		dev_err(&glink->dev, "invalid intent id received\n");
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

/**
 * glink_spi_send_rx_done() - send a rx done to remote side
 * glink:       The transport to transmit on
 * channel:	The glink channel
 * intent:	the intent to send rx done for
 *
 * This function assumes the intent lock is held
 */
static void glink_spi_send_rx_done(struct glink_spi *glink,
				   struct glink_spi_channel *channel,
				   struct glink_spi_rx_intent *intent)
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

	cmd.id = reuse ? SPI_CMD_RX_DONE_W_REUSE : SPI_CMD_RX_DONE;
	cmd.lcid = cid;
	cmd.liid = iid;
	glink_spi_tx(glink, &cmd, sizeof(cmd), NULL, 0, true);
	CH_INFO(channel, "reuse:%d liid:%d", reuse, iid);
}


/**
 * glink_spi_free_intent() - Reset and free intent if not reusuable
 * channel:	The glink channel
 * intent:	the intent to send rx done for
 *
 * This function assumes the intent lock is held
 */
static void glink_spi_free_intent(struct glink_spi_channel *channel,
				  struct glink_spi_rx_intent *intent)
{
	CH_INFO(channel, "reuse:%d liid:%d", intent->reuse, intent->id);
	intent->offset = 0;
	if (!intent->reuse) {
		idr_remove(&channel->liids, intent->id);
		kfree(intent->data);
		kfree(intent);
	}
}

/* Locally initiated rpmsg_create_ept */
static struct glink_spi_channel *glink_spi_create_local(struct glink_spi *glink,
							const char *name)
{
	struct glink_spi_channel *channel;
	int ret;

	channel = glink_spi_alloc_channel(glink, name);
	if (IS_ERR(channel))
		return ERR_CAST(channel);

	CH_INFO(channel, "\n");
	ret = glink_spi_send_open_req(glink, channel);
	if (ret)
		goto release_channel;

	ret = wait_for_completion_timeout(&channel->open_ack, 5 * HZ);
	if (!ret)
		goto err_timeout;

	ret = wait_for_completion_timeout(&channel->open_req, 5 * HZ);
	if (!ret)
		goto err_timeout;

	glink_spi_send_open_ack(glink, channel);

	return channel;

err_timeout:
	CH_INFO(channel, "err_timeout\n");

	/* glink_spi_send_open_req() did register the channel in lcids*/
	mutex_lock(&glink->idr_lock);
	idr_remove(&glink->lcids, channel->lcid);
	mutex_unlock(&glink->idr_lock);

release_channel:
	CH_INFO(channel, "release_channel\n");
	/* Release glink_spi_send_open_req() reference */
	kref_put(&channel->refcount, glink_spi_channel_release);
	/* Release glink_spi_alloc_channel() reference */
	kref_put(&channel->refcount, glink_spi_channel_release);

	return ERR_PTR(-ETIMEDOUT);
}

/* Remote initiated rpmsg_create_ept */
static int glink_spi_create_remote(struct glink_spi *glink,
				   struct glink_spi_channel *channel)
{
	int ret;

	CH_INFO(channel, "\n");

	glink_spi_send_open_ack(glink, channel);

	ret = glink_spi_send_open_req(glink, channel);
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
	 * release glink_spi_send_open_req() reference and the last reference
	 * will be release after rx_close or transport unregister by calling
	 * glink_spi_remove().
	 */
	glink_spi_send_close_req(glink, channel);

	return ret;
}

static struct rpmsg_endpoint *
glink_spi_create_ept(struct rpmsg_device *rpdev, rpmsg_rx_cb_t cb, void *priv,
		     struct rpmsg_channel_info chinfo)
{
	struct glink_spi_channel *parent = to_glink_channel(rpdev->ept);
	struct glink_spi_channel *channel;
	struct glink_spi *glink = parent->glink;
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
		channel = glink_spi_create_local(glink, name);
		if (IS_ERR(channel))
			return NULL;
	} else {
		ret = glink_spi_create_remote(glink, channel);
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

static int glink_spi_announce_create(struct rpmsg_device *rpdev)
{
	struct glink_spi_channel *channel = to_glink_channel(rpdev->ept);
	struct device_node *np = rpdev->dev.of_node;
	struct glink_spi *glink = channel->glink;
	struct glink_spi_rx_intent *intent;
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
			intent = glink_spi_alloc_intent(glink, channel, size,
							 true);
			if (!intent)
				break;

			glink_spi_advertise_intent(glink, channel, intent);
		}
	}
	return 0;
}

static void glink_spi_destroy_ept(struct rpmsg_endpoint *ept)
{
	struct glink_spi_channel *channel = to_glink_channel(ept);
	struct glink_spi *glink = channel->glink;
	unsigned long flags;

	spin_lock_irqsave(&channel->recv_lock, flags);
	channel->ept.cb = NULL;
	spin_unlock_irqrestore(&channel->recv_lock, flags);

	/* Decouple the potential rpdev from the channel */
	channel->rpdev = NULL;

	glink_spi_send_close_req(glink, channel);
}

static void glink_spi_rx_close(struct glink_spi *glink, unsigned int rcid)
{
	struct rpmsg_channel_info chinfo;
	struct glink_spi_channel *channel;

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

		rpmsg_unregister_device(&glink->dev, &chinfo);
	}

	glink_spi_send_close_ack(glink, channel->rcid);

	mutex_lock(&glink->idr_lock);
	idr_remove(&glink->rcids, channel->rcid);
	channel->rcid = 0;
	mutex_unlock(&glink->idr_lock);

	kref_put(&channel->refcount, glink_spi_channel_release);
}

static void glink_spi_rx_close_ack(struct glink_spi *glink, unsigned int lcid)
{
	struct glink_spi_channel *channel;

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

	kref_put(&channel->refcount, glink_spi_channel_release);
}

static int glink_spi_send(struct rpmsg_endpoint *ept, void *data, int len)
{
	struct glink_spi_channel *channel = to_glink_channel(ept);

	return __glink_spi_send(channel, data, len, true);
}

static int glink_spi_trysend(struct rpmsg_endpoint *ept, void *data, int len)
{
	struct glink_spi_channel *channel = to_glink_channel(ept);

	return __glink_spi_send(channel, data, len, false);
}

/**
 * glink_spi_send_signals() - convert a signal  cmd to wire format and transmit
 * @glink:	The transport to transmit on.
 * @channel:	The glink channel
 * @sigs:	The signals to encode.
 *
 * Return: 0 on success or standard Linux error code.
 */
static int glink_spi_send_signals(struct glink_spi *glink,
				  struct glink_spi_channel *channel,
				  u32 sigs)
{
	struct glink_spi_msg msg;

	msg.cmd = cpu_to_le16(SPI_CMD_SIGNALS);
	msg.param1 = cpu_to_le16(channel->lcid);
	msg.param2 = cpu_to_le32(sigs);

	GLINK_INFO(glink, "sigs:%d\n", sigs);
	return glink_spi_tx(glink, &msg, sizeof(msg), NULL, 0, true);
}

static int glink_spi_handle_signals(struct glink_spi *glink,
				    unsigned int rcid, unsigned int signals)
{
	struct glink_spi_channel *channel;
	u32 old;

	mutex_lock(&glink->idr_lock);
	channel = idr_find(&glink->rcids, rcid);
	mutex_unlock(&glink->idr_lock);
	if (!channel) {
		dev_err(&glink->dev, "signal for non-existing channel\n");
		return -EINVAL;
	}

	old = channel->rsigs;
	channel->rsigs = signals;

	if (channel->ept.sig_cb)
		channel->ept.sig_cb(channel->ept.rpdev, old, channel->rsigs);

	CH_INFO(channel, "old:%d new:%d\n", old, channel->rsigs);

	return 0;
}

static int glink_spi_get_sigs(struct rpmsg_endpoint *ept,
			      u32 *lsigs, u32 *rsigs)
{
	struct glink_spi_channel *channel = to_glink_channel(ept);

	*lsigs = channel->lsigs;
	*rsigs = channel->rsigs;

	return 0;
}

static int glink_spi_set_sigs(struct rpmsg_endpoint *ept, u32 sigs)
{
	struct glink_spi_channel *channel = to_glink_channel(ept);
	struct glink_spi *glink = channel->glink;

	channel->lsigs = sigs;

	return glink_spi_send_signals(glink, channel, sigs);
}

/*
 * Finds the device_node for the glink child interested in this channel.
 */
static struct device_node *glink_spi_match_channel(struct device_node *node,
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

static const struct rpmsg_device_ops glink_device_ops = {
	.create_ept = glink_spi_create_ept,
	.announce_create = glink_spi_announce_create,
};

static const struct rpmsg_endpoint_ops glink_endpoint_ops = {
	.destroy_ept = glink_spi_destroy_ept,
	.send = glink_spi_send,
	.trysend = glink_spi_trysend,
	.get_sigs = glink_spi_get_sigs,
	.set_sigs = glink_spi_set_sigs,
};

static void glink_spi_rpdev_release(struct device *dev)
{
	struct rpmsg_device *rpdev = to_rpmsg_device(dev);
	struct glink_spi_channel *channel = to_glink_channel(rpdev->ept);

	channel->rpdev = NULL;
	kfree(rpdev);
}

static int glink_spi_rx_open(struct glink_spi *glink, unsigned int rcid,
			     char *name)
{
	struct glink_spi_channel *channel;
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
		channel = glink_spi_alloc_channel(glink, name);
		if (IS_ERR(channel))
			return PTR_ERR(channel);

		/* The opening dance was initiated by the remote */
		create_device = true;
	}

	mutex_lock(&glink->idr_lock);
	ret = idr_alloc(&glink->rcids, channel, rcid, rcid + 1, GFP_ATOMIC);
	if (ret < 0) {
		dev_err(&glink->dev, "Unable to insert channel into rcid list\n");
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

		node = glink_spi_match_channel(glink->dev.of_node, name);
		rpdev->dev.of_node = node;
		rpdev->dev.parent = &glink->dev;
		rpdev->dev.release = glink_spi_rpdev_release;

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
		kref_put(&channel->refcount, glink_spi_channel_release);

	return ret;
}

static int glink_spi_rx_data(struct glink_spi *glink,
			     unsigned int rcid, unsigned int liid,
			     void *rx_data, size_t avail)
{
	struct glink_spi_rx_intent *intent;
	struct glink_spi_channel *channel;
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

	msglen = sizeof(*hdr);
	if (avail < msglen) {
		dev_dbg(&glink->dev, "Not enough data in fifo\n");
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
		dev_dbg(&glink->dev, "Data on non-existing channel\n");
		return msglen;
	}
	CH_INFO(channel, "chunk_size:%d left_size:%d\n", chunk_size, left_size);

	mutex_lock(&channel->intent_lock);
	intent = idr_find(&channel->liids, liid);

	if (!intent) {
		dev_err(&glink->dev,
			"no intent found for channel %s intent %d",
			channel->name, liid);
		mutex_unlock(&channel->intent_lock);

		return msglen;
	}

	if (intent->size - intent->offset < chunk_size) {
		dev_err(&glink->dev, "Insufficient space in intent\n");
		mutex_unlock(&channel->intent_lock);

		/* The packet header lied, drop payload */
		return msglen;
	}

	/* Read message from addr sent by WDSP */
	glink_spi_read(glink, addr, intent->data + intent->offset, chunk_size);
	intent->offset += chunk_size;

	/* Handle message when no fragments remain to be received */
	if (!left_size) {
		glink_spi_send_rx_done(glink, channel, intent);

		spin_lock_irqsave(&channel->recv_lock, flags);
		if (channel->ept.cb) {
			channel->ept.cb(channel->ept.rpdev,
					intent->data,
					intent->offset,
					channel->ept.priv,
					RPMSG_ADDR_ANY);
		}
		spin_unlock_irqrestore(&channel->recv_lock, flags);

		glink_spi_free_intent(channel, intent);
	}
	mutex_unlock(&channel->intent_lock);

	return msglen;
}

static int glink_spi_rx_short_data(struct glink_spi *glink,
				   unsigned int rcid, unsigned int liid,
				   unsigned int chunk_size,
				   unsigned int left_size,
				   void *src, size_t avail)
{
	struct glink_spi_rx_intent *intent;
	struct glink_spi_channel *channel;
	size_t msglen = SHORT_SIZE;
	unsigned long flags;

	if (avail < msglen) {
		dev_dbg(&glink->dev, "Not enough data in fifo\n");
		return avail;
	}
	mutex_lock(&glink->idr_lock);
	channel = idr_find(&glink->rcids, rcid);
	mutex_unlock(&glink->idr_lock);
	if (!channel) {
		dev_dbg(&glink->dev, "Data on non-existing channel\n");
		return msglen;
	}
	CH_INFO(channel, "chunk_size:%d left_size:%d\n", chunk_size, left_size);

	mutex_lock(&channel->intent_lock);
	intent = idr_find(&channel->liids, liid);

	if (!intent) {
		dev_err(&glink->dev,
			"no intent found for channel %s intent %d",
			channel->name, liid);
		mutex_unlock(&channel->intent_lock);
		return msglen;
	}

	if (intent->size - intent->offset < chunk_size) {
		dev_err(&glink->dev, "Insufficient space in intent\n");
		mutex_unlock(&channel->intent_lock);

		/* The packet header lied, drop payload */
		return msglen;
	}

	/* Read message from addr sent by WDSP */
	memcpy(intent->data + intent->offset, src, chunk_size);
	intent->offset += chunk_size;

	/* Handle message when no fragments remain to be received */
	if (!left_size) {
		glink_spi_send_rx_done(glink, channel, intent);

		spin_lock_irqsave(&channel->recv_lock, flags);
		if (channel->ept.cb) {
			channel->ept.cb(channel->ept.rpdev,
					intent->data,
					intent->offset,
					channel->ept.priv,
					RPMSG_ADDR_ANY);
		}
		spin_unlock_irqrestore(&channel->recv_lock, flags);

		glink_spi_free_intent(channel, intent);
	}
	mutex_unlock(&channel->intent_lock);

	return msglen;
}

static void glink_spi_defer_work(struct work_struct *work)
{
	struct glink_spi *glink = container_of(work, struct glink_spi,
					       rx_defer_work);

	struct glink_spi_defer_cmd *dcmd;
	struct glink_spi_msg *msg;
	unsigned long flags;
	unsigned int param1;
	unsigned int param2;
	unsigned int param3;
	unsigned int param4;
	unsigned int cmd;

	atomic_inc(&glink->activity_cnt);
	spi_resume(&glink->cmpnt);
	for (;;) {
		spin_lock_irqsave(&glink->rx_lock, flags);
		if (list_empty(&glink->rx_queue)) {
			spin_unlock_irqrestore(&glink->rx_lock, flags);
			break;
		}
		dcmd = list_first_entry(&glink->rx_queue,
					struct glink_spi_defer_cmd, node);
		list_del(&dcmd->node);
		spin_unlock_irqrestore(&glink->rx_lock, flags);

		msg = &dcmd->msg;
		cmd = le16_to_cpu(msg->cmd);
		param1 = le16_to_cpu(msg->param1);
		param2 = le32_to_cpu(msg->param2);
		param3 = le32_to_cpu(msg->param3);
		param4 = le32_to_cpu(msg->param4);

		switch (cmd) {
		case SPI_CMD_OPEN:
			glink_spi_rx_open(glink, param1, msg->data);
			break;
		case SPI_CMD_CLOSE:
			glink_spi_rx_close(glink, param1);
			break;
		case SPI_CMD_CLOSE_ACK:
			glink_spi_rx_close_ack(glink, param1);
			break;
		default:
			WARN(1, "Unknown defer object %d\n", cmd);
			break;
		}

		kfree(dcmd);
	}
	atomic_dec(&glink->activity_cnt);
}

static int glink_spi_rx_defer(struct glink_spi *glink,
			      void *rx_data, u32 rx_avail, size_t extra)
{
	struct glink_spi_defer_cmd *dcmd;

	extra = ALIGN(extra, SPI_ALIGNMENT);

	if (rx_avail < sizeof(struct glink_spi_msg) + extra) {
		dev_dbg(&glink->dev, "Insufficient data in rx fifo");
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

static void glink_spi_process_cmd(struct glink_spi *glink, void *rx_data,
				  u32 rx_size)
{
	struct glink_spi_msg *msg;
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
		msg = (struct glink_spi_msg *)(rx_data + offset);
		offset += sizeof(*msg);

		cmd = le16_to_cpu(msg->cmd);
		param1 = le16_to_cpu(msg->param1);
		param2 = le32_to_cpu(msg->param2);
		param3 = le32_to_cpu(msg->param3);
		param4 = le32_to_cpu(msg->param4);

		switch (cmd) {
		case SPI_CMD_VERSION:
			if (param3) {
				glink->rx_pipe.length = param3;
				glink->tx_pipe.length = param3;
			}
			glink_spi_receive_version(glink, param1, param2);
			break;
		case SPI_CMD_VERSION_ACK:
			glink_spi_receive_version_ack(glink, param1, param2);
			break;
		case SPI_CMD_CLOSE:
		case SPI_CMD_CLOSE_ACK:
			glink_spi_rx_defer(glink,
					   rx_data + offset - sizeof(*msg),
					   rx_size + offset - sizeof(*msg), 0);
			break;
		case SPI_CMD_RX_INTENT_REQ:
			glink_spi_handle_intent_req(glink, param1, param2);
			break;
		case SPI_CMD_OPEN_ACK:
			ret = glink_spi_rx_open_ack(glink, param1);
			break;
		case SPI_CMD_OPEN:
			name_len = (u16)(param2 & 0xFFFF);
			name = rx_data + offset;
			glink_spi_rx_defer(glink,
					   rx_data + offset - sizeof(*msg),
					   rx_size + offset - sizeof(*msg),
					   ALIGN(name_len, SPI_ALIGNMENT));

			offset += ALIGN(name_len, SPI_ALIGNMENT);
			break;
		case SPI_CMD_TX_DATA:
		case SPI_CMD_TX_DATA_CONT:
			ret = glink_spi_rx_data(glink, param1, param2,
						rx_data + offset,
						rx_size - offset);
			offset += ALIGN(ret, SPI_ALIGNMENT);
			break;
		case SPI_CMD_TX_SHORT_DATA:
			ret = glink_spi_rx_short_data(glink,
						      param1, param2,
						      param3, param4,
						      rx_data + offset,
						      rx_size - offset);
			offset += ALIGN(ret, SPI_ALIGNMENT);
			break;
		case SPI_CMD_READ_NOTIF:
			break;
		case SPI_CMD_INTENT:
			ret = glink_spi_handle_intent(glink,
						      param1, param2,
						      rx_data + offset,
						      rx_size - offset);
			offset += ALIGN(ret, SPI_ALIGNMENT);
			break;
		case SPI_CMD_RX_DONE:
			glink_spi_handle_rx_done(glink, param1, param2, false);
			break;
		case SPI_CMD_RX_DONE_W_REUSE:
			glink_spi_handle_rx_done(glink, param1, param2, true);
			break;
		case SPI_CMD_RX_INTENT_REQ_ACK:
			glink_spi_handle_intent_req_ack(glink, param1, param2);
			break;
		case SPI_CMD_SIGNALS:
			glink_spi_handle_signals(glink, param1, param2);
			break;
		default:
			dev_err(&glink->dev, "unhandled rx cmd: %d\n", cmd);
			break;
		}
	}
}

static void glink_spi_work(struct kthread_work *work)
{
	struct glink_spi *glink = container_of(work, struct glink_spi,
					       rx_work);
	u32 inactive_cycles = 0;
	u32 rx_avail;
	void *rx_data;

	glink_spi_xprt_set_poll_mode(glink);
	do {
		rx_avail = glink_spi_rx_avail(glink);
		if (!rx_avail) {
			usleep_range(POLL_INTERVAL_US, POLL_INTERVAL_US + 50);
			inactive_cycles++;
			continue;
		}
		inactive_cycles = 0;

		rx_data = kzalloc(rx_avail, GFP_KERNEL);
		if (!rx_data)
			break;

		glink_spi_rx_peak(glink, rx_data, 0, rx_avail);
		glink_spi_process_cmd(glink, rx_data, rx_avail);
		kfree(rx_data);
		glink_spi_rx_advance(glink, rx_avail);

	} while (inactive_cycles < MAX_INACTIVE_CYCLES &&
		 !atomic_read(&glink->in_reset));
	glink_spi_xprt_set_irq_mode(glink);
}

static int glink_spi_cmpnt_init(struct device *dev, void *priv)
{
	return 0;
}

static int glink_spi_cmpnt_deinit(struct device *dev, void *priv)
{
	return 0;
}

static int glink_spi_cmpnt_event_handler(struct device *dev, void *priv,
					 enum wdsp_event_type event,
					 void *data)
{
	struct glink_spi *glink = dev_get_drvdata(dev);
	struct glink_cmpnt *cmpnt = &glink->cmpnt;
	int ret = 0;

	switch (event) {
	case WDSP_EVENT_PRE_BOOTUP:
		if (!cmpnt || !cmpnt->master_dev || !cmpnt->master_ops ||
		    !cmpnt->master_ops->get_devops_for_cmpnt)
			break;

		ret = cmpnt->master_ops->get_devops_for_cmpnt(cmpnt->master_dev,
				WDSP_CMPNT_TRANSPORT, &glink->spi_ops);
		if (ret)
			GLINK_ERR(glink, "Failed to get transport device\n");
		break;
	case WDSP_EVENT_POST_BOOTUP:
		atomic_set(&glink->in_reset, 0);
		ret = glink_spi_send_version(glink);
		if (ret)
			GLINK_ERR(glink, "failed to send version %d\n", ret);

		/* FALLTHROUGH */
	case WDSP_EVENT_IPC1_INTR:
		kthread_queue_work(&glink->rx_worker, &glink->rx_work);
		break;
	case WDSP_EVENT_PRE_SHUTDOWN:
		glink_spi_remove(glink);
		break;
	case WDSP_EVENT_RESUME:
		break;
	case WDSP_EVENT_SUSPEND:
		if (atomic_read(&glink->activity_cnt))
			ret = -EBUSY;
		break;
	default:
		GLINK_INFO(glink, "unhandled event %d", event);
		break;
	}

	return ret;
}

/* glink_spi_cmpnt_ops - Callback operations registered wtih wdsp framework */
static struct wdsp_cmpnt_ops glink_spi_cmpnt_ops = {
	.init = glink_spi_cmpnt_init,
	.deinit = glink_spi_cmpnt_deinit,
	.event_handler = glink_spi_cmpnt_event_handler,
};

static int glink_component_bind(struct device *dev, struct device *master,
				void *data)
{
	struct glink_spi *glink = dev_get_drvdata(dev);
	struct glink_cmpnt *cmpnt = &glink->cmpnt;
	int ret = 0;

	cmpnt->master_dev = master;
	cmpnt->master_ops = data;

	if (cmpnt->master_ops && cmpnt->master_ops->register_cmpnt_ops)
		ret = cmpnt->master_ops->register_cmpnt_ops(master, dev, glink,
							&glink_spi_cmpnt_ops);
	else
		ret = -EINVAL;

	if (ret)
		dev_err(dev, "%s: register_cmpnt_ops failed, err = %d\n",
			__func__, ret);
	return ret;
}

static void glink_component_unbind(struct device *dev, struct device *master,
				   void *data)
{
	struct glink_spi *glink = dev_get_drvdata(dev);
	struct glink_cmpnt *cmpnt = &glink->cmpnt;

	cmpnt->master_dev = NULL;
	cmpnt->master_ops = NULL;
}

static const struct component_ops glink_component_ops = {
	.bind = glink_component_bind,
	.unbind = glink_component_unbind,
};

static int glink_spi_init_pipe(const char *key, struct device_node *node,
			       struct glink_spi_pipe *pipe)
{
	const struct property *prop = NULL;
	__be32 *addrs;

	prop = of_find_property(node, key, NULL);
	if (!prop) {
		pr_err("%s failed to find prop %s", __func__, key);
		return -ENODEV;
	}

	if ((prop->length / sizeof(u32)) != 2) {
		pr_err("%s %s wrong length %d", __func__, key, prop->length);
		return -EINVAL;
	}
	addrs = prop->value;

	pipe->tail_addr = be32_to_cpup(addrs++);
	pipe->head_addr = be32_to_cpup(addrs++);
	pipe->length = DEFAULT_FIFO_SIZE;

	return 0;
}

static void glink_spi_release(struct device *dev)
{
	struct glink_spi *glink = container_of(dev, struct glink_spi, dev);

	kfree(glink);
}

struct glink_spi *qcom_glink_spi_register(struct device *parent,
					  struct device_node *node)
{
	struct glink_spi *glink;
	struct device *dev;
	int ret;

	glink = kzalloc(sizeof(*glink), GFP_KERNEL);
	if (!glink)
		return ERR_PTR(-ENOMEM);

	dev = &glink->dev;
	dev->parent = parent;
	dev->of_node = node;
	dev->release = glink_spi_release;
	dev_set_name(dev, "%s:%s", node->parent->name, node->name);
	ret = device_register(dev);
	if (ret) {
		pr_err("failed to register glink edge\n");
		return ERR_PTR(ret);
	}
	dev_set_drvdata(dev, glink);

	ret = of_property_read_string(dev->of_node, "label", &glink->name);
	if (ret < 0)
		glink->name = dev->of_node->name;

	glink->features = GLINK_FEATURE_INTENT_REUSE;

	mutex_init(&glink->tx_lock);
	spin_lock_init(&glink->rx_lock);
	INIT_LIST_HEAD(&glink->rx_queue);
	INIT_WORK(&glink->rx_defer_work, glink_spi_defer_work);

	kthread_init_work(&glink->rx_work, glink_spi_work);
	kthread_init_worker(&glink->rx_worker);

	mutex_init(&glink->idr_lock);
	idr_init(&glink->lcids);
	idr_init(&glink->rcids);

	atomic_set(&glink->in_reset, 1);
	atomic_set(&glink->activity_cnt, 0);

	ret = glink_spi_init_pipe("tx-descriptors", node, &glink->tx_pipe);
	if (ret)
		goto err_put_dev;

	ret = glink_spi_init_pipe("rx-descriptors", node, &glink->rx_pipe);
	if (ret)
		goto err_put_dev;

	ret = component_add(dev, &glink_component_ops);
	if (ret) {
		dev_err(dev, "component_add failed, err = %d\n", ret);
		goto err_put_dev;
	}

	glink->ilc = ipc_log_context_create(GLINK_LOG_PAGE_CNT, glink->name, 0);

	glink->rx_task = kthread_run(kthread_worker_fn, &glink->rx_worker,
				     "spi_%s", glink->name);
	if (IS_ERR(glink->rx_task)) {
		ret = PTR_ERR(glink->rx_task);
		dev_err(dev, "kthread run failed %d\n", ret);
		goto err_put_dev;
	}

	return glink;

err_put_dev:
	dev_set_drvdata(dev, NULL);
	put_device(dev);

	return ERR_PTR(ret);
}
EXPORT_SYMBOL(qcom_glink_spi_register);

static int glink_spi_remove_device(struct device *dev, void *data)
{
	device_unregister(dev);

	return 0;
}

static void glink_spi_remove(struct glink_spi *glink)
{
	struct glink_spi_pipe *rx_pipe = &glink->rx_pipe;
	struct glink_spi_pipe *tx_pipe = &glink->tx_pipe;
	struct glink_spi_channel *channel;
	int cid;
	int ret;

	GLINK_INFO(glink, "\n");

	atomic_set(&glink->in_reset, 1);
	kthread_cancel_work_sync(&glink->rx_work);
	cancel_work_sync(&glink->rx_defer_work);

	ret = device_for_each_child(&glink->dev, NULL, glink_spi_remove_device);
	if (ret)
		dev_warn(&glink->dev, "Can't remove GLINK devices: %d\n", ret);

	mutex_lock(&glink->idr_lock);
	/* Release any defunct local channels, waiting for close-ack */
	idr_for_each_entry(&glink->lcids, channel, cid) {
		/* Wakeup threads waiting for intent*/
		complete(&channel->intent_req_comp);
		kref_put(&channel->refcount, glink_spi_channel_release);
		idr_remove(&glink->lcids, cid);
	}

	/* Release any defunct local channels, waiting for close-req */
	idr_for_each_entry(&glink->rcids, channel, cid) {
		kref_put(&channel->refcount, glink_spi_channel_release);
		idr_remove(&glink->rcids, cid);
	}

	idr_destroy(&glink->lcids);
	idr_destroy(&glink->rcids);
	mutex_unlock(&glink->idr_lock);

	tx_pipe->fifo_base = 0;
	tx_pipe->local_addr = 0;
	tx_pipe->length = DEFAULT_FIFO_SIZE;

	rx_pipe->fifo_base = 0;
	rx_pipe->local_addr = 0;
	rx_pipe->length = DEFAULT_FIFO_SIZE;
}

void qcom_glink_spi_unregister(struct glink_spi *glink)
{
	device_unregister(&glink->dev);
}
EXPORT_SYMBOL(qcom_glink_spi_unregister);

MODULE_DESCRIPTION("QTI GLINK SPI Transport");
MODULE_LICENSE("GPL v2");
