/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2019 MediaTek Inc. */

#ifndef __MTK_CAM_SENINF_ROUTE_H__
#define __MTK_CAM_SENINF_ROUTE_H__

void mtk_cam_seninf_init_res(struct seninf_core *core);

struct seninf_mux *mtk_cam_seninf_mux_get(struct seninf_ctx *ctx);
struct seninf_mux *mtk_cam_seninf_mux_get_pref(struct seninf_ctx *ctx,
					       int *pref_idx, int pref_cnt);
void mtk_cam_seninf_mux_put(struct seninf_ctx *ctx, struct seninf_mux *mux);
void mtk_cam_seninf_release_mux(struct seninf_ctx *ctx);

void mtk_cam_seninf_get_vcinfo_test(struct seninf_ctx *ctx);

struct seninf_vc *mtk_cam_seninf_get_vc_by_pad(struct seninf_ctx *ctx, int idx);

int mtk_cam_seninf_get_vcinfo(struct seninf_ctx *ctx);

int mtk_cam_seninf_is_vc_enabled(struct seninf_ctx *ctx,
				 struct seninf_vc *vc);

int mtk_cam_seninf_is_di_enabled(struct seninf_ctx *ctx, u8 ch, u8 dt);

int notify_fsync_cammux_usage(struct seninf_ctx *ctx);
void notify_fsync_cammux_usage_with_kthread(struct seninf_ctx *ctx);
int mtk_cam_seninf_get_csi_param(struct seninf_ctx *ctx);
u8 is_reset_by_user(struct seninf_ctx *ctx);
int reset_sensor(struct seninf_ctx *ctx);


#ifdef SENINF_DEBUG
void mtk_cam_seninf_alloc_cam_mux(struct seninf_ctx *ctx);
void mtk_cam_seninf_release_cam_mux(struct seninf_ctx *ctx);
#endif

#endif
