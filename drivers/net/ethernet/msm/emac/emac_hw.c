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

/* MSM EMAC Ethernet Controller Hardware support
 */

#include <linux/crc32.h>
#include <linux/if_vlan.h>

#include "emac_hw.h"
#include "emac_ptp.h"

#define RFD_PREF_LOW_TH     0x10
#define RFD_PREF_UP_TH      0x10
#define JUMBO_1KAH          0x4

#define RXF_DOF_TH          0x0be
#define RXF_UOF_TH          0x1a0

#define RXD_TH              0x100

/* REG */
u32 emac_reg_r32(struct emac_hw *hw, u8 base, u32 reg)
{
	return readl_relaxed(hw->reg_addr[base] + reg);
}

void emac_reg_w32(struct emac_hw *hw, u8 base, u32 reg, u32 val)
{
	writel_relaxed(val, hw->reg_addr[base] + reg);
}

void emac_reg_update32(struct emac_hw *hw, u8 base, u32 reg, u32 mask, u32 val)
{
	u32 data;

	data = emac_reg_r32(hw, base, reg);
	emac_reg_w32(hw, base, reg, ((data & ~mask) | val));
}

u32 emac_reg_field_r32(struct emac_hw *hw, u8 base, u32 reg,
		       u32 mask, u32 shift)
{
	u32 data;

	data = emac_reg_r32(hw, base, reg);
	return (data & mask) >> shift;
}

/* PHY */
static int emac_disable_mdio_autopoll(struct emac_hw *hw)
{
	u32 i, val;

	emac_reg_update32(hw, EMAC, EMAC_MDIO_CTRL, MDIO_AP_EN, 0);
	wmb(); /* ensure mdio autopoll disable is requested */

	/* wait for any mdio polling to complete */
	for (i = 0; i < MDIO_WAIT_TIMES; i++) {
		val = emac_reg_r32(hw, EMAC, EMAC_MDIO_CTRL);
		if (!(val & MDIO_BUSY))
			return 0;

		udelay(100);
	}

	/* failed to disable; ensure it is enabled before returning */
	emac_reg_update32(hw, EMAC, EMAC_MDIO_CTRL, 0, MDIO_AP_EN);
	wmb(); /* ensure mdio autopoll is enabled */
	return -EBUSY;
}

static void emac_enable_mdio_autopoll(struct emac_hw *hw)
{
	emac_reg_update32(hw, EMAC, EMAC_MDIO_CTRL, 0, MDIO_AP_EN);
	wmb(); /* ensure mdio autopoll is enabled */
}

int emac_hw_read_phy_reg(struct emac_hw *hw, bool ext, u8 dev, bool fast,
			 u16 reg_addr, u16 *phy_data)
{
	u32 i, val, clk_sel;
	int retval = 0;

	*phy_data = 0;
	clk_sel = fast ? MDIO_CLK_25_4 : MDIO_CLK_25_28;

	retval = emac_disable_mdio_autopoll(hw);
	if (retval)
		return retval;

	emac_reg_update32(hw, EMAC, EMAC_PHY_STS, PHY_ADDR_BMSK,
			  (dev << PHY_ADDR_SHFT));
	wmb(); /* ensure PHY address is set before we proceed */

	if (ext) {
		val = ((dev << DEVAD_SHFT) & DEVAD_BMSK) |
		      ((reg_addr << EX_REG_ADDR_SHFT) & EX_REG_ADDR_BMSK);
		emac_reg_w32(hw, EMAC, EMAC_MDIO_EX_CTRL, val);
		wmb(); /* ensure proper address is set before proceeding */

		val = SUP_PREAMBLE |
		      ((clk_sel << MDIO_CLK_SEL_SHFT) & MDIO_CLK_SEL_BMSK) |
		      MDIO_START | MDIO_MODE | MDIO_RD_NWR;
	} else {
		val = val & ~(MDIO_REG_ADDR_BMSK | MDIO_CLK_SEL_BMSK |
				MDIO_MODE | MDIO_PR);
		val = SUP_PREAMBLE |
		      ((clk_sel << MDIO_CLK_SEL_SHFT) & MDIO_CLK_SEL_BMSK) |
		      ((reg_addr << MDIO_REG_ADDR_SHFT) & MDIO_REG_ADDR_BMSK) |
		      MDIO_START | MDIO_RD_NWR;
	}

	emac_reg_w32(hw, EMAC, EMAC_MDIO_CTRL, val);
	mb(); /* ensure hw starts the operation before we check for result */

	for (i = 0; i < MDIO_WAIT_TIMES; i++) {
		val = emac_reg_r32(hw, EMAC, EMAC_MDIO_CTRL);
		if (!(val & (MDIO_START | MDIO_BUSY))) {
			*phy_data = (u16)((val >> MDIO_DATA_SHFT) &
					MDIO_DATA_BMSK);
			break;
		}
		udelay(100);
	}

	if (i == MDIO_WAIT_TIMES)
		retval = -EIO;

	emac_enable_mdio_autopoll(hw);
	return retval;
}

