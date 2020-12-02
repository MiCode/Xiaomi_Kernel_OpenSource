// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved
 */

#include <linux/rtnetlink.h>

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
		ops->close_device;
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

static inline bool __is_netdev_link_up(struct ipa_eth_device *eth_dev)
{
	return netif_carrier_ok(eth_dev->net_dev);
}

static inline bool __is_netdev_iface_up(struct ipa_eth_device *eth_dev)
{
	return !!(eth_dev->net_dev->flags & IFF_UP);
}

bool ipa_eth_net_check_active(struct ipa_eth_device *eth_dev)
{
	bool active = false;
	struct rtnl_link_stats64 curr_rtnl_stats;
	struct ipa_eth_device_private *ipa_priv = eth_dev_priv(eth_dev);
	struct rtnl_link_stats64 *last_rtnl_stats = &ipa_priv->last_rtnl_stats;

	dev_get_stats(eth_dev->net_dev, &curr_rtnl_stats);

	if (ipa_priv->assume_active) {
		ipa_priv->assume_active--;
		active = true;
	}

	if (curr_rtnl_stats.rx_packets != last_rtnl_stats->rx_packets)
		active = true;

	*last_rtnl_stats = curr_rtnl_stats;

	return active;
}

static bool ipa_eth_net_update_link(struct ipa_eth_device *eth_dev)
{
	return __is_netdev_link_up(eth_dev) ?
		!test_and_set_bit(IPA_ETH_IF_ST_LOWER_UP, &eth_dev->if_state) :
		test_and_clear_bit(IPA_ETH_IF_ST_LOWER_UP, &eth_dev->if_state);

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
	return ipa_eth_net_update_link(eth_dev);
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
	} else {
		ipa_eth_dev_err(eth_dev, "Event number out of bounds");
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
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

	/* When registering with netdevice notifier, only REGISTER and UP events
	 * are replayed. Fetch current link state; future link state updates
	 * will be processed through CHANGE event.
	 */
	if (ipa_eth_net_update_link(eth_dev))
		ipa_eth_device_refresh_sched(eth_dev);

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

int ipa_eth_net_save_regs(struct ipa_eth_device *eth_dev)
{
	struct ipa_eth_net_driver *nd = eth_dev->nd;

	if (nd && nd->ops->save_regs)
		return ipa_eth_nd_op(eth_dev, save_regs, eth_dev, NULL, NULL);

	return 0;
}
