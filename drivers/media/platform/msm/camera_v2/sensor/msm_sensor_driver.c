/* Copyright (c) 2013-2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#define SENSOR_DRIVER_I2C "i2c_camera"
/* Header file declaration */
#include "msm_sensor.h"
#include "msm_sd.h"
#include "camera.h"
#include "msm_cci.h"
#include "msm_camera_dt_util.h"

#include <soc/qcom/camera2.h>
extern struct vendor_eeprom s_vendor_eeprom[CAMERA_VENDOR_EEPROM_COUNT_MAX];

/* Logging macro */
#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#define LCT_CAMERA_DEBUG 0

#define SENSOR_MAX_MOUNTANGLE (360)

static struct v4l2_file_operations msm_sensor_v4l2_subdev_fops;
static int32_t msm_sensor_driver_platform_probe(struct platform_device *pdev);

/* Static declaration */
static struct msm_sensor_ctrl_t *g_sctrl[MAX_CAMERAS];

static int msm_sensor_platform_remove(struct platform_device *pdev)
{
	struct msm_sensor_ctrl_t  *s_ctrl;

	pr_err("%s: sensor FREE\n", __func__);

	s_ctrl = g_sctrl[pdev->id];
	if (!s_ctrl) {
		pr_err("%s: sensor device is NULL\n", __func__);
		return 0;
	}

	msm_sensor_free_sensor_data(s_ctrl);
	kfree(s_ctrl->msm_sensor_mutex);
	kfree(s_ctrl->sensor_i2c_client);
	kfree(s_ctrl);
	g_sctrl[pdev->id] = NULL;

	return 0;
}


static const struct of_device_id msm_sensor_driver_dt_match[] = {
	{.compatible = "qcom,camera"},
	{}
};

MODULE_DEVICE_TABLE(of, msm_sensor_driver_dt_match);

static struct platform_driver msm_sensor_platform_driver = {
	.probe = msm_sensor_driver_platform_probe,
	.driver = {
		.name = "qcom,camera",
		.owner = THIS_MODULE,
		.of_match_table = msm_sensor_driver_dt_match,
	},
	.remove = msm_sensor_platform_remove,
};

static struct v4l2_subdev_info msm_sensor_driver_subdev_info[] = {
	{
		.code = MEDIA_BUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt = 1,
		.order = 0,
	},
};

static int32_t msm_sensor_driver_create_i2c_v4l_subdev
			(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint32_t session_id = 0;
	struct i2c_client *client = s_ctrl->sensor_i2c_client->client;

	CDBG("%s %s I2c probe succeeded\n", __func__, client->name);
	if (s_ctrl->bypass_video_node_creation == 0) {
		rc = camera_init_v4l2(&client->dev, &session_id);
		if (rc < 0) {
			pr_err("failed: camera_init_i2c_v4l2 rc %d", rc);
			return rc;
		}
	}

	CDBG("%s rc %d session_id %d\n", __func__, rc, session_id);
	snprintf(s_ctrl->msm_sd.sd.name,
		sizeof(s_ctrl->msm_sd.sd.name), "%s",
		s_ctrl->sensordata->sensor_name);
	v4l2_i2c_subdev_init(&s_ctrl->msm_sd.sd, client,
		s_ctrl->sensor_v4l2_subdev_ops);
	v4l2_set_subdevdata(&s_ctrl->msm_sd.sd, client);
	s_ctrl->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	media_entity_init(&s_ctrl->msm_sd.sd.entity, 0, NULL, 0);
	s_ctrl->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	s_ctrl->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_SENSOR;
	s_ctrl->msm_sd.sd.entity.name =	s_ctrl->msm_sd.sd.name;
	s_ctrl->sensordata->sensor_info->session_id = session_id;
	s_ctrl->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x3;
	rc = msm_sd_register(&s_ctrl->msm_sd);
	if (rc < 0) {
		pr_err("failed: msm_sd_register rc %d", rc);
		return rc;
	}
	msm_sensor_v4l2_subdev_fops = v4l2_subdev_fops;
#ifdef CONFIG_COMPAT
	msm_sensor_v4l2_subdev_fops.compat_ioctl32 =
		msm_sensor_subdev_fops_ioctl;
#endif
	s_ctrl->msm_sd.sd.devnode->fops =
		&msm_sensor_v4l2_subdev_fops;
	CDBG("%s:%d\n", __func__, __LINE__);
	return rc;
}

static int32_t msm_sensor_driver_create_v4l_subdev
			(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint32_t session_id = 0;

	if (s_ctrl->bypass_video_node_creation == 0) {
		rc = camera_init_v4l2(&s_ctrl->pdev->dev, &session_id);
		if (rc < 0) {
			pr_err("failed: camera_init_v4l2 rc %d", rc);
			return rc;
		}
	}

	CDBG("rc %d session_id %d", rc, session_id);
	s_ctrl->sensordata->sensor_info->session_id = session_id;

	/* Create /dev/v4l-subdevX device */
	v4l2_subdev_init(&s_ctrl->msm_sd.sd, s_ctrl->sensor_v4l2_subdev_ops);
	snprintf(s_ctrl->msm_sd.sd.name, sizeof(s_ctrl->msm_sd.sd.name), "%s",
		s_ctrl->sensordata->sensor_name);
	v4l2_set_subdevdata(&s_ctrl->msm_sd.sd, s_ctrl->pdev);
	s_ctrl->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	media_entity_init(&s_ctrl->msm_sd.sd.entity, 0, NULL, 0);
	s_ctrl->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	s_ctrl->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_SENSOR;
	s_ctrl->msm_sd.sd.entity.name = s_ctrl->msm_sd.sd.name;
	s_ctrl->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x3;
	rc = msm_sd_register(&s_ctrl->msm_sd);
	if (rc < 0) {
		pr_err("failed: msm_sd_register rc %d", rc);
		return rc;
	}
	msm_cam_copy_v4l2_subdev_fops(&msm_sensor_v4l2_subdev_fops);
#ifdef CONFIG_COMPAT
	msm_sensor_v4l2_subdev_fops.compat_ioctl32 =
		msm_sensor_subdev_fops_ioctl;
#endif
	s_ctrl->msm_sd.sd.devnode->fops =
		&msm_sensor_v4l2_subdev_fops;

	return rc;
}

static int32_t msm_sensor_fill_eeprom_subdevid_by_name(
				struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	const char *eeprom_name;
	const char *lct_eeprom_name;
	struct device_node *src_node = NULL;
	uint32_t val = 0, eeprom_name_len;
	int32_t *eeprom_subdev_id, i, userspace_probe = 0;
	int32_t count = 0;
	struct  msm_sensor_info_t *sensor_info;
	struct device_node *of_node = s_ctrl->of_node;
	const void *p;

	if (!s_ctrl->sensordata->eeprom_name || !of_node)
		return -EINVAL;
	#if LCT_CAMERA_DEBUG
	pr_err("msm_sensor_fill_eeprom_subdevid_by_name s_ctrl->sensordata->eeprom_name  = %s \n", s_ctrl->sensordata->eeprom_name);
	pr_err("msm_sensor_fill_eeprom_subdevid_by_name s_ctrl->sensordata->sensor_name = %s \n", s_ctrl->sensordata->sensor_name);
	#endif
	eeprom_name_len = strlen(s_ctrl->sensordata->eeprom_name);
	#if LCT_CAMERA_DEBUG
	pr_err("msm_sensor_fill_eeprom_subdevid_by_name eeprom_name_len  = %d \n", eeprom_name_len);
	#endif
	if (eeprom_name_len >= MAX_SENSOR_NAME)
		return -EINVAL;

	sensor_info = s_ctrl->sensordata->sensor_info;
	eeprom_subdev_id = &sensor_info->subdev_id[SUB_MODULE_EEPROM];
	/*
	 * string for eeprom name is valid, set sudev id to -1
	 *  and try to found new id
	 */
	*eeprom_subdev_id = -1;

	if (0 == eeprom_name_len)
		return 0;

	p = of_get_property(of_node, "qcom,eeprom-src", &count);
	if (!p || !count)
		return 0;

	count /= sizeof(uint32_t);
	#if LCT_CAMERA_DEBUG
	pr_err("msm_sensor_fill_eeprom_subdevid_by_name count  = %d \n", count);
	#endif
	for (i = 0; i < count; i++) {
		userspace_probe = 0;
		eeprom_name = NULL;
		lct_eeprom_name = NULL;
		src_node = of_parse_phandle(of_node, "qcom,eeprom-src", i);
		if (!src_node) {
			pr_err("eeprom src node NULL\n");
			continue;
		}
		/* In the case of eeprom probe from kernel eeprom name
			should be present, Otherwise it will throw as errors */
		rc = of_property_read_string(src_node, "qcom,eeprom-name",
			&eeprom_name);
		if (rc < 0) {
			pr_err("%s:%d Eeprom userspace probe for %s\n",
				__func__, __LINE__,
				s_ctrl->sensordata->eeprom_name);
			of_node_put(src_node);
			userspace_probe = 1;

			if (count > 5)
				return -EINVAL;
		}
		if (!userspace_probe &&
			strcmp(eeprom_name, s_ctrl->sensordata->eeprom_name))
			continue;


		if (userspace_probe == 1)
		{
			rc = of_property_read_string(src_node, "qcom,lct_eeprom-name", &lct_eeprom_name);
			if (rc < 0)
			{
				#if LCT_CAMERA_DEBUG
				pr_err("%s this eeprom not config  qcom,lct_eeprom-name rc = %d\n", __func__, rc);
				#endif
			}
			else
			{
				#if LCT_CAMERA_DEBUG
				pr_err("msm_sensor_fill_eeprom_subdevid_by_name lct_eeprom_name = %s\n", lct_eeprom_name);
				#endif
				if (strcmp(s_ctrl->sensordata->eeprom_name, lct_eeprom_name))
				{
					rc = 0;
					continue;
				}

			}

		}

		rc = of_property_read_u32(src_node, "cell-index", &val);
		if (rc < 0) {
			pr_err("%s qcom,eeprom cell index %d, rc %d\n",
				__func__, val, rc);
			of_node_put(src_node);
			if (userspace_probe)
				return -EINVAL;
			continue;
		}

		*eeprom_subdev_id = val;
		CDBG("%s:%d Eeprom subdevice id is %d\n",
			__func__, __LINE__, val);
		of_node_put(src_node);
		src_node = NULL;
		break;
	}

	return rc;
}

