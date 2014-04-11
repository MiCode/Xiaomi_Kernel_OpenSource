/*
 * Support for OV9724 720P camera sensor.
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <linux/bitops.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/kmod.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-device.h>

#include "ov9724.h"

/* the bayer order mapping table
 *          hflip=0                  hflip=1
 * vflip=0  atomisp_bayer_order_bggr atomisp_bayer_order_gbrg
 * vflip=1  atomisp_bayer_order_grbg atomisp_bayer_order_rggb
 *
 * usage: ov9724_bayer_order_mapping[vflip][hflip]
 */
static const int ov9724_bayer_order_mapping[2][2] = {
	{atomisp_bayer_order_bggr, atomisp_bayer_order_gbrg},
	{atomisp_bayer_order_grbg, atomisp_bayer_order_rggb}
};

static int
ov9724_read_reg(struct i2c_client *client, u16 len, u16 reg, u16 *val)
{
	struct i2c_msg msg[2];
	u16 data[OV9724_SHORT_MAX] = {0};
	int err, i;

	if (len > OV9724_BYTE_MAX) {
		v4l2_err(client, "%s error, invalid data length\n", __func__);
		return -EINVAL;
	}

	memset(msg, 0 , sizeof(msg));

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = I2C_MSG_LENGTH;
	msg[0].buf = (u8 *)data;
	/* high byte goes first */
	data[0] = cpu_to_be16(reg);

	msg[1].addr = client->addr;
	msg[1].len = len;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = (u8 *)data;

	err = i2c_transfer(client->adapter, msg, 2);
	if (err != 2) {
		if (err >= 0)
			err = -EIO;
		goto error;
	}

	/* high byte comes first */
	if (len == OV9724_8BIT)
		*val = (u8)data[0];
	else {
		/* 16-bit access is default when len > 1 */
		for (i = 0; i < (len >> 1); i++)
			val[i] = be16_to_cpu(data[i]);
	}

	return 0;

error:
	dev_err(&client->dev, "read from offset 0x%x error %d", reg, err);
	return err;
}

static int ov9724_i2c_write(struct i2c_client *client, u16 len, u8 *data)
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

static int
ov9724_write_reg(struct i2c_client *client, u16 data_length, u16 reg, u16 val)
{
	int ret;
	unsigned char data[4] = {0};
	u16 *wreg = (u16 *)data;
	const u16 len = data_length + sizeof(u16); /* 16-bit address + data */

	if (data_length != OV9724_8BIT && data_length != OV9724_16BIT) {
		v4l2_err(client, "%s error, invalid data_length\n", __func__);
		return -EINVAL;
	}

	/* high byte goes out first */
	*wreg = cpu_to_be16(reg);

	if (data_length == OV9724_8BIT) {
		data[2] = (u8)(val);
	} else {
		/* OV9724_16BIT */
		u16 *wdata = (u16 *)&data[2];
		*wdata = cpu_to_be16(val);
	}

	ret = ov9724_i2c_write(client, len, data);
	if (ret)
		dev_err(&client->dev,
			"write error: wrote 0x%x to offset 0x%x error %d",
			val, reg, ret);

	return ret;
}

/*
 * ov9724_write_reg_array - Initializes a list of OV9724 registers
 * @client: i2c driver client structure
 * @reglist: list of registers to be written
 *
 * This function initializes a list of registers. When consecutive addresses
 * are found in a row on the list, this function creates a buffer and sends
 * consecutive data in a single i2c_transfer().
 *
 * __ov9724_flush_reg_array, __ov9724_buf_reg_array() and
 * __ov9724_write_reg_is_consecutive() are internal functions to
 * ov9724_write_reg_array_fast() and should be not used anywhere else.
 *
 */

static int __ov9724_flush_reg_array(struct i2c_client *client,
				     struct ov9724_write_ctrl *ctrl)
{
	u16 size;

	if (ctrl->index == 0)
		return 0;

	size = sizeof(u16) + ctrl->index; /* 16-bit address + data */
	ctrl->buffer.addr = cpu_to_be16(ctrl->buffer.addr);
	ctrl->index = 0;

	return ov9724_i2c_write(client, size, (u8 *)&ctrl->buffer);
}

static int __ov9724_buf_reg_array(struct i2c_client *client,
				   struct ov9724_write_ctrl *ctrl,
				   const struct ov9724_reg *next)
{
	int size;
	u16 *data16;

	switch (next->type) {
	case OV9724_8BIT:
		size = 1;
		ctrl->buffer.data[ctrl->index] = (u8)next->val;
		break;
	case OV9724_16BIT:
		size = 2;
		data16 = (u16 *)&ctrl->buffer.data[ctrl->index];
		*data16 = (u16)next->val;
		break;
	default:
		return -EINVAL;
	}

	/* When first item is added, we need to store its starting address */
	if (ctrl->index == 0)
		ctrl->buffer.addr = next->reg.sreg;

	ctrl->index += size;

	/*
	 * Buffer cannot guarantee free space for u32? Better flush it to avoid
	 * possible lack of memory for next item.
	 */
	if (ctrl->index + sizeof(u16) >= OV9724_MAX_WRITE_BUF_SIZE)
		return __ov9724_flush_reg_array(client, ctrl);

	return 0;
}

