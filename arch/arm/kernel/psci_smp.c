/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Copyright (C) 2012 ARM Limited
 *
 * Author: Will Deacon <will.deacon@arm.com>
 */

#include <linux/init.h>
#include <linux/smp.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <uapi/linux/psci.h>

#include <asm/psci.h>
#include <asm/smp_plat.h>

/*
 * psci_smp assumes that the following is true about PSCI:
 *
 * cpu_suspend   Suspend the execution on a CPU
 * @state        we don't currently describe affinity levels, so just pass 0.
 * @entry_point  the first instruction to be executed on return
 * returns 0  success, < 0 on failure
 *
 * cpu_off       Power down a CPU
 * @state        we don't currently describe affinity levels, so just pass 0.
 * no return on successful call
 *
 * cpu_on        Power up a CPU
 * @cpuid        cpuid of target CPU, as from MPIDR
 * @entry_point  the first instruction to be executed on return
 * returns 0  success, < 0 on failure
 *
 * migrate       Migrate the context to a different CPU
 * @cpuid        cpuid of target CPU, as from MPIDR
 * returns 0  success, < 0 on failure
 *
 */

extern void secondary_startup(void);

extern void __init mt_smp_prepare_cpus(unsigned int max_cpus);
extern void mt_smp_secondary_init(unsigned int cpu);
extern int mt_smp_boot_secondary(unsigned int cpu, struct task_struct *idle);
extern int mt_cpu_kill(unsigned int cpu);

static int psci_boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	int ret = -1;

	if (psci_ops.cpu_on)
		ret = psci_ops.cpu_on(cpu_logical_map(cpu),
				       __pa(secondary_startup));

	if (ret < 0) {
		pr_err("psci cpu_on failed\n");
		return -ENODEV;
	}

	ret = mt_smp_boot_secondary(cpu, idle);
	if (ret < 0) {
		pr_err("mt_smp_boot_secondary failed\n");
		return -ENODEV;
	}
	return 0;
}

#ifdef CONFIG_HOTPLUG_CPU
void __ref psci_cpu_die(unsigned int cpu)
{
	const struct psci_power_state ps = {
		 .type = PSCI_POWER_STATE_TYPE_POWER_DOWN,
	};

	if (psci_ops.cpu_off)
		 psci_ops.cpu_off(ps);

	/* We should never return */
	panic("psci: cpu %d failed to shutdown\n", cpu);
}

int __ref psci_cpu_kill(unsigned int cpu)
{
	//TODO: do more accurate synchornization between src CPU and dst CPU when applying psci
	//int err, i;
	//
	//if (!psci_ops.affinity_info)
	//	return 1;
	///*
	// * cpu_kill could race with cpu_die and we can
	// * potentially end up declaring this cpu undead
	// * while it is dying. So, try again a few times.
	// */
	//
	//for (i = 0; i < 10; i++) {
	//	err = psci_ops.affinity_info(cpu_logical_map(cpu), 0);
	//	if (err == PSCI_0_2_AFFINITY_LEVEL_OFF) {
	//		pr_info("CPU%d killed.\n", cpu);
	//		return 1;
	//	}
	//
	//	msleep(10);
	//	pr_info("Retrying again to check for CPU kill\n");
	//}
	//
	//pr_warn("CPU%d may not have shut down cleanly (AFFINITY_INFO reports %d)\n",
	//		cpu, err);
	///* Make platform_cpu_kill() fail. */
	//return 0;
	return mt_cpu_kill(cpu);
}

#endif

bool __init psci_smp_available(void)
{
	/* is cpu_on available at least? */
	return (psci_ops.cpu_on != NULL);
}

struct smp_operations __initdata psci_smp_ops = {
	.smp_prepare_cpus       = mt_smp_prepare_cpus,
	.smp_boot_secondary	    = psci_boot_secondary,
	// FIXME: better way?
	.smp_secondary_init     = mt_smp_secondary_init,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_die		        = psci_cpu_die,
	.cpu_kill		        = psci_cpu_kill,
#endif
};
