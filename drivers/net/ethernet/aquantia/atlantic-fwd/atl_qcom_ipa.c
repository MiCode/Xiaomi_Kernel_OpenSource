/* Copyright (c) 2019-2020 The Linux Foundation. All rights reserved.
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

#define IPA_ETH_NET_DRIVER
#include <linux/ipa_eth.h>

#include "atl_fwd.h"
#include "atl_qcom_ipa.h"

#if ATL_FWD_API_VERSION >= 2 && IPA_ETH_API_VER >= 4
#define ATL_IPA_SUPPORT_NOTIFY
#endif

struct atl_ipa_device {
	struct atl_nic *atl_nic;
	struct ipa_eth_device *eth_dev;
	struct notifier_block fwd_notify_nb;
};

static inline struct atl_fwd_ring *CH_RING(struct ipa_eth_channel *ch)
{
	return (struct atl_fwd_ring *)(ch->nd_priv);
}

static void *atl_ipa_dma_alloc(struct ipa_eth_device *eth_dev,
			       size_t size, dma_addr_t *daddr, gfp_t gfp,
			       struct ipa_eth_dma_allocator *dma_allocator)
{
	struct ipa_eth_resource mem;

	if (dma_allocator->alloc(eth_dev, size, gfp, &mem))
		return NULL;

	if (daddr)
		*daddr = mem.daddr;

	return mem.vaddr;
}

static void atl_ipa_dma_free(void *buf, struct ipa_eth_device *eth_dev,
			     size_t size, dma_addr_t daddr,
			     struct ipa_eth_dma_allocator *dma_allocator)
{
	struct ipa_eth_resource mem = {
		.size = size,
		.vaddr = buf,
		.daddr = daddr,
	};

	return dma_allocator->free(eth_dev, &mem);
}

static void *atl_ipa_alloc_descs(struct device *dev, size_t size,
				 dma_addr_t *daddr, gfp_t gfp,
				 struct atl_fwd_mem_ops *ops)
{
	struct ipa_eth_channel *ch = ops->private;

	return atl_ipa_dma_alloc(ch->eth_dev, size, daddr, gfp,
			ch->mem_params.desc.allocator);
}

static void *atl_ipa_alloc_buf(struct device *dev, size_t size,
			       dma_addr_t *daddr, gfp_t gfp,
			       struct atl_fwd_mem_ops *ops)
{
	struct ipa_eth_channel *ch = ops->private;

	return atl_ipa_dma_alloc(ch->eth_dev, size, daddr, gfp,
			ch->mem_params.buff.allocator);
}

static void atl_ipa_free_descs(void *buf, struct device *dev, size_t size,
			       dma_addr_t daddr, struct atl_fwd_mem_ops *ops)
{
	struct ipa_eth_channel *ch = ops->private;

	return atl_ipa_dma_free(buf, ch->eth_dev, size, daddr,
			ch->mem_params.desc.allocator);
}

static void atl_ipa_free_buf(void *buf, struct device *dev, size_t size,
			     dma_addr_t daddr, struct atl_fwd_mem_ops *ops)
{
	struct ipa_eth_channel *ch = ops->private;

	return atl_ipa_dma_free(buf, ch->eth_dev, size, daddr,
			ch->mem_params.desc.allocator);
}

#ifdef ATL_IPA_SUPPORT_NOTIFY
static int atl_ipa_fwd_notification(struct notifier_block *nb,
				    unsigned long action, void *data)
{
	enum atl_fwd_notify notif = action;
	struct atl_ipa_device *ai_dev = container_of(
		nb, struct atl_ipa_device, fwd_notify_nb);

	switch (notif) {
	case ATL_FWD_NOTIFY_RESET_PREPARE:
		ipa_eth_device_notify(ai_dev->eth_dev,
				      IPA_ETH_DEV_RESET_PREPARE, NULL);
		break;
	case ATL_FWD_NOTIFY_RESET_COMPLETE:
		ipa_eth_device_notify(ai_dev->eth_dev,
				      IPA_ETH_DEV_RESET_COMPLETE, NULL);
		break;
	default:
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}
#endif

static int atl_ipa_open_device(struct ipa_eth_device *eth_dev)
{
	struct atl_ipa_device *ai_dev;
	struct atl_nic *nic = (struct atl_nic *)dev_get_drvdata(eth_dev->dev);

	if (!nic || !nic->ndev) {
		dev_err(eth_dev->dev, "Invalid atl_nic\n");
		return -ENODEV;
	}

	ai_dev = kzalloc(sizeof(*ai_dev), GFP_KERNEL);
	if (!ai_dev)
		return -ENOMEM;

	/* atl specific init, ref counting go here */

	ai_dev->atl_nic = nic;
	ai_dev->eth_dev = eth_dev;

	eth_dev->nd_priv = ai_dev;
	eth_dev->net_dev = nic->ndev;

