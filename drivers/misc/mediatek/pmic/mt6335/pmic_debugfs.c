/*
 * Copyright (C) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <generated/autoconf.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <mt-plat/upmu_common.h>
#include "include/pmic.h"

unsigned int gPMICDbgLvl;

/*-------set function-------*/
static unsigned int pmic_dbg_level_set(unsigned int level)
{
	gPMICDbgLvl = level > PMIC_LOG_DBG ? PMIC_LOG_DBG : level;
	return 0;
}

/*-------pmic_dbg_level-------*/
static ssize_t pmic_dbg_level_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
	char info[10];
	int value = 0;

	memset(info, 0, 10);

	if (copy_from_user(info, buf, size))
		return -EFAULT;

	if ((info[0] >= '0') && (info[0] <= '9'))
		value = (info[0] - 48);

	pmic_dbg_level_set(value);

	pr_err("pmic_dbg_level_write = %d\n", gPMICDbgLvl);

	return size;
}

static int pmic_dbg_level_show(struct seq_file *s, void *unused)
{
	seq_puts(s, "4:PMIC_LOG_DBG\n");
	seq_puts(s, "3:PMIC_LOG_INFO\n");
	seq_puts(s, "2:PMIC_LOG_NOT\n");
	seq_puts(s, "1:PMIC_LOG_WARN\n");
	seq_puts(s, "0:PMIC_LOG_ERR\n");
	seq_printf(s, "PMIC_Dbg_Lvl = %d\n", gPMICDbgLvl);
	return 0;
}

static int pmic_dbg_level_open(struct inode *inode, struct file *file)
{
	return single_open(file, pmic_dbg_level_show, NULL);
}

static const struct file_operations pmic_dbg_level_operations = {
	.open    = pmic_dbg_level_open,
	.read    = seq_read,
	.write   = pmic_dbg_level_write,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int __init pmic_debugfs_init(void)
{
	struct dentry *mt_pmic;

	mt_pmic = debugfs_create_dir("mt_pmic", NULL);
	if (!mt_pmic)
		pr_err("create dir mt_pmic fail\n");

	debugfs_create_file("pmic_dbg_level", (S_IFREG | S_IRUGO), mt_pmic, NULL, &pmic_dbg_level_operations);

	return 0;
}
subsys_initcall(pmic_debugfs_init);
/*----------------pmic log debug machanism-----------------------*/
