/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/iopoll.h>
#include <linux/delay.h>
#include <linux/clk/msm-clock-generic.h>

#include "mdss-pll.h"
#include "mdss-dsi-pll.h"

#define MMSS_DSI_PHY_PLL_SYS_CLK_CTRL			0x0000
#define MMSS_DSI_PHY_PLL_PLL_VCOTAIL_EN			0x0004
#define MMSS_DSI_PHY_PLL_CMN_MODE			0x0008
#define MMSS_DSI_PHY_PLL_IE_TRIM			0x000C
#define MMSS_DSI_PHY_PLL_IP_TRIM			0x0010

#define MMSS_DSI_PHY_PLL_PLL_PHSEL_CONTROL		0x0018
#define MMSS_DSI_PHY_PLL_IPTAT_TRIM_VCCA_TX_SEL		0x001C
#define MMSS_DSI_PHY_PLL_PLL_PHSEL_DC			0x0020
#define MMSS_DSI_PHY_PLL_PLL_IP_SETI			0x0024
#define MMSS_DSI_PHY_PLL_CORE_CLK_IN_SYNC_SEL		0x0028

#define MMSS_DSI_PHY_PLL_BIAS_EN_CLKBUFLR_EN		0x0030
#define MMSS_DSI_PHY_PLL_PLL_CP_SETI			0x0034
#define MMSS_DSI_PHY_PLL_PLL_IP_SETP			0x0038
#define MMSS_DSI_PHY_PLL_PLL_CP_SETP			0x003C
#define MMSS_DSI_PHY_PLL_ATB_SEL1			0x0040
#define MMSS_DSI_PHY_PLL_ATB_SEL2			0x0044
#define MMSS_DSI_PHY_PLL_SYSCLK_EN_SEL_TXBAND		0x0048
#define MMSS_DSI_PHY_PLL_RESETSM_CNTRL			0x004C
#define MMSS_DSI_PHY_PLL_RESETSM_CNTRL2			0x0050
#define MMSS_DSI_PHY_PLL_RESETSM_CNTRL3			0x0054
#define MMSS_DSI_PHY_PLL_RESETSM_PLL_CAL_COUNT1		0x0058
#define MMSS_DSI_PHY_PLL_RESETSM_PLL_CAL_COUNT2		0x005C
#define MMSS_DSI_PHY_PLL_DIV_REF1			0x0060
#define MMSS_DSI_PHY_PLL_DIV_REF2			0x0064
#define MMSS_DSI_PHY_PLL_KVCO_COUNT1			0x0068
#define MMSS_DSI_PHY_PLL_KVCO_COUNT2			0x006C
#define MMSS_DSI_PHY_PLL_KVCO_CAL_CNTRL			0x0070
#define MMSS_DSI_PHY_PLL_KVCO_CODE			0x0074
#define MMSS_DSI_PHY_PLL_VREF_CFG1			0x0078
#define MMSS_DSI_PHY_PLL_VREF_CFG2			0x007C
#define MMSS_DSI_PHY_PLL_VREF_CFG3			0x0080
#define MMSS_DSI_PHY_PLL_VREF_CFG4			0x0084
#define MMSS_DSI_PHY_PLL_VREF_CFG5			0x0088
#define MMSS_DSI_PHY_PLL_VREF_CFG6			0x008C
#define MMSS_DSI_PHY_PLL_PLLLOCK_CMP1			0x0090
#define MMSS_DSI_PHY_PLL_PLLLOCK_CMP2			0x0094
#define MMSS_DSI_PHY_PLL_PLLLOCK_CMP3			0x0098

#define MMSS_DSI_PHY_PLL_BGTC				0x00A0
#define MMSS_DSI_PHY_PLL_PLL_TEST_UPDN			0x00A4
#define MMSS_DSI_PHY_PLL_PLL_VCO_TUNE			0x00A8
#define MMSS_DSI_PHY_PLL_DEC_START1			0x00AC
#define MMSS_DSI_PHY_PLL_PLL_AMP_OS			0x00B0
#define MMSS_DSI_PHY_PLL_SSC_EN_CENTER			0x00B4
#define MMSS_DSI_PHY_PLL_SSC_ADJ_PER1			0x00B8
#define MMSS_DSI_PHY_PLL_SSC_ADJ_PER2			0x00BC
#define MMSS_DSI_PHY_PLL_SSC_PER1			0x00C0
#define MMSS_DSI_PHY_PLL_SSC_PER2			0x00C4
#define MMSS_DSI_PHY_PLL_SSC_STEP_SIZE1			0x00C8
#define MMSS_DSI_PHY_PLL_SSC_STEP_SIZE2			0x00CC
#define MMSS_DSI_PHY_PLL_RES_CODE_UP			0x00D0
#define MMSS_DSI_PHY_PLL_RES_CODE_DN			0x00D4
#define MMSS_DSI_PHY_PLL_RES_CODE_UP_OFFSET		0x00D8
#define MMSS_DSI_PHY_PLL_RES_CODE_DN_OFFSET		0x00DC
#define MMSS_DSI_PHY_PLL_RES_CODE_START_SEG1		0x00E0
#define MMSS_DSI_PHY_PLL_RES_CODE_START_SEG2		0x00E4
#define MMSS_DSI_PHY_PLL_RES_CODE_CAL_CSR		0x00E8
#define MMSS_DSI_PHY_PLL_RES_CODE			0x00EC
#define MMSS_DSI_PHY_PLL_RES_TRIM_CONTROL		0x00F0
#define MMSS_DSI_PHY_PLL_RES_TRIM_CONTROL2		0x00F4
#define MMSS_DSI_PHY_PLL_RES_TRIM_EN_VCOCALDONE		0x00F8
#define MMSS_DSI_PHY_PLL_FAUX_EN			0x00FC

