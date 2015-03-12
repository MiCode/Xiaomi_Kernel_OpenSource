/*
 * Support for ov2685 Camera Sensor.
 *
 * Copyright (c) 2012 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/firmware.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>

#include "ov2685.h"

#define to_ov2685_sensor(sd) container_of(sd, struct ov2685_device, sd)

static int
ov2685_read_reg(struct i2c_client *client, u16 data_length, u16 reg, u16 *val)
{
	int err;
	struct i2c_msg msg[2];
	unsigned char data[4];

	if (!client->adapter) {
		dev_err(&client->dev, "%s error, no client->adapter\n",
			__func__);
		return -ENODEV;
	}

	if (data_length != OV2685_8BIT && data_length != OV2685_16BIT
					 && data_length != OV2685_32BIT) {
		dev_err(&client->dev, "%s error, invalid data length\n",
			__func__);
		return -EINVAL;
	}

	memset(msg, 0 , sizeof(msg));

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = MSG_LEN_OFFSET;
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
		goto error;
	}

	*val = 0;
	/* high byte comes first */
	if (data_length == OV2685_8BIT)
		*val = (u8)data[0];
	else if (data_length == OV2685_16BIT)
		*val = be16_to_cpu(*(u16 *)&data[0]);
	else
		*val = be32_to_cpu(*(u32 *)&data[0]);

	return 0;
error:
	dev_err(&client->dev, "read from offset 0x%x error %d", reg, err);
	return err;
}

static int
ov2685_write_reg(struct i2c_client *client, u16 data_length, u16 reg, u32 val)
{
	int num_msg;
	struct i2c_msg msg;
	unsigned char data[6] = {0};
	u16 *wreg;
	int retry = 0;

	if (!client->adapter) {
		dev_err(&client->dev, "%s error, no client->adapter\n",
			__func__);
		return -ENODEV;
	}

	if (data_length != OV2685_8BIT && data_length != OV2685_16BIT
					 && data_length != OV2685_32BIT) {
		dev_err(&client->dev, "%s error, invalid data_length\n",
			__func__);
		return -EINVAL;
	}

	memset(&msg, 0, sizeof(msg));

again:
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 2 + data_length;
	msg.buf = data;

	/* high byte goes out first */
	wreg = (u16 *)data;
	*wreg = cpu_to_be16(reg);

	if (data_length == OV2685_8BIT) {
		data[2] = (u8)(val);
	} else if (data_length == OV2685_16BIT) {
		u16 *wdata = (u16 *)&data[2];
		*wdata = cpu_to_be16((u16)val);
	} else {
		u32 *wdata = (u32 *)&data[2];
		*wdata = cpu_to_be32(val);
	}

	num_msg = i2c_transfer(client->adapter, &msg, 1);

	/*
	 * It is said that Rev 2 sensor needs some delay here otherwise
	 * registers do not seem to load correctly. But tests show that
	 * removing the delay would not cause any in-stablility issue and the
	 * delay will cause serious performance down, so, removed previous
	 * mdelay(1) here.
	 */

	if (num_msg >= 0)
		return 0;

	dev_err(&client->dev, "write error: wrote 0x%x to offset 0x%x error %d",
		val, reg, num_msg);
	if (retry <= I2C_RETRY_COUNT) {
		dev_err(&client->dev, "retrying... %d", retry);
		retry++;
		msleep(20);
		goto again;
	}

	return num_msg;
}

/*
 * ov2685_write_reg_array - Initializes a list of ov2685 registers
 * @client: i2c driver client structure
 * @reglist: list of registers to be written
 *
 * Initializes a list of ov2685 registers. The list of registers is
 * terminated by OV2685_TOK_TERM.
 */
static int ov2685_write_reg_array(struct i2c_client *client,
			    const struct ov2685_reg *reglist)
{
	const struct ov2685_reg *next = reglist;
	int err;

	for (; next->length != OV2685_TOK_TERM; next++) {
		if (next->length == OV2685_TOK_DELAY) {
			msleep(next->val);
		} else {
			err = ov2685_write_reg(client, next->length, next->reg,
						next->val);
			/* REVISIT: Do we need this delay? */
			udelay(10);
			if (err) {
				dev_err(&client->dev, "%s err. aborted\n",
					__func__);
				return err;
			}
		}
	}

	return 0;
}

/* Horizontal flip the image. */
static int ov2685_g_hflip(struct v4l2_subdev *sd, s32 *val)
{
	return 0;
}

static int ov2685_g_vflip(struct v4l2_subdev *sd, s32 *val)
{
	return 0;
}

/* Horizontal flip the image. */
static int ov2685_t_hflip(struct v4l2_subdev *sd, int value)
{
	return 0;
}

/* Vertically flip the image */
static int ov2685_t_vflip(struct v4l2_subdev *sd, int value)
{
	return 0;
}