static int32_t msm_sensor_fill_actuator_subdevid_by_name(
				struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct device_node *src_node = NULL;
	uint32_t val = 0, actuator_name_len;
	int32_t *actuator_subdev_id;
	struct  msm_sensor_info_t *sensor_info;
	struct device_node *of_node = s_ctrl->of_node;

	if (!s_ctrl->sensordata->actuator_name || !of_node)
		return -EINVAL;

	actuator_name_len = strlen(s_ctrl->sensordata->actuator_name);
	if (actuator_name_len >= MAX_SENSOR_NAME)
		return -EINVAL;

	sensor_info = s_ctrl->sensordata->sensor_info;
	actuator_subdev_id = &sensor_info->subdev_id[SUB_MODULE_ACTUATOR];
	/*
	 * string for actuator name is valid, set sudev id to -1
	 * and try to found new id
	 */
	*actuator_subdev_id = -1;

	if (0 == actuator_name_len)
		return 0;

	src_node = of_parse_phandle(of_node, "qcom,actuator-src", 0);
	if (!src_node) {
		CDBG("%s:%d src_node NULL\n", __func__, __LINE__);
	} else {
		rc = of_property_read_u32(src_node, "cell-index", &val);
		CDBG("%s qcom,actuator cell index %d, rc %d\n", __func__,
			val, rc);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			return -EINVAL;
		}
		*actuator_subdev_id = val;
		of_node_put(src_node);
		src_node = NULL;
	}

	return rc;
}

static int32_t msm_sensor_fill_laser_led_subdevid_by_name(
				struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct device_node *src_node = NULL;
	uint32_t val = 0;
	int32_t *laser_led_subdev_id;
	struct  msm_sensor_info_t *sensor_info;
	struct device_node *of_node = s_ctrl->of_node;

	if (!of_node)
		return -EINVAL;

	sensor_info = s_ctrl->sensordata->sensor_info;
	laser_led_subdev_id = &sensor_info->subdev_id[SUB_MODULE_LASER_LED];
	/* set sudev id to -1 and try to found new id */
	*laser_led_subdev_id = -1;


	src_node = of_parse_phandle(of_node, "qcom,laserled-src", 0);
	if (!src_node) {
		CDBG("%s:%d src_node NULL\n", __func__, __LINE__);
	} else {
		rc = of_property_read_u32(src_node, "cell-index", &val);
		CDBG("%s qcom,laser led cell index %d, rc %d\n", __func__,
			val, rc);
		of_node_put(src_node);
		src_node = NULL;
		if (rc < 0) {
			pr_err("%s cell index not found %d\n",
				__func__, __LINE__);
			return -EINVAL;
		}
		*laser_led_subdev_id = val;
	}

	return rc;
}

static int32_t msm_sensor_fill_flash_subdevid_by_name(
				struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct device_node *src_node = NULL;
	uint32_t val = 0, flash_name_len;
	int32_t *flash_subdev_id;
	struct  msm_sensor_info_t *sensor_info;
	struct device_node *of_node = s_ctrl->of_node;

	if (!of_node || !s_ctrl->sensordata->flash_name)
		return -EINVAL;

	sensor_info = s_ctrl->sensordata->sensor_info;
	flash_subdev_id = &sensor_info->subdev_id[SUB_MODULE_LED_FLASH];

	*flash_subdev_id = -1;

	flash_name_len = strlen(s_ctrl->sensordata->flash_name);
	if (flash_name_len >= MAX_SENSOR_NAME)
		return -EINVAL;

	if (flash_name_len == 0)
		return 0;

	src_node = of_parse_phandle(of_node, "qcom,led-flash-src", 0);
	if (!src_node) {
		CDBG("%s:%d src_node NULL\n", __func__, __LINE__);
	} else {
		rc = of_property_read_u32(src_node, "cell-index", &val);
		CDBG("%s qcom,flash cell index %d, rc %d\n", __func__,
			val, rc);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			return -EINVAL;
		}
		*flash_subdev_id = val;
		of_node_put(src_node);
		src_node = NULL;
	}
	return rc;
}

static int32_t msm_sensor_fill_ois_subdevid_by_name(
				struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct device_node *src_node = NULL;
	uint32_t val = 0, ois_name_len;
	int32_t *ois_subdev_id;
	struct  msm_sensor_info_t *sensor_info;
	struct device_node *of_node = s_ctrl->of_node;

	if (!s_ctrl->sensordata->ois_name || !of_node)
		return -EINVAL;

	ois_name_len = strlen(s_ctrl->sensordata->ois_name);
	if (ois_name_len >= MAX_SENSOR_NAME)
		return -EINVAL;

	sensor_info = s_ctrl->sensordata->sensor_info;
	ois_subdev_id = &sensor_info->subdev_id[SUB_MODULE_OIS];
	/*
	 * string for ois name is valid, set sudev id to -1
	 * and try to found new id
	 */
	*ois_subdev_id = -1;

	if (0 == ois_name_len)
		return 0;

	src_node = of_parse_phandle(of_node, "qcom,ois-src", 0);
	if (!src_node) {
		CDBG("%s:%d src_node NULL\n", __func__, __LINE__);
	} else {
		rc = of_property_read_u32(src_node, "cell-index", &val);
		CDBG("%s qcom,ois cell index %d, rc %d\n", __func__,
			val, rc);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			return -EINVAL;
		}
		*ois_subdev_id = val;
		of_node_put(src_node);
		src_node = NULL;
	}

	return rc;
}

static int32_t msm_sensor_fill_slave_info_init_params(
	struct msm_camera_sensor_slave_info *slave_info,
	struct msm_sensor_info_t *sensor_info)
{
	struct msm_sensor_init_params *sensor_init_params;
	if (!slave_info ||  !sensor_info)
		return -EINVAL;

	sensor_init_params = &slave_info->sensor_init_params;
	if (INVALID_CAMERA_B != sensor_init_params->position)
		sensor_info->position =
			sensor_init_params->position;

	if (SENSOR_MAX_MOUNTANGLE > sensor_init_params->sensor_mount_angle) {
		sensor_info->sensor_mount_angle =
			sensor_init_params->sensor_mount_angle;
		sensor_info->is_mount_angle_valid = 1;
	}

	if (CAMERA_MODE_INVALID != sensor_init_params->modes_supported)
		sensor_info->modes_supported =
			sensor_init_params->modes_supported;

	return 0;
}


static int32_t msm_sensor_validate_slave_info(
	struct msm_sensor_info_t *sensor_info)
{
	if (INVALID_CAMERA_B == sensor_info->position) {
		sensor_info->position = BACK_CAMERA_B;
		CDBG("%s:%d Set default sensor position\n",
			__func__, __LINE__);
	}
	if (CAMERA_MODE_INVALID == sensor_info->modes_supported) {
		sensor_info->modes_supported = CAMERA_MODE_2D_B;
		CDBG("%s:%d Set default sensor modes_supported\n",
			__func__, __LINE__);
	}
	if (SENSOR_MAX_MOUNTANGLE <= sensor_info->sensor_mount_angle) {
		sensor_info->sensor_mount_angle = 0;
		CDBG("%s:%d Set default sensor mount angle\n",
			__func__, __LINE__);
		sensor_info->is_mount_angle_valid = 1;
	}
	return 0;
}

#ifdef CONFIG_COMPAT
static int32_t msm_sensor_get_pw_settings_compat(
	struct msm_sensor_power_setting *ps,
	struct msm_sensor_power_setting *us_ps, uint32_t size)
{
	int32_t rc = 0, i = 0;
	struct msm_sensor_power_setting32 *ps32 =
		kzalloc(sizeof(*ps32) * size, GFP_KERNEL);

	if (!ps32) {
		pr_err("failed: no memory ps32");
		return -ENOMEM;
	}
	if (copy_from_user(ps32, (void *)us_ps, sizeof(*ps32) * size)) {
		pr_err("failed: copy_from_user");
		kfree(ps32);
		return -EFAULT;
	}
	for (i = 0; i < size; i++) {
		ps[i].config_val = ps32[i].config_val;
		ps[i].delay = ps32[i].delay;
		ps[i].seq_type = ps32[i].seq_type;
		ps[i].seq_val = ps32[i].seq_val;
	}
	kfree(ps32);
	return rc;
}
#endif

