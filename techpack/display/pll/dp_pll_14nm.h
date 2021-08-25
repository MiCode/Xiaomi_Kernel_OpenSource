/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 */

#ifndef __MDSS_DP_PLL_14NM_H
#define __MDSS_DP_PLL_14NM_H

#define DP_PHY_REVISION_ID0			0x0000
#define DP_PHY_REVISION_ID1			0x0004
#define DP_PHY_REVISION_ID2			0x0008
#define DP_PHY_REVISION_ID3			0x000C

#define DP_PHY_CFG				0x0010
#define DP_PHY_CFG_1				0x0014
#define DP_PHY_PD_CTL				0x0018
#define DP_PHY_MODE				0x001C

#define DP_PHY_AUX_CFG0				0x0020
#define DP_PHY_AUX_CFG1				0x0024
#define DP_PHY_AUX_CFG2				0x0028
#define DP_PHY_AUX_CFG3				0x002C
#define DP_PHY_AUX_CFG4				0x0030
#define DP_PHY_AUX_CFG5				0x0034
#define DP_PHY_AUX_CFG6				0x0038
#define DP_PHY_AUX_CFG7				0x003C
#define DP_PHY_AUX_CFG8				0x0040
#define DP_PHY_AUX_CFG9				0x0044
#define DP_PHY_AUX_INTERRUPT_MASK		0x0048
#define DP_PHY_AUX_INTERRUPT_CLEAR		0x004C
#define DP_PHY_AUX_BIST_CFG			0x0050

#define DP_PHY_VCO_DIV				0x0068
#define DP_PHY_TX0_TX1_LANE_CTL			0x006C

#define DP_PHY_TX2_TX3_LANE_CTL			0x0088
#define DP_PHY_SPARE0				0x00AC
#define DP_PHY_STATUS				0x00C0

/* Tx registers */
#define QSERDES_TX0_OFFSET			0x0400
#define QSERDES_TX1_OFFSET			0x0800

#define TXn_BIST_MODE_LANENO			0x0000
#define TXn_CLKBUF_ENABLE			0x0008
#define TXn_TX_EMP_POST1_LVL			0x000C

#define TXn_TX_DRV_LVL				0x001C

#define TXn_RESET_TSYNC_EN			0x0024
#define TXn_PRE_STALL_LDO_BOOST_EN		0x0028
#define TXn_TX_BAND				0x002C
#define TXn_SLEW_CNTL				0x0030
#define TXn_INTERFACE_SELECT			0x0034

#define TXn_RES_CODE_LANE_TX			0x003C
#define TXn_RES_CODE_LANE_RX			0x0040
#define TXn_RES_CODE_LANE_OFFSET_TX		0x0044
#define TXn_RES_CODE_LANE_OFFSET_RX		0x0048

#define TXn_DEBUG_BUS_SEL			0x0058
#define TXn_TRANSCEIVER_BIAS_EN			0x005C
#define TXn_HIGHZ_DRVR_EN			0x0060
#define TXn_TX_POL_INV				0x0064
#define TXn_PARRATE_REC_DETECT_IDLE_EN		0x0068

#define TXn_LANE_MODE_1				0x008C

#define TXn_TRAN_DRVR_EMP_EN			0x00C0
#define TXn_TX_INTERFACE_MODE			0x00C4

#define TXn_VMODE_CTRL1				0x00F0


/* PLL register offset */
#define QSERDES_COM_ATB_SEL1			0x0000
#define QSERDES_COM_ATB_SEL2			0x0004
#define QSERDES_COM_FREQ_UPDATE			0x0008
#define QSERDES_COM_BG_TIMER			0x000C
#define QSERDES_COM_SSC_EN_CENTER		0x0010
#define QSERDES_COM_SSC_ADJ_PER1		0x0014
#define QSERDES_COM_SSC_ADJ_PER2		0x0018
#define QSERDES_COM_SSC_PER1			0x001C
#define QSERDES_COM_SSC_PER2			0x0020
#define QSERDES_COM_SSC_STEP_SIZE1		0x0024
#define QSERDES_COM_SSC_STEP_SIZE2		0x0028
#define QSERDES_COM_POST_DIV			0x002C
#define QSERDES_COM_POST_DIV_MUX		0x0030
#define QSERDES_COM_BIAS_EN_CLKBUFLR_EN		0x0034
#define QSERDES_COM_CLK_ENABLE1			0x0038
#define QSERDES_COM_SYS_CLK_CTRL		0x003C
#define QSERDES_COM_SYSCLK_BUF_ENABLE		0x0040
#define QSERDES_COM_PLL_EN			0x0044
#define QSERDES_COM_PLL_IVCO			0x0048
#define QSERDES_COM_LOCK_CMP1_MODE0		0x004C
#define QSERDES_COM_LOCK_CMP2_MODE0		0x0050
#define QSERDES_COM_LOCK_CMP3_MODE0		0x0054

