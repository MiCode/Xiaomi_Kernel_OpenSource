#undef TRACE_SYSTEM
#define TRACE_SYSTEM power

#if !defined(_TRACE_POWER_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_POWER_H

#include <linux/ktime.h>
#include <linux/tracepoint.h>

DECLARE_EVENT_CLASS(cpu,

	TP_PROTO(unsigned int state, unsigned int cpu_id),

	TP_ARGS(state, cpu_id),

	TP_STRUCT__entry(
		__field(	u32,		state		)
		__field(	u32,		cpu_id		)
	),

	TP_fast_assign(
		__entry->state = state;
		__entry->cpu_id = cpu_id;
	),

	TP_printk("state=%lu cpu_id=%lu", (unsigned long)__entry->state,
		  (unsigned long)__entry->cpu_id)
);

DEFINE_EVENT(cpu, cpu_idle,

	TP_PROTO(unsigned int state, unsigned int cpu_id),

	TP_ARGS(state, cpu_id)
);

/* This file can get included multiple times, TRACE_HEADER_MULTI_READ at top */
#ifndef _PWR_EVENT_AVOID_DOUBLE_DEFINING
#define _PWR_EVENT_AVOID_DOUBLE_DEFINING

#define PWR_EVENT_EXIT -1
#endif

DEFINE_EVENT(cpu, cpu_frequency,

	TP_PROTO(unsigned int frequency, unsigned int cpu_id),

	TP_ARGS(frequency, cpu_id)
);

TRACE_EVENT(cpu_frequency_switch_start,

	TP_PROTO(unsigned int start_freq, unsigned int end_freq,
		 unsigned int cpu_id),

	TP_ARGS(start_freq, end_freq, cpu_id),

	TP_STRUCT__entry(
		__field(	u32,		start_freq	)
		__field(	u32,		end_freq	)
		__field(        u32,            cpu_id          )
	),

	TP_fast_assign(
		__entry->start_freq = start_freq;
		__entry->end_freq = end_freq;
		__entry->cpu_id = cpu_id;
	),

	TP_printk("start=%lu end=%lu cpu_id=%lu",
		  (unsigned long)__entry->start_freq,
		  (unsigned long)__entry->end_freq,
		  (unsigned long)__entry->cpu_id)
);

TRACE_EVENT(cpu_frequency_limits,

	TP_PROTO(unsigned int max_freq, unsigned int min_freq,
		unsigned int cpu_id),

	TP_ARGS(max_freq, min_freq, cpu_id),

	TP_STRUCT__entry(
		__field(	u32,		min_freq	)
		__field(	u32,		max_freq	)
		__field(	u32,		cpu_id		)
	),

	TP_fast_assign(
		__entry->min_freq = min_freq;
		__entry->max_freq = min_freq;
		__entry->cpu_id = cpu_id;
	),

	TP_printk("min=%lu max=%lu cpu_id=%lu",
		  (unsigned long)__entry->min_freq,
		  (unsigned long)__entry->max_freq,
		  (unsigned long)__entry->cpu_id)
);

TRACE_EVENT(cpu_frequency_switch_end,

	TP_PROTO(unsigned int cpu_id),

	TP_ARGS(cpu_id),

	TP_STRUCT__entry(
		__field(	u32,		cpu_id		)
	),

	TP_fast_assign(
		__entry->cpu_id = cpu_id;
	),

	TP_printk("cpu_id=%lu", (unsigned long)__entry->cpu_id)
);

DECLARE_EVENT_CLASS(set,
	TP_PROTO(u32 cpu_id, unsigned long currfreq,
			unsigned long load),
	TP_ARGS(cpu_id, currfreq, load),

	TP_STRUCT__entry(
	    __field(u32, cpu_id)
	    __field(unsigned long, currfreq)
	    __field(unsigned long, load)
	),

	TP_fast_assign(
	    __entry->cpu_id = (u32) cpu_id;
	    __entry->currfreq = currfreq;
	    __entry->load = load;
	),

	TP_printk("cpu=%u currfreq=%lu load=%lu",
	      __entry->cpu_id, __entry->currfreq,
	      __entry->load)
);

DEFINE_EVENT(set, cpufreq_sampling_event,
	TP_PROTO(u32 cpu_id, unsigned long currfreq,
		unsigned long load),
	TP_ARGS(cpu_id, currfreq, load)
);

