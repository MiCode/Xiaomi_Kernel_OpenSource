/*
 * Support for S5K4H8YX 8M camera sensor.
 *
 * Copyright (c) 2015 Intel Corporation. All Rights Reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
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
#ifndef CONFIG_GMIN_INTEL_MID /* FIXME! for non-gmin*/
#include <media/v4l2-chip-ident.h>
#else
#include <linux/atomisp_gmin_platform.h>
#endif
#include <media/v4l2-device.h>
#include <asm/intel-mid.h>
#include <linux/firmware.h>
#include <linux/acpi.h>
#include <linux/proc_fs.h>

#include "s5k4h8yx.h"
#include "dw9761.h"

/* the bayer order mapping table
 *          hflip=0                  hflip=1
 * vflip=0  atomisp_bayer_order_grbg atomisp_bayer_order_rggb
 * vflip=1  atomisp_bayer_order_bggr atomisp_bayer_order_gbrg
 *
 * usage: s5k4h8yx_bayer_order_mapping[vflip][hflip]
 */

/* S5K4H8YX default GRBG */
static const int s5k4h8yx_bayer_order_mapping[2][2] = {
	{ atomisp_bayer_order_grbg, atomisp_bayer_order_rggb },
	{ atomisp_bayer_order_bggr, atomisp_bayer_order_gbrg }
};

static int op_dump_otp;
struct s5k4h8yx_device *global_dev;

static struct s5k4h8yx_write_ctrl ctrl;

static int s5k4h8yx_dump_otp(const char *val, struct kernel_param *kp);
module_param_call(dumpotp, s5k4h8yx_dump_otp, param_get_uint,
				&op_dump_otp, S_IRUGO | S_IWUSR);


static int s5k4h8yx_dump_otp(const char *val, struct kernel_param *kp)
{
	int ret;
	if (NULL != global_dev->otp_raw_data) {
		ret = dw9761_otp_save(global_dev->otp_raw_data,
		DW9761_OTP_RAW_SIZE,
		DW9761_SAVE_RAW_OTP);

		if (ret != 0)
			printk(KERN_ERR "Fail to save s5k4h8 RAW OTP data\n");
	}
	if (NULL != global_dev->otp_data) {
		ret = dw9761_otp_save(global_dev->otp_data,
			DEFAULT_DW9761_OTP_SIZE,
			DW9761_SAVE_PARSED_OTP);

		if (ret != 0)
			printk(KERN_ERR "Fail to save s5k4h8 PARSED OTP data\n");
	}
	return 0;
}


