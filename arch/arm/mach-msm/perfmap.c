/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* Character device driver for memory mapped performance counter interface */
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/cpu_pm.h>
#include <linux/cpu.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/sched.h>
#include <linux/cdev.h>

uint32_t perfmap_base;
uint32_t PERF_BASE_SIZE;


static dev_t perfmap_dev;
static struct cdev perfmap_cdev;
static struct class *perfmap_devclass;

static struct of_device_id perfmap_of_device_ids[] = {
	{.compatible = "qcom,perfmap"},
	{},
};

static int perfmap_device_probe(struct platform_device *pdev)
{
	struct resource *resource;

	resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!resource)
		return -EINVAL;
	perfmap_base = resource->start;

	return 0;
}

static struct platform_driver perfmap_pmu_driver = {
	.driver		= {
		.name	= "perfmap",
		.of_match_table = perfmap_of_device_ids,
	},
	.probe		= perfmap_device_probe,
};

static int perfmap_mmap(struct file *filep, struct vm_area_struct *vma)
{
	int ret = 0;
	unsigned long pfn_num;

	pfn_num = perfmap_base>>PAGE_SHIFT;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	if (remap_pfn_range(vma, vma->vm_start, pfn_num,
		vma->vm_end-vma->vm_start,
		vma->vm_page_prot)) {
		return -EAGAIN;
	}
	return ret;
}

static const struct file_operations perfmap_fops = {
	.mmap	= perfmap_mmap,
};

static int perfmap_init(void)
{
	int result = 0;

	platform_driver_register(&perfmap_pmu_driver);
	if (alloc_chrdev_region(&perfmap_dev, 0, 1, "perfmap") < 0) {
		pr_err("perfmap: Error in alloc_chrdev_region\n");
		goto alloc_err;
	}
	perfmap_devclass = class_create(THIS_MODULE, "chardrv");
	if (IS_ERR(perfmap_devclass)) {
		pr_err("perfmap: Error in class_create\n");
		goto class_err;
	}
	if (device_create(perfmap_devclass, NULL, perfmap_dev, NULL,
			"perfmap0") == NULL) {
		pr_err("perfmap: Error in device_create\n");
		goto create_err;
	}
	cdev_init(&perfmap_cdev, &perfmap_fops);
	if (cdev_add(&perfmap_cdev, perfmap_dev, 1) == -1) {
		pr_err("perfmap: Error in cdev_add\n");
		goto add_err;
	}
	return result;
add_err:
	device_destroy(perfmap_devclass, perfmap_dev);
create_err:
	class_destroy(perfmap_devclass);
class_err:
	unregister_chrdev_region(perfmap_dev, 1);
alloc_err:
	result = -ENODEV;
	return result;
}

static void perfmap_exit(void)
{
	cdev_del(&perfmap_cdev);
	device_destroy(perfmap_devclass, perfmap_dev);
	class_destroy(perfmap_devclass);
	unregister_chrdev_region(perfmap_dev, 1);
	platform_driver_unregister(&perfmap_pmu_driver);
}

module_init(perfmap_init);
module_exit(perfmap_exit);

MODULE_LICENSE("GPL v2");
