/*
 * Support for OmniVision S5K6B2YX 1080p HD camera sensor.
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
#include <media/v4l2-chip-ident.h>
#include <linux/io.h>

#include "s5k6b2yx.h"

/* i2c read/write stuff */
static int s5k6b2yx_read_reg(struct i2c_client *client,
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

	if (data_length != S5K6B2YX_8BIT && data_length != S5K6B2YX_16BIT
					&& data_length != S5K6B2YX_32BIT) {
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
	if (data_length == S5K6B2YX_8BIT)
		*val = (u8)data[0];
	else if (data_length == S5K6B2YX_16BIT)
		*val = be16_to_cpu(*(u16 *)&data[0]);
	else
		*val = be32_to_cpu(*(u32 *)&data[0]);

	return 0;
}

static int s5k6b2yx_i2c_write(struct i2c_client *client, u16 len, u8 *data)
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

static int s5k6b2yx_write_reg(struct i2c_client *client, u16 data_length,
							u16 reg, u16 val)
{
	int ret;
	unsigned char data[4] = {0};
	u16 *wreg = (u16 *)data;
	const u16 len = data_length + sizeof(u16); /* 16-bit address + data */

	if (data_length != S5K6B2YX_8BIT && data_length != S5K6B2YX_16BIT) {
		dev_err(&client->dev,
			"%s error, invalid data_length\n", __func__);
		return -EINVAL;
	}

	/* high byte goes out first */
	*wreg = cpu_to_be16(reg);

	if (data_length == S5K6B2YX_8BIT) {
		data[2] = (u8)(val);
	} else {
		/* S5K6B2YX_16BIT */
		u16 *wdata = (u16 *)&data[2];
		*wdata = cpu_to_be16(val);
	}

	ret = s5k6b2yx_i2c_write(client, len, data);
	if (ret)
		dev_err(&client->dev,
			"write error: wrote 0x%x to offset 0x%x error %d",
			val, reg, ret);

	return ret;
}

/*
 * s5k6b2yx_write_reg_array - Initializes a list of S5K6B2YX registers
 * @client: i2c driver client structure
 * @reglist: list of registers to be written
 *
 * This function initializes a list of registers. When consecutive addresses
 * are found in a row on the list, this function creates a buffer and sends
 * consecutive data in a single i2c_transfer().
 *
 * __s5k6b2yx_flush_reg_array, __s5k6b2yx_buf_reg_array() and
 * __s5k6b2yx_write_reg_is_consecutive() are internal functions to
 * s5k6b2yx_write_reg_array_fast() and should be not used anywhere else.
 *
 */

static int __s5k6b2yx_flush_reg_array(struct i2c_client *client,
				    struct s5k6b2yx_write_ctrl *ctrl)
{
	u16 size;

	if (ctrl->index == 0)
		return 0;

	size = sizeof(u16) + ctrl->index; /* 16-bit address + data */
	ctrl->buffer.addr = cpu_to_be16(ctrl->buffer.addr);
	ctrl->index = 0;

	return s5k6b2yx_i2c_write(client, size, (u8 *)&ctrl->buffer);
}

static int __s5k6b2yx_buf_reg_array(struct i2c_client *client,
				  struct s5k6b2yx_write_ctrl *ctrl,
				  const struct s5k6b2yx_reg *next)
{
	int size;
	u16 *data16;

	switch (next->type) {
	case S5K6B2YX_8BIT:
		size = 1;
		ctrl->buffer.data[ctrl->index] = (u8)next->val;
		break;
	case S5K6B2YX_16BIT:
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
	if (ctrl->index + sizeof(u16) >= S5K6B2YX_MAX_WRITE_BUF_SIZE)
		return __s5k6b2yx_flush_reg_array(client, ctrl);

	return 0;
}

static int __s5k6b2yx_write_reg_is_consecutive(struct i2c_client *client,
					     struct s5k6b2yx_write_ctrl *ctrl,
					     const struct s5k6b2yx_reg *next)
{
	if (ctrl->index == 0)
		return 1;

	return ctrl->buffer.addr + ctrl->index == next->reg;
}

static int s5k6b2yx_write_reg_array(struct i2c_client *client,
				  const struct s5k6b2yx_reg *reglist)
{
	const struct s5k6b2yx_reg *next = reglist;
	struct s5k6b2yx_write_ctrl ctrl;
	int err;

	ctrl.index = 0;
	for (; next->type != S5K6B2YX_TOK_TERM; next++) {
		switch (next->type & S5K6B2YX_TOK_MASK) {
		case S5K6B2YX_TOK_DELAY:
			err = __s5k6b2yx_flush_reg_array(client, &ctrl);
			if (err)
				return err;
			msleep(next->val);
			break;
		default:
			/*
			 * If next address is not consecutive, data needs to be
			 * flushed before proceed.
			 */
			if (!__s5k6b2yx_write_reg_is_consecutive(client, &ctrl,
								next)) {
				err = __s5k6b2yx_flush_reg_array(client, &ctrl);
				if (err)
					return err;
			}
			err = __s5k6b2yx_buf_reg_array(client, &ctrl, next);
			if (err) {
				dev_err(&client->dev, "%s: write error, aborted\n",
					 __func__);
				return err;
			}
			break;
		}
	}

	return __s5k6b2yx_flush_reg_array(client, &ctrl);
}
static int s5k6b2yx_g_focal(struct v4l2_subdev *sd, s32 *val)
{
	*val = (S5K6B2YX_FOCAL_LENGTH_NUM << 16) | S5K6B2YX_FOCAL_LENGTH_DEM;
	return 0;
}

static int s5k6b2yx_g_fnumber(struct v4l2_subdev *sd, s32 *val)
{
	/*const f number for imx*/
	*val = (S5K6B2YX_F_NUMBER_DEFAULT_NUM << 16) | S5K6B2YX_F_NUMBER_DEM;
	return 0;
}

static int s5k6b2yx_g_fnumber_range(struct v4l2_subdev *sd, s32 *val)
{
	*val = (S5K6B2YX_F_NUMBER_DEFAULT_NUM << 24) |
		(S5K6B2YX_F_NUMBER_DEM << 16) |
		(S5K6B2YX_F_NUMBER_DEFAULT_NUM << 8) | S5K6B2YX_F_NUMBER_DEM;
	return 0;
}

static int s5k6b2yx_get_intg_factor(struct i2c_client *client,
				struct camera_mipi_info *info,
				const struct s5k6b2yx_resolution *res)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct s5k6b2yx_device *dev = to_s5k6b2yx_sensor(sd);
	struct atomisp_sensor_mode_data *buf = &info->data;
	const unsigned int ext_clk_freq_hz = 19200000;
	unsigned int pix_clk_freq_hz;
	u16 pre_pll_clk_div;
	u16 pll_multiplier;
	u16 vt_sys_clk_div;
	u16 vt_pix_clk_div;
	u16 reg_val;
	int ret;

	if (info == NULL)
		return -EINVAL;

	/* pixel clock calculattion */
	ret =  s5k6b2yx_read_reg(client, S5K6B2YX_16BIT,
				S5K6B2YX_REG_PRE_PLL_CLK_DIV_H,
				&pre_pll_clk_div);
	if (ret)
		return ret;

	ret =  s5k6b2yx_read_reg(client, S5K6B2YX_16BIT,
				S5K6B2YX_REG_PLL_MULTIPLIER_H,
				&pll_multiplier);
	if (ret)
		return ret;

	ret =  s5k6b2yx_read_reg(client, S5K6B2YX_16BIT,
				S5K6B2YX_REG_VT_PIX_CLK_DIV_H,
				&vt_pix_clk_div);
	if (ret)
		return ret;

	ret =  s5k6b2yx_read_reg(client, S5K6B2YX_16BIT,
				S5K6B2YX_REG_VT_SYS_CLK_DIV_H,
				&vt_sys_clk_div);
	if (ret)
		return ret;

	pre_pll_clk_div = (pre_pll_clk_div & 0x70) >> 4;
	if (0 == pre_pll_clk_div)
		return -EINVAL;

	pix_clk_freq_hz = ext_clk_freq_hz * pll_multiplier / pre_pll_clk_div
				/ vt_sys_clk_div / vt_pix_clk_div;

	dev->vt_pix_clk_freq_mhz = pix_clk_freq_hz;
	buf->vt_pix_clk_freq_mhz = pix_clk_freq_hz;

	/* get integration time */
	buf->coarse_integration_time_min = S5K6B2YX_COARSE_INTG_TIME_MIN;
	buf->coarse_integration_time_max_margin =
					S5K6B2YX_COARSE_INTG_TIME_MAX_MARGIN;

	buf->fine_integration_time_min = S5K6B2YX_FINE_INTG_TIME_MIN;
	buf->fine_integration_time_max_margin =
					S5K6B2YX_FINE_INTG_TIME_MAX_MARGIN;

	buf->fine_integration_time_def = S5K6B2YX_FINE_INTG_TIME_MIN;
	buf->frame_length_lines = res->lines_per_frame;
	buf->line_length_pck = res->pixels_per_line;
	buf->read_mode = res->bin_mode;

	/* get the cropping and output resolution to ISP for this mode. */
	ret =  s5k6b2yx_read_reg(client, S5K6B2YX_16BIT,
					S5K6B2YX_REG_H_CROP_START_H, &reg_val);
	if (ret)
		return ret;
	buf->crop_horizontal_start = reg_val;

	ret =  s5k6b2yx_read_reg(client, S5K6B2YX_16BIT,
					S5K6B2YX_REG_V_CROP_START_H, &reg_val);
	if (ret)
		return ret;
	buf->crop_vertical_start = reg_val;

	ret = s5k6b2yx_read_reg(client, S5K6B2YX_16BIT,
					S5K6B2YX_REG_H_CROP_END_H, &reg_val);
	if (ret)
		return ret;
	buf->crop_horizontal_end = reg_val;

	ret = s5k6b2yx_read_reg(client, S5K6B2YX_16BIT,
					S5K6B2YX_REG_V_CROP_END_H, &reg_val);
	if (ret)
		return ret;
	buf->crop_vertical_end = reg_val;

	ret = s5k6b2yx_read_reg(client, S5K6B2YX_16BIT,
					S5K6B2YX_REG_H_OUTSIZE_H, &reg_val);
	if (ret)
		return ret;
	buf->output_width = reg_val;

	ret = s5k6b2yx_read_reg(client, S5K6B2YX_16BIT,
					S5K6B2YX_REG_V_OUTSIZE_H, &reg_val);
	if (ret)
		return ret;
	buf->output_height = reg_val;

	buf->binning_factor_x = res->bin_factor_x ?
					res->bin_factor_x : 1;
	buf->binning_factor_y = res->bin_factor_y ?
					res->bin_factor_y : 1;
	return 0;
}

static long __s5k6b2yx_set_exposure(struct v4l2_subdev *sd, int coarse_itg,
				 int gain, int digitgain)

{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;


	/*
	 * FIXME!
	 * TBD: Tuning for exposure and gain control
	 */

	/* group hold start */
	ret = s5k6b2yx_write_reg(client, S5K6B2YX_8BIT,
			S5K6B2YX_REG_GROUND_HOLD, 1);
	if (ret)
		return ret;

	/* set exposure */
	ret = s5k6b2yx_write_reg(client, S5K6B2YX_16BIT,
					S5K6B2YX_REG_COARSE_INTEG,
					coarse_itg);
	if (ret)
		return ret;

	/* set analog gain */
	/*
	 * DS: the gain of pixel signal is controlled by Analog Gain Control
	 * Register (analogue_gain_code_global), here is the equation:
	 * Analog Gain = (m0 * x + c0) / (m1 * x + c1)
	 *
	 * S5K6N2YX specifies analog gain by coefficients of m0 = 1, m1 = 0,
	 * c1 = 32. As a result, here is the equation:
	 * Analog Gain = analogue_gain_code_global[12:0] / 32
	 *
	 * The miminum value of analogure_gain_code_global[12:0] is
	 * recommended to 32d, which means that the analog gain should be less
	 * than x1.
	 */
	ret = s5k6b2yx_write_reg(client, S5K6B2YX_16BIT,
					S5K6B2YX_REG_ANALOG_GAIN, gain);
	if (ret)
		return ret;

	/* There is no digital gain control */

	/* group hold end */
	ret = s5k6b2yx_write_reg(client, S5K6B2YX_8BIT,
					S5K6B2YX_REG_GROUND_HOLD, 0);
	if (ret)
		return ret;

	return ret;
}

static int s5k6b2yx_set_exposure(struct v4l2_subdev *sd, int exposure,
	int gain, int digitgain)
{
	struct s5k6b2yx_device *dev = to_s5k6b2yx_sensor(sd);
	int ret;

	mutex_lock(&dev->input_lock);
	ret = __s5k6b2yx_set_exposure(sd, exposure, gain, digitgain);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static long s5k6b2yx_s_exposure(struct v4l2_subdev *sd,
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

	return s5k6b2yx_set_exposure(sd, exp, gain, digitgain);
}

static long s5k6b2yx_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{

	switch (cmd) {
	case ATOMISP_IOC_S_EXPOSURE:
		return s5k6b2yx_s_exposure(sd, arg);
	default:
		return -EINVAL;
	}
	return 0;
}

/* This returns the exposure time being used. This should only be used
   for filling in EXIF data, not for actual image processing. */
static int s5k6b2yx_q_exposure(struct v4l2_subdev *sd, s32 *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u16 coarse;
	int ret;

	/* get exposure */
	ret = s5k6b2yx_read_reg(client, S5K6B2YX_16BIT,
					S5K6B2YX_REG_COARSE_INTEG,
					&coarse);
	if (ret)
		goto err;

	*value = coarse;
err:
	return ret;
}
struct s5k6b2yx_control s5k6b2yx_controls[] = {
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
		.query = s5k6b2yx_q_exposure,
	},
	{
		.qc = {
			.id = V4L2_CID_FOCAL_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focal length",
			.minimum = S5K6B2YX_FOCAL_LENGTH_DEFAULT,
			.maximum = S5K6B2YX_FOCAL_LENGTH_DEFAULT,
			.step = 0x01,
			.default_value = S5K6B2YX_FOCAL_LENGTH_DEFAULT,
			.flags = 0,
		},
		.query = s5k6b2yx_g_focal,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number",
			.minimum = S5K6B2YX_F_NUMBER_DEFAULT,
			.maximum = S5K6B2YX_F_NUMBER_DEFAULT,
			.step = 0x01,
			.default_value = S5K6B2YX_F_NUMBER_DEFAULT,
			.flags = 0,
		},
		.query = s5k6b2yx_g_fnumber,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_RANGE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number range",
			.minimum = S5K6B2YX_F_NUMBER_RANGE,
			.maximum =  S5K6B2YX_F_NUMBER_RANGE,
			.step = 0x01,
			.default_value = S5K6B2YX_F_NUMBER_RANGE,
			.flags = 0,
		},
		.query = s5k6b2yx_g_fnumber_range,
	},
};
#define N_CONTROLS (ARRAY_SIZE(s5k6b2yx_controls))

