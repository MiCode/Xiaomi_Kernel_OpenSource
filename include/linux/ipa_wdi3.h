/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018 - 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _IPA_WDI3_H_
#define _IPA_WDI3_H_

#include <linux/ipa.h>

#define IPA_HW_WDI3_TCL_DATA_CMD_ER_DESC_SIZE 32
#define IPA_HW_WDI3_IPA2FW_ER_DESC_SIZE 8

#define IPA_HW_WDI3_MAX_ER_DESC_SIZE \
	(((IPA_HW_WDI3_TCL_DATA_CMD_ER_DESC_SIZE) > \
	(IPA_HW_WDI3_IPA2FW_ER_DESC_SIZE)) ?  \
	(IPA_HW_WDI3_TCL_DATA_CMD_ER_DESC_SIZE) : \
	(IPA_HW_WDI3_IPA2FW_ER_DESC_SIZE))

#define IPA_WDI_MAX_SUPPORTED_SYS_PIPE 3

typedef u32 ipa_wdi_hdl_t;

enum ipa_wdi_version {
	IPA_WDI_1,
	IPA_WDI_2,
	IPA_WDI_3,
	IPA_WDI_VER_MAX
};

#define IPA_WDI3_TX_DIR 1
#define IPA_WDI3_TX1_DIR 2
#define IPA_WDI3_RX_DIR 3
#define IPA_WDI_INST_MAX (2)

/*
 * <28 bytes of rx_msdu_end_tlv> + <16 bytes of attn_tlv> +
 * <52 bytes of rx_msdu_start tlv>.
 */
#define IPA_WDI_RX_TLV_SIZE 96

/** struct ipa_ast_info_type - structure used for updating the AST table.
 * @mac_addr_ad4_valid: bool to indicate whethere peer supports 4 address
 * scheme or not.
 * @sa_valid: bool to indicate whether SA has a valid entry in AST table.
 * @first_msdu_in_mpdu_flag: bool to indicate whether current packet is
 * first frame in mpdu aggregation or not.
 * @sa_idx: Index to AST table.
 * @sa_peer_id: TA peer id associated with SA.
 * @ta_peer_id: TA peer id for the current frame.
 * @skb: Packet pointer.
 */
struct ipa_ast_info_type {
	bool mac_addr_ad4_valid;
	bool sa_valid;
	bool first_msdu_in_mpdu_flag;
	u16 sa_idx;
	u16 sa_peer_id;
	u16 ta_peer_id;
	struct sk_buff *skb;
};

typedef void (*ipa_wdi_mesh_ast_notifier_cb)(void *priv, unsigned long data);


/**
 * struct ipa_wdi_init_in_params - wdi init input parameters
 *
 * @wdi_version: wdi version
 * @notify: uc ready callback
 * @priv: uc ready callback cookie
 * @wdi_notify: bw notification cb
 * inst_id: Instance ID
 * @ast_update: AST update needed or not.
 */
struct ipa_wdi_init_in_params {
	enum ipa_wdi_version wdi_version;
	ipa_uc_ready_cb notify;
	void *priv;
#ifdef IPA_WAN_MSG_IPv6_ADDR_GW_LEN
	ipa_wdi_meter_notifier_cb wdi_notify;
#endif
	int inst_id;
	bool ast_update;
};

/**
 * struct ipa_wdi_init_out_params - wdi init output parameters
 *
 * @is_uC_ready: is uC ready. No API should be called until uC
    is ready.
 * @is_smmu_enable: is smmu enabled
 * @is_over_gsi: is wdi over GSI or uC
 */
struct ipa_wdi_init_out_params {
	bool is_uC_ready;
	bool is_smmu_enabled;
	bool is_over_gsi;
	ipa_wdi_hdl_t hdl;
};

/**
 * struct ipa_wdi_hdr_info - Header to install on IPA HW
 *
 * @hdr: header to install on IPA HW
 * @hdr_len: length of header
 * @dst_mac_addr_offset: destination mac address offset
 * @hdr_type: layer two header type
 */
struct ipa_wdi_hdr_info {
	u8 *hdr;
	u8 hdr_len;
	u8 dst_mac_addr_offset;
	enum ipa_hdr_l2_type hdr_type;
};

