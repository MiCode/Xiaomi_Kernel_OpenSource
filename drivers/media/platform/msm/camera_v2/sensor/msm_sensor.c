/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <mach/gpiomux.h>
#include "msm_sensor.h"
#include "msm_sd.h"
#include "camera.h"
#include "msm_cci.h"
#include "msm_camera_io_util.h"
#include "msm_camera_i2c_mux.h"
#include <mach/rpm-regulator.h>
#include <mach/rpm-regulator-smd.h>
#include <linux/regulator/consumer.h>

#undef CDBG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

static int32_t msm_sensor_enable_i2c_mux(struct msm_camera_i2c_conf *i2c_conf)
{
	struct v4l2_subdev *i2c_mux_sd =
		dev_get_drvdata(&i2c_conf->mux_dev->dev);
	v4l2_subdev_call(i2c_mux_sd, core, ioctl,
		VIDIOC_MSM_I2C_MUX_INIT, NULL);
	v4l2_subdev_call(i2c_mux_sd, core, ioctl,
		VIDIOC_MSM_I2C_MUX_CFG, (void *)&i2c_conf->i2c_mux_mode);
	return 0;
}

static int32_t msm_sensor_disable_i2c_mux(struct msm_camera_i2c_conf *i2c_conf)
{
	struct v4l2_subdev *i2c_mux_sd =
		dev_get_drvdata(&i2c_conf->mux_dev->dev);
	v4l2_subdev_call(i2c_mux_sd, core, ioctl,
				VIDIOC_MSM_I2C_MUX_RELEASE, NULL);
	return 0;
}

static int32_t msm_sensor_get_sub_module_index(struct device_node *of_node,
	struct  msm_camera_sensor_board_info *sensordata)
{
	int32_t rc = 0, i = 0;
	uint32_t val = 0, count = 0;
	uint32_t *val_array = NULL;
	struct device_node *src_node = NULL;

	sensordata->sensor_info = kzalloc(sizeof(struct msm_sensor_info_t),
		GFP_KERNEL);
	if (!sensordata->sensor_info) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		return -ENOMEM;
	}
	for (i = 0; i < SUB_MODULE_MAX; i++)
		sensordata->sensor_info->subdev_id[i] = -1;

	src_node = of_parse_phandle(of_node, "qcom,actuator-src", 0);
	if (!src_node) {
		CDBG("%s:%d src_node NULL\n", __func__, __LINE__);
	} else {
		rc = of_property_read_u32(src_node, "cell-index", &val);
		CDBG("%s qcom,actuator cell index %d, rc %d\n", __func__,
			val, rc);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR;
		}
		sensordata->sensor_info->
			subdev_id[SUB_MODULE_ACTUATOR] = val;
		of_node_put(src_node);
		src_node = NULL;
	}

	src_node = of_parse_phandle(of_node, "qcom,eeprom-src", 0);
	if (!src_node) {
		CDBG("%s:%d eeprom src_node NULL\n", __func__, __LINE__);
	} else {
		rc = of_property_read_u32(src_node, "cell-index", &val);
		CDBG("%s qcom,eeprom cell index %d, rc %d\n", __func__,
			val, rc);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR;
		}
		sensordata->sensor_info->
			subdev_id[SUB_MODULE_EEPROM] = val;
		of_node_put(src_node);
		src_node = NULL;
	}

	if (of_property_read_bool(of_node, "qcom,eeprom-sd-index") ==
		true) {
		rc = of_property_read_u32(of_node, "qcom,eeprom-sd-index",
			&val);
		CDBG("%s qcom,eeprom-sd-index %d, rc %d\n", __func__, val, rc);
		if (rc < 0) {
			pr_err("%s:%d failed rc %d\n", __func__, __LINE__, rc);
			goto ERROR;
		}
		sensordata->sensor_info->subdev_id[SUB_MODULE_EEPROM] = val;
	}

	src_node = of_parse_phandle(of_node, "qcom,led-flash-src", 0);
	if (!src_node) {
		CDBG("%s:%d src_node NULL\n", __func__, __LINE__);
	} else {
		rc = of_property_read_u32(src_node, "cell-index", &val);
		CDBG("%s qcom,led flash cell index %d, rc %d\n", __func__,
			val, rc);
		if (rc < 0) {
			pr_err("%s:%d failed %d\n", __func__, __LINE__, rc);
			goto ERROR;
		}
		sensordata->sensor_info->
			subdev_id[SUB_MODULE_LED_FLASH] = val;
		of_node_put(src_node);
		src_node = NULL;
	}

	if (of_property_read_bool(of_node, "qcom,strobe-flash-sd-index") ==
		true) {
		rc = of_property_read_u32(of_node, "qcom,strobe-flash-sd-index",
			&val);
		CDBG("%s qcom,strobe-flash-sd-index %d, rc %d\n", __func__,
			val, rc);
		if (rc < 0) {
			pr_err("%s:%d failed rc %d\n", __func__, __LINE__, rc);
			goto ERROR;
		}
		sensordata->sensor_info->subdev_id[SUB_MODULE_STROBE_FLASH] =
			val;
	}

	if (of_get_property(of_node, "qcom,csiphy-sd-index", &count)) {
		count /= sizeof(uint32_t);
		if (count > 2) {
			pr_err("%s qcom,csiphy-sd-index count %d > 2\n",
				__func__, count);
			goto ERROR;
		}
		val_array = kzalloc(sizeof(uint32_t) * count, GFP_KERNEL);
		if (!val_array) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			rc = -ENOMEM;
			goto ERROR;
		}

		rc = of_property_read_u32_array(of_node, "qcom,csiphy-sd-index",
			val_array, count);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			kfree(val_array);
			goto ERROR;
		}
		for (i = 0; i < count; i++) {
			sensordata->sensor_info->subdev_id
				[SUB_MODULE_CSIPHY + i] = val_array[i];
			CDBG("%s csiphy_core[%d] = %d\n",
				__func__, i, val_array[i]);
		}
		kfree(val_array);
	} else {
		pr_err("%s:%d qcom,csiphy-sd-index not present\n", __func__,
			__LINE__);
		rc = -EINVAL;
		goto ERROR;
	}

	if (of_get_property(of_node, "qcom,csid-sd-index", &count)) {
		count /= sizeof(uint32_t);
		if (count > 2) {
			pr_err("%s qcom,csid-sd-index count %d > 2\n",
				__func__, count);
			rc = -EINVAL;
			goto ERROR;
		}
		val_array = kzalloc(sizeof(uint32_t) * count, GFP_KERNEL);
		if (!val_array) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			rc = -ENOMEM;
			goto ERROR;
		}

		rc = of_property_read_u32_array(of_node, "qcom,csid-sd-index",
			val_array, count);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			kfree(val_array);
			goto ERROR;
		}
		for (i = 0; i < count; i++) {
			sensordata->sensor_info->subdev_id
				[SUB_MODULE_CSID + i] = val_array[i];
			CDBG("%s csid_core[%d] = %d\n",
				__func__, i, val_array[i]);
		}
		kfree(val_array);
	} else {
		pr_err("%s:%d qcom,csid-sd-index not present\n", __func__,
			__LINE__);
		rc = -EINVAL;
		goto ERROR;
	}
	return rc;
ERROR:
	kfree(sensordata->sensor_info);
	sensordata->sensor_info = NULL;
	return rc;
}

static int32_t msm_sensor_get_dt_csi_data(struct device_node *of_node,
	struct  msm_camera_sensor_board_info *sensordata)
{
	int32_t rc = 0;
	uint32_t val = 0;

	sensordata->csi_lane_params = kzalloc(
		sizeof(struct msm_camera_csi_lane_params), GFP_KERNEL);
	if (!sensordata->csi_lane_params) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto ERROR1;
	}

	rc = of_property_read_u32(of_node, "qcom,csi-lane-assign", &val);
	CDBG("%s qcom,csi-lane-assign %x, rc %d\n", __func__, val, rc);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	sensordata->csi_lane_params->csi_lane_assign = val;

	rc = of_property_read_u32(of_node, "qcom,csi-lane-mask", &val);
	CDBG("%s qcom,csi-lane-mask %x, rc %d\n", __func__, val, rc);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	sensordata->csi_lane_params->csi_lane_mask = val;

	return rc;
ERROR2:
	kfree(sensordata->csi_lane_params);