static int s5k6b2yx_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	struct s5k6b2yx_device *dev = container_of(ctrl->handler,
			struct s5k6b2yx_device, ctrl_handler);
	unsigned int val;

	switch (ctrl->id) {
	case V4L2_CID_LINK_FREQ:
		val = s5k6b2yx_res[dev->fmt_idx].mipi_freq;
		if (val == 0)
			return -EINVAL;

		ctrl->val = val * 1000;			/* To Hz */
		break;
	default:
		return -EINVAL;
	}

	return 0;
}


static struct s5k6b2yx_control *s5k6b2yx_find_control(u32 id)
{
	int i;

	for (i = 0; i < N_CONTROLS; i++)
		if (s5k6b2yx_controls[i].qc.id == id)
			return &s5k6b2yx_controls[i];
	return NULL;
}

static int s5k6b2yx_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	struct s5k6b2yx_control *ctrl = s5k6b2yx_find_control(qc->id);
	struct s5k6b2yx_device *dev = to_s5k6b2yx_sensor(sd);

	if (ctrl == NULL)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	*qc = ctrl->qc;
	mutex_unlock(&dev->input_lock);

	return 0;
}

/* imx control set/get */
static int s5k6b2yx_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct s5k6b2yx_control *s_ctrl;
	struct s5k6b2yx_device *dev = to_s5k6b2yx_sensor(sd);
	int ret;

	if (!ctrl)
		return -EINVAL;

	s_ctrl = s5k6b2yx_find_control(ctrl->id);
	if ((s_ctrl == NULL) || (s_ctrl->query == NULL))
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	ret = s_ctrl->query(sd, &ctrl->value);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int s5k6b2yx_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct s5k6b2yx_control *octrl = s5k6b2yx_find_control(ctrl->id);
	struct s5k6b2yx_device *dev = to_s5k6b2yx_sensor(sd);
	int ret;

	if ((octrl == NULL) || (octrl->tweak == NULL))
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	ret = octrl->tweak(sd, ctrl->value);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int __s5k6b2yx_init(struct v4l2_subdev *sd)
{
	struct s5k6b2yx_device *dev = to_s5k6b2yx_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	mutex_lock(&dev->input_lock);

	/* restore settings */
	s5k6b2yx_res = s5k6b2yx_res_preview;
	N_RES = N_RES_PREVIEW;

	ret = s5k6b2yx_write_reg_array(client, s5k6b2yx_init);
	if (ret) {
		dev_err(&client->dev, "s5k6b2yx write init registers err.\n");
		return ret;
	}

	mutex_unlock(&dev->input_lock);

	return 0;
}