/**
 * struct ipa_wdi_reg_intf_in_params - parameters for uC offload
 *	interface registration
 *
 * @netdev_name: network interface name
 * @hdr_info: header information
 * @is_meta_data_valid: if meta data is valid
 * @meta_data: meta data if any
 * @meta_data_mask: meta data mask
 * @is_tx1_used: to indicate whether 2.4g or 5g iface
 * @is_rx1_used: to indicate whether additional RX pipe for
 * tagged traffic is needed
 */
struct ipa_wdi_reg_intf_in_params {
	const char *netdev_name;
	struct ipa_wdi_hdr_info hdr_info[IPA_IP_MAX_WLAN];
	enum ipa_client_type alt_dst_pipe;
	u8 is_meta_data_valid;
	u32 meta_data;
	u32 meta_data_mask;
	u8 is_tx1_used;
	ipa_wdi_hdl_t hdl;
	u8 is_rx1_used;
};

/**
 * struct  ipa_wdi_pipe_setup_info - WDI TX/Rx configuration
 * @ipa_ep_cfg: ipa endpoint configuration
 * @client: type of "client"
 * @transfer_ring_base_pa:  physical address of the base of the transfer ring
 * @transfer_ring_size:  size of the transfer ring
 * @transfer_ring_doorbell_pa:  physical address of the doorbell that
	IPA uC will update the tailpointer of the transfer ring
 * @is_txr_rn_db_pcie_addr: Bool indicated txr ring DB is pcie or not
 * @event_ring_base_pa:  physical address of the base of the event ring
 * @event_ring_size:  event ring size
 * @event_ring_doorbell_pa:  physical address of the doorbell that IPA uC
	will update the headpointer of the event ring
 * @is_evt_rn_db_pcie_addr: Bool indicated evt ring DB is pcie or not
 * @num_pkt_buffers:  Number of pkt buffers allocated. The size of the event
	ring and the transfer ring has to be atleast ( num_pkt_buffers + 1)
 * @pkt_offset: packet offset (wdi header length)
 * @desc_format_template[IPA_HW_WDI3_MAX_ER_DESC_SIZE]:  Holds a cached
	template of the desc format
 */
struct ipa_wdi_pipe_setup_info {
	struct ipa_ep_cfg ipa_ep_cfg;
	enum ipa_client_type client;
	phys_addr_t  transfer_ring_base_pa;
	u32  transfer_ring_size;
	phys_addr_t  transfer_ring_doorbell_pa;
	bool is_txr_rn_db_pcie_addr;

	phys_addr_t  event_ring_base_pa;
	u32  event_ring_size;
	phys_addr_t  event_ring_doorbell_pa;
	bool is_evt_rn_db_pcie_addr;
	u16  num_pkt_buffers;

	u16 pkt_offset;

	u32  desc_format_template[IPA_HW_WDI3_MAX_ER_DESC_SIZE];
};

/**
 * struct  ipa_wdi_pipe_setup_info_smmu - WDI TX/Rx configuration
 * @ipa_ep_cfg: ipa endpoint configuration
 * @client: type of "client"
 * @transfer_ring_base_pa:  physical address of the base of the transfer ring
 * @transfer_ring_size:  size of the transfer ring
 * @transfer_ring_doorbell_pa:  physical address of the doorbell that
	IPA uC will update the tailpointer of the transfer ring
 * @is_txr_rn_db_pcie_addr: Bool indicated  txr ring DB is pcie or not
 * @event_ring_base_pa:  physical address of the base of the event ring
 * @event_ring_size:  event ring size
 * @event_ring_doorbell_pa:  physical address of the doorbell that IPA uC
	will update the headpointer of the event ring
 * @is_evt_rn_db_pcie_addr: Bool indicated evt ring DB is pcie or not
 * @num_pkt_buffers:  Number of pkt buffers allocated. The size of the event
	ring and the transfer ring has to be atleast ( num_pkt_buffers + 1)
 * @pkt_offset: packet offset (wdi header length)
 * @desc_format_template[IPA_HW_WDI3_MAX_ER_DESC_SIZE]:  Holds a cached
	template of the desc format
 */
struct ipa_wdi_pipe_setup_info_smmu {
	struct ipa_ep_cfg ipa_ep_cfg;
	enum ipa_client_type client;
	struct sg_table  transfer_ring_base;
	u32  transfer_ring_size;
	phys_addr_t  transfer_ring_doorbell_pa;
	bool is_txr_rn_db_pcie_addr;