int emac_hw_write_phy_reg(struct emac_hw *hw, bool ext, u8 dev,
			  bool fast, u16 reg_addr, u16 phy_data)
{
	u32 i, val, clk_sel;
	int retval = 0;

	clk_sel = fast ? MDIO_CLK_25_4 : MDIO_CLK_25_28;

	retval = emac_disable_mdio_autopoll(hw);
	if (retval)
		return retval;

	emac_reg_update32(hw, EMAC, EMAC_PHY_STS, PHY_ADDR_BMSK,
			  (dev << PHY_ADDR_SHFT));
	wmb(); /* ensure PHY address is set before we proceed */

	if (ext) {
		val = ((dev << DEVAD_SHFT) & DEVAD_BMSK) |
		      ((reg_addr << EX_REG_ADDR_SHFT) & EX_REG_ADDR_BMSK);
		emac_reg_w32(hw, EMAC, EMAC_MDIO_EX_CTRL, val);
		wmb(); /* ensure proper address is set before proceeding */

		val = SUP_PREAMBLE |
			((clk_sel << MDIO_CLK_SEL_SHFT) & MDIO_CLK_SEL_BMSK) |
			((phy_data << MDIO_DATA_SHFT) & MDIO_DATA_BMSK) |
			MDIO_START | MDIO_MODE;
	} else {
		val = val & ~(MDIO_REG_ADDR_BMSK | MDIO_CLK_SEL_BMSK |
			MDIO_DATA_BMSK | MDIO_MODE | MDIO_PR);
		val = SUP_PREAMBLE |
		((clk_sel << MDIO_CLK_SEL_SHFT) & MDIO_CLK_SEL_BMSK) |
		((reg_addr << MDIO_REG_ADDR_SHFT) & MDIO_REG_ADDR_BMSK) |
		((phy_data << MDIO_DATA_SHFT) & MDIO_DATA_BMSK) |
		MDIO_START;
	}

	emac_reg_w32(hw, EMAC, EMAC_MDIO_CTRL, val);
	mb(); /* ensure hw starts the operation before we check for result */

	for (i = 0; i < MDIO_WAIT_TIMES; i++) {
		val = emac_reg_r32(hw, EMAC, EMAC_MDIO_CTRL);
		if (!(val & (MDIO_START | MDIO_BUSY)))
			break;
		udelay(100);
	}

	if (i == MDIO_WAIT_TIMES)
		retval = -EIO;

	emac_enable_mdio_autopoll(hw);
	return retval;
}

int emac_read_phy_reg(struct emac_hw *hw, u16 reg_addr, u16 *phy_data)
{
	unsigned long  flags;
	int  retval;

	spin_lock_irqsave(&hw->mdio_lock, flags);
	retval = emac_hw_read_phy_reg(hw, false, hw->phy_addr, true,
				      reg_addr, phy_data);
	spin_unlock_irqrestore(&hw->mdio_lock, flags);

	if (retval)
		emac_err(hw->adpt, "error reading phy reg 0x%02x\n", reg_addr);
	else
		emac_dbg(hw->adpt, hw, "EMAC PHY RD: 0x%02x -> 0x%04x\n",
			 reg_addr, *phy_data);

	return retval;
}

int emac_write_phy_reg(struct emac_hw *hw, u16 reg_addr, u16 phy_data)
{
	unsigned long  flags;
	int  retval;

	spin_lock_irqsave(&hw->mdio_lock, flags);
	retval = emac_hw_write_phy_reg(hw, false, hw->phy_addr, true,
				       reg_addr, phy_data);
	spin_unlock_irqrestore(&hw->mdio_lock, flags);

	if (retval)
		emac_err(hw->adpt, "error writing phy reg 0x%02x\n", reg_addr);
	else
		emac_dbg(hw->adpt, hw, "EMAC PHY WR: 0x%02x <- 0x%04x\n",
			 reg_addr, phy_data);

	return retval;
}

int emac_hw_ack_phy_intr(struct emac_hw *hw)
{
	/* ack phy interrupt */
	return 0;
}

int emac_hw_init_phy(struct emac_hw *hw)
{
	u16 phy_id[2];
	int retval;

	spin_lock_init(&hw->mdio_lock);

	retval = emac_read_phy_reg(hw, MII_PHYSID1, &phy_id[0]);
	if (retval)
		return retval;
	retval = emac_read_phy_reg(hw, MII_PHYSID2, &phy_id[1]);
	if (retval)
		return retval;

	hw->phy_id[0] = phy_id[0];
	hw->phy_id[1] = phy_id[1];
	hw->autoneg_advertised = EMAC_LINK_SPEED_DEFAULT;

	return retval;
}

int emac_hw_reset_phy(struct emac_hw *hw)
{
	/* reset PHY */
	return 0;
}

