/* Copyright (c) 2011-2015, The Linux Foundation. All rights reserved.
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
#define OV5645_SENSOR_NAME "ov5645"
#define PLATFORM_DRIVER_NAME "msm_camera_ov5645"
#define ov5645_obj ov5645_##obj

#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)

DEFINE_MSM_MUTEX(ov5645_mut);
static struct msm_sensor_ctrl_t ov5645_s_ctrl;

static struct msm_sensor_power_setting ov5645_power_setting[] = {
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VIO,
		.config_val = 0,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VANA,
		.config_val = 0,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VDIG,
		.config_val = 0,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_LOW,
		.delay = 5,
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

static struct msm_camera_i2c_reg_conf ov5645_sxga_settings[] = {
	{0x3612, 0xa9,},
	{0x3614, 0x50,},
	{0x3618, 0x00,},
	{0x3034, 0x18,},
	{0x3035, 0x21,},
	{0x3036, 0x70,},
	{0x3600, 0x09,},
	{0x3601, 0x43,},
	{0x3708, 0x66,},
	{0x370c, 0xc3,},
	{0x3800, 0x00,},
	{0x3801, 0x00,},
	{0x3802, 0x00,},
	{0x3803, 0x06,},
	{0x3804, 0x0a,},
	{0x3805, 0x3f,},
	{0x3806, 0x07,},
	{0x3807, 0x9d,},
	{0x3808, 0x05,},
	{0x3809, 0x00,},
	{0x380a, 0x03,},
	{0x380b, 0xc0,},
	{0x380c, 0x07,},
	{0x380d, 0x68,},
	{0x380e, 0x03,},
	{0x380f, 0xd8,},
	{0x3813, 0x06,},
	{0x3814, 0x31,},
	{0x3815, 0x31,},
	{0x3820, 0x47,},
	{0x3a02, 0x03,},
	{0x3a03, 0xd8,},
	{0x3a08, 0x01,},
	{0x3a09, 0xf8,},
	{0x3a0a, 0x01,},
	{0x3a0b, 0xa4,},
	{0x3a0e, 0x02,},
	{0x3a0d, 0x02,},
	{0x3a14, 0x03,},
	{0x3a15, 0xd8,},
	{0x3a18, 0x00,},
	{0x4004, 0x02,},
	{0x4005, 0x18,},
	{0x4300, 0x30,},
	{0x4202, 0x00,},

};

static struct msm_camera_i2c_reg_conf ov5645_full_settings[] = {
	{0x3612, 0xab,},
	{0x3614, 0x50,},
	{0x3618, 0x04,},
	{0x3034, 0x18,},
	{0x3035, 0x11,},
	{0x3036, 0x54,},
	{0x3600, 0x08,},
	{0x3601, 0x33,},
	{0x3708, 0x63,},
	{0x370c, 0xc0,},
	{0x3800, 0x00,},
	{0x3801, 0x00,},
	{0x3802, 0x00,},
	{0x3803, 0x00,},
	{0x3804, 0x0a,},
	{0x3805, 0x3f,},
	{0x3806, 0x07,},
	{0x3807, 0x9f,},
	{0x3808, 0x0a,},
	{0x3809, 0x20,},
	{0x380a, 0x07,},
	{0x380b, 0x98,},
	{0x380c, 0x0b,},
	{0x380d, 0x1c,},
	{0x380e, 0x07,},
	{0x380f, 0xb0,},
	{0x3813, 0x06,},
	{0x3814, 0x11,},
	{0x3815, 0x11,},
	{0x3820, 0x47,},
	{0x4514, 0x88,},
	{0x3a02, 0x07,},
	{0x3a03, 0xb0,},
	{0x3a08, 0x01,},
	{0x3a09, 0x27,},
	{0x3a0a, 0x00,},
	{0x3a0b, 0xf6,},
	{0x3a0e, 0x06,},
	{0x3a0d, 0x08,},
	{0x3a14, 0x07,},
	{0x3a15, 0xb0,},
	{0x3a18, 0x01,},
	{0x4004, 0x06,},
	{0x4005, 0x18,},
	{0x4300, 0x30,},
	{0x4837, 0x0b,},
	{0x4202, 0x00,},
};

static struct msm_camera_i2c_reg_conf ov5645_1080P_settings[] = {
	{0x3612, 0xab,},
	{0x3614, 0x50,},
	{0x3618, 0x04,},
	{0x3034, 0x18,},
	{0x3035, 0x11,},
	{0x3036, 0x54,},
	{0x3600, 0x08,},
	{0x3601, 0x33,},
	{0x3708, 0x63,},
	{0x370c, 0xc0,},
	{0x3800, 0x01,},
	{0x3801, 0x50,},
	{0x3802, 0x01,},
	{0x3803, 0xb2,},
	{0x3804, 0x08,},
	{0x3805, 0xef,},
	{0x3806, 0x05,},
	{0x3807, 0xf1,},
	{0x3808, 0x07,},
	{0x3809, 0x80,},
	{0x380a, 0x04,},
	{0x380b, 0x38,},
	{0x380c, 0x09,},
	{0x380d, 0xc4,},
	{0x380e, 0x04,},
	{0x380f, 0x60,},
	{0x3813, 0x04,},
	{0x3814, 0x11,},
	{0x3815, 0x11,},
	{0x3820, 0x47,},
	{0x4514, 0x88,},
	{0x3a02, 0x04,},
	{0x3a03, 0x60,},
	{0x3a08, 0x01,},
	{0x3a09, 0x50,},
	{0x3a0a, 0x01,},
	{0x3a0b, 0x18,},
	{0x3a0e, 0x03,},
	{0x3a0d, 0x04,},
	{0x3a14, 0x04,},
	{0x3a15, 0x60,},
	{0x3a18, 0x00,},
	{0x4004, 0x06,},
	{0x4005, 0x18,},
	{0x4300, 0x30,},
	{0x4202, 0x00,},
	{0x4837, 0x0b,},
};


static struct msm_camera_i2c_reg_conf ov5645_recommend_settings[] = {
	{0x3103, 0x11,},
	{0x3008, 0x82,},
	{0x3008, 0x42,},
	{0x3103, 0x03,},
	{0x3503, 0x07,},
	{0x3002, 0x1c,},
	{0x3006, 0xc3,},
	{0x300e, 0x45,},
	{0x3017, 0x00,},
	{0x3018, 0x00,},
	{0x302e, 0x0b,},
	{0x3037, 0x13,},
	{0x3108, 0x01,},
	{0x3611, 0x06,},
	{0x3500, 0x00,},
	{0x3501, 0x01,},
	{0x3502, 0x00,},
	{0x350a, 0x00,},
	{0x350b, 0x3f,},
	{0x3620, 0x33,},
	{0x3621, 0xe0,},
	{0x3622, 0x01,},
	{0x3630, 0x2e,},
	{0x3631, 0x00,},
	{0x3632, 0x32,},
	{0x3633, 0x52,},
	{0x3634, 0x70,},
	{0x3635, 0x13,},
	{0x3636, 0x03,},
	{0x3703, 0x5a,},
	{0x3704, 0xa0,},
	{0x3705, 0x1a,},
	{0x3709, 0x12,},
	{0x370b, 0x61,},
	{0x370f, 0x10,},
	{0x3715, 0x78,},
	{0x3717, 0x01,},
	{0x371b, 0x20,},
	{0x3731, 0x12,},
	{0x3901, 0x0a,},
	{0x3905, 0x02,},
	{0x3906, 0x10,},
	{0x3719, 0x86,},
	{0x3810, 0x00,},
	{0x3811, 0x10,},
	{0x3812, 0x00,},
	{0x3821, 0x01,},
	{0x3824, 0x01,},
	{0x3826, 0x03,},
	{0x3828, 0x08,},
	{0x3a19, 0xf8,},
	{0x3c01, 0x34,},
	{0x3c04, 0x28,},
	{0x3c05, 0x98,},
	{0x3c07, 0x07,},
	{0x3c09, 0xc2,},
	{0x3c0a, 0x9c,},
	{0x3c0b, 0x40,},
	{0x3c01, 0x34,},
	{0x4001, 0x02,},
	{0x4514, 0x00,},
	{0x4520, 0xb0,},
	{0x460b, 0x37,},
	{0x460c, 0x20,},
	{0x4818, 0x01,},
	{0x481d, 0xf0,},
	{0x481f, 0x50,},
	{0x4823, 0x70,},
	{0x4831, 0x14,},
	{0x5000, 0xa7,},
	{0x5001, 0x83,},
	{0x501d, 0x00,},
	{0x501f, 0x00,},
	{0x503d, 0x00,},
	{0x505c, 0x30,},
	{0x5181, 0x59,},
	{0x5183, 0x00,},
	{0x5191, 0xf0,},
	{0x5192, 0x03,},
	{0x5684, 0x10,},
	{0x5685, 0xa0,},
	{0x5686, 0x0c,},
	{0x5687, 0x78,},
	{0x5a00, 0x08,},
	{0x5a21, 0x00,},
	{0x5a24, 0x00,},
	{0x3008, 0x02,},
	{0x3503, 0x00,},
	{0x5180, 0xff,},
	{0x5181, 0xf2,},
	{0x5182, 0x00,},
	{0x5183, 0x14,},
	{0x5184, 0x25,},
	{0x5185, 0x24,},
	{0x5186, 0x09,},
	{0x5187, 0x09,},
	{0x5188, 0x0a,},
	{0x5189, 0x75,},
	{0x518a, 0x52,},
	{0x518b, 0xea,},
	{0x518c, 0xa8,},
	{0x518d, 0x42,},
	{0x518e, 0x38,},
	{0x518f, 0x56,},
	{0x5190, 0x42,},
	{0x5191, 0xf8,},
	{0x5192, 0x04,},
	{0x5193, 0x70,},
	{0x5194, 0xf0,},
	{0x5195, 0xf0,},
	{0x5196, 0x03,},
	{0x5197, 0x01,},
	{0x5198, 0x04,},
	{0x5199, 0x12,},
	{0x519a, 0x04,},
	{0x519b, 0x00,},
	{0x519c, 0x06,},
	{0x519d, 0x82,},
	{0x519e, 0x38,},
	{0x5381, 0x1e,},
	{0x5382, 0x5b,},
	{0x5383, 0x08,},
	{0x5384, 0x0a,},
	{0x5385, 0x7e,},
	{0x5386, 0x88,},
	{0x5387, 0x7c,},
	{0x5388, 0x6c,},
	{0x5389, 0x10,},
	{0x538a, 0x01,},
	{0x538b, 0x98,},
	{0x5300, 0x08,},
	{0x5301, 0x30,},
	{0x5302, 0x10,},
	{0x5303, 0x00,},
	{0x5304, 0x08,},
	{0x5305, 0x30,},
	{0x5306, 0x08,},
	{0x5307, 0x16,},
	{0x5309, 0x08,},
	{0x530a, 0x30,},
	{0x530b, 0x04,},
	{0x530c, 0x06,},
	{0x5480, 0x01,},
	{0x5481, 0x08,},
	{0x5482, 0x14,},
	{0x5483, 0x28,},
	{0x5484, 0x51,},
	{0x5485, 0x65,},
	{0x5486, 0x71,},
	{0x5487, 0x7d,},
	{0x5488, 0x87,},
	{0x5489, 0x91,},
	{0x548a, 0x9a,},
	{0x548b, 0xaa,},
	{0x548c, 0xb8,},
	{0x548d, 0xcd,},
	{0x548e, 0xdd,},
	{0x548f, 0xea,},
	{0x5490, 0x1d,},
	{0x5580, 0x02,},
	{0x5583, 0x40,},
	{0x5584, 0x10,},
	{0x5589, 0x10,},
	{0x558a, 0x00,},
	{0x558b, 0xf8,},
	{0x5800, 0x3f,},
	{0x5801, 0x16,},
	{0x5802, 0x0e,},
	{0x5803, 0x0d,},
	{0x5804, 0x17,},
	{0x5805, 0x3f,},
	{0x5806, 0x0b,},
	{0x5807, 0x06,},
	{0x5808, 0x04,},
	{0x5809, 0x04,},
	{0x580a, 0x06,},
	{0x580b, 0x0b,},
	{0x580c, 0x09,},
	{0x580d, 0x03,},
	{0x580e, 0x00,},
	{0x580f, 0x00,},
	{0x5810, 0x03,},
	{0x5811, 0x08,},
	{0x5812, 0x0a,},
	{0x5813, 0x03,},
	{0x5814, 0x00,},
	{0x5815, 0x00,},
	{0x5816, 0x04,},
	{0x5817, 0x09,},
	{0x5818, 0x0f,},
	{0x5819, 0x08,},
	{0x581a, 0x06,},
	{0x581b, 0x06,},
	{0x581c, 0x08,},
	{0x581d, 0x0c,},
	{0x581e, 0x3f,},
	{0x581f, 0x1e,},
	{0x5820, 0x12,},
	{0x5821, 0x13,},
	{0x5822, 0x21,},
	{0x5823, 0x3f,},
	{0x5824, 0x68,},
	{0x5825, 0x28,},
	{0x5826, 0x2c,},
	{0x5827, 0x28,},
	{0x5828, 0x08,},
	{0x5829, 0x48,},
	{0x582a, 0x64,},
	{0x582b, 0x62,},
	{0x582c, 0x64,},
	{0x582d, 0x28,},
	{0x582e, 0x46,},
	{0x582f, 0x62,},
	{0x5830, 0x60,},
	{0x5831, 0x62,},
	{0x5832, 0x26,},
	{0x5833, 0x48,},
	{0x5834, 0x66,},
	{0x5835, 0x44,},
	{0x5836, 0x64,},
	{0x5837, 0x28,},
	{0x5838, 0x66,},
	{0x5839, 0x48,},
	{0x583a, 0x2c,},
	{0x583b, 0x28,},
	{0x583c, 0x26,},
	{0x583d, 0xae,},
	{0x5025, 0x00,},
	{0x3a0f, 0x30,},
	{0x3a10, 0x28,},
	{0x3a1b, 0x30,},
	{0x3a1e, 0x26,},
	{0x3a11, 0x60,},
	{0x3a1f, 0x14,},
	{0x0601, 0x02,},
	{0x3008, 0x42,},

};

static struct v4l2_subdev_info ov5645_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
};

static struct msm_camera_i2c_reg_conf ov5645_start_settings[] = {
	{0x3008, 0x02,},
};

static struct msm_camera_i2c_reg_conf ov5645_stop_settings[] = {
	{0x3008, 0x42,},
};

static struct msm_camera_i2c_reg_conf ov5645_enable_aec_settings[] = {
	{0x3503, 0x00,},
	{0x3406, 0x00,},
};

static struct msm_camera_i2c_reg_conf ov5645_disable_aec_settings[] = {
	{0x3503, 0x07,},
	{0x3406, 0x01,},
};

static const struct i2c_device_id ov5645_i2c_id[] = {
	{OV5645_SENSOR_NAME, (kernel_ulong_t)&ov5645_s_ctrl},
	{ }
};

static int32_t msm_ov5645_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &ov5645_s_ctrl);
}

static struct i2c_driver ov5645_i2c_driver = {
	.id_table = ov5645_i2c_id,
	.probe  = msm_ov5645_i2c_probe,
	.driver = {
		.name = OV5645_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client ov5645_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id ov5645_dt_match[] = {
	{.compatible = "ovti,ov5645", .data = &ov5645_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, ov5645_dt_match);

static int32_t ov5645_platform_probe(struct platform_device *pdev)
{
	int32_t rc;
	const struct of_device_id *match;
	match = of_match_device(ov5645_dt_match, &pdev->dev);
	if (match)
		rc = msm_sensor_platform_probe(pdev, match->data);
	else {
		pr_err("%s:%d match is null\n", __func__, __LINE__);
		rc = -EINVAL;
	}
	return rc;
}

static struct platform_driver ov5645_platform_driver = {
	.driver = {
		.name = "ovti,ov5645",
		.owner = THIS_MODULE,
		.of_match_table = ov5645_dt_match,
	},
	.probe = ov5645_platform_probe,
};

static int __init ov5645_init_module(void)
{
	int32_t rc;
	pr_err("%s:%d\n", __func__, __LINE__);
	rc = platform_driver_register(&ov5645_platform_driver);
	if (!rc)
		return rc;
	pr_err("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&ov5645_i2c_driver);
}

static void __exit ov5645_exit_module(void)
{
	pr_err("%s:%d\n", __func__, __LINE__);
	if (ov5645_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&ov5645_s_ctrl);
		platform_driver_unregister(&ov5645_platform_driver);
	} else
		i2c_del_driver(&ov5645_i2c_driver);
	return;
}

int32_t ov5645_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
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
			s_ctrl->sensor_i2c_client, ov5645_recommend_settings,
			ARRAY_SIZE(ov5645_recommend_settings),
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
				s_ctrl->sensor_i2c_client, ov5645_full_settings,
				ARRAY_SIZE(ov5645_full_settings),
				MSM_CAMERA_I2C_BYTE_DATA);
				pr_err("%s:%d res =%d\n ov5645_full_settings ",
				__func__, __LINE__, res);
		} else if (res == MSM_SENSOR_RES_QTR) {
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, ov5645_sxga_settings,
				ARRAY_SIZE(ov5645_sxga_settings),
				MSM_CAMERA_I2C_BYTE_DATA);
			pr_err("%s:%d res =%d ov5645_sxga_settings\n",
				 __func__, __LINE__, res);
		} else if (res == MSM_SENSOR_RES_2) {
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client,
				ov5645_1080P_settings,
				ARRAY_SIZE(ov5645_1080P_settings),
				MSM_CAMERA_I2C_BYTE_DATA);
			pr_err("%s:%d res =%d ov5645_1080P_settings\n",
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
			s_ctrl->sensor_i2c_client, ov5645_stop_settings,
			ARRAY_SIZE(ov5645_stop_settings),
			MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CFG_SET_START_STREAM:
		if (s_ctrl->camera_stream_type != MSM_CAMERA_STREAM_SNAPSHOT) {
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client,
				ov5645_enable_aec_settings,
				ARRAY_SIZE(ov5645_enable_aec_settings),
				MSM_CAMERA_I2C_BYTE_DATA);
		} else {
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client,
				ov5645_disable_aec_settings,
				ARRAY_SIZE(ov5645_disable_aec_settings),
				MSM_CAMERA_I2C_BYTE_DATA);
		}
		if (rc) {
			pr_err("%s:%d failed rc = %ld\n", __func__, __LINE__,
				rc);
			rc = -EFAULT;
			break;
		}

		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_conf_tbl(
			s_ctrl->sensor_i2c_client, ov5645_start_settings,
			ARRAY_SIZE(ov5645_start_settings),
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
int32_t ov5645_sensor_config32(struct msm_sensor_ctrl_t *s_ctrl,
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
			s_ctrl->sensor_i2c_client, ov5645_recommend_settings,
			ARRAY_SIZE(ov5645_recommend_settings),
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
				s_ctrl->sensor_i2c_client, ov5645_full_settings,
				ARRAY_SIZE(ov5645_full_settings),
				MSM_CAMERA_I2C_BYTE_DATA);
				pr_err("%s:%d res =%d\n ov5645_full_settings ",
				__func__, __LINE__, res);
		} else if (res == MSM_SENSOR_RES_QTR) {
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client, ov5645_sxga_settings,
				ARRAY_SIZE(ov5645_sxga_settings),
				MSM_CAMERA_I2C_BYTE_DATA);
			pr_err("%s:%d res =%d ov5645_sxga_settings\n",
				 __func__, __LINE__, res);
		} else if (res == MSM_SENSOR_RES_2) {
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client,
				ov5645_1080P_settings,
				ARRAY_SIZE(ov5645_1080P_settings),
				MSM_CAMERA_I2C_BYTE_DATA);
			pr_err("%s:%d res =%d ov5645_1080P_settings\n",
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
			s_ctrl->sensor_i2c_client, ov5645_stop_settings,
			ARRAY_SIZE(ov5645_stop_settings),
			MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CFG_SET_START_STREAM:
		if (s_ctrl->camera_stream_type != MSM_CAMERA_STREAM_SNAPSHOT) {
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client,
				ov5645_enable_aec_settings,
				ARRAY_SIZE(ov5645_enable_aec_settings),
				MSM_CAMERA_I2C_BYTE_DATA);
		} else {
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client,
				ov5645_disable_aec_settings,
				ARRAY_SIZE(ov5645_disable_aec_settings),
				MSM_CAMERA_I2C_BYTE_DATA);
		}
		if (rc) {
			pr_err("%s:%d failed rc = %ld\n", __func__, __LINE__,
				rc);
			rc = -EFAULT;
			break;
		}
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_conf_tbl(
			s_ctrl->sensor_i2c_client, ov5645_start_settings,
			ARRAY_SIZE(ov5645_start_settings),
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

static struct msm_sensor_fn_t ov5645_sensor_func_tbl = {
	.sensor_config = ov5645_sensor_config,
#ifdef CONFIG_COMPAT
	.sensor_config32 = ov5645_sensor_config32,
#endif
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = msm_sensor_match_id,
};

static struct msm_sensor_ctrl_t ov5645_s_ctrl = {
	.sensor_i2c_client = &ov5645_sensor_i2c_client,
	.power_setting_array.power_setting = ov5645_power_setting,
	.power_setting_array.size = ARRAY_SIZE(ov5645_power_setting),
	.msm_sensor_mutex = &ov5645_mut,
	.sensor_v4l2_subdev_info = ov5645_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(ov5645_subdev_info),
	.func_tbl = &ov5645_sensor_func_tbl,
	.is_yuv = 1,
};

module_init(ov5645_init_module);
module_exit(ov5645_exit_module);
MODULE_DESCRIPTION("Aptina 1.26MP YUV sensor driver");
MODULE_LICENSE("GPL v2");
