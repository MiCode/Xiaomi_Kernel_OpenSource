/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
#include <linux/rmnet_data.h>
#include <linux/msm_rmnet.h>
#include <linux/etherdevice.h>
#include <linux/if_arp.h>
#include <linux/spinlock.h>
#include <net/pkt_sched.h>
#include "rmnet_data_config.h"
#include "rmnet_data_handlers.h"
#include "rmnet_data_private.h"
#include "rmnet_map.h"

struct net_device *rmnet_devices[RMNET_DATA_MAX_VND];

struct rmnet_vnd_private_s {
	uint8_t qos_mode:1;
	uint8_t reserved:7;
	struct rmnet_logical_ep_conf_s local_ep;
	struct rmnet_map_flow_control_s flows;
};

/* ***************** Helper Functions *************************************** */

/**
 * rmnet_vnd_add_qos_header() - Adds QoS header to front of skb->data
 * @skb:        Socket buffer ("packet") to modify
 * @dev:        Egress interface
 *
 * Does not check for sufficient headroom! Caller must make sure there is enough
 * headroom.
 */
static void rmnet_vnd_add_qos_header(struct sk_buff *skb,
				     struct net_device *dev)
{
	struct QMI_QOS_HDR_S *qmih;

	qmih = (struct QMI_QOS_HDR_S *)
		skb_push(skb, sizeof(struct QMI_QOS_HDR_S));
	qmih->version = 1;
	qmih->flags = 0;
	qmih->flow_id = skb->mark;
}

/* ***************** RX/TX Fixup ******************************************** */

/**
 * rmnet_vnd_rx_fixup() - Virtual Network Device receive fixup hook
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
		BUG();

	dev->stats.rx_packets++;
	dev->stats.rx_bytes += skb->len;

	return RX_HANDLER_PASS;
}

/**
 * rmnet_vnd_tx_fixup() - Virtual Network Device transmic fixup hook
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
	dev_conf = (struct rmnet_vnd_private_s *) netdev_priv(dev);

	if (unlikely(!dev || !skb))
		BUG();

	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;

	return RX_HANDLER_PASS;
}

/* ***************** Network Device Operations ****************************** */

/**
 * rmnet_vnd_start_xmit() - Transmit NDO callback
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
	dev_conf = (struct rmnet_vnd_private_s *) netdev_priv(dev);
	if (dev_conf->local_ep.egress_dev) {
		/* QoS header should come after MAP header */
		if (dev_conf->qos_mode)
			rmnet_vnd_add_qos_header(skb, dev);
		rmnet_egress_handler(skb, &dev_conf->local_ep);
	} else {
		dev->stats.tx_dropped++;
		kfree_skb(skb);
	}
	return NETDEV_TX_OK;
}

/**
 * rmnet_vnd_change_mtu() - Change MTU NDO callback
 * @dev:         Virtual network device
 * @new_mtu:     New MTU value to set (in bytes)
 *
 * Standard network driver operations hook to set the MTU. Called by kernel to
 * set the device MTU. Checks if desired MTU is less than zero or greater than
 * RMNET_DATA_MAX_PACKET_SIZE;
 *
 * Return:
 *      - 0 if successful
 *      - -EINVAL if new_mtu is out of range
 */
static int rmnet_vnd_change_mtu(struct net_device *dev, int new_mtu)
{
	if (new_mtu < 0 || new_mtu > RMNET_DATA_MAX_PACKET_SIZE)
		return -EINVAL;

	dev->mtu = new_mtu;
	return 0;
}

#ifdef CONFIG_RMNET_DATA_FC
static int _rmnet_vnd_do_qos_ioctl(struct net_device *dev,
				   struct ifreq *ifr,
				   int cmd)
{
	struct rmnet_vnd_private_s *dev_conf;
	int rc;
	rc = 0;
	dev_conf = (struct rmnet_vnd_private_s *) netdev_priv(dev);

	switch (cmd) {

	case RMNET_IOCTL_SET_QOS_ENABLE:
		LOGM("%s(): RMNET_IOCTL_SET_QOS_ENABLE on %s\n",
		     __func__, dev->name);
		dev_conf->qos_mode = 1;
		break;

	case RMNET_IOCTL_SET_QOS_DISABLE:
		LOGM("%s(): RMNET_IOCTL_SET_QOS_DISABLE on %s\n",
		     __func__, dev->name);
		dev_conf->qos_mode = 0;
		break;

	case RMNET_IOCTL_FLOW_ENABLE:
		LOGL("%s(): RMNET_IOCTL_FLOW_ENABLE on %s\n",
		     __func__, dev->name);
		tc_qdisc_flow_control(dev, (u32)ifr->ifr_data, 1);
		break;

	case RMNET_IOCTL_FLOW_DISABLE:
		LOGL("%s(): RMNET_IOCTL_FLOW_DISABLE on %s\n",
		     __func__, dev->name);
		tc_qdisc_flow_control(dev, (u32)ifr->ifr_data, 0);
		break;

	case RMNET_IOCTL_GET_QOS:           /* Get QoS header state    */
		LOGM("%s(): RMNET_IOCTL_GET_QOS on %s\n",
		     __func__, dev->name);
		ifr->ifr_ifru.ifru_data =
			(void *)(dev_conf->qos_mode == 1);
		break;

	default:
		rc = -EINVAL;
	}

	return rc;
}
#else
static int _rmnet_vnd_do_qos_ioctl(struct net_device *dev,
				   struct ifreq *ifr,
				   int cmd)
{
	return -EINVAL;
}
#endif /* CONFIG_RMNET_DATA_FC */

