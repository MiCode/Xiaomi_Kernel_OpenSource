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

/*
 * @file    mt_hotplug_strategy_procfs.c
 * @brief   hotplug strategy(hps) - procfs
 */

#include <linux/kernel.h>	/* printk */
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
*                     - state
***********************************************************/
PROC_FOPS_RO_UINT(init_state, hps_ctxt.init_state);
PROC_FOPS_RO_UINT(state, hps_ctxt.state);

/***********************************************************
* procfs callback - enabled series
*                     - enabled
*                     - early_suspend_enabled
*                     - suspend_enabled
*                     - log_mask
***********************************************************/
PROC_FOPS_RW_UINT(
	enabled,
	hps_ctxt.enabled,
	hps_proc_uint_write_with_lock_reset);
PROC_FOPS_RW_UINT(
	early_suspend_enabled,
	hps_ctxt.early_suspend_enabled,
	hps_proc_uint_write_with_lock);
PROC_FOPS_RW_UINT(
	suspend_enabled,
	hps_ctxt.suspend_enabled,
	hps_proc_uint_write_with_lock);
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
*                     - input_boost_enabled
*                     - input_boost_cpu_num
*                     - rush_boost_enabled
*                     - rush_boost_threshold
*                     - rush_boost_times
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
	input_boost_enabled,
	hps_ctxt.input_boost_enabled,
	hps_proc_uint_write_with_lock);
PROC_FOPS_RW_UINT(
	input_boost_cpu_num,
	hps_ctxt.input_boost_cpu_num,
	hps_proc_uint_write_with_lock);
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
	tlp_times,
	hps_ctxt.tlp_times,
	hps_proc_uint_write_with_lock_reset);

/***********************************************************
* procfs callback - algo bound series
*                     - little_num_base_perf_serv
*                     - big_num_base_perf_serv
***********************************************************/
static int hps_num_base_perf_serv_proc_show(struct seq_file *m, void *v)
{
	if (hps_ctxt.is_hmp)
		seq_printf(m, "%u %u\n", hps_ctxt.little_num_base_perf_serv,
			hps_ctxt.big_num_base_perf_serv);
	else
		seq_printf(m, "%u\n", hps_ctxt.little_num_base_perf_serv);
	return 0;
}