static int32_t msm_sensor_create_pd_settings(void *setting,
	struct msm_sensor_power_setting *pd, uint32_t size_down,
	struct msm_sensor_power_setting *pu)
{
	int32_t rc = 0;
	int c, end;
	struct msm_sensor_power_setting pd_tmp;

	pr_err("Generating power_down_setting");

#ifdef CONFIG_COMPAT
	if (is_compat_task()) {
		rc = msm_sensor_get_pw_settings_compat(
			pd, pu, size_down);
		if (rc < 0) {
			pr_err("failed");
			return -EFAULT;
		}
	} else
#endif
	{
		if (copy_from_user(pd, (void *)pu, sizeof(*pd) * size_down)) {
			pr_err("failed: copy_from_user");
			return -EFAULT;
		}
	}
	/* reverse */
	end = size_down - 1;
	for (c = 0; c < size_down/2; c++) {
		pd_tmp = pd[c];
		pd[c] = pd[end];
		pd[end] = pd_tmp;
		end--;
	}
	return rc;
}

static int32_t msm_sensor_get_power_down_settings(void *setting,
	struct msm_camera_sensor_slave_info *slave_info,
	struct msm_camera_power_ctrl_t *power_info)
{
	int32_t rc = 0;
	uint16_t size_down = 0;
	uint16_t i = 0;
	struct msm_sensor_power_setting *pd = NULL;

	/* DOWN */
	size_down = slave_info->power_setting_array.size_down;
	if (!size_down || size_down > MAX_POWER_CONFIG)
		size_down = slave_info->power_setting_array.size;
	/* Validate size_down */
	if (size_down > MAX_POWER_CONFIG) {
		pr_err("failed: invalid size_down %d", size_down);
		return -EINVAL;
	}
	/* Allocate memory for power down setting */
	pd = kzalloc(sizeof(*pd) * size_down, GFP_KERNEL);
	if (!pd)
		return -EFAULT;

	if (slave_info->power_setting_array.power_down_setting) {
#ifdef CONFIG_COMPAT
		if (is_compat_task()) {
			rc = msm_sensor_get_pw_settings_compat(
				pd, slave_info->power_setting_array.
				power_down_setting, size_down);
			if (rc < 0) {
				pr_err("failed");
				kfree(pd);
				return -EFAULT;
			}
		} else
#endif
		if (copy_from_user(pd, (void *)slave_info->power_setting_array.
				power_down_setting, sizeof(*pd) * size_down)) {
			pr_err("failed: copy_from_user");
			kfree(pd);
			return -EFAULT;
		}
	} else {

		rc = msm_sensor_create_pd_settings(setting, pd, size_down,
			slave_info->power_setting_array.power_setting);
		if (rc < 0) {
			pr_err("failed");
			kfree(pd);
			return -EFAULT;
		}
	}

	/* Fill power down setting and power down setting size */
	power_info->power_down_setting = pd;
	power_info->power_down_setting_size = size_down;

	/* Print power setting */
	for (i = 0; i < size_down; i++) {
		CDBG("DOWN seq_type %d seq_val %d config_val %ld delay %d",
			pd[i].seq_type, pd[i].seq_val,
			pd[i].config_val, pd[i].delay);
	}
	return rc;
}

static int32_t msm_sensor_get_power_up_settings(void *setting,
	struct msm_camera_sensor_slave_info *slave_info,
	struct msm_camera_power_ctrl_t *power_info)
{
	int32_t rc = 0;
	uint16_t size = 0;
	uint16_t i = 0;
	struct msm_sensor_power_setting *pu = NULL;

	size = slave_info->power_setting_array.size;

	/* Validate size */
	if ((size == 0) || (size > MAX_POWER_CONFIG)) {
		pr_err("failed: invalid power_setting size_up = %d\n", size);
		return -EINVAL;
	}

	/* Allocate memory for power up setting */
	pu = kzalloc(sizeof(*pu) * size, GFP_KERNEL);
	if (!pu)
		return -ENOMEM;

#ifdef CONFIG_COMPAT
	if (is_compat_task()) {
		rc = msm_sensor_get_pw_settings_compat(pu,
			slave_info->power_setting_array.
				power_setting, size);
		if (rc < 0) {
			pr_err("failed");
			kfree(pu);
			return -EFAULT;
		}
	} else
#endif
	{
		if (copy_from_user(pu,
			(void *)slave_info->power_setting_array.power_setting,
			sizeof(*pu) * size)) {
			pr_err("failed: copy_from_user");
			kfree(pu);
			return -EFAULT;
		}
	}

	/* Print power setting */
	for (i = 0; i < size; i++) {
		CDBG("UP seq_type %d seq_val %d config_val %ld delay %d",
			pu[i].seq_type, pu[i].seq_val,
			pu[i].config_val, pu[i].delay);
	}


	/* Fill power up setting and power up setting size */
	power_info->power_setting = pu;
	power_info->power_setting_size = size;

	return rc;
}

static int32_t msm_sensor_get_power_settings(void *setting,
	struct msm_camera_sensor_slave_info *slave_info,
	struct msm_camera_power_ctrl_t *power_info)
{
	int32_t rc = 0;

	rc = msm_sensor_get_power_up_settings(setting, slave_info, power_info);
	if (rc < 0) {
		pr_err("failed");
		return -EINVAL;
	}

	rc = msm_sensor_get_power_down_settings(setting, slave_info,
		power_info);
	if (rc < 0) {
		pr_err("failed");
		return -EINVAL;
	}
	return rc;
}

static void msm_sensor_fill_sensor_info(struct msm_sensor_ctrl_t *s_ctrl,
	struct msm_sensor_info_t *sensor_info, char *entity_name)
{
	uint32_t i;

	if (!s_ctrl || !sensor_info) {
		pr_err("%s:failed\n", __func__);
		return;
	}

	strlcpy(sensor_info->sensor_name, s_ctrl->sensordata->sensor_name,
		MAX_SENSOR_NAME);

	sensor_info->session_id = s_ctrl->sensordata->sensor_info->session_id;

	s_ctrl->sensordata->sensor_info->subdev_id[SUB_MODULE_SENSOR] =
		s_ctrl->sensordata->sensor_info->session_id;
	for (i = 0; i < SUB_MODULE_MAX; i++) {
		sensor_info->subdev_id[i] =
			s_ctrl->sensordata->sensor_info->subdev_id[i];
		sensor_info->subdev_intf[i] =
			s_ctrl->sensordata->sensor_info->subdev_intf[i];
	}

	sensor_info->is_mount_angle_valid =
		s_ctrl->sensordata->sensor_info->is_mount_angle_valid;
	sensor_info->sensor_mount_angle =
		s_ctrl->sensordata->sensor_info->sensor_mount_angle;
	sensor_info->modes_supported =
		s_ctrl->sensordata->sensor_info->modes_supported;
	sensor_info->position =
		s_ctrl->sensordata->sensor_info->position;

	strlcpy(entity_name, s_ctrl->msm_sd.sd.entity.name, MAX_SENSOR_NAME);
}

/* static function definition */
static int32_t msm_sensor_driver_is_special_support(
	struct msm_sensor_ctrl_t *s_ctrl,
	char *sensor_name)
{
	int32_t rc = 0, i = 0;
	struct msm_camera_sensor_board_info *sensordata = s_ctrl->sensordata;

	for (i = 0; i < sensordata->special_support_size; i++) {
		if (!strcmp(sensordata->special_support_sensors[i],
						 sensor_name)) {
			rc = TRUE;
			break;
		}
	}
	return rc;
}
/* add sensor info for factory mode
   begin
*/
static struct kobject *msm_sensor_device;
static char module_info[256] = {0};

void msm_sensor_set_module_info(struct msm_sensor_ctrl_t *s_ctrl)
{
	printk(" s_ctrl->sensordata->camera_type = %d\n", s_ctrl->sensordata->sensor_info->position);

	switch (s_ctrl->sensordata->sensor_info->position) {
		case BACK_CAMERA_B:
			strcat(module_info, "back: ");
			break;
		case AUX_CAMERA_B:
			strcat(module_info, "back_aux: ");
			break;
		case FRONT_CAMERA_B:
			strcat(module_info, "front: ");
			break;
		case FRONT_AUX_CAMERA_B:
			strcat(module_info, "front_aux: ");
			break;
		default:
			strcat(module_info, "unknown: ");
			break;
	}
	strcat(module_info, s_ctrl->sensordata->sensor_name);
	strcat(module_info, "\n");
}

static ssize_t msm_sensor_module_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t rc = 0;

	sprintf(buf, "%s\n", module_info);
	rc = strlen(buf) + 1;

	return rc;
}

static DEVICE_ATTR(sensor, 0444, msm_sensor_module_id_show, NULL);

