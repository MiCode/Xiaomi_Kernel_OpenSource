/* SPDX-License-Identifier: GPL-2.0 */
// Copyright (c) 2019 MediaTek Inc.

#ifndef __MTK_CAM_SENINF_IF_H__
#define __MTK_CAM_SENINF_IF_H__

int mtk_cam_seninf_get_pixelmode(struct v4l2_subdev *sd, int pad_id,
				 int *pixelmode);

int mtk_cam_seninf_set_pixelmode(struct v4l2_subdev *sd, int pad_id,
				 int pixelmode);

int mtk_cam_seninf_set_camtg(struct v4l2_subdev *sd, int pad_id, int camtg);

int mtk_cam_seninf_get_pixelrate(struct v4l2_subdev *sd, s64 *pixelrate);

#endif