/* LINK */
static int emac_hw_setup_phy_link(struct emac_hw *hw, u32 speed, bool autoneg,
				  bool fc)
{
	u16 adv, bmcr, ctrl1000 = 0;
	int retval = 0;

	if (autoneg) {
		adv = ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM;
		if (!fc)
			adv &= ~(ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM);

		if (speed & EMAC_LINK_SPEED_10_HALF)
			adv |= ADVERTISE_10HALF;

		if (speed & EMAC_LINK_SPEED_10_FULL)
			adv |= ADVERTISE_10HALF | ADVERTISE_10FULL;

		if (speed & EMAC_LINK_SPEED_100_HALF)
			adv |= ADVERTISE_100HALF;

		if (speed & EMAC_LINK_SPEED_100_FULL)
			adv |= ADVERTISE_100HALF | ADVERTISE_100FULL;

		if (speed & EMAC_LINK_SPEED_1GB_FULL)
			ctrl1000 |= ADVERTISE_1000FULL;

		retval |= emac_write_phy_reg(hw, MII_ADVERTISE, adv);
		retval |= emac_write_phy_reg(hw, MII_CTRL1000, ctrl1000);

		bmcr = BMCR_RESET | BMCR_ANENABLE | BMCR_ANRESTART;
		retval |= emac_write_phy_reg(hw, MII_BMCR, bmcr);
	} else {
		bmcr = BMCR_RESET;
		switch (speed) {
		case EMAC_LINK_SPEED_10_HALF:
			bmcr |= BMCR_SPEED10;
			break;
		case EMAC_LINK_SPEED_10_FULL:
			bmcr |= BMCR_SPEED10 | BMCR_FULLDPLX;
			break;
		case EMAC_LINK_SPEED_100_HALF:
			bmcr |= BMCR_SPEED100;
			break;
		case EMAC_LINK_SPEED_100_FULL:
			bmcr |= BMCR_SPEED100 | BMCR_FULLDPLX;
			break;
		default:
			return -EINVAL;
		}

		retval |= emac_write_phy_reg(hw, MII_BMCR, bmcr);
	}

	return retval;
}

int emac_setup_phy_link(struct emac_hw *hw, u32 speed, bool autoneg, bool fc)
{
	int retval = 0;

	if (emac_hw_setup_phy_link(hw, speed, autoneg, fc)) {
		emac_err(hw->adpt, "error when init phy speed and fc\n");
		retval = -EINVAL;
	}

	return retval;
}

int emac_setup_phy_link_speed(struct emac_hw *hw, u32 speed,
			      bool autoneg, bool fc)
{
	/* update autoneg_advertised based on input link speed */
	hw->autoneg_advertised = speed & EMAC_LINK_SPEED_DEFAULT;
	return emac_setup_phy_link(hw, hw->autoneg_advertised, autoneg, fc);
}

int emac_check_phy_link(struct emac_hw *hw, u32 *speed, bool *link_up)
{
	u16 bmsr;
	int retval;

	retval = emac_read_phy_reg(hw, MII_BMSR, &bmsr);
	if (retval)
		return retval;

	if (!(bmsr & BMSR_LSTATUS)) {
		*link_up = false;
		*speed = EMAC_LINK_SPEED_UNKNOWN;
		return 0;
	}
	*link_up = true;
	/* Read speed from phy specific registers */
	*speed = EMAC_LINK_SPEED_100_FULL;

	return retval;
}

/* INTR */
void emac_hw_enable_intr(struct emac_hw *hw)
{
	struct emac_adapter *adpt = hw->adpt;
	struct emac_irq_info *irq_info;
	int i;

	for (i = 0; i < EMAC_NUM_CORE_IRQ; i++) {
		irq_info = &adpt->irq_info[i];
		emac_reg_w32(hw, EMAC, irq_info->status_reg, ~DIS_INT);
		emac_reg_w32(hw, EMAC, irq_info->mask_reg, irq_info->mask);
	}
	wmb();
}

void emac_hw_disable_intr(struct emac_hw *hw)
{
	struct emac_adapter *adpt = hw->adpt;
	struct emac_irq_info *irq_info;
	int i;

	for (i = 0; i < EMAC_NUM_CORE_IRQ; i++) {
		irq_info = &adpt->irq_info[i];
		emac_reg_w32(hw, EMAC, irq_info->status_reg, DIS_INT);
		emac_reg_w32(hw, EMAC, irq_info->mask_reg, 0);
	}

	emac_reg_w32(hw, EMAC_1588, EMAC_P1588_PTP_EXPANDED_INT_MASK, 0);
	wmb();
}

/* MC */
void emac_hw_set_mc_addr(struct emac_hw *hw, u8 *addr)
{
	u32 crc32, bit, reg, mta;

	/* Calculate the CRC of the MAC address */
	crc32 = ether_crc(ETH_ALEN, addr);

	/* The HASH Table is an array of 2 32-bit registers. It is
	 * treated like an array of 64 bits (BitArray[hash_value]).
	 * Use the upper 6 bits of the above CRC as the hash value.
	 */
	reg = (crc32 >> 31) & 0x1;
	bit = (crc32 >> 26) & 0x1F;

	mta = emac_reg_r32(hw, EMAC, EMAC_HASH_TAB_REG0 + (reg << 2));
	mta |= (0x1 << bit);
	emac_reg_w32(hw, EMAC, EMAC_HASH_TAB_REG0 + (reg << 2), mta);
	wmb();
}

