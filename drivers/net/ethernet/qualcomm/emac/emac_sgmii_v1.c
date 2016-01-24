/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

/* Qualcomm Technologies, Inc. EMAC SGMII Controller driver.
 */

#include "emac_sgmii.h"
#include "emac_hw.h"

#define PCS_MAX_REG_CNT		10
#define PLL_MAX_REG_CNT		18

static const struct emac_reg_write
		physical_coding_sublayer_programming[][PCS_MAX_REG_CNT] = {
	/* EMAC_PHY_MAP_DEFAULT */
	{
		{EMAC_SGMII_PHY_CDR_CTRL0,	SGMII_CDR_MAX_CNT},
		{EMAC_SGMII_PHY_POW_DWN_CTRL0,	PWRDN_B},
		{EMAC_SGMII_PHY_CMN_PWR_CTRL,	BIAS_EN | SYSCLK_EN |
				CLKBUF_L_EN | PLL_TXCLK_EN | PLL_RXCLK_EN},
		{EMAC_SGMII_PHY_TX_PWR_CTRL,	L0_TX_EN | L0_CLKBUF_EN |
				L0_TRAN_BIAS_EN},
		{EMAC_SGMII_PHY_RX_PWR_CTRL,	L0_RX_SIGDET_EN |
				(1 << L0_RX_TERM_MODE_SHFT) | L0_RX_I_EN},
		{EMAC_SGMII_PHY_CMN_PWR_CTRL,	BIAS_EN | PLL_EN | SYSCLK_EN |
				CLKBUF_L_EN | PLL_TXCLK_EN | PLL_RXCLK_EN},
		{EMAC_SGMII_PHY_LANE_CTRL1,	L0_RX_EQ_EN |
				L0_RESET_TSYNC_EN | L0_DRV_LVL_BMSK},
		{END_MARKER,			END_MARKER},
	},
	/* EMAC_PHY_MAP_MDM9607 */
	{
		{EMAC_SGMII_PHY_CDR_CTRL0,	SGMII_CDR_MAX_CNT},
		{EMAC_SGMII_PHY_POW_DWN_CTRL0,	PWRDN_B},
		{EMAC_SGMII_PHY_CMN_PWR_CTRL,	BIAS_EN | SYSCLK_EN |
				CLKBUF_L_EN | PLL_TXCLK_EN | PLL_RXCLK_EN},
		{EMAC_SGMII_PHY_TX_PWR_CTRL,	L0_TX_EN | L0_CLKBUF_EN |
				L0_TRAN_BIAS_EN},
		{EMAC_SGMII_PHY_RX_PWR_CTRL,	L0_RX_SIGDET_EN |
				(1 << L0_RX_TERM_MODE_SHFT) | L0_RX_I_EN},
		{EMAC_SGMII_PHY_CMN_PWR_CTRL,	BIAS_EN | PLL_EN | SYSCLK_EN |
				CLKBUF_L_EN | PLL_TXCLK_EN | PLL_RXCLK_EN},
		{EMAC_QSERDES_COM_PLL_VCOTAIL_EN,	PLL_VCO_TAIL_MUX |
				PLL_VCO_TAIL | PLL_EN_VCOTAIL_EN},
		{EMAC_QSERDES_COM_PLL_CNTRL,		OCP_EN | PLL_DIV_FFEN |
				PLL_DIV_ORD},
		{EMAC_SGMII_PHY_LANE_CTRL1,	L0_RX_EQ_EN |
				L0_RESET_TSYNC_EN | L0_DRV_LVL_BMSK},
		{END_MARKER,			END_MARKER}
	}
};

static const struct emac_reg_write sysclk_refclk_setting[] = {
{EMAC_QSERDES_COM_SYSCLK_EN_SEL,	SYSCLK_SEL_CMOS},
{EMAC_QSERDES_COM_SYS_CLK_CTRL,		SYSCLK_CM | SYSCLK_AC_COUPLE},
{END_MARKER,				END_MARKER},
};

