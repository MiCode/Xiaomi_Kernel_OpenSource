/* Copyright (c) 2018 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/memblock.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <asm/pgtable.h>
#include <media/adsp-shmem-device.h>

#define DEVICE_NAME "adsp_shmem"
#define CLASS_NAME "adsp_class"

static dev_t adsp_dev;
static struct cdev *adsp_cdev;
static struct class *adsp_class;
static struct device *adsp_device;

static void *adsp_shmem_vaddr;
static phys_addr_t adsp_shmem_paddr;
static unsigned long adsp_shmem_size;

int adsp_shmem_get_state(void)
{
	int status;

	if (adsp_shmem_vaddr) {
		struct adsp_camera_header *adsp_header_ptr = adsp_shmem_vaddr;

		status = adsp_header_ptr->status;
		if (status >= CAMERA_STATUS_STOP && status <= CAMERA_STATUS_END)
			return status;
	}

	return CAMERA_STATUS_END;
}

void adsp_shmem_set_state(enum camera_status_state state)
{
	if (adsp_shmem_vaddr) {
		struct adsp_camera_header *adsp_header_ptr = adsp_shmem_vaddr;

		adsp_header_ptr->status = state;
	}
}

const char *adsp_shmem_get_sensor_name(void)
{
	if (adsp_shmem_vaddr) {
		struct adsp_camera_header *adsp_header_ptr = adsp_shmem_vaddr;

		return adsp_header_ptr->sensor_name;
	} else {
		return NULL;
	}
}

int adsp_shmem_is_initialized(void)
{
	return (adsp_shmem_vaddr) ? 1 : 0;
}

int adsp_shmem_is_working(void)
{
	int status = adsp_shmem_get_state();

	if (status == CAMERA_STATUS_STOP || status == CAMERA_STATUS_END)
		return false;

	return true;
}

static ssize_t adsp_shmem_device_mem_read(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%lu", adsp_shmem_size);
}

static DEVICE_ATTR(mem, 0444, adsp_shmem_device_mem_read, NULL);

static int op_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long pfn;
	unsigned long req_map_size = vma->vm_end - vma->vm_start;

	pfn = __phys_to_pfn(adsp_shmem_paddr);
	vma->vm_flags |= VM_IO;

	if (io_remap_pfn_range(vma, vma->vm_start, pfn, req_map_size,
			       vma->vm_page_prot)) {
		pr_err("%s: Error io_remap_pfn_range\n", __func__);
		return -EAGAIN;
	}

	return 0;
}

static int mmapfop_close(struct inode *inode, struct file *filp)
{
	return 0;
}

static int mmapfop_open(struct inode *inode, struct file *filp)
{
	return 0;
}
static const struct file_operations mmap_fops = {
	.open = mmapfop_open,
	.release = mmapfop_close,
	.mmap = op_mmap,
};

static int adsp_shmem_device_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *node = pdev->dev.of_node;
	struct device_node *pnode;
	struct vm_struct *area;
	unsigned int flags;
	unsigned long vaddr;
	const __be32 *basep;
	u64 size;
	phys_addr_t base;

	ret = alloc_chrdev_region(&adsp_dev, 0, 1, DEVICE_NAME);
	if (ret != 0)
		return ret;

	adsp_cdev = cdev_alloc();
	if (!adsp_cdev)
		return -ENOMEM;

	cdev_init(adsp_cdev, &mmap_fops);

	ret = cdev_add(adsp_cdev, adsp_dev, 1);
	if (ret < 0)
		goto cdev_add_error;

	adsp_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(adsp_class)) {
		ret = PTR_ERR(adsp_class);
		goto class_error;
	}

	adsp_device = device_create(adsp_class, NULL, adsp_dev, NULL,
				    DEVICE_NAME);
	if (IS_ERR(adsp_device)) {
		ret = PTR_ERR(adsp_device);
		goto device_err;
	}

	pnode = of_parse_phandle(node, "memory-region", 0);
	if (pnode) {
		basep = of_get_address(pnode, 0, &size, &flags);
		if (basep) {
			base = of_translate_address(pnode, basep);
			dev_dbg(&pdev->dev, "%s:base: 0x%x siz:%lu flgs:0x%x\n",
				__func__, (unsigned int)base,
				(unsigned long)size, flags);

			area = get_vm_area(size, VM_IOREMAP);
			if (!area) {
				ret = -ENOMEM;
				goto vm_area_error;
			}

			vaddr = (unsigned long)area->addr;
			if (ioremap_page_range(vaddr, (vaddr + size),
					       base, PAGE_KERNEL)) {
				ret = -ENOMEM;
				goto ioremap_error;
			}

			device_create_file(&pdev->dev, &dev_attr_mem);

			adsp_shmem_vaddr = area->addr;
			adsp_shmem_paddr = base;
			adsp_shmem_size = size;

			of_node_put(pnode);
		}
	}

	return ret;

ioremap_error:
	vunmap(area->addr);
vm_area_error:
	of_node_put(pnode);
	device_destroy(adsp_class, adsp_dev);
device_err:
	class_destroy(adsp_class);
class_error:
	cdev_del(adsp_cdev);
cdev_add_error:
	kfree(adsp_cdev);
	return ret;
}

static int adsp_shmem_device_remove(struct platform_device *pdev)
{
	vunmap(adsp_shmem_vaddr);
	device_destroy(adsp_class, adsp_dev);
	class_destroy(adsp_class);
	cdev_del(adsp_cdev);

	device_remove_file(&pdev->dev, &dev_attr_mem);
	return 0;
}

static const struct of_device_id adsp_shmem_device_dt_match[] = {
	{.compatible = "adsp-shmem-device"},
	{},
};

static struct platform_driver adsp_shmem_device_driver = {
	.probe  = adsp_shmem_device_probe,
	.remove = adsp_shmem_device_remove,
	.driver = {
		.name = "adsp-shmem-device",
		.owner = THIS_MODULE,
		.of_match_table = adsp_shmem_device_dt_match,
	},
};

static int __init adsp_shmem_device_init(void)
{
	return platform_driver_register(&adsp_shmem_device_driver);
}

static void __exit adsp_shmem_device_exit(void)
{
	platform_driver_unregister(&adsp_shmem_device_driver);
}

subsys_initcall(adsp_shmem_device_init);
module_exit(adsp_shmem_device_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("ADSP Shared Memory Device driver");
