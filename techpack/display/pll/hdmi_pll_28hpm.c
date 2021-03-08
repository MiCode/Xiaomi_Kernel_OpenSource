// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/iopoll.h>
#include <linux/clk/msm-clk-provider.h>
#include <linux/clk/msm-clk.h>
#include <linux/clk/msm-clock-generic.h>

#include "pll_drv.h"
#include "hdmi_pll.h"

/* hdmi phy registers */
#define HDMI_PHY_ANA_CFG0			(0x0000)
#define HDMI_PHY_ANA_CFG1			(0x0004)
#define HDMI_PHY_ANA_CFG2			(0x0008)
#define HDMI_PHY_ANA_CFG3			(0x000C)
#define HDMI_PHY_PD_CTRL0			(0x0010)
#define HDMI_PHY_PD_CTRL1			(0x0014)
#define HDMI_PHY_GLB_CFG			(0x0018)
#define HDMI_PHY_DCC_CFG0			(0x001C)
#define HDMI_PHY_DCC_CFG1			(0x0020)
#define HDMI_PHY_TXCAL_CFG0			(0x0024)
#define HDMI_PHY_TXCAL_CFG1			(0x0028)
#define HDMI_PHY_TXCAL_CFG2			(0x002C)
#define HDMI_PHY_TXCAL_CFG3			(0x0030)
#define HDMI_PHY_BIST_CFG0			(0x0034)
#define HDMI_PHY_BIST_CFG1			(0x0038)
#define HDMI_PHY_BIST_PATN0			(0x003C)
#define HDMI_PHY_BIST_PATN1			(0x0040)
#define HDMI_PHY_BIST_PATN2			(0x0044)
#define HDMI_PHY_BIST_PATN3			(0x0048)
#define HDMI_PHY_STATUS				(0x005C)

/* hdmi phy unified pll registers */
#define HDMI_UNI_PLL_REFCLK_CFG			(0x0000)
#define HDMI_UNI_PLL_POSTDIV1_CFG		(0x0004)
#define HDMI_UNI_PLL_CHFPUMP_CFG		(0x0008)
#define HDMI_UNI_PLL_VCOLPF_CFG			(0x000C)
#define HDMI_UNI_PLL_VREG_CFG			(0x0010)
#define HDMI_UNI_PLL_PWRGEN_CFG			(0x0014)
#define HDMI_UNI_PLL_GLB_CFG			(0x0020)
#define HDMI_UNI_PLL_POSTDIV2_CFG		(0x0024)
#define HDMI_UNI_PLL_POSTDIV3_CFG		(0x0028)
#define HDMI_UNI_PLL_LPFR_CFG			(0x002C)
#define HDMI_UNI_PLL_LPFC1_CFG			(0x0030)
#define HDMI_UNI_PLL_LPFC2_CFG			(0x0034)
#define HDMI_UNI_PLL_SDM_CFG0			(0x0038)
#define HDMI_UNI_PLL_SDM_CFG1			(0x003C)
#define HDMI_UNI_PLL_SDM_CFG2			(0x0040)
#define HDMI_UNI_PLL_SDM_CFG3			(0x0044)
#define HDMI_UNI_PLL_SDM_CFG4			(0x0048)
#define HDMI_UNI_PLL_SSC_CFG0			(0x004C)
#define HDMI_UNI_PLL_SSC_CFG1			(0x0050)
#define HDMI_UNI_PLL_SSC_CFG2			(0x0054)
#define HDMI_UNI_PLL_SSC_CFG3			(0x0058)
#define HDMI_UNI_PLL_LKDET_CFG0			(0x005C)
#define HDMI_UNI_PLL_LKDET_CFG1			(0x0060)
#define HDMI_UNI_PLL_LKDET_CFG2			(0x0064)
#define HDMI_UNI_PLL_CAL_CFG0			(0x006C)
#define HDMI_UNI_PLL_CAL_CFG1			(0x0070)
#define HDMI_UNI_PLL_CAL_CFG2			(0x0074)
#define HDMI_UNI_PLL_CAL_CFG3			(0x0078)
#define HDMI_UNI_PLL_CAL_CFG4			(0x007C)
#define HDMI_UNI_PLL_CAL_CFG5			(0x0080)
#define HDMI_UNI_PLL_CAL_CFG6			(0x0084)
#define HDMI_UNI_PLL_CAL_CFG7			(0x0088)
#define HDMI_UNI_PLL_CAL_CFG8			(0x008C)
#define HDMI_UNI_PLL_CAL_CFG9			(0x0090)
#define HDMI_UNI_PLL_CAL_CFG10			(0x0094)
#define HDMI_UNI_PLL_CAL_CFG11			(0x0098)
#define HDMI_UNI_PLL_STATUS			(0x00C0)

