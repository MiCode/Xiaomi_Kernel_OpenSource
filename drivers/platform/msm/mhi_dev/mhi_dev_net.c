// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.*/

/*
 * MHI Device Network interface
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ip.h>
#include <linux/if_ether.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/netdevice.h>
#include <linux/dma-mapping.h>
#include <linux/ipc_logging.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/ktime.h>
#include <linux/platform_device.h>
#include <linux/etherdevice.h>
#include <linux/of.h>
#include <linux/list.h>

#include "mhi.h"

#define MHI_NET_DRIVER_NAME  "mhi_dev_net_drv"
#define MHI_NET_DEV_NAME     "mhi_swip%d"
#define MHI_NET_DEFAULT_MTU   8192
#define MHI_NET_IPC_PAGES     (100)
#define MHI_MAX_RX_REQ        (128)
#define MHI_MAX_TX_REQ        (128)

enum mhi_dev_net_dbg_lvl {
	MHI_VERBOSE = 0x1,
	MHI_INFO = 0x2,
	MHI_DBG = 0x3,
	MHI_WARNING = 0x4,
	MHI_ERROR = 0x5,
	MHI_CRITICAL = 0x6,
	MSG_NET_reserved = 0x80000000
};

static enum mhi_dev_net_dbg_lvl mhi_net_msg_lvl = MHI_CRITICAL;
static enum mhi_dev_net_dbg_lvl mhi_net_ipc_log_lvl = MHI_VERBOSE;
static void *mhi_net_ipc_log;

enum mhi_chan_dir {
	MHI_DIR_INVALID = 0x0,
	MHI_DIR_OUT = 0x1,
	MHI_DIR_IN = 0x2,
	MHI_DIR__reserved = 0x80000000
};

struct mhi_dev_net_chan_attr {
	/* SW maintained channel id */
	enum mhi_client_channel chan_id;
	/* maximum buffer size for this channel */
	size_t max_packet_size;
	/* direction of the channel, see enum mhi_chan_dir */
	enum mhi_chan_dir dir;
};

#define CHAN_TO_CLIENT(_CHAN_NR) (_CHAN_NR / 2)