	struct sg_table  event_ring_base;
	u32  event_ring_size;
	phys_addr_t  event_ring_doorbell_pa;
	bool is_evt_rn_db_pcie_addr;
	u16  num_pkt_buffers;

	u16 pkt_offset;

	u32  desc_format_template[IPA_HW_WDI3_MAX_ER_DESC_SIZE];
};

/**
 * struct  ipa_wdi_conn_in_params - information provided by
 *		uC offload client
 * @notify: client callback function
 * @ast_notify: ast notification cb
 * @priv: client cookie
 * @is_smmu_enabled: if smmu is enabled
 * @num_sys_pipe_needed: number of sys pipe needed
 * @sys_in: parameters to setup sys pipe in mcc mode
 * @tx: parameters to connect TX pipe(from IPA to WLAN)
 * @tx_smmu: smmu parameters to connect TX pipe(from IPA to WLAN)
 * @rx: parameters to connect RX pipe(from WLAN to IPA)
 * @rx_smmu: smmu parameters to connect RX pipe(from WLAN to IPA)
 * @is_tx1_used: to notify extra pipe required/not
 * @tx1: parameters to connect TX1 pipe(from IPA to WLAN second pipe)
 * @tx1_smmu: smmu parameters to connect TX1 pipe(from IPA to WLAN second pipe)
 * @hdl: handle associated with this instance.
 * @is_rx1_used: bool to indicate if additional rx pipe required or not
 * @rx1: parameters to connect RX1 pipe(from WLAN to IPA second pipe)
 * @rx1_smmu: smmu parameters to connect RX1 pipe(WLAN to IPA second pipe)
 */
struct ipa_wdi_conn_in_params {
	ipa_notify_cb notify;
	ipa_wdi_mesh_ast_notifier_cb ast_notify;
	void *priv;
	bool is_smmu_enabled;
	u8 num_sys_pipe_needed;
	struct ipa_sys_connect_params sys_in[IPA_WDI_MAX_SUPPORTED_SYS_PIPE];
	union {
		struct ipa_wdi_pipe_setup_info tx;
		struct ipa_wdi_pipe_setup_info_smmu tx_smmu;
	} u_tx;
	union {
		struct ipa_wdi_pipe_setup_info rx;
		struct ipa_wdi_pipe_setup_info_smmu rx_smmu;
	} u_rx;
	bool is_tx1_used;
	union {
		struct ipa_wdi_pipe_setup_info tx;
		struct ipa_wdi_pipe_setup_info_smmu tx_smmu;
	} u_tx1;
	ipa_wdi_hdl_t hdl;
	bool is_rx1_used;
	union {
		struct ipa_wdi_pipe_setup_info rx;
		struct ipa_wdi_pipe_setup_info_smmu rx_smmu;
	} u_rx1;
};

/**
 * struct  ipa_wdi_conn_out_params - information provided
 *				to WLAN driver
 * @tx_uc_db_pa: physical address of IPA uC doorbell for TX
 * @rx_uc_db_pa: physical address of IPA uC doorbell for RX
 * @tx1_uc_db_pa: physical address of IPA uC doorbell for TX1
 * @is_ddr_mapped: flag set to true if address is from DDR
 * @rx1_uc_db_pa: physical address of IPA uC doorbell for RX1
 */
struct ipa_wdi_conn_out_params {
	phys_addr_t tx_uc_db_pa;
	phys_addr_t rx_uc_db_pa;
	phys_addr_t tx1_uc_db_pa;
	bool is_ddr_mapped;
	phys_addr_t rx1_uc_db_pa;
};

/**
 * struct  ipa_wdi_perf_profile - To set BandWidth profile
 *
 * @client: type of client
 * @max_supported_bw_mbps: maximum bandwidth needed (in Mbps)
 */
struct ipa_wdi_perf_profile {
	enum ipa_client_type client;
	u32 max_supported_bw_mbps;
};


/**
 * struct ipa_wdi_capabilities - wdi capability parameters
 *
 * @num_of_instances: Number of WLAN instances supported.
 */
struct ipa_wdi_capabilities_out_params {
	u8 num_of_instances;
};

#if IS_ENABLED(CONFIG_IPA3)

