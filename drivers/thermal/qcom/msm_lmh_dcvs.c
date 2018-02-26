/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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
#include <linux/regulator/consumer.h>

#include <asm/smp_plat.h>
#include <asm/cacheflush.h>

#include <soc/qcom/scm.h>

#include "../thermal_core.h"
#include "lmh_dbg.h"

#define CREATE_TRACE_POINTS
#include <trace/events/lmh.h>

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

#define LIMITS_FREQ_CAP             0x46434150

#define LIMITS_TEMP_DEFAULT         75000
#define LIMITS_TEMP_HIGH_THRESH_MAX 120000
#define LIMITS_LOW_THRESHOLD_OFFSET 500
#define LIMITS_POLLING_DELAY_MS     10
#define LIMITS_CLUSTER_0_REQ        0x17D43704
#define LIMITS_CLUSTER_1_REQ        0x17D45F04
#define LIMITS_CLUSTER_0_INT_CLR    0x17D78808
#define LIMITS_CLUSTER_1_INT_CLR    0x17D70808
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

struct __limits_cdev_data {
	struct thermal_cooling_device *cdev;
	u32 max_freq;
	u32 min_freq;
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
	struct delayed_work freq_poll_work;
	unsigned long max_freq;
	unsigned long min_freq;
	unsigned long hw_freq_limit;
	struct device_attribute lmh_freq_attr;
	struct list_head list;
	bool is_irq_enabled;
	struct mutex access_lock;
	struct __limits_cdev_data *cdev_data;
	struct regulator *isens_reg;
};

LIST_HEAD(lmh_dcvs_hw_list);
DEFINE_MUTEX(lmh_dcvs_list_access);

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

	pr_debug("CPU:%d max value read:%lu\n",
			cpumask_first(&hw->core_map),
			max_limit);
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
	pr_debug("CPU:%d max limit:%lu\n", cpumask_first(&hw->core_map),
			max_limit);
	trace_lmh_dcvs_freq(cpumask_first(&hw->core_map), max_limit);

notify_exit:
	hw->hw_freq_limit = max_limit;
	return max_limit;
}

static void limits_dcvs_poll(struct work_struct *work)
{
	unsigned long max_limit = 0;
	struct limits_dcvs_hw *hw = container_of(work,
					struct limits_dcvs_hw,
					freq_poll_work.work);

	mutex_lock(&hw->access_lock);
	if (hw->max_freq == UINT_MAX)
		limits_dcvs_get_freq_limits(cpumask_first(&hw->core_map),
			&hw->max_freq, &hw->min_freq);
	max_limit = limits_mitigation_notify(hw);
	if (max_limit >= hw->max_freq) {
		writel_relaxed(0xFF, hw->int_clr_reg);
		hw->is_irq_enabled = true;
		enable_irq(hw->irq_num);
	} else {
		mod_delayed_work(system_highpri_wq, &hw->freq_poll_work,
			 msecs_to_jiffies(LIMITS_POLLING_DELAY_MS));
	}
	mutex_unlock(&hw->access_lock);
}

static void lmh_dcvs_notify(struct limits_dcvs_hw *hw)
{
	if (hw->is_irq_enabled) {
		hw->is_irq_enabled = false;
		disable_irq_nosync(hw->irq_num);
		limits_mitigation_notify(hw);
		mod_delayed_work(system_highpri_wq, &hw->freq_poll_work,
			 msecs_to_jiffies(LIMITS_POLLING_DELAY_MS));
	}
}

static irqreturn_t lmh_dcvs_handle_isr(int irq, void *data)
{
	struct limits_dcvs_hw *hw = data;

	mutex_lock(&hw->access_lock);
	lmh_dcvs_notify(hw);
	mutex_unlock(&hw->access_lock);

	return IRQ_HANDLED;
}