static int s5k4h8yx_read_reg(struct i2c_client *client, u16 len,
						u16 reg, u16 *val)
{
	struct i2c_msg msg[2];
	u16 data[S5K4H8YX_SHORT_MAX] = {0};
	int err, i;
	int retry_cnt = 5;

	if (len > S5K4H8YX_BYTE_MAX) {
		dev_err(&client->dev,
			"%s error, invalid data length\n", __func__);
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

	while (retry_cnt-- > 0) {
		err = i2c_transfer(client->adapter, msg, 2);
		if (err != 2) {
			if (err >= 0)
				err = -EIO;

			if (retry_cnt <= 0)
				goto error;
		} else
			break;
	}

	/* high byte comes first */
	if (len == S5K4H8YX_8BIT)
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

static int s5k4h8yx_i2c_write(struct i2c_client *client, u16 len, u8 *data)
{
	struct i2c_msg msg;
	const int num_msg = 1;
	int ret;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = len;
	msg.buf = data;
	ret = i2c_transfer(client->adapter, &msg, 1);

	if (ret != num_msg)
		dev_err(&client->dev, "%s error!! ret = %d\n", __func__, ret);

	return ret == num_msg ? 0 : -EIO;
}

static int s5k4h8yx_write_reg(struct i2c_client *client, u16 data_length,
							u16 reg, u16 val)
{
	int ret;
	unsigned char data[4] = {0};
	u16 *wreg = (u16 *)data;
	const u16 len = data_length + sizeof(u16); /* 16-bit address + data */
	int retry_cnt = 5;

	if (data_length != S5K4H8YX_8BIT && data_length != S5K4H8YX_16BIT) {
		dev_err(&client->dev,
			"%s error, invalid data_length\n", __func__);
		return -EINVAL;
	}

	/* high byte goes out first */
	*wreg = cpu_to_be16(reg);

	if (data_length == S5K4H8YX_8BIT) {
		data[2] = (u8)(val);
	} else {
		/* S5K4H8YX_16BIT */
		u16 *wdata = (u16 *)&data[2];
		*wdata = cpu_to_be16(val);
	}

	while (retry_cnt-- > 0) {
		ret = s5k4h8yx_i2c_write(client, len, data);
		if (ret) {

		} else
			break;
	}

	return ret;
}

/*
 * s5k4h8yx_write_reg_array - Initializes a list of S5K4H8YX registers
 * @client: i2c driver client structure
 * @reglist: list of registers to be written
 *
 * This function initializes a list of registers. When consecutive addresses
 * are found in a row on the list, this function creates a buffer and sends
 * consecutive data in a single i2c_transfer().
 *
 * __s5k4h8yx_flush_reg_array, __s5k4h8yx_buf_reg_array() and
 * __s5k4h8yx_write_reg_is_consecutive() are internal functions to
 * s5k4h8yx_write_reg_array_fast() and should be not used anywhere else.
 *
 */

static int __s5k4h8yx_flush_reg_array(struct i2c_client *client,
				     struct s5k4h8yx_write_ctrl *ctrl)
{
	u16 size;

	if (ctrl->index == 0)
		return 0;

	size = sizeof(u16) + ctrl->index; /* 16-bit address + data */
	ctrl->buffer.addr = cpu_to_be16(ctrl->buffer.addr);
	ctrl->index = 0;

	return s5k4h8yx_i2c_write(client, size, (u8 *)&ctrl->buffer);
}

static int __s5k4h8yx_buf_reg_array(struct i2c_client *client,
				   struct s5k4h8yx_write_ctrl *ctrl,
				   const struct s5k4h8yx_reg *next)
{
	int size;
	u16 *data16;

	switch (next->type) {
	case S5K4H8YX_8BIT:
		size = 1;
		ctrl->buffer.data[ctrl->index] = (u8)next->val;
		break;
	case S5K4H8YX_16BIT:
		size = 2;
		data16 = (u16 *)&ctrl->buffer.data[ctrl->index];
		*data16 = cpu_to_be16((u16)next->val);
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
	if (ctrl->index + sizeof(u16) >= S5K4H8YX_MAX_WRITE_BUF_SIZE)
		return __s5k4h8yx_flush_reg_array(client, ctrl);

	return 0;
}

static int __s5k4h8yx_write_reg_is_consecutive(struct i2c_client *client,
				   struct s5k4h8yx_write_ctrl *ctrl,
				   const struct s5k4h8yx_reg *next)
{
	if (ctrl->index == 0)
		return 1;
	else {
		if ((ctrl->index == 2) &&
			(next->sreg == 0x6F12) &&
			(next--->sreg != 0x6F12))
			return 0;
		else if (next->sreg == 0x6F12)
			return 1;
		else
			return 0;
	}

}

static int s5k4h8yx_write_reg_array(struct i2c_client *client,
				   const struct s5k4h8yx_reg *reglist)
{
	const struct s5k4h8yx_reg *next = reglist;
	int err;

	ctrl.index = 0;
	for (; next->type != S5K4H8YX_TOK_TERM; next++) {
		switch (next->type & S5K4H8YX_TOK_MASK) {
		case S5K4H8YX_TOK_DELAY:
			err = __s5k4h8yx_flush_reg_array(client, &ctrl);
			if (err) {
				dev_err(&client->dev,
					"%s: write error\n", __func__);
				return err;
			}
			msleep(next->val);
			break;
		default:
			/*
			 * If next address is not consecutive, data needs to be
			 * flushed before proceed.
			 */
			if (!__s5k4h8yx_write_reg_is_consecutive(client, &ctrl,
								next)) {
				err = __s5k4h8yx_flush_reg_array(client, &ctrl);
				if (err) {
					dev_err(&client->dev,
						"%s: write error\n", __func__);
					return err;
				}
			}
			err = __s5k4h8yx_buf_reg_array(client, &ctrl, next);
			if (err) {
				dev_err(&client->dev,
					"%s: write error\n", __func__);
				return err;
			}
			break;
		}
	}

	return __s5k4h8yx_flush_reg_array(client, &ctrl);
}

static int __s5k4h8yx_init(struct v4l2_subdev *sd, u32 val)
{

	/* restore settings */
	s5k4h8yx_res = s5k4h8yx_res_preview;
	N_RES = N_RES_PREVIEW;

	return 0;
}

static int s5k4h8yx_init(struct v4l2_subdev *sd, u32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	ret = __s5k4h8yx_init(sd, val);
	dev_info(&client->dev, "s5k4h8yx_init_config\n");
	/*enable indirect burst*/
	s5k4h8yx_write_reg(client, S5K4H8YX_16BIT, 0x6028, 0x4000);
	s5k4h8yx_write_reg(client, S5K4H8YX_16BIT, 0x6004, 0x0001);

	ret = s5k4h8yx_write_reg_array(client, s5k4h8yx_init_config);

	/*disable indirect burst*/
	s5k4h8yx_write_reg(client, S5K4H8YX_16BIT, 0x6028, 0x4000);
	s5k4h8yx_write_reg(client, S5K4H8YX_16BIT, 0x6004, 0x0000);

	return ret;
}

static void s5k4h8yx_uninit(struct v4l2_subdev *sd)
{
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);

	dev->coarse_itg = 0;
	dev->fine_itg   = 0;
	dev->gain       = 0;
}

static int __s5k4h8yx_power_ctrl(struct v4l2_subdev *sd, bool flag)
{
	int ret = -1;
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);

	if (!dev || !dev->platform_data)
		return -ENODEV;

	if (flag) {
		ret = dev->platform_data->v1p8_ctrl(sd, 1);
		usleep_range(60, 90);
		if (ret == 0)
			ret |= dev->platform_data->v2p8_ctrl(sd, 1);
	} else {
		ret = dev->platform_data->v1p8_ctrl(sd, 0);
		ret |= dev->platform_data->v2p8_ctrl(sd, 0);
	}

	return ret;
}


static int power_up(struct v4l2_subdev *sd)
{
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	dev_info(&client->dev, "@power_up\n");
	if (NULL == dev->platform_data) {
		dev_err(&client->dev, "no camera_sensor_platform_data\n");
		return -ENODEV;
	}

	/* power control */
	ret = __s5k4h8yx_power_ctrl(sd, 1);
	if (ret)
		goto fail_power;

	/* Check delay time from DS */
	usleep_range(5000, 6000);

	/* gpio ctrl */
	ret = dev->platform_data->gpio1_ctrl(sd, 1);
	dev_info(&client->dev, "gpio1_ctrl set %d ret=%d\n", 1, ret);
	if (ret) {
		dev_err(&client->dev, "gpio1_ctrl failed\n");
		goto fail_gpio1;
	}
	usleep_range(10000, 15000);
	ret = dev->platform_data->gpio0_ctrl(sd, 1);
	dev_info(&client->dev, "gpio0_ctrl set %d ret=%d\n", 1, ret);
	if (ret) {
		dev_err(&client->dev, "gpio0_ctrl failed\n");
		goto fail_gpio0;
	}
	usleep_range(10000, 15000);

	/* flis clock control */
	ret = dev->platform_data->flisclk_ctrl(sd, 1);
	if (ret)
		goto fail_clk;

	return 0;

fail_clk:
	dev->platform_data->flisclk_ctrl(sd, 0);
fail_gpio0:
	dev->platform_data->gpio0_ctrl(sd, 0);
fail_gpio1:
	dev->platform_data->gpio1_ctrl(sd, 0);
fail_power:
	__s5k4h8yx_power_ctrl(sd, 0);
	dev_err(&client->dev, "sensor power-up failed\n");

	return ret;
}

static int power_down(struct v4l2_subdev *sd)
{
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;
	dev_info(&client->dev, "@power_down\n");
	if (NULL == dev->platform_data) {
		dev_err(&client->dev, "no camera_sensor_platform_data\n");
		return -ENODEV;
	}
	/*TODO check power down sequence*/
	ret = dev->platform_data->flisclk_ctrl(sd, 0);
	if (ret) {
		dev_err(&client->dev, "flisclk failed\n");
		goto fail_clk;
	}

	/* gpio ctrl */
	ret = dev->platform_data->gpio1_ctrl(sd, 0);
	dev_info(&client->dev, "gpio1_ctrl set %d ret=%d\n", 0, ret);
	if (ret) {
		dev_err(&client->dev, "gpio1_ctrl failed\n");
		goto fail_gpio;
	}
	usleep_range(10000, 15000);
	ret = dev->platform_data->gpio0_ctrl(sd, 0);
	if (ret) {
		dev_err(&client->dev, "gpio0_ctrl failed\n");
		goto fail_gpio;
	}
	dev_info(&client->dev, "gpio0_ctrl set %d ret=%d\n", 0, ret);
	usleep_range(10000, 15000);

	/* power control */
	ret = __s5k4h8yx_power_ctrl(sd, 0);
	if (ret) {
		dev_err(&client->dev, "power down failed.\n");
		goto fail_power;
	}

	return 0;

fail_clk:
	dev->platform_data->flisclk_ctrl(sd, 0);
fail_gpio:
	dev->platform_data->gpio0_ctrl(sd, 0);
	dev->platform_data->gpio1_ctrl(sd, 0);
fail_power:
	dev->platform_data->power_ctrl(sd, 0);
	dev_err(&client->dev, "sensor power-up failed\n");

	return ret;
}

static int __s5k4h8yx_s_power(struct v4l2_subdev *sd, int power)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);
	int ret = 0;

	if (power == 0) {
		if (dev->power == 0)
			return 0;
		s5k4h8yx_uninit(sd);
		if (dev->vcm_driver) {
			ret = dev->vcm_driver->power_down(sd);
			if (ret)
				v4l2_err(sd, "vcm power down fail");
		}
		ret = power_down(sd);
		if (ret)
			v4l2_err(sd, "sensor power down fail\n");
		dev->power = 0;
		dev_info(&client->dev, "__s5k4h8yx_s_power off %d\n", __LINE__);
		return ret;
	} else {
		if (dev->power == 1)
			return 0;
		ret = power_up(sd);
		if (ret) {
			v4l2_err(sd, "cam sensor power up fail\n");
			return ret;
		}
		if (dev->vcm_driver) {
			ret = dev->vcm_driver->power_up(sd);
			if (ret)
				v4l2_err(sd, "vcm power up fail");
		}
		dev->power = 1;
		dev_info(&client->dev, "__s5k4h8yx_s_power on %d\n", __LINE__);
		return s5k4h8yx_init(sd, 0);
	}
}