static int power_up(struct v4l2_subdev *sd)
{
	struct s5k6b2yx_device *dev = to_s5k6b2yx_sensor(sd);
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

	/* according to DS, at least 5ms is needed between DOVDD and PWDN */
	usleep_range(5000, 6000);

	/* gpio ctrl */
	ret = dev->platform_data->gpio_ctrl(sd, 1);
	if (ret) {
		ret = dev->platform_data->gpio_ctrl(sd, 1);
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
	dev->platform_data->gpio_ctrl(sd, 0);
fail_power:
	dev->platform_data->power_ctrl(sd, 0);
	dev_err(&client->dev, "sensor power-up failed\n");

	return ret;
}

static int power_down(struct v4l2_subdev *sd)
{
	struct s5k6b2yx_device *dev = to_s5k6b2yx_sensor(sd);
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
	if (ret) {
		ret = dev->platform_data->gpio_ctrl(sd, 0);
		if (ret)
			dev_err(&client->dev, "gpio failed 2\n");
	}

	/* power control */
	ret = dev->platform_data->power_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "vprog failed.\n");

	return ret;
}

static int s5k6b2yx_s_power(struct v4l2_subdev *sd, int on)
{
	int ret;
	if (on == 0)
		return power_down(sd);
	else {
		ret = power_up(sd);
		if (!ret)
			return __s5k6b2yx_init(sd);
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
static int distance(struct s5k6b2yx_resolution *res, u32 w, u32 h)
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
	struct s5k6b2yx_resolution *tmp_res = NULL;

	for (i = 0; i < N_RES; i++) {
		tmp_res = &s5k6b2yx_res[i];
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
		if (w != s5k6b2yx_res[i].width)
			continue;
		if (h != s5k6b2yx_res[i].height)
			continue;

		return i;
	}

	return -1;
}

static int s5k6b2yx_try_mbus_fmt(struct v4l2_subdev *sd,
			struct v4l2_mbus_framefmt *fmt)
{
	int idx;

	if (!fmt)
		return -EINVAL;
	idx = nearest_resolution_index(fmt->width,
					fmt->height);
	if (idx == -1) {
		/* return the largest resolution */
		fmt->width = s5k6b2yx_res[N_RES - 1].width;
		fmt->height = s5k6b2yx_res[N_RES - 1].height;
	} else {
		fmt->width = s5k6b2yx_res[idx].width;
		fmt->height = s5k6b2yx_res[idx].height;
	}
	fmt->code = V4L2_MBUS_FMT_SGRBG10_1X10;

	return 0;
}

static int s5k6b2yx_s_mbus_fmt(struct v4l2_subdev *sd,
			     struct v4l2_mbus_framefmt *fmt)
{
	struct s5k6b2yx_device *dev = to_s5k6b2yx_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camera_mipi_info *s5k6b2yx_info = NULL;
	int ret = 0;

	s5k6b2yx_info = v4l2_get_subdev_hostdata(sd);
	if (s5k6b2yx_info == NULL)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	ret = s5k6b2yx_try_mbus_fmt(sd, fmt);
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

	ret = s5k6b2yx_write_reg_array(client,
			s5k6b2yx_res[dev->fmt_idx].regs);
	if (ret) {
		dev_err(&client->dev, "s5k6b2yx write fmt register err.\n");
		return ret;
	}

	ret = s5k6b2yx_get_intg_factor(client, s5k6b2yx_info,
					&s5k6b2yx_res[dev->fmt_idx]);
	if (ret)
		dev_err(&client->dev, "failed to get integration_factor\n");

err:
	mutex_unlock(&dev->input_lock);
	return ret;
}
static int s5k6b2yx_g_mbus_fmt(struct v4l2_subdev *sd,
			     struct v4l2_mbus_framefmt *fmt)
{
	struct s5k6b2yx_device *dev = to_s5k6b2yx_sensor(sd);

	if (!fmt)
		return -EINVAL;

	fmt->width = s5k6b2yx_res[dev->fmt_idx].width;
	fmt->height = s5k6b2yx_res[dev->fmt_idx].height;
	fmt->code = V4L2_MBUS_FMT_SBGGR10_1X10;

	return 0;
}

static int s5k6b2yx_detect(struct i2c_client *client)
{
	struct i2c_adapter *adapter = client->adapter;
	u16 id;
	u16 revision;
	int ret;

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -ENODEV;

	ret = s5k6b2yx_read_reg(client, S5K6B2YX_16BIT,
					S5K6B2YX_REG_CHIP_ID_H, &id);
	if (ret) {
		dev_err(&client->dev, "Err read sensor_id 0x%x\n", ret);
		return -ENODEV;
	}

	if ((id != S5K6B2YX_ID)) {
		dev_err(&client->dev, "sensor ID error\n");
		return -ENODEV;
	}

	ret = s5k6b2yx_read_reg(client, S5K6B2YX_8BIT,
					S5K6B2YX_REG_REVISION, &revision);
	if (ret) {
		dev_err(&client->dev, "Err read sensor revision 0x%x\n", ret);
		return -ENODEV;
	}

	dev_dbg(&client->dev, "sensor_revision = 0x%x\n", revision);
	dev_dbg(&client->dev, "detect s5k6b2yx success\n");
	return 0;
}

static int s5k6b2yx_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct s5k6b2yx_device *dev = to_s5k6b2yx_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	mutex_lock(&dev->input_lock);

	ret = s5k6b2yx_write_reg(client, S5K6B2YX_8BIT,
			S5K6B2YX_REG_MODE_SELECT,
			enable ? S5K6B2YX_START_STREAMING :
			S5K6B2YX_STOP_STREAMING);

	mutex_unlock(&dev->input_lock);
	return ret;
}

