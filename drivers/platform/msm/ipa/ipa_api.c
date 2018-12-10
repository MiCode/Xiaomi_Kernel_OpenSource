/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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

#include <linux/ipa.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/ipa_uc_offload.h>
#include <linux/pci.h>
#include "ipa_api.h"

/*
 * The following for adding code (ie. for EMULATION) not found on x86.
 */
#if defined(CONFIG_IPA_EMULATION)
# include "ipa_v3/ipa_emulation_stubs.h"
#endif

#define DRV_NAME "ipa"

#define IPA_API_DISPATCH_RETURN(api, p...) \
	do { \
		if (!ipa_api_ctrl) { \
			pr_err("%s:%d IPA HW is not supported\n", \
				__func__, __LINE__); \
			ret = -EPERM; \
		} \
		else { \
			if (ipa_api_ctrl->api) { \
				ret = ipa_api_ctrl->api(p); \
			} else { \
				WARN(1, \
					"%s not implemented for IPA ver %d\n", \
						__func__, ipa_api_hw_type); \
				ret = -EPERM; \
			} \
		} \
	} while (0)

#define IPA_API_DISPATCH(api, p...) \
	do { \
		if (!ipa_api_ctrl) \
			pr_err("%s:%d IPA HW is not supported\n", \
				__func__, __LINE__); \
		else { \
			if (ipa_api_ctrl->api) { \
				ipa_api_ctrl->api(p); \
			} else { \
				WARN(1, \
					"%s not implemented for IPA ver %d\n",\
						__func__, ipa_api_hw_type); \
			} \
		} \
	} while (0)

#define IPA_API_DISPATCH_RETURN_PTR(api, p...) \
	do { \
		if (!ipa_api_ctrl) { \
			pr_err("%s:%d IPA HW is not supported\n", \
				__func__, __LINE__); \
			ret = NULL; \
		} \
		else { \
			if (ipa_api_ctrl->api) { \
				ret = ipa_api_ctrl->api(p); \
			} else { \
				WARN(1, "%s not implemented for IPA ver %d\n",\
						__func__, ipa_api_hw_type); \
				ret = NULL; \
			} \
		} \
	} while (0)

#define IPA_API_DISPATCH_RETURN_BOOL(api, p...) \
	do { \
		if (!ipa_api_ctrl) { \
			pr_err("%s:%d IPA HW is not supported\n", \
				__func__, __LINE__); \
			ret = false; \
		} \
		else { \
			if (ipa_api_ctrl->api) { \
				ret = ipa_api_ctrl->api(p); \
			} else { \
				WARN(1, "%s not implemented for IPA ver %d\n",\
						__func__, ipa_api_hw_type); \
				ret = false; \
			} \
		} \
	} while (0)

#if defined(CONFIG_IPA_EMULATION)
static bool running_emulation = true;
#else
static bool running_emulation;
#endif

static enum ipa_hw_type ipa_api_hw_type;
static struct ipa_api_controller *ipa_api_ctrl;

const char *ipa_clients_strings[IPA_CLIENT_MAX] = {
	__stringify(IPA_CLIENT_HSIC1_PROD),
	__stringify(IPA_CLIENT_HSIC1_CONS),
	__stringify(IPA_CLIENT_HSIC2_PROD),
	__stringify(IPA_CLIENT_HSIC2_CONS),
	__stringify(IPA_CLIENT_HSIC3_PROD),
	__stringify(IPA_CLIENT_HSIC3_CONS),
	__stringify(IPA_CLIENT_HSIC4_PROD),
	__stringify(IPA_CLIENT_HSIC4_CONS),
	__stringify(IPA_CLIENT_HSIC5_PROD),
	__stringify(IPA_CLIENT_HSIC5_CONS),
	__stringify(IPA_CLIENT_WLAN1_PROD),
	__stringify(IPA_CLIENT_WLAN1_CONS),
	__stringify(IPA_CLIENT_A5_WLAN_AMPDU_PROD),
	__stringify(IPA_CLIENT_WLAN2_CONS),
	__stringify(RESERVED_PROD_14),
	__stringify(IPA_CLIENT_WLAN3_CONS),
	__stringify(RESERVED_PROD_16),
	__stringify(IPA_CLIENT_WLAN4_CONS),
	__stringify(IPA_CLIENT_USB_PROD),
	__stringify(IPA_CLIENT_USB_CONS),
	__stringify(IPA_CLIENT_USB2_PROD),
	__stringify(IPA_CLIENT_USB2_CONS),
	__stringify(IPA_CLIENT_USB3_PROD),
	__stringify(IPA_CLIENT_USB3_CONS),
	__stringify(IPA_CLIENT_USB4_PROD),
	__stringify(IPA_CLIENT_USB4_CONS),
	__stringify(IPA_CLIENT_UC_USB_PROD),
	__stringify(IPA_CLIENT_USB_DPL_CONS),
	__stringify(IPA_CLIENT_A2_EMBEDDED_PROD),
	__stringify(IPA_CLIENT_A2_EMBEDDED_CONS),
	__stringify(IPA_CLIENT_A2_TETHERED_PROD),
	__stringify(IPA_CLIENT_A2_TETHERED_CONS),
	__stringify(IPA_CLIENT_APPS_LAN_PROD),
	__stringify(IPA_CLIENT_APPS_LAN_CONS),
	__stringify(IPA_CLIENT_APPS_WAN_PROD),
	__stringify(IPA_CLIENT_APPS_WAN_CONS),
	__stringify(IPA_CLIENT_APPS_CMD_PROD),
	__stringify(IPA_CLIENT_A5_LAN_WAN_CONS),
	__stringify(IPA_CLIENT_ODU_PROD),
	__stringify(IPA_CLIENT_ODU_EMB_CONS),
	__stringify(RESERVED_PROD_40),
	__stringify(IPA_CLIENT_ODU_TETH_CONS),
	__stringify(IPA_CLIENT_MHI_PROD),
	__stringify(IPA_CLIENT_MHI_CONS),
	__stringify(IPA_CLIENT_MEMCPY_DMA_SYNC_PROD),
	__stringify(IPA_CLIENT_MEMCPY_DMA_SYNC_CONS),
	__stringify(IPA_CLIENT_MEMCPY_DMA_ASYNC_PROD),
	__stringify(IPA_CLIENT_MEMCPY_DMA_ASYNC_CONS),
	__stringify(IPA_CLIENT_ETHERNET_PROD),
	__stringify(IPA_CLIENT_ETHERNET_CONS),
	__stringify(IPA_CLIENT_Q6_LAN_PROD),
	__stringify(IPA_CLIENT_Q6_LAN_CONS),
	__stringify(IPA_CLIENT_Q6_WAN_PROD),
	__stringify(IPA_CLIENT_Q6_WAN_CONS),
	__stringify(IPA_CLIENT_Q6_CMD_PROD),
	__stringify(IPA_CLIENT_Q6_DUN_CONS),
	__stringify(IPA_CLIENT_Q6_DECOMP_PROD),
	__stringify(IPA_CLIENT_Q6_DECOMP_CONS),
	__stringify(IPA_CLIENT_Q6_DECOMP2_PROD),
	__stringify(IPA_CLIENT_Q6_DECOMP2_CONS),
	__stringify(RESERVED_PROD_60),
	__stringify(IPA_CLIENT_Q6_LTE_WIFI_AGGR_CONS),
	__stringify(IPA_CLIENT_TEST_PROD),
	__stringify(IPA_CLIENT_TEST_CONS),
	__stringify(IPA_CLIENT_TEST1_PROD),
	__stringify(IPA_CLIENT_TEST1_CONS),
	__stringify(IPA_CLIENT_TEST2_PROD),
	__stringify(IPA_CLIENT_TEST2_CONS),
	__stringify(IPA_CLIENT_TEST3_PROD),
	__stringify(IPA_CLIENT_TEST3_CONS),
	__stringify(IPA_CLIENT_TEST4_PROD),
	__stringify(IPA_CLIENT_TEST4_CONS),
	__stringify(RESERVED_PROD_72),
	__stringify(IPA_CLIENT_DUMMY_CONS),
	__stringify(IPA_CLIENT_Q6_DL_NLO_DATA_PROD),
	__stringify(IPA_CLIENT_Q6_UL_NLO_DATA_CONS),
	__stringify(RESERVED_PROD_76),
	__stringify(IPA_CLIENT_Q6_UL_NLO_ACK_CONS),
	__stringify(RESERVED_PROD_78),
	__stringify(IPA_CLIENT_Q6_QBAP_STATUS_CONS),
	__stringify(RESERVED_PROD_80),
	__stringify(IPA_CLIENT_MHI_DPL_CONS),
	__stringify(RESERVED_PROD_82),
	__stringify(IPA_CLIENT_ODL_DPL_CONS),
	__stringify(IPA_CLIENT_Q6_AUDIO_DMA_MHI_PROD),
	__stringify(IPA_CLIENT_Q6_AUDIO_DMA_MHI_CONS),
	__stringify(RESERVED_PROD_86),
	__stringify(IPA_CLIENT_APPS_WAN_COAL_CONS),
};

/**
 * ipa_write_64() - convert 64 bit value to byte array
 * @w: 64 bit integer
 * @dest: byte array
 *
 * Return value: converted value
 */
u8 *ipa_write_64(u64 w, u8 *dest)
{
	if (unlikely(dest == NULL)) {
		pr_err("%s: NULL address\n", __func__);
		return dest;
	}
	*dest++ = (u8)((w) & 0xFF);
	*dest++ = (u8)((w >> 8) & 0xFF);
	*dest++ = (u8)((w >> 16) & 0xFF);
	*dest++ = (u8)((w >> 24) & 0xFF);
	*dest++ = (u8)((w >> 32) & 0xFF);
	*dest++ = (u8)((w >> 40) & 0xFF);
	*dest++ = (u8)((w >> 48) & 0xFF);
	*dest++ = (u8)((w >> 56) & 0xFF);

	return dest;
}

/**
 * ipa_write_32() - convert 32 bit value to byte array
 * @w: 32 bit integer
 * @dest: byte array
 *
 * Return value: converted value
 */
u8 *ipa_write_32(u32 w, u8 *dest)
{
	if (unlikely(dest == NULL)) {
		pr_err("%s: NULL address\n", __func__);
		return dest;
	}
	*dest++ = (u8)((w) & 0xFF);
	*dest++ = (u8)((w >> 8) & 0xFF);
	*dest++ = (u8)((w >> 16) & 0xFF);
	*dest++ = (u8)((w >> 24) & 0xFF);

	return dest;
}

/**
 * ipa_write_16() - convert 16 bit value to byte array
 * @hw: 16 bit integer
 * @dest: byte array
 *
 * Return value: converted value
 */
u8 *ipa_write_16(u16 hw, u8 *dest)
{
	if (unlikely(dest == NULL)) {
		pr_err("%s: NULL address\n", __func__);
		return dest;
	}
	*dest++ = (u8)((hw) & 0xFF);
	*dest++ = (u8)((hw >> 8) & 0xFF);

	return dest;
}

