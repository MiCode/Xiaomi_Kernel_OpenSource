/*
 * arch/arch/mach-tegra/timer-t3.c
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/init.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/smp.h>
#include <linux/syscore_ops.h>
#include <linux/cpu.h>
#include <linux/export.h>

#include <asm/mach/time.h>
#include <asm/localtimer.h>
#include <asm/sched_clock.h>
#include <asm/smp_plat.h>

#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/hardware.h>

#include "board.h"
#include "clock.h"
#include "cpuidle.h"
#include "timer.h"
#include "fuse.h"

#define TEST_LP2_WAKE_TIMERS	0

/*
 * Timers usage:
 * TMR1 - used as general CPU timer.
 * TMR2 - used by AVP.
 * TMR3 - used by CPU0 for LP2 wakeup.
 * TMR4 - used by CPU1 for LP2 wakeup.
 * TMR5 - used by CPU2 for LP2 wakeup.
 * TMR6 - used by CPU3 for LP2 wakeup.
 * TMR7 - watchdog, for generic purpose.
 * TMR8 - Free.
 * TMR9 - Free.
 * TMR10 - watchdog, suspend/resume recovery
*/

#define TIMER1_OFFSET (TEGRA_TMR1_BASE-TEGRA_TMR1_BASE)
#define TIMER2_OFFSET (TEGRA_TMR2_BASE-TEGRA_TMR1_BASE)
#define TIMER3_OFFSET (TEGRA_TMR3_BASE-TEGRA_TMR1_BASE)
#define TIMER4_OFFSET (TEGRA_TMR4_BASE-TEGRA_TMR1_BASE)
#define TIMER5_OFFSET (TEGRA_TMR5_BASE-TEGRA_TMR1_BASE)
#define TIMER6_OFFSET (TEGRA_TMR6_BASE-TEGRA_TMR1_BASE)

static void __iomem *timer_reg_base = IO_ADDRESS(TEGRA_TMR1_BASE);

#if defined(CONFIG_PM_SLEEP)
static cpumask_t wake_timer_canceled;
static cpumask_t wake_timer_ready;
#endif

static inline void timer_writel(u32 value, unsigned long reg)
{
	__raw_writel(value, timer_reg_base + (reg));
}

static inline unsigned int timer_readl(unsigned long reg)
{
	return __raw_readl(timer_reg_base + (reg));
}

#ifdef CONFIG_PM_SLEEP
static u32 lp2_wake_timers[] = {
	TIMER3_OFFSET,
#ifdef CONFIG_SMP
	TIMER4_OFFSET,
	TIMER5_OFFSET,
	TIMER6_OFFSET,
#endif
};

static irqreturn_t tegra_lp2wake_interrupt(int irq, void *dev_id)
{
	int cpu = (int)dev_id;
	int base;

	base = lp2_wake_timers[cpu];
	timer_writel(1<<30, base + TIMER_PCR);
	return IRQ_HANDLED;
}

#define LP2_TIMER_IRQ_ACTION(cpu, irqnum) {			\
	.name		= "tmr_lp2wake_cpu" __stringify(cpu),	\
	.flags		= IRQF_DISABLED,			\
	.handler	= tegra_lp2wake_interrupt,		\
	.dev_id		= (void*)cpu,				\
	.irq		= irqnum }

static struct irqaction tegra_lp2wake_irq[] = {
	LP2_TIMER_IRQ_ACTION(0, INT_TMR3),
#ifdef CONFIG_SMP
	LP2_TIMER_IRQ_ACTION(1, INT_TMR4),
	LP2_TIMER_IRQ_ACTION(2, INT_TMR5),
	LP2_TIMER_IRQ_ACTION(3, INT_TMR6),
#endif
};

/*
 * To sanity test LP2 timer interrupts for CPU 0-3, enable this flag and check
 * /proc/interrupts for timer interrupts. CPUs 0-3 should have one interrupt
 * counted against them for tmr_lp2wake_cpu<n>, where <n> is the CPU number.
 */
#if TEST_LP2_WAKE_TIMERS
static void test_lp2_wake_timer(unsigned int cpu)
{
	unsigned long cycles = 50000;
	unsigned int base = lp2_wake_timers[cpu];
	static bool tested[4] = {false, false, false, false};

	/* Don't repeat the test process on hotplug restart. */
	if (!tested[cpu]) {
		timer_writel(0, base + TIMER_PTV);
		if (cycles) {
			u32 reg = 0x80000000ul | min(0x1ffffffful, cycles);
			timer_writel(reg, base + TIMER_PTV);
			tested[cpu] = true;
		}
	}
}
#else
static inline void test_lp2_wake_timer(unsigned int cpu) {}
#endif