static int limits_dcvs_write(uint32_t node_id, uint32_t fn,
			      uint32_t setting, uint32_t val, uint32_t val1,
			      bool enable_val1)
{
	int ret;
	struct scm_desc desc_arg;
	uint32_t *payload = NULL;
	uint32_t payload_len;

	payload_len = ((enable_val1) ? 6 : 5) * sizeof(uint32_t);
	payload = kzalloc(payload_len, GFP_KERNEL);
	if (!payload)
		return -ENOMEM;

	payload[0] = fn; /* algorithm */
	payload[1] = 0; /* unused sub-algorithm */
	payload[2] = setting;
	payload[3] = enable_val1 ? 2 : 1; /* number of values */
	payload[4] = val;
	if (enable_val1)
		payload[5] = val1;

	desc_arg.args[0] = SCM_BUFFER_PHYS(payload);
	desc_arg.args[1] = payload_len;
	desc_arg.args[2] = LIMITS_NODE_DCVS;
	desc_arg.args[3] = node_id;
	desc_arg.args[4] = 0; /* version */
	desc_arg.arginfo = SCM_ARGS(5, SCM_RO, SCM_VAL, SCM_VAL,
					SCM_VAL, SCM_VAL);

	dmac_flush_range(payload, (void *)payload + payload_len);
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

	if (high >= LIMITS_TEMP_HIGH_THRESH_MAX || low < 0) {
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
				  LIMITS_ARM_THRESHOLD, low, 0, 0);
	if (ret)
		return ret;
	ret =  limits_dcvs_write(hw->affinity, LIMITS_SUB_FN_THERMAL,
				  LIMITS_HI_THRESHOLD, high, 0, 0);
	if (ret)
		return ret;
	ret =  limits_dcvs_write(hw->affinity, LIMITS_SUB_FN_THERMAL,
				  LIMITS_LOW_THRESHOLD,
				  high - LIMITS_LOW_THRESHOLD_OFFSET,
				  0, 0);
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

	mutex_lock(&lmh_dcvs_list_access);
	list_for_each_entry(hw, &lmh_dcvs_hw_list, list) {
		if (cpumask_test_cpu(cpu, &hw->core_map)) {
			mutex_unlock(&lmh_dcvs_list_access);
			return hw;
		}
	}
	mutex_unlock(&lmh_dcvs_list_access);

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
	int ret = 0, cpu_idx, idx = 0;
	u32 max_freq = U32_MAX;

	if (!hw)
		return -EINVAL;

	mutex_lock(&hw->access_lock);
	for_each_cpu(cpu_idx, &hw->core_map) {
		if (cpu_idx == cpu)
		/*
		 * If there is no limits restriction for CPU scaling max
		 * frequency, vote for a very high value. This will allow
		 * the CPU to use the boost frequencies.
		 */
			hw->cdev_data[idx].max_freq =
				(freq == hw->max_freq) ? U32_MAX : freq;
		if (max_freq > hw->cdev_data[idx].max_freq)
			max_freq = hw->cdev_data[idx].max_freq;
		idx++;
	}
	ret = limits_dcvs_write(hw->affinity, LIMITS_SUB_FN_THERMAL,
				  LIMITS_FREQ_CAP, max_freq,
				  (max_freq == U32_MAX) ? 0 : 1, 1);
	lmh_dcvs_notify(hw);
	mutex_unlock(&hw->access_lock);

	return ret;
}

static int lmh_set_min_limit(int cpu, u32 freq)
{
	struct limits_dcvs_hw *hw = get_dcvsh_hw_from_cpu(cpu);
	int cpu_idx, idx = 0;
	u32 min_freq = 0;

	if (!hw)
		return -EINVAL;

	mutex_lock(&hw->access_lock);
	for_each_cpu(cpu_idx, &hw->core_map) {
		if (cpu_idx == cpu)
			hw->cdev_data[idx].min_freq = freq;
		if (min_freq < hw->cdev_data[idx].min_freq)
			min_freq = hw->cdev_data[idx].min_freq;
		idx++;
	}
	if (min_freq != hw->min_freq)
		writel_relaxed(0x01, hw->min_freq_reg);
	else
		writel_relaxed(0x00, hw->min_freq_reg);
	mutex_unlock(&hw->access_lock);

	return 0;
}
static struct cpu_cooling_ops cd_ops = {
	.ceil_limit = lmh_set_max_limit,
	.floor_limit = lmh_set_min_limit,
};

