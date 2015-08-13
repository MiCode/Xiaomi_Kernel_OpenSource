/*
 * Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "m4m-hwmon: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/spinlock.h>
#include "governor_cache_hwmon.h"

#define cntr_offset(idx) (sizeof(u32) * idx)

/* register offsets from base address */
#define DCVS_VERSION(m)		((m)->base + 0x0)
#define GLOBAL_CR_CTL(m)	((m)->base + 0x8)
#define GLOBAL_CR_RESET(m)	((m)->base + 0xC)
#define OVSTAT(m)		((m)->base + 0x30)
#define OVCLR(m)		((m)->base + 0x34)
#define OVSET(m)		((m)->base + 0x3C) /* unused */
#define EVCNTR(m, x)		((m)->base + 0x40 + cntr_offset(x))
#define CNTCTL(m, x)		((m)->base + 0x100 + cntr_offset(x))
/* counter 0/1 does not have type control */
#define EVTYPER_START	2
#define EVTYPER(x)	((m)->base + 0x140 + cntr_offset(x))

/* bitmasks for GLOBAL_CR_CTL and CNTCTLx */
#define CNT_EN	BIT(0)
#define IRQ_EN	BIT(1)

/* non-configurable counters */
#define CYC_CNTR_IDX		0
#define WASTED_CYC_CNTR_IDX	1

/* counter is 28-bit */
#define CNT_MAX 0x0FFFFFFFU

struct m4m_counter {
	int idx;
	u32 event_mask;
	unsigned int last_start;
};

struct m4m_hwmon {
	void __iomem *base;
	struct m4m_counter cntr[MAX_NUM_GROUPS];
	int num_cntr;
	int irq;
	struct cache_hwmon hw;
	struct device *dev;
};

#define to_mon(ptr) container_of(ptr, struct m4m_hwmon, hw)

static DEFINE_SPINLOCK(init_lock);

/* Should only be called once while HW is in POR state */
static inline void mon_global_init(struct m4m_hwmon *m)
{
	writel_relaxed(CNT_EN | IRQ_EN, GLOBAL_CR_CTL(m));
}

static inline void _mon_disable_cntr_and_irq(struct m4m_hwmon *m, int cntr_idx)
{
	writel_relaxed(0, CNTCTL(m, cntr_idx));
}

static inline void _mon_enable_cntr_and_irq(struct m4m_hwmon *m, int cntr_idx)
{
	writel_relaxed(CNT_EN | IRQ_EN, CNTCTL(m, cntr_idx));
}

static void mon_disable(struct m4m_hwmon *m)
{
	int i;

	for (i = 0; i < m->num_cntr; i++)
		_mon_disable_cntr_and_irq(m, m->cntr[i].idx);
	/* make sure all counter/irq are indeed disabled */
	mb();
}

static void mon_enable(struct m4m_hwmon *m)
{
	int i;

	for (i = 0; i < m->num_cntr; i++)
		_mon_enable_cntr_and_irq(m, m->cntr[i].idx);
}

static inline void _mon_ov_clear(struct m4m_hwmon *m, int cntr_idx)
{
	writel_relaxed(BIT(cntr_idx), OVCLR(m));
}

static void mon_ov_clear(struct m4m_hwmon *m, enum request_group grp)
{
	_mon_ov_clear(m, m->cntr[grp].idx);
}

static inline u32 mon_irq_status(struct m4m_hwmon *m)
{
	return readl_relaxed(OVSTAT(m));
}

static bool mon_is_ovstat_set(struct m4m_hwmon *m)
{
	int i;
	u32 status = mon_irq_status(m);

	for (i = 0; i < m->num_cntr; i++)
		if (status & BIT(m->cntr[i].idx))
			return true;
	return false;
}

/* counter must be stopped first */
static unsigned long _mon_get_count(struct m4m_hwmon *m,
				    int cntr_idx, unsigned int start)
{
	unsigned long cnt;
	u32 cur_cnt = readl_relaxed(EVCNTR(m, cntr_idx));
	u32 ov = readl_relaxed(OVSTAT(m)) & BIT(cntr_idx);

	if (!ov && cur_cnt < start) {
		dev_warn(m->dev, "Counter%d overflowed but not detected\n",
			 cntr_idx);
		ov = 1;
	}

	if (ov)
		cnt = CNT_MAX - start + cur_cnt;
	else
		cnt = cur_cnt - start;

	return cnt;
}

