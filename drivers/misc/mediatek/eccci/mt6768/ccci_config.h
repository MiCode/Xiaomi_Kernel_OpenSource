/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __ECCCI_INTERNAL_OPTION__
#define __ECCCI_INTERNAL_OPTION__

/* platform info */
#define MD_GENERATION       (6293)
#define MD_PLATFORM_INFO    "6293"
#define AP_PLATFORM_INFO    "MT6768"
#define CCCI_DRIVER_VER     0x20110118

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
#define CCCI_EE_OFFSET_EPON_MD1 (0x1C24)
#define CCCI_EE_OFFSET_EPON_MD3 (0x464)
/* flag to enable MD power off checking or not, in MD SS debug region */
#define CCCI_EE_OFFSET_EPOF_MD1 (7*1024+0x234)
#define CCCI_EE_SMEM_TOTAL_SIZE (64*1024)
#define CCCI_SMEM_SIZE_RUNTIME_AP (0x800) /* AP runtime data size */
#define CCCI_SMEM_SIZE_RUNTIME_MD (0x800) /* MD runtime data size */
#define CCCI_SMEM_OFFSET_SEQERR (0x34) /* in MD CCCI debug region */
#define CCCI_SMEM_SIZE_DBM (160)
#define CCCI_SMEM_SIZE_DBM_GUARD (8)

#define IPC_L4C_MSG_ID_LEN   (0x40)

/* feature option, always try using platform info first! */
#define FEATURE_SCP_CCCI_SUPPORT
/*#define ENABLE_EMI_PROTECTION*/
#define ENABLE_32K_CLK_LESS
/*#define CCCI_LOG_LEVEL     1*/

#endif
