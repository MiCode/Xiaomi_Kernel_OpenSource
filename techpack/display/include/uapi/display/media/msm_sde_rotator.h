/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __UAPI_MSM_SDE_ROTATOR_H__
#define __UAPI_MSM_SDE_ROTATOR_H__

#include <linux/videodev2.h>
#include <linux/types.h>
#include <linux/ioctl.h>

/* SDE Rotator pixel format definitions */
#define SDE_PIX_FMT_XRGB_8888 \
	v4l2_fourcc('X', 'R', '2', '4') /* 32  BGRX-8-8-8-8  */
#define SDE_PIX_FMT_ARGB_8888 \
	v4l2_fourcc('A', 'R', '2', '4') /* 32  BGRA-8-8-8-8  */
#define SDE_PIX_FMT_ABGR_8888 \
	v4l2_fourcc('R', 'A', '2', '4') /* 32-bit ABGR 8:8:8:8 */
#define SDE_PIX_FMT_RGBA_8888 \
	v4l2_fourcc('A', 'B', '2', '4') /* 32-bit RGBA 8:8:8:8 */
#define SDE_PIX_FMT_BGRA_8888 \
	v4l2_fourcc('B', 'A', '2', '4') /* 32  ARGB-8-8-8-8  */
#define SDE_PIX_FMT_RGBX_8888 \
	v4l2_fourcc('X', 'B', '2', '4') /* 32-bit RGBX 8:8:8:8 */
#define SDE_PIX_FMT_BGRX_8888 \
	v4l2_fourcc('B', 'X', '2', '4') /* 32  XRGB-8-8-8-8  */
#define SDE_PIX_FMT_XBGR_8888 \
	v4l2_fourcc('R', 'X', '2', '4') /* 32-bit XBGR 8:8:8:8 */
#define SDE_PIX_FMT_RGBA_5551 \
	v4l2_fourcc('R', 'A', '1', '5') /* 16-bit RGBA 5:5:5:1 */
#define SDE_PIX_FMT_ARGB_1555 \
	v4l2_fourcc('A', 'R', '1', '5') /* 16  ARGB-1-5-5-5  */
#define SDE_PIX_FMT_ABGR_1555 \
	v4l2_fourcc('A', 'B', '1', '5') /* 16-bit ABGR 1:5:5:5 */
#define SDE_PIX_FMT_BGRA_5551 \
	v4l2_fourcc('B', 'A', '1', '5') /* 16-bit BGRA 5:5:5:1 */
#define SDE_PIX_FMT_BGRX_5551 \
	v4l2_fourcc('B', 'X', '1', '5') /* 16-bit BGRX 5:5:5:1 */
#define SDE_PIX_FMT_RGBX_5551 \
	v4l2_fourcc('R', 'X', '1', '5') /* 16-bit RGBX 5:5:5:1 */
#define SDE_PIX_FMT_XBGR_1555 \
	v4l2_fourcc('X', 'B', '1', '5') /* 16-bit XBGR 1:5:5:5 */
#define SDE_PIX_FMT_XRGB_1555 \
	v4l2_fourcc('X', 'R', '1', '5') /* 16  XRGB-1-5-5-5  */
#define SDE_PIX_FMT_ARGB_4444 \
	v4l2_fourcc('A', 'R', '1', '2') /* 16  aaaarrrr ggggbbbb */
#define SDE_PIX_FMT_RGBA_4444 \
	v4l2_fourcc('R', 'A', '1', '2') /* 16-bit RGBA 4:4:4:4 */
#define SDE_PIX_FMT_BGRA_4444 \
	v4l2_fourcc('b', 'A', '1', '2') /* 16-bit BGRA 4:4:4:4 */
#define SDE_PIX_FMT_ABGR_4444 \
	v4l2_fourcc('A', 'B', '1', '2') /* 16-bit ABGR 4:4:4:4 */
#define SDE_PIX_FMT_RGBX_4444 \
	v4l2_fourcc('R', 'X', '1', '2') /* 16-bit RGBX 4:4:4:4 */
#define SDE_PIX_FMT_XRGB_4444 \
	v4l2_fourcc('X', 'R', '1', '2') /* 16  xxxxrrrr ggggbbbb */
