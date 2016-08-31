/*
 * Copyright (C) 2008-2013, NVIDIA Corporation.  All rights reserved.
 *
 * Author:
 * Ashutosh Patel <ashutoshp@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * drivers/char/tegra-efshlp.c  - EFS filesystem helper
 *
 */
#include <linux/module.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/efshlp.h>
#include <linux/vmalloc.h>
#include <linux/mtd/map.h>
#include <linux/tegra_snor.h>
#include <linux/platform_data/tegra_nor.h>

#define EFSHLP_DEVICE "efshlp"
#define EFSHLP_DEVICE_NO 1
#define EFSHLP_MAJOR 0 /* Get dynamically */
#define EFSHLP_MINOR 0

#define USER_PART_OFFSET 0

static int efshlp_major;
static unsigned long efshlp_is_open;
static struct cdev efshlp_cdev;
static struct class *efshlp_class;

#define EFS_DMA_HANDLE_SIGNATURE 0x55aa

static int efs_handle_copy_dma(struct efshlp_dma *pdma)
{

	int done = 0, offset = pdma->offset;
	struct map_info *map;
	unsigned char *kernelbufaddr;
	unsigned char *addr = kzalloc(pdma->size, GFP_KERNEL);
	kernelbufaddr = addr;

	if (copy_from_user((void *) kernelbufaddr, (void *)pdma->buf,
				pdma->size))
		return -EFAULT;

	pr_debug("%s\n", __func__);

	if (offset & 3) {
		map = get_map_info(pdma->bank);
		tegra_nor_copy_from(map, (void *) kernelbufaddr,
				(offset & ~3), 4);
		if (copy_to_user(pdma->buf,
				(void *)(kernelbufaddr + (offset & 3)),
				pdma->size < (4 - (offset & 3))
					? pdma->size
					: (4 - (offset & 3))
				)) {
			kfree(addr);
			return -EFAULT;
		}
		pdma->buf += (4 - (offset & 3));
		done += (4 - (offset & 3));
		kernelbufaddr += (4 - (offset & 3));
		offset += (4 - (offset & 3));
	}

	while (done < pdma->size) {

		if ((pdma->size - done) > FLASHDMA_DMABUFSIZE) {

			map = get_map_info(pdma->bank);
			tegra_nor_copy_from(map, (void *) kernelbufaddr,
					offset, FLASHDMA_DMABUFSIZE);

			if (copy_to_user(pdma->buf, (void *)kernelbufaddr,
						FLASHDMA_DMABUFSIZE)) {
				kfree(addr);
				return -EFAULT;

			}

			offset += FLASHDMA_DMABUFSIZE;
			pdma->buf += FLASHDMA_DMABUFSIZE;
			done += FLASHDMA_DMABUFSIZE;
			kernelbufaddr += FLASHDMA_DMABUFSIZE;

		} else {

			map = get_map_info(pdma->bank);

			tegra_nor_copy_from(map, (void *) kernelbufaddr,
					offset, pdma->size - done);

			if (copy_to_user(pdma->buf, (void *)kernelbufaddr,
						pdma->size - done)) {
				kfree(addr);
				return -EFAULT;
			}

			offset += pdma->size - done;
			done += pdma->size - done;
			pdma->buf += pdma->size - done;
			kernelbufaddr += pdma->size - done;
		}
	}

	kfree(addr);
	return pdma->size;
}

static int efs_handle_mmap_dma(struct efshlp_dma *pdma,
		struct efs_dma_handle *handle, struct vm_area_struct *vma)
{
	unsigned long curaddr, virtaddr, offset;
	unsigned int i;
	unsigned long kernvirtaddr;
	long pdma_size = pdma->size;
	struct map_info *map;

	pr_debug("%s\n", __func__);

	if ((unsigned long)pdma->buf != vma->vm_start) {
		/* buffer address is not the start address returned from
		 * EfsAllocBuf */
		kernvirtaddr = (unsigned long)handle->virtaddr +
			((unsigned long)pdma->buf -
			 (unsigned long)vma->vm_start);

		pr_debug("%s: Allocated buffer not at start, kernVirtAddr=%lx\n",
				__func__, kernvirtaddr);

	} else {
		kernvirtaddr = (unsigned long)handle->virtaddr;
	}

