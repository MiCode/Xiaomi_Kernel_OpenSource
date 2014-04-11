/*
 * Support for Sony imx 8MP camera sensor.
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
#include "imx.h"

static int
imx_read_reg(struct i2c_client *client, u16 len, u16 reg, void *val, bool otpck)
{
	struct i2c_msg msg[2];
	u16 data[IMX_SHORT_MAX] = { 0 };
	int err, i;

	if (len > IMX_BYTE_MAX) {
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

	if (otpck == 1) {
		memcpy(val, data, len);
		otpck = 0;
	} else if (len == IMX_8BIT) {
		*((u16 *)val) = (u8)data[0];
	} else {
		/* 16-bit access is default when len > 1 */
		for (i = 0; i < (len >> 1); i++)
			((u16 *)val)[i] = be16_to_cpu(data[i]);
	}

	return 0;

error:
	dev_err(&client->dev, "read from offset 0x%x error %d", reg, err);
	return err;
}

static int imx_i2c_write(struct i2c_client *client, u16 len, u8 *data)
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
imx_write_reg(struct i2c_client *client, u16 data_length, u16 reg, u16 val)
{
	int ret;
	unsigned char data[4] = {0};
	u16 *wreg = (u16 *)data;
	const u16 len = data_length + sizeof(u16); /* 16-bit address + data */

	if (data_length != IMX_8BIT && data_length != IMX_16BIT) {
		v4l2_err(client, "%s error, invalid data_length\n", __func__);
		return -EINVAL;
	}

	/* high byte goes out first */
	*wreg = cpu_to_be16(reg);

	if (data_length == IMX_8BIT)
		data[2] = (u8)(val);
	else {
		/* IMX_16BIT */
		u16 *wdata = (u16 *)&data[2];
		*wdata = cpu_to_be16(val);
	}

	ret = imx_i2c_write(client, len, data);
	if (ret)
		dev_err(&client->dev,
			"write error: wrote 0x%x to offset 0x%x error %d",
			val, reg, ret);

	return ret;
}

/*
 * imx_write_reg_array - Initializes a list of imx registers
 * @client: i2c driver client structure
 * @reglist: list of registers to be written
 *
 * This function initializes a list of registers. When consecutive addresses
 * are found in a row on the list, this function creates a buffer and sends
 * consecutive data in a single i2c_transfer().
 *
 * __imx_flush_reg_array, __imx_buf_reg_array() and
 * __imx_write_reg_is_consecutive() are internal functions to
 * imx_write_reg_array_fast() and should be not used anywhere else.
 *
 */

static int __imx_flush_reg_array(struct i2c_client *client,
				     struct imx_write_ctrl *ctrl)
{
	u16 size;

	if (ctrl->index == 0)
		return 0;

	size = sizeof(u16) + ctrl->index; /* 16-bit address + data */
	ctrl->buffer.addr = cpu_to_be16(ctrl->buffer.addr);
	ctrl->index = 0;

	return imx_i2c_write(client, size, (u8 *)&ctrl->buffer);
}

static int __imx_buf_reg_array(struct i2c_client *client,
				   struct imx_write_ctrl *ctrl,
				   const struct imx_reg *next)
{
	int size;
	u16 *data16;

	switch (next->type) {
	case IMX_8BIT:
		size = 1;
		ctrl->buffer.data[ctrl->index] = (u8)next->val;
		break;
	case IMX_16BIT:
		size = 2;
		data16 = (u16 *)&ctrl->buffer.data[ctrl->index];
		*data16 = (u16)next->val;
		break;
	default:
		return -EINVAL;
	}

	/* When first item is added, we need to store its starting address */
	if (ctrl->index == 0)
		ctrl->buffer.addr = next->sreg;

	ctrl->index += size;

	/*
	 * Buffer cannot guarantee free space for u32? Better flush it to avoid
	 * possible lack of memory for next item.
	 */
	if (ctrl->index + sizeof(u16) >= IMX_MAX_WRITE_BUF_SIZE)
		return __imx_flush_reg_array(client, ctrl);

	return 0;
}

static int
__imx_write_reg_is_consecutive(struct i2c_client *client,
				   struct imx_write_ctrl *ctrl,
				   const struct imx_reg *next)
{
	if (ctrl->index == 0)
		return 1;

	return ctrl->buffer.addr + ctrl->index == next->sreg;
}

static int imx_write_reg_array(struct i2c_client *client,
				   const struct imx_reg *reglist)
{
	const struct imx_reg *next = reglist;
	struct imx_write_ctrl ctrl;
	int err;

	ctrl.index = 0;
	for (; next->type != IMX_TOK_TERM; next++) {
		switch (next->type & IMX_TOK_MASK) {
		case IMX_TOK_DELAY:
			err = __imx_flush_reg_array(client, &ctrl);
			if (err)
				return err;
			msleep(next->val);
			break;

		default:
			/*
			 * If next address is not consecutive, data needs to be
			 * flushed before proceed.
			 */
			if (!__imx_write_reg_is_consecutive(client, &ctrl,
								next)) {
				err = __imx_flush_reg_array(client, &ctrl);
				if (err)
					return err;
			}
			err = __imx_buf_reg_array(client, &ctrl, next);
			if (err) {
				v4l2_err(client, "%s: write error, aborted\n",
					 __func__);
				return err;
			}
			break;
		}
	}

	return __imx_flush_reg_array(client, &ctrl);
}

