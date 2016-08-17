/*
 * arch/arch/mach-tegra/mipi-cal.c
 *
 * Copyright (C) 2013 NVIDIA Corporation.
 *
 * Author:
 *	Charlie Huang <chahuang@nvidia.com>
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
#include <linux/io.h>

#include <mach/iomap.h>

#define CLK_OUT_ENB_H		0x014

#define clk_writel(value, reg)	__raw_writel(value, (u32)clk_base + (reg))
#define clk_readl(reg)		__raw_readl((u32)clk_base + (reg))

static void __iomem		*clk_base = IO_ADDRESS(TEGRA_CLK_RESET_BASE);
static atomic_t			mipi_cal_in_use;
static u32			saved_bit;

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
	u32 val;

	/* only one client can work on the mipi-cal registers at one time. */
	if (atomic_xchg(&mipi_cal_in_use, 1))
		return -EBUSY;

	/* Save enable bit to MIPI CAL Logic */
	val = clk_readl(CLK_OUT_ENB_H);
	saved_bit = val & 0x1000000;
	/* Enable clock to MIPI CAL Logic */
	val |= 0x1000000;
	clk_writel(val, CLK_OUT_ENB_H);
	return nonseekable_open(inode, filp);
}

static int mipi_cal_release(struct inode *inode, struct file *filp)
{
	u32 val;

	val = clk_readl(CLK_OUT_ENB_H);
	val &= ~0x1000000;
	/* Restore enable bit to MIPI CAL Logic */
	val |= saved_bit;
	clk_writel(val, CLK_OUT_ENB_H);
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
	return misc_register(&mipi_cal_dev);
}

module_init(mipi_cal_dev_init);
MODULE_LICENSE("GPL");
