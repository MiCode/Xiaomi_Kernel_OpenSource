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

#ifndef __MTK_RSC_H__
#define __MTK_RSC_H__

#include "mtk_rsc-v4l2.h"

#define MTK_RSC_MAX_NO		(1024)

struct v4l2_rsc_init_param {
	uint32_t	reg_base;
	uint32_t	reg_range;
} __attribute__ ((__packed__));

struct v4l2_rsc_img_plane_format {
	uint32_t	size;
	uint16_t	stride;
} __attribute__ ((__packed__));

struct rsc_img_pix_format {
	uint16_t                    width;
	uint16_t                    height;
	struct v4l2_rsc_img_plane_format plane_fmt[1];
} __attribute__ ((__packed__));

struct rsc_img_buffer {
	struct rsc_img_pix_format  format;
	uint32_t                   iova[1];
} __attribute__ ((__packed__));

struct rsc_meta_buffer {
	uint64_t	va;	/* Used by APMCU access */
	uint32_t	pa;	/* Used by CM4 access */
	uint32_t	iova;	/* Used by IOMMU HW access */
} __attribute__ ((__packed__));

struct v4l2_rsc_frame_param {
	struct rsc_img_buffer     pre_rrzo_in[1];
	struct rsc_img_buffer     cur_rrzo_in[1];
	struct rsc_meta_buffer    meta_out;
	uint32_t                  tuning_data[MTK_ISP_CTX_RSC_TUNING_DATA_NUM];
	uint32_t                  frame_id;
} __attribute__ ((__packed__));

/**
 * mtk_rsc_enqueue - enqueue to rsc driver
 *
 * @pdev: RSC platform device
 * @fdvtconfig: frame parameters from V4L2 common Framework
 *
 * Enqueue a frame to rsc driver.
 *
 * Return: Return 0 if successfully, otherwise it is failed.
 */
int mtk_rsc_enqueue(struct platform_device *pdev,
		   struct v4l2_rsc_frame_param *rsc_param);

/**
 * mtk_rsc_open -
 *
 * @pdev: RSC platform device
 *
 * Open the RSC device driver
 *
 * Return: Return 0 if success, otherwise it is failed.
 */
int mtk_rsc_open(struct platform_device *pdev);

/**
 * mtk_rsc_release -
 *
 * @pdev: RSC platform device
 *
 * Enqueue a frame to RSC driver.
 *
 * Return: Return 0 if success, otherwise it is failed.
 */
int mtk_rsc_release(struct platform_device *pdev);

/**
 * mtk_rsc_streamon -
 *
 * @pdev: RSC platform device
 * @id: device context id
 *
 * Stream on
 *
 * Return: Return 0 if success, otherwise it is failed.
 */
int mtk_rsc_streamon(struct platform_device *pdev, u16 id);

/**
 * mtk_rsc_streamoff -
 *
 * @pdev: RSC platform device
 * @id: device context id
 *
 * Stream off
 *
 * Return: Return 0 if success, otherwise it is failed.
 */
int mtk_rsc_streamoff(struct platform_device *pdev, u16 id);

#endif/*__MTK_RSC_H__*/