static int imx_read_otp_reg_array(struct i2c_client *client, u16 size, u16 addr,
				  u8 *buf)
{
	u16 index;
	int ret;

	for (index = 0; index + IMX_OTP_READ_ONETIME <= size;
					index += IMX_OTP_READ_ONETIME) {
		ret = imx_read_reg(client, IMX_OTP_READ_ONETIME, addr + index,
					&buf[index], 1);
		if (ret)
			return ret;
	}
	return 0;
}

static int __imx_otp_read(struct v4l2_subdev *sd, u8 *buf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	int i;

	for (i = 0; i < IMX_OTP_PAGE_MAX; i++) {

		/*set page NO.*/
		ret = imx_write_reg(client, IMX_8BIT,
			       IMX_OTP_PAGE_REG, i & 0xff);
		if (ret) {
			dev_err(&client->dev, "failed to prepare OTP page\n");
			return ret;
		}

		/*set read mode*/
		ret = imx_write_reg(client, IMX_8BIT,
			       IMX_OTP_MODE_REG, IMX_OTP_MODE_READ);
		if (ret) {
			dev_err(&client->dev, "failed to set OTP reading mode page");
			return ret;
		}

		/* Reading the OTP data array */
		ret = imx_read_otp_reg_array(client, IMX_OTP_PAGE_SIZE,
			IMX_OTP_START_ADDR, buf + i * IMX_OTP_PAGE_SIZE);
		if (ret) {
			dev_err(&client->dev, "failed to read OTP data\n");
			return ret;
		}
	}

	return 0;
}

