/*
 * Support for ov5640 Camera Sensor.
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

#include "ov5640.h"

#define to_ov5640_sensor(sd) container_of(sd, struct ov5640_device, sd)

static int
ov5640_read_reg(struct i2c_client *client, u16 data_length, u16 reg, u32 *val)
{
	int err;
	struct i2c_msg msg[2];
	unsigned char data[4];

	if (!client->adapter) {
		dev_err(&client->dev, "%s error, no client->adapter\n",
			__func__);
		return -ENODEV;
	}

	if (data_length != MISENSOR_8BIT && data_length != MISENSOR_16BIT
					 && data_length != MISENSOR_32BIT) {
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
	if (err < 0) {
		dev_err(&client->dev,
			"read from offset 0x%x error %d", reg, err);
		return err;
	}

	*val = 0;
	/* high byte comes first */
	if (data_length == MISENSOR_8BIT)
		*val = (u8)data[0];
	else if (data_length == MISENSOR_16BIT)
		*val = be16_to_cpu(*(u16 *)&data[0]);
	else
		*val = be32_to_cpu(*(u32 *)&data[0]);

	return 0;
}

static int
ov5640_write_reg(struct i2c_client *client, u16 data_length, u16 reg, u32 val)
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

	if (data_length != MISENSOR_8BIT && data_length != MISENSOR_16BIT
					 && data_length != MISENSOR_32BIT) {
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

	if (data_length == MISENSOR_8BIT) {
		data[2] = (u8)(val);
	} else if (data_length == MISENSOR_16BIT) {
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

static int ov5640_i2c_write(struct i2c_client *client, u16 len, u8 *data)
{
	struct i2c_msg msg;
	int ret;
	int retry = 0;

again:
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = len;
	msg.buf = data;

	ret = i2c_transfer(client->adapter, &msg, 1);

	/*
	 * It is said that Rev 2 sensor needs some delay here otherwise
	 * registers do not seem to load correctly. But tests show that
	 * removing the delay would not cause any in-stablility issue and the
	 * delay will cause serious performance down, so, removed previous
	 * mdelay(1) here.
	 */

	if (ret == 1)
		return 0;

	if (retry <= I2C_RETRY_COUNT) {
		dev_dbg(&client->dev, "retrying i2c write transfer... %d",
			retry);
		retry++;
		msleep(20);
		goto again;
	}

	return ret;
}

/*
 * __ov5640_flush_reg_array() is internal function to make writing reg
 * faster and should be not used anywhere else.
 */
static int __ov5640_flush_reg_array(struct i2c_client *client,
				     struct ov5640_write_ctrl *ctrl)
{
	u16 size;

	if (ctrl->index == 0)
		return 0;

	size = sizeof(u16) + ctrl->index; /* 16-bit address + data */
	ctrl->buffer.addr = cpu_to_be16(ctrl->buffer.addr);
	ctrl->index = 0;

	return ov5640_i2c_write(client, size, (u8 *)&ctrl->buffer);
}

/*
 * ov5640_write_reg_array - Initializes a list of MT9T111 registers
 * @client: i2c driver client structure
 * @reglist: list of registers to be written
 *
 * Initializes a list of MT9T111 registers. The list of registers is
 * terminated by MISENSOR_TOK_TERM.
 */
static int ov5640_write_reg_array(struct i2c_client *client,
			    const struct misensor_reg *reglist)
{
	const struct misensor_reg *next = reglist;
	int err;

	for (; next->length != MISENSOR_TOK_TERM; next++) {
		if (next->length == MISENSOR_TOK_DELAY) {
			msleep(next->val);
		} else {
			err = ov5640_write_reg(client, next->length, next->reg,
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

static const struct firmware *
load_firmware(struct device *dev)
{
	const struct firmware *fw;
	int rc;

	rc = request_firmware(&fw, AF_FW_PATH, dev);
	if (rc) {
		if (rc == -ENOENT)
			dev_err(dev, "Error AF firmware %s not found.\n",
					AF_FW_PATH);
		else
			dev_err(dev,
				"Error %d while requesting firmware %s\n",
				rc, AF_FW_PATH);
		return NULL;
	}

	return fw;
}

static int ov5640_af_init(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640_device *dev = to_ov5640_sensor(sd);
	const struct firmware *firmware;
	struct ov5640_write_ctrl ctrl;
	int err;
	int i;
	int group_length;

	/* reset MCU */
	err = ov5640_write_reg(client, MISENSOR_8BIT,
				OV5640_REG_SYS_RESET, OV5640_MCU_RESET);
	if (err)
		return err;

	/* download firmware */
	if (dev->firmware) {
		firmware = dev->firmware;
	} else {
		firmware = load_firmware(&client->dev);
		if (!firmware) {
			dev_err(&client->dev, "Load firmwares failed\n");
			return -EINVAL;
		}
		dev->firmware = firmware;
	}

	/* download firmware in group */
	group_length = (firmware->size) / (OV5640_MAX_WRITE_BUF_SIZE - 2);
	for (i = 0; i < group_length; i++) {
		ctrl.buffer.addr = OV5640_REG_FW_START
				    + i * (OV5640_MAX_WRITE_BUF_SIZE - 2);
		memcpy(ctrl.buffer.data,
			&firmware->data[i * (OV5640_MAX_WRITE_BUF_SIZE - 2)],
			(OV5640_MAX_WRITE_BUF_SIZE - 2));
		ctrl.index = (OV5640_MAX_WRITE_BUF_SIZE - 2);
		err = __ov5640_flush_reg_array(client, &ctrl);
		if (err) {
			dev_err(&client->dev, "write firmwares reg failed\n");
			return err;
		}
	}

	/* download firmware less than 1 group */
	ctrl.buffer.addr = OV5640_REG_FW_START +
				i * (OV5640_MAX_WRITE_BUF_SIZE - 2);
	memcpy(ctrl.buffer.data,
		&firmware->data[i * (OV5640_MAX_WRITE_BUF_SIZE - 2)],
		firmware->size - i * (OV5640_MAX_WRITE_BUF_SIZE - 2));
	ctrl.index = firmware->size - i * (OV5640_MAX_WRITE_BUF_SIZE - 2);
	err = __ov5640_flush_reg_array(client, &ctrl);
	if (err) {
		dev_err(&client->dev, "write firmwares reg failed\n");
		return err;
	}
	return ov5640_write_reg_array(client, ov5640_focus_init);
}

static int ov5640_s_focus_mode(struct v4l2_subdev *sd, int mode)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640_device *dev = to_ov5640_sensor(sd);
	int err = 0;

	/*
	 * if sensor streamoff, writing focus mode reg is invalid.
	 * only writing focus mode reg is valid after streamon.
	 */
	if (dev->streaming == false) {
		dev->focus_mode = mode;
		dev->focus_mode_change = true;
		return 0;
	}

	switch (mode) {
	case V4L2_CID_AUTO_FOCUS_START:
		/* start single focus */
		err = ov5640_write_reg(client, MISENSOR_8BIT,
						OV5640_REG_FOCUS_MODE,
						OV5640_SINGLE_FOCUS);
		break;
	case V4L2_CID_FOCUS_AUTO:
		/* start continuous focus */
		err = ov5640_write_reg(client, MISENSOR_8BIT,
						OV5640_REG_FOCUS_MODE,
						OV5640_CONTINUE_FOCUS);
		break;
	case V4L2_CID_3A_LOCK:
		/* pause focus */
		err = ov5640_write_reg(client, MISENSOR_8BIT,
						OV5640_REG_FOCUS_MODE,
						OV5640_PAUSE_FOCUS);
		break;
	case V4L2_CID_AUTO_FOCUS_STOP:
		/* release focus to infinity */
		err = ov5640_write_reg(client, MISENSOR_8BIT,
						OV5640_REG_FOCUS_MODE,
						OV5640_RELEASE_FOCUS);
		break;
	default:
		dev_err(&client->dev, "invalid mode.\n");
		return -EINVAL;
	}
	if (err) {
		dev_err(&client->dev, "setting focus mode fails.\n");
		return err;
	}

	dev->focus_mode = mode;
	dev->focus_mode_change = false;

	return 0;
}

static int ov5640_s_single_focus(struct v4l2_subdev *sd, s32 value)
{
	return ov5640_s_focus_mode(sd, V4L2_CID_AUTO_FOCUS_START);
}

static int ov5640_s_cont_focus(struct v4l2_subdev *sd, s32 value)
{
	return ov5640_s_focus_mode(sd, V4L2_CID_FOCUS_AUTO);
}

static int ov5640_pause_focus(struct v4l2_subdev *sd, s32 value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (value != V4L2_LOCK_FOCUS) {
		dev_err(&client->dev, "invalid focus cmd.\n");
		return -EINVAL;
	}

	return ov5640_s_focus_mode(sd, V4L2_CID_3A_LOCK);
}

static int ov5640_release_focus(struct v4l2_subdev *sd, s32 value)
{
	return ov5640_s_focus_mode(sd, V4L2_CID_AUTO_FOCUS_STOP);
}

static int ov5640_s_color_effect(struct v4l2_subdev *sd, int effect)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640_device *dev = to_ov5640_sensor(sd);
	int err = 0;

	if (dev->color_effect == effect)
		return 0;

	switch (effect) {
	case V4L2_COLORFX_NONE:
		err = ov5640_write_reg_array(client, ov5640_normal_effect);
		break;
	case V4L2_COLORFX_SEPIA:
		err = ov5640_write_reg_array(client, ov5640_sepia_effect);
		break;
	case V4L2_COLORFX_NEGATIVE:
		err = ov5640_write_reg_array(client, ov5640_negative_effect);
		break;
	case V4L2_COLORFX_BW:
		err = ov5640_write_reg_array(client, ov5640_bw_effect);
		break;
	case V4L2_COLORFX_SKY_BLUE:
		err = ov5640_write_reg_array(client, ov5640_blue_effect);
		break;
	case V4L2_COLORFX_GRASS_GREEN:
		err = ov5640_write_reg_array(client, ov5640_green_effect);
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

static int ov5640_g_color_effect(struct v4l2_subdev *sd, int *effect)
{
	struct ov5640_device *dev = to_ov5640_sensor(sd);

	*effect = dev->color_effect;

	return 0;
}

static int ov5640_g_image_brightness(struct v4l2_subdev *sd, int *brightness)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	/* get target image luminance average */
	return ov5640_read_reg(client, MISENSOR_8BIT,
					OV5640_REG_AE_AVERAGE, brightness);
}

static int ov5640_g_focus_status(struct v4l2_subdev *sd, int *status)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err;
	u32 val = 0;

	err = ov5640_read_reg(client, MISENSOR_8BIT,
					OV5640_REG_FOCUS_STATUS, &val);
	if (err)
		return err;

	switch (val & 0xff) {
	case OV5640_FOCUS_FW_DL:
		/* firmware is downloaded and not to be initialized */
		*status = V4L2_AUTO_FOCUS_STATUS_FAILED;
		break;
	case OV5640_FOCUS_FW_INIT:
		/* firmware is initializing */
	case OV5640_FOCUS_FW_IDLE:
		/* firmware is idle */
		*status = V4L2_AUTO_FOCUS_STATUS_IDLE;
		break;
	case OV5640_FOCUS_FW_RUN:
		/* focus is running */
		*status = V4L2_AUTO_FOCUS_STATUS_BUSY;
		break;
	case OV5640_FOCUS_FW_FINISH:
		/* focus is finished */
		*status = V4L2_AUTO_FOCUS_STATUS_REACHED;
		break;
	default:
		/*
		 * when focus is idle, the status value is variable,
		 * but it is different with status value above, and
		 * the value is 0x20 in most of cases.
		 */
		*status = V4L2_AUTO_FOCUS_STATUS_IDLE;
	}

	return 0;
}

/* calculate sysclk */
static int ov5640_get_sysclk(struct v4l2_subdev *sd, unsigned int *sysclk)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err;
	u32 temp1 = 0, temp2 = 0;
	u32 multiplier = 0, prediv = 0, vco = 0, sysdiv = 0;
	u32 pll_rdiv = 0, bit_div2x = 0, sclk_rdiv = 0;

	static int sclk_rdiv_map[] = {1, 2, 4, 8};

	err = ov5640_read_reg(client, MISENSOR_8BIT,
					OV5640_REG_PLL_CTRL_0, &temp1);
	if (err)
		return err;
	temp2 = temp1 & 0x0f;
	if (temp2 == 8 || temp2 == 10)
		bit_div2x = temp2 >> 1;

	err = ov5640_read_reg(client, MISENSOR_8BIT,
					OV5640_REG_PLL_CTRL_1, &temp1);
	if (err)
		return err;
	sysdiv = temp1 >> 4;
	if (sysdiv == 0)
		sysdiv = 16;

	err = ov5640_read_reg(client, MISENSOR_8BIT,
					OV5640_REG_PLL_CTRL_2, &temp1);
	if (err)
		return err;
	multiplier = temp1;

	err = ov5640_read_reg(client, MISENSOR_8BIT,
					OV5640_REG_PLL_CTRL_3, &temp1);
	if (err)
		return err;
	prediv = temp1 & 0x0f;
	pll_rdiv = ((temp1 >> 4) & 0x01) + 1;

	err = ov5640_read_reg(client, MISENSOR_8BIT,
					OV5640_REG_CLK_DIVIDER, &temp1);
	if (err)
		return err;
	temp2 = temp1 & 0x03;
	sclk_rdiv = sclk_rdiv_map[temp2];

	if ((prediv && sclk_rdiv && bit_div2x) == 0)
		return -EINVAL;

	vco = OV5640_XVCLK * multiplier / prediv;

	*sysclk = vco / sysdiv / pll_rdiv * 2 / bit_div2x / sclk_rdiv;

	if (*sysclk < MIN_SYSCLK)
		return -EINVAL;

	return 0;
}

/* read HTS from register settings */
static int ov5640_get_hts(struct v4l2_subdev *sd, unsigned int *hts)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err;

	err = ov5640_read_reg(client, MISENSOR_16BIT,
				OV5640_REG_TIMING_HTS, hts);
	if (err)
		return err;

	if (*hts < MIN_HTS)
		return -EINVAL;

	return 0;
}

/* read VTS from register settings */
static int ov5640_get_vts(struct v4l2_subdev *sd, unsigned int *vts)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err;

	err = ov5640_read_reg(client, MISENSOR_16BIT,
				OV5640_REG_TIMING_VTS, vts);
	if (err)
		return err;

	if (*vts < MIN_VTS)
		return -EINVAL;

	return 0;
}

/* write VTS to registers */
static int ov5640_set_vts(struct v4l2_subdev *sd, unsigned int vts)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return ov5640_write_reg(client, MISENSOR_16BIT,
					OV5640_REG_TIMING_VTS, vts);
}

