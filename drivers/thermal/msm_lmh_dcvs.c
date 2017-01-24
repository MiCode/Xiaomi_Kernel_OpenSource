/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s:%s " fmt, KBUILD_MODNAME, __func__

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/pm_opp.h>
#include <linux/cpu_cooling.h>
#include <linux/bitmap.h>
#include <linux/msm_thermal.h>

#include <asm/smp_plat.h>
#include <asm/cacheflush.h>

#include <soc/qcom/scm.h>

#include "thermal_core.h"

#define CREATE_TRACE_POINTS
#define LMH_DCVS_TRACE
#include <trace/trace_thermal.h>

#define MSM_LIMITS_DCVSH		0x10
#define MSM_LIMITS_NODE_DCVS		0x44435653

#define MSM_LIMITS_SUB_FN_THERMAL	0x54484D4C
#define MSM_LIMITS_SUB_FN_GENERAL	0x47454E00

#define MSM_LIMITS_ALGO_MODE_ENABLE	0x454E424C

#define MSM_LIMITS_HI_THRESHOLD		0x48494748
#define MSM_LIMITS_LOW_THRESHOLD        0x4C4F5700
#define MSM_LIMITS_ARM_THRESHOLD	0x41524D00

#define MSM_LIMITS_CLUSTER_0		0x6370302D
#define MSM_LIMITS_CLUSTER_1		0x6370312D

#define MSM_LIMITS_DOMAIN_MAX		0x444D4158

#define MSM_LIMITS_HIGH_THRESHOLD_VAL	95000
#define MSM_LIMITS_ARM_THRESHOLD_VAL	65000
#define MSM_LIMITS_LOW_THRESHOLD_OFFSET 500
#define MSM_LIMITS_POLLING_DELAY_MS	10
#define MSM_LIMITS_CLUSTER_0_REQ	0x179C1B04
#define MSM_LIMITS_CLUSTER_1_REQ	0x179C3B04
#define MSM_LIMITS_CLUSTER_0_INT_CLR	0x179CE808
#define MSM_LIMITS_CLUSTER_1_INT_CLR	0x179CC808
#define dcvsh_get_frequency(_val, _max) do { \
	_max = (_val) & 0x3FF; \
	_max *= 19200; \
} while (0)

enum lmh_hw_trips {
	LIMITS_TRIP_LO,
	LIMITS_TRIP_HI,
	LIMITS_TRIP_MAX,
};

struct msm_lmh_dcvs_hw {
	uint32_t affinity;
	uint32_t temp_limits[LIMITS_TRIP_MAX];
	struct sensor_threshold default_lo, default_hi;
	int irq_num;
	void *osm_hw_reg;
	void *int_clr_reg;
	cpumask_t core_map;
	struct timer_list poll_timer;
	uint32_t max_freq;
	uint32_t hw_freq_limit;
	struct list_head list;
	DECLARE_BITMAP(is_irq_enabled, 1);
};

LIST_HEAD(lmh_dcvs_hw_list);

static void msm_lmh_dcvs_get_max_freq(uint32_t cpu, uint32_t *max_freq)
{
	unsigned long freq_ceil = UINT_MAX;
	struct device *cpu_dev = NULL;

	cpu_dev = get_cpu_device(cpu);
	if (!cpu_dev) {
		pr_err("Error in get CPU%d device\n", cpu);
		return;
	}

	rcu_read_lock();
	dev_pm_opp_find_freq_floor(cpu_dev, &freq_ceil);
	rcu_read_unlock();
	*max_freq = freq_ceil/1000;
}

static uint32_t msm_lmh_mitigation_notify(struct msm_lmh_dcvs_hw *hw)
{
	uint32_t max_limit = 0, val = 0;
	struct device *cpu_dev = NULL;
	unsigned long freq_val;

	val = readl_relaxed(hw->osm_hw_reg);
	dcvsh_get_frequency(val, max_limit);
	cpu_dev = get_cpu_device(cpumask_first(&hw->core_map));
	if (!cpu_dev) {
		pr_err("Error in get CPU%d device\n",
			cpumask_first(&hw->core_map));
		goto notify_exit;
	}

	freq_val = max_limit;
	rcu_read_lock();
	dev_pm_opp_find_freq_floor(cpu_dev, &freq_val);
	rcu_read_unlock();
	max_limit = freq_val;

	sched_update_cpu_freq_min_max(&hw->core_map, 0, max_limit);
	trace_lmh_dcvs_freq(cpumask_first(&hw->core_map), max_limit);

notify_exit:
	hw->hw_freq_limit = max_limit;
	return max_limit;
}

