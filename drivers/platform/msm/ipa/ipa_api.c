/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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
#include "ipa_api.h"

#define DRV_NAME "ipa"

#define IPA_API_DISPATCH_RETURN(api, p...) \
	do { \
		if (!ipa_api_ctrl) { \
			pr_err("IPA HW is not supported on this target\n"); \
			ret = -EPERM; \
		} \
		else { \
			if (ipa_api_ctrl->api) { \
				ret = ipa_api_ctrl->api(p); \
			} else { \
				pr_err("%s not implemented for IPA ver %d\n", \
						__func__, ipa_api_hw_type); \
				WARN_ON(1); \
				ret = -EPERM; \
			} \
		} \
	} while (0)

#define IPA_API_DISPATCH(api, p...) \
	do { \
		if (!ipa_api_ctrl) \
			pr_err("IPA HW is not supported on this target\n"); \
		else { \
			if (ipa_api_ctrl->api) { \
				ipa_api_ctrl->api(p); \
			} else { \
				pr_err("%s not implemented for IPA ver %d\n", \
						__func__, ipa_api_hw_type); \
				WARN_ON(1); \
			} \
		} \
	} while (0)

#define IPA_API_DISPATCH_RETURN_PTR(api, p...) \
	do { \
		if (!ipa_api_ctrl) { \
			pr_err("IPA HW is not supported on this target\n"); \
			ret = NULL; \
		} \
		else { \
			if (ipa_api_ctrl->api) { \
				ret = ipa_api_ctrl->api(p); \
			} else { \
				pr_err("%s not implemented for IPA ver %d\n", \
						__func__, ipa_api_hw_type); \
				WARN_ON(1); \
				ret = NULL; \
			} \
		} \
	} while (0)

static enum ipa_hw_type ipa_api_hw_type;
static struct ipa_api_controller *ipa_api_ctrl;


/**
 * ipa_connect() - low-level IPA client connect
 * @in:	[in] input parameters from client
 * @sps:	[out] sps output from IPA needed by client for sps_connect
 * @clnt_hdl:	[out] opaque client handle assigned by IPA to client
 *
 * Should be called by the driver of the peripheral that wants to connect to
 * IPA in BAM-BAM mode. these peripherals are USB and HSIC. this api
 * expects caller to take responsibility to add any needed headers, routing
 * and filtering tables and rules as needed.
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_connect(const struct ipa_connect_params *in, struct ipa_sps_params *sps,
	u32 *clnt_hdl)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_connect, in, sps, clnt_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_connect);

/**
 * ipa_disconnect() - low-level IPA client disconnect
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 *
 * Should be called by the driver of the peripheral that wants to disconnect
 * from IPA in BAM-BAM mode. this api expects caller to take responsibility to
 * free any needed headers, routing and filtering tables and rules as needed.
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_disconnect(u32 clnt_hdl)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_disconnect, clnt_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_disconnect);

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
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
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
 * ipa_cfg_ep_hdr() -  IPA end-point Control configuration
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
 * ipa_del_hdr() - Remove the specified headers from SW and optionally commit them
 * to IPA HW
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
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_reset_hdr(void)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_reset_hdr);

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
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_add_hdr_proc_ctx(struct ipa_ioc_add_hdr_proc_ctx *proc_ctxs)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_add_hdr_proc_ctx, proc_ctxs);

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
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_reset_rt(enum ipa_ip_type ip)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_reset_rt, ip);

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
 * ipa_mdfy_flt_rule() - Modify the specified filtering rules in SW and optionally
 * commit to IPA HW
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
 * @ip:	[in] the family of routing tables
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_reset_flt(enum ipa_ip_type ip)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_reset_flt, ip);

	return ret;
}
EXPORT_SYMBOL(ipa_reset_flt);

/**
 * allocate_nat_device() - Allocates memory for the NAT device
 * @mem:	[in/out] memory parameters
 *
 * Called by NAT client driver to allocate memory for the NAT entries. Based on
 * the request size either shared or system memory will be used.
 *
 * Returns:	0 on success, negative on failure
 */
