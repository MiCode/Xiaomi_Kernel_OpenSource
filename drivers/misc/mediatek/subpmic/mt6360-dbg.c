// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>

#include <linux/extdev_io_class.h>
#include "generic_debugfs.h"

#define MT6360_DBG_TYPESTR	"I2C,MT6360"

struct mt6360_dbg_info {
	struct device *dev;
	struct extdev_desc extdev_desc;
	struct extdev_io_device *extdev;
	struct dbg_info dbg_info;
};

static int mt6360_dbg_io_read(void *drvdata, u16 reg, void *val, u16 size)
{
	int ret;
	u8 data = 0;

	ret = regmap_bulk_read((struct regmap *)drvdata, reg, val, size);
	pr_info("%s: reg:0x%x, data:0x%x(%d)\n", __func__, reg, data, ret);
	return ret;
}

static int mt6360_dbg_io_write(void *drvdata,
			       u16 reg, const void *val, u16 size)
{
	int ret;

	ret = regmap_bulk_write((struct regmap *)drvdata, reg, val, size);
	pr_info("%s: reg:0x%x, val:0x%x(%d)\n", __func__, reg, *(u8 *)val, ret);
	return ret;
}

static int mt6360_dbg_probe(struct platform_device *pdev)
{
	struct mt6360_dbg_info *mdi;
	struct regmap *regmap;
	int ret;

	dev_info(&pdev->dev, "%s\n", __func__);
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

	mdi->extdev_desc.dirname = devm_kasprintf(&pdev->dev,
					       GFP_KERNEL, "MT6360.%s",
					       dev_name(pdev->dev.parent));
	mdi->extdev_desc.devname = dev_name(pdev->dev.parent);
	mdi->extdev_desc.typestr = devm_kasprintf(&pdev->dev,
					       GFP_KERNEL, "I2C,MT6360");
	mdi->extdev_desc.rmap = regmap;
	mdi->extdev_desc.io_read = mt6360_dbg_io_read;
	mdi->extdev_desc.io_write = mt6360_dbg_io_write;
	mdi->extdev = devm_extdev_io_device_register(pdev->dev.parent, &mdi->extdev_desc);
	if (IS_ERR(mdi->extdev)) {
		dev_err(&pdev->dev, "failed to register extdev_io device\n", __func__);
		return PTR_ERR(mdi->extdev);
	}

	/* debugfs interface */
	mdi->dbg_info.dirname = devm_kasprintf(&pdev->dev,
					       GFP_KERNEL,
					       "MT6360.%s",
					       dev_name(pdev->dev.parent));
	mdi->dbg_info.devname = dev_name(pdev->dev.parent);
	mdi->dbg_info.typestr = MT6360_DBG_TYPESTR;
	mdi->dbg_info.io_drvdata = regmap;
	mdi->dbg_info.io_read = mt6360_dbg_io_read;
	mdi->dbg_info.io_write = mt6360_dbg_io_write;
	ret = generic_debugfs_register(&mdi->dbg_info);
	if (ret < 0)
		return ret;
	dev_info(&pdev->dev, "%s successfully\n", __func__);
	return 0;
}

static int mt6360_dbg_remove(struct platform_device *pdev)
{
	struct mt6360_dbg_info *mdi = platform_get_drvdata(pdev);

	generic_debugfs_unregister(&mdi->dbg_info);
	return 0;
}

static const struct platform_device_id mt6360_dbg_id[] = {
	{ "mt6360-dbg", 0 },
	{},
};
MODULE_DEVICE_TABLE(platform, mt6360_dbg_id);

static struct platform_driver mt6360_dbg_driver = {
	.driver = {
		.name = "mt6360-dbg",
	},
	.probe = mt6360_dbg_probe,
	.remove = mt6360_dbg_remove,
	.id_table = mt6360_dbg_id,
};
module_platform_driver(mt6360_dbg_driver);

MODULE_AUTHOR("CY Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MT6360 Dbg Driver");
MODULE_LICENSE("GPL");
