/*
 * arch/arch/mach-tegra/mipi-cal.c
 *
 * Copyright (C) 2013-2014 NVIDIA Corporation. All rights reserved.
 *
 * Author:
 *	Charlie Huang <chahuang@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/clk.h>

#include "iomap.h"

struct mipi_cal_info {
	struct clk *clk;
	struct clk *clk72mhz;
};

static struct mipi_cal_info	pm;
static atomic_t			mipi_cal_in_use;

static int mipi_cal_open(struct inode *inode, struct file *filp);
static int mipi_cal_release(struct inode *inode, struct file *filp);
static int mipi_cal_dev_mmap(struct file *file, struct vm_area_struct *vma);

static const struct file_operations mipi_cal_dev_fops = {
	.owner = THIS_MODULE,
	.open = mipi_cal_open,
	.release = mipi_cal_release,
	.mmap = mipi_cal_dev_mmap,
	.llseek = noop_llseek,
};

static struct miscdevice mipi_cal_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "mipi-cal",
	.fops = &mipi_cal_dev_fops,
};

static int mipi_cal_open(struct inode *inode, struct file *filp)
{
	if (atomic_xchg(&mipi_cal_in_use, 1))
		return -EBUSY;

	if (pm.clk72mhz)
		clk_prepare_enable(pm.clk72mhz);
	if (pm.clk)
		clk_prepare_enable(pm.clk);

	return nonseekable_open(inode, filp);
}

static int mipi_cal_release(struct inode *inode, struct file *filp)
{
	if (pm.clk)
		clk_disable_unprepare(pm.clk);
	if (pm.clk72mhz)
		clk_disable_unprepare(pm.clk72mhz);

	WARN_ON(!atomic_xchg(&mipi_cal_in_use, 0));
	return 0;
}

static int mipi_cal_dev_mmap(struct file *file, struct vm_area_struct *vma)
{
	phys_addr_t addr = TEGRA_MIPI_CAL_BASE;

	if (vma->vm_end  - vma->vm_start != PAGE_SIZE)
		return -EINVAL;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	if (remap_pfn_range(vma, vma->vm_start, addr >> PAGE_SHIFT, PAGE_SIZE,
		vma->vm_page_prot)) {
		pr_err("%s:remap_pfn_range for mipi_cal failed\n",
			mipi_cal_dev.name);
		return -EAGAIN;
	}

	return 0;
}

static int __init mipi_cal_dev_init(void)
{
	pm.clk = clk_get_sys("mipi-cal", NULL);
	if (IS_ERR_OR_NULL(pm.clk)) {
		pr_warn("%s: cannot get mipi-cal clk.\n", __func__);
		pm.clk = NULL;
	}
#ifndef CONFIG_ARCH_TEGRA_11x_SOC
	pm.clk72mhz = clk_get_sys("clk72mhz", NULL);
	if (IS_ERR_OR_NULL(pm.clk72mhz)) {
		pr_warn("%s: cannot get mipi-cal clk.\n", __func__);
		pm.clk72mhz = NULL;
	}
#endif
	return misc_register(&mipi_cal_dev);
}

module_init(mipi_cal_dev_init);
MODULE_LICENSE("GPL v2");
