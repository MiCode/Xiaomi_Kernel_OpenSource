/* Copyright (c) 2002,2007-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __Z80_REG_H
#define __Z80_REG_H

#define REG_VGC_IRQSTATUS__MH_MASK                         0x00000001L
#define REG_VGC_IRQSTATUS__G2D_MASK                        0x00000002L
#define REG_VGC_IRQSTATUS__FIFO_MASK                       0x00000004L

#define	MH_ARBITER_CONFIG__SAME_PAGE_GRANULARITY__SHIFT    0x00000006
#define	MH_ARBITER_CONFIG__L1_ARB_ENABLE__SHIFT            0x00000007
#define	MH_ARBITER_CONFIG__L1_ARB_HOLD_ENABLE__SHIFT       0x00000008
#define	MH_ARBITER_CONFIG__L2_ARB_CONTROL__SHIFT           0x00000009
#define	MH_ARBITER_CONFIG__PAGE_SIZE__SHIFT                0x0000000a
#define	MH_ARBITER_CONFIG__TC_REORDER_ENABLE__SHIFT        0x0000000d
#define	MH_ARBITER_CONFIG__TC_ARB_HOLD_ENABLE__SHIFT       0x0000000e
#define	MH_ARBITER_CONFIG__IN_FLIGHT_LIMIT_ENABLE__SHIFT   0x0000000f
#define	MH_ARBITER_CONFIG__IN_FLIGHT_LIMIT__SHIFT          0x00000010
#define	MH_ARBITER_CONFIG__CP_CLNT_ENABLE__SHIFT           0x00000016
#define	MH_ARBITER_CONFIG__VGT_CLNT_ENABLE__SHIFT          0x00000017
#define	MH_ARBITER_CONFIG__TC_CLNT_ENABLE__SHIFT           0x00000018
#define	MH_ARBITER_CONFIG__RB_CLNT_ENABLE__SHIFT           0x00000019
#define	MH_ARBITER_CONFIG__PA_CLNT_ENABLE__SHIFT           0x0000001a

#define ADDR_VGC_MH_READ_ADDR            0x0510
#define ADDR_VGC_MH_DATA_ADDR            0x0518
#define ADDR_VGC_COMMANDSTREAM           0x0000
#define ADDR_VGC_IRQENABLE               0x0438
#define ADDR_VGC_IRQSTATUS               0x0418
#define ADDR_VGC_IRQ_ACTIVE_CNT          0x04E0
#define ADDR_VGC_MMUCOMMANDSTREAM        0x03FC
#define ADDR_VGV3_CONTROL                0x0070
#define ADDR_VGV3_LAST                   0x007F
#define ADDR_VGV3_MODE                   0x0071
#define ADDR_VGV3_NEXTADDR               0x0075
#define ADDR_VGV3_NEXTCMD                0x0076
#define ADDR_VGV3_WRITEADDR              0x0072
#define ADDR_VGC_VERSION				 0x400
#define ADDR_VGC_SYSSTATUS				 0x410
#define ADDR_VGC_CLOCKEN				 0x508
#define ADDR_VGC_GPR0					 0x520
#define ADDR_VGC_GPR1					 0x528
#define ADDR_VGC_BUSYCNT				 0x530
#define ADDR_VGC_FIFOFREE				 0x7c0

#endif /* __Z180_REG_H */