/**
 * ipa_wdi_get_capabilities - Client should call this function to
 * know the WDI capabilities
 *
 * Note: Should not be called from atomic context and only
 * after checking IPA readiness using ipa_register_ipa_ready_cb()
 *
 * @Return 0 on success, negative on failure
 */
int ipa_wdi_get_capabilities(
	struct ipa_wdi_capabilities_out_params *out);

/**
 * ipa_wdi_init - Client should call this function to
 * init WDI IPA offload data path
 *
 * Note: Should not be called from atomic context and only
 * after checking IPA readiness using ipa_register_ipa_ready_cb()
 *
 * @Return 0 on success, negative on failure
 */
int ipa_wdi_init(struct ipa_wdi_init_in_params *in,
	struct ipa_wdi_init_out_params *out);

/** ipa_get_wdi_version - return wdi version
 *
 * @Return void
 */
int ipa_get_wdi_version(void);

/** ipa_wdi_is_tx1_used - return if DBS mode is active
 *
 * @Return bool
 */
bool ipa_wdi_is_tx1_used(void);

/**
 * ipa_wdi_init_per_inst - Client should call this function to
 * init WDI IPA offload data path
 *
 * Note: Should not be called from atomic context and only
 * after checking IPA readiness using ipa_register_ipa_ready_cb()
 *
 * @Return 0 on success, negative on failure
 */
int ipa_wdi_init_per_inst(struct ipa_wdi_init_in_params *in,
	struct ipa_wdi_init_out_params *out);

/**
 * ipa_wdi_cleanup - Client should call this function to
 * clean up WDI IPA offload data path
 *
 * @Return 0 on success, negative on failure
 */
int ipa_wdi_cleanup(void);

/**
 * ipa_wdi_cleanup_per_inst - Client should call this function to
 * clean up WDI IPA offload data path
 *
 * @hdl: hdl to wdi client
 *
 * @Return 0 on success, negative on failure
 */
int ipa_wdi_cleanup_per_inst(ipa_wdi_hdl_t hdl);


/**
 * ipa_wdi_reg_intf - Client should call this function to
 * register interface
 *
 * Note: Should not be called from atomic context
 *
 * @Return 0 on success, negative on failure
 */
int ipa_wdi_reg_intf(
	struct ipa_wdi_reg_intf_in_params *in);

/**
 * ipa_wdi_reg_intf_per_inst - Client should call this function to
 * register interface
 *
 * Note: Should not be called from atomic context
 *
 * @Return 0 on success, negative on failure
 */
int ipa_wdi_reg_intf_per_inst(
	struct ipa_wdi_reg_intf_in_params *in);

/**
 * ipa_wdi_dereg_intf - Client Driver should call this
 * function to deregister before unload and after disconnect
 *
 * @Return 0 on success, negative on failure
 */
int ipa_wdi_dereg_intf(const char *netdev_name);

/**
 * ipa_wdi_dereg_intf_per_inst - Client Driver should call this
 * function to deregister before unload and after disconnect
 *
 * @Return 0 on success, negative on failure
 */
int ipa_wdi_dereg_intf_per_inst(const char *netdev_name, ipa_wdi_hdl_t hdl);

/**
 * ipa_wdi_conn_pipes - Client should call this
 * function to connect pipes
 *
 * @in:	[in] input parameters from client
 * @out: [out] output params to client
 *
 * Note: Should not be called from atomic context
 *
 * @Return 0 on success, negative on failure
 */
int ipa_wdi_conn_pipes(struct ipa_wdi_conn_in_params *in,
	struct ipa_wdi_conn_out_params *out);

/**
 * ipa_wdi_conn_pipes_per_inst - Client should call this
 * function to connect pipes
 *
 * @in:	[in] input parameters from client
 * @out: [out] output params to client
 *
 * Note: Should not be called from atomic context
 *
 * @Return 0 on success, negative on failure
 */
int ipa_wdi_conn_pipes_per_inst(struct ipa_wdi_conn_in_params *in,
	struct ipa_wdi_conn_out_params *out);