static void *imx_otp_read(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u8 *buf;
	int ret;

	buf = devm_kzalloc(&client->dev, IMX_OTP_DATA_SIZE, GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	ret = __imx_otp_read(sd, buf);

	/* Driver has failed to find valid data */
	if (ret) {
		dev_err(&client->dev, "sensor found no valid OTP data\n");
		return ERR_PTR(ret);
	}

	return buf;
}
static long __imx_set_exposure(struct v4l2_subdev *sd, unsigned int coarse_itg,
			       unsigned int gain, unsigned int digitgain)

{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	struct imx_device *dev = to_imx_sensor(sd);
	u16 vts = dev->lines_per_frame;

	/* enable group hold */
	ret = imx_write_reg_array(client, imx_param_hold);
	if (ret)
		goto out;

	/* imx max exposure is VTS-4 */
	if (coarse_itg > dev->lines_per_frame - IMX_INTEGRATION_TIME_MARGIN)
		vts = coarse_itg + IMX_INTEGRATION_TIME_MARGIN;

	ret = imx_write_reg(client, IMX_16BIT, IMX_FRAME_LENGTH_LINES, vts);
	if (ret)
		return ret;

	ret = imx_write_reg(client, IMX_16BIT,
		IMX_COARSE_INTEGRATION_TIME, coarse_itg);
	if (ret)
		goto out;

	/* set global gain */
	ret = imx_write_reg(client, IMX_8BIT,
		IMX_GLOBAL_GAIN, gain);
	if (ret)
		goto out_disable;

	/* digital gain: GR */
	ret = imx_write_reg(client, IMX_8BIT,
			IMX_DGC_ADJ, (digitgain >> 8) & 0xFF);
	if (ret)
		return ret;
	ret = imx_write_reg(client, IMX_8BIT,
			IMX_DGC_ADJ+1, digitgain & 0xFF);
	if (ret)
		return ret;
	/* digital gain: R */
	ret = imx_write_reg(client, IMX_8BIT,
			IMX_DGC_ADJ+2, (digitgain >> 8) & 0xFF);
	if (ret)
		return ret;
	ret = imx_write_reg(client, IMX_8BIT,
			IMX_DGC_ADJ+3, digitgain & 0xFF);
	if (ret)
		return ret;
	/*  digital gain: B */
	ret = imx_write_reg(client, IMX_8BIT,
			IMX_DGC_ADJ+4, (digitgain >> 8) & 0xFF);
	if (ret)
		return ret;
	ret = imx_write_reg(client, IMX_8BIT,
			IMX_DGC_ADJ+5, digitgain & 0xFF);
	if (ret)
		return ret;
	/* digital gain: GB */
	ret = imx_write_reg(client, IMX_8BIT,
			IMX_DGC_ADJ+6, (digitgain >> 8) & 0xFF);
	if (ret)
		return ret;
	ret = imx_write_reg(client, IMX_8BIT,
			IMX_DGC_ADJ+7, digitgain & 0xFF);
	if (ret)
		return ret;

	dev->gain = gain;
	dev->digital_gain = digitgain;
	dev->coarse_itg = coarse_itg;

out_disable:
	/* disable group hold */
	imx_write_reg_array(client, imx_param_update);
out:
	return ret;
}

static int imx_set_exposure(struct v4l2_subdev *sd, int exposure,
	int gain, int digitgain)
{
	struct imx_device *dev = to_imx_sensor(sd);
	int ret;

	mutex_lock(&dev->input_lock);
	ret = __imx_set_exposure(sd, exposure, gain, digitgain);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static long imx_s_exposure(struct v4l2_subdev *sd,
			       struct atomisp_exposure *exposure)
{
	unsigned int exp = exposure->integration_time[0];
	unsigned int gain = exposure->gain[0];
	unsigned int digitgain = exposure->gain[1];

	return imx_set_exposure(sd, exp, gain, digitgain);
}

static int imx_g_priv_int_data(struct v4l2_subdev *sd,
				   struct v4l2_private_int_data *priv)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct imx_device *dev = to_imx_sensor(sd);
	u8 __user *to = priv->data;
	u32 read_size = priv->size;
	int ret;

	/* No need to copy data if size is 0 */
	if (!read_size)
		goto out;

	if (IS_ERR(dev->otp_data)) {
		dev_err(&client->dev, "OTP data not available");
		return PTR_ERR(dev->otp_data);
	}
	/* Correct read_size value only if bigger than maximum */
	if (read_size > IMX_OTP_DATA_SIZE)
		read_size = IMX_OTP_DATA_SIZE;

	ret = copy_to_user(to, dev->otp_data, read_size);
	if (ret) {
		dev_err(&client->dev, "%s: failed to copy OTP data to user\n",
			 __func__);
		return -EFAULT;
	}
out:
	/* Return correct size */
	priv->size = IMX_OTP_DATA_SIZE;

	return 0;
}

static int imx_init_registers(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = imx_write_reg_array(client, imx135_SwReset);
	ret |= imx_write_reg_array(client, imx_init_settings);

	return ret;
}

static int __imx_init(struct v4l2_subdev *sd, u32 val)
{
	int ret;

	/* restore settings */
	imx_res = imx_res_preview;
	N_RES = N_RES_PREVIEW;

	ret = imx_init_registers(sd);

	return ret;
}

static int imx_init(struct v4l2_subdev *sd, u32 val)
{
	struct imx_device *dev = to_imx_sensor(sd);
	int ret = 0;

	mutex_lock(&dev->input_lock);
	ret = __imx_init(sd, val);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static long imx_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{

	switch (cmd) {
	case ATOMISP_IOC_S_EXPOSURE:
		return imx_s_exposure(sd, arg);
	case ATOMISP_IOC_G_SENSOR_PRIV_INT_DATA:
		return imx_g_priv_int_data(sd, arg);
	default:
		return -EINVAL;
	}
	return 0;
}

static int power_up(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct imx_device *dev = to_imx_sensor(sd);
	int ret;

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
		dev_err(&client->dev, "gpio failed\n");
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
	struct imx_device *dev = to_imx_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

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

static int __imx_s_power(struct v4l2_subdev *sd, int on)
{
	struct imx_device *dev = to_imx_sensor(sd);
	int ret, r;

	if (on == 0) {
		ret = power_down(sd);

		r = imx_vcm_power_down(sd);
		if (ret == 0)
			ret = r;
		dev->power = 0;
	} else {
		ret = imx_vcm_power_up(sd);
		if (ret)
			return ret;
		ret = power_up(sd);
		if (!ret) {
			dev->power = 1;
			return __imx_init(sd, 0);
		}
	}

	return ret;
}

static int imx_s_power(struct v4l2_subdev *sd, int on)
{
	int ret;
	struct imx_device *dev = to_imx_sensor(sd);

	mutex_lock(&dev->input_lock);
	ret = __imx_s_power(sd, on);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int imx_g_chip_ident(struct v4l2_subdev *sd,
				struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!chip)
		return -EINVAL;

	v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_IMX, 0);

	return 0;
}

static int imx_get_intg_factor(struct i2c_client *client,
				struct camera_mipi_info *info,
				const struct imx_reg *reglist)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx_device *dev = to_imx_sensor(sd);
	u32 vt_pix_clk_div;
	u32 vt_sys_clk_div;
	u32 pre_pll_clk_div;
	u32 pll_multiplier;
	u32 op_pix_clk_div;
	u32 op_sys_clk_div;

	const int ext_clk_freq_hz = 19200000;
	struct atomisp_sensor_mode_data buf;
	int vt_pix_clk_freq_mhz, ret = 0;
	u16 data[IMX_INTG_BUF_COUNT];

	u32 coarse_integration_time_min;
	u32 coarse_integration_time_max_margin;
	u32 frame_length_lines;
	u32 line_length_pck;
	u32 read_mode;
	u32 div;

	if (info == NULL) {
		ret = -EINVAL;
		goto out;
	}

	memset(data, 0, IMX_INTG_BUF_COUNT * sizeof(u16));
	ret = imx_read_reg(client, 1, IMX_VT_PIX_CLK_DIV, data, 0);
	if (ret)
		goto out;
	vt_pix_clk_div = data[0] & IMX_MASK_5BIT;
	ret = imx_read_reg(client, 1, IMX_VT_SYS_CLK_DIV, data, 0);
	if (ret)
		goto out;
	vt_sys_clk_div = data[0] & IMX_MASK_2BIT;
	ret = imx_read_reg(client, 1, IMX_PRE_PLL_CLK_DIV, data, 0);
	if (ret)
		goto out;
	pre_pll_clk_div = data[0] & IMX_MASK_4BIT;
	ret = imx_read_reg(client, 2, IMX_PLL_MULTIPLIER, data, 0);
	if (ret)
		goto out;
	pll_multiplier = data[0] & IMX_MASK_11BIT;
	ret = imx_read_reg(client, 1, IMX_OP_PIX_DIV, data, 0);
	if (ret)
		goto out;
	op_pix_clk_div = data[0] & IMX_MASK_5BIT;
	ret = imx_read_reg(client, 1, IMX_OP_SYS_DIV, data, 0);
	if (ret)
		goto out;
	op_sys_clk_div = data[0] & IMX_MASK_2BIT;

	memset(data, 0, IMX_INTG_BUF_COUNT * sizeof(u16));
	ret = imx_read_reg(client, 4, IMX_FRAME_LENGTH_LINES, data, 0);
	if (ret)
		return ret;
	frame_length_lines = data[0];
	line_length_pck = data[1];
	memset(data, 0, IMX_INTG_BUF_COUNT * sizeof(u16));
	ret = imx_read_reg(client, 2, IMX_COARSE_INTG_TIME_MIN, data, 0);
	if (ret)
		goto out;
	coarse_integration_time_min = data[0];
	ret = imx_read_reg(client, 2, IMX_COARSE_INTG_TIME_MAX, data, 0);
	if (ret)
		goto out;
	coarse_integration_time_max_margin = data[0];
	ret = imx_read_reg(client, 2, IMX_CROP_X_START, data, 0);
	if (ret)
		goto out;
	buf.crop_horizontal_start = data[0];
	ret = imx_read_reg(client, 2, IMX_CROP_X_END, data, 0);
	if (ret)
		goto out;
	buf.crop_horizontal_end = data[0];
	ret = imx_read_reg(client, 2, IMX_CROP_Y_START, data, 0);
	if (ret)
		goto out;
	buf.crop_vertical_start = data[0];
	ret = imx_read_reg(client, 2, IMX_CROP_Y_END, data, 0);
	if (ret)
		goto out;
	buf.crop_vertical_end = data[0];
	ret = imx_read_reg(client, 2, IMX_OUTPUT_WIDTH, data, 0);
	if (ret)
		goto out;
	buf.output_width = data[0];
	ret = imx_read_reg(client, 2, IMX_OUTPUT_HEIGHT, data, 0);
	if (ret)
		goto out;
	buf.output_height = data[0];

	memset(data, 0, IMX_INTG_BUF_COUNT * sizeof(u16));
	ret = imx_read_reg(client, 1, IMX_READ_MODE, data, 0);
	if (ret)
		goto out;
	read_mode = data[0] & IMX_MASK_2BIT;

	div = pre_pll_clk_div*vt_sys_clk_div*vt_pix_clk_div;
	if (div == 0) {
		ret = -EINVAL;
		goto out;
	}
	/*
	 * imx135 uses vt_pix_clk * 2 for calculations, i.e. integration
	 * time: coarse_int_time * line_length_pck/(2*vt_pix_clk).
	 * Provide correct value to user.
	 */
	vt_pix_clk_freq_mhz = ext_clk_freq_hz / div * pll_multiplier * 2;

	dev->vt_pix_clk_freq_mhz = vt_pix_clk_freq_mhz;
	buf.coarse_integration_time_min = coarse_integration_time_min;
	buf.coarse_integration_time_max_margin
		= coarse_integration_time_max_margin;
	buf.fine_integration_time_min = IMX_FINE_INTG_TIME;
	buf.fine_integration_time_max_margin = IMX_FINE_INTG_TIME;
	buf.fine_integration_time_def = IMX_FINE_INTG_TIME;
	buf.vt_pix_clk_freq_mhz = vt_pix_clk_freq_mhz;
	buf.line_length_pck = line_length_pck;
	buf.frame_length_lines = frame_length_lines;
	buf.read_mode = read_mode;

	ret = imx_read_reg(client, 1, IMX_BINNING_ENABLE, data, 0);
	if (ret)
		return ret;
	/* 1:binning enabled, 0:disabled */
	if (data[0] == 1) {
		ret = imx_read_reg(client, 1, IMX_BINNING_TYPE, data, 0);
		if (ret)
			return ret;
		buf.binning_factor_x = data[0] >> 4 & 0x0f;
		buf.binning_factor_y = data[0] & 0xf;
	} else {
		buf.binning_factor_x = 1;
		buf.binning_factor_y = 1;
	}

	memcpy(&info->data, &buf, sizeof(buf));

out:
	return ret;
}

/* This returns the exposure time being used. This should only be used
   for filling in EXIF data, not for actual image processing. */
static int imx_q_exposure(struct v4l2_subdev *sd, s32 *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u16 coarse;
	int ret;

	/* the fine integration time is currently not calculated */
	ret = imx_read_reg(client, IMX_16BIT,
			       IMX_COARSE_INTEGRATION_TIME, &coarse, 0);
	*value = coarse;

	return ret;
}

static int imx_test_pattern(struct v4l2_subdev *sd, s32 value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return imx_write_reg(client, IMX_16BIT,
			IMX_TEST_PATTERN_MODE, value);
}

static int imx_v_flip(struct v4l2_subdev *sd, s32 value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	u16 val;

	ret = imx_write_reg_array(client, imx_param_hold);
	if (ret)
		return ret;
	ret = imx_read_reg(client, IMX_8BIT, IMX_IMG_ORIENTATION, &val, 0);
	if (ret)
		return ret;
	if (value)
		val |= IMX_VFLIP_BIT;
	else
		val &= ~IMX_VFLIP_BIT;
	ret = imx_write_reg(client, IMX_8BIT,
			IMX_IMG_ORIENTATION, val);
	if (ret)
		return ret;
	return imx_write_reg_array(client, imx_param_update);
}

static int imx_g_focal(struct v4l2_subdev *sd, s32 *val)
{
	*val = (IMX_FOCAL_LENGTH_NUM << 16) | IMX_FOCAL_LENGTH_DEM;
	return 0;
}

static int imx_g_fnumber(struct v4l2_subdev *sd, s32 *val)
{
	/*const f number for imx*/
	*val = (IMX_F_NUMBER_DEFAULT_NUM << 16) | IMX_F_NUMBER_DEM;
	return 0;
}

static int imx_g_fnumber_range(struct v4l2_subdev *sd, s32 *val)
{
	*val = (IMX_F_NUMBER_DEFAULT_NUM << 24) |
		(IMX_F_NUMBER_DEM << 16) |
		(IMX_F_NUMBER_DEFAULT_NUM << 8) | IMX_F_NUMBER_DEM;
	return 0;
}

static int imx_g_bin_factor_x(struct v4l2_subdev *sd, s32 *val)
{
	struct imx_device *dev = to_imx_sensor(sd);

	*val = imx_res[dev->fmt_idx].bin_factor_x;

	return 0;
}

static int imx_g_bin_factor_y(struct v4l2_subdev *sd, s32 *val)
{
	struct imx_device *dev = to_imx_sensor(sd);

	*val = imx_res[dev->fmt_idx].bin_factor_y;

	return 0;
}

struct imx_control imx_controls[] = {
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
		.query = imx_q_exposure,
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
		.tweak = imx_test_pattern,
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
		.tweak = imx_v_flip,
	},
	{
		.qc = {
			.id = V4L2_CID_FOCUS_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focus move absolute",
			.minimum = 0,
			.maximum = IMX_MAX_FOCUS_POS,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.tweak = imx_t_focus_abs,
		.query = imx_q_focus_abs,
	},
	{
		.qc = {
			.id = V4L2_CID_FOCUS_RELATIVE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focus move relative",
			.minimum = IMX_MAX_FOCUS_NEG,
			.maximum = IMX_MAX_FOCUS_POS,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.tweak = imx_t_focus_rel,
	},
	{
		.qc = {
			.id = V4L2_CID_FOCUS_STATUS,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focus status",
			.minimum = 0,
			.maximum = 100, /* allow enum to grow in the future */
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.query = imx_q_focus_status,
	},
	{
		.qc = {
			.id = V4L2_CID_FOCAL_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focal length",
			.minimum = IMX_FOCAL_LENGTH_DEFAULT,
			.maximum = IMX_FOCAL_LENGTH_DEFAULT,
			.step = 0x01,
			.default_value = IMX_FOCAL_LENGTH_DEFAULT,
			.flags = 0,
		},
		.query = imx_g_focal,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number",
			.minimum = IMX_F_NUMBER_DEFAULT,
			.maximum = IMX_F_NUMBER_DEFAULT,
			.step = 0x01,
			.default_value = IMX_F_NUMBER_DEFAULT,
			.flags = 0,
		},
		.query = imx_g_fnumber,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_RANGE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number range",
			.minimum = IMX_F_NUMBER_RANGE,
			.maximum =  IMX_F_NUMBER_RANGE,
			.step = 0x01,
			.default_value = IMX_F_NUMBER_RANGE,
			.flags = 0,
		},
		.query = imx_g_fnumber_range,
	},
	{
		.qc = {
			.id = V4L2_CID_BIN_FACTOR_HORZ,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "horizontal binning factor",
			.minimum = 0,
			.maximum = IMX_BIN_FACTOR_MAX,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.query = imx_g_bin_factor_x,
	},
	{
		.qc = {
			.id = V4L2_CID_BIN_FACTOR_VERT,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "vertical binning factor",
			.minimum = 0,
			.maximum = IMX_BIN_FACTOR_MAX,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.query = imx_g_bin_factor_y,
	},
};
#define N_CONTROLS (ARRAY_SIZE(imx_controls))

static struct imx_control *imx_find_control(u32 id)
{
	int i;

	for (i = 0; i < N_CONTROLS; i++)
		if (imx_controls[i].qc.id == id)
			return &imx_controls[i];
	return NULL;
}

static int imx_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	struct imx_control *ctrl = imx_find_control(qc->id);
	struct imx_device *dev = to_imx_sensor(sd);

	if (ctrl == NULL)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	*qc = ctrl->qc;
	mutex_unlock(&dev->input_lock);

	return 0;
}

/* imx control set/get */
static int imx_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct imx_control *s_ctrl;
	struct imx_device *dev = to_imx_sensor(sd);
	int ret;

	if (!ctrl)
		return -EINVAL;

	s_ctrl = imx_find_control(ctrl->id);
	if ((s_ctrl == NULL) || (s_ctrl->query == NULL))
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	ret = s_ctrl->query(sd, &ctrl->value);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int imx_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct imx_control *octrl = imx_find_control(ctrl->id);
	struct imx_device *dev = to_imx_sensor(sd);
	int ret;

	if ((octrl == NULL) || (octrl->tweak == NULL))
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	ret = octrl->tweak(sd, ctrl->value);
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
#define LARGEST_ALLOWED_RATIO_MISMATCH  600
static int distance(struct imx_resolution *res, u32 w, u32 h)
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
	struct imx_resolution *tmp_res = NULL;

	for (i = 0; i < N_RES; i++) {
		tmp_res = &imx_res[i];
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
		if (w != imx_res[i].width)
			continue;
		if (h != imx_res[i].height)
			continue;
		/* Found it */
		return i;
	}
	return -1;
}

static int imx_try_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	int idx = 0;

	if ((fmt->width > IMX_RES_WIDTH_MAX)
		|| (fmt->height > IMX_RES_HEIGHT_MAX)) {
		fmt->width = IMX_RES_WIDTH_MAX;
		fmt->height = IMX_RES_HEIGHT_MAX;
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

		fmt->width = imx_res[idx].width;
		fmt->height = imx_res[idx].height;
	}
	fmt->code = V4L2_MBUS_FMT_SGBRG10_1X10;

	return 0;
}