static ssize_t hps_num_base_perf_serv_proc_write(
		struct file *file,
		const char __user *buffer,
		size_t count,
		loff_t *pos)
{
	int len = 0, little_num_base_perf_serv = 0, big_num_base_perf_serv = 0;
	char desc[32];
	unsigned int num_online;

	len = min(count, sizeof(desc) - 1);
	memset(desc, 0, sizeof(desc));
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (hps_ctxt.is_hmp &&
		(sscanf(desc, "%u %u",
			&little_num_base_perf_serv,
			&big_num_base_perf_serv) == 2)) {
		if (little_num_base_perf_serv > num_possible_little_cpus()
			|| little_num_base_perf_serv < 1) {
			hps_warn(
				"hps_num_base_perf_serv_proc_write, bad argument(%u, %u)\n",
				little_num_base_perf_serv,
				big_num_base_perf_serv);
			return -EINVAL;
		}

		if (big_num_base_perf_serv > num_possible_big_cpus()) {
			hps_warn(
				"hps_num_base_perf_serv_proc_write, bad argument(%u, %u)\n",
				little_num_base_perf_serv,
				big_num_base_perf_serv);
			return -EINVAL;
		}

		mutex_lock(&hps_ctxt.lock);

		hps_ctxt.little_num_base_perf_serv = little_num_base_perf_serv;
		hps_ctxt.big_num_base_perf_serv = big_num_base_perf_serv;
		num_online = num_online_big_cpus();
		if ((num_online < big_num_base_perf_serv) &&
			(num_online <
				min(hps_ctxt.big_num_limit_thermal,
					hps_ctxt.big_num_limit_low_battery)) &&
			(num_online <
				min(hps_ctxt.big_num_limit_ultra_power_saving,
					hps_ctxt.big_num_limit_power_serv)))
			hps_task_wakeup_nolock();
		else {
			num_online = num_online_little_cpus();
			if ((num_online < little_num_base_perf_serv) &&
				(num_online <
				min(
				  hps_ctxt.little_num_limit_thermal,
				  hps_ctxt.little_num_limit_low_battery)) &&
				(num_online <
				min(
				  hps_ctxt.little_num_limit_ultra_power_saving,
				  hps_ctxt.little_num_limit_power_serv)) &&
				(num_online_cpus() <
					(little_num_base_perf_serv +
						big_num_base_perf_serv)))
				hps_task_wakeup_nolock();
		}

		/* XXX: should we move mutex_unlock(&hps_ctxt.lock) to
			earlier stage? no! */
		mutex_unlock(&hps_ctxt.lock);

		return count;
	} else if (!hps_ctxt.is_hmp &&
			!kstrtouint(desc, 0, &little_num_base_perf_serv)) {
		if (little_num_base_perf_serv > num_possible_little_cpus()
			|| little_num_base_perf_serv < 1) {
			hps_warn(
				"hps_num_base_perf_serv_proc_write, bad argument(%u)\n",
				little_num_base_perf_serv);
			return -EINVAL;
		}

		mutex_lock(&hps_ctxt.lock);

		hps_ctxt.little_num_base_perf_serv = little_num_base_perf_serv;
		num_online = num_online_little_cpus();
		if ((num_online < little_num_base_perf_serv) &&
			(num_online <
			min(hps_ctxt.little_num_limit_thermal,
				hps_ctxt.little_num_limit_low_battery)) &&
			(num_online <
			min(hps_ctxt.little_num_limit_ultra_power_saving,
				hps_ctxt.little_num_limit_power_serv)))
			hps_task_wakeup_nolock();

		/* XXX: should we move mutex_unlock(&hps_ctxt.lock)
			to earlier stage? no! */
		mutex_unlock(&hps_ctxt.lock);

		return count;
	}

	hps_warn("hps_num_base_perf_serv_proc_write, bad argument\n");

	return -EINVAL;
}

PROC_FOPS_RW(num_base_perf_serv);

/***********************************************************
* procfs callback - algo bound series
*                     - little_num_limit_thermal
*                     - big_num_limit_thermal
***********************************************************/
static int hps_num_limit_thermal_proc_show(struct seq_file *m, void *v)
{
	if (hps_ctxt.is_hmp)
		seq_printf(m, "%u %u\n", hps_ctxt.little_num_limit_thermal,
			hps_ctxt.big_num_limit_thermal);
	else
		seq_printf(m, "%u\n", hps_ctxt.little_num_limit_thermal);
	return 0;
}

static ssize_t hps_num_limit_thermal_proc_write(
		struct file *file,
		const char __user *buffer,
		size_t count,
		loff_t *pos)
{
	int len = 0, little_num_limit_thermal = 0, big_num_limit_thermal = 0;
	char desc[32];

	len = min(count, sizeof(desc) - 1);
	memset(desc, 0, sizeof(desc));
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (hps_ctxt.is_hmp &&
		(sscanf(desc, "%u %u", &little_num_limit_thermal,
			&big_num_limit_thermal) == 2)) {
		if (little_num_limit_thermal > num_possible_little_cpus()
			|| little_num_limit_thermal < 1) {
			hps_warn(
				"hps_num_limit_thermal_proc_write, bad argument(%u, %u)\n",
				little_num_limit_thermal,
				big_num_limit_thermal);
			return -EINVAL;
		}

		if (big_num_limit_thermal > num_possible_big_cpus()) {
			hps_warn(
				"hps_num_limit_thermal_proc_write, bad argument(%u, %u)\n",
				little_num_limit_thermal,
				big_num_limit_thermal);
			return -EINVAL;
		}

		mutex_lock(&hps_ctxt.lock);

		hps_ctxt.little_num_limit_thermal = little_num_limit_thermal;
		hps_ctxt.big_num_limit_thermal = big_num_limit_thermal;
		if (num_online_big_cpus() > big_num_limit_thermal)
			hps_task_wakeup_nolock();
		else if (num_online_little_cpus() > little_num_limit_thermal)
			hps_task_wakeup_nolock();

		mutex_unlock(&hps_ctxt.lock);

		return count;
	} else if (!hps_ctxt.is_hmp &&
			!kstrtouint(desc, 0, &little_num_limit_thermal)) {
		if (little_num_limit_thermal > num_possible_little_cpus()
			|| little_num_limit_thermal < 1) {
			hps_warn(
				"hps_num_limit_thermal_proc_write, bad argument(%u)\n",
				little_num_limit_thermal);
			return -EINVAL;
		}

		mutex_lock(&hps_ctxt.lock);

		hps_ctxt.little_num_limit_thermal = little_num_limit_thermal;
		if (num_online_little_cpus() > little_num_limit_thermal)
			hps_task_wakeup_nolock();

		mutex_unlock(&hps_ctxt.lock);

		return count;
	}

	hps_warn("hps_num_limit_thermal_proc_write, bad argument\n");

	return -EINVAL;
}

