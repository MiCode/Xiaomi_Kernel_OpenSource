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
#include <linux/tegra-powergate.h>

#include "powergate-priv.h"

int tegraxx_powergate_partition(int id,
	struct powergate_partition_info *pg_info)
{
	int ret;

	/* If first clk_ptr is null, fill clk info for the partition */
	if (pg_info->clk_info[0].clk_ptr)
		get_clk_info(pg_info);

	powergate_partition_assert_reset(pg_info);

	/* Powergating is done only if refcnt of all clks is 0 */
	ret = is_partition_clk_disabled(pg_info);
	if (ret)
		goto err_clk_off;

	ret = powergate_module(id);
	if (ret)
		goto err_power_off;

	return 0;

err_power_off:
	WARN(1, "Could not Powergate Partition %d", id);
err_clk_off:
	WARN(1, "Could not Powergate Partition %d, all clks not disabled", id);
	return ret;
}

int tegraxx_unpowergate_partition(int id,
	struct powergate_partition_info *pg_info)
{
	int ret;

	/* If first clk_ptr is null, fill clk info for the partition */
	if (!pg_info->clk_info[0].clk_ptr)
		get_clk_info(pg_info);

	if (tegra_powergate_is_powered(id))
		return tegra_powergate_reset_module(pg_info);

	ret = unpowergate_module(id);
	if (ret)
		goto err_power;

	powergate_partition_assert_reset(pg_info);

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

	tegra_powergate_mc_flush_done(id);

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

int tegraxx_powergate_partition_with_clk_off(int id,
	struct powergate_partition_info *pg_info)
{
	int ret = 0;

	/* Disable clks for the partition */
	partition_clk_disable(pg_info);

	ret = is_partition_clk_disabled(pg_info);
	if (ret)
		goto err_powergate_clk;

	ret = tegra_powergate_partition(id);
	if (ret)
		goto err_powergating;

	return ret;

err_powergate_clk:
	WARN(1, "Could not Powergate Partition %d, all clks not disabled", id);
err_powergating:
	ret = partition_clk_enable(pg_info);
	if (ret)
		return ret;
	WARN(1, "Could not Powergate Partition %d", id);
	return ret;
}

int tegraxx_unpowergate_partition_with_clk_on(int id,
	struct powergate_partition_info *pg_info)
{
	int ret = 0;

	ret = tegra_unpowergate_partition(id);
	if (ret)
		goto err_unpowergating;

	/* Enable clks for the partition */
	ret = partition_clk_enable(pg_info);
	if (ret)
		goto err_unpowergate_clk;

	return ret;

err_unpowergate_clk:
	tegra_powergate_partition(id);
	WARN(1, "Could not Un-Powergate %d, err in enabling clk", id);
err_unpowergating:
	WARN(1, "Could not Un-Powergate %d", id);

	return ret;
}
