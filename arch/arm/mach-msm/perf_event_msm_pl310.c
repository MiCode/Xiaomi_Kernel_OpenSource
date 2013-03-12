/*
 * Copyright (C) 2007 ARM Limited
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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


#include <linux/irq.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>

#include <asm/pmu.h>
#include <asm/hardware/cache-l2x0.h>
#include <mach/socinfo.h>

static u32 rev1;

/*
 * Store dynamic PMU type after registration,
 * to uniquely identify this PMU at runtime.
 */
static u32 pmu_type;

/* This controller only supports 16 Events.*/
PMU_FORMAT_ATTR(l2_config, "config:0-4");

static struct attribute *arm_l2_ev_formats[] = {
	&format_attr_l2_config.attr,
	NULL,
};

/*
 * Format group is essential to access PMU's from userspace
 * via their .name field.
 */
static struct attribute_group arm_l2_pmu_format_group = {
	.name = "format",
	.attrs = arm_l2_ev_formats,
};

static const struct attribute_group *arm_l2_pmu_attr_grps[] = {
	&arm_l2_pmu_format_group,
	NULL,
};

#define L2X0_AUX_CTRL_EVENT_MONITOR_SHIFT	20
#define L2X0_INTR_MASK_ECNTR		1

/* L220/PL310 Event control register values */
#define L2X0_EVENT_CNT_ENABLE_MASK		1
#define L2X0_EVENT_CNT_ENABLE			1
#define L2X0_EVENT_CNT_RESET(x)			(1 << (x+1))

/* Bit-shifted event counter config values */
enum l2x0_perf_types {
	L2X0_EVENT_CNT_CFG_DISABLED		= 0x0,
	L2X0_EVENT_CNT_CFG_CO			= 0x1,
	L2X0_EVENT_CNT_CFG_DRHIT		= 0x2,
	L2X0_EVENT_CNT_CFG_DRREQ		= 0x3,
	L2X0_EVENT_CNT_CFG_DWHIT		= 0x4,
	L2X0_EVENT_CNT_CFG_DWREQ		= 0x5,
	L2X0_EVENT_CNT_CFG_DWTREQ		= 0x6,
	L2X0_EVENT_CNT_CFG_IRHIT		= 0x7,
	L2X0_EVENT_CNT_CFG_IRREQ		= 0x8,
	L2X0_EVENT_CNT_CFG_WA			= 0x9,

	/* PL310 only */
	L2X0_EVENT_CNT_CFG_IPFALLOC		= 0xA,
	L2X0_EVENT_CNT_CFG_EPFHIT		= 0xB,
	L2X0_EVENT_CNT_CFG_EPFALLOC		= 0xC,
	L2X0_EVENT_CNT_CFG_SRRCVD		= 0xD,
	L2X0_EVENT_CNT_CFG_SRCONF		= 0xE,
	L2X0_EVENT_CNT_CFG_EPFRCVD		= 0xF,
};

#define PL310_EVENT_CNT_CFG_MAX			L2X0_EVENT_CNT_CFG_EPFRCVD

#define L2X0_EVENT_CNT_CFG_SHIFT		2
#define L2X0_EVENT_CNT_CFG_MASK			(0xF << 2)

#define L2X0_EVENT_CNT_CFG_INTR_MASK		0x3
#define L2X0_EVENT_CNT_CFG_INTR_DISABLED	0x0
#define L2X0_EVENT_CNT_CFG_INTR_INCREMENT	0x1
#define L2X0_EVENT_CNT_CFG_INTR_OVERFLOW	0x2

#define L2X0_NUM_COUNTERS			2
static struct arm_pmu l2x0_pmu;

static u32 l2x0pmu_max_event_id = 0xf;

static struct perf_event *events[2];
static unsigned long used_mask[BITS_TO_LONGS(2)];
static struct pmu_hw_events l2x0pmu_hw_events = {
	.events = events,
	.used_mask = used_mask,
	.pmu_lock = __RAW_SPIN_LOCK_UNLOCKED(l2x0pmu_hw_events.pmu_lock),
};

#define COUNTER_CFG_ADDR(idx)	(l2x0_base + L2X0_EVENT_CNT0_CFG - 4*idx)

#define COUNTER_CTRL_ADDR	(l2x0_base + L2X0_EVENT_CNT_CTRL)

#define COUNTER_ADDR(idx)	(l2x0_base + L2X0_EVENT_CNT0_VAL - 4*idx)

static u32 l2x0_read_intr_mask(void)
{
	return readl_relaxed(l2x0_base + L2X0_INTR_MASK);
}

static void l2x0_write_intr_mask(u32 val)
{
	writel_relaxed(val, l2x0_base + L2X0_INTR_MASK);
}

static void l2x0_enable_counter_interrupt(void)
{
	u32 intr_mask = l2x0_read_intr_mask();
	intr_mask |= L2X0_INTR_MASK_ECNTR;
	l2x0_write_intr_mask(intr_mask);
}

