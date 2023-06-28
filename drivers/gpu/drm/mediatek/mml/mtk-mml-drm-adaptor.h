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

struct mml_drm_ctx;
struct cmdq_pkt;

/* default lcm pixel, helps calculate HRT bandwidth */
#define MML_DEFAULT_PANEL_PX	(1080 * 2412)
#define MML_HRT_FPS		120

struct mml_drm_param {
	/* [in]set true if display uses dual pipe */
	bool dual;

	/* [in]set true if display uses vdo mode, false for cmd mode */
	bool vdo_mode;

	/* submit done callback api */
	void (*submit_cb)(void *cb_param);

	/* [out]The height of racing mode for each output tile in pixel. */
	u8 racing_height;
};

/*
 * mml_drm_query_cap - Query current running mode and possible support mode
 * for specific frame info.
 *
 * @ctx:	Context of mml drm adaptor. Get by mml_drm_get_context API.
 * @info:	Frame info which describe frame process by mml.
 *
 * Return:	Capability result of target mode by giving info.
 */
enum mml_mode mml_drm_query_cap(struct mml_drm_ctx *ctx,
				struct mml_frame_info *info);

/*
 * mml_drm_try_frame - Try/adjust frame info to match mml hardware spec.
 *
 * @ctx:	Context of mml drm adaptor. Get by mml_drm_get_context API.
 * @info:	Frame info which describe frame process by mml.
 */
void mml_drm_try_frame(struct mml_drm_ctx *ctx, struct mml_frame_info *info);

/*
 * mml_drm_get_context - Get mml drm context to control mml.
 *
 * pdev:	The mml driver platform device pointer. Client driver must call
 *		mml_get_plat_device by giving user client driver platoform
 *		device which contains "mediatek,mml" property link to mml node
 *		in dts.
 * @disp:	Display parameters. See struct.
 *
 * Return:	The drm context pointer to represent mml driver instance.
 *
 */
struct mml_drm_ctx *mml_drm_get_context(struct platform_device *pdev,
	struct mml_drm_param *disp);

/*
 * mml_drm_ctx_idle - Check if all tasks in this drm ctx stop.
 *
 * @ctx:	The drm context instance.
 */
bool mml_drm_ctx_idle(struct mml_drm_ctx *ctx);

/*
 * mml_drm_put_context - Release mml drm context and related cached info
 * inside this context.
 *
 * @ctx:	The drm context instance.
 */
void mml_drm_put_context(struct mml_drm_ctx *ctx);

/*
 * mml_drm_set_panel_pixel - Set pixel count of display panel (lcm) pixel count
 * This value helps mml calculate HRT bandwidth. See frame_calc_layer_hrt for
 * more detail.
 *
 * Note this API also update currecnt existing frame config HRT base on new
 * panel pixel count.
 *
 * @ctx:	The drm context instance.
 * @pixel:	Pixel count of panel. Default value is 1080 * 2412.
 */
void mml_drm_set_panel_pixel(struct mml_drm_ctx *ctx, u32 pixel);

/*
 * mml_drm_racing_config_sync - append event sync instructions to disp pkt
 *
 * @ctx:	The drm context instance.
 * @pkt:	The pkt to append cmdq instructions, which helps this pkt
 *		and mml pkt execute at same time.
 *
 * return:	0 if success and < 0 error no if fail
 */
s32 mml_drm_racing_config_sync(struct mml_drm_ctx *ctx, struct cmdq_pkt *pkt);

/*
 * mml_drm_racing_stop_sync - append event sync instructions to disp pkt
 *
 * @ctx:	The drm context instance.
 * @pkt:	The pkt to append cmdq instructions, which helps this pkt
 *		and mml pkt execute at same time.
 *
 * return:	0 if success and < 0 error no if fail
 */
s32 mml_drm_racing_stop_sync(struct mml_drm_ctx *ctx, struct cmdq_pkt *pkt);

/*
 * mml_drm_split_info - split submit info to racing info and pq info
 *
 * @ctx:	Context of mml drm adaptor. Get by mml_drm_get_context API.
 * @submit:	[in/out]Frame info which want mml driver to execute. The info
 *		data inside this submit will change for racing mode
 * @submit_pq:	[out]Frame info for pq engines separate from submit
 */
void mml_drm_split_info(struct mml_submit *submit, struct mml_submit *submit_pq);

/*
 * mml_drm_submit - submit mml job
 *
 * @ctx:	Context of mml drm adaptor. Get by mml_drm_get_context API.
 * @submit:	Frame info which want mml driver to execute.
 * @cb_param:	The parameter used in submit done callback (if registered).
 *
 * Return:	Result of submit. In value < 0 case job did not send to mml
 *		driver core.
 */
s32 mml_drm_submit(struct mml_drm_ctx *ctx, struct mml_submit *submit,
	void *cb_param);

/*
 * mml_drm_stop - stop mml task (for racing mode)
 *
 * @ctx:	Context of mml drm adaptor. Get by mml_drm_get_context API.
 * @submit:	Frame info which want mml driver to execute.
 * @force:	true to use cmdq stop gce hardware thread, false to set next_spr
 *		to next only.
 *
 * Return:	Result of submit. In value < 0 case job did not send to mml
 *		driver core.
 */
s32 mml_drm_stop(struct mml_drm_ctx *ctx, struct mml_submit *submit, bool force);

/*
 * mml_drm_config_rdone - append instruction to config mmlsys rdone sel to
 *		default, which makes mmlsys always active rdone. This avoid
 *		mml task hang if disp stop.
 *
 * @ctx:	Context of mml drm adaptor. Get by mml_drm_get_context API.
 * @submit:	Frame info which want mml driver to execute.
 * @pkt:	The pkt to append cmdq instructions.
 *
 * Return:	Result of submit. In value < 0 case job did not send to mml
 *		driver core.
 */
void mml_drm_config_rdone(struct mml_drm_ctx *ctx, struct mml_submit *submit,
	struct cmdq_pkt *pkt);

/*
 * mml_drm_dump - dump cmdq thread status for mml
 *
 * @ctx:	Context of mml drm adaptor. Get by mml_drm_get_context API.
 * @submit:	Frame info which want mml driver to execute.
 */
void mml_drm_dump(struct mml_drm_ctx *ctx, struct mml_submit *submit);

#endif	/* __MTK_MML_DRM_ADAPTOR_H__ */
