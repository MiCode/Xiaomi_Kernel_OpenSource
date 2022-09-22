// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/module.h>
#include <sched/sched.h>

MODULE_LICENSE("GPL");

/**
 * idle_cpu - is a given CPU idle currently?
 * @cpu: the processor in question.
 *
 * Return: 1 if the CPU is currently idle. 0 otherwise.
 */
#if !IS_BUILTIN(CONFIG_MTK_SCHEDULER)
int idle_cpu(int cpu)
{
	struct rq *rq = cpu_rq(cpu);

	if (rq->curr != rq->idle)
		return 0;

	if (rq->nr_running)
		return 0;

#ifdef CONFIG_SMP
	if (rq->ttwu_pending)
		return 0;
#endif

	return 1;
}
#endif

#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
/**
 * dequeue_idle_cpu - is a given CPU idle currently?
 * this function is used in after_dequeue  the curr task
 * does not switch to rq->idle
 * @cpu: the processor in question.
 *
 * Return: 1 if the CPU is currently idle. 0 otherwise.
 */
int dequeue_idle_cpu(int cpu)
{
	struct rq *rq = cpu_rq(cpu);

#ifdef CONFIG_SMP
	if (rq->ttwu_pending)
		return 0;
#endif
	if (rq->nr_running == 1)
		return 1;

	return 0;
}
#endif

#ifdef CONFIG_UCLAMP_TASK
/**
 * uclamp_rq_util_with - clamp @util with @rq and @p effective uclamp values.
 * @rq:		The rq to clamp against. Must not be NULL.
 * @util:	The util value to clamp.
 * @p:		The task to clamp against. Can be NULL if you want to clamp
 *		against @rq only.
 *
 * Clamps the passed @util to the max(@rq, @p) effective uclamp values.
 *
 * If sched_uclamp_used static key is disabled, then just return the util
 * without any clamping since uclamp aggregation at the rq level in the fast
 * path is disabled, rendering this operation a NOP.
 *
 * Use uclamp_eff_value() if you don't care about uclamp values at rq level. It
 * will return the correct effective uclamp value of the task even if the
 * static key is disabled.
 */
__always_inline
unsigned long mtk_uclamp_rq_util_with(struct rq *rq, unsigned long util,
				  struct task_struct *p)
{
	unsigned long min_util;
	unsigned long max_util;

	if (!static_branch_likely(&sched_uclamp_used))
		return util;

	min_util = READ_ONCE(rq->uclamp[UCLAMP_MIN].value);
	max_util = READ_ONCE(rq->uclamp[UCLAMP_MAX].value);

	if (p) {
		min_util = max(min_util, uclamp_eff_value(p, UCLAMP_MIN));
#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
		max_util = uclamp_eff_value(p, UCLAMP_MAX);
#else
		max_util = max(max_util, uclamp_eff_value(p, UCLAMP_MAX));
#endif
	}

	/*
	 * Since CPU's {min,max}_util clamps are MAX aggregated considering
	 * RUNNABLE tasks with _different_ clamps, we can end up with an
	 * inversion. Fix it now when the clamps are applied.
	 */
	if (unlikely(min_util >= max_util))
		return min_util;

	return clamp(util, min_util, max_util);
}
#else /* CONFIG_UCLAMP_TASK */
static inline
unsigned long mtk_uclamp_rq_util_with(struct rq *rq, unsigned long util,
				  struct task_struct *p)
{
	return util;
}
#endif /* CONFIG_UCLAMP_TASK */
