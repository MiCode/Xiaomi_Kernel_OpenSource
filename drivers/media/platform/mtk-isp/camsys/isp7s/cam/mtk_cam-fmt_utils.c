// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/align.h>
#include <media/v4l2-common.h>
#include <uapi/linux/media-bus-format.h>

#include <camsys/common/mtk_cam-fmt.h>

#include "mtk_cam-fmt_utils.h"
#include "mtk_cam-ipi_7_1.h"
#include "mtk_camera-videodev2.h"

void fill_ext_mtkcam_fmtdesc(struct v4l2_fmtdesc *f)
{
	const char *descr = NULL;
	const unsigned int sz = sizeof(f->description);

	switch (f->pixelformat) {
	case V4L2_PIX_FMT_YUYV10:
		descr = "YUYV 4:2:2 10 bits"; break;
	case V4L2_PIX_FMT_YVYU10:
		descr = "YVYU 4:2:2 10 bits"; break;
	case V4L2_PIX_FMT_UYVY10:
		descr = "UYVY 4:2:2 10 bits"; break;
	case V4L2_PIX_FMT_VYUY10:
		descr = "VYUY 4:2:2 10 bits"; break;
	case V4L2_PIX_FMT_NV12_10:
		descr = "Y/CbCr 4:2:0 10 bits"; break;
	case V4L2_PIX_FMT_NV21_10:
		descr = "Y/CrCb 4:2:0 10 bits"; break;
	case V4L2_PIX_FMT_NV16_10:
		descr = "Y/CbCr 4:2:2 10 bits"; break;
	case V4L2_PIX_FMT_NV61_10:
		descr = "Y/CrCb 4:2:2 10 bits"; break;
	case V4L2_PIX_FMT_NV12_12:
		descr = "Y/CbCr 4:2:0 12 bits"; break;
	case V4L2_PIX_FMT_NV21_12:
		descr = "Y/CrCb 4:2:0 12 bits"; break;
	case V4L2_PIX_FMT_NV16_12:
		descr = "Y/CbCr 4:2:2 12 bits"; break;
	case V4L2_PIX_FMT_NV61_12:
		descr = "Y/CrCb 4:2:2 12 bits"; break;
	case V4L2_PIX_FMT_MTISP_SBGGR10:
		descr = "10-bit Bayer BGGR MTISP Packed"; break;
	case V4L2_PIX_FMT_MTISP_SGBRG10:
		descr = "10-bit Bayer GBRG MTISP Packed"; break;
	case V4L2_PIX_FMT_MTISP_SGRBG10:
		descr = "10-bit Bayer GRBG MTISP Packed"; break;
	case V4L2_PIX_FMT_MTISP_SRGGB10:
		descr = "10-bit Bayer RGGB MTISP Packed"; break;
	case V4L2_PIX_FMT_MTISP_SBGGR12:
		descr = "12-bit Bayer BGGR MTISP Packed"; break;
	case V4L2_PIX_FMT_MTISP_SGBRG12:
		descr = "12-bit Bayer GBRG MTISP Packed"; break;
	case V4L2_PIX_FMT_MTISP_SGRBG12:
		descr = "12-bit Bayer GRBG MTISP Packed"; break;
	case V4L2_PIX_FMT_MTISP_SRGGB12:
		descr = "12-bit Bayer RGGB MTISP Packed"; break;
	case V4L2_PIX_FMT_MTISP_SBGGR14:
		descr = "14-bit Bayer BGGR MTISP Packed"; break;
	case V4L2_PIX_FMT_MTISP_SGBRG14:
		descr = "14-bit Bayer GBRG MTISP Packed"; break;
	case V4L2_PIX_FMT_MTISP_SGRBG14:
		descr = "14-bit Bayer GRBG MTISP Packed"; break;
	case V4L2_PIX_FMT_MTISP_SRGGB14:
		descr = "14-bit Bayer RGGB MTISP Packed"; break;
	case V4L2_PIX_FMT_MTISP_SBGGR8F:
		descr = "8-bit Enhanced BGGR Packed"; break;
	case V4L2_PIX_FMT_MTISP_SGBRG8F:
		descr = "8-bit Enhanced GBRG Packed"; break;
	case V4L2_PIX_FMT_MTISP_SGRBG8F:
		descr = "8-bit Enhanced GRBG Packed"; break;
	case V4L2_PIX_FMT_MTISP_SRGGB8F:
		descr = "8-bit Enhanced RGGB Packed"; break;
	case V4L2_PIX_FMT_MTISP_SBGGR10F:
		descr = "10-bit Enhanced BGGR Packed"; break;
	case V4L2_PIX_FMT_MTISP_SGBRG10F:
		descr = "10-bit Enhanced GBRG Packed"; break;
	case V4L2_PIX_FMT_MTISP_SGRBG10F:
		descr = "10-bit Enhanced GRBG Packed"; break;
	case V4L2_PIX_FMT_MTISP_SRGGB10F:
		descr = "10-bit Enhanced RGGB Packed"; break;
	case V4L2_PIX_FMT_MTISP_SBGGR12F:
		descr = "12-bit Enhanced BGGR Packed"; break;
	case V4L2_PIX_FMT_MTISP_SGBRG12F:
		descr = "12-bit Enhanced GBRG Packed"; break;
	case V4L2_PIX_FMT_MTISP_SGRBG12F:
		descr = "12-bit Enhanced GRBG Packed"; break;
	case V4L2_PIX_FMT_MTISP_SRGGB12F:
		descr = "12-bit Enhanced RGGB Packed"; break;
	case V4L2_PIX_FMT_MTISP_SBGGR14F:
		descr = "14-bit Enhanced BGGR Packed"; break;
	case V4L2_PIX_FMT_MTISP_SGBRG14F:
		descr = "14-bit Enhanced GBRG Packed"; break;
	case V4L2_PIX_FMT_MTISP_SGRBG14F:
		descr = "14-bit Enhanced GRBG Packed"; break;
	case V4L2_PIX_FMT_MTISP_SRGGB14F:
		descr = "14-bit Enhanced RGGB Packed"; break;
	case V4L2_PIX_FMT_MTISP_NV12_10P:
		descr = "Y/CbCr 4:2:0 10 bits packed"; break;
	case V4L2_PIX_FMT_MTISP_NV21_10P:
		descr = "Y/CrCb 4:2:0 10 bits packed"; break;
	case V4L2_PIX_FMT_MTISP_NV16_10P:
		descr = "Y/CbCr 4:2:2 10 bits packed"; break;
	case V4L2_PIX_FMT_MTISP_NV61_10P:
		descr = "Y/CrCb 4:2:2 10 bits packed"; break;
	case V4L2_PIX_FMT_MTISP_YUYV10P:
		descr = "YUYV 4:2:2 10 bits packed"; break;
	case V4L2_PIX_FMT_MTISP_YVYU10P:
		descr = "YVYU 4:2:2 10 bits packed"; break;
	case V4L2_PIX_FMT_MTISP_UYVY10P:
		descr = "UYVY 4:2:2 10 bits packed"; break;
	case V4L2_PIX_FMT_MTISP_VYUY10P:
		descr = "VYUY 4:2:2 10 bits packed"; break;
	case V4L2_PIX_FMT_MTISP_NV12_12P:
		descr = "Y/CbCr 4:2:0 12 bits packed"; break;
	case V4L2_PIX_FMT_MTISP_NV21_12P:
		descr = "Y/CrCb 4:2:0 12 bits packed"; break;
	case V4L2_PIX_FMT_MTISP_NV16_12P:
		descr = "Y/CbCr 4:2:2 12 bits packed"; break;
	case V4L2_PIX_FMT_MTISP_NV61_12P:
		descr = "Y/CrCb 4:2:2 12 bits packed"; break;
	case V4L2_PIX_FMT_MTISP_YUYV12P:
		descr = "YUYV 4:2:2 12 bits packed"; break;
	case V4L2_PIX_FMT_MTISP_YVYU12P:
		descr = "YVYU 4:2:2 12 bits packed"; break;
	case V4L2_PIX_FMT_MTISP_UYVY12P:
		descr = "UYVY 4:2:2 12 bits packed"; break;
	case V4L2_PIX_FMT_MTISP_VYUY12P:
		descr = "VYUY 4:2:2 12 bits packed"; break;
	case V4L2_PIX_FMT_MTISP_NV12_UFBC:
		descr = "YCbCr 420 8 bits compress"; break;
	case V4L2_PIX_FMT_MTISP_NV21_UFBC:
		descr = "YCrCb 420 8 bits compress"; break;
	case V4L2_PIX_FMT_MTISP_NV12_10_UFBC:
		descr = "YCbCr 420 10 bits compress"; break;
	case V4L2_PIX_FMT_MTISP_NV21_10_UFBC:
		descr = "YCrCb 420 10 bits compress"; break;
	case V4L2_PIX_FMT_MTISP_NV12_12_UFBC:
		descr = "YCbCr 420 12 bits compress"; break;
	case V4L2_PIX_FMT_MTISP_NV21_12_UFBC:
		descr = "YCrCb 420 12 bits compress"; break;
	case V4L2_PIX_FMT_MTISP_BAYER8_UFBC:
		descr = "RAW 8 bits compress"; break;
	case V4L2_PIX_FMT_MTISP_BAYER10_UFBC:
		descr = "RAW 10 bits compress"; break;
	case V4L2_PIX_FMT_MTISP_BAYER12_UFBC:
		descr = "RAW 12 bits compress"; break;
	case V4L2_PIX_FMT_MTISP_BAYER14_UFBC:
		descr = "RAW 14 bits compress"; break;
	case V4L2_META_FMT_MTISP_3A:
		descr = "AE/AWB Histogram"; break;
	case V4L2_META_FMT_MTISP_AF:
		descr = "AF Histogram"; break;
	case V4L2_META_FMT_MTISP_LCS:
		descr = "Local Contrast Enhancement Stat"; break;
	case V4L2_META_FMT_MTISP_LMV:
		descr = "Local Motion Vector Histogram"; break;
	case V4L2_META_FMT_MTISP_PARAMS:
		descr = "MTK ISP Tuning Metadata"; break;
	case V4L2_PIX_FMT_MTISP_SGRB8F:
		descr = "8-bit 3 plane GRB Packed"; break;
	case V4L2_PIX_FMT_MTISP_SGRB10F:
		descr = "10-bit 3 plane GRB Packed"; break;
	case V4L2_PIX_FMT_MTISP_SGRB12F:
		descr = "12-bit 3 plane GRB Packed"; break;
	default:
		pr_info("%s: not-found pixel format " FMT_FOURCC "\n",
			__func__, MEMBER_FOURCC(f->pixelformat));
		break;
	}

	if (descr)
		WARN_ON(strscpy(f->description, descr, sz) < 0);
}

