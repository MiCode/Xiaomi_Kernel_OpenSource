// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 */

/*
 * Display Port PLL driver block diagram for branch clocks
 *
 *		+------------------------------+
 *		|         DP_VCO_CLK           |
 *		|                              |
 *		|    +-------------------+     |
 *		|    |   (DP PLL/VCO)    |     |
 *		|    +---------+---------+     |
 *		|              v               |
 *		|   +----------+-----------+   |
 *		|   | hsclk_divsel_clk_src |   |
 *		|   +----------+-----------+   |
 *		+------------------------------+
 *				|
 *	 +------------<---------v------------>----------+
 *	 |                                              |
 * +-----v------------+                                 |
 * | dp_link_clk_src  |                                 |
 * |    divsel_ten    |                                 |
 * +---------+--------+                                 |
 *	|                                               |
 *	|                                               |
 *	v                                               v
 * Input to DISPCC block                                |
 * for link clk, crypto clk                             |
 * and interface clock                                  |
 *							|
 *							|
 *	+--------<------------+-----------------+---<---+
 *	|                     |                 |
 * +-------v------+  +--------v-----+  +--------v------+
 * | vco_divided  |  | vco_divided  |  | vco_divided   |
 * |    _clk_src  |  |    _clk_src  |  |    _clk_src   |
 * |              |  |              |  |               |
 * |divsel_six    |  |  divsel_two  |  |  divsel_four  |
 * +-------+------+  +-----+--------+  +--------+------+
 *         |	           |		        |
 *	v------->----------v-------------<------v
 *                         |
 *		+----------+---------+
 *		|   vco_divided_clk  |
 *		|       _src_mux     |
 *		+---------+----------+
 *                        |
 *                        v
 *              Input to DISPCC block
 *              for DP pixel clock
 *
 */

#include <dt-bindings/clock/mdss-5nm-pll-clk.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/regmap.h>
#include "clk-regmap-mux.h"
#include "dp_hpd.h"
#include "dp_debug.h"
#include "dp_pll.h"

#define DP_PHY_CFG				0x0010
#define DP_PHY_CFG_1				0x0014
#define DP_PHY_PD_CTL				0x0018
#define DP_PHY_MODE				0x001C

#define DP_PHY_AUX_CFG1				0x0024
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

/* PLL register offset */
#define QSERDES_COM_BG_TIMER			0x000C
#define QSERDES_COM_SSC_EN_CENTER		0x0010
#define QSERDES_COM_SSC_ADJ_PER1		0x0014
#define QSERDES_COM_SSC_PER1			0x001C
#define QSERDES_COM_SSC_PER2			0x0020
#define QSERDES_COM_SSC_STEP_SIZE1_MODE0	0x0024
#define QSERDES_COM_SSC_STEP_SIZE2_MODE0	0X0028
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

#define QSERDES_COM_CMN_STATUS			0x0140
#define QSERDES_COM_CLK_SEL			0x0154
#define QSERDES_COM_HSCLK_SEL			0x0158

#define QSERDES_COM_CORECLK_DIV_MODE0		0x0168

#define QSERDES_COM_CORE_CLK_EN			0x0174
#define QSERDES_COM_C_READY_STATUS		0x0178
#define QSERDES_COM_CMN_CONFIG			0x017C

#define QSERDES_COM_SVS_MODE_CLK_SEL		0x0184

/* Tx tran offsets */
#define DP_TRAN_DRVR_EMP_EN			0x00C0
#define DP_TRAN_DRVR_EMP_EN_7nm			0x00B8
#define DP_TX_INTERFACE_MODE			0x00C4
#define DP_TX_INTERFACE_MODE_7nm		0x00BC

/* Tx VMODE offsets */
#define DP_VMODE_CTRL1				0x00C8
#define DP_VMODE_CTRL1_7nm			0x00E8

#define DP_PHY_PLL_POLL_SLEEP_US		500
#define DP_PHY_PLL_POLL_TIMEOUT_US		10000

#define DP_VCO_RATE_8100MHZDIV1000		8100000UL
#define DP_VCO_RATE_9720MHZDIV1000		9720000UL
#define DP_VCO_RATE_10800MHZDIV1000		10800000UL

#define DP_5NM_C_READY		BIT(0)
#define DP_5NM_FREQ_DONE	BIT(0)
#define DP_5NM_PLL_LOCKED	BIT(1)
#define DP_5NM_PHY_READY	BIT(1)
#define DP_5NM_TSYNC_DONE	BIT(0)

