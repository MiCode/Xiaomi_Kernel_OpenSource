/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/sysfs.h>
#include <linux/cpufreq.h>
#include <linux/jiffies.h>
#include <linux/percpu.h>
#include <linux/kobject.h>
#include <linux/spinlock.h>
#include <linux/notifier.h>
#include <linux/bitops.h>
#include <linux/stat.h>
#include <asm/cputime.h>
#include <linux/module.h>

#define MAX_CPUATTR_NAME_LEN (16)
#define MAX_CPU_STATS_LINE_LEN (64)

static spinlock_t cpufreq_stats_lock;
static unsigned int stats_collection_enabled = 1;
static const char *time_in_state_attr_name = "time_in_state";

struct cpu_persistent_stats {
	/* CPU name, e.g. cpu0 */
	char name[MAX_CPUATTR_NAME_LEN];
	/* the number identifying this CPU */
	unsigned int cpu_id;
	/*
	 * the last time stats were
	 * updated for this CPU
	 */
	unsigned long long last_time;
	/*
	 * the number of frequencies available
	 * to this CPU
	 */
	unsigned int max_state;
	/*
	 * the index corresponding to
	 * the frequency (from freq_table) that this CPU
	 * last ran at
	 */
	unsigned int last_index;
	/*
	 * the kobject corresponding to
	 * this CPU
	 */
	struct kobject cpu_persistent_stat_kobj;
	/*
	 * a table holding the cumulative time
	 * (in jiffies) spent by this CPU at
	 * frequency freq_table[i]
	 */
	cputime64_t *time_in_state;
	/*
	 * a table holding the frequencies this CPU
	 * can run at
	 */
	unsigned int *freq_table;
};

struct cpufreq_persistent_stats {
	char name[MAX_CPUATTR_NAME_LEN];
	struct kobject *persistent_stats_kobj;
} persistent_stats = {
	.name = "stats",
};

static DEFINE_PER_CPU(struct cpu_persistent_stats, pcpu_stats);

static struct attribute cpu_time_in_state_attr[NR_CPUS];

static int cpufreq_stats_update(unsigned int cpu)
{
	unsigned long long cur_time;
	struct cpu_persistent_stats *cpu_stats = &per_cpu(pcpu_stats, cpu);

	if (!stats_collection_enabled)
		return 0;

	if (cpu_stats->last_time) {
		cur_time = get_jiffies_64();

		if (likely(cpu_stats->time_in_state))
			cpu_stats->time_in_state[cpu_stats->last_index] =
				cpu_stats->time_in_state[cpu_stats->last_index]
				+ (cur_time - cpu_stats->last_time);

		cpu_stats->last_time = cur_time;
	}

	return 0;
}

static void reset_stats(void)
{
	unsigned int cpu;
	unsigned long irq_flags;
	struct cpu_persistent_stats *cpux_persistent_stats;

	spin_lock_irqsave(&cpufreq_stats_lock, irq_flags);

	for_each_possible_cpu(cpu) {
		cpux_persistent_stats = &per_cpu(pcpu_stats, cpu);

		if (cpux_persistent_stats->freq_table &&
				cpux_persistent_stats->time_in_state) {
			int i;

			for (i = 0; i < cpux_persistent_stats->max_state; i++)
				cpux_persistent_stats->time_in_state[i] = 0ULL;
		}
	}

	spin_unlock_irqrestore(&cpufreq_stats_lock, irq_flags);
}

static int freq_table_get_index(
		struct cpu_persistent_stats *stats,
		unsigned int freq)
{
	int index;
	for (index = 0; index < stats->max_state; index++)
		if (stats->freq_table[index] == freq)
			return index;
	return -ENOENT;
}

static void cpufreq_stats_free_table(unsigned int cpu)
{
	unsigned long irq_flags;
	struct cpu_persistent_stats *cpu_stats = &per_cpu(pcpu_stats, cpu);

	spin_lock_irqsave(&cpufreq_stats_lock, irq_flags);

	kfree(cpu_stats->time_in_state);
	cpu_stats->time_in_state = NULL;
	cpu_stats->freq_table = NULL;

	spin_unlock_irqrestore(&cpufreq_stats_lock, irq_flags);
}