static int
__ov9724_write_reg_is_consecutive(struct i2c_client *client,
				   struct ov9724_write_ctrl *ctrl,
				   const struct ov9724_reg *next)
{
	if (ctrl->index == 0)
		return 1;

	return ctrl->buffer.addr + ctrl->index == next->reg.sreg;
}

static int ov9724_write_reg_array(struct i2c_client *client,
				   const struct ov9724_reg *reglist)
{
	const struct ov9724_reg *next = reglist;
	struct ov9724_write_ctrl ctrl;
	int err;

	ctrl.index = 0;
	for (; next->type != OV9724_TOK_TERM; next++) {
		switch (next->type & OV9724_TOK_MASK) {
		case OV9724_TOK_DELAY:
			err = __ov9724_flush_reg_array(client, &ctrl);
			if (err)
				return err;
			msleep(next->val);
			break;
		default:
			/*
			 * If next address is not consecutive, data needs to be
			 * flushed before proceed.
			 */
			if (!__ov9724_write_reg_is_consecutive(client, &ctrl,
								next)) {
				err = __ov9724_flush_reg_array(client, &ctrl);
				if (err)
					return err;
			}
			err = __ov9724_buf_reg_array(client, &ctrl, next);
			if (err) {
				v4l2_err(client, "%s: write error, aborted\n",
					 __func__);
				return err;
			}
			break;
		}
	}

	return __ov9724_flush_reg_array(client, &ctrl);
}

static int __ov9724_init(struct v4l2_subdev *sd, u32 val)
{
	int ret;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	ret = ov9724_write_reg_array(client, ov9724_init_config);
	if (ret)
		return ret;

	/* restore settings */
	ov9724_res = ov9724_res_preview;
	N_RES = N_RES_PREVIEW;

	return 0;
}

static int ov9724_init(struct v4l2_subdev *sd, u32 val)
{
	struct ov9724_device *dev = to_ov9724_sensor(sd);
	int ret = 0;

	mutex_lock(&dev->input_lock);
	ret = __ov9724_init(sd, val);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static void ov9724_uninit(struct v4l2_subdev *sd)
{
	struct ov9724_device *dev = to_ov9724_sensor(sd);

	dev->coarse_itg = 0;
	dev->fine_itg   = 0;
	dev->gain       = 0;
}

static int power_up(struct v4l2_subdev *sd)
{
	struct ov9724_device *dev = to_ov9724_sensor(sd);
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

	/* flis clock control */
	ret = dev->platform_data->flisclk_ctrl(sd, 1);
	if (ret)
		goto fail_clk;

	/* gpio ctrl */
	ret = dev->platform_data->gpio_ctrl(sd, 1);
	if (ret) {
		dev_err(&client->dev, "gpio failed 1\n");
		goto fail_gpio;
	}

	return 0;
fail_gpio:
	dev->platform_data->gpio_ctrl(sd, 0);
fail_clk:
	dev->platform_data->flisclk_ctrl(sd, 0);
fail_power:
	dev->platform_data->power_ctrl(sd, 0);
	dev_err(&client->dev, "sensor power-up failed\n");

	return ret;
}

static int power_down(struct v4l2_subdev *sd)
{
	struct ov9724_device *dev = to_ov9724_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	if (NULL == dev->platform_data) {
		dev_err(&client->dev, "no camera_sensor_platform_data");
		return -ENODEV;
	}

	ret = dev->platform_data->flisclk_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "flisclk failed\n");

	/* gpio ctrl */
	ret = dev->platform_data->gpio_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "gpio failed 1\n");

	/* power control */
	ret = dev->platform_data->power_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "vprog failed.\n");

	return ret;
}

static int __ov9724_s_power(struct v4l2_subdev *sd, int power)
{

	if (power == 0) {
		ov9724_uninit(sd);
		return power_down(sd);
	} else {
		if (power_up(sd))
			return -EINVAL;
		return __ov9724_init(sd, 0);
	}
}

static int ov9724_s_power(struct v4l2_subdev *sd, int on)
{
	int ret;
	struct ov9724_device *dev = to_ov9724_sensor(sd);

	mutex_lock(&dev->input_lock);
	ret = __ov9724_s_power(sd, on);
	mutex_unlock(&dev->input_lock);

	return ret;
}

/* This returns the exposure time being used. This should only be used
   for filling in EXIF data, not for actual image processing. */
static int ov9724_q_exposure(struct v4l2_subdev *sd, s32 *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u16 coarse;
	int ret;

	/* the fine integration time is currently not calculated */
	ret = ov9724_read_reg(client, OV9724_16BIT,
			       OV9724_COARSE_INTEGRATION_TIME, &coarse);
	*value = coarse;

	return ret;
}

