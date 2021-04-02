/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_PLAT_SYSSRAM_REG_H__
#define __MTK_PLAT_SYSSRAM_REG_H__


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