int32_t msm_sensor_init_device_name(void)
{
	int32_t rc = 0;
	CDBG("%s %d\n", __func__, __LINE__);
	if (msm_sensor_device != NULL){
		pr_err("Macle android_camera already created\n");
		return 0;
	}
	msm_sensor_device = kobject_create_and_add("android_camera", NULL);
	if (msm_sensor_device == NULL) {
		printk("%s: subsystem_register failed\n", __func__);
		rc = -ENOMEM;
		return rc ;
	}
	rc = sysfs_create_file(msm_sensor_device, &dev_attr_sensor.attr);
	if (rc) {
		printk("%s: sysfs_create_file failed\n", __func__);
		kobject_del(msm_sensor_device);
	}

	return 0 ;
}
/* add sensor info for factory mode
   end
*/


static uint16_t msm_sensor_get_sensor_id_ovti_13855(struct msm_sensor_ctrl_t *s_ctrl, char *sensor_fusion_id)
{
	int rc = 0;
	int i = 0;
	uint16_t sensorid[16] = {0};
	uint16_t temp = 0;
	uint32_t start_add = 0x7000;
	struct msm_camera_i2c_client *sensor_i2c_client;

	CDBG("%s:%d E \n", __func__, __LINE__);

	sensor_i2c_client = s_ctrl->sensor_i2c_client;

	rc = sensor_i2c_client->i2c_func_tbl->i2c_write(
		sensor_i2c_client, 0x0100,
		0x01, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s:lct write 0x0100 failed\n", __func__);
		return rc;
	}

	 sensor_i2c_client->i2c_func_tbl->i2c_read(
		sensor_i2c_client, 0x5000,
		&temp, MSM_CAMERA_I2C_WORD_DATA);

	temp &= ~(1<<3);

	 sensor_i2c_client->i2c_func_tbl->i2c_write(
		sensor_i2c_client, 0x5000,
		temp, MSM_CAMERA_I2C_BYTE_DATA);
	 sensor_i2c_client->i2c_func_tbl->i2c_write(
		sensor_i2c_client, 0x3d84,
		0x40, MSM_CAMERA_I2C_BYTE_DATA);

	 sensor_i2c_client->i2c_func_tbl->i2c_write(
		sensor_i2c_client, 0x3d88,
		0x70, MSM_CAMERA_I2C_BYTE_DATA);

	 sensor_i2c_client->i2c_func_tbl->i2c_write(
		sensor_i2c_client, 0x3d89,
		0x00, MSM_CAMERA_I2C_BYTE_DATA);

	 sensor_i2c_client->i2c_func_tbl->i2c_write(
		sensor_i2c_client, 0x3d8a,
		0x70, MSM_CAMERA_I2C_BYTE_DATA);

	 sensor_i2c_client->i2c_func_tbl->i2c_write(
		sensor_i2c_client, 0x3d8b,
		0x0f, MSM_CAMERA_I2C_BYTE_DATA);

	 sensor_i2c_client->i2c_func_tbl->i2c_write(
		sensor_i2c_client, 0x3d81,
		0x01, MSM_CAMERA_I2C_BYTE_DATA);

	mdelay(1);
	for (i = 0; i < 16; i++){
	 sensor_i2c_client->i2c_func_tbl->i2c_read(
		sensor_i2c_client, start_add,
		&sensorid[i], MSM_CAMERA_I2C_WORD_DATA);
	CDBG("%s:lct read from start_add %x sensrid[%d] %d\n", __func__, start_add, i, sensorid[i]);
	start_add += 1;
	}

	 sensor_i2c_client->i2c_func_tbl->i2c_write(
		sensor_i2c_client, 0x0100,
		0x00, MSM_CAMERA_I2C_BYTE_DATA);
	sprintf(sensor_fusion_id, "%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x",
		sensorid[0],
		sensorid[1],
		sensorid[2],
		sensorid[3],
		sensorid[4],
		sensorid[5],
		sensorid[6],
		sensorid[7],
		sensorid[8],
		sensorid[9],
		sensorid[10],
		sensorid[11],
		sensorid[12],
		sensorid[13],
		sensorid[14],
		sensorid[15]);
	return rc;
}

static uint16_t msm_sensor_get_sensor_id_sony_486(struct msm_sensor_ctrl_t *s_ctrl, char *sensor_fusion_id)
{
	int rc = 0;
	int i = 0;
	uint16_t sensorid[11] = {0};
	uint32_t start_add = 0x0A27;
	struct msm_camera_i2c_client *sensor_i2c_client;

	CDBG("%s:%d E \n", __func__, __LINE__);
	sensor_i2c_client = s_ctrl->sensor_i2c_client;

	rc = sensor_i2c_client->i2c_func_tbl->i2c_write(
		sensor_i2c_client, 0x0100,
		0x01, MSM_CAMERA_I2C_WORD_DATA);
	mdelay(1);
	if (rc < 0) {
		pr_err("%s:lct write 0x0100 failed\n", __func__);
		return rc;
	}

	sensor_i2c_client->i2c_func_tbl->i2c_write(
		sensor_i2c_client, 0x0A02,
		0x0B, MSM_CAMERA_I2C_BYTE_DATA);

	sensor_i2c_client->i2c_func_tbl->i2c_write(
		sensor_i2c_client, 0x0A00,
		0x01, MSM_CAMERA_I2C_BYTE_DATA);
	mdelay(20);

	for (i = 0; i < 11; i++){
	rc = sensor_i2c_client->i2c_func_tbl->i2c_read(
		sensor_i2c_client, start_add,
		&sensorid[i], MSM_CAMERA_I2C_WORD_DATA);
	CDBG("%s:lct++++++++++++ read from start_add %x sensrid[%d] %d\n", __func__, start_add, i, sensorid[i]);
	start_add += 1;
	}

	sprintf(sensor_fusion_id, "%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x",
		sensorid[0],
		sensorid[1],
		sensorid[2],
		sensorid[3],
		sensorid[4],
		sensorid[5],
		sensorid[6],
		sensorid[7],
		sensorid[8],
		sensorid[9],
		sensorid[10]);

	return rc;
}

static uint16_t msm_sensor_get_sensor_id_sony_376(struct msm_sensor_ctrl_t *s_ctrl, char *sensor_fusion_id)
{
	int rc = 0;
	int i = 0;
	uint16_t sensorid[11] = {0};
	uint32_t start_add = 0x0A20;
	struct msm_camera_i2c_client *sensor_i2c_client;

	CDBG("%s:%d E \n", __func__, __LINE__);
	sensor_i2c_client = s_ctrl->sensor_i2c_client;

	rc = sensor_i2c_client->i2c_func_tbl->i2c_write(
		sensor_i2c_client, 0x0100,
		0x01, MSM_CAMERA_I2C_WORD_DATA);
	mdelay(1);
	if (rc < 0) {
		pr_err("%s:lct write 0x0100 failed\n", __func__);
		return rc;
	}

	sensor_i2c_client->i2c_func_tbl->i2c_write(
		sensor_i2c_client, 0x0A02,
		0x27, MSM_CAMERA_I2C_BYTE_DATA);

	sensor_i2c_client->i2c_func_tbl->i2c_write(
		sensor_i2c_client, 0x0A00,
		0x01, MSM_CAMERA_I2C_BYTE_DATA);
	mdelay(20);

	for (i = 0; i < 11; i++){
	rc = sensor_i2c_client->i2c_func_tbl->i2c_read(
		sensor_i2c_client, start_add,
		&sensorid[i], MSM_CAMERA_I2C_WORD_DATA);
	CDBG("%s:lct++++++++++++ read from start_add %x sensrid[%d] %d\n", __func__, start_add, i, sensorid[i]);
	start_add += 1;
	}

	sprintf(sensor_fusion_id, "%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x",
		sensorid[0],
		sensorid[1],
		sensorid[2],
		sensorid[3],
		sensorid[4],
		sensorid[5],
		sensorid[6],
		sensorid[7],
		sensorid[8],
		sensorid[9],
		sensorid[10]);

	return rc;
}

static uint16_t msm_sensor_get_sensor_id_samsung_5e8(struct msm_sensor_ctrl_t *s_ctrl, char *sensor_fusion_id)
{
	int rc = 0;
	int i = 0;
	uint16_t sensorid[16] = {0};
	uint32_t start_add = 0x0a04;
	struct msm_camera_i2c_client *sensor_i2c_client;

	CDBG("%s:%d E \n", __func__, __LINE__);
	sensor_i2c_client = s_ctrl->sensor_i2c_client;

	rc = sensor_i2c_client->i2c_func_tbl->i2c_write(
		sensor_i2c_client, 0x0100,
		0x00, MSM_CAMERA_I2C_BYTE_DATA);
	mdelay(1);
	if (rc < 0) {
		pr_err("%s:lct write 0x0100 failed\n", __func__);
		return rc;
	}

	 sensor_i2c_client->i2c_func_tbl->i2c_write(
		sensor_i2c_client, 0x0a00,
		0x04, MSM_CAMERA_I2C_BYTE_DATA);

	 sensor_i2c_client->i2c_func_tbl->i2c_write(
		sensor_i2c_client, 0x0a02,
		0x00, MSM_CAMERA_I2C_BYTE_DATA);

	 sensor_i2c_client->i2c_func_tbl->i2c_write(
		sensor_i2c_client, 0x0a00,
		0x01, MSM_CAMERA_I2C_BYTE_DATA);

	mdelay(1);

	for (i = 0; i < 16; i++){
	 sensor_i2c_client->i2c_func_tbl->i2c_read(
		sensor_i2c_client, start_add,
		&sensorid[i], MSM_CAMERA_I2C_WORD_DATA);
	CDBG("%s:lct read from reg_add %x sensorid[%d] %d\n", __func__, start_add, i, sensorid[i]);
	start_add += 1;
	}

	sprintf(sensor_fusion_id, "%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x",
		sensorid[0],
		sensorid[1],
		sensorid[2],
		sensorid[3],
		sensorid[4],
		sensorid[5],
		sensorid[6],
		sensorid[7],
		sensorid[8],
		sensorid[9],
		sensorid[10],
		sensorid[12],
		sensorid[13],
		sensorid[14],
		sensorid[15]);
	return rc;
}