/**
 * ipa_write_8() - convert 8 bit value to byte array
 * @hw: 8 bit integer
 * @dest: byte array
 *
 * Return value: converted value
 */
u8 *ipa_write_8(u8 b, u8 *dest)
{
	if (unlikely(dest == NULL)) {
		WARN(1, "%s: NULL address\n", __func__);
		return dest;
	}
	*dest++ = (b) & 0xFF;

	return dest;
}

/**
 * ipa_pad_to_64() - pad byte array to 64 bit value
 * @dest: byte array
 *
 * Return value: padded value
 */
u8 *ipa_pad_to_64(u8 *dest)
{
	int i;
	int j;

	if (unlikely(dest == NULL)) {
		WARN(1, "%s: NULL address\n", __func__);
		return dest;
	}

	i = (long)dest & 0x7;

	if (i)
		for (j = 0; j < (8 - i); j++)
			*dest++ = 0;

	return dest;
}

/**
 * ipa_pad_to_32() - pad byte array to 32 bit value
 * @dest: byte array
 *
 * Return value: padded value
 */
u8 *ipa_pad_to_32(u8 *dest)
{
	int i;
	int j;

	if (unlikely(dest == NULL)) {
		WARN(1, "%s: NULL address\n", __func__);
		return dest;
	}

	i = (long)dest & 0x7;

	if (i)
		for (j = 0; j < (4 - i); j++)
			*dest++ = 0;

	return dest;
}

int ipa_smmu_store_sgt(struct sg_table **out_ch_ptr,
	struct sg_table *in_sgt_ptr)
{
	unsigned int nents;

	if (in_sgt_ptr != NULL) {
		*out_ch_ptr = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
		if (*out_ch_ptr == NULL)
			return -ENOMEM;

		nents = in_sgt_ptr->nents;

		(*out_ch_ptr)->sgl =
			kcalloc(nents, sizeof(struct scatterlist),
				GFP_KERNEL);
		if ((*out_ch_ptr)->sgl == NULL) {
			kfree(*out_ch_ptr);
			*out_ch_ptr = NULL;
			return -ENOMEM;
		}

		memcpy((*out_ch_ptr)->sgl, in_sgt_ptr->sgl,
			nents*sizeof((*out_ch_ptr)->sgl));
		(*out_ch_ptr)->nents = nents;
		(*out_ch_ptr)->orig_nents = in_sgt_ptr->orig_nents;
	}
	return 0;
}

int ipa_smmu_free_sgt(struct sg_table **out_sgt_ptr)
{
	if (*out_sgt_ptr != NULL) {
		kfree((*out_sgt_ptr)->sgl);
		(*out_sgt_ptr)->sgl = NULL;
		kfree(*out_sgt_ptr);
		*out_sgt_ptr = NULL;
	}
	return 0;
}

/**
 * ipa_clear_endpoint_delay() - Clear ep_delay.
 * @clnt_hdl:	[in] IPA client handle
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:		Should not be called from atomic context
 */
int ipa_clear_endpoint_delay(u32 clnt_hdl)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_clear_endpoint_delay, clnt_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_clear_endpoint_delay);

/**
 * ipa_reset_endpoint() - reset an endpoint from BAM perspective
 * @clnt_hdl:	[in] IPA client handle
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:		Should not be called from atomic context
 */
int ipa_reset_endpoint(u32 clnt_hdl)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_reset_endpoint, clnt_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_reset_endpoint);

/**
 * ipa_disable_endpoint() - Disable an endpoint from IPA perspective
 * @clnt_hdl:	[in] IPA client handle
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:		Should not be called from atomic context
 */
int ipa_disable_endpoint(u32 clnt_hdl)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_disable_endpoint, clnt_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_disable_endpoint);


/**
 * ipa_cfg_ep - IPA end-point configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * This includes nat, header, mode, aggregation and route settings and is a one
 * shot API to configure the IPA end-point fully
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_cfg_ep(u32 clnt_hdl, const struct ipa_ep_cfg *ipa_ep_cfg)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_cfg_ep, clnt_hdl, ipa_ep_cfg);

	return ret;
}
EXPORT_SYMBOL(ipa_cfg_ep);

/**
 * ipa_cfg_ep_nat() - IPA end-point NAT configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ep_nat:	[in] IPA NAT end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_cfg_ep_nat(u32 clnt_hdl, const struct ipa_ep_cfg_nat *ep_nat)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_cfg_ep_nat, clnt_hdl, ep_nat);

	return ret;
}
EXPORT_SYMBOL(ipa_cfg_ep_nat);

/**
 * ipa_cfg_ep_conn_track() - IPA end-point IPv6CT configuration
 * @clnt_hdl:		[in] opaque client handle assigned by IPA to client
 * @ep_conn_track:	[in] IPA IPv6CT end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_cfg_ep_conn_track(u32 clnt_hdl,
	const struct ipa_ep_cfg_conn_track *ep_conn_track)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_cfg_ep_conn_track, clnt_hdl,
		ep_conn_track);

	return ret;
}
EXPORT_SYMBOL(ipa_cfg_ep_conn_track);

/**
 * ipa_cfg_ep_hdr() -  IPA end-point header configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_cfg_ep_hdr(u32 clnt_hdl, const struct ipa_ep_cfg_hdr *ep_hdr)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_cfg_ep_hdr, clnt_hdl, ep_hdr);

	return ret;
}
EXPORT_SYMBOL(ipa_cfg_ep_hdr);

/**
 * ipa_cfg_ep_hdr_ext() -  IPA end-point extended header configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ep_hdr_ext:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_cfg_ep_hdr_ext(u32 clnt_hdl,
		       const struct ipa_ep_cfg_hdr_ext *ep_hdr_ext)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_cfg_ep_hdr_ext, clnt_hdl, ep_hdr_ext);

	return ret;
}
EXPORT_SYMBOL(ipa_cfg_ep_hdr_ext);

/**
 * ipa_cfg_ep_mode() - IPA end-point mode configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_cfg_ep_mode(u32 clnt_hdl, const struct ipa_ep_cfg_mode *ep_mode)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_cfg_ep_mode, clnt_hdl, ep_mode);

	return ret;
}
EXPORT_SYMBOL(ipa_cfg_ep_mode);

/**
 * ipa_cfg_ep_aggr() - IPA end-point aggregation configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_cfg_ep_aggr(u32 clnt_hdl, const struct ipa_ep_cfg_aggr *ep_aggr)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_cfg_ep_aggr, clnt_hdl, ep_aggr);

	return ret;
}
EXPORT_SYMBOL(ipa_cfg_ep_aggr);

/**
 * ipa_cfg_ep_deaggr() -  IPA end-point deaggregation configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ep_deaggr:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_cfg_ep_deaggr(u32 clnt_hdl,
			const struct ipa_ep_cfg_deaggr *ep_deaggr)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_cfg_ep_deaggr, clnt_hdl, ep_deaggr);

	return ret;
}
EXPORT_SYMBOL(ipa_cfg_ep_deaggr);

/**
 * ipa_cfg_ep_route() - IPA end-point routing configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_cfg_ep_route(u32 clnt_hdl, const struct ipa_ep_cfg_route *ep_route)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_cfg_ep_route, clnt_hdl, ep_route);

	return ret;
}
EXPORT_SYMBOL(ipa_cfg_ep_route);

/**
 * ipa_cfg_ep_holb() - IPA end-point holb configuration
 *
 * If an IPA producer pipe is full, IPA HW by default will block
 * indefinitely till space opens up. During this time no packets
 * including those from unrelated pipes will be processed. Enabling
 * HOLB means IPA HW will be allowed to drop packets as/when needed
 * and indefinite blocking is avoided.
 *
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_cfg_ep_holb(u32 clnt_hdl, const struct ipa_ep_cfg_holb *ep_holb)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_cfg_ep_holb, clnt_hdl, ep_holb);

	return ret;
}
EXPORT_SYMBOL(ipa_cfg_ep_holb);


/**
 * ipa_cfg_ep_cfg() - IPA end-point cfg configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_cfg_ep_cfg(u32 clnt_hdl, const struct ipa_ep_cfg_cfg *cfg)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_cfg_ep_cfg, clnt_hdl, cfg);

	return ret;
}
EXPORT_SYMBOL(ipa_cfg_ep_cfg);

/**
 * ipa_cfg_ep_metadata_mask() - IPA end-point meta-data mask configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_cfg_ep_metadata_mask(u32 clnt_hdl, const struct ipa_ep_cfg_metadata_mask
		*metadata_mask)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_cfg_ep_metadata_mask, clnt_hdl,
			metadata_mask);

	return ret;
}
EXPORT_SYMBOL(ipa_cfg_ep_metadata_mask);

/**
 * ipa_cfg_ep_holb_by_client() - IPA end-point holb configuration
 *
 * Wrapper function for ipa_cfg_ep_holb() with client name instead of
 * client handle. This function is used for clients that does not have
 * client handle.
 *
 * @client:	[in] client name
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_cfg_ep_holb_by_client(enum ipa_client_type client,
				const struct ipa_ep_cfg_holb *ep_holb)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_cfg_ep_holb_by_client, client, ep_holb);

	return ret;
}
EXPORT_SYMBOL(ipa_cfg_ep_holb_by_client);

/**
 * ipa_cfg_ep_ctrl() -  IPA end-point Control configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg_ctrl:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_cfg_ep_ctrl(u32 clnt_hdl, const struct ipa_ep_cfg_ctrl *ep_ctrl)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_cfg_ep_ctrl, clnt_hdl, ep_ctrl);

	return ret;
}
EXPORT_SYMBOL(ipa_cfg_ep_ctrl);

/**
 * ipa_add_hdr() - add the specified headers to SW and optionally commit them to
 * IPA HW
 * @hdrs:	[inout] set of headers to add
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_add_hdr(struct ipa_ioc_add_hdr *hdrs)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_add_hdr, hdrs);

	return ret;
}
EXPORT_SYMBOL(ipa_add_hdr);

/**
 * ipa_add_hdr_usr() - add the specified headers to SW and optionally
 * commit them to IPA HW
 * @hdrs:		[inout] set of headers to add
 * @user_only:	[in] indicate rules installed by userspace
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_add_hdr_usr(struct ipa_ioc_add_hdr *hdrs, bool user_only)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_add_hdr_usr, hdrs, user_only);

	return ret;
}
EXPORT_SYMBOL(ipa_add_hdr_usr);

/**
 * ipa_del_hdr() - Remove the specified headers from SW and optionally
 * commit them to IPA HW
 * @hdls:	[inout] set of headers to delete
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_del_hdr(struct ipa_ioc_del_hdr *hdls)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_del_hdr, hdls);

	return ret;
}
EXPORT_SYMBOL(ipa_del_hdr);

/**
 * ipa_commit_hdr() - commit to IPA HW the current header table in SW
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_commit_hdr(void)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_commit_hdr);

	return ret;
}
EXPORT_SYMBOL(ipa_commit_hdr);

/**
 * ipa_reset_hdr() - reset the current header table in SW (does not commit to
 * HW)
 *
 * @user_only:	[in] indicate delete rules installed by userspace
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_reset_hdr(bool user_only)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_reset_hdr, user_only);

	return ret;
}
EXPORT_SYMBOL(ipa_reset_hdr);

/**
 * ipa_get_hdr() - Lookup the specified header resource
 * @lookup:	[inout] header to lookup and its handle
 *
 * lookup the specified header resource and return handle if it exists
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 *		Caller should call ipa_put_hdr later if this function succeeds
 */
