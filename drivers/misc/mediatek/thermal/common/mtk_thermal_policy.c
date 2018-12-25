/*
 * Copyright (C) 2018 MediaTek Inc.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/kobject.h>
#include "mt-plat/mtk_thermal_monitor.h"

#define MTK_THERMAL_POLICY_SIGNAL

#if defined(MTK_THERMAL_POLICY_SIGNAL)
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/pid.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/uidgid.h>

#define MAX_LEN	256
#endif

#if 1
#define mtk_thermal_policy_dprintk(fmt, args...)	\
	pr_notice("thermal/cooler/shutdown " fmt, ##args)
#else
#define mtk_thermal_policy_dprintk(fmt, args...)
#endif

#if defined(MTK_THERMAL_POLICY_SIGNAL)

static unsigned int tm_pid;
static unsigned int tm_input_pid;
static int current_tp_index;
static struct task_struct *pg_task;
static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);

static ssize_t _mtk_tp_pid_write
(struct file *filp, const char __user *buf, size_t len, loff_t *data)
{
	int ret = 0;
	char tmp[MAX_LEN] = { 0 };

	len = (len < (MAX_LEN - 1)) ? len : (MAX_LEN - 1);
	/* write data to the buffer */
	if (copy_from_user(tmp, buf, len))
		return -EFAULT;

	ret = kstrtouint(tmp, 10, &tm_input_pid);
	if (ret)
		WARN_ON_ONCE(1);

	mtk_thermal_policy_dprintk("%s %s = %d\n", __func__, tmp,
			tm_input_pid);

	return len;
}

static int _mtk_tp_pid_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", tm_input_pid);
	mtk_thermal_policy_dprintk("%s %d\n", __func__, tm_input_pid);

	return 0;
}

static int _mtk_tp_pid_open(struct inode *inode, struct file *file)
{
	return single_open(file, _mtk_tp_pid_read, PDE_DATA(inode));
}

static const struct file_operations _tp_pid_fops = {
	.owner = THIS_MODULE,
	.open = _mtk_tp_pid_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = _mtk_tp_pid_write,
	.release = single_release,
};

static int _mtk_cl_sd_send_signal(void)
{
	int ret = 0;

	if (tm_input_pid == 0) {
		mtk_thermal_policy_dprintk("%s pid is empty\n", __func__);
		ret = -1;
	}

	mtk_thermal_policy_dprintk("%s pid is %d, %d\n", __func__,
			tm_pid, tm_input_pid);

	if (ret == 0 && tm_input_pid != tm_pid) {
		tm_pid = tm_input_pid;

		if (pg_task != NULL)
			put_task_struct(pg_task);
		pg_task = get_pid_task(find_vpid(tm_pid), PIDTYPE_PID);
	}

	if (ret == 0 && pg_task) {
		siginfo_t info;

		info.si_signo = SIGIO;
		info.si_errno = current_tp_index;
		info.si_code = 4;
		info.si_addr = NULL;
		ret = send_sig_info(SIGIO, &info, pg_task);
	}

	if (ret != 0)
		mtk_thermal_policy_dprintk("%s ret=%d\n", __func__, ret);

	return ret;
}
#endif

int mtk_change_thermal_policy(int tp_index)
{
	int ret = 0;

	current_tp_index = tp_index;
#if defined(MTK_THERMAL_POLICY_SIGNAL)
	ret = _mtk_cl_sd_send_signal();
#else
	mtk_thermal_policy_dprintk("[%s] Skip changing a thermal policy\n",
		 __func__);
#endif
	return ret;
}

static int __init mtk_thermal_policy_init(void)
{
	mtk_thermal_policy_dprintk("init\n");

#if defined(MTK_THERMAL_POLICY_SIGNAL)
	{
		struct proc_dir_entry *entry = NULL;
		struct proc_dir_entry *dir_entry
			= mtk_thermal_get_proc_drv_therm_dir_entry();

		if (!dir_entry) {
			mtk_thermal_policy_dprintk(
					"%s mkdir /proc/driver/thermal failed\n",
					__func__);
			return -1;
		}

		entry =
			proc_create("tp_pid", 0664,
					dir_entry,
					&_tp_pid_fops);
		if (!entry)
			mtk_thermal_policy_dprintk(
				"%s tp_pid creation failed\n", __func__);
		else
			proc_set_user(entry, uid, gid);
	}
#endif
	return 0;
}

static void __exit mtk_thermal_policy_exit(void)
{
	mtk_thermal_policy_dprintk("exit\n");
}
module_init(mtk_thermal_policy_init);
module_exit(mtk_thermal_policy_exit);
