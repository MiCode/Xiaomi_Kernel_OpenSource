/* SPDX-License-Identifier: GPL-2.0 */
/*
 * cpufreq-dbg-lite.c - eem debug driver
 *
 * Copyright (c) 2020 MediaTek Inc.
 * Tungchen Shih <tungchen.shih@mediatek.com>
 */
#define PROC_FOPS_RW(name)\
static int name ## _proc_open(struct inode *inode, struct file *file)\
{\
	return single_open(file, name ## _proc_show, PDE_DATA(inode));\
} \
static const struct proc_ops name ## _proc_fops = {\
	.proc_open           = name ## _proc_open,\
	.proc_read           = seq_read,\
	.proc_lseek          = seq_lseek,\
	.proc_release        = single_release,\
	.proc_write          = name ## _proc_write,\
}

#define PROC_FOPS_RO(name)\
static int name##_proc_open(struct inode *inode, struct file *file)\
{\
	return single_open(file, name##_proc_show, PDE_DATA(inode));\
} \
static const struct proc_ops name##_proc_fops = {\
	.proc_open = name##_proc_open,\
	.proc_read = seq_read,\
	.proc_lseek = seq_lseek,\
	.proc_release = single_release,\
}

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}
#define PROC_ENTRY_DATA(name)	\
{__stringify(name), &name ## _proc_fops, g_ ## name}

extern int mtk_eem_init(struct platform_device *pdev);
extern int mtk_devinfo_init(struct platform_device *pdev);