static int limits_cpu_online(unsigned int online_cpu)
{
	struct limits_dcvs_hw *hw = get_dcvsh_hw_from_cpu(online_cpu);
	unsigned int idx = 0, cpu = 0;

	if (!hw)
		return 0;

	for_each_cpu(cpu, &hw->core_map) {
		cpumask_t cpu_mask  = { CPU_BITS_NONE };

		if (cpu != online_cpu) {
			idx++;
			continue;
		} else if (hw->cdev_data[idx].cdev) {
			return 0;
		}
		cpumask_set_cpu(cpu, &cpu_mask);
		hw->cdev_data[idx].max_freq = U32_MAX;
		hw->cdev_data[idx].min_freq = 0;
		hw->cdev_data[idx].cdev = cpufreq_platform_cooling_register(
						&cpu_mask, &cd_ops);
		if (IS_ERR_OR_NULL(hw->cdev_data[idx].cdev)) {
			pr_err("CPU:%u cooling device register error:%ld\n",
				cpu, PTR_ERR(hw->cdev_data[idx].cdev));
			hw->cdev_data[idx].cdev = NULL;
		} else {
			pr_debug("CPU:%u cooling device registered\n", cpu);
		}
		break;

	}

	return 0;
}

static void limits_isens_vref_ldo_init(struct platform_device *pdev,
					struct limits_dcvs_hw *hw)
{
	int ret = 0;
	uint32_t settings[3];

	hw->isens_reg = devm_regulator_get(&pdev->dev, "isens_vref");
	if (IS_ERR_OR_NULL(hw->isens_reg)) {
		if (PTR_ERR(hw->isens_reg) == -ENODEV)
			return;

		pr_err("Regulator:isens_vref init error:%ld\n",
			PTR_ERR(hw->isens_reg));
		return;
	}
	ret = of_property_read_u32_array(pdev->dev.of_node,
					"isens-vref-settings",
					settings, 3);
	if (ret) {
		pr_err("Regulator:isens_vref settings read error:%d\n",
				ret);
		devm_regulator_put(hw->isens_reg);
		return;
	}
	ret = regulator_set_voltage(hw->isens_reg, settings[0], settings[1]);
	if (ret) {
		pr_err("Regulator:isens_vref set voltage error:%d\n", ret);
		devm_regulator_put(hw->isens_reg);
		return;
	}
	ret = regulator_set_load(hw->isens_reg, settings[2]);
	if (ret) {
		pr_err("Regulator:isens_vref set load error:%d\n", ret);
		devm_regulator_put(hw->isens_reg);
		return;
	}
	if (regulator_enable(hw->isens_reg)) {
		pr_err("Failed to enable regulator:isens_vref\n");
		devm_regulator_put(hw->isens_reg);
		return;
	}
}

static ssize_t
lmh_freq_limit_show(struct device *dev, struct device_attribute *devattr,
		       char *buf)
{
	struct limits_dcvs_hw *hw = container_of(devattr,
						struct limits_dcvs_hw,
						lmh_freq_attr);

	return snprintf(buf, PAGE_SIZE, "%lu\n", hw->hw_freq_limit);
}

