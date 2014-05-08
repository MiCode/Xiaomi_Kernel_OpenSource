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
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <soc/qcom/spm.h>
#include <soc/qcom/pm.h>

static struct dentry *msm_pm_dbg_root;
static struct dentry *msm_pm_dentry[NR_CPUS];

struct pm_debugfs_private_data {
	struct msm_pm_time_stats *stats;
	unsigned int cpu;
	unsigned int stats_id;
};

static DEFINE_PER_CPU_SHARED_ALIGNED(
	struct pm_debugfs_private_data, msm_pm_debugfs_private_data);

static DEFINE_PER_CPU(
	struct pm_debugfs_private_data, msm_pm_cpu_states[MSM_PM_STAT_COUNT]);

static struct pm_debugfs_private_data all_stats_private_data;

struct pm_debugfs_private_data msm_pm_suspend_states_data;

struct msm_pm_time_stats {
	const char *name;
	int64_t first_bucket_time;
	int bucket[CONFIG_MSM_IDLE_STATS_BUCKET_COUNT];
	int64_t min_time[CONFIG_MSM_IDLE_STATS_BUCKET_COUNT];
	int64_t max_time[CONFIG_MSM_IDLE_STATS_BUCKET_COUNT];
	int count;
	int64_t total_time;
	bool enabled;
	int sleep_mode;
};

static struct msm_pm_time_stats suspend_stats;
struct msm_pm_cpu_time_stats {
	struct msm_pm_time_stats stats[MSM_PM_STAT_COUNT];
};
struct pm_l2_debugfs_private_data {
	char *buf;
	unsigned int stats_id;
};

struct _msm_pm_l2_time_stats {
	struct msm_pm_time_stats stats[MSM_SPM_MODE_NR];
};
enum stats_type {
	MSM_PM_STATS_TYPE_CPU,
	MSM_PM_STATS_TYPE_SUSPEND,
	MSM_PM_STATS_TYPE_L2,
};
#define BUF_LEN 64
static DEFINE_MUTEX(msm_pm_stats_mutex);
static DEFINE_SPINLOCK(msm_pm_stats_lock);
static DEFINE_PER_CPU_SHARED_ALIGNED(
	struct msm_pm_cpu_time_stats, msm_pm_stats);
static DEFINE_SPINLOCK(msm_pm_l2_stats_lock);
static struct _msm_pm_l2_time_stats msm_pm_l2_time_stats;
static struct pm_l2_debugfs_private_data l2_stats_private_data[] = {
	{NULL, MSM_SPM_MODE_DISABLED},
	{NULL, MSM_SPM_MODE_RETENTION},
	{NULL, MSM_SPM_MODE_GDHS},
	{NULL, MSM_SPM_MODE_POWER_COLLAPSE},
	{NULL, MSM_SPM_MODE_NR},
};

/*
 *  Function to update stats
 */
static void update_stats(struct msm_pm_time_stats *stats, int64_t t)
{
	int64_t bt;
	int i;

	if (!stats)
		return;

	stats->total_time += t;
	stats->count++;

	bt = t;
	do_div(bt, stats->first_bucket_time);

	if (bt < 1ULL << (CONFIG_MSM_IDLE_STATS_BUCKET_SHIFT *
			(CONFIG_MSM_IDLE_STATS_BUCKET_COUNT - 1)))
		i = DIV_ROUND_UP(fls((uint32_t)bt),
			CONFIG_MSM_IDLE_STATS_BUCKET_SHIFT);
	else
		i = CONFIG_MSM_IDLE_STATS_BUCKET_COUNT - 1;

	if (i >= CONFIG_MSM_IDLE_STATS_BUCKET_COUNT)
		i = CONFIG_MSM_IDLE_STATS_BUCKET_COUNT - 1;

	stats->bucket[i]++;

	if (t < stats->min_time[i] || !stats->max_time[i])
		stats->min_time[i] = t;
	if (t > stats->max_time[i])
		stats->max_time[i] = t;
}

