/*
 * Support for Sony gc2235 8MP camera sensor.
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
#include <linux/acpi.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/kmod.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <media/v4l2-device.h>
#include <asm/intel-mid.h>
#include <linux/atomisp_gmin_platform.h>
#include "gc2235.h"

#ifndef POWER_ALWAYS_ON_BEFORE_SUSPEND
#define POWER_ALWAYS_ON_BEFORE_SUSPEND
#endif

static enum atomisp_bayer_order gc2235_bayer_order_mapping[] = {
	atomisp_bayer_order_gbrg,
	atomisp_bayer_order_bggr,
	atomisp_bayer_order_rggb,
	atomisp_bayer_order_grbg,
};

u16 g_gain = 0, g_exposure = 0;
u8 g_flip = 0x14;

static int
gc2235_read_reg(struct i2c_client *client, u8 len, u8 reg, u8 *val)
{
	struct i2c_msg msg[2];
	unsigned char data[GC2235_SHORT_MAX];
	int err, i;

	if (len > GC2235_BYTE_MAX) {
		dev_err(&client->dev, "%s error, invalid data length\n",
			__func__);
		return -EINVAL;
	}

	memset(msg, 0 , sizeof(msg));
	memset(data, 0 , sizeof(data));

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = (u8 *)data;
	/* high byte goes first */
	data[0] = reg;

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
	if (len == GC2235_8BIT) {
		*val = (u8)data[0];
	} else {
		/* 16-bit access is default when len > 1 */
		for (i = 0; i < (len >> 1); i++)
			val[i] = be16_to_cpu(data[i]);
	}

	return 0;

error:
	dev_err(&client->dev, "read from offset 0x%x error %d", reg, err);
	return err;
}

static int gc2235_i2c_write(struct i2c_client *client, u16 len, u8 *data)
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
gc2235_write_reg(struct i2c_client *client, u16 data_length, u16 reg, u16 val)
{
	int ret;
	unsigned char data[4] = {0};
	const u16 len = data_length + 1; /* 16-bit address + data */

	if (data_length != GC2235_8BIT && data_length != GC2235_16BIT) {
		v4l2_err(client, "%s error, invalid data_length\n", __func__);
		return -EINVAL;
	}

	/* high byte goes out first */
	data[0] = reg;
	if (data_length == GC2235_8BIT)
		data[1] = (u8)(val);
	else {
		/* GC2235_16BIT */
		u16 *wdata = (u16 *)&data[2];
		*wdata = cpu_to_be16(val);
	}

	ret = gc2235_i2c_write(client, len, data);
	if (ret)
		dev_err(&client->dev,
			"write error: wrote 0x%x to offset 0x%x error %d",
			val, reg, ret);

	return ret;
}

static int gc2235_write_reg_array(struct i2c_client *client,
				   const struct gc2235_reg *reglist)
{
	const struct gc2235_reg *next = reglist;
	struct gc2235_write_ctrl ctrl;

	ctrl.index = 0;
	for (; next->type != GC2235_TOK_TERM; next++) {
		switch (next->type & GC2235_TOK_MASK) {
		case GC2235_TOK_DELAY:
			msleep(next->val);
			break;

		default:
			gc2235_write_reg(client, GC2235_8BIT,
			       next->sreg, next->val);
		}
	}

	return 0;
}

static int __gc2235_update_exposure_timing(struct i2c_client *client,
					u16 exposure, u16 llp, u16 fll)
{
	int ret = 0;
	u8 expo_coarse_h, expo_coarse_l;

	/* Increase the VTS to match exposure + margin */
	if (exposure > fll - GC2235_INTEGRATION_TIME_MARGIN)
		fll = exposure + GC2235_INTEGRATION_TIME_MARGIN;

	expo_coarse_h = (u8)(exposure >> 8);
	expo_coarse_l = (u8)(exposure & 0xff);
	ret = gc2235_write_reg(client, GCSENSOR_8BIT, GC2235_REG_EXPO_COARSE, expo_coarse_h);
	if (ret)
		return ret;
	ret = gc2235_write_reg(client, GCSENSOR_8BIT, GC2235_REG_EXPO_COARSE+1, expo_coarse_l);

	return ret;
}

static int __gc2235_update_gain(struct v4l2_subdev *sd, u16 gain)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u16 temp;

	/* set global gain */
	if (256 > gain)	{
		gc2235_write_reg(client, GCSENSOR_8BIT, 0xb0, 0x40);
		gc2235_write_reg(client, GCSENSOR_8BIT, 0xb1, gain & 0xff);
	} else {
		temp = 64 * gain / 256;
		if (temp > 0xff)
			temp = 0xff;
		gc2235_write_reg(client, GCSENSOR_8BIT, 0xb0, temp & 0xff);
		gc2235_write_reg(client, GCSENSOR_8BIT, 0xb1, 0xff);
	}

	return 0;
}

static int gc2235_set_exposure_gain(struct v4l2_subdev *sd, u16 coarse_itg,
	u16 gain, u16 digitgain)
{
	struct gc2235_device *dev = to_gc2235_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	/* Validate exposure:  cannot exceed VTS-4 where VTS is 16bit */
	coarse_itg = clamp_t(u16, coarse_itg, 0, GC2235_MAX_EXPOSURE_SUPPORTED);

	/* Validate gain: must not exceed maximum 8bit value */
	gain = clamp_t(u16, gain, 0, GC2235_MAX_GLOBAL_GAIN_SUPPORTED);
	g_gain = gain;
	g_exposure = coarse_itg;

	mutex_lock(&dev->input_lock);

	ret = __gc2235_update_exposure_timing(client, coarse_itg,
			dev->pixels_per_line, dev->lines_per_frame);
	if (ret)
		goto out;
	dev->coarse_itg = coarse_itg;

	ret = __gc2235_update_gain(sd, gain);
	if (ret)
		goto out;
	dev->gain = gain;
out:
	mutex_unlock(&dev->input_lock);
	return ret;
}

