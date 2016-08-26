/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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
#include <linux/iopoll.h>
#include <linux/delay.h>
#include <linux/clk/msm-clock-generic.h>

#include "mdss-pll.h"
#include "mdss-dp-pll.h"
#include "mdss-dp-pll-cobalt.h"

int link2xclk_divsel_set_div(struct div_clk *clk, int div)
{
	int rc;
	u32 link2xclk_div_tx0, link2xclk_div_tx1;
	u32 phy_mode;
	struct mdss_pll_resources *dp_res = clk->priv;

	rc = mdss_pll_resource_enable(dp_res, true);
	if (rc) {
		pr_err("Failed to enable mdss DP PLL resources\n");
		return rc;
	}

	link2xclk_div_tx0 = MDSS_PLL_REG_R(dp_res->phy_base,
				QSERDES_TX0_OFFSET + TXn_TX_BAND);
	link2xclk_div_tx1 = MDSS_PLL_REG_R(dp_res->phy_base,
				QSERDES_TX1_OFFSET + TXn_TX_BAND);

	link2xclk_div_tx0 &= ~0x07;	/* bits 0 to 2 */
	link2xclk_div_tx1 &= ~0x07;	/* bits 0 to 2 */

	/* Configure TX band Mux */
	link2xclk_div_tx0 |= 0x4;
	link2xclk_div_tx1 |= 0x4;

	/*configure DP PHY MODE */
	phy_mode = 0x48;

	if (div == 10) {
		link2xclk_div_tx0 |= 1;
		link2xclk_div_tx1 |= 1;
		phy_mode |= 1;
	}
	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX0_OFFSET + TXn_TX_BAND,
			link2xclk_div_tx0);
	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX1_OFFSET + TXn_TX_BAND,
			link2xclk_div_tx1);
	MDSS_PLL_REG_W(dp_res->phy_base,
			DP_PHY_MODE, phy_mode);


	pr_debug("%s: div=%d link2xclk_div_tx0=%x, link2xclk_div_tx1=%x\n",
			__func__, div, link2xclk_div_tx0, link2xclk_div_tx1);

	mdss_pll_resource_enable(dp_res, false);

	return rc;
}

int link2xclk_divsel_get_div(struct div_clk *clk)
{
	int rc;
	u32 div = 0, phy_mode;
	struct mdss_pll_resources *dp_res = clk->priv;

	rc = mdss_pll_resource_enable(dp_res, true);
	if (rc) {
		pr_err("Failed to enable dp_res resources\n");
		return rc;
	}

	phy_mode = MDSS_PLL_REG_R(dp_res->phy_base, DP_PHY_MODE);

	if (phy_mode & 0x48)
		pr_err("%s: DP PAR Rate not correct\n", __func__);

	if ((phy_mode & 0x3) == 1)
		div = 10;
	else if ((phy_mode & 0x3) == 0)
		div = 5;
	else
		pr_err("%s: unsupported div: %d\n", __func__, phy_mode);

	mdss_pll_resource_enable(dp_res, false);
	pr_debug("%s: phy_mode=%d, div=%d\n", __func__,
						phy_mode, div);

	return div;
}

int hsclk_divsel_set_div(struct div_clk *clk, int div)
{
	int rc;
	u32 hsclk_div;
	struct mdss_pll_resources *dp_res = clk->priv;

	rc = mdss_pll_resource_enable(dp_res, true);
	if (rc) {
		pr_err("Failed to enable mdss DP PLL resources\n");
		return rc;
	}

	hsclk_div = MDSS_PLL_REG_R(dp_res->pll_base, QSERDES_COM_HSCLK_SEL);
	hsclk_div &= ~0x0f;	/* bits 0 to 3 */

	if (div == 3)
		hsclk_div |= 4;
	else
		hsclk_div |= 0;

	MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_HSCLK_SEL, hsclk_div);

	pr_debug("%s: div=%d hsclk_div=%x\n", __func__, div, hsclk_div);

	mdss_pll_resource_enable(dp_res, false);

	return rc;
}

int hsclk_divsel_get_div(struct div_clk *clk)
{
	int rc;
	u32 hsclk_div, div;
	struct mdss_pll_resources *dp_res = clk->priv;

	rc = mdss_pll_resource_enable(dp_res, true);
	if (rc) {
		pr_err("Failed to enable dp_res resources\n");
		return rc;
	}

	hsclk_div = MDSS_PLL_REG_R(dp_res->pll_base, QSERDES_COM_HSCLK_SEL);
	hsclk_div &= 0x0f;

	if (hsclk_div == 4)
		div = 3;
	else
		div = 2;

	mdss_pll_resource_enable(dp_res, false);

	pr_debug("%s: hsclk_div:%d, div=%d\n", __func__, hsclk_div, div);

	return div;
}