DEFINE_EVENT(set, cpufreq_freq_synced,
	TP_PROTO(u32 cpu_id, unsigned long currfreq,
		unsigned long load),
	TP_ARGS(cpu_id, currfreq, load)
);

TRACE_EVENT(machine_suspend,

	TP_PROTO(unsigned int state),

	TP_ARGS(state),

	TP_STRUCT__entry(
		__field(	u32,		state		)
	),

	TP_fast_assign(
		__entry->state = state;
	),

	TP_printk("state=%lu", (unsigned long)__entry->state)
);

DECLARE_EVENT_CLASS(wakeup_source,

	TP_PROTO(const char *name, unsigned int state),

	TP_ARGS(name, state),

	TP_STRUCT__entry(
		__string(       name,           name            )
		__field(        u64,            state           )
	),

	TP_fast_assign(
		__assign_str(name, name);
		__entry->state = state;
	),

	TP_printk("%s state=0x%lx", __get_str(name),
		(unsigned long)__entry->state)
);

DEFINE_EVENT(wakeup_source, wakeup_source_activate,

	TP_PROTO(const char *name, unsigned int state),

	TP_ARGS(name, state)
);

DEFINE_EVENT(wakeup_source, wakeup_source_deactivate,

	TP_PROTO(const char *name, unsigned int state),

	TP_ARGS(name, state)
);

/*
 * The clock events are used for clock enable/disable and for
 *  clock rate change
 */
DECLARE_EVENT_CLASS(clock,

	TP_PROTO(const char *name, unsigned int state, unsigned int cpu_id),

	TP_ARGS(name, state, cpu_id),

	TP_STRUCT__entry(
		__string(       name,           name            )
		__field(        u64,            state           )
		__field(        u64,            cpu_id          )
	),

	TP_fast_assign(
		__assign_str(name, name);
		__entry->state = state;
		__entry->cpu_id = cpu_id;
	),

	TP_printk("%s state=%lu cpu_id=%lu", __get_str(name),
		(unsigned long)__entry->state, (unsigned long)__entry->cpu_id)
);

DEFINE_EVENT(clock, clock_enable,

	TP_PROTO(const char *name, unsigned int state, unsigned int cpu_id),

	TP_ARGS(name, state, cpu_id)
);

DEFINE_EVENT(clock, clock_disable,

	TP_PROTO(const char *name, unsigned int state, unsigned int cpu_id),

	TP_ARGS(name, state, cpu_id)
);

DEFINE_EVENT(clock, clock_set_rate,

	TP_PROTO(const char *name, unsigned int state, unsigned int cpu_id),

	TP_ARGS(name, state, cpu_id)
);

TRACE_EVENT(clock_set_parent,

	TP_PROTO(const char *name, const char *parent_name),

	TP_ARGS(name, parent_name),

	TP_STRUCT__entry(
		__string(       name,           name            )
		__string(       parent_name,    parent_name     )
	),

	TP_fast_assign(
		__assign_str(name, name);
		__assign_str(parent_name, parent_name);
	),

	TP_printk("%s parent=%s", __get_str(name), __get_str(parent_name))
);

TRACE_EVENT(clock_state,

	TP_PROTO(const char *name, unsigned long prepare_count,
		unsigned long count, unsigned long rate),

	TP_ARGS(name, prepare_count, count, rate),

	TP_STRUCT__entry(
		__string(name,			name)
		__field(unsigned long,		prepare_count)
		__field(unsigned long,		count)
		__field(unsigned long,		rate)
	),

	TP_fast_assign(
		__assign_str(name, name);
		__entry->prepare_count = prepare_count;
		__entry->count = count;
		__entry->rate = rate;
	),

	TP_printk("%s\t[%lu:%lu]\t%lu", __get_str(name), __entry->prepare_count,
					__entry->count, __entry->rate)
);

/*
 * The power domain events are used for power domains transitions
 */