/* read shutter, in number of line period */
static int ov5640_get_shutter(struct v4l2_subdev *sd, unsigned int *shutter)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err;
	u32 val, temp;

	err = ov5640_read_reg(client, MISENSOR_16BIT,
					OV5640_REG_EXPOSURE_0, &val);
	if (err)
		return err;
	temp = (val & 0x0fff);

	err = ov5640_read_reg(client, MISENSOR_8BIT,
					OV5640_REG_EXPOSURE_1, &val);
	if (err)
		return err;

	*shutter = (temp << 4) + (val >> 4);

	if (*shutter < MIN_SHUTTER)
		return -EINVAL;

	return 0;
}

/* write shutter, in number of line period */
static int ov5640_set_shutter(struct v4l2_subdev *sd, unsigned int shutter)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err;
	u32 temp;

	shutter = shutter & 0xffff;
	temp = shutter & 0x0f;
	temp = temp << 4;
	err = ov5640_write_reg(client, MISENSOR_8BIT,
					OV5640_REG_EXPOSURE_1, temp);
	if (err)
		return err;

	temp = shutter >> 4;
	return ov5640_write_reg(client, MISENSOR_16BIT,
					OV5640_REG_EXPOSURE_0, temp);
}

/* read gain, 16 = 1x */
static int ov5640_get_gain16(struct v4l2_subdev *sd, unsigned int *gain16)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err;
	u32 val;

	err = ov5640_read_reg(client, MISENSOR_16BIT,
					OV5640_REG_GAIN, &val);
	if (err)
		return err;

	*gain16 = val & 0x3ff;

	if (*gain16 < MIN_GAIN)
		return -EINVAL;

	return 0;
}

