/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/radix-tree.h>
#include <linux/clk.h>
#include <mach/msm_bus_board.h>
#include <mach/msm_bus.h>
#include "msm_bus_core.h"

static DEFINE_MUTEX(msm_bus_config_lock);

/**
 * msm_bus_axi_porthalt() - Halt the given axi master port
 * @master_port: AXI Master port to be halted
 */
int msm_bus_axi_porthalt(int master_port)
{
	int ret = 0;
	int priv_id;
	struct msm_bus_fabric_device *fabdev;

	priv_id = msm_bus_board_get_iid(master_port);
	MSM_BUS_DBG("master_port: %d iid: %d fabid%d\n",
		master_port, priv_id, GET_FABID(priv_id));
	fabdev = msm_bus_get_fabric_device(GET_FABID(priv_id));
	if (IS_ERR(fabdev)) {
		MSM_BUS_ERR("Fabric device not found for mport: %d\n",
			master_port);
		return -ENODEV;
	}
	mutex_lock(&msm_bus_config_lock);
	ret = fabdev->algo->port_halt(fabdev, priv_id);
	mutex_unlock(&msm_bus_config_lock);
	return ret;
}
EXPORT_SYMBOL(msm_bus_axi_porthalt);

/**
 * msm_bus_axi_portunhalt() - Unhalt the given axi master port
 * @master_port: AXI Master port to be unhalted
 */
int msm_bus_axi_portunhalt(int master_port)
{
	int ret = 0;
	int priv_id;
	struct msm_bus_fabric_device *fabdev;

	priv_id = msm_bus_board_get_iid(master_port);
	MSM_BUS_DBG("master_port: %d iid: %d fabid: %d\n",
		master_port, priv_id, GET_FABID(priv_id));
	fabdev = msm_bus_get_fabric_device(GET_FABID(priv_id));
	if (IS_ERR(fabdev)) {
		MSM_BUS_ERR("Fabric device not found for mport: %d\n",
			master_port);
		return -ENODEV;
	}
	mutex_lock(&msm_bus_config_lock);
	ret = fabdev->algo->port_unhalt(fabdev, priv_id);
	mutex_unlock(&msm_bus_config_lock);
	return ret;
}
EXPORT_SYMBOL(msm_bus_axi_portunhalt);
