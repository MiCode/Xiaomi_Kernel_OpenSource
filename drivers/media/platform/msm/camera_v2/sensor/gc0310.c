/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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
#include "msm_sensor.h"
#include "msm_cci.h"
#include "msm_camera_io_util.h"
#define GC0310_SENSOR_NAME "gc0310"
#define PLATFORM_DRIVER_NAME "msm_camera_gc0310"
#define gc0310_obj gc0310_##obj

/*#define CONFIG_MSMB_CAMERA_DEBUG*/
#undef CDBG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif


DEFINE_MSM_MUTEX(gc0310_mut);
static struct msm_sensor_ctrl_t gc0310_s_ctrl;

static struct msm_sensor_power_setting gc0310_power_setting[] = {
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_LOW,
		.delay = 5,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_HIGH,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VDIG,
		.config_val = 0,
		.delay = 5,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VANA,
		.config_val = 0,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 24000000,
		.delay = 10,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_LOW,
		.delay = 5,
	},
	{
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 0,
	},
};

static struct msm_camera_i2c_reg_array gc0310_recommend_setting_list[] = {
	{0xfe, 0xf0},
	{0xfe, 0xf0},
	{0xfe, 0x00},
	{0xfc, 0x0e},
	{0xfc, 0x0e},
	{0xf2, 0x80},
	{0xf3, 0x00},
	{0xf7, 0x1f},
	{0xf8, 0x03},
	{0xf9, 0x8e},
	{0xfa, 0x11},

	/*	CISCTL reg	*/
	{0x00, 0x2f},
	{0x01, 0x0f},
	{0x02, 0x04},
	{0x03, 0x02},
	{0x04, 0x58},
	{0x05, 0x00},
	{0x06, 0xde},
	{0x07, 0x00},
	{0x08, 0x24},
	{0x09, 0x00},
	{0x0a, 0x00},
	{0x0b, 0x00},
	{0x0c, 0x06},
	{0x0d, 0x01},
	{0x0e, 0xe8},
	{0x0f, 0x02},
	{0x10, 0x88},
	{0x16, 0x00},
	{0x17, 0x14},
	{0x18, 0x1a},
	{0x19, 0x14},
	{0x1b, 0x48},
	{0x1e, 0x6b},
	{0x1f, 0x28},
	{0x20, 0x89},
	{0x21, 0x49},
	{0x22, 0xb0},
	{0x23, 0x04},
	{0x24, 0x16},
	{0x34, 0x20},

	/*	 BLK	 */
	{0x26, 0x23},
	{0x28, 0xff},
	{0x29, 0x00},
	{0x33, 0x18},
	{0x37, 0x20},
	{0x47, 0x80},
	{0x4e, 0x66},
	{0xa8, 0x02},
	{0xa9, 0x80},

	/*	ISP	reg		*/
	{0x40, 0xff},
	{0x41, 0x21},
	{0x42, 0xcf},
	{0x44, 0x02},
	{0x46, 0x06},
	{0x4a, 0x11},
	{0x4b, 0x01},
	{0x4c, 0x20},
	{0x4d, 0x05},
	{0x4f, 0x01},
	{0x50, 0x01},
	{0x55, 0x01},
	{0x56, 0xe0},
	{0x57, 0x02},
	{0x58, 0x80},

	/*		GAIN	 */
	{0x70, 0x50},
	{0x5a, 0x98},
	{0x5b, 0xdc},
	{0x5c, 0xfe},
	{0x77, 0x74},
	{0x78, 0x40},
	{0x79, 0x5f},

	/*		DNDD	*/
	{0x82, 0x0a},
	{0x83, 0x0b},

	/*	 EEINTP	 */
	{0x8f, 0xff},
	{0x90, 0x8c},
	{0x91, 0x90},
	{0x92, 0x18},
	{0x93, 0x10},
	{0x95, 0x43},
	{0x96, 0x82},

