/*
 * arch/arch/mach-tegra/timer.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Copyright (C) 2011 NVIDIA Corporation.
 *
 * Author:
 *	Colin Cross <ccross@google.com>
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

#include <linux/init.h>
#include <linux/err.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/syscore_ops.h>
#include <linux/export.h>

#include <asm/mach/time.h>
#include <asm/localtimer.h>
#include <asm/sched_clock.h>

#include <mach/iomap.h>
#include <mach/irqs.h>

#include "board.h"
#include "clock.h"
#include "timer.h"

/*
 * Timers usage:
 * TMR1 - Free.
 * TMR2 - used by AVP.
 * TMR3 - used as general CPU timer.
 * TMR4 - used for LP2 wakeup.
*/

#define TIMER1_OFFSET (TEGRA_TMR1_BASE-TEGRA_TMR1_BASE)
#define TIMER2_OFFSET (TEGRA_TMR2_BASE-TEGRA_TMR1_BASE)
#define TIMER3_OFFSET (TEGRA_TMR3_BASE-TEGRA_TMR1_BASE)
#define TIMER4_OFFSET (TEGRA_TMR4_BASE-TEGRA_TMR1_BASE)

#define timer_writel(value, reg) \
	__raw_writel(value, timer_reg_base + (reg))
#define timer_readl(reg) \
	__raw_readl(timer_reg_base + (reg))


static void __iomem *timer_reg_base = IO_ADDRESS(TEGRA_TMR1_BASE);

#ifdef CONFIG_PM_SLEEP
static irqreturn_t tegra_lp2wake_interrupt(int irq, void *dev_id)
{
	timer_writel(1<<30, TIMER4_OFFSET + TIMER_PCR);
	return IRQ_HANDLED;
}

static struct irqaction tegra_lp2wake_irq = {
	.name		= "timer_lp2wake",
	.flags		= IRQF_DISABLED,
	.handler	= tegra_lp2wake_interrupt,
	.dev_id		= NULL,
	.irq		= INT_TMR4,
};

void tegra2_lp2_set_trigger(unsigned long cycles)
{
	timer_writel(0, TIMER4_OFFSET + TIMER_PTV);
	if (cycles) {
		u32 reg = 0x80000000ul | min(0x1ffffffful, cycles);
		timer_writel(reg, TIMER4_OFFSET + TIMER_PTV);
	}
}
EXPORT_SYMBOL(tegra2_lp2_set_trigger);

unsigned long tegra2_lp2_timer_remain(void)
{
	return timer_readl(TIMER4_OFFSET + TIMER_PCR) & 0x1ffffffful;
}
#endif

void __init tegra20_init_timer(void)
{
	int ret;

#ifdef CONFIG_PM_SLEEP
	ret = setup_irq(tegra_lp2wake_irq.irq, &tegra_lp2wake_irq);
	if (ret) {
		pr_err("Failed to register LP2 timer IRQ: %d\n", ret);
		BUG();
	}
#endif
}