static uint16_t msm_sensor_get_sensor_id_samsung_2L7(struct msm_sensor_ctrl_t *s_ctrl, char *sensor_fusion_id)
{
	int rc = 0;
	int i = 0;
	uint16_t sensorid[16] = {0};
	uint32_t start_add = 0x0A24;
	struct msm_camera_i2c_client *sensor_i2c_client;

	CDBG("%s:%d E \n", __func__, __LINE__);
	sensor_i2c_client = s_ctrl->sensor_i2c_client;

	rc = sensor_i2c_client->i2c_func_tbl->i2c_write(
		sensor_i2c_client, 0x0100,
		0x0100, MSM_CAMERA_I2C_BYTE_DATA);
	mdelay(1);
	if (rc < 0) {
		pr_err("%s:lct write 0x0100 failed\n", __func__);
		return rc;
	}

	sensor_i2c_client->i2c_func_tbl->i2c_write(
		sensor_i2c_client, 0x0A02,
		0x0000, MSM_CAMERA_I2C_WORD_DATA);

	sensor_i2c_client->i2c_func_tbl->i2c_write(
		sensor_i2c_client, 0x0A00,
		0x0100, MSM_CAMERA_I2C_WORD_DATA);
	mdelay(95);

	for (i = 0; i < 16; i++){
	rc = sensor_i2c_client->i2c_func_tbl->i2c_read(
		sensor_i2c_client, start_add,
		&sensorid[i], MSM_CAMERA_I2C_WORD_DATA);
	CDBG("%s:lct read from start_add %x sensrid[%d] %d\n", __func__, start_add, i, sensorid[i]);
	start_add += 1;
	}

	/*sensor_i2c_client->i2c_func_tbl->i2c_read(
			sensor_i2c_client,0x0a00,
			&temp, MSM_CAMERA_I2C_WORD_DATA);
	pr_err("%s:lct read from 0x0a00 value %x\n", __func__,temp);*/

	sensor_i2c_client->i2c_func_tbl->i2c_write(
		sensor_i2c_client, 0x0a00,
		0x0000, MSM_CAMERA_I2C_WORD_DATA);

	sprintf(sensor_fusion_id, "%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x%04x",
		sensorid[0],
		sensorid[1],
		sensorid[2],
		sensorid[3],
		sensorid[4],
		sensorid[5],
		sensorid[6],
		sensorid[7],
		sensorid[8],
		sensorid[9],
		sensorid[10],
		sensorid[12],
		sensorid[13],
		sensorid[14],
		sensorid[15]);

	return rc;

}

static struct kobject *msm_sensorid_device;
static char sensor_fusion_id[512] = {0};

void msm_sensor_set_sesnor_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	char  sensor_fusion_id_tmp[90] = {0};
	int rc = 0;
	CDBG("lct s_ctrl->sensordata->camera_type = %d\n", s_ctrl->sensordata->sensor_info->position);

	switch (s_ctrl->sensordata->sensor_info->position) {
		case BACK_CAMERA_B:
			strcat(sensor_fusion_id, "back: ");
			break;
		case AUX_CAMERA_B:
			strcat(sensor_fusion_id, "back_aux: ");
			break;
		case FRONT_CAMERA_B:
			strcat(sensor_fusion_id, "front: ");
			break;
		case FRONT_AUX_CAMERA_B:
			strcat(sensor_fusion_id, "front_aux: ");
			break;
		default:
			strcat(sensor_fusion_id, "unknow: ");
			break;

	}

    if ((!strcmp("whyred_imx486_ofilm_global_i", s_ctrl->sensordata->sensor_name)) || (!strcmp("whyred_imx486_qtech_global_ii", s_ctrl->sensordata->sensor_name)) || (!strcmp("wayne_imx486_sunny_i", s_ctrl->sensordata->sensor_name)) || (!strcmp("wayne_imx486_ofilm_ii", s_ctrl->sensordata->sensor_name))){
		rc = msm_sensor_get_sensor_id_sony_486(s_ctrl, sensor_fusion_id_tmp);
		if (rc < 0){
		pr_err("%s:%d lct read sensor %s fusion id failed\n", __func__, __LINE__, s_ctrl->sensordata->sensor_name);
			}
		}
	if ((!strcmp("whyred_imx376_ofilm_global_i", s_ctrl->sensordata->sensor_name)) || (!strcmp("whyred_imx376_sunny_global_ii", s_ctrl->sensordata->sensor_name)) || (!strcmp("wayne_imx376_sunny_back_i", s_ctrl->sensordata->sensor_name)) || (!strcmp("wayne_imx376_ofilm_back_ii", s_ctrl->sensordata->sensor_name)) || (!strcmp("wayne_imx376_sunny_front_i", s_ctrl->sensordata->sensor_name)) || (!strcmp("wayne_imx376_ofilm_front_ii", s_ctrl->sensordata->sensor_name))){
		rc = msm_sensor_get_sensor_id_sony_376(s_ctrl, sensor_fusion_id_tmp);
		if (rc < 0){
		pr_err("%s:%d lct read sensor %s fusion id failed\n", __func__, __LINE__, s_ctrl->sensordata->sensor_name);
			}
		}
	if ((!strcmp("whyred_s5k5e8_ofilm_i", s_ctrl->sensordata->sensor_name)) || (!strcmp("whyred_s5k5e8_qtech_ii", s_ctrl->sensordata->sensor_name))){
		rc = msm_sensor_get_sensor_id_samsung_5e8(s_ctrl, sensor_fusion_id_tmp);
		if (rc < 0){
		pr_err("%s:%d lct read sensor %s fusion id failed\n", __func__, __LINE__, s_ctrl->sensordata->sensor_name);
			}
		}
	if ((!strcmp("whyred_s5k2l7_ofilm_cn_i", s_ctrl->sensordata->sensor_name)) || (!strcmp("whyred_s5k2l7_qtech_cn_ii", s_ctrl->sensordata->sensor_name))){
		rc = msm_sensor_get_sensor_id_samsung_2L7(s_ctrl, sensor_fusion_id_tmp);
		if (rc < 0){
		pr_err("%s:%d lct read sensor %s fusion id failed\n", __func__, __LINE__, s_ctrl->sensordata->sensor_name);
			}
		}
	if (!strcmp("whyred_ov13855_sunny_cn_i", s_ctrl->sensordata->sensor_name)){
			rc = msm_sensor_get_sensor_id_ovti_13855(s_ctrl, sensor_fusion_id_tmp);
			if (rc < 0){
			pr_err("%s:%d lct read sensor %s fusion id failed\n", __func__, __LINE__, s_ctrl->sensordata->sensor_name);
				}
			}

	CDBG("%s:%d lct read sensor fusion id %s\n", __func__, __LINE__, sensor_fusion_id_tmp);
	strcat(sensor_fusion_id, sensor_fusion_id_tmp);
	strcat(sensor_fusion_id, "\n");
}

static ssize_t msm_sensor_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t rc = 0;

	sprintf(buf, "%s", sensor_fusion_id);
	rc = strlen(buf) + 1;

	return rc;
}

static DEVICE_ATTR(sensorid, 0444, msm_sensor_id_show, NULL);

int32_t msm_sensorid_init_device_name(void)
{
	int32_t rc = 0;
	CDBG("%s %d\n", __func__, __LINE__);
	if (msm_sensorid_device != NULL){
		pr_err("Macle android_camera already created\n");
		return 0;
	}
	msm_sensorid_device = kobject_create_and_add("camera_sensorid", NULL);
	if (msm_sensorid_device == NULL) {
		printk("%s: subsystem_register failed\n", __func__);
		rc = -ENOMEM;
		return rc ;
	}
	rc = sysfs_create_file(msm_sensorid_device, &dev_attr_sensorid.attr);
	if (rc) {
		printk("%s: sysfs_create_file failed\n", __func__);
		kobject_del(msm_sensorid_device);
	}

	return 0 ;
}
/* add sensor info for factory mode
   end
*/

/* static function definition */
int32_t msm_sensor_driver_probe(void *setting,
	struct msm_sensor_info_t *probed_info, char *entity_name)
{
	int32_t                              rc = 0;
	struct msm_sensor_ctrl_t            *s_ctrl = NULL;
	struct msm_camera_cci_client        *cci_client = NULL;
	struct msm_camera_sensor_slave_info *slave_info = NULL;
	struct msm_camera_slave_info        *camera_info = NULL;

	unsigned long                        mount_pos = 0;
	uint32_t                             is_yuv;

