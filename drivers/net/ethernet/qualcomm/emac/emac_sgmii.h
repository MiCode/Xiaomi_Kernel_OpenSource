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

#ifndef _EMAC_SGMII_H_
#define _EMAC_SGMII_H_

#include "emac.h"

/* EMAC_QSERDES register offsets */
#define EMAC_QSERDES_COM_SYS_CLK_CTRL            0x000000
#define EMAC_QSERDES_COM_PLL_VCOTAIL_EN		 0x000004
#define EMAC_QSERDES_COM_PLL_CNTRL               0x000014
#define EMAC_QSERDES_COM_PLL_IP_SETI             0x000018
#define EMAC_QSERDES_COM_PLL_CP_SETI             0x000024
#define EMAC_QSERDES_COM_PLL_IP_SETP             0x000028
#define EMAC_QSERDES_COM_PLL_CP_SETP             0x00002c
#define EMAC_QSERDES_COM_SYSCLK_EN_SEL           0x000038
#define EMAC_QSERDES_COM_RESETSM_CNTRL           0x000040
#define EMAC_QSERDES_COM_PLLLOCK_CMP1            0x000044
#define EMAC_QSERDES_COM_PLLLOCK_CMP2            0x000048
#define EMAC_QSERDES_COM_PLLLOCK_CMP3            0x00004c
#define EMAC_QSERDES_COM_PLLLOCK_CMP_EN          0x000050
#define EMAC_QSERDES_COM_BGTC			 0x000058
#define EMAC_QSERDES_COM_DEC_START1              0x000064
#define EMAC_QSERDES_COM_RES_TRIM_SEARCH	 0x000088
#define EMAC_QSERDES_COM_DIV_FRAC_START1         0x000098
#define EMAC_QSERDES_COM_DIV_FRAC_START2         0x00009c
#define EMAC_QSERDES_COM_DIV_FRAC_START3         0x0000a0
#define EMAC_QSERDES_COM_DEC_START2              0x0000a4
#define EMAC_QSERDES_COM_PLL_CRCTRL              0x0000ac
#define EMAC_QSERDES_COM_RESET_SM                0x0000bc
#define EMAC_QSERDES_TX_BIST_MODE_LANENO         0x000100
#define EMAC_QSERDES_TX_TX_EMP_POST1_LVL         0x000108
#define EMAC_QSERDES_TX_TX_DRV_LVL               0x00010c
#define EMAC_QSERDES_TX_LANE_MODE                0x000150
#define EMAC_QSERDES_TX_TRAN_DRVR_EMP_EN         0x000170
#define EMAC_QSERDES_RX_CDR_CONTROL              0x000200
#define EMAC_QSERDES_RX_CDR_CONTROL2             0x000210
#define EMAC_QSERDES_RX_RX_EQ_GAIN12             0x000230

/* EMAC_SGMII register offsets */
#define EMAC_SGMII_PHY_SERDES_START              0x000300
#define EMAC_SGMII_PHY_CMN_PWR_CTRL              0x000304
#define EMAC_SGMII_PHY_RX_PWR_CTRL               0x000308
#define EMAC_SGMII_PHY_TX_PWR_CTRL               0x00030C
#define EMAC_SGMII_PHY_LANE_CTRL1                0x000318
#define EMAC_SGMII_PHY_AUTONEG_CFG2              0x000348
#define EMAC_SGMII_PHY_CDR_CTRL0                 0x000358
#define EMAC_SGMII_PHY_SPEED_CFG1                0x000374
#define EMAC_SGMII_PHY_POW_DWN_CTRL0             0x000380
#define EMAC_SGMII_PHY_RESET_CTRL                0x0003a8
#define EMAC_SGMII_PHY_IRQ_CMD                   0x0003ac
#define EMAC_SGMII_PHY_INTERRUPT_CLEAR           0x0003b0
#define EMAC_SGMII_PHY_INTERRUPT_MASK            0x0003b4
#define EMAC_SGMII_PHY_INTERRUPT_STATUS          0x0003b8
#define EMAC_SGMII_PHY_RX_CHK_STATUS             0x0003d4
#define EMAC_SGMII_PHY_AUTONEG0_STATUS           0x0003e0
#define EMAC_SGMII_PHY_AUTONEG1_STATUS           0x0003e4

#define SGMII_CDR_MAX_CNT               0x0f

