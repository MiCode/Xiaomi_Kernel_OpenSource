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
#include <linux/atomic.h>
#include <mach/ecm_ipa.h>

#define DRIVER_NAME "ecm_ipa"
#define ECM_IPA_IPV4_HDR_NAME "ecm_eth_ipv4"
#define ECM_IPA_IPV6_HDR_NAME "ecm_eth_ipv6"
#define IPA_TO_USB_CLIENT	IPA_CLIENT_USB_CONS
#define INACTIVITY_MSEC_DELAY 100
#define DEFAULT_OUTSTANDING_HIGH 64
#define DEFAULT_OUTSTANDING_LOW 32
#define DEBUGFS_TEMP_BUF_SIZE 4

#define ECM_IPA_ERROR(fmt, args...) \
	pr_err(DRIVER_NAME "@%s@%d@ctx:%s: "\
			fmt, __func__, __LINE__, current->comm, ## args)

#define NULL_CHECK(ptr) \
	do { \
		if (!(ptr)) { \
			ECM_IPA_ERROR("null pointer #ptr\n"); \
			return -EINVAL; \
		} \
	} \
	while (0)

#define ECM_IPA_LOG_ENTRY() pr_debug("begin\n")
#define ECM_IPA_LOG_EXIT() pr_debug("end\n")

/**
 * struct ecm_ipa_dev - main driver context parameters
 * @net: network interface struct implemented by this driver
 * @directory: debugfs directory for various debuging switches
 * @tx_enable: flag that enable/disable Tx path to continue to IPA
 * @rx_enable: flag that enable/disable Rx path to continue to IPA
 * @rm_enable: flag that enable/disable Resource manager request prior to Tx
 * @dma_enable: flag that allow on-the-fly DMA mode for IPA
 * @eth_ipv4_hdr_hdl: saved handle for ipv4 header-insertion table
 * @eth_ipv6_hdr_hdl: saved handle for ipv6 header-insertion table
 * @usb_to_ipa_hdl: save handle for IPA pipe operations
 * @ipa_to_usb_hdl: save handle for IPA pipe operations
 * @outstanding_pkts: number of packets sent to IPA without TX complete ACKed
 * @outstanding_high: number of outstanding packets allowed
 * @outstanding_low: number of outstanding packets which shall cause
 *  to netdev queue start (after stopped due to outstanding_high reached)
 */
struct ecm_ipa_dev {
	struct net_device *net;
	u32 tx_enable;
	u32 rx_enable;
	u32  rm_enable;
	bool dma_enable;
	struct dentry *directory;
	uint32_t eth_ipv4_hdr_hdl;
	uint32_t eth_ipv6_hdr_hdl;
	u32 usb_to_ipa_hdl;
	u32 ipa_to_usb_hdl;
	atomic_t outstanding_pkts;
	u8 outstanding_high;
	u8 outstanding_low;
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
static void ecm_ipa_destory_rm_resource(struct ecm_ipa_dev *dev);
static bool rx_filter(struct sk_buff *skb);
static bool tx_filter(struct sk_buff *skb);
static bool rm_enabled(struct ecm_ipa_dev *dev);

static int ecm_ipa_rules_cfg(struct ecm_ipa_dev *dev,
		const void *dst_mac, const void *src_mac);
static int ecm_ipa_register_properties(void);
static void ecm_ipa_deregister_properties(void);
static int ecm_ipa_debugfs_init(struct ecm_ipa_dev *dev);
static void ecm_ipa_debugfs_destroy(struct ecm_ipa_dev *dev);
static int ecm_ipa_debugfs_dma_open(struct inode *inode, struct file *file);
static int ecm_ipa_debugfs_atomic_open(struct inode *inode, struct file *file);
static ssize_t ecm_ipa_debugfs_enable_read(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos);
static ssize_t ecm_ipa_debugfs_atomic_read(struct file *file,
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

const struct file_operations ecm_ipa_debugfs_dma_ops = {
	.open = ecm_ipa_debugfs_dma_open,
	.read = ecm_ipa_debugfs_enable_read,
	.write = ecm_ipa_debugfs_enable_write_dma,
};

const struct file_operations ecm_ipa_debugfs_atomic_ops = {
		.open = ecm_ipa_debugfs_atomic_open,
		.read = ecm_ipa_debugfs_atomic_read,
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
	pr_debug("%s initializing\n", DRIVER_NAME);
	NULL_CHECK(ecm_ipa_rx_dp_notify);
	NULL_CHECK(ecm_ipa_tx_dp_notify);
	NULL_CHECK(priv);
	pr_debug("rx_cb=0x%p, tx_cb=0x%p priv=0x%p\n",
			ecm_ipa_rx_dp_notify, ecm_ipa_tx_dp_notify, *priv);
	net = alloc_etherdev(sizeof(struct ecm_ipa_dev));
	if (!net) {
		ret = -ENOMEM;
		ECM_IPA_ERROR("fail to allocate etherdev\n");
		goto fail_alloc_etherdev;
	}
	pr_debug("etherdev was successfully allocated\n");
	dev = netdev_priv(net);
	memset(dev, 0, sizeof(*dev));
	dev->tx_enable = true;
	dev->rx_enable = true;
	atomic_set(&dev->outstanding_pkts, 0);
	dev->outstanding_high = DEFAULT_OUTSTANDING_HIGH;
	dev->outstanding_low = DEFAULT_OUTSTANDING_LOW;
	dev->net = net;
	ecm_ipa_ctx = dev;
	*priv = (void *)dev;
	snprintf(net->name, sizeof(net->name), "%s%%d", "ecm");
	net->netdev_ops = &ecm_ipa_netdev_ops;
	pr_debug("internal data structures were intialized\n");
	ret = ecm_ipa_debugfs_init(dev);
	if (ret)
		goto fail_debugfs;
	pr_debug("debugfs entries were created\n");
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
	eth_ipv4->h_proto = htons(ETH_P_IP);
	ipv4_hdr->hdr_len = ETH_HLEN;
	ipv4_hdr->is_partial = 0;
	strlcpy(ipv6_hdr->name, ECM_IPA_IPV6_HDR_NAME, IPA_RESOURCE_NAME_MAX);
	memcpy(eth_ipv6->h_dest, dst_mac, ETH_ALEN);
	memcpy(eth_ipv6->h_source, src_mac, ETH_ALEN);
	eth_ipv6->h_proto = htons(ETH_P_IPV6);
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

/* ecm_ipa_register_properties() - set Tx/Rx properties for ipacm
 *
 * Register ecm0 interface with 2 Tx properties and 2 Rx properties:
 * The 2 Tx properties are for data flowing from IPA to USB, they
 * have Header-Insertion properties both for Ipv4 and Ipv6 Ethernet framing.
 * The 2 Rx properties are for data flowing from USB to IPA, they have
 * simple rule which always "hit".
 *
 */
static int ecm_ipa_register_properties(void)
{
	struct ipa_tx_intf tx_properties = {0};
	struct ipa_ioc_tx_intf_prop properties[2] = { {0}, {0} };
	struct ipa_ioc_tx_intf_prop *ipv4_property;
	struct ipa_ioc_tx_intf_prop *ipv6_property;
	struct ipa_ioc_rx_intf_prop rx_ioc_properties[2] = { {0}, {0} };
	struct ipa_rx_intf rx_properties = {0};
	struct ipa_ioc_rx_intf_prop *rx_ipv4_property;
	struct ipa_ioc_rx_intf_prop *rx_ipv6_property;
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

	rx_properties.prop = rx_ioc_properties;
	rx_ipv4_property = &rx_properties.prop[0];
	rx_ipv4_property->ip = IPA_IP_v4;
	rx_ipv4_property->attrib.attrib_mask = 0;
	rx_ipv4_property->src_pipe = IPA_CLIENT_USB_PROD;
	rx_ipv6_property = &rx_properties.prop[1];
	rx_ipv6_property->ip = IPA_IP_v6;
	rx_ipv6_property->attrib.attrib_mask = 0;
	rx_ipv6_property->src_pipe = IPA_CLIENT_USB_PROD;
	rx_properties.num_props = 2;

	result = ipa_register_intf("ecm0", &tx_properties, &rx_properties);
	if (result)
		ECM_IPA_ERROR("fail on Tx/Rx properties registration\n");

	ECM_IPA_LOG_EXIT();

	return result;
}

static void ecm_ipa_deregister_properties(void)
{
	int result;
	ECM_IPA_LOG_ENTRY();
	result = ipa_deregister_intf("ecm0");
	if (result)
		pr_debug("Fail on Tx prop deregister\n");
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
	pr_debug("priv=0x%p, host_ethaddr=%pM device_ethaddr=%pM\n",
					priv, host_ethaddr, device_ethaddr);
	result = ecm_ipa_create_rm_resource(dev);
	if (result) {
		ECM_IPA_ERROR("fail on RM create\n");
		return -EINVAL;
	}
	pr_debug("RM resource was created\n");
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
	pr_debug("Ethernet header insertion was set\n");
	result = ecm_ipa_register_properties();
	if (result) {
		ECM_IPA_ERROR("fail on properties set\n");
		goto fail_register_tx;
	}
	pr_debug("ECM 2 Tx and 2 Rx properties were registered\n");
	result = register_netdev(net);
	if (result) {
		ECM_IPA_ERROR("register_netdev failed: %d\n", result);
		goto fail_register_netdev;
	}
	pr_debug("register_netdev succeeded\n");
	ECM_IPA_LOG_EXIT();
	return 0;
fail_register_netdev:
	ecm_ipa_deregister_properties();
fail_register_tx:
fail_set_device_ethernet:
	ecm_ipa_rules_destroy(dev);
	ecm_ipa_destory_rm_resource(dev);
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
	pr_debug("usb_to_ipa_hdl = %d, ipa_to_usb_hdl = %d, priv=0x%p\n",
					usb_to_ipa_hdl, ipa_to_usb_hdl, priv);
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
	pr_debug("priv=0x%p\n", priv);
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
		pr_debug("Resource Granted - waking queue\n");
		netif_wake_queue(dev->net);
	} else {
		pr_debug("Resource released\n");
	}
	ECM_IPA_LOG_EXIT();
}

static int ecm_ipa_create_rm_resource(struct ecm_ipa_dev *dev)
{
	struct ipa_rm_create_params create_params = {0};
	int result;
	ECM_IPA_LOG_ENTRY();
	if (!dev->rm_enable) {
		pr_debug("RM feature not used\n");
		return 0;
	}
	create_params.name = IPA_RM_RESOURCE_STD_ECM_PROD;
	create_params.reg_params.user_data = dev;
	create_params.reg_params.notify_cb = ecm_ipa_rm_notify;
	result = ipa_rm_create_resource(&create_params);
	if (result) {
		ECM_IPA_ERROR("Fail on ipa_rm_create_resource\n");
		goto fail_rm_create;
	}
	pr_debug("rm client was created");

	result = ipa_rm_inactivity_timer_init(IPA_RM_RESOURCE_STD_ECM_PROD,
			INACTIVITY_MSEC_DELAY);
	if (result) {
		ECM_IPA_ERROR("Fail on ipa_rm_inactivity_timer_init\n");
		goto fail_it;
	}
	pr_debug("rm_it client was created");

	result = ipa_rm_add_dependency(IPA_RM_RESOURCE_STD_ECM_PROD,
				IPA_RM_RESOURCE_USB_CONS);
	if (result)
		ECM_IPA_ERROR("unable to add dependency (%d)\n", result);

	pr_debug("rm dependency was set\n");

	ECM_IPA_LOG_EXIT();
	return 0;

fail_it:
fail_rm_create:
	return result;
}

static void ecm_ipa_destory_rm_resource(struct ecm_ipa_dev *dev)
{
	ECM_IPA_LOG_ENTRY();
	if (!dev->rm_enable)
		return;
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
	pr_debug("stopping net device\n");
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
	pr_debug("priv=0x%p\n", priv);
	if (!dev) {
		ECM_IPA_ERROR("dev NULL pointer\n");
		return;
	}

	ecm_ipa_destory_rm_resource(dev);
	ecm_ipa_debugfs_destroy(dev);

	unregister_netdev(dev->net);
	free_netdev(dev->net);

	pr_debug("cleanup done\n");
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

	if (unlikely(netif_queue_stopped(net))) {
		ECM_IPA_ERROR("interface queue is stopped\n");
		goto out;
	}

	if (unlikely(tx_filter(skb))) {
		dev_kfree_skb_any(skb);
		pr_debug("packet got filtered out on Tx path\n");
		status = NETDEV_TX_OK;
		goto out;
	}
	ret = resource_request(dev);
	if (ret) {
		pr_debug("Waiting to resource\n");
		netif_stop_queue(net);
		goto resource_busy;
	}

	if (atomic_read(&dev->outstanding_pkts) >= dev->outstanding_high) {
		pr_debug("Outstanding high boundary reached (%d)- stopping queue\n",
				dev->outstanding_high);
		netif_stop_queue(net);
		status = -NETDEV_TX_BUSY;
		goto out;
	}

	ret = ipa_tx_dp(IPA_TO_USB_CLIENT, skb, NULL);
	if (ret) {
		ECM_IPA_ERROR("ipa transmit failed (%d)\n", ret);
		goto fail_tx_packet;
	}

	atomic_inc(&dev->outstanding_pkts);
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
		pr_debug("packet got filtered out on Rx path\n");
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

	if (!dev) {
		ECM_IPA_ERROR("dev is NULL pointer\n");
		return;
	}
	if (evt != IPA_WRITE_DONE) {
		ECM_IPA_ERROR("unsupported event on Tx callback\n");
		return;
	}
	atomic_dec(&dev->outstanding_pkts);
	if (netif_queue_stopped(dev->net) &&
		atomic_read(&dev->outstanding_pkts) < (dev->outstanding_low)) {
		pr_debug("Outstanding low boundary reached (%d) - waking up queue\n",
				dev->outstanding_low);
		netif_wake_queue(dev->net);
	}

	dev_kfree_skb_any(skb);
	return;
}

static int ecm_ipa_debugfs_atomic_open(struct inode *inode, struct file *file)
{
	struct ecm_ipa_dev *dev = inode->i_private;
	ECM_IPA_LOG_ENTRY();
	file->private_data = &(dev->outstanding_pkts);
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
	pr_debug("input received %c\n", input);
	*enable = input - '0';
	pr_debug("value was set to %d\n", *enable);
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

static ssize_t ecm_ipa_debugfs_atomic_read(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos)
{
	int nbytes;
	u8 atomic_str[DEBUGFS_TEMP_BUF_SIZE] = {0};
	atomic_t *atomic_var = file->private_data;
	nbytes = scnprintf(atomic_str, sizeof(atomic_str), "%d\n",
			atomic_read(atomic_var));
	return simple_read_from_buffer(ubuf, count, ppos, atomic_str, nbytes);
}


static int ecm_ipa_debugfs_init(struct ecm_ipa_dev *dev)
{
	const mode_t flags_read_write = S_IRUGO | S_IWUGO;
	const mode_t flags_read_only = S_IRUGO;
	struct dentry *file;

	ECM_IPA_LOG_ENTRY();

	if (!dev)
		return -EINVAL;

	dev->directory = debugfs_create_dir("ecm_ipa", NULL);
	if (!dev->directory) {
		ECM_IPA_ERROR("could not create debugfs directory entry\n");
		goto fail_directory;
	}
	file = debugfs_create_bool("tx_enable", flags_read_write,
			dev->directory, &dev->tx_enable);
	if (!file) {
		ECM_IPA_ERROR("could not create debugfs tx file\n");
		goto fail_file;
	}
	file = debugfs_create_bool("rx_enable", flags_read_write,
			dev->directory, &dev->rx_enable);
	if (!file) {
		ECM_IPA_ERROR("could not create debugfs rx file\n");
		goto fail_file;
	}
	file = debugfs_create_bool("rm_enable", flags_read_write,
			dev->directory, &dev->rm_enable);
	if (!file) {
		ECM_IPA_ERROR("could not create debugfs rm file\n");
		goto fail_file;
	}
	file = debugfs_create_u8("outstanding_high", flags_read_write,
			dev->directory, &dev->outstanding_high);
	if (!file) {
		ECM_IPA_ERROR("could not create outstanding_high file\n");
		goto fail_file;
	}
	file = debugfs_create_u8("outstanding_low", flags_read_write,
			dev->directory, &dev->outstanding_low);
	if (!file) {
		ECM_IPA_ERROR("could not create outstanding_low file\n");
		goto fail_file;
	}
	file = debugfs_create_file("dma_enable", flags_read_write,
			dev->directory, dev, &ecm_ipa_debugfs_dma_ops);
	if (!file) {
		ECM_IPA_ERROR("could not create debugfs dma file\n");
		goto fail_file;
	}
	file = debugfs_create_file("outstanding", flags_read_only,
			dev->directory, dev, &ecm_ipa_debugfs_atomic_ops);
	if (!file) {
		ECM_IPA_ERROR("could not create outstanding file\n");
		goto fail_file;
	}

	ECM_IPA_LOG_EXIT();
	return 0;
fail_file:
	debugfs_remove_recursive(dev->directory);
fail_directory:
	return -EFAULT;
}

static void ecm_ipa_debugfs_destroy(struct ecm_ipa_dev *dev)
{
	debugfs_remove_recursive(dev->directory);
}

static void eth_get_drvinfo(struct net_device *net,
		struct ethtool_drvinfo *drv_info)
{
	ECM_IPA_LOG_ENTRY();
	strlcpy(drv_info->driver, DRIVER_NAME, sizeof(drv_info->driver));
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
	pr_debug("end-point registers successfully configured\n");
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
	pr_debug("end-point registers successfully configured\n");
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
	pr_debug("device ethernet address: %pM\n", dev_ethaddr);
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
