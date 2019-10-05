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
	{ "arm,cortex-a75", 3630 },
	{ "arm,cortex-a73", 3630 },
	{ "arm,cortex-a72", 4186 },
	{ "arm,cortex-a57", 3891 },
	{ "arm,cortex-a53", 2048 },
	{ "arm,cortex-a55", 2048 },
	{ "arm,cortex-a35", 1661 },
	{ NULL, },
};

static unsigned long *__cpu_capacity;
#define cpu_capacity(cpu)	__cpu_capacity[cpu]

static unsigned long max_cpu_perf, min_cpu_perf;


void __init parse_dt_cpu_capacity(void)
{
	const struct cpu_efficiency *cpu_eff;
	struct device_node *cn = NULL;
	int cpu = 0, i = 0;

	__cpu_capacity = kcalloc(nr_cpu_ids, sizeof(*__cpu_capacity),
				 GFP_NOWAIT);

	min_cpu_perf = ULONG_MAX;
	max_cpu_perf = 0;
	for_each_possible_cpu(cpu) {
		const u32 *rate;
		int len;
		unsigned long cpu_perf;

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
 * generic entry point for cpu mask construction, dedicated for
 * mediatek scheduler.
 */
static int cpu_topology_init;
void __init arch_build_cpu_topology_domain(void)
{
	WARN(smp_processor_id() != 0,
		"%s is supposed runs on CPU0 while kernel init", __func__);
	/*
	 * sched_init
	 *   |-> arch_build_cpu_topology_domain()
	 *       |-> init_cpu_topology()
	 *   |-> *scheduler_running = 1*
	 * ...
	 * rest_init
	 *   ^ fork kernel_init
	 *       |-> kernel_init_freeable
	 *        ...
	 *        (x)|-> init_cpu_topology
	 *
	 * here, we focus to build up cpu topology and domain
	 * before scheduler runs.
	 */
	pr_debug("[CPUTOPO][%s] build CPU topology and cluster.\n", __func__);
	init_cpu_topology();
	parse_dt_cpu_capacity();

	cpu_topology_init = 1;
}
#ifdef CONFIG_MTK_UNIFY_POWER

#include "../../../drivers/misc/mediatek/base/power/include/mtk_upower.h"

/* sd energy functions */
inline
const struct sched_group_energy * const cpu_cluster_energy(int cpu)
{
	struct sched_group_energy *sge = sge_array[cpu][SD_LEVEL1];
	int cluster_id = cpu_topology[cpu].cluster_id;
	struct upower_tbl_info **addr_ptr_tbl_info;
	struct upower_tbl_info *ptr_tbl_info;
	struct upower_tbl *ptr_tbl;

	if (!sge) {
		pr_warn("Invalid sched_group_energy for Cluster%d\n", cpu);
		return NULL;
	}

	addr_ptr_tbl_info = upower_get_tbl();
	ptr_tbl_info = *addr_ptr_tbl_info;

	ptr_tbl = ptr_tbl_info[UPOWER_BANK_CLS_BASE+cluster_id].p_upower_tbl;

	sge->nr_cap_states = ptr_tbl->row_num;
	sge->cap_states = ptr_tbl->row;
	sge->lkg_idx = ptr_tbl->lkg_idx;

	return sge;
}

inline
const struct sched_group_energy * const cpu_core_energy(int cpu)
{
	struct sched_group_energy *sge = sge_array[cpu][SD_LEVEL0];
	struct upower_tbl *ptr_tbl;

	if (!sge) {
		pr_warn("Invalid sched_group_energy for CPU%d\n", cpu);
		return NULL;
	}

	ptr_tbl = upower_get_core_tbl(cpu);

	sge->nr_cap_states = ptr_tbl->row_num;
	sge->cap_states = ptr_tbl->row;
	sge->lkg_idx = ptr_tbl->lkg_idx;

	return sge;
}
#endif

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

unsigned long arch_get_max_cpu_capacity(int cpu)
{
	return per_cpu(cpu_scale, cpu);
}

unsigned long arch_get_cur_cpu_capacity(int cpu)
{
	unsigned long scale_freq;

	scale_freq  = arch_scale_freq_capacity(NULL, cpu);

	if (!scale_freq)
		scale_freq = SCHED_CAPACITY_SCALE;

	return (per_cpu(cpu_scale, cpu) * scale_freq / SCHED_CAPACITY_SCALE);
}

int arch_is_smp(void)
{
	return (max_cpu_perf == min_cpu_perf) ? 1 : 0;
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
		struct cpu_topology *cpu_topo = &cpu_topology[cpu];

		if (cpu_topo->cluster_id > max_id)
			max_id = cpu_topo->cluster_id;
	}
	__arch_nr_clusters = max_id + 1;
	return __arch_nr_clusters;
}

int arch_is_multi_cluster(void)
{
	return (arch_get_nr_clusters() > 1);
}

int arch_get_cluster_id(unsigned int cpu)
{
	struct cpu_topology *cpu_topo = &cpu_topology[cpu];

	return cpu_topo->cluster_id < 0 ? 0 : cpu_topo->cluster_id;
}

void arch_get_cluster_cpus(struct cpumask *cpus, int cluster_id)
{
	unsigned int cpu;

	cpumask_clear(cpus);
	for_each_possible_cpu(cpu) {
		struct cpu_topology *cpu_topo = &cpu_topology[cpu];

		if (cpu_topo->cluster_id == cluster_id)
			cpumask_set_cpu(cpu, cpus);
	}
}

/*
 * Heterogenous CPU capacity compare function
 * Only inspect lowest id of cpus in same domain.
 * Assume CPUs in same domain has same capacity.
 */
struct cluster_info {
	struct hmp_domain *hmpd;
	unsigned long cpu_perf;
	int cpu;
};

static inline __init void fillin_cluster(struct cluster_info *cinfo,
		struct hmp_domain *hmpd)
{
	int cpu;
	unsigned long cpu_perf;

	cinfo->hmpd = hmpd;
	cinfo->cpu = cpumask_any(&cinfo->hmpd->possible_cpus);

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
	pr_info("Sort hmp_domains from little to big:\n");
	for_each_hmp_domain_L_first(domain) {
		pr_info("    cpumask: 0x%02lx\n",
				*cpumask_bits(&domain->possible_cpus));
	}

}