static int limits_dcvs_probe(struct platform_device *pdev)
{
	int ret;
	int affinity = -1;
	struct limits_dcvs_hw *hw;
	struct thermal_zone_device *tzdev;
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
	if (cpumask_empty(&mask))
		return -EINVAL;

	ret = limits_dcvs_get_freq_limits(cpumask_first(&mask), &max_freq,
				     &min_freq);
	if (ret)
		return ret;
	hw = devm_kzalloc(&pdev->dev, sizeof(*hw), GFP_KERNEL);
	if (!hw)
		return -ENOMEM;
	hw->cdev_data = devm_kcalloc(&pdev->dev, cpumask_weight(&mask),
				   sizeof(*hw->cdev_data),
				   GFP_KERNEL);
	if (!hw->cdev_data)
		return -ENOMEM;

	cpumask_copy(&hw->core_map, &mask);
	ret = of_property_read_u32(dn, "qcom,affinity", &affinity);
	if (ret)
		return -ENODEV;
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
		 LIMITS_ALGO_MODE_ENABLE, 1, 0, 0);
	if (ret)
		return ret;
	/* Enable the LMH outer loop algorithm */
	ret = limits_dcvs_write(hw->affinity, LIMITS_SUB_FN_CRNT,
		 LIMITS_ALGO_MODE_ENABLE, 1, 0, 0);
	if (ret)
		return ret;
	/* Enable the Reliability algorithm */
	ret = limits_dcvs_write(hw->affinity, LIMITS_SUB_FN_REL,
		 LIMITS_ALGO_MODE_ENABLE, 1, 0, 0);
	if (ret)
		return ret;
	/* Enable the BCL algorithm */
	ret = limits_dcvs_write(hw->affinity, LIMITS_SUB_FN_BCL,
		 LIMITS_ALGO_MODE_ENABLE, 1, 0, 0);
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
		ret = -EINVAL;
		goto unregister_sensor;
	};

	hw->min_freq_reg = devm_ioremap(&pdev->dev, min_reg, 0x4);
	if (!hw->min_freq_reg) {
		pr_err("min frequency enable register remap failed\n");
		ret = -ENOMEM;
		goto unregister_sensor;
	}

	mutex_init(&hw->access_lock);
	INIT_DEFERRABLE_WORK(&hw->freq_poll_work, limits_dcvs_poll);
	hw->osm_hw_reg = devm_ioremap(&pdev->dev, request_reg, 0x4);
	if (!hw->osm_hw_reg) {
		pr_err("register remap failed\n");
		goto probe_exit;
	}
	hw->int_clr_reg = devm_ioremap(&pdev->dev, clear_reg, 0x4);
	if (!hw->int_clr_reg) {
		pr_err("interrupt clear reg remap failed\n");
		goto probe_exit;
	}

	hw->irq_num = of_irq_get(pdev->dev.of_node, 0);
	if (hw->irq_num < 0) {
		pr_err("Error getting IRQ number. err:%d\n", hw->irq_num);
		goto probe_exit;
	}
	hw->is_irq_enabled = true;
	ret = devm_request_threaded_irq(&pdev->dev, hw->irq_num, NULL,
		lmh_dcvs_handle_isr, IRQF_TRIGGER_HIGH | IRQF_ONESHOT
		| IRQF_NO_SUSPEND, hw->sensor_name, hw);
	if (ret) {
		pr_err("Error registering for irq. err:%d\n", ret);
		ret = 0;
		goto probe_exit;
	}
	limits_isens_vref_ldo_init(pdev, hw);
	hw->lmh_freq_attr.attr.name = "lmh_freq_limit";
	hw->lmh_freq_attr.show = lmh_freq_limit_show;
	hw->lmh_freq_attr.attr.mode = 0444;
	device_create_file(&pdev->dev, &hw->lmh_freq_attr);

probe_exit:
	mutex_lock(&lmh_dcvs_list_access);
	INIT_LIST_HEAD(&hw->list);
	list_add(&hw->list, &lmh_dcvs_hw_list);
	mutex_unlock(&lmh_dcvs_list_access);
	lmh_debug_register(pdev);

	ret = cpuhp_setup_state(CPUHP_AP_ONLINE_DYN, "lmh-dcvs/cdev:online",
				limits_cpu_online, NULL);
	if (ret < 0)
		goto unregister_sensor;
	ret = 0;

	return ret;

unregister_sensor:
	thermal_zone_of_sensor_unregister(&pdev->dev, tzdev);

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
