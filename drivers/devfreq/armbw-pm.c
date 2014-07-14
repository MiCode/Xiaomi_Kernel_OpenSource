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

#define pr_fmt(fmt) "armbw-pm: " fmt

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
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/cpu_pm.h>
#include <linux/cpu.h>
#include "governor.h"
#include "governor_bw_hwmon.h"


#define DEFINE_CP15_READ(name, op1, n, m, op2)				\
static u32 read_##name(void)						\
{									\
	u32 val;							\
	asm volatile ("mrc p15, " #op1 ", %0, c" #n ", c" #m ", " #op2  \
			: "=r" (val));				        \
	return val;							\
}

#define DEFINE_CP15_WRITE(name, op1, n, m, op2)				\
static void write_##name(u32 val)					\
{									\
	asm volatile ("mcr p15, " #op1 ", %0, c" #n ", c" #m", "#op2	\
			: : "r" (val));					\
}

#define DEFINE_CP15_RW(name, op1, n, m, op2)	\
DEFINE_CP15_READ(name, op1, n, m, op2)	\
DEFINE_CP15_WRITE(name, op1, n, m, op2)

DEFINE_CP15_WRITE(pmselr, 0, 9, 12, 5)
DEFINE_CP15_WRITE(pmcntenset, 0, 9, 12, 1)
DEFINE_CP15_WRITE(pmcntenclr, 0, 9, 12, 2)
DEFINE_CP15_RW(pmovsr, 0, 9, 12, 3)
DEFINE_CP15_WRITE(pmxevtyper, 0, 9, 13, 1)
DEFINE_CP15_RW(pmxevcntr, 0, 9, 13, 2)
DEFINE_CP15_WRITE(pmintenset, 0, 9, 14, 1)
DEFINE_CP15_WRITE(pmintenclr, 0, 9, 14, 2)
DEFINE_CP15_WRITE(pmcr, 0, 9, 12, 0)

struct bwmon_data {
	int cpu;
	u32 saved_evcntr;
	unsigned long count;
	u32 prev_rw_start_val;
	u32 limit;
};

static DEFINE_SPINLOCK(bw_lock);
static struct bw_hwmon *globalhw;
static struct work_struct irqwork;
static int bw_irq;
static DEFINE_PER_CPU(struct bwmon_data, gov_data);
static int use_cnt;
static DEFINE_MUTEX(use_lock);
static struct workqueue_struct *bw_wq;
static u32 bytes_per_beat;

#define RW_NUM 0x19
#define RW_MON 0

static void mon_enable(void *info)
{
	/* Clear previous overflow state for given counter*/
	write_pmovsr(BIT(RW_MON));
	/* Enable event counter n */
	write_pmcntenset(BIT(RW_MON));
}

static void mon_disable(void *info)
{
	write_pmcntenclr(BIT(RW_MON));
}

static void mon_irq_enable(void *info)
{
	write_pmintenset(BIT(RW_MON));
}

static void mon_irq_disable(void *info)
{
	write_pmintenclr(BIT(RW_MON));
}

static void mon_set_counter(void *count)
{
	write_pmxevcntr(*(u32 *) count);
}

static void mon_bw_init(void *evcntrval)
{
	u32 count;

	if (!evcntrval)
		count = 0xFFFFFFFF;
	else
		count = *(u32 *) evcntrval;

	write_pmcr(BIT(0));
	write_pmselr(RW_MON);
	write_pmxevtyper(RW_NUM);
	write_pmxevcntr(count);
}

static void percpu_bwirq_enable(void *info)
{
	enable_percpu_irq(bw_irq, IRQ_TYPE_EDGE_RISING);
}

static void percpu_bwirq_disable(void *info)
{
	disable_percpu_irq(bw_irq);
}

static irqreturn_t mon_intr_handler(int irq, void *dev_id)
{
	queue_work(bw_wq, &irqwork);
	return IRQ_HANDLED;
}

static void bwmon_work(struct work_struct *work)
{
	update_bw_hwmon(globalhw);
}

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

