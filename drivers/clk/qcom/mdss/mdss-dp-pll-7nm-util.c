/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"[dp-pll] %s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/iopoll.h>
#include <linux/delay.h>
#include <linux/usb/usbpd.h>

#include "mdss-pll.h"
#include "mdss-dp-pll.h"
#include "mdss-dp-pll-7nm.h"

#define DP_PHY_CFG				0x0010
#define DP_PHY_PD_CTL				0x0018
#define DP_PHY_MODE				0x001C

#define DP_PHY_AUX_CFG2				0x0028

#define DP_PHY_VCO_DIV				0x0070
#define DP_PHY_TX0_TX1_LANE_CTL			0x0078
#define DP_PHY_TX2_TX3_LANE_CTL			0x009C

#define DP_PHY_SPARE0				0x00C8
#define DP_PHY_STATUS				0x00DC

/* Tx registers */
#define TXn_CLKBUF_ENABLE			0x0008
#define TXn_TX_EMP_POST1_LVL			0x000C

#define TXn_TX_DRV_LVL				0x0014

#define TXn_RESET_TSYNC_EN			0x001C
#define TXn_PRE_STALL_LDO_BOOST_EN		0x0020
#define TXn_TX_BAND				0x0024
#define TXn_INTERFACE_SELECT			0x002C

#define TXn_RES_CODE_LANE_OFFSET_TX		0x003C
#define TXn_RES_CODE_LANE_OFFSET_RX		0x0040

#define TXn_TRANSCEIVER_BIAS_EN			0x0054
#define TXn_HIGHZ_DRVR_EN			0x0058
#define TXn_TX_POL_INV				0x005C
#define TXn_PARRATE_REC_DETECT_IDLE_EN		0x0060

#define TXn_TRAN_DRVR_EMP_EN			0x00B8
#define TXn_TX_INTERFACE_MODE			0x00BC

#define TXn_VMODE_CTRL1				0x00E8

/* PLL register offset */
#define QSERDES_COM_BG_TIMER			0x000C
#define QSERDES_COM_BIAS_EN_CLKBUFLR_EN		0x0044
#define QSERDES_COM_CLK_ENABLE1			0x0048
#define QSERDES_COM_SYS_CLK_CTRL		0x004C
#define QSERDES_COM_SYSCLK_BUF_ENABLE		0x0050
#define QSERDES_COM_PLL_IVCO			0x0058

#define QSERDES_COM_CP_CTRL_MODE0		0x0074
#define QSERDES_COM_PLL_RCTRL_MODE0		0x007C
#define QSERDES_COM_PLL_CCTRL_MODE0		0x0084
#define QSERDES_COM_SYSCLK_EN_SEL		0x0094
#define QSERDES_COM_RESETSM_CNTRL		0x009C
#define QSERDES_COM_LOCK_CMP_EN			0x00A4
#define QSERDES_COM_LOCK_CMP1_MODE0		0x00AC
#define QSERDES_COM_LOCK_CMP2_MODE0		0x00B0

#define QSERDES_COM_DEC_START_MODE0		0x00BC
#define QSERDES_COM_DIV_FRAC_START1_MODE0	0x00CC
#define QSERDES_COM_DIV_FRAC_START2_MODE0	0x00D0
#define QSERDES_COM_DIV_FRAC_START3_MODE0	0x00D4
#define QSERDES_COM_INTEGLOOP_GAIN0_MODE0	0x00EC
#define QSERDES_COM_INTEGLOOP_GAIN1_MODE0	0x00F0
#define QSERDES_COM_VCO_TUNE_CTRL		0x0108
#define QSERDES_COM_VCO_TUNE_MAP		0x010C

#define QSERDES_COM_CLK_SEL			0x0154
#define QSERDES_COM_HSCLK_SEL			0x0158

#define QSERDES_COM_CORECLK_DIV_MODE0		0x0168

#define QSERDES_COM_CORE_CLK_EN			0x0174
#define QSERDES_COM_C_READY_STATUS		0x0178
#define QSERDES_COM_CMN_CONFIG			0x017C

