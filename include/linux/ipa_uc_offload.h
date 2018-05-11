/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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

#ifndef _IPA_UC_OFFLOAD_H_
#define _IPA_UC_OFFLOAD_H_

#include <linux/ipa.h>

/**
 * enum ipa_uc_offload_proto
 * Protocol type: either WDI or Neutrino
 *
 * @IPA_UC_WDI: wdi Protocol
 * @IPA_UC_NTN: Neutrino Protocol
 */
enum ipa_uc_offload_proto {
	IPA_UC_INVALID = 0,
	IPA_UC_WDI = 1,
	IPA_UC_NTN = 2,
	IPA_UC_MAX_PROT_SIZE
};

/**
 * struct ipa_hdr_info - Header to install on IPA HW
 *
 * @hdr: header to install on IPA HW
 * @hdr_len: length of header
 * @dst_mac_addr_offset: destination mac address offset
 * @hdr_type: layer two header type
 */
struct ipa_hdr_info {
	u8 *hdr;
	u8 hdr_len;
	u8 dst_mac_addr_offset;
	enum ipa_hdr_l2_type hdr_type;
};

/**
 * struct ipa_uc_offload_intf_params - parameters for uC offload
 *	interface registration
 *
 * @netdev_name: network interface name
 * @notify:	callback for exception/embedded packets
 * @priv: callback cookie
 * @hdr_info: header information
 * @meta_data: meta data if any
 * @meta_data_mask: meta data mask
 * @proto: uC offload protocol type
 * @alt_dst_pipe: alternate routing output pipe
 */
struct ipa_uc_offload_intf_params {
	const char *netdev_name;
	ipa_notify_cb notify;
	void *priv;
	struct ipa_hdr_info hdr_info[IPA_IP_MAX];
	u8 is_meta_data_valid;
	u32 meta_data;
	u32 meta_data_mask;
	enum ipa_uc_offload_proto proto;
	enum ipa_client_type alt_dst_pipe;
};

/**
 * struct ntn_buff_smmu_map -  IPA iova->pa SMMU mapping
 * @iova: virtual address of the data buffer
 * @pa: physical address of the data buffer
 */
struct ntn_buff_smmu_map {
	dma_addr_t iova;
	phys_addr_t pa;
};

/**
 * struct  ipa_ntn_setup_info - NTN TX/Rx configuration
 * @client: type of "client" (IPA_CLIENT_ODU#_PROD/CONS)
 * @smmu_enabled: SMMU is enabled for uC or not
 * @ring_base_pa: physical address of the base of the Tx/Rx ring
 * @ring_base_iova: virtual address of the base of the Tx/Rx ring
 * @ring_base_sgt:Scatter table for ntn_rings,contains valid non NULL
 *			value when ENAC S1-SMMU enabed, else NULL.
 * @ntn_ring_size: size of the Tx/Rx ring (in terms of elements)
 * @buff_pool_base_pa: physical address of the base of the Tx/Rx buffer pool
 * @buff_pool_base_iova: virtual address of the base of the Tx/Rx buffer pool
 * @buff_pool_base_sgt: Scatter table for buffer pools,contains valid non NULL
 *			 value when EMAC S1-SMMU enabed, else NULL.
 * @num_buffers: Rx/Tx buffer pool size (in terms of elements)
 * @data_buff_size: size of the each data buffer allocated in DDR
 * @ntn_reg_base_ptr_pa: physical address of the Tx/Rx NTN Ring's
 *						tail pointer
 */
struct ipa_ntn_setup_info {
	enum ipa_client_type client;
	bool smmu_enabled;
	phys_addr_t ring_base_pa;
	dma_addr_t ring_base_iova;
	struct sg_table *ring_base_sgt;

	u32 ntn_ring_size;

	phys_addr_t buff_pool_base_pa;
	dma_addr_t buff_pool_base_iova;
	struct sg_table *buff_pool_base_sgt;

	struct ntn_buff_smmu_map *data_buff_list;

	u32 num_buffers;

	u32 data_buff_size;

	phys_addr_t ntn_reg_base_ptr_pa;
};

/**
 * struct ipa_uc_offload_out_params - out parameters for uC offload
 *
 * @clnt_hndl: Handle that client need to pass during
 *	further operations
 */
struct ipa_uc_offload_out_params {
	u32 clnt_hndl;
};

/**
 * struct  ipa_ntn_conn_in_params - NTN TX/Rx connect parameters
 * @ul: parameters to connect UL pipe(from Neutrino to IPA)
 * @dl: parameters to connect DL pipe(from IPA to Neutrino)
 */
struct ipa_ntn_conn_in_params {
	struct ipa_ntn_setup_info ul;
	struct ipa_ntn_setup_info dl;
};

/**
 * struct  ipa_ntn_conn_out_params - information provided
 *				to uC offload client
 * @ul_uc_db_pa: physical address of IPA uc doorbell for UL
 * @dl_uc_db_pa: physical address of IPA uc doorbell for DL
 * @clnt_hdl: opaque handle assigned to offload client
 */