static int ov9724_get_intg_factor(struct i2c_client *client,
				struct camera_mipi_info *info)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov9724_device *dev = to_ov9724_sensor(sd);
	int ret;
	u32	vt_pix_clk_div;
	u32	vt_sys_clk_div;
	u32	pre_pll_clk_div;
	u32	pll_multiplier;
	u32	op_pix_clk_div;
	u32	op_sys_clk_div;

	const int ext_clk_freq_hz = 19200000;
	struct atomisp_sensor_mode_data *buf = &info->data;
	int vt_pix_clk_freq_mhz;
	u16 data[OV9724_INTG_BUF_COUNT];

	u32 coarse_integration_time_min;
	u32 coarse_integration_time_max_margin;
	u32 fine_integration_time_min;
	u32 fine_integration_time_max_margin;
	u32 frame_length_lines;
	u32 line_length_pck;
	u32 read_mode;
	u32 div;
	u16 tmp;

	if (info == NULL)
		return -EINVAL;

	memset(data, 0, OV9724_INTG_BUF_COUNT * sizeof(u16));
	ret = ov9724_read_reg(client, 2, OV9724_VT_PIX_CLK_DIV, data);
	if (ret)
		return ret;
	ret = ov9724_read_reg(client, 1, OV9724_VT_PIX_CLK_DIV, &tmp);
	if (ret)
		return ret;

	vt_pix_clk_div = data[0];
	ret = ov9724_read_reg(client, 2, OV9724_VT_SYS_CLK_DIV, data);
	if (ret)
		return ret;
	vt_sys_clk_div = data[0];
	ret = ov9724_read_reg(client, 2, OV9724_PRE_PLL_CLK_DIV, data);
	if (ret)
		return ret;
	pre_pll_clk_div = data[0];
	ret = ov9724_read_reg(client, 2, OV9724_PLL_MULTIPLIER, data);
	if (ret)
		return ret;
	pll_multiplier = data[0];

	ret = ov9724_read_reg(client, 2, OV9724_OP_PIX_DIV, data);
	if (ret)
		return ret;
	op_pix_clk_div = data[0];
	ret = ov9724_read_reg(client, 2, OV9724_OP_SYS_DIV, data);
	if (ret)
		return ret;
	op_sys_clk_div = data[0];

	memset(data, 0, OV9724_INTG_BUF_COUNT * sizeof(u16));
	ret = ov9724_read_reg(client, 4, OV9724_FRAME_LENGTH_LINES, data);
	if (ret)
		return ret;
	ret = ov9724_read_reg(client, 1, OV9724_FRAME_LENGTH_LINES, &tmp);
	if (ret)
		return ret;

	frame_length_lines = data[0];
	line_length_pck = data[1];

	memset(data, 0, OV9724_INTG_BUF_COUNT * sizeof(u16));
	ret = ov9724_read_reg(client, 4, OV9724_COARSE_INTG_TIME_MIN, data);
	if (ret)
		return ret;
	coarse_integration_time_min = data[0];
	coarse_integration_time_max_margin = data[1];

	memset(data, 0, OV9724_INTG_BUF_COUNT * sizeof(u16));
	ret = ov9724_read_reg(client, 4, OV9724_FINE_INTG_TIME_MIN, data);
	if (ret)
		return ret;
	fine_integration_time_min = data[0];
	fine_integration_time_max_margin = data[1];

	memset(data, 0, OV9724_INTG_BUF_COUNT * sizeof(u16));
	ret = ov9724_read_reg(client, 2, OV9724_READ_MODE, data);
	if (ret)
		return ret;
	read_mode = data[0];

	div = pre_pll_clk_div*vt_sys_clk_div*vt_pix_clk_div;
	if (div == 0)
		return -EINVAL;
	vt_pix_clk_freq_mhz = ext_clk_freq_hz*pll_multiplier/div;

	dev->vt_pix_clk_freq_mhz = vt_pix_clk_freq_mhz;
	buf->coarse_integration_time_min = coarse_integration_time_min;
	buf->coarse_integration_time_max_margin
		= coarse_integration_time_max_margin;
	buf->fine_integration_time_min = fine_integration_time_min;
	buf->fine_integration_time_max_margin =
					fine_integration_time_max_margin;
	buf->fine_integration_time_def = fine_integration_time_max_margin;
	buf->vt_pix_clk_freq_mhz = vt_pix_clk_freq_mhz;
	buf->line_length_pck = line_length_pck;
	buf->frame_length_lines = frame_length_lines;
	buf->read_mode = read_mode;

	buf->binning_factor_x = ov9724_res[dev->fmt_idx].bin_factor_x ? 2 : 1;
	buf->binning_factor_y = ov9724_res[dev->fmt_idx].bin_factor_y ? 2 : 1;

	/* Get the cropping and output resolution to ISP for this mode. */
	ret =  ov9724_read_reg(client, 2, OV9724_HORIZONTAL_START_H, data);
	if (ret)
		return ret;
	buf->crop_horizontal_start = data[0];

	ret = ov9724_read_reg(client, 2, OV9724_VERTICAL_START_H, data);
	if (ret)
		return ret;
	buf->crop_vertical_start = data[0];

	ret = ov9724_read_reg(client, 2, OV9724_HORIZONTAL_END_H, data);
	if (ret)
		return ret;
	buf->crop_horizontal_end = data[0];

	ret = ov9724_read_reg(client, 2, OV9724_VERTICAL_END_H, data);
	if (ret)
		return ret;
	buf->crop_vertical_end = data[0];

	ret = ov9724_read_reg(client, 2, OV9724_HORIZONTAL_OUTPUT_SIZE_H, data);
	if (ret)
		return ret;
	buf->output_width = data[0];

	ret = ov9724_read_reg(client, 2, OV9724_VERTICAL_OUTPUT_SIZE_H, data);
	if (ret)
		return ret;
	buf->output_height = data[0];

	return 0;
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
#define LARGEST_ALLOWED_RATIO_MISMATCH 140
static int distance(struct ov9724_resolution *res, u32 w, u32 h)
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
	struct ov9724_resolution *tmp_res = NULL;

	for (i = 0; i < N_RES; i++) {
		tmp_res = &ov9724_res[i];
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
		if (w != ov9724_res[i].width)
			continue;
		if (h != ov9724_res[i].height)
			continue;
		/* Found it */
		return i;
	}
	return -1;
}

