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

#include <linux/init.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/smp.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#include <asm/hardware/gic.h>
#include <asm/smp_scu.h>
#include <asm/unified.h>
#include <mach/msm_iomap.h>
#include "pm.h"
#include "platsmp.h"

#define CORE_RESET_BASE		0xA8600590
#define MSM_CORE_STATUS_MSK	0x02800000

static DEFINE_PER_CPU(bool, cold_boot_done);

struct per_cpu_data {
	unsigned int reset_off;
	unsigned int offset;
	unsigned int ipc_irq;
	void __iomem *reset_core_base;
};

static uint32_t *msm8625_boot_vector;

static struct per_cpu_data cpu_data[CONFIG_NR_CPUS];

static void __iomem *scu_base_addr(void)
{
	return MSM_SCU_BASE;
}

static DEFINE_SPINLOCK(boot_lock);

/*
 * MP_CORE_IPC will be used to generate interrupt and can be used by either
 * of core.
 * To bring secondary cores out of GDFS we need to raise the SPI using the
 * MP_CORE_IPC.
 */
static void raise_clear_spi(unsigned int cpu, bool set)
{
	int value;

	value = __raw_readl(MSM_CSR_BASE + 0x54);
	if (set)
		__raw_writel(value | BIT(cpu), MSM_CSR_BASE + 0x54);
	else
		__raw_writel(value & ~BIT(cpu), MSM_CSR_BASE + 0x54);
	mb();
}

static void clear_pending_spi(unsigned int irq)
{
	struct irq_data *d = irq_get_irq_data(irq);
	struct irq_chip *c = irq_data_get_irq_chip(d);

	c->irq_mask(d);
	local_irq_disable();
	/* Clear the IRQ from the ENABLE_SET */
	gic_clear_irq_pending(irq);
	local_irq_enable();
}

void __cpuinit msm8625_platform_secondary_init(unsigned int cpu)
{
	WARN_ON(msm_platform_secondary_init(cpu));

	/*
	 * if any interrupts are already enabled for the primary
	 * core (e.g. timer irq), then they will not have been enabled
	 * for us: do so
	 */
	gic_secondary_init(0);

	/*
	 * let the primary processor know we're out of the
	 * pen, then head off into the C entry point
	 */
	write_pen_release(-1);

	/* clear the IPC pending SPI */
	if (per_cpu(power_collapsed, cpu)) {
		raise_clear_spi(cpu, false);
		clear_pending_spi(cpu_data[cpu].ipc_irq);
		per_cpu(power_collapsed, cpu) = 0;
	}

	/*
	 * Synchronise with the boot thread.
	 */
	spin_lock(&boot_lock);
	spin_unlock(&boot_lock);
}

static int __cpuinit msm8625_release_secondary(unsigned int cpu)
{
	void __iomem *base_ptr;
	int value = 0;
	unsigned long timeout;

	/*
	 * loop to ensure that the GHS_STATUS_CORE1 bit in the
	 * MPA5_STATUS_REG(0x3c) is set. The timeout for the while
	 * loop can be set as 20us as of now
	 */
	timeout = jiffies + usecs_to_jiffies(20);
	while (time_before(jiffies, timeout)) {
		value = __raw_readl(MSM_CFG_CTL_BASE + cpu_data[cpu].offset);
		if ((value & MSM_CORE_STATUS_MSK) ==
				MSM_CORE_STATUS_MSK)
			break;
			udelay(1);
	}

	if (!value) {
		pr_err("Core %u cannot be brought out of Reset!!!\n", cpu);
		return -ENODEV;
	}

	base_ptr = ioremap_nocache(CORE_RESET_BASE +
			cpu_data[cpu].reset_off, SZ_4);
	if (!base_ptr)
		return -ENODEV;

	/* Reset core out of reset */
	__raw_writel(0x0, base_ptr);
	mb();

	cpu_data[cpu].reset_core_base = base_ptr;

	return 0;
}

void __iomem *core_reset_base(unsigned int cpu)
{
	return cpu_data[cpu].reset_core_base;
}