static int s5k4h8yx_s_power(struct v4l2_subdev *sd, int on)
{
	int ret;
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);

	mutex_lock(&dev->input_lock);
	ret = __s5k4h8yx_s_power(sd, on);
	mutex_unlock(&dev->input_lock);

	return ret;
}

/* This returns the exposure time being used. This should only be used
   for filling in EXIF data, not for actual image processing. */
static int s5k4h8yx_q_exposure(struct v4l2_subdev *sd, s32 *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u16 coarse;
	int ret;
	/* the fine integration time is currently not calculated */
	ret = s5k4h8yx_read_reg(client, S5K4H8YX_16BIT,
			       S5K4H8YX_COARSE_INTEGRATION_TIME, &coarse);
	*value = coarse;

	return ret;
}

static int s5k4h8yx_get_intg_factor(struct i2c_client *client,
				struct camera_mipi_info *info)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);
	u32 vt_pix_clk_div;
	u32 vt_sys_clk_div;
	u32 pre_pll_clk_div;
	u32 pll_multiplier;

	const int ext_clk_freq_hz = 19200000;
	struct atomisp_sensor_mode_data *buf = &info->data;
	int ret;
	u16 data[S5K4H8YX_INTG_BUF_COUNT];

	u32 vt_pix_clk_freq_mhz;
	u32 fine_integration_time;
	u32 frame_length_lines;
	u32 line_length_pck;
	u32 div;

	if (info == NULL)
		return -EINVAL;
	dev_info(&client->dev, "enter s5k4h8yx_get_intg_factor\n");

	memset(data, 0, S5K4H8YX_INTG_BUF_COUNT * sizeof(u16));
	ret = s5k4h8yx_read_reg(client, 2, 0x6028, data);
	if (ret)
		return ret;
	dev_info(&client->dev, "page register 0x%x\n", data[0]);

	memset(data, 0, S5K4H8YX_INTG_BUF_COUNT * sizeof(u16));
	ret = s5k4h8yx_read_reg(client, 2, S5K4H8YX_VT_PIX_CLK_DIV, data);
	if (ret)
		return ret;
	vt_pix_clk_div = data[0];
	dev_info(&client->dev, "vt_pix_clk_div = 0x%x\n", vt_pix_clk_div);

	memset(data, 0, S5K4H8YX_INTG_BUF_COUNT * sizeof(u16));
	ret = s5k4h8yx_read_reg(client, 2, S5K4H8YX_VT_SYS_CLK_DIV, data);
	if (ret)
		return ret;
	vt_sys_clk_div = data[0];
	dev_info(&client->dev, "vt_sys_clk_div = 0x%x\n", vt_sys_clk_div);

	memset(data, 0, S5K4H8YX_INTG_BUF_COUNT * sizeof(u16));
	ret = s5k4h8yx_read_reg(client, 2, S5K4H8YX_PRE_PLL_CLK_DIV, data);
	if (ret)
		return ret;
	pre_pll_clk_div = data[0];
	dev_info(&client->dev, "pre_pll_clk_div = 0x%x\n", pre_pll_clk_div);

	memset(data, 0, S5K4H8YX_INTG_BUF_COUNT * sizeof(u16));
	ret = s5k4h8yx_read_reg(client, 2, S5K4H8YX_PLL_MULTIPLIER, data);
	if (ret)
		return ret;
	pll_multiplier = data[0];
	dev_info(&client->dev, "pll_multiplier = 0x%x\n", pll_multiplier);

	memset(data, 0, S5K4H8YX_INTG_BUF_COUNT * sizeof(u16));
	ret = s5k4h8yx_read_reg(client, 2,
				S5K4H8YX_FINE_INTEGRATION_TIME, data);
	if (ret)
		return ret;
	fine_integration_time = data[0];
	dev_info(&client->dev, "fine_integration_time = 0x%x\n",
		fine_integration_time);

	memset(data, 0, S5K4H8YX_INTG_BUF_COUNT * sizeof(u16));
	ret = s5k4h8yx_read_reg(client, 4, S5K4H8YX_FRAME_LENGTH_LINES, data);
	if (ret)
		return ret;
	frame_length_lines = data[0];
	line_length_pck = data[1];
	dev_info(&client->dev, "frame_length_lines = 0x%x\n",
		frame_length_lines);
	dev_info(&client->dev, "line_length_pck = 0x%x\n", line_length_pck);

	div = pre_pll_clk_div * vt_sys_clk_div*vt_pix_clk_div;
	if (div == 0)
		return -EINVAL;

	vt_pix_clk_freq_mhz = ext_clk_freq_hz / div;
	vt_pix_clk_freq_mhz *= pll_multiplier;
	vt_pix_clk_freq_mhz *= 2;

	dev_info(&client->dev, "vt_pix_clk_freq_mhz(inhz) = %d\n",
		vt_pix_clk_freq_mhz);

	dev->vt_pix_clk_freq_mhz = vt_pix_clk_freq_mhz;
	buf->vt_pix_clk_freq_mhz = vt_pix_clk_freq_mhz;
	buf->coarse_integration_time_min =
			S5K4H8YX_COARSE_INTEGRATION_TIME_MIN;
	buf->coarse_integration_time_max_margin =
			S5K4H8YX_COARSE_INTEGRATION_TIME_MARGIN;
	buf->fine_integration_time_min = fine_integration_time;
	buf->fine_integration_time_max_margin =
				fine_integration_time;
	buf->fine_integration_time_def = fine_integration_time;
	buf->line_length_pck = line_length_pck;
	buf->frame_length_lines = frame_length_lines;
	buf->read_mode = 0;

	buf->binning_factor_x = 1;
	buf->binning_factor_y = 1;

	/* Get the cropping and output resolution to ISP for this mode. */
	ret =  s5k4h8yx_read_reg(client, 2, S5K4H8YX_HORIZONTAL_START_H, data);
	if (ret)
		return ret;
	buf->crop_horizontal_start = data[0];

	ret = s5k4h8yx_read_reg(client, 2, S5K4H8YX_VERTICAL_START_H, data);
	if (ret)
		return ret;
	buf->crop_vertical_start = data[0];

	ret = s5k4h8yx_read_reg(client, 2, S5K4H8YX_HORIZONTAL_END_H, data);
	if (ret)
		return ret;
	buf->crop_horizontal_end = data[0];

	ret = s5k4h8yx_read_reg(client, 2, S5K4H8YX_VERTICAL_END_H, data);
	if (ret)
		return ret;
	buf->crop_vertical_end = data[0];

	ret = s5k4h8yx_read_reg(client, 2,
				S5K4H8YX_HORIZONTAL_OUTPUT_SIZE_H, data);
	if (ret)
		return ret;
	buf->output_width = data[0];

	ret = s5k4h8yx_read_reg(client, 2,
				S5K4H8YX_VERTICAL_OUTPUT_SIZE_H, data);
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
#define LARGEST_ALLOWED_RATIO_MISMATCH 600
static int distance(struct s5k4h8yx_resolution *res, u32 w, u32 h)
{
	unsigned int w_ratio = ((res->width << 13) / w);
	unsigned int h_ratio;
	int match;

	if (h == 0)
		return -EPERM;
	h_ratio = ((res->height << 13) / h);
	if (h_ratio == 0)
		return -EPERM;
	match   = abs(((w_ratio << 13) / h_ratio) - ((int)8192));

	if ((w_ratio < (int)8192) || (h_ratio < (int)8192)  ||
		(match > LARGEST_ALLOWED_RATIO_MISMATCH))
		return -EPERM;

	return w_ratio + h_ratio;
}