#define SDE_PIX_FMT_BGRX_4444 \
	v4l2_fourcc('B', 'X', '1', '2') /* 16-bit BGRX 4:4:4:4 */
#define SDE_PIX_FMT_XBGR_4444 \
	v4l2_fourcc('X', 'B', '1', '2') /* 16-bit XBGR 4:4:4:4 */
#define SDE_PIX_FMT_RGB_888 \
	v4l2_fourcc('R', 'G', 'B', '3') /* 24  RGB-8-8-8     */
#define SDE_PIX_FMT_BGR_888 \
	v4l2_fourcc('B', 'G', 'R', '3') /* 24  BGR-8-8-8     */
#define SDE_PIX_FMT_RGB_565 \
	v4l2_fourcc('R', 'G', 'B', 'P') /* 16  RGB-5-6-5     */
#define SDE_PIX_FMT_BGR_565 \
	v4l2_fourcc('B', 'G', '1', '6') /* 16-bit BGR 5:6:5 */
#define SDE_PIX_FMT_Y_CB_CR_H2V2 \
	v4l2_fourcc('Y', 'U', '1', '2') /* 12  YUV 4:2:0     */
#define SDE_PIX_FMT_Y_CR_CB_H2V2 \
	v4l2_fourcc('Y', 'V', '1', '2') /* 12  YVU 4:2:0     */
#define SDE_PIX_FMT_Y_CR_CB_GH2V2 \
	v4l2_fourcc('Y', 'U', '4', '2') /* Planar YVU 4:2:0 A16 */
#define SDE_PIX_FMT_Y_CBCR_H2V2 \
	v4l2_fourcc('N', 'V', '1', '2') /* 12  Y/CbCr 4:2:0  */
#define SDE_PIX_FMT_Y_CRCB_H2V2 \
	v4l2_fourcc('N', 'V', '2', '1') /* 12  Y/CrCb 4:2:0  */
#define SDE_PIX_FMT_Y_CBCR_H1V2 \
	v4l2_fourcc('N', 'H', '1', '6') /* Y/CbCr 4:2:2 */
#define SDE_PIX_FMT_Y_CRCB_H1V2 \
	v4l2_fourcc('N', 'H', '6', '1') /* Y/CrCb 4:2:2 */
#define SDE_PIX_FMT_Y_CBCR_H2V1 \
	v4l2_fourcc('N', 'V', '1', '6') /* 16  Y/CbCr 4:2:2  */
#define SDE_PIX_FMT_Y_CRCB_H2V1 \
	v4l2_fourcc('N', 'V', '6', '1') /* 16  Y/CrCb 4:2:2  */
#define SDE_PIX_FMT_YCBYCR_H2V1 \
	v4l2_fourcc('Y', 'U', 'Y', 'V') /* 16  YUV 4:2:2     */
#define SDE_PIX_FMT_Y_CBCR_H2V2_VENUS \
	v4l2_fourcc('Q', 'N', 'V', '2') /* Y/CbCr 4:2:0 Venus */
#define SDE_PIX_FMT_Y_CRCB_H2V2_VENUS \
	v4l2_fourcc('Q', 'N', 'V', '1') /* Y/CrCb 4:2:0 Venus */
#define SDE_PIX_FMT_RGBA_8888_UBWC \
	v4l2_fourcc('Q', 'R', 'G', 'B') /* RGBA 8:8:8:8 UBWC */
#define SDE_PIX_FMT_RGBX_8888_UBWC \
	v4l2_fourcc('Q', 'X', 'B', '4') /* RGBX 8:8:8:8 UBWC */
#define SDE_PIX_FMT_RGB_565_UBWC \
	v4l2_fourcc('Q', 'R', 'G', '6') /* RGB 5:6:5 UBWC */
#define SDE_PIX_FMT_Y_CBCR_H2V2_UBWC \
	v4l2_fourcc('Q', '1', '2', '8') /* UBWC 8-bit Y/CbCr 4:2:0 */
#define SDE_PIX_FMT_RGBA_1010102 \
	v4l2_fourcc('A', 'B', '3', '0') /* RGBA 10:10:10:2 */
#define SDE_PIX_FMT_RGBX_1010102 \
	v4l2_fourcc('X', 'B', '3', '0') /* RGBX 10:10:10:2 */
