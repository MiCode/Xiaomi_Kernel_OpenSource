/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
extern int emac_read_phy_reg(struct emac_hw *hw, u16 reg_addr, u16 *phy_data);
extern int emac_write_phy_reg(struct emac_hw *hw, u16 reg_addr, u16 phy_data);
extern int emac_setup_phy_link(struct emac_hw *hw, u32 speed,
			       bool autoneg, bool fc);
extern int emac_setup_phy_link_speed(struct emac_hw *hw, u32 speed,
				     bool autoneg, bool fc);
extern int emac_check_phy_link(struct emac_hw *hw, u32 *speed, bool *link_up);
extern int emac_hw_get_lpa_speed(struct emac_hw *hw, u32 *speed);
extern int emac_hw_ack_phy_intr(struct emac_hw *hw);
extern int emac_hw_init_phy(struct emac_hw *hw);
extern int emac_hw_reset_phy(struct emac_hw *hw);

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

extern void emac_hw_get_mac_addr(struct emac_hw *hw, u8 *addr);
extern void emac_hw_set_mac_addr(struct emac_hw *hw, u8 *addr);

#define IMR_NORMAL_MASK         (\
		SW_MAN_INT      |\
		ISR_OVER        |\
		ISR_ERROR       |\
		ISR_GPHY_LINK   |\
		ISR_TX_PKT      |\
		RX_PKT_INT0     |\
		GPHY_WAKEUP_INT)

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

/* PHY */

/* MII_BMCR (0x00) */
#define BMCR_SPEED10                    0x0000

#endif /*_EMAC_HW_H_*/