int ipa_get_hdr(struct ipa_ioc_get_hdr *lookup)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_get_hdr, lookup);

	return ret;
}
EXPORT_SYMBOL(ipa_get_hdr);

/**
 * ipa_put_hdr() - Release the specified header handle
 * @hdr_hdl:	[in] the header handle to release
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_put_hdr(u32 hdr_hdl)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_put_hdr, hdr_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_put_hdr);

/**
 * ipa_copy_hdr() - Lookup the specified header resource and return a copy of it
 * @copy:	[inout] header to lookup and its copy
 *
 * lookup the specified header resource and return a copy of it (along with its
 * attributes) if it exists, this would be called for partial headers
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_copy_hdr(struct ipa_ioc_copy_hdr *copy)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_copy_hdr, copy);

	return ret;
}
EXPORT_SYMBOL(ipa_copy_hdr);

/**
 * ipa_add_hdr_proc_ctx() - add the specified headers to SW
 * and optionally commit them to IPA HW
 * @proc_ctxs:	[inout] set of processing context headers to add
 * @user_only:	[in] indicate rules installed by userspace
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_add_hdr_proc_ctx(struct ipa_ioc_add_hdr_proc_ctx *proc_ctxs,
							bool user_only)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_add_hdr_proc_ctx, proc_ctxs, user_only);

	return ret;
}
EXPORT_SYMBOL(ipa_add_hdr_proc_ctx);

/**
 * ipa_del_hdr_proc_ctx() -
 * Remove the specified processing context headers from SW and
 * optionally commit them to IPA HW.
 * @hdls:	[inout] set of processing context headers to delete
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_del_hdr_proc_ctx(struct ipa_ioc_del_hdr_proc_ctx *hdls)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_del_hdr_proc_ctx, hdls);

	return ret;
}
EXPORT_SYMBOL(ipa_del_hdr_proc_ctx);

/**
 * ipa_add_rt_rule() - Add the specified routing rules to SW and optionally
 * commit to IPA HW
 * @rules:	[inout] set of routing rules to add
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_add_rt_rule(struct ipa_ioc_add_rt_rule *rules)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_add_rt_rule, rules);

	return ret;
}
EXPORT_SYMBOL(ipa_add_rt_rule);

/**
 * ipa_add_rt_rule_usr() - Add the specified routing rules to SW and optionally
 * commit to IPA HW
 * @rules:	[inout] set of routing rules to add
 * @user_only:	[in] indicate rules installed by userspace
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_add_rt_rule_usr(struct ipa_ioc_add_rt_rule *rules, bool user_only)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_add_rt_rule_usr, rules, user_only);

	return ret;
}
EXPORT_SYMBOL(ipa_add_rt_rule_usr);

/**
 * ipa_del_rt_rule() - Remove the specified routing rules to SW and optionally
 * commit to IPA HW
 * @hdls:	[inout] set of routing rules to delete
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_del_rt_rule(struct ipa_ioc_del_rt_rule *hdls)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_del_rt_rule, hdls);

	return ret;
}
EXPORT_SYMBOL(ipa_del_rt_rule);

/**
 * ipa_commit_rt_rule() - Commit the current SW routing table of specified type
 * to IPA HW
 * @ip:	The family of routing tables
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_commit_rt(enum ipa_ip_type ip)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_commit_rt, ip);

	return ret;
}
EXPORT_SYMBOL(ipa_commit_rt);

/**
 * ipa_reset_rt() - reset the current SW routing table of specified type
 * (does not commit to HW)
 * @ip:	The family of routing tables
 * @user_only:	[in] indicate delete rules installed by userspace
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_reset_rt(enum ipa_ip_type ip, bool user_only)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_reset_rt, ip, user_only);

	return ret;
}
EXPORT_SYMBOL(ipa_reset_rt);

/**
 * ipa_get_rt_tbl() - lookup the specified routing table and return handle if it
 * exists, if lookup succeeds the routing table ref cnt is increased
 * @lookup:	[inout] routing table to lookup and its handle
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 *	Caller should call ipa_put_rt_tbl later if this function succeeds
 */
int ipa_get_rt_tbl(struct ipa_ioc_get_rt_tbl *lookup)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_get_rt_tbl, lookup);

	return ret;
}
EXPORT_SYMBOL(ipa_get_rt_tbl);

/**
 * ipa_put_rt_tbl() - Release the specified routing table handle
 * @rt_tbl_hdl:	[in] the routing table handle to release
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_put_rt_tbl(u32 rt_tbl_hdl)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_put_rt_tbl, rt_tbl_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_put_rt_tbl);

/**
 * ipa_query_rt_index() - find the routing table index
 *			which name and ip type are given as parameters
 * @in:	[out] the index of the wanted routing table
 *
 * Returns: the routing table which name is given as parameter, or NULL if it
 * doesn't exist
 */
int ipa_query_rt_index(struct ipa_ioc_get_rt_tbl_indx *in)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_query_rt_index, in);

	return ret;
}
EXPORT_SYMBOL(ipa_query_rt_index);

/**
 * ipa_mdfy_rt_rule() - Modify the specified routing rules in SW and optionally
 * commit to IPA HW
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_mdfy_rt_rule(struct ipa_ioc_mdfy_rt_rule *hdls)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_mdfy_rt_rule, hdls);

	return ret;
}
EXPORT_SYMBOL(ipa_mdfy_rt_rule);

/**
 * ipa_add_flt_rule() - Add the specified filtering rules to SW and optionally
 * commit to IPA HW
 * @rules:	[inout] set of filtering rules to add
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_add_flt_rule(struct ipa_ioc_add_flt_rule *rules)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_add_flt_rule, rules);

	return ret;
}
EXPORT_SYMBOL(ipa_add_flt_rule);

/**
 * ipa_add_flt_rule_usr() - Add the specified filtering rules to
 * SW and optionally commit to IPA HW
 * @rules:		[inout] set of filtering rules to add
 * @user_only:	[in] indicate rules installed by userspace
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_add_flt_rule_usr(struct ipa_ioc_add_flt_rule *rules, bool user_only)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_add_flt_rule_usr, rules, user_only);

	return ret;
}
EXPORT_SYMBOL(ipa_add_flt_rule_usr);

/**
 * ipa_del_flt_rule() - Remove the specified filtering rules from SW and
 * optionally commit to IPA HW
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_del_flt_rule(struct ipa_ioc_del_flt_rule *hdls)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_del_flt_rule, hdls);

	return ret;
}
EXPORT_SYMBOL(ipa_del_flt_rule);

/**
 * ipa_mdfy_flt_rule() - Modify the specified filtering rules in SW and
 * optionally commit to IPA HW
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_mdfy_flt_rule(struct ipa_ioc_mdfy_flt_rule *hdls)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_mdfy_flt_rule, hdls);

	return ret;
}
EXPORT_SYMBOL(ipa_mdfy_flt_rule);

/**
 * ipa_commit_flt() - Commit the current SW filtering table of specified type to
 * IPA HW
 * @ip:	[in] the family of routing tables
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_commit_flt(enum ipa_ip_type ip)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_commit_flt, ip);

	return ret;
}
EXPORT_SYMBOL(ipa_commit_flt);

/**
 * ipa_reset_flt() - Reset the current SW filtering table of specified type
 * (does not commit to HW)
 * @ip:			[in] the family of routing tables
 * @user_only:	[in] indicate delete rules installed by userspace
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_reset_flt(enum ipa_ip_type ip, bool user_only)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_reset_flt, ip, user_only);

	return ret;
}
EXPORT_SYMBOL(ipa_reset_flt);

/**
 * ipa_allocate_nat_device() - Allocates memory for the NAT device
 * @mem:	[in/out] memory parameters
 *
 * Called by NAT client driver to allocate memory for the NAT entries. Based on
 * the request size either shared or system memory will be used.
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_allocate_nat_device(struct ipa_ioc_nat_alloc_mem *mem)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_allocate_nat_device, mem);

	return ret;
}
EXPORT_SYMBOL(ipa_allocate_nat_device);

/**
 * ipa_allocate_nat_table() - Allocates memory for the NAT table
 * @table_alloc: [in/out] memory parameters
 *
 * Called by NAT client to allocate memory for the table entries.
 * Based on the request size either shared or system memory will be used.
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_allocate_nat_table(struct ipa_ioc_nat_ipv6ct_table_alloc *table_alloc)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_allocate_nat_table, table_alloc);

	return ret;
}
EXPORT_SYMBOL(ipa_allocate_nat_table);


/**
 * ipa_allocate_ipv6ct_table() - Allocates memory for the IPv6CT table
 * @table_alloc: [in/out] memory parameters
 *
 * Called by IPv6CT client to allocate memory for the table entries.
 * Based on the request size either shared or system memory will be used.
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_allocate_ipv6ct_table(
	struct ipa_ioc_nat_ipv6ct_table_alloc *table_alloc)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_allocate_ipv6ct_table, table_alloc);

	return ret;
}
EXPORT_SYMBOL(ipa_allocate_ipv6ct_table);

/**
 * ipa_nat_init_cmd() - Post IP_V4_NAT_INIT command to IPA HW
 * @init:	[in] initialization command attributes
 *
 * Called by NAT client driver to post IP_V4_NAT_INIT command to IPA HW
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_nat_init_cmd(struct ipa_ioc_v4_nat_init *init)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_nat_init_cmd, init);

	return ret;
}
EXPORT_SYMBOL(ipa_nat_init_cmd);

/**
 * ipa_ipv6ct_init_cmd() - Post IP_V6_CONN_TRACK_INIT command to IPA HW
 * @init:	[in] initialization command attributes
 *
 * Called by IPv6CT client driver to post IP_V6_CONN_TRACK_INIT command
 * to IPA HW.
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_ipv6ct_init_cmd(struct ipa_ioc_ipv6ct_init *init)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_ipv6ct_init_cmd, init);

	return ret;
}
EXPORT_SYMBOL(ipa_ipv6ct_init_cmd);

/**
 * ipa_nat_dma_cmd() - Post NAT_DMA command to IPA HW
 * @dma:	[in] initialization command attributes
 *
 * Called by NAT client driver to post NAT_DMA command to IPA HW
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_nat_dma_cmd(struct ipa_ioc_nat_dma_cmd *dma)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_nat_dma_cmd, dma);

	return ret;
}
EXPORT_SYMBOL(ipa_nat_dma_cmd);

/**
 * ipa_table_dma_cmd() - Post TABLE_DMA command to IPA HW
 * @dma:	[in] initialization command attributes
 *
 * Called by NAT/IPv6CT client to post TABLE_DMA command to IPA HW
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_table_dma_cmd(struct ipa_ioc_nat_dma_cmd *dma)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_table_dma_cmd, dma);

	return ret;
}
EXPORT_SYMBOL(ipa_table_dma_cmd);

/**
 * ipa_nat_del_cmd() - Delete the NAT table
 * @del:	[in] delete NAT table parameters
 *
 * Called by NAT client driver to delete the nat table
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_nat_del_cmd(struct ipa_ioc_v4_nat_del *del)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_nat_del_cmd, del);

	return ret;
}
EXPORT_SYMBOL(ipa_nat_del_cmd);

/**
 * ipa_del_nat_table() - Delete the NAT table
 * @del:	[in] delete table parameters
 *
 * Called by NAT client to delete the table
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_del_nat_table(struct ipa_ioc_nat_ipv6ct_table_del *del)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_del_nat_table, del);

	return ret;
}
EXPORT_SYMBOL(ipa_del_nat_table);

/**
 * ipa_del_ipv6ct_table() - Delete the IPv6CT table
 * @del:	[in] delete table parameters
 *
 * Called by IPv6CT client to delete the table
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_del_ipv6ct_table(struct ipa_ioc_nat_ipv6ct_table_del *del)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_del_ipv6ct_table, del);

	return ret;
}
EXPORT_SYMBOL(ipa_del_ipv6ct_table);

/**
 * ipa3_nat_mdfy_pdn() - Modify a PDN entry in PDN config table in IPA SRAM
 * @mdfy_pdn:	[in] PDN info to be written to SRAM
 *
 * Called by NAT client driver to modify an entry in the PDN config table
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_nat_mdfy_pdn(struct ipa_ioc_nat_pdn_entry *mdfy_pdn)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_nat_mdfy_pdn, mdfy_pdn);

	return ret;
}
EXPORT_SYMBOL(ipa_nat_mdfy_pdn);

/**
 * ipa_send_msg() - Send "message" from kernel client to IPA driver
 * @meta: [in] message meta-data
 * @buff: [in] the payload for message
 * @callback: [in] free callback
 *
 * Client supplies the message meta-data and payload which IPA driver buffers
 * till read by user-space. After read from user space IPA driver invokes the
 * callback supplied to free the message payload. Client must not touch/free
 * the message payload after calling this API.
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_send_msg(struct ipa_msg_meta *meta, void *buff,
		  ipa_msg_free_fn callback)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_send_msg, meta, buff, callback);

	return ret;
}
EXPORT_SYMBOL(ipa_send_msg);

/**
 * ipa_register_pull_msg() - register pull message type
 * @meta: [in] message meta-data
 * @callback: [in] pull callback
 *
 * Register message callback by kernel client with IPA driver for IPA driver to
 * pull message on-demand.
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_register_pull_msg(struct ipa_msg_meta *meta, ipa_msg_pull_fn callback)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_register_pull_msg, meta, callback);

	return ret;
}
EXPORT_SYMBOL(ipa_register_pull_msg);

/**
 * ipa_deregister_pull_msg() - De-register pull message type
 * @meta: [in] message meta-data
 *
 * De-register "message" by kernel client from IPA driver
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_deregister_pull_msg(struct ipa_msg_meta *meta)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_deregister_pull_msg, meta);

	return ret;
}
EXPORT_SYMBOL(ipa_deregister_pull_msg);

/**
 * ipa_register_intf() - register "logical" interface
 * @name: [in] interface name
 * @tx:	[in] TX properties of the interface
 * @rx:	[in] RX properties of the interface
 *
 * Register an interface and its tx and rx properties, this allows
 * configuration of rules from user-space
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_register_intf(const char *name, const struct ipa_tx_intf *tx,
		       const struct ipa_rx_intf *rx)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_register_intf, name, tx, rx);

	return ret;
}
EXPORT_SYMBOL(ipa_register_intf);

/**
 * ipa_register_intf_ext() - register "logical" interface which has only
 * extended properties
 * @name: [in] interface name
 * @tx:	[in] TX properties of the interface
 * @rx:	[in] RX properties of the interface
 * @ext: [in] EXT properties of the interface
 *
 * Register an interface and its tx, rx and ext properties, this allows
 * configuration of rules from user-space
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_register_intf_ext(const char *name, const struct ipa_tx_intf *tx,
	const struct ipa_rx_intf *rx,
	const struct ipa_ext_intf *ext)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_register_intf_ext, name, tx, rx, ext);

	return ret;
}
EXPORT_SYMBOL(ipa_register_intf_ext);

/**
 * ipa_deregister_intf() - de-register previously registered logical interface
 * @name: [in] interface name
 *
 * De-register a previously registered interface
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_deregister_intf(const char *name)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_deregister_intf, name);

	return ret;
}
EXPORT_SYMBOL(ipa_deregister_intf);

/**
 * ipa_set_aggr_mode() - Set the aggregation mode which is a global setting
 * @mode:	[in] the desired aggregation mode for e.g. straight MBIM, QCNCM,
 * etc
 *
 * Returns:	0 on success
 */
