/*
 * Copyright (C) 2013 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/percpu.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/trusty/smcall.h>
#include <linux/trusty/trusty.h>

#include <asm/fiq_glue.h>

#include "trusty-fiq.h"

extern void trusty_fiq_glue_arm64(void);

static struct device *trusty_dev;
static DEFINE_PER_CPU(void *, fiq_stack);
static struct fiq_glue_handler *fiq_handlers;
static DEFINE_MUTEX(fiq_glue_lock);

void trusty_fiq_handler(struct pt_regs *regs, void *svc_sp)
{
	struct fiq_glue_handler *handler;

	for (handler = ACCESS_ONCE(fiq_handlers); handler;
	     handler = ACCESS_ONCE(handler->next)) {
		/* Barrier paired with smp_wmb in fiq_glue_register_handler */
		smp_read_barrier_depends();
		handler->fiq(handler, regs, svc_sp);
	}
}

static void smp_nop_call(void *info)
{
	/* If this call is reached, the fiq handler is not currently running */
}

static void fiq_glue_clear_handler(void)
{
	int cpu;
	int ret;
	void *stack;

	for_each_possible_cpu(cpu) {
		stack = per_cpu(fiq_stack, cpu);
		if (!stack)
			continue;

		ret = trusty_fast_call64(trusty_dev, SMC_FC64_SET_FIQ_HANDLER,
					 cpu, 0, 0);
		if (ret) {
			pr_err("%s: SMC_FC_SET_FIQ_HANDLER(%d, 0, 0) failed 0x%x, skip free stack\n",
			       __func__, cpu, ret);
			continue;
		}

		per_cpu(fiq_stack, cpu) = NULL;
		smp_call_function_single(cpu, smp_nop_call, NULL, true);
		free_pages((unsigned long)stack, THREAD_SIZE_ORDER);
	}
}

static int fiq_glue_set_handler(void)
{
	int ret;
	int cpu;
	void *stack;
	unsigned long irqflags;

	for_each_possible_cpu(cpu) {
		stack = (void *)__get_free_pages(GFP_KERNEL, THREAD_SIZE_ORDER);
		if (WARN_ON(!stack)) {
			ret = -ENOMEM;
			goto err_alloc_fiq_stack;
		}
		per_cpu(fiq_stack, cpu) = stack;
		stack += THREAD_START_SP;

		local_irq_save(irqflags);
		ret = trusty_fast_call64(trusty_dev, SMC_FC64_SET_FIQ_HANDLER,
					 cpu, (uintptr_t)trusty_fiq_glue_arm64,
					 (uintptr_t)stack);
		local_irq_restore(irqflags);
		if (ret) {
			pr_err("%s: SMC_FC_SET_FIQ_HANDLER(%d, %p, %p) failed 0x%x\n",
			       __func__, cpu, trusty_fiq_glue_arm64,
			       stack, ret);
			ret = -EINVAL;
			goto err_set_fiq_handler;
		}
	}
	return 0;

err_alloc_fiq_stack:
err_set_fiq_handler:
	fiq_glue_clear_handler();
	return ret;
}

int fiq_glue_register_handler(struct fiq_glue_handler *handler)
{
	int ret;

	if (!handler || !handler->fiq) {
		ret = -EINVAL;
		goto err_bad_arg;
	}

	mutex_lock(&fiq_glue_lock);

	if (!trusty_dev) {
		ret = -ENODEV;
		goto err_no_trusty;
	}

	handler->next = fiq_handlers;
	/*
	 * Write barrier paired with smp_read_barrier_depends in
	 * trusty_fiq_handler. Make sure next pointer is updated before
	 * fiq_handlers so trusty_fiq_handler does not see an uninitialized
	 * value and terminate early or crash.
	 */
	smp_wmb();
	fiq_handlers = handler;

	smp_call_function(smp_nop_call, NULL, true);

	if (!handler->next) {
		ret = fiq_glue_set_handler();
		if (ret)
			goto err_set_fiq_handler;
	}

	mutex_unlock(&fiq_glue_lock);
	return 0;

err_set_fiq_handler:
	fiq_handlers = handler->next;
err_no_trusty:
	mutex_unlock(&fiq_glue_lock);
err_bad_arg:
	pr_err("%s: failed, %d\n", __func__, ret);
	return ret;
}

int trusty_fiq_arch_probe(struct platform_device *pdev)
{
	mutex_lock(&fiq_glue_lock);
	trusty_dev = pdev->dev.parent;
	mutex_unlock(&fiq_glue_lock);

	return 0;
}

void trusty_fiq_arch_remove(struct platform_device *pdev)
{
	mutex_lock(&fiq_glue_lock);
	fiq_glue_clear_handler();
	trusty_dev = NULL;
	mutex_unlock(&fiq_glue_lock);
}