int allocate_nat_device(struct ipa_ioc_nat_alloc_mem *mem)
{
	int ret;

	IPA_API_DISPATCH_RETURN(allocate_nat_device, mem);

	return ret;
}
EXPORT_SYMBOL(allocate_nat_device);

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
 * ipa_nat_del_cmd() - Delete a NAT table
 * @del:	[in] delete table table table parameters
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
 * ipa_rm_create_resource() - create resource
 * @create_params: [in] parameters needed
 *                  for resource initialization
 *
 * Returns: 0 on success, negative on failure
 *
 * This function is called by IPA RM client to initialize client's resources.
 * This API should be called before any other IPA RM API on a given resource
 * name.
 *
 */
int ipa_rm_create_resource(struct ipa_rm_create_params *create_params)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_rm_create_resource, create_params);

	return ret;
}
EXPORT_SYMBOL(ipa_rm_create_resource);

/**
 * ipa_rm_delete_resource() - delete resource
 * @resource_name: name of resource to be deleted
 *
 * Returns: 0 on success, negative on failure
 *
 * This function is called by IPA RM client to delete client's resources.
 *
 */
int ipa_rm_delete_resource(enum ipa_rm_resource_name resource_name)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_rm_delete_resource, resource_name);

	return ret;
}
EXPORT_SYMBOL(ipa_rm_delete_resource);

/**
 * ipa_rm_add_dependency() - create dependency
 *					between 2 resources
 * @resource_name: name of dependent resource
 * @depends_on_name: name of its dependency
 *
 * Returns: 0 on success, negative on failure
 *
 * Side effects: IPA_RM_RESORCE_GRANTED could be generated
 * in case client registered with IPA RM
 */
int ipa_rm_add_dependency(enum ipa_rm_resource_name resource_name,
			enum ipa_rm_resource_name depends_on_name)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_rm_add_dependency, resource_name,
		depends_on_name);

	return ret;
}
EXPORT_SYMBOL(ipa_rm_add_dependency);

/**
 * ipa_rm_delete_dependency() - create dependency
 *					between 2 resources
 * @resource_name: name of dependent resource
 * @depends_on_name: name of its dependency
 *
 * Returns: 0 on success, negative on failure
 *
 * Side effects: IPA_RM_RESORCE_GRANTED could be generated
 * in case client registered with IPA RM
 */
int ipa_rm_delete_dependency(enum ipa_rm_resource_name resource_name,
			enum ipa_rm_resource_name depends_on_name)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_rm_delete_dependency, resource_name,
		depends_on_name);

	return ret;
}
EXPORT_SYMBOL(ipa_rm_delete_dependency);

/**
 * ipa_rm_request_resource() - request resource
 * @resource_name: [in] name of the requested resource
 *
 * Returns: 0 on success, negative on failure
 *
 * All registered callbacks are called with IPA_RM_RESOURCE_GRANTED
 * on successful completion of this operation.
 */
int ipa_rm_request_resource(enum ipa_rm_resource_name resource_name)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_rm_request_resource, resource_name);

	return ret;
}
EXPORT_SYMBOL(ipa_rm_request_resource);

/**
 * ipa_rm_release_resource() - release resource
 * @resource_name: [in] name of the requested resource
 *
 * Returns: 0 on success, negative on failure
 *
 * All registered callbacks are called with IPA_RM_RESOURCE_RELEASED
 * on successful completion of this operation.
 */
int ipa_rm_release_resource(enum ipa_rm_resource_name resource_name)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_rm_release_resource, resource_name);

	return ret;
}
EXPORT_SYMBOL(ipa_rm_release_resource);

/**
 * ipa_rm_register() - register for event
 * @resource_name: resource name
 * @reg_params: [in] registration parameters
 *
 * Returns: 0 on success, negative on failure
 *
 * Registration parameters provided here should be the same
 * as provided later in  ipa_rm_deregister() call.
 */
int ipa_rm_register(enum ipa_rm_resource_name resource_name,
			struct ipa_rm_register_params *reg_params)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_rm_register, resource_name, reg_params);

	return ret;
}
EXPORT_SYMBOL(ipa_rm_register);

/**
 * ipa_rm_deregister() - cancel the registration
 * @resource_name: resource name
 * @reg_params: [in] registration parameters
 *
 * Returns: 0 on success, negative on failure
 *
 * Registration parameters provided here should be the same
 * as provided in  ipa_rm_register() call.
 */
int ipa_rm_deregister(enum ipa_rm_resource_name resource_name,
			struct ipa_rm_register_params *reg_params)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_rm_deregister, resource_name, reg_params);

	return ret;
}
EXPORT_SYMBOL(ipa_rm_deregister);