	/* Validate input parameters */
	if (!setting) {
		pr_err("failed: slave_info %pK", setting);
		return -EINVAL;
	}

	/* Allocate memory for slave info */
	slave_info = kzalloc(sizeof(*slave_info), GFP_KERNEL);
	if (!slave_info)
		return -ENOMEM;
#ifdef CONFIG_COMPAT
	if (is_compat_task()) {
		struct msm_camera_sensor_slave_info32 *slave_info32 =
			kzalloc(sizeof(*slave_info32), GFP_KERNEL);
		if (!slave_info32) {
			pr_err("failed: no memory for slave_info32 %pK\n",
				slave_info32);
			rc = -ENOMEM;
			goto free_slave_info;
		}
		if (copy_from_user((void *)slave_info32, setting,
			sizeof(*slave_info32))) {
				pr_err("failed: copy_from_user");
				rc = -EFAULT;
				kfree(slave_info32);
				goto free_slave_info;
			}

		strlcpy(slave_info->actuator_name, slave_info32->actuator_name,
			sizeof(slave_info->actuator_name));

		strlcpy(slave_info->eeprom_name, slave_info32->eeprom_name,
			sizeof(slave_info->eeprom_name));

		strlcpy(slave_info->sensor_name, slave_info32->sensor_name,
			sizeof(slave_info->sensor_name));

		strlcpy(slave_info->ois_name, slave_info32->ois_name,
			sizeof(slave_info->ois_name));

		strlcpy(slave_info->flash_name, slave_info32->flash_name,
			sizeof(slave_info->flash_name));

		slave_info->addr_type = slave_info32->addr_type;
		slave_info->camera_id = slave_info32->camera_id;

		slave_info->i2c_freq_mode = slave_info32->i2c_freq_mode;
		slave_info->sensor_id_info = slave_info32->sensor_id_info;
		slave_info->vendor_id_info = slave_info32->vendor_id_info;
		slave_info->vcm_id_info = slave_info32->vcm_id_info;

		slave_info->slave_addr = slave_info32->slave_addr;
		slave_info->power_setting_array.size =
			slave_info32->power_setting_array.size;
		slave_info->power_setting_array.size_down =
			slave_info32->power_setting_array.size_down;
		slave_info->power_setting_array.size_down =
			slave_info32->power_setting_array.size_down;
		slave_info->power_setting_array.power_setting =
			compat_ptr(slave_info32->
				power_setting_array.power_setting);
		slave_info->power_setting_array.power_down_setting =
			compat_ptr(slave_info32->
				power_setting_array.power_down_setting);
		slave_info->sensor_init_params =
			slave_info32->sensor_init_params;
		slave_info->output_format =
			slave_info32->output_format;
		slave_info->bypass_video_node_creation =
			!!slave_info32->bypass_video_node_creation;
		kfree(slave_info32);
	} else
#endif
	{
		if (copy_from_user(slave_info,
					(void *)setting, sizeof(*slave_info))) {
			pr_err("failed: copy_from_user");
			rc = -EFAULT;
			goto free_slave_info;
		}
	}

	if (strlen(slave_info->sensor_name) >= MAX_SENSOR_NAME ||
		strlen(slave_info->eeprom_name) >= MAX_SENSOR_NAME ||
		strlen(slave_info->actuator_name) >= MAX_SENSOR_NAME ||
		strlen(slave_info->ois_name) >= MAX_SENSOR_NAME) {
		pr_err("failed: name len greater than 32.\n");
		pr_err("sensor name len:%zu, eeprom name len: %zu.\n",
			strlen(slave_info->sensor_name),
			strlen(slave_info->eeprom_name));
		pr_err("actuator name len: %zu, ois name len:%zu.\n",
			strlen(slave_info->actuator_name),
			strlen(slave_info->ois_name));
		rc = -EINVAL;
		goto free_slave_info;
	}


	/* Print slave info */
	CDBG("camera id %d Slave addr 0x%X addr_type %d\n",
		slave_info->camera_id, slave_info->slave_addr,
		slave_info->addr_type);
	CDBG("sensor_id_reg_addr 0x%X sensor_id 0x%X sensor id mask %d",
		slave_info->sensor_id_info.sensor_id_reg_addr,
		slave_info->sensor_id_info.sensor_id,
		slave_info->sensor_id_info.sensor_id_mask);
	CDBG("power up size %d power down size %d\n",
		slave_info->power_setting_array.size,
		slave_info->power_setting_array.size_down);
	CDBG("position %d",
		slave_info->sensor_init_params.position);
	CDBG("mount %d",
		slave_info->sensor_init_params.sensor_mount_angle);
	CDBG("bypass video node creation %d",
		slave_info->bypass_video_node_creation);
	/* Validate camera id */
	if (slave_info->camera_id >= MAX_CAMERAS) {
		pr_err("failed: invalid camera id %d max %d",
			slave_info->camera_id, MAX_CAMERAS);
		rc = -EINVAL;
		goto free_slave_info;
	}

	/* Extract s_ctrl from camera id */
	s_ctrl = g_sctrl[slave_info->camera_id];
	if (!s_ctrl) {
		pr_err("failed: s_ctrl %pK for camera_id %d", s_ctrl,
			slave_info->camera_id);
		rc = -EINVAL;
		goto free_slave_info;
	}

	CDBG("s_ctrl[%d] %pK", slave_info->camera_id, s_ctrl);

	if (s_ctrl->sensordata->special_support_size > 0) {
		if (!msm_sensor_driver_is_special_support(s_ctrl,
			slave_info->sensor_name)) {
			pr_err("%s:%s is not support on this board\n",
				__func__, slave_info->sensor_name);
			rc = 0;
			goto free_slave_info;
		}
	}

	if (s_ctrl->is_probe_succeed == 1) {
		/*
		 * Different sensor on this camera slot has been connected
		 * and probe already succeeded for that sensor. Ignore this
		 * probe
		 */
		if ((slave_info->sensor_id_info.sensor_id ==
			s_ctrl->sensordata->cam_slave_info->sensor_id_info.sensor_id) &&
			(!(strcmp(slave_info->sensor_name,
			s_ctrl->sensordata->cam_slave_info->sensor_name))) && (slave_info->vendor_id_info.vendor_id ==
			s_ctrl->sensordata->cam_slave_info->vendor_id_info.vendor_id)) {
			pr_err("slot%d: sensor name: %s sensor id%d already probed\n",
				slave_info->camera_id,
				slave_info->sensor_name,
				s_ctrl->sensordata->cam_slave_info->
					sensor_id_info.sensor_id);
			msm_sensor_fill_sensor_info(s_ctrl,
				probed_info, entity_name);
		} else
			pr_err("slot %d has some other sensor\n",
				slave_info->camera_id);

		rc = 0;
		goto free_slave_info;
	}

	if (slave_info->power_setting_array.size == 0 &&
		slave_info->slave_addr == 0) {
		s_ctrl->is_csid_tg_mode = 1;
		goto CSID_TG;
	}

	rc = msm_sensor_get_power_settings(setting, slave_info,
		&s_ctrl->sensordata->power_info);
	if (rc < 0) {
		pr_err("failed");
		goto free_slave_info;
	}


	camera_info = kzalloc(sizeof(struct msm_camera_slave_info), GFP_KERNEL);
	if (!camera_info)
		goto free_slave_info;

	s_ctrl->sensordata->slave_info = camera_info;

	/* Fill sensor slave info */
	camera_info->sensor_slave_addr = slave_info->slave_addr;
	camera_info->sensor_id_reg_addr =
		slave_info->sensor_id_info.sensor_id_reg_addr;
	camera_info->sensor_id = slave_info->sensor_id_info.sensor_id;
	camera_info->sensor_id_mask = slave_info->sensor_id_info.sensor_id_mask;

	/* Fill CCI master, slave address and CCI default params */
	if (!s_ctrl->sensor_i2c_client) {
		pr_err("failed: sensor_i2c_client %pK",
			s_ctrl->sensor_i2c_client);
		rc = -EINVAL;
		goto free_camera_info;
	}
	/* Fill sensor address type */
	s_ctrl->sensor_i2c_client->addr_type = slave_info->addr_type;
	if (s_ctrl->sensor_i2c_client->client)
		s_ctrl->sensor_i2c_client->client->addr =
			camera_info->sensor_slave_addr;

	cci_client = s_ctrl->sensor_i2c_client->cci_client;
	if (!cci_client) {
		pr_err("failed: cci_client %pK", cci_client);
		goto free_camera_info;
	}
	cci_client->cci_i2c_master = s_ctrl->cci_i2c_master;
	cci_client->sid = slave_info->slave_addr >> 1;
	cci_client->retries = 3;
	cci_client->id_map = 0;
	cci_client->i2c_freq_mode = slave_info->i2c_freq_mode;

	/* Parse and fill vreg params for powerup settings */
	rc = msm_camera_fill_vreg_params(
		s_ctrl->sensordata->power_info.cam_vreg,
		s_ctrl->sensordata->power_info.num_vreg,
		s_ctrl->sensordata->power_info.power_setting,
		s_ctrl->sensordata->power_info.power_setting_size);
	if (rc < 0) {
		pr_err("failed: msm_camera_get_dt_power_setting_data rc %d",
			rc);
		goto free_camera_info;
	}

