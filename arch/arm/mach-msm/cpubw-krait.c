/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "cpubw-krait: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <trace/events/power.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>

#include <mach/msm-krait-l2-accessors.h>

#define L2PMRESR2		0x412
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
#define MON_INT			33

#define MBYTE			(1 << 20)

#define BW(_bw) \
	{ \
		.vectors = (struct msm_bus_vectors[]){ \
			{\
				.src = MSM_BUS_MASTER_AMPSS_M0, \
				.dst = MSM_BUS_SLAVE_EBI_CH0, \
			}, \
			{ \
				.src = MSM_BUS_MASTER_AMPSS_M1, \
				.dst = MSM_BUS_SLAVE_EBI_CH0, \
			}, \
		}, \
		.num_paths = 2, \
	}

/* Has to be a power of 2 to work correctly */
static unsigned int bytes_per_beat = 8;
module_param(bytes_per_beat, uint, 0644);

static unsigned int sample_ms = 50;
module_param(sample_ms, uint, 0644);

static unsigned int tolerance_percent = 10;
module_param(tolerance_percent, uint, 0644);

static unsigned int guard_band_mbps = 100;
module_param(guard_band_mbps, uint, 0644);

static unsigned int decay_rate = 90;
module_param(decay_rate, uint, 0644);

static unsigned int io_percent = 15;
module_param(io_percent, uint, 0644);

static unsigned int bw_step = 200;
module_param(bw_step, uint, 0644);

static struct kernel_param_ops enable_ops;
static bool enable;
module_param_cb(enable, &enable_ops, &enable, S_IRUGO | S_IWUSR);

