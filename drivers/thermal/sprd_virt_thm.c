// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2020 Spreadtrum Communications Inc.

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/thermal.h>

struct virt_sen {
	int sen_id;
	struct device *dev;
	struct thermal_zone_device *thm_dev;
};

struct real_tz_list {
	int temp;
	struct thermal_zone_device *tz_dev;
};

struct virt_sen_data {
	int num;
	struct real_tz_list *tz_list;
	struct virt_sen *v_sen;
};

static int get_thm_zone_counts(struct device *dev)
{
	int count;
	struct device_node *np = dev->of_node;

	if (!np) {
		dev_err(dev, "device node not found\n");
		return -EINVAL;
	}

	if (!of_find_property(np, "thmzone-cells", &count)) {
		dev_err(dev, "thmzone-cells not found\n");
		return -EINVAL;
	}
	count = count / sizeof(u32);

	return count;
}

static int get_thm_zone_device(struct platform_device *pdev)
{
	int i;
	const char *name;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node, *node = NULL;
	struct real_tz_list *tz_list;
	struct virt_sen_data *data = platform_get_drvdata(pdev);

	for (i = 0; i < data->num; i++) {
		node = of_parse_phandle(np, "thmzone-cells", i);
		if (!node) {
			dev_err(dev, "thmzone-cell%d not found\n", i);
			return -EINVAL;
		}
		name = node->name;
		of_node_put(node);
		tz_list = &data->tz_list[i];
		tz_list->tz_dev = thermal_zone_get_zone_by_name(name);
		if (IS_ERR(tz_list->tz_dev)) {
			dev_err(dev, "failed to get thermal zone by name\n");
			return -EINVAL;
		}
	}

	return 0;
}

static int sprd_get_max_temp(void *devdata, int *temp)
{
	int i = 0, ret = 0;
	int max_temp = INT_MIN;
	struct real_tz_list *tz_list = NULL;
	struct virt_sen_data *sen_data = devdata;
	struct device *dev;
	struct thermal_zone_device *tz = NULL;

	if (!sen_data || !temp)
		return -EINVAL;

	dev = sen_data->v_sen->dev;
	for (; i < sen_data->num; i++) {
		tz_list = &sen_data->tz_list[i];
		tz = tz_list->tz_dev;
		if (!tz || IS_ERR(tz) || !tz->ops->get_temp)
			continue;

		ret = tz->ops->get_temp(tz, &tz_list->temp);
		if (ret) {
			dev_err(dev, "fail to get temp\n");
			continue;
		}
		max_temp = max(max_temp, tz_list->temp);
	}
	*temp = max_temp;
	return ret;
}

static const struct thermal_zone_of_device_ops virt_thm_ops = {
	.get_temp = sprd_get_max_temp,
};

static int sprd_virt_thm_probe(struct platform_device *pdev)
{
	int count;
	int ret, sensor_id = 0;
	struct virt_sen_data *data;
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;

	if (!np) {
		dev_err(&pdev->dev, "device node not found\n");
		return -EINVAL;
	}

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	count = get_thm_zone_counts(dev);
	if (count <= 0) {
		dev_err(dev, "failed to get thmzone count\n");
		return -EINVAL;
	}

	data->num = count;
	data->tz_list = devm_kzalloc(dev, sizeof(*data->tz_list) * data->num, GFP_KERNEL);
	if (!data->tz_list)
		return -ENOMEM;

	data->v_sen = devm_kzalloc(dev, sizeof(*data->v_sen), GFP_KERNEL);
	if (!data->v_sen)
		return  -ENOMEM;

	platform_set_drvdata(pdev, data);
	ret = get_thm_zone_device(pdev);
	if (ret)
		return -EPROBE_DEFER;

	data->v_sen->sen_id = sensor_id;
	data->v_sen->dev = dev;
	data->v_sen->thm_dev = devm_thermal_zone_of_sensor_register(dev, data->v_sen->sen_id,
								    data, &virt_thm_ops);
	if (IS_ERR_OR_NULL(data->v_sen->thm_dev)) {
		pr_err("Register thermal zone device failed.\n");
		return PTR_ERR(data->v_sen->thm_dev);
	};
	dev_info(&pdev->dev, "virt thermal probe success\n");
	return 0;
}

static const struct of_device_id virt_thm_of_match[] = {
	{.compatible = "sprd,virt-thm"},
	{},
};

static struct platform_driver sprd_virt_thm_driver = {
	.probe = sprd_virt_thm_probe,
	.driver = {
		.owner = THIS_MODULE,
		.name = "sprd_virt_thermal",
		.of_match_table = virt_thm_of_match,
	},
};

module_platform_driver(sprd_virt_thm_driver);
MODULE_LICENSE("GPL");
