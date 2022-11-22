// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include "ioctl_touch_boost.h"

#define TAG "IOCTL_TOUCH_BOOST"

void (*touch_boost_get_cmd_fp)(int *cmd, int *enable,
	int *deboost_when_render, int *active_time, int *boost_duration,
	int *idleprefer_ta, int *idleprefer_fg, int *util_ta, int *util_fg,
	int *cpufreq_c0, int *cpufreq_c1, int *cpufreq_c2);
EXPORT_SYMBOL_GPL(touch_boost_get_cmd_fp);

struct proc_dir_entry *perfmgr_root;

static unsigned long perfctl_copy_from_user(void *pvTo,
		const void __user *pvFrom, unsigned long ulBytes)
{
	if (access_ok(pvFrom, ulBytes))
		return __copy_from_user(pvTo, pvFrom, ulBytes);

	return ulBytes;
}

static unsigned long perfctl_copy_to_user(void __user *pvTo,
		const void *pvFrom, unsigned long ulBytes)
{
	if (access_ok(pvTo, ulBytes))
		return __copy_to_user(pvTo, pvFrom, ulBytes);

	return ulBytes;
}

static int device_show(struct seq_file *m, void *v)
{
	return 0;
}

static int device_open(struct inode *inode, struct file *file)
{
	return single_open(file, device_show, inode->i_private);
}

static long device_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	ssize_t ret = 0;
	int _cmd = -1;
	int enable = -1;
	int deboost_when_render = -1, active_time = -1, boost_duration = -1;
	int cpufreq_c0 = -1, cpufreq_c1 = -1, cpufreq_c2 = -1;
	int idleprefer_ta = -1, idleprefer_fg = -1, util_ta = -1, util_fg = -1;

	struct _TOUCH_BOOST_PACKAGE *t_msgKM = NULL,
			*t_msgUM = (struct _TOUCH_BOOST_PACKAGE *)arg;
	struct _TOUCH_BOOST_PACKAGE t_smsgKM;

	t_msgKM = &t_smsgKM;

	if (perfctl_copy_from_user(t_msgKM, t_msgUM,
				sizeof(struct _TOUCH_BOOST_PACKAGE))) {
		ret = -EFAULT;
		goto ret_ioctl;
	}

	switch (cmd) {
	case TOUCH_BOOST_GET_CMD:
		if (touch_boost_get_cmd_fp) {
			touch_boost_get_cmd_fp(&_cmd,
			&enable, &deboost_when_render,
			&active_time, &boost_duration,
			&idleprefer_ta, &idleprefer_fg,
			&util_ta, &util_fg,
			&cpufreq_c0, &cpufreq_c1, &cpufreq_c2);
			t_msgKM->cmd = _cmd;
			t_msgKM->enable = enable;
			t_msgKM->deboost_when_render = deboost_when_render;
			t_msgKM->active_time = active_time;
			t_msgKM->boost_duration = boost_duration;
			t_msgKM->idleprefer_ta = idleprefer_ta;
			t_msgKM->idleprefer_fg = idleprefer_fg;
			t_msgKM->util_ta = util_ta;
			t_msgKM->util_fg = util_fg;
			t_msgKM->cpufreq_c0 = cpufreq_c0;
			t_msgKM->cpufreq_c1 = cpufreq_c1;
			t_msgKM->cpufreq_c2 = cpufreq_c2;
		} else
			ret = -1;

		perfctl_copy_to_user(t_msgUM, t_msgKM,
			sizeof(struct _TOUCH_BOOST_PACKAGE));
		break;
	default:
		pr_debug(TAG "%s %d: unknown cmd %x\n", __FILE__, __LINE__, cmd);
		ret = -1;
		goto ret_ioctl;
	}

ret_ioctl:
	return ret;
}

static const struct proc_ops Fops = {
	.proc_compat_ioctl = device_ioctl,
	.proc_ioctl = device_ioctl,
	.proc_open = device_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static void __exit exit_perfctl(void) {}
static int __init init_perfctl(void)
{
	struct proc_dir_entry *pe, *parent;
	int ret_val = 0;

	pr_debug(TAG"START to init ioctl_touch_boost driver\n");

	parent = proc_mkdir("perfmgr_touch_boost", NULL);
	perfmgr_root = parent;

	pe = proc_create("ioctl_touch_boost", 0664, parent, &Fops);
	if (!pe) {
		pr_debug(TAG"%s failed with %d\n",
				"Creating file node ",
				ret_val);
		ret_val = -ENOMEM;
		goto out_wq;
	}

	pr_debug(TAG"FINISH init ioctl_touch_boost driver\n");

	return 0;

out_wq:
	return ret_val;
}

module_init(init_perfctl);
module_exit(exit_perfctl);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek TOUCH_BOOST perf_ioctl");
MODULE_AUTHOR("MediaTek Inc.");
