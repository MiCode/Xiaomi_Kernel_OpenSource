/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include "tsens.h"

LIST_HEAD(tsens_device_list);

static int tsens_get_temp(struct tsens_sensor *s, int *temp)
{
	struct tsens_device *tmdev = s->tmdev;

	return tmdev->ops->get_temp(s, temp);
}

static int tsens_set_trip_temp(struct tsens_sensor *s, int trip, int temp)
{
	struct tsens_device *tmdev = s->tmdev;

	if (tmdev->ops->set_trip_temp)
		return tmdev->ops->set_trip_temp(s, trip, temp);

	return 0;
}

static int tsens_init(struct tsens_device *tmdev)
{
	if (tmdev->ops->hw_init)
		return tmdev->ops->hw_init(tmdev);

	return 0;
}

static int tsens_register_interrupts(struct tsens_device *tmdev)
{
	if (tmdev->ops->interrupts_reg)
		return tmdev->ops->interrupts_reg(tmdev);

	return 0;
}

static const struct of_device_id tsens_table[] = {
	{	.compatible = "qcom,msm8996-tsens",
		.data = &data_tsens2xxx,
	},
	{	.compatible = "qcom,msm8953-tsens",
		.data = &data_tsens2xxx,
	},
	{	.compatible = "qcom,msm8998-tsens",
		.data = &data_tsens2xxx,
	},
	{	.compatible = "qcom,msmhamster-tsens",
		.data = &data_tsens2xxx,
	},
	{	.compatible = "qcom,sdm660-tsens",
		.data = &data_tsens23xx,
	},
	{	.compatible = "qcom,sdm630-tsens",
		.data = &data_tsens23xx,
	},
	{	.compatible = "qcom,sdm845-tsens",
		.data = &data_tsens24xx,
	},
	{}
};
MODULE_DEVICE_TABLE(of, tsens_table);

static struct thermal_zone_of_device_ops tsens_tm_thermal_zone_ops = {
	.get_temp = tsens_get_temp,
	.set_trip_temp = tsens_set_trip_temp,
};

static int get_device_tree_data(struct platform_device *pdev,
				struct tsens_device *tmdev)
{
	struct device_node *of_node = pdev->dev.of_node;
	u32 *hw_id, *client_id;
	u32 rc = 0, i, tsens_num_sensors = 0;
	int tsens_len;
	const struct of_device_id *id;
	const struct tsens_data *data;
	struct resource *res_tsens_mem, *res_mem = NULL;

	if (!of_match_node(tsens_table, of_node)) {
		pr_err("Need to read SoC specific fuse map\n");
		return -ENODEV;
	}

	id = of_match_node(tsens_table, of_node);
	if (id == NULL) {
		pr_err("can not find tsens_table of_node\n");
		return -ENODEV;
	}

	data = id->data;
	hw_id = devm_kzalloc(&pdev->dev,
		tsens_num_sensors * sizeof(u32), GFP_KERNEL);
	if (!hw_id)
		return -ENOMEM;

	client_id = devm_kzalloc(&pdev->dev,
		tsens_num_sensors * sizeof(u32), GFP_KERNEL);
	if (!client_id)
		return -ENOMEM;

	tmdev->ops = data->ops;
	tmdev->ctrl_data = data;
	tmdev->pdev = pdev;

	if (!tmdev->ops || !tmdev->ops->hw_init || !tmdev->ops->get_temp) {
		pr_err("Invalid ops\n");
		return -EINVAL;
	}

