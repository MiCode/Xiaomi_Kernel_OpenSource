/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Zhiyong Tao <zhiyong.tao@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "star.h"

#ifdef CONFIG_ARM64
uintptr_t tx_skb_reserve[TX_DESC_NUM];
uintptr_t rx_skb_reserve[RX_DESC_NUM];
#endif

u16 star_mdc_mdio_read(star_dev *dev, u32 phy_addr, u32 phy_reg)
{
	u16 data;
	u32 phy_ctl;
	void __iomem *base = dev->base;

	/* Clear previous read/write OK status (write 1 clear) */
	star_set_reg(STAR_PHY_CTRL0(base), STAR_PHY_CTRL0_RWOK);
	phy_ctl = (phy_addr & STAR_PHY_CTRL0_PA_MASK)
		  << STAR_PHY_CTRL0_PA_OFFSET |
		  (phy_reg & STAR_PHY_CTRL0_PREG_MASK)
		  << STAR_PHY_CTRL0_PREG_OFFSET |
		  STAR_PHY_CTRL0_RDCMD;
	star_mb();
	star_set_reg(STAR_PHY_CTRL0(base), phy_ctl);
	star_mb();

	STAR_POLLING_TIMEOUT(star_is_set_bit(STAR_PHY_CTRL0(base),
					     STAR_PHY_CTRL0_RWOK));
	star_mb();
	data = (u16)star_get_bit_mask(STAR_PHY_CTRL0(base),
				      STAR_PHY_CTRL0_RWDATA_MASK,
				      STAR_PHY_CTRL0_RWDATA_OFFSET);

	return data;
	}

void star_mdc_mdio_write(star_dev *dev, u32 phy_addr, u32 phy_reg, u16 value)
{
	u32 phy_ctl;
	void __iomem *base = dev->base;

	/* Clear previous read/write OK status (write 1 clear) */
	star_set_reg(STAR_PHY_CTRL0(base), STAR_PHY_CTRL0_RWOK);
	phy_ctl = ((value & STAR_PHY_CTRL0_RWDATA_MASK)
		   << STAR_PHY_CTRL0_RWDATA_OFFSET) |
		   ((phy_addr & STAR_PHY_CTRL0_PA_MASK)
		   << STAR_PHY_CTRL0_PA_OFFSET) |
		   ((phy_reg & STAR_PHY_CTRL0_PREG_MASK)
		   << STAR_PHY_CTRL0_PREG_OFFSET) |
		   STAR_PHY_CTRL0_WTCMD;
	star_mb();
	star_set_reg(STAR_PHY_CTRL0(base), phy_ctl);
	star_mb();
	STAR_POLLING_TIMEOUT(star_is_set_bit(STAR_PHY_CTRL0(base),
					     STAR_PHY_CTRL0_RWOK));
}

static void desc_tx_init(tx_desc *tx_desc, u32 is_eor)
{
	tx_desc->buffer = 0;
	tx_desc->ctrl_len = TX_COWN | (is_eor ? TX_EOR : 0);
	tx_desc->vtag = 0;
	tx_desc->reserve = 0;
}

static void desc_rx_init(rx_desc *rx_desc, u32 is_eor)
{
	rx_desc->buffer = 0;
	rx_desc->ctrl_len = RX_COWN | (is_eor ? RX_EOR : 0);
	rx_desc->vtag = 0;
	rx_desc->reserve = 0;
}

u32 desc_tx_empty(tx_desc *tx_desc)
{
	return (((tx_desc)->buffer == 0) &&
		(((tx_desc)->ctrl_len & ~TX_EOR) == TX_COWN) &&
		((tx_desc)->vtag == 0) &&
		((tx_desc)->reserve == 0));
}

u32 desc_rx_empty(rx_desc *rx_desc)
{
	return (((rx_desc)->buffer == 0) &&
		(((rx_desc)->ctrl_len & ~RX_EOR) == RX_COWN) &&
		((rx_desc)->vtag == 0) &&
		((rx_desc)->reserve == 0));
}

static void desc_tx_take(tx_desc *tx_desc)
{
	if (desc_tx_dma(tx_desc))
		tx_desc->ctrl_len |= TX_COWN;
}

static void desc_rx_take(rx_desc *rx_desc)
{
	if (desc_rx_dma(rx_desc))
		rx_desc->ctrl_len |= RX_COWN;
}