void emac_hw_clear_mc_addr(struct emac_hw *hw)
{
	emac_reg_w32(hw, EMAC, EMAC_HASH_TAB_REG0, 0);
	emac_reg_w32(hw, EMAC, EMAC_HASH_TAB_REG1, 0);
	wmb();
}

/* definitions for RSS */
#define EMAC_RSS_KEY(_i, _type) \
		(EMAC_RSS_KEY0 + ((_i) * sizeof(_type)))
#define EMAC_RSS_TBL(_i, _type) \
		(EMAC_IDT_TABLE0 + ((_i) * sizeof(_type)))

/* RSS */
void emac_hw_config_rss(struct emac_hw *hw)
{
	int key_len_by_u32 = sizeof(hw->rss_key) / sizeof(u32);
	int idt_len_by_u32 = sizeof(hw->rss_idt) / sizeof(u32);
	u32 rxq0;
	int i;

	/* Fill out hash function keys */
	for (i = 0; i < key_len_by_u32; i++) {
		u32 key, idx_base;
		idx_base = (key_len_by_u32 - i) * 4;
		key = ((hw->rss_key[idx_base - 1])       |
		       (hw->rss_key[idx_base - 2] << 8)  |
		       (hw->rss_key[idx_base - 3] << 16) |
		       (hw->rss_key[idx_base - 4] << 24));
		emac_reg_w32(hw, EMAC, EMAC_RSS_KEY(i, u32), key);
	}

	/* Fill out redirection table */
	for (i = 0; i < idt_len_by_u32; i++)
		emac_reg_w32(hw, EMAC, EMAC_RSS_TBL(i, u32), hw->rss_idt[i]);

	emac_reg_w32(hw, EMAC, EMAC_BASE_CPU_NUMBER, hw->rss_base_cpu);

	rxq0 = emac_reg_r32(hw, EMAC, EMAC_RXQ_CTRL_0);
	if (hw->rss_hstype & EMAC_RSS_HSTYP_IPV4_EN)
		rxq0 |= RXQ0_RSS_HSTYP_IPV4_EN;
	else
		rxq0 &= ~RXQ0_RSS_HSTYP_IPV4_EN;

	if (hw->rss_hstype & EMAC_RSS_HSTYP_TCP4_EN)
		rxq0 |= RXQ0_RSS_HSTYP_IPV4_TCP_EN;
	else
		rxq0 &= ~RXQ0_RSS_HSTYP_IPV4_TCP_EN;

	if (hw->rss_hstype & EMAC_RSS_HSTYP_IPV6_EN)
		rxq0 |= RXQ0_RSS_HSTYP_IPV6_EN;
	else
		rxq0 &= ~RXQ0_RSS_HSTYP_IPV6_EN;

	if (hw->rss_hstype & EMAC_RSS_HSTYP_TCP6_EN)
		rxq0 |= RXQ0_RSS_HSTYP_IPV6_TCP_EN;
	else
		rxq0 &= ~RXQ0_RSS_HSTYP_IPV6_TCP_EN;

	rxq0 |= ((hw->rss_idt_size << IDT_TABLE_SIZE_SHFT) &
		IDT_TABLE_SIZE_BMSK);
	rxq0 |= RSS_HASH_EN;

	wmb(); /* ensure all parameters are written before we enable RSS */
	emac_reg_w32(hw, EMAC, EMAC_RXQ_CTRL_0, rxq0);
	wmb();
}

/* Config MAC modes */
void emac_hw_config_mac_ctrl(struct emac_hw *hw)
{
	u32 mac;

	mac = emac_reg_r32(hw, EMAC, EMAC_MAC_CTRL);

	if (CHK_HW_FLAG(VLANSTRIP_EN))
		mac |= VLAN_STRIP;
	else
		mac &= ~VLAN_STRIP;

	if (CHK_HW_FLAG(PROMISC_EN))
		mac |= PROM_MODE;
	else
		mac &= ~PROM_MODE;

	if (CHK_HW_FLAG(MULTIALL_EN))
		mac |= MULTI_ALL;
	else
		mac &= ~MULTI_ALL;

	if (CHK_HW_FLAG(LOOPBACK_EN))
		mac |= MAC_LP_EN;
	else
		mac &= ~MAC_LP_EN;

	emac_reg_w32(hw, EMAC, EMAC_MAC_CTRL, mac);
	wmb();
}