int vco_divided_clk_set_div(struct div_clk *clk, int div)
{
	int rc;
	u32 auxclk_div;
	struct mdss_pll_resources *dp_res = clk->priv;

	rc = mdss_pll_resource_enable(dp_res, true);
	if (rc) {
		pr_err("Failed to enable mdss DP PLL resources\n");
		return rc;
	}

	auxclk_div = MDSS_PLL_REG_R(dp_res->pll_base, DP_PHY_VCO_DIV);
	auxclk_div &= ~0x03;	/* bits 0 to 1 */
	if (div == 4)
		auxclk_div |= 2;
	else if (div == 2)
		auxclk_div |= 1;
	else
		auxclk_div |= 2; /* Default divider */

	MDSS_PLL_REG_W(dp_res->pll_base,
			DP_PHY_VCO_DIV, auxclk_div);

	pr_debug("%s: div=%d auxclk_div=%x\n", __func__, div, auxclk_div);

	mdss_pll_resource_enable(dp_res, false);

	return rc;
}


enum handoff vco_divided_clk_handoff(struct clk *c)
{
	/*
	 * Since cont-splash is not enabled, disable handoff
	 * for vco_divider_clk.
	*/
	return HANDOFF_DISABLED_CLK;
}

int vco_divided_clk_get_div(struct div_clk *clk)
{
	int rc;
	u32 div, auxclk_div;
	struct mdss_pll_resources *dp_res = clk->priv;

	rc = mdss_pll_resource_enable(dp_res, true);
	if (rc) {
		pr_err("Failed to enable dp_res resources\n");
		return rc;
	}

	auxclk_div = MDSS_PLL_REG_R(dp_res->pll_base, DP_PHY_VCO_DIV);
	auxclk_div &= 0x03;

	if (auxclk_div == 2)
		div = 4;
	else if (auxclk_div == 1)
		div = 2;
	else
		div = 0;

	mdss_pll_resource_enable(dp_res, false);

	pr_debug("%s: auxclk_div=%d, div=%d\n", __func__, auxclk_div, div);

	return div;
}

