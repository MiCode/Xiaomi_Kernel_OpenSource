/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (C) 2021 MediaTek Inc.
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <upmu_common.h>

static int rtc_alarm_enabled = 1;

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
			pmic_enable_interrupt(INT_RTC, 1, "RTC");
		else
			pmic_enable_interrupt(INT_RTC, 0, "RTC");
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

int __init rtc_debug_init(void)
{
	struct dentry *mtk_rtc_dir = NULL;
	struct dentry *mtk_rtc_file = NULL;

	mtk_rtc_dir = debugfs_create_dir("mtk_rtc", NULL);
	if (!mtk_rtc_dir) {
		pr_info("create /sys/kernel/debug/mtk_rtc_dir failed\n");
		return -ENOMEM;
	}

	mtk_rtc_file = debugfs_create_file("mtk_rtc", 0644,
				mtk_rtc_dir, NULL,
				&mtk_rtc_debug_ops);
	if (!mtk_rtc_file) {
		pr_info("create /sys/kernel/debug/mtk_rtc/mtk_rtc failed\n");
		return -ENOMEM;
	}

	return 0;
}

device_initcall(rtc_debug_init);
