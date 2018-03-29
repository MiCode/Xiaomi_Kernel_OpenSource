/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/percpu.h>
#include <linux/smp.h>
#include <asm/fiq_smp_call.h>
#ifdef CONFIG_MTK_GIC_EXT
#include <linux/irqchip/mtk-gic-extend.h>
#else
#include <mach/irqs.h>
#endif

#if defined(CONFIG_FIQ_GLUE)

#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT)
#include <mach/mt_secure_api.h>
#endif

enum {
	CSD_FLAG_LOCK = 0x01,
};

struct fiq_call_single_data {
	struct list_head list;
	smp_call_func_t func;
	void *info;
	cpumask_var_t cpumask;
	u16 flags;
	u16 priv;
};

struct call_function_data {
	struct fiq_call_single_data csd;
	fiq_smp_call_func_t func;
	atomic_t refs;
	cpumask_var_t cpumask;
};

static DEFINE_PER_CPU_SHARED_ALIGNED(struct call_function_data, fiq_cfd_data);
static struct call_function_data *current_cfd_data;

static int __csd_lock_wait(struct fiq_call_single_data *data)
{
	int cpu, nr_online_cpus = 0;

	while (data->flags & CSD_FLAG_LOCK) {
		for_each_cpu(cpu, data->cpumask) {
			if (cpu_online(cpu))
				nr_online_cpus++;
		}
		if (!nr_online_cpus)
			return -ENXIO;
		cpu_relax();
	}

	return 0;
}

static void __csd_lock(struct fiq_call_single_data *data)
{
	__csd_lock_wait(data);
	data->flags = CSD_FLAG_LOCK;

	/*
	 * prevent CPU from reordering the above assignment
	 * to ->flags with any subsequent assignments to other
	 * fields of the specified fiq_call_single_data structure:
	 */
	smp_mb();
}

static void __csd_unlock(struct fiq_call_single_data *data)
{
	WARN_ON(!(data->flags & CSD_FLAG_LOCK));

	/*
	 * ensure we're all done before releasing data:
	 */
	smp_mb();

	data->flags &= ~CSD_FLAG_LOCK;
}

#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT)
static void fiq_security_fastcall(const struct cpumask *mask)
{
	unsigned long map = *cpus_addr(*mask);

	mt_secure_call(MC_FC_MTK_AEEDUMP, map, 0, 0);
}
#endif

static int get_HW_cpuid(void)
{
	int id;

	asm ("mrc p15, 0, %0, c0, c0, 5 @ Get CPUID\n" : "=r"(id));
	return (id&0x3)+((id&0xF00)>>6);
}

/*
 * fiq_smp_call_function: FIQ version of smp_call_function.
 * @func:
 * @info:
 * @wait:
 * Return 0 for success and error code for failure.
 *
 * This function is designed for the debugger only.
 * Other kernel code or drivers should NOT use this function.
 * This function can only be used in the FIQ-WDT handler.
 */
int fiq_smp_call_function(fiq_smp_call_func_t func, void *info, int wait)
{
	struct cpumask *mask = (struct cpumask *)cpu_online_mask;
	struct call_function_data *data;
	int refs, install_csd, this_cpu = 0;

	this_cpu = get_HW_cpuid();
	data = &__get_cpu_var(fiq_cfd_data);
	__csd_lock(&data->csd);

	atomic_set(&data->refs, 0);

	data->func = func;
	data->csd.info = info;

	/* make sure data prepared before next step */
	smp_wmb();

	cpumask_and(data->cpumask, mask, cpu_online_mask);
	cpumask_clear_cpu(this_cpu, data->cpumask);
	refs = cpumask_weight(data->cpumask);
	cpumask_and(data->csd.cpumask, data->cpumask, data->cpumask);

	if (unlikely(!refs)) {
		__csd_unlock(&data->csd);
		goto fiq_smp_call_function_exit;
	}

	/* poll to install data on current_cfd_data */
	install_csd = 0;
	do {
#if 0				/* no need to protect due to FIQ-WDT */
		spin_lock(&fiq_smp_call_lock);
#endif

		if (!current_cfd_data) {
			atomic_set(&data->refs, refs);
			current_cfd_data = data;
			install_csd = 1;
		}
#if 0
		spin_unlock(&fiq_smp_call_lock);
#endif
	} while (!install_csd);

	/* make sure data prepared before sending SGI */
	smp_mb();

	/* send a message to all CPUs in the map */
#if !defined(CONFIG_TRUSTONIC_TEE_SUPPORT)
	irq_raise_softirq(data->cpumask, FIQ_SMP_CALL_SGI);
#else
	fiq_security_fastcall(data->cpumask);
#endif

	if (wait)
		__csd_lock_wait(&data->csd);

fiq_smp_call_function_exit:
	return 0;
}

static void fiq_smp_call_handler(void *arg, void *regs, void *svc_sp)
{
	struct call_function_data *data;
	int cpu = 0, refs;
	fiq_smp_call_func_t func;

	/* get the current cpu id */
	asm volatile ("MRC p15, 0, %0, c0, c0, 5\n" "AND %0, %0, #0xf\n"
		: "+r"(cpu) :  : "cc");

	data = current_cfd_data;
	if (data) {
		func = data->func;
		func(data->csd.info, regs, svc_sp);

		cpumask_clear_cpu(cpu, data->csd.cpumask);
		refs = atomic_dec_return(&data->refs);

		if (refs == 0) {
			__csd_unlock(&data->csd);
			current_cfd_data = NULL;
		}
	}
}

static void __fiq_smp_call_init(void *info)
{
	int err;

	err = request_fiq(FIQ_SMP_CALL_SGI, fiq_smp_call_handler, 0, NULL);
	if (err)
		pr_err("fail to request FIQ for FIQ_SMP_CALL_SGI\n");
	else
		pr_debug("Request FIQ for FIQ_SMP_CALL_SGI\n");
}

static int __init fiq_smp_call_init(void)
{
	__fiq_smp_call_init(NULL);
	smp_call_function(__fiq_smp_call_init, NULL, 1);

	return 0;
}
subsys_initcall(fiq_smp_call_init);

#endif				/* CONFIG_FIQ_GLUE */
