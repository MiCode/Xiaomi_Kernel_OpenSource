#ifndef _ASM_ARM_TOPOLOGY_H
#define _ASM_ARM_TOPOLOGY_H

#ifdef CONFIG_ARM_CPU_TOPOLOGY

#include <linux/cpumask.h>

struct cputopo_arm {
	int thread_id;
	int core_id;
	int socket_id;
	cpumask_t thread_sibling;
	cpumask_t core_sibling;
};

extern struct cputopo_arm cpu_topology[NR_CPUS];

#define topology_physical_package_id(cpu)	(cpu_topology[cpu].socket_id)
#define topology_core_id(cpu)		(cpu_topology[cpu].core_id)
#define topology_core_cpumask(cpu)	(&cpu_topology[cpu].core_sibling)
#define topology_thread_cpumask(cpu)	(&cpu_topology[cpu].thread_sibling)
#ifdef CONFIG_ARCH_SCALE_INVARIANT_CPU_CAPACITY
#define CPUPOWER_FREQSCALE_SHIFT 10
#define CPUPOWER_FREQSCALE_DEFAULT (1L << CPUPOWER_FREQSCALE_SHIFT)
extern unsigned long arch_get_max_cpu_capacity(int);
extern unsigned long arch_get_cpu_capacity(int);
extern int arch_get_invariant_power_enabled(void);
extern int arch_get_cpu_throttling(int);

#define topology_max_cpu_capacity(cpu)	(arch_get_max_cpu_capacity(cpu))
#define topology_cpu_capacity(cpu)	(arch_get_cpu_capacity(cpu))
#define topology_cpu_throttling(cpu)	(arch_get_cpu_capacity(cpu))
#define topology_cpu_inv_power_en(void) (arch_get_invariant_power_enabled())
#endif /* CONFIG_ARCH_SCALE_INVARIANT_CPU_CAPACITY */

#define mc_capable()	(cpu_topology[0].socket_id != -1)
#define smt_capable()	(cpu_topology[0].thread_id != -1)

void init_cpu_topology(void);
void store_cpu_topology(unsigned int cpuid);
const struct cpumask *cpu_coregroup_mask(int cpu);
int cluster_to_logical_mask(unsigned int socket_id, cpumask_t *cluster_mask);

#ifdef CONFIG_DISABLE_CPU_SCHED_DOMAIN_BALANCE

#if defined (CONFIG_HMP_PACK_SMALL_TASK) && !defined(CONFIG_MTK_SCHED_CMP)
/* Common values for CPUs */
#ifndef SD_CPU_INIT
#define SD_CPU_INIT (struct sched_domain) {				\
	.min_interval		= 1,					\
	.max_interval		= 4,					\
	.busy_factor		= 64,					\
	.imbalance_pct		= 125,					\
	.cache_nice_tries	= 1,					\
	.busy_idx		= 2,					\
	.idle_idx		= 1,					\
	.newidle_idx		= 0,					\
	.wake_idx		= 0,					\
	.forkexec_idx		= 0,					\
									\
	.flags			= 0*SD_LOAD_BALANCE			\
				| 1*SD_BALANCE_NEWIDLE			\
				| 1*SD_BALANCE_EXEC			\
				| 1*SD_BALANCE_FORK			\
				| 0*SD_BALANCE_WAKE			\
				| 1*SD_WAKE_AFFINE			\
				| 0*SD_SHARE_CPUPOWER			\
				| 0*SD_SHARE_PKG_RESOURCES		\
				| arch_sd_share_power_line()		\
				| 0*SD_SERIALIZE			\
				,					\
	.last_balance		 = jiffies,				\
	.balance_interval	= 1,					\
}
#endif
#endif /* CONFIG_HMP_PACK_SMALL_TASK */

#endif /* CONFIG_DISABLE_CPU_SCHED_DOMAIN_BALANCE */

/* CPU cluster functions */
extern void arch_build_cpu_topology_domain(void);
extern int arch_cpu_is_big(unsigned int cpu);
extern int arch_cpu_is_little(unsigned int cpu);
extern int arch_is_multi_cluster(void);
extern int arch_is_big_little(void);
extern int arch_get_nr_clusters(void);
extern int arch_get_cluster_id(unsigned int cpu);
extern void arch_get_cluster_cpus(struct cpumask *cpus, int cluster_id);
extern void arch_get_big_little_cpus(struct cpumask *big, struct cpumask *little);

#else /* !CONFIG_ARM_CPU_TOPOLOGY */

static inline void init_cpu_topology(void) { }
static inline void store_cpu_topology(unsigned int cpuid) { }
static inline int cluster_to_logical_mask(unsigned int socket_id,
	cpumask_t *cluster_mask) { return -EINVAL; }

static inline void arch_build_cpu_topology_domain(void) {}
static inline int arch_cpu_is_big(unsigned int cpu) { return 0; }
static inline int arch_cpu_is_little(unsigned int cpu) { return 1; }
static inline int arch_is_multi_cluster(void) { return 0; }
static inline int arch_is_big_little(void) { return 0; }
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
static inline void arch_get_big_little_cpus(struct cpumask *big,struct cpumask *little)
{
    unsigned int cpu;
    cpumask_clear(big);
    cpumask_clear(little);
    for_each_possible_cpu(cpu)
        cpumask_set_cpu(cpu, little);
}

#endif /* CONFIG_ARM_CPU_TOPOLOGY */

#include <asm-generic/topology.h>

#endif /* _ASM_ARM_TOPOLOGY_H */
