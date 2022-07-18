/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef __MTK_CAM_POOL_H
#define __MTK_CAM_POOL_H

struct mtk_cam_ctx;

int mtk_cam_working_buf_pool_alloc(struct mtk_cam_ctx *ctx);
int mtk_cam_working_buf_pool_init(struct mtk_cam_ctx *ctx);
void mtk_cam_working_buf_pool_release(struct mtk_cam_ctx *ctx);
void
mtk_cam_working_buf_put(struct mtk_cam_working_buf_entry *buf_entry);
struct mtk_cam_working_buf_entry*
mtk_cam_working_buf_get(struct mtk_cam_ctx *ctx);

int mtk_cam_user_img_working_buf_pool_init(struct mtk_cam_ctx *ctx,
					   int buf_num,
					   int working_buf_size);
int mtk_cam_internal_img_working_buf_pool_init(struct mtk_cam_ctx *ctx,
					       int buf_num,
					       int working_buf_size);
void mtk_cam_user_img_working_buf_pool_release(struct mtk_cam_ctx *ctx);
void mtk_cam_internal_img_working_buf_pool_release(struct mtk_cam_ctx *ctx);
void
mtk_cam_img_working_buf_put(struct mtk_cam_img_working_buf_entry *buf_entry);
struct mtk_cam_img_working_buf_entry*
mtk_cam_img_working_buf_get(struct mtk_cam_ctx *ctx);

long mtk_cam_generic_buf_alloc(struct mtk_cam_ctx *ctx, u32 size);
void mtk_cam_generic_buf_release(struct mtk_cam_ctx *ctx);

int mtk_cam_sv_working_buf_pool_init(struct mtk_cam_ctx *ctx);
void
mtk_cam_sv_working_buf_put(struct mtk_camsv_working_buf_entry *buf_entry);

struct mtk_camsv_working_buf_entry*
mtk_cam_sv_working_buf_get(struct mtk_cam_ctx *ctx);

int mtk_cam_mraw_working_buf_pool_init(struct mtk_cam_ctx *ctx);
void mtk_cam_mraw_working_buf_put(struct mtk_cam_ctx *ctx,
				struct mtk_mraw_working_buf_entry *buf_entry);
struct mtk_mraw_working_buf_entry*
mtk_cam_mraw_working_buf_get(struct mtk_cam_ctx *ctx);
int mtk_cam_get_internl_buf_num(int user_reserved_exp_num, struct mtk_cam_scen *scen, int hw_mode);

#endif
