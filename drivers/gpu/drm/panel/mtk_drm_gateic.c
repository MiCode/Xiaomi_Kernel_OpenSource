// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_drm_gateic.h"

static struct mtk_gateic_funcs *mtk_gateic_dbi_ops;
static struct mtk_gateic_funcs *mtk_gateic_dpi_ops;
static struct mtk_gateic_funcs *mtk_gateic_dsi_ops;

struct mtk_gateic_funcs *mtk_drm_gateic_get_ops(char func)
{
	struct mtk_gateic_funcs *ops = NULL;

	switch (func) {
	case MTK_LCM_FUNC_DBI:
		ops = mtk_gateic_dbi_ops;
		break;
	case MTK_LCM_FUNC_DPI:
		ops = mtk_gateic_dpi_ops;
		break;
	case MTK_LCM_FUNC_DSI:
		ops = mtk_gateic_dsi_ops;
		break;
	default:
		DDPMSG("%s: invalid func:%d\n", __func__, func);
		ops = NULL;
		break;
	}

	return ops;
}

int mtk_drm_gateic_set(struct mtk_gateic_funcs *gateic_ops,
		char func)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(gateic_ops))
		return -EFAULT;

	switch (func) {
	case MTK_LCM_FUNC_DBI:
		if (mtk_gateic_dbi_ops != NULL) {
			DDPMSG("%s, DBI gateic repeat settings\n", __func__);
			return -EEXIST;
		}
		mtk_gateic_dbi_ops = gateic_ops;
		DDPMSG("%s: DBI gateic ops:0x%lx-0x%lx\n",
			__func__, (unsigned long)mtk_gateic_dbi_ops,
			(unsigned long)gateic_ops);
		break;
	case MTK_LCM_FUNC_DPI:
		if (mtk_gateic_dpi_ops != NULL) {
			DDPMSG("%s, DPI gateic repeat settings\n", __func__);
			return -EEXIST;
		}
		mtk_gateic_dpi_ops = gateic_ops;
		DDPMSG("%s: DPI gateic ops:0x%lx-0x%lx\n",
			__func__, (unsigned long)mtk_gateic_dpi_ops,
			(unsigned long)gateic_ops);
		break;
	case MTK_LCM_FUNC_DSI:
		if (mtk_gateic_dsi_ops != NULL) {
			DDPMSG("%s, DSI gateic repeat settings\n", __func__);
			return -EEXIST;
		}
		mtk_gateic_dsi_ops = gateic_ops;
		DDPMSG("%s: DSI gateic ops:0x%lx-0x%lx\n",
			__func__, (unsigned long)mtk_gateic_dsi_ops,
			(unsigned long)gateic_ops);
		break;
	default:
		DDPMSG("%s: invalid func:%d\n", __func__, func);
		ret = -EINVAL;
		break;
	}

	return ret;
}
EXPORT_SYMBOL(mtk_drm_gateic_set);

struct mtk_gateic_funcs *mtk_drm_gateic_get(char func)
{
	return mtk_drm_gateic_get_ops(func);
}
EXPORT_SYMBOL(mtk_drm_gateic_get);

int mtk_drm_gateic_power_on(char func)
{
	struct mtk_gateic_funcs *ops  = mtk_drm_gateic_get_ops(func);

	if (IS_ERR_OR_NULL(ops) || IS_ERR_OR_NULL(ops->power_on))
		return -EFAULT;

	return ops->power_on();
}
EXPORT_SYMBOL(mtk_drm_gateic_power_on);

int mtk_drm_gateic_power_off(char func)
{
	struct mtk_gateic_funcs *ops  = mtk_drm_gateic_get_ops(func);

	if (IS_ERR_OR_NULL(ops) || IS_ERR_OR_NULL(ops->power_off))
		return -EFAULT;

	return ops->power_off();
}
EXPORT_SYMBOL(mtk_drm_gateic_power_off);

int mtk_drm_gateic_set_voltage(enum vol_level level,
		char func)
{
	struct mtk_gateic_funcs *ops  = mtk_drm_gateic_get_ops(func);

	if (IS_ERR_OR_NULL(ops) || IS_ERR_OR_NULL(ops->set_voltage))
		return -EFAULT;

	return ops->set_voltage(level);
}
EXPORT_SYMBOL(mtk_drm_gateic_set_voltage);

int mtk_drm_gateic_reset(int on, char func)
{
	struct mtk_gateic_funcs *ops  = mtk_drm_gateic_get_ops(func);

	if (IS_ERR_OR_NULL(ops) || IS_ERR_OR_NULL(ops->reset))
		return -EFAULT;

	return ops->reset(on);
}
EXPORT_SYMBOL(mtk_drm_gateic_reset);

static struct platform_driver *const mtk_drm_gateic_drivers[] = {
	&mtk_gateic_rt4801h_driver,
};

static int __init mtk_drm_gateic_init(void)
{
	int ret;
	unsigned int i;

	DDPMSG("%s+\n", __func__);
	for (i = 0; i < ARRAY_SIZE(mtk_drm_gateic_drivers); i++) {
		ret = platform_driver_register(mtk_drm_gateic_drivers[i]);
		if (ret < 0) {
			DDPPR_ERR("Failed to register %s driver: %d\n",
				  mtk_drm_gateic_drivers[i]->driver.name, ret);
			goto err;
		}
	}
	DDPMSG("%s-\n", __func__);

	ret = i2c_add_driver(&mtk_panel_i2c_driver);
	if (ret < 0) {
		DDPPR_ERR("Failed to register i2c driver: %d\n",
			  ret);
		return ret;
	}

	return 0;

err:
	while (--i >= 0)
		platform_driver_unregister(mtk_drm_gateic_drivers[i]);

	return ret;
}

static void __exit mtk_drm_gateic_exit(void)
{
	int i;

	for (i = ARRAY_SIZE(mtk_drm_gateic_drivers) - 1; i >= 0; i--)
		platform_driver_unregister(mtk_drm_gateic_drivers[i]);

	i2c_del_driver(&mtk_panel_i2c_driver);
}
module_init(mtk_drm_gateic_init);
module_exit(mtk_drm_gateic_exit);

MODULE_AUTHOR("Cui Zhang <cui.zhang@mediatek.com>");
MODULE_DESCRIPTION("mediatek, drm panel gateic driver");
MODULE_LICENSE("GPL v2");
