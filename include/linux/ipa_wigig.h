/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _IPA_WIGIG_H_
#define _IPA_WIGIG_H_

#include <linux/msm_ipa.h>
#include <linux/ipa.h>

typedef void (*ipa_wigig_misc_int_cb)(void *priv);

/*
 * struct ipa_wigig_init_in_params - wigig init input parameters
 *
 * @periph_baddr_pa: physical address of wigig HW base
 * @pseudo_cause_pa: physical address of wigig HW pseudo_cause register
 * @int_gen_tx_pa: physical address of wigig HW int_gen_tx register
 * @int_gen_rx_pa: physical address of wigig HW int_gen_rx register
 * @dma_ep_misc_pa: physical address of wigig HW dma_ep_misc register
 * @notify: uc ready callback
 * @int_notify: wigig misc interrupt callback
 * @priv: uc ready callback cookie
 */
struct ipa_wigig_init_in_params {
	phys_addr_t periph_baddr_pa;
	phys_addr_t pseudo_cause_pa;
	phys_addr_t int_gen_tx_pa;
	phys_addr_t int_gen_rx_pa;
	phys_addr_t dma_ep_misc_pa;
	ipa_uc_ready_cb notify;
	ipa_wigig_misc_int_cb int_notify;
	void *priv;
};

/*
 * struct ipa_wigig_init_out_params - wigig init output parameters
 *
 * @is_uC_ready: is uC ready. No API should be called until uC is ready.
 * @uc_db_pa: physical address of IPA uC doorbell
 * @lan_rx_napi_enable: if we use NAPI in the LAN rx
 */
struct ipa_wigig_init_out_params {
	bool is_uc_ready;
	phys_addr_t uc_db_pa;
	bool lan_rx_napi_enable;
};

/*
 * struct ipa_wigig_hdr_info - Header to install on IPA HW
 *
 * @hdr: header to install on IPA HW
 * @hdr_len: length of header
 * @dst_mac_addr_offset: destination mac address offset
 * @hdr_type: layer two header type
 */
struct ipa_wigig_hdr_info {
	u8 *hdr;
	u8 hdr_len;
	u8 dst_mac_addr_offset;
	enum ipa_hdr_l2_type hdr_type;
};

/*
 * struct ipa_wigig_reg_intf_in_params - parameters for offload interface
 *	registration
 *
 * @netdev_name: network interface name
 * @netdev_mac: netdev mac address
 * @hdr_info: header information
 */
struct ipa_wigig_reg_intf_in_params {
	const char *netdev_name;
	u8 netdev_mac[IPA_MAC_ADDR_SIZE];
	struct ipa_wigig_hdr_info hdr_info[IPA_IP_MAX];
};

/*
 * struct ipa_wigig_pipe_setup_info - WIGIG TX/Rx configuration
 * @desc_ring_base_pa: physical address of the base of the descriptor ring
 * @desc_ring_size: size of the descriptor ring in bytes
 * @desc_ring_HWHEAD_pa: physical address of the wigig descriptor ring HWHEAD
 * @desc_ring_HWTAIL_pa: physical address of the wigig descriptor ring HWTAIL
 * @status_ring_base_pa: physical address of the base of the status ring
 * @status_ring_size: status ring size in bytes
 * @desc_ring_HWHEAD_pa: physical address of the wigig descriptor ring HWHEAD
 * @desc_ring_HWTAIL_pa: physical address of the wigig descriptor ring HWTAIL
 */
struct ipa_wigig_pipe_setup_info {
	phys_addr_t desc_ring_base_pa;
	u16 desc_ring_size;
	phys_addr_t desc_ring_HWHEAD_pa;
	phys_addr_t desc_ring_HWTAIL_pa;

	phys_addr_t status_ring_base_pa;
	u16 status_ring_size;
	phys_addr_t status_ring_HWHEAD_pa;
	phys_addr_t status_ring_HWTAIL_pa;
};