static const struct emac_reg_write pll_setting[][PLL_MAX_REG_CNT] = {
	/* EMAC_PHY_MAP_DEFAULT */
	{
		{EMAC_QSERDES_COM_PLL_IP_SETI,		QSERDES_PLL_IPSETI_DEF},
		{EMAC_QSERDES_COM_PLL_CP_SETI,		QSERDES_PLL_CP_SETI},
		{EMAC_QSERDES_COM_PLL_IP_SETP,		QSERDES_PLL_IP_SETP},
		{EMAC_QSERDES_COM_PLL_CP_SETP,		QSERDES_PLL_CP_SETP},
		{EMAC_QSERDES_COM_PLL_CRCTRL,		QSERDES_PLL_CRCTRL},
		{EMAC_QSERDES_COM_PLL_CNTRL,		OCP_EN | PLL_DIV_FFEN |
				PLL_DIV_ORD},
		{EMAC_QSERDES_COM_DEC_START1,		DEC_START1_MUX |
				QSERDES_PLL_DEC},
		{EMAC_QSERDES_COM_DEC_START2,		DEC_START2_MUX |
				DEC_START2},
		{EMAC_QSERDES_COM_DIV_FRAC_START1,	DIV_FRAC_START1_MUX |
				QSERDES_PLL_DIV_FRAC_START1},
		{EMAC_QSERDES_COM_DIV_FRAC_START2,	DIV_FRAC_START2_MUX |
				QSERDES_PLL_DIV_FRAC_START2},
		{EMAC_QSERDES_COM_DIV_FRAC_START3,	DIV_FRAC_START3_MUX |
				QSERDES_PLL_DIV_FRAC_START3},
		{EMAC_QSERDES_COM_PLLLOCK_CMP1,		QSERDES_PLL_LOCK_CMP1},
		{EMAC_QSERDES_COM_PLLLOCK_CMP2,		QSERDES_PLL_LOCK_CMP2},
		{EMAC_QSERDES_COM_PLLLOCK_CMP3,		QSERDES_PLL_LOCK_CMP3},
		{EMAC_QSERDES_COM_PLLLOCK_CMP_EN,	PLLLOCK_CMP_EN},
		{EMAC_QSERDES_COM_RESETSM_CNTRL,	FRQ_TUNE_MODE},
		{END_MARKER,				END_MARKER}
	},
	/* EMAC_PHY_MAP_MDM9607 */
	{
		{EMAC_QSERDES_COM_PLL_IP_SETI,		QSERDES_PLL_IPSETI_MDM},
		{EMAC_QSERDES_COM_PLL_CP_SETI,		QSERDES_PLL_CP_SETI},
		{EMAC_QSERDES_COM_PLL_IP_SETP,		QSERDES_PLL_IP_SETP},
		{EMAC_QSERDES_COM_PLL_CP_SETP,		QSERDES_PLL_CP_SETP},
		{EMAC_QSERDES_COM_PLL_CRCTRL,		QSERDES_PLL_CRCTRL},
		{EMAC_QSERDES_COM_DEC_START1,		DEC_START1_MUX |
				QSERDES_PLL_DEC},
		{EMAC_QSERDES_COM_DEC_START2,		DEC_START2_MUX |
				DEC_START2},
		{EMAC_QSERDES_COM_DIV_FRAC_START1,	DIV_FRAC_START1_MUX |
				QSERDES_PLL_DIV_FRAC_START1},
		{EMAC_QSERDES_COM_DIV_FRAC_START2,	DIV_FRAC_START2_MUX |
				QSERDES_PLL_DIV_FRAC_START2},
		{EMAC_QSERDES_COM_DIV_FRAC_START3,	DIV_FRAC_START3_MUX |
				QSERDES_PLL_DIV_FRAC_START3},
		{EMAC_QSERDES_COM_PLLLOCK_CMP1,		QSERDES_PLL_LOCK_CMP1},
		{EMAC_QSERDES_COM_PLLLOCK_CMP2,		QSERDES_PLL_LOCK_CMP2},
		{EMAC_QSERDES_COM_PLLLOCK_CMP3,		QSERDES_PLL_LOCK_CMP3},
		{EMAC_QSERDES_COM_PLLLOCK_CMP_EN,	PLLLOCK_CMP_EN},
		{EMAC_QSERDES_COM_RESETSM_CNTRL,	FRQ_TUNE_MODE},
		{EMAC_QSERDES_COM_RES_TRIM_SEARCH,	RESTRIM_SEARCH},
		{EMAC_QSERDES_COM_BGTC,				BGTC},
		{END_MARKER,				END_MARKER},
	}
};

static const struct emac_reg_write cdr_setting[] = {
{EMAC_QSERDES_RX_CDR_CONTROL,	SECONDORDERENABLE |
		(QSERDES_RX_CDR_CTRL1_THRESH << FIRSTORDER_THRESH_SHFT) |
		(QSERDES_RX_CDR_CTRL1_GAIN << SECONDORDERGAIN_SHFT)},
{EMAC_QSERDES_RX_CDR_CONTROL2,	SECONDORDERENABLE |
		(QSERDES_RX_CDR_CTRL2_THRESH << FIRSTORDER_THRESH_SHFT) |
		(QSERDES_RX_CDR_CTRL2_GAIN << SECONDORDERGAIN_SHFT)},
{END_MARKER,				END_MARKER},
};

