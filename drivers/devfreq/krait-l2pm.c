/*
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "krait-l2pm: " fmt

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
#include "governor_cpubw_hwmon.h"

#include <mach/msm-krait-l2-accessors.h>

#define L2PMRESR(n)		(0x410 + n)
#define L2PMCR			0x400
#define L2PMCNTENCLR		0x402
#define L2PMCNTENSET		0x403
#define L2PMINTENCLR		0x404
#define L2PMINTENSET		0x405
#define L2PMOVSR		0x406
#define L2PMOVSSET		0x407
#define L2PMnEVCNTCR(n)		(0x420 + n * 0x10)
#define L2PMnEVCNTR(n)		(0x421 + n * 0x10)
#define L2PMnEVCNTSR(n)		(0x422 + n * 0x10)
#define L2PMnEVFILTER(n)	(0x423 + n * 0x10)
#define L2PMnEVTYPER(n)		(0x424 + n * 0x10)

#define RD_MON	0
#define WR_MON	1
static void mon_init(void)
{
	/* Set up counters 0/1 to count write/read beats */
	set_l2_indirect_reg(L2PMRESR(2), 0x8B0B0000);
	set_l2_indirect_reg(L2PMnEVCNTCR(RD_MON), 0x0);
	set_l2_indirect_reg(L2PMnEVCNTCR(WR_MON), 0x0);
	set_l2_indirect_reg(L2PMnEVCNTR(RD_MON), 0xFFFFFFFF);
	set_l2_indirect_reg(L2PMnEVCNTR(WR_MON), 0xFFFFFFFF);
	set_l2_indirect_reg(L2PMnEVFILTER(RD_MON), 0xF003F);
	set_l2_indirect_reg(L2PMnEVFILTER(WR_MON), 0xF003F);
	set_l2_indirect_reg(L2PMnEVTYPER(RD_MON), 0xA);
	set_l2_indirect_reg(L2PMnEVTYPER(WR_MON), 0xB);
}

static void global_mon_enable(bool en)
{
	u32 regval;

	/* Global counter enable */
	regval = get_l2_indirect_reg(L2PMCR);
	if (en)
		regval |= BIT(0);
	else
		regval &= ~BIT(0);
	set_l2_indirect_reg(L2PMCR, regval);
}

static void mon_enable(int n)
{
	/* Clear previous overflow state for event counter n */
	set_l2_indirect_reg(L2PMOVSR, BIT(n));

	/* Enable event counter n */
	set_l2_indirect_reg(L2PMCNTENSET, BIT(n));
}

static void mon_disable(int n)
{
	/* Disable event counter n */
	set_l2_indirect_reg(L2PMCNTENCLR, BIT(n));
}

static void mon_irq_enable(int n, bool en)
{
	if (en)
		set_l2_indirect_reg(L2PMINTENSET, BIT(n));
	else
		set_l2_indirect_reg(L2PMINTENCLR, BIT(n));
}

static int mon_overflow(int n)
{
	return get_l2_indirect_reg(L2PMOVSR) & BIT(n);
}

/* Returns start counter value to be used with mon_get_count() */
static u32 mon_set_limit(int n, u32 count)
{
	u32 regval;

	regval = 0xFFFFFFFF - count;
	set_l2_indirect_reg(L2PMnEVCNTR(n), regval);
	pr_debug("EV%d start val: %x\n", n, regval);

	return regval;
}

static long mon_get_count(int n, u32 start_val)
{
	u32 overflow, count;

	count = get_l2_indirect_reg(L2PMnEVCNTR(n));
	overflow = get_l2_indirect_reg(L2PMOVSR);

	pr_debug("EV%d ov: %x, cnt: %x\n", n, overflow, count);

	if (overflow & BIT(n))
		return 0xFFFFFFFF - start_val + count;
	else
		return count - start_val;
}

static u32 bytes_per_beat;
static u32 prev_r_start_val;
static u32 prev_w_start_val;

/* Returns MBps of read/writes for the sampling window. */
static unsigned int beats_to_mbps(long long beats, unsigned int us)
{
	beats *= USEC_PER_SEC;
	beats *= bytes_per_beat;
	do_div(beats, us);
	beats = DIV_ROUND_UP_ULL(beats, SZ_1M);

	return beats;
}

