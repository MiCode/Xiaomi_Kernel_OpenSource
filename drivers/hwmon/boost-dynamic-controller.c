/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/module.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/qpnp/qpnp-adc.h>
#include <soc/qcom/rpm-smd.h>

struct boost_dynamic_controller {
	/* ADC_TM parameters */
	struct qpnp_adc_tm_chip		*boost_adc_tm_dev;
	struct qpnp_adc_tm_btm_param	boost_monitor_params;
	u32				vph_high_thresh_uv;
	u32				vph_low_thresh_uv;

	/* Boost mode parameters */
	int				old_boost_mode;

	/* RPM control parameters */
	const char			*boost_resource_type;
	const char			*boost_resource_key;
	int				resource_type;
	u32				resource_key;
	u32				resource_id;

	struct mutex			boost_mutex;
};

#define BOOST_DYNAMIC_CONTROLLER_DRIVER_NAME	"qcom,boost-dynamic-controller"

static u32 rpm_vreg_string_to_int(const u8 *str)
{
	int i, len;
	u32 output = 0;

	len = strnlen(str, sizeof(u32));
	for (i = 0; i < len; i++)
		output |= str[i] << (i * 8);

	return output;
}

static int boost_dynamic_controller_parse_dt(
		struct boost_dynamic_controller *boost_controller,
		struct device_node *of_node)
{
	int ret;

	ret = of_property_read_u32(of_node,
			"qcom,boost-dynamic-controller-vph-high-threshold-uv",
			&boost_controller->vph_high_thresh_uv);
	if (ret < 0) {
		pr_err("qcom,boost-dynamic-controller-vph-high-threshold-uv is missing, ret = %d",
			ret);
		return ret;
	}

	ret = of_property_read_u32(of_node,
			"qcom,boost-dynamic-controller-vph-low-threshold-uv",
			&boost_controller->vph_low_thresh_uv);
	if (ret < 0) {
		pr_err("qcom,boost-dynamic-controller-vph-low-threshold-uv is missing, ret = %d",
			ret);
		return ret;
	}

	ret = of_property_read_u32(of_node,
			"qcom,boost-dynamic-controller-boost-resource-id",
			&boost_controller->resource_id);
	if (ret < 0) {
		pr_err("qcom,boost-dynamic-controller-boost-resource-id is missing, ret = %d",
			ret);
		return ret;
	}

	ret = of_property_read_string(of_node,
			"qcom,boost-dynamic-controller-boost-resource-type",
			&boost_controller->boost_resource_type);
	if (ret < 0) {
		pr_err("qcom,boost-dynamic-controller-boost-resource-type is missing, ret = %d",
			ret);
		return ret;
	}

	ret = of_property_read_string(of_node,
			"qcom,boost-dynamic-controller-boost-resource-key",
			&boost_controller->boost_resource_key);
	if (ret < 0) {
		pr_err("qcom,boost-dynamic-controller-boost-resource-key is missing, ret = %d",
			ret);
		return ret;
	}

	return ret;
}

static int boost_rpm_send_message(
		struct boost_dynamic_controller *boost_controller,
		u32 data)
{
	int ret;
	struct msm_rpm_kvp kvp = {
		.key = boost_controller->resource_key,
		.data = (void *)&data,
		.length = sizeof(data),
	};

	ret = msm_rpm_send_message(MSM_RPM_CTX_ACTIVE_SET,
		boost_controller->resource_type,
		boost_controller->resource_id,
		&kvp, 1);
	if (ret < 0)
		pr_err("failed to inform RPM! (err = %d)\n", ret);

	return ret;
}

static void boost_dynamic_controller_notification(enum qpnp_tm_state state,
							void *ctx)
{
	struct boost_dynamic_controller *boost_controller = ctx;
	int ret;

	mutex_lock(&boost_controller->boost_mutex);

	if (state == ADC_TM_LOW_STATE &&
		boost_controller->old_boost_mode != ADC_TM_LOW_STATE) {
		ret = boost_rpm_send_message(boost_controller, 0);
		if (ret) {
			pr_err("sending boost mode to RPM failed, ret=%d\n",
				ret);
			boost_controller->boost_monitor_params.state_request =
				ADC_TM_LOW_THR_ENABLE;
		} else {
			boost_controller->old_boost_mode = ADC_TM_LOW_STATE;
			boost_controller->boost_monitor_params.state_request =
				ADC_TM_HIGH_THR_ENABLE;
		}
	} else if (state == ADC_TM_HIGH_STATE &&
		boost_controller->old_boost_mode != ADC_TM_HIGH_STATE) {
		ret = boost_rpm_send_message(boost_controller, 0);
		if (ret) {
			pr_err("sending boost mode to RPM failed, ret=%d\n",
				ret);
			boost_controller->boost_monitor_params.state_request =
				ADC_TM_HIGH_THR_ENABLE;
		} else {
			boost_controller->old_boost_mode = ADC_TM_HIGH_STATE;
			boost_controller->boost_monitor_params.state_request =
				ADC_TM_LOW_THR_ENABLE;
		}
	}

	mutex_unlock(&boost_controller->boost_mutex);

	qpnp_adc_tm_channel_measure(boost_controller->boost_adc_tm_dev,
		&boost_controller->boost_monitor_params);
}

static int boost_dynamic_controller_setup_vph_monitoring(
		struct boost_dynamic_controller *boost_controller,
		struct device *dev)
{
	int ret;
	char *key = NULL;
	struct qpnp_vadc_chip *vadc_dev;
	struct qpnp_vadc_result adc_result;

