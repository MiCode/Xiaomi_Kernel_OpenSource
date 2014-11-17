/*
 * Support for OmniVision ov8830 1080p HD camera sensor.
 *
 * Copyright (c) 2011 Intel Corporation. All Rights Reserved.
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
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <asm/intel-mid.h>

#include "ov8830.h"
#include "ov8835.h"

#define OV8830_BIN_FACTOR_MAX	2

#define to_ov8830_sensor(sd) container_of(sd, struct ov8830_device, sd)

static int
ov8830_read_reg(struct i2c_client *client, u16 len, u16 reg, u16 *val)
{
	struct i2c_msg msg[2];
	u16 data[OV8830_SHORT_MAX];
	int err, i;

	if (!client->adapter) {
		v4l2_err(client, "%s error, no client->adapter\n", __func__);
		return -ENODEV;
	}

	/* @len should be even when > 1 */
	if (len > OV8830_BYTE_MAX) {
		v4l2_err(client, "%s error, invalid data length\n", __func__);
		return -EINVAL;
	}

	memset(msg, 0, sizeof(msg));
	memset(data, 0, sizeof(data));

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
	if (err < 0)
		goto error;

	/* high byte comes first */
	if (len == OV8830_8BIT) {
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

static int ov8830_i2c_write(struct i2c_client *client, u16 len, u8 *data)
{
	struct i2c_msg msg;
	const int num_msg = 1;
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

	if (ret == num_msg)
		return 0;

	if (retry <= I2C_RETRY_COUNT) {
		dev_err(&client->dev, "retrying i2c write transfer... %d",
			retry);
		retry++;
		msleep(20);
		goto again;
	}

	return ret;
}

static int
ov8830_write_reg(struct i2c_client *client, u16 data_length, u16 reg, u16 val)
{
	int ret;
	unsigned char data[4] = {0};
	u16 *wreg;
	const u16 len = data_length + sizeof(u16); /* 16-bit address + data */

	if (!client->adapter) {
		v4l2_err(client, "%s error, no client->adapter\n", __func__);
		return -ENODEV;
	}

	if (data_length != OV8830_8BIT && data_length != OV8830_16BIT) {
		v4l2_err(client, "%s error, invalid data_length\n", __func__);
		return -EINVAL;
	}

	/* high byte goes out first */
	wreg = (u16 *)data;
	*wreg = cpu_to_be16(reg);

	if (data_length == OV8830_8BIT) {
		data[2] = (u8)(val);
	} else {
		/* OV8830_16BIT */
		u16 *wdata = (u16 *)&data[2];
		*wdata = be16_to_cpu(val);
	}

	ret = ov8830_i2c_write(client, len, data);
	if (ret)
		dev_err(&client->dev,
			"write error: wrote 0x%x to offset 0x%x error %d",
			val, reg, ret);

	return ret;
}


/*
 * ov8830_write_reg_array - Initializes a list of MT9M114 registers
 * @client: i2c driver client structure
 * @reglist: list of registers to be written
 *
 * This function initializes a list of registers. When consecutive addresses
 * are found in a row on the list, this function creates a buffer and sends
 * consecutive data in a single i2c_transfer().
 *
 * __ov8830_flush_reg_array, __ov8830_buf_reg_array() and
 * __ov8830_write_reg_is_consecutive() are internal functions to
 * ov8830_write_reg_array_fast() and should be not used anywhere else.
 *
 */

static int __ov8830_flush_reg_array(struct i2c_client *client,
				     struct ov8830_write_ctrl *ctrl)
{
	u16 size;

	if (ctrl->index == 0)
		return 0;

	size = sizeof(u16) + ctrl->index; /* 16-bit address + data */
	ctrl->buffer.addr = cpu_to_be16(ctrl->buffer.addr);
	ctrl->index = 0;

	return ov8830_i2c_write(client, size, (u8 *)&ctrl->buffer);
}

static int __ov8830_buf_reg_array(struct i2c_client *client,
				   struct ov8830_write_ctrl *ctrl,
				   const struct ov8830_reg *next)
{
	int size;
	u16 *data16;

	switch (next->type) {
	case OV8830_8BIT:
		size = 1;
		ctrl->buffer.data[ctrl->index] = (u8)next->val;
		break;
	case OV8830_16BIT:
		size = 2;
		data16 = (u16 *)&ctrl->buffer.data[ctrl->index];
		*data16 = cpu_to_be16((u16)next->val);
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
	if (ctrl->index + sizeof(u16) >= OV8830_MAX_WRITE_BUF_SIZE)
		__ov8830_flush_reg_array(client, ctrl);

	return 0;
}

static int
__ov8830_write_reg_is_consecutive(struct i2c_client *client,
				   struct ov8830_write_ctrl *ctrl,
				   const struct ov8830_reg *next)
{
	if (ctrl->index == 0)
		return 1;

	return ctrl->buffer.addr + ctrl->index == next->reg.sreg;
}

static int ov8830_write_reg_array(struct i2c_client *client,
				   const struct ov8830_reg *reglist)
{
	const struct ov8830_reg *next = reglist;
	struct ov8830_write_ctrl ctrl;
	int err;

	ctrl.index = 0;
	for (; next->type != OV8830_TOK_TERM; next++) {
		switch (next->type & OV8830_TOK_MASK) {
		case OV8830_TOK_DELAY:
			err = __ov8830_flush_reg_array(client, &ctrl);
			if (err)
				return err;
			msleep(next->val);
			break;

		default:
			/*
			 * If next address is not consecutive, data needs to be
			 * flushed before proceed.
			 */
			if (!__ov8830_write_reg_is_consecutive(client, &ctrl,
								next)) {
				err = __ov8830_flush_reg_array(client, &ctrl);
				if (err)
					return err;
			}
			err = __ov8830_buf_reg_array(client, &ctrl, next);
			if (err) {
				v4l2_err(client, "%s: write error, aborted\n",
					 __func__);
				return err;
			}
			break;
		}
	}

	return __ov8830_flush_reg_array(client, &ctrl);
}

static int drv201_write8(struct v4l2_subdev *sd, int reg, int val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct drv201_device *dev = to_drv201_device(sd);
	struct i2c_msg msg;

	memset(&msg, 0 , sizeof(msg));
	msg.addr = DRV201_I2C_ADDR;
	msg.len = 2;
	msg.buf = dev->buffer;
	msg.buf[0] = reg;
	msg.buf[1] = val;

	return i2c_transfer(client->adapter, &msg, 1);
}

static int drv201_write16(struct v4l2_subdev *sd, int reg, int val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct drv201_device *dev = to_drv201_device(sd);
	struct i2c_msg msg;

	memset(&msg, 0 , sizeof(msg));
	msg.addr = DRV201_I2C_ADDR;
	msg.len = 3;
	msg.buf = dev->buffer;
	msg.buf[0] = reg;
	msg.buf[1] = val >> 8;
	msg.buf[2] = val & 0xFF;

	return i2c_transfer(client->adapter, &msg, 1);
}

static int drv201_read8(struct v4l2_subdev *sd, int reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct drv201_device *dev = to_drv201_device(sd);
	struct i2c_msg msg[2];
	int r;

	memset(msg, 0 , sizeof(msg));
	msg[0].addr = DRV201_I2C_ADDR;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = dev->buffer;
	msg[0].buf[0] = reg;

	msg[1].addr = DRV201_I2C_ADDR;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = dev->buffer;

	r = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
	if (r != ARRAY_SIZE(msg))
		return -EIO;

	return dev->buffer[0];
}

static int drv201_init(struct v4l2_subdev *sd)
{
	struct drv201_device *dev = to_drv201_device(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	dev->platform_data = camera_get_af_platform_data();
	if (!dev->platform_data) {
		v4l2_err(client, "failed to get platform data\n");
		return -ENXIO;
	}
	return 0;
}

static int drv201_power_up(struct v4l2_subdev *sd)
{
	/* Transition time required from shutdown to standby state */
	const int WAKEUP_DELAY_US = 100;
	const int DEFAULT_CONTROL_VAL = 0x02;

	struct drv201_device *dev = to_drv201_device(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int r;

	/* Enable power */
	r = dev->platform_data->power_ctrl(sd, 1);
	if (r)
		return r;

	udelay(1);		/* Wait for VBAT to stabilize */

	/* jiggle SCL pin to wake up device */
	drv201_write8(sd, DRV201_CONTROL, 1);

	usleep_range(WAKEUP_DELAY_US, WAKEUP_DELAY_US * 10);

	/* Reset device */
	r = drv201_write8(sd, DRV201_CONTROL, 1);
	if (r < 0)
		goto fail_powerdown;

	/* Detect device */
	r = drv201_read8(sd, DRV201_CONTROL);
	if (r < 0)
		goto fail_powerdown;
	if (r != DEFAULT_CONTROL_VAL) {
		r = -ENXIO;
		goto fail_powerdown;
	}

	/* Use the liner mode to reduce the noise */
	r = drv201_write8(sd, DRV201_MODE, DRV201_MODE_LINEAR);
	if (r < 0)
		goto fail_powerdown;

	/* VCM RESONANCE FREQUENCY REGISTER (VCM_FREQ) */
	r = drv201_write8(sd, DRV201_VCM_FREQ, DRV201_DEFAULT_VCM_FREQ);
	if (r < 0)
		goto fail_powerdown;

	dev->focus = DRV201_MAX_FOCUS_POS;
	dev->initialized = true;

	v4l2_info(client, "detected drv201\n");
	return 0;

fail_powerdown:
	dev->platform_data->power_ctrl(sd, 0);
	return r;
}

static int drv201_power_down(struct v4l2_subdev *sd)
{
	struct drv201_device *dev = to_drv201_device(sd);

	return dev->platform_data->power_ctrl(sd, 0);
}

static int drv201_t_focus_abs(struct v4l2_subdev *sd, s32 value)
{
	struct drv201_device *dev = to_drv201_device(sd);
	int r;

	if (!dev->initialized)
		return -ENODEV;

	value = clamp(value, 0, DRV201_MAX_FOCUS_POS);
	r = drv201_write16(sd, DRV201_VCM_CURRENT, value);
	if (r < 0)
		return r;

	getnstimeofday(&dev->focus_time);
	dev->focus = value;
	return 0;
}

/* Start group hold for the following register writes */
static int ov8830_grouphold_start(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	const int group = 0;

	return ov8830_write_reg(client, OV8830_8BIT,
				OV8830_GROUP_ACCESS,
				group | OV8830_GROUP_ACCESS_HOLD_START);
}

/* End group hold and delay launch it */
static int ov8830_grouphold_launch(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	const int group = 0;
	int ret;

	/* End group */
	ret = ov8830_write_reg(client, OV8830_8BIT,
			       OV8830_GROUP_ACCESS,
			       group | OV8830_GROUP_ACCESS_HOLD_END);
	if (ret)
		return ret;

	/* Delay launch group (during next vertical blanking) */
	return ov8830_write_reg(client, OV8830_8BIT,
				OV8830_GROUP_ACCESS,
				group | OV8830_GROUP_ACCESS_DELAY_LAUNCH);
}

/*
 * Read EEPROM data from the le24l042cs chip and store
 * it into a kmalloced buffer. On error return NULL.
 * The caller must kfree the buffer when no more needed.
 * @size: set to the size of the returned EEPROM data.
 */
static void *le24l042cs_read(struct i2c_client *client, u32 *size)
{
	static const unsigned int LE24L042CS_I2C_ADDR = 0xA0 >> 1;
	static const unsigned int LE24L042CS_EEPROM_SIZE = 512;
	static const unsigned int MAX_READ_SIZE = OV8830_MAX_WRITE_BUF_SIZE;
	struct i2c_msg msg[2];
	int addr;
	char *buffer;

	buffer = kmalloc(LE24L042CS_EEPROM_SIZE, GFP_KERNEL);
	if (!buffer)
		return NULL;

	memset(msg, 0, sizeof(msg));
	for (addr = 0; addr < LE24L042CS_EEPROM_SIZE; addr += MAX_READ_SIZE) {
		unsigned int i2c_addr = LE24L042CS_I2C_ADDR;
		unsigned char addr_buf;
		int r;

		i2c_addr |= (addr >> 8) & 1;
		addr_buf = addr & 0xFF;

		msg[0].addr = i2c_addr;
		msg[0].flags = 0;
		msg[0].len = 1;
		msg[0].buf = &addr_buf;

		msg[1].addr = i2c_addr;
		msg[1].flags = I2C_M_RD;
		msg[1].len = min(MAX_READ_SIZE, LE24L042CS_EEPROM_SIZE - addr);
		msg[1].buf = &buffer[addr];

		r = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
		if (r != ARRAY_SIZE(msg)) {
			kfree(buffer);
			dev_err(&client->dev, "read failed at 0x%03x\n", addr);
			return NULL;
		}
	}

	if (size)
		*size = LE24L042CS_EEPROM_SIZE;
	return buffer;
}

static int ov8830_g_priv_int_data(struct v4l2_subdev *sd,
				  struct v4l2_private_int_data *priv)
{
	u32 size;
	void *b = le24l042cs_read(v4l2_get_subdevdata(sd), &size);
	int r = 0;

	if (!b)
		return -EIO;

	if (copy_to_user(priv->data, b, min_t(__u32, priv->size, size)))
		r = -EFAULT;

	priv->size = size;
	kfree(b);

	return r;
}

static int __ov8830_get_max_fps_index(
				const struct ov8830_fps_setting *fps_settings)
{
	int i;

	for (i = 0; i < MAX_FPS_OPTIONS_SUPPORTED; i++) {
		if (fps_settings[i].fps == 0)
			break;
	}

	return i - 1;
}

static int __ov8830_update_frame_timing(struct v4l2_subdev *sd, int exposure,
			u16 *hts, u16 *vts)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	/* Increase the VTS to match exposure + 14 */
	if (exposure > *vts - OV8830_INTEGRATION_TIME_MARGIN)
		*vts = (u16) exposure + OV8830_INTEGRATION_TIME_MARGIN;

	ret = ov8830_write_reg(client, OV8830_16BIT, OV8830_TIMING_HTS, *hts);
	if (ret)
		return ret;

	return ov8830_write_reg(client, OV8830_16BIT, OV8830_TIMING_VTS, *vts);
}

static int __ov8830_set_exposure(struct v4l2_subdev *sd, int exposure, int gain,
			int dig_gain, u16 *hts, u16 *vts)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int exp_val, ret;

	dev_dbg(&client->dev, "set exposure:0x%x, gain:0x%x, dig_gain:0x%x",
		exposure,
		gain,
		dig_gain);

	if (!(exposure && gain)) {
		return 0;
	}

	/* Update frame timings. Expsure must be minimum <  vts-14 */
	ret = __ov8830_update_frame_timing(sd, exposure, hts, vts);
	if (ret)
		return ret;

	/* For OV8835, the low 4 bits are fraction bits and must be kept 0 */
	exp_val = exposure << 4;
	ret = ov8830_write_reg(client, OV8830_8BIT,
			       OV8830_LONG_EXPO+2, exp_val & 0xFF);
	if (ret)
		return ret;

	ret = ov8830_write_reg(client, OV8830_8BIT,
			       OV8830_LONG_EXPO+1, (exp_val >> 8) & 0xFF);
	if (ret)
		return ret;

	ret = ov8830_write_reg(client, OV8830_8BIT,
			       OV8830_LONG_EXPO, (exp_val >> 16) & 0x0F);
	if (ret)
		return ret;

	/* Digital gain : to all MWB channel gains */
	if (dig_gain) {
		ret = ov8830_write_reg(client, OV8830_16BIT,
				OV8830_MWB_RED_GAIN_H, dig_gain);
		if (ret)
			return ret;

		ret = ov8830_write_reg(client, OV8830_16BIT,
				OV8830_MWB_GREEN_GAIN_H, dig_gain);
		if (ret)
			return ret;

		ret = ov8830_write_reg(client, OV8830_16BIT,
				OV8830_MWB_BLUE_GAIN_H, dig_gain);
		if (ret)
			return ret;
	}

	/* set global gain */
	return ov8830_write_reg(client, OV8830_8BIT, OV8830_AGC_ADJ, gain);
}

static int ov8830_set_exposure(struct v4l2_subdev *sd, int exposure, int gain,
				int dig_gain)
{
	struct ov8830_device *dev = to_ov8830_sensor(sd);
	const struct ov8830_resolution *res;
	u16 hts, vts;
	int ret;

	mutex_lock(&dev->input_lock);

	/* Validate exposure:  cannot exceed 16bit value */
	exposure = clamp_t(int, exposure, 0, OV8830_MAX_EXPOSURE_VALUE);

	/* Validate gain: must not exceed maximum 8bit value */
	gain = clamp_t(int, gain, 0, OV8830_MAX_GAIN_VALUE);

	/* Validate digital gain: must not exceed 12 bit value*/
	dig_gain = clamp_t(int, dig_gain, 0, OV8830_MWB_GAIN_MAX);

	/* Group hold is valid only if sensor is streaming. */
	if (dev->streaming) {
		ret = ov8830_grouphold_start(sd);
		if (ret)
			goto out;
	}

	res = &dev->curr_res_table[dev->fmt_idx];
	hts = res->fps_options[dev->fps_index].pixels_per_line;
	vts = res->fps_options[dev->fps_index].lines_per_frame;

	ret = __ov8830_set_exposure(sd, exposure, gain, dig_gain, &hts, &vts);
	if (ret)
		goto out;

	/* Updated the device variable. These are the current values. */
	dev->gain = gain;
	dev->exposure = exposure;
	dev->digital_gain = dig_gain;

out:
	/* Group hold launch - delayed launch */
	if (dev->streaming)
		ret = ov8830_grouphold_launch(sd);

	mutex_unlock(&dev->input_lock);

	return ret;
}

static int ov8830_s_exposure(struct v4l2_subdev *sd,
			      struct atomisp_exposure *exposure)
{
	return ov8830_set_exposure(sd, exposure->integration_time[0],
				exposure->gain[0], exposure->gain[1]);
}

static long ov8830_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	switch (cmd) {
	case ATOMISP_IOC_S_EXPOSURE:
		return ov8830_s_exposure(sd, (struct atomisp_exposure *)arg);
	case ATOMISP_IOC_G_SENSOR_PRIV_INT_DATA:
		return ov8830_g_priv_int_data(sd, arg);
	default:
		dev_err(&client->dev, "%s: invalid ioctl cmd\n", __func__);
		return -EINVAL;
	}
	return 0;
}

static int ov8830_init_registers(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov8830_device *dev = to_ov8830_sensor(sd);

	if (dev->sensor_id == OV8835_CHIP_ID)
		dev->basic_settings_list = ov8835_basic_settings;
	else
		dev->basic_settings_list = ov8830_BasicSettings;

	return ov8830_write_reg_array(client, dev->basic_settings_list);
}

static int ov8830_init(struct v4l2_subdev *sd, u32 val)
{
	struct ov8830_device *dev = to_ov8830_sensor(sd);
	int ret;

	mutex_lock(&dev->input_lock);
	ret = ov8830_init_registers(sd);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static void ov8830_uninit(struct v4l2_subdev *sd)
{
	struct ov8830_device *dev = to_ov8830_sensor(sd);

	dev->exposure = 0;
	dev->gain     = 0;
	dev->digital_gain = 0;
}

static int power_up(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov8830_device *dev = to_ov8830_sensor(sd);
	int ret;

	/* Enable power */
	ret = dev->platform_data->power_ctrl(sd, 1);
	if (ret)
		goto fail_power;

	/* Release reset */
	ret = dev->platform_data->gpio_ctrl(sd, 1);
	if (ret)
		dev_err(&client->dev, "gpio failed 1\n");

	/* Enable clock */
	ret = dev->platform_data->flisclk_ctrl(sd, 1);
	if (ret)
		goto fail_clk;

	/* Minumum delay is 8192 clock cycles before first i2c transaction,
	 * which is 1.37 ms at the lowest allowed clock rate 6 MHz */
	msleep(2);
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
	struct ov8830_device *dev = to_ov8830_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

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

static int __ov8830_s_power(struct v4l2_subdev *sd, int on)
{
	struct ov8830_device *dev = to_ov8830_sensor(sd);
	int ret, r;

	if (on == 0) {
		ov8830_uninit(sd);
		ret = power_down(sd);
		r = drv201_power_down(sd);
		if (ret == 0)
			ret = r;
		dev->power = 0;
	} else {
		ret = power_up(sd);
		if (ret)
			return ret;
		ret = drv201_power_up(sd);
		if (ret) {
			power_down(sd);
			return ret;
		}

		dev->power = 1;

		/* Initalise sensor settings */
		ret = ov8830_init_registers(sd);
	}

	return ret;
}

static int ov8830_s_power(struct v4l2_subdev *sd, int on)
{
	int ret;
	struct ov8830_device *dev = to_ov8830_sensor(sd);

	mutex_lock(&dev->input_lock);
	ret = __ov8830_s_power(sd, on);
	mutex_unlock(&dev->input_lock);

	/*
	 * FIXME: Compatibility with old behaviour: return to preview
	 * when the device is power cycled.
	 */
	if (!ret && on)
		v4l2_ctrl_s_ctrl(dev->run_mode, ATOMISP_RUN_MODE_PREVIEW);

	return ret;
}

static int ov8830_g_chip_ident(struct v4l2_subdev *sd,
				struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_OV8830, 0);

	return 0;
}

/* Return value of the specified register, first try getting it from
 * the register list and if not found, get from the sensor via i2c.
 */
static int ov8830_get_register(struct v4l2_subdev *sd, int reg,
			       const struct ov8830_reg *reglist)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	const struct ov8830_reg *next;
	u16 val;

	/* Try if the values is in the register list */
	for (next = reglist; next->type != OV8830_TOK_TERM; next++) {
		if (next->type != OV8830_8BIT) {
			v4l2_err(sd, "only 8-bit registers supported\n");
			return -ENXIO;
		}
		if (next->reg.sreg == reg)
			return next->val;
	}

	/* If not, read from sensor */
	if (ov8830_read_reg(client, OV8830_8BIT, reg, &val)) {
		v4l2_err(sd, "failed to read register 0x%04X\n", reg);
		return -EIO;
	}

	return val;
}

static int ov8830_get_register_16bit(struct v4l2_subdev *sd, int reg,
		const struct ov8830_reg *reglist, unsigned int *value)
{
	int high, low;

	high = ov8830_get_register(sd, reg, reglist);
	if (high < 0)
		return high;

	low = ov8830_get_register(sd, reg + 1, reglist);
	if (low < 0)
		return low;

	*value = ((u8) high << 8) | (u8) low;
	return 0;
}

static int ov8830_get_intg_factor(struct v4l2_subdev *sd,
				  struct camera_mipi_info *info,
				  const struct ov8830_reg *reglist)
{
	const int ext_clk = 19200000; /* MHz */
	struct atomisp_sensor_mode_data *m = &info->data;
	struct ov8830_device *dev = to_ov8830_sensor(sd);
	const struct ov8830_resolution *res =
				&dev->curr_res_table[dev->fmt_idx];
	int pll2_prediv;
	int pll2_multiplier;
	int pll2_divs;
	int pll2_seld5;
	int t1, t2, t3;
	int sclk;
	int ret;

	memset(&info->data, 0, sizeof(info->data));

	pll2_prediv     = ov8830_get_register(sd, OV8830_PLL_PLL10, reglist);
	pll2_multiplier = ov8830_get_register(sd, OV8830_PLL_PLL11, reglist);
	pll2_divs       = ov8830_get_register(sd, OV8830_PLL_PLL12, reglist);
	pll2_seld5      = ov8830_get_register(sd, OV8830_PLL_PLL13, reglist);

	if (pll2_prediv < 0 || pll2_multiplier < 0 ||
	    pll2_divs < 0 || pll2_seld5 < 0)
		return -EIO;

	pll2_prediv &= 0x07;
	pll2_multiplier &= 0x3F;
	pll2_divs = (pll2_divs & 0x0F) + 1;
	pll2_seld5 &= 0x03;

	if (pll2_prediv <= 0)
		return -EIO;

	t1 = ext_clk / pll2_prediv;
	t2 = t1 * pll2_multiplier;
	t3 = t2 / pll2_divs;
	sclk = t3;
	if (pll2_seld5 == 0)
		sclk = t3;
	else if (pll2_seld5 == 3)
		sclk = t3 * 2 / 5;
	else
		sclk = t3 / pll2_seld5;
	m->vt_pix_clk_freq_mhz = sclk;

	/* HTS and VTS */
	m->frame_length_lines =
			res->fps_options[dev->fps_index].lines_per_frame;
	m->line_length_pck = res->fps_options[dev->fps_index].pixels_per_line;

	m->coarse_integration_time_min = 0;
	m->coarse_integration_time_max_margin = OV8830_INTEGRATION_TIME_MARGIN;

	/* OV Sensor do not use fine integration time. */
	m->fine_integration_time_min = 0;
	m->fine_integration_time_max_margin = 0;

	/*
	 * read_mode indicate whether binning is used for calculating
	 * the correct exposure value from the user side. So adapt the
	 * read mode values accordingly.
	 */
	m->read_mode = res->bin_factor_x ?
		OV8830_READ_MODE_BINNING_ON : OV8830_READ_MODE_BINNING_OFF;

	ret = ov8830_get_register(sd, OV8830_TIMING_X_INC, res->regs);
	if (ret < 0)
		return ret;
	m->binning_factor_x = ((ret >> 4) + 1) / 2;

	ret = ov8830_get_register(sd, OV8830_TIMING_Y_INC, res->regs);
	if (ret < 0)
		return ret;
	m->binning_factor_y = ((ret >> 4) + 1) / 2;

	/* Get the cropping and output resolution to ISP for this mode. */
	ret =  ov8830_get_register_16bit(sd, OV8830_HORIZONTAL_START_H,
		res->regs, &m->crop_horizontal_start);
	if (ret)
		return ret;

	ret = ov8830_get_register_16bit(sd, OV8830_VERTICAL_START_H,
		res->regs, &m->crop_vertical_start);
	if (ret)
		return ret;

	ret = ov8830_get_register_16bit(sd, OV8830_HORIZONTAL_END_H,
		res->regs, &m->crop_horizontal_end);
	if (ret)
		return ret;

	ret = ov8830_get_register_16bit(sd, OV8830_VERTICAL_END_H,
		res->regs, &m->crop_vertical_end);
	if (ret)
		return ret;

	ret = ov8830_get_register_16bit(sd, OV8830_HORIZONTAL_OUTPUT_SIZE_H,
		res->regs, &m->output_width);
	if (ret)
		return ret;

	return ov8830_get_register_16bit(sd, OV8830_VERTICAL_OUTPUT_SIZE_H,
		res->regs, &m->output_height);
}

static int __ov8830_s_frame_interval(struct v4l2_subdev *sd,
			struct v4l2_subdev_frame_interval *interval)
{
	struct ov8830_device *dev = to_ov8830_sensor(sd);
	struct camera_mipi_info *info = v4l2_get_subdev_hostdata(sd);
	const struct ov8830_resolution *res =
		res = &dev->curr_res_table[dev->fmt_idx];
	int i;
	int ret;
	int fps;
	u16 hts;
	u16 vts;

	if (!interval->interval.numerator)
		interval->interval.numerator = 1;

	fps = interval->interval.denominator / interval->interval.numerator;

	/* Ignore if we are already using the required FPS. */
	if (fps == res->fps_options[dev->fps_index].fps)
		return 0;

	dev->fps_index = 0;

	/* Go through the supported FPS list */
	for (i = 0; i < MAX_FPS_OPTIONS_SUPPORTED; i++) {
		if (!res->fps_options[i].fps)
			break;
		if (abs(res->fps_options[i].fps - fps)
		    < abs(res->fps_options[dev->fps_index].fps - fps))
			dev->fps_index = i;
	}

	/* Get the new Frame timing values for new exposure */
	hts = res->fps_options[dev->fps_index].pixels_per_line;
	vts = res->fps_options[dev->fps_index].lines_per_frame;

	/* update frametiming. Conside the curren exposure/gain as well */
	ret = __ov8830_set_exposure(sd, dev->exposure, dev->gain,
					dev->digital_gain, &hts, &vts);
	if (ret)
		return ret;

	/* Update the new values so that user side knows the current settings */
	ret = ov8830_get_intg_factor(sd, info, dev->basic_settings_list);
	if (ret)
		return ret;

	interval->interval.denominator = res->fps_options[dev->fps_index].fps;
	interval->interval.numerator = 1;

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
/* tune this value so that the DVS resolutions get selected properly,
 * but make sure 16:9 does not match 4:3.
 */
#define LARGEST_ALLOWED_RATIO_MISMATCH 500
static int distance(struct ov8830_resolution const *res, const u32 w,
				const u32 h)
{
	unsigned int w_ratio = ((res->width<<13)/w);
	unsigned int h_ratio = ((res->height<<13)/h);
	int match   = abs(((w_ratio<<13)/h_ratio) - ((int)8192));

	if ((w_ratio < (int)8192) || (h_ratio < (int)8192)
		|| (match > LARGEST_ALLOWED_RATIO_MISMATCH))
		return -1;

	return w_ratio + h_ratio;
}

/*
 * Returns the nearest higher resolution index.
 * @w: width
 * @h: height
 * matching is done based on enveloping resolution and
 * aspect ratio. If the aspect ratio cannot be matched
 * to any index, -1 is returned.
 */
static int nearest_resolution_index(struct v4l2_subdev *sd, int w, int h)
{
	int i;
	int idx = -1;
	int dist;
	int min_dist = INT_MAX;
	const struct ov8830_resolution *tmp_res = NULL;
	struct ov8830_device *dev = to_ov8830_sensor(sd);

	for (i = 0; i < dev->entries_curr_table; i++) {
		tmp_res = &dev->curr_res_table[i];
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

static int get_resolution_index(struct v4l2_subdev *sd, int w, int h)
{
	int i;
	struct ov8830_device *dev = to_ov8830_sensor(sd);

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

static int __ov8830_try_mbus_fmt(struct v4l2_subdev *sd,
				 struct v4l2_mbus_framefmt *fmt)
{
	int idx;
	struct ov8830_device *dev = to_ov8830_sensor(sd);

	if (!fmt)
		return -EINVAL;

	if ((fmt->width > OV8830_RES_WIDTH_MAX) ||
	    (fmt->height > OV8830_RES_HEIGHT_MAX)) {
		fmt->width = OV8830_RES_WIDTH_MAX;
		fmt->height = OV8830_RES_HEIGHT_MAX;
	} else {
		idx = nearest_resolution_index(sd, fmt->width, fmt->height);

		/*
		 * nearest_resolution_index() doesn't return smaller resolutions.
		 * If it fails, it means the requested resolution is higher than we
		 * can support. Fallback to highest possible resolution in this case.
		 */
		if (idx == -1)
			idx = dev->entries_curr_table - 1;

		fmt->width = dev->curr_res_table[idx].width;
		fmt->height = dev->curr_res_table[idx].height;
	}

	fmt->code = V4L2_MBUS_FMT_SBGGR10_1X10;
	return 0;
}

static int ov8830_try_mbus_fmt(struct v4l2_subdev *sd,
			       struct v4l2_mbus_framefmt *fmt)
{
	struct ov8830_device *dev = to_ov8830_sensor(sd);
	int r;

	mutex_lock(&dev->input_lock);
	r = __ov8830_try_mbus_fmt(sd, fmt);
	mutex_unlock(&dev->input_lock);

	return r;
}

static int ov8830_s_mbus_fmt(struct v4l2_subdev *sd,
			      struct v4l2_mbus_framefmt *fmt)
{
	struct ov8830_device *dev = to_ov8830_sensor(sd);
	struct camera_mipi_info *ov8830_info = NULL;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u16 hts, vts;
	int ret;
	const struct ov8830_resolution *res;

	ov8830_info = v4l2_get_subdev_hostdata(sd);
	if (ov8830_info == NULL)
		return -EINVAL;

	mutex_lock(&dev->input_lock);

	ret = __ov8830_try_mbus_fmt(sd, fmt);
	if (ret)
		goto out;

	dev->fmt_idx = get_resolution_index(sd, fmt->width, fmt->height);
	/* Sanity check */
	if (unlikely(dev->fmt_idx == -1)) {
		ret = -EINVAL;
		goto out;
	}

	/* Sets the default FPS */
	dev->fps_index = 0;

	/* Get the current resolution setting */
	res = &dev->curr_res_table[dev->fmt_idx];

	/* Write the selected resolution table values to the registers */
	ret = ov8830_write_reg_array(client, res->regs);
	if (ret)
		goto out;

	/* Frame timing registers are updates as part of exposure */
	hts = res->fps_options[dev->fps_index].pixels_per_line;
	vts = res->fps_options[dev->fps_index].lines_per_frame;

	/*
	 * update hts, vts, exposure and gain as one block. Note that the vts
	 * will be changed according to the exposure used. But the maximum vts
	 * dev->curr_res_table[dev->fmt_idx] should not be changed at all.
	 */
	ret = __ov8830_set_exposure(sd, dev->exposure, dev->gain,
					dev->digital_gain, &hts, &vts);
	if (ret)
		goto out;

	ret = ov8830_get_intg_factor(sd, ov8830_info, dev->basic_settings_list);

out:
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int ov8830_g_mbus_fmt(struct v4l2_subdev *sd,
			      struct v4l2_mbus_framefmt *fmt)
{
	struct ov8830_device *dev = to_ov8830_sensor(sd);

	if (!fmt)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	fmt->width = dev->curr_res_table[dev->fmt_idx].width;
	fmt->height = dev->curr_res_table[dev->fmt_idx].height;
	fmt->code = V4L2_MBUS_FMT_SBGGR10_1X10;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static int ov8830_detect(struct i2c_client *client, u16 *id, u8 *revision)
{
	struct i2c_adapter *adapter = client->adapter;
	u16 id35;
	int ret, s_ret;

	/* i2c check */
	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -ENODEV;

	/* check sensor chip ID - are same for both 8830 and 8835 modules */
	ret = ov8830_read_reg(client, OV8830_16BIT, OV8830_CHIP_ID_HIGH, id);
	if (ret)
		return ret;

	/* This always reads as 0x8830, even on 8835. */
	dev_info(&client->dev, "chip_id = 0x%4.4x\n", *id);
	if (*id != OV8830_CHIP_ID)
		return -ENODEV;

	/*
	 * Check which module is attached OV8835 or OV8830.
	 * We need to support OV8830 for a while.
	 *
	 * For correctly identifying the OV8835 module, sensor needs
	 * to start streaming, OTP read enabled and wait for about 10ms
	 * before reading the OTB Bank 0 for OV8835 module identification.
	 *
	 * TODO/FIXME Revisit OTP support is added or OV8830 not needed anymore.
	 */
	ret = ov8830_write_reg_array(client, ov8835_module_detection);
	if (ret)
		return ret;

	msleep(20);

	ret = ov8830_read_reg(client, OV8830_8BIT, OV8830_OTP_BANK0_PID, &id35);
	if (ret)
		goto out;

	/* OTP BANK0 read will return 0x35 for OV8835 else 0*/
	if (id35 == 0x35)
		*id = OV8835_CHIP_ID;

	dev_info(&client->dev, "sensor is ov%4.4x\n", *id);

	/* REVISIT: HACK: Driver is currently forcing revision to 0 */
	*revision = 0;

out:
	/* Stream off now. */
	s_ret = ov8830_write_reg(client, OV8830_8BIT, OV8830_STREAM_MODE, 0);

	return ret ? ret : s_ret;
}

/*
 * ov8830 stream on/off
 */
static int ov8830_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct ov8830_device *dev = to_ov8830_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	mutex_lock(&dev->input_lock);

	ret = ov8830_write_reg(client, OV8830_8BIT, 0x0100, enable ? 1 : 0);
	if (ret != 0) {
		mutex_unlock(&dev->input_lock);
		v4l2_err(client, "failed to set streaming\n");
		return ret;
	}

	dev->streaming = enable;

	mutex_unlock(&dev->input_lock);

	return 0;
}

/*
 * ov8830 enum frame size, frame intervals
 */
static int ov8830_enum_framesizes(struct v4l2_subdev *sd,
				   struct v4l2_frmsizeenum *fsize)
{
	unsigned int index = fsize->index;
	struct ov8830_device *dev = to_ov8830_sensor(sd);

	mutex_lock(&dev->input_lock);
	if (index >= dev->entries_curr_table) {
		mutex_unlock(&dev->input_lock);
		return -EINVAL;
	}

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = dev->curr_res_table[index].width;
	fsize->discrete.height = dev->curr_res_table[index].height;
	fsize->reserved[0] = dev->curr_res_table[index].used;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static int ov8830_enum_frameintervals(struct v4l2_subdev *sd,
				       struct v4l2_frmivalenum *fival)
{
	unsigned int index = fival->index;
	int fmt_index;
	struct ov8830_device *dev = to_ov8830_sensor(sd);
	const struct ov8830_resolution *res;

	mutex_lock(&dev->input_lock);

	/*
	 * since the isp will donwscale the resolution to the right size,
	 * find the nearest one that will allow the isp to do so important to
	 * ensure that the resolution requested is padded correctly by the
	 * requester, which is the atomisp driver in this case.
	 */
	fmt_index = nearest_resolution_index(sd, fival->width, fival->height);
	if (-1 == fmt_index)
		fmt_index = dev->entries_curr_table - 1;

	res = &dev->curr_res_table[fmt_index];

	/* Check if this index is supported */
	if (index > __ov8830_get_max_fps_index(res->fps_options)) {
		mutex_unlock(&dev->input_lock);
		return -EINVAL;
	}

	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fival->discrete.numerator = 1;
	fival->discrete.denominator = res->fps_options[index].fps;

	mutex_unlock(&dev->input_lock);

	return 0;
}

static int ov8830_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned int index,
				 enum v4l2_mbus_pixelcode *code)
{
	*code = V4L2_MBUS_FMT_SBGGR10_1X10;
	return 0;
}

static int ov8830_s_config(struct v4l2_subdev *sd,
			    int irq, void *pdata)
{
	struct ov8830_device *dev = to_ov8830_sensor(sd);
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
			v4l2_err(client, "ov8830 platform init err\n");
			return ret;
		}
	}

	ret = __ov8830_s_power(sd, 1);
	if (ret) {
		mutex_unlock(&dev->input_lock);
		v4l2_err(client, "ov8830 power-up err.\n");
		return ret;
	}

	ret = dev->platform_data->csi_cfg(sd, 1);
	if (ret)
		goto fail_csi_cfg;

	/* config & detect sensor */
	ret = ov8830_detect(client, &sensor_id, &sensor_revision);
	if (ret) {
		v4l2_err(client, "ov8830_detect err s_config.\n");
		goto fail_detect;
	}

	dev->sensor_id = sensor_id;
	dev->sensor_revision = sensor_revision;

	/* power off sensor */
	ret = __ov8830_s_power(sd, 0);
	mutex_unlock(&dev->input_lock);
	if (ret) {
		v4l2_err(client, "ov8830 power-down err.\n");
		return ret;
	}

	return 0;

fail_detect:
	dev->platform_data->csi_cfg(sd, 0);
fail_csi_cfg:
	__ov8830_s_power(sd, 0);
	mutex_unlock(&dev->input_lock);
	dev_err(&client->dev, "sensor power-gating failed\n");
	return ret;
}

static int
ov8830_enum_mbus_code(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index)
		return -EINVAL;
	code->code = V4L2_MBUS_FMT_SBGGR10_1X10;

	return 0;
}

static int
ov8830_enum_frame_size(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			struct v4l2_subdev_frame_size_enum *fse)
{
	int index = fse->index;
	struct ov8830_device *dev = to_ov8830_sensor(sd);

	mutex_lock(&dev->input_lock);
	if (index >= dev->entries_curr_table) {
		mutex_unlock(&dev->input_lock);
		return -EINVAL;
	}

	fse->min_width = dev->curr_res_table[index].width;
	fse->min_height = dev->curr_res_table[index].height;
	fse->max_width = dev->curr_res_table[index].width;
	fse->max_height = dev->curr_res_table[index].height;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static struct v4l2_mbus_framefmt *
__ov8830_get_pad_format(struct ov8830_device *sensor,
			 struct v4l2_subdev_fh *fh, unsigned int pad,
			 enum v4l2_subdev_format_whence which)
{
	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_format(fh, pad);

	return &sensor->format;
}

static int
ov8830_get_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct ov8830_device *dev = to_ov8830_sensor(sd);

	fmt->format = *__ov8830_get_pad_format(dev, fh, fmt->pad, fmt->which);

	return 0;
}

static int
ov8830_set_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct ov8830_device *dev = to_ov8830_sensor(sd);
	struct v4l2_mbus_framefmt *format =
			__ov8830_get_pad_format(dev, fh, fmt->pad, fmt->which);

	*format = fmt->format;

	return 0;
}

static int ov8830_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov8830_device *dev = container_of(
		ctrl->handler, struct ov8830_device, ctrl_handler);
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);

	/* input_lock is taken by the control framework, so it
	 * doesn't need to be taken here.
	 */

	/* We only handle V4L2_CID_RUN_MODE for now. */
	switch (ctrl->id) {
	case V4L2_CID_RUN_MODE:
		switch (ctrl->val) {
		case ATOMISP_RUN_MODE_VIDEO:
			dev->curr_res_table = dev->sensor_id == OV8835_CHIP_ID ?
				ov8835_res_video : ov8830_res_video;
			dev->entries_curr_table =
				dev->sensor_id == OV8835_CHIP_ID ?
				ARRAY_SIZE(ov8835_res_video) :
				ARRAY_SIZE(ov8830_res_video);
			break;
		case ATOMISP_RUN_MODE_STILL_CAPTURE:
			dev->curr_res_table = dev->sensor_id == OV8835_CHIP_ID ?
				ov8835_res_still : ov8830_res_still;
			dev->entries_curr_table =
				dev->sensor_id == OV8835_CHIP_ID ?
				ARRAY_SIZE(ov8835_res_still) :
				ARRAY_SIZE(ov8830_res_still);
			break;
		default:
			dev->curr_res_table = dev->sensor_id == OV8835_CHIP_ID ?
				ov8835_res_preview : ov8830_res_preview;
			dev->entries_curr_table =
				dev->sensor_id == OV8835_CHIP_ID ?
				ARRAY_SIZE(ov8835_res_preview) :
				ARRAY_SIZE(ov8830_res_preview);
		}

		dev->fmt_idx = 0;
		dev->fps_index = 0;

		return 0;
	case V4L2_CID_TEST_PATTERN:
		return ov8830_write_reg(client, OV8830_16BIT, 0x3070,
					ctrl->val);
	case V4L2_CID_FOCUS_ABSOLUTE:
		return drv201_t_focus_abs(&dev->sd, ctrl->val);
	}

	return -EINVAL; /* Should not happen. */
}