#define QSERDES_COM_SVS_MODE_CLK_SEL		0x0184

#define DP_PHY_PLL_POLL_SLEEP_US		500
#define DP_PHY_PLL_POLL_TIMEOUT_US		10000

#define DP_VCO_RATE_8100MHZDIV1000		8100000UL
#define DP_VCO_RATE_9720MHZDIV1000		9720000UL
#define DP_VCO_RATE_10800MHZDIV1000		10800000UL

int dp_mux_set_parent_7nm(void *context, unsigned int reg, unsigned int val)
{
	struct mdss_pll_resources *dp_res = context;
	int rc;
	u32 auxclk_div;

	if (!context) {
		pr_err("invalid input parameters\n");
		return -EINVAL;
	}

	rc = mdss_pll_resource_enable(dp_res, true);
	if (rc) {
		pr_err("Failed to enable mdss DP PLL resources\n");
		return rc;
	}

	auxclk_div = MDSS_PLL_REG_R(dp_res->phy_base, DP_PHY_VCO_DIV);
	auxclk_div &= ~0x03;

	if (val == 0)
		auxclk_div |= 1;
	else if (val == 1)
		auxclk_div |= 2;
	else if (val == 2)
		auxclk_div |= 0;

	MDSS_PLL_REG_W(dp_res->phy_base, DP_PHY_VCO_DIV, auxclk_div);
	/* Make sure the PHY registers writes are done */
	wmb();
	pr_debug("mux=%d auxclk_div=%x\n", val, auxclk_div);

	mdss_pll_resource_enable(dp_res, false);

	return 0;
}

int dp_mux_get_parent_7nm(void *context, unsigned int reg, unsigned int *val)
{
	int rc;
	u32 auxclk_div = 0;
	struct mdss_pll_resources *dp_res = context;

	if (!context || !val) {
		pr_err("invalid input parameters\n");
		return -EINVAL;
	}

	rc = mdss_pll_resource_enable(dp_res, true);
	if (rc) {
		pr_err("Failed to enable dp_res resources\n");
		return rc;
	}

	auxclk_div = MDSS_PLL_REG_R(dp_res->phy_base, DP_PHY_VCO_DIV);
	auxclk_div &= 0x03;

	if (auxclk_div == 1) /* Default divider */
		*val = 0;
	else if (auxclk_div == 2)
		*val = 1;
	else if (auxclk_div == 0)
		*val = 2;

	mdss_pll_resource_enable(dp_res, false);

	pr_debug("auxclk_div=%d, val=%d\n", auxclk_div, *val);

	return 0;
}

