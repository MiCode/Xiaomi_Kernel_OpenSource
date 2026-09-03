// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2020 Spreadtrum Communications Inc.

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include <linux/of.h>
#include <linux/kernel.h>

#define THM_NAME_LENGTH		20
#define DEF_THMZONE		"quiet_therm"
#define NORMAL_TEMP		25000
#define ABNORMAL_TEMP		120000
#define INIT_TEMP		25000
#define RD_TEMP_IDX		3

struct virt_sensor {
	u16 sensor_id;
	int cur_temp;
	size_t nsensor;
	struct sprd_thm_zone *vzone;
	const char **sensor_names;
	struct thermal_zone_device **thm_zones;
};

struct sprd_thm_zone {
	struct thermal_zone_device *therm_dev;
	struct device *dev;
	const struct thermal_zone_of_device_ops *ops;
	char name[THM_NAME_LENGTH];
	int id;
};

static int sprd_get_max_temp(struct virt_sensor *vsensor)
{
	int i = 0, ret = 0;
	int max_temp = 0;
	bool flag = true;
	struct thermal_zone_device *tz = NULL;

	for (; i < vsensor->nsensor; i++) {
		tz = vsensor->thm_zones[i];
		if (!tz || IS_ERR(tz) || !tz->ops->get_temp)
			continue;

		ret = tz->ops->get_temp(tz, &(vsensor->cur_temp));
		max_temp = vsensor->cur_temp > 0 ? max(max_temp, vsensor->cur_temp)
						 : min(max_temp, vsensor->cur_temp);
		if (max_temp < ABNORMAL_TEMP)
			flag = false;
	}

	if (flag && strcmp(DEF_THMZONE, tz->type)) {
		tz = thermal_zone_get_zone_by_name(DEF_THMZONE);
		ret = tz->ops->get_temp(tz, &max_temp);
		if (ret || (max_temp > ABNORMAL_TEMP))
			max_temp = NORMAL_TEMP;
	}
	return max_temp;
}

static int sprd_read_temp(void *devdata, int *temp)
{
	int ret = -EINVAL;
	struct sprd_thm_zone *vzone = devdata;
	struct virt_sensor *vsensor = NULL;

	if (!vzone)
		return ret;
	vsensor = (struct virt_sensor *)dev_get_drvdata(vzone->dev);
	if (!vsensor || !temp)
		return ret;

	*temp = sprd_get_max_temp(vsensor);
	pr_debug("vsensor_id:%d, temp=%d\n", vzone->id, *temp);
	return 0;
}

const struct thermal_zone_of_device_ops virt_thm_ops = {
	.get_temp = sprd_read_temp,
};

static int sprd_parse_dt(struct device *dev, struct virt_sensor *vsensor)
{
	int i, ret = 0;
	size_t count;
	struct device_node *np = dev->of_node;

	if (!np) {
		dev_err(dev, "device node not found\n");
		return -EINVAL;
	}

	count = (size_t)of_property_count_strings(np, "sensor-names");
	if (count < 0) {
		dev_err(dev, "sensor names not found\n");
		return count;
	}

	vsensor->thm_zones = devm_kmalloc_array(dev, count,
						sizeof(struct thermal_zone_device *), GFP_KERNEL);
	vsensor->sensor_names = devm_kmalloc_array(dev, count, sizeof(char *), GFP_KERNEL);
	vsensor->nsensor = count;
	for (i = 0; i < vsensor->nsensor; i++) {
		ret = of_property_read_string_index(np, "sensor-names", i,
						    &vsensor->sensor_names[i]);
		if (ret) {
			dev_err(dev, "fail to get  sensor-names\n");
			return ret;
		}
	}
	return ret;
}

static int sprd_virt_thm_probe(struct platform_device *pdev)
{
	int i = 0;
	int ret = 0, sensor_id = 0;
	struct sprd_thm_zone *vzone = NULL;
	struct virt_sensor *vsensor = NULL;
	struct device_node *np = pdev->dev.of_node;

	if (!np) {
		dev_err(&pdev->dev, "device node not found\n");
		return -EINVAL;
	}

	vsensor = devm_kzalloc(&pdev->dev, sizeof(*vsensor), GFP_KERNEL);
	if (!vsensor)
		return -ENOMEM;

	vsensor->cur_temp = 0;
	ret = sprd_parse_dt(&pdev->dev, vsensor);
	if (ret) {
		dev_err(&pdev->dev, "not found dts node\n");
		return -EINVAL;
	}
	for (i = 0; i < vsensor->nsensor; i++) {
		vsensor->thm_zones[i] = thermal_zone_get_zone_by_name(vsensor->sensor_names[i]);
		if (IS_ERR(vsensor->thm_zones[i])) {
			pr_err("get thermal zone %s failed\n", vsensor->sensor_names[i]);
			return -EPROBE_DEFER;
		}
	}
	vzone = devm_kzalloc(&pdev->dev, sizeof(*vzone), GFP_KERNEL);
	if (!vzone)
		return -ENOMEM;
	vzone->dev = &pdev->dev;
	vzone->id = sensor_id;
	vzone->ops = &virt_thm_ops;
	strncpy(vzone->name, np->name, sizeof(vzone->name));
	vzone->therm_dev = thermal_zone_of_sensor_register(vzone->dev, vzone->id, vzone,
							   &virt_thm_ops);
	if (IS_ERR_OR_NULL(vzone->therm_dev)) {
		pr_err("Register thermal zone device failed.\n");
		return PTR_ERR(vzone->therm_dev);
	};
	vsensor->vzone = vzone;
	platform_set_drvdata(pdev, vsensor);
	dev_info(&pdev->dev, "virt thermal probe success\n");
	return 0;
}

static int sprd_thm_remove(struct platform_device *pdev)
{
	struct virt_sensor *vsensor = platform_get_drvdata(pdev);
	struct sprd_thm_zone *vzone = vsensor->vzone;

	thermal_zone_device_unregister(vzone->therm_dev);
	return 0;
}

static const struct of_device_id virt_thm_of_match[] = {
	{.compatible = "sprd, skin-thermal"},
	{},
};

static struct platform_driver sprd_virt_thm_driver = {
	.probe = sprd_virt_thm_probe,
	.remove = sprd_thm_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "skin-thermal",
		.of_match_table = virt_thm_of_match,
	},
};

module_platform_driver(sprd_virt_thm_driver);
MODULE_LICENSE("GPL");