static const struct emac_reg_write tx_rx_setting[] = {
{EMAC_QSERDES_TX_BIST_MODE_LANENO,	QSERDES_TX_BIST_MODE_LANENO},
{EMAC_QSERDES_TX_TX_DRV_LVL,		TX_DRV_LVL_MUX |
			(QSERDES_TX_DRV_LVL << TX_DRV_LVL_SHFT)},
{EMAC_QSERDES_TX_TRAN_DRVR_EMP_EN,	EMP_EN_MUX | EMP_EN},
{EMAC_QSERDES_TX_TX_EMP_POST1_LVL,	TX_EMP_POST1_LVL_MUX |
			(QSERDES_TX_EMP_POST1_LVL << TX_EMP_POST1_LVL_SHFT)},
{EMAC_QSERDES_RX_RX_EQ_GAIN12,
			(QSERDES_RX_EQ_GAIN2 << RX_EQ_GAIN2_SHFT) |
			(QSERDES_RX_EQ_GAIN1 << RX_EQ_GAIN1_SHFT)},
{EMAC_QSERDES_TX_LANE_MODE,
			QSERDES_TX_LANE_MODE},
{END_MARKER,				END_MARKER}
};

static int emac_sgmii_v1_init(struct emac_adapter *adpt)
{
	int i;
	struct emac_phy *phy = &adpt->phy;
	struct emac_sgmii *sgmii = phy->private;

	emac_sgmii_init_link(adpt, phy->autoneg_advertised, phy->autoneg,
			     !phy->disable_fc_autoneg);

	emac_reg_write_all(
		sgmii->base,
		(const struct emac_reg_write *)
		&physical_coding_sublayer_programming[phy->board_id]);

	/* Ensure Rx/Tx lanes power configuration is written to hw before
	 * configuring the SerDes engine's clocks
	 */
	wmb();

	emac_reg_write_all(sgmii->base, sysclk_refclk_setting);
	emac_reg_write_all(
		sgmii->base,
		(const struct emac_reg_write *)&pll_setting[phy->board_id]);
	emac_reg_write_all(sgmii->base, cdr_setting);
	emac_reg_write_all(sgmii->base, tx_rx_setting);

	/* Ensure SerDes engine configuration is written to hw before powering
	 * it up
	 */
	wmb();

	writel_relaxed(SERDES_START, sgmii->base + EMAC_SGMII_PHY_SERDES_START);

	/* Ensure Rx/Tx SerDes engine power-up command is written to HW */
	wmb();

	for (i = 0; i < SERDES_START_WAIT_TIMES; i++) {
		if (readl_relaxed(sgmii->base + EMAC_QSERDES_COM_RESET_SM) &
		    QSERDES_READY)
			break;
		usleep_range(100, 200);
	}

	if (i == SERDES_START_WAIT_TIMES) {
		emac_err(adpt, "serdes failed to start\n");
		return -EIO;
	}
	/* Mask out all the SGMII Interrupt */
	writel_relaxed(0, sgmii->base + EMAC_SGMII_PHY_INTERRUPT_MASK);
	/* Ensure SGMII interrupts are masked out before clearing them */
	wmb();

	emac_hw_clear_sgmii_intr_status(adpt, SGMII_PHY_INTERRUPT_ERR);

	return 0;
}

static void emac_sgmii_v1_reset(struct emac_adapter *adpt)
{
	emac_clk_set_rate(adpt, EMAC_CLK_125M, EMC_CLK_RATE_19_2MHZ);
	emac_sgmii_reset_prepare(adpt);
	emac_sgmii_v1_init(adpt);
	emac_clk_set_rate(adpt, EMAC_CLK_125M, EMC_CLK_RATE_125MHZ);
}

int emac_sgmii_v1_link_setup_no_ephy(struct emac_adapter *adpt, u32 speed,
				     bool autoneg)
{
	struct emac_phy *phy = &adpt->phy;

	phy->autoneg		= autoneg;
	phy->autoneg_advertised	= speed;
	/* The AN_ENABLE and SPEED_CFG can't change on fly. The SGMII_PHY has
	 * to be re-initialized.
	 */
	emac_sgmii_reset_prepare(adpt);
	return emac_sgmii_v1_init(adpt);
}

struct emac_phy_ops emac_sgmii_v1_ops = {
	.config			= emac_sgmii_config,
	.up			= emac_sgmii_up,
	.down			= emac_sgmii_down,
	.init			= emac_sgmii_v1_init,
	.reset			= emac_sgmii_v1_reset,
	.init_ephy		= emac_sgmii_init_ephy_nop,
	.link_setup_no_ephy	= emac_sgmii_v1_link_setup_no_ephy,
	.link_check_no_ephy	= emac_sgmii_link_check_no_ephy,
	.tx_clk_set_rate	= emac_sgmii_tx_clk_set_rate_nop,
	.periodic_task		= emac_sgmii_periodic_check,
};
