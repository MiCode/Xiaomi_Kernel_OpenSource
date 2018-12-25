/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/regulator/mediatek/mtk_regulator.h>
#include <linux/regulator/mediatek/mtk_regulator_core.h>

/* #ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
#if 0
int mtk_regulator_get(struct device *dev, const char *id,
	struct mtk_regulator *mreg)
{
	return 0;
}

int mtk_regulator_get_exclusive(struct device *dev, const char *id,
	struct mtk_regulator *mreg)
{
	return 0;
}

int devm_mtk_regulator_get(struct device *dev, const char *id,
	struct mtk_regulator *mreg)
{
	return 0;
}

void mtk_regulator_put(struct mtk_regulator *mreg)
{
}

void devm_mtk_regulator_put(struct mtk_regulator *mreg)
{
}

int mtk_regulator_enable(struct mtk_regulator *mreg, bool enable)
{
	return 0;
}

int mtk_regulator_force_disable(struct mtk_regulator *mreg)
{
	return 0;
}

int mtk_regulator_is_enabled(struct mtk_regulator *mreg)
{
	return 0;
}

int mtk_regulator_set_mode(struct mtk_regulator *mreg,
	unsigned int mode)
{
	return 0;
}

unsigned int mtk_regulator_get_mode(struct mtk_regulator *mreg)
{
	return 0;
}

int mtk_regulator_set_voltage(struct mtk_regulator *mreg, int min_uV,
	int max_uV)
{
	return 0;
}

int mtk_regulator_get_voltage(struct mtk_regulator *mreg)
{
	return 0;
}

int mtk_regulator_set_current_limit(struct mtk_regulator *mreg,
					     int min_uA, int max_uA)
{
	return 0;
}

int mtk_regulator_get_current_limit(struct mtk_regulator *mreg)
{
	return 0;
}

#else /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */

int mtk_regulator_get(struct device *dev, const char *id,
	struct mtk_regulator *mreg)
{
	mreg->consumer = regulator_get(dev, id);
	if (IS_ERR(mreg->consumer))
		return PTR_ERR(mreg->consumer);

	mreg->mreg_dev = mtk_simple_regulator_get_dev_by_name(id);
	if (!mreg->mreg_dev) {
		pr_info("%s: no mreg device\n", __func__);
		return -ENODEV;
	}

	return 0;
}

int mtk_regulator_get_exclusive(struct device *dev, const char *id,
	struct mtk_regulator *mreg)
{
	mreg->consumer = regulator_get_exclusive(dev, id);
	if (IS_ERR(mreg->consumer))
		return PTR_ERR(mreg->consumer);

	mreg->mreg_dev = mtk_simple_regulator_get_dev_by_name(id);
	if (!mreg->mreg_dev) {
		pr_info("%s: no mreg device\n", __func__);
		return -ENODEV;
	}

	return 0;
}

int devm_mtk_regulator_get(struct device *dev, const char *id,
	struct mtk_regulator *mreg)
{
	struct mtk_simple_regulator_device *mreg_dev = NULL;

	mreg->consumer = devm_regulator_get(dev, id);
	if (IS_ERR(mreg->consumer))
		return PTR_ERR(mreg->consumer);

	mreg_dev = mtk_simple_regulator_get_dev_by_name(id);
	if (!mreg_dev) {
		pr_info("%s: no mreg device\n", __func__);
		return -ENODEV;
	}

	return 0;
}

void mtk_regulator_put(struct mtk_regulator *mreg)
{
	regulator_put(mreg->consumer);
}

void devm_mtk_regulator_put(struct mtk_regulator *mreg)
{
	devm_regulator_put(mreg->consumer);
}

int mtk_regulator_enable(struct mtk_regulator *mreg, bool enable)
{
	return (enable ? regulator_enable :  regulator_disable)(mreg->consumer);
}

int mtk_regulator_force_disable(struct mtk_regulator *mreg)
{
	return regulator_force_disable(mreg->consumer);
}

int mtk_regulator_is_enabled(struct mtk_regulator *mreg)
{
	return regulator_is_enabled(mreg->consumer);
}

int mtk_regulator_set_mode(struct mtk_regulator *mreg,
	unsigned int mode)
{
	return regulator_set_mode(mreg->consumer, mode);
}

unsigned int mtk_regulator_get_mode(struct mtk_regulator *mreg)
{
	return regulator_get_mode(mreg->consumer);
}

int mtk_regulator_set_voltage(struct mtk_regulator *mreg, int min_uV,
	int max_uV)
{
	return regulator_set_voltage(mreg->consumer, min_uV, max_uV);
}

int mtk_regulator_get_voltage(struct mtk_regulator *mreg)
{
	return regulator_get_voltage(mreg->consumer);
}

int mtk_regulator_set_current_limit(struct mtk_regulator *mreg,
					     int min_uA, int max_uA)
{
	return regulator_set_current_limit(mreg->consumer,
		min_uA, max_uA);
}

int mtk_regulator_get_current_limit(struct mtk_regulator *mreg)
{
	return regulator_get_current_limit(mreg->consumer);
}

int mtk_regulator_set_property(struct mtk_regulator *mreg,
	enum mtk_simple_regulator_property prop,
	union mtk_simple_regulator_propval *val)
{
	int ret = 0;
	struct mtk_simple_regulator_desc *mreg_desc = NULL;

	if (!mreg || !mreg->mreg_dev)
		return -EINVAL;

	mreg_desc = (struct mtk_simple_regulator_desc *)
		dev_get_drvdata(&mreg->mreg_dev->dev);

	if (!mreg_desc) {
		pr_info("%s: no mreg desc\n", __func__);
		return -EINVAL;
	}

	if (!mreg_desc->mreg_adv_ops ||
	    !mreg_desc->mreg_adv_ops->set_property) {
		pr_info("%s: no adv ops\n", __func__);
		return -EINVAL;
	}

	ret = mreg_desc->mreg_adv_ops->set_property(mreg_desc, prop, val);
	return ret;
}

int mtk_regulator_get_property(struct mtk_regulator *mreg,
	enum mtk_simple_regulator_property prop,
	union mtk_simple_regulator_propval *val)
{
	int ret = 0;
	struct mtk_simple_regulator_desc *mreg_desc = NULL;

	if (!mreg || !mreg->mreg_dev)
		return -EINVAL;

	mreg_desc = (struct mtk_simple_regulator_desc *)
		dev_get_drvdata(&mreg->mreg_dev->dev);

	if (!mreg_desc) {
		pr_info("%s: no mreg desc\n", __func__);
		return -EINVAL;
	}

	if (!mreg_desc->mreg_adv_ops ||
	    !mreg_desc->mreg_adv_ops->get_property) {
		pr_info("%s: no adv ops\n", __func__);
		return -EINVAL;
	}

	ret = mreg_desc->mreg_adv_ops->get_property(mreg_desc, prop, val);
	return ret;
}
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
