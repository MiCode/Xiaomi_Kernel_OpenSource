/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Symbols stubs needed for GKI compliance
 */

#include "sched.h"

int sched_isolate_cpu(int cpu)
{
#ifdef CONFIG_MTK_SCHED_EXTENSION
	int err = -EINVAL;

	if (cpu >= nr_cpu_ids)
		return err;

	err = _sched_isolate_cpu(cpu);

	return err;
#else
	return -EINVAL;
#endif
}
EXPORT_SYMBOL_GPL(sched_isolate_cpu);

int sched_unisolate_cpu_unlocked(int cpu)
{
#ifdef CONFIG_MTK_SCHED_EXTENSION
	int err = -EINVAL;

	if (cpu >= nr_cpu_ids)
		return err;

	err = __sched_deisolate_cpu_unlocked(cpu);

	return err;
#else
	return -EINVAL;
#endif
}
EXPORT_SYMBOL_GPL(sched_unisolate_cpu_unlocked);

int sched_unisolate_cpu(int cpu)
{
#ifdef CONFIG_MTK_SCHED_EXTENSION
	int err = -EINVAL;

	if (cpu >= nr_cpu_ids)
		return err;

	err =  _sched_deisolate_cpu(cpu);
	return err;
#else
	return -EINVAL;
#endif
}
EXPORT_SYMBOL_GPL(sched_unisolate_cpu);

int set_task_boost(int boost, u64 period)
{
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(set_task_boost);