static int cpufreq_stats_create_table(unsigned int cpu,
		struct cpufreq_policy *policy)
{
	unsigned int i, k, count = 0;
	unsigned int alloc_size;
	struct cpu_persistent_stats *cpu_stats = &per_cpu(pcpu_stats, cpu);
	unsigned long irq_flags;
	struct cpufreq_frequency_table *table =
		cpufreq_frequency_get_table(cpu);

	if (unlikely(!table))
		return -EINVAL;

	if (likely(cpu_stats->time_in_state))
		return -EBUSY;

	for (i = 0; table[i].frequency != CPUFREQ_TABLE_END; i++) {
		unsigned int freq = table[i].frequency;
		if (freq == CPUFREQ_ENTRY_INVALID)
			continue;
		count++;
	}

	alloc_size = count * sizeof(int) + count * sizeof(cputime64_t);
	cpu_stats->max_state = count;
	cpu_stats->time_in_state = kzalloc(alloc_size, GFP_KERNEL);

	if (!cpu_stats->time_in_state)
		return -ENOMEM;

	cpu_stats->freq_table =
		(unsigned int *)(cpu_stats->time_in_state + count);

	k = 0;
	for (i = 0; table[i].frequency != CPUFREQ_TABLE_END; i++) {
		unsigned int freq = table[i].frequency;
		if (freq == CPUFREQ_ENTRY_INVALID)
			continue;
		cpu_stats->freq_table[k++] = freq;
	}

	spin_lock_irqsave(&cpufreq_stats_lock, irq_flags);

	cpu_stats->last_time = get_jiffies_64();
	cpu_stats->last_index = freq_table_get_index(
			cpu_stats, policy->cur);

	spin_unlock_irqrestore(&cpufreq_stats_lock, irq_flags);

	return 0;
}

static int cpufreq_stat_notifier_trans(struct notifier_block *nb,
		unsigned long val, void *data)
{
	struct cpufreq_freqs *freq = data;
	struct cpufreq_policy *policy;
	static unsigned int table_created_count;
	struct cpu_persistent_stats *cpu_stats = &per_cpu(pcpu_stats,
		freq->cpu);
	unsigned long irq_flags;
	unsigned int cpu;

	if (val != CPUFREQ_POSTCHANGE)
		return 0;

	policy = cpufreq_cpu_get(freq->cpu);

	if (!policy)
		return 0;

	if (table_created_count < num_possible_cpus()) {
		for_each_cpu(cpu, policy->cpus) {
			if (cpufreq_stats_create_table(cpu, policy) == 0)
				table_created_count++;
		}
	}

	spin_lock_irqsave(&cpufreq_stats_lock, irq_flags);

	for_each_cpu(cpu, policy->cpus) {
		cpu_stats = &per_cpu(pcpu_stats, cpu);

		if (cpu_online(cpu)) {
			cpufreq_stats_update(cpu);
			cpu_stats->last_time = get_jiffies_64();
		}

		cpu_stats->last_index = freq_table_get_index(cpu_stats,
				freq->new);
	}

	spin_unlock_irqrestore(&cpufreq_stats_lock, irq_flags);
	cpufreq_cpu_put(policy);

	return 0;
}

static int __cpuinit cpufreq_stat_cpu_callback(struct notifier_block *nfb,
					       unsigned long action,
					       void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;
	struct cpu_persistent_stats *cpu_stats = &per_cpu(pcpu_stats, cpu);
	unsigned long flags;

	spin_lock_irqsave(&cpufreq_stats_lock, flags);

	switch (action) {
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		cpu_stats->last_time = get_jiffies_64();
		break;
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		cpufreq_stats_update(cpu);
		cpu_stats->last_time = 0;
		break;
	}

	spin_unlock_irqrestore(&cpufreq_stats_lock, flags);

	return NOTIFY_OK;
}

/************************** sysfs interface ************************/
static ssize_t show_cpu_time_in_state(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	/*
	 * The container of kobj is of type cpu_persistent_stats
	 * so we can use it to get the cpu ID corresponding to this attribute.
	 */
	int len = 0, i;
	unsigned long irq_flags;
	struct cpu_persistent_stats *cpux_persistent_stats =
		container_of(kobj, struct cpu_persistent_stats,
		cpu_persistent_stat_kobj);

	spin_lock_irqsave(&cpufreq_stats_lock, irq_flags);
	cpufreq_stats_update(cpux_persistent_stats->cpu_id);

	if (cpux_persistent_stats->time_in_state) {
		for (i = 0; i < cpux_persistent_stats->max_state; i++) {

			cputime64_t time_in_state =
				cpux_persistent_stats->time_in_state[i];
			s64 clocktime_in_state =
				cputime64_to_clock_t(time_in_state);

			len += snprintf(buf + len, MAX_CPU_STATS_LINE_LEN,
					"%u %lld\n",
					cpux_persistent_stats->freq_table[i],
					clocktime_in_state);
		}
	}

	spin_unlock_irqrestore(&cpufreq_stats_lock, irq_flags);

	return len;
}