/*
 * Note
 *	differt dma(fmt) would have different bus_size
 *	align xsize(bytes per line) with [bus_size * pixel_mode]
 */
static int mtk_cam_is_fullg(unsigned int ipi_fmt)
{
	return (ipi_fmt == MTKCAM_IPI_IMG_FMT_FG_BAYER8)
		|| (ipi_fmt == MTKCAM_IPI_IMG_FMT_FG_BAYER10)
		|| (ipi_fmt == MTKCAM_IPI_IMG_FMT_FG_BAYER12);
}

static inline
int mtk_cam_dma_bus_size(int bpp, int pixel_mode_shift, int is_fg)
{
	unsigned int bus_size = ALIGN(bpp, 16) << pixel_mode_shift;

	if (is_fg)
		bus_size <<= 1;
	return bus_size / 8; /* in bytes */
}

unsigned int mtk_cam_get_pixel_bits(unsigned int ipi_fmt)
{
	switch (ipi_fmt) {
	case MTKCAM_IPI_IMG_FMT_BAYER8:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER8:
		return 8;
	case MTKCAM_IPI_IMG_FMT_BAYER10:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER10:
	case MTKCAM_IPI_IMG_FMT_BAYER10_MIPI:
		return 10;
	case MTKCAM_IPI_IMG_FMT_BAYER12:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER12:
		return 12;
	case MTKCAM_IPI_IMG_FMT_BAYER14:
	case MTKCAM_IPI_IMG_FMT_UFBC_BAYER14:
		return 14;
	case MTKCAM_IPI_IMG_FMT_BAYER10_UNPACKED:
	case MTKCAM_IPI_IMG_FMT_BAYER12_UNPACKED:
	case MTKCAM_IPI_IMG_FMT_BAYER14_UNPACKED:
	case MTKCAM_IPI_IMG_FMT_BAYER16:
	case MTKCAM_IPI_IMG_FMT_YUYV:
	case MTKCAM_IPI_IMG_FMT_YVYU:
	case MTKCAM_IPI_IMG_FMT_UYVY:
	case MTKCAM_IPI_IMG_FMT_VYUY:
		return 16;
	case MTKCAM_IPI_IMG_FMT_Y8:
	case MTKCAM_IPI_IMG_FMT_YUV_422_2P:
	case MTKCAM_IPI_IMG_FMT_YVU_422_2P:
	case MTKCAM_IPI_IMG_FMT_YUV_422_3P:
	case MTKCAM_IPI_IMG_FMT_YVU_422_3P:
	case MTKCAM_IPI_IMG_FMT_YUV_420_2P:
	case MTKCAM_IPI_IMG_FMT_YVU_420_2P:
	case MTKCAM_IPI_IMG_FMT_YUV_420_3P:
	case MTKCAM_IPI_IMG_FMT_YVU_420_3P:
		return 8;
	case MTKCAM_IPI_IMG_FMT_YUYV_Y210:
	case MTKCAM_IPI_IMG_FMT_YVYU_Y210:
	case MTKCAM_IPI_IMG_FMT_UYVY_Y210:
	case MTKCAM_IPI_IMG_FMT_VYUY_Y210:
		return 32;
	case MTKCAM_IPI_IMG_FMT_YUV_P210:
	case MTKCAM_IPI_IMG_FMT_YVU_P210:
	case MTKCAM_IPI_IMG_FMT_YUV_P010:
	case MTKCAM_IPI_IMG_FMT_YVU_P010:
	case MTKCAM_IPI_IMG_FMT_YUV_P212:
	case MTKCAM_IPI_IMG_FMT_YVU_P212:
	case MTKCAM_IPI_IMG_FMT_YUV_P012:
	case MTKCAM_IPI_IMG_FMT_YVU_P012:
		return 16;
	case MTKCAM_IPI_IMG_FMT_YUYV_Y210_PACKED:
	case MTKCAM_IPI_IMG_FMT_YVYU_Y210_PACKED:
	case MTKCAM_IPI_IMG_FMT_UYVY_Y210_PACKED:
	case MTKCAM_IPI_IMG_FMT_VYUY_Y210_PACKED:
		return 20;
	case MTKCAM_IPI_IMG_FMT_YUV_P210_PACKED:
	case MTKCAM_IPI_IMG_FMT_YVU_P210_PACKED:
	case MTKCAM_IPI_IMG_FMT_YUV_P010_PACKED:
	case MTKCAM_IPI_IMG_FMT_YVU_P010_PACKED:
		return 10;
	case MTKCAM_IPI_IMG_FMT_YUV_P212_PACKED:
	case MTKCAM_IPI_IMG_FMT_YVU_P212_PACKED:
	case MTKCAM_IPI_IMG_FMT_YUV_P012_PACKED:
	case MTKCAM_IPI_IMG_FMT_YVU_P012_PACKED:
		return 12;
	case MTKCAM_IPI_IMG_FMT_RGB_8B_3P:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER8_3P:
	case MTKCAM_IPI_IMG_FMT_UFBC_NV12:
	case MTKCAM_IPI_IMG_FMT_UFBC_NV21:
	case MTKCAM_IPI_IMG_FMT_UFBC_BAYER8:
		return 8;
	case MTKCAM_IPI_IMG_FMT_RGB_10B_3P_PACKED:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER10_3P_PACKED:
	case MTKCAM_IPI_IMG_FMT_UFBC_YUV_P010:
	case MTKCAM_IPI_IMG_FMT_UFBC_YVU_P010:
	case MTKCAM_IPI_IMG_FMT_UFBC_BAYER10:
		return 10;
	case MTKCAM_IPI_IMG_FMT_RGB_12B_3P_PACKED:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER12_3P_PACKED:
	case MTKCAM_IPI_IMG_FMT_UFBC_YUV_P012:
	case MTKCAM_IPI_IMG_FMT_UFBC_YVU_P012:
	case MTKCAM_IPI_IMG_FMT_UFBC_BAYER12:
		return 12;
	case MTKCAM_IPI_IMG_FMT_RGB_10B_3P:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER10_3P:
	case MTKCAM_IPI_IMG_FMT_RGB_12B_3P:
	case MTKCAM_IPI_IMG_FMT_FG_BAYER12_3P:
		return 16;

	default:
		break;
	}
	pr_debug("not supported ipi-fmt 0x%08x", ipi_fmt);

	return -1;
}

