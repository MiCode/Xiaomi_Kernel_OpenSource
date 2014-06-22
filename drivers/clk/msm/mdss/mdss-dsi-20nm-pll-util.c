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

#define DSI_PLL_POLL_MAX_READS			15
#define DSI_PLL_POLL_TIMEOUT_US			1000


int set_bypass_lp_div_mux_sel(struct mux_clk *clk, int sel)
{
	struct mdss_pll_resources *dsi_pll_res = clk->priv;
	int reg_data;

	pr_debug("bypass_lp_div mux set to %s mode\n",
				sel ? "indirect" : "direct");

	reg_data = MDSS_PLL_REG_R(dsi_pll_res->pll_base,
			MMSS_DSI_PHY_PLL_POST_DIVIDER_CONTROL);
	reg_data |= BIT(7);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
			MMSS_DSI_PHY_PLL_POST_DIVIDER_CONTROL,
				reg_data | (sel << 5));

	return 0;
}

int get_bypass_lp_div_mux_sel(struct mux_clk *clk)
{
	int mux_mode, rc;
	struct mdss_pll_resources *dsi_pll_res = clk->priv;

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

	mdss_pll_resource_enable(dsi_pll_res, false);
	return rc;
}

int ndiv_get_div(struct div_clk *clk)
{
	int div = 0, rc;
	struct mdss_pll_resources *dsi_pll_res = clk->priv;

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

int fixed_hr_oclk2_get_div(struct div_clk *clk)
{
	int div = 0, rc;
	struct mdss_pll_resources *dsi_pll_res = clk->priv;

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

	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_HR_OCLK3_DIVIDER,
				(div - 1));

	mdss_pll_resource_enable(dsi_pll_res, false);
	return rc;
}

int hr_oclk3_get_div(struct div_clk *clk)
{
	int div = 0, rc;
	struct mdss_pll_resources *dsi_pll_res = clk->priv;

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

int dsi_20nm_pll_lock_status(struct mdss_pll_resources *dsi_pll_res)
{
	u32 status;
	int pll_locked;

	/* poll for PLL ready status */
	if (readl_poll_timeout_noirq((dsi_pll_res->pll_base +
			MMSS_DSI_PHY_PLL_RESET_SM),
			status,
			((status & BIT(5)) > 0),
			DSI_PLL_POLL_MAX_READS,
			DSI_PLL_POLL_TIMEOUT_US)) {
		pr_debug("DSI PLL status=%x failed to Lock\n", status);
		pll_locked = 0;
	} else if (readl_poll_timeout_noirq((dsi_pll_res->pll_base +
				MMSS_DSI_PHY_PLL_RESET_SM),
				status,
				((status & BIT(6)) > 0),
				DSI_PLL_POLL_MAX_READS,
				DSI_PLL_POLL_TIMEOUT_US)) {
			pr_debug("DSI PLL status=%x PLl not ready\n", status);
			pll_locked = 0;
	} else {
		pll_locked = 1;
	}

	return pll_locked;
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

	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_PLL_VCOTAIL_EN, 0x02);

	mdss_pll_resource_enable(dsi_pll_res, false);
	dsi_pll_res->pll_on = false;

	pr_debug("DSI PLL Disabled\n");
	return;
}

static inline void pll_20nm_phy_reset_state_machine_ctrl
			(struct mdss_pll_resources *dsi_pll_res)
{
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_RES_CODE_START_SEG1, 0x64);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_RES_CODE_START_SEG2, 0x64);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_RES_TRIM_CONTROL, 0x15);

	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_RESETSM_CNTRL, 0x20);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_RESETSM_CNTRL2, 0x07);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_RESETSM_CNTRL3, 0x02);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_RESETSM_CNTRL3, 0x03);
}

void dsi_pll_20nm_phy_ctrl_config(struct mdss_pll_resources *dsi_pll_res,
							int off)
{
	pll_20nm_phy_reset_state_machine_ctrl(dsi_pll_res);
	wmb();
	/* MMSS_DSI_0_PHY_DSIPHY_CTRL_1 */
	MDSS_PLL_REG_W(dsi_pll_res->pll_base, off + 0x0174, 0x80);
	udelay(1000);

	/* MMSS_DSI_0_PHY_DSIPHY_CTRL_1 */
	MDSS_PLL_REG_W(dsi_pll_res->pll_base, off + 0x0174, 0x00);
	/* Strength ctrl 0 */
	MDSS_PLL_REG_W(dsi_pll_res->pll_base, off + 0x0184, 0x77);
	/* MMSS_DSI_0_PHY_DSIPHY_CTRL_0 */
	MDSS_PLL_REG_W(dsi_pll_res->pll_base, off + 0x0170, 0x7f);

	/* DSI_0_PHY_DSIPHY_GLBL_TEST_CTRL */
	MDSS_PLL_REG_W(dsi_pll_res->pll_base, off + 0x01d4, 0x00);

	/* MMSS_DSI_0_PHY_DSIPHY_CTRL_2 */
	MDSS_PLL_REG_W(dsi_pll_res->pll_base, off + 0x0178, 0x00);
}

