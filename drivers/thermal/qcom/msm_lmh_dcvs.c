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
#include <linux/atomic.h>

#include <asm/smp_plat.h>
#include <asm/cacheflush.h>

#include <soc/qcom/scm.h>

#include "../thermal_core.h"

#define LIMITS_DCVSH                0x10
#define LIMITS_PROFILE_CHANGE       0x01
#define LIMITS_NODE_DCVS            0x44435653

#define LIMITS_SUB_FN_THERMAL       0x54484D4C
#define LIMITS_SUB_FN_CRNT          0x43524E54
#define LIMITS_SUB_FN_REL           0x52454C00
#define LIMITS_SUB_FN_BCL           0x42434C00
#define LIMITS_SUB_FN_GENERAL       0x47454E00

#define LIMITS_ALGO_MODE_ENABLE     0x454E424C

#define LIMITS_HI_THRESHOLD         0x48494748
#define LIMITS_LOW_THRESHOLD        0x4C4F5700
#define LIMITS_ARM_THRESHOLD        0x41524D00

#define LIMITS_CLUSTER_0            0x6370302D
#define LIMITS_CLUSTER_1            0x6370312D

#define LIMITS_DOMAIN_MAX           0x444D4158
#define LIMITS_DOMAIN_MIN           0x444D494E

#define LIMITS_TEMP_DEFAULT         75000
#define LIMITS_LOW_THRESHOLD_OFFSET 500
#define LIMITS_POLLING_DELAY_MS     10
#define LIMITS_CLUSTER_0_REQ        0x179C1B04
#define LIMITS_CLUSTER_1_REQ        0x179C3B04
#define LIMITS_CLUSTER_0_INT_CLR    0x179CE808
#define LIMITS_CLUSTER_1_INT_CLR    0x179CC808
#define LIMITS_CLUSTER_0_MIN_FREQ   0x17D78BC0
#define LIMITS_CLUSTER_1_MIN_FREQ   0x17D70BC0
#define dcvsh_get_frequency(_val, _max) do { \
	_max = (_val) & 0x3FF; \
	_max *= 19200; \
} while (0)
#define FREQ_KHZ_TO_HZ(_val) ((_val) * 1000)
#define FREQ_HZ_TO_KHZ(_val) ((_val) / 1000)

enum lmh_hw_trips {
	LIMITS_TRIP_ARM,
	LIMITS_TRIP_HI,
	LIMITS_TRIP_MAX,
};

struct limits_dcvs_hw {
	char sensor_name[THERMAL_NAME_LENGTH];
	uint32_t affinity;
	uint32_t temp_limits[LIMITS_TRIP_MAX];
	int irq_num;
	void *osm_hw_reg;
	void *int_clr_reg;
	void *min_freq_reg;
	cpumask_t core_map;
	struct timer_list poll_timer;
	unsigned long max_freq;
	unsigned long min_freq;
	unsigned long hw_freq_limit;
	struct list_head list;
	atomic_t is_irq_enabled;
};

LIST_HEAD(lmh_dcvs_hw_list);

static int limits_dcvs_get_freq_limits(uint32_t cpu, unsigned long *max_freq,
					 unsigned long *min_freq)
{
	unsigned long freq_ceil = UINT_MAX, freq_floor = 0;
	struct device *cpu_dev = NULL;
	int ret = 0;

	cpu_dev = get_cpu_device(cpu);
	if (!cpu_dev) {
		pr_err("Error in get CPU%d device\n", cpu);
		return -ENODEV;
	}

	rcu_read_lock();
	dev_pm_opp_find_freq_floor(cpu_dev, &freq_ceil);
	dev_pm_opp_find_freq_ceil(cpu_dev, &freq_floor);
	rcu_read_unlock();

	*max_freq = freq_ceil / 1000;
	*min_freq = freq_floor / 1000;

	return ret;
}