static int dp_vco_pll_init_db_5nm(struct dp_pll_db *pdb,
		unsigned long rate)
{
	struct dp_pll *pll = pdb->pll;
	u32 spare_value = 0;

	spare_value = dp_pll_read(dp_phy, DP_PHY_SPARE0);
	pdb->lane_cnt = spare_value & 0x0F;
	pdb->orientation = (spare_value & 0xF0) >> 4;

	DP_DEBUG("spare_value=0x%x, ln_cnt=0x%x, orientation=0x%x\n",
			spare_value, pdb->lane_cnt, pdb->orientation);

	pdb->div_frac_start1_mode0 = 0x00;
	pdb->integloop_gain0_mode0 = 0x3f;
	pdb->integloop_gain1_mode0 = 0x00;

	switch (rate) {
	case DP_VCO_HSCLK_RATE_1620MHZDIV1000:
		DP_DEBUG("VCO rate: %ld\n", DP_VCO_RATE_9720MHZDIV1000);
		pdb->hsclk_sel = 0x05;
		pdb->dec_start_mode0 = 0x69;
		pdb->div_frac_start2_mode0 = 0x80;
		pdb->div_frac_start3_mode0 = 0x07;
		pdb->lock_cmp1_mode0 = 0x6f;
		pdb->lock_cmp2_mode0 = 0x08;
		pdb->phy_vco_div = 0x1;
		pdb->lock_cmp_en = 0x04;
		pdb->ssc_step_size1_mode0 = 0x45;
		pdb->ssc_step_size2_mode0 = 0x06;
		break;
	case DP_VCO_HSCLK_RATE_2700MHZDIV1000:
		DP_DEBUG("VCO rate: %ld\n", DP_VCO_RATE_10800MHZDIV1000);
		pdb->hsclk_sel = 0x03;
		pdb->dec_start_mode0 = 0x69;
		pdb->div_frac_start2_mode0 = 0x80;
		pdb->div_frac_start3_mode0 = 0x07;
		pdb->lock_cmp1_mode0 = 0x0f;
		pdb->lock_cmp2_mode0 = 0x0e;
		pdb->phy_vco_div = 0x1;
		pdb->lock_cmp_en = 0x08;
		pdb->ssc_step_size1_mode0 = 0x45;
		pdb->ssc_step_size2_mode0 = 0x06;
		break;
	case DP_VCO_HSCLK_RATE_5400MHZDIV1000:
		DP_DEBUG("VCO rate: %ld\n", DP_VCO_RATE_10800MHZDIV1000);
		pdb->hsclk_sel = 0x01;
		pdb->dec_start_mode0 = 0x8c;
		pdb->div_frac_start2_mode0 = 0x00;
		pdb->div_frac_start3_mode0 = 0x0a;
		pdb->lock_cmp1_mode0 = 0x1f;
		pdb->lock_cmp2_mode0 = 0x1c;
		pdb->phy_vco_div = 0x2;
		pdb->lock_cmp_en = 0x08;
		pdb->ssc_step_size1_mode0 = 0x5c;
		pdb->ssc_step_size2_mode0 = 0x08;
		break;
	case DP_VCO_HSCLK_RATE_8100MHZDIV1000:
		DP_DEBUG("VCO rate: %ld\n", DP_VCO_RATE_8100MHZDIV1000);
		pdb->hsclk_sel = 0x00;
		pdb->dec_start_mode0 = 0x69;
		pdb->div_frac_start2_mode0 = 0x80;
		pdb->div_frac_start3_mode0 = 0x07;
		pdb->lock_cmp1_mode0 = 0x2f;
		pdb->lock_cmp2_mode0 = 0x2a;
		pdb->phy_vco_div = 0x0;
		pdb->lock_cmp_en = 0x08;
		pdb->ssc_step_size1_mode0 = 0x45;
		pdb->ssc_step_size2_mode0 = 0x06;
		break;
	default:
		DP_ERR("unsupported rate %ld\n", rate);
		return -EINVAL;
	}
	return 0;
}

static void dp_pll_config_tx_7nm(struct dp_pll *pll)
{
	/* TX-0 register configuration */
	dp_pll_write(dp_phy, DP_PHY_TX0_TX1_LANE_CTL, 0x05);
	dp_pll_write(dp_ln_tx0, DP_VMODE_CTRL1_7nm, 0x40);
	dp_pll_write(dp_ln_tx0, TXn_PRE_STALL_LDO_BOOST_EN, 0x30);
	dp_pll_write(dp_ln_tx0, TXn_INTERFACE_SELECT, 0x3b);
	dp_pll_write(dp_ln_tx0, TXn_CLKBUF_ENABLE, 0x0f);
	dp_pll_write(dp_ln_tx0, TXn_RESET_TSYNC_EN, 0x03);
	dp_pll_write(dp_ln_tx0, DP_TRAN_DRVR_EMP_EN_7nm, 0xf);
	dp_pll_write(dp_ln_tx0, TXn_PARRATE_REC_DETECT_IDLE_EN, 0x00);
	dp_pll_write(dp_ln_tx0, DP_TX_INTERFACE_MODE_7nm, 0x00);
	dp_pll_write(dp_ln_tx0, TXn_RES_CODE_LANE_OFFSET_TX, 0x11);
	dp_pll_write(dp_ln_tx0, TXn_RES_CODE_LANE_OFFSET_RX, 0x11);
	dp_pll_write(dp_ln_tx0, TXn_TX_BAND, 0x04);

	/* Make sure the PLL register writes are done */
	wmb();

	/* TX-1 register configuration */
	dp_pll_write(dp_phy, DP_PHY_TX2_TX3_LANE_CTL, 0x05);
	dp_pll_write(dp_ln_tx1, DP_VMODE_CTRL1_7nm, 0x40);
	dp_pll_write(dp_ln_tx1, TXn_PRE_STALL_LDO_BOOST_EN, 0x30);
	dp_pll_write(dp_ln_tx1, TXn_INTERFACE_SELECT, 0x3b);
	dp_pll_write(dp_ln_tx1, TXn_CLKBUF_ENABLE, 0x0f);
	dp_pll_write(dp_ln_tx1, TXn_RESET_TSYNC_EN, 0x03);
	dp_pll_write(dp_ln_tx1, DP_TRAN_DRVR_EMP_EN_7nm, 0xf);
	dp_pll_write(dp_ln_tx1, TXn_PARRATE_REC_DETECT_IDLE_EN, 0x00);
	dp_pll_write(dp_ln_tx1, DP_TX_INTERFACE_MODE_7nm, 0x00);
	dp_pll_write(dp_ln_tx1, TXn_RES_CODE_LANE_OFFSET_TX, 0x11);
	dp_pll_write(dp_ln_tx1, TXn_RES_CODE_LANE_OFFSET_RX, 0x11);
	dp_pll_write(dp_ln_tx1, TXn_TX_BAND, 0x04);

	/* Make sure the PHY register writes are done */
	wmb();
}

