// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sipa_phy: " fmt

#include <linux/sipa.h>
#include <linux/kernel.h>
#include <linux/dma-mapping.h>

#include "../sipa_priv.h"

#define PTR_MASK(depth) \
({\
	typeof(depth) tmp = (depth); \
	tmp | (tmp - 1); \
})

#define SIPA_FIFO_USB_UL_OFFSET		0x0l
#define SIPA_FIFO_WIFI_UL_OFFSET	0x80l
#define SIPA_FIFO_PCIE_UL_OFFSET	0x100l
#define SIPA_FIFO_WIAP_DL_OFFSET	0x180l
#define SIPA_FIFO_MAP_IN_OFFSET		0x200l
#define SIPA_FIFO_USB_DL_OFFSET		0x280l
#define SIPA_FIFO_WIFI_DL_OFFSET	0x300l
#define SIPA_FIFO_PCIE_DL_DFFSET	0x380l
#define SIPA_FIFO_WIAP_UL_OFFSET	0x400l
#define SIAP_FIFO_MAP0_OUT_OFFSET	0x480l
#define SIAP_FIFO_MAP1_OUT_OFFSET	0x500l
#define SIAP_FIFO_MAP2_OUT_OFFSET	0x580l
#define SIAP_FIFO_MAP3_OUT_OFFSET	0x600l
#define SIAP_FIFO_MAP4_OUT_OFFSET	0x680l
#define SIAP_FIFO_MAP5_OUT_OFFSET	0x700l
#define SIAP_FIFO_MAP6_OUT_OFFSET	0x780l
#define SIAP_FIFO_MAP7_OUT_OFFSET       0x800l

/* common fifo reg */
#define IPA_COMMON_RX_FIFO_DEPTH	0x00l
#define IPA_COMMON_RX_FIFO_WR		0x04l
#define IPA_COMMON_RX_FIFO_RD		0x08l
#define IPA_COMMON_TX_FIFO_DEPTH	0x0Cl
#define IPA_COMMON_TX_FIFO_WR		0x10l
#define IPA_COMMON_TX_FIFO_RD		0x14l
#define IPA_COMMON_RX_FIFO_ADDRL	0x18l
#define IPA_COMMON_RX_FIFO_ADDRH	0x1Cl
#define IPA_COMMON_TX_FIFO_ADDRL	0x20l
#define IPA_COMMON_TX_FIFO_ADDRH	0x24l
#define IPA_PERFETCH_FIFO_CTL		0x28l
#define IPA_INT_GEN_CTL_TX_FIFO_VALUE	0x2Cl
#define IPA_INT_GEN_CTL_EN		0x30l
#define IPA_DROP_PACKET_CNT		0x34l
#define IPA_TX_FIFO_FLOW_CTRL		0x3Cl
#define IPA_RX_FIFO_FLOW_CTRL		0x40l
#define IPA_RX_FIFO_FULL_NEG_PULSE_NUM	0x44l
#define IPA_INT_GEN_CTL_CLR		0x48l
#define IPA_FIFO_INT_ADDR_HIGH		0x4Cl
#define IPA_FIFO_INT_ADDR_LOW		0x50l
#define IPA_FLW_CTL_INT_ADDR_LOW	0x54l
#define IPA_FIFO_INT_PATTERN		0x58l
#define IPA_FLW_CTL_INT_PATTERN		0x5Cl
#define IPA_COMMON_TX_FIFO_WR_INIT	0x60l
#define IPA_COMMON_RX_FIFO_AXI_STS	0x64l
#define IPA_ERRCODE_INT_ADDR_LOW	0x68l
#define IPA_ERRCODE_INT_PATTERN		0x6Cl

/* fifo interrupt enable bit */
#define IPA_TXFIFO_INT_THRES_ONESHOT_EN		BIT(11)
#define IPA_TXFIFO_INT_THRES_SW_EN		BIT(10)
#define IPA_TXFIFO_INT_DELAY_TIMER_SW_EN	BIT(9)
#define IPA_TXFIFO_FULL_INT_EN			BIT(8)
#define IPA_TXFIFO_OVERFLOW_EN			BIT(7)
#define IPA_ERRORCODE_IN_TX_FIFO_EN		BIT(6)
#define IPA_DROP_PACKET_OCCUR_INT_EN		BIT(5)
#define IPA_RX_FIFO_INT_EXIT_FLOW_CTRL_EN	BIT(4)
#define IPA_RX_FIFO_INT_ENTER_FLOW_CTRL_EN	BIT(3)
#define IPA_TX_FIFO_INTR_SW_BIT_EN		BIT(2)
#define IPA_TX_FIFO_THRESHOLD_EN		BIT(1)
#define IPA_TX_FIFO_DELAY_TIMER_EN		BIT(0)
#define IPA_INT_EN_BIT_GROUP			0x00000FFFl

/* fifo interrupt status bit */
#define IPA_INT_TX_FIFO_THRESHOLD_SW_STS	BIT(22)
#define IPA_INT_EXIT_FLOW_CTRL_STS		BIT(20)
#define IPA_INT_ENTER_FLOW_CTRL_STS		BIT(19)
#define IPA_INT_TXFIFO_FULL_INT_STS		BIT(18)
#define IPA_INT_TXFIFO_OVERFLOW_STS		BIT(17)
#define IPA_INT_ERRORCODE_IN_TX_FIFO_STS	BIT(16)
#define IPA_INT_INTR_BIT_STS			BIT(15)
#define IPA_INT_THRESHOLD_STS			BIT(14)
#define IPA_INT_DELAY_TIMER_STS			BIT(13)
#define IPA_INT_DROP_PACKT_OCCUR		BIT(12)
#define IPA_INT_INT_STS_GROUP			0x5FF000l

/* fifo interrupt clear bit */
#define IPA_TX_FIFO_TIMER_CLR_BIT		BIT(0)
#define IPA_TX_FIFO_THRESHOLD_CLR_BIT		BIT(1)
#define IPA_TX_FIFO_INTR_CLR_BIT		BIT(2)
#define IPA_ENTER_FLOW_CONTROL_CLR_BIT		BIT(3)
#define IPA_EXIT_FLOW_CONTROL_CLR_BIT		BIT(4)
#define IPA_DROP_PACKET_INTR_CLR_BIT		BIT(5)
#define IPA_ERROR_CODE_INTR_CLR_BIT		BIT(6)
#define IPA_TX_FIFO_OVERFLOW_CLR_BIT		BIT(7)
#define IPA_TX_FIFO_FULL_INT_CLR_BIT		BIT(8)
#define IPA_INT_STS_CLR_GROUP			(0x000001FFl)

/* fifo flow control config */
#define IPA_CMN_FIFO_FLOW_CTL_CONFIG		(GENMASK(1, 0))
#define IPA_CMN_FIFO_FLOW_CTL_RECOVER		BIT(2)
#define IPA_CMN_FIFO_FLOW_CTL_STOP_RECEIVE	BIT(3)
#define IPA_CMN_FIFO_FLOW_CTL_NON_STOP		BIT(4)
#define IPA_FLOW_CTRL_CONFIG			0x38l

#define NODE_DESCRIPTION_SIZE			16l
/**
 * Description: set rx fifo total depth.
 * Input:
 *   @fifo_base: Need to set total depth of the fifo,
 *               the base address of the FIFO.
 *   @depth: the size of depth.
 * return:
 *   0: set successfully.
 *   -EINVAL: set failed.
 * Note:
 */
static int ipa_phy_set_rx_fifo_total_depth(void __iomem *fifo_base,
					   u32 depth)
{
	u32 tmp;

	if (depth > 0xFFFF)
		return -EINVAL;

	tmp = readl_relaxed(fifo_base + IPA_COMMON_RX_FIFO_DEPTH);
	tmp &= 0x0000FFFF;
	tmp |= (depth << 16);
	writel_relaxed(tmp, fifo_base + IPA_COMMON_RX_FIFO_DEPTH);

	tmp = readl_relaxed(fifo_base + IPA_COMMON_RX_FIFO_DEPTH);
	if ((tmp >> 16) != depth)
		return -EIO;

	return 0;
}

/**
 * Description: get rx fifo total depth.
 * Input:
 *   @fifo_base: Need to get total depth of the fifo, the base address of the
 *              FIFO.
 * return: The size of toal depth.
 * Note:
 */
static u32 ipa_phy_get_rx_fifo_total_depth(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_COMMON_RX_FIFO_DEPTH);

	return (tmp >> 16) & 0x0000FFFF;
}

/**
 * Description: get rx fifo filled depth.
 * Input:
 *   @fifo_base: Need to get filled depth of the FIFO, the base address of the
 *              FIFO.
 * return:
 *   0: The size of rx filled depth
 * Note:
 */
static u32 ipa_phy_get_rx_fifo_filled_depth(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_COMMON_RX_FIFO_DEPTH);

	return tmp & 0x0000FFFF;
}

/**
 * Description: get rx fifo full status.
 * Input:
 *   @fifo_base: Need to get rx fifo full status of the FIFO, the base address
 *              of the FIFO.
 * return:
 *   1: rx fifo full.
 *   0: rx fifo not full.
 * Note:
 */
static u32 ipa_phy_get_rx_fifo_full_status(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_COMMON_RX_FIFO_WR);

	return tmp & 0x00000001;
}

/**
 * Description: update rx fifo write pointer.
 * Input:
 *   @fifo_base: Need to update rx fifo write pointer of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   0: update rx fifo write pointer successfully,
 *   -EINVAL: update rx fifo write pointer failed.
 * Note:
 */
static int ipa_phy_update_rx_fifo_wptr(void __iomem *fifo_base,
				       u32 wptr)
{
	u32 tmp;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_COMMON_RX_FIFO_WR;

	if (wptr > 0xFFFF)
		return -EINVAL;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0x0000FFFF;
	tmp |= (wptr << 16);
	writel_relaxed(tmp, fifo_reg_addr);

	tmp = readl_relaxed(fifo_reg_addr);
	if (tmp >> 16 != wptr)
		return -EIO;

	return 0;
}

/**
 * Description: get rx fifo write pointer.
 * Input:
 *   @fifo_base: Need to get rx fifo write pointer of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   The write pointer of rx fifo.
 * Note:
 */
static u32 ipa_phy_get_rx_fifo_wptr(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_COMMON_RX_FIFO_WR);

	return (tmp >> 16);
}

/**
 * Description: update rx fifo read pointer.
 * Input:
 *   @fifo_base: Need to update rx fifo read pointer of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   0: update rx fifo read pointer successfully,
 *   -EINVAL: update rx fifo read pointer failed.
 * Note:
 */
static int ipa_phy_update_rx_fifo_rptr(void __iomem *fifo_base,
				       u32 rptr)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_COMMON_RX_FIFO_RD;

	if (rptr > 0xFFFF)
		return -EINVAL;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0x0000FFFF;
	tmp |= (rptr << 16);
	writel_relaxed(tmp, fifo_reg_addr);

	tmp = readl_relaxed(fifo_reg_addr);
	if ((tmp >> 16) != rptr)
		return -EIO;

	return 0;
}

