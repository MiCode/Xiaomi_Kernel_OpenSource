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

#include <linux/atomic.h>
#include <linux/errno.h>
#include <linux/etherdevice.h>
#include <linux/debugfs.h>
#include <linux/in.h>
#include <linux/stddef.h>
#include <linux/ip.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/msm_ipa.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/sched.h>
#include <linux/ipa.h>
#include <linux/random.h>
#include <linux/rndis_ipa.h>
#include <linux/workqueue.h>

#define CREATE_TRACE_POINTS
#include "rndis_ipa_trace.h"

#define DRV_NAME "RNDIS_IPA"
#define DEBUGFS_DIR_NAME "rndis_ipa"
#define DEBUGFS_AGGR_DIR_NAME "rndis_ipa_aggregation"
#define NETDEV_NAME "rndis"
#define DRV_RESOURCE_ID IPA_RM_RESOURCE_RNDIS_PROD
#define IPV4_HDR_NAME "rndis_eth_ipv4"
#define IPV6_HDR_NAME "rndis_eth_ipv6"
#define IPA_TO_USB_CLIENT IPA_CLIENT_USB_CONS
#define INACTIVITY_MSEC_DELAY 100
#define DEFAULT_OUTSTANDING_HIGH 64
#define DEFAULT_OUTSTANDING_LOW 32
#define DEBUGFS_TEMP_BUF_SIZE 4
#define RNDIS_IPA_PKT_TYPE 0x00000001
#define RNDIS_IPA_DFLT_RT_HDL 0
#define FROM_IPA_TO_USB_BAMDMA 4
#define FROM_USB_TO_IPA_BAMDMA 5
#define BAM_DMA_MAX_PKT_NUMBER 10
#define BAM_DMA_DATA_FIFO_SIZE \
		(BAM_DMA_MAX_PKT_NUMBER* \
			(ETH_FRAME_LEN + sizeof(struct rndis_pkt_hdr)))
#define BAM_DMA_DESC_FIFO_SIZE \
		(BAM_DMA_MAX_PKT_NUMBER*(sizeof(struct sps_iovec)))
#define TX_TIMEOUT (5 * HZ)
#define MIN_TX_ERROR_SLEEP_PERIOD 500
#define DEFAULT_AGGR_TIME_LIMIT 1
#define DEFAULT_AGGR_PKT_LIMIT 0


