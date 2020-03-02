/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MTK_MCDI_MT6739_H__
#define __MTK_MCDI_MT6739_H__

/* #define MCDI_CLUSTER_COUNTER */
#define MCDI_CLUSTER_THRESHOLD (1)

#define NF_CPU                  4
#define NF_CLUSTER              1

#define MCDI_RESERVE_FOR_TIMESYNC 0x14 /* 5 words for timesync */
#define MCDI_DEBUG_VERSION 0x00010002

/* MCUPM sram for mcdi (debug part + mbox part)*/
#define MCDI_MCUPM_SRAM_DEBUG_SIZE 0x500
#define MCDI_MCUPM_SRAM_MBOX_SIZE  0x100
#define MCDI_MCUPM_SRAM_DEBUG_NF_WORD (MCDI_MCUPM_SRAM_DEBUG_SIZE / 4)
#define MCDI_MCUPM_SRAM_MBOX_NF_WORD  (MCDI_MCUPM_SRAM_MBOX_SIZE / 4)

/* system sram for mcdi */
#define MCDI_SYSRAM_SIZE    0x400 /* 1K size */
#define MCDI_SYSRAM_NF_WORD (MCDI_SYSRAM_SIZE / 4)
#define SYSRAM_DUMP_RANGE   50

enum {
	ALL_CPU_IN_CLUSTER = 0,
	CPU_CLUSTER,
	CPU_IN_OTHER_CLUSTER,
	OTHER_CLUSTER_IDX,

	NF_PWR_STAT_MAP_TYPE
};

extern void __iomem *mcdi_sysram_base;
extern void __iomem *mcdi_mcupm_base;
extern void __iomem *mcdi_mcupm_sram_base;
extern unsigned int cpu_cluster_pwr_stat_map[NF_PWR_STAT_MAP_TYPE][NF_CPU];
#endif /* __MTK_MCDI_MT6739_H__ */
