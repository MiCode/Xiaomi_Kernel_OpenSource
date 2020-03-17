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

#include "ipa_eth_i.h"

#define ipa_eth_nd_op(eth_dev, op, args...) (eth_dev->nd->ops->op(args))

static LIST_HEAD(ipa_eth_net_drivers);
static DEFINE_MUTEX(ipa_eth_net_drivers_mutex);

static bool ipa_eth_net_check_ops(struct ipa_eth_net_ops *ops)
{
	/* Following call backs are optional:
	 *  - save_regs()
	 */
	return
		ops &&
		ops->open_device &&
		ops->close_device &&
		ops->request_channel &&
		ops->release_channel &&
		ops->enable_channel &&
		ops->disable_channel &&
		ops->request_event &&
		ops->release_event &&
		ops->enable_event &&
		ops->disable_event &&
		ops->moderate_event &&
		ops->receive_skb &&
		ops->transmit_skb;
}

static bool ipa_eth_net_check_driver(struct ipa_eth_net_driver *nd)
{
	return
		nd &&
		nd->bus &&
		nd->name &&
		nd->driver &&
		ipa_eth_net_check_ops(nd->ops);
}

static int __ipa_eth_net_register_driver(struct ipa_eth_net_driver *nd)
{
	int rc;

	rc = ipa_eth_bus_register_driver(nd);
	if (rc) {
		ipa_eth_err("Failed to register network driver %s", nd->name);
		return rc;
	}

	mutex_lock(&ipa_eth_net_drivers_mutex);
	list_add(&nd->driver_list, &ipa_eth_net_drivers);
	mutex_unlock(&ipa_eth_net_drivers_mutex);

	ipa_eth_log("Registered network driver %s", nd->name);

	return 0;
}

int ipa_eth_net_register_driver(struct ipa_eth_net_driver *nd)
{
	if (!ipa_eth_net_check_driver(nd)) {
		ipa_eth_err("Net driver validation failed");
		return -EINVAL;
	}

	return __ipa_eth_net_register_driver(nd);
}

void ipa_eth_net_unregister_driver(struct ipa_eth_net_driver *nd)
{
	mutex_lock(&ipa_eth_net_drivers_mutex);
	list_del(&nd->driver_list);
	mutex_unlock(&ipa_eth_net_drivers_mutex);

	ipa_eth_bus_unregister_driver(nd);
}

