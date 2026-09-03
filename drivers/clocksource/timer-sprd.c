// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 */

#include <linux/alarmtimer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/soc/sprd/sprd_systimer.h>
#include <linux/suspend.h>
#include <linux/syscore_ops.h>
#include <linux/rtc.h>

#include "timer-of.h"

#define TIMER_NAME		"sprd_timer"

#define TIMER_LOAD_LO		0x0
#define TIMER_LOAD_HI		0x4
#define TIMER_VALUE_LO		0x8
#define TIMER_VALUE_HI		0xc

#define TIMER_CTL		0x10
#define TIMER_CTL_PERIOD_MODE	BIT(0)
#define TIMER_CTL_ENABLE	BIT(1)
#define TIMER_CTL_64BIT_WIDTH	BIT(16)

#define TIMER_INT		0x14
#define TIMER_INT_EN		BIT(0)
#define TIMER_INT_RAW_STS	BIT(1)
#define TIMER_INT_MASK_STS	BIT(2)
#define TIMER_INT_CLR		BIT(3)

#define TIMER_VALUE_SHDW_LO	0x18
#define TIMER_VALUE_SHDW_HI	0x1c

#define TIMER_VALUE_LO_MASK	GENMASK(31, 0)

static u64 suspend_timer_count;
#if IS_ENABLED(CONFIG_SPRD_TIMER_TEST)
static struct notifier_block suspend_notify;
static u64 sys_timer_count;
static u64 frt_timer_count;
static struct rtc_device *rtc;
static struct rtc_time tm_start, tm_now;
static u64 boottime_start, boottime_now;
#endif

static void sprd_timer_enable(void __iomem *base, u32 flag)
{
	u32 val = readl_relaxed(base + TIMER_CTL);

	val |= TIMER_CTL_ENABLE;
	if (flag & TIMER_CTL_64BIT_WIDTH)
		val |= TIMER_CTL_64BIT_WIDTH;
	else
		val &= ~TIMER_CTL_64BIT_WIDTH;

	if (flag & TIMER_CTL_PERIOD_MODE)
		val |= TIMER_CTL_PERIOD_MODE;
	else
		val &= ~TIMER_CTL_PERIOD_MODE;

	writel_relaxed(val, base + TIMER_CTL);
}

static void sprd_timer_disable(void __iomem *base)
{
	u32 val = readl_relaxed(base + TIMER_CTL);

	val &= ~TIMER_CTL_ENABLE;
	writel_relaxed(val, base + TIMER_CTL);
}

static void sprd_timer_update_counter(void __iomem *base, unsigned long cycles)
{
	writel_relaxed(cycles & TIMER_VALUE_LO_MASK, base + TIMER_LOAD_LO);
	writel_relaxed(0, base + TIMER_LOAD_HI);
}

static void sprd_timer_enable_interrupt(void __iomem *base)
{
	writel_relaxed(TIMER_INT_EN, base + TIMER_INT);
}

static void sprd_timer_clear_interrupt(void __iomem *base)
{
	u32 val = readl_relaxed(base + TIMER_INT);

	val |= TIMER_INT_CLR;
	writel_relaxed(val, base + TIMER_INT);
}

static int sprd_timer_set_next_event(unsigned long cycles,
				     struct clock_event_device *ce)
{
	struct timer_of *to = to_timer_of(ce);

	sprd_timer_disable(timer_of_base(to));
	sprd_timer_update_counter(timer_of_base(to), cycles);
	sprd_timer_enable(timer_of_base(to), 0);

	return 0;
}

static int sprd_timer_set_periodic(struct clock_event_device *ce)
{
	struct timer_of *to = to_timer_of(ce);

	sprd_timer_disable(timer_of_base(to));
	sprd_timer_update_counter(timer_of_base(to), timer_of_period(to));
	sprd_timer_enable(timer_of_base(to), TIMER_CTL_PERIOD_MODE);

	return 0;
}

static int sprd_timer_shutdown(struct clock_event_device *ce)
{
	struct timer_of *to = to_timer_of(ce);

	sprd_timer_disable(timer_of_base(to));
	return 0;
}

static irqreturn_t sprd_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *ce = (struct clock_event_device *)dev_id;
	struct timer_of *to = to_timer_of(ce);

	sprd_timer_clear_interrupt(timer_of_base(to));

	if (clockevent_state_oneshot(ce))
		sprd_timer_disable(timer_of_base(to));

	ce->event_handler(ce);
	return IRQ_HANDLED;
}

