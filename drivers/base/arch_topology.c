/*
 * Arch specific cpu topology information
 *
 * Copyright (C) 2016, ARM Ltd.
 * Written by: Juri Lelli, ARM Ltd.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Released under the GPLv2 only.
 * SPDX-License-Identifier: GPL-2.0
 */

#include <linux/acpi.h>
#include <linux/arch_topology.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sched/topology.h>
#include <linux/cpuset.h>

DEFINE_PER_CPU(unsigned long, freq_scale) = SCHED_CAPACITY_SCALE;

void arch_set_freq_scale(struct cpumask *cpus, unsigned long cur_freq,
			 unsigned long max_freq)
{
	unsigned long scale;
	int i;

	scale = (cur_freq << SCHED_CAPACITY_SHIFT) / max_freq;

	for_each_cpu(i, cpus)
		per_cpu(freq_scale, i) = scale;
}

static DEFINE_MUTEX(cpu_scale_mutex);
DEFINE_PER_CPU(unsigned long, cpu_scale) = SCHED_CAPACITY_SCALE;

void topology_set_cpu_scale(unsigned int cpu, unsigned long capacity)
{
	per_cpu(cpu_scale, cpu) = capacity;
}

static ssize_t cpu_capacity_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct cpu *cpu = container_of(dev, struct cpu, dev);

	return sprintf(buf, "%lu\n", topology_get_cpu_scale(NULL, cpu->dev.id));
}

static void update_topology_flags_workfn(struct work_struct *work);
static DECLARE_WORK(update_topology_flags_work, update_topology_flags_workfn);

static ssize_t cpu_capacity_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf,
				  size_t count)
{
	struct cpu *cpu = container_of(dev, struct cpu, dev);
	int this_cpu = cpu->dev.id;
	int i;
	unsigned long new_capacity;
	ssize_t ret;
	cpumask_var_t mask;

	if (!count)
		return 0;

	ret = kstrtoul(buf, 0, &new_capacity);
	if (ret)
		return ret;
	if (new_capacity > SCHED_CAPACITY_SCALE)
		return -EINVAL;

	mutex_lock(&cpu_scale_mutex);

	if (new_capacity < SCHED_CAPACITY_SCALE) {
		int highest_score_cpu = 0;

		if (!alloc_cpumask_var(&mask, GFP_KERNEL)) {
			mutex_unlock(&cpu_scale_mutex);
			return -ENOMEM;
		}

		cpumask_andnot(mask, cpu_online_mask,
				topology_core_cpumask(this_cpu));

		for_each_cpu(i, mask) {
			if (topology_get_cpu_scale(NULL, i) ==
					SCHED_CAPACITY_SCALE) {
				highest_score_cpu = 1;
				break;
			}
		}

		free_cpumask_var(mask);

		if (!highest_score_cpu) {
			mutex_unlock(&cpu_scale_mutex);
			return -EINVAL;
		}
	}

	for_each_cpu(i, topology_core_cpumask(this_cpu))
		topology_set_cpu_scale(i, new_capacity);
	mutex_unlock(&cpu_scale_mutex);

	if (topology_detect_flags())
		schedule_work(&update_topology_flags_work);

	return count;
}

static DEVICE_ATTR_RW(cpu_capacity);

static int register_cpu_capacity_sysctl(void)
{
	int i;
	struct device *cpu;

	for_each_possible_cpu(i) {
		cpu = get_cpu_device(i);
		if (!cpu) {
			pr_err("%s: too early to get CPU%d device!\n",
			       __func__, i);
			continue;
		}
		device_create_file(cpu, &dev_attr_cpu_capacity);
	}

	return 0;
}
subsys_initcall(register_cpu_capacity_sysctl);

enum asym_cpucap_type { no_asym, asym_thread, asym_core, asym_die };
static enum asym_cpucap_type asym_cpucap = no_asym;
enum share_cap_type { no_share_cap, share_cap_thread, share_cap_core, share_cap_die};
static enum share_cap_type share_cap = no_share_cap;

