/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_QOS_SRAM_H__
#define __MTK_QOS_SRAM_H__

struct qos_sram_addr {
	int offset;
	bool valid;
};

enum {
	QOS_DEBUG_0,
	QOS_DEBUG_1,
	QOS_DEBUG_2,
	QOS_DEBUG_3,
	QOS_DEBUG_4,
	MM_SMI_VENC,
	MM_SMI_CAM,
	MM_SMI_IMG,
	MM_SMI_MDP,
	MM_SMI_CLK,
	MM_SMI_CLR,
	MM_SMI_EXE,
	MM_SMI_DUMP,
	APU_CLK,
	APU_BW_NORD,
	DVFSRC_TIMESTAMP_OFFSET,
	CM_STALL_RATIO_ID_0,
	CM_STALL_RATIO_ID_1,
	CM_STALL_RATIO_ID_2,
	CM_STALL_RATIO_ID_3,
	CM_STALL_RATIO_ID_4,
	CM_STALL_RATIO_ID_5,
	CM_STALL_RATIO_ID_6,
	CM_STALL_RATIO_ID_7,
	QOS_SRAM_ID_MAX,
};

extern u32 qos_sram_read(u32 id);
extern void qos_sram_write(u32 id, u32 val);
extern void qos_sram_init(void __iomem *regs, unsigned int bound);
#endif