	/*	 ASDE	 */
	{0xfe, 0x00},
	{0x9a, 0x20},
	{0x9b, 0x80},
	{0x9c, 0x40},
	{0x9d, 0x80},
	{0xa1, 0x30},
	{0xa2, 0x32},
	{0xa4, 0x30},
	{0xa5, 0x28},
	{0xaa, 0x50},
	{0xac, 0x24},

	/*		GAMMA		*/
	{0xbf, 0x0d},
	{0xc0, 0x26},
	{0xc1, 0x3d},
	{0xc2, 0x52},
	{0xc3, 0x66},
	{0xc4, 0x75},
	{0xc5, 0x82},
	{0xc6, 0x99},
	{0xc7, 0xab},
	{0xc8, 0xb9},
	{0xc9, 0xc6},
	{0xca, 0xd1},
	{0xcb, 0xda},
	{0xcc, 0xe1},
	{0xcd, 0xec},
	{0xce, 0xf5},
	{0xcf, 0xfb},

	/*		YCP	 */
	{0xd0, 0x40},
	{0xd1, 0x30},
	{0xd2, 0x30},
	{0xd3, 0x48},
	{0xfe, 0x00},
	{0xd6, 0xf2},
	{0xd7, 0x1b},
	{0xdd, 0x71},
	{0x7c, 0x80},

	/*	 AEC	 */
	{0xfe, 0x01},
	{0x05, 0x30},
	{0x06, 0x75},
	{0x07, 0x40},
	{0x08, 0xb0},
	{0x0a, 0xc5},
	{0x0b, 0x11},
	{0x0c, 0x00},
	{0x12, 0x52},
	{0x13, 0x30},
	{0x18, 0x95},
	{0x19, 0x95},
	{0x1e, 0x41},
	{0x1f, 0x30},
	{0x20, 0x40},

	{0x25, 0x00},
	{0x26, 0x83},
	{0x27, 0x02},
	{0x28, 0x0c},
	{0x29, 0x03},
	{0x2a, 0x12},
	{0x2b, 0x04},
	{0x2c, 0x18},
	{0x2d, 0x0a},
	{0x2e, 0x3c},

	{0x3c, 0x20},
	{0x3e, 0x40},
	{0x3f, 0x5c},
	{0x40, 0x7b},
	{0x41, 0xbd},
	{0x42, 0xf6},
	{0x43, 0x63},

	{0x03, 0x60},

	{0x44, 0x03},

	/*	 AWB	 */
	{0x1c, 0x91},
	{0x21, 0x15},
	{0x50, 0x80},
	{0x56, 0x06},
	{0x59, 0x08},
	{0x5b, 0x02},
	{0x61, 0x8d},
	{0x62, 0xa7},
	{0x63, 0xd0},
	{0x65, 0x06},
	{0x66, 0x06},
	{0x67, 0x84},
	{0x69, 0x08},
	{0x6a, 0x50},
	{0x6b, 0x01},
	{0x6c, 0x00},
	{0x6d, 0x02},
	{0x6e, 0xf0},
	{0x6f, 0x90},

	{0x78, 0xaf},
	{0x79, 0x70},
	{0x7a, 0x58},
	{0x7b, 0x60},
	{0x7c, 0x0c},
	{0x86, 0x00},
	{0x87, 0x00},
	{0x8b, 0x00},
	{0x8c, 0x00},
	{0x8a, 0x06},
	{0x8f, 0x00},

