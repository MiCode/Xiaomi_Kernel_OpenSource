/*
 * Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 * Copyright (c) 2013, LGE Inc. All rights reserved
 * Copyright (c) 2014 savoca <adeddo27@gmail.com>
 * Copyright (c) 2014 Paul Reioux <reioux@gmail.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/module.h>

#include "mdss_mdp.h"
#include "mdss_mdp_kcal_ctrl.h"

static void kcal_apply_values(struct kcal_lut_data *lut_data)
{
	/* gc_lut_* will save lut values even when disabled and
	 * properly restore them on enable.
	 */
	lut_data->red = (lut_data->red < lut_data->minimum) ?
		lut_data->minimum : lut_data->red;
	lut_data->green = (lut_data->green < lut_data->minimum) ?
		lut_data->minimum : lut_data->green;
	lut_data->blue = (lut_data->blue < lut_data->minimum) ?
		lut_data->minimum : lut_data->blue;

	mdss_mdp_pp_kcal_update(lut_data);
}

static ssize_t kcal_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	int kcal_r, kcal_g, kcal_b;
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	if (count > 12)
		return -EINVAL;

	sscanf(buf, "%d %d %d", &kcal_r, &kcal_g, &kcal_b);

	if (kcal_r < 0 || kcal_r > 256)
		return -EINVAL;

	if (kcal_g < 0 || kcal_g > 256)
		return -EINVAL;

	if (kcal_b < 0 || kcal_b > 256)
		return -EINVAL;

	lut_data->red = kcal_r;
	lut_data->green = kcal_g;
	lut_data->blue = kcal_b;

	kcal_apply_values(lut_data);

	return count;
}

static ssize_t kcal_show(struct device *dev, struct device_attribute *attr,
								char *buf)
{
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	return sprintf(buf, "%d %d %d\n", lut_data->red, lut_data->green,
		lut_data->blue);
}

static ssize_t kcal_min_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int kcal_min;
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	if (count > 4)
		return -EINVAL;

	sscanf(buf, "%d", &kcal_min);

	if (kcal_min < 0 || kcal_min > 256)
		return -EINVAL;

	lut_data->minimum = kcal_min;

	kcal_apply_values(lut_data);

	return count;
}

static ssize_t kcal_min_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", lut_data->minimum);
}

static ssize_t kcal_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int kcal_enable;
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	if (count != 2)
		return -EINVAL;

	sscanf(buf, "%d", &kcal_enable);

	if (kcal_enable != 0 && kcal_enable != 1)
		return -EINVAL;

	if (lut_data->enable == kcal_enable)
		return -EINVAL;

	lut_data->enable = kcal_enable;

	mdss_mdp_pp_kcal_update(lut_data);

	return count;
}

static ssize_t kcal_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", lut_data->enable);
}

static ssize_t kcal_invert_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int kcal_invert;
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	if (count != 2)
		return -EINVAL;

	sscanf(buf, "%d", &kcal_invert);

	if (kcal_invert != 0 && kcal_invert != 1)
		return -EINVAL;

	if (lut_data->invert == kcal_invert)
		return -EINVAL;

	lut_data->invert = kcal_invert;

	mdss_mdp_pp_kcal_invert(lut_data);

	return count;
}

static ssize_t kcal_invert_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", lut_data->invert);
}

static ssize_t kcal_sat_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int kcal_sat;
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	if (count != 4)
		return -EINVAL;

	sscanf(buf, "%d", &kcal_sat);

	if ((kcal_sat < 224 || kcal_sat > 383) && kcal_sat != 128)
		return -EINVAL;

	lut_data->sat = kcal_sat;

	mdss_mdp_pp_kcal_pa(lut_data);

	return count;
}

static ssize_t kcal_sat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", lut_data->sat);
}

static ssize_t kcal_hue_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int kcal_hue;
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	if (count > 5)
		return -EINVAL;

	sscanf(buf, "%d", &kcal_hue);

	if (kcal_hue < 0 || kcal_hue > 1536)
		return -EINVAL;

	lut_data->hue = kcal_hue;

	mdss_mdp_pp_kcal_pa(lut_data);

	return count;
}

static ssize_t kcal_hue_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", lut_data->hue);
}

static ssize_t kcal_val_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int kcal_val;
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	if (count != 4)
		return -EINVAL;

	sscanf(buf, "%d", &kcal_val);

	if (kcal_val < 128 || kcal_val > 383)
		return -EINVAL;

	lut_data->val = kcal_val;

	mdss_mdp_pp_kcal_pa(lut_data);

	return count;
}

