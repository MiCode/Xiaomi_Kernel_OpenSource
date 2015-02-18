/*
 * Support for OmniVision OV5648 5M camera sensor.
 * Based on OmniVision OV2722 driver.
 *
 * Copyright (c) 2013 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kmod.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/moduleparam.h>
#include <media/v4l2-device.h>
#include <linux/io.h>
#include <linux/atomisp_gmin_platform.h>

#include "ov5648.h"

#define OV5648_DEBUG_EN 0
#define ov5648_debug(...) // dev_err(__VA_ARGS__)

#define H_FLIP_DEFAULT 1
#define V_FLIP_DEFAULT 0
static int h_flag = H_FLIP_DEFAULT;
static int v_flag = V_FLIP_DEFAULT;

/* i2c read/write stuff */
static int ov5648_read_reg(struct i2c_client *client,
			   u16 data_length, u16 reg, u16 *val)
{
	int err;
	struct i2c_msg msg[2];
	unsigned char data[6];

	if (!client->adapter) {
		dev_err(&client->dev, "%s error, no client->adapter\n",
			__func__);
		return -ENODEV;
	}

	if (data_length != OV5648_8BIT && data_length != OV5648_16BIT
					&& data_length != OV5648_32BIT) {
		dev_err(&client->dev, "%s error, invalid data length\n",
			__func__);
		return -EINVAL;
	}

	memset(msg, 0 , sizeof(msg));

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = I2C_MSG_LENGTH;
	msg[0].buf = data;

	/* high byte goes out first */
	data[0] = (u8)(reg >> 8);
	data[1] = (u8)(reg & 0xff);

	msg[1].addr = client->addr;
	msg[1].len = data_length;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = data;

	err = i2c_transfer(client->adapter, msg, 2);
	if (err != 2) {
		if (err >= 0)
			err = -EIO;
		dev_err(&client->dev,
			"read from offset 0x%x error %d", reg, err);
		return err;
	}

	*val = 0;
	/* high byte comes first */
	if (data_length == OV5648_8BIT)
		*val = (u8)data[0];
	else if (data_length == OV5648_16BIT)
		*val = be16_to_cpu(*(u16 *)&data[0]);
	else
		*val = be32_to_cpu(*(u32 *)&data[0]);

	return 0;
}

static int ov5648_i2c_write(struct i2c_client *client, u16 len, u8 *data)
{
	struct i2c_msg msg;
	const int num_msg = 1;
	int ret;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = len;
	msg.buf = data;
	ret = i2c_transfer(client->adapter, &msg, 1);

	return ret == num_msg ? 0 : -EIO;
}

static int ov5648_write_reg(struct i2c_client *client, u16 data_length,
							u16 reg, u16 val)
{
	int ret;
	unsigned char data[4] = {0};
	u16 *wreg = (u16 *)data;
	const u16 len = data_length + sizeof(u16); /* 16-bit address + data */

	if (data_length != OV5648_8BIT && data_length != OV5648_16BIT) {
		dev_err(&client->dev,
			"%s error, invalid data_length\n", __func__);
		return -EINVAL;
	}

	/* high byte goes out first */
	*wreg = cpu_to_be16(reg);

	if (data_length == OV5648_8BIT) {
		data[2] = (u8)(val);
	} else {
		/* OV5648_16BIT */
		u16 *wdata = (u16 *)&data[2];
		*wdata = cpu_to_be16(val);
	}

	ret = ov5648_i2c_write(client, len, data);
	if (ret)
		dev_err(&client->dev,
			"write error: wrote 0x%x to offset 0x%x error %d",
			val, reg, ret);

	return ret;
}

/*
 * ov5648_write_reg_array - Initializes a list of OV5648 registers
 * @client: i2c driver client structure
 * @reglist: list of registers to be written
 *
 * This function initializes a list of registers. When consecutive addresses
 * are found in a row on the list, this function creates a buffer and sends
 * consecutive data in a single i2c_transfer().
 *
 * __ov5648_flush_reg_array, __ov5648_buf_reg_array() and
 * __ov5648_write_reg_is_consecutive() are internal functions to
 * ov5648_write_reg_array_fast() and should be not used anywhere else.
 *
 */

static int __ov5648_flush_reg_array(struct i2c_client *client,
				    struct ov5648_write_ctrl *ctrl)
{
	u16 size;

	if (ctrl->index == 0)
		return 0;

	size = sizeof(u16) + ctrl->index; /* 16-bit address + data */
	ctrl->buffer.addr = cpu_to_be16(ctrl->buffer.addr);
	ctrl->index = 0;

	return ov5648_i2c_write(client, size, (u8 *)&ctrl->buffer);
}

static int __ov5648_buf_reg_array(struct i2c_client *client,
				  struct ov5648_write_ctrl *ctrl,
				  const struct ov5648_reg *next)
{
	int size;
	u16 *data16;

	switch (next->type) {
	case OV5648_8BIT:
		size = 1;
		ctrl->buffer.data[ctrl->index] = (u8)next->val;
		break;
	case OV5648_16BIT:
		size = 2;
		data16 = (u16 *)&ctrl->buffer.data[ctrl->index];
		*data16 = cpu_to_be16((u16)next->val);
		break;
	default:
		return -EINVAL;
	}

	/* When first item is added, we need to store its starting address */
	if (ctrl->index == 0)
		ctrl->buffer.addr = next->reg;

	ctrl->index += size;

	/*
	 * Buffer cannot guarantee free space for u32? Better flush it to avoid
	 * possible lack of memory for next item.
	 */
	if (ctrl->index + sizeof(u16) >= OV5648_MAX_WRITE_BUF_SIZE)
		return __ov5648_flush_reg_array(client, ctrl);

	return 0;
}

static int __ov5648_write_reg_is_consecutive(struct i2c_client *client,
					     struct ov5648_write_ctrl *ctrl,
					     const struct ov5648_reg *next)
{
	if (ctrl->index == 0)
		return 1;

	return ctrl->buffer.addr + ctrl->index == next->reg;
}

static int ov5648_write_reg_array(struct i2c_client *client,
				  const struct ov5648_reg *reglist)
{
	const struct ov5648_reg *next = reglist;
	struct ov5648_write_ctrl ctrl;
	int err;

	ctrl.index = 0;
	for (; next->type != OV5648_TOK_TERM; next++) {
		switch (next->type & OV5648_TOK_MASK) {
		case OV5648_TOK_DELAY:
			err = __ov5648_flush_reg_array(client, &ctrl);
			if (err)
				return err;
			msleep(next->val);
			break;
		default:
			/*
			 * If next address is not consecutive, data needs to be
			 * flushed before proceed.
			 */
			if (!__ov5648_write_reg_is_consecutive(client, &ctrl,
								next)) {
				err = __ov5648_flush_reg_array(client, &ctrl);
			if (err)
				return err;
			}
			err = __ov5648_buf_reg_array(client, &ctrl, next);
			if (err) {
				dev_err(&client->dev, "%s: write error, aborted\n",
					 __func__);
				return err;
			}
			break;
		}
	}

	return __ov5648_flush_reg_array(client, &ctrl);
}
static int ov5648_g_focal(struct v4l2_subdev *sd, s32 *val)
{
	*val = (OV5648_FOCAL_LENGTH_NUM << 16) | OV5648_FOCAL_LENGTH_DEM;
	return 0;
}

static int ov5648_g_fnumber(struct v4l2_subdev *sd, s32 *val)
{
	/*const f number for imx*/
	*val = (OV5648_F_NUMBER_DEFAULT_NUM << 16) | OV5648_F_NUMBER_DEM;
	return 0;
}

static int ov5648_g_fnumber_range(struct v4l2_subdev *sd, s32 *val)
{
	*val = (OV5648_F_NUMBER_DEFAULT_NUM << 24) |
		(OV5648_F_NUMBER_DEM << 16) |
		(OV5648_F_NUMBER_DEFAULT_NUM << 8) | OV5648_F_NUMBER_DEM;
	return 0;
}

