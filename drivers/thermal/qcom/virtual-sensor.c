// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/thermal.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/module.h>

#ifndef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif
#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

#define VIRTUAL_SENSOR_DRIVER "virtual-sensor"

enum aggr_logic {
	VIRT_MINIMUM,
	VIRT_MAXIMUM,
};

struct virtual_sensor_data {
	struct thermal_zone_device	*tz_dev;
	struct device		    *dev;
	int				num_sensors;
	int32_t			   last_reading;
	enum aggr_logic		   logic;
	int				sensor_id;
	struct sensor_names *sensors_name;
	struct thermal_zone_device	*tz[0];
};

struct sensor_names {
	const char *sensor_name;
};

static int virtual_sensor_read(void *data, int *temp)
{
	struct virtual_sensor_data *vs_sens = (struct virtual_sensor_data *)data;
	int ret = 0, idx = 0, curr_temp = 0;

	curr_temp = (vs_sens->logic == VIRT_MAXIMUM) ? INT_MIN : INT_MAX;

	for (idx = 0; idx < vs_sens->num_sensors; idx++) {
		int sens_temp = 0;

		ret = thermal_zone_get_temp(vs_sens->tz[idx], &sens_temp);
		if (ret) {
			pr_err("virtual zone: sensor[%s] read error:%d\n",
					vs_sens->tz[idx]->type, ret);
			return ret;
		}

		switch (vs_sens->logic) {
		case VIRT_MINIMUM:
			curr_temp = MIN(curr_temp, sens_temp);
			break;
		case VIRT_MAXIMUM:
			curr_temp = MAX(curr_temp, sens_temp);
			break;
		default:
			break;
		}
	}
	vs_sens->last_reading = *temp = curr_temp;

	return 0;
}

static struct thermal_zone_of_device_ops virtual_sensor_ops = {
	.get_temp = virtual_sensor_read,
};

static int virtual_sensor_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *subsys_np = NULL;
	struct virtual_sensor_data *vs_sens;
	struct thermal_zone_device *tzd;
	int ret, sensors_cnt = 0, idx = 0, sens_id = 0;
	u32 res;

	for_each_available_child_of_node(np, subsys_np) {
		ret = of_property_count_strings(subsys_np, "sensor-names");
		if (ret < 0) {
			dev_err(dev, "Couldn't get sensor names. %d\n", ret);
			return ret;
		}

		sensors_cnt = ret;
		vs_sens = devm_kzalloc(dev, struct_size(vs_sens, tz, sensors_cnt), GFP_KERNEL);
		if (!vs_sens)
			return -ENOMEM;

		ret = of_property_read_u32(subsys_np, "qcom,logic", &res);
		if (ret < 0)
			return ret;

		vs_sens->logic = res;
		vs_sens->num_sensors = sensors_cnt;
		vs_sens->dev = dev;
		vs_sens->sensor_id = sens_id;
		vs_sens->sensors_name = devm_kcalloc(dev, sensors_cnt + 1,
							sizeof(*vs_sens->sensors_name), GFP_KERNEL);
		if (!vs_sens->sensors_name)
			return -ENOMEM;

		for (idx = 0; idx < vs_sens->num_sensors; idx++) {
			ret = of_property_read_string_index(subsys_np,
							"sensor-names",
							idx,
							&vs_sens->sensors_name[idx].sensor_name);
			if (ret < 0) {
				dev_err(dev, "Couldn't get sensor name index: %d. %d\n",
								idx, ret);
			}

			tzd = thermal_zone_get_zone_by_name(vs_sens->sensors_name[idx].sensor_name);
			if (IS_ERR(tzd)) {
				ret = PTR_ERR(tzd);
				pr_err("No thermal zone found for sensor:%s. err:%d\n",
							vs_sens->sensors_name[idx].sensor_name,
							ret);
				return ret;
			}
			vs_sens->tz[idx] = tzd;
		}
		vs_sens->tz_dev = devm_thermal_zone_of_sensor_register(&pdev->dev,
							vs_sens->sensor_id,
							vs_sens,
							&virtual_sensor_ops);
		if (IS_ERR(vs_sens->tz_dev)) {
			ret = IS_ERR(vs_sens->tz_dev);
			if (ret != -ENODEV)
				dev_err(dev, "sensor register failed. ret:%d\n", ret);
			vs_sens->tz_dev = NULL;
			return ret;
		}
		sens_id++;
	}

	return 0;
}

static const struct of_device_id virtual_sensor_match[] = {
	{.compatible = "qcom,vs-sensor"},
	{}
};

static struct platform_driver virtual_sensor_driver = {
	.probe  = virtual_sensor_probe,
	.driver = {
		.name = VIRTUAL_SENSOR_DRIVER,
		.of_match_table = virtual_sensor_match,
	},
};
module_platform_driver(virtual_sensor_driver);
MODULE_DESCRIPTION("QTI VIRTUAL SENSOR driver");