#define mhi_dev_net_log(_msg_lvl, _msg, ...) do { \
	if (_msg_lvl >= mhi_net_msg_lvl) { \
		pr_err("[%s] "_msg, __func__, ##__VA_ARGS__); \
	} \
	if (mhi_net_ipc_log && (_msg_lvl >= mhi_net_ipc_log_lvl)) { \
		ipc_log_string(mhi_net_ipc_log,                     \
			"[%s] " _msg, __func__, ##__VA_ARGS__);     \
	} \
} while (0)

struct mhi_dev_net_client {
	/* write channel - always even*/
	u32 out_chan;
	/* read channel - always odd */
	u32 in_chan;
	bool eth_iface;
	struct mhi_dev_client *out_handle;
	struct mhi_dev_client *in_handle;
	/* TX and RX Reqs  */
	u32 tx_reqs;
	u32 rx_reqs;
	/*process pendig packets */
	struct workqueue_struct *pending_pckt_wq;
	struct work_struct       xmit_work;
	/*Read data from host work queue*/
	atomic_t  rx_enabled;
	atomic_t  tx_enabled;
	struct net_device *dev;
	struct sk_buff_head tx_buffers;
	struct list_head rx_buffers;
	struct list_head wr_req_buffers;
	struct mhi_dev_net_ctxt *net_ctxt;
	/*To check write channel is empty or not*/
	spinlock_t wrt_lock;
	spinlock_t rd_lock;
	struct mutex in_chan_lock;
	struct mutex out_chan_lock;
};

struct mhi_dev_net_ctxt {
	struct mhi_dev_net_chan_attr chan_attr[MHI_MAX_SOFTWARE_CHANNELS];
	struct mhi_dev_net_client *client_handle;
	struct platform_device		*pdev;
	void (*net_event_notifier)(struct mhi_dev_client_cb_reason *cb);
	struct mhi_dev_ops *dev_ops;
};

static struct mhi_dev_net_ctxt mhi_net_ctxt;
static ssize_t mhi_dev_net_client_read(struct mhi_dev_net_client *);

static int mhi_dev_net_init_ch_attributes(struct mhi_dev_net_ctxt *mhi_ctxt)
{
	u32 channel = 0;
	struct mhi_dev_net_chan_attr *chan_attrib = NULL;

	channel = MHI_CLIENT_IP_SW_4_OUT;
	chan_attrib = &mhi_ctxt->chan_attr[channel];
	chan_attrib->dir = MHI_DIR_OUT;
	chan_attrib->chan_id = channel;
	chan_attrib->max_packet_size = TRB_MAX_DATA_SIZE;
	mhi_dev_net_log(MHI_INFO, "Write chan attributes dir %d chan_id %d\n",
			chan_attrib->dir, chan_attrib->chan_id);

	channel = MHI_CLIENT_IP_SW_4_IN;
	chan_attrib = &mhi_ctxt->chan_attr[channel];
	chan_attrib->dir = MHI_DIR_IN;
	chan_attrib->chan_id = channel;
	chan_attrib->max_packet_size = TRB_MAX_DATA_SIZE;
	mhi_dev_net_log(MHI_INFO, "Read chan attributes dir %d chan_id %d\n",
			chan_attrib->dir, chan_attrib->chan_id);
	return 0;
}

static void mhi_dev_net_process_queue_packets(struct work_struct *work)
{
	struct mhi_dev_net_client *client = container_of(work,
			struct mhi_dev_net_client, xmit_work);
	unsigned long flags = 0;
	int xfer_data = 0;
	struct sk_buff *skb = NULL;
	struct mhi_req *wreq = NULL;

	if (mhi_net_ctxt.dev_ops->is_channel_empty(client->in_handle)) {
		mhi_dev_net_log(MHI_INFO, "%s stop network xmmit\n", __func__);
		netif_stop_queue(client->dev);
		return;
	}
	while (!((skb_queue_empty(&client->tx_buffers)) ||
			(list_empty(&client->wr_req_buffers)))) {
		spin_lock_irqsave(&client->wrt_lock, flags);
		skb = skb_dequeue(&(client->tx_buffers));
		if (!skb) {
			mhi_dev_net_log(MHI_INFO,
					"SKB is NULL from dequeue\n");
			spin_unlock_irqrestore(&client->wrt_lock, flags);
			return;
		}
		wreq = container_of(client->wr_req_buffers.next,
				struct mhi_req, list);
		list_del_init(&wreq->list);

		wreq->client = client->in_handle;
		wreq->context = skb;
		wreq->buf = skb->data;
		wreq->len = skb->len;
		wreq->chan = client->in_chan;
		wreq->mode = DMA_ASYNC;
		if (skb_queue_empty(&client->tx_buffers) ||
				list_empty(&client->wr_req_buffers)) {
			wreq->snd_cmpl = 1;
		} else
			wreq->snd_cmpl = 0;
		spin_unlock_irqrestore(&client->wrt_lock, flags);
		xfer_data = mhi_net_ctxt.dev_ops->write_channel(wreq);
		if (xfer_data <= 0) {
			pr_err("%s(): Failed to write skb len %d\n",
					__func__, skb->len);
			kfree_skb(skb);
			return;
		}
		client->dev->stats.tx_packets++;

		/* Check if free buffers are available*/
		if (mhi_net_ctxt.dev_ops->is_channel_empty(client->in_handle)) {
			mhi_dev_net_log(MHI_INFO,
					"%s buffers are full stop xmit\n",
					__func__);
			netif_stop_queue(client->dev);
			break;
		}
	} /* While TX queue is not empty */
}

static void mhi_dev_net_event_notifier(struct mhi_dev_client_cb_reason *reason)
{
	struct mhi_dev_net_client *client_handle = mhi_net_ctxt.client_handle;

	if (reason->reason == MHI_DEV_TRE_AVAILABLE) {
		if (reason->ch_id % 2) {
			if (netif_queue_stopped(client_handle->dev)) {
				netif_wake_queue(client_handle->dev);
				queue_work(client_handle->pending_pckt_wq,
						&client_handle->xmit_work);
			}
		} else
			mhi_dev_net_client_read(client_handle);
	}
}

static __be16 mhi_dev_net_eth_type_trans(struct sk_buff *skb)
{
	__be16 protocol = 0;
	/* Determine L3 protocol */
	switch (skb->data[0] & 0xf0) {
	case 0x40:
		protocol = htons(ETH_P_IP);
		break;
	case 0x60:
		protocol = htons(ETH_P_IPV6);
		break;
	default:
		/* Default is QMAP */
		protocol = htons(ETH_P_MAP);
		break;
	}
	return protocol;
}

static void mhi_dev_net_read_completion_cb(void *req)
{
	struct mhi_dev_net_client *net_handle =
		mhi_net_ctxt.client_handle;
	struct mhi_req *mreq =
		(struct mhi_req *)req;
	struct sk_buff *skb = mreq->context;
	unsigned long   flags;

	skb_put(skb, mreq->transfer_len);

	if (net_handle->eth_iface)
		skb->protocol = eth_type_trans(skb, net_handle->dev);
	else
		skb->protocol = mhi_dev_net_eth_type_trans(skb);

	net_handle->dev->stats.rx_packets++;
	skb->dev = net_handle->dev;
	netif_rx(skb);
	spin_lock_irqsave(&net_handle->rd_lock, flags);
	list_add_tail(&mreq->list, &net_handle->rx_buffers);
	spin_unlock_irqrestore(&net_handle->rd_lock, flags);
}

static ssize_t mhi_dev_net_client_read(struct mhi_dev_net_client *mhi_handle)
{
	int bytes_avail = 0;
	int ret_val = 0;
	u32 chan = 0;
	struct mhi_dev_client *client_handle = NULL;
	struct mhi_req *req;
	struct sk_buff *skb;
	unsigned long   flags;

	client_handle = mhi_handle->out_handle;
	chan = mhi_handle->out_chan;
	if (!atomic_read(&mhi_handle->rx_enabled))
		return -EPERM;
	while (1) {
		spin_lock_irqsave(&mhi_handle->rd_lock, flags);
		if (list_empty(&mhi_handle->rx_buffers)) {
			spin_unlock_irqrestore(&mhi_handle->rd_lock, flags);
			break;
		}

		req = container_of(mhi_handle->rx_buffers.next,
				struct mhi_req, list);
		list_del_init(&req->list);
		spin_unlock_irqrestore(&mhi_handle->rd_lock, flags);
		skb = alloc_skb(MHI_NET_DEFAULT_MTU, GFP_KERNEL);
		if (skb == NULL) {
			pr_err("%s(): skb alloc failed\n", __func__);
			spin_lock_irqsave(&mhi_handle->rd_lock, flags);
			list_add_tail(&req->list, &mhi_handle->rx_buffers);
			spin_unlock_irqrestore(&mhi_handle->rd_lock, flags);
			ret_val = -ENOMEM;
			return ret_val;
		}

		req->client = client_handle;
		req->chan = chan;
		req->buf = skb->data;
		req->len = MHI_NET_DEFAULT_MTU;
		req->context = skb;
		req->mode = DMA_ASYNC;
		req->snd_cmpl = 0;
		bytes_avail = mhi_net_ctxt.dev_ops->read_channel(req);

		if (bytes_avail < 0) {
			pr_err("Failed to read chan %d bytes_avail = %d\n",
					chan, bytes_avail);
			spin_lock_irqsave(&mhi_handle->rd_lock, flags);
			kfree_skb(skb);
			list_add_tail(&req->list, &mhi_handle->rx_buffers);
			spin_unlock_irqrestore(&mhi_handle->rd_lock, flags);
			ret_val = -EIO;
			return 0;
		}
		/* no data to send to network stack, break */
		if (!bytes_avail) {
			spin_lock_irqsave(&mhi_handle->rd_lock, flags);
			kfree_skb(skb);
			list_add_tail(&req->list, &mhi_handle->rx_buffers);
			spin_unlock_irqrestore(&mhi_handle->rd_lock, flags);
			return 0;
		}
	}
	/* coming out while only in case of no data or error */
	return ret_val;

}

static void mhi_dev_net_write_completion_cb(void *req)
{
	struct mhi_dev_net_client *client_handle = mhi_net_ctxt.client_handle;
	struct mhi_req *wreq = (struct mhi_req *)req;
	struct sk_buff *skb = wreq->context;
	unsigned long   flags;

	kfree_skb(skb);
	spin_lock_irqsave(&client_handle->wrt_lock, flags);
	list_add_tail(&wreq->list, &client_handle->wr_req_buffers);
	spin_unlock_irqrestore(&client_handle->wrt_lock, flags);
}

static int mhi_dev_net_alloc_write_reqs(struct mhi_dev_net_client *client)
{
	int nreq = 0, rc = 0;
	struct mhi_req *wreq;

	while (nreq < client->tx_reqs) {
		wreq = kzalloc(sizeof(struct mhi_req), GFP_ATOMIC);
		if (!wreq)
			return -ENOMEM;
		wreq->client_cb =  mhi_dev_net_write_completion_cb;
		list_add_tail(&wreq->list, &client->wr_req_buffers);
		nreq++;
	}
	mhi_dev_net_log(MHI_INFO,
			"mhi write reqs allocation success\n");
	return rc;

}

static int mhi_dev_net_alloc_read_reqs(struct mhi_dev_net_client *client)
{
	int nreq = 0, rc = 0;
	struct mhi_req *mreq;

	while (nreq < client->rx_reqs) {
		mreq = kzalloc(sizeof(struct mhi_req), GFP_ATOMIC);
		if (!mreq)
			return -ENOMEM;
		mreq->len =  TRB_MAX_DATA_SIZE;
		mreq->client_cb =  mhi_dev_net_read_completion_cb;
		list_add_tail(&mreq->list, &client->rx_buffers);
		nreq++;
	}
	mhi_dev_net_log(MHI_INFO,
			"mhi read reqs allocation success\n");
	return rc;

}

static int mhi_dev_net_open(struct net_device *dev)
{
	struct mhi_dev_net_client *mhi_dev_net_ptr =
		*(struct mhi_dev_net_client **)netdev_priv(dev);
	mhi_dev_net_log(MHI_INFO,
			"mhi_net_dev interface is up for IN %d OUT %d\n",
			mhi_dev_net_ptr->out_chan,
			mhi_dev_net_ptr->in_chan);
	netif_start_queue(dev);
	return 0;
}

static netdev_tx_t mhi_dev_net_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct mhi_dev_net_client *mhi_dev_net_ptr =
			*(struct mhi_dev_net_client **)netdev_priv(dev);
	unsigned long flags;

	if (skb->len <= 0) {
		mhi_dev_net_log(MHI_ERROR,
				"Invalid skb received freeing skb\n");
		kfree_skb(skb);
		return NETDEV_TX_OK;
	}
	spin_lock_irqsave(&mhi_dev_net_ptr->wrt_lock, flags);
	skb_queue_tail(&(mhi_dev_net_ptr->tx_buffers), skb);
	spin_unlock_irqrestore(&mhi_dev_net_ptr->wrt_lock, flags);

	queue_work(mhi_dev_net_ptr->pending_pckt_wq,
			&mhi_dev_net_ptr->xmit_work);

	return NETDEV_TX_OK;
}