static long gc2235_s_exposure(struct v4l2_subdev *sd,
			       struct atomisp_exposure *exposure)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	dev_dbg(&client->dev, "%s(0x%X 0x%X 0x%X)\n", __func__, exposure->integration_time[0], exposure->gain[0], exposure->gain[1]);
	return gc2235_set_exposure_gain(sd, exposure->integration_time[0],
				exposure->gain[0], exposure->gain[1]);
}

static long gc2235_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	switch (cmd) {
	case ATOMISP_IOC_S_EXPOSURE:
		return gc2235_s_exposure(sd, arg);
	default:
		return -EINVAL;
	}
	return 0;
}

static int power_ctrl(struct v4l2_subdev *sd, int flag)
{
	int ret;
	struct gc2235_device *dev = to_gc2235_sensor(sd);

	if (!dev || !dev->platform_data)
		return -ENODEV;

	/* Non-gmin platforms use the legacy callback */
	if (dev->platform_data->power_ctrl)
		return dev->platform_data->power_ctrl(sd, flag);

       if (flag) {
               ret = dev->platform_data->v1p8_ctrl(sd, 1);
               usleep_range(60, 90);
               ret = dev->platform_data->v2p8_ctrl(sd, 1);
               msleep(20);
       } else {
               ret = dev->platform_data->v2p8_ctrl(sd, 0);
               ret |= dev->platform_data->v1p8_ctrl(sd, 0);
       }
       return ret;
}

static int gpio_ctrl(struct v4l2_subdev *sd, int flag)
{
	int ret;
	struct gc2235_device *dev = to_gc2235_sensor(sd);

	if (!dev || !dev->platform_data)
		return -ENODEV;

	/* Non-gmin platforms use the legacy callback */
	if (dev->platform_data->gpio_ctrl)
		return dev->platform_data->gpio_ctrl(sd, flag);

	/* GPIO0 == "reset" (active low), GPIO1 == "power down" */
	if (flag) {
		ret = dev->platform_data->gpio1_ctrl(sd, 0);
		ret |= dev->platform_data->gpio0_ctrl(sd, 1);
	} else {
		ret = dev->platform_data->gpio1_ctrl(sd, 1);
		ret |= dev->platform_data->gpio0_ctrl(sd, 0);
	}
	return ret;
}


static int power_up(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct gc2235_device *dev = to_gc2235_sensor(sd);
	int ret;

       /* power control */
	ret = power_ctrl(sd, 1);
	if (ret)
		goto fail_power;

	/* flis clock control*/
	ret = dev->platform_data->flisclk_ctrl(sd, 1);
	if (ret)
		goto fail_clk;

	/*MCLK to PWDN*/
	usleep_range(1000, 1500);

	/* gpio ctrl*/
	ret = gpio_ctrl(sd, 1);
	if (ret) {
		dev_err(&client->dev, "gpio failed\n");
		goto fail_gpio;
	}

	msleep(50);
	return 0;

fail_gpio:
	gpio_ctrl(sd, 0);
fail_clk:
	dev->platform_data->flisclk_ctrl(sd, 0);
fail_power:
	power_ctrl(sd, 0);
	dev_err(&client->dev, "sensor power-up failed\n");

	return ret;
}

static int power_down(struct v4l2_subdev *sd)
{
	struct gc2235_device *dev = to_gc2235_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = dev->platform_data->flisclk_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "flisclk failed\n");

	/* gpio ctrl*/
	ret = gpio_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "gpio failed\n");

	/* power control */
	ret = power_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "vprog failed.\n");

	msleep(20);

	return ret;
}

static int gc2235_set_suspend(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	gc2235_write_reg(client, GCSENSOR_8BIT, 0xfe, 0x03);
	gc2235_write_reg(client, GCSENSOR_8BIT,  0x10, 0x81);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0xfe, 0x00);

	return 0;
}

static int gc2235_set_streaming(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	gc2235_write_reg(client, GCSENSOR_8BIT, 0xfe, 0x30);
	__gc2235_update_exposure_timing(client, g_exposure, 0, 0);
	__gc2235_update_gain(sd, g_gain);

	gc2235_write_reg(client, GCSENSOR_8BIT, 0xfe, 0x03);
	gc2235_write_reg(client, GCSENSOR_8BIT,  0x10, 0x91);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0xfe, 0x00);
	return 0;
}