static int ov8830_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov8830_device *dev = container_of(
		ctrl->handler, struct ov8830_device, ctrl_handler);

	switch (ctrl->id) {
	case V4L2_CID_FOCUS_STATUS: {
		static const struct timespec move_time = {
			/* The time required for focus motor to move the lens */
			.tv_sec = 0,
			.tv_nsec = 60000000,
		};
		struct drv201_device *drv201 = to_drv201_device(&dev->sd);
		struct timespec current_time, finish_time, delta_time;

		getnstimeofday(&current_time);
		finish_time = timespec_add(drv201->focus_time, move_time);
		delta_time = timespec_sub(current_time, finish_time);
		if (delta_time.tv_sec >= 0 && delta_time.tv_nsec >= 0) {
			/* VCM motor is not moving */
			ctrl->val = ATOMISP_FOCUS_HP_COMPLETE |
				ATOMISP_FOCUS_STATUS_ACCEPTS_NEW_MOVE;
		} else {
			/* VCM motor is still moving */
			ctrl->val = ATOMISP_FOCUS_STATUS_MOVING |
				ATOMISP_FOCUS_HP_IN_PROGRESS;
		}
		return 0;
	}
	case V4L2_CID_BIN_FACTOR_HORZ:
	case V4L2_CID_BIN_FACTOR_VERT: {
		uint16_t reg = ctrl->id == V4L2_CID_BIN_FACTOR_VERT ?
			OV8830_TIMING_X_INC : OV8830_TIMING_Y_INC;
		int r = ov8830_get_register(
			&dev->sd, reg, dev->curr_res_table[dev->fmt_idx].regs);

		if (r < 0)
			return r;

		ctrl->val = fls((r >> 4) + (r & 0xf)) - 2;

		return 0;
	}
	}

	return 0;
}