static int mhi_dev_net_stop(struct net_device *dev)
{
	netif_stop_queue(dev);
	mhi_dev_net_log(MHI_VERBOSE, "mhi_dev_net interface is down\n");
	return 0;
}

static int mhi_dev_net_change_mtu(struct net_device *dev, int new_mtu)
{
	if (0 > new_mtu || MHI_NET_DEFAULT_MTU < new_mtu)
		return -EINVAL;
	dev->mtu = new_mtu;
	return 0;
}

static const struct net_device_ops mhi_dev_net_ops_ip = {
	.ndo_open = mhi_dev_net_open,
	.ndo_stop = mhi_dev_net_stop,
	.ndo_start_xmit = mhi_dev_net_xmit,
	.ndo_change_mtu = mhi_dev_net_change_mtu,
};

static void mhi_dev_net_rawip_setup(struct net_device *dev)
{
	dev->netdev_ops = &mhi_dev_net_ops_ip;
	ether_setup(dev);
	mhi_dev_net_log(MHI_INFO,
			"mhi_dev_net Raw IP setup\n");

	/* set this after calling ether_setup */
	dev->header_ops = NULL;
	dev->type = ARPHRD_RAWIP;
	dev->hard_header_len = 0;
	dev->mtu = MHI_NET_DEFAULT_MTU;
	dev->addr_len = 0;
	dev->flags &= ~(IFF_BROADCAST | IFF_MULTICAST);
}