static int ov2685_s_freq(struct v4l2_subdev *sd, int value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u16 reg_val;

	switch (value) {
	case V4L2_CID_POWER_LINE_FREQUENCY_DISABLED:
		ov2685_read_reg(client, OV2685_8BIT,
					OV2685_AEC_CTRL0, &reg_val);
		/* turn off band filter, bit[0] = 0 */
		reg_val &= ~OV2685_BAND_ENABLE_MASK;
		ov2685_write_reg(client, OV2685_8BIT,
			OV2685_AEC_CTRL0, (u32)reg_val);
		break;
	case V4L2_CID_POWER_LINE_FREQUENCY_50HZ:
		ov2685_read_reg(client, OV2685_8BIT,
					OV2685_AEC_CTRL2, &reg_val);
		/* set 50Hz, bit[7] = 1 */
		reg_val |= OV2685_BAND_50HZ_MASK;
		ov2685_write_reg(client, OV2685_8BIT,
			OV2685_AEC_CTRL2, (u32)reg_val);

		ov2685_read_reg(client, OV2685_8BIT,
					OV2685_AEC_CTRL0, &reg_val);
		/* turn on band filter, bit[0] = 1 */
		reg_val |= OV2685_BAND_ENABLE_MASK;
		ov2685_write_reg(client, OV2685_8BIT,
			OV2685_AEC_CTRL0, (u32)reg_val);
		break;
	case V4L2_CID_POWER_LINE_FREQUENCY_60HZ:
		ov2685_read_reg(client, OV2685_8BIT,
					OV2685_AEC_CTRL2, &reg_val);
		/* set 50Hz, bit[7] = 0 */
		reg_val &= ~OV2685_BAND_50HZ_MASK;
		ov2685_write_reg(client, OV2685_8BIT,
			OV2685_AEC_CTRL2, (u32)reg_val);
		ov2685_read_reg(client, OV2685_8BIT,
					OV2685_AEC_CTRL0, &reg_val);
		/* turn on band filter, bit[0] = 1 */
		reg_val |= OV2685_BAND_ENABLE_MASK;
		ov2685_write_reg(client, OV2685_8BIT,
			OV2685_AEC_CTRL0, (u32)reg_val);
		break;
	case V4L2_CID_POWER_LINE_FREQUENCY_AUTO:
		ov2685_read_reg(client, OV2685_8BIT,
					OV2685_AEC_CTRL0, &reg_val);
		/* turn on band filter, bit[0] = 1 */
		reg_val |= OV2685_BAND_50HZ_MASK;
		ov2685_write_reg(client, OV2685_8BIT,
			OV2685_AEC_CTRL0, (u32)reg_val);
		break;
	default:
		dev_err(&client->dev, "Invalid freq value %d\n", value);
		return -EINVAL;
	}
	return 0;
}

static int ov2685_g_scene(struct v4l2_subdev *sd, s32 *value)
{
	return 0;
}

static int ov2685_s_scene(struct v4l2_subdev *sd, int value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	switch (value) {
	case V4L2_SCENE_MODE_NONE:
	case V4L2_SCENE_MODE_BACKLIGHT:
	case V4L2_SCENE_MODE_BEACH_SNOW:
	case V4L2_SCENE_MODE_CANDLE_LIGHT:
	case V4L2_SCENE_MODE_DAWN_DUSK:
	case V4L2_SCENE_MODE_FALL_COLORS:
	case V4L2_SCENE_MODE_FIREWORKS:
	case V4L2_SCENE_MODE_LANDSCAPE:
	case V4L2_SCENE_MODE_NIGHT:
	case V4L2_SCENE_MODE_PARTY_INDOOR:
	case V4L2_SCENE_MODE_PORTRAIT:
	case V4L2_SCENE_MODE_SPORTS:
	case V4L2_SCENE_MODE_SUNSET:
	case V4L2_SCENE_MODE_TEXT:
	default:
		dev_err(&client->dev, "ov2685_s_scene: %d\n", value);
		break;
	}
	return 0;
}

static int ov2685_g_wb(struct v4l2_subdev *sd, s32 *value)
{
	struct ov2685_device *dev = to_ov2685_sensor(sd);
	*value = dev->wb_mode;
	return 0;
}

static int ov2685_s_wb(struct v4l2_subdev *sd, int value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov2685_device *dev = to_ov2685_sensor(sd);

	switch (value) {
	case V4L2_WHITE_BALANCE_MANUAL:
		ov2685_write_reg_array(client, ov2685_AWB_manual);
		break;
	case V4L2_WHITE_BALANCE_AUTO:
		ov2685_write_reg_array(client, ov2685_AWB_auto);
		break;
	case V4L2_WHITE_BALANCE_INCANDESCENT:
		ov2685_write_reg_array(client, ov2685_AWB_incandescent);
		break;
	case V4L2_WHITE_BALANCE_CLOUDY:
		ov2685_write_reg_array(client, ov2685_AWB_cloudy);
		break;
	case V4L2_WHITE_BALANCE_DAYLIGHT:
		ov2685_write_reg_array(client, ov2685_AWB_sunny);
		break;
	default:
		dev_err(&client->dev, "ov2685_s_wb: %d\n", value);
	}

	dev->wb_mode = value;
	return 0;
}

static int ov2685_get_sysclk(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int sysclk;
	u16 reg_val;
	int pre_div0, pre_div2x, div_loop, sp_div, sys_div, vco;
	int pre_div2x_map[] = {2, 3, 4, 5, 6, 8, 12, 16};

	ov2685_read_reg(client, OV2685_8BIT,
			OV2685_REG_PLL_CTRL, &reg_val);
	pre_div0 = ((reg_val >> 4) & 0x01) + 1;

	ov2685_read_reg(client, OV2685_8BIT,
			OV2685_REG_PLL_PRE_DIV, &reg_val);
	reg_val &= 0x07;
	pre_div2x = pre_div2x_map[reg_val];

	ov2685_read_reg(client, OV2685_8BIT,
				OV2685_REG_PLL_MULT_H, &reg_val);
	div_loop = (reg_val & 0x01) << 8;

	ov2685_read_reg(client, OV2685_8BIT,
				OV2685_REG_PLL_MULT_L, &reg_val);
	div_loop += reg_val;

	ov2685_read_reg(client, OV2685_8BIT,
				OV2685_REG_PLL_SP_DIV, &reg_val);
	sp_div = (reg_val & 0x07) + 1;

	ov2685_read_reg(client, OV2685_8BIT,
				OV2685_REG_PLL_SYS_DIV, &reg_val);

	sys_div = (reg_val & 0x0f) + 1;

	vco = OV2685_XVCLK * div_loop * 2 / pre_div0 / pre_div2x;
	sysclk = vco / sp_div / sys_div;
	return sysclk;
}