static unsigned long mon_get_count(struct m4m_hwmon *m,
				   enum request_group grp)
{
	return _mon_get_count(m, m->cntr[grp].idx, m->cntr[grp].last_start);
}

static inline void mon_set_limit(struct m4m_hwmon *m, enum request_group grp,
			  unsigned int limit)
{
	u32 start;

	if (limit >= CNT_MAX)
		limit = CNT_MAX;
	start = CNT_MAX - limit;

	writel_relaxed(start, EVCNTR(m, m->cntr[grp].idx));
	m->cntr[grp].last_start = start;
}

static inline void mon_enable_cycle_cntr(struct m4m_hwmon *m)
{
	writel_relaxed(CNT_EN, CNTCTL(m, CYC_CNTR_IDX));
}

static inline void mon_disable_cycle_cntr(struct m4m_hwmon *m)
{
	_mon_disable_cntr_and_irq(m, CYC_CNTR_IDX);
}

static inline unsigned long mon_get_cycle_count(struct m4m_hwmon *m)
{
	return _mon_get_count(m, CYC_CNTR_IDX, 0);
}

static inline void mon_clear_cycle_cntr(struct m4m_hwmon *m)
{
	writel_relaxed(0, EVCNTR(m, CYC_CNTR_IDX));
	_mon_ov_clear(m, CYC_CNTR_IDX);
}

static void mon_init(struct m4m_hwmon *m)
{
	static bool mon_inited;
	unsigned long flags;
	int i;

	spin_lock_irqsave(&init_lock, flags);
	if (!mon_inited)
		mon_global_init(m);
	spin_unlock_irqrestore(&init_lock, flags);

	/* configure counter events */
	for (i = 0; i < m->num_cntr; i++)
		writel_relaxed(m->cntr[i].event_mask, EVTYPER(m->cntr[i].idx));
}