#ifdef ATL_IPA_SUPPORT_NOTIFY
	ai_dev->fwd_notify_nb.notifier_call = atl_ipa_fwd_notification;

	if (atl_fwd_register_notifier(nic->ndev, &ai_dev->fwd_notify_nb)) {
		dev_err(eth_dev->dev, "Failed to register notifier\n");

		eth_dev->nd_priv = NULL;
		eth_dev->net_dev = NULL;
		kfree(ai_dev);

		return -EFAULT;
	}
#endif

	return 0;
}

static void atl_ipa_close_device(struct ipa_eth_device *eth_dev)
{
	struct atl_ipa_device *ai_dev = eth_dev->nd_priv;

#ifdef ATL_IPA_SUPPORT_NOTIFY
	atl_fwd_unregister_notifier(ai_dev->atl_nic->ndev,
				    &ai_dev->fwd_notify_nb);
#endif

	eth_dev->nd_priv = NULL;
	eth_dev->net_dev = NULL;

	memset(ai_dev, 0, sizeof(ai_dev));
	kfree(ai_dev);
}

static struct ipa_eth_channel *atl_ipa_request_channel(
	struct ipa_eth_device *eth_dev, enum ipa_eth_channel_dir dir,
	unsigned long events, unsigned long features,
	const struct ipa_eth_channel_mem_params *mem_params)
{
	struct atl_fwd_ring *ring = NULL;
	enum atl_fwd_ring_flags ring_flags = 0;
	struct ipa_eth_channel *channel = NULL;
	struct atl_fwd_mem_ops *mem_ops = NULL;
	struct ipa_eth_channel_mem *desc_mem = NULL;
	struct ipa_eth_channel_mem *buff_mem = NULL;
	size_t desc_count;
	size_t buff_size;

	channel =
		ipa_eth_net_alloc_channel(eth_dev, dir,
					  events, features, mem_params);
	if (!channel) {
		dev_err(eth_dev->dev, "Failed to alloc ipa eth channel\n");
		goto err_channel;
	}

	desc_count = channel->mem_params.desc.count;
	buff_size = channel->mem_params.buff.size;

	mem_ops = kzalloc(sizeof(*mem_ops), GFP_KERNEL);
	if (!mem_ops)
		goto err_mem_ops;

	mem_ops->alloc_descs = atl_ipa_alloc_descs;
	mem_ops->alloc_buf = atl_ipa_alloc_buf;
	mem_ops->free_descs = atl_ipa_free_descs;
	mem_ops->free_buf = atl_ipa_free_buf;
	mem_ops->private = channel;

	switch (dir) {
	case IPA_ETH_DIR_RX:
		break;
	case IPA_ETH_DIR_TX:
		ring_flags |= ATL_FWR_TX;
		break;
	default:
		dev_err(eth_dev->dev, "Unsupported direction %d\n", dir);
		goto err_dir;
	}

	ring_flags |= ATL_FWR_ALLOC_BUFS;
	ring_flags |= ATL_FWR_CONTIG_BUFS;

	ring = atl_fwd_request_ring(eth_dev->net_dev, ring_flags,
				    desc_count, buff_size, 1, mem_ops);
	if (IS_ERR_OR_NULL(ring)) {
		dev_err(eth_dev->dev, "Request ring failed\n");
		goto err_ring;
	}

	channel->nd_priv = ring;
	channel->queue = ring->idx;

	desc_mem = kzalloc(sizeof(*desc_mem), GFP_KERNEL);
	if (!desc_mem)
		goto err_desc_mem;

	channel->mem_params.desc.size = 16;
	channel->mem_params.desc.count = ring->hw.size;

	desc_mem->mem.size =
		channel->mem_params.desc.size * channel->mem_params.desc.count;
	desc_mem->mem.vaddr = ring->hw.descs;
	desc_mem->mem.daddr = ring->hw.daddr;
	desc_mem->mem.paddr = channel->mem_params.desc.allocator->paddr(
				eth_dev, desc_mem->mem.vaddr);

	buff_mem = kzalloc(sizeof(*buff_mem), GFP_KERNEL);
	if (!buff_mem)
		goto err_buff_mem;

	channel->mem_params.buff.size = buff_size;
	channel->mem_params.buff.count = channel->mem_params.desc.count;

	buff_mem->mem.size =
		channel->mem_params.buff.size * channel->mem_params.buff.count;
	buff_mem->mem.vaddr = (void *)ring->bufs->vaddr_vec;
	buff_mem->mem.daddr = ring->bufs->daddr_vec_base;
	buff_mem->mem.paddr = channel->mem_params.buff.allocator->paddr(
				eth_dev, buff_mem->mem.vaddr);

	list_add(&desc_mem->mem_list_entry, &channel->desc_mem);
	list_add(&buff_mem->mem_list_entry, &channel->buff_mem);

	return channel;

err_buff_mem:
	kzfree(desc_mem);
err_desc_mem:
	atl_fwd_release_ring(ring);
err_ring:
err_dir:
	if (mem_ops)
		kzfree(mem_ops);
err_mem_ops:
	ipa_eth_net_free_channel(channel);
err_channel:
	return NULL;
}

