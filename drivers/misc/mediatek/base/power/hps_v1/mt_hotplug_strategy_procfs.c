// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

/*
 * @file    mt_hotplug_strategy_procfs.c
 * @brief   hotplug strategy(hps) - procfs
 */

#include <linux/kernel.h>
#include <linux/module.h>	/* MODULE_DESCRIPTION, MODULE_LICENSE */
#include <linux/init.h>		/* module_init, module_exit */
#include <linux/proc_fs.h>	/* proc_mkdir, proc_create */
#include <linux/seq_file.h>	/* seq_printf, single_open */
#include <linux/uaccess.h>	/* copy_from_user */

#include "mt_hotplug_strategy_internal.h"

typedef void (*func_void)(void);

static int hps_proc_uint_show(struct seq_file *m, void *v)
{
	unsigned int *pv = (unsigned int *)m->private;

	seq_printf(m, "%u\n", *pv);
	return 0;
}

static ssize_t hps_proc_uint_write(
		struct file *file, const char __user *buffer,
		size_t count, loff_t *pos,
		func_void before_write, func_void after_write)
{
	int len = 0;
	char desc[32];
	unsigned int var;
	unsigned int *pv;

	pv = (unsigned int *)((struct seq_file *)file->private_data)->private;

	len = min(count, sizeof(desc) - 1);
	memset(desc, 0, sizeof(desc));
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (!kstrtouint(desc, 0, &var)) {
		if (before_write)
			before_write();

		*pv = var;

		if (after_write)
			after_write();

		return count;
	}

	hps_warn("%s(): bad argument\n", __func__);

	return -EINVAL;
}

static void lock_hps_ctxt(void)
{
	mutex_lock(&hps_ctxt.lock);
}

static void unlock_hps_ctxt(void)
{
	mutex_unlock(&hps_ctxt.lock);
}

static void reset_unlock_hps_ctxt(void)
{
	hps_ctxt_reset_stas_nolock();
	mutex_unlock(&hps_ctxt.lock);
}

static ssize_t hps_proc_uint_write_with_lock(
		struct file *file, const char __user *buffer,
		size_t count, loff_t *pos)
{
	return hps_proc_uint_write(file, buffer, count, pos,
			lock_hps_ctxt, unlock_hps_ctxt);
}

static ssize_t hps_proc_uint_write_with_lock_reset(
		struct file *file, const char __user *buffer,
		size_t count, loff_t *pos)
{
	return hps_proc_uint_write(file, buffer, count, pos,
			lock_hps_ctxt, reset_unlock_hps_ctxt);
}

#define RPOC_FOPS_OPEN_WRITE(_name, _open, _write)			\
	static const struct file_operations _name = {			\
		.owner		= THIS_MODULE,				\
		.open		= _open,				\
		.read		= seq_read,				\
		.llseek		= seq_lseek,				\
		.release	= single_release,			\
		.write		= _write,				\
	}