/**
 * Description: get rx fifo read pointer.
 * Input:
 *   @fifo_base: Need to get rx fifo read pointer of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   The read pointer of rx fifo.
 * Note:
 */
static u32 ipa_phy_get_rx_fifo_rptr(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_COMMON_RX_FIFO_RD);

	return (tmp >> 16);
}

/**
 * Description: get rx fifo empty status.
 * Input:
 *   @fifo_base: Need to get rx fifo empty status of the FIFO, the base
 *               address of the FIFO.
 * return:
 *   The empty status of rx fifo.
 * Note:
 */
static bool ipa_phy_get_rx_fifo_empty_status(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_COMMON_RX_FIFO_RD);

	return tmp & 0x00000001;
}

/**
 * Description: set tx fifo total depth.
 * Input:
 *   @fifo_base: Need to set tx fifo empty status of the FIFO, the base
 *		 address of the FIFO.
 * return:
 *   0: set tx fifo total depth successfully.
 *   -EINVAL: set tx fifo total_depth failed.
 * Note:
 */
static int ipa_phy_set_tx_fifo_total_depth(void __iomem *fifo_base,
					   u32 depth)
{
	u32 tmp;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_COMMON_TX_FIFO_DEPTH;

	if (depth > 0xFFFF)
		return -EINVAL;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0x0000FFFF;
	tmp |= (depth << 16);
	writel_relaxed(tmp, fifo_reg_addr);

	tmp = readl_relaxed(fifo_reg_addr);
	if ((tmp >> 16) != depth)
		return -EIO;

	return 0;
}

/**
 * Description: get tx fifo total depth.
 * Input:
 *   @fifo_base: Need to get tx fifo empty status of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   The total depth of tx fifo.
 * Note:
 */
static u32 ipa_phy_get_tx_fifo_total_depth(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_COMMON_TX_FIFO_DEPTH);

	return ((tmp >> 16) & 0x0000FFFF);
}

/**
 * Description: get tx fifo filled depth.
 * Input:
 *   @fifo_base: Need to get tx fifo filled depth of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   The tx fifo filled depth.
 * Note:
 */
static u32 ipa_phy_get_tx_fifo_filled_depth(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_COMMON_TX_FIFO_DEPTH);

	return (tmp & 0x0000FFFF);
}

/**
 * Description: get tx fifo full status.
 * Input:
 *   @fifo_base: Need to get tx fifo full status of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   The full status of tx fifo.
 * Note:
 */
static u32 ipa_phy_get_tx_fifo_full_status(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_COMMON_TX_FIFO_WR);

	return (tmp & 0x00000001);
}

/**
 * Description: get tx fifo empty status.
 * Input:
 *   @fifo_base: Need to get tx fifo empty status of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   The empty status of tx fifo.
 * Note:
 */
static u32 ipa_phy_get_tx_fifo_empty_status(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_COMMON_TX_FIFO_RD);

	return (tmp & 0x00000001);
}

/**
 * Description: update tx fifo write pointer.
 * Input:
 *   @fifo_base: Need to update tx fifo write pointer of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   0: update tx fifo write pointer successfully.
 *   -EINVAL: update tx fifo write pointer failed.
 * Note:
 */
static int ipa_phy_update_tx_fifo_wptr(void __iomem *fifo_base,
				       u32 wptr)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_COMMON_TX_FIFO_WR_INIT;

	if (tmp > 0xFFFF)
		return -EINVAL;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0x0000FFFF;
	tmp |= (wptr << 16);
	writel_relaxed(tmp, fifo_reg_addr);

	tmp = readl_relaxed(fifo_reg_addr);
	tmp |= BIT(1);
	writel_relaxed(tmp, fifo_reg_addr);

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0xFFFFFFFD;
	writel_relaxed(tmp, fifo_reg_addr);

	tmp = readl_relaxed(fifo_reg_addr);
	if ((tmp >> 16) != wptr)
		return -EIO;

	return 0;
}

/**
 * Description: get tx fifo write pointer.
 * Input:
 *   @fifo_base: Need to get tx fifo write pointer of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   The write pointer of tx fifo.
 * Note:
 */
static u32 ipa_phy_get_tx_fifo_wptr(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_COMMON_TX_FIFO_WR);

	return (tmp >> 16);
}

/**
 * Description: update tx fifo read pointer.
 * Input:
 *   @fifo_base: Need to update tx fifo read pointer of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   0: update tx fifo read pointer successfully.
 *   -EINVAL: update tx fifo read pointer failed.
 * Note:
 */
static int ipa_phy_update_tx_fifo_rptr(void __iomem *fifo_base,
				       u32 rptr)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_COMMON_TX_FIFO_RD;

	if (tmp > 0xFFFF)
		return -EINVAL;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0x0000FFFF;
	tmp |= (rptr << 16);
	writel_relaxed(tmp, fifo_reg_addr);

	tmp = readl_relaxed(fifo_reg_addr);
	if ((tmp >> 16) != rptr)
		return -EIO;

	return 0;
}

/**
 * Description: get tx fifo write pointer.
 * Input:
 *   @fifo_base: Need to get tx fifo write pointer of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   The write pointer of rx fifo.
 * Note:
 */
static u32 ipa_phy_get_tx_fifo_rptr(void __iomem *fifo_base)
{
	u32 tmp;

	tmp = readl_relaxed(fifo_base + IPA_COMMON_TX_FIFO_RD);

	return (tmp >> 16);
}

/**
 * Description: set rx fifo address of iram.
 * Input:
 *   @fifo_base: Need to set rx fifo address of the FIFO, the base
 *              address of the FIFO.
 *   @addr_l: low 32 bit.
 *   @addr_h: high 8 bit.
 * return:
 *   0: update rx fifo address of iram successfully.
 *   -EINVAL: update rx fifo address of iram failed.
 * Note:
 */
static int ipa_phy_set_rx_fifo_addr(void __iomem *fifo_base,
				    u32 addr_l, u32 addr_h)
{
	u32 tmp_l, tmp_h;

	writel_relaxed(addr_l, fifo_base + IPA_COMMON_RX_FIFO_ADDRL);
	writel_relaxed(addr_h, fifo_base + IPA_COMMON_RX_FIFO_ADDRH);

	tmp_l = readl_relaxed(fifo_base + IPA_COMMON_RX_FIFO_ADDRL);
	tmp_h = readl_relaxed(fifo_base + IPA_COMMON_RX_FIFO_ADDRH);

	if (tmp_l != addr_l || tmp_h != addr_h)
		return -EIO;

	return 0;
}

/**
 * Description: set tx fifo address of iram.
 * Input:
 *   @fifo_base: Need to set tx fifo address of the FIFO, the base
 *              address of the FIFO.
 *   @addr_l: low 32 bit.
 *   @addr_h: high 8 bit.
 * return:
 *   0: update tx fifo address of iram successfully.
 *   -EINVAL: update tx fifo address of iram failed.
 * Note:
 */
static int ipa_phy_set_tx_fifo_addr(void __iomem *fifo_base,
				    u32 addr_l, u32 addr_h)
{
	u32 tmp_l, tmp_h;

	writel_relaxed(addr_l, fifo_base + IPA_COMMON_TX_FIFO_ADDRL);
	writel_relaxed(addr_h, fifo_base + IPA_COMMON_TX_FIFO_ADDRH);

	tmp_l = readl_relaxed(fifo_base + IPA_COMMON_TX_FIFO_ADDRL);
	tmp_h = readl_relaxed(fifo_base + IPA_COMMON_TX_FIFO_ADDRH);

	if (tmp_l != addr_l || tmp_h != addr_h)
		return -EIO;

	return 0;
}

/**
 * Description: Enable interrupt bit.
 * Input:
 *   @fifo_base: Need to enable interrupr bit of the FIFO, the base
 *              address of the FIFO.
 *   @int_bit: The interrupt bit that need to enable.
 * return:
 *   0: Enable successfully.
 *   -EINVAL: Enable successfully.
 * Note:
 */
static int ipa_phy_enable_int_bit(void __iomem *fifo_base,
				  u32 int_bit)
{
	u32 tmp;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_INT_GEN_CTL_EN;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp |= int_bit;
	writel_relaxed(tmp, fifo_reg_addr);

	tmp = readl_relaxed(fifo_reg_addr);
	if ((tmp & int_bit) != int_bit)
		return -EIO;

	return 0;
}

/**
 * Description: Disable interrupt bit.
 * Input:
 *   @fifo_base: Need to Disable interrupr bit of the FIFO, the base
 *              address of the FIFO.
 *   @int_bit: The interrupt bit that need to disable.
 * return:
 *   0: Disable successfully.
 *   -EINVAL: Disable failed.
 * Note:
 */
static int ipa_phy_disable_int_bit(void __iomem *fifo_base,
				   u32 int_bit)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_INT_GEN_CTL_EN;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= (~int_bit);
	writel_relaxed(tmp, fifo_reg_addr);

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= int_bit;

	if (tmp)
		return -EIO;

	return 0;
}

/**
 * Description: Set tx fifo interrupt threshold of value.
 * Input:
 *   @fifo_base: Need to get threshold interrupt value of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   0: set successfully.
 *   -EINVAL: set failed.
 * Note:
 */
static int ipa_phy_set_tx_fifo_intr_thres(void __iomem *fifo_base, u32 thres)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_INT_GEN_CTL_TX_FIFO_VALUE;

	if (thres > 0xFFFF)
		return -EINVAL;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0x0000FFFF;
	tmp |= (thres << 16);
	writel_relaxed(tmp, fifo_reg_addr);
	tmp = readl_relaxed(fifo_reg_addr);

	if ((tmp >> 16) != thres)
		return -EIO;

	return 0;
}

/**
 * Description: Set tx fifo interrupt of delay timer value.
 * Input:
 *   @fifo_base: Need to set delay timer interrupt of the FIFO, the base
 *              address of the FIFO.
 *   @threshold: The overflow value that need to set.
 * return:
 *   0: Set successfully.
 *   -EINVAL: set failed.
 * Note:
 */
static int ipa_phy_set_tx_fifo_timeout(void __iomem *fifo_base, u32 threshold)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_INT_GEN_CTL_TX_FIFO_VALUE;

	if (threshold > 0xFFFF)
		return -EINVAL;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0xFFFF0000;
	tmp |= threshold;
	writel_relaxed(tmp, fifo_reg_addr);
	tmp = readl_relaxed(fifo_reg_addr);

	if ((tmp & 0x0000FFFF) != threshold)
		return -EIO;
	return 0;
}

/**
 * Description: Set current term number.
 * Input:
 *   @fifo_base: Need to set current term number of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   0: Set successfully.
 *   -EINVAL: Set failed.
 * Note:
 */
