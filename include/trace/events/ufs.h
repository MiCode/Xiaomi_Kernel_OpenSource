/*
 * Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
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

DECLARE_EVENT_CLASS(ufshcd_state_change_template,
	TP_PROTO(const char *dev_name, int state),

	TP_ARGS(dev_name, state),

	TP_STRUCT__entry(
		__string(dev_name, dev_name)
		__field(int, state)
	),

	TP_fast_assign(
		__assign_str(dev_name, dev_name);
		__entry->state = state;
	),

	TP_printk("%s: state changed to %s",
		__get_str(dev_name), __entry->state ? "ON" : "OFF")
);

DEFINE_EVENT_PRINT(ufshcd_state_change_template, ufshcd_clk_gating,
	TP_PROTO(const char *dev_name, int state),
	TP_ARGS(dev_name, state),
	TP_printk("%s: state changed to %s", __get_str(dev_name),
		__print_symbolic(__entry->state,
				{ CLKS_OFF, "CLKS_OFF" },
				{ CLKS_ON, "CLKS_ON" },
				{ REQ_CLKS_OFF, "REQ_CLKS_OFF" },
				{ REQ_CLKS_ON, "REQ_CLKS_ON" }))
);

DEFINE_EVENT_PRINT(ufshcd_state_change_template, ufshcd_hibern8_on_idle,
	TP_PROTO(const char *dev_name, int state),
	TP_ARGS(dev_name, state),
	TP_printk("%s: state changed to %s", __get_str(dev_name),
		__print_symbolic(__entry->state,
			{ HIBERN8_ENTERED, "HIBERN8_ENTER" },
			{ HIBERN8_EXITED, "HIBERN8_EXIT" },
			{ REQ_HIBERN8_ENTER, "REQ_HIBERN8_ENTER" },
			{ REQ_HIBERN8_EXIT, "REQ_HIBERN8_EXIT" }))
);

DEFINE_EVENT(ufshcd_state_change_template, ufshcd_auto_bkops_state,
	TP_PROTO(const char *dev_name, int state),
	TP_ARGS(dev_name, state));

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

DECLARE_EVENT_CLASS(ufshcd_profiling_template,
	TP_PROTO(const char *dev_name, const char *profile_info, s64 time_us,
		 int err),

	TP_ARGS(dev_name, profile_info, time_us, err),

	TP_STRUCT__entry(
		__string(dev_name, dev_name)
		__string(profile_info, profile_info)
		__field(s64, time_us)
		__field(int, err)
	),

	TP_fast_assign(
		__assign_str(dev_name, dev_name);
		__assign_str(profile_info, profile_info);
		__entry->time_us = time_us;
		__entry->err = err;
	),

	TP_printk("%s: %s: took %lld usecs, err %d",
		__get_str(dev_name), __get_str(profile_info),
		__entry->time_us, __entry->err)
);

DEFINE_EVENT(ufshcd_profiling_template, ufshcd_profile_hibern8,
	TP_PROTO(const char *dev_name, const char *profile_info, s64 time_us,
		 int err),
	TP_ARGS(dev_name, profile_info, time_us, err));

DEFINE_EVENT(ufshcd_profiling_template, ufshcd_profile_clk_gating,
	TP_PROTO(const char *dev_name, const char *profile_info, s64 time_us,
		 int err),
	TP_ARGS(dev_name, profile_info, time_us, err));

DEFINE_EVENT(ufshcd_profiling_template, ufshcd_profile_clk_scaling,
	TP_PROTO(const char *dev_name, const char *profile_info, s64 time_us,
		 int err),
	TP_ARGS(dev_name, profile_info, time_us, err));

DECLARE_EVENT_CLASS(ufshcd_template,
	TP_PROTO(const char *dev_name, int err, s64 usecs,
		 int dev_state, int link_state),

	TP_ARGS(dev_name, err, usecs, dev_state, link_state),

	TP_STRUCT__entry(
		__field(s64, usecs)
		__field(int, err)
		__string(dev_name, dev_name)
		__field(int, dev_state)
		__field(int, link_state)
	),

	TP_fast_assign(
		__entry->usecs = usecs;
		__entry->err = err;
		__assign_str(dev_name, dev_name);
		__entry->dev_state = dev_state;
		__entry->link_state = link_state;
	),

	TP_printk(
		"%s: took %lld usecs, dev_state: %s, link_state: %s, err %d",
		__get_str(dev_name),
		__entry->usecs,
		__print_symbolic(__entry->dev_state,
			{ UFS_ACTIVE_PWR_MODE, "ACTIVE" },
			{ UFS_SLEEP_PWR_MODE, "SLEEP" },
			{ UFS_POWERDOWN_PWR_MODE, "POWERDOWN" }),
		__print_symbolic(__entry->link_state,
			{ UIC_LINK_OFF_STATE, "LINK_OFF" },
			{ UIC_LINK_ACTIVE_STATE, "LINK_ACTIVE" },
			{ UIC_LINK_HIBERN8_STATE, "LINK_HIBERN8" }),
		__entry->err
	)
);

DEFINE_EVENT(ufshcd_template, ufshcd_system_suspend,
	TP_PROTO(const char *dev_name, int err, s64 usecs,
		int dev_state, int link_state),
	TP_ARGS(dev_name, err, usecs, dev_state, link_state));

DEFINE_EVENT(ufshcd_template, ufshcd_system_resume,
	TP_PROTO(const char *dev_name, int err, s64 usecs,
		int dev_state, int link_state),
	TP_ARGS(dev_name, err, usecs, dev_state, link_state));

DEFINE_EVENT(ufshcd_template, ufshcd_runtime_suspend,
	TP_PROTO(const char *dev_name, int err, s64 usecs,
		int dev_state, int link_state),
	TP_ARGS(dev_name, err, usecs, dev_state, link_state));

DEFINE_EVENT(ufshcd_template, ufshcd_runtime_resume,
	TP_PROTO(const char *dev_name, int err, s64 usecs,
		int dev_state, int link_state),
	TP_ARGS(dev_name, err, usecs, dev_state, link_state));

DEFINE_EVENT(ufshcd_template, ufshcd_init,
	TP_PROTO(const char *dev_name, int err, s64 usecs,
		int dev_state, int link_state),
	TP_ARGS(dev_name, err, usecs, dev_state, link_state));

TRACE_EVENT(ufshcd_command,
	TP_PROTO(const char *dev_name, const char *str, unsigned int tag,
			u32 doorbell, int transfer_len, u32 intr, u64 lba,
			u8 opcode),

	TP_ARGS(dev_name, str, tag, doorbell, transfer_len, intr, lba, opcode),

	TP_STRUCT__entry(
		__string(dev_name, dev_name)
		__string(str, str)
		__field(unsigned int, tag)
		__field(u32, doorbell)
		__field(int, transfer_len)
		__field(u32, intr)
		__field(u64, lba)
		__field(u8, opcode)
	),

	TP_fast_assign(
		__assign_str(dev_name, dev_name);
		__assign_str(str, str);
		__entry->tag = tag;
		__entry->doorbell = doorbell;
		__entry->transfer_len = transfer_len;
		__entry->intr = intr;
		__entry->lba = lba;
		__entry->opcode = opcode;
	),

	TP_printk(
		"%s: %s: tag: %u, DB: 0x%x, size: %d, IS: %u, LBA: %llu, opcode: 0x%x",
		__get_str(str), __get_str(dev_name), __entry->tag,
		__entry->doorbell, __entry->transfer_len,
		__entry->intr, __entry->lba, (u32)__entry->opcode
	)
);

#endif /* if !defined(_TRACE_UFS_H) || defined(TRACE_HEADER_MULTI_READ) */

/* This part must be outside protection */
#include <trace/define_trace.h>
