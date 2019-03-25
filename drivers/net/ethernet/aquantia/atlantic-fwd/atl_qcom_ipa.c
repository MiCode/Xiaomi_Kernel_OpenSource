/* Copyright (c) 2019 The Linux Foundation. All rights reserved.
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

#include <linux/pci.h>

#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/ipa_eth.h>

#include "atl_fwd.h"
#include "atl_qcom_ipa.h"

#define ATL_IPA_DEFAULT_RING_SZ 256
#define ATL_IPA_DEFAULT_BUFF_SZ 2048

static inline struct atl_fwd_ring *CH_RING(struct ipa_eth_channel *ch)
{
	return (struct atl_fwd_ring *)(ch->nd_priv);
}

static int atl_ipa_open_device(struct ipa_eth_device *eth_dev)
{
	struct atl_nic *nic = (struct atl_nic *)dev_get_drvdata(eth_dev->dev);

	if (!nic || !nic->ndev) {
		dev_err(eth_dev->dev, "Invalid atl_nic");
		return -ENODEV;
	}

	/* atl specific init, ref counting go here */

	eth_dev->nd_priv = nic;
	eth_dev->net_dev = nic->ndev;

	return 0;
}

static void atl_ipa_close_device(struct ipa_eth_device *eth_dev)
{
	eth_dev->nd_priv = NULL;
	eth_dev->net_dev = NULL;
}

static struct ipa_eth_channel *atl_ipa_request_channel(
	struct ipa_eth_device *eth_dev, enum ipa_eth_channel_dir dir,
	unsigned long features, unsigned long events)
{
	struct atl_fwd_ring *ring = NULL;
	enum atl_fwd_ring_flags ring_flags = 0;
	struct ipa_eth_channel *channel = NULL;

	switch (dir) {
	case IPA_ETH_DIR_RX:
		break;
	case IPA_ETH_DIR_TX:
		ring_flags |= ATL_FWR_TX;
		break;
	default:
		dev_err(eth_dev->dev, "Unsupported direction %d", dir);
		return NULL;
	}

	ring_flags |= ATL_FWR_ALLOC_BUFS;
	ring_flags |= ATL_FWR_CONTIG_BUFS;

	ring = atl_fwd_request_ring(eth_dev->net_dev, ring_flags,
				    ATL_IPA_DEFAULT_RING_SZ,
				    ATL_IPA_DEFAULT_BUFF_SZ, 1);
	if (IS_ERR_OR_NULL(ring)) {
		dev_err(eth_dev->dev, "Request ring failed");
		goto err_exit;
	}

	channel = kzalloc(sizeof(*channel), GFP_KERNEL);
	if (!channel)
		goto err_exit;

	channel->events = 0;
	channel->features = 0;
	channel->direction = dir;
	channel->queue = ring->idx;

	channel->desc_size = 16;
	channel->desc_count = ring->hw.size;
	channel->desc_mem.size = channel->desc_size * channel->desc_count;

	channel->desc_mem.vaddr = ring->hw.descs;
	channel->desc_mem.daddr = ring->hw.daddr;
	channel->desc_mem.paddr =
		page_to_phys(vmalloc_to_page(channel->desc_mem.vaddr));

	channel->buff_size = ATL_IPA_DEFAULT_BUFF_SZ;
	channel->buff_count = channel->desc_count;
	channel->buff_mem.size = channel->buff_size * channel->buff_count;

	channel->buff_mem.vaddr = (void *)ring->bufs->vaddr_vec;
	channel->buff_mem.daddr = ring->bufs->daddr_vec_base;
	channel->buff_mem.paddr = virt_to_phys((void *)ring->bufs->vaddr_vec);

	channel->eth_dev = eth_dev;
	channel->nd_priv = ring;

	return channel;

err_exit:
	kzfree(channel);

	if (!IS_ERR_OR_NULL(ring)) {
		atl_fwd_release_ring(ring);
		ring = NULL;
	}

	return NULL;
}

static void atl_ipa_release_channel(struct ipa_eth_channel *ch)
{
	atl_fwd_release_ring(CH_RING(ch));
	kzfree(ch);
}

static int atl_ipa_enable_channel(struct ipa_eth_channel *ch)
{
	return atl_fwd_enable_ring(CH_RING(ch));
}

static int atl_ipa_disable_channel(struct ipa_eth_channel *ch)
{
	atl_fwd_disable_ring(CH_RING(ch));

	return 0;
}