#define HDMI_PLL_POLL_DELAY_US			50
#define HDMI_PLL_POLL_TIMEOUT_US		500

static inline struct hdmi_pll_vco_clk *to_hdmi_vco_clk(struct clk *clk)
{
	return container_of(clk, struct hdmi_pll_vco_clk, c);
}

static void hdmi_vco_disable(struct clk *c)
{
	struct hdmi_pll_vco_clk *vco = to_hdmi_vco_clk(c);
	struct mdss_pll_resources *hdmi_pll_res = vco->priv;

	if (!hdmi_pll_res) {
		pr_err("Invalid input parameter\n");
		return;
	}

	if (!hdmi_pll_res->pll_on &&
		mdss_pll_resource_enable(hdmi_pll_res, true)) {
		pr_err("pll resource can't be enabled\n");
		return;
	}

	MDSS_PLL_REG_W(hdmi_pll_res->pll_base, HDMI_UNI_PLL_GLB_CFG, 0x0);
	udelay(5);
	MDSS_PLL_REG_W(hdmi_pll_res->phy_base, HDMI_PHY_GLB_CFG, 0x0);

	hdmi_pll_res->handoff_resources = false;
	mdss_pll_resource_enable(hdmi_pll_res, false);
	hdmi_pll_res->pll_on = false;
} /* hdmi_vco_disable */

static int hdmi_vco_enable(struct clk *c)
{
	u32 status;
	u32 delay_us, timeout_us;
	int rc;
	struct hdmi_pll_vco_clk *vco = to_hdmi_vco_clk(c);
	struct mdss_pll_resources *hdmi_pll_res = vco->priv;

	rc = mdss_pll_resource_enable(hdmi_pll_res, true);
	if (rc) {
		pr_err("pll resource can't be enabled\n");
		return rc;
	}

	/* Global Enable */
	MDSS_PLL_REG_W(hdmi_pll_res->phy_base, HDMI_PHY_GLB_CFG, 0x81);
	/* Power up power gen */
	MDSS_PLL_REG_W(hdmi_pll_res->phy_base, HDMI_PHY_PD_CTRL0, 0x00);
	udelay(350);

	/* PLL Power-Up */
	MDSS_PLL_REG_W(hdmi_pll_res->pll_base, HDMI_UNI_PLL_GLB_CFG, 0x01);
	udelay(5);
	/* Power up PLL LDO */
	MDSS_PLL_REG_W(hdmi_pll_res->pll_base, HDMI_UNI_PLL_GLB_CFG, 0x03);
	udelay(350);

	/* PLL Power-Up */
	MDSS_PLL_REG_W(hdmi_pll_res->pll_base, HDMI_UNI_PLL_GLB_CFG, 0x0F);
	udelay(350);

	/* poll for PLL ready status */
	delay_us = 100;
	timeout_us = 2000;
	if (readl_poll_timeout_atomic(
		(hdmi_pll_res->pll_base + HDMI_UNI_PLL_STATUS),
		status, ((status & BIT(0)) == 1), delay_us, timeout_us)) {
		pr_err("hdmi phy pll status=%x failed to Lock\n", status);
		hdmi_vco_disable(c);
		mdss_pll_resource_enable(hdmi_pll_res, false);
		return -EINVAL;
	}
	pr_debug("hdmi phy pll is locked\n");

	udelay(350);
	/* poll for PHY ready status */
	delay_us = 100;
	timeout_us = 2000;
	if (readl_poll_timeout_atomic(
		(hdmi_pll_res->phy_base + HDMI_PHY_STATUS),
		status, ((status & BIT(0)) == 1), delay_us, timeout_us)) {
		pr_err("hdmi phy status=%x failed to Lock\n", status);
		hdmi_vco_disable(c);
		mdss_pll_resource_enable(hdmi_pll_res, false);
		return -EINVAL;
	}
	hdmi_pll_res->pll_on = true;
	pr_debug("hdmi phy is locked\n");

	return 0;
} /* hdmi_vco_enable */


static void hdmi_phy_pll_calculator(u32 vco_freq,
				struct mdss_pll_resources *hdmi_pll_res)
{
	u32 ref_clk             = 19200000;
	u32 sdm_mode            = 1;
	u32 ref_clk_multiplier  = sdm_mode == 1 ? 2 : 1;
	u32 int_ref_clk_freq    = ref_clk * ref_clk_multiplier;
	u32 fbclk_pre_div       = 1;
	u32 ssc_mode            = 0;
	u32 kvco                = 270;
	u32 vdd                 = 95;
	u32 ten_power_six       = 1000000;
	u32 ssc_ds_ppm          = ssc_mode ? 5000 : 0;
	u32 sdm_res             = 16;
	u32 ssc_tri_step        = 32;
	u32 ssc_freq            = 2;
	u64 ssc_ds              = vco_freq * ssc_ds_ppm;
	u32 div_in_freq         = vco_freq / fbclk_pre_div;
	u64 dc_offset           = (div_in_freq / int_ref_clk_freq - 1) *
					ten_power_six * 10;
	u32 ssc_kdiv            = (int_ref_clk_freq / ssc_freq) -
					ten_power_six;
	u64 sdm_freq_seed;
	u32 ssc_tri_inc;
	u64 fb_div_n;
	void __iomem		*pll_base = hdmi_pll_res->pll_base;
	u32 val;