/*
 * Add the given time data to the statistics collection.
 */
void msm_pm_add_stat(enum msm_pm_time_stats_id id, int64_t t)
{
	struct msm_pm_time_stats *stats;
	unsigned long flags;

	spin_lock_irqsave(&msm_pm_stats_lock, flags);
	if (id == MSM_PM_STAT_SUSPEND) {
		stats = &suspend_stats;
	} else {
		stats = __get_cpu_var(msm_pm_stats).stats;
		if (!stats[id].enabled)
			goto add_bail;
		stats = &stats[id];
	}
	update_stats(stats, t);
add_bail:
	spin_unlock_irqrestore(&msm_pm_stats_lock, flags);
}
void msm_pm_l2_add_stat(uint32_t id, int64_t t)
{
	unsigned long flags;
	struct msm_pm_time_stats *stats;

	if (id == MSM_SPM_MODE_DISABLED || id >= MSM_SPM_MODE_NR)
		return;

	spin_lock_irqsave(&msm_pm_l2_stats_lock, flags);

	stats = msm_pm_l2_time_stats.stats;
	stats = &stats[id];
	update_stats(stats, t);

	spin_unlock_irqrestore(&msm_pm_l2_stats_lock, flags);
}

static void stats_show(struct seq_file *m,
		struct msm_pm_time_stats *stats,
		int cpu, enum stats_type type)
{
	int64_t bucket_time;
	int64_t s;
	uint32_t ns;
	int i;
	char str[BUF_LEN];
	int bucket_count = CONFIG_MSM_IDLE_STATS_BUCKET_COUNT - 1;
	int bucket_shift = CONFIG_MSM_IDLE_STATS_BUCKET_SHIFT;

	if (!stats || !m)
		return;

	s = stats->total_time;
	ns = do_div(s, NSEC_PER_SEC);
	switch (type) {

	case MSM_PM_STATS_TYPE_CPU:
		snprintf(str , BUF_LEN, "[cpu %u] %s", cpu, stats->name);
		break;

	case MSM_PM_STATS_TYPE_SUSPEND:
		snprintf(str , BUF_LEN, "%s", stats->name);
		break;
	case MSM_PM_STATS_TYPE_L2:
		snprintf(str , BUF_LEN, "[L2] %s", stats->name);
		break;
	default:
		pr_err(" stats type error\n");
		return;
	}
	seq_printf(m,	"%s:\n"
			"  count: %7d\n"
			"  total_time: %lld.%09u\n",
			str,
			stats->count,
			s, ns);

	bucket_time = stats->first_bucket_time;
	for (i = 0; i < bucket_count; i++) {
		s = bucket_time;
		ns = do_div(s, NSEC_PER_SEC);
		seq_printf(m, "   <%6lld.%09u: %7d (%lld-%lld)\n",
			s, ns, stats->bucket[i],
			stats->min_time[i],
			stats->max_time[i]);
			bucket_time <<= bucket_shift;
	}

	seq_printf(m, "  >=%6lld.%09u: %7d (%lld-%lld)\n",
		s, ns, stats->bucket[i],
		stats->min_time[i],
		stats->max_time[i]);
}

/*
 * Write out the power management statistics.
 */

static int msm_pm_stats_show(struct seq_file *m, void *v)
{
	int cpu;
	unsigned long flags;

	spin_lock_irqsave(&msm_pm_stats_lock, flags);
	for_each_possible_cpu(cpu) {
		struct msm_pm_time_stats *stats;
		int id;

		stats = per_cpu(msm_pm_stats, cpu).stats;

		for (id = 0; id < MSM_PM_STAT_COUNT; id++) {
			/* Skip the disabled ones */
			if (!stats[id].enabled)
				continue;

			if (id == MSM_PM_STAT_SUSPEND)
				continue;

			stats_show(m, &stats[id], cpu, MSM_PM_STATS_TYPE_CPU);
		}
	}
	stats_show(m, &suspend_stats, cpu, true);
	spin_unlock_irqrestore(&msm_pm_stats_lock, flags);
	return 0;
}