static int ov2685_g_exposure(struct v4l2_subdev *sd, s32 *value)
{

	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u16 reg_v, reg_v2, hts, hts_v2;
	u32 exp_val, sys_clk;
	int ret;

	*value = OV2685_EXPOSURE_DEFAULT_VAL;

	/* get exposure */
	ret = ov2685_read_reg(client, OV2685_8BIT,
					OV2685_REG_EXPOSURE_2,
					&reg_v);
	if (ret)
		goto err;

	ret = ov2685_read_reg(client, OV2685_8BIT,
					OV2685_REG_EXPOSURE_1,
					&reg_v2);
	if (ret)
		goto err;

	reg_v = (reg_v >> 4) | (reg_v2 << 4);
	ret = ov2685_read_reg(client, OV2685_8BIT,
					OV2685_REG_EXPOSURE_0,
					&reg_v2);
	if (ret)
		goto err;

	ret = ov2685_read_reg(client, OV2685_8BIT,
					OV2685_REG_HTS_H,
					&hts);
	if (ret)
		goto err;

	ret = ov2685_read_reg(client, OV2685_8BIT,
					OV2685_REG_HTS_L,
					&hts_v2);
	if (ret)
		goto err;

	hts = (hts << 8) | hts_v2;

	sys_clk = ov2685_get_sysclk(sd);
	if (!sys_clk)
		return 0;

	/* transfer exposure time to us */
	exp_val = ((reg_v | (((u32)reg_v2 << 12))) * hts)  * 1000 / (sys_clk*10);

	/* FIX ME! The exposure value could be 0 in some cases*/
	if (exp_val)
		*value = exp_val;

	return 0;
err:
	return ret;
}

static long ov2685_s_exposure(struct v4l2_subdev *sd,
			       struct atomisp_exposure *exposure)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u16 hts, hts_v2, gain;
	u32 exp, exp_v2, sys_clk;
	int ret;
	/* set exposure */
	ret = ov2685_read_reg(client, OV2685_8BIT,
					OV2685_REG_HTS_H,
					&hts);
	if (ret)
		return ret;

	ret = ov2685_read_reg(client, OV2685_8BIT,
					OV2685_REG_HTS_L,
					&hts_v2);
	if (ret)
		return ret;

	hts = (hts << 8) | hts_v2;


	sys_clk = ov2685_get_sysclk(sd);
	if (!sys_clk)
		return 0;

	exp = exposure->integration_time[0] * sys_clk / 1000;

	exp_v2 = exp >> 8;
	ov2685_write_reg(client, OV2685_8BIT,
			OV2685_REG_EXPOSURE_0, exp_v2);

	exp_v2 = (exp & 0xff0) >> 4;
	ov2685_write_reg(client, OV2685_8BIT,
			OV2685_REG_EXPOSURE_1, exp_v2);

	exp_v2 = (exp & 0x0f) << 4;
	ov2685_write_reg(client, OV2685_8BIT,
			OV2685_REG_EXPOSURE_2, exp_v2);

	/* set gain */
	gain = exposure->gain[0] >> 8;
	ov2685_write_reg(client, OV2685_8BIT,
			OV2685_REG_GAIN_0, gain);

	gain =  exposure->gain[0] & 0xff;
	ov2685_write_reg(client, OV2685_8BIT,
			OV2685_REG_GAIN_0, gain);

	return 0;
}

static int ov2685_s_ev(struct v4l2_subdev *sd, int value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	switch (value) {
	case -2:
		ov2685_write_reg(client, OV2685_8BIT,
			OV2685_REG_WPT, 0x32);
		ov2685_write_reg(client, OV2685_8BIT,
			OV2685_REG_BPT, 0x28);
		break;
	case -1:
		ov2685_write_reg(client, OV2685_8BIT,
			OV2685_REG_WPT, 0x3a);
		ov2685_write_reg(client, OV2685_8BIT,
			OV2685_REG_BPT, 0x30);
		break;
	case 0:
		ov2685_write_reg(client, OV2685_8BIT,
			OV2685_REG_WPT, 0x4e);
		ov2685_write_reg(client, OV2685_8BIT,
			OV2685_REG_BPT, 0x40);
		break;
	case 1:
		ov2685_write_reg(client, OV2685_8BIT,
			OV2685_REG_WPT, 0x5a);
		ov2685_write_reg(client, OV2685_8BIT,
			OV2685_REG_BPT, 0x50);
		break;
	case 2:
		ov2685_write_reg(client, OV2685_8BIT,
			OV2685_REG_WPT, 0x62);
		ov2685_write_reg(client, OV2685_8BIT,
			OV2685_REG_BPT, 0x58);
		break;
	}

	return 0;
}

static int ov2685_g_exposure_mode(struct v4l2_subdev *sd, s32 *value)
{

	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u16 reg_v;
	int ret;

	/* get exposure mode */
	ret = ov2685_read_reg(client, OV2685_8BIT,
					OV2685_REG_EXPOSURE_AUTO,
					&reg_v);
	if (ret)
		return ret;

	*value = reg_v & OV2685_EXPOSURE_MANUAL_MASK;
	return 0;
}

static int ov2685_s_exposure_mode(struct v4l2_subdev *sd, int value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	u16 reg_v;

	ret = ov2685_read_reg(client, OV2685_8BIT,
					OV2685_REG_EXPOSURE_AUTO,
					&reg_v);
	if (ret)
		return ret;

	switch (value) {
	case V4L2_EXPOSURE_AUTO:
		reg_v &= 0xfffc;
		ov2685_write_reg(client, OV2685_8BIT,
					OV2685_REG_EXPOSURE_AUTO, reg_v);
		break;
	case V4L2_EXPOSURE_MANUAL:
		reg_v |= 0x03;
		ov2685_write_reg(client, OV2685_8BIT,
					OV2685_REG_EXPOSURE_AUTO, reg_v);
		break;
	default:
		dev_err(&client->dev,
				"Failed to set unsupported exposure mode!\n");
		return -EINVAL;
	}

	return 0;
}