static void mhi_dev_net_ether_setup(struct net_device *dev)
{
	dev->netdev_ops = &mhi_dev_net_ops_ip;
	ether_setup(dev);
	dev->min_mtu = ETH_MIN_MTU;
	dev->max_mtu = ETH_MAX_MTU;
	mhi_dev_net_log(MHI_INFO,
			"mhi_dev_net Ethernet setup\n");
}

static int mhi_dev_net_enable_iface(struct mhi_dev_net_client *mhi_dev_net_ptr)
{
	int ret = 0;
	struct mhi_dev_net_client **mhi_dev_net_ctxt = NULL;
	struct net_device *netdev;

	if (!mhi_dev_net_ptr)
		return -EINVAL;

	/* Initialize skb list head to queue the packets for mhi dev client */
	skb_queue_head_init(&(mhi_dev_net_ptr->tx_buffers));

	mhi_dev_net_log(MHI_INFO,
			"mhi_dev_net interface registration\n");
	netdev = alloc_netdev(sizeof(struct mhi_dev_net_client),
			MHI_NET_DEV_NAME, NET_NAME_PREDICTABLE,
			mhi_net_ctxt.client_handle->eth_iface ?
			mhi_dev_net_ether_setup :
			mhi_dev_net_rawip_setup);
	if (!netdev) {
		pr_err("Failed to allocate netdev for mhi_dev_net\n");
		goto net_dev_alloc_fail;
	}

	if (mhi_net_ctxt.client_handle->eth_iface) {
		eth_random_addr(netdev->dev_addr);
		if (!is_valid_ether_addr(netdev->dev_addr))
			return -EADDRNOTAVAIL;
	}

	mhi_dev_net_ctxt = netdev_priv(netdev);
	mhi_dev_net_ptr->dev = netdev;
	*mhi_dev_net_ctxt = mhi_dev_net_ptr;
	ret = register_netdev(mhi_dev_net_ptr->dev);
	if (ret) {
		pr_err("Failed to register mhi_dev_net device\n");
		goto net_dev_reg_fail;
	}
	mhi_dev_net_log(MHI_INFO, "Successfully registred mhi_dev_net\n");
	return 0;

net_dev_reg_fail:
	free_netdev(mhi_dev_net_ptr->dev);
net_dev_alloc_fail:
	mhi_net_ctxt.dev_ops->close_channel(mhi_dev_net_ptr->in_handle);
	mhi_net_ctxt.dev_ops->close_channel(mhi_dev_net_ptr->out_handle);
	mhi_dev_net_ptr->dev = NULL;
	return -ENOMEM;
}