	/* Parse and fill vreg params for powerdown settings*/
	rc = msm_camera_fill_vreg_params(
		s_ctrl->sensordata->power_info.cam_vreg,
		s_ctrl->sensordata->power_info.num_vreg,
		s_ctrl->sensordata->power_info.power_down_setting,
		s_ctrl->sensordata->power_info.power_down_setting_size);
	if (rc < 0) {
		pr_err("failed: msm_camera_fill_vreg_params for PDOWN rc %d",
			rc);
		goto free_camera_info;
	}

CSID_TG:
	/* Update sensor, actuator and eeprom name in
	*  sensor control structure */
	s_ctrl->sensordata->sensor_name = slave_info->sensor_name;
	s_ctrl->sensordata->eeprom_name = slave_info->eeprom_name;
	s_ctrl->sensordata->actuator_name = slave_info->actuator_name;
	s_ctrl->sensordata->ois_name = slave_info->ois_name;
	s_ctrl->sensordata->flash_name = slave_info->flash_name;
	s_ctrl->sensordata->vendor_id_info = &(slave_info->vendor_id_info);
	s_ctrl->sensordata->vcm_id_info = &(slave_info->vcm_id_info);


	/*
	 * Update eeporm subdevice Id by input eeprom name
	 */
	rc = msm_sensor_fill_eeprom_subdevid_by_name(s_ctrl);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto free_camera_info;
	}
	/*
	 * Update actuator subdevice Id by input actuator name
	 */
	rc = msm_sensor_fill_actuator_subdevid_by_name(s_ctrl);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto free_camera_info;
	}
	rc = msm_sensor_fill_laser_led_subdevid_by_name(s_ctrl);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto free_camera_info;
	}

	rc = msm_sensor_fill_ois_subdevid_by_name(s_ctrl);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto free_camera_info;
	}

	rc = msm_sensor_fill_flash_subdevid_by_name(s_ctrl);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto free_camera_info;
	}

	/* Power up and probe sensor */
	rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
	if (rc < 0) {
		pr_err("%s power up failed", slave_info->sensor_name);
		goto free_camera_info;
	}

	pr_err("%s probe succeeded", slave_info->sensor_name);

	s_ctrl->bypass_video_node_creation =
		slave_info->bypass_video_node_creation;

	/*
	 * Create /dev/videoX node, comment for now until dummy /dev/videoX
	 * node is created and used by HAL
	 */

	if (s_ctrl->sensor_device_type == MSM_CAMERA_PLATFORM_DEVICE)
		rc = msm_sensor_driver_create_v4l_subdev(s_ctrl);
	else
		rc = msm_sensor_driver_create_i2c_v4l_subdev(s_ctrl);
	if (rc < 0) {
		pr_err("failed: camera creat v4l2 rc %d", rc);
		goto camera_power_down;
	}

	rc = msm_sensor_fill_slave_info_init_params(
		slave_info,
		s_ctrl->sensordata->sensor_info);
	if (rc < 0) {
		pr_err("%s Fill slave info failed", slave_info->sensor_name);
		goto camera_power_down;
	}
	rc = msm_sensor_validate_slave_info(s_ctrl->sensordata->sensor_info);
	if (rc < 0) {
		pr_err("%s Validate slave info failed",
			slave_info->sensor_name);
		goto camera_power_down;
	}
	/* Update sensor mount angle and position in media entity flag */
	is_yuv = (slave_info->output_format == MSM_SENSOR_YCBCR) ? 1 : 0;
	mount_pos = ((s_ctrl->is_secure & 0x1) << 26) | is_yuv << 25 |
		(s_ctrl->sensordata->sensor_info->position << 16) |
		((s_ctrl->sensordata->
		sensor_info->sensor_mount_angle / 90) << 8);

	s_ctrl->msm_sd.sd.entity.flags = mount_pos | MEDIA_ENT_FL_DEFAULT;

	/*Save sensor info*/
	s_ctrl->sensordata->cam_slave_info = slave_info;

	msm_sensor_fill_sensor_info(s_ctrl, probed_info, entity_name);

	msm_sensor_init_device_name();
	msm_sensor_set_module_info(s_ctrl);
	msm_sensorid_init_device_name();
	msm_sensor_set_sesnor_id(s_ctrl);

	/* Power down */
	s_ctrl->func_tbl->sensor_power_down(s_ctrl);


	/*
	 * Set probe succeeded flag to 1 so that no other camera shall
	 * probed on this slot
	 */
	s_ctrl->is_probe_succeed = 1;
	return rc;

camera_power_down:
	s_ctrl->func_tbl->sensor_power_down(s_ctrl);
free_camera_info:
	kfree(camera_info);
free_slave_info:
	kfree(slave_info);
	return rc;
}

static int32_t msm_sensor_driver_get_dt_data(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t                              rc = 0, i = 0;
	struct msm_camera_sensor_board_info *sensordata = NULL;
	struct device_node                  *of_node = s_ctrl->of_node;
	uint32_t	cell_id;

	s_ctrl->sensordata = kzalloc(sizeof(*sensordata), GFP_KERNEL);
	if (!s_ctrl->sensordata) {
		pr_err("failed: no memory");
		return -ENOMEM;
	}

	sensordata = s_ctrl->sensordata;

	/*
	 * Read cell index - this cell index will be the camera slot where
	 * this camera will be mounted
	 */
	rc = of_property_read_u32(of_node, "cell-index", &cell_id);
	if (rc < 0) {
		pr_err("failed: cell-index rc %d", rc);
		goto FREE_SENSOR_DATA;
	}
	s_ctrl->id = cell_id;

	/* Validate cell_id */
	if (cell_id >= MAX_CAMERAS) {
		pr_err("failed: invalid cell_id %d", cell_id);
		rc = -EINVAL;
		goto FREE_SENSOR_DATA;
	}

	/* Check whether g_sctrl is already filled for this cell_id */
	if (g_sctrl[cell_id]) {
		pr_err("failed: sctrl already filled for cell_id %d", cell_id);
		rc = -EINVAL;
		goto FREE_SENSOR_DATA;
	}

	sensordata->special_support_size =
		of_property_count_strings(of_node,
				 "qcom,special-support-sensors");

	if (sensordata->special_support_size < 0)
		sensordata->special_support_size = 0;

	if (sensordata->special_support_size > MAX_SPECIAL_SUPPORT_SIZE) {
		pr_debug("%s:support_size exceed max support size\n", __func__);
		sensordata->special_support_size = MAX_SPECIAL_SUPPORT_SIZE;
	}

	if (sensordata->special_support_size) {
		for (i = 0; i < sensordata->special_support_size; i++) {
			rc = of_property_read_string_index(of_node,
				"qcom,special-support-sensors", i,
				&(sensordata->special_support_sensors[i]));
			if (rc < 0) {
				/* if read sensor support names failed,
				*   set support all sensors, break;
				*/
				sensordata->special_support_size = 0;
				break;
			}
			CDBG("%s special_support_sensors[%d] = %s\n", __func__,
				i, sensordata->special_support_sensors[i]);
		}
	}

	/* Read subdev info */
	rc = msm_sensor_get_sub_module_index(of_node, &sensordata->sensor_info);
	if (rc < 0) {
		pr_err("failed");
		goto FREE_SENSOR_DATA;
	}

	/* Read vreg information */
	rc = msm_camera_get_dt_vreg_data(of_node,
		&sensordata->power_info.cam_vreg,
		&sensordata->power_info.num_vreg);
	if (rc < 0) {
		pr_err("failed: msm_camera_get_dt_vreg_data rc %d", rc);
		goto FREE_SUB_MODULE_DATA;
	}

	/* Read gpio information */
	rc = msm_sensor_driver_get_gpio_data
		(&(sensordata->power_info.gpio_conf), of_node);
	if (rc < 0) {
		pr_err("failed: msm_sensor_driver_get_gpio_data rc %d", rc);
		goto FREE_VREG_DATA;
	}

	/* Get custom mode */
	rc = of_property_read_u32(of_node, "qcom,secure",
		&s_ctrl->is_secure);
	CDBG("qcom,secure = %d, rc %d", s_ctrl->is_secure, rc);
	if (rc < 0) {
		/* Set default to non-secure mode */
		s_ctrl->is_secure = 0;
		rc = 0;
	}

	/* Get CCI master */
	rc = of_property_read_u32(of_node, "qcom,cci-master",
		&s_ctrl->cci_i2c_master);
	CDBG("qcom,cci-master %d, rc %d", s_ctrl->cci_i2c_master, rc);
	if (rc < 0) {
		/* Set default master 0 */
		s_ctrl->cci_i2c_master = MASTER_0;
		rc = 0;
	}

	/* Get mount angle */
	if (0 > of_property_read_u32(of_node, "qcom,mount-angle",
		&sensordata->sensor_info->sensor_mount_angle)) {
		/* Invalidate mount angle flag */
		sensordata->sensor_info->is_mount_angle_valid = 0;
		sensordata->sensor_info->sensor_mount_angle = 0;
	} else {
		sensordata->sensor_info->is_mount_angle_valid = 1;
	}
	CDBG("%s qcom,mount-angle %d\n", __func__,
		sensordata->sensor_info->sensor_mount_angle);
	if (0 > of_property_read_u32(of_node, "qcom,sensor-position",
		&sensordata->sensor_info->position)) {
		CDBG("%s:%d Invalid sensor position\n", __func__, __LINE__);
		sensordata->sensor_info->position = INVALID_CAMERA_B;
	}
	if (0 > of_property_read_u32(of_node, "qcom,sensor-mode",
		&sensordata->sensor_info->modes_supported)) {
		CDBG("%s:%d Invalid sensor mode supported\n",
			__func__, __LINE__);
		sensordata->sensor_info->modes_supported = CAMERA_MODE_INVALID;
	}
	/* Get vdd-cx regulator */
	/*Optional property, don't return error if absent */
	of_property_read_string(of_node, "qcom,vdd-cx-name",
		&sensordata->misc_regulator);
	CDBG("qcom,misc_regulator %s", sensordata->misc_regulator);

	s_ctrl->set_mclk_23880000 = of_property_read_bool(of_node,
						"qcom,mclk-23880000");

	CDBG("%s qcom,mclk-23880000 = %d\n", __func__,
		s_ctrl->set_mclk_23880000);

	return rc;

FREE_VREG_DATA:
	kfree(sensordata->power_info.cam_vreg);
FREE_SUB_MODULE_DATA:
	kfree(sensordata->sensor_info);
FREE_SENSOR_DATA:
	kfree(sensordata);
	return rc;
}