static int ipa_phy_set_cur_term_num(void __iomem *fifo_base,
				    u32 num)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_PERFETCH_FIFO_CTL;

	if (num > 0x1F)
		return -EINVAL;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0xFFFC1FFF;
	tmp |= (num << 13);
	writel_relaxed(tmp, fifo_reg_addr);

	tmp = readl_relaxed(fifo_reg_addr);
	if (((tmp & 0x0003E000) >> 13) != num)
		return -EIO;

	return 0;
}

/**
 * Description: Set dst term number.
 * Input:
 *   @fifo_base: Need to set dst term number of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   0: Set successfully.
 *   -EINVAL: Set failed.
 * Note:
 */
static int ipa_phy_set_dst_term_num(void __iomem *fifo_base, u32 num)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_PERFETCH_FIFO_CTL;
	if (num > 0x1F)
		return -EINVAL;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0xFFFFE0FF;
	tmp |= (num << 8);
	writel_relaxed(tmp, fifo_reg_addr);

	tmp = readl_relaxed(fifo_reg_addr);
	if (((tmp & 0x00001F00) >> 8) != num)
		return -EIO;

	return 0;
}

/**
 * Description: Set stop receive bit.
 * Input:
 *   @fifo_base: Need to set stop receive bit of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   0: Set successfully.
 *   -EINVAL: Set failed.
 * Note:
 */
static int ipa_phy_stop_receive(void __iomem *fifo_base)
{
	u32 tmp;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_FLOW_CTRL_CONFIG;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp |= 0x8;
	writel_relaxed(tmp, fifo_reg_addr);

	tmp = readl_relaxed(fifo_reg_addr);
	if (!(tmp & 0x8))
		return -EIO;

	return 0;
}

/**
 * Description: Clear stop receive bit.
 * Input:
 *   @fifo_base: Need to clear stop receive bit of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   0: clear successfully.
 *   -EINVAL: clear failed.
 * Note:
 */
static int ipa_phy_clear_stop_receive(void __iomem *fifo_base)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_FLOW_CTRL_CONFIG;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0xFFFFFFF7;
	writel_relaxed(tmp, fifo_reg_addr);
	tmp = readl_relaxed(fifo_reg_addr);

	if (tmp & 0x8)
		return -EIO;
	return 0;
}

/**
 * Description: Set flow ctrl mode.
 * Input:
 *   @fifo_base: Need to set flow ctrl mode of the FIFO, the base
 *              address of the FIFO.
 * return:
 *   0: Set successfully.
 *   -EINVAL: Set failed.
 * Note:
 */
static int ipa_phy_set_flow_ctrl_config(void __iomem *fifo_base,
					u32 config)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_FLOW_CTRL_CONFIG;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0xFFFFFFFC;
	tmp |= config;
	writel_relaxed(tmp, fifo_reg_addr);

	tmp = readl_relaxed(fifo_reg_addr);
	if ((tmp & 0x00000003) != config)
		return -EIO;
	return 0;
}

static void ipa_phy_cmn_fifo_non_stop_on_flowctl(void __iomem *fifo_base,
						 bool status)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_FLOW_CTRL_CONFIG;

	tmp = readl_relaxed(fifo_reg_addr);

	if (status)
		tmp |= IPA_CMN_FIFO_FLOW_CTL_NON_STOP;
	else
		tmp &= ~(IPA_CMN_FIFO_FLOW_CTL_NON_STOP);

	writel_relaxed(tmp, fifo_reg_addr);
}

static void ipa_phy_cmn_fifo_flowctl_recover(void __iomem *fifo_base)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_FLOW_CTRL_CONFIG;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp |= IPA_CMN_FIFO_FLOW_CTL_RECOVER;
	writel_relaxed(tmp, fifo_reg_addr);

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= ~(IPA_CMN_FIFO_FLOW_CTL_RECOVER);
	writel_relaxed(tmp, fifo_reg_addr);
}

static bool ipa_phy_check_cmn_fifo_flowctl(void __iomem *fifo_base)
{
	void __iomem *fifo_reg_addr;
	u32 tmp = 0;

	fifo_reg_addr = fifo_base + IPA_INT_GEN_CTL_EN;

	tmp = readl_relaxed(fifo_reg_addr);

	if (tmp & IPA_INT_ENTER_FLOW_CTRL_STS &&
	    tmp & IPA_INT_EXIT_FLOW_CTRL_STS)
		return true;
	else
		return false;
}

static bool ipa_phy_check_cmn_fifo_enter_flowctl(void __iomem *fifo_base)
{
	void __iomem *fifo_reg_addr;
	u32 tmp = 0;

	fifo_reg_addr = fifo_base + IPA_INT_GEN_CTL_EN;

	tmp = readl_relaxed(fifo_reg_addr);

	if (tmp & IPA_INT_ENTER_FLOW_CTRL_STS)
		return true;

	return false;
}

static bool ipa_phy_check_cmn_fifo_exit_flowctl(void __iomem *fifo_base)
{
	void __iomem *fifo_reg_addr;
	u32 tmp = 0;

	fifo_reg_addr = fifo_base + IPA_INT_GEN_CTL_EN;

	tmp = readl_relaxed(fifo_reg_addr);

	if (tmp & IPA_INT_EXIT_FLOW_CTRL_STS)
		return true;

	return false;
}

static void ipa_phy_clr_cmn_fifo_flowctl_interrupt(void __iomem *fifo_base)
{
	void __iomem *fifo_reg_addr;
	u32 tmp = 0;

	fifo_reg_addr = fifo_base + IPA_INT_GEN_CTL_CLR;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp |= IPA_ENTER_FLOW_CONTROL_CLR_BIT | IPA_EXIT_FLOW_CONTROL_CLR_BIT;
	writel_relaxed(tmp, fifo_reg_addr);
}

static void ipa_phy_clr_cfifo_flowctl_enter_inter(void __iomem *fifo_base)
{
	void __iomem *fifo_reg_addr;
	u32 tmp = 0;

	fifo_reg_addr = fifo_base + IPA_INT_GEN_CTL_CLR;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp |= IPA_ENTER_FLOW_CONTROL_CLR_BIT;
	writel_relaxed(tmp, fifo_reg_addr);
}

static void ipa_phy_clr_cfifo_flowctl_exit_inter(void __iomem *fifo_base)
{
	void __iomem *fifo_reg_addr;
	u32 tmp = 0;

	fifo_reg_addr = fifo_base + IPA_INT_GEN_CTL_CLR;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp |= IPA_EXIT_FLOW_CONTROL_CLR_BIT;
	writel_relaxed(tmp, fifo_reg_addr);
}

/**
 * Description: Set tx fifo exit flow ctrl watermark.
 * Input:
 *   @fifo_base: Need to be set of the FIFO, the base
 *              address of the FIFO.
 *   @watermark: The need to be set.
 * return:
 *   0: Set successfully.
 *   -EINVAL: Set failed.
 * Note:
 */
static int ipa_phy_set_tx_fifo_exit_flow_ctrl_watermark(void __iomem *fifo_base,
							u32 watermark)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_TX_FIFO_FLOW_CTRL;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0x0000FFFF;
	tmp |= (watermark << 16);
	writel_relaxed(tmp, fifo_reg_addr);

	tmp = readl_relaxed(fifo_reg_addr);
	if ((tmp >> 16) != watermark)
		return -EIO;
	return 0;
}

/**
 * Description: Set tx fifo entry flow ctrl watermark.
 * Input:
 *   @fifo_base: Need to be set of the FIFO, the base
 *              address of the FIFO.
 *   @watermark: The need to be set.
 * return:
 *   0: Set successfully.
 *   -EINVAL: Set failed.
 * Note:
 */
static int ipa_phy_set_tx_fifo_entry_flow_ctrl_watermark(void __iomem *base,
							 u32 watermark)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = base + IPA_TX_FIFO_FLOW_CTRL;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0xFFFF0000;
	tmp |= watermark;
	writel_relaxed(tmp, fifo_reg_addr);
	tmp = readl_relaxed(fifo_reg_addr);

	if ((tmp & 0x0000FFFF) != watermark)
		return -EIO;
	return 0;
}

/**
 * Description: Set rx fifo exit flow ctrl watermark.
 * Input:
 *   @fifo_base: Need to be set of the FIFO, the base
 *              address of the FIFO.
 *   @watermark: The value of rx fifo exit watermark.
 * return:
 *   0: Set successfully.
 *   -EINVAL: Set failed.
 * Note:
 */
static int ipa_phy_set_rx_fifo_exit_flow_ctrl_watermark(void __iomem *fifo_base,
							u32 watermark)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = fifo_base + IPA_RX_FIFO_FLOW_CTRL;

	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0x0000FFFF;
	tmp |= (watermark << 16);
	writel_relaxed(tmp, fifo_reg_addr);
	tmp = readl_relaxed(fifo_reg_addr);

	if ((tmp >> 16) != watermark)
		return -EIO;
	return 0;
}

/**
 * Description: Set rx fifo entry flow ctrl watermark.
 * Input:
 *   @fifo_base: Need to be set of the FIFO, the base
 *              address of the FIFO.
 *   @watermark: The value of rx fifo entry watermark.
 * return:
 *   0: Set successfully.
 *   -EINVAL: Set failed.
 * Note:
 */
static u32 ipa_phy_set_rx_fifo_entry_flow_ctrl_watermark(void __iomem *base,
							 u32 watermark)
{
	u32 tmp = 0;
	void __iomem *fifo_reg_addr;

	fifo_reg_addr = base + IPA_RX_FIFO_FLOW_CTRL;
	tmp = readl_relaxed(fifo_reg_addr);
	tmp &= 0xFFFF0000;
	tmp |= watermark;
	writel_relaxed(tmp, fifo_reg_addr);
	tmp = readl_relaxed(fifo_reg_addr);

	if ((tmp & 0x0000FFFF) != watermark)
		return -EIO;
	return 0;
}

static int ipa_put_pkt_to_rx_fifo(struct sipa_cmn_fifo_cfg_tag *fifo_cfg,
				  struct sipa_node_desc_tag *fill_node,
				  u32 num)
{
	u32 tmp, tmp1, index = 0, left_cnt = 0;
	struct sipa_node_desc_tag *node;

	if (!fill_node) {
		pr_err("sipa fill node is NULL\n");
		return -EINVAL;
	}

	node = (struct sipa_node_desc_tag *)fifo_cfg->rx_fifo.virt_addr;

	if (ipa_phy_get_rx_fifo_full_status(fifo_cfg->fifo_reg_base))
		return -ENOSPC;

	left_cnt = ipa_phy_get_rx_fifo_total_depth(fifo_cfg->fifo_reg_base) -
		ipa_phy_get_rx_fifo_filled_depth(fifo_cfg->fifo_reg_base);

	if (left_cnt < num)
		num = left_cnt;

	index = fifo_cfg->rx_fifo.wr & (fifo_cfg->rx_fifo.depth - 1);
	if (index + num <= fifo_cfg->rx_fifo.depth) {
		memcpy(node + index, fill_node, sizeof(*node) * num);
	} else {
		tmp = fifo_cfg->rx_fifo.depth - index;
		memcpy(node + index, fill_node, sizeof(*node) * tmp);
		tmp1 = num - tmp;
		memcpy(node, fill_node + tmp, sizeof(*node) * tmp1);
	}

	/* Ensure that data copy completion before we update rptr/wptr */
	smp_wmb();
	fifo_cfg->rx_fifo.wr = (fifo_cfg->rx_fifo.wr + num) &
		PTR_MASK(fifo_cfg->rx_fifo.depth);
	if (ipa_phy_update_rx_fifo_wptr(fifo_cfg->fifo_reg_base,
					fifo_cfg->rx_fifo.wr))
		pr_err("sipa_phy_update_rx_fifo_wptr fail\n");

	return num;
}

