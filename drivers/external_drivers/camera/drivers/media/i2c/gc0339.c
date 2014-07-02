/*
 * Support for GalaxyCore GC0339 VGA camera sensor.
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

#include "gc0339.h"

/* i2c read/write stuff */
static int gc0339_read_reg(struct i2c_client *client,
			   u16 data_length, u8 reg, u8 *val)
{
	int err;
	struct i2c_msg msg[2];
	unsigned char data[2];

	if (!client->adapter) {
		dev_err(&client->dev, "%s error, no client->adapter\n",
			__func__);
		return -ENODEV;
	}

	if (data_length != GC0339_8BIT) {
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
	data[0] = (u8)(reg & 0xff);
	//pr_info("msg0 %x %d %d %d\n", msg[0].addr, msg[0].flags, msg[0].len, *msg[0].buf);

	msg[1].addr = client->addr;
	msg[1].len = data_length;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = data+1;
	//pr_info("msg1 %x %d %d %d\n", msg[1].addr, msg[1].flags, msg[1].len, *msg[1].buf);

	err = i2c_transfer(client->adapter, msg, 2);
	if (err != 2) {
		if (err >= 0)
			err = -EIO;
		dev_err(&client->dev,
			"read from offset 0x%x error %d", reg, err);
		return err;
	}

//pr_info("read %d %d %d\n", data[0], data[1], err);
	*val = 0;
	/* high byte comes first */
	if (data_length == GC0339_8BIT)
		*val = (u8)data[1];

	return 0;
}

static int gc0339_i2c_write(struct i2c_client *client, u16 len, u8 *data)
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

static int gc0339_write_reg(struct i2c_client *client, u16 data_length,
							u8 reg, u8 val)
{
	int ret;
	unsigned char data[2] = {0};
	u8 *wreg = (u8 *)data;
	const u16 len = data_length + sizeof(u8); /* 8-bit address + data */

	if (data_length != GC0339_8BIT) {
		dev_err(&client->dev,
			"%s error, invalid data_length\n", __func__);
		return -EINVAL;
	}

	/* high byte goes out first */
	*wreg = (u8)(reg & 0xff);

	if (data_length == GC0339_8BIT) {
		data[1] = (u8)(val);
	}

	ret = gc0339_i2c_write(client, len, data);
	if (ret)
		dev_err(&client->dev,
			"write error: wrote 0x%x to offset 0x%x error %d",
			val, reg, ret);

	return ret;
}

/*
 * gc0339_write_reg_array - Initializes a list of GC0339 registers
 * @client: i2c driver client structure
 * @reglist: list of registers to be written
 *
 * This function initializes a list of registers. When consecutive addresses
 * are found in a row on the list, this function creates a buffer and sends
 * consecutive data in a single i2c_transfer().
 *
 * __gc0339_flush_reg_array, __gc0339_buf_reg_array() and
 * __gc0339_write_reg_is_consecutive() are internal functions to
 * gc0339_write_reg_array_fast() and should be not used anywhere else.
 *
 */

static int __gc0339_flush_reg_array(struct i2c_client *client,
				    struct gc0339_write_ctrl *ctrl)
{
	u16 size;

	if (ctrl->index == 0)
		return 0;

	size = sizeof(u8) + ctrl->index; /* 8-bit address + data */
	ctrl->buffer.addr = (u8)(ctrl->buffer.addr);
	ctrl->index = 0;

	return gc0339_i2c_write(client, size, (u8 *)&ctrl->buffer);
}

static int __gc0339_buf_reg_array(struct i2c_client *client,
				  struct gc0339_write_ctrl *ctrl,
				  const struct gc0339_reg *next)
{
	int size;

	switch (next->type) {
	case GC0339_8BIT:
		size = 1;
		ctrl->buffer.data[ctrl->index] = (u8)next->val;
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
	if (ctrl->index + sizeof(u8) >= GC0339_MAX_WRITE_BUF_SIZE)
		return __gc0339_flush_reg_array(client, ctrl);

	return 0;
}

static int __gc0339_write_reg_is_consecutive(struct i2c_client *client,
					     struct gc0339_write_ctrl *ctrl,
					     const struct gc0339_reg *next)
{
	if (ctrl->index == 0)
		return 1;

	return ctrl->buffer.addr + ctrl->index == next->reg;
}

static int gc0339_write_reg_array(struct i2c_client *client,
				  const struct gc0339_reg *reglist)
{
	const struct gc0339_reg *next = reglist;
	struct gc0339_write_ctrl ctrl;
	int err;

	ctrl.index = 0;
	for (; next->type != GC0339_TOK_TERM; next++) {
		switch (next->type & GC0339_TOK_MASK) {
		case GC0339_TOK_DELAY:
			err = __gc0339_flush_reg_array(client, &ctrl);
			if (err)
				return err;
			msleep(next->val);
			break;
		default:
			/*
			 * If next address is not consecutive, data needs to be
			 * flushed before proceed.
			 */
			if (!__gc0339_write_reg_is_consecutive(client, &ctrl,
								next)) {
				err = __gc0339_flush_reg_array(client, &ctrl);
				if (err)
					return err;
			}
			err = __gc0339_buf_reg_array(client, &ctrl, next);
			if (err) {
				dev_err(&client->dev, "%s: write error, aborted\n",
					 __func__);
				return err;
			}
			break;
		}
	}

	return __gc0339_flush_reg_array(client, &ctrl);
}
static int gc0339_g_focal(struct v4l2_subdev *sd, s32 *val)
{
	*val = (GC0339_FOCAL_LENGTH_NUM << 16) | GC0339_FOCAL_LENGTH_DEM;
	return 0;
}

static int gc0339_g_fnumber(struct v4l2_subdev *sd, s32 *val)
{
	/*const f number for imx*/
	*val = (GC0339_F_NUMBER_DEFAULT_NUM << 16) | GC0339_F_NUMBER_DEM;
	return 0;
}

static int gc0339_g_fnumber_range(struct v4l2_subdev *sd, s32 *val)
{
	*val = (GC0339_F_NUMBER_DEFAULT_NUM << 24) |
		(GC0339_F_NUMBER_DEM << 16) |
		(GC0339_F_NUMBER_DEFAULT_NUM << 8) | GC0339_F_NUMBER_DEM;
	return 0;
}

static int gc0339_g_bin_factor_x(struct v4l2_subdev *sd, s32 *val)
{
	struct gc0339_device *dev = to_gc0339_sensor(sd);

	*val = gc0339_res[dev->fmt_idx].bin_factor_x;

	return 0;
}

static int gc0339_g_bin_factor_y(struct v4l2_subdev *sd, s32 *val)
{
	struct gc0339_device *dev = to_gc0339_sensor(sd);

	*val = gc0339_res[dev->fmt_idx].bin_factor_y;

	return 0;
}

static int gc0339_get_intg_factor(struct i2c_client *client,
				struct camera_mipi_info *info,
				const struct gc0339_resolution *res)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc0339_device *dev = to_gc0339_sensor(sd);
	struct atomisp_sensor_mode_data *buf = &info->data;
	const unsigned int ext_clk_freq_hz = 19200000;
	u16 val;
	u8 reg_val;
	int ret;
	unsigned int hori_blanking;
	unsigned int vert_blanking;
	unsigned int sh_delay;

	if (info == NULL)
		return -EINVAL;

	/* pixel clock calculattion */
	dev->vt_pix_clk_freq_mhz = ext_clk_freq_hz / 2;
	buf->vt_pix_clk_freq_mhz = ext_clk_freq_hz / 2;
	pr_info("vt_pix_clk_freq_mhz=%d\n", buf->vt_pix_clk_freq_mhz);

	/* get integration time */
	buf->coarse_integration_time_min = GC0339_COARSE_INTG_TIME_MIN;
	buf->coarse_integration_time_max_margin =
					GC0339_COARSE_INTG_TIME_MAX_MARGIN;

	buf->fine_integration_time_min = GC0339_FINE_INTG_TIME_MIN;
	buf->fine_integration_time_max_margin =
					GC0339_FINE_INTG_TIME_MAX_MARGIN;

	buf->fine_integration_time_def = GC0339_FINE_INTG_TIME_MIN;
	buf->read_mode = res->bin_mode;

	/* get the cropping and output resolution to ISP for this mode. */
	/* Getting crop_horizontal_start */
	ret =  gc0339_read_reg(client, GC0339_8BIT,
					GC0339_H_CROP_START_H, &reg_val);
	if (ret)
		return ret;
	val = (reg_val & 0xFF) << 8;
	ret =  gc0339_read_reg(client, GC0339_8BIT,
					GC0339_H_CROP_START_L, &reg_val);
	if (ret)
		return ret;
	buf->crop_horizontal_start = val | (reg_val & 0xFF);
	pr_info("crop_horizontal_start=%d\n", buf->crop_horizontal_start);

	/* Getting crop_vertical_start */
	ret =  gc0339_read_reg(client, GC0339_8BIT,
					GC0339_V_CROP_START_H, &reg_val);
	if (ret)
		return ret;
	val = (reg_val & 0xFF) << 8;
	ret =  gc0339_read_reg(client, GC0339_8BIT,
					GC0339_V_CROP_START_L, &reg_val);
	if (ret)
		return ret;
	buf->crop_vertical_start = val | (reg_val & 0xFF);
	pr_info("crop_vertical_start=%d\n", buf->crop_vertical_start);

	/* Getting output_width */
	ret = gc0339_read_reg(client, GC0339_8BIT,
					GC0339_H_OUTSIZE_H, &reg_val);
	if (ret)
		return ret;
	val = (reg_val & 0xFF) << 8;
	ret = gc0339_read_reg(client, GC0339_8BIT,
					GC0339_H_OUTSIZE_L, &reg_val);
	if (ret)
		return ret;
	buf->output_width = val | (reg_val & 0xFF);
	pr_info("output_width=%d\n", buf->output_width);

	/* Getting output_height */
	ret = gc0339_read_reg(client, GC0339_8BIT,
					GC0339_V_OUTSIZE_H, &reg_val);
	if (ret)
		return ret;
	val = (reg_val & 0xFF) << 8;
	ret = gc0339_read_reg(client, GC0339_8BIT,
					GC0339_V_OUTSIZE_L, &reg_val);
	if (ret)
		return ret;
	buf->output_height = val | (reg_val & 0xFF);
	pr_info("output_height=%d\n", buf->output_height);

	buf->crop_horizontal_end = buf->crop_horizontal_start + buf->output_width - 1;
	buf->crop_vertical_end = buf->crop_vertical_start + buf->output_height - 1;
	pr_info("crop_horizontal_end=%d\n", buf->crop_horizontal_end);
	pr_info("crop_vertical_end=%d\n", buf->crop_vertical_end);

	/* Getting line_length_pck */
#if 1
	ret = gc0339_read_reg(client, GC0339_8BIT,
					GC0339_H_BLANKING_H, &reg_val);
	if (ret)
		return ret;
	val = ((reg_val & 0xF0) >> 4) << 8;
	ret = gc0339_read_reg(client, GC0339_8BIT,
					GC0339_H_BLANKING_L, &reg_val);
	if (ret)
		return ret;
	hori_blanking = val | (reg_val & 0xFF);
	ret = gc0339_read_reg(client, GC0339_8BIT,
					GC0339_SH_DELAY, &reg_val);
	if (ret)
		return ret;
	sh_delay = reg_val;
	buf->line_length_pck = buf->output_width + hori_blanking + sh_delay + 4;
	pr_info("hori_blanking=%d sh_delay=%d line_length_pck=%d\n", hori_blanking, sh_delay, buf->line_length_pck);
#else
	buf->line_length_pck = res->pixels_per_line;
#endif

	/* Getting frame_length_lines */
#if 1
	ret = gc0339_read_reg(client, GC0339_8BIT,
					GC0339_V_BLANKING_H, &reg_val);
	if (ret)
		return ret;
	val = (reg_val & 0x0F) << 8;
	ret = gc0339_read_reg(client, GC0339_8BIT,
					GC0339_V_BLANKING_L, &reg_val);
	if (ret)
		return ret;
	vert_blanking = val | (reg_val & 0xFF);
	buf->frame_length_lines = buf->output_height + vert_blanking;
	pr_info("vert_blanking=%d frame_length_lines=%d\n", vert_blanking, buf->frame_length_lines);
#else
	buf->frame_length_lines = res->lines_per_frame;
#endif

	buf->binning_factor_x = res->bin_factor_x ?
					res->bin_factor_x : 1;
	buf->binning_factor_y = res->bin_factor_y ?
					res->bin_factor_y : 1;
	return 0;
}

static long __gc0339_set_exposure(struct v4l2_subdev *sd, int coarse_itg,
				 int gain, int digitgain)

{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct gc0339_device *dev = to_gc0339_sensor(sd);
	u16 vts;
	int frame_length;
	int ret;

	pr_info("coarse_itg=%d gain=%d digitgain=%d\n", coarse_itg, gain, digitgain);

	vts = gc0339_res[dev->fmt_idx].lines_per_frame;
	if ((coarse_itg + GC0339_COARSE_INTG_TIME_MAX_MARGIN) >= vts)
		frame_length = coarse_itg + GC0339_COARSE_INTG_TIME_MAX_MARGIN;
	else
		frame_length = vts;

#if 0
	/* group hold start */
	ret = gc0339_write_reg(client, GC0339_8BIT, GC0339_GROUP_ACCESS, 1);
	if (ret)
		return ret;
#endif

#if 0
	ret = gc0339_write_reg(client, GC0339_8BIT,
				GC0339_VTS_DIFF_H, frame_length >> 8);
	if (ret)
		return ret;
#endif

	/* set exposure */
	ret = gc0339_write_reg(client, GC0339_8BIT,
					GC0339_AEC_PK_EXPO_L,
					coarse_itg & 0xff);
	if (ret)
		return ret;

	ret = gc0339_write_reg(client, GC0339_8BIT,
					GC0339_AEC_PK_EXPO_H,
					(coarse_itg >> 8) & 0x0f);
	if (ret)
		return ret;

	/* set analog gain */
	ret = gc0339_write_reg(client, GC0339_8BIT,
					GC0339_AGC_ADJ, gain);
	if (ret)
		return ret;

#if 0
	/* group hold end */
	ret = gc0339_write_reg(client, GC0339_8BIT,
					GC0339_GROUP_ACCESS, 0x0);
#endif

	return ret;
}

static int gc0339_set_exposure(struct v4l2_subdev *sd, int exposure,
	int gain, int digitgain)
{
	struct gc0339_device *dev = to_gc0339_sensor(sd);
	int ret;

	mutex_lock(&dev->input_lock);
	ret = __gc0339_set_exposure(sd, exposure, gain, digitgain);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static long gc0339_s_exposure(struct v4l2_subdev *sd,
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

	return gc0339_set_exposure(sd, exp, gain, digitgain);
}

/* TO DO */
static int gc0339_v_flip(struct v4l2_subdev *sd, s32 value)
{
	return 0;
}

/* TO DO */
static int gc0339_h_flip(struct v4l2_subdev *sd, s32 value)
{
	return 0;
}

static long gc0339_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{

	switch (cmd) {
	case ATOMISP_IOC_S_EXPOSURE:
		return gc0339_s_exposure(sd, arg);
	default:
		return -EINVAL;
	}
	return 0;
}

/* This returns the exposure time being used. This should only be used
   for filling in EXIF data, not for actual image processing. */
static int gc0339_q_exposure(struct v4l2_subdev *sd, s32 *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u8 reg_v;
	int ret;

	/* get exposure */
	ret = gc0339_read_reg(client, GC0339_8BIT,
					GC0339_AEC_PK_EXPO_L,
					&reg_v);
	if (ret)
		goto err;

	*value = reg_v;
	ret = gc0339_read_reg(client, GC0339_8BIT,
					GC0339_AEC_PK_EXPO_H,
					&reg_v);
	if (ret)
		goto err;

	*value = *value + (reg_v << 8);
	//pr_info("gc0339_q_exposure %d\n", *value);
err:
	return ret;
}
struct gc0339_control gc0339_controls[] = {
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
		.query = gc0339_q_exposure,
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
		.tweak = gc0339_v_flip,
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
		.tweak = gc0339_h_flip,
	},
	{
		.qc = {
			.id = V4L2_CID_FOCAL_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focal length",
			.minimum = GC0339_FOCAL_LENGTH_DEFAULT,
			.maximum = GC0339_FOCAL_LENGTH_DEFAULT,
			.step = 0x01,
			.default_value = GC0339_FOCAL_LENGTH_DEFAULT,
			.flags = 0,
		},
		.query = gc0339_g_focal,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number",
			.minimum = GC0339_F_NUMBER_DEFAULT,
			.maximum = GC0339_F_NUMBER_DEFAULT,
			.step = 0x01,
			.default_value = GC0339_F_NUMBER_DEFAULT,
			.flags = 0,
		},
		.query = gc0339_g_fnumber,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_RANGE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number range",
			.minimum = GC0339_F_NUMBER_RANGE,
			.maximum =  GC0339_F_NUMBER_RANGE,
			.step = 0x01,
			.default_value = GC0339_F_NUMBER_RANGE,
			.flags = 0,
		},
		.query = gc0339_g_fnumber_range,
	},
	{
		.qc = {
			.id = V4L2_CID_BIN_FACTOR_HORZ,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "horizontal binning factor",
			.minimum = 0,
			.maximum = GC0339_BIN_FACTOR_MAX,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.query = gc0339_g_bin_factor_x,
	},
	{
		.qc = {
			.id = V4L2_CID_BIN_FACTOR_VERT,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "vertical binning factor",
			.minimum = 0,
			.maximum = GC0339_BIN_FACTOR_MAX,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.query = gc0339_g_bin_factor_y,
	},
};
#define N_CONTROLS (ARRAY_SIZE(gc0339_controls))

static struct gc0339_control *gc0339_find_control(u32 id)
{
	int i;

	for (i = 0; i < N_CONTROLS; i++)
		if (gc0339_controls[i].qc.id == id)
			return &gc0339_controls[i];
	return NULL;
}

static int gc0339_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	struct gc0339_control *ctrl = gc0339_find_control(qc->id);
	struct gc0339_device *dev = to_gc0339_sensor(sd);

	if (ctrl == NULL)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	*qc = ctrl->qc;
	mutex_unlock(&dev->input_lock);

	return 0;
}

