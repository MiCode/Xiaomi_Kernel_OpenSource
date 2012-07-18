/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

/* #define DEBUG */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/memory_alloc.h>
#include "msm-buspm-dev.h"

#define MSM_BUSPM_DRV_NAME "msm-buspm-dev"

/*
 * Allocate kernel buffer.
 * Currently limited to one buffer per file descriptor.  If alloc() is
 * called twice for the same descriptor, the original buffer is freed.
 * There is also no locking protection so the same descriptor can not be shared.
 */

static inline void *msm_buspm_dev_get_vaddr(struct file *filp)
{
	struct msm_buspm_map_dev *dev = filp->private_data;

	return (dev) ? dev->vaddr : NULL;
}

static inline unsigned int msm_buspm_dev_get_buflen(struct file *filp)
{
	struct msm_buspm_map_dev *dev = filp->private_data;

	return dev ? dev->buflen : 0;
}

static inline unsigned long msm_buspm_dev_get_paddr(struct file *filp)
{
	struct msm_buspm_map_dev *dev = filp->private_data;

	return (dev) ? dev->paddr : 0L;
}

static void msm_buspm_dev_free(struct file *filp)
{
	struct msm_buspm_map_dev *dev = filp->private_data;

	if (dev) {
		pr_debug("freeing memory at 0x%p\n", dev->vaddr);
		free_contiguous_memory(dev->vaddr);
		dev->paddr = 0L;
		dev->vaddr = NULL;
	}
}

static int msm_buspm_dev_open(struct inode *inode, struct file *filp)
{
	struct msm_buspm_map_dev *dev;

	if (capable(CAP_SYS_ADMIN)) {
		dev = kzalloc(sizeof(*dev), GFP_KERNEL);
		if (dev)
			filp->private_data = dev;
		else
			return -ENOMEM;
	} else {
		return -EPERM;
	}

	return 0;
}

static int
msm_buspm_dev_alloc(struct file *filp, struct buspm_alloc_params data)
{
	unsigned long paddr;
	void *vaddr;
	struct msm_buspm_map_dev *dev = filp->private_data;

	/* If buffer already allocated, then free it */
	if (dev->vaddr)
		msm_buspm_dev_free(filp);

	/* Allocate uncached memory */
	vaddr = allocate_contiguous_ebi(data.size, PAGE_SIZE, 0);
	paddr = (vaddr) ? memory_pool_node_paddr(vaddr) : 0L;

	if (vaddr == NULL) {
		pr_err("allocation of 0x%x bytes failed", data.size);
		return -ENOMEM;
	}

	dev->vaddr = vaddr;
	dev->paddr = paddr;
	dev->buflen = data.size;
	filp->f_pos = 0;
	pr_debug("virt addr = 0x%p\n", dev->vaddr);
	pr_debug("phys addr = 0x%lx\n", dev->paddr);

	return 0;
}

static long
msm_buspm_dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct buspm_xfer_req xfer;
	struct buspm_alloc_params alloc_data;
	unsigned long paddr;
	int retval = 0;
	void *buf = msm_buspm_dev_get_vaddr(filp);
	unsigned int buflen = msm_buspm_dev_get_buflen(filp);
	unsigned char *dbgbuf = buf;

	switch (cmd) {
	case MSM_BUSPM_IOC_FREE:
		pr_debug("cmd = 0x%x (FREE)\n", cmd);
		msm_buspm_dev_free(filp);
		break;

	case MSM_BUSPM_IOC_ALLOC:
		pr_debug("cmd = 0x%x (ALLOC)\n", cmd);
		retval = __get_user(alloc_data.size, (size_t __user *)arg);

		if (retval == 0)
			retval = msm_buspm_dev_alloc(filp, alloc_data);
		break;

	case MSM_BUSPM_IOC_RD_PHYS_ADDR:
		pr_debug("Read Physical Address\n");
		paddr = msm_buspm_dev_get_paddr(filp);
		if (paddr == 0L) {
			retval = -EINVAL;
		} else {
			pr_debug("phys addr = 0x%lx\n", paddr);
			retval = __put_user(paddr,
				(unsigned long __user *)arg);
		}
		break;

	case MSM_BUSPM_IOC_RDBUF:
		pr_debug("Read Buffer: 0x%x%x%x%x\n",
				dbgbuf[0], dbgbuf[1], dbgbuf[2], dbgbuf[3]);

		if (!buf) {
			retval = -EINVAL;
			break;
		}

		if (copy_from_user(&xfer, (void __user *)arg, sizeof(xfer))) {
			retval = -EFAULT;
			break;
		}

		if ((xfer.size <= buflen) &&
			(copy_to_user((void __user *)xfer.data, buf,
					xfer.size))) {
			retval = -EFAULT;
			break;
		}
		break;

	case MSM_BUSPM_IOC_WRBUF:
		pr_debug("Write Buffer\n");

		if (!buf) {
			retval = -EINVAL;
			break;
		}

		if (copy_from_user(&xfer, (void __user *)arg, sizeof(xfer))) {
			retval = -EFAULT;
			break;
		}

		if ((buflen <= xfer.size) &&
			(copy_from_user(buf, (void __user *)xfer.data,
			xfer.size))) {
			retval = -EFAULT;
			break;
		}
		break;

	default:
		pr_debug("Unknown command 0x%x\n", cmd);
		retval = -EINVAL;
		break;
	}

	return retval;
}

static int msm_buspm_dev_release(struct inode *inode, struct file *filp)
{
	struct msm_buspm_map_dev *dev = filp->private_data;

	msm_buspm_dev_free(filp);
	kfree(dev);
	filp->private_data = NULL;

	return 0;
}

static int msm_buspm_dev_mmap(struct file *filp, struct vm_area_struct *vma)
{
	pr_debug("vma = 0x%p\n", vma);

	/* Mappings are uncached */
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
		vma->vm_end - vma->vm_start, vma->vm_page_prot))
		return -EFAULT;

	return 0;
}

static const struct file_operations msm_buspm_dev_fops = {
	.owner		= THIS_MODULE,
	.mmap		= msm_buspm_dev_mmap,
	.open		= msm_buspm_dev_open,
	.unlocked_ioctl	= msm_buspm_dev_ioctl,
	.llseek		= noop_llseek,
	.release	= msm_buspm_dev_release,
};

struct miscdevice msm_buspm_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= MSM_BUSPM_DRV_NAME,
	.fops	= &msm_buspm_dev_fops,
};

static int __init msm_buspm_dev_init(void)
{
	int ret = 0;

	ret = misc_register(&msm_buspm_misc);
	if (ret < 0)
		pr_err("%s: Cannot register misc device\n", __func__);

	return ret;
}

static void __exit msm_buspm_dev_exit(void)
{
	misc_deregister(&msm_buspm_misc);
}
module_init(msm_buspm_dev_init);
module_exit(msm_buspm_dev_exit);

MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:"MSM_BUSPM_DRV_NAME);
