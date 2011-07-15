/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/types.h>
#include <mach/camera.h>
#include <mach/gpio.h>
#include <media/msm_camera.h>
#include "msm_sensor.h"
#include "ov2720.h"
#include "msm.h"
#define SENSOR_NAME "ov2720"
#define PLATFORM_DRIVER_NAME "msm_camera_ov2720"
#define ov2720_obj ov2720_##obj

DEFINE_MUTEX(ov2720_mut);
static struct msm_sensor_ctrl_t ov2720_s_ctrl;

struct msm_sensor_i2c_reg_conf ov2720_start_settings[] = {
	{0x0100, 0x01},
};

struct msm_sensor_i2c_reg_conf ov2720_stop_settings[] = {
	{0x0100, 0x00},
};

struct msm_sensor_i2c_reg_conf ov2720_groupon_settings[] = {
	{0x3208, 0x00},
};

struct msm_sensor_i2c_reg_conf ov2720_groupoff_settings[] = {
	{0x3208, 0x10},
	{0x3208, 0xA0},
};

static struct msm_sensor_i2c_reg_conf ov2720_prev_settings[] = {
	{0x3800, 0x00},
	{0x3801, 0x02},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3804, 0x07},
	{0x3805, 0xA1},
	{0x3806, 0x04},
	{0x3807, 0x47},
	{0x3808, 0x07},
	{0x3809, 0x8C},
	{0x380a, 0x04},
	{0x380b, 0x44},
	{0x380c, 0x08},/*Line Length Pclk Hi*/
	{0x380d, 0x5c},/*Line Length Pclk Lo*/
	{0x380e, 0x04},/*Frame Length Line Hi*/
	{0x380f, 0x60},/*Frame Length Line Lo*/
	{0x3810, 0x00},
	{0x3811, 0x09},
	{0x3812, 0x00},
	{0x3813, 0x02},
	{0x3820, 0x80},
	{0x3821, 0x06},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3612, 0x0b},
	{0x3618, 0x04},
	{0x3a08, 0x01},
	{0x3a09, 0x50},
	{0x3a0a, 0x01},
	{0x3a0b, 0x18},
	{0x3a0d, 0x03},
	{0x3a0e, 0x03},
	{0x4520, 0x00},
	{0x4837, 0x1b},
	{0x3000, 0xff},
	{0x3001, 0xff},
	{0x3002, 0xf0},
	{0x3600, 0x08},
	{0x3621, 0xc0},
	{0x3632, 0xd2},
	{0x3633, 0x23},
	{0x3634, 0x54},
	{0x3f01, 0x0c},
	{0x5001, 0xc1},
	{0x3614, 0xf0},
	{0x3630, 0x2d},
	{0x370b, 0x62},
	{0x3706, 0x61},
	{0x4000, 0x02},
	{0x4002, 0xc5},
	{0x4005, 0x08},
	{0x404f, 0x84},
	{0x4051, 0x00},
	{0x5000, 0xff},
	{0x3a18, 0x00},
	{0x3a19, 0x80},
	{0x3503, 0x13},
	{0x4521, 0x00},
	{0x5183, 0xb0},
	{0x5184, 0xb0},
	{0x5185, 0xb0},
	{0x370c, 0x0c},
	{0x3035, 0x10},
	{0x3036, 0x1e},
	{0x3037, 0x21},
	{0x303e, 0x19},
	{0x3038, 0x06},
	{0x3018, 0x04},
	{0x3000, 0x00},
	{0x3001, 0x00},
	{0x3002, 0x00},
	{0x3a0f, 0x40},
	{0x3a10, 0x38},
	{0x3a1b, 0x48},
	{0x3a1e, 0x30},
	{0x3a11, 0x90},
	{0x3a1f, 0x10},
	{0x4800, 0x24},
};

