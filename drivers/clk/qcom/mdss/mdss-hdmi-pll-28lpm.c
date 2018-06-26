/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/iopoll.h>
#include <dt-bindings/clock/mdss-28nm-pll-clk.h>
#include "mdss-pll.h"
#include "mdss-hdmi-pll.h"

/* HDMI PLL macros */
#define HDMI_PHY_PLL_REFCLK_CFG				(0x0400)
#define HDMI_PHY_PLL_CHRG_PUMP_CFG			(0x0404)
#define HDMI_PHY_PLL_LOOP_FLT_CFG0			(0x0408)
#define HDMI_PHY_PLL_LOOP_FLT_CFG1			(0x040c)
#define HDMI_PHY_PLL_IDAC_ADJ_CFG			(0x0410)
#define HDMI_PHY_PLL_I_VI_KVCO_CFG			(0x0414)
#define HDMI_PHY_PLL_PWRDN_B				(0x0418)
#define HDMI_PHY_PLL_SDM_CFG0				(0x041c)
#define HDMI_PHY_PLL_SDM_CFG1				(0x0420)
#define HDMI_PHY_PLL_SDM_CFG2				(0x0424)
#define HDMI_PHY_PLL_SDM_CFG3				(0x0428)
#define HDMI_PHY_PLL_SDM_CFG4				(0x042c)
#define HDMI_PHY_PLL_SSC_CFG0				(0x0430)
#define HDMI_PHY_PLL_SSC_CFG1				(0x0434)
#define HDMI_PHY_PLL_SSC_CFG2				(0x0438)
#define HDMI_PHY_PLL_SSC_CFG3				(0x043c)
#define HDMI_PHY_PLL_LOCKDET_CFG0			(0x0440)
#define HDMI_PHY_PLL_LOCKDET_CFG1			(0x0444)
#define HDMI_PHY_PLL_LOCKDET_CFG2			(0x0448)
#define HDMI_PHY_PLL_VCOCAL_CFG0			(0x044c)
#define HDMI_PHY_PLL_VCOCAL_CFG1			(0x0450)
#define HDMI_PHY_PLL_VCOCAL_CFG2			(0x0454)
#define HDMI_PHY_PLL_VCOCAL_CFG3			(0x0458)
#define HDMI_PHY_PLL_VCOCAL_CFG4			(0x045c)
#define HDMI_PHY_PLL_VCOCAL_CFG5			(0x0460)
#define HDMI_PHY_PLL_VCOCAL_CFG6			(0x0464)
#define HDMI_PHY_PLL_VCOCAL_CFG7			(0x0468)
#define HDMI_PHY_PLL_DEBUG_SEL				(0x046c)
#define HDMI_PHY_PLL_MISC0				(0x0470)
#define HDMI_PHY_PLL_MISC1				(0x0474)
#define HDMI_PHY_PLL_MISC2				(0x0478)
#define HDMI_PHY_PLL_MISC3				(0x047c)
#define HDMI_PHY_PLL_MISC4				(0x0480)
#define HDMI_PHY_PLL_MISC5				(0x0484)
#define HDMI_PHY_PLL_MISC6				(0x0488)
#define HDMI_PHY_PLL_DEBUG_BUS0				(0x048c)
#define HDMI_PHY_PLL_DEBUG_BUS1				(0x0490)
#define HDMI_PHY_PLL_DEBUG_BUS2				(0x0494)
#define HDMI_PHY_PLL_STATUS0				(0x0498)
#define HDMI_PHY_PLL_STATUS1				(0x049c)

#define HDMI_PHY_REG_0					(0x0000)
#define HDMI_PHY_REG_1					(0x0004)
#define HDMI_PHY_REG_2					(0x0008)
#define HDMI_PHY_REG_3					(0x000c)
#define HDMI_PHY_REG_4					(0x0010)
#define HDMI_PHY_REG_5					(0x0014)
#define HDMI_PHY_REG_6					(0x0018)
#define HDMI_PHY_REG_7					(0x001c)
#define HDMI_PHY_REG_8					(0x0020)
#define HDMI_PHY_REG_9					(0x0024)
#define HDMI_PHY_REG_10					(0x0028)
#define HDMI_PHY_REG_11					(0x002c)
#define HDMI_PHY_REG_12					(0x0030)
#define HDMI_PHY_REG_BIST_CFG				(0x0034)
#define HDMI_PHY_DEBUG_BUS_SEL				(0x0038)
#define HDMI_PHY_REG_MISC0				(0x003c)
#define HDMI_PHY_REG_13					(0x0040)
#define HDMI_PHY_REG_14					(0x0044)
#define HDMI_PHY_REG_15					(0x0048)

