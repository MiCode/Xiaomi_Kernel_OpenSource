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

// FIXME: should read base address from device tree
#if 1
#define	APU_RPCTOP_BASE		(0x190F0000)
#else
extern uint32_t g_APU_RPCTOP_BASE;
#define	APU_RPCTOP_BASE		(g_APU_RPCTOP_BASE)
#endif

#define	APU_RPC_TOP_CON		(APU_RPCTOP_BASE + 0x0)
#define	APU_RPC_SW_FIFO_WE	(APU_RPCTOP_BASE + 0x8)
#define	APU_RPC_INTF_PWR_RDY	(APU_RPCTOP_BASE + 0x44)
#define	APU_RPC_SW_TYPE0	(APU_RPCTOP_BASE + 0x200)
#define	APU_RPC_SW_TYPE1	(APU_RPCTOP_BASE + 0x210)
#define	APU_RPC_SW_TYPE2	(APU_RPCTOP_BASE + 0x220)
#define	APU_RPC_SW_TYPE3	(APU_RPCTOP_BASE + 0x230)
#define	APU_RPC_SW_TYPE4	(APU_RPCTOP_BASE + 0x240)