PROC_FOPS_RW(num_limit_thermal);

/***********************************************************
* procfs callback - algo bound series
*                     - little_num_limit_low_battery
*                     - big_num_limit_low_battery
***********************************************************/
static int hps_num_limit_low_battery_proc_show(struct seq_file *m, void *v)
{
	if (hps_ctxt.is_hmp)
		seq_printf(m, "%u %u\n", hps_ctxt.little_num_limit_low_battery,
			hps_ctxt.big_num_limit_low_battery);
	else
		seq_printf(m, "%u\n", hps_ctxt.little_num_limit_low_battery);
	return 0;
}

static ssize_t hps_num_limit_low_battery_proc_write(
		struct file *file,
		const char __user *buffer,
		size_t count,
		loff_t *pos)
{
	int len = 0;
	int little_num_limit_low_battery = 0, big_num_limit_low_battery = 0;
	char desc[32];

	len = min(count, sizeof(desc) - 1);
	memset(desc, 0, sizeof(desc));
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (hps_ctxt.is_hmp &&
		(sscanf(desc, "%u %u", &little_num_limit_low_battery,
			&big_num_limit_low_battery) == 2)) {
		if (little_num_limit_low_battery > num_possible_little_cpus()
			|| little_num_limit_low_battery < 1) {
			hps_warn(
				"hps_num_limit_low_battery_proc_write, bad argument(%u, %u)\n",
				little_num_limit_low_battery,
				big_num_limit_low_battery);
			return -EINVAL;
		}

		if (big_num_limit_low_battery > num_possible_big_cpus()) {
			hps_warn(
				"hps_num_limit_low_battery_proc_write, bad argument(%u, %u)\n",
				little_num_limit_low_battery,
				big_num_limit_low_battery);
			return -EINVAL;
		}

		mutex_lock(&hps_ctxt.lock);

		hps_ctxt.little_num_limit_low_battery =
			little_num_limit_low_battery;
		hps_ctxt.big_num_limit_low_battery = big_num_limit_low_battery;
		if (num_online_big_cpus() > big_num_limit_low_battery)
			hps_task_wakeup_nolock();
		else if (num_online_little_cpus() >
			little_num_limit_low_battery)
			hps_task_wakeup_nolock();

		mutex_unlock(&hps_ctxt.lock);

		return count;
	} else if (!hps_ctxt.is_hmp &&
			!kstrtouint(desc, 0, &little_num_limit_low_battery)) {
		if (little_num_limit_low_battery > num_possible_little_cpus()
			|| little_num_limit_low_battery < 1) {
			hps_warn(
				"hps_num_limit_low_battery_proc_write, bad argument(%u)\n",
				little_num_limit_low_battery);
			return -EINVAL;
		}

		mutex_lock(&hps_ctxt.lock);

		hps_ctxt.little_num_limit_low_battery =
			little_num_limit_low_battery;
		if (num_online_little_cpus() > little_num_limit_low_battery)
			hps_task_wakeup_nolock();

		mutex_unlock(&hps_ctxt.lock);

		return count;
	}

	hps_warn("hps_num_limit_low_battery_proc_write, bad argument\n");

	return -EINVAL;
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
	if (hps_ctxt.is_hmp)
		seq_printf(m, "%u %u\n",
			hps_ctxt.little_num_limit_ultra_power_saving,
			hps_ctxt.big_num_limit_ultra_power_saving);
	else
		seq_printf(m, "%u\n",
			hps_ctxt.little_num_limit_ultra_power_saving);
	return 0;
}