/* HDMI PHY/PLL bit field macros */
#define SW_RESET BIT(2)
#define SW_RESET_PLL BIT(0)
#define PWRDN_B BIT(7)

#define PLL_PWRDN_B BIT(3)
#define REG_VTEST_EN BIT(2)
#define PD_PLL BIT(1)
#define PD_PLL_REG BIT(0)


#define HDMI_PLL_POLL_DELAY_US			50
#define HDMI_PLL_POLL_TIMEOUT_US		500

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
			(hdmi_pll_res->pll_base + HDMI_PHY_PLL_STATUS0),
			status, ((status & BIT(0)) == 1),
			HDMI_PLL_POLL_DELAY_US,
			HDMI_PLL_POLL_TIMEOUT_US)) {
		pr_debug("HDMI PLL status=%x failed to Lock\n", status);
		pll_locked = 0;
	} else {
		pr_debug("HDMI PLL locked\n");
		pll_locked = 1;
	}
	mdss_pll_resource_enable(hdmi_pll_res, false);

	return pll_locked;
}

static void hdmi_pll_disable_28lpm(struct clk_hw *hw)
{
	struct hdmi_pll_vco_clk *vco = to_hdmi_vco_clk_hw(hw);
	struct mdss_pll_resources *hdmi_pll_res = vco->priv;
	u32 val;

	if (!hdmi_pll_res) {
		pr_err("Invalid input parameter\n");
		return;
	}

	val = MDSS_PLL_REG_R(hdmi_pll_res->pll_base, HDMI_PHY_REG_12);
	val &= (~PWRDN_B);
	MDSS_PLL_REG_W(hdmi_pll_res->pll_base, HDMI_PHY_REG_12, val);

	val = MDSS_PLL_REG_R(hdmi_pll_res->pll_base, HDMI_PHY_PLL_PWRDN_B);
	val |= PD_PLL;
	val &= (~PLL_PWRDN_B);
	MDSS_PLL_REG_W(hdmi_pll_res->pll_base, HDMI_PHY_PLL_PWRDN_B, val);

	/* Make sure HDMI PHY/PLL are powered down */
	wmb();

} /* hdmi_pll_disable_28lpm */

