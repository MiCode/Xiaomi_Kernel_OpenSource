/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>

#include <mach/powergate.h>

#include "powergate-priv.h"

int tegra1xx_powergate(int id, struct powergate_partition_info *pg_info)
{
	int ret;

	/* If first clk_ptr is null, fill clk info for the partition */
	if (!pg_info->clk_info[0].clk_ptr)
		get_clk_info(pg_info);

	ret = partition_clk_enable(pg_info);
	if (ret)
		WARN(1, "Couldn't enable clock");

	udelay(10);

	tegra_powergate_mc_flush(id);

	udelay(10);

	powergate_partition_assert_reset(pg_info);

	udelay(10);

	/* Powergating is done only if refcnt of all clks is 0 */
	partition_clk_disable(pg_info);

	udelay(10);

	ret = tegra_powergate_set(id, false);
	if (ret)
		goto err_power_off;

	return 0;

err_power_off:
	WARN(1, "Could not Powergate Partition %d", id);
	return ret;
}

int tegra1xx_unpowergate(int id, struct powergate_partition_info *pg_info)
{
	int ret;

	/* If first clk_ptr is null, fill clk info for the partition */
	if (!pg_info->clk_info[0].clk_ptr)
		get_clk_info(pg_info);

	if (tegra_powergate_is_powered(id))
		return tegra_powergate_reset_module(pg_info);

	ret = tegra_powergate_set(id, true);
	if (ret)
		goto err_power;

	udelay(10);

	/* Un-Powergating fails if all clks are not enabled */
	ret = partition_clk_enable(pg_info);
	if (ret)
		goto err_clk_on;

	udelay(10);

	ret = tegra_powergate_remove_clamping(id);
	if (ret)
		goto err_clamp;

	udelay(10);

	powergate_partition_deassert_reset(pg_info);

	udelay(10);

	tegra_powergate_mc_flush_done(id);

	udelay(10);

	/* Disable all clks enabled earlier. Drivers should enable clks */
	partition_clk_disable(pg_info);

	return 0;

err_clamp:
	partition_clk_disable(pg_info);
err_clk_on:
	powergate_module(id);
err_power:
	WARN(1, "Could not Un-Powergate %d", id);
	return ret;
}