static unsigned long limits_mitigation_notify(struct limits_dcvs_hw *hw)
{
	uint32_t val = 0;
	struct device *cpu_dev = NULL;
	unsigned long freq_val, max_limit = 0;
	struct dev_pm_opp *opp_entry;

	val = readl_relaxed(hw->osm_hw_reg);
	dcvsh_get_frequency(val, max_limit);
	cpu_dev = get_cpu_device(cpumask_first(&hw->core_map));
	if (!cpu_dev) {
		pr_err("Error in get CPU%d device\n",
			cpumask_first(&hw->core_map));
		goto notify_exit;
	}

	freq_val = FREQ_KHZ_TO_HZ(max_limit);
	rcu_read_lock();
	opp_entry = dev_pm_opp_find_freq_floor(cpu_dev, &freq_val);
	/*
	 * Hardware mitigation frequency can be lower than the lowest
	 * possible CPU frequency. In that case freq floor call will
	 * fail with -ERANGE and we need to match to the lowest
	 * frequency using freq_ceil.
	 */
	if (IS_ERR(opp_entry) && PTR_ERR(opp_entry) == -ERANGE) {
		opp_entry = dev_pm_opp_find_freq_ceil(cpu_dev, &freq_val);
		if (IS_ERR(opp_entry))
			dev_err(cpu_dev, "frequency:%lu. opp error:%ld\n",
					freq_val, PTR_ERR(opp_entry));
	}
	rcu_read_unlock();
	max_limit = FREQ_HZ_TO_KHZ(freq_val);

	sched_update_cpu_freq_min_max(&hw->core_map, 0, max_limit);

notify_exit:
	hw->hw_freq_limit = max_limit;
	return max_limit;
}

static void limits_dcvs_poll(unsigned long data)
{
	unsigned long max_limit = 0;
	struct limits_dcvs_hw *hw = (struct limits_dcvs_hw *)data;

	if (hw->max_freq == UINT_MAX)
		limits_dcvs_get_freq_limits(cpumask_first(&hw->core_map),
			&hw->max_freq, &hw->min_freq);
	max_limit = limits_mitigation_notify(hw);
	if (max_limit >= hw->max_freq) {
		del_timer(&hw->poll_timer);
		writel_relaxed(0xFF, hw->int_clr_reg);
		atomic_set(&hw->is_irq_enabled, 1);
		enable_irq(hw->irq_num);
	} else {
		mod_timer(&hw->poll_timer, jiffies + msecs_to_jiffies(
			LIMITS_POLLING_DELAY_MS));
	}
}

static void lmh_dcvs_notify(struct limits_dcvs_hw *hw)
{
	if (atomic_dec_and_test(&hw->is_irq_enabled)) {
		disable_irq_nosync(hw->irq_num);
		limits_mitigation_notify(hw);
		mod_timer(&hw->poll_timer, jiffies + msecs_to_jiffies(
			LIMITS_POLLING_DELAY_MS));
	}
}

static irqreturn_t lmh_dcvs_handle_isr(int irq, void *data)
{
	struct limits_dcvs_hw *hw = data;

	lmh_dcvs_notify(hw);

	return IRQ_HANDLED;
}

static int limits_dcvs_write(uint32_t node_id, uint32_t fn,
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
	desc_arg.args[2] = LIMITS_NODE_DCVS;
	desc_arg.args[3] = node_id;
	desc_arg.args[4] = 0; /* version */
	desc_arg.arginfo = SCM_ARGS(5, SCM_RO, SCM_VAL, SCM_VAL,
					SCM_VAL, SCM_VAL);

	dmac_flush_range(payload, (void *)payload + 5 * (sizeof(uint32_t)));
	ret = scm_call2(SCM_SIP_FNID(SCM_SVC_LMH, LIMITS_DCVSH), &desc_arg);

	kfree(payload);

	return ret;
}

static int lmh_get_temp(void *data, int *val)
{
	/*
	 * LMH DCVSh hardware doesn't support temperature read.
	 * return a default value for the thermal core to aggregate
	 * the thresholds
	 */
	*val = LIMITS_TEMP_DEFAULT;

	return 0;
}

