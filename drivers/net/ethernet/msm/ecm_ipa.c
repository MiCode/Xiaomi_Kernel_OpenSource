/* Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
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
#include <linux/ecm_ipa.h>

#define DRIVER_NAME "ecm_ipa"
#define ECM_IPA_IPV4_HDR_NAME "ecm_eth_ipv4"
#define ECM_IPA_IPV6_HDR_NAME "ecm_eth_ipv6"
#define INACTIVITY_MSEC_DELAY 100
#define DEFAULT_OUTSTANDING_HIGH 64
#define DEFAULT_OUTSTANDING_LOW 32
#define DEBUGFS_TEMP_BUF_SIZE 4
#define TX_TIMEOUT (5 * HZ)


#define ECM_IPA_DEBUG(fmt, args...) \
	pr_debug("ctx:%s: "\
			fmt, current->comm, ## args)

#define ECM_IPA_INFO(fmt, args...) \
	pr_err(DRIVER_NAME "@%s@%d@ctx:%s: "\
			fmt, __func__, __LINE__, current->comm, ## args)

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

#define ECM_IPA_LOG_ENTRY() ECM_IPA_DEBUG("begin\n")
#define ECM_IPA_LOG_EXIT() ECM_IPA_DEBUG("end\n")

/**
 * enum ecm_ipa_state - specify the current driver internal state
 *  which is guarded by a state machine.
 *
 * The driver internal state changes due to its external API usage.
 * The driver saves its internal state to guard from caller illegal
 * call sequence.
 * states:
 * UNLOADED is the first state which is the default one and is also the state
 *  after the driver gets unloaded(cleanup).
 * INITIALIZED is the driver state once it finished registering
 *  the network device and all internal data struct were initialized
 * CONNECTED is the driver state once the USB pipes were connected to IPA
 * UP is the driver state after the interface mode was set to UP but the
 *  pipes are not connected yet - this state is meta-stable state.
 * CONNECTED_AND_UP is the driver state when the pipe were connected and
 *  the interface got UP request from the network stack. this is the driver
 *   idle operation state which allows it to transmit/receive data.
 * INVALID is a state which is not allowed.
 */
enum ecm_ipa_state {
	ECM_IPA_UNLOADED = 0,
	ECM_IPA_INITIALIZED,
	ECM_IPA_CONNECTED,
	ECM_IPA_UP,
	ECM_IPA_CONNECTED_AND_UP,
	ECM_IPA_INVALID,
};

/**
 * enum ecm_ipa_operation - enumerations used to descibe the API operation
 *
 * Those enums are used as input for the driver state machine.
 */
enum ecm_ipa_operation {
	ECM_IPA_INITIALIZE,
	ECM_IPA_CONNECT,
	ECM_IPA_OPEN,
	ECM_IPA_STOP,
	ECM_IPA_DISCONNECT,
	ECM_IPA_CLEANUP,
};

#define ECM_IPA_STATE_DEBUG(ecm_ipa_ctx) \
	ECM_IPA_DEBUG("Driver state - %s\n",\
	ecm_ipa_state_string(ecm_ipa_ctx->state));

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
 * @state: current state of ecm_ipa driver
 * @device_ready_notify: callback supplied by USB core driver
 * This callback shall be called by the Netdev once the Netdev internal
 * state is changed to RNDIS_IPA_CONNECTED_AND_UP
 * @ipa_to_usb_client: consumer client
 * @usb_to_ipa_client: producer client
 * @ipa_rm_resource_name_prod: IPA resource manager producer resource
 * @ipa_rm_resource_name_cons: IPA resource manager consumer resource
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
	enum ecm_ipa_state state;
	void (*device_ready_notify)(void);
	enum ipa_client_type ipa_to_usb_client;
	enum ipa_client_type usb_to_ipa_client;
	enum ipa_rm_resource_name ipa_rm_resource_name_prod;
	enum ipa_rm_resource_name ipa_rm_resource_name_cons;
};

static int ecm_ipa_open(struct net_device *net);
static void ecm_ipa_packet_receive_notify(void *priv,
		enum ipa_dp_evt_type evt, unsigned long data);
static void ecm_ipa_tx_complete_notify(void *priv,
		enum ipa_dp_evt_type evt, unsigned long data);
static void ecm_ipa_tx_timeout(struct net_device *net);
static int ecm_ipa_stop(struct net_device *net);
static void ecm_ipa_enable_data_path(struct ecm_ipa_dev *ecm_ipa_ctx);
static int ecm_ipa_rules_cfg(struct ecm_ipa_dev *ecm_ipa_ctx,
		const void *dst_mac, const void *src_mac);
static void ecm_ipa_rules_destroy(struct ecm_ipa_dev *ecm_ipa_ctx);
static int ecm_ipa_register_properties(struct ecm_ipa_dev *ecm_ipa_ctx);
static void ecm_ipa_deregister_properties(void);
static void ecm_ipa_rm_notify(void *user_data, enum ipa_rm_event event,
		unsigned long data);
static struct net_device_stats *ecm_ipa_get_stats(struct net_device *net);
static int ecm_ipa_create_rm_resource(struct ecm_ipa_dev *ecm_ipa_ctx);
static void ecm_ipa_destory_rm_resource(struct ecm_ipa_dev *ecm_ipa_ctx);
static bool rx_filter(struct sk_buff *skb);
static bool tx_filter(struct sk_buff *skb);
static bool rm_enabled(struct ecm_ipa_dev *ecm_ipa_ctx);
static int resource_request(struct ecm_ipa_dev *ecm_ipa_ctx);
static void resource_release(struct ecm_ipa_dev *ecm_ipa_ctx);
static netdev_tx_t ecm_ipa_start_xmit(struct sk_buff *skb,
					struct net_device *net);
static int ecm_ipa_debugfs_stall_open(struct inode *inode,
	struct file *file);
static ssize_t ecm_ipa_debugfs_stall_write(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos);
static int ecm_ipa_debugfs_atomic_open(struct inode *inode, struct file *file);
static ssize_t ecm_ipa_debugfs_enable_write_dma(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos);
static int ecm_ipa_debugfs_dma_open(struct inode *inode, struct file *file);
static ssize_t ecm_ipa_debugfs_enable_write(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos);
static ssize_t ecm_ipa_debugfs_enable_read(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos);
static ssize_t ecm_ipa_debugfs_atomic_read(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos);
static int ecm_ipa_debugfs_init(struct ecm_ipa_dev *ecm_ipa_ctx);
static void ecm_ipa_debugfs_destroy(struct ecm_ipa_dev *ecm_ipa_ctx);
static int ecm_ipa_ep_registers_cfg(u32 usb_to_ipa_hdl, u32 ipa_to_usb_hdl);
static int ecm_ipa_ep_registers_dma_cfg(u32 usb_to_ipa_hdl,
					enum ipa_client_type prod_client);
static int ecm_ipa_set_device_ethernet_addr(u8 *dev_ethaddr,
		u8 device_ethaddr[]);