static int ov2685_g_ae_lock(struct v4l2_subdev *sd, s32 *value)
{
	struct ov2685_device *dev = to_ov2685_sensor(sd);

	*value = dev->ae_lock;
	return 0;
}

static int ov2685_s_ae_lock(struct v4l2_subdev *sd, int value)
{
	struct ov2685_device *dev = to_ov2685_sensor(sd);

	dev->ae_lock = value;
	return 0;
}
static int ov2685_s_color_effect(struct v4l2_subdev *sd, int effect)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov2685_device *dev = to_ov2685_sensor(sd);
	int err = 0;

	if (dev->color_effect == effect)
		return 0;

	switch (effect) {
	case V4L2_COLORFX_NONE:
		err = ov2685_write_reg_array(client, ov2685_normal_effect);
		break;
	case V4L2_COLORFX_SEPIA:
		err = ov2685_write_reg_array(client, ov2685_sepia_effect);
		break;
	case V4L2_COLORFX_NEGATIVE:
		err = ov2685_write_reg_array(client, ov2685_negative_effect);
		break;
	case V4L2_COLORFX_BW:
		err = ov2685_write_reg_array(client, ov2685_bw_effect);
		break;
	case V4L2_COLORFX_SKY_BLUE:
		err = ov2685_write_reg_array(client, ov2685_blue_effect);
		break;
	case V4L2_COLORFX_GRASS_GREEN:
		err = ov2685_write_reg_array(client, ov2685_green_effect);
		break;
	default:
		dev_err(&client->dev, "invalid color effect.\n");
		return -ERANGE;
	}
	if (err) {
		dev_err(&client->dev, "setting color effect fails.\n");
		return err;
	}

	dev->color_effect = effect;
	return 0;
}

static int ov2685_g_color_effect(struct v4l2_subdev *sd, int *effect)
{
	struct ov2685_device *dev = to_ov2685_sensor(sd);

	*effect = dev->color_effect;

	return 0;
}

static int ov2685_s_test_pattern(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov2685_device *dev = to_ov2685_sensor(sd);

	return ov2685_write_reg(client, OV2685_8BIT, OV2685_REG_TEST_PATTERN,
		(u16)(dev->tp_mode->val));
}

static int ov2685_g_fnumber(struct v4l2_subdev *sd, s32 *value)
{
	/*const f number for imx*/
	*value = (OV2685_F_NUMBER_DEFAULT_NUM << 16) | OV2685_F_NUMBER_DEM;
	return 0;
}
static int ov2685_get_intg_factor(struct i2c_client *client,
				struct camera_mipi_info *info,
				const struct ov2685_res_struct *res)
{
	struct atomisp_sensor_mode_data *buf = &info->data;
	u16 reg_val;
	int ret;

	if (info == NULL)
		return -EINVAL;

	buf->vt_pix_clk_freq_mhz = res->pix_clk * 1000000;

	/* get integration time */
	buf->coarse_integration_time_min = OV2685_COARSE_INTG_TIME_MIN;
	buf->coarse_integration_time_max_margin =
					OV2685_COARSE_INTG_TIME_MAX_MARGIN;

	buf->fine_integration_time_min = OV2685_FINE_INTG_TIME_MIN;
	buf->fine_integration_time_max_margin =
					OV2685_FINE_INTG_TIME_MAX_MARGIN;

	buf->fine_integration_time_def = OV2685_FINE_INTG_TIME_MIN;
	buf->frame_length_lines = res->lines_per_frame;
	buf->line_length_pck = res->pixels_per_line;
	buf->read_mode = res->bin_mode;

	/* get the cropping and output resolution to ISP for this mode. */
	ret =  ov2685_read_reg(client, OV2685_16BIT,
					OV2685_REG_H_START_H, &reg_val);
	if (ret)
		return ret;
	buf->crop_horizontal_start = reg_val;

	ret =  ov2685_read_reg(client, OV2685_16BIT,
					OV2685_REG_V_START_H, &reg_val);
	if (ret)
		return ret;
	buf->crop_vertical_start = reg_val;

	ret = ov2685_read_reg(client, OV2685_16BIT,
					OV2685_REG_H_END_H, &reg_val);
	if (ret)
		return ret;
	buf->crop_horizontal_end = reg_val;

	ret = ov2685_read_reg(client, OV2685_16BIT,
					OV2685_REG_V_END_H, &reg_val);
	if (ret)
		return ret;
	buf->crop_vertical_end = reg_val;

	buf->output_width = res->width;
	buf->output_height = res->height;

	buf->binning_factor_x = res->bin_factor_x ?
					res->bin_factor_x : 1;
	buf->binning_factor_y = res->bin_factor_y ?
					res->bin_factor_y : 1;
	return 0;
}

static long ov2685_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	switch (cmd) {
	case ATOMISP_IOC_S_EXPOSURE:
		return ov2685_s_exposure(sd, arg);
	default:
		return -EINVAL;
	}
	return 0;
}

static int power_up(struct v4l2_subdev *sd)
{
	struct ov2685_device *dev = to_ov2685_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	if (NULL == dev->platform_data) {
		dev_err(&client->dev, "no camera_sensor_platform_data");
		return -ENODEV;
	}

	/* power control */
	ret = dev->platform_data->power_ctrl(sd, 1);
	if (ret)
		goto fail_power;

	/* gpio ctrl */
	ret = dev->platform_data->gpio_ctrl(sd, 1);
	if (ret)
		dev_err(&client->dev, "gpio failed\n");

	/* flis clock control */
	ret = dev->platform_data->flisclk_ctrl(sd, 1);
	if (ret)
		goto fail_clk;

	return 0;

fail_clk:
	dev->platform_data->flisclk_ctrl(sd, 0);
fail_power:
	dev->platform_data->power_ctrl(sd, 0);
	dev_err(&client->dev, "sensor power-up failed\n");

	return ret;
}