#define RNDIS_IPA_ERROR(fmt, args...) \
		pr_err(DRV_NAME "@%s@%d@ctx:%s: "\
				fmt, __func__, __LINE__, current->comm, ## args)
#define RNDIS_IPA_DEBUG(fmt, args...) \
			pr_debug("ctx: %s, "fmt, current->comm, ## args)

#define NULL_CHECK_RETVAL(ptr) \
		do { \
			if (!(ptr)) { \
				RNDIS_IPA_ERROR("null pointer #ptr\n"); \
				return -EINVAL; \
			} \
		} \
		while (0)

#define NULL_CHECK_NO_RETVAL(ptr) \
		do { \
			if (!(ptr)) {\
				RNDIS_IPA_ERROR("null pointer #ptr\n"); \
				return; \
			} \
		} \
		while (0)

#define RNDIS_HDR_OFST(field) offsetof(struct rndis_pkt_hdr, field)
#define RNDIS_IPA_LOG_ENTRY() RNDIS_IPA_DEBUG("begin\n")
#define RNDIS_IPA_LOG_EXIT()  RNDIS_IPA_DEBUG("end\n")


/**
 * enum rndis_ipa_state - specify the current driver internal state
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
enum rndis_ipa_state {
	RNDIS_IPA_UNLOADED          = 0,
	RNDIS_IPA_INITIALIZED       = 1,
	RNDIS_IPA_CONNECTED         = 2,
	RNDIS_IPA_UP                = 3,
	RNDIS_IPA_CONNECTED_AND_UP  = 4,
	RNDIS_IPA_INVALID           = 5,
};

/**
 * enum rndis_ipa_operation - enumerations used to describe the API operation
 *
 * Those enums are used as input for the driver state machine.
 */
enum rndis_ipa_operation {
	RNDIS_IPA_INITIALIZE,
	RNDIS_IPA_CONNECT,
	RNDIS_IPA_OPEN,
	RNDIS_IPA_STOP,
	RNDIS_IPA_DISCONNECT,
	RNDIS_IPA_CLEANUP,
};

#define RNDIS_IPA_STATE_DEBUG(ctx) \
	RNDIS_IPA_DEBUG("Driver state: %s\n",\
	rndis_ipa_state_string(ctx->state));

/**
 * struct rndis_loopback_pipe - hold all information needed for
 *  pipe loopback logic
 */
struct rndis_loopback_pipe {
	struct sps_pipe          *ipa_sps;
	struct ipa_sps_params ipa_sps_connect;
	struct ipa_connect_params ipa_connect_params;

	struct sps_pipe          *dma_sps;
	struct sps_connect        dma_connect;

	struct sps_alloc_dma_chan dst_alloc;
	struct sps_dma_chan       ipa_sps_channel;
	enum sps_mode mode;
	u32 ipa_peer_bam_hdl;
	u32 peer_pipe_index;
	u32 ipa_drv_ep_hdl;
	u32 ipa_pipe_index;
	enum ipa_client_type ipa_client;
	ipa_notify_cb ipa_callback;
	struct ipa_ep_cfg *ipa_ep_cfg;
};

/**
 * struct rndis_ipa_dev - main driver context parameters
 *
 * @net: network interface struct implemented by this driver
 * @directory: debugfs directory for various debugging switches
 * @tx_filter: flag that enable/disable Tx path to continue to IPA
 * @tx_dropped: number of filtered out Tx packets
 * @tx_dump_enable: dump all Tx packets
 * @rx_filter: flag that enable/disable Rx path to continue to IPA
 * @rx_dropped: number of filtered out Rx packets
 * @rx_dump_enable: dump all Rx packets
 * @icmp_filter: allow all ICMP packet to pass through the filters
 * @rm_enable: flag that enable/disable Resource manager request prior to Tx
 * @loopback_enable:  flag that enable/disable USB stub loopback
 * @deaggregation_enable: enable/disable IPA HW deaggregation logic
 * @during_xmit_error: flags that indicate that the driver is in a middle
 *  of error handling in Tx path
 * @usb_to_ipa_loopback_pipe: usb to ipa (Rx) pipe representation for loopback
 * @ipa_to_usb_loopback_pipe: ipa to usb (Tx) pipe representation for loopback
 * @bam_dma_hdl: handle representing bam-dma, used for loopback logic
 * @directory: holds all debug flags used by the driver to allow cleanup
 *  for driver unload
 * @eth_ipv4_hdr_hdl: saved handle for ipv4 header-insertion table
 * @eth_ipv6_hdr_hdl: saved handle for ipv6 header-insertion table
 * @usb_to_ipa_hdl: save handle for IPA pipe operations
 * @ipa_to_usb_hdl: save handle for IPA pipe operations
 * @outstanding_pkts: number of packets sent to IPA without TX complete ACKed
 * @outstanding_high: number of outstanding packets allowed
 * @outstanding_low: number of outstanding packets which shall cause
 *  to netdev queue start (after stopped due to outstanding_high reached)
 * @error_msec_sleep_time: number of msec for sleeping in case of Tx error
 * @state: current state of the driver
 * @host_ethaddr: holds the tethered PC ethernet address
 * @device_ethaddr: holds the device ethernet address
 * @device_ready_notify: callback supplied by USB core driver
 * This callback shall be called by the Netdev once the Netdev internal
 * state is changed to RNDIS_IPA_CONNECTED_AND_UP
 * @xmit_error_delayed_work: work item for cases where IPA driver Tx fails
 */
struct rndis_ipa_dev {
	struct net_device *net;
	u32 tx_filter;
	u32 tx_dropped;
	u32 tx_dump_enable;
	u32 rx_filter;
	u32 rx_dropped;
	u32 rx_dump_enable;
	u32 icmp_filter;
	u32 rm_enable;
	bool loopback_enable;
	u32 deaggregation_enable;
	u32 during_xmit_error;
	struct rndis_loopback_pipe usb_to_ipa_loopback_pipe;
	struct rndis_loopback_pipe ipa_to_usb_loopback_pipe;
	u32 bam_dma_hdl;
	struct dentry *directory;
	uint32_t eth_ipv4_hdr_hdl;
	uint32_t eth_ipv6_hdr_hdl;
	u32 usb_to_ipa_hdl;
	u32 ipa_to_usb_hdl;
	atomic_t outstanding_pkts;
	u32 outstanding_high;
	u32 outstanding_low;
	u32 error_msec_sleep_time;
	enum rndis_ipa_state state;
	u8 host_ethaddr[ETH_ALEN];
	u8 device_ethaddr[ETH_ALEN];
	void (*device_ready_notify)(void);
	struct delayed_work xmit_error_delayed_work;
};

/**
 * rndis_pkt_hdr - RNDIS_IPA representation of REMOTE_NDIS_PACKET_MSG
 * @msg_type: for REMOTE_NDIS_PACKET_MSG this value should be 1
 * @msg_len:  total message length in bytes, including RNDIS header an payload
 * @data_ofst: offset in bytes from start of the data_ofst to payload
 * @data_len: payload size in bytes
 * @zeroes: OOB place holder - not used for RNDIS_IPA.
 */
struct rndis_pkt_hdr {
	__le32	msg_type;
	__le32	msg_len;
	__le32	data_ofst;
	__le32	data_len;
	__le32  zeroes[7];
} __packed__;

static int rndis_ipa_open(struct net_device *net);
static void rndis_ipa_packet_receive_notify(void *private,
		enum ipa_dp_evt_type evt, unsigned long data);
static void rndis_ipa_tx_complete_notify(void *private,
		enum ipa_dp_evt_type evt, unsigned long data);
static void rndis_ipa_tx_timeout(struct net_device *net);
static int rndis_ipa_stop(struct net_device *net);
static void rndis_ipa_enable_data_path(struct rndis_ipa_dev *rndis_ipa_ctx);
static struct sk_buff *rndis_encapsulate_skb(struct sk_buff *skb);
static void rndis_ipa_xmit_error(struct sk_buff *skb);
static void rndis_ipa_xmit_error_aftercare_wq(struct work_struct *work);
static void rndis_ipa_prepare_header_insertion(int eth_type,
		const char *hdr_name, struct ipa_hdr_add *add_hdr,
		const void *dst_mac, const void *src_mac);
static int rndis_ipa_hdrs_cfg(struct rndis_ipa_dev *rndis_ipa_ctx,
		const void *dst_mac, const void *src_mac);
static int rndis_ipa_hdrs_destroy(struct rndis_ipa_dev *rndis_ipa_ctx);
static struct net_device_stats *rndis_ipa_get_stats(struct net_device *net);
static int rndis_ipa_register_properties(char *netdev_name);
static int rndis_ipa_deregister_properties(char *netdev_name);
static void rndis_ipa_rm_notify(void *user_data, enum ipa_rm_event event,
		unsigned long data);
static int rndis_ipa_create_rm_resource(struct rndis_ipa_dev *rndis_ipa_ctx);
static int rndis_ipa_destory_rm_resource(struct rndis_ipa_dev *rndis_ipa_ctx);
static bool rx_filter(struct sk_buff *skb);
static bool tx_filter(struct sk_buff *skb);
static bool rm_enabled(struct rndis_ipa_dev *rndis_ipa_ctx);
static int resource_request(struct rndis_ipa_dev *rndis_ipa_ctx);
static void resource_release(struct rndis_ipa_dev *rndis_ipa_ctx);
static netdev_tx_t rndis_ipa_start_xmit(struct sk_buff *skb,
					struct net_device *net);
static int rndis_ipa_loopback_pipe_create(
		struct rndis_ipa_dev *rndis_ipa_ctx,
		struct rndis_loopback_pipe *loopback_pipe);
static void rndis_ipa_destroy_loopback_pipe(
		struct rndis_loopback_pipe *loopback_pipe);
static int rndis_ipa_create_loopback(struct rndis_ipa_dev *rndis_ipa_ctx);
static void rndis_ipa_destroy_loopback(struct rndis_ipa_dev *rndis_ipa_ctx);
static int rndis_ipa_setup_loopback(bool enable,
		struct rndis_ipa_dev *rndis_ipa_ctx);
static int rndis_ipa_debugfs_loopback_open(struct inode *inode,
		struct file *file);
static int rndis_ipa_debugfs_atomic_open(struct inode *inode,
		struct file *file);
static int rndis_ipa_debugfs_aggr_open(struct inode *inode,
		struct file *file);
static ssize_t rndis_ipa_debugfs_aggr_write(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos);
static ssize_t rndis_ipa_debugfs_loopback_write(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos);
static ssize_t rndis_ipa_debugfs_enable_write(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos);
static ssize_t rndis_ipa_debugfs_enable_read(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos);
static ssize_t rndis_ipa_debugfs_loopback_read(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos);
static ssize_t rndis_ipa_debugfs_atomic_read(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos);
static void rndis_ipa_dump_skb(struct sk_buff *skb);
static int rndis_ipa_debugfs_init(struct rndis_ipa_dev *rndis_ipa_ctx);
static void rndis_ipa_debugfs_destroy(struct rndis_ipa_dev *rndis_ipa_ctx);
static int rndis_ipa_ep_registers_cfg(u32 usb_to_ipa_hdl,
		u32 ipa_to_usb_hdl, u32 max_xfer_size_bytes_to_dev,
		u32 max_xfer_size_bytes_to_host, u32 mtu,
		bool deaggr_enable);
static int rndis_ipa_set_device_ethernet_addr(u8 *dev_ethaddr,
		u8 device_ethaddr[]);
static enum rndis_ipa_state rndis_ipa_next_state(
		enum rndis_ipa_state current_state,
		enum rndis_ipa_operation operation);
static const char *rndis_ipa_state_string(enum rndis_ipa_state state);
static int rndis_ipa_init_module(void);
static void rndis_ipa_cleanup_module(void);

struct rndis_ipa_dev *rndis_ipa;

static const struct net_device_ops rndis_ipa_netdev_ops = {
	.ndo_open		= rndis_ipa_open,
	.ndo_stop		= rndis_ipa_stop,
	.ndo_start_xmit = rndis_ipa_start_xmit,
	.ndo_tx_timeout = rndis_ipa_tx_timeout,
	.ndo_get_stats = rndis_ipa_get_stats,
	.ndo_set_mac_address = eth_mac_addr,
};

const struct file_operations rndis_ipa_debugfs_atomic_ops = {
	.open = rndis_ipa_debugfs_atomic_open,
	.read = rndis_ipa_debugfs_atomic_read,
};

const struct file_operations rndis_ipa_loopback_ops = {
		.open = rndis_ipa_debugfs_loopback_open,
		.read = rndis_ipa_debugfs_loopback_read,
		.write = rndis_ipa_debugfs_loopback_write,
};

const struct file_operations rndis_ipa_aggr_ops = {
		.open = rndis_ipa_debugfs_aggr_open,
		.write = rndis_ipa_debugfs_aggr_write,
};

static struct ipa_ep_cfg ipa_to_usb_ep_cfg = {
	.mode = {
		.mode = IPA_BASIC,
		.dst  = IPA_CLIENT_APPS_LAN_CONS,
	},
	.hdr = {
		.hdr_len = ETH_HLEN + sizeof(struct rndis_pkt_hdr),
		.hdr_ofst_metadata_valid = false,
		.hdr_ofst_metadata = 0,
		.hdr_additional_const_len = ETH_HLEN,
		.hdr_ofst_pkt_size_valid = true,
		.hdr_ofst_pkt_size = 3*sizeof(u32),
		.hdr_a5_mux = false,
		.hdr_remove_additional = false,
		.hdr_metadata_reg_valid = false,
	},
	.hdr_ext = {
		.hdr_pad_to_alignment = 0,
		.hdr_total_len_or_pad_offset = 1*sizeof(u32),
		.hdr_payload_len_inc_padding = false,
		.hdr_total_len_or_pad = IPA_HDR_TOTAL_LEN,
		.hdr_total_len_or_pad_valid = true,
		.hdr_little_endian = true,
	},
	.aggr = {
		.aggr_en = IPA_ENABLE_AGGR,
		.aggr = IPA_GENERIC,
		.aggr_byte_limit = 4,
		.aggr_time_limit = DEFAULT_AGGR_TIME_LIMIT,
		.aggr_pkt_limit = DEFAULT_AGGR_PKT_LIMIT
	},
	.deaggr = {
		.deaggr_hdr_len = 0,
		.packet_offset_valid = 0,
		.packet_offset_location = 0,
		.max_packet_len = 0,
	},
	.route = {
		.rt_tbl_hdl = RNDIS_IPA_DFLT_RT_HDL,
	},
	.nat = {
		.nat_en = IPA_SRC_NAT,
	},
};

static struct ipa_ep_cfg usb_to_ipa_ep_cfg_deaggr_dis = {
	.mode = {
		.mode = IPA_BASIC,
		.dst  = IPA_CLIENT_APPS_LAN_CONS,
	},
	.hdr = {
		.hdr_len = ETH_HLEN + sizeof(struct rndis_pkt_hdr),
		.hdr_ofst_metadata_valid = false,
		.hdr_ofst_metadata = 0,
		.hdr_additional_const_len = 0,
		.hdr_ofst_pkt_size_valid = true,
		.hdr_ofst_pkt_size = 3*sizeof(u32) +
			sizeof(struct rndis_pkt_hdr),
		.hdr_a5_mux = false,
		.hdr_remove_additional = false,
		.hdr_metadata_reg_valid = false,
	},
	.hdr_ext = {
		.hdr_pad_to_alignment = 0,
		.hdr_total_len_or_pad_offset = 1*sizeof(u32),
		.hdr_payload_len_inc_padding = false,
		.hdr_total_len_or_pad = IPA_HDR_TOTAL_LEN,
		.hdr_total_len_or_pad_valid = true,
		.hdr_little_endian = true,
	},

	.aggr = {
		.aggr_en = IPA_BYPASS_AGGR,
		.aggr = 0,
		.aggr_byte_limit = 0,
		.aggr_time_limit = 0,
		.aggr_pkt_limit  = 0,
	},
	.deaggr = {
		.deaggr_hdr_len = 0,
		.packet_offset_valid = false,
		.packet_offset_location = 0,
		.max_packet_len = 0,
	},

	.route = {
		.rt_tbl_hdl = RNDIS_IPA_DFLT_RT_HDL,
	},
	.nat = {
		.nat_en = IPA_BYPASS_NAT,
	},
};

static struct ipa_ep_cfg usb_to_ipa_ep_cfg_deaggr_en = {
	.mode = {
		.mode = IPA_BASIC,
		.dst  = IPA_CLIENT_APPS_LAN_CONS,
	},
	.hdr = {
		.hdr_len = ETH_HLEN,
		.hdr_ofst_metadata_valid = false,
		.hdr_ofst_metadata = 0,
		.hdr_additional_const_len = 0,
		.hdr_ofst_pkt_size_valid = true,
		.hdr_ofst_pkt_size = 3*sizeof(u32),
		.hdr_a5_mux = false,
		.hdr_remove_additional = false,
		.hdr_metadata_reg_valid = false,
	},
	.hdr_ext = {
		.hdr_pad_to_alignment = 0,
		.hdr_total_len_or_pad_offset = 1*sizeof(u32),
		.hdr_payload_len_inc_padding = false,
		.hdr_total_len_or_pad = IPA_HDR_TOTAL_LEN,
		.hdr_total_len_or_pad_valid = true,
		.hdr_little_endian = true,
	},
	.aggr = {
		.aggr_en = IPA_ENABLE_DEAGGR,
		.aggr = IPA_GENERIC,
		.aggr_byte_limit = 0,
		.aggr_time_limit = 0,
		.aggr_pkt_limit  = 0,
	},
	.deaggr = {
		.deaggr_hdr_len = sizeof(struct rndis_pkt_hdr),
		.packet_offset_valid = true,
		.packet_offset_location = 8,
		.max_packet_len = 8192, /* Will be overridden*/
	},
	.route = {
		.rt_tbl_hdl = RNDIS_IPA_DFLT_RT_HDL,
	},
	.nat = {
		.nat_en = IPA_BYPASS_NAT,
	},
};


/**
 * rndis_template_hdr - RNDIS template structure for RNDIS_IPA SW insertion
 * @msg_type: set for REMOTE_NDIS_PACKET_MSG (0x00000001)
 *  this value will be used for all data packets
 * @msg_len:  will add the skb length to get final size
 * @data_ofst: this field value will not be changed
 * @data_len: set as skb length to get final size
 * @zeroes: make sure all OOB data is not used
 */
struct rndis_pkt_hdr rndis_template_hdr = {
	.msg_type = RNDIS_IPA_PKT_TYPE,
	.msg_len = sizeof(struct rndis_pkt_hdr),
	.data_ofst = sizeof(struct rndis_pkt_hdr) - RNDIS_HDR_OFST(data_ofst),
	.data_len = 0,
	.zeroes = {0},
};

/**
 * rndis_ipa_init() - create network device and initialize internal
 *  data structures
 * @params: in/out parameters required for initialization,
 *  see "struct ipa_usb_init_params" for more details
 *
 * Shall be called prior to pipe connection.
 * Detailed description:
 *  - allocate the network device
 *  - set default values for driver internal switches and stash them inside
 *     the netdev private field
 *  - set needed headroom for RNDIS header
 *  - create debugfs folder and files
 *  - create IPA resource manager client
 *  - set the ethernet address for the netdev to be added on SW Tx path
 *  - add header insertion rules for IPA driver (based on host/device Ethernet
 *     addresses given in input params and on RNDIS data template struct)
 *  - register tx/rx properties to IPA driver (will be later used
 *    by IPA configuration manager to configure rest of the IPA rules)
 *  - set the carrier state to "off" (until connect is called)
 *  - register the network device
 *  - set the out parameters
 *  - change driver internal state to INITIALIZED
 *
 * Returns negative errno, or zero on success
 */
int rndis_ipa_init(struct ipa_usb_init_params *params)
{
	int result = 0;
	struct net_device *net;
	struct rndis_ipa_dev *rndis_ipa_ctx;

	RNDIS_IPA_LOG_ENTRY();
	RNDIS_IPA_DEBUG("%s initializing\n", DRV_NAME);
	NULL_CHECK_RETVAL(params);

	RNDIS_IPA_DEBUG("host_ethaddr=%pM, device_ethaddr=%pM\n",
		params->host_ethaddr,
		params->device_ethaddr);

	net = alloc_etherdev(sizeof(struct rndis_ipa_dev));
	if (!net) {
		result = -ENOMEM;
		RNDIS_IPA_ERROR("fail to allocate Ethernet device\n");
		goto fail_alloc_etherdev;
	}
	RNDIS_IPA_DEBUG("network device was successfully allocated\n");

	rndis_ipa_ctx = netdev_priv(net);
	if (!rndis_ipa_ctx) {
		result = -ENOMEM;
		RNDIS_IPA_ERROR("fail to extract netdev priv\n");
		goto fail_netdev_priv;
	}
	memset(rndis_ipa_ctx, 0, sizeof(*rndis_ipa_ctx));
	RNDIS_IPA_DEBUG("rndis_ipa_ctx (private)=%p\n", rndis_ipa_ctx);

	rndis_ipa_ctx->net = net;
	rndis_ipa_ctx->tx_filter = false;
	rndis_ipa_ctx->rx_filter = false;
	rndis_ipa_ctx->icmp_filter = true;
	rndis_ipa_ctx->rm_enable = true;
	rndis_ipa_ctx->tx_dropped = 0;
	rndis_ipa_ctx->rx_dropped = 0;
	rndis_ipa_ctx->tx_dump_enable = false;
	rndis_ipa_ctx->rx_dump_enable = false;
	rndis_ipa_ctx->deaggregation_enable = false;
	rndis_ipa_ctx->outstanding_high = DEFAULT_OUTSTANDING_HIGH;
	rndis_ipa_ctx->outstanding_low = DEFAULT_OUTSTANDING_LOW;
	atomic_set(&rndis_ipa_ctx->outstanding_pkts, 0);
	memcpy(rndis_ipa_ctx->device_ethaddr, params->device_ethaddr,
		sizeof(rndis_ipa_ctx->device_ethaddr));
	memcpy(rndis_ipa_ctx->host_ethaddr, params->host_ethaddr,
		sizeof(rndis_ipa_ctx->host_ethaddr));
	INIT_DELAYED_WORK(&rndis_ipa_ctx->xmit_error_delayed_work,
		rndis_ipa_xmit_error_aftercare_wq);
	rndis_ipa_ctx->error_msec_sleep_time =
		MIN_TX_ERROR_SLEEP_PERIOD;
	RNDIS_IPA_DEBUG("internal data structures were set\n");

	if (!params->device_ready_notify)
		RNDIS_IPA_DEBUG("device_ready_notify() was not supplied\n");
	rndis_ipa_ctx->device_ready_notify = params->device_ready_notify;

	snprintf(net->name, sizeof(net->name), "%s%%d", NETDEV_NAME);
	RNDIS_IPA_DEBUG("Setting network interface driver name to: %s\n",
		net->name);

	net->netdev_ops = &rndis_ipa_netdev_ops;
	net->watchdog_timeo = TX_TIMEOUT;

	net->needed_headroom = sizeof(rndis_template_hdr);
	RNDIS_IPA_DEBUG("Needed headroom for RNDIS header set to %d\n",
		net->needed_headroom);

	result = rndis_ipa_debugfs_init(rndis_ipa_ctx);
	if (result)
		goto fail_debugfs;
	RNDIS_IPA_DEBUG("debugfs entries were created\n");

	result = rndis_ipa_set_device_ethernet_addr(net->dev_addr,
			rndis_ipa_ctx->device_ethaddr);
	if (result) {
		RNDIS_IPA_ERROR("set device MAC failed\n");
		goto fail_set_device_ethernet;
	}
	RNDIS_IPA_DEBUG("Device Ethernet address set %pM\n", net->dev_addr);

	result = rndis_ipa_hdrs_cfg(rndis_ipa_ctx,
			params->host_ethaddr,
			params->device_ethaddr);
	if (result) {
		RNDIS_IPA_ERROR("fail on ipa hdrs set\n");
		goto fail_hdrs_cfg;
	}
	RNDIS_IPA_DEBUG("IPA header-insertion configed for Ethernet+RNDIS\n");

	result = rndis_ipa_register_properties(net->name);
	if (result) {
		RNDIS_IPA_ERROR("fail on properties set\n");
		goto fail_register_tx;
	}
	RNDIS_IPA_DEBUG("2 TX and 2 RX properties were registered\n");

	netif_carrier_off(net);
	RNDIS_IPA_DEBUG("set carrier off until pipes are connected\n");

	result = register_netdev(net);
	if (result) {
		RNDIS_IPA_ERROR("register_netdev failed: %d\n", result);
		goto fail_register_netdev;
	}
	RNDIS_IPA_DEBUG("netdev:%s registration succeeded, index=%d\n",
		net->name, net->ifindex);

	rndis_ipa = rndis_ipa_ctx;
	params->ipa_rx_notify = rndis_ipa_packet_receive_notify;
	params->ipa_tx_notify = rndis_ipa_tx_complete_notify;
	params->private = rndis_ipa_ctx;
	params->skip_ep_cfg = false;
	rndis_ipa_ctx->state = RNDIS_IPA_INITIALIZED;
	RNDIS_IPA_STATE_DEBUG(rndis_ipa_ctx);
	pr_info("RNDIS_IPA NetDev was initialized");

	RNDIS_IPA_LOG_EXIT();

	return 0;

fail_register_netdev:
	rndis_ipa_deregister_properties(net->name);
fail_register_tx:
	rndis_ipa_hdrs_destroy(rndis_ipa_ctx);
fail_set_device_ethernet:
fail_hdrs_cfg:
	rndis_ipa_debugfs_destroy(rndis_ipa_ctx);
fail_debugfs:
fail_netdev_priv:
	free_netdev(net);
fail_alloc_etherdev:
	return result;
}
EXPORT_SYMBOL(rndis_ipa_init);

/**
 * rndis_ipa_pipe_connect_notify() - notify rndis_ipa Netdev that the USB pipes
 *  were connected
 * @usb_to_ipa_hdl: handle from IPA driver client for USB->IPA
 * @ipa_to_usb_hdl: handle from IPA driver client for IPA->USB
 * @private: same value that was set by init(), this parameter holds the
 *  network device pointer.
 * @max_transfer_byte_size: RNDIS protocol specific, the maximum size that
 *  the host expect
 * @max_packet_number: RNDIS protocol specific, the maximum packet number
 *  that the host expects
 *
 * Once USB driver finishes the pipe connection between IPA core
 * and USB core this method shall be called in order to
 * allow the driver to complete the data path configurations.
 * Detailed description:
 *  - configure the IPA end-points register
 *  - notify the Linux kernel for "carrier_on"
 *  - change the driver internal state
 *
 *  After this function is done the driver state changes to "Connected"  or
 *  Connected and Up.
 *  This API is expected to be called after initialization() or
 *  after a call to disconnect().
 *
 * Returns negative errno, or zero on success
 */
int rndis_ipa_pipe_connect_notify(u32 usb_to_ipa_hdl,
			u32 ipa_to_usb_hdl,
			u32 max_xfer_size_bytes_to_dev,
			u32 max_packet_number_to_dev,
			u32 max_xfer_size_bytes_to_host,
			void *private)
{
	struct rndis_ipa_dev *rndis_ipa_ctx = private;
	int next_state;
	int result;

	RNDIS_IPA_LOG_ENTRY();

	NULL_CHECK_RETVAL(private);

	RNDIS_IPA_DEBUG("usb_to_ipa_hdl=%d, ipa_to_usb_hdl=%d, private=0x%p\n",
				usb_to_ipa_hdl, ipa_to_usb_hdl, private);
	RNDIS_IPA_DEBUG("max_xfer_sz_to_dev=%d, max_pkt_num_to_dev=%d\n",
			max_xfer_size_bytes_to_dev,
			max_packet_number_to_dev);
	RNDIS_IPA_DEBUG("max_xfer_sz_to_host=%d\n",
			max_xfer_size_bytes_to_host);

	next_state = rndis_ipa_next_state(rndis_ipa_ctx->state,
		RNDIS_IPA_CONNECT);
	if (next_state == RNDIS_IPA_INVALID) {
		RNDIS_IPA_ERROR("use init()/disconnect() before connect()\n");
		return -EPERM;
	}

	if (usb_to_ipa_hdl >= IPA_CLIENT_MAX) {
		RNDIS_IPA_ERROR("usb_to_ipa_hdl(%d) - not valid ipa handle\n",
				usb_to_ipa_hdl);
		return -EINVAL;
	}
	if (ipa_to_usb_hdl >= IPA_CLIENT_MAX) {
		RNDIS_IPA_ERROR("ipa_to_usb_hdl(%d) - not valid ipa handle\n",
				ipa_to_usb_hdl);
		return -EINVAL;
	}

	result = rndis_ipa_create_rm_resource(rndis_ipa_ctx);
	if (result) {
		RNDIS_IPA_ERROR("fail on RM create\n");
		goto fail_create_rm;
	}
	RNDIS_IPA_DEBUG("RM resource was created\n");

	rndis_ipa_ctx->ipa_to_usb_hdl = ipa_to_usb_hdl;
	rndis_ipa_ctx->usb_to_ipa_hdl = usb_to_ipa_hdl;
	if (max_packet_number_to_dev > 1)
		rndis_ipa_ctx->deaggregation_enable = true;
	else
		rndis_ipa_ctx->deaggregation_enable = false;
	result = rndis_ipa_ep_registers_cfg(usb_to_ipa_hdl,
			ipa_to_usb_hdl,
			max_xfer_size_bytes_to_dev,
			max_xfer_size_bytes_to_host,
			rndis_ipa_ctx->net->mtu,
			rndis_ipa_ctx->deaggregation_enable);
	if (result) {
		RNDIS_IPA_ERROR("fail on ep cfg\n");
		goto fail;
	}
	RNDIS_IPA_DEBUG("end-points configured\n");

	netif_stop_queue(rndis_ipa_ctx->net);
	RNDIS_IPA_DEBUG("netif_stop_queue() was called\n");

	netif_carrier_on(rndis_ipa_ctx->net);
	if (!netif_carrier_ok(rndis_ipa_ctx->net)) {
		RNDIS_IPA_ERROR("netif_carrier_ok error\n");
		result = -EBUSY;
		goto fail;
	}
	RNDIS_IPA_DEBUG("netif_carrier_on() was called\n");

	rndis_ipa_ctx->state = next_state;
	RNDIS_IPA_STATE_DEBUG(rndis_ipa_ctx);

	if (next_state == RNDIS_IPA_CONNECTED_AND_UP)
		rndis_ipa_enable_data_path(rndis_ipa_ctx);
	else
		RNDIS_IPA_DEBUG("queue shall be started after open()\n");

	pr_info("RNDIS_IPA NetDev pipes were connected\n");

	RNDIS_IPA_LOG_EXIT();

	return 0;

fail:
	rndis_ipa_destory_rm_resource(rndis_ipa_ctx);
fail_create_rm:
	return result;
}
EXPORT_SYMBOL(rndis_ipa_pipe_connect_notify);

/**
 * rndis_ipa_open() - notify Linux network stack to start sending packets
 * @net: the network interface supplied by the network stack
 *
 * Linux uses this API to notify the driver that the network interface
 * transitions to the up state.
 * The driver will instruct the Linux network stack to start
 * delivering data packets.
 * The driver internal state shall be changed to Up or Connected and Up
 *
 * Returns negative errno, or zero on success
 */
static int rndis_ipa_open(struct net_device *net)
{
	struct rndis_ipa_dev *rndis_ipa_ctx;
	int next_state;

	RNDIS_IPA_LOG_ENTRY();

	rndis_ipa_ctx = netdev_priv(net);

	next_state = rndis_ipa_next_state(rndis_ipa_ctx->state, RNDIS_IPA_OPEN);
	if (next_state == RNDIS_IPA_INVALID) {
		RNDIS_IPA_ERROR("can't bring driver up before initialize\n");
		return -EPERM;
	}

	rndis_ipa_ctx->state = next_state;
	RNDIS_IPA_STATE_DEBUG(rndis_ipa_ctx);


	if (next_state == RNDIS_IPA_CONNECTED_AND_UP)
		rndis_ipa_enable_data_path(rndis_ipa_ctx);
	else
		RNDIS_IPA_DEBUG("queue shall be started after connect()\n");

	pr_info("RNDIS_IPA NetDev was opened\n");

	RNDIS_IPA_LOG_EXIT();

	return 0;
}

/**
 * rndis_ipa_start_xmit() - send data from APPs to USB core via IPA core
 *  using SW path (Tx data path)
 * Tx path for this Netdev is Apps-processor->IPA->USB
 * @skb: packet received from Linux network stack destined for tethered PC
 * @net: the network device being used to send this packet (rndis0)
 *
 * Several conditions needed in order to send the packet to IPA:
 * - Transmit queue for the network driver is currently
 *   in "started" state
 * - The driver internal state is in Connected and Up state.
 * - Filters Tx switch are turned off
 * - The IPA resource manager state for the driver producer client
 *   is "Granted" which implies that all the resources in the dependency
 *   graph are valid for data flow.
 * - outstanding high boundary was not reached.
 *
 * In case the outstanding packets high boundary is reached, the driver will
 * stop the send queue until enough packets are processed by
 * the IPA core (based on calls to rndis_ipa_tx_complete_notify).
 *
 * In case all of the conditions are met, the network driver shall:
 *  - encapsulate the Ethernet packet with RNDIS header (REMOTE_NDIS_PACKET_MSG)
 *  - send the packet by using IPA Driver SW path (IP_PACKET_INIT)
 *  - Netdev status fields shall be updated based on the current Tx packet
 *
 * Returns NETDEV_TX_BUSY if retry should be made later,
 * or NETDEV_TX_OK on success.
 */
static netdev_tx_t rndis_ipa_start_xmit(struct sk_buff *skb,
					struct net_device *net)
{
	int ret;
	netdev_tx_t status = NETDEV_TX_BUSY;
	struct rndis_ipa_dev *rndis_ipa_ctx = netdev_priv(net);

	net->trans_start = jiffies;

	RNDIS_IPA_DEBUG("Tx, len=%d, skb->protocol=%d, outstanding=%d\n",
		skb->len, skb->protocol,
		atomic_read(&rndis_ipa_ctx->outstanding_pkts));

	if (unlikely(netif_queue_stopped(net))) {
		RNDIS_IPA_ERROR("interface queue is stopped\n");
		goto out;
	}

	if (unlikely(rndis_ipa_ctx->tx_dump_enable))
		rndis_ipa_dump_skb(skb);

	if (unlikely(rndis_ipa_ctx->state != RNDIS_IPA_CONNECTED_AND_UP)) {
		RNDIS_IPA_ERROR("Missing pipe connected and/or iface up\n");
		return NETDEV_TX_BUSY;
	}

	if (unlikely(tx_filter(skb))) {
		dev_kfree_skb_any(skb);
		RNDIS_IPA_DEBUG("packet got filtered out on Tx path\n");
		rndis_ipa_ctx->tx_dropped++;
		status = NETDEV_TX_OK;
		goto out;
	}

	ret = resource_request(rndis_ipa_ctx);
	if (ret) {
		RNDIS_IPA_DEBUG("Waiting to resource\n");
		netif_stop_queue(net);
		goto resource_busy;
	}

	if (atomic_read(&rndis_ipa_ctx->outstanding_pkts) >=
				rndis_ipa_ctx->outstanding_high) {
		RNDIS_IPA_DEBUG("Outstanding high boundary reached (%d)\n",
				rndis_ipa_ctx->outstanding_high);
		netif_stop_queue(net);
		RNDIS_IPA_DEBUG("send  queue was stopped\n");
		status = NETDEV_TX_BUSY;
		goto out;
	}

	skb = rndis_encapsulate_skb(skb);
	trace_rndis_tx_dp(skb->protocol);
	ret = ipa_tx_dp(IPA_TO_USB_CLIENT, skb, NULL);
	if (ret) {
		RNDIS_IPA_ERROR("ipa transmit failed (%d)\n", ret);
		goto fail_tx_packet;
	}

	atomic_inc(&rndis_ipa_ctx->outstanding_pkts);

	status = NETDEV_TX_OK;
	goto out;

fail_tx_packet:
	rndis_ipa_xmit_error(skb);
out:
	resource_release(rndis_ipa_ctx);
resource_busy:
	RNDIS_IPA_DEBUG("packet Tx done - %s\n",
		(status == NETDEV_TX_OK) ? "OK" : "FAIL");

	return status;
}

/**
 * rndis_ipa_tx_complete_notify() - notification for Netdev that the
 *  last packet was successfully sent
 * @private: driver context stashed by IPA driver upon pipe connect
 * @evt: event type (expected to be write-done event)
 * @data: data provided with event (this is actually the skb that
 *  holds the sent packet)
 *
 * This function will be called on interrupt bottom halve deferred context.
 * outstanding packets counter shall be decremented.
 * Network stack send queue will be re-started in case low outstanding
 * boundary is reached and queue was stopped before.
 * At the end the skb shall be freed.
 */
static void rndis_ipa_tx_complete_notify(void *private,
		enum ipa_dp_evt_type evt,
		unsigned long data)
{
	struct sk_buff *skb = (struct sk_buff *)data;
	struct rndis_ipa_dev *rndis_ipa_ctx = private;

	NULL_CHECK_NO_RETVAL(private);

	trace_rndis_status_rcvd(skb->protocol);

	RNDIS_IPA_DEBUG("Tx-complete, len=%d, skb->prot=%d, outstanding=%d\n",
		skb->len, skb->protocol,
		atomic_read(&rndis_ipa_ctx->outstanding_pkts));

	if (unlikely((evt != IPA_WRITE_DONE))) {
		RNDIS_IPA_ERROR("unsupported event on TX call-back\n");
		return;
	}

	if (unlikely(rndis_ipa_ctx->state != RNDIS_IPA_CONNECTED_AND_UP)) {
		RNDIS_IPA_DEBUG("dropping Tx-complete pkt, state=%s\n",
			rndis_ipa_state_string(rndis_ipa_ctx->state));
		goto out;
	}

	rndis_ipa_ctx->net->stats.tx_packets++;
	rndis_ipa_ctx->net->stats.tx_bytes += skb->len;

	atomic_dec(&rndis_ipa_ctx->outstanding_pkts);
	if (netif_queue_stopped(rndis_ipa_ctx->net) &&
		netif_carrier_ok(rndis_ipa_ctx->net) &&
		atomic_read(&rndis_ipa_ctx->outstanding_pkts) <
					(rndis_ipa_ctx->outstanding_low)) {
		RNDIS_IPA_DEBUG("outstanding low boundary reached (%d)n",
				rndis_ipa_ctx->outstanding_low);
		netif_wake_queue(rndis_ipa_ctx->net);
		RNDIS_IPA_DEBUG("send queue was awaken\n");
	}

out:
	dev_kfree_skb_any(skb);

	return;
}

static void rndis_ipa_tx_timeout(struct net_device *net)
{
	struct rndis_ipa_dev *rndis_ipa_ctx = netdev_priv(net);
	int outstanding = atomic_read(&rndis_ipa_ctx->outstanding_pkts);

	RNDIS_IPA_ERROR("possible IPA stall was detected, %d outstanding\n",
		outstanding);

	net->stats.tx_errors++;
}

/**
 * rndis_ipa_rm_notify() - callback supplied to IPA resource manager
 *   for grant/release events
 * user_data: the driver context supplied to IPA resource manager during call
 *  to ipa_rm_create_resource().
 * event: the event notified to us by IPA resource manager (Release/Grant)
 * data: reserved field supplied by IPA resource manager
 *
 * This callback shall be called based on resource request/release sent
 * to the IPA resource manager.
 * In case the queue was stopped during EINPROGRESS for Tx path and the
 * event received is Grant then the queue shall be restarted.
 * In case the event notified is a release notification the netdev discard it.
 */
static void rndis_ipa_rm_notify(void *user_data, enum ipa_rm_event event,
		unsigned long data)
{
	struct rndis_ipa_dev *rndis_ipa_ctx = user_data;

	RNDIS_IPA_LOG_ENTRY();

	if (event == IPA_RM_RESOURCE_RELEASED) {
		RNDIS_IPA_DEBUG("Resource Released\n");
		return;
	}

	if (event != IPA_RM_RESOURCE_GRANTED) {
		RNDIS_IPA_ERROR("Unexceoted event receieved from RM (%d\n)",
			event);
		return;
	}
	RNDIS_IPA_DEBUG("Resource Granted\n");

	if (netif_queue_stopped(rndis_ipa_ctx->net)) {
		RNDIS_IPA_DEBUG("starting queue\n");
		netif_start_queue(rndis_ipa_ctx->net);
	} else {
		RNDIS_IPA_DEBUG("queue already awake\n");
	}

	RNDIS_IPA_LOG_EXIT();
}

/**
 * rndis_ipa_packet_receive_notify() - Rx notify for packet sent from
 *  tethered PC (USB->IPA).
 *  is USB->IPA->Apps-processor
 * @private: driver context
 * @evt: event type
 * @data: data provided with event
 *
 * Once IPA driver receives a packet from USB client this callback will be
 * called from bottom-half interrupt handling context (ipa Rx workqueue).
   *
 * Packets that shall be sent to Apps processor may be of two types:
 * 1) Packets that are destined for Apps (e.g: WEBSERVER running on Apps)
 * 2) Exception packets that need special handling (based on IPA core
 *    configuration, e.g: new TCP session or any other packets that IPA core
 *    can't handle)
 * If the next conditions are met, the packet shall be sent up to the
 * Linux network stack:
 *  - Driver internal state is Connected and Up
 *  - Notification received from IPA driver meets the expected type
 *    for Rx packet
 *  -Filters Rx switch are turned off
 *
 * Prior to the sending to the network stack:
 *  - Netdev struct shall be stashed to the skb as required by the network stack
 *  - Ethernet header shall be removed (skb->data shall point to the Ethernet
 *     payload, Ethernet still stashed under MAC header).
 *  - The skb->pkt_protocol shall be set based on the ethernet destination
 *     address, Can be Broadcast, Multicast or Other-Host, The later
 *     pkt-types packets shall be dropped in case the Netdev is not
 *     in  promisc mode.
 *   - Set the skb protocol field based on the EtherType field
 *
 * Netdev status fields shall be updated based on the current Rx packet
 */
static void rndis_ipa_packet_receive_notify(void *private,
		enum ipa_dp_evt_type evt,
		unsigned long data)
{
	struct sk_buff *skb = (struct sk_buff *)data;
	struct rndis_ipa_dev *rndis_ipa_ctx = private;
	int result;
	unsigned int packet_len = skb->len;

	RNDIS_IPA_DEBUG("packet Rx, len=%d\n",
		skb->len);

	if (unlikely(rndis_ipa_ctx->rx_dump_enable))
		rndis_ipa_dump_skb(skb);

	if (unlikely(rndis_ipa_ctx->state != RNDIS_IPA_CONNECTED_AND_UP)) {
		RNDIS_IPA_DEBUG("use connect()/up() before receive()\n");
		RNDIS_IPA_DEBUG("packet dropped (length=%d)\n",
				skb->len);
		return;
	}

	if (evt != IPA_RECEIVE)	{
		RNDIS_IPA_ERROR("a none IPA_RECEIVE event in driver RX\n");
		return;
	}

	if (!rndis_ipa_ctx->deaggregation_enable)
		skb_pull(skb, sizeof(struct rndis_pkt_hdr));

	skb->dev = rndis_ipa_ctx->net;
	skb->protocol = eth_type_trans(skb, rndis_ipa_ctx->net);

	if (rx_filter(skb)) {
		RNDIS_IPA_DEBUG("packet got filtered out on RX path\n");
		rndis_ipa_ctx->rx_dropped++;
		dev_kfree_skb_any(skb);
		return;
	}

	trace_rndis_netif_ni(skb->protocol);
	result = netif_rx_ni(skb);
	if (result)
		RNDIS_IPA_ERROR("fail on netif_rx_ni\n");
	rndis_ipa_ctx->net->stats.rx_packets++;
	rndis_ipa_ctx->net->stats.rx_bytes += packet_len;

	return;
}

/** rndis_ipa_stop() - notify the network interface to stop
 *   sending/receiving data
 *  @net: the network device being stopped.
 *
 * This API is used by Linux network stack to notify the network driver that
 * its state was changed to "down"
 * The driver will stop the "send" queue and change its internal
 * state to "Connected".
 * The Netdev shall be returned to be "Up" after rndis_ipa_open().
 */
static int rndis_ipa_stop(struct net_device *net)
{
	struct rndis_ipa_dev *rndis_ipa_ctx = netdev_priv(net);
	int next_state;

	RNDIS_IPA_LOG_ENTRY();

	next_state = rndis_ipa_next_state(rndis_ipa_ctx->state, RNDIS_IPA_STOP);
	if (next_state == RNDIS_IPA_INVALID) {
		RNDIS_IPA_DEBUG("can't do network interface down without up\n");
		return -EPERM;
	}

	netif_stop_queue(net);
	pr_info("RNDIS_IPA NetDev queue is stopped\n");

	rndis_ipa_ctx->state = next_state;
	RNDIS_IPA_STATE_DEBUG(rndis_ipa_ctx);

	RNDIS_IPA_LOG_EXIT();

	return 0;
}

/** rndis_ipa_disconnect() - notify rndis_ipa Netdev that the USB pipes
 *   were disconnected
 * @private: same value that was set by init(), this  parameter holds the
 *  network device pointer.
 *
 * USB shall notify the Netdev after disconnecting the pipe.
 * - The internal driver state shall returned to its previous
 *   state (Up or Initialized).
 * - Linux network stack shall be informed for carrier off to notify
 *   user space for pipe disconnect
 * - send queue shall be stopped
 * During the transition between the pipe disconnection to
 * the Netdev notification packets
 * are expected to be dropped by IPA driver or IPA core.
 */
int rndis_ipa_pipe_disconnect_notify(void *private)
{
	struct rndis_ipa_dev *rndis_ipa_ctx = private;
	int next_state;
	int outstanding_dropped_pkts;
	int retval;

	RNDIS_IPA_LOG_ENTRY();

	NULL_CHECK_RETVAL(rndis_ipa_ctx);
	RNDIS_IPA_DEBUG("private=0x%p\n", private);

	next_state = rndis_ipa_next_state(rndis_ipa_ctx->state,
		RNDIS_IPA_DISCONNECT);
	if (next_state == RNDIS_IPA_INVALID) {
		RNDIS_IPA_ERROR("can't disconnect before connect\n");
		return -EPERM;
	}

	if (rndis_ipa_ctx->during_xmit_error) {
		RNDIS_IPA_DEBUG("canceling xmit-error delayed work\n");
		cancel_delayed_work_sync(
			&rndis_ipa_ctx->xmit_error_delayed_work);
		rndis_ipa_ctx->during_xmit_error = false;
	}

	netif_carrier_off(rndis_ipa_ctx->net);
	RNDIS_IPA_DEBUG("carrier_off notification was sent\n");

	netif_stop_queue(rndis_ipa_ctx->net);
	RNDIS_IPA_DEBUG("queue stopped\n");

	outstanding_dropped_pkts =
		atomic_read(&rndis_ipa_ctx->outstanding_pkts);

	rndis_ipa_ctx->net->stats.tx_dropped += outstanding_dropped_pkts;
	atomic_set(&rndis_ipa_ctx->outstanding_pkts, 0);

	retval = rndis_ipa_destory_rm_resource(rndis_ipa_ctx);
	if (retval) {
		RNDIS_IPA_ERROR("Fail to clean RM\n");
		return retval;
	}
	RNDIS_IPA_DEBUG("RM was successfully destroyed\n");

	rndis_ipa_ctx->state = next_state;
	RNDIS_IPA_STATE_DEBUG(rndis_ipa_ctx);

	pr_info("RNDIS_IPA NetDev pipes disconnected (%d outstanding clr)\n",
		outstanding_dropped_pkts);

	RNDIS_IPA_LOG_EXIT();

	return 0;
}
EXPORT_SYMBOL(rndis_ipa_pipe_disconnect_notify);

/**
 * rndis_ipa_cleanup() - unregister the network interface driver and free
 *  internal data structs.
 * @private: same value that was set by init(), this
 *   parameter holds the network device pointer.
 *
 * This function shall be called once the network interface is not
 * needed anymore, e.g: when the USB composition does not support it.
 * This function shall be called after the pipes were disconnected.
 * Detailed description:
 *  - remove header-insertion headers from IPA core
 *  - delete the driver dependency defined for IPA resource manager and
 *   destroy the producer resource.
 *  -  remove the debugfs entries
 *  - deregister the network interface from Linux network stack
 *  - free all internal data structs
 *
 * It is assumed that no packets shall be sent through HW bridging
 * during cleanup to avoid packets trying to add an header that is
 * removed during cleanup (IPA configuration manager should have
 * removed them at this point)
 */
void rndis_ipa_cleanup(void *private)
{
	struct rndis_ipa_dev *rndis_ipa_ctx = private;
	int next_state;
	int retval;

	RNDIS_IPA_LOG_ENTRY();

	RNDIS_IPA_DEBUG("private=0x%p\n", private);

	if (!rndis_ipa_ctx) {
		RNDIS_IPA_ERROR("rndis_ipa_ctx NULL pointer\n");
		return;
	}

	next_state = rndis_ipa_next_state(rndis_ipa_ctx->state,
		RNDIS_IPA_CLEANUP);
	if (next_state == RNDIS_IPA_INVALID) {
		RNDIS_IPA_ERROR("use disconnect()before clean()\n");
		return;
	}
	RNDIS_IPA_STATE_DEBUG(rndis_ipa_ctx);

	retval = rndis_ipa_deregister_properties(rndis_ipa_ctx->net->name);
	if (retval) {
		RNDIS_IPA_ERROR("Fail to deregister Tx/Rx properties\n");
		return;
	}
	RNDIS_IPA_DEBUG("deregister Tx/Rx properties was successful\n");

	retval = rndis_ipa_hdrs_destroy(rndis_ipa_ctx);
	if (retval)
		RNDIS_IPA_ERROR(
			"Failed removing RNDIS headers from IPA core. Continue anyway\n");
	else
		RNDIS_IPA_DEBUG("RNDIS headers were removed from IPA core\n");

	rndis_ipa_debugfs_destroy(rndis_ipa_ctx);
	RNDIS_IPA_DEBUG("debugfs remove was done\n");

	unregister_netdev(rndis_ipa_ctx->net);
	RNDIS_IPA_DEBUG("netdev unregistered\n");

	rndis_ipa_ctx->state = next_state;
	free_netdev(rndis_ipa_ctx->net);
	pr_info("RNDIS_IPA NetDev was cleaned\n");

	RNDIS_IPA_LOG_EXIT();

	return;
}
EXPORT_SYMBOL(rndis_ipa_cleanup);


static void rndis_ipa_enable_data_path(struct rndis_ipa_dev *rndis_ipa_ctx)
{
	if (rndis_ipa_ctx->device_ready_notify) {
		rndis_ipa_ctx->device_ready_notify();
		RNDIS_IPA_DEBUG("USB device_ready_notify() was called\n");
	} else {
		RNDIS_IPA_DEBUG("device_ready_notify() not supplied\n");
	}

	netif_start_queue(rndis_ipa_ctx->net);
	RNDIS_IPA_DEBUG("netif_start_queue() was called\n");
}

static void rndis_ipa_xmit_error(struct sk_buff *skb)
{
	bool retval;
	struct rndis_ipa_dev *rndis_ipa_ctx = netdev_priv(skb->dev);
	unsigned long delay_jiffies;
	u8 rand_dealy_msec;

	RNDIS_IPA_LOG_ENTRY();

	RNDIS_IPA_DEBUG("starting Tx-queue backoff\n");

	netif_stop_queue(rndis_ipa_ctx->net);
	RNDIS_IPA_DEBUG("netif_stop_queue was called\n");

	skb_pull(skb, sizeof(rndis_template_hdr));
	rndis_ipa_ctx->net->stats.tx_errors++;

	get_random_bytes(&rand_dealy_msec, sizeof(rand_dealy_msec));
	delay_jiffies = msecs_to_jiffies(
		rndis_ipa_ctx->error_msec_sleep_time + rand_dealy_msec);

	retval = schedule_delayed_work(
		&rndis_ipa_ctx->xmit_error_delayed_work, delay_jiffies);
	if (!retval) {
		RNDIS_IPA_ERROR("fail to schedule delayed work\n");
		netif_start_queue(rndis_ipa_ctx->net);
	} else {
		RNDIS_IPA_DEBUG("work scheduled to start Tx-queue in %d msec\n",
			rndis_ipa_ctx->error_msec_sleep_time + rand_dealy_msec);
		rndis_ipa_ctx->during_xmit_error = true;
	}

	RNDIS_IPA_LOG_EXIT();
}

static void rndis_ipa_xmit_error_aftercare_wq(struct work_struct *work)
{
	struct rndis_ipa_dev *rndis_ipa_ctx;
	struct delayed_work *delayed_work;

	RNDIS_IPA_LOG_ENTRY();

	RNDIS_IPA_DEBUG("Starting queue after xmit error\n");

	delayed_work = to_delayed_work(work);
	rndis_ipa_ctx = container_of(delayed_work, struct rndis_ipa_dev,
		xmit_error_delayed_work);

	if (unlikely(rndis_ipa_ctx->state != RNDIS_IPA_CONNECTED_AND_UP)) {
		RNDIS_IPA_ERROR("error aftercare handling in bad state (%d)",
			rndis_ipa_ctx->state);
		return;
	}

	rndis_ipa_ctx->during_xmit_error = false;

	netif_start_queue(rndis_ipa_ctx->net);
	RNDIS_IPA_DEBUG("netif_start_queue() was called\n");

	RNDIS_IPA_LOG_EXIT();
}

/**
 * rndis_ipa_prepare_header_insertion() - prepare the header insertion request
 *  for IPA driver
 * eth_type: the Ethernet type for this header-insertion header
 * hdr_name: string that shall represent this header in IPA data base
 * add_hdr: output for caller to be used with ipa_add_hdr() to configure
 *  the IPA core
 * dst_mac: tethered PC MAC (Ethernet) address to be added to packets
 *  for IPA->USB pipe
 * src_mac: device MAC (Ethernet) address to be added to packets
 *  for IPA->USB pipe
 *
 * This function shall build the header-insertion block request for a
 * single Ethernet+RNDIS header)
 * this header shall be inserted for packets processed by IPA
 * and destined for USB client.
 * This header shall be used for HW bridging for packets destined for
 *  tethered PC.
 * For SW data-path, this header won't be used.
 */
static void rndis_ipa_prepare_header_insertion(int eth_type,
		const char *hdr_name, struct ipa_hdr_add *add_hdr,
		const void *dst_mac, const void *src_mac)
{
	struct ethhdr *eth_hdr;

	add_hdr->hdr_len = sizeof(rndis_template_hdr);
	add_hdr->is_partial = false;
	strlcpy(add_hdr->name, hdr_name, IPA_RESOURCE_NAME_MAX);

	memcpy(add_hdr->hdr, &rndis_template_hdr, sizeof(rndis_template_hdr));
	eth_hdr = (struct ethhdr *)(add_hdr->hdr + sizeof(rndis_template_hdr));
	memcpy(eth_hdr->h_dest, dst_mac, ETH_ALEN);
	memcpy(eth_hdr->h_source, src_mac, ETH_ALEN);
	eth_hdr->h_proto = htons(eth_type);
	add_hdr->hdr_len += ETH_HLEN;
	add_hdr->is_eth2_ofst_valid = true;
	add_hdr->eth2_ofst = sizeof(rndis_template_hdr);
	add_hdr->type = IPA_HDR_L2_ETHERNET_II;
}

/**
 * rndis_ipa_hdrs_cfg() - configure header insertion block in IPA core
 *  to allow HW bridging
 * @rndis_ipa_ctx: main driver context
 * @dst_mac: destination MAC address (tethered PC)
 * @src_mac: source MAC address (MDM device)
 *
 * This function shall add 2 headers.
 * One header for Ipv4 and one header for Ipv6.
 * Both headers shall contain Ethernet header and RNDIS header, the only
 * difference shall be in the EtherTye field.
 * Headers will be committed to HW
 *
 * Returns negative errno, or zero on success
 */
static int rndis_ipa_hdrs_cfg(struct rndis_ipa_dev *rndis_ipa_ctx,
		const void *dst_mac, const void *src_mac)
{
	struct ipa_ioc_add_hdr *hdrs;
	struct ipa_hdr_add *ipv4_hdr;
	struct ipa_hdr_add *ipv6_hdr;
	int result = 0;

	RNDIS_IPA_LOG_ENTRY();

	hdrs = kzalloc(sizeof(*hdrs) + sizeof(*ipv4_hdr) + sizeof(*ipv6_hdr),
			GFP_KERNEL);
	if (!hdrs) {
		RNDIS_IPA_ERROR("mem allocation fail for header-insertion\n");
		result = -ENOMEM;
		goto fail_mem;
	}

	ipv4_hdr = &hdrs->hdr[0];
	ipv6_hdr = &hdrs->hdr[1];
	rndis_ipa_prepare_header_insertion(ETH_P_IP, IPV4_HDR_NAME,
		ipv4_hdr, dst_mac, src_mac);
	rndis_ipa_prepare_header_insertion(ETH_P_IPV6, IPV6_HDR_NAME,
		ipv6_hdr, dst_mac, src_mac);

	hdrs->commit = 1;
	hdrs->num_hdrs = 2;
	result = ipa_add_hdr(hdrs);
	if (result) {
		RNDIS_IPA_ERROR("Fail on Header-Insertion(%d)\n", result);
		goto fail_add_hdr;
	}
	if (ipv4_hdr->status) {
		RNDIS_IPA_ERROR("Fail on Header-Insertion ipv4(%d)\n",
				ipv4_hdr->status);
		result = ipv4_hdr->status;
		goto fail_add_hdr;
	}
	if (ipv6_hdr->status) {
		RNDIS_IPA_ERROR("Fail on Header-Insertion ipv6(%d)\n",
				ipv6_hdr->status);
		result = ipv6_hdr->status;
		goto fail_add_hdr;
	}
	rndis_ipa_ctx->eth_ipv4_hdr_hdl = ipv4_hdr->hdr_hdl;
	rndis_ipa_ctx->eth_ipv6_hdr_hdl = ipv6_hdr->hdr_hdl;

	RNDIS_IPA_LOG_EXIT();

fail_add_hdr:
	kfree(hdrs);
fail_mem:
	return result;
}

/**
 * rndis_ipa_hdrs_destroy() - remove the IPA core configuration done for
 *  the driver data path bridging.
 * @rndis_ipa_ctx: the driver context
 *
 *  Revert the work done on rndis_ipa_hdrs_cfg(), which is,
 * remove 2 headers for Ethernet+RNDIS.
 */
static int rndis_ipa_hdrs_destroy(struct rndis_ipa_dev *rndis_ipa_ctx)
{
	struct ipa_ioc_del_hdr *del_hdr;
	struct ipa_hdr_del *ipv4;
	struct ipa_hdr_del *ipv6;
	int result;

	del_hdr = kzalloc(sizeof(*del_hdr) + sizeof(*ipv4) +
			sizeof(*ipv6), GFP_KERNEL);
	if (!del_hdr) {
		RNDIS_IPA_ERROR("memory allocation for del_hdr failed\n");
		return -ENOMEM;
	}

	del_hdr->commit = 1;
	del_hdr->num_hdls = 2;

	ipv4 = &del_hdr->hdl[0];
	ipv4->hdl = rndis_ipa_ctx->eth_ipv4_hdr_hdl;
	ipv6 = &del_hdr->hdl[1];
	ipv6->hdl = rndis_ipa_ctx->eth_ipv6_hdr_hdl;

	result = ipa_del_hdr(del_hdr);
	if (result || ipv4->status || ipv6->status)
		RNDIS_IPA_ERROR("ipa_del_hdr failed\n");
	else
		RNDIS_IPA_DEBUG("hdrs deletion done\n");

	kfree(del_hdr);
	return result;
}

static struct net_device_stats *rndis_ipa_get_stats(struct net_device *net)
{
	return &net->stats;
}


/**
 * rndis_ipa_register_properties() - set Tx/Rx properties needed
 *  by IPA configuration manager
 * @netdev_name: a string with the name of the network interface device
 *
 * Register Tx/Rx properties to allow user space configuration (IPA
 * Configuration Manager):
 *
 * - Two Tx properties (IPA->USB): specify the header names and pipe number
 *   that shall be used by user space for header-addition configuration
 *   for ipv4/ipv6 packets flowing from IPA to USB for HW bridging data.
 *   That header-addition header is added by the Netdev and used by user
 *   space to close the the HW bridge by adding filtering and routing rules
 *   that point to this header.
 *
 * - Two Rx properties (USB->IPA): these properties shall be used by user space
 *   to configure the IPA core to identify the packets destined
 *   for Apps-processor by configuring the unicast rules destined for
 *   the Netdev IP address.
 *   This rules shall be added based on the attribute mask supplied at
 *   this function, that is, always hit rule.
 */
static int rndis_ipa_register_properties(char *netdev_name)
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

	RNDIS_IPA_LOG_ENTRY();

	tx_properties.prop = properties;
	ipv4_property = &tx_properties.prop[0];
	ipv4_property->ip = IPA_IP_v4;
	ipv4_property->dst_pipe = IPA_TO_USB_CLIENT;
	strlcpy(ipv4_property->hdr_name, IPV4_HDR_NAME,
			IPA_RESOURCE_NAME_MAX);
	ipv4_property->hdr_l2_type = IPA_HDR_L2_ETHERNET_II;
	ipv6_property = &tx_properties.prop[1];
	ipv6_property->ip = IPA_IP_v6;
	ipv6_property->dst_pipe = IPA_TO_USB_CLIENT;
	strlcpy(ipv6_property->hdr_name, IPV6_HDR_NAME,
			IPA_RESOURCE_NAME_MAX);
	ipv6_property->hdr_l2_type = IPA_HDR_L2_ETHERNET_II;
	tx_properties.num_props = 2;

	rx_properties.prop = rx_ioc_properties;
	rx_ipv4_property = &rx_properties.prop[0];
	rx_ipv4_property->ip = IPA_IP_v4;
	rx_ipv4_property->attrib.attrib_mask = 0;
	rx_ipv4_property->src_pipe = IPA_CLIENT_USB_PROD;
	rx_ipv4_property->hdr_l2_type = IPA_HDR_L2_ETHERNET_II;
	rx_ipv6_property = &rx_properties.prop[1];
	rx_ipv6_property->ip = IPA_IP_v6;
	rx_ipv6_property->attrib.attrib_mask = 0;
	rx_ipv6_property->src_pipe = IPA_CLIENT_USB_PROD;
	rx_ipv6_property->hdr_l2_type = IPA_HDR_L2_ETHERNET_II;
	rx_properties.num_props = 2;

	result = ipa_register_intf("rndis0", &tx_properties, &rx_properties);
	if (result)
		RNDIS_IPA_ERROR("fail on Tx/Rx properties registration\n");
	else
		RNDIS_IPA_DEBUG("Tx/Rx properties registration done\n");

	RNDIS_IPA_LOG_EXIT();

	return result;
}

/**
 * rndis_ipa_deregister_properties() - remove the 2 Tx and 2 Rx properties
 * @netdev_name: a string with the name of the network interface device
 *
 * This function revert the work done on rndis_ipa_register_properties().
 */
static int  rndis_ipa_deregister_properties(char *netdev_name)
{
	int result;

	RNDIS_IPA_LOG_ENTRY();

	result = ipa_deregister_intf(netdev_name);
	if (result) {
		RNDIS_IPA_DEBUG("Fail on Tx prop deregister\n");
		return result;
	}
	RNDIS_IPA_LOG_EXIT();

	return 0;
}

/**
 * rndis_ipa_create_rm_resource() -creates the resource representing
 *  this Netdev and supply notification callback for resource event
 *  such as Grant/Release
 * @rndis_ipa_ctx: this driver context
 *
 * In order make sure all needed resources are available during packet
 * transmit this Netdev shall use Request/Release mechanism of
 * the IPA resource manager.
 * This mechanism shall iterate over a dependency graph and make sure
 * all dependent entities are ready to for packet Tx
 * transfer (Apps->IPA->USB).
 * In this function the resource representing the Netdev is created
 * in addition to the basic dependency between the Netdev and the USB client.
 * Hence, USB client, is a dependency for the Netdev and may be notified in
 * case of packet transmit from this Netdev to tethered Host.
 * As implied from the "may" in the above sentence there is a scenario where
 * the USB is not notified. This is done thanks to the IPA resource manager
 * inactivity timer.
 * The inactivity timer allow the Release requests to be delayed in order
 * prevent ping-pong with the USB and other dependencies.
 */
static int rndis_ipa_create_rm_resource(struct rndis_ipa_dev *rndis_ipa_ctx)
{
	struct ipa_rm_create_params create_params = {0};
	struct ipa_rm_perf_profile profile;
	int result;

	RNDIS_IPA_LOG_ENTRY();

	create_params.name = DRV_RESOURCE_ID;
	create_params.reg_params.user_data = rndis_ipa_ctx;
	create_params.reg_params.notify_cb = rndis_ipa_rm_notify;
	result = ipa_rm_create_resource(&create_params);
	if (result) {
		RNDIS_IPA_ERROR("Fail on ipa_rm_create_resource\n");
		goto fail_rm_create;
	}
	RNDIS_IPA_DEBUG("RM client was created\n");

	profile.max_supported_bandwidth_mbps = IPA_APPS_MAX_BW_IN_MBPS;
	ipa_rm_set_perf_profile(DRV_RESOURCE_ID, &profile);

	result = ipa_rm_inactivity_timer_init(DRV_RESOURCE_ID,
			INACTIVITY_MSEC_DELAY);
	if (result) {
		RNDIS_IPA_ERROR("Fail on ipa_rm_inactivity_timer_init\n");
		goto fail_inactivity_timer;
	}

	RNDIS_IPA_DEBUG("rm_it client was created\n");

	result = ipa_rm_add_dependency_sync(DRV_RESOURCE_ID,
					    IPA_RM_RESOURCE_USB_CONS);

	if (result && result != -EINPROGRESS)
		RNDIS_IPA_ERROR("unable to add RNDIS/USB dependency (%d)\n",
				result);
	else
		RNDIS_IPA_DEBUG("RNDIS/USB dependency was set\n");

	result = ipa_rm_add_dependency_sync(IPA_RM_RESOURCE_USB_PROD,
					    IPA_RM_RESOURCE_APPS_CONS);
	if (result && result != -EINPROGRESS)
		RNDIS_IPA_ERROR("unable to add USB/APPS dependency (%d)\n",
				result);
	else
		RNDIS_IPA_DEBUG("USB/APPS dependency was set\n");

	RNDIS_IPA_LOG_EXIT();

	return 0;

fail_inactivity_timer:
fail_rm_create:
	return result;
}

/**
 * rndis_ipa_destroy_rm_resource() - delete the dependency and destroy
 * the resource done on rndis_ipa_create_rm_resource()
 * @rndis_ipa_ctx: this driver context
 *
 * This function shall delete the dependency create between
 * the Netdev to the USB.
 * In addition the inactivity time shall be destroy and the resource shall
 * be deleted.
 */
static int rndis_ipa_destory_rm_resource(struct rndis_ipa_dev *rndis_ipa_ctx)
{
	int result;

	RNDIS_IPA_LOG_ENTRY();

	result = ipa_rm_delete_dependency(DRV_RESOURCE_ID,
			IPA_RM_RESOURCE_USB_CONS);
	if (result && result != -EINPROGRESS) {
		RNDIS_IPA_ERROR("Fail to delete RNDIS/USB dependency\n");
		goto bail;
	}
	RNDIS_IPA_DEBUG("RNDIS/USB dependency was successfully deleted\n");

	result = ipa_rm_delete_dependency(IPA_RM_RESOURCE_USB_PROD,
					IPA_RM_RESOURCE_APPS_CONS);
	if (result == -EINPROGRESS) {
		RNDIS_IPA_DEBUG("RM dependency deletion is in progress");
	} else if (result) {
		RNDIS_IPA_ERROR("Fail to delete USB/APPS dependency\n");
		goto bail;
	} else {
		RNDIS_IPA_DEBUG("USB/APPS dependency was deleted\n");
	}

	result = ipa_rm_inactivity_timer_destroy(DRV_RESOURCE_ID);
	if (result) {
		RNDIS_IPA_ERROR("Fail to destroy inactivity timern");
		goto bail;
	}
	RNDIS_IPA_DEBUG("RM inactivity timer was successfully destroy\n");

	result = ipa_rm_delete_resource(DRV_RESOURCE_ID);
	if (result) {
		RNDIS_IPA_ERROR("resource deletion failed\n");
		goto bail;
	}
	RNDIS_IPA_DEBUG("Netdev RM resource was deleted (resid:%d)\n",
		DRV_RESOURCE_ID);


	RNDIS_IPA_LOG_EXIT();

bail:
	return result;
}

/**
 * resource_request() - request for the Netdev resource
 * @rndis_ipa_ctx: main driver context
 *
 * This function shall send the IPA resource manager inactivity time a request
 * to Grant the Netdev producer.
 * In case the resource is already Granted the function shall return immediately
 * and "pet" the inactivity timer.
 * In case the resource was not already Granted this function shall
 * return EINPROGRESS and the Netdev shall stop the send queue until
 * the IPA resource manager notify it that the resource is
 * granted (done in a differ context)
 */
static int resource_request(struct rndis_ipa_dev *rndis_ipa_ctx)
{
	int result = 0;

	if (!rm_enabled(rndis_ipa_ctx))
		goto out;
	result = ipa_rm_inactivity_timer_request_resource(
			DRV_RESOURCE_ID);
out:
	return result;
}

/**
 * resource_release() - release the Netdev resource
 * @rndis_ipa_ctx: main driver context
 *
 * start the inactivity timer count down.by using the IPA resource
 * manager inactivity time.
 * The actual resource release shall occur only if no request shall be done
 * during the INACTIVITY_MSEC_DELAY.
 */
static void resource_release(struct rndis_ipa_dev *rndis_ipa_ctx)
{
	if (!rm_enabled(rndis_ipa_ctx))
		goto out;
	ipa_rm_inactivity_timer_release_resource(DRV_RESOURCE_ID);
out:
	return;
}

/**
 * rndis_encapsulate_skb() - encapsulate the given Ethernet skb with
 *  an RNDIS header
 * @skb: packet to be encapsulated with the RNDIS header
 *
 * Shall use a template header for RNDIS and update it with the given
 * skb values.
 * Ethernet is expected to be already encapsulate the packet.
 */
static struct sk_buff *rndis_encapsulate_skb(struct sk_buff *skb)
{
	struct rndis_pkt_hdr *rndis_hdr;
	int payload_byte_len = skb->len;

	/* if there is no room in this skb, allocate a new one */
	if (unlikely(skb_headroom(skb) < sizeof(rndis_template_hdr))) {
		struct sk_buff *new_skb = skb_copy_expand(skb,
			sizeof(rndis_template_hdr), 0, GFP_ATOMIC);
		if (!new_skb) {
			RNDIS_IPA_ERROR("no memory for skb expand\n");
			return skb;
		}
		RNDIS_IPA_DEBUG("skb expanded. old %p new %p\n", skb, new_skb);
		dev_kfree_skb_any(skb);
		skb = new_skb;
	}

	/* make room at the head of the SKB to put the RNDIS header */
	rndis_hdr = (struct rndis_pkt_hdr *)skb_push(skb,
					sizeof(rndis_template_hdr));

	memcpy(rndis_hdr, &rndis_template_hdr, sizeof(*rndis_hdr));
	rndis_hdr->msg_len +=  payload_byte_len;
	rndis_hdr->data_len +=  payload_byte_len;

	return skb;
}

/**
 * rx_filter() - logic that decide if the current skb is to be filtered out
 * @skb: skb that may be sent up to the network stack
 *
 * This function shall do Rx packet filtering on the Netdev level.
 */
static bool rx_filter(struct sk_buff *skb)
{
	struct rndis_ipa_dev *rndis_ipa_ctx = netdev_priv(skb->dev);

	return rndis_ipa_ctx->rx_filter;
}

/**
 * tx_filter() - logic that decide if the current skb is to be filtered out
 * @skb: skb that may be sent to the USB core
 *
 * This function shall do Tx packet filtering on the Netdev level.
 * ICMP filter bypass is possible to allow only ICMP packet to be
 * sent (pings and etc)
 */

static bool tx_filter(struct sk_buff *skb)
{
	struct rndis_ipa_dev *rndis_ipa_ctx = netdev_priv(skb->dev);
	bool is_icmp;

	if (likely(rndis_ipa_ctx->tx_filter == false))
		return false;

	is_icmp = (skb->protocol == htons(ETH_P_IP)	&&
		ip_hdr(skb)->protocol == IPPROTO_ICMP);

	if ((rndis_ipa_ctx->icmp_filter == false) && is_icmp)
		return false;

	return true;
}

/**
 * rm_enabled() - allow the use of resource manager Request/Release to
 *  be bypassed
 * @rndis_ipa_ctx: main driver context
 *
 * By disabling the resource manager flag the Request for the Netdev resource
 * shall be bypassed and the packet shall be sent.
 * accordingly, Release request shall be bypass as well.
 */
static bool rm_enabled(struct rndis_ipa_dev *rndis_ipa_ctx)
{
	return rndis_ipa_ctx->rm_enable;
}

/**
 * rndis_ipa_ep_registers_cfg() - configure the USB endpoints
 * @usb_to_ipa_hdl: handle received from ipa_connect which represents
 *  the USB to IPA end-point
 * @ipa_to_usb_hdl: handle received from ipa_connect which represents
 *  the IPA to USB end-point
 * @max_xfer_size_bytes_to_dev: the maximum size, in bytes, that the device
 *  expects to receive from the host. supplied on REMOTE_NDIS_INITIALIZE_CMPLT.
 * @max_xfer_size_bytes_to_host: the maximum size, in bytes, that the host
 *  expects to receive from the device. supplied on REMOTE_NDIS_INITIALIZE_MSG.
 * @mtu: the netdev MTU size, in bytes
 *
 * USB to IPA pipe:
 *  - de-aggregation
 *  - Remove Ethernet header
 *  - Remove RNDIS header
 *  - SRC NAT
 *  - Default routing(0)
 * IPA to USB Pipe:
 *  - aggregation
 *  - Add Ethernet header
 *  - Add RNDIS header
 */
static int rndis_ipa_ep_registers_cfg(u32 usb_to_ipa_hdl,
		u32 ipa_to_usb_hdl,
		u32 max_xfer_size_bytes_to_dev,
		u32 max_xfer_size_bytes_to_host,
		u32 mtu,
		bool deaggr_enable)
{
	int result;
	struct ipa_ep_cfg *usb_to_ipa_ep_cfg;

	if (deaggr_enable) {
		usb_to_ipa_ep_cfg = &usb_to_ipa_ep_cfg_deaggr_en;
		RNDIS_IPA_DEBUG("deaggregation enabled\n");
	} else {
		usb_to_ipa_ep_cfg = &usb_to_ipa_ep_cfg_deaggr_dis;
		RNDIS_IPA_DEBUG("deaggregation disabled\n");
	}

	usb_to_ipa_ep_cfg->deaggr.max_packet_len = max_xfer_size_bytes_to_dev;
	result = ipa_cfg_ep(usb_to_ipa_hdl, usb_to_ipa_ep_cfg);
	if (result) {
		pr_err("failed to configure USB to IPA point\n");
		return result;
	}
	RNDIS_IPA_DEBUG("IPA<-USB end-point configured\n");

	ipa_to_usb_ep_cfg.aggr.aggr_byte_limit =
		(max_xfer_size_bytes_to_host - mtu)/1024;

	if (ipa_to_usb_ep_cfg.aggr.aggr_byte_limit == 0) {
		ipa_to_usb_ep_cfg.aggr.aggr_time_limit = 0;
		ipa_to_usb_ep_cfg.aggr.aggr_pkt_limit = 1;
	} else {
		ipa_to_usb_ep_cfg.aggr.aggr_time_limit =
			DEFAULT_AGGR_TIME_LIMIT;
		ipa_to_usb_ep_cfg.aggr.aggr_pkt_limit =
			DEFAULT_AGGR_PKT_LIMIT;
	}

	RNDIS_IPA_DEBUG("RNDIS aggregation param:"
			" en=%d"
			" byte_limit=%d"
			" time_limit=%d"
			" pkt_limit=%d\n",
			ipa_to_usb_ep_cfg.aggr.aggr_en,
			ipa_to_usb_ep_cfg.aggr.aggr_byte_limit,
			ipa_to_usb_ep_cfg.aggr.aggr_time_limit,
			ipa_to_usb_ep_cfg.aggr.aggr_pkt_limit);

	result = ipa_cfg_ep(ipa_to_usb_hdl, &ipa_to_usb_ep_cfg);
	if (result) {
		pr_err("failed to configure IPA to USB end-point\n");
		return result;
	}
	RNDIS_IPA_DEBUG("IPA->USB end-point configured\n");

	return 0;
}

/**
 * rndis_ipa_set_device_ethernet_addr() - set device Ethernet address
 * @dev_ethaddr: device Ethernet address
 *
 * Returns 0 for success, negative otherwise
 */
static int rndis_ipa_set_device_ethernet_addr(u8 *dev_ethaddr,
		u8 device_ethaddr[])
{
	if (!is_valid_ether_addr(device_ethaddr))
		return -EINVAL;
	memcpy(dev_ethaddr, device_ethaddr, ETH_ALEN);

	return 0;
}

/** rndis_ipa_next_state - return the next state of the driver
 * @current_state: the current state of the driver
 * @operation: an enum which represent the operation being made on the driver
 *  by its API.
 *
 * This function implements the driver internal state machine.
 * Its decisions are based on the driver current state and the operation
 * being made.
 * In case the operation is invalid this state machine will return
 * the value RNDIS_IPA_INVALID to inform the caller for a forbidden sequence.
 */
static enum rndis_ipa_state rndis_ipa_next_state(
		enum rndis_ipa_state current_state,
		enum rndis_ipa_operation operation)
{
	int next_state = RNDIS_IPA_INVALID;

	switch (current_state) {
	case RNDIS_IPA_UNLOADED:
		if (operation == RNDIS_IPA_INITIALIZE)
			next_state = RNDIS_IPA_INITIALIZED;
		break;
	case RNDIS_IPA_INITIALIZED:
		if (operation == RNDIS_IPA_CONNECT)
			next_state = RNDIS_IPA_CONNECTED;
		else if (operation == RNDIS_IPA_OPEN)
			next_state = RNDIS_IPA_UP;
		else if (operation == RNDIS_IPA_CLEANUP)
			next_state = RNDIS_IPA_UNLOADED;
		break;
	case RNDIS_IPA_CONNECTED:
		if (operation == RNDIS_IPA_DISCONNECT)
			next_state = RNDIS_IPA_INITIALIZED;
		else if (operation == RNDIS_IPA_OPEN)
			next_state = RNDIS_IPA_CONNECTED_AND_UP;
		break;
	case RNDIS_IPA_UP:
		if (operation == RNDIS_IPA_STOP)
			next_state = RNDIS_IPA_INITIALIZED;
		else if (operation == RNDIS_IPA_CONNECT)
			next_state = RNDIS_IPA_CONNECTED_AND_UP;
		else if (operation == RNDIS_IPA_CLEANUP)
			next_state = RNDIS_IPA_UNLOADED;
		break;
	case RNDIS_IPA_CONNECTED_AND_UP:
		if (operation == RNDIS_IPA_STOP)
			next_state = RNDIS_IPA_CONNECTED;
		else if (operation == RNDIS_IPA_DISCONNECT)
			next_state = RNDIS_IPA_UP;
		break;
	default:
		RNDIS_IPA_ERROR("State is not supported\n");
		WARN_ON(true);
		break;
	}

	RNDIS_IPA_DEBUG("state transition ( %s -> %s )- %s\n",
			rndis_ipa_state_string(current_state),
			rndis_ipa_state_string(next_state) ,
			next_state == RNDIS_IPA_INVALID ?
					"Forbidden" : "Allowed");

	return next_state;
}

/**
 * rndis_ipa_state_string - return the state string representation
 * @state: enum which describe the state
 */
static const char *rndis_ipa_state_string(enum rndis_ipa_state state)
{
	switch (state) {
	case RNDIS_IPA_UNLOADED:
		return "RNDIS_IPA_UNLOADED";
	case RNDIS_IPA_INITIALIZED:
		return "RNDIS_IPA_INITIALIZED";
	case RNDIS_IPA_CONNECTED:
		return "RNDIS_IPA_CONNECTED";
	case RNDIS_IPA_UP:
		return "RNDIS_IPA_UP";
	case RNDIS_IPA_CONNECTED_AND_UP:
		return "RNDIS_IPA_CONNECTED_AND_UP";
	default:
		return "Not supported";
	}
}

static void rndis_ipa_dump_skb(struct sk_buff *skb)
{
	int i;
	u32 *cur = (u32 *)skb->data;
	u8 *byte;

	RNDIS_IPA_DEBUG("packet dump start for skb->len=%d\n",
		skb->len);

	for (i = 0; i < (skb->len/4); i++) {
		byte = (u8 *)(cur + i);
		pr_info("%2d %08x   %02x %02x %02x %02x\n",
			i, *(cur + i),
			byte[0], byte[1], byte[2], byte[3]);
	}
	RNDIS_IPA_DEBUG("packet dump ended for skb->len=%d\n",
		skb->len);
}

/**
 * Creates the root folder for the driver
 */
static int rndis_ipa_debugfs_init(struct rndis_ipa_dev *rndis_ipa_ctx)
{
	const mode_t flags_read_write = S_IRUGO | S_IWUGO;
	const mode_t flags_read_only = S_IRUGO;
	const mode_t  flags_write_only = S_IWUGO;
	struct dentry *file;
	struct dentry *aggr_directory;

	RNDIS_IPA_LOG_ENTRY();

	if (!rndis_ipa_ctx)
		return -EINVAL;

	rndis_ipa_ctx->directory = debugfs_create_dir(DEBUGFS_DIR_NAME, NULL);
	if (!rndis_ipa_ctx->directory) {
		RNDIS_IPA_ERROR("could not create debugfs directory entry\n");
		goto fail_directory;
	}

	file = debugfs_create_bool("tx_filter", flags_read_write,
		rndis_ipa_ctx->directory, &rndis_ipa_ctx->tx_filter);
	if (!file) {
		RNDIS_IPA_ERROR("could not create debugfs tx_filter file\n");
		goto fail_file;
	}

	file = debugfs_create_bool("rx_filter", flags_read_write,
			rndis_ipa_ctx->directory, &rndis_ipa_ctx->rx_filter);
	if (!file) {
		RNDIS_IPA_ERROR("could not create debugfs rx_filter file\n");
		goto fail_file;
	}

	file = debugfs_create_bool("icmp_filter", flags_read_write,
			rndis_ipa_ctx->directory, &rndis_ipa_ctx->icmp_filter);
	if (!file) {
		RNDIS_IPA_ERROR("could not create debugfs icmp_filter file\n");
		goto fail_file;
	}

	file = debugfs_create_bool("rm_enable", flags_read_write,
			rndis_ipa_ctx->directory, &rndis_ipa_ctx->rm_enable);
	if (!file) {
		RNDIS_IPA_ERROR("could not create debugfs rm file\n");
		goto fail_file;
	}

	file = debugfs_create_u32("outstanding_high", flags_read_write,
			rndis_ipa_ctx->directory,
			&rndis_ipa_ctx->outstanding_high);
	if (!file) {
		RNDIS_IPA_ERROR("could not create outstanding_high file\n");
		goto fail_file;
	}

	file = debugfs_create_u32("outstanding_low", flags_read_write,
			rndis_ipa_ctx->directory,
			&rndis_ipa_ctx->outstanding_low);
	if (!file) {
		RNDIS_IPA_ERROR("could not create outstanding_low file\n");
		goto fail_file;
	}

	file = debugfs_create_file("outstanding", flags_read_only,
			rndis_ipa_ctx->directory,
			rndis_ipa_ctx, &rndis_ipa_debugfs_atomic_ops);
	if (!file) {
		RNDIS_IPA_ERROR("could not create outstanding file\n");
		goto fail_file;
	}

	file = debugfs_create_file("loopback_enable", flags_read_write,
				rndis_ipa_ctx->directory,
				rndis_ipa_ctx, &rndis_ipa_loopback_ops);
	if (!file) {
		RNDIS_IPA_ERROR("could not create outstanding file\n");
		goto fail_file;
	}

	file = debugfs_create_u8("state", flags_read_only,
			rndis_ipa_ctx->directory, (u8 *)&rndis_ipa_ctx->state);
	if (!file) {
		RNDIS_IPA_ERROR("could not create state file\n");
		goto fail_file;
	}

	file = debugfs_create_u32("tx_dropped", flags_read_only,
			rndis_ipa_ctx->directory, &rndis_ipa_ctx->tx_dropped);
	if (!file) {
		RNDIS_IPA_ERROR("could not create tx_dropped file\n");
		goto fail_file;
	}

	file = debugfs_create_u32("rx_dropped", flags_read_only,
			rndis_ipa_ctx->directory, &rndis_ipa_ctx->rx_dropped);
	if (!file) {
		RNDIS_IPA_ERROR("could not create rx_dropped file\n");
		goto fail_file;
	}

	aggr_directory = debugfs_create_dir(DEBUGFS_AGGR_DIR_NAME,
		rndis_ipa_ctx->directory);
	if (!aggr_directory) {
		RNDIS_IPA_ERROR("could not create debugfs aggr entry\n");
		goto fail_directory;
	}

	file = debugfs_create_file("aggr_value_set", flags_write_only,
				aggr_directory,
				rndis_ipa_ctx, &rndis_ipa_aggr_ops);
	if (!file) {
		RNDIS_IPA_ERROR("could not create aggr_value_set file\n");
		goto fail_file;
	}

	file = debugfs_create_u8("aggr_enable", flags_read_write,
			aggr_directory, (u8 *)&ipa_to_usb_ep_cfg.aggr.aggr_en);
	if (!file) {
		RNDIS_IPA_ERROR("could not create aggr_enable file\n");
		goto fail_file;
	}

	file = debugfs_create_u8("aggr_type", flags_read_write,
			aggr_directory, (u8 *)&ipa_to_usb_ep_cfg.aggr.aggr);
	if (!file) {
		RNDIS_IPA_ERROR("could not create aggr_type file\n");
		goto fail_file;
	}

	file = debugfs_create_u32("aggr_byte_limit", flags_read_write,
			aggr_directory,
			&ipa_to_usb_ep_cfg.aggr.aggr_byte_limit);
	if (!file) {
		RNDIS_IPA_ERROR("could not create aggr_byte_limit file\n");
		goto fail_file;
	}

	file = debugfs_create_u32("aggr_time_limit", flags_read_write,
			aggr_directory,
			&ipa_to_usb_ep_cfg.aggr.aggr_time_limit);
	if (!file) {
		RNDIS_IPA_ERROR("could not create aggr_time_limit file\n");
		goto fail_file;
	}

	file = debugfs_create_u32("aggr_pkt_limit", flags_read_write,
			aggr_directory,
			&ipa_to_usb_ep_cfg.aggr.aggr_pkt_limit);
	if (!file) {
		RNDIS_IPA_ERROR("could not create aggr_pkt_limit file\n");
		goto fail_file;
	}

	file = debugfs_create_bool("tx_dump_enable", flags_read_write,
			rndis_ipa_ctx->directory,
			&rndis_ipa_ctx->tx_dump_enable);
	if (!file) {
		RNDIS_IPA_ERROR("fail to create tx_dump_enable file\n");
		goto fail_file;
	}

	file = debugfs_create_bool("rx_dump_enable", flags_read_write,
			rndis_ipa_ctx->directory,
			&rndis_ipa_ctx->rx_dump_enable);
	if (!file) {
		RNDIS_IPA_ERROR("fail to create rx_dump_enable file\n");
		goto fail_file;
	}

	file = debugfs_create_bool("deaggregation_enable", flags_read_write,
			rndis_ipa_ctx->directory,
			&rndis_ipa_ctx->deaggregation_enable);
	if (!file) {
		RNDIS_IPA_ERROR("fail to create deaggregation_enable file\n");
		goto fail_file;
	}

	file = debugfs_create_u32("error_msec_sleep_time", flags_read_write,
			rndis_ipa_ctx->directory,
			&rndis_ipa_ctx->error_msec_sleep_time);
	if (!file) {
		RNDIS_IPA_ERROR("fail to create error_msec_sleep_time file\n");
		goto fail_file;
	}

	file = debugfs_create_bool("during_xmit_error", flags_read_only,
			rndis_ipa_ctx->directory,
			&rndis_ipa_ctx->during_xmit_error);
	if (!file) {
		RNDIS_IPA_ERROR("fail to create during_xmit_error file\n");
		goto fail_file;
	}

	RNDIS_IPA_LOG_EXIT();

	return 0;
fail_file:
	debugfs_remove_recursive(rndis_ipa_ctx->directory);
fail_directory:
	return -EFAULT;
}

static void rndis_ipa_debugfs_destroy(struct rndis_ipa_dev *rndis_ipa_ctx)
{
	debugfs_remove_recursive(rndis_ipa_ctx->directory);
}


static int rndis_ipa_debugfs_aggr_open(struct inode *inode,
		struct file *file)
{
	struct rndis_ipa_dev *rndis_ipa_ctx = inode->i_private;
	file->private_data = rndis_ipa_ctx;

	return 0;
}


static ssize_t rndis_ipa_debugfs_aggr_write(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos)
{
	struct rndis_ipa_dev *rndis_ipa_ctx = file->private_data;
	int result;

	result = ipa_cfg_ep(rndis_ipa_ctx->usb_to_ipa_hdl, &ipa_to_usb_ep_cfg);
	if (result) {
		pr_err("failed to re-configure USB to IPA point\n");
		return result;
	}
	pr_info("IPA<-USB end-point re-configured\n");

	return count;
}

static int rndis_ipa_debugfs_loopback_open(struct inode *inode,
		struct file *file)
{
	struct rndis_ipa_dev *rndis_ipa_ctx = inode->i_private;
	file->private_data = rndis_ipa_ctx;

	return 0;
}

static ssize_t rndis_ipa_debugfs_loopback_read(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos)
{
	int cnt;
	struct rndis_ipa_dev *rndis_ipa_ctx = file->private_data;

	file->private_data = &rndis_ipa_ctx->loopback_enable;

	cnt = rndis_ipa_debugfs_enable_read(file,
			ubuf, count, ppos);

	return cnt;
}

static ssize_t rndis_ipa_debugfs_loopback_write(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos)
{
	int retval;
	int cnt;
	struct rndis_ipa_dev *rndis_ipa_ctx = file->private_data;
	bool old_state = rndis_ipa_ctx->loopback_enable;

	file->private_data = &rndis_ipa_ctx->loopback_enable;

	cnt = rndis_ipa_debugfs_enable_write(file,
			buf, count, ppos);

	RNDIS_IPA_DEBUG("loopback_enable was set to:%d->%d\n",
			old_state, rndis_ipa_ctx->loopback_enable);

	if (old_state == rndis_ipa_ctx->loopback_enable) {
		RNDIS_IPA_ERROR("NOP - same state\n");
		return cnt;
	}

	retval = rndis_ipa_setup_loopback(
				rndis_ipa_ctx->loopback_enable,
				rndis_ipa_ctx);
	if (retval)
		rndis_ipa_ctx->loopback_enable = old_state;

	return cnt;
}

static int rndis_ipa_debugfs_atomic_open(struct inode *inode, struct file *file)
{
	struct rndis_ipa_dev *rndis_ipa_ctx = inode->i_private;

	RNDIS_IPA_LOG_ENTRY();

	file->private_data = &(rndis_ipa_ctx->outstanding_pkts);

	RNDIS_IPA_LOG_EXIT();

	return 0;
}

static ssize_t rndis_ipa_debugfs_atomic_read(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos)
{
	int nbytes;
	u8 atomic_str[DEBUGFS_TEMP_BUF_SIZE] = {0};
	atomic_t *atomic_var = file->private_data;

	RNDIS_IPA_LOG_ENTRY();

	nbytes = scnprintf(atomic_str, sizeof(atomic_str), "%d\n",
			atomic_read(atomic_var));

	RNDIS_IPA_LOG_EXIT();

	return simple_read_from_buffer(ubuf, count, ppos, atomic_str, nbytes);
}

static ssize_t rndis_ipa_debugfs_enable_read(struct file *file,
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
		RNDIS_IPA_ERROR("simple_read_from_buffer problem\n");
		return ret;
	}
	size += ret;
	count -= nbytes;
	*ppos = pos + size;
	return size;
}

