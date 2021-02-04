/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#include <linux/list_sort.h>

struct cpu_efficiency {
	const char *compatible;
	unsigned long efficiency;
};

/*
 * Table of relative efficiency of each processors
 * The efficiency value must fit in 20bit and the final
 * cpu_scale value must be in the range
 *   0 < cpu_scale < SCHED_CAPACITY_SCALE.
 * Processors that are not defined in the table,
 * use the default SCHED_CAPACITY_SCALE value for cpu_scale.
 */
static const struct cpu_efficiency table_efficiency[] = {
	{ "arm,cortex-a73", 3630 },
	{ "arm,cortex-a72", 4186 },
	{ "arm,cortex-a57", 3891 },
	{ "arm,cortex-a53", 2048 },
	{ "arm,cortex-a35", 1661 },
	{ NULL, },
};

static void update_siblings_masks(unsigned int cpuid);
static void update_cpu_capacity(unsigned int cpu);

static unsigned long *__cpu_capacity;
#define cpu_capacity(cpu)	__cpu_capacity[cpu]

static u64 max_cpu_perf, min_cpu_perf;

static int __init get_cpu_for_node(struct device_node *node)
{
	struct device_node *cpu_node;
	int cpu;

	cpu_node = of_parse_phandle(node, "cpu", 0);
	if (!cpu_node)
		return -1;

	for_each_possible_cpu(cpu) {
		if (of_get_cpu_node(cpu, NULL) == cpu_node) {
			of_node_put(cpu_node);
			return cpu;
		}
	}

	pr_crit("Unable to find CPU node for %s\n", cpu_node->full_name);

	of_node_put(cpu_node);
	return -1;
}

static int __init parse_core(struct device_node *core, int cluster_id,
			     int core_id)
{
	char name[10];
	bool leaf = true;
	int i = 0;
	int cpu;
	struct device_node *t;

	do {
		snprintf(name, sizeof(name), "thread%d", i);
		t = of_get_child_by_name(core, name);
		if (t) {
			leaf = false;
			cpu = get_cpu_for_node(t);
			if (cpu >= 0) {
				cpu_topology[cpu].socket_id = cluster_id;
				cpu_topology[cpu].core_id = core_id;
				cpu_topology[cpu].thread_id = i;
			} else {
				pr_err("%s: Can't get CPU for thread\n",
				       t->full_name);
				of_node_put(t);
				return -EINVAL;
			}
			of_node_put(t);
		}
		i++;
	} while (t);

	cpu = get_cpu_for_node(core);
	if (cpu >= 0) {
		if (!leaf) {
			pr_err("%s: Core has both threads and CPU\n",
			       core->full_name);
			return -EINVAL;
		}

		cpu_topology[cpu].socket_id = cluster_id;
		cpu_topology[cpu].core_id = core_id;
	} else if (leaf) {
		pr_err("%s: Can't get CPU for leaf core\n", core->full_name);
		return -EINVAL;
	}

	return 0;
}

static int __init parse_cluster(struct device_node *cluster, int depth)
{
	char name[10];
	bool leaf = true;
	bool has_cores = false;
	struct device_node *c;
	static int cluster_id __initdata;
	int core_id = 0;
	int i, ret;

	/*
	 * First check for child clusters; we currently ignore any
	 * information about the nesting of clusters and present the
	 * scheduler with a flat list of them.
	 */
	i = 0;
	do {
		snprintf(name, sizeof(name), "cluster%d", i);
		c = of_get_child_by_name(cluster, name);
		if (c) {
			leaf = false;
			ret = parse_cluster(c, depth + 1);
			of_node_put(c);
			if (ret != 0)
				return ret;
		}
		i++;
	} while (c);

	/* Now check for cores */
	i = 0;
	do {
		snprintf(name, sizeof(name), "core%d", i);
		c = of_get_child_by_name(cluster, name);
		if (c) {
			has_cores = true;

			if (depth == 0) {
				pr_err("%s: cpu-map children should be clusters\n",
				       c->full_name);
				of_node_put(c);
				return -EINVAL;
			}

			if (leaf) {
				ret = parse_core(c, cluster_id, core_id++);
			} else {
				pr_err("%s: Non-leaf cluster with core %s\n",
				       cluster->full_name, name);
				ret = -EINVAL;
			}

			of_node_put(c);
			if (ret != 0)
				return ret;
		}
		i++;
	} while (c);

	if (leaf && !has_cores)
		pr_warn("%s: empty cluster\n", cluster->full_name);

	if (leaf)
		cluster_id++;

	return 0;
}

static int __init parse_dt_topology(void)
{
	struct device_node *cn, *map;
	int ret = 0;
	int cpu;

	cn = of_find_node_by_path("/cpus");
	if (!cn) {
		pr_err("No CPU information found in DT\n");
		return 0;
	}

	/*
	 * When topology is provided cpu-map is essentially a root
	 * cluster with restricted subnodes.
	 */
	map = of_get_child_by_name(cn, "cpu-map");
	if (!map)
		goto out;

	ret = parse_cluster(map, 0);
	if (ret != 0)
		goto out_map;

	/*
	 * Check that all cores are in the topology; the SMP code will
	 * only mark cores described in the DT as possible.
	 */
	for_each_possible_cpu(cpu)
		if (cpu_topology[cpu].socket_id == -1)
			ret = -EINVAL;

out_map:
	of_node_put(map);
out:
	of_node_put(cn);
	return ret;
}