static int gc2235_init_common(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = -1;

       if (0 != (ret = gc2235_write_reg(client, GCSENSOR_8BIT, 0xfe, 0x80))) {
               dev_err(&client->dev, "%s:init common error", __func__);
               return ret;
       }

	gc2235_write_reg(client, GCSENSOR_8BIT, 0xfe, 0x80);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0xfe, 0x80);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0xf2, 0x00);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0xf6, 0x00);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0xfc, 0x06);

	gc2235_write_reg(client, GCSENSOR_8BIT, 0xf7, 0x15);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0xf8, 0x85);

	gc2235_write_reg(client, GCSENSOR_8BIT, 0xfa, 0x00);

	gc2235_write_reg(client, GCSENSOR_8BIT, 0xf9, 0xfe);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0xfe, 0x00);

	gc2235_write_reg(client, GCSENSOR_8BIT, 0x03, 0x04);

	gc2235_write_reg(client, GCSENSOR_8BIT, 0x04, 0xb0);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x05, 0x01);

	gc2235_write_reg(client, GCSENSOR_8BIT, 0x06, 0x2a);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x07, 0x00);

	gc2235_write_reg(client, GCSENSOR_8BIT, 0x08, 0x30);

	gc2235_write_reg(client, GCSENSOR_8BIT, 0x0a, 0x02);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x0c, 0x00);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x0d, 0x04);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x0e, 0xd0);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x0f, 0x06);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x10, 0x58);

	gc2235_write_reg(client, GCSENSOR_8BIT, 0x17, 0x14);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x17, g_flip);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x18, 0x12);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x19, 0x0d);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x1a, 0x01);

	gc2235_write_reg(client, GCSENSOR_8BIT, 0x1b, 0x48);

	gc2235_write_reg(client, GCSENSOR_8BIT, 0x1e, 0x88);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x1f, 0x48);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x20, 0x03);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x21, 0x6f);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x22, 0x80);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x23, 0xc1);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x24, 0x2f);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x26, 0x01);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x27, 0x30);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x3f, 0x00);

	gc2235_write_reg(client, GCSENSOR_8BIT, 0x8b, 0xa4);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x8c, 0x02);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x90, 0x01);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x92, 0x02);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x94, 0x00);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x95, 0x04);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x96, 0xc0);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x97, 0x06);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x98, 0x50);

	gc2235_write_reg(client, GCSENSOR_8BIT, 0x40, 0x72);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x41, 0x04);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x5e, 0x00);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x5f, 0x00);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x60, 0x00);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x61, 0x00);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x62, 0x00);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x63, 0x00);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x64, 0x00);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x65, 0x00);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x66, 0x20);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x67, 0x20);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x68, 0x20);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x69, 0x20);

	gc2235_write_reg(client, GCSENSOR_8BIT, 0xb2, 0x00);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0xb3, 0x40);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0xb4, 0x40);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0xb5, 0x40);

	gc2235_write_reg(client, GCSENSOR_8BIT, 0xb8, 0x0f);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0xb9, 0x23);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0xba, 0xff);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0xbc, 0x00);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0xbd, 0x00);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0xbe, 0xff);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0xbf, 0x09);

	gc2235_write_reg(client, GCSENSOR_8BIT, 0xfe, 0x03);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x01, 0x07);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x02, 0x11);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x03, 0x11);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x06, 0x80);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x11, 0x2b);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x12, 0xe4);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x13, 0x07);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x15, 0x12);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x04, 0x20);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x05, 0x00);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x17, 0x01);

	gc2235_write_reg(client, GCSENSOR_8BIT, 0x21, 0x01);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x22, 0x02);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x23, 0x01);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x29, 0x02);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0x2a, 0x01);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0xfe, 0x00);
	gc2235_write_reg(client, GCSENSOR_8BIT, 0xf2, 0x00);

	return 0;
}

static int __gc2235_s_power(struct v4l2_subdev *sd, int on)
{
	struct gc2235_device *dev = to_gc2235_sensor(sd);
	int ret = 0;

	if (on == 0) {
		ret = power_down(sd);
		dev->power = 0;
	} else {
		ret = power_up(sd);
		if (!ret) {
			dev->power = 1;
			return gc2235_init_common(sd);
		}
	}

	return ret;
}

#ifndef POWER_ALWAYS_ON_BEFORE_SUSPEND
static int gc2235_s_power(struct v4l2_subdev *sd, int on)
{
	int ret;
	struct gc2235_device *dev = to_gc2235_sensor(sd);

	mutex_lock(&dev->input_lock);
	ret = __gc2235_s_power(sd, on);
	mutex_unlock(&dev->input_lock);

	return ret;
}
#endif

#ifdef POWER_ALWAYS_ON_BEFORE_SUSPEND
static int __gc2235_s_power_always_on(struct v4l2_subdev *sd, int on)
{
	struct gc2235_device *dev = to_gc2235_sensor(sd);
	int ret = 0;

	if (on == 0) {
		//ret = power_down(sd);
		//dev->power = 0;
	} else {
		if (!dev->power) {
			ret = power_up(sd);
			if (!ret) {
				dev->power = 1;
				dev->once_launched = 1;
				return gc2235_init_common(sd);
			}
		}
	}

	return ret;
}

static int gc2235_s_power_always_on(struct v4l2_subdev *sd, int on)
{
	int ret;
	struct gc2235_device *dev = to_gc2235_sensor(sd);

	mutex_lock(&dev->input_lock);
	ret = __gc2235_s_power_always_on(sd, on);
	mutex_unlock(&dev->input_lock);

	return ret;
}
#endif