int ipa_set_aggr_mode(enum ipa_aggr_mode mode)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_set_aggr_mode, mode);

	return ret;
}
EXPORT_SYMBOL(ipa_set_aggr_mode);


/**
 * ipa_set_qcncm_ndp_sig() - Set the NDP signature used for QCNCM aggregation
 * mode
 * @sig:	[in] the first 3 bytes of QCNCM NDP signature (expected to be
 * "QND")
 *
 * Set the NDP signature used for QCNCM aggregation mode. The fourth byte
 * (expected to be 'P') needs to be set using the header addition mechanism
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_set_qcncm_ndp_sig(char sig[3])
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_set_qcncm_ndp_sig, sig);

	return ret;
}
EXPORT_SYMBOL(ipa_set_qcncm_ndp_sig);

/**
 * ipa_set_single_ndp_per_mbim() - Enable/disable single NDP per MBIM frame
 * configuration
 * @enable:	[in] true for single NDP/MBIM; false otherwise
 *
 * Returns:	0 on success
 */
int ipa_set_single_ndp_per_mbim(bool enable)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_set_single_ndp_per_mbim, enable);

	return ret;
}
EXPORT_SYMBOL(ipa_set_single_ndp_per_mbim);

/**
 * ipa_tx_dp() - Data-path tx handler
 * @dst:	[in] which IPA destination to route tx packets to
 * @skb:	[in] the packet to send
 * @metadata:	[in] TX packet meta-data
 *
 * Data-path tx handler, this is used for both SW data-path which by-passes most
 * IPA HW blocks AND the regular HW data-path for WLAN AMPDU traffic only. If
 * dst is a "valid" CONS type, then SW data-path is used. If dst is the
 * WLAN_AMPDU PROD type, then HW data-path for WLAN AMPDU is used. Anything else
 * is an error. For errors, client needs to free the skb as needed. For success,
 * IPA driver will later invoke client callback if one was supplied. That
 * callback should free the skb. If no callback supplied, IPA driver will free
 * the skb internally
 *
 * The function will use two descriptors for this send command
 * (for A5_WLAN_AMPDU_PROD only one desciprtor will be sent),
 * the first descriptor will be used to inform the IPA hardware that
 * apps need to push data into the IPA (IP_PACKET_INIT immediate command).
 * Once this send was done from SPS point-of-view the IPA driver will
 * get notified by the supplied callback - ipa_sps_irq_tx_comp()
 *
 * ipa_sps_irq_tx_comp will call to the user supplied
 * callback (from ipa_connect)
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_tx_dp(enum ipa_client_type dst, struct sk_buff *skb,
		struct ipa_tx_meta *meta)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_tx_dp, dst, skb, meta);

	return ret;
}
EXPORT_SYMBOL(ipa_tx_dp);

/**
 * ipa_tx_dp_mul() - Data-path tx handler for multiple packets
 * @src: [in] - Client that is sending data
 * @ipa_tx_data_desc:	[in] data descriptors from wlan
 *
 * this is used for to transfer data descriptors that received
 * from WLAN1_PROD pipe to IPA HW
 *
 * The function will send data descriptors from WLAN1_PROD (one
 * at a time) using sps_transfer_one. Will set EOT flag for last
 * descriptor Once this send was done from SPS point-of-view the
 * IPA driver will get notified by the supplied callback -
 * ipa_sps_irq_tx_no_aggr_notify()
 *
 * ipa_sps_irq_tx_no_aggr_notify will call to the user supplied
 * callback (from ipa_connect)
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_tx_dp_mul(enum ipa_client_type src,
			struct ipa_tx_data_desc *data_desc)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_tx_dp_mul, src, data_desc);

	return ret;
}
EXPORT_SYMBOL(ipa_tx_dp_mul);

void ipa_free_skb(struct ipa_rx_data *data)
{
	IPA_API_DISPATCH(ipa_free_skb, data);
}
EXPORT_SYMBOL(ipa_free_skb);

/**
 * ipa_setup_sys_pipe() - Setup an IPA end-point in system-BAM mode and perform
 * IPA EP configuration
 * @sys_in:	[in] input needed to setup BAM pipe and configure EP
 * @clnt_hdl:	[out] client handle
 *
 *  - configure the end-point registers with the supplied
 *    parameters from the user.
 *  - call SPS APIs to create a system-to-bam connection with IPA.
 *  - allocate descriptor FIFO
 *  - register callback function(ipa_sps_irq_rx_notify or
 *    ipa_sps_irq_tx_notify - depends on client type) in case the driver is
 *    not configured to pulling mode
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_setup_sys_pipe(struct ipa_sys_connect_params *sys_in, u32 *clnt_hdl)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_setup_sys_pipe, sys_in, clnt_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_setup_sys_pipe);

/**
 * ipa_teardown_sys_pipe() - Teardown the system-BAM pipe and cleanup IPA EP
 * @clnt_hdl:	[in] the handle obtained from ipa_setup_sys_pipe
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_teardown_sys_pipe(u32 clnt_hdl)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_teardown_sys_pipe, clnt_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_teardown_sys_pipe);

int ipa_sys_setup(struct ipa_sys_connect_params *sys_in,
	unsigned long *ipa_bam_or_gsi_hdl,
	u32 *ipa_pipe_num, u32 *clnt_hdl, bool en_status)

{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_sys_setup, sys_in, ipa_bam_or_gsi_hdl,
			ipa_pipe_num, clnt_hdl, en_status);

	return ret;
}
EXPORT_SYMBOL(ipa_sys_setup);

int ipa_sys_teardown(u32 clnt_hdl)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_sys_teardown, clnt_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_sys_teardown);

int ipa_sys_update_gsi_hdls(u32 clnt_hdl, unsigned long gsi_ch_hdl,
	unsigned long gsi_ev_hdl)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_sys_update_gsi_hdls, clnt_hdl,
		gsi_ch_hdl, gsi_ev_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_sys_update_gsi_hdls);

/**
 * ipa_connect_wdi_pipe() - WDI client connect
 * @in:	[in] input parameters from client
 * @out: [out] output params to client
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_connect_wdi_pipe(struct ipa_wdi_in_params *in,
		struct ipa_wdi_out_params *out)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_connect_wdi_pipe, in, out);

	return ret;
}
EXPORT_SYMBOL(ipa_connect_wdi_pipe);

/**
 * ipa_disconnect_wdi_pipe() - WDI client disconnect
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_disconnect_wdi_pipe(u32 clnt_hdl)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_disconnect_wdi_pipe, clnt_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_disconnect_wdi_pipe);

/**
 * ipa_enable_wdi_pipe() - WDI client enable
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_enable_wdi_pipe(u32 clnt_hdl)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_enable_wdi_pipe, clnt_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_enable_wdi_pipe);

/**
 * ipa_disable_wdi_pipe() - WDI client disable
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_disable_wdi_pipe(u32 clnt_hdl)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_disable_wdi_pipe, clnt_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_disable_wdi_pipe);

/**
 * ipa_resume_wdi_pipe() - WDI client resume
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_resume_wdi_pipe(u32 clnt_hdl)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_resume_wdi_pipe, clnt_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_resume_wdi_pipe);

/**
 * ipa_suspend_wdi_pipe() - WDI client suspend
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_suspend_wdi_pipe(u32 clnt_hdl)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_suspend_wdi_pipe, clnt_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_suspend_wdi_pipe);

/**
 * ipa_get_wdi_stats() - Query WDI statistics from uc
 * @stats:	[inout] stats blob from client populated by driver
 *
 * Returns:	0 on success, negative on failure
 *
 * @note Cannot be called from atomic context
 *
 */
