/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "bimc-bwmon: " fmt

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
#include <linux/spinlock.h>
#include "governor_bw_hwmon.h"

#define GLB_INT_STATUS(m)	((m)->global_base + 0x100)
#define GLB_INT_CLR(m)		((m)->global_base + 0x108)
#define	GLB_INT_EN(m)		((m)->global_base + 0x10C)
#define MON_INT_STATUS(m)	((m)->base + 0x100)
#define MON_INT_CLR(m)		((m)->base + 0x108)
#define	MON_INT_EN(m)		((m)->base + 0x10C)
#define	MON_EN(m)		((m)->base + 0x280)
#define MON_CLEAR(m)		((m)->base + 0x284)
#define MON_CNT(m)		((m)->base + 0x288)
#define MON_THRES(m)		((m)->base + 0x290)
#define MON_MASK(m)		((m)->base + 0x298)
#define MON_MATCH(m)		((m)->base + 0x29C)

struct bwmon {
	void __iomem *base;
	void __iomem *global_base;
	unsigned int mport;
	unsigned int irq;
	struct device *dev;
	struct bw_hwmon hw;
};

#define to_bwmon(ptr)		container_of(ptr, struct bwmon, hw)

static DEFINE_SPINLOCK(glb_lock);
static void mon_enable(struct bwmon *m)
{
	writel_relaxed(0x1, MON_EN(m));
}

static void mon_disable(struct bwmon *m)
{
	writel_relaxed(0x0, MON_EN(m));
}

static void mon_clear(struct bwmon *m)
{
	writel_relaxed(0x1, MON_CLEAR(m));
}

static void mon_irq_enable(struct bwmon *m)
{
	u32 val;

	spin_lock(&glb_lock);
	val = readl_relaxed(GLB_INT_EN(m));
	val |= 1 << m->mport;
	writel_relaxed(val, GLB_INT_EN(m));
	spin_unlock(&glb_lock);

	val = readl_relaxed(MON_INT_EN(m));
	val |= 0x1;
	writel_relaxed(val, MON_INT_EN(m));
}

static void mon_irq_disable(struct bwmon *m)
{
	u32 val;

	spin_lock(&glb_lock);
	val = readl_relaxed(GLB_INT_EN(m));
	val &= ~(1 << m->mport);
	writel_relaxed(val, GLB_INT_EN(m));
	spin_unlock(&glb_lock);

	val = readl_relaxed(MON_INT_EN(m));
	val &= ~0x1;
	writel_relaxed(val, MON_INT_EN(m));
}

static int mon_irq_status(struct bwmon *m)
{
	return readl_relaxed(MON_INT_STATUS(m)) & 0x1;
}

static void mon_irq_clear(struct bwmon *m)
{
	writel_relaxed(1 << m->mport, GLB_INT_CLR(m));
	writel_relaxed(0x1, MON_INT_CLR(m));
}

static void mon_set_limit(struct bwmon *m, u32 count)
{
	writel_relaxed(count, MON_THRES(m));
	dev_dbg(m->dev, "Thres: %08x\n", count);
}

static u32 mon_get_limit(struct bwmon *m)
{
	return readl_relaxed(MON_THRES(m));
}

static long mon_get_count(struct bwmon *m)
{
	long count;

	count = readl_relaxed(MON_CNT(m));
	if (mon_irq_status(m))
		count += mon_get_limit(m);
	dev_dbg(m->dev, "Count: %ld\n", count);

	return count;
}

/* ********** CPUBW specific code  ********** */

/* Returns MBps of read/writes for the sampling window. */
static unsigned int bytes_to_mbps(long long bytes, unsigned int us)
{
	bytes *= USEC_PER_SEC;
	do_div(bytes, us);
	bytes = DIV_ROUND_UP_ULL(bytes, SZ_1M);
	return bytes;
}

static unsigned int mbps_to_bytes(unsigned long mbps, unsigned int ms,
				  unsigned int tolerance_percent)
{
	mbps *= (100 + tolerance_percent) * ms;
	mbps /= 100;
	mbps = DIV_ROUND_UP(mbps, MSEC_PER_SEC);
	mbps *= SZ_1M;
	return mbps;
}

