/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2019 MediaTek Inc. */

#ifndef __MTK_CAM_SENINF_ROUTE_H__
#define __MTK_CAM_SENINF_ROUTE_H__

void mtk_cam_seninf_init_res(struct seninf_core *core);

struct seninf_mux *mtk_cam_seninf_mux_get_by_type(struct seninf_ctx *ctx,
						enum CAM_TYPE_ENUM cam_type);
void mtk_cam_seninf_alloc_cammux(struct seninf_ctx *ctx);
void mtk_cam_seninf_mux_put(struct seninf_ctx *ctx, struct seninf_mux *mux);
void mtk_cam_seninf_release_mux(struct seninf_ctx *ctx);

void mtk_cam_seninf_get_vcinfo_test(struct seninf_ctx *ctx);

struct seninf_vc *mtk_cam_seninf_get_vc_by_pad(struct seninf_ctx *ctx, int idx);

int mtk_cam_seninf_get_vcinfo(struct seninf_ctx *ctx);

int mtk_cam_seninf_is_vc_enabled(struct seninf_ctx *ctx,
				 struct seninf_vc *vc);

int mtk_cam_seninf_is_di_enabled(struct seninf_ctx *ctx, u8 ch, u8 dt);

int notify_fsync_listen_target(struct seninf_ctx *ctx);
void notify_fsync_listen_target_with_kthread(struct seninf_ctx *ctx,
	const unsigned int mdelay);
int mtk_cam_seninf_get_csi_param(struct seninf_ctx *ctx);
u8 is_reset_by_user(struct seninf_ctx *ctx);
int reset_sensor(struct seninf_ctx *ctx);
int mtk_cam_seninf_s_stream_mux(struct seninf_ctx *ctx);

#ifdef SENINF_DEBUG
void mtk_cam_seninf_release_cam_mux(struct seninf_ctx *ctx);
int mux2mux_vr(struct seninf_ctx *ctx, int mux, int cammux, int vc_idx);
int mux_vr2mux(struct seninf_ctx *ctx, int mux_vr);
enum CAM_TYPE_ENUM cammux2camtype(struct seninf_ctx *ctx, int cammux);
#endif

bool has_multiple_expo_mode(struct seninf_ctx *ctx);

int aov_switch_i2c_bus_scl_aux(struct seninf_ctx *ctx,
	enum mtk_cam_sensor_i2c_bus_scl aux);
int aov_switch_i2c_bus_sda_aux(struct seninf_ctx *ctx,
	enum mtk_cam_sensor_i2c_bus_sda aux);

int aov_switch_pm_ops(struct seninf_ctx *ctx,
	enum mtk_cam_sensor_pm_ops pm_ops);

#endif
