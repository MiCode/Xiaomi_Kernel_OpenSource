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
#define OV7695_SENSOR_NAME "ov7695"
#define PLATFORM_DRIVER_NAME "msm_camera_ov7695"
#define ov7695_obj ov7695_##obj

/*#define CONFIG_MSMB_CAMERA_DEBUG*/
#undef CDBG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif


DEFINE_MSM_MUTEX(ov7695_mut);
static struct msm_sensor_ctrl_t ov7695_s_ctrl;

static struct msm_sensor_power_setting ov7695_power_setting[] = {
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_LOW,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_LOW,
		.delay = 10,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VAF,
		.config_val = 0,
		.delay = 5,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VIO,
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
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_HIGH,
		.delay = 5,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_HIGH,
		.delay = 10,
	},
	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 24000000,
		.delay = 10,
	},
	{
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 0,
	},
};

static struct msm_camera_i2c_reg_conf ov7695_vga_settings[] = {
	{0x3630, 0x79,},
};

static struct msm_camera_i2c_reg_conf ov7695_recommend_settings[] = {
	{0x0103, 0x01,},
	{0x0100, 0x01,},
	{0x3620, 0x2f,},
	{0x3623, 0x12,},
	{0x3718, 0x88,},
	{0x3703, 0x80,},
	{0x3712, 0x40,},
	{0x3706, 0x40,},
	{0x3631, 0x44,},
	{0x3632, 0x05,},
	{0x3013, 0xd0,},
	{0x3705, 0x1d,},
	{0x3713, 0x0e,},
	{0x3012, 0x0a,},
	{0x3717, 0x18,},
	{0x3621, 0x47,},
	{0x0309, 0x24,},
	{0x3820, 0x90,},
	{0x4803, 0x08,},
	{0x0101, 0x01,},
	{0x5100, 0x01,},
	{0x4500, 0x24,},
	{0x5301, 0x05,},
	{0x5302, 0x0c,},
	{0x5303, 0x1c,},
	{0x5304, 0x2a,},
	{0x5305, 0x39,},
	{0x5306, 0x45,},
	{0x5307, 0x52,},
	{0x5308, 0x5d,},
	{0x5309, 0x68,},
	{0x530a, 0x7f,},
	{0x530b, 0x91,},
	{0x530c, 0xa5,},
	{0x530d, 0xc6,},
	{0x530e, 0xde,},
	{0x530f, 0xef,},
	{0x5310, 0x16,},
	{0x520a, 0x74,}, //f4
	{0x520b, 0x64,}, //f4
	{0x520c, 0xd4,}, //f4
	{0x5504, 0x08,},
	{0x5505, 0x48,},
	{0x5506, 0x07,},
	{0x5507, 0x0b,},
	{0x3a18, 0x01,},
	{0x3a19, 0x00,},
	{0x3503, 0x03,},
	{0x3500, 0x00,},
	{0x3501, 0x21,},
	{0x3502, 0x00,},
	{0x350a, 0x00,},
	{0x350b, 0x00,},
	{0x4008, 0x02,},
	{0x4009, 0x09,},
	{0x3002, 0x09,},
	{0x3024, 0x00,},
	{0x3503, 0x00,},
	{0x0101, 0x01,},
	{0x5002, 0x48,},
	{0x5910, 0x00,},
	{0x3a0f, 0x58,},
	{0x3a10, 0x50,},
	{0x3a1b, 0x5a,},
	{0x3a1e, 0x4e,},
	{0x3a11, 0xa0,},
	{0x3a1f, 0x28,},
	{0x3a18, 0x00,},
	{0x3a19, 0xf8,},
	{0x3503, 0x00,},
	{0x3a0d, 0x04,},
	{0x5000, 0xff,},
	{0x5001, 0x3f,},
	{0x5100, 0x1 ,}, //01
	{0x5101, 0x25,}, //48
	{0x5102, 0x0 ,}, //00
	{0x5103, 0xf3,}, //f8
	{0x5104, 0x7f,}, //04
	{0x5105, 0x5 ,}, //00
	{0x5106, 0xff,}, //00
	{0x5107, 0xf ,}, //00
	{0x5108, 0x1 ,}, //01
	{0x5109, 0x1f,}, //48
	{0x510a, 0x0 ,}, //00
	{0x510b, 0xde,}, //f8
	{0x510c, 0x56,}, //03
	{0x510d, 0x5 ,}, //00
	{0x510e, 0xff,}, //00
	{0x510f, 0xf ,}, //00
	{0x5110, 0x1 ,}, //01
	{0x5111, 0x23,}, //48
	{0x5112, 0x0 ,}, //00
	{0x5113, 0xe3,}, //f8
	{0x5114, 0x5c,}, //03
	{0x5115, 0x5 ,}, //00
	{0x5116, 0xff,}, //00
	{0x5117, 0xf ,}, //00
	{0x520a, 0x74,}, //f4
	{0x520b, 0x64,}, //f4
	{0x520c, 0xd4,}, //f4
	{0x5004, 0x41,},
	{0x5006, 0x41,},
	{0x5301, 0x05,},
	{0x5302, 0x0c,},
	{0x5303, 0x1c,},
	{0x5304, 0x2a,},
	{0x5305, 0x39,},
	{0x5306, 0x45,},
	{0x5307, 0x53,},
	{0x5308, 0x5d,},
	{0x5309, 0x68,},
	{0x530a, 0x7f,},
	{0x530b, 0x91,},
	{0x530c, 0xa5,},
	{0x530d, 0xc6,},
	{0x530e, 0xde,},
	{0x530f, 0xef,},
	{0x5310, 0x16,},
	{0x5003, 0x80,},
	{0x5500, 0x08,},
	{0x5501, 0x48,},
	{0x5502, 0x18,},
	{0x5503, 0x04,},
	{0x5504, 0x08,},
	{0x5505, 0x48,},
	{0x5506, 0x02,},
	{0x5507, 0x16,},
	{0x5508, 0x2d,},
	{0x5509, 0x08,},
	{0x550a, 0x48,},
	{0x550b, 0x06,},
	{0x550c, 0x04,},
	{0x550d, 0x01,},
	{0x5800, 0x02,},
	{0x5803, 0x2e,},
	{0x5804, 0x20,},
	{0x5600, 0x00,},
	{0x5601, 0x2c,},
	{0x5602, 0x5a,},
	{0x5603, 0x06,},
	{0x5604, 0x1c,},
	{0x5605, 0x65,},
	{0x5606, 0x81,},
	{0x5607, 0x9f,},
	{0x5608, 0x8a,},
	{0x5609, 0x15,},
	{0x560a, 0x01,},
	{0x560b, 0x9c,},
	{0x3811, 0x07,},
	{0x3813, 0x06,},
	{0x3630, 0x79,},
};

