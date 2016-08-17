/*
 * arch/arch/mach-tegra/timerinfo.c
 *
 * Copyright (C) 2012 NVIDIA Corporation.
 *
 * Author:
 *	Jon Mayo <jmayo@nvidia.com>
 *
 * Copyright (C) 2012 NVIDIA Corporation.
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

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/export.h>
#include <linux/module.h>

#include <mach/iomap.h>

#include "timer.h"

static int timerinfo_dev_mmap(struct file *file, struct vm_area_struct *vma);

static const struct file_operations timerinfo_dev_fops = {
	.owner = THIS_MODULE,
	.open = nonseekable_open,
	.mmap = timerinfo_dev_mmap,
	.llseek = noop_llseek,
};

static struct miscdevice timerinfo_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "timerinfo",
	.fops = &timerinfo_dev_fops,
};

static int timerinfo_dev_mmap(struct file *file, struct vm_area_struct *vma)
{
	/* start at first page containing TIMERUS_CNTR_1US */
	phys_addr_t addr = TEGRA_TMR1_BASE;

	if (vma->vm_end  - vma->vm_start != PAGE_SIZE)
		return -EINVAL;

	if (vma->vm_flags & VM_WRITE)
		return -EPERM;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	if (remap_pfn_range(vma, vma->vm_start, addr >> PAGE_SHIFT, PAGE_SIZE,
		vma->vm_page_prot)) {
		pr_err("%s:remap_pfn_range failed\n", timerinfo_dev.name);
		return -EAGAIN;
	}

	return 0;
}

static int __init timerinfo_dev_init(void)
{
	return misc_register(&timerinfo_dev);
}

module_init(timerinfo_dev_init);
MODULE_LICENSE("GPL");
