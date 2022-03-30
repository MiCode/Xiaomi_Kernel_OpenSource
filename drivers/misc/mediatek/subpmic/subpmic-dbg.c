// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include "generic_debugfs.h"
#include <linux/extdev_io_class.h>

#define MT6360_DATA 1
#define MT6375_DATA 2
struct subpmic_dbg_info {
	struct device *dev;
	struct extdev_desc extdev_desc;
	struct extdev_io_device *extdev;
	struct dbg_info dbg_info;
};

struct subpmic_data {
	const char *bus;
	const char *name;
};

static int subpmic_dbg_io_read(void *drvdata, u16 reg, void *val, u16 size)
{
	return regmap_bulk_read((struct regmap *)drvdata, reg, val, size);
}

static int subpmic_dbg_io_write(void *drvdata,
			       u16 reg, const void *val, u16 size)
{
	return regmap_bulk_write((struct regmap *)drvdata, reg, val, size);
}

static int subpmic_dbg_probe(struct platform_device *pdev)
{
	struct subpmic_dbg_info *mdi;
	struct regmap *regmap;
	const struct subpmic_data *sdata;
	const struct platform_device_id *id;
	char *dirname, *typestr;
	int ret;

	mdi = devm_kzalloc(&pdev->dev, sizeof(*mdi), GFP_KERNEL);
	if (!mdi)
		return -ENOMEM;

	mdi->dev = &pdev->dev;
	platform_set_drvdata(pdev, mdi);

	regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!regmap) {
		dev_err(&pdev->dev, "failed to allocate regmap\n");
		return -ENODEV;
	}

	if (pdev->dev.of_node)
		sdata = of_device_get_match_data(&pdev->dev);
	else {
		id = platform_get_device_id(pdev);
		sdata = (struct subpmic_data *)id->driver_data;
	}
	if (!sdata) {
		dev_err(&pdev->dev, "failed to get subpimic data\n");
		return -ENODEV;
	}

	dirname = devm_kasprintf(&pdev->dev, GFP_KERNEL, "%s.%s", sdata->name,
				 dev_name(pdev->dev.parent));
	typestr = devm_kasprintf(&pdev->dev, GFP_KERNEL, "%s,%s", sdata->bus,
				 sdata->name);
	if (!dirname || !typestr)
		return -ENOMEM;

	/* sysfs interface */
	mdi->extdev_desc.dirname = dirname;
	mdi->extdev_desc.devname = dev_name(pdev->dev.parent);
	mdi->extdev_desc.typestr = typestr;
	mdi->extdev_desc.rmap = regmap;
	mdi->extdev_desc.io_read = subpmic_dbg_io_read;
	mdi->extdev_desc.io_write = subpmic_dbg_io_write;
	mdi->extdev = devm_extdev_io_device_register(pdev->dev.parent, &mdi->extdev_desc);
	if (IS_ERR(mdi->extdev)) {
		dev_err(&pdev->dev, "failed to register extdev_io device\n", __func__);
		return PTR_ERR(mdi->extdev);
	}

	/* debugfs interface */
	mdi->dbg_info.dirname = dirname;
	mdi->dbg_info.devname = dev_name(pdev->dev.parent);
	mdi->dbg_info.typestr = typestr;
	mdi->dbg_info.io_drvdata = regmap;
	mdi->dbg_info.io_read = subpmic_dbg_io_read;
	mdi->dbg_info.io_write = subpmic_dbg_io_write;
	ret = generic_debugfs_register(&mdi->dbg_info);
	if (ret < 0)
		return ret;

	dev_info(&pdev->dev, "%s: successfully\n", __func__);
	return 0;
}

static int subpmic_dbg_remove(struct platform_device *pdev)
{
	struct subpmic_dbg_info *mdi = platform_get_drvdata(pdev);

	generic_debugfs_unregister(&mdi->dbg_info);
	return 0;
}

static const struct subpmic_data mt6360_data = {
	.bus = "I2C",
	.name = "MT6360",
};

static const struct subpmic_data mt6375_data = {
	.bus = "I2C",
	.name = "MT6375",
};

static const struct of_device_id __maybe_unused subpmic_dbg_of_id[] = {
	{ .compatible = "mediatek,mt6360-dbg", .data = &mt6360_data},
	{ .compatible = "mediatek,mt6375-dbg", .data = &mt6375_data},
	{ }
};
MODULE_DEVICE_TABLE(of, subpmic_dbg_of_id);

static const struct platform_device_id subpmic_dbg_id[] = {
	{ "mt6360_dbg", (kernel_ulong_t)&mt6360_data},
	{ "mt6375_dbg", (kernel_ulong_t)&mt6375_data},
	{ }
};
MODULE_DEVICE_TABLE(platform, subpmic_dbg_id);

static struct platform_driver subpmic_dbg_driver = {
	.driver = {
		.name = "subpmic_dbg",
		.of_match_table = subpmic_dbg_of_id,
	},
	.probe = subpmic_dbg_probe,
	.remove = subpmic_dbg_remove,
	.id_table = subpmic_dbg_id,
};
module_platform_driver(subpmic_dbg_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("alice_chen <alice_chen@richtek.com>");
MODULE_DESCRIPTION("Subpmic Debug driver");