static int ov9724_try_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	int idx;

	if ((fmt->width > OV9724_RES_WIDTH_MAX)
		|| (fmt->height > OV9724_RES_HEIGHT_MAX)) {
		fmt->width = OV9724_RES_WIDTH_MAX;
		fmt->height = OV9724_RES_HEIGHT_MAX;
	} else {
		idx = nearest_resolution_index(fmt->width, fmt->height);

		/*
		 * nearest_resolution_index() doesn't return smaller
		 *  resolutions. If it fails, it means the requested
		 *  resolution is higher than wecan support. Fallback
		 *  to highest possible resolution in this case.
		 */
		if (idx == -1)
			idx = N_RES - 1;

		fmt->width = ov9724_res[idx].width;
		fmt->height = ov9724_res[idx].height;
	}

	fmt->code = V4L2_MBUS_FMT_SBGGR10_1X10;


	return 0;
}

static int ov9724_set_mbus_fmt(struct v4l2_subdev *sd,
			      struct v4l2_mbus_framefmt *fmt)
{
	struct ov9724_device *dev = to_ov9724_sensor(sd);
	const struct ov9724_reg *ov9724_def_reg;
	struct camera_mipi_info *ov9724_info = NULL;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	u8 tmp;
	int vflip, hflip;

	ov9724_info = v4l2_get_subdev_hostdata(sd);
	if (ov9724_info == NULL)
		return -EINVAL;

	ret = ov9724_try_mbus_fmt(sd, fmt);
	if (ret) {
		v4l2_err(sd, "try fmt fail\n");
		return ret;
	}

	mutex_lock(&dev->input_lock);
	dev->fmt_idx = get_resolution_index(fmt->width, fmt->height);

	/* Sanity check */
	if (unlikely(dev->fmt_idx == -1)) {
		mutex_unlock(&dev->input_lock);
		v4l2_err(sd, "get resolution fail\n");
		return -EINVAL;
	}

	ov9724_def_reg = ov9724_res[dev->fmt_idx].regs;
	/* enable group hold */
	ret = ov9724_write_reg_array(client, ov9724_param_hold);
	if (ret) {
		mutex_unlock(&dev->input_lock);
		return ret;
	}

	ret = ov9724_write_reg_array(client, ov9724_def_reg);
	if (ret) {
		mutex_unlock(&dev->input_lock);
		return ret;
	}

	/* disable group hold */
	ret = ov9724_write_reg_array(client, ov9724_param_update);
	if (ret) {
		mutex_unlock(&dev->input_lock);
		return ret;
	}
	dev->fps = ov9724_res[dev->fmt_idx].fps;
	dev->pixels_per_line = ov9724_res[dev->fmt_idx].pixels_per_line;
	dev->lines_per_frame = ov9724_res[dev->fmt_idx].lines_per_frame;
	dev->coarse_itg = 0;
	dev->fine_itg = 0;
	dev->gain = 0;

	ret = ov9724_read_reg(client, OV9724_8BIT,
				OV9724_IMG_ORIENTATION, &tmp);
	if (ret) {
		mutex_unlock(&dev->input_lock);
		return ret;
	}
	hflip = tmp & OV9724_HFLIP_BIT;
	vflip = (tmp & OV9724_VFLIP_BIT) >> OV9724_VFLIP_OFFSET;
	ov9724_info->raw_bayer_order =
		ov9724_bayer_order_mapping[vflip][hflip];

	ret = ov9724_get_intg_factor(client, ov9724_info);
	mutex_unlock(&dev->input_lock);
	if (ret) {
		v4l2_err(sd, "failed to get integration_factor\n");
		return -EINVAL;
	}
	return 0;
}

static int ov9724_g_mbus_fmt(struct v4l2_subdev *sd,
			      struct v4l2_mbus_framefmt *fmt)
{
	struct ov9724_device *dev = to_ov9724_sensor(sd);

	if (!fmt)
		return -EINVAL;

	fmt->width = ov9724_res[dev->fmt_idx].width;
	fmt->height = ov9724_res[dev->fmt_idx].height;
	fmt->code = V4L2_MBUS_FMT_SBGGR10_1X10;

	return 0;
}

static int ov9724_g_focal(struct v4l2_subdev *sd, s32 *val)
{

	*val = (OV9724_FOCAL_LENGTH_NUM << 16) | OV9724_FOCAL_LENGTH_DEM;
	return 0;
}

static int ov9724_g_fnumber(struct v4l2_subdev *sd, s32 *val)
{
	/*const f number for ov9724*/
	*val = (OV9724_F_NUMBER_DEFAULT_NUM << 16) | OV9724_F_NUMBER_DEM;
	return 0;
}

static int ov9724_g_fnumber_range(struct v4l2_subdev *sd, s32 *val)
{

	*val = (OV9724_F_NUMBER_DEFAULT_NUM << 24) |
		(OV9724_F_NUMBER_DEM << 16) |
		(OV9724_F_NUMBER_DEFAULT_NUM << 8) | OV9724_F_NUMBER_DEM;
	return 0;
}

