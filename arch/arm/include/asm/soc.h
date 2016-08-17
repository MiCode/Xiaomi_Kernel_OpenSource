/*
 *  linux/arch/arm/include/asm/soc.h
 *
 *  Copyright (C) 2011 ARM Ltd.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARM_SOC_H
#define __ASM_ARM_SOC_H

struct task_struct;

struct arm_soc_smp_init_ops {
	/*
	 * Setup the set of possible CPUs (via set_cpu_possible)
	 */
	void (*smp_init_cpus)(void);
	/*
	 * Initialize cpu_possible map, and enable coherency
	 */
	void (*smp_prepare_cpus)(unsigned int max_cpus);
};

struct arm_soc_smp_ops {
	/*
	 * Perform platform specific initialisation of the specified CPU.
	 */
	void (*smp_secondary_init)(unsigned int cpu);
	/*
	 * Boot a secondary CPU, and assign it the specified idle task.
	 * This also gives us the initial stack to use for this CPU.
	 */
	int  (*smp_boot_secondary)(unsigned int cpu, struct task_struct *idle);
#ifdef CONFIG_HOTPLUG_CPU
	int  (*cpu_kill)(unsigned int cpu);
	void (*cpu_die)(unsigned int cpu);
	int  (*cpu_disable)(unsigned int cpu);
#endif
};

struct arm_soc_desc {
	const char			*name;
#ifdef CONFIG_SMP
	struct arm_soc_smp_init_ops	*smp_init_ops;
	struct arm_soc_smp_ops		*smp_ops;
#endif
};

#ifdef CONFIG_SMP
#define soc_smp_init_ops(ops)		.smp_init_ops = &(ops),
#define soc_smp_ops(ops)		.smp_ops = &(ops),
extern void soc_smp_ops_register(struct arm_soc_smp_init_ops *,
				 struct arm_soc_smp_ops *);
#else
#define soc_smp_init_ops(ops)		/* empty */
#define soc_smp_ops(ops)		/* empty */
#define soc_smp_ops_register(a,b)	do {} while(0)
#endif

#endif	/* __ASM_ARM_SOC_H */