struct ipa_ntn_conn_out_params {
	phys_addr_t ul_uc_db_pa;
	phys_addr_t dl_uc_db_pa;
};

/**
 * struct  ipa_uc_offload_conn_in_params - information provided by
 *		uC offload client
 * @clnt_hndl: Handle that return as part of reg interface
 * @proto: Protocol to use for offload data path
 * @ntn: uC RX/Tx configuration info
 */
struct ipa_uc_offload_conn_in_params {
	u32 clnt_hndl;
	union {
		struct ipa_ntn_conn_in_params ntn;
	} u;
};

/**
 * struct  ipa_uc_offload_conn_out_params - information provided
 *		to uC offload client
 * @ul_uc_db_pa: physical address of IPA uc doorbell for UL
 * @dl_uc_db_pa: physical address of IPA uc doorbell for DL
 * @clnt_hdl: opaque handle assigned to offload client
 */
struct ipa_uc_offload_conn_out_params {
	union {
		struct ipa_ntn_conn_out_params ntn;
	} u;
};

/**
 * struct  ipa_perf_profile - To set BandWidth profile
 *
 * @client: type of "client" (IPA_CLIENT_ODU#_PROD/CONS)
 * @max_supported_bw_mbps: maximum bandwidth needed (in Mbps)
 */
struct ipa_perf_profile {
	enum ipa_client_type client;
	u32 max_supported_bw_mbps;
};

/**
 * struct  ipa_uc_ready_params - uC ready CB parameters
 * @is_uC_ready: uC loaded or not
 * @priv : callback cookie
 * @notify:	callback
 * @proto: uC offload protocol type
 */
struct ipa_uc_ready_params {
	bool is_uC_ready;
	void *priv;
	ipa_uc_ready_cb notify;
	enum ipa_uc_offload_proto proto;
};

#if defined CONFIG_IPA || defined CONFIG_IPA3

/**
 * ipa_uc_offload_reg_intf - Client should call this function to
 * init uC offload data path
 *
 * @init:	[in] initialization parameters
 *
 * Note: Should not be called from atomic context and only
 * after checking IPA readiness using ipa_register_ipa_ready_cb()
 *
 * @Return 0 on success, negative on failure
 */
int ipa_uc_offload_reg_intf(
	struct ipa_uc_offload_intf_params *in,
	struct ipa_uc_offload_out_params *out);

/**
 * ipa_uc_offload_cleanup - Client Driver should call this
 * function before unload and after disconnect
 *
 * @Return 0 on success, negative on failure
 */
int ipa_uc_offload_cleanup(u32 clnt_hdl);

/**
 * ipa_uc_offload_conn_pipes - Client should call this
 * function to connect uC pipe for offload data path
 *
 * @in:	[in] input parameters from client
 * @out: [out] output params to client
 *
 * Note: Should not be called from atomic context and only
 * after checking IPA readiness using ipa_register_ipa_ready_cb()
 *
 * @Return 0 on success, negative on failure
 */
int ipa_uc_offload_conn_pipes(struct ipa_uc_offload_conn_in_params *in,
			struct ipa_uc_offload_conn_out_params *out);

/**
 * ipa_uc_offload_disconn_pipes() - Client should call this
 *		function to disconnect uC pipe to disable offload data path
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 *
 * Note: Should not be called from atomic context
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_uc_offload_disconn_pipes(u32 clnt_hdl);

/**
 * ipa_set_perf_profile() - Client should call this function to
 *		set IPA clock Band Width based on data rates
 * @profile: [in] BandWidth profile to use
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_set_perf_profile(struct ipa_perf_profile *profile);


/*
 * To register uC ready callback if uC not ready
 * and also check uC readiness
 * if uC not ready only, register callback
 */
int ipa_uc_offload_reg_rdyCB(struct ipa_uc_ready_params *param);

/*
 * To de-register uC ready callback
 */
void ipa_uc_offload_dereg_rdyCB(enum ipa_uc_offload_proto proto);

#else /* (CONFIG_IPA || CONFIG_IPA3) */

static inline int ipa_uc_offload_reg_intf(
		struct ipa_uc_offload_intf_params *in,
		struct ipa_uc_offload_out_params *out)
{
	return -EPERM;
}

static inline int ipa_uC_offload_cleanup(u32 clnt_hdl)
{
	return -EPERM;
}

static inline int ipa_uc_offload_conn_pipes(
		struct ipa_uc_offload_conn_in_params *in,
		struct ipa_uc_offload_conn_out_params *out)
{
	return -EPERM;
}

static inline int ipa_uc_offload_disconn_pipes(u32 clnt_hdl)
{
	return -EPERM;
}

static inline int ipa_set_perf_profile(struct ipa_perf_profile *profile)
{
	return -EPERM;
}

static inline int ipa_uc_offload_reg_rdyCB(struct ipa_uc_ready_params *param)
{
	return -EPERM;
}

static void ipa_uc_offload_dereg_rdyCB(enum ipa_uc_offload_proto proto)
{
}

#endif /* CONFIG_IPA3 */

#endif /* _IPA_UC_OFFLOAD_H_ */