static int power_down(struct v4l2_subdev *sd)
{
	struct ov2685_device *dev = to_ov2685_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	if (NULL == dev->platform_data) {
		dev_err(&client->dev, "no camera_sensor_platform_data");
		return -ENODEV;
	}

	/* Wait for 1 frame + 512 XVCLK cycle.*/
	msleep(50);

	ret = dev->platform_data->flisclk_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "flisclk failed\n");

	/* gpio ctrl */
	ret = dev->platform_data->gpio_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "gpio failed\n");

	/* power control */
	ret = dev->platform_data->power_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "vprog failed.\n");

	return ret;
}

static int ov2685_s_power(struct v4l2_subdev *sd, int power)
{
	if (power == 0)
		return power_down(sd);

	if (power_up(sd))
		return -EINVAL;

	return 0;
}

/* ov2685 resolutions supports below 2 aspect ratio */
#define OV2685_4_3 1333 /* 4:3*//* 1600x1200*/
#define OV2585_3_2 1500 /* 3:2*//* 720x480*/
#define OV2685_16_9 1777 /* 16:9*//* 1280x720*/

static int ov2685_try_res(u32 *w, u32 *h)
{
	int i;
	/*
	 * The mode list is in ascending order. We're done as soon as
	 * we have found the first equal or bigger size.
	 */
	for (i = 0; i < N_RES; i++) {
		if (ov2685_res[i].width >= *w &&
		    ov2685_res[i].height >= *h)
			break;
	}

	/*
	 * If no mode was found, it means we can provide only a smaller size.
	 * Returning the biggest one available in this case.
	 */
	if (i == N_RES)
		i--;

	*w = ov2685_res[i].width;
	*h = ov2685_res[i].height;
	return 0;
}

static int ov2685_to_res(u32 w, u32 h)
{
	int  index;

	for (index = 0; index < N_RES; index++) {
		if (ov2685_res[index].width == w &&
		    ov2685_res[index].height == h)
			break;
	}

	/* No mode found */
	if (index >= N_RES)
		index--;

	return index;
}

static int ov2685_try_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	return ov2685_try_res(&fmt->width, &fmt->height);
}

static int ov2685_g_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	struct ov2685_device *dev = to_ov2685_sensor(sd);

	if (!fmt)
		return -EINVAL;

	fmt->width = ov2685_res[dev->fmt_idx].width;
	fmt->height = ov2685_res[dev->fmt_idx].height;
	fmt->code = V4L2_MBUS_FMT_UYVY8_1X16;

	return 0;
}

static int ov2685_s_mbus_fmt(struct v4l2_subdev *sd,
			      struct v4l2_mbus_framefmt *fmt)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov2685_device *dev = to_ov2685_sensor(sd);
	int res_index;
	struct camera_mipi_info *mipi_info = NULL;
	u32 width = fmt->width;
	u32 height = fmt->height;
	int ret;
	mipi_info = v4l2_get_subdev_hostdata(sd);
	if (mipi_info == NULL) {
		dev_err(&client->dev, "%s: can not find mipi info!!!\n",
							__func__);
		return -EINVAL;
	}

	mutex_lock(&dev->input_lock);
	ov2685_try_res(&width, &height);
	res_index = ov2685_to_res(width, height);

	switch (ov2685_res[res_index].res)  {
	/*
	case OV2685_RES_VGA:
		ret = ov2685_write_reg_array(c, ov2685_vga_init);
		break;
	*/
	case OV2685_RES_720P:
		ret = ov2685_write_reg_array(client, ov2685_720p_init);
		break;
	case OV2685_RES_2M:
		ret = ov2685_write_reg_array(client, ov2685_2M_init);
		break;
	default:
		dev_err(&client->dev, "%s: can not support the resolution!!!\n",
			__func__);

		mutex_unlock(&dev->input_lock);
		return  -EINVAL;
	}

	mipi_info->num_lanes = ov2685_res[res_index].lanes;

	ret = ov2685_get_intg_factor(client, mipi_info,
					&ov2685_res[res_index]);
	if (ret)
		dev_err(&client->dev, "failed to get integration_factor\n");

	/*
	 * ov2685 - we don't poll for context switch
	 * because it does not happen with streaming disabled.
	 */
	dev->fmt_idx = res_index;
	fmt->width = width;
	fmt->height = height;

	mutex_unlock(&dev->input_lock);
	return ret;
}

static int ov2685_detect(struct i2c_client *client,  u16 *id, u8 *revision)
{
	struct i2c_adapter *adapter = client->adapter;
	int timeout = 10;

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "%s: i2c error", __func__);
		return -ENODEV;
	}
	/* WORKAROUND: there is instable issue when reading the REG_PID,
	 * try more times to check the sensor id.
	 */
	while (timeout > 0) {
		if (ov2685_read_reg(client, OV2685_16BIT,
					OV2685_REG_PID, id)) {
			dev_err(&client->dev, "sensor id error\n");
			return -ENODEV;
		}

		if (*id == OV2685_MOD_ID)
			break;
		timeout--;
	}

	if (timeout == 0) {
		dev_err(&client->dev, "timeout to read sensor id\n");
		return -ENODEV;
	}
	/* REVISIT: HACK: Driver is currently forcing revision to 0 */
	*revision = 0;

	return 0;
}