int ipa_get_wdi_stats(struct IpaHwStatsWDIInfoData_t *stats)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_get_wdi_stats, stats);

	return ret;
}
EXPORT_SYMBOL(ipa_get_wdi_stats);

/**
 * ipa_get_smem_restr_bytes()- Return IPA smem restricted bytes
 *
 * Return value: u16 - number of IPA smem restricted bytes
 */
u16 ipa_get_smem_restr_bytes(void)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_get_smem_restr_bytes);

	return ret;
}
EXPORT_SYMBOL(ipa_get_smem_restr_bytes);

/**
 * ipa_broadcast_wdi_quota_reach_ind() - quota reach
 * @uint32_t fid: [in] input netdev ID
 * @uint64_t num_bytes: [in] used bytes
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_broadcast_wdi_quota_reach_ind(uint32_t fid,
		uint64_t num_bytes)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_broadcast_wdi_quota_reach_ind,
		fid, num_bytes);

	return ret;
}
EXPORT_SYMBOL(ipa_broadcast_wdi_quota_reach_ind);

/**
 * ipa_uc_wdi_get_dbpa() - To retrieve
 * doorbell physical address of wlan pipes
 * @param:  [in/out] input/output parameters
 *          from/to client
 *
 * Returns:	0 on success, negative on failure
 *
 */
int ipa_uc_wdi_get_dbpa(
	struct ipa_wdi_db_params *param)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_uc_wdi_get_dbpa, param);

	return ret;
}
EXPORT_SYMBOL(ipa_uc_wdi_get_dbpa);

/**
 * ipa_uc_reg_rdyCB() - To register uC
 * ready CB if uC not ready
 * @inout:	[in/out] input/output parameters
 * from/to client
 *
 * Returns:	0 on success, negative on failure
 *
 */
int ipa_uc_reg_rdyCB(
	struct ipa_wdi_uc_ready_params *inout)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_uc_reg_rdyCB, inout);

	return ret;
}
EXPORT_SYMBOL(ipa_uc_reg_rdyCB);

/**
 * ipa_uc_dereg_rdyCB() - To de-register uC ready CB
 *
 * Returns:	0 on success, negative on failure
 *
 */
int ipa_uc_dereg_rdyCB(void)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_uc_dereg_rdyCB);

	return ret;
}
EXPORT_SYMBOL(ipa_uc_dereg_rdyCB);

/**
 * teth_bridge_init() - Initialize the Tethering bridge driver
 * @params - in/out params for USB initialization API (please look at struct
 *  definition for more info)
 *
 * USB driver gets a pointer to a callback function (usb_notify_cb) and an
 * associated data. USB driver installs this callback function in the call to
 * ipa_connect().
 *
 * Builds IPA resource manager dependency graph.
 *
 * Return codes: 0: success,
 *		-EINVAL - Bad parameter
 *		Other negative value - Failure
 */
int teth_bridge_init(struct teth_bridge_init_params *params)
{
	int ret;

	IPA_API_DISPATCH_RETURN(teth_bridge_init, params);

	return ret;
}
EXPORT_SYMBOL(teth_bridge_init);

/**
 * teth_bridge_disconnect() - Disconnect tethering bridge module
 */
int teth_bridge_disconnect(enum ipa_client_type client)
{
	int ret;

	IPA_API_DISPATCH_RETURN(teth_bridge_disconnect, client);

	return ret;
}
EXPORT_SYMBOL(teth_bridge_disconnect);

/**
 * teth_bridge_connect() - Connect bridge for a tethered Rmnet / MBIM call
 * @connect_params:	Connection info
 *
 * Return codes: 0: success
 *		-EINVAL: invalid parameters
 *		-EPERM: Operation not permitted as the bridge is already
 *		connected
 */
int teth_bridge_connect(struct teth_bridge_connect_params *connect_params)
{
	int ret;

	IPA_API_DISPATCH_RETURN(teth_bridge_connect, connect_params);

	return ret;
}
EXPORT_SYMBOL(teth_bridge_connect);

/* ipa_set_client() - provide client mapping
 * @client: client type
 *
 * Return value: none
 */

void ipa_set_client(int index, enum ipacm_client_enum client, bool uplink)
{
	IPA_API_DISPATCH(ipa_set_client, index, client, uplink);
}
EXPORT_SYMBOL(ipa_set_client);

/**
 * ipa_get_client() - provide client mapping
 * @client: client type
 *
 * Return value: none
 */
enum ipacm_client_enum ipa_get_client(int pipe_idx)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_get_client, pipe_idx);

	return ret;
}
EXPORT_SYMBOL(ipa_get_client);

/**
 * ipa_get_client_uplink() - provide client mapping
 * @client: client type
 *
 * Return value: none
 */
bool ipa_get_client_uplink(int pipe_idx)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_get_client_uplink, pipe_idx);

	return ret;
}
EXPORT_SYMBOL(ipa_get_client_uplink);

/**
 * ipa_dma_init() -Initialize IPADMA.
 *
 * This function initialize all IPADMA internal data and connect in dma:
 *	MEMCPY_DMA_SYNC_PROD ->MEMCPY_DMA_SYNC_CONS
 *	MEMCPY_DMA_ASYNC_PROD->MEMCPY_DMA_SYNC_CONS
 *
 * Return codes: 0: success
 *		-EFAULT: IPADMA is already initialized
 *		-ENOMEM: allocating memory error
 *		-EPERM: pipe connection failed
 */
int ipa_dma_init(void)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_dma_init);

	return ret;
}
EXPORT_SYMBOL(ipa_dma_init);

/**
 * ipa_dma_enable() -Vote for IPA clocks.
 *
 *Return codes: 0: success
 *		-EINVAL: IPADMA is not initialized
 *		-EPERM: Operation not permitted as ipa_dma is already
 *		 enabled
 */
int ipa_dma_enable(void)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_dma_enable);

	return ret;
}
EXPORT_SYMBOL(ipa_dma_enable);

/**
 * ipa_dma_disable()- Unvote for IPA clocks.
 *
 * enter to power save mode.
 *
 * Return codes: 0: success
 *		-EINVAL: IPADMA is not initialized
 *		-EPERM: Operation not permitted as ipa_dma is already
 *			diabled
 *		-EFAULT: can not disable ipa_dma as there are pending
 *			memcopy works
 */
int ipa_dma_disable(void)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_dma_disable);

	return ret;
}
EXPORT_SYMBOL(ipa_dma_disable);

/**
 * ipa_dma_sync_memcpy()- Perform synchronous memcpy using IPA.
 *
 * @dest: physical address to store the copied data.
 * @src: physical address of the source data to copy.
 * @len: number of bytes to copy.
 *
 * Return codes: 0: success
 *		-EINVAL: invalid params
 *		-EPERM: operation not permitted as ipa_dma isn't enable or
 *			initialized
 *		-SPS_ERROR: on sps faliures
 *		-EFAULT: other
 */
int ipa_dma_sync_memcpy(u64 dest, u64 src, int len)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_dma_sync_memcpy, dest, src, len);

	return ret;
}
EXPORT_SYMBOL(ipa_dma_sync_memcpy);

/**
 * ipa_dma_async_memcpy()- Perform asynchronous memcpy using IPA.
 *
 * @dest: physical address to store the copied data.
 * @src: physical address of the source data to copy.
 * @len: number of bytes to copy.
 * @user_cb: callback function to notify the client when the copy was done.
 * @user_param: cookie for user_cb.
 *
 * Return codes: 0: success
 *		-EINVAL: invalid params
 *		-EPERM: operation not permitted as ipa_dma isn't enable or
 *			initialized
 *		-SPS_ERROR: on sps faliures
 *		-EFAULT: descr fifo is full.
 */
int ipa_dma_async_memcpy(u64 dest, u64 src, int len,
		void (*user_cb)(void *user1), void *user_param)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_dma_async_memcpy, dest, src, len, user_cb,
		user_param);

	return ret;
}
EXPORT_SYMBOL(ipa_dma_async_memcpy);

/**
 * ipa_dma_uc_memcpy() - Perform a memcpy action using IPA uC
 * @dest: physical address to store the copied data.
 * @src: physical address of the source data to copy.
 * @len: number of bytes to copy.
 *
 * Return codes: 0: success
 *		-EINVAL: invalid params
 *		-EPERM: operation not permitted as ipa_dma isn't enable or
 *			initialized
 *		-EBADF: IPA uC is not loaded
 */
int ipa_dma_uc_memcpy(phys_addr_t dest, phys_addr_t src, int len)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_dma_uc_memcpy, dest, src, len);

	return ret;
}
EXPORT_SYMBOL(ipa_dma_uc_memcpy);

/**
 * ipa_dma_destroy() -teardown IPADMA pipes and release ipadma.
 *
 * this is a blocking function, returns just after destroying IPADMA.
 */
void ipa_dma_destroy(void)
{
	IPA_API_DISPATCH(ipa_dma_destroy);
}
EXPORT_SYMBOL(ipa_dma_destroy);

int ipa_mhi_init_engine(struct ipa_mhi_init_engine *params)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_mhi_init_engine, params);

	return ret;
}
EXPORT_SYMBOL(ipa_mhi_init_engine);