static void dp_pll_config_tx_5nm(struct dp_pll *pll)
{
	/* TX-0 register configuration */
	dp_pll_write(dp_phy, DP_PHY_TX0_TX1_LANE_CTL, 0x05);
	dp_pll_write(dp_ln_tx0, DP_VMODE_CTRL1, 0x40);
	dp_pll_write(dp_ln_tx0, TXn_PRE_STALL_LDO_BOOST_EN, 0x30);
	dp_pll_write(dp_ln_tx0, TXn_INTERFACE_SELECT, 0x3b);
	dp_pll_write(dp_ln_tx0, TXn_CLKBUF_ENABLE, 0x0f);
	dp_pll_write(dp_ln_tx0, TXn_RESET_TSYNC_EN, 0x03);
	dp_pll_write(dp_ln_tx0, DP_TRAN_DRVR_EMP_EN, 0xf);
	dp_pll_write(dp_ln_tx0, TXn_PARRATE_REC_DETECT_IDLE_EN, 0x00);
	dp_pll_write(dp_ln_tx0, DP_TX_INTERFACE_MODE, 0x00);
	dp_pll_write(dp_ln_tx0, TXn_RES_CODE_LANE_OFFSET_TX, 0x11);
	dp_pll_write(dp_ln_tx0, TXn_RES_CODE_LANE_OFFSET_RX, 0x11);
	dp_pll_write(dp_ln_tx0, TXn_TX_BAND, 0x04);
	/* Make sure the PLL register writes are done */
	wmb();

	/* TX-1 register configuration */
	dp_pll_write(dp_phy, DP_PHY_TX2_TX3_LANE_CTL, 0x05);
	dp_pll_write(dp_ln_tx1, DP_VMODE_CTRL1, 0x40);
	dp_pll_write(dp_ln_tx1, TXn_PRE_STALL_LDO_BOOST_EN, 0x30);
	dp_pll_write(dp_ln_tx1, TXn_INTERFACE_SELECT, 0x3b);
	dp_pll_write(dp_ln_tx1, TXn_CLKBUF_ENABLE, 0x0f);
	dp_pll_write(dp_ln_tx1, TXn_RESET_TSYNC_EN, 0x03);
	dp_pll_write(dp_ln_tx1, DP_TRAN_DRVR_EMP_EN, 0xf);
	dp_pll_write(dp_ln_tx1, TXn_PARRATE_REC_DETECT_IDLE_EN, 0x00);
	dp_pll_write(dp_ln_tx1, DP_TX_INTERFACE_MODE, 0x00);
	dp_pll_write(dp_ln_tx1, TXn_RES_CODE_LANE_OFFSET_TX, 0x11);
	dp_pll_write(dp_ln_tx1, TXn_RES_CODE_LANE_OFFSET_RX, 0x11);
	dp_pll_write(dp_ln_tx1, TXn_TX_BAND, 0x04);

	/* Make sure the PHY register writes are done */
	wmb();
}