static void msm_lmh_dcvs_poll(unsigned long data)
{
	uint32_t max_limit = 0;
	struct msm_lmh_dcvs_hw *hw = (struct msm_lmh_dcvs_hw *)data;

	if (hw->max_freq == UINT_MAX)
		msm_lmh_dcvs_get_max_freq(cpumask_first(&hw->core_map),
			&hw->max_freq);
	max_limit = msm_lmh_mitigation_notify(hw);
	if (max_limit >= hw->max_freq) {
		del_timer(&hw->poll_timer);
		writel_relaxed(0xFF, hw->int_clr_reg);
		set_bit(1, hw->is_irq_enabled);
		enable_irq(hw->irq_num);
	} else {
		mod_timer(&hw->poll_timer, jiffies + msecs_to_jiffies(
			MSM_LIMITS_POLLING_DELAY_MS));
	}
}

static void lmh_dcvs_notify(struct msm_lmh_dcvs_hw *hw)
{
	if (test_and_clear_bit(1, hw->is_irq_enabled)) {
		disable_irq_nosync(hw->irq_num);
		msm_lmh_mitigation_notify(hw);
		mod_timer(&hw->poll_timer, jiffies + msecs_to_jiffies(
			MSM_LIMITS_POLLING_DELAY_MS));
	}
}

static irqreturn_t lmh_dcvs_handle_isr(int irq, void *data)
{
	struct msm_lmh_dcvs_hw *hw = data;

	lmh_dcvs_notify(hw);
	return IRQ_HANDLED;
}

static int msm_lmh_dcvs_write(uint32_t node_id, uint32_t fn,
		uint32_t setting, uint32_t val)
{
	int ret;
	struct scm_desc desc_arg;
	uint32_t *payload = NULL;

	payload = kzalloc(sizeof(uint32_t) * 5, GFP_KERNEL);
	if (!payload)
		return -ENOMEM;

	payload[0] = fn; /* algorithm */
	payload[1] = 0; /* unused sub-algorithm */
	payload[2] = setting;
	payload[3] = 1; /* number of values */
	payload[4] = val;

	desc_arg.args[0] = SCM_BUFFER_PHYS(payload);
	desc_arg.args[1] = sizeof(uint32_t) * 5;
	desc_arg.args[2] = MSM_LIMITS_NODE_DCVS;
	desc_arg.args[3] = node_id;
	desc_arg.args[4] = 0; /* version */
	desc_arg.arginfo = SCM_ARGS(5, SCM_RO, SCM_VAL, SCM_VAL,
					SCM_VAL, SCM_VAL);

	dmac_flush_range(payload, (void *)payload + 5 * (sizeof(uint32_t)));
	ret = scm_call2(SCM_SIP_FNID(SCM_SVC_LMH, MSM_LIMITS_DCVSH), &desc_arg);

	kfree(payload);
	return ret;
}

