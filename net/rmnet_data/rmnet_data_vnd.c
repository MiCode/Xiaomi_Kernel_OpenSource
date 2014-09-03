/*
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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
#include <linux/atomic.h>
#include <linux/net_map.h>
#include "rmnet_data_config.h"
#include "rmnet_data_handlers.h"
#include "rmnet_data_private.h"
#include "rmnet_map.h"
#include "rmnet_data_vnd.h"
#include "rmnet_data_stats.h"
#include "rmnet_data_trace.h"

RMNET_LOG_MODULE(RMNET_DATA_LOGMASK_VND);

#define RMNET_MAP_FLOW_NUM_TC_HANDLE 3
#define RMNET_VND_UF_ACTION_ADD 0
#define RMNET_VND_UF_ACTION_DEL 1
enum {
	RMNET_VND_UPDATE_FLOW_OK,
	RMNET_VND_UPDATE_FLOW_NO_ACTION,
	RMNET_VND_UPDATE_FLOW_NO_MORE_ROOM,
	RMNET_VND_UPDATE_FLOW_NO_VALID_LEFT
};

struct net_device *rmnet_devices[RMNET_DATA_MAX_VND];

struct rmnet_map_flow_mapping_s {
	struct list_head list;
	uint32_t map_flow_id;
	uint32_t tc_flow_valid[RMNET_MAP_FLOW_NUM_TC_HANDLE];
	uint32_t tc_flow_id[RMNET_MAP_FLOW_NUM_TC_HANDLE];
	atomic_t v4_seq;
	atomic_t v6_seq;
};

struct rmnet_vnd_private_s {
	uint32_t qos_version;
	struct rmnet_logical_ep_conf_s local_ep;

	rwlock_t flow_map_lock;
	struct list_head flow_head;
};

#define RMNET_VND_FC_QUEUED      0
#define RMNET_VND_FC_NOT_ENABLED 1
#define RMNET_VND_FC_KMALLOC_ERR 2

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
				     struct net_device *dev,
				     uint32_t qos_version)
{
	struct QMI_QOS_HDR_S *qmih;
	struct qmi_qos_hdr8_s *qmi8h;

	if (qos_version & RMNET_IOCTL_QOS_MODE_6) {
		qmih = (struct QMI_QOS_HDR_S *)
			skb_push(skb, sizeof(struct QMI_QOS_HDR_S));
		qmih->version = 1;
		qmih->flags = 0;
		qmih->flow_id = skb->mark;
	} else if (qos_version & RMNET_IOCTL_QOS_MODE_8) {
		qmi8h = (struct qmi_qos_hdr8_s *)
			skb_push(skb, sizeof(struct qmi_qos_hdr8_s));
		/* Flags are 0 always */
		qmi8h->hdr.version = 0;
		qmi8h->hdr.flags = 0;
		memset(qmi8h->reserved, 0, sizeof(qmi8h->reserved));
		qmi8h->hdr.flow_id = skb->mark;
	} else {
		LOGD("%s(): Bad QoS version configured\n", __func__);
	}
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
	trace_rmnet_vnd_start_xmit(skb);
	dev_conf = (struct rmnet_vnd_private_s *) netdev_priv(dev);
	if (dev_conf->local_ep.egress_dev) {
		/* QoS header should come after MAP header */
		if (dev_conf->qos_version)
			rmnet_vnd_add_qos_header(skb,
						 dev,
						 dev_conf->qos_version);
		rmnet_egress_handler(skb, &dev_conf->local_ep);
	} else {
		dev->stats.tx_dropped++;
		rmnet_kfree_skb(skb, RMNET_STATS_SKBFREE_VND_NO_EGRESS);
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
	int rc, qdisc_len = 0;
	struct rmnet_ioctl_data_s ioctl_data;
	rc = 0;
	dev_conf = (struct rmnet_vnd_private_s *) netdev_priv(dev);

	switch (cmd) {

	case RMNET_IOCTL_SET_QOS_ENABLE:
		LOGM("RMNET_IOCTL_SET_QOS_ENABLE on %s", dev->name);
		if (!dev_conf->qos_version)
			dev_conf->qos_version = RMNET_IOCTL_QOS_MODE_6;
		break;

	case RMNET_IOCTL_SET_QOS_DISABLE:
		LOGM("RMNET_IOCTL_SET_QOS_DISABLE on %s", dev->name);
		dev_conf->qos_version = 0;
		break;

	case RMNET_IOCTL_GET_QOS:           /* Get QoS header state    */
		LOGM("RMNET_IOCTL_GET_QOS on %s", dev->name);
		ioctl_data.u.operation_mode = (dev_conf->qos_version ==
						RMNET_IOCTL_QOS_MODE_6);
		if (copy_to_user(ifr->ifr_ifru.ifru_data, &ioctl_data,
			sizeof(struct rmnet_ioctl_data_s)))
			rc = -EFAULT;
		break;

	case RMNET_IOCTL_FLOW_ENABLE:
		LOGL("RMNET_IOCTL_FLOW_ENABLE on %s", dev->name);
		if (copy_from_user(&ioctl_data, ifr->ifr_ifru.ifru_data,
			sizeof(struct rmnet_ioctl_data_s))) {
			rc = -EFAULT;
			break;
		}
		qdisc_len = tc_qdisc_flow_control(dev,
						  ioctl_data.u.tcm_handle, 1);
		trace_rmnet_fc_qmi(ioctl_data.u.tcm_handle, qdisc_len, 1);
		break;

	case RMNET_IOCTL_FLOW_DISABLE:
		LOGL("RMNET_IOCTL_FLOW_DISABLE on %s", dev->name);
		if (copy_from_user(&ioctl_data, ifr->ifr_ifru.ifru_data,
			sizeof(struct rmnet_ioctl_data_s))) {
			rc = -EFAULT;
		break;
		}
		qdisc_len = tc_qdisc_flow_control(dev,
						  ioctl_data.u.tcm_handle, 0);
		trace_rmnet_fc_qmi(ioctl_data.u.tcm_handle, qdisc_len, 0);
		break;

	default:
		rc = -EINVAL;
	}

	return rc;
}