/* Return the nearest higher resolution index */
static int nearest_resolution_index(int w, int h)
{
	int i;
	int idx = -1;
	int dist;
	int min_dist = INT_MAX;
	struct s5k4h8yx_resolution *tmp_res = NULL;

	for (i = 0; i < N_RES; i++) {
		tmp_res = &s5k4h8yx_res[i];
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

static int s5k4h8yx_try_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);
	int idx;

	mutex_lock(&dev->input_lock);

	if ((fmt->width > S5K4H8YX_RES_WIDTH_MAX) ||
		(fmt->height > S5K4H8YX_RES_HEIGHT_MAX)) {
		fmt->width = S5K4H8YX_RES_WIDTH_MAX;
		fmt->height = S5K4H8YX_RES_HEIGHT_MAX;
		fmt->code = V4L2_MBUS_FMT_SGRBG10_1X10;
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

		fmt->width = s5k4h8yx_res[idx].width;
		fmt->height = s5k4h8yx_res[idx].height;
		fmt->code = s5k4h8yx_res[idx].code;
	}

	mutex_unlock(&dev->input_lock);
	return 0;
}

static int s5k4h8yx_set_mbus_fmt(struct v4l2_subdev *sd,
			      struct v4l2_mbus_framefmt *fmt)
{
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);
	const struct s5k4h8yx_reg *s5k4h8yx_def_reg;
	struct camera_mipi_info *s5k4h8yx_info = NULL;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	u8 tmp;
	int vflip, hflip;

	dev_info(&client->dev, "enter s5k4h8yx_set_mbus_fmt\n");

	s5k4h8yx_info = v4l2_get_subdev_hostdata(sd);
	if (s5k4h8yx_info == NULL)
		return -EINVAL;

	ret = s5k4h8yx_try_mbus_fmt(sd, fmt);
	if (ret) {
		v4l2_err(sd, "try fmt fail\n");
		return ret;
	}

	mutex_lock(&dev->input_lock);
	dev->fmt_idx = nearest_resolution_index(fmt->width, fmt->height);
	dev_info(&client->dev, "fmt_idx %d, width %d , height %d\n",
		dev->fmt_idx, fmt->width, fmt->height);
	/* Sanity check */
	if (unlikely(dev->fmt_idx == -1)) {
		mutex_unlock(&dev->input_lock);
		v4l2_err(sd, "get resolution fail\n");
		return -EINVAL;
	}

	s5k4h8yx_def_reg = s5k4h8yx_res[dev->fmt_idx].regs;

	s5k4h8yx_info->input_format = ATOMISP_INPUT_FORMAT_RAW_10;
	/* enable group hold */
	ret = s5k4h8yx_write_reg_array(client, s5k4h8yx_param_hold);
	if (ret) {
		mutex_unlock(&dev->input_lock);
		return ret;
	}

	ret = s5k4h8yx_write_reg_array(client, s5k4h8yx_def_reg);
	if (ret) {
		mutex_unlock(&dev->input_lock);
		return ret;
	}

	/* disable group hold */
	ret = s5k4h8yx_write_reg_array(client, s5k4h8yx_param_update);
	if (ret) {
		mutex_unlock(&dev->input_lock);
		return ret;
	}
	dev->fps = s5k4h8yx_res[dev->fmt_idx].fps;
	dev->pixels_per_line = s5k4h8yx_res[dev->fmt_idx].pixels_per_line;
	dev->lines_per_frame = s5k4h8yx_res[dev->fmt_idx].lines_per_frame;
	dev->coarse_itg = 0;
	dev->fine_itg = 0;
	dev->gain = 0;

	ret = s5k4h8yx_get_intg_factor(client, s5k4h8yx_info);
	if (ret) {
		v4l2_err(sd, "failed to get integration_factor\n");
		mutex_unlock(&dev->input_lock);
		return -EINVAL;
	}

	ret = s5k4h8yx_read_reg(client, S5K4H8YX_8BIT,
				S5K4H8YX_IMG_ORIENTATION, (u16 *)&tmp);
	if (ret) {
		mutex_unlock(&dev->input_lock);
		return ret;
	}
	hflip = tmp & S5K4H8YX_HFLIP_BIT;
	vflip = (tmp & S5K4H8YX_VFLIP_BIT) >> S5K4H8YX_VFLIP_OFFSET;
	s5k4h8yx_info->raw_bayer_order =
				s5k4h8yx_bayer_order_mapping[vflip][hflip];
	dev_info(&client->dev, "bayer order %d\n",
		s5k4h8yx_info->raw_bayer_order);

	mutex_unlock(&dev->input_lock);
	return 0;
}

static int s5k4h8yx_g_mbus_fmt(struct v4l2_subdev *sd,
			      struct v4l2_mbus_framefmt *fmt)
{
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);

	if (!fmt)
		return -EINVAL;

	fmt->width = s5k4h8yx_res[dev->fmt_idx].width;
	fmt->height = s5k4h8yx_res[dev->fmt_idx].height;
	fmt->code = s5k4h8yx_res[dev->fmt_idx].code;

	return 0;
}

static int s5k4h8yx_g_focal(struct v4l2_subdev *sd, s32 *val)
{
	*val = (S5K4H8YX_FOCAL_LENGTH_NUM << 16) | S5K4H8YX_FOCAL_LENGTH_DEM;
	return 0;
}

static int s5k4h8yx_g_fnumber(struct v4l2_subdev *sd, s32 *val)
{
	/*const f number for s5k4h8yx*/
	*val = (S5K4H8YX_F_NUMBER_DEFAULT_NUM << 16) | S5K4H8YX_F_NUMBER_DEM;
	return 0;
}

static int s5k4h8yx_g_fnumber_range(struct v4l2_subdev *sd, s32 *val)
{
	*val = (S5K4H8YX_F_NUMBER_DEFAULT_NUM << 24) |
		(S5K4H8YX_F_NUMBER_DEM << 16) |
		(S5K4H8YX_F_NUMBER_DEFAULT_NUM << 8) | S5K4H8YX_F_NUMBER_DEM;
	return 0;
}

/* Horizontal flip the image. */
static int s5k4h8yx_t_hflip(struct v4l2_subdev *sd, int value)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);
	int ret;
	u16 val;

	/* enable group hold */
	ret = s5k4h8yx_write_reg_array(c, s5k4h8yx_param_hold);

	ret = s5k4h8yx_read_reg(c, S5K4H8YX_8BIT,
				S5K4H8YX_IMG_ORIENTATION, &val);
	if (ret)
		return ret;
	if (value)
		val |= S5K4H8YX_HFLIP_BIT;
	else
		val &= ~S5K4H8YX_HFLIP_BIT;
	ret = s5k4h8yx_write_reg(c, S5K4H8YX_8BIT,
				S5K4H8YX_IMG_ORIENTATION, val);
	if (ret)
		return ret;

	ret = s5k4h8yx_write_reg_array(c, s5k4h8yx_param_update);

	dev->flip = val;

	return ret;
}

