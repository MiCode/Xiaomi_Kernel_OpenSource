#ifndef _ASM_ARM_TOPOLOGY_H
#define _ASM_ARM_TOPOLOGY_H

#ifdef CONFIG_ARM_CPU_TOPOLOGY

#include <linux/cpufreq.h>
#include <linux/cpumask.h>

struct cputopo_arm {
	int thread_id;
	int core_id;
	int socket_id;
	unsigned int partno;
	cpumask_t thread_sibling;
	cpumask_t core_sibling;
};

extern struct cputopo_arm cpu_topology[NR_CPUS];
extern unsigned long arch_get_max_cpu_capacity(int cpu);
extern unsigned long arch_get_cur_cpu_capacity(int cpu);

#define topology_physical_package_id(cpu)	(cpu_topology[cpu].socket_id)
#define topology_core_id(cpu)		(cpu_topology[cpu].core_id)
#define topology_core_cpumask(cpu)	(&cpu_topology[cpu].core_sibling)
#define topology_sibling_cpumask(cpu)	(&cpu_topology[cpu].thread_sibling)
#define topology_max_cpu_capacity(cpu) (arch_get_max_cpu_capacity(cpu))
#define topology_cur_cpu_capacity(cpu) (arch_get_cur_cpu_capacity(cpu))

void init_cpu_topology(void);
void store_cpu_topology(unsigned int cpuid);
const struct cpumask *cpu_coregroup_mask(int cpu);

#ifdef CONFIG_CPU_FREQ
#define arch_scale_freq_capacity arch_scale_freq_capacity

extern unsigned long arch_scale_freq_capacity(struct sched_domain *sd, int cpu);
extern unsigned long
cpufreq_scale_freq_capacity(struct sched_domain *sd, int cpu);
#define arch_scale_max_freq_capacity cpufreq_scale_max_freq_capacity
extern unsigned long
cpufreq_scale_max_freq_capacity(struct sched_domain *sd, int cpu);
#define arch_scale_min_freq_capacity cpufreq_scale_min_freq_capacity
extern unsigned long
cpufreq_scale_min_freq_capacity(struct sched_domain *sd, int cpu);
#endif

#define arch_scale_cpu_capacity scale_cpu_capacity
extern unsigned long scale_cpu_capacity(struct sched_domain *sd, int cpu);

#else

static inline void init_cpu_topology(void) { }
static inline void store_cpu_topology(unsigned int cpuid) { }

#endif
/* Extras of CPU & Cluster functions */
extern int arch_cpu_is_big(unsigned int cpu);
extern int arch_cpu_is_little(unsigned int cpu);
extern int arch_is_multi_cluster(void);
extern int arch_is_smp(void);
extern int arch_get_nr_clusters(void);
extern int arch_get_cluster_id(unsigned int cpu);
extern void arch_get_cluster_cpus(struct cpumask *cpus, int cluster_id);
extern int arch_better_capacity(unsigned int cpu);
void arch_build_cpu_topology_domain(void);

#include <asm-generic/topology.h>

#endif /* _ASM_ARM_TOPOLOGY_H */