static ssize_t rndis_ipa_debugfs_enable_write(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos)
{
	unsigned long missing;
	char input;
	bool *enable = file->private_data;
	if (count != sizeof(input) + 1) {
		RNDIS_IPA_ERROR("wrong input length(%zd)\n", count);
		return -EINVAL;
	}
	if (!buf) {
		RNDIS_IPA_ERROR("Bad argument\n");
		return -EINVAL;
	}
	missing = copy_from_user(&input, buf, 1);
	if (missing)
		return -EFAULT;
	RNDIS_IPA_DEBUG("input received %c\n", input);
	*enable = input - '0';
	RNDIS_IPA_DEBUG("value was set to %d\n", *enable);
	return count;
}

/**
 * Connects IPA->BAMDMA
 * This shall simulate the path from IPA to USB
 * Allowing the driver TX path
 */
static int rndis_ipa_loopback_pipe_create(
		struct rndis_ipa_dev *rndis_ipa_ctx,
		struct rndis_loopback_pipe *loopback_pipe)
{
	int retval;

	RNDIS_IPA_LOG_ENTRY();

	/* SPS pipe has two side handshake
	 * This is the first handshake of IPA->BAMDMA,
	 * This is the IPA side
	 */
	loopback_pipe->ipa_connect_params.client = loopback_pipe->ipa_client;
	loopback_pipe->ipa_connect_params.client_bam_hdl =
			rndis_ipa_ctx->bam_dma_hdl;
	loopback_pipe->ipa_connect_params.client_ep_idx =
		loopback_pipe->peer_pipe_index;
	loopback_pipe->ipa_connect_params.desc_fifo_sz = BAM_DMA_DESC_FIFO_SIZE;
	loopback_pipe->ipa_connect_params.data_fifo_sz = BAM_DMA_DATA_FIFO_SIZE;
	loopback_pipe->ipa_connect_params.notify = loopback_pipe->ipa_callback;
	loopback_pipe->ipa_connect_params.priv = rndis_ipa_ctx;
	loopback_pipe->ipa_connect_params.ipa_ep_cfg =
		*(loopback_pipe->ipa_ep_cfg);