static int ipa_recv_pkt_from_tx_fifo(struct sipa_cmn_fifo_cfg_tag *fifo_cfg,
				     struct sipa_node_desc_tag *recv_node,
				     u32 num)
{
	struct sipa_node_desc_tag *node;
	u32 tmp = 0, tmp1, index = 0, left_cnt = 0;

	if (!recv_node) {
		pr_err("sipa fill node is NULL\n");
		return -EINVAL;
	}

	node = (struct sipa_node_desc_tag *)fifo_cfg->tx_fifo.virt_addr;

	if (ipa_phy_get_tx_fifo_empty_status(fifo_cfg->fifo_reg_base))
		return 0;

	left_cnt = ipa_phy_get_tx_fifo_filled_depth(fifo_cfg->fifo_reg_base);
	if (left_cnt < num) {
		pr_info("sipa fifo_id = %d only have %d nodes\n",
			fifo_cfg->fifo_id, left_cnt);
		num = left_cnt;
	}

	index = fifo_cfg->tx_fifo.rd & (fifo_cfg->tx_fifo.depth - 1);
	if (index + num <= fifo_cfg->tx_fifo.depth) {
		memcpy(recv_node, node + index, sizeof(*node) * num);
	} else {
		tmp = fifo_cfg->tx_fifo.depth - index;
		memcpy(recv_node, node + index, sizeof(*node) * tmp);
		tmp1 = num - tmp;
		memcpy(recv_node + tmp, node, sizeof(*node) * tmp1);
	}

	/* Ensure that data copy completion before we update rptr/wptr */
	smp_wmb();
	fifo_cfg->tx_fifo.rd = (fifo_cfg->tx_fifo.rd + num) &
		PTR_MASK(fifo_cfg->tx_fifo.depth);
	if (ipa_phy_update_tx_fifo_rptr(fifo_cfg->fifo_reg_base,
					fifo_cfg->tx_fifo.rd))
		pr_err("update tx fifo rptr fail !!!\n");

	return num;
}

static int ipa_sync_pkt_from_tx_fifo(struct device *dev,
				     struct sipa_cmn_fifo_cfg_tag *fifo_cfg,
				     u32 num)
{
	dma_addr_t dma_addr;
	u32 tmp = 0, index = 0, depth;
	ssize_t node_size = sizeof(struct sipa_node_desc_tag);

	dma_addr = IPA_STI_64BIT(fifo_cfg->tx_fifo.fifo_base_addr_l,
				 fifo_cfg->tx_fifo.fifo_base_addr_h);
	depth = ipa_phy_get_tx_fifo_filled_depth(fifo_cfg->fifo_reg_base);
	if (depth < num)
		num = depth;

	index = fifo_cfg->tx_fifo.rd & (fifo_cfg->tx_fifo.depth - 1);
	if (index + num <= fifo_cfg->tx_fifo.depth) {
		dma_sync_single_for_cpu(dev, dma_addr + index * node_size,
					num * node_size, DMA_FROM_DEVICE);
	} else {
		tmp = fifo_cfg->tx_fifo.depth - index;
		dma_sync_single_for_cpu(dev, dma_addr + index * node_size,
					tmp * node_size, DMA_FROM_DEVICE);
		tmp = num - tmp;
		dma_sync_single_for_cpu(dev, dma_addr,
					tmp * node_size, DMA_FROM_DEVICE);
	}

	return num;
}

static int ipa_sync_pkt_to_rx_fifo(struct device *dev,
				   struct sipa_cmn_fifo_cfg_tag *fifo_cfg,
				   u32 num)
{
	dma_addr_t dma_addr;
	u32 tmp = 0, index = 0;
	ssize_t node_size = sizeof(struct sipa_node_desc_tag);

	dma_addr = IPA_STI_64BIT(fifo_cfg->rx_fifo.fifo_base_addr_l,
				 fifo_cfg->rx_fifo.fifo_base_addr_h);

	index = fifo_cfg->rx_fifo.wr & (fifo_cfg->rx_fifo.depth - 1);
	if (index + num <= fifo_cfg->rx_fifo.depth) {
		dma_sync_single_for_device(dev, dma_addr + index * node_size,
					   num * node_size, DMA_TO_DEVICE);
	} else {
		tmp = fifo_cfg->rx_fifo.depth - index;
		dma_sync_single_for_device(dev, dma_addr + index * node_size,
					   tmp * node_size, DMA_TO_DEVICE);
		tmp = num - tmp;
		dma_sync_single_for_device(dev, dma_addr,
					   tmp * node_size, DMA_TO_DEVICE);
	}

	return num;
}

static int ipa_put_pkt_to_tx_fifo(struct sipa_cmn_fifo_cfg_tag *fifo_cfg,
				  struct sipa_node_desc_tag *fill_node, u32 num)
{
	u32 left_cnt = 0, tmp, tmp1, index = 0;
	struct sipa_node_desc_tag *node;

	node = (struct sipa_node_desc_tag *)fifo_cfg->tx_fifo.virt_addr;

	if (ipa_phy_get_tx_fifo_full_status(fifo_cfg->fifo_reg_base))
		return -ENOSPC;

	left_cnt = ipa_phy_get_tx_fifo_total_depth(fifo_cfg->fifo_reg_base) -
		ipa_phy_get_tx_fifo_filled_depth(fifo_cfg->fifo_reg_base);

	if (num > left_cnt) {
		pr_info("fifo_id = %d don't have enough space\n",
			fifo_cfg->fifo_id);
		num = left_cnt;
	}

	index = fifo_cfg->tx_fifo.wr & (fifo_cfg->tx_fifo.depth - 1);
	if (index + num <= fifo_cfg->tx_fifo.depth) {
		memcpy(node + index, fill_node, sizeof(*node) * num);
	} else {
		tmp = fifo_cfg->tx_fifo.depth - index;
		memcpy(node + index, fill_node, sizeof(*node) * tmp);
		tmp1 = num - tmp;
		memcpy(node, fill_node + tmp, sizeof(*node) * tmp1);
	}

	/* Ensure that data copy completion before we update rptr/wptr */
	smp_wmb();
	fifo_cfg->tx_fifo.wr = (fifo_cfg->tx_fifo.wr + num) &
		PTR_MASK(fifo_cfg->tx_fifo.depth);
	if (ipa_phy_update_tx_fifo_wptr(fifo_cfg->fifo_reg_base,
					fifo_cfg->tx_fifo.wr))
		pr_err("update tx fifo rptr fail !!!\n");

	return num;
}

static int ipa_relm_tx_fifo_unread_free_node(struct sipa_cmn_fifo_cfg_tag *fifo_cfg,
					     u32 tx_rptr, u32 tx_wptr,
					     u32 rx_rptr, u32 rx_wptr)
{
	u32 depth;

	depth = (tx_wptr - tx_rptr) & (fifo_cfg->rx_fifo.depth - 1);

	/* Ensure that data copy completion before we update rptr/wptr */
	smp_wmb();

	tx_rptr = tx_wptr;
	if (ipa_phy_update_tx_fifo_rptr(fifo_cfg->fifo_reg_base,
					tx_rptr)) {
		pr_err("sipa reclaim update tx_fifo free node rptr failed\n");
		return -EIO;
	}

	return 0;
}

static int ipa_relm_tx_fifo_unread_node(struct sipa_cmn_fifo_cfg_tag *fifo_cfg,
					u32 tx_rptr, u32 tx_wptr,
					u32 rx_rptr,  u32 rx_wptr)
{
	u32 index, tmp, depth;
	ssize_t node_size = sizeof(struct sipa_node_desc_tag);
	struct sipa_node_desc_tag *fill_node, *free_node;

	fill_node = fifo_cfg->tx_fifo.virt_addr;
	free_node = fifo_cfg->rx_fifo.virt_addr;
	depth = (tx_wptr - rx_wptr) & (fifo_cfg->rx_fifo.depth - 1);

	index = rx_wptr & (fifo_cfg->tx_fifo.depth - 1);
	if (!depth) {
		memcpy(free_node, fill_node,
		       node_size * fifo_cfg->rx_fifo.depth);
		depth = fifo_cfg->rx_fifo.depth;
	} else if (index + depth <= fifo_cfg->rx_fifo.depth) {
		memcpy(free_node + index, fill_node + index,
		       node_size * depth);
	} else {
		tmp = fifo_cfg->rx_fifo.depth - index;
		memcpy(free_node + index, fill_node + index,
		       node_size * tmp);
		tmp = depth - tmp;
		memcpy(free_node, fill_node, node_size * tmp);
	}

	/* Ensure that data copy completion before we update rptr/wptr */
	smp_wmb();

	tx_rptr = tx_wptr;
	if (ipa_phy_update_tx_fifo_rptr(fifo_cfg->fifo_reg_base,
					tx_rptr)) {
		pr_err("sipa reclaim update tx_fifo rptr failed\n");
		return -EIO;
	}
	rx_wptr = (rx_wptr + depth) & PTR_MASK(fifo_cfg->rx_fifo.depth);
	if (ipa_phy_update_rx_fifo_wptr(fifo_cfg->fifo_reg_base,
					rx_wptr)) {
		pr_err("sipa reclaim update rx_fifo wptr failed\n");
		return -EIO;
	}

	return 0;
}

static int ipa_relm_unfree_node(struct sipa_cmn_fifo_cfg_tag *fifo_cfg,
				u32 tx_rptr, u32 tx_wptr,
				u32 rx_rptr, u32 rx_wptr)
{
	u32 index, tmp, num;
	ssize_t node_size = sizeof(struct sipa_node_desc_tag);
	struct sipa_node_desc_tag *fill_node, *free_node;

	fill_node = fifo_cfg->tx_fifo.virt_addr;
	free_node = fifo_cfg->rx_fifo.virt_addr;
	num = (tx_wptr - rx_wptr) & (fifo_cfg->rx_fifo.depth - 1);

	index = rx_wptr & (fifo_cfg->rx_fifo.depth - 1);
	if (!num) {
		memcpy(free_node, fill_node,
		       node_size * fifo_cfg->rx_fifo.depth);
		num = fifo_cfg->rx_fifo.depth;
	} else if (index + num <= fifo_cfg->rx_fifo.depth) {
		memcpy(free_node + index, fill_node + index,
		       node_size * num);
	} else {
		tmp = fifo_cfg->rx_fifo.depth - index;
		memcpy(free_node + index, fill_node + index,
		       node_size * tmp);
		tmp = num - tmp;
		memcpy(free_node, fill_node, node_size * tmp);
	}

	/* Ensure that data copy completion before we update rptr/wptr */
	smp_wmb();

	rx_wptr = (rx_wptr + num) & PTR_MASK(fifo_cfg->rx_fifo.depth);
	if (ipa_phy_update_rx_fifo_wptr(fifo_cfg->fifo_reg_base,
					rx_wptr)) {
		pr_err("sipa reclaim unfree update rx_fifo wptr failed\n");
		return -EIO;
	}

	return 0;
}

