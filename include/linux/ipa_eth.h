/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _IPA_ETH_H_
#define _IPA_ETH_H_

#include <linux/ipa.h>
#include <linux/msm_ipa.h>
#include <linux/msm_gsi.h>

#define IPA_ETH_API_VER 2

/* New architecture prototypes */

typedef void (*ipa_eth_ready_cb)(void *user_data);
typedef u32 ipa_eth_hdl_t;

/**
 * struct ipa_eth_ready_cb - eth readiness parameters
 *
 * @notify: ipa_eth client ready callback notifier
 * @userdata: userdata for ipa_eth ready cb
 * @is_eth_ready: true if ipa_eth client is already ready
 */
struct ipa_eth_ready {
	ipa_eth_ready_cb notify;
	void *userdata;

	/* out params */
	bool is_eth_ready;
};

/**
 * enum ipa_eth_client_type - names for the various IPA
 * eth "clients".
 */
enum ipa_eth_client_type {
	IPA_ETH_CLIENT_AQC107,
	IPA_ETH_CLIENT_AQC113,
	IPA_ETH_CLIENT_RTK8111K,
	IPA_ETH_CLIENT_RTK8125B,
	IPA_ETH_CLIENT_NTN,
	IPA_ETH_CLIENT_EMAC,
	IPA_ETH_CLIENT_NTN3,
	IPA_ETH_CLIENT_MAX,
};

/**
 * enum ipa_eth_pipe_traffic_type - traffic type for the various IPA
 * eth "pipes".
 */
enum ipa_eth_pipe_traffic_type {
	IPA_ETH_PIPE_BEST_EFFORT,
	IPA_ETH_PIPE_LOW_LATENCY,
	IPA_ETH_PIPE_TRAFFIC_TYPE_MAX,
};

/**
 * enum ipa_eth_pipe_direction - pipe direcitons for same
 * ethernet client.
 */
enum ipa_eth_pipe_direction {
	IPA_ETH_PIPE_DIR_TX,
	IPA_ETH_PIPE_DIR_RX,
	IPA_ETH_PIPE_DIR_MAX,
};

#define IPA_ETH_INST_ID_MAX (2)

/**
 * struct ipa_eth_ntn_setup_info - parameters for ntn ethernet
 * offloading
 *
 * @bar_addr: bar PA to access NTN register
 * @tail_ptr_offs: tail ptr offset
 */
struct ipa_eth_ntn_setup_info {
	phys_addr_t bar_addr;
	phys_addr_t tail_ptr_offs;
};

/**
 * struct ipa_eth_aqc_setup_info - parameters for aqc ethernet
 * offloading
 *
 * @bar_addr: bar PA to access AQC register
 * @head_ptr_offs: head ptr offset
 * @aqc_ch: AQC ch number
 * @dest_tail_ptr_offs: tail ptr offset
 */
struct ipa_eth_aqc_setup_info {
	phys_addr_t bar_addr;
	phys_addr_t head_ptr_offs;
	u8 aqc_ch;
	phys_addr_t dest_tail_ptr_offs;
};


/**
 * struct ipa_eth_realtek_setup_info - parameters for realtek ethernet
 * offloading
 *
 * @bar_addr: bar PA to access RTK register
 * @bar_size: bar region size
 * @queue_number: Which RTK queue to check the status on
 * @dest_tail_ptr_offs: tail ptr offset
 */
struct ipa_eth_realtek_setup_info {
	phys_addr_t bar_addr;
	u32 bar_size;
	u8 queue_number;
	phys_addr_t dest_tail_ptr_offs;
};

/**
 * struct ipa_eth_buff_smmu_map -  IPA iova->pa SMMU mapping
 * @iova: virtual address of the data buffer
 * @pa: physical address of the data buffer
 */
struct ipa_eth_buff_smmu_map {
	dma_addr_t iova;
	phys_addr_t pa;
};

/**
 * struct  ipa_eth_pipe_setup_info - info needed for IPA setups
 * @is_transfer_ring_valid: if transfer ring is needed
 * @transfer_ring_base:  the base of the transfer ring
 * @transfer_ring_sgt: sgtable of transfer ring
 * @transfer_ring_size:  size of the transfer ring
 * @is_buffer_pool_valid: if buffer pool is needed
 * @buffer_pool_base_addr:  base of buffer pool address
 * @buffer_pool_base_sgt:  sgtable of buffer pool
 * @data_buff_list_size: number of buffers
 * @data_buff_list: array of data buffer list
 * @fix_buffer_size: buffer size
 * @notify:	callback for exception/embedded packets
 * @priv: priv for exception callback
 * @client_info: vendor specific pipe setup info
 * @db_pa: doorbell physical address
 * @db_val: doorbell value ethernet HW need to ring
 */