/**
 * rmnet_vnd_ioctl() - IOCTL NDO callback
 * @dev:         Virtual network device
 * @ifreq:       User data
 * @cmd:         IOCTL command value
 *
 * Standard network driver operations hook to process IOCTLs. Called by kernel
 * to process non-stanard IOCTLs for device
 *
 * Return:
 *      - 0 if successful
 *      - -EINVAL if unknown IOCTL
 */
static int rmnet_vnd_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct rmnet_vnd_private_s *dev_conf;
	int rc;
	rc = 0;
	dev_conf = (struct rmnet_vnd_private_s *) netdev_priv(dev);

	rc = _rmnet_vnd_do_qos_ioctl(dev, ifr, cmd);
	if (rc != -EINVAL)
		return rc;
	rc = 0; /* Reset rc as it may contain -EINVAL from above */

	switch (cmd) {

	case RMNET_IOCTL_OPEN: /* Do nothing. Support legacy behavior */
		LOGM("%s(): RMNET_IOCTL_OPEN on %s (ignored)\n",
		     __func__, dev->name);
		break;

	case RMNET_IOCTL_CLOSE: /* Do nothing. Support legacy behavior */
		LOGM("%s(): RMNET_IOCTL_CLOSE on %s (ignored)\n",
		     __func__, dev->name);
		break;

	case RMNET_IOCTL_SET_LLP_ETHERNET:
		LOGM("%s(): RMNET_IOCTL_SET_LLP_ETHERNET on %s (no support)\n",
		     __func__, dev->name);
		rc = -EINVAL;
		break;

	case RMNET_IOCTL_SET_LLP_IP: /* Do nothing. Support legacy behavior */
		LOGM("%s(): RMNET_IOCTL_SET_LLP_IP on %s  (ignored)\n",
		     __func__, dev->name);
		break;

	case RMNET_IOCTL_GET_LLP: /* Always return IP mode */
		LOGM("%s(): RMNET_IOCTL_GET_LLP on %s\n",
		     __func__, dev->name);
		ifr->ifr_ifru.ifru_data = (void *)(RMNET_MODE_LLP_IP);
		break;

	default:
		LOGH("%s(): Unkown IOCTL 0x%08X\n", __func__, cmd);
		rc = -EINVAL;
	}

	return rc;
}

static const struct net_device_ops rmnet_data_vnd_ops = {
	.ndo_init = 0,
	.ndo_start_xmit = rmnet_vnd_start_xmit,
	.ndo_do_ioctl = rmnet_vnd_ioctl,
	.ndo_change_mtu = rmnet_vnd_change_mtu,
	.ndo_set_mac_address = 0,
	.ndo_validate_addr = 0,
};

/**
 * rmnet_vnd_setup() - net_device initialization callback
 * @dev:      Virtual network device
 *
 * Called by kernel whenever a new rmnet_data<n> device is created. Sets MTU,
 * flags, ARP type, needed headroom, etc...
 *
 * todo: What is watchdog_timeo? Do we need to explicitly set it?
 */
static void rmnet_vnd_setup(struct net_device *dev)
{
	struct rmnet_vnd_private_s *dev_conf;
	LOGM("%s(): Setting up device %s\n", __func__, dev->name);

	/* Clear out private data */
	dev_conf = (struct rmnet_vnd_private_s *) netdev_priv(dev);
	memset(dev_conf, 0, sizeof(struct rmnet_vnd_private_s));

	/* keep the default flags, just add NOARP */
	dev->flags |= IFF_NOARP;
	dev->netdev_ops = &rmnet_data_vnd_ops;
	dev->mtu = RMNET_DATA_DFLT_PACKET_SIZE;
	dev->needed_headroom = RMNET_DATA_NEEDED_HEADROOM;
	random_ether_addr(dev->dev_addr);
	dev->watchdog_timeo = 1000;

	/* Raw IP mode */
	dev->header_ops = 0;  /* No header */
	dev->type = ARPHRD_RAWIP;
	dev->hard_header_len = 0;
	dev->flags &= ~(IFF_BROADCAST | IFF_MULTICAST);

	/* Flow control locks */
	rwlock_init(&dev_conf->flows.flow_map_lock);
}

/* ***************** Exposed API ******************************************** */

/**
 * rmnet_vnd_exit() - Shutdown cleanup hook
 *
 * Called by RmNet main on module unload. Cleans up data structures and
 * unregisters/frees net_devices.
 */
