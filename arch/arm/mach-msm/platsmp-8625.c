/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <asm/cacheflush.h>
#include <asm/hardware/gic.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/smp_scu.h>
#include <asm/unified.h>
#include <mach/msm_iomap.h>
#include "pm.h"

#define MSM_CORE1_RESET		0xA8600590
#define MSM_CORE1_STATUS_MSK	0x02800000

/*
 * control for which core is the next to come out of the secondary
 * boot "holding pen"
 */
int pen_release = -1;

static bool cold_boot_done;

static uint32_t *msm8625_boot_vector;
static void __iomem *reset_core1_base;

/*
 * Write pen_release in a way that is guaranteed to be visible to all
 * observers, irrespective of whether they're taking part in coherency
 * or not.  This is necessary for the hotplug code to work reliably.
 */
static void __cpuinit write_pen_release(int val)
{
	pen_release = val;
	smp_wmb();
	__cpuc_flush_dcache_area((void *)&pen_release, sizeof(pen_release));
	outer_clean_range(__pa(&pen_release), __pa(&pen_release + 1));
}

static void __iomem *scu_base_addr(void)
{
	return MSM_SCU_BASE;
}

static DEFINE_SPINLOCK(boot_lock);

/*
 * MP_CORE_IPC will be used to generate interrupt and can be used by either
 * of core.
 * To bring core1 out of GDFS we need to raise the SPI using the MP_CORE_IPC.
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

void __cpuinit platform_secondary_init(unsigned int cpu)
{
	pr_debug("CPU%u: Booted secondary processor\n", cpu);

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

	/* clear the IPC1(SPI-8) pending SPI */
	if (power_collapsed) {
		raise_clear_spi(1, false);
		clear_pending_spi(MSM8625_INT_ACSR_MP_CORE_IPC1);
		power_collapsed = 0;
	}

	/*
	 * Synchronise with the boot thread.
	 */
	spin_lock(&boot_lock);
	spin_unlock(&boot_lock);
}

static int  __cpuinit msm8625_release_secondary(void)
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
		value = __raw_readl(MSM_CFG_CTL_BASE + 0x3c);
		if ((value & MSM_CORE1_STATUS_MSK) ==
				MSM_CORE1_STATUS_MSK)
			break;
			udelay(1);
	}

	if (!value) {
		pr_err("Core 1 cannot be brought out of Reset!!!\n");
		return -ENODEV;
	}

	base_ptr = ioremap_nocache(MSM_CORE1_RESET, SZ_4);
	if (!base_ptr)
		return -ENODEV;
	/* Reset core 1 out of reset */
	__raw_writel(0x0, base_ptr);
	mb();

	reset_core1_base = base_ptr;

	return 0;
}

void __iomem *core1_reset_base(void)
{
	return reset_core1_base;
}

int __cpuinit boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	unsigned long timeout;

	preset_lpj = loops_per_jiffy;

	if (cold_boot_done == false) {
		if (msm8625_release_secondary()) {
			pr_err("Failed to release secondary core\n");
			return -ENODEV;
		}
		cold_boot_done = true;
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
	 * Once we are out of PC, as Core1 will be in the state of GDFS which
	 * needs to be brought out by raising an SPI.
	 */

	if (power_collapsed) {
		core1_gic_configure_and_raise();
		raise_clear_spi(1, true);
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
void __init smp_init_cpus(void)
{
	void __iomem *scu_base = scu_base_addr();

	unsigned int i, ncores;

	ncores = scu_base ? scu_get_core_count(scu_base) : 1;

	for (i = 0; i < ncores; i++)
		set_cpu_possible(i, true);

	set_smp_cross_call(gic_raise_softirq);
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

void __init platform_smp_prepare_cpus(unsigned int max_cpus)
{
	int i, value;
	void __iomem *second_ptr;

	/*
	 * Initialise the present map, which describes the set of CPUs
	 * actually populated at the present time.
	 */
	for (i = 0; i < max_cpus; i++)
		set_cpu_present(i, true);

	scu_enable(scu_base_addr());

	/*
	 * Write the address of secondary startup into the
	 * boot remapper register. The secondary CPU branches to this address.
	 */
	__raw_writel(MSM8625_SECONDARY_PHYS, (MSM_CFG_CTL_BASE + 0x34));
	mb();

	second_ptr = ioremap_nocache(MSM8625_SECONDARY_PHYS, SZ_8);
	if (!second_ptr) {
		pr_err("failed to ioremap for secondary core\n");
		return;
	}

	msm8625_boot_vector_init(second_ptr,
			virt_to_phys(msm_secondary_startup));
	iounmap(second_ptr);

	/* Enable boot remapper address: bit 26 for core1 */
	value = __raw_readl(MSM_CFG_CTL_BASE + 0x30);
	__raw_writel(value | (0x4 << 24), MSM_CFG_CTL_BASE + 0x30) ;
	mb();
}
