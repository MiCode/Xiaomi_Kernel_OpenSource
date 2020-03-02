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

static void __iomem *g_APU_RPCTOP_BASE;
static void __iomem *g_APU_PCUTOP_BASE;

#define	APU_RPCTOP_BASE		(g_APU_RPCTOP_BASE)
#define	APU_PCUTOP_BASE		(g_APU_PCUTOP_BASE)

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