static int dp_config_vco_rate_5nm(struct dp_pll_vco_clk *vco,
		unsigned long rate)
{
	int res = 0;
	struct dp_pll *pll = vco->priv;
	struct dp_pll_db *pdb = (struct dp_pll_db *)pll->priv;

	res = dp_vco_pll_init_db_5nm(pdb, rate);
	if (res) {
		DP_ERR("VCO Init DB failed\n");
		return res;
	}

	dp_pll_write(dp_phy, DP_PHY_CFG_1, 0x0F);

	if (pdb->lane_cnt != 4) {
		if (pdb->orientation == ORIENTATION_CC2)
			dp_pll_write(dp_phy, DP_PHY_PD_CTL, 0x6d);
		else
			dp_pll_write(dp_phy, DP_PHY_PD_CTL, 0x75);
	} else {
		dp_pll_write(dp_phy, DP_PHY_PD_CTL, 0x7d);
	}

	/* Make sure the PHY register writes are done */
	wmb();

	dp_pll_write(dp_pll, QSERDES_COM_SVS_MODE_CLK_SEL, 0x05);
	dp_pll_write(dp_pll, QSERDES_COM_SYSCLK_EN_SEL, 0x3b);
	dp_pll_write(dp_pll, QSERDES_COM_SYS_CLK_CTRL, 0x02);
	dp_pll_write(dp_pll, QSERDES_COM_CLK_ENABLE1, 0x0c);
	dp_pll_write(dp_pll, QSERDES_COM_SYSCLK_BUF_ENABLE, 0x06);
	dp_pll_write(dp_pll, QSERDES_COM_CLK_SEL, 0x30);
	/* Make sure the PHY register writes are done */
	wmb();

	/* PLL Optimization */
	dp_pll_write(dp_pll, QSERDES_COM_PLL_IVCO, 0x0f);
	dp_pll_write(dp_pll, QSERDES_COM_PLL_CCTRL_MODE0, 0x36);
	dp_pll_write(dp_pll, QSERDES_COM_PLL_RCTRL_MODE0, 0x16);
	dp_pll_write(dp_pll, QSERDES_COM_CP_CTRL_MODE0, 0x06);
	/* Make sure the PLL register writes are done */
	wmb();

	/* link rate dependent params */
	dp_pll_write(dp_pll, QSERDES_COM_HSCLK_SEL, pdb->hsclk_sel);
	dp_pll_write(dp_pll, QSERDES_COM_DEC_START_MODE0, pdb->dec_start_mode0);
	dp_pll_write(dp_pll,
		QSERDES_COM_DIV_FRAC_START1_MODE0, pdb->div_frac_start1_mode0);
	dp_pll_write(dp_pll,
		QSERDES_COM_DIV_FRAC_START2_MODE0, pdb->div_frac_start2_mode0);
	dp_pll_write(dp_pll,
		QSERDES_COM_DIV_FRAC_START3_MODE0, pdb->div_frac_start3_mode0);
	dp_pll_write(dp_pll, QSERDES_COM_LOCK_CMP1_MODE0, pdb->lock_cmp1_mode0);
	dp_pll_write(dp_pll, QSERDES_COM_LOCK_CMP2_MODE0, pdb->lock_cmp2_mode0);
	dp_pll_write(dp_pll, QSERDES_COM_LOCK_CMP_EN, pdb->lock_cmp_en);
	dp_pll_write(dp_phy, DP_PHY_VCO_DIV, pdb->phy_vco_div);
	/* Make sure the PLL register writes are done */
	wmb();

	dp_pll_write(dp_pll, QSERDES_COM_CMN_CONFIG, 0x02);
	dp_pll_write(dp_pll, QSERDES_COM_INTEGLOOP_GAIN0_MODE0, 0x3f);
	dp_pll_write(dp_pll, QSERDES_COM_INTEGLOOP_GAIN1_MODE0, 0x00);
	dp_pll_write(dp_pll, QSERDES_COM_VCO_TUNE_MAP, 0x00);
	/* Make sure the PHY register writes are done */
	wmb();

	dp_pll_write(dp_pll, QSERDES_COM_BG_TIMER, 0x0a);
	dp_pll_write(dp_pll, QSERDES_COM_CORECLK_DIV_MODE0, 0x0a);
	dp_pll_write(dp_pll, QSERDES_COM_VCO_TUNE_CTRL, 0x00);
	if (pll->bonding_en)
		dp_pll_write(dp_pll, QSERDES_COM_BIAS_EN_CLKBUFLR_EN, 0x1f);
	else
		dp_pll_write(dp_pll, QSERDES_COM_BIAS_EN_CLKBUFLR_EN, 0x17);
	dp_pll_write(dp_pll, QSERDES_COM_CORE_CLK_EN, 0x1f);
	/* Make sure the PHY register writes are done */
	wmb();

	if (pll->ssc_en) {
		dp_pll_write(dp_pll, QSERDES_COM_SSC_EN_CENTER, 0x01);
		dp_pll_write(dp_pll, QSERDES_COM_SSC_ADJ_PER1, 0x00);
		dp_pll_write(dp_pll, QSERDES_COM_SSC_PER1, 0x36);
		dp_pll_write(dp_pll, QSERDES_COM_SSC_PER2, 0x01);
		dp_pll_write(dp_pll, QSERDES_COM_SSC_STEP_SIZE1_MODE0,
				pdb->ssc_step_size1_mode0);
		dp_pll_write(dp_pll, QSERDES_COM_SSC_STEP_SIZE2_MODE0,
				pdb->ssc_step_size2_mode0);
	}

	if (pdb->orientation == ORIENTATION_CC2)
		dp_pll_write(dp_phy, DP_PHY_MODE, 0x4c);
	else
		dp_pll_write(dp_phy, DP_PHY_MODE, 0x5c);

	dp_pll_write(dp_phy, DP_PHY_AUX_CFG1, 0x13);
	dp_pll_write(dp_phy, DP_PHY_AUX_CFG2, 0xA4);
	/* Make sure the PLL register writes are done */
	wmb();

	if (pll->revision == DP_PLL_7NM)
		dp_pll_config_tx_7nm(pll);
	else
		dp_pll_config_tx_5nm(pll);

	return res;
}

