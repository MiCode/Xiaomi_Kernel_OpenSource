/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_MML_DRM_ADAPTOR_H__
#define __MTK_MML_DRM_ADAPTOR_H__

#include <linux/platform_device.h>
#include <linux/types.h>

#include "mtk-mml.h"

struct mml_drm_ctx;

/*
 * mml_drm_query_cap - Query current running mode and possible support mode
 * for specific frame info.
 *
 * @info:	Frame info which describe frame process by mml.
 *
 * Return:	Capability result which include running and target mode by
 *		giving info.
 */
struct mml_cap mml_drm_query_cap(struct mml_frame_info *info);

/*
 * mml_drm_get_context - Get mml drm context to control mml.
 *
 * pdev:	The mml driver platform device pointer. Client driver must call
 *		mml_get_plat_device by giving user client driver platoform
 *		device which contains "mediatek,mml" property link to mml node
 *		in dts.
 *
 * Return:	The drm context pointer to represent mml driver instance.
 *
 */
struct mml_drm_ctx *mml_drm_get_context(struct platform_device *pdev);

/*
 * mml_drm_put_context - Release mml drm context and related cached info
 * inside this context.
 *
 * @ctx:	The drm context instance.
 */
void mml_drm_put_context(struct mml_drm_ctx *ctx);

/*
 * mml_drm_submit - submit mml job
 *
 * @ctx:	Context of mml drm adaptor. Get by mml_drm_get_context API.
 * @submit:	Frame info which want mml driver to execute.
 *
 * Return:	Result of submit. In value < 0 case job did not send to mml
 *		driver core.
 */
s32 mml_drm_submit(struct mml_drm_ctx *ctx, struct mml_submit *submit);

#endif	/* __MTK_MML_DRM_ADAPTOR_H__ */