static int mhi_dev_net_open_chan_create_netif(struct mhi_dev_net_client *client)
{
	int rc = 0;
	int ret = 0;
	struct list_head *cp, *q;
	struct mhi_req *mreq;

	mhi_dev_net_log(MHI_DBG, "opening OUT %d IN %d channels\n",
			client->out_chan,
			client->in_chan);
	mhi_dev_net_log(MHI_DBG,
			"Initializing inbound chan %d.\n",
			client->in_chan);

	rc = mhi_net_ctxt.dev_ops->open_channel(client->out_chan,
			&client->out_handle, mhi_net_ctxt.net_event_notifier);
	if (rc < 0) {
		mhi_dev_net_log(MHI_ERROR,
				"Failed to open chan %d, ret 0x%x\n",
				client->out_chan, rc);
		goto handle_not_rdy_err;
	} else
		atomic_set(&client->rx_enabled, 1);

	rc = mhi_net_ctxt.dev_ops->open_channel(client->in_chan,
			&client->in_handle, mhi_net_ctxt.net_event_notifier);
	if (rc < 0) {
		mhi_dev_net_log(MHI_ERROR,
				"Failed to open chan %d, ret 0x%x\n",
				client->in_chan, rc);
		goto handle_in_err;
	} else
		atomic_set(&client->tx_enabled, 1);

	mhi_dev_net_log(MHI_INFO, "IN %d, OUT %d channels are opened",
			client->in_chan, client->out_chan);

	INIT_LIST_HEAD(&client->rx_buffers);
	INIT_LIST_HEAD(&client->wr_req_buffers);
	/* pre allocate read request buffer */

	ret = mhi_dev_net_alloc_read_reqs(client);
	if (ret) {
		pr_err("failed to allocate rx req buffers\n");
		goto rx_req_failed;
	}
	ret = mhi_dev_net_alloc_write_reqs(client);
	if (ret) {
		pr_err("failed to allocate write req buffers\n");
		goto tx_req_failed;
	}
	if (atomic_read(&client->tx_enabled)) {
		ret = mhi_dev_net_enable_iface(client);
		if (ret < 0)
			mhi_dev_net_log(MHI_ERROR,
					"failed to enable mhi_dev_net iface\n");
	}
	return ret;
tx_req_failed:
	list_for_each_safe(cp, q, &client->rx_buffers);
	mreq = list_entry(cp, struct mhi_req, list);
	list_del(cp);
	kfree(mreq);
rx_req_failed:
	mhi_net_ctxt.dev_ops->close_channel(client->in_handle);
handle_in_err:
	mhi_net_ctxt.dev_ops->close_channel(client->out_handle);
handle_not_rdy_err:
	mutex_unlock(&client->in_chan_lock);
	mutex_unlock(&client->out_chan_lock);
	return rc;
}