#define MMSS_DSI_PHY_PLL_DIV_FRAC_START1		0x0100
#define MMSS_DSI_PHY_PLL_DIV_FRAC_START2		0x0104
#define MMSS_DSI_PHY_PLL_DIV_FRAC_START3		0x0108
#define MMSS_DSI_PHY_PLL_DEC_START2			0x010C
#define MMSS_DSI_PHY_PLL_PLL_RXTXEPCLK_EN		0x0110
#define MMSS_DSI_PHY_PLL_PLL_CRCTRL			0x0114
#define MMSS_DSI_PHY_PLL_LOW_POWER_RO_CONTROL		0x013C
#define MMSS_DSI_PHY_PLL_POST_DIVIDER_CONTROL		0x0140
#define MMSS_DSI_PHY_PLL_HR_OCLK2_DIVIDER		0x0144
#define MMSS_DSI_PHY_PLL_HR_OCLK3_DIVIDER		0x0148
#define MMSS_DSI_PHY_PLL_PLL_VCO_HIGH			0x014C
#define MMSS_DSI_PHY_PLL_RESET_SM			0x0150
#define MMSS_DSI_PHY_PLL_MUXVAL			0x0154
#define MMSS_DSI_PHY_PLL_CORE_RES_CODE_DN		0x0158
#define MMSS_DSI_PHY_PLL_CORE_RES_CODE_UP		0x015C
#define MMSS_DSI_PHY_PLL_CORE_VCO_TUNE			0x0160
#define MMSS_DSI_PHY_PLL_CORE_VCO_TAIL			0x0164
#define MMSS_DSI_PHY_PLL_CORE_KVCO_CODE		0x0168

#define MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL0		0x014
#define MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL1		0x018
#define MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL2		0x01C
#define MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL3		0x020
#define MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL4		0x024
#define MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL5		0x028
#define MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL6		0x02C
#define MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL7		0x030
#define MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL8		0x034
#define MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL9		0x038
#define MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL10		0x03C
#define MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL11		0x040
#define MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL12		0x044
#define MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL13		0x048
#define MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL14		0x04C
#define MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL15		0x050
#define MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL16		0x054
#define MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL17		0x058

#define DSI_PLL_POLL_MAX_READS			15
#define DSI_PLL_POLL_TIMEOUT_US			1000

int set_mdss_byte_mux_sel(struct mux_clk *clk, int sel)
{
	return 0;
}

int get_mdss_byte_mux_sel(struct mux_clk *clk)
{
	return 0;
}

int set_mdss_pixel_mux_sel(struct mux_clk *clk, int sel)
{
	return 0;
}

int get_mdss_pixel_mux_sel(struct mux_clk *clk)
{
	return 0;
}

static void pll_20nm_cache_trim_codes(struct mdss_pll_resources *dsi_pll_res)
{
	int rc;

	rc = mdss_pll_resource_enable(dsi_pll_res, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return;
	}

	dsi_pll_res->cache_pll_trim_codes[0] =
		MDSS_PLL_REG_R(dsi_pll_res->pll_base,
			MMSS_DSI_PHY_PLL_CORE_KVCO_CODE);
	dsi_pll_res->cache_pll_trim_codes[1] =
		MDSS_PLL_REG_R(dsi_pll_res->pll_base,
			MMSS_DSI_PHY_PLL_CORE_VCO_TUNE);

	pr_debug("core_kvco_code=0x%x core_vco_turn=0x%x\n",
		dsi_pll_res->cache_pll_trim_codes[0],
		dsi_pll_res->cache_pll_trim_codes[1]);

	mdss_pll_resource_enable(dsi_pll_res, false);
}

static int pll_20nm_read_codes_stored(struct mdss_pll_resources *dsi_pll_res,
	s64 vco_clk_rate)
{
	int i;
	int rc = 0;
	bool found = false;

	if (!dsi_pll_res->dfps) {
		rc = -EINVAL;
		goto pll_codes;
	}

	for (i = 0; i < dsi_pll_res->dfps->panel_dfps.frame_rate_cnt; i++) {
		struct dfps_codes_info *codes_info =
			&dsi_pll_res->dfps->codes_dfps[i];

		pr_debug("valid=%d frame_rate=%d, vco_rate=%d, code %d %d\n",
			codes_info->is_valid, codes_info->frame_rate,
			codes_info->clk_rate, codes_info->pll_codes.pll_codes_1,
			codes_info->pll_codes.pll_codes_2);

		if (vco_clk_rate != codes_info->clk_rate &&
			codes_info->is_valid)
			continue;

		dsi_pll_res->cache_pll_trim_codes[0] =
			codes_info->pll_codes.pll_codes_1;
		dsi_pll_res->cache_pll_trim_codes[1] =
			codes_info->pll_codes.pll_codes_2;
		found = true;

		break;
	}

	if (!found)
		rc = -EINVAL;

pll_codes:
	pr_debug("core_kvco_code=0x%x core_vco_turn=0x%x\n",
		dsi_pll_res->cache_pll_trim_codes[0],
		dsi_pll_res->cache_pll_trim_codes[1]);

	return rc;
}