	/* loopback_pipe->ipa_sps_connect is out param */
	retval = ipa_connect(&loopback_pipe->ipa_connect_params,
			&loopback_pipe->ipa_sps_connect,
			&loopback_pipe->ipa_drv_ep_hdl);
	if (retval) {
		RNDIS_IPA_ERROR("ipa_connect() fail (%d)", retval);
		return retval;
	}
	RNDIS_IPA_DEBUG("ipa_connect() succeeded, ipa_drv_ep_hdl=%d",
			loopback_pipe->ipa_drv_ep_hdl);

	/* SPS pipe has two side handshake
	 * This is the second handshake of IPA->BAMDMA,
	 * This is the BAMDMA side
	 */
	loopback_pipe->dma_sps = sps_alloc_endpoint();
	if (!loopback_pipe->dma_sps) {
		RNDIS_IPA_ERROR("sps_alloc_endpoint() failed ");
		retval = -ENOMEM;
		goto fail_sps_alloc;
	}

	retval = sps_get_config(loopback_pipe->dma_sps,
		&loopback_pipe->dma_connect);
	if (retval) {
		RNDIS_IPA_ERROR("sps_get_config() failed (%d)", retval);
		goto fail_get_cfg;
	}

	/* Start setting the non IPA ep for SPS driver*/
	loopback_pipe->dma_connect.mode = loopback_pipe->mode;