struct rmnet_vnd_fc_work {
	struct work_struct work;
	struct net_device *dev;
	uint32_t tc_handle;
	int enable;
};

static void _rmnet_vnd_wq_flow_control(struct work_struct *work)
{
	struct rmnet_vnd_fc_work *fcwork;
	int qdisc_len = 0;
	fcwork = (struct rmnet_vnd_fc_work *)work;

	rtnl_lock();
	qdisc_len = tc_qdisc_flow_control(fcwork->dev, fcwork->tc_handle,
				     fcwork->enable);
	trace_rmnet_fc_map(fcwork->tc_handle, qdisc_len, fcwork->enable);
	rtnl_unlock();

	LOGL("[%s] handle:%08X enable:%d",
	     fcwork->dev->name, fcwork->tc_handle, fcwork->enable);

	kfree(work);
}

static int _rmnet_vnd_do_flow_control(struct net_device *dev,
					     uint32_t tc_handle,
					     int enable)
{
	struct rmnet_vnd_fc_work *fcwork;

	fcwork = (struct rmnet_vnd_fc_work *)
			kmalloc(sizeof(struct rmnet_vnd_fc_work), GFP_ATOMIC);
	if (!fcwork)
		return RMNET_VND_FC_KMALLOC_ERR;
	memset(fcwork, 0, sizeof(struct rmnet_vnd_fc_work));

	INIT_WORK((struct work_struct *)fcwork, _rmnet_vnd_wq_flow_control);
	fcwork->dev = dev;
	fcwork->tc_handle = tc_handle;
	fcwork->enable = enable;

	schedule_work((struct work_struct *)fcwork);
	return RMNET_VND_FC_QUEUED;
}
#else
static int _rmnet_vnd_do_qos_ioctl(struct net_device *dev,
				   struct ifreq *ifr,
				   int cmd)
{
	return -EINVAL;
}

