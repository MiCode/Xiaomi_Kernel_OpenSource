/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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
#include "msm_camera_dt_util.h"
#include "msm_camera_io_util.h"
#include "msm_camera_i2c_mux.h"
#include "msm_cci.h"

/*#define CONFIG_MSM_CAMERA_DT_DEBUG*/
#undef CDBG
#ifdef CONFIG_MSM_CAMERA_DT_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

int msm_camera_fill_vreg_params(struct camera_vreg_t *cam_vreg,
	int num_vreg, struct msm_sensor_power_setting *power_setting,
	uint16_t power_setting_size)
{
	uint16_t i = 0;
	int      j = 0;

	/* Validate input parameters */
	if (!cam_vreg || !power_setting) {
		pr_err("%s:%d failed: cam_vreg %p power_setting %p", __func__,
			__LINE__,  cam_vreg, power_setting);
		return -EINVAL;
	}

	/* Validate size of num_vreg */
	if (num_vreg <= 0) {
		pr_err("failed: num_vreg %d", num_vreg);
		return -EINVAL;
	}

	for (i = 0; i < power_setting_size; i++) {
		if (power_setting[i].seq_type != SENSOR_VREG)
			continue;

		switch (power_setting[i].seq_val) {
		case CAM_VDIG:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(cam_vreg[j].reg_name, "cam_vdig")) {
					pr_err("%s:%d i %d j %d cam_vdig\n",
						__func__, __LINE__, i, j);
					power_setting[i].seq_val = j;
					break;
				}
			}
			break;

		case CAM_VIO:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(cam_vreg[j].reg_name, "cam_vio")) {
					pr_err("%s:%d i %d j %d cam_vio\n",
						__func__, __LINE__, i, j);
					power_setting[i].seq_val = j;
					break;
				}
			}
			break;

		case CAM_VANA:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(cam_vreg[j].reg_name, "cam_vana")) {
					pr_err("%s:%d i %d j %d cam_vana\n",
						__func__, __LINE__, i, j);
					power_setting[i].seq_val = j;
					break;
				}
			}
			break;

		case CAM_VAF:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(cam_vreg[j].reg_name, "cam_vaf")) {
					pr_err("%s:%d i %d j %d cam_vaf\n",
						__func__, __LINE__, i, j);
					power_setting[i].seq_val = j;
					break;
				}
			}
			break;

		default:
			pr_err("%s:%d invalid seq_val %d\n", __func__,
				__LINE__, power_setting[i].seq_val);
			break;
		}
	}

	return 0;
}

int msm_sensor_get_sub_module_index(struct device_node *of_node,
				    struct  msm_sensor_info_t **s_info)
{
	int rc = 0, i = 0;
	uint32_t val = 0, count = 0;
	uint32_t *val_array = NULL;
	struct device_node *src_node = NULL;
	struct msm_sensor_info_t *sensor_info;

	sensor_info = kzalloc(sizeof(*sensor_info), GFP_KERNEL);
	if (!sensor_info) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		return -ENOMEM;
	}
	for (i = 0; i < SUB_MODULE_MAX; i++)
		sensor_info->subdev_id[i] = -1;

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
		sensor_info->subdev_id[SUB_MODULE_ACTUATOR] = val;
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
		sensor_info->subdev_id[SUB_MODULE_EEPROM] = val;
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
		sensor_info->subdev_id[SUB_MODULE_EEPROM] = val;
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
		sensor_info->subdev_id[SUB_MODULE_LED_FLASH] = val;
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
		sensor_info->subdev_id[SUB_MODULE_STROBE_FLASH] = val;
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
			sensor_info->subdev_id[SUB_MODULE_CSIPHY + i] =
								val_array[i];
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
			sensor_info->subdev_id
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

	*s_info = sensor_info;
	return rc;
ERROR:
	kfree(sensor_info);
	return rc;
}

int msm_sensor_get_dt_actuator_data(struct device_node *of_node,
				    struct msm_actuator_info **act_info)
{
	int rc = 0;
	uint32_t val = 0;
	struct msm_actuator_info *actuator_info;