static int mhi_dev_net_close(void)
{
	struct mhi_dev_net_client *client;

	mhi_dev_net_log(MHI_INFO,
			"mhi_dev_net module is removed\n");
	client = mhi_net_ctxt.client_handle;
	mhi_net_ctxt.dev_ops->close_channel(client->out_handle);
	atomic_set(&client->tx_enabled, 0);
	mhi_net_ctxt.dev_ops->close_channel(client->in_handle);
	atomic_set(&client->rx_enabled, 0);
	if (client->dev != NULL) {
		netif_stop_queue(client->dev);
		unregister_netdev(client->dev);
		free_netdev(client->dev);
		client->dev = NULL;
	}
	/* freeing mhi client and IPC context */
	kfree(client);
	kfree(mhi_net_ipc_log);
	return 0;
}

static int mhi_dev_net_rgstr_client(struct mhi_dev_net_client *client, int idx)
{
	client->out_chan = idx;
	client->in_chan = idx + 1;
	mutex_init(&client->in_chan_lock);
	mutex_init(&client->out_chan_lock);
	spin_lock_init(&client->wrt_lock);
	spin_lock_init(&client->rd_lock);
	mhi_dev_net_log(MHI_INFO, "Registering out %d, In %d channels\n",
			client->out_chan, client->in_chan);
	return 0;
}

static int mhi_dev_net_dergstr_client
				(struct mhi_dev_net_client *client)
{
	mutex_destroy(&client->in_chan_lock);
	mutex_destroy(&client->out_chan_lock);

	return 0;
}

static void mhi_dev_net_free_reqs(struct list_head *buff)
{
	struct list_head *node, *next;
	struct mhi_req *mreq;

	list_for_each_safe(node, next, buff) {
		mreq = list_entry(node, struct mhi_req, list);
		list_del(&mreq->list);
		kfree(mreq);
	}
}

