/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/version.h>
#include <asm/io.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/genalloc.h>
#include <linux/sched.h>
#include <linux/mutex.h>
//#include <linux/xlog.h>
#include <linux/proc_fs.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <mt-plat/mtk_gpu_utility.h>

#include "ged_base.h"
#include "ged_mm.h"
#include "ged_debugFS.h"

#include "ged_dvfs.h"

static struct dentry* gpsMMDir = NULL;
static struct dentry* gpsDvfsServiceData = NULL;

GED_DVFS_POLICY_DATA* gpDVFSdata=NULL;

void mmap_open(struct vm_area_struct *vma)
{
	struct mmap_info *info = (struct mmap_info *)vma->vm_private_data;
}

static int mmap_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct page *page;
	struct mmap_info *info;
	/* is the address valid? */			//--changed
	/*if (address > vma->vm_end) {
	  printk("invalid address\n");
	//return NOPAGE_SIGBUS;
	return VM_FAULT_SIGBUS;
	}
	/* the data is in vma->vm_private_data */
	info = (struct mmap_info *)vma->vm_private_data;
	if (!info) {
		printk("no data\n");
		return NULL;	
	}

	/* get the page */
	page = virt_to_page(info);

	/* increment the reference count of this page */
	get_page(page);
	vmf->page = page;					//--changed
	/* type is the page fault type */
	/*if (type)
	 *type = VM_FAULT_MINOR;
	 */
	return 0;
}

struct vm_operations_struct mmap_vm_ops = {
	.open =     mmap_open,
	.close =    mmap_open,
	.fault =    mmap_fault,
};


int ged_dvfs_service_data_mmap(struct file *filp, struct vm_area_struct *vma)
{
	vma->vm_ops = &mmap_vm_ops;
	vma->vm_flags |= VM_RESERVED;
	/* assign the file private data to the vm private data */
	vma->vm_private_data = filp->private_data;
	mmap_open(vma);
	return 0;
}

int ged_dvfs_service_data_close(struct inode *inode, struct file *filp)
{
	void *info = filp->private_data;
	/* obtain new memory */
	//kfree(info);
	free_page(info);
	gpDVFSdata = NULL;
	filp->private_data = NULL;
	return 0;
}

int ged_dvfs_service_data_open(struct inode *inode, struct file *filp)
{
	void *info;
	/* obtain new memory */
	//info = kmalloc(sizeof(GED_DVFS_POLICY_DATA),GFP_KERNEL);
	info = get_zeroed_page(GFP_KERNEL);
	//info = kmalloc(sizeof(GED_DVFS_POLICY_DATA),GFP_KERNEL);
	gpDVFSdata =(GED_DVFS_POLICY_DATA*) info;
	/* assign this info struct to the file */
	filp->private_data = info;
	return 0;
}

static const struct file_operations gsDVFSServiceData = {
	.open = ged_dvfs_service_data_open,
	.release = ged_dvfs_service_data_close,
	.mmap = ged_dvfs_service_data_mmap,
};

//-----------------------------------------------------------------------------

GED_ERROR ged_mm_init(void)
{
	GED_ERROR err = GED_OK;

	err = ged_debugFS_create_entry_dir(
			"mm",
			NULL,
			&gpsMMDir);

	if (unlikely(err != GED_OK))
	{
		err = GED_ERROR_FAIL;
		GED_LOGE("ged: failed to create mm dir!\n");
		goto ERROR;
	}


	gpsDvfsServiceData = debugfs_create_file("ged_dvfs_service_data", 0644, gpsMMDir, NULL, &gsDVFSServiceData);


	return err;

ERROR:

	ged_mm_exit();

	return err;
}
//-----------------------------------------------------------------------------
void ged_mm_exit(void)
{
	debugfs_remove(gpsDvfsServiceData);
	ged_debugFS_remove_entry_dir(gpsMMDir);
}
//-----------------------------------------------------------------------------
