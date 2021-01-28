/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __ECCCI_INTERNAL_OPTION__
#define __ECCCI_INTERNAL_OPTION__

/* platform info */
//#define MD_GENERATION       (6295)
//#define MD_PLATFORM_INFO    "6295"
//#define AP_PLATFORM_INFO    "MT6779"
#define CCCI_DRIVER_VER     0x20110118

/* flag to tell WDT is triggered by EPON or not, in MD SS debug region */
//#define CCCI_EE_OFFSET_EPON_MD1 (0x1C24)
#define CCCI_EE_OFFSET_EPON_MD3 (0x464)
/* flag to enable MD power off checking or not, in MD SS debug region */
#define CCCI_EE_OFFSET_EPOF_MD1 (7*1024+0x234)



#endif