static int32_t msm_sensor_driver_parse(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t                   rc = 0;

	CDBG("Enter");
	/* Validate input parameters */


	/* Allocate memory for sensor_i2c_client */
	s_ctrl->sensor_i2c_client = kzalloc(sizeof(*s_ctrl->sensor_i2c_client),
		GFP_KERNEL);
	if (!s_ctrl->sensor_i2c_client) {
		pr_err("failed: no memory sensor_i2c_client %pK",
			s_ctrl->sensor_i2c_client);
		return -ENOMEM;
	}

	/* Allocate memory for mutex */
	s_ctrl->msm_sensor_mutex = kzalloc(sizeof(*s_ctrl->msm_sensor_mutex),
		GFP_KERNEL);
	if (!s_ctrl->msm_sensor_mutex) {
		pr_err("failed: no memory msm_sensor_mutex %pK",
			s_ctrl->msm_sensor_mutex);
		rc = -ENOMEM;
		goto FREE_SENSOR_I2C_CLIENT;
	}

	/* Parse dt information and store in sensor control structure */
	rc = msm_sensor_driver_get_dt_data(s_ctrl);
	if (rc < 0) {
		pr_err("failed: rc %d", rc);
		goto FREE_MUTEX;
	}

	/* Initialize mutex */
	mutex_init(s_ctrl->msm_sensor_mutex);

	/* Initilize v4l2 subdev info */
	s_ctrl->sensor_v4l2_subdev_info = msm_sensor_driver_subdev_info;
	s_ctrl->sensor_v4l2_subdev_info_size =
		ARRAY_SIZE(msm_sensor_driver_subdev_info);

	/* Initialize default parameters */
	rc = msm_sensor_init_default_params(s_ctrl);
	if (rc < 0) {
		pr_err("failed: msm_sensor_init_default_params rc %d", rc);
		goto FREE_DT_DATA;
	}

	/* Store sensor control structure in static database */
	g_sctrl[s_ctrl->id] = s_ctrl;
	CDBG("g_sctrl[%d] %pK", s_ctrl->id, g_sctrl[s_ctrl->id]);

	return rc;

FREE_DT_DATA:
	kfree(s_ctrl->sensordata->power_info.gpio_conf->gpio_num_info);
	kfree(s_ctrl->sensordata->power_info.gpio_conf->cam_gpio_req_tbl);
	kfree(s_ctrl->sensordata->power_info.gpio_conf);
	kfree(s_ctrl->sensordata->power_info.cam_vreg);
	kfree(s_ctrl->sensordata);
FREE_MUTEX:
	kfree(s_ctrl->msm_sensor_mutex);
FREE_SENSOR_I2C_CLIENT:
	kfree(s_ctrl->sensor_i2c_client);
	return rc;
}

static int32_t msm_sensor_driver_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl = NULL;

	/* Create sensor control structure */
	s_ctrl = kzalloc(sizeof(*s_ctrl), GFP_KERNEL);
	if (!s_ctrl)
		return -ENOMEM;

	platform_set_drvdata(pdev, s_ctrl);

	/* Initialize sensor device type */
	s_ctrl->sensor_device_type = MSM_CAMERA_PLATFORM_DEVICE;
	s_ctrl->of_node = pdev->dev.of_node;

	/*fill in platform device*/
	s_ctrl->pdev = pdev;

	rc = msm_sensor_driver_parse(s_ctrl);
	if (rc < 0) {
		pr_err("failed: msm_sensor_driver_parse rc %d", rc);
		goto FREE_S_CTRL;
	}

	/* Get clocks information */
	rc = msm_camera_get_clk_info(s_ctrl->pdev,
		&s_ctrl->sensordata->power_info.clk_info,
		&s_ctrl->sensordata->power_info.clk_ptr,
		&s_ctrl->sensordata->power_info.clk_info_size);
	if (rc < 0) {
		pr_err("failed: msm_camera_get_clk_info rc %d", rc);
		goto FREE_S_CTRL;
	}

	/* Fill platform device id*/
	pdev->id = s_ctrl->id;

	/* Fill device in power info */
	s_ctrl->sensordata->power_info.dev = &pdev->dev;
	return rc;
FREE_S_CTRL:
	kfree(s_ctrl);
	return rc;
}

static int32_t msm_sensor_driver_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int32_t rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl;

	CDBG("\n\nEnter: msm_sensor_driver_i2c_probe");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s %s i2c_check_functionality failed\n",
			__func__, client->name);
		rc = -EFAULT;
		return rc;
	}

	/* Create sensor control structure */
	s_ctrl = kzalloc(sizeof(*s_ctrl), GFP_KERNEL);
	if (!s_ctrl)
		return -ENOMEM;

	i2c_set_clientdata(client, s_ctrl);

	/* Initialize sensor device type */
	s_ctrl->sensor_device_type = MSM_CAMERA_I2C_DEVICE;
	s_ctrl->of_node = client->dev.of_node;

	rc = msm_sensor_driver_parse(s_ctrl);
	if (rc < 0) {
		pr_err("failed: msm_sensor_driver_parse rc %d", rc);
		goto FREE_S_CTRL;
	}

	if (s_ctrl->sensor_i2c_client != NULL) {
		s_ctrl->sensor_i2c_client->client = client;
		s_ctrl->sensordata->power_info.dev = &client->dev;

		/* Get clocks information */
		rc = msm_camera_i2c_dev_get_clk_info(
			&s_ctrl->sensor_i2c_client->client->dev,
			&s_ctrl->sensordata->power_info.clk_info,
			&s_ctrl->sensordata->power_info.clk_ptr,
			&s_ctrl->sensordata->power_info.clk_info_size);
		if (rc < 0) {
			pr_err("failed: msm_camera_i2c_dev_get_clk_info rc %d",
				rc);
			goto FREE_S_CTRL;
		}
	}
	return rc;
FREE_S_CTRL:
	kfree(s_ctrl);
	return rc;
}

static int msm_sensor_driver_i2c_remove(struct i2c_client *client)
{
	struct msm_sensor_ctrl_t  *s_ctrl = i2c_get_clientdata(client);

	pr_err("%s: sensor FREE\n", __func__);

	if (!s_ctrl) {
		pr_err("%s: sensor device is NULL\n", __func__);
		return 0;
	}

	g_sctrl[s_ctrl->id] = NULL;
	msm_sensor_free_sensor_data(s_ctrl);
	kfree(s_ctrl->msm_sensor_mutex);
	kfree(s_ctrl->sensor_i2c_client);
	kfree(s_ctrl);

	return 0;
}

static const struct i2c_device_id i2c_id[] = {
	{SENSOR_DRIVER_I2C, (kernel_ulong_t)NULL},
	{ }
};

static struct i2c_driver msm_sensor_driver_i2c = {
	.id_table = i2c_id,
	.probe  = msm_sensor_driver_i2c_probe,
	.remove = msm_sensor_driver_i2c_remove,
	.driver = {
		.name = SENSOR_DRIVER_I2C,
	},
};

static int __init msm_sensor_driver_init(void)
{
	int32_t rc = 0;

	CDBG("%s Enter\n", __func__);
	rc = platform_driver_register(&msm_sensor_platform_driver);
	if (rc)
		pr_err("%s platform_driver_register failed rc = %d",
			__func__, rc);
	rc = i2c_add_driver(&msm_sensor_driver_i2c);
	if (rc)
		pr_err("%s i2c_add_driver failed rc = %d",  __func__, rc);

	return rc;
}

static void __exit msm_sensor_driver_exit(void)
{
	CDBG("Enter");
	platform_driver_unregister(&msm_sensor_platform_driver);
	i2c_del_driver(&msm_sensor_driver_i2c);
	return;
}

module_init(msm_sensor_driver_init);
module_exit(msm_sensor_driver_exit);
MODULE_DESCRIPTION("msm_sensor_driver");
MODULE_LICENSE("GPL v2");