ERROR1:
	return rc;
}

static int32_t msm_sensor_get_dt_vreg_data(struct device_node *of_node,
	struct msm_camera_sensor_board_info *sensordata)
{
	int32_t rc = 0, i = 0;
	uint32_t count = 0;
	uint32_t *vreg_array = NULL;

	count = of_property_count_strings(of_node, "qcom,cam-vreg-name");
	CDBG("%s qcom,cam-vreg-name count %d\n", __func__, count);

	if (!count)
		return 0;

	sensordata->cam_vreg = kzalloc(sizeof(struct camera_vreg_t) * count,
		GFP_KERNEL);
	if (!sensordata->cam_vreg) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}

	sensordata->num_vreg = count;
	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(of_node,
			"qcom,cam-vreg-name", i,
			&sensordata->cam_vreg[i].reg_name);
		CDBG("%s reg_name[%d] = %s\n", __func__, i,
			sensordata->cam_vreg[i].reg_name);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR1;
		}
	}

	vreg_array = kzalloc(sizeof(uint32_t) * count, GFP_KERNEL);
	if (!vreg_array) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto ERROR1;
	}

	rc = of_property_read_u32_array(of_node, "qcom,cam-vreg-type",
		vreg_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		sensordata->cam_vreg[i].type = vreg_array[i];
		CDBG("%s cam_vreg[%d].type = %d\n", __func__, i,
			sensordata->cam_vreg[i].type);
	}

	rc = of_property_read_u32_array(of_node, "qcom,cam-vreg-min-voltage",
		vreg_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		sensordata->cam_vreg[i].min_voltage = vreg_array[i];
		CDBG("%s cam_vreg[%d].min_voltage = %d\n", __func__,
			i, sensordata->cam_vreg[i].min_voltage);
	}

	rc = of_property_read_u32_array(of_node, "qcom,cam-vreg-max-voltage",
		vreg_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		sensordata->cam_vreg[i].max_voltage = vreg_array[i];
		CDBG("%s cam_vreg[%d].max_voltage = %d\n", __func__,
			i, sensordata->cam_vreg[i].max_voltage);
	}

	rc = of_property_read_u32_array(of_node, "qcom,cam-vreg-op-mode",
		vreg_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		sensordata->cam_vreg[i].op_mode = vreg_array[i];
		CDBG("%s cam_vreg[%d].op_mode = %d\n", __func__, i,
			sensordata->cam_vreg[i].op_mode);
	}

	kfree(vreg_array);
	return rc;
ERROR2:
	kfree(vreg_array);
ERROR1:
	kfree(sensordata->cam_vreg);
	sensordata->num_vreg = 0;
	return rc;
}

int32_t msm_sensor_get_dt_gpio_req_tbl(struct device_node *of_node,
	struct msm_camera_gpio_conf *gconf, uint16_t *gpio_array,
	uint16_t gpio_array_size)
{
	int32_t rc = 0, i = 0;
	uint32_t count = 0;
	uint32_t *val_array = NULL;

	if (!of_get_property(of_node, "qcom,gpio-req-tbl-num", &count))
		return 0;

	count /= sizeof(uint32_t);
	if (!count) {
		pr_err("%s qcom,gpio-req-tbl-num 0\n", __func__);
		return 0;
	}

	val_array = kzalloc(sizeof(uint32_t) * count, GFP_KERNEL);
	if (!val_array) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}

	gconf->cam_gpio_req_tbl = kzalloc(sizeof(struct gpio) * count,
		GFP_KERNEL);
	if (!gconf->cam_gpio_req_tbl) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto ERROR1;
	}
	gconf->cam_gpio_req_tbl_size = count;

	rc = of_property_read_u32_array(of_node, "qcom,gpio-req-tbl-num",
		val_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		if (val_array[i] >= gpio_array_size) {
			pr_err("%s gpio req tbl index %d invalid\n",
				__func__, val_array[i]);
			return -EINVAL;
		}
		gconf->cam_gpio_req_tbl[i].gpio = gpio_array[val_array[i]];
		CDBG("%s cam_gpio_req_tbl[%d].gpio = %d\n", __func__, i,
			gconf->cam_gpio_req_tbl[i].gpio);
	}

	rc = of_property_read_u32_array(of_node, "qcom,gpio-req-tbl-flags",
		val_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		gconf->cam_gpio_req_tbl[i].flags = val_array[i];
		CDBG("%s cam_gpio_req_tbl[%d].flags = %ld\n", __func__, i,
			gconf->cam_gpio_req_tbl[i].flags);
	}

	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(of_node,
			"qcom,gpio-req-tbl-label", i,
			&gconf->cam_gpio_req_tbl[i].label);
		CDBG("%s cam_gpio_req_tbl[%d].label = %s\n", __func__, i,
			gconf->cam_gpio_req_tbl[i].label);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR2;
		}
	}

	kfree(val_array);
	return rc;

ERROR2:
	kfree(gconf->cam_gpio_req_tbl);
ERROR1:
	kfree(val_array);
	gconf->cam_gpio_req_tbl_size = 0;
	return rc;
}

int32_t msm_sensor_get_dt_gpio_set_tbl(struct device_node *of_node,
	struct msm_camera_gpio_conf *gconf, uint16_t *gpio_array,
	uint16_t gpio_array_size)
{
	int32_t rc = 0, i = 0;
	uint32_t count = 0;
	uint32_t *val_array = NULL;

	if (!of_get_property(of_node, "qcom,gpio-set-tbl-num", &count))
		return 0;

	count /= sizeof(uint32_t);
	if (!count) {
		pr_err("%s qcom,gpio-set-tbl-num 0\n", __func__);
		return 0;
	}

	val_array = kzalloc(sizeof(uint32_t) * count, GFP_KERNEL);
	if (!val_array) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}

	gconf->cam_gpio_set_tbl = kzalloc(sizeof(struct msm_gpio_set_tbl) *
		count, GFP_KERNEL);
	if (!gconf->cam_gpio_set_tbl) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto ERROR1;
	}
	gconf->cam_gpio_set_tbl_size = count;

	rc = of_property_read_u32_array(of_node, "qcom,gpio-set-tbl-num",
		val_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		if (val_array[i] >= gpio_array_size) {
			pr_err("%s gpio set tbl index %d invalid\n",
				__func__, val_array[i]);
			return -EINVAL;
		}
		gconf->cam_gpio_set_tbl[i].gpio = gpio_array[val_array[i]];
		CDBG("%s cam_gpio_set_tbl[%d].gpio = %d\n", __func__, i,
			gconf->cam_gpio_set_tbl[i].gpio);
	}

	rc = of_property_read_u32_array(of_node, "qcom,gpio-set-tbl-flags",
		val_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		gconf->cam_gpio_set_tbl[i].flags = val_array[i];
		CDBG("%s cam_gpio_set_tbl[%d].flags = %ld\n", __func__, i,
			gconf->cam_gpio_set_tbl[i].flags);
	}

	rc = of_property_read_u32_array(of_node, "qcom,gpio-set-tbl-delay",
		val_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		gconf->cam_gpio_set_tbl[i].delay = val_array[i];
		CDBG("%s cam_gpio_set_tbl[%d].delay = %d\n", __func__, i,
			gconf->cam_gpio_set_tbl[i].delay);
	}

	kfree(val_array);
	return rc;

ERROR2:
	kfree(gconf->cam_gpio_set_tbl);
ERROR1:
	kfree(val_array);
	gconf->cam_gpio_set_tbl_size = 0;
	return rc;
}