/**
 * ipa_connect_mhi_pipe() - Connect pipe to IPA and start corresponding
 * MHI channel
 * @in: connect parameters
 * @clnt_hdl: [out] client handle for this pipe
 *
 * This function is called by IPA MHI client driver on MHI channel start.
 * This function is called after MHI engine was started.
 * This function is doing the following:
 *	- Send command to uC to start corresponding MHI channel
 *	- Configure IPA EP control
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int ipa_connect_mhi_pipe(struct ipa_mhi_connect_params_internal *in,
		u32 *clnt_hdl)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_connect_mhi_pipe, in, clnt_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_connect_mhi_pipe);

/**
 * ipa_disconnect_mhi_pipe() - Disconnect pipe from IPA and reset corresponding
 * MHI channel
 * @in: connect parameters
 * @clnt_hdl: [out] client handle for this pipe
 *
 * This function is called by IPA MHI client driver on MHI channel reset.
 * This function is called after MHI channel was started.
 * This function is doing the following:
 *	- Send command to uC to reset corresponding MHI channel
 *	- Configure IPA EP control
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int ipa_disconnect_mhi_pipe(u32 clnt_hdl)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_disconnect_mhi_pipe, clnt_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_disconnect_mhi_pipe);

bool ipa_mhi_stop_gsi_channel(enum ipa_client_type client)
{
	bool ret;

	IPA_API_DISPATCH_RETURN_BOOL(ipa_mhi_stop_gsi_channel, client);

	return ret;
}

int ipa_uc_mhi_reset_channel(int channelHandle)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_uc_mhi_reset_channel, channelHandle);

	return ret;
}

bool ipa_mhi_sps_channel_empty(enum ipa_client_type client)
{
	bool ret;

	IPA_API_DISPATCH_RETURN_BOOL(ipa_mhi_sps_channel_empty, client);

	return ret;
}

int ipa_qmi_enable_force_clear_datapath_send(
	struct ipa_enable_force_clear_datapath_req_msg_v01 *req)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_qmi_enable_force_clear_datapath_send, req);

	return ret;
}

int ipa_qmi_disable_force_clear_datapath_send(
	struct ipa_disable_force_clear_datapath_req_msg_v01 *req)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_qmi_disable_force_clear_datapath_send, req);

	return ret;
}

int ipa_generate_tag_process(void)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_generate_tag_process);

	return ret;
}

int ipa_disable_sps_pipe(enum ipa_client_type client)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_disable_sps_pipe, client);

	return ret;
}

int ipa_mhi_reset_channel_internal(enum ipa_client_type client)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_mhi_reset_channel_internal, client);

	return ret;
}

int ipa_mhi_start_channel_internal(enum ipa_client_type client)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_mhi_start_channel_internal, client);

	return ret;
}

void ipa_get_holb(int ep_idx, struct ipa_ep_cfg_holb *holb)
{
	IPA_API_DISPATCH(ipa_get_holb, ep_idx, holb);
}

void ipa_set_tag_process_before_gating(bool val)
{
	IPA_API_DISPATCH(ipa_set_tag_process_before_gating, val);
}

int ipa_mhi_query_ch_info(enum ipa_client_type client,
		struct gsi_chan_info *ch_info)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_mhi_query_ch_info, client, ch_info);

	return ret;
}

int ipa_uc_mhi_suspend_channel(int channelHandle)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_uc_mhi_suspend_channel, channelHandle);

	return ret;
}

int ipa_uc_mhi_stop_event_update_channel(int channelHandle)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_uc_mhi_stop_event_update_channel,
			channelHandle);

	return ret;
}

bool ipa_has_open_aggr_frame(enum ipa_client_type client)
{
	bool ret;

	IPA_API_DISPATCH_RETURN_BOOL(ipa_has_open_aggr_frame, client);

	return ret;
}

int ipa_mhi_resume_channels_internal(enum ipa_client_type client,
		bool LPTransitionRejected, bool brstmode_enabled,
		union __packed gsi_channel_scratch ch_scratch, u8 index)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_mhi_resume_channels_internal, client,
			LPTransitionRejected, brstmode_enabled, ch_scratch,
			index);

	return ret;
}

int ipa_uc_mhi_send_dl_ul_sync_info(union IpaHwMhiDlUlSyncCmdData_t *cmd)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_uc_mhi_send_dl_ul_sync_info,
			cmd);

	return ret;
}

int ipa_mhi_destroy_channel(enum ipa_client_type client)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_mhi_destroy_channel, client);

	return ret;
}

int ipa_uc_mhi_init(void (*ready_cb)(void),
		void (*wakeup_request_cb)(void))
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_uc_mhi_init, ready_cb, wakeup_request_cb);

	return ret;
}

void ipa_uc_mhi_cleanup(void)
{
	IPA_API_DISPATCH(ipa_uc_mhi_cleanup);
}

int ipa_uc_mhi_print_stats(char *dbg_buff, int size)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_uc_mhi_print_stats, dbg_buff, size);

	return ret;
}

/**
 * ipa_uc_state_check() - Check the status of the uC interface
 *
 * Return value: 0 if the uC is loaded, interface is initialized
 *               and there was no recent failure in one of the commands.
 *               A negative value is returned otherwise.
 */
int ipa_uc_state_check(void)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_uc_state_check);

	return ret;
}

int ipa_write_qmap_id(struct ipa_ioc_write_qmapid *param_in)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_write_qmap_id, param_in);

	return ret;
}
EXPORT_SYMBOL(ipa_write_qmap_id);

/**
 * ipa_add_interrupt_handler() - Adds handler to an interrupt type
 * @interrupt:		Interrupt type
 * @handler:		The handler to be added
 * @deferred_flag:	whether the handler processing should be deferred in
 *			a workqueue
 * @private_data:	the client's private data
 *
 * Adds handler to an interrupt type and enable the specific bit
 * in IRQ_EN register, associated interrupt in IRQ_STTS register will be enabled
 */
int ipa_add_interrupt_handler(enum ipa_irq_type interrupt,
	ipa_irq_handler_t handler,
	bool deferred_flag,
	void *private_data)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_add_interrupt_handler, interrupt, handler,
		deferred_flag, private_data);

	return ret;
}
EXPORT_SYMBOL(ipa_add_interrupt_handler);

/**
 * ipa_remove_interrupt_handler() - Removes handler to an interrupt type
 * @interrupt:		Interrupt type
 *
 * Removes the handler and disable the specific bit in IRQ_EN register
 */
int ipa_remove_interrupt_handler(enum ipa_irq_type interrupt)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_remove_interrupt_handler, interrupt);

	return ret;
}
EXPORT_SYMBOL(ipa_remove_interrupt_handler);

/**
 * ipa_restore_suspend_handler() - restores the original suspend IRQ handler
 * as it was registered in the IPA init sequence.
 * Return codes:
 * 0: success
 * -EPERM: failed to remove current handler or failed to add original handler
 */
int ipa_restore_suspend_handler(void)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_restore_suspend_handler);

	return ret;
}
EXPORT_SYMBOL(ipa_restore_suspend_handler);

/**
 * ipa_bam_reg_dump() - Dump selected BAM registers for IPA and DMA-BAM
 *
 * Function is rate limited to avoid flooding kernel log buffer
 */
void ipa_bam_reg_dump(void)
{
	IPA_API_DISPATCH(ipa_bam_reg_dump);
}
EXPORT_SYMBOL(ipa_bam_reg_dump);

/**
 * ipa_get_ep_mapping() - provide endpoint mapping
 * @client: client type
 *
 * Return value: endpoint mapping
 */
int ipa_get_ep_mapping(enum ipa_client_type client)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_get_ep_mapping, client);

	return ret;
}
EXPORT_SYMBOL(ipa_get_ep_mapping);

/**
 * ipa_is_ready() - check if IPA module was initialized
 * successfully
 *
 * Return value: true for yes; false for no
 */
bool ipa_is_ready(void)
{
	if (!ipa_api_ctrl || !ipa_api_ctrl->ipa_is_ready)
		return false;
	return ipa_api_ctrl->ipa_is_ready();
}
EXPORT_SYMBOL(ipa_is_ready);

/**
 * ipa_proxy_clk_vote() - called to add IPA clock proxy vote
 *
 * Return value: none
 */
void ipa_proxy_clk_vote(void)
{
	IPA_API_DISPATCH(ipa_proxy_clk_vote);
}
EXPORT_SYMBOL(ipa_proxy_clk_vote);

/**
 * ipa_proxy_clk_unvote() - called to remove IPA clock proxy vote
 *
 * Return value: none
 */
void ipa_proxy_clk_unvote(void)
{
	IPA_API_DISPATCH(ipa_proxy_clk_unvote);
}
EXPORT_SYMBOL(ipa_proxy_clk_unvote);

/**
 * ipa_get_hw_type() - Return IPA HW version
 *
 * Return value: enum ipa_hw_type
 */
enum ipa_hw_type ipa_get_hw_type(void)
{
	return ipa_api_hw_type;
}
EXPORT_SYMBOL(ipa_get_hw_type);

/**
 * ipa_is_client_handle_valid() - check if IPA client handle is valid handle
 *
 * Return value: true for yes; false for no
 */
bool ipa_is_client_handle_valid(u32 clnt_hdl)
{
	if (!ipa_api_ctrl || !ipa_api_ctrl->ipa_is_client_handle_valid)
		return false;
	return ipa_api_ctrl->ipa_is_client_handle_valid(clnt_hdl);
}
EXPORT_SYMBOL(ipa_is_client_handle_valid);

/**
 * ipa_get_client_mapping() - provide client mapping
 * @pipe_idx: IPA end-point number
 *
 * Return value: client mapping
 */
enum ipa_client_type ipa_get_client_mapping(int pipe_idx)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_get_client_mapping, pipe_idx);

	return ret;
}
EXPORT_SYMBOL(ipa_get_client_mapping);

/**
 * ipa_get_rm_resource_from_ep() - get the IPA_RM resource which is related to
 * the supplied pipe index.
 *
 * @pipe_idx:
 *
 * Return value: IPA_RM resource related to the pipe, -1 if a resource was not
 * found.
 */
enum ipa_rm_resource_name ipa_get_rm_resource_from_ep(int pipe_idx)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_get_rm_resource_from_ep, pipe_idx);

	return ret;
}
EXPORT_SYMBOL(ipa_get_rm_resource_from_ep);

/**
 * ipa_get_modem_cfg_emb_pipe_flt()- Return ipa_ctx->modem_cfg_emb_pipe_flt
 *
 * Return value: true if modem configures embedded pipe flt, false otherwise
 */
bool ipa_get_modem_cfg_emb_pipe_flt(void)
{
	if (!ipa_api_ctrl || !ipa_api_ctrl->ipa_get_modem_cfg_emb_pipe_flt)
		return false;
	return ipa_api_ctrl->ipa_get_modem_cfg_emb_pipe_flt();
}
EXPORT_SYMBOL(ipa_get_modem_cfg_emb_pipe_flt);

/**
 * ipa_get_transport_type()- Return ipa_ctx->transport_prototype
 *
 * Return value: enum ipa_transport_type
 */
enum ipa_transport_type ipa_get_transport_type(void)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_get_transport_type);

	return ret;
}
EXPORT_SYMBOL(ipa_get_transport_type);