static ssize_t store_reset(struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf, size_t count)
{
	int ret, input;

	ret = sscanf(buf, "%d", &input);

	if (ret < 0)
		return -EINVAL;

	if (input)
		reset_stats();

	return count;
}

static ssize_t show_enabled(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 2, "%u\n", !!stats_collection_enabled);
}

static ssize_t store_enable(struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf, size_t count)
{
	int ret, input;
	unsigned int cpu;
	unsigned long irq_flags;

	ret = sscanf(buf, "%d", &input);

	if (ret < 0)
		return -EINVAL;

	if (!!input == stats_collection_enabled)
		return count;

	if (input && !stats_collection_enabled) {
		spin_lock_irqsave(&cpufreq_stats_lock, irq_flags);

		for_each_possible_cpu(cpu) {
			if (per_cpu(pcpu_stats, cpu).last_time)
				per_cpu(pcpu_stats, cpu).last_time =
					get_jiffies_64();
		}

		spin_unlock_irqrestore(&cpufreq_stats_lock, irq_flags);
	}

	stats_collection_enabled = !!input;

	return count;
}

static const struct sysfs_ops cpu_time_in_state_ops = {
	.show = show_cpu_time_in_state,
};

static struct kobj_attribute reset_attr =
	__ATTR(reset, 0220, NULL, store_reset);

static struct kobj_attribute enable_attr =
	__ATTR(enable, S_IRUGO|S_IWUSR, show_enabled, store_enable);

static int create_persistent_stats_groups(void)
{
	unsigned int cpu_id;
	int ret;
	struct cpu_persistent_stats *cpu_stats;

	/* Create toplevel persistent stats kobject. */
	ret = cpufreq_get_global_kobject();

	if (ret)
		return ret;

	persistent_stats.persistent_stats_kobj =
		kobject_create_and_add(persistent_stats.name,
			cpufreq_global_kobject);

	if (!persistent_stats.persistent_stats_kobj) {
		pr_err("%s: Unable to create persistent_stats_kobj.\n",
				__func__);
		ret = -EINVAL;

		goto abort_stats_kobj_create_failed;
	}

	ret = sysfs_create_file(
			persistent_stats.persistent_stats_kobj,
			&enable_attr.attr);

	if (ret) {
		pr_err("%s: Unable to create enable_attr\n", __func__);

		goto abort_enable_attr_create_failed;
	}

	ret = sysfs_create_file(
			persistent_stats.persistent_stats_kobj,
			&reset_attr.attr);

	if (ret) {
		pr_err("%s: Unable to create reset_attr\n", __func__);

		goto abort_reset_attr_create_failed;
	}

	/* Create kobjects and add them to persistent_stats_kobj */
	for_each_possible_cpu(cpu_id) {
		memset(&cpu_time_in_state_attr[cpu_id],
			0, sizeof(struct attribute));

		cpu_stats = &per_cpu(pcpu_stats, cpu_id);

		snprintf(cpu_stats->name, MAX_CPUATTR_NAME_LEN,
				"cpu%u", cpu_id);

		cpu_time_in_state_attr[cpu_id].name =
			time_in_state_attr_name;
		cpu_time_in_state_attr[cpu_id].mode = S_IRUGO;
		cpu_stats->cpu_id = cpu_id;
		cpu_stats->cpu_persistent_stat_kobj.ktype =
			kzalloc(sizeof(struct kobj_type), GFP_KERNEL);

		if (!cpu_stats->cpu_persistent_stat_kobj.ktype) {
			ret = -ENOMEM;
			goto abort_cpu_persistent_stats_create_failed;
		}

		ret = kobject_init_and_add(
				&cpu_stats->cpu_persistent_stat_kobj,
				cpu_stats->cpu_persistent_stat_kobj.ktype,
				persistent_stats.persistent_stats_kobj,
				cpu_stats->name);

		if (ret) {
			pr_err("%s: Failed to create persistent stats node for cpu%u\n",
					__func__, cpu_id);
			goto abort_cpu_persistent_stats_create_failed;
		}

		cpu_stats->cpu_persistent_stat_kobj.ktype->sysfs_ops =
				&cpu_time_in_state_ops;

		ret = sysfs_create_file(
				&cpu_stats->cpu_persistent_stat_kobj,
				&cpu_time_in_state_attr[cpu_id]);

		if (ret) {
			pr_err("%s: sys_create_file failed.\n", __func__);
			goto abort_cpu_persistent_stats_create_failed;
		}
	}

	return 0;
abort_cpu_persistent_stats_create_failed:
	/* Remove all CPU entries and the root persistent_stats entry */
	for_each_possible_cpu(cpu_id) {
		cpu_stats = &per_cpu(pcpu_stats, cpu_id);

		sysfs_remove_file(
				&cpu_stats->cpu_persistent_stat_kobj,
				&cpu_time_in_state_attr[cpu_id]);
		kfree(cpu_stats->cpu_persistent_stat_kobj.ktype);
		kobject_put(&cpu_stats->cpu_persistent_stat_kobj);
	}
abort_reset_attr_create_failed:
	sysfs_remove_file(persistent_stats.persistent_stats_kobj,
			&enable_attr.attr);
abort_enable_attr_create_failed:
	kobject_put(persistent_stats.persistent_stats_kobj);
abort_stats_kobj_create_failed:
	cpufreq_put_global_kobject();
	return ret;
}