	pr_debug("vco_freq = %u\n", vco_freq);

	do_div(ssc_ds, (u64)ten_power_six);

	fb_div_n = (u64)div_in_freq * (u64)ten_power_six * 10;
	do_div(fb_div_n, int_ref_clk_freq);

	sdm_freq_seed = ((fb_div_n - dc_offset - ten_power_six * 10) *
				(1 << sdm_res)  * 10) + 5;
	do_div(sdm_freq_seed, ((u64)ten_power_six * 100));

	ssc_tri_inc = (u32)ssc_ds;
	ssc_tri_inc = (ssc_tri_inc / int_ref_clk_freq) * (1 << 16) /
			ssc_tri_step;

	val = (ref_clk_multiplier == 2 ? 1 : 0) +
		((fbclk_pre_div == 2 ? 1 : 0) * 16);
	pr_debug("HDMI_UNI_PLL_REFCLK_CFG = 0x%x\n", val);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_REFCLK_CFG, val);

	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CHFPUMP_CFG, 0x02);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_VCOLPF_CFG, 0x19);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_VREG_CFG, 0x04);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_PWRGEN_CFG, 0x00);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_POSTDIV2_CFG, 0x00);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_POSTDIV3_CFG, 0x00);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LPFR_CFG, 0x0E);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LPFC1_CFG, 0x20);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LPFC2_CFG, 0x0D);

	do_div(dc_offset, (u64)ten_power_six * 10);
	val = sdm_mode == 0 ? 64 + dc_offset : 0;
	pr_debug("HDMI_UNI_PLL_SDM_CFG0 = 0x%x\n", val);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG0, val);

	val = 64 + dc_offset;
	pr_debug("HDMI_UNI_PLL_SDM_CFG1 = 0x%x\n", val);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG1, val);

	val = sdm_freq_seed & 0xFF;
	pr_debug("HDMI_UNI_PLL_SDM_CFG2 = 0x%x\n", val);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG2, val);

	val = (sdm_freq_seed >> 8) & 0xFF;
	pr_debug("HDMI_UNI_PLL_SDM_CFG3 = 0x%x\n", val);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG3, val);

	val = (sdm_freq_seed >> 16) & 0xFF;
	pr_debug("HDMI_UNI_PLL_SDM_CFG4 = 0x%x\n", val);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG4, val);

	val = (ssc_mode == 0 ? 128 : 0) + (ssc_kdiv / ten_power_six);
	pr_debug("HDMI_UNI_PLL_SSC_CFG0 = 0x%x\n", val);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SSC_CFG0, val);

	val = ssc_tri_inc & 0xFF;
	pr_debug("HDMI_UNI_PLL_SSC_CFG1 = 0x%x\n", val);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SSC_CFG1, val);

	val = (ssc_tri_inc >> 8) & 0xFF;
	pr_debug("HDMI_UNI_PLL_SSC_CFG2 = 0x%x\n", val);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SSC_CFG2, val);

	pr_debug("HDMI_UNI_PLL_SSC_CFG3 = 0x%x\n", ssc_tri_step);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SSC_CFG3, ssc_tri_step);

	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LKDET_CFG0, 0x10);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LKDET_CFG1, 0x1A);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LKDET_CFG2, 0x05);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG0, 0x0A);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG1, 0x04);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG2, 0x01);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG3, 0x00);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG4, 0x00);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG5, 0x00);

	val = (kvco * vdd * 10000) / 6;
	val += 500000;
	val /= ten_power_six;
	pr_debug("HDMI_UNI_PLL_CAL_CFG6 = 0x%x\n", val);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG6, val & 0xFF);

	val = (kvco * vdd * 10000) / 6;
	val -= ten_power_six;
	val /= ten_power_six;
	val = (val >> 8) & 0xFF;
	pr_debug("HDMI_UNI_PLL_CAL_CFG7 = 0x%x\n", val);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG7, val);

	val = (ref_clk * 5) / ten_power_six;
	pr_debug("HDMI_UNI_PLL_CAL_CFG8 = 0x%x\n", val);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG8, val);

	val = ((ref_clk * 5) / ten_power_six) >> 8;
	pr_debug("HDMI_UNI_PLL_CAL_CFG9 = 0x%x\n", val);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG9, val);

	vco_freq /= ten_power_six;
	val = vco_freq & 0xFF;
	pr_debug("HDMI_UNI_PLL_CAL_CFG10 = 0x%x\n", val);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG10, val);

	val = vco_freq >> 8;
	pr_debug("HDMI_UNI_PLL_CAL_CFG11 = 0x%x\n", val);
	MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG11, val);
} /* hdmi_phy_pll_calculator */