static int hdmi_pll_enable_28lpm(struct clk_hw *hw)
{
	struct hdmi_pll_vco_clk *vco = to_hdmi_vco_clk_hw(hw);
	struct mdss_pll_resources *hdmi_pll_res = vco->priv;
	void __iomem            *pll_base;
	u32 val;
	int pll_lock_retry = 10;

	pll_base = hdmi_pll_res->pll_base;

	/* Assert PLL S/W reset */
	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_LOCKDET_CFG2, 0x8d);
	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_LOCKDET_CFG0, 0x10);
	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_LOCKDET_CFG1, 0x1a);
	udelay(10);
	/* De-assert PLL S/W reset */
	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_LOCKDET_CFG2, 0x0d);

	MDSS_PLL_REG_W(pll_base, HDMI_PHY_REG_1, 0xf2);

	udelay(10);

	MDSS_PLL_REG_W(pll_base, HDMI_PHY_REG_2, 0x1f);

	val = MDSS_PLL_REG_R(pll_base, HDMI_PHY_REG_12);
	val |= BIT(5);
	/* Assert PHY S/W reset */
	MDSS_PLL_REG_W(pll_base, HDMI_PHY_REG_12, val);
	val &= ~BIT(5);
	udelay(10);
	/* De-assert PHY S/W reset */
	MDSS_PLL_REG_W(pll_base, HDMI_PHY_REG_12, val);

	val = MDSS_PLL_REG_R(pll_base, HDMI_PHY_REG_12);
	val |= PWRDN_B;
	MDSS_PLL_REG_W(pll_base, HDMI_PHY_REG_12, val);

	/* Wait 10 us for enabling global power for PHY */
	wmb();
	udelay(10);

	MDSS_PLL_REG_W(pll_base, HDMI_PHY_REG_3, 0x20);
	MDSS_PLL_REG_W(pll_base, HDMI_PHY_REG_4, 0x10);

	val = MDSS_PLL_REG_R(pll_base, HDMI_PHY_PLL_PWRDN_B);
	val |= PLL_PWRDN_B;
	val |= REG_VTEST_EN;
	val &= ~PD_PLL;
	val |= PD_PLL_REG;
	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_PWRDN_B, val);

	MDSS_PLL_REG_W(pll_base, HDMI_PHY_REG_2, 0x81);

	do {
		if (!hdmi_pll_lock_status(hdmi_pll_res)) {
			/* PLL has still not locked.
			 * Do a software reset and try again
			 * Assert PLL S/W reset first
			 */
			MDSS_PLL_REG_W(pll_base,
					HDMI_PHY_PLL_LOCKDET_CFG2, 0x8d);

			/* Wait for a short time before de-asserting
			 * to allow the hardware to complete its job.
			 * This much of delay should be fine for hardware
			 * to assert and de-assert.
			 */
			udelay(10);
			MDSS_PLL_REG_W(pll_base,
					HDMI_PHY_PLL_LOCKDET_CFG2, 0xd);

			/* Wait for a short duration for the PLL calibration
			 * before checking if the PLL gets locked
			 */
			udelay(350);
		} else {
			pr_debug("HDMI PLL locked\n");
			break;
		}

	} while (--pll_lock_retry);

	if (!pll_lock_retry) {
		pr_err("HDMI PLL not locked\n");
		hdmi_pll_disable_28lpm(hw);
		return -EAGAIN;
	}

	return 0;
} /* hdmi_pll_enable_28lpm */

static void hdmi_phy_pll_calculator_28lpm(unsigned long vco_rate,
			struct mdss_pll_resources *hdmi_pll_res)
{
	u32 ref_clk		= 19200000;
	u32 integer_mode	= 0;
	u32 ref_clk_multiplier	= integer_mode == 0 ? 2 : 1;
	u32 int_ref_clk_freq    = ref_clk * ref_clk_multiplier;
	u32 refclk_cfg		= 0;
	u32 ten_power_six	= 1000000;
	u64 multiplier_q	= 0;
	u64 multiplier_r	= 0;
	u32 lf_cfg0		= 0;
	u32 lf_cfg1		= 0;
	u64 vco_cfg0		= 0;
	u64 vco_cfg4		= 0;
	u64 sdm_cfg0		= 0;
	u64 sdm_cfg1		= 0;
	u64 sdm_cfg2		= 0;
	u32 val1		= 0;
	u32 val2		= 0;
	u32 val3		= 0;
	void __iomem *pll_base	= hdmi_pll_res->pll_base;

	multiplier_q = vco_rate;
	multiplier_r = do_div(multiplier_q, int_ref_clk_freq);

	lf_cfg0 = multiplier_q > 30 ? 0 : (multiplier_q > 16 ? 16 : 32);
	lf_cfg0 += integer_mode;

	lf_cfg1 = multiplier_q > 30 ? 0xc3 : (multiplier_q > 16 ? 0xbb : 0xf9);

	vco_cfg0 = vco_rate / ten_power_six;
	vco_cfg4 = ((ref_clk * 5) / ten_power_six) - 1;

	sdm_cfg0 = (integer_mode * 64) + multiplier_q - 1;
	sdm_cfg1 = 64 + multiplier_q - 1;

	sdm_cfg2 = (multiplier_r) * 65536;
	do_div(sdm_cfg2, int_ref_clk_freq);

	pr_debug("lf_cfg0 = 0x%x    lf_cfg1 = 0x%x\n", lf_cfg0, lf_cfg1);
	pr_debug("vco_cfg0 = 0x%x   vco_cfg4 = 0x%x\n", vco_cfg0, vco_cfg4);
	pr_debug("sdm_cfg0 = 0x%x   sdm_cfg1 = 0x%x   sdm_cfg2 = 0x%x\n",
				sdm_cfg0, sdm_cfg1, sdm_cfg2);