static int
ov8830_g_frame_interval(struct v4l2_subdev *sd,
			struct v4l2_subdev_frame_interval *interval)
{
	struct ov8830_device *dev = to_ov8830_sensor(sd);
	const struct ov8830_resolution *res;

	mutex_lock(&dev->input_lock);

	/* Return the currently selected settings' maximum frame interval */
	res = &dev->curr_res_table[dev->fmt_idx];

	interval->interval.numerator = 1;
	interval->interval.denominator = res->fps_options[dev->fps_index].fps;

	mutex_unlock(&dev->input_lock);

	return 0;
}

static int ov8830_s_frame_interval(struct v4l2_subdev *sd,
			struct v4l2_subdev_frame_interval *interval)
{
	struct ov8830_device *dev = to_ov8830_sensor(sd);
	int ret;

	mutex_lock(&dev->input_lock);
	ret = __ov8830_s_frame_interval(sd, interval);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int ov8830_g_skip_frames(struct v4l2_subdev *sd, u32 *frames)
{
	struct ov8830_device *dev = to_ov8830_sensor(sd);

	mutex_lock(&dev->input_lock);
	*frames = dev->curr_res_table[dev->fmt_idx].skip_frames;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static const struct v4l2_subdev_video_ops ov8830_video_ops = {
	.s_stream = ov8830_s_stream,
	.enum_framesizes = ov8830_enum_framesizes,
	.enum_frameintervals = ov8830_enum_frameintervals,
	.enum_mbus_fmt = ov8830_enum_mbus_fmt,
	.try_mbus_fmt = ov8830_try_mbus_fmt,
	.g_mbus_fmt = ov8830_g_mbus_fmt,
	.s_mbus_fmt = ov8830_s_mbus_fmt,
	.g_frame_interval = ov8830_g_frame_interval,
	.s_frame_interval = ov8830_s_frame_interval,
};

static const struct v4l2_subdev_sensor_ops ov8830_sensor_ops = {
	.g_skip_frames	= ov8830_g_skip_frames,
};

static const struct v4l2_subdev_core_ops ov8830_core_ops = {
	.g_chip_ident = ov8830_g_chip_ident,
	.queryctrl = v4l2_subdev_queryctrl,
	.g_ctrl = v4l2_subdev_g_ctrl,
	.s_ctrl = v4l2_subdev_s_ctrl,
	.s_power = ov8830_s_power,
	.ioctl = ov8830_ioctl,
	.init = ov8830_init,
};

/* REVISIT: Do we need pad operations? */
static const struct v4l2_subdev_pad_ops ov8830_pad_ops = {
	.enum_mbus_code = ov8830_enum_mbus_code,
	.enum_frame_size = ov8830_enum_frame_size,
	.get_fmt = ov8830_get_pad_format,
	.set_fmt = ov8830_set_pad_format,
};

static const struct v4l2_subdev_ops ov8830_ops = {
	.core = &ov8830_core_ops,
	.video = &ov8830_video_ops,
	.pad = &ov8830_pad_ops,
	.sensor = &ov8830_sensor_ops,
};

static int ov8830_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov8830_device *dev = to_ov8830_sensor(sd);
	if (dev->platform_data->platform_deinit)
		dev->platform_data->platform_deinit();

	media_entity_cleanup(&dev->sd.entity);
	v4l2_ctrl_handler_free(&dev->ctrl_handler);
	dev->platform_data->csi_cfg(sd, 0);
	v4l2_device_unregister_subdev(sd);
	kfree(dev);

	return 0;
}

static const struct v4l2_ctrl_ops ctrl_ops = {
	.s_ctrl = ov8830_s_ctrl,
	.g_volatile_ctrl = ov8830_g_ctrl,
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
		.name = "Absolute exposure",
		.type = V4L2_CTRL_TYPE_MENU,
		.max = 0xffff,
		.qmenu = ctrl_run_mode_menu,
	}, {
		.ops = &ctrl_ops,
		.id = V4L2_CID_TEST_PATTERN,
		.name = "Test pattern",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.step = 1,
		.max = 0xffff,
	}, {
		.ops = &ctrl_ops,
		.id = V4L2_CID_FOCUS_ABSOLUTE,
		.name = "Focus absolute",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.step = 1,
		.max = DRV201_MAX_FOCUS_POS,
	}, {
		/* This one is junk: see the spec for proper use of this CID. */
		.ops = &ctrl_ops,
		.id = V4L2_CID_FOCUS_STATUS,
		.name = "Focus status",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.step = 1,
		.max = 100,
		.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE,
	}, {
		/* This is crap. For compatibility use only. */
		.ops = &ctrl_ops,
		.id = V4L2_CID_FOCAL_ABSOLUTE,
		.name = "Focal lenght",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = (OV8830_FOCAL_LENGTH_NUM << 16) | OV8830_FOCAL_LENGTH_DEM,
		.max = (OV8830_FOCAL_LENGTH_NUM << 16) | OV8830_FOCAL_LENGTH_DEM,
		.step = 1,
		.def = (OV8830_FOCAL_LENGTH_NUM << 16) | OV8830_FOCAL_LENGTH_DEM,
		.flags = V4L2_CTRL_FLAG_READ_ONLY,
	}, {
		/* This one is crap, too. For compatibility use only. */
		.ops = &ctrl_ops,
		.id = V4L2_CID_FNUMBER_ABSOLUTE,
		.name = "F-number",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = (OV8830_F_NUMBER_DEFAULT_NUM << 16) | OV8830_F_NUMBER_DEM,
		.max = (OV8830_F_NUMBER_DEFAULT_NUM << 16) | OV8830_F_NUMBER_DEM,
		.step = 1,
		.def = (OV8830_F_NUMBER_DEFAULT_NUM << 16) | OV8830_F_NUMBER_DEM,
		.flags = V4L2_CTRL_FLAG_READ_ONLY,
	}, {
		/*
		 * The most utter crap. _Never_ use this, even for
		 * compatibility reasons!
		 */
		.ops = &ctrl_ops,
		.id = V4L2_CID_FNUMBER_RANGE,
		.name = "F-number range",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = (OV8830_F_NUMBER_DEFAULT_NUM << 24) | (OV8830_F_NUMBER_DEM << 16) | (OV8830_F_NUMBER_DEFAULT_NUM << 8) | OV8830_F_NUMBER_DEM,
		.max = (OV8830_F_NUMBER_DEFAULT_NUM << 24) | (OV8830_F_NUMBER_DEM << 16) | (OV8830_F_NUMBER_DEFAULT_NUM << 8) | OV8830_F_NUMBER_DEM,
		.step = 1,
		.def = (OV8830_F_NUMBER_DEFAULT_NUM << 24) | (OV8830_F_NUMBER_DEM << 16) | (OV8830_F_NUMBER_DEFAULT_NUM << 8) | OV8830_F_NUMBER_DEM,
		.flags = V4L2_CTRL_FLAG_READ_ONLY,
	}, {
		.ops = &ctrl_ops,
		.id = V4L2_CID_BIN_FACTOR_HORZ,
		.name = "Horizontal binning factor",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.max = OV8830_BIN_FACTOR_MAX,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE,
	}, {
		.ops = &ctrl_ops,
		.id = V4L2_CID_BIN_FACTOR_VERT,
		.name = "Vertical binning factor",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.max = OV8830_BIN_FACTOR_MAX,
		.step = 1,
		.flags = V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_VOLATILE,
	}
};