/* Horizontal flip the image. */
static int ov9724_t_hflip(struct v4l2_subdev *sd, int value)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	int ret;
	u16 val;

	/* enable group hold */
	ret = ov9724_write_reg_array(c, ov9724_param_hold);

	ret = ov9724_read_reg(c, OV9724_8BIT, OV9724_IMG_ORIENTATION, &val);
	if (ret)
		return ret;
	if (value)
		val |= OV9724_HFLIP_BIT;
	else
		val &= ~OV9724_HFLIP_BIT;
	ret = ov9724_write_reg(c, OV9724_8BIT, OV9724_IMG_ORIENTATION, val);
	if (ret)
		return ret;

	ret = ov9724_write_reg_array(c, ov9724_param_update);

	return ret;
}

/* Vertically flip the image */
static int ov9724_t_vflip(struct v4l2_subdev *sd, int value)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	int ret;
	u16 val;

	/* enable group hold */
	ret = ov9724_write_reg_array(c, ov9724_param_hold);

	ret = ov9724_read_reg(c, OV9724_8BIT, OV9724_IMG_ORIENTATION, &val);
	if (ret)
		return ret;
	if (value)
		val |= OV9724_VFLIP_BIT;
	else
		val &= ~OV9724_VFLIP_BIT;
	ret = ov9724_write_reg(c, OV9724_8BIT, OV9724_IMG_ORIENTATION, val);
	if (ret)
		return ret;

	ret = ov9724_write_reg_array(c, ov9724_param_update);
	return ret;
}

static int ov9724_test_pattern(struct v4l2_subdev *sd, s32 value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return ov9724_write_reg(client, OV9724_8BIT,
			OV9724_TEST_PATTERN_MODE, value);
}

static int ov9724_g_bin_factor_x(struct v4l2_subdev *sd, s32 *val)
{
	struct ov9724_device *dev = to_ov9724_sensor(sd);

	*val = ov9724_res[dev->fmt_idx].bin_factor_x;

	return 0;
}

static int ov9724_g_bin_factor_y(struct v4l2_subdev *sd, s32 *val)
{
	struct ov9724_device *dev = to_ov9724_sensor(sd);

	*val = ov9724_res[dev->fmt_idx].bin_factor_y;

	return 0;
}


