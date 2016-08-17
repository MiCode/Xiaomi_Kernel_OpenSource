/*
 * drivers/video/tegra/dc/mipi_cal.c
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION, All rights reserved.
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

#include <linux/ioport.h>
#include <linux/gfp.h>
#include "dc_priv.h"
#include "mipi_cal.h"
#include "mipi_cal_regs.h"
#include "dsi.h"

int tegra_mipi_cal_init_hw(struct tegra_mipi_cal *mipi_cal)
{
	unsigned cnt = MIPI_CAL_MIPI_CAL_CTRL_0;

	BUG_ON(IS_ERR_OR_NULL(mipi_cal));

	mutex_lock(&mipi_cal->lock);

	tegra_mipi_cal_clk_enable(mipi_cal);

	for (; cnt <= MIPI_CAL_MIPI_BIAS_PAD_CFG2_0; cnt += 4)
		tegra_mipi_cal_write(mipi_cal, 0, cnt);

	/* Clear MIPI cal status register */
	tegra_mipi_cal_write(mipi_cal,
			MIPI_AUTO_CAL_DONE_DSID(0x1) |
			MIPI_AUTO_CAL_DONE_DSIC(0x1) |
			MIPI_AUTO_CAL_DONE_DSIB(0x1) |
			MIPI_AUTO_CAL_DONE_DSIA(0x1) |
			MIPI_AUTO_CAL_DONE_CSIE(0x1) |
			MIPI_AUTO_CAL_DONE_CSID(0x1) |
			MIPI_AUTO_CAL_DONE_CSIC(0x1) |
			MIPI_AUTO_CAL_DONE_CSIB(0x1) |
			MIPI_AUTO_CAL_DONE_CSIA(0x1) |
			MIPI_AUTO_CAL_DONE(0x1) |
			MIPI_CAL_DRIV_DN_ADJ(0x0) |
			MIPI_CAL_DRIV_UP_ADJ(0x0) |
			MIPI_CAL_TERMADJ(0x0) |
			MIPI_CAL_ACTIVE(0x0),
		MIPI_CAL_CIL_MIPI_CAL_STATUS_0);

	tegra_mipi_cal_clk_disable(mipi_cal);
	mutex_unlock(&mipi_cal->lock);

	return 0;
}
EXPORT_SYMBOL(tegra_mipi_cal_init_hw);

struct tegra_mipi_cal *tegra_mipi_cal_init_sw(struct tegra_dc *dc)
{
	struct tegra_mipi_cal *mipi_cal;
	struct resource *res;
	struct clk *clk;
	struct clk *fixed_clk;
	void __iomem *base;
	int err = 0;

	mipi_cal = devm_kzalloc(&dc->ndev->dev, sizeof(*mipi_cal), GFP_KERNEL);
	if (!mipi_cal) {
		dev_err(&dc->ndev->dev, "mipi_cal: memory allocation fail\n");
		err = -ENOMEM;
		goto fail;
	}

	res = platform_get_resource_byname(dc->ndev,
				IORESOURCE_MEM, "mipi_cal");
	if (!res) {
		dev_err(&dc->ndev->dev, "mipi_cal: no entry in resource\n");
		err = -ENOENT;
		goto fail_free_mipi_cal;
	}

	base = devm_request_and_ioremap(&dc->ndev->dev, res);
	if (!base) {
		dev_err(&dc->ndev->dev, "mipi_cal: bus to virtual mapping failed\n");
		err = -EBUSY;
		goto fail_free_res;
	}

	clk = clk_get_sys("mipi-cal", NULL);
	if (IS_ERR_OR_NULL(clk)) {
		dev_err(&dc->ndev->dev, "mipi_cal: clk get failed\n");
		err = PTR_ERR(clk);
		goto fail_free_map;
	}
	fixed_clk = clk_get_sys("mipi-cal-fixed", NULL);
	if (IS_ERR_OR_NULL(fixed_clk)) {
		dev_err(&dc->ndev->dev, "mipi_cal: fixed clk get failed\n");
		err = PTR_ERR(fixed_clk);
		goto fail_free_map;
	}

	mutex_init(&mipi_cal->lock);
	mipi_cal->dc = dc;
	mipi_cal->res = res;
	mipi_cal->base = base;
	mipi_cal->clk = clk;
	mipi_cal->fixed_clk = fixed_clk;

	return mipi_cal;

fail_free_map:
	devm_iounmap(&dc->ndev->dev, base);
	devm_release_mem_region(&dc->ndev->dev, res->start, resource_size(res));
fail_free_res:
	release_resource(res);
fail_free_mipi_cal:
	devm_kfree(&dc->ndev->dev, mipi_cal);
fail:
	return ERR_PTR(err);
}
EXPORT_SYMBOL(tegra_mipi_cal_init_sw);

void tegra_mipi_cal_destroy(struct tegra_dc *dc)
{
	struct tegra_mipi_cal *mipi_cal =
		((struct tegra_dc_dsi_data *)
		(tegra_dc_get_outdata(dc)))->mipi_cal;

	BUG_ON(IS_ERR_OR_NULL(mipi_cal));

	mutex_lock(&mipi_cal->lock);

	clk_put(mipi_cal->clk);
	devm_iounmap(&dc->ndev->dev, mipi_cal->base);
	devm_release_mem_region(&dc->ndev->dev, mipi_cal->res->start,
					resource_size(mipi_cal->res));
	release_resource(mipi_cal->res);

	mutex_unlock(&mipi_cal->lock);

	mutex_destroy(&mipi_cal->lock);
	devm_kfree(&dc->ndev->dev, mipi_cal);
}
EXPORT_SYMBOL(tegra_mipi_cal_destroy);

