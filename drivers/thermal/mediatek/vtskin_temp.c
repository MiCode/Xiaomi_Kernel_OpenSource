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
	MT6983_VTSKIN_1,
	MT6983_VTSKIN_2,
	MT6983_VTSKIN_3,
	MT6983_VTSKIN_4,
	MT6983_VTSKIN_5,
	MT6983_VTSKIN_6,
	MT6983_NUM_VTSKIN,
};

struct vtskin_tz_param mt6983_vtskin_params[] = {
	[MT6983_VTSKIN_1] = {
		.ref_num = 8,
		.vtskin_ref = {
			{          "soc_top",    2313635},
			{           "ap_ntc",  -17278441},
			{         "nrpa_ntc",  168147662},
			{ "pmic6363_bk3_bk7",     564886},
			{ "pmic6373_bk3_bk7",    -490919},
			{      "pmic6338_ts",  -40986186},
			{           "consys",    3412150},
			{          "battery",  -22576691}},
	},
	[MT6983_VTSKIN_2] = {
		.ref_num = 8,
		.vtskin_ref = {
			{          "soc_top",    3255065},
			{           "ap_ntc",  -62150677},
			{         "nrpa_ntc",  149976078},
			{ "pmic6363_bk3_bk7",   22876394},
			{ "pmic6373_bk3_bk7",   -2064824},
			{      "pmic6338_ts",   27027423},
			{           "consys",  -15933089},
			{          "battery",  -34217383}},
	},
	[MT6983_VTSKIN_3] = {
		.ref_num = 8,
		.vtskin_ref = {
			{          "soc_top",     574011},
			{           "ap_ntc",  -46643505},
			{         "nrpa_ntc",  167793585},
			{ "pmic6363_bk3_bk7",   14117694},
			{ "pmic6373_bk3_bk7",    -825852},
			{      "pmic6338_ts",  -20259317},
			{           "consys",    2173021},
			{          "battery",  -22997333}},
	},
	[MT6983_VTSKIN_4] = {
		.ref_num = 8,
		.vtskin_ref = {
			{          "soc_top",    1025071},
			{           "ap_ntc",  -46468989},
			{         "nrpa_ntc",  163038976},
			{ "pmic6363_bk3_bk7",   13007917},
			{ "pmic6373_bk3_bk7",    -575286},
			{      "pmic6338_ts",  -14243451},
			{           "consys",     149975},
			{          "battery",  -21612202}},
	},
	[MT6983_VTSKIN_5] = {
		.ref_num = 0,
		.vtskin_ref = {
			{          "soc_top",    2272107},
			{           "ap_ntc",  -12028723},
			{         "nrpa_ntc",  167869082},
			{ "pmic6363_bk3_bk7",   -1604055},
			{ "pmic6373_bk3_bk7",    -339789},
			{      "pmic6338_ts",  -46784687},
			{           "consys",    5544910},
			{          "battery",  -21683366}},
	},
	[MT6983_VTSKIN_6] = {
		.ref_num = 0,
		.vtskin_ref = {
			{          "soc_top",    1192347},
			{           "ap_ntc",  -26876988},
			{         "nrpa_ntc",  151476705},
			{ "pmic6363_bk3_bk7",    9613622},
			{ "pmic6373_bk3_bk7",    -910593},
			{      "pmic6338_ts",  -26222648},
			{           "consys",    2997056},
			{          "battery",  -19949450}},
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