static void __init parse_dt_cpu_capacity(void)
{
	const struct cpu_efficiency *cpu_eff;
	struct device_node *cn = NULL;
	int cpu = 0, i = 0;

	__cpu_capacity = kcalloc(nr_cpu_ids, sizeof(*__cpu_capacity),
				 GFP_NOWAIT);

	min_cpu_perf = ULONG_MAX;
	max_cpu_perf = 0;

	min_cpu_perf = ULONG_MAX;
	max_cpu_perf = 0;
	for_each_possible_cpu(cpu) {
		const u32 *rate;
		int len;
		u64 cpu_perf;

		/* too early to use cpu->of_node */
		cn = of_get_cpu_node(cpu, NULL);
		if (!cn) {
			pr_debug("missing device node for CPU %d\n", cpu);
			continue;
		}

		for (cpu_eff = table_efficiency; cpu_eff->compatible; cpu_eff++)
			if (of_device_is_compatible(cn, cpu_eff->compatible))
				break;

		if (cpu_eff->compatible == NULL)
			continue;

		rate = of_get_property(cn, "clock-frequency", &len);
		if (!rate || len != 4) {
			pr_debug("%s missing clock-frequency property\n",
				cn->full_name);
			continue;
		}

		cpu_perf = ((be32_to_cpup(rate)) >> 20) * cpu_eff->efficiency;
		cpu_capacity(cpu) = cpu_perf;

		max_cpu_perf = max(max_cpu_perf, cpu_perf);
		min_cpu_perf = min(min_cpu_perf, cpu_perf);
		i++;
	}

	if (i < num_possible_cpus()) {
		max_cpu_perf = 0;
		min_cpu_perf = 0;
	}
}

/*
 * Scheduler load-tracking scale-invariance
 *
 * Provides the scheduler with a scale-invariance correction factor that
 * compensates for frequency scaling.
 */

static DEFINE_PER_CPU(atomic_long_t, cpu_freq_capacity);
static DEFINE_PER_CPU(atomic_long_t, cpu_max_freq);
static DEFINE_PER_CPU(atomic_long_t, cpu_min_freq);

/* cpufreq callback function setting current cpu frequency */
void arch_scale_set_curr_freq(int cpu, unsigned long freq)
{
	unsigned long max = atomic_long_read(&per_cpu(cpu_max_freq, cpu));
	unsigned long curr;

	if (!max)
		return;

	curr = (freq * SCHED_CAPACITY_SCALE) / max;

	atomic_long_set(&per_cpu(cpu_freq_capacity, cpu), curr);
}

/* cpufreq callback function setting max cpu frequency */
void arch_scale_set_max_freq(int cpu, unsigned long freq)
{
	atomic_long_set(&per_cpu(cpu_max_freq, cpu), freq);
}

void arch_scale_set_min_freq(int cpu, unsigned long freq)
{
	atomic_long_set(&per_cpu(cpu_min_freq, cpu), freq);
}

unsigned long arch_scale_get_max_freq(int cpu)
{
	unsigned long max = atomic_long_read(&per_cpu(cpu_max_freq, cpu));

	return max;
}

unsigned long arch_scale_get_min_freq(int cpu)
{
	unsigned long min = atomic_long_read(&per_cpu(cpu_min_freq, cpu));

	return min;
}

unsigned long arch_scale_freq_capacity(struct sched_domain *sd, int cpu)
{
	unsigned long curr = atomic_long_read(&per_cpu(cpu_freq_capacity, cpu));

	if (!curr)
		return SCHED_CAPACITY_SCALE;

	return curr;
}

unsigned long arch_get_max_cpu_capacity(int cpu)
{
	return per_cpu(cpu_scale, cpu);
}

unsigned long arch_get_cur_cpu_capacity(int cpu)
{
	unsigned long scale_freq;

	scale_freq  = atomic_long_read(&per_cpu(cpu_freq_capacity, cpu));

	if (!scale_freq)
		scale_freq = SCHED_CAPACITY_SCALE;

	return (per_cpu(cpu_scale, cpu) * scale_freq / SCHED_CAPACITY_SCALE);
}

static int cpu_topology_init;
void __init arch_build_cpu_topology_domain(void)
{
	int cpuid;

	init_cpu_topology();

	/* update core and thread sibling masks */
	for_each_possible_cpu(cpuid) {
		update_siblings_masks(cpuid);
		update_cpu_capacity(cpuid);
	}

	cpu_topology_init = 1;
}

/*
 * Extras of CPU & Cluster functions
 */
int arch_cpu_is_big(unsigned int cpu)
{
	struct cputopo_arm *cpu_topo = &cpu_topology[cpu];

	switch (cpu_topo->partno) {
	case ARM_CPU_PART_CORTEX_A57:
	case ARM_CPU_PART_CORTEX_A72:
		return 1;
	default:
		return 0;
	}
}

