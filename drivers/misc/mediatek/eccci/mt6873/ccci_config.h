/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __ECCCI_INTERNAL_OPTION__
#define __ECCCI_INTERNAL_OPTION__

/* platform info */
#define MD_GENERATION       (6297)
#define MD_PLATFORM_INFO    "6297"
#define AP_PLATFORM_INFO    "MT6873"
#define CCCI_DRIVER_VER     0x20110118

#define MT6297
#define _97_REORDER_BAT_PAGE_TABLE_

/* flag to tell WDT is triggered by EPON or not, in MD SS debug region */
#define CCCI_EE_OFFSET_EPON_MD1 (0x2844)
#define CCCI_EE_OFFSET_EPON_MD3 (0x464)
/* flag to enable MD power off checking or not, in MD SS debug region */
#define CCCI_EE_OFFSET_EPOF_MD1 (0x2840)

#define _HW_REORDER_SW_WORKAROUND_
//#define ENABLE_CPU_AFFINITY
#define REFINE_BAT_OFFSET_REMOVE
//#define PIT_USING_CACHE_MEM

#define CCCI_USE_DFD_OFFSET_0

//#define CCCI_LOG_LEVEL  CCCI_LOG_ALL_UART
#define USING_PM_RUNTIME
#endif
