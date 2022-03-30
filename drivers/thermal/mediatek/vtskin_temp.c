// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/bits.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/thermal.h>


#define MAX_REF_NUM 10

struct vtskin_coef {
	char *sensor_name;
	long long sensor_coef;
};

struct vtskin_tz_param {
	unsigned int ref_num;
	struct vtskin_coef vtskin_ref[MAX_REF_NUM];
};

struct vtskin_data {
	struct device *dev;
	int num_sensor;
	struct vtskin_tz_param *params;
};

struct vtskin_temp_tz {
	unsigned int id;
	struct vtskin_data *skin_data;
	struct vtskin_tz_param *skin_param;
};

static int vtskin_get_temp(void *data, int *temp)
{
	struct vtskin_temp_tz *skin_tz = (struct vtskin_temp_tz *)data;
	struct vtskin_data *skin_data = skin_tz->skin_data;
	struct vtskin_tz_param *skin_param = skin_data->params;
	struct thermal_zone_device *tzd;
	long long vtskin = 0, coef;
	int tz_temp, i;
	char *sensor_name;

	for (i = 0; i < skin_param[skin_tz->id].ref_num; i++) {
		sensor_name = skin_param[skin_tz->id].vtskin_ref[i].sensor_name;
		if (!sensor_name) {
			dev_err(skin_data->dev, "get sensor name fail %d\n", i);
			*temp = THERMAL_TEMP_INVALID;
			return -EINVAL;
		}

		tzd = thermal_zone_get_zone_by_name(sensor_name);
		if (IS_ERR_OR_NULL(tzd) || !tzd->ops->get_temp) {
			dev_err(skin_data->dev, "get %s temp fail\n", sensor_name);
			*temp = THERMAL_TEMP_INVALID;
			return -EINVAL;
		}

		tzd->ops->get_temp(tzd, &tz_temp);
		coef = skin_param[skin_tz->id].vtskin_ref[i].sensor_coef;
		vtskin += tz_temp * coef;
	}

	*temp = (int)(vtskin / 100000000);

	return 0;
}

static const struct thermal_zone_of_device_ops vtskin_ops = {
	.get_temp = vtskin_get_temp,
};

static int vtskin_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct vtskin_temp_tz *skin_tz;
	struct vtskin_data *skin_data;
	struct thermal_zone_device *tzdev;
	int i, ret;

	if (!pdev->dev.of_node) {
		dev_err(dev, "Only DT based supported\n");
		return -ENODEV;
	}

	skin_data = (struct vtskin_data *)of_device_get_match_data(dev);
	if (!skin_data)	{
		dev_err(dev, "Error: Failed to get lvts platform data\n");
		return -ENODATA;
	}

	skin_data->dev = dev;
	platform_set_drvdata(pdev, skin_data);

	for (i = 0; i < skin_data->num_sensor; i++) {
		skin_tz = devm_kzalloc(dev, sizeof(*skin_tz), GFP_KERNEL);
		if (!skin_tz)
			return -ENOMEM;

		skin_tz->id = i;
		skin_tz->skin_data = skin_data;

		tzdev = devm_thermal_zone_of_sensor_register(dev, skin_tz->id,
				skin_tz, &vtskin_ops);

		if (IS_ERR(tzdev)) {
			ret = PTR_ERR(tzdev);
			dev_err(dev,
				"Error: Failed to register skin_tz.id %d, ret = %d\n",
				skin_tz->id, ret);
			return ret;
		}

	}

	return 0;
}

enum mt6983_vtskin_sensor_enum {
	MT6983_BACK_VTSKIN,
	MT6983_FRONT_VTSKIN,
	MT6983_NUM_VTSKIN,
};

struct vtskin_tz_param mt6983_vtskin_params[] = {
	[MT6983_BACK_VTSKIN] = {
		.ref_num = 8,
		.vtskin_ref = {
			{          "soc_top",     7025154},
			{           "ap_ntc",    37119632},
			{         "nrpa_ntc",   151820993},
			{ "pmic6363_bk3_bk7",    14432832},
			{ "pmic6373_bk3_bk7",   -50115396},
			{      "pmic6338_ts",   -28439482},
			{           "consys",    -5678249},
			{          "battery",   -43545666}},
	},
	[MT6983_FRONT_VTSKIN] = {
		.ref_num = 8,
		.vtskin_ref = {
			{	   "soc_top",    21039971},
			{	    "ap_ntc",   -17108825},
			{	  "nrpa_ntc",   192868475},
			{ "pmic6363_bk3_bk7",    -5402859},
			{ "pmic6373_bk3_bk7",   -48071710},
			{      "pmic6338_ts",   -47777387},
			{           "consys",    -4666669},
			{          "battery",    -6006729}},
	}
};

static struct vtskin_data mt6983_vtskin_data = {
	.num_sensor = MT6983_NUM_VTSKIN,
	.params = mt6983_vtskin_params,
};

static const struct of_device_id vtskin_of_match[] = {
	{
		.compatible = "mediatek,mt6983-virtual-tskin",
		.data = (void *)&mt6983_vtskin_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, vtskin_of_match);

static struct platform_driver vtskin_driver = {
	.probe = vtskin_probe,
	.driver = {
		.name = "mtk-virtual-tskin",
		.of_match_table = vtskin_of_match,
	},
};

module_platform_driver(vtskin_driver);

MODULE_AUTHOR("Samuel Hsieh <samuel.hsieh@mediatek.com>");
MODULE_DESCRIPTION("Mediatek on virtual tskin driver");
MODULE_LICENSE("GPL v2");