	refclk_cfg = MDSS_PLL_REG_R(pll_base, HDMI_PHY_PLL_REFCLK_CFG);
	refclk_cfg &= ~0xf;
	refclk_cfg |= (ref_clk_multiplier == 2) ? 0x8
			: (ref_clk_multiplier == 1) ? 0 : 0x2;

	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_REFCLK_CFG, refclk_cfg);
	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_CHRG_PUMP_CFG, 0x02);
	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_LOOP_FLT_CFG0, lf_cfg0);
	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_LOOP_FLT_CFG1, lf_cfg1);
	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_IDAC_ADJ_CFG, 0x2c);
	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_I_VI_KVCO_CFG, 0x06);
	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_PWRDN_B, 0x0a);

	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_SDM_CFG0, sdm_cfg0);
	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_SDM_CFG1, sdm_cfg1);

	val1 = sdm_cfg2 & 0xff;
	val2 = (sdm_cfg2 >> 8) & 0xff;
	val3 = (sdm_cfg2 >> 16) & 0xff;
	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_SDM_CFG2, val1);
	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_SDM_CFG3, val2);
	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_SDM_CFG4, val3);

	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_SSC_CFG0, 0x9a);
	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_SSC_CFG1, 0x00);
	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_SSC_CFG2, 0x00);
	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_SSC_CFG3, 0x00);
	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_LOCKDET_CFG0, 0x10);
	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_LOCKDET_CFG1, 0x1a);
	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_LOCKDET_CFG2, 0x0d);

	val1 = vco_cfg0 & 0xff;
	val2 = (vco_cfg0 >> 8) & 0xff;
	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_VCOCAL_CFG0, val1);
	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_VCOCAL_CFG1, val2);

	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_VCOCAL_CFG2, 0x3b);
	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_VCOCAL_CFG3, 0x00);

	val1 = vco_cfg4 & 0xff;
	val2 = (vco_cfg4 >> 8) & 0xff;
	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_VCOCAL_CFG4, val1);
	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_VCOCAL_CFG5, val2);

	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_VCOCAL_CFG6, 0x33);
	MDSS_PLL_REG_W(pll_base, HDMI_PHY_PLL_VCOCAL_CFG7, 0x03);

}

int hdmi_vco_set_rate_28lpm(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate)
{
	struct hdmi_pll_vco_clk *vco = to_hdmi_vco_clk_hw(hw);
	struct mdss_pll_resources *hdmi_pll_res = vco->priv;
	void __iomem		*pll_base;
	int rc;

	rc = mdss_pll_resource_enable(hdmi_pll_res, true);
	if (rc) {
		pr_err("pll resource can't be enabled\n");
		return rc;
	}

	if (hdmi_pll_res->pll_on)
		return 0;

	pll_base = hdmi_pll_res->pll_base;

	pr_debug("rate=%ld\n", rate);

	hdmi_phy_pll_calculator_28lpm(rate, hdmi_pll_res);

	/* Make sure writes complete before disabling iface clock */
	wmb();

	vco->rate = rate;
	hdmi_pll_res->vco_current_rate = rate;

	mdss_pll_resource_enable(hdmi_pll_res, false);


	return 0;
} /* hdmi_pll_set_rate */

