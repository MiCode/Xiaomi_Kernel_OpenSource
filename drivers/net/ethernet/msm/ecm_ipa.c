/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/etherdevice.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <mach/ecm_ipa.h>

#define DRIVER_NAME "ecm_ipa"
#define DRIVER_VERSION "12-Mar-2013"
#define ECM_IPA_IPV4_HDR_NAME "ecm_eth_ipv4"
#define ECM_IPA_IPV6_HDR_NAME "ecm_eth_ipv6"
#define IPA_TO_USB_CLIENT	IPA_CLIENT_USB_CONS
#define INACTIVITY_MSEC_DELAY 100
#define ECM_IPA_ERROR(fmt, args...) \
	pr_err(DRIVER_NAME "@%s@%d@ctx:%s: "\
			fmt, __func__, __LINE__, current->comm, ## args)
#ifdef ECM_IPA_DEBUG_ON
#define ECM_IPA_DEBUG(fmt, args...) \
	pr_err(DRIVER_NAME "@%s@%d@ctx:%s: "\
			fmt, __func__, __LINE__, current->comm, ## args)
#else /* ECM_IPA_DEBUG_ON */
#define ECM_IPA_DEBUG(fmt, args...)
#endif /* ECM_IPA_DEBUG_ON */

#define NULL_CHECK(ptr) \
	do { \
		if (!(ptr)) { \
			ECM_IPA_ERROR("null pointer #ptr\n"); \
			return -EINVAL; \
		} \
	} \
	while (0)

#define ECM_IPA_LOG_ENTRY() ECM_IPA_DEBUG("begin\n")
#define ECM_IPA_LOG_EXIT() ECM_IPA_DEBUG("end\n")

/**
 * struct ecm_ipa_dev - main driver context parameters
 * @ack_spinlock: protect last sent skb
 * @last_out_skb: last sent skb saved until Tx notify is received from IPA
 * @net: network interface struct implemented by this driver
 * @folder: debugfs folder for various debuging switches
 * @tx_enable: flag that enable/disable Tx path to continue to IPA
 * @rx_enable: flag that enable/disable Rx path to continue to IPA
 * @rm_enable: flag that enable/disable Resource manager request prior to Tx
 * @dma_enable: flag that allow on-the-fly DMA mode for IPA
 * @tx_file: saved debugfs entry to allow cleanup
 * @rx_file: saved debugfs entry to allow cleanup
 * @rm_file: saved debugfs entry to allow cleanup
 * @dma_file: saved debugfs entry to allow cleanup
 * @eth_ipv4_hdr_hdl: saved handle for ipv4 header-insertion table
 * @eth_ipv6_hdr_hdl: saved handle for ipv6 header-insertion table
 * @usb_to_ipa_hdl: save handle for IPA pipe operations
 * @ipa_to_usb_hdl: save handle for IPA pipe operations
 */
struct ecm_ipa_dev {
	spinlock_t ack_spinlock;
	struct sk_buff *last_out_skb;
	struct net_device *net;
	bool tx_enable;
	bool rx_enable;
	bool rm_enable;
	bool dma_enable;
	struct dentry *folder;
	struct dentry *tx_file;
	struct dentry *rx_file;
	struct dentry *rm_file;
	struct dentry *dma_file;
	uint32_t eth_ipv4_hdr_hdl;
	uint32_t eth_ipv6_hdr_hdl;
	u32 usb_to_ipa_hdl;
	u32 ipa_to_usb_hdl;
};

/**
 * struct ecm_ipa_ctx - saved pointer for the std ecm network device
 *                which allow ecm_ipa to be a singleton
 */
static struct ecm_ipa_dev *ecm_ipa_ctx;

static int ecm_ipa_ep_registers_cfg(u32 usb_to_ipa_hdl, u32 ipa_to_usb_hdl);
static int ecm_ipa_set_device_ethernet_addr(
	u8 *dev_ethaddr, u8 device_ethaddr[]);
static void ecm_ipa_packet_receive_notify(void *priv,
		enum ipa_dp_evt_type evt,
		unsigned long data);
static void ecm_ipa_tx_complete_notify(void *priv,
		enum ipa_dp_evt_type evt,
		unsigned long data);
static int ecm_ipa_ep_registers_dma_cfg(u32 usb_to_ipa_hdl);
static int ecm_ipa_open(struct net_device *net);
static int ecm_ipa_stop(struct net_device *net);
static netdev_tx_t ecm_ipa_start_xmit(struct sk_buff *skb,
					struct net_device *net);
static void ecm_ipa_rm_notify(void *user_data, enum ipa_rm_event event,
		unsigned long data);
static int ecm_ipa_create_rm_resource(struct ecm_ipa_dev *dev);
static void ecm_ipa_destory_rm_resource(void);
static bool rx_filter(struct sk_buff *skb);
static bool tx_filter(struct sk_buff *skb);
static bool rm_enabled(struct ecm_ipa_dev *dev);

static int ecm_ipa_rules_cfg(struct ecm_ipa_dev *dev,
		const void *dst_mac, const void *src_mac);
static int ecm_ipa_register_tx(struct ecm_ipa_dev *dev);
static void ecm_ipa_deregister_tx(struct ecm_ipa_dev *dev);
static int ecm_ipa_debugfs_init(struct ecm_ipa_dev *dev);
static void ecm_ipa_debugfs_destroy(struct ecm_ipa_dev *dev);
static int ecm_ipa_debugfs_tx_open(struct inode *inode, struct file *file);
static int ecm_ipa_debugfs_rx_open(struct inode *inode, struct file *file);
static int ecm_ipa_debugfs_rm_open(struct inode *inode, struct file *file);
static int ecm_ipa_debugfs_dma_open(struct inode *inode, struct file *file);
static ssize_t ecm_ipa_debugfs_enable_read(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos);
static ssize_t ecm_ipa_debugfs_enable_write(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos);
static ssize_t ecm_ipa_debugfs_enable_write_dma(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos);
static void eth_get_drvinfo(struct net_device *net,
		struct ethtool_drvinfo *drv_info);

static const struct net_device_ops ecm_ipa_netdev_ops = {
	.ndo_open		= ecm_ipa_open,
	.ndo_stop		= ecm_ipa_stop,
	.ndo_start_xmit = ecm_ipa_start_xmit,
	.ndo_set_mac_address = eth_mac_addr,
};
static const struct ethtool_ops ops = {
	.get_drvinfo = eth_get_drvinfo,
	.get_link = ethtool_op_get_link,
};
const struct file_operations ecm_ipa_debugfs_tx_ops = {
	.open = ecm_ipa_debugfs_tx_open,
	.read = ecm_ipa_debugfs_enable_read,
	.write = ecm_ipa_debugfs_enable_write,
};
const struct file_operations ecm_ipa_debugfs_rx_ops = {
	.open = ecm_ipa_debugfs_rx_open,
	.read = ecm_ipa_debugfs_enable_read,
	.write = ecm_ipa_debugfs_enable_write,
};
const struct file_operations ecm_ipa_debugfs_rm_ops = {
	.open = ecm_ipa_debugfs_rm_open,
	.read = ecm_ipa_debugfs_enable_read,
	.write = ecm_ipa_debugfs_enable_write,
};
const struct file_operations ecm_ipa_debugfs_dma_ops = {
	.open = ecm_ipa_debugfs_dma_open,
	.read = ecm_ipa_debugfs_enable_read,
	.write = ecm_ipa_debugfs_enable_write_dma,
};

/**
 * ecm_ipa_init() - initializes internal data structures
 * @ecm_ipa_rx_dp_notify: supplied callback to be called by the IPA
 * driver upon data packets received from USB pipe into IPA core.
 * @ecm_ipa_rt_dp_notify: supplied callback to be called by the IPA
 * driver upon exception packets sent from IPA pipe into USB core.
 * @priv: should be passed later on to ecm_ipa_configure, hold the network
 * structure allocated for STD ECM interface.
 *
 * Shall be called prior to pipe connection.
 * The out parameters (the callbacks) shall be supplied to ipa_connect.
 * Detailed description:
 *  - set the callbacks to be used by the caller upon ipa_connect
 *  - allocate the network device
 *  - set the priv argument with a reference to the network device
 *
 * Returns negative errno, or zero on success
 */
int ecm_ipa_init(ecm_ipa_callback *ecm_ipa_rx_dp_notify,
		ecm_ipa_callback *ecm_ipa_tx_dp_notify,
		void **priv)
{
	int ret = 0;
	struct net_device *net;
	struct ecm_ipa_dev *dev;
	ECM_IPA_LOG_ENTRY();
	ECM_IPA_DEBUG("%s version %s\n", DRIVER_NAME, DRIVER_VERSION);
	NULL_CHECK(ecm_ipa_rx_dp_notify);
	NULL_CHECK(ecm_ipa_tx_dp_notify);
	NULL_CHECK(priv);
	net = alloc_etherdev(sizeof(struct ecm_ipa_dev));
	if (!net) {
		ret = -ENOMEM;
		ECM_IPA_ERROR("fail to allocate etherdev\n");
		goto fail_alloc_etherdev;
	}
	ECM_IPA_DEBUG("etherdev was successfully allocated\n");
	dev = netdev_priv(net);
	memset(dev, 0, sizeof(*dev));
	dev->tx_enable = true;
	dev->rx_enable = true;
	spin_lock_init(&dev->ack_spinlock);
	dev->net = net;
	ecm_ipa_ctx = dev;
	*priv = (void *)dev;
	snprintf(net->name, sizeof(net->name), "%s%%d", "ecm");
	net->netdev_ops = &ecm_ipa_netdev_ops;
	ECM_IPA_DEBUG("internal data structures were intialized\n");
	ret = ecm_ipa_debugfs_init(dev);
	if (ret)
		goto fail_debugfs;
	ECM_IPA_DEBUG("debugfs entries were created\n");
	*ecm_ipa_rx_dp_notify = ecm_ipa_packet_receive_notify;
	*ecm_ipa_tx_dp_notify = ecm_ipa_tx_complete_notify;
	ECM_IPA_LOG_EXIT();
	return 0;
fail_debugfs:
	free_netdev(net);
fail_alloc_etherdev:
	return ret;
}
EXPORT_SYMBOL(ecm_ipa_init);

/**
 * ecm_ipa_rules_cfg() - set header insertion and register Tx/Rx properties
 *				Headers will be commited to HW
 * @dev: main driver context parameters
 * @dst_mac: destination MAC address
 * @src_mac: source MAC address
 *
 * Returns negative errno, or zero on success
 */
static int ecm_ipa_rules_cfg(struct ecm_ipa_dev *dev,
		const void *dst_mac, const void *src_mac)
{
	struct ipa_ioc_add_hdr *hdrs;
	struct ipa_hdr_add *ipv4_hdr;
	struct ipa_hdr_add *ipv6_hdr;
	struct ethhdr *eth_ipv4;
	struct ethhdr *eth_ipv6;
	int result = 0;

	ECM_IPA_LOG_ENTRY();
	hdrs = kzalloc(sizeof(*hdrs) + sizeof(*ipv4_hdr) + sizeof(*ipv6_hdr),
			GFP_KERNEL);
	if (!hdrs) {
		result = -ENOMEM;
		goto out;
	}
	ipv4_hdr = &hdrs->hdr[0];
	eth_ipv4 = (struct ethhdr *)ipv4_hdr->hdr;
	ipv6_hdr = &hdrs->hdr[1];
	eth_ipv6 = (struct ethhdr *)ipv6_hdr->hdr;
	strlcpy(ipv4_hdr->name, ECM_IPA_IPV4_HDR_NAME, IPA_RESOURCE_NAME_MAX);
	memcpy(eth_ipv4->h_dest, dst_mac, ETH_ALEN);
	memcpy(eth_ipv4->h_source, src_mac, ETH_ALEN);
	eth_ipv4->h_proto = ETH_P_IP;
	ipv4_hdr->hdr_len = ETH_HLEN;
	ipv4_hdr->is_partial = 0;
	strlcpy(ipv6_hdr->name, ECM_IPA_IPV6_HDR_NAME, IPA_RESOURCE_NAME_MAX);
	memcpy(eth_ipv6->h_dest, dst_mac, ETH_ALEN);
	memcpy(eth_ipv6->h_source, src_mac, ETH_ALEN);
	eth_ipv6->h_proto = ETH_P_IPV6;
	ipv6_hdr->hdr_len = ETH_HLEN;
	ipv6_hdr->is_partial = 0;
	hdrs->commit = 1;
	hdrs->num_hdrs = 2;
	result = ipa_add_hdr(hdrs);
	if (result) {
		ECM_IPA_ERROR("Fail on Header-Insertion(%d)\n", result);
		goto out_free_mem;
	}
	if (ipv4_hdr->status) {
		ECM_IPA_ERROR("Fail on Header-Insertion ipv4(%d)\n",
				ipv4_hdr->status);
		result = ipv4_hdr->status;
		goto out_free_mem;
	}
	if (ipv6_hdr->status) {
		ECM_IPA_ERROR("Fail on Header-Insertion ipv6(%d)\n",
				ipv6_hdr->status);
		result = ipv6_hdr->status;
		goto out_free_mem;
	}
	dev->eth_ipv4_hdr_hdl = ipv4_hdr->hdr_hdl;
	dev->eth_ipv6_hdr_hdl = ipv6_hdr->hdr_hdl;
	ECM_IPA_LOG_EXIT();
out_free_mem:
	kfree(hdrs);
out:
	return result;
}

static void ecm_ipa_rules_destroy(struct ecm_ipa_dev *dev)
{
	struct ipa_ioc_del_hdr *del_hdr;
	struct ipa_hdr_del *ipv4;
	struct ipa_hdr_del *ipv6;
	int result;
	del_hdr = kzalloc(sizeof(*del_hdr) + sizeof(*ipv4) +
			sizeof(*ipv6), GFP_KERNEL);
	if (!del_hdr)
		return;
	del_hdr->commit = 1;
	del_hdr->num_hdls = 2;
	ipv4 = &del_hdr->hdl[0];
	ipv4->hdl = dev->eth_ipv4_hdr_hdl;
	ipv6 = &del_hdr->hdl[1];
	ipv6->hdl = dev->eth_ipv6_hdr_hdl;
	result = ipa_del_hdr(del_hdr);
	if (result || ipv4->status || ipv6->status)
		ECM_IPA_ERROR("ipa_del_hdr failed");
}

static int ecm_ipa_register_tx(struct ecm_ipa_dev *dev)
{
	struct ipa_tx_intf tx_properties = {0};
	struct ipa_ioc_tx_intf_prop properties[2] = { {0}, {0} };
	struct ipa_ioc_tx_intf_prop *ipv4_property;
	struct ipa_ioc_tx_intf_prop *ipv6_property;
	int result = 0;
	ECM_IPA_LOG_ENTRY();
	tx_properties.prop = properties;
	ipv4_property = &tx_properties.prop[0];
	ipv4_property->ip = IPA_IP_v4;
	ipv4_property->dst_pipe = IPA_TO_USB_CLIENT;
	strlcpy(ipv4_property->hdr_name, ECM_IPA_IPV4_HDR_NAME,
			IPA_RESOURCE_NAME_MAX);
	ipv6_property = &tx_properties.prop[1];
	ipv6_property->ip = IPA_IP_v6;
	ipv6_property->dst_pipe = IPA_TO_USB_CLIENT;
	strlcpy(ipv6_property->hdr_name, ECM_IPA_IPV6_HDR_NAME,
			IPA_RESOURCE_NAME_MAX);
	tx_properties.num_props = 2;
	result = ipa_register_intf("ecm0", &tx_properties, NULL);
	if (result)
		ECM_IPA_ERROR("fail on Tx_prop registration\n");
	ECM_IPA_LOG_EXIT();
	return result;
}

static void ecm_ipa_deregister_tx(struct ecm_ipa_dev *dev)
{
	int result;
	ECM_IPA_LOG_ENTRY();
	result = ipa_deregister_intf(dev->net->name);
	if (result)
		ECM_IPA_DEBUG("Fail on Tx prop deregister\n");
	ECM_IPA_LOG_EXIT();
	return;
}

/**
 * ecm_ipa_configure() - make IPA core end-point specific configuration
 * @usb_to_ipa_hdl: handle of usb_to_ipa end-point for IPA driver
 * @ipa_to_usb_hdl: handle of ipa_to_usb end-point for IPA driver
 * @host_ethaddr: host Ethernet address in network order
 * @device_ethaddr: device Ethernet address in network order
 *
 * Configure the usb_to_ipa and ipa_to_usb end-point registers
 * - USB->IPA end-point: disable de-aggregation, enable link layer
 *   header removal (Ethernet removal), source NATing and default routing.
 * - IPA->USB end-point: disable aggregation, add link layer header (Ethernet)
 * - allocate Ethernet device
 * - register to Linux network stack
 *
 * Returns negative errno, or zero on success
 */
int ecm_ipa_configure(u8 host_ethaddr[], u8 device_ethaddr[],
		void *priv)
{
	struct ecm_ipa_dev *dev = priv;
	struct net_device *net;
	int result;
	ECM_IPA_LOG_ENTRY();
	NULL_CHECK(host_ethaddr);
	NULL_CHECK(host_ethaddr);
	NULL_CHECK(dev);
	net = dev->net;
	NULL_CHECK(net);
	ECM_IPA_DEBUG("host_ethaddr=%pM device_ethaddr=%pM\n",
					host_ethaddr, device_ethaddr);
	result = ecm_ipa_create_rm_resource(dev);
	if (result) {
		ECM_IPA_ERROR("fail on RM create\n");
		return -EINVAL;
	}
	ECM_IPA_DEBUG("RM resource was created\n");
	netif_carrier_off(dev->net);
	result = ecm_ipa_set_device_ethernet_addr(net->dev_addr,
			device_ethaddr);
	if (result) {
		ECM_IPA_ERROR("set device MAC failed\n");
		goto fail_set_device_ethernet;
	}
	result = ecm_ipa_rules_cfg(dev, host_ethaddr, device_ethaddr);
	if (result) {
		ECM_IPA_ERROR("fail on ipa rules set\n");
		goto fail_set_device_ethernet;
	}
	ECM_IPA_DEBUG("Ethernet header insertion was set\n");
	result = ecm_ipa_register_tx(dev);
	if (result) {
		ECM_IPA_ERROR("fail on properties set\n");
		goto fail_register_tx;
	}
	ECM_IPA_DEBUG("ECM Tx properties were registered\n");
	result = register_netdev(net);
	if (result) {
		ECM_IPA_ERROR("register_netdev failed: %d\n", result);
		goto fail_register_netdev;
	}
	ECM_IPA_DEBUG("register_netdev succeeded\n");
	ECM_IPA_LOG_EXIT();
	return 0;
fail_register_netdev:
	ecm_ipa_deregister_tx(dev);
fail_register_tx:
fail_set_device_ethernet:
	ecm_ipa_rules_destroy(dev);
	ecm_ipa_destory_rm_resource();
	free_netdev(net);
	return result;
}
EXPORT_SYMBOL(ecm_ipa_configure);

int ecm_ipa_connect(u32 usb_to_ipa_hdl, u32 ipa_to_usb_hdl,
		void *priv)
{
	struct ecm_ipa_dev *dev = priv;
	ECM_IPA_LOG_ENTRY();
	NULL_CHECK(priv);
	ECM_IPA_DEBUG("usb_to_ipa_hdl = %d, ipa_to_usb_hdl = %d\n",
					usb_to_ipa_hdl, ipa_to_usb_hdl);
	if (!usb_to_ipa_hdl || usb_to_ipa_hdl >= IPA_CLIENT_MAX) {
		ECM_IPA_ERROR("usb_to_ipa_hdl(%d) is not a valid ipa handle\n",
				usb_to_ipa_hdl);
		return -EINVAL;
	}
	if (!ipa_to_usb_hdl || ipa_to_usb_hdl >= IPA_CLIENT_MAX) {
		ECM_IPA_ERROR("ipa_to_usb_hdl(%d) is not a valid ipa handle\n",
				ipa_to_usb_hdl);
		return -EINVAL;
	}
	dev->ipa_to_usb_hdl = ipa_to_usb_hdl;
	dev->usb_to_ipa_hdl = usb_to_ipa_hdl;
	ecm_ipa_ep_registers_cfg(usb_to_ipa_hdl, ipa_to_usb_hdl);
	netif_carrier_on(dev->net);
	if (!netif_carrier_ok(dev->net)) {
		ECM_IPA_ERROR("netif_carrier_ok error\n");
		return -EBUSY;
	}
	ECM_IPA_LOG_EXIT();
	return 0;
}
EXPORT_SYMBOL(ecm_ipa_connect);

int ecm_ipa_disconnect(void *priv)
{
	struct ecm_ipa_dev *dev = priv;
	ECM_IPA_LOG_ENTRY();
	NULL_CHECK(dev);
	netif_carrier_off(dev->net);
	ECM_IPA_LOG_EXIT();
	return 0;
}
EXPORT_SYMBOL(ecm_ipa_disconnect);


static void ecm_ipa_rm_notify(void *user_data, enum ipa_rm_event event,
		unsigned long data)
{
	struct ecm_ipa_dev *dev = user_data;
	ECM_IPA_LOG_ENTRY();
	if (event == IPA_RM_RESOURCE_GRANTED &&
			netif_queue_stopped(dev->net)) {
		ECM_IPA_DEBUG("Resource Granted - waking queue\n");
		netif_wake_queue(dev->net);
	} else {
		ECM_IPA_DEBUG("Resource released\n");
	}
	ECM_IPA_LOG_EXIT();
}

static int ecm_ipa_create_rm_resource(struct ecm_ipa_dev *dev)
{
	struct ipa_rm_create_params create_params = {0};
	int result;
	ECM_IPA_LOG_ENTRY();
	create_params.name = IPA_RM_RESOURCE_STD_ECM_PROD;
	create_params.reg_params.user_data = dev;
	create_params.reg_params.notify_cb = ecm_ipa_rm_notify;
	result = ipa_rm_create_resource(&create_params);
	if (result) {
		ECM_IPA_ERROR("Fail on ipa_rm_create_resource\n");
		goto fail_rm_create;
	}
	ECM_IPA_DEBUG("rm client was created");

	result = ipa_rm_inactivity_timer_init(IPA_RM_RESOURCE_STD_ECM_PROD,
			INACTIVITY_MSEC_DELAY);
	if (result) {
		ECM_IPA_ERROR("Fail on ipa_rm_inactivity_timer_init\n");
		goto fail_it;
	}
	ECM_IPA_DEBUG("rm_it client was created");

	result = ipa_rm_add_dependency(IPA_RM_RESOURCE_STD_ECM_PROD,
				IPA_RM_RESOURCE_USB_CONS);
	if (result)
		ECM_IPA_ERROR("unable to add dependency (%d)\n", result);

	ECM_IPA_DEBUG("rm dependency was set\n");

	ECM_IPA_LOG_EXIT();
	return 0;

fail_it:
fail_rm_create:
	return result;
}

static void ecm_ipa_destory_rm_resource(void)
{
	ECM_IPA_LOG_ENTRY();

	ipa_rm_delete_dependency(IPA_RM_RESOURCE_STD_ECM_PROD,
			IPA_RM_RESOURCE_USB_CONS);
	ipa_rm_inactivity_timer_destroy(IPA_RM_RESOURCE_STD_ECM_PROD);

	ECM_IPA_LOG_EXIT();
}

static bool rx_filter(struct sk_buff *skb)
{
	struct ecm_ipa_dev *dev = netdev_priv(skb->dev);
	return !dev->rx_enable;
}

static bool tx_filter(struct sk_buff *skb)
{
	struct ecm_ipa_dev *dev = netdev_priv(skb->dev);
	return !dev->tx_enable;
}

static bool rm_enabled(struct ecm_ipa_dev *dev)
{
	return dev->rm_enable;
}

static int ecm_ipa_open(struct net_device *net)
{
	ECM_IPA_LOG_ENTRY();
	netif_start_queue(net);
	ECM_IPA_LOG_EXIT();
	return 0;
}

static int ecm_ipa_stop(struct net_device *net)
{
	ECM_IPA_LOG_ENTRY();
	ECM_IPA_DEBUG("stopping net device\n");
	netif_stop_queue(net);
	ECM_IPA_LOG_EXIT();
	return 0;
}

/**
 * ecm_ipa_cleanup() - destroys all
 * ecm information
 * @priv: main driver context parameters
 *
 */
void ecm_ipa_cleanup(void *priv)
{
	struct ecm_ipa_dev *dev = priv;
	ECM_IPA_LOG_ENTRY();
	if (!dev) {
		ECM_IPA_ERROR("dev NULL pointer\n");
		return;
	}
	if (rm_enabled(dev)) {
		ecm_ipa_destory_rm_resource();
		ecm_ipa_debugfs_destroy(dev);
	}
	if (!dev->net) {
		unregister_netdev(dev->net);
		free_netdev(dev->net);
	}
	ECM_IPA_DEBUG("cleanup done\n");
	ecm_ipa_ctx = NULL;
	ECM_IPA_LOG_EXIT();
	return ;
}
EXPORT_SYMBOL(ecm_ipa_cleanup);

static int resource_request(struct ecm_ipa_dev *dev)
{
	int result = 0;

	if (!rm_enabled(dev))
		goto out;
	result = ipa_rm_inactivity_timer_request_resource(
			IPA_RM_RESOURCE_STD_ECM_PROD);
out:
	return result;
}

static void resource_release(struct ecm_ipa_dev *dev)
{
	if (!rm_enabled(dev))
		goto out;
	ipa_rm_inactivity_timer_release_resource(IPA_RM_RESOURCE_STD_ECM_PROD);
out:
	return;
}

/**
 * ecm_ipa_start_xmit() - send data from APPs to USB core via IPA core
 * @skb: packet received from Linux stack
 * @net: the network device being used to send this packet
 *
 * Several conditions needed in order to send the packet to IPA:
 * - we are in a valid state were the queue is not stopped
 * - Filter Tx switch is turned off
 * - The resources required for actual Tx are all up
 *
 */
static netdev_tx_t ecm_ipa_start_xmit(struct sk_buff *skb,
					struct net_device *net)
{
	int ret;
	netdev_tx_t status = NETDEV_TX_BUSY;
	struct ecm_ipa_dev *dev = netdev_priv(net);
	unsigned long flags;

	if (unlikely(netif_queue_stopped(net))) {
		ECM_IPA_ERROR("interface queue is stopped\n");
		goto out;
	}

	if (unlikely(tx_filter(skb))) {
		dev_kfree_skb_any(skb);
		ECM_IPA_DEBUG("packet got filtered out on Tx path\n");
		status = NETDEV_TX_OK;
		goto out;
	}
	ret = resource_request(dev);
	if (ret) {
		ECM_IPA_DEBUG("Waiting to resource\n");
		netif_stop_queue(net);
		goto resource_busy;
	}

	spin_lock_irqsave(&dev->ack_spinlock, flags);
	if (dev->last_out_skb) {
		ECM_IPA_DEBUG("No Tx-ack received for previous packet\n");
		spin_unlock_irqrestore(&dev->ack_spinlock, flags);
		netif_stop_queue(net);
		status = -NETDEV_TX_BUSY;
		goto out;
	} else {
		dev->last_out_skb = skb;
	}
	spin_unlock_irqrestore(&dev->ack_spinlock, flags);

	ret = ipa_tx_dp(IPA_TO_USB_CLIENT, skb, NULL);
	if (ret) {
		ECM_IPA_ERROR("ipa transmit failed (%d)\n", ret);
		goto fail_tx_packet;
	}
	net->stats.tx_packets++;
	net->stats.tx_bytes += skb->len;
	status = NETDEV_TX_OK;
	goto out;

fail_tx_packet:
out:
	resource_release(dev);
resource_busy:
	return status;
}

/**
 * ecm_ipa_packet_receive_notify() - Rx notify
 *
 * @priv: ecm driver context
 * @evt: event type
 * @data: data provided with event
 *
 * IPA will pass a packet with skb->data pointing to Ethernet packet frame
 */
void ecm_ipa_packet_receive_notify(void *priv,
		enum ipa_dp_evt_type evt,
		unsigned long data)
{
	struct sk_buff *skb = (struct sk_buff *)data;
	struct ecm_ipa_dev *dev = priv;
	int result;

	if (evt != IPA_RECEIVE)	{
		ECM_IPA_ERROR("A none IPA_RECEIVE event in ecm_ipa_receive\n");
		return;
	}

	skb->dev = dev->net;
	skb->protocol = eth_type_trans(skb, dev->net);
	if (rx_filter(skb)) {
		ECM_IPA_DEBUG("packet got filtered out on Rx path\n");
		dev_kfree_skb_any(skb);
		return;
	}

	result = netif_rx(skb);
	if (result)
		ECM_IPA_ERROR("fail on netif_rx\n");
	dev->net->stats.rx_packets++;
	dev->net->stats.rx_bytes += skb->len;

	return;
}

/**
 * ecm_ipa_tx_complete_notify() - Rx notify
 *
 * @priv: ecm driver context
 * @evt: event type
 * @data: data provided with event
 *
 * Check that the packet is the one we sent and release it
 * This function will be called in defered context in IPA wq.
 */
void ecm_ipa_tx_complete_notify(void *priv,
		enum ipa_dp_evt_type evt,
		unsigned long data)
{
	struct sk_buff *skb = (struct sk_buff *)data;
	struct ecm_ipa_dev *dev = priv;
	unsigned long flags;

	if (!dev) {
		ECM_IPA_ERROR("dev is NULL pointer\n");
		return;
	}
	if (evt != IPA_WRITE_DONE) {
		ECM_IPA_ERROR("unsupported event on Tx callback\n");
		return;
	}
	spin_lock_irqsave(&dev->ack_spinlock, flags);
	if (skb != dev->last_out_skb)
		ECM_IPA_ERROR("ACKed/Sent not the same(FIFO expected)\n");
	dev->last_out_skb = NULL;
	spin_unlock_irqrestore(&dev->ack_spinlock, flags);
	if (netif_queue_stopped(dev->net)) {
		ECM_IPA_DEBUG("waking up queue\n");
		netif_wake_queue(dev->net);
	}
	dev_kfree_skb_any(skb);
	return;
}

static int ecm_ipa_debugfs_tx_open(struct inode *inode, struct file *file)
{
	struct ecm_ipa_dev *dev = inode->i_private;
	ECM_IPA_LOG_ENTRY();
	file->private_data = &(dev->tx_enable);
	ECM_IPA_LOG_ENTRY();
	return 0;
}

static int ecm_ipa_debugfs_rx_open(struct inode *inode, struct file *file)
{
	struct ecm_ipa_dev *dev = inode->i_private;
	ECM_IPA_LOG_ENTRY();
	file->private_data = &(dev->rx_enable);
	ECM_IPA_LOG_EXIT();
	return 0;
}

static int ecm_ipa_debugfs_rm_open(struct inode *inode, struct file *file)
{
	struct ecm_ipa_dev *dev = inode->i_private;
	ECM_IPA_LOG_ENTRY();
	file->private_data = &(dev->rm_enable);
	ECM_IPA_LOG_EXIT();
	return 0;
}

static ssize_t ecm_ipa_debugfs_enable_write_dma(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos)
{
	struct ecm_ipa_dev *dev = file->private_data;
	int result;
	ECM_IPA_LOG_ENTRY();
	file->private_data = &dev->dma_enable;
	result = ecm_ipa_debugfs_enable_write(file, buf, count, ppos);
	if (dev->dma_enable)
		ecm_ipa_ep_registers_dma_cfg(dev->usb_to_ipa_hdl);
	else
		ecm_ipa_ep_registers_cfg(dev->usb_to_ipa_hdl,
				dev->usb_to_ipa_hdl);
	ECM_IPA_LOG_EXIT();
	return result;
}

static int ecm_ipa_debugfs_dma_open(struct inode *inode, struct file *file)
{
	struct ecm_ipa_dev *dev = inode->i_private;
	ECM_IPA_LOG_ENTRY();
	file->private_data = dev;
	ECM_IPA_LOG_EXIT();
	return 0;
}

static ssize_t ecm_ipa_debugfs_enable_write(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos)
{
	unsigned long missing;
	char input;
	bool *enable = file->private_data;
	if (count != sizeof(input) + 1) {
		ECM_IPA_ERROR("wrong input length(%zd)\n", count);
		return -EINVAL;
	}
	if (!buf) {
		ECM_IPA_ERROR("Bad argument\n");
		return -EINVAL;
	}
	missing = copy_from_user(&input, buf, 1);
	if (missing)
		return -EFAULT;
	ECM_IPA_DEBUG("input received %c\n", input);
	*enable = input - '0';
	ECM_IPA_DEBUG("value was set to %d\n", *enable);
	return count;
}

static ssize_t ecm_ipa_debugfs_enable_read(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos)
{
	int nbytes;
	int size = 0;
	int ret;
	loff_t pos;
	u8 enable_str[sizeof(char)*3] = {0};
	bool *enable = file->private_data;
	pos = *ppos;
	nbytes = scnprintf(enable_str, sizeof(enable_str), "%d\n", *enable);
	ret = simple_read_from_buffer(ubuf, count, ppos, enable_str, nbytes);
	if (ret < 0) {
		ECM_IPA_ERROR("simple_read_from_buffer problem");
		return ret;
	}
	size += ret;
	count -= nbytes;
	*ppos = pos + size;
	return size;
}

static int ecm_ipa_debugfs_init(struct ecm_ipa_dev *dev)
{
	const mode_t flags = S_IRUSR | S_IRGRP | S_IROTH |
			S_IWUSR | S_IWGRP | S_IWOTH;
	int ret = -EINVAL;
	ECM_IPA_LOG_ENTRY();
	if (!dev)
		return -EINVAL;
	dev->folder = debugfs_create_dir("ecm_ipa", NULL);
	if (!dev->folder) {
		ECM_IPA_ERROR("could not create debugfs folder entry\n");
		ret = -EFAULT;
		goto fail_folder;
	}
	dev->tx_file = debugfs_create_file("tx_enable", flags, dev->folder, dev,
		   &ecm_ipa_debugfs_tx_ops);
	if (!dev->tx_file) {
		ECM_IPA_ERROR("could not create debugfs tx file\n");
		ret = -EFAULT;
		goto fail_file;
	}
	dev->rx_file = debugfs_create_file("rx_enable", flags, dev->folder, dev,
			&ecm_ipa_debugfs_rx_ops);
	if (!dev->rx_file) {
		ECM_IPA_ERROR("could not create debugfs rx file\n");
		ret = -EFAULT;
		goto fail_file;
	}
	dev->rm_file = debugfs_create_file("rm_enable", flags, dev->folder, dev,
			&ecm_ipa_debugfs_rm_ops);
	if (!dev->rm_file) {
		ECM_IPA_ERROR("could not create debugfs rm file\n");
		ret = -EFAULT;
		goto fail_file;
	}
	dev->dma_file = debugfs_create_file("dma_enable", flags, dev->folder,
			dev, &ecm_ipa_debugfs_dma_ops);
	if (!dev->dma_file) {
		ECM_IPA_ERROR("could not create debugfs dma file\n");
		ret = -EFAULT;
		goto fail_file;
	}
	ECM_IPA_LOG_EXIT();
	return 0;
fail_file:
	debugfs_remove_recursive(dev->folder);
fail_folder:
	return ret;
}

static void ecm_ipa_debugfs_destroy(struct ecm_ipa_dev *dev)
{
	debugfs_remove_recursive(dev->folder);
}

static void eth_get_drvinfo(struct net_device *net,
		struct ethtool_drvinfo *drv_info)
{
	ECM_IPA_LOG_ENTRY();
	strlcpy(drv_info->driver, DRIVER_NAME, sizeof(drv_info->driver));
	strlcpy(drv_info->version, DRIVER_VERSION, sizeof(drv_info->version));
	ECM_IPA_LOG_EXIT();
}


/**
 * ecm_ipa_ep_cfg() - configure the USB endpoints for ECM
 *
 *usb_to_ipa_hdl: handle received from ipa_connect
 *ipa_to_usb_hdl: handle received from ipa_connect
 *
 * USB to IPA pipe:
 *  - No de-aggregation
 *  - Remove Ethernet header
 *  - SRC NAT
 *  - Default routing(0)
 * IPA to USB Pipe:
 *  - No aggregation
 *  - Add Ethernet header
 */
int ecm_ipa_ep_registers_cfg(u32 usb_to_ipa_hdl, u32 ipa_to_usb_hdl)
{
	int result = 0;
	struct ipa_ep_cfg usb_to_ipa_ep_cfg;
	struct ipa_ep_cfg ipa_to_usb_ep_cfg;
	ECM_IPA_LOG_ENTRY();
	memset(&usb_to_ipa_ep_cfg, 0 , sizeof(struct ipa_ep_cfg));
	usb_to_ipa_ep_cfg.aggr.aggr_en = IPA_BYPASS_AGGR;
	usb_to_ipa_ep_cfg.hdr.hdr_len = ETH_HLEN;
	usb_to_ipa_ep_cfg.nat.nat_en = IPA_SRC_NAT;
	usb_to_ipa_ep_cfg.route.rt_tbl_hdl = 0;
	usb_to_ipa_ep_cfg.mode.dst = IPA_CLIENT_A5_LAN_WAN_CONS;
	usb_to_ipa_ep_cfg.mode.mode = IPA_BASIC;
	result = ipa_cfg_ep(usb_to_ipa_hdl, &usb_to_ipa_ep_cfg);
	if (result) {
		ECM_IPA_ERROR("failed to configure USB to IPA point\n");
		goto out;
	}
	memset(&ipa_to_usb_ep_cfg, 0 , sizeof(struct ipa_ep_cfg));
	ipa_to_usb_ep_cfg.aggr.aggr_en = IPA_BYPASS_AGGR;
	ipa_to_usb_ep_cfg.hdr.hdr_len = ETH_HLEN;
	ipa_to_usb_ep_cfg.nat.nat_en = IPA_BYPASS_NAT;
	result = ipa_cfg_ep(ipa_to_usb_hdl, &ipa_to_usb_ep_cfg);
	if (result) {
		ECM_IPA_ERROR("failed to configure IPA to USB end-point\n");
		goto out;
	}
	ECM_IPA_DEBUG("end-point registers successfully configured\n");
out:
	ECM_IPA_LOG_EXIT();
	return result;
}

/**
 * ecm_ipa_ep_registers_dma_cfg() - configure the USB endpoints for ECM
 *	DMA
 * @usb_to_ipa_hdl: handle received from ipa_connect
 *
 * This function will override the previous configuration
 * which is needed for cores that does not support blocks logic
 * Note that client handles are the actual pipe index
 */
int ecm_ipa_ep_registers_dma_cfg(u32 usb_to_ipa_hdl)
{
	int result = 0;
	struct ipa_ep_cfg_mode cfg_mode;
	u32 apps_to_ipa_hdl = 2;
	ECM_IPA_LOG_ENTRY();
	/* Apps to IPA - override the configuration made by IPA driver
	 * in order to allow data path on older platforms*/
	memset(&cfg_mode, 0 , sizeof(cfg_mode));
	cfg_mode.mode = IPA_DMA;
	cfg_mode.dst = IPA_CLIENT_USB_CONS;
	result = ipa_cfg_ep_mode(apps_to_ipa_hdl, &cfg_mode);
	if (result) {
		ECM_IPA_ERROR("failed to configure Apps to IPA\n");
		goto out;
	}
	memset(&cfg_mode, 0 , sizeof(cfg_mode));
	cfg_mode.mode = IPA_DMA;
	cfg_mode.dst = IPA_CLIENT_A5_LAN_WAN_CONS;
	result = ipa_cfg_ep_mode(usb_to_ipa_hdl, &cfg_mode);
	if (result) {
		ECM_IPA_ERROR("failed to configure USB to IPA\n");
		goto out;
	}
	ECM_IPA_DEBUG("end-point registers successfully configured\n");
out:
	ECM_IPA_LOG_EXIT();
	return result;
}

/**
 * ecm_ipa_set_device_ethernet_addr() - set device etherenet address
 * @dev_ethaddr: device etherenet address
 *
 * Returns 0 for success, negative otherwise
 */
int ecm_ipa_set_device_ethernet_addr(u8 *dev_ethaddr, u8 device_ethaddr[])
{
	if (!is_valid_ether_addr(device_ethaddr))
		return -EINVAL;
	memcpy(dev_ethaddr, device_ethaddr, ETH_ALEN);
	ECM_IPA_DEBUG("device ethernet address: %pM\n", dev_ethaddr);
	return 0;
}

/**
 * ecm_ipa_init_module() - module initialization
 *
 */
static int ecm_ipa_init_module(void)
{
	ECM_IPA_LOG_ENTRY();
	ECM_IPA_LOG_EXIT();
	return 0;
}

/**
 * ecm_ipa_cleanup_module() - module cleanup
 *
 */
static void ecm_ipa_cleanup_module(void)
{
	ECM_IPA_LOG_ENTRY();
	ECM_IPA_LOG_EXIT();
	return;
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("ECM IPA network interface");

late_initcall(ecm_ipa_init_module);
module_exit(ecm_ipa_cleanup_module);