static int gc2235_get_intg_factor(struct i2c_client *client,
				struct camera_mipi_info *info,
				const struct gc2235_reg *reglist)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc2235_device *dev = to_gc2235_sensor(sd);
	struct atomisp_sensor_mode_data *buf = &info->data;
	int ret;
	u8 data[GC2235_INTG_BUF_COUNT];

	u32 vt_pix_clk_freq_mhz;
	u32 coarse_integration_time_min;
	u32 coarse_integration_time_max_margin;
	u16 tmp;

	if (info == NULL)
		return -EINVAL;

	ret =  gc2235_write_reg(client, 1, 0xfe, 0);
	if (ret)
		return ret;

	memset(data, 0, GC2235_INTG_BUF_COUNT);
	coarse_integration_time_min = 1;
	coarse_integration_time_max_margin = 6;

	vt_pix_clk_freq_mhz = 67200000;

	dev->vt_pix_clk_freq_mhz = vt_pix_clk_freq_mhz;

	buf->vt_pix_clk_freq_mhz = vt_pix_clk_freq_mhz;
	buf->coarse_integration_time_min = coarse_integration_time_min;
	buf->coarse_integration_time_max_margin =
				coarse_integration_time_max_margin;

	buf->fine_integration_time_min = GC2235_FINE_INTG_TIME;
	buf->fine_integration_time_max_margin = GC2235_FINE_INTG_TIME;
	buf->fine_integration_time_def = GC2235_FINE_INTG_TIME;

	buf->frame_length_lines = dev->curr_res_table[dev->fmt_idx].lines_per_frame;
	buf->line_length_pck = dev->curr_res_table[dev->fmt_idx].pixels_per_line;
	buf->read_mode = dev->curr_res_table[dev->fmt_idx].bin_mode;

	/* Get the cropping and output resolution to ISP for this mode. */
	tmp = 0;
	ret = gc2235_read_reg(client, 1, GC2235_HORIZONTAL_START_H, data);
	if (ret)
		return ret;
	tmp = (data[0] & 0x07) << 8;
	ret =  gc2235_read_reg(client, 1, GC2235_HORIZONTAL_START_L, data);
	if (ret)
		return ret;
	tmp += data[0] & 0x7f;
	buf->crop_horizontal_start = tmp;

	tmp = 0;
	ret = gc2235_read_reg(client, 1, GC2235_VERTICAL_START_H, data);
	if (ret)
		return ret;
	tmp = data[0] << 8;
	ret =  gc2235_read_reg(client, 1, GC2235_VERTICAL_START_L, data);
	if (ret)
		return ret;
	tmp += data[0];
	buf->crop_vertical_start = tmp;

	tmp = 0;
	ret = gc2235_read_reg(client, 1, GC2235_HORIZONTAL_OUTPUT_SIZE_H, data);
	if (ret)
		return ret;
	tmp = (data[0] & 0x07) << 8;
	ret = gc2235_read_reg(client, 1, GC2235_HORIZONTAL_OUTPUT_SIZE_L, data);
	if (ret)
		return ret;
	tmp += data[0] & 0x7f ;
	buf->output_width = tmp;
	buf->crop_horizontal_end = buf->crop_horizontal_start + tmp - 1;

	tmp = 0;
	ret = gc2235_read_reg(client, 1, GC2235_VERTICAL_OUTPUT_SIZE_H, data);
	if (ret)
		return ret;
	tmp = (data[0] & 0x07) << 8;
	ret = gc2235_read_reg(client, 1, GC2235_VERTICAL_OUTPUT_SIZE_L, data);
	if (ret)
		return ret;
	tmp += data[0];
	buf->output_height = tmp;
	buf->crop_vertical_end = tmp + buf->crop_vertical_start - 1;

	tmp = 0;
	ret = gc2235_read_reg(client, GC2235_8BIT, REG_HORI_BLANKING_H, data);
	tmp = (data[0] & 0x0f) << 8;
	ret = gc2235_read_reg(client, GC2235_8BIT, REG_HORI_BLANKING_L, data);
	tmp += data[0];

	ret = gc2235_read_reg(client, GC2235_8BIT,  REG_SH_DELAY_H, data);
	if (ret)
		return ret;
	tmp += (data[0] & 0x03) << 8;

	ret = gc2235_read_reg(client, GC2235_8BIT,  REG_SH_DELAY_L, data);
	if (ret)
		return ret;
	tmp += data[0];

	buf->line_length_pck = (buf->output_width / 2 + tmp + 4) * 2;

	tmp = 0;
	ret = gc2235_read_reg(client, GC2235_8BIT,  REG_VERT_DUMMY_H, data);
	tmp = (data[0] & 0x1f) << 8;
	ret = gc2235_read_reg(client, GC2235_8BIT,  REG_VERT_DUMMY_L, data);
	tmp += data[0];
	buf->frame_length_lines = buf->output_height + tmp;

	buf->binning_factor_x = 1;
	buf->binning_factor_y = 1;

	return 0;
}

/* This returns the exposure time being used. This should only be used
   for filling in EXIF data, not for actual image processing. */
static int gc2235_q_exposure(struct v4l2_subdev *sd, s32 *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u16 coarse;
	u8 tmp;
	int ret;

	/* the fine integration time is currently not calculated */
	ret = gc2235_read_reg(client, GC2235_8BIT,
			       GC2235_REG_EXPO_COARSE, &tmp);
	if (ret != 0)
		return ret;

	coarse = (u16)((tmp & 0x1f) << 8);

	ret = gc2235_read_reg(client, GC2235_8BIT,
			       GC2235_REG_EXPO_COARSE + 1, &tmp);

	if (ret != 0)
		return ret;

	coarse += (u16)tmp;

	*value = coarse;

	return ret;
}

static enum v4l2_mbus_pixelcode
gc2235_translate_bayer_order(enum atomisp_bayer_order code)
{
	switch (code) {
	case atomisp_bayer_order_rggb:
		return V4L2_MBUS_FMT_SRGGB10_1X10;
	case atomisp_bayer_order_grbg:
		return V4L2_MBUS_FMT_SGRBG10_1X10;
	case atomisp_bayer_order_bggr:
		return V4L2_MBUS_FMT_SBGGR10_1X10;
	case atomisp_bayer_order_gbrg:
		return V4L2_MBUS_FMT_SGBRG10_1X10;
	}
	return 0;
}

static int gc2235_v_flip(struct v4l2_subdev *sd, s32 value)
{
	struct gc2235_device *dev = to_gc2235_sensor(sd);
	struct camera_mipi_info *gc2235_info = NULL;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	u8 val;

	val = g_flip;
	dev_dbg(&client->dev, "@%s++ %d, 0x17:0x%x\n", __func__, value, val);
	if (value) {
		val |= GC2235_VFLIP_BIT;
	} else {
		val &= ~GC2235_VFLIP_BIT;
	}
	ret = gc2235_write_reg(client, GC2235_8BIT,
		GC2235_IMG_ORIENTATION, val);
	if (ret)
		return ret;
	g_flip = val;
	gc2235_info = v4l2_get_subdev_hostdata(sd);
	if (gc2235_info) {
		val &= (GC2235_VFLIP_BIT|GC2235_HFLIP_BIT);
		gc2235_info->raw_bayer_order = gc2235_bayer_order_mapping[val];
		dev->format.code = gc2235_translate_bayer_order(
			gc2235_info->raw_bayer_order);
	}

	return 0;
}