static int lmh_get_trip_type(struct thermal_zone_device *dev,
				int trip, enum thermal_trip_type *type)
{
	switch (trip) {
	case LIMITS_TRIP_LO:
		*type = THERMAL_TRIP_CONFIGURABLE_LOW;
		break;
	case LIMITS_TRIP_HI:
		*type = THERMAL_TRIP_CONFIGURABLE_HI;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int lmh_activate_trip(struct thermal_zone_device *dev,
		int trip, enum thermal_trip_activation_mode mode)
{
	struct msm_lmh_dcvs_hw *hw = dev->devdata;
	uint32_t enable, temp;
	int ret = 0;

	enable = (mode == THERMAL_TRIP_ACTIVATION_ENABLED) ? 1 : 0;
	if (!enable) {
		pr_info("%s: disable not supported\n", __func__);
		return 0;
	}

	/* Sanity check limits before writing to the hardware */
	if (hw->temp_limits[LIMITS_TRIP_LO] >=
			hw->temp_limits[LIMITS_TRIP_HI])
		return -EINVAL;

	temp = hw->temp_limits[trip];
	switch (trip) {
	case LIMITS_TRIP_LO:
		ret =  msm_lmh_dcvs_write(hw->affinity,
				MSM_LIMITS_SUB_FN_THERMAL,
				MSM_LIMITS_ARM_THRESHOLD, temp);
		break;
	case LIMITS_TRIP_HI:
		/*
		 * The high threshold should be atleast greater than the
		 * low threshold offset
		 */
		if (temp < MSM_LIMITS_LOW_THRESHOLD_OFFSET)
			return -EINVAL;
		ret =  msm_lmh_dcvs_write(hw->affinity,
				MSM_LIMITS_SUB_FN_THERMAL,
				MSM_LIMITS_HI_THRESHOLD, temp);
		if (ret)
			break;
		ret =  msm_lmh_dcvs_write(hw->affinity,
				MSM_LIMITS_SUB_FN_THERMAL,
				MSM_LIMITS_LOW_THRESHOLD, temp -
				MSM_LIMITS_LOW_THRESHOLD_OFFSET);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static int lmh_get_trip_temp(struct thermal_zone_device *dev,
			int trip, int *value)
{
	struct msm_lmh_dcvs_hw *hw = dev->devdata;

	*value = hw->temp_limits[trip];

	return 0;
}

static int lmh_set_trip_temp(struct thermal_zone_device *dev,
			int trip, int value)
{
	struct msm_lmh_dcvs_hw *hw = dev->devdata;

	if (value < 0) {
		pr_err("Value out of range :%d\n", value);
		return -EINVAL;
	}

	hw->temp_limits[trip] = (uint32_t)value;

	return 0;
}

static struct thermal_zone_device_ops limits_sensor_ops = {
	.get_trip_type		= lmh_get_trip_type,
	.activate_trip_type	= lmh_activate_trip,
	.get_trip_temp		= lmh_get_trip_temp,
	.set_trip_temp		= lmh_set_trip_temp,
};

static int trip_notify(enum thermal_trip_type type, int temp, void *data)
{
	return 0;
}

static struct msm_lmh_dcvs_hw *get_dcvsh_hw_from_cpu(int cpu)
{
	struct msm_lmh_dcvs_hw *hw;

	list_for_each_entry(hw, &lmh_dcvs_hw_list, list) {
		if (cpumask_test_cpu(cpu, &hw->core_map))
			return hw;
	}

	return NULL;
}

static int lmh_set_max_limit(int cpu, u32 freq)
{
	struct msm_lmh_dcvs_hw *hw = get_dcvsh_hw_from_cpu(cpu);

	if (!hw)
		return -EINVAL;

	return msm_lmh_dcvs_write(hw->affinity, MSM_LIMITS_SUB_FN_GENERAL,
				MSM_LIMITS_DOMAIN_MAX, freq);
}

static int lmh_get_cur_limit(int cpu, unsigned long *freq)
{
	struct msm_lmh_dcvs_hw *hw = get_dcvsh_hw_from_cpu(cpu);

	if (!hw)
		return -EINVAL;
	*freq = hw->hw_freq_limit;

	return 0;
}

static struct cpu_cooling_ops cd_ops = {
	.get_cur_state = lmh_get_cur_limit,
	.ceil_limit = lmh_set_max_limit,
};

int msm_lmh_dcvsh_sw_notify(int cpu)
{
	struct msm_lmh_dcvs_hw *hw = get_dcvsh_hw_from_cpu(cpu);

	if (!hw)
		return -EINVAL;

	lmh_dcvs_notify(hw);
	return 0;
}

static int msm_lmh_dcvs_probe(struct platform_device *pdev)
{
	int ret;
	int affinity = -1;
	struct msm_lmh_dcvs_hw *hw;
	char sensor_name[] = "limits_sensor-00";
	struct thermal_zone_device *tzdev;
	struct thermal_cooling_device *cdev;
	struct device_node *dn = pdev->dev.of_node;
	struct device_node *cpu_node, *lmh_node;
	uint32_t id, max_freq, request_reg, clear_reg;
	int cpu;
	cpumask_t mask = { CPU_BITS_NONE };

	for_each_possible_cpu(cpu) {
		cpu_node = of_cpu_device_node_get(cpu);
		if (!cpu_node)
			continue;
		lmh_node = of_parse_phandle(cpu_node, "qcom,lmh-dcvs", 0);
		if (lmh_node == dn) {
			affinity = MPIDR_AFFINITY_LEVEL(
					cpu_logical_map(cpu), 1);
			/*set the cpumask*/
			cpumask_set_cpu(cpu, &(mask));
		}
		of_node_put(cpu_node);
		of_node_put(lmh_node);
	}

	/*
	 * We return error if none of the CPUs have
	 * reference to our LMH node
	 */
	if (affinity == -1)
		return -EINVAL;

	msm_lmh_dcvs_get_max_freq(cpumask_first(&mask), &max_freq);
	hw = devm_kzalloc(&pdev->dev, sizeof(*hw), GFP_KERNEL);
	if (!hw)
		return -ENOMEM;

	cpumask_copy(&hw->core_map, &mask);
	switch (affinity) {
	case 0:
		hw->affinity = MSM_LIMITS_CLUSTER_0;
		break;
	case 1:
		hw->affinity = MSM_LIMITS_CLUSTER_1;
		break;
	default:
		return -EINVAL;
	};

	/* Enable the thermal algorithm early */
	ret = msm_lmh_dcvs_write(hw->affinity, MSM_LIMITS_SUB_FN_THERMAL,
		 MSM_LIMITS_ALGO_MODE_ENABLE, 1);
	if (ret)
		return ret;

	hw->default_lo.temp = MSM_LIMITS_ARM_THRESHOLD_VAL;
	hw->default_lo.trip = THERMAL_TRIP_CONFIGURABLE_LOW;
	hw->default_lo.notify = trip_notify;

	hw->default_hi.temp = MSM_LIMITS_HIGH_THRESHOLD_VAL;
	hw->default_hi.trip = THERMAL_TRIP_CONFIGURABLE_HI;
	hw->default_hi.notify = trip_notify;

	/*
	 * Setup virtual thermal zones for each LMH-DCVS hardware
	 * The sensor does not do actual thermal temperature readings
	 * but does support setting thresholds for trips.
	 * Let's register with thermal framework, so we have the ability
	 * to set low/high thresholds.
	 */
	snprintf(sensor_name, sizeof(sensor_name), "limits_sensor-%02d",
			affinity);
	tzdev = thermal_zone_device_register(sensor_name, LIMITS_TRIP_MAX,
			(1 << LIMITS_TRIP_MAX) - 1, hw, &limits_sensor_ops,
			NULL, 0, 0);
	if (IS_ERR_OR_NULL(tzdev))
		return PTR_ERR(tzdev);

	/* Setup cooling devices to request mitigation states */
	cdev = cpufreq_platform_cooling_register(&hw->core_map, &cd_ops);
	if (IS_ERR_OR_NULL(cdev))
		return PTR_ERR(cdev);
	/*
	 * Driver defaults to for low and hi thresholds.
	 * Since we make a check for hi > lo value, set the hi threshold
	 * before the low threshold
	 */
	id = sensor_get_id(sensor_name);
	if (id < 0)
		return id;

	ret = sensor_set_trip(id, &hw->default_hi);
	if (!ret) {
		ret = sensor_activate_trip(id, &hw->default_hi, true);
		if (ret)
			return ret;
	} else {
		return ret;
	}

	ret = sensor_set_trip(id, &hw->default_lo);
	if (!ret) {
		ret = sensor_activate_trip(id, &hw->default_lo, true);
		if (ret)
			return ret;
	}

	hw->hw_freq_limit = hw->max_freq = max_freq;

	switch (affinity) {
	case 0:
		request_reg = MSM_LIMITS_CLUSTER_0_REQ;
		clear_reg = MSM_LIMITS_CLUSTER_0_INT_CLR;
		break;
	case 1:
		request_reg = MSM_LIMITS_CLUSTER_1_REQ;
		clear_reg = MSM_LIMITS_CLUSTER_1_INT_CLR;
		break;
	default:
		return -EINVAL;
	};

	hw->osm_hw_reg = devm_ioremap(&pdev->dev, request_reg, 0x4);
	if (!hw->osm_hw_reg) {
		pr_err("register remap failed\n");
		return -ENOMEM;
	}
	hw->int_clr_reg = devm_ioremap(&pdev->dev, clear_reg, 0x4);
	if (!hw->int_clr_reg) {
		pr_err("interrupt clear reg remap failed\n");
		return -ENOMEM;
	}
	init_timer_deferrable(&hw->poll_timer);
	hw->poll_timer.data = (unsigned long)hw;
	hw->poll_timer.function = msm_lmh_dcvs_poll;

	hw->irq_num = of_irq_get(pdev->dev.of_node, 0);
	if (hw->irq_num < 0) {
		ret = hw->irq_num;
		pr_err("Error getting IRQ number. err:%d\n", ret);
		return ret;
	}
	set_bit(1, hw->is_irq_enabled);
	ret = devm_request_threaded_irq(&pdev->dev, hw->irq_num, NULL,
		lmh_dcvs_handle_isr, IRQF_TRIGGER_HIGH | IRQF_ONESHOT
		| IRQF_NO_SUSPEND, sensor_name, hw);
	if (ret) {
		pr_err("Error registering for irq. err:%d\n", ret);
		return ret;
	}

	INIT_LIST_HEAD(&hw->list);
	list_add(&hw->list, &lmh_dcvs_hw_list);

	return ret;
}

static const struct of_device_id msm_lmh_dcvs_match[] = {
	{ .compatible = "qcom,msm-hw-limits", },
	{},
};

static struct platform_driver msm_lmh_dcvs_driver = {
	.probe		= msm_lmh_dcvs_probe,
	.driver		= {
		.name = KBUILD_MODNAME,
		.of_match_table = msm_lmh_dcvs_match,
	},
};
builtin_platform_driver(msm_lmh_dcvs_driver);
