// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include "sched.h"

bool is_intra_domain(int prev, int target)
{
	return arch_cpu_cluster_id(prev) ==
			arch_cpu_cluster_id(target);
}

#if defined(CONFIG_ENERGY_MODEL) && defined(CONFIG_CPU_FREQ_GOV_SCHEDUTIL)
/*
 * The cpu types are distinguished using a list of perf_order_domains
 * which each represent one cpu type using a cpumask.
 * The list is assumed ordered by compute capacity with the
 * fastest domain first.
 */

/* Perf order domain common utils */
LIST_HEAD(perf_order_domains);
DEFINE_PER_CPU(struct perf_order_domain *, perf_order_cpu_domain);
static bool pod_ready;

bool pod_is_ready(void)
{
	return pod_ready;
}

/* Initialize perf_order_cpu_domains */
void perf_order_cpu_mask_setup(void)
{
	struct perf_order_domain *domain;
	struct list_head *pos;
	int cpu;

	list_for_each(pos, &perf_order_domains) {
		domain = list_entry(pos, struct perf_order_domain,
					perf_order_domains);

		for_each_cpu(cpu, &domain->possible_cpus)
			per_cpu(perf_order_cpu_domain, cpu) = domain;
	}
}

/*
 *Perf domain capacity compare function
 * Only inspect lowest id of cpus in same domain.
 * Assume CPUs in same domain has same capacity.
 */
struct cluster_info {
	struct perf_order_domain *pod;
	unsigned long cpu_perf;
	int cpu;
};

static inline void fillin_cluster(struct cluster_info *cinfo,
		struct perf_order_domain *pod)
{
	int cpu;
	unsigned long cpu_perf;

	cinfo->pod = pod;
	cinfo->cpu = cpumask_any(&cinfo->pod->possible_cpus);

	for_each_cpu(cpu, &pod->possible_cpus) {
		cpu_perf = arch_scale_cpu_capacity(NULL, cpu);
		pr_info("cpu=%d, cpu_perf=%lu\n", cpu, cpu_perf);
		if (cpu_perf > 0)
			break;
	}
	cinfo->cpu_perf = cpu_perf;

	if (cpu_perf == 0)
		pr_info("Uninitialized CPU performance (CPU mask: %lx)",
				cpumask_bits(&pod->possible_cpus)[0]);
}

/*
 * Negative, if @a should sort before @b
 * Positive, if @a should sort after @b.
 * Return 0, if ordering is to be preserved
 */
int perf_domain_compare(void *priv, struct list_head *a, struct list_head *b)
{
	struct cluster_info ca;
	struct cluster_info cb;

	fillin_cluster(&ca, list_entry(a, struct perf_order_domain,
				perf_order_domains));
	fillin_cluster(&cb, list_entry(b, struct perf_order_domain,
				perf_order_domains));

	return (ca.cpu_perf > cb.cpu_perf) ? -1 : 1;
}

void init_perf_order_domains(struct perf_domain *pd)
{
	struct perf_order_domain *domain;

	pr_info("Initializing perf order domain:\n");

	if (!pd) {
		pr_info("Perf domain is not ready!\n");
		return;
	}

	for (; pd; pd = pd->next) {
		domain = (struct perf_order_domain *)
			kmalloc(sizeof(struct perf_order_domain), GFP_KERNEL);
		if (domain) {
			cpumask_copy(&domain->possible_cpus,
					perf_domain_span(pd));
			cpumask_and(&domain->cpus, cpu_online_mask,
				&domain->possible_cpus);
			list_add(&domain->perf_order_domains,
					&perf_order_domains);
		}
	}

	if (list_empty(&perf_order_domains)) {
		pr_info("Perf order domain list is empty!\n");
		return;
	}

	/*
	 * Sorting Perf domain by CPU capacity
	 */
	list_sort(NULL, &perf_order_domains, &perf_domain_compare);
	pr_info("Sort perf_domains from little to big:\n");
	for_each_perf_domain_ascending(domain) {
		pr_info("    cpumask: 0x%02lx\n",
				*cpumask_bits(&domain->possible_cpus));
	}

	/* Initialize perf_order_cpu_domains */
	perf_order_cpu_mask_setup();

	pod_ready = true;

	pr_info("Initializing perf order domain done\n");
}
EXPORT_SYMBOL(init_perf_order_domains);

/* Check if cpu is in fastest perf_order_domain */
inline unsigned int cpu_is_fastest(int cpu)
{
	struct list_head *pos;

	if (!pod_is_ready()) {
		pr_info("Perf order domain is not ready!\n");
		return -1;
	}

	pos = &perf_order_cpu_domain(cpu)->perf_order_domains;
	return pos == perf_order_domains.next;
}
EXPORT_SYMBOL(cpu_is_fastest);

/* Check if cpu is in slowest perf_order_domain */
inline unsigned int cpu_is_slowest(int cpu)
{
	struct list_head *pos;

	if (!pod_is_ready()) {
		pr_info("Perf order domain is not ready!\n");
		return -1;
	}

	pos = &perf_order_cpu_domain(cpu)->perf_order_domains;
	return list_is_last(pos, &perf_order_domains);
}
EXPORT_SYMBOL(cpu_is_slowest);
#endif
