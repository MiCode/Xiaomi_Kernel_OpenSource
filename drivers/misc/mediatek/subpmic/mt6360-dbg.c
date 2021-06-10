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

#include <linux/mfd/mt6360.h>
#include <linux/extdev_io_class.h>
#include "generic_debugfs.h"

#define MT6360_DBG_TYPESTR	"I2C,MT6360"

struct mt6360_dbg_info {
	struct device *dev;
	struct extdev_desc extdev_desc[MT6360_SLAVE_MAX];
	struct extdev_io_device *extdev[MT6360_SLAVE_MAX];
	struct dbg_info dbg_info[MT6360_SLAVE_MAX];
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

static int mt6360_dbg_slave_register(struct device *parent,
				     struct i2c_client *i2c,
				     struct dbg_info *di)
{
	struct device *dev = &i2c->dev;
	struct regmap *regmap = dev_get_regmap(dev, NULL);

	if (IS_ERR_OR_NULL(regmap))
		return -EINVAL;
	di->dirname = devm_kasprintf(parent, GFP_KERNEL,
				     "MT6360.%s", dev_name(dev));
	di->devname = dev_name(dev);
	di->typestr = MT6360_DBG_TYPESTR;
	di->io_drvdata = regmap;
	di->io_read = mt6360_dbg_io_read;
	di->io_write = mt6360_dbg_io_write;
	return generic_debugfs_init(di);
}

static int mt6360_dbg_probe(struct platform_device *pdev)
{
	struct mt6360_pmu_data *mpd = dev_get_drvdata(pdev->dev.parent);
	struct mt6360_dbg_info *mdi;
	int i, ret;

	dev_info(&pdev->dev, "%s\n", __func__);
	mdi = devm_kzalloc(&pdev->dev, sizeof(*mdi), GFP_KERNEL);
	if (!mdi)
		return -ENOMEM;
	mdi->dev = &pdev->dev;
	platform_set_drvdata(pdev, mdi);

	for (i = 0; i < MT6360_SLAVE_MAX; i++) {
		/* sysfs interface */
		mdi->extdev_desc[i].dirname = devm_kasprintf(&mpd->i2c[i]->dev,
						       GFP_KERNEL, "MT6360.%s",
						       dev_name(&mpd->i2c[i]->dev));
		mdi->extdev_desc[i].devname = dev_name(&mpd->i2c[i]->dev);
		mdi->extdev_desc[i].typestr = devm_kasprintf(&pdev->dev,
						       GFP_KERNEL, "I2C,MT6360");
		mdi->extdev_desc[i].rmap = dev_get_regmap(&mpd->i2c[i]->dev, NULL);
		if (mdi->extdev_desc[i].rmap == NULL)
			dev_info(&mpd->i2c[i]->dev, "%s: regmap is null\n", __func__);
		mdi->extdev_desc[i].io_read = mt6360_dbg_io_read;
		mdi->extdev_desc[i].io_write = mt6360_dbg_io_write;
		mdi->extdev[i] = devm_extdev_io_device_register(pdev->dev.parent, &mdi->extdev_desc[i]);
		if (IS_ERR(mdi->extdev[i])) {
			dev_err(&pdev->dev, "failed to register extdev_io device\n", __func__);
			return PTR_ERR(mdi->extdev[i]);
		}

		/* debugfs interface */
		ret = mt6360_dbg_slave_register(&pdev->dev,
						mpd->i2c[i], &mdi->dbg_info[i]);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to register [%d] dbg\n", i);
			return ret;
		}
	}
	dev_info(&pdev->dev, "%s successfully\n", __func__);
	return 0;
}

static int mt6360_dbg_remove(struct platform_device *pdev)
{
	struct mt6360_dbg_info *mdi = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < MT6360_SLAVE_MAX; i++)
		generic_debugfs_exit(&mdi->dbg_info[i]);
	return 0;
}

static const struct platform_device_id mt6360_dbg_id[] = {
	{ "mt6360_dbg", 0 },
	{},
};
MODULE_DEVICE_TABLE(platform, mt6360_dbg_id);

static struct platform_driver mt6360_dbg_driver = {
	.driver = {
		.name = "mt6360_dbg",
	},
	.probe = mt6360_dbg_probe,
	.remove = mt6360_dbg_remove,
	.id_table = mt6360_dbg_id,
};
module_platform_driver(mt6360_dbg_driver);

MODULE_AUTHOR("CY Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MT6360 Dbg Driver");
MODULE_LICENSE("GPL");