enum dp_5nm_pll_status {
	C_READY,
	FREQ_DONE,
	PLL_LOCKED,
	PHY_READY,
	TSYNC_DONE,
};

char *dp_5nm_pll_get_status_name(enum dp_5nm_pll_status status)
{
	switch (status) {
	case C_READY:
		return "C_READY";
	case FREQ_DONE:
		return "FREQ_DONE";
	case PLL_LOCKED:
		return "PLL_LOCKED";
	case PHY_READY:
		return "PHY_READY";
	case TSYNC_DONE:
		return "TSYNC_DONE";
	default:
		return "unknown";
	}
}

static bool dp_5nm_pll_get_status(struct dp_pll *pll,
		enum dp_5nm_pll_status status)
{
	u32 reg, state, bit;
	void __iomem *base;
	bool success = true;

	switch (status) {
	case C_READY:
		base = dp_pll_get_base(dp_pll);
		reg = QSERDES_COM_C_READY_STATUS;
		bit = DP_5NM_C_READY;
		break;
	case FREQ_DONE:
		base = dp_pll_get_base(dp_pll);
		reg = QSERDES_COM_CMN_STATUS;
		bit = DP_5NM_FREQ_DONE;
		break;
	case PLL_LOCKED:
		base = dp_pll_get_base(dp_pll);
		reg = QSERDES_COM_CMN_STATUS;
		bit = DP_5NM_PLL_LOCKED;
		break;
	case PHY_READY:
		base = dp_pll_get_base(dp_phy);
		reg = DP_PHY_STATUS;
		bit = DP_5NM_PHY_READY;
		break;
	case TSYNC_DONE:
		base = dp_pll_get_base(dp_phy);
		reg = DP_PHY_STATUS;
		bit = DP_5NM_TSYNC_DONE;
		break;
	default:
		return false;
	}

	if (readl_poll_timeout_atomic((base + reg), state,
			((state & bit) > 0),
			DP_PHY_PLL_POLL_SLEEP_US,
			DP_PHY_PLL_POLL_TIMEOUT_US)) {
		DP_ERR("%s failed, status=%x\n",
			dp_5nm_pll_get_status_name(status), state);

		success = false;
	}

	return success;
}

static int dp_pll_enable_5nm(struct clk_hw *hw)
{
	int rc = 0;
	struct dp_pll_vco_clk *vco = to_dp_vco_hw(hw);
	struct dp_pll *pll = vco->priv;

	pll->aux->state &= ~DP_STATE_PLL_LOCKED;

	dp_pll_write(dp_phy, DP_PHY_CFG, 0x01);
	dp_pll_write(dp_phy, DP_PHY_CFG, 0x05);
	dp_pll_write(dp_phy, DP_PHY_CFG, 0x01);
	dp_pll_write(dp_phy, DP_PHY_CFG, 0x09);
	dp_pll_write(dp_pll, QSERDES_COM_RESETSM_CNTRL, 0x20);
	wmb();	/* Make sure the PLL register writes are done */

	if (!dp_5nm_pll_get_status(pll, C_READY)) {
		rc = -EINVAL;
		goto lock_err;
	}

	if (!dp_5nm_pll_get_status(pll, FREQ_DONE)) {
		rc = -EINVAL;
		goto lock_err;
	}

	if (!dp_5nm_pll_get_status(pll, PLL_LOCKED)) {
		rc = -EINVAL;
		goto lock_err;
	}

	dp_pll_write(dp_phy, DP_PHY_CFG, 0x19);
	/* Make sure the PHY register writes are done */
	wmb();

	if (!dp_5nm_pll_get_status(pll, TSYNC_DONE)) {
		rc = -EINVAL;
		goto lock_err;
	}

	if (!dp_5nm_pll_get_status(pll, PHY_READY)) {
		rc = -EINVAL;
		goto lock_err;
	}

	pll->aux->state |= DP_STATE_PLL_LOCKED;
	DP_DEBUG("PLL is locked\n");
lock_err:
	return rc;
}

static int dp_pll_disable_5nm(struct clk_hw *hw)
{
	struct dp_pll_vco_clk *vco = to_dp_vco_hw(hw);
	struct dp_pll *pll = vco->priv;

	/* Assert DP PHY power down */
	dp_pll_write(dp_phy, DP_PHY_PD_CTL, 0x2);
	/*
	 * Make sure all the register writes to disable PLL are
	 * completed before doing any other operation
	 */
	wmb();

	return 0;
}

static struct clk_ops mux_clk_ops;
static struct regmap_config dp_pll_5nm_cfg = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register = 0x910,
};

