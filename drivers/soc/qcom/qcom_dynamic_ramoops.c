// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/pstore_ram.h>

#define MEM_TYPE_NORMAL         2

/*
 * Read a u32 from a dt property and make sure it's safe for an int.
 * This function is taken from fs/pstore/ram.c
 */
static int ramoops_parse_dt_u32(struct platform_device *pdev,
				const char *propname,
				u32 default_value, u32 *value)
{
	u32 val32 = 0;
	int ret;

	ret = of_property_read_u32(pdev->dev.of_node, propname, &val32);
	if (ret == -EINVAL) {
		/* field is missing, use default value. */
		val32 = default_value;
	} else if (ret < 0) {
		dev_err(&pdev->dev, "failed to parse property %s: %d\n",
			propname, ret);
		return ret;
	}

	/* Sanity check our results. */
	if (val32 > INT_MAX) {
		dev_err(&pdev->dev, "%s %u > INT_MAX\n", propname, val32);
		return -EOVERFLOW;
	}

	*value = val32;

	return 0;
}

static int qcom_ramoops_probe(struct platform_device *pdev)
{
	struct device_node *of_node = pdev->dev.of_node;
	struct platform_device *ramoops_pdev;
	struct ramoops_platform_data *pdata;
	struct device_node *node;
	struct reserved_mem *rmem;
	long ret = 0;
	u32 value;

	node = of_parse_phandle(of_node, "memory-region", 0);
	if (!node)
		return -ENODEV;

	rmem = of_reserved_mem_lookup(node);
	of_node_put(node);
	if (!rmem) {
		dev_err(&pdev->dev, "failed to locate DT /reserved-memory resource\n");
		return -EINVAL;
	}

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->mem_size = rmem->size;
	pdata->mem_address = rmem->base;
	pdata->max_reason = KMSG_DUMP_PANIC;
	pdata->mem_type = MEM_TYPE_NORMAL;

#define parse_u32(name, field, default_value) {				\
		ret = ramoops_parse_dt_u32(pdev, name, default_value,	\
					   &value);			\
		if (ret < 0)						\
			return ret;					\
		field = value;						\
	}

	parse_u32("mem-type", pdata->mem_type, pdata->mem_type);
	parse_u32("record-size", pdata->record_size, 0);
	parse_u32("console-size", pdata->console_size, 0);
	parse_u32("ftrace-size", pdata->ftrace_size, 0);
	parse_u32("pmsg-size", pdata->pmsg_size, 0);
	parse_u32("ecc-size", pdata->ecc_info.ecc_size, 0);
	parse_u32("flags", pdata->flags, 0);
	parse_u32("max-reason", pdata->max_reason, pdata->max_reason);

#undef parse_u32
	ramoops_pdev = platform_device_register_data(NULL, "ramoops", -1,
						     pdata, sizeof(*pdata));
	if (IS_ERR(ramoops_pdev)) {
		ret = PTR_ERR(ramoops_pdev);
		dev_err(&pdev->dev, "could not create platform device: %ld\n", ret);
		ramoops_pdev = NULL;
	}

	platform_set_drvdata(pdev, ramoops_pdev);

	return ret;
}

static int qcom_ramoops_remove(struct platform_device *pdev)
{
	struct platform_device *ramoops_pdev;

	ramoops_pdev = platform_get_drvdata(pdev);
	platform_device_unregister(ramoops_pdev);

	return 0;
}

static const struct of_device_id qcom_ramoops_of_match[] = {
	{ .compatible = "qcom,ramoops"},
	{}
};

MODULE_DEVICE_TABLE(of, qcom_ramoops_of_match);
static struct platform_driver qcom_ramoops_drv = {
	.driver		= {
		.name	= "qcom,ramoops",
		.of_match_table = qcom_ramoops_of_match,
	},
	.probe = qcom_ramoops_probe,
	.remove = qcom_ramoops_remove,
};

module_platform_driver(qcom_ramoops_drv);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. dynamic ramoops platform device driver support");
MODULE_LICENSE("GPL");
