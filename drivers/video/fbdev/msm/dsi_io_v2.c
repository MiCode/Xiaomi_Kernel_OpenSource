/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>

#include <mach/clk.h>

#include "dsi_v2.h"
#include "dsi_io_v2.h"
#include "dsi_host_v2.h"

struct msm_dsi_io_private {
	struct clk *dsi_byte_clk;
	struct clk *dsi_esc_clk;
	struct clk *dsi_pixel_clk;
	struct clk *dsi_ahb_clk;
	struct clk *dsi_clk;
	int msm_dsi_clk_on;
	int msm_dsi_ahb_clk_on;
};

static struct msm_dsi_io_private *dsi_io_private;

#define DSI_VDDA_VOLTAGE 1200000

void msm_dsi_ahb_ctrl(int enable)
{
	if (enable) {
		dsi_io_private->msm_dsi_ahb_clk_on++;
		if (dsi_io_private->msm_dsi_ahb_clk_on == 1)
			clk_enable(dsi_io_private->dsi_ahb_clk);
	} else {
		dsi_io_private->msm_dsi_ahb_clk_on--;
		if (dsi_io_private->msm_dsi_ahb_clk_on == 0)
			clk_disable(dsi_io_private->dsi_ahb_clk);
	}
}

int msm_dsi_io_init(struct platform_device *pdev, struct dss_module_power *mp)
{
	int rc;

	if (!dsi_io_private) {
		dsi_io_private = kzalloc(sizeof(struct msm_dsi_io_private),
					GFP_KERNEL);
		if (!dsi_io_private) {
			pr_err("fail to alloc dsi io private data structure\n");
			return -ENOMEM;
		}
	}

	rc = msm_dsi_clk_init(pdev);
	if (rc) {
		pr_err("fail to initialize DSI clock\n");
		return rc;
	}

	rc = msm_dss_config_vreg(&pdev->dev, mp->vreg_config,
						mp->num_vreg, 1);
	if (rc) {
		pr_err("fail to initialize DSI regulator\n");
		return rc;
	}

	return 0;
}

void msm_dsi_io_deinit(struct platform_device *pdev,
				 struct dss_module_power *mp)
{
	if (dsi_io_private) {
		msm_dsi_clk_deinit();
		msm_dss_config_vreg(&pdev->dev, mp->vreg_config,
					mp->num_vreg, 0);
		kfree(dsi_io_private);
		dsi_io_private = NULL;
	}
}

int msm_dsi_clk_init(struct platform_device *dev)
{
	int rc = 0;

	dsi_io_private->dsi_clk = clk_get(&dev->dev, "dsi_clk");
	if (IS_ERR(dsi_io_private->dsi_clk)) {
		pr_err("can't find dsi core_clk\n");
		rc = PTR_ERR(dsi_io_private->dsi_clk);
		dsi_io_private->dsi_clk = NULL;
		return rc;
	}
	dsi_io_private->dsi_byte_clk = clk_get(&dev->dev, "byte_clk");
	if (IS_ERR(dsi_io_private->dsi_byte_clk)) {
		pr_err("can't find dsi byte_clk\n");
		rc = PTR_ERR(dsi_io_private->dsi_byte_clk);
		dsi_io_private->dsi_byte_clk = NULL;
		return rc;
	}

	dsi_io_private->dsi_esc_clk = clk_get(&dev->dev, "esc_clk");
	if (IS_ERR(dsi_io_private->dsi_esc_clk)) {
		pr_err("can't find dsi esc_clk\n");
		rc = PTR_ERR(dsi_io_private->dsi_esc_clk);
		dsi_io_private->dsi_esc_clk = NULL;
		return rc;
	}

	dsi_io_private->dsi_pixel_clk = clk_get(&dev->dev, "pixel_clk");
	if (IS_ERR(dsi_io_private->dsi_pixel_clk)) {
		pr_err("can't find dsi pixel\n");
		rc = PTR_ERR(dsi_io_private->dsi_pixel_clk);
		dsi_io_private->dsi_pixel_clk = NULL;
		return rc;
	}

	dsi_io_private->dsi_ahb_clk = clk_get(&dev->dev, "iface_clk");
	if (IS_ERR(dsi_io_private->dsi_ahb_clk)) {
		pr_err("can't find dsi iface_clk\n");
		rc = PTR_ERR(dsi_io_private->dsi_ahb_clk);
		dsi_io_private->dsi_ahb_clk = NULL;
		return rc;
	}
	clk_prepare(dsi_io_private->dsi_ahb_clk);

	return 0;
}