	/* SPS_MODE_DEST: DMA end point is the dest (consumer) IPA->DMA */
	if (loopback_pipe->mode == SPS_MODE_DEST) {

		loopback_pipe->dma_connect.source =
				loopback_pipe->ipa_sps_connect.ipa_bam_hdl;
		loopback_pipe->dma_connect.src_pipe_index =
				loopback_pipe->ipa_sps_connect.ipa_ep_idx;
		loopback_pipe->dma_connect.destination =
				rndis_ipa_ctx->bam_dma_hdl;
		loopback_pipe->dma_connect.dest_pipe_index =
				loopback_pipe->peer_pipe_index;

	/* SPS_MODE_SRC: DMA end point is the source (producer) DMA->IPA */
	} else {

		loopback_pipe->dma_connect.source =
				rndis_ipa_ctx->bam_dma_hdl;
		loopback_pipe->dma_connect.src_pipe_index =
				loopback_pipe->peer_pipe_index;
		loopback_pipe->dma_connect.destination =
				loopback_pipe->ipa_sps_connect.ipa_bam_hdl;
		loopback_pipe->dma_connect.dest_pipe_index =
				loopback_pipe->ipa_sps_connect.ipa_ep_idx;

	}

	loopback_pipe->dma_connect.desc = loopback_pipe->ipa_sps_connect.desc;
	loopback_pipe->dma_connect.data = loopback_pipe->ipa_sps_connect.data;
	loopback_pipe->dma_connect.event_thresh = 0x10;
	/* BAM-to-BAM */
	loopback_pipe->dma_connect.options = SPS_O_AUTO_ENABLE;