#ifdef CONFIG_CPU_FREQ
int detect_share_cap_flag(void)
{
	int cpu;
	enum share_cap_type share_cap_level = no_share_cap;
	struct cpufreq_policy *policy;

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);

		if (!policy)
			return 0;

		if (cpumask_equal(topology_sibling_cpumask(cpu),
				  policy->related_cpus)) {
			share_cap_level = share_cap_thread;
			continue;
		}

		if (cpumask_equal(topology_core_cpumask(cpu),
				  policy->related_cpus)) {
			share_cap_level = share_cap_core;
			continue;
		}

		if (cpumask_equal(cpu_cpu_mask(cpu),
				  policy->related_cpus)) {
			share_cap_level = share_cap_die;
			continue;
		}
	}

	if (share_cap != share_cap_level) {
		share_cap = share_cap_level;
		return 1;
	}

	return 0;
}
#else
int detect_share_cap_flags(void) { return 0; }
#endif

/*
 * Walk cpu topology to determine sched_domain flags.
 *
 * SD_ASYM_CPUCAPACITY: Indicates the lowest level that spans all cpu
 * capacities found in the system for all cpus, i.e. the flag is set
 * at the same level for all systems. The current algorithm implements
 * this by looking for higher capacities, which doesn't work for all
 * conceivable topology, but don't complicate things until it is
 * necessary.
 */
int topology_detect_flags(void)
{
	unsigned long max_capacity, capacity;
	enum asym_cpucap_type asym_level = no_asym;
	int cpu, die_cpu, core, thread, flags_changed = 0;

	for_each_possible_cpu(cpu) {
		max_capacity = 0;

		if (asym_level >= asym_thread)
			goto check_core;

		for_each_cpu(thread, topology_sibling_cpumask(cpu)) {
			capacity = topology_get_cpu_scale(NULL, thread);

			if (capacity > max_capacity) {
				if (max_capacity != 0)
					asym_level = asym_thread;

				max_capacity = capacity;
			}
		}

check_core:
		if (asym_level >= asym_core)
			goto check_die;

		for_each_cpu(core, topology_core_cpumask(cpu)) {
			capacity = topology_get_cpu_scale(NULL, core);

			if (capacity > max_capacity) {
				if (max_capacity != 0)
					asym_level = asym_core;

				max_capacity = capacity;
			}
		}
check_die:
		for_each_possible_cpu(die_cpu) {
			capacity = topology_get_cpu_scale(NULL, die_cpu);

			if (capacity > max_capacity) {
				if (max_capacity != 0) {
					asym_level = asym_die;
					goto done;
				}
			}
		}
	}

done:
	if (asym_cpucap != asym_level) {
		asym_cpucap = asym_level;
		flags_changed = 1;
		pr_debug("topology flag change detected\n");
	}

	if (detect_share_cap_flag())
		flags_changed = 1;

	return flags_changed;
}

int topology_smt_flags(void)
{
	int flags = 0;

	if (asym_cpucap == asym_thread)
		flags |= SD_ASYM_CPUCAPACITY;

	if (share_cap == share_cap_thread)
		flags |= SD_SHARE_CAP_STATES;

	return flags;
}

int topology_core_flags(void)
{
	int flags = 0;

	if (asym_cpucap == asym_core)
		flags |= SD_ASYM_CPUCAPACITY;

	if (share_cap == share_cap_core)
		flags |= SD_SHARE_CAP_STATES;

	return flags;
}

int topology_cpu_flags(void)
{
	int flags = 0;

	if (asym_cpucap == asym_die)
		flags |= SD_ASYM_CPUCAPACITY;

	if (share_cap == share_cap_die)
		flags |= SD_SHARE_CAP_STATES;

	return flags;
}

static int update_topology = 0;

int topology_update_cpu_topology(void)
{
	return update_topology;
}

/*
 * Updating the sched_domains can't be done directly from cpufreq callbacks
 * due to locking, so queue the work for later.
 */
static void update_topology_flags_workfn(struct work_struct *work)
{
	update_topology = 1;
	rebuild_sched_domains();
	pr_debug("sched_domain hierarchy rebuilt, flags updated\n");
	update_topology = 0;
}

static u32 capacity_scale;
static u32 *raw_capacity;

static int free_raw_capacity(void)
{
	kfree(raw_capacity);
	raw_capacity = NULL;

	return 0;
}

void topology_normalize_cpu_scale(void)
{
	u64 capacity;
	int cpu;

	if (!raw_capacity)
		return;

	pr_debug("cpu_capacity: capacity_scale=%u\n", capacity_scale);
	mutex_lock(&cpu_scale_mutex);
	for_each_possible_cpu(cpu) {
		capacity = (raw_capacity[cpu] << SCHED_CAPACITY_SHIFT)
			/ capacity_scale;
		topology_set_cpu_scale(cpu, capacity);
		pr_debug("cpu_capacity: CPU%d cpu_capacity=%lu raw_capacity=%u\n",
			cpu, topology_get_cpu_scale(NULL, cpu),
			raw_capacity[cpu]);
	}
	mutex_unlock(&cpu_scale_mutex);
}

