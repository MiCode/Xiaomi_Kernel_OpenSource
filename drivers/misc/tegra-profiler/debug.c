/*
 * drivers/misc/tegra-profiler/debug.c
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/module.h>
#include <asm/irq_regs.h>

#include <linux/tegra_profiler.h>

#include "debug.h"
#include "hrt.h"
#include "tegra.h"

#ifdef QM_DEBUG_SAMPLES_ENABLE

static inline void
init_sample(struct quadd_record_data *record, struct pt_regs *regs)
{
	struct quadd_debug_data *s = &record->debug;

	record->magic = QUADD_RECORD_MAGIC;
	record->record_type = QUADD_RECORD_TYPE_DEBUG;

	if (!regs)
		regs = get_irq_regs();

	if (!regs)
		record->cpu_mode = QUADD_CPU_MODE_NONE;
	else
		record->cpu_mode = user_mode(regs) ?
			QUADD_CPU_MODE_USER : QUADD_CPU_MODE_KERNEL;

	s->cpu = quadd_get_processor_id();
	s->pid = 0;
	s->time = quadd_get_time();
	s->timer_period = 0;

	s->extra_value1 = 0;
	s->extra_value2 = 0;
	s->extra_value3 = 0;
}

void qm_debug_handler_sample(struct pt_regs *regs)
{
	struct quadd_record_data record;
	struct quadd_debug_data *s = &record.debug;

	init_sample(&record, regs);

	s->type = QM_DEBUG_SAMPLE_TYPE_TIMER_HANDLE;

	quadd_put_sample(&record, NULL, 0);
}

void qm_debug_timer_forward(struct pt_regs *regs, u64 period)
{
	struct quadd_record_data record;
	struct quadd_debug_data *s = &record.debug;

	init_sample(&record, regs);

	s->type = QM_DEBUG_SAMPLE_TYPE_TIMER_FORWARD;
	s->timer_period = period;

	quadd_put_sample(&record, NULL, 0);
}

void qm_debug_timer_start(struct pt_regs *regs, u64 period)
{
	struct quadd_record_data record;
	struct quadd_debug_data *s = &record.debug;

	init_sample(&record, regs);

	s->type = QM_DEBUG_SAMPLE_TYPE_TIMER_START;
	s->timer_period = period;

	quadd_put_sample(&record, NULL, 0);
}

void qm_debug_timer_cancel(void)
{
	struct quadd_record_data record;
	struct quadd_debug_data *s = &record.debug;

	init_sample(&record, NULL);

	s->type = QM_DEBUG_SAMPLE_TYPE_TIMER_CANCEL;

	quadd_put_sample(&record, NULL, 0);
}

void
qm_debug_task_sched_in(pid_t prev_pid, pid_t current_pid, int prev_nr_active)
{
	struct quadd_record_data record;
	struct quadd_debug_data *s = &record.debug;

	init_sample(&record, NULL);

	s->type = QM_DEBUG_SAMPLE_TYPE_SCHED_IN;

	s->extra_value1 = prev_pid;
	s->extra_value2 = current_pid;
	s->extra_value3 = prev_nr_active;

	quadd_put_sample(&record, NULL, 0);
}

void qm_debug_read_counter(int event_id, u32 prev_val, u32 val)
{
	struct quadd_record_data record;
	struct quadd_debug_data *s = &record.debug;

	init_sample(&record, NULL);

	s->type = QM_DEBUG_SAMPLE_TYPE_READ_COUNTER;

	s->extra_value1 = event_id;
	s->extra_value2 = prev_val;
	s->extra_value3 = val;

	quadd_put_sample(&record, NULL, 0);
}

void qm_debug_start_source(int source_type)
{
	struct quadd_record_data record;
	struct quadd_debug_data *s = &record.debug;

	init_sample(&record, NULL);

	s->type = QM_DEBUG_SAMPLE_TYPE_SOURCE_START;
	s->extra_value1 = source_type;

	quadd_put_sample(&record, NULL, 0);
}

void qm_debug_stop_source(int source_type)
{
	struct quadd_record_data record;
	struct quadd_debug_data *s = &record.debug;

	init_sample(&record, NULL);

	s->type = QM_DEBUG_SAMPLE_TYPE_SOURCE_STOP;
	s->extra_value1 = source_type;

	quadd_put_sample(&record, NULL, 0);
}

#endif	/* QM_DEBUG_SAMPLES_ENABLE */