#define QSERDES_PLL_IPSETI_DEF          0x01
#define QSERDES_PLL_IPSETI_MDM          0x03
#define QSERDES_PLL_CP_SETI             0x3b
#define QSERDES_PLL_IP_SETP             0x0a
#define QSERDES_PLL_CP_SETP             0x09
#define QSERDES_PLL_CRCTRL              0xfb
#define QSERDES_PLL_DEC                    2
#define QSERDES_PLL_DIV_FRAC_START1     0x55
#define QSERDES_PLL_DIV_FRAC_START2     0x2a
#define QSERDES_PLL_DIV_FRAC_START3     0x03
#define QSERDES_PLL_LOCK_CMP1           0x2b
#define QSERDES_PLL_LOCK_CMP2           0x68
#define QSERDES_PLL_LOCK_CMP3           0x00

#define QSERDES_RX_CDR_CTRL1_THRESH     0x03
#define QSERDES_RX_CDR_CTRL1_GAIN       0x02
#define QSERDES_RX_CDR_CTRL2_THRESH     0x03
#define QSERDES_RX_CDR_CTRL2_GAIN       0x04
#define QSERDES_RX_EQ_GAIN2_DEF              0xf
#define QSERDES_RX_EQ_GAIN1_DEF              0xf
#define QSERDES_RX_EQ_GAIN2_MDM              0x3
#define QSERDES_RX_EQ_GAIN1_MDM              0x3

#define QSERDES_TX_BIST_MODE_LANENO     0x00
#define QSERDES_TX_DRV_LVL_DEF              0x0f
#define QSERDES_TX_EMP_POST1_LVL_DEF           1
#define QSERDES_TX_LANE_MODE_DEF            0x08
#define QSERDES_TX_DRV_LVL_MDM              0x0c
#define QSERDES_TX_EMP_POST1_LVL_MDM           4
#define QSERDES_TX_LANE_MODE_MDM            0x00

#define SGMII_PHY_IRQ_CLR_WAIT_TIME     10

#define SGMII_PHY_INTERRUPT_ERR (\
	DECODE_CODE_ERR         |\
	DECODE_DISP_ERR)

#define SGMII_ISR_AN_MASK       (\
	AN_REQUEST              |\
	AN_START                |\
	AN_END                  |\
	AN_ILLEGAL_TERM         |\
	PLL_UNLOCK              |\
	SYNC_FAIL)

#define SGMII_ISR_MASK          (\
	SGMII_PHY_INTERRUPT_ERR |\
	SGMII_ISR_AN_MASK)

/* SGMII TX_CONFIG */
#define TXCFG_LINK                      0x8000
#define TXCFG_MODE_BMSK                 0x1c00
#define TXCFG_1000_FULL                 0x1800
#define TXCFG_100_FULL                  0x1400
#define TXCFG_100_HALF                  0x0400
#define TXCFG_10_FULL                   0x1000
#define TXCFG_10_HALF                   0x0000

#define SERDES_START_WAIT_TIMES         100

struct emac_sgmii {
	void __iomem	*base;
	/* Points to digital part of the per lane base address */
	void __iomem	*laned;
	int		irq;
};

int emac_sgmii_config(struct platform_device *pdev, struct emac_adapter *adpt);
int emac_sgmii_link_check_no_ephy(struct emac_adapter *adpt, u32 *speed,
				  bool *link_up);
int emac_hw_clear_sgmii_intr_status(struct emac_adapter *adpt, u32 irq_bits);
int emac_sgmii_up(struct emac_adapter *adpt);
int emac_sgmii_autoneg_check(struct emac_adapter *adpt, u32 *speed,
			     bool *link_up);
int emac_sgmii_init_ephy_nop(struct emac_adapter *adpt);
int emac_sgmii_v1_init_link(struct emac_adapter *adpt, u32 speed, bool autoneg,
			    bool fc);
int emac_sgmii_init_link(struct emac_adapter *adpt, u32 speed, bool autoneg,
			 bool fc);
void emac_sgmii_reset_prepare(struct emac_adapter *adpt);
void emac_sgmii_down(struct emac_adapter *adpt);
void emac_sgmii_tx_clk_set_rate_nop(struct emac_adapter *adpt);
void emac_sgmii_periodic_check(struct emac_adapter *adpt);

irqreturn_t emac_sgmii_isr(int _irq, void *data);

#endif /*_EMAC_SGMII_H_*/