	rc = of_property_read_u32(of_node, "qcom,actuator-cam-name", &val);
	CDBG("%s qcom,actuator-cam-name %d, rc %d\n", __func__, val, rc);
	if (rc < 0)
		return 0;

	actuator_info = kzalloc(sizeof(*actuator_info), GFP_KERNEL);
	if (!actuator_info) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto ERROR;
	}

	actuator_info->cam_name = val;

	rc = of_property_read_u32(of_node, "qcom,actuator-vcm-pwd", &val);
	CDBG("%s qcom,actuator-vcm-pwd %d, rc %d\n", __func__, val, rc);
	if (!rc)
		actuator_info->vcm_pwd = val;

	rc = of_property_read_u32(of_node, "qcom,actuator-vcm-enable", &val);
	CDBG("%s qcom,actuator-vcm-enable %d, rc %d\n", __func__, val, rc);
	if (!rc)
		actuator_info->vcm_enable = val;

	*act_info = actuator_info;
	return 0;
ERROR:
	kfree(actuator_info);
	return rc;
}

int msm_sensor_get_dt_csi_data(struct device_node *of_node,
	struct msm_camera_csi_lane_params **csi_lane_params)
{
	int rc = 0;
	uint32_t val = 0;
	struct msm_camera_csi_lane_params *clp;

	clp = kzalloc(sizeof(*clp), GFP_KERNEL);
	if (!clp) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}
	*csi_lane_params = clp;

	rc = of_property_read_u32(of_node, "qcom,csi-lane-assign", &val);
	CDBG("%s qcom,csi-lane-assign %x, rc %d\n", __func__, val, rc);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR;
	}
	clp->csi_lane_assign = val;

	rc = of_property_read_u32(of_node, "qcom,csi-lane-mask", &val);
	CDBG("%s qcom,csi-lane-mask %x, rc %d\n", __func__, val, rc);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR;
	}
	clp->csi_lane_mask = val;

	return rc;
ERROR:
	kfree(clp);
	return rc;
}

