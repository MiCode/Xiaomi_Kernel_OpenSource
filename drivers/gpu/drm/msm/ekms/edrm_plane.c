/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "edrm_plane.h"
#include "edrm_crtc.h"
#include "sde_kms.h"
#include "edrm_kms.h"

/* SDE_SSPP_SRC */
#define SSPP_SRC_SIZE                      0x00
#define SSPP_SRC_XY                        0x08
#define SSPP_OUT_SIZE                      0x0c
#define SSPP_OUT_XY                        0x10
#define SSPP_SRC0_ADDR                     0x14
#define SSPP_SRC1_ADDR                     0x18
#define SSPP_SRC2_ADDR                     0x1C
#define SSPP_SRC3_ADDR                     0x20
#define SSPP_SRC_YSTRIDE0                  0x24
#define SSPP_SRC_YSTRIDE1                  0x28
#define SSPP_SRC_FORMAT                    0x30
#define SSPP_SRC_UNPACK_PATTERN            0x34
#define SSPP_SRC_OP_MODE                   0x38
#define SSPP_CONSTANT_COLOR                0x3c
#define PIPE_SW_PIX_EXT_C0_LR              0x100
#define PIPE_SW_PIX_EXT_C0_TB              0x104
#define PIPE_SW_PIXEL_EXT_C0_REQ           0x108
#define PIPE_SW_PIX_EXT_C1C2_LR            0x110
#define PIPE_SW_PIX_EXT_C1C2_TB            0x114
#define PIPE_SW_PIXEL_EXT_C1C2_REQ         0x118
#define PIPE_SW_PIX_EXT_C3_LR              0x120
#define PIPE_SW_PIX_EXT_C3_TB              0x124
#define PIPE_SW_PIXEL_EXT_C3_REQ           0x128
#define SSPP_CDP_CNTL                      0x134
#define FLUSH_OFFSET                       0x18
#define PIPE_OP_MODE                       0x200
#define PIPE_CSC_1_MATRIX_COEFF_0          0x320
#define PIPE_CSC_1_MATRIX_COEFF_1          0x324
#define PIPE_CSC_1_MATRIX_COEFF_2          0x328
#define PIPE_CSC_1_MATRIX_COEFF_3          0x32C
#define PIPE_CSC_1_MATRIX_COEFF_4          0x330
#define PIPE_CSC_1_COMP_0_PRE_CLAMP        0x334
#define PIPE_CSC_1_COMP_1_PRE_CLAMP        0x338
#define PIPE_CSC_1_COMP_2_PRE_CLAMP        0x33C
#define PIPE_CSC_1_COMP_0_POST_CAMP        0x340
#define PIPE_CSC_1_COMP_1_POST_CLAMP       0x344
#define PIPE_CSC_1_COMP_2_POST_CLAMP       0x348
#define PIPE_CSC_1_COMP_0_PRE_BIAS         0x34C
#define PIPE_CSC_1_COMP_1_PRE_BIAS         0x350
#define PIPE_CSC_1_COMP_2_PRE_BIAS         0x354
#define PIPE_CSC_1_COMP_0_POST_BIAS        0x358
#define PIPE_CSC_1_COMP_1_POST_BIAS        0x35C
#define PIPE_CSC_1_COMP_2_POST_BIAS        0x360
#define PIPE_VP_0_QSEED2_CONFIG            0x204
#define PIPE_COMP0_3_PHASE_STEP_X          0x210
#define PIPE_COMP0_3_PHASE_STEP_Y          0x214
#define PIPE_COMP1_2_PHASE_STEP_X          0x218
#define PIPE_COMP1_2_PHASE_STEP_Y          0x21C
#define PIPE_VP_0_QSEED2_SHARP_SMOOTH_STRENGTH  0x230
#define PIPE_VP_0_QSEED2_SHARP_THRESHOLD_EDGE   0x234
#define PIPE_VP_0_QSEED2_SHARP_THRESHOLD_SMOOTH 0x238
#define PIPE_VP_0_QSEED2_SHARP_THRESHOLD_NOISE 0x23C

#define SSPP_SOLID_FILL_FORMAT             0x004237FF
#define SSPP_ARGB8888_FORMAT               0x000237FF
#define SSPP_XRGB8888_FORMAT               0x000236FF
#define SSPP_ARGB1555_FORMAT               0x00023315
#define SSPP_XRGB1555_FORMAT               0x00023215
#define SSPP_ARGB4444_FORMAT               0x00023340
#define SSPP_XRGB4444_FORMAT               0x00023240
#define SSPP_NV12_FORMAT                   0x0192923F
#define SSPP_NV16_FORMAT                   0x0092923F
#define SSPP_YUYV_FORMAT                   0x0082B23F
#define SSPP_YUV420_FORMAT                 0x018A803F
#define SSPP_RGB888_FORMAT                 0x0002243F
#define SSPP_RGB565_FORMAT                 0x00022216
#define SSPP_ARGB_PATTERN                  0x03020001
#define SSPP_ABGR_PATTERN                  0x03010002
#define SSPP_RGBA_PATTERN                  0x02000103
#define SSPP_BGRA_PATTERN                  0x01000203