static int ipa_cmn_fifo_phy_open(enum sipa_cmn_fifo_index id,
				 struct sipa_cmn_fifo_cfg_tag *cfg_base,
				 void *cookie)
{
	struct sipa_cmn_fifo_cfg_tag *ipa_term_fifo = NULL;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have this id %d\n", id);
		return -EINVAL;
	}
	ipa_term_fifo = cfg_base + id;

	if (ipa_term_fifo->state) {
		pr_err("sipa fifo_id = %d has already opened\n",
		       ipa_term_fifo->fifo_id);
		return 0;
	}

	ipa_phy_set_rx_fifo_total_depth(ipa_term_fifo->fifo_reg_base,
					ipa_term_fifo->rx_fifo.depth);
	ipa_phy_set_rx_fifo_addr(ipa_term_fifo->fifo_reg_base,
				 ipa_term_fifo->rx_fifo.fifo_base_addr_l,
				 ipa_term_fifo->rx_fifo.fifo_base_addr_h);

	ipa_phy_set_tx_fifo_total_depth(ipa_term_fifo->fifo_reg_base,
					ipa_term_fifo->tx_fifo.depth);
	ipa_phy_set_tx_fifo_addr(ipa_term_fifo->fifo_reg_base,
				 ipa_term_fifo->tx_fifo.fifo_base_addr_l,
				 ipa_term_fifo->tx_fifo.fifo_base_addr_h);

	ipa_phy_set_cur_term_num(ipa_term_fifo->fifo_reg_base,
				 ipa_term_fifo->src);
	ipa_phy_set_dst_term_num(ipa_term_fifo->fifo_reg_base,
				 ipa_term_fifo->dst);

	ipa_phy_update_rx_fifo_rptr(ipa_term_fifo->fifo_reg_base, 0);
	ipa_phy_update_rx_fifo_wptr(ipa_term_fifo->fifo_reg_base, 0);
	ipa_phy_update_tx_fifo_rptr(ipa_term_fifo->fifo_reg_base, 0);
	ipa_phy_update_tx_fifo_wptr(ipa_term_fifo->fifo_reg_base, 0);

	ipa_phy_clear_stop_receive(ipa_term_fifo->fifo_reg_base);

	ipa_term_fifo->rx_fifo.rd = 0;
	ipa_term_fifo->rx_fifo.wr = 0;
	ipa_term_fifo->tx_fifo.rd = 0;
	ipa_term_fifo->tx_fifo.wr = 0;

	/* the parameter of cookie is not useful */
	cookie = NULL;
	ipa_term_fifo->state = true;

	return 0;
}

static int ipa_cmn_fifo_phy_close(enum sipa_cmn_fifo_index id,
				  struct sipa_cmn_fifo_cfg_tag *cfg_base)
{
	struct sipa_cmn_fifo_cfg_tag *ipa_term_fifo;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have this id %d\n", id);
		return -EINVAL;
	}
	ipa_term_fifo = cfg_base + id;

	ipa_phy_set_rx_fifo_total_depth(ipa_term_fifo->fifo_reg_base, 0);
	ipa_phy_set_tx_fifo_total_depth(ipa_term_fifo->fifo_reg_base, 0);

	ipa_phy_update_rx_fifo_rptr(ipa_term_fifo->fifo_reg_base, 0);
	ipa_phy_update_rx_fifo_wptr(ipa_term_fifo->fifo_reg_base, 0);
	ipa_phy_update_tx_fifo_rptr(ipa_term_fifo->fifo_reg_base, 0);
	ipa_phy_update_tx_fifo_wptr(ipa_term_fifo->fifo_reg_base, 0);

	ipa_phy_stop_receive(ipa_term_fifo->fifo_reg_base);

	ipa_term_fifo->rx_fifo.rd = 0;
	ipa_term_fifo->rx_fifo.wr = 0;
	ipa_term_fifo->tx_fifo.rd = 0;
	ipa_term_fifo->tx_fifo.wr = 0;

	ipa_term_fifo->state = false;

	return 0;
}

static int ipa_cmn_fifo_phy_reset_fifo(enum sipa_cmn_fifo_index id,
				       struct sipa_cmn_fifo_cfg_tag *cfg_base)
{
	struct sipa_cmn_fifo_cfg_tag *ipa_term_fifo;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have this id %d\n", id);
		return -EINVAL;
	}
	ipa_term_fifo = cfg_base + id;

	ipa_phy_set_rx_fifo_total_depth(ipa_term_fifo->fifo_reg_base,
					ipa_term_fifo->rx_fifo.depth);
	ipa_phy_set_rx_fifo_addr(ipa_term_fifo->fifo_reg_base,
				 ipa_term_fifo->rx_fifo.fifo_base_addr_l,
				 ipa_term_fifo->rx_fifo.fifo_base_addr_h);

	ipa_phy_set_tx_fifo_total_depth(ipa_term_fifo->fifo_reg_base,
					ipa_term_fifo->tx_fifo.depth);
	ipa_phy_set_tx_fifo_addr(ipa_term_fifo->fifo_reg_base,
				 ipa_term_fifo->tx_fifo.fifo_base_addr_l,
				 ipa_term_fifo->tx_fifo.fifo_base_addr_h);

	ipa_phy_update_rx_fifo_rptr(ipa_term_fifo->fifo_reg_base, 0);
	ipa_phy_update_rx_fifo_wptr(ipa_term_fifo->fifo_reg_base, 0);

	ipa_phy_update_tx_fifo_rptr(ipa_term_fifo->fifo_reg_base, 0);
	ipa_phy_update_tx_fifo_wptr(ipa_term_fifo->fifo_reg_base, 0);
	ipa_term_fifo->rx_fifo.rd = 0;
	ipa_term_fifo->rx_fifo.wr = 0;

	ipa_term_fifo->tx_fifo.rd = 0;
	ipa_term_fifo->tx_fifo.wr = 0;

	ipa_term_fifo->state = true;

	return 0;
}

static int ipa_cmn_fifo_phy_set_rx_depth(enum sipa_cmn_fifo_index id,
					 struct sipa_cmn_fifo_cfg_tag *cfg_base,
					 u32 depth)
{
	struct sipa_cmn_fifo_cfg_tag *ipa_term_fifo;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have this id %d\n", id);
		return -EINVAL;
	}
	ipa_term_fifo = cfg_base + id;

	return ipa_phy_set_rx_fifo_total_depth(ipa_term_fifo->fifo_reg_base,
					       depth);
}

static int ipa_cmn_fifo_phy_get_rx_depth(enum sipa_cmn_fifo_index id,
					 struct sipa_cmn_fifo_cfg_tag *cfg_base)
{
	struct sipa_cmn_fifo_cfg_tag *ipa_term_fifo;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have this id %d\n", id);
		return -EINVAL;
	}
	ipa_term_fifo = cfg_base + id;

	return ipa_phy_get_rx_fifo_total_depth(ipa_term_fifo->fifo_reg_base);
}

static int ipa_cmn_fifo_phy_set_tx_depth(enum sipa_cmn_fifo_index id,
					 struct sipa_cmn_fifo_cfg_tag *cfg_base,
					 u32 depth)
{
	struct sipa_cmn_fifo_cfg_tag *ipa_term_fifo;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have this id %d\n", id);
		return -EINVAL;
	}
	ipa_term_fifo = cfg_base + id;

	return ipa_phy_set_tx_fifo_total_depth(ipa_term_fifo->fifo_reg_base,
					       depth);
}

static int ipa_cmn_fifo_phy_get_tx_depth(enum sipa_cmn_fifo_index id,
					 struct sipa_cmn_fifo_cfg_tag *cfg_base)
{
	struct sipa_cmn_fifo_cfg_tag *ipa_term_fifo;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have this id %d\n", id);
		return -EINVAL;
	}
	ipa_term_fifo = cfg_base + id;

	return ipa_phy_get_tx_fifo_total_depth(ipa_term_fifo->fifo_reg_base);
}

static int ipa_cmn_fifo_phy_set_intr_errcode(enum sipa_cmn_fifo_index id,
					     struct sipa_cmn_fifo_cfg_tag *base,
					     bool enable)
{
	struct sipa_cmn_fifo_cfg_tag *ipa_term_fifo;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have this id %d\n", id);
		return -EINVAL;
	}
	ipa_term_fifo = base + id;

	if (enable)
		return ipa_phy_enable_int_bit(ipa_term_fifo->fifo_reg_base,
					      IPA_ERRORCODE_IN_TX_FIFO_EN);
	else
		return ipa_phy_disable_int_bit(ipa_term_fifo->fifo_reg_base,
					       IPA_ERRORCODE_IN_TX_FIFO_EN);
}

static int ipa_cmn_fifo_phy_set_intr_timeout(enum sipa_cmn_fifo_index id,
					     struct sipa_cmn_fifo_cfg_tag *base,
					     bool enable, u32 time)
{
	int ret;
	struct sipa_cmn_fifo_cfg_tag *ipa_term_fifo;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have this id %d\n", id);
		return -EINVAL;
	}
	ipa_term_fifo = base + id;

	if (enable) {
		ret = ipa_phy_set_tx_fifo_timeout(ipa_term_fifo->fifo_reg_base,
						  time);
		if (ret) {
			pr_err("sipa fifo %d set timeout threshold fail\n", id);
			return ret;
		}

		ret = ipa_phy_enable_int_bit(ipa_term_fifo->fifo_reg_base,
					     IPA_TXFIFO_INT_DELAY_TIMER_SW_EN);
	} else {
		ret = ipa_phy_disable_int_bit(ipa_term_fifo->fifo_reg_base,
					      IPA_TXFIFO_INT_DELAY_TIMER_SW_EN);
	}

	return ret;
}

static int ipa_cmn_fifo_phy_set_hw_intr_timeout(enum sipa_cmn_fifo_index id,
						struct sipa_cmn_fifo_cfg_tag *b,
						bool enable, u32 time)
{
	int ret;
	struct sipa_cmn_fifo_cfg_tag *ipa_term_fifo;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have this id %d\n", id);
		return -EINVAL;
	}
	ipa_term_fifo = b + id;

	if (enable) {
		ret = ipa_phy_set_tx_fifo_timeout(ipa_term_fifo->fifo_reg_base,
						  time);
		if (ret) {
			pr_err("sipa fifo %d set timeout threshold fail\n", id);
			return ret;
		}

		ret = ipa_phy_enable_int_bit(ipa_term_fifo->fifo_reg_base,
					     IPA_TX_FIFO_DELAY_TIMER_EN);
	} else {
		ret = ipa_phy_disable_int_bit(ipa_term_fifo->fifo_reg_base,
					      IPA_TX_FIFO_DELAY_TIMER_EN);
	}

	return ret;
}

