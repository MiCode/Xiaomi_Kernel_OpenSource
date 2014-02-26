/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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
 * MSM PCIe PHY driver.
 */

#include <linux/io.h>
#include <mach/msm_iomap.h>
#include "pcie.h"
#include "pcie_phy.h"

static inline void write_phy(void *base, u32 offset, u32 value)
{
	writel_relaxed(value, base + offset);
	wmb();
}

#ifndef CONFIG_ARCH_MDM9630
static inline void pcie20_phy_init_default(struct msm_pcie_dev_t *dev)
{

	PCIE_DBG("Initializing 28nm QMP phy - 19.2MHz\n");

	write_phy(dev->phy, PCIE_PHY_POWER_DOWN_CONTROL,		0x03);
	write_phy(dev->phy, QSERDES_COM_SYSCLK_EN_SEL,		0x08);
	write_phy(dev->phy, QSERDES_COM_DEC_START1,			0x82);
	write_phy(dev->phy, QSERDES_COM_DEC_START2,			0x03);
	write_phy(dev->phy, QSERDES_COM_DIV_FRAC_START1,		0xd5);
	write_phy(dev->phy, QSERDES_COM_DIV_FRAC_START2,		0xaa);
	write_phy(dev->phy, QSERDES_COM_DIV_FRAC_START3,		0x13);
	write_phy(dev->phy, QSERDES_COM_PLLLOCK_CMP_EN,		0x01);
	write_phy(dev->phy, QSERDES_COM_PLLLOCK_CMP1,		0x2b);
	write_phy(dev->phy, QSERDES_COM_PLLLOCK_CMP2,		0x68);
	write_phy(dev->phy, QSERDES_COM_PLL_CRCTRL,			0xff);
	write_phy(dev->phy, QSERDES_COM_PLL_CP_SETI,		0x3f);
	write_phy(dev->phy, QSERDES_COM_PLL_IP_SETP,		0x07);
	write_phy(dev->phy, QSERDES_COM_PLL_CP_SETP,		0x03);
	write_phy(dev->phy, QSERDES_RX_CDR_CONTROL,			0xf3);
	write_phy(dev->phy, QSERDES_RX_CDR_CONTROL2,		0x6b);
	write_phy(dev->phy, QSERDES_COM_RESETSM_CNTRL,		0x10);
	write_phy(dev->phy, QSERDES_RX_RX_TERM_HIGHZ_CM_AC_COUPLE,	0x87);
	write_phy(dev->phy, QSERDES_RX_RX_EQ_GAIN12,		0x54);
	write_phy(dev->phy, PCIE_PHY_POWER_STATE_CONFIG1,		0xa3);
	write_phy(dev->phy, PCIE_PHY_POWER_STATE_CONFIG2,		0xcb);
	write_phy(dev->phy, QSERDES_COM_PLL_RXTXEPCLK_EN,		0x10);
	write_phy(dev->phy, PCIE_PHY_ENDPOINT_REFCLK_DRIVE,		0x10);
	write_phy(dev->phy, PCIE_PHY_SW_RESET,			0x00);
	write_phy(dev->phy, PCIE_PHY_START,				0x03);
}
#endif

