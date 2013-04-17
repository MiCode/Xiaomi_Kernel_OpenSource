/*
 *  linux/arch/arm/kernel/arch_timer.c
 *
 *  Copyright (C) 2011 ARM Ltd.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/timex.h>
#include <linux/device.h>
#include <linux/smp.h>
#include <linux/cpu.h>
#include <linux/jiffies.h>
#include <linux/clockchips.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/export.h>
#include <linux/slab.h>

#include <asm/cputype.h>
#include <asm/delay.h>
#include <asm/localtimer.h>
#include <asm/arch_timer.h>
#include <asm/sched_clock.h>
#include <asm/hardware/gic.h>
#include <asm/system_info.h>

static unsigned long arch_timer_rate;
static int arch_timer_spi;
static int arch_timer_ppi;
static int arch_timer_ppi2;

static struct clock_event_device __percpu **arch_timer_evt;
static void __iomem *timer_base;

static struct delay_timer arch_delay_timer;

/*
 * Architected system timer support.
 */

#define ARCH_TIMER_CTRL_ENABLE		(1 << 0)
#define ARCH_TIMER_CTRL_IT_MASK		(1 << 1)
#define ARCH_TIMER_CTRL_IT_STAT		(1 << 2)

#define ARCH_TIMER_REG_CTRL		0
#define ARCH_TIMER_REG_FREQ		1
#define ARCH_TIMER_REG_TVAL		2

/* Iomapped Register Offsets */
#define QTIMER_CNTP_LOW_REG		0x000
#define QTIMER_CNTP_HIGH_REG		0x004
#define QTIMER_CNTV_LOW_REG		0x008
#define QTIMER_CNTV_HIGH_REG		0x00C
#define QTIMER_CTRL_REG			0x02C
#define QTIMER_FREQ_REG			0x010
#define QTIMER_CNTP_TVAL_REG		0x028
#define QTIMER_CNTV_TVAL_REG		0x038

static inline void timer_reg_write_mem(int reg, u32 val)
{
	switch (reg) {
	case ARCH_TIMER_REG_CTRL:
		__raw_writel(val, timer_base + QTIMER_CTRL_REG);
		break;
	case ARCH_TIMER_REG_TVAL:
		__raw_writel(val, timer_base + QTIMER_CNTP_TVAL_REG);
		break;
	}
}

static inline void timer_reg_write_cp15(int reg, u32 val)
{
	switch (reg) {
	case ARCH_TIMER_REG_CTRL:
		asm volatile("mcr p15, 0, %0, c14, c2, 1" : : "r" (val));
		break;
	case ARCH_TIMER_REG_TVAL:
		asm volatile("mcr p15, 0, %0, c14, c2, 0" : : "r" (val));
		break;
	}

	isb();
}

static inline void arch_timer_reg_write(int cp15, int reg, u32 val)
{
	if (cp15)
		timer_reg_write_cp15(reg, val);
	else
		timer_reg_write_mem(reg, val);
}

static inline u32 timer_reg_read_mem(int reg)
{
	u32 val;

	switch (reg) {
	case ARCH_TIMER_REG_CTRL:
		val = __raw_readl(timer_base + QTIMER_CTRL_REG);
		break;
	case ARCH_TIMER_REG_FREQ:
		val = __raw_readl(timer_base + QTIMER_FREQ_REG);
		break;
	case ARCH_TIMER_REG_TVAL:
		val = __raw_readl(timer_base + QTIMER_CNTP_TVAL_REG);
		break;
	default:
		BUG();
	}

	return val;
}

static inline u32 timer_reg_read_cp15(int reg)
{
	u32 val;

	switch (reg) {
	case ARCH_TIMER_REG_CTRL:
		asm volatile("mrc p15, 0, %0, c14, c2, 1" : "=r" (val));
		break;
	case ARCH_TIMER_REG_FREQ:
		asm volatile("mrc p15, 0, %0, c14, c0, 0" : "=r" (val));
		break;
	case ARCH_TIMER_REG_TVAL:
		asm volatile("mrc p15, 0, %0, c14, c2, 0" : "=r" (val));
		break;
	default:
		BUG();
	}

	return val;
}

static inline u32 arch_timer_reg_read(int cp15, int reg)
{
	if (cp15)
		return timer_reg_read_cp15(reg);
	else
		return timer_reg_read_mem(reg);
}

static inline irqreturn_t arch_timer_handler(int cp15,
					     struct clock_event_device *evt)
{
	unsigned long ctrl;