	RNDIS_IPA_DEBUG("doing sps_connect() with - ");
	RNDIS_IPA_DEBUG("src bam_hdl:0x%lx, src_pipe#:%d",
			loopback_pipe->dma_connect.source,
			loopback_pipe->dma_connect.src_pipe_index);
	RNDIS_IPA_DEBUG("dst bam_hdl:0x%lx, dst_pipe#:%d",
			loopback_pipe->dma_connect.destination,
			loopback_pipe->dma_connect.dest_pipe_index);

	retval = sps_connect(loopback_pipe->dma_sps,
		&loopback_pipe->dma_connect);
	if (retval) {
		RNDIS_IPA_ERROR("sps_connect() fail for BAMDMA side (%d)",
			retval);
		goto fail_sps_connect;
	}

	RNDIS_IPA_LOG_EXIT();

	return 0;

fail_sps_connect:
fail_get_cfg:
	sps_free_endpoint(loopback_pipe->dma_sps);
fail_sps_alloc:
	ipa_disconnect(loopback_pipe->ipa_drv_ep_hdl);
	return retval;
}

static void rndis_ipa_destroy_loopback_pipe(
		struct rndis_loopback_pipe *loopback_pipe)
{
	sps_disconnect(loopback_pipe->dma_sps);
	sps_free_endpoint(loopback_pipe->dma_sps);
}

