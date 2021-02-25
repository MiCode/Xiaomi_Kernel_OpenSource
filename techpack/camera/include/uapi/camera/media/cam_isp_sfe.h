/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef __UAPI_CAM_ISP_SFE_H__
#define __UAPI_CAM_ISP_SFE_H__

/* SFE output port resource type */
#define CAM_ISP_SFE_OUT_RES_BASE              0x6000

#define CAM_ISP_SFE_OUT_RES_RDI_0             (CAM_ISP_SFE_OUT_RES_BASE + 0)
#define CAM_ISP_SFE_OUT_RES_RDI_1             (CAM_ISP_SFE_OUT_RES_BASE + 1)
#define CAM_ISP_SFE_OUT_RES_RDI_2             (CAM_ISP_SFE_OUT_RES_BASE + 2)
#define CAM_ISP_SFE_OUT_RES_RDI_3             (CAM_ISP_SFE_OUT_RES_BASE + 3)
#define CAM_ISP_SFE_OUT_RES_RDI_4             (CAM_ISP_SFE_OUT_RES_BASE + 4)
#define CAM_ISP_SFE_OUT_BE_STATS_0            (CAM_ISP_SFE_OUT_RES_BASE + 5)
#define CAM_ISP_SFE_OUT_BHIST_STATS_0         (CAM_ISP_SFE_OUT_RES_BASE + 6)
#define CAM_ISP_SFE_OUT_BE_STATS_1            (CAM_ISP_SFE_OUT_RES_BASE + 7)
#define CAM_ISP_SFE_OUT_BHIST_STATS_1         (CAM_ISP_SFE_OUT_RES_BASE + 8)
#define CAM_ISP_SFE_OUT_BE_STATS_2            (CAM_ISP_SFE_OUT_RES_BASE + 9)
#define CAM_ISP_SFE_OUT_BHIST_STATS_2         (CAM_ISP_SFE_OUT_RES_BASE + 10)
#define CAM_ISP_SFE_OUT_RES_LCR               (CAM_ISP_SFE_OUT_RES_BASE + 11)
#define CAM_ISP_SFE_OUT_RES_RAW_DUMP          (CAM_ISP_SFE_OUT_RES_BASE + 12)

#define CAM_ISP_SFE_OUT_RES_MAX               (CAM_ISP_SFE_OUT_RES_BASE + 13)

/* SFE input port resource type */
#define CAM_ISP_SFE_IN_RES_BASE               0x5000

#define CAM_ISP_SFE_INLINE_PIX                (CAM_ISP_SFE_IN_RES_BASE + 0)
#define CAM_ISP_SFE_IN_RD_0                   (CAM_ISP_SFE_IN_RES_BASE + 1)
#define CAM_ISP_SFE_IN_RD_1                   (CAM_ISP_SFE_IN_RES_BASE + 2)
#define CAM_ISP_SFE_IN_RD_2                   (CAM_ISP_SFE_IN_RES_BASE + 3)
#define CAM_ISP_SFE_IN_RES_MAX                (CAM_ISP_SFE_IN_RES_BASE + 4)

#endif /* __UAPI_CAM_ISP_SFE_H__ */