/* s5k6b2yx enum frame size, frame intervals */
static int s5k6b2yx_enum_framesizes(struct v4l2_subdev *sd,
				  struct v4l2_frmsizeenum *fsize)
{
	unsigned int index = fsize->index;

	if (index >= N_RES)
		return -EINVAL;

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = s5k6b2yx_res[index].width;
	fsize->discrete.height = s5k6b2yx_res[index].height;
	fsize->reserved[0] = s5k6b2yx_res[index].used;

	return 0;
}

static int s5k6b2yx_enum_frameintervals(struct v4l2_subdev *sd,
				      struct v4l2_frmivalenum *fival)
{
	unsigned int index = fival->index;

	if (index >= N_RES)
		return -EINVAL;

	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fival->width = s5k6b2yx_res[index].width;
	fival->height = s5k6b2yx_res[index].height;
	fival->discrete.numerator = 1;
	fival->discrete.denominator = s5k6b2yx_res[index].fps;

	return 0;
}

static int s5k6b2yx_enum_mbus_fmt(struct v4l2_subdev *sd,
				unsigned int index,
				enum v4l2_mbus_pixelcode *code)
{
	*code = V4L2_MBUS_FMT_SBGGR10_1X10;

	return 0;
}

static int s5k6b2yx_s_config(struct v4l2_subdev *sd,
			   int irq, void *platform_data)
{
	struct s5k6b2yx_device *dev = to_s5k6b2yx_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	if (platform_data == NULL)
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

	/* power off the module, then power on it in future
	 * as first power on by board may not fulfill the
	 * power on sequqence needed by the module
	 */
	ret = power_down(sd);
	if (ret) {
		dev_err(&client->dev, "s5k6b2yx power-off err.\n");
		goto fail_power_off;
	}

	ret = power_up(sd);
	if (ret) {
		dev_err(&client->dev, "s5k6b2yx power-up err.\n");
		goto fail_power_on;
	}

	ret = dev->platform_data->csi_cfg(sd, 1);
	if (ret)
		goto fail_csi_cfg;

	/* config & detect sensor */
	ret = s5k6b2yx_detect(client);
	if (ret) {
		dev_err(&client->dev, "s5k6b2yx_detect err s_config.\n");
		goto fail_csi_cfg;
	}

	/* turn off sensor, after probed */
	ret = power_down(sd);
	if (ret) {
		dev_err(&client->dev, "s5k6b2yx power-off err.\n");
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
	if (dev->platform_data->platform_deinit)
		dev->platform_data->platform_deinit();
platform_init_failed:
	mutex_unlock(&dev->input_lock);
	return ret;
}

static int s5k6b2yx_g_parm(struct v4l2_subdev *sd,
			struct v4l2_streamparm *param)
{
	struct s5k6b2yx_device *dev = to_s5k6b2yx_sensor(sd);
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
			s5k6b2yx_res[dev->fmt_idx].fps;
	}
	return 0;
}

