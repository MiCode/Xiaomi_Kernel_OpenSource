// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_drm_panel_drv.h"

//static struct mtk_panel_context *ctx_dbi;

int mtk_panel_register_dbi_customization_callback(
		void *dev,
		struct mtk_panel_cust *cust)
{
	return 0;
}

static const struct of_device_id mtk_panel_dbi_of_match[] = {
	{ .compatible = "mediatek,mtk-drm-panel-drv-dbi", },
	{ }
};

MODULE_DEVICE_TABLE(of, mtk_panel_dbi_of_match);

struct platform_driver mtk_drm_panel_dbi_driver = {
	.probe = NULL,
	.remove = NULL,
	.driver = {
		.name = "mtk-drm-panel-drv-dbi",
		.owner = THIS_MODULE,
		.of_match_table = mtk_panel_dbi_of_match,
	},
};
//module_platform_driver(mtk_drm_panel_dbi_driver);

MODULE_AUTHOR("Cui Zhang <cui.zhang@mediatek.com>");
MODULE_DESCRIPTION("mediatek, drm panel dbi driver");
MODULE_LICENSE("GPL v2");