static long mon_get_bw_count(u32 start_val)
{
	u32 overflow, count;

	count = read_pmxevcntr();
	overflow = read_pmovsr();
	if (overflow & BIT(RW_MON))
		return 0xFFFFFFFF - start_val + count;
	else
		return count - start_val;
}

static void get_beat_count(void *arg)
{
	int cpu = smp_processor_id();
	struct bwmon_data *data = &per_cpu(gov_data, cpu);

	mon_disable(NULL);
	data->count = mon_get_bw_count(data->prev_rw_start_val);
}

static unsigned long measure_bw_and_set_irq(struct bw_hwmon *hw,
					unsigned int tol, unsigned int us)
{
	unsigned long bw = 0;
	unsigned long tempbw;
	int cpu;
	struct bwmon_data *data;
	unsigned int sample_ms = hw->df->profile->polling_ms;

	spin_lock(&bw_lock);
	on_each_cpu(get_beat_count, NULL, true);
	for_each_possible_cpu(cpu) {
		data = &per_cpu(gov_data, cpu);

		tempbw = beats_to_mbps(data->count, us);
		data->limit = mbps_to_beats(tempbw, sample_ms, tol);
		data->prev_rw_start_val = 0xFFFFFFFF - data->limit;
		if (cpu_online(cpu))
			smp_call_function_single(cpu, mon_set_counter,
					      &(data->prev_rw_start_val), true);
		bw += tempbw;
		data->count = 0;
	}
	on_each_cpu(mon_enable, NULL, true);
	spin_unlock(&bw_lock);
	return bw;
}

static void save_hotplugstate(void)
{
	int cpu = smp_processor_id();
	struct bwmon_data *data;

	data = &per_cpu(gov_data, cpu);
	percpu_bwirq_disable(NULL);
	mon_disable(NULL);
	data->saved_evcntr = read_pmxevcntr();
	data->count = mon_get_bw_count(data->prev_rw_start_val);
}

static void restore_hotplugstate(void)
{
	int cpu = smp_processor_id();
	u32 count;
	struct bwmon_data *data;

	data = &per_cpu(gov_data, cpu);
	percpu_bwirq_enable(NULL);
	if (data->count != 0)
		count = data->saved_evcntr;
	else
		count = data->prev_rw_start_val = 0xFFFFFFFF - data->limit;
	mon_bw_init(&count);
	mon_irq_enable(NULL);
	mon_enable(NULL);
}

static void save_pmstate(void)
{
	int cpu = smp_processor_id();
	struct bwmon_data *data;

	data = &per_cpu(gov_data, cpu);
	mon_disable(NULL);
	data->saved_evcntr = read_pmxevcntr();
}

static void restore_pmstate(void)
{
	int cpu = smp_processor_id();
	u32 count;
	struct bwmon_data *data;

	data = &per_cpu(gov_data, cpu);
	count = data->saved_evcntr;
	mon_bw_init(&count);
	mon_irq_enable(NULL);
	mon_enable(NULL);
}

