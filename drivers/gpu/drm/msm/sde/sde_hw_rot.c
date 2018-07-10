/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__

#include <linux/mutex.h>
#include <linux/platform_device.h>

#include "sde_kms.h"
#include "sde_hw_mdss.h"
#include "sde_hwio.h"
#include "sde_hw_catalog.h"
#include "sde_hw_rot.h"
#include "sde_formats.h"
#include "sde_rotator_inline.h"

#define SDE_MODIFLER(_modifier_) ((_modifier_) & 0x00ffffffffffffffULL)
#define SDE_MODIFIER_IS_TILE(_modifier_) \
	SDE_MODIFLER((_modifier_) & DRM_FORMAT_MOD_QCOM_TILE)
#define SDE_MODIFIER_IS_UBWC(_modifier_) \
	SDE_MODIFLER((_modifier_) & DRM_FORMAT_MOD_QCOM_COMPRESSED)
#define SDE_MODIFIER_IS_10B(_modifier_) \
	SDE_MODIFLER((_modifier_) & DRM_FORMAT_MOD_QCOM_DX)
#define SDE_MODIFIER_IS_TIGHT(_modifier_) \
	SDE_MODIFLER((_modifier_) & DRM_FORMAT_MOD_QCOM_TIGHT)

/**
 * _rot_offset - update register map of the given rotator instance
 * @rot: rotator identifier
 * @m: Pointer to mdss catalog
 * @addr: i/o address mapping
 * @b: Pointer to register block mapping structure
 * return: Pointer to rotator configuration of the given instance
 */
static struct sde_rot_cfg *_rot_offset(enum sde_rot rot,
		struct sde_mdss_cfg *m,
		void __iomem *addr,
		struct sde_hw_blk_reg_map *b)
{
	int i;

	for (i = 0; i < m->rot_count; i++) {
		if (rot == m->rot[i].id) {
			b->base_off = addr;
			b->blk_off = m->rot[i].base;
			b->hwversion = m->hwversion;
			b->log_mask = SDE_DBG_MASK_ROT;
			return &m->rot[i];
		}
	}

	return ERR_PTR(-EINVAL);
}

/**
 * _sde_hw_rot_reg_dump - perform register dump
 * @ptr: private pointer to rotator platform device
 * return: None
 */
static void _sde_hw_rot_reg_dump(void *ptr)
{
	sde_rotator_inline_reg_dump((struct platform_device *) ptr);
}

/**
 * sde_hw_rot_start - start rotator before any commit
 * @hw: Pointer to rotator hardware driver
 * return: 0 if success; error code otherwise
 */
static int sde_hw_rot_start(struct sde_hw_rot *hw)
{
	struct platform_device *pdev;
	int rc;

	if (!hw || !hw->caps || !hw->caps->pdev) {
		SDE_ERROR("invalid parameters\n");
		return -EINVAL;
	}

	pdev = hw->caps->pdev;

	rc = sde_dbg_reg_register_cb(hw->name, _sde_hw_rot_reg_dump, pdev);
	if (rc)
		SDE_ERROR("failed to register debug dump %d\n", rc);

	hw->rot_ctx = sde_rotator_inline_open(pdev);
	if (IS_ERR_OR_NULL(hw->rot_ctx)) {
		rc = PTR_ERR(hw->rot_ctx);
		hw->rot_ctx = NULL;
		return rc;
	}

	return 0;
}

/**
 * sde_hw_rot_stop - stop rotator after final commit
 * @hw: Pointer to rotator hardware driver
 * return: none
 */
static void sde_hw_rot_stop(struct sde_hw_rot *hw)
{
	if (!hw || !hw->caps || !hw->caps->pdev) {
		SDE_ERROR("invalid parameter\n");
		return;
	}

	sde_rotator_inline_release(hw->rot_ctx);
	hw->rot_ctx = NULL;

	sde_dbg_reg_unregister_cb(hw->name, _sde_hw_rot_reg_dump,
			hw->caps->pdev);
}

/**
 * sde_hw_rot_to_v4l2_pixfmt - convert drm pixel format to v4l2 pixel format
 * @drm_pixfmt: drm fourcc pixel format
 * @drm_modifier: drm pixel format modifier
 * @pixfmt: Pointer to v4l2 fourcc pixel format (output)
 * return: 0 if success; error code otherwise
 */