	/*awb*/
	{0x90, 0x00},
	{0x91, 0x00},
	{0x92, 0xf0},
	{0x93, 0xd9},
	{0x95, 0x21},
	{0x96, 0xf0},
	{0x97, 0x3e},
	{0x98, 0x21},
	{0x9a, 0x3e},
	{0x9b, 0x21},
	{0x9c, 0x70},
	{0x9d, 0x3e},
	{0x9f, 0x00},
	{0xa0, 0x00},
	{0xa1, 0x00},
	{0xa2, 0x00},
	{0x86, 0x00},
	{0x87, 0x00},
	{0x88, 0x00},
	{0x89, 0x00},
	{0xa4, 0x00},
	{0xa5, 0x00},
	{0xa6, 0xbe},
	{0xa7, 0x9c},
	{0xa9, 0xbe},
	{0xaa, 0x9a},
	{0xab, 0xb3},
	{0xac, 0x9e},
	{0xae, 0xcb},
	{0xaf, 0xb3},
	{0xb0, 0xcf},
	{0xb1, 0xa1},
	{0xb3, 0x00},
	{0xb4, 0x00},
	{0xb5, 0x00},
	{0xb6, 0x00},
	{0x8b, 0x00},
	{0x8c, 0x00},
	{0x8d, 0x00},
	{0x8e, 0x00},
	{0x94, 0x56},
	{0x99, 0xa6},
	{0x9e, 0xaa},
	{0xa3, 0x00},
	{0x8a, 0x00},
	{0xa8, 0x50},
	{0xad, 0x55},
	{0xb2, 0x55},
	{0xb7, 0x00},
	{0x8f, 0x00},
	{0xb8, 0xdd},
	{0xb9, 0x80},

	/*CC*/
	{0xfe, 0x01},
	{0xd0, 0x38},
	{0xd1, 0xf0},
	{0xd2, 0xfe},
	{0xd3, 0x00},
	{0xd4, 0x57},
	{0xd5, 0xfa},
	{0xd6, 0x3f},
	{0xd7, 0xf8},
	{0xd8, 0x05},
	{0xd9, 0x08},
	{0xda, 0x38},
	{0xdb, 0xe9},

	/*	 LSC	 */
	{0xfe, 0x01},
	{0x76, 0x80},
	{0xc1, 0x3c},
	{0xc2, 0x50},
	{0xc3, 0x00},
	{0xc4, 0x4b},
	{0xc5, 0x38},
	{0xc6, 0x36},
	{0xc7, 0x24},
	{0xc8, 0x00},
	{0xc9, 0x00},
	{0xdc, 0x20},
	{0xdd, 0x10},
	{0xdf, 0x00},
	{0xde, 0x00},

	/*	 Histogram	*/
	{0x01, 0x10},
	{0x0b, 0x31},
	{0x0e, 0x6c},
	{0x0f, 0x0f},
	{0x10, 0x6e},
	{0x12, 0xa0},
	{0x15, 0x40},
	{0x16, 0x60},
	{0x17, 0x20},

	/* Measure Window */
	{0xcc, 0x10},
	{0xcd, 0x10},
	{0xce, 0xa0},
	{0xcf, 0xe0},

	/* dark sun  */
	{0x45, 0xf7},
	{0x46, 0xff},
	{0x47, 0x15},
	{0x48, 0x03},
	{0x4f, 0x60},

	/* MIPI  */
	{0xfe, 0x03},
	{0x01, 0x03},
	{0x02, 0x22},
	{0x03, 0x94},
	{0x04, 0x01},
	{0x05, 0x00},
	{0x06, 0x80},
	{0x10, 0x84},
	{0x11, 0x1e},
	{0x12, 0x00},
	{0x13, 0x05},
	{0x15, 0x10},
	{0x17, 0xf0},
	/* 1/24M= 42ns */
	{0x21, 0x02},
	{0x22, 0x02},
	{0x23, 0x04},
	{0x24, 0x10},
	{0x29, 0x02},
	{0x2a, 0x04},
	{0xfe, 0x00},

};

static struct msm_camera_i2c_reg_setting gc0310_recommend_setting[] = {
  {
    .reg_setting = gc0310_recommend_setting_list,
    .size = ARRAY_SIZE(gc0310_recommend_setting_list),
    .addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
    .data_type = MSM_CAMERA_I2C_BYTE_DATA,
    .delay = 0,
  },
};

static struct v4l2_subdev_info gc0310_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
};

static struct msm_camera_i2c_reg_array gc0310_start_settings_list[] = {
	{0xfe, 0x03,},
	{0x10, 0x94,},
	{0xfe, 0x00,},
};

