/* Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
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

#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/msm-core-interface.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/pm_opp.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/thermal.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/uio_driver.h>
#include <asm/smp_plat.h>
#include <stdbool.h>
#define CREATE_TRACE_POINTS
#include <trace/events/trace_msm_core.h>

#define TEMP_BASE_POINT 35
#define TEMP_MAX_POINT 95
#define CPU_HOTPLUG_LIMIT 80
#define CPU_BIT_MASK(cpu) BIT(cpu)
#define DEFAULT_TEMP 40
#define DEFAULT_LOW_HYST_TEMP 10
#define DEFAULT_HIGH_HYST_TEMP 5
#define CLUSTER_OFFSET_FOR_MPIDR 8
#define MAX_CORES_PER_CLUSTER 4
#define MAX_NUM_OF_CLUSTERS 2
#define NUM_OF_CORNERS 10
#define DEFAULT_SCALING_FACTOR 1

#define ALLOCATE_2D_ARRAY(type)\
static type **allocate_2d_array_##type(int idx)\
{\
	int i;\
	type **ptr = NULL;\
	if (!idx) \
		return ERR_PTR(-EINVAL);\
	ptr = kzalloc(sizeof(*ptr) * TEMP_DATA_POINTS, \
				GFP_KERNEL);\
	if (!ptr) { \
		return ERR_PTR(-ENOMEM); \
	} \
	for (i = 0; i < TEMP_DATA_POINTS; i++) { \
		ptr[i] = kzalloc(sizeof(*ptr[i]) * \
					idx, GFP_KERNEL);\
		if (!ptr[i]) {\
			goto done;\
		} \
	} \
	return ptr;\
done:\
	for (i = 0; i < TEMP_DATA_POINTS; i++) \
		kfree(ptr[i]);\
	kfree(ptr);\
	return ERR_PTR(-ENOMEM);\
}

struct cpu_activity_info {
	int cpu;
	int mpidr;
	long temp;
	int sensor_id;
	struct sensor_threshold hi_threshold;
	struct sensor_threshold low_threshold;
	struct cpu_static_info *sp;
};

struct cpu_static_info {
	uint32_t **power;
	cpumask_t mask;
	struct cpufreq_frequency_table *table;
	uint32_t *voltage;
	uint32_t num_of_freqs;
};

static DEFINE_MUTEX(policy_update_mutex);
static DEFINE_MUTEX(kthread_update_mutex);
static DEFINE_SPINLOCK(update_lock);
static struct delayed_work sampling_work;
static struct completion sampling_completion;
static struct task_struct *sampling_task;
static int low_hyst_temp;
static int high_hyst_temp;
static struct platform_device *msm_core_pdev;
static struct cpu_activity_info activity[NR_CPUS];
DEFINE_PER_CPU(struct cpu_pstate_pwr *, ptable);
static struct cpu_pwr_stats cpu_stats[NR_CPUS];
static uint32_t scaling_factor;
ALLOCATE_2D_ARRAY(uint32_t);

static int poll_ms;
module_param_named(polling_interval, poll_ms, int,
		S_IRUGO | S_IWUSR | S_IWGRP);

static int disabled;
module_param_named(disabled, disabled, int,
		S_IRUGO | S_IWUSR | S_IWGRP);
static bool in_suspend;
static bool activate_power_table;
static int max_throttling_temp = 80; /* in C */
module_param_named(throttling_temp, max_throttling_temp, int,
		S_IRUGO | S_IWUSR | S_IWGRP);

/*
 * Cannot be called from an interrupt context
 */
static void set_and_activate_threshold(uint32_t sensor_id,
	struct sensor_threshold *threshold)
{
	if (sensor_set_trip(sensor_id, threshold)) {
		pr_err("%s: Error in setting trip %d\n",
			KBUILD_MODNAME, threshold->trip);
		return;
	}

	if (sensor_activate_trip(sensor_id, threshold, true)) {
		sensor_cancel_trip(sensor_id, threshold);
		pr_err("%s: Error in enabling trip %d\n",
			KBUILD_MODNAME, threshold->trip);
		return;
	}
}