#define LAYER_BLEND5_OP                    0x260
#define LAYER_OP_ENABLE_ALPHA_BLEND        0x600
#define LAYER_OP_DISABLE_ALPHA_BLEND       0x200

static u32 edrm_plane_formats_RGB[] = {
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_ABGR8888,
	DRM_FORMAT_RGBA8888,
	DRM_FORMAT_BGRA8888,
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_RGBX8888,
	DRM_FORMAT_BGRX8888,
	DRM_FORMAT_XBGR8888,
	DRM_FORMAT_XBGR8888,
	DRM_FORMAT_RGB888,
	DRM_FORMAT_BGR888,
	DRM_FORMAT_RGB565,
	DRM_FORMAT_BGR565,
	DRM_FORMAT_BGR565,
	DRM_FORMAT_ARGB1555,
	DRM_FORMAT_ABGR1555,
	DRM_FORMAT_RGBA5551,
	DRM_FORMAT_BGRA5551,
	DRM_FORMAT_XRGB1555,
	DRM_FORMAT_XBGR1555,
	DRM_FORMAT_RGBX5551,
	DRM_FORMAT_BGRX5551,
	DRM_FORMAT_ARGB4444,
	DRM_FORMAT_ABGR4444,
	DRM_FORMAT_RGBA4444,
	DRM_FORMAT_BGRA4444,
	DRM_FORMAT_XRGB4444,
	DRM_FORMAT_XBGR4444,
	DRM_FORMAT_RGBX4444,
	DRM_FORMAT_BGRX4444
};

static u32 edrm_plane_formats_YUV[] = {
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_ABGR8888,
	DRM_FORMAT_RGBA8888,
	DRM_FORMAT_BGRX8888,
	DRM_FORMAT_BGRA8888,
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_XBGR8888,
	DRM_FORMAT_RGBX8888,
	DRM_FORMAT_RGB888,
	DRM_FORMAT_BGR888,
	DRM_FORMAT_RGB565,
	DRM_FORMAT_BGR565,
	DRM_FORMAT_ARGB1555,
	DRM_FORMAT_ABGR1555,
	DRM_FORMAT_RGBA5551,
	DRM_FORMAT_BGRA5551,
	DRM_FORMAT_XRGB1555,
	DRM_FORMAT_XBGR1555,
	DRM_FORMAT_RGBX5551,
	DRM_FORMAT_BGRX5551,
	DRM_FORMAT_ARGB4444,
	DRM_FORMAT_ABGR4444,
	DRM_FORMAT_RGBA4444,
	DRM_FORMAT_BGRA4444,
	DRM_FORMAT_XRGB4444,
	DRM_FORMAT_XBGR4444,
	DRM_FORMAT_RGBX4444,
	DRM_FORMAT_BGRX4444,
	DRM_FORMAT_NV12,
	DRM_FORMAT_NV21,
	DRM_FORMAT_NV16,
	DRM_FORMAT_NV61,
	DRM_FORMAT_VYUY,
	DRM_FORMAT_UYVY,
	DRM_FORMAT_YUYV,
	DRM_FORMAT_YVYU,
	DRM_FORMAT_YUV420,
	DRM_FORMAT_YVU420
};