static void atl_ipa_release_channel(struct ipa_eth_channel *ch)
{
	struct ipa_eth_channel_mem *mem, *tmp;
	struct atl_fwd_ring *ring = CH_RING(ch);
	struct atl_fwd_mem_ops *mem_ops = ring->mem_ops;

	atl_fwd_release_ring(ring);

	if (mem_ops)
		kzfree(mem_ops);

	list_for_each_entry_safe(mem, tmp, &ch->desc_mem, mem_list_entry) {
		list_del(&mem->mem_list_entry);
		kzfree(mem);
	}

	list_for_each_entry_safe(mem, tmp, &ch->buff_mem, mem_list_entry) {
		list_del(&mem->mem_list_entry);
		kzfree(mem);
	}

	ipa_eth_net_free_channel(ch);
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
				"Rx interrupt requested on tx channel\n");
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
				"Tx interrupt requested on rx channel\n");
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
				"Tx ptr wrb requested on rx channel\n");
			return -EFAULT;
		}

		atl_event.flags = ATL_FWD_EVT_TXWB;
		atl_event.tx_head_wrb = dma_map_resource(eth_dev->dev,
							 addr, sizeof(u32),
							 DMA_FROM_DEVICE, 0);
		break;

	default:
		dev_err(eth_dev->dev, "Unsupported event requested\n");
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
		dev_err(eth_dev->dev, "Unsupported event for release\n");
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

#if IPA_ETH_API_VER >= 7
static int atl_ipa_receive_skb(struct ipa_eth_device *eth_dev,
			       struct sk_buff *skb, bool in_napi)
{
	return in_napi ?
		atl_fwd_napi_receive_skb(eth_dev->net_dev, skb) :
		atl_fwd_receive_skb(eth_dev->net_dev, skb);
}
#else
static int atl_ipa_receive_skb(struct ipa_eth_device *eth_dev,
			       struct sk_buff *skb)
{
	return atl_fwd_receive_skb(eth_dev->net_dev, skb);
}
#endif

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
