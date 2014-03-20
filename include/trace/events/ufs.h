/*
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM ufs

#if !defined(_TRACE_UFS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_UFS_H

#include <linux/tracepoint.h>

TRACE_EVENT(ufshcd_clk_gating,

	TP_PROTO(const char *dev_name, const char *state),

	TP_ARGS(dev_name, state),

	TP_STRUCT__entry(
		__string(dev_name, dev_name)
		__string(state, state)
	),

	TP_fast_assign(
		__assign_str(dev_name, dev_name);
		__assign_str(state, state);
	),

	TP_printk("%s: gating state changed to %s",
		__get_str(dev_name), __get_str(state))
);

TRACE_EVENT(ufshcd_clk_scaling,

	TP_PROTO(const char *dev_name, const char *state, const char *clk,
		u32 prev_state, u32 curr_state),

	TP_ARGS(dev_name, state, clk, prev_state, curr_state),

	TP_STRUCT__entry(
		__string(dev_name, dev_name)
		__string(state, state)
		__string(clk, clk)
		__field(u32, prev_state)
		__field(u32, curr_state)
	),

	TP_fast_assign(
		__assign_str(dev_name, dev_name);
		__assign_str(state, state);
		__assign_str(clk, clk);
		__entry->prev_state = prev_state;
		__entry->curr_state = curr_state;
	),

	TP_printk("%s: %s %s from %u to %u Hz",
		__get_str(dev_name), __get_str(state), __get_str(clk),
		__entry->prev_state, __entry->curr_state)
);

TRACE_EVENT(ufshcd_auto_bkops_state,

	TP_PROTO(const char *dev_name, const char *state),

	TP_ARGS(dev_name, state),

	TP_STRUCT__entry(
		__string(dev_name, dev_name)
		__string(state, state)
	),

	TP_fast_assign(
		__assign_str(dev_name, dev_name);
		__assign_str(state, state);
	),

	TP_printk("%s: auto bkops - %s",
		__get_str(dev_name), __get_str(state))
);

DECLARE_EVENT_CLASS(ufshcd_template,
	TP_PROTO(const char *dev_name, int err, s64 usecs,
		 const char *dev_state, const char *link_state),

	TP_ARGS(dev_name, err, usecs, dev_state, link_state),

	TP_STRUCT__entry(
		__field(s64, usecs)
		__field(int, err)
		__string(dev_name, dev_name)
		__string(dev_state, dev_state)
		__string(link_state, link_state)
	),

	TP_fast_assign(
		__entry->usecs = usecs;
		__entry->err = err;
		__assign_str(dev_name, dev_name);
		__assign_str(dev_state, dev_state);
		__assign_str(link_state, link_state);
	),

	TP_printk(
		"%s: took %lld usecs, dev_state: %s, link_state: %s, err %d",
		__get_str(dev_name),
		__entry->usecs,
		__get_str(dev_state),
		__get_str(link_state),
		__entry->err
	)
);

DEFINE_EVENT(ufshcd_template, ufshcd_system_suspend,
	     TP_PROTO(const char *dev_name, int err, s64 usecs,
		      const char *dev_state, const char *link_state),
	     TP_ARGS(dev_name, err, usecs, dev_state, link_state));

DEFINE_EVENT(ufshcd_template, ufshcd_system_resume,
	     TP_PROTO(const char *dev_name, int err, s64 usecs,
		      const char *dev_state, const char *link_state),
	     TP_ARGS(dev_name, err, usecs, dev_state, link_state));

DEFINE_EVENT(ufshcd_template, ufshcd_runtime_suspend,
	     TP_PROTO(const char *dev_name, int err, s64 usecs,
		      const char *dev_state, const char *link_state),
	     TP_ARGS(dev_name, err, usecs, dev_state, link_state));

DEFINE_EVENT(ufshcd_template, ufshcd_runtime_resume,
	     TP_PROTO(const char *dev_name, int err, s64 usecs,
		      const char *dev_state, const char *link_state),
	     TP_ARGS(dev_name, err, usecs, dev_state, link_state));

DEFINE_EVENT(ufshcd_template, ufshcd_init,
	     TP_PROTO(const char *dev_name, int err, s64 usecs,
		      const char *dev_state, const char *link_state),
	     TP_ARGS(dev_name, err, usecs, dev_state, link_state));
#endif /* if !defined(_TRACE_UFS_H) || defined(TRACE_HEADER_MULTI_READ) */

/* This part must be outside protection */
#include <trace/define_trace.h>
