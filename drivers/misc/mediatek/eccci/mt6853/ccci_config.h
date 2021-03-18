/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef __ECCCI_INTERNAL_OPTION__
#define __ECCCI_INTERNAL_OPTION__

/* platform info */
#define MD_GENERATION       (6297)
#define MD_PLATFORM_INFO    "6297"
#ifdef CCCI_PLATFORM_MT6877
#define AP_PLATFORM_INFO    "MT6877"
#else
#define AP_PLATFORM_INFO    "MT6853"
#endif
#define CCCI_DRIVER_VER     0x20110118
#define MT6297
#define _97_REORDER_BAT_PAGE_TABLE_

/* buffer management customization */
#define CCCI_MTU            (3584-128)
#define CCCI_NET_MTU        (1500)
#define SKB_POOL_SIZE_4K    (256)
#define SKB_POOL_SIZE_1_5K  (256)
#define SKB_POOL_SIZE_16    (64)
#define BM_POOL_SIZE  \
(SKB_POOL_SIZE_4K+SKB_POOL_SIZE_1_5K+SKB_POOL_SIZE_16)
/*reload pool if pool size dropped below 1/RELOAD_TH */
#define RELOAD_TH           (3)

/* EE dump cunstomization */
#define CCCI_EE_SIZE_CCIF_SRAM (72) /* SRAM size we dump into smem */
/* CCIF dump offset in MD SS debug region */
#define CCCI_EE_OFFSET_CCIF_SRAM (1024 - CCCI_EE_SIZE_CCIF_SRAM)
/* flag to tell WDT is triggered by EPON or not, in MD SS debug region */
#define CCCI_EE_OFFSET_EPON_MD1 (0x2844)
#define CCCI_EE_OFFSET_EPON_MD3 (0x464)
/* flag to enable MD power off checking or not, in MD SS debug region */
#define CCCI_EE_OFFSET_EPOF_MD1 (0x2840)
#define CCCI_EE_SMEM_TOTAL_SIZE (64*1024)
#define CCCI_SMEM_SIZE_RUNTIME_AP (0x800) /* AP runtime data size */
#define CCCI_SMEM_SIZE_RUNTIME_MD (0x800) /* MD runtime data size */
#define CCCI_SMEM_OFFSET_SEQERR (0x34) /* in MD CCCI debug region */
#define CCCI_SMEM_SIZE_DBM (160)
#define CCCI_SMEM_SIZE_DBM_GUARD (8)

#define IPC_L4C_MSG_ID_LEN   (0x40)

/* feature option, always try using platform info first! */
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
#ifdef CCCI_PLATFORM_MT6877
#define FEATURE_SCP_CCCI_SUPPORT
#endif
#endif
/*#define ENABLE_EMI_PROTECTION*/
/* #define FEATURE_LOW_BATTERY_SUPPORT */
#define ENABLE_32K_CLK_LESS

#define HW_CHECK_SUM_ENABLE
#define HW_FRG_FEATURE_ENABLE
#ifdef HW_FRG_FEATURE_ENABLE
/* #define FRG_FEATURE_TEST */
#ifdef FRG_FEATURE_TEST
/* 1024 + 2432 = 3456 */
#define DPMAIF_PKT_SIZE      (128*8) /* == 1024 */
#define DPMAIF_FRG_SIZE      (128*19) /* 2432 */
#else
/* 1664 + 1920 = 3584 (> 3* 1024)+8 */
#define DPMAIF_PKT_SIZE      (128*13) /* == 1664 */
#define DPMAIF_FRG_SIZE      (128*15) /* 1920  */
#endif
#else
#define DPMAIF_PKT_SIZE      (128*28) /* 3584 ==SKB_4K */
#define DPMAIF_FRG_SIZE      (128) /* =_=, no used */
#endif
#define _HW_REORDER_SW_WORKAROUND_

/*#define CCCI_LOG_LEVEL     1*/
#define FEATURE_CLK_BUF
/*#define DPMAIF_DEBUG_LOG*/
#define DEBUG_FOR_CCB
#define ENABLE_CPU_AFFINITY
#define REFINE_BAT_OFFSET_REMOVE
#define PIT_USING_CACHE_MEM
#define USING_TX_DONE_KERNEL_THREAD

#define CCCI_USE_DFD_OFFSET_0


#endif