	curaddr = kernvirtaddr;
	virtaddr = kernvirtaddr;
	offset = pdma->offset;
	map = get_map_info(pdma->bank);
	if (pdma_size >= PAGE_SIZE) {
		virtaddr = (unsigned long)(kernvirtaddr +
				(PAGE_SIZE - (kernvirtaddr % PAGE_SIZE)));

		tegra_nor_copy_from(map, (void *)(curaddr +
				(kernvirtaddr % PAGE_SIZE)), pdma->offset,
				PAGE_SIZE - (kernvirtaddr % PAGE_SIZE));

		pdma_size = pdma_size -
			(PAGE_SIZE - (kernvirtaddr % PAGE_SIZE));

		offset = pdma->offset + PAGE_SIZE - (kernvirtaddr % PAGE_SIZE);
	}

	for (i = 0; i < (pdma_size / PAGE_SIZE); i++) {

		curaddr = virtaddr;
		virtaddr += PAGE_SIZE;
		map = get_map_info(pdma->bank);

		tegra_nor_copy_from(map, (void *)curaddr,
				offset, PAGE_SIZE);
		pdma_size = pdma_size - PAGE_SIZE;

		offset += PAGE_SIZE;
	}

	curaddr = virtaddr;
	map = get_map_info(pdma->bank);

	if (pdma_size > 0) {
		tegra_nor_copy_from(map, (void *) curaddr, offset,
				pdma_size);
	}
	return pdma->size;
}

static long efshlp_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{

	struct efs_flash_info flash_info;
	struct efshlp_dma pdma;
	struct efs_dma_handle *handle;
	struct vm_area_struct *vma;
	int retval, i = 0;
	struct map_info *map;

	switch (cmd) {
	case EFSHLP_INIT:

		flash_info.total_flash_size = getflashsize();
		flash_info.user_part_size = getflashsize();
		flash_info.user_part_ofs = USER_PART_OFFSET;
		flash_info.nbanks = get_maps_no();

		for (i = 0; i < flash_info.nbanks; i++) {

			map = get_map_info(i);
			flash_info.bank_addr[i] = (unsigned long)map->virt;
			flash_info.bank_size[i] = map->size;
			flash_info.bank_switch_id[i] = i;

		}

		if (copy_to_user((void *)arg, (void *)&flash_info,
						sizeof(flash_info)))
				retval = -EFAULT;
			else
				retval = 0;
			break;

	case EFSHLP_DO_DMA:

		if (copy_from_user((void *)&pdma, (void *)arg,
						sizeof(pdma)))
				return -EFAULT;

		pr_debug("DO DMA\n");
		vma = find_vma(current->mm, (unsigned long)pdma.buf);

		if (!vma) {
			pr_debug("efs: No VMA found for read buffer!\n");
			retval = efs_handle_copy_dma(&pdma);
			goto dma_done;
		}

		handle = vma->vm_private_data;

		if (!handle) {
			pr_debug("efs: cannot find dma handle in VMA!\n");
			retval = efs_handle_copy_dma(&pdma);
			goto dma_done;
		}

		if (handle->signature != EFS_DMA_HANDLE_SIGNATURE) {
			pr_debug("efs: Invalid DMA handle!\n");
			retval = efs_handle_copy_dma(&pdma);
			goto dma_done;
		}

		/* check for allignment to dword * in both source and dst */

		if (((unsigned long)pdma.buf |
					(unsigned long)pdma.offset) & 3) {

			pr_debug("Alignment check failed, performing copydma!\n");
			retval = efs_handle_copy_dma(&pdma);
			goto dma_done;
		}

		retval = efs_handle_mmap_dma(&pdma, handle, vma);

dma_done:
		break;

	case EFSHLP_GET_ALLOC_SIZE:

		vma = find_vma(current->mm, (unsigned long)arg);

		if (!vma) {
			pr_debug("ALLOC null\n");
			return -1;
		}

		handle = vma->vm_private_data;

		if (!handle) {
			pr_debug("ALLOC no handle\n");
			return -1;
		}

		if (handle->signature != EFS_DMA_HANDLE_SIGNATURE) {
			pr_debug("ALLOC no sigature\n");
			return -1;
		}

		retval = handle->size;
		break;

	default:
		retval = -1;
		break;
	}

	return retval;
}

static void efshlp_mmap_open(struct vm_area_struct *area)
{

	struct efs_dma_handle *handle = area->vm_private_data;

	pr_debug("Allocating map %p %p %i\n", handle->virtaddr,
			(void *)handle->phyaddr, handle->size);

	handle->mapcount++;
}

static void efshlp_mmap_close(struct vm_area_struct *area)
{

	int i;
	struct efs_dma_handle *handle = area->vm_private_data;

	handle->mapcount--;

	if (!handle->mapcount) {

		pr_debug("Freeing map %p %p %i\n", handle->virtaddr,
				(void *)handle->phyaddr, handle->size);

		/* all references to this map have been freed */

		for (i = 0; i < handle->size; i += 4096)
			ClearPageReserved(vmalloc_to_page(handle->virtaddr+i));

		vfree(handle->virtaddr);
		kfree(handle);
	}
}

