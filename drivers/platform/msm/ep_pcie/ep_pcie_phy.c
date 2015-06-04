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

static u32 qserdes_com_oft;

void ep_pcie_phy_init(struct ep_pcie_dev_t *dev)
{
	EP_PCIE_DBG(dev,
		"PCIe V%d: PHY V%d: Initializing 20nm QMP phy - 100MHz\n",
		dev->rev, dev->phy_rev);

	switch (dev->phy_rev) {
	case 3:
		qserdes_com_oft = 8;
		break;
	default:
		qserdes_com_oft = 0;
	}

	ep_pcie_write_reg(dev->phy, PCIE_PHY_POWER_DOWN_CONTROL, 0x01);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_SYS_CLK_CTRL, 0x1E);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_PLL_CP_SETI, 0x11);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_PLL_IP_SETP, 0x3F);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_PLL_CP_SETP, 0x00);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_PLL_IP_SETI, 0x3F);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_IP_TRIM, 0x0F);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_RESETSM_CNTRL, 0x90);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_RES_CODE_CAL_CSR
					+ qserdes_com_oft, 0x77);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_RES_TRIM_CONTROL
					+ qserdes_com_oft, 0x15);
	ep_pcie_write_reg(dev->phy, QSERDES_TX_RCV_DETECT_LVL, 0x03);
	ep_pcie_write_reg(dev->phy, QSERDES_RX_RX_EQ_GAIN1_LSB, 0xFF);
	ep_pcie_write_reg(dev->phy, QSERDES_RX_RX_EQ_GAIN2_LSB, 0xFF);
	ep_pcie_write_reg(dev->phy, QSERDES_RX_RX_EQ_GAIN2_MSB, 0x00);
	ep_pcie_write_reg(dev->phy, QSERDES_RX_SIGDET_ENABLES, 0x40);
	ep_pcie_write_reg(dev->phy, QSERDES_RX_SIGDET_CNTRL, 0x70);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_PWRUP_RESET_DLY_TIME_SYSCLK, 0xC8);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_POWER_STATE_CONFIG1, 0xA3);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_POWER_STATE_CONFIG2, 0x1B);

	ep_pcie_write_reg(dev->phy, QSERDES_COM_PLL_VCOTAIL_EN, 0xE1);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_RESETSM_CNTRL2, 0x07);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_IE_TRIM, 0x3F);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_PLL_CNTRL, 0x46);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_PLLLOCK_CMP2
					+ qserdes_com_oft, 0x05);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_PLLLOCK_CMP_EN
					+ qserdes_com_oft, 0x03);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_DEC_START1
					+ qserdes_com_oft, 0x99);
	ep_pcie_write_reg(dev->phy, QSERDES_RX_CDR_CONTROL1, 0xF5);
	ep_pcie_write_reg(dev->phy, QSERDES_RX_CDR_CONTROL_HALF, 0x2C);
	ep_pcie_write_reg(dev->phy, QSERDES_COM_RES_CODE_START_SEG1
					+ qserdes_com_oft, 0x24);
	ep_pcie_write_reg(dev->phy, QSERDES_RX_RX_EQ_GAIN1_MSB, 0x07);
	ep_pcie_write_reg(dev->phy, QSERDES_RX_RX_EQU_ADAPTOR_CNTRL2, 0x1E);
	ep_pcie_write_reg(dev->phy, QSERDES_RX_RX_EQ_OFFSET_ADAPTOR_CNTRL1,
									0x67);
	ep_pcie_write_reg(dev->phy, QSERDES_RX_RX_OFFSET_ADAPTOR_CNTRL2, 0x80);
	ep_pcie_write_reg(dev->phy, QSERDES_RX_SIGDET_DEGLITCH_CNTRL, 0x0C);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_PWRUP_RESET_DLY_TIME_AUXCLK, 0x80);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_RX_IDLE_DTCT_CNTRL, 0x4D);

	if (dev->phy_rev == 1) {
		ep_pcie_write_reg(dev->phy, QSERDES_RX_RX_RCVR_IQ_EN, 0x31);
		ep_pcie_write_reg(dev->phy, QSERDES_COM_RESETSM_CNTRL2, 0x5);
		ep_pcie_write_reg(dev->phy, QSERDES_COM_PLL_VCOTAIL_EN, 0x1);
	} else if (dev->phy_rev == 3) {
		ep_pcie_write_reg(dev->phy, QSERDES_COM_RES_CODE_START_SEG1
						+ qserdes_com_oft, 0x20);

		ep_pcie_write_reg(dev->phy, QSERDES_COM_PLL_CP_SETI, 0x3F);
		ep_pcie_write_reg(dev->phy, QSERDES_COM_PLL_IP_SETP, 0x34);
		ep_pcie_write_reg(dev->phy, QSERDES_COM_IE_TRIM, 0x0F);
		ep_pcie_write_reg(dev->phy, QSERDES_RX_CDR_CONTROL1, 0xF4);
		ep_pcie_write_reg(dev->phy, QSERDES_RX_RX_EQ_GAIN1_MSB, 0x1F);
		ep_pcie_write_reg(dev->phy, QSERDES_RX_SIGDET_CNTRL, 0x90);
		ep_pcie_write_reg(dev->phy, QSERDES_RX_SIGDET_DEGLITCH_CNTRL,
									0x06);

		ep_pcie_write_reg(dev->phy, QSERDES_COM_PLL_CRCTRL
						+ qserdes_com_oft, 0x09);

		ep_pcie_write_reg(dev->phy,
				QSERDES_RX_UCDR_SO_SATURATION_AND_ENABLE, 0x49);
		ep_pcie_write_reg(dev->phy, QSERDES_RX_UCDR_FO_GAIN, 0x09);
		ep_pcie_write_reg(dev->phy, QSERDES_RX_UCDR_SO_GAIN, 0x04);
	}

	ep_pcie_write_reg(dev->phy, PCIE_PHY_SW_RESET, 0x00);
	ep_pcie_write_reg(dev->phy, PCIE_PHY_START, 0x03);
}

bool ep_pcie_phy_is_ready(struct ep_pcie_dev_t *dev)
{
	if (readl_relaxed(dev->phy + PCIE_PHY_PCS_STATUS) & BIT(6))
		return false;
	else
		return true;
}
