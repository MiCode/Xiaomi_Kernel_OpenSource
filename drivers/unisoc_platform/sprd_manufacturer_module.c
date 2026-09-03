// SPDX-License-Identifier: GPL-2.0-only
/*
 * sprd_manufacturer_module.c - Unisoc platform driver
 *
 * Copyright 2022 Unisoc(Shanghai) Technologies Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include<linux/module.h>
#include<linux/init.h>
#include<linux/device.h>
#include<linux/of_device.h>
#include<linux/of.h>

MODULE_AUTHOR("Shian.Wang");
MODULE_LICENSE("GPL");

static const char *sprd_soc_model;
static const char *sprd_soc_manufacturer;

static int init_manufacturer_model(void)
{
	struct device_node *root;
	int ret;

	root = of_find_node_by_path("/");
	if (root) {
		ret = of_property_read_string(root, "soc-module",
						&sprd_soc_model);
		if (ret)
			sprd_soc_model = NULL;
		ret = of_property_read_string(root, "soc-manufacturer",
						&sprd_soc_manufacturer);
		if (ret)
			sprd_soc_manufacturer = NULL;
	}
	return 0;
}

static ssize_t soc_model_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	return (ssize_t) sprintf(buf, "soc_model: %s \n", sprd_soc_model);
}

static ssize_t soc_manufacturer_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	return (ssize_t) sprintf(buf, "soc_model: %s \n", sprd_soc_manufacturer);
}


static struct kobj_attribute kobj_soc_model = {
	.attr = {.name = __stringify(soc_model),
		.mode = VERIFY_OCTAL_PERMISSIONS(S_IRUGO) },
	.show = soc_model_show,
};

static struct kobj_attribute kobj_soc_manufacturer = {
	.attr = {.name = __stringify(soc_manufacturer),
		.mode = VERIFY_OCTAL_PERMISSIONS(S_IRUGO) },
	.show = soc_manufacturer_show,
};

static struct attribute *default_attrs[] = {
	&kobj_soc_model.attr,
	&kobj_soc_manufacturer.attr,
	NULL
};

static const struct attribute_group soc_attr_group = {
	.attrs = default_attrs,
	.name = "soc0"
};

static int sprd_manufacturer_model(void)
{
	static struct device sprd_manufacturer_model_dev;
	static struct device *pdev = (struct device *) &sprd_manufacturer_model_dev;

	device_initialize(pdev);

	if(!sysfs_create_group(&pdev->kobj.kset->kobj, &soc_attr_group))
		return 0;
	return -1;
}

static int __init manufacturer_model_init(void)
{
	int err;

	init_manufacturer_model();
	err = sprd_manufacturer_model();
	if (!err)
		printk(KERN_ALERT"Init manufacturer_model mudule successfully\n");
	else
		printk(KERN_ALERT"Init manufacturer_model mudule failed\n");

	return 0;
}

static void __exit manufacturer_model_exit(void)
{
	printk(KERN_ALERT"Exit manufacturer_model mudule...\n");
}


module_init(manufacturer_model_init);
module_exit(manufacturer_model_exit);