/* write gain, 16 = 1x */
static int ov5640_set_gain16(struct v4l2_subdev *sd, unsigned int gain16)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	gain16 = gain16 & 0x3ff;

	return ov5640_write_reg(client, MISENSOR_16BIT,
					OV5640_REG_GAIN, gain16);
}

/*
 * This returns the exposure compensation value, which is expressed in
 * terms of EV. The default EV value is 0, and driver don't support
 * adjust EV value.
 */
static int ov5640_get_exposure_bias(struct v4l2_subdev *sd, s32 *value)
{
	*value = 0;

	return 0;
}

/*
 * This returns ISO sensitivity.
 */
static int ov5640_get_iso(struct v4l2_subdev *sd, s32 *value)
{
	u32 gain;
	int err;

	err = ov5640_get_gain16(sd, &gain);
	if (err)
		return err;

	*value = gain / 16 * 100;

	return 0;
}

/* get banding filter value */
static int ov5640_get_light_frequency(struct v4l2_subdev *sd,
				unsigned int *light_frequency)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err;
	u32 temp;

	err = ov5640_read_reg(client, MISENSOR_8BIT,
					OV5640_REG_LIGHT_CTRL_0, &temp);
	if (err)
		return err;

	if (temp & OV5640_AUTO_BAND) {
		/* manual */
		err = ov5640_read_reg(client, MISENSOR_8BIT,
					OV5640_REG_LIGHT_CTRL_1, &temp);
		if (err)
			return err;
		if (temp & 0x04)
			/* 50Hz */
			*light_frequency = OV5640_LIGHT_50HZ;
		else
			/* 60Hz */
			*light_frequency = OV5640_LIGHT_60HZ;
	} else {
		/* auto */
		err = ov5640_read_reg(client, MISENSOR_8BIT,
					OV5640_REG_LIGHT_CTRL_2, &temp);
		if (err)
			return err;
		if (temp & 0x01)
			/* 50Hz */
			*light_frequency = OV5640_LIGHT_50HZ;
		else
			/* 60Hz */
			*light_frequency = OV5640_LIGHT_60HZ;
	}

	return 0;
}

static int ov5640_set_bandingfilter(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640_device *dev = to_ov5640_sensor(sd);
	u32 band_step60, max_band60, band_step50, max_band50;
	int err;

	/* read preview PCLK */
	err = ov5640_get_sysclk(sd, &dev->preview_sysclk);
	if (err)
		return err;

	/* read preview HTS */
	err = ov5640_get_hts(sd, &dev->preview_hts);
	if (err)
		return err;

	/* read preview VTS */
	err = ov5640_get_vts(sd, &dev->preview_vts);
	if (err)
		return err;

	/* calculate banding filter */
	/* 60Hz */
	band_step60 = dev->preview_sysclk * 100 / dev->preview_hts * 100 / 120;
	if (band_step60 == 0)
		return -EINVAL;

	err = ov5640_write_reg(client, MISENSOR_16BIT, OV5640_REG_B60_STEP,
								band_step60);
	if (err)
		return err;

	max_band60 = (dev->preview_vts - 4) / band_step60;
	err = ov5640_write_reg(client, MISENSOR_8BIT,
						OV5640_REG_B60_MAX, max_band60);
	if (err)
		return err;

	/* 50Hz */
	band_step50 = dev->preview_sysclk * 100 / dev->preview_hts;
	if (band_step50 == 0)
		return -EINVAL;

	err = ov5640_write_reg(client, MISENSOR_16BIT, OV5640_REG_B50_STEP,
								band_step50);
	if (err)
		return err;

	max_band50 = (dev->preview_vts - 4) / band_step50;
	return ov5640_write_reg(client, MISENSOR_8BIT,
						OV5640_REG_B50_MAX, max_band50);
}

/* stable in high */
static int ov5640_set_ae_target(struct v4l2_subdev *sd, unsigned int target)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640_device *dev = to_ov5640_sensor(sd);
	int err;
	u32 fast_high, fast_low;
	dev->ae_low = target * 23 / 25;	 /* 0.92 */
	dev->ae_high = target * 27 / 25; /* 1.08 */

	fast_high = dev->ae_high << 1;
	if (fast_high > 255)
		fast_high = 255;

	fast_low = dev->ae_low >> 1;

	err = ov5640_write_reg(client, MISENSOR_8BIT,
					OV5640_REG_AE_STAB_IN_H, dev->ae_high);
	if (err)
		return err;
	err = ov5640_write_reg(client, MISENSOR_8BIT,
					OV5640_REG_AE_STAB_IN_L, dev->ae_low);
	if (err)
		return err;
	err = ov5640_write_reg(client, MISENSOR_8BIT,
					OV5640_REG_AE_STAB_OUT_H, dev->ae_high);
	if (err)
		return err;
	err = ov5640_write_reg(client, MISENSOR_8BIT,
					OV5640_REG_AE_STAB_OUT_L, dev->ae_low);
	if (err)
		return err;
	err = ov5640_write_reg(client, MISENSOR_8BIT,
					OV5640_REG_AE_FAST_H, fast_high);
	if (err)
		return err;
	return ov5640_write_reg(client, MISENSOR_8BIT,
					OV5640_REG_AE_FAST_L, fast_low);
}