static int imx_s_mbus_fmt(struct v4l2_subdev *sd,
			      struct v4l2_mbus_framefmt *fmt)
{
	struct imx_device *dev = to_imx_sensor(sd);
	const struct imx_reg *imx_def_reg;
	struct camera_mipi_info *imx_info = NULL;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	imx_info = v4l2_get_subdev_hostdata(sd);
	if (imx_info == NULL)
		return -EINVAL;
	ret = imx_try_mbus_fmt(sd, fmt);
	if (ret) {
		v4l2_err(sd, "try fmt fail\n");
		return ret;
	}

	mutex_lock(&dev->input_lock);
	dev->fmt_idx = get_resolution_index(fmt->width, fmt->height);

	/* Sanity check */
	if (unlikely(dev->fmt_idx == -1)) {
		v4l2_err(sd, "get resolution fail\n");
		ret = -EINVAL;
		goto out;
	}

	imx_def_reg = imx_res[dev->fmt_idx].regs;

	ret = imx_write_reg_array(client, imx_def_reg);
	if (ret) {
		ret = -EINVAL;
		goto out;
	}
	/* FIXME: workround for MERR Pre-alpha due to ISP perf - start */

	ret = imx_write_reg_array(client, imx_param_update);
	if (ret) {
		ret = -EINVAL;
		goto out;
	}