static int lmh_set_trips(void *data, int low, int high)
{
	struct limits_dcvs_hw *hw = (struct limits_dcvs_hw *)data;
	int ret = 0;

	if (high < LIMITS_LOW_THRESHOLD_OFFSET || low < 0) {
		pr_err("Value out of range low:%d high:%d\n",
				low, high);
		return -EINVAL;
	}

	/* Sanity check limits before writing to the hardware */
	if (low >= high)
		return -EINVAL;

	hw->temp_limits[LIMITS_TRIP_HI] = (uint32_t)high;
	hw->temp_limits[LIMITS_TRIP_ARM] = (uint32_t)low;

	ret =  limits_dcvs_write(hw->affinity, LIMITS_SUB_FN_THERMAL,
				  LIMITS_ARM_THRESHOLD, low);
	if (ret)
		return ret;
	ret =  limits_dcvs_write(hw->affinity, LIMITS_SUB_FN_THERMAL,
				  LIMITS_HI_THRESHOLD, high);
	if (ret)
		return ret;
	ret =  limits_dcvs_write(hw->affinity, LIMITS_SUB_FN_THERMAL,
				  LIMITS_LOW_THRESHOLD,
				  high - LIMITS_LOW_THRESHOLD_OFFSET);
	if (ret)
		return ret;

	return ret;
}

static struct thermal_zone_of_device_ops limits_sensor_ops = {
	.get_temp   = lmh_get_temp,
	.set_trips  = lmh_set_trips,
};

static struct limits_dcvs_hw *get_dcvsh_hw_from_cpu(int cpu)
{
	struct limits_dcvs_hw *hw;

	list_for_each_entry(hw, &lmh_dcvs_hw_list, list) {
		if (cpumask_test_cpu(cpu, &hw->core_map))
			return hw;
	}

	return NULL;
}

static int enable_lmh(void)
{
	int ret = 0;
	struct scm_desc desc_arg;

	desc_arg.args[0] = 1;
	desc_arg.arginfo = SCM_ARGS(1, SCM_VAL);
	ret = scm_call2(SCM_SIP_FNID(SCM_SVC_LMH, LIMITS_PROFILE_CHANGE),
			&desc_arg);
	if (ret) {
		pr_err("Error switching profile:[1]. err:%d\n", ret);
		return ret;
	}

	return ret;
}

static int lmh_set_max_limit(int cpu, u32 freq)
{
	struct limits_dcvs_hw *hw = get_dcvsh_hw_from_cpu(cpu);

	if (!hw)
		return -EINVAL;

	return limits_dcvs_write(hw->affinity, LIMITS_SUB_FN_GENERAL,
				  LIMITS_DOMAIN_MAX, freq);
}

static int lmh_set_min_limit(int cpu, u32 freq)
{
	struct limits_dcvs_hw *hw = get_dcvsh_hw_from_cpu(cpu);

	if (!hw)
		return -EINVAL;

	if (freq != hw->min_freq)
		writel_relaxed(0x01, hw->min_freq_reg);
	else
		writel_relaxed(0x00, hw->min_freq_reg);

	return 0;
}
static struct cpu_cooling_ops cd_ops = {
	.ceil_limit = lmh_set_max_limit,
	.floor_limit = lmh_set_min_limit,
};