int __cpuinit msm8625_boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	unsigned long timeout;

	preset_lpj = loops_per_jiffy;

	if (per_cpu(cold_boot_done, cpu) == false) {
		if (msm8625_release_secondary(cpu)) {
			pr_err("Failed to release core %u\n", cpu);
			return -ENODEV;
		}
		per_cpu(cold_boot_done, cpu) = true;
	}

	/*
	 * Set synchronisation state between this boot processor
	 * and the secondary one
	 */
	spin_lock(&boot_lock);

	/*
	 * This is really belt and braces; we hold unintended secondary
	 * CPUs in the holding pen until we're ready for them.  However,
	 * since we haven't sent them a soft interrupt, they shouldn't
	 * be there.
	 */
	write_pen_release(cpu);

	/*
	 * Send the secondary CPU a soft interrupt, thereby causing
	 * the boot monitor to read the system wide flags register,
	 * and branch to the address found there.
	 *
	 * power_collapsed is the flag which will be updated for Powercollapse.
	 * Once we are out of PC, as secondary cores will be in the state of
	 * GDFS which needs to be brought out by raising an SPI.
	 */

	if (per_cpu(power_collapsed, cpu)) {
		gic_configure_and_raise(cpu_data[cpu].ipc_irq, cpu);
		raise_clear_spi(cpu, true);
	} else {
		gic_raise_softirq(cpumask_of(cpu), 1);
	}

	timeout = jiffies + (1 * HZ);
	while (time_before(jiffies, timeout)) {
		smp_rmb();
		if (pen_release == -1)
			break;

		udelay(10);
	}

	/*
	 * now the secondary core is starting up let it run its
	 * calibrations, then wait for it to finish
	 */
	spin_unlock(&boot_lock);

	return 0;
}

/*
 * Initialise the CPU possible map early - this describes the CPUs
 * which may be present or become present in the system.
 */
void __init msm8625_smp_init_cpus(void)
{
	void __iomem *scu_base = scu_base_addr();

	unsigned int i, ncores;

	ncores = scu_base ? scu_get_core_count(scu_base) : 1;

	for (i = 0; i < ncores; i++)
		set_cpu_possible(i, true);

	set_smp_cross_call(gic_raise_softirq);
}

static void per_cpu_data(unsigned int cpu, unsigned int off,
	unsigned int off1, unsigned int irq)
{
	cpu_data[cpu].reset_off = off;
	cpu_data[cpu].offset    = off1;
	cpu_data[cpu].ipc_irq   = irq;
}

static void enable_boot_remapper(unsigned long bit, unsigned int off)
{
	int value;

	/* Enable boot remapper address */
	value = __raw_readl(MSM_CFG_CTL_BASE + off);
	__raw_writel(value | bit, MSM_CFG_CTL_BASE + off) ;
	mb();
}

static void remapper_address(unsigned long phys, unsigned int off)
{
	/*
	 * Write the address of secondary startup into the
	 * boot remapper register. The secondary CPU branches to this address.
	 */
	__raw_writel(phys, (MSM_CFG_CTL_BASE + off));
	mb();
}

static void __init msm8625_boot_vector_init(uint32_t *boot_vector,
		unsigned long entry)
{
	if (!boot_vector)
		return;
	msm8625_boot_vector = boot_vector;

	msm8625_boot_vector[0] = 0xE51FF004; /* ldr pc, 4 */
	msm8625_boot_vector[1] = entry;
}

void __init msm8625_platform_smp_prepare_cpus(unsigned int max_cpus)
{
	int cpu, value;
	void __iomem *cpu_ptr;

	scu_enable(scu_base_addr());

	cpu_ptr = ioremap_nocache(MSM8625_CPU_PHYS, SZ_8);
	if (!cpu_ptr) {
		pr_err("failed to ioremap for secondary cores\n");
		return;
	}

	msm8625_boot_vector_init(cpu_ptr,
			virt_to_phys(msm_secondary_startup));

	iounmap(cpu_ptr);

	for_each_possible_cpu(cpu) {
		switch (cpu) {
		case 0:
			break;
		case 1:
			remapper_address(MSM8625_CPU_PHYS, 0x34);
			per_cpu_data(cpu, 0x0, 0x3c,
					MSM8625_INT_ACSR_MP_CORE_IPC1);
			enable_boot_remapper(BIT(26), 0x30);
			break;
		case 2:
			remapper_address((MSM8625_CPU_PHYS >> 16), 0x4C);
			per_cpu_data(cpu, 0x8, 0x50,
					MSM8625_INT_ACSR_MP_CORE_IPC2);
			enable_boot_remapper(BIT(25), 0x48);
			break;
		case 3:
			value = __raw_readl(MSM_CFG_CTL_BASE + 0x4C);
			remapper_address(value | MSM8625_CPU_PHYS, 0x4C);
			per_cpu_data(cpu, 0xC, 0x50,
					MSM8625_INT_ACSR_MP_CORE_IPC3);
			enable_boot_remapper(BIT(26), 0x48);
			break;
		}

	}
}

struct smp_operations msm8625_smp_ops __initdata = {
	.smp_init_cpus = msm8625_smp_init_cpus,
	.smp_prepare_cpus = msm8625_platform_smp_prepare_cpus,
	.smp_secondary_init = msm8625_platform_secondary_init,
	.smp_boot_secondary = msm8625_boot_secondary,
	.cpu_kill = platform_cpu_kill,
	.cpu_die = platform_cpu_die,
	.cpu_disable = platform_cpu_disable
};