static int sde_hw_rot_to_v4l2_pixfmt(u32 drm_pixfmt, u64 drm_modifier,
		u32 *pixfmt)
{
	u32 rc = 0;

	if (!pixfmt)
		return -EINVAL;

	switch (drm_pixfmt) {
	case DRM_FORMAT_BGR565:
		if (SDE_MODIFIER_IS_UBWC(drm_modifier))
			*pixfmt = SDE_PIX_FMT_RGB_565_UBWC;
		else
			*pixfmt = SDE_PIX_FMT_RGB_565;
		break;
	case DRM_FORMAT_BGRA8888:
		if (SDE_MODIFIER_IS_TILE(drm_modifier))
			*pixfmt = SDE_PIX_FMT_ARGB_8888_TILE;
		else
			*pixfmt = SDE_PIX_FMT_ARGB_8888;
		break;
	case DRM_FORMAT_BGRX8888:
		if (SDE_MODIFIER_IS_TILE(drm_modifier))
			*pixfmt = SDE_PIX_FMT_XRGB_8888_TILE;
		else
			*pixfmt = SDE_PIX_FMT_XRGB_8888;
		break;
	case DRM_FORMAT_RGBA8888:
		if (SDE_MODIFIER_IS_TILE(drm_modifier))
			*pixfmt = SDE_PIX_FMT_ABGR_8888_TILE;
		else
			*pixfmt = SDE_PIX_FMT_ABGR_8888;
		break;
	case DRM_FORMAT_RGBX8888:
		if (SDE_MODIFIER_IS_TILE(drm_modifier))
			*pixfmt = SDE_PIX_FMT_XBGR_8888_TILE;
		else
			*pixfmt = SDE_PIX_FMT_XBGR_8888;
		break;
	case DRM_FORMAT_ABGR8888:
		if (SDE_MODIFIER_IS_UBWC(drm_modifier))
			*pixfmt = SDE_PIX_FMT_RGBA_8888_UBWC;
		else if (SDE_MODIFIER_IS_TILE(drm_modifier))
			*pixfmt = SDE_PIX_FMT_RGBA_8888_TILE;
		else
			*pixfmt = SDE_PIX_FMT_RGBA_8888;
		break;
	case DRM_FORMAT_XBGR8888:
		if (SDE_MODIFIER_IS_UBWC(drm_modifier))
			*pixfmt = SDE_PIX_FMT_RGBX_8888_UBWC;
		else if (SDE_MODIFIER_IS_TILE(drm_modifier))
			*pixfmt = SDE_PIX_FMT_RGBX_8888_TILE;
		else
			*pixfmt = SDE_PIX_FMT_RGBX_8888;
		break;
	case DRM_FORMAT_ARGB8888:
		if (SDE_MODIFIER_IS_TILE(drm_modifier))
			*pixfmt = SDE_PIX_FMT_BGRA_8888_TILE;
		else
			*pixfmt = SDE_PIX_FMT_BGRA_8888;
		break;
	case DRM_FORMAT_XRGB8888:
		if (SDE_MODIFIER_IS_TILE(drm_modifier))
			*pixfmt = SDE_PIX_FMT_BGRX_8888_TILE;
		else
			*pixfmt = SDE_PIX_FMT_BGRX_8888;
		break;
	case DRM_FORMAT_NV12:
		if (SDE_MODIFIER_IS_UBWC(drm_modifier)) {
			if (SDE_MODIFIER_IS_10B(drm_modifier)) {
				if (SDE_MODIFIER_IS_TIGHT(drm_modifier))
					*pixfmt =
					SDE_PIX_FMT_Y_CBCR_H2V2_TP10_UBWC;
				else
					*pixfmt =
					SDE_PIX_FMT_Y_CBCR_H2V2_P010_UBWC;
			} else {
				*pixfmt = SDE_PIX_FMT_Y_CBCR_H2V2_UBWC;
			}
		} else if (SDE_MODIFIER_IS_TILE(drm_modifier)) {
			if (SDE_MODIFIER_IS_10B(drm_modifier)) {
				if (SDE_MODIFIER_IS_TIGHT(drm_modifier))
					*pixfmt =
					SDE_PIX_FMT_Y_CBCR_H2V2_TP10;
				else
					*pixfmt =
					SDE_PIX_FMT_Y_CBCR_H2V2_P010_TILE;
			} else {
				*pixfmt = SDE_PIX_FMT_Y_CBCR_H2V2_TILE;
			}
		} else {
			if (SDE_MODIFIER_IS_10B(drm_modifier)) {
				if (SDE_MODIFIER_IS_TIGHT(drm_modifier))
					*pixfmt =
					SDE_PIX_FMT_Y_CBCR_H2V2_TP10;
				else
					*pixfmt =
					SDE_PIX_FMT_Y_CBCR_H2V2_P010;
			} else {
				*pixfmt = SDE_PIX_FMT_Y_CBCR_H2V2;
			}
		}
		break;
	case DRM_FORMAT_NV21:
		if (SDE_MODIFIER_IS_TILE(drm_modifier))
			*pixfmt = SDE_PIX_FMT_Y_CRCB_H2V2_TILE;
		else
			*pixfmt = SDE_PIX_FMT_Y_CRCB_H2V2;
		break;
	case DRM_FORMAT_BGRA1010102:
		if (SDE_MODIFIER_IS_TILE(drm_modifier))
			*pixfmt = SDE_PIX_FMT_ARGB_2101010_TILE;
		else
			*pixfmt = SDE_PIX_FMT_ARGB_2101010;
		break;
	case DRM_FORMAT_BGRX1010102:
		if (SDE_MODIFIER_IS_TILE(drm_modifier))
			*pixfmt = SDE_PIX_FMT_XRGB_2101010_TILE;
		else
			*pixfmt = SDE_PIX_FMT_XRGB_2101010;
		break;
	case DRM_FORMAT_RGBA1010102:
		if (SDE_MODIFIER_IS_TILE(drm_modifier))
			*pixfmt = SDE_PIX_FMT_ABGR_2101010_TILE;
		else
			*pixfmt = SDE_PIX_FMT_ABGR_2101010;
		break;
	case DRM_FORMAT_RGBX1010102:
		if (SDE_MODIFIER_IS_TILE(drm_modifier))
			*pixfmt = SDE_PIX_FMT_XBGR_2101010_TILE;
		else
			*pixfmt = SDE_PIX_FMT_XBGR_2101010;
		break;
	case DRM_FORMAT_ARGB2101010:
		if (SDE_MODIFIER_IS_TILE(drm_modifier))
			*pixfmt = SDE_PIX_FMT_BGRA_1010102_TILE;
		else
			*pixfmt = SDE_PIX_FMT_BGRA_1010102;
		break;
	case DRM_FORMAT_XRGB2101010:
		if (SDE_MODIFIER_IS_TILE(drm_modifier))
			*pixfmt = SDE_PIX_FMT_BGRX_1010102_TILE;
		else
			*pixfmt = SDE_PIX_FMT_BGRX_1010102;
		break;
	case DRM_FORMAT_ABGR2101010:
		if (SDE_MODIFIER_IS_UBWC(drm_modifier))
			*pixfmt = SDE_PIX_FMT_RGBA_1010102_UBWC;
		else if (SDE_MODIFIER_IS_TILE(drm_modifier))
			*pixfmt = SDE_PIX_FMT_RGBA_1010102_TILE;
		else
			*pixfmt = SDE_PIX_FMT_RGBA_1010102;
		break;
	case DRM_FORMAT_XBGR2101010:
		if (SDE_MODIFIER_IS_UBWC(drm_modifier))
			*pixfmt = SDE_PIX_FMT_RGBX_1010102_UBWC;
		else if (SDE_MODIFIER_IS_TILE(drm_modifier))
			*pixfmt = SDE_PIX_FMT_RGBX_1010102_TILE;
		else
			*pixfmt = SDE_PIX_FMT_RGBX_1010102;
		break;
	default:
		SDE_ERROR("invalid drm pixel format %c%c%c%c/%llx\n",
				drm_pixfmt >> 0, drm_pixfmt >> 8,
				drm_pixfmt >> 16, drm_pixfmt >> 24,
				drm_modifier);
		rc = -EINVAL;
		break;
	}

	return rc;
}