static int ov5648_g_bin_factor_x(struct v4l2_subdev *sd, s32 *val)
{
	struct ov5648_device *dev = to_ov5648_sensor(sd);

	*val = ov5648_res[dev->fmt_idx].bin_factor_x;

	return 0;
}

static int ov5648_g_bin_factor_y(struct v4l2_subdev *sd, s32 *val)
{
	struct ov5648_device *dev = to_ov5648_sensor(sd);

	*val = ov5648_res[dev->fmt_idx].bin_factor_y;

	return 0;
}

static int ov5648_get_intg_factor(struct i2c_client *client,
				struct camera_mipi_info *info,
				const struct ov5648_resolution *res)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov5648_device *dev = to_ov5648_sensor(sd);
	struct atomisp_sensor_mode_data *buf = &info->data;
	unsigned int pix_clk_freq_hz;
	u16 reg_val;
	int ret;

	if (info == NULL)
		return -EINVAL;

	/* pixel clock */
	pix_clk_freq_hz = res->pix_clk_freq * 1000000;

	dev->vt_pix_clk_freq_mhz = pix_clk_freq_hz;
	buf->vt_pix_clk_freq_mhz = pix_clk_freq_hz;

	/* get integration time */
	buf->coarse_integration_time_min = OV5648_COARSE_INTG_TIME_MIN;
	buf->coarse_integration_time_max_margin =
		OV5648_COARSE_INTG_TIME_MAX_MARGIN;

	buf->fine_integration_time_min = OV5648_FINE_INTG_TIME_MIN;
	buf->fine_integration_time_max_margin =
		OV5648_FINE_INTG_TIME_MAX_MARGIN;

	buf->fine_integration_time_def = OV5648_FINE_INTG_TIME_MIN;
	buf->frame_length_lines = res->lines_per_frame;
	buf->line_length_pck = res->pixels_per_line;
	buf->read_mode = res->bin_mode;

	/* get the cropping and output resolution to ISP for this mode. */
	ret =  ov5648_read_reg(client, OV5648_16BIT,
		OV5648_HORIZONTAL_START_H, &reg_val);
	if (ret)
		return ret;
	buf->crop_horizontal_start = reg_val;

	ret =  ov5648_read_reg(client, OV5648_16BIT,
		OV5648_VERTICAL_START_H, &reg_val);
	if (ret)
		return ret;
	buf->crop_vertical_start = reg_val;

	ret = ov5648_read_reg(client, OV5648_16BIT,
		OV5648_HORIZONTAL_END_H, &reg_val);
	if (ret)
		return ret;
	buf->crop_horizontal_end = reg_val;

	ret = ov5648_read_reg(client, OV5648_16BIT,
		OV5648_VERTICAL_END_H, &reg_val);
	if (ret)
		return ret;
	buf->crop_vertical_end = reg_val;

	ret = ov5648_read_reg(client, OV5648_16BIT,
		OV5648_HORIZONTAL_OUTPUT_SIZE_H, &reg_val);
	if (ret)
		return ret;
	buf->output_width = reg_val;

	ret = ov5648_read_reg(client, OV5648_16BIT,
		OV5648_VERTICAL_OUTPUT_SIZE_H, &reg_val);
	if (ret)
		return ret;
	buf->output_height = reg_val;

	buf->binning_factor_x = res->bin_factor_x ?
		res->bin_factor_x : 1;
	buf->binning_factor_y = res->bin_factor_y ?
		res->bin_factor_y : 1;
	return 0;
}

static long __ov5648_set_exposure(struct v4l2_subdev *sd, int coarse_itg,
				 int gain, int digitgain)

{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5648_device *dev = to_ov5648_sensor(sd);
	u16 vts, hts;
	int ret, exp_val, vts_val;
	int temp;

	if (dev->run_mode == CI_MODE_VIDEO)
		ov5648_res = ov5648_res_video;
	else if (dev->run_mode == CI_MODE_STILL_CAPTURE)
		ov5648_res = ov5648_res_still;
	else
		ov5648_res = ov5648_res_preview;


	hts = ov5648_res[dev->fmt_idx].pixels_per_line;
	vts = ov5648_res[dev->fmt_idx].lines_per_frame;

	/* group hold */
	ret = ov5648_write_reg(client, OV5648_8BIT,
		OV5648_GROUP_ACCESS, 0x00);
	if (ret)
		return ret;

	/* Increase the VTS to match exposure + 4 */
	if (coarse_itg + OV5648_INTEGRATION_TIME_MARGIN > vts)
		vts_val = coarse_itg + OV5648_INTEGRATION_TIME_MARGIN;
	else
		vts_val = vts;
	{
		ret = ov5648_write_reg(client, OV5648_8BIT,
			OV5648_TIMING_VTS_H, (vts_val >> 8) & 0xFF);
		if (ret)
			return ret;
		ret = ov5648_write_reg(client, OV5648_8BIT,
			OV5648_TIMING_VTS_L, vts_val & 0xFF);
		if (ret)
			return ret;
	}

	/* set exposure */
	/* Lower four bit should be 0*/
	exp_val = coarse_itg << 4;

	ret = ov5648_write_reg(client, OV5648_8BIT,
		OV5648_EXPOSURE_L, exp_val & 0xFF);
	if (ret)
		return ret;

	ret = ov5648_write_reg(client, OV5648_8BIT,
		OV5648_EXPOSURE_M, (exp_val >> 8) & 0xFF);
	if (ret)
		return ret;

	ret = ov5648_write_reg(client, OV5648_8BIT,
		OV5648_EXPOSURE_H, (exp_val >> 16) & 0x0F);
	if (ret)
		return ret;

	/* Digital gain */
	if (digitgain != dev->pre_digitgain){
		dev->pre_digitgain = digitgain;
		temp = digitgain*(dev->current_otp.R_gain)>>10;
		if (temp >= 0x400){
			ret = ov5648_write_reg(client, OV5648_16BIT,
				OV5648_MWB_RED_GAIN_H, temp);
			if (ret)
				return ret;
		}

		temp = digitgain*(dev->current_otp.G_gain)>>10;
		if (temp >= 0x400){
			ret = ov5648_write_reg(client, OV5648_16BIT,
				OV5648_MWB_GREEN_GAIN_H, temp);
			if (ret)
				return ret;
		}

		temp = digitgain*(dev->current_otp.B_gain)>>10;
		if (temp >= 0x400){
			ret = ov5648_write_reg(client, OV5648_16BIT,
				OV5648_MWB_BLUE_GAIN_H, temp);
			if (ret)
				return ret;
		}
	}

	/* Analog gain */
	ret = ov5648_write_reg(client, OV5648_8BIT,
		OV5648_AGC_L, gain & 0xff);
	if (ret)
		return ret;

	ret = ov5648_write_reg(client, OV5648_8BIT,
		OV5648_AGC_H, (gain >> 8) & 0xff);
	if (ret)
		return ret;

	/* End group */
	ret = ov5648_write_reg(client, OV5648_8BIT,
		OV5648_GROUP_ACCESS, 0x10);
	if (ret)
		return ret;

	/* Delay launch group */
	ret = ov5648_write_reg(client, OV5648_8BIT,
		OV5648_GROUP_ACCESS, 0xa0);
	if (ret)
		return ret;

	return ret;
}