static void mhi_dev_net_state_cb(struct mhi_dev_client_cb_data *cb_data)
{
	struct mhi_dev_net_client *mhi_client;
	uint32_t info_in_ch = 0, info_out_ch = 0;
	int ret;

	if (!cb_data || !cb_data->user_data) {
		mhi_dev_net_log(MHI_ERROR, "invalid input received\n");
		return;
	}
	mhi_client = cb_data->user_data;

	ret = mhi_net_ctxt.dev_ops->ctrl_state_info(mhi_client->in_chan,
								&info_in_ch);
	if (ret) {
		mhi_dev_net_log(MHI_ERROR,
			"Failed to obtain in_channel %d state\n",
			mhi_client->in_chan);
		return;
	}
	ret = mhi_net_ctxt.dev_ops->ctrl_state_info(mhi_client->out_chan,
								&info_out_ch);
	if (ret) {
		mhi_dev_net_log(MHI_ERROR,
			"Failed to obtain out_channel %d state\n",
			mhi_client->out_chan);
		return;
	}
	mhi_dev_net_log(MHI_MSG_VERBOSE, "in_channel :%d, state :%d\n",
			mhi_client->in_chan, info_in_ch);
	mhi_dev_net_log(MHI_MSG_VERBOSE, "out_channel :%d, state :%d\n",
			mhi_client->out_chan, info_out_ch);
	if (info_in_ch == MHI_STATE_CONNECTED &&
		info_out_ch == MHI_STATE_CONNECTED) {
		/**
		 * Open IN and OUT channels for Network client
		 * and create Network Interface.
		 */
		ret = mhi_dev_net_open_chan_create_netif(mhi_client);
		if (ret) {
			mhi_dev_net_log(MHI_ERROR,
				"Failed to open channels\n");
			return;
		}
	} else if (info_in_ch == MHI_STATE_DISCONNECTED ||
				info_out_ch == MHI_STATE_DISCONNECTED) {
		if (mhi_client->dev != NULL) {
			netif_stop_queue(mhi_client->dev);
			unregister_netdev(mhi_client->dev);
			mhi_net_ctxt.dev_ops->close_channel(mhi_client->out_handle);
			atomic_set(&mhi_client->tx_enabled, 0);
			mhi_net_ctxt.dev_ops->close_channel(mhi_client->in_handle);
			atomic_set(&mhi_client->rx_enabled, 0);
			mhi_dev_net_free_reqs(&mhi_client->rx_buffers);
			mhi_dev_net_free_reqs(&mhi_client->wr_req_buffers);
			free_netdev(mhi_client->dev);
			mhi_client->dev = NULL;
		}
	}
}

