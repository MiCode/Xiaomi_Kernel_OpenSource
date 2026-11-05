// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024-2025, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/soc/qcom/llcc-qcom.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/scmi_protocol.h>
#include <linux/qcom_scmi_vendor.h>
#include <soc/qcom/llcc_heuristics.h>
#include <linux/of.h>

static struct scmi_protocol_handle *ph;
static const struct qcom_scmi_vendor_ops *ops;
static int number_of_domains, heur_scid;

static int qcom_llcc_set_params(struct scid_heuristics_params *param, u32 param_id)
{
	return ops->set_param(ph, param, SCID_HEURISTICS_SCMI_STR, param_id,
				sizeof(struct scid_heuristics_params));
}

static int qcom_llcc_set_params_v2(struct scid_heuristics_params_v2 *param, u32 param_id)
{
	return ops->set_param(ph, param, SCID_HEURISTICS_SCMI_STR, param_id,
				sizeof(struct scid_heuristics_params_v2));
}

static int heuristics_set_param(struct platform_device *pdev, bool flag)
{
	int ret;
	struct device_node *node;
	struct scid_heuristics_data *heuristics_data;
	struct scid_heuristics_params *heuristics_param;

	heuristics_data = devm_kzalloc(&pdev->dev,
					sizeof(struct scid_heuristics_data), GFP_KERNEL);
	if (heuristics_data == NULL) {
		return dev_err_probe(&pdev->dev, -ENOMEM,
				"Failed to alloc memory for heuristics_data\n");
	}

	heuristics_param = &heuristics_data->params;
	if (number_of_domains > ARRAY_SIZE(heuristics_param->freq_idx))
		return dev_err_probe(&pdev->dev, -EINVAL,
				"Missing Nodes for qcom,freq-threshold-idx!\n");

	heuristics_param->scid_heuristics_enabled = flag ? 1 : 0;

	if (!(heuristics_param->scid_heuristics_enabled)) {
		ret =  qcom_llcc_set_params(heuristics_param, SCID_ACTIVATION_CONTROL);
		if (ret < 0)
			return dev_err_probe(&pdev->dev, ret,
						"llcc_heuristics scmi set_param failed!\n");
		return 0;
	}
	heuristics_param->heuristics_scid = heur_scid;

	node = pdev->dev.of_node;
	for (int dom_idx = 0; dom_idx < number_of_domains; dom_idx++) {
		ret = of_property_read_u32_index(node, "qcom,freq-threshold-idx", dom_idx,
							&(heuristics_param->freq_idx[dom_idx]));
		if (ret)
			return dev_err_probe(&pdev->dev, ret,
					"Missing index %d qcom,freq-threshold-idx\n",
					dom_idx);

		ret = of_property_read_u32_index(node, "qcom,frequency-threshold-residency",
				dom_idx, &(heuristics_param->freq_idx_residency[dom_idx]));
		if (ret)
			return dev_err_probe(&pdev->dev, ret,
					"Missing index %d qcom,frequency-threshold-residency\n",
					dom_idx);
	}

	if (heuristics_param->scid_heuristics_enabled) {
		ret = qcom_llcc_set_params(heuristics_param, HEURISTICS_INIT);
		if (ret < 0)
			return dev_err_probe(&pdev->dev, ret,
						"llcc_heuristics scmi set_param failed!\n");
	}

	platform_set_drvdata(pdev, heuristics_data);

	return 0;
}