static void set_threshold(struct cpu_activity_info *cpu_node)
{
	if (cpu_node->sensor_id < 0)
		return;

	/*
	 * Before operating on the threshold structure which is used by
	 * thermal core ensure that the sensor is disabled to prevent
	 * incorrect operations on the sensor list maintained by thermal code.
	 */
	sensor_activate_trip(cpu_node->sensor_id,
			&cpu_node->hi_threshold, false);
	sensor_activate_trip(cpu_node->sensor_id,
			&cpu_node->low_threshold, false);

	cpu_node->hi_threshold.temp = (cpu_node->temp + high_hyst_temp) *
					scaling_factor;
	cpu_node->low_threshold.temp = (cpu_node->temp - low_hyst_temp) *
					scaling_factor;

	/*
	 * Set the threshold only if we are below the hotplug limit
	 * Adding more work at this high temperature range, seems to
	 * fail hotplug notifications.
	 */
	if (cpu_node->hi_threshold.temp < (CPU_HOTPLUG_LIMIT * scaling_factor))
		set_and_activate_threshold(cpu_node->sensor_id,
			&cpu_node->hi_threshold);

	set_and_activate_threshold(cpu_node->sensor_id,
		&cpu_node->low_threshold);
}

static void samplequeue_handle(struct work_struct *work)
{
	complete(&sampling_completion);
}

/* May be called from an interrupt context */
static void core_temp_notify(enum thermal_trip_type type,
		int temp, void *data)
{
	struct cpu_activity_info *cpu_node =
		(struct cpu_activity_info *) data;

	temp /= scaling_factor;

	trace_temp_notification(cpu_node->sensor_id,
		type, temp, cpu_node->temp);

	cpu_node->temp = temp;

	complete(&sampling_completion);
}

static void repopulate_stats(int cpu)
{
	int i;
	struct cpu_activity_info *cpu_node = &activity[cpu];
	int temp_point;
	struct cpu_pstate_pwr *pt =  per_cpu(ptable, cpu);

	if (!pt)
		return;

	if (cpu_node->temp < TEMP_BASE_POINT)
		temp_point = 0;
	else if (cpu_node->temp > TEMP_MAX_POINT)
		temp_point = TEMP_DATA_POINTS - 1;
	else
		temp_point = (cpu_node->temp - TEMP_BASE_POINT) / 5;

	cpu_stats[cpu].temp = cpu_node->temp;
	for (i = 0; i < cpu_node->sp->num_of_freqs; i++)
		pt[i].power = cpu_node->sp->power[temp_point][i];

	trace_cpu_stats(cpu, cpu_stats[cpu].temp, pt[0].power,
			pt[cpu_node->sp->num_of_freqs-1].power);
};

void trigger_cpu_pwr_stats_calc(void)
{
	int cpu;
	static long prev_temp[NR_CPUS];
	struct cpu_activity_info *cpu_node;
	int temp;

	if (disabled)
		return;

	spin_lock(&update_lock);

	for_each_online_cpu(cpu) {
		cpu_node = &activity[cpu];
		if (cpu_node->sensor_id < 0)
			continue;

		if (cpu_node->temp == prev_temp[cpu]) {
			sensor_get_temp(cpu_node->sensor_id, &temp);
			cpu_node->temp = temp / scaling_factor;
		}

		prev_temp[cpu] = cpu_node->temp;

		/*
		 * Do not populate/update stats before policy and ptable have
		 * been updated.
		 */
		if (activate_power_table && cpu_stats[cpu].ptable
			&& cpu_node->sp->table)
			repopulate_stats(cpu);
	}
	spin_unlock(&update_lock);
}
EXPORT_SYMBOL(trigger_cpu_pwr_stats_calc);

void set_cpu_throttled(cpumask_t *mask, bool throttling)
{
	int cpu;

	if (!mask)
		return;

	spin_lock(&update_lock);
	for_each_cpu(cpu, mask)
		cpu_stats[cpu].throttling = throttling;
	spin_unlock(&update_lock);
}
EXPORT_SYMBOL(set_cpu_throttled);