/**
 * sde_hw_rot_to_drm_pixfmt - convert v4l2 pixel format to drm pixel format
 * @pixfmt: v4l2 fourcc pixel format
 * @drm_pixfmt: Pointer to drm forucc pixel format (output)
 * @drm_modifier: Pointer to drm pixel format modifier (output)
 * return: 0 if success; error code otherwise
 */
static int sde_hw_rot_to_drm_pixfmt(u32 pixfmt, u32 *drm_pixfmt,
		u64 *drm_modifier)
{
	u32 rc = 0;

	switch (pixfmt) {
	case SDE_PIX_FMT_RGB_565:
		*drm_pixfmt = DRM_FORMAT_BGR565;
		*drm_modifier = 0;
		break;
	case SDE_PIX_FMT_RGB_565_UBWC:
		*drm_pixfmt = DRM_FORMAT_BGR565;
		*drm_modifier = DRM_FORMAT_MOD_QCOM_COMPRESSED |
				DRM_FORMAT_MOD_QCOM_TILE;
		break;
	case SDE_PIX_FMT_RGBA_8888:
		*drm_pixfmt = DRM_FORMAT_ABGR8888;
		*drm_modifier = 0;
		break;
	case SDE_PIX_FMT_RGBX_8888:
		*drm_pixfmt = DRM_FORMAT_XBGR8888;
		*drm_modifier = 0;
		break;
	case SDE_PIX_FMT_BGRA_8888:
		*drm_pixfmt = DRM_FORMAT_ARGB8888;
		*drm_modifier = 0;
		break;
	case SDE_PIX_FMT_BGRX_8888:
		*drm_pixfmt = DRM_FORMAT_XRGB8888;
		*drm_modifier = 0;
		break;
	case SDE_PIX_FMT_Y_CBCR_H2V2_UBWC:
		*drm_pixfmt = DRM_FORMAT_NV12;
		*drm_modifier = DRM_FORMAT_MOD_QCOM_COMPRESSED |
				DRM_FORMAT_MOD_QCOM_TILE;
		break;
	case SDE_PIX_FMT_Y_CRCB_H2V2:
		*drm_pixfmt = DRM_FORMAT_NV21;
		*drm_modifier = 0;
		break;
	case SDE_PIX_FMT_RGBA_8888_UBWC:
		*drm_pixfmt = DRM_FORMAT_ABGR8888;
		*drm_modifier = DRM_FORMAT_MOD_QCOM_COMPRESSED |
				DRM_FORMAT_MOD_QCOM_TILE;
		break;
	case SDE_PIX_FMT_RGBX_8888_UBWC:
		*drm_pixfmt = DRM_FORMAT_XBGR8888;
		*drm_modifier = DRM_FORMAT_MOD_QCOM_COMPRESSED |
				DRM_FORMAT_MOD_QCOM_TILE;
		break;
	case SDE_PIX_FMT_Y_CBCR_H2V2:
		*drm_pixfmt = DRM_FORMAT_NV12;
		*drm_modifier = 0;
		break;
	case SDE_PIX_FMT_ARGB_8888:
		*drm_pixfmt = DRM_FORMAT_BGRA8888;
		*drm_modifier = 0;
		break;
	case SDE_PIX_FMT_XRGB_8888:
		*drm_pixfmt = DRM_FORMAT_BGRX8888;
		*drm_modifier = 0;
		break;
	case SDE_PIX_FMT_ABGR_8888:
		*drm_pixfmt = DRM_FORMAT_RGBA8888;
		*drm_modifier = 0;
		break;
	case SDE_PIX_FMT_XBGR_8888:
		*drm_pixfmt = DRM_FORMAT_RGBX8888;
		*drm_modifier = 0;
		break;
	case SDE_PIX_FMT_ARGB_2101010:
		*drm_pixfmt = DRM_FORMAT_BGRA1010102;
		*drm_modifier = 0;
		break;
	case SDE_PIX_FMT_XRGB_2101010:
		*drm_pixfmt = DRM_FORMAT_BGRX1010102;
		*drm_modifier = 0;
		break;
	case SDE_PIX_FMT_ABGR_2101010:
		*drm_pixfmt = DRM_FORMAT_RGBA1010102;
		*drm_modifier = 0;
		break;
	case SDE_PIX_FMT_XBGR_2101010:
		*drm_pixfmt = DRM_FORMAT_RGBX1010102;
		*drm_modifier = 0;
		break;
	case SDE_PIX_FMT_BGRA_1010102:
		*drm_pixfmt = DRM_FORMAT_ARGB2101010;
		*drm_modifier = 0;
		break;
	case SDE_PIX_FMT_BGRX_1010102:
		*drm_pixfmt = DRM_FORMAT_XRGB2101010;
		*drm_modifier = 0;
		break;
	case SDE_PIX_FMT_RGBA_8888_TILE:
		*drm_pixfmt = DRM_FORMAT_ABGR8888;
		*drm_modifier = DRM_FORMAT_MOD_QCOM_TILE;
		break;
	case SDE_PIX_FMT_RGBX_8888_TILE:
		*drm_pixfmt = DRM_FORMAT_XBGR8888;
		*drm_modifier = DRM_FORMAT_MOD_QCOM_TILE;
		break;
	case SDE_PIX_FMT_BGRA_8888_TILE:
		*drm_pixfmt = DRM_FORMAT_ARGB8888;
		*drm_modifier = DRM_FORMAT_MOD_QCOM_TILE;
		break;
	case SDE_PIX_FMT_BGRX_8888_TILE:
		*drm_pixfmt = DRM_FORMAT_XRGB8888;
		*drm_modifier = DRM_FORMAT_MOD_QCOM_TILE;
		break;
	case SDE_PIX_FMT_Y_CRCB_H2V2_TILE:
		*drm_pixfmt = DRM_FORMAT_NV21;
		*drm_modifier = DRM_FORMAT_MOD_QCOM_TILE;
		break;
	case SDE_PIX_FMT_Y_CBCR_H2V2_TILE:
		*drm_pixfmt = DRM_FORMAT_NV12;
		*drm_modifier = DRM_FORMAT_MOD_QCOM_TILE;
		break;
	case SDE_PIX_FMT_ARGB_8888_TILE:
		*drm_pixfmt = DRM_FORMAT_BGRA8888;
		*drm_modifier = DRM_FORMAT_MOD_QCOM_TILE;
		break;
	case SDE_PIX_FMT_XRGB_8888_TILE:
		*drm_pixfmt = DRM_FORMAT_BGRX8888;
		*drm_modifier = DRM_FORMAT_MOD_QCOM_TILE;
		break;
	case SDE_PIX_FMT_ABGR_8888_TILE:
		*drm_pixfmt = DRM_FORMAT_RGBA8888;
		*drm_modifier = DRM_FORMAT_MOD_QCOM_TILE;
		break;
	case SDE_PIX_FMT_XBGR_8888_TILE:
		*drm_pixfmt = DRM_FORMAT_RGBX8888;
		*drm_modifier = DRM_FORMAT_MOD_QCOM_TILE;
		break;
	case SDE_PIX_FMT_ARGB_2101010_TILE:
		*drm_pixfmt = DRM_FORMAT_BGRA1010102;
		*drm_modifier = DRM_FORMAT_MOD_QCOM_TILE;
		break;
	case SDE_PIX_FMT_XRGB_2101010_TILE:
		*drm_pixfmt = DRM_FORMAT_BGRX1010102;
		*drm_modifier = DRM_FORMAT_MOD_QCOM_TILE;
		break;
	case SDE_PIX_FMT_ABGR_2101010_TILE:
		*drm_pixfmt = DRM_FORMAT_RGBA1010102;
		*drm_modifier = DRM_FORMAT_MOD_QCOM_TILE;
		break;
	case SDE_PIX_FMT_XBGR_2101010_TILE:
		*drm_pixfmt = DRM_FORMAT_RGBX1010102;
		*drm_modifier = DRM_FORMAT_MOD_QCOM_TILE;
		break;
	case SDE_PIX_FMT_BGRA_1010102_TILE:
		*drm_pixfmt = DRM_FORMAT_ARGB2101010;
		*drm_modifier = DRM_FORMAT_MOD_QCOM_TILE;
		break;
	case SDE_PIX_FMT_BGRX_1010102_TILE:
		*drm_pixfmt = DRM_FORMAT_XRGB2101010;
		*drm_modifier = DRM_FORMAT_MOD_QCOM_TILE;
		break;
	case SDE_PIX_FMT_RGBA_1010102_UBWC:
		*drm_pixfmt = DRM_FORMAT_ABGR2101010;
		*drm_modifier = DRM_FORMAT_MOD_QCOM_COMPRESSED |
				DRM_FORMAT_MOD_QCOM_TILE;
		break;
	case SDE_PIX_FMT_RGBX_1010102_UBWC:
		*drm_pixfmt = DRM_FORMAT_XBGR2101010;
		*drm_modifier = DRM_FORMAT_MOD_QCOM_COMPRESSED |
				DRM_FORMAT_MOD_QCOM_TILE;
		break;
	case SDE_PIX_FMT_Y_CBCR_H2V2_P010:
		*drm_pixfmt = DRM_FORMAT_NV12;
		*drm_modifier = DRM_FORMAT_MOD_QCOM_DX;
		break;
	case SDE_PIX_FMT_Y_CBCR_H2V2_P010_TILE:
		*drm_pixfmt = DRM_FORMAT_NV12;
		*drm_modifier = DRM_FORMAT_MOD_QCOM_TILE |
				DRM_FORMAT_MOD_QCOM_DX;
		break;
	case SDE_PIX_FMT_Y_CBCR_H2V2_P010_UBWC:
		*drm_pixfmt = DRM_FORMAT_NV12;
		*drm_modifier = DRM_FORMAT_MOD_QCOM_COMPRESSED |
				DRM_FORMAT_MOD_QCOM_TILE |
				DRM_FORMAT_MOD_QCOM_DX;
		break;
	case SDE_PIX_FMT_Y_CBCR_H2V2_TP10:
		*drm_pixfmt = DRM_FORMAT_NV12;
		*drm_modifier = DRM_FORMAT_MOD_QCOM_TILE |
				DRM_FORMAT_MOD_QCOM_DX |
				DRM_FORMAT_MOD_QCOM_TIGHT;
		break;
	case SDE_PIX_FMT_Y_CBCR_H2V2_TP10_UBWC:
		*drm_pixfmt = DRM_FORMAT_NV12;
		*drm_modifier = DRM_FORMAT_MOD_QCOM_COMPRESSED |
				DRM_FORMAT_MOD_QCOM_TILE |
				DRM_FORMAT_MOD_QCOM_DX |
				DRM_FORMAT_MOD_QCOM_TIGHT;
		break;
	default:
		SDE_DEBUG("invalid v4l2 pixel format %c%c%c%c\n",
				pixfmt >> 0, pixfmt >> 8,
				pixfmt >> 16, pixfmt >> 24);
		rc = -EINVAL;
		break;
	}

	return rc;
}