static inline int _rmnet_vnd_do_flow_control(struct net_device *dev,
					     uint32_t tc_handle,
					     int enable)
{
	LOGD("[%s] called with no QoS support", dev->name);
	return RMNET_VND_FC_NOT_ENABLED;
}
#endif /* CONFIG_RMNET_DATA_FC */

static int rmnet_vnd_ioctl_extended(struct net_device *dev, struct ifreq *ifr)
{
	struct rmnet_vnd_private_s *dev_conf;
	struct rmnet_ioctl_extended_s ext_cmd;
	int rc = 0;
	dev_conf = (struct rmnet_vnd_private_s *) netdev_priv(dev);

	rc = copy_from_user(&ext_cmd, ifr->ifr_ifru.ifru_data,
			    sizeof(struct rmnet_ioctl_extended_s));
	if (rc) {
		LOGM("%s(): copy_from_user() failed\n", __func__);
		return rc;
	}

	switch (ext_cmd.extended_ioctl) {
	case RMNET_IOCTL_GET_SUPPORTED_FEATURES:
		ext_cmd.u.data = 0;
		break;

	case RMNET_IOCTL_GET_DRIVER_NAME:
		strlcpy(ext_cmd.u.if_name, "rmnet_data",
			sizeof(ext_cmd.u.if_name));
		break;

	case RMNET_IOCTL_GET_SUPPORTED_QOS_MODES:
		ext_cmd.u.data = RMNET_IOCTL_QOS_MODE_6
				 | RMNET_IOCTL_QOS_MODE_8;
		break;

	case RMNET_IOCTL_GET_QOS_VERSION:
		ext_cmd.u.data = dev_conf->qos_version;
		break;

	case RMNET_IOCTL_SET_QOS_VERSION:
		if (ext_cmd.u.data == RMNET_IOCTL_QOS_MODE_6
		    || ext_cmd.u.data == RMNET_IOCTL_QOS_MODE_8
		    ||  ext_cmd.u.data == 0) {
			dev_conf->qos_version = ext_cmd.u.data;
		} else {
			rc = -EINVAL;
			goto done;
		}
		break;

	default:
		rc = -EINVAL;
		goto done;
		break;
	}

	rc = copy_to_user(ifr->ifr_ifru.ifru_data, &ext_cmd,
			  sizeof(struct rmnet_ioctl_extended_s));
	if (rc)
		LOGM("%s(): copy_to_user() failed\n", __func__);

done:
	return rc;
}


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
	struct rmnet_ioctl_data_s ioctl_data;
	rc = 0;
	dev_conf = (struct rmnet_vnd_private_s *) netdev_priv(dev);

	rc = _rmnet_vnd_do_qos_ioctl(dev, ifr, cmd);
	if (rc != -EINVAL)
		return rc;
	rc = 0; /* Reset rc as it may contain -EINVAL from above */

	switch (cmd) {

	case RMNET_IOCTL_OPEN: /* Do nothing. Support legacy behavior */
		LOGM("RMNET_IOCTL_OPEN on %s (ignored)", dev->name);
		break;

	case RMNET_IOCTL_CLOSE: /* Do nothing. Support legacy behavior */
		LOGM("RMNET_IOCTL_CLOSE on %s (ignored)", dev->name);
		break;

	case RMNET_IOCTL_SET_LLP_ETHERNET:
		LOGM("RMNET_IOCTL_SET_LLP_ETHERNET on %s (no support)",
		     dev->name);
		rc = -EINVAL;
		break;

	case RMNET_IOCTL_SET_LLP_IP: /* Do nothing. Support legacy behavior */
		LOGM("RMNET_IOCTL_SET_LLP_IP on %s (ignored)", dev->name);
		break;

	case RMNET_IOCTL_GET_LLP: /* Always return IP mode */
		LOGM("RMNET_IOCTL_GET_LLP on %s", dev->name);
		ioctl_data.u.operation_mode = RMNET_MODE_LLP_IP;
		if (copy_to_user(ifr->ifr_ifru.ifru_data, &ioctl_data,
			sizeof(struct rmnet_ioctl_data_s)))
			rc = -EFAULT;
		break;

	case RMNET_IOCTL_EXTENDED:
		rc = rmnet_vnd_ioctl_extended(dev, ifr);
		break;

	default:
		LOGH("Unkown IOCTL 0x%08X", cmd);
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
	LOGM("Setting up device %s", dev->name);

	/* Clear out private data */
	dev_conf = (struct rmnet_vnd_private_s *) netdev_priv(dev);
	memset(dev_conf, 0, sizeof(struct rmnet_vnd_private_s));

	dev->netdev_ops = &rmnet_data_vnd_ops;
	dev->mtu = RMNET_DATA_DFLT_PACKET_SIZE;
	dev->needed_headroom = RMNET_DATA_NEEDED_HEADROOM;
	random_ether_addr(dev->dev_addr);
	dev->watchdog_timeo = 1000;
	dev->tx_queue_len = RMNET_DATA_TX_QUEUE_LEN;

	/* Raw IP mode */
	dev->header_ops = 0;  /* No header */
	dev->type = ARPHRD_RAWIP;
	dev->hard_header_len = 0;
	dev->flags &= ~(IFF_BROADCAST | IFF_MULTICAST);

	/* Flow control */
	rwlock_init(&dev_conf->flow_map_lock);
	INIT_LIST_HEAD(&dev_conf->flow_head);
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
 * @prefix:     Device name prefix
 *
 * Allocates structures for new virtual network devices. Sets the name of the
 * new device and registers it with the network stack. Device will appear in
 * ifconfig list after this is called. If the prefix is null, then
 * RMNET_DATA_DEV_NAME_STR will be assumed.
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

	if (id < 0 || id >= RMNET_DATA_MAX_VND) {
		*new_device = 0;
		return RMNET_CONFIG_BAD_ARGUMENTS;
	}

	if (rmnet_devices[id] != 0) {
		*new_device = 0;
		return RMNET_CONFIG_DEVICE_IN_USE;
	}

	if (!prefix)
		p = scnprintf(dev_prefix, IFNAMSIZ, "%s%%d",
			  RMNET_DATA_DEV_NAME_STR);
	else
		p = scnprintf(dev_prefix, IFNAMSIZ, "%s%%d",
			  prefix);
	if (p >= (IFNAMSIZ-1)) {
		LOGE("Specified prefix longer than IFNAMSIZ");
		return RMNET_CONFIG_BAD_ARGUMENTS;
	}

	dev = alloc_netdev(sizeof(struct rmnet_vnd_private_s),
			   dev_prefix,
			   rmnet_vnd_setup);
	if (!dev) {
		LOGE("Failed to to allocate netdev for id %d", id);
		*new_device = 0;
		return RMNET_CONFIG_NOMEM;
	}

	if (!prefix) {
		/* Configuring UL checksum offload on rmnet_data interfaces */
		dev->hw_features = NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM;
	}

	rc = register_netdevice(dev);
	if (rc != 0) {
		LOGE("Failed to to register netdev [%s]", dev->name);
		free_netdev(dev);
		*new_device = 0;
		return RMNET_CONFIG_UNKNOWN_ERROR;
	} else {
		rmnet_devices[id] = dev;
		*new_device = dev;
	}

	LOGM("Registered device %s", dev->name);
	return rc;
}