int mhi_dev_net_interface_init(struct mhi_dev_ops *dev_ops)
{
	int ret_val = 0, index = 0;
	uint32_t info_out_ch = 0;
	uint32_t reqs = 0;
	struct mhi_dev_net_client *mhi_net_client = NULL;

	if (mhi_net_ctxt.client_handle) {
		mhi_dev_net_log(MHI_INFO,
			"MHI Netdev interface already initialized\n");
		return ret_val;
	}

	mhi_net_client = kzalloc(sizeof(struct mhi_dev_net_client), GFP_KERNEL);
	if (!mhi_net_client)
		return -ENOMEM;

	mhi_net_ipc_log = ipc_log_context_create(MHI_NET_IPC_PAGES,
						"mhi-net", 0);
	if (!mhi_net_ipc_log) {
		mhi_dev_net_log(MHI_DBG,
				"Failed to create IPC logging for mhi_dev_net\n");
		kfree(mhi_net_client);
		return -ENOMEM;
	}
	mhi_net_ctxt.client_handle = mhi_net_client;

	if (mhi_net_ctxt.pdev) {
		mhi_net_ctxt.client_handle->eth_iface =
			of_property_read_bool
			((&mhi_net_ctxt.pdev->dev)->of_node,
				"qcom,mhi-ethernet-interface");
		ret_val = of_property_read_u32
				((&mhi_net_ctxt.pdev->dev)->of_node,
				 "qcom,tx_rx_reqs", &reqs);
		if (ret_val < 0) {
			mhi_net_client->tx_reqs = MHI_MAX_TX_REQ;
			mhi_net_client->rx_reqs = MHI_MAX_RX_REQ;
		} else {
			mhi_net_client->tx_reqs = reqs;
			mhi_net_client->rx_reqs = reqs;
		}
	}

	/*Process pending packet work queue*/
	mhi_net_client->pending_pckt_wq =
		create_singlethread_workqueue("pending_xmit_pckt_wq");
	INIT_WORK(&mhi_net_client->xmit_work,
			mhi_dev_net_process_queue_packets);

	mhi_dev_net_log(MHI_INFO,
			"Registering for MHI transfer events from host\n");
	mhi_net_ctxt.net_event_notifier = mhi_dev_net_event_notifier;

	ret_val = mhi_dev_net_init_ch_attributes(&mhi_net_ctxt);
	if (ret_val < 0) {
		mhi_dev_net_log(MHI_ERROR,
				"Failed to init client attributes\n");
		goto channel_init_fail;
	}
	mhi_dev_net_log(MHI_DBG, "Initializing client\n");
	index = MHI_CLIENT_IP_SW_4_OUT;
	ret_val = mhi_dev_net_rgstr_client(mhi_net_client, index);
	if (ret_val) {
		mhi_dev_net_log(MHI_CRITICAL,
				"Failed to reg client %d ret 0\n", ret_val);
		goto client_register_fail;
	}
	ret_val = dev_ops->register_state_cb(mhi_dev_net_state_cb,
				mhi_net_client, MHI_CLIENT_IP_SW_4_OUT);
	if (ret_val < 0 && ret_val != -EEXIST)
		goto register_state_cb_fail;

	ret_val = dev_ops->register_state_cb(mhi_dev_net_state_cb,
				mhi_net_client, MHI_CLIENT_IP_SW_4_IN);
	/* -EEXIST indicates success and channel is already open */
	if (ret_val == -EEXIST) {
		/**
		 * If both in and out channels were opened by host at the
		 * time of registration proceed with opening channels and
		 * create network interface from device side.
		 * if the channels are not opened at the time of registration
		 * we will get a call back notification mhi_dev_net_state_cb()
		 * and proceed to open channels and create network interface
		 * with mhi_dev_net_open_chan_create_netif().
		 */
		ret_val = 0;
		if (!mhi_net_ctxt.dev_ops->ctrl_state_info(mhi_net_client->out_chan,
					&info_out_ch)) {
			if (info_out_ch == MHI_STATE_CONNECTED) {
				ret_val = mhi_dev_net_open_chan_create_netif
					(mhi_net_client);
				if (ret_val < 0) {
					mhi_dev_net_log(MHI_ERROR,
							"Failed to open channels\n");
					goto channel_open_fail;
				}
			}
		}
	} else if (ret_val < 0) {
		goto register_state_cb_fail;
	}

	mhi_net_ctxt.dev_ops = dev_ops;
	return ret_val;

channel_open_fail:
register_state_cb_fail:
	mhi_dev_net_dergstr_client(mhi_net_client);
client_register_fail:
channel_init_fail:
	destroy_workqueue(mhi_net_client->pending_pckt_wq);
	kfree(mhi_net_client);
	kfree(mhi_net_ipc_log);
	return ret_val;
}
EXPORT_SYMBOL(mhi_dev_net_interface_init);

void mhi_dev_net_exit(void)
{
	mhi_dev_net_log(MHI_INFO,
			"MHI Network Interface Module exited\n");
	mhi_dev_net_close();
}
EXPORT_SYMBOL(mhi_dev_net_exit);

static int mhi_dev_net_probe(struct platform_device *pdev)
{
	if (pdev->dev.of_node) {
		mhi_net_ctxt.pdev = pdev;
		mhi_dev_net_log(MHI_INFO,
				"MHI Network probe success");
	}

	return 0;
}

static int mhi_dev_net_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id mhi_dev_net_match_table[] = {
	{	.compatible = "qcom,msm-mhi-dev-net" },
	{}
};

static struct platform_driver mhi_dev_net_driver = {
	.driver		= {
		.name	= "qcom,msm-mhi-dev-net",
		.of_match_table = mhi_dev_net_match_table,
	},
	.probe		= mhi_dev_net_probe,
	.remove		= mhi_dev_net_remove,
};

static int __init mhi_dev_net_init(void)
{
	return platform_driver_register(&mhi_dev_net_driver);
}
subsys_initcall(mhi_dev_net_init);

static void __exit mhi_dev_exit(void)
{
	platform_driver_unregister(&mhi_dev_net_driver);
}
module_exit(mhi_dev_exit);

MODULE_DESCRIPTION("MHI net device driver");
MODULE_LICENSE("GPL v2");