static int
ov2685_s_config(struct v4l2_subdev *sd, int irq, void *platform_data)
{
	struct ov2685_device *dev = to_ov2685_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u8 sensor_revision;
	u16 sensor_id = 0;
	int ret;

	if (NULL == platform_data)
		return -ENODEV;

	dev->platform_data =
	    (struct camera_sensor_platform_data *)platform_data;

	mutex_lock(&dev->input_lock);

	if (dev->platform_data->platform_init) {
		ret = dev->platform_data->platform_init(client);
		if (ret) {
			dev_err(&client->dev, "platform init err\n");
			goto platform_init_failed;
		}
	}

	ret = power_down(sd);
	if (ret) {
		dev_err(&client->dev, "power_off failed");
		goto fail_power_off;
	}

	ret = power_up(sd);
	if (ret) {
		dev_err(&client->dev, "power_up failed");
		goto fail_power_on;
	}

	ret = dev->platform_data->csi_cfg(sd, 1);
	if (ret)
		goto fail_csi_cfg;

	/* config & detect sensor */
	ret = ov2685_detect(client, &sensor_id, &sensor_revision);
	if (ret) {
		dev_err(&client->dev, "ov2685_detect err s_config.\n");
		goto fail_csi_cfg;
	}

	dev->sensor_id = sensor_id;
	dev->sensor_revision = sensor_revision;

	ret = power_down(sd);
	if (ret)
		dev_err(&client->dev, "sensor power-gating failed\n");

	mutex_unlock(&dev->input_lock);
	return ret;

fail_csi_cfg:
	dev->platform_data->csi_cfg(sd, 0);
fail_power_on:
	power_down(sd);
	dev_err(&client->dev, "sensor power-gating failed\n");
fail_power_off:
	if (dev->platform_data->platform_deinit)
		dev->platform_data->platform_deinit();
platform_init_failed:
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int ov2685_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov2685_device *dev = container_of(
		ctrl->handler, struct ov2685_device, ctrl_handler);
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		dev_dbg(&client->dev, "%s: CID_EXPOSURE:%d.\n",
			__func__, ctrl->val);
		ret = ov2685_s_ev(&dev->sd, ctrl->val);
		break;
	case V4L2_CID_EXPOSURE_AUTO:
		dev_dbg(&client->dev, "%s: CID_EXPOSURE_AUTO:%d.\n",
			__func__, ctrl->val);
		ret = ov2685_s_exposure_mode(&dev->sd, ctrl->val);
		break;
	case V4L2_CID_3A_LOCK:
		dev_dbg(&client->dev, "%s: CID_3A_LOCK:%d.\n",
			__func__, ctrl->val);
		ret = ov2685_s_ae_lock(&dev->sd, ctrl->val);
		break;
	case V4L2_CID_COLORFX:
		dev_dbg(&client->dev, "%s: CID_3A_LOCK:%d.\n",
			__func__, ctrl->val);
		ret = ov2685_s_color_effect(&dev->sd, ctrl->val);
		break;
	case V4L2_CID_VFLIP:
		dev_dbg(&client->dev, "%s: CID_VFLIP:%d.\n",
			__func__, ctrl->val);
		ret = ov2685_t_vflip(&dev->sd, ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		dev_dbg(&client->dev, "%s: CID_HFLIP:%d.\n",
			__func__, ctrl->val);
		ret = ov2685_t_hflip(&dev->sd, ctrl->val);
		break;
	case V4L2_CID_POWER_LINE_FREQUENCY:
		dev_dbg(&client->dev, "%s: CID_POWER_LINE_FREQUENCY:%d.\n",
			__func__, ctrl->val);
		ret = ov2685_s_freq(&dev->sd, ctrl->val);
		break;
	case V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE:
		dev_dbg(&client->dev, "%s: CID_WHITE_BALANCE:%d.\n",
			__func__, ctrl->val);
		ret = ov2685_s_wb(&dev->sd, ctrl->val);
		break;
	case V4L2_CID_SCENE_MODE:
		dev_dbg(&client->dev, "%s: CID_SCENE_MODE:%d.\n",
			__func__, ctrl->val);
		ret = ov2685_s_scene(&dev->sd, ctrl->val);
		break;
	case V4L2_CID_TEST_PATTERN:
		dev_dbg(&client->dev, "%s: CID_CID_TEST_PATTERN:%d.\n",
			__func__, ctrl->val);
		ret = ov2685_s_test_pattern(&dev->sd);
		break;
	}

	return ret;
}

