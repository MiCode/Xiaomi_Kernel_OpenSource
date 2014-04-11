/*
 * Support for OmniVision OV5693 5M HD camera sensor.
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

#include <linux/device.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kmod.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/moduleparam.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>

#include "ov5693.h"

static const uint32_t ov5693_embedded_effective_size = 28;

/* i2c read/write stuff */
static int ov5693_read_reg(struct i2c_client *client,
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

	if (data_length != OV5693_8BIT && data_length != OV5693_16BIT
					&& data_length != OV5693_32BIT) {
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
	if (data_length == OV5693_8BIT)
		*val = (u8)data[0];
	else if (data_length == OV5693_16BIT)
		*val = be16_to_cpu(*(u16 *)&data[0]);
	else
		*val = be32_to_cpu(*(u32 *)&data[0]);

	return 0;
}

static int ov5693_i2c_write(struct i2c_client *client, u16 len, u8 *data)
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

static int ov5693_write_reg(struct i2c_client *client, u16 data_length,
							u16 reg, u16 val)
{
	int ret;
	unsigned char data[4] = {0};
	u16 *wreg = (u16 *)data;
	const u16 len = data_length + sizeof(u16); /* 16-bit address + data */

	if (data_length != OV5693_8BIT && data_length != OV5693_16BIT) {
		dev_err(&client->dev,
			"%s error, invalid data_length\n", __func__);
		return -EINVAL;
	}

	/* high byte goes out first */
	*wreg = cpu_to_be16(reg);

	if (data_length == OV5693_8BIT) {
		data[2] = (u8)(val);
	} else {
		/* OV5693_16BIT */
		u16 *wdata = (u16 *)&data[2];
		*wdata = cpu_to_be16(val);
	}

	ret = ov5693_i2c_write(client, len, data);
	if (ret)
		dev_err(&client->dev,
			"write error: wrote 0x%x to offset 0x%x error %d",
			val, reg, ret);

	return ret;
}

/*
 * ov5693_write_reg_array - Initializes a list of OV5693 registers
 * @client: i2c driver client structure
 * @reglist: list of registers to be written
 *
 * This function initializes a list of registers. When consecutive addresses
 * are found in a row on the list, this function creates a buffer and sends
 * consecutive data in a single i2c_transfer().
 *
 * __ov5693_flush_reg_array, __ov5693_buf_reg_array() and
 * __ov5693_write_reg_is_consecutive() are internal functions to
 * ov5693_write_reg_array_fast() and should be not used anywhere else.
 *
 */
static int __ov5693_flush_reg_array(struct i2c_client *client,
				    struct ov5693_write_ctrl *ctrl)
{
	u16 size;

	if (ctrl->index == 0)
		return 0;

	size = sizeof(u16) + ctrl->index; /* 16-bit address + data */
	ctrl->buffer.addr = cpu_to_be16(ctrl->buffer.addr);
	ctrl->index = 0;

	return ov5693_i2c_write(client, size, (u8 *)&ctrl->buffer);
}

static int __ov5693_buf_reg_array(struct i2c_client *client,
				  struct ov5693_write_ctrl *ctrl,
				  const struct ov5693_reg *next)
{
	int size;
	u16 *data16;

	switch (next->type) {
	case OV5693_8BIT:
		size = 1;
		ctrl->buffer.data[ctrl->index] = (u8)next->val;
		break;
	case OV5693_16BIT:
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
	if (ctrl->index + sizeof(u16) >= OV5693_MAX_WRITE_BUF_SIZE)
		return __ov5693_flush_reg_array(client, ctrl);

	return 0;
}

static int __ov5693_write_reg_is_consecutive(struct i2c_client *client,
					     struct ov5693_write_ctrl *ctrl,
					     const struct ov5693_reg *next)
{
	if (ctrl->index == 0)
		return 1;

	return ctrl->buffer.addr + ctrl->index == next->reg;
}

static int ov5693_write_reg_array(struct i2c_client *client,
				  const struct ov5693_reg *reglist)
{
	const struct ov5693_reg *next = reglist;
	struct ov5693_write_ctrl ctrl;
	int err;

	ctrl.index = 0;
	for (; next->type != OV5693_TOK_TERM; next++) {
		switch (next->type & OV5693_TOK_MASK) {
		case OV5693_TOK_DELAY:
			err = __ov5693_flush_reg_array(client, &ctrl);
			if (err)
				return err;
			usleep_range(next->val * 1000, (next->val + 1) * 1000);
			break;
		default:
			/*
			 * If next address is not consecutive, data needs to be
			 * flushed before proceed.
			 */
			if (!__ov5693_write_reg_is_consecutive(client, &ctrl,
								next)) {
				err = __ov5693_flush_reg_array(client, &ctrl);
				if (err)
					return err;
			}
			err = __ov5693_buf_reg_array(client, &ctrl, next);
			if (err) {
				dev_err(&client->dev, "%s: write error, aborted\n",
					 __func__);
				return err;
			}
			break;
		}
	}

	return __ov5693_flush_reg_array(client, &ctrl);
}
static int ov5693_g_focal(struct v4l2_subdev *sd, s32 *val)
{
	*val = (OV5693_FOCAL_LENGTH_NUM << 16) | OV5693_FOCAL_LENGTH_DEM;
	return 0;
}

static int ov5693_g_fnumber(struct v4l2_subdev *sd, s32 *val)
{
	/*const f number for ov5693*/
	*val = (OV5693_F_NUMBER_DEFAULT_NUM << 16) | OV5693_F_NUMBER_DEM;
	return 0;
}

static int ov5693_g_fnumber_range(struct v4l2_subdev *sd, s32 *val)
{
	*val = (OV5693_F_NUMBER_DEFAULT_NUM << 24) |
		(OV5693_F_NUMBER_DEM << 16) |
		(OV5693_F_NUMBER_DEFAULT_NUM << 8) | OV5693_F_NUMBER_DEM;
	return 0;
}


static int ov5693_get_intg_factor(struct i2c_client *client,
				struct camera_mipi_info *info,
				const struct ov5693_resolution *res)
{
	struct atomisp_sensor_mode_data *buf = &info->data;
	unsigned int pix_clk_freq_hz;
	u16 reg_val;
	int ret;

	if (info == NULL)
		return -EINVAL;

	/* pixel clock calculattion */
	pix_clk_freq_hz = res->pix_clk_freq * 1000000;

	buf->vt_pix_clk_freq_mhz = pix_clk_freq_hz;

	/* get integration time */
	buf->coarse_integration_time_min = OV5693_COARSE_INTG_TIME_MIN;
	buf->coarse_integration_time_max_margin =
					OV5693_COARSE_INTG_TIME_MAX_MARGIN;

	buf->fine_integration_time_min = OV5693_FINE_INTG_TIME_MIN;
	buf->fine_integration_time_max_margin =
					OV5693_FINE_INTG_TIME_MAX_MARGIN;

	buf->fine_integration_time_def = OV5693_FINE_INTG_TIME_MIN;
	buf->frame_length_lines = res->lines_per_frame;
	buf->line_length_pck = res->pixels_per_line;
	buf->read_mode = res->bin_mode;

	/* get the cropping and output resolution to ISP for this mode. */
	ret =  ov5693_read_reg(client, OV5693_16BIT,
					OV5693_H_CROP_START_H, &reg_val);
	if (ret)
		return ret;
	buf->crop_horizontal_start = reg_val;

	ret =  ov5693_read_reg(client, OV5693_16BIT,
					OV5693_V_CROP_START_H, &reg_val);
	if (ret)
		return ret;
	buf->crop_vertical_start = reg_val;

	ret = ov5693_read_reg(client, OV5693_16BIT,
					OV5693_H_CROP_END_H, &reg_val);
	if (ret)
		return ret;
	buf->crop_horizontal_end = reg_val;

	ret = ov5693_read_reg(client, OV5693_16BIT,
					OV5693_V_CROP_END_H, &reg_val);
	if (ret)
		return ret;
	buf->crop_vertical_end = reg_val;

	ret = ov5693_read_reg(client, OV5693_16BIT,
					OV5693_H_OUTSIZE_H, &reg_val);
	if (ret)
		return ret;
	buf->output_width = reg_val;

	ret = ov5693_read_reg(client, OV5693_16BIT,
					OV5693_V_OUTSIZE_H, &reg_val);
	if (ret)
		return ret;
	buf->output_height = reg_val;

	/*
	 * we can't return 0 for bin_factor, this is because camera
	 * HAL will use them as denominator, bin_factor = 0 will
	 * cause camera HAL crash. So we return bin_factor as this
	 * rules:
	 * [1]. res->bin_factor = 0, return 1 for bin_factor.
	 * [2]. res->bin_factor > 0, return res->bin_factor.
	 */
	buf->binning_factor_x = res->bin_factor_x ?
					res->bin_factor_x : 1;
	buf->binning_factor_y = res->bin_factor_y ?
					res->bin_factor_y : 1;
	return 0;
}

static long __ov5693_set_exposure(struct v4l2_subdev *sd, int coarse_itg,
				 int gain, int digitgain)

{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u16 vts;
	int ret;

	/*
	 * According to spec, the low 4 bits of exposure/gain reg are
	 * fraction bits, so need to take 4 bits left shift to align
	 * reg integer bits.
	 */
	coarse_itg <<= 4;
	gain <<= 4;

	ret = ov5693_read_reg(client, OV5693_16BIT,
					OV5693_VTS_H, &vts);
	if (ret)
		return ret;

	if (coarse_itg + OV5693_INTEGRATION_TIME_MARGIN >= vts)
		vts = coarse_itg + OV5693_INTEGRATION_TIME_MARGIN;

	ret = ov5693_write_reg(client, OV5693_16BIT, OV5693_VTS_H, vts);
	if (ret)
		return ret;

	/* group hold start */
	ret = ov5693_write_reg(client, OV5693_8BIT, OV5693_GROUP_ACCESS, 0);
	if (ret)
		return ret;

	/* set exposure */
	ret = ov5693_write_reg(client, OV5693_8BIT,
					OV5693_AEC_PK_EXPO_L,
					coarse_itg & 0xff);
	if (ret)
		return ret;

	ret = ov5693_write_reg(client, OV5693_16BIT,
					OV5693_AEC_PK_EXPO_H,
					(coarse_itg >> 8) & 0xfff);
	if (ret)
		return ret;

	/* set analog gain */
	ret = ov5693_write_reg(client, OV5693_16BIT,
					OV5693_AGC_ADJ_H, gain);
	if (ret)
		return ret;

	/* set digital gain */
	ret = ov5693_write_reg(client, OV5693_16BIT,
				OV5693_MWB_GAIN_R_H, digitgain);
	if (ret)
		return ret;

	ret = ov5693_write_reg(client, OV5693_16BIT,
				OV5693_MWB_GAIN_G_H, digitgain);
	if (ret)
		return ret;

	ret = ov5693_write_reg(client, OV5693_16BIT,
				OV5693_MWB_GAIN_B_H, digitgain);
	if (ret)
		return ret;

	/* group hold end */
	ret = ov5693_write_reg(client, OV5693_8BIT,
					OV5693_GROUP_ACCESS, 0x10);
	if (ret)
		return ret;

	/* group hold launch */
	ret = ov5693_write_reg(client, OV5693_8BIT,
					OV5693_GROUP_ACCESS, 0xa0);

	return ret;
}

static int ov5693_set_exposure(struct v4l2_subdev *sd, int exposure,
	int gain, int digitgain)
{
	struct ov5693_device *dev = to_ov5693_sensor(sd);
	int ret;

	mutex_lock(&dev->input_lock);
	ret = __ov5693_set_exposure(sd, exposure, gain, digitgain);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static long ov5693_s_exposure(struct v4l2_subdev *sd,
			       struct atomisp_exposure *exposure)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int exp = exposure->integration_time[0];
	int gain = exposure->gain[0];
	int digitgain = exposure->gain[1];

	/* we should not accept the invalid value below. */
	if (gain == 0) {
		dev_err(&client->dev, "%s: invalid value\n", __func__);
		return -EINVAL;
	}

	return ov5693_set_exposure(sd, exp, gain, digitgain);
}

static long ov5693_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{

	switch (cmd) {
	case ATOMISP_IOC_S_EXPOSURE:
		return ov5693_s_exposure(sd, arg);
	default:
		return -EINVAL;
	}
	return 0;
}

/* This returns the exposure time being used. This should only be used
   for filling in EXIF data, not for actual image processing. */
static int ov5693_q_exposure(struct v4l2_subdev *sd, s32 *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u16 reg_v, reg_v2;
	int ret;

	/* get exposure */
	ret = ov5693_read_reg(client, OV5693_8BIT,
					OV5693_AEC_PK_EXPO_L,
					&reg_v);
	if (ret)
		goto err;

	ret = ov5693_read_reg(client, OV5693_8BIT,
					OV5693_AEC_PK_EXPO_M,
					&reg_v2);
	if (ret)
		goto err;

	reg_v += reg_v2 << 8;
	ret = ov5693_read_reg(client, OV5693_8BIT,
					OV5693_AEC_PK_EXPO_H,
					&reg_v2);
	if (ret)
		goto err;

	*value = (reg_v + (((u32)reg_v2 << 16))) >> 4;
err:
	return ret;
}

/*
 * This below focus func don't need input_lock mutex_lock
 * since they are just called in v4l2 s_ctrl/g_ctrl framework
 * where mutex input_lock have been done.
 */
int ov5693_t_focus_abs(struct v4l2_subdev *sd, s32 value)
{
	struct ov5693_device *dev = to_ov5693_sensor(sd);
	int ret = 0;

	if (dev->vcm_driver && dev->vcm_driver->t_focus_abs)
		ret = dev->vcm_driver->t_focus_abs(sd, value);

	return ret;
}

int ov5693_t_focus_rel(struct v4l2_subdev *sd, s32 value)
{
	struct ov5693_device *dev = to_ov5693_sensor(sd);
	int ret = 0;

	if (dev->vcm_driver && dev->vcm_driver->t_focus_rel)
		ret = dev->vcm_driver->t_focus_rel(sd, value);

	return ret;
}

int ov5693_q_focus_status(struct v4l2_subdev *sd, s32 *value)
{
	struct ov5693_device *dev = to_ov5693_sensor(sd);
	int ret = 0;

	if (dev->vcm_driver && dev->vcm_driver->q_focus_status)
		ret = dev->vcm_driver->q_focus_status(sd, value);

	return ret;
}

int ov5693_q_focus_abs(struct v4l2_subdev *sd, s32 *value)
{
	struct ov5693_device *dev = to_ov5693_sensor(sd);
	int ret = 0;

	if (dev->vcm_driver && dev->vcm_driver->q_focus_abs)
		ret = dev->vcm_driver->q_focus_abs(sd, value);

	return ret;
}

/* ov5693 control set/get */
static int ov5693_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov5693_device *dev = container_of(
		ctrl->handler, struct ov5693_device, ctrl_handler);
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE_ABSOLUTE:
		ret = ov5693_q_exposure(&dev->sd, &ctrl->val);
		break;
	case V4L2_CID_FOCUS_ABSOLUTE:
		ret = ov5693_q_focus_abs(&dev->sd, &ctrl->val);
		break;
	case V4L2_CID_FOCUS_STATUS:
		ret = ov5693_q_focus_status(&dev->sd, &ctrl->val);
		break;
	case V4L2_CID_FOCAL_ABSOLUTE:
		ret = ov5693_g_focal(&dev->sd, &ctrl->val);
		break;
	case V4L2_CID_FNUMBER_ABSOLUTE:
		ret = ov5693_g_fnumber(&dev->sd, &ctrl->val);
		break;
	case V4L2_CID_FNUMBER_RANGE:
		ret = ov5693_g_fnumber_range(&dev->sd, &ctrl->val);
		break;
	case V4L2_CID_BIN_FACTOR_HORZ:
		ctrl->val = dev->ov5693_res[dev->fmt_idx].bin_factor_x;
		break;
	case V4L2_CID_BIN_FACTOR_VERT:
		ctrl->val = dev->ov5693_res[dev->fmt_idx].bin_factor_y;
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int ov5693_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov5693_device *dev = container_of(
		ctrl->handler, struct ov5693_device, ctrl_handler);
	int ret = 0;

	if (!ctrl)
		return -EINVAL;

	switch (ctrl->id) {
	case V4L2_CID_RUN_MODE:
		switch (ctrl->val) {
		case ATOMISP_RUN_MODE_VIDEO:
			dev->ov5693_res = ov5693_res_video;
			dev->curr_res_num = N_RES_VIDEO;
			break;
		case ATOMISP_RUN_MODE_STILL_CAPTURE:
			dev->ov5693_res = ov5693_res_still;
			dev->curr_res_num = N_RES_STILL;
			break;
		default:
			dev->ov5693_res = ov5693_res_preview;
			dev->curr_res_num = N_RES_PREVIEW;
		}
		break;
	case V4L2_CID_FOCUS_ABSOLUTE:
		ret = ov5693_t_focus_abs(&dev->sd, ctrl->val);
		break;
	case V4L2_CID_FOCUS_RELATIVE:
		ret = ov5693_t_focus_rel(&dev->sd, ctrl->val);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int ov5693_init(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5693_device *dev = to_ov5693_sensor(sd);
	int ret = 0;

	/* restore settings */
	dev->ov5693_res = ov5693_res_preview;
	dev->curr_res_num = N_RES_PREVIEW;

	ret = ov5693_write_reg_array(client, ov5693_init_setting);
	if (ret)
		dev_err(&client->dev, "ov5693 write init setting reg err.\n");

	return ret;
}


static int power_up(struct v4l2_subdev *sd)
{
	struct ov5693_device *dev = to_ov5693_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	if (NULL == dev->platform_data) {
		dev_err(&client->dev,
			"no camera_sensor_platform_data");
		return -ENODEV;
	}

	/* power control */
	ret = dev->platform_data->power_ctrl(sd, 1);
	if (ret)
		goto fail_power;

	/* gpio ctrl */
	ret = dev->platform_data->gpio_ctrl(sd, 1);
	if (ret)
		goto fail_power;

	/* flis clock control */
	ret = dev->platform_data->flisclk_ctrl(sd, 1);
	if (ret)
		goto fail_clk;

	/* according to DS, 20ms is needed between PWDN and i2c access */
	msleep(20);

	return 0;

fail_clk:
	dev->platform_data->gpio_ctrl(sd, 0);
fail_power:
	dev->platform_data->power_ctrl(sd, 0);
	dev_err(&client->dev, "sensor power-up failed\n");

	return ret;
}

static int power_down(struct v4l2_subdev *sd)
{
	struct ov5693_device *dev = to_ov5693_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	if (NULL == dev->platform_data) {
		dev_err(&client->dev,
			"no camera_sensor_platform_data");
		return -ENODEV;
	}

	ret = dev->platform_data->flisclk_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "flisclk failed\n");

	/* gpio ctrl */
	ret = dev->platform_data->gpio_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "gpio failed.\n");

	/* power control */
	ret = dev->platform_data->power_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "vprog failed.\n");

	return ret;
}

