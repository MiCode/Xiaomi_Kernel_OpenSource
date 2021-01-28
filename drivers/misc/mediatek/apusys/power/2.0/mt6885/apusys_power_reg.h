/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/io.h>
#include <sync_write.h>

static inline void DRV_WriteReg32(void *addr, uint32_t value)
{
	mt_reg_sync_writel(value, addr);
}

static inline u32 DRV_Reg32(void *addr)
{
	return ioread32(addr);
}

static inline void DRV_SetBitReg32(void *addr, uint32_t bit_mask)
{
	u32 tmp = ioread32(addr);

	tmp |= bit_mask;
	mt_reg_sync_writel(tmp, addr);
}


static void *g_APU_RPCTOP_BASE;
static void *g_APU_PCUTOP_BASE;


/**************************************************
 * APU_RPC related register
 *************************************************/
#define	APU_RPCTOP_BASE		(g_APU_RPCTOP_BASE)

#define	APU_RPC_TOP_CON		(void *)(APU_RPCTOP_BASE + 0x000)
#define	APU_RPC_TOP_SEL		(void *)(APU_RPCTOP_BASE + 0x004)
#define	APU_RPC_SW_FIFO_WE	(void *)(APU_RPCTOP_BASE + 0x008)
#define	APU_RPC_INTF_PWR_RDY	(void *)(APU_RPCTOP_BASE + 0x044)
#define	APU_RPC_SW_TYPE0	(void *)(APU_RPCTOP_BASE + 0x200)
#define	APU_RPC_SW_TYPE1	(void *)(APU_RPCTOP_BASE + 0x210)
#define	APU_RPC_SW_TYPE2	(void *)(APU_RPCTOP_BASE + 0x220)
#define	APU_RPC_SW_TYPE3	(void *)(APU_RPCTOP_BASE + 0x230)
#define	APU_RPC_SW_TYPE4	(void *)(APU_RPCTOP_BASE + 0x240)

#define REG_WAKEUP_SET		BIT(8)

/**************************************************
 * APU_PCU related register
 *************************************************/
#define	APU_PCUTOP_BASE		(g_APU_PCUTOP_BASE)

#define	APU_PCU_PMIC_TAR_BUF	(void *)(APU_PCUTOP_BASE + 0x120)
#define	APU_PCU_PMIC_CUR_BUF	(void *)(APU_PCUTOP_BASE + 0x124)
