/*
 * Copyright (C) 2017 MediaTek Inc.
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

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kallsyms.h>
#include <linux/utsname.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/uaccess.h>
#include <linux/printk.h>
#include <linux/string.h>

#include <linux/platform_device.h>

#include <mtk_vcorefs_governor.h>
#include <mtk_vcorefs_manager.h>

#define SEQ_printf(m, x...)\
	do {\
		if (m)\
			seq_printf(m, x);\
		else\
			pr_debug(x);\
	} while (0)
#undef TAG
#define TAG "[SOC DVFS FLIPER]"

/* for debug */
static int fliper_debug;
/* KIR_PERF */
static int perf_now;



static int mt_fliper_show(struct seq_file *m, void *v)
{
	SEQ_printf(m, "KIR_PERF: %d\n", perf_now);
	SEQ_printf(m, "DEBUG: %d\n", fliper_debug);
	return 0;
}
/*** Seq operation of mtprof ****/
static int mt_fliper_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_fliper_show, inode->i_private);
}


static const struct file_operations mt_fliper_fops = {
	.open = mt_fliper_open,
	/*.write = mt_fliper_write,*/
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


static ssize_t mt_perf_write(struct file *filp, const char *ubuf,
		size_t cnt, loff_t *data)
{
	char buf[64];
	int val;
	int ret;

	if (fliper_debug)
		return -1;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;
	buf[cnt] = 0;
	ret = kstrtoint(buf, 10, &val);
	if (ret < 0)
		return ret;

	if (val < -1 || val > 4)
		return -1;

	if (vcorefs_request_dvfs_opp(KIR_PERF, val) < 0)
		return cnt;

	perf_now = val;

	return cnt;
}

static int mt_perf_show(struct seq_file *m, void *v)
{
	SEQ_printf(m, "%d\n", perf_now);
	return 0;
}
/*** Seq operation of mtprof ****/
static int mt_perf_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_perf_show, inode->i_private);
}

static const struct file_operations mt_perf_fops = {
	.open = mt_perf_open,
	.write = mt_perf_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


/*--------------------INIT------------------------*/

static int __init init_fliper(void)
{
	struct proc_dir_entry *fliperfs_dir;
	struct proc_dir_entry *perf_dir;

	pr_debug(TAG"init fliper driver start\n");
	fliperfs_dir = proc_mkdir("fliperfs", NULL);
	if (!fliperfs_dir)
		return -ENOMEM;

	perf_dir = proc_create("perf", 0644, fliperfs_dir, &mt_perf_fops);
	if (!perf_dir)
		return -ENOMEM;


	perf_now = -1;
	fliper_debug = 0;


	pr_debug(TAG"init fliper driver done\n");

	return 0;
}
late_initcall(init_fliper);

