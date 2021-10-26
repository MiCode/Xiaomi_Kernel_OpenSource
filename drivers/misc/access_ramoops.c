/*
 * Persistent memory accessor
 *
 * Copyright (C) 2019 Google, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/of_address.h>

#define DEVICE_NAME "access_ramoops"
#define MAX_OPEN 4

struct access_ramoops_info {
	char *name;
	phys_addr_t phys;
	void *addr;
	size_t size;
	struct mutex lock;
	int is_mapped;
	int is_open;

	const char *label;
	struct miscdevice miscdev;
};

static ssize_t label_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct access_ramoops_info *info =
		container_of(miscdev, struct access_ramoops_info, miscdev);

	return snprintf(buf, PAGE_SIZE, "%s\n", info->label);
}

static ssize_t size_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct access_ramoops_info *info =
		container_of(miscdev, struct access_ramoops_info, miscdev);

	return snprintf(buf, PAGE_SIZE, "%zd\n", info->size);
}

static DEVICE_ATTR_RO(label);
static DEVICE_ATTR_RO(size);

static struct attribute *access_ramoops_attrs[] = {
	&dev_attr_label.attr,
	&dev_attr_size.attr,
	NULL,
};
ATTRIBUTE_GROUPS(access_ramoops);

static ssize_t access_ramoops_read(struct file *filp, char __user *buf,
				   size_t count, loff_t *ppos)
{
	struct access_ramoops_info *info = filp->private_data;

	return simple_read_from_buffer(buf, count, ppos,
				       info->addr, info->size);
}

static ssize_t access_ramoops_write(struct file *filp, const char __user *buf,
				    size_t count, loff_t *ppos)
{
	struct access_ramoops_info *info = filp->private_data;

	return simple_write_to_buffer(info->addr, info->size,
				      ppos, buf, count);
}

static int access_ramoops_open(struct inode *inode, struct file *filp)
{
	int res = 0;
	struct access_ramoops_info *info =
		container_of(filp->private_data, struct access_ramoops_info,
			     miscdev);

	filp->private_data = info;
	mutex_lock(&info->lock);

	if (info->is_open == MAX_OPEN) {
		res = -EBUSY;
		goto out;
	}

	if (!info->is_open) {
		if (!request_mem_region(info->phys, info->size,
					"access_ramoops")) {
			dev_err(info->miscdev.this_device,
				"request mem region (%zx@%pa) failed\n",
				info->size, &info->phys);
			res = -ENOMEM;
			goto out;
		}
		info->addr = ioremap(info->phys, info->size);
		if (IS_ERR(info->addr)) {
			dev_err(info->miscdev.this_device,
				"unable to map region (%zx@%pa)\n",
				info->size, &info->phys);
			info->addr = NULL;
			res = -ENOMEM;
			goto out;
		}
		info->is_mapped = 1;
	}
	info->is_open++;
out:
	mutex_unlock(&info->lock);
	return res;
}

static int access_ramoops_release(struct inode *inode, struct file *filp)
{
	struct access_ramoops_info *info = filp->private_data;

	mutex_lock(&info->lock);
	info->is_open--;
	if (!info->is_open) {
		info->is_mapped = 0;
		iounmap(info->addr);
		info->addr = NULL;
		release_mem_region(info->phys, info->size);
	}
	filp->private_data = NULL;
	mutex_unlock(&info->lock);
	return 0;
}

static const struct file_operations access_ramoops_fops = {
	.owner		= THIS_MODULE,
	.open		= access_ramoops_open,
	.read		= access_ramoops_read,
	.write		= access_ramoops_write,
	.release	= access_ramoops_release,
};

static int __init access_ramoops_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct access_ramoops_info *info;
	struct device_node *of_node = pdev->dev.of_node;
	struct device_node *mem_region;
	struct resource res;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	platform_set_drvdata(pdev, info);

	mem_region = of_parse_phandle(of_node, "memory-region", 0);
	if (!mem_region) {
		dev_err(&pdev->dev, "no memory-region phandle\n");
		return -ENODEV;
	}

	ret = of_property_read_string(of_node, "label", &info->label);
	if (ret) {
		dev_err(&pdev->dev, "failed to get region label: %d\n", ret);
		return -EINVAL;
	}
	info->name = devm_kasprintf(&pdev->dev, GFP_KERNEL, "access-%s",
				    info->label);
	if (!info->name) {
		dev_err(&pdev->dev, "failed to alloc name\n");
		return -ENOMEM;
	}

	ret = of_address_to_resource(mem_region, 0, &res);
	of_node_put(mem_region);
	if (ret) {
		dev_err(&pdev->dev,
			"failed to get memory-region resource: %d\n", ret);
		return -ENOMEM;
	}

	info->phys = res.start;
	info->size = resource_size(&res);
	mutex_init(&info->lock);

	info->miscdev.minor	= MISC_DYNAMIC_MINOR;
	info->miscdev.name	= info->name;
	info->miscdev.fops	= &access_ramoops_fops;
	info->miscdev.groups	= access_ramoops_groups;
	info->miscdev.parent	= &pdev->dev;
	ret = misc_register(&info->miscdev);
	if (ret) {
		dev_err(info->miscdev.this_device,
			"failed to register misc device %d\n", ret);
		return ret;
	}

	dev_info(info->miscdev.this_device,
		 "registered '%s' %d:%d, (%zx@%pa)\n", info->label,
		 MISC_MAJOR, info->miscdev.minor, info->size, &info->phys);

	return ret;
}

static int __exit access_ramoops_remove(struct platform_device *pdev)
{
	struct access_ramoops_info *info = platform_get_drvdata(pdev);

	dev_info(info->miscdev.this_device, "removing");
	misc_deregister(&info->miscdev);

	if (!info->is_mapped)
		return 0;

	mutex_lock(&info->lock);
	info->is_mapped = 0;
	iounmap(info->addr);
	release_mem_region(info->phys, info->size);
	mutex_unlock(&info->lock);
	return 0;
}

static const struct of_device_id dt_match[] = {
	{ .compatible = "access_ramoops" },
	{}
};

static struct platform_driver access_ramoops_driver = {
	.driver		= {
		.name		= "access_ramoops",
		.of_match_table	= dt_match,
	},
	.remove		= __exit_p(access_ramoops_remove),
};

module_platform_driver_probe(access_ramoops_driver, access_ramoops_probe);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Patrick Tjin <pattjin@google.com>");
MODULE_DESCRIPTION("Persistent memory access driver");

