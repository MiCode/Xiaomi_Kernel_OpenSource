/* SPDX-License-Identifier: GPL-2.0 */

#include <linux/string.h>
#include <linux/sched/clock.h>

struct preemptirq_info {
	u64	start_ts;
	u64	extra_start_ts;
	u64	extra_time;
	struct task_struct *task;
	int	pid;
	unsigned long	ncsw;
	void*	callback[5];
};

static inline void timing_reset(struct preemptirq_info *info)
{
	memset(info, 0, sizeof(*info));
}

struct preemptirq_settings {
	unsigned int warn_val;
};

#define timing_clock()		sched_clock()

#ifdef CONFIG_IRQSOFF_WARN
void start_irqsoff_extra_timing(void);
void stop_irqsoff_extra_timing(void);
#else
static inline void start_irqsoff_extra_timing(void) { }
static inline void stop_irqsoff_extra_timing(void) { }
#endif

#ifdef CONFIG_PREEMPT_WARN
void start_preemptoff_extra_timing(void);
void stop_preemptoff_extra_timing(void);
#else
static inline void start_preemptoff_extra_timing(void) { }
static inline void stop_preemptoff_extra_timing(void) { }
#endif

extern void show_stack(struct task_struct *task, unsigned long *sp, const char *loglvl);