static int limits_dcvs_probe(struct platform_device *pdev)
{
	int ret;
	int affinity = -1;
	struct limits_dcvs_hw *hw;
	struct thermal_zone_device *tzdev;
	struct thermal_cooling_device *cdev;
	struct device_node *dn = pdev->dev.of_node;
	struct device_node *cpu_node, *lmh_node;
	uint32_t request_reg, clear_reg, min_reg;
	unsigned long max_freq, min_freq;
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

	ret = limits_dcvs_get_freq_limits(cpumask_first(&mask), &max_freq,
				     &min_freq);
	if (ret)
		return ret;
	hw = devm_kzalloc(&pdev->dev, sizeof(*hw), GFP_KERNEL);
	if (!hw)
		return -ENOMEM;

	cpumask_copy(&hw->core_map, &mask);
	switch (affinity) {
	case 0:
		hw->affinity = LIMITS_CLUSTER_0;
		break;
	case 1:
		hw->affinity = LIMITS_CLUSTER_1;
		break;
	default:
		return -EINVAL;
	};

	/* Enable the thermal algorithm early */
	ret = limits_dcvs_write(hw->affinity, LIMITS_SUB_FN_THERMAL,
		 LIMITS_ALGO_MODE_ENABLE, 1);
	if (ret)
		return ret;
	/* Enable the LMH outer loop algorithm */
	ret = limits_dcvs_write(hw->affinity, LIMITS_SUB_FN_CRNT,
		 LIMITS_ALGO_MODE_ENABLE, 1);
	if (ret)
		return ret;
	/* Enable the Reliability algorithm */
	ret = limits_dcvs_write(hw->affinity, LIMITS_SUB_FN_REL,
		 LIMITS_ALGO_MODE_ENABLE, 1);
	if (ret)
		return ret;
	/* Enable the BCL algorithm */
	ret = limits_dcvs_write(hw->affinity, LIMITS_SUB_FN_BCL,
		 LIMITS_ALGO_MODE_ENABLE, 1);
	if (ret)
		return ret;
	ret = enable_lmh();
	if (ret)
		return ret;

	/*
	 * Setup virtual thermal zones for each LMH-DCVS hardware
	 * The sensor does not do actual thermal temperature readings
	 * but does support setting thresholds for trips.
	 * Let's register with thermal framework, so we have the ability
	 * to set low/high thresholds.
	 */
	hw->temp_limits[LIMITS_TRIP_HI] = INT_MAX;
	hw->temp_limits[LIMITS_TRIP_ARM] = 0;
	hw->hw_freq_limit = hw->max_freq = max_freq;
	hw->min_freq = min_freq;
	snprintf(hw->sensor_name, sizeof(hw->sensor_name), "limits_sensor-%02d",
			affinity);
	tzdev = thermal_zone_of_sensor_register(&pdev->dev, 0, hw,
			&limits_sensor_ops);
	if (IS_ERR_OR_NULL(tzdev))
		return PTR_ERR(tzdev);

	/* Setup cooling devices to request mitigation states */
	cdev = cpufreq_platform_cooling_register(&hw->core_map, &cd_ops);
	if (IS_ERR_OR_NULL(cdev))
		return PTR_ERR(cdev);

	switch (affinity) {
	case 0:
		request_reg = LIMITS_CLUSTER_0_REQ;
		clear_reg = LIMITS_CLUSTER_0_INT_CLR;
		min_reg = LIMITS_CLUSTER_0_MIN_FREQ;
		break;
	case 1:
		request_reg = LIMITS_CLUSTER_1_REQ;
		clear_reg = LIMITS_CLUSTER_1_INT_CLR;
		min_reg = LIMITS_CLUSTER_1_MIN_FREQ;
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
	hw->min_freq_reg = devm_ioremap(&pdev->dev, min_reg, 0x4);
	if (!hw->min_freq_reg) {
		pr_err("min frequency enable register remap failed\n");
		return -ENOMEM;
	}
	init_timer_deferrable(&hw->poll_timer);
	hw->poll_timer.data = (unsigned long)hw;
	hw->poll_timer.function = limits_dcvs_poll;

	hw->irq_num = of_irq_get(pdev->dev.of_node, 0);
	if (hw->irq_num < 0) {
		ret = hw->irq_num;
		pr_err("Error getting IRQ number. err:%d\n", ret);
		return ret;
	}
	atomic_set(&hw->is_irq_enabled, 1);
	ret = devm_request_threaded_irq(&pdev->dev, hw->irq_num, NULL,
		lmh_dcvs_handle_isr, IRQF_TRIGGER_HIGH | IRQF_ONESHOT
		| IRQF_NO_SUSPEND, hw->sensor_name, hw);
	if (ret) {
		pr_err("Error registering for irq. err:%d\n", ret);
		return ret;
	}

	INIT_LIST_HEAD(&hw->list);
	list_add(&hw->list, &lmh_dcvs_hw_list);

	return ret;
}

static const struct of_device_id limits_dcvs_match[] = {
	{ .compatible = "qcom,msm-hw-limits", },
	{},
};

static struct platform_driver limits_dcvs_driver = {
	.probe		= limits_dcvs_probe,
	.driver		= {
		.name = KBUILD_MODNAME,
		.of_match_table = limits_dcvs_match,
	},
};
builtin_platform_driver(limits_dcvs_driver);