/**
 * ipa_get_smmu_domain()- Return the smmu domain
 *
 * Return value: pointer to iommu domain if smmu_cb valid, NULL otherwise
 */
struct iommu_domain *ipa_get_smmu_domain(void)
{
	struct iommu_domain *ret;

	IPA_API_DISPATCH_RETURN_PTR(ipa_get_smmu_domain);

	return ret;
}
EXPORT_SYMBOL(ipa_get_smmu_domain);

/**
 * ipa_disable_apps_wan_cons_deaggr()- set
 * ipa_ctx->ipa_client_apps_wan_cons_agg_gro
 *
 * Return value: 0 or negative in case of failure
 */
int ipa_disable_apps_wan_cons_deaggr(uint32_t agg_size, uint32_t agg_count)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_disable_apps_wan_cons_deaggr, agg_size,
		agg_count);

	return ret;
}
EXPORT_SYMBOL(ipa_disable_apps_wan_cons_deaggr);

/**
 * ipa_get_dma_dev()- Returns ipa_ctx dma dev pointer
 *
 * Return value: pointer to ipa_ctx dma dev pointer
 */
struct device *ipa_get_dma_dev(void)
{
	struct device *ret;

	IPA_API_DISPATCH_RETURN_PTR(ipa_get_dma_dev);

	return ret;
}
EXPORT_SYMBOL(ipa_get_dma_dev);

/**
 * ipa_release_wdi_mapping() - release iommu mapping
 *
 *
 * @num_buffers: number of buffers to be released
 *
 * @info: pointer to wdi buffers info array
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int ipa_release_wdi_mapping(u32 num_buffers, struct ipa_wdi_buffer_info *info)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_release_wdi_mapping, num_buffers, info);

	return ret;
}
EXPORT_SYMBOL(ipa_release_wdi_mapping);

/**
 * ipa_create_wdi_mapping() - Perform iommu mapping
 *
 *
 * @num_buffers: number of buffers to be mapped
 *
 * @info: pointer to wdi buffers info array
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int ipa_create_wdi_mapping(u32 num_buffers, struct ipa_wdi_buffer_info *info)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_create_wdi_mapping, num_buffers, info);

	return ret;
}
EXPORT_SYMBOL(ipa_create_wdi_mapping);

/**
 * ipa_get_gsi_ep_info() - provide gsi ep information
 * @client: IPA client type
 *
 * Return value: pointer to ipa_gsi_ep_info
 */
const struct ipa_gsi_ep_config *ipa_get_gsi_ep_info(enum ipa_client_type client)
{
	if (!ipa_api_ctrl || !ipa_api_ctrl->ipa_get_gsi_ep_info)
		return NULL;
	return ipa_api_ctrl->ipa_get_gsi_ep_info(client);
}
EXPORT_SYMBOL(ipa_get_gsi_ep_info);

/**
 * ipa_stop_gsi_channel()- Stops a GSI channel in IPA
 *
 * Return value: 0 on success, negative otherwise
 */
int ipa_stop_gsi_channel(u32 clnt_hdl)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_stop_gsi_channel, clnt_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_stop_gsi_channel);

/**
 * ipa_start_gsi_channel()- Startsa GSI channel in IPA
 *
 * Return value: 0 on success, negative otherwise
 */
int ipa_start_gsi_channel(u32 clnt_hdl)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_start_gsi_channel, clnt_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_start_gsi_channel);

/**
 * ipa_is_vlan_mode - check if a LAN driver should load in VLAN mode
 * @iface - type of vlan capable device
 * @res - query result: true for vlan mode, false for non vlan mode
 *
 * API must be called after ipa_is_ready() returns true, otherwise it will fail
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_is_vlan_mode(enum ipa_vlan_ifaces iface, bool *res)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_is_vlan_mode, iface, res);

	return ret;

}
EXPORT_SYMBOL(ipa_is_vlan_mode);

/**
 * ipa_get_version_string() - Get string representation of IPA version
 * @ver: IPA version
 *
 * Return: Constant string representation
 */
const char *ipa_get_version_string(enum ipa_hw_type ver)
{
	const char *str;

	switch (ver) {
	case IPA_HW_v1_0:
		str = "1.0";
		break;
	case IPA_HW_v1_1:
		str = "1.1";
		break;
	case IPA_HW_v2_0:
		str = "2.0";
		break;
	case IPA_HW_v2_1:
		str = "2.1";
		break;
	case IPA_HW_v2_5:
		str = "2.5/2.6";
		break;
	case IPA_HW_v2_6L:
		str = "2.6L";
		break;
	case IPA_HW_v3_0:
		str = "3.0";
		break;
	case IPA_HW_v3_1:
		str = "3.1";
		break;
	case IPA_HW_v3_5:
		str = "3.5";
		break;
	case IPA_HW_v3_5_1:
		str = "3.5.1";
		break;
	case IPA_HW_v4_0:
		str = "4.0";
		break;
	case IPA_HW_v4_1:
		str = "4.1";
		break;
	case IPA_HW_v4_2:
		str = "4.2";
		break;
	case IPA_HW_v4_5:
		str = "4.5";
		break;
	default:
		str = "Invalid version";
		break;
	}

	return str;
}
EXPORT_SYMBOL(ipa_get_version_string);

static const struct of_device_id ipa_plat_drv_match[] = {
	{ .compatible = "qcom,ipa", },
	{ .compatible = "qcom,ipa-smmu-ap-cb", },
	{ .compatible = "qcom,ipa-smmu-wlan-cb", },
	{ .compatible = "qcom,ipa-smmu-uc-cb", },
	{ .compatible = "qcom,smp2p-map-ipa-1-in", },
	{ .compatible = "qcom,smp2p-map-ipa-1-out", },
	{}
};

/*********************************************************/
/*                PCIe Version                           */
/*********************************************************/

static const struct of_device_id ipa_pci_drv_match[] = {
	{ .compatible = "qcom,ipa", },
	{}
};

/*
 * Forward declarations of static functions required for PCI
 * registraion
 *
 * VENDOR and DEVICE should be defined in pci_ids.h
 */
static int ipa_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent);
static void ipa_pci_remove(struct pci_dev *pdev);
static void ipa_pci_shutdown(struct pci_dev *pdev);
static pci_ers_result_t ipa_pci_io_error_detected(struct pci_dev *dev,
	pci_channel_state_t state);
static pci_ers_result_t ipa_pci_io_slot_reset(struct pci_dev *dev);
static void ipa_pci_io_resume(struct pci_dev *dev);

#define LOCAL_VENDOR 0x17CB
#define LOCAL_DEVICE 0x00ff

static const char ipa_pci_driver_name[] = "qcipav3";

static const struct pci_device_id ipa_pci_tbl[] = {
	{ PCI_DEVICE(LOCAL_VENDOR, LOCAL_DEVICE) },
	{ 0, 0, 0, 0, 0, 0, 0 }
};

MODULE_DEVICE_TABLE(pci, ipa_pci_tbl);

/* PCI Error Recovery */
static const struct pci_error_handlers ipa_pci_err_handler = {
	.error_detected = ipa_pci_io_error_detected,
	.slot_reset = ipa_pci_io_slot_reset,
	.resume = ipa_pci_io_resume,
};

static struct pci_driver ipa_pci_driver = {
	.name     = ipa_pci_driver_name,
	.id_table = ipa_pci_tbl,
	.probe    = ipa_pci_probe,
	.remove   = ipa_pci_remove,
	.shutdown = ipa_pci_shutdown,
	.err_handler = &ipa_pci_err_handler
};

static int ipa_generic_plat_drv_probe(struct platform_device *pdev_p)
{
	int result;

	/*
	 * IPA probe function can be called for multiple times as the same probe
	 * function handles multiple compatibilities
	 */
	pr_debug("ipa: IPA driver probing started for %s\n",
		pdev_p->dev.of_node->name);

	if (!ipa_api_ctrl) {
		ipa_api_ctrl = kzalloc(sizeof(*ipa_api_ctrl), GFP_KERNEL);
		if (!ipa_api_ctrl)
			return -ENOMEM;

		/* Get IPA HW Version */
		result = of_property_read_u32(pdev_p->dev.of_node,
			"qcom,ipa-hw-ver", &ipa_api_hw_type);
		if ((result) || (ipa_api_hw_type == 0)) {
			pr_err("ipa: get resource failed for ipa-hw-ver!\n");
			kfree(ipa_api_ctrl);
			ipa_api_ctrl = 0;
			return -ENODEV;
		}
		pr_debug("ipa: ipa_api_hw_type = %d", ipa_api_hw_type);
	}

	/* call probe based on IPA HW version */
	switch (ipa_api_hw_type) {
	case IPA_HW_v3_0:
	case IPA_HW_v3_1:
	case IPA_HW_v3_5:
	case IPA_HW_v3_5_1:
	case IPA_HW_v4_0:
	case IPA_HW_v4_1:
	case IPA_HW_v4_2:
	case IPA_HW_v4_5:
		result = ipa3_plat_drv_probe(pdev_p, ipa_api_ctrl,
			ipa_plat_drv_match);
		break;
	default:
		pr_err("ipa: unsupported version %d\n", ipa_api_hw_type);
		return -EPERM;
	}

	if (result && result != -EPROBE_DEFER)
		pr_err("ipa: ipa_plat_drv_probe failed\n");

	return result;
}

static int ipa_ap_suspend(struct device *dev)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_ap_suspend, dev);

	return ret;
}

static int ipa_ap_resume(struct device *dev)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_ap_resume, dev);

	return ret;
}

int ipa_register_ipa_ready_cb(void (*ipa_ready_cb)(void *user_data),
			      void *user_data)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_register_ipa_ready_cb,
				ipa_ready_cb, user_data);

	return ret;
}
EXPORT_SYMBOL(ipa_register_ipa_ready_cb);

/**
 * ipa_inc_client_enable_clks() - Increase active clients counter, and
 * enable ipa clocks if necessary
 *
 * Please do not use this API, use the wrapper macros instead (ipa_i.h)
 * IPA_ACTIVE_CLIENTS_INC_XXX();
 *
 * Return codes:
 * None
 */
void ipa_inc_client_enable_clks(struct ipa_active_client_logging_info *id)
{
	IPA_API_DISPATCH(ipa_inc_client_enable_clks, id);
}
EXPORT_SYMBOL(ipa_inc_client_enable_clks);

/**
 * ipa_dec_client_disable_clks() - Increase active clients counter, and
 * enable ipa clocks if necessary
 *
 * Please do not use this API, use the wrapper macros instead (ipa_i.h)
 * IPA_ACTIVE_CLIENTS_DEC_XXX();
 *
 * Return codes:
 * None
 */
void ipa_dec_client_disable_clks(struct ipa_active_client_logging_info *id)
{
	IPA_API_DISPATCH(ipa_dec_client_disable_clks, id);
}
EXPORT_SYMBOL(ipa_dec_client_disable_clks);