static ssize_t hps_num_limit_ultra_power_saving_proc_write(
		struct file *file,
		const char __user *buffer,
		size_t count,
		loff_t *pos)
{
	int len = 0;
	int little_num_limit_ultra_power_saving = 0;
	int big_num_limit_ultra_power_saving = 0;
	char desc[32];

	len = min(count, sizeof(desc) - 1);
	memset(desc, 0, sizeof(desc));
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (hps_ctxt.is_hmp &&
		(sscanf(desc, "%u %u",
			&little_num_limit_ultra_power_saving,
			&big_num_limit_ultra_power_saving) == 2)) {
		if (little_num_limit_ultra_power_saving >
			num_possible_little_cpus() ||
			little_num_limit_ultra_power_saving < 1) {
			hps_warn(
				"hps_num_limit_ultra_power_saving_proc_write, bad argument(%u, %u)\n",
				little_num_limit_ultra_power_saving,
				big_num_limit_ultra_power_saving);
			return -EINVAL;
		}

		if (big_num_limit_ultra_power_saving >
			num_possible_big_cpus()) {
			hps_warn(
				"hps_num_limit_ultra_power_saving_proc_write, bad argument(%u, %u)\n",
				little_num_limit_ultra_power_saving,
				big_num_limit_ultra_power_saving);
			return -EINVAL;
		}

		mutex_lock(&hps_ctxt.lock);

		hps_ctxt.little_num_limit_ultra_power_saving =
			little_num_limit_ultra_power_saving;
		hps_ctxt.big_num_limit_ultra_power_saving =
			big_num_limit_ultra_power_saving;
		if (num_online_big_cpus() > big_num_limit_ultra_power_saving)
			hps_task_wakeup_nolock();
		else if (num_online_little_cpus() >
				little_num_limit_ultra_power_saving)
			hps_task_wakeup_nolock();

		mutex_unlock(&hps_ctxt.lock);

		return count;
	} else if (!hps_ctxt.is_hmp &&
			!kstrtouint(desc, 0,
				&little_num_limit_ultra_power_saving)) {
		if (little_num_limit_ultra_power_saving >
			num_possible_little_cpus()
			|| little_num_limit_ultra_power_saving < 1) {
			hps_warn(
				"hps_num_limit_ultra_power_saving_proc_write, bad argument(%u)\n",
				little_num_limit_ultra_power_saving);
			return -EINVAL;
		}

		mutex_lock(&hps_ctxt.lock);

		hps_ctxt.little_num_limit_ultra_power_saving =
			little_num_limit_ultra_power_saving;
		if (num_online_little_cpus() >
			little_num_limit_ultra_power_saving)
			hps_task_wakeup_nolock();

		mutex_unlock(&hps_ctxt.lock);

		return count;
	}

	hps_warn("hps_num_limit_ultra_power_saving_proc_write, bad argument\n");

	return -EINVAL;
}

PROC_FOPS_RW(num_limit_ultra_power_saving);

/***********************************************************
* procfs callback - algo bound series
*                     - little_num_limit_power_serv
*                     - big_num_limit_power_serv
***********************************************************/
static int hps_num_limit_power_serv_proc_show(struct seq_file *m, void *v)
{
	if (hps_ctxt.is_hmp)
		seq_printf(m, "%u %u\n", hps_ctxt.little_num_limit_power_serv,
			hps_ctxt.big_num_limit_power_serv);
	else
		seq_printf(m, "%u\n", hps_ctxt.little_num_limit_power_serv);
	return 0;
}