int dp_mux_set_parent_5nm(void *context, unsigned int reg, unsigned int val)
{
	struct dp_pll *pll = context;
	u32 auxclk_div;

	if (!context) {
		DP_ERR("invalid input parameters\n");
		return -EINVAL;
	}

	auxclk_div = dp_pll_read(dp_phy, DP_PHY_VCO_DIV);
	auxclk_div &= ~0x03;

	if (val == 0)
		auxclk_div |= 1;
	else if (val == 1)
		auxclk_div |= 2;
	else if (val == 2)
		auxclk_div |= 0;

	dp_pll_write(dp_phy, DP_PHY_VCO_DIV, auxclk_div);
	/* Make sure the PHY registers writes are done */
	wmb();
	DP_DEBUG("mux=%d auxclk_div=%x\n", val, auxclk_div);

	return 0;
}

int dp_mux_get_parent_5nm(void *context, unsigned int reg, unsigned int *val)
{
	u32 auxclk_div = 0;
	struct dp_pll *pll = context;

	if (!context || !val) {
		DP_ERR("invalid input parameters\n");
		return -EINVAL;
	}

	if (is_gdsc_disabled(pll))
		return 0;

	auxclk_div = dp_pll_read(dp_phy, DP_PHY_VCO_DIV);
	auxclk_div &= 0x03;

	if (auxclk_div == 1) /* Default divider */
		*val = 0;
	else if (auxclk_div == 2)
		*val = 1;
	else if (auxclk_div == 0)
		*val = 2;

	DP_DEBUG("auxclk_div=%d, val=%d\n", auxclk_div, *val);

	return 0;
}

static struct regmap_bus dp_pixel_mux_regmap_ops = {
	.reg_write = dp_mux_set_parent_5nm,
	.reg_read = dp_mux_get_parent_5nm,
};

static int dp_vco_set_rate_5nm(struct clk_hw *hw, unsigned long rate,
					unsigned long parent_rate)
{
	struct dp_pll_vco_clk *vco;
	int rc;
	struct dp_pll *pll;

	if (!hw) {
		DP_ERR("invalid input parameters\n");
		return -EINVAL;
	}

	vco = to_dp_vco_hw(hw);
	pll = vco->priv;

	DP_DEBUG("DP lane CLK rate=%ld\n", rate);

	rc = dp_config_vco_rate_5nm(vco, rate);
	if (rc)
		DP_ERR("Failed to set clk rate\n");

	vco->rate = rate;

	return 0;
}

static int dp_regulator_enable_5nm(struct dp_parser *parser,
		enum dp_pm_type pm_type, bool enable)
{
	int rc = 0;
	struct dss_module_power mp;

	if (pm_type < DP_CORE_PM || pm_type >= DP_MAX_PM) {
		DP_ERR("invalid resource: %d %s\n", pm_type,
				dp_parser_pm_name(pm_type));
		return -EINVAL;
	}

	mp = parser->mp[pm_type];
	rc = msm_dss_enable_vreg(mp.vreg_config, mp.num_vreg, enable);
	if (rc) {
		DP_ERR("failed to '%s' vregs for %s\n",
				enable ? "enable" : "disable",
				dp_parser_pm_name(pm_type));
		return rc;
	}

	DP_DEBUG("success: '%s' vregs for %s\n", enable ? "enable" : "disable",
			dp_parser_pm_name(pm_type));
	return rc;
}

static int dp_vco_prepare_5nm(struct clk_hw *hw)
{
	int rc = 0;
	struct dp_pll_vco_clk *vco;
	struct dp_pll *pll;

	if (!hw) {
		DP_ERR("invalid input parameters\n");
		return -EINVAL;
	}

	vco = to_dp_vco_hw(hw);
	pll = vco->priv;

	DP_DEBUG("rate=%ld\n", vco->rate);

	/*
	 * Enable DP_PM_PLL regulator if the PLL revision is 5nm-V1 and the
	 * link rate is 8.1Gbps. This will result in voting to place Mx rail in
	 * turbo as required for V1 hardware PLL functionality.
	 */
	if (pll->revision == DP_PLL_5NM_V1 &&
			vco->rate == DP_VCO_HSCLK_RATE_8100MHZDIV1000)
		dp_regulator_enable_5nm(pll->parser, DP_PLL_PM, true);

	if ((pll->vco_cached_rate != 0)
		&& (pll->vco_cached_rate == vco->rate)) {
		rc = dp_vco_set_rate_5nm(hw, pll->vco_cached_rate,
				pll->vco_cached_rate);
		if (rc) {
			DP_ERR("index=%d vco_set_rate failed. rc=%d\n",
				rc, pll->index);
			goto error;
		}
	}

	rc = dp_pll_enable_5nm(hw);
	if (rc) {
		DP_ERR("ndx=%d failed to enable dp pll\n", pll->index);
		goto error;
	}

error:
	return rc;
}