/**
 * ipa_rm_set_perf_profile() - set performance profile
 * @resource_name: resource name
 * @profile: [in] profile information.
 *
 * Returns: 0 on success, negative on failure
 *
 * Set resource performance profile.
 * Updates IPA driver if performance level changed.
 */
int ipa_rm_set_perf_profile(enum ipa_rm_resource_name resource_name,
			struct ipa_rm_perf_profile *profile)
{
	int ret;

	IPA_API_DISPATCH_RETURN(
		ipa_rm_set_perf_profile,
		resource_name,
		profile);

	return ret;
}
EXPORT_SYMBOL(ipa_rm_set_perf_profile);

/**
 * ipa_rm_notify_completion() -
 *	consumer driver notification for
 *	request_resource / release_resource operations
 *	completion
 * @event: notified event
 * @resource_name: resource name
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_rm_notify_completion(enum ipa_rm_event event,
		enum ipa_rm_resource_name resource_name)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_rm_notify_completion, event, resource_name);

	return ret;
}
EXPORT_SYMBOL(ipa_rm_notify_completion);

/**
* ipa_rm_inactivity_timer_init() - Init function for IPA RM
* inactivity timer. This function shall be called prior calling
* any other API of IPA RM inactivity timer.
*
* @resource_name: Resource name. @see ipa_rm.h
* @msecs: time in miliseccond, that IPA RM inactivity timer
* shall wait prior calling to ipa_rm_release_resource().
*
* Return codes:
* 0: success
* -EINVAL: invalid parameters
*/
int ipa_rm_inactivity_timer_init(enum ipa_rm_resource_name resource_name,
	unsigned long msecs)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_rm_inactivity_timer_init, resource_name,
		msecs);

	return ret;
}
EXPORT_SYMBOL(ipa_rm_inactivity_timer_init);

/**
* ipa_rm_inactivity_timer_destroy() - De-Init function for IPA
* RM inactivity timer.
*
* @resource_name: Resource name. @see ipa_rm.h
*
* Return codes:
* 0: success
* -EINVAL: invalid parameters
*/
int ipa_rm_inactivity_timer_destroy(enum ipa_rm_resource_name resource_name)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_rm_inactivity_timer_destroy, resource_name);

	return ret;
}
EXPORT_SYMBOL(ipa_rm_inactivity_timer_destroy);

/**
* ipa_rm_inactivity_timer_request_resource() - Same as
* ipa_rm_request_resource(), with a difference that calling to
* this function will also cancel the inactivity timer, if
* ipa_rm_inactivity_timer_release_resource() was called earlier.
*
* @resource_name: Resource name. @see ipa_rm.h
*
* Return codes:
* 0: success
* -EINVAL: invalid parameters
*/
int ipa_rm_inactivity_timer_request_resource(
	enum ipa_rm_resource_name resource_name)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_rm_inactivity_timer_request_resource,
		resource_name);

	return ret;
}
EXPORT_SYMBOL(ipa_rm_inactivity_timer_request_resource);

/**
* ipa_rm_inactivity_timer_release_resource() - Sets the
* inactivity timer to the timeout set by
* ipa_rm_inactivity_timer_init(). When the timeout expires, IPA
* RM inactivity timer will call to ipa_rm_release_resource().
* If a call to ipa_rm_inactivity_timer_request_resource() was
* made BEFORE the timout has expired, rge timer will be
* cancelled.
*
* @resource_name: Resource name. @see ipa_rm.h
*
* Return codes:
* 0: success
* -EINVAL: invalid parameters
*/
int ipa_rm_inactivity_timer_release_resource(
	enum ipa_rm_resource_name resource_name)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_rm_inactivity_timer_release_resource,
		resource_name);

	return ret;
}
EXPORT_SYMBOL(ipa_rm_inactivity_timer_release_resource);

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
 * odu_bridge_init() - Initialize the ODU bridge driver
 * @params: initialization parameters
 *
 * This function initialize all bridge internal data and register odu bridge to
 * kernel for IOCTL and debugfs.
 * Header addition and properties are registered to IPA driver.
 *
 * Return codes: 0: success,
 *		-EINVAL - Bad parameter
 *		Other negative value - Failure
 */
