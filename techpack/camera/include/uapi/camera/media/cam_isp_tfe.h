/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __UAPI_CAM_ISP_TFE_H__
#define __UAPI_CAM_ISP_TFE_H__

/* TFE output port resource id number */
#define CAM_ISP_TFE_OUT_RES_BASE               0x1

#define CAM_ISP_TFE_OUT_RES_FULL               (CAM_ISP_TFE_OUT_RES_BASE + 0)
#define CAM_ISP_TFE_OUT_RES_RAW_DUMP           (CAM_ISP_TFE_OUT_RES_BASE + 1)
#define CAM_ISP_TFE_OUT_RES_PDAF               (CAM_ISP_TFE_OUT_RES_BASE + 2)
#define CAM_ISP_TFE_OUT_RES_RDI_0              (CAM_ISP_TFE_OUT_RES_BASE + 3)
#define CAM_ISP_TFE_OUT_RES_RDI_1              (CAM_ISP_TFE_OUT_RES_BASE + 4)
#define CAM_ISP_TFE_OUT_RES_RDI_2              (CAM_ISP_TFE_OUT_RES_BASE + 5)
#define CAM_ISP_TFE_OUT_RES_STATS_HDR_BE       (CAM_ISP_TFE_OUT_RES_BASE + 6)
#define CAM_ISP_TFE_OUT_RES_STATS_HDR_BHIST    (CAM_ISP_TFE_OUT_RES_BASE + 7)
#define CAM_ISP_TFE_OUT_RES_STATS_TL_BG        (CAM_ISP_TFE_OUT_RES_BASE + 8)
#define CAM_ISP_TFE_OUT_RES_STATS_BF           (CAM_ISP_TFE_OUT_RES_BASE + 9)
#define CAM_ISP_TFE_OUT_RES_STATS_AWB_BG       (CAM_ISP_TFE_OUT_RES_BASE + 10)
#define CAM_ISP_TFE_OUT_RES_MAX                (CAM_ISP_TFE_OUT_RES_BASE + 11)


/* TFE input port resource type */
#define CAM_ISP_TFE_IN_RES_BASE                 0x1

#define CAM_ISP_TFE_IN_RES_TPG                 (CAM_ISP_TFE_IN_RES_BASE + 0)
#define CAM_ISP_TFE_IN_RES_PHY_0               (CAM_ISP_TFE_IN_RES_BASE + 1)
#define CAM_ISP_TFE_IN_RES_PHY_1               (CAM_ISP_TFE_IN_RES_BASE + 2)
#define CAM_ISP_TFE_IN_RES_PHY_2               (CAM_ISP_TFE_IN_RES_BASE + 3)
#define CAM_ISP_TFE_IN_RES_PHY_3               (CAM_ISP_TFE_IN_RES_BASE + 4)
#define CAM_ISP_TFE_IN_RES_MAX                 (CAM_ISP_TFE_IN_RES_BASE + 5)

#endif /* __UAPI_CAM_ISP_TFE_H__ */