static int ov2685_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov2685_device *dev = container_of(
		ctrl->handler, struct ov2685_device, ctrl_handler);
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE_ABSOLUTE:
		ret = ov2685_g_exposure(&dev->sd, &ctrl->val);
		break;
	case V4L2_CID_EXPOSURE_AUTO:
		ret = ov2685_g_exposure_mode(&dev->sd, &ctrl->val);
		break;

	case V4L2_CID_3A_LOCK:
		ret = ov2685_g_ae_lock(&dev->sd, &ctrl->val);
		break;
	case V4L2_CID_COLORFX:
		ret = ov2685_g_color_effect(&dev->sd, &ctrl->val);
		break;
	case V4L2_CID_SCENE_MODE:
		ret = ov2685_g_scene(&dev->sd, &ctrl->val);
		break;
	case V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE:
		ret = ov2685_g_wb(&dev->sd, &ctrl->val);
		break;
	case V4L2_CID_VFLIP:
		ret = ov2685_g_vflip(&dev->sd, &ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		ret = ov2685_g_hflip(&dev->sd, &ctrl->val);
		break;
	case V4L2_CID_FNUMBER_ABSOLUTE:
		ret = ov2685_g_fnumber(&dev->sd, &ctrl->val);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static const struct v4l2_ctrl_ops ctrl_ops = {
	.s_ctrl = ov2685_s_ctrl,
	.g_volatile_ctrl = ov2685_g_volatile_ctrl
};

static const struct v4l2_ctrl_config ov2685_controls[] = {
	{
		.ops = &ctrl_ops,
		.id = V4L2_CID_EXPOSURE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "exposure biasx",
		.min = -2,
		.max = 2,
		.step = 0x01,
		.def = 0x00,
	},
	{
		.ops = &ctrl_ops,
		.id = V4L2_CID_EXPOSURE_ABSOLUTE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "exposure",
		.min = 0x0,
		.max = 0xffff,
		.step = 0x01,
		.def = 0x00,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.ops = &ctrl_ops,
		.id = V4L2_CID_EXPOSURE_AUTO,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "exposure mode",
		.min = 0,
		.max = 3,
		.step = 1,
		.def = 0,
		.flags = 0,
	},
	{
		.ops = &ctrl_ops,
		.id = V4L2_CID_3A_LOCK,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "exposure mode",
		.min = 0,
		.max = SHRT_MAX,
		.step = 1,
		.def = 0,
		.flags = 0,
	},
	{
		.ops = &ctrl_ops,
		.id = V4L2_CID_COLORFX,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "color effect",
		.min = 0,
		.max = SHRT_MAX,
		.step = 1,
		.def = 0,
		.flags = 0,
	},
/*	{
		.ops = &ctrl_ops,
		.id = V4L2_CID_HOT_PIXEL,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "hot pixel",
		.min = 0,
		.max = SHRT_MAX,
		.step = 1,
		.def = 0,
		.flags = 0,
	},*/
	{
		.ops = &ctrl_ops,
		.id = V4L2_CID_VFLIP,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Flip",
		.min = 0,
		.max = 1,
		.step = 1,
		.def = 0,
	},
	{
		.ops = &ctrl_ops,
		.id = V4L2_CID_HFLIP,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Mirror",
		.min = 0,
		.max = 1,
		.step = 1,
		.def = 0,
	},
	{
		.ops = &ctrl_ops,
		.id = V4L2_CID_HFLIP,
		.name = "Horizontal Flip",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.max = 1,
		.step = 1,
		.def = 0,
		.flags = 0,
	},
	{
		.ops = &ctrl_ops,
		.id = V4L2_CID_VFLIP,
		.name = "Vertical Flip",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.max = 1,
		.step = 1,
		.def = 0,
		.flags = 0,
	},
	{
		.ops = &ctrl_ops,
		.id = V4L2_CID_POWER_LINE_FREQUENCY,
		.name = "frequency",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.max = SHRT_MAX,
		.step = 1,
		.def = 0,
		.flags = 0,
	},
	{
		.ops = &ctrl_ops,
		.id = V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE,
		.name = "white balance",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.max = SHRT_MAX,
		.step = 1,
		.def = 0,
		.flags = 0,
	},
	{
		.ops = &ctrl_ops,
		.id = V4L2_CID_SCENE_MODE,
		.name = "Vertical Flip",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.max = SHRT_MAX,
		.step = 1,
		.def = 0,
		.flags = 0,
	},
	{
		.ops = &ctrl_ops,
		.id = V4L2_CID_FNUMBER_ABSOLUTE,
		.name = "focal number",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = OV2685_F_NUMBER_DEFAULT,
		.max = OV2685_F_NUMBER_DEFAULT,
		.step = 1,
		.def = OV2685_F_NUMBER_DEFAULT,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	},
};

static int ov2685_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov2685_device *dev = to_ov2685_sensor(sd);
	int ret;

	mutex_lock(&dev->input_lock);
	dev->streaming = enable;
	ret = ov2685_write_reg(client, OV2685_8BIT,
			0x301c,
			enable ? 0xf0 : 0xf4);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int
ov2685_enum_framesizes(struct v4l2_subdev *sd, struct v4l2_frmsizeenum *fsize)
{
	unsigned int index = fsize->index;

	if (index >= N_RES)
		return -EINVAL;

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = ov2685_res[index].width;
	fsize->discrete.height = ov2685_res[index].height;

	fsize->reserved[0] = ov2685_res[index].used;

	return 0;
}

static int ov2685_enum_frameintervals(struct v4l2_subdev *sd,
				       struct v4l2_frmivalenum *fival)
{
	unsigned int index = fival->index;

	if (index >= N_RES)
		return -EINVAL;

	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fival->discrete.numerator = 1;
	fival->discrete.denominator = ov2685_res[index].fps;

	return 0;
}

static int
ov2685_g_chip_ident(struct v4l2_subdev *sd, struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_OV2685, 0);
}

static int ov2685_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_fh *fh,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index)
		return -EINVAL;
	code->code = V4L2_MBUS_FMT_UYVY8_1X16;

	return 0;
}

static int ov2685_enum_frame_size(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh,
	struct v4l2_subdev_frame_size_enum *fse)
{
	unsigned int index = fse->index;

	if (index >= N_RES)
		return -EINVAL;

	fse->min_width = ov2685_res[index].width;
	fse->min_height = ov2685_res[index].height;
	fse->max_width = ov2685_res[index].width;
	fse->max_height = ov2685_res[index].height;

	return 0;
}

static int
ov2685_get_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct ov2685_device *snr = to_ov2685_sensor(sd);

	switch (fmt->which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		fmt->format = *v4l2_subdev_get_try_format(fh, fmt->pad);
		break;
	case V4L2_SUBDEV_FORMAT_ACTIVE:
		fmt->format = snr->format;
	}

	return 0;
}

static int
ov2685_set_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct ov2685_device *snr = to_ov2685_sensor(sd);

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		snr->format = fmt->format;

	return 0;
}

/* set focus zone */
static int
ov2685_set_selection(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			     struct v4l2_subdev_selection *sel)
{
	return 0;
}

static int ov2685_g_parm(struct v4l2_subdev *sd,
			struct v4l2_streamparm *param)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!param)
		return -EINVAL;

	if (param->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		dev_err(&client->dev,  "unsupported buffer type.\n");
		return -EINVAL;
	}

	memset(param, 0, sizeof(*param));
	param->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	return 0;
}

static int ov2685_s_parm(struct v4l2_subdev *sd,
			struct v4l2_streamparm *param)
{
	struct ov2685_device *dev = to_ov2685_sensor(sd);