/**
 * sde_hw_rot_to_v4l2_buffer - convert drm buffer to v4l2 buffer
 * @drm_pixfmt: pixel format in drm fourcc
 * @drm_modifier: pixel format modifier
 * @drm_addr: drm buffer address per plane
 * @drm_len: drm buffer length per plane
 * @drm_planes: drm buffer number of planes
 * @v4l_addr: v4l2 buffer address per plane
 * @v4l_len: v4l2 buffer length per plane
 * @v4l_planes: v4l2 buffer number of planes
 */
static void sde_hw_rot_to_v4l2_buffer(u32 drm_pixfmt, u64 drm_modifier,
		dma_addr_t *drm_addr, u32 *drm_len, u32 *drm_planes,
		dma_addr_t *v4l_addr, u32 *v4l_len, u32 *v4l_planes)
{
	int i, total_size = 0;

	for (i = 0; i < SDE_ROTATOR_INLINE_PLANE_MAX; i++) {
		v4l_addr[i] = drm_addr[i];
		v4l_len[i] = drm_len[i];
		total_size += drm_len[i];
		SDE_DEBUG("drm[%d]:%pad/%x\n", i, &drm_addr[i], drm_len[i]);
	}

	if (SDE_MODIFIER_IS_UBWC(drm_modifier)) {
		/* v4l2 driver uses plane[0] as single ubwc buffer plane */
		v4l_addr[0] = drm_addr[2];
		v4l_len[0] = total_size;
		*v4l_planes = 1;
		SDE_DEBUG("v4l2[0]:%pad/%x/%d\n", &v4l_addr[0], v4l_len[0],
				*v4l_planes);
	} else {
		*v4l_planes = *drm_planes;
	}
}

