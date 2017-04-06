/* Copyright (c) 2013-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 * RMNET Data virtual network driver
 *
 */

#include <linux/types.h>
#include <linux/rmnet.h>
#include <linux/etherdevice.h>
#include <linux/if_arp.h>
#include <linux/spinlock.h>
#include <net/pkt_sched.h>
#include <linux/atomic.h>
#include "rmnet_config.h"
#include "rmnet_handlers.h"
#include "rmnet_private.h"
#include "rmnet_map.h"
#include "rmnet_vnd.h"
#include "rmnet_stats.h"

RMNET_LOG_MODULE(RMNET_LOGMASK_VND);

struct net_device *rmnet_devices[RMNET_MAX_VND];

struct rmnet_vnd_private_s {
	struct rmnet_logical_ep_conf_s local_ep;
};

/* RX/TX Fixup */

/* rmnet_vnd_rx_fixup() - Virtual Network Device receive fixup hook
 * @skb:        Socket buffer ("packet") to modify
 * @dev:        Virtual network device
 *
 * Additional VND specific packet processing for ingress packets
 *
 * Return:
 *      - RX_HANDLER_PASS if packet should continue to process in stack
 *      - RX_HANDLER_CONSUMED if packet should not be processed in stack
 *
 */
int rmnet_vnd_rx_fixup(struct sk_buff *skb, struct net_device *dev)
{
	if (unlikely(!dev || !skb))
		return RX_HANDLER_CONSUMED;

	dev->stats.rx_packets++;
	dev->stats.rx_bytes += skb->len;

	return RX_HANDLER_PASS;
}

/* rmnet_vnd_tx_fixup() - Virtual Network Device transmic fixup hook
 * @skb:      Socket buffer ("packet") to modify
 * @dev:      Virtual network device
 *
 * Additional VND specific packet processing for egress packets
 *
 * Return:
 *      - RX_HANDLER_PASS if packet should continue to be transmitted
 *      - RX_HANDLER_CONSUMED if packet should not be transmitted by stack
 */
int rmnet_vnd_tx_fixup(struct sk_buff *skb, struct net_device *dev)
{
	struct rmnet_vnd_private_s *dev_conf;

	dev_conf = (struct rmnet_vnd_private_s *)netdev_priv(dev);

	if (unlikely(!dev || !skb))
		return RX_HANDLER_CONSUMED;

	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;

	return RX_HANDLER_PASS;
}

/* Network Device Operations */

/* rmnet_vnd_start_xmit() - Transmit NDO callback
 * @skb:        Socket buffer ("packet") being sent from network stack
 * @dev:        Virtual Network Device
 *
 * Standard network driver operations hook to transmit packets on virtual
 * network device. Called by network stack. Packet is not transmitted directly
 * from here; instead it is given to the rmnet egress handler.
 *
 * Return:
 *      - NETDEV_TX_OK under all cirumstances (cannot block/fail)
 */
static netdev_tx_t rmnet_vnd_start_xmit(struct sk_buff *skb,
					struct net_device *dev)
{
	struct rmnet_vnd_private_s *dev_conf;

	dev_conf = (struct rmnet_vnd_private_s *)netdev_priv(dev);
	if (dev_conf->local_ep.egress_dev) {
		rmnet_egress_handler(skb, &dev_conf->local_ep);
	} else {
		dev->stats.tx_dropped++;
		rmnet_kfree_skb(skb, RMNET_STATS_SKBFREE_VND_NO_EGRESS);
	}
	return NETDEV_TX_OK;
}

/* rmnet_vnd_change_mtu() - Change MTU NDO callback
 * @dev:         Virtual network device
 * @new_mtu:     New MTU value to set (in bytes)
 *
 * Standard network driver operations hook to set the MTU. Called by kernel to
 * set the device MTU. Checks if desired MTU is less than zero or greater than
 * RMNET_MAX_PACKET_SIZE;
 *
 * Return:
 *      - 0 if successful
 *      - -EINVAL if new_mtu is out of range
 */
static int rmnet_vnd_change_mtu(struct net_device *dev, int new_mtu)
{
	if (new_mtu < 0 || new_mtu > RMNET_MAX_PACKET_SIZE)
		return -EINVAL;

	dev->mtu = new_mtu;
	return 0;
}

static const struct net_device_ops rmnet_vnd_ops = {
	.ndo_init = 0,
	.ndo_start_xmit = rmnet_vnd_start_xmit,
	.ndo_change_mtu = rmnet_vnd_change_mtu,
	.ndo_set_mac_address = 0,
	.ndo_validate_addr = 0,
};

/* rmnet_vnd_setup() - net_device initialization callback
 * @dev:      Virtual network device
 *
 * Called by kernel whenever a new rmnet<n> device is created. Sets MTU,
 * flags, ARP type, needed headroom, etc...
 */
