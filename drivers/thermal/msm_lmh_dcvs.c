/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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
#include <linux/platform_device.h>

#include <asm/smp_plat.h>
#include <asm/cacheflush.h>

#include <soc/qcom/scm.h>

#include "thermal_core.h"

#define MSM_LIMITS_DCVSH		0x10
#define MSM_LIMITS_NODE_DCVS		0x44435653

#define MSM_LIMITS_SUB_FN_THERMAL	0x54484D4C

#define MSM_LIMITS_ALGO_MODE_ENABLE	0x454E424C

#define MSM_LIMITS_HI_THRESHOLD		0x48494748
#define MSM_LIMITS_LO_THRESHOLD		0x4C4F5700

#define MSM_LIMITS_CLUSTER_0		0x6370302D
#define MSM_LIMITS_CLUSTER_1		0x6370312D

enum lmh_hw_trips {
	LIMITS_TRIP_LO,
	LIMITS_TRIP_HI,
	LIMITS_TRIP_MAX,
};

struct msm_lmh_dcvs_hw {
	uint32_t affinity;
	uint32_t temp_limits[LIMITS_TRIP_MAX];
	struct sensor_threshold default_lo, default_hi;
};

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

	dmac_flush_range(payload, payload + 5 * (sizeof(uint32_t)));
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
	uint32_t enable, temp, thresh;

	enable = (mode == THERMAL_TRIP_ACTIVATION_ENABLED) ? 1 : 0;
	if (!enable) {
		pr_info("%s: disable not supported\n", __func__);
		return 0;
	}

	/* Sanity check limits before writing to the hardware */
	if (hw->temp_limits[LIMITS_TRIP_LO] >=
			hw->temp_limits[LIMITS_TRIP_HI])
		return -EINVAL;

	thresh = (trip == LIMITS_TRIP_LO) ? MSM_LIMITS_LO_THRESHOLD :
			MSM_LIMITS_HI_THRESHOLD;
	temp = hw->temp_limits[trip];

	return msm_lmh_dcvs_write(hw->affinity, MSM_LIMITS_SUB_FN_THERMAL,
				thresh, temp);
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

static int msm_lmh_dcvs_probe(struct platform_device *pdev)
{
	int ret;
	uint32_t affinity;
	struct msm_lmh_dcvs_hw *hw;
	char sensor_name[] = "limits_sensor-00";
	struct thermal_zone_device *tzdev;
	struct device_node *dn = pdev->dev.of_node;
	struct device_node *cpu_node, *lmh_node;
	uint32_t id;
	int cpu;
	bool found = false;

	for_each_possible_cpu(cpu) {
		cpu_node = of_cpu_device_node_get(cpu);
		if (!cpu_node)
			continue;
		lmh_node = of_parse_phandle(cpu_node, "qcom,lmh-dcvs", 0);
		found = (lmh_node == dn);
		of_node_put(cpu_node);
		of_node_put(lmh_node);
		if (found) {
			affinity = MPIDR_AFFINITY_LEVEL(
					cpu_logical_map(cpu), 1);
			break;
		}
	}

	/*
	 * We return error if none of the CPUs have
	 * reference to our LMH node
	 */
	if (!found)
		return -EINVAL;

	hw = devm_kzalloc(&pdev->dev, sizeof(*hw), GFP_KERNEL);
	if (!hw)
		return -ENOMEM;

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

	hw->default_lo.temp = 65000;
	hw->default_lo.trip = THERMAL_TRIP_CONFIGURABLE_LOW;
	hw->default_lo.notify = trip_notify;

	hw->default_hi.temp = 95000;
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