/**
 * sde_hw_rot_adjust_prefill_bw - update prefill bw based on pipe config
 * @hw: Pointer to rotator hardware driver
 * @data: Pointer to command descriptor
 * @prefill_bw: adjusted prefill bw (output)
 * return: 0 if success; error code otherwise
 */
static int sde_hw_rot_adjust_prefill_bw(struct sde_hw_rot *hw,
		struct sde_hw_rot_cmd *data, u64 *prefill_bw)
{
	if (!hw || !data || !prefill_bw) {
		SDE_ERROR("invalid parameter(s)\n");
		return -EINVAL;
	}

	/* adjust bw for scaling */
	if (data->dst_rect_h) {
		u64 temp;

		temp = DIV_ROUND_UP_ULL(data->prefill_bw,
				data->dst_rect_h);
		*prefill_bw = temp * data->crtc_h;
	}

	return 0;
}

/**
 * sde_hw_rot_commit - commit/execute given rotator command
 * @hw: Pointer to rotator hardware driver
 * @data: Pointer to command descriptor
 * @hw_cmd: type of command to be executed
 * return: 0 if success; error code otherwise
 */
static int sde_hw_rot_commit(struct sde_hw_rot *hw, struct sde_hw_rot_cmd *data,
		enum sde_hw_rot_cmd_type hw_cmd)
{
	struct sde_rotator_inline_cmd rot_cmd;
	enum sde_rotator_inline_cmd_type cmd_type;
	void *priv_handle = NULL;
	int rc;