static int hdmi_vco_set_rate(struct clk *c, unsigned long rate)
{
	struct hdmi_pll_vco_clk *vco = to_hdmi_vco_clk(c);
	struct mdss_pll_resources *hdmi_pll_res = vco->priv;
	void __iomem		*pll_base;
	void __iomem		*phy_base;
	unsigned int set_power_dwn = 0;
	int rc;

	rc = mdss_pll_resource_enable(hdmi_pll_res, true);
	if (rc) {
		pr_err("pll resource can't be enabled\n");
		return rc;
	}

	if (hdmi_pll_res->pll_on) {
		hdmi_vco_disable(c);
		set_power_dwn = 1;
	}

	pll_base = hdmi_pll_res->pll_base;
	phy_base = hdmi_pll_res->phy_base;

	pr_debug("rate=%ld\n", rate);

	switch (rate) {
	case 0:
		break;

	case 756000000:
		/* 640x480p60 */
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_GLB_CFG, 0x81);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_GLB_CFG, 0x01);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_REFCLK_CFG, 0x01);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_VCOLPF_CFG, 0x19);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LPFR_CFG, 0x0E);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LPFC1_CFG, 0x20);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LPFC2_CFG, 0x0D);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG0, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG1, 0x52);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG2, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG3, 0xB0);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG4, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LKDET_CFG0, 0x10);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LKDET_CFG1, 0x1A);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LKDET_CFG2, 0x05);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_POSTDIV2_CFG, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_POSTDIV3_CFG, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG2, 0x01);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG8, 0x60);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG9, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG10, 0xF4);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG11, 0x02);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_PD_CTRL0, 0x1F);
		udelay(50);

		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_GLB_CFG, 0x0F);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_PD_CTRL1, 0x00);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG2, 0x10);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG0, 0xDB);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG1, 0x43);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG2, 0x02);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG3, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_VREG_CFG, 0x04);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_DCC_CFG0, 0xD0);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_DCC_CFG1, 0x1A);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG0, 0x00);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG1, 0x00);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG2, 0x02);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG3, 0x05);
		udelay(200);
		break;

	case 810000000:
		/* 576p50/576i50 case */
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_GLB_CFG, 0x81);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_GLB_CFG, 0x01);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_REFCLK_CFG, 0x01);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_VCOLPF_CFG, 0x19);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LPFR_CFG, 0x0E);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LPFC1_CFG, 0x20);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LPFC2_CFG, 0x0D);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG0, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG1, 0x54);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG2, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG3, 0x18);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG4, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LKDET_CFG0, 0x10);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LKDET_CFG1, 0x1A);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LKDET_CFG2, 0x05);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_POSTDIV2_CFG, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_POSTDIV3_CFG, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG2, 0x01);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG8, 0x60);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG9, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG10, 0x2A);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG11, 0x03);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_PD_CTRL0, 0x1F);
		udelay(50);

		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_GLB_CFG, 0x0F);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_PD_CTRL1, 0x00);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG2, 0x10);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG0, 0xDB);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG1, 0x43);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG2, 0x02);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG3, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_VREG_CFG, 0x04);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_DCC_CFG0, 0xD0);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_DCC_CFG1, 0x1A);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG0, 0x00);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG1, 0x00);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG2, 0x02);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG3, 0x05);
		udelay(200);
		break;

	case 810900000:
		/* 480p60/480i60 case */
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_GLB_CFG, 0x81);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_GLB_CFG, 0x01);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_REFCLK_CFG, 0x01);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_VCOLPF_CFG, 0x19);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LPFR_CFG, 0x0E);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LPFC1_CFG, 0x20);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LPFC2_CFG, 0x0D);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG0, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG1, 0x54);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG2, 0x66);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG3, 0x1D);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG4, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LKDET_CFG0, 0x10);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LKDET_CFG1, 0x1A);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LKDET_CFG2, 0x05);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_POSTDIV2_CFG, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_POSTDIV3_CFG, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG2, 0x01);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG8, 0x60);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG9, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG10, 0x2A);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG11, 0x03);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_PD_CTRL0, 0x1F);
		udelay(50);

		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_GLB_CFG, 0x0F);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_PD_CTRL1, 0x00);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG2, 0x10);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG0, 0xDB);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG1, 0x43);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG2, 0x02);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG3, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_VREG_CFG, 0x04);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_DCC_CFG0, 0xD0);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_DCC_CFG1, 0x1A);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG0, 0x00);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG1, 0x00);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG2, 0x02);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG3, 0x05);
		udelay(200);
		break;

	case 650000000:
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_GLB_CFG, 0x81);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_GLB_CFG, 0x01);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_REFCLK_CFG, 0x01);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_VCOLPF_CFG, 0x19);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LPFR_CFG, 0x0E);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LPFC1_CFG, 0x20);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LPFC2_CFG, 0x0D);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG0, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG1, 0x4F);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG2, 0x55);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG3, 0xED);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG4, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LKDET_CFG0, 0x10);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LKDET_CFG1, 0x1A);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LKDET_CFG2, 0x05);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_POSTDIV2_CFG, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_POSTDIV3_CFG, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG2, 0x01);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG8, 0x60);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG9, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG10, 0x8A);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG11, 0x02);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_PD_CTRL0, 0x1F);
		udelay(50);

		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_GLB_CFG, 0x0F);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_PD_CTRL1, 0x00);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG2, 0x10);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG0, 0xDB);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG1, 0x43);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG2, 0x02);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG3, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_VREG_CFG, 0x04);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_DCC_CFG0, 0xD0);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_DCC_CFG1, 0x1A);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG0, 0x00);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG1, 0x00);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG2, 0x02);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG3, 0x05);
		udelay(200);
		break;

	case 742500000:
		/*
		 * 720p60/720p50/1080i60/1080i50
		 * 1080p24/1080p30/1080p25 case
		 */
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_GLB_CFG, 0x81);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_GLB_CFG, 0x01);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_REFCLK_CFG, 0x01);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_VCOLPF_CFG, 0x19);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LPFR_CFG, 0x0E);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LPFC1_CFG, 0x20);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LPFC2_CFG, 0x0D);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG0, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG1, 0x52);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG2, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG3, 0x56);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG4, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LKDET_CFG0, 0x10);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LKDET_CFG1, 0x1A);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LKDET_CFG2, 0x05);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_POSTDIV2_CFG, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_POSTDIV3_CFG, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG2, 0x01);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG8, 0x60);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG9, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG10, 0xE6);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG11, 0x02);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_PD_CTRL0, 0x1F);
		udelay(50);

		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_GLB_CFG, 0x0F);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_PD_CTRL1, 0x00);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG2, 0x10);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG0, 0xDB);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG1, 0x43);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG2, 0x02);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG3, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_VREG_CFG, 0x04);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_DCC_CFG0, 0xD0);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_DCC_CFG1, 0x1A);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG0, 0x00);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG1, 0x00);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG2, 0x02);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG3, 0x05);
		udelay(200);
		break;

	case 1080000000:
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_GLB_CFG, 0x81);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_GLB_CFG, 0x01);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_REFCLK_CFG, 0x01);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_VCOLPF_CFG, 0x19);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LPFR_CFG, 0x0E);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LPFC1_CFG, 0x20);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LPFC2_CFG, 0x0D);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG0, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG1, 0x5B);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG2, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG3, 0x20);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG4, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LKDET_CFG0, 0x10);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LKDET_CFG1, 0x1A);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LKDET_CFG2, 0x05);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_POSTDIV2_CFG, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_POSTDIV3_CFG, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG2, 0x01);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG8, 0x60);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG9, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG10, 0x38);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG11, 0x04);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_PD_CTRL0, 0x1F);
		udelay(50);

		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_GLB_CFG, 0x0F);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_PD_CTRL1, 0x00);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG2, 0x10);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG0, 0xDB);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG1, 0x43);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG2, 0x02);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG3, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_VREG_CFG, 0x04);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_DCC_CFG0, 0xD0);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_DCC_CFG1, 0x1A);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG0, 0x00);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG1, 0x00);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG2, 0x02);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG3, 0x05);
		udelay(200);
		break;

	case 1342500000:
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_GLB_CFG, 0x81);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_GLB_CFG, 0x01);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_REFCLK_CFG, 0x01);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_VCOLPF_CFG, 0x19);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LPFR_CFG, 0x0E);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LPFC1_CFG, 0x20);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LPFC2_CFG, 0x0D);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG0, 0x36);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG1, 0x61);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG2, 0x01);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG3, 0xF6);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG4, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LKDET_CFG0, 0x10);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LKDET_CFG1, 0x1A);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LKDET_CFG2, 0x05);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_POSTDIV2_CFG, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_POSTDIV3_CFG, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG2, 0x01);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG8, 0x60);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG9, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG10, 0x3E);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG11, 0x05);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_PD_CTRL0, 0x1F);
		udelay(50);

		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_GLB_CFG, 0x0F);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_PD_CTRL1, 0x00);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG2, 0x10);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG0, 0xDB);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG1, 0x43);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG2, 0x05);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG3, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_VREG_CFG, 0x04);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_DCC_CFG0, 0xD0);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_DCC_CFG1, 0x1A);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG0, 0x00);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG1, 0x00);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG2, 0x11);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG3, 0x05);
		udelay(200);
		break;

	case 1485000000:
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_GLB_CFG, 0x81);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_GLB_CFG, 0x01);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_REFCLK_CFG, 0x01);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_VCOLPF_CFG, 0x19);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LPFR_CFG, 0x0E);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LPFC1_CFG, 0x20);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LPFC2_CFG, 0x0D);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG0, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG1, 0x65);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG2, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG3, 0xAC);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_SDM_CFG4, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LKDET_CFG0, 0x10);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LKDET_CFG1, 0x1A);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_LKDET_CFG2, 0x05);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_POSTDIV2_CFG, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_POSTDIV3_CFG, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG2, 0x01);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG8, 0x60);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG9, 0x00);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG10, 0xCD);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_CAL_CFG11, 0x05);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_PD_CTRL0, 0x1F);
		udelay(50);

		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_GLB_CFG, 0x0F);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_PD_CTRL1, 0x00);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG2, 0x10);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG0, 0xDB);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG1, 0x43);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG2, 0x06);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG3, 0x03);
		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_VREG_CFG, 0x04);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_DCC_CFG0, 0xD0);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_DCC_CFG1, 0x1A);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG0, 0x00);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG1, 0x00);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG2, 0x02);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG3, 0x05);
		udelay(200);
		break;

	default:
		pr_debug("Use pll settings calculator for rate=%ld\n", rate);

		MDSS_PLL_REG_W(phy_base, HDMI_PHY_GLB_CFG, 0x81);
		hdmi_phy_pll_calculator(rate, hdmi_pll_res);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_PD_CTRL0, 0x1F);
		udelay(50);

		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_GLB_CFG, 0x0F);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_PD_CTRL1, 0x00);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG2, 0x10);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG0, 0xDB);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG1, 0x43);

		if (rate < 825000000) {
			MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG2, 0x01);
			MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG3, 0x00);
		} else if (rate >= 825000000 && rate < 1342500000) {
			MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG2, 0x05);
			MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG3, 0x03);
		} else {
			MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG2, 0x06);
			MDSS_PLL_REG_W(phy_base, HDMI_PHY_ANA_CFG3, 0x03);
		}

		MDSS_PLL_REG_W(pll_base, HDMI_UNI_PLL_VREG_CFG, 0x04);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_DCC_CFG0, 0xD0);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_DCC_CFG1, 0x1A);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG0, 0x00);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG1, 0x00);

		if (rate < 825000000)
			MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG2, 0x01);
		else
			MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG2, 0x00);

		MDSS_PLL_REG_W(phy_base, HDMI_PHY_TXCAL_CFG3, 0x05);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_BIST_PATN0, 0x62);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_BIST_PATN1, 0x03);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_BIST_PATN2, 0x69);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_BIST_PATN3, 0x02);

		udelay(200);

		MDSS_PLL_REG_W(phy_base, HDMI_PHY_BIST_CFG1, 0x00);
		MDSS_PLL_REG_W(phy_base, HDMI_PHY_BIST_CFG0, 0x00);
	}

	/* Make sure writes complete before disabling iface clock */
	mb();

	mdss_pll_resource_enable(hdmi_pll_res, false);

	if (set_power_dwn)
		hdmi_vco_enable(c);

	vco->rate = rate;
	vco->rate_set = true;

	return 0;
} /* hdmi_pll_set_rate */