static ssize_t kcal_val_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", lut_data->val);
}

static ssize_t kcal_cont_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int kcal_cont;
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	if (count != 4)
		return -EINVAL;

	sscanf(buf, "%d", &kcal_cont);

	if (kcal_cont < 128 || kcal_cont > 383)
		return -EINVAL;

	lut_data->cont = kcal_cont;

	mdss_mdp_pp_kcal_pa(lut_data);

	return count;
}

static ssize_t kcal_cont_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kcal_lut_data *lut_data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", lut_data->cont);
}

static DEVICE_ATTR(kcal, 0644, kcal_show, kcal_store);
static DEVICE_ATTR(kcal_min, 0644, kcal_min_show, kcal_min_store);
static DEVICE_ATTR(kcal_enable, 0644, kcal_enable_show, kcal_enable_store);
static DEVICE_ATTR(kcal_invert, 0644, kcal_invert_show, kcal_invert_store);
static DEVICE_ATTR(kcal_sat, 0644, kcal_sat_show, kcal_sat_store);
static DEVICE_ATTR(kcal_hue, 0644, kcal_hue_show, kcal_hue_store);
static DEVICE_ATTR(kcal_val, 0644, kcal_val_show, kcal_val_store);
static DEVICE_ATTR(kcal_cont, 0644, kcal_cont_show, kcal_cont_store);

static int kcal_ctrl_probe(struct platform_device *pdev)
{
	int ret;
	struct kcal_lut_data *lut_data;

	lut_data = kzalloc(sizeof(*lut_data), GFP_KERNEL);
	if (!lut_data) {
		pr_err("%s: failed to allocate memory for lut_data\n",
			__func__);
		return -ENOMEM;
	}

	lut_data->red = lut_data->green = lut_data->blue = NUM_QLUT;
	lut_data->minimum = 35;
	lut_data->enable = 1;
	lut_data->invert = 0;
	lut_data->sat = DEF_PA;
	lut_data->hue = 0;
	lut_data->val = DEF_PA;
	lut_data->cont = DEF_PA;

	platform_set_drvdata(pdev, lut_data);

	ret = device_create_file(&pdev->dev, &dev_attr_kcal);
	ret |= device_create_file(&pdev->dev, &dev_attr_kcal_min);
	ret |= device_create_file(&pdev->dev, &dev_attr_kcal_enable);
	ret |= device_create_file(&pdev->dev, &dev_attr_kcal_invert);
	ret |= device_create_file(&pdev->dev, &dev_attr_kcal_sat);
	ret |= device_create_file(&pdev->dev, &dev_attr_kcal_hue);
	ret |= device_create_file(&pdev->dev, &dev_attr_kcal_val);
	ret |= device_create_file(&pdev->dev, &dev_attr_kcal_cont);
	if (ret)
		pr_err("%s: unable to create sysfs entries\n", __func__);

	return ret;
}

static int kcal_ctrl_remove(struct platform_device *pdev)
{
	struct kcal_lut_data *lut_data = platform_get_drvdata(pdev);

	device_remove_file(&pdev->dev, &dev_attr_kcal);
	device_remove_file(&pdev->dev, &dev_attr_kcal_min);
	device_remove_file(&pdev->dev, &dev_attr_kcal_enable);
	device_remove_file(&pdev->dev, &dev_attr_kcal_invert);
	device_remove_file(&pdev->dev, &dev_attr_kcal_sat);
	device_remove_file(&pdev->dev, &dev_attr_kcal_hue);
	device_remove_file(&pdev->dev, &dev_attr_kcal_val);
	device_remove_file(&pdev->dev, &dev_attr_kcal_cont);

	kfree(lut_data);

	return 0;
}

static struct platform_driver kcal_ctrl_driver = {
	.probe = kcal_ctrl_probe,
	.remove = kcal_ctrl_remove,
	.driver = {
		.name = "kcal_ctrl",
	},
};

static struct platform_device kcal_ctrl_device = {
	.name = "kcal_ctrl",
};

static int __init kcal_ctrl_init(void)
{
	if (platform_driver_register(&kcal_ctrl_driver))
		return -ENODEV;

	if (platform_device_register(&kcal_ctrl_device))
		return -ENODEV;

	pr_info("%s: registered\n", __func__);

	return 0;
}

static void __exit kcal_ctrl_exit(void)
{
	platform_device_unregister(&kcal_ctrl_device);
	platform_driver_unregister(&kcal_ctrl_driver);
}

late_initcall(kcal_ctrl_init);
module_exit(kcal_ctrl_exit);

MODULE_DESCRIPTION("LCD KCAL Driver");