int star_dma_init(star_dev *dev, uintptr_t desc_viraddr,
		  dma_addr_t desc_dmaaddrdr)
{
	int i;
	void __iomem *base = dev->base;

	STAR_PR_DEBUG("%s virAddr=0x%lx\n", __func__, desc_viraddr);
	dev->tx_ring_size = TX_DESC_NUM;
	dev->rx_ring_size = RX_DESC_NUM;

	dev->tx_desc = (tx_desc *)desc_viraddr;
	dev->rx_desc = (rx_desc *)dev->tx_desc + dev->tx_ring_size;

	for (i = 0; i < dev->tx_ring_size; i++)
		desc_tx_init(dev->tx_desc + i, i == dev->tx_ring_size - 1);
	for (i = 0; i < dev->rx_ring_size; i++)
		desc_rx_init(dev->rx_desc + i, i == dev->rx_ring_size - 1);

	dev->tx_head = 0;
	dev->tx_tail = 0;
	dev->rx_head = 0;
	dev->rx_tail = 0;
	dev->tx_num = 0;
	dev->rx_num = 0;

	/* Set Tx/Rx descriptor address */
	star_set_reg(STAR_TX_BASE_ADDR(base), (u32)desc_dmaaddrdr);
	star_set_reg(STAR_TX_DPTR(base), (u32)desc_dmaaddrdr);
	star_set_reg(STAR_RX_BASE_ADDR(base),
		     (u32)desc_dmaaddrdr + sizeof(tx_desc) * dev->tx_ring_size);
	star_set_reg(STAR_RX_DPTR(base),
		     (u32)desc_dmaaddrdr + sizeof(tx_desc) * dev->tx_ring_size);

	star_intr_disable(dev);

	return 0;
}

int star_dma_tx_set(star_dev *dev, u32 buffer, u32 length, uintptr_t ext_buf)
{
	int is_tx_last;
	int desc_idx = dev->tx_head;
	tx_desc *tx_desc = dev->tx_desc + desc_idx;
	u32 len = (((length < 60) ? 60 : length) & TX_LEN_MASK)
		  << TX_LEN_OFFSET;

	/* Error checking */
	if (dev->tx_num == dev->tx_ring_size)
		goto err;
	/* descriptor is not empty - cannot set */
	if (!desc_tx_empty(tx_desc))
		goto err;

	tx_desc->buffer = buffer;
	tx_desc->ctrl_len |= len | TX_FS | TX_LS | TX_INT;
#ifdef CONFIG_ARM64
	tx_skb_reserve[desc_idx] = ext_buf;
#else
	tx_desc->reserve = ext_buf;
#endif
	/* star memory barrier */
	wmb();
	/* Set HW own */
	tx_desc->ctrl_len &= ~TX_COWN;

	dev->tx_num++;
	is_tx_last = desc_tx_last(tx_desc);
	dev->tx_head = is_tx_last ? 0 : desc_idx + 1;

	return desc_idx;
err:
	return -1;
}

int star_dma_tx_get(star_dev *dev, u32 *buffer,
		    u32 *ctrl_len, uintptr_t *ext_buf)
{
	int is_tx_last;
	int desc_idx = dev->tx_tail;
	tx_desc *tx_desc = dev->tx_desc + desc_idx;

	if (dev->tx_num == 0)
		goto err;
	if (desc_tx_dma(tx_desc))
		goto err;
	if (desc_tx_empty(tx_desc))
		goto err;

	if (buffer != 0)
		*buffer = tx_desc->buffer;
	if (ctrl_len != 0)
		*ctrl_len = tx_desc->ctrl_len;

#ifdef CONFIG_ARM64
	if (ext_buf != 0)
		*ext_buf = tx_skb_reserve[desc_idx];
#else
	if (ext_buf != 0)
		*ext_buf = tx_desc->reserve;
#endif
	/* add star memory barrier */
	rmb();

	desc_tx_init(tx_desc, desc_tx_last(tx_desc));
	dev->tx_num--;
	is_tx_last = desc_tx_last(tx_desc);
	dev->tx_tail = is_tx_last ? 0 : desc_idx + 1;

	return desc_idx;
err:
	return -1;
}

