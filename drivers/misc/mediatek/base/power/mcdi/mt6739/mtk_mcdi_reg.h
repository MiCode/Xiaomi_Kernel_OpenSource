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
#ifndef __MTK_MCDI_REG_H__
#define __MTK_MCDI_REG_H__

/* MCDI systram */
#define MCDI_SYSRAM_SIZE	0x400

extern void __iomem *mcdi_sysram_base;
extern void __iomem *mcdi_mcupm_base;
extern void __iomem *mcdi_mcupm_sram_base;

/* cluster counter */
#define MCUPM_CFGREG_MP0_SLEEP_TH	(0x0080)
#define MCUPM_CFGREG_MP0_CPU0_RES	(0x0084)
#define MCUPM_CFGREG_MP0_CPU1_RES	(0x0088)
#define MCUPM_CFGREG_MP0_CPU2_RES	(0x008C)
#define MCUPM_CFGREG_MP0_CPU3_RES	(0x0090)

/* debug */
#define MCUPM_SRAM_DEBUG_BASE		(mcdi_mcupm_sram_base + 0x1800)
/* mbox */
#define MCUPM_SRAM_MBOX_BASE		(mcdi_mcupm_sram_base + 0x1D00)
#define MCDI_MBOX			MCUPM_SRAM_MBOX_BASE

/* MBOX */
#define MCDI_MBOX_SLOT_OFFSET_START		0
#define MCDI_MBOX_SLOT_OFFSET_END		12
#define MCDI_MBOX_NONUSE			11
/* MBOX: AP write, SSPM read */
#define MCDI_MBOX_CLUSTER_0_CAN_POWER_OFF	MCDI_MBOX_NONUSE
#define MCDI_MBOX_CLUSTER_1_CAN_POWER_OFF	MCDI_MBOX_NONUSE
#define MCDI_MBOX_BUCK_POWER_OFF_MASK		10
#define MCDI_MBOX_CLUSTER_0_ATF_ACTION_DONE	MCDI_MBOX_NONUSE
#define MCDI_MBOX_CLUSTER_1_ATF_ACTION_DONE	MCDI_MBOX_NONUSE
#define MCDI_MBOX_RESERVED			MCDI_MBOX_NONUSE
#define MCDI_MBOX_PAUSE_ACTION			0
#define MCDI_MBOX_AVAIL_CPU_MASK		2
#define MCDI_MBOX_CPU_ISOLATION_MASK		MCDI_MBOX_NONUSE
/* MBOX: AP read, SSPM write */
#define MCDI_MBOX_CPU_CLUSTER_PWR_STAT		8
#define MCDI_MBOX_ACTION_STAT			MCDI_MBOX_NONUSE
#define MCDI_MBOX_CLUSTER_0_CNT			3
#define MCDI_MBOX_CLUSTER_1_CNT			MCDI_MBOX_NONUSE
#define MCDI_MBOX_PAUSE_ACK			1
#define MCDI_MBOX_PENDING_ON_EVENT		4
#define MCDI_MBOX_PROF_CMD			9
#define MCDI_MBOX_PROF_CLUSTER			MCDI_MBOX_NONUSE
#define MCDI_MBOX_AP_READY			MCDI_MBOX_NONUSE
#define MCDI_MBOX_PENDING_OFF_EVENT		5
#define MCDI_MBOX_PENDING_ON_CLUSTER		6
#define MCDI_MBOX_PENDING_OFF_CLUSTER		7

/* profile */
#define MCUPM_SRAM_PROFILE_BASE		(mcdi_mcupm_sram_base + 0x1000)
#define SYSRAM_PROF_DATA_REG		(MCUPM_SRAM_PROFILE_BASE + 0xA60)
#define SYSRAM_PROF_RATIO_REG		(MCUPM_SRAM_PROFILE_BASE + 0xAFC)
#define SYSRAM_PROF_BASE_REG		(MCUPM_SRAM_PROFILE_BASE + 0xAD0)
#define SYSRAM_DISTRIBUTE_BASE_REG	(MCUPM_SRAM_PROFILE_BASE + 0xB28)
#define SYSRAM_LATENCY_BASE_REG		(MCUPM_SRAM_PROFILE_BASE + 0xB44)
#define SYSRAM_PROF_RARIO_DUR		(MCUPM_SRAM_PROFILE_BASE + 0xB84)

#define SYSRAM_PROF_REG(ofs)		(SYSRAM_PROF_BASE_REG + ofs)
#define CPU_OFF_LATENCY_REG(ofs)	(SYSRAM_LATENCY_BASE_REG + ofs)
#define CPU_ON_LATENCY_REG(ofs)		(SYSRAM_LATENCY_BASE_REG + 0x10 + ofs)
#define Cluster_OFF_LATENCY_REG(ofs)	(SYSRAM_LATENCY_BASE_REG + 0x20 + ofs)
#define Cluster_ON_LATENCY_REG(ofs)	(SYSRAM_LATENCY_BASE_REG + 0x30 + ofs)

#define DISTRIBUTE_NUM 4
#define LATENCY_DISTRIBUTE_REG(ofs)	(SYSRAM_DISTRIBUTE_BASE_REG + ofs)
#define PROF_OFF_CNT_REG(idx)		(LATENCY_DISTRIBUTE_REG(idx * 4))
#define PROF_ON_CNT_REG(idx)		(LATENCY_DISTRIBUTE_REG((idx + 3) * 4))

#define ID_OFS		(0x0)
#define AVG_OFS		(0x4)
#define MAX_OFS		(0x8)
#define CNT_OFS		(0xC)

#define cpu_ratio_shift(idx)		((idx) * 0x4)
#define cluster_ratio_shift(idx)	(((idx) + NF_CPU) * 0x4)
#define PROF_CPU_RATIO_REG(idx) \
		(SYSRAM_PROF_RATIO_REG + cpu_ratio_shift(idx))
#define PROF_CLUSTER_RATIO_REG(idx) \
		(SYSRAM_PROF_RATIO_REG + cluster_ratio_shift(idx))

#endif /* __MTK_MCDI_REG_H__ */