int odu_bridge_init(struct odu_bridge_params *params)
{
	int ret;

	IPA_API_DISPATCH_RETURN(odu_bridge_init, params);

	return ret;
}
EXPORT_SYMBOL(odu_bridge_init);

/**
 * odu_bridge_disconnect() - Disconnect odu bridge
 *
 * Disconnect all pipes and deletes IPA RM dependencies on bridge mode
 *
 * Return codes: 0- success, error otherwise
 */
int odu_bridge_disconnect(void)
{
	int ret;

	IPA_API_DISPATCH_RETURN(odu_bridge_disconnect);

	return ret;
}
EXPORT_SYMBOL(odu_bridge_disconnect);

/**
 * odu_bridge_connect() - Connect odu bridge.
 *
 * Call to the mode-specific connect function for connection IPA pipes
 * and adding IPA RM dependencies

 * Return codes: 0: success
 *		-EINVAL: invalid parameters
 *		-EPERM: Operation not permitted as the bridge is already
 *		connected
 */
int odu_bridge_connect(void)
{
	int ret;

	IPA_API_DISPATCH_RETURN(odu_bridge_connect);

	return ret;
}
EXPORT_SYMBOL(odu_bridge_connect);

/**
 * odu_bridge_tx_dp() - Send skb to ODU bridge
 * @skb: skb to send
 * @metadata: metadata on packet
 *
 * This function handles uplink packet.
 * In Router Mode:
 *	packet is sent directly to IPA.
 * In Router Mode:
 *	packet is classified if it should arrive to network stack.
 *	QMI IP packet should arrive to APPS network stack
 *	IPv6 Multicast packet should arrive to APPS network stack and Q6
 *
 * Return codes: 0- success, error otherwise
 */
int odu_bridge_tx_dp(struct sk_buff *skb, struct ipa_tx_meta *metadata)
{
	int ret;

	IPA_API_DISPATCH_RETURN(odu_bridge_tx_dp, skb, metadata);

	return ret;
}
EXPORT_SYMBOL(odu_bridge_tx_dp);

/**
 * odu_bridge_cleanup() - De-Initialize the ODU bridge driver
 *
 * Return codes: 0: success,
 *		-EINVAL - Bad parameter
 *		Other negative value - Failure
 */
int odu_bridge_cleanup(void)
{
	int ret;

	IPA_API_DISPATCH_RETURN(odu_bridge_cleanup);

	return ret;
}
EXPORT_SYMBOL(odu_bridge_cleanup);

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

/**
 * ipa_mhi_init() - Initialize IPA MHI driver
 * @params: initialization params
 *
 * This function is called by MHI client driver on boot to initialize IPA MHI
 * Driver. When this function returns device can move to READY state.
 * This function is doing the following:
 *	- Initialize MHI IPA internal data structures
 *	- Create IPA RM resources
 *	- Initialize debugfs
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int ipa_mhi_init(struct ipa_mhi_init_params *params)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_mhi_init, params);

	return ret;
}
EXPORT_SYMBOL(ipa_mhi_init);

/**
 * ipa_mhi_start() - Start IPA MHI engine
 * @params: pcie addresses for MHI
 *
 * This function is called by MHI client driver on MHI engine start for
 * handling MHI accelerated channels. This function is called after
 * ipa_mhi_init() was called and can be called after MHI reset to restart MHI
 * engine. When this function returns device can move to M0 state.
 * This function is doing the following:
 *	- Send command to uC for initialization of MHI engine
 *	- Add dependencies to IPA RM
 *	- Request MHI_PROD in IPA RM
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int ipa_mhi_start(struct ipa_mhi_start_params *params)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_mhi_start, params);

	return ret;
}
EXPORT_SYMBOL(ipa_mhi_start);

/**
 * ipa_mhi_connect_pipe() - Connect pipe to IPA and start corresponding
 * MHI channel
 * @in: connect parameters
 * @clnt_hdl: [out] client handle for this pipe
 *
 * This function is called by MHI client driver on MHI channel start.
 * This function is called after MHI engine was started.
 * This function is doing the following:
 *	- Send command to uC to start corresponding MHI channel
 *	- Configure IPA EP control
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int ipa_mhi_connect_pipe(struct ipa_mhi_connect_params *in, u32 *clnt_hdl)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_mhi_connect_pipe, in, clnt_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_mhi_connect_pipe);

/**
 * ipa_mhi_disconnect_pipe() - Disconnect pipe from IPA and reset corresponding
 * MHI channel
 * @in: connect parameters
 * @clnt_hdl: [out] client handle for this pipe
 *
 * This function is called by MHI client driver on MHI channel reset.
 * This function is called after MHI channel was started.
 * This function is doing the following:
 *	- Send command to uC to reset corresponding MHI channel
 *	- Configure IPA EP control
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int ipa_mhi_disconnect_pipe(u32 clnt_hdl)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_mhi_disconnect_pipe, clnt_hdl);

	return ret;
}
EXPORT_SYMBOL(ipa_mhi_disconnect_pipe);

/**
 * ipa_mhi_suspend() - Suspend MHI accelerated channels
 * @force:
 *	false: in case of data pending in IPA, MHI channels will not be
 *		suspended and function will fail.
 *	true:  in case of data pending in IPA, make sure no further access from
 *		IPA to PCIe is possible. In this case suspend cannot fail.
 *
 * This function is called by MHI client driver on MHI suspend.
 * This function is called after MHI channel was started.
 * When this function returns device can move to M1/M2/M3/D3cold state.
 * This function is doing the following:
 *	- Send command to uC to suspend corresponding MHI channel
 *	- Make sure no further access is possible from IPA to PCIe
 *	- Release MHI_PROD in IPA RM
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int ipa_mhi_suspend(bool force)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_mhi_suspend, force);

	return ret;
}
EXPORT_SYMBOL(ipa_mhi_suspend);

/**
 * ipa_mhi_resume() - Resume MHI accelerated channels
 *
 * This function is called by MHI client driver on MHI resume.
 * This function is called after MHI channel was suspended.
 * When this function returns device can move to M0 state.
 * This function is doing the following:
 *	- Send command to uC to resume corresponding MHI channel
 *	- Request MHI_PROD in IPA RM
 *	- Resume data to IPA
 *
 * Return codes: 0	  : success
 *		 negative : error
 */