int star_dma_rx_set(star_dev *dev, u32 buffer, u32 length, uintptr_t ext_buf)
{
	int desc_idx = dev->rx_head;
	rx_desc *rx_desc = dev->rx_desc + desc_idx;
	int is_rx_last;

	/* Error checking */
	if (dev->rx_num == dev->rx_ring_size)
		goto err;
	/* descriptor is not empty - cannot set */
	if (!desc_rx_empty(rx_desc))
		goto err;

	rx_desc->buffer = buffer;
	rx_desc->ctrl_len |= ((length & RX_LEN_MASK) << RX_LEN_OFFSET);
#ifdef CONFIG_ARM64
	rx_skb_reserve[desc_idx] = ext_buf;
#else
	rx_desc->reserve = ext_buf;
#endif
	/* star memory barrier */
	wmb();
	/* Set HW own */
	rx_desc->ctrl_len &= ~RX_COWN;

	dev->rx_num++;
	is_rx_last = desc_rx_last(rx_desc);
	dev->rx_head = is_rx_last ? 0 : desc_idx + 1;

	return desc_idx;
err:
	return -1;
}

int star_dma_rx_get(star_dev *dev, u32 *buffer,
		    u32 *ctrl_len, uintptr_t *ext_buf)
{
	int is_rx_last;
	int desc_idx = dev->rx_tail;
	rx_desc *rx_desc = dev->rx_desc + desc_idx;

	/* Error checking */
	/* No buffer can be got */
	if (dev->rx_num == 0)
		goto err;
	/* descriptor is owned by DMA - cannot get */
	if (desc_rx_dma(rx_desc))
		goto err;
	/* descriptor is empty - cannot get */
	if (desc_rx_empty(rx_desc))
		goto err;

	if (buffer != 0)
		*buffer = rx_desc->buffer;
	if (ctrl_len != 0)
		*ctrl_len = rx_desc->ctrl_len;
#ifdef CONFIG_ARM64
	if (ext_buf != 0)
		*ext_buf = rx_skb_reserve[desc_idx];
#else
	if (ext_buf != 0)
		*ext_buf = rx_desc->reserve;
#endif
	/* star memory barrier */
	rmb();

	desc_rx_init(rx_desc, desc_rx_last(rx_desc));
	dev->rx_num--;
	is_rx_last = desc_rx_last(rx_desc);
	dev->rx_tail = is_rx_last ? 0 : desc_idx + 1;

	return desc_idx;
err:
	return -1;
}

void star_dma_tx_stop(star_dev *dev)
{
	int i;

	star_dma_tx_disable(dev);
	for (i = 0; i < dev->tx_ring_size; i++)
		desc_tx_take(dev->tx_desc + i);
}

void star_dma_rx_stop(star_dev *dev)
{
	int i;

	star_dma_rx_disable(dev);
	for (i = 0; i < dev->rx_ring_size; i++)
		desc_rx_take(dev->rx_desc + i);
}

int star_mac_init(star_dev *dev, u8 mac_addr[6])
{
	void __iomem *base = dev->base;

	STAR_PR_DEBUG("MAC Initialization\n");

	/* Set Mac Address */
	star_set_reg(star_my_mac_h(base),
		     mac_addr[0] << 8 | mac_addr[1] << 0);
	star_set_reg(star_my_mac_l(base),
		     mac_addr[2] << 24 | mac_addr[3] << 16 |
				mac_addr[4] << 8 | mac_addr[5] << 0);

	/* Set Mac Configuration */
	star_set_reg(STAR_MAC_CFG(base),
		     STAR_MAC_CFG_CRCSTRIP |
		     STAR_MAC_CFG_MAXLEN_1522 |
		     /* 12 byte IPG */
		     (0x1f & STAR_MAC_CFG_IPG_MASK) << STAR_MAC_CFG_IPG_OFFSET);

	/* Init Flow Control register */
	star_set_reg(STAR_FC_CFG(base),
		     STAR_FC_CFG_SEND_PAUSE_TH_DEF |
		     STAR_FC_CFG_UCPAUSEDIS |
		     STAR_FC_CFG_BPEN);

	/* Init SEND_PAUSE_RLS */
	star_set_reg(star_extend_cfg(base), STAR_EXTEND_CFG_SEND_PAUSE_RLS_DEF);

	/* Init MIB counter (reset to 0) */
	star_mib_init(dev);

	/* Enable Hash Table BIST */
	star_set_bit(star_hash_ctrl(base), STAR_HASH_CTRL_HASHEN);

	/* Reset Hash Table (All reset to 0) */
	star_reset_hash_table(dev);
	star_clear_bit(STAR_ARL_CFG(base), STAR_ARL_CFG_MISCMODE);
	star_clear_bit(STAR_ARL_CFG(base), STAR_ARL_CFG_HASHALG_CRCDA);

	/*Recv VLAN tag in RX packet */
	star_clear_bit(STAR_MAC_CFG(base), STAR_MAC_CFG_VLANSTRIP);

	return 0;
}

