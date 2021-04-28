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

#ifndef _STAR_H_
#define _STAR_H_

#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ip.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/netdevice.h>
#include <linux/platform_device.h>
#include <linux/mii.h>
#include <linux/version.h>
#include <linux/dma-mapping.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/regulator/consumer.h>

#include "star_mac.h"
#include "star_phy.h"

/* use  rmii mode */
#define CONFIG_STAR_USE_RMII_MODE

#define ETH_MAX_FRAME_SIZE          1536
#define ETH_SKB_ALIGNMENT           16

#define TX_DESC_NUM  128
#define RX_DESC_NUM  128
#define TX_DESC_TOTAL_SIZE (sizeof(tx_desc) * TX_DESC_NUM)
#define RX_DESC_TOTAL_SIZE (sizeof(rx_desc) * RX_DESC_NUM)
#define ETH_EXTRA_PKT_LEN 36
#define ETH_ADDR_LEN 6
#define STAR_NAPI_WEIGHT (RX_DESC_NUM << 1)

/* Star Ethernet Configuration*/
/* ====================================== */
#define star_intr_disable(dev) \
		star_set_reg(star_int_mask((dev)->base), 0xffffffff)
#define star_intr_enable(dev) \
		star_set_reg(star_int_mask((dev)->base), 0)
#define star_intr_clear(dev, intrStatus) \
		star_set_reg(star_int_sta((dev)->base), intrStatus)
#define star_intr_status(dev) \
		star_get_reg(star_int_sta((dev)->base))
#define star_intr_rx_enable(dev) \
		star_clear_bit(star_int_mask((dev)->base), STAR_INT_STA_RXC)
#define star_intr_rx_disable(dev) \
		star_set_bit(star_int_mask((dev)->base), STAR_INT_STA_RXC)
#define star_intr_mask(dev) \
		star_get_reg(star_int_mask((dev)->base))

#define RX_RESUME BIT(2)
#define RX_STOP BIT(1)
#define RX_START BIT(0)

#define dma_tx_start_and_reset_tx_desc(dev) \
		star_set_bit(star_tx_dma_ctrl((dev)->base), TX_START)
#define dma_rx_start_and_reset_rx_desc(dev) \
		star_set_bit(star_rx_dma_ctrl((dev)->base), RX_START)
#define star_dma_tx_enable(dev) \
		star_set_bit(star_tx_dma_ctrl((dev)->base), TX_RESUME)
#define star_dma_tx_disable(dev) \
		star_set_bit(star_tx_dma_ctrl((dev)->base), TX_STOP)
#define star_dma_rx_enable(dev) \
		star_set_bit(star_rx_dma_ctrl((dev)->base), RX_RESUME)
#define star_dma_rx_disable(dev) \
		star_set_bit(star_rx_dma_ctrl((dev)->base), RX_STOP)

#define star_dma_tx_resume(dev) star_dma_tx_enable(dev)
#define star_dma_rx_resume(dev) star_dma_rx_enable(dev)

#define star_reset_hash_table(dev) \
		star_set_bit(star_test1((dev)->base), STAR_TEST1_RST_HASH_BIST)

#define star_dma_rx_crc_err(ctrl_len) ((ctrl_len & RX_CRCERR) ? 1 : 0)
#define star_dma_rx_over_size(ctrl_len) ((ctrl_len & RX_OSIZE) ? 1 : 0)

#define star_dma_rx_length(ctrl_len) \
		((ctrl_len >> RX_LEN_OFFSET) & RX_LEN_MASK)
#define star_dma_tx_length(ctrl_len) \
		((ctrl_len >> TX_LEN_OFFSET) & TX_LEN_MASK)

#define star_arl_promisc_enable(dev) \
		star_set_bit(STAR_ARL_CFG((dev)->base), STAR_ARL_CFG_MISCMODE)

enum wol_type {
	WOL_NONE = 0,
	MAC_WOL,
	PHY_WOL,
};

/**
 * @brief structure for Star private data
 * @wol:		ethernet mac wol type status
 * @wol_flag:		normal wol: set true to enable, set false to disable.
 */
struct star_private_s {
	struct regulator *phy_regulator;
	struct clk *core_clk, *reg_clk, *trans_clk;
	star_dev star_dev;
	struct net_device *dev;
	dma_addr_t desc_dma_addr;
	uintptr_t desc_vir_addr;
	u32 phy_addr;
	/* star lock */
	spinlock_t lock;
	struct tasklet_struct dsr;
	bool tsk_tx;
	struct napi_struct napi;
	struct mii_if_info mii;
	struct input_dev *idev;
	bool opened;
	bool support_wol;
	bool support_rmii;
	int eint_irq;
	int eint_pin;
	enum wol_type wol;
	bool wol_flag;
} star_private;

struct eth_phy_ops {
	u32 addr;
	/* value of phy reg3(identifier2) */
	u32 phy_id;
	void (*init)(star_dev *sdev);
	void (*wol_enable)(struct net_device *netdev);
	void (*wol_disable)(struct net_device *netdev);
};

/* star mac memory barrier */
#define star_mb() mb()

#define STAR_PR_ERR(fmt...) pr_err("star: " fmt)
#define STAR_PR_INFO(fmt...) pr_info("star: " fmt)
#define STAR_PR_DEBUG(fmt...) pr_debug("star: " fmt)

static inline void star_set_reg(void __iomem *reg, u32 value)
{
	STAR_PR_DEBUG("%s(%p)=%08x\n", __func__, reg, value);
	iowrite32(value, reg);
}

static inline u32 star_get_reg(void __iomem *reg)
{
	u32 data = ioread32(reg);

	STAR_PR_DEBUG("%s(%p)=%08x\n", __func__, reg, data);
	return data;
}

static inline void star_set_bit(void __iomem *reg, u32 bit)
{
	u32 data = ioread32(reg);

	data |= bit;
	STAR_PR_DEBUG("%s(%p,bit:%08x)=%08x\n", __func__, reg, bit, data);
	iowrite32(data, reg);
	star_mb();
}

static inline void star_clear_bit(void __iomem *reg, u32 bit)
{
	u32 data = ioread32(reg);

	data &= ~bit;
	STAR_PR_DEBUG(
		 "%s(%p,bit:%08x)=%08x\n", __func__, reg, bit, data);
	iowrite32(data, reg);
	star_mb();
}

static inline u32 star_get_bit_mask(void __iomem *reg, u32 mask, u32 offset)
{
	u32 data = ioread32(reg);

	data = ((data >> offset) & mask);
	STAR_PR_DEBUG(
		 "%s(%p,mask:%08x,offset:%08x)=%08x(data)\n",
		 __func__, reg, mask, offset, data);
	return data;
}

static inline u32 star_is_set_bit(void __iomem *reg, u32 bit)
{
	u32 data = ioread32(reg);

	data &= bit;
	STAR_PR_DEBUG(
		 "%s(%p,bit:%08x)=%08x\n", __func__, reg, bit, data);
	return data ? 1 : 0;
}

int star_get_wol_flag(star_private *star_prv);
void star_set_wol_flag(star_private *star_prv, bool flag);
int star_get_dbg_level(void);
void star_set_dbg_level(int dbg);
u32 star_dma_rx_valid(u32 ctrl_len);

#endif /* _STAR_H_ */