#define PROC_FOPS_RW_UINT(name, var, _write)				\
	static int hps_##name##_proc_open(				\
		struct inode *inode, struct file *file)			\
	{								\
		return single_open(file, hps_proc_uint_show, &(var));	\
	}								\
	RPOC_FOPS_OPEN_WRITE(						\
		hps_##name##_proc_fops, hps_##name##_proc_open, _write)

#define PROC_FOPS_RO_UINT(name, var)	PROC_FOPS_RW_UINT(name, var, NULL)

#define PROC_FOPS_RW(name)						\
	static int hps_##name##_proc_open(				\
		struct inode *inode, struct file *file)			\
	{								\
		return single_open(file, hps_##name##_proc_show,	\
				PDE_DATA(inode));			\
	}								\
	RPOC_FOPS_OPEN_WRITE(						\
		hps_##name##_proc_fops,					\
		hps_##name##_proc_open,					\
		hps_##name##_proc_write)

#define PROC_ENTRY(name)	{__stringify(name), &hps_##name##_proc_fops}

/***********************************************************
 * procfs callback - state series
 *                     - init_state
 ***********************************************************/
PROC_FOPS_RO_UINT(init_state, hps_ctxt.init_state);

/***********************************************************
 * procfs callback - enabled series
 *                     - enabled
 *                     - log_mask
 ***********************************************************/
PROC_FOPS_RW_UINT(
	enabled,
	hps_ctxt.enabled,
	hps_proc_uint_write_with_lock_reset);
PROC_FOPS_RW_UINT(
	log_mask,
	hps_ctxt.log_mask,
	hps_proc_uint_write_with_lock);

/***********************************************************
 * procfs callback - algo config series
 *                     - up_threshold
 *                     - up_times
 *                     - down_threshold
 *                     - down_times
 *                     - rush_boost_enabled
 *                     - rush_boost_threshold
 *                     - rush_boost_times
 *                     - quick_landing_enabled
 *                     - tlp_times
 ***********************************************************/
PROC_FOPS_RW_UINT(
	up_threshold,
	hps_ctxt.up_threshold,
	hps_proc_uint_write_with_lock_reset);
PROC_FOPS_RW_UINT(
	up_times,
	hps_ctxt.up_times,
	hps_proc_uint_write_with_lock_reset);
PROC_FOPS_RW_UINT(
	down_threshold,
	hps_ctxt.down_threshold,
	hps_proc_uint_write_with_lock_reset);
PROC_FOPS_RW_UINT(
	down_times,
	hps_ctxt.down_times,
	hps_proc_uint_write_with_lock_reset);
PROC_FOPS_RW_UINT(
	rush_boost_enabled,
	hps_ctxt.rush_boost_enabled,
	hps_proc_uint_write_with_lock);
PROC_FOPS_RW_UINT(
	rush_boost_threshold,
	hps_ctxt.rush_boost_threshold,
	hps_proc_uint_write_with_lock_reset);
PROC_FOPS_RW_UINT(
	rush_boost_times,
	hps_ctxt.rush_boost_times,
	hps_proc_uint_write_with_lock_reset);
PROC_FOPS_RW_UINT(
	quick_landing_enabled,
	hps_ctxt.quick_landing_enabled,
	hps_proc_uint_write_with_lock);
PROC_FOPS_RW_UINT(
	tlp_times,
	hps_ctxt.tlp_times,
	hps_proc_uint_write_with_lock_reset);

/***********************************************************
 * procfs callback - algo bound series
 *                     - little_num_base_perf_serv
 *                     - big_num_base_perf_serv
 *                     - little_num_base_custom1
 *                     - big_num_base_custom1
 *                     - little_num_base_custom2
 *                     - big_num_base_custom2
 ***********************************************************/
static int hps_num_base_proc_show(unsigned int nbl, unsigned int nbb,
				struct seq_file *m, void *v)
{
	if (hps_ctxt.is_hmp)
		seq_printf(m, "%u %u\n", nbl, nbb);
	else
		seq_printf(m, "%u\n", nbl);

	return 0;
}

static ssize_t hps_num_base_proc_write(
		unsigned int *pnbl,
		unsigned int *pnbb,
		struct file *file,
		const char __user *buffer,
		size_t count,
		loff_t *pos)
{
	int len = 0, nbl = 0, nbb = 0;
	unsigned int nll, nlb;
	char desc[32];
	unsigned int lo, bo;

	len = min(count, sizeof(desc) - 1);
	memset(desc, 0, sizeof(desc));
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (hps_ctxt.is_hmp &&
		(sscanf(desc, "%u %u", &nbl, &nbb) == 2)) {
		if (nbl > num_possible_little_cpus() || nbl < 1) {
			hps_warn("%s(): bad argument(%u, %u)\n",
				__func__, nbl, nbb);
			return -EINVAL;
		}

		if (nbb > num_possible_big_cpus()) {
			hps_warn("%s(): bad argument(%u, %u)\n",
				__func__, nbl, nbb);
			return -EINVAL;
		}

		mutex_lock(&hps_ctxt.lock);

		*pnbl = nbl;
		*pnbb = nbb;

		nll = num_limit_little_cpus();
		nlb = num_limit_big_cpus();
		lo = num_online_little_cpus();
		bo = num_online_big_cpus();

		if ((bo < nbb && bo < nlb) || (lo < nbl && lo < nll))
			hps_task_wakeup_nolock();

		mutex_unlock(&hps_ctxt.lock);

		return count;
	} else if (!hps_ctxt.is_hmp &&
			!kstrtouint(desc, 0, &nbl)) {
		if (nbl > num_possible_little_cpus() || nbl < 1) {
			hps_warn("%s(): bad argument(%u)\n",
				__func__, nbl);
			return -EINVAL;
		}

		mutex_lock(&hps_ctxt.lock);

		*pnbl = nbl;

		nll = num_limit_little_cpus();
		lo = num_online_little_cpus();

		if (lo < nbl && lo < nll)
			hps_task_wakeup_nolock();

		mutex_unlock(&hps_ctxt.lock);

		return count;
	}

	hps_warn("%s(): bad argument\n", __func__);

	return -EINVAL;
}

static int hps_num_base_perf_serv_proc_show(struct seq_file *m, void *v)
{
	return hps_num_base_proc_show(
			hps_ctxt.little_num_base_perf_serv,
			hps_ctxt.big_num_base_perf_serv,
			m, v);
}

static ssize_t hps_num_base_perf_serv_proc_write(
		struct file *file,
		const char __user *buffer,
		size_t count,
		loff_t *pos)
{
	return hps_num_base_proc_write(
			&hps_ctxt.little_num_base_perf_serv,
			&hps_ctxt.big_num_base_perf_serv,
			file, buffer, count, pos);
}

static int hps_num_base_custom1_proc_show(struct seq_file *m, void *v)
{
	return hps_num_base_proc_show(
			hps_ctxt.little_num_base_custom1,
			hps_ctxt.big_num_base_custom1,
			m, v);
}

static ssize_t hps_num_base_custom1_proc_write(
		struct file *file,
		const char __user *buffer,
		size_t count,
		loff_t *pos)
{
	return hps_num_base_proc_write(
			&hps_ctxt.little_num_base_custom1,
			&hps_ctxt.big_num_base_custom1,
			file, buffer, count, pos);
}

static int hps_num_base_custom2_proc_show(struct seq_file *m, void *v)
{
	return hps_num_base_proc_show(
			hps_ctxt.little_num_base_custom2,
			hps_ctxt.big_num_base_custom2,
			m, v);
}

static ssize_t hps_num_base_custom2_proc_write(
		struct file *file,
		const char __user *buffer,
		size_t count,
		loff_t *pos)
{
	return hps_num_base_proc_write(
			&hps_ctxt.little_num_base_custom2,
			&hps_ctxt.big_num_base_custom2,
			file, buffer, count, pos);
}

PROC_FOPS_RW(num_base_perf_serv);
PROC_FOPS_RW(num_base_custom1);
PROC_FOPS_RW(num_base_custom2);

/***********************************************************
 * common read/write for num_limit
 ***********************************************************/
static int hps_num_limit_proc_show(unsigned int nll, unsigned int nlb,
				struct seq_file *m, void *v)
{
	if (hps_ctxt.is_hmp)
		seq_printf(m, "%u %u\n", nll, nlb);
	else
		seq_printf(m, "%u\n", nll);

	return 0;
}

static ssize_t hps_num_limit_proc_write(
		unsigned int *pnll,
		unsigned int *pnlb,
		struct file *file,
		const char __user *buffer,
		size_t count,
		loff_t *pos)
{
	int len = 0, nll = 0, nlb = 0;
	char desc[32];

	len = min(count, sizeof(desc) - 1);
	memset(desc, 0, sizeof(desc));
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (hps_ctxt.is_hmp &&
		(sscanf(desc, "%u %u", &nll, &nlb) == 2)) {
		if (nll > num_possible_little_cpus() || nll < 1) {
			hps_warn("%s(): bad argument(%u, %u)\n",
				__func__, nll, nlb);
			return -EINVAL;
		}

		if (nlb > num_possible_big_cpus()) {
			hps_warn("%s(): bad argument(%u, %u)\n",
				__func__, nll, nlb);
			return -EINVAL;
		}

		mutex_lock(&hps_ctxt.lock);

		*pnll = nll;
		*pnlb = nlb;

		if (num_online_big_cpus() > nlb)
			hps_task_wakeup_nolock();
		else if (num_online_little_cpus() > nll)
			hps_task_wakeup_nolock();

		mutex_unlock(&hps_ctxt.lock);

		return count;
	} else if (!hps_ctxt.is_hmp &&
			!kstrtouint(desc, 0, &nll)) {
		if (nll > num_possible_little_cpus() || nll < 1) {
			hps_warn("%s(): bad argument(%u)\n",
				__func__, nll);
			return -EINVAL;
		}

		mutex_lock(&hps_ctxt.lock);

		*pnll = nll;

		if (num_online_little_cpus() > nll)
			hps_task_wakeup_nolock();

		mutex_unlock(&hps_ctxt.lock);

		return count;
	}

	hps_warn("%s(): bad argument\n", __func__);

	return -EINVAL;
}

/***********************************************************
 * procfs callback - algo bound series
 *                     - little_num_limit_thermal
 *                     - big_num_limit_thermal
 ***********************************************************/
static int hps_num_limit_thermal_proc_show(struct seq_file *m, void *v)
{
	return hps_num_limit_proc_show(
			hps_ctxt.little_num_limit_thermal,
			hps_ctxt.big_num_limit_thermal,
			m, v);
}

static ssize_t hps_num_limit_thermal_proc_write(
		struct file *file,
		const char __user *buffer,
		size_t count,
		loff_t *pos)
{
	return hps_num_limit_proc_write(
			&hps_ctxt.little_num_limit_thermal,
			&hps_ctxt.big_num_limit_thermal,
			file, buffer, count, pos);
}

PROC_FOPS_RW(num_limit_thermal);

/***********************************************************
 * procfs callback - algo bound series
 *                     - little_num_limit_low_battery
 *                     - big_num_limit_low_battery
 ***********************************************************/
static int hps_num_limit_low_battery_proc_show(struct seq_file *m, void *v)
{
	return hps_num_limit_proc_show(
			hps_ctxt.little_num_limit_low_battery,
			hps_ctxt.big_num_limit_low_battery,
			m, v);
}

static ssize_t hps_num_limit_low_battery_proc_write(
		struct file *file,
		const char __user *buffer,
		size_t count,
		loff_t *pos)
{
	return hps_num_limit_proc_write(
			&hps_ctxt.little_num_limit_low_battery,
			&hps_ctxt.big_num_limit_low_battery,
			file, buffer, count, pos);
}

PROC_FOPS_RW(num_limit_low_battery);

/***********************************************************
 * procfs callback - algo bound series
 *                     - little_num_limit_ultra_power_saving
 *                     - big_num_limit_ultra_power_saving
 ***********************************************************/
static int hps_num_limit_ultra_power_saving_proc_show(
			struct seq_file *m, void *v)
{
	return hps_num_limit_proc_show(
			hps_ctxt.little_num_limit_ultra_power_saving,
			hps_ctxt.big_num_limit_ultra_power_saving,
			m, v);
}

static ssize_t hps_num_limit_ultra_power_saving_proc_write(
		struct file *file,
		const char __user *buffer,
		size_t count,
		loff_t *pos)
{
	return hps_num_limit_proc_write(
			&hps_ctxt.little_num_limit_ultra_power_saving,
			&hps_ctxt.big_num_limit_ultra_power_saving,
			file, buffer, count, pos);
}

PROC_FOPS_RW(num_limit_ultra_power_saving);

/***********************************************************
 * procfs callback - algo bound series
 *                     - little_num_limit_power_serv
 *                     - big_num_limit_power_serv
 ***********************************************************/
static int hps_num_limit_power_serv_proc_show(struct seq_file *m, void *v)
{
	return hps_num_limit_proc_show(
			hps_ctxt.little_num_limit_power_serv,
			hps_ctxt.big_num_limit_power_serv,
			m, v);
}

static ssize_t hps_num_limit_power_serv_proc_write(
		struct file *file,
		const char __user *buffer,
		size_t count,
		loff_t *pos)
{
	return hps_num_limit_proc_write(
			&hps_ctxt.little_num_limit_power_serv,
			&hps_ctxt.big_num_limit_power_serv,
			file, buffer, count, pos);
}

PROC_FOPS_RW(num_limit_power_serv);

/***********************************************************
 * procfs callback - algo bound series
 *                     - little_num_limit_custom1
 *                     - big_num_limit_custom1
 *                     - little_num_limit_custom2
 *                     - big_num_limit_custom2
 ***********************************************************/
static int hps_num_limit_custom1_proc_show(struct seq_file *m, void *v)
{
	return hps_num_limit_proc_show(
			hps_ctxt.little_num_limit_custom1,
			hps_ctxt.big_num_limit_custom1,
			m, v);
}

static ssize_t hps_num_limit_custom1_proc_write(
		struct file *file,
		const char __user *buffer,
		size_t count,
		loff_t *pos)
{
	return hps_num_limit_proc_write(
			&hps_ctxt.little_num_limit_custom1,
			&hps_ctxt.big_num_limit_custom1,
			file, buffer, count, pos);
}

static int hps_num_limit_custom2_proc_show(struct seq_file *m, void *v)
{
	return hps_num_limit_proc_show(
			hps_ctxt.little_num_limit_custom2,
			hps_ctxt.big_num_limit_custom2,
			m, v);
}

static ssize_t hps_num_limit_custom2_proc_write(
		struct file *file,
		const char __user *buffer,
		size_t count,
		loff_t *pos)
{
	return hps_num_limit_proc_write(
			&hps_ctxt.little_num_limit_custom2,
			&hps_ctxt.big_num_limit_custom2,
			file, buffer, count, pos);
}

PROC_FOPS_RW(num_limit_custom1);
PROC_FOPS_RW(num_limit_custom2);

/*
 * init
 */
int hps_procfs_init(void)
{
	/* struct proc_dir_entry *entry = NULL; */
	struct proc_dir_entry *hps_dir = NULL;
	int r = 0;
	int i;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(init_state),
		PROC_ENTRY(enabled),
		PROC_ENTRY(log_mask),
		PROC_ENTRY(up_threshold),
		PROC_ENTRY(up_times),
		PROC_ENTRY(down_threshold),
		PROC_ENTRY(down_times),
		PROC_ENTRY(rush_boost_enabled),
		PROC_ENTRY(rush_boost_threshold),
		PROC_ENTRY(rush_boost_times),
		PROC_ENTRY(quick_landing_enabled),
		PROC_ENTRY(tlp_times),
		PROC_ENTRY(num_base_perf_serv),
		PROC_ENTRY(num_base_custom1),
		PROC_ENTRY(num_base_custom2),
		PROC_ENTRY(num_limit_custom1),
		PROC_ENTRY(num_limit_custom2),
		PROC_ENTRY(num_limit_thermal),
		PROC_ENTRY(num_limit_low_battery),
		PROC_ENTRY(num_limit_ultra_power_saving),
		PROC_ENTRY(num_limit_power_serv),
	};

	log_info("%s\n", __func__);

	hps_dir = proc_mkdir("hps", NULL);
	if (hps_dir == NULL) {
		hps_err("mkdir /proc/hps fail\n");
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name,
			0664,
			hps_dir, entries[i].fops))
			hps_err("create /proc/hps/%s failed\n",
				entries[i].name);
	}

	return r;
}