	if (!hw || !data) {
		SDE_ERROR("invalid parameter\n");
		return -EINVAL;
	}

	memset(&rot_cmd, 0, sizeof(struct sde_rotator_inline_cmd));

	switch (hw_cmd) {
	case SDE_HW_ROT_CMD_VALIDATE:
		cmd_type = SDE_ROTATOR_INLINE_CMD_VALIDATE;
		break;
	case SDE_HW_ROT_CMD_COMMIT:
		cmd_type = SDE_ROTATOR_INLINE_CMD_COMMIT;
		break;
	case SDE_HW_ROT_CMD_START:
		cmd_type = SDE_ROTATOR_INLINE_CMD_START;
		priv_handle = data->priv_handle;
		break;
	case SDE_HW_ROT_CMD_CLEANUP:
		cmd_type = SDE_ROTATOR_INLINE_CMD_CLEANUP;
		priv_handle = data->priv_handle;
		break;
	case SDE_HW_ROT_CMD_RESET:
		cmd_type = SDE_ROTATOR_INLINE_CMD_ABORT;
		priv_handle = data->priv_handle;
		break;
	default:
		SDE_ERROR("invalid hw rotator command %d\n", hw_cmd);
		return -EINVAL;
	}

	rot_cmd.sequence_id = data->sequence_id;
	rot_cmd.video_mode = data->video_mode;
	rot_cmd.fps = data->fps;

	/*
	 * DRM rotation property is specified in counter clockwise direction
	 * whereas rotator h/w rotates in clockwise direction.
	 * Convert rotation property to clockwise 90 by toggling h/v flip
	 */
	rot_cmd.rot90 = data->rot90;
	rot_cmd.hflip = data->rot90 ? !data->hflip : data->hflip;
	rot_cmd.vflip = data->rot90 ? !data->vflip : data->vflip;

	rot_cmd.secure = data->secure;
	rot_cmd.clkrate = data->clkrate;
	rot_cmd.data_bw = 0;
	rot_cmd.prefill_bw = data->prefill_bw;
	rot_cmd.src_width = data->src_width;
	rot_cmd.src_height = data->src_height;
	rot_cmd.src_rect_x = data->src_rect_x;
	rot_cmd.src_rect_y = data->src_rect_y;
	rot_cmd.src_rect_w = data->src_rect_w;
	rot_cmd.src_rect_h = data->src_rect_h;
	rot_cmd.dst_writeback = data->dst_writeback;
	rot_cmd.dst_rect_x = data->dst_rect_x;
	rot_cmd.dst_rect_y = data->dst_rect_y;
	rot_cmd.dst_rect_w = data->dst_rect_w;
	rot_cmd.dst_rect_h = data->dst_rect_h;
	rot_cmd.priv_handle = priv_handle;

	rc = sde_hw_rot_to_v4l2_pixfmt(data->src_pixel_format,
			data->src_modifier, &rot_cmd.src_pixfmt);
	if (rc) {
		SDE_ERROR("invalid src format %d\n", rc);
		return rc;
	}

	/* calculate preferred output format during validation */
	if (hw_cmd == SDE_HW_ROT_CMD_VALIDATE) {
		rc = sde_rotator_inline_get_dst_pixfmt(hw->caps->pdev,
				rot_cmd.src_pixfmt, &rot_cmd.dst_pixfmt);
		if (rc) {
			SDE_ERROR("invalid src format %d\n", rc);
			return rc;
		}

		rc = sde_hw_rot_to_drm_pixfmt(rot_cmd.dst_pixfmt,
				&data->dst_pixel_format, &data->dst_modifier);
		if (rc) {
			SDE_ERROR("invalid dst format %c%c%c%c\n",
					rot_cmd.dst_pixfmt >> 0,
					rot_cmd.dst_pixfmt >> 8,
					rot_cmd.dst_pixfmt >> 16,
					rot_cmd.dst_pixfmt >> 24);
			return rc;
		}

		data->dst_format = sde_get_sde_format_ext(
				data->dst_pixel_format, &data->dst_modifier, 1);
		if (!data->dst_format) {
			SDE_ERROR("failed to get dst format\n");
			return -EINVAL;
		}
	} else {
		rc = sde_hw_rot_to_v4l2_pixfmt(data->dst_pixel_format,
				data->dst_modifier, &rot_cmd.dst_pixfmt);
		if (rc) {
			SDE_ERROR("invalid dst format %d\n", rc);
			return rc;
		}

		sde_hw_rot_to_v4l2_buffer(data->src_pixel_format,
				data->src_modifier,
				data->src_iova, data->src_len,
				&data->src_planes,
				rot_cmd.src_addr, rot_cmd.src_len,
				&rot_cmd.src_planes);

		sde_hw_rot_to_v4l2_buffer(data->dst_pixel_format,
				data->dst_modifier,
				data->dst_iova, data->dst_len,
				&data->dst_planes,
				rot_cmd.dst_addr, rot_cmd.dst_len,
				&rot_cmd.dst_planes);
	}