static struct ov9724_control ov9724_controls[] = {
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
		.query = ov9724_q_exposure,
	},
	{
		.qc = {
			.id = V4L2_CID_VFLIP,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "Image v-Flip",
			.minimum = 0,
			.maximum = 1,
			.step = 1,
			.default_value = 0,
		},
		.tweak = ov9724_t_vflip,
	},
	{
		.qc = {
			.id = V4L2_CID_HFLIP,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "Image h-Flip",
			.minimum = 0,
			.maximum = 1,
			.step = 1,
			.default_value = 0,
		},
		.tweak = ov9724_t_hflip,
	},
	{
		.qc = {
			.id = V4L2_CID_TEST_PATTERN,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "Test pattern",
			.minimum = 0,
			.maximum = 0xffff,
			.step = 1,
			.default_value = 0,
		},
		.tweak = ov9724_test_pattern,
	},
	{
		.qc = {
			.id = V4L2_CID_FOCAL_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focal length",
			.minimum = OV9724_FOCAL_LENGTH_DEFAULT,
			.maximum = OV9724_FOCAL_LENGTH_DEFAULT,
			.step = 0x01,
			.default_value = OV9724_FOCAL_LENGTH_DEFAULT,
			.flags = 0,
		},
		.query = ov9724_g_focal,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number",
			.minimum = OV9724_F_NUMBER_DEFAULT,
			.maximum = OV9724_F_NUMBER_DEFAULT,
			.step = 0x01,
			.default_value = OV9724_F_NUMBER_DEFAULT,
			.flags = 0,
		},
		.query = ov9724_g_fnumber,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_RANGE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number range",
			.minimum = OV9724_F_NUMBER_RANGE,
			.maximum =  OV9724_F_NUMBER_RANGE,
			.step = 0x01,
			.default_value = OV9724_F_NUMBER_RANGE,
			.flags = 0,
		},
		.query = ov9724_g_fnumber_range,
	},
	{
		.qc = {
			.id = V4L2_CID_BIN_FACTOR_HORZ,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "horizontal binning factor",
			.minimum = 0,
			.maximum = OV9724_BIN_FACTOR_MAX,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.query = ov9724_g_bin_factor_x,
	},
	{
		.qc = {
			.id = V4L2_CID_BIN_FACTOR_VERT,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "vertical binning factor",
			.minimum = 0,
			.maximum = OV9724_BIN_FACTOR_MAX,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.query = ov9724_g_bin_factor_y,
	},
};
#define N_CONTROLS (ARRAY_SIZE(ov9724_controls))

static long __ov9724_set_exposure(struct v4l2_subdev *sd, u16 coarse_itg,
				 u16 gain)

{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	struct ov9724_device *dev = to_ov9724_sensor(sd);

	/* enable group hold */
	ret = ov9724_write_reg_array(client, ov9724_param_hold);
	if (ret)
		goto out;

	/* set coarse integration time */
	if (coarse_itg > (dev->lines_per_frame - 5))
		ov9724_write_reg(client, OV9724_16BIT,
			OV9724_FRAME_LENGTH_LINES, coarse_itg + 5);
	else
		ov9724_write_reg(client, OV9724_16BIT,
			OV9724_FRAME_LENGTH_LINES, dev->lines_per_frame);

	ret = ov9724_write_reg(client, OV9724_16BIT,
			OV9724_COARSE_INTEGRATION_TIME, coarse_itg);
	if (ret)
		goto out_disable;

	/* set global gain */
	ret = ov9724_write_reg(client, OV9724_8BIT,
			OV9724_GLOBAL_GAIN, gain);
	if (ret)
		goto out_disable;
	dev->gain       = gain;
	dev->coarse_itg = coarse_itg;

out_disable:
	/* disable group hold */
	ov9724_write_reg_array(client, ov9724_param_update);
out:
	return ret;
}

static int ov9724_set_exposure(struct v4l2_subdev *sd, u16 exposure, u16 gain)
{
	struct ov9724_device *dev = to_ov9724_sensor(sd);
	int ret;

	mutex_lock(&dev->input_lock);
	ret = __ov9724_set_exposure(sd, exposure, gain);
	mutex_unlock(&dev->input_lock);

	return ret;
}
static long ov9724_s_exposure(struct v4l2_subdev *sd,
			       struct atomisp_exposure *exposure)
{
	u16 coarse_itg, gain;

	coarse_itg = exposure->integration_time[0];
	gain = exposure->gain[0];

	return ov9724_set_exposure(sd, coarse_itg, gain);
}

static long ov9724_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{

	switch (cmd) {
	case ATOMISP_IOC_S_EXPOSURE:
		return ov9724_s_exposure(sd, (struct atomisp_exposure *)arg);
	default:
		return -EINVAL;
	}
	return 0;
}

static struct ov9724_control *ov9724_find_control(__u32 id)
{
	int i;

	for (i = 0; i < N_CONTROLS; i++) {
		if (ov9724_controls[i].qc.id == id)
			return &ov9724_controls[i];
	}
	return NULL;
}

static int ov9724_detect(struct i2c_client *client, u16 *id, u8 *revision)
{
	struct i2c_adapter *adapter = client->adapter;
	u16 high, low, rev;

	/* i2c check */
	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -ENODEV;

	/* check sensor chip ID	 */
	if (ov9724_read_reg(client, OV9724_8BIT, OV9724_PID_HIGH,
			     &high)) {
		v4l2_err(client, "sensor_id_high = 0x%x\n", high);
		return -ENODEV;
	}

	if (ov9724_read_reg(client, OV9724_8BIT, OV9724_PID_LOW,
			     &low)) {
		v4l2_err(client, "sensor_id_low = 0x%x\n", low);
		return -ENODEV;
	}

	*id = (((u8) high) << 8) | (u8) low;
	v4l2_info(client, "sensor_id = 0x%x\n", *id);

	if (*id != OV9724_MOD_ID) {
		v4l2_err(client, "sensor ID error\n");
		return -ENODEV;
	}

	v4l2_info(client, "detect ov9724 success\n");

	if (ov9724_read_reg(client, OV9724_8BIT, OV9724_REV,
			     &rev)) {
		v4l2_err(client, "sensor_id_low = 0x%x\n", rev);
		return -ENODEV;
	}

	/* TODO - need to be updated */
	*revision = rev;

	return 0;
}

static int
ov9724_s_config(struct v4l2_subdev *sd, int irq, void *platform_data)
{
	struct ov9724_device *dev = to_ov9724_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u8 sensor_revision;
	u16 sensor_id;
	int ret;

	if (NULL == platform_data)
		return -ENODEV;

	dev->platform_data =
	    (struct camera_sensor_platform_data *)platform_data;

	mutex_lock(&dev->input_lock);
	if (dev->platform_data->platform_init) {
		ret = dev->platform_data->platform_init(client);
		if (ret) {
			mutex_unlock(&dev->input_lock);
			dev_err(&client->dev, "imx platform init err\n");
			return ret;
		}
	}

	ret = __ov9724_s_power(sd, 1);
	if (ret) {
		mutex_unlock(&dev->input_lock);
		v4l2_err(client, "ov9724 power-up err");
		return ret;
	}

	/* config & detect sensor */
	ret = ov9724_detect(client, &sensor_id, &sensor_revision);
	if (ret) {
		v4l2_err(client, "ov9724_detect err s_config.\n");
		goto fail_detect;
	}
	dev->sensor_id = sensor_id;
	dev->sensor_revision = sensor_revision;

	ret = dev->platform_data->csi_cfg(sd, 1);
	if (ret)
		goto fail_csi_cfg;

	ret = __ov9724_s_power(sd, 0);
	mutex_unlock(&dev->input_lock);
	if (ret) {
		v4l2_err(client, "ov9724 power down err");
		return ret;
	}

	return 0;

fail_csi_cfg:
	dev->platform_data->csi_cfg(sd, 0);
fail_detect:
	__ov9724_s_power(sd, 0);

	if (dev->platform_data->platform_deinit)
		dev->platform_data->platform_deinit();

	mutex_unlock(&dev->input_lock);
	dev_err(&client->dev, "sensor power-gating failed\n");
	return ret;
}

static int ov9724_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	struct ov9724_control *ctrl = ov9724_find_control(qc->id);
	struct ov9724_device *dev = to_ov9724_sensor(sd);

	mutex_lock(&dev->input_lock);
	if (ctrl == NULL)
		return -EINVAL;
	*qc = ctrl->qc;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static int ov9724_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct ov9724_control *octrl = ov9724_find_control(ctrl->id);
	struct ov9724_device *dev = to_ov9724_sensor(sd);
	int ret;

	if (octrl == NULL)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	ret = octrl->query(sd, &ctrl->value);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int ov9724_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct ov9724_control *octrl = ov9724_find_control(ctrl->id);
	struct ov9724_device *dev = to_ov9724_sensor(sd);
	int ret;

	if (!octrl || !octrl->tweak)
		return -EINVAL;
	mutex_lock(&dev->input_lock);
	ret = octrl->tweak(sd, ctrl->value);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int ov9724_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov9724_device *dev = to_ov9724_sensor(sd);

	mutex_lock(&dev->input_lock);
	if (enable) {
		ret = ov9724_write_reg_array(client, ov9724_streaming);
		if (ret != 0) {
			mutex_unlock(&dev->input_lock);
			v4l2_err(client, "write_reg_array err\n");
			return ret;
		}

		dev->streaming = 1;
	} else {
		ret = ov9724_write_reg_array(client, ov9724_suspend);
		if (ret != 0) {
			mutex_unlock(&dev->input_lock);
			v4l2_err(client, "write_reg_array err\n");
			return ret;
		}
		dev->streaming = 0;
	}
	mutex_unlock(&dev->input_lock);
	return 0;
}

