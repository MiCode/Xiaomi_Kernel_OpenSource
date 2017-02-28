/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "AXI: %s(): " fmt, __func__

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/radix-tree.h>
#include <linux/clk.h>
#include <linux/msm-bus.h>
#include "msm_bus_core.h"

struct msm_bus_arb_ops arb_ops;

/**
 * msm_bus_scale_register_client() - Register the clients with the msm bus
 * driver
 * @pdata: Platform data of the client, containing src, dest, ab, ib.
 * Return non-zero value in case of success, 0 in case of failure.
 *
 * Client data contains the vectors specifying arbitrated bandwidth (ab)
 * and instantaneous bandwidth (ib) requested between a particular
 * src and dest.
 */
uint32_t msm_bus_scale_register_client(struct msm_bus_scale_pdata *pdata)
{
	if (arb_ops.register_client)
		return arb_ops.register_client(pdata);
	pr_err("%s: Bus driver not ready.",
			__func__);
	return 0;
}
EXPORT_SYMBOL(msm_bus_scale_register_client);

/**
 * msm_bus_scale_client_update_request() - Update the request for bandwidth
 * from a particular client
 *
 * cl: Handle to the client
 * index: Index into the vector, to which the bw and clock values need to be
 * updated
 */
int msm_bus_scale_client_update_request(uint32_t cl, unsigned int index)
{
	if (arb_ops.update_request)
		return arb_ops.update_request(cl, index);
	pr_err("%s: Bus driver not ready.",
			__func__);
	return -EPROBE_DEFER;
}
EXPORT_SYMBOL(msm_bus_scale_client_update_request);

/**
 * msm_bus_scale_client_update_context() - Update the context for a client
 * cl: Handle to the client
 * active_only: Bool to indicate dual context or active-only context.
 * ctx_idx: Voting index to be used when switching contexts.
 */
int msm_bus_scale_client_update_context(uint32_t cl, bool active_only,
							unsigned int ctx_idx)
{
	if (arb_ops.update_context)
		return arb_ops.update_context(cl, active_only, ctx_idx);

	return -EPROBE_DEFER;
}
EXPORT_SYMBOL(msm_bus_scale_client_update_context);

/**
 * msm_bus_scale_unregister_client() - Unregister the client from the bus driver
 * @cl: Handle to the client
 */
void msm_bus_scale_unregister_client(uint32_t cl)
{
	if (arb_ops.unregister_client)
		arb_ops.unregister_client(cl);
	else {
		pr_err("%s: Bus driver not ready.",
				__func__);
	}
}
EXPORT_SYMBOL(msm_bus_scale_unregister_client);

/**
 * msm_bus_scale_register() - Register the clients with the msm bus
 * driver
 *
 * @mas: Master ID
 * @slv: Slave ID
 * @name: descriptive name for this client
 * @active_only: Whether or not this bandwidth vote should only be
 *               effective while the application processor is active.
 *
 * Client data contains the vectors specifying arbitrated bandwidth (ab)
 * and instantaneous bandwidth (ib) requested between a particular
 * src and dest.
 */
struct msm_bus_client_handle*
msm_bus_scale_register(uint32_t mas, uint32_t slv, char *name, bool active_only)
{
	if (arb_ops.register_cl)
		return arb_ops.register_cl(mas, slv, name, active_only);
	pr_err("%s: Bus driver not ready.",
			__func__);
	return ERR_PTR(-EPROBE_DEFER);
}
EXPORT_SYMBOL(msm_bus_scale_register);

/**
 * msm_bus_scale_client_update_bw() - Update the request for bandwidth
 * from a particular client
 *
 * @cl: Handle to the client
 * @ab: Arbitrated bandwidth being requested
 * @ib: Instantaneous bandwidth being requested
 */
int msm_bus_scale_update_bw(struct msm_bus_client_handle *cl, u64 ab, u64 ib)
{
	if (arb_ops.update_request)
		return arb_ops.update_bw(cl, ab, ib);
	pr_err("%s: Bus driver not ready.", __func__);
	return -EPROBE_DEFER;
}
EXPORT_SYMBOL(msm_bus_scale_update_bw);

/**
 * msm_bus_scale_change_context() - Update the context for a particular client
 * cl: Handle to the client
 * act_ab: The average bandwidth(AB) in Bytes/s to be used in active context.
 * act_ib: The instantaneous bandwidth(IB) in Bytes/s to be used in active
 *         context.
 * slp_ib: The average bandwidth(AB) in Bytes/s to be used in dual context.
 * slp_ab: The instantaneous bandwidth(IB) in Bytes/s to be used in dual
 *         context.
 */
int
msm_bus_scale_update_bw_context(struct msm_bus_client_handle *cl, u64 act_ab,
				u64 act_ib, u64 slp_ib, u64 slp_ab)
{
	if (arb_ops.update_context)
		return arb_ops.update_bw_context(cl, act_ab, act_ib,
							slp_ab, slp_ib);

	return -EPROBE_DEFER;
}
EXPORT_SYMBOL(msm_bus_scale_update_bw_context);

/**
 * msm_bus_scale_unregister() - Update the request for bandwidth
 * from a particular client
 *
 * cl: Handle to the client
 */
void msm_bus_scale_unregister(struct msm_bus_client_handle *cl)
{
	if (arb_ops.unregister)
		arb_ops.unregister(cl);
	else
		pr_err("%s: Bus driver not ready.",
				__func__);
}
EXPORT_SYMBOL(msm_bus_scale_unregister);

/**
 * msm_bus_scale_query_tcs_cmd() - Query for a list of TCS commands for
 * an aggregated votes of paths from a single usecase.
 *
 * tcs_usecase: pointer to client allocated memory blob
 * cl: Handle to the client
 * index: Index into the vector, to which the bw and clock values need to be
 * updated
 */
int msm_bus_scale_query_tcs_cmd(struct msm_bus_tcs_usecase *tcs_usecase,
					uint32_t cl, unsigned int index)
{
	if (arb_ops.query_usecase)
		return arb_ops.query_usecase(tcs_usecase, cl, index);
	pr_err("%s: Bus driver not ready.",
			__func__);
	return -EPROBE_DEFER;
}
EXPORT_SYMBOL(msm_bus_scale_query_tcs_cmd);

/**
 * msm_bus_scale_query_tcs_cmd_all() - Query for a list of TCS commands for
 * an aggregated vote of paths for all usecases registered by client
 *
 * tcs_handle: pointer to client allocated memory blob
 * cl: Handle to the client
 *
 */
int msm_bus_scale_query_tcs_cmd_all(struct msm_bus_tcs_handle *tcs_handle,
					uint32_t cl)
{
	if (arb_ops.query_usecase)
		return arb_ops.query_usecase_all(tcs_handle, cl);
	pr_err("%s: Bus driver not ready.",
			__func__);
	return -EPROBE_DEFER;
}
EXPORT_SYMBOL(msm_bus_scale_query_tcs_cmd_all);