static void update_related_freq_table(struct cpufreq_policy *policy)
{
	int cpu, num_of_freqs;
	struct cpufreq_frequency_table *table;

	table = cpufreq_frequency_get_table(policy->cpu);
	if (!table) {
		pr_err("Couldn't get freq table for cpu%d\n",
				policy->cpu);
		return;
	}

	for (num_of_freqs = 0; table[num_of_freqs].frequency !=
			CPUFREQ_TABLE_END;)
		num_of_freqs++;

	/*
	 * Synchronous cores within cluster have the same
	 * policy. Since these cores do not have the cpufreq
	 * table initialized for all of them, copy the same
	 * table to all the related cpus.
	 */
	for_each_cpu(cpu, policy->related_cpus) {
		activity[cpu].sp->table = table;
		activity[cpu].sp->num_of_freqs = num_of_freqs;
	}
}

static __ref int do_sampling(void *data)
{
	int cpu;
	struct cpu_activity_info *cpu_node;
	static int prev_temp[NR_CPUS];

	while (!kthread_should_stop()) {
		wait_for_completion(&sampling_completion);
		cancel_delayed_work(&sampling_work);

		mutex_lock(&kthread_update_mutex);
		if (in_suspend)
			goto unlock;

		trigger_cpu_pwr_stats_calc();

		for_each_online_cpu(cpu) {
			cpu_node = &activity[cpu];
			if (prev_temp[cpu] != cpu_node->temp) {
				prev_temp[cpu] = cpu_node->temp;
				set_threshold(cpu_node);
				trace_temp_threshold(cpu, cpu_node->temp,
					cpu_node->hi_threshold.temp /
					scaling_factor,
					cpu_node->low_threshold.temp /
					scaling_factor);
			}
		}
		if (!poll_ms)
			goto unlock;

		schedule_delayed_work(&sampling_work,
			msecs_to_jiffies(poll_ms));
unlock:
		mutex_unlock(&kthread_update_mutex);
	}
	return 0;
}

static void clear_static_power(struct cpu_static_info *sp)
{
	int i;

	if (!sp)
		return;

	if (cpumask_first(&sp->mask) < num_possible_cpus())
		return;

	for (i = 0; i < TEMP_DATA_POINTS; i++)
		kfree(sp->power[i]);
	kfree(sp->power);
	kfree(sp);
}

BLOCKING_NOTIFIER_HEAD(msm_core_stats_notifier_list);

struct blocking_notifier_head *get_power_update_notifier(void)
{
	return &msm_core_stats_notifier_list;
}

int register_cpu_pwr_stats_ready_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&msm_core_stats_notifier_list,
						nb);
}

