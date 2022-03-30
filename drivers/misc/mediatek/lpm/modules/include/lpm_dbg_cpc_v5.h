/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_PLAT_CPC_REG_H__
#define __MTK_PLAT_CPC_REG_H__

/**************************************
 * CPC related register
 * base address = 0x0C040000
 **************************************/
#define CPC_FLOW_CTRL_CFG              (0x114)
#define CPC_MCUSYS_LAST_CORE_REQ       (0x118)
#define CPC_CPUSYS_LAST_CORE_RESP      (0x11C)
#define CPC_MCUSYS_LAST_CORE_RESP      (0x124)
#define CPC_PWR_ON_MASK                (0x128)
#define CPC_SPMC_PWR_STATUS            (0x140)
#define CPC_CPUSYS_CPU_ON_SW_HINT_CLR  (0x1AC)

#define SPMC_DBG_SETTING               (0x200)

#define CPC_TRACE_DATA_SEL             (0x214)

#define CPC_CPU0_LATENCY               (0x240)
#define CPC_CLUSTER_OFF_LATENCY        (0x260)
#define CPC_CLUSTER_ON_LATENCY         (0x264)
#define CPC_MCUSYS_LATENCY             (0x268)
#define CPC_DORMANT_COUNTER            (0x270)
#define CPC_DORMANT_COUNTER_CLR        (0x274)
#define CPC_CPU_LATENCY(cpu)           ((CPC_CPU0_LATENCY) + 4 * (cpu))

/**************************************
 * system sram for apmcu debug
 * base address = 0x0011B000
 **************************************/
#define SYSRAM_CPUSYS_CNT              (0x1E8)
#define SYSRAM_MCUSYS_CNT              (0x1EC)
#define SYSRAM_CPC_CPUSYS_CNT_BACKUP   (0x1F0)
#define SYSRAM_MCUPM_MCUSYS_COUNTER    (0x1F4)
#define SYSRAM_CPU_ONLINE              (0x1F8)

/* Run time information. The registers may be cleared in db dump flow */
#define SYSRAM_RECENT_CPU_CNT(i)       (4 * (i) + 0x1B0)
#define SYSRAM_RECENT_CPUSYS_CNT       (0x1D0)
#define SYSRAM_RECENT_MCUSYS_CNT       (0x1D4)
#define SYSRAM_RECENT_CNT_TS_L         (0x1D8)
#define SYSRAM_RECENT_CNT_TS_H         (0x1DC)

#endif
