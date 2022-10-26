// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/seq_file.h>

#include <mtk_lp_plat_apmcu_mbox.h>

#include "mtk_cpuidle_status.h"
#include "mtk_idle_procfs.h"


static int idle_proc_log_show(struct seq_file *m, void *v)
{
	seq_printf(m, "CPU idle log : %s\n",
		mtk_cpuidle_ctrl_log_sta_get() ?
		"Enable" : "Disable");

	seq_puts(m, "\n======== Command Usage ========\n");
	seq_puts(m, "echo [0|1] > /proc/cpuidle/control/log\n");

	return 0;
}

static ssize_t idle_proc_log_write(struct file *filp,
		const char __user *userbuf, size_t count, loff_t *f_pos)
{
	ssize_t ret = count;
	unsigned int enabled = 0;
	char *buf;

	mtk_idle_procfs_alloc_from_user(buf, userbuf, count);

	if (!buf)
		return -EINVAL;

	if (kstrtouint(buf, 10, &enabled) != 0) {
		ret = -EINVAL;
		goto free;
	}

	mtk_cpuidle_ctrl_log_en(!!enabled);

free:
	mtk_idle_procfs_free(buf);

	return ret;
}

static int idle_proc_timer_show(struct seq_file *m, void *v)
{
	seq_printf(m, "Limit sleep timer : %s\n",
		mtk_cpuidle_ctrl_timer_sta_get() ?
		"Enable" : "Disable");

	seq_puts(m, "\n======== Command Usage ========\n");
	seq_puts(m, "echo [0|1] > /proc/cpuidle/control/timer\n");

	return 0;
}

static ssize_t idle_proc_timer_write(struct file *filp,
		const char __user *userbuf, size_t count, loff_t *f_pos)
{
	ssize_t ret = count;
	unsigned int enabled = 0;
	char *buf;

	mtk_idle_procfs_alloc_from_user(buf, userbuf, count);

	if (!buf)
		return -EINVAL;

	if (kstrtouint(buf, 10, &enabled) != 0) {
		ret = -EINVAL;
		goto free;
	}

	mtk_cpuidle_ctrl_timer_en(!!enabled);

free:
	mtk_idle_procfs_free(buf);

	return ret;
}

static int idle_proc_stress_show(struct seq_file *m, void *v)
{
	seq_printf(m, "CPU idle stress : %s\n",
		mtk_cpuidle_get_stress_status() ?
		"Enable" : "Disable");

	seq_puts(m, "\n======== Command Usage ========\n");
	seq_puts(m, "echo [0|1] > /proc/cpuidle/control/stress\n");

	return 0;
}

static ssize_t idle_proc_stress_write(struct file *filp,
		const char __user *userbuf, size_t count, loff_t *f_pos)
{
	ssize_t ret = count;
	unsigned int enabled = 0;
	char *buf;

	mtk_idle_procfs_alloc_from_user(buf, userbuf, count);

	if (!buf)
		return -EINVAL;

	if (kstrtouint(buf, 10, &enabled) != 0) {
		ret = -EINVAL;
		goto free;
	}

	mtk_cpuidle_set_stress_test(!!enabled);

free:
	mtk_idle_procfs_free(buf);

	return ret;
}

static int idle_proc_stress_time_show(struct seq_file *m, void *v)
{
	seq_printf(m, "CPU idle stress interval time : %u\n",
		mtk_cpuidle_get_stress_time());


	seq_puts(m, "\n======== Command Usage ========\n");
	seq_puts(m, "echo [us] > /proc/cpuidle/control/stress_time\n");

	return 0;
}

static ssize_t idle_proc_stress_time_write(struct file *filp,
		const char __user *userbuf, size_t count, loff_t *f_pos)
{
	ssize_t ret = count;
	unsigned int interval_us = 0;
	char *buf;

	mtk_idle_procfs_alloc_from_user(buf, userbuf, count);

	if (!buf)
		return -EINVAL;

	if (kstrtouint(buf, 10, &interval_us) != 0) {
		ret = -EINVAL;
		goto free;
	}

	mtk_cpuidle_set_stress_time(interval_us);

free:
	mtk_idle_procfs_free(buf);

	return ret;
}