static int dp_vco_pll_init_db_7nm(struct dp_pll_db_7nm *pdb,
		unsigned long rate)
{
	struct mdss_pll_resources *dp_res = pdb->pll;
	u32 spare_value = 0;

	spare_value = MDSS_PLL_REG_R(dp_res->phy_base, DP_PHY_SPARE0);
	pdb->lane_cnt = spare_value & 0x0F;
	pdb->orientation = (spare_value & 0xF0) >> 4;

	pr_debug("spare_value=0x%x, ln_cnt=0x%x, orientation=0x%x\n",
			spare_value, pdb->lane_cnt, pdb->orientation);

	pdb->div_frac_start1_mode0 = 0x00;
	pdb->integloop_gain0_mode0 = 0x3f;
	pdb->integloop_gain1_mode0 = 0x00;
	pdb->vco_tune_map = 0x00;
	pdb->cmn_config = 0x02;
	pdb->txn_tran_drv_emp_en = 0xf;

	switch (rate) {
	case DP_VCO_HSCLK_RATE_1620MHZDIV1000:
		pr_debug("VCO rate: %ld\n", DP_VCO_RATE_9720MHZDIV1000);
		pdb->hsclk_sel = 0x05;
		pdb->dec_start_mode0 = 0x69;
		pdb->div_frac_start2_mode0 = 0x80;
		pdb->div_frac_start3_mode0 = 0x07;
		pdb->lock_cmp1_mode0 = 0x6f;
		pdb->lock_cmp2_mode0 = 0x08;
		pdb->phy_vco_div = 0x1;
		pdb->lock_cmp_en = 0x04;
		break;
	case DP_VCO_HSCLK_RATE_2700MHZDIV1000:
		pr_debug("VCO rate: %ld\n", DP_VCO_RATE_10800MHZDIV1000);
		pdb->hsclk_sel = 0x03;
		pdb->dec_start_mode0 = 0x69;
		pdb->div_frac_start2_mode0 = 0x80;
		pdb->div_frac_start3_mode0 = 0x07;
		pdb->lock_cmp1_mode0 = 0x0f;
		pdb->lock_cmp2_mode0 = 0x0e;
		pdb->phy_vco_div = 0x1;
		pdb->lock_cmp_en = 0x08;
		break;
	case DP_VCO_HSCLK_RATE_5400MHZDIV1000:
		pr_debug("VCO rate: %ld\n", DP_VCO_RATE_10800MHZDIV1000);
		pdb->hsclk_sel = 0x01;
		pdb->dec_start_mode0 = 0x8c;
		pdb->div_frac_start2_mode0 = 0x00;
		pdb->div_frac_start3_mode0 = 0x0a;
		pdb->lock_cmp1_mode0 = 0x1f;
		pdb->lock_cmp2_mode0 = 0x1c;
		pdb->phy_vco_div = 0x2;
		pdb->lock_cmp_en = 0x08;
		break;
	case DP_VCO_HSCLK_RATE_8100MHZDIV1000:
		pr_debug("VCO rate: %ld\n", DP_VCO_RATE_8100MHZDIV1000);
		pdb->hsclk_sel = 0x00;
		pdb->dec_start_mode0 = 0x69;
		pdb->div_frac_start2_mode0 = 0x80;
		pdb->div_frac_start3_mode0 = 0x07;
		pdb->lock_cmp1_mode0 = 0x2f;
		pdb->lock_cmp2_mode0 = 0x2a;
		pdb->phy_vco_div = 0x0;
		pdb->lock_cmp_en = 0x08;
		break;
	default:
		pr_err("unsupported rate %ld\n", rate);
		return -EINVAL;
	}
	return 0;
}