	sde_hw_rot_adjust_prefill_bw(hw, data, &rot_cmd.prefill_bw);

	/* only process any command if client is master or for validation */
	if (data->master || hw_cmd == SDE_HW_ROT_CMD_VALIDATE) {
		SDE_DEBUG("dispatch seq:%d cmd:%d\n", data->sequence_id,
				hw_cmd);

		rc = sde_rotator_inline_commit(hw->rot_ctx, &rot_cmd, cmd_type);
		if (rc)
			return rc;

		/* return to caller */
		data->priv_handle = rot_cmd.priv_handle;
	} else {
		SDE_DEBUG("bypass seq:%d cmd:%d\n", data->sequence_id, hw_cmd);
	}

	return 0;
}

/**
 * sde_hw_rot_get_format_caps - get pixel format capability
 * @hw: Pointer to rotator hardware driver
 * return: Pointer to pixel format capability array: NULL otherwise
 */
static const struct sde_format_extended *sde_hw_rot_get_format_caps(
		struct sde_hw_rot *hw)
{
	int rc, i, j, len;
	u32 *v4l_pixfmts;
	struct sde_format_extended *drm_pixfmts;
	struct platform_device *pdev;

	if (!hw || !hw->caps || !hw->caps->pdev) {
		SDE_ERROR("invalid rotator hw\n");
		return NULL;
	}

	pdev = hw->caps->pdev;

	if (hw->format_caps)
		return hw->format_caps;

	len = sde_rotator_inline_get_pixfmt_caps(pdev, true, NULL, 0);
	if (len < 0) {
		SDE_ERROR("invalid pixfmt caps %d\n", len);
		return NULL;
	}

	v4l_pixfmts = kcalloc(len, sizeof(u32), GFP_KERNEL);
	if (!v4l_pixfmts)
		goto done;

	sde_rotator_inline_get_pixfmt_caps(pdev, true, v4l_pixfmts, len);

	/* allocate one more to indicate termination */
	drm_pixfmts = kzalloc((len + 1) * sizeof(struct sde_format_extended),
			GFP_KERNEL);
	if (!drm_pixfmts)
		goto done;

	for (i = 0, j = 0; i < len; i++) {
		rc = sde_hw_rot_to_drm_pixfmt(v4l_pixfmts[i],
				&drm_pixfmts[j].fourcc_format,
				&drm_pixfmts[j].modifier);
		if (!rc) {
			SDE_DEBUG("%d: vl42:%c%c%c%c => drm:%c%c%c%c/0x%llx\n",
				i, v4l_pixfmts[i] >> 0, v4l_pixfmts[i] >> 8,
				v4l_pixfmts[i] >> 16, v4l_pixfmts[i] >> 24,
				drm_pixfmts[j].fourcc_format >> 0,
				drm_pixfmts[j].fourcc_format >> 8,
				drm_pixfmts[j].fourcc_format >> 16,
				drm_pixfmts[j].fourcc_format >> 24,
				drm_pixfmts[j].modifier);
			j++;
		} else {
			SDE_DEBUG("%d: vl42:%c%c%c%c not mapped\n",
				i, v4l_pixfmts[i] >> 0, v4l_pixfmts[i] >> 8,
				v4l_pixfmts[i] >> 16, v4l_pixfmts[i] >> 24);
		}
	}

	hw->format_caps = drm_pixfmts;
done:
	kfree(v4l_pixfmts);

	return hw->format_caps;
}

/**
 * sde_hw_rot_get_downscale_caps - get scaling capability string
 * @hw: Pointer to rotator hardware driver
 * return: Pointer to capability string: NULL otherwise
 */
static const char *sde_hw_rot_get_downscale_caps(struct sde_hw_rot *hw)
{
	int len;
	struct platform_device *pdev;

	if (!hw || !hw->caps || !hw->caps->pdev) {
		SDE_ERROR("invalid rotator hw\n");
		return NULL;
	}

	pdev = hw->caps->pdev;

	if (hw->downscale_caps)
		return hw->downscale_caps;

	len = sde_rotator_inline_get_downscale_caps(pdev, NULL, 0);
	if (len < 0) {
		SDE_ERROR("invalid scaling caps %d\n", len);
		return NULL;
	}

	/* add one for ending zero */
	len += 1;
	hw->downscale_caps = kzalloc(len, GFP_KERNEL);
	sde_rotator_inline_get_downscale_caps(pdev, hw->downscale_caps, len);

	return hw->downscale_caps;
}