void msm_dsi_clk_deinit(void)
{
	if (dsi_io_private->dsi_clk) {
		clk_put(dsi_io_private->dsi_clk);
		dsi_io_private->dsi_clk = NULL;
	}
	if (dsi_io_private->dsi_byte_clk) {
		clk_put(dsi_io_private->dsi_byte_clk);
		dsi_io_private->dsi_byte_clk = NULL;
	}
	if (dsi_io_private->dsi_esc_clk) {
		clk_put(dsi_io_private->dsi_esc_clk);
		dsi_io_private->dsi_esc_clk = NULL;
	}
	if (dsi_io_private->dsi_pixel_clk) {
		clk_put(dsi_io_private->dsi_pixel_clk);
		dsi_io_private->dsi_pixel_clk = NULL;
	}
	if (dsi_io_private->dsi_ahb_clk) {
		clk_unprepare(dsi_io_private->dsi_ahb_clk);
		clk_put(dsi_io_private->dsi_ahb_clk);
		dsi_io_private->dsi_ahb_clk = NULL;
	}
}

int msm_dsi_prepare_clocks(void)
{
	clk_prepare(dsi_io_private->dsi_clk);
	clk_prepare(dsi_io_private->dsi_byte_clk);
	clk_prepare(dsi_io_private->dsi_esc_clk);
	clk_prepare(dsi_io_private->dsi_pixel_clk);
	return 0;
}

int msm_dsi_unprepare_clocks(void)
{
	clk_unprepare(dsi_io_private->dsi_clk);
	clk_unprepare(dsi_io_private->dsi_esc_clk);
	clk_unprepare(dsi_io_private->dsi_byte_clk);
	clk_unprepare(dsi_io_private->dsi_pixel_clk);
	return 0;
}

int msm_dsi_clk_set_rate(unsigned long esc_rate,
			unsigned long dsi_rate,
			unsigned long byte_rate,
			unsigned long pixel_rate)
{
	int rc;
	rc = clk_set_rate(dsi_io_private->dsi_clk, dsi_rate);
	if (rc) {
		pr_err("dsi_esc_clk - clk_set_rate failed =%d\n", rc);
		return rc;
	}

	rc = clk_set_rate(dsi_io_private->dsi_esc_clk, esc_rate);
	if (rc) {
		pr_err("dsi_esc_clk - clk_set_rate failed =%d\n", rc);
		return rc;
	}

	rc = clk_set_rate(dsi_io_private->dsi_byte_clk, byte_rate);
	if (rc) {
		pr_err("dsi_byte_clk - clk_set_rate faile = %dd\n", rc);
		return rc;
	}

	rc = clk_set_rate(dsi_io_private->dsi_pixel_clk, pixel_rate);
	if (rc) {
		pr_err("dsi_pixel_clk - clk_set_rate failed = %d\n", rc);
		return rc;
	}
	return 0;
}

int  msm_dsi_clk_enable(void)
{
	if (dsi_io_private->msm_dsi_clk_on) {
		pr_debug("dsi_clks on already\n");
		return 0;
	}

	clk_enable(dsi_io_private->dsi_clk);
	clk_enable(dsi_io_private->dsi_esc_clk);
	clk_enable(dsi_io_private->dsi_byte_clk);
	clk_enable(dsi_io_private->dsi_pixel_clk);

	dsi_io_private->msm_dsi_clk_on = 1;
	return 0;
}