static int dp_config_vco_rate_7nm(struct dp_pll_vco_clk *vco,
		unsigned long rate)
{
	u32 res = 0;
	struct mdss_pll_resources *dp_res = vco->priv;
	struct dp_pll_db_7nm *pdb = (struct dp_pll_db_7nm *)dp_res->priv;

	res = dp_vco_pll_init_db_7nm(pdb, rate);
	if (res) {
		pr_err("VCO Init DB failed\n");
		return res;
	}

	if (pdb->lane_cnt != 4) {
		if (pdb->orientation == ORIENTATION_CC2)
			MDSS_PLL_REG_W(dp_res->phy_base, DP_PHY_PD_CTL, 0x6d);
		else
			MDSS_PLL_REG_W(dp_res->phy_base, DP_PHY_PD_CTL, 0x75);
	} else {
		MDSS_PLL_REG_W(dp_res->phy_base, DP_PHY_PD_CTL, 0x7d);
	}

	/* Make sure the PHY register writes are done */
	wmb();

	MDSS_PLL_REG_W(dp_res->pll_base, QSERDES_COM_SVS_MODE_CLK_SEL, 0x05);
	MDSS_PLL_REG_W(dp_res->pll_base, QSERDES_COM_SYSCLK_EN_SEL, 0x3b);
	MDSS_PLL_REG_W(dp_res->pll_base, QSERDES_COM_SYS_CLK_CTRL, 0x02);
	MDSS_PLL_REG_W(dp_res->pll_base, QSERDES_COM_CLK_ENABLE1, 0x0c);
	MDSS_PLL_REG_W(dp_res->pll_base, QSERDES_COM_SYSCLK_BUF_ENABLE, 0x06);
	MDSS_PLL_REG_W(dp_res->pll_base, QSERDES_COM_CLK_SEL, 0x30);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_HSCLK_SEL, pdb->hsclk_sel);
	MDSS_PLL_REG_W(dp_res->pll_base, QSERDES_COM_PLL_IVCO, 0x0f);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_LOCK_CMP_EN, pdb->lock_cmp_en);
	MDSS_PLL_REG_W(dp_res->pll_base, QSERDES_COM_PLL_CCTRL_MODE0, 0x36);
	MDSS_PLL_REG_W(dp_res->pll_base, QSERDES_COM_PLL_RCTRL_MODE0, 0x16);
	MDSS_PLL_REG_W(dp_res->pll_base, QSERDES_COM_CP_CTRL_MODE0, 0x06);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_DEC_START_MODE0, pdb->dec_start_mode0);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_DIV_FRAC_START1_MODE0, pdb->div_frac_start1_mode0);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_DIV_FRAC_START2_MODE0, pdb->div_frac_start2_mode0);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_DIV_FRAC_START3_MODE0, pdb->div_frac_start3_mode0);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_CMN_CONFIG, pdb->cmn_config);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_INTEGLOOP_GAIN0_MODE0, pdb->integloop_gain0_mode0);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_INTEGLOOP_GAIN1_MODE0, pdb->integloop_gain1_mode0);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_VCO_TUNE_MAP, pdb->vco_tune_map);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_LOCK_CMP1_MODE0, pdb->lock_cmp1_mode0);
	MDSS_PLL_REG_W(dp_res->pll_base,
		QSERDES_COM_LOCK_CMP2_MODE0, pdb->lock_cmp2_mode0);
	/* Make sure the PLL register writes are done */
	wmb();

	MDSS_PLL_REG_W(dp_res->pll_base, QSERDES_COM_BG_TIMER, 0x0a);
	MDSS_PLL_REG_W(dp_res->pll_base, QSERDES_COM_CORECLK_DIV_MODE0, 0x0a);
	MDSS_PLL_REG_W(dp_res->pll_base, QSERDES_COM_VCO_TUNE_CTRL, 0x00);
	MDSS_PLL_REG_W(dp_res->pll_base, QSERDES_COM_BIAS_EN_CLKBUFLR_EN, 0x17);
	MDSS_PLL_REG_W(dp_res->pll_base, QSERDES_COM_CORE_CLK_EN, 0x1f);
	/* Make sure the PHY register writes are done */
	wmb();

	if (pdb->orientation == ORIENTATION_CC2)
		MDSS_PLL_REG_W(dp_res->phy_base, DP_PHY_MODE, 0x4c);
	else
		MDSS_PLL_REG_W(dp_res->phy_base, DP_PHY_MODE, 0x5c);
	/* Make sure the PLL register writes are done */
	wmb();

	/* TX Lane configuration */
	MDSS_PLL_REG_W(dp_res->phy_base, DP_PHY_TX0_TX1_LANE_CTL, 0x05);
	MDSS_PLL_REG_W(dp_res->phy_base, DP_PHY_TX2_TX3_LANE_CTL, 0x05);

	/* TX-0 register configuration */
	MDSS_PLL_REG_W(dp_res->ln_tx0_base, TXn_TRANSCEIVER_BIAS_EN, 0x1a);
	MDSS_PLL_REG_W(dp_res->ln_tx0_base, TXn_VMODE_CTRL1, 0x40);
	MDSS_PLL_REG_W(dp_res->ln_tx0_base, TXn_PRE_STALL_LDO_BOOST_EN, 0x30);
	MDSS_PLL_REG_W(dp_res->ln_tx0_base, TXn_INTERFACE_SELECT, 0x3b);
	MDSS_PLL_REG_W(dp_res->ln_tx0_base, TXn_CLKBUF_ENABLE, 0x0f);
	MDSS_PLL_REG_W(dp_res->ln_tx0_base, TXn_RESET_TSYNC_EN, 0x03);
	MDSS_PLL_REG_W(dp_res->ln_tx0_base, TXn_TRAN_DRVR_EMP_EN,
		pdb->txn_tran_drv_emp_en);
	MDSS_PLL_REG_W(dp_res->ln_tx0_base,
		TXn_PARRATE_REC_DETECT_IDLE_EN, 0x00);
	MDSS_PLL_REG_W(dp_res->ln_tx0_base, TXn_TX_INTERFACE_MODE, 0x00);
	MDSS_PLL_REG_W(dp_res->ln_tx0_base, TXn_TX_BAND, 0x4);

	/* TX-1 register configuration */
	MDSS_PLL_REG_W(dp_res->ln_tx1_base, TXn_TRANSCEIVER_BIAS_EN, 0x1a);
	MDSS_PLL_REG_W(dp_res->ln_tx1_base, TXn_VMODE_CTRL1, 0x40);
	MDSS_PLL_REG_W(dp_res->ln_tx1_base, TXn_PRE_STALL_LDO_BOOST_EN, 0x30);
	MDSS_PLL_REG_W(dp_res->ln_tx1_base, TXn_INTERFACE_SELECT, 0x3b);
	MDSS_PLL_REG_W(dp_res->ln_tx1_base, TXn_CLKBUF_ENABLE, 0x0f);
	MDSS_PLL_REG_W(dp_res->ln_tx1_base, TXn_RESET_TSYNC_EN, 0x03);
	MDSS_PLL_REG_W(dp_res->ln_tx1_base, TXn_TRAN_DRVR_EMP_EN,
		pdb->txn_tran_drv_emp_en);
	MDSS_PLL_REG_W(dp_res->ln_tx1_base,
		TXn_PARRATE_REC_DETECT_IDLE_EN, 0x00);
	MDSS_PLL_REG_W(dp_res->ln_tx1_base, TXn_TX_INTERFACE_MODE, 0x00);
	MDSS_PLL_REG_W(dp_res->ln_tx1_base, TXn_TX_BAND, 0x4);
	/* Make sure the PHY register writes are done */
	wmb();

	MDSS_PLL_REG_W(dp_res->phy_base, DP_PHY_VCO_DIV, pdb->phy_vco_div);

	return res;
}