	/* TSENS register region */
	res_tsens_mem = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "tsens_physical");
	if (!res_tsens_mem) {
		pr_err("Could not get tsens physical address resource\n");
		return -EINVAL;
	}

	tsens_len = res_tsens_mem->end - res_tsens_mem->start + 1;

	res_mem = request_mem_region(res_tsens_mem->start,
				tsens_len, res_tsens_mem->name);
	if (!res_mem) {
		pr_err("Request tsens physical memory region failed\n");
		return -EINVAL;
	}

	tmdev->tsens_addr = ioremap(res_mem->start, tsens_len);
	if (!tmdev->tsens_addr) {
		pr_err("Failed to IO map TSENS registers.\n");
		return -EINVAL;
	}

	rc = of_property_read_u32_array(of_node,
		"qcom,sensor-id", hw_id, tsens_num_sensors);
	if (rc) {
		pr_err("Default sensor id mapping\n");
		for (i = 0; i < tsens_num_sensors; i++)
			tmdev->sensor[i].hw_id = i;
	} else {
		pr_err("Use specified sensor id mapping\n");
		for (i = 0; i < tsens_num_sensors; i++)
			tmdev->sensor[i].hw_id = hw_id[i];
	}

	rc = of_property_read_u32_array(of_node,
		"qcom,client-id", client_id, tsens_num_sensors);
	if (rc) {
		for (i = 0; i < tsens_num_sensors; i++)
			tmdev->sensor[i].id = i;
		pr_debug("Default client id mapping\n");
	} else {
		for (i = 0; i < tsens_num_sensors; i++)
			tmdev->sensor[i].id = client_id[i];
		pr_debug("Use specified client id mapping\n");
	}

	return 0;
}

static int tsens_thermal_zone_register(struct tsens_device *tmdev)
{
	int rc = 0, i = 0;

	for (i = 0; i < tmdev->num_sensors; i++) {
		tmdev->sensor[i].tmdev = tmdev;
		tmdev->sensor[i].tzd = devm_thermal_zone_of_sensor_register(
					&tmdev->pdev->dev, i, &tmdev->sensor[i],
					&tsens_tm_thermal_zone_ops);
		if (IS_ERR(tmdev->sensor[i].tzd)) {
			pr_err("Error registering sensor:%d\n", i);
			continue;
		}
	}

	return rc;
}

static int tsens_tm_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);

	return 0;
}

int tsens_tm_probe(struct platform_device *pdev)
{
	struct device_node *of_node = pdev->dev.of_node;
	struct tsens_device *tmdev = NULL;
	u32 tsens_num_sensors = 0;
	int rc;

	if (!(pdev->dev.of_node))
		return -ENODEV;

	rc = of_property_read_u32(of_node,
			"qcom,sensors", &tsens_num_sensors);
	if (rc || (!tsens_num_sensors)) {
		dev_err(&pdev->dev, "missing sensors\n");
		return -ENODEV;
	}

	tmdev = devm_kzalloc(&pdev->dev,
			sizeof(struct tsens_device) +
			tsens_num_sensors *
			sizeof(struct tsens_sensor),
			GFP_KERNEL);
	if (tmdev == NULL) {
		pr_err("%s: kzalloc() failed.\n", __func__);
		return -ENOMEM;
	}

	tmdev->num_sensors = tsens_num_sensors;

	rc = get_device_tree_data(pdev, tmdev);
	if (rc) {
		pr_err("Error reading TSENS DT\n");
		return rc;
	}

	rc = tsens_init(tmdev);
	if (rc)
		return rc;

	rc = tsens_thermal_zone_register(tmdev);
	if (rc) {
		pr_err("Error registering the thermal zone\n");
		return rc;
	}

	rc = tsens_register_interrupts(tmdev);
	if (rc < 0) {
		pr_err("TSENS interrupt register failed:%d\n", rc);
		return rc;
	}

	list_add_tail(&tmdev->list, &tsens_device_list);
	platform_set_drvdata(pdev, tmdev);

	return rc;
}

static struct platform_driver tsens_tm_driver = {
	.probe = tsens_tm_probe,
	.remove = tsens_tm_remove,
	.driver = {
		.name = "msm-tsens",
		.owner = THIS_MODULE,
		.of_match_table = tsens_table,
	},
};

int __init tsens_tm_init_driver(void)
{
	return platform_driver_register(&tsens_tm_driver);
}
subsys_initcall(tsens_tm_init_driver);

static void __exit tsens_tm_deinit(void)
{
	platform_driver_unregister(&tsens_tm_driver);
}
module_exit(tsens_tm_deinit);

MODULE_ALIAS("platform:" TSENS_DRIVER_NAME);
MODULE_LICENSE("GPL v2");