static int heuristics_set_param_v2(struct platform_device *pdev, bool flag)
{
	int ret;
	struct device_node *node;
	struct scid_heuristics_data_v2 *heuristics_data;
	struct scid_heuristics_params_v2 *heuristics_param;

	heuristics_data = devm_kzalloc(&pdev->dev,
					sizeof(struct scid_heuristics_data_v2), GFP_KERNEL);
	if (heuristics_data == NULL) {
		return dev_err_probe(&pdev->dev, -ENOMEM,
				"Failed to alloc memory for heuristics_data\n");
	}

	heuristics_param = &heuristics_data->params;
	if (number_of_domains > ARRAY_SIZE(heuristics_param->freq_idx))
		return dev_err_probe(&pdev->dev, -EINVAL,
				"Missing Nodes for qcom,freq-threshold-idx!\n");

	heuristics_param->scid_heuristics_enabled = flag ? 1 : 0;

	if (!(heuristics_param->scid_heuristics_enabled)) {
		ret =  qcom_llcc_set_params_v2(heuristics_param, SCID_ACTIVATION_CONTROL);
		if (ret < 0)
			return dev_err_probe(&pdev->dev, ret,
						"llcc_heuristics scmi set_param failed!\n");
		return 0;
	}
	heuristics_param->heuristics_scid = heur_scid;

	node = pdev->dev.of_node;
	for (int dom_idx = 0; dom_idx < number_of_domains; dom_idx++) {
		ret = of_property_read_u32_index(node, "qcom,freq-threshold-idx", dom_idx,
							&(heuristics_param->freq_idx[dom_idx]));
		if (ret)
			return dev_err_probe(&pdev->dev, ret,
					"Missing index %d qcom,freq-threshold-idx\n",
					dom_idx);

		ret = of_property_read_u32_index(node, "qcom,frequency-threshold-residency",
				dom_idx, &(heuristics_param->freq_idx_residency[dom_idx]));
		if (ret)
			return dev_err_probe(&pdev->dev, ret,
					"Missing index %d qcom,frequency-threshold-residency\n",
					dom_idx);
	}

	if (heuristics_param->scid_heuristics_enabled) {
		ret = qcom_llcc_set_params_v2(heuristics_param, HEURISTICS_INIT);
		if (ret < 0)
			return dev_err_probe(&pdev->dev, ret,
						"llcc_heuristics scmi set_param failed!\n");
	}

	platform_set_drvdata(pdev, heuristics_data);

	return 0;
}

static int heuristics_scid_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *node;
	struct scmi_device *scmi_dev;
	bool flag;

	scmi_dev = get_qcom_scmi_device();
	if (IS_ERR(scmi_dev))
		return PTR_ERR(scmi_dev);

	ops = scmi_dev->handle->devm_protocol_get(scmi_dev, QCOM_SCMI_VENDOR_PROTOCOL, &ph);
	if (IS_ERR(ops))
		return dev_err_probe(&pdev->dev, PTR_ERR(ops),
				"Error getting vendor protocol ops\n");

	node = pdev->dev.of_node;
	ret = of_property_read_u32(node, "qcom,heuristics-scid", &heur_scid);
	if (ret)
		return dev_err_probe(&pdev->dev, ret,
					"Missing Node value qcom,heuristics_scid!\n");

	number_of_domains = of_property_count_u32_elems(node, "qcom,freq-threshold-idx");
	flag = of_property_read_bool(node, "qcom,scid-heuristics-enabled");

	if (of_property_match_string(pdev->dev.parent->of_node,
				"compatible", "qcom,sun-llcc") >= 0) {
		heuristics_set_param(pdev, flag);
	} else {
		heuristics_set_param_v2(pdev, flag);
	}

	return ret;
}

static int heuristics_scid_remove(struct platform_device *pdev)
{
	bool flag = false;

	/* Disable Heuristics Thread on module exit */
	if (of_property_match_string(pdev->dev.parent->of_node,
				"compatible", "qcom,sun-llcc") >= 0) {
		heuristics_set_param(pdev, flag);
	} else {
		heuristics_set_param_v2(pdev, flag);
	}

	platform_set_drvdata(pdev, NULL);
	return 0;
}

static const struct of_device_id heuristics_scid_table[] = {
	{ .compatible = "qcom,scid-heuristics" },
	{}
};

MODULE_DEVICE_TABLE(of, heuristics_scid_table);

static struct platform_driver heuristics_scid_driver = {
	.driver = {
		.name = "scid-heuristics",
		.of_match_table = heuristics_scid_table,
	},
	.probe = heuristics_scid_probe,
	.remove = heuristics_scid_remove,
};

module_platform_driver(heuristics_scid_driver);

MODULE_SOFTDEP("pre: llcc_qcom");
MODULE_SOFTDEP("pre: qcom_scmi_client");
MODULE_DESCRIPTION("QCOM HEURISTICS SCID driver");
MODULE_LICENSE("GPL");