struct vm_operations_struct vmops = {
	.open =	efshlp_mmap_open,
	.close = efshlp_mmap_close
};

static int efshlp_mmap(struct file *filp, struct vm_area_struct *vma)
{
	size_t size = vma->vm_end - vma->vm_start;
	unsigned long i;
	struct efs_dma_handle *handle;

	pr_debug("%s\n", __func__);
	handle = kmalloc(sizeof(struct efs_dma_handle), GFP_KERNEL);

	if (!handle) {
		printk(KERN_ERR "efs: failed to allocate dma handle mem\n");
		return -ENOMEM;
	}

	handle->size = size;
	handle->mapcount = 1;
	handle->signature = EFS_DMA_HANDLE_SIGNATURE;

	handle->virtaddr = vmalloc(size);

	if (!handle->virtaddr) {
		printk(KERN_ERR "DMA KERNEL ALLOC FAILED\n");
		kfree(handle);
		return -ENOMEM;
	}

	/* map to userspace, one page at a time */
	for (i = 0 ; i < size ; i += PAGE_SIZE) {
		SetPageReserved(vmalloc_to_page(handle->virtaddr+i));

		remap_pfn_range(vma, vma->vm_start + i,
				vmalloc_to_pfn(handle->virtaddr+i),
				PAGE_SIZE, vma->vm_page_prot);
	}

	vma->vm_ops = &vmops;
	vma->vm_private_data = (void *) handle;

	return 0;
}

static int efshlp_open(struct inode *inode, struct file *file)
{
	/* Only one active open supported */

	if (test_and_set_bit(0, &efshlp_is_open))
		return -EBUSY;

	return 0;
}

static int efshlp_close(struct inode *inode, struct file *file)
{
	/* TODO: freeup all allocated memory here */
	clear_bit(0, &efshlp_is_open);
	return 0;
}

static const struct file_operations efshlp_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = efshlp_ioctl,
	.open  = efshlp_open,
	.mmap  = efshlp_mmap,
	.release = efshlp_close,
};


static void efshlp_cleanup(void)
{

	cdev_del(&efshlp_cdev);
	device_destroy(efshlp_class, MKDEV(efshlp_major, EFSHLP_MINOR));

	if (efshlp_class)
		class_destroy(efshlp_class);

	unregister_chrdev(efshlp_major, EFSHLP_DEVICE);
}

static int __init efshlp_init(void)
{
	int result;
	int ret = -ENODEV;
	dev_t efshlp_dev ;

	printk(KERN_INFO "EFS helper driver\n");

	result = alloc_chrdev_region(&efshlp_dev, 0,
			EFSHLP_DEVICE_NO, EFSHLP_DEVICE);

	efshlp_major = MAJOR(efshlp_dev);

	if (result < 0) {
		printk(KERN_ERR "alloc_chrdev_region() failed for efshlp\n");
		goto fail_err;
	}

	/* Register a character device. */
	cdev_init(&efshlp_cdev, &efshlp_fops);
	efshlp_cdev.owner = THIS_MODULE;
	efshlp_cdev.ops = &efshlp_fops;
	result = cdev_add(&efshlp_cdev, efshlp_dev, EFSHLP_DEVICE_NO);

	if (result < 0)
		goto fail_chrdev;

	/* Create a sysfs class. */
	efshlp_class = class_create(THIS_MODULE, EFSHLP_DEVICE);

	if (IS_ERR(efshlp_class)) {
		printk(KERN_ERR "efshlp: device class file already in use.\n");
		efshlp_cleanup();
		return PTR_ERR(efshlp_class);
	}

	device_create(efshlp_class, NULL,
			MKDEV(efshlp_major, EFSHLP_MINOR),
			NULL, "%s", EFSHLP_DEVICE);

	return 0;

fail_chrdev:
	unregister_chrdev_region(efshlp_dev, EFSHLP_DEVICE_NO);

fail_err:
	return ret;
}

static void __exit efshlp_exit(void)
{
	device_destroy(efshlp_class, MKDEV(efshlp_major, EFSHLP_MINOR));

	if (efshlp_class)
		class_destroy(efshlp_class);

	cdev_del(&efshlp_cdev);
	unregister_chrdev_region(MKDEV(efshlp_major, EFSHLP_MINOR),
			EFSHLP_DEVICE_NO);
}

module_init(efshlp_init);
module_exit(efshlp_exit);
MODULE_AUTHOR("Ashutosh Patel <ashutoshp@nvidia.com>");
MODULE_LICENSE("GPL");