/* Wake On LAN (WOL) */
int emac_hw_config_wol(struct emac_hw *hw, u32 wufc)
{
	u32 wol = 0;

	wol |= WK_FRAME_EN;
	/* turn on magic packet event */
	if (wufc & EMAC_WOL_MAGIC)
		wol |= MG_FRAME_EN | MG_FRAME_PME;

	/* turn on link up event */
	if (wufc & EMAC_WOL_PHY)
		wol |=  LK_CHG_EN | LK_CHG_PME;

	emac_reg_w32(hw, EMAC, EMAC_WOL_CTRL0, wol);
	emac_reg_update32(hw, EMAC_CSR, EMAC_EMAC_WRAPPER_CSR2, 0, WOL_EN);
	wmb();

	return 0;
}

/* Power Management */
int emac_hw_config_pow_save(struct emac_hw *hw, u32 speed, bool wol_en,
			    bool tx_en, bool rx_en)
{
	u32 dma_mas, mac;

	dma_mas = emac_reg_r32(hw, EMAC, EMAC_DMA_MAS_CTRL);
	dma_mas &= ~MASTER_CTRL_CLK_SEL_DIS;

	mac = emac_reg_r32(hw, EMAC, EMAC_MAC_CTRL);
	mac = (mac & ~SPEED_BMSK) |
		(((u32)emac_mac_speed_1000 << SPEED_SHFT) & SPEED_BMSK);

	mac &= ~(FULLD | RXEN | TXEN);

	if (wol_en) {
		if (rx_en)
			mac |= (RXEN | BROAD_EN);

		if (tx_en)
			mac |= TXEN;

		if (EMAC_LINK_SPEED_1GB_FULL == speed) {
			mac = (mac & ~SPEED_BMSK) |
				(((u32)emac_mac_speed_1000 << SPEED_SHFT) &
				SPEED_BMSK);
		}
		if ((EMAC_LINK_SPEED_10_FULL == speed) ||
		    (EMAC_LINK_SPEED_100_FULL == speed) ||
		    (EMAC_LINK_SPEED_1GB_FULL == speed))
			mac |= FULLD;

	} else {
		dma_mas |= MASTER_CTRL_CLK_SEL_DIS;
	}
	emac_reg_w32(hw, EMAC, EMAC_DMA_MAS_CTRL, dma_mas);
	emac_reg_w32(hw, EMAC, EMAC_MAC_CTRL, mac);
	wmb();

	return 0;
}

/* Config descriptor rings */
static void emac_hw_config_ring_ctrl(struct emac_hw *hw)
{
	struct emac_adapter *adpt = hw->adpt;

	if (adpt->tstamp_en) {
		emac_reg_update32(hw, EMAC_CSR, EMAC_EMAC_WRAPPER_CSR1,
				  0, ENABLE_RRD_TIMESTAMP);
	}

	/* TPD */
	emac_reg_w32(hw, EMAC, EMAC_DESC_CTRL_1,
		     EMAC_DMA_ADDR_HI(adpt->tx_queue[0].tpd.tpdma));
	switch (adpt->num_txques) {
	case 4:
		emac_reg_w32(hw, EMAC, EMAC_H3TPD_BASE_ADDR_LO,
			     EMAC_DMA_ADDR_LO(adpt->tx_queue[3].tpd.tpdma));
	case 3:
		emac_reg_w32(hw, EMAC, EMAC_H2TPD_BASE_ADDR_LO,
			     EMAC_DMA_ADDR_LO(adpt->tx_queue[2].tpd.tpdma));
	case 2:
		emac_reg_w32(hw, EMAC, EMAC_H1TPD_BASE_ADDR_LO,
			     EMAC_DMA_ADDR_LO(adpt->tx_queue[1].tpd.tpdma));
	case 1:
		emac_reg_w32(hw, EMAC, EMAC_DESC_CTRL_8,
			     EMAC_DMA_ADDR_LO(adpt->tx_queue[0].tpd.tpdma));
		break;
	default:
		emac_err(hw->adpt, "Invalid number of TX queues (%d)\n",
			 adpt->num_txques);
		return;
	}
	emac_reg_w32(hw, EMAC, EMAC_DESC_CTRL_9,
		     adpt->tx_queue[0].tpd.count & TPD_RING_SIZE_BMSK);

	/* RFD & RRD */
	emac_reg_w32(hw, EMAC, EMAC_DESC_CTRL_0,
		     EMAC_DMA_ADDR_HI(adpt->rx_queue[0].rfd.rfdma));
	switch (adpt->num_rxques) {
	case 4:
		emac_reg_w32(hw, EMAC, EMAC_DESC_CTRL_13,
			     EMAC_DMA_ADDR_LO(adpt->rx_queue[3].rfd.rfdma));
		emac_reg_w32(hw, EMAC, EMAC_DESC_CTRL_16,
			     EMAC_DMA_ADDR_LO(adpt->rx_queue[3].rrd.rrdma));
	case 3:
		emac_reg_w32(hw, EMAC, EMAC_DESC_CTRL_12,
			     EMAC_DMA_ADDR_LO(adpt->rx_queue[2].rfd.rfdma));
		emac_reg_w32(hw, EMAC, EMAC_DESC_CTRL_15,
			     EMAC_DMA_ADDR_LO(adpt->rx_queue[2].rrd.rrdma));
	case 2:
		emac_reg_w32(hw, EMAC, EMAC_DESC_CTRL_10,
			     EMAC_DMA_ADDR_LO(adpt->rx_queue[1].rfd.rfdma));
		emac_reg_w32(hw, EMAC, EMAC_DESC_CTRL_14,
			     EMAC_DMA_ADDR_LO(adpt->rx_queue[1].rrd.rrdma));
	case 1:
		emac_reg_w32(hw, EMAC, EMAC_DESC_CTRL_2,
			     EMAC_DMA_ADDR_LO(adpt->rx_queue[0].rfd.rfdma));
		emac_reg_w32(hw, EMAC, EMAC_DESC_CTRL_5,
			     EMAC_DMA_ADDR_LO(adpt->rx_queue[0].rrd.rrdma));
		break;
	default:
		emac_err(hw->adpt, "Invalid number of RX queues (%d)\n",
			 adpt->num_rxques);
		return;
	}
	emac_reg_w32(hw, EMAC, EMAC_DESC_CTRL_3,
		     adpt->rx_queue[0].rfd.count & RFD_RING_SIZE_BMSK);
	emac_reg_w32(hw, EMAC, EMAC_DESC_CTRL_6,
		     adpt->rx_queue[0].rrd.count & RRD_RING_SIZE_BMSK);
	emac_reg_w32(hw, EMAC, EMAC_DESC_CTRL_4,
		     adpt->rxbuf_size & RX_BUFFER_SIZE_BMSK);

	emac_reg_w32(hw, EMAC, EMAC_DESC_CTRL_11, 0);

	wmb(); /* ensure all parameters are written before we enable them */
	/* Load all of base address above */
	emac_reg_w32(hw, EMAC, EMAC_INTER_SRAM_PART9, 1);
	wmb();
}