unsigned int mtk_cam_get_img_fmt(unsigned int fourcc)
{
	switch (fourcc) {
	case V4L2_PIX_FMT_GREY:
		return MTKCAM_IPI_IMG_FMT_Y8;
	case V4L2_PIX_FMT_YUYV:
		return MTKCAM_IPI_IMG_FMT_YUYV;
	case V4L2_PIX_FMT_YVYU:
		return MTKCAM_IPI_IMG_FMT_YVYU;
	case V4L2_PIX_FMT_NV16:
		return MTKCAM_IPI_IMG_FMT_YUV_422_2P;
	case V4L2_PIX_FMT_NV61:
		return MTKCAM_IPI_IMG_FMT_YVU_422_2P;
	case V4L2_PIX_FMT_NV12:
		return MTKCAM_IPI_IMG_FMT_YUV_420_2P;
	case V4L2_PIX_FMT_NV21:
		return MTKCAM_IPI_IMG_FMT_YVU_420_2P;
	case V4L2_PIX_FMT_YUV422P:
		return MTKCAM_IPI_IMG_FMT_YUV_422_3P;
	case V4L2_PIX_FMT_YUV420:
		return MTKCAM_IPI_IMG_FMT_YUV_420_3P;
	case V4L2_PIX_FMT_YVU420:
		return MTKCAM_IPI_IMG_FMT_YVU_420_3P;
	case V4L2_PIX_FMT_NV12_10:
		return MTKCAM_IPI_IMG_FMT_YUV_P010;
	case V4L2_PIX_FMT_NV21_10:
		return MTKCAM_IPI_IMG_FMT_YVU_P010;
	case V4L2_PIX_FMT_NV16_10:
		return MTKCAM_IPI_IMG_FMT_YUV_P210;
	case V4L2_PIX_FMT_NV61_10:
		return MTKCAM_IPI_IMG_FMT_YVU_P210;
	case V4L2_PIX_FMT_MTISP_NV12_10P:
		return MTKCAM_IPI_IMG_FMT_YUV_P010_PACKED;
	case V4L2_PIX_FMT_MTISP_NV21_10P:
		return MTKCAM_IPI_IMG_FMT_YVU_P010_PACKED;
	case V4L2_PIX_FMT_MTISP_NV16_10P:
		return MTKCAM_IPI_IMG_FMT_YUV_P210_PACKED;
	case V4L2_PIX_FMT_MTISP_NV61_10P:
		return MTKCAM_IPI_IMG_FMT_YVU_P210_PACKED;
	case V4L2_PIX_FMT_YUYV10:
		return MTKCAM_IPI_IMG_FMT_YUYV_Y210;
	case V4L2_PIX_FMT_YVYU10:
		return MTKCAM_IPI_IMG_FMT_YVYU_Y210;
	case V4L2_PIX_FMT_UYVY10:
		return MTKCAM_IPI_IMG_FMT_UYVY_Y210;
	case V4L2_PIX_FMT_VYUY10:
		return MTKCAM_IPI_IMG_FMT_VYUY_Y210;
	case V4L2_PIX_FMT_MTISP_YUYV10P:
		return MTKCAM_IPI_IMG_FMT_YUYV_Y210_PACKED;
	case V4L2_PIX_FMT_MTISP_YVYU10P:
		return MTKCAM_IPI_IMG_FMT_YVYU_Y210_PACKED;
	case V4L2_PIX_FMT_MTISP_UYVY10P:
		return MTKCAM_IPI_IMG_FMT_UYVY_Y210_PACKED;
	case V4L2_PIX_FMT_MTISP_VYUY10P:
		return MTKCAM_IPI_IMG_FMT_VYUY_Y210_PACKED;
	case V4L2_PIX_FMT_NV12_12:
		return MTKCAM_IPI_IMG_FMT_YUV_P012;
	case V4L2_PIX_FMT_NV21_12:
		return MTKCAM_IPI_IMG_FMT_YVU_P012;
	case V4L2_PIX_FMT_NV16_12:
		return MTKCAM_IPI_IMG_FMT_YUV_P212;
	case V4L2_PIX_FMT_NV61_12:
		return MTKCAM_IPI_IMG_FMT_YVU_P212;
	case V4L2_PIX_FMT_MTISP_NV12_12P:
		return MTKCAM_IPI_IMG_FMT_YUV_P012_PACKED;
	case V4L2_PIX_FMT_MTISP_NV21_12P:
		return MTKCAM_IPI_IMG_FMT_YVU_P012_PACKED;
	case V4L2_PIX_FMT_MTISP_NV16_12P:
		return MTKCAM_IPI_IMG_FMT_YUV_P212_PACKED;
	case V4L2_PIX_FMT_MTISP_NV61_12P:
		return MTKCAM_IPI_IMG_FMT_YVU_P212_PACKED;
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
		return MTKCAM_IPI_IMG_FMT_BAYER8;
	case V4L2_PIX_FMT_MTISP_SBGGR8F:
	case V4L2_PIX_FMT_MTISP_SGBRG8F:
	case V4L2_PIX_FMT_MTISP_SGRBG8F:
	case V4L2_PIX_FMT_MTISP_SRGGB8F:
		return MTKCAM_IPI_IMG_FMT_FG_BAYER8;
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
		return MTKCAM_IPI_IMG_FMT_BAYER10_UNPACKED;
	case V4L2_PIX_FMT_SBGGR10P:
	case V4L2_PIX_FMT_SGBRG10P:
	case V4L2_PIX_FMT_SGRBG10P:
	case V4L2_PIX_FMT_SRGGB10P:
		return MTKCAM_IPI_IMG_FMT_BAYER10_MIPI;
	case V4L2_PIX_FMT_MTISP_SBGGR10:
	case V4L2_PIX_FMT_MTISP_SGBRG10:
	case V4L2_PIX_FMT_MTISP_SGRBG10:
	case V4L2_PIX_FMT_MTISP_SRGGB10:
		return MTKCAM_IPI_IMG_FMT_BAYER10;
	case V4L2_PIX_FMT_MTISP_SBGGR10F:
	case V4L2_PIX_FMT_MTISP_SGBRG10F:
	case V4L2_PIX_FMT_MTISP_SGRBG10F:
	case V4L2_PIX_FMT_MTISP_SRGGB10F:
		return MTKCAM_IPI_IMG_FMT_FG_BAYER10;
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
		return MTKCAM_IPI_IMG_FMT_BAYER12_UNPACKED;
	case V4L2_PIX_FMT_MTISP_SBGGR12:
	case V4L2_PIX_FMT_MTISP_SGBRG12:
	case V4L2_PIX_FMT_MTISP_SGRBG12:
	case V4L2_PIX_FMT_MTISP_SRGGB12:
		return MTKCAM_IPI_IMG_FMT_BAYER12;
	case V4L2_PIX_FMT_MTISP_SBGGR12F:
	case V4L2_PIX_FMT_MTISP_SGBRG12F:
	case V4L2_PIX_FMT_MTISP_SGRBG12F:
	case V4L2_PIX_FMT_MTISP_SRGGB12F:
		return MTKCAM_IPI_IMG_FMT_FG_BAYER12;
	case V4L2_PIX_FMT_SBGGR14:
	case V4L2_PIX_FMT_SGBRG14:
	case V4L2_PIX_FMT_SGRBG14:
	case V4L2_PIX_FMT_SRGGB14:
		return MTKCAM_IPI_IMG_FMT_BAYER14_UNPACKED;
	case V4L2_PIX_FMT_MTISP_SBGGR14:
	case V4L2_PIX_FMT_MTISP_SGBRG14:
	case V4L2_PIX_FMT_MTISP_SGRBG14:
	case V4L2_PIX_FMT_MTISP_SRGGB14:
		return MTKCAM_IPI_IMG_FMT_BAYER14;
	case V4L2_PIX_FMT_MTISP_SBGGR14F:
	case V4L2_PIX_FMT_MTISP_SGBRG14F:
	case V4L2_PIX_FMT_MTISP_SGRBG14F:
	case V4L2_PIX_FMT_MTISP_SRGGB14F:
		return MTKCAM_IPI_IMG_FMT_FG_BAYER14;
	case V4L2_PIX_FMT_SBGGR16:
	case V4L2_PIX_FMT_SGBRG16:
	case V4L2_PIX_FMT_SGRBG16:
	case V4L2_PIX_FMT_SRGGB16:
		return MTKCAM_IPI_IMG_FMT_BAYER16;
	case V4L2_PIX_FMT_MTISP_NV12_UFBC:
		return MTKCAM_IPI_IMG_FMT_UFBC_NV12;
	case V4L2_PIX_FMT_MTISP_NV21_UFBC:
		return MTKCAM_IPI_IMG_FMT_UFBC_NV21;
	case V4L2_PIX_FMT_MTISP_NV12_10_UFBC:
		return MTKCAM_IPI_IMG_FMT_UFBC_YUV_P010;
	case V4L2_PIX_FMT_MTISP_NV21_10_UFBC:
		return MTKCAM_IPI_IMG_FMT_UFBC_YVU_P010;
	case V4L2_PIX_FMT_MTISP_NV12_12_UFBC:
		return MTKCAM_IPI_IMG_FMT_UFBC_YUV_P012;
	case V4L2_PIX_FMT_MTISP_NV21_12_UFBC:
		return MTKCAM_IPI_IMG_FMT_UFBC_YVU_P012;
	case V4L2_PIX_FMT_MTISP_BAYER8_UFBC:
		return MTKCAM_IPI_IMG_FMT_UFBC_BAYER8;
	case V4L2_PIX_FMT_MTISP_BAYER10_UFBC:
		return MTKCAM_IPI_IMG_FMT_UFBC_BAYER10;
	case V4L2_PIX_FMT_MTISP_BAYER12_UFBC:
		return MTKCAM_IPI_IMG_FMT_UFBC_BAYER12;
	case V4L2_PIX_FMT_MTISP_BAYER14_UFBC:
		return MTKCAM_IPI_IMG_FMT_UFBC_BAYER14;
	case V4L2_PIX_FMT_MTISP_SGRB8F:
		return MTKCAM_IPI_IMG_FMT_FG_BAYER8_3P;
	case V4L2_PIX_FMT_MTISP_SGRB10F:
		return MTKCAM_IPI_IMG_FMT_FG_BAYER10_3P_PACKED;
	case V4L2_PIX_FMT_MTISP_SGRB12F:
		return MTKCAM_IPI_IMG_FMT_FG_BAYER12_3P_PACKED;
	default:
		return MTKCAM_IPI_IMG_FMT_UNKNOWN;
	}
}