static int ipa_cmn_fifo_phy_set_intr_thres(enum sipa_cmn_fifo_index id,
					   struct sipa_cmn_fifo_cfg_tag *b,
					   bool enable, u32 cnt)
{
	struct sipa_cmn_fifo_cfg_tag *fifo;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have this id %d\n", id);
		return -EINVAL;
	}
	fifo = b + id;

	if (enable) {
		if (ipa_phy_set_tx_fifo_intr_thres(fifo->fifo_reg_base, cnt)) {
			pr_err("sipa fifo %d set threshold fail\n", id);
			return -EINVAL;
		}

		if (ipa_phy_enable_int_bit(fifo->fifo_reg_base,
					   IPA_TXFIFO_INT_THRES_ONESHOT_EN)) {
			pr_err("sipa fifo %d enable intr thres err", id);
			return -EINVAL;
		}
	} else {
		if (ipa_phy_disable_int_bit(fifo->fifo_reg_base,
					    IPA_TXFIFO_INT_THRES_ONESHOT_EN)) {
			pr_err("sipa fifo %d disable intr thres err", id);
			return -EINVAL;
		}
	}

	return 0;
}

static int ipa_cmn_fifo_phy_set_hw_intr_thres(enum sipa_cmn_fifo_index id,
					      struct sipa_cmn_fifo_cfg_tag *b,
					      u32 enable, u32 cnt)
{
	int ret = 0;
	struct sipa_cmn_fifo_cfg_tag *fifo;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have this id %d\n", id);
		return -EINVAL;
	}
	fifo = b + id;

	if (enable) {
		ret = ipa_phy_set_tx_fifo_intr_thres(fifo->fifo_reg_base, cnt);
		if (ret) {
			pr_err("sipa fifo %d set threshold fail\n", id);
			return -EINVAL;
		}

		ret = ipa_phy_enable_int_bit(fifo->fifo_reg_base,
					     IPA_TX_FIFO_THRESHOLD_EN);
		if (ret) {
			pr_err("sipa fifo %d enable hw threshold err\n", id);
			return -EINVAL;
		}
	} else {
		ret = ipa_phy_disable_int_bit(fifo->fifo_reg_base,
					      IPA_TX_FIFO_THRESHOLD_EN);
		if (ret) {
			pr_err("sipa fifo %d disable hw threshold err\n", id);
			return -EINVAL;
		}
	}

	return 0;
}

static int ipa_cmn_fifo_phy_set_src_dst_term(enum sipa_cmn_fifo_index id,
					     struct sipa_cmn_fifo_cfg_tag *b,
					     u32 src, u32 dst)
{
	struct sipa_cmn_fifo_cfg_tag *ipa_term_fifo;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have this id %d\n", id);
		return -EINVAL;
	}
	ipa_term_fifo = b + id;

	if (ipa_phy_set_cur_term_num(ipa_term_fifo->fifo_reg_base, src)) {
		pr_err("sipa fifo %d set cur err\n", id);
		return -EINVAL;
	}

	if (ipa_phy_set_dst_term_num(ipa_term_fifo->fifo_reg_base, dst)) {
		pr_err("sipa fifo %d set dst err\n", id);
		return -EINVAL;
	}

	return 0;
}

static int ipa_cmn_fifo_phy_enable_flowctrl_irq(enum sipa_cmn_fifo_index id,
						struct sipa_cmn_fifo_cfg_tag *b,
						u32 enable, u32 irq_mode)
{
	u32 irq;
	struct sipa_cmn_fifo_cfg_tag *ipa_term_fifo;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have this id %d\n", id);
		return -EINVAL;
	}
	ipa_term_fifo = b + id;

	switch (irq_mode) {
	case 0:
		irq = IPA_RX_FIFO_INT_EXIT_FLOW_CTRL_EN;
		break;
	case 1:
		irq = IPA_RX_FIFO_INT_ENTER_FLOW_CTRL_EN;
		break;
	case 2:
		irq = IPA_RX_FIFO_INT_EXIT_FLOW_CTRL_EN |
			  IPA_RX_FIFO_INT_ENTER_FLOW_CTRL_EN;
		break;
	default:
		pr_err("sipa don't have this %d irq type\n", irq_mode);
		return -EINVAL;
	}

	if (enable) {
		if (ipa_phy_enable_int_bit(ipa_term_fifo->fifo_reg_base, irq)) {
			pr_err("sipa fifo_id = %d irq_mode = %d en failed\n",
			       id, irq);
			return -EIO;
		}
	} else {
		if (ipa_phy_disable_int_bit(ipa_term_fifo->fifo_reg_base,
					    irq)){
			pr_err("sipa fifo_id = %d irq_mode = %d disa failed\n",
			       id, irq);
			return -EIO;
		}
	}

	return 0;
}

static int ipa_cmn_fifo_phy_set_flowctrl_mode(enum sipa_cmn_fifo_index id,
					      struct sipa_cmn_fifo_cfg_tag *b,
					      u32 work_mode,
					      u32 tx_entry_watermark,
					      u32 tx_exit_watermark,
					      u32 rx_entry_watermark,
					      u32 rx_exit_watermark)
{
	struct sipa_cmn_fifo_cfg_tag *fifo;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have this id %d\n", id);
		return -EINVAL;
	}
	fifo = b + id;

	if (ipa_phy_set_tx_fifo_exit_flow_ctrl_watermark(fifo->fifo_reg_base,
							 tx_exit_watermark)) {
		pr_err("fifo_id = %d tx_exit_watermark(0x%x) failed\n",
		       id, tx_exit_watermark);
		return -EIO;
	}

	if (ipa_phy_set_tx_fifo_entry_flow_ctrl_watermark(fifo->fifo_reg_base,
							  tx_entry_watermark)) {
		pr_err("fifo_id = %d tx_entry_watermark(0x%x) failed\n",
		       id, tx_entry_watermark);
		return -EIO;
	}

	if (ipa_phy_set_rx_fifo_exit_flow_ctrl_watermark(fifo->fifo_reg_base,
							 rx_exit_watermark)) {
		pr_err("fifo_id = %d rx_exit_watermark(0x%x) failed\n",
		       id, rx_exit_watermark);
		return -EIO;
	}

	if (ipa_phy_set_rx_fifo_entry_flow_ctrl_watermark(fifo->fifo_reg_base,
							  rx_entry_watermark)) {
		pr_err("fifo_id = %d rx_entry_watermark(0x%x) failed\n",
		       id, rx_entry_watermark);
		return -EIO;
	}

	if (ipa_phy_set_flow_ctrl_config(fifo->fifo_reg_base, work_mode)) {
		pr_err("sipa fifo_id = %d set flow ctrl mode = 0x%x failed\n",
		       id, work_mode);
		return -EIO;
	}

	return 0;
}

static int ipa_cmn_fifo_phy_set_node_intr(enum sipa_cmn_fifo_index id,
					  struct sipa_cmn_fifo_cfg_tag *b,
					  u32 enable)
{
	struct sipa_cmn_fifo_cfg_tag *ipa_term_fifo;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have this id %d\n", id);
		return -EINVAL;
	}
	ipa_term_fifo = b + id;

	if (enable) {
		if (ipa_phy_enable_int_bit(ipa_term_fifo->fifo_reg_base,
					   IPA_TX_FIFO_INTR_SW_BIT_EN)) {
			pr_err("sipa enable node intr sw fail\n");
			return -EIO;
		}
	} else {
		if (ipa_phy_disable_int_bit(ipa_term_fifo->fifo_reg_base,
					    IPA_TX_FIFO_INTR_SW_BIT_EN)) {
			pr_err("sipa enable node intr sw fail\n");
			return -EIO;
		}
	}

	return 0;
}

static int ipa_cmn_fifo_phy_set_overflow_intr(enum sipa_cmn_fifo_index id,
					      struct sipa_cmn_fifo_cfg_tag *b,
					      u32 enable)
{
	struct sipa_cmn_fifo_cfg_tag *ipa_term_fifo;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have this id %d\n", id);
		return -EINVAL;
	}
	ipa_term_fifo = b + id;

	if (enable) {
		if (ipa_phy_enable_int_bit(ipa_term_fifo->fifo_reg_base,
					   IPA_TXFIFO_OVERFLOW_EN)) {
			pr_err("sipa enable overflow intr err\n");
			return -EIO;
		}
	} else {
		if (ipa_phy_disable_int_bit(ipa_term_fifo->fifo_reg_base,
					    IPA_TXFIFO_OVERFLOW_EN)) {
			pr_err("sipa disable overflow intr err\n");
			return -EIO;
		}
	}

	return 0;
}

static int ipa_cmn_fifo_phy_set_tx_full_intr(enum sipa_cmn_fifo_index id,
					     struct sipa_cmn_fifo_cfg_tag *b,
					     u32 enable)
{
	struct sipa_cmn_fifo_cfg_tag *ipa_term_fifo;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have this id %d\n", id);
		return -EINVAL;
	}
	ipa_term_fifo = b + id;

	if (enable) {
		if (ipa_phy_enable_int_bit(ipa_term_fifo->fifo_reg_base,
					   IPA_TXFIFO_FULL_INT_EN)) {
			pr_err("sipa enable tx full intr err\n");
			return -EIO;
		}
	} else {
		if (ipa_phy_disable_int_bit(ipa_term_fifo->fifo_reg_base,
					    IPA_TXFIFO_FULL_INT_EN)) {
			pr_err("sipa disable tx full intr err\n");
			return -EIO;
		}
	}

	return 0;
}

static int
ipa_cmn_fifo_phy_restore_irq_map_in(struct sipa_cmn_fifo_cfg_tag *base)
{
	return 0;
}

static int
ipa_cmn_fifo_phy_restore_irq_map_out(struct sipa_cmn_fifo_cfg_tag *base)
{
	return 0;
}

/**
 * Description: Receive Node from tx fifo.
 * Input:
 *   @id: The FIFO id that need to be operated.
 *   @pkt: The node that need to be stored address.
 *   @num: The num of receive.
 * OUTPUT:
 *   @The num that has be received from tx fifo successful.
 * Note:
 */
static int ipa_cmn_fifo_phy_put_node_to_rx_fifo(struct device *dev,
						enum sipa_cmn_fifo_index id,
						struct sipa_cmn_fifo_cfg_tag *b,
						struct sipa_node_desc_tag *node,
						u32 num)
{
	struct sipa_cmn_fifo_cfg_tag *ipa_term_fifo;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have this id %d\n", id);
		return -EINVAL;
	}
	ipa_term_fifo = b + id;

	return ipa_put_pkt_to_rx_fifo(ipa_term_fifo, node, num);
}