/* Config transmit parameters */
static void emac_hw_config_tx_ctrl(struct emac_hw *hw)
{
	u16 tx_offload_thresh = EMAC_MAX_TX_OFFLOAD_THRESH;
	u32 val;

	emac_reg_w32(hw, EMAC, EMAC_TXQ_CTRL_1,
		(tx_offload_thresh >> 3) & JUMBO_TASK_OFFLOAD_THRESHOLD_BMSK);

	val = (hw->tpd_burst << NUM_TPD_BURST_PREF_SHFT) &
		NUM_TPD_BURST_PREF_BMSK;

	val |= (TXQ_MODE | LS_8023_SP);
	val |= (0x0100 << NUM_TXF_BURST_PREF_SHFT) &
		NUM_TXF_BURST_PREF_BMSK;

	emac_reg_w32(hw, EMAC, EMAC_TXQ_CTRL_0, val);
	emac_reg_update32(hw, EMAC, EMAC_TXQ_CTRL_2,
			  (TXF_HWM_BMSK | TXF_LWM_BMSK), 0);
	wmb();
}

/* Config receive parameters */
static void emac_hw_config_rx_ctrl(struct emac_hw *hw)
{
	u32 val;

	val = ((hw->rfd_burst << NUM_RFD_BURST_PREF_SHFT) &
	       NUM_RFD_BURST_PREF_BMSK);
	val |= (SP_IPV6 | CUT_THRU_EN);

	emac_reg_w32(hw, EMAC, EMAC_RXQ_CTRL_0, val);

	val = emac_reg_r32(hw, EMAC, EMAC_RXQ_CTRL_1);
	val &= ~(JUMBO_1KAH_BMSK | RFD_PREF_LOW_THRESHOLD_BMSK |
		 RFD_PREF_UP_THRESHOLD_BMSK);
	val |= (JUMBO_1KAH << JUMBO_1KAH_SHFT) |
		(RFD_PREF_LOW_TH << RFD_PREF_LOW_THRESHOLD_SHFT) |
		(RFD_PREF_UP_TH << RFD_PREF_UP_THRESHOLD_SHFT);
	emac_reg_w32(hw, EMAC, EMAC_RXQ_CTRL_1, val);

	val = emac_reg_r32(hw, EMAC, EMAC_RXQ_CTRL_2);
	val &= ~(RXF_DOF_THRESHOLD_BMSK | RXF_UOF_THRESHOLD_BMSK);
	val |= (RXF_DOF_TH << RXF_DOF_THRESHOLD_SHFT) |
		(RXF_UOF_TH << RXF_UOF_THRESHOLD_SHFT);
	emac_reg_w32(hw, EMAC, EMAC_RXQ_CTRL_2, val);

	val = emac_reg_r32(hw, EMAC, EMAC_RXQ_CTRL_3);
	val &= ~(RXD_TIMER_BMSK | RXD_THRESHOLD_BMSK);
	val |= RXD_TH << RXD_THRESHOLD_SHFT;
	emac_reg_w32(hw, EMAC, EMAC_RXQ_CTRL_3, val);
	wmb();
}

