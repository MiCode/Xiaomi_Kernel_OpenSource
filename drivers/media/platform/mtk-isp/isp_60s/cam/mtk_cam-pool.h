/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef __MTK_CAM_POOL_H
#define __MTK_CAM_POOL_H

#include <linux/dma-mapping.h>

struct mtk_cam_device;

int mtk_cam_working_buf_pool_init(struct mtk_cam_device *cam);

void mtk_cam_working_buf_pool_release(struct mtk_cam_device *cam);

void mtk_cam_working_buf_put_one(struct mtk_cam_device *cam);

void mtk_cam_working_buf_put(struct mtk_cam_device *cam,
			     struct mtk_cam_working_buf_entry *buf_entry);
struct mtk_cam_working_buf_entry*
mtk_cam_working_buf_get(struct mtk_cam_device *cam);

#endif