/* HDMI PLL DIV CLK */

static unsigned long hdmi_vco_get_rate(struct clk *c)
{
	unsigned long freq = 0;
	int rc;
	struct hdmi_pll_vco_clk *vco = to_hdmi_vco_clk(c);
	struct mdss_pll_resources *hdmi_pll_res = vco->priv;

	if (is_gdsc_disabled(hdmi_pll_res))
		return 0;

	rc = mdss_pll_resource_enable(hdmi_pll_res, true);
	if (rc) {
		pr_err("pll resource can't be enabled\n");
		return rc;
	}

	freq = MDSS_PLL_REG_R(hdmi_pll_res->pll_base,
			HDMI_UNI_PLL_CAL_CFG11) << 8 |
		MDSS_PLL_REG_R(hdmi_pll_res->pll_base, HDMI_UNI_PLL_CAL_CFG10);

	switch (freq) {
	case 742:
		freq = 742500000;
		break;
	case 810:
		if (MDSS_PLL_REG_R(hdmi_pll_res->pll_base,
					HDMI_UNI_PLL_SDM_CFG3) == 0x18)
			freq = 810000000;
		else
			freq = 810900000;
		break;
	case 1342:
		freq = 1342500000;
		break;
	default:
		freq *= 1000000;
	}
	mdss_pll_resource_enable(hdmi_pll_res, false);

	return freq;
}