/* imx control set/get */
static int gc0339_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct gc0339_control *s_ctrl;
	struct gc0339_device *dev = to_gc0339_sensor(sd);
	int ret;

	if (!ctrl)
		return -EINVAL;

	s_ctrl = gc0339_find_control(ctrl->id);
	if ((s_ctrl == NULL) || (s_ctrl->query == NULL))
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	ret = s_ctrl->query(sd, &ctrl->value);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int gc0339_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct gc0339_control *octrl = gc0339_find_control(ctrl->id);
	struct gc0339_device *dev = to_gc0339_sensor(sd);
	int ret;

	if ((octrl == NULL) || (octrl->tweak == NULL))
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	ret = octrl->tweak(sd, ctrl->value);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int gc0339_init(struct v4l2_subdev *sd)
{
	int ret;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct gc0339_device *dev = to_gc0339_sensor(sd);

	pr_info("%s S\n", __func__);
	mutex_lock(&dev->input_lock);

	/* set inital registers */
	ret  = gc0339_write_reg_array(client, gc0339_reset_register);

	/* restore settings */
	gc0339_res = gc0339_res_preview;
	N_RES = N_RES_PREVIEW;

	mutex_unlock(&dev->input_lock);

	pr_info("%s E\n", __func__);
	return 0;
}