DECLARE_EVENT_CLASS(power_domain,

	TP_PROTO(const char *name, unsigned int state, unsigned int cpu_id),

	TP_ARGS(name, state, cpu_id),

	TP_STRUCT__entry(
		__string(       name,           name            )
		__field(        u64,            state           )
		__field(        u64,            cpu_id          )
	),

	TP_fast_assign(
		__assign_str(name, name);
		__entry->state = state;
		__entry->cpu_id = cpu_id;
),

	TP_printk("%s state=%lu cpu_id=%lu", __get_str(name),
		(unsigned long)__entry->state, (unsigned long)__entry->cpu_id)
);

DEFINE_EVENT(power_domain, power_domain_target,

	TP_PROTO(const char *name, unsigned int state, unsigned int cpu_id),

	TP_ARGS(name, state, cpu_id)
);

DECLARE_EVENT_CLASS(kpm_module,

	TP_PROTO(unsigned int managed_cpus, unsigned int max_cpus),

	TP_ARGS(managed_cpus, max_cpus),

	TP_STRUCT__entry(
		__field(u32, managed_cpus)
		__field(u32, max_cpus)
	),

	TP_fast_assign(
		__entry->managed_cpus = managed_cpus;
		__entry->max_cpus = max_cpus;
	),

	TP_printk("managed:%x max_cpus=%u", (unsigned int)__entry->managed_cpus,
					(unsigned int)__entry->max_cpus)
);

DEFINE_EVENT(kpm_module, set_max_cpus,
	TP_PROTO(unsigned int managed_cpus, unsigned int max_cpus),
	TP_ARGS(managed_cpus, max_cpus)
);

DEFINE_EVENT(kpm_module, reevaluate_hotplug,
	TP_PROTO(unsigned int managed_cpus, unsigned int max_cpus),
	TP_ARGS(managed_cpus, max_cpus)
);

TRACE_EVENT(core_ctl_eval_need,

	TP_PROTO(unsigned int cpu, unsigned int old_need,
		 unsigned int new_need, unsigned int updated),
	TP_ARGS(cpu, old_need, new_need, updated),
	TP_STRUCT__entry(
		__field(u32, cpu)
		__field(u32, old_need)
		__field(u32, new_need)
		__field(u32, updated)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->old_need = old_need;
		__entry->new_need = new_need;
		__entry->updated = updated;
	),
	TP_printk("cpu=%u, old_need=%u, new_need=%u, updated=%u", __entry->cpu,
		  __entry->old_need, __entry->new_need, __entry->updated)
);

TRACE_EVENT(core_ctl_set_busy,

	TP_PROTO(unsigned int cpu, unsigned int busy,
		 unsigned int old_is_busy, unsigned int is_busy),
	TP_ARGS(cpu, busy, old_is_busy, is_busy),
	TP_STRUCT__entry(
		__field(u32, cpu)
		__field(u32, busy)
		__field(u32, old_is_busy)
		__field(u32, is_busy)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->busy = busy;
		__entry->old_is_busy = old_is_busy;
		__entry->is_busy = is_busy;
	),
	TP_printk("cpu=%u, busy=%u, old_is_busy=%u, new_is_busy=%u",
		  __entry->cpu, __entry->busy, __entry->old_is_busy,
		  __entry->is_busy)
);

DECLARE_EVENT_CLASS(kpm_module2,

	TP_PROTO(unsigned int cpu, unsigned int enter_cycle_cnt,
		unsigned int exit_cycle_cnt,
		unsigned int io_busy, u64 iowait),

	TP_ARGS(cpu, enter_cycle_cnt, exit_cycle_cnt, io_busy, iowait),

	TP_STRUCT__entry(
		__field(u32, cpu)
		__field(u32, enter_cycle_cnt)
		__field(u32, exit_cycle_cnt)
		__field(u32, io_busy)
		__field(u64, iowait)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->enter_cycle_cnt = enter_cycle_cnt;
		__entry->exit_cycle_cnt = exit_cycle_cnt;
		__entry->io_busy = io_busy;
		__entry->iowait = iowait;
	),

	TP_printk("CPU:%u enter_cycles=%u exit_cycles=%u io_busy=%u iowait=%lu",
		(unsigned int)__entry->cpu,
		(unsigned int)__entry->enter_cycle_cnt,
		(unsigned int)__entry->exit_cycle_cnt,
		(unsigned int)__entry->io_busy,
		(unsigned long)__entry->iowait)
);