static unsigned int mbps_to_beats(unsigned long mbps, unsigned int ms,
				  unsigned int tolerance_percent)
{
	mbps *= (100 + tolerance_percent) * ms;
	mbps /= 100;
	mbps = DIV_ROUND_UP(mbps, MSEC_PER_SEC);
	mbps = mult_frac(mbps, SZ_1M, bytes_per_beat);
	return mbps;
}

static unsigned long meas_bw_and_set_irq(struct devfreq *df, unsigned int tol,
					 unsigned int us)
{
	unsigned long r_mbps, w_mbps;
	u32 r_limit, w_limit;
	unsigned int sample_ms = df->profile->polling_ms;

	mon_disable(RD_MON);
	mon_disable(WR_MON);

	r_mbps = mon_get_count(RD_MON, prev_r_start_val);
	r_mbps = beats_to_mbps(r_mbps, us);
	w_mbps = mon_get_count(WR_MON, prev_w_start_val);
	w_mbps = beats_to_mbps(w_mbps, us);

	r_limit = mbps_to_beats(r_mbps, sample_ms, tol);
	w_limit = mbps_to_beats(w_mbps, sample_ms, tol);

	prev_r_start_val = mon_set_limit(RD_MON, r_limit);
	prev_w_start_val = mon_set_limit(WR_MON, w_limit);

	mon_enable(RD_MON);
	mon_enable(WR_MON);

	pr_debug("R/W = %ld/%ld\n", r_mbps, w_mbps);

	return r_mbps + w_mbps;
}

static bool is_valid_bw_irq(struct devfreq *df)
{
	return mon_overflow(RD_MON) || mon_overflow(WR_MON);
}

static int start_bw_hwmon(struct devfreq *df, unsigned long mbps)
{
	u32 limit;

	mon_init();
	mon_disable(RD_MON);
	mon_disable(WR_MON);

	limit = mbps_to_beats(mbps, df->profile->polling_ms, 0);
	limit /= 2;
	prev_r_start_val = mon_set_limit(RD_MON, limit);
	prev_w_start_val = mon_set_limit(WR_MON, limit);

	mon_irq_enable(RD_MON, true);
	mon_irq_enable(WR_MON, true);
	mon_enable(RD_MON);
	mon_enable(WR_MON);
	global_mon_enable(true);

	return 0;
}

static void stop_bw_hwmon(struct devfreq *df)
{
	global_mon_enable(false);
	mon_disable(RD_MON);
	mon_disable(WR_MON);
	mon_irq_enable(RD_MON, false);
	mon_irq_enable(WR_MON, false);
}

static struct cpubw_hwmon bw_hwmon = {
	.start_hwmon = &start_bw_hwmon,
	.stop_hwmon = &stop_bw_hwmon,
	.is_valid_irq = &is_valid_bw_irq,
	.meas_bw_and_set_irq = &meas_bw_and_set_irq,
};

static int krait_l2pm_driver_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret;

	bw_hwmon.irq = platform_get_irq(pdev, 0);
	if (bw_hwmon.irq < 0) {
		pr_err("Unable to get IRQ number\n");
		return bw_hwmon.irq;
	}

	ret = of_property_read_u32(dev->of_node, "qcom,bytes-per-beat",
					&bytes_per_beat);
	if (ret) {
		pr_err("Unable to read bytes per beat\n");
		return ret;
	}

	ret = register_cpubw_hwmon(&bw_hwmon);
	if (ret) {
		pr_err("CPUBW hwmon registration failed\n");
		return ret;
	}

	return 0;
}

static struct of_device_id match_table[] = {
	{ .compatible = "qcom,kraitbw-l2pm" },
	{}
};

static struct platform_driver krait_l2pm_driver = {
	.probe = krait_l2pm_driver_probe,
	.driver = {
		.name = "kraitbw-l2pm",
		.of_match_table = match_table,
		.owner = THIS_MODULE,
	},
};

static int __init krait_l2pm_init(void)
{
	return platform_driver_register(&krait_l2pm_driver);
}
module_init(krait_l2pm_init);

static void __exit krait_l2pm_exit(void)
{
	platform_driver_unregister(&krait_l2pm_driver);
}
module_exit(krait_l2pm_exit);

MODULE_DESCRIPTION("Krait L2 performance monitor driver");
MODULE_LICENSE("GPL v2");