/**
 * ipa_inc_client_enable_clks_no_block() - Only increment the number of active
 * clients if no asynchronous actions should be done.Asynchronous actions are
 * locking a mutex and waking up IPA HW.
 *
 * Please do not use this API, use the wrapper macros instead(ipa_i.h)
 *
 *
 * Return codes : 0 for success
 *		-EPERM if an asynchronous action should have been done
 */
int ipa_inc_client_enable_clks_no_block(
	struct ipa_active_client_logging_info *id)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_inc_client_enable_clks_no_block, id);

	return ret;
}
EXPORT_SYMBOL(ipa_inc_client_enable_clks_no_block);

/**
 * ipa_suspend_resource_no_block() - suspend client endpoints related to the
 * IPA_RM resource and decrement active clients counter. This function is
 * guaranteed to avoid sleeping.
 *
 * @resource: [IN] IPA Resource Manager resource
 *
 * Return codes: 0 on success, negative on failure.
 */
int ipa_suspend_resource_no_block(enum ipa_rm_resource_name resource)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_suspend_resource_no_block, resource);

	return ret;
}
EXPORT_SYMBOL(ipa_suspend_resource_no_block);
/**
 * ipa_resume_resource() - resume client endpoints related to the IPA_RM
 * resource.
 *
 * @resource: [IN] IPA Resource Manager resource
 *
 * Return codes: 0 on success, negative on failure.
 */
int ipa_resume_resource(enum ipa_rm_resource_name resource)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_resume_resource, resource);

	return ret;
}
EXPORT_SYMBOL(ipa_resume_resource);

/**
 * ipa_suspend_resource_sync() - suspend client endpoints related to the IPA_RM
 * resource and decrement active clients counter, which may result in clock
 * gating of IPA clocks.
 *
 * @resource: [IN] IPA Resource Manager resource
 *
 * Return codes: 0 on success, negative on failure.
 */
int ipa_suspend_resource_sync(enum ipa_rm_resource_name resource)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_suspend_resource_sync, resource);

	return ret;
}
EXPORT_SYMBOL(ipa_suspend_resource_sync);

/**
 * ipa_set_required_perf_profile() - set IPA to the specified performance
 *	profile based on the bandwidth, unless minimum voltage required is
 *	higher. In this case the floor_voltage specified will be used.
 * @floor_voltage: minimum voltage to operate
 * @bandwidth_mbps: needed bandwidth from IPA
 *
 * Return codes: 0 on success, negative on failure.
 */
int ipa_set_required_perf_profile(enum ipa_voltage_level floor_voltage,
	u32 bandwidth_mbps)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_set_required_perf_profile, floor_voltage,
		bandwidth_mbps);

	return ret;
}
EXPORT_SYMBOL(ipa_set_required_perf_profile);

/**
 * ipa_get_ipc_logbuf() - return a pointer to IPA driver IPC log
 */
void *ipa_get_ipc_logbuf(void)
{
	void *ret;

	IPA_API_DISPATCH_RETURN_PTR(ipa_get_ipc_logbuf);

	return ret;
}
EXPORT_SYMBOL(ipa_get_ipc_logbuf);

/**
 * ipa_get_ipc_logbuf_low() - return a pointer to IPA driver IPC low prio log
 */
void *ipa_get_ipc_logbuf_low(void)
{
	void *ret;

	IPA_API_DISPATCH_RETURN_PTR(ipa_get_ipc_logbuf_low);

	return ret;
}
EXPORT_SYMBOL(ipa_get_ipc_logbuf_low);

/**
 * ipa_assert() - general function for assertion
 */
void ipa_assert(void)
{
	pr_err("IPA: unrecoverable error has occurred, asserting\n");
	BUG();
}

/**
 * ipa_rx_poll() - Poll the rx packets from IPA HW in the
 * softirq context
 *
 * @budget: number of packets to be polled in single iteration
 *
 * Return codes: >= 0  : Actual number of packets polled
 *
 */
int ipa_rx_poll(u32 clnt_hdl, int budget)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_rx_poll, clnt_hdl, budget);

	return ret;
}
EXPORT_SYMBOL(ipa_rx_poll);

/**
 * ipa_recycle_wan_skb() - Recycle the Wan skb
 *
 * @skb: skb that needs to recycle
 *
 */
void ipa_recycle_wan_skb(struct sk_buff *skb)
{
	IPA_API_DISPATCH(ipa_recycle_wan_skb, skb);
}
EXPORT_SYMBOL(ipa_recycle_wan_skb);

/**
 * ipa_setup_uc_ntn_pipes() - setup uc offload pipes
 */
int ipa_setup_uc_ntn_pipes(struct ipa_ntn_conn_in_params *inp,
		ipa_notify_cb notify, void *priv, u8 hdr_len,
		struct ipa_ntn_conn_out_params *outp)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_setup_uc_ntn_pipes, inp,
		notify, priv, hdr_len, outp);

	return ret;
}

/**
 * ipa_tear_down_uc_offload_pipes() - tear down uc offload pipes
 */
int ipa_tear_down_uc_offload_pipes(int ipa_ep_idx_ul,
		int ipa_ep_idx_dl, struct ipa_ntn_conn_in_params *params)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_tear_down_uc_offload_pipes, ipa_ep_idx_ul,
		ipa_ep_idx_dl, params);

	return ret;
}

/**
 * ipa_get_pdev() - return a pointer to IPA dev struct
 *
 * Return value: a pointer to IPA dev struct
 *
 */
struct device *ipa_get_pdev(void)
{
	struct device *ret;

	IPA_API_DISPATCH_RETURN_PTR(ipa_get_pdev);

	return ret;
}
EXPORT_SYMBOL(ipa_get_pdev);

int ipa_ntn_uc_reg_rdyCB(void (*ipauc_ready_cb)(void *user_data),
			      void *user_data)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_ntn_uc_reg_rdyCB,
				ipauc_ready_cb, user_data);

	return ret;
}
EXPORT_SYMBOL(ipa_ntn_uc_reg_rdyCB);

void ipa_ntn_uc_dereg_rdyCB(void)
{
	IPA_API_DISPATCH(ipa_ntn_uc_dereg_rdyCB);
}
EXPORT_SYMBOL(ipa_ntn_uc_dereg_rdyCB);

int ipa_get_smmu_params(struct ipa_smmu_in_params *in,
	struct ipa_smmu_out_params *out)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_get_smmu_params, in, out);

	return ret;
}
EXPORT_SYMBOL(ipa_get_smmu_params);

/**
 * ipa_conn_wdi_pipes() - connect wdi pipes
 */
int ipa_conn_wdi_pipes(struct ipa_wdi_conn_in_params *in,
	struct ipa_wdi_conn_out_params *out,
	ipa_wdi_meter_notifier_cb wdi_notify)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_conn_wdi_pipes, in, out, wdi_notify);

	return ret;
}

/**
 * ipa_disconn_wdi_pipes() - disconnect wdi pipes
 */
int ipa_disconn_wdi_pipes(int ipa_ep_idx_tx, int ipa_ep_idx_rx)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_disconn_wdi_pipes, ipa_ep_idx_tx,
		ipa_ep_idx_rx);

	return ret;
}

/**
 * ipa_enable_wdi_pipes() - enable wdi pipes
 */
int ipa_enable_wdi_pipes(int ipa_ep_idx_tx, int ipa_ep_idx_rx)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_enable_wdi_pipes, ipa_ep_idx_tx,
		ipa_ep_idx_rx);

	return ret;
}

/**
 * ipa_disable_wdi_pipes() - disable wdi pipes
 */
int ipa_disable_wdi_pipes(int ipa_ep_idx_tx, int ipa_ep_idx_rx)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_disable_wdi_pipes, ipa_ep_idx_tx,
		ipa_ep_idx_rx);

	return ret;
}

/**
 * ipa_tz_unlock_reg() - Allow AP access to memory regions controlled by TZ
 */
int ipa_tz_unlock_reg(struct ipa_tz_unlock_reg_info *reg_info, u16 num_regs)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_tz_unlock_reg, reg_info, num_regs);

	return ret;
}

/**
 * ipa_pm_is_used() - Returns if IPA PM framework is used
 */
bool ipa_pm_is_used(void)
{
	bool ret;

	IPA_API_DISPATCH_RETURN(ipa_pm_is_used);

	return ret;
}

static const struct dev_pm_ops ipa_pm_ops = {
	.suspend_noirq = ipa_ap_suspend,
	.resume_noirq = ipa_ap_resume,
};

static struct platform_driver ipa_plat_drv = {
	.probe = ipa_generic_plat_drv_probe,
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &ipa_pm_ops,
		.of_match_table = ipa_plat_drv_match,
	},
};

/*********************************************************/
/*                PCIe Version                           */
/*********************************************************/

static int ipa_pci_probe(
	struct pci_dev             *pci_dev,
	const struct pci_device_id *ent)
{
	int result;

	if (!pci_dev || !ent) {
		pr_err(
		    "Bad arg: pci_dev (%pK) and/or ent (%pK)\n",
		    pci_dev, ent);
		return -EOPNOTSUPP;
	}

	if (!ipa_api_ctrl) {
		ipa_api_ctrl = kzalloc(sizeof(*ipa_api_ctrl), GFP_KERNEL);
		if (ipa_api_ctrl == NULL)
			return -ENOMEM;
		/* Get IPA HW Version */
		result = of_property_read_u32(NULL,
			"qcom,ipa-hw-ver", &ipa_api_hw_type);
		if (result || ipa_api_hw_type == 0) {
			pr_err("ipa: get resource failed for ipa-hw-ver!\n");
			kfree(ipa_api_ctrl);
			ipa_api_ctrl = NULL;
			return -ENODEV;
		}
		pr_debug("ipa: ipa_api_hw_type = %d", ipa_api_hw_type);
	}

	/*
	 * Call a reduced version of platform_probe appropriate for PCIe
	 */
	result = ipa3_pci_drv_probe(pci_dev, ipa_api_ctrl, ipa_pci_drv_match);

	if (result && result != -EPROBE_DEFER)
		pr_err("ipa: ipa3_pci_drv_probe failed\n");

	if (running_emulation)
		ipa_ut_module_init();

	return result;
}

static void ipa_pci_remove(struct pci_dev *pci_dev)
{
	if (running_emulation)
		ipa_ut_module_exit();
}

static void ipa_pci_shutdown(struct pci_dev *pci_dev)
{
}

static pci_ers_result_t ipa_pci_io_error_detected(struct pci_dev *pci_dev,
	pci_channel_state_t state)
{
	return 0;
}

static pci_ers_result_t ipa_pci_io_slot_reset(struct pci_dev *pci_dev)
{
	return 0;
}

static void ipa_pci_io_resume(struct pci_dev *pci_dev)
{
}

static int __init ipa_module_init(void)
{
	pr_debug("IPA module init\n");

	if (running_emulation) {
		/* Register as a PCI device driver */
		return pci_register_driver(&ipa_pci_driver);
	}
	/* Register as a platform device driver */
	return platform_driver_register(&ipa_plat_drv);
}
subsys_initcall(ipa_module_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("IPA HW device driver");
