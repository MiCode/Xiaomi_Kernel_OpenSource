// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */
 #define pr_fmt(fmt) "qcom_mpam: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/qcom_scmi_vendor.h>
#include <linux/scmi_protocol.h>
#include <soc/qcom/mpam.h>

#define MPAM_ALGO_STR	0x4D50414D4558544E  /* "MPAMEXTN" */

/* Parameter IDs for SET */
enum mpam_set_param_ids {
	/* CPU MPAM ID - L2 CACHE */
	PARAM_SET_CACHE_PARTITION = 1,
	PARAM_SET_CONFIG_MONITOR = 2,
	PARAM_SET_CAPTURE_ALL_MONITOR = 3,
	/*PLATFORM MPAM ID - NOC */
	PARAM_SET_PLATFORM_BW_CTRL = 4,
	PARAM_SET_PLATFORM_BW_MONITOR = 5
};

/* Parameter IDs for GET */
enum mpam_get_param_ids {
	PARAM_GET_MPAM_VERSION = 1,
	PARAM_GET_CACHE_PARTITION = 2,
	PARAM_GET_PLATFORM_BW_CTRL_CONFIG = 3
};

static struct scmi_protocol_handle *ph;
static const struct qcom_scmi_vendor_ops *ops;
static struct scmi_device *sdev;

int qcom_mpam_set_cache_partition(struct mpam_set_cache_partition *param)
{
	if (!ops)
		return -EPERM;

	return ops->set_param(ph, param, MPAM_ALGO_STR,
			PARAM_SET_CACHE_PARTITION,
			sizeof(struct mpam_set_cache_partition));
}
EXPORT_SYMBOL_GPL(qcom_mpam_set_cache_partition);

int qcom_mpam_get_version(struct mpam_ver_ret *ver)
{
	if (!ops)
		return -EPERM;

	return ops->get_param(ph, ver, MPAM_ALGO_STR,
			PARAM_GET_MPAM_VERSION, 0,
			sizeof(struct mpam_ver_ret));
}
EXPORT_SYMBOL_GPL(qcom_mpam_get_version);

int qcom_mpam_get_cache_partition(struct mpam_read_cache_portion *param,
						struct mpam_config_val *val)
{
	int ret;
	uint8_t buf[32];

	if (!ops)
		return -EPERM;

	memcpy(buf, param, sizeof(struct mpam_read_cache_portion));
	ret = ops->get_param(ph, buf, MPAM_ALGO_STR,
			PARAM_GET_CACHE_PARTITION,
			sizeof(struct mpam_read_cache_portion),
			sizeof(struct mpam_config_val));

	if (!ret)
		memcpy(val, buf, sizeof(struct mpam_config_val));

	return ret;
}
EXPORT_SYMBOL_GPL(qcom_mpam_get_cache_partition);

int qcom_mpam_config_monitor(struct mpam_monitor_configuration *param)
{
	if (!ops)
		return -EPERM;

	return ops->set_param(ph, param, MPAM_ALGO_STR,
			PARAM_SET_CONFIG_MONITOR,
			sizeof(struct mpam_monitor_configuration));
}
EXPORT_SYMBOL_GPL(qcom_mpam_config_monitor);

int qcom_mpam_set_platform_bw_ctrl(struct platform_mpam_bw_ctrl_cfg *param)
{
	if (!ops)
		return -EPERM;

	return ops->set_param(ph, param, MPAM_ALGO_STR,
			PARAM_SET_PLATFORM_BW_CTRL,
			sizeof(struct platform_mpam_bw_ctrl_cfg));
}
EXPORT_SYMBOL_GPL(qcom_mpam_set_platform_bw_ctrl);

int qcom_mpam_get_platform_bw_ctrl(struct platform_mpam_read_bw_ctrl *param,
						struct platform_mpam_bw_ctrl_config *val)
{
	int ret;
	uint8_t buf[32];

	if (!ops)
		return -EPERM;

	memcpy(buf, param, sizeof(struct platform_mpam_read_bw_ctrl));
	ret = ops->get_param(ph, buf, MPAM_ALGO_STR,
			PARAM_GET_PLATFORM_BW_CTRL_CONFIG,
			sizeof(struct platform_mpam_read_bw_ctrl),
			sizeof(struct platform_mpam_bw_ctrl_config));

	if (!ret)
		memcpy(val, buf, sizeof(struct platform_mpam_bw_ctrl_config));

	return ret;
}
EXPORT_SYMBOL_GPL(qcom_mpam_get_platform_bw_ctrl);

int qcom_mpam_set_platform_bw_monitor(struct platform_mpam_bw_monitor_cfg *param)
{
	if (!ops)
		return -EPERM;

	return ops->set_param(ph, param, MPAM_ALGO_STR,
			PARAM_SET_PLATFORM_BW_MONITOR,
			sizeof(struct platform_mpam_bw_monitor_cfg));
}
EXPORT_SYMBOL_GPL(qcom_mpam_set_platform_bw_monitor);

static int qcom_mpam_probe(struct platform_device *pdev)
{
	int ret;

	sdev = get_qcom_scmi_device();
	if (IS_ERR(sdev)) {
		ret = PTR_ERR(sdev);
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Error getting scmi_dev ret=%d\n", ret);
		return ret;
	}
	ops = sdev->handle->devm_protocol_get(sdev, QCOM_SCMI_VENDOR_PROTOCOL, &ph);
	if (IS_ERR(ops)) {
		ret = PTR_ERR(ops);
		ops = NULL;
		dev_err(&pdev->dev, "Error getting vendor protocol ops: %d\n", ret);
		return ret;
	}

	return 0;
}

static int qcom_mpam_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id qcom_mpam_table[] = {
	{ .compatible = "qcom,mpam" },
	{}
};
MODULE_DEVICE_TABLE(of, qcom_mpam_table);

static struct platform_driver qcom_mpam_driver = {
	.driver = {
		.name = "qcom-mpam",
		.of_match_table = qcom_mpam_table,
		.suppress_bind_attrs = true,
	},
	.probe = qcom_mpam_probe,
	.remove = qcom_mpam_remove,
};

module_platform_driver(qcom_mpam_driver);

MODULE_SOFTDEP("pre: qcom_scmi_client");
MODULE_DESCRIPTION("QCOM MPAM driver");
MODULE_LICENSE("GPL");