static int power_ctrl(struct v4l2_subdev *sd, bool flag)
{
	int ret = 0;
	struct gc0339_device *dev = to_gc0339_sensor(sd);
	if (!dev || !dev->platform_data)
		return -ENODEV;

	/* Non-gmin platforms use the legacy callback */
	if (dev->platform_data->power_ctrl)
		return dev->platform_data->power_ctrl(sd, flag);

	if (flag) {
		/* The upstream module driver (written to Crystal
		 * Cove) had this logic to pulse the rails low first.
		 * This appears to break things on the MRD7 with the
		 * X-Powers PMIC...
		 *
		 *     ret = dev->platform_data->v1p8_ctrl(sd, 0);
		 *     ret |= dev->platform_data->v2p8_ctrl(sd, 0);
		 *     mdelay(50);
		*/
		ret |= dev->platform_data->v1p8_ctrl(sd, 1);
		ret |= dev->platform_data->v2p8_ctrl(sd, 1);
		msleep(10);
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
	struct gc0339_device *dev = to_gc0339_sensor(sd);

	if (!dev || !dev->platform_data)
		return -ENODEV;

	/* Non-gmin platforms use the legacy callback */
	if (dev->platform_data->gpio_ctrl)
		return dev->platform_data->gpio_ctrl(sd, flag);

	/* GPIO0 == "reset" (active low), GPIO1 == "power down" */
	if (flag) {
		/* Pulse reset, then release power down */
		ret = dev->platform_data->gpio0_ctrl(sd, 0);
		msleep(5);
		ret |= dev->platform_data->gpio0_ctrl(sd, 1);
		msleep(10);
		ret |= dev->platform_data->gpio1_ctrl(sd, 0);
		msleep(10);
	} else {
		ret = dev->platform_data->gpio1_ctrl(sd, 1);
		ret |= dev->platform_data->gpio0_ctrl(sd, 0);
	}
	return ret;
}

static int power_down(struct v4l2_subdev *sd);

static int power_up(struct v4l2_subdev *sd)
{
	struct gc0339_device *dev = to_gc0339_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	pr_info("%s S\n", __func__);
	if (NULL == dev->platform_data) {
		dev_err(&client->dev,
			"no camera_sensor_platform_data");
		return -ENODEV;
	}

	/* power control */
	ret = power_ctrl(sd, 1);
	if (ret)
		goto fail_power;

	/* flis clock control */
	ret = dev->platform_data->flisclk_ctrl(sd, 1);
	if (ret)
		goto fail_clk;

	/* gpio ctrl */
	ret = gpio_ctrl(sd, 1);
	if (ret) {
		ret = gpio_ctrl(sd, 1);
		if (ret)
			goto fail_gpio;
	}

	msleep(100);

	pr_info("%s E\n", __func__);
	return 0;

fail_gpio:
	power_ctrl(sd, 0);
fail_power:
	dev->platform_data->flisclk_ctrl(sd, 0);
fail_clk:
	dev_err(&client->dev, "sensor power-up failed\n");

	return ret;
}

static int power_down(struct v4l2_subdev *sd)
{
	struct gc0339_device *dev = to_gc0339_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	if (NULL == dev->platform_data) {
		dev_err(&client->dev,
			"no camera_sensor_platform_data");
		return -ENODEV;
	}

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

	ret = dev->platform_data->flisclk_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "flisclk failed\n");

	return ret;
}

