// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include "mtk_hps_internal.h"

typedef void (*func_void) (void);

static int hps_proc_uint_show(struct seq_file *m, void *v)
{
	unsigned int *pv = (unsigned int *)m->private;

	seq_printf(m, "%u\n", *pv);
	return 0;
}

static int hps_sanitize_var(unsigned int *hpsvar, unsigned int newv)
{
	int rc = -EINVAL;

	if (hpsvar == &hps_ctxt.enabled) {
		if (newv >= 0 && newv <= 1)
			rc = 0;
	} else if (hpsvar == &hps_ctxt.heavy_task_enabled) {
		if (newv >= 0 && newv <= 1)
			rc = 0;
	} else if (hpsvar == &hps_ctxt.suspend_enabled) {
		if (newv >= 0 && newv <= 1)
			rc = 0;
	} else if (hpsvar == &hps_ctxt.cur_dump_enabled) {
		if (newv >= 0 && newv <= 1)
			rc = 0;
	} else if (hpsvar == &hps_ctxt.stats_dump_enabled) {
		if (newv >= 0 && newv <= 1)
			rc = 0;
	} else if (hpsvar == &hps_ctxt.big_task_enabled) {
		if (newv >= 0 && newv <= 1)
			rc = 0;
	} else if (hpsvar == &hps_ctxt.idle_det_enabled) {
		if (newv >= 0 && newv <= 1)
			rc = 0;
	} else if (hpsvar == &hps_ctxt.up_threshold) {
		if (newv > 0)
			rc = 0;
	} else if (hpsvar == &hps_ctxt.up_times) {
		if (newv > 0 && newv <= (unsigned int)MAX_CPU_UP_TIMES)
			rc = 0;
	} else if (hpsvar == &hps_ctxt.down_threshold) {
		if (newv > 0)
			rc = 0;
	} else if (hpsvar == &hps_ctxt.down_times) {
		if (newv > 0 && newv <= (unsigned int)MAX_CPU_DOWN_TIMES)
			rc = 0;
	} else if (hpsvar == &hps_ctxt.input_boost_enabled) {
		if (newv >= 0 && newv <= 1)
			rc = 0;
	} else if (hpsvar == &hps_ctxt.input_boost_cpu_num) {
		/* NOTE: what is the maximum? */
		if (newv > 0)
			rc = 0;
	} else if (hpsvar == &hps_ctxt.rush_boost_enabled) {
		if (newv >= 0 && newv <= 1)
			rc = 0;
	} else if (hpsvar == &hps_ctxt.rush_boost_threshold) {
		if (newv > 0)
			rc = 0;
	} else if (hpsvar == &hps_ctxt.idle_threshold) {
		if (newv >= 0)
			rc = 0;
	} else if (hpsvar == &hps_ctxt.rush_boost_times) {
		if (newv > 0)
			rc = 0;
	} else if (hpsvar == &hps_ctxt.tlp_times) {
		if (newv > 0 && newv <= (unsigned int)MAX_TLP_TIMES)
			rc = 0;
	} else if (hpsvar == &hps_ctxt.power_mode) {
		/* NOTE: what is the accepted range? */
		if (newv > 0)
			rc = 0;
	} else {
		hps_warn("Unknown hps procfs node\n");
		rc = -EINVAL;
	}

	return rc;
}