static int idle_proc_buck_mode_show(struct seq_file *m, void *v)
{
	int mode;
	struct mode_param {
		int id;
		char *str;
	};

	const struct mode_param table[] = {
		{MCUPM_BUCK_NORMAL_MODE, "on"},
		{MCUPM_BUCK_LP_MODE, "low power"},
		{MCUPM_BUCK_OFF_MODE, "off"},
		{NF_MCUPM_BUCK_MODE, "unknown"},
	};

	mode = mtk_get_mcupm_buck_mode();

	if (mode < 0 || mode > NF_MCUPM_BUCK_MODE)
		mode = NF_MCUPM_BUCK_MODE;

	seq_printf(m, "Vproc/Vproc_sram buck mode : %s\n",
		table[mode].str);


	seq_puts(m, "\n======== Command Usage ========\n");
	seq_puts(m, "echo [mode] > /proc/cpuidle/control/buck_mode\n");
	seq_puts(m, "mode:\n");

	for (mode = 0; mode < NF_MCUPM_BUCK_MODE; mode++)
		seq_printf(m, "\t%d: %s\n",
				table[mode].id,
				table[mode].str);

	return 0;
}

static ssize_t idle_proc_buck_mode_write(struct file *filp,
		const char __user *userbuf, size_t count, loff_t *f_pos)
{
	ssize_t ret = count;
	unsigned int mode = 0;
	char *buf;

	mtk_idle_procfs_alloc_from_user(buf, userbuf, count);

	if (!buf)
		return -EINVAL;

	if (kstrtouint(buf, 10, &mode) != 0) {
		ret = -EINVAL;
		goto free;
	}

	if (mode < NF_MCUPM_BUCK_MODE)
		mtk_set_mcupm_buck_mode(mode);

free:
	mtk_idle_procfs_free(buf);

	return ret;
}

static int idle_proc_armpll_mode_show(struct seq_file *m, void *v)
{
	int mode;
	struct mode_param {
		int id;
		char *str;
	};

	const struct mode_param table[] = {
		{MCUPM_ARMPLL_ON, "on"},
		{MCUPM_ARMPLL_GATING, "gating"},
		{MCUPM_ARMPLL_OFF, "off"},
		{NF_MCUPM_ARMPLL_MODE, "unknown"},
	};

	mode = mtk_get_mcupm_pll_mode();

	if (mode < 0 || mode > NF_MCUPM_ARMPLL_MODE)
		mode = NF_MCUPM_ARMPLL_MODE;

	seq_printf(m, "armpll mode : %s\n",
		table[mode].str);


	seq_puts(m, "\n======== Command Usage ========\n");
	seq_puts(m, "echo [mode] > /proc/cpuidle/control/armpll_mode\n");
	seq_puts(m, "mode:\n");

	for (mode = 0; mode < NF_MCUPM_ARMPLL_MODE; mode++)
		seq_printf(m, "\t%d: %s\n",
				table[mode].id,
				table[mode].str);

	return 0;
}

static ssize_t idle_proc_armpll_mode_write(struct file *filp,
		const char __user *userbuf, size_t count, loff_t *f_pos)
{
	ssize_t ret = count;
	unsigned int mode = 0;
	char *buf;

	mtk_idle_procfs_alloc_from_user(buf, userbuf, count);

	if (!buf)
		return -EINVAL;

	if (kstrtouint(buf, 10, &mode) != 0) {
		ret = -EINVAL;
		goto free;
	}

	if (mode < NF_MCUPM_ARMPLL_MODE)
		mtk_set_mcupm_pll_mode(mode);

free:
	mtk_idle_procfs_free(buf);

	return ret;
}

static int idle_proc_notify_cm_show(struct seq_file *m, void *v)
{
	bool notify;

	notify = mtk_mcupm_cm_is_notified();

	seq_printf(m, "mcupm cm mgr : %s\n",
		notify ? "Enable" : "Disable");

	seq_puts(m, "\n======== Command Usage ========\n");
	seq_puts(m, "Read Only : Not support dynamic control\n");
	return 0;
}

static ssize_t idle_proc_notify_cm_write(struct file *filp,
		const char __user *userbuf, size_t count, loff_t *f_pos)
{
	return count;
}

PROC_FOPS(log);
PROC_FOPS(timer);
PROC_FOPS(stress);
PROC_FOPS(stress_time);
PROC_FOPS(buck_mode);
PROC_FOPS(armpll_mode);
PROC_FOPS(notify_cm);
void __init mtk_idle_procfs_control_dir_init(struct proc_dir_entry *parent)
{
	int i;
	struct proc_dir_entry *dir = NULL;

	const struct idle_proc_entry entries[] = {
		PROC_ENTRY(log),
		PROC_ENTRY(timer),
		PROC_ENTRY(stress),
		PROC_ENTRY(stress_time),
		PROC_ENTRY(buck_mode),
		PROC_ENTRY(armpll_mode),
		PROC_ENTRY(notify_cm)
	};

	dir = proc_mkdir("control", parent);

	if (!dir) {
		pr_notice("fail to create procfs @ %s()\n", __func__);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++)
		PROC_CREATE_NODE(dir, entries[i]);
}