static unsigned long hdmi_vco_get_rate(struct hdmi_pll_vco_clk *vco)
{
	unsigned long freq = 0;
	int rc = 0;
	struct mdss_pll_resources *hdmi_pll_res = vco->priv;

	rc = mdss_pll_resource_enable(hdmi_pll_res, true);
	if (rc) {
		pr_err("Failed to enable hdmi pll resources\n");
		return 0;
	}

	freq = MDSS_PLL_REG_R(hdmi_pll_res->pll_base,
			HDMI_PHY_PLL_VCOCAL_CFG1) << 8 |
		MDSS_PLL_REG_R(hdmi_pll_res->pll_base,
				HDMI_PHY_PLL_VCOCAL_CFG0);

	switch (freq) {
	case 742:
		freq = 742500000;
		break;
	case 810:
		if (MDSS_PLL_REG_R(hdmi_pll_res->pll_base,
					HDMI_PHY_PLL_SDM_CFG3) == 0x18)
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

long hdmi_vco_round_rate_28lpm(struct clk_hw *hw, unsigned long rate,
		unsigned long *parent_rate)
{
	unsigned long rrate = rate;
	struct hdmi_pll_vco_clk *vco = to_hdmi_vco_clk_hw(hw);

	if (rate < vco->min_rate)
		rrate = vco->min_rate;
	if (rate > vco->max_rate)
		rrate = vco->max_rate;

	*parent_rate = rrate;
	pr_debug("rrate=%ld\n", rrate);

	return rrate;
}

int hdmi_vco_prepare_28lpm(struct clk_hw *hw)
{
	int rc = 0;
	struct hdmi_pll_vco_clk *vco = to_hdmi_vco_clk_hw(hw);
	struct mdss_pll_resources *hdmi_res = vco->priv;

	pr_debug("rate=%ld\n", clk_hw_get_rate(hw));
	rc = mdss_pll_resource_enable(hdmi_res, true);
	if (rc) {
		pr_err("Failed to enable mdss HDMI pll resources\n");
		goto error;
	}

	if ((hdmi_res->vco_cached_rate != 0)
		&& (hdmi_res->vco_cached_rate == clk_hw_get_rate(hw))) {
		rc = vco->hw.init->ops->set_rate(hw,
			hdmi_res->vco_cached_rate, hdmi_res->vco_cached_rate);
		if (rc) {
			pr_err("index=%d vco_set_rate failed. rc=%d\n",
				rc, hdmi_res->index);
			mdss_pll_resource_enable(hdmi_res, false);
			goto error;
		}
	}

	rc = hdmi_pll_enable_28lpm(hw);
	if (rc) {
		mdss_pll_resource_enable(hdmi_res, false);
		pr_err("ndx=%d failed to enable hdmi pll\n",
					hdmi_res->index);
		goto error;
	}

	mdss_pll_resource_enable(hdmi_res, false);
	pr_debug("HDMI PLL enabled\n");
error:
	return rc;
}

void hdmi_vco_unprepare_28lpm(struct clk_hw *hw)
{
	struct hdmi_pll_vco_clk *vco = to_hdmi_vco_clk_hw(hw);
	struct mdss_pll_resources *hdmi_res = vco->priv;

	if (!hdmi_res) {
		pr_err("Invalid input parameter\n");
		return;
	}

	if (!hdmi_res->pll_on &&
		mdss_pll_resource_enable(hdmi_res, true)) {
		pr_err("pll resource can't be enabled\n");
		return;
	}

	hdmi_res->vco_cached_rate = clk_hw_get_rate(hw);
	hdmi_pll_disable_28lpm(hw);

	hdmi_res->handoff_resources = false;
	mdss_pll_resource_enable(hdmi_res, false);
	hdmi_res->pll_on = false;

	pr_debug("HDMI PLL disabled\n");
}


unsigned long hdmi_vco_recalc_rate_28lpm(struct clk_hw *hw,
				unsigned long parent_rate)
{
	struct hdmi_pll_vco_clk *vco = to_hdmi_vco_clk_hw(hw);
	struct mdss_pll_resources *hdmi_pll_res = vco->priv;
	u64 vco_rate = 0;

	if (!hdmi_pll_res) {
		pr_err("dsi pll resources not available\n");
		return 0;
	}

	if (hdmi_pll_res->vco_current_rate) {
		vco_rate = (unsigned long)hdmi_pll_res->vco_current_rate;
		pr_debug("vco_rate=%ld\n", vco_rate);
		return vco_rate;
	}

	if (is_gdsc_disabled(hdmi_pll_res))
		return 0;

	if (mdss_pll_resource_enable(hdmi_pll_res, true)) {
		pr_err("Failed to enable hdmi pll resources\n");
		return 0;
	}

	if (hdmi_pll_lock_status(hdmi_pll_res)) {
		hdmi_pll_res->handoff_resources = true;
		hdmi_pll_res->pll_on = true;
		vco_rate = hdmi_vco_get_rate(vco);
	} else {
		hdmi_pll_res->handoff_resources = false;
		mdss_pll_resource_enable(hdmi_pll_res, false);
	}

	pr_debug("vco_rate = %ld\n", vco_rate);

	return (unsigned long)vco_rate;
}

static int hdmi_mux_set_parent(void *context, unsigned int reg,
				unsigned int mux_sel)
{
	struct mdss_pll_resources *hdmi_pll_res = context;
	int rc = 0;
	u32 reg_val = 0;

	rc = mdss_pll_resource_enable(hdmi_pll_res, true);
	if (rc) {
		pr_err("Failed to enable hdmi pll resources\n");
		return rc;
	}

	pr_debug("mux_sel = %d\n", mux_sel);

	reg_val = MDSS_PLL_REG_R(hdmi_pll_res->pll_base,
				HDMI_PHY_PLL_REFCLK_CFG);
	reg_val &= ~0x70;
	reg_val |= (mux_sel & 0x70);
	pr_debug("pll_refclk_cfg = 0x%x\n", reg_val);
	MDSS_PLL_REG_W(hdmi_pll_res->pll_base,
				HDMI_PHY_PLL_REFCLK_CFG, reg_val);

	(void)mdss_pll_resource_enable(hdmi_pll_res, false);

	return 0;
}

static int hdmi_mux_get_parent(void *context, unsigned int reg,
				unsigned int *val)
{
	int rc = 0;
	int mux_sel = 0;
	struct mdss_pll_resources *hdmi_pll_res = context;

	rc = mdss_pll_resource_enable(hdmi_pll_res, true);
	if (rc) {
		*val = 0;
		pr_err("Failed to enable hdmi pll resources\n");
	} else {
		mux_sel = MDSS_PLL_REG_R(hdmi_pll_res->pll_base,
				HDMI_PHY_PLL_REFCLK_CFG);
		mux_sel &= 0x70;
		*val = mux_sel;
		pr_debug("mux_sel = %d\n", *val);
	}

	(void)mdss_pll_resource_enable(hdmi_pll_res, false);

	return rc;
}

static struct regmap_config hdmi_pll_28lpm_cfg = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register = 0x49c,
};

static struct regmap_bus hdmi_pclk_src_mux_regmap_ops = {
	.reg_write = hdmi_mux_set_parent,
	.reg_read = hdmi_mux_get_parent,
};

/* Op structures */
static const struct clk_ops hdmi_28lpm_vco_clk_ops = {
	.recalc_rate = hdmi_vco_recalc_rate_28lpm,
	.set_rate = hdmi_vco_set_rate_28lpm,
	.round_rate = hdmi_vco_round_rate_28lpm,
	.prepare = hdmi_vco_prepare_28lpm,
	.unprepare = hdmi_vco_unprepare_28lpm,
};

static struct hdmi_pll_vco_clk hdmi_vco_clk = {
	.min_rate = 540000000,
	.max_rate = 1125000000,
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_vco_clk",
		.parent_names = (const char *[]){ "cxo" },
		.num_parents = 1,
		.ops = &hdmi_28lpm_vco_clk_ops,
	},
};