static struct v4l2_subdev_info ov7695_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
};

static struct msm_camera_i2c_reg_conf ov7695_start_settings[] = {
	{0x301a, 0xf0},
};

static struct msm_camera_i2c_reg_conf ov7695_stop_settings[] = {
	{0x301a, 0xf4},
};

static const struct i2c_device_id ov7695_i2c_id[] = {
	{OV7695_SENSOR_NAME, (kernel_ulong_t)&ov7695_s_ctrl},
	{ }
};

static int32_t msm_ov7695_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &ov7695_s_ctrl);
}

static struct i2c_driver ov7695_i2c_driver = {
	.id_table = ov7695_i2c_id,
	.probe  = msm_ov7695_i2c_probe,
	.driver = {
		.name = OV7695_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client ov7695_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id ov7695_dt_match[] = {
	{.compatible = "ovti,ov7695", .data = &ov7695_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, ov7695_dt_match);

static int32_t ov7695_platform_probe(struct platform_device *pdev)
{
	int32_t rc;
	const struct of_device_id *match;
	match = of_match_device(ov7695_dt_match, &pdev->dev);
	rc = msm_sensor_platform_probe(pdev, match->data);
	return rc;
}

static struct platform_driver ov7695_platform_driver = {
	.driver = {
		.name = "ovti,ov7695",
		.owner = THIS_MODULE,
		.of_match_table = ov7695_dt_match,
	},
	.probe = ov7695_platform_probe,
};

static int __init ov7695_init_module(void)
{
	int32_t rc;
	pr_err("%s:%d\n", __func__, __LINE__);
	rc = platform_driver_register(&ov7695_platform_driver);
	if (!rc)
		return rc;
	pr_err("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&ov7695_i2c_driver);
}

static void __exit ov7695_exit_module(void)
{
	pr_err("%s:%d\n", __func__, __LINE__);
	if (ov7695_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&ov7695_s_ctrl);
		platform_driver_unregister(&ov7695_platform_driver);
	} else
		i2c_del_driver(&ov7695_i2c_driver);
	return;
}

int32_t ov7695_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
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
		for (i = 0; i < SUB_MODULE_MAX; i++) {
			cdata->cfg.sensor_info.subdev_id[i] =
				s_ctrl->sensordata->sensor_info->subdev_id[i];
			cdata->cfg.sensor_info.subdev_intf[i] =
				s_ctrl->sensordata->sensor_info->subdev_intf[i];
		}
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
			i2c_write_conf_tbl(
			s_ctrl->sensor_i2c_client, ov7695_recommend_settings,
			ARRAY_SIZE(ov7695_recommend_settings),
			MSM_CAMERA_I2C_BYTE_DATA);
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
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, ov7695_vga_settings,
				ARRAY_SIZE(ov7695_vga_settings),
				MSM_CAMERA_I2C_BYTE_DATA);
				pr_err("%s:%d res =%d\n ov7695_vga_settings ",
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
			i2c_write_conf_tbl(
			s_ctrl->sensor_i2c_client, ov7695_stop_settings,
			ARRAY_SIZE(ov7695_stop_settings),
			MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CFG_SET_START_STREAM:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_conf_tbl(
			s_ctrl->sensor_i2c_client, ov7695_start_settings,
			ARRAY_SIZE(ov7695_start_settings),
			MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CFG_GET_SENSOR_INIT_PARAMS:
		cdata->cfg.sensor_init_params.modes_supported =
			s_ctrl->sensordata->sensor_info->modes_supported;
		cdata->cfg.sensor_init_params.position =
			s_ctrl->sensordata->sensor_info->position;
		cdata->cfg.sensor_init_params.sensor_mount_angle =
			s_ctrl->sensordata->sensor_info->sensor_mount_angle;
		CDBG("%s:%d init params mode %d pos %d mount %d\n", __func__,
			__LINE__,
			cdata->cfg.sensor_init_params.modes_supported,
			cdata->cfg.sensor_init_params.position,
			cdata->cfg.sensor_init_params.sensor_mount_angle);
		break;
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
		enum msm_camera_stream_type_t stream_type =
					MSM_CAMERA_STREAM_INVALID;
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
int32_t ov7695_sensor_config32(struct msm_sensor_ctrl_t *s_ctrl,
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
		for (i = 0; i < SUB_MODULE_MAX; i++) {
			cdata->cfg.sensor_info.subdev_id[i] =
				s_ctrl->sensordata->sensor_info->subdev_id[i];
			cdata->cfg.sensor_info.subdev_intf[i] =
				s_ctrl->sensordata->sensor_info->subdev_intf[i];
		}
		cdata->cfg.sensor_info.is_mount_angle_valid =
			s_ctrl->sensordata->sensor_info->is_mount_angle_valid;
		cdata->cfg.sensor_info.sensor_mount_angle =
			s_ctrl->sensordata->sensor_info->sensor_mount_angle;
		cdata->cfg.sensor_info.position =
			s_ctrl->sensordata->sensor_info->position;
		cdata->cfg.sensor_info.modes_supported =
			s_ctrl->sensordata->sensor_info->modes_supported;
		CDBG("%s:%d sensor name %s\n", __func__, __LINE__,
			cdata->cfg.sensor_info.sensor_name);
		CDBG("%s:%d session id %d\n", __func__, __LINE__,
			cdata->cfg.sensor_info.session_id);
		break;
	case CFG_SET_INIT_SETTING:
		/* 1. Write Recommend settings */
		/* 2. Write change settings */
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_conf_tbl(
			s_ctrl->sensor_i2c_client, ov7695_recommend_settings,
			ARRAY_SIZE(ov7695_recommend_settings),
			MSM_CAMERA_I2C_BYTE_DATA);
		break;

	case CFG_SET_RESOLUTION: {
	/*copy from user the desired resoltuion*/
		enum msm_sensor_resolution_t res = MSM_SENSOR_INVALID_RES;
		if (copy_from_user(&res,
			(void *)compat_ptr(cdata->cfg.setting),
			sizeof(enum msm_sensor_resolution_t))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		pr_err("%s:%d  res =%d\n", __func__, __LINE__, res);

		if (res == MSM_SENSOR_RES_FULL) {
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, ov7695_vga_settings,
				ARRAY_SIZE(ov7695_vga_settings),
				MSM_CAMERA_I2C_BYTE_DATA);
				pr_err("%s:%d res =%d\n ov7695_vga_settings ",
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
			i2c_write_conf_tbl(
			s_ctrl->sensor_i2c_client, ov7695_stop_settings,
			ARRAY_SIZE(ov7695_stop_settings),
			MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CFG_SET_START_STREAM:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_conf_tbl(
			s_ctrl->sensor_i2c_client, ov7695_start_settings,
			ARRAY_SIZE(ov7695_start_settings),
			MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CFG_GET_SENSOR_INIT_PARAMS:
		cdata->cfg.sensor_init_params.modes_supported =
			s_ctrl->sensordata->sensor_info->modes_supported;
		cdata->cfg.sensor_init_params.position =
			s_ctrl->sensordata->sensor_info->position;
		cdata->cfg.sensor_init_params.sensor_mount_angle =
			s_ctrl->sensordata->sensor_info->sensor_mount_angle;
		CDBG("%s:%d init params mode %d pos %d mount %d\n", __func__,
			__LINE__,
			cdata->cfg.sensor_init_params.modes_supported,
			cdata->cfg.sensor_init_params.position,
			cdata->cfg.sensor_init_params.sensor_mount_angle);
		break;
	case CFG_WRITE_I2C_ARRAY: {
		struct msm_camera_i2c_reg_setting32 conf_array32;
		struct msm_camera_i2c_reg_setting conf_array;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;

		if (copy_from_user(&conf_array32,
			(void *)compat_ptr(cdata->cfg.setting),
			sizeof(struct msm_camera_i2c_reg_setting32))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		conf_array.addr_type = conf_array32.addr_type;
		conf_array.data_type = conf_array32.data_type;
		conf_array.delay = conf_array32.delay;
		conf_array.size = conf_array32.size;
		conf_array.reg_setting = compat_ptr(conf_array32.reg_setting);
		conf_array.qup_i2c_batch = conf_array32.qup_i2c_batch;

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
	case CFG_SET_STREAM_TYPE: {
		enum msm_camera_stream_type_t stream_type =
			MSM_CAMERA_STREAM_INVALID;
		if (copy_from_user(&stream_type,
			(void *)compat_ptr(cdata->cfg.setting),
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
		pr_err("Invalid cfgtype func %s line %d cfgtype = %d\n",
			__func__, __LINE__, (int32_t)cdata->cfgtype);
		rc = -EFAULT;
		break;
	}

	mutex_unlock(s_ctrl->msm_sensor_mutex);

	return rc;
}
#endif

static struct msm_sensor_fn_t ov7695_sensor_func_tbl = {
	.sensor_config = ov7695_sensor_config,
#ifdef CONFIG_COMPAT
	.sensor_config32 = ov7695_sensor_config32,
#endif
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = msm_sensor_match_id,
};

static struct msm_sensor_ctrl_t ov7695_s_ctrl = {
	.sensor_i2c_client = &ov7695_sensor_i2c_client,
	.power_setting_array.power_setting = ov7695_power_setting,
	.power_setting_array.size = ARRAY_SIZE(ov7695_power_setting),
	.msm_sensor_mutex = &ov7695_mut,
	.sensor_v4l2_subdev_info = ov7695_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(ov7695_subdev_info),
	.func_tbl = &ov7695_sensor_func_tbl,
};

module_init(ov7695_init_module);
module_exit(ov7695_exit_module);
MODULE_DESCRIPTION("Aptina 0.3MP YUV sensor driver");
MODULE_LICENSE("GPL v2");