static irqreturn_t m4m_hwmon_intr_handler(int irq, void *dev)
{
	struct m4m_hwmon *m = dev;

	if (mon_is_ovstat_set(m)) {
		update_cache_hwmon(&m->hw);
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}

static int count_to_mrps(unsigned long count, unsigned int us)
{
	do_div(count, us);
	count++;
	return count;
}

static unsigned int mrps_to_count(unsigned int mrps, unsigned int ms,
				  unsigned int tolerance)
{
	mrps += tolerance;
	mrps *= ms * USEC_PER_MSEC;
	return mrps;
}

static unsigned long m4m_meas_mrps_and_set_irq(struct cache_hwmon *hw,
		unsigned int tol, unsigned int us, struct mrps_stats *mrps)
{
	struct m4m_hwmon *m = to_mon(hw);
	unsigned long count, cyc_count;
	unsigned long f = hw->df->previous_freq;
	unsigned int sample_ms = hw->df->profile->polling_ms;
	int i;
	u32 limit;

	mon_disable(m);
	mon_disable_cycle_cntr(m);

	/* calculate mrps and set limit */
	for (i = 0; i < m->num_cntr; i++) {
		count = mon_get_count(m, i);
		mrps->mrps[i] = count_to_mrps(count, us);
		limit = mrps_to_count(mrps->mrps[i], sample_ms, tol);
		mon_ov_clear(m, i);
		mon_set_limit(m, i, limit);
		dev_dbg(m->dev, "Counter[%d] count 0x%lx, limit 0x%x\n",
			m->cntr[i].idx, count, limit);
	}

	/* get cycle count and calculate busy percent */
	cyc_count = mon_get_cycle_count(m);
	mrps->busy_percent = mult_frac(cyc_count, 1000, us) * 100 / f;
	mon_clear_cycle_cntr(m);
	dev_dbg(m->dev, "Cycle count 0x%lx\n", cyc_count);

	/* re-enable monitor */
	mon_enable(m);
	mon_enable_cycle_cntr(m);

	return 0;
}

static int m4m_start_hwmon(struct cache_hwmon *hw, struct mrps_stats *mrps)
{
	struct m4m_hwmon *m = to_mon(hw);
	unsigned int sample_ms = hw->df->profile->polling_ms;
	int ret, i;
	u32 limit;

	ret = request_threaded_irq(m->irq, NULL, m4m_hwmon_intr_handler,
				  IRQF_ONESHOT | IRQF_SHARED,
				  dev_name(m->dev), m);
	if (ret) {
		dev_err(m->dev, "Unable to register for irq\n");
		return ret;
	}

	mon_init(m);
	mon_disable(m);
	mon_disable_cycle_cntr(m);
	for (i = 0; i < m->num_cntr; i++) {
		mon_ov_clear(m, i);
		limit = mrps_to_count(mrps->mrps[i], sample_ms, 0);
		mon_set_limit(m, i, limit);
	}
	mon_clear_cycle_cntr(m);
	mon_enable(m);
	mon_enable_cycle_cntr(m);

	return 0;
}

static void m4m_stop_hwmon(struct cache_hwmon *hw)
{
	struct m4m_hwmon *m = to_mon(hw);
	int i;

	mon_disable(m);
	free_irq(m->irq, m);
	for (i = 0; i < m->num_cntr; i++)
		mon_ov_clear(m, i);
}

/* device probe functions */
static struct of_device_id match_table[] = {
	{ .compatible = "qcom,m4m-hwmon" },
	{}
};

static int m4m_hwmon_parse_cntr(struct device *dev,
				struct m4m_hwmon *m)
{
	u32 *data;
	const char *prop_name = "qcom,counter-event-sel";
	int ret, len, i;

	if (!of_find_property(dev->of_node, prop_name, &len))
		return -EINVAL;
	len /= sizeof(*data);

	if (len % 2 || len > MAX_NUM_GROUPS * 2)
		return -EINVAL;

	data = devm_kcalloc(dev, len, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	ret = of_property_read_u32_array(dev->of_node, prop_name, data, len);
	if (ret)
		return ret;

	len /= 2;
	m->num_cntr = len;
	for (i = 0; i < len; i++) {
		/* disallow non-configurable counters */
		if (data[i * 2] < EVTYPER_START)
			return -EINVAL;
		m->cntr[i].idx = data[i * 2];
		m->cntr[i].event_mask = data[i * 2 + 1];
	}

	devm_kfree(dev, data);
	return 0;
}

static int m4m_hwmon_driver_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct m4m_hwmon *m;
	int ret;

	m = devm_kzalloc(dev, sizeof(*m), GFP_KERNEL);
	if (!m)
		return -ENOMEM;
	m->dev = dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "base not found!\n");
		return -EINVAL;
	}
	m->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!m->base)
		return -ENOMEM;

	m->irq = platform_get_irq(pdev, 0);
	if (m->irq < 0) {
		dev_err(dev, "Unable to get IRQ number\n");
		return m->irq;
	}

	ret = m4m_hwmon_parse_cntr(dev, m);
	if (ret) {
		dev_err(dev, "Unable to parse counter events\n");
		return ret;
	}

	m->hw.of_node = of_parse_phandle(dev->of_node, "qcom,target-dev", 0);
	if (!m->hw.of_node)
		return -EINVAL;
	m->hw.start_hwmon = &m4m_start_hwmon;
	m->hw.stop_hwmon = &m4m_stop_hwmon;
	m->hw.meas_mrps_and_set_irq = &m4m_meas_mrps_and_set_irq;

	ret = register_cache_hwmon(dev, &m->hw);
	if (ret) {
		dev_err(dev, "Dev BW hwmon registration failed\n");
		return ret;
	}

	return 0;
}

static struct platform_driver m4m_hwmon_driver = {
	.probe = m4m_hwmon_driver_probe,
	.driver = {
		.name = "m4m-hwmon",
		.of_match_table = match_table,
		.owner = THIS_MODULE,
	},
};

static int __init m4m_hwmon_init(void)
{
	return platform_driver_register(&m4m_hwmon_driver);
}
module_init(m4m_hwmon_init);

static void __exit m4m_hwmon_exit(void)
{
	platform_driver_unregister(&m4m_hwmon_driver);
}
module_exit(m4m_hwmon_exit);

MODULE_DESCRIPTION("M4M hardware monitor driver");
MODULE_LICENSE("GPL v2");