static bool dp_7nm_pll_lock_status(struct mdss_pll_resources *dp_res)
{
	u32 status;
	bool pll_locked;

	if (readl_poll_timeout_atomic((dp_res->pll_base +
			QSERDES_COM_C_READY_STATUS),
			status,
			((status & BIT(0)) > 0),
			DP_PHY_PLL_POLL_SLEEP_US,
			DP_PHY_PLL_POLL_TIMEOUT_US)) {
		pr_err("C_READY status is not high. Status=%x\n", status);
		pll_locked = false;
	} else {
		pll_locked = true;
	}

	return pll_locked;
}

static bool dp_7nm_phy_rdy_status(struct mdss_pll_resources *dp_res)
{
	u32 status;
	bool phy_ready = true;

	/* poll for PHY ready status */
	if (readl_poll_timeout_atomic((dp_res->phy_base +
			DP_PHY_STATUS),
			status,
			((status & (BIT(1))) > 0),
			DP_PHY_PLL_POLL_SLEEP_US,
			DP_PHY_PLL_POLL_TIMEOUT_US)) {
		pr_err("Phy_ready is not high. Status=%x\n", status);
		phy_ready = false;
	}

	return phy_ready;
}

static int dp_pll_enable_7nm(struct clk_hw *hw)
{
	int rc = 0;
	struct dp_pll_vco_clk *vco = to_dp_vco_hw(hw);
	struct mdss_pll_resources *dp_res = vco->priv;
	struct dp_pll_db_7nm *pdb = (struct dp_pll_db_7nm *)dp_res->priv;
	u32 bias_en, drvr_en;

	MDSS_PLL_REG_W(dp_res->phy_base, DP_PHY_AUX_CFG2, 0x24);
	MDSS_PLL_REG_W(dp_res->phy_base, DP_PHY_CFG, 0x01);
	MDSS_PLL_REG_W(dp_res->phy_base, DP_PHY_CFG, 0x05);
	MDSS_PLL_REG_W(dp_res->phy_base, DP_PHY_CFG, 0x01);
	MDSS_PLL_REG_W(dp_res->phy_base, DP_PHY_CFG, 0x09);
	wmb(); /* Make sure the PHY register writes are done */

	MDSS_PLL_REG_W(dp_res->pll_base, QSERDES_COM_RESETSM_CNTRL, 0x20);
	wmb();	/* Make sure the PLL register writes are done */

	if (!dp_7nm_pll_lock_status(dp_res)) {
		rc = -EINVAL;
		goto lock_err;
	}

	MDSS_PLL_REG_W(dp_res->phy_base, DP_PHY_CFG, 0x19);
	/* Make sure the PHY register writes are done */
	wmb();
	/* poll for PHY ready status */
	if (!dp_7nm_phy_rdy_status(dp_res)) {
		rc = -EINVAL;
		goto lock_err;
	}

	pr_debug("PLL is locked\n");

	if (pdb->lane_cnt == 1) {
		bias_en = 0x3e;
		drvr_en = 0x13;
	} else {
		bias_en = 0x3f;
		drvr_en = 0x10;
	}

	if (pdb->lane_cnt != 4) {
		if (pdb->orientation == ORIENTATION_CC1) {
			MDSS_PLL_REG_W(dp_res->ln_tx1_base,
				TXn_HIGHZ_DRVR_EN, drvr_en);
			MDSS_PLL_REG_W(dp_res->ln_tx1_base,
				TXn_TRANSCEIVER_BIAS_EN, bias_en);
		} else {
			MDSS_PLL_REG_W(dp_res->ln_tx0_base,
				TXn_HIGHZ_DRVR_EN, drvr_en);
			MDSS_PLL_REG_W(dp_res->ln_tx0_base,
				TXn_TRANSCEIVER_BIAS_EN, bias_en);
		}
	} else {
		MDSS_PLL_REG_W(dp_res->ln_tx0_base, TXn_HIGHZ_DRVR_EN, drvr_en);
		MDSS_PLL_REG_W(dp_res->ln_tx0_base,
			TXn_TRANSCEIVER_BIAS_EN, bias_en);
		MDSS_PLL_REG_W(dp_res->ln_tx1_base, TXn_HIGHZ_DRVR_EN, drvr_en);
		MDSS_PLL_REG_W(dp_res->ln_tx1_base,
			TXn_TRANSCEIVER_BIAS_EN, bias_en);
	}

	MDSS_PLL_REG_W(dp_res->ln_tx0_base, TXn_TX_POL_INV, 0x0a);
	MDSS_PLL_REG_W(dp_res->ln_tx1_base, TXn_TX_POL_INV, 0x0a);
	MDSS_PLL_REG_W(dp_res->phy_base, DP_PHY_CFG, 0x18);
	udelay(2000);

	MDSS_PLL_REG_W(dp_res->phy_base, DP_PHY_CFG, 0x19);

	/*
	 * Make sure all the register writes are completed before
	 * doing any other operation
	 */
	wmb();

	/* poll for PHY ready status */
	if (!dp_7nm_phy_rdy_status(dp_res)) {
		rc = -EINVAL;
		goto lock_err;
	}

	MDSS_PLL_REG_W(dp_res->ln_tx0_base, TXn_TX_DRV_LVL, 0x3f);
	MDSS_PLL_REG_W(dp_res->ln_tx1_base, TXn_TX_DRV_LVL, 0x3f);
	MDSS_PLL_REG_W(dp_res->ln_tx0_base, TXn_TX_EMP_POST1_LVL, 0x23);
	MDSS_PLL_REG_W(dp_res->ln_tx1_base, TXn_TX_EMP_POST1_LVL, 0x23);
	MDSS_PLL_REG_W(dp_res->ln_tx0_base, TXn_RES_CODE_LANE_OFFSET_TX, 0x11);
	MDSS_PLL_REG_W(dp_res->ln_tx1_base, TXn_RES_CODE_LANE_OFFSET_TX, 0x11);
	MDSS_PLL_REG_W(dp_res->ln_tx0_base, TXn_RES_CODE_LANE_OFFSET_RX, 0x11);
	MDSS_PLL_REG_W(dp_res->ln_tx1_base, TXn_RES_CODE_LANE_OFFSET_RX, 0x11);
	MDSS_PLL_REG_W(dp_res->ln_tx0_base, TXn_INTERFACE_SELECT, 0x3b);
	MDSS_PLL_REG_W(dp_res->ln_tx1_base, TXn_INTERFACE_SELECT, 0x3b);
	/* Make sure the PHY register writes are done */
	wmb();

lock_err:
	return rc;
}

