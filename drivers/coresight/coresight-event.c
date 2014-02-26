/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

static int event_abort_enable;
static int event_abort_set(const char *val, struct kernel_param *kp);
module_param_call(event_abort_enable, event_abort_set, param_get_int,
		  &event_abort_enable, 0644);

static int event_abort_early_panic = 1;
static int event_abort_on_panic_set(const char *val, struct kernel_param *kp);
module_param_call(event_abort_early_panic, event_abort_on_panic_set,
		  param_get_int, &event_abort_early_panic, 0644);

static void event_abort_user_fault(void *ignore,
				   struct task_struct *task,
				   unsigned long addr,
				   unsigned int fsr)
{
	coresight_abort();
	pr_debug("coresight_event: task_name: %s, addr: %lu, fsr:%u",
		(char *)task->comm, addr, fsr);
}

static void event_abort_undef_instr(void *ignore,
				    struct pt_regs *regs,
				    void *pc)
{
	if (user_mode(regs)) {
		coresight_abort();
		pr_debug("coresight_event: pc: %p", pc);
	}
}

static void event_abort_unhandled_abort(void *ignore,
					struct pt_regs *regs,
					unsigned long addr,
					unsigned int fsr)
{
	if (user_mode(regs)) {
		coresight_abort();
		pr_debug("coresight_event: addr: %lu, fsr:%u", addr, fsr);
	}
}

static void event_abort_kernel_panic(void *ignore, long state)
{
	coresight_abort();
}

static int event_abort_register(void)
{
	int ret;

	ret = register_trace_user_fault(event_abort_user_fault, NULL);
	if (ret)
		goto err_usr_fault;
	ret = register_trace_undef_instr(event_abort_undef_instr, NULL);
	if (ret)
		goto err_undef_instr;
	ret = register_trace_unhandled_abort(event_abort_unhandled_abort, NULL);
	if (ret)
		goto err_unhandled_abort;

	return 0;

err_unhandled_abort:
	unregister_trace_undef_instr(event_abort_undef_instr, NULL);
err_undef_instr:
	unregister_trace_user_fault(event_abort_user_fault, NULL);
err_usr_fault:
	return ret;
}

static void event_abort_unregister(void)
{
	unregister_trace_user_fault(event_abort_user_fault, NULL);
	unregister_trace_undef_instr(event_abort_undef_instr, NULL);
	unregister_trace_unhandled_abort(event_abort_unhandled_abort, NULL);
}

static int event_abort_set(const char *val, struct kernel_param *kp)
{
	int ret;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_err("coresight_event: error setting value %d\n", ret);
		return ret;
	}

	if (event_abort_enable)
		ret = event_abort_register();
	else
		event_abort_unregister();

	return ret;
}

static int event_abort_on_panic_set(const char *val, struct kernel_param *kp)
{
	int ret;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_err("coresight_event: error setting val on panic %d\n", ret);
		return ret;
	}

	if (event_abort_early_panic) {
		unregister_trace_kernel_panic_late(event_abort_kernel_panic,
						   NULL);
		ret = register_trace_kernel_panic(event_abort_kernel_panic,
						  NULL);
		if (ret)
			goto err;
	} else {
		unregister_trace_kernel_panic(event_abort_kernel_panic, NULL);
		ret = register_trace_kernel_panic_late(event_abort_kernel_panic,
						       NULL);
		if (ret)
			goto err;
	}
	return 0;
err:
	pr_err("coresight_event: error registering panic event %d\n", ret);
	return ret;
}

static int __init event_init(void)
{
	int ret;

	ret = register_trace_kernel_panic(event_abort_kernel_panic, NULL);
	if (ret) {
		/* We do not want to fail module init. This module can still
		 * be used to register other abort events.
		 */
		pr_err("coresight_event: error registering on panic %d\n", ret);
	}
	return 0;
}
module_init(event_init);

static void __exit event_exit(void)
{
	unregister_trace_kernel_panic(event_abort_kernel_panic, NULL);
}
module_exit(event_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Coresight Event driver to abort tracing");