static int gc2235_h_flip(struct v4l2_subdev *sd, s32 value)
{
	struct gc2235_device *dev = to_gc2235_sensor(sd);
	struct camera_mipi_info *gc2235_info = NULL;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	u8 val;

	val = g_flip;
	dev_dbg(&client->dev, "@%s++ %d, 0x17:0x%x\n", __func__, value, val);
	if (value){
		val |= GC2235_HFLIP_BIT;
	} else {
		val &= ~GC2235_HFLIP_BIT;
	}
	ret = gc2235_write_reg(client, GC2235_8BIT,
		GC2235_IMG_ORIENTATION, val);
	if (ret)
		return ret;

	g_flip = val;
	gc2235_info = v4l2_get_subdev_hostdata(sd);
	if (gc2235_info) {
		val &= (GC2235_VFLIP_BIT|GC2235_HFLIP_BIT);
		gc2235_info->raw_bayer_order = gc2235_bayer_order_mapping[val];
		dev->format.code = gc2235_translate_bayer_order(
		gc2235_info->raw_bayer_order);
	}

	return 0;
}

static int gc2235_g_focal(struct v4l2_subdev *sd, s32 *val)
{
	*val = (GC2235_FOCAL_LENGTH_NUM << 16) | GC2235_FOCAL_LENGTH_DEM;
	return 0;
}

static int gc2235_g_fnumber(struct v4l2_subdev *sd, s32 *val)
{
	/*const f number for gc2235*/
	*val = (GC2235_F_NUMBER_DEFAULT_NUM << 16) | GC2235_F_NUMBER_DEM;
	return 0;
}

static int gc2235_g_fnumber_range(struct v4l2_subdev *sd, s32 *val)
{
	*val = (GC2235_F_NUMBER_DEFAULT_NUM << 24) |
		(GC2235_F_NUMBER_DEM << 16) |
		(GC2235_F_NUMBER_DEFAULT_NUM << 8) | GC2235_F_NUMBER_DEM;
	return 0;
}

/* This returns the exposure time being used. This should only be used
   for filling in EXIF data, not for actual image processing. */
static int gc2235_g_exposure(struct v4l2_subdev *sd, s32 *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u16 coarse;
	u8 reg_val_h, reg_val_l;
	int ret;

	/* the fine integration time is currently not calculated */
	ret = gc2235_read_reg(client, GCSENSOR_8BIT,
			       GC2235_REG_EXPO_COARSE, &reg_val_h);
	if (ret)
		return ret;

	coarse = ((u16)(reg_val_h & 0x1f)) << 8;

	ret = gc2235_read_reg(client, GCSENSOR_8BIT,
			       GC2235_REG_EXPO_COARSE + 1, &reg_val_l);
	if (ret)
		return ret;

	coarse |= reg_val_l;

	*value = coarse;
	return 0;
}

struct gc2235_control gc2235_controls[] = {
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
		.query = gc2235_q_exposure,
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
		.tweak = gc2235_v_flip,
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
		.tweak = gc2235_h_flip,
	},
	{
		.qc = {
			.id = V4L2_CID_FOCAL_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focal length",
			.minimum = GC2235_FOCAL_LENGTH_DEFAULT,
			.maximum = GC2235_FOCAL_LENGTH_DEFAULT,
			.step = 0x01,
			.default_value = GC2235_FOCAL_LENGTH_DEFAULT,
			.flags = 0,
		},
		.query = gc2235_g_focal,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number",
			.minimum = GC2235_F_NUMBER_DEFAULT,
			.maximum = GC2235_F_NUMBER_DEFAULT,
			.step = 0x01,
			.default_value = GC2235_F_NUMBER_DEFAULT,
			.flags = 0,
		},
		.query = gc2235_g_fnumber,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_RANGE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number range",
			.minimum = GC2235_F_NUMBER_RANGE,
			.maximum =  GC2235_F_NUMBER_RANGE,
			.step = 0x01,
			.default_value = GC2235_F_NUMBER_RANGE,
			.flags = 0,
		},
		.query = gc2235_g_fnumber_range,
	},
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
		.query = gc2235_g_exposure,
	},
};

#define N_CONTROLS (ARRAY_SIZE(gc2235_controls))
static struct gc2235_control *gc2235_find_control(u32 id)
{
	int i;

	for (i = 0; i < N_CONTROLS; i++)
		if (gc2235_controls[i].qc.id == id)
			return &gc2235_controls[i];
	return NULL;
}

static int gc2235_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	struct gc2235_control *ctrl = gc2235_find_control(qc->id);
	struct gc2235_device *dev = to_gc2235_sensor(sd);

	if (ctrl == NULL)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	*qc = ctrl->qc;
	mutex_unlock(&dev->input_lock);

	return 0;
}

/* gc2235 control set/get */
static int gc2235_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct gc2235_control *s_ctrl;
	struct gc2235_device *dev = to_gc2235_sensor(sd);
	int ret;

	if (!ctrl)
		return -EINVAL;

	s_ctrl = gc2235_find_control(ctrl->id);
	if ((s_ctrl == NULL) || (s_ctrl->query == NULL))
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	ret = s_ctrl->query(sd, &ctrl->value);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int gc2235_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct gc2235_control *octrl = gc2235_find_control(ctrl->id);
	struct gc2235_device *dev = to_gc2235_sensor(sd);
	int ret;

	if ((octrl == NULL) || (octrl->tweak == NULL))
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	ret = octrl->tweak(sd, ctrl->value);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int get_resolution_index(struct v4l2_subdev *sd, int w, int h)
{
	int i;
	struct gc2235_device *dev = to_gc2235_sensor(sd);

	for (i = 0; i < dev->entries_curr_table; i++) {
		if (w != dev->curr_res_table[i].width)
			continue;
		if (h != dev->curr_res_table[i].height)
			continue;
		/* Found it */
		return i;
	}
	return -1;
}