static void edrm_plane_enable_csc(struct sde_kms *master_kms,
				  u32 plane_offset)
{
	writel_relaxed(0x00060000, master_kms->mmio + plane_offset +
		PIPE_OP_MODE);

	writel_relaxed(0x9,  master_kms->mmio + plane_offset + SSPP_CDP_CNTL);
	writel_relaxed(0x00000254, master_kms->mmio + plane_offset +
		PIPE_CSC_1_MATRIX_COEFF_0);
	writel_relaxed(0x02540396, master_kms->mmio + plane_offset +
		PIPE_CSC_1_MATRIX_COEFF_1);
	writel_relaxed(0x1eef1f93, master_kms->mmio + plane_offset +
		PIPE_CSC_1_MATRIX_COEFF_2);
	writel_relaxed(0x043e0254, master_kms->mmio + plane_offset +
		PIPE_CSC_1_MATRIX_COEFF_3);
	writel_relaxed(0x00000000, master_kms->mmio + plane_offset +
		PIPE_CSC_1_MATRIX_COEFF_4);

	writel_relaxed(0x000010eb, master_kms->mmio + plane_offset +
		PIPE_CSC_1_COMP_0_PRE_CLAMP);
	writel_relaxed(0x000010f0, master_kms->mmio + plane_offset +
		PIPE_CSC_1_COMP_1_PRE_CLAMP);
	writel_relaxed(0x000010f0, master_kms->mmio + plane_offset +
		PIPE_CSC_1_COMP_2_PRE_CLAMP);
	writel_relaxed(0x000000ff, master_kms->mmio + plane_offset +
		PIPE_CSC_1_COMP_0_POST_CAMP);
	writel_relaxed(0x000000ff, master_kms->mmio + plane_offset +
		PIPE_CSC_1_COMP_1_POST_CLAMP);
	writel_relaxed(0x000000ff, master_kms->mmio + plane_offset +
		PIPE_CSC_1_COMP_2_POST_CLAMP);
	writel_relaxed(0x0000fff0, master_kms->mmio + plane_offset +
		PIPE_CSC_1_COMP_0_PRE_BIAS);
	writel_relaxed(0x0000ff80, master_kms->mmio + plane_offset +
		PIPE_CSC_1_COMP_1_PRE_BIAS);
	writel_relaxed(0x0000ff80, master_kms->mmio + plane_offset +
		PIPE_CSC_1_COMP_2_PRE_BIAS);
	writel_relaxed(0x00000000, master_kms->mmio + plane_offset +
		PIPE_CSC_1_COMP_0_POST_BIAS);
	writel_relaxed(0x00000000, master_kms->mmio + plane_offset +
		PIPE_CSC_1_COMP_1_POST_BIAS);
	writel_relaxed(0x00000000, master_kms->mmio + plane_offset +
		PIPE_CSC_1_COMP_2_POST_BIAS);

	writel_relaxed(0x200000, master_kms->mmio + plane_offset +
		PIPE_COMP0_3_PHASE_STEP_X);
	writel_relaxed(0x200000, master_kms->mmio + plane_offset +
		PIPE_COMP0_3_PHASE_STEP_Y);
	writel_relaxed(0x100000, master_kms->mmio + plane_offset +
		PIPE_COMP1_2_PHASE_STEP_X);
	writel_relaxed(0x100000, master_kms->mmio + plane_offset +
		PIPE_COMP1_2_PHASE_STEP_Y);
}

static void edrm_plane_set_yuv_plane(struct drm_plane *plane,
	struct sde_kms *master_kms, u32 lm_off)
{
	u32 img_size, ystride0, ystride1;
	u32 plane0_addr, plane1_addr, plane2_addr, plane3_addr;
	struct edrm_plane *edrm_plane;

	edrm_plane = to_edrm_plane(plane);
	edrm_plane_enable_csc(master_kms, edrm_plane->sspp_offset);
	if ((plane->state->fb->pixel_format == DRM_FORMAT_NV12) ||
		(plane->state->fb->pixel_format == DRM_FORMAT_NV21) ||
		(plane->state->fb->pixel_format == DRM_FORMAT_NV16) ||
		(plane->state->fb->pixel_format == DRM_FORMAT_NV61)) {
		ystride0 = (plane->state->fb->width << 16) |
			plane->state->fb->width;
		ystride1 = 0;
		plane0_addr = msm_framebuffer_iova(plane->state->fb,
			edrm_plane->aspace, 0);
		plane1_addr = msm_framebuffer_iova(plane->state->fb,
			edrm_plane->aspace, 1);
		plane2_addr = 0;
		plane3_addr = 0;
	} else if ((plane->state->fb->pixel_format == DRM_FORMAT_YUYV) ||
		(plane->state->fb->pixel_format == DRM_FORMAT_YVYU) ||
		(plane->state->fb->pixel_format == DRM_FORMAT_VYUY) ||
		(plane->state->fb->pixel_format == DRM_FORMAT_UYVY)) {
		/* YUYV formats are single plane */
		ystride0 = plane->state->fb->width * 2;
		ystride1 = 0;
		plane0_addr = msm_framebuffer_iova(plane->state->fb,
			edrm_plane->aspace, 0);
		plane1_addr = 0;
		plane2_addr = 0;
		plane3_addr = 0;
	} else if ((plane->state->fb->pixel_format == DRM_FORMAT_YUV420) ||
		(plane->state->fb->pixel_format == DRM_FORMAT_YVU420)) {
		ystride0 = ((plane->state->fb->width/2) << 16) |
			plane->state->fb->width;
		ystride1 = plane->state->fb->width/2;
		plane0_addr = msm_framebuffer_iova(plane->state->fb,
			edrm_plane->aspace, 0);
		plane1_addr = msm_framebuffer_iova(plane->state->fb,
			edrm_plane->aspace, 1);
		plane2_addr = msm_framebuffer_iova(plane->state->fb,
			edrm_plane->aspace, 2);
		plane3_addr = 0;
	} else {
		pr_err("Format %x not supported in eDRM\n",
				plane->state->fb->pixel_format);
		return;
	}

