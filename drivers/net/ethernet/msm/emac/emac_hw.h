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

#ifndef _EMAC_HW_H_
#define _EMAC_HW_H_

#include <linux/mii.h>

#include "emac.h"
#include "emac_regs.h"
#include "emac_defines.h"

/* function prototype */

/* REG */
extern u32 emac_reg_r32(struct emac_hw *hw, u8 base, u32 reg);
extern void emac_reg_w32(struct emac_hw *hw, u8 base, u32 reg, u32 val);
extern void emac_reg_update32(struct emac_hw *hw, u8 base, u32 reg,
			      u32 mask, u32 val);
extern u32 emac_reg_field_r32(struct emac_hw *hw, u8 base, u32 reg,
			      u32 mask, u32 shift);
/* PHY */
extern int emac_hw_read_phy_reg(struct emac_hw *hw, bool ext, u8 dev,
				bool fast, u16 reg_addr, u16 *phy_data);
extern int emac_hw_write_phy_reg(struct emac_hw *hw, bool ext, u8 dev,
				 bool fast, u16 reg_addr, u16 phy_data);
extern int emac_read_phy_reg(struct emac_hw *hw, u16 phy_addr,
			     u16 reg_addr, u16 *phy_data);
extern int emac_write_phy_reg(struct emac_hw *hw, u16 phy_addr,
			      u16 reg_addr, u16 phy_data);
extern int emac_setup_phy_link(struct emac_hw *hw, u32 speed,
			       bool autoneg, bool fc);
extern int emac_setup_phy_link_speed(struct emac_hw *hw, u32 speed,
				     bool autoneg, bool fc);
extern int emac_check_phy_link(struct emac_hw *hw, u32 *speed, bool *link_up);
extern int emac_hw_get_lpa_speed(struct emac_hw *hw, u32 *speed);
extern int emac_hw_ack_phy_intr(struct emac_hw *hw);
extern int emac_hw_init_phy(struct emac_hw *hw);
extern int emac_hw_init_ephy(struct emac_hw *hw);
extern int emac_hw_init_sgmii(struct emac_hw *hw);
extern int emac_hw_reset_sgmii(struct emac_hw *hw);
extern int emac_check_sgmii_link(struct emac_hw *hw, u32 *speed, bool *linkup);
extern int emac_check_sgmii_autoneg(struct emac_hw *hw, u32 *speed,
				    bool *linkup);
extern int emac_hw_clear_sgmii_intr_status(struct emac_hw *hw, u32 irq_bits);
extern void emac_hw_config_pow_save(struct emac_hw *hw, u32 speed, bool wol_en,
				    bool rx_en);
/* MAC */
extern void emac_hw_enable_intr(struct emac_hw *hw);
extern void emac_hw_disable_intr(struct emac_hw *hw);
extern void emac_hw_set_mc_addr(struct emac_hw *hw, u8 *addr);
extern void emac_hw_clear_mc_addr(struct emac_hw *hw);

extern void emac_hw_config_mac_ctrl(struct emac_hw *hw);
extern void emac_hw_config_rss(struct emac_hw *hw);
extern void emac_hw_config_wol(struct emac_hw *hw, u32 wufc);
extern int emac_hw_config_fc(struct emac_hw *hw);

extern void emac_hw_reset_mac(struct emac_hw *hw);
extern void emac_hw_config_mac(struct emac_hw *hw);
extern void emac_hw_start_mac(struct emac_hw *hw);
extern void emac_hw_stop_mac(struct emac_hw *hw);

extern void emac_hw_set_mac_addr(struct emac_hw *hw, u8 *addr);

/* TX Timestamp */
extern bool emac_hw_read_tx_tstamp(struct emac_hw *hw,
				   struct emac_hwtxtstamp *ts);

#define IMR_NORMAL_MASK         (\
		ISR_ERROR       |\
		ISR_GPHY_LINK   |\
		ISR_TX_PKT      |\
		GPHY_WAKEUP_INT)

#define IMR_EXTENDED_MASK       (\
		SW_MAN_INT      |\
		ISR_OVER        |\
		ISR_ERROR       |\
		ISR_GPHY_LINK   |\
		ISR_TX_PKT      |\
		GPHY_WAKEUP_INT)

