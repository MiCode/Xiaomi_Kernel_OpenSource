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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/regulator/mediatek/mtk_regulator_class.h>

struct class *mreg_class;

static void mtk_simple_regulator_device_release(struct device *dev)
{
	struct mtk_simple_regulator_device *mreg_dev = to_mreg_device(dev);

	kfree(mreg_dev);
}

struct mtk_simple_regulator_device *mtk_simple_regulator_device_register(
	const char *name, struct device *parent, void *drvdata)
{
	int ret = 0;
	struct mtk_simple_regulator_device *mreg_dev = NULL;

	pr_info("%s: name = %s\n", __func__, name);
	mreg_dev = kzalloc(sizeof(struct mtk_simple_regulator_device),
		GFP_KERNEL);
	if (!mreg_dev)
		return ERR_PTR(-ENOMEM);

	mreg_dev->dev.parent = parent;
	mreg_dev->dev.class = mreg_class;
	mreg_dev->dev.release = mtk_simple_regulator_device_release;
	dev_set_name(&mreg_dev->dev, name);
	dev_set_drvdata(&mreg_dev->dev, drvdata);
	ret = device_register(&mreg_dev->dev);
	if (ret) {
		kfree(mreg_dev);
		return ERR_PTR(ret);
	}

	return mreg_dev;
}
EXPORT_SYMBOL(mtk_simple_regulator_device_register);

void mtk_simple_regulator_device_unregister(
	struct mtk_simple_regulator_device *mreg_dev)
{
	if (!mreg_dev)
		return;

	device_unregister(&mreg_dev->dev);
}
EXPORT_SYMBOL(mtk_simple_regulator_device_unregister);

static int mtk_simple_regulator_match_device_by_name(struct device *dev,
	const void *data)
{
	const char *name = data;

	return strcmp(dev_name(dev), name) == 0;
}

struct mtk_simple_regulator_device *mtk_simple_regulator_get_dev_by_name(
	const char *name)
{
	struct device *dev = NULL;

	if (!name)
		return NULL;

	dev = class_find_device(mreg_class, NULL, name,
		mtk_simple_regulator_match_device_by_name);
	return dev ? to_mreg_device(dev) : NULL;
}

static int __init mtk_simple_regulator_class_init(void)
{
	mreg_class = class_create(THIS_MODULE, "mtk_simple_regulator");
	if (IS_ERR(mreg_class)) {
		pr_info("%s: Unable to create mreg class, errno = %ld\n",
			__func__, PTR_ERR(mreg_class));
		return PTR_ERR(mreg_class);
	}

	return 0;
}

static void __exit mtk_simple_regulator_class_exit(void)
{
	class_destroy(mreg_class);
}

subsys_initcall(mtk_simple_regulator_class_init);
module_exit(mtk_simple_regulator_class_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ShuFan Lee <shufan_lee@richtek.com>");
MODULE_VERSION("1.0.1_MTK");
MODULE_DESCRIPTION("MTK Regulator Framework Class");