static int s5k6b2yx_s_parm(struct v4l2_subdev *sd,
			struct v4l2_streamparm *param)
{
	struct s5k6b2yx_device *dev = to_s5k6b2yx_sensor(sd);
	dev->run_mode = param->parm.capture.capturemode;

	mutex_lock(&dev->input_lock);
	switch (dev->run_mode) {
	case CI_MODE_VIDEO:
		s5k6b2yx_res = s5k6b2yx_res_video;
		N_RES = N_RES_VIDEO;
		break;
	case CI_MODE_STILL_CAPTURE:
		s5k6b2yx_res = s5k6b2yx_res_still;
		N_RES = N_RES_STILL;
		break;
	default:
		s5k6b2yx_res = s5k6b2yx_res_preview;
		N_RES = N_RES_PREVIEW;
	}
	mutex_unlock(&dev->input_lock);
	return 0;
}

static int s5k6b2yx_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *interval)
{
	struct s5k6b2yx_device *dev = to_s5k6b2yx_sensor(sd);

	interval->interval.numerator = 1;
	interval->interval.denominator = s5k6b2yx_res[dev->fmt_idx].fps;

	return 0;
}

static int s5k6b2yx_enum_mbus_code(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index >= MAX_FMTS)
		return -EINVAL;

	code->code = V4L2_MBUS_FMT_SBGGR10_1X10;
	return 0;
}