static ssize_t hps_num_limit_power_serv_proc_write(
		struct file *file,
		const char __user *buffer,
		size_t count,
		loff_t *pos)
{
	int len = 0;
	int little_num_limit_power_serv = 0, big_num_limit_power_serv = 0;
	char desc[32];

	len = min(count, sizeof(desc) - 1);
	memset(desc, 0, sizeof(desc));
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (hps_ctxt.is_hmp &&
		(sscanf(desc, "%u %u",
			&little_num_limit_power_serv,
			&big_num_limit_power_serv) == 2)) {
		if (little_num_limit_power_serv > num_possible_little_cpus()
			|| little_num_limit_power_serv < 1) {
			hps_warn(
				"hps_num_limit_power_serv_proc_write, bad argument(%u, %u)\n",
				little_num_limit_power_serv,
				big_num_limit_power_serv);
			return -EINVAL;
		}

		if (big_num_limit_power_serv > num_possible_big_cpus()) {
			hps_warn(
				"hps_num_limit_power_serv_proc_write, bad argument(%u, %u)\n",
				little_num_limit_power_serv,
				big_num_limit_power_serv);
			return -EINVAL;
		}

		mutex_lock(&hps_ctxt.lock);

		hps_ctxt.little_num_limit_power_serv =
			little_num_limit_power_serv;
		hps_ctxt.big_num_limit_power_serv = big_num_limit_power_serv;
		if (num_online_big_cpus() > big_num_limit_power_serv)
			hps_task_wakeup_nolock();
		else if (num_online_little_cpus() > little_num_limit_power_serv)
			hps_task_wakeup_nolock();

		mutex_unlock(&hps_ctxt.lock);

		return count;
	} else if (!hps_ctxt.is_hmp &&
			!kstrtouint(desc, 0, &little_num_limit_power_serv)) {
		if (little_num_limit_power_serv > num_possible_little_cpus()
			|| little_num_limit_power_serv < 1) {
			hps_warn(
				"hps_num_limit_power_serv_proc_write, bad argument(%u)\n",
				little_num_limit_power_serv);
			return -EINVAL;
		}

		mutex_lock(&hps_ctxt.lock);

		hps_ctxt.little_num_limit_power_serv =
			little_num_limit_power_serv;
		if (num_online_little_cpus() > little_num_limit_power_serv)
			hps_task_wakeup_nolock();

		mutex_unlock(&hps_ctxt.lock);

		return count;
	}

	hps_warn("hps_num_limit_power_serv_proc_write, bad argument\n");


	return -EINVAL;
}

PROC_FOPS_RW(num_limit_power_serv);

/*============================================================================
 * Gobal function definition
 *============================================================================*/
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
		PROC_ENTRY(state),
		PROC_ENTRY(enabled),
		PROC_ENTRY(early_suspend_enabled),
		PROC_ENTRY(suspend_enabled),
		PROC_ENTRY(log_mask),
		PROC_ENTRY(up_threshold),
		PROC_ENTRY(up_times),
		PROC_ENTRY(down_threshold),
		PROC_ENTRY(down_times),
		PROC_ENTRY(input_boost_enabled),
		PROC_ENTRY(input_boost_cpu_num),
		PROC_ENTRY(rush_boost_enabled),
		PROC_ENTRY(rush_boost_threshold),
		PROC_ENTRY(rush_boost_times),
		PROC_ENTRY(tlp_times),
		PROC_ENTRY(num_base_perf_serv),
		PROC_ENTRY(num_limit_thermal),
		PROC_ENTRY(num_limit_low_battery),
		PROC_ENTRY(num_limit_ultra_power_saving),
		PROC_ENTRY(num_limit_power_serv),
	};

	log_info("hps_procfs_init\n");

	hps_dir = proc_mkdir("hps", NULL);
	if (hps_dir == NULL) {
		hps_err("mkdir /proc/hps fail\n");
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name,
			S_IRUGO | S_IWUSR | S_IWGRP,
			hps_dir, entries[i].fops))
			hps_err("create /proc/hps/%s failed\n",
				entries[i].name);
	}

	return r;
}