#define MSM_PM_STATS_RESET "reset"
/*
 * Reset the power management statistics values.
 */
static ssize_t msm_pm_write_proc(struct file *file, const char __user *buffer,
	size_t count, loff_t *off)
{
	char buf[sizeof(MSM_PM_STATS_RESET)];
	int ret;
	unsigned long flags;
	unsigned int cpu;
	size_t len = strnlen(MSM_PM_STATS_RESET, sizeof(MSM_PM_STATS_RESET));

	if (count < sizeof(MSM_PM_STATS_RESET)) {
		ret = -EINVAL;
		goto write_proc_failed;
	}

	if (copy_from_user(buf, buffer, len)) {
		ret = -EFAULT;
		goto write_proc_failed;
	}

	if (strncmp(buf, MSM_PM_STATS_RESET, len)) {
		ret = -EINVAL;
		goto write_proc_failed;
	}

	spin_lock_irqsave(&msm_pm_stats_lock, flags);
	for_each_possible_cpu(cpu) {
		struct msm_pm_time_stats *stats;
		int i;

		stats = per_cpu(msm_pm_stats, cpu).stats;
		for (i = 0; i < MSM_PM_STAT_COUNT; i++) {
			memset(stats[i].bucket,
				0, sizeof(stats[i].bucket));
			memset(stats[i].min_time,
				0, sizeof(stats[i].min_time));
			memset(stats[i].max_time,
				0, sizeof(stats[i].max_time));
			stats[i].count = 0;
			stats[i].total_time = 0;
		}
	}

	spin_unlock_irqrestore(&msm_pm_stats_lock, flags);
	return count;

write_proc_failed:
	return ret;
}
#undef MSM_PM_STATS_RESET
static size_t read_cpu_state_stats(struct seq_file *m,
		struct pm_debugfs_private_data *private_data)
{
	struct msm_pm_time_stats *stats = NULL;
	unsigned int id;
	unsigned int cpu = 0;
	unsigned long flags;

	if (private_data == NULL || !private_data->stats)
		return 0;

	stats = private_data->stats;
	cpu = private_data->cpu;
	id = private_data->stats_id;

	spin_lock_irqsave(&msm_pm_stats_lock, flags);

	if (id == MSM_PM_STAT_SUSPEND)
		stats_show(m, &suspend_stats, cpu, MSM_PM_STATS_TYPE_SUSPEND);
	else
		stats_show(m, &stats[id], cpu, MSM_PM_STATS_TYPE_CPU);

	spin_unlock_irqrestore(&msm_pm_stats_lock, flags);
	return 0;
}

static size_t read_cpu_stats(struct seq_file *m,
		struct pm_debugfs_private_data *private_data,
		unsigned int cpu)
{
	struct msm_pm_time_stats *stats = NULL;
	unsigned int id;
	unsigned long flags;

	if (private_data == NULL || !private_data->stats)
		return 0;

	stats = private_data->stats;

	spin_lock_irqsave(&msm_pm_stats_lock, flags);

	for (id = 0; id < MSM_PM_STAT_COUNT; id++) {
		int mode, idx;

		if (!stats[id].enabled || id == MSM_PM_STAT_SUSPEND)
			continue;

		mode = stats[id].sleep_mode;
		idx = MSM_PM_MODE(cpu, mode);

		if (!msm_pm_sleep_mode_supported(cpu, mode, true) &&
				!msm_pm_sleep_mode_supported(cpu, mode, false))
			continue;
		stats_show(m, &stats[id], cpu, MSM_PM_STATS_TYPE_CPU);
	}
	spin_unlock_irqrestore(&msm_pm_stats_lock, flags);

	return 0;
}

