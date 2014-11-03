/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
 * MHI RMNET Network interface
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/msm_rmnet.h>
#include <linux/if_arp.h>
#include <linux/dma-mapping.h>
#include <linux/msm_mhi.h>
#include <linux/debugfs.h>
#include <linux/ipc_logging.h>

#define RMNET_MHI_DRIVER_NAME "rmnet_mhi"
#define RMNET_MHI_DEV_NAME    "rmnet_mhi%d"
#define MHI_DEFAULT_MTU        8000
#define MHI_DEFAULT_MRU        8000
#define MHI_MAX_MRU            0xFFFF
#define MHI_NAPI_WEIGHT_VALUE  12
#define MHI_RX_HEADROOM        64
#define WATCHDOG_TIMEOUT       (30 * HZ)
#define MHI_RMNET_DEVICE_COUNT 1
#define RMNET_IPC_LOG_PAGES (10)
#define IS_INBOUND(_chan) (((u32)(_chan)) % 2)

enum DBG_LVL {
	MSG_VERBOSE = 0x1,
	MSG_INFO = 0x2,
	MSG_DBG = 0x4,
	MSG_WARNING = 0x8,
	MSG_ERROR = 0x10,
	MSG_CRITICAL = 0x20,
	MSG_reserved = 0x80000000
};

enum DBG_LVL rmnet_ipc_log_lvl = MSG_INFO;
enum DBG_LVL rmnet_msg_lvl = MSG_CRITICAL;