static int ov5648_set_exposure(struct v4l2_subdev *sd, int exposure,
	int gain, int digitgain)
{
	struct ov5648_device *dev = to_ov5648_sensor(sd);
	int ret;

	mutex_lock(&dev->input_lock);
	ret = __ov5648_set_exposure(sd, exposure, gain, digitgain);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static long ov5648_s_exposure(struct v4l2_subdev *sd,
			       struct atomisp_exposure *exposure)
{
	int exp = exposure->integration_time[0];
	int gain = exposure->gain[0];
	int digitgain = exposure->gain[1];

	/* we should not accept the invalid value below. */
	if (gain == 0) {
		struct i2c_client *client = v4l2_get_subdevdata(sd);
		v4l2_err(client, "%s: invalid value\n", __func__);
		return -EINVAL;
	}

	// EXPOSURE CONTROL DISABLED FOR INITIAL CHECKIN, TUNING DOESN'T WORK
	return ov5648_set_exposure(sd, exp, gain, digitgain);
}

static long ov5648_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{

	switch (cmd) {
	case ATOMISP_IOC_S_EXPOSURE:
		return ov5648_s_exposure(sd, arg);
	default:
		return -EINVAL;
	}
	return 0;
}

/* This returns the exposure time being used. This should only be used
   for filling in EXIF data, not for actual image processing. */
static int ov5648_q_exposure(struct v4l2_subdev *sd, s32 *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u16 reg_v, reg_v2;
	int ret;

	/* get exposure */
	ret = ov5648_read_reg(client, OV5648_8BIT,
					OV5648_EXPOSURE_L,
					&reg_v);
	if (ret)
		goto err;

	ret = ov5648_read_reg(client, OV5648_8BIT,
					OV5648_EXPOSURE_M,
					&reg_v2);
	if (ret)
		goto err;

	reg_v += reg_v2 << 8;
	ret = ov5648_read_reg(client, OV5648_8BIT,
					OV5648_EXPOSURE_H,
					&reg_v2);
	if (ret)
		goto err;

	*value = reg_v + (((u32)reg_v2 << 16));
err:
	return ret;
}

static int ov5648_vcm_power_up(struct v4l2_subdev *sd)
{
	struct ov5648_device *dev = to_ov5648_sensor(sd);
	struct camera_sensor_platform_data *pdata = dev->platform_data;
	struct camera_vcm_control *vcm;

	if (!dev->vcm_driver)
		if (pdata && pdata->get_vcm_ctrl)
			dev->vcm_driver =
				pdata->get_vcm_ctrl(&dev->sd,
						dev->camera_module);

	vcm = dev->vcm_driver;
	if (vcm && vcm->ops && vcm->ops->power_up)
		return vcm->ops->power_up(sd, vcm);

	return 0;
}

static int ov5648_vcm_power_down(struct v4l2_subdev *sd)
{
	struct ov5648_device *dev = to_ov5648_sensor(sd);
	struct camera_vcm_control *vcm = dev->vcm_driver;

	if (vcm && vcm->ops && vcm->ops->power_down)
		return vcm->ops->power_down(sd, vcm);

	return 0;
}

static int ov5648_v_flip(struct v4l2_subdev *sd, s32 value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	u16 val;

	ov5648_debug(&client->dev, "@%s: value:%d\n", __func__, value);
	ret = ov5648_read_reg(client, OV5648_8BIT, OV5648_VFLIP_REG, &val);
	if (ret)
		return ret;
	if (value) {
		val |= OV5648_VFLIP_VALUE;
	} else {
		val &= ~OV5648_VFLIP_VALUE;
	}
	ret = ov5648_write_reg(client, OV5648_8BIT,
			OV5648_VFLIP_REG, val);
	if (ret)
		return ret;
	return ret;
}

static int ov5648_h_flip(struct v4l2_subdev *sd, s32 value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	u16 val;
	ov5648_debug(&client->dev, "@%s: value:%d\n", __func__, value);

	ret = ov5648_read_reg(client, OV5648_8BIT, OV5648_HFLIP_REG, &val);
	if (ret)
		return ret;
	if (value) {
		val |= OV5648_HFLIP_VALUE;
	} else {
		val &= ~OV5648_HFLIP_VALUE;
	}
	ret = ov5648_write_reg(client, OV5648_8BIT,
			OV5648_HFLIP_REG, val);
	if (ret)
		return ret;
	return ret;
}


struct ov5648_control ov5648_controls[] = {
	{
		.qc = {
			.id = V4L2_CID_EXPOSURE_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "exposure",
			.minimum = 0x0,
			.maximum = 0xffff,
			.step = 0x01,
			.default_value = 0x00,
			.flags = 0,
		},
		.query = ov5648_q_exposure,
	},
	{
		.qc = {
			.id = V4L2_CID_FOCAL_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focal length",
			.minimum = OV5648_FOCAL_LENGTH_DEFAULT,
			.maximum = OV5648_FOCAL_LENGTH_DEFAULT,
			.step = 0x01,
			.default_value = OV5648_FOCAL_LENGTH_DEFAULT,
			.flags = 0,
		},
		.query = ov5648_g_focal,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number",
			.minimum = OV5648_F_NUMBER_DEFAULT,
			.maximum = OV5648_F_NUMBER_DEFAULT,
			.step = 0x01,
			.default_value = OV5648_F_NUMBER_DEFAULT,
			.flags = 0,
		},
		.query = ov5648_g_fnumber,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_RANGE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number range",
			.minimum = OV5648_F_NUMBER_RANGE,
			.maximum =  OV5648_F_NUMBER_RANGE,
			.step = 0x01,
			.default_value = OV5648_F_NUMBER_RANGE,
			.flags = 0,
		},
		.query = ov5648_g_fnumber_range,
	},
	{
		.qc = {
			.id = V4L2_CID_BIN_FACTOR_HORZ,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "horizontal binning factor",
			.minimum = 0,
			.maximum = OV5648_BIN_FACTOR_MAX,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.query = ov5648_g_bin_factor_x,
	},
	{
		.qc = {
			.id = V4L2_CID_BIN_FACTOR_VERT,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "vertical binning factor",
			.minimum = 0,
			.maximum = OV5648_BIN_FACTOR_MAX,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.query = ov5648_g_bin_factor_y,
	},
	{
		.qc = {
			.id = V4L2_CID_VFLIP,
			.type = V4L2_CTRL_TYPE_BOOLEAN,
			.name = "Flip",
			.minimum = 0,
			.maximum = 1,
			.step = 1,
			.default_value = 0,
		},
		.tweak = ov5648_v_flip,
	},
	{
		.qc = {
			.id = V4L2_CID_HFLIP,
			.type = V4L2_CTRL_TYPE_BOOLEAN,
			.name = "Mirror",
			.minimum = 0,
			.maximum = 1,
			.step = 1,
			.default_value = 0,
		},
		.tweak = ov5648_h_flip,
	},
};
#define N_CONTROLS (ARRAY_SIZE(ov5648_controls))

static struct ov5648_control *ov5648_find_control(u32 id)
{
	int i;

	for (i = 0; i < N_CONTROLS; i++)
		if (ov5648_controls[i].qc.id == id)
			return &ov5648_controls[i];
	return NULL;
}

static int ov5648_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	struct ov5648_control *ctrl = ov5648_find_control(qc->id);
	struct ov5648_device *dev = to_ov5648_sensor(sd);
	struct camera_vcm_control *vcm = dev->vcm_driver;

	if (ctrl == NULL) {
		if (vcm && vcm->ops && vcm->ops->queryctrl)
			return vcm->ops->queryctrl(sd, qc, vcm);

		return -EINVAL;
	}

	mutex_lock(&dev->input_lock);
	*qc = ctrl->qc;
	mutex_unlock(&dev->input_lock);

	return 0;
}