	writel_relaxed(ystride0, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_YSTRIDE0);
	writel_relaxed(ystride1, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_YSTRIDE1);
	writel_relaxed(plane0_addr, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC0_ADDR);
	writel_relaxed(plane1_addr, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC1_ADDR);
	writel_relaxed(plane2_addr, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC2_ADDR);
	writel_relaxed(plane3_addr, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC3_ADDR);
	writel_relaxed(0x00055f03, master_kms->mmio + edrm_plane->sspp_offset
		+ PIPE_VP_0_QSEED2_CONFIG);
	writel_relaxed(0x00000020, master_kms->mmio + edrm_plane->sspp_offset
		+ PIPE_VP_0_QSEED2_SHARP_SMOOTH_STRENGTH);
	writel_relaxed(0x00000070, master_kms->mmio + edrm_plane->sspp_offset
		+ PIPE_VP_0_QSEED2_SHARP_THRESHOLD_EDGE);
	writel_relaxed(0x00000008, master_kms->mmio + edrm_plane->sspp_offset
		+ PIPE_VP_0_QSEED2_SHARP_THRESHOLD_SMOOTH);
	writel_relaxed(0x00000002, master_kms->mmio + edrm_plane->sspp_offset
		+ PIPE_VP_0_QSEED2_SHARP_THRESHOLD_NOISE);

	writel_relaxed(0x00020001, master_kms->mmio +
		edrm_plane->sspp_offset + PIPE_SW_PIX_EXT_C0_LR);
	writel_relaxed(0x00020001, master_kms->mmio +
		edrm_plane->sspp_offset + PIPE_SW_PIX_EXT_C0_TB);
	img_size = ((plane->state->fb->height + 3) << 16) |
		(plane->state->fb->width + 3);
	writel_relaxed(img_size, master_kms->mmio +
		edrm_plane->sspp_offset + PIPE_SW_PIXEL_EXT_C0_REQ);

	writel_relaxed(0x00010000, master_kms->mmio +
		edrm_plane->sspp_offset + PIPE_SW_PIX_EXT_C1C2_LR);
	writel_relaxed(0x00010000, master_kms->mmio +
		edrm_plane->sspp_offset + PIPE_SW_PIX_EXT_C1C2_TB);
	img_size = ((plane->state->fb->height/2 + 1) << 16) |
		(plane->state->fb->width/2 + 1);
	writel_relaxed(img_size, master_kms->mmio +
		edrm_plane->sspp_offset + PIPE_SW_PIXEL_EXT_C1C2_REQ);

	writel_relaxed(0x00010000, master_kms->mmio +
		edrm_plane->sspp_offset + PIPE_SW_PIX_EXT_C3_LR);
	writel_relaxed(0x00010000, master_kms->mmio +
		edrm_plane->sspp_offset + PIPE_SW_PIX_EXT_C3_TB);
	img_size = ((plane->state->fb->height + 1) << 16) |
		(plane->state->fb->width + 1);
	writel_relaxed(img_size, master_kms->mmio +
		edrm_plane->sspp_offset + PIPE_SW_PIXEL_EXT_C3_REQ);


	/* do a solid fill of transparent color */
	writel_relaxed(0xFF000000, master_kms->mmio +
		edrm_plane->sspp_offset + SSPP_CONSTANT_COLOR);

	/* setup blending for mixer stage 5 */
	writel_relaxed(LAYER_OP_DISABLE_ALPHA_BLEND, master_kms->mmio + lm_off
		+ LAYER_BLEND5_OP);
}

static void edrm_plane_set_rgb_plane(struct drm_plane *plane,
	struct sde_kms *master_kms, u32 lm_off)
{
	u32 img_size, ystride0, ystride1, plane_addr;
	struct edrm_plane *edrm_plane;

	edrm_plane = to_edrm_plane(plane);