/*
 * struct ipa_wigig_pipe_setup_info_smmu - WIGIG TX/Rx configuration smmu mode
 * @desc_ring_base: sg_table of the base of the descriptor ring
 * @desc_ring_base_iova: IO virtual address mapped to physical base address
 * @desc_ring_size: size of the descriptor ring in bytes
 * @desc_ring_HWHEAD_pa: physical address of the wigig descriptor ring HWHEAD
 * @desc_ring_HWTAIL_pa: physical address of the wigig descriptor ring HWTAIL
 * @status_ring_base: sg_table of the base of the status ring
 * @status_ring_base_iova: IO virtual address mapped to physical base address
 * @status_ring_size: status ring size in bytes
 * @desc_ring_HWHEAD_pa: physical address of the wigig descriptor ring HWHEAD
 * @desc_ring_HWTAIL_pa: physical address of the wigig descriptor ring HWTAIL
 */
struct ipa_wigig_pipe_setup_info_smmu {
	struct sg_table desc_ring_base;
	u64 desc_ring_base_iova;
	u16 desc_ring_size;
	phys_addr_t desc_ring_HWHEAD_pa;
	phys_addr_t desc_ring_HWTAIL_pa;

	struct sg_table status_ring_base;
	u64 status_ring_base_iova;
	u16 status_ring_size;
	phys_addr_t status_ring_HWHEAD_pa;
	phys_addr_t status_ring_HWTAIL_pa;
};

/*
 * struct ipa_wigig_rx_pipe_data_buffer_info - WIGIG Rx data buffer
 *	configuration
 * @data_buffer_base_pa: physical address of the physically contiguous
 *			Rx data buffer
 * @data_buffer_size: size of the data buffer
 */
struct ipa_wigig_rx_pipe_data_buffer_info {
	phys_addr_t data_buffer_base_pa;
	u32 data_buffer_size;
};

/*
 * struct ipa_wigig_rx_pipe_data_buffer_info_smmu - WIGIG Rx data buffer
 *	configuration smmu mode
 * @data_buffer_base: sg_table of the physically contiguous
 *			Rx data buffer
 * @data_buffer_base_iova: IO virtual address mapped to physical base address
 * @data_buffer_size: size of the data buffer
 */
struct ipa_wigig_rx_pipe_data_buffer_info_smmu {
	struct sg_table data_buffer_base;
	u64 data_buffer_base_iova;
	u32 data_buffer_size;
};

/*
 * struct ipa_wigig_conn_rx_in_params - information provided by
 *				WIGIG offload client for Rx pipe
 * @notify: client callback function
 * @priv: client cookie
 * @pipe: parameters to connect Rx pipe (WIGIG to IPA)
 * @dbuff: Rx data buffer info
 */
struct ipa_wigig_conn_rx_in_params {
	ipa_notify_cb notify;
	void *priv;
	struct ipa_wigig_pipe_setup_info pipe;
	struct ipa_wigig_rx_pipe_data_buffer_info dbuff;
};

/*
 * struct ipa_wigig_conn_rx_in_params_smmu - information provided by
 *				WIGIG offload client for Rx pipe
 * @notify: client callback function
 * @priv: client cookie
 * @pipe_smmu: parameters to connect Rx pipe (WIGIG to IPA) smmu mode
 * @dbuff_smmu: Rx data buffer info smmu mode
 */
struct ipa_wigig_conn_rx_in_params_smmu {
	ipa_notify_cb notify;
	void *priv;
	struct ipa_wigig_pipe_setup_info_smmu pipe_smmu;
	struct ipa_wigig_rx_pipe_data_buffer_info_smmu dbuff_smmu;
};

/*
 * struct ipa_wigig_conn_out_params - information provided
 *				to WIGIG driver
 * @client: client type allocated by IPA driver
 */
struct ipa_wigig_conn_out_params {
	enum ipa_client_type client;
};

/*
 * struct ipa_wigig_tx_pipe_data_buffer_info - WIGIG Tx data buffer
 *	configuration
 * @data_buffer_size: size of a single data buffer
 */
struct ipa_wigig_tx_pipe_data_buffer_info {
	u32 data_buffer_size;
};

/*
 * struct ipa_wigig_tx_pipe_data_buffer_info_smmu - WIGIG Tx data buffer
 *				configuration smmu mode
 * @data_buffer_base_pa: sg_tables of the Tx data buffers
 * @data_buffer_base_iova: IO virtual address mapped to physical base address
 * @num_buffers: number of buffers
 * @data_buffer_size: size of a single data buffer
 */