/* imx control set/get */
static int ov5648_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct ov5648_control *s_ctrl;
	struct ov5648_device *dev = to_ov5648_sensor(sd);
	struct camera_vcm_control *vcm = dev->vcm_driver;
	int ret;

	if (!ctrl)
		return -EINVAL;

	s_ctrl = ov5648_find_control(ctrl->id);
	if (s_ctrl == NULL) {
		if (vcm && vcm->ops && vcm->ops->g_ctrl)
			return vcm->ops->g_ctrl(sd, ctrl, vcm);

		return -EINVAL;
	}

	if (s_ctrl->query == NULL)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	ret = s_ctrl->query(sd, &ctrl->value);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int ov5648_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct ov5648_control *octrl = ov5648_find_control(ctrl->id);
	struct ov5648_device *dev = to_ov5648_sensor(sd);
	struct camera_vcm_control *vcm = dev->vcm_driver;
	int ret;

	if (octrl == NULL) {
		if (vcm && vcm->ops && vcm->ops->g_ctrl)
			return vcm->ops->s_ctrl(sd, ctrl, vcm);

		return -EINVAL;
	}

	if (octrl->tweak == NULL)
		return -EINVAL;

	switch(ctrl->id)
	{
		case V4L2_CID_VFLIP:
			if(ctrl->value)
				v_flag=1;
			else
				v_flag=0;
			break;
		case V4L2_CID_HFLIP:
			if(ctrl->value)
				h_flag=1;
			else
				h_flag=0;
			break;
		default:break;
	};

	mutex_lock(&dev->input_lock);
	ret = octrl->tweak(sd, ctrl->value);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int ov5648_init(struct v4l2_subdev *sd)
{
	struct ov5648_device *dev = to_ov5648_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	mutex_lock(&dev->input_lock);

	/* restore settings */
	ov5648_res = ov5648_res_preview;
	N_RES = N_RES_PREVIEW;

	ret = ov5648_write_reg_array(client, ov5648_global_settings);
	if (ret) {
		dev_err(&client->dev, "ov5648 write global settings err.\n");
		mutex_unlock(&dev->input_lock);
		return ret;
	}

	mutex_unlock(&dev->input_lock);

	return 0;
}

#if 1
/*
 *Camera driver need to load AWB calibration data
 *stored in OTP and write to gain registers after
 *initialization of register settings.
 * index: index of otp group. (1, 2, 3)
 * return: 0, group index is empty
 *		1, group index has invalid data
 *		2, group index has valid data
 */
static int check_otp(struct i2c_client *client, int index)
{
	int i;
	u16 flag = 0, rg = 0, bg = 0;
	if (index == 1) {
		/* read otp --Bank 0 */
		ov5648_write_reg(client, OV5648_8BIT, 0x3d84, 0xc0);
		ov5648_write_reg(client, OV5648_8BIT, 0x3d85, 0x00);
		ov5648_write_reg(client, OV5648_8BIT, 0x3d86, 0x0f);
		ov5648_write_reg(client, OV5648_8BIT, 0x3d81, 0x01);
		mdelay(5);
		ov5648_read_reg(client, OV5648_8BIT, 0x3d05, &flag);
		ov5648_read_reg(client, OV5648_8BIT, 0x3d07, &rg);
		ov5648_read_reg(client, OV5648_8BIT, 0x3d08, &bg);
	} else if (index == 2) {
		/* read otp --Bank 0 */
		ov5648_write_reg(client, OV5648_8BIT, 0x3d84, 0xc0);
		ov5648_write_reg(client, OV5648_8BIT, 0x3d85, 0x00);
		ov5648_write_reg(client, OV5648_8BIT, 0x3d86, 0x0f);
		ov5648_write_reg(client, OV5648_8BIT, 0x3d81, 0x01);
		mdelay(5);
		ov5648_read_reg(client, OV5648_8BIT, 0x3d0e, &flag);

		/* read otp --Bank 1 */
		ov5648_write_reg(client, OV5648_8BIT, 0x3d84, 0xc0);
		ov5648_write_reg(client, OV5648_8BIT, 0x3d85, 0x10);
		ov5648_write_reg(client, OV5648_8BIT, 0x3d86, 0x1f);
		ov5648_write_reg(client, OV5648_8BIT, 0x3d81, 0x01);
		mdelay(5);
		ov5648_read_reg(client, OV5648_8BIT, 0x3d00, &rg);
		ov5648_read_reg(client, OV5648_8BIT, 0x3d01, &bg);
	} else if (index == 3) {
		/* read otp --Bank 1 */
		ov5648_write_reg(client, OV5648_8BIT, 0x3d84, 0xc0);
		ov5648_write_reg(client, OV5648_8BIT, 0x3d85, 0x10);
		ov5648_write_reg(client, OV5648_8BIT, 0x3d86, 0x1f);
		ov5648_write_reg(client, OV5648_8BIT, 0x3d81, 0x01);
		mdelay(5);
		ov5648_read_reg(client, OV5648_8BIT, 0x3d07, &flag);
		ov5648_read_reg(client, OV5648_8BIT, 0x3d09, &rg);
		ov5648_read_reg(client, OV5648_8BIT, 0x3d0a, &bg);
	}

	flag = flag & 0x80;

	/* clear otp buffer */
	for (i = 0; i < 16; i++)
		ov5648_write_reg(client, OV5648_8BIT, 0x3d00 + i, 0x00);

	if (flag)
		return 1;
	else {
		if (rg == 0 && bg == 0)
			return 0;
		else
			return 2;
	}

}

/* index: index of otp group. (1, 2, 3)
 * return: 0,
 */