	ystride0 = (plane->state->fb->width *
		plane->state->fb->bits_per_pixel/8);
	ystride1 = 0;
	plane_addr = msm_framebuffer_iova(plane->state->fb,
		edrm_plane->aspace, 0);
	img_size = (plane->state->fb->height << 16) | plane->state->fb->width;
	writel_relaxed(plane_addr, master_kms->mmio +
		edrm_plane->sspp_offset + SSPP_SRC0_ADDR);
	writel_relaxed(ystride0, master_kms->mmio +
		edrm_plane->sspp_offset + SSPP_SRC_YSTRIDE0);
	writel_relaxed(ystride1, master_kms->mmio +
		edrm_plane->sspp_offset + SSPP_SRC_YSTRIDE1);
	writel_relaxed(0x0, master_kms->mmio +
		edrm_plane->sspp_offset + PIPE_SW_PIX_EXT_C0_LR);
	writel_relaxed(0x0, master_kms->mmio +
		edrm_plane->sspp_offset + PIPE_SW_PIX_EXT_C0_TB);
	writel_relaxed(img_size, master_kms->mmio +
		edrm_plane->sspp_offset + PIPE_SW_PIXEL_EXT_C0_REQ);
	writel_relaxed(0x0, master_kms->mmio +
		edrm_plane->sspp_offset + PIPE_SW_PIX_EXT_C1C2_LR);
	writel_relaxed(0x0, master_kms->mmio +
		edrm_plane->sspp_offset + PIPE_SW_PIX_EXT_C1C2_TB);
	writel_relaxed(img_size, master_kms->mmio +
		edrm_plane->sspp_offset + PIPE_SW_PIXEL_EXT_C1C2_REQ);
	writel_relaxed(0x0, master_kms->mmio + edrm_plane->sspp_offset +
			PIPE_SW_PIX_EXT_C3_LR);
	writel_relaxed(0x0, master_kms->mmio + edrm_plane->sspp_offset +
			PIPE_SW_PIX_EXT_C3_TB);
	writel_relaxed(img_size, master_kms->mmio +
		edrm_plane->sspp_offset + PIPE_SW_PIXEL_EXT_C3_REQ);
	/* do a solid fill of transparent color */
	writel_relaxed(0x0, master_kms->mmio + edrm_plane->sspp_offset +
			SSPP_CONSTANT_COLOR);

	/* setup blending for mixer stage 5 */
	writel_relaxed(LAYER_OP_ENABLE_ALPHA_BLEND, master_kms->mmio + lm_off
		+ LAYER_BLEND5_OP);

	/* disable CSC */
	writel_relaxed(0x0, master_kms->mmio + edrm_plane->sspp_offset +
		PIPE_OP_MODE);
	writel_relaxed(0x0, master_kms->mmio + edrm_plane->sspp_offset
		+ PIPE_VP_0_QSEED2_CONFIG);
}