static int ov8830_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct ov8830_device *dev;
	unsigned int i;
	int ret;

	/* allocate sensor device & init sub device */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		v4l2_err(client, "%s: out of memory\n", __func__);
		return -ENOMEM;
	}

	mutex_init(&dev->input_lock);

	dev->fmt_idx = 0;
	v4l2_i2c_subdev_init(&(dev->sd), client, &ov8830_ops);

	ret = drv201_init(&dev->sd);
	if (ret < 0)
		goto out_free;

	if (client->dev.platform_data) {
		ret = ov8830_s_config(&dev->sd, client->irq,
				      client->dev.platform_data);
		if (ret)
			goto out_free;
	}

	dev->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->pad.flags = MEDIA_PAD_FL_SOURCE;
	dev->sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
	dev->format.code = V4L2_MBUS_FMT_SBGGR10_1X10;

	ret = v4l2_ctrl_handler_init(&dev->ctrl_handler, ARRAY_SIZE(ctrls) + 1);
	if (ret) {
		ov8830_remove(client);
		return ret;
	}

	dev->run_mode = v4l2_ctrl_new_custom(&dev->ctrl_handler,
					     &ctrl_run_mode, NULL);

	for (i = 0; i < ARRAY_SIZE(ctrls); i++)
		v4l2_ctrl_new_custom(&dev->ctrl_handler, &ctrls[i], NULL);

	if (dev->ctrl_handler.error) {
		ov8830_remove(client);
		return dev->ctrl_handler.error;
	}

	/* Use same lock for controls as for everything else. */
	dev->ctrl_handler.lock = &dev->input_lock;
	dev->sd.ctrl_handler = &dev->ctrl_handler;
	v4l2_ctrl_handler_setup(&dev->ctrl_handler);

	ret = media_entity_init(&dev->sd.entity, 1, &dev->pad, 0);
	if (ret) {
		ov8830_remove(client);
		return ret;
	}

	return 0;

out_free:
	v4l2_device_unregister_subdev(&dev->sd);
	kfree(dev);
	return ret;
}

static const struct i2c_device_id ov8830_id[] = {
	{OV8830_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, ov8830_id);

static struct i2c_driver ov8830_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = OV8830_NAME,
	},
	.probe = ov8830_probe,
	.remove = ov8830_remove,
	.id_table = ov8830_id,
};

static __init int ov8830_init_mod(void)
{
	return i2c_add_driver(&ov8830_driver);
}

static __exit void ov8830_exit_mod(void)
{
	i2c_del_driver(&ov8830_driver);
}

module_init(ov8830_init_mod);
module_exit(ov8830_exit_mod);

MODULE_DESCRIPTION("A low-level driver for Omnivision OV8830 sensors");
MODULE_LICENSE("GPL");
