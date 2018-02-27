/*
 * Copyright (c) 2014-2015, 2017, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM clk

#if !defined(_TRACE_CLK_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_CLK_H

#include <linux/tracepoint.h>

struct clk_core;

DECLARE_EVENT_CLASS(clk,

	TP_PROTO(struct clk_core *core),

	TP_ARGS(core),

	TP_STRUCT__entry(
		__string(        name,           core->name       )
	),

	TP_fast_assign(
		__assign_str(name, core->name);
	),

	TP_printk("%s", __get_str(name))
);

DEFINE_EVENT(clk, clk_enable,

	TP_PROTO(struct clk_core *core),

	TP_ARGS(core)
);

DEFINE_EVENT(clk, clk_enable_complete,

	TP_PROTO(struct clk_core *core),

	TP_ARGS(core)
);

DEFINE_EVENT(clk, clk_disable,

	TP_PROTO(struct clk_core *core),

	TP_ARGS(core)
);

DEFINE_EVENT(clk, clk_disable_complete,

	TP_PROTO(struct clk_core *core),

	TP_ARGS(core)
);

DEFINE_EVENT(clk, clk_prepare,

	TP_PROTO(struct clk_core *core),

	TP_ARGS(core)
);

DEFINE_EVENT(clk, clk_prepare_complete,

	TP_PROTO(struct clk_core *core),

	TP_ARGS(core)
);

DEFINE_EVENT(clk, clk_unprepare,

	TP_PROTO(struct clk_core *core),

	TP_ARGS(core)
);

DEFINE_EVENT(clk, clk_unprepare_complete,

	TP_PROTO(struct clk_core *core),

	TP_ARGS(core)
);

DECLARE_EVENT_CLASS(clk_rate,

	TP_PROTO(struct clk_core *core, unsigned long rate),

	TP_ARGS(core, rate),

	TP_STRUCT__entry(
		__string(        name,           core->name                )
		__field(unsigned long,           rate                      )
	),

	TP_fast_assign(
		__assign_str(name, core->name);
		__entry->rate = rate;
	),

	TP_printk("%s %lu", __get_str(name), (unsigned long)__entry->rate)
);

DEFINE_EVENT(clk_rate, clk_set_rate,

	TP_PROTO(struct clk_core *core, unsigned long rate),

	TP_ARGS(core, rate)
);

DEFINE_EVENT(clk_rate, clk_set_rate_complete,

	TP_PROTO(struct clk_core *core, unsigned long rate),

	TP_ARGS(core, rate)
);

DECLARE_EVENT_CLASS(clk_parent,

	TP_PROTO(struct clk_core *core, struct clk_core *parent),

	TP_ARGS(core, parent),

	TP_STRUCT__entry(
		__string(        name,           core->name                )
		__string(        pname, parent ? parent->name : "none"     )
	),

	TP_fast_assign(
		__assign_str(name, core->name);
		__assign_str(pname, parent ? parent->name : "none");
	),

	TP_printk("%s %s", __get_str(name), __get_str(pname))
);

DEFINE_EVENT(clk_parent, clk_set_parent,

	TP_PROTO(struct clk_core *core, struct clk_core *parent),

	TP_ARGS(core, parent)
);

DEFINE_EVENT(clk_parent, clk_set_parent_complete,

	TP_PROTO(struct clk_core *core, struct clk_core *parent),

	TP_ARGS(core, parent)
);

DECLARE_EVENT_CLASS(clk_phase,

	TP_PROTO(struct clk_core *core, int phase),

	TP_ARGS(core, phase),

	TP_STRUCT__entry(
		__string(        name,           core->name                )
		__field(	  int,           phase                     )
	),

	TP_fast_assign(
		__assign_str(name, core->name);
		__entry->phase = phase;
	),

	TP_printk("%s %d", __get_str(name), (int)__entry->phase)
);

DEFINE_EVENT(clk_phase, clk_set_phase,

	TP_PROTO(struct clk_core *core, int phase),

	TP_ARGS(core, phase)
);

DEFINE_EVENT(clk_phase, clk_set_phase_complete,

	TP_PROTO(struct clk_core *core, int phase),

	TP_ARGS(core, phase)
);

DECLARE_EVENT_CLASS(clk_state_dump,

	TP_PROTO(const char *name, unsigned int prepare_count,
	unsigned int enable_count, unsigned long rate, unsigned int vdd_level),

	TP_ARGS(name, prepare_count, enable_count, rate, vdd_level),

	TP_STRUCT__entry(
		__string(name,			name)
		__field(unsigned int,		prepare_count)
		__field(unsigned int,		enable_count)
		__field(unsigned long,		rate)
		__field(unsigned int,		vdd_level)
	),

	TP_fast_assign(
		__assign_str(name, name);
		__entry->prepare_count = prepare_count;
		__entry->enable_count = enable_count;
		__entry->rate = rate;
		__entry->vdd_level = vdd_level;
	),

	TP_printk("%s\tprepare:enable cnt [%u:%u]\trate: vdd_level [%lu:%u]",
		__get_str(name), __entry->prepare_count, __entry->enable_count,
		__entry->rate, __entry->vdd_level)
);

DEFINE_EVENT(clk_state_dump, clk_state,

	TP_PROTO(const char *name, unsigned int prepare_count,
	unsigned int enable_count, unsigned long rate, unsigned int vdd_level),

	TP_ARGS(name, prepare_count, enable_count, rate, vdd_level)
);

#endif /* _TRACE_CLK_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
