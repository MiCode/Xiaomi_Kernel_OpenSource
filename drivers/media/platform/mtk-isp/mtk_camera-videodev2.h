/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MTK_CAMERA_VIDEODEV2_H
#define _MTK_CAMERA_VIDEODEV2_H

#include <linux/videodev2.h>

/**
 * E V E N T
 */
#define V4L2_EVENT_REQUEST_DRAINED (V4L2_EVENT_PRIVATE_START + 1)
#define V4L2_EVENT_REQUEST_DUMPED  (V4L2_EVENT_PRIVATE_START + 2)
#define V4L2_EVENT_ESD_RECOVERY  (V4L2_EVENT_PRIVATE_START + 3)
#define V4L2_EVENT_REQUEST_SENSOR_TRIGGER  (V4L2_EVENT_PRIVATE_START + 4)
#define V4L2_EVENT_ERROR  (V4L2_EVENT_PRIVATE_START + 5)
#define V4L2_EVENT_EXTISP_CAMSYS_READY  (V4L2_EVENT_PRIVATE_START + 6)

struct mtk_cam_event_sensor_trigger {
	__u32	tg_cnt;
	__u32	sensor_seq;
	__u32	reserved[14];
};


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
#define V4L2_PIX_FMT_MTISP_SBGGR22  v4l2_fourcc('M', 'B', 'B', 'M')
#define V4L2_PIX_FMT_MTISP_SGBRG22  v4l2_fourcc('M', 'B', 'G', 'M')
#define V4L2_PIX_FMT_MTISP_SGRBG22  v4l2_fourcc('M', 'B', 'g', 'M')
#define V4L2_PIX_FMT_MTISP_SRGGB22  v4l2_fourcc('M', 'B', 'R', 'M')
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
#define V4L2_META_FMT_MTISP_PARAMS      v4l2_fourcc('M', 'T', 'f', 'p')
#define V4L2_META_FMT_MTISP_PARAMS_RGBW v4l2_fourcc('M', 'T', 'p', 'r')
#define V4L2_META_FMT_MTISP_3A          v4l2_fourcc('M', 'T', 'f', 'a')
#define V4L2_META_FMT_MTISP_3A_RGBW     v4l2_fourcc('M', 'T', 'a', 'r')
#define V4L2_META_FMT_MTISP_AF          v4l2_fourcc('M', 'T', 'f', 'f')
#define V4L2_META_FMT_MTISP_AF_RGBW     v4l2_fourcc('M', 'T', 'f', 'r')
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
#define V4L2_PIX_FMT_MTISP_NV12_UFBC v4l2_fourcc('1', '2', '8', 'F')
#define V4L2_PIX_FMT_MTISP_NV21_UFBC v4l2_fourcc('2', '1', '8', 'F')
#define V4L2_PIX_FMT_MTISP_NV12_10_UFBC v4l2_fourcc('1', '2', 'A', 'F')
#define V4L2_PIX_FMT_MTISP_NV21_10_UFBC v4l2_fourcc('2', '1', 'A', 'F')
#define V4L2_PIX_FMT_MTISP_NV12_12_UFBC v4l2_fourcc('1', '2', 'C', 'F')
#define V4L2_PIX_FMT_MTISP_NV21_12_UFBC v4l2_fourcc('2', '1', 'C', 'F')
#define V4L2_PIX_FMT_MTISP_BAYER8_UFBC v4l2_fourcc('M', 'B', '8', 'U')
#define V4L2_PIX_FMT_MTISP_BAYER10_UFBC v4l2_fourcc('M', 'B', 'A', 'U')
#define V4L2_PIX_FMT_MTISP_BAYER12_UFBC v4l2_fourcc('M', 'B', 'C', 'U')
#define V4L2_PIX_FMT_MTISP_BAYER14_UFBC v4l2_fourcc('M', 'B', 'E', 'U')
#define V4L2_PIX_FMT_MTISP_SGRB8F  v4l2_fourcc('M', 'F', '8', 'P')
#define V4L2_PIX_FMT_MTISP_SGRB10F  v4l2_fourcc('M', 'F', 'A', 'P')
#define V4L2_PIX_FMT_MTISP_SGRB12F  v4l2_fourcc('M', 'F', 'C', 'P')
/* MTK 4 plane RGB, 8bit, BGGR */
#define V4L2_PIX_FMT_MTISP_PLANAR_BGGR_8  v4l2_fourcc('M', 'T', '8', 'B')
#define V4L2_PIX_FMT_MTISP_PLANAR_GBRG_8  v4l2_fourcc('M', 'T', '8', 'g')
#define V4L2_PIX_FMT_MTISP_PLANAR_GRBG_8  v4l2_fourcc('M', 'T', '8', 'G')
#define V4L2_PIX_FMT_MTISP_PLANAR_RGGB_8  v4l2_fourcc('M', 'T', '8', 'R')
/* MTK 4 plane RGB, 10bit, BGGR */
#define V4L2_PIX_FMT_MTISP_PLANAR_BGGR_10  v4l2_fourcc('M', 'T', 'A', 'B')
#define V4L2_PIX_FMT_MTISP_PLANAR_GBRG_10  v4l2_fourcc('M', 'T', 'A', 'g')
#define V4L2_PIX_FMT_MTISP_PLANAR_GRBG_10  v4l2_fourcc('M', 'T', 'A', 'G')
#define V4L2_PIX_FMT_MTISP_PLANAR_RGGB_10  v4l2_fourcc('M', 'T', 'A', 'R')
/* MTK 4 plane RGB, 12bit, BGGR */
#define V4L2_PIX_FMT_MTISP_PLANAR_BGGR_12  v4l2_fourcc('M', 'T', 'C', 'B')
#define V4L2_PIX_FMT_MTISP_PLANAR_GBRG_12  v4l2_fourcc('M', 'T', 'C', 'g')
#define V4L2_PIX_FMT_MTISP_PLANAR_GRBG_12  v4l2_fourcc('M', 'T', 'C', 'G')
#define V4L2_PIX_FMT_MTISP_PLANAR_RGGB_12  v4l2_fourcc('M', 'T', 'C', 'R')
/* MTK 4 plane RGB Packed, 10bit, BGGR */
#define V4L2_PIX_FMT_MTISP_PLANAR_BGGR_10P  v4l2_fourcc('M', 'T', 'a', 'B')
#define V4L2_PIX_FMT_MTISP_PLANAR_GBRG_10P  v4l2_fourcc('M', 'T', 'a', 'g')
#define V4L2_PIX_FMT_MTISP_PLANAR_GRBG_10P  v4l2_fourcc('M', 'T', 'a', 'G')
#define V4L2_PIX_FMT_MTISP_PLANAR_RGGB_10P  v4l2_fourcc('M', 'T', 'a', 'R')
/* MTK 4 plane RGB Packed, 12bit, BGGR */
#define V4L2_PIX_FMT_MTISP_PLANAR_BGGR_12P  v4l2_fourcc('M', 'T', 'c', 'B')
#define V4L2_PIX_FMT_MTISP_PLANAR_GBRG_12P  v4l2_fourcc('M', 'T', 'c', 'g')
#define V4L2_PIX_FMT_MTISP_PLANAR_GRBG_12P  v4l2_fourcc('M', 'T', 'c', 'G')
#define V4L2_PIX_FMT_MTISP_PLANAR_RGGB_12P  v4l2_fourcc('M', 'T', 'c', 'R')

/* To be upstream */
#define V4L2_BUF_FLAG_TIMESTAMP_BOOT		0x00008000

#endif /* _MTK_CAMERA_VIDEODEV2_H */