static int edrm_plane_modeset(struct drm_plane *plane)
{
	struct drm_device *dev = plane->dev;
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_kms *kms = priv->kms;
	struct msm_edrm_kms *edrm_kms;
	struct msm_drm_private *master_priv;
	struct sde_kms *master_kms;
	struct edrm_plane *edrm_plane;
	bool yuv_format;
	u32 img_size, src_xy, dst_xy, lm_off;
	struct msm_edrm_display *display;

	edrm_kms = to_edrm_kms(kms);
	master_priv = edrm_kms->master_dev->dev_private;
	master_kms = to_sde_kms(master_priv->kms);
	edrm_plane = to_edrm_plane(plane);
	display = &edrm_kms->display[edrm_plane->display_id];
	lm_off = display->lm_off;

	switch (plane->state->fb->pixel_format) {
	case DRM_FORMAT_ARGB8888:
		writel_relaxed(SSPP_ARGB8888_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(SSPP_ARGB_PATTERN, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = false;
		break;
	case DRM_FORMAT_ABGR8888:
		writel_relaxed(SSPP_ARGB8888_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(SSPP_ABGR_PATTERN, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = false;
		break;
	case DRM_FORMAT_RGBA8888:
		writel_relaxed(SSPP_ARGB8888_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(SSPP_RGBA_PATTERN, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = false;
		break;
	case DRM_FORMAT_BGRX8888:
		writel_relaxed(SSPP_XRGB8888_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(SSPP_BGRA_PATTERN, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = false;
		break;
	case DRM_FORMAT_BGRA8888:
		writel_relaxed(SSPP_ARGB8888_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(SSPP_BGRA_PATTERN, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = false;
		break;
	case DRM_FORMAT_XRGB8888:
		writel_relaxed(SSPP_XRGB8888_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(SSPP_ARGB_PATTERN, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = false;
		break;
	case DRM_FORMAT_XBGR8888:
		writel_relaxed(SSPP_XRGB8888_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(SSPP_ABGR_PATTERN, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = false;
		break;
	case DRM_FORMAT_RGBX8888:
		writel_relaxed(SSPP_XRGB8888_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(SSPP_RGBA_PATTERN, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = false;
		break;
	case DRM_FORMAT_RGB888:
		writel_relaxed(SSPP_RGB888_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(0x00020001, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = false;
		break;
	case DRM_FORMAT_BGR888:
		writel_relaxed(SSPP_RGB888_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(0x00010002, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = false;
		break;
	case DRM_FORMAT_RGB565:
		writel_relaxed(SSPP_RGB565_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(0x00020001, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = false;
		break;
	case DRM_FORMAT_BGR565:
		writel_relaxed(SSPP_RGB565_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(0x00010002, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = false;
		break;
	case DRM_FORMAT_ARGB1555:
		writel_relaxed(SSPP_ARGB1555_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(SSPP_ARGB_PATTERN, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = false;
		break;
	case DRM_FORMAT_ABGR1555:
		writel_relaxed(SSPP_ARGB1555_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(SSPP_ABGR_PATTERN, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = false;
		break;
	case DRM_FORMAT_RGBA5551:
		writel_relaxed(SSPP_ARGB1555_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(SSPP_RGBA_PATTERN, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = false;
		break;
	case DRM_FORMAT_BGRA5551:
		writel_relaxed(SSPP_ARGB1555_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(SSPP_BGRA_PATTERN, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = false;
		break;
	case DRM_FORMAT_XRGB1555:
		writel_relaxed(SSPP_XRGB1555_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(SSPP_ARGB_PATTERN, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = false;
		break;
	case DRM_FORMAT_XBGR1555:
		writel_relaxed(SSPP_XRGB1555_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(SSPP_ABGR_PATTERN, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = false;
		break;
	case DRM_FORMAT_RGBX5551:
		writel_relaxed(SSPP_XRGB1555_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(SSPP_RGBA_PATTERN, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = false;
		break;
	case DRM_FORMAT_BGRX5551:
		writel_relaxed(SSPP_XRGB1555_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(SSPP_BGRA_PATTERN, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = false;
		break;
	case DRM_FORMAT_ARGB4444:
		writel_relaxed(SSPP_ARGB4444_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(SSPP_ARGB_PATTERN, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = false;
		break;
	case DRM_FORMAT_ABGR4444:
		writel_relaxed(SSPP_ARGB4444_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(SSPP_ARGB_PATTERN, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = false;
		break;
	case DRM_FORMAT_RGBA4444:
		writel_relaxed(SSPP_ARGB4444_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(SSPP_RGBA_PATTERN, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = false;
		break;
	case DRM_FORMAT_BGRA4444:
		writel_relaxed(SSPP_ARGB4444_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(SSPP_BGRA_PATTERN, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = false;
		break;
	case DRM_FORMAT_XRGB4444:
		writel_relaxed(SSPP_ARGB4444_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(SSPP_ARGB_PATTERN, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = false;
		break;
	case DRM_FORMAT_XBGR4444:
		writel_relaxed(SSPP_XRGB4444_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(SSPP_ABGR_PATTERN, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = false;
		break;
	case DRM_FORMAT_RGBX4444:
		writel_relaxed(SSPP_XRGB4444_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(SSPP_RGBA_PATTERN, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = false;
		break;
	case DRM_FORMAT_BGRX4444:
		writel_relaxed(SSPP_XRGB4444_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(SSPP_BGRA_PATTERN, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = false;
		break;
	case DRM_FORMAT_NV12:
		writel_relaxed(SSPP_NV12_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(0x00000201, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = true;
		break;
	case DRM_FORMAT_NV21:
		writel_relaxed(SSPP_NV12_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(0x00000102, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = true;
		break;
	case DRM_FORMAT_NV16:
		writel_relaxed(SSPP_NV16_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(0x00000201, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = true;
		break;
	case DRM_FORMAT_NV61:
		writel_relaxed(SSPP_NV16_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(0x00000102, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = true;
		break;
	case DRM_FORMAT_VYUY:
		writel_relaxed(SSPP_YUYV_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(0x00010002, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = true;
		break;
	case DRM_FORMAT_UYVY:
		writel_relaxed(SSPP_YUYV_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(0x00020001, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = true;
		break;
	case DRM_FORMAT_YUYV:
		writel_relaxed(SSPP_YUYV_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(0x02000100, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = true;
		break;
	case DRM_FORMAT_YVYU:
		writel_relaxed(SSPP_YUYV_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(0x01000200, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = true;
		break;
	case DRM_FORMAT_YUV420:
		writel_relaxed(SSPP_YUV420_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(0x00000102, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = true;
		break;
	case DRM_FORMAT_YVU420:
		writel_relaxed(SSPP_YUV420_FORMAT, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
		writel_relaxed(0x00000201, master_kms->mmio +
			edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
		yuv_format = true;
		break;
	default:
		pr_err("Format %x not supported in eDRM\n",
				plane->state->fb->pixel_format);
		return -EINVAL;
	}

	if (yuv_format)
		edrm_plane_set_yuv_plane(plane, master_kms, lm_off);
	else
		edrm_plane_set_rgb_plane(plane, master_kms, lm_off);

	img_size = (plane->state->fb->height << 16) | plane->state->fb->width;
	src_xy = (plane->state->src_x << 16) | plane->state->src_y;
	dst_xy = (plane->state->crtc_x << 16) | plane->state->crtc_y;

	writel_relaxed(img_size, master_kms->mmio + edrm_plane->sspp_offset +
		SSPP_SRC_SIZE);
	writel_relaxed(src_xy, master_kms->mmio + edrm_plane->sspp_offset +
		SSPP_SRC_XY);
	writel_relaxed(img_size, master_kms->mmio + edrm_plane->sspp_offset +
		SSPP_OUT_SIZE);
	writel_relaxed(dst_xy, master_kms->mmio + edrm_plane->sspp_offset +
		SSPP_OUT_XY);

	return 0;
}

void edrm_plane_destroy(struct drm_plane *plane)
{
	struct edrm_plane *edrm_plane = to_edrm_plane(plane);

	drm_plane_helper_disable(plane);
	drm_plane_cleanup(plane);
	kfree(edrm_plane);
}

int edrm_plane_flush(struct drm_plane *plane)
{
	struct edrm_plane *edrm_plane = to_edrm_plane(plane);
	struct edrm_crtc *edrm_crtc = to_edrm_crtc(plane->state->crtc);
	u32 sspp_flush_mask_bit[10] = {
				0, 1, 2, 18, 3, 4, 5, 19, 11, 12};

	edrm_crtc->sspp_flush_mask |=
		BIT(sspp_flush_mask_bit[edrm_plane->sspp_cfg_id - 1]);
	return 0;
}

static int edrm_plane_atomic_check(struct drm_plane *plane,
		struct drm_plane_state *state)
{
	/* TODO: check plane setting */
	return 0;
}

static void edrm_plane_atomic_update(struct drm_plane *plane,
		struct drm_plane_state *state)
{
	if (!plane->state->crtc) {
		pr_err("state crtc is null, skip pipe programming\n");
		return;
	}
	if (!plane->state->fb) {
		pr_err("state fb is null, skip pipe programming\n");
		return;
	}

	if (edrm_plane_modeset(plane))
		pr_err("Plane modeset failed\n");
}

/* Plane disable should setup the sspp to show a transparent frame
 * If the pipe still attached with a buffer pointer, the buffer could
 * be released and cause SMMU fault.  We don't want to change CTL and
 * LM during eDRM closing because main DRM could be updating CTL and
 * LM at any moment.  In eDRM lastclose(), it will notify main DRM to
 * release eDRM display resouse.  The next main DRM commit will clear
 * the stage setup by eDRM
 */
static void edrm_plane_atomic_disable(struct drm_plane *plane,
		struct drm_plane_state *state)
{
	struct drm_device *dev = plane->dev;
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_kms *kms = priv->kms;
	struct msm_edrm_kms *edrm_kms;
	struct msm_drm_private *master_priv;
	struct sde_kms *master_kms;
	struct msm_edrm_display *display;
	struct edrm_plane *edrm_plane;
	u32 img_size, stride, lm_off;

	edrm_kms = to_edrm_kms(kms);
	master_priv = edrm_kms->master_dev->dev_private;
	master_kms = to_sde_kms(master_priv->kms);
	dev = edrm_kms->dev;
	priv = dev->dev_private;

	edrm_plane = to_edrm_plane(plane);
	display = &edrm_kms->display[edrm_plane->display_id];
	lm_off = display->lm_off;

	/* setup SSPP */
	img_size = (display->mode.vdisplay << 16) | display->mode.hdisplay;
	stride = display->mode.hdisplay * 4;
	writel_relaxed(img_size, master_kms->mmio + edrm_plane->sspp_offset +
		SSPP_SRC_SIZE);
	writel_relaxed(0, master_kms->mmio + edrm_plane->sspp_offset
		+ SSPP_SRC_XY);
	writel_relaxed(img_size, master_kms->mmio + edrm_plane->sspp_offset +
			SSPP_OUT_SIZE);
	writel_relaxed(0, master_kms->mmio + edrm_plane->sspp_offset +
			SSPP_OUT_XY);
	writel_relaxed(stride, master_kms->mmio + edrm_plane->sspp_offset +
			SSPP_SRC_YSTRIDE0);
	writel_relaxed(0x0, master_kms->mmio + edrm_plane->sspp_offset +
			PIPE_SW_PIX_EXT_C0_LR);
	writel_relaxed(0x0, master_kms->mmio + edrm_plane->sspp_offset +
			PIPE_SW_PIX_EXT_C0_TB);
	writel_relaxed(img_size, master_kms->mmio + edrm_plane->sspp_offset +
			PIPE_SW_PIXEL_EXT_C0_REQ);
	writel_relaxed(0x0, master_kms->mmio + edrm_plane->sspp_offset +
			PIPE_SW_PIX_EXT_C1C2_LR);
	writel_relaxed(0x0, master_kms->mmio + edrm_plane->sspp_offset +
			PIPE_SW_PIX_EXT_C1C2_TB);
	writel_relaxed(img_size, master_kms->mmio + edrm_plane->sspp_offset +
			PIPE_SW_PIXEL_EXT_C1C2_REQ);
	writel_relaxed(0x0, master_kms->mmio + edrm_plane->sspp_offset +
			PIPE_SW_PIX_EXT_C3_LR);
	writel_relaxed(0x0, master_kms->mmio + edrm_plane->sspp_offset +
			PIPE_SW_PIX_EXT_C3_TB);
	writel_relaxed(img_size, master_kms->mmio + edrm_plane->sspp_offset +
			PIPE_SW_PIXEL_EXT_C3_REQ);

	/* RGB format */
	writel_relaxed(SSPP_SOLID_FILL_FORMAT, master_kms->mmio +
		edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
	writel_relaxed(SSPP_ARGB_PATTERN, master_kms->mmio +
		edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
	/* do a solid fill of transparent color */
	writel_relaxed(0x0, master_kms->mmio + edrm_plane->sspp_offset +
			SSPP_CONSTANT_COLOR);
	writel_relaxed(LAYER_OP_ENABLE_ALPHA_BLEND, master_kms->mmio + lm_off
		+ LAYER_BLEND5_OP);

	/* disable CSC */
	writel_relaxed(0x0, master_kms->mmio + edrm_plane->sspp_offset +
		PIPE_OP_MODE);
	writel_relaxed(0x0, master_kms->mmio + edrm_plane->sspp_offset
		+ PIPE_VP_0_QSEED2_CONFIG);
}

static int edrm_plane_prepare_fb(struct drm_plane *plane,
		const struct drm_plane_state *new_state)
{
	struct drm_framebuffer *fb;
	struct edrm_plane *edrm_plane;

	if (!plane || !new_state)
		return -EINVAL;

	if (!new_state->fb)
		return 0;
	edrm_plane = to_edrm_plane(plane);
	fb = new_state->fb;
	return msm_framebuffer_prepare(fb, edrm_plane->aspace);
}

static void edrm_plane_cleanup_fb(struct drm_plane *plane,
		const struct drm_plane_state *old_state)
{
	struct drm_framebuffer *fb = old_state ? old_state->fb : NULL;
	struct edrm_plane *edrm_plane = plane ? to_edrm_plane(plane) : NULL;

	if (!fb || !plane)
		return;

	msm_framebuffer_cleanup(fb, edrm_plane->aspace);
}

static const struct drm_plane_funcs edrm_plane_funcs = {
	.update_plane = drm_atomic_helper_update_plane,
	.disable_plane = drm_atomic_helper_disable_plane,
	.destroy = edrm_plane_destroy,
	.reset = drm_atomic_helper_plane_reset,
	.atomic_duplicate_state = drm_atomic_helper_plane_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_plane_destroy_state,
};

static const struct drm_plane_helper_funcs edrm_plane_helper_funcs = {
	.prepare_fb = edrm_plane_prepare_fb,
	.cleanup_fb = edrm_plane_cleanup_fb,
	.atomic_check = edrm_plane_atomic_check,
	.atomic_update = edrm_plane_atomic_update,
	.atomic_disable = edrm_plane_atomic_disable,
};

struct drm_plane *edrm_plane_init(struct drm_device *dev, int pipe,
		u32 pipe_type)
{
	struct msm_drm_private *priv;
	struct msm_edrm_kms *edrm_kms;
	struct edrm_plane *edrm_plane;
	struct drm_plane *plane;
	int ret;

	edrm_plane = kzalloc(sizeof(*edrm_plane), GFP_KERNEL);
	if (!edrm_plane) {
		ret = -ENOMEM;
		goto fail;
	}

	plane = &edrm_plane->base;
	if (pipe_type == SSPP_TYPE_VIG)
		ret = drm_universal_plane_init(dev, plane, 0,
			&edrm_plane_funcs,
			edrm_plane_formats_YUV,
			ARRAY_SIZE(edrm_plane_formats_YUV),
			DRM_PLANE_TYPE_PRIMARY);
	else
		ret = drm_universal_plane_init(dev, plane, 0,
			&edrm_plane_funcs,
			edrm_plane_formats_RGB,
			ARRAY_SIZE(edrm_plane_formats_RGB),
			DRM_PLANE_TYPE_PRIMARY);
	if (ret)
		goto fail;

	drm_plane_helper_add(plane, &edrm_plane_helper_funcs);

	priv = dev->dev_private;
	edrm_kms = to_edrm_kms(priv->kms);

	edrm_plane->pipe = pipe;
	edrm_plane->aspace = edrm_kms->aspace;

	return plane;
fail:
	return ERR_PTR(ret);
}