int ipa_mhi_resume(void)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_mhi_resume);

	return ret;
}
EXPORT_SYMBOL(ipa_mhi_resume);

/**
 * ipa_mhi_destroy() - Destroy MHI IPA
 *
 * This function is called by MHI client driver on MHI reset to destroy all IPA
 * MHI resources.
 */
void ipa_mhi_destroy(void)
{
	IPA_API_DISPATCH(ipa_mhi_destroy);

}
EXPORT_SYMBOL(ipa_mhi_destroy);

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
* */
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
 * ipa_rm_add_dependency_sync() - Create a dependency between 2 resources
 * in a synchronized fashion. In case a producer resource is in GRANTED state
 * and the newly added consumer resource is in RELEASED state, the consumer
 * entity will be requested and the function will block until the consumer
 * is granted.
 * @resource_name: name of dependent resource
 * @depends_on_name: name of its dependency
 *
 * Returns: 0 on success, negative on failure
 *
 * Side effects: May block. See documentation above.
 */
int ipa_rm_add_dependency_sync(enum ipa_rm_resource_name resource_name,
		enum ipa_rm_resource_name depends_on_name)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_rm_add_dependency_sync, resource_name,
		depends_on_name);

	return ret;
}
EXPORT_SYMBOL(ipa_rm_add_dependency_sync);

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
 * @ipa_ep_idx: IPA endpoint index
 *
 * Return value: pointer to ipa_gsi_ep_info
 */
