// MIUI ADD: Performance_FramePredictBoost
#ifndef _HYPERFRAME_CPU_TRACER_H
#define _HYPERFRAME_CPU_TRACER_H

#include "hyperframe_base.h"

void cpu_frequency_tracer(void *ignore, unsigned int frequency, unsigned int cpu_id);

void sched_switch_tracer(void *ignore,
		bool preempt,
		struct task_struct *prev,
		struct task_struct *next,
		unsigned int prev_state);

struct tracepoints_table {
	const char *name;
	void *func;
	struct tracepoint *tp;
	bool registered;
};

static struct tracepoints_table cpu_tracepoints[] = {
	{.name = "cpu_frequency", .func = cpu_frequency_tracer},
	{.name = "sched_switch", .func = sched_switch_tracer},
};

void hyperframe_cpu_tracer_init(void);
void hyperframe_cpu_tracer_exit(void);

#define FOR_EACH_INTEREST(i) \
	for (i = 0; i < sizeof(cpu_tracepoints) / sizeof(struct tracepoints_table); i++)

#endif
// END Performance_FramePredictBoost