static int gc0339_s_power(struct v4l2_subdev *sd, int on)
{
	int ret;
	if (on == 0)
		return power_down(sd);
	else {
		ret = power_up(sd);
		if (!ret)
			return gc0339_init(sd);
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
static int distance(struct gc0339_resolution *res, u32 w, u32 h)
{
	unsigned int w_ratio = ((res->width << 13)/w);
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
	struct gc0339_resolution *tmp_res = NULL;

	for (i = 0; i < N_RES; i++) {
		tmp_res = &gc0339_res[i];
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
		if (w != gc0339_res[i].width)
			continue;
		if (h != gc0339_res[i].height)
			continue;

		return i;
	}

	return -1;
}

static int gc0339_try_mbus_fmt(struct v4l2_subdev *sd,
			struct v4l2_mbus_framefmt *fmt)
{
	int idx;

	if (!fmt)
		return -EINVAL;
	idx = nearest_resolution_index(fmt->width,
					fmt->height);
	if (idx == -1) {
		/* return the largest resolution */
		fmt->width = gc0339_res[N_RES - 1].width;
		fmt->height = gc0339_res[N_RES - 1].height;
	} else {
		fmt->width = gc0339_res[idx].width;
		fmt->height = gc0339_res[idx].height;
	}
	fmt->code = V4L2_MBUS_FMT_SGRBG10_1X10;

	return 0;
}

/* TODO: remove it. */
static int startup(struct v4l2_subdev *sd)
{
	struct gc0339_device *dev = to_gc0339_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	pr_info("%s S\n", __func__);
#if 0
	ret = gc0339_write_reg(client, GC0339_8BIT,
					GC0339_SW_RESET, 0x01);
	if (ret) {
		dev_err(&client->dev, "gc0339 reset err.\n");
		return ret;
	}
#endif

	ret = gc0339_write_reg_array(client, gc0339_res[dev->fmt_idx].regs);
	if (ret) {
		dev_err(&client->dev, "gc0339 write register err.\n");
		return ret;
	}

	pr_info("%s E\n", __func__);
	return ret;
}

static int gc0339_s_mbus_fmt(struct v4l2_subdev *sd,
			     struct v4l2_mbus_framefmt *fmt)
{
	struct gc0339_device *dev = to_gc0339_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camera_mipi_info *gc0339_info = NULL;
	int ret = 0;

	pr_info("%s S\n", __func__);
	gc0339_info = v4l2_get_subdev_hostdata(sd);
	if (gc0339_info == NULL)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	ret = gc0339_try_mbus_fmt(sd, fmt);
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

	printk("%s: before gc0339_write_reg_array %s\n",__FUNCTION__, gc0339_res[dev->fmt_idx].desc);
	ret = startup(sd);
	if (ret) {
		dev_err(&client->dev, "gc0339 startup err\n");
		goto err;
	}

	ret = gc0339_get_intg_factor(client, gc0339_info,
					&gc0339_res[dev->fmt_idx]);
	if (ret) {
		dev_err(&client->dev, "failed to get integration_factor\n");
		goto err;
	}

	pr_info("%s E\n", __func__);
err:
	mutex_unlock(&dev->input_lock);
	return ret;
}
static int gc0339_g_mbus_fmt(struct v4l2_subdev *sd,
			     struct v4l2_mbus_framefmt *fmt)
{
	struct gc0339_device *dev = to_gc0339_sensor(sd);

	if (!fmt)
		return -EINVAL;

	fmt->width = gc0339_res[dev->fmt_idx].width;
	fmt->height = gc0339_res[dev->fmt_idx].height;
	fmt->code = V4L2_MBUS_FMT_SGRBG10_1X10;

	return 0;
}

static int gc0339_detect(struct i2c_client *client)
{
	struct i2c_adapter *adapter = client->adapter;
	int ret;
	u8 id;

	pr_info("%s S\n", __func__);
	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -ENODEV;

	ret = gc0339_write_reg(client, GC0339_8BIT,
					0xFC, 0x10);
	if (ret) {
		dev_err(&client->dev, "gc0339 reset err.\n");
		return ret;
	}

	ret = gc0339_read_reg(client, GC0339_8BIT,
					GC0339_SC_CMMN_CHIP_ID, &id);
	if (ret) {
		dev_err(&client->dev, "read sensor ID failed\n");
		return -ENODEV;
	}

	pr_info("sensor ID = 0x%x\n", id);
	if (id != GC0339_ID) {
		dev_err(&client->dev, "sensor ID error, read id = 0x%x, target id = 0x%x\n", id, GC0339_ID);
		return -ENODEV;
	}

	dev_dbg(&client->dev, "detect gc0339 success\n");

	pr_info("%s E\n", __func__);

	return 0;
}

static int gc0339_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct gc0339_device *dev = to_gc0339_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	pr_info("%s S enable=%d\n", __func__, enable);
	mutex_lock(&dev->input_lock);

#if 1
	if (enable) {
		/* enable per frame MIPI and sensor ctrl reset  */
		ret = gc0339_write_reg(client, GC0339_8BIT,0xFE, 0x50);
		if (ret) {
			mutex_unlock(&dev->input_lock);
			return ret;
		}
		printk("reset register.\n");
		//ret = gc0339_write_reg_array(client, gc0339_reset_register);
		//ret = gc0339_write_reg_array(client, gc0339_VGA_30fps);

	}
#endif	
	ret = gc0339_write_reg(client, GC0339_8BIT, GC0339_SW_STREAM,
				enable ? GC0339_START_STREAMING :
				GC0339_STOP_STREAMING);

	mutex_unlock(&dev->input_lock);
	pr_info("%s E\n", __func__);
	return ret;
}

/* gc0339 enum frame size, frame intervals */
static int gc0339_enum_framesizes(struct v4l2_subdev *sd,
				  struct v4l2_frmsizeenum *fsize)
{
	unsigned int index = fsize->index;

	if (index >= N_RES)
		return -EINVAL;

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = gc0339_res[index].width;
	fsize->discrete.height = gc0339_res[index].height;
	fsize->reserved[0] = gc0339_res[index].used;

	return 0;
}

static int gc0339_enum_frameintervals(struct v4l2_subdev *sd,
				      struct v4l2_frmivalenum *fival)
{
	unsigned int index = fival->index;

	if (index >= N_RES)
		return -EINVAL;

	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fival->width = gc0339_res[index].width;
	fival->height = gc0339_res[index].height;
	fival->discrete.numerator = 1;
	fival->discrete.denominator = gc0339_res[index].fps;

	return 0;
}

static int gc0339_enum_mbus_fmt(struct v4l2_subdev *sd,
				unsigned int index,
				enum v4l2_mbus_pixelcode *code)
{
	*code = V4L2_MBUS_FMT_SGRBG10_1X10;

	return 0;
}

static int gc0339_s_config(struct v4l2_subdev *sd,
			   int irq, void *platform_data)
{
	struct gc0339_device *dev = to_gc0339_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	pr_info("%s S\n", __func__);
	if (platform_data == NULL)
		return -ENODEV;

	dev->platform_data =
		(struct camera_sensor_platform_data *)platform_data;

	mutex_lock(&dev->input_lock);
	/* power off the module, then power on it in future
	 * as first power on by board may not fulfill the
	 * power on sequqence needed by the module
	 */
	printk("+++[%s], platform_init.\n",__func__);
	dev->platform_data->platform_init(client);

	ret = power_down(sd);
	if (ret) {
		dev_err(&client->dev, "gc0339 power-off err.\n");
		goto fail_power_off;
	}
	msleep(100);

	ret = power_up(sd);
	if (ret) {
		dev_err(&client->dev, "gc0339 power-up err.\n");
		goto fail_power_on;
	}

	ret = dev->platform_data->csi_cfg(sd, 1);
	if (ret)
		goto fail_csi_cfg;

	/* config & detect sensor */
	ret = gc0339_detect(client);
	if (ret) {
		dev_err(&client->dev, "gc0339_detect err s_config.\n");
		goto fail_csi_cfg;
	}

	/* turn off sensor, after probed */
	ret = power_down(sd);
	if (ret) {
		dev_err(&client->dev, "gc0339 power-off err.\n");
		goto fail_csi_cfg;
	}

	/* Register the atomisp platform data prior to the ISP module
	 * load.  Ideally this would be stored as data on the
	 * subdevices, but this API matches upstream better. */
	ret = atomisp_register_i2c_module(sd, client, platform_data,
					  gmin_get_var_int(&client->dev, "CamType",
						     RAW_CAMERA),
					  gmin_get_var_int(&client->dev, "CsiPort",
						     ATOMISP_CAMERA_PORT_PRIMARY));
	if (ret) {
		dev_err(&client->dev,
			"gc2235 atomisp_register_i2c_module failed.\n");
		goto fail_csi_cfg;
	}
	mutex_unlock(&dev->input_lock);

	pr_info("%s E\n", __func__);
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

static int gc0339_g_parm(struct v4l2_subdev *sd,
			struct v4l2_streamparm *param)
{
	struct gc0339_device *dev = to_gc0339_sensor(sd);
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
			gc0339_res[dev->fmt_idx].fps;
	}
	return 0;
}

static int gc0339_s_parm(struct v4l2_subdev *sd,
			struct v4l2_streamparm *param)
{
	struct gc0339_device *dev = to_gc0339_sensor(sd);
	dev->run_mode = param->parm.capture.capturemode;

	mutex_lock(&dev->input_lock);
	switch (dev->run_mode) {
	case CI_MODE_VIDEO:
		gc0339_res = gc0339_res_video;
		N_RES = N_RES_VIDEO;
		break;
	case CI_MODE_STILL_CAPTURE:
		gc0339_res = gc0339_res_still;
		N_RES = N_RES_STILL;
		break;
	default:
		gc0339_res = gc0339_res_preview;
		N_RES = N_RES_PREVIEW;
	}
	mutex_unlock(&dev->input_lock);
	return 0;
}

static int gc0339_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *interval)
{
	struct gc0339_device *dev = to_gc0339_sensor(sd);

	interval->interval.numerator = 1;
	interval->interval.denominator = gc0339_res[dev->fmt_idx].fps;

	return 0;
}