struct ipa_wigig_tx_pipe_data_buffer_info_smmu {
	struct sg_table *data_buffer_base;
	u64 *data_buffer_base_iova;
	u32 num_buffers;
	u32 data_buffer_size;
};

/*
 * struct ipa_wigig_conn_tx_in_params - information provided by
 *		wigig offload client for Tx pipe
 * @pipe: parameters to connect Tx pipe (IPA to WIGIG)
 * @dbuff: Tx data buffer info
 * @int_gen_tx_bit_num: bit in int_gen_tx register associated with this client
 * @client_mac: MAC address of client to be connected
 */
struct ipa_wigig_conn_tx_in_params {
	struct ipa_wigig_pipe_setup_info pipe;
	struct ipa_wigig_tx_pipe_data_buffer_info dbuff;
	u8 int_gen_tx_bit_num;
	u8 client_mac[IPA_MAC_ADDR_SIZE];
};

/*
 * struct ipa_wigig_conn_tx_in_params_smmu - information provided by
 *		wigig offload client for Tx pipe
 * @pipe_smmu: parameters to connect Tx pipe (IPA to WIGIG) smmu mode
 * @dbuff_smmu: Tx data buffer info smmu mode
 * @int_gen_tx_bit_num: bit in int_gen_tx register associated with this client
 * @client_mac: MAC address of client to be connected
 */
struct ipa_wigig_conn_tx_in_params_smmu {
	struct ipa_wigig_pipe_setup_info_smmu pipe_smmu;
	struct ipa_wigig_tx_pipe_data_buffer_info_smmu dbuff_smmu;
	u8 int_gen_tx_bit_num;
	u8 client_mac[IPA_MAC_ADDR_SIZE];
};

#if IS_ENABLED(CONFIG_IPA3)

/*
 * ipa_wigig_init - Client should call this function to
 * init WIGIG IPA offload data path
 *
 * Note: Should not be called from atomic context
 *
 * @Return 0 on success, negative on failure
 */
int ipa_wigig_init(struct ipa_wigig_init_in_params *in,
	struct ipa_wigig_init_out_params *out);

/*
 * ipa_wigig_cleanup - Client should call this function to
 * clean up WIGIG IPA offload data path
 *
 * @Return 0 on success, negative on failure
 */
int ipa_wigig_cleanup(void);

/*
 * ipa_wigig_is_smmu_enabled - get smmu state
 *
 * @Return true if smmu is enabled, false if disabled
 */
bool ipa_wigig_is_smmu_enabled(void);

/*
 * ipa_wigig_reg_intf - Client should call this function to
 * register interface
 *
 * Note: Should not be called from atomic context
 *
 * @Return 0 on success, negative on failure
 */
int ipa_wigig_reg_intf(struct ipa_wigig_reg_intf_in_params *in);

/*
 * ipa_wigig_dereg_intf - Client Driver should call this
 * function to deregister before unload and after disconnect
 *
 * @Return 0 on success, negative on failure
 */
int ipa_wigig_dereg_intf(const char *netdev_name);

/*
 * ipa_wigig_conn_rx_pipe - Client should call this
 * function to connect the rx (UL) pipe
 *
 * @in: [in] input parameters from client
 * @out: [out] output params to client
 *
 * Note: Non SMMU mode only, Should not be called from atomic context
 *
 * @Return 0 on success, negative on failure
 */
int ipa_wigig_conn_rx_pipe(struct ipa_wigig_conn_rx_in_params *in,
	struct ipa_wigig_conn_out_params *out);

/*
 * ipa_wigig_conn_rx_pipe_smmu - Client should call this
 * function to connect the rx (UL) pipe
 *
 * @in: [in] input parameters from client
 * @out: [out] output params to client
 *
 * Note: SMMU mode only, Should not be called from atomic context
 *
 * @Return 0 on success, negative on failure
 */
int ipa_wigig_conn_rx_pipe_smmu(struct ipa_wigig_conn_rx_in_params_smmu *in,
	struct ipa_wigig_conn_out_params *out);

/*
 * ipa_wigig_conn_client - Client should call this
 * function to connect one of the tx (DL) pipes when a WIGIG client connects
 *
 * @in: [in] input parameters from client
 * @out: [out] output params to client
 *
 * Note: Non SMMU mode only, Should not be called from atomic context
 *
 * @Return 0 on success, negative on failure
 */