#ifdef CONFIG_ARCH_MDM9630
void pcie_phy_init(struct msm_pcie_dev_t *dev)
{

	PCIE_DBG("Initializing 28nm QMP phy - 19.2MHz\n");

	write_phy(dev->phy, PCIE_PHY_POWER_DOWN_CONTROL, 0x03);

	write_phy(dev->phy, QSERDES_COM_SYSCLK_EN_SEL_TXBAND, 0x08);
	write_phy(dev->phy, QSERDES_COM_DEC_START1, 0x82);
	write_phy(dev->phy, QSERDES_COM_DEC_START2, 0x03);
	write_phy(dev->phy, QSERDES_COM_DIV_FRAC_START1, 0xD5);
	write_phy(dev->phy, QSERDES_COM_DIV_FRAC_START2, 0xAA);
	write_phy(dev->phy, QSERDES_COM_DIV_FRAC_START3, 0x4D);
	write_phy(dev->phy, QSERDES_COM_PLLLOCK_CMP_EN, 0x01);
	write_phy(dev->phy, QSERDES_COM_PLLLOCK_CMP1, 0x2B);
	write_phy(dev->phy, QSERDES_COM_PLLLOCK_CMP2, 0x68);
	write_phy(dev->phy, QSERDES_COM_PLL_CRCTRL, 0x7C);
	write_phy(dev->phy, QSERDES_COM_PLL_CP_SETI, 0x02);
	write_phy(dev->phy, QSERDES_COM_PLL_IP_SETP, 0x1F);
	write_phy(dev->phy, QSERDES_COM_PLL_CP_SETP, 0x0F);
	write_phy(dev->phy, QSERDES_COM_PLL_IP_SETI, 0x01);
	write_phy(dev->phy, QSERDES_COM_IE_TRIM, 0x0F);
	write_phy(dev->phy, QSERDES_COM_IP_TRIM, 0x0F);
	write_phy(dev->phy, QSERDES_COM_PLL_CNTRL, 0x46);

	/* CDR Settings */
	write_phy(dev->phy, QSERDES_RX_CDR_CONTROL1, 0xF3);
	write_phy(dev->phy, QSERDES_RX_CDR_CONTROL_HALF, 0x2B);

	/* Calibration Settings */
	write_phy(dev->phy, QSERDES_COM_RESETSM_CNTRL, 0x90);
	write_phy(dev->phy, QSERDES_COM_RESETSM_CNTRL2, 0x05);

	/* Additional writes */
	write_phy(dev->phy, QSERDES_COM_RES_CODE_START_SEG1, 0x20);
	write_phy(dev->phy, QSERDES_COM_RES_CODE_CAL_CSR, 0x77);
	write_phy(dev->phy, QSERDES_COM_RES_TRIM_CONTROL, 0x15);
	write_phy(dev->phy, QSERDES_TX_RCV_DETECT_LVL, 0x03);
	write_phy(dev->phy, QSERDES_RX_RX_EQ_GAIN1_LSB, 0xFF);
	write_phy(dev->phy, QSERDES_RX_RX_EQ_GAIN1_MSB, 0x1F);
	write_phy(dev->phy, QSERDES_RX_RX_EQ_GAIN2_LSB, 0xFF);
	write_phy(dev->phy, QSERDES_RX_RX_EQ_GAIN2_MSB, 0x00);
	write_phy(dev->phy, QSERDES_RX_RX_EQU_ADAPTOR_CNTRL2, 0x1A);
	write_phy(dev->phy, QSERDES_RX_RX_EQ_OFFSET_ADAPTOR_CNTRL1, 0x80);
	write_phy(dev->phy, QSERDES_RX_SIGDET_ENABLES, 0x40);
	write_phy(dev->phy, QSERDES_RX_SIGDET_CNTRL, 0x70);
	write_phy(dev->phy, QSERDES_RX_SIGDET_DEGLITCH_CNTRL, 0x06);
	write_phy(dev->phy, QSERDES_COM_PLL_RXTXEPCLK_EN, 0x10);
	write_phy(dev->phy, PCIE_PHY_ENDPOINT_REFCLK_DRIVE, 0x10);
	write_phy(dev->phy, PCIE_PHY_POWER_STATE_CONFIG1, 0x23);
	write_phy(dev->phy, PCIE_PHY_POWER_STATE_CONFIG2, 0xCB);
	write_phy(dev->phy, QSERDES_RX_RX_RCVR_IQ_EN, 0x31);

	write_phy(dev->phy, PCIE_PHY_SW_RESET, 0x00);
	write_phy(dev->phy, PCIE_PHY_START, 0x03);
}
#elif defined(CONFIG_ARCH_FSM9900)
void pcie_phy_init(struct msm_pcie_dev_t *dev)
{
	if (dev->ext_ref_clk == false) {
		pcie20_phy_init_default(dev);
		return;
	}

	PCIE_DBG("Initializing 28nm ATE phy - 100MHz\n");

	/*  1 */
	write_phy(dev->phy, PCIE_PHY_POWER_DOWN_CONTROL, 0x01);
	/*  2 */
	write_phy(dev->phy, QSERDES_COM_SYS_CLK_CTRL, 0x3e);
	/*  3 */
	write_phy(dev->phy, QSERDES_COM_PLL_CP_SETI, 0x0f);
	/*  4 */
	write_phy(dev->phy, QSERDES_COM_PLL_IP_SETP, 0x23);
	/*  5 */
	write_phy(dev->phy, QSERDES_COM_PLL_IP_SETI, 0x3f);
	/*  6 */
	write_phy(dev->phy, QSERDES_RX_CDR_CONTROL, 0xf3);
	/*  7 */
	write_phy(dev->phy, QSERDES_RX_CDR_CONTROL2, 0x6b);
	/*  8 */
	write_phy(dev->phy, QSERDES_COM_RESETSM_CNTRL, 0x10);
	/*  9 */
	write_phy(dev->phy, QSERDES_RX_RX_TERM_HIGHZ_CM_AC_COUPLE, 0x87);
	/* 10 */
	write_phy(dev->phy, QSERDES_RX_RX_EQ_GAIN12, 0x54);
	/* 11 */
	write_phy(dev->phy, PCIE_PHY_POWER_STATE_CONFIG1, 0xa3);
	/* 12 */
	write_phy(dev->phy, PCIE_PHY_POWER_STATE_CONFIG2, 0x1b);
	/* 13 */
	write_phy(dev->phy, PCIE_PHY_SW_RESET,		0x00);
	/* 14 */
	write_phy(dev->phy, PCIE_PHY_START,		0x03);
}
#else
void pcie_phy_init(struct msm_pcie_dev_t *dev)
{

	pcie20_phy_init_default(dev);
}

#endif

bool pcie_phy_is_ready(struct msm_pcie_dev_t *dev)
{
	if (readl_relaxed(dev->phy + PCIE_PHY_PCS_STATUS) & BIT(6))
		return false;
	else
		return true;
}