static int s5k6b2yx_enum_frame_size(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_frame_size_enum *fse)
{
	int index = fse->index;

	if (index >= N_RES)
		return -EINVAL;

	fse->min_width = s5k6b2yx_res[index].width;
	fse->min_height = s5k6b2yx_res[index].height;
	fse->max_width = s5k6b2yx_res[index].width;
	fse->max_height = s5k6b2yx_res[index].height;

	return 0;

}

static struct v4l2_mbus_framefmt *
__s5k6b2yx_get_pad_format(struct s5k6b2yx_device *sensor,
			struct v4l2_subdev_fh *fh, unsigned int pad,
			enum v4l2_subdev_format_whence which)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->sd);

	if (pad != 0) {
		dev_err(&client->dev,
			"__s5k6b2yx_get_pad_format err. pad %x\n", pad);
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

static int s5k6b2yx_get_pad_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	struct s5k6b2yx_device *snr = to_s5k6b2yx_sensor(sd);
	struct v4l2_mbus_framefmt *format =
			__s5k6b2yx_get_pad_format(snr, fh, fmt->pad,
					fmt->which);
	if (!format)
		return -EINVAL;

	fmt->format = *format;
	return 0;
}

static int s5k6b2yx_set_pad_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	struct s5k6b2yx_device *snr = to_s5k6b2yx_sensor(sd);

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		snr->format = fmt->format;

	return 0;
}

