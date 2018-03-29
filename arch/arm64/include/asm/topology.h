#ifndef __ASM_TOPOLOGY_H
#define __ASM_TOPOLOGY_H

#ifdef CONFIG_SMP

#include <linux/cpumask.h>

struct cpu_topology {
	int thread_id;
	int core_id;
	int cluster_id;
	unsigned int partno;
	cpumask_t thread_sibling;
	cpumask_t core_sibling;
};

extern struct cpu_topology cpu_topology[NR_CPUS];
extern unsigned long arch_get_max_cpu_capacity(int);
extern unsigned long arch_get_cur_cpu_capacity(int);

#define topology_physical_package_id(cpu)	(cpu_topology[cpu].cluster_id)
#define topology_core_id(cpu)		(cpu_topology[cpu].core_id)
#define topology_core_cpumask(cpu)	(&cpu_topology[cpu].core_sibling)
#define topology_thread_cpumask(cpu)	(&cpu_topology[cpu].thread_sibling)
#define topology_max_cpu_capacity(cpu) (arch_get_max_cpu_capacity(cpu))
#define topology_cur_cpu_capacity(cpu) (arch_get_cur_cpu_capacity(cpu))

void init_cpu_topology(void);
void store_cpu_topology(unsigned int cpuid);
const struct cpumask *cpu_coregroup_mask(int cpu);

/* Extras of CPU & Cluster functions */
extern int arch_cpu_is_big(unsigned int cpu);
extern int arch_cpu_is_little(unsigned int cpu);
extern int arch_is_multi_cluster(void);
extern int arch_is_smp(void);
extern int arch_get_nr_clusters(void);
extern int arch_get_cluster_id(unsigned int cpu);
extern void arch_get_cluster_cpus(struct cpumask *cpus, int cluster_id);
extern int arch_better_capacity(unsigned int cpu);

#else /* !CONFIG_ARM_CPU_TOPOLOGY */

static inline void init_cpu_topology(void) { }
static inline void store_cpu_topology(unsigned int cpuid) { }

static inline int arch_cpu_is_big(unsigned int cpu) { return 0; }
static inline int arch_cpu_is_little(unsigned int cpu) { return 1; }
static inline int arch_is_multi_cluster(void) { return 0; }
static inline int arch_is_smp(void) { return 1; }
static inline int arch_get_nr_clusters(void) { return 1; }
static inline int arch_get_cluster_id(unsigned int cpu) { return 0; }
static inline void arch_get_cluster_cpus(struct cpumask *cpus, int cluster_id)
{
	cpumask_clear(cpus);
	if (0 == cluster_id) {
		unsigned int cpu;

		for_each_possible_cpu(cpu)
			cpumask_set_cpu(cpu, cpus);
	}
}
static inline int arch_better_capacity(unsigned int cpu) { return 0; }

#endif /* CONFIG_ARM_CPU_TOPOLOGY */

#ifdef CONFIG_MTK_CPU_TOPOLOGY
void arch_build_cpu_topology_domain(void);
#endif

#include <asm-generic/topology.h>

#endif /* _ASM_ARM_TOPOLOGY_H */