static struct timer_of to = {
	.flags = TIMER_OF_IRQ | TIMER_OF_BASE | TIMER_OF_CLOCK,

	.clkevt = {
		.name = TIMER_NAME,
		.rating = 300,
		.features = CLOCK_EVT_FEAT_DYNIRQ | CLOCK_EVT_FEAT_PERIODIC |
			CLOCK_EVT_FEAT_ONESHOT,
		.set_state_shutdown = sprd_timer_shutdown,
		.set_state_periodic = sprd_timer_set_periodic,
		.set_next_event = sprd_timer_set_next_event,
		.cpumask = cpu_possible_mask,
	},

	.of_irq = {
		.handler = sprd_timer_interrupt,
		.flags = IRQF_TIMER | IRQF_IRQPOLL,
	},
};

static int sprd_timer_init(struct device_node *np)
{
	int ret;

	ret = timer_of_init(np, &to);
	if (ret)
		return ret;

	sprd_timer_enable_interrupt(timer_of_base(&to));
	clockevents_config_and_register(&to.clkevt, timer_of_rate(&to),
					1, UINT_MAX);

	return 0;
}

static struct timer_of suspend_to = {
	.flags = TIMER_OF_BASE | TIMER_OF_CLOCK,
};

static u64 sprd_suspend_timer_read(struct clocksource *cs)
{
	suspend_timer_count = ~(u64)readl_relaxed(timer_of_base(&suspend_to) +
				TIMER_VALUE_SHDW_LO) & cs->mask;
#if IS_ENABLED(CONFIG_SPRD_SYSTIMER) && IS_ENABLED(CONFIG_SPRD_TIMER_TEST)
	frt_timer_count = sprd_sysfrt_read() - frt_timer_count;
	sys_timer_count = sprd_systimer_read() - sys_timer_count;

#endif
	return suspend_timer_count;
}

static int sprd_suspend_timer_enable(struct clocksource *cs)
{
	sprd_timer_update_counter(timer_of_base(&suspend_to),
				  TIMER_VALUE_LO_MASK);
	sprd_timer_enable(timer_of_base(&suspend_to), TIMER_CTL_PERIOD_MODE);

	return 0;
}

static void sprd_suspend_timer_disable(struct clocksource *cs)
{
	sprd_timer_disable(timer_of_base(&suspend_to));
}

static struct clocksource suspend_clocksource = {
	.name	= "sprd_suspend_timer",
	.rating	= 200,
	.read	= sprd_suspend_timer_read,
	.enable = sprd_suspend_timer_enable,
	.disable = sprd_suspend_timer_disable,
	.mask	= CLOCKSOURCE_MASK(32),
	.flags	= CLOCK_SOURCE_IS_CONTINUOUS | CLOCK_SOURCE_SUSPEND_NONSTOP,
};