static int s5k6b2yx_g_skip_frames(struct v4l2_subdev *sd, u32 *frames)
{
	struct s5k6b2yx_device *dev = to_s5k6b2yx_sensor(sd);

	mutex_lock(&dev->input_lock);
	*frames = s5k6b2yx_res[dev->fmt_idx].skip_frames;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static const struct v4l2_subdev_sensor_ops s5k6b2yx_sensor_ops = {
	.g_skip_frames	= s5k6b2yx_g_skip_frames,
};

static struct v4l2_ctrl_ops s5k6b2yx_ctrl_ops = {
	.g_volatile_ctrl = s5k6b2yx_g_volatile_ctrl,
};

static const struct v4l2_ctrl_config v4l2_ctrl_link_freq = {
	.ops = &s5k6b2yx_ctrl_ops,
	.id = V4L2_CID_LINK_FREQ,
	.name = "Link Frequency",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 1,
	.max = 1500000 * 1000,
	.step = 1,
	.def = 1,
	.flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_READ_ONLY,
};

static const struct v4l2_subdev_video_ops s5k6b2yx_video_ops = {
	.s_stream = s5k6b2yx_s_stream,
	.g_parm = s5k6b2yx_g_parm,
	.s_parm = s5k6b2yx_s_parm,
	.enum_framesizes = s5k6b2yx_enum_framesizes,
	.enum_frameintervals = s5k6b2yx_enum_frameintervals,
	.enum_mbus_fmt = s5k6b2yx_enum_mbus_fmt,
	.try_mbus_fmt = s5k6b2yx_try_mbus_fmt,
	.g_mbus_fmt = s5k6b2yx_g_mbus_fmt,
	.s_mbus_fmt = s5k6b2yx_s_mbus_fmt,
	.g_frame_interval = s5k6b2yx_g_frame_interval,
};

static const struct v4l2_subdev_core_ops s5k6b2yx_core_ops = {
	.s_power = s5k6b2yx_s_power,
	.queryctrl = s5k6b2yx_queryctrl,
	.g_ctrl = s5k6b2yx_g_ctrl,
	.s_ctrl = s5k6b2yx_s_ctrl,
	.ioctl = s5k6b2yx_ioctl,
};

static const struct v4l2_subdev_pad_ops s5k6b2yx_pad_ops = {
	.enum_mbus_code = s5k6b2yx_enum_mbus_code,
	.enum_frame_size = s5k6b2yx_enum_frame_size,
	.get_fmt = s5k6b2yx_get_pad_format,
	.set_fmt = s5k6b2yx_set_pad_format,
};

static const struct v4l2_subdev_ops s5k6b2yx_ops = {
	.core = &s5k6b2yx_core_ops,
	.video = &s5k6b2yx_video_ops,
	.pad = &s5k6b2yx_pad_ops,
	.sensor = &s5k6b2yx_sensor_ops,
};

static int s5k6b2yx_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct s5k6b2yx_device *dev = to_s5k6b2yx_sensor(sd);
	dev_dbg(&client->dev, "s5k6b2yx_remove...\n");

	if (dev->platform_data->platform_deinit)
		dev->platform_data->platform_deinit();

	dev->platform_data->csi_cfg(sd, 0);
	v4l2_ctrl_handler_free(&dev->ctrl_handler);
	v4l2_device_unregister_subdev(sd);
	media_entity_cleanup(&dev->sd.entity);
	kfree(dev);

	return 0;
}