static unsigned long meas_bw_and_set_irq(struct bw_hwmon *hw,
					 unsigned int tol, unsigned int us)
{
	unsigned long mbps;
	u32 limit;
	unsigned int sample_ms = hw->df->profile->polling_ms;
	struct bwmon *m = to_bwmon(hw);

	mon_disable(m);

	mbps = mon_get_count(m);
	mbps = bytes_to_mbps(mbps, us);
	/* + 1024 is to workaround HW design issue. Needs further tuning. */
	limit = mbps_to_bytes(mbps + 1024, sample_ms, tol);
	mon_set_limit(m, limit);

	mon_clear(m);
	mon_irq_clear(m);
	mon_enable(m);

	dev_dbg(m->dev, "MBps = %lu\n", mbps);
	return mbps;
}

static irqreturn_t bwmon_intr_handler(int irq, void *dev)
{
	struct bwmon *m = dev;
	if (mon_irq_status(m)) {
		update_bw_hwmon(&m->hw);
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static int start_bw_hwmon(struct bw_hwmon *hw, unsigned long mbps)
{
	struct bwmon *m = to_bwmon(hw);
	u32 limit;
	int ret;

	ret = request_threaded_irq(m->irq, NULL, bwmon_intr_handler,
				  IRQF_ONESHOT | IRQF_SHARED,
				  dev_name(m->dev), m);
	if (ret) {
		dev_err(m->dev, "Unable to register interrupt handler!\n");
		return ret;
	}

	mon_disable(m);

	limit = mbps_to_bytes(mbps, hw->df->profile->polling_ms, 0);
	mon_set_limit(m, limit);

	mon_clear(m);
	mon_irq_clear(m);
	mon_irq_enable(m);
	mon_enable(m);

	return 0;
}

static void stop_bw_hwmon(struct bw_hwmon *hw)
{
	struct bwmon *m = to_bwmon(hw);

	disable_irq(m->irq);
	free_irq(m->irq, m);
	mon_disable(m);
	mon_irq_disable(m);
	mon_irq_clear(m);
	mon_clear(m);
}

/*************************************************************************/

static int bimc_bwmon_driver_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct bwmon *m;
	int ret;
	u32 data;

	m = devm_kzalloc(dev, sizeof(*m), GFP_KERNEL);
	if (!m)
		return -ENOMEM;
	m->dev = dev;

	ret = of_property_read_u32(dev->of_node, "qcom,mport", &data);
	if (ret) {
		dev_err(dev, "mport not found!\n");
		return ret;
	}
	m->mport = data;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "base");
	if (!res) {
		dev_err(dev, "base not found!\n");
		return -EINVAL;
	}
	m->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!m->base) {
		dev_err(dev, "Unable map base!\n");
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "global_base");
	if (!res) {
		dev_err(dev, "global_base not found!\n");
		return -EINVAL;
	}
	m->global_base = devm_ioremap(dev, res->start, resource_size(res));
	if (!m->global_base) {
		dev_err(dev, "Unable map global_base!\n");
		return -ENOMEM;
	}

	m->irq = platform_get_irq(pdev, 0);
	if (m->irq < 0) {
		dev_err(dev, "Unable to get IRQ number\n");
		return m->irq;
	}

	m->hw.of_node = of_parse_phandle(dev->of_node, "qcom,target-dev", 0);
	if (!m->hw.of_node)
		return -EINVAL;
	m->hw.start_hwmon = &start_bw_hwmon,
	m->hw.stop_hwmon = &stop_bw_hwmon,
	m->hw.meas_bw_and_set_irq = &meas_bw_and_set_irq,

	ret = register_bw_hwmon(dev, &m->hw);
	if (ret) {
		dev_err(dev, "Dev BW hwmon registration failed\n");
		return ret;
	}

	return 0;
}

static struct of_device_id match_table[] = {
	{ .compatible = "qcom,bimc-bwmon" },
	{}
};

static struct platform_driver bimc_bwmon_driver = {
	.probe = bimc_bwmon_driver_probe,
	.driver = {
		.name = "bimc-bwmon",
		.of_match_table = match_table,
		.owner = THIS_MODULE,
	},
};

static int __init bimc_bwmon_init(void)
{
	return platform_driver_register(&bimc_bwmon_driver);
}
module_init(bimc_bwmon_init);

static void __exit bimc_bwmon_exit(void)
{
	platform_driver_unregister(&bimc_bwmon_driver);
}
module_exit(bimc_bwmon_exit);

MODULE_DESCRIPTION("BIMC bandwidth monitor driver");
MODULE_LICENSE("GPL v2");