int mtk_cam_dmao_xsize(int w, unsigned int ipi_fmt, int pixel_mode_shift)
{
	const int is_fg		= mtk_cam_is_fullg(ipi_fmt);
	const int bpp		= mtk_cam_get_pixel_bits(ipi_fmt);
	const int bytes		= is_fg ?
		DIV_ROUND_UP(w * bpp * 3 / 2, 8) : DIV_ROUND_UP(w * bpp, 8);
	const int bus_size	= mtk_cam_dma_bus_size(bpp, pixel_mode_shift, is_fg);

	return ALIGN(bytes, bus_size);
}

int is_yuv_ufo(unsigned int pixelformat)
{
	switch (pixelformat) {
	case V4L2_PIX_FMT_MTISP_NV12_UFBC:
	case V4L2_PIX_FMT_MTISP_NV21_UFBC:
	case V4L2_PIX_FMT_MTISP_NV12_10_UFBC:
	case V4L2_PIX_FMT_MTISP_NV21_10_UFBC:
	case V4L2_PIX_FMT_MTISP_NV12_12_UFBC:
	case V4L2_PIX_FMT_MTISP_NV21_12_UFBC:
		return 1;
	default:
		return 0;
	}
}

int is_raw_ufo(unsigned int pixelformat)
{
	switch (pixelformat) {
	case V4L2_PIX_FMT_MTISP_BAYER8_UFBC:
	case V4L2_PIX_FMT_MTISP_BAYER10_UFBC:
	case V4L2_PIX_FMT_MTISP_BAYER12_UFBC:
	case V4L2_PIX_FMT_MTISP_BAYER14_UFBC:
		return 1;
	default:
		return 0;
	}
}

