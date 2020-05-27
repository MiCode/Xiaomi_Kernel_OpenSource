#ifndef _LINUX_SCHED_ISOLATION_H
#define _LINUX_SCHED_ISOLATION_H

#include <linux/cpumask.h>
#include <linux/init.h>
#include <linux/tick.h>

enum hk_flags {
	HK_FLAG_TIMER		= 1,
	HK_FLAG_RCU		= (1 << 1),
	HK_FLAG_MISC		= (1 << 2),
	HK_FLAG_SCHED		= (1 << 3),
	HK_FLAG_TICK		= (1 << 4),
	HK_FLAG_DOMAIN		= (1 << 5),
	HK_FLAG_WQ		= (1 << 6),
};

#ifdef CONFIG_CPU_ISOLATION
DECLARE_STATIC_KEY_FALSE(housekeeping_overridden);
extern int housekeeping_any_cpu(enum hk_flags flags);
extern const struct cpumask *housekeeping_cpumask(enum hk_flags flags);
extern bool housekeeping_enabled(enum hk_flags flags);
extern void housekeeping_affine(struct task_struct *t, enum hk_flags flags);
extern bool housekeeping_test_cpu(int cpu, enum hk_flags flags);
extern void __init housekeeping_init(void);

#else

#ifdef CONFIG_SCHED_WALT
static inline int housekeeping_any_cpu(enum hk_flags flags)
{
	cpumask_t available;
	int cpu;

	cpumask_andnot(&available, cpu_online_mask, cpu_isolated_mask);
	cpu = cpumask_any(&available);
	if (cpu >= nr_cpu_ids)
		cpu = smp_processor_id();

	return cpu;
}
#else
static inline int housekeeping_any_cpu(enum hk_flags flags)
{
	return smp_processor_id();
}
#endif


static inline const struct cpumask *housekeeping_cpumask(enum hk_flags flags)
{
	return cpu_possible_mask;
}

static inline bool housekeeping_enabled(enum hk_flags flags)
{
	return false;
}

static inline void housekeeping_affine(struct task_struct *t,
				       enum hk_flags flags) { }
static inline void housekeeping_init(void) { }
#endif /* CONFIG_CPU_ISOLATION */

static inline bool housekeeping_cpu(int cpu, enum hk_flags flags)
{
#ifdef CONFIG_CPU_ISOLATION
	if (static_branch_unlikely(&housekeeping_overridden))
		return housekeeping_test_cpu(cpu, flags);
#endif
#ifdef CONFIG_SCHED_WALT
	return !cpu_isolated(cpu);
#else
	return true;
#endif
}

#endif /* _LINUX_SCHED_ISOLATION_H */