/**
 * Description: Put Node to rx fifo.
 * Input:
 *   @id: The FIFO id that need to be operated.
 *   @pkt: The node address that need to be put into rx fifo.
 *   @num: The number of put.
 * OUTPUT:
 *   @The num that has be put into rx fifo successful.
 * Note:
 */
static int ipa_cmn_fifo_phy_put_node_to_tx_fifo(struct device *dev,
						enum sipa_cmn_fifo_index id,
						struct sipa_cmn_fifo_cfg_tag *b,
						struct sipa_node_desc_tag *node,
						u32 num)
{
	struct sipa_cmn_fifo_cfg_tag *ipa_term_fifo;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have this id %d\n", id);
		return -EINVAL;
	}
	ipa_term_fifo = b + id;

	return ipa_put_pkt_to_tx_fifo(ipa_term_fifo, node, num);
}

static int ipa_cmn_fifo_phy_get_tx_left_cnt(enum sipa_cmn_fifo_index id,
					    struct sipa_cmn_fifo_cfg_tag *b)
{
	struct sipa_cmn_fifo_cfg_tag *ipa_term_fifo = NULL;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have this id %d\n", id);
		return -EINVAL;
	}
	ipa_term_fifo = b + id;

	return (ipa_phy_get_tx_fifo_total_depth(ipa_term_fifo->fifo_reg_base) -
		ipa_phy_get_tx_fifo_filled_depth(ipa_term_fifo->fifo_reg_base));
}

/* Description: Send Node to rx fifo.
 * Input:
 *   id: The FIFO id that need to be operated.
 *   pkt: The node address that need send to rx fifo.
 *   num: The number of need to send.
 * OUTPUT:
 *   The number that has get from tx fifo successful.
 * Note:
 */
static u32
ipa_cmn_fifo_phy_recv_node_from_tx_fifo(struct device *dev,
					enum sipa_cmn_fifo_index id,
					struct sipa_cmn_fifo_cfg_tag *cfg_base,
					struct sipa_node_desc_tag *node,
					u32 num)
{
	struct sipa_cmn_fifo_cfg_tag *ipa_term_fifo;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have this id %d\n", id);
		return -EINVAL;
	}
	ipa_term_fifo = cfg_base + id;

	return ipa_recv_pkt_from_tx_fifo(ipa_term_fifo, node, num);
}

static u32
ipa_cmn_fifo_phy_sync_node_from_tx_fifo(struct device *dev,
					enum sipa_cmn_fifo_index id,
					struct sipa_cmn_fifo_cfg_tag *cfg_base,
					u32 num)
{
	struct sipa_cmn_fifo_cfg_tag *ipa_term_fifo;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		dev_err(dev, "sipa don't have this id %d\n", id);
		return -EINVAL;
	}

	ipa_term_fifo = cfg_base + id;

	return ipa_sync_pkt_from_tx_fifo(dev, ipa_term_fifo, num);
}

static u32
ipa_cmn_fifo_phy_sync_node_to_rx_fifo(struct device *dev,
				      enum sipa_cmn_fifo_index id,
				      struct sipa_cmn_fifo_cfg_tag *cfg_base,
				      u32 num)
{
	struct sipa_cmn_fifo_cfg_tag *ipa_term_fifo;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		dev_err(dev, "sipa don't have this id %d\n", id);
		return -EINVAL;
	}

	ipa_term_fifo = cfg_base + id;

	return ipa_sync_pkt_to_rx_fifo(dev, ipa_term_fifo, num);
}

static int ipa_cmn_fifo_phy_get_rx_ptr(enum sipa_cmn_fifo_index id,
				       struct sipa_cmn_fifo_cfg_tag *cfg_base,
				       u32 *wr, u32 *rd)
{
	struct sipa_cmn_fifo_cfg_tag *ipa_term_fifo;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have this id %d\n", id);
		return -EINVAL;
	}
	ipa_term_fifo = cfg_base + id;

	if (rd)
		*rd = ipa_phy_get_rx_fifo_rptr(ipa_term_fifo->fifo_reg_base);
	if (wr)
		*wr = ipa_phy_get_rx_fifo_wptr(ipa_term_fifo->fifo_reg_base);

	return 0;
}

static int ipa_cmn_fifo_phy_get_tx_ptr(enum sipa_cmn_fifo_index id,
				       struct sipa_cmn_fifo_cfg_tag *cfg_base,
				       u32 *wr, u32 *rd)
{
	struct sipa_cmn_fifo_cfg_tag *ipa_term_fifo;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have this id %d\n", id);
		return -EINVAL;
	}
	ipa_term_fifo = cfg_base + id;

	if (rd)
		*rd = ipa_phy_get_tx_fifo_rptr(ipa_term_fifo->fifo_reg_base);
	if (wr)
		*wr = ipa_phy_get_tx_fifo_wptr(ipa_term_fifo->fifo_reg_base);

	return 0;
}

static int ipa_cmn_fifo_phy_get_filled_depth(enum sipa_cmn_fifo_index id,
					     struct sipa_cmn_fifo_cfg_tag *b,
					     u32 *rx, u32 *tx)
{
	struct sipa_cmn_fifo_cfg_tag *fifo;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have this id %d\n", id);
		return -EINVAL;
	}
	fifo = b + id;

	if (tx)
		*tx = ipa_phy_get_tx_fifo_filled_depth(fifo->fifo_reg_base);
	if (rx)
		*rx = ipa_phy_get_rx_fifo_filled_depth(fifo->fifo_reg_base);

	return 0;
}

static int ipa_cmn_fifo_phy_get_tx_full_status(enum sipa_cmn_fifo_index id,
					       struct sipa_cmn_fifo_cfg_tag *b)
{
	struct sipa_cmn_fifo_cfg_tag *ipa_term_fifo;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have this id %d\n", id);
		return -EINVAL;
	}
	ipa_term_fifo = b + id;

	return ipa_phy_get_tx_fifo_full_status(ipa_term_fifo->fifo_reg_base);
}

static int ipa_cmn_fifo_phy_get_tx_empty_status(enum sipa_cmn_fifo_index id,
						struct sipa_cmn_fifo_cfg_tag *b)
{
	struct sipa_cmn_fifo_cfg_tag *ipa_term_fifo;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have this id %d\n", id);
		return -EINVAL;
	}
	ipa_term_fifo = b + id;

	return ipa_phy_get_tx_fifo_empty_status(ipa_term_fifo->fifo_reg_base);
}

static int ipa_cmn_fifo_phy_get_rx_full_status(enum sipa_cmn_fifo_index id,
					       struct sipa_cmn_fifo_cfg_tag *b)
{
	struct sipa_cmn_fifo_cfg_tag *ipa_term_fifo;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have this id %d\n", id);
		return -EINVAL;
	}
	ipa_term_fifo = b + id;

	return ipa_phy_get_rx_fifo_full_status(ipa_term_fifo->fifo_reg_base);
}

static int ipa_cmn_fifo_phy_get_rx_empty_status(enum sipa_cmn_fifo_index id,
						struct sipa_cmn_fifo_cfg_tag *b)
{
	struct sipa_cmn_fifo_cfg_tag *ipa_term_fifo;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have this id %d\n", id);
		return -EINVAL;
	}
	ipa_term_fifo = b + id;

	return ipa_phy_get_rx_fifo_empty_status(ipa_term_fifo->fifo_reg_base);
}

static int ipa_cmn_fifo_phy_stop_recv(enum sipa_cmn_fifo_index id,
				      struct sipa_cmn_fifo_cfg_tag *cfg_base,
				      bool stop)
{
	struct sipa_cmn_fifo_cfg_tag *ipa_term_fifo;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have this id %d\n", id);
		return -EINVAL;
	}
	ipa_term_fifo = cfg_base + id;

	if (stop) {
		if (ipa_phy_stop_receive(ipa_term_fifo->fifo_reg_base)) {
			pr_err("sipa fifo %d stop recv fail\n", id);
			return -EIO;
		}
	} else {
		if (ipa_phy_clear_stop_receive(ipa_term_fifo->fifo_reg_base)) {
			pr_err("sipa fifo %d clear stop recv fail\n", id);
			return -EIO;
		}
	}

	return 0;
}

static void *ipa_cmn_fifo_phy_get_tx_node_rptr(enum sipa_cmn_fifo_index id,
					       struct sipa_cmn_fifo_cfg_tag *b,
					       u32 index)
{
	u32 tmp;
	struct sipa_cmn_fifo_cfg_tag *fifo_cfg;
	struct sipa_node_desc_tag *node;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have fifo %d id err\n", id);
		return NULL;
	}

	fifo_cfg = b + id;
	node = (struct sipa_node_desc_tag *)fifo_cfg->tx_fifo.virt_addr;

	if (unlikely(!node))
		return NULL;

	tmp = (fifo_cfg->tx_fifo.rd + index) & (fifo_cfg->tx_fifo.depth - 1);

	return node + tmp;
}

static int ipa_cmn_fifo_phy_add_tx_fifo_rptr(enum sipa_cmn_fifo_index id,
					     struct sipa_cmn_fifo_cfg_tag *b,
					     u32 tx_rd)
{
	struct sipa_cmn_fifo_cfg_tag *fifo_cfg;

	if (unlikely(id >= SIPA_FIFO_MAX))
		return -EINVAL;

	fifo_cfg = b + id;
	fifo_cfg->tx_fifo.rd = (fifo_cfg->tx_fifo.rd + tx_rd) &
		PTR_MASK(fifo_cfg->tx_fifo.depth);
	if (ipa_phy_update_tx_fifo_rptr(fifo_cfg->fifo_reg_base,
					fifo_cfg->tx_fifo.rd)) {
		pr_err("update tx fifo rptr fail !!!\n");
		return -EINVAL;
	}

	return 0;
}

static void *ipa_cmn_fifo_phy_get_rx_node_wptr(enum sipa_cmn_fifo_index id,
					       struct sipa_cmn_fifo_cfg_tag *b,
					       u32 index)
{
	u32 tmp;
	struct sipa_cmn_fifo_cfg_tag *fifo_cfg;
	struct sipa_node_desc_tag *node;

	if (unlikely(id >= SIPA_FIFO_MAX)) {
		pr_err("sipa don't have fifo %d id err\n", id);
		return NULL;
	}

	fifo_cfg = b + id;
	node = (struct sipa_node_desc_tag *)fifo_cfg->rx_fifo.virt_addr;

	if (unlikely(!node)) {
		pr_err("sipa fifo %d rx fifo virt addr node = %p\n",
		       id, node);
		return NULL;
	}

	tmp = (fifo_cfg->rx_fifo.wr + index) & (fifo_cfg->rx_fifo.depth - 1);

	return node + tmp;
}

