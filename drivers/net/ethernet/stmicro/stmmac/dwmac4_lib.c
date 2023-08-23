/*
 * Copyright (C) 2007-2015  STMicroelectronics Ltd
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * Author: Alexandre Torgue <alexandre.torgue@st.com>
 */

#include <linux/io.h>
#include <linux/delay.h>
#include "common.h"
#include "dwmac4_dma.h"
#include "dwmac4.h"

int dwmac4_dma_reset(void __iomem *ioaddr)
{
	u32 value = readl(ioaddr + DMA_BUS_MODE);
	int limit;

	/* DMA SW reset */
	value |= DMA_BUS_MODE_SFT_RESET;
	writel(value, ioaddr + DMA_BUS_MODE);
	limit = 10;
	while (limit--) {
		if (!(readl(ioaddr + DMA_BUS_MODE) & DMA_BUS_MODE_SFT_RESET))
			break;
		mdelay(10);
	}

	if (limit < 0)
		return -EBUSY;

	return 0;
}

void dwmac4_set_rx_tail_ptr(void __iomem *ioaddr, u32 tail_ptr, u32 chan)
{
	writel(tail_ptr, ioaddr + DMA_CHAN_RX_END_ADDR(chan));
}

void dwmac4_set_tx_tail_ptr(void __iomem *ioaddr, u32 tail_ptr, u32 chan)
{
	writel(tail_ptr, ioaddr + DMA_CHAN_TX_END_ADDR(chan));
}

void dwmac4_dma_start_tx(void __iomem *ioaddr, u32 chan)
{
	u32 value = readl(ioaddr + DMA_CHAN_TX_CONTROL(chan));

	value |= DMA_CONTROL_ST;
	writel(value, ioaddr + DMA_CHAN_TX_CONTROL(chan));

	value = readl(ioaddr + GMAC_CONFIG);
	value |= GMAC_CONFIG_TE;
	writel(value, ioaddr + GMAC_CONFIG);
}

void dwmac4_dma_stop_tx(void __iomem *ioaddr, u32 chan)
{
	u32 value = readl(ioaddr + DMA_CHAN_TX_CONTROL(chan));

	value &= ~DMA_CONTROL_ST;
	writel(value, ioaddr + DMA_CHAN_TX_CONTROL(chan));

	value = readl(ioaddr + GMAC_CONFIG);
	value &= ~GMAC_CONFIG_TE;
	writel(value, ioaddr + GMAC_CONFIG);
}

void dwmac4_dma_start_rx(void __iomem *ioaddr, u32 chan)
{
	u32 value = readl(ioaddr + DMA_CHAN_RX_CONTROL(chan));

	value |= DMA_CONTROL_SR;

	writel(value, ioaddr + DMA_CHAN_RX_CONTROL(chan));

	value = readl(ioaddr + GMAC_CONFIG);
	value |= GMAC_CONFIG_RE;
	writel(value, ioaddr + GMAC_CONFIG);
}

void dwmac4_dma_stop_rx(void __iomem *ioaddr, u32 chan)
{
	u32 value = readl(ioaddr + DMA_CHAN_RX_CONTROL(chan));

	value &= ~DMA_CONTROL_SR;
	writel(value, ioaddr + DMA_CHAN_RX_CONTROL(chan));

	value = readl(ioaddr + GMAC_CONFIG);
	value &= ~GMAC_CONFIG_RE;
	writel(value, ioaddr + GMAC_CONFIG);
}

void dwmac4_set_tx_ring_len(void __iomem *ioaddr, u32 len, u32 chan)
{
	writel(len, ioaddr + DMA_CHAN_TX_RING_LEN(chan));
}

void dwmac4_set_rx_ring_len(void __iomem *ioaddr, u32 len, u32 chan)
{
	writel(len, ioaddr + DMA_CHAN_RX_RING_LEN(chan));
}

