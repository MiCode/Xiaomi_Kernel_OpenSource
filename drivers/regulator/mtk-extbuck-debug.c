// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 MediaTek Inc.

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

struct extbuck_consumer_data {
	struct mutex lock;
	struct regmap *regmap;
	unsigned int reg_value;
};

static ssize_t extbuck_access_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct extbuck_consumer_data *data = dev_get_drvdata(dev);

	dev_info(dev, "[%s] 0x%x\n", __func__, data->reg_value);
	return sprintf(buf, "0x%x\n", data->reg_value);
}

static ssize_t extbuck_access_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf,
				    size_t size)
{
	struct extbuck_consumer_data *data;
	int ret = 0;
	char *pvalue = NULL, *addr, *val;
	unsigned int reg_val = 0;
	unsigned int reg_adr = 0;

	if (dev) {
		data = dev_get_drvdata(dev);
		if (!data)
			return -ENODEV;
	} else
		return -ENODEV;

	if (buf != NULL && size != 0) {
		dev_info(dev, "[%s] size is %d, buf is %s\n", __func__,
			(int)size, buf);

		pvalue = (char *)buf;
		addr = strsep(&pvalue, " ");
		val = strsep(&pvalue, " ");
		if (addr)
			ret = kstrtou32(addr, 16, (unsigned int *)&reg_adr);
		mutex_lock(&data->lock);
		if (val) {
			ret = kstrtou32(val, 16, (unsigned int *)&reg_val);
			ret = regmap_write(data->regmap, reg_adr, reg_val);
		} else {
			ret = regmap_read(data->regmap,
					  reg_adr, &data->reg_value);
		}
		mutex_unlock(&data->lock);
		dev_info(dev, "%s Ext. BUCK Reg[0x%x]=0x%x!\n",
			(val ? "write" : "read"), reg_adr,
			(val ? reg_val : data->reg_value));
	}
	return size;
}
static DEVICE_ATTR_RW(extbuck_access);

static int extbuck_debug_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct extbuck_consumer_data *drvdata;
	int ret = 0;

	drvdata = devm_kzalloc(dev, sizeof(struct extbuck_consumer_data),
			       GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->regmap = dev_get_regmap(dev->parent, NULL);
	if (!drvdata->regmap)
		return -ENODEV;
	mutex_init(&drvdata->lock);
	dev_set_drvdata(dev, drvdata);

	/* Create sysfs entry */
	ret = device_create_file(dev, &dev_attr_extbuck_access);
	if (ret < 0)
		dev_notice(dev, "%s failed to create sysfs file\n", __func__);

	return ret;
}

static int extbuck_debug_remove(struct platform_device *pdev)
{
	device_remove_file(&pdev->dev, &dev_attr_extbuck_access);

	return 0;
}

static const struct of_device_id extbuck_debug_of_match[] = {
	{
		.compatible = "mediatek,extbuck-debug",
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, extbuck_debug_of_match);

static struct platform_driver extbuck_debug_driver = {
	.driver = {
		.name = "extbuck_debug",
		.of_match_table = extbuck_debug_of_match,
	},
	.probe = extbuck_debug_probe,
	.remove = extbuck_debug_remove,
};

module_platform_driver(extbuck_debug_driver);

MODULE_AUTHOR("Jeter Chen <jeter.chen@mediatek.com>");
MODULE_DESCRIPTION("Debug driver for MediaTek Ext. BUCK PMIC");
MODULE_LICENSE("GPL");