int msm_camera_get_dt_power_setting_data(struct device_node *of_node,
	struct camera_vreg_t *cam_vreg, int num_vreg,
	struct msm_sensor_power_setting **power_setting,
	uint16_t *power_setting_size)
{
	int rc = 0, i, j;
	int count = 0;
	const char *seq_name = NULL;
	uint32_t *array = NULL;
	struct msm_sensor_power_setting *ps;

	if (!power_setting || !power_setting_size)
		return -EINVAL;

	count = of_property_count_strings(of_node, "qcom,cam-power-seq-type");
	*power_setting_size = count;
	CDBG("%s qcom,cam-power-seq-type count %d\n", __func__, count);

	if (count <= 0)
		return 0;

	ps = kzalloc(sizeof(*ps) * count, GFP_KERNEL);
	if (!ps) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}
	*power_setting = ps;

	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(of_node,
			"qcom,cam-power-seq-type", i,
			&seq_name);
		CDBG("%s seq_name[%d] = %s\n", __func__, i,
			seq_name);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR1;
		}
		if (!strcmp(seq_name, "sensor_vreg")) {
			ps[i].seq_type = SENSOR_VREG;
			CDBG("%s:%d seq_type[%d] %d\n", __func__, __LINE__,
				i, ps[i].seq_type);
		} else if (!strcmp(seq_name, "sensor_gpio")) {
			ps[i].seq_type = SENSOR_GPIO;
			CDBG("%s:%d seq_type[%d] %d\n", __func__, __LINE__,
				i, ps[i].seq_type);
		} else if (!strcmp(seq_name, "sensor_clk")) {
			ps[i].seq_type = SENSOR_CLK;
			CDBG("%s:%d seq_type[%d] %d\n", __func__, __LINE__,
				i, ps[i].seq_type);
		} else if (!strcmp(seq_name, "sensor_i2c_mux")) {
			ps[i].seq_type = SENSOR_I2C_MUX;
			CDBG("%s:%d seq_type[%d] %d\n", __func__, __LINE__,
				i, ps[i].seq_type);
		} else {
			CDBG("%s: unrecognized seq-type\n", __func__);
			rc = -EILSEQ;
			goto ERROR1;
		}
	}


	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(of_node,
			"qcom,cam-power-seq-val", i,
			&seq_name);
		CDBG("%s seq_name[%d] = %s\n", __func__, i,
			seq_name);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR1;
		}
		switch (ps[i].seq_type) {
		case SENSOR_VREG:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(seq_name, cam_vreg[j].reg_name))
					break;
			}
			if (j < num_vreg)
				ps[i].seq_val = j;
			else
				rc = -EILSEQ;
			break;
		case SENSOR_GPIO:
			if (!strcmp(seq_name, "sensor_gpio_reset"))
				ps[i].seq_val = SENSOR_GPIO_RESET;
			else if (!strcmp(seq_name, "sensor_gpio_standby"))
				ps[i].seq_val = SENSOR_GPIO_STANDBY;
			else if (!strcmp(seq_name, "sensor_gpio_vdig"))
				ps[i].seq_val = SENSOR_GPIO_VDIG;
			else
				rc = -EILSEQ;
			break;
		case SENSOR_CLK:
			if (!strcmp(seq_name, "sensor_cam_mclk"))
				ps[i].seq_val = SENSOR_CAM_MCLK;
			else if (!strcmp(seq_name, "sensor_cam_clk"))
				ps[i].seq_val = SENSOR_CAM_CLK;
			else
				rc = -EILSEQ;
			break;
		case SENSOR_I2C_MUX:
			if (!strcmp(seq_name, "none"))
				ps[i].seq_val = 0;
			else
				rc = -EILSEQ;
			break;
		default:
			rc = -EILSEQ;
			break;
		}
		if (rc < 0) {
			CDBG("%s: unrecognized seq-val\n", __func__);
			goto ERROR1;
		}
	}

	array = kzalloc(sizeof(uint32_t) * count, GFP_KERNEL);
	if (!array) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto ERROR1;
	}


	rc = of_property_read_u32_array(of_node, "qcom,cam-power-seq-cfg-val",
		array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		if (ps[i].seq_type == SENSOR_GPIO) {
			if (array[i] == 0)
				ps[i].config_val = GPIO_OUT_LOW;
			else if (array[i] == 1)
				ps[i].config_val = GPIO_OUT_HIGH;
		} else {
			ps[i].config_val = array[i];
		}
		CDBG("%s power_setting[%d].config_val = %ld\n", __func__, i,
			ps[i].config_val);
	}

	rc = of_property_read_u32_array(of_node, "qcom,cam-power-seq-delay",
		array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		ps[i].delay = array[i];
		CDBG("%s power_setting[%d].delay = %d\n", __func__,
			i, ps[i].delay);
	}
	kfree(array);
	return rc;

ERROR2:
	kfree(array);
ERROR1:
	kfree(ps);
	power_setting_size = 0;
	return rc;
}

