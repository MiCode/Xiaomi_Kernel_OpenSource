/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
/*
 * MHI Device Network interface
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/dma-mapping.h>
#include <linux/ipc_logging.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/ktime.h>

#include "mhi.h"

#define MHI_NET_DRIVER_NAME  "mhi_dev_net_drv"
#define MHI_NET_DEV_NAME     "mhi_dev_net%d"
#define MHI_NET_DEFAULT_MTU   4000
#define MHI_NET_IPC_PAGES     (100)

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
static enum mhi_dev_net_dbg_lvl mhi_net_ipc_log_lvl = MHI_INFO;
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

module_param(mhi_net_msg_lvl , uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mhi_net_msg_lvl, "mhi dev net dbg lvl");

module_param(mhi_net_ipc_log_lvl, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(mhi_net_ipc_log_lvl, "mhi dev net dbg lvl");

struct mhi_dev_net_client {
	/* write channel - always even*/
	u32 out_chan;
	/* read channel - always odd */
	u32 in_chan;
	struct mhi_dev_client *out_handle;
	struct mhi_dev_client *in_handle;
	/*process pendig packets */
	struct workqueue_struct *pending_pckt_wq;
	struct work_struct       xmit_work;
	/*Read data from host work queue*/
	struct workqueue_struct *read_data_wq;
	struct work_struct       dev_read_wrk;
	atomic_t pckt_queue_count;
	atomic_t  rx_enabled;
	atomic_t  tx_enabled;
	struct net_device *dev;
	struct sk_buff_head tx_buffers;
	struct mhi_dev_net_ctxt *net_ctxt;
	/*To check write channel is empty or not*/
	spinlock_t write_chan_lock;
	struct mutex in_chan_lock;
	struct mutex out_chan_lock;
};

struct mhi_dev_net_ctxt {
	struct mhi_dev_net_chan_attr chan_attr[MHI_MAX_SOFTWARE_CHANNELS];
	struct mhi_dev_net_client *client_handle;
	void (*net_event_notifier)(struct mhi_dev_client_cb_reason *cb);
};

static struct mhi_dev_net_ctxt mhi_net_ctxt;
static ssize_t mhi_dev_net_client_read(struct mhi_dev_net_client *);

static void mhi_dev_net_rx_scheduler(struct work_struct *work)
{
	struct mhi_dev_net_client *mhi_dev_net_client = container_of(work,
			struct mhi_dev_net_client, dev_read_wrk);

	if (mhi_dev_net_client)
		mhi_dev_net_client_read(mhi_dev_net_client);
	else
		mhi_dev_net_log(MHI_CRITICAL, "mhi_dev_net client is NULL\n");
}

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

static void process_queue_packets(struct work_struct *work)
{
	u32 xfer_data = 0;
	ktime_t start_time;

	struct mhi_dev_net_client *mhi_net_client = container_of(work,
			struct mhi_dev_net_client, xmit_work);
	if (mhi_dev_channel_isempty(mhi_net_client->in_handle)) {
		netif_stop_queue(mhi_net_client->dev);
		return;
	}
	while (!skb_queue_empty(&(mhi_net_client->tx_buffers))) {
		struct sk_buff *skb =
			skb_dequeue(&(mhi_net_client->tx_buffers));
		atomic_dec(&mhi_net_client->pckt_queue_count);
		if (!skb) {
			mhi_dev_net_log(MHI_CRITICAL,
					"skb dequeue returned NULL\n");
			return;
		}
		start_time = ktime_get();
		xfer_data =
			mhi_dev_write_channel(mhi_net_client->in_handle,
					skb->data, skb->len);
		if (xfer_data != skb->len) {
			pr_err("Failed to write skb len %d xfered data %d\n",
					skb->len, xfer_data);
			kfree_skb(skb);
			return;
		}
		mhi_net_client->dev->stats.tx_packets++;
		mhi_dev_net_log(MHI_VERBOSE, "write_chan time = %lld\n",
			ktime_to_us(ktime_sub(ktime_get(), start_time)));
		kfree_skb(skb);
		/* Check if free buffers availability */
		if (mhi_dev_channel_isempty(mhi_net_client->in_handle)) {
			netif_stop_queue(mhi_net_client->dev);
			break;
		}
	} /* While TX queue is not empty */
}