static long hdmi_vco_round_rate(struct clk *c, unsigned long rate)
{
	unsigned long rrate = rate;
	struct hdmi_pll_vco_clk *vco = to_hdmi_vco_clk(c);

	if (rate < vco->min_rate)
		rrate = vco->min_rate;
	if (rate > vco->max_rate)
		rrate = vco->max_rate;

	pr_debug("rrate=%ld\n", rrate);

	return rrate;
}

static int hdmi_vco_prepare(struct clk *c)
{
	struct hdmi_pll_vco_clk *vco = to_hdmi_vco_clk(c);
	int ret = 0;

	pr_debug("rate=%ld\n", vco->rate);

	if (!vco->rate_set && vco->rate)
		ret = hdmi_vco_set_rate(c, vco->rate);

	return ret;
}

static void hdmi_vco_unprepare(struct clk *c)
{
	struct hdmi_pll_vco_clk *vco = to_hdmi_vco_clk(c);

	vco->rate_set = false;
}

static int hdmi_pll_lock_status(struct mdss_pll_resources *hdmi_pll_res)
{
	u32 status;
	int pll_locked = 0;
	int rc;

	rc = mdss_pll_resource_enable(hdmi_pll_res, true);
	if (rc) {
		pr_err("pll resource can't be enabled\n");
		return rc;
	}

	/* poll for PLL ready status */
	if (readl_poll_timeout_atomic(
			(hdmi_pll_res->phy_base + HDMI_PHY_STATUS),
			status, ((status & BIT(0)) == 1),
			HDMI_PLL_POLL_DELAY_US,
			HDMI_PLL_POLL_TIMEOUT_US)) {
		pr_debug("HDMI PLL status=%x failed to Lock\n", status);
		pll_locked = 0;
	} else {
		pll_locked = 1;
	}
	mdss_pll_resource_enable(hdmi_pll_res, false);

	return pll_locked;
}