int msm_camera_get_dt_gpio_req_tbl(struct device_node *of_node,
	struct msm_camera_gpio_conf *gconf, uint16_t *gpio_array,
	uint16_t gpio_array_size)
{
	int rc = 0, i = 0;
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

int msm_camera_get_dt_gpio_set_tbl(struct device_node *of_node,
	struct msm_camera_gpio_conf *gconf, uint16_t *gpio_array,
	uint16_t gpio_array_size)
{
	int rc = 0, i = 0;
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

int msm_camera_init_gpio_pin_tbl(struct device_node *of_node,
	struct msm_camera_gpio_conf *gconf, uint16_t *gpio_array,
	uint16_t gpio_array_size)
{
	int rc = 0, val = 0;

	gconf->gpio_num_info = kzalloc(sizeof(struct msm_camera_gpio_num_info),
		GFP_KERNEL);
	if (!gconf->gpio_num_info) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		return rc;
	}

	if (of_property_read_bool(of_node, "qcom,gpio-vdig") == true) {
		rc = of_property_read_u32(of_node, "qcom,gpio-vdig", &val);
		if (rc < 0) {
			pr_err("%s:%d read qcom,gpio-reset failed rc %d\n",
				__func__, __LINE__, rc);
			goto ERROR;
		} else if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-reset invalid %d\n",
				__func__, __LINE__, val);
			goto ERROR;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_GPIO_VDIG] =
			gpio_array[val];
		CDBG("%s qcom,gpio-reset %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_GPIO_VDIG]);
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
	return rc;

ERROR:
	kfree(gconf->gpio_num_info);
	gconf->gpio_num_info = NULL;
	return rc;
}

int msm_camera_get_dt_vreg_data(struct device_node *of_node,
	struct camera_vreg_t **cam_vreg, int *num_vreg)
{
	int rc = 0, i = 0;
	uint32_t count = 0;
	uint32_t *vreg_array = NULL;
	struct camera_vreg_t *vreg = NULL;

	count = of_property_count_strings(of_node, "qcom,cam-vreg-name");
	CDBG("%s qcom,cam-vreg-name count %d\n", __func__, count);

	if (!count)
		return 0;

	vreg = kzalloc(sizeof(*vreg) * count, GFP_KERNEL);
	if (!vreg) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}
	*cam_vreg = vreg;
	*num_vreg = count;
	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(of_node,
			"qcom,cam-vreg-name", i,
			&vreg[i].reg_name);
		CDBG("%s reg_name[%d] = %s\n", __func__, i,
			vreg[i].reg_name);
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
		vreg[i].type = vreg_array[i];
		CDBG("%s cam_vreg[%d].type = %d\n", __func__, i,
			vreg[i].type);
	}

	rc = of_property_read_u32_array(of_node, "qcom,cam-vreg-min-voltage",
		vreg_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		vreg[i].min_voltage = vreg_array[i];
		CDBG("%s cam_vreg[%d].min_voltage = %d\n", __func__,
			i, vreg[i].min_voltage);
	}

	rc = of_property_read_u32_array(of_node, "qcom,cam-vreg-max-voltage",
		vreg_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		vreg[i].max_voltage = vreg_array[i];
		CDBG("%s cam_vreg[%d].max_voltage = %d\n", __func__,
			i, vreg[i].max_voltage);
	}

	rc = of_property_read_u32_array(of_node, "qcom,cam-vreg-op-mode",
		vreg_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		vreg[i].op_mode = vreg_array[i];
		CDBG("%s cam_vreg[%d].op_mode = %d\n", __func__, i,
			vreg[i].op_mode);
	}

	kfree(vreg_array);
	return rc;
ERROR2:
	kfree(vreg_array);
ERROR1:
	kfree(vreg);
	*num_vreg = 0;
	return rc;
}

static int msm_camera_enable_i2c_mux(struct msm_camera_i2c_conf *i2c_conf)
{
	struct v4l2_subdev *i2c_mux_sd =
		dev_get_drvdata(&i2c_conf->mux_dev->dev);
	v4l2_subdev_call(i2c_mux_sd, core, ioctl,
		VIDIOC_MSM_I2C_MUX_INIT, NULL);
	v4l2_subdev_call(i2c_mux_sd, core, ioctl,
		VIDIOC_MSM_I2C_MUX_CFG, (void *)&i2c_conf->i2c_mux_mode);
	return 0;
}

static int msm_camera_disable_i2c_mux(struct msm_camera_i2c_conf *i2c_conf)
{
	struct v4l2_subdev *i2c_mux_sd =
		dev_get_drvdata(&i2c_conf->mux_dev->dev);
	v4l2_subdev_call(i2c_mux_sd, core, ioctl,
		VIDIOC_MSM_I2C_MUX_RELEASE, NULL);
	return 0;
}