int dp_config_vco_rate(struct dp_pll_vco_clk *vco, unsigned long rate)
{
	u32 res = 0;
	struct mdss_pll_resources *dp_res = vco->priv;

	MDSS_PLL_REG_W(dp_res->phy_base,
			DP_PHY_PD_CTL, 0x3d);
	MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_PLL_IVCO, 0x0f);
	MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_SVS_MODE_CLK_SEL, 0x01);
	MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_SYSCLK_EN_SEL, 0x37);
	MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_SYS_CLK_CTRL, 0x06);

	MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_CLK_ENABLE1, 0x0e);
	MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_SYSCLK_BUF_ENABLE, 0x06);
	MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_CLK_SEL, 0x30);

	MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_LOCK_CMP_EN, 0x00);
	MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_PLL_CCTRL_MODE0, 0x34);
	MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_PLL_RCTRL_MODE0, 0x16);
	MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_CP_CTRL_MODE0, 0x08);
	/* Different for each clock rates */
	if (rate == DP_VCO_RATE_8100MHz) {
		MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_DEC_START_MODE0, 0x69);
		MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_DIV_FRAC_START1_MODE0, 0x00);
		MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_DIV_FRAC_START2_MODE0, 0x80);
		MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_DIV_FRAC_START3_MODE0, 0x07);
	} else if (rate == DP_VCO_RATE_9720MHz) {
		MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_DEC_START_MODE0, 0x7e);
		MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_DIV_FRAC_START1_MODE0, 0x00);
		MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_DIV_FRAC_START2_MODE0, 0x00);
		MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_DIV_FRAC_START3_MODE0, 0x09);
	} else if (rate == DP_VCO_RATE_10800MHz) {
		MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_DEC_START_MODE0, 0x8c);
		MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_DIV_FRAC_START1_MODE0, 0x00);
		MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_DIV_FRAC_START2_MODE0, 0x00);
		MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_DIV_FRAC_START3_MODE0, 0x0a);
	} else {
		pr_err("%s: unsupported rate: %ld\n", __func__, rate);
		return -EINVAL;
	}

	MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_INTEGLOOP_GAIN0_MODE0, 0x3f);
	MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_INTEGLOOP_GAIN1_MODE0, 0x00);
	MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_VCO_TUNE_MAP, 0x00);

	if (rate == DP_VCO_RATE_8100MHz) {
		MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_LOCK_CMP1_MODE0, 0x3f);
		MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_LOCK_CMP2_MODE0, 0x38);
		MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_LOCK_CMP3_MODE0, 0x00);
	} else if (rate == DP_VCO_RATE_9720MHz) {
		MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_LOCK_CMP1_MODE0, 0x7f);
		MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_LOCK_CMP2_MODE0, 0x43);
		MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_LOCK_CMP3_MODE0, 0x00);
	} else {
		MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_LOCK_CMP1_MODE0, 0x7f);
		MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_LOCK_CMP2_MODE0, 0x70);
		MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_LOCK_CMP3_MODE0, 0x00);
	}

	MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_BG_TIMER, 0x00);
	MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_BG_TIMER, 0x0a);
	MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_CORECLK_DIV_MODE0, 0x05);
	MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_VCO_TUNE_CTRL, 0x00);
	MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_BIAS_EN_CLKBUFLR_EN, 0x37);
	MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_CORE_CLK_EN, 0x0f);

	/* Different for each clock rate */
	if (rate == DP_VCO_RATE_8100MHz) {
		MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_CMN_CONFIG, 0x02);
	} else if (rate == DP_VCO_RATE_9720MHz) {
		MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_CMN_CONFIG, 0x02);
	} else {
		MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_CMN_CONFIG, 0x02);
	}

	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX0_OFFSET + TXn_TRANSCEIVER_BIAS_EN,
			0x1a);
	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX1_OFFSET + TXn_TRANSCEIVER_BIAS_EN,
			0x1a);
	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX0_OFFSET + TXn_HIGHZ_DRVR_EN,
			0x00);
	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX1_OFFSET + TXn_HIGHZ_DRVR_EN,
			0x00);

	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX0_OFFSET + TXn_TX_DRV_LVL,
			0x38);
	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX1_OFFSET + TXn_TX_DRV_LVL,
			0x38);
	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX0_OFFSET + TXn_TX_EMP_POST1_LVL,
			0x2c);
	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX1_OFFSET + TXn_TX_EMP_POST1_LVL,
			0x2c);

	MDSS_PLL_REG_W(dp_res->phy_base,
			DP_PHY_TX0_TX1_LANE_CTL, 0x05);
	MDSS_PLL_REG_W(dp_res->phy_base,
			DP_PHY_TX2_TX3_LANE_CTL, 0x05);
	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX0_OFFSET + TXn_VMODE_CTRL1,
			0x40);
	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX1_OFFSET + TXn_VMODE_CTRL1,
			0x40);
	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX0_OFFSET + TXn_PRE_STALL_LDO_BOOST_EN,
			0x30);
	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX1_OFFSET + TXn_PRE_STALL_LDO_BOOST_EN,
			0x30);
	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX0_OFFSET + TXn_INTERFACE_SELECT,
			0x3d);
	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX1_OFFSET + TXn_INTERFACE_SELECT,
			0x3d);
	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX0_OFFSET + TXn_CLKBUF_ENABLE,
			0x0f);
	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX1_OFFSET + TXn_CLKBUF_ENABLE,
			0x0f);
	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX0_OFFSET + TXn_RESET_TSYNC_EN,
			0x03);
	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX1_OFFSET + TXn_RESET_TSYNC_EN,
			0x03);
	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX0_OFFSET + TXn_TRAN_DRVR_EMP_EN,
			0x03);
	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX1_OFFSET + TXn_TRAN_DRVR_EMP_EN,
			0x03);
	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX0_OFFSET + TXn_PARRATE_REC_DETECT_IDLE_EN,
			0x00);
	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX1_OFFSET + TXn_PARRATE_REC_DETECT_IDLE_EN,
			0x00);
	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX0_OFFSET + TXn_TX_INTERFACE_MODE,
			0x00);
	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX1_OFFSET + TXn_TX_INTERFACE_MODE,
			0x00);
	return res;
}

static bool dp_pll_lock_status(struct mdss_pll_resources *dp_res)
{
	u32 status;
	bool pll_locked;

	/* poll for PLL ready status */
	if (readl_poll_timeout_atomic((dp_res->pll_base +
			QSERDES_COM_C_READY_STATUS),
			status,
			((status & BIT(0)) > 0),
			DP_PLL_POLL_SLEEP_US,
			DP_PLL_POLL_TIMEOUT_US)) {
		pr_err("%s: C_READY status is not high. Status=%x\n",
				__func__, status);
		pll_locked = false;
	} else if (readl_poll_timeout_atomic((dp_res->pll_base +
			DP_PHY_STATUS),
			status,
			((status & BIT(1)) > 0),
			DP_PLL_POLL_SLEEP_US,
			DP_PLL_POLL_TIMEOUT_US)) {
		pr_err("%s: Phy_ready is not high. Status=%x\n",
				__func__, status);
		pll_locked = false;
	} else {
		pll_locked = true;
	}

	return pll_locked;
}