static int update_userspace_power(struct sched_params __user *argp)
{
	int i;
	int ret;
	int cpu = -1;
	struct cpu_activity_info *node;
	struct cpu_static_info *sp, *clear_sp;
	int cpumask, cluster, mpidr;
	bool pdata_valid[NR_CPUS] = {0};

	get_user(cpumask, &argp->cpumask);
	get_user(cluster, &argp->cluster);
	mpidr = cluster << 8;

	pr_debug("%s: cpumask %d, cluster: %d\n", __func__, cpumask,
					cluster);
	for (i = 0; i < MAX_CORES_PER_CLUSTER; i++, cpumask >>= 1) {
		if (!(cpumask & 0x01))
			continue;

		mpidr |= i;
		for_each_possible_cpu(cpu) {
			if (cpu_logical_map(cpu) == mpidr)
				break;
		}
	}

	if ((cpu < 0) || (cpu >= num_possible_cpus()))
		return -EINVAL;

	node = &activity[cpu];
	/* Allocate new memory to copy cpumask specific power
	 * information.
	 */
	sp = kzalloc(sizeof(*sp), GFP_KERNEL);
	if (!sp)
		return -ENOMEM;

	mutex_lock(&policy_update_mutex);
	sp->power = allocate_2d_array_uint32_t(node->sp->num_of_freqs);
	if (IS_ERR_OR_NULL(sp->power)) {
		mutex_unlock(&policy_update_mutex);
		ret = PTR_ERR(sp->power);
		kfree(sp);
		return ret;
	}
	sp->num_of_freqs = node->sp->num_of_freqs;
	sp->voltage = node->sp->voltage;
	sp->table = node->sp->table;

	for (i = 0; i < TEMP_DATA_POINTS; i++) {
		ret = copy_from_user(sp->power[i], &argp->power[i][0],
			sizeof(sp->power[i][0]) * node->sp->num_of_freqs);
		if (ret)
			goto failed;
	}

	/* Copy the same power values for all the cpus in the cpumask
	 * argp->cpumask within the cluster (argp->cluster)
	 */
	get_user(cpumask, &argp->cpumask);
	spin_lock(&update_lock);
	for (i = 0; i < MAX_CORES_PER_CLUSTER; i++, cpumask >>= 1) {
		if (!(cpumask & 0x01))
			continue;
		mpidr = (cluster << CLUSTER_OFFSET_FOR_MPIDR);
		mpidr |= i;
		for_each_possible_cpu(cpu) {
			if (!(cpu_logical_map(cpu) == mpidr))
				continue;

			node = &activity[cpu];
			clear_sp = node->sp;
			node->sp = sp;
			cpumask_set_cpu(cpu, &sp->mask);
			if (clear_sp) {
				cpumask_clear_cpu(cpu, &clear_sp->mask);
				clear_static_power(clear_sp);
			}
			cpu_stats[cpu].ptable = per_cpu(ptable, cpu);
			repopulate_stats(cpu);
			pdata_valid[cpu] = true;
		}
	}
	spin_unlock(&update_lock);
	mutex_unlock(&policy_update_mutex);

	for_each_possible_cpu(cpu) {
		if (!pdata_valid[cpu])
			continue;

		blocking_notifier_call_chain(
			&msm_core_stats_notifier_list, cpu, NULL);
	}

	activate_power_table = true;
	return 0;

failed:
	mutex_unlock(&policy_update_mutex);
	for (i = 0; i < TEMP_DATA_POINTS; i++)
		kfree(sp->power[i]);
	kfree(sp->power);
	kfree(sp);
	return ret;
}

static long msm_core_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	long ret = 0;
	struct cpu_activity_info *node = NULL;
	struct sched_params __user *argp = (struct sched_params __user *)arg;
	int i, cpu = num_possible_cpus();
	int mpidr, cluster, cpumask;

	if (!argp)
		return -EINVAL;

	get_user(cluster, &argp->cluster);
	mpidr = (cluster << (MAX_CORES_PER_CLUSTER *
			MAX_NUM_OF_CLUSTERS));
	get_user(cpumask, &argp->cpumask);

	switch (cmd) {
	case EA_LEAKAGE:
		ret = update_userspace_power(argp);
		if (ret)
			pr_err("Userspace power update failed with %ld\n", ret);
		break;
	case EA_VOLT:
		for (i = 0; cpumask > 0; i++, cpumask >>= 1) {
			for_each_possible_cpu(cpu) {
				if (cpu_logical_map(cpu) == (mpidr | i))
					break;
			}
		}
		if (cpu >= num_possible_cpus())
			break;

		mutex_lock(&policy_update_mutex);
		node = &activity[cpu];
		if (!node->sp->table) {
			ret = -EINVAL;
			goto unlock;
		}
		ret = copy_to_user((void __user *)&argp->voltage[0],
				node->sp->voltage,
				sizeof(uint32_t) * node->sp->num_of_freqs);
		if (ret)
			break;
		for (i = 0; i < node->sp->num_of_freqs; i++) {
			ret = copy_to_user((void __user *)&argp->freq[i],
					&node->sp->table[i].frequency,
					sizeof(uint32_t));
			if (ret)
				break;
		}
unlock:
		mutex_unlock(&policy_update_mutex);
		break;
	default:
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long msm_core_compat_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	arg = (unsigned long)compat_ptr(arg);
	return msm_core_ioctl(file, cmd, arg);
}
#endif