	key = "vph-threshold";
	boost_controller->boost_adc_tm_dev = qpnp_get_adc_tm(dev, key);
	if (IS_ERR(boost_controller->boost_adc_tm_dev)) {
		ret = PTR_ERR(boost_controller->boost_adc_tm_dev);
		if (ret != -EPROBE_DEFER)
			pr_err("adc_tm property missing, ret=%d\n", ret);
		return ret;
	}

	boost_controller->boost_monitor_params.high_thr =
		boost_controller->vph_high_thresh_uv;
	boost_controller->boost_monitor_params.low_thr =
		boost_controller->vph_low_thresh_uv;
	boost_controller->boost_monitor_params.channel = VSYS;
	boost_controller->boost_monitor_params.btm_ctx =
		(void *)boost_controller;
	boost_controller->boost_monitor_params.timer_interval =
		ADC_MEAS1_INTERVAL_1S;
	boost_controller->boost_monitor_params.threshold_notification =
		&boost_dynamic_controller_notification;

	key = "vph";
	vadc_dev = qpnp_get_vadc(dev, key);
	if (IS_ERR(vadc_dev)) {
		ret = PTR_ERR(vadc_dev);
		if (ret != -EPROBE_DEFER)
			pr_err("vadc property missing, ret=%d\n", ret);
		return ret;
	}

	ret = qpnp_vadc_read(vadc_dev, VSYS, &adc_result);
	if (ret) {
		pr_err("Unable to read VPH ret=%d\n", ret);
		return ret;
	}

	if (adc_result.physical > boost_controller->vph_high_thresh_uv) {
		ret = boost_rpm_send_message(boost_controller, 1);
		if (ret) {
			pr_err("sending boost mode to RPM failed, ret=%d\n",
				ret);
			return ret;
		}
		boost_controller->old_boost_mode = ADC_TM_HIGH_STATE;
		boost_controller->boost_monitor_params.state_request =
			ADC_TM_LOW_THR_ENABLE;
	} else {
		ret = boost_rpm_send_message(boost_controller, 0);
		if (ret) {
			pr_err("sending boost mode to RPM failed, ret=%d\n",
				ret);
			return ret;
		}
		boost_controller->old_boost_mode = ADC_TM_LOW_STATE;
		boost_controller->boost_monitor_params.state_request =
			ADC_TM_HIGH_THR_ENABLE;
	}

	ret = qpnp_adc_tm_channel_measure(boost_controller->boost_adc_tm_dev,
		&boost_controller->boost_monitor_params);
	if (ret) {
		pr_err("adc-tm setup failed: ret = %d\n", ret);
		return ret;
	}

	return ret;
}

static int boost_dynamic_controller_probe(struct platform_device *pdev)
{
	struct boost_dynamic_controller *boost_controller = NULL;
	struct device *dev = &pdev->dev;
	int ret;

	boost_controller = devm_kzalloc(dev,
			sizeof(struct boost_dynamic_controller), GFP_KERNEL);
	if (!boost_controller) {
		dev_err(dev, "Cannot allocate boost_dynamic_controller\n");
		return -ENOMEM;
	}

	if (!dev->of_node) {
		dev_err(dev, "Device tree node is missing\n");
		return -EINVAL;
	}

	ret = boost_dynamic_controller_parse_dt(boost_controller, dev->of_node);
	if (ret) {
		pr_err("Wrong DT parameter specified: ret = %d\n", ret);
		return ret;
	}

	boost_controller->resource_type =
		rpm_vreg_string_to_int(boost_controller->boost_resource_type);
	boost_controller->resource_key =
		rpm_vreg_string_to_int(boost_controller->boost_resource_key);

	mutex_init(&boost_controller->boost_mutex);

	ret = boost_dynamic_controller_setup_vph_monitoring(boost_controller,
		dev);

	if (ret) {
		mutex_destroy(&boost_controller->boost_mutex);
		if (ret != -EPROBE_DEFER)
			pr_err("Vph monitor setup failure, ret = %d\n", ret);
		return ret;
	}

	return ret;
}

static int boost_dynamic_controller_remove(struct platform_device *pdev)
{
	struct boost_dynamic_controller *boost_controller;

	boost_controller = platform_get_drvdata(pdev);
	mutex_destroy(&boost_controller->boost_mutex);
	return 0;
}

static struct of_device_id boost_dynamic_controller_match_table[] = {
	{ .compatible = BOOST_DYNAMIC_CONTROLLER_DRIVER_NAME, },
	{}
};

static struct platform_driver boost_dynamic_controller_driver = {
	.driver		= {
		.name	= BOOST_DYNAMIC_CONTROLLER_DRIVER_NAME,
		.of_match_table = boost_dynamic_controller_match_table,
		.owner = THIS_MODULE,
	},
	.probe		= boost_dynamic_controller_probe,
	.remove		= boost_dynamic_controller_remove,
};

static int __init boost_dynamic_controller_init(void)
{
	return platform_driver_register(&boost_dynamic_controller_driver);
}
module_init(boost_dynamic_controller_init);

static void __exit boost_dynamic_controller_exit(void)
{
	platform_driver_unregister(&boost_dynamic_controller_driver);
}
module_exit(boost_dynamic_controller_exit);

MODULE_DESCRIPTION("Boost dynamic controller driver");
MODULE_LICENSE("GPL v2");
