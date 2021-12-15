/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_QOS_SRAM_H__
#define __MTK_QOS_SRAM_H__

#define QOS_DEBUG_0			0x0
#define QOS_DEBUG_1			0x4
#define QOS_DEBUG_2			0x8
#define QOS_DEBUG_3			0xC
#define QOS_DEBUG_4			0x10
#define CM_PERF_HINT_OFFSET		0x18
#define QOS_PREFETCH_STATUS_OFFSET	0x1C
#define MM_SMI_VENC			0x20
#define MM_SMI_CAM			0x24
#define MM_SMI_IMG			0x28
#define MM_SMI_MDP			0x2C
#define MM_SMI_CLK			0x30
#define MM_SMI_CLR			0x34
#define MM_SMI_EXE			0x38
#define MM_SMI_DUMP			0x3C
#define CM_DDR_HISTORY			0x40
#define APU_CLK				0x48
#define APU_BW_NORD			0x4C

#define DVFSRC_TIMESTAMP_OFFSET		0x50
#define CM_STALL_RATIO_OFFSET		0x60
#define QOS_SRAM_MAX_SIZE		0x100

extern u32 qos_sram_read(u32 offset);
extern void qos_sram_write(u32 offset, u32 val);
extern void qos_sram_init(void __iomem *regs);

#endif