static int gc0339_enum_mbus_code(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index >= MAX_FMTS)
		return -EINVAL;

	code->code = V4L2_MBUS_FMT_SGRBG10_1X10;
	return 0;
}

static int gc0339_enum_frame_size(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_frame_size_enum *fse)
{
	int index = fse->index;

	if (index >= N_RES)
		return -EINVAL;

	fse->min_width = gc0339_res[index].width;
	fse->min_height = gc0339_res[index].height;
	fse->max_width = gc0339_res[index].width;
	fse->max_height = gc0339_res[index].height;

	return 0;

}

static struct v4l2_mbus_framefmt *
__gc0339_get_pad_format(struct gc0339_device *sensor,
			struct v4l2_subdev_fh *fh, unsigned int pad,
			enum v4l2_subdev_format_whence which)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->sd);

	if (pad != 0) {
		dev_err(&client->dev,
			"__gc0339_get_pad_format err. pad %x\n", pad);
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

static int gc0339_get_pad_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	struct gc0339_device *snr = to_gc0339_sensor(sd);
	struct v4l2_mbus_framefmt *format =
			__gc0339_get_pad_format(snr, fh, fmt->pad, fmt->which);
	if (!format)
		return -EINVAL;

	fmt->format = *format;
	return 0;
}

static int gc0339_set_pad_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	struct gc0339_device *snr = to_gc0339_sensor(sd);

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		snr->format = fmt->format;

	return 0;
}