static int dp_pll_disable_7nm(struct clk_hw *hw)
{
	int rc = 0;
	struct dp_pll_vco_clk *vco = to_dp_vco_hw(hw);
	struct mdss_pll_resources *dp_res = vco->priv;

	/* Assert DP PHY power down */
	MDSS_PLL_REG_W(dp_res->phy_base, DP_PHY_PD_CTL, 0x2);
	/*
	 * Make sure all the register writes to disable PLL are
	 * completed before doing any other operation
	 */
	wmb();

	return rc;
}

int dp_vco_prepare_7nm(struct clk_hw *hw)
{
	int rc = 0;
	struct dp_pll_vco_clk *vco;
	struct mdss_pll_resources *dp_res;

	if (!hw) {
		pr_err("invalid input parameters\n");
		return -EINVAL;
	}

	vco = to_dp_vco_hw(hw);
	dp_res = vco->priv;

	pr_debug("rate=%ld\n", vco->rate);
	rc = mdss_pll_resource_enable(dp_res, true);
	if (rc) {
		pr_err("Failed to enable mdss DP pll resources\n");
		goto error;
	}

	if ((dp_res->vco_cached_rate != 0)
		&& (dp_res->vco_cached_rate == vco->rate)) {
		rc = vco->hw.init->ops->set_rate(hw,
			dp_res->vco_cached_rate, dp_res->vco_cached_rate);
		if (rc) {
			pr_err("index=%d vco_set_rate failed. rc=%d\n",
				rc, dp_res->index);
			mdss_pll_resource_enable(dp_res, false);
			goto error;
		}
	}

	rc = dp_pll_enable_7nm(hw);
	if (rc) {
		mdss_pll_resource_enable(dp_res, false);
		pr_err("ndx=%d failed to enable dp pll\n", dp_res->index);
		goto error;
	}

	mdss_pll_resource_enable(dp_res, false);
error:
	return rc;
}