static void l2x0_disable_counter_interrupt(void)
{
	u32 intr_mask = l2x0_read_intr_mask();
	intr_mask &= ~L2X0_INTR_MASK_ECNTR;
	l2x0_write_intr_mask(intr_mask);
}

static void l2x0_clear_interrupts(u32 flags)
{
	writel_relaxed(flags, l2x0_base + L2X0_INTR_CLEAR);
}

static struct pmu_hw_events *l2x0pmu_get_hw_events(void)
{
	return &l2x0pmu_hw_events;
}

static u32 l2x0pmu_read_ctrl(void)
{
	return readl_relaxed(COUNTER_CTRL_ADDR);
}

static void l2x0pmu_write_ctrl(u32 val)
{
	writel_relaxed(val, COUNTER_CTRL_ADDR);
}

static u32 l2x0pmu_read_cfg(int idx)
{
	return readl_relaxed(COUNTER_CFG_ADDR(idx));
}

static void l2x0pmu_write_cfg(u32 val, int idx)
{
	writel_relaxed(val, COUNTER_CFG_ADDR(idx));
}

static void l2x0pmu_enable_counter(u32 cfg, int idx)
{
	cfg |= L2X0_EVENT_CNT_CFG_INTR_OVERFLOW;
	l2x0pmu_write_cfg(cfg, idx);
}

static u32 l2x0pmu_disable_counter(int idx)
{
	u32 cfg, oldcfg;

	cfg = oldcfg = l2x0pmu_read_cfg(idx);

	cfg &= ~L2X0_EVENT_CNT_CFG_MASK;
	cfg &= ~L2X0_EVENT_CNT_CFG_INTR_MASK;
	l2x0pmu_write_cfg(cfg, idx);

	return oldcfg;
}

static u32 l2x0pmu_read_counter(int idx)
{
	u32 val = readl_relaxed(COUNTER_ADDR(idx));

	return val;
}

static void l2x0pmu_write_counter(int idx, u32 val)
{
	/*
	 * L2X0 counters can only be written to when they are disabled.
	 * As perf core does not disable counters before writing to them
	 * under interrupts, we must do so here.
	 */
	u32 cfg = l2x0pmu_disable_counter(idx);
	writel_relaxed(val, COUNTER_ADDR(idx));
	l2x0pmu_write_cfg(cfg, idx);
}

static int counter_is_saturated(int idx)
{
	return l2x0pmu_read_counter(idx) == 0xFFFFFFFF;
}

static void l2x0pmu_start(void)
{
	unsigned long flags;
	u32 val;

	raw_spin_lock_irqsave(&l2x0pmu_hw_events.pmu_lock, flags);

	if (!rev1)
		l2x0_enable_counter_interrupt();

	val = l2x0pmu_read_ctrl();

	val |= L2X0_EVENT_CNT_ENABLE;
	l2x0pmu_write_ctrl(val);

	raw_spin_unlock_irqrestore(&l2x0pmu_hw_events.pmu_lock, flags);
}

static void l2x0pmu_stop(void)
{
	unsigned long flags;
	u32 val;

	raw_spin_lock_irqsave(&l2x0pmu_hw_events.pmu_lock, flags);

	val = l2x0pmu_read_ctrl();
	val &= ~L2X0_EVENT_CNT_ENABLE_MASK;
	l2x0pmu_write_ctrl(val);

	if (!rev1)
		l2x0_disable_counter_interrupt();

	raw_spin_unlock_irqrestore(&l2x0pmu_hw_events.pmu_lock, flags);
}

static void l2x0pmu_enable(struct hw_perf_event *event, int idx, int cpu)
{
	unsigned long flags;
	u32 cfg;

	raw_spin_lock_irqsave(&l2x0pmu_hw_events.pmu_lock, flags);

	cfg = (event->config_base << L2X0_EVENT_CNT_CFG_SHIFT) &
						L2X0_EVENT_CNT_CFG_MASK;
	l2x0pmu_enable_counter(cfg, idx);

	raw_spin_unlock_irqrestore(&l2x0pmu_hw_events.pmu_lock, flags);
}

static void l2x0pmu_disable(struct hw_perf_event *event, int idx)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&l2x0pmu_hw_events.pmu_lock, flags);
	l2x0pmu_disable_counter(idx);
	raw_spin_unlock_irqrestore(&l2x0pmu_hw_events.pmu_lock, flags);
}

static int l2x0pmu_get_event_idx(struct pmu_hw_events *events,
					struct hw_perf_event *hwc)
{
	int idx;

	/* Counters are identical. Just grab a free one. */
	for (idx = 0; idx < L2X0_NUM_COUNTERS; ++idx) {
		if (!test_and_set_bit(idx, l2x0pmu_hw_events.used_mask))
			return idx;
	}

	return -EAGAIN;
}

/*
 * As System PMUs are affine to CPU0, the fact that interrupts are disabled
 * during interrupt handling is enough to serialise our actions and make this
 * safe. We do not need to grab our pmu_lock here.
 */