struct ipa_eth_pipe_setup_info {
	/* transfer ring info */
	bool is_transfer_ring_valid;
	dma_addr_t  transfer_ring_base;
	struct sg_table *transfer_ring_sgt;
	u32 transfer_ring_size;

	/* buffer pool info */
	bool is_buffer_pool_valid;
	dma_addr_t buffer_pool_base_addr;
	struct sg_table *buffer_pool_base_sgt;

	/* buffer info */
	u32 data_buff_list_size;
	struct ipa_eth_buff_smmu_map *data_buff_list;
	u32 fix_buffer_size;

	/* client notify cb */
	ipa_notify_cb notify;
	void *priv;

	/* vendor specific info */
	union {
		struct ipa_eth_aqc_setup_info aqc;
		struct ipa_eth_realtek_setup_info rtk;
		struct ipa_eth_ntn_setup_info ntn;
	} client_info;

	/* output params */
	phys_addr_t db_pa;
	u32 db_val;
};

/**
 * struct  ipa_eth_client_pipe_info - ETH pipe/gsi related configuration
 * @link: link of ep for different client function on same ethernet HW
 * @dir: TX or RX direction
 * @info: tx/rx pipe setup info
 * @client_info: client the pipe belongs to
 * @pipe_hdl: output params, pipe handle
 */
struct ipa_eth_client_pipe_info {
	struct list_head link;
	enum ipa_eth_pipe_direction dir;
	struct ipa_eth_pipe_setup_info info;
	struct ipa_eth_client *client_info;

	/* output params */
	ipa_eth_hdl_t pipe_hdl;
};

/**
 * struct  ipa_eth_client - client info per traffic type
 * provided by offload client
 * @client_type: ethernet client type
 * @inst_id: instance id for dual NIC support
 * @net_dev: network device client belongs to
 * @traffic_type: traffic type
 * @pipe_list: list of pipes with same traffic type
 * @priv: private data for client
 * @test: is test client
 */
struct ipa_eth_client {
	enum ipa_eth_client_type client_type;
	u8 inst_id;

	/* traffic type */
	enum ipa_eth_pipe_traffic_type traffic_type;
	struct list_head pipe_list;

	/* client specific priv data*/
	void *priv;
	bool test;

	/* vendor driver */
	struct net_device *net_dev;
};

/**
 * struct  ipa_eth_perf_profile - To set BandWidth profile
 *
 * @max_supported_bw_mbps: maximum bandwidth needed (in Mbps)
 */
struct ipa_eth_perf_profile {
	u32 max_supported_bw_mbps;
};

/**
 * struct ipa_eth_hdr_info - Header to install on IPA HW
 *
 * @hdr: header to install on IPA HW
 * @hdr_len: length of header
 * @dst_mac_addr_offset: destination mac address offset
 * @hdr_type: layer two header type
 */
struct ipa_eth_hdr_info {
	u8 *hdr;
	u8 hdr_len;
	u8 dst_mac_addr_offset;
	enum ipa_hdr_l2_type hdr_type;
};

/**
 * struct ipa_eth_intf_info - parameters for ipa offload
 *	interface registration
 *
 * @client: ipa ethernet client associated with the interface
 * @is_conn_evt: whether or not trigger periph conn/disconn event
 * @net_dev: network device
 */
struct ipa_eth_intf_info {
	struct ipa_eth_client *client;

	/* trigger iface peripheral event */
	bool is_conn_evt;

	/* IPA internal fields */
	struct net_device *net_dev;
};

int ipa_eth_register_ready_cb(struct ipa_eth_ready *ready_info);
int ipa_eth_unregister_ready_cb(struct ipa_eth_ready *ready_info);
int ipa_eth_client_conn_pipes(struct ipa_eth_client *client);
int ipa_eth_client_disconn_pipes(struct ipa_eth_client *client);
int ipa_eth_client_reg_intf(struct ipa_eth_intf_info *intf);
int ipa_eth_client_unreg_intf(struct ipa_eth_intf_info *intf);
int ipa_eth_client_set_perf_profile(struct ipa_eth_client *client,
	struct ipa_eth_perf_profile *profile);
enum ipa_client_type ipa_eth_get_ipa_client_type_from_eth_type(
	enum ipa_eth_client_type eth_client_type, enum ipa_eth_pipe_direction dir);
bool ipa_eth_client_exist(
	enum ipa_eth_client_type eth_client_type, int inst_id);

#endif // _IPA_ETH_H_
