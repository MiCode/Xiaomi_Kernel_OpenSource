/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __LINUX_MTK_MEDIA_H__
#define __LINUX_MTK_MEDIA_H__

#include <linux/videodev2.h>
/**
 * Workaround before we check the suitable interfaces
 * in community.
 */

#define V4L2_EVENT_REQUEST_DRAINED (V4L2_EVENT_PRIVATE_START + 1)
#define V4L2_EVENT_REQUEST_DUMPED  (V4L2_EVENT_PRIVATE_START + 2)

#define V4L2_CID_USER_MTK_CAM_BASE		(V4L2_CID_USER_BASE + 0x10d0)

#define V4L2_CID_MTK_CAM_USED_ENGINE_LIMIT \
	(V4L2_CID_USER_MTK_CAM_BASE + 1)
#define V4L2_CID_MTK_CAM_BIN_LIMIT \
	(V4L2_CID_USER_MTK_CAM_BASE + 2)
#define V4L2_CID_MTK_CAM_FRZ_LIMIT \
	(V4L2_CID_USER_MTK_CAM_BASE + 3)
#define V4L2_CID_MTK_CAM_RESOURCE_PLAN_POLICY \
	(V4L2_CID_USER_MTK_CAM_BASE + 4)
#define V4L2_CID_MTK_CAM_USED_ENGINE \
	(V4L2_CID_USER_MTK_CAM_BASE + 5)
#define V4L2_CID_MTK_CAM_BIN \
	(V4L2_CID_USER_MTK_CAM_BASE + 6)
#define V4L2_CID_MTK_CAM_FRZ \
	(V4L2_CID_USER_MTK_CAM_BASE + 7)
#define V4L2_CID_MTK_CAM_USED_ENGINE_TRY \
	(V4L2_CID_USER_MTK_CAM_BASE + 8)
#define V4L2_CID_MTK_CAM_BIN_TRY \
	(V4L2_CID_USER_MTK_CAM_BASE + 9)
#define V4L2_CID_MTK_CAM_FRZ_TRY \
	(V4L2_CID_USER_MTK_CAM_BASE + 10)
#define V4L2_CID_MTK_CAM_PIXEL_RATE \
	(V4L2_CID_USER_MTK_CAM_BASE + 11)
#define V4L2_CID_MTK_CAM_FEATURE \
	(V4L2_CID_USER_MTK_CAM_BASE + 12)
#define V4L2_CID_MTK_CAM_SYNC_ID \
	(V4L2_CID_USER_MTK_CAM_BASE + 13)
#define V4L2_CID_MTK_CAM_RAW_PATH_SELECT \
	(V4L2_CID_USER_MTK_CAM_BASE + 14)

/* Allowed value of V4L2_CID_MTK_CAM_RAW_PATH_SELECT */
#define V4L2_MTK_CAM_RAW_PATH_SELECT_LSC	1

