/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

/*
 * MSM PCIe PHY endpoint mode
 */

#include "ep_pcie_com.h"
#include "ep_pcie_phy.h"

void ep_pcie_phy_init(struct ep_pcie_dev_t *dev)
{
	EP_PCIE_DBG(dev,
		"PCIe V%d: PHY V%d: Initializing 14nm QMP phy - 100MHz\n",
		dev->rev, dev->phy_rev);

	ep_pcie_write_reg(dev->phy, PCIE_PHY_SW_RESET, 0x01);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_POWER_DOWN_CONTROL, 0x01);

	ep_pcie_write_reg(dev->phy, QSERDES_COM_SYSCLK_EN_SEL, 0x00);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_BIAS_EN_CLKBUFLR_EN, 0x18);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_CLK_SELECT, 0x30);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_RESETSM_CNTRL, 0x20);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_CMN_CONFIG, 0x06);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_DEC_START_MODE0, 0x19);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_CP_CTRL_MODE0, 0x3F);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_PLL_RCTRL_MODE0, 0x1A);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_PLL_CCTRL_MODE0, 0x00);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_INTEGLOOP_GAIN1_MODE0, 0x00);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_INTEGLOOP_GAIN0_MODE0, 0xFF);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_SVS_MODE_CLK_SEL, 0x01);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_CORE_CLK_EN, 0x00);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_LOCK_CMP2_MODE0, 0x04);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_LOCK_CMP1_MODE0, 0xFF);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_LOCK_CMP_EN, 0x42);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_VCO_TUNE_MAP, 0x00);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_VCO_TUNE_TIMER2, 0x3F);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_VCO_TUNE_TIMER1, 0xFF);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_BG_TIMER, 0x01);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_BG_TRIM, 0x0F);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_PLL_IVCO, 0x0F);

	ep_pcie_write_reg(dev->phy, QSERDES_RX_UCDR_SO_SATURATION_AND_ENABLE,
					0x4B);
	ep_pcie_write_reg(dev->phy, QSERDES_RX_UCDR_FO_GAIN, 0x0A);
	ep_pcie_write_reg(dev->phy, QSERDES_RX_UCDR_FO_GAIN_HALF, 0x0A);
	ep_pcie_write_reg(dev->phy, QSERDES_RX_UCDR_SO_GAIN, 0x04);
	ep_pcie_write_reg(dev->phy, QSERDES_RX_UCDR_SO_GAIN_HALF, 0x04);
	ep_pcie_write_reg(dev->phy, QSERDES_RX_RX_EQU_ADAPTOR_CNTRL2, 0x01);
	ep_pcie_write_reg(dev->phy, QSERDES_RX_RX_EQU_ADAPTOR_CNTRL3, 0x00);
	ep_pcie_write_reg(dev->phy, QSERDES_RX_RX_EQU_ADAPTOR_CNTRL4, 0xDB);
	ep_pcie_write_reg(dev->phy, QSERDES_RX_RX_EQ_OFFSET_ADAPTOR_CNTRL1,
					0x77);
	ep_pcie_write_reg(dev->phy, QSERDES_RX_RX_OFFSET_ADAPTOR_CNTRL2, 0x80);
	ep_pcie_write_reg(dev->phy, QSERDES_RX_SIGDET_ENABLES, 0x1C);
	ep_pcie_write_reg(dev->phy, QSERDES_RX_SIGDET_CNTRL, 0x03);
	ep_pcie_write_reg(dev->phy, QSERDES_RX_SIGDET_DEGLITCH_CNTRL, 0x14);

	ep_pcie_write_reg(dev->phy, QSERDES_TX_RES_CODE_LANE_OFFSET, 0x02);
	ep_pcie_write_reg(dev->phy, QSERDES_TX_HIGHZ_TRANSCEIVEREN_BIAS_DRVR_EN,
					0x45);
	ep_pcie_write_reg(dev->phy, QSERDES_TX_RCV_DETECT_LVL_2, 0x12);
	ep_pcie_write_reg(dev->phy, QSERDES_TX_LANE_MODE, 0x06);

	ep_pcie_write_reg(dev->phy, PCIE_PHY_OSC_DTCT_ACTIONS, 0x0A);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_RX_IDLE_DTCT_CNTRL, 0x4C);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_TXDEEMPH_M3P5DB_V0, 0x0F);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_POWER_STATE_CONFIG1, 0xA3);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_POWER_STATE_CONFIG2, 0x1B);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_POWER_STATE_CONFIG4, 0x00);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_RCVR_DTCT_DLY_P1U2_L, 0xF1);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_RCVR_DTCT_DLY_P1U2_H, 0x01);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_LOCK_DETECT_CONFIG1, 0x80);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_LOCK_DETECT_CONFIG2, 0x02);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_LOCK_DETECT_CONFIG3, 0x40);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_LOCK_DETECT_CONFIG4, 0x07);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_PWRUP_RESET_DLY_TIME_AUXCLK, 0x40);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_LP_WAKEUP_DLY_TIME_AUXCLK, 0x40);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_L1SS_WAKEUP_DLY_TIME_AUXCLK_LSB,
					0x40);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_PLL_LOCK_CHK_DLY_TIME, 0x73);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_ELECIDLE_DLY_SEL, 0x01);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_SIGDET_CNTRL, 0x0F);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_RX_SIGDET_LVL, 0x77);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_SW_RESET, 0x00);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_START_CONTROL, 0x03);
}

bool ep_pcie_phy_is_ready(struct ep_pcie_dev_t *dev)
{
	if (readl_relaxed(dev->phy + PCIE_PHY_PCS_STATUS) & BIT(6))
		return false;
	else
		return true;
}