static int msm_core_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int msm_core_release(struct inode *inode, struct file *file)
{
	return 0;
}

static inline void init_sens_threshold(struct sensor_threshold *threshold,
		enum thermal_trip_type trip, long temp,
		void *data)
{
	threshold->trip = trip;
	threshold->temp = temp;
	threshold->data = data;
	threshold->notify = (void *)core_temp_notify;
}

static int msm_core_stats_init(struct device *dev, int cpu)
{
	int i;
	struct cpu_activity_info *cpu_node;
	struct cpu_pstate_pwr *pstate = NULL;

	cpu_node = &activity[cpu];
	cpu_stats[cpu].cpu = cpu;
	cpu_stats[cpu].temp = cpu_node->temp;
	cpu_stats[cpu].throttling = false;

	cpu_stats[cpu].len = cpu_node->sp->num_of_freqs;
	pstate = devm_kzalloc(dev,
		sizeof(*pstate) * cpu_node->sp->num_of_freqs,
		GFP_KERNEL);
	if (!pstate)
		return -ENOMEM;

	for (i = 0; i < cpu_node->sp->num_of_freqs; i++)
		pstate[i].freq = cpu_node->sp->table[i].frequency;

	per_cpu(ptable, cpu) = pstate;

	return 0;
}

static int msm_core_task_init(struct device *dev)
{
	init_completion(&sampling_completion);
	sampling_task = kthread_run(do_sampling, NULL, "msm-core:sampling");
	if (IS_ERR(sampling_task)) {
		pr_err("Failed to create do_sampling err: %ld\n",
				PTR_ERR(sampling_task));
		return PTR_ERR(sampling_task);
	}
	return 0;
}

struct cpu_pwr_stats *get_cpu_pwr_stats(void)
{
	return cpu_stats;
}
EXPORT_SYMBOL(get_cpu_pwr_stats);

static int msm_get_power_values(int cpu, struct cpu_static_info *sp)
{
	int i = 0, j;
	int ret = 0;
	uint64_t power;

	/* Calculate dynamic power spent for every frequency using formula:
	 * Power = V * V * f
	 * where V = voltage for frequency
	 *       f = frequency
	 * */
	sp->power = allocate_2d_array_uint32_t(sp->num_of_freqs);
	if (IS_ERR_OR_NULL(sp->power))
		return PTR_ERR(sp->power);

	for (i = 0; i < TEMP_DATA_POINTS; i++) {
		for (j = 0; j < sp->num_of_freqs; j++) {
			power = sp->voltage[j] *
						sp->table[j].frequency;
			do_div(power, 1000);
			do_div(power, 1000);
			power *= sp->voltage[j];
			do_div(power, 1000);
			sp->power[i][j] = power;
		}
	}
	return ret;
}

static int msm_get_voltage_levels(struct device *dev, int cpu,
		struct cpu_static_info *sp)
{
	unsigned int *voltage;
	int i;
	int corner;
	struct dev_pm_opp *opp;
	struct device *cpu_dev = get_cpu_device(cpu);
	/*
	 * Convert cpr corner voltage to average voltage of both
	 * a53 and a57 votlage value
	 */
	int average_voltage[NUM_OF_CORNERS] = {0, 746, 841, 843, 940, 953, 976,
			1024, 1090, 1100};

	if (!cpu_dev)
		return -ENODEV;

	voltage = devm_kzalloc(dev,
			sizeof(*voltage) * sp->num_of_freqs, GFP_KERNEL);

	if (!voltage)
		return -ENOMEM;

	rcu_read_lock();
	for (i = 0; i < sp->num_of_freqs; i++) {
		opp = dev_pm_opp_find_freq_exact(cpu_dev,
				sp->table[i].frequency * 1000, true);
		corner = dev_pm_opp_get_voltage(opp);

		if (corner > 400000)
			voltage[i] = corner / 1000;
		else if (corner > 0 && corner < ARRAY_SIZE(average_voltage))
			voltage[i] = average_voltage[corner];
		else
			voltage[i]
			     = average_voltage[ARRAY_SIZE(average_voltage) - 1];
	}
	rcu_read_unlock();