/**
 * rndis_ipa_create_loopback() - create a BAM-DMA loopback
 *  in order to replace the USB core
 */
static int rndis_ipa_create_loopback(struct rndis_ipa_dev *rndis_ipa_ctx)
{
	/* The BAM handle should be use as
	 * source/destination in the sps_connect()
	 */
	int retval;

	RNDIS_IPA_LOG_ENTRY();


	retval = sps_ctrl_bam_dma_clk(true);
	if (retval) {
		RNDIS_IPA_ERROR("fail on enabling BAM-DMA clocks");
		return -ENODEV;
	}

	/* Get BAM handle instead of USB handle */
	rndis_ipa_ctx->bam_dma_hdl = sps_dma_get_bam_handle();
	if (!rndis_ipa_ctx->bam_dma_hdl) {
		RNDIS_IPA_ERROR("sps_dma_get_bam_handle() failed");
		return -ENODEV;
	}
	RNDIS_IPA_DEBUG("sps_dma_get_bam_handle() succeeded (0x%x)",
			rndis_ipa_ctx->bam_dma_hdl);

	/* IPA<-BAMDMA, NetDev Rx path (BAMDMA is the USB stub) */
	rndis_ipa_ctx->usb_to_ipa_loopback_pipe.ipa_client =
	IPA_CLIENT_USB_PROD;
	rndis_ipa_ctx->usb_to_ipa_loopback_pipe.peer_pipe_index =
		FROM_USB_TO_IPA_BAMDMA;
	/*DMA EP mode*/
	rndis_ipa_ctx->usb_to_ipa_loopback_pipe.mode = SPS_MODE_SRC;
	rndis_ipa_ctx->usb_to_ipa_loopback_pipe.ipa_ep_cfg =
		&usb_to_ipa_ep_cfg_deaggr_en;
	rndis_ipa_ctx->usb_to_ipa_loopback_pipe.ipa_callback =
			rndis_ipa_packet_receive_notify;
	RNDIS_IPA_DEBUG("setting up IPA<-BAMDAM pipe (RNDIS_IPA RX path)");
	retval = rndis_ipa_loopback_pipe_create(rndis_ipa_ctx,
			&rndis_ipa_ctx->usb_to_ipa_loopback_pipe);
	if (retval) {
		RNDIS_IPA_ERROR("fail to close IPA->BAMDAM pipe");
		goto fail_to_usb;
	}
	RNDIS_IPA_DEBUG("IPA->BAMDAM pipe successfully connected (TX path)");

	/* IPA->BAMDMA, NetDev Tx path (BAMDMA is the USB stub)*/
	rndis_ipa_ctx->ipa_to_usb_loopback_pipe.ipa_client =
		IPA_CLIENT_USB_CONS;
	/*DMA EP mode*/
	rndis_ipa_ctx->ipa_to_usb_loopback_pipe.mode = SPS_MODE_DEST;
	rndis_ipa_ctx->ipa_to_usb_loopback_pipe.ipa_ep_cfg = &ipa_to_usb_ep_cfg;
	rndis_ipa_ctx->ipa_to_usb_loopback_pipe.peer_pipe_index =
		FROM_IPA_TO_USB_BAMDMA;
	rndis_ipa_ctx->ipa_to_usb_loopback_pipe.ipa_callback =
			rndis_ipa_tx_complete_notify;
	RNDIS_IPA_DEBUG("setting up IPA->BAMDAM pipe (RNDIS_IPA TX path)");
	retval = rndis_ipa_loopback_pipe_create(rndis_ipa_ctx,
			&rndis_ipa_ctx->ipa_to_usb_loopback_pipe);
	if (retval) {
		RNDIS_IPA_ERROR("fail to close IPA<-BAMDAM pipe");
		goto fail_from_usb;
	}
	RNDIS_IPA_DEBUG("IPA<-BAMDAM pipe successfully connected(RX path)");

	RNDIS_IPA_LOG_EXIT();

	return 0;

fail_from_usb:
	rndis_ipa_destroy_loopback_pipe(
			&rndis_ipa_ctx->usb_to_ipa_loopback_pipe);
fail_to_usb:

	return retval;
}

