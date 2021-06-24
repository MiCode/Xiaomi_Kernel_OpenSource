/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Dennis YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */

#ifndef __MTK_MML_DRM_ADAPTOR_H__
#define __MTK_MML_DRM_ADAPTOR_H__

#include <linux/platform_device.h>
#include <linux/types.h>
#include "mtk-mml.h"
#include "mediatek_v2/mtk_drm_ddp_comp.h"

struct mml_drm_ctx;

/* mml_topology_query_mode - define in mtk-mml-tp-mt68xx.c
 *
 * @info:	Frame info which describe frame process by mml.
 *
 * return:	mml mode to run this frame
 */
enum mml_mode mml_topology_query_mode(struct mml_frame_info *info);

/*
 * mml_drm_query_cap - Query current running mode and possible support mode
 * for specific frame info.
 *
 * @info:	Frame info which describe frame process by mml.
 *
 * Return:	Capability result of target mode by giving info.
 */
enum mml_mode mml_drm_query_cap(struct mml_frame_info *info);

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

/*
 * mml_ddp_comp_register - register ddp component to drm
 *
 * @drm:	Device of drm.
 * @comp:	The component that will be reigster to drm.
 *
 * Return:	Result of component register. In case value < 0 means registeration fail
 */
int mml_ddp_comp_register(struct drm_device *drm, struct mtk_ddp_comp *comp);


/*
 * mml_ddp_comp_unregister - unregister ddp component to drm
 *
 * @drm:	Device of drm.
 * @comp:	The component that will be reigster to drm.
 *
 * Return:	None
 */
void mml_ddp_comp_unregister(struct drm_device *drm, struct mtk_ddp_comp *comp);

#endif	/* __MTK_MML_DRM_ADAPTOR_H__ */