static enum ecm_ipa_state ecm_ipa_next_state(enum ecm_ipa_state current_state,
		enum ecm_ipa_operation operation);
static const char *ecm_ipa_state_string(enum ecm_ipa_state state);
static int ecm_ipa_init_module(void);
static void ecm_ipa_cleanup_module(void);

static const struct net_device_ops ecm_ipa_netdev_ops = {
	.ndo_open		= ecm_ipa_open,
	.ndo_stop		= ecm_ipa_stop,
	.ndo_start_xmit = ecm_ipa_start_xmit,
	.ndo_set_mac_address = eth_mac_addr,
	.ndo_tx_timeout = ecm_ipa_tx_timeout,
	.ndo_get_stats = ecm_ipa_get_stats,
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

const struct file_operations ecm_ipa_debugfs_stall_ops = {
	.open = ecm_ipa_debugfs_stall_open,
	.write = ecm_ipa_debugfs_stall_write,
};

static void ecm_ipa_msg_free_cb(void *buff, u32 len, u32 type)
{
	kfree(buff);
}

/**
 * ecm_ipa_init() - create network device and initializes internal
 *  data structures
 * @params: in/out parameters required for ecm_ipa initialization
 *
 * Shall be called prior to pipe connection.
 * The out parameters (the callbacks) shall be supplied to ipa_connect.
 * Detailed description:
 *  - allocate the network device
 *  - set default values for driver internals
 *  - create debugfs folder and files
 *  - create IPA resource manager client
 *  - add header insertion rules for IPA driver (based on host/device
 *    Ethernet addresses given in input params)
 *  - register tx/rx properties to IPA driver (will be later used
 *    by IPA configuration manager to configure reset of the IPA rules)
 *  - set the carrier state to "off" (until ecm_ipa_connect is called)
 *  - register the network device
 *  - set the out parameters
 *
 * Returns negative errno, or zero on success
 */
int ecm_ipa_init(struct ecm_ipa_params *params)
{
	int result = 0;
	struct net_device *net;
	struct ecm_ipa_dev *ecm_ipa_ctx;

	ECM_IPA_LOG_ENTRY();

	ECM_IPA_DEBUG("%s initializing\n", DRIVER_NAME);
	NULL_CHECK(params);

	ECM_IPA_DEBUG("host_ethaddr=%pM, device_ethaddr=%pM\n",
		params->host_ethaddr,
		params->device_ethaddr);

	net = alloc_etherdev(sizeof(struct ecm_ipa_dev));
	if (!net) {
		result = -ENOMEM;
		ECM_IPA_ERROR("fail to allocate etherdev\n");
		goto fail_alloc_etherdev;
	}
	ECM_IPA_DEBUG("network device was successfully allocated\n");

	ecm_ipa_ctx = netdev_priv(net);
	if (!ecm_ipa_ctx) {
		ECM_IPA_ERROR("fail to extract netdev priv\n");
		result = -ENOMEM;
		goto fail_netdev_priv;
	}
	memset(ecm_ipa_ctx, 0, sizeof(*ecm_ipa_ctx));
	ECM_IPA_DEBUG("ecm_ipa_ctx (private) = %p\n", ecm_ipa_ctx);

	ecm_ipa_ctx->net = net;
	ecm_ipa_ctx->tx_enable = true;
	ecm_ipa_ctx->rx_enable = true;
	ecm_ipa_ctx->rm_enable = true;
	ecm_ipa_ctx->outstanding_high = DEFAULT_OUTSTANDING_HIGH;
	ecm_ipa_ctx->outstanding_low = DEFAULT_OUTSTANDING_LOW;
	atomic_set(&ecm_ipa_ctx->outstanding_pkts, 0);
	snprintf(net->name, sizeof(net->name), "%s%%d", "ecm");
	net->netdev_ops = &ecm_ipa_netdev_ops;
	net->watchdog_timeo = TX_TIMEOUT;
	ECM_IPA_DEBUG("internal data structures were intialized\n");

	if (!params->device_ready_notify)
		ECM_IPA_DEBUG("device_ready_notify() was not supplied");
	ecm_ipa_ctx->device_ready_notify = params->device_ready_notify;

	result = ecm_ipa_debugfs_init(ecm_ipa_ctx);
	if (result)
		goto fail_debugfs;
	ECM_IPA_DEBUG("debugfs entries were created\n");

	result = ecm_ipa_set_device_ethernet_addr(net->dev_addr,
			params->device_ethaddr);
	if (result) {
		ECM_IPA_ERROR("set device MAC failed\n");
		goto fail_set_device_ethernet;
	}
	ECM_IPA_DEBUG("Device Ethernet address set %pM\n", net->dev_addr);

	result = ecm_ipa_rules_cfg(ecm_ipa_ctx, params->host_ethaddr,
			params->device_ethaddr);
	if (result) {
		ECM_IPA_ERROR("fail on ipa rules set\n");
		goto fail_rules_cfg;
	}
	ECM_IPA_DEBUG("Ethernet header insertion set\n");

	netif_carrier_off(net);
	ECM_IPA_DEBUG("netif_carrier_off() was called\n");

	netif_stop_queue(ecm_ipa_ctx->net);
	ECM_IPA_DEBUG("netif_stop_queue() was called");

	result = register_netdev(net);
	if (result) {
		ECM_IPA_ERROR("register_netdev failed: %d\n", result);
		goto fail_register_netdev;
	}
	ECM_IPA_DEBUG("register_netdev succeeded\n");

	params->ecm_ipa_rx_dp_notify = ecm_ipa_packet_receive_notify;
	params->ecm_ipa_tx_dp_notify = ecm_ipa_tx_complete_notify;
	params->private = (void *)ecm_ipa_ctx;
	params->skip_ep_cfg = false;
	ecm_ipa_ctx->state = ECM_IPA_INITIALIZED;
	ECM_IPA_STATE_DEBUG(ecm_ipa_ctx);

	ECM_IPA_INFO("ECM_IPA was initialized successfully\n");

	ECM_IPA_LOG_EXIT();

	return 0;

fail_register_netdev:
	ecm_ipa_rules_destroy(ecm_ipa_ctx);
fail_set_device_ethernet:
fail_rules_cfg:
	ecm_ipa_debugfs_destroy(ecm_ipa_ctx);
fail_debugfs:
fail_netdev_priv:
	free_netdev(net);
fail_alloc_etherdev:
	return result;
}
EXPORT_SYMBOL(ecm_ipa_init);

/**
 * ecm_ipa_connect() - notify ecm_ipa for IPA<->USB pipes connection
 * @usb_to_ipa_hdl: handle of IPA driver client for USB->IPA
 * @ipa_to_usb_hdl: handle of IPA driver client for IPA->USB
 * @priv: same value that was set by ecm_ipa_init(), this
 *  parameter holds the network device pointer.
 *
 * Once USB driver finishes the pipe connection between IPA core
 * and USB core this method shall be called in order to
 * allow ecm_ipa complete the data path configurations.
 * Caller should make sure that it is calling this function
 * from a context that allows it to handle device_ready_notify().
 * Detailed description:
 *  - configure the IPA end-points register
 *  - notify the Linux kernel for "carrier_on"
 *  After this function is done the driver state changes to "Connected".
 *  This API is expected to be called after ecm_ipa_init() or
 *  after a call to ecm_ipa_disconnect.
 */
int ecm_ipa_connect(u32 usb_to_ipa_hdl, u32 ipa_to_usb_hdl, void *priv)
{
	struct ecm_ipa_dev *ecm_ipa_ctx = priv;
	int next_state;
	struct ipa_ecm_msg *ecm_msg;
	struct ipa_msg_meta msg_meta;
	int retval;

	ECM_IPA_LOG_ENTRY();
	NULL_CHECK(priv);
	ECM_IPA_DEBUG("usb_to_ipa_hdl = %d, ipa_to_usb_hdl = %d, priv=0x%p\n",
		      usb_to_ipa_hdl, ipa_to_usb_hdl, priv);

	next_state = ecm_ipa_next_state(ecm_ipa_ctx->state, ECM_IPA_CONNECT);
	if (next_state == ECM_IPA_INVALID) {
		ECM_IPA_ERROR("can't call connect before calling initialize\n");
		return -EPERM;
	}
	ecm_ipa_ctx->state = next_state;
	ECM_IPA_STATE_DEBUG(ecm_ipa_ctx);

	if (!ipa_is_client_handle_valid(usb_to_ipa_hdl)) {
		ECM_IPA_ERROR("usb_to_ipa_hdl(%d) is not a valid ipa handle\n",
				usb_to_ipa_hdl);
		return -EINVAL;
	}
	if (!ipa_is_client_handle_valid(ipa_to_usb_hdl)) {
		ECM_IPA_ERROR("ipa_to_usb_hdl(%d) is not a valid ipa handle\n",
				ipa_to_usb_hdl);
		return -EINVAL;
	}

	ecm_ipa_ctx->ipa_to_usb_hdl = ipa_to_usb_hdl;
	ecm_ipa_ctx->usb_to_ipa_hdl = usb_to_ipa_hdl;

	ecm_ipa_ctx->ipa_to_usb_client = ipa_get_client_mapping(ipa_to_usb_hdl);
	if (ecm_ipa_ctx->ipa_to_usb_client < 0) {
		ECM_IPA_ERROR(
			"Error getting IPA->USB client from handle %d\n",
			ecm_ipa_ctx->ipa_to_usb_client);
		return -EINVAL;
	}
	ECM_IPA_DEBUG("ipa_to_usb_client = %d\n",
		      ecm_ipa_ctx->ipa_to_usb_client);

	ecm_ipa_ctx->usb_to_ipa_client = ipa_get_client_mapping(usb_to_ipa_hdl);
	if (ecm_ipa_ctx->usb_to_ipa_client < 0) {
		ECM_IPA_ERROR(
			"Error getting USB->IPA client from handle %d\n",
			ecm_ipa_ctx->usb_to_ipa_client);
		return -EINVAL;
	}
	ECM_IPA_DEBUG("usb_to_ipa_client = %d\n",
		      ecm_ipa_ctx->usb_to_ipa_client);

	ecm_ipa_ctx->ipa_rm_resource_name_cons =
		ipa_get_rm_resource_from_ep(ipa_to_usb_hdl);
	if (ecm_ipa_ctx->ipa_rm_resource_name_cons < 0) {
		ECM_IPA_ERROR("Error getting CONS RM resource from handle %d\n",
			      ecm_ipa_ctx->ipa_rm_resource_name_cons);
		return -EINVAL;
	}
	ECM_IPA_DEBUG("ipa_rm_resource_name_cons = %d\n",
		      ecm_ipa_ctx->ipa_rm_resource_name_cons);

	ecm_ipa_ctx->ipa_rm_resource_name_prod =
		ipa_get_rm_resource_from_ep(usb_to_ipa_hdl);
	if (ecm_ipa_ctx->ipa_rm_resource_name_prod < 0) {
		ECM_IPA_ERROR("Error getting PROD RM resource from handle %d\n",
			      ecm_ipa_ctx->ipa_rm_resource_name_prod);
		return -EINVAL;
	}
	ECM_IPA_DEBUG("ipa_rm_resource_name_prod = %d\n",
		      ecm_ipa_ctx->ipa_rm_resource_name_prod);

	retval = ecm_ipa_create_rm_resource(ecm_ipa_ctx);
	if (retval) {
		ECM_IPA_ERROR("fail on RM create\n");
		goto fail_create_rm;
	}
	ECM_IPA_DEBUG("RM resource was created\n");

	retval = ecm_ipa_register_properties(ecm_ipa_ctx);
	if (retval) {
		ECM_IPA_ERROR("fail on properties set\n");
		goto fail_create_rm;
	}
	ECM_IPA_DEBUG("ecm_ipa 2 Tx and 2 Rx properties were registered\n");

	retval = ecm_ipa_ep_registers_cfg(usb_to_ipa_hdl, ipa_to_usb_hdl);
	if (retval) {
		ECM_IPA_ERROR("fail on ep cfg\n");
		goto fail;
	}
	ECM_IPA_DEBUG("end-point configured\n");

	netif_carrier_on(ecm_ipa_ctx->net);

	ecm_msg = kzalloc(sizeof(struct ipa_ecm_msg), GFP_KERNEL);
	if (!ecm_msg) {
		ECM_IPA_ERROR("can't alloc msg mem\n");
		retval = -ENOMEM;
		goto fail;
	}

	memset(&msg_meta, 0, sizeof(struct ipa_msg_meta));
	msg_meta.msg_type = ECM_CONNECT;
	msg_meta.msg_len = sizeof(struct ipa_ecm_msg);
	strlcpy(ecm_msg->name, ecm_ipa_ctx->net->name,
		IPA_RESOURCE_NAME_MAX);
	ecm_msg->ifindex = ecm_ipa_ctx->net->ifindex;

	retval = ipa_send_msg(&msg_meta, ecm_msg, ecm_ipa_msg_free_cb);
	if (retval) {
		ECM_IPA_ERROR("fail to send ECM_CONNECT message\n");
		kfree(ecm_msg);
		goto fail;
	}

	if (!netif_carrier_ok(ecm_ipa_ctx->net)) {
		ECM_IPA_ERROR("netif_carrier_ok error\n");
		retval = -EBUSY;
		goto fail;
	}
	ECM_IPA_DEBUG("carrier_on notified\n");

	if (ecm_ipa_ctx->state == ECM_IPA_CONNECTED_AND_UP)
		ecm_ipa_enable_data_path(ecm_ipa_ctx);
	else
		ECM_IPA_DEBUG("data path was not enabled yet\n");

	ECM_IPA_INFO("ECM_IPA was connected successfully\n");

	ECM_IPA_LOG_EXIT();

	return 0;

fail:
	ecm_ipa_deregister_properties();
fail_create_rm:
	ecm_ipa_destory_rm_resource(ecm_ipa_ctx);
	return retval;
}
EXPORT_SYMBOL(ecm_ipa_connect);

/**
 * ecm_ipa_open() - notify Linux network stack to start sending packets
 * @net: the network interface supplied by the network stack
 *
 * Linux uses this API to notify the driver that the network interface
 * transitions to the up state.
 * The driver will instruct the Linux network stack to start
 * delivering data packets.
 */
static int ecm_ipa_open(struct net_device *net)
{
	struct ecm_ipa_dev *ecm_ipa_ctx;
	int next_state;

	ECM_IPA_LOG_ENTRY();

	ecm_ipa_ctx = netdev_priv(net);

	next_state = ecm_ipa_next_state(ecm_ipa_ctx->state, ECM_IPA_OPEN);
	if (next_state == ECM_IPA_INVALID) {
		ECM_IPA_ERROR("can't bring driver up before initialize\n");
		return -EPERM;
	}
	ecm_ipa_ctx->state = next_state;
	ECM_IPA_STATE_DEBUG(ecm_ipa_ctx);

	if (ecm_ipa_ctx->state == ECM_IPA_CONNECTED_AND_UP)
		ecm_ipa_enable_data_path(ecm_ipa_ctx);
	else
		ECM_IPA_DEBUG("data path was not enabled yet\n");

	ECM_IPA_LOG_EXIT();

	return 0;
}

/**
 * ecm_ipa_start_xmit() - send data from APPs to USB core via IPA core
 * @skb: packet received from Linux network stack
 * @net: the network device being used to send this packet
 *
 * Several conditions needed in order to send the packet to IPA:
 * - Transmit queue for the network driver is currently
 *   in "send" state
 * - The driver internal state is in "UP" state.
 * - Filter Tx switch is turned off
 * - The IPA resource manager state for the driver producer client
 *   is "Granted" which implies that all the resources in the dependency
 *   graph are valid for data flow.
 * - outstanding high boundary did not reach.
 *
 * In case all of the above conditions are met, the network driver will
 * send the packet by using the IPA API for Tx.
 * In case the outstanding packet high boundary is reached, the driver will
 * stop the send queue until enough packet were proceeded by the IPA core.
 */
static netdev_tx_t ecm_ipa_start_xmit(struct sk_buff *skb,
					struct net_device *net)
{
	int ret;
	netdev_tx_t status = NETDEV_TX_BUSY;
	struct ecm_ipa_dev *ecm_ipa_ctx = netdev_priv(net);

	net->trans_start = jiffies;

	ECM_IPA_DEBUG("Tx, len=%d, skb->protocol=%d, outstanding=%d\n",
		skb->len, skb->protocol,
		atomic_read(&ecm_ipa_ctx->outstanding_pkts));

	if (unlikely(netif_queue_stopped(net))) {
		ECM_IPA_ERROR("interface queue is stopped\n");
		goto out;
	}

	if (unlikely(ecm_ipa_ctx->state != ECM_IPA_CONNECTED_AND_UP)) {
		ECM_IPA_ERROR("Missing pipe connected and/or iface up\n");
		return NETDEV_TX_BUSY;
	}

	if (unlikely(tx_filter(skb))) {
		dev_kfree_skb_any(skb);
		ECM_IPA_DEBUG("packet got filtered out on Tx path\n");
		status = NETDEV_TX_OK;
		goto out;
	}
	ret = resource_request(ecm_ipa_ctx);
	if (ret) {
		ECM_IPA_DEBUG("Waiting to resource\n");
		netif_stop_queue(net);
		goto resource_busy;
	}

	if (atomic_read(&ecm_ipa_ctx->outstanding_pkts) >=
					ecm_ipa_ctx->outstanding_high) {
		ECM_IPA_DEBUG("outstanding high (%d)- stopping\n",
				ecm_ipa_ctx->outstanding_high);
		netif_stop_queue(net);
		status = NETDEV_TX_BUSY;
		goto out;
	}

	ret = ipa_tx_dp(ecm_ipa_ctx->ipa_to_usb_client, skb, NULL);
	if (ret) {
		ECM_IPA_ERROR("ipa transmit failed (%d)\n", ret);
		goto fail_tx_packet;
	}

	atomic_inc(&ecm_ipa_ctx->outstanding_pkts);

	status = NETDEV_TX_OK;
	goto out;

fail_tx_packet:
out:
	resource_release(ecm_ipa_ctx);
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
 * IPA will pass a packet to the Linux network stack with skb->data pointing
 * to Ethernet packet frame.
 */
static void ecm_ipa_packet_receive_notify(void *priv,
		enum ipa_dp_evt_type evt,
		unsigned long data)
{
	struct sk_buff *skb = (struct sk_buff *)data;
	struct ecm_ipa_dev *ecm_ipa_ctx = priv;
	int result;
	unsigned int packet_len = skb->len;

	if (!skb) {
		ECM_IPA_ERROR("Bad SKB received from IPA driver\n");
		return;
	}

	ECM_IPA_DEBUG("packet RX, len=%d\n", skb->len);

	if (unlikely(ecm_ipa_ctx->state != ECM_IPA_CONNECTED_AND_UP)) {
		ECM_IPA_DEBUG("Missing pipe connected and/or iface up\n");
		return;
	}

	if (evt != IPA_RECEIVE)	{
		ECM_IPA_ERROR("A none IPA_RECEIVE event in ecm_ipa_receive\n");
		return;
	}

	skb->dev = ecm_ipa_ctx->net;
	skb->protocol = eth_type_trans(skb, ecm_ipa_ctx->net);
	if (rx_filter(skb)) {
		ECM_IPA_DEBUG("packet got filtered out on Rx path\n");
		dev_kfree_skb_any(skb);
		return;
	}

	result = netif_rx_ni(skb);
	if (result)
		ECM_IPA_ERROR("fail on netif_rx_ni\n");
	ecm_ipa_ctx->net->stats.rx_packets++;
	ecm_ipa_ctx->net->stats.rx_bytes += packet_len;

	return;
}

/** ecm_ipa_stop() - called when network device transitions to the down
 *     state.
 *  @net: the network device being stopped.
 *
 * This API is used by Linux network stack to notify the network driver that
 * its state was changed to "down"
 * The driver will stop the "send" queue and change its internal
 * state to "Connected".
 */
static int ecm_ipa_stop(struct net_device *net)
{
	struct ecm_ipa_dev *ecm_ipa_ctx = netdev_priv(net);
	int next_state;

	ECM_IPA_LOG_ENTRY();

	next_state = ecm_ipa_next_state(ecm_ipa_ctx->state, ECM_IPA_STOP);
	if (next_state == ECM_IPA_INVALID) {
		ECM_IPA_ERROR("can't do network interface down without up\n");
		return -EPERM;
	}
	ecm_ipa_ctx->state = next_state;
	ECM_IPA_STATE_DEBUG(ecm_ipa_ctx);

	netif_stop_queue(net);
	ECM_IPA_DEBUG("network device stopped\n");

	ECM_IPA_LOG_EXIT();
	return 0;
}

/** ecm_ipa_disconnect() - called when the USB cable is unplugged.
 * @priv: same value that was set by ecm_ipa_init(), this
 *  parameter holds the network device pointer.
 *
 * Once the USB cable is unplugged the USB driver will notify the network
 * interface driver.
 * The internal driver state will returned to its initialized state and
 * Linux network stack will be informed for carrier off and the send queue
 * will be stopped.
 */
int ecm_ipa_disconnect(void *priv)
{
	struct ecm_ipa_dev *ecm_ipa_ctx = priv;
	int next_state;
	struct ipa_ecm_msg *ecm_msg;
	struct ipa_msg_meta msg_meta;
	int retval;
	int outstanding_dropped_pkts;

	ECM_IPA_LOG_ENTRY();
	NULL_CHECK(ecm_ipa_ctx);
	ECM_IPA_DEBUG("priv=0x%p\n", priv);

	next_state = ecm_ipa_next_state(ecm_ipa_ctx->state, ECM_IPA_DISCONNECT);
	if (next_state == ECM_IPA_INVALID) {
		ECM_IPA_ERROR("can't disconnect before connect\n");
		return -EPERM;
	}
	ecm_ipa_ctx->state = next_state;
	ECM_IPA_STATE_DEBUG(ecm_ipa_ctx);

	netif_carrier_off(ecm_ipa_ctx->net);
	ECM_IPA_DEBUG("carrier_off notifcation was sent\n");

	ecm_msg = kzalloc(sizeof(struct ipa_ecm_msg), GFP_KERNEL);
	if (!ecm_msg) {
		ECM_IPA_ERROR("can't alloc msg mem\n");
		return -ENOMEM;
	}

	memset(&msg_meta, 0, sizeof(struct ipa_msg_meta));
	msg_meta.msg_type = ECM_DISCONNECT;
	msg_meta.msg_len = sizeof(struct ipa_ecm_msg);
	strlcpy(ecm_msg->name, ecm_ipa_ctx->net->name,
		IPA_RESOURCE_NAME_MAX);
	ecm_msg->ifindex = ecm_ipa_ctx->net->ifindex;

	retval = ipa_send_msg(&msg_meta, ecm_msg, ecm_ipa_msg_free_cb);
	if (retval) {
		ECM_IPA_ERROR("fail to send ECM_DISCONNECT message\n");
		kfree(ecm_msg);
		return -EPERM;
	}

	netif_stop_queue(ecm_ipa_ctx->net);
	ECM_IPA_DEBUG("queue stopped\n");

	ecm_ipa_destory_rm_resource(ecm_ipa_ctx);

	outstanding_dropped_pkts =
		atomic_read(&ecm_ipa_ctx->outstanding_pkts);
	ecm_ipa_ctx->net->stats.tx_errors += outstanding_dropped_pkts;
	atomic_set(&ecm_ipa_ctx->outstanding_pkts, 0);

	ECM_IPA_INFO("ECM_IPA was disconnected successfully\n");

	ECM_IPA_LOG_EXIT();

	return 0;
}
EXPORT_SYMBOL(ecm_ipa_disconnect);


/**
 * ecm_ipa_cleanup() - unregister the network interface driver and free
 *  internal data structs.
 * @priv: same value that was set by ecm_ipa_init(), this
 *   parameter holds the network device pointer.
 *
 * This function shall be called once the network interface is not
 * needed anymore, e.g: when the USB composition does not support ECM.
 * This function shall be called after the pipes were disconnected.
 * Detailed description:
 *  - delete the driver dependency defined for IPA resource manager and
 *   destroy the producer resource.
 *  -  remove the debugfs entries
 *  - deregister the network interface from Linux network stack
 *  - free all internal data structs
 */
void ecm_ipa_cleanup(void *priv)
{
	struct ecm_ipa_dev *ecm_ipa_ctx = priv;
	int next_state;

	ECM_IPA_LOG_ENTRY();

	ECM_IPA_DEBUG("priv=0x%p\n", priv);

	if (!ecm_ipa_ctx) {
		ECM_IPA_ERROR("ecm_ipa_ctx NULL pointer\n");
		return;
	}

	next_state = ecm_ipa_next_state(ecm_ipa_ctx->state, ECM_IPA_CLEANUP);
	if (next_state == ECM_IPA_INVALID) {
		ECM_IPA_ERROR("can't clean driver without cable disconnect\n");
		return;
	}
	ecm_ipa_ctx->state = next_state;
	ECM_IPA_STATE_DEBUG(ecm_ipa_ctx);

	ecm_ipa_rules_destroy(ecm_ipa_ctx);
	ecm_ipa_debugfs_destroy(ecm_ipa_ctx);

	unregister_netdev(ecm_ipa_ctx->net);
	free_netdev(ecm_ipa_ctx->net);

	ECM_IPA_INFO("ECM_IPA was destroyed successfully\n");

	ECM_IPA_LOG_EXIT();

	return;
}
EXPORT_SYMBOL(ecm_ipa_cleanup);

static void ecm_ipa_enable_data_path(struct ecm_ipa_dev *ecm_ipa_ctx)
{
	if (ecm_ipa_ctx->device_ready_notify) {
		ecm_ipa_ctx->device_ready_notify();
		ECM_IPA_DEBUG("USB device_ready_notify() was called\n");
	} else {
		ECM_IPA_DEBUG("device_ready_notify() not supplied\n");
	}

	netif_start_queue(ecm_ipa_ctx->net);
	ECM_IPA_DEBUG("queue started\n");
}

/**
 * ecm_ipa_rules_cfg() - set header insertion and register Tx/Rx properties
 *				Headers will be commited to HW
 * @ecm_ipa_ctx: main driver context parameters
 * @dst_mac: destination MAC address
 * @src_mac: source MAC address
 *
 * Returns negative errno, or zero on success
 */
static int ecm_ipa_rules_cfg(struct ecm_ipa_dev *ecm_ipa_ctx,
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
	ipv4_hdr->is_eth2_ofst_valid = true;
	ipv4_hdr->eth2_ofst = 0;
	ipv4_hdr->type = IPA_HDR_L2_ETHERNET_II;
	strlcpy(ipv6_hdr->name, ECM_IPA_IPV6_HDR_NAME, IPA_RESOURCE_NAME_MAX);
	memcpy(eth_ipv6->h_dest, dst_mac, ETH_ALEN);
	memcpy(eth_ipv6->h_source, src_mac, ETH_ALEN);
	eth_ipv6->h_proto = htons(ETH_P_IPV6);
	ipv6_hdr->hdr_len = ETH_HLEN;
	ipv6_hdr->is_partial = 0;
	ipv6_hdr->is_eth2_ofst_valid = true;
	ipv6_hdr->eth2_ofst = 0;
	ipv6_hdr->type = IPA_HDR_L2_ETHERNET_II;
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
	ecm_ipa_ctx->eth_ipv4_hdr_hdl = ipv4_hdr->hdr_hdl;
	ecm_ipa_ctx->eth_ipv6_hdr_hdl = ipv6_hdr->hdr_hdl;
	ECM_IPA_LOG_EXIT();
out_free_mem:
	kfree(hdrs);
out:
	return result;
}

/**
 * ecm_ipa_rules_destroy() - remove the IPA core configuration done for
 *  the driver data path.
 *  @ecm_ipa_ctx: the driver context
 *
 *  Revert the work done on ecm_ipa_rules_cfg.
 */
static void ecm_ipa_rules_destroy(struct ecm_ipa_dev *ecm_ipa_ctx)
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
	ipv4->hdl = ecm_ipa_ctx->eth_ipv4_hdr_hdl;
	ipv6 = &del_hdr->hdl[1];
	ipv6->hdl = ecm_ipa_ctx->eth_ipv6_hdr_hdl;
	result = ipa_del_hdr(del_hdr);
	if (result || ipv4->status || ipv6->status)
		ECM_IPA_ERROR("ipa_del_hdr failed\n");
	kfree(del_hdr);
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
static int ecm_ipa_register_properties(struct ecm_ipa_dev *ecm_ipa_ctx)
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
	ipv4_property->dst_pipe = ecm_ipa_ctx->ipa_to_usb_client;
	strlcpy(ipv4_property->hdr_name, ECM_IPA_IPV4_HDR_NAME,
			IPA_RESOURCE_NAME_MAX);
	ipv4_property->hdr_l2_type = IPA_HDR_L2_ETHERNET_II;
	ipv6_property = &tx_properties.prop[1];
	ipv6_property->ip = IPA_IP_v6;
	ipv6_property->dst_pipe = ecm_ipa_ctx->ipa_to_usb_client;
	ipv6_property->hdr_l2_type = IPA_HDR_L2_ETHERNET_II;
	strlcpy(ipv6_property->hdr_name, ECM_IPA_IPV6_HDR_NAME,
			IPA_RESOURCE_NAME_MAX);
	tx_properties.num_props = 2;

	rx_properties.prop = rx_ioc_properties;
	rx_ipv4_property = &rx_properties.prop[0];
	rx_ipv4_property->ip = IPA_IP_v4;
	rx_ipv4_property->attrib.attrib_mask = 0;
	rx_ipv4_property->src_pipe = ecm_ipa_ctx->usb_to_ipa_client;
	rx_ipv4_property->hdr_l2_type = IPA_HDR_L2_ETHERNET_II;
	rx_ipv6_property = &rx_properties.prop[1];
	rx_ipv6_property->ip = IPA_IP_v6;
	rx_ipv6_property->attrib.attrib_mask = 0;
	rx_ipv6_property->src_pipe = ecm_ipa_ctx->usb_to_ipa_client;
	rx_ipv6_property->hdr_l2_type = IPA_HDR_L2_ETHERNET_II;
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


static void ecm_ipa_rm_notify(void *user_data, enum ipa_rm_event event,
		unsigned long data)
{
	struct ecm_ipa_dev *ecm_ipa_ctx = user_data;
	ECM_IPA_LOG_ENTRY();
	if (event == IPA_RM_RESOURCE_GRANTED &&
			netif_queue_stopped(ecm_ipa_ctx->net)) {
		ECM_IPA_DEBUG("Resource Granted - starting queue\n");
		netif_start_queue(ecm_ipa_ctx->net);
	} else {
		ECM_IPA_DEBUG("Resource released\n");
	}
	ECM_IPA_LOG_EXIT();
}

static struct net_device_stats *ecm_ipa_get_stats(struct net_device *net)
{
	return &net->stats;
}


static int ecm_ipa_create_rm_resource(struct ecm_ipa_dev *ecm_ipa_ctx)
{
	struct ipa_rm_create_params create_params = {0};
	struct ipa_rm_perf_profile profile;
	int result;
	ECM_IPA_LOG_ENTRY();
	create_params.name = IPA_RM_RESOURCE_STD_ECM_PROD;
	create_params.reg_params.user_data = ecm_ipa_ctx;
	create_params.reg_params.notify_cb = ecm_ipa_rm_notify;
	result = ipa_rm_create_resource(&create_params);
	if (result) {
		ECM_IPA_ERROR("Fail on ipa_rm_create_resource\n");
		goto fail_rm_create;
	}
	ECM_IPA_DEBUG("rm client was created");

	profile.max_supported_bandwidth_mbps = IPA_APPS_MAX_BW_IN_MBPS;
	ipa_rm_set_perf_profile(IPA_RM_RESOURCE_STD_ECM_PROD, &profile);

	result = ipa_rm_inactivity_timer_init(IPA_RM_RESOURCE_STD_ECM_PROD,
			INACTIVITY_MSEC_DELAY);
	if (result) {
		ECM_IPA_ERROR("Fail on ipa_rm_inactivity_timer_init\n");
		goto fail_it;
	}
	ECM_IPA_DEBUG("rm_it client was created");

	result = ipa_rm_add_dependency_sync(IPA_RM_RESOURCE_STD_ECM_PROD,
		ecm_ipa_ctx->ipa_rm_resource_name_cons);
	if (result && result != -EINPROGRESS)
		ECM_IPA_ERROR("unable to add ECM/USB dependency (%d)\n",
				result);

	result = ipa_rm_add_dependency_sync(
			ecm_ipa_ctx->ipa_rm_resource_name_prod,
			IPA_RM_RESOURCE_APPS_CONS);
	if (result && result != -EINPROGRESS)
		ECM_IPA_ERROR("unable to add USB/APPS dependency (%d)\n",
				result);

	ECM_IPA_DEBUG("rm dependency was set\n");

	ECM_IPA_LOG_EXIT();
	return 0;

fail_it:
fail_rm_create:
	return result;
}

static void ecm_ipa_destory_rm_resource(struct ecm_ipa_dev *ecm_ipa_ctx)
{
	int result;

	ECM_IPA_LOG_ENTRY();

	ipa_rm_delete_dependency(IPA_RM_RESOURCE_STD_ECM_PROD,
				 ecm_ipa_ctx->ipa_rm_resource_name_cons);
	ipa_rm_delete_dependency(ecm_ipa_ctx->ipa_rm_resource_name_prod,
				 IPA_RM_RESOURCE_APPS_CONS);
	ipa_rm_inactivity_timer_destroy(IPA_RM_RESOURCE_STD_ECM_PROD);
	result = ipa_rm_delete_resource(IPA_RM_RESOURCE_STD_ECM_PROD);
	if (result)
		ECM_IPA_ERROR("resource deletion failed\n");

	ECM_IPA_LOG_EXIT();
}

static bool rx_filter(struct sk_buff *skb)
{
	struct ecm_ipa_dev *ecm_ipa_ctx = netdev_priv(skb->dev);
	return !ecm_ipa_ctx->rx_enable;
}

static bool tx_filter(struct sk_buff *skb)
{
	struct ecm_ipa_dev *ecm_ipa_ctx = netdev_priv(skb->dev);
	return !ecm_ipa_ctx->tx_enable;
}

static bool rm_enabled(struct ecm_ipa_dev *ecm_ipa_ctx)
{
	return ecm_ipa_ctx->rm_enable;
}

static int resource_request(struct ecm_ipa_dev *ecm_ipa_ctx)
{
	int result = 0;

	if (!rm_enabled(ecm_ipa_ctx))
		goto out;
	result = ipa_rm_inactivity_timer_request_resource(
			IPA_RM_RESOURCE_STD_ECM_PROD);
out:
	return result;
}

static void resource_release(struct ecm_ipa_dev *ecm_ipa_ctx)
{
	if (!rm_enabled(ecm_ipa_ctx))
		goto out;
	ipa_rm_inactivity_timer_release_resource(IPA_RM_RESOURCE_STD_ECM_PROD);
out:
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
static void ecm_ipa_tx_complete_notify(void *priv,
		enum ipa_dp_evt_type evt,
		unsigned long data)
{
	struct sk_buff *skb = (struct sk_buff *)data;
	struct ecm_ipa_dev *ecm_ipa_ctx = priv;

	if (!skb) {
		ECM_IPA_ERROR("Bad SKB received from IPA driver\n");
		return;
	}

	if (!ecm_ipa_ctx) {
		ECM_IPA_ERROR("ecm_ipa_ctx is NULL pointer\n");
		return;
	}

	ECM_IPA_DEBUG("Tx-complete, len=%d, skb->prot=%d, outstanding=%d\n",
			skb->len, skb->protocol,
			atomic_read(&ecm_ipa_ctx->outstanding_pkts));

	if (evt != IPA_WRITE_DONE) {
		ECM_IPA_ERROR("unsupported event on Tx callback\n");
		return;
	}

	if (unlikely(ecm_ipa_ctx->state != ECM_IPA_CONNECTED_AND_UP)) {
		ECM_IPA_DEBUG("dropping Tx-complete pkt, state=%s",
			ecm_ipa_state_string(ecm_ipa_ctx->state));
		goto out;
	}

	ecm_ipa_ctx->net->stats.tx_packets++;
	ecm_ipa_ctx->net->stats.tx_bytes += skb->len;

	atomic_dec(&ecm_ipa_ctx->outstanding_pkts);
	if (netif_queue_stopped(ecm_ipa_ctx->net) &&
		netif_carrier_ok(ecm_ipa_ctx->net) &&
		atomic_read(&ecm_ipa_ctx->outstanding_pkts) <
					(ecm_ipa_ctx->outstanding_low)) {
		ECM_IPA_DEBUG("outstanding low (%d) - waking up queue\n",
				ecm_ipa_ctx->outstanding_low);
		netif_wake_queue(ecm_ipa_ctx->net);
	}

out:
	dev_kfree_skb_any(skb);
	return;
}

static void ecm_ipa_tx_timeout(struct net_device *net)
{
	struct ecm_ipa_dev *ecm_ipa_ctx = netdev_priv(net);

	ECM_IPA_ERROR("possible IPA stall was detected, %d outstanding",
		atomic_read(&ecm_ipa_ctx->outstanding_pkts));

	net->stats.tx_errors++;
}

static int ecm_ipa_debugfs_stall_open(struct inode *inode,
	struct file *file)
{
	ECM_IPA_LOG_ENTRY();

	ECM_IPA_LOG_EXIT();

	return 0;
}

static ssize_t ecm_ipa_debugfs_stall_write(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos)
{
	u32 cmdq_cfg_mmio_phy = 0xFD4E3038;
	void *cmdq_cfg_mmio_virt;
	int result;
	bool val = 0;

	ECM_IPA_LOG_ENTRY();

	file->private_data = &val;
	result = ecm_ipa_debugfs_enable_write(file, buf, count, ppos);

	cmdq_cfg_mmio_virt = ioremap(cmdq_cfg_mmio_phy, sizeof(u32));
	if (!cmdq_cfg_mmio_virt) {
		ECM_IPA_ERROR("fail on mmio for cmdq_cfg_mmio_phy=0x%x",
			cmdq_cfg_mmio_phy);
		return result;
	}

	iowrite32(val, cmdq_cfg_mmio_virt);
	ECM_IPA_DEBUG("Value %d was written to cfgq", val);

	ECM_IPA_LOG_EXIT();

	return result;

}

static int ecm_ipa_debugfs_atomic_open(struct inode *inode, struct file *file)
{
	struct ecm_ipa_dev *ecm_ipa_ctx = inode->i_private;
	ECM_IPA_LOG_ENTRY();
	file->private_data = &(ecm_ipa_ctx->outstanding_pkts);
	ECM_IPA_LOG_EXIT();
	return 0;
}

static ssize_t ecm_ipa_debugfs_enable_write_dma(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos)
{
	struct ecm_ipa_dev *ecm_ipa_ctx = file->private_data;
	int result;
	ECM_IPA_LOG_ENTRY();
	file->private_data = &ecm_ipa_ctx->dma_enable;
	result = ecm_ipa_debugfs_enable_write(file, buf, count, ppos);
	if (ecm_ipa_ctx->dma_enable)
		ecm_ipa_ep_registers_dma_cfg(ecm_ipa_ctx->usb_to_ipa_hdl,
					     ecm_ipa_ctx->ipa_to_usb_client);
	else
		ecm_ipa_ep_registers_cfg(ecm_ipa_ctx->usb_to_ipa_hdl,
				ecm_ipa_ctx->usb_to_ipa_hdl);
	ECM_IPA_LOG_EXIT();
	return result;
}

static int ecm_ipa_debugfs_dma_open(struct inode *inode, struct file *file)
{
	struct ecm_ipa_dev *ecm_ipa_ctx = inode->i_private;
	ECM_IPA_LOG_ENTRY();
	file->private_data = ecm_ipa_ctx;
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
		ECM_IPA_ERROR("simple_read_from_buffer problem\n");
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


static int ecm_ipa_debugfs_init(struct ecm_ipa_dev *ecm_ipa_ctx)
{
	const mode_t flags_read_write = S_IRUGO | S_IWUGO;
	const mode_t flags_read_only = S_IRUGO;
	const mode_t flags_write_only = S_IWUGO;
	struct dentry *file;

	ECM_IPA_LOG_ENTRY();

	if (!ecm_ipa_ctx)
		return -EINVAL;

	ecm_ipa_ctx->directory = debugfs_create_dir("ecm_ipa", NULL);
	if (!ecm_ipa_ctx->directory) {
		ECM_IPA_ERROR("could not create debugfs directory entry\n");
		goto fail_directory;
	}
	file = debugfs_create_bool("tx_enable", flags_read_write,
			ecm_ipa_ctx->directory, &ecm_ipa_ctx->tx_enable);
	if (!file) {
		ECM_IPA_ERROR("could not create debugfs tx file\n");
		goto fail_file;
	}
	file = debugfs_create_bool("rx_enable", flags_read_write,
			ecm_ipa_ctx->directory, &ecm_ipa_ctx->rx_enable);
	if (!file) {
		ECM_IPA_ERROR("could not create debugfs rx file\n");
		goto fail_file;
	}
	file = debugfs_create_bool("rm_enable", flags_read_write,
			ecm_ipa_ctx->directory, &ecm_ipa_ctx->rm_enable);
	if (!file) {
		ECM_IPA_ERROR("could not create debugfs rm file\n");
		goto fail_file;
	}
	file = debugfs_create_u8("outstanding_high", flags_read_write,
			ecm_ipa_ctx->directory, &ecm_ipa_ctx->outstanding_high);
	if (!file) {
		ECM_IPA_ERROR("could not create outstanding_high file\n");
		goto fail_file;
	}
	file = debugfs_create_u8("outstanding_low", flags_read_write,
			ecm_ipa_ctx->directory, &ecm_ipa_ctx->outstanding_low);
	if (!file) {
		ECM_IPA_ERROR("could not create outstanding_low file\n");
		goto fail_file;
	}
	file = debugfs_create_file("dma_enable", flags_read_write,
			ecm_ipa_ctx->directory,
			ecm_ipa_ctx, &ecm_ipa_debugfs_dma_ops);
	if (!file) {
		ECM_IPA_ERROR("could not create debugfs dma file\n");
		goto fail_file;
	}
	file = debugfs_create_file("outstanding", flags_read_only,
			ecm_ipa_ctx->directory,
			ecm_ipa_ctx, &ecm_ipa_debugfs_atomic_ops);
	if (!file) {
		ECM_IPA_ERROR("could not create outstanding file\n");
		goto fail_file;
	}

	file = debugfs_create_file("stall_ipa_rx_proc", flags_write_only,
			ecm_ipa_ctx->directory,
			ecm_ipa_ctx, &ecm_ipa_debugfs_stall_ops);
	if (!file) {
		ECM_IPA_ERROR("could not create stall_ipa_rx_proc file\n");
		goto fail_file;
	}

	ECM_IPA_LOG_EXIT();

	return 0;
fail_file:
	debugfs_remove_recursive(ecm_ipa_ctx->directory);
fail_directory:
	return -EFAULT;
}

static void ecm_ipa_debugfs_destroy(struct ecm_ipa_dev *ecm_ipa_ctx)
{
	debugfs_remove_recursive(ecm_ipa_ctx->directory);
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
static int ecm_ipa_ep_registers_cfg(u32 usb_to_ipa_hdl, u32 ipa_to_usb_hdl)
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
static int ecm_ipa_ep_registers_dma_cfg(u32 usb_to_ipa_hdl,
					enum ipa_client_type prod_client)
{
	int result = 0;
	struct ipa_ep_cfg_mode cfg_mode;
	u32 apps_to_ipa_hdl = 2;

	ECM_IPA_LOG_ENTRY();

	memset(&cfg_mode, 0 , sizeof(cfg_mode));
	cfg_mode.mode = IPA_DMA;
	cfg_mode.dst = prod_client;
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
static int ecm_ipa_set_device_ethernet_addr(u8 *dev_ethaddr,
		u8 device_ethaddr[])
{
	if (!is_valid_ether_addr(device_ethaddr))
		return -EINVAL;
	memcpy(dev_ethaddr, device_ethaddr, ETH_ALEN);
	ECM_IPA_DEBUG("device ethernet address: %pM\n", dev_ethaddr);
	return 0;
}

/** ecm_ipa_next_state - return the next state of the driver
 * @current_state: the current state of the driver
 * @operation: an enum which represent the operation being made on the driver
 *  by its API.
 *
 * This function implements the driver internal state machine.
 * Its decisions are based on the driver current state and the operation
 * being made.
 * In case the operation is invalid this state machine will return
 * the value ECM_IPA_INVALID to inform the caller for a forbidden sequence.
 */
static enum ecm_ipa_state ecm_ipa_next_state(enum ecm_ipa_state current_state,
		enum ecm_ipa_operation operation)
{
	int next_state = ECM_IPA_INVALID;

	switch (current_state) {
	case ECM_IPA_UNLOADED:
		if (operation == ECM_IPA_INITIALIZE)
			next_state = ECM_IPA_INITIALIZED;
		break;
	case ECM_IPA_INITIALIZED:
		if (operation == ECM_IPA_CONNECT)
			next_state = ECM_IPA_CONNECTED;
		else if (operation == ECM_IPA_OPEN)
			next_state = ECM_IPA_UP;
		else if (operation == ECM_IPA_CLEANUP)
			next_state = ECM_IPA_UNLOADED;
		break;
	case ECM_IPA_CONNECTED:
		if (operation == ECM_IPA_DISCONNECT)
			next_state = ECM_IPA_INITIALIZED;
		else if (operation == ECM_IPA_OPEN)
			next_state = ECM_IPA_CONNECTED_AND_UP;
		break;
	case ECM_IPA_UP:
		if (operation == ECM_IPA_STOP)
			next_state = ECM_IPA_INITIALIZED;
		else if (operation == ECM_IPA_CONNECT)
			next_state = ECM_IPA_CONNECTED_AND_UP;
		else if (operation == ECM_IPA_CLEANUP)
			next_state = ECM_IPA_UNLOADED;
		break;
	case ECM_IPA_CONNECTED_AND_UP:
		if (operation == ECM_IPA_STOP)
			next_state = ECM_IPA_CONNECTED;
		else if (operation == ECM_IPA_DISCONNECT)
			next_state = ECM_IPA_UP;
		break;
	default:
		ECM_IPA_ERROR("State is not supported\n");
		break;
	}

	ECM_IPA_DEBUG("state transition ( %s -> %s )- %s\n",
			ecm_ipa_state_string(current_state),
			ecm_ipa_state_string(next_state) ,
			next_state == ECM_IPA_INVALID ?
					"Forbidden" : "Allowed");

	return next_state;
}

/**
 * ecm_ipa_state_string - return the state string representation
 * @state: enum which describe the state
 */
static const char *ecm_ipa_state_string(enum ecm_ipa_state state)
{
	switch (state) {
	case ECM_IPA_UNLOADED:
		return "ECM_IPA_UNLOADED";
	case ECM_IPA_INITIALIZED:
		return "ECM_IPA_INITIALIZED";
	case ECM_IPA_CONNECTED:
		return "ECM_IPA_CONNECTED";
	case ECM_IPA_UP:
		return "ECM_IPA_UP";
	case ECM_IPA_CONNECTED_AND_UP:
		return "ECM_IPA_CONNECTED_AND_UP";
	default:
		return "Not supported";
	}
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