static int gc0339_g_skip_frames(struct v4l2_subdev *sd, u32 *frames)
{
	struct gc0339_device *dev = to_gc0339_sensor(sd);

	mutex_lock(&dev->input_lock);
	*frames = gc0339_res[dev->fmt_idx].skip_frames;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static const struct v4l2_subdev_sensor_ops gc0339_sensor_ops = {
	.g_skip_frames	= gc0339_g_skip_frames,
};

static const struct v4l2_subdev_video_ops gc0339_video_ops = {
	.s_stream = gc0339_s_stream,
	.g_parm = gc0339_g_parm,
	.s_parm = gc0339_s_parm,
	.enum_framesizes = gc0339_enum_framesizes,
	.enum_frameintervals = gc0339_enum_frameintervals,
	.enum_mbus_fmt = gc0339_enum_mbus_fmt,
	.try_mbus_fmt = gc0339_try_mbus_fmt,
	.g_mbus_fmt = gc0339_g_mbus_fmt,
	.s_mbus_fmt = gc0339_s_mbus_fmt,
	.g_frame_interval = gc0339_g_frame_interval,
};

static const struct v4l2_subdev_core_ops gc0339_core_ops = {
	.s_power = gc0339_s_power,
	.queryctrl = gc0339_queryctrl,
	.g_ctrl = gc0339_g_ctrl,
	.s_ctrl = gc0339_s_ctrl,
	.ioctl = gc0339_ioctl,
};

static const struct v4l2_subdev_pad_ops gc0339_pad_ops = {
	.enum_mbus_code = gc0339_enum_mbus_code,
	.enum_frame_size = gc0339_enum_frame_size,
	.get_fmt = gc0339_get_pad_format,
	.set_fmt = gc0339_set_pad_format,
};

static const struct v4l2_subdev_ops gc0339_ops = {
	.core = &gc0339_core_ops,
	.video = &gc0339_video_ops,
	.pad = &gc0339_pad_ops,
	.sensor = &gc0339_sensor_ops,
};

static int gc0339_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc0339_device *dev = to_gc0339_sensor(sd);
	dev_dbg(&client->dev, "gc0339_remove...\n");

	dev->platform_data->csi_cfg(sd, 0);

	v4l2_device_unregister_subdev(sd);
	media_entity_cleanup(&dev->sd.entity);
	kfree(dev);

	return 0;
}