static struct msm_sensor_i2c_reg_conf ov2720_720_settings[] = {
	{0x3800, 0x01},
	{0x3801, 0x4a},
	{0x3802, 0x00},
	{0x3803, 0xba},
	{0x3804, 0x06},
	{0x3805, 0x51+32},
	{0x3806, 0x03},
	{0x3807, 0x8d+24},
	{0x3808, 0x05},
	{0x3809, 0x00+16},
	{0x380a, 0x02},
	{0x380b, 0x78},
	{0x380c, 0x08},/*Line Length Pclk Hi*/
	{0x380d, 0x5e},/*Line Length Pclk Lo*/
	{0x380e, 0x04},/*Frame Length Line Hi*/
	{0x380f, 0x60},/*Frame Length Line Lo*/
	{0x3810, 0x00},
	{0x3811, 0x05},
	{0x3812, 0x00},
	{0x3813, 0x02},
	{0x3820, 0x80},
	{0x3821, 0x06},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3612, 0x0b},
	{0x3618, 0x04},
	{0x3a08, 0x01},
	{0x3a09, 0x50},
	{0x3a0a, 0x01},
	{0x3a0b, 0x18},
	{0x3a0d, 0x03},
	{0x3a0e, 0x03},
	{0x4520, 0x00},
	{0x4837, 0x1b},
	{0x3000, 0xff},
	{0x3001, 0xff},
	{0x3002, 0xf0},
	{0x3600, 0x08},
	{0x3621, 0xc0},
	{0x3632, 0xd2},
	{0x3633, 0x23},
	{0x3634, 0x54},
	{0x3f01, 0x0c},
	{0x5001, 0xc1},
	{0x3614, 0xf0},
	{0x3630, 0x2d},
	{0x370b, 0x62},
	{0x3706, 0x61},
	{0x4000, 0x02},
	{0x4002, 0xc5},
	{0x4005, 0x08},
	{0x404f, 0x84},
	{0x4051, 0x00},
	{0x5000, 0xff},
	{0x3a18, 0x00},
	{0x3a19, 0x80},
	{0x3503, 0x13},
	{0x4521, 0x00},
	{0x5183, 0xb0},
	{0x5184, 0xb0},
	{0x5185, 0xb0},
	{0x370c, 0x0c},
	{0x3035, 0x10},
	{0x3036, 0x04},
	{0x3037, 0x61},
	{0x303e, 0x19},
	{0x3038, 0x06},
	{0x3018, 0x04},
	{0x3000, 0x00},
	{0x3001, 0x00},
	{0x3002, 0x00},
	{0x3a0f, 0x40},
	{0x3a10, 0x38},
	{0x3a1b, 0x48},
	{0x3a1e, 0x30},
	{0x3a11, 0x90},
	{0x3a1f, 0x10},
	{0x4800, 0x24},
};

static struct msm_sensor_i2c_reg_conf ov2720_vga_settings[] = {
	{0x3800, 0x00},
	{0x3801, 0x0c},
	{0x3802, 0x00},
	{0x3803, 0x02},
	{0x3804, 0x07},
	{0x3805, 0x97+32},
	{0x3806, 0x04},
	{0x3807, 0x45+24},
	{0x3808, 0x02},
	{0x3809, 0x88+16},
	{0x380a, 0x01},
	{0x380b, 0xe6+12},
	{0x380c, 0x08},/*Line Length Pclk Hi*/
	{0x380d, 0x5e},/*Line Length Pclk Lo*/
	{0x380e, 0x04},/*Frame Length Line Hi*/
	{0x380f, 0x68},/*Frame Length Line Lo*/
	{0x3810, 0x00},
	{0x3811, 0x03},
	{0x3812, 0x00},
	{0x3813, 0x03},
	{0x3820, 0x80},
	{0x3821, 0x06},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3612, 0x0b},
	{0x3618, 0x04},
	{0x3a08, 0x01},
	{0x3a09, 0x50},
	{0x3a0a, 0x01},
	{0x3a0b, 0x18},
	{0x3a0d, 0x03},
	{0x3a0e, 0x03},
	{0x4520, 0x00},
	{0x4837, 0x1b},
	{0x3000, 0xff},
	{0x3001, 0xff},
	{0x3002, 0xf0},
	{0x3600, 0x08},
	{0x3621, 0xc0},
	{0x3632, 0xd2},
	{0x3633, 0x23},
	{0x3634, 0x54},
	{0x3f01, 0x0c},
	{0x5001, 0xc1},
	{0x3614, 0xf0},
	{0x3630, 0x2d},
	{0x370b, 0x62},
	{0x3706, 0x61},
	{0x4000, 0x02},
	{0x4002, 0xc5},
	{0x4005, 0x08},
	{0x404f, 0x84},
	{0x4051, 0x00},
	{0x5000, 0xff},
	{0x3a18, 0x00},
	{0x3a19, 0x80},
	{0x3503, 0x13},
	{0x4521, 0x00},
	{0x5183, 0xb0},
	{0x5184, 0xb0},
	{0x5185, 0xb0},
	{0x370c, 0x0c},
	{0x3035, 0x10},
	{0x3036, 0x04},
	{0x3037, 0x61},
	{0x303e, 0x19},
	{0x3038, 0x06},
	{0x3018, 0x04},
	{0x3000, 0x00},
	{0x3001, 0x00},
	{0x3002, 0x00},
	{0x3a0f, 0x40},
	{0x3a10, 0x38},
	{0x3a1b, 0x48},
	{0x3a1e, 0x30},
	{0x3a11, 0x90},
	{0x3a1f, 0x10},
	{0x4800, 0x24},
	{0x3500, 0x00},
	{0x3501, 0x17},
	{0x3502, 0xf0},
	{0x3508, 0x00},
	{0x3509, 0x20},
};