int is_fullg_rb(unsigned int pixelformat)
{
	switch (pixelformat) {
	case V4L2_PIX_FMT_MTISP_SGRB8F:
	case V4L2_PIX_FMT_MTISP_SGRB10F:
	case V4L2_PIX_FMT_MTISP_SGRB12F:
		return 1;
	default:
		return 0;
	}
}

const struct mtk_format_info *mtk_format_info(u32 format)
{
	static const struct mtk_format_info formats[] = {
		/* RAW formats - packed */
		/* bayer8, packed */
		{ .format = V4L2_PIX_FMT_SBGGR8,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 8, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bus_align = 2 * 8 /* 8p */ },
		{ .format = V4L2_PIX_FMT_SGBRG8,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 8, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bus_align = 2 * 8 /* 8p */ },
		{ .format = V4L2_PIX_FMT_SGRBG8,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 8, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bus_align = 2 * 8 /* 8p */ },
		{ .format = V4L2_PIX_FMT_SRGGB8,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 8, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bus_align = 2 * 8 /* 8p */ },
		/* bayer10, packed */
		{ .format = V4L2_PIX_FMT_MTISP_SBGGR10,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 10, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bus_align = 2 * 8 /* 8p */ },
		{ .format = V4L2_PIX_FMT_MTISP_SGBRG10,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 10, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bus_align = 2 * 8 /* 8p */ },
		{ .format = V4L2_PIX_FMT_MTISP_SGRBG10,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 10, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bus_align = 2 * 8 /* 8p */ },
		{ .format = V4L2_PIX_FMT_MTISP_SRGGB10,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 10, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bus_align = 2 * 8 /* 8p */ },
		/* bayer12, packed */
		{ .format = V4L2_PIX_FMT_MTISP_SBGGR12,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 12, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bus_align = 2 * 8 /* 8p */ },
		{ .format = V4L2_PIX_FMT_MTISP_SGBRG12,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 12, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bus_align = 2 * 8 /* 8p */ },
		{ .format = V4L2_PIX_FMT_MTISP_SGRBG12,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 12, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bus_align = 2 * 8 /* 8p */ },
		{ .format = V4L2_PIX_FMT_MTISP_SRGGB12,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 12, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bus_align = 2 * 8 /* 8p */ },
		/* bayer14, packed */
		{ .format = V4L2_PIX_FMT_MTISP_SBGGR14,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 14, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bus_align = 2 * 8 /* 8p */ },
		{ .format = V4L2_PIX_FMT_MTISP_SGBRG14,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 14, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bus_align = 2 * 8 /* 8p */ },
		{ .format = V4L2_PIX_FMT_MTISP_SGRBG14,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 14, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bus_align = 2 * 8 /* 8p */ },
		{ .format = V4L2_PIX_FMT_MTISP_SRGGB14,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 14, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bus_align = 2 * 8 /* 8p */ },
		/* bayer16, packed/unpacked */
		{ .format = V4L2_PIX_FMT_SBGGR16,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 16, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bus_align = 2 * 8 /* 8p */ },
		{ .format = V4L2_PIX_FMT_SGBRG16,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 16, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bus_align = 2 * 8 /* 8p */ },
		{ .format = V4L2_PIX_FMT_SGRBG16,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 16, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bus_align = 2 * 8 /* 8p */ },
		{ .format = V4L2_PIX_FMT_SRGGB16,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 16, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bus_align = 2 * 8 /* 8p */ },
		/* bayer10, mipi */
		{ .format = V4L2_PIX_FMT_SBGGR10P,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 10, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bus_align = 2 * 8 /* 8p */ },
		{ .format = V4L2_PIX_FMT_SGBRG10P,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 10, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bus_align = 2 * 8 /* 8p */ },
		{ .format = V4L2_PIX_FMT_SGRBG10P,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 10, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bus_align = 2 * 8 /* 8p */ },
		{ .format = V4L2_PIX_FMT_SRGGB10P,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 10, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bus_align = 2 * 8 /* 8p */ },

		/* RAW formats - unpacked */

		/* note: v4l2 supports bayer10 unpack */
		/* note: v4l2 supports bayer12 unpack */

		/* bayer14, unpacked */
		{ .format = V4L2_PIX_FMT_MTISP_SBGGR14,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 16, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bus_align = 2 * 8 /* 8p */ },
		{ .format = V4L2_PIX_FMT_MTISP_SGBRG14,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 16, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bus_align = 2 * 8 /* 8p */ },
		{ .format = V4L2_PIX_FMT_MTISP_SGRBG14,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 16, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bus_align = 2 * 8 /* 8p */ },
		{ .format = V4L2_PIX_FMT_MTISP_SRGGB14,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 16, 0, 0, 0 }, .hdiv = 1, .vdiv = 1,
			.bus_align = 2 * 8 /* 8p */ },

		/* note: FG_BAYERx is out-dated. */

		/* YUV planar formats */
		{ .format = V4L2_PIX_FMT_NV12_10,  .mem_planes = 1, .comp_planes = 2,
			.bitpp = { 16, 32, 0, 0 }, .hdiv = 2, .vdiv = 2,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_NV21_10,  .mem_planes = 1, .comp_planes = 2,
			.bitpp = { 16, 32, 0, 0 }, .hdiv = 2, .vdiv = 2,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_NV16_10,  .mem_planes = 1, .comp_planes = 2,
			.bitpp = { 16, 32, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_NV61_10,  .mem_planes = 1, .comp_planes = 2,
			.bitpp = { 16, 32, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_YUYV10, .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 32, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_YVYU10, .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 32, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_UYVY10, .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 32, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_VYUY10, .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 32, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_NV12_12, .mem_planes = 1, .comp_planes = 2,
			.bitpp = { 16, 32, 0, 0 }, .hdiv = 2, .vdiv = 2,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_NV21_12, .mem_planes = 1, .comp_planes = 2,
			.bitpp = { 16, 32, 0, 0 }, .hdiv = 2, .vdiv = 2,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_NV16_12, .mem_planes = 1, .comp_planes = 2,
			.bitpp = { 16, 32, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_NV61_12, .mem_planes = 1, .comp_planes = 2,
			.bitpp = { 16, 32, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_YUYV12, .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 32, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_YVYU12, .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 32, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_UYVY12, .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 32, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_VYUY12, .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 32, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bus_align = 4 },
		/* YUV packed formats */
		{ .format = V4L2_PIX_FMT_MTISP_YUYV10P, .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 20, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_YVYU10P, .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 20, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_UYVY10P, .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 20, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_VYUY10P, .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 20, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_NV12_10P, .mem_planes = 1, .comp_planes = 2,
			.bitpp = { 10, 20, 0, 0 }, .hdiv = 2, .vdiv = 2,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_NV21_10P, .mem_planes = 1, .comp_planes = 2,
			.bitpp = { 10, 20, 0, 0 }, .hdiv = 2, .vdiv = 2,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_NV16_10P, .mem_planes = 1, .comp_planes = 2,
			.bitpp = { 10, 20, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_NV61_10P, .mem_planes = 1, .comp_planes = 2,
			.bitpp = { 10, 20, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_YUYV12P,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 24, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_YVYU12P,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 24, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_UYVY12P,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 24, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_VYUY12P,  .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 24, 0, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_NV12_12P, .mem_planes = 1, .comp_planes = 2,
			.bitpp = { 12, 24, 0, 0 }, .hdiv = 2, .vdiv = 2,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_NV21_12P, .mem_planes = 1, .comp_planes = 2,
			.bitpp = { 12, 24, 0, 0 }, .hdiv = 2, .vdiv = 2,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_NV16_12P, .mem_planes = 1, .comp_planes = 2,
			.bitpp = { 12, 24, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_NV61_12P, .mem_planes = 1, .comp_planes = 2,
			.bitpp = { 12, 24, 0, 0 }, .hdiv = 2, .vdiv = 1,
			.bus_align = 4 },
		/* YUV UFBC formats */
		{ .format = V4L2_PIX_FMT_MTISP_NV12_UFBC, .mem_planes = 1, .comp_planes = 2,
			.bitpp = { 8, 16, 0, 0 }, .hdiv = 2, .vdiv = 2,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_NV21_UFBC, .mem_planes = 1, .comp_planes = 2,
			.bitpp = { 8, 16, 0, 0 }, .hdiv = 2, .vdiv = 2,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_NV12_10_UFBC, .mem_planes = 1, .comp_planes = 2,
			.bitpp = { 10, 20, 0, 0 }, .hdiv = 2, .vdiv = 2,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_NV21_10_UFBC, .mem_planes = 1, .comp_planes = 2,
			.bitpp = { 10, 20, 0, 0 }, .hdiv = 2, .vdiv = 2,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_NV12_12_UFBC, .mem_planes = 1, .comp_planes = 2,
			.bitpp = { 12, 24, 0, 0 }, .hdiv = 2, .vdiv = 2,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_NV21_12_UFBC, .mem_planes = 1, .comp_planes = 2,
			.bitpp = { 12, 24, 0, 0 }, .hdiv = 2, .vdiv = 2,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_BAYER8_UFBC, .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 8, 0, 0, 0 }, .hdiv = 1, .vdiv = 1, },
		{ .format = V4L2_PIX_FMT_MTISP_BAYER10_UFBC, .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 10, 0, 0, 0 }, .hdiv = 1, .vdiv = 1, },
		{ .format = V4L2_PIX_FMT_MTISP_BAYER12_UFBC, .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 12, 0, 0, 0 }, .hdiv = 1, .vdiv = 1, },
		{ .format = V4L2_PIX_FMT_MTISP_BAYER14_UFBC, .mem_planes = 1, .comp_planes = 1,
			.bitpp = { 16, 0, 0, 0 }, .hdiv = 1, .vdiv = 1, },
		/* Full-G RGB formats */
		{ .format = V4L2_PIX_FMT_MTISP_SGRB8F, .mem_planes = 1, .comp_planes = 3,
			.bitpp = { 8, 8, 8, 0 }, .hdiv = 2, .vdiv = 2,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_SGRB10F, .mem_planes = 1, .comp_planes = 3,
			.bitpp = { 10, 10, 10, 0 }, .hdiv = 2, .vdiv = 2,
			.bus_align = 4 },
		{ .format = V4L2_PIX_FMT_MTISP_SGRB12F, .mem_planes = 1, .comp_planes = 3,
			.bitpp = { 12, 12, 12, 0 }, .hdiv = 2, .vdiv = 2,
			.bus_align = 4 },
	};
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(formats); ++i)
		if (formats[i].format == format)
			return &formats[i];
	return NULL;
}

unsigned int mtk_format_calc_stride(const struct mtk_format_info *info,
				    unsigned int i,
				    unsigned int w,
				    unsigned int stride0 /* may be 0 */)
{
	unsigned int hdiv = (i == 0) ? 1 : info->hdiv;
	unsigned int bit = info->bitpp[i];
	unsigned int stride;

	stride = DIV_ROUND_UP(DIV_ROUND_UP(w, hdiv) * bit, 8);

	if (info->bus_align)
		stride = ALIGN(stride, info->bus_align);

	if (i == 0)
		return max(stride, stride0);

	return max(stride,
		   /* infer stride from plane 0's stride */
		   DIV_ROUND_UP(stride0 * bit / info->bitpp[0], hdiv));
}

unsigned int mtk_format_calc_planesize(const struct mtk_format_info *info,
				       unsigned int i,
				       unsigned int h,
				       unsigned int stride)
{
	unsigned int vdiv = (i == 0) ? 1 : info->vdiv;

	return stride * DIV_ROUND_UP(h, vdiv);
}

unsigned int v4l2_format_calc_stride(const struct v4l2_format_info *info,
				     unsigned int i,
				     unsigned int w,
				     unsigned int stride0 /* may be 0 */)
{
	unsigned int hdiv = (i == 0) ? 1 : info->hdiv;
	unsigned int bpp = info->bpp[i];
	unsigned int stride;

	stride = DIV_ROUND_UP(w, hdiv) * bpp;

	/* wdma constraint: align 4 bytes for all yuv formats */
	if (v4l2_is_format_yuv(info))
		stride = ALIGN(stride, 4);

	if (i == 0)
		return max(stride, stride0);

	return max(stride,
		   /* infer stride from plane 0's stride */
		   DIV_ROUND_UP(stride0 * bpp / info->bpp[0], hdiv));
}

unsigned int v4l2_format_calc_planesize(const struct v4l2_format_info *info,
				       unsigned int i,
				       unsigned int h,
				       unsigned int stride)
{
	unsigned int vdiv = (i == 0) ? 1 : info->vdiv;

	return stride * DIV_ROUND_UP(h, vdiv);
}

unsigned int sensor_mbus_to_ipi_fmt(unsigned int mbus_code)
{
	unsigned int fmt = MTKCAM_IPI_IMG_FMT_UNKNOWN;

	switch (mbus_code & SENSOR_FMT_MASK) {
	case MEDIA_BUS_FMT_SBGGR8_1X8:
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_SGRBG8_1X8:
	case MEDIA_BUS_FMT_SRGGB8_1X8:
		return MTKCAM_IPI_IMG_FMT_BAYER8;
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
		return MTKCAM_IPI_IMG_FMT_BAYER10;
	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SRGGB12_1X12:
		return MTKCAM_IPI_IMG_FMT_BAYER12;
	case MEDIA_BUS_FMT_SBGGR14_1X14:
	case MEDIA_BUS_FMT_SGBRG14_1X14:
	case MEDIA_BUS_FMT_SGRBG14_1X14:
	case MEDIA_BUS_FMT_SRGGB14_1X14:
		return MTKCAM_IPI_IMG_FMT_BAYER14;
	default:
		break;
	}

	/* may fail */
	//if (WARN_ON(fmt == MTKCAM_IPI_IMG_FMT_UNKNOWN))
	//    pr_info("Unsupported fmt 0x%08x\n", mbus_code);

	return fmt;
}

unsigned int sensor_mbus_to_ipi_pixel_id(unsigned int mbus_code)
{
	unsigned int pxl_id = MTKCAM_IPI_BAYER_PXL_ID_UNKNOWN;

	switch (mbus_code & SENSOR_FMT_MASK) {
	case MEDIA_BUS_FMT_SBGGR8_1X8:
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SBGGR14_1X14:
		return MTKCAM_IPI_BAYER_PXL_ID_B;
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGBRG14_1X14:
		return MTKCAM_IPI_BAYER_PXL_ID_GB;
	case MEDIA_BUS_FMT_SGRBG8_1X8:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SGRBG14_1X14:
		return MTKCAM_IPI_BAYER_PXL_ID_GR;
	case MEDIA_BUS_FMT_SRGGB8_1X8:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
	case MEDIA_BUS_FMT_SRGGB12_1X12:
	case MEDIA_BUS_FMT_SRGGB14_1X14:
		return MTKCAM_IPI_BAYER_PXL_ID_R;
	default:
		break;
	}

	if (WARN_ON(pxl_id == MTKCAM_IPI_BAYER_PXL_ID_UNKNOWN))
		pr_info("Unsupported fmt 0x%08x\n", mbus_code);

	return pxl_id;
}

