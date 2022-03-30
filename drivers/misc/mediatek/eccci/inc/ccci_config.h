/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __ECCCI_INTERNAL_OPTION__
#define __ECCCI_INTERNAL_OPTION__

/* platform info */
#define MD_GENERATION       (6297)
//#define CCCI_DRIVER_VER     0x20110118

#define MT6297

#define _HW_REORDER_SW_WORKAROUND_
//#define CCCI_GEN98_LRO_NEW_FEATURE

//#define ENABLE_CPU_AFFINITY
#define REFINE_BAT_OFFSET_REMOVE
#define PIT_USING_CACHE_MEM

//#define CCCI_LOG_LEVEL  CCCI_LOG_ALL_UART
#define USING_PM_RUNTIME

/* AMMS DRDI bank4 share memory size */
#define BANK4_DRDI_SMEM_SIZE (64*1024)
#endif