int32_t msm_sensor_init_gpio_pin_tbl(struct device_node *of_node,
	struct msm_camera_gpio_conf *gconf, uint16_t *gpio_array,
	uint16_t gpio_array_size)
{
	int32_t rc = 0;
	int32_t val = 0;

	gconf->gpio_num_info = kzalloc(sizeof(struct msm_camera_gpio_num_info),
		GFP_KERNEL);
	if (!gconf->gpio_num_info) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		return rc;
	}

	if (of_property_read_bool(of_node, "qcom,gpio-reset") == true) {
		rc = of_property_read_u32(of_node, "qcom,gpio-reset", &val);
		if (rc < 0) {
			pr_err("%s:%d read qcom,gpio-reset failed rc %d\n",
				__func__, __LINE__, rc);
			goto ERROR;
		} else if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-reset invalid %d\n",
				__func__, __LINE__, val);
			goto ERROR;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_GPIO_RESET] =
			gpio_array[val];
		CDBG("%s qcom,gpio-reset %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_GPIO_RESET]);
	}

	if (of_property_read_bool(of_node, "qcom,gpio-standby") == true) {
		rc = of_property_read_u32(of_node, "qcom,gpio-standby", &val);
		if (rc < 0) {
			pr_err("%s:%d read qcom,gpio-standby failed rc %d\n",
				__func__, __LINE__, rc);
			goto ERROR;
		} else if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-standby invalid %d\n",
				__func__, __LINE__, val);
			goto ERROR;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_GPIO_STANDBY] =
			gpio_array[val];
		CDBG("%s qcom,gpio-reset %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_GPIO_STANDBY]);
	}

	rc = of_property_read_u32(of_node, "qcom,gpio-vio", &val);
	if (!rc) {
		if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-vio invalid %d\n",
				__func__, __LINE__, val);
			goto ERROR;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_GPIO_VIO] =
			gpio_array[val];
		CDBG("%s qcom,gpio-vio %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_GPIO_VIO]);
	} else if (rc != -EINVAL) {
		pr_err("%s:%d read qcom,gpio-vio failed rc %d\n",
			__func__, __LINE__, rc);
		goto ERROR;
	}

	rc = of_property_read_u32(of_node, "qcom,gpio-vana", &val);
	if (!rc) {
		if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-vana invalid %d\n",
				__func__, __LINE__, val);
			goto ERROR;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_GPIO_VANA] =
			gpio_array[val];
		CDBG("%s qcom,gpio-vana %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_GPIO_VANA]);
	} else if (rc != -EINVAL) {
		pr_err("%s:%d read qcom,gpio-vana failed rc %d\n",
			__func__, __LINE__, rc);
		goto ERROR;
	}

	rc = of_property_read_u32(of_node, "qcom,gpio-vdig", &val);
	if (!rc) {
		if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-vdig invalid %d\n",
				__func__, __LINE__, val);
			goto ERROR;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_GPIO_VDIG] =
			gpio_array[val];
		CDBG("%s qcom,gpio-vdig %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_GPIO_VDIG]);
	} else if (rc != -EINVAL) {
		pr_err("%s:%d read qcom,gpio-vdig failed rc %d\n",
			__func__, __LINE__, rc);
		goto ERROR;
	}

	rc = of_property_read_u32(of_node, "qcom,gpio-vaf", &val);
	if (!rc) {
		if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-vaf invalid %d\n",
				__func__, __LINE__, val);
			goto ERROR;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_GPIO_VAF] =
			gpio_array[val];
		CDBG("%s qcom,gpio-vaf %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_GPIO_VAF]);
	} else if (rc != -EINVAL) {
		pr_err("%s:%d read qcom,gpio-vaf failed rc %d\n",
			__func__, __LINE__, rc);
		goto ERROR;
	}

	rc = of_property_read_u32(of_node, "qcom,gpio-af-pwdm", &val);
	if (!rc) {
		if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-af-pwdm invalid %d\n",
				__func__, __LINE__, val);
			goto ERROR;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_GPIO_AF_PWDM] =
			gpio_array[val];
		CDBG("%s qcom,gpio-af-pwdm %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_GPIO_AF_PWDM]);
	} else if (rc != -EINVAL) {
		pr_err("%s:%d read qcom,gpio-af-pwdm failed rc %d\n",
			__func__, __LINE__, rc);
		goto ERROR;
	}
	return 0;

ERROR:
	kfree(gconf->gpio_num_info);
	gconf->gpio_num_info = NULL;
	return rc;
}

static int32_t msm_sensor_get_dt_actuator_data(struct device_node *of_node,
	struct  msm_camera_sensor_board_info *sensordata)
{
	int32_t rc = 0;
	uint32_t val = 0;

	rc = of_property_read_u32(of_node, "qcom,actuator-cam-name", &val);
	CDBG("%s qcom,actuator-cam-name %d, rc %d\n", __func__, val, rc);
	if (rc < 0)
		return 0;

	sensordata->actuator_info = kzalloc(sizeof(struct msm_actuator_info),
		GFP_KERNEL);
	if (!sensordata->actuator_info) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto ERROR;
	}

	sensordata->actuator_info->cam_name = val;

	rc = of_property_read_u32(of_node, "qcom,actuator-vcm-pwd", &val);
	CDBG("%s qcom,actuator-vcm-pwd %d, rc %d\n", __func__, val, rc);
	if (!rc)
		sensordata->actuator_info->vcm_pwd = val;

	rc = of_property_read_u32(of_node, "qcom,actuator-vcm-enable", &val);
	CDBG("%s qcom,actuator-vcm-enable %d, rc %d\n", __func__, val, rc);
	if (!rc)
		sensordata->actuator_info->vcm_enable = val;

	return 0;
ERROR:
	return rc;
}

static int32_t msm_sensor_get_dt_data(struct device_node *of_node,
	struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0, i = 0, ret = 0;
	struct msm_camera_gpio_conf *gconf = NULL;
	struct msm_camera_sensor_board_info *sensordata = NULL;
	uint16_t *gpio_array = NULL;
	uint16_t gpio_array_size = 0;
	uint32_t id_info[3];

	s_ctrl->sensordata = kzalloc(sizeof(
		struct msm_camera_sensor_board_info),
		GFP_KERNEL);
	if (!s_ctrl->sensordata) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}

	sensordata = s_ctrl->sensordata;

	sensordata->sensor_init_params = kzalloc(sizeof(
		struct msm_sensor_init_params), GFP_KERNEL);
	if (!sensordata->sensor_init_params) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}

	rc = of_property_read_string(of_node, "qcom,sensor-name",
		&sensordata->sensor_name);
	CDBG("%s qcom,sensor-name %s, rc %d\n", __func__,
		sensordata->sensor_name, rc);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR1;
	}

	rc = of_property_read_u32(of_node, "qcom,sensor-mode",
		&sensordata->sensor_init_params->modes_supported);
	CDBG("%s qcom,sensor-mode %d, rc %d\n", __func__,
		sensordata->sensor_init_params->modes_supported, rc);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR1;
	}

	rc = of_property_read_u32(of_node, "qcom,sensor-position",
		&sensordata->sensor_init_params->position);
	CDBG("%s qcom,sensor-position %d, rc %d\n", __func__,
		sensordata->sensor_init_params->position, rc);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR1;
	}

	rc = of_property_read_u32(of_node, "qcom,mount-angle",
		&sensordata->sensor_init_params->sensor_mount_angle);
	CDBG("%s qcom,mount-angle %d, rc %d\n", __func__,
		sensordata->sensor_init_params->sensor_mount_angle, rc);
	if (rc < 0) {
		/* Set default mount angle */
		sensordata->sensor_init_params->sensor_mount_angle = 0;
		rc = 0;
	}

	rc = of_property_read_u32(of_node, "qcom,cci-master",
		&s_ctrl->cci_i2c_master);
	CDBG("%s qcom,cci-master %d, rc %d\n", __func__, s_ctrl->cci_i2c_master,
		rc);
	if (rc < 0) {
		/* Set default master 0 */
		s_ctrl->cci_i2c_master = MASTER_0;
		rc = 0;
	}

	rc = msm_sensor_get_sub_module_index(of_node, sensordata);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR1;
	}

	rc = msm_sensor_get_dt_csi_data(of_node, sensordata);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR1;
	}

	rc = msm_sensor_get_dt_vreg_data(of_node, sensordata);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}

	sensordata->gpio_conf = kzalloc(sizeof(struct msm_camera_gpio_conf),
		GFP_KERNEL);
	if (!sensordata->gpio_conf) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto ERROR3;
	}
	gconf = sensordata->gpio_conf;

	gpio_array_size = of_gpio_count(of_node);
	CDBG("%s gpio count %d\n", __func__, gpio_array_size);

	if (gpio_array_size) {
		gpio_array = kzalloc(sizeof(uint16_t) * gpio_array_size,
			GFP_KERNEL);
		if (!gpio_array) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR4;
		}
		for (i = 0; i < gpio_array_size; i++) {
			gpio_array[i] = of_get_gpio(of_node, i);
			CDBG("%s gpio_array[%d] = %d\n", __func__, i,
				gpio_array[i]);
		}

		rc = msm_sensor_get_dt_gpio_req_tbl(of_node, gconf,
			gpio_array, gpio_array_size);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR4;
		}

		rc = msm_sensor_get_dt_gpio_set_tbl(of_node, gconf,
			gpio_array, gpio_array_size);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR5;
		}

		rc = msm_sensor_init_gpio_pin_tbl(of_node, gconf,
			gpio_array, gpio_array_size);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR6;
		}
	}
	rc = msm_sensor_get_dt_actuator_data(of_node, sensordata);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR7;
	}

	sensordata->slave_info = kzalloc(sizeof(struct msm_camera_slave_info),
		GFP_KERNEL);
	if (!sensordata->slave_info) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto ERROR8;
	}

	rc = of_property_read_u32_array(of_node, "qcom,slave-id",
		id_info, 3);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR9;
	}

	sensordata->slave_info->sensor_slave_addr = id_info[0];
	sensordata->slave_info->sensor_id_reg_addr = id_info[1];
	sensordata->slave_info->sensor_id = id_info[2];

	/*Optional property, don't return error if absent */
	ret = of_property_read_string(of_node, "qcom,vdd-cx-name",
		&sensordata->misc_regulator);
	CDBG("%s qcom,misc_regulator %s, rc %d\n", __func__,
		 sensordata->misc_regulator, ret);

	kfree(gpio_array);

	return rc;