static void pll_20nm_override_trim_codes(struct mdss_pll_resources *dsi_pll_res)
{
	u32 reg_data;
	void __iomem *pll_base = dsi_pll_res->pll_base;

	/*
	 * Override mux config for all cached trim codes from
	 * saved config except for VCO Tune
	 */
	reg_data = (dsi_pll_res->cache_pll_trim_codes[0] & 0x3f) | BIT(5);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_KVCO_CODE, reg_data);

	reg_data = (dsi_pll_res->cache_pll_trim_codes[1] & 0x7f) | BIT(7);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_PLL_VCO_TUNE, reg_data);
}


int set_bypass_lp_div_mux_sel(struct mux_clk *clk, int sel)
{
	struct mdss_pll_resources *dsi_pll_res = clk->priv;
	int reg_data;

	pr_debug("bypass_lp_div mux set to %s mode\n",
				sel ? "indirect" : "direct");

	pr_debug("POST_DIVIDER_CONTROL = 0x%x\n",
		MDSS_PLL_REG_R(dsi_pll_res->pll_base,
			MMSS_DSI_PHY_PLL_POST_DIVIDER_CONTROL));

	reg_data = MDSS_PLL_REG_R(dsi_pll_res->pll_base,
			MMSS_DSI_PHY_PLL_POST_DIVIDER_CONTROL);
	reg_data |= BIT(7);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
			MMSS_DSI_PHY_PLL_POST_DIVIDER_CONTROL,
			reg_data | (sel << 5));
	pr_debug("POST_DIVIDER_CONTROL = 0x%x\n",
		MDSS_PLL_REG_R(dsi_pll_res->pll_base,
			MMSS_DSI_PHY_PLL_POST_DIVIDER_CONTROL));

	return 0;
}

int set_shadow_bypass_lp_div_mux_sel(struct mux_clk *clk, int sel)
{
	struct mdss_pll_resources *dsi_pll_res = clk->priv;
	int reg_data, rem;

	if (!dsi_pll_res->resource_enable) {
		pr_err("PLL resources disabled. Dynamic fps invalid\n");
		return -EINVAL;
	}

	reg_data = MDSS_PLL_REG_R(dsi_pll_res->pll_base,
			MMSS_DSI_PHY_PLL_POST_DIVIDER_CONTROL);
	reg_data |= BIT(7);

	pr_debug("%d: reg_data = %x\n", __LINE__, reg_data);

	/* Repeat POST DIVIDER 2 times (4 writes)*/
	for (rem = 0; rem < 2; rem++)
		MDSS_DYN_PLL_REG_W(dsi_pll_res->dyn_pll_base,
			MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL16 + (4 * rem),
			MMSS_DSI_PHY_PLL_POST_DIVIDER_CONTROL,
			MMSS_DSI_PHY_PLL_POST_DIVIDER_CONTROL,
			(reg_data | (sel << 5)), (reg_data | (sel << 5)));

	return 0;
}