	sp->voltage = voltage;
	return 0;
}

static int msm_core_dyn_pwr_init(struct platform_device *pdev,
				int cpu)
{
	int ret = 0;

	if (!activity[cpu].sp->table)
		return 0;

	ret = msm_get_voltage_levels(&pdev->dev, cpu, activity[cpu].sp);
	if (ret)
		return ret;

	ret = msm_get_power_values(cpu, activity[cpu].sp);

	return ret;
}

static int msm_core_tsens_init(struct device_node *node, int cpu)
{
	int ret = 0;
	char *key = NULL;
	struct device_node *phandle;
	const char *sensor_type = NULL;
	struct cpu_activity_info *cpu_node = &activity[cpu];
	int temp;

	if (!node)
		return -ENODEV;

	key = "sensor";
	phandle = of_parse_phandle(node, key, 0);
	if (!phandle) {
		pr_info("%s: No sensor mapping found for the core\n",
				__func__);
		/* Do not treat this as error as some targets might have
		 * temperature notification only in userspace.
		 * Use default temperature for the core. Userspace might
		 * update the temperature once it is up.
		 */
		cpu_node->sensor_id = -ENODEV;
		cpu_node->temp = DEFAULT_TEMP;
		return 0;
	}

	key = "qcom,sensor-name";
	ret = of_property_read_string(phandle, key,
				&sensor_type);
	if (ret) {
		pr_err("%s: Cannot read tsens id\n", __func__);
		return ret;
	}

	cpu_node->sensor_id = sensor_get_id((char *)sensor_type);
	if (cpu_node->sensor_id < 0)
		return cpu_node->sensor_id;

	key = "qcom,scaling-factor";
	ret = of_property_read_u32(phandle, key,
				&scaling_factor);
	if (ret) {
		pr_info("%s: Cannot read tsens scaling factor\n", __func__);
		scaling_factor = DEFAULT_SCALING_FACTOR;
	}

	ret = sensor_get_temp(cpu_node->sensor_id, &temp);
	if (ret)
		return ret;

	cpu_node->temp = temp / scaling_factor;

	init_sens_threshold(&cpu_node->hi_threshold,
			THERMAL_TRIP_CONFIGURABLE_HI,
			(cpu_node->temp + high_hyst_temp) * scaling_factor,
			(void *)cpu_node);
	init_sens_threshold(&cpu_node->low_threshold,
			THERMAL_TRIP_CONFIGURABLE_LOW,
			(cpu_node->temp - low_hyst_temp) * scaling_factor,
			(void *)cpu_node);

	return ret;
}

static int msm_core_mpidr_init(struct device_node *phandle)
{
	int ret = 0;
	char *key = NULL;
	int mpidr;

	key = "reg";
	ret = of_property_read_u32(phandle, key,
				&mpidr);
	if (ret) {
		pr_err("%s: Cannot read mpidr\n", __func__);
		return ret;
	}
	return mpidr;
}

