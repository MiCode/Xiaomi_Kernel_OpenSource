// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Xiaomi, Inc.
 */

#define pr_fmt(fmt) "ispv4 elog: " fmt
#define DEBUG

#include "xm_ispv4_rproc.h"
#include <linux/slab.h>

#define ISPV4_ELOG_NAME "ispv4-elog"

static int ispv4_elogdev_open(struct inode *inode, struct file *filp)
{
	int ret;
	struct ispv4_elog_dev *edev =
		container_of(inode->i_cdev, struct ispv4_elog_dev, cdev);
	ret = atomic_cmpxchg(&edev->opened, 0, 1);
	if (ret != 0)
		ret = -EBUSY;
	else
		filp->private_data = edev;

	return ret;
}

static int ispv4_elogdev_release(struct inode *nodp, struct file *filp)
{
	struct ispv4_elog_dev *edev = filp->private_data;
	atomic_set(&edev->opened, 0);
	return 0;
}

static int ispv4_elogdev_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct ispv4_elog_dev *edev = file->private_data;
	unsigned long vsize = vma->vm_end - vma->vm_start;
	unsigned long psize = 1024 * 1024;
	struct page *page;
	struct scatterlist *sg, *sgl = edev->rp->elog_dma_sg;
	int i, sg_nents = edev->rp->elog_dma_sg_nents;
	unsigned long addr = vma->vm_start;

	vsize = vma->vm_end - vma->vm_start;

	if (vsize > psize)
		return -EINVAL;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_flags |= VM_IO | VM_DONTEXPAND | VM_DONTDUMP;

	page = sg_page(sgl);
	if (page) {
		for_each_sg(sgl, sg, sg_nents, i) {
			unsigned long remainder = vma->vm_end - addr;
			unsigned long len = sg->length;
			page = sg_page(sg);
			len = min(len, remainder);
			remap_pfn_range(vma, addr, page_to_pfn(page), len,
					vma->vm_page_prot);
			addr += len;
			if (addr >= vma->vm_end)
				return 0;
		}
	} else {
		return -EINVAL;
	}

	return 0;
}


static const struct file_operations rpept_cdev_fops = {
	.owner = THIS_MODULE,
	.open = ispv4_elogdev_open,
	.release = ispv4_elogdev_release,
	.mmap = ispv4_elogdev_mmap,
};

static void elog_device_release(struct device *dev)
{
	struct ispv4_elog_dev *edev =
		container_of(dev, struct ispv4_elog_dev, dev);
	cdev_del(&edev->cdev);
	kfree(edev);
}

int ispv4_elog_init(struct xm_ispv4_rproc *rp)
{
	int ret = 0;
	struct ispv4_elog_dev *edev;

	ret = alloc_chrdev_region(&rp->elog_devt, 0, 1, ISPV4_ELOG_NAME);
	if (ret != 0) {
		pr_err("alloc cdev region failed %d\n", ret);
		goto alloc_cr_err;
	}
	rp->elog_class = class_create(THIS_MODULE, ISPV4_ELOG_NAME);
	if (IS_ERR_OR_NULL(rp->elog_class)) {
		ret = PTR_ERR(rp->elog_class);
		pr_err("class create failed %d\n", ret);
		goto class_err;
	}

	edev = kzalloc(sizeof(*rp->elog_dev), GFP_KERNEL);
	if (rp->elog_dev == NULL) {
		ret = -ENOMEM;
		goto err_alloc;
	}

	rp->elog_dev = edev;
	edev->rp = rp;
	atomic_set(&edev->opened, 0);

	cdev_init(&edev->cdev, NULL);
	edev->cdev.owner = THIS_MODULE;
	ret = cdev_add(&edev->cdev, rp->elog_devt, 1);
	if (ret != 0) {
		pr_err("cdev add failed %s %d\n", ret);
		goto err_cd_add;
	}

	device_initialize(&edev->dev);
	dev_set_name(&edev->dev, "%s-io", ISPV4_ELOG_NAME);
	edev->dev.release = elog_device_release;
	edev->dev.class = rp->elog_class;
	edev->dev.devt = rp->elog_devt;

	ret = device_add(&edev->dev);
	if (ret != 0) {
		pr_err("bind-dev add failed %d\n", ret);
		/* release resource in `rpept_device_release` */
		put_device(&edev->dev);
		goto err_dev_add;
	}

	pr_info("init finish! success!\n");
	return 0;

err_cd_add:
	kfree(edev);
err_dev_add: /* cdev_del and kfree will do in release */
err_alloc:
	class_destroy(rp->elog_class);
class_err:
	unregister_chrdev_region(rp->elog_devt, 1);
alloc_cr_err:
	return ret;
}

void ispv4_elog_exit(struct xm_ispv4_rproc *rp)
{
	device_del(&rp->elog_dev->dev);
	put_device(&rp->elog_dev->dev);
	unregister_chrdev_region(rp->elog_devt, 1);
	class_destroy(rp->elog_class);
}