#define QSERDES_COM_CP_CTRL_MODE0		0x0078
#define QSERDES_COM_CP_CTRL_MODE1		0x007C
#define QSERDES_COM_PLL_RCTRL_MODE0		0x0084
#define QSERDES_COM_PLL_CCTRL_MODE0		0x0090
#define QSERDES_COM_PLL_CNTRL			0x009C

#define QSERDES_COM_SYSCLK_EN_SEL		0x00AC
#define QSERDES_COM_CML_SYSCLK_SEL		0x00B0
#define QSERDES_COM_RESETSM_CNTRL		0x00B4
#define QSERDES_COM_RESETSM_CNTRL2		0x00B8
#define QSERDES_COM_LOCK_CMP_EN			0x00C8
#define QSERDES_COM_LOCK_CMP_CFG		0x00CC


#define QSERDES_COM_DEC_START_MODE0		0x00D0
#define QSERDES_COM_DEC_START_MODE1		0x00D4
#define QSERDES_COM_DIV_FRAC_START1_MODE0	0x00DC
#define QSERDES_COM_DIV_FRAC_START2_MODE0	0x00E0
#define QSERDES_COM_DIV_FRAC_START3_MODE0	0x00E4

#define QSERDES_COM_INTEGLOOP_GAIN0_MODE0	0x0108
#define QSERDES_COM_INTEGLOOP_GAIN1_MODE0	0x010C
#define QSERDES_COM_VCO_TUNE_CTRL		0x0124
#define QSERDES_COM_VCO_TUNE_MAP		0x0128
#define QSERDES_COM_VCO_TUNE1_MODE0		0x012C
#define QSERDES_COM_VCO_TUNE2_MODE0		0x0130

#define QSERDES_COM_CMN_STATUS			0x015C
#define QSERDES_COM_RESET_SM_STATUS		0x0160

#define QSERDES_COM_BG_CTRL			0x0170
#define QSERDES_COM_CLK_SELECT			0x0174
#define QSERDES_COM_HSCLK_SEL			0x0178
#define QSERDES_COM_CORECLK_DIV			0x0184
#define QSERDES_COM_SW_RESET			0x0188
#define QSERDES_COM_CORE_CLK_EN			0x018C
#define QSERDES_COM_C_READY_STATUS		0x0190
#define QSERDES_COM_CMN_CONFIG			0x0194
#define QSERDES_COM_SVS_MODE_CLK_SEL		0x019C

#define DP_PLL_POLL_SLEEP_US			500
#define DP_PLL_POLL_TIMEOUT_US			10000

#define DP_PHY_POLL_SLEEP_US			500
#define DP_PHY_POLL_TIMEOUT_US			10000

#define DP_VCO_RATE_8100MHZDIV1000		8100000UL
#define DP_VCO_RATE_10800MHZDIV1000		10800000UL

#define DP_VCO_HSCLK_RATE_1620MHZDIV1000	1620000UL
#define DP_VCO_HSCLK_RATE_2700MHZDIV1000	2700000UL
#define DP_VCO_HSCLK_RATE_5400MHZDIV1000	5400000UL

struct dp_pll_db {
	struct mdss_pll_resources *pll;

	/* lane and orientation settings */
	u8 lane_cnt;
	u8 orientation;

	/* COM PHY settings */
	u32 hsclk_sel;
	u32 dec_start_mode0;
	u32 div_frac_start1_mode0;
	u32 div_frac_start2_mode0;
	u32 div_frac_start3_mode0;
	u32 lock_cmp1_mode0;
	u32 lock_cmp2_mode0;
	u32 lock_cmp3_mode0;

	/* PHY vco divider */
	u32 phy_vco_div;

	/* TX settings */
	u32 lane_mode_1;
};

int dp_vco_set_rate_14nm(struct clk_hw *hw, unsigned long rate,
				unsigned long parent_rate);
unsigned long dp_vco_recalc_rate_14nm(struct clk_hw *hw,
				unsigned long parent_rate);
long dp_vco_round_rate_14nm(struct clk_hw *hw, unsigned long rate,
				unsigned long *parent_rate);
int dp_vco_prepare_14nm(struct clk_hw *hw);
void dp_vco_unprepare_14nm(struct clk_hw *hw);
int dp_mux_set_parent_14nm(void *context,
				unsigned int reg, unsigned int val);
int dp_mux_get_parent_14nm(void *context,
				unsigned int reg, unsigned int *val);
#endif /* __MDSS_DP_PLL_14NM_H */
