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

#include <mt-plat/sync_write.h>

extern void __iomem *sspm_base;
extern void __iomem *mcdi_mcupm_base;
extern void __iomem *mcdi_mcupm_sram_base;

/* dummy definition, not used */
#define MCUPM_CFGREG_WFI_EN_SET              (MCUPM_SRAM_MBOX_BASE + 0x00)

/* cluster counter */
#define MCUPM_CFGREG_MP0_SLEEP_TH            (mcdi_mcupm_base + 0x0080)
#define MCUPM_CFGREG_MP0_CPU0_RES            (mcdi_mcupm_base + 0x0084)
#define MCUPM_CFGREG_MP0_CPU1_RES            (mcdi_mcupm_base + 0x0088)
#define MCUPM_CFGREG_MP0_CPU2_RES            (mcdi_mcupm_base + 0x008C)
#define MCUPM_CFGREG_MP0_CPU3_RES            (mcdi_mcupm_base + 0x0090)

/* debug */
#define MCUPM_SRAM_DEBUG_BASE                (mcdi_mcupm_sram_base + 0x1800)
/* mbox */
#define MCUPM_SRAM_MBOX_BASE                 (mcdi_mcupm_sram_base + 0x1D00)
/* MBOX: AP write, MCUPM read */
#define MCDI_MBOX_CLUSTER_0_ATF_ACTION_DONE  (MCUPM_CFGREG_WFI_EN_SET)
#define MCDI_MBOX_PAUSE_ACTION               (MCUPM_SRAM_MBOX_BASE + 0x00)
#define MCDI_MBOX_AVAIL_CPU_MASK             (MCUPM_SRAM_MBOX_BASE + 0x08)
#define MCDI_MBOX_BUCK_POWER_OFF_MASK        (MCUPM_SRAM_MBOX_BASE + 0x28)
/* MBOX: AP read, MCUPM write */
#define MCDI_MBOX_PAUSE_ACK                  (MCUPM_SRAM_MBOX_BASE + 0x04)
#define MCUPM_CFGREG_CPU_PDN_STA             (MCUPM_SRAM_MBOX_BASE + 0x20)
#define MCDI_MBOX_CPU_CLUSTER_PWR_STAT       (MCUPM_CFGREG_CPU_PDN_STA)
#define MCDI_MBOX_CLUSTER_0_CNT              (MCUPM_SRAM_MBOX_BASE + 0x0C)
#define MCDI_MBOX_PENDING_ON_EVENT           (MCUPM_SRAM_MBOX_BASE + 0x10)
#define MCDI_MBOX_PROF_CMD                   (MCUPM_SRAM_MBOX_BASE + 0x24)

/* profile */
#define MCUPM_SRAM_PROFILE_BASE              (mcdi_mcupm_sram_base + 0x1000)
#define SYSRAM_PROF_DATA_REG                 (MCUPM_SRAM_PROFILE_BASE + 0xA60)
#define SYSRAM_PROF_RATIO_REG                (MCUPM_SRAM_PROFILE_BASE + 0xAFC)
#define SYSRAM_PROF_BASE_REG                 (MCUPM_SRAM_PROFILE_BASE + 0xAD0)
#define SYSRAM_DISTRIBUTE_BASE_REG           (MCUPM_SRAM_PROFILE_BASE + 0xB28)
#define SYSRAM_LATENCY_BASE_REG              (MCUPM_SRAM_PROFILE_BASE + 0xB44)
#define SYSRAM_PROF_RARIO_DUR                (MCUPM_SRAM_PROFILE_BASE + 0xB84)

#define SYSRAM_PROF_REG(ofs)                 (SYSRAM_PROF_BASE_REG + ofs)
#define CPU_OFF_LATENCY_REG(ofs)             (SYSRAM_LATENCY_BASE_REG + ofs)
#define CPU_ON_LATENCY_REG(ofs)              (SYSRAM_LATENCY_BASE_REG + 0x10 + ofs)
#define Cluster_OFF_LATENCY_REG(ofs)         (SYSRAM_LATENCY_BASE_REG + 0x20 + ofs)
#define Cluster_ON_LATENCY_REG(ofs)          (SYSRAM_LATENCY_BASE_REG + 0x30 + ofs)

#define LATENCY_DISTRIBUTE_REG(ofs)          (SYSRAM_DISTRIBUTE_BASE_REG + ofs)
#define PROF_OFF_CNT_REG(idx)                (LATENCY_DISTRIBUTE_REG(idx * 4))
#define PROF_ON_CNT_REG(idx)                 (LATENCY_DISTRIBUTE_REG((idx + 3) * 4))

#define STANDBYWFI_EN(n)                     (1 << (n +  8))
#define GIC_IRQOUT_EN(n)                     (1 << (n +  0))

#define mcdi_read(addr)                      __raw_readl((void __force __iomem *)(addr))
#define mcdi_write(addr, val)                mt_reg_sync_writel(val, addr)

#endif /* __MTK_MCDI_REG_H__ */