static int gc2235_try_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	struct gc2235_device *dev = to_gc2235_sensor(sd);
	int idx = 0;
	const struct gc2235_resolution *tmp_res = NULL;

	mutex_lock(&dev->input_lock);

	if ((fmt->width > gc2235_max_res[0].res_max_width)
		|| (fmt->height > gc2235_max_res[0].res_max_height)) {
		fmt->width =  gc2235_max_res[0].res_max_width;
		fmt->height = gc2235_max_res[0].res_max_height;
	} else {
		for (idx = 0; idx < dev->entries_curr_table; idx++) {
				tmp_res = &dev->curr_res_table[idx];
				if ((tmp_res[idx].width >= fmt->width) &&
					(tmp_res[idx].height >= fmt->height))
					break;
		}

		/*
		 * nearest_resolution_index() doesn't return smaller
		 *  resolutions. If it fails, it means the requested
		 *  resolution is higher than wecan support. Fallback
		 *  to highest possible resolution in this case.
		 */
		if (idx == dev->entries_curr_table)
			idx = dev->entries_curr_table - 1;

		fmt->width = dev->curr_res_table[idx].width;
		fmt->height = dev->curr_res_table[idx].height;
	}

	fmt->code = dev->format.code;

	mutex_unlock(&dev->input_lock);
	return 0;
}

static int gc2235_s_mbus_fmt(struct v4l2_subdev *sd,
			      struct v4l2_mbus_framefmt *fmt)
{
	struct gc2235_device *dev = to_gc2235_sensor(sd);
	const struct gc2235_reg *gc2235_def_reg;
	struct camera_mipi_info *gc2235_info = NULL;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	u8 val;

	gc2235_info = v4l2_get_subdev_hostdata(sd);
	if (gc2235_info == NULL)
		return -EINVAL;

	ret = gc2235_try_mbus_fmt(sd, fmt);
	if (ret)
		return ret;

	mutex_lock(&dev->input_lock);

	dev->fmt_idx = get_resolution_index(sd, fmt->width, fmt->height);
	/* Sanity check */
	if (unlikely(dev->fmt_idx == -1)) {
		ret = -EINVAL;
		goto out;
	}

	gc2235_def_reg = dev->curr_res_table[dev->fmt_idx].regs;

	ret = gc2235_write_reg_array(client, gc2235_def_reg);
	if (ret)
		goto out;
	dev->fps = dev->curr_res_table[dev->fmt_idx].fps;
	dev->pixels_per_line =
		dev->curr_res_table[dev->fmt_idx].pixels_per_line;
	dev->lines_per_frame =
		dev->curr_res_table[dev->fmt_idx].lines_per_frame;
	ret = __gc2235_update_exposure_timing(client, dev->coarse_itg,
		dev->pixels_per_line, dev->lines_per_frame);
	if (ret)
		goto out;

	ret = gc2235_write_reg_array(client, gc2235_param_update);
	if (ret)
		goto out;

	ret = gc2235_get_intg_factor(client, gc2235_info, gc2235_def_reg);
	if (ret)
		goto out;

	ret = gc2235_read_reg(client, GC2235_8BIT, GC2235_IMG_ORIENTATION, &val);
	if (ret)
		goto out;
	val &= (GC2235_VFLIP_BIT|GC2235_HFLIP_BIT);
	gc2235_info->raw_bayer_order = gc2235_bayer_order_mapping[val];
	dev->format.code = gc2235_translate_bayer_order(
		gc2235_info->raw_bayer_order);
out:
	mutex_unlock(&dev->input_lock);
	return ret;
}

static int gc2235_g_mbus_fmt(struct v4l2_subdev *sd,
			      struct v4l2_mbus_framefmt *fmt)
{
	struct gc2235_device *dev = to_gc2235_sensor(sd);

	if (!fmt)
		return -EINVAL;

	fmt->width = dev->curr_res_table[dev->fmt_idx].width;
	fmt->height = dev->curr_res_table[dev->fmt_idx].height;
	fmt->code = dev->format.code;

	return 0;
}

static int gc2235_detect(struct i2c_client *client, u16 *id)
{
	struct i2c_adapter *adapter = client->adapter;
	u8 id_l, id_h;

	/* i2c check */
	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -ENODEV;

	/* check sensor chip ID	 */
	if (gc2235_read_reg(client, GCSENSOR_8BIT, GC2235_REG_SENSOR_ID_HIGH_BIT, &id_h)) {
		v4l2_err(client, "sensor id = 0x%x\n", id_h);
		return -ENODEV;
	}
	*id = (u16)(id_h << 0x8);
	if (gc2235_read_reg(client, GCSENSOR_8BIT, GC2235_REG_SENSOR_ID_LOW_BIT, &id_l)) {
		v4l2_err(client, "sensor_id = 0x%x\n", id_l);
		return -ENODEV;
	}
	*id = *id + id_l;

	if (*id == GC2235_ID)
		goto found;
	else {
		v4l2_err(client, "no gc2235 sensor found\n");
		return -ENODEV;
	}
found:
	v4l2_info(client, "gc2235_detect: sensor_id = 0x%x\n", *id);

	return 0;
}

/*
 * gc2235 stream on/off
 */
static int gc2235_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret;
	struct gc2235_device *dev = to_gc2235_sensor(sd);

	mutex_lock(&dev->input_lock);
	if (enable) {
		ret = gc2235_set_streaming(sd);
		dev->streaming = 1;
	} else {
		ret = gc2235_set_suspend(sd);
		dev->streaming = 0;
		dev->fps_index = 0;
	}
	mutex_unlock(&dev->input_lock);

	return 0;
}

/*
 * gc2235 enum frame size, frame intervals
 */
static int gc2235_enum_framesizes(struct v4l2_subdev *sd,
				   struct v4l2_frmsizeenum *fsize)
{
	unsigned int index = fsize->index;
	struct gc2235_device *dev = to_gc2235_sensor(sd);

	if (index >= dev->entries_curr_table)
		return -EINVAL;

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = dev->curr_res_table[index].width;
	fsize->discrete.height = dev->curr_res_table[index].height;
	fsize->reserved[0] = dev->curr_res_table[index].used;

	return 0;
}