/*
 * ov9724 enum frame size, frame intervals
 */
static int ov9724_enum_framesizes(struct v4l2_subdev *sd,
				   struct v4l2_frmsizeenum *fsize)
{
	unsigned int index = fsize->index;

	if (index >= N_RES)
		return -EINVAL;

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = ov9724_res[index].width;
	fsize->discrete.height = ov9724_res[index].height;
	fsize->reserved[0] = ov9724_res[index].used;

	return 0;
}

static int ov9724_enum_frameintervals(struct v4l2_subdev *sd,
				       struct v4l2_frmivalenum *fival)
{
	int i;

	/* since the isp will donwscale the resolution to the right size,
	  * find the nearest one that will allow the isp to do so
	  * important to ensure that the resolution requested is padded
	  * correctly by the requester, which is the atomisp driver in
	  * this case.
	  */
	i = nearest_resolution_index(fival->width, fival->height);

	if (i == -1)
		return -EINVAL;

	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fival->width = ov9724_res[i].width;
	fival->height = ov9724_res[i].height;
	fival->discrete.numerator = 1;
	fival->discrete.denominator = ov9724_res[i].fps;

	return 0;
}

static int ov9724_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned int index,
				 enum v4l2_mbus_pixelcode *code)
{
	if (index >= MAX_FMTS)
		return -EINVAL;

	*code = V4L2_MBUS_FMT_SBGGR10_1X10;
	return 0;
}


static int
ov9724_g_chip_ident(struct v4l2_subdev *sd, struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_OV9724, 0);
}

static int
ov9724_enum_mbus_code(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index >= MAX_FMTS)
		return -EINVAL;
	code->code = V4L2_MBUS_FMT_SBGGR10_1X10;

	return 0;
}

static int
ov9724_enum_frame_size(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			struct v4l2_subdev_frame_size_enum *fse)
{
	int index = fse->index;

	if (index >= N_RES)
		return -EINVAL;

	fse->min_width = ov9724_res[index].width;
	fse->min_height = ov9724_res[index].height;
	fse->max_width = ov9724_res[index].width;
	fse->max_height = ov9724_res[index].height;

	return 0;
}

static struct v4l2_mbus_framefmt *
__ov9724_get_pad_format(struct ov9724_device *sensor,
			 struct v4l2_subdev_fh *fh, unsigned int pad,
			 enum v4l2_subdev_format_whence which)
{
	switch (which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		return v4l2_subdev_get_try_format(fh, pad);
	case V4L2_SUBDEV_FORMAT_ACTIVE:
		return &sensor->format;
	default:
		return NULL;
	}
}

static int
ov9724_get_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct ov9724_device *dev = to_ov9724_sensor(sd);
	struct v4l2_mbus_framefmt *format =
			__ov9724_get_pad_format(dev, fh, fmt->pad, fmt->which);

	fmt->format = *format;

	return 0;
}

static int
ov9724_set_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct ov9724_device *dev = to_ov9724_sensor(sd);

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		dev->format = fmt->format;

	return 0;
}

static int
ov9724_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct ov9724_device *dev = to_ov9724_sensor(sd);
	dev->run_mode = param->parm.capture.capturemode;

	mutex_lock(&dev->input_lock);
	switch (dev->run_mode) {
	case CI_MODE_VIDEO:
		ov9724_res = ov9724_res_video;
		N_RES = N_RES_VIDEO;
		break;
	case CI_MODE_STILL_CAPTURE:
		ov9724_res = ov9724_res_still;
		N_RES = N_RES_STILL;
		break;
	default:
		ov9724_res = ov9724_res_preview;
		N_RES = N_RES_PREVIEW;
	}
	mutex_unlock(&dev->input_lock);
	return 0;
}

int
ov9724_g_frame_interval(struct v4l2_subdev *sd,
				struct v4l2_subdev_frame_interval *interval)
{
	struct ov9724_device *dev = to_ov9724_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u16 lines_per_frame;
	/*
	 * if no specific information to calculate the fps,
	 * just used the value in sensor settings
	 */
	if (!dev->pixels_per_line || !dev->lines_per_frame) {
		interval->interval.numerator = 1;
		interval->interval.denominator = dev->fps;
		return 0;
	}