/**
 * rmnet_vnd_free_dev() - free a virtual network device node.
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
	if ((id < 0) || (id >= RMNET_DATA_MAX_VND) || !rmnet_devices[id]) {
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

/**
 * rmnet_vnd_get_name() - Gets the string name of a VND based on ID
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

	if ((id < 0) || (id >= RMNET_DATA_MAX_VND) || !rmnet_devices[id]) {
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
 * _rmnet_vnd_get_flow_map() - Gets object representing a MAP flow handle
 * @dev_conf: Private configuration structure for virtual network device
 * @map_flow: MAP flow handle IF
 *
 * Loops through available flow mappings and compares the MAP flow handle.
 * Returns when mapping is found.
 *
 * Return:
 *      - Null if no mapping was found
 *      - Pointer to mapping otherwise
 */
static struct rmnet_map_flow_mapping_s *_rmnet_vnd_get_flow_map
					(struct rmnet_vnd_private_s *dev_conf,
					 uint32_t map_flow)
{
	struct list_head *p;
	struct rmnet_map_flow_mapping_s *itm;

	list_for_each(p, &(dev_conf->flow_head)) {
		itm = list_entry(p, struct rmnet_map_flow_mapping_s, list);

		if (unlikely(!itm))
			BUG();

		if (itm->map_flow_id == map_flow)
			return itm;
	}
	return 0;
}