void dp_vco_unprepare_7nm(struct clk_hw *hw)
{
	struct dp_pll_vco_clk *vco;
	struct mdss_pll_resources *dp_res;

	if (!hw) {
		pr_err("invalid input parameters\n");
		return;
	}

	vco = to_dp_vco_hw(hw);
	dp_res = vco->priv;

	if (!dp_res) {
		pr_err("invalid input parameter\n");
		return;
	}

	if (!dp_res->pll_on &&
		mdss_pll_resource_enable(dp_res, true)) {
		pr_err("pll resource can't be enabled\n");
		return;
	}
	dp_res->vco_cached_rate = vco->rate;
	dp_pll_disable_7nm(hw);

	dp_res->handoff_resources = false;
	mdss_pll_resource_enable(dp_res, false);
	dp_res->pll_on = false;
}

int dp_vco_set_rate_7nm(struct clk_hw *hw, unsigned long rate,
					unsigned long parent_rate)
{
	struct dp_pll_vco_clk *vco;
	struct mdss_pll_resources *dp_res;
	int rc;

	if (!hw) {
		pr_err("invalid input parameters\n");
		return -EINVAL;
	}

	vco = to_dp_vco_hw(hw);
	dp_res = vco->priv;

	rc = mdss_pll_resource_enable(dp_res, true);
	if (rc) {
		pr_err("pll resource can't be enabled\n");
		return rc;
	}

	pr_debug("DP lane CLK rate=%ld\n", rate);

	rc = dp_config_vco_rate_7nm(vco, rate);
	if (rc)
		pr_err("Failed to set clk rate\n");

	mdss_pll_resource_enable(dp_res, false);

	vco->rate = rate;

	return 0;
}

