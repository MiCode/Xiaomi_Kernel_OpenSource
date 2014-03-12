/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
	else {
		pr_err("%s: Bus driver not ready.",
				__func__);
		return 0;
	}
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
	else {
		pr_err("%s: Bus driver not ready.",
				__func__);
		return -EPROBE_DEFER;
	}
}
EXPORT_SYMBOL(msm_bus_scale_client_update_request);

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
