// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
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
#include "thermal_core.h"

LIST_HEAD(tsens_device_list);

static int tsens_get_temp(void *data, int *temp)
{
	struct tsens_sensor *s = data;
	struct tsens_device *tmdev = s->tmdev;

	return tmdev->ops->get_temp(s, temp);
}

static int tsens_get_zeroc_status(void *data, int *status)
{
	struct tsens_sensor *s = data;

	return tsens_2xxx_get_zeroc_status(s, status);
}

static int tsens_set_trip_temp(void *data, int low_temp, int high_temp)
{
	struct tsens_sensor *s = data;
	struct tsens_device *tmdev = s->tmdev;

	if (tmdev->ops->set_trips)
		return tmdev->ops->set_trips(s, low_temp, high_temp);

	return 0;
}

static int tsens_init(struct tsens_device *tmdev)
{
	return tmdev->ops->hw_init(tmdev);
}

static int tsens_calib(struct tsens_device *tmdev)
{
	return tmdev->ops->calibrate(tmdev);
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
	{	.compatible = "qcom,sm6150-tsens",
		.data = &data_tsens23xx,
	},
	{	.compatible = "qcom,sdm845-tsens",
		.data = &data_tsens24xx,
	},
	{	.compatible = "qcom,tsens24xx",
		.data = &data_tsens24xx,
	},
	{	.compatible = "qcom,tsens26xx",
		.data = &data_tsens26xx,
	},
	{	.compatible = "qcom,msm8937-tsens",
		.data = &data_tsens14xx,
	},
	{	.compatible = "qcom,qcs405-tsens",
		.data = &data_tsens14xx_405,
	},
	{}
};
MODULE_DEVICE_TABLE(of, tsens_table);

static struct thermal_zone_of_device_ops tsens_tm_thermal_zone_ops = {
	.get_temp = tsens_get_temp,
	.set_trips = tsens_set_trip_temp,
};

static struct thermal_zone_of_device_ops tsens_tm_min_thermal_zone_ops = {
	.get_temp = tsens_get_zeroc_status,
};

static int get_device_tree_data(struct platform_device *pdev,
				struct tsens_device *tmdev)
{
	struct device_node *of_node = pdev->dev.of_node;
	const struct of_device_id *id;
	const struct tsens_data *data;
	int rc = 0;
	struct resource *res_tsens_mem;
	u32 zeroc_id;

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
	tmdev->ops = data->ops;
	tmdev->ctrl_data = data;
	tmdev->pdev = pdev;
	tmdev->dev = &pdev->dev;

	if (!tmdev->ops || !tmdev->ops->hw_init || !tmdev->ops->get_temp) {
		pr_err("Invalid ops\n");
		return -EINVAL;
	}

	/* TSENS register region */
	res_tsens_mem = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "tsens_srot_physical");
	if (!res_tsens_mem) {
		pr_err("Could not get tsens physical address resource\n");
		return -EINVAL;
	}

	tmdev->tsens_srot_addr = devm_ioremap_resource(&pdev->dev,
							res_tsens_mem);
	if (IS_ERR(tmdev->tsens_srot_addr)) {
		dev_err(&pdev->dev, "Failed to IO map TSENS registers.\n");
		return PTR_ERR(tmdev->tsens_srot_addr);
	}

	/* TSENS TM register region */
	res_tsens_mem = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "tsens_tm_physical");
	if (!res_tsens_mem) {
		pr_err("Could not get tsens physical address resource\n");
		return -EINVAL;
	}

	tmdev->tsens_tm_addr = devm_ioremap_resource(&pdev->dev,
								res_tsens_mem);
	if (IS_ERR(tmdev->tsens_tm_addr)) {
		dev_err(&pdev->dev, "Failed to IO map TSENS TM registers.\n");
		return PTR_ERR(tmdev->tsens_tm_addr);
	}

	tmdev->phys_addr_tm = res_tsens_mem->start;

	/* TSENS eeprom register region */
	res_tsens_mem = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "tsens_eeprom_physical");
	if (!res_tsens_mem) {
		pr_debug("Could not get tsens physical address resource\n");
	} else {
		tmdev->tsens_calib_addr = devm_ioremap_resource(&pdev->dev,
								res_tsens_mem);
		if (IS_ERR(tmdev->tsens_calib_addr)) {
			dev_err(&pdev->dev, "Failed to IO map TSENS EEPROM registers.\n");
			rc = PTR_ERR(tmdev->tsens_calib_addr);
		}  else {
			rc = tsens_calib(tmdev);
			if (rc) {
				pr_err("Error initializing TSENS controller\n");
				return rc;
			}
		}
	}

	if (!of_property_read_u32(of_node, "0C-sensor-num", &zeroc_id))
		tmdev->zeroc_sensor_id = (int)zeroc_id;
	else
		tmdev->zeroc_sensor_id = MIN_TEMP_DEF_OFFSET;

	tmdev->tsens_reinit_wa =
		of_property_read_bool(of_node, "tsens-reinit-wa");
	return rc;
}