static int
ov5640_set_ag_ae(struct i2c_client *client, int enable)
{
	return ov5640_write_reg(client, MISENSOR_8BIT,
			OV5640_REG_AE_MODE_CTRL,
			enable ? OV5640_AUTO_AG_AE : OV5640_MANUAL_AG_AE);
}

static int ov5640_set_night_mode(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int temp;
	int err;

	err = ov5640_read_reg(client, MISENSOR_8BIT,
					OV5640_REG_AE_SYS_CTRL, &temp);
	if (err)
		return err;

	if (enable) {
		temp = temp | 0x04;
		return ov5640_write_reg(client, MISENSOR_8BIT,
					OV5640_REG_AE_SYS_CTRL, temp);
	} else {
		temp = temp & 0xfb;
		return ov5640_write_reg(client, MISENSOR_8BIT,
					OV5640_REG_AE_SYS_CTRL, temp);
	}
}

static int ov5640_set_awb_gain_mode(struct v4l2_subdev *sd, int mode)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int temp;
	int err;

	err = ov5640_read_reg(client, MISENSOR_8BIT,
					OV5640_REG_AWB_CTRL, &temp);
	if (err)
		return err;

	switch (mode) {
	case OV5640_AWB_GAIN_AUTO:
		/* set awb gain to auto */
		temp = temp & 0xfe;
		err = ov5640_write_reg(client, MISENSOR_8BIT,
						OV5640_REG_AWB_CTRL, temp);
		break;
	case OV5640_AWB_GAIN_MANUAL:
		/* set awb gain to manual */
		temp = temp | 0x01;
		err = ov5640_write_reg(client, MISENSOR_8BIT,
						OV5640_REG_AWB_CTRL, temp);
		break;
	default:
		dev_err(&client->dev, "invalid awb gain mode.\n");
		return -EINVAL;
	}

	return err;
}

static int ov5640_start_preview(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640_device *dev = to_ov5640_sensor(sd);
	int ret;

	dev->preview_ag_ae = false;

	ret = ov5640_set_awb_gain_mode(sd, OV5640_AWB_GAIN_AUTO);
	if (ret)
		return ret;

	ret = ov5640_set_gain16(sd, dev->preview_gain16);
	if (ret)
		return ret;

	ret = ov5640_set_shutter(sd, dev->preview_shutter);
	if (ret)
		return ret;

	ret = ov5640_set_ag_ae(client, 1);
	if (ret)
		return ret;

	ret = ov5640_set_bandingfilter(sd);
	if (ret)
		return ret;

	ret = ov5640_set_ae_target(sd, OV5640_AE_TARGET);
	if (ret)
		return ret;

	return ov5640_set_night_mode(sd, dev->night_mode);
}

static int ov5640_stop_preview(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640_device *dev = to_ov5640_sensor(sd);
	int err;

	/* read preview shutter */
	err = ov5640_get_shutter(sd, &dev->preview_shutter);
	if (err)
		return err;

	err = ov5640_get_gain16(sd, &dev->preview_gain16);
	if (err)
		return err;

	err = ov5640_get_hts(sd, &dev->preview_hts);
	if (err)
		return err;

	dev->preview_ag_ae = true;

	err = ov5640_set_awb_gain_mode(sd, OV5640_AWB_GAIN_MANUAL);
	if (err)
		return err;

	/* get average */
	return ov5640_read_reg(client, MISENSOR_8BIT,
				OV5640_REG_AE_AVERAGE, &dev->average);
}

static int ov5640_start_video(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err;

	err = ov5640_set_awb_gain_mode(sd, OV5640_AWB_GAIN_AUTO);
	if (err)
		return err;

	err = ov5640_set_ag_ae(client, 1);
	if (err)
		return err;

	err = ov5640_set_bandingfilter(sd);
	if (err)
		return err;

	err = ov5640_set_ae_target(sd, OV5640_AE_TARGET);
	if (err)
		return err;

	return ov5640_set_night_mode(sd, 0);
}

static int ov5640_start_capture(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640_device *dev = to_ov5640_sensor(sd);
	u32 capture_sysclk, capture_hts, capture_vts;
	u32 capture_shutter, capture_gain16;
	u32 light_frequency, capture_bandingfilter, capture_max_band;
	long capture_gain16_shutter;
	int err;

	if (!dev->preview_ag_ae) {
		dev_err(&client->dev, "preview gain and shutter are not available.\n");
		return -EINVAL;
	}

	err = ov5640_set_awb_gain_mode(sd, OV5640_AWB_GAIN_AUTO);
	if (err)
		return err;

	err = ov5640_set_ag_ae(client, 0);
	if (err)
		return err;

	/* read capture VTS */
	err = ov5640_get_vts(sd, &capture_vts);
	if (err)
		return err;

	err = ov5640_get_hts(sd, &capture_hts);
	if (err)
		return err;

	err = ov5640_get_sysclk(sd, &capture_sysclk);
	if (err)
		return err;

	/* calculate capture banding filter */
	err = ov5640_get_light_frequency(sd, &light_frequency);
	if (err)
		return err;

	if (light_frequency == OV5640_LIGHT_60HZ) {
		/* 60Hz */
		capture_bandingfilter = capture_sysclk * 100 /
						capture_hts * 100 / 120;
	} else {
		/* 50Hz */
		capture_bandingfilter = capture_sysclk * 100 / capture_hts;
	}

	if (capture_bandingfilter == 0)
		return -EINVAL;

	capture_max_band = (int)((capture_vts - 4) / capture_bandingfilter);
	if (capture_max_band == 0)
		return -EINVAL;

	/* calculate capture shutter/gain16 */
	if (dev->average > dev->ae_low && dev->average < dev->ae_high) {
		/* in stable range */
		capture_gain16_shutter = dev->preview_gain16 *
					dev->preview_shutter *
					capture_sysclk / dev->preview_sysclk *
					dev->preview_hts / capture_hts *
					OV5640_AE_TARGET / dev->average;
	} else {
		capture_gain16_shutter = dev->preview_gain16 *
					dev->preview_shutter *
					capture_sysclk / dev->preview_sysclk *
					dev->preview_hts / capture_hts;
	}
	/* gain to shutter */
	if (capture_gain16_shutter < (capture_bandingfilter * 16)) {
		/* shutter < 1/100 */
		capture_shutter = capture_gain16_shutter / 16;
		if (capture_shutter < 1)
			capture_shutter = 1;
		capture_gain16 = capture_gain16_shutter / capture_shutter;
		if (capture_gain16 < 16)
			capture_gain16 = 16;
	} else {
		if (capture_gain16_shutter >
			(capture_bandingfilter * capture_max_band * 16)) {
			/* exposure reach max */
			capture_shutter = capture_bandingfilter *
							capture_max_band;
			capture_gain16 = capture_gain16_shutter /
							capture_shutter;
		} else {
			/*
			 * 1/100 < capture_shutter =< max,
			 * capture_shutter = n/100
			 */
			capture_shutter = ((int)(capture_gain16_shutter / 16 /
						capture_bandingfilter)) *
						capture_bandingfilter;
			if (capture_shutter == 0)
				return -EINVAL;

			capture_gain16 = capture_gain16_shutter /
						capture_shutter;
		}
	}

	/* write capture gain */
	err = ov5640_set_gain16(sd, capture_gain16);
	if (err)
		return err;

	/* write capture shutter */
	if (capture_shutter > (capture_vts - 4)) {
		capture_vts = capture_shutter + 4;
		err = ov5640_set_vts(sd, capture_vts);
		if (err)
			return err;
	}

	return ov5640_set_shutter(sd, capture_shutter);
}

