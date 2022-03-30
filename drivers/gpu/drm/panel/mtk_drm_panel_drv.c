// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_drm_panel_drv.h"

int mtk_panel_register_drv_customization_callback(char func,
		void *dev, struct mtk_panel_cust *cust)
{
	int ret = 0;

	switch (func) {
	case MTK_LCM_FUNC_DBI:
		ret = mtk_panel_register_dbi_customization_callback(dev, cust);
		break;
	case MTK_LCM_FUNC_DPI:
		ret = mtk_panel_register_dpi_customization_callback(dev, cust);
		break;
	case MTK_LCM_FUNC_DSI:
		ret = mtk_panel_register_dsi_customization_callback(dev, cust);
		break;
	default:
		DDPMSG("%s, invalid func:%d\n", __func__, func);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int __init mtk_drm_panel_drv_init(void)
{
	int ret = 0;

	DDPMSG("%s+\n", __func__);
	ret = platform_driver_register(&mtk_drm_panel_dbi_driver);
	if (ret < 0)
		DDPPR_ERR("%s, Failed to register dbi driver: %d\n",
			__func__, ret);

	ret = platform_driver_register(&mtk_drm_panel_dpi_driver);
	if (ret < 0)
		DDPPR_ERR("%s, Failed to register dpi driver: %d\n",
			__func__, ret);

	ret = mipi_dsi_driver_register(&mtk_drm_panel_dsi_driver);
	if (ret < 0)
		DDPPR_ERR("%s, Failed to register dsi driver: %d\n",
			__func__, ret);

	DDPMSG("%s- ret:%d\n", __func__, ret);
	return 0;
}

static void __exit mtk_drm_panel_drv_exit(void)
{
	DDPMSG("%s+\n", __func__);
	mipi_dsi_driver_unregister(&mtk_drm_panel_dsi_driver);
	platform_driver_unregister(&mtk_drm_panel_dpi_driver);
	platform_driver_unregister(&mtk_drm_panel_dbi_driver);
	DDPMSG("%s-\n", __func__);
}
module_init(mtk_drm_panel_drv_init);
module_exit(mtk_drm_panel_drv_exit);

MODULE_AUTHOR("Cui Zhang <cui.zhang@mediatek.com>");
MODULE_DESCRIPTION("mediatek, drm panel driver");
MODULE_LICENSE("GPL v2");
