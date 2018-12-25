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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/printk.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/debugfs.h>

#include <mt-plat/mtk_io.h>
#include <mt-plat/sync_write.h>

#include <dramc.h>
#include <mt_emi.h>

static struct dentry *emi_mbw_dir;
static struct dentry *dump_buf;
static void __iomem *mbw_dram_buf;
static bool elm_enabled;

static ssize_t dump_buf_read
	(struct file *file, char __user *data, size_t len, loff_t *ppos)
{
	ssize_t bytes = len < (MBW_BUF_LEN - *ppos) ?
		len : (MBW_BUF_LEN - *ppos);

	if (!mbw_dram_buf)
		return 0;

	if (bytes) {
		if (copy_to_user(data, (char *)mbw_dram_buf + *ppos, bytes))
			return -EFAULT;
	}

	*ppos += bytes;

	return bytes;
}

static int dump_buf_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static const struct file_operations dump_buf_fops = {
	.owner = THIS_MODULE,
	.read = dump_buf_read,
	.open = dump_buf_open
};

void elm_init(struct platform_driver *emi_ctrl, struct platform_device *pdev)
{
	void __iomem *LAST_EMI_BASE;
	unsigned int mbw_buf_h, mbw_buf_l;
	phys_addr_t mbw_buf_start;

	pr_info("[ELM] initialize EMI ELMv2\n");

	LAST_EMI_BASE = get_dbg_info_base(0xE31C);
	if (!LAST_EMI_BASE) {
		pr_err("[ELM] get LAST_EMI_BASE fail\n");
		return;
	}

	elm_enabled = false;
	if (readl(IOMEM(LAST_EMI_DECS_CTRL)) == 0xDECDDECD) {
		mbw_buf_l = readl(IOMEM(LAST_EMI_MBW_BUF_L));
		mbw_buf_h = readl(IOMEM(LAST_EMI_MBW_BUF_H));
		mbw_buf_start = ((phys_addr_t)mbw_buf_h << 32) + mbw_buf_l;
		mbw_dram_buf = ioremap_wc(mbw_buf_start, MBW_BUF_LEN);
		elm_enabled = true;
		pr_info("[ELM] enable mbw_buf dump\n");
	}

	emi_mbw_dir = debugfs_create_dir("emi_mbw", NULL);
	if (!emi_mbw_dir) {
		pr_err("[ELM] create dir fail\n");
		return;
	}

	dump_buf = debugfs_create_file("dump_buf", 0444,
		emi_mbw_dir, NULL, &dump_buf_fops);
	if (!dump_buf) {
		pr_err("[ELM] create dump_buf fail\n");
		return;
	}
}

void suspend_elm(void)
{
}

void resume_elm(void)
{
}