static int read_otp(struct i2c_client *client,
	    int index, struct otp_struct *otp_ptr)
{
	int i;
	u16 temp;
	/* read otp into buffer */
	if (index == 1) {
		/* read otp --Bank 0 */
		ov5648_write_reg(client, OV5648_8BIT, 0x3d84, 0xc0);
		ov5648_write_reg(client, OV5648_8BIT, 0x3d85, 0x00);
		ov5648_write_reg(client, OV5648_8BIT, 0x3d86, 0x0f);
		ov5648_write_reg(client, OV5648_8BIT, 0x3d81, 0x01);
		mdelay(5);
		ov5648_read_reg(client, OV5648_8BIT,
			0x3d05, &((*otp_ptr).module_integrator_id));
		(*otp_ptr).module_integrator_id =
			(*otp_ptr).module_integrator_id & 0x7f;
		ov5648_read_reg(client, OV5648_8BIT,
			0x3d06, &((*otp_ptr).lens_id));
		ov5648_read_reg(client, OV5648_8BIT, 0x3d0b, &temp);
		ov5648_read_reg(client, OV5648_8BIT,
			0x3d07, &((*otp_ptr).rg_ratio));
		(*otp_ptr).rg_ratio =
			((*otp_ptr).rg_ratio<<2) + ((temp>>6) & 0x03);
		ov5648_read_reg(client, OV5648_8BIT,
			0x3d08, &((*otp_ptr).bg_ratio));
		(*otp_ptr).bg_ratio =
			((*otp_ptr).bg_ratio<<2) + ((temp>>4) & 0x03);
		ov5648_read_reg(client, OV5648_8BIT,
			0x3d0c, &((*otp_ptr).light_rg));
		(*otp_ptr).light_rg =
			((*otp_ptr).light_rg<<2) + ((temp>>2) & 0x03);
		ov5648_read_reg(client, OV5648_8BIT,
			0x3d0d, &((*otp_ptr).light_bg));
		(*otp_ptr).light_bg =
			((*otp_ptr).light_bg<<2) + (temp & 0x03);
		ov5648_read_reg(client, OV5648_8BIT,
			0x3d09, &((*otp_ptr).user_data[0]));
		ov5648_read_reg(client, OV5648_8BIT,
			0x3d0a, &((*otp_ptr).user_data[1]));
	} else if (index == 2) {
		/* read otp --Bank 0 */
		ov5648_write_reg(client, OV5648_8BIT, 0x3d84, 0xc0);
		ov5648_write_reg(client, OV5648_8BIT, 0x3d85, 0x00);
		ov5648_write_reg(client, OV5648_8BIT, 0x3d86, 0x0f);
		ov5648_write_reg(client, OV5648_8BIT, 0x3d81, 0x01);
		mdelay(5);
		ov5648_read_reg(client, OV5648_8BIT,
			0x3d0e, &((*otp_ptr).module_integrator_id));
		(*otp_ptr).module_integrator_id =
			(*otp_ptr).module_integrator_id & 0x7f;
		ov5648_read_reg(client, OV5648_8BIT,
			0x3d0f, &((*otp_ptr).lens_id));
		/* read otp --Bank 1 */
		ov5648_write_reg(client, OV5648_8BIT, 0x3d84, 0xc0);
		ov5648_write_reg(client, OV5648_8BIT, 0x3d85, 0x10);
		ov5648_write_reg(client, OV5648_8BIT, 0x3d86, 0x1f);
		ov5648_write_reg(client, OV5648_8BIT, 0x3d81, 0x01);
		mdelay(5);
		ov5648_read_reg(client, OV5648_8BIT, 0x3d04, &temp);
		ov5648_read_reg(client, OV5648_8BIT,
			0x3d00, &((*otp_ptr).rg_ratio));
		(*otp_ptr).rg_ratio =
			((*otp_ptr).rg_ratio<<2) + ((temp>>6) & 0x03);
		ov5648_read_reg(client, OV5648_8BIT,
			0x3d01, &((*otp_ptr).bg_ratio));
		(*otp_ptr).bg_ratio =
			((*otp_ptr).bg_ratio<<2) + ((temp>>4) & 0x03);
		ov5648_read_reg(client, OV5648_8BIT,
			0x3d05, &((*otp_ptr).light_rg));
		(*otp_ptr).light_rg =
			((*otp_ptr).light_rg<<2) + ((temp>>2) & 0x03);
		ov5648_read_reg(client, OV5648_8BIT,
			0x3d06, &((*otp_ptr).light_bg));
		(*otp_ptr).light_bg =
			((*otp_ptr).light_bg<<2) + (temp & 0x03);
		ov5648_read_reg(client, OV5648_8BIT,
			0x3d02, &((*otp_ptr).user_data[0]));
		ov5648_read_reg(client, OV5648_8BIT,
			0x3d03, &((*otp_ptr).user_data[1]));
	} else if (index == 3) {
		/* read otp --Bank 1 */
		ov5648_write_reg(client, OV5648_8BIT, 0x3d84, 0xc0);
		ov5648_write_reg(client, OV5648_8BIT, 0x3d85, 0x10);
		ov5648_write_reg(client, OV5648_8BIT, 0x3d86, 0x1f);
		ov5648_write_reg(client, OV5648_8BIT, 0x3d81, 0x01);
		mdelay(5);
		ov5648_read_reg(client, OV5648_8BIT,
			0x3d07, &((*otp_ptr).module_integrator_id));
		(*otp_ptr).module_integrator_id =
			(*otp_ptr).module_integrator_id & 0x7f;
		ov5648_read_reg(client, OV5648_8BIT,
			0x3d08, &((*otp_ptr).lens_id));
		ov5648_read_reg(client, OV5648_8BIT, 0x3d0d, &temp);
		ov5648_read_reg(client, OV5648_8BIT,
			0x3d09, &((*otp_ptr).rg_ratio));
		(*otp_ptr).rg_ratio =
			((*otp_ptr).rg_ratio<<2) + ((temp>>6) & 0x03);
		ov5648_read_reg(client, OV5648_8BIT,
			0x3d0a, &((*otp_ptr).bg_ratio));
		(*otp_ptr).bg_ratio =
			((*otp_ptr).bg_ratio<<2) + ((temp>>4) & 0x03);
		ov5648_read_reg(client, OV5648_8BIT,
			0x3d0e, &((*otp_ptr).light_rg));
		(*otp_ptr).light_rg =
			((*otp_ptr).light_rg<<2) + ((temp>>2) & 0x03);
		ov5648_read_reg(client, OV5648_8BIT,
			0x3d0f, &((*otp_ptr).light_bg));
		(*otp_ptr).light_bg =
			((*otp_ptr).light_bg<<2) + (temp & 0x03);
		ov5648_read_reg(client, OV5648_8BIT,
			0x3d0b, &((*otp_ptr).user_data[0]));
		ov5648_read_reg(client, OV5648_8BIT,
			0x3d0c, &((*otp_ptr).user_data[1]));
	}
	/* clear otp buffer */
	for (i = 0; i < 16; i++)
		ov5648_write_reg(client, OV5648_8BIT, 0x3d00 + i, 0x00);

	return 0;
}
/* R_gain, sensor red gain of AWB, 0x400 =1
 * G_gain, sensor green gain of AWB, 0x400 =1
 * B_gain, sensor blue gain of AWB, 0x400 =1
 * return 0;
 */
static int update_awb_gain(struct v4l2_subdev *sd)
{

	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5648_device *dev = to_ov5648_sensor(sd);
	int R_gain = dev->current_otp.R_gain;
	int G_gain = dev->current_otp.G_gain;
	int B_gain = dev->current_otp.B_gain;
	if (R_gain > 0x400) {
		ov5648_write_reg(client, OV5648_8BIT, 0x5186, R_gain>>8);
		ov5648_write_reg(client, OV5648_8BIT, 0x5187, R_gain & 0x00ff);
	}
	if (G_gain > 0x400) {
		ov5648_write_reg(client, OV5648_8BIT, 0x5188, G_gain>>8);
		ov5648_write_reg(client, OV5648_8BIT, 0x5189, G_gain & 0x00ff);
	}
	if (B_gain > 0x400) {
		ov5648_write_reg(client, OV5648_8BIT, 0x518a, B_gain>>8);
		ov5648_write_reg(client, OV5648_8BIT, 0x518b, B_gain & 0x00ff);
	}
	#ifdef OV5648_DEBUG_EN
	ov5648_debug(&client->dev, "_ov5648_: %s :rgain:%x ggain %x bgain %x\n",__func__,R_gain,G_gain,B_gain);
	#endif
	return 0;
}

/* call this function after OV5648 initialization
 * return: 0 update success
 *		1, no OTP
 */
static int update_otp(struct v4l2_subdev *sd)
{
	struct otp_struct current_otp;
	struct ov5648_device *dev = to_ov5648_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int i, ret;
	int otp_index;
	u16 temp;
	int R_gain, G_gain, B_gain, G_gain_R, G_gain_B;
	u16 rg = 1, bg = 1;

	//otp valid after mipi on and sw stream on
	ov5648_write_reg(client, OV5648_8BIT, OV5648_SW_STREAM, OV5648_START_STREAMING);

	/* R/G and B/G of current camera module is read out from sensor OTP
	 * check first OTP with valid data
	 */
	for (i = 1; i <= 3; i++) {
		temp = check_otp(client, i);
		if (temp == 2) {
			otp_index = i;
			break;
		}
	}
	if (i > 3) {
		printk(KERN_INFO"@%s: no valid wb otp data\n", __func__);
		/* no valid wb OTP data */
		return 1;
	}
	read_otp(client, otp_index, &current_otp);
	if (current_otp.light_rg == 0) {
		/* no light source information in OTP */
		rg = current_otp.rg_ratio;
	} else {
		/* light source information found in OTP */
		rg = current_otp.rg_ratio * (current_otp.light_rg + 512) / 1024;
	}
	if (current_otp.light_bg == 0) {
		/* no light source information in OTP */
		bg = current_otp.bg_ratio;
	} else {
		/* light source information found in OTP */
		bg = current_otp.bg_ratio * (current_otp.light_bg + 512) / 1024;
	}
	#ifdef OV5648_DEBUG_EN
	ov5648_debug(&client->dev, "_ov5648_: %s :rg:%x bg %x\n",__func__,rg,bg);
	#endif
	if(rg == 0)
		rg = 1;
	if(bg == 0)
		bg = 1;
	/*calculate G gain
	 *0x400 = 1x gain
	 */
	if (bg < BG_Ratio_Typical) {
		if (rg < RG_Ratio_Typical) {
			/* current_otp.bg_ratio < BG_Ratio_typical &&
			 * current_otp.rg_ratio < RG_Ratio_typical
			 */
			G_gain = 0x400;
			B_gain = 0x400 * BG_Ratio_Typical / bg;
			R_gain = 0x400 * RG_Ratio_Typical / rg;
		} else {
			/* current_otp.bg_ratio < BG_Ratio_typical &&
			 * current_otp.rg_ratio >= RG_Ratio_typical
			 */
			R_gain = 0x400;
			G_gain = 0x400 * rg / RG_Ratio_Typical;
			B_gain = G_gain * BG_Ratio_Typical / bg;
		}
	} else {
		if (rg < RG_Ratio_Typical) {
			/* current_otp.bg_ratio >= BG_Ratio_typical &&
			 * current_otp.rg_ratio < RG_Ratio_typical
			 */
			B_gain = 0x400;
			G_gain = 0x400 * bg / BG_Ratio_Typical;
			R_gain = G_gain * RG_Ratio_Typical / rg;
		} else {
			/* current_otp.bg_ratio >= BG_Ratio_typical &&
			 * current_otp.rg_ratio >= RG_Ratio_typical
			 */
			G_gain_B = 0x400 * bg / BG_Ratio_Typical;
			G_gain_R = 0x400 * rg / RG_Ratio_Typical;
			if (G_gain_B > G_gain_R) {
				B_gain = 0x400;
				G_gain = G_gain_B;
				R_gain = G_gain * RG_Ratio_Typical / rg;
			} else {
				R_gain = 0x400;
				G_gain = G_gain_R;
				B_gain = G_gain * BG_Ratio_Typical / bg;
			}
		}
	}

	dev->current_otp.R_gain = R_gain;
	dev->current_otp.G_gain = G_gain;
	dev->current_otp.B_gain = B_gain;

	ret = ov5648_write_reg(client,OV5648_8BIT,
		OV5648_SW_STREAM,OV5648_STOP_STREAMING);
	return ret ;
}