static int msm_pm_stat_file_show(struct seq_file *m, void *v)
{
	unsigned int cpu = 0;
	static struct pm_debugfs_private_data *private_data;
	enum msm_pm_time_stats_id stats_id = MSM_PM_STAT_COUNT;

	if (!m->private)
		return 0;

	private_data = m->private;

	if (num_possible_cpus() == private_data->cpu) {
		/* statistics of all the cpus to be printed */
		unsigned int i;
		for (i = 0; i < num_possible_cpus(); i++) {
			private_data = &per_cpu(msm_pm_debugfs_private_data, i);
			read_cpu_stats(m, private_data, i);
		}
		stats_show(m, &suspend_stats, cpu, MSM_PM_STATS_TYPE_SUSPEND);
	} else {
		/* only current cpu statistics has to be printed */
		cpu = private_data->cpu;
		stats_id = private_data->stats_id;
		if (private_data->stats_id == MSM_PM_STAT_COUNT) {
			/* Read all the status for the CPU */
			read_cpu_stats(m, private_data, cpu);

		} else {

			if (private_data == NULL)
				return 0;

			read_cpu_state_stats(m, private_data);
		}
	}
	return 0;
}


static int msm_pm_stat_file_open(struct inode *inode, struct file *file)
{
	return single_open(file, msm_pm_stat_file_show, inode->i_private);
}


static const struct file_operations msm_pm_stat_fops = {
	.owner	  = THIS_MODULE,
	.open	  = msm_pm_stat_file_open,
	.read	  = seq_read,
	.release  = single_release,
	.llseek   = no_llseek,
};
static int msm_pm_l2_stat_file_show(struct seq_file *m, void *v)
{
	struct msm_pm_time_stats *stats = NULL;
	unsigned int id;
	static struct pm_l2_debugfs_private_data *private_data;

	if (!m->private)
		return 0;

	private_data = m->private;
	stats = msm_pm_l2_time_stats.stats;

	if (private_data->stats_id == MSM_SPM_MODE_NR) {
		/* All stats print */
		for (id = 1; id < MSM_SPM_MODE_NR; id++)
			stats_show(m, &stats[id], 0, MSM_PM_STATS_TYPE_L2);
	} else {
		/* individual status print */
		id = private_data->stats_id;
		stats_show(m, &stats[id], 0, MSM_PM_STATS_TYPE_L2);
	}
	return 0;
}

static int msm_pm_l2_stat_file_open(struct inode *inode, struct file *file)
{
	return single_open(file, msm_pm_l2_stat_file_show, inode->i_private);
}

static const struct file_operations msm_pm_l2_stat_fops = {
	.owner	  = THIS_MODULE,
	.open	  = msm_pm_l2_stat_file_open,
	.read	  = seq_read,
	.release  = single_release,
	.llseek   = no_llseek,
};

static bool msm_pm_debugfs_create_l2(void)
{
	struct msm_pm_time_stats *stats = msm_pm_l2_time_stats.stats;
	struct dentry *msm_pm_l2_root;
	uint32_t stat_id;

	msm_pm_l2_root = debugfs_create_dir("l2", msm_pm_dbg_root);
	if (!msm_pm_l2_root)
		return false;

	stats[MSM_SPM_MODE_GDHS].name = "GDHS";
	stats[MSM_SPM_MODE_GDHS].first_bucket_time =
		CONFIG_MSM_IDLE_STATS_FIRST_BUCKET;

	stats[MSM_SPM_MODE_RETENTION].name = "Retention";
	stats[MSM_SPM_MODE_RETENTION].first_bucket_time =
		CONFIG_MSM_IDLE_STATS_FIRST_BUCKET;

	stats[MSM_SPM_MODE_POWER_COLLAPSE].name = "PC";
	stats[MSM_SPM_MODE_POWER_COLLAPSE].first_bucket_time =
		CONFIG_MSM_SUSPEND_STATS_FIRST_BUCKET;

	for (stat_id = 1; stat_id < MSM_SPM_MODE_NR; stat_id++) {
		if (!stats[stat_id].name)
			continue;
		if (!debugfs_create_file(
			stats[stat_id].name,
			S_IRUGO, msm_pm_l2_root,
			(void *)&l2_stats_private_data[stat_id],
			&msm_pm_l2_stat_fops)) {
			goto l2_err;
		}
	}
	stat_id = MSM_SPM_MODE_NR;
	if (!debugfs_create_file("stats",
		S_IRUGO, msm_pm_l2_root,
		(void *)&l2_stats_private_data[stat_id],
		&msm_pm_l2_stat_fops)) {
		goto l2_err;
	}

	return true;
l2_err:
	debugfs_remove(msm_pm_l2_root);
	return false;
}