static struct msm_sensor_i2c_reg_conf ov2720_recommend_settings[] = {
	{0x0103, 0x01},
	{0x3718, 0x10},
	{0x3702, 0x24},
	{0x373a, 0x60},
	{0x3715, 0x01},
	{0x3703, 0x2e},
	{0x3705, 0x10},
	{0x3730, 0x30},
	{0x3704, 0x62},
	{0x3f06, 0x3a},
	{0x371c, 0x00},
	{0x371d, 0xc4},
	{0x371e, 0x01},
	{0x371f, 0x0d},
	{0x3708, 0x61},
	{0x3709, 0x12},
	{0x5800, 0x03},
	{0x5801, 0xD0},
	{0x5802, 0x02},
	{0x5803, 0x56},
	{0x5804, 0x22},
	{0x5805, 0x06},
	{0x5806, 0xC2},
	{0x5807, 0x08},
	{0x5808, 0x03},
	{0x5809, 0xD0},
	{0x580A, 0x02},
	{0x580B, 0x56},
	{0x580C, 0x22},
	{0x580D, 0x07},
	{0x580E, 0xC2},
	{0x580F, 0x08},
	{0x5810, 0x03},
	{0x5811, 0xD0},
	{0x5812, 0x02},
	{0x5813, 0x56},
	{0x5814, 0x18},
	{0x5815, 0x07},
	{0x5816, 0xC2},
	{0x5817, 0x08},
	{0x5818, 0x04},
	{0x5819, 0x80},
	{0x581A, 0x06},
	{0x581B, 0x0C},
	{0x581C, 0x80},
};

static struct msm_camera_csi_params ov2720_csi_params = {
	.lane_cnt = 2,
	.data_format = CSI_10BIT,
	.lane_assign = 0xe4,
	.dpcm_scheme = 0,
	.settle_cnt = 0x18,
};

static struct v4l2_subdev_info ov2720_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_sensor_i2c_conf_array ov2720_init_conf[] = {
	{&ov2720_recommend_settings[0],
	ARRAY_SIZE(ov2720_recommend_settings), 0}
};

static struct msm_sensor_i2c_conf_array ov2720_confs[] = {
	{&ov2720_prev_settings[0], ARRAY_SIZE(ov2720_prev_settings), 0},
	{&ov2720_vga_settings[0], ARRAY_SIZE(ov2720_vga_settings), 0},
	{&ov2720_720_settings[0], ARRAY_SIZE(ov2720_720_settings), 0},
};

static int32_t ov2720_write_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{
	uint32_t fl_lines, offset;
	fl_lines =
		(s_ctrl->curr_frame_length_lines * s_ctrl->fps_divider) / Q10;
	offset = s_ctrl->vert_offset;
	if (line > (fl_lines - offset))
		fl_lines = line + offset;

	pr_err("LINE: 0x%x\n", line);
	s_ctrl->func_tbl.sensor_group_hold_on(s_ctrl);
	msm_sensor_i2c_waddr_write_w(s_ctrl,
			s_ctrl->frame_length_lines_addr, fl_lines);
	msm_sensor_i2c_waddr_write_b(s_ctrl,
			s_ctrl->coarse_int_time_addr-1, line >> 12);
	msm_sensor_i2c_waddr_write_w(s_ctrl,
			s_ctrl->coarse_int_time_addr, ((line << 4) & 0xFFFF));
	msm_sensor_i2c_waddr_write_w(s_ctrl,
			s_ctrl->global_gain_addr, gain);
	s_ctrl->func_tbl.sensor_group_hold_off(s_ctrl);
	return 0;
}