/* Vertically flip the image */
static int s5k4h8yx_t_vflip(struct v4l2_subdev *sd, int value)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);
	int ret;
	u16 val;

	/* enable group hold */
	ret = s5k4h8yx_write_reg_array(c, s5k4h8yx_param_hold);

	ret = s5k4h8yx_read_reg(c, S5K4H8YX_8BIT,
				S5K4H8YX_IMG_ORIENTATION, &val);
	if (ret)
		return ret;
	if (value)
		val |= S5K4H8YX_VFLIP_BIT;
	else
		val &= ~S5K4H8YX_VFLIP_BIT;
	ret = s5k4h8yx_write_reg(c, S5K4H8YX_8BIT,
				S5K4H8YX_IMG_ORIENTATION, val);
	if (ret)
		return ret;

	ret = s5k4h8yx_write_reg_array(c, s5k4h8yx_param_update);

	dev->flip = val;

	return ret;
}

static int s5k4h8yx_test_pattern(struct v4l2_subdev *sd, s32 value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return s5k4h8yx_write_reg(client, S5K4H8YX_16BIT,
			S5K4H8YX_TEST_PATTERN_MODE, 0x0304);
}

static int s5k4h8yx_g_bin_factor_x(struct v4l2_subdev *sd, s32 *val)
{
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);

	*val = s5k4h8yx_res[dev->fmt_idx].bin_factor_x;

	return 0;
}

static int s5k4h8yx_g_bin_factor_y(struct v4l2_subdev *sd, s32 *val)
{
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);

	*val = s5k4h8yx_res[dev->fmt_idx].bin_factor_y;

	return 0;
}

static int s5k4h8yx_q_focus_status(struct v4l2_subdev *sd, s32 *value)
{
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);
	int ret = 0;

	if (dev->vcm_driver && dev->vcm_driver->q_focus_status)
		ret = dev->vcm_driver->q_focus_status(sd, value);

	return ret;
}

static int s5k4h8yx_q_focus_abs(struct v4l2_subdev *sd, s32 *value)
{
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);
	int ret = 0;

	if (dev->vcm_driver && dev->vcm_driver->q_focus_abs)
		ret = dev->vcm_driver->q_focus_abs(sd, value);

	return ret;
}

static int s5k4h8yx_t_focus_abs(struct v4l2_subdev *sd, s32 value)
{
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);
	int ret = 0;

	if (dev->vcm_driver && dev->vcm_driver->t_focus_abs)
		ret = dev->vcm_driver->t_focus_abs(sd, value);

	return ret;
}

static int s5k4h8yx_t_focus_rel(struct v4l2_subdev *sd, s32 value)
{
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);
	int ret = 0;

	if (dev->vcm_driver && dev->vcm_driver->t_focus_rel)
		ret = dev->vcm_driver->t_focus_rel(sd, value);

	return ret;
}


static struct s5k4h8yx_control s5k4h8yx_controls[] = {
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
		.query = s5k4h8yx_q_exposure,
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
			.flags = V4L2_CTRL_FLAG_READ_ONLY |
				V4L2_CTRL_FLAG_VOLATILE,
		},
		.query = s5k4h8yx_q_focus_status,
	},
	{
		.qc = {
			.id = V4L2_CID_FOCUS_RELATIVE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focus move relative",
			.minimum = 0,
			.maximum = 1023,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.tweak = s5k4h8yx_t_focus_rel,
	},
	{
		.qc = {
			.id = V4L2_CID_FOCUS_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focus move absolute",
			.minimum = 0,
			.maximum = 1023,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.tweak = s5k4h8yx_t_focus_abs,
		.query = s5k4h8yx_q_focus_abs,

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
		.tweak = s5k4h8yx_test_pattern,
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
		.tweak = s5k4h8yx_t_vflip,
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
		.tweak = s5k4h8yx_t_hflip,
	},
	{
		.qc = {
			.id = V4L2_CID_FOCAL_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focal length",
			.minimum = S5K4H8YX_FOCAL_LENGTH_DEFAULT,
			.maximum = S5K4H8YX_FOCAL_LENGTH_DEFAULT,
			.step = 0x01,
			.default_value = S5K4H8YX_FOCAL_LENGTH_DEFAULT,
			.flags = 0,
		},
		.query = s5k4h8yx_g_focal,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number",
			.minimum = S5K4H8YX_F_NUMBER_DEFAULT,
			.maximum = S5K4H8YX_F_NUMBER_DEFAULT,
			.step = 0x01,
			.default_value = S5K4H8YX_F_NUMBER_DEFAULT,
			.flags = 0,
		},
		.query = s5k4h8yx_g_fnumber,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_RANGE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number range",
			.minimum = S5K4H8YX_F_NUMBER_RANGE,
			.maximum =  S5K4H8YX_F_NUMBER_RANGE,
			.step = 0x01,
			.default_value = S5K4H8YX_F_NUMBER_RANGE,
			.flags = 0,
		},
		.query = s5k4h8yx_g_fnumber_range,
	},
	{
		.qc = {
			.id = V4L2_CID_BIN_FACTOR_HORZ,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "horizontal binning factor",
			.minimum = 0,
			.maximum = S5K4H8YX_BIN_FACTOR_MAX,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.query = s5k4h8yx_g_bin_factor_x,
	},
	{
		.qc = {
			.id = V4L2_CID_BIN_FACTOR_VERT,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "vertical binning factor",
			.minimum = 0,
			.maximum = S5K4H8YX_BIN_FACTOR_MAX,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.query = s5k4h8yx_g_bin_factor_y,
	},
};
#define N_CONTROLS (ARRAY_SIZE(s5k4h8yx_controls))

static long __s5k4h8yx_set_exposure(struct v4l2_subdev *sd,
					u16 coarse_itg, u16 gain)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	u16 lines_per_frame;
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);

	/* Validate exposure:  cannot exceed VTS-4 where VTS is 16bit */
	coarse_itg = clamp_t(u16, coarse_itg,
		S5K4H8YX_COARSE_INTEGRATION_TIME_MIN,
					S5K4H8YX_MAX_EXPOSURE_SUPPORTED);
	/* Validate gain: must not exceed maximum 8bit value */
	gain = clamp_t(u16, gain, S5K4H8YX_MIN_GLOBAL_GAIN_SUPPORTED,
					S5K4H8YX_MAX_GLOBAL_GAIN_SUPPORTED);

	/* enable group hold */
	ret = s5k4h8yx_write_reg_array(client, s5k4h8yx_param_hold);
	if (ret)
		goto out;

	/* check coarse integration time margin */
	if (coarse_itg > dev->lines_per_frame -
					S5K4H8YX_COARSE_INTEGRATION_TIME_MARGIN)
		lines_per_frame = coarse_itg +
					S5K4H8YX_COARSE_INTEGRATION_TIME_MARGIN;
	else
		lines_per_frame = dev->lines_per_frame;

	ret = s5k4h8yx_write_reg(client, S5K4H8YX_16BIT,
				S5K4H8YX_FRAME_LENGTH_LINES,
				lines_per_frame);
	if (ret)
		goto out_disable;

	/* set exposure gain */
	ret = s5k4h8yx_write_reg(client, S5K4H8YX_16BIT,
				S5K4H8YX_COARSE_INTEGRATION_TIME,
				coarse_itg);
	if (ret)
		goto out_disable;

	/* set analogue gain */
	ret = s5k4h8yx_write_reg(client, S5K4H8YX_16BIT,
					S5K4H8YX_GLOBAL_GAIN, gain);
	if (ret)
		goto out_disable;

	dev->gain       = gain;
	dev->coarse_itg = coarse_itg;