	dev->fps = imx_res[dev->fmt_idx].fps;
	dev->pixels_per_line = imx_res[dev->fmt_idx].pixels_per_line;
	dev->lines_per_frame = imx_res[dev->fmt_idx].lines_per_frame;

	ret = __imx_set_exposure(sd, dev->coarse_itg, dev->gain,
				 dev->digital_gain);
	if (ret)
		goto out;

	ret = imx_get_intg_factor(client, imx_info, imx_def_reg);
	if (ret) {
		v4l2_err(sd, "failed to get integration_factor\n");
		goto out;
	}
out:
	mutex_unlock(&dev->input_lock);
	return ret;
}


static int imx_g_mbus_fmt(struct v4l2_subdev *sd,
			      struct v4l2_mbus_framefmt *fmt)
{
	struct imx_device *dev = to_imx_sensor(sd);

	if (!fmt)
		return -EINVAL;

	fmt->width = imx_res[dev->fmt_idx].width;
	fmt->height = imx_res[dev->fmt_idx].height;
	fmt->code = V4L2_MBUS_FMT_SGBRG10_1X10;

	return 0;
}

static int imx_detect(struct i2c_client *client, u16 *id, u8 *revision)
{
	struct i2c_adapter *adapter = client->adapter;
	u16 high, low;

	/* i2c check */
	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -ENODEV;

	/* check sensor chip ID	 */
	if (imx_read_reg(client, IMX_8BIT, IMX_SC_CMMN_CHIP_ID_H, &high, 0)) {
		v4l2_err(client, "sensor_id_high = 0x%x\n", high);
		return -ENODEV;
	}

	if (imx_read_reg(client, IMX_8BIT, IMX_SC_CMMN_CHIP_ID_L, &low, 0)) {
		v4l2_err(client, "sensor_id_low = 0x%x\n", low);
		return -ENODEV;
	}

	*id = (((u8) high) << 8) | (u8) low;
	v4l2_info(client, "sensor_id = 0x%x\n", *id);

	if (*id != IMX_ID) {
		v4l2_err(client, "sensor ID error\n");
		return -ENODEV;
	}

	v4l2_info(client, "detect imx success\n");

	/* TODO - need to be updated */
	*revision = 0;

	return 0;
}