int arch_cpu_is_little(unsigned int cpu)
{
	return !arch_cpu_is_big(cpu);
}

int arch_is_smp(void)
{
	static int __arch_smp = -1;

	if (__arch_smp != -1)
		return __arch_smp;

	__arch_smp = (max_cpu_perf != min_cpu_perf) ? 0 : 1;

	return __arch_smp;
}

int arch_get_nr_clusters(void)
{
	static int __arch_nr_clusters = -1;
	int max_id = 0;
	unsigned int cpu;

	if (__arch_nr_clusters != -1)
		return __arch_nr_clusters;

	/* assume socket id is monotonic increasing without gap. */
	for_each_possible_cpu(cpu) {
		struct cputopo_arm *cpu_topo = &cpu_topology[cpu];

		if (cpu_topo->socket_id > max_id)
			max_id = cpu_topo->socket_id;
	}
	__arch_nr_clusters = max_id + 1;
	return __arch_nr_clusters;
}

int arch_is_multi_cluster(void)
{
	return arch_get_nr_clusters() > 1 ? 1 : 0;
}

int arch_get_cluster_id(unsigned int cpu)
{
	struct cputopo_arm *cpu_topo = &cpu_topology[cpu];

	return cpu_topo->socket_id < 0 ? 0 : cpu_topo->socket_id;
}

void arch_get_cluster_cpus(struct cpumask *cpus, int cluster_id)
{
	unsigned int cpu;

	cpumask_clear(cpus);
	for_each_possible_cpu(cpu) {
		struct cputopo_arm *cpu_topo = &cpu_topology[cpu];

		if (cpu_topo->socket_id == cluster_id)
			cpumask_set_cpu(cpu, cpus);
	}
}

int arch_better_capacity(unsigned int cpu)
{
	return cpu_capacity(cpu) > min_cpu_perf;
}

/*
 * Heterogenous CPU capacity compare function
 * Only inspect lowest id of cpus in same domain.
 * Assume CPUs in same domain has same capacity.
 */
struct cluster_info {
	struct hmp_domain *hmpd;
	int cpu;
	bool is_big;
	unsigned long cpu_perf;
};

static inline __init void fillin_cluster(struct cluster_info *cinfo,
		struct hmp_domain *hmpd)
{
	int cpu;
	unsigned long cpu_perf;

	cinfo->hmpd = hmpd;
	cinfo->cpu = cpumask_any(&cinfo->hmpd->possible_cpus);
	cinfo->is_big = arch_cpu_is_big(cinfo->cpu);

	for_each_cpu(cpu, &hmpd->possible_cpus) {
		cpu_perf = cpu_capacity(cpu);
		if (cpu_perf > 0)
			break;
	}
	cinfo->cpu_perf = cpu_perf;

	if (cpu_perf == 0)
		pr_info("Uninitialized CPU performance (CPU mask: %lx)",
				cpumask_bits(&hmpd->possible_cpus)[0]);
}

/*
 * Negative, if @a should sort before @b
 * Positive, if @a should sort after @b.
 * Return 0, if ordering is to be preserved
 */
int __init hmp_compare(void *priv, struct list_head *a, struct list_head *b)
{
	struct cluster_info ca;
	struct cluster_info cb;

	fillin_cluster(&ca, list_entry(a, struct hmp_domain, hmp_domains));
	fillin_cluster(&cb, list_entry(b, struct hmp_domain, hmp_domains));

	/* Handle diff CPU type */
	if (ca.is_big != cb.is_big)
		return ca.is_big ? -1 : 1;

	return (ca.cpu_perf > cb.cpu_perf) ? -1 : 1;
}

void __init arch_init_hmp_domains(void)
{
	struct hmp_domain *domain;
	struct cpumask cpu_mask;
	int id, maxid;

	cpumask_clear(&cpu_mask);
	maxid = arch_get_nr_clusters();

	/*
	 * Initialize hmp_domains
	 * Must be ordered with respect to compute capacity.
	 * Fastest domain at head of list.
	 */
	for (id = 0; id < maxid; id++) {
		arch_get_cluster_cpus(&cpu_mask, id);
		domain = (struct hmp_domain *)
			kmalloc(sizeof(struct hmp_domain), GFP_KERNEL);
		if (domain) {
			cpumask_copy(&domain->possible_cpus, &cpu_mask);
			cpumask_and(&domain->cpus, cpu_online_mask,
				&domain->possible_cpus);
			list_add(&domain->hmp_domains, &hmp_domains);
		}
	}

	/*
	 * Sorting HMP domain by CPU capacity
	 */
	list_sort(NULL, &hmp_domains, &hmp_compare);

}

#ifdef CONFIG_MTK_SCHED_RQAVG_KS
/* To add this function for sched_avg.c */
unsigned long get_cpu_orig_capacity(unsigned int cpu)
{
	u64 capacity = cpu_capacity(cpu);

	if (!capacity || !max_cpu_perf)
		return 1024;

	capacity *= SCHED_CAPACITY_SCALE;
	capacity = div64_u64(capacity, max_cpu_perf);

	return capacity;
}
#endif