static int gc2235_enum_frameintervals(struct v4l2_subdev *sd,
				       struct v4l2_frmivalenum *fival)
{
	int i;
	struct gc2235_device *dev = to_gc2235_sensor(sd);
	const struct gc2235_resolution *tmp_res = NULL;

	for (i = 0; i < dev->entries_curr_table; i++) {
			tmp_res = &dev->curr_res_table[i];
			if ((tmp_res[i].width >= fival->width) &&
			 (tmp_res[i].height >= fival->height))
				break;
	}

	if (i == dev->entries_curr_table)
		i--;

	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fival->width = dev->curr_res_table[i].width;
	fival->height = dev->curr_res_table[i].height;
	fival->discrete.numerator = 1;
	fival->discrete.denominator = dev->curr_res_table[i].fps;

	return 0;
}

static int gc2235_s_config(struct v4l2_subdev *sd,
			    int irq, void *pdata)
{
	struct gc2235_device *dev = to_gc2235_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u16 sensor_id;
	int ret;

	if (NULL == pdata)
		return -ENODEV;

	dev->platform_data =
		(struct camera_sensor_platform_data *)pdata;

	mutex_lock(&dev->input_lock);

	if (dev->platform_data->platform_init) {
		ret = dev->platform_data->platform_init(client);
		if (ret) {
			mutex_unlock(&dev->input_lock);
			dev_err(&client->dev, "gc2235 platform init err\n");
			return ret;
		}
	}

	ret = __gc2235_s_power(sd, 1);
	if (ret) {
		v4l2_err(client, "gc2235 power-up err.\n");
		goto  fail_csi_cfg;
	}

	/* config & detect sensor */
	ret = gc2235_detect(client, &sensor_id);
	if (ret) {
		v4l2_err(client, "gc2235_detect err s_config.\n");
		goto fail_detect;
	}

	ret = dev->platform_data->csi_cfg(sd, 1);
	if (ret)
		goto fail_csi_cfg;

	dev->sensor_id = sensor_id;

	/* power off sensor */
	ret = __gc2235_s_power(sd, 0);
	mutex_unlock(&dev->input_lock);
	if (ret)
		v4l2_err(client, "gc2235 power-down err.\n");

	return ret;

fail_detect:
	dev->platform_data->csi_cfg(sd, 0);
fail_csi_cfg:
	__gc2235_s_power(sd, 0);
	mutex_unlock(&dev->input_lock);
	dev_err(&client->dev, "sensor power-gating failed\n");
	return ret;
}

static int
gc2235_enum_mbus_code(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_mbus_code_enum *code)
{
	struct gc2235_device *dev = to_gc2235_sensor(sd);
	if (code->index >= MAX_FMTS)
		return -EINVAL;
	code->code = dev->format.code;
	return 0;
}

static int
gc2235_enum_frame_size(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			struct v4l2_subdev_frame_size_enum *fse)
{
	int index = fse->index;
	struct gc2235_device *dev = to_gc2235_sensor(sd);

	if (index >= dev->entries_curr_table)
		return -EINVAL;

	fse->min_width = dev->curr_res_table[index].width;
	fse->min_height = dev->curr_res_table[index].height;
	fse->max_width = dev->curr_res_table[index].width;
	fse->max_height = dev->curr_res_table[index].height;

	return 0;
}

static struct v4l2_mbus_framefmt *
__gc2235_get_pad_format(struct gc2235_device *sensor,
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
gc2235_get_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct gc2235_device *dev = to_gc2235_sensor(sd);
	struct v4l2_mbus_framefmt *format =
			__gc2235_get_pad_format(dev, fh, fmt->pad, fmt->which);

	fmt->format = *format;

	return 0;
}

static int
gc2235_set_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct gc2235_device *dev = to_gc2235_sensor(sd);

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		dev->format = fmt->format;

	return 0;
}

static int
gc2235_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct gc2235_device *dev = to_gc2235_sensor(sd);
	dev->run_mode = param->parm.capture.capturemode;

	mutex_lock(&dev->input_lock);
	switch (dev->run_mode) {
	case CI_MODE_VIDEO:
		dev->curr_res_table = dev->mode_tables->res_video;
		dev->entries_curr_table = dev->mode_tables->n_res_video;
		break;
	case CI_MODE_STILL_CAPTURE:
		dev->curr_res_table = dev->mode_tables->res_still;
		dev->entries_curr_table = dev->mode_tables->n_res_still;
		break;
	default:
		dev->curr_res_table = dev->mode_tables->res_preview;
		dev->entries_curr_table = dev->mode_tables->n_res_preview;
	}
	mutex_unlock(&dev->input_lock);
	return 0;
}