static void pll_20nm_phy_kvco_config(struct dsi_pll_vco_clk *vco)
{
	struct mdss_pll_resources *dsi_pll_res = vco->priv;

	MDSS_PLL_REG_W(dsi_pll_res->pll_base, MMSS_DSI_PHY_PLL_DIV_REF1, 0x00);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base, MMSS_DSI_PHY_PLL_DIV_REF2, 0x01);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_KVCO_COUNT1, 0x8A);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_KVCO_CAL_CNTRL, 0x00);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_KVCO_CODE, 0x00);
}

static void pll_20nm_phy_loop_bw_config(struct mdss_pll_resources *dsi_pll_res)
{
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_PLL_IP_SETI, 0x03);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_PLL_CP_SETI, 0x3F);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_PLL_IP_SETP, 0x03);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_PLL_CP_SETP, 0x1F);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_PLL_CRCTRL, 0x77);
}

static void pll_20nm_phy_config(struct dsi_pll_vco_clk *vco)
{
	struct mdss_pll_resources *dsi_pll_res = vco->priv;

	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_SYS_CLK_CTRL, 0x40);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_PLL_VCOTAIL_EN, 0x00);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base, MMSS_DSI_PHY_PLL_CMN_MODE, 0x00);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base, MMSS_DSI_PHY_PLL_IE_TRIM, 0x0F);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base, MMSS_DSI_PHY_PLL_IP_TRIM, 0x0F);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_PLL_PHSEL_CONTROL, 0x08);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_IPTAT_TRIM_VCCA_TX_SEL, 0x0E);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_PLL_PHSEL_DC, 0x00);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_CORE_CLK_IN_SYNC_SEL, 0x00);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_PLL_BKG_KVCO_CAL_EN, 0x08);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_BIAS_EN_CLKBUFLR_EN, 0x3F);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_ATB_SEL1, 0x00);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_ATB_SEL2, 0x00);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_SYSCLK_EN_SEL_TXBAND, 0x4B);
	udelay(1000);
	pll_20nm_phy_kvco_config(vco);

	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_VREF_CFG1, 0x00);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_VREF_CFG2, 0x00);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_VREF_CFG3, 0x10);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_VREF_CFG4, 0x00);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_BGTC, 0x0F);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_PLL_TEST_UPDN, 0x00);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_PLL_VCO_TUNE, 0x00);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_PLL_AMP_OS, 0x00);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_SSC_EN_CENTER, 0x00);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_RES_CODE_UP, 0x00);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_RES_CODE_DN, 0x00);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_RES_CODE_CAL_CSR, 0x00);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_RES_TRIM_EN_VCOCALDONE, 0x0);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_FAUX_EN, 0x0C);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_PLL_RXTXEPCLK_EN, 0x0F);

	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_LOW_POWER_RO_CONTROL, 0x0F);
	udelay(1000);
	pll_20nm_phy_loop_bw_config(dsi_pll_res);
}