/*
 * imx stream on/off
 */
static int imx_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct imx_device *dev = to_imx_sensor(sd);

	mutex_lock(&dev->input_lock);
	if (enable) {
		ret = imx_write_reg_array(client, imx_streaming);
		if (ret != 0) {
			v4l2_err(client, "write_reg_array err\n");
			mutex_unlock(&dev->input_lock);
			return ret;
		}
		dev->streaming = 1;
	} else {
		ret = imx_write_reg_array(client, imx_soft_standby);
		if (ret != 0) {
			v4l2_err(client, "write_reg_array err\n");
			mutex_unlock(&dev->input_lock);
			return ret;
		}
		dev->streaming = 0;
	}
	mutex_unlock(&dev->input_lock);

	return 0;
}

/*
 * imx enum frame size, frame intervals
 */
static int imx_enum_framesizes(struct v4l2_subdev *sd,
				   struct v4l2_frmsizeenum *fsize)
{
	unsigned int index = fsize->index;

	if (index >= N_RES)
		return -EINVAL;

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = imx_res[index].width;
	fsize->discrete.height = imx_res[index].height;
	fsize->reserved[0] = imx_res[index].used;

	return 0;
}

static int imx_enum_frameintervals(struct v4l2_subdev *sd,
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
	fival->width = imx_res[i].width;
	fival->height = imx_res[i].height;
	fival->discrete.numerator = 1;
	fival->discrete.denominator = imx_res[i].fps;

	return 0;
}