/**
 * _rmnet_vnd_update_flow_map() - Add or remove individual TC flow handles
 * @action: One of RMNET_VND_UF_ACTION_ADD / RMNET_VND_UF_ACTION_DEL
 * @itm: Flow mapping object
 * @map_flow: TC flow handle
 *
 * RMNET_VND_UF_ACTION_ADD:
 * Will check for a free mapping slot in the mapping object. If one is found,
 * valid for that slot will be set to 1 and the value will be set.
 *
 * RMNET_VND_UF_ACTION_DEL:
 * Will check for matching tc handle. If found, valid for that slot will be
 * set to 0 and the value will also be zeroed.
 *
 * Return:
 *      - RMNET_VND_UPDATE_FLOW_OK tc flow handle is added/removed ok
 *      - RMNET_VND_UPDATE_FLOW_NO_MORE_ROOM if there are no more tc handles
 *      - RMNET_VND_UPDATE_FLOW_NO_VALID_LEFT if flow mapping is now empty
 *      - RMNET_VND_UPDATE_FLOW_NO_ACTION if no action was taken
 */
static int _rmnet_vnd_update_flow_map(uint8_t action,
				      struct rmnet_map_flow_mapping_s *itm,
				      uint32_t tc_flow)
{
	int rc, i, j;
	rc = RMNET_VND_UPDATE_FLOW_OK;

	switch (action) {
	case RMNET_VND_UF_ACTION_ADD:
		rc = RMNET_VND_UPDATE_FLOW_NO_MORE_ROOM;
		for (i = 0; i < RMNET_MAP_FLOW_NUM_TC_HANDLE; i++) {
			if (itm->tc_flow_valid[i] == 0) {
				itm->tc_flow_valid[i] = 1;
				itm->tc_flow_id[i] = tc_flow;
				rc = RMNET_VND_UPDATE_FLOW_OK;
				LOGD("{%p}->tc_flow_id[%d]=%08X",
				     itm, i, tc_flow);
				break;
			}
		}
		break;

	case RMNET_VND_UF_ACTION_DEL:
		j = 0;
		rc = RMNET_VND_UPDATE_FLOW_OK;
		for (i = 0; i < RMNET_MAP_FLOW_NUM_TC_HANDLE; i++) {
			if (itm->tc_flow_valid[i] == 1) {
				if (itm->tc_flow_id[i] == tc_flow) {
					itm->tc_flow_valid[i] = 0;
					itm->tc_flow_id[i] = 0;
					j++;
					LOGD("{%p}->tc_flow_id[%d]=0", itm, i);
				}
			} else {
				j++;
			}
		}
		if (j == RMNET_MAP_FLOW_NUM_TC_HANDLE)
			rc = RMNET_VND_UPDATE_FLOW_NO_VALID_LEFT;
		break;

	default:
		rc = RMNET_VND_UPDATE_FLOW_NO_ACTION;
		break;
	}
	return rc;
}

/**
 * rmnet_vnd_add_tc_flow() - Add a MAP/TC flow handle mapping
 * @id: Virtual network device ID
 * @map_flow: MAP flow handle
 * @tc_flow: TC flow handle
 *
 * Checkes for an existing flow mapping object corresponding to map_flow. If one
 * is found, then it will try to add to the existing mapping object. Otherwise,
 * a new mapping object is created.
 *
 * Return:
 *      - RMNET_CONFIG_OK if successful
 *      - RMNET_CONFIG_TC_HANDLE_FULL if there is no more room in the map object
 *      - RMNET_CONFIG_NOMEM failed to allocate a new map object
 */
