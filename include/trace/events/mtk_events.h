#undef TRACE_SYSTEM
#define TRACE_SYSTEM mtk_events

#if !defined(_TRACE_MTK_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MTK_EVENTS_H

#include <linux/tracepoint.h>

TRACE_EVENT(tracing_on,

	TP_PROTO(int on, unsigned long ip),

	TP_ARGS(on, ip),

	TP_STRUCT__entry(
		__field(int,	on)
        __field(unsigned long, ip)
	),

	TP_fast_assign(
		__entry->on = on;
        __entry->ip = ip;
	),

	TP_printk("ftrace is %s caller=%pf",
		__entry->on ? "enabled" : "disabled",
        (void *)__entry->ip)
);

TRACE_EVENT(cpu_hotplug,

	TP_PROTO(unsigned int cpu_id, unsigned int state, unsigned long long ts),

	TP_ARGS(cpu_id, state, ts),

	TP_STRUCT__entry(
		__field(u32,	cpu_id)
		__field(u32,	state)
		__field(u64,	ts)
	),

	TP_fast_assign(
		__entry->cpu_id = cpu_id;
		__entry->state = state;
		__entry->ts = ts;
	),

	TP_printk("cpu=%lu state=%s last_%s_ts=%llu",
		(unsigned long)__entry->cpu_id,
		__entry->state ? "online" : "offline",
		__entry->state ? "offline" : "online",
		__entry->ts)
);

TRACE_EVENT(irq_entry,

	TP_PROTO(int irqnr, const char *irqname),

	TP_ARGS(irqnr, irqname),

	TP_STRUCT__entry(
		__field(int,	irq)
		__string(name,	irqname)
	),

	TP_fast_assign(
			__entry->irq = irqnr;
			__assign_str(name, irqname);
	),

	TP_printk("irq=%d name=%s", __entry->irq, __get_str(name))
);

TRACE_EVENT(irq_exit,

	TP_PROTO(int irqnr),

	TP_ARGS(irqnr),

	TP_STRUCT__entry(
		__field(int,	irq)
	),

	TP_fast_assign(
			__entry->irq = irqnr;
	),

	TP_printk("irq=%d", __entry->irq)
);

TRACE_EVENT(gpu_freq,

    TP_PROTO(unsigned int frequency),

    TP_ARGS(frequency),

    TP_STRUCT__entry(
        __field(u32, frequency)
    ),

    TP_fast_assign(
        __entry->frequency = frequency;
    ),

    TP_printk("frequency=%lu", (unsigned long)__entry->frequency)
);

#endif /* _TRACE_MTK_EVENTS_H */
/* This part must be outside protection */
#include <trace/define_trace.h>