static struct msm_camera_i2c_reg_setting gc0310_start_settings[] = {
  {
    .reg_setting = gc0310_start_settings_list,
    .size = ARRAY_SIZE(gc0310_start_settings_list),
    .addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
    .data_type = MSM_CAMERA_I2C_BYTE_DATA,
    .delay = 0,
  },
};

static struct msm_camera_i2c_reg_array gc0310_stop_settings_list[] = {
	{0xfe, 0x03,},
	{0x10, 0x84,},
	{0xfe, 0x00,},
};

static struct msm_camera_i2c_reg_setting gc0310_stop_settings[] = {
  {
    .reg_setting = gc0310_stop_settings_list,
    .size = ARRAY_SIZE(gc0310_stop_settings_list),
    .addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
    .data_type = MSM_CAMERA_I2C_BYTE_DATA,
    .delay = 0,
  },
};

static const struct i2c_device_id gc0310_i2c_id[] = {
	{GC0310_SENSOR_NAME, (kernel_ulong_t)&gc0310_s_ctrl},
	{ }
};

static int32_t msm_gc0310_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &gc0310_s_ctrl);
}

static struct i2c_driver gc0310_i2c_driver = {
	.id_table = gc0310_i2c_id,
	.probe  = msm_gc0310_i2c_probe,
	.driver = {
		.name = GC0310_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client gc0310_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
};

static const struct of_device_id gc0310_dt_match[] = {
	{.compatible = "shinetech,gc0310", .data = &gc0310_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, gc0310_dt_match);

static int32_t gc0310_platform_probe(struct platform_device *pdev)
{
	int32_t rc;
	const struct of_device_id *match;
	match = of_match_device(gc0310_dt_match, &pdev->dev);
	rc = msm_sensor_platform_probe(pdev, match->data);
	return rc;
}

static struct platform_driver gc0310_platform_driver = {
	.driver = {
		.name = "shinetech,gc0310",
		.owner = THIS_MODULE,
		.of_match_table = gc0310_dt_match,
	},
	.probe = gc0310_platform_probe,
};

static int __init gc0310_init_module(void)
{
	int32_t rc;
	pr_err("%s:%d\n", __func__, __LINE__);
	rc = i2c_add_driver(&gc0310_i2c_driver);
	if (!rc)
		return rc;
	pr_err("%s:%d rc \n", __func__, __LINE__);
	return platform_driver_register(&gc0310_platform_driver);
}

static void __exit gc0310_exit_module(void)
{
	pr_err("%s:%d\n", __func__, __LINE__);
	if (gc0310_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&gc0310_s_ctrl);
		platform_driver_unregister(&gc0310_platform_driver);
	} else
		i2c_del_driver(&gc0310_i2c_driver);
	return;
}

int32_t gc0310_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
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
		cdata->cfg.sensor_info.is_mount_angle_valid =
			s_ctrl->sensordata->sensor_info->is_mount_angle_valid;
		cdata->cfg.sensor_info.sensor_mount_angle =
			s_ctrl->sensordata->sensor_info->sensor_mount_angle;
		CDBG("%s:%d sensor name %s\n", __func__, __LINE__,
			cdata->cfg.sensor_info.sensor_name);
		CDBG("%s:%d session id %d\n", __func__, __LINE__,
			cdata->cfg.sensor_info.session_id);
		for (i = 0; i < SUB_MODULE_MAX; i++)
			CDBG("%s:%d subdev_id[%d] %d\n", __func__, __LINE__, i,
				cdata->cfg.sensor_info.subdev_id[i]);
		CDBG("%s:%d mount angle valid %d value %d\n", __func__,
			__LINE__, cdata->cfg.sensor_info.is_mount_angle_valid,
			cdata->cfg.sensor_info.sensor_mount_angle);

		break;
	case CFG_SET_INIT_SETTING:
		/* 1. Write Recommend settings */
		/* 2. Write change settings */
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_table(
			s_ctrl->sensor_i2c_client, gc0310_recommend_setting);
		break;