static irqreturn_t l2x0pmu_handle_irq(int irq, void *dev)
{
	irqreturn_t status = IRQ_NONE;
	struct perf_sample_data data;
	struct pt_regs *regs;
	int idx;

	regs = get_irq_regs();

	for (idx = 0; idx < L2X0_NUM_COUNTERS; ++idx) {
		struct perf_event *event = l2x0pmu_hw_events.events[idx];
		struct hw_perf_event *hwc;

		if (!counter_is_saturated(idx))
			continue;

		status = IRQ_HANDLED;

		hwc = &event->hw;

		/*
		 * The armpmu_* functions expect counters to overflow, but
		 * L220/PL310 counters saturate instead. Fake the overflow
		 * here so the hardware is in sync with what the framework
		 * expects.
		 */
		l2x0pmu_write_counter(idx, 0);

		armpmu_event_update(event, hwc, idx);
		data.period = event->hw.last_period;

		if (!armpmu_event_set_period(event, hwc, idx))
			continue;

		if (perf_event_overflow(event, &data, regs))
			l2x0pmu_disable_counter(idx);
	}

	l2x0_clear_interrupts(L2X0_INTR_MASK_ECNTR);

	irq_work_run();

	return status;
}

static int map_l2x0_raw_event(u64 config)
{
	return (config <= l2x0pmu_max_event_id) ? config : -ENOENT;
}

static int l2x0pmu_map_event(struct perf_event *event)
{
	u64 config = event->attr.config;
	u64 supported_samples = (PERF_SAMPLE_TIME	|
				 PERF_SAMPLE_ID		|
				 PERF_SAMPLE_PERIOD	|
				 PERF_SAMPLE_STREAM_ID	|
				 PERF_SAMPLE_RAW);

	if ((pmu_type == 0) || (pmu_type != event->attr.type))
		return -ENOENT;

	if (event->attr.sample_type & ~supported_samples)
		return -ENOENT;

	return map_l2x0_raw_event(config);
}

static int
arm_l2_pmu_generic_request_irq(int irq, irq_handler_t *handle_irq)
{
	return request_irq(irq, *handle_irq,
			IRQF_DISABLED | IRQF_NOBALANCING,
			"arm-l2-armpmu", NULL);
}

static void
arm_l2_pmu_generic_free_irq(int irq)
{
	if (irq >= 0)
		free_irq(irq, NULL);
}

static struct arm_pmu l2x0_pmu = {
	.id		= ARM_PERF_PMU_ID_L2X0,
	.type		= ARM_PMU_DEVICE_L2CC,
	.name		= "msm-l2",
	.start		= l2x0pmu_start,
	.stop		= l2x0pmu_stop,
	.handle_irq	= l2x0pmu_handle_irq,
	.enable		= l2x0pmu_enable,
	.disable	= l2x0pmu_disable,
	.get_event_idx	= l2x0pmu_get_event_idx,
	.read_counter	= l2x0pmu_read_counter,
	.write_counter	= l2x0pmu_write_counter,
	.map_event	= l2x0pmu_map_event,
	.num_events	= 2,
	.max_period	= 0xFFFFFFFF,
	.get_hw_events	= l2x0pmu_get_hw_events,
	.pmu.attr_groups = arm_l2_pmu_attr_grps,
	.request_pmu_irq = arm_l2_pmu_generic_request_irq,
	.free_pmu_irq	= arm_l2_pmu_generic_free_irq,
};

static int __devinit l2x0pmu_device_probe(struct platform_device *pdev)
{
	u32 aux = readl_relaxed(l2x0_base + L2X0_AUX_CTRL);
	u32 debug = readl_relaxed(l2x0_base + L2X0_DEBUG_CTRL);
	l2x0_pmu.plat_device = pdev;

	if (!(aux & (1 << L2X0_AUX_CTRL_EVENT_MONITOR_SHIFT))) {
		pr_err("Ev Monitor is OFF. L2 counters disabled.\n");
		return -EOPNOTSUPP;
	}

	pr_info("L2CC PMU device found. DEBUG_CTRL: %x\n", debug);

	/* Get value of dynamically allocated PMU type. */
	if (!armpmu_register(&l2x0_pmu, "msm-l2", -1))
		pmu_type = l2x0_pmu.pmu.type;
	else {
		pr_err("l2x0_pmu registration failed\n");
		return -EOPNOTSUPP;
	}

	return 0;
}

/*
 * PMU platform driver and devicetree bindings.
 */
static struct of_device_id l2pmu_of_device_ids[] = {
	{.compatible = "qcom,l2-pmu"},
	{},
};

static struct platform_driver l2x0pmu_driver = {
	.driver		= {
		.name	= "l2-pmu",
		.of_match_table = l2pmu_of_device_ids,
	},
	.probe		= l2x0pmu_device_probe,
};

static int __init register_pmu_driver(void)
{
	if (machine_is_msm9625())
		rev1 = 1;

	return platform_driver_register(&l2x0pmu_driver);
}
device_initcall(register_pmu_driver);