static int ov5640_standby(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return ov5640_write_reg_array(client, ov5640_standby_reg);
}

static int ov5640_wakeup(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return ov5640_write_reg_array(client, ov5640_wakeup_reg);
}

static int __ov5640_init(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = ov5640_write_reg_array(client, ov5640_init);
	if (ret)
		return ret;

	/*
	 * delay 5ms to wait for sensor initialization finish.
	 */
	usleep_range(5000, 6000);

	ret = ov5640_af_init(sd);
	if (ret)
		return ret;
	msleep(20);

	return ov5640_standby(sd);
}

static int power_up(struct v4l2_subdev *sd)
{
	struct ov5640_device *dev = to_ov5640_sensor(sd);
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
	if (ret)
		dev_err(&client->dev, "gpio failed\n");

	/*
	 * according to DS, 20ms is needed between power up and first i2c
	 * commend
	 */
	msleep(20);

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
	struct ov5640_device *dev = to_ov5640_sensor(sd);
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
		dev_err(&client->dev, "gpio failed\n");

	/* power control */
	ret = dev->platform_data->power_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "vprog failed.\n");

	/*according to DS, 20ms is needed after power down*/
	msleep(20);

	return ret;
}

static int ov5640_s_power(struct v4l2_subdev *sd, int power)
{
	if (power == 0)
		return power_down(sd);

	if (power_up(sd))
		return -EINVAL;

	return __ov5640_init(sd);
}

static int ov5640_try_res(u32 *w, u32 *h)
{
	int i;

	/*
	 * The mode list is in ascending order. We're done as soon as
	 * we have found the first equal or bigger size.
	 */
	for (i = 0; i < N_RES; i++) {
		if (ov5640_res[i].width >= *w &&
		    ov5640_res[i].height >= *h)
			break;
	}

	/*
	 * If no mode was found, it means we can provide only a smaller size.
	 * Returning the biggest one available in this case.
	 */
	if (i == N_RES)
		i--;

	*w = ov5640_res[i].width;
	*h = ov5640_res[i].height;

	return 0;
}

static struct ov5640_res_struct *ov5640_to_res(u32 w, u32 h)
{
	int  index;

	for (index = 0; index < N_RES; index++) {
		if (ov5640_res[index].width == w &&
		    ov5640_res[index].height == h)
			break;
	}

	/* No mode found */
	if (index >= N_RES)
		return NULL;

	return &ov5640_res[index];
}

static int ov5640_try_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	return ov5640_try_res(&fmt->width, &fmt->height);
}

static int ov5640_res2size(unsigned int res, int *h_size, int *v_size)
{
	unsigned short hsize;
	unsigned short vsize;

	switch (res) {
	case OV5640_RES_QVGA:
		hsize = OV5640_RES_QVGA_SIZE_H;
		vsize = OV5640_RES_QVGA_SIZE_V;
		break;
	case OV5640_RES_DVGA:
		hsize = OV5640_RES_DVGA_SIZE_H;
		vsize = OV5640_RES_DVGA_SIZE_V;
		break;
	case OV5640_RES_320P:
		hsize = OV5640_RES_320P_SIZE_H;
		vsize = OV5640_RES_320P_SIZE_V;
		break;
	case OV5640_RES_360P:
		hsize = OV5640_RES_360P_SIZE_H;
		vsize = OV5640_RES_360P_SIZE_V;
		break;
	case OV5640_RES_VGA:
		hsize = OV5640_RES_VGA_SIZE_H;
		vsize = OV5640_RES_VGA_SIZE_V;
		break;
	case OV5640_RES_480P:
		hsize = OV5640_RES_480P_SIZE_H;
		vsize = OV5640_RES_480P_SIZE_V;
		break;
	case OV5640_RES_720P:
		hsize = OV5640_RES_720P_SIZE_H;
		vsize = OV5640_RES_720P_SIZE_V;
		break;
	case OV5640_RES_1080P:
		hsize = OV5640_RES_1080P_SIZE_H;
		vsize = OV5640_RES_1080P_SIZE_V;
		break;
	case OV5640_RES_1088P:
		hsize = OV5640_RES_1088P_SIZE_H;
		vsize = OV5640_RES_1088P_SIZE_V;
		break;
	case OV5640_RES_D3M:
		hsize = OV5640_RES_D3M_SIZE_H;
		vsize = OV5640_RES_D3M_SIZE_V;
		break;
	case OV5640_RES_3M:
		hsize = OV5640_RES_3M_SIZE_H;
		vsize = OV5640_RES_3M_SIZE_V;
		break;
	case OV5640_RES_D5M:
		hsize = OV5640_RES_D5M_SIZE_H;
		vsize = OV5640_RES_D5M_SIZE_V;
		break;
	case OV5640_RES_5M:
		hsize = OV5640_RES_5M_SIZE_H;
		vsize = OV5640_RES_5M_SIZE_V;
		break;
	default:
		/* QVGA mode is still unsupported */
		WARN(1, "%s: Resolution 0x%08x unknown\n", __func__, res);
		return -EINVAL;
	}

	if (h_size != NULL)
		*h_size = hsize;
	if (v_size != NULL)
		*v_size = vsize;

	return 0;
}

static int ov5640_g_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	struct ov5640_device *dev = to_ov5640_sensor(sd);
	int width, height;
	int ret;

	ret = ov5640_res2size(dev->res, &width, &height);
	if (ret)
		return ret;
	fmt->width = width;
	fmt->height = height;

	return 0;
}

