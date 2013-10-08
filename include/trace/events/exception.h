/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#define TRACE_SYSTEM exception

#if !defined(_TRACE_EXCEPTION_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EXCEPTION_H

#include <linux/tracepoint.h>

struct task_struct;

TRACE_EVENT(user_fault,

	TP_PROTO(struct task_struct *tsk, unsigned long addr, unsigned int fsr),

	TP_ARGS(tsk, addr, fsr),

	TP_STRUCT__entry(
		__string(task_name, tsk->comm)
		__field(unsigned long, addr)
		__field(unsigned int, fsr)
	),

	TP_fast_assign(
	__assign_str(task_name, tsk->comm)
		__entry->addr	= addr;
		__entry->fsr	= fsr;
	),

	TP_printk("task_name:%s addr:%lu, fsr:%u", __get_str(task_name),\
		__entry->addr, __entry->fsr)
);

struct pt_regs;

TRACE_EVENT(undef_instr,

	TP_PROTO(struct pt_regs *regs, void *prog_cnt),

	TP_ARGS(regs, prog_cnt),

	TP_STRUCT__entry(
		__field(void *, prog_cnt)
		__field(struct pt_regs *, regs)
	),

	TP_fast_assign(
		__entry->regs		= regs;
		__entry->prog_cnt	= prog_cnt;
	),

	TP_printk("pc:%p", __entry->prog_cnt)
);

TRACE_EVENT(unhandled_abort,

	TP_PROTO(struct pt_regs *regs, unsigned long addr, unsigned int fsr),

	TP_ARGS(regs, addr, fsr),

	TP_STRUCT__entry(
		__field(struct pt_regs *, regs)
		__field(unsigned long, addr)
		__field(unsigned int, fsr)
	),

	TP_fast_assign(
		__entry->regs	= regs;
		__entry->addr	= addr;
		__entry->fsr	= fsr;
	),

	TP_printk("addr:%lu, fsr:%u", __entry->addr, __entry->fsr)
);

#endif

#include <trace/define_trace.h>