#if IS_ENABLED(CONFIG_PM_SLEEP) && IS_ENABLED(CONFIG_SPRD_TIMER_TEST)
static int sprd_suspend_notifier_fn(struct notifier_block *nb, unsigned long action, void *data)
{

	switch (action) {
	case PM_SUSPEND_PREPARE:

		if ((tm_start.tm_year + 1900) == 1970) {
			if (rtc)
				rtc_read_time(rtc, &tm_start);
			boottime_start = ktime_get_boot_fast_ns();
			pr_warn("rtc time wrong\n");
		}

		break;
	case PM_POST_SUSPEND:
		if (suspend_timer_count == 0)
			pr_err("suspend timer count is error\n");
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}
#endif

#if IS_ENABLED(CONFIG_PROC_FS) && IS_ENABLED(CONFIG_SPRD_TIMER_TEST)
static int sprd_suspend_time_show(struct seq_file *m, void *v)
{
	long suspend_time = suspend_timer_count * 1000000000 / 32768;
	/*long sys_time = sys_timer_count * 1000000000UL / 1000UL;*/
	long sys_time = sys_timer_count * 1000000;
	long frt_time = frt_timer_count * 1000000000 / 32768;
	time64_t time_start, time_now;
	static long suspend_time_total;

	suspend_time_total += suspend_time;

	if (rtc)
		rtc_read_time(rtc, &tm_now);
	boottime_now = ktime_get_boot_fast_ns();

	time_now   = rtc_tm_to_time64(&tm_now);
	time_start = rtc_tm_to_time64(&tm_start);

	if ((tm_start.tm_year + 1900) == 1970) {
		memset(&tm_start, 0, sizeof(struct rtc_time));
		memset(&tm_now, 0, sizeof(struct rtc_time));
		boottime_start = ktime_get_boot_fast_ns();
		boottime_now = 0;
		time_now = 0;
		time_start = 0;

		if (rtc)
			rtc_read_time(rtc, &tm_start);
	}

	seq_printf(m, "suspend time   : %ld ns, suspend time total : %ld ns\n", suspend_time,
									suspend_time_total);
	seq_printf(m, "sys time       : %ld ns\n", sys_time);
	seq_printf(m, "frt time       : %ld ns\n", frt_time);
	seq_printf(m, "boot time diff : %lld ns\n", boottime_now - boottime_start);
	seq_printf(m, "boot time start: %lld ns\n", boottime_start);
	seq_printf(m, "boot time now  : %lld ns\n", boottime_now);
	seq_printf(m, "rtc time diff  : %lld s\n", time_now - time_start);
	seq_printf(m, "rtc start time : %d-%d-%d %d:%d:%d\n", tm_start.tm_year+1900,
								tm_start.tm_mon + 1,
								tm_start.tm_mday,
								tm_start.tm_hour,
								tm_start.tm_min,
								tm_start.tm_sec);
	seq_printf(m, "rtc now   time : %d-%d-%d %d:%d:%d\n", tm_now.tm_year + 1900,
								tm_now.tm_mon + 1,
								tm_now.tm_mday,
								tm_now.tm_hour,
								tm_now.tm_min,
								tm_now.tm_sec);
	return 0;
}
#endif

static int sprd_suspend_timer_init(struct device_node *np)
{
	int ret;
#if IS_ENABLED(CONFIG_PROC_FS) && IS_ENABLED(CONFIG_SPRD_TIMER_TEST)
	static struct proc_dir_entry *suspend_timer_entry;
#endif

	ret = timer_of_init(np, &suspend_to);
	if (ret)
		return ret;

	clocksource_register_hz(&suspend_clocksource,
				timer_of_rate(&suspend_to));
#if IS_ENABLED(CONFIG_PROC_FS) && IS_ENABLED(CONFIG_SPRD_TIMER_TEST)
	suspend_timer_entry = proc_create_single("suspend_time", 0444, NULL,
						sprd_suspend_time_show);
	if (!suspend_timer_entry) {
		pr_err("Failed to create /proc/suspend_time\n");
		return -1;
	}
#endif

#if IS_ENABLED(CONFIG_PM_SLEEP) && IS_ENABLED(CONFIG_SPRD_TIMER_TEST)
	suspend_notify.notifier_call = sprd_suspend_notifier_fn;
	register_pm_notifier(&suspend_notify);

	memset(&tm_start, 0, sizeof(struct rtc_time));
	memset(&tm_now, 0, sizeof(struct rtc_time));
	rtc = alarmtimer_get_rtcdev();

	if (rtc)
		rtc_read_time(rtc, &tm_start);

	boottime_start = ktime_get_boot_fast_ns();
#endif

	return 0;
}

static const struct of_device_id sc9860_timer_match_table[] = {
	{ .compatible = "sprd,sc9860-timer",
		.data = sprd_timer_init },
	{ .compatible = "sprd,sc9860-suspend-timer",
		.data = sprd_suspend_timer_init },
	{},
};
MODULE_DEVICE_TABLE(of, sc9860_timer_match_table);

static int sprd_timer_probe(struct platform_device *pdev)
{
	int (*init_cb)(struct device_node *node);
	struct device_node *np = pdev->dev.of_node;

	init_cb = of_device_get_match_data(&pdev->dev);

	return init_cb(np);
}

static struct platform_driver sc9860_timer_driver = {
	.probe  = sprd_timer_probe,
	.driver = {
		.name = "sc9860_timer",
		.of_match_table = sc9860_timer_match_table,
	},
};
builtin_platform_driver(sc9860_timer_driver);
MODULE_DESCRIPTION("Unisoc broadcast timer module");
MODULE_LICENSE("GPL");