static int tegra3_resume_wake_timer(unsigned int cpu)
{
#ifdef CONFIG_SMP
	int ret = irq_set_affinity(tegra_lp2wake_irq[cpu].irq, cpumask_of(cpu));
	if (ret) {
		pr_err("Failed to set affinity for LP2 timer IRQ to "
			"CPU %d: irq=%d, ret=%d\n", cpu,
			tegra_lp2wake_irq[cpu].irq, ret);
		return ret;
	}
#endif
	cpumask_set_cpu(cpu, &wake_timer_ready);
	return 0;
}

static void tegra3_register_wake_timer(unsigned int cpu)
{
	int ret;

	ret = setup_irq(tegra_lp2wake_irq[cpu].irq, &tegra_lp2wake_irq[cpu]);
	if (ret) {
		pr_err("Failed to register LP2 timer IRQ for CPU %d: "
			"irq=%d, ret=%d\n", cpu,
			tegra_lp2wake_irq[cpu].irq, ret);
		goto fail;
	}

	ret = tegra3_resume_wake_timer(cpu);
	if (ret)
		goto fail;

	test_lp2_wake_timer(cpu);
	return;
fail:
	tegra_pd_in_idle(false);
}

#if defined(CONFIG_PM_SLEEP) && defined(CONFIG_HOTPLUG_CPU)
static void tegra3_suspend_wake_timer(unsigned int cpu)
{
	cpumask_clear_cpu(cpu, &wake_timer_ready);
#ifdef CONFIG_SMP
	/* Reassign the affinity of the wake IRQ to any ready CPU. */
	for_each_cpu_not(cpu, &wake_timer_ready)
	{
		(void)irq_set_affinity(tegra_lp2wake_irq[cpu].irq,
			cpumask_of(cpumask_any(&wake_timer_ready)));
	}
#endif
}

static void tegra3_unregister_wake_timer(unsigned int cpu)
{
	tegra3_suspend_wake_timer(cpu);

	/* Dispose of this IRQ. */
	remove_irq(tegra_lp2wake_irq[cpu].irq, &tegra_lp2wake_irq[cpu]);
}
#endif

void tegra3_lp2_set_trigger(unsigned long cycles)
{
	int cpu = cpu_logical_map(smp_processor_id());
	int base;

	base = lp2_wake_timers[cpu];
	timer_writel(0, base + TIMER_PTV);
	if (cycles) {
		u32 reg = 0x80000000ul | min(0x1ffffffful, cycles);
		timer_writel(reg, base + TIMER_PTV);
	}
}
EXPORT_SYMBOL(tegra3_lp2_set_trigger);

unsigned long tegra3_lp2_timer_remain(void)
{
	int cpu = cpu_logical_map(smp_processor_id());

	if (cpumask_test_and_clear_cpu(cpu, &wake_timer_canceled))
		return -ETIME;

	return timer_readl(lp2_wake_timers[cpu] + TIMER_PCR) & 0x1ffffffful;
}

int tegra3_is_cpu_wake_timer_ready(unsigned int cpu)
{
	return cpumask_test_cpu(cpu, &wake_timer_ready);
}

void tegra3_lp2_timer_cancel_secondary(void)
{
	int cpu;
	int base;

	for (cpu = 1; cpu < ARRAY_SIZE(lp2_wake_timers); cpu++) {
		base = lp2_wake_timers[cpu];
		cpumask_set_cpu(cpu, &wake_timer_canceled);
		timer_writel(0, base + TIMER_PTV);
		timer_writel(1<<30, base + TIMER_PCR);
	}
}
#endif

void __init tegra30_init_timer(void)
{
#ifdef CONFIG_PM_SLEEP
#ifdef CONFIG_SMP
	/* For T30.A01 use INT_TMR_SHARED instead of INT_TMR6 for CPU3. */
	if ((tegra_get_chipid() == TEGRA_CHIPID_TEGRA3) &&
		(tegra_revision == TEGRA_REVISION_A01))
			tegra_lp2wake_irq[3].irq = INT_TMR_SHARED;
#endif

	tegra3_register_wake_timer(0);
#endif
}

#if defined(CONFIG_PM_SLEEP) && defined(CONFIG_HOTPLUG_CPU)
static int hotplug_notify(struct notifier_block *self,
				      unsigned long action, void *cpu)
{
	switch (action) {
	case CPU_ONLINE:
		tegra3_register_wake_timer((unsigned int)cpu);
		break;
	case CPU_ONLINE_FROZEN:
		tegra3_resume_wake_timer((unsigned int)cpu);
		break;
	case CPU_DOWN_PREPARE:
		tegra3_unregister_wake_timer((unsigned int)cpu);
		break;
	case CPU_DOWN_PREPARE_FROZEN:
		tegra3_suspend_wake_timer((unsigned int)cpu);
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block __cpuinitdata hotplug_notifier_block = {
	.notifier_call = hotplug_notify,
};

static int __init hotplug_cpu_register(void)
{
	return register_cpu_notifier(&hotplug_notifier_block);
}
early_initcall(hotplug_cpu_register);
#endif