static void mhi_dev_net_event_notifier(struct mhi_dev_client_cb_reason *reason)
{
	struct mhi_dev_net_client *mhi_handle = NULL;

	if (reason->reason == MHI_DEV_TRE_AVAILABLE) {
		mhi_handle = mhi_net_ctxt.client_handle;
		mhi_dev_net_log(MHI_VERBOSE,
				"recived TRE available event for chan %d\n",
				mhi_handle->in_chan);
		if (reason->ch_id % 2) {
			spin_lock(&mhi_handle->write_chan_lock);
			if (netif_queue_stopped(mhi_handle->dev)) {
				if (atomic_read(&mhi_handle->pckt_queue_count))
					queue_work(mhi_handle->pending_pckt_wq,
							&mhi_handle->xmit_work);
				else
					netif_wake_queue(mhi_handle->dev);
			}
			spin_unlock(&mhi_handle->write_chan_lock);
		} else
			queue_work(mhi_handle->read_data_wq,
					&mhi_handle->dev_read_wrk);
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

static ssize_t mhi_dev_net_client_read(struct mhi_dev_net_client *mhi_handle)
{
	int bytes_avail = 0;
	int ret_val = 0;
	u32 chan = 0;
	uint32_t buf_size = TRB_MAX_DATA_SIZE;
	uint32_t chained = 0;
	struct mhi_dev_client *client_handle = NULL;
	struct sk_buff *skb_buff;
	ktime_t start_time;

	client_handle = mhi_handle->out_handle;
	chan = mhi_handle->out_chan;
	if (!atomic_read(&mhi_handle->rx_enabled))
		return -EPERM;
	do {
		start_time = ktime_get();
		skb_buff = alloc_skb(MHI_NET_DEFAULT_MTU, GFP_ATOMIC);
		if (!skb_buff) {
			mhi_dev_net_log(MHI_ERROR,
				"Error while allocating skb\n");
			return -ENOMEM;
		}
		bytes_avail = mhi_dev_read_channel(client_handle,
				skb_buff->data,
				buf_size, &chained);
		mhi_dev_net_log(MHI_VERBOSE,
				"dev_read_channel time = %lld\n",
			ktime_to_us(ktime_sub(ktime_get(), start_time)));
		if (bytes_avail < 0) {
			pr_err("Failed to read chan %d bytes_avail = %d\n",
					chan, bytes_avail);
			ret_val = -EIO;
			break;
		}
		/* no data to send to network stack, break */
		if (!bytes_avail)
			break;

		skb_buff->len = bytes_avail;
		mhi_dev_net_log(MHI_VERBOSE, "reading frm chan %d buff size %d",
				chan, buf_size);
		mhi_dev_net_log(MHI_VERBOSE, "bytes_read %d chained %d",
				bytes_avail, chained);
		skb_buff->protocol =
			mhi_dev_net_eth_type_trans(skb_buff);
		skb_put(skb_buff, bytes_avail);
		mhi_handle->dev->stats.rx_packets++;
		skb_buff->dev = mhi_handle->dev;
		start_time = ktime_get();
		netif_rx(skb_buff);
	} while (1);
	/* coming out while only in case of no data or error */
	kfree_skb(skb_buff);
	return ret_val;
}

static int mhi_dev_net_open(struct net_device *dev)
{
	struct mhi_dev_net_client *mhi_dev_net_ptr =
		*(struct mhi_dev_net_client **)netdev_priv(dev);
	mhi_dev_net_log(MHI_INFO,
			"%s mhi_net_dev interface is up for IN %d OUT %d\n",
			__func__, mhi_dev_net_ptr->out_chan,
			mhi_dev_net_ptr->in_chan);
	netif_start_queue(dev);
	return 0;
}

static int mhi_dev_net_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct mhi_dev_net_client *mhi_dev_net_ptr =
			*(struct mhi_dev_net_client **)netdev_priv(dev);

	mhi_dev_net_log(MHI_VERBOSE, "SKB received\n");
	if (skb->len <= 0) {
		mhi_dev_net_log(MHI_ERROR,
				"Invalid skb received freeing skb\n");
		kfree_skb(skb);
		return NETDEV_TX_OK;
	}
	skb_queue_tail(&(mhi_dev_net_ptr->tx_buffers), skb);
	atomic_inc(&mhi_dev_net_ptr->pckt_queue_count);
	queue_work(mhi_dev_net_ptr->pending_pckt_wq,
			&mhi_dev_net_ptr->xmit_work);
	mhi_dev_net_log(MHI_VERBOSE, "Exiting from transmit function\n");
	return 0;
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
	.ndo_set_mac_address = 0,
	.ndo_validate_addr = 0,
};

static void mhi_dev_net_setup(struct net_device *dev)
{
	dev->netdev_ops = &mhi_dev_net_ops_ip;
	ether_setup(dev);

	/* set this after calling ether_setup */
	dev->header_ops = 0;  /* No header */
	dev->type = ARPHRD_RAWIP;
	dev->hard_header_len = 0;
	dev->mtu = MHI_NET_DEFAULT_MTU;
	dev->addr_len = 0;
	dev->flags &= ~(IFF_BROADCAST | IFF_MULTICAST);
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
			mhi_dev_net_setup);
	if (!netdev) {
		pr_err("Failed to allocate netdev for mhi_dev_net\n");
		goto net_dev_alloc_fail;
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
	mhi_dev_close_channel(mhi_dev_net_ptr->in_handle);
	mhi_dev_close_channel(mhi_dev_net_ptr->out_handle);
	mhi_dev_net_ptr->dev = NULL;
	return -ENOMEM;
}

static int mhi_dev_net_open_channels(struct mhi_dev_net_client *client)
{
	int rc = 0;
	int ret = 0;

	mhi_dev_net_log(MHI_DBG, "opening OUT %d IN %d channels\n",
			client->out_chan,
			client->in_chan);
	mutex_lock(&client->out_chan_lock);
	mutex_lock(&client->in_chan_lock);
	mhi_dev_net_log(MHI_DBG,
			"Initializing inbound chan %d.\n",
			client->in_chan);

	rc = mhi_dev_open_channel(client->out_chan, &client->out_handle,
			mhi_net_ctxt.net_event_notifier);
	if (rc < 0) {
		mhi_dev_net_log(MHI_ERROR,
				"Failed to open chan %d, ret 0x%x\n",
				client->out_chan, rc);
		goto handle_not_rdy_err;
	} else
		atomic_set(&client->rx_enabled, 1);

	rc = mhi_dev_open_channel(client->in_chan, &client->in_handle,
			mhi_net_ctxt.net_event_notifier);
	if (rc < 0) {
		mhi_dev_net_log(MHI_ERROR,
				"Failed to open chan %d, ret 0x%x\n",
				client->in_chan, rc);
		goto handle_in_err;
	} else
		atomic_set(&client->tx_enabled, 1);

	mutex_unlock(&client->in_chan_lock);
	mutex_unlock(&client->out_chan_lock);
	mhi_dev_net_log(MHI_INFO, "IN %d, OUT %d channels are opened",
			client->in_chan, client->out_chan);
	if (atomic_read(&client->tx_enabled)) {
		ret = mhi_dev_net_enable_iface(client);
		if (ret < 0)
			mhi_dev_net_log(MHI_ERROR,
					"failed to enable mhi_dev_net iface\n");
	}
	return ret;
handle_in_err:
	mhi_dev_close_channel(client->out_handle);
	mutex_unlock(&client->in_chan_lock);
	mutex_unlock(&client->out_chan_lock);
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
	mhi_dev_close_channel(client->out_handle);
	mhi_dev_close_channel(client->in_handle);
	atomic_set(&client->tx_enabled, 0);
	atomic_set(&client->rx_enabled, 0);
	if (client->dev != 0) {
		netif_stop_queue(client->dev);
		unregister_netdev(client->dev);
		free_netdev(client->dev);
		client->dev = 0;
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
	spin_lock_init(&client->write_chan_lock);
	mhi_dev_net_log(MHI_INFO, "Registering out %d, In %d channels\n",
			client->out_chan, client->in_chan);

	/* Open IN and OUT channels for Network client*/
	mhi_dev_net_open_channels(client);
	return 0;
}

int mhi_dev_net_interface_init(void)
{
	int ret_val = 0;
	int index = 0;
	struct mhi_dev_net_client *mhi_net_client = NULL;

	mhi_net_client = kzalloc(sizeof(struct mhi_dev_net_client), GFP_KERNEL);
	if (!mhi_net_client)
		return -ENOMEM;

	mhi_net_ipc_log = ipc_log_context_create(MHI_NET_IPC_PAGES,
						"mhi-net", 0);
	if (mhi_net_ipc_log == NULL)
		mhi_dev_net_log(MHI_DBG,
				"Failed to create IPC logging for mhi_dev_net\n");
	mhi_net_ctxt.client_handle = mhi_net_client;

	/*Process pending packet work queue*/
	mhi_net_client->pending_pckt_wq =
		create_singlethread_workqueue("pending_xmit_pckt_wq");
	INIT_WORK(&mhi_net_client->xmit_work, process_queue_packets);

	/* read data from host when event trigger */
	mhi_net_client->read_data_wq =
		create_singlethread_workqueue("dev_read_from_host_wq");
	INIT_WORK(&mhi_net_client->dev_read_wrk, mhi_dev_net_rx_scheduler);

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
	return ret_val;

channel_init_fail:
	kfree(mhi_net_client);
	kfree(mhi_net_ipc_log);
	return ret_val;
client_register_fail:
	kfree(mhi_net_client);
	kfree(mhi_net_ipc_log);
	return ret_val;
}
EXPORT_SYMBOL(mhi_dev_net_interface_init);

void __exit mhi_dev_net_exit(void)
{
	mhi_dev_net_log(MHI_INFO,
			"MHI Network Interface Module exited ");
	mhi_dev_net_close();
}
EXPORT_SYMBOL(mhi_dev_net_exit);
