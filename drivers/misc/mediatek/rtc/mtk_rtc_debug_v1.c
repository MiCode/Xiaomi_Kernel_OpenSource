// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/proc_fs.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>



static int rtc_alarm_enabled = 1;
int irq;

static ssize_t mtk_rtc_debug_write(struct file *file,
	const char __user *buf, size_t size, loff_t *ppos)
{
	char lbuf[128];
	char option[16];
	int setting;
	ssize_t res;


	if (*ppos != 0 || size >= sizeof(lbuf) || size == 0)
		return -EINVAL;

	res = simple_write_to_buffer(lbuf, sizeof(lbuf) - 1, ppos, buf, size);
	if (res <= 0)
		return -EFAULT;
	lbuf[size] = '\0';

	if (sscanf(lbuf, "%15s %d", option, &setting) != 2) {
		pr_notice("Invalid para %s\n", lbuf);
		return -EFAULT;
	}

	if (!strncmp(option, "alarm", strlen("alarm"))) {
		pr_notice("alarm = %d\n", setting);
		rtc_alarm_enabled = setting;
		if (rtc_alarm_enabled)
			enable_irq(irq);
		else
			disable_irq_nosync(irq);
	}

	return size;
}

static int mtk_rtc_debug_show(struct seq_file *s, void *unused)
{
	seq_printf(s, "rtc alarm %s\n",
		rtc_alarm_enabled ? "enabled" : "disabled");
	return 0;
}

static int mtk_rtc_debug_open(struct inode *inode,
	struct file *file)
{
	return single_open(file, mtk_rtc_debug_show, NULL);
}

static const struct file_operations mtk_rtc_debug_ops = {
	.open    = mtk_rtc_debug_open,
	.read    = seq_read,
	.write   = mtk_rtc_debug_write,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int mtk_rtc_dbg_probe(struct platform_device *pdev)
{
	struct dentry *mtk_rtc_dir;
	struct dentry *mtk_rtc_file;
	struct proc_dir_entry *pdir, *pentry;

	irq = platform_get_irq(to_platform_device(pdev->dev.parent), 0);
	if (irq <= 0)
		return -EINVAL;

	dev_notice(&pdev->dev, " irq = %d\n", irq);

	mtk_rtc_dir = debugfs_create_dir("mtk_rtc", NULL);
	if (!mtk_rtc_dir) {
		dev_err(&pdev->dev,
			"create /sys/kernel/debug/mtk_rtc_dir failed\n");
	} else {
		mtk_rtc_file = debugfs_create_file("mtk_rtc", 0644,
			mtk_rtc_dir, NULL,
			&mtk_rtc_debug_ops);
		if (!mtk_rtc_file) {
			dev_err(&pdev->dev,
			"create /sys/kernel/debug/mtk_rtc/mtk_rtc failed\n");
		}
	}

	pdir = proc_mkdir("mtk_rtc", NULL);
	if (!pdir) {
		dev_notice(&pdev->dev,
			"create /proc/mtk_rtc_dir failed\n");
	} else {
		pentry = proc_create_data("mtk_rtc", 0644,
			pdir, &mtk_rtc_debug_ops, NULL);
		if (!pentry) {
			dev_notice(&pdev->dev,
				"create /proc/mtk_rtc/mtk_rtc failed\n");
		}
	}

	return 0;
}

static struct platform_driver mtk_rtc_dbg_driver = {
	.driver = {
		.name = "mtk_rtc_dbg",
	},
	.probe	= mtk_rtc_dbg_probe,
};

module_platform_driver(mtk_rtc_dbg_driver);