int get_bypass_lp_div_mux_sel(struct mux_clk *clk)
{
	int mux_mode, rc;
	struct mdss_pll_resources *dsi_pll_res = clk->priv;

	if (is_gdsc_disabled(dsi_pll_res))
		return 0;

	rc = mdss_pll_resource_enable(dsi_pll_res, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	mux_mode = MDSS_PLL_REG_R(dsi_pll_res->pll_base,
			MMSS_DSI_PHY_PLL_POST_DIVIDER_CONTROL) & BIT(5);

	pr_debug("bypass_lp_div mux mode = %s",
				mux_mode ? "indirect" : "direct");
	mdss_pll_resource_enable(dsi_pll_res, false);

	return !!mux_mode;
}

int ndiv_set_div(struct div_clk *clk, int div)
{
	int rc, reg_data;
	struct mdss_pll_resources *dsi_pll_res = clk->priv;

	rc = mdss_pll_resource_enable(dsi_pll_res, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	reg_data = MDSS_PLL_REG_R(dsi_pll_res->pll_base,
			MMSS_DSI_PHY_PLL_POST_DIVIDER_CONTROL);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_POST_DIVIDER_CONTROL,
				reg_data | div);

	pr_debug("POST_DIVIDER_CONTROL = 0x%x\n",
		MDSS_PLL_REG_R(dsi_pll_res->pll_base,
			MMSS_DSI_PHY_PLL_POST_DIVIDER_CONTROL));

	mdss_pll_resource_enable(dsi_pll_res, false);
	return rc;
}

int shadow_ndiv_set_div(struct div_clk *clk, int div)
{
	struct mdss_pll_resources *dsi_pll_res = clk->priv;

	if (!dsi_pll_res->resource_enable) {
		pr_err("PLL resources disabled. Dynamic fps invalid\n");
		return -EINVAL;
	}

	pr_debug("%d div=%i\n", __LINE__, div);

	MDSS_DYN_PLL_REG_W(dsi_pll_res->dyn_pll_base,
		MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL10,
		MMSS_DSI_PHY_PLL_POST_DIVIDER_CONTROL,
		MMSS_DSI_PHY_PLL_POST_DIVIDER_CONTROL,
		(0xB | div), (0xB | div));

	return 0;
}

int ndiv_get_div(struct div_clk *clk)
{
	int div = 0, rc;
	struct mdss_pll_resources *dsi_pll_res = clk->priv;

	if (is_gdsc_disabled(dsi_pll_res))
		return 0;

	rc = mdss_pll_resource_enable(clk->priv, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	div = MDSS_PLL_REG_R(dsi_pll_res->pll_base,
		MMSS_DSI_PHY_PLL_POST_DIVIDER_CONTROL) & 0x0F;

	mdss_pll_resource_enable(dsi_pll_res, false);

	return div;
}

int fixed_hr_oclk2_set_div(struct div_clk *clk, int div)
{
	int rc;
	struct mdss_pll_resources *dsi_pll_res = clk->priv;

	rc = mdss_pll_resource_enable(dsi_pll_res, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_HR_OCLK2_DIVIDER,
				(div - 1));

	mdss_pll_resource_enable(dsi_pll_res, false);
	return rc;
}

int shadow_fixed_hr_oclk2_set_div(struct div_clk *clk, int div)
{
	struct mdss_pll_resources *dsi_pll_res = clk->priv;

	if (!dsi_pll_res->resource_enable) {
		pr_err("PLL resources disabled. Dynamic fps invalid\n");
		return -EINVAL;
	}
	pr_debug("%d div = %d\n", __LINE__, div);

	MDSS_DYN_PLL_REG_W(dsi_pll_res->dyn_pll_base,
		MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL9,
		MMSS_DSI_PHY_PLL_HR_OCLK2_DIVIDER,
		MMSS_DSI_PHY_PLL_HR_OCLK2_DIVIDER,
		(div - 1), (div - 1));

	return 0;
}

int fixed_hr_oclk2_get_div(struct div_clk *clk)
{
	int div = 0, rc;
	struct mdss_pll_resources *dsi_pll_res = clk->priv;

	if (is_gdsc_disabled(dsi_pll_res))
		return 0;

	rc = mdss_pll_resource_enable(dsi_pll_res, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	div = MDSS_PLL_REG_R(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_HR_OCLK2_DIVIDER);

	mdss_pll_resource_enable(dsi_pll_res, false);
	return div + 1;
}

int hr_oclk3_set_div(struct div_clk *clk, int div)
{
	int rc;
	struct mdss_pll_resources *dsi_pll_res = clk->priv;

	rc = mdss_pll_resource_enable(dsi_pll_res, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	pr_debug("%d div = %d\n", __LINE__, div);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_HR_OCLK3_DIVIDER,
				(div - 1));
	pr_debug("%s: HR_OCLK3_DIVIDER = 0x%x\n", __func__,
		MDSS_PLL_REG_R(dsi_pll_res->pll_base,
			MMSS_DSI_PHY_PLL_HR_OCLK3_DIVIDER));

	mdss_pll_resource_enable(dsi_pll_res, false);
	return rc;
}

int shadow_hr_oclk3_set_div(struct div_clk *clk, int div)
{
	struct mdss_pll_resources *dsi_pll_res = clk->priv;

	if (!dsi_pll_res->resource_enable) {
		pr_err("PLL resources disabled. Dynamic fps invalid\n");
		return -EINVAL;
	}

	pr_debug("%d div = %d\n", __LINE__, div);

	MDSS_DYN_PLL_REG_W(dsi_pll_res->dyn_pll_base,
		MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL8,
		MMSS_DSI_PHY_PLL_HR_OCLK3_DIVIDER,
		MMSS_DSI_PHY_PLL_HR_OCLK3_DIVIDER,
		(div - 1), (div - 1));

	return 0;
}

int hr_oclk3_get_div(struct div_clk *clk)
{
	int div = 0, rc;
	struct mdss_pll_resources *dsi_pll_res = clk->priv;

	if (is_gdsc_disabled(dsi_pll_res))
		return 0;

	rc = mdss_pll_resource_enable(dsi_pll_res, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	div = MDSS_PLL_REG_R(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_HR_OCLK3_DIVIDER);

	mdss_pll_resource_enable(dsi_pll_res, false);
	return div + 1;
}

static bool pll_20nm_is_pll_locked(struct mdss_pll_resources *dsi_pll_res)
{
	u32 status;
	bool pll_locked;

	/* poll for PLL ready status */
	if (readl_poll_timeout_noirq((dsi_pll_res->pll_base +
			MMSS_DSI_PHY_PLL_RESET_SM),
			status,
			((status & BIT(5)) > 0),
			DSI_PLL_POLL_MAX_READS,
			DSI_PLL_POLL_TIMEOUT_US)) {
		pr_debug("DSI PLL status=%x failed to Lock\n", status);
		pll_locked = false;
	} else if (readl_poll_timeout_noirq((dsi_pll_res->pll_base +
				MMSS_DSI_PHY_PLL_RESET_SM),
				status,
				((status & BIT(6)) > 0),
				DSI_PLL_POLL_MAX_READS,
				DSI_PLL_POLL_TIMEOUT_US)) {
		pr_debug("DSI PLL status=%x PLl not ready\n", status);
		pll_locked = false;
	} else {
		pll_locked = true;
	}

	return pll_locked;
}

void pll_20nm_config_powerdown(void __iomem *pll_base)
{
	if (!pll_base) {
		pr_err("Invalid pll base.\n");
		return;
	}

	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_SYS_CLK_CTRL, 0x00);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_CMN_MODE, 0x01);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_PLL_VCOTAIL_EN, 0x82);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_BIAS_EN_CLKBUFLR_EN, 0x02);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_RESETSM_CNTRL3, 0x06);
}