static int ov5640_s_mbus_fmt(struct v4l2_subdev *sd,
			      struct v4l2_mbus_framefmt *fmt)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	struct ov5640_device *dev = to_ov5640_sensor(sd);
	struct ov5640_res_struct *res_index;
	u32 width = fmt->width;
	u32 height = fmt->height;
	int ret;

	ov5640_try_res(&width, &height);
	res_index = ov5640_to_res(width, height);

	/* Sanity check */
	if (unlikely(!res_index)) {
		WARN_ON(1);
		return -EINVAL;
	}

	switch (res_index->res) {
	case OV5640_RES_QVGA:
		ret = ov5640_write_reg_array(c, ov5640_qvga_init);
		break;
	case OV5640_RES_DVGA:
		ret = ov5640_write_reg_array(c, ov5640_dvga_init);
		break;
	case OV5640_RES_320P:
		ret = ov5640_write_reg_array(c, ov5640_320p_init);
		break;
	case OV5640_RES_360P:
		ret = ov5640_write_reg_array(c, ov5640_360p_init);
		break;
	case OV5640_RES_VGA:
		ret = ov5640_write_reg_array(c, ov5640_vga_init);
		break;
	case OV5640_RES_480P:
		ret = ov5640_write_reg_array(c, ov5640_480p_init);
		break;
	case OV5640_RES_720P:
		ret = ov5640_write_reg_array(c, ov5640_720p_init);
		break;
	case OV5640_RES_1080P:
		ret = ov5640_write_reg_array(c, ov5640_1080p_init);
		break;
	case OV5640_RES_1088P:
		ret = ov5640_write_reg_array(c, ov5640_1088p_init);
		break;
	case OV5640_RES_D3M:
		ret = ov5640_write_reg_array(c, ov5640_D3M_init);
		break;
	case OV5640_RES_3M:
		ret = ov5640_write_reg_array(c, ov5640_3M_init);
		break;
	case OV5640_RES_D5M:
		ret = ov5640_write_reg_array(c, ov5640_D5M_init);
		break;
	case OV5640_RES_5M:
		ret = ov5640_write_reg_array(c, ov5640_5M_init);
		break;
	default:
		/* QVGA is not implemented yet */
		dev_err(&c->dev, "set resolution: %d failed!\n",
							res_index->res);
		return -EINVAL;
	}
	if (ret)
		return -EINVAL;

	if (dev->res != res_index->res) {
		int index;

		/*
		 * Marked current sensor res as being "used"
		 *
		 * REVISIT: We don't need to use an "used" field on each mode
		 * list entry to know which mode is selected. If this
		 * information is really necessary, how about to use a single
		 * variable on sensor dev struct?
		 */
		for (index = 0; index < N_RES; index++) {
			if (width == ov5640_res[index].width &&
			    height == ov5640_res[index].height) {
				ov5640_res[index].used = 1;
				continue;
			}
			ov5640_res[index].used = 0;
		}
	}

	/*
	 * ov5640 - we don't poll for context switch
	 * because it does not happen with streaming disabled.
	 */
	dev->res = res_index->res;

	fmt->width = width;
	fmt->height = height;

	/* relaunch default focus zone */
	ret = ov5640_write_reg(c, MISENSOR_8BIT,
					OV5640_REG_FOCUS_MODE,
					OV5640_RELAUNCH_FOCUS);
	if (ret)
		return -EINVAL;

	return ov5640_wakeup(sd);
}

static int ov5640_detect(struct i2c_client *client,  u16 *id, u8 *revision)
{
	struct i2c_adapter *adapter = client->adapter;
	u32 retvalue;

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "%s: i2c error", __func__);
		return -ENODEV;
	}

	if (ov5640_read_reg(client, MISENSOR_16BIT,
		OV5640_REG_PID, &retvalue)) {
		dev_err(&client->dev, "sensor_id_high = 0x%x\n", retvalue);
		return -ENODEV;
	}

	dev_info(&client->dev, "sensor_id = 0x%x\n", retvalue);
	if (retvalue != OV5640_MOD_ID) {
		dev_err(&client->dev, "%s: failed: client->addr = %x\n",
			__func__, client->addr);
		return -ENODEV;
	}

	/* REVISIT: HACK: Driver is currently forcing revision to 0 */
	*revision = 0;

	return 0;
}

static int
ov5640_s_config(struct v4l2_subdev *sd, int irq, void *platform_data)
{
	struct ov5640_device *dev = to_ov5640_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u8 sensor_revision;
	u16 sensor_id;
	int ret;

	if (NULL == platform_data)
		return -ENODEV;

	dev->platform_data =
	    (struct camera_sensor_platform_data *)platform_data;

	ret = ov5640_s_power(sd, 1);
	if (ret) {
		dev_err(&client->dev, "power_ctrl failed");
		return ret;
	}

	/* config & detect sensor */
	ret = ov5640_detect(client, &sensor_id, &sensor_revision);
	if (ret) {
		dev_err(&client->dev, "ov5640_detect err s_config.\n");
		goto fail_detect;
	}

	dev->sensor_id = sensor_id;
	dev->sensor_revision = sensor_revision;

	ret = dev->platform_data->csi_cfg(sd, 1);
	if (ret)
		goto fail_csi_cfg;

	ret = ov5640_s_power(sd, 0);
	if (ret)
		dev_err(&client->dev, "sensor power-gating failed\n");

	return ret;

fail_csi_cfg:
	dev->platform_data->csi_cfg(sd, 0);
fail_detect:
	ov5640_s_power(sd, 0);
	dev_err(&client->dev, "sensor power-gating failed\n");
	return ret;
}


static int ov5640_g_focal(struct v4l2_subdev *sd, s32 *val)
{
	*val = (OV5640_FOCAL_LENGTH_NUM << 16) | OV5640_FOCAL_LENGTH_DEM;
	return 0;
}

static int ov5640_g_fnumber(struct v4l2_subdev *sd, s32 *val)
{
	/* const f number for OV5640 */
	*val = (OV5640_F_NUMBER_DEFAULT_NUM << 16) | OV5640_F_NUMBER_DEM;
	return 0;
}

static int ov5640_g_fnumber_range(struct v4l2_subdev *sd, s32 *val)
{
	*val = (OV5640_F_NUMBER_DEFAULT_NUM << 24) |
		(OV5640_F_NUMBER_DEM << 16) |
		(OV5640_F_NUMBER_DEFAULT_NUM << 8) | OV5640_F_NUMBER_DEM;
	return 0;
}