/**
 * ipa_wdi_disconn_pipes() - Client should call this
 *		function to disconnect pipes
 *
 * Note: Should not be called from atomic context
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_wdi_disconn_pipes(void);

/**
 * ipa_wdi_disconn_pipes_per_inst() - Client should call this
 *		function to disconnect pipes
 *
 * @hdl: hdl to wdi client
 * Note: Should not be called from atomic context
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_wdi_disconn_pipes_per_inst(ipa_wdi_hdl_t hdl);

/**
 * ipa_wdi_enable_pipes() - Client should call this
 *		function to enable IPA offload data path
 *
 * Note: Should not be called from atomic context
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_wdi_enable_pipes(void);

/**
 * ipa_wdi_enable_pipes_per_inst() - Client should call this
 *		function to enable IPA offload data path
 *
 * @hdl: hdl to wdi client
 * Note: Should not be called from atomic context
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_wdi_enable_pipes_per_inst(ipa_wdi_hdl_t hdl);

/**
 * ipa_wdi_disable_pipes() - Client should call this
 *		function to disable IPA offload data path
 *
 * Note: Should not be called from atomic context
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_wdi_disable_pipes(void);

/**
 * ipa_wdi_disable_pipes_per_inst() - Client should call this
 *		function to disable IPA offload data path
 *
 * @hdl: hdl to wdi client
 * Note: Should not be called from atomic context
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_wdi_disable_pipes_per_inst(ipa_wdi_hdl_t hdl);

/**
 * ipa_wdi_set_perf_profile() - Client should call this function to
 *		set IPA clock bandwidth based on data rates
 *
 * @profile: [in] BandWidth profile to use
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_wdi_set_perf_profile(struct ipa_wdi_perf_profile *profile);

/**
 * ipa_wdi_set_perf_profile_per_inst() - Client should call this function to
 *		set IPA clock bandwidth based on data rates
 *
 * @hdl: hdl to wdi client
 * @profile: [in] BandWidth profile to use
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_wdi_set_perf_profile_per_inst(ipa_wdi_hdl_t hdl,
	struct ipa_wdi_perf_profile *profile);

/**
 * ipa_wdi_create_smmu_mapping() - Create smmu mapping
 *
 * @num_buffers: number of buffers
 *
 * @info: wdi buffer info
 */
int ipa_wdi_create_smmu_mapping(u32 num_buffers,
	struct ipa_wdi_buffer_info *info);

/**
 * ipa_wdi_create_smmu_mapping_per_inst() - Create smmu mapping
 *
 * @hdl: hdl to wdi client
 * @num_buffers: number of buffers
 * @info: wdi buffer info
 */
int ipa_wdi_create_smmu_mapping_per_inst(ipa_wdi_hdl_t hdl,
	u32 num_buffers,
	struct ipa_wdi_buffer_info *info);

/**
 * ipa_wdi_release_smmu_mapping() - Release smmu mapping
 *
 * @num_buffers: number of buffers
 *
 * @info: wdi buffer info
 */
int ipa_wdi_release_smmu_mapping(u32 num_buffers,
	struct ipa_wdi_buffer_info *info);

/**
 * ipa_wdi_release_smmu_mapping_per_inst() - Release smmu mapping
 *
 * @hdl: hdl to wdi client
 * @num_buffers: number of buffers
 *
 * @info: wdi buffer info
 */
int ipa_wdi_release_smmu_mapping_per_inst(ipa_wdi_hdl_t hdl,
	u32 num_buffers,
	struct ipa_wdi_buffer_info *info);

/**
 * ipa_wdi_get_stats() - Query WDI statistics
 * @stats:	[inout] stats blob from client populated by driver
 *
 * Returns:	0 on success, negative on failure
 *
 * @note Cannot be called from atomic context
 *
 */
int ipa_wdi_get_stats(struct IpaHwStatsWDIInfoData_t *stats);


/**
 * ipa_wdi_bw_monitor() - set wdi BW monitoring
 * @info:	[inout] info blob from client populated by driver
 *
 * Returns:	0 on success, negative on failure
 *
 * @note Cannot be called from atomic context
 *
 */
int ipa_wdi_bw_monitor(struct ipa_wdi_bw_info *info);

/**
 * ipa_wdi_sw_stats() - set wdi BW monitoring
 * @info:	[inout] info blob from client populated by driver
 *
 * Returns:	0 on success, negative on failure
 *
 * @note Cannot be called from atomic context
 *
 */
int ipa_wdi_sw_stats(struct ipa_wdi_tx_info *info);

#else /* IS_ENABLED(CONFIG_IPA3) */

