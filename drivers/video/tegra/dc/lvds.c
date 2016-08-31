/*
 * drivers/video/tegra/dc/lvds.c
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/err.h>
#include <linux/kernel.h>

#include <mach/dc.h>

#include "lvds.h"
#include "dc_priv.h"


static int tegra_dc_lvds_init(struct tegra_dc *dc)
{
	struct tegra_dc_lvds_data *lvds;
	int err;

	lvds = kzalloc(sizeof(*lvds), GFP_KERNEL);
	if (!lvds)
		return -ENOMEM;

	lvds->dc = dc;
	lvds->sor = tegra_dc_sor_init(dc, NULL);

	if (IS_ERR_OR_NULL(lvds->sor)) {
		err = PTR_ERR(lvds->sor);
		lvds->sor = NULL;
		goto err_init;
	}
	tegra_dc_set_outdata(dc, lvds);

	return 0;

err_init:
	kfree(lvds);

	return err;
}


static void tegra_dc_lvds_destroy(struct tegra_dc *dc)
{
	struct tegra_dc_lvds_data *lvds = tegra_dc_get_outdata(dc);

	if (lvds->sor)
		tegra_dc_sor_destroy(lvds->sor);
	kfree(lvds);
}


static void tegra_dc_lvds_enable(struct tegra_dc *dc)
{
	struct tegra_dc_lvds_data *lvds = tegra_dc_get_outdata(dc);

	tegra_dc_io_start(dc);

	/* Power on panel */
	tegra_dc_sor_set_panel_power(lvds->sor, true);
	tegra_dc_sor_set_internal_panel(lvds->sor, true);
	tegra_dc_sor_set_power_state(lvds->sor, 1);
	tegra_dc_sor_enable_lvds(lvds->sor, false, false);
	tegra_dc_io_end(dc);
}

static void tegra_dc_lvds_disable(struct tegra_dc *dc)
{
	struct tegra_dc_lvds_data *lvds = tegra_dc_get_outdata(dc);

	/* Power down SOR */
	tegra_dc_sor_disable(lvds->sor, true);
}


static void tegra_dc_lvds_suspend(struct tegra_dc *dc)
{
	struct tegra_dc_lvds_data *lvds = tegra_dc_get_outdata(dc);

	tegra_dc_lvds_disable(dc);
	lvds->suspended = true;
}


static void tegra_dc_lvds_resume(struct tegra_dc *dc)
{
	struct tegra_dc_lvds_data *lvds = tegra_dc_get_outdata(dc);

	if (!lvds->suspended)
		return;
	tegra_dc_lvds_enable(dc);
}

static long tegra_dc_lvds_setup_clk(struct tegra_dc *dc, struct clk *clk)
{
	struct tegra_dc_lvds_data *lvds = tegra_dc_get_outdata(dc);
	struct clk	*parent_clk;

	tegra_dc_sor_setup_clk(lvds->sor, clk, true);

	parent_clk = clk_get_parent(clk);
	if (clk_get_parent(lvds->sor->sor_clk) != parent_clk)
		clk_set_parent(lvds->sor->sor_clk, parent_clk);

	return tegra_dc_pclk_round_rate(dc, lvds->sor->dc->mode.pclk);
}

struct tegra_dc_out_ops tegra_dc_lvds_ops = {
	.init	   = tegra_dc_lvds_init,
	.destroy   = tegra_dc_lvds_destroy,
	.enable	   = tegra_dc_lvds_enable,
	.disable   = tegra_dc_lvds_disable,
	.suspend   = tegra_dc_lvds_suspend,
	.resume	   = tegra_dc_lvds_resume,
	.setup_clk = tegra_dc_lvds_setup_clk,
};
