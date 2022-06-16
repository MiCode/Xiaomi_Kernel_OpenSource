// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_drm_gateic.h"

static struct list_head dbi_gateic_list;
static struct list_head dpi_gateic_list;
static struct list_head dsi_gateic_list;
static struct mtk_gateic_funcs *mtk_gateic_dbi_ops;
static struct mtk_gateic_funcs *mtk_gateic_dpi_ops;
static struct mtk_gateic_funcs *mtk_gateic_dsi_ops;

bool mtk_gateic_match_lcm_list(const char *lcm_name,
	const char **list, unsigned int count, const char *gateic_name)
{
	int ret = 0;
	unsigned int i = 0;
	char owner[MTK_LCM_NAME_LENGTH] = "unknown";

	if (count == 0 || IS_ERR_OR_NULL(list)) {
		DDPPR_ERR("%s, invalid list\n", __func__);
		return false;
	}

	if (IS_ERR_OR_NULL(gateic_name) == false)
		ret = snprintf(owner, MTK_LCM_NAME_LENGTH - 1, "%s", gateic_name);
	if (ret < 0 || ret >= MTK_LCM_NAME_LENGTH)
		DDPMSG("%s, snprintf failed:%d\n", __func__, ret);

	if (count == 1 &&
	    strcmp(*list, "default") == 0) {
		DDPMSG("%s, use default gateic:\"%s\"\n",
			__func__, owner);
		return true;
	}

	for (i = 0; i < count; i++) {
		if (strcmp(*(list + i), lcm_name) == 0) {
			DDPMSG("%s, lcm%d:\"%s\" of gateic:\"%s\" matched\n",
				__func__, i, *(list + i), owner);
			return true;
		}
	}
	DDPMSG("%s, gateic:\"%s\" doesn't support lcm:\"%s\", count:%u\n",
		__func__, owner, lcm_name, count);
	return false;
}

static struct mtk_gateic_funcs *mtk_drm_gateic_match_lcm_list(
		struct list_head *gateic_list, const char *lcm_name)
{
	struct mtk_gateic_funcs *ops = NULL;

	list_for_each_entry(ops, gateic_list, list) {
		if (ops->match_lcm_list != NULL) {
			if (ops->match_lcm_list(lcm_name) != 0)
				return ops;
		}
	}
	return NULL;
}

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

int mtk_drm_gateic_register(struct mtk_gateic_funcs *ops, char func)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(ops))
		return -EFAULT;

	switch (func) {
	case MTK_LCM_FUNC_DBI:
		list_add_tail(&ops->list, &dbi_gateic_list);
		break;
	case MTK_LCM_FUNC_DPI:
		list_add_tail(&ops->list, &dpi_gateic_list);
		break;
	case MTK_LCM_FUNC_DSI:
		list_add_tail(&ops->list, &dsi_gateic_list);
		break;
	default:
		DDPMSG("%s: invalid func:%d\n", __func__, func);
		ret = -EINVAL;
		break;
	}

	return ret;
}
EXPORT_SYMBOL(mtk_drm_gateic_register);

int mtk_drm_gateic_select(const char *lcm_name, char func)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	int ret = 0;

	if (IS_ERR_OR_NULL(lcm_name))
		return -EFAULT;

	switch (func) {
	case MTK_LCM_FUNC_DBI:
		if (mtk_gateic_dbi_ops != NULL) {
			DDPMSG("%s, DBI gateic repeat settings\n", __func__);
			return -EEXIST;
		}
		mtk_gateic_dbi_ops = mtk_drm_gateic_match_lcm_list(
				&dbi_gateic_list, lcm_name);
		if (mtk_gateic_dbi_ops == NULL)
			ret = EFAULT;
		break;
	case MTK_LCM_FUNC_DPI:
		if (mtk_gateic_dpi_ops != NULL) {
			DDPMSG("%s, DPI gateic repeat settings\n", __func__);
			return -EEXIST;
		}
		mtk_gateic_dpi_ops = mtk_drm_gateic_match_lcm_list(
				&dpi_gateic_list, lcm_name);
		if (mtk_gateic_dpi_ops == NULL)
			ret = EFAULT;
		break;
	case MTK_LCM_FUNC_DSI:
		if (mtk_gateic_dsi_ops != NULL) {
			DDPMSG("%s, DSI gateic repeat settings\n", __func__);
			return -EEXIST;
		}
		mtk_gateic_dsi_ops = mtk_drm_gateic_match_lcm_list(
				&dsi_gateic_list, lcm_name);
		if (mtk_gateic_dsi_ops == NULL)
			ret = EFAULT;
		break;
	default:
		DDPMSG("%s: invalid func:%d\n", __func__, func);
		ret = -EINVAL;
		break;
	}

	return ret;
#else
	return 0;
#endif
}
EXPORT_SYMBOL(mtk_drm_gateic_select);

struct mtk_gateic_funcs *mtk_drm_gateic_get(char func)
{
	return mtk_drm_gateic_get_ops(func);
}
EXPORT_SYMBOL(mtk_drm_gateic_get);

int mtk_drm_gateic_power_on(char func)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct mtk_gateic_funcs *ops  = mtk_drm_gateic_get_ops(func);

	if (IS_ERR_OR_NULL(ops) || IS_ERR_OR_NULL(ops->power_on))
		return -EFAULT;

	return ops->power_on();