static int imx_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned int index,
				 enum v4l2_mbus_pixelcode *code)
{
	if (index >= MAX_FMTS)
		return -EINVAL;

	*code = V4L2_MBUS_FMT_SGBRG10_1X10;
	return 0;
}

static int imx_s_config(struct v4l2_subdev *sd,
			    int irq, void *pdata)
{
	struct imx_device *dev = to_imx_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u8 sensor_revision;
	u16 sensor_id;
	int ret;
	if (pdata == NULL)
		return -ENODEV;

	dev->platform_data = pdata;

	mutex_lock(&dev->input_lock);

	if (dev->platform_data->platform_init) {
		ret = dev->platform_data->platform_init(client);
		if (ret) {
			mutex_unlock(&dev->input_lock);
			v4l2_err(client, "imx platform init err\n");
			return ret;
		}
	}
	ret = __imx_s_power(sd, 1);
	if (ret) {
		v4l2_err(client, "imx power-up err.\n");
		mutex_unlock(&dev->input_lock);
		return ret;
	}

	ret = dev->platform_data->csi_cfg(sd, 1);
	if (ret)
		goto fail_csi_cfg;

	/* config & detect sensor */
	ret = imx_detect(client, &sensor_id, &sensor_revision);
	if (ret) {
		v4l2_err(client, "imx_detect err s_config.\n");
		goto fail_detect;
	}

	dev->sensor_id = sensor_id;
	dev->sensor_revision = sensor_revision;
	/* Read sensor's OTP data */
	dev->otp_data = imx_otp_read(sd);
	/* power off sensor */
	ret = __imx_s_power(sd, 0);
	mutex_unlock(&dev->input_lock);
	if (ret)
		v4l2_err(client, "imx power-down err.\n");

	return ret;

fail_detect:
	dev->platform_data->csi_cfg(sd, 0);
fail_csi_cfg:
	__imx_s_power(sd, 0);
	mutex_unlock(&dev->input_lock);
	dev_err(&client->dev, "sensor power-gating failed\n");
	return ret;
}

static int
imx_enum_mbus_code(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index >= MAX_FMTS)
		return -EINVAL;
	code->code = V4L2_MBUS_FMT_SGBRG10_1X10;

	return 0;
}

static int
imx_enum_frame_size(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			struct v4l2_subdev_frame_size_enum *fse)
{
	int index = fse->index;

	if (index >= N_RES)
		return -EINVAL;

	fse->min_width = imx_res[index].width;
	fse->min_height = imx_res[index].height;
	fse->max_width = imx_res[index].width;
	fse->max_height = imx_res[index].height;

	return 0;
}

static struct v4l2_mbus_framefmt *
__imx_get_pad_format(struct imx_device *sensor,
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
imx_get_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct imx_device *dev = to_imx_sensor(sd);
	struct v4l2_mbus_framefmt *format =
			__imx_get_pad_format(dev, fh, fmt->pad, fmt->which);

	fmt->format = *format;

	return 0;
}

static int
imx_set_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct imx_device *dev = to_imx_sensor(sd);

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		dev->format = fmt->format;

	return 0;
}

