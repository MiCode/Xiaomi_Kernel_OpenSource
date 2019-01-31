/*
 *  sound/soc/codecs/mt6660-param.c
 *  Driver to Mediatek MT6660 SPKAMP Param
 *
 *  Copyright (C) 2018 Mediatek Inc.
 *  cy_huang <cy_huang@richtek.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <sound/soc.h>

#include "mt6660.h"

struct mt6660_param_drvdata {
	struct device *dev;
	struct mt6660_chip *chip;
	int param;
};

static ssize_t mt6660_param_file_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mt6660_param_drvdata *p_drvdata = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "param = %d\n", p_drvdata->param);
}

static ssize_t mt6660_param_file_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mt6660_param_drvdata *p_drvdata = dev_get_drvdata(dev);
	int parse_val = 0, ret = 0;

	if (kstrtoint(buf, 10, &parse_val) < 0)
		return -EINVAL;
	p_drvdata->param = parse_val;
	ret = mt6660_codec_trigger_param_write(p_drvdata->chip, NULL, 0);
	if (ret < 0)
		return ret;
	return count;
}

static const DEVICE_ATTR(prop_params, 0644, mt6660_param_file_show,
			 mt6660_param_file_store);

static int mt6660_param_probe(struct platform_device *pdev)
{
	struct mt6660_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct mt6660_param_drvdata *p_drvdata = NULL;
	int ret = 0;

	dev_info(&pdev->dev, "%s: ++\n", __func__);
	p_drvdata = devm_kzalloc(&pdev->dev, sizeof(*p_drvdata), GFP_KERNEL);
	if (!p_drvdata)
		return -ENOMEM;
	/* drvdata initialize */
	p_drvdata->dev = &pdev->dev;
	p_drvdata->chip = chip;
	p_drvdata->param = 1;
	platform_set_drvdata(pdev, p_drvdata);
	ret = device_create_file(&pdev->dev, &dev_attr_prop_params);
	if (ret < 0)
		goto cfile_fail;
	dev_info(&pdev->dev, "%s: --\n", __func__);
	return 0;
cfile_fail:
	devm_kfree(&pdev->dev, p_drvdata);
	platform_set_drvdata(pdev, NULL);
	return ret;
}

static int mt6660_param_remove(struct platform_device *pdev)
{
	struct mt6660_param_drvdata *p_drvdata = platform_get_drvdata(pdev);

	dev_dbg(p_drvdata->dev, "%s: ++\n", __func__);
	dev_dbg(p_drvdata->dev, "%s: --\n", __func__);
	return 0;
}

static const struct platform_device_id mt6660_param_pdev_id[] = {
	{ "mt6660-param", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, mt6660_param_pdev_id);

static struct platform_driver mt6660_param_driver = {
	.driver = {
		.name = "mt6660-param",
		.owner = THIS_MODULE,
	},
	.probe = mt6660_param_probe,
	.remove = mt6660_param_remove,
	.id_table = mt6660_param_pdev_id,
};
module_platform_driver(mt6660_param_driver);