static void rmnet_vnd_setup(struct net_device *dev)
{
	struct rmnet_vnd_private_s *dev_conf;

	LOGM("Setting up device %s", dev->name);

	/* Clear out private data */
	dev_conf = (struct rmnet_vnd_private_s *)netdev_priv(dev);
	memset(dev_conf, 0, sizeof(struct rmnet_vnd_private_s));

	dev->netdev_ops = &rmnet_vnd_ops;
	dev->mtu = RMNET_DFLT_PACKET_SIZE;
	dev->needed_headroom = RMNET_NEEDED_HEADROOM;
	random_ether_addr(dev->dev_addr);
	dev->tx_queue_len = RMNET_TX_QUEUE_LEN;

	/* Raw IP mode */
	dev->header_ops = 0;  /* No header */
	dev->type = ARPHRD_RAWIP;
	dev->hard_header_len = 0;
	dev->flags &= ~(IFF_BROADCAST | IFF_MULTICAST);
}

/* Exposed API */

/* rmnet_vnd_exit() - Shutdown cleanup hook
 *
 * Called by RmNet main on module unload. Cleans up data structures and
 * unregisters/frees net_devices.
 */
void rmnet_vnd_exit(void)
{
	int i;

	for (i = 0; i < RMNET_MAX_VND; i++)
		if (rmnet_devices[i]) {
			unregister_netdev(rmnet_devices[i]);
			free_netdev(rmnet_devices[i]);
	}
}

/* rmnet_vnd_init() - Init hook
 *
 * Called by RmNet main on module load. Initializes data structures
 */
int rmnet_vnd_init(void)
{
	memset(rmnet_devices, 0,
	       sizeof(struct net_device *) * RMNET_MAX_VND);
	return 0;
}

/* rmnet_vnd_create_dev() - Create a new virtual network device node.
 * @id:         Virtual device node id
 * @new_device: Pointer to newly created device node
 * @prefix:     Device name prefix
 *
 * Allocates structures for new virtual network devices. Sets the name of the
 * new device and registers it with the network stack. Device will appear in
 * ifconfig list after this is called. If the prefix is null, then
 * RMNET_DEV_NAME_STR will be assumed.
 *
 * Return:
 *      - 0 if successful
 *      - RMNET_CONFIG_BAD_ARGUMENTS if id is out of range or prefix is too long
 *      - RMNET_CONFIG_DEVICE_IN_USE if id already in use
 *      - RMNET_CONFIG_NOMEM if net_device allocation failed
 *      - RMNET_CONFIG_UNKNOWN_ERROR if register_netdevice() fails
 */
int rmnet_vnd_create_dev(int id, struct net_device **new_device,
			 const char *prefix)
{
	struct net_device *dev;
	char dev_prefix[IFNAMSIZ];
	int p, rc = 0;

	if (id < 0 || id >= RMNET_MAX_VND) {
		*new_device = 0;
		return RMNET_CONFIG_BAD_ARGUMENTS;
	}

	if (rmnet_devices[id]) {
		*new_device = 0;
		return RMNET_CONFIG_DEVICE_IN_USE;
	}

	if (!prefix)
		p = scnprintf(dev_prefix, IFNAMSIZ, "%s%%d",
			      RMNET_DEV_NAME_STR);
	else
		p = scnprintf(dev_prefix, IFNAMSIZ, "%s%%d", prefix);
	if (p >= (IFNAMSIZ - 1)) {
		LOGE("Specified prefix longer than IFNAMSIZ");
		return RMNET_CONFIG_BAD_ARGUMENTS;
	}

	dev = alloc_netdev(sizeof(struct rmnet_vnd_private_s),
			   dev_prefix,
			   NET_NAME_ENUM,
			   rmnet_vnd_setup);
	if (!dev) {
		LOGE("Failed to to allocate netdev for id %d", id);
		*new_device = 0;
		return RMNET_CONFIG_NOMEM;
	}

	rc = register_netdevice(dev);
	if (rc != 0) {
		LOGE("Failed to to register netdev [%s]", dev->name);
		free_netdev(dev);
		*new_device = 0;
		rc = RMNET_CONFIG_UNKNOWN_ERROR;
	} else {
		rmnet_devices[id] = dev;
		*new_device = dev;
		LOGM("Registered device %s", dev->name);
	}

	return rc;
}

/* rmnet_vnd_free_dev() - free a virtual network device node.
 * @id:         Virtual device node id
 *
 * Unregisters the virtual network device node and frees it.
 * unregister_netdev locks the rtnl mutex, so the mutex must not be locked
 * by the caller of the function. unregister_netdev enqueues the request to
 * unregister the device into a TODO queue. The requests in the TODO queue
 * are only done after rtnl mutex is unlocked, therefore free_netdev has to
 * called after unlocking rtnl mutex.
 *
 * Return:
 *      - 0 if successful
 *      - RMNET_CONFIG_NO_SUCH_DEVICE if id is invalid or not in range
 *      - RMNET_CONFIG_DEVICE_IN_USE if device has logical ep that wasn't unset
 */