static int dsi_pll_enable(struct clk *c)
{
	int i, rc;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *dsi_pll_res = vco->priv;

	rc = mdss_pll_resource_enable(dsi_pll_res, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	/* Try all enable sequences until one succeeds */
	for (i = 0; i < vco->pll_en_seq_cnt; i++) {
		rc = vco->pll_enable_seqs[i](dsi_pll_res);
		pr_debug("DSI PLL %s after sequence #%d\n",
			rc ? "unlocked" : "locked", i + 1);
		if (!rc)
			break;
	}
	/* Disable PLL1 to avoid current leakage while toggling MDSS GDSC */
	if (dsi_pll_res->pll_1_base)
		pll_20nm_config_powerdown(dsi_pll_res->pll_1_base);

	if (rc) {
		mdss_pll_resource_enable(dsi_pll_res, false);
		pr_err("DSI PLL failed to lock\n");
	}
	dsi_pll_res->pll_on = true;

	return rc;
}

static void dsi_pll_disable(struct clk *c)
{
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *dsi_pll_res = vco->priv;

	if (!dsi_pll_res->pll_on &&
		mdss_pll_resource_enable(dsi_pll_res, true)) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return;
	}

	dsi_pll_res->handoff_resources = false;

	/* Disable PLL1 to avoid current leakage while toggling MDSS GDSC */
	if (dsi_pll_res->pll_1_base)
		pll_20nm_config_powerdown(dsi_pll_res->pll_1_base);

	pll_20nm_config_powerdown(dsi_pll_res->pll_base);

	mdss_pll_resource_enable(dsi_pll_res, false);

	dsi_pll_res->pll_on = false;

	pr_debug("DSI PLL Disabled\n");
	return;
}

static void pll_20nm_config_common_block_1(void __iomem *pll_base)
{
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_PLL_VCOTAIL_EN, 0x82);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_BIAS_EN_CLKBUFLR_EN, 0x2a);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_BIAS_EN_CLKBUFLR_EN, 0x2b);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_RESETSM_CNTRL3, 0x02);
}

static void pll_20nm_config_common_block_2(void __iomem *pll_base)
{
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_SYS_CLK_CTRL, 0x40);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_IE_TRIM, 0x0F);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_IP_TRIM, 0x0F);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_PLL_PHSEL_CONTROL, 0x08);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_IPTAT_TRIM_VCCA_TX_SEL, 0x0E);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_PLL_BKG_KVCO_CAL_EN, 0x08);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_SYSCLK_EN_SEL_TXBAND, 0x4A);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_DIV_REF1, 0x00);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_DIV_REF2, 0x01);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_PLL_CNTRL, 0x07);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_KVCO_CAL_CNTRL, 0x1f);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_KVCO_COUNT1, 0x8A);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_VREF_CFG3, 0x10);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_SSC_EN_CENTER, 0x00);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_FAUX_EN, 0x0C);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_PLL_RXTXEPCLK_EN, 0x0a);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_LOW_POWER_RO_CONTROL, 0x0f);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_CMN_MODE, 0x00);
}

static void pll_20nm_config_loop_bw(void __iomem *pll_base)
{
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_PLL_IP_SETI, 0x03);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_PLL_CP_SETI, 0x3F);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_PLL_IP_SETP, 0x03);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_PLL_CP_SETP, 0x1F);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_PLL_CRCTRL, 0x77);
}

static void pll_20nm_vco_rate_calc(struct mdss_pll_vco_calc *vco_calc,
		s64 vco_clk_rate, s64 ref_clk_rate, bool pll_en_90_phase)
{
	s64 multiplier = (1 << 20);
	s64 duration, pll_comp_val;
	s64 dec_start_multiple, dec_start;
	s32 div_frac_start;
	s64 dec_start1, dec_start2;
	s32 div_frac_start1, div_frac_start2, div_frac_start3;
	s64 pll_plllock_cmp1, pll_plllock_cmp2, pll_plllock_cmp3;

	if (pll_en_90_phase)
		duration = 128;
	else
		duration = 1024;

	memset(vco_calc, 0, sizeof(*vco_calc));
	pr_debug("vco_clk_rate=%lld ref_clk_rate=%lld\n", vco_clk_rate,
		ref_clk_rate);

	dec_start_multiple = div_s64(vco_clk_rate * multiplier,
					2 * ref_clk_rate);
	div_s64_rem(dec_start_multiple,
			multiplier, &div_frac_start);

	dec_start = div_s64(dec_start_multiple, multiplier);
	dec_start1 = (dec_start & 0x7f) | BIT(7);
	dec_start2 = ((dec_start & 0x80) >> 7) | BIT(1);
	div_frac_start1 = (div_frac_start & 0x7f) | BIT(7);
	div_frac_start2 = ((div_frac_start >> 7) & 0x7f) | BIT(7);
	div_frac_start3 = ((div_frac_start >> 14) & 0x3f) | BIT(6);
	if (pll_en_90_phase)
		pll_comp_val = div_s64(dec_start_multiple * 2 * (duration - 1),
				10 * multiplier);
	else
		pll_comp_val = (div_s64(dec_start_multiple * 2 * duration,
				10 * multiplier)) - 1;
	pll_plllock_cmp1 = pll_comp_val & 0xff;
	pll_plllock_cmp2 = (pll_comp_val >> 8) & 0xff;
	pll_plllock_cmp3 = (pll_comp_val >> 16) & 0xff;

	pr_debug("dec_start_multiple = 0x%llx\n", dec_start_multiple);
	pr_debug("dec_start = 0x%llx, div_frac_start = 0x%x\n",
			dec_start, div_frac_start);
	pr_debug("dec_start1 = 0x%llx, dec_start2 = 0x%llx\n",
			dec_start1, dec_start2);
	pr_debug("div_frac_start1 = 0x%x, div_frac_start2 = 0x%x\n",
			div_frac_start1, div_frac_start2);
	pr_debug("div_frac_start3 = 0x%x\n", div_frac_start3);
	pr_debug("pll_comp_val = 0x%llx\n", pll_comp_val);
	pr_debug("pll_plllock_cmp1 = 0x%llx, pll_plllock_cmp2 =%llx\n",
			pll_plllock_cmp1, pll_plllock_cmp2);
	pr_debug("pll_plllock_cmp3 = 0x%llx\n",	pll_plllock_cmp3);

	/* Assign to vco struct */
	vco_calc->div_frac_start1 = div_frac_start1;
	vco_calc->div_frac_start2 = div_frac_start2;
	vco_calc->div_frac_start3 = div_frac_start3;
	vco_calc->dec_start1 = dec_start1;
	vco_calc->dec_start2 = dec_start2;
	vco_calc->pll_plllock_cmp1 = pll_plllock_cmp1;
	vco_calc->pll_plllock_cmp2 = pll_plllock_cmp2;
	vco_calc->pll_plllock_cmp3 = pll_plllock_cmp3;
}