static int dp_pll_enable(struct clk *c)
{
	int rc = 0;
	u32 status;
	struct dp_pll_vco_clk *vco = mdss_dp_to_vco_clk(c);
	struct mdss_pll_resources *dp_res = vco->priv;

	MDSS_PLL_REG_W(dp_res->phy_base,
			DP_PHY_CFG, 0x01);
	MDSS_PLL_REG_W(dp_res->phy_base,
			DP_PHY_CFG, 0x05);
	MDSS_PLL_REG_W(dp_res->phy_base,
			DP_PHY_CFG, 0x01);
	MDSS_PLL_REG_W(dp_res->phy_base,
			DP_PHY_CFG, 0x09);
	MDSS_PLL_REG_W(dp_res->pll_base,
			QSERDES_COM_RESETSM_CNTRL, 0x20);

	/* poll for PLL ready status */
	if (readl_poll_timeout_atomic((dp_res->pll_base +
			QSERDES_COM_C_READY_STATUS),
			status,
			((status & BIT(0)) > 0),
			DP_PLL_POLL_SLEEP_US,
			DP_PLL_POLL_TIMEOUT_US)) {
		pr_err("%s: C_READY status is not high. Status=%x\n",
				__func__, status);
		rc = -EINVAL;
		goto lock_err;
	}

	MDSS_PLL_REG_W(dp_res->phy_base,
			DP_PHY_CFG, 0x19);

	/* poll for PHY ready status */
	if (readl_poll_timeout_atomic((dp_res->phy_base +
			DP_PHY_STATUS),
			status,
			((status & BIT(1)) > 0),
			DP_PLL_POLL_SLEEP_US,
			DP_PLL_POLL_TIMEOUT_US)) {
		pr_err("%s: Phy_ready is not high. Status=%x\n",
				__func__, status);
		rc = -EINVAL;
		goto lock_err;
	}

	pr_debug("%s: PLL is locked\n", __func__);

	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX0_OFFSET + TXn_TRANSCEIVER_BIAS_EN,
			0x3f);
	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX1_OFFSET + TXn_TRANSCEIVER_BIAS_EN,
			0x3f);
	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX0_OFFSET + TXn_HIGHZ_DRVR_EN,
			0x10);
	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX1_OFFSET + TXn_HIGHZ_DRVR_EN,
			0x10);
	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX0_OFFSET + TXn_TX_POL_INV,
			0x0a);
	MDSS_PLL_REG_W(dp_res->phy_base,
			QSERDES_TX1_OFFSET + TXn_TX_POL_INV,
			0x0a);
	MDSS_PLL_REG_W(dp_res->phy_base,
			DP_PHY_CFG, 0x18);
	MDSS_PLL_REG_W(dp_res->phy_base,
			DP_PHY_CFG, 0x19);

	/*
	 * Make sure all the register writes are completed before
	 * doing any other operation
	 */
	wmb();

lock_err:
	return rc;
}

static int dp_pll_disable(struct clk *c)
{
	int rc = 0;
	struct dp_pll_vco_clk *vco = mdss_dp_to_vco_clk(c);
	struct mdss_pll_resources *dp_res = vco->priv;

	/* Assert DP PHY power down */
	MDSS_PLL_REG_W(dp_res->phy_base,
			DP_PHY_PD_CTL, 0x3c);
	/*
	 * Make sure all the register writes to disable PLL are
	 * completed before doing any other operation
	 */
	wmb();

	return rc;
}


int dp_vco_prepare(struct clk *c)
{
	int rc = 0;
	struct dp_pll_vco_clk *vco = mdss_dp_to_vco_clk(c);
	struct mdss_pll_resources *dp_pll_res = vco->priv;

	DEV_DBG("rate=%ld\n", vco->rate);
	rc = mdss_pll_resource_enable(dp_pll_res, true);
	if (rc) {
		pr_err("Failed to enable mdss DP pll resources\n");
		goto error;
	}

	rc = dp_pll_enable(c);
	if (rc) {
		mdss_pll_resource_enable(dp_pll_res, false);
		pr_err("ndx=%d failed to enable dsi pll\n",
					dp_pll_res->index);
		goto error;
	}

	mdss_pll_resource_enable(dp_pll_res, false);
error:
	return rc;
}