static void star_mib_reset(star_dev *dev)
{
	void __iomem *base = dev->base;

	star_get_reg(STAR_MIB_RXOKPKT(base));
	star_get_reg(STAR_MIB_RXOKBYTE(base));
	star_get_reg(STAR_MIB_RXRUNT(base));
	star_get_reg(STAR_MIB_RXOVERSIZE(base));
	star_get_reg(STAR_MIB_RXNOBUFDROP(base));
	star_get_reg(STAR_MIB_RXCRCERR(base));
	star_get_reg(STAR_MIB_RXARLDROP(base));
	star_get_reg(STAR_MIB_RXVLANDROP(base));
	star_get_reg(STAR_MIB_RXCKSERR(base));
	star_get_reg(STAR_MIB_RXPAUSE(base));
	star_get_reg(STAR_MIB_TXOKPKT(base));
	star_get_reg(STAR_MIB_TXOKBYTE(base));
	star_get_reg(STAR_MIB_TXPAUSECOL(base));
}

int star_mib_init(star_dev *dev)
{
	star_mib_reset(dev);

	return 0;
}

int star_phyctrl_init(star_dev *dev, u32 enable, u32 phy_addr)
{
	u32 data;
	void __iomem *base = dev->base;

	data = STAR_PHY_CTRL1_FORCETXFC |
	STAR_PHY_CTRL1_FORCERXFC |
	STAR_PHY_CTRL1_FORCEFULL |
	STAR_PHY_CTRL1_FORCESPD_100M |
	STAR_PHY_CTRL1_ANEN;

	STAR_PR_DEBUG("PHY Control Initialization\n");
	/* Enable/Disable PHY auto-polling */
	if (enable)
		star_set_reg(STAR_PHY_CTRL1(base),
			     data | STAR_PHY_CTRL1_APEN |
			     (phy_addr &
			     STAR_PHY_CTRL1_phy_addr_MASK)
			     << STAR_PHY_CTRL1_phy_addr_OFFSET);
	else
		star_set_reg(STAR_PHY_CTRL1(base), data | STAR_PHY_CTRL1_APDIS);

	return 0;
}

void star_set_hashbit(star_dev *dev, u32 addr, u32 value)
{
	u32 data;
	void __iomem *base = dev->base;

	STAR_POLLING_TIMEOUT(star_is_set_bit(star_hash_ctrl(base),
					     STAR_HASH_CTRL_HTBISTDONE));
	STAR_POLLING_TIMEOUT(star_is_set_bit(star_hash_ctrl(base),
					     STAR_HASH_CTRL_HTBISTOK));
	STAR_POLLING_TIMEOUT(!star_is_set_bit(star_hash_ctrl(base),
					      STAR_HASH_CTRL_START));

	data = (STAR_HASH_CTRL_HASHEN |
		STAR_HASH_CTRL_ACCESSWT | STAR_HASH_CTRL_START |
		(value ? STAR_HASH_CTRL_HBITDATA : 0) |
		(addr &	STAR_HASH_CTRL_HBITADDR_MASK)
		<< STAR_HASH_CTRL_HBITADDR_OFFSET);
	star_set_reg(star_hash_ctrl(base), data);
	STAR_POLLING_TIMEOUT(!star_is_set_bit(star_hash_ctrl(base),
					      STAR_HASH_CTRL_START));
}

int star_hw_init(star_dev *dev)
{
	star_set_reg(ETHSYS_CONFIG(dev->base),
		     SWC_MII_MODE | EXT_MDC_MODE | MII_PAD_OE);
	star_set_reg(MAC_CLOCK_CONFIG(dev->base),
		     (star_get_reg(MAC_CLOCK_CONFIG(dev->base)) &
		     (~(0xff << 0))) | MDC_CLK_DIV_10);

	return 0;
}