static void rndis_ipa_destroy_loopback(struct rndis_ipa_dev *rndis_ipa_ctx)
{
	rndis_ipa_destroy_loopback_pipe(
			&rndis_ipa_ctx->ipa_to_usb_loopback_pipe);
	rndis_ipa_destroy_loopback_pipe(
			&rndis_ipa_ctx->usb_to_ipa_loopback_pipe);
	sps_dma_free_bam_handle(rndis_ipa_ctx->bam_dma_hdl);
	if (sps_ctrl_bam_dma_clk(false))
		RNDIS_IPA_ERROR("fail to disable BAM-DMA clocks");
}

/**
 * rndis_ipa_setup_loopback() - create/destroy a loopback on IPA HW
 *  (as USB pipes loopback) and notify RNDIS_IPA netdev for pipe connected
 * @enable: flag that determines if the loopback should be created or destroyed
 * @rndis_ipa_ctx: driver main context
 *
 * This function is the main loopback logic.
 * It shall create/destory the loopback by using BAM-DMA and notify
 * the netdev accordingly.
 */
static int rndis_ipa_setup_loopback(bool enable,
		struct rndis_ipa_dev *rndis_ipa_ctx)
{
	int retval;

	if (!enable) {
		rndis_ipa_destroy_loopback(rndis_ipa_ctx);
		RNDIS_IPA_DEBUG("loopback destroy done");
		retval = rndis_ipa_pipe_disconnect_notify(rndis_ipa_ctx);
		if (retval) {
			RNDIS_IPA_ERROR("connect notify fail");
			return -ENODEV;
		}
		return 0;
	}

	RNDIS_IPA_DEBUG("creating loopback (instead of USB core)");
	retval = rndis_ipa_create_loopback(rndis_ipa_ctx);
	RNDIS_IPA_DEBUG("creating loopback- %s", (retval ? "FAIL" : "OK"));
	if (retval) {
		RNDIS_IPA_ERROR("Fail to connect loopback");
		return -ENODEV;
	}
	retval = rndis_ipa_pipe_connect_notify(
			rndis_ipa_ctx->usb_to_ipa_loopback_pipe.ipa_drv_ep_hdl,
			rndis_ipa_ctx->ipa_to_usb_loopback_pipe.ipa_drv_ep_hdl,
			BAM_DMA_DATA_FIFO_SIZE,
			15,
			BAM_DMA_DATA_FIFO_SIZE - rndis_ipa_ctx->net->mtu,
			rndis_ipa_ctx);
	if (retval) {
		RNDIS_IPA_ERROR("connect notify fail");
		return -ENODEV;
	}

	return 0;

}

static int rndis_ipa_init_module(void)
{
	pr_info("RNDIS_IPA module is loaded.");
	return 0;
}

static void rndis_ipa_cleanup_module(void)
{
	pr_info("RNDIS_IPA module is unloaded.");
	return;
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("RNDIS_IPA network interface");

late_initcall(rndis_ipa_init_module);
module_exit(rndis_ipa_cleanup_module);