static int ipa_cmn_fifo_phy_add_rx_fifo_wptr(enum sipa_cmn_fifo_index id,
					     struct sipa_cmn_fifo_cfg_tag *b,
					     u32 index)
{
	struct sipa_cmn_fifo_cfg_tag *fifo_cfg;

	if (unlikely(id >= SIPA_FIFO_MAX))
		return -EINVAL;

	/* Ensure that data copy completion before we update rptr/wptr */
	smp_wmb();
	fifo_cfg = b + id;
	fifo_cfg->rx_fifo.wr = (fifo_cfg->rx_fifo.wr + index) &
		PTR_MASK(fifo_cfg->rx_fifo.depth);
	if (ipa_phy_update_rx_fifo_wptr(fifo_cfg->fifo_reg_base,
					fifo_cfg->rx_fifo.wr)) {
		pr_err("update rx fifo wptr fail !!!\n");
		return -EIO;
	}

	return 0;
}

static int ipa_cmn_fifo_phy_reclaim_cmn_fifo(enum sipa_cmn_fifo_index id,
					     struct sipa_cmn_fifo_cfg_tag *b)
{
	int ret = 0;
	u32 tx_rptr, tx_wptr, rx_wptr, rx_rptr;
	struct sipa_cmn_fifo_cfg_tag *fifo_cfg;

	if (unlikely(id >= SIPA_FIFO_MAX))
		return -EINVAL;

	fifo_cfg = b + id;
	if (!fifo_cfg->is_pam) {
		pr_err("sipa reclaim fifo id is err\n");
		return -EINVAL;
	}

	rx_rptr = ipa_phy_get_rx_fifo_rptr(fifo_cfg->fifo_reg_base);
	rx_wptr = ipa_phy_get_rx_fifo_wptr(fifo_cfg->fifo_reg_base);
	tx_rptr = ipa_phy_get_tx_fifo_rptr(fifo_cfg->fifo_reg_base);
	tx_wptr = ipa_phy_get_tx_fifo_wptr(fifo_cfg->fifo_reg_base);

	pr_info("reclaim %d start, rx_rptr = 0x%x, rx_wptr = 0x%x, tx_rptr = 0x%x, tx_wptr = 0x%x\n",
		id, rx_rptr, rx_wptr, tx_rptr, tx_wptr);

	if (id >= SIPA_FIFO_USB_DL) {
		if (tx_wptr != tx_rptr)
			ret = ipa_relm_tx_fifo_unread_node(fifo_cfg,
							   tx_rptr, tx_wptr,
							   rx_rptr, rx_wptr);
		else if (tx_wptr != rx_wptr)
			ret = ipa_relm_unfree_node(fifo_cfg, tx_rptr, tx_wptr,
						   rx_rptr, rx_wptr);
	}

	if (id < SIPA_FIFO_USB_DL) {
		if (tx_wptr != tx_rptr)
			ret = ipa_relm_tx_fifo_unread_free_node(fifo_cfg,
								tx_rptr, tx_wptr,
								rx_rptr, rx_wptr);
	}

	rx_rptr = ipa_phy_get_rx_fifo_rptr(fifo_cfg->fifo_reg_base);
	rx_wptr = ipa_phy_get_rx_fifo_wptr(fifo_cfg->fifo_reg_base);
	tx_rptr = ipa_phy_get_tx_fifo_rptr(fifo_cfg->fifo_reg_base);
	tx_wptr = ipa_phy_get_tx_fifo_wptr(fifo_cfg->fifo_reg_base);

	pr_info("reclaim %d end, rx_rptr = 0x%x, rx_wptr = 0x%x, tx_rptr = 0x%x, tx_wptr = 0x%x\n",
		id, rx_rptr, rx_wptr, tx_rptr, tx_wptr);

	return ret;
}

static int ipa_cmn_fifo_traverse_int_bit(enum sipa_cmn_fifo_index id,
					 struct sipa_cmn_fifo_cfg_tag *base,
					 int irq)
{
	void __iomem *fifo_base;
	u32 clr_sts = 0, int_status = 0;
	struct sipa_cmn_fifo_cfg_tag *ipa_cfg;

	if (unlikely(id >= SIPA_FIFO_MAX))
		return -EINVAL;

	ipa_cfg = base + id;
	fifo_base = ipa_cfg->fifo_reg_base;
	int_status = readl_relaxed(fifo_base + IPA_INT_GEN_CTL_EN);
	int_status &= IPA_INT_INT_STS_GROUP;

	if (!int_status) {
		if (ipa_cfg->fifo_irq_callback)
			ipa_cfg->fifo_irq_callback(ipa_cfg->priv, int_status,
						   id, irq);
		return 0;
	}

	if (int_status & IPA_INT_EXIT_FLOW_CTRL_STS) {
		ipa_cfg->exit_flow_ctrl_cnt++;
		clr_sts |= IPA_EXIT_FLOW_CONTROL_CLR_BIT;
	}

	if (int_status & IPA_INT_ENTER_FLOW_CTRL_STS) {
		ipa_cfg->enter_flow_ctrl_cnt++;
		clr_sts |= IPA_ENTER_FLOW_CONTROL_CLR_BIT;
	}

	if (int_status & IPA_INT_ERRORCODE_IN_TX_FIFO_STS)
		clr_sts |= IPA_ERROR_CODE_INTR_CLR_BIT;

	if (int_status & (IPA_INT_INTR_BIT_STS |
			  IPA_INT_TX_FIFO_THRESHOLD_SW_STS |
			  IPA_INT_DELAY_TIMER_STS)) {
		clr_sts |= IPA_TX_FIFO_INTR_CLR_BIT |
			IPA_TX_FIFO_THRESHOLD_CLR_BIT |
			IPA_TX_FIFO_TIMER_CLR_BIT;
	}

	if (int_status & IPA_INT_TXFIFO_OVERFLOW_STS)
		clr_sts |= IPA_TX_FIFO_OVERFLOW_CLR_BIT;

	if (int_status & IPA_INT_TXFIFO_FULL_INT_STS)
		clr_sts |= IPA_TX_FIFO_FULL_INT_CLR_BIT;

	if (ipa_cfg->fifo_irq_callback)
		ipa_cfg->fifo_irq_callback(ipa_cfg->priv, int_status, id, irq);
	else
		pr_debug("Don't register this fifo(%d) irq callback\n", id);

	writel_relaxed(clr_sts, fifo_base + IPA_INT_GEN_CTL_CLR);

	/* make sure the write operation is complete */
	smp_wmb();

	return 0;
}

void sipa_fifo_ops_init(struct sipa_fifo_phy_ops *ops)
{
	if (!ops)
		return;

	ops->open = ipa_cmn_fifo_phy_open;
	ops->close = ipa_cmn_fifo_phy_close;
	ops->reset_fifo = ipa_cmn_fifo_phy_reset_fifo;
	ops->set_rx_depth = ipa_cmn_fifo_phy_set_rx_depth;
	ops->get_rx_depth = ipa_cmn_fifo_phy_get_rx_depth;
	ops->set_tx_depth = ipa_cmn_fifo_phy_set_tx_depth;
	ops->get_tx_depth = ipa_cmn_fifo_phy_get_tx_depth;
	ops->set_intr_errcode = ipa_cmn_fifo_phy_set_intr_errcode;
	ops->set_intr_timeout = ipa_cmn_fifo_phy_set_intr_timeout;
	ops->set_hw_intr_timeout = ipa_cmn_fifo_phy_set_hw_intr_timeout;
	ops->set_intr_thres = ipa_cmn_fifo_phy_set_intr_thres;
	ops->set_hw_intr_thres = ipa_cmn_fifo_phy_set_hw_intr_thres;
	ops->set_src_dst_term = ipa_cmn_fifo_phy_set_src_dst_term;
	ops->enable_flowctrl_irq = ipa_cmn_fifo_phy_enable_flowctrl_irq;
	ops->set_flowctrl_mode = ipa_cmn_fifo_phy_set_flowctrl_mode;
	ops->cmn_fifo_non_stop_on_flowctl = ipa_phy_cmn_fifo_non_stop_on_flowctl;
	ops->check_cmn_fifo_flowctl = ipa_phy_check_cmn_fifo_flowctl;
	ops->check_cmn_fifo_enter_flowctl = ipa_phy_check_cmn_fifo_enter_flowctl;
	ops->check_cmn_fifo_exit_flowctl = ipa_phy_check_cmn_fifo_exit_flowctl;
	ops->cmn_fifo_flowctl_recover = ipa_phy_cmn_fifo_flowctl_recover;
	ops->clr_cmn_fifo_flowctl_interrupt = ipa_phy_clr_cmn_fifo_flowctl_interrupt;
	ops->clr_cfifo_flowctl_exit_inter = ipa_phy_clr_cfifo_flowctl_exit_inter;
	ops->clr_cfifo_flowctl_enter_inter = ipa_phy_clr_cfifo_flowctl_enter_inter;
	ops->set_node_intr = ipa_cmn_fifo_phy_set_node_intr;
	ops->set_overflow_intr = ipa_cmn_fifo_phy_set_overflow_intr;
	ops->set_tx_full_intr = ipa_cmn_fifo_phy_set_tx_full_intr;
	ops->put_node_to_rx_fifo = ipa_cmn_fifo_phy_put_node_to_rx_fifo;
	ops->put_node_to_tx_fifo = ipa_cmn_fifo_phy_put_node_to_tx_fifo;
	ops->get_tx_left_cnt = ipa_cmn_fifo_phy_get_tx_left_cnt;
	ops->recv_node_from_tx_fifo = ipa_cmn_fifo_phy_recv_node_from_tx_fifo;
	ops->sync_node_from_tx_fifo = ipa_cmn_fifo_phy_sync_node_from_tx_fifo;
	ops->sync_node_to_rx_fifo = ipa_cmn_fifo_phy_sync_node_to_rx_fifo;
	ops->get_rx_ptr = ipa_cmn_fifo_phy_get_rx_ptr;
	ops->get_tx_ptr = ipa_cmn_fifo_phy_get_tx_ptr;
	ops->get_filled_depth = ipa_cmn_fifo_phy_get_filled_depth;
	ops->get_tx_full_status = ipa_cmn_fifo_phy_get_tx_full_status;
	ops->get_tx_empty_status = ipa_cmn_fifo_phy_get_tx_empty_status;
	ops->get_rx_full_status = ipa_cmn_fifo_phy_get_rx_full_status;
	ops->get_rx_empty_status = ipa_cmn_fifo_phy_get_rx_empty_status;
	ops->stop_recv = ipa_cmn_fifo_phy_stop_recv;
	ops->get_tx_node_rptr = ipa_cmn_fifo_phy_get_tx_node_rptr;
	ops->add_tx_fifo_rptr = ipa_cmn_fifo_phy_add_tx_fifo_rptr;
	ops->get_rx_node_wptr = ipa_cmn_fifo_phy_get_rx_node_wptr;
	ops->add_rx_fifo_wptr = ipa_cmn_fifo_phy_add_rx_fifo_wptr;
	ops->reclaim_cmn_fifo = ipa_cmn_fifo_phy_reclaim_cmn_fifo;
	ops->restore_irq_map_in = ipa_cmn_fifo_phy_restore_irq_map_in;
	ops->restore_irq_map_out = ipa_cmn_fifo_phy_restore_irq_map_out;
	ops->traverse_int_bit = ipa_cmn_fifo_traverse_int_bit;
}