	/*
	 * DS: if coarse_integration_time is set larger than
	 * lines_per_frame the frame_size will be expanded to
	 * coarse_integration_time+1
	 */
	if (dev->coarse_itg > dev->lines_per_frame) {
		if (dev->coarse_itg == 0xFFFF) {
			/*
			 * we can not add 1 according to ds, as this will
			 * cause over flow
			 */
			v4l2_warn(client, "%s: abnormal coarse_itg:0x%x\n",
				  __func__, dev->coarse_itg);
			lines_per_frame = dev->coarse_itg;
		} else {
			lines_per_frame = dev->coarse_itg + 1;
		}
	} else {
		lines_per_frame = dev->lines_per_frame;
	}

	interval->interval.numerator = dev->pixels_per_line *
					lines_per_frame;
	interval->interval.denominator = dev->vt_pix_clk_freq_mhz;

	return 0;
}

static int ov9724_g_skip_frames(struct v4l2_subdev *sd, u32 *frames)
{
	struct ov9724_device *dev = to_ov9724_sensor(sd);

	mutex_lock(&dev->input_lock);
	*frames = ov9724_res[dev->fmt_idx].skip_frames;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static const struct v4l2_subdev_sensor_ops ov9724_sensor_ops = {
	.g_skip_frames = ov9724_g_skip_frames,
};

static const struct v4l2_subdev_video_ops ov9724_video_ops = {
	.try_mbus_fmt = ov9724_try_mbus_fmt,
	.s_mbus_fmt = ov9724_set_mbus_fmt,
	.s_stream = ov9724_s_stream,
	.enum_framesizes = ov9724_enum_framesizes,
	.enum_frameintervals = ov9724_enum_frameintervals,
	.s_parm = ov9724_s_parm,
	.g_mbus_fmt = ov9724_g_mbus_fmt,
	.enum_mbus_fmt = ov9724_enum_mbus_fmt,
	.g_frame_interval = ov9724_g_frame_interval,
};

static const struct v4l2_subdev_core_ops ov9724_core_ops = {
	.g_chip_ident = ov9724_g_chip_ident,
	.queryctrl = ov9724_queryctrl,
	.g_ctrl = ov9724_g_ctrl,
	.s_ctrl = ov9724_s_ctrl,
	.ioctl = ov9724_ioctl,
	.s_power = ov9724_s_power,
	.init = ov9724_init,
};

static const struct v4l2_subdev_pad_ops ov9724_pad_ops = {
	.enum_mbus_code = ov9724_enum_mbus_code,
	.enum_frame_size = ov9724_enum_frame_size,
	.get_fmt = ov9724_get_pad_format,
	.set_fmt = ov9724_set_pad_format,
};

static const struct v4l2_subdev_ops ov9724_ops = {
	.core = &ov9724_core_ops,
	.video = &ov9724_video_ops,
	.pad = &ov9724_pad_ops,
	.sensor = &ov9724_sensor_ops,
};

static const struct media_entity_operations ov9724_entity_ops = {
	.link_setup = NULL,
};

static int ov9724_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov9724_device *dev = to_ov9724_sensor(sd);

	dev->platform_data->csi_cfg(sd, 0);
	if (dev->platform_data->platform_deinit)
		dev->platform_data->platform_deinit();

	v4l2_device_unregister_subdev(sd);
	kfree(dev);

	return 0;
}

static int ov9724_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct ov9724_device *dev;
	int ret;

	/* allocate sensor device & init sub device */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		v4l2_err(client, "%s: out of memory\n", __func__);
		return -ENOMEM;
	}

	mutex_init(&dev->input_lock);

	dev->fmt_idx = 0;
	v4l2_i2c_subdev_init(&(dev->sd), client, &ov9724_ops);


	if (client->dev.platform_data) {
		ret = ov9724_s_config(&dev->sd, client->irq,
				       client->dev.platform_data);
		if (ret)
			goto out_free;
	}

	dev->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->pad.flags = MEDIA_PAD_FL_SOURCE;
	dev->format.code = V4L2_MBUS_FMT_SBGGR10_1X10;
	dev->sd.entity.ops = &ov9724_entity_ops;
	dev->sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;

	ret = media_entity_init(&dev->sd.entity, 1, &dev->pad, 0);
	if (ret)
		ov9724_remove(client);

	return ret;
out_free:
	v4l2_device_unregister_subdev(&dev->sd);
	kfree(dev);
	return ret;
}


static const struct i2c_device_id ov9724_id[] = {
	{OV9724_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, ov9724_id);

static struct i2c_driver ov9724_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = OV9724_NAME,
	},
	.probe = ov9724_probe,
	.remove = ov9724_remove,
	.id_table = ov9724_id,
};


static __init int init_ov9724(void)
{
	return i2c_add_driver(&ov9724_driver);
}

static __exit void exit_ov9724(void)
{

	i2c_del_driver(&ov9724_driver);
}

module_init(init_ov9724);
module_exit(exit_ov9724);

MODULE_DESCRIPTION("A low-level driver for OV9724 sensor");
MODULE_AUTHOR("Shenbo Huang <shenbo.huang@intel.com>");
MODULE_LICENSE("GPL");