static void dp_vco_unprepare_5nm(struct clk_hw *hw)
{
	struct dp_pll_vco_clk *vco;
	struct dp_pll *pll;

	if (!hw) {
		DP_ERR("invalid input parameters\n");
		return;
	}

	vco = to_dp_vco_hw(hw);
	pll = vco->priv;

	if (!pll) {
		DP_ERR("invalid input parameter\n");
		return;
	}

	if (pll->revision == DP_PLL_5NM_V1 &&
			vco->rate == DP_VCO_HSCLK_RATE_8100MHZDIV1000)
		dp_regulator_enable_5nm(pll->parser, DP_PLL_PM, false);

	pll->vco_cached_rate = vco->rate;
	dp_pll_disable_5nm(hw);
}

static unsigned long dp_vco_recalc_rate_5nm(struct clk_hw *hw,
					unsigned long parent_rate)
{
	struct dp_pll_vco_clk *vco;
	u32 hsclk_sel, link_clk_divsel, hsclk_div, link_clk_div = 0;
	unsigned long vco_rate;
	struct dp_pll *pll;

	if (!hw) {
		DP_ERR("invalid input parameters\n");
		return 0;
	}

	vco = to_dp_vco_hw(hw);
	pll = vco->priv;

	if (is_gdsc_disabled(pll))
		return 0;

	DP_DEBUG("input rates: parent=%lu, vco=%lu\n", parent_rate, vco->rate);

	hsclk_sel = dp_pll_read(dp_pll, QSERDES_COM_HSCLK_SEL);
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
		DP_DEBUG("unknown divider. forcing to default\n");
		hsclk_div = 5;
	}

	link_clk_divsel = dp_pll_read(dp_phy, DP_PHY_AUX_CFG2);
	link_clk_divsel >>= 2;
	link_clk_divsel &= 0x3;

	if (link_clk_divsel == 0)
		link_clk_div = 5;
	else if (link_clk_divsel == 1)
		link_clk_div = 10;
	else if (link_clk_divsel == 2)
		link_clk_div = 20;
	else
		DP_ERR("unsupported div. Phy_mode: %d\n", link_clk_divsel);

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

	DP_DEBUG("hsclk: sel=0x%x, div=0x%x; lclk: sel=%u, div=%u, rate=%lu\n",
		hsclk_sel, hsclk_div, link_clk_divsel, link_clk_div, vco_rate);

	pll->vco_cached_rate = vco->rate = vco_rate;

	return vco_rate;
}