static int ov5693_s_power(struct v4l2_subdev *sd, int on)
{
	struct ov5693_device *dev = to_ov5693_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	mutex_lock(&dev->input_lock);
	if (on == 0) {
		if (dev->vcm_driver && dev->vcm_driver->power_down)
			ret = dev->vcm_driver->power_down(sd);
		if (ret)
			dev_err(&client->dev, "vcm power-down failed.\n");

		ret = power_down(sd);
	} else {
		ret = power_up(sd);
		if (ret)
			goto done;

		ret = ov5693_init(sd);
		if (ret)
			goto done;

		if (dev->vcm_driver && dev->vcm_driver->power_up)
			ret = dev->vcm_driver->power_up(sd);
		if (ret)
			dev_err(&client->dev, "vcm power-up failed.\n");
	}

done:
	mutex_unlock(&dev->input_lock);
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
static int distance(struct ov5693_resolution *res, u32 w, u32 h)
{
	unsigned int w_ratio = ((res->width << RATIO_SHIFT_BITS)/w);
	unsigned int h_ratio;
	int match;

	if (h == 0)
		return -1;
	h_ratio = ((res->height << RATIO_SHIFT_BITS) / h);
	if (h_ratio == 0)
		return -1;
	match   = abs(((w_ratio << RATIO_SHIFT_BITS) / h_ratio)
			- ((int)(1 << RATIO_SHIFT_BITS)));

	if ((w_ratio < (int)(1 << RATIO_SHIFT_BITS))
	    || (h_ratio < (int)(1 << RATIO_SHIFT_BITS))  ||
		(match > LARGEST_ALLOWED_RATIO_MISMATCH))
		return -1;

	return w_ratio + h_ratio;
}

/* Return the nearest higher resolution index */
static int nearest_resolution_index(struct v4l2_subdev *sd,
				    int w, int h)
{
	struct ov5693_device *dev = to_ov5693_sensor(sd);
	int i;
	int idx = dev->curr_res_num-1;
	int dist;
	int min_dist = INT_MAX;
	struct ov5693_resolution *tmp_res = NULL;

	for (i = 0; i < dev->curr_res_num; i++) {
		tmp_res = &dev->ov5693_res[i];
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

static int get_resolution_index(struct v4l2_subdev *sd,
				int w, int h)
{
	struct ov5693_device *dev = to_ov5693_sensor(sd);
	int i;

	for (i = 0; i < dev->curr_res_num; i++) {
		if (w != dev->ov5693_res[i].width)
			continue;
		if (h != dev->ov5693_res[i].height)
			continue;

		return i;
	}

	return -1;
}

static int __ov5693_try_mbus_fmt(struct v4l2_subdev *sd,
				 struct v4l2_mbus_framefmt *fmt)
{
	struct ov5693_device *dev = to_ov5693_sensor(sd);
	int idx;

	if (!fmt)
		return -EINVAL;

	idx = nearest_resolution_index(sd, fmt->width, fmt->height);
	fmt->width = dev->ov5693_res[idx].width;
	fmt->height = dev->ov5693_res[idx].height;
	fmt->code = V4L2_MBUS_FMT_SGRBG10_1X10;

	return 0;
}

static int ov5693_try_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	struct ov5693_device *dev = to_ov5693_sensor(sd);
	int ret;

	mutex_lock(&dev->input_lock);
	ret = __ov5693_try_mbus_fmt(sd, fmt);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int ov5693_s_mbus_fmt(struct v4l2_subdev *sd,
			     struct v4l2_mbus_framefmt *fmt)
{
	struct ov5693_device *dev = to_ov5693_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camera_mipi_info *ov5693_info = NULL;
	int ret = 0;

	ov5693_info = v4l2_get_subdev_hostdata(sd);
	if (ov5693_info == NULL)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	ret = __ov5693_try_mbus_fmt(sd, fmt);
	if (ret == -1) {
		dev_err(&client->dev, "try fmt fail\n");
		goto done;
	}

	dev->fmt_idx = get_resolution_index(sd, fmt->width, fmt->height);
	if (dev->fmt_idx == -1) {
		dev_err(&client->dev, "get resolution fail\n");
		goto done;
	}

	ret = ov5693_write_reg_array(client, dev->ov5693_res[dev->fmt_idx].regs);
	if (ret) {
		dev_err(&client->dev, "ov5693 write fmt register err.\n");
		goto done;
	}

	ret = ov5693_get_intg_factor(client, ov5693_info,
					&dev->ov5693_res[dev->fmt_idx]);
	if (ret)
		dev_err(&client->dev, "failed to get integration_factor\n");

	ov5693_info->metadata_width = fmt->width * 10 / 8;
	ov5693_info->metadata_height = 1;
	ov5693_info->metadata_effective_width = &ov5693_embedded_effective_size;

done:
	mutex_unlock(&dev->input_lock);
	return ret;
}
static int ov5693_g_mbus_fmt(struct v4l2_subdev *sd,
			     struct v4l2_mbus_framefmt *fmt)
{
	struct ov5693_device *dev = to_ov5693_sensor(sd);

	mutex_lock(&dev->input_lock);
	fmt->width = dev->ov5693_res[dev->fmt_idx].width;
	fmt->height = dev->ov5693_res[dev->fmt_idx].height;
	fmt->code = V4L2_MBUS_FMT_SBGGR10_1X10;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static int ov5693_detect(struct i2c_client *client)
{
	struct i2c_adapter *adapter = client->adapter;
	int ret = 0;
	u16 id;
	u8 revision;

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -ENODEV;

	ret = ov5693_read_reg(client, OV5693_16BIT,
					OV5693_SC_CMMN_CHIP_ID, &id);
	if (ret) {
		dev_err(&client->dev, "read sensor_id err.\n");
		return -ENODEV;
	}

	if (id != OV5693_ID) {
		dev_err(&client->dev, "sensor ID error\n");
		return -ENODEV;
	}

	ret = ov5693_read_reg(client, OV5693_8BIT,
					OV5693_SC_CMMN_SUB_ID, &id);
	revision = (u8)id & 0x0f;

	dev_dbg(&client->dev, "sensor_revision = 0x%x\n", revision);
	dev_dbg(&client->dev, "detect ov5693 success\n");
	return ret;
}

static int ov5693_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct ov5693_device *dev = to_ov5693_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	mutex_lock(&dev->input_lock);

	ret = ov5693_write_reg(client, OV5693_8BIT, OV5693_SW_STREAM,
				enable ? OV5693_START_STREAMING :
				OV5693_STOP_STREAMING);

	mutex_unlock(&dev->input_lock);
	return ret;
}

/* ov5693 enum frame size, frame intervals */
static int ov5693_enum_framesizes(struct v4l2_subdev *sd,
				  struct v4l2_frmsizeenum *fsize)
{
	struct ov5693_device *dev = to_ov5693_sensor(sd);
	unsigned int index = fsize->index;

	mutex_lock(&dev->input_lock);

	if (index >= dev->curr_res_num) {
		mutex_unlock(&dev->input_lock);
		return -EINVAL;
	}

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = dev->ov5693_res[index].width;
	fsize->discrete.height = dev->ov5693_res[index].height;

	mutex_unlock(&dev->input_lock);
	return 0;
}

static int ov5693_enum_frameintervals(struct v4l2_subdev *sd,
				      struct v4l2_frmivalenum *fival)
{
	struct ov5693_device *dev = to_ov5693_sensor(sd);
	unsigned int index = fival->index;

	mutex_lock(&dev->input_lock);

	if (index >= dev->curr_res_num) {
		mutex_unlock(&dev->input_lock);
		return -EINVAL;
	}

	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fival->width = dev->ov5693_res[index].width;
	fival->height = dev->ov5693_res[index].height;
	fival->discrete.numerator = 1;
	fival->discrete.denominator = dev->ov5693_res[index].fps;

	mutex_unlock(&dev->input_lock);

	return 0;
}

static int ov5693_enum_mbus_fmt(struct v4l2_subdev *sd,
				unsigned int index,
				enum v4l2_mbus_pixelcode *code)
{
	*code = V4L2_MBUS_FMT_SBGGR10_1X10;

	return 0;
}

static int ov5693_s_config(struct v4l2_subdev *sd,
			   int irq, void *platform_data)
{
	struct ov5693_device *dev = to_ov5693_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	if (platform_data == NULL)
		return -ENODEV;

	mutex_lock(&dev->input_lock);

	dev->platform_data = platform_data;

	ret = power_up(sd);
	if (ret) {
		dev_err(&client->dev, "ov5693 power-up err.\n");
		goto fail_power_on;
	}

	ret = dev->platform_data->csi_cfg(sd, 1);
	if (ret)
		goto fail_csi_cfg;

	/* config & detect sensor */
	ret = ov5693_detect(client);
	if (ret) {
		dev_err(&client->dev, "ov5693_detect err s_config.\n");
		goto fail_csi_cfg;
	}

	/* turn off sensor, after probed */
	ret = power_down(sd);
	if (ret) {
		dev_err(&client->dev, "ov5693 power-off err.\n");
		goto fail_csi_cfg;
	}
	mutex_unlock(&dev->input_lock);

	return 0;

fail_csi_cfg:
	dev->platform_data->csi_cfg(sd, 0);
fail_power_on:
	power_down(sd);
	dev_err(&client->dev, "sensor power-gating failed\n");
	mutex_unlock(&dev->input_lock);
	return ret;
}

static int ov5693_g_parm(struct v4l2_subdev *sd,
			struct v4l2_streamparm *param)
{
	struct ov5693_device *dev = to_ov5693_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (param->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		dev_err(&client->dev,  "unsupported buffer type.\n");
		return -EINVAL;
	}

	memset(param, 0, sizeof(*param));
	param->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	mutex_lock(&dev->input_lock);
	if (dev->fmt_idx >= 0 && dev->fmt_idx < dev->curr_res_num) {
		param->parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
		param->parm.capture.timeperframe.numerator = 1;
		param->parm.capture.capturemode = dev->run_mode->val;
		param->parm.capture.timeperframe.denominator =
					dev->ov5693_res[dev->fmt_idx].fps;
	}
	mutex_unlock(&dev->input_lock);
	return 0;
}

static int ov5693_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *interval)
{
	struct ov5693_device *dev = to_ov5693_sensor(sd);

	mutex_lock(&dev->input_lock);
	interval->interval.numerator = 1;
	interval->interval.denominator = dev->ov5693_res[dev->fmt_idx].fps;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static int ov5693_enum_mbus_code(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_mbus_code_enum *code)
{
	code->code = V4L2_MBUS_FMT_SBGGR10_1X10;
	return 0;
}

static int ov5693_enum_frame_size(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_frame_size_enum *fse)
{
	struct ov5693_device *dev = to_ov5693_sensor(sd);
	int index = fse->index;

	mutex_lock(&dev->input_lock);

	if (index >= dev->curr_res_num) {
		mutex_unlock(&dev->input_lock);
		return -EINVAL;
	}

	fse->min_width = dev->ov5693_res[index].width;
	fse->min_height = dev->ov5693_res[index].height;
	fse->max_width = dev->ov5693_res[index].width;
	fse->max_height = dev->ov5693_res[index].height;

	mutex_unlock(&dev->input_lock);
	return 0;
}

static int ov5693_get_pad_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	struct ov5693_device *dev = to_ov5693_sensor(sd);
	struct v4l2_mbus_framefmt *format;

	mutex_lock(&dev->input_lock);

	switch (fmt->which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		format = v4l2_subdev_get_try_format(fh, fmt->pad);
		break;
	case V4L2_SUBDEV_FORMAT_ACTIVE:
		format = &dev->format;
		break;
	default:
		format = NULL;
	}

	mutex_unlock(&dev->input_lock);

	if (!format)
		return -EINVAL;

	fmt->format = *format;
	return 0;
}

static int ov5693_set_pad_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	struct ov5693_device *dev = to_ov5693_sensor(sd);

	mutex_lock(&dev->input_lock);

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		dev->format = fmt->format;

	mutex_unlock(&dev->input_lock);
	return 0;
}

static int ov5693_g_skip_frames(struct v4l2_subdev *sd, u32 *frames)
{
	struct ov5693_device *dev = to_ov5693_sensor(sd);

	mutex_lock(&dev->input_lock);
	*frames = dev->ov5693_res[dev->fmt_idx].skip_frames;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static const struct v4l2_ctrl_ops ctrl_ops = {
	.s_ctrl = ov5693_s_ctrl,
	.g_volatile_ctrl = ov5693_g_ctrl,
};

static const char * const ctrl_run_mode_menu[] = {
	NULL,
	"Video",
	"Still capture",
	"Continuous capture",
	"Preview",
};

static const struct v4l2_ctrl_config ctrl_run_mode = {
	.ops = &ctrl_ops,
	.id = V4L2_CID_RUN_MODE,
	.name = "run mode",
	.type = V4L2_CTRL_TYPE_MENU,
	.min = 1,
	.def = 4,
	.max = 4,
	.qmenu = ctrl_run_mode_menu,
};

static const struct v4l2_ctrl_config ctrls[] = {
	{
		.ops = &ctrl_ops,
		.id = V4L2_CID_EXPOSURE_ABSOLUTE,
		.name = "absolute exposure",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0x0,
		.max = 0xffff,
		.step = 0x01,
		.def = 0x00,
		.flags = 0,
	}, {
		.ops = &ctrl_ops,
		.id = V4L2_CID_FOCUS_ABSOLUTE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "focus move absolute",
		.min = 0,
		.max = OV5693_MAX_FOCUS_POS,
		.step = 1,
		.def = 0,
		.flags = 0,
	}, {
		.ops = &ctrl_ops,
		.id = V4L2_CID_FOCUS_RELATIVE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "focus move relative",
		.min = OV5693_MAX_FOCUS_NEG,
		.max = OV5693_MAX_FOCUS_POS,
		.step = 1,
		.def = 0,
		.flags = 0,
	}, {
		.ops = &ctrl_ops,
		.id = V4L2_CID_FOCUS_STATUS,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "focus status",
		.min = 0,
		.max = 100,
		.step = 1,
		.def = 0,
		.flags = 0,
	}, {
		.ops = &ctrl_ops,
		.id = V4L2_CID_FOCAL_ABSOLUTE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "focal length",
		.min = OV5693_FOCAL_LENGTH_DEFAULT,
		.max = OV5693_FOCAL_LENGTH_DEFAULT,
		.step = 0x01,
		.def = OV5693_FOCAL_LENGTH_DEFAULT,
		.flags = 0,
	}, {
		.ops = &ctrl_ops,
		.id = V4L2_CID_FNUMBER_ABSOLUTE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "f-number",
		.min = OV5693_F_NUMBER_DEFAULT,
		.max = OV5693_F_NUMBER_DEFAULT,
		.step = 0x01,
		.def = OV5693_F_NUMBER_DEFAULT,
		.flags = 0,
	}, {
		.ops = &ctrl_ops,
		.id = V4L2_CID_FNUMBER_RANGE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "f-number range",
		.min = OV5693_F_NUMBER_RANGE,
		.max =  OV5693_F_NUMBER_RANGE,
		.step = 0x01,
		.def = OV5693_F_NUMBER_RANGE,
		.flags = 0,
	}, {
		.ops = &ctrl_ops,
		.id = V4L2_CID_BIN_FACTOR_HORZ,
		.name = "horizontal binning factor",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.max = OV5693_BIN_FACTOR_MAX,
		.step = 2,
		.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE,
	}, {
		.ops = &ctrl_ops,
		.id = V4L2_CID_BIN_FACTOR_VERT,
		.name = "vertical binning factor",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.max = OV5693_BIN_FACTOR_MAX,
		.step = 2,
		.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE,
	}
};

static const struct v4l2_subdev_sensor_ops ov5693_sensor_ops = {
	.g_skip_frames	= ov5693_g_skip_frames,
};

static const struct v4l2_subdev_video_ops ov5693_video_ops = {
	.s_stream = ov5693_s_stream,
	.g_parm = ov5693_g_parm,
	.enum_framesizes = ov5693_enum_framesizes,
	.enum_frameintervals = ov5693_enum_frameintervals,
	.enum_mbus_fmt = ov5693_enum_mbus_fmt,
	.try_mbus_fmt = ov5693_try_mbus_fmt,
	.g_mbus_fmt = ov5693_g_mbus_fmt,
	.s_mbus_fmt = ov5693_s_mbus_fmt,
	.g_frame_interval = ov5693_g_frame_interval,
};

static const struct v4l2_subdev_core_ops ov5693_core_ops = {
	.s_power = ov5693_s_power,
	.queryctrl = v4l2_subdev_queryctrl,
	.g_ctrl = v4l2_subdev_g_ctrl,
	.s_ctrl = v4l2_subdev_s_ctrl,
	.ioctl = ov5693_ioctl,
};

static const struct v4l2_subdev_pad_ops ov5693_pad_ops = {
	.enum_mbus_code = ov5693_enum_mbus_code,
	.enum_frame_size = ov5693_enum_frame_size,
	.get_fmt = ov5693_get_pad_format,
	.set_fmt = ov5693_set_pad_format,
};

static const struct v4l2_subdev_ops ov5693_ops = {
	.core = &ov5693_core_ops,
	.video = &ov5693_video_ops,
	.pad = &ov5693_pad_ops,
	.sensor = &ov5693_sensor_ops,
};

static int ov5693_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov5693_device *dev = to_ov5693_sensor(sd);
	dev_dbg(&client->dev, "ov5693_remove...\n");

	dev->platform_data->csi_cfg(sd, 0);

	v4l2_device_unregister_subdev(sd);
	media_entity_cleanup(&dev->sd.entity);
	devm_kfree(&client->dev, dev);

	return 0;
}

static int ov5693_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct ov5693_device *dev;
	int i;
	int ret;

	dev = devm_kzalloc(&client->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&client->dev, "out of memory\n");
		return -ENOMEM;
	}

	mutex_init(&dev->input_lock);

	/*
	 * Initialize related res members of dev.
	 */
	dev->fmt_idx = 0;
	dev->ov5693_res = ov5693_res_preview;
	dev->curr_res_num = N_RES_PREVIEW;

	v4l2_i2c_subdev_init(&(dev->sd), client, &ov5693_ops);

	if (client->dev.platform_data) {
		ret = ov5693_s_config(&dev->sd, client->irq,
				       client->dev.platform_data);
		if (ret)
			goto out_free;
	}

	dev->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->pad.flags = MEDIA_PAD_FL_SOURCE;
	dev->format.code = V4L2_MBUS_FMT_SBGGR10_1X10;
	dev->sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
	dev->vcm_driver = &ov5693_vcm_ops;

	ret = v4l2_ctrl_handler_init(&dev->ctrl_handler, ARRAY_SIZE(ctrls) + 1);
	if (ret) {
		ov5693_remove(client);
		return ret;
	}

	dev->run_mode = v4l2_ctrl_new_custom(&dev->ctrl_handler,
					     &ctrl_run_mode, NULL);

	for (i = 0; i < ARRAY_SIZE(ctrls); i++)
		v4l2_ctrl_new_custom(&dev->ctrl_handler, &ctrls[i], NULL);

	if (dev->ctrl_handler.error) {
		ov5693_remove(client);
		return dev->ctrl_handler.error;
	}

	dev->ctrl_handler.lock = &dev->input_lock;
	dev->sd.ctrl_handler = &dev->ctrl_handler;
	v4l2_ctrl_handler_setup(&dev->ctrl_handler);

	ret = media_entity_init(&dev->sd.entity, 1, &dev->pad, 0);
	if (ret)
		ov5693_remove(client);

	/* vcm initialization */
	if (dev->vcm_driver && dev->vcm_driver->init)
		ret = dev->vcm_driver->init(&dev->sd);
	if (ret) {
		dev_err(&client->dev, "vcm init failed.\n");
		ov5693_remove(client);
	}

	return ret;
out_free:
	v4l2_device_unregister_subdev(&dev->sd);
	devm_kfree(&client->dev, dev);
	return ret;
}

MODULE_DEVICE_TABLE(i2c, ov5693_id);
static struct i2c_driver ov5693_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = OV5693_NAME,
	},
	.probe = ov5693_probe,
	.remove = ov5693_remove,
	.id_table = ov5693_id,
};

module_i2c_driver(ov5693_driver);

MODULE_DESCRIPTION("A low-level driver for OmniVision 5693 sensors");
MODULE_LICENSE("GPL");