int pll_20nm_vco_set_rate(struct dsi_pll_vco_clk *vco, unsigned long rate)
{
	s64 vco_clk_rate = rate;
	s32 div_frac_start;
	s32 div_frac_start1, div_frac_start2, div_frac_start3;
	s64 dec_start_multiple, dec_start, multiplier = (1 << 20);
	s64 dec_start1, dec_start2;
	s64 duration = 128, pll_comp_val;
	s64 pll_plllock_cmp1, pll_plllock_cmp2, pll_plllock_cmp3;

	struct mdss_pll_resources *dsi_pll_res = vco->priv;

	pr_debug("%s: vco set rate: %lld\n", __func__, vco_clk_rate);
	pll_20nm_phy_config(vco);

	dec_start_multiple = div_s64(vco_clk_rate * multiplier,
					2 * vco->ref_clk_rate);
	div_s64_rem(dec_start_multiple,
			multiplier, &div_frac_start);

	dec_start = div_s64(dec_start_multiple, multiplier);
	pr_debug("%s: dec_start_multiple = 0x%llx\n",
				__func__, dec_start_multiple);
	pr_debug("%s: dec_start = 0x%llx, div_frac_start = 0x%x\n",
			__func__, dec_start, div_frac_start);

	dec_start1 = (dec_start & 0x7f) | BIT(7);
	dec_start2 = ((dec_start & 0x80) >> 7) | BIT(1);
	pr_debug("%s: dec_start1 = 0x%llx, dec_start2 = 0x%llx\n",
				__func__, dec_start1, dec_start2);

	div_frac_start1 = (div_frac_start & 0x7f) | BIT(7);
	div_frac_start2 = ((div_frac_start >> 7) & 0x7f) | BIT(7);
	div_frac_start3 = ((div_frac_start >> 14) & 0x3f) | BIT(6);
	pr_debug("%s: div_frac_start1 = 0x%x\n",
				__func__, div_frac_start1);
	pr_debug("%s: div_frac_start2 = 0x%x\n",
				__func__, div_frac_start2);
	pr_debug("%s: div_frac_start3 = 0x%x\n",
				__func__, div_frac_start3);

	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
			MMSS_DSI_PHY_PLL_DIV_FRAC_START1,
			div_frac_start1);

	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
			MMSS_DSI_PHY_PLL_DIV_FRAC_START2,
			div_frac_start2);

	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
			MMSS_DSI_PHY_PLL_DIV_FRAC_START3,
			div_frac_start3);

	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
			MMSS_DSI_PHY_PLL_DEC_START1,
			dec_start1);

	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
			MMSS_DSI_PHY_PLL_DEC_START2,
			dec_start2);

	pll_comp_val = div_s64(dec_start_multiple * 2 * (duration - 1),
				10 * multiplier);

	pr_debug("%s: pll_comp_val = 0x%llx\n", __func__, pll_comp_val);

	pll_plllock_cmp1 = pll_comp_val & 0xff;
	pll_plllock_cmp2 = (pll_comp_val >> 8) & 0xff;
	pll_plllock_cmp3 = (pll_comp_val >> 16) & 0xff;
	pr_debug("%s: pll_plllock_cmp1 = 0x%llx\n",
				__func__, pll_plllock_cmp1);
	pr_debug("%s: pll_plllock_cmp2 = 0x%llx\n",
				__func__, pll_plllock_cmp2);
	pr_debug("%s: pll_plllock_cmp3 = 0x%llx\n",
				__func__, pll_plllock_cmp3);

	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
			MMSS_DSI_PHY_PLL_PLLLOCK_CMP1,
			pll_plllock_cmp1);

	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
			MMSS_DSI_PHY_PLL_PLLLOCK_CMP2,
			pll_plllock_cmp2);

	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
			MMSS_DSI_PHY_PLL_PLLLOCK_CMP3,
			pll_plllock_cmp3);

	/*
	 * Make sure that PLL vco configuration is complete
	 * before controlling the state machine.
	 */
	udelay(1000);
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

	rc = mdss_pll_resource_enable(dsi_pll_res, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	dec_start = (MDSS_PLL_REG_R(dsi_pll_res->pll_base,
			MMSS_DSI_PHY_PLL_DEC_START2) & BIT(0)) << 7;
	dec_start |= (MDSS_PLL_REG_R(dsi_pll_res->pll_base,
			MMSS_DSI_PHY_PLL_DEC_START1) & 0x7f);
	pr_debug("%s: dec_start = 0x%x\n", __func__, dec_start);

	div_frac_start = (MDSS_PLL_REG_R(dsi_pll_res->pll_base,
			MMSS_DSI_PHY_PLL_DIV_FRAC_START3) & 0x3f) << 14;
	div_frac_start |= (MDSS_PLL_REG_R(dsi_pll_res->pll_base,
			MMSS_DSI_PHY_PLL_DIV_FRAC_START2) & 0x7f) << 7;
	div_frac_start |= MDSS_PLL_REG_R(dsi_pll_res->pll_base,
			MMSS_DSI_PHY_PLL_DIV_FRAC_START1) & 0x7f;
	pr_debug("%s: div_frac_start = 0x%x\n",
			__func__, div_frac_start);

	vco_rate = ref_clk * 2 * dec_start;
	vco_rate += ((ref_clk * 2 * div_frac_start) / multiplier);
	pr_debug("%s: returning vco rate = %lu\n",
			__func__, (unsigned long)vco_rate);

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

	rc = mdss_pll_resource_enable(dsi_pll_res, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return ret;
	}

	if (dsi_20nm_pll_lock_status(dsi_pll_res)) {
		dsi_pll_res->handoff_resources = true;
		dsi_pll_res->pll_on = true;
		c->rate = pll_20nm_vco_get_rate(c);
		ret = HANDOFF_ENABLED_CLK;
	} else {
		mdss_pll_resource_enable(dsi_pll_res, false);
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