#endif

static int power_ctrl(struct v4l2_subdev *sd, bool flag)
{
	int ret = 0;
	struct ov5648_device *dev = to_ov5648_sensor(sd);
	if (!dev || !dev->platform_data)
		return -ENODEV;

	/* Non-gmin platforms use the legacy callback */
	if (dev->platform_data->power_ctrl)
		return dev->platform_data->power_ctrl(sd, flag);

	if (flag) {
		ret |= dev->platform_data->v1p8_ctrl(sd, 1);
		ret |= dev->platform_data->v2p8_ctrl(sd, 1);
		usleep_range(10000, 15000);
	}

	if (!flag || ret) {
		ret |= dev->platform_data->v1p8_ctrl(sd, 0);
		ret |= dev->platform_data->v2p8_ctrl(sd, 0);
	}
	return ret;
}

static int gpio_ctrl(struct v4l2_subdev *sd, bool flag)
{
	int ret;
	struct ov5648_device *dev = to_ov5648_sensor(sd);

	if (!dev || !dev->platform_data)
		return -ENODEV;

	/* Non-gmin platforms use the legacy callback */
	if (dev->platform_data->gpio_ctrl)
		return dev->platform_data->gpio_ctrl(sd, flag);

	/* GPIO0 == "RESETB", GPIO1 == "PWDNB", named in opposite
	 * senses but with the same behavior: both must be high for
	 * the device to opperate */
	if (flag) {
		ret = dev->platform_data->gpio0_ctrl(sd, 1);
		usleep_range(10000, 15000);
		ret |= dev->platform_data->gpio1_ctrl(sd, 1);
		usleep_range(10000, 15000);
	} else {
		ret = dev->platform_data->gpio1_ctrl(sd, 0);
		ret |= dev->platform_data->gpio0_ctrl(sd, 0);
	}
	return ret;
}

static int power_up(struct v4l2_subdev *sd)
{
	struct ov5648_device *dev = to_ov5648_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	dev_dbg(&client->dev, "@%s:\n", __func__);
	if (NULL == dev->platform_data) {
		dev_err(&client->dev,
			"no camera_sensor_platform_data");
		return -ENODEV;
	}

	/* power control */
	ret = power_ctrl(sd, 1);
	if (ret)
		goto fail_power;

	/* according to DS, at least 5ms is needed between DOVDD and PWDN */
	usleep_range(5000, 6000);

	/* gpio ctrl */
	ret = gpio_ctrl(sd, 1);
	if (ret) {
		ret = gpio_ctrl(sd, 1);
		if (ret)
			goto fail_power;
	}

	/* flis clock control */
	ret = dev->platform_data->flisclk_ctrl(sd, 1);
	if (ret)
		goto fail_clk;

	/* according to DS, 20ms is needed between PWDN and i2c access */
	msleep(20);

	return 0;

fail_clk:
	gpio_ctrl(sd, 0);
fail_power:
	power_ctrl(sd, 0);
	dev_err(&client->dev, "sensor power-up failed\n");

	return ret;
}

static int power_down(struct v4l2_subdev *sd)
{
	struct ov5648_device *dev = to_ov5648_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	h_flag = H_FLIP_DEFAULT;
	v_flag = V_FLIP_DEFAULT;
	dev_dbg(&client->dev, "@%s:\n", __func__);
	if (NULL == dev->platform_data) {
		dev_err(&client->dev,
			"no camera_sensor_platform_data");
		return -ENODEV;
	}

	ret = dev->platform_data->flisclk_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "flisclk failed\n");

	/* gpio ctrl */
	ret = gpio_ctrl(sd, 0);
	if (ret) {
		ret = gpio_ctrl(sd, 0);
		if (ret)
			dev_err(&client->dev, "gpio failed 2\n");
	}

	/* power control */
	ret = power_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "vprog failed.\n");

	return ret;
}

static int ov5648_s_power(struct v4l2_subdev *sd, int on)
{
	struct ov5648_device *dev = to_ov5648_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	dev_dbg(&client->dev, "@%s:\n", __func__);
	if (on == 0) {
		ret = power_down(sd);
		ret |= ov5648_vcm_power_down(sd);
	} else {
		ret = ov5648_vcm_power_up(sd);
		if (ret)
			return ret;

		ret |= power_up(sd);
		if (!ret)
			return ov5648_init(sd);
	}
	return ret;
}

/*
 * distance - calculate the distance
 * @res: resolution
 * @w: width
 * @h: height
 *
 * Get the gap between resolution and w/h.
 * res->width/height smaller than w/h wouldn't be considered.
 * Returns the value of gap or -1 if fail.
 */
#define LARGEST_ALLOWED_RATIO_MISMATCH 800
static int distance(struct ov5648_resolution *res, u32 w, u32 h)
{
	unsigned int w_ratio = ((res->width << 13) / w);
	unsigned int h_ratio;
	int match;

	if (h == 0)
		return -1;
	h_ratio = ((res->height << 13) / h);
	if (h_ratio == 0)
		return -1;
	match   = abs(((w_ratio << 13) / h_ratio) - ((int)8192));

	if ((w_ratio < (int)8192) || (h_ratio < (int)8192)  ||
		(match > LARGEST_ALLOWED_RATIO_MISMATCH))
		return -1;

	return w_ratio + h_ratio;
}

/* Return the nearest higher resolution index */
static int nearest_resolution_index(int w, int h)
{
	int i;
	int idx = -1;
	int dist;
	int min_dist = INT_MAX;
	struct ov5648_resolution *tmp_res = NULL;

	for (i = 0; i < N_RES; i++) {
		tmp_res = &ov5648_res[i];
		dist = distance(tmp_res, w, h);
		if (dist == -1)
			continue;
		if (dist < min_dist) {
			min_dist = dist;
			idx = i;
		}
	}

	return idx;
}

static int get_resolution_index(int w, int h)
{
	int i;

	for (i = 0; i < N_RES; i++) {
		if (w != ov5648_res[i].width)
			continue;
		if (h != ov5648_res[i].height)
			continue;

		return i;
	}

	return -1;
}

static int ov5648_try_mbus_fmt(struct v4l2_subdev *sd,
			struct v4l2_mbus_framefmt *fmt)
{
	int idx;

	if (!fmt)
		return -EINVAL;
	idx = nearest_resolution_index(fmt->width,
					fmt->height);
	if (idx == -1) {
		/* return the largest resolution */
		fmt->width = ov5648_res[0].width;
		fmt->height = ov5648_res[0].height;
	} else {
		fmt->width = ov5648_res[idx].width;
		fmt->height = ov5648_res[idx].height;
	}
	fmt->code = V4L2_MBUS_FMT_SGRBG10_1X10;

