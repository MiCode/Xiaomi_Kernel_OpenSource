/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

/**
 * struct ipa_wdi3_hdr_info - Header to install on IPA HW
 *
 * @hdr: header to install on IPA HW
 * @hdr_len: length of header
 * @dst_mac_addr_offset: destination mac address offset
 * @hdr_type: layer two header type
 */
struct ipa_wdi3_hdr_info {
	u8 *hdr;
	u8 hdr_len;
	u8 dst_mac_addr_offset;
	enum ipa_hdr_l2_type hdr_type;
};

/**
 * struct ipa_wdi3_reg_intf_in_params - parameters for uC offload
 *	interface registration
 *
 * @netdev_name: network interface name
 * @hdr_info: header information
 * @is_meta_data_valid: if meta data is valid
 * @meta_data: meta data if any
 * @meta_data_mask: meta data mask
 */
struct ipa_wdi3_reg_intf_in_params {
	const char *netdev_name;
	struct ipa_wdi3_hdr_info hdr_info[IPA_IP_MAX];
	u8 is_meta_data_valid;
	u32 meta_data;
	u32 meta_data_mask;
};

/**
 * struct  ipa_wdi3_setup_info - WDI3 TX/Rx configuration
 * @ipa_ep_cfg: ipa endpoint configuration
 * @client: type of "client"
 * @transfer_ring_base_pa:  physical address of the base of the transfer ring
 * @transfer_ring_size:  size of the transfer ring
 * @transfer_ring_doorbell_pa:  physical address of the doorbell that
	IPA uC will update the tailpointer of the transfer ring
 * @event_ring_base_pa:  physical address of the base of the event ring
 * @event_ring_size:  event ring size
 * @event_ring_doorbell_pa:  physical address of the doorbell that IPA uC
	will update the headpointer of the event ring
 * @num_pkt_buffers:  Number of pkt buffers allocated. The size of the event
	ring and the transfer ring has to be atleast ( num_pkt_buffers + 1)
 * @pkt_offset: packet offset (wdi3 header length)
 * @desc_format_template[IPA_HW_WDI3_MAX_ER_DESC_SIZE]:  Holds a cached
	template of the desc format
 */
struct ipa_wdi3_setup_info {
	struct ipa_ep_cfg ipa_ep_cfg;
	enum ipa_client_type client;
	dma_addr_t  transfer_ring_base_pa;
	u32  transfer_ring_size;
	dma_addr_t  transfer_ring_doorbell_pa;

	dma_addr_t  event_ring_base_pa;
	u32  event_ring_size;
	dma_addr_t  event_ring_doorbell_pa;
	u16  num_pkt_buffers;

	u16 pkt_offset;

	u32  desc_format_template[IPA_HW_WDI3_MAX_ER_DESC_SIZE];
};

/**
 * struct  ipa_wdi3_conn_in_params - information provided by
 *		uC offload client
 * @notify: client callback function
 * @priv: client cookie
 * @tx: parameters to connect TX pipe(from IPA to WLAN)
 * @rx: parameters to connect RX pipe(from WLAN to IPA)
 */
struct ipa_wdi3_conn_in_params {
	ipa_notify_cb notify;
	void *priv;
	struct ipa_wdi3_setup_info tx;
	struct ipa_wdi3_setup_info rx;
};

/**
 * struct  ipa_wdi3_conn_out_params - information provided
 *				to WLAN driver
 * @tx_uc_db_pa: physical address of IPA uC doorbell for TX
 * @tx_uc_db_va: virtual address of IPA uC doorbell for TX
 * @rx_uc_db_pa: physical address of IPA uC doorbell for RX
 */
struct ipa_wdi3_conn_out_params {
	dma_addr_t tx_uc_db_pa;
	void __iomem *tx_uc_db_va;
	dma_addr_t rx_uc_db_pa;
};

/**
 * struct  ipa_wdi3_perf_profile - To set BandWidth profile
 *
 * @client: type of client
 * @max_supported_bw_mbps: maximum bandwidth needed (in Mbps)
 */
struct ipa_wdi3_perf_profile {
	enum ipa_client_type client;
	u32 max_supported_bw_mbps;
};

#if defined CONFIG_IPA || defined CONFIG_IPA3

/**
 * ipa_wdi3_reg_intf - Client should call this function to
 * init WDI3 IPA offload data path
 *
 * Note: Should not be called from atomic context and only
 * after checking IPA readiness using ipa_register_ipa_ready_cb()
 *
 * @Return 0 on success, negative on failure
 */
int ipa_wdi3_reg_intf(
	struct ipa_wdi3_reg_intf_in_params *in);

/**
 * ipa_wdi3_dereg_intf - Client Driver should call this
 * function to deregister before unload and after disconnect
 *
 * @Return 0 on success, negative on failure
 */
int ipa_wdi3_dereg_intf(const char *netdev_name);

/**
 * ipa_wdi3_conn_pipes - Client should call this
 * function to connect pipes
 *
 * @in:	[in] input parameters from client
 * @out: [out] output params to client
 *
 * Note: Should not be called from atomic context and only
 * after checking IPA readiness using ipa_register_ipa_ready_cb()
 *
 * @Return 0 on success, negative on failure
 */
int ipa_wdi3_conn_pipes(struct ipa_wdi3_conn_in_params *in,
			struct ipa_wdi3_conn_out_params *out);

/**
 * ipa_wdi3_disconn_pipes() - Client should call this
 *		function to disconnect pipes
 *
 * Note: Should not be called from atomic context
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_wdi3_disconn_pipes(void);

/**
 * ipa_wdi3_enable_pipes() - Client should call this
 *		function to enable IPA offload data path
 *
 * Note: Should not be called from atomic context
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_wdi3_enable_pipes(void);

/**
 * ipa_wdi3_disable_pipes() - Client should call this
 *		function to disable IPA offload data path
 *
 * Note: Should not be called from atomic context
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_wdi3_disable_pipes(void);

/**
 * ipa_wdi3_set_perf_profile() - Client should call this function to
 *		set IPA clock bandwidth based on data rates
 *
 * @profile: [in] BandWidth profile to use
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_wdi3_set_perf_profile(struct ipa_wdi3_perf_profile *profile);


#else /* (CONFIG_IPA || CONFIG_IPA3) */

static inline int ipa_wdi3_reg_intf(
	struct ipa_wdi3_reg_intf_in_params *in)
{
	return -EPERM;
}

static inline int ipa_wdi3_dereg_intf(const char *netdev_name)
{
	return -EPERM;
}

static inline int ipa_wdi3_conn_pipes(struct ipa_wdi3_conn_in_params *in,
			struct ipa_wdi3_conn_out_params *out)
{
	return -EPERM;
}

static inline int ipa_wdi3_disconn_pipes(void)
{
	return -EPERM;
}

static inline int ipa_wdi3_enable_pipes(void)
{
	return -EPERM;
}

static inline int ipa_wdi3_disable_pipes(void)
{
	return -EPERM;
}

static inline int ipa_wdi3_set_perf_profile(
	struct ipa_wdi3_perf_profile *profile)
{
	return -EPERM;
}

#endif /* CONFIG_IPA3 */

#endif /* _IPA_WDI3_H_ */