/* Event handler for netdevice events from upper interfaces */
static int ipa_eth_net_upper_event(struct notifier_block *nb,
	unsigned long event, void *ptr)
{
	int rc;
	struct net_device *net_dev = netdev_notifier_info_to_dev(ptr);
	struct ipa_eth_upper_device *upper_dev =
			container_of(nb,
				struct ipa_eth_upper_device, netdevice_nb);
	struct ipa_eth_device *eth_dev = upper_dev->eth_dev;

	if (net_dev != upper_dev->net_dev)
		return NOTIFY_DONE;

	ipa_eth_dev_log(eth_dev,
			"Received netdev event %s (0x%04lx) for %s",
			ipa_eth_net_device_event_name(event), event,
			net_dev->name);

	switch (event) {
	case NETDEV_UP:
		rc = ipa_eth_ep_register_upper_interface(upper_dev);
		if (rc)
			ipa_eth_dev_err(eth_dev, "Failed to register upper");
		break;
	case NETDEV_DOWN:
		rc = ipa_eth_ep_unregister_upper_interface(upper_dev);
		if (rc)
			ipa_eth_dev_err(eth_dev, "Failed to register upper");
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

static void __ipa_eth_upper_release(struct kref *ref)
{
	struct ipa_eth_upper_device *upper_dev =
		container_of(ref, struct ipa_eth_upper_device, refcount);

	list_del(&upper_dev->upper_list);
	kzfree(upper_dev);
}

static inline void kref_get_upper(struct ipa_eth_upper_device *upper_dev)
{
	kref_get(&upper_dev->refcount);
}

static inline int kref_put_upper(struct ipa_eth_upper_device *upper_dev)
{
	return kref_put(&upper_dev->refcount, __ipa_eth_upper_release);
}

static int ipa_eth_net_watch_upper_device(
		struct ipa_eth_upper_device *upper_dev)
{
	int rc;
	struct ipa_eth_device *eth_dev = upper_dev->eth_dev;

	if (upper_dev->watching)
		return 0;

	ipa_eth_dev_log(eth_dev,
			"Going to watch upper device %s",
			upper_dev->net_dev->name);

	rc = register_netdevice_notifier(&upper_dev->netdevice_nb);
	if (rc) {
		ipa_eth_dev_err(eth_dev,
			"Failed to register with netdevice notifier");
		return rc;
	}

	upper_dev->watching = true;

	kref_get_upper(upper_dev);

	return 0;
}

static int ipa_eth_net_unwatch_upper_device_unsafe(
		struct ipa_eth_upper_device *upper_dev)
{
	int rc;
	struct ipa_eth_device *eth_dev = upper_dev->eth_dev;

	if (!upper_dev->watching)
		return 0;

	rc = unregister_netdevice_notifier(&upper_dev->netdevice_nb);
	if (rc) {
		ipa_eth_dev_err(eth_dev,
			"Failed to unregister with netdevice notifier");
		return rc;
	}

	ipa_eth_dev_log(eth_dev, "Stopped watching upper device %s",
			upper_dev->net_dev->name);

	upper_dev->watching = false;

	/* kref_put_upper() unlinks upper_dev from upper_devices list before
	 * freeing it, causing this function unsafe to use during linked list
	 * iteration.
	 */
	kref_put_upper(upper_dev);

	return rc;
}

static int ipa_eth_net_unwatch_unlinked(struct ipa_eth_device *eth_dev)
{
	int rc = 0;
	struct ipa_eth_device_private *dev_priv = eth_dev->ipa_priv;
	struct ipa_eth_upper_device *upper_dev = NULL;
	struct ipa_eth_upper_device *tmp = NULL;

	mutex_lock(&dev_priv->upper_mutex);

	list_for_each_entry_safe(upper_dev, tmp,
					&dev_priv->upper_devices, upper_list) {
		if (upper_dev->linked)
			continue;

		rc |= ipa_eth_net_unwatch_upper_device_unsafe(upper_dev);
	}

	mutex_unlock(&dev_priv->upper_mutex);

	return rc;
}

int ipa_eth_net_watch_upper(struct ipa_eth_device *eth_dev)
{
	int rc = 0;
	struct ipa_eth_device_private *dev_priv = eth_dev->ipa_priv;
	struct ipa_eth_upper_device *upper_dev = NULL;

	/* We cannot acquire rtnl_mutex because we need to subsequently call
	 * register_netdevice_notifier.
	 */
	mutex_lock(&dev_priv->upper_mutex);

	list_for_each_entry(upper_dev, &dev_priv->upper_devices, upper_list) {
		if (!upper_dev->linked)
			continue;

		rc = ipa_eth_net_watch_upper_device(upper_dev);
		if (rc)
			break;
	}

	if (rc) {
		list_for_each_entry_continue_reverse(upper_dev,
				&dev_priv->upper_devices, upper_list) {
			/* Since we are unwatching only linked devices, they
			 * will not be removed from the linked list, so we
			 * do not need to use safe iteration for linked list.
			 */
			if (upper_dev->linked)
				ipa_eth_net_unwatch_upper_device_unsafe(
						upper_dev);
		}
	}

	mutex_unlock(&dev_priv->upper_mutex);

	if (ipa_eth_net_unwatch_unlinked(eth_dev)) {
		ipa_eth_dev_err(eth_dev,
			"Failed to unwatch one or more unliked upper devices");
	}

	return rc;
}

int ipa_eth_net_unwatch_upper(struct ipa_eth_device *eth_dev)
{
	int rc = 0;
	struct ipa_eth_device_private *dev_priv = eth_dev->ipa_priv;
	struct ipa_eth_upper_device *upper_dev = NULL;
	struct ipa_eth_upper_device *tmp = NULL;

	mutex_lock(&dev_priv->upper_mutex);

	list_for_each_entry_safe(upper_dev, tmp,
					&dev_priv->upper_devices, upper_list)
		rc |= ipa_eth_net_unwatch_upper_device_unsafe(upper_dev);

	if (rc)
		ipa_eth_dev_err(eth_dev,
			"Failed to unwatch one or more upper devices");

	mutex_unlock(&dev_priv->upper_mutex);

	return rc;
}

static int ipa_eth_net_link_upper(struct ipa_eth_device *eth_dev,
	struct net_device *upper_net_dev)
{
	int rc = 0;
	struct ipa_eth_upper_device *upper_dev;
	struct ipa_eth_device_private *dev_priv = eth_dev->ipa_priv;

	ipa_eth_dev_log(eth_dev,
		"Linking upper interface %s", upper_net_dev->name);

	upper_dev = kzalloc(sizeof(*upper_dev), GFP_KERNEL);
	if (!upper_dev)
		return -ENOMEM;

	kref_init(&upper_dev->refcount);

	upper_dev->linked = true;
	upper_dev->eth_dev = eth_dev;
	upper_dev->net_dev = upper_net_dev;
	upper_dev->netdevice_nb.notifier_call = ipa_eth_net_upper_event;

	mutex_lock(&dev_priv->upper_mutex);
	list_add(&upper_dev->upper_list, &dev_priv->upper_devices);
	mutex_unlock(&dev_priv->upper_mutex);

	/* We cannot call register_netdevice_notifier() from here since we
	 * are already holding rtnl_mutex. Schedule a device refresh for the
	 * offload sub-system workqueue to re-scan upper list and register for
	 * notifications.
	 */
	ipa_eth_device_refresh_sched(eth_dev);

	return rc;
}

static int ipa_eth_net_unlink_upper(struct ipa_eth_device *eth_dev,
	struct net_device *upper_net_dev)
{
	int rc = -ENODEV;
	struct ipa_eth_device_private *dev_priv = eth_dev->ipa_priv;
	struct ipa_eth_upper_device *upper_dev = NULL;

	ipa_eth_dev_log(eth_dev,
		"Unlinking upper interface %s", upper_net_dev->name);

	mutex_lock(&dev_priv->upper_mutex);

	list_for_each_entry(upper_dev, &dev_priv->upper_devices, upper_list) {
		if (upper_dev->net_dev == upper_net_dev) {
			upper_dev->linked = false;

			/* We can free upper_dev only if the refresh wq has
			 * already unregistered the netdevice notifier.
			 */
			kref_put_upper(upper_dev);

			rc = 0;
			break;
		}
	}

	mutex_unlock(&dev_priv->upper_mutex);

	ipa_eth_device_refresh_sched(eth_dev);

	return rc;
}

static bool ipa_eth_net_event_up(struct ipa_eth_device *eth_dev,
		unsigned long event, void *ptr)
{
	return !test_and_set_bit(IPA_ETH_IF_ST_UP, &eth_dev->if_state);
}

static bool ipa_eth_net_event_down(struct ipa_eth_device *eth_dev,
		unsigned long event, void *ptr)
{
	return test_and_clear_bit(IPA_ETH_IF_ST_UP, &eth_dev->if_state);
}

static bool ipa_eth_net_event_change(struct ipa_eth_device *eth_dev,
		unsigned long event, void *ptr)
{
	return netif_carrier_ok(eth_dev->net_dev) ?
		!test_and_set_bit(IPA_ETH_IF_ST_LOWER_UP, &eth_dev->if_state) :
		test_and_clear_bit(IPA_ETH_IF_ST_LOWER_UP, &eth_dev->if_state);

}

static bool ipa_eth_net_event_pre_change_upper(struct ipa_eth_device *eth_dev,
		unsigned long event, void *ptr)
{
	struct netdev_notifier_changeupper_info *upper_info = ptr;

	if (!upper_info->linking)
		ipa_eth_net_unlink_upper(eth_dev, upper_info->upper_dev);

	return false;
}

static bool ipa_eth_net_event_change_upper(struct ipa_eth_device *eth_dev,
		unsigned long event, void *ptr)
{
	struct netdev_notifier_changeupper_info *upper_info = ptr;

	if (upper_info->linking)
		ipa_eth_net_link_upper(eth_dev, upper_info->upper_dev);

	return false;
}

typedef bool (*ipa_eth_net_event_handler)(struct ipa_eth_device *eth_dev,
		unsigned long event, void *ptr);

/* Event handlers for netdevice events from real interface */
static ipa_eth_net_event_handler
		ipa_eth_net_event_handlers[IPA_ETH_NET_DEVICE_MAX_EVENTS] = {
	[NETDEV_UP] = ipa_eth_net_event_up,
	[NETDEV_DOWN] = ipa_eth_net_event_down,
	[NETDEV_CHANGE] = ipa_eth_net_event_change,
	[NETDEV_CHANGELOWERSTATE] = ipa_eth_net_event_change,
	[NETDEV_PRECHANGEUPPER] = ipa_eth_net_event_pre_change_upper,
	[NETDEV_CHANGEUPPER] = ipa_eth_net_event_change_upper,
};

static int ipa_eth_net_device_event(struct notifier_block *nb,
	unsigned long event, void *ptr)
{
	struct net_device *net_dev = netdev_notifier_info_to_dev(ptr);
	struct ipa_eth_device *eth_dev = container_of(nb,
				struct ipa_eth_device, netdevice_nb);

	if (net_dev != eth_dev->net_dev)
		return NOTIFY_DONE;

	ipa_eth_dev_log(eth_dev, "Received netdev event %s (0x%04lx)",
			ipa_eth_net_device_event_name(event), event);

	if (event < IPA_ETH_NET_DEVICE_MAX_EVENTS) {
		ipa_eth_net_event_handler handler =
					ipa_eth_net_event_handlers[event];
		bool refresh_needed = handler && handler(eth_dev, event, ptr);

		/* We can not wait for refresh to complete as we are holding
		 * the rtnl mutex.
		 */
		if (refresh_needed)
			ipa_eth_device_refresh_sched(eth_dev);
	}

	return NOTIFY_DONE;
}

int ipa_eth_net_open_device(struct ipa_eth_device *eth_dev)
{
	int rc;

	rc = ipa_eth_nd_op(eth_dev, open_device, eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to open net device");
		goto err_open;
	}

	if (!eth_dev->net_dev) {
		rc = -EFAULT;
		ipa_eth_dev_err(eth_dev,
			"Network driver failed to fill net_dev");
		goto err_net_dev;
	}

	eth_dev->netdevice_nb.notifier_call = ipa_eth_net_device_event;
	rc = register_netdevice_notifier(&eth_dev->netdevice_nb);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to register netdev notifier");
		goto err_register;
	}

	return 0;

err_register:
	eth_dev->netdevice_nb.notifier_call = NULL;
err_net_dev:
	ipa_eth_nd_op(eth_dev, close_device, eth_dev);
err_open:
	return rc;
}

void ipa_eth_net_close_device(struct ipa_eth_device *eth_dev)
{
	unregister_netdevice_notifier(&eth_dev->netdevice_nb);
	eth_dev->netdevice_nb.notifier_call = NULL;

	ipa_eth_nd_op(eth_dev, close_device, eth_dev);
}

static phys_addr_t ipa_eth_dma_pgaddr(struct ipa_eth_device *eth_dev,
	const void *vaddr)
{
	return page_to_phys(vmalloc_to_page(vaddr));
}

static phys_addr_t ipa_eth_dma_paddr(struct ipa_eth_device *eth_dev,
	const void *vaddr)
{
	return ipa_eth_dma_pgaddr(eth_dev, vaddr) |
			((phys_addr_t)vaddr & ~PAGE_MASK);
}

static int ipa_eth_dma_alloc(struct ipa_eth_device *eth_dev,
	size_t size, gfp_t gfp, struct ipa_eth_resource *mem)
{
	if (!eth_dev || !eth_dev->dev) {
		ipa_eth_dev_err(eth_dev, "eth_dev is invalid");
		return -EFAULT;
	}

	if (!mem) {
		ipa_eth_dev_err(eth_dev, "Missing mem parameter");
		return -EINVAL;
	}

	memset(mem, 0, sizeof(*mem));

	mem->vaddr = dma_alloc_coherent(eth_dev->dev, size, &mem->daddr, gfp);
	if (!mem->vaddr) {
		ipa_eth_dev_err(eth_dev,
			"Failed to allocate memory of size %zu", size);
		return -ENOMEM;
	}

	mem->size = size;
	mem->paddr = ipa_eth_dma_paddr(eth_dev, mem->vaddr);

	ipa_eth_dev_log(eth_dev,
		"Allocated memory of size %zu at [%pK,%pad,%pap]",
		mem->size, mem->vaddr, &mem->daddr, &mem->paddr);

	return 0;
}

static void ipa_eth_dma_free(struct ipa_eth_device *eth_dev,
	struct ipa_eth_resource *mem)
{
	dma_free_coherent(eth_dev->dev, mem->size, mem->vaddr, mem->daddr);
}

static size_t ipa_eth_dma_walk(struct ipa_eth_device *eth_dev,
	const struct ipa_eth_resource *mem,
	ipa_eth_mem_it_t it, void *arg)
{
	struct ipa_eth_resource cmem = {
		.size =  PAGE_SIZE,
		.vaddr = (void *) rounddown((unsigned long) mem->vaddr,
						PAGE_SIZE),
		.daddr = rounddown(mem->daddr, PAGE_SIZE),
	};

	if ((mem->daddr - cmem.daddr) != (mem->vaddr - cmem.vaddr)) {
		ipa_eth_dev_err(eth_dev,
			"Alignment mismatch between daddr and addr");
		return 0;
	}

	if (cmem.daddr != mem->daddr)
		ipa_eth_dev_dbg(eth_dev, "Daddr %pad is realigned to %pad",
			mem->daddr, &cmem.daddr);

	if (cmem.vaddr != mem->vaddr)
		ipa_eth_dev_dbg(eth_dev, "Vaddr %pK is realigned to %pK",
			mem->vaddr, cmem.vaddr);

	while (cmem.vaddr < (mem->vaddr + mem->size)) {
		cmem.paddr = ipa_eth_dma_paddr(eth_dev, cmem.vaddr);

		if (it(eth_dev, &cmem, arg)) {
			ipa_eth_dev_err(eth_dev,
				"Remap failed for page at [%pK,%pad,%pap]",
				cmem.vaddr, &cmem.daddr, &cmem.paddr);
			break;
		}

		cmem.vaddr += PAGE_SIZE;
		cmem.daddr += PAGE_SIZE;
	}

	return clamp_val(cmem.vaddr, mem->vaddr, mem->vaddr + mem->size) -
			mem->vaddr;
}

static struct ipa_eth_dma_allocator default_dma_allocator = {
	.name = "ipa_eth_dma_alloc_coherent",
	.paddr = ipa_eth_dma_paddr,
	.alloc = ipa_eth_dma_alloc,
	.free = ipa_eth_dma_free,
	.walk = ipa_eth_dma_walk,
};

static int ipa_eth_net_process_skb(struct ipa_eth_channel *ch,
	struct sk_buff *skb)
{
	if (likely(IPA_ETH_CH_IS_RX(ch)))
		return ipa_eth_net_receive_skb(ch->eth_dev, skb);
	else
		return ipa_eth_net_transmit_skb(ch->eth_dev, skb);
}

static enum ipa_smmu_cb_type ipa_eth_hw_to_cb_type(
	struct ipa_eth_channel *ch,
	enum ipa_eth_hw_type hw_type)
{
	enum ipa_smmu_cb_type cb_type = IPA_SMMU_CB_MAX;

	switch (hw_type) {
	case IPA_ETH_HW_UC:
		cb_type = IPA_SMMU_CB_UC;
		break;

	case IPA_ETH_HW_GSI:
	case IPA_ETH_HW_IPA: /* All current ethernet client EPs uses GSI CB */
		cb_type = IPA_SMMU_CB_AP;
		break;

	default:
		ipa_eth_bug("Unknown Eth CB type %d", hw_type);
		break;
	}

	return cb_type;
}

static struct ipa_smmu_cb_ctx *ipa_eth_get_smmu_ctx(
		enum ipa_smmu_cb_type cb_type)
{
	struct ipa_smmu_cb_ctx *cb;

	if (cb_type >= IPA_SMMU_CB_MAX)
		return NULL;

	cb = ipa3_get_smmu_ctx(cb_type);

	if (!cb || !cb->valid)
		return NULL;

	return cb;
}

static int ipa_eth_hw_to_cb_map_one(struct ipa_eth_channel *ch,
	enum ipa_eth_hw_type hw_type,
	struct ipa_eth_hw_map_param *hw_map_param,
	enum ipa_smmu_cb_type cb_type,
	struct ipa_eth_cb_map_param *cb_map_param)
{
	struct ipa_smmu_cb_ctx *cb_ctx = ipa_eth_get_smmu_ctx(cb_type);

	if (!cb_ctx) {
		ipa_eth_dev_err(ch->eth_dev,
			"Failed to get smmu ctx for hw type %d",
			hw_type);
		return -EFAULT;
	}

	/* One CB can be shared between multiple HW. Make sure the parameters
	 * for CB are aggregated to a common denominator.
	 */
	cb_map_param->map = cb_map_param->map || hw_map_param->map;
	cb_map_param->sym = cb_map_param->sym || hw_map_param->sym;
	cb_map_param->iommu_prot |= hw_map_param->read ? IOMMU_READ : 0;
	cb_map_param->iommu_prot |= hw_map_param->write ? IOMMU_WRITE : 0;

	switch (cb_map_param->iommu_prot) {
	case IOMMU_READ:
		cb_map_param->dma_dir = DMA_TO_DEVICE;
		break;
	case IOMMU_WRITE:
		cb_map_param->dma_dir = DMA_FROM_DEVICE;
		break;
	case 0:
		cb_map_param->dma_dir = DMA_NONE;
		break;
	default:
		cb_map_param->dma_dir = DMA_BIDIRECTIONAL;
		break;
	}

	cb_map_param->cb_ctx = cb_ctx;

	return 0;
}

/**
 * ipa_eth_hw_to_cb_map() - Translate memory mapping parameters from IPA
 *                          hardware types to context bank types
 * @ch: Channel to which the parameters belong
 * @hw_map_params: Mapping parameters for various hardware types
 * @cb_map_params: Translated mapping parameters output for context banks
 *
 * Return: 0 if successful, non-zero otherwise
 */
static int ipa_eth_hw_to_cb_map(struct ipa_eth_channel *ch,
		struct ipa_eth_hw_map_param *hw_map_params,
		struct ipa_eth_cb_map_param *cb_map_params)
{
	int rc;
	enum ipa_eth_hw_type hw_type;

	for (hw_type = 0; hw_type < IPA_ETH_HW_MAX; hw_type++) {
		enum ipa_smmu_cb_type cb_type =
					ipa_eth_hw_to_cb_type(ch, hw_type);

		rc = ipa_eth_hw_to_cb_map_one(ch,
				hw_type, &hw_map_params[hw_type],
				cb_type, &cb_map_params[cb_type]);
		if (rc) {
			ipa_eth_dev_err(ch->eth_dev,
				"Failed to convert from hw to cb map params");
			return rc;
		}
	}

	return 0;
}

static int ipa_eth_cb_mapper_sym(struct ipa_eth_device *eth_dev,
		const struct ipa_eth_resource *cmem, void *arg)
{
	int rc;
	struct ipa_eth_cb_map_param *cb_map_param = arg;
	const struct ipa_smmu_cb_ctx *cb_ctx = cb_map_param->cb_ctx;

	if (!cb_map_param->map) {
		ipa_eth_dev_bug(eth_dev, "CB map is not enabled");
		return -EFAULT;
	}

	if (!cmem->size) {
		ipa_eth_dev_bug(eth_dev, "Requested CB mapping of size 0");
		return -EINVAL;
	}

	rc = ipa3_iommu_map(cb_ctx->mapping->domain,
		cmem->daddr, cmem->paddr, cmem->size, cb_map_param->iommu_prot);
	if (rc) {
		ipa_eth_dev_err(eth_dev,
			"Failed to map %zu bytes into CB %s at [%pK,%pad,%pap]",
			cmem->size, cb_ctx->iommu->name,
			cmem->vaddr, &cmem->daddr, &cmem->paddr);
	}

	return rc;
}

static int ipa_eth_cb_unmapper_sym(struct ipa_eth_device *eth_dev,
		const struct ipa_eth_resource *cmem, void *arg)
{
	size_t size;
	struct ipa_eth_cb_map_param *cb_map_param = arg;
	const struct ipa_smmu_cb_ctx *cb_ctx = cb_map_param->cb_ctx;

	if (!cmem->size) {
		ipa_eth_dev_err(eth_dev, "Requested CB unmapping of size 0");
		return -EINVAL;
	}

	size = iommu_unmap(cb_ctx->mapping->domain, cmem->daddr, cmem->size);
	if (size != cmem->size) {
		ipa_eth_dev_err(eth_dev,
			"Failed to unmap %zu bytes in domain %s at daddr %pad",
			cmem->size, dev_name(cb_ctx->dev), &cmem->daddr);
		return -EFAULT;
	}

	return 0;
}

static size_t ipa_eth_net_remap(struct ipa_eth_device *eth_dev,
	const struct ipa_eth_resource *mem,
	struct ipa_eth_dma_allocator *allocator,
	ipa_eth_mem_it_t map_op,
	struct ipa_eth_cb_map_param *cb_map_param)
{
	return allocator->walk(eth_dev, mem, map_op, cb_map_param);
}

/**
 * ipa_eth_net_cb_map_sym() - Symmetrically map a channel memory to a given IPA
 *                            SMMU context bank
 * @eth_dev: Device to which the channel memory belong
 * @ch_mem: Channel memory that need to be mapped
 * @allocator: Allocator used for allocating the channel memory
 * @cb_type: IPA SMMU context bank to which to map the memory
 * @cb_map_param: Parameters for mapping memory to the given context bank
 *
 * Symmetric mapping creates the same DADDR->PADDR memory mapping in the
 * given IPA context bank as in the original channel memory @ch_mem. Use this
 * API if any of the IPA hardware need to use the same IO virtual address as
 * the network device.
 *
 * Return: 0 on successful mapping to the given @cb_type, non-zero otherwise.
 */
static int ipa_eth_net_cb_map_sym(struct ipa_eth_device *eth_dev,
	struct ipa_eth_channel_mem *ch_mem,
	struct ipa_eth_dma_allocator *allocator,
	enum ipa_smmu_cb_type cb_type,
	struct ipa_eth_cb_map_param *cb_map_param)
{
	size_t size;
	struct ipa_eth_resource *cb_mem = &ch_mem->cb_mem[cb_type];
	const struct ipa_smmu_cb_ctx *cb_ctx = cb_map_param->cb_ctx;

	if (!ch_mem->mem.daddr) {
		ipa_eth_dev_err(eth_dev,
			"Symmetric mapping requires a valid DMA address");
		return -EFAULT;
	}

	ipa_eth_dev_log(eth_dev,
		"Mapping %zu bytes into domain %s at [%pK,%pad,%pap]",
		ch_mem->mem.size, dev_name(cb_ctx->dev),
		ch_mem->mem.vaddr, &ch_mem->mem.daddr, &ch_mem->mem.paddr);

	size = ipa_eth_net_remap(eth_dev, &ch_mem->mem, allocator,
					ipa_eth_cb_mapper_sym, cb_map_param);
	if (size != ch_mem->mem.size) {
		/* Unmap any partially mapped memory */
		struct ipa_eth_resource unmap_mem = {
			.size = size,
			.vaddr = ch_mem->mem.vaddr,
			.daddr = ch_mem->mem.daddr,
			.paddr = ch_mem->mem.paddr,
		};

		ipa_eth_dev_err(eth_dev,
			"Failed to map %zu bytes to domain %s, undo mapping",
			ch_mem->mem.size - size, dev_name(cb_ctx->dev));

		(void) ipa_eth_net_remap(eth_dev, &unmap_mem, allocator,
					ipa_eth_cb_unmapper_sym, cb_map_param);

		return -EFAULT;
	}

	*cb_mem = ch_mem->mem;

	ipa_eth_dev_log(eth_dev,
		"Mapped %zu bytes into domain %s at [%pK,%pad,%pap]",
		cb_mem->size, dev_name(cb_ctx->dev),
		cb_mem->vaddr, &cb_mem->daddr, &cb_mem->paddr);

	return 0;
}

/**
 * ipa_eth_net_cb_map_asym() - Map a channel memory to a context bank, not
 *                             necessarily using the same IO virtual address
 * @eth_dev: Device to which the channel memory belong
 * @ch_mem: Channel memory that need to be mapped
 * @allocator: Allocator used for allocating the channel memory
 * @cb_type: IPA SMMU context bank to which to map the memory
 * @cb_map_param: Parameters for mapping memory to the given context bank
 *
 * Use this API if symmetric mapping is not required for a channel memory. See
 * ipa_eth_net_cb_map_sym() for more details on symmetric mapping.
 *
 * Note that since assymmetric mapping uses dma_map_single(), it can only be
 * used to map memory that was allocated using kmalloc(). Memory allocated from
 * vmalloc region (like dma_alloc_coherent() on ARM targets) can not be mapped
 * again using this API.
 *
 * Return: 0 on success, non-zero otherwise
 */
static int ipa_eth_net_cb_map_asym(struct ipa_eth_device *eth_dev,
	struct ipa_eth_channel_mem *ch_mem,
	struct ipa_eth_dma_allocator *allocator,
	enum ipa_smmu_cb_type cb_type,
	struct ipa_eth_cb_map_param *cb_map_param)
{
	struct ipa_eth_resource *cb_mem = &ch_mem->cb_mem[cb_type];
	const struct ipa_smmu_cb_ctx *cb_ctx = cb_map_param->cb_ctx;

	if (is_vmalloc_addr(ch_mem->mem.vaddr)) {
		ipa_eth_dev_err(eth_dev,
			"Asymmetric mapping cannot use vmalloc address");
		return -EFAULT;
	}

	ipa_eth_dev_log(eth_dev,
		"Mapping %zu bytes into device %s from [%pK,%pad,%pap]",
		ch_mem->mem.size, dev_name(cb_ctx->dev),
		ch_mem->mem.vaddr, &ch_mem->mem.daddr, &ch_mem->mem.paddr);

	*cb_mem = ch_mem->mem;

	cb_mem->daddr = dma_map_single(cb_ctx->dev,
				ch_mem->mem.vaddr, ch_mem->mem.size,
				cb_map_param->dma_dir);
	if (dma_mapping_error(cb_ctx->dev, cb_mem->daddr)) {
		cb_mem->size = 0;
		ipa_eth_dev_err(eth_dev, "Failed to map buffer to device %s",
			dev_name(cb_ctx->dev));
		return -EFAULT;
	}

	ipa_eth_dev_log(eth_dev,
		"Mapped %zu bytes into device %s at [%pK,%pad,%pap]",
		cb_mem->size, dev_name(cb_ctx->dev),
		cb_mem->vaddr, &cb_mem->daddr, &cb_mem->paddr);

	return 0;
}

static int ipa_eth_net_cb_map_one(struct ipa_eth_device *eth_dev,
	struct ipa_eth_channel_mem *ch_mem,
	struct ipa_eth_dma_allocator *allocator,
	enum ipa_smmu_cb_type cb_type,
	struct ipa_eth_cb_map_param *cb_map_param)
{
	if (cb_map_param->sym)
		return ipa_eth_net_cb_map_sym(
			eth_dev, ch_mem, allocator, cb_type, cb_map_param);
	else
		return ipa_eth_net_cb_map_asym(
			eth_dev, ch_mem, allocator, cb_type, cb_map_param);
}

static void ipa_eth_net_cb_unmap_sym(struct ipa_eth_device *eth_dev,
	struct ipa_eth_channel_mem *ch_mem,
	struct ipa_eth_dma_allocator *allocator,
	enum ipa_smmu_cb_type cb_type,
	struct ipa_eth_cb_map_param *cb_map_param)
{
	struct ipa_eth_resource *cb_mem = &ch_mem->cb_mem[cb_type];
	const struct ipa_smmu_cb_ctx *cb_ctx = cb_map_param->cb_ctx;

	if (!cb_mem->size)
		return;

	ipa_eth_dev_log(eth_dev,
		"Unmapping %zu bytes in domain %s from daddr %pad",
		cb_mem->size, dev_name(cb_ctx->dev), &cb_mem->daddr);

	(void) ipa_eth_net_remap(eth_dev, &ch_mem->mem, allocator,
					ipa_eth_cb_unmapper_sym, cb_map_param);

	ipa_eth_dev_log(eth_dev,
		"Unmapped %zu bytes in domain %s from daddr %pad",
		cb_mem->size, dev_name(cb_ctx->dev), &cb_mem->daddr);

	cb_mem->size = 0;
}

static void ipa_eth_net_cb_unmap_asym(struct ipa_eth_device *eth_dev,
	struct ipa_eth_channel_mem *ch_mem,
	struct ipa_eth_dma_allocator *allocator,
	enum ipa_smmu_cb_type cb_type,
	struct ipa_eth_cb_map_param *cb_map_param)
{
	struct ipa_eth_resource *cb_mem = &ch_mem->cb_mem[cb_type];
	const struct ipa_smmu_cb_ctx *cb_ctx = cb_map_param->cb_ctx;

	if (!cb_mem->size)
		return;

	ipa_eth_dev_log(eth_dev,
		"Unmapping %zu bytes in device %s from daddr %pad",
		cb_mem->size, dev_name(cb_ctx->dev), &cb_mem->daddr);

	dma_unmap_single(cb_ctx->dev, cb_mem->daddr, cb_mem->size,
			cb_map_param->dma_dir);

	ipa_eth_dev_log(eth_dev,
		"Unmapped %zu bytes in device %s from daddr %pad",
		cb_mem->size, dev_name(cb_ctx->dev), &cb_mem->daddr);

	cb_mem->size = 0;
}

static void ipa_eth_net_cb_unmap_one(struct ipa_eth_device *eth_dev,
	struct ipa_eth_channel_mem *ch_mem,
	struct ipa_eth_dma_allocator *allocator,
	enum ipa_smmu_cb_type cb_type,
	struct ipa_eth_cb_map_param *cb_map_param)
{
	if (cb_map_param->sym)
		return ipa_eth_net_cb_unmap_sym(
			eth_dev, ch_mem, allocator, cb_type, cb_map_param);
	else
		return ipa_eth_net_cb_unmap_asym(
			eth_dev, ch_mem, allocator, cb_type, cb_map_param);
}

/**
 * ipa_eth_net_cb_unmap_ch_mem() - Unmap one channel memory from one of more
 *                                 IPA context banks
 * @eth_dev: Device to which the channel memory belong
 * @ch_mem: Channel memory that need to be unmapped
 * @allocator: Allocator used for allocating the channel memory
 * @cb_map_params: Mapping parameters for all known IPA context banks
 *
 * Use this API to unmap a channel memory from various IPA context banks that
 * was previously mapped using ipa_eth_net_cb_map_ch_mem().
 */
static void ipa_eth_net_cb_unmap_ch_mem(struct ipa_eth_device *eth_dev,
	struct ipa_eth_channel_mem *ch_mem,
	struct ipa_eth_dma_allocator *allocator,
	struct ipa_eth_cb_map_param *cb_map_params)
{
	enum ipa_smmu_cb_type cb_type;

	if (!ch_mem->cb_mem)
		return;

	for (cb_type = 0; cb_type < IPA_SMMU_CB_MAX; cb_type++) {
		if (!cb_map_params[cb_type].map)
			continue;

		ipa_eth_net_cb_unmap_one(eth_dev, ch_mem, allocator,
					cb_type, &cb_map_params[cb_type]);
	}

	kfree(ch_mem->cb_mem);
	ch_mem->cb_mem = NULL;
}

/**
 * ipa_eth_net_cb_map_ch_mem() - Map one channel memory to one or more IPA
 *                               context banks
 * @eth_dev: Device to which the channel memory belong
 * @ch_mem: Channel memory that need to be mapped
 * @allocator: Allocator used for allocating the channel memory
 * @cb_map_params: Mapping parameters for all known IPA context banks
 *
 * Return: 0 on success, non-zero otherwise
 */
static int ipa_eth_net_cb_map_ch_mem(struct ipa_eth_device *eth_dev,
	struct ipa_eth_channel_mem *ch_mem,
	struct ipa_eth_dma_allocator *allocator,
	struct ipa_eth_cb_map_param *cb_map_params)
{
	int rc;
	enum ipa_smmu_cb_type cb_type;

	if (ch_mem->cb_mem) {
		ipa_eth_dev_err(eth_dev, "CB mem is already initialized");
		return -EEXIST;
	}

	ch_mem->cb_mem = kzalloc(sizeof(*ch_mem->cb_mem) * IPA_SMMU_CB_MAX,
				GFP_KERNEL);
	if (!ch_mem->cb_mem) {
		ipa_eth_dev_err(eth_dev, "Failed to alloc CB mem resource");
		return -ENOMEM;
	}

	for (cb_type = 0; cb_type < IPA_SMMU_CB_MAX; cb_type++) {
		if (!cb_map_params[cb_type].map)
			continue;

		rc = ipa_eth_net_cb_map_one(eth_dev, ch_mem, allocator,
					cb_type, &cb_map_params[cb_type]);
		if (rc)
			goto err_map;
	}

	return 0;

err_map:
	ipa_eth_net_cb_unmap_ch_mem(eth_dev, ch_mem, allocator, cb_map_params);
	return -ENOMEM;
}

/**
 * ipa_eth_net_hw_unmap_desc_mem() - Unmap all descriptor memory from various
 *                                   IPA hardware types
 * @eth_dev: Device to which the channel belong
 * @ch: Channel whose descriptor memory need to be unmapped
 *
 * Use this API to unmap any descriptor memory previously mapped to any of the
 * IPA hardware types.
 */
static void ipa_eth_net_hw_unmap_desc_mem(struct ipa_eth_device *eth_dev,
	struct ipa_eth_channel *ch)
{
	int rc;
	struct ipa_eth_channel_mem *ch_mem;
	struct ipa_eth_cb_map_param cb_map_params[IPA_SMMU_CB_MAX];

	memset(cb_map_params, 0, sizeof(cb_map_params));

	rc = ipa_eth_hw_to_cb_map(ch,
		ch->mem_params.desc.hw_map_params, cb_map_params);
	if (rc) {
		ipa_eth_dev_err(eth_dev,
			"Failed to convert map params from hw to cb");
		return;
	}

	list_for_each_entry(ch_mem, &ch->desc_mem, mem_list_entry) {
		ipa_eth_net_cb_unmap_ch_mem(eth_dev, ch_mem,
			ch->mem_params.desc.allocator, cb_map_params);
	}
}

/**
 * ipa_eth_net_hw_map_desc_mem() - Map all descriptor memory to various IPA
 *                                 hadware types
 * @eth_dev: Device to which the channel belong
 * @ch: Channel from which the descriptor memory need to be mapped
 *
 * The API uses hardware mapping parameters listed in hw_map_params[] of struct
 * ipa_eth_desc_params to determine how the mapping need to be perfomed. The
 * actual SMMU context banks used by each hardware type is determined by using
 * the ipa_eth_hw_to_cb_map() API.
 *
 * Return: 0 on success, non-zero otherwise
 */
static int ipa_eth_net_hw_map_desc_mem(struct ipa_eth_device *eth_dev,
	struct ipa_eth_channel *ch)
{
	int rc;
	struct ipa_eth_channel_mem *ch_mem;
	struct ipa_eth_cb_map_param cb_map_params[IPA_SMMU_CB_MAX];

	memset(cb_map_params, 0, sizeof(cb_map_params));

	rc = ipa_eth_hw_to_cb_map(ch,
		ch->mem_params.desc.hw_map_params, cb_map_params);
	if (rc) {
		ipa_eth_dev_err(eth_dev,
			"Failed to convert map params from hw to cb");
		return rc;
	}

	list_for_each_entry(ch_mem, &ch->desc_mem, mem_list_entry) {
		rc = ipa_eth_net_cb_map_ch_mem(eth_dev, ch_mem,
			ch->mem_params.desc.allocator, cb_map_params);
		if (rc)
			goto err_map;
	}

	return 0;

err_map:
	ipa_eth_net_hw_unmap_desc_mem(eth_dev, ch);
	return rc;
}

/**
 * ipa_eth_net_hw_unmap_buff_mem() - Unmap all buffer memory from various IPA
 *                                   hardware types
 * @eth_dev: Device to which the channel belong
 * @ch: Channel whose buffer memory need to be unmapped
 *
 * Use this API to unmap any buffer memory previously mapped to any of the IPA
 * hardware types.
 */
static void ipa_eth_net_hw_unmap_buff_mem(struct ipa_eth_device *eth_dev,
	struct ipa_eth_channel *ch)
{
	int rc;
	struct ipa_eth_channel_mem *ch_mem;
	struct ipa_eth_cb_map_param cb_map_params[IPA_SMMU_CB_MAX];

	memset(cb_map_params, 0, sizeof(cb_map_params));

	rc = ipa_eth_hw_to_cb_map(ch,
		ch->mem_params.buff.hw_map_params, cb_map_params);
	if (rc) {
		ipa_eth_dev_err(eth_dev,
			"Failed to convert map params from hw to cb");
		return;
	}

	list_for_each_entry(ch_mem, &ch->buff_mem, mem_list_entry) {
		ipa_eth_net_cb_unmap_ch_mem(eth_dev, ch_mem,
			ch->mem_params.buff.allocator, cb_map_params);
	}
}

/**
 * ipa_eth_net_hw_map_buff_mem() - Map all buffer memory to various IPA hadware
 *                                 types
 * @eth_dev: Device to which the channel belong
 * @ch: Channel from which the buffer memory need to be mapped
 *
 * The API uses hardware mapping parameters listed in hw_map_params[] of struct
 * ipa_eth_buff_params to determine how the mapping need to be perfomed. The
 * actual SMMU context banks used by each hardware type is determined by using
 * the ipa_eth_hw_to_cb_map() API.
 *
 * Return: 0 on success, non-zero otherwise
 */
static int ipa_eth_net_hw_map_buff_mem(struct ipa_eth_device *eth_dev,
	struct ipa_eth_channel *ch)
{
	int rc;
	struct ipa_eth_channel_mem *ch_mem;
	struct ipa_eth_cb_map_param cb_map_params[IPA_SMMU_CB_MAX];

	memset(cb_map_params, 0, sizeof(cb_map_params));

	rc = ipa_eth_hw_to_cb_map(ch,
		ch->mem_params.buff.hw_map_params, cb_map_params);
	if (rc) {
		ipa_eth_dev_err(eth_dev,
			"Failed to convert map params from hw to cb");
		return rc;
	}

	list_for_each_entry(ch_mem, &ch->buff_mem, mem_list_entry) {
		rc = ipa_eth_net_cb_map_ch_mem(eth_dev, ch_mem,
			ch->mem_params.buff.allocator, cb_map_params);
		if (rc)
			goto err_map;
	}

	return 0;

err_map:
	ipa_eth_net_hw_unmap_buff_mem(eth_dev, ch);
	return rc;
}

/**
 * ipa_eth_net_hw_map_channel() - Map all the channel memory to various IPA
 *                                hardware types
 * @eth_dev: Device to which the channel belong
 * @ch: Channel whose memory need to be mapped
 *
 * Return: 0 if successful, non-zero otherwise
 */
static int ipa_eth_net_hw_map_channel(struct ipa_eth_device *eth_dev,
	struct ipa_eth_channel *ch)
{
	int rc;

	rc = ipa_eth_net_hw_map_desc_mem(eth_dev, ch);
	if (rc)
		return rc;

	rc = ipa_eth_net_hw_map_buff_mem(eth_dev, ch);
	if (rc) {
		ipa_eth_net_hw_unmap_desc_mem(eth_dev, ch);
		return rc;
	}

	return 0;
}

/**
 * ipa_eth_net_cb_unmap_channel() - Unmap channel descriptor and buffer memory
 *                                  from IPA CBs
 * @eth_dev: Ethernet device
 * @ch: Ethernet device channel
 */
static void ipa_eth_net_hw_unmap_channel(struct ipa_eth_device *eth_dev,
	struct ipa_eth_channel *ch)
{
	ipa_eth_net_hw_unmap_desc_mem(eth_dev, ch);
	ipa_eth_net_hw_unmap_buff_mem(eth_dev, ch);
}

/**
 * ipa_eth_net_alloc_channel() - Allocate and initialize ipa_eth_channel
 * @eth_dev: Ethernet device
 * @dir: Channel direction
 * @events: Supported events
 * @featues: Supported features
 * @mem_params: Channel memory allocation parameters
 *
 * This API is expected to be called by network driver .request_channel()
 * callback implementation.
 *
 * Return: Pointer to the allocated ipa_eth_channel, NULL if the allocation
 *         fails
 */
struct ipa_eth_channel *ipa_eth_net_alloc_channel(
	struct ipa_eth_device *eth_dev, enum ipa_eth_channel_dir dir,
	unsigned long events, unsigned long features,
	const struct ipa_eth_channel_mem_params *mem_params)
{
	struct ipa_eth_channel *channel;

	if (!mem_params) {
		ipa_eth_dev_err(eth_dev, "Missing channel mem params");
		return NULL;
	}

	channel = kzalloc(sizeof(*channel), GFP_KERNEL);
	if (!channel)
		return NULL;

	channel->eth_dev = eth_dev;
	channel->direction = dir;
	channel->events = events;
	channel->features = features;

	channel->mem_params = *mem_params;

	INIT_LIST_HEAD(&channel->desc_mem);
	INIT_LIST_HEAD(&channel->buff_mem);

	return channel;
}
EXPORT_SYMBOL(ipa_eth_net_alloc_channel);

/**
 * ipa_eth_net_free_channel() - Deallocate an ipa_eth_channel previously
 *                              allocated by ipa_eth_net_alloc_channel()
 * @channel: Channel to be deallocated
 */
void ipa_eth_net_free_channel(struct ipa_eth_channel *channel)
{
	struct ipa_eth_device *eth_dev = channel->eth_dev;

	if (!list_empty(&channel->desc_mem))
		ipa_eth_dev_bug(eth_dev, "Descriptor memory still in use");

	if (!list_empty(&channel->desc_mem))
		ipa_eth_dev_bug(eth_dev, "Buffer memory still in use");

	kzfree(channel);
}
EXPORT_SYMBOL(ipa_eth_net_free_channel);


/**
 * ipa_eth_net_request_channel() - Request a channel from network device to be
 *                                 used for a specific end-point
 * @eth_dev: Ethernet device
 * @ipa_client: IPA EP client enum that also determines the channel direction
 * @events: Refer documentation of ipa_eth_net_ops.request_channel()
 * @features: Refer documentation of ipa_eth_net_ops.request_channel()
 * @mem_params: Refer documentation of ipa_eth_net_ops.request_channel()
 *
 * Offload drivers should use this API in order to invoke the network driver API
 * ipa_eth_net_ops.request_channel(). The function also initializes EP context
 * and maps channel memory to various IPA SMMU context banks.
 *
 * Return: Allocated channel if the allocation succeeds. NULL otherwise.
 */
struct ipa_eth_channel *ipa_eth_net_request_channel(
	struct ipa_eth_device *eth_dev, enum ipa_client_type ipa_client,
	unsigned long events, unsigned long features,
	const struct ipa_eth_channel_mem_params *mem_params)
{
	int rc;
	bool vlan_mode;
	int ipa_ep_num;
	struct ipa_eth_channel *ch;
	enum ipa_eth_channel_dir dir;
	struct ipa_eth_channel_mem_params params;
	struct ipa3_ep_context *ep_ctx = NULL;

	if (!mem_params) {
		ipa_eth_dev_err(eth_dev, "Missing channel mem params");
		return NULL;
	}

	params = *mem_params;

	if (!params.desc.allocator)
		params.desc.allocator = &default_dma_allocator;

	if (!params.buff.allocator)
		params.buff.allocator = &default_dma_allocator;

	dir = IPA_CLIENT_IS_PROD(ipa_client) ? IPA_ETH_DIR_RX : IPA_ETH_DIR_TX;

	ipa_ep_num = ipa_get_ep_mapping(ipa_client);
	if (ipa_ep_num == IPA_EP_NOT_ALLOCATED) {
		ipa_eth_dev_err(eth_dev,
			"Could not determine EP number for client %d",
			ipa_client);
		return NULL;
	}

	ep_ctx = &ipa3_ctx->ep[ipa_ep_num];
	if (ep_ctx->valid) {
		ipa_eth_dev_err(eth_dev,
				"EP context is already initialiazed");
		return NULL;
	}

	rc = ipa3_is_vlan_mode(IPA_VLAN_IF_ETH, &vlan_mode);
	if (rc) {
		ipa_eth_dev_err(eth_dev,
				"Could not determine IPA VLAN mode");
		return NULL;
	}

	ch = ipa_eth_nd_op(eth_dev, request_channel,
			eth_dev, dir, events, features, &params);
	if (IS_ERR_OR_NULL(ch)) {
		ipa_eth_dev_err(eth_dev,
			"Failed to request channel from net driver %s",
			eth_dev->nd->name);
		return ch;
	}

	ch->ipa_ep_num = ipa_ep_num;
	ch->ipa_client = ipa_client;
	ch->process_skb = ipa_eth_net_process_skb;
	ch->eth_dev = eth_dev;

	if (ipa_eth_net_hw_map_channel(eth_dev, ch)) {
		ipa_eth_dev_err(eth_dev,
			"Failed to map channel memory to IPA CBs");
		ipa_eth_nd_op(eth_dev, release_channel, ch);
		return NULL;
	}

	ipa_eth_ep_init_ctx(ch, vlan_mode);

	if (dir == IPA_ETH_CH_DIR_RX)
		list_add(&ch->channel_list, &eth_dev->rx_channels);
	else
		list_add(&ch->channel_list, &eth_dev->tx_channels);

	return ch;
}
EXPORT_SYMBOL(ipa_eth_net_request_channel);

/**
 * ipa_eth_net_release_channel() - Releases a channel presiously allocated using
 *                                 ipa_eth_net_request_channel()
 * @ch: Channel to be released
 */
void ipa_eth_net_release_channel(struct ipa_eth_channel *ch)
{
	list_del(&ch->channel_list);
	ipa_eth_ep_deinit_ctx(ch);
	ipa_eth_net_hw_unmap_channel(ch->eth_dev, ch);
	return ipa_eth_nd_op(ch->eth_dev, release_channel, ch);
}
EXPORT_SYMBOL(ipa_eth_net_release_channel);

int ipa_eth_net_enable_channel(struct ipa_eth_channel *ch)
{
	return ipa_eth_nd_op(ch->eth_dev, enable_channel, ch);
}
EXPORT_SYMBOL(ipa_eth_net_enable_channel);

int ipa_eth_net_disable_channel(struct ipa_eth_channel *ch)
{
	return ipa_eth_nd_op(ch->eth_dev, disable_channel, ch);
}
EXPORT_SYMBOL(ipa_eth_net_disable_channel);

int ipa_eth_net_request_event(struct ipa_eth_channel *ch, unsigned long event,
	phys_addr_t addr, u64 data)
{
	return ipa_eth_nd_op(ch->eth_dev, request_event, ch, event, addr, data);
}
EXPORT_SYMBOL(ipa_eth_net_request_event);

void ipa_eth_net_release_event(struct ipa_eth_channel *ch, unsigned long event)
{
	return ipa_eth_nd_op(ch->eth_dev, release_event, ch, event);
}
EXPORT_SYMBOL(ipa_eth_net_release_event);

int ipa_eth_net_enable_event(struct ipa_eth_channel *ch, unsigned long event)
{
	return ipa_eth_nd_op(ch->eth_dev, enable_event, ch, event);
}
EXPORT_SYMBOL(ipa_eth_net_enable_event);

int ipa_eth_net_disable_event(struct ipa_eth_channel *ch, unsigned long event)
{
	return ipa_eth_nd_op(ch->eth_dev, disable_event, ch, event);
}
EXPORT_SYMBOL(ipa_eth_net_disable_event);

int ipa_eth_net_moderate_event(struct ipa_eth_channel *ch, unsigned long event,
	u64 min_count, u64 max_count,
	u64 min_usecs, u64 max_usecs)
{
	return ipa_eth_nd_op(ch->eth_dev, moderate_event, ch, event,
		min_count, max_count, min_usecs, max_usecs);
}
EXPORT_SYMBOL(ipa_eth_net_moderate_event);

int ipa_eth_net_receive_skb(struct ipa_eth_device *eth_dev,
	struct sk_buff *skb)
{
	return ipa_eth_nd_op(eth_dev,
		receive_skb, eth_dev, skb, ipa_get_lan_rx_napi());
}
EXPORT_SYMBOL(ipa_eth_net_receive_skb);

int ipa_eth_net_transmit_skb(struct ipa_eth_device *eth_dev,
	struct sk_buff *skb)
{
	return ipa_eth_nd_op(eth_dev, transmit_skb, eth_dev, skb);
}
EXPORT_SYMBOL(ipa_eth_net_transmit_skb);

/**
 * ipa_eth_net_ch_to_cb_mem() - Provides memory mapping of a specific channel
 *                              memory on an IPA hardware type
 * @ch: Channel to which the memory belong
 * @ch_mem: Channel memory whose hardware mapping need to be found out
 * @hw_type: Hardware for which the mapping need to be determined
 *
 * The SMMU context bank used by each hw_type could vary based on channel. Use
 * this API to correctly identify the context bank and the mapping made for the
 * channel memory to it.
 *
 * Return: Memory mapping info for the given @hw_type if a mapping was
 *         previously made via one of ipa_eth_net_*() APIs. NULL if no mapping
 *         was made before to the context bank associated with a @hw_type.
 */
struct ipa_eth_resource *ipa_eth_net_ch_to_cb_mem(
	struct ipa_eth_channel *ch,
	struct ipa_eth_channel_mem *ch_mem,
	enum ipa_eth_hw_type hw_type)
{
	struct ipa_eth_resource *cb_mem;
	enum ipa_smmu_cb_type cb_type = ipa_eth_hw_to_cb_type(ch, hw_type);

	if (ch_mem->cb_mem == NULL || cb_type >= IPA_SMMU_CB_MAX)
		return NULL;

	cb_mem = &ch_mem->cb_mem[cb_type];
	if (!cb_mem->size)
		return NULL;

	return cb_mem;
}
EXPORT_SYMBOL(ipa_eth_net_ch_to_cb_mem);

int ipa_eth_net_save_regs(struct ipa_eth_device *eth_dev)
{
	struct ipa_eth_net_driver *nd = eth_dev->nd;

	if (nd && nd->ops->save_regs)
		return ipa_eth_nd_op(eth_dev, save_regs, eth_dev, NULL, NULL);

	return 0;
}