int rmnet_vnd_add_tc_flow(uint32_t id, uint32_t map_flow, uint32_t tc_flow)
{
	struct rmnet_map_flow_mapping_s *itm;
	struct net_device *dev;
	struct rmnet_vnd_private_s *dev_conf;
	int r;
	unsigned long flags;

	if ((id < 0) || (id >= RMNET_DATA_MAX_VND) || !rmnet_devices[id]) {
		LOGM("Invalid VND id [%d]", id);
		return RMNET_CONFIG_NO_SUCH_DEVICE;
	}

	dev = rmnet_devices[id];
	dev_conf = (struct rmnet_vnd_private_s *) netdev_priv(dev);

	if (!dev_conf)
		BUG();

	write_lock_irqsave(&dev_conf->flow_map_lock, flags);
	itm = _rmnet_vnd_get_flow_map(dev_conf, map_flow);
	if (itm) {
		r = _rmnet_vnd_update_flow_map(RMNET_VND_UF_ACTION_ADD,
					       itm, tc_flow);
		if (r != RMNET_VND_UPDATE_FLOW_OK) {
			write_unlock_irqrestore(&dev_conf->flow_map_lock,
						flags);
			return RMNET_CONFIG_TC_HANDLE_FULL;
		}
		write_unlock_irqrestore(&dev_conf->flow_map_lock, flags);
		return RMNET_CONFIG_OK;
	}
	write_unlock_irqrestore(&dev_conf->flow_map_lock, flags);

	itm = (struct rmnet_map_flow_mapping_s *)
		kmalloc(sizeof(struct rmnet_map_flow_mapping_s), GFP_KERNEL);

	if (!itm) {
		LOGM("%s", "Failure allocating flow mapping");
		return RMNET_CONFIG_NOMEM;
	}
	memset(itm, 0, sizeof(struct rmnet_map_flow_mapping_s));

	itm->map_flow_id = map_flow;
	itm->tc_flow_valid[0] = 1;
	itm->tc_flow_id[0] = tc_flow;

	/* How can we dynamically init these safely? Kernel only provides static
	 * initializers for atomic_t
	 */
	itm->v4_seq.counter =  0; /* Init is broken: ATOMIC_INIT(0); */
	itm->v6_seq.counter =  0; /* Init is broken: ATOMIC_INIT(0); */

	write_lock_irqsave(&dev_conf->flow_map_lock, flags);
	list_add(&(itm->list), &(dev_conf->flow_head));
	write_unlock_irqrestore(&dev_conf->flow_map_lock, flags);

	LOGD("Created flow mapping [%s][0x%08X][0x%08X]@%p",
	     dev->name, itm->map_flow_id, itm->tc_flow_id[0], itm);

	return RMNET_CONFIG_OK;
}

/**
 * rmnet_vnd_del_tc_flow() - Delete a MAP/TC flow handle mapping
 * @id: Virtual network device ID
 * @map_flow: MAP flow handle
 * @tc_flow: TC flow handle
 *
 * Checkes for an existing flow mapping object corresponding to map_flow. If one
 * is found, then it will try to remove the existing tc_flow mapping. If the
 * mapping object no longer contains any mappings, then it is freed. Otherwise
 * the mapping object is left in the list
 *
 * Return:
 *      - RMNET_CONFIG_OK if successful or if there was no such tc_flow
 *      - RMNET_CONFIG_INVALID_REQUEST if there is no such map_flow
 */