static int __s5k6b2yx_init_ctrl_handler(struct s5k6b2yx_device *dev)
{
	struct v4l2_ctrl_handler *hdl;

	hdl = &dev->ctrl_handler;

	v4l2_ctrl_handler_init(&dev->ctrl_handler, 3);

	dev->link_freq = v4l2_ctrl_new_custom(&dev->ctrl_handler,
					      &v4l2_ctrl_link_freq,
					      NULL);

	if (dev->ctrl_handler.error || dev->link_freq == NULL)
		return dev->ctrl_handler.error;

	dev->sd.ctrl_handler = hdl;

	return 0;
}
static int s5k6b2yx_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct s5k6b2yx_device *dev;
	int ret;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&client->dev, "out of memory\n");
		return -ENOMEM;
	}

	mutex_init(&dev->input_lock);

	dev->fmt_idx = 0;
	v4l2_i2c_subdev_init(&(dev->sd), client, &s5k6b2yx_ops);

	if (client->dev.platform_data) {
		ret = s5k6b2yx_s_config(&dev->sd, client->irq,
				       client->dev.platform_data);
		if (ret)
			goto out_free;
	}

	ret = __s5k6b2yx_init_ctrl_handler(dev);
	if (ret)
		goto out_ctrl_handler_free;

	dev->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->pad.flags = MEDIA_PAD_FL_SOURCE;
	dev->format.code = V4L2_MBUS_FMT_SBGGR10_1X10;
	dev->sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;

	ret = media_entity_init(&dev->sd.entity, 1, &dev->pad, 0);
	if (ret)
		s5k6b2yx_remove(client);

	return ret;

out_ctrl_handler_free:
	v4l2_ctrl_handler_free(&dev->ctrl_handler);

out_free:
	v4l2_device_unregister_subdev(&dev->sd);
	kfree(dev);
	return ret;
}

MODULE_DEVICE_TABLE(i2c, s5k6b2yx_id);
static struct i2c_driver s5k6b2yx_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = S5K6B2YX_NAME,
	},
	.probe = s5k6b2yx_probe,
	.remove = s5k6b2yx_remove,
	.id_table = s5k6b2yx_id,
};

static int init_s5k6b2yx(void)
{
	return i2c_add_driver(&s5k6b2yx_driver);
}

static void exit_s5k6b2yx(void)
{

	i2c_del_driver(&s5k6b2yx_driver);
}

module_init(init_s5k6b2yx);
module_exit(exit_s5k6b2yx);

MODULE_AUTHOR("Tao Jing <jing.tao@intel.com>");
MODULE_DESCRIPTION("A low-level driver for SANGSUME S5K6B2YX sensors");
MODULE_LICENSE("GPL");