bool __init topology_parse_cpu_capacity(struct device_node *cpu_node, int cpu)
{
	static bool cap_parsing_failed;
	int ret;
	u32 cpu_capacity;

	if (cap_parsing_failed)
		return false;

	ret = of_property_read_u32(cpu_node, "capacity-dmips-mhz",
				   &cpu_capacity);
	if (!ret) {
		if (!raw_capacity) {
			raw_capacity = kcalloc(num_possible_cpus(),
					       sizeof(*raw_capacity),
					       GFP_KERNEL);
			if (!raw_capacity) {
				pr_err("cpu_capacity: failed to allocate memory for raw capacities\n");
				cap_parsing_failed = true;
				return false;
			}
		}
		capacity_scale = max(cpu_capacity, capacity_scale);
		raw_capacity[cpu] = cpu_capacity;
		pr_debug("cpu_capacity: %pOF cpu_capacity=%u (raw)\n",
			cpu_node, raw_capacity[cpu]);
	} else {
		if (raw_capacity) {
			pr_err("cpu_capacity: missing %pOF raw capacity\n",
				cpu_node);
			pr_err("cpu_capacity: partial information: fallback to 1024 for all CPUs\n");
		}
		cap_parsing_failed = true;
		free_raw_capacity();
	}

	return !ret;
}

#ifdef CONFIG_CPU_FREQ
static cpumask_var_t cpus_to_visit __initdata;
static void __init parsing_done_workfn(struct work_struct *work);
static __initdata DECLARE_WORK(parsing_done_work, parsing_done_workfn);

static int __init
init_cpu_capacity_callback(struct notifier_block *nb,
			   unsigned long val,
			   void *data)
{
	struct cpufreq_policy *policy = data;
	int cpu;

	if (!raw_capacity)
		return 0;

	if (val != CPUFREQ_NOTIFY)
		return 0;

	pr_debug("cpu_capacity: init cpu capacity for CPUs [%*pbl] (to_visit=%*pbl)\n",
		 cpumask_pr_args(policy->related_cpus),
		 cpumask_pr_args(cpus_to_visit));

	cpumask_andnot(cpus_to_visit, cpus_to_visit, policy->related_cpus);

	for_each_cpu(cpu, policy->related_cpus) {
		raw_capacity[cpu] = topology_get_cpu_scale(NULL, cpu) *
				    policy->cpuinfo.max_freq / 1000UL;
		capacity_scale = max(raw_capacity[cpu], capacity_scale);
	}

	if (cpumask_empty(cpus_to_visit)) {
		topology_normalize_cpu_scale();
		if (topology_detect_flags())
			schedule_work(&update_topology_flags_work);
		free_raw_capacity();
		pr_debug("cpu_capacity: parsing done\n");
		schedule_work(&parsing_done_work);
	}

	return 0;
}

static struct notifier_block init_cpu_capacity_notifier __initdata = {
	.notifier_call = init_cpu_capacity_callback,
};

static int __init register_cpufreq_notifier(void)
{
	int ret;

	/*
	 * on ACPI-based systems we need to use the default cpu capacity
	 * until we have the necessary code to parse the cpu capacity, so
	 * skip registering cpufreq notifier.
	 */
	if (!acpi_disabled || !raw_capacity)
		return -EINVAL;

	if (!alloc_cpumask_var(&cpus_to_visit, GFP_KERNEL)) {
		pr_err("cpu_capacity: failed to allocate memory for cpus_to_visit\n");
		return -ENOMEM;
	}

	cpumask_copy(cpus_to_visit, cpu_possible_mask);

	ret = cpufreq_register_notifier(&init_cpu_capacity_notifier,
					CPUFREQ_POLICY_NOTIFIER);

	if (ret)
		free_cpumask_var(cpus_to_visit);

	return ret;
}
core_initcall(register_cpufreq_notifier);

static void __init parsing_done_workfn(struct work_struct *work)
{
	cpufreq_unregister_notifier(&init_cpu_capacity_notifier,
					 CPUFREQ_POLICY_NOTIFIER);
	free_cpumask_var(cpus_to_visit);
}

#else
core_initcall(free_raw_capacity);
#endif
