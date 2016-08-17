/*
 * Copyright (C) 2010 Google, Inc.
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

#include <linux/kernel.h>
#include <linux/percpu.h>
#include <linux/slab.h>
#include <linux/syscore_ops.h>
#include <linux/cpu_pm.h>
#include <asm/fiq.h>
#include <asm/fiq_glue.h>

extern unsigned char fiq_glue, fiq_glue_end;
extern void fiq_glue_setup(void *func, void *data, void *sp);

static struct fiq_handler fiq_debbuger_fiq_handler = {
	.name = "fiq_glue",
};
DEFINE_PER_CPU(void *, fiq_stack);
static struct fiq_glue_handler *current_handler;
static DEFINE_MUTEX(fiq_glue_lock);

static void fiq_glue_setup_helper(void *info)
{
	struct fiq_glue_handler *handler = info;
	fiq_glue_setup(handler->fiq, handler,
		__get_cpu_var(fiq_stack) + THREAD_START_SP);
}

int fiq_glue_register_handler(struct fiq_glue_handler *handler)
{
	int ret;
	int cpu;

	if (!handler || !handler->fiq)
		return -EINVAL;

	mutex_lock(&fiq_glue_lock);
	if (fiq_stack) {
		ret = -EBUSY;
		goto err_busy;
	}

	for_each_possible_cpu(cpu) {
		void *stack;
		stack = (void *)__get_free_pages(GFP_KERNEL, THREAD_SIZE_ORDER);
		if (WARN_ON(!stack)) {
			ret = -ENOMEM;
			goto err_alloc_fiq_stack;
		}
		per_cpu(fiq_stack, cpu) = stack;
	}

	ret = claim_fiq(&fiq_debbuger_fiq_handler);
	if (WARN_ON(ret))
		goto err_claim_fiq;

	current_handler = handler;
	on_each_cpu(fiq_glue_setup_helper, handler, true);
	set_fiq_handler(&fiq_glue, &fiq_glue_end - &fiq_glue);

	mutex_unlock(&fiq_glue_lock);
	return 0;

err_claim_fiq:
err_alloc_fiq_stack:
	for_each_possible_cpu(cpu) {
		__free_pages(per_cpu(fiq_stack, cpu), THREAD_SIZE_ORDER);
		per_cpu(fiq_stack, cpu) = NULL;
	}
err_busy:
	mutex_unlock(&fiq_glue_lock);
	return ret;
}

/**
 * fiq_glue_resume - Restore fiqs after suspend or low power idle states
 *
 * This must be called before calling local_fiq_enable after returning from a
 * power state where the fiq mode registers were lost. If a driver provided
 * a resume hook when it registered the handler it will be called.
 */

void fiq_glue_resume(void)
{
	if (!current_handler)
		return;
	fiq_glue_setup(current_handler->fiq, current_handler,
		__get_cpu_var(fiq_stack) + THREAD_START_SP);
	if (current_handler->resume)
		current_handler->resume(current_handler);
}

static int fiq_glue_cpu_pm_notify(struct notifier_block *self, unsigned long cmd,
	void *v)
{
	switch (cmd) {
	case CPU_PM_ENTER:
		//pr_info("cpu pm enter %d\n", smp_processor_id());
		local_fiq_disable();
		break;
	case CPU_PM_ENTER_FAILED:
	case CPU_PM_EXIT:
		fiq_glue_resume();
		local_fiq_enable();
		//pr_info("cpu pm exit %d\n", smp_processor_id());
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block fiq_glue_cpu_pm_notifier = {
	.notifier_call = fiq_glue_cpu_pm_notify,
};

static int __init fiq_glue_cpu_pm_init(void)
{
	return cpu_pm_register_notifier(&fiq_glue_cpu_pm_notifier);
}
core_initcall(fiq_glue_cpu_pm_init);

#ifdef CONFIG_PM
static int fiq_glue_syscore_suspend(void)
{
	local_fiq_disable();
	return 0;
}

static void fiq_glue_syscore_resume(void)
{
	fiq_glue_resume();
	local_fiq_enable();
}

static struct syscore_ops fiq_glue_syscore_ops = {
	.suspend = fiq_glue_syscore_suspend,
	.resume = fiq_glue_syscore_resume,
};

static int __init fiq_glue_syscore_init(void)
{
	register_syscore_ops(&fiq_glue_syscore_ops);
	return 0;
}
late_initcall(fiq_glue_syscore_init);
#endif
