/* Copyright (C) 2014 - 2015 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/nvmem-consumer.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sysfs.h>

#define UID_NAME	"sprd_uid"

struct sprd_uid {
	struct miscdevice misc;
	int start;
	int end;
};

static int sprd_uid_cal_read(struct device_node *np, const char *cell_id,
			     u32 *val)
{
	struct nvmem_cell *cell;
	void *buf;
	size_t len;

	cell = of_nvmem_cell_get(np, cell_id);
	if (IS_ERR(cell))
		return PTR_ERR(cell);

	buf = nvmem_cell_read(cell, &len);
	if (IS_ERR(buf)) {
		nvmem_cell_put(cell);
		return PTR_ERR(buf);
	}

	memcpy(val, buf, min(len, sizeof(u32)));

	kfree(buf);
	nvmem_cell_put(cell);
	return 0;
}

static ssize_t uid_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct sprd_uid *uid = dev_get_drvdata(dev);
	char *p = buf;

	p += sprintf(p, "%08x%08x", uid->start, uid->end);
	p += sprintf(p, "\n");

	return p - buf;
}
static DEVICE_ATTR_RO(uid);

static struct attribute *uid_device[] = {
	&dev_attr_uid.attr,
	NULL,
};

static struct attribute_group uid_attribute_group = {
	.attrs = uid_device,
};

static int sprd_uid_probe(struct platform_device *pdev)
{
	struct sprd_uid *uid = NULL;
	struct device_node *np = pdev->dev.of_node;
	int ret;

	uid = devm_kzalloc(&pdev->dev, sizeof(*uid), GFP_KERNEL);
	if (!uid)
		return -ENOMEM;

	ret = sprd_uid_cal_read(np, "uid_start", &uid->start);
	if (ret)
		return ret;

	ret = sprd_uid_cal_read(np, "uid_end", &uid->end);
	if (ret)
		return ret;

	uid->misc.name = UID_NAME;
	uid->misc.parent = &pdev->dev;
	uid->misc.minor = MISC_DYNAMIC_MINOR;
	ret = misc_register(&uid->misc);
	if (ret) {
		dev_err(&pdev->dev, "unable to register misc dev\n");
		return ret;
	}

	ret = sysfs_create_group(&uid->misc.this_device->kobj, &uid_attribute_group);
	if (ret) {
		dev_err(&pdev->dev, "unable to create group\n");
		misc_deregister(&uid->misc);
		return ret;
	}

	platform_set_drvdata(pdev, uid);

	return 0;
}

static int sprd_uid_remove(struct platform_device *pdev)
{
	struct sprd_uid *uid = dev_get_drvdata(&pdev->dev);

	sysfs_remove_group(pdev->dev.kobj.parent, &uid_attribute_group);
	misc_deregister(&uid->misc);

	return 0;
}

static const struct of_device_id sprd_uid_of_match[] = {
	{.compatible = "sprd,uid"},
	{},
};

static struct platform_driver sprd_uid_driver = {
	.probe = sprd_uid_probe,
	.remove = sprd_uid_remove,
	.driver = {
		.name = "sprd-uid",
		.of_match_table = sprd_uid_of_match,
	},
};

module_platform_driver(sprd_uid_driver);

MODULE_AUTHOR("Freeman Liu <freeman.liu@spreadtrum.com>");
MODULE_DESCRIPTION("Spreadtrum uid driver");
MODULE_LICENSE("GPL v2");

