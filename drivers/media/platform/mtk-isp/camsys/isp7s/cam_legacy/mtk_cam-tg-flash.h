/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_CAM_TG_FALSH_H
#define __MTK_CAM_TG_FALSH_H

#include "mtk_cam.h"
#include "mtk_camera-v4l2-controls.h"

void
mtk_cam_tg_flash_req_update(struct mtk_raw_pipeline *pipe,
			    struct mtk_cam_request_stream_data *s_data);

void
mtk_cam_tg_flash_req_setup(struct mtk_cam_ctx *ctx,
			   struct mtk_cam_request_stream_data *s_data);

void
mtk_cam_tg_flash_req_done(struct mtk_cam_request_stream_data *s_data);

int mtk_cam_tg_flash_try_ctrl(struct v4l2_ctrl *ctrl);

int mtk_cam_tg_flash_s_ctrl(struct v4l2_ctrl *ctrl);

#endif /*__MTK_CAM_TG_FALSH_H*/