static struct ov5640_control ov5640_controls[] = {
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
		.query = ov5640_get_shutter,
	},
	{
		.qc = {
			.id = V4L2_CID_AUTO_EXPOSURE_BIAS,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "exposure bias",
			.minimum = 0x0,
			.maximum = 0xffff,
			.step = 0x01,
			.default_value = 0x00,
			.flags = 0,
		},
		.query = ov5640_get_exposure_bias,
	},
	{
		.qc = {
			.id = V4L2_CID_ISO_SENSITIVITY,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "iso",
			.minimum = 0x0,
			.maximum = 0xffff,
			.step = 0x01,
			.default_value = 0x00,
			.flags = 0,
		},
		.query = ov5640_get_iso,
	},
	{
		.qc = {
			.id = V4L2_CID_AUTO_FOCUS_START,
			.type = V4L2_CTRL_TYPE_BUTTON,
			.name = "single focus",
			.minimum = 0,
			.maximum = 1,
			.step = 1,
			.default_value = 0,
		},
		.tweak = ov5640_s_single_focus,
	},
	{
		.qc = {
			.id = V4L2_CID_FOCUS_AUTO,
			.type = V4L2_CTRL_TYPE_BOOLEAN,
			.name = "continuous focus",
			.minimum = 0,
			.maximum = 1,
			.step = 1,
			.default_value = 0,
		},
		.tweak = ov5640_s_cont_focus,
	},
	{
		.qc = {
			.id = V4L2_CID_3A_LOCK,
			.type = V4L2_CTRL_TYPE_BITMASK,
			.name = "pause focus",
			.minimum = 0,
			.maximum = 1 << 2,
			.step = 1,
			.default_value = 0,
		},
		.tweak = ov5640_pause_focus,
	},
	{
		.qc = {
			.id = V4L2_CID_AUTO_FOCUS_STOP,
			.type = V4L2_CTRL_TYPE_BUTTON,
			.name = "release focus",
			.minimum = 0,
			.maximum = 1,
			.step = 1,
			.default_value = 0,
		},
		.tweak = ov5640_release_focus,
	},
	{
		.qc = {
			.id = V4L2_CID_AUTO_FOCUS_STATUS,
			.type = V4L2_CTRL_TYPE_BITMASK,
			.name = "focus status",
			.minimum = 0,
			.maximum = 0x07,
			.step = 1,
			.default_value = 0,
		},
		.query = ov5640_g_focus_status,
	},
	{
		.qc = {
			.id = V4L2_CID_BRIGHTNESS,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "target image luminance",
			.minimum = 0,
			.maximum = 255,
			.step = 1,
			.default_value = 0,
		},
		.query = ov5640_g_image_brightness,
	},
	{
		.qc = {
			.id = V4L2_CID_COLORFX,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "color effect",
			.minimum = 0,
			.maximum = 9,
			.step = 1,
			.default_value = 0,
		},
		.tweak = ov5640_s_color_effect,
		.query = ov5640_g_color_effect,
	},
	{
		.qc = {
			.id = V4L2_CID_FOCAL_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focal length",
			.minimum = 0,
			.maximum = OV5640_FOCAL_LENGTH_DEFAULT,
			.step = 0x01,
			.default_value = OV5640_FOCAL_LENGTH_DEFAULT,
			.flags = 0,
		},
		.query = ov5640_g_focal,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number",
			.minimum = 0,
			.maximum = OV5640_F_NUMBER_DEFAULT,
			.step = 0x01,
			.default_value = OV5640_F_NUMBER_DEFAULT,
			.flags = 0,
		},
		.query = ov5640_g_fnumber,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_RANGE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number range",
			.minimum = 0,
			.maximum =  OV5640_F_NUMBER_RANGE,
			.step = 0x01,
			.default_value = OV5640_F_NUMBER_RANGE,
			.flags = 0,
		},
		.query = ov5640_g_fnumber_range,
	},
};
#define N_CONTROLS (ARRAY_SIZE(ov5640_controls))

static struct ov5640_control *ov5640_find_control(__u32 id)
{
	int i;

	for (i = 0; i < N_CONTROLS; i++) {
		if (ov5640_controls[i].qc.id == id)
			return &ov5640_controls[i];
	}
	return NULL;
}

static int ov5640_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	struct ov5640_control *ctrl = ov5640_find_control(qc->id);

	if (ctrl == NULL)
		return -EINVAL;
	*qc = ctrl->qc;
	return 0;
}

static int ov5640_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct ov5640_control *octrl = ov5640_find_control(ctrl->id);
	int ret;

	if (octrl == NULL)
		return -EINVAL;

	ret = octrl->query(sd, &ctrl->value);
	if (ret < 0)
		return ret;

	return 0;
}

static int ov5640_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct ov5640_control *octrl = ov5640_find_control(ctrl->id);
	int ret;

	if (!octrl || !octrl->tweak)
		return -EINVAL;

	ret = octrl->tweak(sd, ctrl->value);
	if (ret < 0)
		return ret;

	return 0;
}

static int ov5640_mipi_stream(struct i2c_client *client, int enable)
{
	return ov5640_write_reg(client, MISENSOR_8BIT,
			OV5640_REG_FRAME_CTRL,
			enable ? OV5640_FRAME_START : OV5640_FRAME_STOP);
}

static int ov5640_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640_device *dev = to_ov5640_sensor(sd);
	int err;

	if (enable) {
		switch (dev->run_mode) {
		case CI_MODE_PREVIEW:
			err = ov5640_start_preview(sd);
			break;
		case CI_MODE_VIDEO:
			err = ov5640_start_video(sd);
			break;
		case CI_MODE_STILL_CAPTURE:
			err = ov5640_start_capture(sd);
			break;
		default:
			dev_err(&client->dev,
				"invalid run mode.\n");
			return -EINVAL;
		}
		if (err)
			dev_warn(&client->dev,
				"fail to start preview/video/capture.\n");

		err = ov5640_mipi_stream(client, enable);
		if (err)
			return err;

		dev->streaming = true;
		if (dev->focus_mode_change) {
			err = ov5640_s_focus_mode(sd, dev->focus_mode);
			if (err) {
				dev_err(&client->dev,
					"writing focus mode reg fails.\n");
				return err;
			}
			dev->focus_mode_change = false;
		}
	} else {
		if (dev->run_mode == CI_MODE_PREVIEW) {
			err = ov5640_stop_preview(sd);
			if (err)
				dev_warn(&client->dev,
					"fail to stop preview\n");
		}

		err = ov5640_mipi_stream(client, enable);
		if (err)
			return err;

		err = ov5640_standby(sd);
		if (err)
			return err;
		dev->streaming = false;
		dev->focus_mode = V4L2_CID_3A_LOCK;
	}

	return 0;
}

static int
ov5640_enum_framesizes(struct v4l2_subdev *sd, struct v4l2_frmsizeenum *fsize)
{
	unsigned int index = fsize->index;

	if (index >= N_RES)
		return -EINVAL;

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = ov5640_res[index].width;
	fsize->discrete.height = ov5640_res[index].height;

	fsize->reserved[0] = ov5640_res[index].used;

	return 0;
}

static int ov5640_enum_frameintervals(struct v4l2_subdev *sd,
				       struct v4l2_frmivalenum *fival)
{
	unsigned int index = fival->index;
	int i;

	if (index >= N_RES)
		return -EINVAL;

	/* find out the first equal or bigger size */
	for (i = 0; i < N_RES; i++) {
		if (ov5640_res[i].width >= fival->width &&
		    ov5640_res[i].height >= fival->height)
			break;
	}
	if (i == N_RES)
		i--;

	index = i;

	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fival->discrete.numerator = 1;
	fival->discrete.denominator = ov5640_res[index].fps;

	return 0;
}

static int
ov5640_g_chip_ident(struct v4l2_subdev *sd, struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_OV5640, 0);
}

static int ov5640_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_fh *fh,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index)
		return -EINVAL;
	code->code = V4L2_MBUS_FMT_SGRBG10_1X10;

	return 0;
}

static int ov5640_enum_frame_size(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh,
	struct v4l2_subdev_frame_size_enum *fse)
{
	unsigned int index = fse->index;

	if (index >= N_RES)
		return -EINVAL;

	fse->min_width = ov5640_res[index].width;
	fse->min_height = ov5640_res[index].height;
	fse->max_width = ov5640_res[index].width;
	fse->max_height = ov5640_res[index].height;

	return 0;
}

static int
ov5640_get_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct ov5640_device *snr = to_ov5640_sensor(sd);

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
ov5640_set_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct ov5640_device *snr = to_ov5640_sensor(sd);

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		snr->format = fmt->format;

	return 0;
}