static void mon_init(void)
{
	/* Set up counters 0/1 to count write/read beats */
	set_l2_indirect_reg(L2PMRESR2, 0x8B0B0000);
	set_l2_indirect_reg(L2PMnEVCNTCR(0), 0x0);
	set_l2_indirect_reg(L2PMnEVCNTCR(1), 0x0);
	set_l2_indirect_reg(L2PMnEVCNTR(0), 0xFFFFFFFF);
	set_l2_indirect_reg(L2PMnEVCNTR(1), 0xFFFFFFFF);
	set_l2_indirect_reg(L2PMnEVFILTER(0), 0xF003F);
	set_l2_indirect_reg(L2PMnEVFILTER(1), 0xF003F);
	set_l2_indirect_reg(L2PMnEVTYPER(0), 0xA);
	set_l2_indirect_reg(L2PMnEVTYPER(1), 0xB);
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

/* Returns start counter value to be used with mon_get_mbps() */
static u32 mon_set_limit_mbyte(int n, unsigned int mbytes)
{
	u32 regval, beats;

	beats = mult_frac(mbytes, MBYTE, bytes_per_beat);
	regval = 0xFFFFFFFF - beats;
	set_l2_indirect_reg(L2PMnEVCNTR(n), regval);
	pr_debug("EV%d MB: %d, start val: %x\n", n, mbytes, regval);

	return regval;
}

/* Returns MBps of read/writes for the sampling window. */
static int mon_get_mbps(int n, u32 start_val, unsigned int us)
{
	u32 overflow, count;
	long long beats;

	count = get_l2_indirect_reg(L2PMnEVCNTR(n));
	overflow = get_l2_indirect_reg(L2PMOVSR);

	if (overflow & BIT(n))
		beats = 0xFFFFFFFF - start_val + count;
	else
		beats = count - start_val;

	beats *= USEC_PER_SEC;
	beats *= bytes_per_beat;
	do_div(beats, us);
	beats = DIV_ROUND_UP_ULL(beats, MBYTE);

	pr_debug("EV%d ov: %x, cnt: %x\n", n, overflow, count);

	return beats;
}

static void do_bw_sample(struct work_struct *work);
static DECLARE_DEFERRED_WORK(bw_sample, do_bw_sample);
static struct workqueue_struct *bw_sample_wq;

static DEFINE_MUTEX(bw_lock);
static ktime_t prev_ts;
static u32 prev_r_start_val;
static u32 prev_w_start_val;

static struct msm_bus_paths bw_levels[] = {
	BW(0), BW(200),
};
static struct msm_bus_scale_pdata bw_data = {
	.usecase = bw_levels,
	.num_usecases = ARRAY_SIZE(bw_levels),
	.name = "cpubw-krait",
	.active_only = 1,
};
static u32 bus_client;
static void compute_bw(int mbps);
static irqreturn_t mon_intr_handler(int irq, void *dev_id);

#define START_LIMIT	100 /* MBps */
static int start_monitoring(void)
{
	int mb_limit;
	int ret;

	ret = request_threaded_irq(MON_INT, NULL, mon_intr_handler,
			  IRQF_ONESHOT | IRQF_SHARED | IRQF_TRIGGER_RISING,
			  "cpubw_krait", mon_intr_handler);
	if (ret) {
		pr_err("Unable to register interrupt handler\n");
		return ret;
	}

	bw_sample_wq = alloc_workqueue("cpubw-krait", WQ_HIGHPRI, 0);
	if (!bw_sample_wq) {
		pr_err("Unable to alloc workqueue\n");
		ret = -ENOMEM;
		goto alloc_wq_fail;
	}

	bus_client = msm_bus_scale_register_client(&bw_data);
	if (!bus_client) {
		pr_err("Unable to register bus client\n");
		ret = -ENODEV;
		goto bus_reg_fail;
	}

	compute_bw(START_LIMIT);

	mon_init();
	mon_disable(0);
	mon_disable(1);

	mb_limit = mult_frac(START_LIMIT, sample_ms, MSEC_PER_SEC);
	mb_limit /= 2;

	prev_r_start_val = mon_set_limit_mbyte(0, mb_limit);
	prev_w_start_val = mon_set_limit_mbyte(1, mb_limit);

	prev_ts = ktime_get();

	set_l2_indirect_reg(L2PMINTENSET, BIT(0));
	set_l2_indirect_reg(L2PMINTENSET, BIT(1));
	mon_enable(0);
	mon_enable(1);
	global_mon_enable(true);

	queue_delayed_work(bw_sample_wq, &bw_sample,
				msecs_to_jiffies(sample_ms));

	return 0;

bus_reg_fail:
	destroy_workqueue(bw_sample_wq);
alloc_wq_fail:
	disable_irq(MON_INT);
	free_irq(MON_INT, mon_intr_handler);
	return ret;
}

static void stop_monitoring(void)
{
	global_mon_enable(false);
	mon_disable(0);
	mon_disable(1);
	set_l2_indirect_reg(L2PMINTENCLR, BIT(0));
	set_l2_indirect_reg(L2PMINTENCLR, BIT(1));

	disable_irq(MON_INT);
	free_irq(MON_INT, mon_intr_handler);

	cancel_delayed_work_sync(&bw_sample);
	destroy_workqueue(bw_sample_wq);

	bw_levels[0].vectors[0].ib = 0;
	bw_levels[0].vectors[0].ab = 0;
	bw_levels[0].vectors[1].ib = 0;
	bw_levels[0].vectors[1].ab = 0;

	bw_levels[1].vectors[0].ib = 0;
	bw_levels[1].vectors[0].ab = 0;
	bw_levels[1].vectors[1].ib = 0;
	bw_levels[1].vectors[1].ab = 0;
	msm_bus_scale_unregister_client(bus_client);
}

static void set_bw(int mbps)
{
	static int cur_idx, cur_ab, cur_ib;
	int new_ab, new_ib;
	int i, ret;

	if (!io_percent)
		io_percent = 1;
	new_ab = roundup(mbps, bw_step);
	new_ib = mbps * 100 / io_percent;
	new_ib = roundup(new_ib, bw_step);

	if (cur_ib == new_ib && cur_ab == new_ab)
		return;

	i = (cur_idx + 1) % ARRAY_SIZE(bw_levels);

	bw_levels[i].vectors[0].ib = new_ib * 1000000ULL;
	bw_levels[i].vectors[0].ab = new_ab * 1000000ULL;
	bw_levels[i].vectors[1].ib = new_ib * 1000000ULL;
	bw_levels[i].vectors[1].ab = new_ab * 1000000ULL;

	pr_debug("BW MBps: Req: %d AB: %d IB: %d\n", mbps, new_ab, new_ib);

	ret = msm_bus_scale_client_update_request(bus_client, i);
	if (ret)
		pr_err("bandwidth request failed (%d)\n", ret);
	else {
		cur_idx = i;
		cur_ib = new_ib;
		cur_ab = new_ab;
	}
}

static void compute_bw(int mbps)
{
	static int cur_bw;
	int new_bw;

	mbps += guard_band_mbps;

	if (mbps > cur_bw) {
		new_bw = mbps;
	} else {
		new_bw = mbps * decay_rate + cur_bw * (100 - decay_rate);
		new_bw /= 100;
	}

	if (new_bw == cur_bw)
		return;

	set_bw(new_bw);
	cur_bw = new_bw;
}

static int to_limit(int mbps)
{
	mbps *= (100 + tolerance_percent) * sample_ms;
	mbps /= 100;
	mbps = DIV_ROUND_UP(mbps, MSEC_PER_SEC);
	return mbps;
}

static void measure_bw(void)
{
	int r_mbps, w_mbps, mbps;
	ktime_t ts;
	unsigned int us;

	mutex_lock(&bw_lock);

	/*
	 * Since we are stopping the counters, we don't want this short work
	 * to be interrupted by other tasks and cause the measurements to be
	 * wrong. Not blocking interrupts to avoid affecting interrupt
	 * latency and since they should be short anyway because they run in
	 * atomic context.
	 */
	preempt_disable();

	ts = ktime_get();
	us = ktime_to_us(ktime_sub(ts, prev_ts));
	if (!us)
		us = 1;

	mon_disable(0);
	mon_disable(1);

	r_mbps = mon_get_mbps(0, prev_r_start_val, us);
	w_mbps = mon_get_mbps(1, prev_w_start_val, us);

	prev_r_start_val = mon_set_limit_mbyte(0, to_limit(r_mbps));
	prev_w_start_val = mon_set_limit_mbyte(1, to_limit(w_mbps));

	mon_enable(0);
	mon_enable(1);

	preempt_enable();

	mbps = r_mbps + w_mbps;
	pr_debug("R/W/BW/us = %d/%d/%d/%d\n", r_mbps, w_mbps, mbps, us);
	compute_bw(mbps);

	prev_ts = ts;
	mutex_unlock(&bw_lock);
}

static void do_bw_sample(struct work_struct *work)
{
	measure_bw();
	queue_delayed_work(bw_sample_wq, &bw_sample,
				msecs_to_jiffies(sample_ms));
}

static irqreturn_t mon_intr_handler(int irq, void *dev_id)
{
	bool pending;
	u32 regval;

	regval = get_l2_indirect_reg(L2PMOVSR);
	pr_debug("Got interrupt: %x\n", regval);

	pending = cancel_delayed_work_sync(&bw_sample);

	/*
	 * Don't recalc bandwidth if the interrupt came just after the end
	 * of the sample period (!pending). This is done for two reasons:
	 *
	 * 1. Sampling the BW during a very short duration can result in a
	 *    very inaccurate measurement due to very short bursts.
	 * 2. If the limit was hit very close to the sample period, then the
	 *    current BW estimate is not very off and can stay as such.
	 */
	if (pending)
		measure_bw();

	queue_delayed_work(bw_sample_wq, &bw_sample,
				msecs_to_jiffies(sample_ms));

	return IRQ_HANDLED;
}

static int set_enable(const char *arg, const struct kernel_param *kp)
{
	int ret;
	bool old_val = *((bool *) kp->arg);
	bool new_val;

	if (!arg)
		arg = "1";
	ret = strtobool(arg, &new_val);
	if (ret)
		return ret;

	if (!old_val && new_val) {
		if (start_monitoring()) {
			pr_err("L2PM counters already in use.\n");
			return ret;
		} else {
			pr_info("Enabling CPU BW monitoring\n");
		}
	} else if (old_val && !new_val) {
		pr_info("Disabling CPU BW monitoring\n");
		stop_monitoring();
	}

	*(bool *) kp->arg = new_val;
	return 0;
}

static struct kernel_param_ops enable_ops = {
	.set = set_enable,
	.get = param_get_bool,
};

static int cpubw_krait_init(void)
{
	bw_sample_wq = alloc_workqueue("cpubw-krait", WQ_HIGHPRI, 0);
	if (!bw_sample_wq)
		return -ENOMEM;

	bus_client = msm_bus_scale_register_client(&bw_data);
	if (!bus_client) {
		pr_err("Unable to register bus client\n");
		destroy_workqueue(bw_sample_wq);
		return -ENODEV;
	}

	return 0;
}
late_initcall(cpubw_krait_init);

MODULE_DESCRIPTION("CPU DDR bandwidth voting driver for Krait CPUs");
MODULE_LICENSE("GPL v2");