out_disable:
	s5k4h8yx_write_reg_array(client, s5k4h8yx_param_update);
out:
	return ret;
}

static int s5k4h8yx_set_exposure(struct v4l2_subdev *sd,
					u16 exposure, u16 gain)
{
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);
	int ret;

	mutex_lock(&dev->input_lock);
	ret = __s5k4h8yx_set_exposure(sd, exposure, gain);
	mutex_unlock(&dev->input_lock);

	return ret;
}
static long s5k4h8yx_s_exposure(struct v4l2_subdev *sd,
			struct atomisp_exposure *exposure)
{
	u16 coarse_itg, gain;

	coarse_itg = exposure->integration_time[0];
	gain = exposure->gain[0];

	return s5k4h8yx_set_exposure(sd, coarse_itg, gain);
}

static int s5k4h8yx_g_priv_int_data(struct v4l2_subdev *sd,
				  struct v4l2_private_int_data *priv)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);
	u8 __user *to = priv->data;
	u32 read_size = priv->size;
	int ret;
	dev_info(&client->dev, "enter s5k4h8yx_g_priv_int_data\n");
	/* No need to copy data if size is 0 */
	if (!read_size)
		goto out;

	if (dev->otp_data == NULL) {
		dev_err(&client->dev, "OTP data not available");
		return -EPERM;
	}
	/* Correct read_size value only if bigger than maximum */
	if (read_size > DEFAULT_DW9761_OTP_SIZE)
		read_size = DEFAULT_DW9761_OTP_SIZE;
			ret = copy_to_user(to, dev->otp_data, read_size);
	if (ret) {
		dev_err(&client->dev, "%s: failed to copy OTP data to user\n",
			 __func__);
		return -EFAULT;
	}
out:
	/* Return correct size */
	priv->size = DEFAULT_DW9761_OTP_SIZE;

	return 0;
}


static long s5k4h8yx_ioctl(struct v4l2_subdev *sd,
						unsigned int cmd, void *arg)
{

	switch (cmd) {
	case ATOMISP_IOC_S_EXPOSURE:
		return s5k4h8yx_s_exposure(sd, (struct atomisp_exposure *)arg);
	case ATOMISP_IOC_G_SENSOR_PRIV_INT_DATA:
		return s5k4h8yx_g_priv_int_data(sd, arg);
	default:
		return -EINVAL;
	}
	return 0;
}

static struct s5k4h8yx_control *s5k4h8yx_find_control(__u32 id)
{
	int i;

	for (i = 0; i < N_CONTROLS; i++) {
		if (s5k4h8yx_controls[i].qc.id == id)
			return &s5k4h8yx_controls[i];
	}
	return NULL;
}

static u8 vid;
static struct proc_dir_entry *iddir;
static struct proc_dir_entry *idfile;
static int vendorid_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", vid);
	return 0;
}

static int vendorid_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, vendorid_proc_show, NULL);
}
static const struct file_operations vendorid_proc_fops = {
	.owner = THIS_MODULE,
	.open = vendorid_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int s5k4h8yx_vendorid_procfs_init(struct i2c_client *client)
{
	iddir = proc_mkdir("camera", NULL);
	if (!iddir) {
		dev_err(&client->dev, "can't create /proc/camera/\n");
		return -EPERM;
	}

	idfile = proc_create("backvid", 0644, iddir,
				&vendorid_proc_fops);
	if (!idfile) {
		dev_err(&client->dev, "Can't create file /proc/camera/backvid\n");
		remove_proc_entry("camera", iddir);
		return -EPERM;
	}

	return 0;
}
static int s5k4h8yx_vendorid_procfs_uninit(void)
{
	if (idfile != NULL)
		proc_remove(idfile);
	if (iddir != NULL)
		proc_remove(iddir);
	return 0;
}
static int s5k4h8yx_vendorid_set(u8 vendorid)
{
	vid = vendorid;
	return 0;
}

static int s5k4h8yx_detect(struct i2c_client *client, u16 *id, u8 *revision)
{
	struct i2c_adapter *adapter = client->adapter;
	u16 high, low, rev;

	/* i2c check */
	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -ENODEV;

	/* check sensor chip ID	 */
	if (s5k4h8yx_read_reg(client, S5K4H8YX_8BIT, S5K4H8YX_PID_HIGH,
			     &high)) {
		dev_err(&client->dev, "sensor_id_high = 0x%x\n", high);
		return -ENODEV;
	}

	if (s5k4h8yx_read_reg(client, S5K4H8YX_8BIT, S5K4H8YX_PID_LOW,
			     &low)) {
		dev_err(&client->dev, "sensor_id_low = 0x%x\n", low);
		return -ENODEV;
	}

	*id = (((u8) high) << 8) | (u8) low;

	if (*id != S5K4H8YX_MOD_ID) {
		dev_err(&client->dev, "main sensor s5k4h8yx ID error\n");
		return -ENODEV;
	}

	dev_info(&client->dev, "sensor detect find sensor_id = 0x%x\n", *id);

	if (s5k4h8yx_read_reg(client, S5K4H8YX_8BIT, S5K4H8YX_REV,
			     &rev)) {
		dev_err(&client->dev, "reversion = 0x%x\n", rev);
		return -ENODEV;
	}

	*revision = rev;

	return 0;
}

static int
s5k4h8yx_s_config(struct v4l2_subdev *sd, int irq, void *platform_data)
{
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u8 sensor_revision;
	u16 sensor_id;
	u8 vendor_id = 0;
	int ret;

	if (NULL == platform_data)
		return -ENODEV;

	dev->platform_data =
		(struct camera_sensor_platform_data *)platform_data;

	mutex_lock(&dev->input_lock);
	ret = __s5k4h8yx_s_power(sd, 1);
	if (ret) {
		mutex_unlock(&dev->input_lock);
		dev_err(&client->dev, "s5k4h8yx power-up err");
		return ret;
	}

	/* config & detect sensor */
	ret = s5k4h8yx_detect(client, &sensor_id, &sensor_revision);
	if (ret) {
		dev_err(&client->dev, "s5k4h8yx_detect err s_config.\n");
		goto fail_detect;
	}
	dev->sensor_id = sensor_id;
	dev->sensor_revision = sensor_revision;



	if (dev->otp_driver) {
		/* Read sensor's OTP data should only do once*/
		dev_info(&client->dev, "before call otp_read\n");
		dev->otp_data = dev->otp_driver->otp_read(sd,
			&dev->otp_raw_data, &vendor_id);
		dev->module_vendor_id = vendor_id;
		s5k4h8yx_vendorid_set(dev->module_vendor_id);
	}


	ret = dev->platform_data->csi_cfg(sd, 1);
	if (ret)
		goto fail_csi_cfg;

	ret = __s5k4h8yx_s_power(sd, 0);
	mutex_unlock(&dev->input_lock);
	if (ret) {
		dev_err(&client->dev, "s5k4h8yx power down err\n");
		return ret;
	}
	dev_info(&client->dev, "s_config finish\n");
	return 0;

fail_csi_cfg:
	dev->platform_data->csi_cfg(sd, 0);
fail_detect:
	__s5k4h8yx_s_power(sd, 0);
	mutex_unlock(&dev->input_lock);
	dev_err(&client->dev, "s5k4h8yx sensor power-gating failed\n");
	return ret;
}

static int s5k4h8yx_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	struct s5k4h8yx_control *ctrl = s5k4h8yx_find_control(qc->id);
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);

	if (ctrl == NULL)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	*qc = ctrl->qc;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static int s5k4h8yx_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct s5k4h8yx_control *octrl = s5k4h8yx_find_control(ctrl->id);
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);
	int ret;

	if (octrl == NULL)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	ret = octrl->query(sd, &ctrl->value);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int s5k4h8yx_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct s5k4h8yx_control *octrl = s5k4h8yx_find_control(ctrl->id);
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);
	int ret;

	if (!octrl || !octrl->tweak)
		return -EINVAL;
	mutex_lock(&dev->input_lock);
	ret = octrl->tweak(sd, ctrl->value);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int s5k4h8yx_recovery(struct v4l2_subdev *sd)
{
	int ret;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);

	ret = __s5k4h8yx_s_power(sd, 0);
	if (ret) {
		dev_err(&client->dev, "power-down err.\n");
		return ret;
	}

	ret = __s5k4h8yx_s_power(sd, 1);
	if (ret) {
		dev_err(&client->dev, "power-up err.\n");
		return ret;
	}

	/* enable group hold */
	ret = s5k4h8yx_write_reg_array(client, s5k4h8yx_param_hold);
	if (ret)
		return ret;

	ret = s5k4h8yx_write_reg(client, S5K4H8YX_8BIT,
			S5K4H8YX_IMG_ORIENTATION, dev->flip);
	if (ret)
		return ret;

	ret = s5k4h8yx_write_reg_array(client, s5k4h8yx_res[dev->fmt_idx].regs);
	if (ret)
		return ret;

	/* disable group hold */
	ret = s5k4h8yx_write_reg_array(client, s5k4h8yx_param_update);
	if (ret)
		return ret;

	return ret;
}