int rmnet_vnd_free_dev(int id)
{
	struct rmnet_logical_ep_conf_s *epconfig_l;
	struct net_device *dev;

	rtnl_lock();
	if ((id < 0) || (id >= RMNET_MAX_VND) || !rmnet_devices[id]) {
		rtnl_unlock();
		LOGM("Invalid id [%d]", id);
		return RMNET_CONFIG_NO_SUCH_DEVICE;
	}

	epconfig_l = rmnet_vnd_get_le_config(rmnet_devices[id]);
	if (epconfig_l && epconfig_l->refcount) {
		rtnl_unlock();
		return RMNET_CONFIG_DEVICE_IN_USE;
	}

	dev = rmnet_devices[id];
	rmnet_devices[id] = 0;
	rtnl_unlock();

	if (dev) {
		unregister_netdev(dev);
		free_netdev(dev);
		return 0;
	} else {
		return RMNET_CONFIG_NO_SUCH_DEVICE;
	}
}

/* rmnet_vnd_get_name() - Gets the string name of a VND based on ID
 * @id:         Virtual device node id
 * @name:       Buffer to store name of virtual device node
 * @name_len:   Length of name buffer
 *
 * Copies the name of the virtual device node into the users buffer. Will throw
 * an error if the buffer is null, or too small to hold the device name.
 *
 * Return:
 *      - 0 if successful
 *      - -EINVAL if name is null
 *      - -EINVAL if id is invalid or not in range
 *      - -EINVAL if name is too small to hold things
 */
int rmnet_vnd_get_name(int id, char *name, int name_len)
{
	int p;

	if (!name) {
		LOGM("%s", "Bad arguments; name buffer null");
		return -EINVAL;
	}

	if ((id < 0) || (id >= RMNET_MAX_VND) || !rmnet_devices[id]) {
		LOGM("Invalid id [%d]", id);
		return -EINVAL;
	}

	p = strlcpy(name, rmnet_devices[id]->name, name_len);
	if (p >= name_len) {
		LOGM("Buffer to small (%d) to fit device name", name_len);
		return -EINVAL;
	}
	LOGL("Found mapping [%d]->\"%s\"", id, name);

	return 0;
}

/* rmnet_vnd_is_vnd() - Determine if net_device is RmNet owned virtual devices
 * @dev:        Network device to test
 *
 * Searches through list of known RmNet virtual devices. This function is O(n)
 * and should not be used in the data path.
 *
 * Return:
 *      - 0 if device is not RmNet virtual device
 *      - 1 if device is RmNet virtual device
 */
int rmnet_vnd_is_vnd(struct net_device *dev)
{
	/* This is not an efficient search, but, this will only be called in
	 * a configuration context, and the list is small.
	 */
	int i;

	if (!dev)
		return 0;

	for (i = 0; i < RMNET_MAX_VND; i++)
		if (dev == rmnet_devices[i])
			return i + 1;

	return 0;
}

/* rmnet_vnd_get_le_config() - Get the logical endpoint configuration
 * @dev:      Virtual device node
 *
 * Gets the logical endpoint configuration for a RmNet virtual network device
 * node. Caller should confirm that devices is a RmNet VND before calling.
 *
 * Return:
 *      - Pointer to logical endpoint configuration structure
 *      - 0 (null) if dev is null
 */
struct rmnet_logical_ep_conf_s *rmnet_vnd_get_le_config(struct net_device *dev)
{
	struct rmnet_vnd_private_s *dev_conf;

	if (!dev)
		return 0;

	dev_conf = (struct rmnet_vnd_private_s *)netdev_priv(dev);
	if (!dev_conf)
		return 0;

	return &dev_conf->local_ep;
}

/* rmnet_vnd_do_flow_control() - Process flow control request
 * @dev: Virtual network device node to do lookup on
 * @enable: boolean to enable/disable flow.
 *
 * Return:
 *      - 0 if successful
 *      - -EINVAL if dev is not RmNet virtual network device node
 */
int rmnet_vnd_do_flow_control(struct net_device *dev, int enable)
{
	struct rmnet_vnd_private_s *dev_conf;

	if (unlikely(!dev))
		return -EINVAL;

	if (!rmnet_vnd_is_vnd(dev))
		return -EINVAL;

	dev_conf = (struct rmnet_vnd_private_s *)netdev_priv(dev);

	if (unlikely(!dev_conf))
		return -EINVAL;

	LOGD("Setting VND TX queue state to %d", enable);
	/* Although we expect similar number of enable/disable
	 * commands, optimize for the disable. That is more
	 * latency sensitive than enable
	 */
	if (unlikely(enable))
		netif_wake_queue(dev);
	else
		netif_stop_queue(dev);

	return 0;
}

/* rmnet_vnd_get_by_id() - Get VND by array index ID
 * @id: Virtual network deice id [0:RMNET_MAX_VND]
 *
 * Return:
 *      - 0 if no device or ID out of range
 *      - otherwise return pointer to VND net_device struct
 */
struct net_device *rmnet_vnd_get_by_id(int id)
{
	if (id < 0 || id >= RMNET_MAX_VND) {
		pr_err("Bug; VND ID out of bounds");
		return 0;
	}
	return rmnet_devices[id];
}