	mutex_lock(&dev->input_lock);
	dev->run_mode = param->parm.capture.capturemode;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static int ov2685_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *interval)
{
	struct ov2685_device *dev = to_ov2685_sensor(sd);

	interval->interval.numerator = 1;
	interval->interval.denominator = ov2685_res[dev->fmt_idx].fps;

	return 0;
}

static int
ov2685_g_skip_frames(struct v4l2_subdev *sd, u32 *frames)
{
	struct ov2685_device *dev = to_ov2685_sensor(sd);
	*frames = ov2685_res[dev->fmt_idx].skip_frames;
	return 0;
}

static const struct v4l2_subdev_video_ops ov2685_video_ops = {
	.try_mbus_fmt = ov2685_try_mbus_fmt,
	.g_mbus_fmt = ov2685_g_mbus_fmt,
	.s_mbus_fmt = ov2685_s_mbus_fmt,
	.s_parm = ov2685_s_parm,
	.g_parm = ov2685_g_parm,
	.s_stream = ov2685_s_stream,
	.enum_framesizes = ov2685_enum_framesizes,
	.enum_frameintervals = ov2685_enum_frameintervals,
	.g_frame_interval = ov2685_g_frame_interval,
};

static const struct v4l2_subdev_sensor_ops ov2685_sensor_ops = {
	.g_skip_frames	= ov2685_g_skip_frames,
};

static const struct v4l2_subdev_core_ops ov2685_core_ops = {
#ifndef CONFIG_GMIN_INTEL_MID /* FIXME! for non-gmin*/
	.g_chip_ident = ov2685_g_chip_ident,
#endif
	.queryctrl = v4l2_subdev_queryctrl,
	.g_ctrl = v4l2_subdev_g_ctrl,
	.s_ctrl = v4l2_subdev_s_ctrl,
	.s_power = ov2685_s_power,
	.ioctl = ov2685_ioctl,
};
static const struct v4l2_subdev_pad_ops ov2685_pad_ops = {
	.enum_mbus_code = ov2685_enum_mbus_code,
	.enum_frame_size = ov2685_enum_frame_size,
	.get_fmt = ov2685_get_pad_format,
	.set_fmt = ov2685_set_pad_format,
	.set_selection = ov2685_set_selection,
};

static const struct v4l2_subdev_ops ov2685_ops = {
	.core = &ov2685_core_ops,
	.video = &ov2685_video_ops,
	.sensor = &ov2685_sensor_ops,
	.pad = &ov2685_pad_ops,
};

static const struct media_entity_operations ov2685_entity_ops = {
	.link_setup = NULL,
};

static int ov2685_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov2685_device *dev = container_of(sd,
					struct ov2685_device, sd);

	dev->platform_data->csi_cfg(sd, 0);

	release_firmware(dev->firmware);
	v4l2_device_unregister_subdev(sd);
	media_entity_cleanup(&dev->sd.entity);
	kfree(dev);

	return 0;
}

static int __ov2685_init_ctrl_handler(struct ov2685_device *dev)
{
	struct v4l2_ctrl_handler *hdl;
	int i;

	hdl = &dev->ctrl_handler;

	v4l2_ctrl_handler_init(&dev->ctrl_handler, ARRAY_SIZE(ov2685_controls));

	for (i = 0; i < ARRAY_SIZE(ov2685_controls); i++)
		v4l2_ctrl_new_custom(&dev->ctrl_handler,
				&ov2685_controls[i], NULL);

	dev->tp_mode = v4l2_ctrl_find(&dev->ctrl_handler,
				V4L2_CID_TEST_PATTERN);

	if (dev->ctrl_handler.error)
		return dev->ctrl_handler.error;

	dev->ctrl_handler.lock = &dev->input_lock;
	dev->sd.ctrl_handler = hdl;
	v4l2_ctrl_handler_setup(&dev->ctrl_handler);

	return 0;
}

static int ov2685_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov2685_device *dev = container_of(sd,
					struct ov2685_device, sd);
	int ret;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&client->dev, "out of memory\n");
		return -ENOMEM;
	}

	mutex_init(&dev->input_lock);

	v4l2_i2c_subdev_init(&(dev->sd), client, &ov2685_ops);
	if (client->dev.platform_data) {
		ret = ov2685_s_config(&dev->sd, client->irq,
				       client->dev.platform_data);
		if (ret)
			goto out_free;
	}

	ret = __ov2685_init_ctrl_handler(dev);
	if (ret)
		goto out_ctrl_handler_free;

	dev->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->pad.flags = MEDIA_PAD_FL_SOURCE;
	dev->format.code = V4L2_MBUS_FMT_UYVY8_1X16;
	dev->sd.entity.ops = &ov2685_entity_ops;
	dev->sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
	dev->ae_lock = 0;
	dev->color_effect = 0;
	dev->hot_pixel = 0;
	ret = media_entity_init(&dev->sd.entity, 1, &dev->pad, 0);
	if (ret)
		ov2685_remove(client);

	return ret;

out_ctrl_handler_free:
	v4l2_ctrl_handler_free(&dev->ctrl_handler);

out_free:
	v4l2_device_unregister_subdev(&dev->sd);
	kfree(dev);
	return ret;
}

MODULE_DEVICE_TABLE(i2c, ov2685_id);
static struct i2c_driver ov2685_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = OV2685_NAME,
	},
	.probe = ov2685_probe,
	.remove = __exit_p(ov2685_remove),
	.id_table = ov2685_id,
};

static __init int ov2685_init_mod(void)
{
	return i2c_add_driver(&ov2685_driver);
}

static __exit void ov2685_exit_mod(void)
{
	i2c_del_driver(&ov2685_driver);
}

module_init(ov2685_init_mod);
module_exit(ov2685_exit_mod);

MODULE_DESCRIPTION("A low-level driver for Omnivision OV2685 sensors");
MODULE_LICENSE("GPL");