void dwmac4_enable_dma_irq(void __iomem *ioaddr, u32 chan)
{
	writel(DMA_CHAN_INTR_DEFAULT_MASK, ioaddr +
	       DMA_CHAN_INTR_ENA(chan));
}

void dwmac410_enable_dma_irq(void __iomem *ioaddr, u32 chan)
{
	writel(DMA_CHAN_INTR_DEFAULT_MASK_4_10,
	       ioaddr + DMA_CHAN_INTR_ENA(chan));
}

void dwmac4_disable_dma_irq(void __iomem *ioaddr, u32 chan)
{
	writel(0, ioaddr + DMA_CHAN_INTR_ENA(chan));
}

int dwmac4_dma_interrupt(void __iomem *ioaddr,
			 struct stmmac_extra_stats *x, u32 chan)
{
	int ret = 0;

	u32 intr_status = readl(ioaddr + DMA_CHAN_STATUS(chan));

	/* ABNORMAL interrupts */
	if (unlikely(intr_status & DMA_CHAN_STATUS_AIS)) {
		if (unlikely(intr_status & DMA_CHAN_STATUS_RBU))
			x->rx_buf_unav_irq++;
		if (unlikely(intr_status & DMA_CHAN_STATUS_RPS))
			x->rx_process_stopped_irq++;
		if (unlikely(intr_status & DMA_CHAN_STATUS_RWT))
			x->rx_watchdog_irq++;
		if (unlikely(intr_status & DMA_CHAN_STATUS_ETI))
			x->tx_early_irq++;
		if (unlikely(intr_status & DMA_CHAN_STATUS_TPS)) {
			x->tx_process_stopped_irq++;
			ret = tx_hard_error;
		}
		if (unlikely(intr_status & DMA_CHAN_STATUS_FBE)) {
			x->fatal_bus_error_irq++;
			ret = tx_hard_error;
		}
	}
	/* TX/RX NORMAL interrupts */
	if (likely(intr_status & DMA_CHAN_STATUS_NIS)) {
		x->normal_irq_n++;
		if (likely(intr_status & DMA_CHAN_STATUS_RI)) {
			u32 value;

			value = readl(ioaddr + DMA_CHAN_INTR_ENA(chan));
			/* to schedule NAPI on real RIE event. */
			if (likely(value & DMA_CHAN_INTR_ENA_RIE)) {
				x->rx_normal_irq_n++;
				ret |= handle_rx;
			}
		}
		if (likely(intr_status & DMA_CHAN_STATUS_TI)) {
			x->tx_normal_irq_n++;
			ret |= handle_tx;
		}
		if (unlikely(intr_status & DMA_CHAN_STATUS_ERI))
			x->rx_early_irq++;
	}

	/* Clear the interrupt by writing a logic 1 to the chanX interrupt
	 * status [21-0] expect reserved bits [5-3]
	 */
	writel((intr_status & 0x3fffc7),
	       ioaddr + DMA_CHAN_STATUS(chan));

	return ret;
}

void stmmac_dwmac4_set_mac_addr(void __iomem *ioaddr, u8 addr[6],
				unsigned int high, unsigned int low)
{
	unsigned long data;

	data = (addr[5] << 8) | addr[4];
	/* For MAC Addr registers se have to set the Address Enable (AE)
	 * bit that has no effect on the High Reg 0 where the bit 31 (MO)
	 * is RO.
	 */
	data |= (STMMAC_CHAN0 << GMAC_HI_DCS_SHIFT);
	writel(data | GMAC_HI_REG_AE, ioaddr + high);
	data = (addr[3] << 24) | (addr[2] << 16) | (addr[1] << 8) | addr[0];
	writel(data, ioaddr + low);
}