	return 0;
}

/* TODO: remove it. */
static int startup(struct v4l2_subdev *sd)
{
	struct ov5648_device *dev = to_ov5648_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;
	/*
	ret = ov5648_write_reg(client, OV5648_8BIT,
					OV5648_SW_RESET, 0x01);
	if (ret) {
		dev_err(&client->dev, "ov5648 reset err.\n");
		return ret;
	}*/
	ret = ov5648_write_reg_array(client, ov5648_res[dev->fmt_idx].regs);
	if (ret) {
		dev_err(&client->dev, "ov5648 write register err.\n");
		return ret;
	}
	if(dev->current_otp.otp_en == 1)
	{
		update_awb_gain(sd);
	}
	return ret;
}

static int ov5648_s_mbus_fmt(struct v4l2_subdev *sd,
			     struct v4l2_mbus_framefmt *fmt)
{
	struct ov5648_device *dev = to_ov5648_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camera_mipi_info *ov5648_info = NULL;
	int ret = 0;

	ov5648_info = v4l2_get_subdev_hostdata(sd);
	if (ov5648_info == NULL)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	ret = ov5648_try_mbus_fmt(sd, fmt);
	if (ret == -1) {
		dev_err(&client->dev, "try fmt fail\n");
		goto err;
	}
	dev->fmt_idx = get_resolution_index(fmt->width,
					      fmt->height);
	if (dev->fmt_idx == -1) {
		dev_err(&client->dev, "get resolution fail\n");
		mutex_unlock(&dev->input_lock);
		return -EINVAL;
	}

	ret = startup(sd);
	if (ret)
		dev_err(&client->dev, "ov5648 startup err\n");

	/*recall flip functions to avoid flip registers
	 * were overrided by default setting
	 */
	if (h_flag)
		ov5648_h_flip(sd, h_flag);
	if (v_flag)
		ov5648_v_flip(sd, v_flag);

	ret = ov5648_get_intg_factor(client, ov5648_info,
					&ov5648_res[dev->fmt_idx]);
	if (ret) {
		dev_err(&client->dev, "failed to get integration_factor\n");
		goto err;
	}

err:
	mutex_unlock(&dev->input_lock);
	return ret;
}
static int ov5648_g_mbus_fmt(struct v4l2_subdev *sd,
			     struct v4l2_mbus_framefmt *fmt)
{
	struct ov5648_device *dev = to_ov5648_sensor(sd);

	if (!fmt)
		return -EINVAL;

	fmt->width = ov5648_res[dev->fmt_idx].width;
	fmt->height = ov5648_res[dev->fmt_idx].height;
	fmt->code = V4L2_MBUS_FMT_SBGGR10_1X10;

	return 0;
}

static int ov5648_detect(struct i2c_client *client)
{
	struct i2c_adapter *adapter = client->adapter;
	u16 high, low;
	int ret;
	u16 id;
	u8 revision;

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -ENODEV;

	ret = ov5648_read_reg(client, OV5648_8BIT,
					OV5648_SC_CMMN_CHIP_ID_H, &high);
	if (ret) {
		dev_err(&client->dev, "sensor_id_high = 0x%x\n", high);
		return -ENODEV;
	}
	ret = ov5648_read_reg(client, OV5648_8BIT,
					OV5648_SC_CMMN_CHIP_ID_L, &low);
	id = ((((u16) high) << 8) | (u16) low);

	if (id != OV5648_ID) {
		dev_err(&client->dev, "sensor ID error\n");
		return -ENODEV;
	}

	ret = ov5648_read_reg(client, OV5648_8BIT,
					OV5648_SC_CMMN_SUB_ID, &high);
	revision = (u8) high & 0x0f;

	dev_dbg(&client->dev, "sensor_revision = 0x%x\n", revision);
	dev_dbg(&client->dev, "detect ov5648 success\n");
	return 0;
}

static int ov5648_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct ov5648_device *dev = to_ov5648_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	dev_dbg(&client->dev, "@%s:\n", __func__);
	mutex_lock(&dev->input_lock);

	ret = ov5648_write_reg(client, OV5648_8BIT, OV5648_SW_STREAM,
				enable ? OV5648_START_STREAMING :
				OV5648_STOP_STREAMING);

	mutex_unlock(&dev->input_lock);

	return ret;
}

/* ov5648 enum frame size, frame intervals */
static int ov5648_enum_framesizes(struct v4l2_subdev *sd,
				  struct v4l2_frmsizeenum *fsize)
{
	unsigned int index = fsize->index;

	if (index >= N_RES)
		return -EINVAL;

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = ov5648_res[index].width;
	fsize->discrete.height = ov5648_res[index].height;
	fsize->reserved[0] = ov5648_res[index].used;

	return 0;
}

static int ov5648_enum_frameintervals(struct v4l2_subdev *sd,
				      struct v4l2_frmivalenum *fival)
{
	unsigned int index = fival->index;

	if (index >= N_RES)
		return -EINVAL;

	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fival->width = ov5648_res[index].width;
	fival->height = ov5648_res[index].height;
	fival->discrete.numerator = 1;
	fival->discrete.denominator = ov5648_res[index].fps;

	return 0;
}

static int ov5648_enum_mbus_fmt(struct v4l2_subdev *sd,
				unsigned int index,
				enum v4l2_mbus_pixelcode *code)
{
	*code = V4L2_MBUS_FMT_SBGGR10_1X10;

	return 0;
}

static int ov5648_s_config(struct v4l2_subdev *sd,
			   int irq, void *platform_data)
{
	struct ov5648_device *dev = to_ov5648_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	if (platform_data == NULL)
		return -ENODEV;

	dev->platform_data =
		(struct camera_sensor_platform_data *)platform_data;

	mutex_lock(&dev->input_lock);
	/* power off the module, then power on it in future
	 * as first power on by board may not fulfill the
	 * power on sequqence needed by the module
	 */
	ret = power_down(sd);
	if (ret) {
		dev_err(&client->dev, "ov5648 power-off err.\n");
		goto fail_power_off;
	}

	ret = power_up(sd);
	if (ret) {
		dev_err(&client->dev, "ov5648 power-up err.\n");
		goto fail_power_on;
	}

	ret = dev->platform_data->csi_cfg(sd, 1);
	if (ret)
		goto fail_csi_cfg;

	/* config & detect sensor */
	ret = ov5648_detect(client);
	if (ret) {
		dev_err(&client->dev, "ov5648_detect err s_config.\n");
		goto fail_csi_cfg;
	}
	if(dev->current_otp.otp_en == 1)
		update_otp(sd);
	/* turn off sensor, after probed */
	ret = power_down(sd);
	if (ret) {
		dev_err(&client->dev, "ov5648 power-off err.\n");
		goto fail_csi_cfg;
	}
	mutex_unlock(&dev->input_lock);

	return 0;

fail_csi_cfg:
	dev->platform_data->csi_cfg(sd, 0);
fail_power_on:
	power_down(sd);
	dev_err(&client->dev, "sensor power-gating failed\n");
fail_power_off:
	mutex_unlock(&dev->input_lock);
	return ret;
}

static int ov5648_g_parm(struct v4l2_subdev *sd,
			struct v4l2_streamparm *param)
{
	struct ov5648_device *dev = to_ov5648_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!param)
		return -EINVAL;

	if (param->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		dev_err(&client->dev,  "unsupported buffer type.\n");
		return -EINVAL;
	}

	memset(param, 0, sizeof(*param));
	param->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (dev->fmt_idx >= 0 && dev->fmt_idx < N_RES) {
		param->parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
		param->parm.capture.timeperframe.numerator = 1;
		param->parm.capture.capturemode = dev->run_mode;
		param->parm.capture.timeperframe.denominator =
			ov5648_res[dev->fmt_idx].fps;
	}
	return 0;
}

