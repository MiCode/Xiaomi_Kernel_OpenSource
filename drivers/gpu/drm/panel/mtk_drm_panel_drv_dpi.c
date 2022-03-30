// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_drm_panel_drv.h"

//static struct mtk_panel_context *ctx_dpi;

int mtk_panel_register_dpi_customization_callback(
		void *dev,
		struct mtk_panel_cust *cust)
{
	return 0;
}

static const struct of_device_id mtk_panel_dpi_of_match[] = {
	{ .compatible = "mediatek,mtk-drm-panel-drv-dpi", },
	{ }
};

MODULE_DEVICE_TABLE(of, mtk_panel_dpi_of_match);

struct platform_driver mtk_drm_panel_dpi_driver = {
	.probe = NULL,
	.remove = NULL,
	.driver = {
		.name = "mtk-drm-panel-drv-dpi",
		.owner = THIS_MODULE,
		.of_match_table = mtk_panel_dpi_of_match,
	},
};
//module_platform_driver(mtk_drm_panel_dpi_driver);

MODULE_AUTHOR("Cui Zhang <cui.zhang@mediatek.com>");
MODULE_DESCRIPTION("mediatek, drm panel dpi driver");
MODULE_LICENSE("GPL v2");