ERROR9:
	kfree(s_ctrl->sensordata->slave_info);
ERROR8:
	kfree(s_ctrl->sensordata->actuator_info);
ERROR7:
	kfree(s_ctrl->sensordata->gpio_conf->gpio_num_info);
ERROR6:
	kfree(s_ctrl->sensordata->gpio_conf->cam_gpio_set_tbl);
ERROR5:
	kfree(s_ctrl->sensordata->gpio_conf->cam_gpio_req_tbl);
ERROR4:
	kfree(s_ctrl->sensordata->gpio_conf);
ERROR3:
	kfree(s_ctrl->sensordata->cam_vreg);
ERROR2:
	kfree(s_ctrl->sensordata->csi_lane_params);
ERROR1:
	kfree(s_ctrl->sensordata);
	kfree(gpio_array);
	return rc;
}

static void msm_sensor_misc_regulator(
	struct msm_sensor_ctrl_t *sctrl, uint32_t enable)
{
	int32_t rc = 0;
	if (enable) {
		sctrl->misc_regulator = (void *)rpm_regulator_get(
			&sctrl->pdev->dev, sctrl->sensordata->misc_regulator);
		if (sctrl->misc_regulator) {
			rc = rpm_regulator_set_mode(sctrl->misc_regulator,
				RPM_REGULATOR_MODE_HPM);
			if (rc < 0) {
				pr_err("%s: Failed to set for rpm regulator on %s: %d\n",
					__func__,
					sctrl->sensordata->misc_regulator, rc);
				rpm_regulator_put(sctrl->misc_regulator);
			}
		} else {
			pr_err("%s: Failed to vote for rpm regulator on %s: %d\n",
				__func__,
				sctrl->sensordata->misc_regulator, rc);
		}
	} else {
		if (sctrl->misc_regulator) {
			rc = rpm_regulator_set_mode(
				(struct rpm_regulator *)sctrl->misc_regulator,
				RPM_REGULATOR_MODE_AUTO);
			if (rc < 0)
				pr_err("%s: Failed to set for rpm regulator on %s: %d\n",
					__func__,
					sctrl->sensordata->misc_regulator, rc);
			rpm_regulator_put(sctrl->misc_regulator);
		}
	}
}

int32_t msm_sensor_free_sensor_data(struct msm_sensor_ctrl_t *s_ctrl)
{
	if (!s_ctrl->pdev)
		return 0;
	kfree(s_ctrl->sensordata->slave_info);
	kfree(s_ctrl->sensordata->actuator_info);
	kfree(s_ctrl->sensordata->gpio_conf->gpio_num_info);
	kfree(s_ctrl->sensordata->gpio_conf->cam_gpio_set_tbl);
	kfree(s_ctrl->sensordata->gpio_conf->cam_gpio_req_tbl);
	kfree(s_ctrl->sensordata->gpio_conf);
	kfree(s_ctrl->sensordata->cam_vreg);
	kfree(s_ctrl->sensordata->csi_lane_params);
	kfree(s_ctrl->sensordata->sensor_info);
	kfree(s_ctrl->sensordata->sensor_init_params);
	kfree(s_ctrl->sensordata);
	kfree(s_ctrl->clk_info);
	return 0;
}

static struct msm_cam_clk_info cam_8960_clk_info[] = {
	[SENSOR_CAM_MCLK] = {"cam_clk", 24000000},
};

static struct msm_cam_clk_info cam_8610_clk_info[] = {
	[SENSOR_CAM_MCLK] = {"cam_src_clk", 24000000},
	[SENSOR_CAM_CLK] = {"cam_clk", 0},
};

static struct msm_cam_clk_info cam_8974_clk_info[] = {
	[SENSOR_CAM_MCLK] = {"cam_src_clk", 24000000},
	[SENSOR_CAM_CLK] = {"cam_clk", 0},
};

int32_t msm_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0, index = 0;
	struct msm_sensor_power_setting_array *power_setting_array = NULL;
	struct msm_sensor_power_setting *power_setting = NULL;
	struct msm_camera_sensor_board_info *data = s_ctrl->sensordata;
	uint32_t retry = 0;
	s_ctrl->stop_setting_valid = 0;

	CDBG("%s:%d\n", __func__, __LINE__);
	power_setting_array = &s_ctrl->power_setting_array;

	if (data->gpio_conf->cam_gpiomux_conf_tbl != NULL) {
		pr_err("%s:%d mux install\n", __func__, __LINE__);
		msm_gpiomux_install(
			(struct msm_gpiomux_config *)
			data->gpio_conf->cam_gpiomux_conf_tbl,
			data->gpio_conf->cam_gpiomux_conf_tbl_size);
	}

	rc = msm_camera_request_gpio_table(
		data->gpio_conf->cam_gpio_req_tbl,
		data->gpio_conf->cam_gpio_req_tbl_size, 1);
	if (rc < 0) {
		pr_err("%s: request gpio failed\n", __func__);
		return rc;
	}
	for (index = 0; index < power_setting_array->size; index++) {
		CDBG("%s index %d\n", __func__, index);
		power_setting = &power_setting_array->power_setting[index];
		CDBG("%s type %d\n", __func__, power_setting->seq_type);
		switch (power_setting->seq_type) {
		case SENSOR_CLK:
			if (power_setting->seq_val >= s_ctrl->clk_info_size) {
				pr_err("%s clk index %d >= max %d\n", __func__,
					power_setting->seq_val,
					s_ctrl->clk_info_size);
				goto power_up_failed;
			}
			if (power_setting->config_val)
				s_ctrl->clk_info[power_setting->seq_val].
					clk_rate = power_setting->config_val;

			rc = msm_cam_clk_enable(s_ctrl->dev,
				&s_ctrl->clk_info[0],
				(struct clk **)&power_setting->data[0],
				s_ctrl->clk_info_size,
				1);
			if (rc < 0) {
				pr_err("%s: clk enable failed\n",
					__func__);
				goto power_up_failed;
			}
			break;
		case SENSOR_GPIO:
			if (power_setting->seq_val >= SENSOR_GPIO_MAX ||
				!data->gpio_conf->gpio_num_info) {
				pr_err("%s gpio index %d >= max %d\n", __func__,
					power_setting->seq_val,
					SENSOR_GPIO_MAX);
				goto power_up_failed;
			}
			pr_debug("%s:%d gpio set val %d\n", __func__, __LINE__,
				data->gpio_conf->gpio_num_info->gpio_num
				[power_setting->seq_val]);
			gpio_set_value_cansleep(
				data->gpio_conf->gpio_num_info->gpio_num
				[power_setting->seq_val],
				power_setting->config_val);
			break;
		case SENSOR_VREG:
			if (power_setting->seq_val >= CAM_VREG_MAX) {
				pr_err("%s vreg index %d >= max %d\n", __func__,
					power_setting->seq_val,
					SENSOR_GPIO_MAX);
				goto power_up_failed;
			}
			msm_camera_config_single_vreg(s_ctrl->dev,
				&data->cam_vreg[power_setting->seq_val],
				(struct regulator **)&power_setting->data[0],
				1);
			break;
		case SENSOR_I2C_MUX:
			if (data->i2c_conf && data->i2c_conf->use_i2c_mux)
				msm_sensor_enable_i2c_mux(data->i2c_conf);
			break;
		default:
			pr_err("%s error power seq type %d\n", __func__,
				power_setting->seq_type);
			break;
		}
		if (power_setting->delay > 20) {
			msleep(power_setting->delay);
		} else if (power_setting->delay) {
			usleep_range(power_setting->delay * 1000,
				(power_setting->delay * 1000) + 1000);
		}
	}

	if (s_ctrl->sensor_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_util(
			s_ctrl->sensor_i2c_client, MSM_CCI_INIT);
		if (rc < 0) {
			pr_err("%s cci_init failed\n", __func__);
			goto power_up_failed;
		}
	}

	for (retry = 0; retry < 3; retry++)
	{
		if (s_ctrl->func_tbl->sensor_match_id)
			rc = s_ctrl->func_tbl->sensor_match_id(s_ctrl);
		else
			rc = msm_sensor_match_id(s_ctrl);
		if (rc < 0) {
			if (retry < 2) {
				continue;
			} else {
				pr_err("%s:%d match id failed rc %d\n", __func__, __LINE__, rc);
				goto power_up_failed;
			}
		} else {
			break;
		}
	}

	CDBG("%s exit\n", __func__);
	return 0;