static void pll_20nm_config_vco_rate(void __iomem *pll_base,
		struct mdss_pll_vco_calc *vco_calc, bool pll_en_90_phase)
{
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_DIV_FRAC_START1,
		vco_calc->div_frac_start1);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_DIV_FRAC_START2,
		vco_calc->div_frac_start2);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_DIV_FRAC_START3,
		vco_calc->div_frac_start3);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_DEC_START1,
		vco_calc->dec_start1);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_DEC_START2,
		vco_calc->dec_start2);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_PLLLOCK_CMP1,
		vco_calc->pll_plllock_cmp1);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_PLLLOCK_CMP2,
		vco_calc->pll_plllock_cmp2);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_PLLLOCK_CMP3,
		vco_calc->pll_plllock_cmp3);
	if (pll_en_90_phase)
		MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_PLLLOCK_CMP_EN, 0x0d);
	else
		MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_PLLLOCK_CMP_EN, 0x01);
}

int pll_20nm_vco_set_rate(struct dsi_pll_vco_clk *vco, unsigned long rate)
{
	struct mdss_pll_resources *dsi_pll_res = vco->priv;

	dsi_pll_res->vco_current_rate = rate;
	dsi_pll_res->vco_ref_clk_rate = vco->ref_clk_rate;

	return 0;
}

int shadow_pll_20nm_vco_set_rate(struct dsi_pll_vco_clk *vco,
		unsigned long rate)
{
	struct mdss_pll_resources *dsi_pll_res = vco->priv;
	struct mdss_pll_vco_calc vco_calc;
	s64 vco_clk_rate = (s64)rate;
	u32 rem;

	if (!dsi_pll_res->resource_enable) {
		pr_err("PLL resources disabled. Dynamic fps invalid\n");
		return -EINVAL;
	}

	if (pll_20nm_read_codes_stored(dsi_pll_res, vco_clk_rate)) {
		pr_err("cannot find pll codes rate=%lld\n", vco_clk_rate);
		return -EINVAL;
	}

	/* div fraction, start and comp calculations */
	pll_20nm_vco_rate_calc(&vco_calc, vco_clk_rate,
			dsi_pll_res->vco_ref_clk_rate,
			dsi_pll_res->pll_en_90_phase);

	MDSS_DYN_PLL_REG_W(dsi_pll_res->dyn_pll_base,
		MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL0,
		MMSS_DSI_PHY_PLL_POST_DIVIDER_CONTROL,
		MMSS_DSI_PHY_PLL_POST_DIVIDER_CONTROL,
		0x90, 0x90);

	MDSS_DYN_PLL_REG_W(dsi_pll_res->dyn_pll_base,
		MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL1,
		MMSS_DSI_PHY_PLL_RESETSM_CNTRL3,
		MMSS_DSI_PHY_PLL_RESETSM_CNTRL3,
		0x06, 0x02);

	MDSS_DYN_PLL_REG_W(dsi_pll_res->dyn_pll_base,
		MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL2,
		MMSS_DSI_PHY_PLL_PLLLOCK_CMP1,
		MMSS_DSI_PHY_PLL_PLLLOCK_CMP2,
		vco_calc.pll_plllock_cmp1, vco_calc.pll_plllock_cmp2);

	MDSS_DYN_PLL_REG_W(dsi_pll_res->dyn_pll_base,
		MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL3,
		MMSS_DSI_PHY_PLL_PLLLOCK_CMP3,
		MMSS_DSI_PHY_PLL_PLLLOCK_CMP3,
		vco_calc.pll_plllock_cmp3, vco_calc.pll_plllock_cmp3);

	MDSS_DYN_PLL_REG_W(dsi_pll_res->dyn_pll_base,
		MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL4,
		MMSS_DSI_PHY_PLL_DEC_START1,
		MMSS_DSI_PHY_PLL_DEC_START2,
		vco_calc.dec_start1, vco_calc.dec_start2);

	MDSS_DYN_PLL_REG_W(dsi_pll_res->dyn_pll_base,
		MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL5,
		MMSS_DSI_PHY_PLL_DIV_FRAC_START1,
		MMSS_DSI_PHY_PLL_DIV_FRAC_START2,
		vco_calc.div_frac_start1, vco_calc.div_frac_start2);

	MDSS_DYN_PLL_REG_W(dsi_pll_res->dyn_pll_base,
		MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL6,
		MMSS_DSI_PHY_PLL_DIV_FRAC_START3,
		MMSS_DSI_PHY_PLL_DIV_FRAC_START3,
		vco_calc.div_frac_start3, vco_calc.div_frac_start3);

	MDSS_DYN_PLL_REG_W(dsi_pll_res->dyn_pll_base,
		MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL7,
		MMSS_DSI_PHY_PLL_KVCO_CODE,
		MMSS_DSI_PHY_PLL_PLL_VCO_TUNE,
		((dsi_pll_res->cache_pll_trim_codes[0] & 0x3f) | BIT(5)),
		((dsi_pll_res->cache_pll_trim_codes[1] & 0x7f) | BIT(7)));

	/* fill other dfps registers with resetsm_ctrl3 = 0x2 */
	for (rem = MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL10; rem <=
		     MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL14; rem += 4)
		MDSS_DYN_PLL_REG_W(dsi_pll_res->dyn_pll_base, rem,
			MMSS_DSI_PHY_PLL_RESETSM_CNTRL3,
			MMSS_DSI_PHY_PLL_RESETSM_CNTRL3,
			0x02, 0x02);

	MDSS_DYN_PLL_REG_W(dsi_pll_res->dyn_pll_base,
		MMSS_DSI_DYNAMIC_REFRESH_PLL_CTRL15,
		MMSS_DSI_PHY_PLL_RESETSM_CNTRL3,
		MMSS_DSI_PHY_PLL_RESETSM_CNTRL3,
		0x02, 0x03);

	wmb();
	return 0;
}