int
gc2235_g_frame_interval(struct v4l2_subdev *sd,
				struct v4l2_subdev_frame_interval *interval)
{
	struct gc2235_device *dev = to_gc2235_sensor(sd);
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
		if ((dev->coarse_itg + 4) < dev->coarse_itg) {
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

static int gc2235_g_skip_frames(struct v4l2_subdev *sd, u32 *frames)
{
	struct gc2235_device *dev = to_gc2235_sensor(sd);

	mutex_lock(&dev->input_lock);
	*frames = dev->curr_res_table[dev->fmt_idx].skip_frames;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static const struct v4l2_subdev_sensor_ops gc2235_sensor_ops = {
	.g_skip_frames	= gc2235_g_skip_frames,
};

static const struct v4l2_subdev_video_ops gc2235_video_ops = {
	.s_stream = gc2235_s_stream,
	.enum_framesizes = gc2235_enum_framesizes,
	.enum_frameintervals = gc2235_enum_frameintervals,
	.try_mbus_fmt = gc2235_try_mbus_fmt,
	.g_mbus_fmt = gc2235_g_mbus_fmt,
	.s_mbus_fmt = gc2235_s_mbus_fmt,
	.s_parm = gc2235_s_parm,
};

static const struct v4l2_subdev_core_ops gc2235_core_ops = {
	.queryctrl = gc2235_queryctrl,
	.g_ctrl = gc2235_g_ctrl,
	.s_ctrl = gc2235_s_ctrl,
#ifdef POWER_ALWAYS_ON_BEFORE_SUSPEND
	.s_power = gc2235_s_power_always_on,
#else
	.s_power = gc2235_s_power,
#endif
	.ioctl = gc2235_ioctl,
};

static const struct v4l2_subdev_pad_ops gc2235_pad_ops = {
	.enum_mbus_code = gc2235_enum_mbus_code,
	.enum_frame_size = gc2235_enum_frame_size,
	.get_fmt = gc2235_get_pad_format,
	.set_fmt = gc2235_set_pad_format,
};

static const struct v4l2_subdev_ops gc2235_ops = {
	.core = &gc2235_core_ops,
	.video = &gc2235_video_ops,
	.pad = &gc2235_pad_ops,
	.sensor = &gc2235_sensor_ops,
};

static int gc2235_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc2235_device *dev = to_gc2235_sensor(sd);

	if (dev->platform_data->platform_deinit)
		dev->platform_data->platform_deinit();

	v4l2_device_unregister_subdev(sd);
	media_entity_cleanup(&dev->sd.entity);
	kfree(dev);

	return 0;
}

static int __update_gc2235_device_settings(struct gc2235_device *dev, u16 sensor_id)
{
	switch (sensor_id) {
	case GC2235_ID:
		dev->mode_tables = &gc2235_sets[0];
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int gc2235_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct gc2235_device *dev;
	struct camera_mipi_info *gc2235_info = NULL;
	int ret = -1;
	void *pdata = client->dev.platform_data;

	/* allocate sensor device & init sub device */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		v4l2_err(client, "%s: out of memory\n", __func__);
		return -ENOMEM;
	}
	mutex_init(&dev->input_lock);

	dev->fmt_idx = 0;
	dev->sensor_id = GC2235_ID_DEFAULT;
	v4l2_i2c_subdev_init(&(dev->sd), client, &gc2235_ops);

	dev->once_launched = 0;

	if (ACPI_COMPANION(&client->dev))
		pdata = gmin_camera_platform_data(&dev->sd,
						  ATOMISP_INPUT_FORMAT_RAW_10,
						  atomisp_bayer_order_grbg);
	if (!pdata)
		goto out_free;

	ret = gc2235_s_config(&dev->sd, client->irq, pdata);
	if (ret)
		goto out_free;

	ret = atomisp_register_i2c_module(&dev->sd, pdata, RAW_CAMERA);
	if (ret)
		goto out_free;

	gc2235_info = v4l2_get_subdev_hostdata(&dev->sd);

	/*
	 * sd->name is updated with sensor driver name by the v4l2.
	 * change it to sensor name in this case.
	 */
	snprintf(dev->sd.name, sizeof(dev->sd.name), "%s%x %d-%04x",
		GC2235_SUBDEV_PREFIX, dev->sensor_id,
		i2c_adapter_id(client->adapter), client->addr);

	/* Resolution settings depend on sensor type and platform */
	ret = __update_gc2235_device_settings(dev, dev->sensor_id);
	if (ret)
		goto out_free;

	dev->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->pad.flags = MEDIA_PAD_FL_SOURCE;
	dev->format.code = V4L2_MBUS_FMT_SGRBG10_1X10;

	dev->sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;

	ret = media_entity_init(&dev->sd.entity, 1, &dev->pad, 0);
	if (ret)
		gc2235_remove(client);

	return ret;
out_free:
	v4l2_device_unregister_subdev(&dev->sd);
	kfree(dev);
	return ret;
}

#ifdef POWER_ALWAYS_ON_BEFORE_SUSPEND
static int gc2235_suspend(struct device *dev)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc2235_device *gc_dev = to_gc2235_sensor(sd);
	int ret = 0;

	//printk("%s() in\n", __func__);

	if (gc_dev->once_launched) {
		ret = __gc2235_s_power(sd, 0);
		if (ret) {
			v4l2_err(client, "gc2235 power-down err.\n");
		}
	}

	return 0;
}

static int gc2235_resume(struct device *dev)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc2235_device *gc_dev = to_gc2235_sensor(sd);
	int ret = 0;

	//printk("%s() in\n", __func__);

	if (gc_dev->once_launched) {
		ret = __gc2235_s_power(sd, 1);
		if (ret) {
			v4l2_err(client, "gc2235 power-up err.\n");
		}
	}

	return 0;
}

SIMPLE_DEV_PM_OPS(gc2235_pm_ops, gc2235_suspend, gc2235_resume);
#endif

static const struct i2c_device_id gc2235_ids[] = {
	{GC2235_NAME, GC2235_ID},
	{}
};

MODULE_DEVICE_TABLE(i2c, gc2235_ids);

static struct acpi_device_id gc2235_acpi_match[] = {
       {"INT33F8"},
       {},
};
MODULE_DEVICE_TABLE(acpi, gc2235_acpi_match);

static struct i2c_driver gc2235_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = GC2235_DRIVER,
		.acpi_match_table = ACPI_PTR(gc2235_acpi_match),
#ifdef POWER_ALWAYS_ON_BEFORE_SUSPEND
		.pm = &gc2235_pm_ops,
#endif
	},
	.probe = gc2235_probe,
	.remove = gc2235_remove,
	.id_table = gc2235_ids,
};

static __init int init_gc2235(void)
{
	return i2c_add_driver(&gc2235_driver);
}

static __exit void exit_gc2235(void)
{
	i2c_del_driver(&gc2235_driver);
}

module_init(init_gc2235);
module_exit(exit_gc2235);

MODULE_DESCRIPTION("A low-level driver for  sensors");
MODULE_AUTHOR("Kun Jiang <kunx.jiang@intel.com>");
MODULE_LICENSE("GPL");