/* Config dma */
static void emac_hw_config_dma_ctrl(struct emac_hw *hw)
{
	u32 dma_ctrl;

	dma_ctrl = DMAR_REQ_PRI;

	switch (hw->dma_order) {
	case emac_dma_ord_in:
		dma_ctrl |= IN_ORDER_MODE;
		break;
	case emac_dma_ord_enh:
		dma_ctrl |= ENH_ORDER_MODE;
		break;
	case emac_dma_ord_out:
		dma_ctrl |= OUT_ORDER_MODE;
		break;
	default:
		break;
	}

	dma_ctrl |= (((u32)hw->dmar_block) << REGRDBLEN_SHFT) &
						REGRDBLEN_BMSK;
	dma_ctrl |= (((u32)hw->dmaw_block) << REGWRBLEN_SHFT) &
						REGWRBLEN_BMSK;
	dma_ctrl |= (((u32)hw->dmar_dly_cnt) << DMAR_DLY_CNT_SHFT) &
						DMAR_DLY_CNT_BMSK;
	dma_ctrl |= (((u32) hw->dmaw_dly_cnt) << DMAW_DLY_CNT_SHFT) &
						DMAW_DLY_CNT_BMSK;

	emac_reg_w32(hw, EMAC, EMAC_DMA_CTRL, dma_ctrl);
	wmb();
}

/* Flow Control (fc)  */
static int emac_get_fc_mode(struct emac_hw *hw, enum emac_fc_mode *mode)
{
	u16 i, bmsr = 0;
	int retval = 0;

	for (i = 0; i < EMAC_MAX_SETUP_LNK_CYCLE; i++) {
		retval = emac_read_phy_reg(hw, MII_BMSR, &bmsr);
		if (retval)
			return retval;

		if (bmsr & BMSR_LSTATUS) {
			/* Read fc mode from phy specific registers */
			*mode = emac_fc_full;
			break;
		}
		msleep(100); /* link can take upto few seconds to come up */
	}

	if (i == EMAC_MAX_SETUP_LNK_CYCLE) {
		emac_err(hw->adpt, "error when get flow control mode\n");
		retval = -EINVAL;
	}

	return retval;
}

int emac_hw_config_fc(struct emac_hw *hw)
{
	u32 mac;
	int retval;

	if (hw->disable_fc_autoneg) {
		hw->cur_fc_mode = hw->req_fc_mode;
	} else {
		retval = emac_get_fc_mode(hw, &hw->cur_fc_mode);
		if (retval)
			return retval;
	}

	mac = emac_reg_r32(hw, EMAC, EMAC_MAC_CTRL);

	switch (hw->cur_fc_mode) {
	case emac_fc_none:
		mac &= ~(RXFC | TXFC);
		break;
	case emac_fc_rx_pause:
		mac &= ~TXFC;
		mac |= RXFC;
		break;
	case emac_fc_tx_pause:
		mac |= TXFC;
		mac &= ~RXFC;
		break;
	case emac_fc_full:
	case emac_fc_default:
		mac |= (TXFC | RXFC);
		break;
	default:
		emac_err(hw->adpt, "flow control param set incorrectly\n");
		return -EINVAL;
	}

	emac_reg_w32(hw, EMAC, EMAC_MAC_CTRL, mac);
	wmb();
	return 0;
}

/* Configure MAC */
void emac_hw_config_mac(struct emac_hw *hw)
{
	u32 val;

	emac_hw_set_mac_addr(hw, hw->mac_addr);

	emac_hw_config_ring_ctrl(hw);

	emac_reg_w32(hw, EMAC, EMAC_MAX_FRAM_LEN_CTRL,
		     hw->mtu + ETH_HLEN + VLAN_HLEN + ETH_FCS_LEN);

	emac_hw_config_tx_ctrl(hw);
	emac_hw_config_rx_ctrl(hw);
	emac_hw_config_dma_ctrl(hw);

	if (CHK_HW_FLAG(PTP_CAP))
		emac_ptp_config(hw);

	val = emac_reg_r32(hw, EMAC, EMAC_AXI_MAST_CTRL);
	val &= ~(DATA_BYTE_SWAP | MAX_BOUND);
	val |= MAX_BTYPE;
	emac_reg_w32(hw, EMAC, EMAC_AXI_MAST_CTRL, val);

	emac_reg_w32(hw, EMAC, EMAC_CLK_GATE_CTRL, 0);
	emac_reg_w32(hw, EMAC, EMAC_MISC_CTRL, RX_UNCPL_INT_EN);
	wmb();
}

/* Reset MAC */
void emac_hw_reset_mac(struct emac_hw *hw)
{
	emac_reg_w32(hw, EMAC, EMAC_INT_MASK, 0);
	emac_reg_w32(hw, EMAC, EMAC_INT_STATUS, DIS_INT);

	emac_hw_stop_mac(hw);

	emac_reg_update32(hw, EMAC, EMAC_DMA_MAS_CTRL, 0, SOFT_RST);
	wmb(); /* ensure mac is fully reset */
	udelay(100);

	emac_reg_update32(hw, EMAC, EMAC_DMA_MAS_CTRL, 0, INT_RD_CLR_EN);
	wmb();
}