unsigned long pll_20nm_vco_get_rate(struct clk *c)
{
	u64 vco_rate, multiplier = (1 << 20);
	s32 div_frac_start;
	u32 dec_start;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	u64 ref_clk = vco->ref_clk_rate;
	int rc;
	struct mdss_pll_resources *dsi_pll_res = vco->priv;

	if (is_gdsc_disabled(dsi_pll_res))
		return 0;

	rc = mdss_pll_resource_enable(dsi_pll_res, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	dec_start = (MDSS_PLL_REG_R(dsi_pll_res->pll_base,
			MMSS_DSI_PHY_PLL_DEC_START2) & BIT(0)) << 7;
	dec_start |= (MDSS_PLL_REG_R(dsi_pll_res->pll_base,
			MMSS_DSI_PHY_PLL_DEC_START1) & 0x7f);
	pr_debug("dec_start = 0x%x\n", dec_start);

	div_frac_start = (MDSS_PLL_REG_R(dsi_pll_res->pll_base,
			MMSS_DSI_PHY_PLL_DIV_FRAC_START3) & 0x3f) << 14;
	div_frac_start |= (MDSS_PLL_REG_R(dsi_pll_res->pll_base,
			MMSS_DSI_PHY_PLL_DIV_FRAC_START2) & 0x7f) << 7;
	div_frac_start |= MDSS_PLL_REG_R(dsi_pll_res->pll_base,
			MMSS_DSI_PHY_PLL_DIV_FRAC_START1) & 0x7f;
	pr_debug("div_frac_start = 0x%x\n", div_frac_start);

	vco_rate = ref_clk * 2 * dec_start;
	vco_rate += ((ref_clk * 2 * div_frac_start) / multiplier);
	pr_debug("returning vco rate = %lu\n", (unsigned long)vco_rate);

	mdss_pll_resource_enable(dsi_pll_res, false);

	return (unsigned long)vco_rate;
}
long pll_20nm_vco_round_rate(struct clk *c, unsigned long rate)
{
	unsigned long rrate = rate;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);

	if (rate < vco->min_rate)
		rrate = vco->min_rate;
	if (rate > vco->max_rate)
		rrate = vco->max_rate;

	return rrate;
}

enum handoff pll_20nm_vco_handoff(struct clk *c)
{
	int rc;
	enum handoff ret = HANDOFF_DISABLED_CLK;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *dsi_pll_res = vco->priv;

	if (is_gdsc_disabled(dsi_pll_res))
		return HANDOFF_DISABLED_CLK;

	rc = mdss_pll_resource_enable(dsi_pll_res, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return ret;
	}

	if (pll_20nm_is_pll_locked(dsi_pll_res)) {
		dsi_pll_res->handoff_resources = true;
		dsi_pll_res->pll_on = true;
		c->rate = pll_20nm_vco_get_rate(c);
		ret = HANDOFF_ENABLED_CLK;
		dsi_pll_res->vco_locking_rate = c->rate;
		dsi_pll_res->is_init_locked = true;
		pll_20nm_vco_set_rate(vco, c->rate);
		pll_20nm_cache_trim_codes(dsi_pll_res);
		pr_debug("handoff vco_locking_rate=0x%llu\n",
			dsi_pll_res->vco_locking_rate);
	} else {
		mdss_pll_resource_enable(dsi_pll_res, false);
		dsi_pll_res->vco_locking_rate = 0;
		dsi_pll_res->is_init_locked = false;
	}