	case CFG_SET_RESOLUTION: {
	/*copy from user the desired resoltuion*/
		enum msm_sensor_resolution_t res = MSM_SENSOR_INVALID_RES;
		if (copy_from_user(&res, (void *)cdata->cfg.setting,
			sizeof(enum msm_sensor_resolution_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		pr_err("%s:%d  res =%d\n", __func__, __LINE__, res);

		if (res == MSM_SENSOR_RES_FULL) {
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_table(
				s_ctrl->sensor_i2c_client, gc0310_recommend_setting);
				pr_err("%s:%d res =%d\n gc0310_recommend_setting ",
				__func__, __LINE__, res);
		} else {
			pr_err("%s:%d failed resoultion set\n", __func__,
				__LINE__);
			rc = -EFAULT;
		}
	}
		break;
	case CFG_SET_STOP_STREAM:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_table(
			s_ctrl->sensor_i2c_client, gc0310_stop_settings);
		break;
	case CFG_SET_START_STREAM:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_table(
			s_ctrl->sensor_i2c_client, gc0310_start_settings);
		break;
	case CFG_GET_SENSOR_INIT_PARAMS:
		cdata->cfg.sensor_init_params.modes_supported =
			s_ctrl->sensordata->sensor_info->modes_supported;
		cdata->cfg.sensor_init_params.position =
			s_ctrl->sensordata->sensor_info->position;
		cdata->cfg.sensor_init_params.sensor_mount_angle =
			s_ctrl->sensordata->sensor_info->sensor_mount_angle;
		pr_err("%s:%d init params mode %d pos %d mount %d\n", __func__,
			__LINE__,
			cdata->cfg.sensor_init_params.modes_supported,
			cdata->cfg.sensor_init_params.position,
			cdata->cfg.sensor_init_params.sensor_mount_angle);
		break;
	case CFG_SET_SLAVE_INFO: {
		struct msm_camera_sensor_slave_info *sensor_slave_info;
		struct msm_camera_power_ctrl_t *p_ctrl;
		uint16_t size;
		int slave_index = 0;
		sensor_slave_info = kmalloc(sizeof(struct msm_camera_sensor_slave_info)
				      * 1, GFP_KERNEL);

		if (!sensor_slave_info) {
			pr_err("%s: failed to alloc mem\n", __func__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(sensor_slave_info,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_sensor_slave_info))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		/* Update sensor slave address */
		if (sensor_slave_info->slave_addr)
			s_ctrl->sensor_i2c_client->cci_client->sid =
				sensor_slave_info->slave_addr >> 1;

		/* Update sensor address type */
		s_ctrl->sensor_i2c_client->addr_type =
			sensor_slave_info->addr_type;

		/* Update power up / down sequence */
		p_ctrl = &s_ctrl->sensordata->power_info;
		size = sensor_slave_info->power_setting_array.size;
		if (p_ctrl->power_setting_size < size) {
			struct msm_sensor_power_setting *tmp;
			tmp = kmalloc(sizeof(struct msm_sensor_power_setting)
				      * size, GFP_KERNEL);
			if (!tmp) {
				pr_err("%s: failed to alloc mem\n", __func__);
				rc = -ENOMEM;
				break;
			}
			kfree(p_ctrl->power_setting);
			p_ctrl->power_setting = tmp;
		}
		p_ctrl->power_setting_size = size;

		rc = copy_from_user(p_ctrl->power_setting, (void *)
			sensor_slave_info->power_setting_array.power_setting,
			size * sizeof(struct msm_sensor_power_setting));
		if (rc) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		for (slave_index = 0; slave_index <
			p_ctrl->power_setting_size; slave_index++) {
			CDBG("%s i %d power setting %d %d %ld %d\n", __func__,
				slave_index,
				p_ctrl->power_setting[slave_index].seq_type,
				p_ctrl->power_setting[slave_index].seq_val,
				p_ctrl->power_setting[slave_index].config_val,
				p_ctrl->power_setting[slave_index].delay);
		}
		break;
	}
	case CFG_WRITE_I2C_ARRAY: {
		struct msm_camera_i2c_reg_setting conf_array;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;

		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		if (!conf_array.size ||
			conf_array.size > I2C_REG_DATA_MAX) {
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
	case CFG_WRITE_I2C_SEQ_ARRAY: {
		struct msm_camera_i2c_seq_reg_setting conf_array;
		struct msm_camera_i2c_seq_reg_array *reg_setting = NULL;

		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_seq_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		if (!conf_array.size ||
			conf_array.size > I2C_SEQ_REG_DATA_MAX) {
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
		if (s_ctrl->func_tbl->sensor_power_up)
			rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
		else
			rc = -EFAULT;
		break;

	case CFG_POWER_DOWN:
		if (s_ctrl->func_tbl->sensor_power_down)
			rc = s_ctrl->func_tbl->sensor_power_down(s_ctrl);
		else
			rc = -EFAULT;
		break;

	case CFG_SET_STOP_STREAM_SETTING: {
		struct msm_camera_i2c_reg_setting *stop_setting =
			&s_ctrl->stop_setting;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;
		if (copy_from_user(stop_setting, (void *)cdata->cfg.setting,
		    sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = stop_setting->reg_setting;
		stop_setting->reg_setting = kzalloc(stop_setting->size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!stop_setting->reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(stop_setting->reg_setting,
		    (void *)reg_setting, stop_setting->size *
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
	case CFG_SET_STREAM_TYPE: {
		enum msm_camera_stream_type_t stream_type = MSM_CAMERA_STREAM_INVALID;
		if (copy_from_user(&stream_type, (void *)cdata->cfg.setting,
			sizeof(enum msm_camera_stream_type_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		s_ctrl->camera_stream_type = stream_type;
		break;
	}
	case CFG_SET_SATURATION:
		break;
	case CFG_SET_CONTRAST:
		break;
	case CFG_SET_SHARPNESS:
		break;
	case CFG_SET_AUTOFOCUS:
		/* TO-DO: set the Auto Focus */
		pr_debug("%s: Setting Auto Focus", __func__);
		break;
	case CFG_CANCEL_AUTOFOCUS:
		/* TO-DO: Cancel the Auto Focus */
		pr_debug("%s: Cancelling Auto Focus", __func__);
		break;
	case CFG_SET_ISO:
		break;
	case CFG_SET_EXPOSURE_COMPENSATION:
		break;
	case CFG_SET_EFFECT:
		break;
	case CFG_SET_ANTIBANDING:
		break;
	case CFG_SET_BESTSHOT_MODE:
		break;
	case CFG_SET_WHITE_BALANCE:
		break;
	default:
		rc = -EFAULT;
		break;
	}

	mutex_unlock(s_ctrl->msm_sensor_mutex);

	return rc;
}

#ifdef CONFIG_COMPAT
int32_t gc0310_sensor_config32(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	struct sensorb_cfg_data32 *cdata = (struct sensorb_cfg_data32 *)argp;
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
		cdata->cfg.sensor_info.is_mount_angle_valid =
			s_ctrl->sensordata->sensor_info->is_mount_angle_valid;
		cdata->cfg.sensor_info.sensor_mount_angle =
			s_ctrl->sensordata->sensor_info->sensor_mount_angle;
		CDBG("%s:%d sensor name %s\n", __func__, __LINE__,
			cdata->cfg.sensor_info.sensor_name);
		CDBG("%s:%d session id %d\n", __func__, __LINE__,
			cdata->cfg.sensor_info.session_id);
		for (i = 0; i < SUB_MODULE_MAX; i++)
			CDBG("%s:%d subdev_id[%d] %d\n", __func__, __LINE__, i,
				cdata->cfg.sensor_info.subdev_id[i]);
		CDBG("%s:%d mount angle valid %d value %d\n", __func__,
			__LINE__, cdata->cfg.sensor_info.is_mount_angle_valid,
			cdata->cfg.sensor_info.sensor_mount_angle);

		break;
	case CFG_SET_INIT_SETTING:
		/* 1. Write Recommend settings */
		/* 2. Write change settings */
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_table(
			s_ctrl->sensor_i2c_client, gc0310_recommend_setting);
		break;

	case CFG_SET_RESOLUTION: {
	/*copy from user the desired resoltuion*/
		enum msm_sensor_resolution_t res = MSM_SENSOR_INVALID_RES;
		if (copy_from_user(&res, (void *)cdata->cfg.setting,
			sizeof(enum msm_sensor_resolution_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		pr_err("%s:%d  res =%d\n", __func__, __LINE__, res);

		if (res == MSM_SENSOR_RES_FULL) {
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_table(
				s_ctrl->sensor_i2c_client, gc0310_recommend_setting);
				pr_err("%s:%d res =%d\n gc0310_recommend_setting ",
				__func__, __LINE__, res);
		} else {
			pr_err("%s:%d failed resoultion set\n", __func__,
				__LINE__);
			rc = -EFAULT;
		}
	}
		break;
	case CFG_SET_STOP_STREAM:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_table(
			s_ctrl->sensor_i2c_client, gc0310_stop_settings);
		break;
	case CFG_SET_START_STREAM:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_table(
			s_ctrl->sensor_i2c_client, gc0310_start_settings);
		break;
	case CFG_GET_SENSOR_INIT_PARAMS:
		cdata->cfg.sensor_init_params.modes_supported =
			s_ctrl->sensordata->sensor_info->modes_supported;
		cdata->cfg.sensor_init_params.position =
			s_ctrl->sensordata->sensor_info->position;
		cdata->cfg.sensor_init_params.sensor_mount_angle =
			s_ctrl->sensordata->sensor_info->sensor_mount_angle;
		pr_err("%s:%d init params mode %d pos %d mount %d\n", __func__,
			__LINE__,
			cdata->cfg.sensor_init_params.modes_supported,
			cdata->cfg.sensor_init_params.position,
			cdata->cfg.sensor_init_params.sensor_mount_angle);
		break;
	case CFG_SET_SLAVE_INFO: {
		struct msm_camera_sensor_slave_info *sensor_slave_info;
		struct msm_camera_power_ctrl_t *p_ctrl;
		uint16_t size;
		int slave_index = 0;
		sensor_slave_info = kmalloc(sizeof(struct msm_camera_sensor_slave_info)
				      * 1, GFP_KERNEL);

		if (!sensor_slave_info) {
			pr_err("%s: failed to alloc mem\n", __func__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(sensor_slave_info,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_sensor_slave_info))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		/* Update sensor slave address */
		if (sensor_slave_info->slave_addr)
			s_ctrl->sensor_i2c_client->cci_client->sid =
				sensor_slave_info->slave_addr >> 1;

		/* Update sensor address type */
		s_ctrl->sensor_i2c_client->addr_type =
			sensor_slave_info->addr_type;

		/* Update power up / down sequence */
		p_ctrl = &s_ctrl->sensordata->power_info;
		size = sensor_slave_info->power_setting_array.size;
		if (p_ctrl->power_setting_size < size) {
			struct msm_sensor_power_setting *tmp;
			tmp = kmalloc(sizeof(struct msm_sensor_power_setting)
				      * size, GFP_KERNEL);
			if (!tmp) {
				pr_err("%s: failed to alloc mem\n", __func__);
				rc = -ENOMEM;
				break;
			}
			kfree(p_ctrl->power_setting);
			p_ctrl->power_setting = tmp;
		}
		p_ctrl->power_setting_size = size;

		rc = copy_from_user(p_ctrl->power_setting, (void *)
			sensor_slave_info->power_setting_array.power_setting,
			size * sizeof(struct msm_sensor_power_setting));
		if (rc) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		for (slave_index = 0; slave_index <
			p_ctrl->power_setting_size; slave_index++) {
			CDBG("%s i %d power setting %d %d %ld %d\n", __func__,
				slave_index,
				p_ctrl->power_setting[slave_index].seq_type,
				p_ctrl->power_setting[slave_index].seq_val,
				p_ctrl->power_setting[slave_index].config_val,
				p_ctrl->power_setting[slave_index].delay);
		}
		break;
	}
	case CFG_WRITE_I2C_ARRAY: {
		struct msm_camera_i2c_reg_setting conf_array;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;

		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		if (!conf_array.size ||
			conf_array.size > I2C_REG_DATA_MAX) {
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
	case CFG_WRITE_I2C_SEQ_ARRAY: {
		struct msm_camera_i2c_seq_reg_setting conf_array;
		struct msm_camera_i2c_seq_reg_array *reg_setting = NULL;

		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_seq_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		if (!conf_array.size ||
			conf_array.size > I2C_SEQ_REG_DATA_MAX) {
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
		if (s_ctrl->func_tbl->sensor_power_up)
			rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
		else
			rc = -EFAULT;
		break;

	case CFG_POWER_DOWN:
		if (s_ctrl->func_tbl->sensor_power_down)
			rc = s_ctrl->func_tbl->sensor_power_down(s_ctrl);
		else
			rc = -EFAULT;
		break;

	case CFG_SET_STOP_STREAM_SETTING: {
		struct msm_camera_i2c_reg_setting *stop_setting =
			&s_ctrl->stop_setting;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;
		if (copy_from_user(stop_setting, (void *)cdata->cfg.setting,
		    sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = stop_setting->reg_setting;
		stop_setting->reg_setting = kzalloc(stop_setting->size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!stop_setting->reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(stop_setting->reg_setting,
		    (void *)reg_setting, stop_setting->size *
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
	case CFG_SET_STREAM_TYPE: {
		enum msm_camera_stream_type_t stream_type = MSM_CAMERA_STREAM_INVALID;
		if (copy_from_user(&stream_type, (void *)cdata->cfg.setting,
			sizeof(enum msm_camera_stream_type_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		s_ctrl->camera_stream_type = stream_type;
		break;
	}
	case CFG_SET_SATURATION:
		break;
	case CFG_SET_CONTRAST:
		break;
	case CFG_SET_SHARPNESS:
		break;
	case CFG_SET_AUTOFOCUS:
		/* TO-DO: set the Auto Focus */
		pr_debug("%s: Setting Auto Focus", __func__);
		break;
	case CFG_CANCEL_AUTOFOCUS:
		/* TO-DO: Cancel the Auto Focus */
		pr_debug("%s: Cancelling Auto Focus", __func__);
		break;
	case CFG_SET_ISO:
		break;
	case CFG_SET_EXPOSURE_COMPENSATION:
		break;
	case CFG_SET_EFFECT:
		break;
	case CFG_SET_ANTIBANDING:
		break;
	case CFG_SET_BESTSHOT_MODE:
		break;
	case CFG_SET_WHITE_BALANCE:
		break;
	default:
		rc = -EFAULT;
		break;
	}

	mutex_unlock(s_ctrl->msm_sensor_mutex);

	return rc;
}
#endif

static struct msm_sensor_fn_t gc0310_sensor_func_tbl = {
	.sensor_config = gc0310_sensor_config,
#ifdef CONFIG_COMPAT
	.sensor_config32 = gc0310_sensor_config32,
#endif
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = msm_sensor_match_id,
};

static struct msm_sensor_ctrl_t gc0310_s_ctrl = {
	.sensor_i2c_client = &gc0310_sensor_i2c_client,
	.power_setting_array.power_setting = gc0310_power_setting,
	.power_setting_array.size = ARRAY_SIZE(gc0310_power_setting),
	.msm_sensor_mutex = &gc0310_mut,
	.sensor_v4l2_subdev_info = gc0310_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(gc0310_subdev_info),
	.func_tbl = &gc0310_sensor_func_tbl,
};

module_init(gc0310_init_module);
module_exit(gc0310_exit_module);
MODULE_DESCRIPTION("Aptina 0.3MP YUV sensor driver");
MODULE_LICENSE("GPL v2");