/**
 * ipa_wdi_get_capabilities - Client should call this function to
 * know the WDI capabilities
 *
 * Note: Should not be called from atomic context and only
 * after checking IPA readiness using ipa_register_ipa_ready_cb()
 *
 * @Return 0 on success, negative on failure
 */
int ipa_wdi_get_capabilities(
	struct ipa_wdi_capabilities_out_params *out)
{
	return -EPERM;
}

static inline int ipa_wdi_init(struct ipa_wdi_init_in_params *in,
	struct ipa_wdi_init_out_params *out)
{
	return -EPERM;
}

static inline int ipa_wdi_init_per_inst(
	struct ipa_wdi_init_in_params *in,
	struct ipa_wdi_init_out_params *out)
{
	return -EPERM;
}

static inline int ipa_get_wdi_version(void)
{
	return -EPERM;
}

static inline int ipa_wdi_is_tx1_used(void)
{
	return -EPERM;
}

static inline int ipa_wdi_cleanup(void)
{
	return -EPERM;
}

static inline int ipa_wdi_cleanup_per_inst(ipa_wdi_hdl_t hdl)
{
	return -EPERM;
}

static inline int ipa_wdi_reg_intf(
	struct ipa_wdi_reg_intf_in_params *in)
{
	return -EPERM;
}

static inline int ipa_wdi_reg_intf_per_inst(
	struct ipa_wdi_reg_intf_in_params *in)
{
	return -EPERM;
}

static inline int ipa_wdi_dereg_intf(const char *netdev_name)
{
	return -EPERM;
}

static inline int ipa_wdi_dereg_intf_per_inst(const char *netdev_name,
	ipa_wdi_hdl_t hdl)
{
	return -EPERM;
}

static inline int ipa_wdi_conn_pipes(struct ipa_wdi_conn_in_params *in,
	struct ipa_wdi_conn_out_params *out)
{
	return -EPERM;
}

static inline int ipa_wdi_conn_pipes_per_inst(
	struct ipa_wdi_conn_in_params *in,
	struct ipa_wdi_conn_out_params *out)
{
	return -EPERM;
}

static inline int ipa_wdi_disconn_pipes(void)
{
	return -EPERM;
}

static inline int ipa_wdi_disconn_pipes_per_inst(ipa_wdi_hdl_t hdl)
{
	return -EPERM;
}


static inline int ipa_wdi_enable_pipes(void)
{
	return -EPERM;
}

static inline int ipa_wdi_enable_pipes_per_inst(ipa_wdi_hdl_t hdl)
{
	return -EPERM;
}

static inline int ipa_wdi_disable_pipes(void)
{
	return -EPERM;
}

static inline int ipa_wdi_disable_pipes_per_inst(ipa_wdi_hdl_t hdl)
{
	return -EPERM;
}

static inline int ipa_wdi_set_perf_profile(
	struct ipa_wdi_perf_profile *profile)
{
	return -EPERM;
}

static inline int ipa_wdi_set_perf_profile_per_inst(
	ipa_wdi_hdl_t hdl,
	struct ipa_wdi_perf_profile *profile)
{
	return -EPERM;
}

static inline int ipa_wdi_create_smmu_mapping(u32 num_buffers,
	struct ipa_wdi_buffer_info *info)
{
	return -EPERM;
}

static inline int ipa_wdi_create_smmu_mapping_per_inst(
	ipa_wdi_hdl_t hdl,
	u32 num_buffers,
	struct ipa_wdi_buffer_info *info)
{
	return -EPERM;
}

static inline int ipa_wdi_release_smmu_mapping(u32 num_buffers,
	struct ipa_wdi_buffer_info *info)
{
	return -EPERM;
}

static inline int ipa_wdi_release_smmu_mapping_per_inst(
	ipa_wdi_hdl_t hdl,
	u32 num_buffers,
	struct ipa_wdi_buffer_info *info)
{
	return -EPERM;
}

static inline int ipa_wdi_get_stats(struct IpaHwStatsWDIInfoData_t *stats)
{
	return -EPERM;
}

static inline int ipa_wdi_bw_monitor(struct ipa_wdi_bw_info *info)
{
	return -EPERM;
}

static inline int ipa_wdi_sw_stats(struct ipa_wdi_tx_info *info)
{
	return -EPERM;
}

#endif /* IS_ENABLED(CONFIG_IPA3) */

#endif /* _IPA_WDI3_H_ */