static enum handoff hdmi_vco_handoff(struct clk *c)
{
	enum handoff ret = HANDOFF_DISABLED_CLK;
	struct hdmi_pll_vco_clk *vco = to_hdmi_vco_clk(c);
	struct mdss_pll_resources *hdmi_pll_res = vco->priv;

	if (is_gdsc_disabled(hdmi_pll_res))
		return HANDOFF_DISABLED_CLK;

	if (mdss_pll_resource_enable(hdmi_pll_res, true)) {
		pr_err("pll resource can't be enabled\n");
		return ret;
	}

	hdmi_pll_res->handoff_resources = true;

	if (hdmi_pll_lock_status(hdmi_pll_res)) {
		hdmi_pll_res->pll_on = true;
		c->rate = hdmi_vco_get_rate(c);
		ret = HANDOFF_ENABLED_CLK;
	} else {
		hdmi_pll_res->handoff_resources = false;
		mdss_pll_resource_enable(hdmi_pll_res, false);
	}

	pr_debug("done, ret=%d\n", ret);
	return ret;
}

static const struct clk_ops hdmi_vco_clk_ops = {
	.enable = hdmi_vco_enable,
	.set_rate = hdmi_vco_set_rate,
	.get_rate = hdmi_vco_get_rate,
	.round_rate = hdmi_vco_round_rate,
	.prepare = hdmi_vco_prepare,
	.unprepare = hdmi_vco_unprepare,
	.disable = hdmi_vco_disable,
	.handoff = hdmi_vco_handoff,
};

static struct hdmi_pll_vco_clk hdmi_vco_clk = {
	.min_rate = 600000000,
	.max_rate = 1800000000,
	.c = {
		.dbg_name = "hdmi_vco_clk",
		.ops = &hdmi_vco_clk_ops,
		CLK_INIT(hdmi_vco_clk.c),
	},
};

struct div_clk hdmipll_div1_clk = {
	.data = {
		.div = 1,
		.min_div = 1,
		.max_div = 1,
	},
	.c = {
		.parent = &hdmi_vco_clk.c,
		.dbg_name = "hdmipll_div1_clk",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(hdmipll_div1_clk.c),
	},
};

struct div_clk hdmipll_div2_clk = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &hdmi_vco_clk.c,
		.dbg_name = "hdmipll_div2_clk",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(hdmipll_div2_clk.c),
	},
};