power_up_failed:
	pr_err("%s:%d failed\n", __func__, __LINE__);
	if (s_ctrl->sensor_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_util(
			s_ctrl->sensor_i2c_client, MSM_CCI_RELEASE);
	}

	for (index--; index >= 0; index--) {
		CDBG("%s index %d\n", __func__, index);
		power_setting = &power_setting_array->power_setting[index];
		CDBG("%s type %d\n", __func__, power_setting->seq_type);
		switch (power_setting->seq_type) {
		case SENSOR_CLK:
			msm_cam_clk_enable(s_ctrl->dev,
				&s_ctrl->clk_info[0],
				(struct clk **)&power_setting->data[0],
				s_ctrl->clk_info_size,
				0);
			break;
		case SENSOR_GPIO:
			gpio_set_value_cansleep(
				data->gpio_conf->gpio_num_info->gpio_num
				[power_setting->seq_val], GPIOF_OUT_INIT_LOW);
			break;
		case SENSOR_VREG:
			msm_camera_config_single_vreg(s_ctrl->dev,
				&data->cam_vreg[power_setting->seq_val],
				(struct regulator **)&power_setting->data[0],
				0);
			break;
		case SENSOR_I2C_MUX:
			if (data->i2c_conf && data->i2c_conf->use_i2c_mux)
				msm_sensor_disable_i2c_mux(data->i2c_conf);
			break;
		default:
			pr_err("%s error power seq type %d\n", __func__,
				power_setting->seq_type);
			break;
		}
		if (power_setting->delay > 20) {
			msleep(power_setting->delay);
		} else if (power_setting->delay) {
			usleep_range(power_setting->delay * 1000,
				(power_setting->delay * 1000) + 1000);
		}
	}
	msm_camera_request_gpio_table(
		data->gpio_conf->cam_gpio_req_tbl,
		data->gpio_conf->cam_gpio_req_tbl_size, 0);
	return rc;
}

int32_t msm_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t index = 0;
	struct msm_sensor_power_setting_array *power_setting_array = NULL;
	struct msm_sensor_power_setting *power_setting = NULL;
	struct msm_camera_sensor_board_info *data = s_ctrl->sensordata;
	s_ctrl->stop_setting_valid = 0;

	CDBG("%s:%d\n", __func__, __LINE__);
	power_setting_array = &s_ctrl->power_setting_array;

	if (s_ctrl->sensor_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_util(
			s_ctrl->sensor_i2c_client, MSM_CCI_RELEASE);
	}

	for (index = (power_setting_array->size - 1); index >= 0; index--) {
		CDBG("%s index %d\n", __func__, index);
		power_setting = &power_setting_array->power_setting[index];
		CDBG("%s type %d\n", __func__, power_setting->seq_type);
		switch (power_setting->seq_type) {
		case SENSOR_CLK:
			msm_cam_clk_enable(s_ctrl->dev,
				&s_ctrl->clk_info[0],
				(struct clk **)&power_setting->data[0],
				s_ctrl->clk_info_size,
				0);
			break;
		case SENSOR_GPIO:
			if (power_setting->seq_val >= SENSOR_GPIO_MAX ||
				!data->gpio_conf->gpio_num_info) {
				pr_err("%s gpio index %d >= max %d\n", __func__,
					power_setting->seq_val,
					SENSOR_GPIO_MAX);
				continue;
			}
			gpio_set_value_cansleep(
				data->gpio_conf->gpio_num_info->gpio_num
				[power_setting->seq_val], GPIOF_OUT_INIT_LOW);
			break;
		case SENSOR_VREG:
			if (power_setting->seq_val >= CAM_VREG_MAX) {
				pr_err("%s vreg index %d >= max %d\n", __func__,
					power_setting->seq_val,
					SENSOR_GPIO_MAX);
				continue;
			}
			msm_camera_config_single_vreg(s_ctrl->dev,
				&data->cam_vreg[power_setting->seq_val],
				(struct regulator **)&power_setting->data[0],
				0);
			break;
		case SENSOR_I2C_MUX:
			if (data->i2c_conf && data->i2c_conf->use_i2c_mux)
				msm_sensor_disable_i2c_mux(data->i2c_conf);
			break;
		default:
			pr_err("%s error power seq type %d\n", __func__,
				power_setting->seq_type);
			break;
		}
		if (power_setting->delay > 20) {
			msleep(power_setting->delay);
		} else if (power_setting->delay) {
			usleep_range(power_setting->delay * 1000,
				(power_setting->delay * 1000) + 1000);
		}
	}
	msm_camera_request_gpio_table(
		data->gpio_conf->cam_gpio_req_tbl,
		data->gpio_conf->cam_gpio_req_tbl_size, 0);
	CDBG("%s exit\n", __func__);
	return 0;
}

int32_t msm_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensordata->slave_info->sensor_id_reg_addr,
			&chipid, MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		pr_err("%s: %s: read id failed\n", __func__,
			s_ctrl->sensordata->sensor_name);
		return rc;
	}

	CDBG("%s: read id: %x expected id %x:\n", __func__, chipid,
		s_ctrl->sensordata->slave_info->sensor_id);
	if (chipid != s_ctrl->sensordata->slave_info->sensor_id) {
		pr_err("msm_sensor_match_id chip id doesnot match\n");
		return -ENODEV;
	}
	return rc;
}

static struct msm_sensor_ctrl_t *get_sctrl(struct v4l2_subdev *sd)
{
	return container_of(container_of(sd, struct msm_sd_subdev, sd),
		struct msm_sensor_ctrl_t, msm_sd);
}

static void msm_sensor_stop_stream(struct msm_sensor_ctrl_t *s_ctrl)
{
	mutex_lock(s_ctrl->msm_sensor_mutex);
	if (s_ctrl->sensor_state == MSM_SENSOR_POWER_UP) {
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_table(
			s_ctrl->sensor_i2c_client, &s_ctrl->stop_setting);
		kfree(s_ctrl->stop_setting.reg_setting);
		s_ctrl->stop_setting.reg_setting = NULL;
	}
	mutex_unlock(s_ctrl->msm_sensor_mutex);
	return;
}

static int msm_sensor_get_af_status(struct msm_sensor_ctrl_t *s_ctrl,
			void __user *argp)
{
	/* TO-DO: Need to set AF status register address and expected value
	We need to check the AF status in the sensor register and
	set the status in the *status variable accordingly*/
	return 0;
}

static long msm_sensor_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	struct msm_sensor_ctrl_t *s_ctrl = get_sctrl(sd);
	void __user *argp = (void __user *)arg;
	if (!s_ctrl) {
		pr_err("%s s_ctrl NULL\n", __func__);
		return -EBADF;
	}
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_CFG:
		return s_ctrl->func_tbl->sensor_config(s_ctrl, argp);
	case VIDIOC_MSM_SENSOR_GET_AF_STATUS:
		return msm_sensor_get_af_status(s_ctrl, argp);
	case VIDIOC_MSM_SENSOR_RELEASE:
		msm_sensor_stop_stream(s_ctrl);
		return 0;
	case MSM_SD_SHUTDOWN:
		return 0;
	default:
		return -ENOIOCTLCMD;
	}
}