static long dp_vco_round_rate_5nm(struct clk_hw *hw, unsigned long rate,
			unsigned long *parent_rate)
{
	unsigned long rrate = rate;
	struct dp_pll_vco_clk *vco;

	if (!hw) {
		DP_ERR("invalid input parameters\n");
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

	DP_DEBUG("rrate=%ld\n", rrate);

	if (parent_rate)
		*parent_rate = rrate;
	return rrate;
}

/* Op structures */
static const struct clk_ops dp_5nm_vco_clk_ops = {
	.recalc_rate = dp_vco_recalc_rate_5nm,
	.set_rate = dp_vco_set_rate_5nm,
	.round_rate = dp_vco_round_rate_5nm,
	.prepare = dp_vco_prepare_5nm,
	.unprepare = dp_vco_unprepare_5nm,
};

static struct dp_pll_vco_clk dp_vco_clk = {
	.min_rate = DP_VCO_HSCLK_RATE_1620MHZDIV1000,
	.max_rate = DP_VCO_HSCLK_RATE_8100MHZDIV1000,
	.hw.init = &(struct clk_init_data){
		.name = "dp_vco_clk",
		.parent_names = (const char *[]){ "bi_tcxo" },
		.num_parents = 1,
		.ops = &dp_5nm_vco_clk_ops,
	},
};

static struct clk_fixed_factor dp_phy_pll_link_clk = {
	.div = 10,
	.mult = 1,

	.hw.init = &(struct clk_init_data){
		.name = "dp_phy_pll_link_clk",
		.parent_names =
			(const char *[]){ "dp_vco_clk" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dp_vco_divsel_two_clk_src = {
	.div = 2,
	.mult = 1,

	.hw.init = &(struct clk_init_data){
		.name = "dp_vco_divsel_two_clk_src",
		.parent_names =
			(const char *[]){ "dp_vco_clk" },
		.num_parents = 1,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dp_vco_divsel_four_clk_src = {
	.div = 4,
	.mult = 1,

	.hw.init = &(struct clk_init_data){
		.name = "dp_vco_divsel_four_clk_src",
		.parent_names =
			(const char *[]){ "dp_vco_clk" },
		.num_parents = 1,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dp_vco_divsel_six_clk_src = {
	.div = 6,
	.mult = 1,

	.hw.init = &(struct clk_init_data){
		.name = "dp_vco_divsel_six_clk_src",
		.parent_names =
			(const char *[]){ "dp_vco_clk" },
		.num_parents = 1,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_regmap_mux dp_phy_pll_vco_div_clk = {
	.reg = 0x64,
	.shift = 0,
	.width = 2,

	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dp_phy_pll_vco_div_clk",
			.parent_names =
				(const char *[]){"dp_vco_divsel_two_clk_src",
					"dp_vco_divsel_four_clk_src",
					"dp_vco_divsel_six_clk_src"},
			.num_parents = 3,
			.ops = &mux_clk_ops,
			.flags = CLK_SET_RATE_PARENT,
		},
	},
};

static int clk_mux_determine_rate(struct clk_hw *hw,
				  struct clk_rate_request *req)
{
	int ret = 0;

	if (!hw || !req) {
		DP_ERR("Invalid input parameters\n");
		return -EINVAL;
	}

	ret = __clk_mux_determine_rate_closest(hw, req);
	if (ret)
		return ret;

	/* Set the new parent of mux if there is a new valid parent */
	if (hw->clk && req->best_parent_hw->clk)
		clk_set_parent(hw->clk, req->best_parent_hw->clk);

	return 0;
}

static unsigned long mux_recalc_rate(struct clk_hw *hw,
					unsigned long parent_rate)
{
	struct clk *div_clk = NULL, *vco_clk = NULL;
	struct dp_pll_vco_clk *vco = NULL;

	if (!hw) {
		DP_ERR("Invalid input parameter\n");
		return 0;
	}

	div_clk = clk_get_parent(hw->clk);
	if (!div_clk)
		return 0;

	vco_clk = clk_get_parent(div_clk);
	if (!vco_clk)
		return 0;

	vco = to_dp_vco_hw(__clk_get_hw(vco_clk));
	if (!vco)
		return 0;

	if (vco->rate == DP_VCO_HSCLK_RATE_8100MHZDIV1000)
		return (vco->rate / 6);
	else if (vco->rate == DP_VCO_HSCLK_RATE_5400MHZDIV1000)
		return (vco->rate / 4);
	else
		return (vco->rate / 2);
}

static struct clk_hw *mdss_dp_pllcc_5nm[] = {
	[DP_VCO_CLK] = &dp_vco_clk.hw,
	[DP_LINK_CLK_DIVSEL_TEN] = &dp_phy_pll_link_clk.hw,
	[DP_VCO_DIVIDED_TWO_CLK_SRC] = &dp_vco_divsel_two_clk_src.hw,
	[DP_VCO_DIVIDED_FOUR_CLK_SRC] = &dp_vco_divsel_four_clk_src.hw,
	[DP_VCO_DIVIDED_SIX_CLK_SRC] = &dp_vco_divsel_six_clk_src.hw,
	[DP_PHY_PLL_VCO_DIV_CLK] = &dp_phy_pll_vco_div_clk.clkr.hw,
};

static struct dp_pll_db dp_pdb;

int dp_pll_clock_register_5nm(struct dp_pll *pll)
{
	int rc = -ENOTSUPP, i = 0;
	struct platform_device *pdev;
	struct clk *clk;
	struct regmap *regmap;
	int num_clks = ARRAY_SIZE(mdss_dp_pllcc_5nm);

	if (!pll) {
		DP_ERR("pll data not initialized\n");
		return -EINVAL;
	}
	pdev = pll->pdev;

	pll->clk_data = kzalloc(sizeof(*pll->clk_data), GFP_KERNEL);
	if (!pll->clk_data)
		return -ENOMEM;

	pll->clk_data->clks = kcalloc(num_clks, sizeof(struct clk *),
			GFP_KERNEL);
	if (!pll->clk_data->clks) {
		kfree(pll->clk_data);
		return -ENOMEM;
	}

	pll->clk_data->clk_num = num_clks;

	pll->priv = &dp_pdb;
	dp_pdb.pll = pll;

	/* Set client data for vco, mux and div clocks */
	regmap = regmap_init(&pdev->dev, &dp_pixel_mux_regmap_ops,
			pll, &dp_pll_5nm_cfg);
	mux_clk_ops = clk_regmap_mux_closest_ops;
	mux_clk_ops.determine_rate = clk_mux_determine_rate;
	mux_clk_ops.recalc_rate = mux_recalc_rate;

	dp_vco_clk.priv = pll;
	dp_phy_pll_vco_div_clk.clkr.regmap = regmap;

	for (i = DP_VCO_CLK; i <= DP_PHY_PLL_VCO_DIV_CLK; i++) {
		DP_DEBUG("reg clk: %d index: %d\n", i, pll->index);
		clk = clk_register(&pdev->dev, mdss_dp_pllcc_5nm[i]);
		if (IS_ERR(clk)) {
			DP_ERR("clk registration failed for DP: %d\n",
					pll->index);
			rc = -EINVAL;
			goto clk_reg_fail;
		}
		pll->clk_data->clks[i] = clk;
	}

	rc = of_clk_add_provider(pdev->dev.of_node,
			of_clk_src_onecell_get, pll->clk_data);
	if (rc) {
		DP_ERR("Clock register failed rc=%d\n", rc);
		rc = -EPROBE_DEFER;
		goto clk_reg_fail;
	} else {
		DP_DEBUG("success\n");
	}
	return rc;
clk_reg_fail:
	dp_pll_clock_unregister_5nm(pll);
	return rc;
}

void dp_pll_clock_unregister_5nm(struct dp_pll *pll)
{
	kfree(pll->clk_data->clks);
	kfree(pll->clk_data);
}