static int s5k4h8yx_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret;
	u16 id;
	u8 rev;

	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);

	mutex_lock(&dev->input_lock);
	if (enable) {
		ret = s5k4h8yx_detect(client, &id, &rev);
		if (ret) {
			ret = s5k4h8yx_recovery(sd);
			if (ret) {
				dev_err(&client->dev, "recovery err.\n");
				mutex_unlock(&dev->input_lock);
				return ret;
			}
		}

		ret = s5k4h8yx_write_reg_array(client,
							s5k4h8yx_streaming);

		if (ret) {
			mutex_unlock(&dev->input_lock);
			return ret;
		}

		dev->streaming = 1;
	} else {

		ret = s5k4h8yx_write_reg_array(client,
							s5k4h8yx_suspend);

		if (ret != 0) {
			mutex_unlock(&dev->input_lock);
			return ret;
		}
		dev->streaming = 0;
	}

	mutex_unlock(&dev->input_lock);
	return 0;
}

/*
 * s5k4h8yx enum frame size, frame intervals
 */
static int s5k4h8yx_enum_framesizes(struct v4l2_subdev *sd,
				   struct v4l2_frmsizeenum *fsize)
{
	unsigned int index = fsize->index;

	if (index >= N_RES)
		return -EINVAL;

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = s5k4h8yx_res[index].width;
	fsize->discrete.height = s5k4h8yx_res[index].height;
	fsize->reserved[0] = s5k4h8yx_res[index].used;

	return 0;
}

static int s5k4h8yx_enum_frameintervals(struct v4l2_subdev *sd,
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
	fival->width = s5k4h8yx_res[i].width;
	fival->height = s5k4h8yx_res[i].height;
	fival->discrete.numerator = 1;
	fival->discrete.denominator = s5k4h8yx_res[i].fps;

	return 0;
}

static int s5k4h8yx_enum_mbus_fmt(struct v4l2_subdev *sd,
					unsigned int index,
				 enum v4l2_mbus_pixelcode *code)
{
	if (index >= MAX_FMTS)
		return -EINVAL;

	*code = V4L2_MBUS_FMT_SGRBG10_1X10;
	return 0;
}

static int
s5k4h8yx_enum_mbus_code(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index >= MAX_FMTS)
		return -EINVAL;
	code->code = V4L2_MBUS_FMT_SGRBG10_1X10;

	return 0;
}

static int
s5k4h8yx_enum_frame_size(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_frame_size_enum *fse)
{
	int index = fse->index;

	if (index >= N_RES)
		return -EINVAL;

	fse->min_width = s5k4h8yx_res[index].width;
	fse->min_height = s5k4h8yx_res[index].height;
	fse->max_width = s5k4h8yx_res[index].width;
	fse->max_height = s5k4h8yx_res[index].height;

	return 0;
}

static struct v4l2_mbus_framefmt *
__s5k4h8yx_get_pad_format(struct s5k4h8yx_device *sensor,
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
s5k4h8yx_get_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);
	struct v4l2_mbus_framefmt *format =
			__s5k4h8yx_get_pad_format(dev, fh,
						fmt->pad, fmt->which);

	fmt->format = *format;

	return 0;
}

static int
s5k4h8yx_set_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		dev->format = fmt->format;

	return 0;
}

static int
s5k4h8yx_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);
	dev->run_mode = param->parm.capture.capturemode;

	mutex_lock(&dev->input_lock);
	switch (dev->run_mode) {
	case CI_MODE_VIDEO:
		s5k4h8yx_res = s5k4h8yx_res_video;
		N_RES = N_RES_VIDEO;
		break;
	case CI_MODE_STILL_CAPTURE:
		s5k4h8yx_res = s5k4h8yx_res_still;
		N_RES = N_RES_STILL;
		break;
	default:
		s5k4h8yx_res = s5k4h8yx_res_preview;
		N_RES = N_RES_PREVIEW;
	}
	mutex_unlock(&dev->input_lock);
	return 0;
}

int
s5k4h8yx_g_frame_interval(struct v4l2_subdev *sd,
				struct v4l2_subdev_frame_interval *interval)
{
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);
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
	if (dev->coarse_itg > dev->lines_per_frame -
			S5K4H8YX_COARSE_INTEGRATION_TIME_MARGIN) {

		if (dev->coarse_itg > S5K4H8YX_MAX_EXPOSURE_SUPPORTED) {
			lines_per_frame = dev->coarse_itg;
		} else {
			lines_per_frame = dev->coarse_itg +
				S5K4H8YX_COARSE_INTEGRATION_TIME_MARGIN;
		}
	} else {
		lines_per_frame = dev->lines_per_frame;
	}

	interval->interval.numerator = dev->pixels_per_line *
					lines_per_frame;
	interval->interval.denominator = dev->vt_pix_clk_freq_mhz;

	return 0;
}