#define SDE_PIX_FMT_ARGB_2101010 \
	v4l2_fourcc('A', 'R', '3', '0') /* ARGB 2:10:10:10 */
#define SDE_PIX_FMT_XRGB_2101010 \
	v4l2_fourcc('X', 'R', '3', '0') /* XRGB 2:10:10:10 */
#define SDE_PIX_FMT_BGRA_1010102 \
	v4l2_fourcc('B', 'A', '3', '0') /* BGRA 10:10:10:2 */
#define SDE_PIX_FMT_BGRX_1010102 \
	v4l2_fourcc('B', 'X', '3', '0') /* BGRX 10:10:10:2 */
#define SDE_PIX_FMT_ABGR_2101010 \
	v4l2_fourcc('R', 'A', '3', '0') /* ABGR 2:10:10:10 */
#define SDE_PIX_FMT_XBGR_2101010 \
	v4l2_fourcc('R', 'X', '3', '0') /* XBGR 2:10:10:10 */
#define SDE_PIX_FMT_RGBA_1010102_UBWC \
	v4l2_fourcc('Q', 'R', 'B', 'A') /* RGBA 10:10:10:2 UBWC */
#define SDE_PIX_FMT_RGBX_1010102_UBWC \
	v4l2_fourcc('Q', 'X', 'B', 'A') /* RGBX 10:10:10:2 UBWC */
#define SDE_PIX_FMT_Y_CBCR_H2V2_P010 \
	v4l2_fourcc('P', '0', '1', '0') /* Y/CbCr 4:2:0 P10 */
#define SDE_PIX_FMT_Y_CBCR_H2V2_P010_VENUS \
	v4l2_fourcc('Q', 'P', '1', '0') /* Y/CbCr 4:2:0 P10 Venus*/
#define SDE_PIX_FMT_Y_CBCR_H2V2_TP10 \
	v4l2_fourcc('T', 'P', '1', '0') /* Y/CbCr 4:2:0 TP10 */
#define SDE_PIX_FMT_Y_CBCR_H2V2_TP10_UBWC \
	v4l2_fourcc('Q', '1', '2', 'A') /* UBWC Y/CbCr 4:2:0 TP10 */
#define SDE_PIX_FMT_Y_CBCR_H2V2_P010_UBWC \
	v4l2_fourcc('Q', '1', '2', 'B') /* UBWC Y/CbCr 4:2:0 P10 */

/*
 * struct msm_sde_rotator_fence - v4l2 buffer fence info
 * @index: id number of the buffer
 * @type: enum v4l2_buf_type; buffer type
 * @fd: file descriptor of the fence associated with this buffer
 */
struct msm_sde_rotator_fence {
	__u32	index;
	__u32	type;
	__s32	fd;
	__u32	reserved[5];
};

/*
 * struct msm_sde_rotator_comp_ratio - v4l2 buffer compression ratio
 * @index: id number of the buffer
 * @type: enum v4l2_buf_type; buffer type
 * @numer: numerator of the ratio
 * @denom: denominator of the ratio
 */
struct msm_sde_rotator_comp_ratio {
	__u32	index;
	__u32	type;
	__u32	numer;
	__u32	denom;
	__u32	reserved[4];
};

/* SDE Rotator private ioctl ID */
#define VIDIOC_G_SDE_ROTATOR_FENCE \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 10, struct msm_sde_rotator_fence)
#define VIDIOC_S_SDE_ROTATOR_FENCE \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 11, struct msm_sde_rotator_fence)
#define VIDIOC_G_SDE_ROTATOR_COMP_RATIO \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 12, struct msm_sde_rotator_comp_ratio)
#define VIDIOC_S_SDE_ROTATOR_COMP_RATIO \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 13, struct msm_sde_rotator_comp_ratio)

/* SDE Rotator private control ID's */
#define V4L2_CID_SDE_ROTATOR_SECURE	(V4L2_CID_USER_BASE + 0x1000)

/*
 * This control Id indicates this context is associated with the
 * secure camera.
 */
#define V4L2_CID_SDE_ROTATOR_SECURE_CAMERA	(V4L2_CID_USER_BASE + 0x2000)

#endif /* __UAPI_MSM_SDE_ROTATOR_H__ */