static int msm_core_cpu_policy_handler(struct notifier_block *nb,
		unsigned long val, void *data)
{
	struct cpufreq_policy *policy = data;
	struct cpu_activity_info *cpu_info = &activity[policy->cpu];
	int cpu;
	int ret;

	if (cpu_info->sp->table)
		return NOTIFY_OK;

	switch (val) {
	case CPUFREQ_CREATE_POLICY:
		mutex_lock(&policy_update_mutex);
		update_related_freq_table(policy);

		for_each_cpu(cpu, policy->related_cpus) {
			ret = msm_core_dyn_pwr_init(msm_core_pdev, cpu);
			if (ret)
				pr_debug("voltage-pwr table update failed\n");

			ret = msm_core_stats_init(&msm_core_pdev->dev, cpu);
			if (ret)
				pr_debug("Stats table update failed\n");
		}
		mutex_unlock(&policy_update_mutex);
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

struct notifier_block cpu_policy = {
	.notifier_call = msm_core_cpu_policy_handler
};

static int system_suspend_handler(struct notifier_block *nb,
				unsigned long val, void *data)
{
	int cpu;

	mutex_lock(&kthread_update_mutex);
	switch (val) {
	case PM_POST_HIBERNATION:
	case PM_POST_SUSPEND:
	case PM_POST_RESTORE:
		/*
		 * Set completion event to read temperature and repopulate
		 * stats
		 */
		in_suspend = 0;
		complete(&sampling_completion);
		break;
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
		/*
		 * cancel delayed work to be able to restart immediately
		 * after system resume
		 */
		in_suspend = 1;
		cancel_delayed_work(&sampling_work);
		/*
		 * cancel TSENS interrupts as we do not want to wake up from
		 * suspend to take care of repopulate stats while the system is
		 * in suspend
		 */
		for_each_possible_cpu(cpu) {
			if (activity[cpu].sensor_id < 0)
				continue;

			sensor_activate_trip(activity[cpu].sensor_id,
				&activity[cpu].hi_threshold, false);
			sensor_activate_trip(activity[cpu].sensor_id,
				&activity[cpu].low_threshold, false);
		}
		break;
	default:
		break;
	}
	mutex_unlock(&kthread_update_mutex);

	return NOTIFY_OK;
}

static int msm_core_freq_init(void)
{
	int cpu;
	struct cpufreq_policy *policy;

	for_each_possible_cpu(cpu) {
		activity[cpu].sp = kzalloc(sizeof(*(activity[cpu].sp)),
				GFP_KERNEL);
		if (!activity[cpu].sp)
			return -ENOMEM;
	}

	for_each_online_cpu(cpu) {
		if (activity[cpu].sp->table)
			continue;

		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			continue;

		update_related_freq_table(policy);
		cpufreq_cpu_put(policy);
	}

	return 0;
}

static int msm_core_params_init(struct platform_device *pdev)
{
	int ret = 0;
	unsigned long cpu = 0;
	struct device_node *child_node = NULL;
	struct device_node *ea_node = NULL;
	char *key = NULL;
	int mpidr;

	for_each_possible_cpu(cpu) {
		child_node = of_get_cpu_node(cpu, NULL);

		if (!child_node)
			continue;

		mpidr = msm_core_mpidr_init(child_node);
		if (mpidr < 0)
			return mpidr;

		if (cpu >= num_possible_cpus())
			continue;

		activity[cpu].mpidr = mpidr;

		key = "qcom,ea";
		ea_node = of_parse_phandle(child_node, key, 0);
		if (!ea_node) {
			pr_err("%s Couldn't find the ea_node for cpu%lu\n",
				__func__, cpu);
			return -ENODEV;
		}

		ret = msm_core_tsens_init(ea_node, cpu);
		if (ret)
			return ret;

		if (!activity[cpu].sp->table)
			continue;

		ret = msm_core_dyn_pwr_init(msm_core_pdev, cpu);
		if (ret)
			pr_debug("voltage-pwr table update failed\n");

		ret = msm_core_stats_init(&msm_core_pdev->dev, cpu);
		if (ret)
			pr_debug("Stats table update failed\n");
	}

	return 0;
}

static const struct file_operations msm_core_ops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = msm_core_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = msm_core_compat_ioctl,
#endif
	.open = msm_core_open,
	.release = msm_core_release,
};

static struct miscdevice msm_core_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "pta",
	.fops = &msm_core_ops
};

static void free_dyn_memory(void)
{
	int i, cpu;

	for_each_possible_cpu(cpu) {
		if (activity[cpu].sp) {
			for (i = 0; i < TEMP_DATA_POINTS; i++) {
				if (!activity[cpu].sp->power)
					break;

				kfree(activity[cpu].sp->power[i]);
			}
		}
		kfree(activity[cpu].sp);
	}
}