int rmnet_vnd_del_tc_flow(uint32_t id, uint32_t map_flow, uint32_t tc_flow)
{
	struct rmnet_vnd_private_s *dev_conf;
	struct net_device *dev;
	struct rmnet_map_flow_mapping_s *itm;
	int r;
	unsigned long flags;
	int rc = RMNET_CONFIG_OK;

	if ((id < 0) || (id >= RMNET_DATA_MAX_VND) || !rmnet_devices[id]) {
		LOGM("Invalid VND id [%d]", id);
		return RMNET_CONFIG_NO_SUCH_DEVICE;
	}

	dev = rmnet_devices[id];
	dev_conf = (struct rmnet_vnd_private_s *) netdev_priv(dev);

	if (!dev_conf)
		BUG();

	r = RMNET_VND_UPDATE_FLOW_NO_ACTION;
	write_lock_irqsave(&dev_conf->flow_map_lock, flags);
	itm = _rmnet_vnd_get_flow_map(dev_conf, map_flow);
	if (!itm) {
		rc = RMNET_CONFIG_INVALID_REQUEST;
	} else {
		r = _rmnet_vnd_update_flow_map(RMNET_VND_UF_ACTION_DEL,
					       itm, tc_flow);
		if (r ==  RMNET_VND_UPDATE_FLOW_NO_VALID_LEFT)
			list_del(&(itm->list));
	}
	write_unlock_irqrestore(&dev_conf->flow_map_lock, flags);

	if (r ==  RMNET_VND_UPDATE_FLOW_NO_VALID_LEFT) {
		if (itm)
			LOGD("Removed flow mapping [%s][0x%08X]@%p",
			     dev->name, itm->map_flow_id, itm);
		kfree(itm);
	}

	return rc;
}

/**
 * rmnet_vnd_do_flow_control() - Process flow control request
 * @dev: Virtual network device node to do lookup on
 * @map_flow_id: Flow ID from MAP message
 * @v4_seq: pointer to IPv4 indication sequence number
 * @v6_seq: pointer to IPv6 indication sequence number
 * @enable: boolean to enable/disable flow.
 *
 * Return:
 *      - 0 if successful
 *      - 1 if no mapping is found
 *      - 2 if dev is not RmNet virtual network device node
 */
int rmnet_vnd_do_flow_control(struct net_device *dev,
			       uint32_t map_flow_id,
			       uint16_t v4_seq,
			       uint16_t v6_seq,
			       int enable)
{
	struct rmnet_vnd_private_s *dev_conf;
	struct rmnet_map_flow_mapping_s *itm;
	int do_fc, error, i;
	error = 0;
	do_fc = 0;

	if (unlikely(!dev))
		BUG();

	if (!rmnet_vnd_is_vnd(dev)) {
		return 2;
	} else {
		dev_conf = (struct rmnet_vnd_private_s *) netdev_priv(dev);
	}

	if (unlikely(!dev_conf))
		BUG();

	read_lock(&dev_conf->flow_map_lock);
	itm = _rmnet_vnd_get_flow_map(dev_conf, map_flow_id);

	if (!itm) {
		LOGL("Got flow control request for unknown flow %08X",
		     map_flow_id);
		goto fcdone;
	}
	if (v4_seq == 0 || v4_seq >= atomic_read(&(itm->v4_seq))) {
		atomic_set(&(itm->v4_seq), v4_seq);
		for (i = 0; i < RMNET_MAP_FLOW_NUM_TC_HANDLE; i++) {
			if (itm->tc_flow_valid[i] == 1) {
				LOGD("Found [%s][0x%08X][%d:0x%08X]",
				     dev->name, itm->map_flow_id, i,
				     itm->tc_flow_id[i]);

				_rmnet_vnd_do_flow_control(dev,
							   itm->tc_flow_id[i],
							   enable);
			}
		}
	} else {
		LOGD("Internal seq(%hd) higher than called(%hd)",
		     atomic_read(&(itm->v4_seq)), v4_seq);
	}

fcdone:
	read_unlock(&dev_conf->flow_map_lock);

	return error;
}

/**
 * rmnet_vnd_get_by_id() - Get VND by array index ID
 * @id: Virtual network deice id [0:RMNET_DATA_MAX_VND]
 *
 * Return:
 *      - 0 if no device or ID out of range
 *      - otherwise return pointer to VND net_device struct
 */
struct net_device *rmnet_vnd_get_by_id(int id)
{
	if (id < 0 || id >= RMNET_DATA_MAX_VND) {
		pr_err("Bug; VND ID out of bounds");
		BUG();
		return 0;
	}
	return rmnet_devices[id];
}
