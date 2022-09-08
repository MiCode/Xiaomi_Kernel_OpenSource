/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 MediaTek Inc.
 */
#ifndef __MTK_MCDI_REG_H__
#define __MTK_MCDI_REG_H__

/* MCDI systram */
#define MCDI_SYSRAM_SIZE    0x800

extern void __iomem *mcdi_sysram_base;

/* MCDI latency profile */
#define SYSRAM_PROF_PWR_SEQ_BK     (mcdi_sysram_base + 0x520)
#define SYSRAM_PROF_RATIO_REG      (mcdi_sysram_base + 0x5B0)
#define SYSRAM_PROF_BASE_REG       (mcdi_sysram_base + 0x600)
#define SYSRAM_PROF_DATA_REG       (mcdi_sysram_base + 0x680)
#define SYSRAM_LATENCY_BASE_REG    (mcdi_sysram_base + 0x780)
#define SYSRAM_PROF_RARIO_DUR      (mcdi_sysram_base + 0x7C0)

#define SYSRAM_PROF_REG(ofs)          (SYSRAM_PROF_BASE_REG + ofs)
#define CPU_OFF_LATENCY_REG(ofs)      (SYSRAM_LATENCY_BASE_REG + ofs)
#define CPU_ON_LATENCY_REG(ofs)       (SYSRAM_LATENCY_BASE_REG + 0x10 + ofs)
#define Cluster_OFF_LATENCY_REG(ofs)  (SYSRAM_LATENCY_BASE_REG + 0x20 + ofs)
#define Cluster_ON_LATENCY_REG(ofs)   (SYSRAM_LATENCY_BASE_REG + 0x30 + ofs)

#define ID_OFS   (0x0)
#define AVG_OFS  (0x4)
#define MAX_OFS  (0x8)
#define CNT_OFS  (0xC)

#define DISTRIBUTE_NUM 4
#define LATENCY_DISTRIBUTE_REG(ofs) (SYSRAM_PROF_BASE_REG + 0x140 + ofs)
#define PROF_OFF_CNT_REG(idx)       (LATENCY_DISTRIBUTE_REG(idx * 4))
#define PROF_ON_CNT_REG(idx)        (LATENCY_DISTRIBUTE_REG((idx + 4) * 4))

#define cpu_ratio_shift(idx)        ((idx) * 0x8 + 0x4)
#define cluster_ratio_shift(idx)    (((idx) + NF_CPU) * 0x8 + 0x4)
#define PROF_CPU_RATIO_REG(idx) \
	(SYSRAM_PROF_RATIO_REG + cpu_ratio_shift(idx))
#define PROF_CLUSTER_RATIO_REG(idx) \
	(SYSRAM_PROF_RATIO_REG + cluster_ratio_shift(idx))

/* MBOX */
#define MCDI_MBOX                               3
#define MCDI_MBOX_SLOT_OFFSET_START             0
#define MCDI_MBOX_SLOT_OFFSET_END               18

/* MBOX: AP write, SSPM read */
#define MCDI_MBOX_CLUSTER_0_CAN_POWER_OFF       0
#define MCDI_MBOX_CLUSTER_1_CAN_POWER_OFF       1
#define MCDI_MBOX_BUCK_POWER_OFF_MASK           2
#define MCDI_MBOX_CLUSTER_0_ATF_ACTION_DONE     3
#define MCDI_MBOX_CLUSTER_1_ATF_ACTION_DONE     4
#define MCDI_MBOX_RESERVED                      5
#define MCDI_MBOX_PAUSE_ACTION                  6
#define MCDI_MBOX_AVAIL_CPU_MASK                7
/* MBOX: AP read, SSPM write */
#define MCDI_MBOX_CPU_CLUSTER_PWR_STAT          8
#define MCDI_MBOX_ACTION_STAT                   9
#define MCDI_MBOX_CLUSTER_0_CNT                 10
#define MCDI_MBOX_CLUSTER_1_CNT                 11
#define MCDI_MBOX_CPU_ISOLATION_MASK            12
#define MCDI_MBOX_PAUSE_ACK                     13
#define MCDI_MBOX_PENDING_ON_EVENT              14
#define MCDI_MBOX_PROF_CMD                      15
#define MCDI_MBOX_PROF_CLUSTER                  16
#define MCDI_MBOX_AP_READY                      17

/* MCDI_MBOX_ACTION_STAT */
#define MCDI_ACTION_INIT                        0
#define MCDI_ACTION_PAUSED                      1
#define MCDI_ACTION_WAIT_EVENT                  2
#define MCDI_ACTION_WORKING                     3

#endif /* __MTK_MCDI_REG_H__ */