static struct clk_fixed_factor hdmi_vco_divsel_one_clk_src = {
	.div = 1,
	.mult = 1,

	.hw.init = &(struct clk_init_data){
		.name = "hdmi_vco_divsel_one_clk_src",
		.parent_names =
			(const char *[]){ "hdmi_vco_clk" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor hdmi_vco_divsel_two_clk_src = {
	.div = 2,
	.mult = 1,

	.hw.init = &(struct clk_init_data){
		.name = "hdmi_vco_divsel_two_clk_src",
		.parent_names =
			(const char *[]){ "hdmi_vco_clk" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor hdmi_vco_divsel_four_clk_src = {
	.div = 4,
	.mult = 1,

	.hw.init = &(struct clk_init_data){
		.name = "hdmi_vco_divsel_four_clk_src",
		.parent_names =
			(const char *[]){ "hdmi_vco_clk" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor hdmi_vco_divsel_six_clk_src = {
	.div = 6,
	.mult = 1,

	.hw.init = &(struct clk_init_data){
		.name = "hdmi_vco_divsel_six_clk_src",
		.parent_names =
			(const char *[]){ "hdmi_vco_clk" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_regmap_mux hdmi_pclk_src_mux = {
	.reg = HDMI_PHY_PLL_REFCLK_CFG,
	.shift = 4,
	.width = 2,

	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "hdmi_pclk_src_mux",
			.parent_names =
				(const char *[]){"hdmi_vco_divsel_one_clk_src",
					"hdmi_vco_divsel_two_clk_src",
					"hdmi_vco_divsel_six_clk_src",
					"hdmi_vco_divsel_four_clk_src"},
			.num_parents = 4,
			.ops = &clk_regmap_mux_closest_ops,
			.flags = CLK_SET_RATE_PARENT,
		},
	},
};

static struct clk_fixed_factor hdmi_pclk_src = {
	.div = 5,
	.mult = 1,

	.hw.init = &(struct clk_init_data){
		.name = "hdmi_phy_pll_clk",
		.parent_names =
			(const char *[]){ "hdmi_pclk_src_mux" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_hw *mdss_hdmi_pllcc_28lpm[] = {
	[HDMI_VCO_CLK] = &hdmi_vco_clk.hw,
	[HDMI_VCO_DIVIDED_1_CLK_SRC] = &hdmi_vco_divsel_one_clk_src.hw,
	[HDMI_VCO_DIVIDED_TWO_CLK_SRC] = &hdmi_vco_divsel_two_clk_src.hw,
	[HDMI_VCO_DIVIDED_FOUR_CLK_SRC] = &hdmi_vco_divsel_four_clk_src.hw,
	[HDMI_VCO_DIVIDED_SIX_CLK_SRC] = &hdmi_vco_divsel_six_clk_src.hw,
	[HDMI_PCLK_SRC_MUX] = &hdmi_pclk_src_mux.clkr.hw,
	[HDMI_PCLK_SRC] = &hdmi_pclk_src.hw,
};

int hdmi_pll_clock_register_28lpm(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res)
{
	int rc = -ENOTSUPP, i;
	struct clk *clk;
	struct clk_onecell_data *clk_data;
	int num_clks = ARRAY_SIZE(mdss_hdmi_pllcc_28lpm);
	struct regmap *regmap;

	if (!pdev || !pdev->dev.of_node ||
		!pll_res || !pll_res->pll_base) {
		pr_err("Invalid input parameters\n");
		return -EPROBE_DEFER;
	}

	clk_data = devm_kzalloc(&pdev->dev, sizeof(struct clk_onecell_data),
			GFP_KERNEL);
	if (!clk_data)
		return -ENOMEM;

	clk_data->clks = devm_kzalloc(&pdev->dev, (num_clks *
				sizeof(struct clk *)), GFP_KERNEL);
	if (!clk_data->clks) {
		devm_kfree(&pdev->dev, clk_data);
		return -ENOMEM;
	}
	clk_data->clk_num = num_clks;

	/* Set client data for vco, mux and div clocks */
	regmap = devm_regmap_init(&pdev->dev, &hdmi_pclk_src_mux_regmap_ops,
			pll_res, &hdmi_pll_28lpm_cfg);
	hdmi_pclk_src_mux.clkr.regmap = regmap;

	hdmi_vco_clk.priv = pll_res;

	for (i = HDMI_VCO_CLK; i <= HDMI_PCLK_SRC; i++) {
		pr_debug("reg clk: %d index: %d\n", i, pll_res->index);
		clk = devm_clk_register(&pdev->dev,
				mdss_hdmi_pllcc_28lpm[i]);
		if (IS_ERR(clk)) {
			pr_err("clk registration failed for HDMI: %d\n",
					pll_res->index);
			rc = -EINVAL;
			goto clk_reg_fail;
		}
		clk_data->clks[i] = clk;
	}

	rc = of_clk_add_provider(pdev->dev.of_node,
			of_clk_src_onecell_get, clk_data);
	if (rc) {
		pr_err("%s: Clock register failed rc=%d\n", __func__, rc);
		rc = -EPROBE_DEFER;
	} else {
		pr_debug("%s SUCCESS\n", __func__);
		rc = 0;
	}
	return rc;
clk_reg_fail:
	devm_kfree(&pdev->dev, clk_data->clks);
	devm_kfree(&pdev->dev, clk_data);
	return rc;
}
