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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/tracepoint.h>
#include <linux/coresight.h>

#include <trace/events/exception.h>

static void event_trace_user_fault(void *ignore,
				   struct task_struct *task,
				   unsigned long addr,
				   unsigned int fsr)
{
	coresight_abort();
	pr_debug("coresight_event: task_name: %s, addr: %lu, fsr:%u",
		(char *)task->comm, addr, fsr);
}

static void event_trace_undef_instr(void *ignore,
				    struct pt_regs *regs,
				    void *pc)
{
	if (user_mode(regs)) {
		coresight_abort();
		pr_debug("coresight_event: pc: %p", pc);
	}
}

static int __init event_init(void)
{
	int ret_user_fault, ret_undef_instr;
	ret_user_fault = register_trace_user_fault(
				event_trace_user_fault, NULL);
	ret_undef_instr = register_trace_undef_instr(
				event_trace_undef_instr, NULL);
	if (ret_user_fault != 0 || ret_undef_instr != 0) {
		pr_info("coresight_event: Module Not Registered\n");
		return (ret_user_fault < 0 ?\
			ret_user_fault : ret_undef_instr);
	}
	pr_info("coresight_event: Module Initialized\n");
	return 0;
}
module_init(event_init);

static void __exit event_exit(void)
{
	unregister_trace_user_fault(event_trace_user_fault, NULL);
	unregister_trace_undef_instr(event_trace_undef_instr, NULL);
	pr_info("coresight_event: Module Removed\n");
}
module_exit(event_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Coresight Event driver to abort tracing");
