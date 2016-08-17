/*
 * arch/arm/mach-tegra/wdt-recovery.c
 *
 * Copyright (c) 2012, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
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

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/suspend.h>
#include <linux/resource.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/syscore_ops.h>
#include <linux/io.h>

#include <asm/mach-types.h>
#include <asm/mach/time.h>
#include <asm/localtimer.h>

#include <linux/nvmap.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/clk.h>
#include <mach/io.h>

static int wdt_heartbeat = 30;

#if defined(CONFIG_ARCH_TEGRA_3x_SOC)
#define TIMER_PTV			0
 #define TIMER_EN			(1 << 31)
 #define TIMER_PERIODIC			(1 << 30)
#define TIMER_PCR			0x4
 #define TIMER_PCR_INTR			(1 << 30)
#define WDT_CFG				(0)
 #define WDT_CFG_TMR_SRC		(0 << 0) /* for TMR10. */
 #define WDT_CFG_PERIOD			(1 << 4)
 #define WDT_CFG_INT_EN			(1 << 12)
 #define WDT_CFG_SYS_RST_EN		(1 << 14)
 #define WDT_CFG_PMC2CAR_RST_EN		(1 << 15)
#define WDT_CMD				(8)
 #define WDT_CMD_START_COUNTER		(1 << 0)
 #define WDT_CMD_DISABLE_COUNTER	(1 << 1)
#define WDT_UNLOCK			(0xC)
 #define WDT_UNLOCK_PATTERN		(0xC45A << 0)

static void __iomem *wdt_timer  = IO_ADDRESS(TEGRA_TMR10_BASE);
static void __iomem *wdt_source = IO_ADDRESS(TEGRA_WDT3_BASE);

static void tegra_wdt_reset_enable(void)
{
	u32 val;

	writel(TIMER_PCR_INTR, wdt_timer + TIMER_PCR);
	val = (wdt_heartbeat * 1000000ul) / 4;
	val |= (TIMER_EN | TIMER_PERIODIC);
	writel(val, wdt_timer + TIMER_PTV);

	val = WDT_CFG_TMR_SRC | WDT_CFG_PERIOD | /*WDT_CFG_INT_EN |*/
		/*WDT_CFG_SYS_RST_EN |*/ WDT_CFG_PMC2CAR_RST_EN;
	writel(val, wdt_source + WDT_CFG);
	writel(WDT_CMD_START_COUNTER, wdt_source + WDT_CMD);
	pr_info("%s: WDT Recovery Enabled\n", __func__);
}

static int tegra_wdt_reset_disable(void)
{
	writel(TIMER_PCR_INTR, wdt_timer + TIMER_PCR);
	writel(WDT_UNLOCK_PATTERN, wdt_source + WDT_UNLOCK);
	writel(WDT_CMD_DISABLE_COUNTER, wdt_source + WDT_CMD);

	writel(0, wdt_timer + TIMER_PTV);
	pr_info("%s: WDT Recovery Disabled\n", __func__);

	return 0;
}
#elif defined(CONFIG_ARCH_TEGRA_2x_SOC)

static void tegra_wdt_reset_enable(void)
{
}
static int tegra_wdt_reset_disable(void)
{
	return 0;
}
#endif

static int tegra_pm_notify(struct notifier_block *nb,
			unsigned long event, void *nouse)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		tegra_wdt_reset_enable();
		break;
	case PM_POST_SUSPEND:
		tegra_wdt_reset_disable();
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block tegra_wdt_notify = {
	.notifier_call = tegra_pm_notify,
};

static struct syscore_ops tegra_wdt_syscore_ops = {
	.suspend =	tegra_wdt_reset_disable,
	.resume =	tegra_wdt_reset_enable,
};

void __init tegra_wdt_recovery_init(void)
{
#ifdef CONFIG_PM
	/* Register PM notifier. */
	register_pm_notifier(&tegra_wdt_notify);
#endif
	register_syscore_ops(&tegra_wdt_syscore_ops);
}