	ctrl = arch_timer_reg_read(cp15, ARCH_TIMER_REG_CTRL);
	if (ctrl & ARCH_TIMER_CTRL_IT_STAT) {
		ctrl |= ARCH_TIMER_CTRL_IT_MASK;
		arch_timer_reg_write(cp15, ARCH_TIMER_REG_CTRL, ctrl);
		evt->event_handler(evt);
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static irqreturn_t arch_timer_handler_cp15(int irq, void *dev_id)
{
	struct clock_event_device *evt = *(struct clock_event_device **)dev_id;
	return arch_timer_handler(1, evt);
}

static irqreturn_t arch_timer_handler_mem(int irq, void *dev_id)
{
	return arch_timer_handler(0, dev_id);
}

static inline void arch_timer_set_mode(int cp15, enum clock_event_mode mode,
				struct clock_event_device *clk)
{
	unsigned long ctrl;

	switch (mode) {
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
		ctrl = arch_timer_reg_read(cp15, ARCH_TIMER_REG_CTRL);
		ctrl &= ~ARCH_TIMER_CTRL_ENABLE;
		arch_timer_reg_write(cp15, ARCH_TIMER_REG_CTRL, ctrl);
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		ctrl = arch_timer_reg_read(cp15, ARCH_TIMER_REG_CTRL);
		ctrl |= ARCH_TIMER_CTRL_ENABLE;
		arch_timer_reg_write(cp15, ARCH_TIMER_REG_CTRL, ctrl);
	default:
		break;
	}
}

static void arch_timer_set_mode_cp15(enum clock_event_mode mode,
				struct clock_event_device *clk)
{
	arch_timer_set_mode(1, mode, clk);
}

static void arch_timer_set_mode_mem(enum clock_event_mode mode,
				struct clock_event_device *clk)
{
	arch_timer_set_mode(0, mode, clk);
}

static int arch_timer_set_next_event(int cp15, unsigned long evt,
				     struct clock_event_device *unused)
{
	unsigned long ctrl;

	ctrl = arch_timer_reg_read(cp15, ARCH_TIMER_REG_CTRL);
	ctrl &= ~ARCH_TIMER_CTRL_IT_MASK;
	arch_timer_reg_write(cp15, ARCH_TIMER_REG_CTRL, ctrl);
	arch_timer_reg_write(cp15, ARCH_TIMER_REG_TVAL, evt);

	return 0;
}

static int arch_timer_set_next_event_cp15(unsigned long evt,
				     struct clock_event_device *unused)
{
	return arch_timer_set_next_event(1, evt, unused);
}

static int arch_timer_set_next_event_mem(unsigned long evt,
				     struct clock_event_device *unused)
{
	return arch_timer_set_next_event(0, evt, unused);
}

static int __cpuinit arch_timer_setup(struct clock_event_device *clk)
{
	/* setup clock event only once for CPU 0 */
	if (!smp_processor_id() && clk->irq == arch_timer_ppi)
		return 0;

	clk->features = CLOCK_EVT_FEAT_ONESHOT | CLOCK_EVT_FEAT_C3STOP;
	clk->name = "arch_sys_timer";
	clk->rating = 450;
	clk->set_mode = arch_timer_set_mode_cp15;
	clk->set_next_event = arch_timer_set_next_event_cp15;
	clk->irq = arch_timer_ppi;

	/* Be safe... */
	clk->set_mode(CLOCK_EVT_MODE_SHUTDOWN, clk);

	clockevents_config_and_register(clk, arch_timer_rate,
					0xf, 0x7fffffff);

	*__this_cpu_ptr(arch_timer_evt) = clk;

	enable_percpu_irq(clk->irq, 0);
	if (arch_timer_ppi2)
		enable_percpu_irq(arch_timer_ppi2, 0);

	return 0;
}

/* Is the optional system timer available? */
static int local_timer_is_architected(void)
{
	return (cpu_architecture() >= CPU_ARCH_ARMv7) &&
		((read_cpuid_ext(CPUID_EXT_PFR1) >> 16) & 0xf) == 1;
}

static int arch_timer_available(void)
{
	unsigned long freq;

	if (arch_timer_rate == 0) {
		arch_timer_reg_write(1, ARCH_TIMER_REG_CTRL, 0);
		freq = arch_timer_reg_read(1, ARCH_TIMER_REG_FREQ);

		/* Check the timer frequency. */
		if (freq == 0) {
			pr_warn("Architected timer frequency not available\n");
			return -EINVAL;
		}

		arch_timer_rate = freq;
		pr_info("Architected local timer running at %lu.%02luMHz.\n",
			freq / 1000000, (freq / 10000) % 100);
	}

	return 0;
}

static inline cycle_t counter_get_cntpct_mem(void)
{
	u32 cvall, cvalh, thigh;

	do {
		cvalh = __raw_readl(timer_base + QTIMER_CNTP_HIGH_REG);
		cvall = __raw_readl(timer_base + QTIMER_CNTP_LOW_REG);
		thigh = __raw_readl(timer_base + QTIMER_CNTP_HIGH_REG);
	} while (cvalh != thigh);

	return ((cycle_t) cvalh << 32) | cvall;
}

static inline cycle_t counter_get_cntpct_cp15(void)
{
	u32 cvall, cvalh;

	asm volatile("mrrc p15, 0, %0, %1, c14" : "=r" (cvall), "=r" (cvalh));
	return ((cycle_t) cvalh << 32) | cvall;
}

static inline cycle_t counter_get_cntvct_mem(void)
{
	u32 cvall, cvalh, thigh;

	do {
		cvalh = __raw_readl(timer_base + QTIMER_CNTV_HIGH_REG);
		cvall = __raw_readl(timer_base + QTIMER_CNTV_LOW_REG);
		thigh = __raw_readl(timer_base + QTIMER_CNTV_HIGH_REG);
	} while (cvalh != thigh);

	return ((cycle_t) cvalh << 32) | cvall;
}

static inline cycle_t counter_get_cntvct_cp15(void)
{
	u32 cvall, cvalh;

	asm volatile("mrrc p15, 1, %0, %1, c14" : "=r" (cvall), "=r" (cvalh));
	return ((cycle_t) cvalh << 32) | cvall;
}

static cycle_t (*get_cntpct_func)(void) = counter_get_cntpct_cp15;
static cycle_t (*get_cntvct_func)(void) = counter_get_cntvct_cp15;

cycle_t arch_counter_get_cntpct(void)
{
	return get_cntpct_func();
}
EXPORT_SYMBOL(arch_counter_get_cntpct);

static cycle_t arch_counter_read(struct clocksource *cs)
{
	return arch_counter_get_cntpct();
}

static unsigned long arch_timer_read_current_timer(void)
{
	return arch_counter_get_cntpct();
}

static struct clocksource clocksource_counter = {
	.name	= "arch_sys_counter",
	.rating	= 400,
	.read	= arch_counter_read,
	.mask	= CLOCKSOURCE_MASK(56),
	.flags	= CLOCK_SOURCE_IS_CONTINUOUS,
};

static u32 arch_counter_get_cntvct32(void)
{
	cycle_t cntvct;

	cntvct = get_cntvct_func();

	/*
	 * The sched_clock infrastructure only knows about counters
	 * with at most 32bits. Forget about the upper 24 bits for the
	 * time being...
	 */
	return (u32)(cntvct & (u32)~0);
}

static u32 notrace arch_timer_update_sched_clock(void)
{
	return arch_counter_get_cntvct32();
}

static void __cpuinit arch_timer_stop(struct clock_event_device *clk)
{
	pr_debug("arch_timer_teardown disable IRQ%d cpu #%d\n",
		 clk->irq, smp_processor_id());
	disable_percpu_irq(clk->irq);
	if (arch_timer_ppi2)
		disable_percpu_irq(arch_timer_ppi2);
	clk->set_mode(CLOCK_EVT_MODE_UNUSED, clk);
}

static struct local_timer_ops arch_timer_ops __cpuinitdata = {
	.setup	= arch_timer_setup,
	.stop	= arch_timer_stop,
};

static struct clock_event_device arch_timer_global_evt;

static void __init arch_timer_counter_init(void)
{
	clocksource_register_hz(&clocksource_counter, arch_timer_rate);

	setup_sched_clock(arch_timer_update_sched_clock, 32, arch_timer_rate);

	/* Use the architected timer for the delay loop. */
	arch_delay_timer.read_current_timer = &arch_timer_read_current_timer;
	arch_delay_timer.freq = arch_timer_rate;
	register_current_timer_delay(&arch_delay_timer);
}

static int __init arch_timer_common_register(void)
{
	int err;

	if (!local_timer_is_architected())
		return -ENXIO;

	err = arch_timer_available();
	if (err)
		return err;

	arch_timer_evt = alloc_percpu(struct clock_event_device *);
	if (!arch_timer_evt)
		return -ENOMEM;

	err = request_percpu_irq(arch_timer_ppi, arch_timer_handler_cp15,
			 "arch_timer", arch_timer_evt);
	if (err) {
		pr_err("arch_timer: can't register interrupt %d (%d)\n",
		       arch_timer_ppi, err);
		goto out_free;
	}

	if (arch_timer_ppi2) {
		err = request_percpu_irq(arch_timer_ppi2,
				arch_timer_handler_cp15,
				"arch_timer", arch_timer_evt);
		if (err) {
			pr_err("arch_timer: can't register interrupt %d (%d)\n",
			       arch_timer_ppi2, err);
			arch_timer_ppi2 = 0;
			goto out_free_irq;
		}
	}

	err = local_timer_register(&arch_timer_ops);
	if (err) {
		/*
		 * We couldn't register as a local timer (could be
		 * because we're on a UP platform, or because some
		 * other local timer is already present...). Try as a
		 * global timer instead.
		 */
		arch_timer_global_evt.cpumask = cpumask_of(0);
		err = arch_timer_setup(&arch_timer_global_evt);
	}

	if (err)
		goto out_free_irq;

	return 0;

out_free_irq:
	free_percpu_irq(arch_timer_ppi, arch_timer_evt);
	if (arch_timer_ppi2)
		free_percpu_irq(arch_timer_ppi2, arch_timer_evt);

out_free:
	free_percpu(arch_timer_evt);

	return err;
}

static int __init arch_timer_mem_register(void)
{
	int err;
	struct clock_event_device *clk;

	clk = kzalloc(sizeof(*clk), GFP_KERNEL);
	if (!clk)
		return -ENOMEM;

	clk->features = CLOCK_EVT_FEAT_ONESHOT;
	clk->name = "arch_mem_timer";
	clk->rating = 400;
	clk->set_mode = arch_timer_set_mode_mem;
	clk->set_next_event = arch_timer_set_next_event_mem;
	clk->irq = arch_timer_spi;
	clk->cpumask = cpu_all_mask;

	clk->set_mode(CLOCK_EVT_MODE_SHUTDOWN, clk);

	clockevents_config_and_register(clk, arch_timer_rate,
					0xf, 0x7fffffff);

	err = request_irq(arch_timer_spi, arch_timer_handler_mem, 0,
		"arch_timer", clk);

	return err;
}

int __init arch_timer_register(struct arch_timer *at)
{
	if (at->res[0].start <= 0 || !(at->res[0].flags & IORESOURCE_IRQ))
		return -EINVAL;

	arch_timer_ppi = at->res[0].start;

	if (at->res[1].start > 0 && (at->res[1].flags & IORESOURCE_IRQ))
		arch_timer_ppi2 = at->res[1].start;

	if (at->res[2].start > 0 && at->res[2].end > 0 &&
					(at->res[2].flags & IORESOURCE_MEM))
		timer_base = ioremap(at->res[2].start,
				resource_size(&at->res[2]));

	if (!timer_base) {
		pr_err("arch_timer: cant map timer base\n");
		return -ENOMEM;
	}

	return arch_timer_common_register();
}

#ifdef CONFIG_OF
static const struct of_device_id arch_timer_of_match[] __initconst = {
	{ .compatible	= "arm,armv7-timer",	},
	{},
};

static const struct of_device_id arch_timer_mem_of_match[] __initconst = {
	{ .compatible	= "arm,armv7-timer-mem",	},
	{},
};

int __init arch_timer_of_register(void)
{
	struct device_node *np, *frame;
	u32 freq;
	int ret;
	int has_cp15 = false, has_mem = false;

	np = of_find_matching_node(NULL, arch_timer_of_match);
	if (np) {
		has_cp15 = true;
		/*
		 * Try to determine the frequency from the device tree
		 */
		if (!of_property_read_u32(np, "clock-frequency", &freq))
			arch_timer_rate = freq;

		ret = irq_of_parse_and_map(np, 0);
		if (ret <= 0) {
			pr_err("arch_timer: interrupt not specified in timer node\n");
			return -ENODEV;
		}
		arch_timer_ppi = ret;
		ret = irq_of_parse_and_map(np, 1);
		if (ret > 0)
			arch_timer_ppi2 = ret;

		ret = arch_timer_common_register();
		if (ret)
			return ret;
	}

	np = of_find_matching_node(NULL, arch_timer_mem_of_match);
	if (np) {
		has_mem = true;

		if (!has_cp15) {
			get_cntpct_func = counter_get_cntpct_mem;
			get_cntvct_func = counter_get_cntvct_mem;
		}
		/*
		 * Try to determine the frequency from the device tree
		 */
		if (!of_property_read_u32(np, "clock-frequency", &freq))
			arch_timer_rate = freq;

		frame = of_get_next_child(np, NULL);
		if (!frame) {
			pr_err("arch_timer: no child frame\n");
			return -EINVAL;
		}

		timer_base = of_iomap(frame, 0);
		if (!timer_base) {
			pr_err("arch_timer: cant map timer base\n");
			return -ENOMEM;
		}

		arch_timer_spi = irq_of_parse_and_map(frame, 0);
		if (!arch_timer_spi) {
			pr_err("arch_timer: no physical timer irq\n");
			return -EINVAL;
		}

		ret = arch_timer_mem_register();
		if (ret)
			return ret;
	}

	if (!has_cp15 && !has_mem) {
		pr_err("arch_timer: can't find DT node\n");
		return -ENODEV;
	}

	arch_timer_counter_init();

	return 0;
}
#endif