static int ov5648_s_parm(struct v4l2_subdev *sd,
			struct v4l2_streamparm *param)
{
	struct ov5648_device *dev = to_ov5648_sensor(sd);
	dev->run_mode = param->parm.capture.capturemode;

	mutex_lock(&dev->input_lock);
	switch (dev->run_mode) {
	case CI_MODE_VIDEO:
		ov5648_res = ov5648_res_video;
		N_RES = N_RES_VIDEO;
		break;
	case CI_MODE_STILL_CAPTURE:
		ov5648_res = ov5648_res_still;
		N_RES = N_RES_STILL;
		break;
	default:
		ov5648_res = ov5648_res_preview;
		N_RES = N_RES_PREVIEW;
	}
	mutex_unlock(&dev->input_lock);
	return 0;
}

static int ov5648_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *interval)
{
	struct ov5648_device *dev = to_ov5648_sensor(sd);

	interval->interval.numerator = 1;
	interval->interval.denominator = ov5648_res[dev->fmt_idx].fps;

	return 0;
}

static int ov5648_enum_mbus_code(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index >= MAX_FMTS)
		return -EINVAL;

	code->code = V4L2_MBUS_FMT_SBGGR10_1X10;
	return 0;
}

static int ov5648_enum_frame_size(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_frame_size_enum *fse)
{
	int index = fse->index;

	if (index >= N_RES)
		return -EINVAL;

	fse->min_width = ov5648_res[index].width;
	fse->min_height = ov5648_res[index].height;
	fse->max_width = ov5648_res[index].width;
	fse->max_height = ov5648_res[index].height;

	return 0;

}

static struct v4l2_mbus_framefmt *
__ov5648_get_pad_format(struct ov5648_device *sensor,
			struct v4l2_subdev_fh *fh, unsigned int pad,
			enum v4l2_subdev_format_whence which)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->sd);

	if (pad != 0) {
		dev_err(&client->dev,
			"__ov5648_get_pad_format err. pad %x\n", pad);
		return NULL;
	}

	switch (which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		return v4l2_subdev_get_try_format(fh, pad);
	case V4L2_SUBDEV_FORMAT_ACTIVE:
		return &sensor->format;
	default:
		return NULL;
	}
}

static int ov5648_get_pad_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	struct ov5648_device *snr = to_ov5648_sensor(sd);
	struct v4l2_mbus_framefmt *format =
			__ov5648_get_pad_format(snr, fh, fmt->pad, fmt->which);
	if (!format)
		return -EINVAL;

	fmt->format = *format;
	return 0;
}

static int ov5648_set_pad_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	struct ov5648_device *snr = to_ov5648_sensor(sd);

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		snr->format = fmt->format;

	return 0;
}

static int ov5648_g_skip_frames(struct v4l2_subdev *sd, u32 *frames)
{
	struct ov5648_device *dev = to_ov5648_sensor(sd);

	mutex_lock(&dev->input_lock);
	*frames = ov5648_res[dev->fmt_idx].skip_frames;
	mutex_unlock(&dev->input_lock);

	return 0;
}


static const struct v4l2_subdev_sensor_ops ov5648_sensor_ops = {
	.g_skip_frames	= ov5648_g_skip_frames,
};

static const struct v4l2_subdev_video_ops ov5648_video_ops = {
	.s_stream = ov5648_s_stream,
	.g_parm = ov5648_g_parm,
	.s_parm = ov5648_s_parm,
	.enum_framesizes = ov5648_enum_framesizes,
	.enum_frameintervals = ov5648_enum_frameintervals,
	.enum_mbus_fmt = ov5648_enum_mbus_fmt,
	.try_mbus_fmt = ov5648_try_mbus_fmt,
	.g_mbus_fmt = ov5648_g_mbus_fmt,
	.s_mbus_fmt = ov5648_s_mbus_fmt,
	.g_frame_interval = ov5648_g_frame_interval,
};

static const struct v4l2_subdev_core_ops ov5648_core_ops = {
	.s_power = ov5648_s_power,
	.queryctrl = ov5648_queryctrl,
	.g_ctrl = ov5648_g_ctrl,
	.s_ctrl = ov5648_s_ctrl,
	.ioctl = ov5648_ioctl,
};

static const struct v4l2_subdev_pad_ops ov5648_pad_ops = {
	.enum_mbus_code = ov5648_enum_mbus_code,
	.enum_frame_size = ov5648_enum_frame_size,
	.get_fmt = ov5648_get_pad_format,
	.set_fmt = ov5648_set_pad_format,
};

static const struct v4l2_subdev_ops ov5648_ops = {
	.core = &ov5648_core_ops,
	.video = &ov5648_video_ops,
	.pad = &ov5648_pad_ops,
	.sensor = &ov5648_sensor_ops,
};

static int ov5648_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov5648_device *dev = to_ov5648_sensor(sd);
	dev_dbg(&client->dev, "ov5648_remove...\n");

	dev->platform_data->csi_cfg(sd, 0);

	v4l2_device_unregister_subdev(sd);
	media_entity_cleanup(&dev->sd.entity);
	kfree(dev);

	return 0;
}

static int ov5648_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct ov5648_device *dev;
	size_t len = CAMERA_MODULE_ID_LEN * sizeof(char);
	int ret;
	void *pdata;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&client->dev, "out of memory\n");
		return -ENOMEM;
	}

	dev->camera_module = kzalloc(len, GFP_KERNEL);
	if (!dev->camera_module) {
		kfree(dev);
		dev_err(&client->dev, "out of memory\n");
		return -ENOMEM;
	}

	mutex_init(&dev->input_lock);

	dev->fmt_idx = 0;
	//otp functions
	dev->current_otp.otp_en = 1;// enable otp functions
	v4l2_i2c_subdev_init(&(dev->sd), client, &ov5648_ops);

	if (gmin_get_config_var(&client->dev, "CameraModule",
				dev->camera_module, &len)) {
		kfree(dev->camera_module);
		dev->camera_module = NULL;
		dev_info(&client->dev, "Camera module id is missing\n");
	}

	if (ACPI_COMPANION(&client->dev))
		pdata = gmin_camera_platform_data(&dev->sd,
						  ATOMISP_INPUT_FORMAT_RAW_10,
						  atomisp_bayer_order_bggr);
	else
		pdata = client->dev.platform_data;

	if (!pdata) {
		ret = -EINVAL;
		goto out_free;
	}

	ret = ov5648_s_config(&dev->sd, client->irq, pdata);
	if (ret)
		goto out_free;

	ret = atomisp_register_i2c_module(&dev->sd, pdata, RAW_CAMERA);
	if (ret)
		goto out_free;

	dev->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->pad.flags = MEDIA_PAD_FL_SOURCE;
	dev->format.code = V4L2_MBUS_FMT_SBGGR10_1X10;
	dev->sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;

	ret = media_entity_init(&dev->sd.entity, 1, &dev->pad, 0);
	if (ret)
		ov5648_remove(client);

	return ret;
out_free:
	v4l2_device_unregister_subdev(&dev->sd);
	kfree(dev->camera_module);
	kfree(dev);
	return ret;
}

static struct acpi_device_id ov5648_acpi_match[] = {
	{"XXOV5648"},
	{},
};
MODULE_DEVICE_TABLE(acpi, ov5648_acpi_match);

MODULE_DEVICE_TABLE(i2c, ov5648_id);
static struct i2c_driver ov5648_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = OV5648_NAME,
		.acpi_match_table = ACPI_PTR(ov5648_acpi_match),
	},
	.probe = ov5648_probe,
	.remove = ov5648_remove,
	.id_table = ov5648_id,
};

static int init_ov5648(void)
{
	return i2c_add_driver(&ov5648_driver);
}

static void exit_ov5648(void)
{

	i2c_del_driver(&ov5648_driver);
}

module_init(init_ov5648);
module_exit(exit_ov5648);

MODULE_DESCRIPTION("A low-level driver for OmniVision 5648 sensors");
MODULE_LICENSE("GPL");