int msm_dsi_clk_disable(void)
{
	if (dsi_io_private->msm_dsi_clk_on == 0) {
		pr_debug("mdss_dsi_clks already OFF\n");
		return 0;
	}

	clk_disable(dsi_io_private->dsi_clk);
	clk_disable(dsi_io_private->dsi_byte_clk);
	clk_disable(dsi_io_private->dsi_esc_clk);
	clk_disable(dsi_io_private->dsi_pixel_clk);

	dsi_io_private->msm_dsi_clk_on = 0;
	return 0;
}

static void msm_dsi_phy_strength_init(unsigned char *ctrl_base,
					struct mdss_dsi_phy_ctrl *pd)
{
	MIPI_OUTP(ctrl_base + DSI_DSIPHY_STRENGTH_CTRL_0, pd->strength[0]);
	MIPI_OUTP(ctrl_base + DSI_DSIPHY_STRENGTH_CTRL_2, pd->strength[1]);
}

static void msm_dsi_phy_ctrl_init(unsigned char *ctrl_base,
				struct mdss_panel_data *pdata)
{
	MIPI_OUTP(ctrl_base + DSI_DSIPHY_CTRL_0, 0x5f);
	MIPI_OUTP(ctrl_base + DSI_DSIPHY_CTRL_3, 0x10);
}

static void msm_dsi_phy_regulator_init(unsigned char *ctrl_base,
					struct mdss_dsi_phy_ctrl *pd)
{
	MIPI_OUTP(ctrl_base + DSI_DSIPHY_LDO_CNTRL, 0x25);
	MIPI_OUTP(ctrl_base + DSI_DSIPHY_REGULATOR_CTRL_0, pd->regulator[0]);
	MIPI_OUTP(ctrl_base + DSI_DSIPHY_REGULATOR_CTRL_1, pd->regulator[1]);
	MIPI_OUTP(ctrl_base + DSI_DSIPHY_REGULATOR_CTRL_2, pd->regulator[2]);
	MIPI_OUTP(ctrl_base + DSI_DSIPHY_REGULATOR_CTRL_3, pd->regulator[3]);
	MIPI_OUTP(ctrl_base + DSI_DSIPHY_REGULATOR_CTRL_4, pd->regulator[4]);
	MIPI_OUTP(ctrl_base + DSI_DSIPHY_REGULATOR_CAL_PWR_CFG,
			pd->regulator[5]);

}

static int msm_dsi_phy_calibration(unsigned char *ctrl_base)
{
	int i = 0, term_cnt = 5000, ret = 0, cal_busy;

	MIPI_OUTP(ctrl_base + DSI_DSIPHY_CAL_SW_CFG2, 0x0);
	MIPI_OUTP(ctrl_base + DSI_DSIPHY_CAL_HW_CFG1, 0x5a);
	MIPI_OUTP(ctrl_base + DSI_DSIPHY_CAL_HW_CFG3, 0x10);
	MIPI_OUTP(ctrl_base + DSI_DSIPHY_CAL_HW_CFG4, 0x01);
	MIPI_OUTP(ctrl_base + DSI_DSIPHY_CAL_HW_CFG0, 0x01);
	MIPI_OUTP(ctrl_base + DSI_DSIPHY_CAL_HW_TRIGGER, 0x01);
	usleep_range(5000, 5000); /*per DSI controller spec*/
	MIPI_OUTP(ctrl_base + DSI_DSIPHY_CAL_HW_TRIGGER, 0x00);

	cal_busy = MIPI_INP(ctrl_base + DSI_DSIPHY_REGULATOR_CAL_STATUS0);
	while (cal_busy & 0x10) {
		i++;
		if (i > term_cnt) {
			ret = -EINVAL;
			pr_err("msm_dsi_phy_calibration error\n");
			break;
		}
		cal_busy = MIPI_INP(ctrl_base +
					DSI_DSIPHY_REGULATOR_CAL_STATUS0);
	}

	return ret;
}

