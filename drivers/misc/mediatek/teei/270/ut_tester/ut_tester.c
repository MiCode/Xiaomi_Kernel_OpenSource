/*
 * Copyright (c) 2015-2017 MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <asm/cacheflush.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <fdrv.h>

#define IMSG_TAG "[ut_tester]"
#include <imsg_log.h>

static struct teei_fdrv ut_tester_fdrv = {
	.buff_size = PAGE_SIZE,
	.call_type = 200,
};

static ssize_t run_tests_show(struct device *cd, struct device_attribute *attr,
			 char *buf)
{
	if (!ut_tester_fdrv.buf) {
		IMSG_WARN("ut_tester driver is not init\n");
		return 0;
	}

	/* compatible for old trigger method */
	strcpy(ut_tester_fdrv.buf, "all");
	fdrv_notify(&ut_tester_fdrv);

	return 0;
}
static ssize_t run_tests_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	if (!ut_tester_fdrv.buf) {
		IMSG_WARN("ut_tester driver is not init\n");
		return len;
	}

	memset(ut_tester_fdrv.buf, 0, ut_tester_fdrv.buff_size);

	if (len > 0)
		if (sscanf(buf, "%64s\n", (char *)ut_tester_fdrv.buf) < 0)
			IMSG_WARN("sscanf failed\n");

	IMSG_DEBUG("buf '%s', len %zd, fdrv_buf '%s'\n",
			buf, len, (const char *)ut_tester_fdrv.buf);

	fdrv_notify(&ut_tester_fdrv);

	return len;
}
static DEVICE_ATTR_RW(run_tests);

static struct device_attribute *tester_attrs[] = {
	&dev_attr_run_tests,
	NULL
};

static void remove_sysfs(struct platform_device *pdev)
{
	int i;

	for (i = 0; tester_attrs[i]; i++)
		device_remove_file(&pdev->dev, tester_attrs[i]);
}

static int create_sysfs(struct platform_device *pdev)
{
	int res = 0;
	int i;

	for (i = 0; tester_attrs[i]; i++) {
		res = device_create_file(&pdev->dev, tester_attrs[i]);
		if (res) {
			IMSG_ERROR("failed to create sysfs entry: %s\n",
						tester_attrs[i]->attr.name);
			break;
		}
	}

	if (res)
		remove_sysfs(pdev);

	return res;
}

static int ut_tester_probe(struct platform_device *pdev)
{
	int res;

	res = create_sysfs(pdev);
	if (res)
		return -EFAULT;

	register_fdrv(&ut_tester_fdrv);

	return 0;
}

static int ut_tester_remove(struct platform_device *pdev)
{
	remove_sysfs(pdev);
	return 0;
}

static const struct of_device_id ut_tester_of_match[] = {
	{ .compatible = "microtrust,tester-v1", },
	{},
};

static struct platform_driver ut_tester_driver = {
	.probe = ut_tester_probe,
	.remove = ut_tester_remove,
	.driver = {
		.name = "ut-tester",
		.owner = THIS_MODULE,
		.of_match_table = ut_tester_of_match,
	},
};

module_platform_driver(ut_tester_driver);