static int gc0339_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct gc0339_device *dev;
	int ret;

	pr_info("%s S\n", __func__);
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&client->dev, "out of memory\n");
		return -ENOMEM;
	}

	mutex_init(&dev->input_lock);

	dev->fmt_idx = 0;
	v4l2_i2c_subdev_init(&(dev->sd), client, &gc0339_ops);

	if (client->dev.platform_data) {
		ret = gc0339_s_config(&dev->sd, client->irq,
				       client->dev.platform_data);
		if (ret)
			goto out_free;
	} else if (ACPI_COMPANION(&client->dev)) {
		/*
		 * If no SFI firmware, grab the platform struct
		 * directly and configure via ACPI/EFIvars instead
		 */
		ret = gc0339_s_config(&dev->sd, client->irq,
				      gmin_camera_platform_data());
		if (ret)
			goto out_free;
	}

	dev->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->pad.flags = MEDIA_PAD_FL_SOURCE;
	dev->format.code = V4L2_MBUS_FMT_SGRBG10_1X10;
	dev->sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;

	ret = media_entity_init(&dev->sd.entity, 1, &dev->pad, 0);
	if (ret)
		gc0339_remove(client);

	pr_info("%s E\n", __func__);

	return ret;
out_free:
	v4l2_device_unregister_subdev(&dev->sd);
	kfree(dev);
	return ret;
}

static struct acpi_device_id gc0339_acpi_match[] = {
	{"INT33F9"},
	{},
};
MODULE_DEVICE_TABLE(acpi, gc0339_acpi_match);

MODULE_DEVICE_TABLE(i2c, gc0339_id);
static struct i2c_driver gc0339_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = GC0339_NAME,
		.acpi_match_table = ACPI_PTR(gc0339_acpi_match),
	},
	.probe = gc0339_probe,
	.remove = gc0339_remove,
	.id_table = gc0339_id,
};

static int init_gc0339(void)
{
	return i2c_add_driver(&gc0339_driver);
}

static void exit_gc0339(void)
{

	i2c_del_driver(&gc0339_driver);
}

module_init(init_gc0339);
module_exit(exit_gc0339);

MODULE_AUTHOR("Lai, Angie <angie.lai@intel.com>");
MODULE_DESCRIPTION("A low-level driver for GalaxyCore GC0339 sensors");
MODULE_LICENSE("GPL");
