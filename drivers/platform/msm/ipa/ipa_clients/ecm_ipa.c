// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/etherdevice.h>
#include <linux/if_vlan.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/sched.h>
#include <linux/atomic.h>
#include <linux/ecm_ipa.h>
#include "../ipa_common_i.h"
#include "../ipa_v3/ipa_pm.h"

#define DRIVER_NAME "ecm_ipa"
#define ECM_IPA_IPV4_HDR_NAME "ecm_eth_ipv4"
#define ECM_IPA_IPV6_HDR_NAME "ecm_eth_ipv6"
#define INACTIVITY_MSEC_DELAY 100
#define DEFAULT_OUTSTANDING_HIGH 64
#define DEFAULT_OUTSTANDING_LOW 32
#define DEBUGFS_TEMP_BUF_SIZE 4
#define TX_TIMEOUT (5 * HZ)

#define IPA_ECM_IPC_LOG_PAGES 50

#define IPA_ECM_IPC_LOGGING(buf, fmt, args...) \
	do { \
		if (buf) \
			ipc_log_string((buf), fmt, __func__, __LINE__, \
				## args); \
	} while (0)

static void *ipa_ecm_logbuf;

#define ECM_IPA_DEBUG(fmt, args...) \
	do { \
		pr_debug(DRIVER_NAME " %s:%d "\
			fmt, __func__, __LINE__, ## args);\
		if (ipa_ecm_logbuf) { \
			IPA_ECM_IPC_LOGGING(ipa_ecm_logbuf, \
				DRIVER_NAME " %s:%d " fmt, ## args); \
		} \
	} while (0)

#define ECM_IPA_DEBUG_XMIT(fmt, args...) \
	pr_debug(DRIVER_NAME " %s:%d " fmt, __func__, __LINE__, ## args)

#define ECM_IPA_INFO(fmt, args...) \
	do { \
		pr_info(DRIVER_NAME "@%s@%d@ctx:%s: "\
			fmt, __func__, __LINE__, current->comm, ## args);\
		if (ipa_ecm_logbuf) { \
			IPA_ECM_IPC_LOGGING(ipa_ecm_logbuf, \
				DRIVER_NAME " %s:%d " fmt, ## args); \
		} \
	} while (0)

#define ECM_IPA_ERROR(fmt, args...) \
	do { \
		pr_err(DRIVER_NAME "@%s@%d@ctx:%s: "\
			fmt, __func__, __LINE__, current->comm, ## args);\
		if (ipa_ecm_logbuf) { \
			IPA_ECM_IPC_LOGGING(ipa_ecm_logbuf, \
				DRIVER_NAME " %s:%d " fmt, ## args); \
		} \
	} while (0)

#define NULL_CHECK(ptr) \
	do { \
		if (!(ptr)) { \
			ECM_IPA_ERROR("null pointer #ptr\n"); \
			ret = -EINVAL; \
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
 * enum ecm_ipa_operation - enumerations used to describe the API operation
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
	ecm_ipa_state_string((ecm_ipa_ctx)->state))

/**
 * struct ecm_ipa_dev - main driver context parameters
 * @net: network interface struct implemented by this driver
 * @directory: debugfs directory for various debuging switches
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
 * @pm_hdl: handle for IPA PM
 * @is_vlan_mode: does the driver need to work in VLAN mode?
 * @netif_rx_function: holds the correct network stack API, needed for NAPI
 */
struct ecm_ipa_dev {
	struct net_device *net;
	struct dentry *directory;
	u32 eth_ipv4_hdr_hdl;
	u32 eth_ipv6_hdr_hdl;
	u32 usb_to_ipa_hdl;
	u32 ipa_to_usb_hdl;
	atomic_t outstanding_pkts;
	u8 outstanding_high;
	u8 outstanding_low;
	enum ecm_ipa_state state;
	void (*device_ready_notify)(void);
	enum ipa_client_type ipa_to_usb_client;
	enum ipa_client_type usb_to_ipa_client;
	u32 pm_hdl;
	bool is_vlan_mode;
	int (*netif_rx_function)(struct sk_buff *skb);
};

static int ecm_ipa_open(struct net_device *net);
static void ecm_ipa_packet_receive_notify
	(void *priv, enum ipa_dp_evt_type evt, unsigned long data);
static void ecm_ipa_tx_complete_notify
	(void *priv, enum ipa_dp_evt_type evt, unsigned long data);
static void ecm_ipa_tx_timeout(struct net_device *net);
static int ecm_ipa_stop(struct net_device *net);
static void ecm_ipa_enable_data_path(struct ecm_ipa_dev *ecm_ipa_ctx);
static int ecm_ipa_rules_cfg
	(struct ecm_ipa_dev *ecm_ipa_ctx, const void *dst_mac,
		const void *src_mac);
static void ecm_ipa_rules_destroy(struct ecm_ipa_dev *ecm_ipa_ctx);
static int ecm_ipa_register_properties(struct ecm_ipa_dev *ecm_ipa_ctx);
static void ecm_ipa_deregister_properties(void);
static struct net_device_stats *ecm_ipa_get_stats(struct net_device *net);
static int ecm_ipa_register_pm_client(struct ecm_ipa_dev *ecm_ipa_ctx);
static void ecm_ipa_deregister_pm_client(struct ecm_ipa_dev *ecm_ipa_ctx);
static netdev_tx_t ecm_ipa_start_xmit
	(struct sk_buff *skb, struct net_device *net);
static int ecm_ipa_debugfs_atomic_open(struct inode *inode, struct file *file);
static ssize_t ecm_ipa_debugfs_atomic_read
	(struct file *file, char __user *ubuf, size_t count, loff_t *ppos);
static void ecm_ipa_debugfs_init(struct ecm_ipa_dev *ecm_ipa_ctx);
static void ecm_ipa_debugfs_destroy(struct ecm_ipa_dev *ecm_ipa_ctx);
static int ecm_ipa_ep_registers_cfg(u32 usb_to_ipa_hdl, u32 ipa_to_usb_hdl,
	bool is_vlan_mode);
static int ecm_ipa_set_device_ethernet_addr
	(u8 *dev_ethaddr, u8 device_ethaddr[]);
static enum ecm_ipa_state ecm_ipa_next_state
	(enum ecm_ipa_state current_state, enum ecm_ipa_operation operation);
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

const struct file_operations ecm_ipa_debugfs_atomic_ops = {
	.open = ecm_ipa_debugfs_atomic_open,
	.read = ecm_ipa_debugfs_atomic_read,
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
	int ret;

	ECM_IPA_LOG_ENTRY();

	ECM_IPA_DEBUG("%s initializing\n", DRIVER_NAME);
	ret = 0;
	NULL_CHECK(params);
	if (ret)
		return ret;

	ECM_IPA_DEBUG
		("host_ethaddr=%pM, device_ethaddr=%pM\n",
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
	ECM_IPA_DEBUG("ecm_ipa_ctx (private) = %pK\n", ecm_ipa_ctx);

	ecm_ipa_ctx->net = net;
	ecm_ipa_ctx->outstanding_high = DEFAULT_OUTSTANDING_HIGH;
	ecm_ipa_ctx->outstanding_low = DEFAULT_OUTSTANDING_LOW;
	atomic_set(&ecm_ipa_ctx->outstanding_pkts, 0);
	snprintf(net->name, sizeof(net->name), "%s%%d", "ecm");
	net->netdev_ops = &ecm_ipa_netdev_ops;
	net->watchdog_timeo = TX_TIMEOUT;
	if (ipa_get_lan_rx_napi()) {
		ecm_ipa_ctx->netif_rx_function = netif_receive_skb;
		ECM_IPA_DEBUG("LAN RX NAPI enabled = True");
	} else {
		ecm_ipa_ctx->netif_rx_function = netif_rx_ni;
		ECM_IPA_DEBUG("LAN RX NAPI enabled = False");
	}
	ECM_IPA_DEBUG("internal data structures were initialized\n");

	if (!params->device_ready_notify)
		ECM_IPA_DEBUG("device_ready_notify() was not supplied");
	ecm_ipa_ctx->device_ready_notify = params->device_ready_notify;

	ecm_ipa_debugfs_init(ecm_ipa_ctx);

	result = ecm_ipa_set_device_ethernet_addr
		(net->dev_addr, params->device_ethaddr);
	if (result) {
		ECM_IPA_ERROR("set device MAC failed\n");
		goto fail_set_device_ethernet;
	}
	ECM_IPA_DEBUG("Device Ethernet address set %pM\n", net->dev_addr);

	if (ipa_is_vlan_mode(IPA_VLAN_IF_ECM, &ecm_ipa_ctx->is_vlan_mode)) {
		ECM_IPA_ERROR("couldn't acquire vlan mode, is ipa ready?\n");
		goto fail_get_vlan_mode;
	}
	ECM_IPA_DEBUG("is vlan mode %d\n", ecm_ipa_ctx->is_vlan_mode);

	result = ecm_ipa_rules_cfg
		(ecm_ipa_ctx, params->host_ethaddr, params->device_ethaddr);
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
fail_rules_cfg:
fail_get_vlan_mode:
fail_set_device_ethernet:
	ecm_ipa_debugfs_destroy(ecm_ipa_ctx);
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
	int ret;

	ECM_IPA_LOG_ENTRY();
	ret = 0;
	NULL_CHECK(priv);
	if (ret)
		return ret;
	ECM_IPA_DEBUG("usb_to_ipa_hdl = %d, ipa_to_usb_hdl = %d, priv=0x%pK\n",
		      usb_to_ipa_hdl, ipa_to_usb_hdl, priv);

	next_state = ecm_ipa_next_state(ecm_ipa_ctx->state, ECM_IPA_CONNECT);
	if (next_state == ECM_IPA_INVALID) {
		ECM_IPA_ERROR("can't call connect before calling initialize\n");
		return -EPERM;
	}
	ecm_ipa_ctx->state = next_state;
	ECM_IPA_STATE_DEBUG(ecm_ipa_ctx);

	if (!ipa_is_client_handle_valid(usb_to_ipa_hdl)) {
		ECM_IPA_ERROR
			("usb_to_ipa_hdl(%d) is not a valid ipa handle\n",
			usb_to_ipa_hdl);
		return -EINVAL;
	}
	if (!ipa_is_client_handle_valid(ipa_to_usb_hdl)) {
		ECM_IPA_ERROR
			("ipa_to_usb_hdl(%d) is not a valid ipa handle\n",
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

	retval = ecm_ipa_register_pm_client(ecm_ipa_ctx);

	if (retval) {
		ECM_IPA_ERROR("fail register PM client\n");
		return retval;
	}
	ECM_IPA_DEBUG("PM client registered\n");

	retval = ecm_ipa_register_properties(ecm_ipa_ctx);
	if (retval) {
		ECM_IPA_ERROR("fail on properties set\n");
		goto fail_register_pm;
	}
	ECM_IPA_DEBUG("ecm_ipa 2 Tx and 2 Rx properties were registered\n");

	retval = ecm_ipa_ep_registers_cfg(usb_to_ipa_hdl, ipa_to_usb_hdl,
		ecm_ipa_ctx->is_vlan_mode);
	if (retval) {
		ECM_IPA_ERROR("fail on ep cfg\n");
		goto fail;
	}
	ECM_IPA_DEBUG("end-point configured\n");

	netif_carrier_on(ecm_ipa_ctx->net);

	ecm_msg = kzalloc(sizeof(*ecm_msg), GFP_KERNEL);
	if (!ecm_msg) {
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
fail_register_pm:
	ecm_ipa_deregister_pm_client(ecm_ipa_ctx);
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
 * - Outstanding high boundary did not reach.
 *
 * In case all of the above conditions are met, the network driver will
 * send the packet by using the IPA API for Tx.
 * In case the outstanding packet high boundary is reached, the driver will
 * stop the send queue until enough packet were proceeded by the IPA core.
 */
static netdev_tx_t ecm_ipa_start_xmit
	(struct sk_buff *skb, struct net_device *net)
{
	int ret;
	netdev_tx_t status = NETDEV_TX_BUSY;
	struct ecm_ipa_dev *ecm_ipa_ctx = netdev_priv(net);

	netif_trans_update(net);

	ECM_IPA_DEBUG_XMIT
		("Tx, len=%d, skb->protocol=%d, outstanding=%d\n",
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

	ret = ipa_pm_activate(ecm_ipa_ctx->pm_hdl);
	if (unlikely(ret)) {
		ECM_IPA_DEBUG("Failed to activate PM client\n");
		netif_stop_queue(net);
		goto fail_pm_activate;
	}

	if (atomic_read(&ecm_ipa_ctx->outstanding_pkts) >=
					ecm_ipa_ctx->outstanding_high) {
		ECM_IPA_DEBUG
			("outstanding high (%d)- stopping\n",
			ecm_ipa_ctx->outstanding_high);
		netif_stop_queue(net);
		status = NETDEV_TX_BUSY;
		goto out;
	}

	if (ecm_ipa_ctx->is_vlan_mode)
		if (unlikely(skb->protocol != htons(ETH_P_8021Q)))
			ECM_IPA_DEBUG(
				"ether_type != ETH_P_8021Q && vlan, prot = 0x%X\n"
				, skb->protocol);

	ret = ipa_tx_dp(ecm_ipa_ctx->ipa_to_usb_client, skb, NULL);
	if (unlikely(ret)) {
		ECM_IPA_ERROR("ipa transmit failed (%d)\n", ret);
		goto fail_tx_packet;
	}

	atomic_inc(&ecm_ipa_ctx->outstanding_pkts);

	status = NETDEV_TX_OK;
	goto out;

fail_tx_packet:
out:
	ipa_pm_deferred_deactivate(ecm_ipa_ctx->pm_hdl);
fail_pm_activate:
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
static void ecm_ipa_packet_receive_notify
	(void *priv, enum ipa_dp_evt_type evt, unsigned long data)
{
	struct sk_buff *skb = (struct sk_buff *)data;
	struct ecm_ipa_dev *ecm_ipa_ctx = priv;
	int result;
	unsigned int packet_len;

	if (unlikely(!skb)) {
		ECM_IPA_ERROR("Bad SKB received from IPA driver\n");
		return;
	}

	packet_len = skb->len;
	ECM_IPA_DEBUG("packet RX, len=%d\n", skb->len);

	if (unlikely(ecm_ipa_ctx == NULL)) {
		ECM_IPA_DEBUG("Private context is NULL. Drop SKB.\n");
		dev_kfree_skb_any(skb);
		return;
	}

	if (unlikely(ecm_ipa_ctx->state != ECM_IPA_CONNECTED_AND_UP)) {
		ECM_IPA_DEBUG("Missing pipe connected and/or iface up\n");
		dev_kfree_skb_any(skb);
		return;
	}

	if (unlikely(evt != IPA_RECEIVE)) {
		ECM_IPA_ERROR("A none IPA_RECEIVE event in ecm_ipa_receive\n");
		dev_kfree_skb_any(skb);
		return;
	}

	skb->dev = ecm_ipa_ctx->net;
	skb->protocol = eth_type_trans(skb, ecm_ipa_ctx->net);

	result = ecm_ipa_ctx->netif_rx_function(skb);
	if (unlikely(result))
		ECM_IPA_ERROR("fail on netif_rx_function\n");
	ecm_ipa_ctx->net->stats.rx_packets++;
	ecm_ipa_ctx->net->stats.rx_bytes += packet_len;
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
	int ret;

	ECM_IPA_LOG_ENTRY();
	ret = 0;
	NULL_CHECK(ecm_ipa_ctx);
	if (ret)
		return ret;
	ECM_IPA_DEBUG("priv=0x%pK\n", priv);

	next_state = ecm_ipa_next_state(ecm_ipa_ctx->state, ECM_IPA_DISCONNECT);
	if (next_state == ECM_IPA_INVALID) {
		ECM_IPA_ERROR("can't disconnect before connect\n");
		return -EPERM;
	}
	ecm_ipa_ctx->state = next_state;
	ECM_IPA_STATE_DEBUG(ecm_ipa_ctx);

	netif_carrier_off(ecm_ipa_ctx->net);
	ECM_IPA_DEBUG("carrier_off notifcation was sent\n");

	ecm_msg = kzalloc(sizeof(*ecm_msg), GFP_KERNEL);
	if (!ecm_msg)
		return -ENOMEM;

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

	ecm_ipa_deregister_pm_client(ecm_ipa_ctx);

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
 *  -  remove the debugfs entries
 *  - deregister the network interface from Linux network stack
 *  - free all internal data structs
 */
void ecm_ipa_cleanup(void *priv)
{
	struct ecm_ipa_dev *ecm_ipa_ctx = priv;
	int next_state;

	ECM_IPA_LOG_ENTRY();

	ECM_IPA_DEBUG("priv=0x%pK\n", priv);

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

	ECM_IPA_DEBUG("ECM_IPA unregister_netdev started\n");
	unregister_netdev(ecm_ipa_ctx->net);
	ECM_IPA_DEBUG("ECM_IPA unregister_netdev completed\n");
	free_netdev(ecm_ipa_ctx->net);

	ECM_IPA_INFO("ECM_IPA was destroyed successfully\n");

	ECM_IPA_LOG_EXIT();
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

static void ecm_ipa_prepare_header_insertion(
	int eth_type,
	const char *hdr_name, struct ipa_hdr_add *add_hdr,
	const void *dst_mac, const void *src_mac, bool is_vlan_mode)
{
	struct ethhdr *eth_hdr;
	struct vlan_ethhdr *eth_vlan_hdr;

	ECM_IPA_LOG_ENTRY();

	add_hdr->is_partial = 0;
	strlcpy(add_hdr->name, hdr_name, IPA_RESOURCE_NAME_MAX);
	add_hdr->is_eth2_ofst_valid = true;
	add_hdr->eth2_ofst = 0;

	if (is_vlan_mode) {
		eth_vlan_hdr = (struct vlan_ethhdr *)add_hdr->hdr;
		memcpy(eth_vlan_hdr->h_dest, dst_mac, ETH_ALEN);
		memcpy(eth_vlan_hdr->h_source, src_mac, ETH_ALEN);
		eth_vlan_hdr->h_vlan_encapsulated_proto =
			htons(eth_type);
		eth_vlan_hdr->h_vlan_proto = htons(ETH_P_8021Q);
		add_hdr->hdr_len = VLAN_ETH_HLEN;
		add_hdr->type = IPA_HDR_L2_802_1Q;
	} else {
		eth_hdr = (struct ethhdr *)add_hdr->hdr;
		memcpy(eth_hdr->h_dest, dst_mac, ETH_ALEN);
		memcpy(eth_hdr->h_source, src_mac, ETH_ALEN);
		eth_hdr->h_proto = htons(eth_type);
		add_hdr->hdr_len = ETH_HLEN;
		add_hdr->type = IPA_HDR_L2_ETHERNET_II;
	}
	ECM_IPA_LOG_EXIT();
}

/**
 * ecm_ipa_rules_cfg() - set header insertion and register Tx/Rx properties
 *				Headers will be committed to HW
 * @ecm_ipa_ctx: main driver context parameters
 * @dst_mac: destination MAC address
 * @src_mac: source MAC address
 *
 * Returns negative errno, or zero on success
 */
static int ecm_ipa_rules_cfg
	(struct ecm_ipa_dev *ecm_ipa_ctx,
	const void *dst_mac, const void *src_mac)
{
	struct ipa_ioc_add_hdr *hdrs;
	struct ipa_hdr_add *ipv4_hdr;
	struct ipa_hdr_add *ipv6_hdr;
	int result = 0;

	ECM_IPA_LOG_ENTRY();
	hdrs = kzalloc
		(sizeof(*hdrs) + sizeof(*ipv4_hdr) + sizeof(*ipv6_hdr),
			GFP_KERNEL);
	if (!hdrs) {
		result = -ENOMEM;
		goto out;
	}

	ipv4_hdr = &hdrs->hdr[0];
	ecm_ipa_prepare_header_insertion(
		ETH_P_IP, ECM_IPA_IPV4_HDR_NAME,
		ipv4_hdr, dst_mac, src_mac, ecm_ipa_ctx->is_vlan_mode);

	ipv6_hdr = &hdrs->hdr[1];
	ecm_ipa_prepare_header_insertion(
		ETH_P_IPV6, ECM_IPA_IPV6_HDR_NAME,
		ipv6_hdr, dst_mac, src_mac, ecm_ipa_ctx->is_vlan_mode);

	hdrs->commit = 1;
	hdrs->num_hdrs = 2;
	result = ipa_add_hdr(hdrs);
	if (result) {
		ECM_IPA_ERROR("Fail on Header-Insertion(%d)\n", result);
		goto out_free_mem;
	}
	if (ipv4_hdr->status) {
		ECM_IPA_ERROR
			("Fail on Header-Insertion ipv4(%d)\n",
			ipv4_hdr->status);
		result = ipv4_hdr->status;
		goto out_free_mem;
	}
	if (ipv6_hdr->status) {
		ECM_IPA_ERROR
			("Fail on Header-Insertion ipv6(%d)\n",
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
	enum ipa_hdr_l2_type hdr_l2_type = IPA_HDR_L2_ETHERNET_II;
	int result = 0;

	ECM_IPA_LOG_ENTRY();

	if (ecm_ipa_ctx->is_vlan_mode)
		hdr_l2_type = IPA_HDR_L2_802_1Q;

	tx_properties.prop = properties;
	ipv4_property = &tx_properties.prop[0];
	ipv4_property->ip = IPA_IP_v4;
	ipv4_property->dst_pipe = ecm_ipa_ctx->ipa_to_usb_client;
	strlcpy
		(ipv4_property->hdr_name, ECM_IPA_IPV4_HDR_NAME,
		IPA_RESOURCE_NAME_MAX);
	ipv4_property->hdr_l2_type = hdr_l2_type;
	ipv6_property = &tx_properties.prop[1];
	ipv6_property->ip = IPA_IP_v6;
	ipv6_property->dst_pipe = ecm_ipa_ctx->ipa_to_usb_client;
	ipv6_property->hdr_l2_type = hdr_l2_type;
	strlcpy
		(ipv6_property->hdr_name, ECM_IPA_IPV6_HDR_NAME,
		IPA_RESOURCE_NAME_MAX);
	tx_properties.num_props = 2;

	rx_properties.prop = rx_ioc_properties;
	rx_ipv4_property = &rx_properties.prop[0];
	rx_ipv4_property->ip = IPA_IP_v4;
	rx_ipv4_property->attrib.attrib_mask = 0;
	rx_ipv4_property->src_pipe = ecm_ipa_ctx->usb_to_ipa_client;
	rx_ipv4_property->hdr_l2_type = hdr_l2_type;
	rx_ipv6_property = &rx_properties.prop[1];
	rx_ipv6_property->ip = IPA_IP_v6;
	rx_ipv6_property->attrib.attrib_mask = 0;
	rx_ipv6_property->src_pipe = ecm_ipa_ctx->usb_to_ipa_client;
	rx_ipv6_property->hdr_l2_type = hdr_l2_type;
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

static struct net_device_stats *ecm_ipa_get_stats(struct net_device *net)
{
	return &net->stats;
}

static void ecm_ipa_pm_cb(void *p, enum ipa_pm_cb_event event)
{
	struct ecm_ipa_dev *ecm_ipa_ctx = p;

	ECM_IPA_LOG_ENTRY();
	if (event != IPA_PM_CLIENT_ACTIVATED) {
		ECM_IPA_ERROR("unexpected event %d\n", event);
		WARN_ON(1);
		return;
	}

	if (netif_queue_stopped(ecm_ipa_ctx->net)) {
		ECM_IPA_DEBUG("Resource Granted - starting queue\n");
		netif_start_queue(ecm_ipa_ctx->net);
	}
	ECM_IPA_LOG_EXIT();
}

static int ecm_ipa_register_pm_client(struct ecm_ipa_dev *ecm_ipa_ctx)
{
	int result;
	struct ipa_pm_register_params pm_reg;

	memset(&pm_reg, 0, sizeof(pm_reg));
	pm_reg.name = ecm_ipa_ctx->net->name;
	pm_reg.user_data = ecm_ipa_ctx;
	pm_reg.callback = ecm_ipa_pm_cb;
	pm_reg.group = IPA_PM_GROUP_APPS;
	result = ipa_pm_register(&pm_reg, &ecm_ipa_ctx->pm_hdl);
	if (result) {
		ECM_IPA_ERROR("failed to create IPA PM client %d\n", result);
		return result;
	}
	return 0;
}

static void ecm_ipa_deregister_pm_client(struct ecm_ipa_dev *ecm_ipa_ctx)
{
	ipa_pm_deactivate_sync(ecm_ipa_ctx->pm_hdl);
	ipa_pm_deregister(ecm_ipa_ctx->pm_hdl);
	ecm_ipa_ctx->pm_hdl = ~0;
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
static void ecm_ipa_tx_complete_notify
		(void *priv,
		enum ipa_dp_evt_type evt,
		unsigned long data)
{
	struct sk_buff *skb = (struct sk_buff *)data;
	struct ecm_ipa_dev *ecm_ipa_ctx = priv;

	if (unlikely(!skb)) {
		ECM_IPA_ERROR("Bad SKB received from IPA driver\n");
		return;
	}

	if (unlikely(!ecm_ipa_ctx)) {
		ECM_IPA_ERROR("ecm_ipa_ctx is NULL pointer\n");
		return;
	}

	ECM_IPA_DEBUG
		("Tx-complete, len=%d, skb->prot=%d, outstanding=%d\n",
		skb->len, skb->protocol,
		atomic_read(&ecm_ipa_ctx->outstanding_pkts));

	if (unlikely(evt != IPA_WRITE_DONE)) {
		ECM_IPA_ERROR("unsupported event on Tx callback\n");
		return;
	}

	if (unlikely(ecm_ipa_ctx->state != ECM_IPA_CONNECTED_AND_UP)) {
		ECM_IPA_DEBUG
			("dropping Tx-complete pkt, state=%s",
			ecm_ipa_state_string(ecm_ipa_ctx->state));
		goto out;
	}

	ecm_ipa_ctx->net->stats.tx_packets++;
	ecm_ipa_ctx->net->stats.tx_bytes += skb->len;

	if (atomic_read(&ecm_ipa_ctx->outstanding_pkts) > 0)
		atomic_dec(&ecm_ipa_ctx->outstanding_pkts);

	if
		(netif_queue_stopped(ecm_ipa_ctx->net) &&
		netif_carrier_ok(ecm_ipa_ctx->net) &&
		atomic_read(&ecm_ipa_ctx->outstanding_pkts)
		< (ecm_ipa_ctx->outstanding_low)) {
		ECM_IPA_DEBUG
			("outstanding low (%d) - waking up queue\n",
			ecm_ipa_ctx->outstanding_low);
		netif_wake_queue(ecm_ipa_ctx->net);
	}

out:
	dev_kfree_skb_any(skb);
}

static void ecm_ipa_tx_timeout(struct net_device *net)
{
	struct ecm_ipa_dev *ecm_ipa_ctx = netdev_priv(net);

	ECM_IPA_ERROR
		("possible IPA stall was detected, %d outstanding",
		atomic_read(&ecm_ipa_ctx->outstanding_pkts));

	net->stats.tx_errors++;
}

static int ecm_ipa_debugfs_atomic_open(struct inode *inode, struct file *file)
{
	struct ecm_ipa_dev *ecm_ipa_ctx = inode->i_private;

	ECM_IPA_LOG_ENTRY();
	file->private_data = &ecm_ipa_ctx->outstanding_pkts;
	ECM_IPA_LOG_EXIT();
	return 0;
}

static ssize_t ecm_ipa_debugfs_atomic_read
	(struct file *file, char __user *ubuf, size_t count, loff_t *ppos)
{
	int nbytes;
	u8 atomic_str[DEBUGFS_TEMP_BUF_SIZE] = {0};
	atomic_t *atomic_var = file->private_data;

	nbytes = scnprintf
		(atomic_str, sizeof(atomic_str), "%d\n",
			atomic_read(atomic_var));
	return simple_read_from_buffer(ubuf, count, ppos, atomic_str, nbytes);
}

#ifdef CONFIG_DEBUG_FS

static void ecm_ipa_debugfs_init(struct ecm_ipa_dev *ecm_ipa_ctx)
{
	const mode_t flags_read_write = 0666;
	const mode_t flags_read_only = 0444;
	struct dentry *file;

	ECM_IPA_LOG_ENTRY();

	if (!ecm_ipa_ctx)
		return;

	ecm_ipa_ctx->directory = debugfs_create_dir("ecm_ipa", NULL);
	if (!ecm_ipa_ctx->directory) {
		ECM_IPA_ERROR("could not create debugfs directory entry\n");
		goto fail_directory;
	}
	file = debugfs_create_u8
		("outstanding_high", flags_read_write,
		ecm_ipa_ctx->directory, &ecm_ipa_ctx->outstanding_high);
	if (!file) {
		ECM_IPA_ERROR("could not create outstanding_high file\n");
		goto fail_file;
	}
	file = debugfs_create_u8
		("outstanding_low", flags_read_write,
		ecm_ipa_ctx->directory, &ecm_ipa_ctx->outstanding_low);
	if (!file) {
		ECM_IPA_ERROR("could not create outstanding_low file\n");
		goto fail_file;
	}
	file = debugfs_create_file
		("outstanding", flags_read_only,
		ecm_ipa_ctx->directory,
		ecm_ipa_ctx, &ecm_ipa_debugfs_atomic_ops);
	if (!file) {
		ECM_IPA_ERROR("could not create outstanding file\n");
		goto fail_file;
	}

	file = debugfs_create_bool("is_vlan_mode", flags_read_only,
		ecm_ipa_ctx->directory, &ecm_ipa_ctx->is_vlan_mode);
	if (!file) {
		ECM_IPA_ERROR("could not create is_vlan_mode file\n");
		goto fail_file;
	}

	ECM_IPA_DEBUG("debugfs entries were created\n");
	ECM_IPA_LOG_EXIT();

	return;
fail_file:
	debugfs_remove_recursive(ecm_ipa_ctx->directory);
fail_directory:
	return;
}

static void ecm_ipa_debugfs_destroy(struct ecm_ipa_dev *ecm_ipa_ctx)
{
	debugfs_remove_recursive(ecm_ipa_ctx->directory);
}

#else /* !CONFIG_DEBUG_FS*/

static void ecm_ipa_debugfs_init(struct ecm_ipa_dev *ecm_ipa_ctx) {}

static void ecm_ipa_debugfs_destroy(struct ecm_ipa_dev *ecm_ipa_ctx) {}

#endif /* CONFIG_DEBUG_FS */

/**
 * ecm_ipa_ep_cfg() - configure the USB endpoints for ECM
 *
 * @usb_to_ipa_hdl: handle received from ipa_connect
 * @ipa_to_usb_hdl: handle received from ipa_connect
 * @is_vlan_mode - should driver work in vlan mode?
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
static int ecm_ipa_ep_registers_cfg(u32 usb_to_ipa_hdl, u32 ipa_to_usb_hdl,
	bool is_vlan_mode)
{
	int result = 0;
	struct ipa_ep_cfg usb_to_ipa_ep_cfg;
	struct ipa_ep_cfg ipa_to_usb_ep_cfg;
	uint8_t hdr_add = 0;


	ECM_IPA_LOG_ENTRY();
	if (is_vlan_mode)
		hdr_add = VLAN_HLEN;
	memset(&usb_to_ipa_ep_cfg, 0, sizeof(struct ipa_ep_cfg));
	usb_to_ipa_ep_cfg.aggr.aggr_en = IPA_BYPASS_AGGR;
	usb_to_ipa_ep_cfg.hdr.hdr_len = ETH_HLEN + hdr_add;
	usb_to_ipa_ep_cfg.nat.nat_en = IPA_SRC_NAT;
	usb_to_ipa_ep_cfg.route.rt_tbl_hdl = 0;
	usb_to_ipa_ep_cfg.mode.dst = IPA_CLIENT_A5_LAN_WAN_CONS;
	usb_to_ipa_ep_cfg.mode.mode = IPA_BASIC;

	/* enable hdr_metadata_reg_valid */
	usb_to_ipa_ep_cfg.hdr.hdr_metadata_reg_valid = true;

	result = ipa_cfg_ep(usb_to_ipa_hdl, &usb_to_ipa_ep_cfg);
	if (result) {
		ECM_IPA_ERROR("failed to configure USB to IPA point\n");
		goto out;
	}
	memset(&ipa_to_usb_ep_cfg, 0, sizeof(struct ipa_ep_cfg));
	ipa_to_usb_ep_cfg.aggr.aggr_en = IPA_BYPASS_AGGR;
	ipa_to_usb_ep_cfg.hdr.hdr_len = ETH_HLEN + hdr_add;
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
 * ecm_ipa_set_device_ethernet_addr() - set device etherenet address
 * @dev_ethaddr: device etherenet address
 *
 * Returns 0 for success, negative otherwise
 */
static int ecm_ipa_set_device_ethernet_addr
	(u8 *dev_ethaddr, u8 device_ethaddr[])
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
static enum ecm_ipa_state ecm_ipa_next_state
	(enum ecm_ipa_state current_state, enum ecm_ipa_operation operation)
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

	ECM_IPA_DEBUG
		("state transition ( %s -> %s )- %s\n",
		ecm_ipa_state_string(current_state),
		ecm_ipa_state_string(next_state),
		next_state == ECM_IPA_INVALID ? "Forbidden" : "Allowed");

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
	ipa_ecm_logbuf = ipc_log_context_create(IPA_ECM_IPC_LOG_PAGES,
			"ipa_ecm", 0);
	if (ipa_ecm_logbuf == NULL)
		ECM_IPA_DEBUG("failed to create IPC log, continue...\n");
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
	if (ipa_ecm_logbuf)
		ipc_log_context_destroy(ipa_ecm_logbuf);
	ipa_ecm_logbuf = NULL;
	ECM_IPA_LOG_EXIT();
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("ECM IPA network interface");

late_initcall(ecm_ipa_init_module);
module_exit(ecm_ipa_cleanup_module);