static int s5k4h8yx_g_skip_frames(struct v4l2_subdev *sd, u32 *frames)
{
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);

	mutex_lock(&dev->input_lock);
	*frames = s5k4h8yx_res[dev->fmt_idx].skip_frames;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static int s5k4h8yx_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	struct s5k4h8yx_device *dev = container_of(ctrl->handler,
	struct s5k4h8yx_device, ctrl_handler);
	unsigned int val;

	switch (ctrl->id) {
	case V4L2_CID_LINK_FREQ:
		val = s5k4h8yx_res[dev->fmt_idx].mipi_freq;
		if (val == 0)
			return -EINVAL;

		ctrl->val = val * 1000;	/* To Hz */
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static struct v4l2_ctrl_ops s5k4h8yx_ctrl_ops = {
	.g_volatile_ctrl = s5k4h8yx_g_volatile_ctrl,
};

static const struct v4l2_ctrl_config v4l2_ctrl_link_freq = {
	.ops = &s5k4h8yx_ctrl_ops,
	.id = V4L2_CID_LINK_FREQ,
	.name = "Link Frequency",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 1,
	.max = 1500000 * 1000,
	.step = 1,
	.def = 1,
	.flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_READ_ONLY,
};

static const struct v4l2_subdev_sensor_ops s5k4h8yx_sensor_ops = {
	.g_skip_frames = s5k4h8yx_g_skip_frames,
};

static const struct v4l2_subdev_video_ops s5k4h8yx_video_ops = {
	.try_mbus_fmt = s5k4h8yx_try_mbus_fmt,
	.s_mbus_fmt = s5k4h8yx_set_mbus_fmt,
	.s_stream = s5k4h8yx_s_stream,
	.enum_framesizes = s5k4h8yx_enum_framesizes,
	.enum_frameintervals = s5k4h8yx_enum_frameintervals,
	.s_parm = s5k4h8yx_s_parm,
	.g_mbus_fmt = s5k4h8yx_g_mbus_fmt,
	.enum_mbus_fmt = s5k4h8yx_enum_mbus_fmt,
	.g_frame_interval = s5k4h8yx_g_frame_interval,
};

static const struct v4l2_subdev_core_ops s5k4h8yx_core_ops = {
	.queryctrl = s5k4h8yx_queryctrl,
	.g_ctrl = s5k4h8yx_g_ctrl,
	.s_ctrl = s5k4h8yx_s_ctrl,
	.ioctl = s5k4h8yx_ioctl,
	.s_power = s5k4h8yx_s_power,
	.init = s5k4h8yx_init,
};

static const struct v4l2_subdev_pad_ops s5k4h8yx_pad_ops = {
	.enum_mbus_code = s5k4h8yx_enum_mbus_code,
	.enum_frame_size = s5k4h8yx_enum_frame_size,
	.get_fmt = s5k4h8yx_get_pad_format,
	.set_fmt = s5k4h8yx_set_pad_format,
};

static const struct v4l2_subdev_ops s5k4h8yx_ops = {
	.core = &s5k4h8yx_core_ops,
	.video = &s5k4h8yx_video_ops,
	.pad = &s5k4h8yx_pad_ops,
	.sensor = &s5k4h8yx_sensor_ops,
};

static struct s5k4h8yx_vcm s5k4h8yx_vcm_ops = {
	.power_up = dw9761_vcm_power_up,
	.power_down = dw9761_vcm_power_down,
	.init = dw9761_vcm_init,
	.t_focus_vcm = dw9761_t_focus_vcm,
	.t_focus_abs = dw9761_t_focus_abs,
	.t_focus_rel = dw9761_t_focus_rel,
	.q_focus_status = dw9761_q_focus_status,
	.q_focus_abs = dw9761_q_focus_abs,
};

static struct s5k4h8yx_otp s5k4h8yx_otp_ops = {
		.otp_read = dw9761_otp_read,
};

static int s5k4h8yx_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct s5k4h8yx_device *dev = to_s5k4h8yx_sensor(sd);

	dev->platform_data->csi_cfg(sd, 0);
	v4l2_device_unregister_subdev(sd);
	media_entity_cleanup(&dev->sd.entity);
	atomisp_gmin_remove_subdev(sd);
	s5k4h8yx_vendorid_procfs_uninit();
	kfree(dev);

	return 0;
}

static int s5k4h8yx_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct s5k4h8yx_device *dev;
	int ret = 0;
	void *pdata = client->dev.platform_data;

	dev_info(&client->dev, "%s start build time %s\n", __func__, __TIME__);

	/* allocate sensor device & init sub device */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&client->dev, "%s: out of memory\n", __func__);
		return -ENOMEM;
	}

	mutex_init(&dev->input_lock);

	dev->fmt_idx = 0;
	dev->module_vendor_id = 0x1;/* default 0x01 is sunny */
	dev->otp_data = NULL;
	dev->otp_driver = &s5k4h8yx_otp_ops;
	dev->vcm_driver = &s5k4h8yx_vcm_ops;
	iddir = NULL;
	idfile = NULL;

	v4l2_i2c_subdev_init(&(dev->sd), client, &s5k4h8yx_ops);

	if (ACPI_COMPANION(&client->dev))
		pdata = gmin_camera_platform_data(&dev->sd,
					  ATOMISP_INPUT_FORMAT_RAW_10,
					  atomisp_bayer_order_grbg);
	dev_info(&client->dev, "before s_config\n");
	if (NULL != pdata) {
		ret = s5k4h8yx_s_config(&dev->sd, client->irq, pdata);
		if (ret) {
			dev_err(&client->dev, "%s: configuration fail!!\n",
							__func__);
			atomisp_gmin_remove_subdev(&dev->sd);
			goto out_free;
		}
	}

	ret = atomisp_register_i2c_module(&dev->sd, pdata, RAW_CAMERA);
	if (ret)
		goto out_free;

	dev->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->pad.flags = MEDIA_PAD_FL_SOURCE;
	dev->format.code = V4L2_MBUS_FMT_SGRBG10_1X10;
	dev->sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
	dev->flip = 0;

	ret = media_entity_init(&dev->sd.entity, 1, &dev->pad, 0);
	if (ret)
		s5k4h8yx_remove(client);

	/* vcm initialization */

	if (dev->vcm_driver && dev->vcm_driver->init)
		ret = dev->vcm_driver->init(&dev->sd);
	if (ret) {
		dev_err(&client->dev, "vcm init failed.\n");
		s5k4h8yx_remove(client);
	}


	ret = s5k4h8yx_vendorid_procfs_init(client);
	if (ret) {
		dev_err(&client->dev, "%s Failed to proc fs\n", __func__);
		s5k4h8yx_vendorid_procfs_uninit();
	}

	global_dev = dev;
	v4l2_info(client, "%s: done!!\n", __func__);

	return ret;

out_free:
	v4l2_device_unregister_subdev(&dev->sd);
	kfree(dev);
	return ret;
}

static const struct i2c_device_id s5k4h8yx_id[] = {
	{S5K4H8YX_NAME, 0},
	{ }
};

static struct acpi_device_id S5K4H8YX_acpi_match[] = {
	{ "SS5K0008" },
	{},
};


MODULE_DEVICE_TABLE(i2c, s5k4h8yx_id);

static struct i2c_driver s5k4h8yx_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = S5K4H8YX_NAME,
		.acpi_match_table = ACPI_PTR(S5K4H8YX_acpi_match),
	},
	.probe = s5k4h8yx_probe,
	.remove = s5k4h8yx_remove,
	.id_table = s5k4h8yx_id,
};

static __init int init_s5k4h8yx(void)
{
	return i2c_add_driver(&s5k4h8yx_driver);
}

static __exit void exit_s5k4h8yx(void)
{
	i2c_del_driver(&s5k4h8yx_driver);
}

module_init(init_s5k4h8yx);
module_exit(exit_s5k4h8yx);

MODULE_DESCRIPTION("A low-level driver for S5K4H8YX sensor");
MODULE_AUTHOR("HARVEY LV <harvey.lv@intel.com>");
MODULE_LICENSE("GPL");
