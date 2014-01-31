/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pm.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>

#if defined(CONFIG_MSM_RPM_SMD)
#include "lpm_resources.h"
#endif
#include "timer.h"
#include "test-lpm.h"

#define LPM_STATS_RESET "reset"
#define LPM_TEST_ALL_LEVELS "lpm"
#define LPM_TEST_LATENCIES "latency"
#define LPM_TEST_CLEAR "clear"
#define BUF_SIZE 200
#define STAT_BUF_EXTRA_SIZE 500
#define WAIT_FOR_XO 1
#define COMM_BUF_SIZE 15
#define INPUT_COUNT_BUF 10
#define LPM_DEFAULT_CPU 0

#define SNPRINTF(buf, size, format, ...) \
{ \
	if (size > 0) { \
		int ret; \
		ret = snprintf(buf, size, format, ## __VA_ARGS__); \
		if (ret > size) { \
			buf += size; \
			size = 0; \
		} else { \
			buf += ret; \
			size -= ret; \
		} \
	} \
} \

static DEFINE_MUTEX(lpm_stats_mutex);

struct lpm_level_stat {
	char level_name[BUF_SIZE];
	int64_t min_time;
	int64_t max_time;
	int64_t avg_time;
	int64_t exit_early;
	int64_t count;
	unsigned long min_threshold;
	uint32_t kernel_sleep_time;
	bool entered;
};

static DEFINE_PER_CPU(struct lpm_level_stat *, lpm_levels);

static struct dentry *lpm_stat;
static struct dentry *lpm_ext_comm;
static struct msm_rpmrs_level *lpm_supp_level;
static int lpm_level_count;
static int lpm_level_iter;
static bool msm_lpm_use_qtimer;
static unsigned long lpm_sleep_time;
static bool lpm_latency_test;

static unsigned int timer_interval = 5000;
module_param_named(lpm_timer_interval_msec, timer_interval, uint,
	S_IRUGO | S_IWUSR | S_IWGRP);

static unsigned int latency_test_interval = 50;
module_param_named(lpm_latency_timer_interval_usec, latency_test_interval, uint,
	S_IRUGO | S_IWUSR | S_IWGRP);

static unsigned int cpu_to_debug = LPM_DEFAULT_CPU;
static int lpm_cpu_update(const char *val, const struct kernel_param *kp)
{
	int ret = 0;
	unsigned int debug_val;

	ret = kstrtouint(val, 10, &debug_val);
	if ((ret < 0) || (debug_val >= num_possible_cpus()))
		return -EINVAL;
	cpu_to_debug = debug_val;
	return ret;
}

static struct kernel_param_ops cpu_debug_events = {
	.set = lpm_cpu_update,
};

module_param_cb(cpu_to_debug, &cpu_debug_events, &cpu_to_debug,
			S_IRUGO | S_IWUSR | S_IWGRP);

static void lpm_populate_name(struct lpm_level_stat *stat,
		struct msm_rpmrs_level *supp)
{
	char nm[BUF_SIZE] = {0};
	char default_buf[20];

	switch (supp->sleep_mode) {
	case MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT:
		strlcat(nm, "WFI ", BUF_SIZE);
		break;
	case MSM_PM_SLEEP_MODE_RETENTION:
		strlcat(nm, "Retention ", BUF_SIZE);
		break;
	case MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE:
		strlcat(nm, "Standalone Power collapse ", BUF_SIZE);
		break;
	case MSM_PM_SLEEP_MODE_POWER_COLLAPSE:
		strlcat(nm, "Idle Power collapse ", BUF_SIZE);
		break;
	case MSM_PM_MODE_POWER_COLLASE_SUSPEND:
		strlcat(nm, "Suspend Power collapse", BUF_SIZE);
	default:
		strlcat(nm, "Invalid Mode ", BUF_SIZE);
		break;
	}

	switch (msm_pm_get_pxo(&(supp->rs_limits))) {
	case MSM_PM(PXO_OFF):
		strlcat(nm, "XO: OFF ", BUF_SIZE);
		break;
	case MSM_PM(PXO_ON):
		strlcat(nm, "XO: ON ", BUF_SIZE);
		break;
	default:
		snprintf(default_buf, sizeof(default_buf),
			"XO : %d ", msm_pm_get_pxo(&(supp->rs_limits)));
		strlcat(nm, default_buf , BUF_SIZE);
		break;
	}

	switch (msm_pm_get_l2_cache(&(supp->rs_limits))) {
	case MSM_PM(L2_CACHE_HSFS_OPEN):
		strlcat(nm, "L2: HSFS ", BUF_SIZE);
		break;
	case MSM_PM(L2_CACHE_GDHS):
		strlcat(nm, "L2: GDHS ", BUF_SIZE);
		break;
	case MSM_PM(L2_CACHE_RETENTION):
		strlcat(nm, "L2: Retention ", BUF_SIZE);
		break;
	case MSM_PM(L2_CACHE_ACTIVE):
		strlcat(nm, "L2: Active ", BUF_SIZE);
		break;
	default:
		snprintf(default_buf, sizeof(default_buf),
			"L2 : %d ", msm_pm_get_l2_cache(&(supp->rs_limits)));
		strlcat(nm, default_buf , BUF_SIZE);
		break;
	}

	snprintf(default_buf, sizeof(default_buf),
		"Vdd_mem : %d ", msm_pm_get_vdd_mem(&(supp->rs_limits)));
	strlcat(nm, default_buf , BUF_SIZE);

	snprintf(default_buf, sizeof(default_buf),
		"Vdd_dig : %d ", msm_pm_get_vdd_dig(&(supp->rs_limits)));
	strlcat(nm, default_buf , BUF_SIZE);

	strlcpy(stat->level_name, nm, strnlen(nm, BUF_SIZE));
}

static int64_t msm_lpm_get_time(void)
{
	if (msm_lpm_use_qtimer)
		return ktime_to_ns(ktime_get());

	return msm_timer_get_sclk_time(NULL);
}

static bool lpm_get_level(void *v, unsigned int *ct)
{
	bool ret = false;
	int it;
	struct msm_rpmrs_level *level_enter;

	level_enter = container_of(((struct msm_lpm_sleep_data *)v)->limits,
			struct msm_rpmrs_level, rs_limits);
	if (level_enter) {
		for (it = 0; it < lpm_level_count; it++)
			if (!memcmp(level_enter , lpm_supp_level + it,
					sizeof(struct msm_rpmrs_level))) {
				*ct = it;
				ret = true;
				break;
			}
	}
	return ret;
}

static int lpm_callback(struct notifier_block *self, unsigned long cmd,
				void *sleep_data)
{
	static int64_t time;
	unsigned int ct;
	struct lpm_level_stat *stats;
	stats = per_cpu(lpm_levels, cpu_to_debug);
	/* Update the stats and get the start/stop time */
	if (cmd == MSM_LPM_STATE_ENTER && !lpm_latency_test) {
		time = msm_lpm_get_time();
		stats[lpm_level_iter].entered = true;
	} else if ((cmd == MSM_LPM_STATE_EXIT) && (time)
			&& (!lpm_latency_test)) {
		int64_t time1;
		time1 = msm_lpm_get_time();
		time = time1 - time;

		if ((time < stats[lpm_level_iter].min_time) ||
			(!stats[lpm_level_iter].min_time))
			stats[lpm_level_iter].min_time = time;

		if (time > stats[lpm_level_iter].max_time)
			stats[lpm_level_iter].max_time = time;

		time1 = stats[lpm_level_iter].avg_time *
			stats[lpm_level_iter].count + time;
		do_div(time1, ++(stats[lpm_level_iter].count));

		stats[lpm_level_iter].avg_time = time1;
		do_div(time, NSEC_PER_USEC);
		if (time < lpm_supp_level[lpm_level_iter].
				time_overhead_us)
			stats[lpm_level_iter].exit_early++;
		time = 0;
	} else if (cmd == MSM_LPM_STATE_ENTER && lpm_latency_test) {

		struct msm_lpm_sleep_data *data = sleep_data;
		if ((lpm_get_level(sleep_data, &ct)) &&
		(stats[ct].min_threshold == 0) &&
		data->kernel_sleep <= lpm_sleep_time) {

			stats[ct].min_threshold = lpm_sleep_time;
			stats[ct].kernel_sleep_time =
				data->kernel_sleep;
		}
	}
	return 0;
}

static struct notifier_block lpm_idle_nb = {
	.notifier_call = lpm_callback,
};

static void lpm_test_initiate(int lpm_level_test)
{
	int test_ret;

	/* This will communicate to 'stat' debugfs to skip latency printing*/
	lpm_sleep_time = 0;
	lpm_latency_test = false;
	/* Unregister any infinitely registered level*/
	msm_lpm_unregister_notifier(cpu_to_debug, &lpm_idle_nb);

	/* Register/Unregister for Notification */
	while (lpm_level_iter < lpm_level_count) {
		test_ret = msm_lpm_register_notifier(cpu_to_debug,
				lpm_level_iter, &lpm_idle_nb, false);
		if (test_ret < 0) {
			pr_err("%s: Registering notifier failed\n", __func__);
			return;
		}
		if (!timer_interval)
			break;
		msleep(timer_interval);
		msm_lpm_unregister_notifier(cpu_to_debug, &lpm_idle_nb);
		if (lpm_level_test == lpm_level_count)
			lpm_level_iter++;
		else
			break;
	}
}

static void lpm_latency_test_initiate(unsigned long max_time)
{
	int test_ret;
	lpm_latency_test = true;
	lpm_sleep_time = latency_test_interval;

	msm_lpm_unregister_notifier(cpu_to_debug, &lpm_idle_nb);
	if (max_time > lpm_sleep_time) {

		do {
			test_ret = msm_lpm_register_notifier(cpu_to_debug,
					lpm_level_count + 1,
					&lpm_idle_nb, true);
			if (test_ret) {
				pr_err("%s: Registering notifier failed\n",
						__func__);
				return;
			}
			usleep(lpm_sleep_time);
			/*Unregister to ensure that we dont update the latency
			during the timer value transistion*/
			msm_lpm_unregister_notifier(cpu_to_debug,
				&lpm_idle_nb);
			lpm_sleep_time += latency_test_interval;
		} while (lpm_sleep_time < max_time);
	} else
		pr_err("%s: Invalid time interval specified\n", __func__);

	lpm_latency_test = false;
}

static ssize_t lpm_test_comm_read(struct file *fp, char __user *user_buffer,
				size_t buffer_length, loff_t *position)
{
	int i = 0;
	int count = buffer_length;
	int alloc_size = 100 * lpm_level_count;
	char *temp_buf;
	char *comm_buf;
	ssize_t ret;

	comm_buf = kzalloc(alloc_size, GFP_KERNEL);
	if (!comm_buf) {
		pr_err("%s:Memory alloc failed\n", __func__);
		ret = 0;
		goto com_read_failed;
	}
	temp_buf = comm_buf;

	SNPRINTF(temp_buf, count, "Low power modes available:\n");

	for (i = 0; i < lpm_level_count; i++)
		SNPRINTF(temp_buf, count, "%d. %s\n", i,
			per_cpu(lpm_levels, cpu_to_debug)[i].level_name);

	SNPRINTF(temp_buf, count, "%d. MSM test all lpm\n", i++);
	SNPRINTF(temp_buf, count, "%d. MSM determine latency\n", i);

	ret = simple_read_from_buffer(user_buffer, buffer_length - count,
					position, comm_buf, alloc_size);
	kfree(comm_buf);

com_read_failed:
	return ret;
}

char *trimspaces(char *time_buf)
{
	int len;
	char *tail;

	len = strnlen(time_buf, INPUT_COUNT_BUF);
	tail = time_buf + len;
	while (isspace(*time_buf) && (time_buf != tail))
		time_buf++;
	if (time_buf == tail) {
		time_buf = NULL;
		goto exit_trim_spaces;
	}
	len = strnlen(time_buf, INPUT_COUNT_BUF);
	tail = time_buf + len - 1;
	while (isspace(*tail) && tail != time_buf) {
		*tail = '\0';
		tail--;
	}
exit_trim_spaces:
	return time_buf;
}

static ssize_t lpm_test_comm_write(struct file *fp, const char __user
			*user_buffer, size_t count, loff_t *position)
{
	ssize_t ret;
	int str_ret;
	int lpm_level_test;
	char *new_ptr;
	char *comm_buf;

	comm_buf = kzalloc(COMM_BUF_SIZE, GFP_KERNEL);
	if (!comm_buf) {
		pr_err("\'%s\': kzalloc failed\n", __func__);
		return -EINVAL;
	}

	memset(comm_buf, '\0', COMM_BUF_SIZE);

	ret = simple_write_to_buffer(comm_buf, COMM_BUF_SIZE, position,
					user_buffer, count);
	new_ptr = trimspaces(comm_buf);
	if (!new_ptr) {
		pr_err("%s: Test case number input invalid\n", __func__);
		goto write_com_failed;
	}

	if (!memcmp(comm_buf, LPM_TEST_ALL_LEVELS,
			sizeof(LPM_TEST_ALL_LEVELS) - 1)) {
		lpm_level_test = lpm_level_count;
		lpm_level_iter = 0;
		lpm_test_initiate(lpm_level_test);
		goto write_com_success;
	} else if (!memcmp(comm_buf, LPM_TEST_LATENCIES,
			sizeof(LPM_TEST_LATENCIES) - 1)) {
		lpm_level_test = lpm_level_count + 1;
		lpm_latency_test_initiate(timer_interval * USEC_PER_MSEC);
		goto write_com_success;
	} else if (!memcmp(comm_buf, LPM_TEST_CLEAR,
			sizeof(LPM_TEST_CLEAR) - 1)) {
		msm_lpm_unregister_notifier(cpu_to_debug, &lpm_idle_nb);
		goto write_com_success;
	}

	str_ret = kstrtoint(new_ptr, 10, &lpm_level_test);
	if ((str_ret) || (lpm_level_test > (lpm_level_count + 1)) ||
		(lpm_level_test < 0))
		goto write_com_failed;

	lpm_level_iter = lpm_level_test;
	lpm_test_initiate(lpm_level_test);
	goto write_com_success;

write_com_failed:
	ret = -EINVAL;
write_com_success:
	kfree(comm_buf);
	return ret;
}

static ssize_t lpm_test_stat_read(struct file *fp, char __user *user_buffer,
				size_t buffer_length, loff_t *position)
{
	int i = 0;
	int j = 0;
	int count = buffer_length;
	char *stat_buf;
	char *stat_buf_start;
	size_t stat_buf_size;
	ssize_t ret;
	int64_t min_ns;
	int64_t max_ns;
	int64_t avg_ns;
	uint32_t min_ms;
	uint32_t max_ms;
	uint32_t avg_ms;

	stat_buf_size = ((sizeof(struct lpm_level_stat) * lpm_level_count) +
				STAT_BUF_EXTRA_SIZE);
	stat_buf = kzalloc(stat_buf_size, GFP_KERNEL);
	if (!stat_buf) {
		pr_err("\'%s\': kzalloc failed\n", __func__);
		return -EINVAL;
	}
	stat_buf_start = stat_buf;
	mutex_lock(&lpm_stats_mutex);
	memset(stat_buf, '\0', stat_buf_size);
	SNPRINTF(stat_buf, count, "\n\nStats for CPU: %d\nTotal Levels: %d\n",
			cpu_to_debug, lpm_level_count);
	if (!lpm_sleep_time) {
		SNPRINTF(stat_buf, count, "Level(s) failed: ");
		for (i = 0 ; i < lpm_level_count; i++) {
			if (per_cpu(lpm_levels, cpu_to_debug)[i].entered)
				continue;
			else {
				SNPRINTF(stat_buf, count,
					"\n%d. %s", ++j, per_cpu(lpm_levels,
					cpu_to_debug)[i].level_name);
			}
		}
		SNPRINTF(stat_buf, count, "\n\nSTATS:");
		for (i = 0; i < lpm_level_count; i++) {
			min_ns = per_cpu(lpm_levels, cpu_to_debug)[i].min_time;
			min_ms = do_div(min_ns, NSEC_PER_MSEC);
			max_ns = per_cpu(lpm_levels, cpu_to_debug)[i].max_time;
			max_ms = do_div(max_ns, NSEC_PER_MSEC);
			avg_ns = per_cpu(lpm_levels, cpu_to_debug)[i].avg_time;
			avg_ms = do_div(avg_ns, NSEC_PER_MSEC);
			SNPRINTF(stat_buf, count, "\nLEVEL: %s\n"
				"Entered : %lld\n"
				"Early wakeup : %lld\n"
				"Min Time (mSec): %lld.%06u\n"
				"Max Time (mSec): %lld.%06u\n"
				"Avg Time (mSec): %lld.%06u\n",
				per_cpu(lpm_levels, cpu_to_debug)[i].level_name,
				per_cpu(lpm_levels, cpu_to_debug)[i].count,
				per_cpu(lpm_levels, cpu_to_debug)[i].exit_early,
				min_ns, min_ms,
				max_ns, max_ms,
				avg_ns, avg_ms);
		}
	} else {
		for (i = 0; i < lpm_level_count; i++) {
			SNPRINTF(stat_buf, count, "\nLEVEL: %s\n"
				"Min Timer value (uSec): %lu\n"
				"Kernel sleep time (uSec): %u\n",
				per_cpu(lpm_levels, cpu_to_debug)[i].level_name,
				per_cpu(lpm_levels, cpu_to_debug)[i].
				min_threshold,
				per_cpu(lpm_levels,
				cpu_to_debug)[i].kernel_sleep_time);
		}
	}

	ret = simple_read_from_buffer(user_buffer, buffer_length - count,
				position, stat_buf_start, stat_buf_size);

	mutex_unlock(&lpm_stats_mutex);
	kfree(stat_buf_start);
	return ret;
}

static ssize_t lpm_test_stat_write(struct file *fp, const char __user
				*user_buffer, size_t count, loff_t *position)
{
	char buf[sizeof(LPM_STATS_RESET)];
	int ret;
	int i;
	struct lpm_level_stat *stats;

	if (count > sizeof(LPM_STATS_RESET)) {
		ret = -EINVAL;
		goto write_debug_failed;
	}

	simple_write_to_buffer(buf, sizeof(LPM_STATS_RESET), position,
				user_buffer, count);

	if (memcmp(buf, LPM_STATS_RESET, sizeof(LPM_STATS_RESET) - 1)) {
		ret = -EINVAL;
		goto write_debug_failed;
	}

	mutex_lock(&lpm_stats_mutex);
	stats = per_cpu(lpm_levels, cpu_to_debug);
	for (i = 0 ; i < lpm_level_count; i++) {
		stats[i].entered = 0;
		stats[i].min_time = 0;
		stats[i].max_time = 0;
		stats[i].avg_time = 0;
		stats[i].count = 0;
		stats[i].exit_early = 0;
		stats[i].min_threshold = 0;
		stats[i].kernel_sleep_time = 0;
	}
	mutex_unlock(&lpm_stats_mutex);
	return count;
write_debug_failed:
	return ret;
}

static void lpm_init_rpm_levels(int test_lpm_level_count,
		struct msm_rpmrs_level *test_levels)
{
	int i = 0;
	unsigned int m_cpu = 0;
	struct lpm_level_stat *stat_levels = NULL;

	if (test_lpm_level_count < 0)
		return;

	lpm_level_count = test_lpm_level_count;

	lpm_supp_level = test_levels;
	for_each_possible_cpu(m_cpu) {
		stat_levels = kzalloc(sizeof(struct lpm_level_stat) *
				lpm_level_count, GFP_KERNEL);
		if (!stat_levels) {
			for (i = m_cpu - 1; i >= 0; i--)
				kfree(per_cpu(lpm_levels, i));
			return;
		}

		for (i = 0; i < lpm_level_count; i++)
			lpm_populate_name(&stat_levels[i], &lpm_supp_level[i]);

		per_cpu(lpm_levels, m_cpu) = stat_levels;
	}
}

static const struct file_operations fops_stat = {
	.read = lpm_test_stat_read,
	.write = lpm_test_stat_write,
};

static const struct file_operations fops_comm = {
	.read = lpm_test_comm_read,
	.write = lpm_test_comm_write,
};

static int lpm_test_init(int test_lpm_level_count,
		struct msm_rpmrs_level *test_levels)
{
	int filevalue;
	int lpm_comm;
	int ret = -EINVAL;
	struct dentry *parent_dir = NULL;

	parent_dir = debugfs_create_dir("msm_lpm_debug", NULL);
	if (!parent_dir) {
		pr_err("%s: debugfs directory creation failed\n",
				__func__);
		goto init_err;
	}

	lpm_stat = debugfs_create_file("stat",
			S_IRUGO | S_IWUSR | S_IWGRP, parent_dir,
			&filevalue, &fops_stat);
	if (!lpm_stat) {
		pr_err("%s: lpm_stats debugfs creation failed\n",
				__func__);
		goto init_err;
	}

	lpm_ext_comm = debugfs_create_file("comm",
			S_IRUGO | S_IWUSR | S_IWGRP, parent_dir, &lpm_comm,
			&fops_comm);
	if (!lpm_ext_comm) {
		pr_err("%s: lpm_comm debugfs creation failed\n",
			__func__);
		debugfs_remove(lpm_stat);
		goto init_err;
	}

	/*Query RPM resources and allocate the data sturctures*/
	lpm_init_rpm_levels(test_lpm_level_count, test_levels);
	ret = 0;

init_err:
	return ret;
}

static int  lpm_test_exit(struct platform_device *pdev)
{
	unsigned int m_cpu = 0;

	kfree(lpm_supp_level);
	for_each_possible_cpu(m_cpu)
		kfree(per_cpu(lpm_levels, m_cpu));
	debugfs_remove(lpm_stat);
	debugfs_remove(lpm_ext_comm);
	return 0;
}

static int lpm_test_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct lpm_test_platform_data *pdata;
	struct msm_rpmrs_level *test_levels;
	int test_lpm_level_count;

	pdata = pdev->dev.platform_data;

	if (!pdata) {
		dev_err(dev, "no platform data specified\n");
		return -EINVAL;
	}

	test_levels = pdata->msm_lpm_test_levels;
	test_lpm_level_count = pdata->msm_lpm_test_level_count;

	if (pdata->use_qtimer)
		msm_lpm_use_qtimer = true;

	lpm_test_init(test_lpm_level_count, test_levels);

	return 0;
}

static struct platform_driver lpm_test_driver = {
	.probe = lpm_test_probe,
	.remove = lpm_test_exit,
	.driver = {
		.name = "lpm_test",
		.owner = THIS_MODULE,
	},
};

static int __init lpm_test_platform_driver_init(void)
{
	return platform_driver_register(&lpm_test_driver);
}

late_initcall(lpm_test_platform_driver_init);