static int atl_ipa_request_event(struct ipa_eth_channel *ch,
				 unsigned long ipa_event,
				 phys_addr_t addr, u64 data)
{
	int rc = 0;
	struct atl_fwd_event *event = NULL;
	struct atl_fwd_event atl_event = {0};
	struct ipa_eth_device *eth_dev = ch->eth_dev;

	switch (ipa_event) {
	case IPA_ETH_DEV_EV_RX_INT:
		if (ch->direction != IPA_ETH_DIR_RX) {
			dev_err(eth_dev->dev,
				"Rx interrupt requested on incorrect channel");
			return -EFAULT;
		}

		atl_event.msi_addr = dma_map_resource(eth_dev->dev,
						      addr, sizeof(u32),
						      DMA_FROM_DEVICE, 0);
		atl_event.msi_data = (u32)data;
		break;

	case IPA_ETH_DEV_EV_TX_INT:
		if (ch->direction != IPA_ETH_DIR_TX) {
			dev_err(eth_dev->dev,
				"Tx interrupt requested on incorrect channel");
			return -EFAULT;
		}

		atl_event.msi_addr = dma_map_resource(eth_dev->dev,
						      addr, sizeof(u32),
						      DMA_FROM_DEVICE, 0);
		atl_event.msi_data = (u32)data;
		break;

	case IPA_ETH_DEV_EV_TX_PTR:
		if (ch->direction != IPA_ETH_DIR_TX) {
			dev_err(eth_dev->dev,
				"Tx ptr wrb requested on incorrect channel");
			return -EFAULT;
		}

		atl_event.flags = ATL_FWD_EVT_TXWB;
		atl_event.tx_head_wrb = dma_map_resource(eth_dev->dev,
							 addr, sizeof(u32),
							 DMA_FROM_DEVICE, 0);
		break;

	default:
		dev_err(eth_dev->dev, "Unsupported event requested");
		return -ENODEV;
	}

	event = kzalloc(sizeof(*event), GFP_KERNEL);
	if (!event)
		return -ENOMEM;

	*event = atl_event;
	event->ring = CH_RING(ch);

	rc = atl_fwd_request_event(event);
	if (rc)
		kfree(event);

	return rc;
}

static void atl_ipa_release_event(struct ipa_eth_channel *ch,
				  unsigned long ipa_event)
{
	dma_addr_t daddr;
	struct atl_fwd_event *event = CH_RING(ch)->evt;
	struct ipa_eth_device *eth_dev = ch->eth_dev;

	switch (ipa_event) {
	case IPA_ETH_DEV_EV_RX_INT:
	case IPA_ETH_DEV_EV_TX_INT:
		daddr = event->msi_addr;
		break;

	case IPA_ETH_DEV_EV_TX_PTR:
		daddr = event->tx_head_wrb;
		break;

	default:
		dev_err(eth_dev->dev, "Unsupported event for release");
		return;
	}

	/* An atl ring can have only one associated event */
	atl_fwd_release_event(event);

	dma_unmap_resource(eth_dev->dev,
			   daddr, sizeof(u32), DMA_FROM_DEVICE, 0);
}

static int atl_ipa_enable_event(struct ipa_eth_channel *ch,
				unsigned long event)
{
	/* An atl ring can have only one associated event */
	return atl_fwd_enable_event(CH_RING(ch)->evt);
}

static int atl_ipa_disable_event(struct ipa_eth_channel *ch,
				 unsigned long event)
{
	/* An atl ring can have only one associated event */
	return atl_fwd_disable_event(CH_RING(ch)->evt);
}

int atl_ipa_moderate_event(struct ipa_eth_channel *ch, unsigned long event,
			   u64 min_count, u64 max_count,
			   u64 min_usecs, u64 max_usecs)
{
	return atl_fwd_set_ring_intr_mod(CH_RING(ch), min_usecs, max_usecs);
}

static int atl_ipa_receive_skb(struct ipa_eth_device *eth_dev,
			       struct sk_buff *skb)
{
	return atl_fwd_receive_skb(eth_dev->net_dev, skb);
}

static int atl_ipa_transmit_skb(struct ipa_eth_device *eth_dev,
				struct sk_buff *skb)
{
	return atl_fwd_transmit_skb(eth_dev->net_dev, skb);
}

struct ipa_eth_net_ops atl_net_ops = {
	.open_device = atl_ipa_open_device,
	.close_device = atl_ipa_close_device,
	.request_channel = atl_ipa_request_channel,
	.release_channel = atl_ipa_release_channel,
	.enable_channel = atl_ipa_enable_channel,
	.disable_channel = atl_ipa_disable_channel,
	.request_event = atl_ipa_request_event,
	.release_event = atl_ipa_release_event,
	.enable_event = atl_ipa_enable_event,
	.disable_event = atl_ipa_disable_event,
	.moderate_event = atl_ipa_moderate_event,
	.receive_skb = atl_ipa_receive_skb,
	.transmit_skb = atl_ipa_transmit_skb,
};

static struct ipa_eth_net_driver atl_net_driver = {
	.events =
		IPA_ETH_DEV_EV_RX_INT |
		IPA_ETH_DEV_EV_TX_INT |
		IPA_ETH_DEV_EV_TX_PTR,
	.features =
		IPA_ETH_DEV_F_L2_CSUM |
		IPA_ETH_DEV_F_L3_CSUM |
		IPA_ETH_DEV_F_TCP_CSUM |
		IPA_ETH_DEV_F_UDP_CSUM |
		IPA_ETH_DEV_F_LSO |
		IPA_ETH_DEV_F_LRO |
		IPA_ETH_DEV_F_VLAN |
		IPA_ETH_DEV_F_MODC |
		IPA_ETH_DEV_F_MODT,
	.bus = &pci_bus_type,
	.ops = &atl_net_ops,
};

int atl_qcom_ipa_register(struct pci_driver *pdrv)
{
	if (!atl_net_driver.name)
		atl_net_driver.name = pdrv->name;

	if (!atl_net_driver.driver)
		atl_net_driver.driver = &pdrv->driver;

	return ipa_eth_register_net_driver(&atl_net_driver);
}

void atl_qcom_ipa_unregister(struct pci_driver *pdrv)
{
	ipa_eth_unregister_net_driver(&atl_net_driver);
}