/* set focus zone */
static int
ov5640_set_selection(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			     struct v4l2_subdev_selection *sel)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640_device *dev = to_ov5640_sensor(sd);
	int focus_width_step, focus_height_step;
	u32 x_center, y_center;
	int width, height;
	int err, index;

	if (sel->which != V4L2_SUBDEV_FORMAT_ACTIVE) {
		dev_err(&client->dev,
				"invalid selection format.\n");
		return -EINVAL;
	}

	if (sel->target != V4L2_SEL_TGT_COMPOSE) {
		dev_err(&client->dev,
				"invalid selection compose.\n");
		return -EINVAL;
	}

	for (index = 0; index < N_RES; index++) {
		if (dev->res == ov5640_res[index].res) {
			width = ov5640_res[index].width;
			height = ov5640_res[index].height;
			break;
		}
	}

	focus_width_step = width / OV5640_FOCUS_ZONE_ARRAY_WIDTH;
	focus_height_step = height / OV5640_FOCUS_ZONE_ARRAY_HEIGHT;

	/* calculate the center coordinate of selection rectangle */
	x_center = DIV_ROUND_UP((sel->r.left + sel->r.width / 2),
						focus_width_step);
	y_center = DIV_ROUND_UP((sel->r.top + sel->r.height / 2),
						focus_height_step);

	err = ov5640_write_reg(client, MISENSOR_8BIT,
					OV5640_REG_FOCUS_ZONE_X,
					x_center);
	if (err)
		return err;
	err = ov5640_write_reg(client, MISENSOR_8BIT,
					OV5640_REG_FOCUS_ZONE_Y,
					y_center);
	if (err)
		return err;
	err = ov5640_write_reg(client, MISENSOR_8BIT,
						OV5640_REG_FOCUS_MODE,
						OV5640_S_FOCUS_ZONE);
	if (err)
		return err;

	return ov5640_s_focus_mode(sd, dev->focus_mode);
}

static int
ov5640_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct ov5640_device *dev = to_ov5640_sensor(sd);

	if (param->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	memset(param, 0, sizeof(*param));
	param->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (dev->res >= 0 && dev->res < N_RES) {
		param->parm.capture.capability = V4L2_CAP_TIMEPERFRAME;
		param->parm.capture.timeperframe.numerator = 1;
		param->parm.capture.capturemode = dev->run_mode;
		param->parm.capture.timeperframe.denominator =
			ov5640_res[dev->res].fps;
	}
	return 0;
}

static int
ov5640_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct ov5640_device *dev = to_ov5640_sensor(sd);

	if (param->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	dev->run_mode = param->parm.capture.capturemode;
	return ov5640_g_parm(sd, param);
}

static int
ov5640_g_skip_frames(struct v4l2_subdev *sd, u32 *frames)
{
	int index;
	struct ov5640_device *snr = to_ov5640_sensor(sd);

	for (index = 0; index < N_RES; index++) {
		if (ov5640_res[index].res == snr->res) {
			*frames = ov5640_res[index].skip_frames;
			return 0;
		}
	}
	return -EINVAL;
}

static const struct v4l2_subdev_video_ops ov5640_video_ops = {
	.try_mbus_fmt = ov5640_try_mbus_fmt,
	.g_mbus_fmt = ov5640_g_mbus_fmt,
	.s_mbus_fmt = ov5640_s_mbus_fmt,
	.s_parm = ov5640_s_parm,
	.g_parm = ov5640_g_parm,
	.s_stream = ov5640_s_stream,
	.enum_framesizes = ov5640_enum_framesizes,
	.enum_frameintervals = ov5640_enum_frameintervals,
};

static const struct v4l2_subdev_sensor_ops ov5640_sensor_ops = {
	.g_skip_frames	= ov5640_g_skip_frames,
};

static const struct v4l2_subdev_core_ops ov5640_core_ops = {
	.g_chip_ident = ov5640_g_chip_ident,
	.queryctrl = ov5640_queryctrl,
	.g_ctrl = ov5640_g_ctrl,
	.s_ctrl = ov5640_s_ctrl,
	.s_power = ov5640_s_power,
};

static const struct v4l2_subdev_pad_ops ov5640_pad_ops = {
	.enum_mbus_code = ov5640_enum_mbus_code,
	.enum_frame_size = ov5640_enum_frame_size,
	.get_fmt = ov5640_get_pad_format,
	.set_fmt = ov5640_set_pad_format,
	.set_selection = ov5640_set_selection,
};

static const struct v4l2_subdev_ops ov5640_ops = {
	.core = &ov5640_core_ops,
	.video = &ov5640_video_ops,
	.sensor = &ov5640_sensor_ops,
	.pad = &ov5640_pad_ops,
};

static const struct media_entity_operations ov5640_entity_ops;

static int ov5640_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov5640_device *dev = container_of(sd,
					struct ov5640_device, sd);

	dev->platform_data->csi_cfg(sd, 0);

	release_firmware(dev->firmware);
	v4l2_device_unregister_subdev(sd);
	media_entity_cleanup(&dev->sd.entity);
	kfree(dev);

	return 0;
}

static int ov5640_probe(struct i2c_client *client,
		       const struct i2c_device_id *id)
{
	struct ov5640_device *dev;
	int ret;

	/* Setup sensor configuration structure */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&client->dev, "out of memory\n");
		return -ENOMEM;
	}

	v4l2_i2c_subdev_init(&dev->sd, client, &ov5640_ops);
	if (client->dev.platform_data) {
		ret = ov5640_s_config(&dev->sd, client->irq,
				       client->dev.platform_data);
		if (ret) {
			v4l2_device_unregister_subdev(&dev->sd);
			kfree(dev);
			return ret;
		}
	}
	/*TODO add format code here*/
	dev->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->pad.flags = MEDIA_PAD_FL_SOURCE;
	dev->sd.entity.ops = &ov5640_entity_ops;

	ret = media_entity_init(&dev->sd.entity, 1, &dev->pad, 0);
	if (ret) {
		ov5640_remove(client);
		return ret;
	}

	/* set res index to be invalid */
	dev->res = -1;

	/* set focus mode to be invalid */
	dev->focus_mode = -1;

	/* set color_effect to be invalid */
	dev->color_effect = -1;
	dev->preview_gain16 = OV5640_DEFAULT_GAIN;
	dev->preview_shutter = OV5640_DEFAULT_SHUTTER;

	return 0;
}

MODULE_DEVICE_TABLE(i2c, ov5640_id);
static struct i2c_driver ov5640_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = OV5640_NAME,
	},
	.probe = ov5640_probe,
	.remove = __exit_p(ov5640_remove),
	.id_table = ov5640_id,
};

static __init int ov5640_init_mod(void)
{
	return i2c_add_driver(&ov5640_driver);
}

static __exit void ov5640_exit_mod(void)
{
	i2c_del_driver(&ov5640_driver);
}

module_init(ov5640_init_mod);
module_exit(ov5640_exit_mod);

MODULE_DESCRIPTION("A low-level driver for Omnivision OV5640 sensors");
MODULE_LICENSE("GPL");