/* Enable disable MAC RX/TX */
void stmmac_dwmac4_set_mac(void __iomem *ioaddr, bool enable)
{
	u32 value = readl(ioaddr + GMAC_CONFIG);

	if (enable)
		value |= GMAC_CONFIG_RE | GMAC_CONFIG_TE;
	else
		value &= ~(GMAC_CONFIG_TE | GMAC_CONFIG_RE);

	writel(value, ioaddr + GMAC_CONFIG);
}

void stmmac_dwmac4_get_mac_addr(void __iomem *ioaddr, unsigned char *addr,
				unsigned int high, unsigned int low)
{
	unsigned int hi_addr, lo_addr;

	/* Read the MAC address from the hardware */
	hi_addr = readl(ioaddr + high);
	lo_addr = readl(ioaddr + low);

	/* Extract the MAC address from the high and low words */
	addr[0] = lo_addr & 0xff;
	addr[1] = (lo_addr >> 8) & 0xff;
	addr[2] = (lo_addr >> 16) & 0xff;
	addr[3] = (lo_addr >> 24) & 0xff;
	addr[4] = hi_addr & 0xff;
	addr[5] = (hi_addr >> 8) & 0xff;
}

/**
 *  stmmac_set_vlan_filter_rx_queue - Configure VLAN register
 *  @priv: driver private structure
 *  Description: It is used for configurring QMI over ethernet
 */
void stmmac_set_vlan_filter_rx_queue(struct vlan_filter_info *vlan,
				     void __iomem *ioaddr)
{
	u32 queue = vlan->rx_queue;
	u32 vlan_offset = vlan->vlan_offset;
	u32 vlan_id = vlan->vlan_id;
	u32 value = 0;
	u32 retry_count = 5;
	u32 count = 0;

	pr_info("%s init value:\n", __func__);
	pr_info("rx_queue %u, vlan_offset %u vlan_id %u\n",
		queue, vlan_offset, vlan_id);

	if (queue >= 4)
		return;

	if (vlan_id >= 4096)
		return;

/* Check if operation is busy before write */
	while (1) {
		if (count > retry_count)
			return;
		value = readl_relaxed(ioaddr + GMAC_VLAN_CTRL_TAG);
		if ((value & GMAC_VLANTR_OB_MASK) == 0x0)
			break;
		count++;
		usleep_range(500, 1000);
	}
	value = readl_relaxed(ioaddr + GMAC_VLAN_DATA_TAG);
	value = vlan_id;
	value |= GMAC_VLANTR_VLAN_EN;
	value |= GMAC_VLANTR_VLAN_CMP;
	value |= GMAC_VLANTR_VLAN_CMP_DISABLE;
	value |= GMAC_VLANTR_DMA_CHAN_EN;
	value |= (queue << GMAC_VLANTR_DMA_CHAN_NUM);
	pr_info("%s VLAN_DATA_TAG val %x\n", __func__, value);
	writel_relaxed(value, ioaddr + GMAC_VLAN_DATA_TAG);

	count = 1;
/* Write the above value to offset if operation is not busy*/
	while (1) {
		if (count > retry_count)
			return;
		value = readl_relaxed(ioaddr + GMAC_VLAN_CTRL_TAG);
		if ((value & GMAC_VLANTR_OB_MASK) == 0x0) {
			value |= GMAC_VLANTR_OB_MASK;
			value &= ~(GMAC_VLANTR_CT_MASKBIT);
			value |= (vlan_offset << GMAC_VLANTR_OFFSET_SHIFT);
			pr_info("VLAN_CTRL_TAG val %x\n", value);
			writel_relaxed(value, ioaddr + GMAC_VLAN_CTRL_TAG);
			break;
		}
		count++;
		usleep_range(500, 1000);
	}

/*Configure DMA register to route packets to particular queue*/
	value = readl_relaxed(ioaddr + GMAC_MTL_RX_QMAP);
	value |= GMAC_MTL_RXQ_DMACH;
	pr_info("GMAC_MTL_RX_QMAP val %x\n", value);
	writel_relaxed(value, ioaddr + GMAC_MTL_RX_QMAP);
}