static ssize_t hps_proc_uint_write(struct file *file, const char __user *buffer,
				   size_t count, loff_t *pos,
				   func_void before_write,
				   func_void after_write)
{
	unsigned int var;
	unsigned int *pv;
	int rc;

	rc = kstrtouint_from_user(buffer, count, 0, &var);
	if (rc)
		return rc;

	pv = ((struct seq_file *)file->private_data)->private;
	rc = hps_sanitize_var(pv, var);
	if (rc)
		return rc;

	if (before_write)
		before_write();

	*pv = var;

	if (after_write)
		after_write();

	return count;
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

static ssize_t hps_proc_uint_write_with_lock(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	return hps_proc_uint_write(file, buffer, count, pos,
		lock_hps_ctxt, unlock_hps_ctxt);
}

static ssize_t hps_proc_uint_write_with_lock_reset(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
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

/*
 * procfs callback - state series
 *     - init_state
 *     - state
 */
PROC_FOPS_RO_UINT(init_state, hps_ctxt.init_state);
PROC_FOPS_RO_UINT(state, hps_ctxt.state);

/*
 * procfs callback - enabled series
 *     - enabled
 *     - suspend_enabled
 *     - cur_dump_enabled
 *     - stats_dump_enabled
 *     - heavy_task_enabled
 *     - big_task_enabled
 *     - idle_det_enabled
 */
PROC_FOPS_RW_UINT(enabled, hps_ctxt.enabled,
	hps_proc_uint_write_with_lock_reset);
PROC_FOPS_RW_UINT(eas_enabled, hps_ctxt.eas_enabled,
	hps_proc_uint_write_with_lock_reset);
PROC_FOPS_RW_UINT(heavy_task_enabled, hps_ctxt.heavy_task_enabled,
		  hps_proc_uint_write_with_lock_reset);
PROC_FOPS_RW_UINT(big_task_enabled, hps_ctxt.big_task_enabled,
		hps_proc_uint_write_with_lock_reset);
PROC_FOPS_RW_UINT(idle_det_enabled, hps_ctxt.idle_det_enabled,
	hps_proc_uint_write_with_lock_reset);
PROC_FOPS_RW_UINT(suspend_enabled, hps_ctxt.suspend_enabled,
	hps_proc_uint_write_with_lock);
PROC_FOPS_RW_UINT(cur_dump_enabled, hps_ctxt.cur_dump_enabled,
	hps_proc_uint_write_with_lock);
PROC_FOPS_RW_UINT(stats_dump_enabled, hps_ctxt.stats_dump_enabled,
	hps_proc_uint_write_with_lock);

/*
 * procfs callback - algo config series
 *     - up_threshold
 *     - up_times
 *     - down_threshold
 *     - down_times
 *     - input_boost_enabled
 *     - input_boost_cpu_num
 *     - rush_boost_enabled
 *     - rush_boost_threshold
 *     - rush_boost_times
 *     - tlp_times
 *     - idle_threshold
 */
PROC_FOPS_RW_UINT(up_threshold, hps_ctxt.up_threshold,
	hps_proc_uint_write_with_lock_reset);
PROC_FOPS_RW_UINT(up_times, hps_ctxt.up_times,
	hps_proc_uint_write_with_lock_reset);
PROC_FOPS_RW_UINT(down_threshold, hps_ctxt.down_threshold,
	hps_proc_uint_write_with_lock_reset);
PROC_FOPS_RW_UINT(down_times, hps_ctxt.down_times,
	hps_proc_uint_write_with_lock_reset);
PROC_FOPS_RW_UINT(input_boost_enabled, hps_ctxt.input_boost_enabled,
	hps_proc_uint_write_with_lock);
PROC_FOPS_RW_UINT(input_boost_cpu_num, hps_ctxt.input_boost_cpu_num,
	hps_proc_uint_write_with_lock);
PROC_FOPS_RW_UINT(rush_boost_enabled, hps_ctxt.rush_boost_enabled,
	hps_proc_uint_write_with_lock_reset);
PROC_FOPS_RW_UINT(rush_boost_threshold, hps_ctxt.rush_boost_threshold,
	hps_proc_uint_write_with_lock_reset);
PROC_FOPS_RW_UINT(rush_boost_times, hps_ctxt.rush_boost_times,
	hps_proc_uint_write_with_lock_reset);
PROC_FOPS_RW_UINT(tlp_times, hps_ctxt.tlp_times,
	hps_proc_uint_write_with_lock_reset);
PROC_FOPS_RW_UINT(power_mode, hps_ctxt.power_mode,
	hps_proc_uint_write_with_lock_reset);
PROC_FOPS_RW_UINT(idle_threshold, hps_ctxt.idle_threshold,
	hps_proc_uint_write_with_lock_reset);

/*
 * procfs callback - algo bound series
 *     - little_num_base_perf_serv
 *     - big_num_base_perf_serv
 */
static int hps_pwrseq_proc_show(struct seq_file *m, void *v)
{
	int i = 0;
	unsigned int cluster_num = hps_sys.cluster_num;

	for (i = 0; i < cluster_num; i++)
		seq_printf(m, "cluster %d, power sequence = %d\n",
			i, hps_sys.cluster_info[i].pwr_seq);
	return 0;
}

static ssize_t hps_pwrseq_proc_write(struct file *file,
				     const char __user *buffer,
				     size_t count, loff_t *pos)
{
	int i = 0;
	int j = 0;
	int *pwrseq;
	char *tok;
	unsigned int cluster_num = hps_sys.cluster_num;
	char desc[64], *pdesc = desc;
	int rc;
	int len;

	len = min(count, sizeof(desc) - 1);
	rc = copy_from_user(desc, buffer, len);
	if (rc)
		return rc;

	if (len > 0)
		desc[len] = 0;

	pwrseq = kcalloc(cluster_num, sizeof(*pwrseq), GFP_KERNEL);
	if (!pwrseq)
		goto out;

	while ((tok = strsep(&pdesc, " ")) != NULL) {
		if (i == cluster_num) {
			hps_warn("@%s: number of arguments > %d!\n",
				__func__, cluster_num);
			goto out;
		}

		if (kstrtoint(tok, 10, &pwrseq[i])) {
			hps_warn("@%s: Invalid input: %s\n", __func__, tok);
			goto out;
		} else
			i++;
	}

	if (i < cluster_num) {
		hps_warn("@%s: number of arguments < %d!\n",
			__func__, cluster_num);
		goto out;
	}

	for (i = 0; i < cluster_num; i++) {
		if (pwrseq[i] > cluster_num) {
			hps_warn("@%s: Invalid input! pwrseq[%d] = %d\n",
				__func__, i, pwrseq[i]);
			goto out;
		}
		for (j = 0; j < cluster_num; j++) {
			if (i != j) {
				if (pwrseq[i] == pwrseq[j]) {
					hps_warn
			("@%s: Invalid input! pwrseq[%d] = pwrseq[%d] = %d\n",
			__func__, i, j, pwrseq[i]);
					goto out;
				}
			}
		}
	}
	mutex_lock(&hps_ctxt.lock);
	for (i = 0; i < cluster_num; i++)
		hps_sys.cluster_info[i].pwr_seq = pwrseq[i];

	mutex_unlock(&hps_ctxt.lock);
	kfree(pwrseq);
	return count;

out:
	kfree(pwrseq);
	return -EINVAL;

}
PROC_FOPS_RW(pwrseq);

static int hps_num_base_perf_serv_proc_show(struct seq_file *m, void *v)
{
	if (hps_ctxt.is_hmp || hps_ctxt.is_amp)
		seq_printf(m, "%u %u\n", hps_ctxt.little_num_base_perf_serv,
		hps_ctxt.big_num_base_perf_serv);
	else
		seq_printf(m, "%u\n", hps_ctxt.little_num_base_perf_serv);
	return 0;
}

static ssize_t hps_num_base_perf_serv_proc_write(struct file *file,
						 const char __user *buffer,
						 size_t count, loff_t *pos)
{
	int len = 0, little_num_base_perf_serv = 0, big_num_base_perf_serv = 0;
	char desc[32];
	unsigned int num_online;

	len = min(count, sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	if (len > 0)
		desc[len] = '\0';

	if ((hps_ctxt.is_hmp || hps_ctxt.is_amp)
	&& (sscanf(desc, "%u %u", &little_num_base_perf_serv,
	&big_num_base_perf_serv) == 2)) {
		if (little_num_base_perf_serv > num_possible_little_cpus()
		    || little_num_base_perf_serv < 1) {
			hps_warn
		("%s, bad argument(%u, %u)\n",
		__func__,
		little_num_base_perf_serv, big_num_base_perf_serv);
			return -EINVAL;
		}

		if (big_num_base_perf_serv > num_possible_big_cpus()) {
			hps_warn
		("%s, bad argument(%u, %u)\n",
		__func__,
		little_num_base_perf_serv, big_num_base_perf_serv);
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
			     min(hps_ctxt.little_num_limit_thermal,
				 hps_ctxt.little_num_limit_low_battery)) &&
			    (num_online <
			     min(hps_ctxt.little_num_limit_ultra_power_saving,
				 hps_ctxt.little_num_limit_power_serv)) &&
			    (num_online_cpus() <
			(little_num_base_perf_serv + big_num_base_perf_serv)))
				hps_task_wakeup_nolock();
		}

		/* XXX: should we move mutex_unlock(&hps_ctxt.lock) */
		/*to earlier stage? NO */
		mutex_unlock(&hps_ctxt.lock);

		return count;
	} else if ((!hps_ctxt.is_hmp || !hps_ctxt.is_amp)
		   && !kstrtouint(desc, 0, &little_num_base_perf_serv)) {
		if (little_num_base_perf_serv > num_possible_little_cpus()
		    || little_num_base_perf_serv < 1) {
			hps_warn
		("%s, bad argument(%u)\n",
		__func__,
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

		/* XXX: should we move mutex_unlock(&hps_ctxt.lock) */
		/*to earlier stage? NO */
		mutex_unlock(&hps_ctxt.lock);

		return count;
	}

	hps_warn("%s, bad argument\n", __func__);

	return -EINVAL;
}

PROC_FOPS_RW(num_base_perf_serv);

/*
 * procfs callback - algo bound series
 *     - little_num_limit_thermal
 *     - big_num_limit_thermal
 */
static int hps_num_limit_thermal_proc_show(struct seq_file *m, void *v)
{
	if (hps_ctxt.is_hmp || hps_ctxt.is_amp)
		seq_printf(m, "%u %u\n", hps_ctxt.little_num_limit_thermal,
			   hps_ctxt.big_num_limit_thermal);
	else
		seq_printf(m, "%u\n", hps_ctxt.little_num_limit_thermal);
	return 0;
}

static ssize_t hps_num_limit_thermal_proc_write(struct file *file,
						const char __user *buffer,
						size_t count, loff_t *pos)
{
	int len = 0, little_num_limit_thermal = 0, big_num_limit_thermal = 0;
	char desc[32];

	len = min(count, sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	if (len > 0)
		desc[len] = '\0';

	if ((hps_ctxt.is_hmp || hps_ctxt.is_amp)
	&& (sscanf(desc, "%u %u", &little_num_limit_thermal,
	&big_num_limit_thermal) == 2)) {
		if (little_num_limit_thermal > num_possible_little_cpus()
		    || little_num_limit_thermal < 1) {
			hps_warn
		("%s, bad argument(%u, %u)\n",
		__func__,
		little_num_limit_thermal, big_num_limit_thermal);
			return -EINVAL;
		}

		if (big_num_limit_thermal > num_possible_big_cpus()) {
			hps_warn
		("%s, bad argument(%u, %u)\n",
		__func__,
		little_num_limit_thermal, big_num_limit_thermal);
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
	} else if ((!hps_ctxt.is_hmp || !hps_ctxt.is_amp)
		   && !kstrtouint(desc, 0, &little_num_limit_thermal)) {
		if (little_num_limit_thermal > num_possible_little_cpus()
		    || little_num_limit_thermal < 1) {
			hps_warn
		("%s, bad argument(%u)\n",
		__func__,
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

	hps_warn("%s, bad argument\n", __func__);

	return -EINVAL;
}

PROC_FOPS_RW(num_limit_thermal);

/*
 * procfs callback - algo bound series
 *     - little_num_limit_low_battery
 *     - big_num_limit_low_battery
 */
static int hps_num_limit_low_battery_proc_show(struct seq_file *m, void *v)
{
	if (hps_ctxt.is_hmp || hps_ctxt.is_amp)
		seq_printf(m, "%u %u\n", hps_ctxt.little_num_limit_low_battery,
			   hps_ctxt.big_num_limit_low_battery);
	else
		seq_printf(m, "%u\n", hps_ctxt.little_num_limit_low_battery);
	return 0;
}

static ssize_t hps_num_limit_low_battery_proc_write(struct file *file,
						    const char __user *buffer,
						    size_t count, loff_t *pos)
{
	int len = 0;
	int little_num_limit_low_battery = 0, big_num_limit_low_battery = 0;
	char desc[32];

	len = min(count, sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	if (len > 0)
		desc[len] = '\0';

	if ((hps_ctxt.is_hmp || hps_ctxt.is_amp)
	    && (sscanf(desc, "%u %u", &little_num_limit_low_battery,
		       &big_num_limit_low_battery) == 2)) {
		if (little_num_limit_low_battery > num_possible_little_cpus()
		    || little_num_limit_low_battery < 1) {
			hps_warn
	("%s, bad argument(%u, %u)\n",
	__func__,
	little_num_limit_low_battery, big_num_limit_low_battery);
			return -EINVAL;
		}

		if (big_num_limit_low_battery > num_possible_big_cpus()) {
			hps_warn
	("%s, bad argument(%u, %u)\n",
	__func__,
	little_num_limit_low_battery, big_num_limit_low_battery);
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
	} else if ((!hps_ctxt.is_hmp || !hps_ctxt.is_amp)
		   && !kstrtouint(desc, 0, &little_num_limit_low_battery)) {
		if (little_num_limit_low_battery > num_possible_little_cpus()
		    || little_num_limit_low_battery < 1) {
			hps_warn
		("%s, bad argument(%u)\n",
		__func__,
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

	hps_warn("%s, bad argument\n", __func__);

	return -EINVAL;
}

PROC_FOPS_RW(num_limit_low_battery);

/*
 * procfs callback - algo bound series
 *     - little_num_limit_ultra_power_saving
 *     - big_num_limit_ultra_power_saving
 */
static int hps_num_limit_ultra_power_saving_proc_show(struct seq_file *m,
	void *v)
{
	if (hps_ctxt.is_hmp || hps_ctxt.is_amp)
		seq_printf(m, "%u %u\n",
			   hps_ctxt.little_num_limit_ultra_power_saving,
			   hps_ctxt.big_num_limit_ultra_power_saving);
	else
		seq_printf(m, "%u\n",
			hps_ctxt.little_num_limit_ultra_power_saving);
	return 0;
}

static ssize_t hps_num_limit_ultra_power_saving_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int len = 0;
	int little_num_limit_ultra_power_saving = 0;
	int big_num_limit_ultra_power_saving = 0;
	char desc[32];

	len = min(count, sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	if (len > 0)
		desc[len] = '\0';

	if ((hps_ctxt.is_hmp || hps_ctxt.is_amp)
	    &&
	    (sscanf(desc, "%u %u",
		    &little_num_limit_ultra_power_saving,
		    &big_num_limit_ultra_power_saving) == 2)) {
		if (little_num_limit_ultra_power_saving >
		num_possible_little_cpus() ||
		little_num_limit_ultra_power_saving < 1) {
			hps_warn
("%s, bad argument(%u, %u)\n",
__func__,
little_num_limit_ultra_power_saving, big_num_limit_ultra_power_saving);
			return -EINVAL;
		}

		if (big_num_limit_ultra_power_saving >
		num_possible_big_cpus()) {
			hps_warn
("%s, bad argument(%u, %u)\n",
__func__,
little_num_limit_ultra_power_saving, big_num_limit_ultra_power_saving);
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
	} else if ((!hps_ctxt.is_hmp || !hps_ctxt.is_amp)
	&& !kstrtouint(desc, 0,
	&little_num_limit_ultra_power_saving)) {
		if (little_num_limit_ultra_power_saving >
		num_possible_little_cpus()
		    || little_num_limit_ultra_power_saving < 1) {
			hps_warn
	("%s, bad argument(%u)\n",
	__func__,
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

	hps_warn("%s, bad argument\n", __func__);

	return -EINVAL;
}

PROC_FOPS_RW(num_limit_ultra_power_saving);

/*
 * procfs callback - algo bound series
 *     - little_num_limit_power_serv
 *     - big_num_limit_power_serv
 */
static int hps_num_limit_power_serv_proc_show(struct seq_file *m, void *v)
{
	if (hps_ctxt.is_hmp || hps_ctxt.is_amp)
		seq_printf(m, "%u %u\n", hps_ctxt.little_num_limit_power_serv,
			   hps_ctxt.big_num_limit_power_serv);
	else
		seq_printf(m, "%u\n", hps_ctxt.little_num_limit_power_serv);
	return 0;
}

static ssize_t hps_num_limit_power_serv_proc_write(struct file *file,
						   const char __user *buffer,
						   size_t count, loff_t *pos)
{
	int len = 0;
	int little_num_limit_power_serv = 0, big_num_limit_power_serv = 0;
	char desc[32];

	len = min(count, sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	if (len > 0)
		desc[len] = '\0';

	if ((hps_ctxt.is_hmp || hps_ctxt.is_amp)
	&&
	(sscanf(desc, "%u %u", &little_num_limit_power_serv,
	&big_num_limit_power_serv) == 2)) {
		if (little_num_limit_power_serv > num_possible_little_cpus()
		    || little_num_limit_power_serv < 1) {
			hps_warn
		("%s, bad argument(%u, %u)\n",
		__func__,
		little_num_limit_power_serv, big_num_limit_power_serv);
			return -EINVAL;
		}

		if (big_num_limit_power_serv > num_possible_big_cpus()) {
			hps_warn
		("%s, bad argument(%u, %u)\n",
		__func__,
		little_num_limit_power_serv, big_num_limit_power_serv);
			return -EINVAL;
		}

		mutex_lock(&hps_ctxt.lock);

		hps_ctxt.little_num_limit_power_serv =
			little_num_limit_power_serv;
		hps_ctxt.big_num_limit_power_serv = big_num_limit_power_serv;
		if (num_online_big_cpus() > big_num_limit_power_serv)
			hps_task_wakeup_nolock();
		else if (num_online_little_cpus() >
		little_num_limit_power_serv)
			hps_task_wakeup_nolock();

		mutex_unlock(&hps_ctxt.lock);

		return count;
	} else if ((!hps_ctxt.is_hmp || !hps_ctxt.is_amp)
		   && !kstrtouint(desc, 0, &little_num_limit_power_serv)) {
		if (little_num_limit_power_serv > num_possible_little_cpus()
		    || little_num_limit_power_serv < 1) {
			hps_warn
		("%s, bad argument(%u)\n",
		__func__,
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

	hps_warn("%s, bad argument\n", __func__);


	return -EINVAL;
}

PROC_FOPS_RW(num_limit_power_serv);

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
		PROC_ENTRY(eas_enabled),
		PROC_ENTRY(heavy_task_enabled),
		PROC_ENTRY(big_task_enabled),
		PROC_ENTRY(idle_det_enabled),
		PROC_ENTRY(idle_threshold),
		PROC_ENTRY(suspend_enabled),
		PROC_ENTRY(cur_dump_enabled),
		PROC_ENTRY(stats_dump_enabled),
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
		PROC_ENTRY(power_mode),
		PROC_ENTRY(pwrseq),
	};

	hps_warn("%s\n", __func__);

	hps_dir = proc_mkdir("hps", NULL);
	if (hps_dir == NULL) {
		hps_emerg("mkdir /proc/hps fail\n");
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name,
				 0664, hps_dir, entries[i].fops))
			hps_emerg("create /proc/hps/%s failed\n",
				entries[i].name);
	}

	return r;
}