#else
	return 0;
#endif
}
EXPORT_SYMBOL(mtk_drm_gateic_power_on);

int mtk_drm_gateic_power_off(char func)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct mtk_gateic_funcs *ops  = mtk_drm_gateic_get_ops(func);

	if (IS_ERR_OR_NULL(ops) || IS_ERR_OR_NULL(ops->power_off))
		return -EFAULT;

	return ops->power_off();
#else
	return 0;
#endif
}
EXPORT_SYMBOL(mtk_drm_gateic_power_off);

int mtk_drm_gateic_set_voltage(unsigned int level,
		char func)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct mtk_gateic_funcs *ops  = mtk_drm_gateic_get_ops(func);

	if (IS_ERR_OR_NULL(ops) || IS_ERR_OR_NULL(ops->set_voltage))
		return -EFAULT;

	return ops->set_voltage(level);
#else
	return 0;
#endif
}
EXPORT_SYMBOL(mtk_drm_gateic_set_voltage);

int mtk_drm_gateic_reset(int on, char func)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct mtk_gateic_funcs *ops  = mtk_drm_gateic_get_ops(func);

	if (IS_ERR_OR_NULL(ops) || IS_ERR_OR_NULL(ops->reset))
		return -EFAULT;

	return ops->reset(on);
#else
	return 0;
#endif
}
EXPORT_SYMBOL(mtk_drm_gateic_reset);

int mtk_drm_gateic_set_backlight(unsigned int level,
		char func)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct mtk_gateic_funcs *ops  = mtk_drm_gateic_get_ops(func);

	if (IS_ERR_OR_NULL(ops) || IS_ERR_OR_NULL(ops->set_backlight))
		return -EFAULT;

	return ops->set_backlight(level);
#else
	return 0;
#endif
}
EXPORT_SYMBOL(mtk_drm_gateic_set_backlight);

int mtk_drm_gateic_enable_backlight(char func)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	struct mtk_gateic_funcs *ops  = mtk_drm_gateic_get_ops(func);

	if (IS_ERR_OR_NULL(ops) || IS_ERR_OR_NULL(ops->enable_backlight))
		return -EFAULT;

	return ops->enable_backlight();
#else
	return 0;
#endif
}
EXPORT_SYMBOL(mtk_drm_gateic_enable_backlight);

int mtk_drm_gateic_write_bytes(unsigned char addr, unsigned char value, char func)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	return mtk_panel_i2c_write_bytes(addr, value);
#else
	return 0;
#endif
}

int mtk_drm_gateic_read_bytes(unsigned char addr, unsigned char *value, char func)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	return mtk_panel_i2c_read_bytes(addr, value);
#else
	return 0;
#endif
}

int mtk_drm_gateic_write_multiple_bytes(unsigned char addr,
		 unsigned char *value, unsigned int size, char func)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	return mtk_panel_i2c_write_multiple_bytes(addr, value, size);
#else
	return 0;
#endif
}

static struct platform_driver *const mtk_drm_gateic_drivers[] = {
	&mtk_gateic_rt4801h_driver,
	&mtk_gateic_rt4831a_driver,
};

static int __init mtk_drm_gateic_init(void)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	int ret = 0;
	int i = 0;

	DDPMSG("%s+\n", __func__);
	INIT_LIST_HEAD(&dbi_gateic_list);
	INIT_LIST_HEAD(&dpi_gateic_list);
	INIT_LIST_HEAD(&dsi_gateic_list);

	for (i = 0; (unsigned int)i < ARRAY_SIZE(mtk_drm_gateic_drivers); i++) {
		ret = platform_driver_register(mtk_drm_gateic_drivers[i]);
		if (ret < 0) {
			DDPPR_ERR("%s: Failed to register %s driver: %d\n",
				  __func__, mtk_drm_gateic_drivers[i]->driver.name, ret);
			goto err;
		}
	}

	DDPMSG("%s, add i2c driver\n", __func__);
	ret = i2c_add_driver(&mtk_panel_i2c_driver);
	if (ret < 0) {
		DDPPR_ERR("%s, failed to register i2c driver: %d\n",
			  __func__, ret);
		return ret;
	}

	DDPMSG("%s-, ret:%d\n", __func__, ret);
	return 0;

err:
	while (--i >= 0)
		platform_driver_unregister(mtk_drm_gateic_drivers[i]);

	return ret;
#else
	return 0;
#endif
}

static void __exit mtk_drm_gateic_exit(void)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	int i;

	for (i = ARRAY_SIZE(mtk_drm_gateic_drivers) - 1; i >= 0; i--)
		platform_driver_unregister(mtk_drm_gateic_drivers[i]);

	i2c_del_driver(&mtk_panel_i2c_driver);
#else
	return 0;
#endif
}
module_init(mtk_drm_gateic_init);
module_exit(mtk_drm_gateic_exit);

MODULE_AUTHOR("Cui Zhang <cui.zhang@mediatek.com>");
MODULE_DESCRIPTION("mediatek, drm panel gateic driver");
MODULE_LICENSE("GPL v2");