static int
imx_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct imx_device *dev = to_imx_sensor(sd);
	dev->run_mode = param->parm.capture.capturemode;
	mutex_lock(&dev->input_lock);
	switch (dev->run_mode) {
	case CI_MODE_VIDEO:
		imx_res = imx_res_video;
		N_RES = N_RES_VIDEO;
		break;
	case CI_MODE_STILL_CAPTURE:
		imx_res = imx_res_still;
		N_RES = N_RES_STILL;
		break;
	default:
		imx_res = imx_res_preview;
		N_RES = N_RES_PREVIEW;
	}
	mutex_unlock(&dev->input_lock);
	return 0;
}

int
imx_g_frame_interval(struct v4l2_subdev *sd,
				struct v4l2_subdev_frame_interval *interval)
{
	struct imx_device *dev = to_imx_sensor(sd);
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
			 * we can not add 4 according to ds, as this will
			 * cause over flow
			 */
			v4l2_warn(client, "%s: abnormal coarse_itg:0x%x\n",
				  __func__, dev->coarse_itg);
			lines_per_frame = dev->coarse_itg;
		} else {
			lines_per_frame = dev->coarse_itg + 4;
		}
	} else {
		lines_per_frame = dev->lines_per_frame;
	}
	interval->interval.numerator = dev->pixels_per_line *
					lines_per_frame;
	interval->interval.denominator = dev->vt_pix_clk_freq_mhz;

	return 0;
}

static const struct v4l2_subdev_video_ops imx_video_ops = {
	.s_stream = imx_s_stream,
	.enum_framesizes = imx_enum_framesizes,
	.enum_frameintervals = imx_enum_frameintervals,
	.enum_mbus_fmt = imx_enum_mbus_fmt,
	.try_mbus_fmt = imx_try_mbus_fmt,
	.g_mbus_fmt = imx_g_mbus_fmt,
	.s_mbus_fmt = imx_s_mbus_fmt,
	.s_parm = imx_s_parm,
	.g_frame_interval = imx_g_frame_interval,
};

static const struct v4l2_subdev_core_ops imx_core_ops = {
	.g_chip_ident = imx_g_chip_ident,
	.queryctrl = imx_queryctrl,
	.g_ctrl = imx_g_ctrl,
	.s_ctrl = imx_s_ctrl,
	.s_power = imx_s_power,
	.ioctl = imx_ioctl,
	.init = imx_init,
};

static const struct v4l2_subdev_pad_ops imx_pad_ops = {
	.enum_mbus_code = imx_enum_mbus_code,
	.enum_frame_size = imx_enum_frame_size,
	.get_fmt = imx_get_pad_format,
	.set_fmt = imx_set_pad_format,
};

static const struct v4l2_subdev_ops imx_ops = {
	.core = &imx_core_ops,
	.video = &imx_video_ops,
	.pad = &imx_pad_ops,
};

static const struct media_entity_operations imx_entity_ops = {
	.link_setup = NULL,
};

static int imx_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx_device *dev = to_imx_sensor(sd);

	if (dev->platform_data->platform_deinit)
		dev->platform_data->platform_deinit();

	media_entity_cleanup(&dev->sd.entity);
	dev->platform_data->csi_cfg(sd, 0);
	v4l2_device_unregister_subdev(sd);
	kfree(dev);

	return 0;
}

static int imx_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct imx_device *dev;
	int ret;

	/* allocate sensor device & init sub device */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		v4l2_err(client, "%s: out of memory\n", __func__);
		return -ENOMEM;
	}

	mutex_init(&dev->input_lock);

	dev->fmt_idx = 0;
	v4l2_i2c_subdev_init(&(dev->sd), client, &imx_ops);

	ret = imx_vcm_init(&dev->sd);
	if (ret < 0)
		goto out_free;

	if (client->dev.platform_data) {
		ret = imx_s_config(&dev->sd, client->irq,
				       client->dev.platform_data);
		if (ret)
			goto out_free;
	}

	dev->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->pad.flags = MEDIA_PAD_FL_SOURCE;
	dev->format.code = V4L2_MBUS_FMT_SGBRG10_1X10;
	dev->sd.entity.ops = &imx_entity_ops;
	dev->sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;

	ret = media_entity_init(&dev->sd.entity, 1, &dev->pad, 0);
	if (ret)
		imx_remove(client);

	return ret;
out_free:
	v4l2_device_unregister_subdev(&dev->sd);
	kfree(dev);
	return ret;
}

static const struct i2c_device_id imx_id[] = {
	{IMX_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, imx_id);

static struct i2c_driver imx_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = IMX_NAME,
	},
	.probe = imx_probe,
	.remove = imx_remove,
	.id_table = imx_id,
};


static __init int init_imx(void)
{
	return i2c_add_driver(&imx_driver);
}

static __exit void exit_imx(void)
{
	i2c_del_driver(&imx_driver);
}

module_init(init_imx);
module_exit(exit_imx);

MODULE_DESCRIPTION("A low-level driver for Sony IMX sensors");
MODULE_AUTHOR("Shenbo Huang <shenbo.huang@intel.com>");
MODULE_LICENSE("GPL");