static int32_t ov2720_sensor_setting(struct msm_sensor_ctrl_t *s_ctrl,
				int update_type, int rt)
{
	struct msm_camera_csid_params ov2720_csid_params;
	struct msm_camera_csiphy_params ov2720_csiphy_params;
	int32_t rc = 0;
	s_ctrl->func_tbl.sensor_stop_stream(s_ctrl);
	msleep(30);
	if (update_type == MSM_SENSOR_REG_INIT) {
		s_ctrl->config_csi_flag = 1;
		msm_sensor_enable_debugfs(s_ctrl);
		msm_sensor_write_b_init_settings(s_ctrl);
	} else if (update_type == MSM_SENSOR_UPDATE_PERIODIC) {
		msm_sensor_write_b_res_settings(s_ctrl, rt);
		if (s_ctrl->config_csi_flag) {
			struct msm_camera_csid_vc_cfg ov2720_vccfg[] = {
				{0, CSI_RAW10, CSI_DECODE_10BIT},
				{1, CSI_EMBED_DATA, CSI_DECODE_8BIT},
			};
			ov2720_csid_params.lane_cnt = 2;
			ov2720_csid_params.lane_assign = 0xe4;
			ov2720_csid_params.lut_params.num_cid =
				ARRAY_SIZE(ov2720_vccfg);
			ov2720_csid_params.lut_params.vc_cfg =
				&ov2720_vccfg[0];
			ov2720_csiphy_params.lane_cnt = 2;
			ov2720_csiphy_params.settle_cnt = 0x1B;
			rc = msm_camio_csid_config(&ov2720_csid_params);
			v4l2_subdev_notify(s_ctrl->sensor_v4l2_subdev,
						NOTIFY_CID_CHANGE, NULL);
			mb();
			rc = msm_camio_csiphy_config(&ov2720_csiphy_params);
			mb();
			msleep(20);
			s_ctrl->config_csi_flag = 0;
		}
		s_ctrl->func_tbl.sensor_start_stream(s_ctrl);
		msleep(30);
	}
	return rc;
}

static int ov2720_sensor_config(void __user *argp)
{
	return (int) msm_sensor_config(&ov2720_s_ctrl, argp);
}

static int ov2720_power_down(const struct msm_camera_sensor_info *data)
{
	pr_err("%s\n", __func__);
	gpio_set_value_cansleep(data->sensor_reset, 0);
	gpio_free(data->sensor_reset);
	return 0;
}

static int ov2720_power_up(const struct msm_camera_sensor_info *data)
{
	int32_t rc = 0;
	pr_err("%s: %d\n", __func__, __LINE__);
	msm_camio_clk_rate_set(MSM_SENSOR_MCLK_24HZ);
	rc = gpio_request(data->sensor_reset, "SENSOR_NAME");
	if (rc < 0)
		goto gpio_request_fail;

	pr_err("%s: reset sensor\n", __func__);
	gpio_direction_output(data->sensor_reset, 0);
	msleep(50);
	gpio_set_value_cansleep(data->sensor_reset, 1);
	msleep(50);

	rc = msm_sensor_match_id(&ov2720_s_ctrl);
	if (rc < 0)
		goto init_probe_fail;

	goto init_probe_done;
gpio_request_fail:
	pr_err("%s: gpio request fail\n", __func__);
	return rc;
init_probe_fail:
	pr_err(" %s fails\n", __func__);
	ov2720_power_down(data);
	return rc;
init_probe_done:
	pr_err("%s finishes\n", __func__);
	return rc;
}

static int ov2720_sensor_open_init(const struct msm_camera_sensor_info *data)
{
	int32_t rc = 0;
	pr_err("%s: %d\n", __func__, __LINE__);
	ov2720_s_ctrl.fps = 30*Q8;
	ov2720_s_ctrl.fps_divider = 1 * 0x00000400;
	ov2720_s_ctrl.cam_mode = MSM_SENSOR_MODE_INVALID;

	if (data)
		ov2720_s_ctrl.sensordata = data;

	rc = ov2720_power_up(data);
	if (rc < 0)
		goto init_done;

	goto init_done;
init_done:
	pr_err("%s finishes\n", __func__);
	return rc;
}

static int ov2720_sensor_release(void)
{
	mutex_lock(ov2720_s_ctrl.msm_sensor_mutex);
	gpio_set_value_cansleep(ov2720_s_ctrl.sensordata->sensor_reset, 0);
	msleep(20);
	gpio_free(ov2720_s_ctrl.sensordata->sensor_reset);
	mutex_unlock(ov2720_s_ctrl.msm_sensor_mutex);
	pr_err("%s completed\n", __func__);
	return 0;
}

static const struct i2c_device_id ov2720_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&ov2720_s_ctrl},
	{ }
};