static int tsens_thermal_zone_register(struct tsens_device *tmdev)
{
	int i = 0, sensor_missing = 0;

	for (i = 0; i < TSENS_MAX_SENSORS; i++) {
		tmdev->sensor[i].tmdev = tmdev;
		tmdev->sensor[i].hw_id = i;
		if (tmdev->ops->sensor_en(tmdev, i)) {
			tmdev->sensor[i].tzd =
				devm_thermal_zone_of_sensor_register(
				&tmdev->pdev->dev, i,
				&tmdev->sensor[i], &tsens_tm_thermal_zone_ops);
			if (IS_ERR(tmdev->sensor[i].tzd)) {
				pr_debug("Error registering sensor:%d\n", i);
				sensor_missing++;
				continue;
			}
		} else {
			pr_debug("Sensor not enabled:%d\n", i);
		}
	}

	if (sensor_missing == TSENS_MAX_SENSORS) {
		pr_err("No TSENS sensors to register?\n");
		return -ENODEV;
	}

	if (tmdev->zeroc_sensor_id != MIN_TEMP_DEF_OFFSET) {
		tmdev->zeroc.tmdev = tmdev;
		tmdev->zeroc.hw_id = tmdev->zeroc_sensor_id;
		tmdev->zeroc.tzd =
			devm_thermal_zone_of_sensor_register(
			&tmdev->pdev->dev, tmdev->zeroc_sensor_id,
			&tmdev->zeroc, &tsens_tm_min_thermal_zone_ops);
		if (IS_ERR(tmdev->zeroc.tzd))
			pr_err("Error registering min temp sensor\n");
	}


	return 0;
}

static int tsens_tm_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static void tsens_therm_fwk_notify(struct work_struct *work)
{
	int i, rc, temp;
	struct tsens_device *tmdev =
		container_of(work, struct tsens_device, therm_fwk_notify);

	TSENS_DBG(tmdev, "Controller %pK\n", &tmdev->phys_addr_tm);
	for (i = 0; i < TSENS_MAX_SENSORS; i++) {
		if (tmdev->ops->sensor_en(tmdev, i)) {
			rc = tsens_get_temp(&tmdev->sensor[i], &temp);
			if (rc) {
				pr_err("%s: Error:%d reading temp sensor:%d\n",
					__func__, rc, i);
				continue;
			}
			TSENS_DBG(tmdev, "Calling trip_temp for sensor %d\n",
					i);
			of_thermal_handle_trip(tmdev->dev, tmdev->sensor[i].tzd);
		}
	}
	if (tmdev->zeroc_sensor_id != MIN_TEMP_DEF_OFFSET) {
		rc = tsens_get_zeroc_status(&tmdev->zeroc, &temp);
		if (rc) {
			pr_err("%s: Error:%d reading temp sensor:%d\n",
				   __func__, rc, i);
			return;
		}
		TSENS_DBG(tmdev, "Calling trip_temp for sensor %d\n", i);
		of_thermal_handle_trip(tmdev->dev, tmdev->zeroc.tzd);
	}
}

static int tsens_tm_probe(struct platform_device *pdev)
{
	struct tsens_device *tmdev = NULL;
	int rc;
	char tsens_name[40];

	if (!(pdev->dev.of_node))
		return -ENODEV;

	tmdev = devm_kzalloc(&pdev->dev,
			sizeof(struct tsens_device) +
			TSENS_MAX_SENSORS *
			sizeof(struct tsens_sensor),
			GFP_KERNEL);
	if (tmdev == NULL)
		return -ENOMEM;

	rc = get_device_tree_data(pdev, tmdev);
	if (rc) {
		pr_err("Error reading TSENS DT\n");
		return rc;
	}

	rc = tsens_init(tmdev);
	if (rc) {
		pr_err("Error initializing TSENS controller\n");
		return rc;
	}

	snprintf(tsens_name, sizeof(tsens_name), "tsens_wq_%pa",
		&tmdev->phys_addr_tm);

	tmdev->tsens_reinit_work = alloc_workqueue(tsens_name,
		WQ_HIGHPRI, 0);
	if (!tmdev->tsens_reinit_work) {
		rc = -ENOMEM;
		return rc;
	}
	INIT_WORK(&tmdev->therm_fwk_notify, tsens_therm_fwk_notify);
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

	snprintf(tsens_name, sizeof(tsens_name), "tsens_%pa_0",
					&tmdev->phys_addr_tm);

	tmdev->ipc_log0 = ipc_log_context_create(IPC_LOGPAGES,
							tsens_name, 0);
	if (!tmdev->ipc_log0)
		pr_err("%s : unable to create IPC Logging 0 for tsens %pa\n",
					__func__, &tmdev->phys_addr_tm);

	snprintf(tsens_name, sizeof(tsens_name), "tsens_%pa_1",
					&tmdev->phys_addr_tm);

	tmdev->ipc_log1 = ipc_log_context_create(IPC_LOGPAGES,
							tsens_name, 0);
	if (!tmdev->ipc_log1)
		pr_err("%s : unable to create IPC Logging 1 for tsens %pa\n",
					__func__, &tmdev->phys_addr_tm);

	snprintf(tsens_name, sizeof(tsens_name), "tsens_%pa_2",
					&tmdev->phys_addr_tm);

	tmdev->ipc_log2 = ipc_log_context_create(IPC_LOGPAGES,
							tsens_name, 0);
	if (!tmdev->ipc_log2)
		pr_err("%s : unable to create IPC Logging 2 for tsens %pa\n",
					__func__, &tmdev->phys_addr_tm);

	list_add_tail(&tmdev->list, &tsens_device_list);
	platform_set_drvdata(pdev, tmdev);

	return rc;
}

static struct platform_driver tsens_tm_driver = {
	.probe = tsens_tm_probe,
	.remove = tsens_tm_remove,
	.driver = {
		.name = "msm-tsens",
		.of_match_table = tsens_table,
	},
};

static int __init tsens_tm_init_driver(void)
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