int32_t msm_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	struct sensorb_cfg_data *cdata = (struct sensorb_cfg_data *)argp;
	long rc = 0;
	int32_t i = 0;
	mutex_lock(s_ctrl->msm_sensor_mutex);
	CDBG("%s:%d %s cfgtype = %d\n", __func__, __LINE__,
		s_ctrl->sensordata->sensor_name, cdata->cfgtype);
	switch (cdata->cfgtype) {
	case CFG_GET_SENSOR_INFO:
		memcpy(cdata->cfg.sensor_info.sensor_name,
			s_ctrl->sensordata->sensor_name,
			sizeof(cdata->cfg.sensor_info.sensor_name));
		cdata->cfg.sensor_info.session_id =
			s_ctrl->sensordata->sensor_info->session_id;
		for (i = 0; i < SUB_MODULE_MAX; i++)
			cdata->cfg.sensor_info.subdev_id[i] =
				s_ctrl->sensordata->sensor_info->subdev_id[i];
		CDBG("%s:%d sensor name %s\n", __func__, __LINE__,
			cdata->cfg.sensor_info.sensor_name);
		CDBG("%s:%d session id %d\n", __func__, __LINE__,
			cdata->cfg.sensor_info.session_id);
		for (i = 0; i < SUB_MODULE_MAX; i++)
			CDBG("%s:%d subdev_id[%d] %d\n", __func__, __LINE__, i,
				cdata->cfg.sensor_info.subdev_id[i]);

		break;
	case CFG_GET_SENSOR_INIT_PARAMS:
		cdata->cfg.sensor_init_params =
			*s_ctrl->sensordata->sensor_init_params;
		CDBG("%s:%d init params mode %d pos %d mount %d\n", __func__,
			__LINE__,
			cdata->cfg.sensor_init_params.modes_supported,
			cdata->cfg.sensor_init_params.position,
			cdata->cfg.sensor_init_params.sensor_mount_angle);
		break;
	case CFG_SET_SLAVE_INFO: {
		struct msm_camera_sensor_slave_info sensor_slave_info;
		struct msm_sensor_power_setting_array *power_setting_array;
		int slave_index = 0;
		if (copy_from_user(&sensor_slave_info,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_sensor_slave_info))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		/* Update sensor slave address */
		if (sensor_slave_info.slave_addr) {
			s_ctrl->sensor_i2c_client->cci_client->sid =
				sensor_slave_info.slave_addr >> 1;
		}

		/* Update sensor address type */
		s_ctrl->sensor_i2c_client->addr_type =
			sensor_slave_info.addr_type;

		/* Update power up / down sequence */
		s_ctrl->power_setting_array =
			sensor_slave_info.power_setting_array;
		power_setting_array = &s_ctrl->power_setting_array;

		if (!power_setting_array->size) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		power_setting_array->power_setting = kzalloc(
			power_setting_array->size *
			sizeof(struct msm_sensor_power_setting), GFP_KERNEL);
		if (!power_setting_array->power_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(power_setting_array->power_setting,
			(void *)
			sensor_slave_info.power_setting_array.power_setting,
			power_setting_array->size *
			sizeof(struct msm_sensor_power_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(power_setting_array->power_setting);
			rc = -EFAULT;
			break;
		}
		s_ctrl->free_power_setting = true;
		CDBG("%s sensor id %x\n", __func__,
			sensor_slave_info.slave_addr);
		CDBG("%s sensor addr type %d\n", __func__,
			sensor_slave_info.addr_type);
		CDBG("%s sensor reg %x\n", __func__,
			sensor_slave_info.sensor_id_info.sensor_id_reg_addr);
		CDBG("%s sensor id %x\n", __func__,
			sensor_slave_info.sensor_id_info.sensor_id);
		for (slave_index = 0; slave_index <
			power_setting_array->size; slave_index++) {
			CDBG("%s i %d power setting %d %d %ld %d\n", __func__,
				slave_index,
				power_setting_array->power_setting[slave_index].
				seq_type,
				power_setting_array->power_setting[slave_index].
				seq_val,
				power_setting_array->power_setting[slave_index].
				config_val,
				power_setting_array->power_setting[slave_index].
				delay);
		}
		break;
	}
	case CFG_WRITE_I2C_ARRAY: {
		struct msm_camera_i2c_reg_setting conf_array;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;

		if (s_ctrl->sensor_state != MSM_SENSOR_POWER_UP) {
			pr_err("%s:%d failed: invalid state %d\n", __func__,
				__LINE__, s_ctrl->sensor_state);
			rc = -EFAULT;
			break;
		}

		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		if (!conf_array.size) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
			conf_array.size *
			sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_table(
			s_ctrl->sensor_i2c_client, &conf_array);
		kfree(reg_setting);
		break;
	}
	case CFG_SLAVE_READ_I2C: {
		struct msm_camera_i2c_read_config read_config;
		uint16_t local_data = 0;
		uint16_t orig_slave_addr = 0, read_slave_addr = 0;
		if (copy_from_user(&read_config,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_read_config))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		read_slave_addr = read_config.slave_addr;
		CDBG("%s:CFG_SLAVE_READ_I2C:", __func__);
		CDBG("%s:slave_addr=0x%x reg_addr=0x%x, data_type=%d\n",
			__func__, read_config.slave_addr,
			read_config.reg_addr, read_config.data_type);
		if (s_ctrl->sensor_i2c_client->cci_client) {
			orig_slave_addr =
				s_ctrl->sensor_i2c_client->cci_client->sid;
			s_ctrl->sensor_i2c_client->cci_client->sid =
				read_slave_addr >> 1;
		} else if (s_ctrl->sensor_i2c_client->client) {
			orig_slave_addr =
				s_ctrl->sensor_i2c_client->client->addr;
			s_ctrl->sensor_i2c_client->client->addr =
				read_slave_addr >> 1;
		} else {
			pr_err("%s: error: no i2c/cci client found.", __func__);
			rc = -EFAULT;
			break;
		}
		CDBG("%s:orig_slave_addr=0x%x, new_slave_addr=0x%x",
				__func__, orig_slave_addr,
				read_slave_addr >> 1);
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
				s_ctrl->sensor_i2c_client,
				read_config.reg_addr,
				&local_data, read_config.data_type);
		if (rc < 0) {
			pr_err("%s:%d: i2c_read failed\n", __func__, __LINE__);
			break;
		}
		if (copy_to_user((void __user *)read_config.data,
			(void *)&local_data, sizeof(uint16_t))) {
			pr_err("%s:%d copy failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		break;
	}
	case CFG_SLAVE_WRITE_I2C_ARRAY: {
		struct msm_camera_i2c_array_write_config write_config;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;
		uint16_t write_slave_addr = 0;
		uint16_t orig_slave_addr = 0;

		if (copy_from_user(&write_config,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_array_write_config))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		CDBG("%s:CFG_SLAVE_WRITE_I2C_ARRAY:", __func__);
		CDBG("%s:slave_addr=0x%x, array_size=%d\n", __func__,
			write_config.slave_addr,
			write_config.conf_array.size);

		if (!write_config.conf_array.size) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		reg_setting = kzalloc(write_config.conf_array.size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting,
				(void *)(write_config.conf_array.reg_setting),
				write_config.conf_array.size *
				sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}
		write_config.conf_array.reg_setting = reg_setting;
		write_slave_addr = write_config.slave_addr;
		if (s_ctrl->sensor_i2c_client->cci_client) {
			orig_slave_addr =
				s_ctrl->sensor_i2c_client->cci_client->sid;
			s_ctrl->sensor_i2c_client->cci_client->sid =
				write_slave_addr >> 1;
		} else if (s_ctrl->sensor_i2c_client->client) {
			orig_slave_addr =
				s_ctrl->sensor_i2c_client->client->addr;
			s_ctrl->sensor_i2c_client->client->addr =
				write_slave_addr >> 1;
		} else {
			pr_err("%s: error: no i2c/cci client found.", __func__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}
		CDBG("%s:orig_slave_addr=0x%x, new_slave_addr=0x%x",
				__func__, orig_slave_addr,
				write_slave_addr >> 1);
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_table(
			s_ctrl->sensor_i2c_client, &(write_config.conf_array));
		if (s_ctrl->sensor_i2c_client->cci_client) {
			s_ctrl->sensor_i2c_client->cci_client->sid =
				orig_slave_addr;
		} else if (s_ctrl->sensor_i2c_client->client) {
			s_ctrl->sensor_i2c_client->client->addr =
				orig_slave_addr;
		} else {
			pr_err("%s: error: no i2c/cci client found.", __func__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}
		kfree(reg_setting);
		break;
	}
	case CFG_WRITE_I2C_SEQ_ARRAY: {
		struct msm_camera_i2c_seq_reg_setting conf_array;
		struct msm_camera_i2c_seq_reg_array *reg_setting = NULL;

		if (s_ctrl->sensor_state != MSM_SENSOR_POWER_UP) {
			pr_err("%s:%d failed: invalid state %d\n", __func__,
				__LINE__, s_ctrl->sensor_state);
			rc = -EFAULT;
			break;
		}

		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_seq_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		if (!conf_array.size) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_seq_reg_array)),
			GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
			conf_array.size *
			sizeof(struct msm_camera_i2c_seq_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_seq_table(s_ctrl->sensor_i2c_client,
			&conf_array);
		kfree(reg_setting);
		break;
	}

	case CFG_POWER_UP:
		if (s_ctrl->sensor_state != MSM_SENSOR_POWER_DOWN) {
			pr_err("%s:%d failed: invalid state %d\n", __func__,
				__LINE__, s_ctrl->sensor_state);
			rc = -EFAULT;
			break;
		}
		if (s_ctrl->func_tbl->sensor_power_up) {
			if (s_ctrl->sensordata->misc_regulator)
				msm_sensor_misc_regulator(s_ctrl, 1);

			rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
			if (rc < 0) {
				pr_err("%s:%d failed rc %ld\n", __func__,
					__LINE__, rc);
				break;
			}
			s_ctrl->sensor_state = MSM_SENSOR_POWER_UP;
			pr_err("%s:%d sensor state %d\n", __func__, __LINE__,
				s_ctrl->sensor_state);
		} else {
			rc = -EFAULT;
		}
		break;

	case CFG_POWER_DOWN:
		kfree(s_ctrl->stop_setting.reg_setting);
		s_ctrl->stop_setting.reg_setting = NULL;
		if (s_ctrl->sensor_state != MSM_SENSOR_POWER_UP) {
			pr_err("%s:%d failed: invalid state %d\n", __func__,
				__LINE__, s_ctrl->sensor_state);
			rc = -EFAULT;
			break;
		}
		if (s_ctrl->func_tbl->sensor_power_down) {
			if (s_ctrl->sensordata->misc_regulator)
				msm_sensor_misc_regulator(s_ctrl, 0);

			rc = s_ctrl->func_tbl->sensor_power_down(
				s_ctrl);
			if (rc < 0) {
				pr_err("%s:%d failed rc %ld\n", __func__,
					__LINE__, rc);
				break;
			}
			s_ctrl->sensor_state = MSM_SENSOR_POWER_DOWN;
			pr_err("%s:%d sensor state %d\n", __func__, __LINE__,
				s_ctrl->sensor_state);
		} else {
			rc = -EFAULT;
		}
		break;

	case CFG_SET_STOP_STREAM_SETTING: {
		struct msm_camera_i2c_reg_setting *stop_setting =
			&s_ctrl->stop_setting;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;
		if (copy_from_user(stop_setting,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		s_ctrl->stop_setting_valid = 1;
		reg_setting = stop_setting->reg_setting;

		if (!stop_setting->size) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		stop_setting->reg_setting = kzalloc(stop_setting->size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!stop_setting->reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(stop_setting->reg_setting,
			(void *)reg_setting,
			stop_setting->size *
			sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(stop_setting->reg_setting);
			stop_setting->reg_setting = NULL;
			stop_setting->size = 0;
			rc = -EFAULT;
			break;
		}
		break;
	}
	default:
		rc = -EFAULT;
		break;
	}

	mutex_unlock(s_ctrl->msm_sensor_mutex);

	return rc;
}

static int32_t msm_sensor_power(struct v4l2_subdev *sd, int on)
{
	int rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl = get_sctrl(sd);
	mutex_lock(s_ctrl->msm_sensor_mutex);
	if (!on && s_ctrl->sensor_state == MSM_SENSOR_POWER_UP) {
		s_ctrl->func_tbl->sensor_power_down(s_ctrl);
		s_ctrl->sensor_state = MSM_SENSOR_POWER_DOWN;
	}
	if (s_ctrl->free_power_setting == true) {
		kfree(s_ctrl->power_setting_array.power_setting);
		s_ctrl->free_power_setting = false;
	}
	mutex_unlock(s_ctrl->msm_sensor_mutex);
	return rc;
}

static int32_t msm_sensor_v4l2_enum_fmt(struct v4l2_subdev *sd,
	unsigned int index, enum v4l2_mbus_pixelcode *code)
{
	struct msm_sensor_ctrl_t *s_ctrl = get_sctrl(sd);

	if ((unsigned int)index >= s_ctrl->sensor_v4l2_subdev_info_size)
		return -EINVAL;

	*code = s_ctrl->sensor_v4l2_subdev_info[index].code;
	return 0;
}

static struct v4l2_subdev_core_ops msm_sensor_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops msm_sensor_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops msm_sensor_subdev_ops = {
	.core = &msm_sensor_subdev_core_ops,
	.video  = &msm_sensor_subdev_video_ops,
};

static struct msm_sensor_fn_t msm_sensor_func_tbl = {
	.sensor_config = msm_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = msm_sensor_match_id,
};

static struct msm_camera_i2c_fn_t msm_sensor_cci_func_tbl = {
	.i2c_read = msm_camera_cci_i2c_read,
	.i2c_read_seq = msm_camera_cci_i2c_read_seq,
	.i2c_write = msm_camera_cci_i2c_write,
	.i2c_write_table = msm_camera_cci_i2c_write_table,
	.i2c_write_seq_table = msm_camera_cci_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_cci_i2c_write_table_w_microdelay,
	.i2c_util = msm_sensor_cci_i2c_util,
	.i2c_write_conf_tbl = msm_camera_cci_i2c_write_conf_tbl,
};

static struct msm_camera_i2c_fn_t msm_sensor_qup_func_tbl = {
	.i2c_read = msm_camera_qup_i2c_read,
	.i2c_read_seq = msm_camera_qup_i2c_read_seq,
	.i2c_write = msm_camera_qup_i2c_write,
	.i2c_write_table = msm_camera_qup_i2c_write_table,
	.i2c_write_seq_table = msm_camera_qup_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_qup_i2c_write_table_w_microdelay,
	.i2c_write_conf_tbl = msm_camera_qup_i2c_write_conf_tbl,
};

int32_t msm_sensor_platform_probe(struct platform_device *pdev, void *data)
{
	int32_t rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl =
		(struct msm_sensor_ctrl_t *)data;
	struct msm_camera_cci_client *cci_client = NULL;
	uint32_t session_id;
	unsigned long mount_pos;

	s_ctrl->pdev = pdev;
	s_ctrl->dev = &pdev->dev;
	CDBG("%s called data %p\n", __func__, data);
	CDBG("%s pdev name %s\n", __func__, pdev->id_entry->name);
	if (pdev->dev.of_node) {
		rc = msm_sensor_get_dt_data(pdev->dev.of_node, s_ctrl);
		if (rc < 0) {
			pr_err("%s failed line %d\n", __func__, __LINE__);
			return rc;
		}
	}
	s_ctrl->sensor_device_type = MSM_CAMERA_PLATFORM_DEVICE;
	s_ctrl->sensor_i2c_client->cci_client = kzalloc(sizeof(
		struct msm_camera_cci_client), GFP_KERNEL);
	if (!s_ctrl->sensor_i2c_client->cci_client) {
		pr_err("%s failed line %d\n", __func__, __LINE__);
		return rc;
	}
	/* TODO: get CCI subdev */
	cci_client = s_ctrl->sensor_i2c_client->cci_client;
	cci_client->cci_subdev = msm_cci_get_subdev();
	cci_client->cci_i2c_master = s_ctrl->cci_i2c_master;
	cci_client->sid =
		s_ctrl->sensordata->slave_info->sensor_slave_addr >> 1;
	cci_client->retries = 3;
	cci_client->id_map = 0;
	if (!s_ctrl->func_tbl)
		s_ctrl->func_tbl = &msm_sensor_func_tbl;
	if (!s_ctrl->sensor_i2c_client->i2c_func_tbl)
		s_ctrl->sensor_i2c_client->i2c_func_tbl =
			&msm_sensor_cci_func_tbl;
	if (!s_ctrl->sensor_v4l2_subdev_ops)
		s_ctrl->sensor_v4l2_subdev_ops = &msm_sensor_subdev_ops;
	s_ctrl->clk_info = kzalloc(sizeof(cam_8974_clk_info),
		GFP_KERNEL);
	if (!s_ctrl->clk_info) {
		pr_err("%s:%d failed nomem\n", __func__, __LINE__);
		kfree(cci_client);
		return -ENOMEM;
	}
	memcpy(s_ctrl->clk_info, cam_8974_clk_info, sizeof(cam_8974_clk_info));
	s_ctrl->clk_info_size = ARRAY_SIZE(cam_8974_clk_info);
	rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
	if (rc < 0) {
		pr_err("%s %s power up failed\n", __func__,
			s_ctrl->sensordata->sensor_name);
		kfree(s_ctrl->clk_info);
		kfree(cci_client);
		return rc;
	}

	CDBG("%s %s probe succeeded\n", __func__,
		s_ctrl->sensordata->sensor_name);
	v4l2_subdev_init(&s_ctrl->msm_sd.sd,
		s_ctrl->sensor_v4l2_subdev_ops);
	snprintf(s_ctrl->msm_sd.sd.name,
		sizeof(s_ctrl->msm_sd.sd.name), "%s",
		s_ctrl->sensordata->sensor_name);
	v4l2_set_subdevdata(&s_ctrl->msm_sd.sd, pdev);
	s_ctrl->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	media_entity_init(&s_ctrl->msm_sd.sd.entity, 0, NULL, 0);
	s_ctrl->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	s_ctrl->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_SENSOR;
	s_ctrl->msm_sd.sd.entity.name =
		s_ctrl->msm_sd.sd.name;

	mount_pos = s_ctrl->sensordata->sensor_init_params->position << 16;
	mount_pos = mount_pos | ((s_ctrl->sensordata->sensor_init_params->
					sensor_mount_angle / 90) << 8);
	s_ctrl->msm_sd.sd.entity.flags = mount_pos | MEDIA_ENT_FL_DEFAULT;

	rc = camera_init_v4l2(&s_ctrl->pdev->dev, &session_id);
	CDBG("%s rc %d session_id %d\n", __func__, rc, session_id);
	s_ctrl->sensordata->sensor_info->session_id = session_id;
	s_ctrl->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x3;
	msm_sd_register(&s_ctrl->msm_sd);
	CDBG("%s:%d\n", __func__, __LINE__);

	s_ctrl->func_tbl->sensor_power_down(s_ctrl);
	CDBG("%s:%d\n", __func__, __LINE__);
	return rc;
}

int32_t msm_sensor_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id, struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	uint32_t session_id;
	unsigned long mount_pos;

	CDBG("%s %s_i2c_probe called\n", __func__, client->name);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s %s i2c_check_functionality failed\n",
			__func__, client->name);
		rc = -EFAULT;
		return rc;
	}

	if (!client->dev.of_node) {
		CDBG("msm_sensor_i2c_probe: of_node is NULL");
		s_ctrl = (struct msm_sensor_ctrl_t *)(id->driver_data);
		if (!s_ctrl) {
			pr_err("%s:%d sensor ctrl structure NULL\n", __func__,
				__LINE__);
			return -EINVAL;
		}
		s_ctrl->sensordata = client->dev.platform_data;
	} else {
		CDBG("msm_sensor_i2c_probe: of_node exisists");
		rc = msm_sensor_get_dt_data(client->dev.of_node, s_ctrl);
		if (rc < 0) {
			pr_err("%s failed line %d\n", __func__, __LINE__);
			return rc;
		}
	}

	s_ctrl->sensor_device_type = MSM_CAMERA_I2C_DEVICE;
	if (s_ctrl->sensordata == NULL) {
		pr_err("%s %s NULL sensor data\n", __func__, client->name);
		return -EFAULT;
	}

	if (s_ctrl->sensor_i2c_client != NULL) {
		s_ctrl->sensor_i2c_client->client = client;
		s_ctrl->dev = &client->dev;
		if (s_ctrl->sensordata->slave_info->sensor_slave_addr)
			s_ctrl->sensor_i2c_client->client->addr =
				s_ctrl->sensordata->slave_info->
				sensor_slave_addr;
	} else {
		pr_err("%s %s sensor_i2c_client NULL\n",
			__func__, client->name);
		rc = -EFAULT;
		return rc;
	}

	if (!s_ctrl->func_tbl)
		s_ctrl->func_tbl = &msm_sensor_func_tbl;
	if (!s_ctrl->sensor_i2c_client->i2c_func_tbl)
		s_ctrl->sensor_i2c_client->i2c_func_tbl =
			&msm_sensor_qup_func_tbl;
	if (!s_ctrl->sensor_v4l2_subdev_ops)
		s_ctrl->sensor_v4l2_subdev_ops = &msm_sensor_subdev_ops;

	if (!client->dev.of_node) {
		s_ctrl->clk_info = kzalloc(sizeof(cam_8960_clk_info),
			GFP_KERNEL);
		if (!s_ctrl->clk_info) {
			pr_err("%s:%d failed nomem\n", __func__, __LINE__);
			return -ENOMEM;
		}
		memcpy(s_ctrl->clk_info, cam_8960_clk_info,
			sizeof(cam_8960_clk_info));
		s_ctrl->clk_info_size = ARRAY_SIZE(cam_8960_clk_info);
	} else {
		s_ctrl->clk_info = kzalloc(sizeof(cam_8610_clk_info),
			GFP_KERNEL);
		if (!s_ctrl->clk_info) {
			pr_err("%s:%d failed nomem\n", __func__, __LINE__);
			return -ENOMEM;
		}
		memcpy(s_ctrl->clk_info, cam_8610_clk_info,
			sizeof(cam_8610_clk_info));
		s_ctrl->clk_info_size = ARRAY_SIZE(cam_8610_clk_info);
	}

	rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
	if (rc < 0) {
		pr_err("%s %s power up failed\n", __func__, client->name);
		kfree(s_ctrl->clk_info);
		return rc;
	}

	CDBG("%s %s probe succeeded\n", __func__, client->name);
	snprintf(s_ctrl->msm_sd.sd.name,
		sizeof(s_ctrl->msm_sd.sd.name), "%s", id->name);
	v4l2_i2c_subdev_init(&s_ctrl->msm_sd.sd, client,
		s_ctrl->sensor_v4l2_subdev_ops);
	v4l2_set_subdevdata(&s_ctrl->msm_sd.sd, client);
	s_ctrl->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	media_entity_init(&s_ctrl->msm_sd.sd.entity, 0, NULL, 0);
	s_ctrl->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	s_ctrl->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_SENSOR;
	s_ctrl->msm_sd.sd.entity.name =
		s_ctrl->msm_sd.sd.name;

	mount_pos = s_ctrl->sensordata->sensor_init_params->position << 16;
	mount_pos = mount_pos | ((s_ctrl->sensordata->sensor_init_params->
					sensor_mount_angle / 90) << 8);
	s_ctrl->msm_sd.sd.entity.flags = mount_pos | MEDIA_ENT_FL_DEFAULT;

	rc = camera_init_v4l2(&s_ctrl->sensor_i2c_client->client->dev,
		&session_id);
	CDBG("%s rc %d session_id %d\n", __func__, rc, session_id);
	s_ctrl->sensordata->sensor_info->session_id = session_id;
	s_ctrl->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x3;
	msm_sd_register(&s_ctrl->msm_sd);
	CDBG("%s:%d\n", __func__, __LINE__);

	s_ctrl->func_tbl->sensor_power_down(s_ctrl);
	return rc;
}