static int uio_init(struct platform_device *pdev)
{
	int ret = 0;
	struct uio_info *info = NULL;
	struct resource *clnt_res = NULL;
	u32 ea_mem_size = 0;
	phys_addr_t ea_mem_pyhsical = 0;

	clnt_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!clnt_res) {
		pr_err("resource not found\n");
		return -ENODEV;
	}

	info = devm_kzalloc(&pdev->dev, sizeof(struct uio_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	ea_mem_size = resource_size(clnt_res);
	ea_mem_pyhsical = clnt_res->start;

	if (ea_mem_size == 0) {
		pr_err("msm-core: memory size is zero");
		return -EINVAL;
	}

	/* Setup device */
	info->name = clnt_res->name;
	info->version = "1.0";
	info->mem[0].addr = ea_mem_pyhsical;
	info->mem[0].size = ea_mem_size;
	info->mem[0].memtype = UIO_MEM_PHYS;

	ret = uio_register_device(&pdev->dev, info);
	if (ret) {
		pr_err("uio register failed ret=%d", ret);
		return ret;
	}
	dev_set_drvdata(&pdev->dev, info);

	return 0;
}

static int msm_core_dev_probe(struct platform_device *pdev)
{
	int ret = 0;
	char *key = NULL;
	struct device_node *node;
	int cpu;
	struct uio_info *info;

	if (!pdev)
		return -ENODEV;

	msm_core_pdev = pdev;
	node = pdev->dev.of_node;
	if (!node)
		return -ENODEV;

	key = "qcom,low-hyst-temp";
	ret = of_property_read_u32(node, key, &low_hyst_temp);
	if (ret)
		low_hyst_temp = DEFAULT_LOW_HYST_TEMP;

	key = "qcom,high-hyst-temp";
	ret = of_property_read_u32(node, key, &high_hyst_temp);
	if (ret)
		high_hyst_temp = DEFAULT_HIGH_HYST_TEMP;

	key = "qcom,polling-interval";
	ret = of_property_read_u32(node, key, &poll_ms);
	if (ret)
		pr_info("msm-core initialized without polling period\n");

	key = "qcom,throttling-temp";
	ret = of_property_read_u32(node, key, &max_throttling_temp);

	ret = uio_init(pdev);
	if (ret)
		return ret;

	ret = msm_core_freq_init();
	if (ret)
		goto failed;

	ret = misc_register(&msm_core_device);
	if (ret) {
		pr_err("%s: Error registering device %d\n", __func__, ret);
		goto failed;
	}

	ret = msm_core_params_init(pdev);
	if (ret)
		goto failed;

	INIT_DEFERRABLE_WORK(&sampling_work, samplequeue_handle);
	ret = msm_core_task_init(&pdev->dev);
	if (ret)
		goto failed;

	for_each_possible_cpu(cpu)
		set_threshold(&activity[cpu]);

	schedule_delayed_work(&sampling_work, msecs_to_jiffies(0));
	cpufreq_register_notifier(&cpu_policy, CPUFREQ_POLICY_NOTIFIER);
	pm_notifier(system_suspend_handler, 0);
	return 0;
failed:
	info = dev_get_drvdata(&pdev->dev);
	uio_unregister_device(info);
	free_dyn_memory();
	return ret;
}

static int msm_core_remove(struct platform_device *pdev)
{
	int cpu;
	struct uio_info *info = dev_get_drvdata(&pdev->dev);

	uio_unregister_device(info);

	for_each_possible_cpu(cpu) {
		if (activity[cpu].sensor_id < 0)
			continue;

		sensor_cancel_trip(activity[cpu].sensor_id,
				&activity[cpu].hi_threshold);
		sensor_cancel_trip(activity[cpu].sensor_id,
				&activity[cpu].low_threshold);
	}
	free_dyn_memory();
	misc_deregister(&msm_core_device);
	return 0;
}

static struct of_device_id msm_core_match_table[] = {
	{.compatible = "qcom,apss-core-ea"},
	{},
};

static struct platform_driver msm_core_driver = {
	.probe = msm_core_dev_probe,
	.driver = {
		.name = "msm_core",
		.owner = THIS_MODULE,
		.of_match_table = msm_core_match_table,
		},
	.remove = msm_core_remove,
};

static int __init msm_core_init(void)
{
	return platform_driver_register(&msm_core_driver);
}
late_initcall(msm_core_init);