int msm_camera_power_up(struct msm_camera_power_ctrl_t *ctrl,
	enum msm_camera_device_type_t device_type,
	struct msm_camera_i2c_client *sensor_i2c_client)
{
	int rc = 0, index = 0, no_gpio = 0;
	struct msm_sensor_power_setting *power_setting = NULL;

	CDBG("%s:%d\n", __func__, __LINE__);
	if (!ctrl || !sensor_i2c_client) {
		pr_err("failed ctrl %p sensor_i2c_client %p\n", ctrl,
			sensor_i2c_client);
		return -EINVAL;
	}
	if (ctrl->gpio_conf->cam_gpiomux_conf_tbl != NULL) {
		pr_err("%s:%d mux install\n", __func__, __LINE__);
		msm_gpiomux_install(
			(struct msm_gpiomux_config *)
			ctrl->gpio_conf->cam_gpiomux_conf_tbl,
			ctrl->gpio_conf->cam_gpiomux_conf_tbl_size);
	}

	rc = msm_camera_request_gpio_table(
		ctrl->gpio_conf->cam_gpio_req_tbl,
		ctrl->gpio_conf->cam_gpio_req_tbl_size, 1);
	if (rc < 0)
		no_gpio = rc;

	for (index = 0; index < ctrl->power_setting_size; index++) {
		CDBG("%s index %d\n", __func__, index);
		power_setting = &ctrl->power_setting[index];
		CDBG("%s type %d\n", __func__, power_setting->seq_type);
		switch (power_setting->seq_type) {
		case SENSOR_CLK:
			if (power_setting->seq_val >= ctrl->clk_info_size) {
				pr_err("%s clk index %d >= max %d\n", __func__,
					power_setting->seq_val,
					ctrl->clk_info_size);
				goto power_up_failed;
			}
			if (power_setting->config_val)
				ctrl->clk_info[power_setting->seq_val].
					clk_rate = power_setting->config_val;

			rc = msm_cam_clk_enable(ctrl->dev,
				&ctrl->clk_info[0],
				(struct clk **)&power_setting->data[0],
				ctrl->clk_info_size,
				1);
			if (rc < 0) {
				pr_err("%s: clk enable failed\n",
					__func__);
				goto power_up_failed;
			}
			break;
		case SENSOR_GPIO:
			if (no_gpio) {
				pr_err("%s: request gpio failed\n", __func__);
				return no_gpio;
			}
			if (power_setting->seq_val >= SENSOR_GPIO_MAX ||
				!ctrl->gpio_conf->gpio_num_info) {
				pr_err("%s gpio index %d >= max %d\n", __func__,
					power_setting->seq_val,
					SENSOR_GPIO_MAX);
				goto power_up_failed;
			}
			CDBG("%s:%d gpio set val %d\n", __func__, __LINE__,
				ctrl->gpio_conf->gpio_num_info->gpio_num
				[power_setting->seq_val]);
			gpio_set_value_cansleep(
				ctrl->gpio_conf->gpio_num_info->gpio_num
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
			msm_camera_config_single_vreg(ctrl->dev,
				&ctrl->cam_vreg[power_setting->seq_val],
				(struct regulator **)&power_setting->data[0],
				1);
			break;
		case SENSOR_I2C_MUX:
			if (ctrl->i2c_conf && ctrl->i2c_conf->use_i2c_mux)
				msm_camera_enable_i2c_mux(ctrl->i2c_conf);
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

	if (device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = sensor_i2c_client->i2c_func_tbl->i2c_util(
			sensor_i2c_client, MSM_CCI_INIT);
		if (rc < 0) {
			pr_err("%s cci_init failed\n", __func__);
			goto power_up_failed;
		}
	}

	CDBG("%s exit\n", __func__);
	return 0;
power_up_failed:
	pr_err("%s:%d failed\n", __func__, __LINE__);
	if (device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		sensor_i2c_client->i2c_func_tbl->i2c_util(
			sensor_i2c_client, MSM_CCI_RELEASE);
	}

	for (index--; index >= 0; index--) {
		CDBG("%s index %d\n", __func__, index);
		power_setting = &ctrl->power_setting[index];
		CDBG("%s type %d\n", __func__, power_setting->seq_type);
		switch (power_setting->seq_type) {
		case SENSOR_CLK:
			msm_cam_clk_enable(ctrl->dev,
				&ctrl->clk_info[0],
				(struct clk **)&power_setting->data[0],
				ctrl->clk_info_size,
				0);
			break;
		case SENSOR_GPIO:
			gpio_set_value_cansleep(
				ctrl->gpio_conf->gpio_num_info->gpio_num
				[power_setting->seq_val], GPIOF_OUT_INIT_LOW);
			break;
		case SENSOR_VREG:
			msm_camera_config_single_vreg(ctrl->dev,
				&ctrl->cam_vreg[power_setting->seq_val],
				(struct regulator **)&power_setting->data[0],
				0);
			break;
		case SENSOR_I2C_MUX:
			if (ctrl->i2c_conf && ctrl->i2c_conf->use_i2c_mux)
				msm_camera_disable_i2c_mux(ctrl->i2c_conf);
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
		ctrl->gpio_conf->cam_gpio_req_tbl,
		ctrl->gpio_conf->cam_gpio_req_tbl_size, 0);
	return rc;
}

int msm_camera_power_down(struct msm_camera_power_ctrl_t *ctrl,
	enum msm_camera_device_type_t device_type,
	struct msm_camera_i2c_client *sensor_i2c_client)
{
	int index = 0;
	struct msm_sensor_power_setting *power_setting = NULL;

	CDBG("%s:%d\n", __func__, __LINE__);
	if (!ctrl || !sensor_i2c_client) {
		pr_err("failed ctrl %p sensor_i2c_client %p\n", ctrl,
			sensor_i2c_client);
		return -EINVAL;
	}

	if (device_type == MSM_CAMERA_PLATFORM_DEVICE)
		sensor_i2c_client->i2c_func_tbl->i2c_util(
			sensor_i2c_client, MSM_CCI_RELEASE);

	for (index = (ctrl->power_setting_size - 1); index >= 0; index--) {
		CDBG("%s index %d\n", __func__, index);
		power_setting = &ctrl->power_setting[index];
		CDBG("%s type %d\n", __func__, power_setting->seq_type);
		switch (power_setting->seq_type) {
		case SENSOR_CLK:
			msm_cam_clk_enable(ctrl->dev,
				&ctrl->clk_info[0],
				(struct clk **)&power_setting->data[0],
				ctrl->clk_info_size,
				0);
			break;
		case SENSOR_GPIO:
			if (power_setting->seq_val >= SENSOR_GPIO_MAX ||
				!ctrl->gpio_conf->gpio_num_info) {
				pr_err("%s gpio index %d >= max %d\n", __func__,
					power_setting->seq_val,
					SENSOR_GPIO_MAX);
				continue;
			}
			gpio_set_value_cansleep(
				ctrl->gpio_conf->gpio_num_info->gpio_num
				[power_setting->seq_val],
				ctrl->gpio_conf->gpio_num_info->gpio_num
				[power_setting->config_val]);
			break;
		case SENSOR_VREG:
			if (power_setting->seq_val >= CAM_VREG_MAX) {
				pr_err("%s vreg index %d >= max %d\n", __func__,
					power_setting->seq_val,
					SENSOR_GPIO_MAX);
				continue;
			}
			msm_camera_config_single_vreg(ctrl->dev,
				&ctrl->cam_vreg[power_setting->seq_val],
				(struct regulator **)&power_setting->data[0],
				0);
			break;
		case SENSOR_I2C_MUX:
			if (ctrl->i2c_conf && ctrl->i2c_conf->use_i2c_mux)
				msm_camera_disable_i2c_mux(ctrl->i2c_conf);
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
		ctrl->gpio_conf->cam_gpio_req_tbl,
		ctrl->gpio_conf->cam_gpio_req_tbl_size, 0);
	CDBG("%s exit\n", __func__);
	return 0;
}