#define V4L2_PIX_FMT_MTISP_SBGGR10  v4l2_fourcc('M', 'B', 'B', 'A')
#define V4L2_PIX_FMT_MTISP_SGBRG10  v4l2_fourcc('M', 'B', 'G', 'A')
#define V4L2_PIX_FMT_MTISP_SGRBG10  v4l2_fourcc('M', 'B', 'g', 'A')
#define V4L2_PIX_FMT_MTISP_SRGGB10  v4l2_fourcc('M', 'B', 'R', 'A')
#define V4L2_PIX_FMT_MTISP_SBGGR12  v4l2_fourcc('M', 'B', 'B', 'C')
#define V4L2_PIX_FMT_MTISP_SGBRG12  v4l2_fourcc('M', 'B', 'G', 'C')
#define V4L2_PIX_FMT_MTISP_SGRBG12  v4l2_fourcc('M', 'B', 'g', 'C')
#define V4L2_PIX_FMT_MTISP_SRGGB12  v4l2_fourcc('M', 'B', 'R', 'C')
#define V4L2_PIX_FMT_MTISP_SBGGR14  v4l2_fourcc('M', 'B', 'B', 'E')
#define V4L2_PIX_FMT_MTISP_SGBRG14  v4l2_fourcc('M', 'B', 'G', 'E')
#define V4L2_PIX_FMT_MTISP_SGRBG14  v4l2_fourcc('M', 'B', 'g', 'E')
#define V4L2_PIX_FMT_MTISP_SRGGB14  v4l2_fourcc('M', 'B', 'R', 'E')
#define V4L2_PIX_FMT_MTISP_SBGGR8F   v4l2_fourcc('M', 'F', 'B', '8')
#define V4L2_PIX_FMT_MTISP_SGBRG8F   v4l2_fourcc('M', 'F', 'G', '8')
#define V4L2_PIX_FMT_MTISP_SGRBG8F   v4l2_fourcc('M', 'F', 'g', '8')
#define V4L2_PIX_FMT_MTISP_SRGGB8F   v4l2_fourcc('M', 'F', 'R', '8')
#define V4L2_PIX_FMT_MTISP_SBGGR10F  v4l2_fourcc('M', 'F', 'B', 'A')
#define V4L2_PIX_FMT_MTISP_SGBRG10F  v4l2_fourcc('M', 'F', 'G', 'A')
#define V4L2_PIX_FMT_MTISP_SGRBG10F  v4l2_fourcc('M', 'F', 'g', 'A')
#define V4L2_PIX_FMT_MTISP_SRGGB10F  v4l2_fourcc('M', 'F', 'R', 'A')
#define V4L2_PIX_FMT_MTISP_SBGGR12F  v4l2_fourcc('M', 'F', 'B', 'C')
#define V4L2_PIX_FMT_MTISP_SGBRG12F  v4l2_fourcc('M', 'F', 'G', 'C')
#define V4L2_PIX_FMT_MTISP_SGRBG12F  v4l2_fourcc('M', 'F', 'g', 'C')
#define V4L2_PIX_FMT_MTISP_SRGGB12F  v4l2_fourcc('M', 'F', 'R', 'C')
#define V4L2_PIX_FMT_MTISP_SBGGR14F  v4l2_fourcc('M', 'F', 'B', 'E')
#define V4L2_PIX_FMT_MTISP_SGBRG14F  v4l2_fourcc('M', 'F', 'G', 'E')
#define V4L2_PIX_FMT_MTISP_SGRBG14F  v4l2_fourcc('M', 'F', 'g', 'E')
#define V4L2_PIX_FMT_MTISP_SRGGB14F  v4l2_fourcc('M', 'F', 'R', 'E')
#define V4L2_META_FMT_MTISP_PARAMS v4l2_fourcc('M', 'T', 'f', 'p')
#define V4L2_META_FMT_MTISP_3A     v4l2_fourcc('M', 'T', 'f', 'a')
#define V4L2_META_FMT_MTISP_AF     v4l2_fourcc('M', 'T', 'f', 'f')
#define V4L2_META_FMT_MTISP_LCS    v4l2_fourcc('M', 'T', 'f', 'c')
#define V4L2_META_FMT_MTISP_LMV    v4l2_fourcc('M', 'T', 'f', 'm')
#define V4L2_PIX_FMT_YUYV10  v4l2_fourcc('Y', 'U', 'Y', 'A')
#define V4L2_PIX_FMT_YVYU10  v4l2_fourcc('Y', 'V', 'Y', 'A')
#define V4L2_PIX_FMT_UYVY10  v4l2_fourcc('U', 'Y', 'V', 'A')
#define V4L2_PIX_FMT_VYUY10  v4l2_fourcc('V', 'Y', 'U', 'A')
#define V4L2_PIX_FMT_YUYV12  v4l2_fourcc('Y', 'U', 'Y', 'C')
#define V4L2_PIX_FMT_YVYU12  v4l2_fourcc('Y', 'V', 'Y', 'C')
#define V4L2_PIX_FMT_UYVY12  v4l2_fourcc('U', 'Y', 'V', 'C')
#define V4L2_PIX_FMT_VYUY12  v4l2_fourcc('V', 'Y', 'U', 'C')
#define V4L2_PIX_FMT_NV12_10 v4l2_fourcc('1', '2', 'A', 'U')
#define V4L2_PIX_FMT_NV21_10 v4l2_fourcc('2', '1', 'A', 'U')
#define V4L2_PIX_FMT_NV16_10 v4l2_fourcc('1', '6', 'A', 'U')
#define V4L2_PIX_FMT_NV61_10 v4l2_fourcc('6', '1', 'A', 'U')
#define V4L2_PIX_FMT_NV12_12 v4l2_fourcc('1', '2', 'C', 'U')
#define V4L2_PIX_FMT_NV21_12 v4l2_fourcc('2', '1', 'C', 'U')
#define V4L2_PIX_FMT_NV16_12 v4l2_fourcc('1', '6', 'C', 'U')
#define V4L2_PIX_FMT_NV61_12 v4l2_fourcc('6', '1', 'C', 'U')

#define V4L2_PIX_FMT_MTISP_YUYV10P v4l2_fourcc('Y', 'U', 'A', 'P')
#define V4L2_PIX_FMT_MTISP_YVYU10P v4l2_fourcc('Y', 'V', 'A', 'P')
#define V4L2_PIX_FMT_MTISP_UYVY10P v4l2_fourcc('U', 'Y', 'A', 'P')
#define V4L2_PIX_FMT_MTISP_VYUY10P v4l2_fourcc('V', 'Y', 'A', 'P')
#define V4L2_PIX_FMT_MTISP_YUYV12P v4l2_fourcc('Y', 'U', 'C', 'P')
#define V4L2_PIX_FMT_MTISP_YVYU12P v4l2_fourcc('Y', 'V', 'C', 'P')
#define V4L2_PIX_FMT_MTISP_UYVY12P v4l2_fourcc('U', 'Y', 'C', 'P')
#define V4L2_PIX_FMT_MTISP_VYUY12P v4l2_fourcc('V', 'Y', 'C', 'P')
#define V4L2_PIX_FMT_MTISP_NV12_10P v4l2_fourcc('1', '2', 'A', 'P')
#define V4L2_PIX_FMT_MTISP_NV21_10P v4l2_fourcc('2', '1', 'A', 'P')
#define V4L2_PIX_FMT_MTISP_NV16_10P v4l2_fourcc('1', '6', 'A', 'P')
#define V4L2_PIX_FMT_MTISP_NV61_10P v4l2_fourcc('6', '1', 'A', 'P')
#define V4L2_PIX_FMT_MTISP_NV12_12P v4l2_fourcc('1', '2', 'C', 'P')
#define V4L2_PIX_FMT_MTISP_NV21_12P v4l2_fourcc('2', '1', 'C', 'P')
#define V4L2_PIX_FMT_MTISP_NV16_12P v4l2_fourcc('1', '6', 'C', 'P')
#define V4L2_PIX_FMT_MTISP_NV61_12P v4l2_fourcc('6', '1', 'C', 'P')
#endif
