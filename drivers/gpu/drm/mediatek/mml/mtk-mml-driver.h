/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MTK_MML_DRIVER_H__
#define __MTK_MML_DRIVER_H__

#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk-cmdq.h>

struct mml_drm_ctx;

struct mml_dev {
	struct platform_device *pdev;
	/* struct mml_comp *comp[MML_ENGINE_TOTAL]; */
	struct cmdq_base *cmdq_base;
	struct cmdq_client *cmdq_clt;

	struct mml_drm_ctx *drm_ctx;
	struct mutex drm_ctx_mutex;
};

struct platform_device *mml_get_plat_device(struct platform_device *pdev);

#endif	/* __MTK_MML_DRIVER_H__ */