unsigned long dp_vco_recalc_rate_7nm(struct clk_hw *hw,
					unsigned long parent_rate)
{
	struct dp_pll_vco_clk *vco;
	int rc;
	u32 hsclk_sel, link_clk_divsel, hsclk_div, link_clk_div = 0;
	unsigned long vco_rate;
	struct mdss_pll_resources *dp_res;

	if (!hw) {
		pr_err("invalid input parameters\n");
		return 0;
	}

	vco = to_dp_vco_hw(hw);
	dp_res = vco->priv;

	rc = mdss_pll_resource_enable(dp_res, true);
	if (rc) {
		pr_err("Failed to enable mdss DP pll=%d\n", dp_res->index);
		return 0;
	}

	pr_debug("input rates: parent=%lu, vco=%lu\n", parent_rate, vco->rate);

	hsclk_sel = MDSS_PLL_REG_R(dp_res->pll_base, QSERDES_COM_HSCLK_SEL);
	hsclk_sel &= 0x0f;

	if (hsclk_sel == 5)
		hsclk_div = 5;
	else if (hsclk_sel == 3)
		hsclk_div = 3;
	else if (hsclk_sel == 1)
		hsclk_div = 2;
	else if (hsclk_sel == 0)
		hsclk_div = 1;
	else {
		pr_debug("unknown divider. forcing to default\n");
		hsclk_div = 5;
	}

	link_clk_divsel = MDSS_PLL_REG_R(dp_res->phy_base, DP_PHY_AUX_CFG2);
	link_clk_divsel >>= 2;
	link_clk_divsel &= 0x3;

	if (link_clk_divsel == 0)
		link_clk_div = 5;
	else if (link_clk_divsel == 1)
		link_clk_div = 10;
	else if (link_clk_divsel == 2)
		link_clk_div = 20;
	else
		pr_err("unsupported div. Phy_mode: %d\n", link_clk_divsel);

	if (link_clk_div == 20) {
		vco_rate = DP_VCO_HSCLK_RATE_2700MHZDIV1000;
	} else {
		if (hsclk_div == 5)
			vco_rate = DP_VCO_HSCLK_RATE_1620MHZDIV1000;
		else if (hsclk_div == 3)
			vco_rate = DP_VCO_HSCLK_RATE_2700MHZDIV1000;
		else if (hsclk_div == 2)
			vco_rate = DP_VCO_HSCLK_RATE_5400MHZDIV1000;
		else
			vco_rate = DP_VCO_HSCLK_RATE_8100MHZDIV1000;
	}

	pr_debug("hsclk: sel=0x%x, div=0x%x; lclk: sel=%lu, div=%lu, rate=%lu\n",
		hsclk_sel, hsclk_div, link_clk_divsel, link_clk_div, vco_rate);

	mdss_pll_resource_enable(dp_res, false);

	dp_res->vco_cached_rate = vco->rate = vco_rate;
	return vco_rate;
}

long dp_vco_round_rate_7nm(struct clk_hw *hw, unsigned long rate,
			unsigned long *parent_rate)
{
	unsigned long rrate = rate;
	struct dp_pll_vco_clk *vco;

	if (!hw) {
		pr_err("invalid input parameters\n");
		return 0;
	}

	vco = to_dp_vco_hw(hw);
	if (rate <= vco->min_rate)
		rrate = vco->min_rate;
	else if (rate <= DP_VCO_HSCLK_RATE_2700MHZDIV1000)
		rrate = DP_VCO_HSCLK_RATE_2700MHZDIV1000;
	else if (rate <= DP_VCO_HSCLK_RATE_5400MHZDIV1000)
		rrate = DP_VCO_HSCLK_RATE_5400MHZDIV1000;
	else
		rrate = vco->max_rate;

	pr_debug("rrate=%ld\n", rrate);

	if (parent_rate)
		*parent_rate = rrate;
	return rrate;
}