/* Start MAC */
void emac_hw_start_mac(struct emac_hw *hw)
{
	u32 mac, csr1;

	/* enable tx queue */
	if (hw->adpt->num_txques &&
		(hw->adpt->num_txques <= EMAC_MAX_TX_QUEUES)) {
		emac_reg_update32(hw, EMAC, EMAC_TXQ_CTRL_0, 0, TXQ_EN);
	}

	/* enable rx queue */
	if (hw->adpt->num_rxques &&
		(hw->adpt->num_rxques <= EMAC_MAX_RX_QUEUES)) {
		emac_reg_update32(hw, EMAC, EMAC_RXQ_CTRL_0, 0, RXQ_EN);
	}

	/* enable mac control */
	mac = emac_reg_r32(hw, EMAC, EMAC_MAC_CTRL);
	csr1 = emac_reg_r32(hw, EMAC_CSR, EMAC_EMAC_WRAPPER_CSR1);

	mac |= TXEN | RXEN;     /* enable RX/TX */
	mac |= (TXFC | RXFC);   /* enable RX/TX Flow Control */

	/* setup link speed */
	mac &= ~SPEED_BMSK;
	switch (hw->link_speed) {
	case EMAC_LINK_SPEED_1GB_FULL:
		mac |= ((emac_mac_speed_1000 << SPEED_SHFT) & SPEED_BMSK);
		csr1 |= FREQ_MODE;
		break;
	default:
		mac |= ((emac_mac_speed_10_100 << SPEED_SHFT) & SPEED_BMSK);
		csr1 &= ~FREQ_MODE;
		break;
	}

	switch (hw->link_speed) {
	case EMAC_LINK_SPEED_1GB_FULL:
	case EMAC_LINK_SPEED_100_FULL:
	case EMAC_LINK_SPEED_10_FULL:
		mac |= FULLD;
		break;
	default:
		mac &= ~FULLD;
	}

	/* other parameters */
	mac |= (CRCE | PCRCE);
	mac |= ((hw->preamble << PRLEN_SHFT) & PRLEN_BMSK);
	mac |= BROAD_EN;
	mac |= (FLCHK | PROM_MODE | RX_CHKSUM_EN);
	mac &= ~(HUGEN | VLAN_STRIP | TPAUSE | SIMR | HUGE | MULTI_ALL |
		 DEBUG_MODE | SINGLE_PAUSE_MODE);

	emac_reg_w32(hw, EMAC_CSR, EMAC_EMAC_WRAPPER_CSR1, csr1);
	emac_reg_w32(hw, EMAC, EMAC_MAC_CTRL, mac);

	/* clear all interrupts, set interrupt read clear */
	emac_reg_w32(hw, EMAC, EMAC_DMA_MAS_CTRL, INT_RD_CLR_EN);

	if (CHK_HW_FLAG(PTP_EN)) {
		if (hw->link_speed == EMAC_LINK_SPEED_1GB_FULL)
			emac_ptp_set_linkspeed(hw, emac_mac_speed_1000);
		else
			emac_ptp_set_linkspeed(hw, emac_mac_speed_10_100);
	}

	emac_hw_config_mac_ctrl(hw);

	emac_reg_update32(hw, EMAC, EMAC_ATHR_HEADER_CTRL,
			  (HEADER_ENABLE | HEADER_CNT_EN), 0);
	wmb();
}

/* Stop MAC */
void emac_hw_stop_mac(struct emac_hw *hw)
{
	emac_reg_update32(hw, EMAC, EMAC_RXQ_CTRL_0, RXQ_EN, 0);
	emac_reg_update32(hw, EMAC, EMAC_TXQ_CTRL_0, TXQ_EN, 0);
	emac_reg_update32(hw, EMAC, EMAC_MAC_CTRL, (TXEN | RXEN), 0);
	wmb(); /* make sure mac is stopped before we proceede */
	udelay(1000);
}

/* mac address should come from device tree */
void emac_hw_get_mac_addr(struct emac_hw *hw, u8 *addr)
{
	addr[0] = 0x00;
	addr[1] = 0xa0;
	addr[2] = 0xc6;
	addr[3] = 0x11;
	addr[4] = 0x22;
	addr[5] = 0x33;
}

/* set MAC address */
void emac_hw_set_mac_addr(struct emac_hw *hw, u8 *addr)
{
	u32 sta;

	/* for example: 00-A0-C6-11-22-33
	 * 0<-->C6112233, 1<-->00A0.
	 */

	/* low dword */
	sta = (((u32)addr[2]) << 24) | (((u32)addr[3]) << 16) |
	      (((u32)addr[4]) << 8)  | (((u32)addr[5]));
	emac_reg_w32(hw, EMAC, EMAC_MAC_STA_ADDR0, sta);

	/* hight dword */
	sta = (((u32)addr[0]) << 8) | (((u32)addr[1]));
	emac_reg_w32(hw, EMAC, EMAC_MAC_STA_ADDR1, sta);
	wmb();
}