/**
 * sde_hw_rot_get_cache_size - get cache size
 * @hw: Pointer to rotator hardware driver
 * return: size of cache
 */
static size_t sde_hw_rot_get_cache_size(struct sde_hw_rot *hw)
{
	if (!hw || !hw->caps) {
		SDE_ERROR("invalid rotator hw\n");
		return 0;
	}

	return hw->caps->slice_size;
}

/**
 * sde_hw_rot_get_maxlinewidth - get maximum line width of rotator
 * @hw: Pointer to rotator hardware driver
 * return: maximum line width
 */
static int sde_hw_rot_get_maxlinewidth(struct sde_hw_rot *hw)
{
	struct platform_device *pdev;

	if (!hw || !hw->caps || !hw->caps->pdev) {
		SDE_ERROR("invalid rotator hw\n");
		return 0;
	}

	pdev = hw->caps->pdev;

	return sde_rotator_inline_get_maxlinewidth(pdev);
}

/**
 * _setup_rot_ops - setup rotator operations
 * @ops: Pointer to operation table
 * @features: available feature bitmask
 * return: none
 */
static void _setup_rot_ops(struct sde_hw_rot_ops *ops, unsigned long features)
{
	ops->commit = sde_hw_rot_commit;
	ops->get_format_caps = sde_hw_rot_get_format_caps;
	ops->get_downscale_caps = sde_hw_rot_get_downscale_caps;
	ops->get_cache_size = sde_hw_rot_get_cache_size;
	ops->get_maxlinewidth = sde_hw_rot_get_maxlinewidth;
}

/**
 * sde_hw_rot_blk_stop - stop rotator block
 * @hw_blk: Pointer to base hardware block
 * return: none
 */
static void sde_hw_rot_blk_stop(struct sde_hw_blk *hw_blk)
{
	struct sde_hw_rot *hw_rot = to_sde_hw_rot(hw_blk);

	SDE_DEBUG("type:%d id:%d\n", hw_blk->type, hw_blk->id);

	sde_hw_rot_stop(hw_rot);
}

/**
 * sde_hw_rot_blk_start - art rotator block
 * @hw_blk: Pointer to base hardware block
 * return: 0 if success; error code otherwise
 */
static int sde_hw_rot_blk_start(struct sde_hw_blk *hw_blk)
{
	struct sde_hw_rot *hw_rot = to_sde_hw_rot(hw_blk);
	int rc = 0;

	SDE_DEBUG("type:%d id:%d\n", hw_blk->type, hw_blk->id);

	rc = sde_hw_rot_start(hw_rot);

	return rc;
}

static struct sde_hw_blk_ops sde_hw_rot_ops = {
	.start = sde_hw_rot_blk_start,
	.stop = sde_hw_rot_blk_stop,
};

/**
 * sde_hw_rot_init - create/initialize given rotator instance
 * @idx: index of given rotator
 * @addr: i/o address mapping
 * @m: Pointer to mdss catalog
 * return: Pointer to hardware rotator driver of the given instance
 */
struct sde_hw_rot *sde_hw_rot_init(enum sde_rot idx,
		void __iomem *addr,
		struct sde_mdss_cfg *m)
{
	struct sde_hw_rot *c;
	struct sde_rot_cfg *cfg;
	int rc;

	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return ERR_PTR(-ENOMEM);

	cfg = _rot_offset(idx, m, addr, &c->hw);
	if (IS_ERR(cfg)) {
		WARN(1, "Unable to find rot idx=%d\n", idx);
		kfree(c);
		return ERR_PTR(-EINVAL);
	}

	/* Assign ops */
	c->idx = idx;
	c->caps = cfg;
	c->catalog = m;
	_setup_rot_ops(&c->ops, c->caps->features);
	snprintf(c->name, ARRAY_SIZE(c->name), "sde_rot_%d", idx - ROT_0);

	rc = sde_hw_blk_init(&c->base, SDE_HW_BLK_ROT, idx, &sde_hw_rot_ops);
	if (rc) {
		SDE_ERROR("failed to init hw blk %d\n", rc);
		goto blk_init_error;
	}

	return c;

blk_init_error:
	kzfree(c);

	return ERR_PTR(rc);
}

/**
 * sde_hw_rot_destroy - destroy given hardware rotator driver
 * @hw_rot: Pointer to hardware rotator driver
 * return: none
 */
void sde_hw_rot_destroy(struct sde_hw_rot *hw_rot)
{
	if (hw_rot) {
		sde_hw_blk_destroy(&hw_rot->base);
		kfree(hw_rot->downscale_caps);
		kfree(hw_rot->format_caps);
	}
	kfree(hw_rot);
}

struct sde_hw_rot *sde_hw_rot_get(struct sde_hw_rot *hw_rot)
{
	struct sde_hw_blk *hw_blk = sde_hw_blk_get(hw_rot ? &hw_rot->base :
			NULL, SDE_HW_BLK_ROT, -1);

	return IS_ERR_OR_NULL(hw_blk) ? NULL : to_sde_hw_rot(hw_blk);
}

void sde_hw_rot_put(struct sde_hw_rot *hw_rot)
{
	struct sde_hw_blk *hw_blk = hw_rot ? &hw_rot->base : NULL;

	sde_hw_blk_put(hw_blk);
}