static int pm_notif(struct notifier_block *nb, unsigned long action,
			void *data)
{
	switch (action) {
	case CPU_PM_ENTER:
		save_pmstate();
		break;
	case CPU_PM_ENTER_FAILED:
	case CPU_PM_EXIT:
		restore_pmstate();
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block bwmon_cpu_pm_nb = {
	.notifier_call = pm_notif,
};

static int hotplug_notif(struct notifier_block *nb, unsigned long action,
			void *data)
{
	switch (action) {
	case CPU_DYING:
		spin_lock(&bw_lock);
		save_hotplugstate();
		spin_unlock(&bw_lock);
		break;
	case CPU_STARTING:
		spin_lock(&bw_lock);
		restore_hotplugstate();
		spin_unlock(&bw_lock);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block cpu_hotplug_nb = {
	.notifier_call = hotplug_notif,
};

static int register_notifier(void)
{
	int ret = 0;

	mutex_lock(&use_lock);
	if (use_cnt == 0) {
		ret = cpu_pm_register_notifier(&bwmon_cpu_pm_nb);
		if (ret)
			goto out;
		ret = register_cpu_notifier(&cpu_hotplug_nb);
		if (ret) {
			cpu_pm_unregister_notifier(&bwmon_cpu_pm_nb);
			goto out;
		}
	}
	use_cnt++;
out:
	mutex_unlock(&use_lock);
	return ret;
}

static void unregister_notifier(void)
{
	mutex_lock(&use_lock);
	if (use_cnt == 1) {
		unregister_cpu_notifier(&cpu_hotplug_nb);
		cpu_pm_unregister_notifier(&bwmon_cpu_pm_nb);
	} else if (use_cnt == 0) {
		pr_warn("Notifier ref count unbalanced\n");
		goto out;
	}
	use_cnt--;
out:
	mutex_unlock(&use_lock);
}

static void stop_bw_hwmon(struct bw_hwmon *hw)
{
	unregister_notifier();
	on_each_cpu(mon_disable, NULL, true);
	on_each_cpu(mon_irq_disable, NULL, true);
	on_each_cpu(percpu_bwirq_disable, NULL, true);
	free_percpu_irq(bw_irq, &gov_data);
}

static int start_bw_hwmon(struct bw_hwmon *hw, unsigned long mbps)
{
	u32 limit;
	int cpu;
	struct bwmon_data *data;
	struct device *dev = hw->df->dev.parent;
	int ret;

	ret = request_percpu_irq(bw_irq, mon_intr_handler,
				"bw_hwmon", &gov_data);
	if (ret) {
		dev_err(dev, "Unable to register interrupt handler!\n");
		return ret;
	}

	get_online_cpus();
	on_each_cpu(mon_bw_init, NULL, true);
	on_each_cpu(mon_disable, NULL, true);

	ret = register_notifier();
	if (ret) {
		pr_err("Unable to register notifier\n");
		return ret;
	}

	limit = mbps_to_beats(mbps, hw->df->profile->polling_ms, 0);
	limit /= num_online_cpus();

	for_each_possible_cpu(cpu) {
		data = &per_cpu(gov_data, cpu);
		data->limit = limit;
		data->prev_rw_start_val = 0xFFFFFFFF - data->limit;
	}

	INIT_WORK(&irqwork, bwmon_work);

	on_each_cpu(percpu_bwirq_enable, NULL, true);
	on_each_cpu(mon_irq_enable, NULL, true);
	on_each_cpu(mon_enable, NULL, true);
	put_online_cpus();
	return 0;
}

static int armbw_pm_driver_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct bw_hwmon *bw;
	int ret;

	bw = devm_kzalloc(dev, sizeof(*bw), GFP_KERNEL);
	if (!bw)
		return -ENOMEM;
	bw->dev = dev;

	bw_irq = platform_get_irq(pdev, 0);
	if (bw_irq < 0) {
		pr_err("Unable to get IRQ number\n");
		return bw_irq;
	}

	ret = of_property_read_u32(dev->of_node, "qcom,bytes-per-beat",
					&bytes_per_beat);

	if (ret) {
		pr_err("Unable to read bytes per beat\n");
		return ret;
	}

	bw->start_hwmon = &start_bw_hwmon;
	bw->stop_hwmon = &stop_bw_hwmon;
	bw->meas_bw_and_set_irq = &measure_bw_and_set_irq;
	globalhw = bw;

	ret = register_bw_hwmon(dev, bw);
	if (ret) {
		pr_err("CPUBW hwmon registration failed\n");
		return ret;
	}
	return 0;
}

static struct of_device_id match_table[] = {
	{ .compatible = "qcom,armbw-pm" },
	{}
};

static struct platform_driver armbw_pm_driver = {
	.probe = armbw_pm_driver_probe,
	.driver = {
		.name = "armbw-pm",
		.of_match_table = match_table,
		.owner = THIS_MODULE,
	},
};

static int __init armbw_pm_init(void)
{
	bw_wq = alloc_workqueue("armbw-pm-bwmon", WQ_HIGHPRI, 2);
	return platform_driver_register(&armbw_pm_driver);
}
module_init(armbw_pm_init);

static void __exit armbw_pm_exit(void)
{
	platform_driver_unregister(&armbw_pm_driver);
	destroy_workqueue(bw_wq);
}
module_exit(armbw_pm_exit);