DEFINE_EVENT(kpm_module2, track_iowait,
	TP_PROTO(unsigned int cpu, unsigned int enter_cycle_cnt,
		unsigned int exit_cycle_cnt, unsigned int io_busy, u64 iowait),
	TP_ARGS(cpu, enter_cycle_cnt, exit_cycle_cnt, io_busy, iowait)
);

DECLARE_EVENT_CLASS(cpu_modes,

	TP_PROTO(unsigned int cpu, unsigned int max_load,
		unsigned int single_enter_cycle_cnt,
		unsigned int single_exit_cycle_cnt,
		unsigned int total_load, unsigned int multi_enter_cycle_cnt,
		unsigned int multi_exit_cycle_cnt, unsigned int mode,
		unsigned int cpu_cnt),

	TP_ARGS(cpu, max_load, single_enter_cycle_cnt, single_exit_cycle_cnt,
		total_load, multi_enter_cycle_cnt, multi_exit_cycle_cnt, mode,
		cpu_cnt),

	TP_STRUCT__entry(
		__field(u32, cpu)
		__field(u32, max_load)
		__field(u32, single_enter_cycle_cnt)
		__field(u32, single_exit_cycle_cnt)
		__field(u32, total_load)
		__field(u32, multi_enter_cycle_cnt)
		__field(u32, multi_exit_cycle_cnt)
		__field(u32, mode)
		__field(u32, cpu_cnt)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->max_load = max_load;
		__entry->single_enter_cycle_cnt = single_enter_cycle_cnt;
		__entry->single_exit_cycle_cnt = single_exit_cycle_cnt;
		__entry->total_load = total_load;
		__entry->multi_enter_cycle_cnt = multi_enter_cycle_cnt;
		__entry->multi_exit_cycle_cnt = multi_exit_cycle_cnt;
		__entry->mode = mode;
		__entry->cpu_cnt = cpu_cnt;
	),

	TP_printk("%u:%4u:%4u:%4u:%4u:%4u:%4u:%4u:%u",
		(unsigned int)__entry->cpu, (unsigned int)__entry->max_load,
		(unsigned int)__entry->single_enter_cycle_cnt,
		(unsigned int)__entry->single_exit_cycle_cnt,
		(unsigned int)__entry->total_load,
		(unsigned int)__entry->multi_enter_cycle_cnt,
		(unsigned int)__entry->multi_exit_cycle_cnt,
		(unsigned int)__entry->mode,
		(unsigned int)__entry->cpu_cnt)
);

DEFINE_EVENT(cpu_modes, cpu_mode_detect,
	TP_PROTO(unsigned int cpu, unsigned int max_load,
		unsigned int single_enter_cycle_cnt,
		unsigned int single_exit_cycle_cnt,
		unsigned int total_load, unsigned int multi_enter_cycle_cnt,
		unsigned int multi_exit_cycle_cnt, unsigned int mode,
		unsigned int cpu_cnt),
	TP_ARGS(cpu, max_load, single_enter_cycle_cnt, single_exit_cycle_cnt,
		total_load, multi_enter_cycle_cnt, multi_exit_cycle_cnt,
		mode, cpu_cnt)
);

