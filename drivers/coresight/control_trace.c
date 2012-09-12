/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

/*
 * DLKM to register a callback with a ftrace event
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/tracepoint.h>
#include <linux/coresight.h>

#include <trace/events/exception.h>

static void abort_coresight_tracing(void *ignore, struct task_struct *task,\
					unsigned long addr, unsigned int fsr)
{
	coresight_abort();
	pr_debug("control_trace: task_name: %s, addr: %lu, fsr:%u",\
		(char *)task->comm, addr, fsr);
}

static void abort_tracing_undef_instr(void *ignore, struct pt_regs *regs,\
					void *pc)
{
	if (user_mode(regs)) {
		coresight_abort();
		pr_debug("control_trace: pc: %p", pc);
	}
}

static int __init control_trace_init(void)
{
	int ret_user_fault, ret_undef_instr;
	ret_user_fault = register_trace_user_fault(abort_coresight_tracing,\
							NULL);
	ret_undef_instr = register_trace_undef_instr(abort_tracing_undef_instr,\
							NULL);
	if (ret_user_fault != 0 || ret_undef_instr != 0) {
		pr_info("control_trace: Module Not Registered\n");
		return (ret_user_fault < 0 ?\
			ret_user_fault : ret_undef_instr);
	}
	pr_info("control_trace: Module Registered\n");
	return 0;
}

module_init(control_trace_init);

static void __exit control_trace_exit(void)
{
	unregister_trace_user_fault(abort_coresight_tracing, NULL);
	unregister_trace_undef_instr(abort_tracing_undef_instr, NULL);
	pr_info("control_trace: Module Removed\n");
}

module_exit(control_trace_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Kernel Module to abort tracing");