module_param(rmnet_msg_lvl , uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(rmnet_msg_lvl, "dbg lvl");
module_param(rmnet_ipc_log_lvl, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(rmnet_ipc_log_lvl, "dbg lvl");

void *rmnet_ipc_log;

#define rmnet_log(_msg_lvl, _msg, ...) do { \
		if ((_msg_lvl) >= rmnet_msg_lvl) \
			pr_alert("[%s] " _msg, __func__, ##__VA_ARGS__);\
		if (rmnet_ipc_log && ((_msg_lvl) >= rmnet_ipc_log_lvl))	\
			ipc_log_string(rmnet_ipc_log,			\
			       "[%s] " _msg, __func__, ##__VA_ARGS__);	\
} while (0)

unsigned long tx_interrupts_count[MHI_RMNET_DEVICE_COUNT];
module_param_array(tx_interrupts_count, ulong, 0, S_IRUGO);
MODULE_PARM_DESC(tx_interrupts_count, "Tx interrupts");

unsigned long rx_interrupts_count[MHI_RMNET_DEVICE_COUNT];
module_param_array(rx_interrupts_count, ulong, 0, S_IRUGO);
MODULE_PARM_DESC(rx_interrupts_count, "RX interrupts");

unsigned long tx_ring_full_count[MHI_RMNET_DEVICE_COUNT];
module_param_array(tx_ring_full_count, ulong, 0, S_IRUGO);
MODULE_PARM_DESC(tx_ring_full_count, "RING FULL errors from MHI Core");


unsigned long tx_queued_packets_count[MHI_RMNET_DEVICE_COUNT];
module_param_array(tx_queued_packets_count, ulong, 0, S_IRUGO);
MODULE_PARM_DESC(tx_queued_packets_count, "TX packets queued in MHI core");

unsigned long rx_interrupts_in_masked_irq[MHI_RMNET_DEVICE_COUNT];
module_param_array(rx_interrupts_in_masked_irq, ulong, 0, S_IRUGO);
MODULE_PARM_DESC(rx_interrupts_in_masked_irq,
		 "RX interrupts while IRQs are masked");

unsigned long rx_napi_skb_burst_min[MHI_RMNET_DEVICE_COUNT];
module_param_array(rx_napi_skb_burst_min, ulong, 0, S_IRUGO);
MODULE_PARM_DESC(rx_napi_skb_burst_min, "MIN SKBs sent to NS during NAPI");

unsigned long rx_napi_skb_burst_max[MHI_RMNET_DEVICE_COUNT];
module_param_array(rx_napi_skb_burst_max, ulong, 0, S_IRUGO);
MODULE_PARM_DESC(rx_napi_skb_burst_max, "MAX SKBs sent to NS during NAPI");

unsigned long tx_cb_skb_free_burst_min[MHI_RMNET_DEVICE_COUNT];
module_param_array(tx_cb_skb_free_burst_min, ulong, 0, S_IRUGO);
MODULE_PARM_DESC(tx_cb_skb_free_burst_min, "MIN SKBs freed during TX CB");

unsigned long tx_cb_skb_free_burst_max[MHI_RMNET_DEVICE_COUNT];
module_param_array(tx_cb_skb_free_burst_max, ulong, 0, S_IRUGO);
MODULE_PARM_DESC(tx_cb_skb_free_burst_max, "MAX SKBs freed during TX CB");

unsigned long rx_napi_budget_overflow[MHI_RMNET_DEVICE_COUNT];
module_param_array(rx_napi_budget_overflow, ulong, 0, S_IRUGO);
MODULE_PARM_DESC(rx_napi_budget_overflow,
		 "Budget hit with more items to read counter");

struct rmnet_mhi_private {
	int                           dev_index;
	struct mhi_client_handle      *tx_client_handle;
	struct mhi_client_handle      *rx_client_handle;
	enum MHI_CLIENT_CHANNEL       tx_channel;
	enum MHI_CLIENT_CHANNEL       rx_channel;
	struct sk_buff_head           tx_buffers;
	struct sk_buff_head           rx_buffers;
	uint32_t                      mru;
	struct napi_struct            napi;
	gfp_t                         allocation_flags;
	uint32_t                      tx_buffers_max;
	uint32_t                      rx_buffers_max;
	u32			      tx_enabled;
	u32			      rx_enabled;
	u32			      mhi_enabled;
	struct net_device	      *dev;
	atomic_t		      irq_masked_cntr;
	rwlock_t		      out_chan_full_lock;
};

struct tx_buffer_priv {
	dma_addr_t dma_addr;
};

static struct rmnet_mhi_private rmnet_mhi_ctxt_list[MHI_RMNET_DEVICE_COUNT];

static dma_addr_t rmnet_mhi_internal_get_dma_addr(struct sk_buff *skb,
						  enum dma_data_direction dir)
{
	if (dir == DMA_TO_DEVICE) {
		struct tx_buffer_priv *tx_priv =
			(struct tx_buffer_priv *)(skb->cb);
		return tx_priv->dma_addr;
	} else /* DMA_FROM_DEVICE */{
		uintptr_t *cb_ptr = 0;
		cb_ptr = (uintptr_t *)skb->cb;
		return (dma_addr_t)(uintptr_t)(*cb_ptr);
	}
}

static void rmnet_mhi_internal_clean_unmap_buffers(struct net_device *dev,
						   struct sk_buff_head *queue,
						   enum dma_data_direction dir)
{
	struct rmnet_mhi_private *rmnet_mhi_ptr =
		*(struct rmnet_mhi_private **)netdev_priv(dev);
	rmnet_log(MSG_INFO, "Entered\n");
	while (!skb_queue_empty(queue)) {
		struct sk_buff *skb = skb_dequeue(queue);
		if (skb != 0) {
			dma_addr_t dma_addr =
				rmnet_mhi_internal_get_dma_addr(skb, dir);
			if (dir == DMA_FROM_DEVICE)
				dma_unmap_single(&(dev->dev),
					dma_addr,
					(rmnet_mhi_ptr->mru - MHI_RX_HEADROOM),
					dir);
			else
				dma_unmap_single(&(dev->dev),
						dma_addr,
						skb->len,
						dir);
			kfree_skb(skb);
		}
	}
	rmnet_log(MSG_INFO, "Exited\n");
}

static __be16 rmnet_mhi_ip_type_trans(struct sk_buff *skb)
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

static int rmnet_mhi_poll(struct napi_struct *napi, int budget)
{
	int received_packets = 0;
	struct net_device *dev = napi->dev;
	struct rmnet_mhi_private *rmnet_mhi_ptr =
			*(struct rmnet_mhi_private **)netdev_priv(dev);
	enum MHI_STATUS res = MHI_STATUS_reserved;
	bool should_reschedule = true;
	struct sk_buff *skb;
	dma_addr_t dma_addr;
	uintptr_t *cb_ptr;

	rmnet_log(MSG_VERBOSE, "Entered\n");
	while (received_packets < budget) {
		struct mhi_result *result =
		      mhi_poll(rmnet_mhi_ptr->rx_client_handle);
		if (result->transaction_status == MHI_STATUS_DEVICE_NOT_READY) {
			continue;
		} else if (result->transaction_status != MHI_STATUS_SUCCESS) {
			rmnet_log(MSG_CRITICAL,
				  "mhi_poll failed, error is %d\n",
				  result->transaction_status);
			break;
		}

		/* Nothing more to read, or out of buffers in MHI layer */
		if (unlikely(!result->payload_buf ||
						!result->bytes_xferd)) {
			should_reschedule = false;
			break;
		}

		skb = skb_dequeue(&(rmnet_mhi_ptr->rx_buffers));
		if (unlikely(!skb)) {
			rmnet_log(MSG_CRITICAL,
				  "No RX buffers to match");
			break;
		}

		cb_ptr = (uintptr_t *)skb->cb;
		dma_addr = (dma_addr_t)(uintptr_t)(*cb_ptr);

		/* Sanity check, ensuring that this is actually the buffer */
		if (unlikely(dma_addr != result->payload_buf)) {
			rmnet_log(MSG_CRITICAL,
				  "Buf mismatch, expected 0x%lx, got 0x%lx",
					(uintptr_t)dma_addr,
					(uintptr_t)result->payload_buf);
			break;
		}

		dma_unmap_single(&(dev->dev), dma_addr,
				(rmnet_mhi_ptr->mru - MHI_RX_HEADROOM),
				 DMA_FROM_DEVICE);
		skb_put(skb, result->bytes_xferd);

		skb->dev = dev;
		skb->protocol = rmnet_mhi_ip_type_trans(skb);

		netif_receive_skb(skb);

		/* Statistics */
		received_packets++;
		dev->stats.rx_packets++;
		dev->stats.rx_bytes += result->bytes_xferd;

		/* Need to allocate a new buffer instead of this one */
		skb = alloc_skb(rmnet_mhi_ptr->mru, GFP_ATOMIC);

		if (unlikely(!skb)) {
			rmnet_log(MSG_CRITICAL,
				  "Can't allocate a new RX buffer for MHI");
			break;
		}

		skb_reserve(skb, MHI_RX_HEADROOM);

		cb_ptr = (uintptr_t *)skb->cb;
		dma_addr = dma_map_single(&(dev->dev), skb->data,
					(rmnet_mhi_ptr->mru - MHI_RX_HEADROOM),
					DMA_FROM_DEVICE);
		*cb_ptr = (uintptr_t)dma_addr;

		if (unlikely(dma_mapping_error(&(dev->dev), dma_addr))) {
			rmnet_log(MSG_CRITICAL,
				  "DMA mapping error in polling function");
			dev_kfree_skb_irq(skb);
			break;
		}

		res = mhi_queue_xfer(
			rmnet_mhi_ptr->rx_client_handle,
			(uintptr_t)dma_addr, rmnet_mhi_ptr->mru, MHI_EOT);

		if (unlikely(MHI_STATUS_SUCCESS != res)) {
			rmnet_log(MSG_CRITICAL,
				"mhi_queue_xfer failed, error %d", res);
			dma_unmap_single(&(dev->dev), dma_addr,
					(rmnet_mhi_ptr->mru - MHI_RX_HEADROOM),
					DMA_FROM_DEVICE);

			dev_kfree_skb_irq(skb);
			break;
		}

		skb_queue_tail(&(rmnet_mhi_ptr->rx_buffers), skb);

	} /* while (received_packets < budget) or any other error */

	napi_complete(napi);

	/* We got a NULL descriptor back */
	if (should_reschedule == false) {
		if (atomic_read(&rmnet_mhi_ptr->irq_masked_cntr)) {
			atomic_dec(&rmnet_mhi_ptr->irq_masked_cntr);
			mhi_unmask_irq(rmnet_mhi_ptr->rx_client_handle);
		}
	} else {
		if (received_packets == budget)
			rx_napi_budget_overflow[rmnet_mhi_ptr->dev_index]++;
		napi_reschedule(napi);
	}

	rx_napi_skb_burst_min[rmnet_mhi_ptr->dev_index] =
	min((unsigned long)received_packets,
	    rx_napi_skb_burst_min[rmnet_mhi_ptr->dev_index]);

	rx_napi_skb_burst_max[rmnet_mhi_ptr->dev_index] =
	max((unsigned long)received_packets,
	    rx_napi_skb_burst_max[rmnet_mhi_ptr->dev_index]);

	rmnet_log(MSG_VERBOSE, "Exited, polled %d pkts\n", received_packets);
	return received_packets;
}

void rmnet_mhi_clean_buffers(struct net_device *dev)
{
	struct rmnet_mhi_private *rmnet_mhi_ptr =
		*(struct rmnet_mhi_private **)netdev_priv(dev);
	rmnet_log(MSG_INFO, "Entered\n");
	/* Clean TX buffers */
	rmnet_mhi_internal_clean_unmap_buffers(dev,
					       &(rmnet_mhi_ptr->tx_buffers),
					       DMA_TO_DEVICE);

	/* Clean RX buffers */
	rmnet_mhi_internal_clean_unmap_buffers(dev,
					       &(rmnet_mhi_ptr->rx_buffers),
					       DMA_FROM_DEVICE);
	rmnet_log(MSG_INFO, "Exited\n");
}

static int rmnet_mhi_disable_channels(struct rmnet_mhi_private *rmnet_mhi_ptr)
{
	rmnet_log(MSG_INFO, "Closing MHI TX channel\n");
	mhi_close_channel(rmnet_mhi_ptr->tx_client_handle);
	rmnet_log(MSG_INFO, "Closing MHI RX channel\n");
	mhi_close_channel(rmnet_mhi_ptr->rx_client_handle);
	rmnet_log(MSG_INFO, "Clearing Pending TX buffers.\n");
	rmnet_mhi_clean_buffers(rmnet_mhi_ptr->dev);
	rmnet_mhi_ptr->tx_client_handle = NULL;
	rmnet_mhi_ptr->rx_client_handle = NULL;

	return 0;
}

static int rmnet_mhi_init_inbound(struct rmnet_mhi_private *rmnet_mhi_ptr)
{
	u32 i;
	enum MHI_STATUS res;
	rmnet_log(MSG_INFO, "Entered\n");
	rmnet_mhi_ptr->tx_buffers_max =
		mhi_get_max_desc(
			rmnet_mhi_ptr->tx_client_handle);
	rmnet_mhi_ptr->rx_buffers_max =
		mhi_get_max_desc(
			rmnet_mhi_ptr->rx_client_handle);

	for (i = 0; i < rmnet_mhi_ptr->rx_buffers_max; i++) {
		struct sk_buff *skb = 0;
		dma_addr_t dma_addr;
		dma_addr_t *cb_ptr = 0;

		skb = alloc_skb(rmnet_mhi_ptr->mru,
				rmnet_mhi_ptr->allocation_flags);

		if (!skb) {
			rmnet_log(MSG_CRITICAL,
					"SKB allocation failure during open");
			return -ENOMEM;
		}

		skb_reserve(skb, MHI_RX_HEADROOM);
		cb_ptr = (dma_addr_t *)skb->cb;
		dma_addr = dma_map_single(&(rmnet_mhi_ptr->dev->dev), skb->data,
					 (rmnet_mhi_ptr->mru - MHI_RX_HEADROOM),
					 DMA_FROM_DEVICE);
		*cb_ptr = dma_addr;
		if (dma_mapping_error(&(rmnet_mhi_ptr->dev->dev), dma_addr)) {
			rmnet_log(MSG_CRITICAL,
				  "DMA mapping for RX buffers has failed");
			kfree_skb(skb);
			return -EIO;
		}
		skb_queue_tail(&(rmnet_mhi_ptr->rx_buffers), skb);
	}

	/* Submit the RX buffers */
	for (i = 0; i < rmnet_mhi_ptr->rx_buffers_max; i++) {
		struct sk_buff *skb = skb_dequeue(&(rmnet_mhi_ptr->rx_buffers));
		res = mhi_queue_xfer(rmnet_mhi_ptr->rx_client_handle,
					*((dma_addr_t *)(skb->cb)),
					rmnet_mhi_ptr->mru - MHI_RX_HEADROOM,
					MHI_EOT);
		if (MHI_STATUS_SUCCESS != res) {
			rmnet_log(MSG_CRITICAL,
					"mhi_queue_xfer failed, error %d", res);
			return -EIO;
		}
		skb_queue_tail(&(rmnet_mhi_ptr->rx_buffers), skb);
	}
	rmnet_log(MSG_INFO, "Exited\n");
	return 0;
}

static void rmnet_mhi_tx_cb(struct mhi_result *result)
{
	struct net_device *dev;
	struct rmnet_mhi_private *rmnet_mhi_ptr;
	unsigned long burst_counter = 0;
	unsigned long flags;

	rmnet_mhi_ptr = result->user_data;
	dev = rmnet_mhi_ptr->dev;
	tx_interrupts_count[rmnet_mhi_ptr->dev_index]++;

	rmnet_log(MSG_VERBOSE, "Entered\n");
	if (!result->payload_buf || !result->bytes_xferd)
		return;
	/* Free the buffers which are TX'd up to the provided address */
	while (!skb_queue_empty(&(rmnet_mhi_ptr->tx_buffers))) {
		struct sk_buff *skb =
			skb_dequeue(&(rmnet_mhi_ptr->tx_buffers));
		if (!skb) {
			rmnet_log(MSG_CRITICAL,
				  "NULL buffer returned, error");
			break;
		} else {
			struct tx_buffer_priv *tx_priv =
				(struct tx_buffer_priv *)(skb->cb);
			dma_addr_t dma_addr = tx_priv->dma_addr;
			int data_len = skb->len;

			dma_unmap_single(&(dev->dev),
					dma_addr,
					 skb->len,
					 DMA_TO_DEVICE);
			kfree_skb(skb);
			burst_counter++;

			/* Update statistics */
			dev->stats.tx_packets++;
			dev->stats.tx_bytes += data_len;

			/* The payload is expected to be the phy addr.
			   Comparing to see if it's the last skb to
			   replenish
			*/
			if (dma_addr ==
				result->payload_buf)
				break;
		}
	} /* While TX queue is not empty */
	tx_cb_skb_free_burst_min[rmnet_mhi_ptr->dev_index] =
		min(burst_counter,
		    tx_cb_skb_free_burst_min[rmnet_mhi_ptr->dev_index]);

	tx_cb_skb_free_burst_max[rmnet_mhi_ptr->dev_index] =
		max(burst_counter,
		    tx_cb_skb_free_burst_max[rmnet_mhi_ptr->dev_index]);

	/* In case we couldn't write again, now we can! */
	read_lock_irqsave(&rmnet_mhi_ptr->out_chan_full_lock, flags);
	netif_wake_queue(dev);
	read_unlock_irqrestore(&rmnet_mhi_ptr->out_chan_full_lock, flags);
	rmnet_log(MSG_VERBOSE, "Exited\n");
}

static void rmnet_mhi_rx_cb(struct mhi_result *result)
{
	struct net_device *dev;
	struct rmnet_mhi_private *rmnet_mhi_ptr;
	rmnet_mhi_ptr = result->user_data;
	dev = rmnet_mhi_ptr->dev;

	rmnet_log(MSG_VERBOSE, "Entered\n");
	rx_interrupts_count[rmnet_mhi_ptr->dev_index]++;

	if (napi_schedule_prep(&(rmnet_mhi_ptr->napi))) {
		mhi_mask_irq(rmnet_mhi_ptr->rx_client_handle);
		atomic_inc(&rmnet_mhi_ptr->irq_masked_cntr);
		__napi_schedule(&(rmnet_mhi_ptr->napi));
	} else {
		rx_interrupts_in_masked_irq[rmnet_mhi_ptr->dev_index]++;
	}
	rmnet_log(MSG_VERBOSE, "Exited\n");
}

static int rmnet_mhi_open(struct net_device *dev)
{
	struct rmnet_mhi_private *rmnet_mhi_ptr =
			*(struct rmnet_mhi_private **)netdev_priv(dev);

	rmnet_log(MSG_INFO,
			"Opened net dev interface for MHI chans %d and %d\n",
			rmnet_mhi_ptr->tx_channel,
			rmnet_mhi_ptr->rx_channel);
	netif_start_queue(dev);
	napi_enable(&(rmnet_mhi_ptr->napi));

	/* Poll to check if any buffers are accumulated in the
	 * transport buffers
	 */
	if (napi_schedule_prep(&(rmnet_mhi_ptr->napi))) {
		mhi_mask_irq(rmnet_mhi_ptr->rx_client_handle);
		atomic_inc(&rmnet_mhi_ptr->irq_masked_cntr);
		__napi_schedule(&(rmnet_mhi_ptr->napi));
	} else {
		rx_interrupts_in_masked_irq[rmnet_mhi_ptr->dev_index]++;
	}
	return 0;

}

static int rmnet_mhi_disable_iface(struct rmnet_mhi_private *rmnet_mhi_ptr)
{
	rmnet_mhi_ptr->rx_enabled = 0;
	rmnet_mhi_ptr->tx_enabled = 0;
	rmnet_mhi_ptr->mhi_enabled = 0;
	if (rmnet_mhi_ptr->dev != 0) {
		netif_stop_queue(rmnet_mhi_ptr->dev);
		netif_napi_del(&(rmnet_mhi_ptr->napi));
		rmnet_mhi_disable_channels(rmnet_mhi_ptr);
		unregister_netdev(rmnet_mhi_ptr->dev);
		free_netdev(rmnet_mhi_ptr->dev);
		rmnet_mhi_ptr->dev = 0;
	}
	return 0;
}

static int rmnet_mhi_disable(struct rmnet_mhi_private *rmnet_mhi_ptr)
{
	rmnet_mhi_ptr->mhi_enabled = 0;
	rmnet_mhi_disable_iface(rmnet_mhi_ptr);
	napi_disable(&(rmnet_mhi_ptr->napi));
	if (atomic_read(&rmnet_mhi_ptr->irq_masked_cntr)) {
		mhi_unmask_irq(rmnet_mhi_ptr->rx_client_handle);
		atomic_dec(&rmnet_mhi_ptr->irq_masked_cntr);
	}
	return 0;
}

static int rmnet_mhi_stop(struct net_device *dev)
{
	struct rmnet_mhi_private *rmnet_mhi_ptr =
		*(struct rmnet_mhi_private **)netdev_priv(dev);
	netif_stop_queue(dev);
	napi_disable(&(rmnet_mhi_ptr->napi));
	rmnet_log(MSG_VERBOSE, "Entered\n");
	if (atomic_read(&rmnet_mhi_ptr->irq_masked_cntr)) {
		mhi_unmask_irq(rmnet_mhi_ptr->rx_client_handle);
		atomic_dec(&rmnet_mhi_ptr->irq_masked_cntr);
		rmnet_log(MSG_ERROR, "IRQ was masked, unmasking...\n");
	}
	rmnet_log(MSG_VERBOSE, "Exited\n");
	return 0;
}

static int rmnet_mhi_change_mtu(struct net_device *dev, int new_mtu)
{
	if (0 > new_mtu || MHI_MAX_MTU < new_mtu)
		return -EINVAL;

	dev->mtu = new_mtu;
	return 0;
}

static int rmnet_mhi_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct rmnet_mhi_private *rmnet_mhi_ptr =
			*(struct rmnet_mhi_private **)netdev_priv(dev);
	enum MHI_STATUS res = MHI_STATUS_reserved;
	unsigned long flags;
	int retry = 0;
	struct tx_buffer_priv *tx_priv;
	dma_addr_t dma_addr;

	rmnet_log(MSG_VERBOSE, "Entered\n");
	dma_addr = dma_map_single(&(dev->dev), skb->data, skb->len,
				  DMA_TO_DEVICE);
	if (dma_mapping_error(&(dev->dev), dma_addr)) {
			rmnet_log(MSG_CRITICAL,
				"DMA mapping error in transmit function\n");
			return NETDEV_TX_BUSY;
	}

	/* DMA mapping is OK, need to update the cb field properly */
	tx_priv = (struct tx_buffer_priv *)(skb->cb);
	tx_priv->dma_addr = dma_addr;
	do {
		retry = 0;
		res = mhi_queue_xfer(rmnet_mhi_ptr->tx_client_handle,
				     dma_addr, skb->len, MHI_EOT);

		if (MHI_STATUS_RING_FULL == res) {
			write_lock_irqsave(&rmnet_mhi_ptr->out_chan_full_lock,
									flags);
			if (!mhi_get_free_desc(
					    rmnet_mhi_ptr->tx_client_handle)) {
				/* Stop writing until we can write again */
				tx_ring_full_count[rmnet_mhi_ptr->dev_index]++;
				netif_stop_queue(dev);
				goto rmnet_mhi_xmit_error_cleanup;
			} else {
				retry = 1;
			}
			write_unlock_irqrestore(
					&rmnet_mhi_ptr->out_chan_full_lock,
					flags);
		}
	} while (retry);

	if (MHI_STATUS_SUCCESS != res) {
		netif_stop_queue(dev);
		rmnet_log(MSG_CRITICAL,
			  "mhi_queue_xfer failed, error %d\n", res);
		goto rmnet_mhi_xmit_error_cleanup;
	}

	skb_queue_tail(&(rmnet_mhi_ptr->tx_buffers), skb);

	dev->trans_start = jiffies;

	tx_queued_packets_count[rmnet_mhi_ptr->dev_index]++;
	rmnet_log(MSG_VERBOSE, "Exited\n");
	return 0;

rmnet_mhi_xmit_error_cleanup:
	dma_unmap_single(&(dev->dev), dma_addr, skb->len,
			 DMA_TO_DEVICE);
	rmnet_log(MSG_VERBOSE, "Ring full\n");
	write_unlock_irqrestore(&rmnet_mhi_ptr->out_chan_full_lock, flags);
	return NETDEV_TX_BUSY;
}

static int rmnet_mhi_ioctl_extended(struct net_device *dev, struct ifreq *ifr)
{
	struct rmnet_ioctl_extended_s ext_cmd;
	int rc = 0;
	struct rmnet_mhi_private *rmnet_mhi_ptr =
			*(struct rmnet_mhi_private **)netdev_priv(dev);


	rc = copy_from_user(&ext_cmd, ifr->ifr_ifru.ifru_data,
			    sizeof(struct rmnet_ioctl_extended_s));

	if (rc) {
		rmnet_log(MSG_CRITICAL,
				"copy_from_user failed ,error %d", rc);
		return rc;
	}

	switch (ext_cmd.extended_ioctl) {
	case RMNET_IOCTL_SET_MRU:
		if ((0 > ext_cmd.u.data) || (ext_cmd.u.data > MHI_MAX_MRU)) {
			rmnet_log(MSG_CRITICAL,
				 "Can't set MRU, value %u is invalid\n",
				 ext_cmd.u.data);
			return -EINVAL;
		}
		rmnet_mhi_ptr->mru = ext_cmd.u.data;
		break;
	case RMNET_IOCTL_GET_EPID:
		ext_cmd.u.data =
			mhi_get_epid(rmnet_mhi_ptr->tx_client_handle);
		break;
	case RMNET_IOCTL_GET_SUPPORTED_FEATURES:
		ext_cmd.u.data = 0;
		break;
	case RMNET_IOCTL_GET_DRIVER_NAME:
		strlcpy(ext_cmd.u.if_name, RMNET_MHI_DRIVER_NAME,
			sizeof(ext_cmd.u.if_name));
		break;
	case RMNET_IOCTL_SET_SLEEP_STATE:
		if (rmnet_mhi_ptr->mhi_enabled &&
		    rmnet_mhi_ptr->tx_client_handle != NULL) {
			mhi_set_lpm(rmnet_mhi_ptr->tx_client_handle,
				   ext_cmd.u.data);
		} else {
			rmnet_log(MSG_ERROR,
				  "Cannot set LPM value, MHI is not up.\n");
			return -ENODEV;
		}
		break;
	default:
		rc = -EINVAL;
		break;
	}

	rc = copy_to_user(ifr->ifr_ifru.ifru_data, &ext_cmd,
			  sizeof(struct rmnet_ioctl_extended_s));

	if (rc)
		rmnet_log(MSG_CRITICAL,
				"copy_to_user failed, error %d\n",
				rc);

	return rc;
}

static int rmnet_mhi_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	int rc = 0;
	struct rmnet_ioctl_data_s ioctl_data;

	switch (cmd) {
	case RMNET_IOCTL_SET_LLP_IP:        /* Set RAWIP protocol */
		break;
	case RMNET_IOCTL_GET_LLP:           /* Get link protocol state */
		ioctl_data.u.operation_mode = RMNET_MODE_LLP_IP;
		if (copy_to_user(ifr->ifr_ifru.ifru_data, &ioctl_data,
		    sizeof(struct rmnet_ioctl_data_s)))
			rc = -EFAULT;
		break;
	case RMNET_IOCTL_GET_OPMODE:        /* Get operation mode      */
		ioctl_data.u.operation_mode = RMNET_MODE_LLP_IP;
		if (copy_to_user(ifr->ifr_ifru.ifru_data, &ioctl_data,
		    sizeof(struct rmnet_ioctl_data_s)))
			rc = -EFAULT;
		break;
	case RMNET_IOCTL_SET_QOS_ENABLE:
		rc = -EINVAL;
		break;
	case RMNET_IOCTL_SET_QOS_DISABLE:
		rc = 0;
		break;
	case RMNET_IOCTL_OPEN:
	case RMNET_IOCTL_CLOSE:
		/* We just ignore them and return success */
		rc = 0;
		break;
	case RMNET_IOCTL_EXTENDED:
		rc = rmnet_mhi_ioctl_extended(dev, ifr);
		break;
	default:
		/* Don't fail any IOCTL right now */
		rc = 0;
		break;
	}

	return rc;
}

static const struct net_device_ops rmnet_mhi_ops_ip = {
	.ndo_open = rmnet_mhi_open,
	.ndo_stop = rmnet_mhi_stop,
	.ndo_start_xmit = rmnet_mhi_xmit,
	.ndo_do_ioctl = rmnet_mhi_ioctl,
	.ndo_change_mtu = rmnet_mhi_change_mtu,
	.ndo_set_mac_address = 0,
	.ndo_validate_addr = 0,
};

static void rmnet_mhi_setup(struct net_device *dev)
{
	dev->netdev_ops = &rmnet_mhi_ops_ip;
	ether_setup(dev);

	/* set this after calling ether_setup */
	dev->header_ops = 0;  /* No header */
	dev->type = ARPHRD_RAWIP;
	dev->hard_header_len = 0;
	dev->mtu = MHI_DEFAULT_MTU;
	dev->addr_len = 0;
	dev->flags &= ~(IFF_BROADCAST | IFF_MULTICAST);
	dev->watchdog_timeo = WATCHDOG_TIMEOUT;
}

static int rmnet_mhi_enable_iface(struct rmnet_mhi_private *rmnet_mhi_ptr)
{
	int ret = 0;
	struct rmnet_mhi_private **rmnet_mhi_ctxt = NULL;
	enum MHI_STATUS r = MHI_STATUS_SUCCESS;

	memset(tx_interrupts_count, 0, sizeof(tx_interrupts_count));
	memset(rx_interrupts_count, 0, sizeof(rx_interrupts_count));
	memset(rx_interrupts_in_masked_irq, 0,
	       sizeof(rx_interrupts_in_masked_irq));
	memset(rx_napi_skb_burst_min, 0, sizeof(rx_napi_skb_burst_min));
	memset(rx_napi_skb_burst_max, 0, sizeof(rx_napi_skb_burst_max));
	memset(tx_cb_skb_free_burst_min, 0, sizeof(tx_cb_skb_free_burst_min));
	memset(tx_cb_skb_free_burst_max, 0, sizeof(tx_cb_skb_free_burst_max));
	memset(tx_ring_full_count, 0, sizeof(tx_ring_full_count));
	memset(tx_queued_packets_count, 0, sizeof(tx_queued_packets_count));
	memset(rx_napi_budget_overflow, 0, sizeof(rx_napi_budget_overflow));

	rmnet_log(MSG_INFO, "Entered.\n");

	if (rmnet_mhi_ptr == NULL) {
		rmnet_log(MSG_CRITICAL, "Bad input args.\n");
		return -EINVAL;
	}

	rx_napi_skb_burst_min[rmnet_mhi_ptr->dev_index] = UINT_MAX;
	tx_cb_skb_free_burst_min[rmnet_mhi_ptr->dev_index] = UINT_MAX;

	skb_queue_head_init(&(rmnet_mhi_ptr->tx_buffers));
	skb_queue_head_init(&(rmnet_mhi_ptr->rx_buffers));

	if (rmnet_mhi_ptr->tx_client_handle != NULL) {
		rmnet_log(MSG_INFO,
			"Opening TX channel\n");
		r = mhi_open_channel(rmnet_mhi_ptr->tx_client_handle);
		if (r != MHI_STATUS_SUCCESS) {
			rmnet_log(MSG_CRITICAL,
				"Failed to start TX chan ret %d\n", r);
			goto mhi_tx_chan_start_fail;
		} else {
			rmnet_mhi_ptr->tx_enabled = 1;
		}
	}
	if (rmnet_mhi_ptr->rx_client_handle != NULL) {
		rmnet_log(MSG_INFO,
			"Opening RX channel\n");
		r = mhi_open_channel(rmnet_mhi_ptr->rx_client_handle);
		if (r != MHI_STATUS_SUCCESS) {
			rmnet_log(MSG_CRITICAL,
				"Failed to start RX chan ret %d\n", r);
			goto mhi_rx_chan_start_fail;
		} else {
			rmnet_mhi_ptr->rx_enabled = 1;
		}
	}
	rmnet_mhi_ptr->dev =
		alloc_netdev(sizeof(struct rmnet_mhi_private *),
			     RMNET_MHI_DEV_NAME, rmnet_mhi_setup);
	if (!rmnet_mhi_ptr->dev) {
		rmnet_log(MSG_CRITICAL, "Network device allocation failed\n");
		ret = -ENOMEM;
		goto net_dev_alloc_fail;
	}

	rmnet_mhi_ctxt = netdev_priv(rmnet_mhi_ptr->dev);
	*rmnet_mhi_ctxt = rmnet_mhi_ptr;

	ret = dma_set_mask(&(rmnet_mhi_ptr->dev->dev),
						MHI_DMA_MASK);
	if (ret)
		rmnet_mhi_ptr->allocation_flags = GFP_KERNEL;
	else
		rmnet_mhi_ptr->allocation_flags = GFP_DMA;

	r = rmnet_mhi_init_inbound(rmnet_mhi_ptr);
	if (r) {
		rmnet_log(MSG_CRITICAL,
			"Failed to init inbound ret %d\n", r);
	}

	netif_napi_add(rmnet_mhi_ptr->dev, &(rmnet_mhi_ptr->napi),
		       rmnet_mhi_poll, MHI_NAPI_WEIGHT_VALUE);

	rmnet_mhi_ptr->mhi_enabled = 1;
	ret = register_netdev(rmnet_mhi_ptr->dev);
	if (ret) {
		rmnet_log(MSG_CRITICAL,
			  "Network device registration failed\n");
		goto net_dev_reg_fail;
	}

	netif_start_queue(rmnet_mhi_ptr->dev);
	rmnet_log(MSG_INFO, "Exited.\n");

	return 0;

net_dev_reg_fail:
	netif_napi_del(&(rmnet_mhi_ptr->napi));
	free_netdev(rmnet_mhi_ptr->dev);
net_dev_alloc_fail:
	mhi_close_channel(rmnet_mhi_ptr->rx_client_handle);
	rmnet_mhi_ptr->dev = NULL;
mhi_rx_chan_start_fail:
	mhi_close_channel(rmnet_mhi_ptr->tx_client_handle);
mhi_tx_chan_start_fail:
	rmnet_log(MSG_INFO, "Exited ret %d.\n", ret);
	return ret;
}

static void rmnet_mhi_cb(struct mhi_cb_info *cb_info)
{
	struct rmnet_mhi_private *rmnet_mhi_ptr;
	struct mhi_result *result;
	enum MHI_STATUS r = MHI_STATUS_SUCCESS;

	if (NULL != cb_info && NULL != cb_info->result) {
		result = cb_info->result;
		rmnet_mhi_ptr = result->user_data;
	} else {
		rmnet_log(MSG_CRITICAL,
			"Invalid data in MHI callback, quitting\n");
	}

	switch (cb_info->cb_reason) {
	case MHI_CB_MHI_DISABLED:
		rmnet_log(MSG_CRITICAL,
			"Got MHI_DISABLED notification. Stopping stack\n");
		if (rmnet_mhi_ptr->mhi_enabled)
			rmnet_mhi_disable(rmnet_mhi_ptr);
		break;
	case MHI_CB_MHI_ENABLED:
		rmnet_log(MSG_CRITICAL,
			"Got MHI_ENABLED notification. Starting stack\n");
		if (IS_INBOUND(cb_info->chan))
			rmnet_mhi_ptr->rx_enabled = 1;
		else
			rmnet_mhi_ptr->tx_enabled = 1;

		if (rmnet_mhi_ptr->tx_enabled &&
		    rmnet_mhi_ptr->rx_enabled) {
			rmnet_log(MSG_INFO,
			"Both RX/TX are enabled, enabling iface.\n");
			r = rmnet_mhi_enable_iface(rmnet_mhi_ptr);
			if (r)
				rmnet_log(MSG_CRITICAL,
					"Failed to enable iface for chan %d\n",
					cb_info->chan);
			else
				rmnet_log(MSG_INFO,
					"Enabled iface for chan %d\n",
					cb_info->chan);
		}
		break;
	case MHI_CB_XFER:
		if (IS_INBOUND(cb_info->chan))
			rmnet_mhi_rx_cb(cb_info->result);
		else
			rmnet_mhi_tx_cb(cb_info->result);
		break;
	default:
		break;
	}
}

static struct mhi_client_info_t rmnet_mhi_info = {rmnet_mhi_cb};

static int __init rmnet_mhi_init(void)
{
	int i;
	enum MHI_STATUS res = MHI_STATUS_SUCCESS;
	struct rmnet_mhi_private *rmnet_mhi_ptr = 0;
	rmnet_ipc_log = ipc_log_context_create(RMNET_IPC_LOG_PAGES,
						"mhi_rmnet", 0);

	for (i = 0; i < MHI_RMNET_DEVICE_COUNT; i++) {
		rmnet_mhi_ptr = &rmnet_mhi_ctxt_list[i];

		rmnet_mhi_ptr->tx_channel = MHI_CLIENT_IP_HW_0_OUT +
				(enum MHI_CLIENT_CHANNEL)(i * 2);
		rmnet_mhi_ptr->rx_channel = MHI_CLIENT_IP_HW_0_IN +
				(enum MHI_CLIENT_CHANNEL)((i * 2));

		rmnet_mhi_ptr->tx_client_handle = 0;
		rmnet_mhi_ptr->rx_client_handle = 0;
		rwlock_init(&rmnet_mhi_ptr->out_chan_full_lock);

		rmnet_mhi_ptr->mru = MHI_DEFAULT_MRU;
		rmnet_mhi_ptr->dev_index = i;

		res = mhi_register_channel(
			&(rmnet_mhi_ptr->tx_client_handle),
			rmnet_mhi_ptr->tx_channel, 0,
			&rmnet_mhi_info, rmnet_mhi_ptr);

		if (MHI_STATUS_SUCCESS != res) {
			rmnet_mhi_ptr->tx_client_handle = 0;
			rmnet_log(MSG_CRITICAL,
				"mhi_register_channel failed chan %d ret %d\n",
				rmnet_mhi_ptr->tx_channel, res);
		}
		res = mhi_register_channel(
			&(rmnet_mhi_ptr->rx_client_handle),
			rmnet_mhi_ptr->rx_channel, 0,
			&rmnet_mhi_info, rmnet_mhi_ptr);

		if (MHI_STATUS_SUCCESS != res) {
			rmnet_mhi_ptr->rx_client_handle = 0;
			rmnet_log(MSG_CRITICAL,
				"mhi_register_channel failed chan %d, ret %d\n",
				rmnet_mhi_ptr->rx_channel, res);
		}
	}
	return 0;
}

static void __exit rmnet_mhi_exit(void)
{
	struct rmnet_mhi_private *rmnet_mhi_ptr = 0;
	int index = 0;

	for (index = 0; index < MHI_RMNET_DEVICE_COUNT; index++) {
		rmnet_mhi_ptr = &rmnet_mhi_ctxt_list[index];
		mhi_deregister_channel(rmnet_mhi_ptr->tx_client_handle);
		mhi_deregister_channel(rmnet_mhi_ptr->rx_client_handle);
	}
}

module_exit(rmnet_mhi_exit);
module_init(rmnet_mhi_init);

MODULE_DESCRIPTION("MHI RMNET Network Interface");
MODULE_LICENSE("GPL v2");