struct div_clk hdmipll_div4_clk = {
	.data = {
		.div = 4,
		.min_div = 4,
		.max_div = 4,
	},
	.c = {
		.parent = &hdmi_vco_clk.c,
		.dbg_name = "hdmipll_div4_clk",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(hdmipll_div4_clk.c),
	},
};

struct div_clk hdmipll_div6_clk = {
	.data = {
		.div = 6,
		.min_div = 6,
		.max_div = 6,
	},
	.c = {
		.parent = &hdmi_vco_clk.c,
		.dbg_name = "hdmipll_div6_clk",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(hdmipll_div6_clk.c),
	},
};

static int hdmipll_set_mux_sel(struct mux_clk *clk, int mux_sel)
{
	struct mdss_pll_resources *hdmi_pll_res = clk->priv;
	int rc;

	rc = mdss_pll_resource_enable(hdmi_pll_res, true);
	if (rc) {
		pr_err("pll resource can't be enabled\n");
		return rc;
	}

	pr_debug("mux_sel=%d\n", mux_sel);
	MDSS_PLL_REG_W(hdmi_pll_res->pll_base,
				HDMI_UNI_PLL_POSTDIV1_CFG, mux_sel);
	mdss_pll_resource_enable(hdmi_pll_res, false);

	return 0;
}

static int hdmipll_get_mux_sel(struct mux_clk *clk)
{
	int rc;
	int mux_sel = 0;
	struct mdss_pll_resources *hdmi_pll_res = clk->priv;

	if (is_gdsc_disabled(hdmi_pll_res))
		return 0;

	rc = mdss_pll_resource_enable(hdmi_pll_res, true);
	if (rc) {
		pr_err("pll resource can't be enabled\n");
		return rc;
	}

	mux_sel = MDSS_PLL_REG_R(hdmi_pll_res->pll_base,
				HDMI_UNI_PLL_POSTDIV1_CFG);
	mdss_pll_resource_enable(hdmi_pll_res, false);
	mux_sel &= 0x03;
	pr_debug("mux_sel=%d\n", mux_sel);

	return mux_sel;
}

static struct clk_mux_ops hdmipll_mux_ops = {
	.set_mux_sel = hdmipll_set_mux_sel,
	.get_mux_sel = hdmipll_get_mux_sel,
};

static const struct clk_ops hdmi_mux_ops;

static int hdmi_mux_prepare(struct clk *c)
{
	int ret = 0;

	if (c && c->ops && c->ops->set_rate)
		ret = c->ops->set_rate(c, c->rate);

	return ret;
}

struct mux_clk hdmipll_mux_clk = {
	MUX_SRC_LIST(
		{ &hdmipll_div1_clk.c, 0 },
		{ &hdmipll_div2_clk.c, 1 },
		{ &hdmipll_div4_clk.c, 2 },
		{ &hdmipll_div6_clk.c, 3 },
	),
	.ops = &hdmipll_mux_ops,
	.c = {
		.parent = &hdmipll_div1_clk.c,
		.dbg_name = "hdmipll_mux_clk",
		.ops = &hdmi_mux_ops,
		CLK_INIT(hdmipll_mux_clk.c),
	},
};

struct div_clk hdmipll_clk_src = {
	.data = {
		.div = 5,
		.min_div = 5,
		.max_div = 5,
	},
	.c = {
		.parent = &hdmipll_mux_clk.c,
		.dbg_name = "hdmipll_clk_src",
		.ops = &clk_ops_div,
		CLK_INIT(hdmipll_clk_src.c),
	},
};

static struct clk_lookup hdmipllcc_8974[] = {
	CLK_LOOKUP("extp_clk_src", hdmipll_clk_src.c,
						"fd8c0000.qcom,mmsscc-mdss"),
};

int hdmi_pll_clock_register(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res)
{
	int rc = -ENOTSUPP;

	/* Set client data for vco, mux and div clocks */
	hdmipll_clk_src.priv = pll_res;
	hdmipll_mux_clk.priv = pll_res;
	hdmipll_div1_clk.priv = pll_res;
	hdmipll_div2_clk.priv = pll_res;
	hdmipll_div4_clk.priv = pll_res;
	hdmipll_div6_clk.priv = pll_res;
	hdmi_vco_clk.priv = pll_res;

	/* Set hdmi mux clock operation */
	hdmi_mux_ops = clk_ops_gen_mux;
	hdmi_mux_ops.prepare = hdmi_mux_prepare;

	rc = of_msm_clock_register(pdev->dev.of_node, hdmipllcc_8974,
						 ARRAY_SIZE(hdmipllcc_8974));
	if (rc) {
		pr_err("Clock register failed rc=%d\n", rc);
		rc = -EPROBE_DEFER;
	}

	return rc;
}