struct ipa_gsi_ep_config *ipa_get_gsi_ep_info(int ipa_ep_idx)
{
	if (!ipa_api_ctrl || !ipa_api_ctrl->ipa_get_gsi_ep_info)
		return NULL;
	return ipa_api_ctrl->ipa_get_gsi_ep_info(ipa_ep_idx);
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

static struct of_device_id ipa_plat_drv_match[] = {
	{ .compatible = "qcom,ipa", },
	{ .compatible = "qcom,ipa-smmu-ap-cb", },
	{ .compatible = "qcom,ipa-smmu-wlan-cb", },
	{ .compatible = "qcom,ipa-smmu-uc-cb", },
	{}
};

static int ipa_generic_plat_drv_probe(struct platform_device *pdev_p)
{
	int result;

	pr_debug("ipa: IPA driver probing started\n");

	ipa_api_ctrl = kzalloc(sizeof(*ipa_api_ctrl), GFP_KERNEL);
	if (!ipa_api_ctrl)
		return -ENOMEM;

	/* Get IPA HW Version */
	result = of_property_read_u32(pdev_p->dev.of_node, "qcom,ipa-hw-ver",
		&ipa_api_hw_type);
	if ((result) || (ipa_api_hw_type == 0)) {
		pr_err("ipa: get resource failed for ipa-hw-ver!\n");
		result = -ENODEV;
		goto fail;
	}
	pr_debug("ipa: ipa_api_hw_type = %d", ipa_api_hw_type);

	/* call probe based on IPA HW version */
	switch (ipa_api_hw_type) {
	case IPA_HW_v2_0:
	case IPA_HW_v2_1:
	case IPA_HW_v2_5:
	case IPA_HW_v2_6L:
		result = ipa_plat_drv_probe(pdev_p, ipa_api_ctrl,
			ipa_plat_drv_match);
		if (result) {
			pr_err("ipa: ipa_plat_drv_probe failed\n");
			goto fail;
		}
		break;
	case IPA_HW_v3_0:
	case IPA_HW_v3_1:
		result = ipa3_plat_drv_probe(pdev_p, ipa_api_ctrl,
			ipa_plat_drv_match);
		if (result) {
			pr_err("ipa: ipa3_plat_drv_probe failed\n");
			goto fail;
		}
		break;
	default:
		pr_err("ipa: unsupported version %d\n", ipa_api_hw_type);
		result = -EPERM;
		goto fail;
	}

	return 0;
fail:
	kfree(ipa_api_ctrl);
	ipa_api_ctrl = 0;
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

int ipa_usb_init_teth_prot(enum ipa_usb_teth_prot teth_prot,
	struct ipa_usb_teth_params *teth_params,
	int (*ipa_usb_notify_cb)(enum ipa_usb_notify_event, void *),
	void *user_data)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_usb_init_teth_prot, teth_prot, teth_params,
		ipa_usb_notify_cb, user_data);

	return ret;
}
EXPORT_SYMBOL(ipa_usb_init_teth_prot);

int ipa_usb_xdci_connect(struct ipa_usb_xdci_chan_params *ul_chan_params,
	struct ipa_usb_xdci_chan_params *dl_chan_params,
	struct ipa_req_chan_out_params *ul_out_params,
	struct ipa_req_chan_out_params *dl_out_params,
	struct ipa_usb_xdci_connect_params *connect_params)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_usb_xdci_connect, ul_chan_params,
		dl_chan_params, ul_out_params, dl_out_params, connect_params);

	return ret;
}
EXPORT_SYMBOL(ipa_usb_xdci_connect);

int ipa_usb_xdci_disconnect(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
	enum ipa_usb_teth_prot teth_prot)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_usb_xdci_disconnect, ul_clnt_hdl,
		dl_clnt_hdl, teth_prot);

	return ret;
}
EXPORT_SYMBOL(ipa_usb_xdci_disconnect);

int ipa_usb_deinit_teth_prot(enum ipa_usb_teth_prot teth_prot)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_usb_deinit_teth_prot, teth_prot);

	return ret;
}
EXPORT_SYMBOL(ipa_usb_deinit_teth_prot);

int ipa_usb_xdci_suspend(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
	enum ipa_usb_teth_prot teth_prot)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_usb_xdci_suspend, ul_clnt_hdl,
		dl_clnt_hdl, teth_prot);

	return ret;
}
EXPORT_SYMBOL(ipa_usb_xdci_suspend);

int ipa_usb_xdci_resume(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
	enum ipa_usb_teth_prot teth_prot)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_usb_xdci_resume, ul_clnt_hdl,
		dl_clnt_hdl, teth_prot);

	return ret;
}
EXPORT_SYMBOL(ipa_usb_xdci_resume);

int ipa_register_ipa_ready_cb(void (*ipa_ready_cb)(void *user_data),
			      void *user_data)
{
	int ret;

	IPA_API_DISPATCH_RETURN(ipa_register_ipa_ready_cb,
				ipa_ready_cb, user_data);

	return ret;
}
EXPORT_SYMBOL(ipa_register_ipa_ready_cb);

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

static int __init ipa_module_init(void)
{
	pr_debug("IPA module init\n");

	/* Register as a platform device driver */
	return platform_driver_register(&ipa_plat_drv);
}
subsys_initcall(ipa_module_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("IPA HW device driver");