static bool msm_pm_debugfs_create_root(void)
{
	bool ret = false;

	msm_pm_dbg_root = debugfs_create_dir("msm_pm_stats", NULL);
	if (!msm_pm_dbg_root)
		goto root_error;

	/* create over all stats file */
	all_stats_private_data.cpu = num_possible_cpus();
	all_stats_private_data.stats_id = MSM_PM_STAT_COUNT;
	if (!debugfs_create_file("stats",
		S_IRUGO, msm_pm_dbg_root, &all_stats_private_data,
		&msm_pm_stat_fops)) {
		debugfs_remove(msm_pm_dbg_root);
		goto root_error;
	}
	ret = true;

root_error:
	return ret;
}
static int msm_pm_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, msm_pm_stats_show, NULL);
}

static const struct file_operations msm_pm_stats_fops = {
	.open		= msm_pm_stats_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.write		= msm_pm_write_proc,
};

void msm_pm_add_stats(enum msm_pm_time_stats_id *enable_stats, int size)
{
	unsigned int cpu;
	struct proc_dir_entry *d_entry;
	int i = 0;
	char cpu_name[8];
	bool root_avl = false;

	root_avl = msm_pm_debugfs_create_root();
	if (root_avl) {
		if (!msm_pm_debugfs_create_l2())
			pr_err(" L2 debugfs create error\n");
	}
	suspend_stats.name = "system_suspend";
	suspend_stats.first_bucket_time =
		CONFIG_MSM_SUSPEND_STATS_FIRST_BUCKET;

	msm_pm_suspend_states_data.stats = &suspend_stats;
	msm_pm_suspend_states_data.stats_id = MSM_PM_STAT_SUSPEND;
	if (!debugfs_create_file(suspend_stats.name,
				S_IRUGO, msm_pm_dbg_root,
				&msm_pm_suspend_states_data,
				&msm_pm_stat_fops))
		pr_err(" system_suspend debugfs create error\n");

	for_each_possible_cpu(cpu) {
		struct msm_pm_time_stats *stats =
			per_cpu(msm_pm_stats, cpu).stats;

		struct pm_debugfs_private_data *private_data =
			&per_cpu(msm_pm_debugfs_private_data, cpu);
		private_data->stats = stats;

		stats[MSM_PM_STAT_REQUESTED_IDLE].name = "idle-request";
		stats[MSM_PM_STAT_REQUESTED_IDLE].first_bucket_time =
			CONFIG_MSM_IDLE_STATS_FIRST_BUCKET;

		stats[MSM_PM_STAT_IDLE_SPIN].name = "idle-spin";
		stats[MSM_PM_STAT_IDLE_SPIN].first_bucket_time =
			CONFIG_MSM_IDLE_STATS_FIRST_BUCKET;

		stats[MSM_PM_STAT_IDLE_WFI].name = "idle-wfi";
		stats[MSM_PM_STAT_IDLE_WFI].first_bucket_time =
			CONFIG_MSM_IDLE_STATS_FIRST_BUCKET;

		stats[MSM_PM_STAT_IDLE_WFI].sleep_mode =
			MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT;

		stats[MSM_PM_STAT_RETENTION].name = "retention";
		stats[MSM_PM_STAT_RETENTION].first_bucket_time =
			CONFIG_MSM_IDLE_STATS_FIRST_BUCKET;
		stats[MSM_PM_STAT_RETENTION].sleep_mode =
			MSM_PM_SLEEP_MODE_RETENTION;

		stats[MSM_PM_STAT_IDLE_STANDALONE_POWER_COLLAPSE].name =
			"idle-standalone-power-collapse";
		stats[MSM_PM_STAT_IDLE_STANDALONE_POWER_COLLAPSE].
			first_bucket_time = CONFIG_MSM_IDLE_STATS_FIRST_BUCKET;
		stats[MSM_PM_STAT_IDLE_STANDALONE_POWER_COLLAPSE].sleep_mode =
			MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE;

		stats[MSM_PM_STAT_IDLE_FAILED_STANDALONE_POWER_COLLAPSE].name =
			"idle-failed-standalone-power-collapse";
		stats[MSM_PM_STAT_IDLE_FAILED_STANDALONE_POWER_COLLAPSE].
			first_bucket_time =
			CONFIG_MSM_IDLE_STATS_FIRST_BUCKET;

		stats[MSM_PM_STAT_IDLE_POWER_COLLAPSE].name =
			"idle-power-collapse";
		stats[MSM_PM_STAT_IDLE_POWER_COLLAPSE].first_bucket_time =
			CONFIG_MSM_IDLE_STATS_FIRST_BUCKET;
		stats[MSM_PM_STAT_IDLE_POWER_COLLAPSE].sleep_mode =
			MSM_PM_SLEEP_MODE_POWER_COLLAPSE;

		stats[MSM_PM_STAT_IDLE_FAILED_POWER_COLLAPSE].name =
			"idle-failed-power-collapse";
		stats[MSM_PM_STAT_IDLE_FAILED_POWER_COLLAPSE].
			first_bucket_time =
			CONFIG_MSM_IDLE_STATS_FIRST_BUCKET;
		stats[MSM_PM_STAT_NOT_IDLE].name = "not-idle";
		stats[MSM_PM_STAT_NOT_IDLE].first_bucket_time =
			CONFIG_MSM_IDLE_STATS_FIRST_BUCKET;

		for (i = 0; i < size; i++)
			stats[enable_stats[i]].enabled = true;

		if (root_avl) {
			struct dentry *msm_pm_dbg_core;
			/* create cpu directory */
			snprintf(cpu_name, sizeof(cpu_name), "cpu%u", cpu);
			msm_pm_dbg_core = debugfs_create_dir(cpu_name,
							 msm_pm_dbg_root);
			if (!msm_pm_dbg_core)
				continue;

			/* create per cpu stats file */
			private_data->cpu = cpu;
			private_data->stats_id = MSM_PM_STAT_COUNT;
			msm_pm_dentry[cpu] = debugfs_create_file("stats",
				S_IRUGO, msm_pm_dbg_core, private_data,
				&msm_pm_stat_fops);

			if (msm_pm_dentry[cpu]) {
				/* Create files related to individual states */
				int id = 0;
				struct dentry *handle;
				struct pm_debugfs_private_data
							*msm_pm_states_data;
				for (id = 0; id < MSM_PM_STAT_COUNT; id++) {

					if (stats[id].enabled != true ||
						id == MSM_PM_STAT_SUSPEND)
						continue;

					msm_pm_states_data =
					 &per_cpu(msm_pm_cpu_states[id], cpu);
					msm_pm_states_data->cpu = cpu;
					msm_pm_states_data->stats_id = id;
					msm_pm_states_data->stats = stats;
					handle = debugfs_create_file(
						stats[id].name,
						S_IRUGO, msm_pm_dbg_core,
						msm_pm_states_data,
						&msm_pm_stat_fops);
				}
			}
		}

	}
	d_entry = proc_create_data("msm_pm_stats", S_IRUGO | S_IWUSR | S_IWGRP,
			NULL, &msm_pm_stats_fops, NULL);
}