static void msm_dsi_phy_lane_init(unsigned char *ctrl_base,
			struct mdss_dsi_phy_ctrl *pd)
{
	int ln, index;

	/*CFG0, CFG1, CFG2, TEST_DATAPATH, TEST_STR0, TEST_STR1*/
	for (ln = 0; ln < 5; ln++) {
		unsigned char *off = ctrl_base + 0x0300 + (ln * 0x40);
		index = ln * 6;
		MIPI_OUTP(off, pd->lanecfg[index]);
		MIPI_OUTP(off + 4, pd->lanecfg[index + 1]);
		MIPI_OUTP(off + 8, pd->lanecfg[index + 2]);
		MIPI_OUTP(off + 12, pd->lanecfg[index + 3]);
		MIPI_OUTP(off + 20, pd->lanecfg[index + 4]);
		MIPI_OUTP(off + 24, pd->lanecfg[index + 5]);
	}
	wmb();
}

static void msm_dsi_phy_timing_init(unsigned char *ctrl_base,
			struct mdss_dsi_phy_ctrl *pd)
{
	int i, off = DSI_DSIPHY_TIMING_CTRL_0;
	for (i = 0; i < 12; i++) {
		MIPI_OUTP(ctrl_base + off, pd->timing[i]);
		off += 4;
	}
	wmb();
}

static void msm_dsi_phy_bist_init(unsigned char *ctrl_base,
			struct mdss_dsi_phy_ctrl *pd)
{
	MIPI_OUTP(ctrl_base + DSI_DSIPHY_BIST_CTRL4, pd->bistctrl[4]);
	MIPI_OUTP(ctrl_base + DSI_DSIPHY_BIST_CTRL1, pd->bistctrl[1]);
	MIPI_OUTP(ctrl_base + DSI_DSIPHY_BIST_CTRL0, pd->bistctrl[0]);
	MIPI_OUTP(ctrl_base + DSI_DSIPHY_BIST_CTRL4, 0);
	wmb();
}

int msm_dsi_phy_init(unsigned char *ctrl_base,
			struct mdss_panel_data *pdata)
{
	struct mdss_dsi_phy_ctrl *pd;

	pd = &(pdata->panel_info.mipi.dsi_phy_db);

	msm_dsi_phy_strength_init(ctrl_base, pd);

	msm_dsi_phy_ctrl_init(ctrl_base, pdata);

	msm_dsi_phy_regulator_init(ctrl_base, pd);

	msm_dsi_phy_calibration(ctrl_base);

	msm_dsi_phy_lane_init(ctrl_base, pd);

	msm_dsi_phy_timing_init(ctrl_base, pd);

	msm_dsi_phy_bist_init(ctrl_base, pd);

	return 0;
}

void msm_dsi_phy_sw_reset(unsigned char *ctrl_base)
{
	/* start phy sw reset */
	MIPI_OUTP(ctrl_base + DSI_PHY_SW_RESET, 0x0001);
	udelay(1000); /*per DSI controller spec*/
	wmb();
	/* end phy sw reset */
	MIPI_OUTP(ctrl_base + DSI_PHY_SW_RESET, 0x0000);
	udelay(100); /*per DSI controller spec*/
	wmb();
}

void msm_dsi_phy_off(unsigned char *ctrl_base)
{
	MIPI_OUTP(ctrl_base + DSI_DSIPHY_PLL_CTRL_5, 0x05f);
	MIPI_OUTP(ctrl_base + DSI_DSIPHY_REGULATOR_CTRL_0, 0x02);
	MIPI_OUTP(ctrl_base + DSI_DSIPHY_CTRL_0, 0x00);
	MIPI_OUTP(ctrl_base + DSI_DSIPHY_CTRL_1, 0x7f);
	MIPI_OUTP(ctrl_base + DSI_CLK_CTRL, 0);
}