DECLARE_EVENT_CLASS(timer_status,
	TP_PROTO(unsigned int cpu, unsigned int single_enter_cycles,
		unsigned int single_enter_cycle_cnt,
		unsigned int single_exit_cycles,
		unsigned int single_exit_cycle_cnt,
		unsigned int multi_enter_cycles,
		unsigned int multi_enter_cycle_cnt,
		unsigned int multi_exit_cycles,
		unsigned int multi_exit_cycle_cnt, unsigned int timer_rate,
		unsigned int mode),
	TP_ARGS(cpu, single_enter_cycles, single_enter_cycle_cnt,
		single_exit_cycles, single_exit_cycle_cnt, multi_enter_cycles,
		multi_enter_cycle_cnt, multi_exit_cycles,
		multi_exit_cycle_cnt, timer_rate, mode),

	TP_STRUCT__entry(
		__field(unsigned int, cpu)
		__field(unsigned int, single_enter_cycles)
		__field(unsigned int, single_enter_cycle_cnt)
		__field(unsigned int, single_exit_cycles)
		__field(unsigned int, single_exit_cycle_cnt)
		__field(unsigned int, multi_enter_cycles)
		__field(unsigned int, multi_enter_cycle_cnt)
		__field(unsigned int, multi_exit_cycles)
		__field(unsigned int, multi_exit_cycle_cnt)
		__field(unsigned int, timer_rate)
		__field(unsigned int, mode)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->single_enter_cycles = single_enter_cycles;
		__entry->single_enter_cycle_cnt = single_enter_cycle_cnt;
		__entry->single_exit_cycles = single_exit_cycles;
		__entry->single_exit_cycle_cnt = single_exit_cycle_cnt;
		__entry->multi_enter_cycles = multi_enter_cycles;
		__entry->multi_enter_cycle_cnt = multi_enter_cycle_cnt;
		__entry->multi_exit_cycles = multi_exit_cycles;
		__entry->multi_exit_cycle_cnt = multi_exit_cycle_cnt;
		__entry->timer_rate = timer_rate;
		__entry->mode = mode;
	),

	TP_printk("%u:%4u:%4u:%4u:%4u:%4u:%4u:%4u:%4u:%4u:%4u",
		(unsigned int) __entry->cpu,
		(unsigned int) __entry->single_enter_cycles,
		(unsigned int) __entry->single_enter_cycle_cnt,
		(unsigned int) __entry->single_exit_cycles,
		(unsigned int) __entry->single_exit_cycle_cnt,
		(unsigned int) __entry->multi_enter_cycles,
		(unsigned int) __entry->multi_enter_cycle_cnt,
		(unsigned int) __entry->multi_exit_cycles,
		(unsigned int) __entry->multi_exit_cycle_cnt,
		(unsigned int) __entry->timer_rate,
		(unsigned int) __entry->mode)
);

DEFINE_EVENT(timer_status, single_mode_timeout,
	TP_PROTO(unsigned int cpu, unsigned int single_enter_cycles,
		unsigned int single_enter_cycle_cnt,
		unsigned int single_exit_cycles,
		unsigned int single_exit_cycle_cnt,
		unsigned int multi_enter_cycles,
		unsigned int multi_enter_cycle_cnt,
		unsigned int multi_exit_cycles,
		unsigned int multi_exit_cycle_cnt, unsigned int timer_rate,
		unsigned int mode),
	TP_ARGS(cpu, single_enter_cycles, single_enter_cycle_cnt,
		single_exit_cycles, single_exit_cycle_cnt, multi_enter_cycles,
		multi_enter_cycle_cnt, multi_exit_cycles, multi_exit_cycle_cnt,
		timer_rate, mode)
);

DEFINE_EVENT(timer_status, single_cycle_exit_timer_start,
	TP_PROTO(unsigned int cpu, unsigned int single_enter_cycles,
		unsigned int single_enter_cycle_cnt,
		unsigned int single_exit_cycles,
		unsigned int single_exit_cycle_cnt,
		unsigned int multi_enter_cycles,
		unsigned int multi_enter_cycle_cnt,
		unsigned int multi_exit_cycles,
		unsigned int multi_exit_cycle_cnt, unsigned int timer_rate,
		unsigned int mode),
	TP_ARGS(cpu, single_enter_cycles, single_enter_cycle_cnt,
		single_exit_cycles, single_exit_cycle_cnt, multi_enter_cycles,
		multi_enter_cycle_cnt, multi_exit_cycles, multi_exit_cycle_cnt,
		timer_rate, mode)
);

DEFINE_EVENT(timer_status, single_cycle_exit_timer_stop,
	TP_PROTO(unsigned int cpu, unsigned int single_enter_cycles,
		unsigned int single_enter_cycle_cnt,
		unsigned int single_exit_cycles,
		unsigned int single_exit_cycle_cnt,
		unsigned int multi_enter_cycles,
		unsigned int multi_enter_cycle_cnt,
		unsigned int multi_exit_cycles,
		unsigned int multi_exit_cycle_cnt, unsigned int timer_rate,
		unsigned int mode),
	TP_ARGS(cpu, single_enter_cycles, single_enter_cycle_cnt,
		single_exit_cycles, single_exit_cycle_cnt, multi_enter_cycles,
		multi_enter_cycle_cnt, multi_exit_cycles, multi_exit_cycle_cnt,
		timer_rate, mode)
);

#endif /* _TRACE_POWER_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