	return ret;
}

int pll_20nm_vco_prepare(struct clk *c)
{
	int rc = 0;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *dsi_pll_res = vco->priv;

	if (!dsi_pll_res) {
		pr_err("Dsi pll resources are not available\n");
		return -EINVAL;
	}

	if ((dsi_pll_res->vco_cached_rate != 0)
	    && (dsi_pll_res->vco_cached_rate == c->rate)) {
		rc = c->ops->set_rate(c, dsi_pll_res->vco_cached_rate);
		if (rc) {
			pr_err("vco_set_rate failed. rc=%d\n", rc);
			goto error;
		}
	}

	rc = dsi_pll_enable(c);

error:
	return rc;
}

void pll_20nm_vco_unprepare(struct clk *c)
{
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *dsi_pll_res = vco->priv;

	if (!dsi_pll_res) {
		pr_err("Dsi pll resources are not available\n");
		return;
	}

	dsi_pll_res->vco_cached_rate = c->rate;
	dsi_pll_disable(c);
}

static void pll_20nm_config_resetsm(void __iomem *pll_base)
{
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_KVCO_CODE, 0x00);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_PLL_VCO_TUNE, 0x00);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_RESETSM_CNTRL, 0x24);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_RESETSM_CNTRL2, 0x07);
}

static void pll_20nm_config_vco_start(void __iomem *pll_base)
{

	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_PLL_VCOTAIL_EN, 0x03);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_RESETSM_CNTRL3, 0x02);
	udelay(10);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_RESETSM_CNTRL3, 0x03);
}

static void pll_20nm_config_bypass_cal(void __iomem *pll_base)
{
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_RESETSM_CNTRL, 0xac);
	MDSS_PLL_REG_W(pll_base, MMSS_DSI_PHY_PLL_PLL_BKG_KVCO_CAL_EN, 0x28);
}

static int pll_20nm_vco_relock(struct mdss_pll_resources *dsi_pll_res)
{
	int rc = 0;

	pll_20nm_override_trim_codes(dsi_pll_res);
	pll_20nm_config_bypass_cal(dsi_pll_res->pll_base);
	pll_20nm_config_vco_start(dsi_pll_res->pll_base);

	MDSS_PLL_REG_W(dsi_pll_res->pll_base, 0x374, 0x00);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base, 0x370, 0x7f);

	if (!pll_20nm_is_pll_locked(dsi_pll_res)) {
		pr_err("DSI PLL re-lock failed\n");
		rc = -EINVAL;
	}

	return rc;
}

static int pll_20nm_vco_init_lock(struct mdss_pll_resources *dsi_pll_res)
{
	int rc = 0;

	pll_20nm_config_resetsm(dsi_pll_res->pll_base);
	pll_20nm_config_vco_start(dsi_pll_res->pll_base);

	MDSS_PLL_REG_W(dsi_pll_res->pll_base, 0x374, 0x00);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base, 0x370, 0x7f);

	if (!pll_20nm_is_pll_locked(dsi_pll_res)) {
		pr_err("DSI PLL init lock failed\n");
		rc = -EINVAL;
		goto init_lock_err;
	}

	pll_20nm_cache_trim_codes(dsi_pll_res);

init_lock_err:
	return rc;
}

int pll_20nm_vco_enable_seq(struct mdss_pll_resources *dsi_pll_res)
{
	int rc = 0;
	struct mdss_pll_vco_calc vco_calc;

	if (!dsi_pll_res) {
		pr_err("Invalid PLL resources\n");
		return -EINVAL;
	}

	pll_20nm_config_common_block_1(dsi_pll_res->pll_1_base);
	pll_20nm_config_common_block_1(dsi_pll_res->pll_base);
	pll_20nm_config_common_block_2(dsi_pll_res->pll_base);
	pll_20nm_config_loop_bw(dsi_pll_res->pll_base);

	pll_20nm_vco_rate_calc(&vco_calc, dsi_pll_res->vco_current_rate,
				dsi_pll_res->vco_ref_clk_rate,
				dsi_pll_res->pll_en_90_phase);
	pll_20nm_config_vco_rate(dsi_pll_res->pll_base, &vco_calc,
				dsi_pll_res->pll_en_90_phase);

	pr_debug("init lock=%d prev vco_rate=%llu, new vco_rate=%llu\n",
		dsi_pll_res->is_init_locked, dsi_pll_res->vco_locking_rate,
		dsi_pll_res->vco_current_rate);
	/*
	 * Run auto-lock sequence if it is either bootup initial
	 * locking or when the vco rate is changed. Otherwise, just
	 * use stored codes and bypass caliberation.
	 */
	if (!dsi_pll_res->is_init_locked || (dsi_pll_res->vco_locking_rate !=
			dsi_pll_res->vco_current_rate)) {
		rc = pll_20nm_vco_init_lock(dsi_pll_res);
		dsi_pll_res->is_init_locked = (rc) ? false : true;
	} else {
		rc = pll_20nm_vco_relock(dsi_pll_res);
	}

	dsi_pll_res->vco_locking_rate = (rc) ? 0 :
		dsi_pll_res->vco_current_rate;

	return rc;
}