#define ISR_RX_PKT      (\
	RX_PKT_INT0     |\
	RX_PKT_INT1     |\
	RX_PKT_INT2     |\
	RX_PKT_INT3)

#define ISR_TX_PKT      (\
	TX_PKT_INT      |\
	TX_PKT_INT1     |\
	TX_PKT_INT2     |\
	TX_PKT_INT3)

#define ISR_GPHY_LINK        (\
	GPHY_LINK_UP_INT     |\
	GPHY_LINK_DOWN_INT)

#define ISR_OVER        (\
	RFD0_UR_INT     |\
	RFD1_UR_INT     |\
	RFD2_UR_INT     |\
	RFD3_UR_INT     |\
	RFD4_UR_INT     |\
	RXF_OF_INT      |\
	TXF_UR_INT)

#define ISR_ERROR       (\
	DMAR_TO_INT     |\
	DMAW_TO_INT     |\
	TXQ_TO_INT)

#define REG_MAC_RX_STATUS_BIN           EMAC_RXMAC_STATC_REG0
#define REG_MAC_RX_STATUS_END           EMAC_RXMAC_STATC_REG22
#define REG_MAC_TX_STATUS_BIN           EMAC_TXMAC_STATC_REG0
#define REG_MAC_TX_STATUS_END           EMAC_TXMAC_STATC_REG24

#define RXQ0_NUM_RFD_PREF_DEF           8
#define TXQ0_NUM_TPD_PREF_DEF           5

#define EMAC_PREAMBLE_DEF               7

#define DMAR_DLY_CNT_DEF                15
#define DMAW_DLY_CNT_DEF                4

#define MDIO_CLK_25_4                   0
#define MDIO_CLK_25_28                  7

#define RXQ0_RSS_HSTYP_IPV6_TCP_EN      0x20
#define RXQ0_RSS_HSTYP_IPV6_EN          0x10
#define RXQ0_RSS_HSTYP_IPV4_TCP_EN      0x8
#define RXQ0_RSS_HSTYP_IPV4_EN          0x4

#define MASTER_CTRL_CLK_SEL_DIS         0x1000

#define MDIO_WAIT_TIMES                 1000

#define SERDES_START_WAIT_TIMES         100

#define SGMII_CDR_MAX_CNT               0x0f

#define QSERDES_PLL_IPSETI              0x01
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
#define QSERDES_RX_EQ_GAIN2              0xf
#define QSERDES_RX_EQ_GAIN1              0xf

#define QSERDES_TX_BIST_MODE_LANENO     0x00
#define QSERDES_TX_DRV_LVL              0x0f
#define QSERDES_TX_EMP_POST1_LVL           1
#define QSERDES_TX_LANE_MODE            0x08

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

/* PHY */
#define MII_PSSR                        0x11 /* PHY Specific Status Reg */
#define MII_DBG_ADDR                    0x1D /* PHY Debug Address Reg */
#define MII_DBG_DATA                    0x1E /* PHY Debug Data Reg */

/* MII_BMCR (0x00) */
#define BMCR_SPEED10                    0x0000

/* MII_PSSR (0x11) */
#define PSSR_FC_RXEN                    0x0004
#define PSSR_FC_TXEN                    0x0008
#define PSSR_SPD_DPLX_RESOLVED          0x0800  /* 1=Speed & Duplex resolved */
#define PSSR_DPLX                       0x2000  /* 1=Duplex 0=Half Duplex */
#define PSSR_SPEED                      0xC000  /* Speed, bits 14:15 */
#define PSSR_10MBS                      0x0000  /* 00=10Mbs */
#define PSSR_100MBS                     0x4000  /* 01=100Mbs */
#define PSSR_1000MBS                    0x8000  /* 10=1000Mbs */

/* MII DBG registers */
#define HIBERNATE_CTRL_REG              0xB

/* HIBERNATE_CTRL_REG */
#define HIBERNATE_EN                    0x8000

#endif /*_EMAC_HW_H_*/