void rmnet_vnd_exit(void)
{
	int i;
	for (i = 0; i < RMNET_DATA_MAX_VND; i++)
		if (rmnet_devices[i]) {
			unregister_netdev(rmnet_devices[i]);
			free_netdev(rmnet_devices[i]);
	}
}

/**
 * rmnet_vnd_init() - Init hook
 *
 * Called by RmNet main on module load. Initializes data structures
 */
int rmnet_vnd_init(void)
{
	memset(rmnet_devices, 0,
	       sizeof(struct net_device *) * RMNET_DATA_MAX_VND);
	return 0;
}

/**
 * rmnet_vnd_create_dev() - Create a new virtual network device node.
 * @id:         Virtual device node id
 * @new_device: Pointer to newly created device node
 *
 * Allocates structures for new virtual network devices. Sets the name of the
 * new device and registers it with the network stack. Device will appear in
 * ifconfig list after this is called.
 *
 * Return:
 *      - 0 if successful
 *      - -EINVAL if id is out of range, or id already in use
 *      - -EINVAL if net_device allocation failed
 *      - return code of register_netdevice() on other errors
 */
int rmnet_vnd_create_dev(int id, struct net_device **new_device)
{
	struct net_device *dev;
	int rc = 0;

	if (id < 0 || id > RMNET_DATA_MAX_VND || rmnet_devices[id] != 0) {
		*new_device = 0;
		return -EINVAL;
	}

	dev = alloc_netdev(sizeof(struct rmnet_vnd_private_s),
			   RMNET_DATA_DEV_NAME_STR,
			   rmnet_vnd_setup);
	if (!dev) {
		LOGE("%s(): Failed to to allocate netdev for id %d",
		      __func__, id);
		*new_device = 0;
		return -EINVAL;
	}

	rc = register_netdevice(dev);
	if (rc != 0) {
		LOGE("%s(): Failed to to register netdev [%s]",
		      __func__, dev->name);
		free_netdev(dev);
		*new_device = 0;
	} else {
		rmnet_devices[id] = dev;
		*new_device = dev;
	}

	LOGM("%s(): Registered device %s\n", __func__, dev->name);
	return rc;
}

/**
 * rmnet_vnd_is_vnd() - Determine if net_device is RmNet owned virtual devices
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
	/*
	 * This is not an efficient search, but, this will only be called in
	 * a configuration context, and the list is small.
	 */
	int i;

	if (!dev)
		BUG();

	for (i = 0; i < RMNET_DATA_MAX_VND; i++)
		if (dev == rmnet_devices[i])
			return i+1;

	return 0;
}

/**
 * rmnet_vnd_get_le_config() - Get the logical endpoint configuration
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

	dev_conf = (struct rmnet_vnd_private_s *) netdev_priv(dev);
	if (!dev_conf)
		BUG();

	return &dev_conf->local_ep;
}

/**
 * rmnet_vnd_get_flow_mapping() - Retrieve QoS flow mapping.
 * @dev: Virtual network device node to do lookup on
 * @map_flow_id: Flow ID
 * @tc_handle: Pointer to TC qdisc flow handle. Results stored here
 * @v4_seq: pointer to IPv4 indication sequence number. Caller can modify value
 * @v6_seq: pointer to IPv6 indication sequence number. Caller can modify value
 *
 * Sets flow_map to 0 on error or if no flow is configured
 * todo: Add flow specific mappings
 * todo: Standardize return codes.
 *
 * Return:
 *      - 0 if successful
 *      - 1 if no mapping is found
 *      - 2 if dev is not RmNet virtual network device node
 */
int rmnet_vnd_get_flow_mapping(struct net_device *dev,
			       uint32_t map_flow_id,
			       uint32_t *tc_handle,
			       uint64_t **v4_seq,
			       uint64_t **v6_seq)
{
	struct rmnet_vnd_private_s *dev_conf;
	struct rmnet_map_flow_mapping_s *flowmap;
	int i;
	int error = 0;

	if (!dev || !tc_handle)
		BUG();

	if (!rmnet_vnd_is_vnd(dev)) {
		*tc_handle = 0;
		return 2;
	} else {
		dev_conf = (struct rmnet_vnd_private_s *) netdev_priv(dev);
	}

	if (!dev_conf)
		BUG();

	if (map_flow_id == 0xFFFFFFFF) {
		*tc_handle = dev_conf->flows.default_tc_handle;
		*v4_seq = &dev_conf->flows.default_v4_seq;
		*v6_seq = &dev_conf->flows.default_v6_seq;
		if (*tc_handle == 0)
			error = 1;
	} else {
		flowmap = &dev_conf->flows.flowmap[0];
		for (i = 0; i < RMNET_MAP_MAX_FLOWS; i++) {
			if ((flowmap[i].flow_id != 0)
			     && (flowmap[i].flow_id == map_flow_id)) {

				*tc_handle = flowmap[i].tc_handle;
				*v4_seq = &flowmap[i].v4_seq;
				*v6_seq = &flowmap[i].v6_seq;
				error = 0;
				break;
			}
		}
		*v4_seq = 0;
		*v6_seq = 0;
		*tc_handle = 0;
		error = 1;
	}

	return error;
}