void star_link_status_change(star_dev *dev)
{
	u32 val, speed;

	val = star_get_reg(STAR_PHY_CTRL1(dev->base));
	if (dev->link_up != ((val & STAR_PHY_CTRL1_STA_LINK) ? 1UL : 0UL)) {
		dev->link_up = (val & STAR_PHY_CTRL1_STA_LINK) ? 1UL : 0UL;
		STAR_PR_INFO("Link status: %s\n",
			     dev->link_up ? "Up" : "Down");
		if (dev->link_up == 1UL) {
			speed = ((val >> STAR_PHY_CTRL1_STA_SPD_OFFSET) &
				STAR_PHY_CTRL1_STA_SPD_MASK);
			STAR_PR_INFO("%s Duplex - %s Mbps mode\n",
				     (val & STAR_PHY_CTRL1_STA_FULL) ?
				     "Full" : "Half",
				     !speed ? "10" : (speed == 1 ? "100" :
				     (speed == 2 ? "1000" : "unknown")));
			STAR_PR_INFO(
				 "TX flow control:%s, RX flow control:%s\n",
				(val & STAR_PHY_CTRL1_STA_TXFC) ? "On" : "Off",
				(val & STAR_PHY_CTRL1_STA_RXFC) ? "On" : "Off");
		} else if (dev->link_up == 0UL) {
			netif_carrier_off(((star_private *)dev->star_prv)->dev);
		}
	}

	if (dev->link_up)
		netif_carrier_on(((star_private *)dev->star_prv)->dev);
}

void star_nic_pdset(star_dev *dev, bool flag)
{
#define MAX_NICPDRDY_RETRY  10000
	u32 data, retry = 0;

	data = star_get_reg(STAR_MAC_CFG(dev->base));
	if (flag) {
		data |= STAR_MAC_CFG_NICPD;
		star_set_reg(STAR_MAC_CFG(dev->base), data);
		/* wait until NIC_PD_READY and clear it */
		do {
			data = star_get_reg(STAR_MAC_CFG(dev->base));
			if (data & STAR_MAC_CFG_NICPDRDY) {
				/* clear NIC_PD_READY */
				data |= STAR_MAC_CFG_NICPDRDY;
				star_set_reg(STAR_MAC_CFG(dev->base), data);
				break;
			}
		} while (retry++ < MAX_NICPDRDY_RETRY);
		if (retry >= MAX_NICPDRDY_RETRY)
			STAR_PR_ERR("timeout MAX_NICPDRDY_RETRY(%d)\n",
				    MAX_NICPDRDY_RETRY);
	} else {
		data &= ~STAR_MAC_CFG_NICPD;
		star_set_reg(STAR_MAC_CFG(dev->base), data);
	}
}

void star_config_wol(star_dev *star_dev, bool enable)
{
	STAR_PR_INFO("[%s]%s wol\n", __func__,
		     enable ? "enable" : "disable");
	if (enable) {
		star_set_reg(star_int_sta(star_dev->base),
			     star_get_reg(star_int_sta(star_dev->base)));
		star_set_bit(STAR_MAC_CFG(star_dev->base), STAR_MAC_CFG_WOLEN);
		star_mb();
		star_clear_bit(star_int_mask(star_dev->base),
			       STAR_INT_STA_MAGICPKT);
	} else {
		star_clear_bit(STAR_MAC_CFG(star_dev->base),
			       STAR_MAC_CFG_WOLEN);
		star_mb();
		star_set_bit(star_int_mask(star_dev->base),
			     STAR_INT_STA_MAGICPKT);
	}
}

void star_switch_to_rmii_mode(star_dev *star_dev)
{
	u32 reg_val;

	reg_val = star_get_reg(star_dev->pericfg_base + 0x10);
	reg_val &= ~(0xf << 0);
	/* select RMII mode */
	reg_val |= (0x1 << 0);
	star_set_reg(star_dev->pericfg_base + 0x10, reg_val);

#ifdef STAR_USE_TX_CLOCK
	reg_val = star_get_reg(star_dev->pericfg_base + 0x10);
	reg_val &= ~(0x1 << 8);
	/* select tx clock */
	reg_val |= (0x1 << 8);
	star_set_reg(star_dev->pericfg_base + 0x10, reg_val);
#endif
}