void dp_vco_unprepare(struct clk *c)
{
	struct dp_pll_vco_clk *vco = mdss_dp_to_vco_clk(c);
	struct mdss_pll_resources *io = vco->priv;

	if (!io) {
		DEV_ERR("Invalid input parameter\n");
		return;
	}

	if (!io->pll_on &&
		mdss_pll_resource_enable(io, true)) {
		DEV_ERR("pll resource can't be enabled\n");
		return;
	}
	dp_pll_disable(c);

	io->handoff_resources = false;
	mdss_pll_resource_enable(io, false);
	io->pll_on = false;
}

int dp_vco_set_rate(struct clk *c, unsigned long rate)
{
	struct dp_pll_vco_clk *vco = mdss_dp_to_vco_clk(c);
	struct mdss_pll_resources *io = vco->priv;
	int rc;

	rc = mdss_pll_resource_enable(io, true);
	if (rc) {
		DEV_ERR("pll resource can't be enabled\n");
		return rc;
	}

	DEV_DBG("DP lane CLK rate=%ld\n", rate);

	rc = dp_config_vco_rate(vco, rate);
	if (rc)
		DEV_ERR("%s: Failed to set clk rate\n", __func__);

	mdss_pll_resource_enable(io, false);

	vco->rate = rate;

	return 0;
}

unsigned long dp_vco_get_rate(struct clk *c)
{
	struct dp_pll_vco_clk *vco = mdss_dp_to_vco_clk(c);
	int rc;
	u32 div, hsclk_div, link2xclk_div;
	u64 vco_rate;
	struct mdss_pll_resources *pll = vco->priv;

	rc = mdss_pll_resource_enable(pll, true);
	if (rc) {
		pr_err("Failed to enable mdss DP pll=%d\n", pll->index);
		return rc;
	}

	div = MDSS_PLL_REG_R(pll->pll_base, QSERDES_COM_HSCLK_SEL);
	div &= 0x0f;

	if (div == 4)
		hsclk_div = 3;
	else
		hsclk_div = 2;

	div = MDSS_PLL_REG_R(pll->phy_base, DP_PHY_MODE);

	if (div & 0x48)
		pr_err("%s: DP PAR Rate not correct\n", __func__);

	if ((div & 0x3) == 1)
		link2xclk_div = 10;
	else if ((div & 0x3) == 0)
		link2xclk_div = 5;
	else
		pr_err("%s: unsupported div. Phy_mode: %d\n", __func__, div);

	if (link2xclk_div == 10) {
		vco_rate = DP_VCO_RATE_9720MHz;
	} else {
		if (hsclk_div == 3)
			vco_rate = DP_VCO_RATE_8100MHz;
		else
			vco_rate = DP_VCO_RATE_10800MHz;
	}

	pr_debug("returning vco rate = %lu\n", (unsigned long)vco_rate);

	mdss_pll_resource_enable(pll, false);

	return (unsigned long)vco_rate;
}

long dp_vco_round_rate(struct clk *c, unsigned long rate)
{
	unsigned long rrate = rate;
	struct dp_pll_vco_clk *vco = mdss_dp_to_vco_clk(c);

	if (rate <= vco->min_rate)
		rrate = vco->min_rate;
	else if (rate <= DP_VCO_RATE_9720MHz)
		rrate = DP_VCO_RATE_9720MHz;
	else
		rrate = vco->max_rate;

	pr_debug("%s: rrate=%ld\n", __func__, rrate);

	return rrate;
}

enum handoff dp_vco_handoff(struct clk *c)
{
	enum handoff ret = HANDOFF_DISABLED_CLK;
	struct dp_pll_vco_clk *vco = mdss_dp_to_vco_clk(c);
	struct mdss_pll_resources *io = vco->priv;

	if (mdss_pll_resource_enable(io, true)) {
		DEV_ERR("pll resource can't be enabled\n");
		return ret;
	}

	if (dp_pll_lock_status(io)) {
		io->pll_on = true;
		c->rate = dp_vco_get_rate(c);
		io->handoff_resources = true;
		ret = HANDOFF_ENABLED_CLK;
	} else {
		io->handoff_resources = false;
		mdss_pll_resource_enable(io, false);
		DEV_DBG("%s: PLL not locked\n", __func__);
	}

	DEV_DBG("done, ret=%d\n", ret);
	return ret;
}