/*
 * Remove the persistent stats groups created at this driver's init time.
 */
static void remove_persistent_stats_groups(void)
{
	int cpu_id = 0;
	struct cpu_persistent_stats *cpu_stats;

	for (cpu_id = 0; cpu_id < num_possible_cpus(); cpu_id++) {
		cpu_stats = &per_cpu(pcpu_stats, cpu_id);

		sysfs_remove_file(
				&cpu_stats->cpu_persistent_stat_kobj,
				&cpu_time_in_state_attr[cpu_id]);
		kfree(cpu_stats->cpu_persistent_stat_kobj.ktype);
		kobject_put(&cpu_stats->cpu_persistent_stat_kobj);
	}

	/* Remove the root persistent stats kobject. */
	kobject_put(persistent_stats.persistent_stats_kobj);
	cpufreq_put_global_kobject();
}

static struct notifier_block cpufreq_stat_cpu_notifier __refdata = {
	.notifier_call = cpufreq_stat_cpu_callback,
};

static struct notifier_block notifier_trans_block = {
	.notifier_call = cpufreq_stat_notifier_trans
};

static int __init cpufreq_persistent_stats_init(void)
{
	int ret = 0;

	spin_lock_init(&cpufreq_stats_lock);

	ret = cpufreq_register_notifier(&notifier_trans_block,
				CPUFREQ_TRANSITION_NOTIFIER);

	if (ret)
		return ret;

	ret = register_hotcpu_notifier(&cpufreq_stat_cpu_notifier);

	if (ret)
		goto abort_register_hotcpu_failed;

	ret = create_persistent_stats_groups();

	if (ret)
		goto abort_create_stats_group_failed;

	return 0;
abort_create_stats_group_failed:
	unregister_hotcpu_notifier(&cpufreq_stat_cpu_notifier);
abort_register_hotcpu_failed:
	cpufreq_unregister_notifier(&notifier_trans_block,
			CPUFREQ_TRANSITION_NOTIFIER);
	return ret;
}

static void __exit cpufreq_persistent_stats_exit(void)
{
	unsigned int cpu;

	cpufreq_unregister_notifier(&notifier_trans_block,
			CPUFREQ_TRANSITION_NOTIFIER);
	unregister_hotcpu_notifier(&cpufreq_stat_cpu_notifier);

	for_each_possible_cpu(cpu)
		cpufreq_stats_free_table(cpu);

	remove_persistent_stats_groups();
}

MODULE_DESCRIPTION("Persists CPUFreq stats across CPU hotplugs.");
MODULE_LICENSE("GPL");

module_init(cpufreq_persistent_stats_init);
module_exit(cpufreq_persistent_stats_exit);