static struct i2c_driver ov2720_i2c_driver = {
	.id_table = ov2720_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static int ov2720_sensor_v4l2_probe(const struct msm_camera_sensor_info *info,
	struct v4l2_subdev *sdev, struct msm_sensor_ctrl *s)
{
	return msm_sensor_v4l2_probe(&ov2720_s_ctrl, info, sdev, s);
}

static int ov2720_probe(struct platform_device *pdev)
{
	return msm_sensor_register(pdev, ov2720_sensor_v4l2_probe);
}

struct platform_driver ov2720_driver = {
	.probe = ov2720_probe,
	.driver = {
		.name = PLATFORM_DRIVER_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init msm_sensor_init_module(void)
{
	return platform_driver_register(&ov2720_driver);
}

static struct v4l2_subdev_core_ops ov2720_subdev_core_ops;
static struct v4l2_subdev_video_ops ov2720_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops ov2720_subdev_ops = {
	.core = &ov2720_subdev_core_ops,
	.video  = &ov2720_subdev_video_ops,
};

static struct msm_sensor_ctrl_t ov2720_s_ctrl = {
	.msm_sensor_reg = {
		.start_stream_conf = ov2720_start_settings,
		.start_stream_conf_size = ARRAY_SIZE(ov2720_start_settings),
		.stop_stream_conf = ov2720_stop_settings,
		.stop_stream_conf_size = ARRAY_SIZE(ov2720_stop_settings),
		.group_hold_on_conf = ov2720_groupon_settings,
		.group_hold_on_conf_size = ARRAY_SIZE(ov2720_groupon_settings),
		.group_hold_off_conf = ov2720_groupoff_settings,
		.group_hold_off_conf_size =
			ARRAY_SIZE(ov2720_groupoff_settings),
		.init_settings = &ov2720_init_conf[0],
		.init_size = ARRAY_SIZE(ov2720_init_conf),
		.res_settings = &ov2720_confs[0],
		.num_conf = ARRAY_SIZE(ov2720_confs),
	},
	.sensor_id_addr = 0x300A,
	.sensor_id = 0x2720,
	.frame_length_lines_addr = 0x380e,
	.coarse_int_time_addr = 0x3501,
	.global_gain_addr = 0x3508,
	.line_length_pck_addr = 0x380c,
	.frame_length_lines_array_addr = 14,
	.line_length_pck_array_addr = 12,
	.vert_offset = 6,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.camera_type = FRONT_CAMERA_2D,
	.config_csi_flag = 1,
	.csi_params = &ov2720_csi_params,
	.msm_sensor_mutex = &ov2720_mut,
	.msm_sensor_i2c_driver = &ov2720_i2c_driver,
	.sensor_v4l2_subdev_info = ov2720_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(ov2720_subdev_info),
	.sensor_v4l2_subdev_ops = &ov2720_subdev_ops,

	.func_tbl = {
		.sensor_start_stream = msm_sensor_start_stream,
		.sensor_stop_stream = msm_sensor_stop_stream,
		.sensor_group_hold_on = msm_sensor_group_hold_on,
		.sensor_group_hold_off = msm_sensor_group_hold_off,
		.sensor_get_prev_lines_pf = msm_sensor_get_prev_lines_pf,
		.sensor_get_prev_pixels_pl = msm_sensor_get_prev_pixels_pl,
		.sensor_get_pict_lines_pf = msm_sensor_get_pict_lines_pf,
		.sensor_get_pict_pixels_pl = msm_sensor_get_pict_pixels_pl,
		.sensor_get_pict_max_exp_lc = msm_sensor_get_pict_max_exp_lc,
		.sensor_get_pict_fps = msm_sensor_get_pict_fps,
		.sensor_set_fps = msm_sensor_set_fps,
		.sensor_write_exp_gain = ov2720_write_exp_gain,
		.sensor_setting = ov2720_sensor_setting,
		.sensor_set_sensor_mode = msm_sensor_set_sensor_mode_b,
		.sensor_mode_init = msm_sensor_mode_init_bdata,
		.sensor_config = ov2720_sensor_config,
		.sensor_open_init = ov2720_sensor_open_init,
		.sensor_release = ov2720_sensor_release,
		.sensor_power_up = ov2720_power_up,
		.sensor_power_down = ov2720_power_down,
		.sensor_probe = msm_sensor_probe,
	},
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Omnivision 2MP Bayer sensor driver");
MODULE_LICENSE("GPL v2");