int ipa_wigig_conn_client(struct ipa_wigig_conn_tx_in_params *in,
	struct ipa_wigig_conn_out_params *out);

/*
 * ipa_wigig_conn_client_smmu - Client should call this
 * function to connect one of the tx (DL) pipes when a WIGIG client connects
 *
 * @in: [in] input parameters from client
 * @out: [out] output params to client
 *
 * Note: SMMU mode only, Should not be called from atomic context
 *
 * @Return 0 on success, negative on failure
 */
int ipa_wigig_conn_client_smmu(struct ipa_wigig_conn_tx_in_params_smmu *in,
	struct ipa_wigig_conn_out_params *out);

/*
 * ipa_wigig_disconn_pipe() - Client should call this
 *		function to disconnect a pipe
 *
 * @client: [in] pipe to be disconnected
 *
 * Note: Should not be called from atomic context
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_wigig_disconn_pipe(enum ipa_client_type client);

/*
 * ipa_wigig_enable_pipe() - Client should call this
 *		function to enable IPA offload data path
 *
 * @client: [in] pipe to be enabled
 * Note: Should not be called from atomic context
 *
 * Returns: 0 on success, negative on failure
 */

int ipa_wigig_enable_pipe(enum ipa_client_type client);

/*
 * ipa_wigig_disable_pipe() - Client should call this
 *		function to disable IPA offload data path
 *
 * @client: [in] pipe to be disabled
 * Note: Should not be called from atomic context
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_wigig_disable_pipe(enum ipa_client_type client);

/*
 * ipa_wigig_tx_dp() - transmit tx packet through IPA to 11ad HW
 *
 * @dst: [in] destination ipa client pipe to be used
 * @skb: [in] skb to be transmitted
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_wigig_tx_dp(enum ipa_client_type dst, struct sk_buff *skb);

/**
 * ipa_wigig_set_perf_profile() - Client should call this function to
 *		set IPA clock bandwidth based on data rates
 *
 * @max_supported_bw_mbps: [in] maximum bandwidth needed (in Mbps)
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_wigig_set_perf_profile(u32 max_supported_bw_mbps);

#else /* IS_ENABLED(CONFIG_IPA3) */
static inline int ipa_wigig_init(struct ipa_wigig_init_in_params *in,
	struct ipa_wigig_init_out_params *out)
{
	return -EPERM;
}

static inline int ipa_wigig_cleanup(void)
{
	return -EPERM;
}

static inline bool ipa_wigig_is_smmu_enabled(void)
{
	return -EPERM;
}

static inline int ipa_wigig_reg_intf(struct ipa_wigig_reg_intf_in_params *in)
{
	return -EPERM;
}

static inline int ipa_wigig_dereg_intf(const char *netdev_name)
{
	return -EPERM;
}

static inline int ipa_wigig_conn_rx_pipe(
	struct ipa_wigig_conn_rx_in_params *in,
	struct ipa_wigig_conn_out_params *out)
{
	return -EPERM;
}

static inline int ipa_wigig_conn_rx_pipe_smmu(
	struct ipa_wigig_conn_rx_in_params_smmu *in,
	struct ipa_wigig_conn_out_params *out)
{
	return -EPERM;
}

static inline int ipa_wigig_conn_client(
	struct ipa_wigig_conn_tx_in_params *in,
	struct ipa_wigig_conn_out_params *out)
{
	return -EPERM;
}

static inline int ipa_wigig_conn_client_smmu(
	struct ipa_wigig_conn_tx_in_params_smmu *in,
	struct ipa_wigig_conn_out_params *out)
{
	return -EPERM;
}

static inline int ipa_wigig_disconn_pipe(enum ipa_client_type client)
{
	return -EPERM;
}

static inline int ipa_wigig_enable_pipe(enum ipa_client_type client)
{
	return -EPERM;
}

static inline int ipa_wigig_disable_pipe(enum ipa_client_type client)
{
	return -EPERM;
}

static inline int ipa_wigig_tx_dp(enum ipa_client_type dst,
	struct sk_buff *skb)
{
	return -EPERM;
}

static inline int ipa_wigig_set_perf_profile(u32 max_supported_bw_mbps)
{
	return -EPERM;
}
#endif /* IS_ENABLED(CONFIG_IPA3) */
#endif /* _IPA_WIGIG_H_ */
