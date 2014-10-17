/*
 * Support for mt9m114 Camera Sensor.
 *
 * Copyright (c) 2010 Intel Corporation. All Rights Reserved.
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
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#ifdef CONFIG_GMIN_INTEL_MID
#include <linux/acpi.h>
#include <linux/atomisp_gmin_platform.h>
#endif
#include <media/v4l2-device.h>
#ifndef CONFIG_GMIN_INTEL_MID
#include <media/v4l2-chip-ident.h>
#endif

#include "mt9m114.h"

#define to_mt9m114_sensor(sd) container_of(sd, struct mt9m114_device, sd)

/*
 * TODO: use debug parameter to actually define when debug messages should
 * be printed.
 */
static int debug;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Debug level (0-1)");

static int mt9m114_t_vflip(struct v4l2_subdev *sd, int value);
static int mt9m114_t_hflip(struct v4l2_subdev *sd, int value);
static int mt9m114_wait_state(struct i2c_client *client, int timeout);

static int
mt9m114_read_reg(struct i2c_client *client, u16 data_length, u32 reg, u32 *val)
{
	int err;
	struct i2c_msg msg[2];
	unsigned char data[4];

	if (!client->adapter) {
		v4l2_err(client, "%s error, no client->adapter\n", __func__);
		return -ENODEV;
	}

	if (data_length != MISENSOR_8BIT && data_length != MISENSOR_16BIT
					 && data_length != MISENSOR_32BIT) {
		v4l2_err(client, "%s error, invalid data length\n", __func__);
		return -EINVAL;
	}

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = MSG_LEN_OFFSET;
	msg[0].buf = data;

	/* high byte goes out first */
	data[0] = (u16) (reg >> 8);
	data[1] = (u16) (reg & 0xff);

	msg[1].addr = client->addr;
	msg[1].len = data_length;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = data;

	err = i2c_transfer(client->adapter, msg, 2);

	if (err >= 0) {
		*val = 0;
		/* high byte comes first */
		if (data_length == MISENSOR_8BIT)
			*val = data[0];
		else if (data_length == MISENSOR_16BIT)
			*val = data[1] + (data[0] << 8);
		else
			*val = data[3] + (data[2] << 8) +
			    (data[1] << 16) + (data[0] << 24);

		return 0;
	}

	dev_err(&client->dev, "read from offset 0x%x error %d", reg, err);
	return err;
}

static int
mt9m114_write_reg(struct i2c_client *client, u16 data_length, u16 reg, u32 val)
{
	int num_msg;
	struct i2c_msg msg;
	unsigned char data[6] = {0};
	u16 *wreg;
	int retry = 0;

	if (!client->adapter) {
		v4l2_err(client, "%s error, no client->adapter\n", __func__);
		return -ENODEV;
	}

	if (data_length != MISENSOR_8BIT && data_length != MISENSOR_16BIT
					 && data_length != MISENSOR_32BIT) {
		v4l2_err(client, "%s error, invalid data_length\n", __func__);
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
		*wdata = be16_to_cpu((u16)val);
	} else {
		/* MISENSOR_32BIT */
		u32 *wdata = (u32 *)&data[2];
		*wdata = be32_to_cpu(val);
	}

	num_msg = i2c_transfer(client->adapter, &msg, 1);

	/*
	 * HACK: Need some delay here for Rev 2 sensors otherwise some
	 * registers do not seem to load correctly.
	 */
	mdelay(1);

	if (num_msg >= 0)
		return 0;

	dev_err(&client->dev, "write error: wrote 0x%x to offset 0x%x error %d",
		val, reg, num_msg);
	if (retry <= I2C_RETRY_COUNT) {
		dev_dbg(&client->dev, "retrying... %d", retry);
		retry++;
		msleep(20);
		goto again;
	}

	return num_msg;
}

/**
 * misensor_rmw_reg - Read/Modify/Write a value to a register in the sensor
 * device
 * @client: i2c driver client structure
 * @data_length: 8/16/32-bits length
 * @reg: register address
 * @mask: masked out bits
 * @set: bits set
 *
 * Read/modify/write a value to a register in the  sensor device.
 * Returns zero if successful, or non-zero otherwise.
 */
static int
misensor_rmw_reg(struct i2c_client *client, u16 data_length, u16 reg,
		     u32 mask, u32 set)
{
	int err;
	u32 val;

	/* Exit when no mask */
	if (mask == 0)
		return 0;

	/* @mask must not exceed data length */
	switch (data_length) {
	case MISENSOR_8BIT:
		if (mask & ~0xff)
			return -EINVAL;
		break;
	case MISENSOR_16BIT:
		if (mask & ~0xffff)
			return -EINVAL;
		break;
	case MISENSOR_32BIT:
		break;
	default:
		/* Wrong @data_length */
		return -EINVAL;
	}

	err = mt9m114_read_reg(client, data_length, reg, &val);
	if (err) {
		v4l2_err(client, "misensor_rmw_reg error exit, read failed\n");
		return -EINVAL;
	}

	val &= ~mask;

	/*
	 * Perform the OR function if the @set exists.
	 * Shift @set value to target bit location. @set should set only
	 * bits included in @mask.
	 *
	 * REVISIT: This function expects @set to be non-shifted. Its shift
	 * value is then defined to be equal to mask's LSB position.
	 * How about to inform values in their right offset position and avoid
	 * this unneeded shift operation?
	 */
	set <<= ffs(mask) - 1;
	val |= set & mask;

	err = mt9m114_write_reg(client, data_length, reg, val);
	if (err) {
		v4l2_err(client, "misensor_rmw_reg error exit, write failed\n");
		return -EINVAL;
	}

	return 0;
}


static int __mt9m114_flush_reg_array(struct i2c_client *client,
				     struct mt9m114_write_ctrl *ctrl)
{
	struct i2c_msg msg;
	const int num_msg = 1;
	int ret;
	int retry = 0;

	if (ctrl->index == 0)
		return 0;

again:
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 2 + ctrl->index;
	ctrl->buffer.addr = cpu_to_be16(ctrl->buffer.addr);
	msg.buf = (u8 *)&ctrl->buffer;

	ret = i2c_transfer(client->adapter, &msg, num_msg);
	if (ret != num_msg) {
		if (++retry <= I2C_RETRY_COUNT) {
			dev_dbg(&client->dev, "retrying... %d\n", retry);
			msleep(20);
			goto again;
		}
		dev_err(&client->dev, "%s: i2c transfer error\n", __func__);
		return -EIO;
	}

	ctrl->index = 0;

	/*
	 * REVISIT: Previously we had a delay after writing data to sensor.
	 * But it was removed as our tests have shown it is not necessary
	 * anymore.
	 */

	return 0;
}

static int __mt9m114_buf_reg_array(struct i2c_client *client,
				   struct mt9m114_write_ctrl *ctrl,
				   const struct misensor_reg *next)
{
	u16 *data16;
	u32 *data32;
	int err;

	/* Insufficient buffer? Let's flush and get more free space. */
	if (ctrl->index + next->length >= MT9M114_MAX_WRITE_BUF_SIZE) {
		err = __mt9m114_flush_reg_array(client, ctrl);
		if (err)
			return err;
	}

	switch (next->length) {
	case MISENSOR_8BIT:
		ctrl->buffer.data[ctrl->index] = (u8)next->val;
		break;
	case MISENSOR_16BIT:
		data16 = (u16 *)&ctrl->buffer.data[ctrl->index];
		*data16 = cpu_to_be16((u16)next->val);
		break;
	case MISENSOR_32BIT:
		data32 = (u32 *)&ctrl->buffer.data[ctrl->index];
		*data32 = cpu_to_be32(next->val);
		break;
	default:
		return -EINVAL;
	}

	/* When first item is added, we need to store its starting address */
	if (ctrl->index == 0)
		ctrl->buffer.addr = next->reg;

	ctrl->index += next->length;

	return 0;
}

static int
__mt9m114_write_reg_is_consecutive(struct i2c_client *client,
				   struct mt9m114_write_ctrl *ctrl,
				   const struct misensor_reg *next)
{
	if (ctrl->index == 0)
		return 1;

	return ctrl->buffer.addr + ctrl->index == next->reg;
}

/*
 * mt9m114_write_reg_array - Initializes a list of mt9m114 registers
 * @client: i2c driver client structure
 * @reglist: list of registers to be written
 * @poll: completion polling requirement
 * This function initializes a list of registers. When consecutive addresses
 * are found in a row on the list, this function creates a buffer and sends
 * consecutive data in a single i2c_transfer().
 *
 * __mt9m114_flush_reg_array, __mt9m114_buf_reg_array() and
 * __mt9m114_write_reg_is_consecutive() are internal functions to
 * mt9m114_write_reg_array() and should be not used anywhere else.
 *
 */
static int mt9m114_write_reg_array(struct i2c_client *client,
				const struct misensor_reg *reglist,
				int poll)
{
	const struct misensor_reg *next = reglist;
	struct mt9m114_write_ctrl ctrl;
	int err;

	if (poll == PRE_POLLING) {
		err = mt9m114_wait_state(client, MT9M114_WAIT_STAT_TIMEOUT);
		if (err)
			return err;
	}

	ctrl.index = 0;
	for (; next->length != MISENSOR_TOK_TERM; next++) {
		switch (next->length & MISENSOR_TOK_MASK) {
		case MISENSOR_TOK_DELAY:
			err = __mt9m114_flush_reg_array(client, &ctrl);
			if (err)
				return err;
			msleep(next->val);
			break;
		case MISENSOR_TOK_RMW:
			err = __mt9m114_flush_reg_array(client, &ctrl);
			err |= misensor_rmw_reg(client,
						next->length &
							~MISENSOR_TOK_RMW,
						next->reg, next->val,
						next->val2);
			if (err) {
				dev_err(&client->dev, "%s read err. aborted\n",
					__func__);
				return -EINVAL;
			}
			break;
		default:
			/*
			 * If next address is not consecutive, data needs to be
			 * flushed before proceed.
			 */
			if (!__mt9m114_write_reg_is_consecutive(client, &ctrl,
								next)) {
				err = __mt9m114_flush_reg_array(client, &ctrl);
				if (err)
					return err;
			}
			err = __mt9m114_buf_reg_array(client, &ctrl, next);
			if (err) {
				v4l2_err(client, "%s: write error, aborted\n",
					 __func__);
				return err;
			}
			break;
		}
	}

	err = __mt9m114_flush_reg_array(client, &ctrl);
	if (err)
		return err;

	if (poll == POST_POLLING)
		return mt9m114_wait_state(client, MT9M114_WAIT_STAT_TIMEOUT);

	return 0;
}

static int mt9m114_wait_state(struct i2c_client *client, int timeout)
{
	int ret;
	unsigned int val;

	while (timeout-- > 0) {
		ret = mt9m114_read_reg(client, MISENSOR_16BIT, 0x0080, &val);
		if (ret)
			return ret;
		if ((val & 0x2) == 0)
			return 0;
		msleep(20);
	}

	return -EINVAL;

}

static int mt9m114_set_suspend(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
#ifndef CONFIG_GMIN_INTEL_MID /* FIXME! */
	return mt9m114_write_reg_array(client, mt9m114_suspend, POST_POLLING);
#else
	return mt9m114_write_reg_array(client, mt9m114_standby_reg, POST_POLLING);
#endif
}
#ifndef CONFIG_GMIN_INTEL_MID /* FIXME! */
static int mt9m114_set_streaming(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return mt9m114_write_reg_array(client, mt9m114_streaming, POST_POLLING);
}
#endif

static int mt9m114_init_common(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = mt9m114_write_reg_array(client, mt9m114_common, PRE_POLLING);
	if (ret)
		return ret;
#ifndef CONFIG_GMIN_INTEL_MID /* FIXME! */
	ret = mt9m114_write_reg_array(client, mt9m114_iq, NO_POLLING);
#endif
	return ret;
}

static int power_ctrl(struct v4l2_subdev *sd, bool flag)
{
	int ret;
	struct mt9m114_device *dev = to_mt9m114_sensor(sd);

	if (!dev || !dev->platform_data)
		return -ENODEV;

	/* Non-gmin platforms use the legacy callback */
	if (dev->platform_data->power_ctrl)
		return dev->platform_data->power_ctrl(sd, flag);

#ifdef CONFIG_GMIN_INTEL_MID
	if (flag) {
		ret = dev->platform_data->v2p8_ctrl(sd, 1);
		if (ret == 0) {
			ret = dev->platform_data->v1p8_ctrl(sd, 1);
			if (ret)
				ret = dev->platform_data->v2p8_ctrl(sd, 0);
		}
	} else {
		ret = dev->platform_data->v2p8_ctrl(sd, 0);
		ret = dev->platform_data->v1p8_ctrl(sd, 0);
	}
#else
	ret = -EINVAL;
#endif
	return ret;
}

static int gpio_ctrl(struct v4l2_subdev *sd, bool flag)
{
	int ret;
	struct mt9m114_device *dev = to_mt9m114_sensor(sd);

	if (!dev || !dev->platform_data)
		return -ENODEV;

	/* Non-gmin platforms use the legacy callback */
	if (dev->platform_data->gpio_ctrl)
		return dev->platform_data->gpio_ctrl(sd, flag);

#ifdef CONFIG_GMIN_INTEL_MID
	/* Note: current modules wire only one GPIO signal (RESET#),
	 * but the schematic wires up two to the connector.  BIOS
	 * versions have been unfortunately inconsistent with which
	 * ACPI index RESET# is on, so hit both */

	if (flag) {
		ret = dev->platform_data->gpio0_ctrl(sd, 0);
		ret = dev->platform_data->gpio1_ctrl(sd, 0);
		msleep(60);
		ret |= dev->platform_data->gpio0_ctrl(sd, 1);
		ret |= dev->platform_data->gpio1_ctrl(sd, 1);
	} else {
		ret = dev->platform_data->gpio0_ctrl(sd, 0);
		ret = dev->platform_data->gpio1_ctrl(sd, 0);
	}
#else
	ret = -EINVAL;
#endif
	return ret;
}

static int power_up(struct v4l2_subdev *sd)
{
	struct mt9m114_device *dev = to_mt9m114_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	if (NULL == dev->platform_data) {
		dev_err(&client->dev, "no camera_sensor_platform_data");
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
	if (ret)
		dev_err(&client->dev, "gpio failed 1\n");
	/*
	 * according to DS, 44ms is needed between power up and first i2c
	 * commend
	 */
	msleep(50);

	return 0;

fail_clk:
	dev->platform_data->flisclk_ctrl(sd, 0);
fail_power:
	power_ctrl(sd, 0);
	dev_err(&client->dev, "sensor power-up failed\n");

	return ret;
}

static int power_down(struct v4l2_subdev *sd)
{
	struct mt9m114_device *dev = to_mt9m114_sensor(sd);
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
	ret = gpio_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "gpio failed 1\n");

	/* power control */
	ret = power_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "vprog failed.\n");

	/*according to DS, 20ms is needed after power down*/
	msleep(20);

	return ret;
}

static int mt9m114_s_power(struct v4l2_subdev *sd, int power)
{
	if (power == 0)
		return power_down(sd);
	else {
		if (power_up(sd))
			return -EINVAL;

		return mt9m114_init_common(sd);
	}
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
static int distance(struct mt9m114_res_struct *res, u32 w, u32 h)
{
	unsigned int w_ratio;
	unsigned int h_ratio;
	int match;

	if (w == 0)
		return -1;
	w_ratio = ((res->width << 13) / w);

	if (h == 0)
		return -1;
	h_ratio = ((res->height << 13) / h);
	if (h_ratio == 0)
		return -1;
	match = abs(((w_ratio << 13) / h_ratio) - ((int)8192));

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
	struct mt9m114_res_struct *tmp_res = NULL;

	for (i = 0; i < N_RES; i++) {
		tmp_res = &mt9m114_res[i];
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

static int mt9m114_try_res(u32 *w, u32 *h)
{
	int idx;

	idx = nearest_resolution_index(*w, *h);

	if (idx == -1) {
		/* return the largest resolution */
		*w = mt9m114_res[N_RES-1].width;
		*h = mt9m114_res[N_RES-1].height;
	} else {
		*w = mt9m114_res[idx].width;
		*h = mt9m114_res[idx].height;
	}

	return 0;
}

static struct mt9m114_res_struct *mt9m114_to_res(u32 w, u32 h)
{
	int  index;

	for (index = 0; index < N_RES; index++) {
		if ((mt9m114_res[index].width == w) &&
		    (mt9m114_res[index].height == h))
			break;
	}

	/* No mode found */
	if (index >= N_RES)
		return NULL;

	return &mt9m114_res[index];
}

static int mt9m114_try_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	return mt9m114_try_res(&fmt->width, &fmt->height);
}

static int mt9m114_res2size(unsigned int res, int *h_size, int *v_size)
{
	unsigned short hsize;
	unsigned short vsize;

	switch (res) {
#ifndef CONFIG_GMIN_INTEL_MID /* FIXME! */
	case MT9M114_RES_QCIF:
		hsize = MT9M114_RES_QCIF_SIZE_H;
		vsize = MT9M114_RES_QCIF_SIZE_V;
		break;
	case MT9M114_RES_QVGA:
		hsize = MT9M114_RES_QVGA_SIZE_H;
		vsize = MT9M114_RES_QVGA_SIZE_V;
		break;
	case MT9M114_RES_VGA:
		hsize = MT9M114_RES_VGA_SIZE_H;
		vsize = MT9M114_RES_VGA_SIZE_V;
		break;
	case MT9M114_RES_480P:
		hsize = MT9M114_RES_480P_SIZE_H;
		vsize = MT9M114_RES_480P_SIZE_V;
		break;
	case MT9M114_RES_576P:
		hsize = MT9M114_RES_576P_SIZE_H;
		vsize = MT9M114_RES_576P_SIZE_V;
		break;
	case MT9M114_RES_720P:
		hsize = MT9M114_RES_720P_SIZE_H;
		vsize = MT9M114_RES_720P_SIZE_V;
		break;
#else
	case MT9M114_RES_720_480p_768:
		hsize = MT9M114_RES_720_480p_768_SIZE_H;
		vsize = MT9M114_RES_720_480p_768_SIZE_V;
		break;
	case MT9M114_RES_736P:
		hsize = MT9M114_RES_736P_SIZE_H;
		vsize = MT9M114_RES_736P_SIZE_V;
		break;
	case MT9M114_RES_864P:
		hsize = MT9M114_RES_864P_SIZE_H;
		vsize = MT9M114_RES_864P_SIZE_V;
		break;
#endif
	case MT9M114_RES_960P:
		hsize = MT9M114_RES_960P_SIZE_H;
		vsize = MT9M114_RES_960P_SIZE_V;
		break;
	default:
		WARN(1, "%s: Resolution 0x%08x unknown\n", __func__, res);
		return -EINVAL;
	}

	if (h_size != NULL)
		*h_size = hsize;
	if (v_size != NULL)
		*v_size = vsize;

	return 0;
}

#ifdef CONFIG_GMIN_INTEL_MID /* FIXME! */
static int mt9m114_get_intg_factor(struct i2c_client *client,
				struct camera_mipi_info *info,
				const struct mt9m114_res_struct *res)
{
	struct atomisp_sensor_mode_data *buf = &info->data;
	u32 reg_val;
	int ret;

	if (info == NULL)
		return -EINVAL;

	ret =  mt9m114_read_reg(client, MISENSOR_32BIT,
					REG_PIXEL_CLK, &reg_val);
	if (ret)
		return ret;
	buf->vt_pix_clk_freq_mhz = reg_val;

	/* get integration time */
	buf->coarse_integration_time_min = MT9M114_COARSE_INTG_TIME_MIN;
	buf->coarse_integration_time_max_margin =
					MT9M114_COARSE_INTG_TIME_MAX_MARGIN;

	buf->fine_integration_time_min = MT9M114_FINE_INTG_TIME_MIN;
	buf->fine_integration_time_max_margin =
					MT9M114_FINE_INTG_TIME_MAX_MARGIN;

	buf->fine_integration_time_def = MT9M114_FINE_INTG_TIME_MIN;

	buf->frame_length_lines = res->lines_per_frame;
	buf->line_length_pck = res->pixels_per_line;
	buf->read_mode = res->bin_mode;

	/* get the cropping and output resolution to ISP for this mode. */
	ret =  mt9m114_read_reg(client, MISENSOR_16BIT,
					REG_H_START, &reg_val);
	if (ret)
		return ret;
	buf->crop_horizontal_start = reg_val;

	ret =  mt9m114_read_reg(client, MISENSOR_16BIT,
					REG_V_START, &reg_val);
	if (ret)
		return ret;
	buf->crop_vertical_start = reg_val;

	ret = mt9m114_read_reg(client, MISENSOR_16BIT,
					REG_H_END, &reg_val);
	if (ret)
		return ret;
	buf->crop_horizontal_end = reg_val;

	ret = mt9m114_read_reg(client, MISENSOR_16BIT,
					REG_V_END, &reg_val);
	if (ret)
		return ret;
	buf->crop_vertical_end = reg_val;

	ret = mt9m114_read_reg(client, MISENSOR_16BIT,
					REG_WIDTH, &reg_val);
	if (ret)
		return ret;
	buf->output_width = reg_val;

	ret = mt9m114_read_reg(client, MISENSOR_16BIT,
					REG_HEIGHT, &reg_val);
	if (ret)
		return ret;
	buf->output_height = reg_val;

	ret = mt9m114_read_reg(client, MISENSOR_16BIT,
					REG_TIMING_HTS, &reg_val);
	if (ret)
		return ret;
	buf->line_length_pck = reg_val;

	ret = mt9m114_read_reg(client, MISENSOR_16BIT,
					REG_TIMING_VTS, &reg_val);
	if (ret)
		return ret;
	buf->frame_length_lines = reg_val;

	buf->binning_factor_x = res->bin_factor_x ?
					res->bin_factor_x : 1;
	buf->binning_factor_y = res->bin_factor_y ?
					res->bin_factor_y : 1;
	return 0;
}
#endif

static int mt9m114_get_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	struct mt9m114_device *dev = to_mt9m114_sensor(sd);
	int width, height;
	int ret;
#ifndef CONFIG_GMIN_INTEL_MID /* FIXME! */
	fmt->code = V4L2_MBUS_FMT_UYVY8_1X16;
#else
	fmt->code = V4L2_MBUS_FMT_SGRBG10_1X10;
#endif

	ret = mt9m114_res2size(dev->res, &width, &height);
	if (ret)
		return ret;
	fmt->width = width;
	fmt->height = height;

	return 0;
}

static int mt9m114_set_mbus_fmt(struct v4l2_subdev *sd,
			      struct v4l2_mbus_framefmt *fmt)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	struct mt9m114_device *dev = to_mt9m114_sensor(sd);
	struct mt9m114_res_struct *res_index;
	u32 width = fmt->width;
	u32 height = fmt->height;
#ifdef CONFIG_GMIN_INTEL_MID /* FIXME! */
	struct camera_mipi_info *mt9m114_info = NULL;
#endif
	int ret;

	dev->streamon = 0;
	dev->first_exp = MT9M114_DEFAULT_FIRST_EXP;

#ifdef CONFIG_GMIN_INTEL_MID /* FIXME! */
	mt9m114_info = v4l2_get_subdev_hostdata(sd);
	if (mt9m114_info == NULL)
		return -EINVAL;
#endif
	mt9m114_try_res(&width, &height);
	res_index = mt9m114_to_res(width, height);

	/* Sanity check */
	if (unlikely(!res_index)) {
		WARN_ON(1);
		return -EINVAL;
	}

	switch (res_index->res) {
#ifndef CONFIG_GMIN_INTEL_MID /* FIXME! */
	case MT9M114_RES_QCIF:
		ret = mt9m114_write_reg_array(c, mt9m114_qcif_init, NO_POLLING);
		break;
	case MT9M114_RES_QVGA:
		ret = mt9m114_write_reg_array(c, mt9m114_qvga_init, NO_POLLING);
		/* set sensor read_mode to Skipping */
		ret += misensor_rmw_reg(c, MISENSOR_16BIT, MISENSOR_READ_MODE,
				MISENSOR_R_MODE_MASK, MISENSOR_SKIPPING_SET);
		break;
	case MT9M114_RES_VGA:
		ret = mt9m114_write_reg_array(c, mt9m114_vga_init, NO_POLLING);
		/* set sensor read_mode to Summing */
		ret += misensor_rmw_reg(c, MISENSOR_16BIT, MISENSOR_READ_MODE,
				MISENSOR_R_MODE_MASK, MISENSOR_SUMMING_SET);
		break;
	case MT9M114_RES_480P:
		ret = mt9m114_write_reg_array(c, mt9m114_480p_init, NO_POLLING);
		break;
	case MT9M114_RES_576P:
		ret = mt9m114_write_reg_array(c, mt9m114_576p_init, NO_POLLING);
		/* set sensor read_mode to Normal */
		ret += misensor_rmw_reg(c, MISENSOR_16BIT, MISENSOR_READ_MODE,
				MISENSOR_R_MODE_MASK, MISENSOR_NORMAL_SET);
		break;
	case MT9M114_RES_720P:
		ret = mt9m114_write_reg_array(c, mt9m114_720p_init, NO_POLLING);
		/* set sensor read_mode to Normal */
		ret += misensor_rmw_reg(c, MISENSOR_16BIT, MISENSOR_READ_MODE,
				MISENSOR_R_MODE_MASK, MISENSOR_NORMAL_SET);
		break;
	case MT9M114_RES_960P:
		ret = mt9m114_write_reg_array(c, mt9m114_960P_init, NO_POLLING);
		/* set sensor read_mode to Normal */
		ret += misensor_rmw_reg(c, MISENSOR_16BIT, MISENSOR_READ_MODE,
				MISENSOR_R_MODE_MASK, MISENSOR_NORMAL_SET);
		break;
#else
	case MT9M114_RES_720_480p_768:
		ret = mt9m114_write_reg_array(c, mt9m114_720_480P_init, NO_POLLING);
		ret += misensor_rmw_reg(c, MISENSOR_16BIT, MISENSOR_READ_MODE,
				MISENSOR_R_MODE_MASK, MISENSOR_NORMAL_SET);
		break;
	case MT9M114_RES_736P:
		ret = mt9m114_write_reg_array(c, mt9m114_736P_init, NO_POLLING);
		ret += misensor_rmw_reg(c, MISENSOR_16BIT, MISENSOR_READ_MODE,
				MISENSOR_R_MODE_MASK, MISENSOR_NORMAL_SET);
		break;
	case MT9M114_RES_864P:
		ret = mt9m114_write_reg_array(c, mt9m114_864P_init, NO_POLLING);
		ret += misensor_rmw_reg(c, MISENSOR_16BIT, MISENSOR_READ_MODE,
				MISENSOR_R_MODE_MASK, MISENSOR_NORMAL_SET);
		break;
	case MT9M114_RES_960P:
		ret = mt9m114_write_reg_array(c, mt9m114_976P_init, NO_POLLING);
		/* set sensor read_mode to Normal */
		ret += misensor_rmw_reg(c, MISENSOR_16BIT, MISENSOR_READ_MODE,
				MISENSOR_R_MODE_MASK, MISENSOR_NORMAL_SET);
		break;
#endif
	default:
		v4l2_err(sd, "set resolution: %d failed!\n", res_index->res);
		return -EINVAL;
	}

	if (ret)
		return -EINVAL;

	ret = mt9m114_write_reg_array(c, mt9m114_chgstat_reg, POST_POLLING);
	if (ret < 0)
		return ret;
#ifndef CONFIG_GMIN_INTEL_MID /* FIXME! */
	if (mt9m114_set_suspend(sd))
		return -EINVAL;
#endif
	if (dev->res != res_index->res) {
		int index;

		/* Switch to different size */
		if (width <= 640) {
			dev->nctx = 0x00; /* Set for context A */
		} else {
			/*
			 * Context B is used for resolutions larger than 640x480
			 * Using YUV for Context B.
			 */
			dev->nctx = 0x01; /* set for context B */
		}

		/*
		 * Marked current sensor res as being "used"
		 *
		 * REVISIT: We don't need to use an "used" field on each mode
		 * list entry to know which mode is selected. If this
		 * information is really necessary, how about to use a single
		 * variable on sensor dev struct?
		 */
		for (index = 0; index < N_RES; index++) {
			if ((width == mt9m114_res[index].width) &&
			    (height == mt9m114_res[index].height)) {
				mt9m114_res[index].used = 1;
				continue;
			}
			mt9m114_res[index].used = 0;
		}
	}
#ifdef CONFIG_GMIN_INTEL_MID /* FIXME! */
	ret = mt9m114_get_intg_factor(c, mt9m114_info,
					&mt9m114_res[res_index->res]);
	if (ret) {
		dev_err(&c->dev, "failed to get integration_factor\n");
		return -EINVAL;
	}
#endif
	/*
	 * mt9m114 - we don't poll for context switch
	 * because it does not happen with streaming disabled.
	 */
	dev->res = res_index->res;

	fmt->width = width;
	fmt->height = height;
#ifdef CONFIG_GMIN_INTEL_MID /* FIXME! */
	fmt->code = V4L2_MBUS_FMT_SGRBG10_1X10;
#else
	fmt->code = V4L2_MBUS_FMT_UYVY8_1X16;
#endif
	return 0;
}

/* TODO: Update to SOC functions, remove exposure and gain */
static int mt9m114_g_focal(struct v4l2_subdev *sd, s32 * val)
{
	*val = (MT9M114_FOCAL_LENGTH_NUM << 16) | MT9M114_FOCAL_LENGTH_DEM;
	return 0;
}

static int mt9m114_g_fnumber(struct v4l2_subdev *sd, s32 * val)
{
	/*const f number for mt9m114*/
	*val = (MT9M114_F_NUMBER_DEFAULT_NUM << 16) | MT9M114_F_NUMBER_DEM;
	return 0;
}

static int mt9m114_g_fnumber_range(struct v4l2_subdev *sd, s32 * val)
{
	*val = (MT9M114_F_NUMBER_DEFAULT_NUM << 24) |
		(MT9M114_F_NUMBER_DEM << 16) |
		(MT9M114_F_NUMBER_DEFAULT_NUM << 8) | MT9M114_F_NUMBER_DEM;
	return 0;
}

/* Horizontal flip the image. */
static int mt9m114_g_hflip(struct v4l2_subdev *sd, s32 * val)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	int ret;
	u32 data;
	ret = mt9m114_read_reg(c, MISENSOR_16BIT,
			(u32)MISENSOR_READ_MODE, &data);
	if (ret)
		return ret;
	*val = !!(data & MISENSOR_HFLIP_MASK);

	return 0;
}

static int mt9m114_g_vflip(struct v4l2_subdev *sd, s32 * val)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	int ret;
	u32 data;

	ret = mt9m114_read_reg(c, MISENSOR_16BIT,
			(u32)MISENSOR_READ_MODE, &data);
	if (ret)
		return ret;
	*val = !!(data & MISENSOR_VFLIP_MASK);

	return 0;
}

#ifndef CONFIG_GMIN_INTEL_MID /* FIXME! for SOC Mode*/
static int mt9m114_s_freq(struct v4l2_subdev *sd, s32  val)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	struct mt9m114_device *dev = to_mt9m114_sensor(sd);
	int ret;

	if (val != MT9M114_FLICKER_MODE_50HZ &&
			val != MT9M114_FLICKER_MODE_60HZ)
		return -EINVAL;

	if (val == MT9M114_FLICKER_MODE_50HZ) {
		ret = mt9m114_write_reg_array(c, mt9m114_antiflicker_50hz,
					POST_POLLING);
		if (ret < 0)
			return ret;
	} else {
		ret = mt9m114_write_reg_array(c, mt9m114_antiflicker_60hz,
					POST_POLLING);
		if (ret < 0)
			return ret;
	}

	if (ret == 0)
		dev->lightfreq = val;

	return ret;
}

static int mt9m114_g_2a_status(struct v4l2_subdev *sd, s32 *val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	unsigned int status_exp, status_wb;
	*val = 0;

	ret = mt9m114_read_reg(client, MISENSOR_16BIT,
			MISENSOR_AE_TRACK_STATUS, &status_exp);
	if (ret)
		return ret;

	ret = mt9m114_read_reg(client, MISENSOR_16BIT,
			MISENSOR_AWB_STATUS, &status_wb);
	if (ret)
		return ret;

	if (status_exp & MISENSOR_AE_READY)
		*val = V4L2_2A_STATUS_AE_READY;

	if (status_wb & MISENSOR_AWB_STEADY)
		*val |= V4L2_2A_STATUS_AWB_READY;

	return 0;
}
#endif
#ifdef CONFIG_GMIN_INTEL_MID /* FIXME! for RAW Mode*/
static long mt9m114_s_exposure(struct v4l2_subdev *sd,
			       struct atomisp_exposure *exposure)
{
    struct i2c_client *client = v4l2_get_subdevdata(sd);
    struct mt9m114_device *dev = to_mt9m114_sensor(sd);
    int ret = 0;
    unsigned int coarse_integration = 0;
    unsigned int fine_integration = 0;
    unsigned int FLines = 0;
    unsigned int FrameLengthLines = 0; //ExposureTime.FrameLengthLines;
    unsigned int AnalogGain, DigitalGain;
    u32 AnalogGainToWrite = 0;
    u16 exposure_local[3];

    dev_dbg(&client->dev, "%s(0x%X 0x%X 0x%X)\n", __func__,
		    exposure->integration_time[0], exposure->gain[0],
		    exposure->gain[1]);

    coarse_integration = exposure->integration_time[0];
//    fine_integration = ExposureTime.FineIntegrationTime;
//    FrameLengthLines = ExposureTime.FrameLengthLines;
    FLines = mt9m114_res[dev->res].lines_per_frame;
    AnalogGain = exposure->gain[0];
    DigitalGain = exposure->gain[1];
	if (!dev->streamon) {
		/*Save the first exposure values while stream is off*/
		dev->first_exp = coarse_integration;
		dev->first_gain = AnalogGain;
		dev->first_diggain = DigitalGain;
	}
    //DigitalGain = 0x400 * (((u16) DigitalGain) >> 8) + ((unsigned int)(0x400 * (((u16) DigitalGain) & 0xFF)) >>8);

    //set frame length
    if (FLines < coarse_integration + 6)
        FLines = coarse_integration + 6;
    if (FLines < FrameLengthLines)
        FLines = FrameLengthLines;
    ret = mt9m114_write_reg(client, MISENSOR_16BIT, 0x300A, FLines);
	if (ret) {
		v4l2_err(client, "%s: fail to set FLines\n", __func__);
		return -EINVAL;
	}

    //set coarse/fine integration
    exposure_local[0] = REG_EXPO_COARSE;
    exposure_local[1] = (u16)coarse_integration;
    exposure_local[2] = (u16)fine_integration;
    // 3A provide real exposure time. should not translate to any value here.
    ret = mt9m114_write_reg(client, MISENSOR_16BIT, REG_EXPO_COARSE, (u16)(coarse_integration));
    if (ret) {
		 v4l2_err(client, "%s: fail to set exposure time\n", __func__);
		 return -EINVAL;
    }

     /*
    // set analog/digital gain
    switch(AnalogGain)
    {
      case 0:
          AnalogGainToWrite = 0x0;
          break;
      case 1:
          AnalogGainToWrite = 0x20;
          break;
      case 2:
          AnalogGainToWrite = 0x60;
          break;
      case 4:
          AnalogGainToWrite = 0xA0;
          break;
      case 8:
          AnalogGainToWrite = 0xE0;
          break;
      default:
          AnalogGainToWrite = 0x20;
          break;
    }
    */
   if (DigitalGain >= 16 || DigitalGain <= 1)
        DigitalGain = 1;
   // AnalogGainToWrite = (u16)((DigitalGain << 12) | AnalogGainToWrite);
   AnalogGainToWrite = (u16)((DigitalGain << 12) | (u16)AnalogGain);
   ret = mt9m114_write_reg(client, MISENSOR_16BIT, REG_GAIN, AnalogGainToWrite);
   if (ret) {
		v4l2_err(client, "%s: fail to set AnalogGainToWrite\n", __func__);
		return -EINVAL;
   }

    return ret;
}

static long mt9m114_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{

	switch (cmd) {
	case ATOMISP_IOC_S_EXPOSURE:
		return mt9m114_s_exposure(sd, arg);
	default:
		return -EINVAL;
	}
	return 0;
}


/* This returns the exposure time being used. This should only be used
   for filling in EXIF data, not for actual image processing. */
static int mt9m114_g_exposure(struct v4l2_subdev *sd, s32 *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u32 coarse;
	int ret;

	/* the fine integration time is currently not calculated */
	ret = mt9m114_read_reg(client, MISENSOR_16BIT,
			       MISENSOR_COARSE_INTEGRATION_TIME, &coarse);
	if (ret)
		return ret;

	*value = coarse;
	return 0;
}
#endif
#ifndef CSS15
/*
 * This function will return the sensor supported max exposure zone number.
 * the sensor which supports max exposure zone number is 1.
 */
static int mt9m114_g_exposure_zone_num(struct v4l2_subdev *sd, s32 *val)
{
	*val = 1;

	return 0;
}

/*
 * set exposure metering, average/center_weighted/spot/matrix.
 */
static int mt9m114_s_exposure_metering(struct v4l2_subdev *sd, s32 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	switch (val) {
	case V4L2_EXPOSURE_METERING_SPOT:
		ret = mt9m114_write_reg_array(client, mt9m114_exp_average, NO_POLLING);
		if (ret) {
			dev_err(&client->dev, "write exp_average reg err.\n");
			return ret;
		}
		break;
	case V4L2_EXPOSURE_METERING_CENTER_WEIGHTED:
	default:
		ret = mt9m114_write_reg_array(client, mt9m114_exp_center, NO_POLLING);
		if (ret) {
			dev_err(&client->dev, "write exp_default reg err");
			return ret;
		}
	}

	return 0;
}

/*
 * This function is for touch exposure feature.
 */
static int mt9m114_s_exposure_selection(struct v4l2_subdev *sd,
					struct v4l2_subdev_fh *fh,
					struct v4l2_subdev_selection *sel)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9m114_device *dev = to_mt9m114_sensor(sd);
	struct misensor_reg exp_reg;
	int width, height;
	int grid_width, grid_height;
	int grid_left, grid_top, grid_right, grid_bottom;
	int win_left, win_top, win_right, win_bottom;
	int i,j;
	int ret;

	if (sel->which != V4L2_SUBDEV_FORMAT_TRY &&
	    sel->which != V4L2_SUBDEV_FORMAT_ACTIVE)
		return -EINVAL;

	grid_left = sel->r.left;
	grid_top = sel->r.top;
	grid_right = sel->r.left + sel->r.width - 1;
	grid_bottom = sel->r.top + sel->r.height - 1;

	ret = mt9m114_res2size(dev->res, &width, &height);
	if (ret)
		return ret;

	grid_width = width / 5;
	grid_height = height / 5;

	if (grid_width && grid_height) {
		win_left = grid_left / grid_width;
		win_top = grid_top / grid_height;
		win_right = grid_right / grid_width;
		win_bottom = grid_bottom / grid_height;
	} else {
		dev_err(&client->dev, "Incorrect exp grid.\n");
		return -EINVAL;
	}

	clamp_t(int, win_left, 0, 4);
	clamp_t(int, win_top, 0, 4);
	clamp_t(int, win_right, 0, 4);
	clamp_t(int, win_bottom, 0, 4);

	ret = mt9m114_write_reg_array(client, mt9m114_exp_average, NO_POLLING);
	if (ret) {
		dev_err(&client->dev, "write exp_average reg err.\n");
		return ret;
	}

	for (i = win_top; i <= win_bottom; i++) {
		for (j = win_left; j <= win_right; j++) {
			exp_reg = mt9m114_exp_win[i][j];

			ret = mt9m114_write_reg(client, exp_reg.length,
						exp_reg.reg, exp_reg.val);
			if (ret) {
				dev_err(&client->dev, "write exp_reg err.\n");
				return ret;
			}
		}
	}

	return 0;
}
#endif

#ifdef CONFIG_GMIN_INTEL_MID /* FIXME! for RAW Mode*/
static int mt9m114_g_bin_factor_x(struct v4l2_subdev *sd, s32 *val)
{
	struct mt9m114_device *dev = to_mt9m114_sensor(sd);

	*val = mt9m114_res[dev->res].bin_factor_x;

	return 0;
}

static int mt9m114_g_bin_factor_y(struct v4l2_subdev *sd, s32 *val)
{
	struct mt9m114_device *dev = to_mt9m114_sensor(sd);

	*val = mt9m114_res[dev->res].bin_factor_y;

	return 0;
}
#endif

static struct mt9m114_control mt9m114_controls[] = {
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
		.query = mt9m114_g_vflip,
		.tweak = mt9m114_t_vflip,
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
		.query = mt9m114_g_hflip,
		.tweak = mt9m114_t_hflip,
	},
	{
		.qc = {
			.id = V4L2_CID_FOCAL_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focal length",
			.minimum = MT9M114_FOCAL_LENGTH_DEFAULT,
			.maximum = MT9M114_FOCAL_LENGTH_DEFAULT,
			.step = 0x01,
			.default_value = MT9M114_FOCAL_LENGTH_DEFAULT,
			.flags = 0,
		},
		.query = mt9m114_g_focal,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number",
			.minimum = MT9M114_F_NUMBER_DEFAULT,
			.maximum = MT9M114_F_NUMBER_DEFAULT,
			.step = 0x01,
			.default_value = MT9M114_F_NUMBER_DEFAULT,
			.flags = 0,
		},
		.query = mt9m114_g_fnumber,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_RANGE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number range",
			.minimum = MT9M114_F_NUMBER_RANGE,
			.maximum =  MT9M114_F_NUMBER_RANGE,
			.step = 0x01,
			.default_value = MT9M114_F_NUMBER_RANGE,
			.flags = 0,
		},
		.query = mt9m114_g_fnumber_range,
	},
#ifndef CONFIG_GMIN_INTEL_MID /* FIXME! for non-gmin*/
	{
		.qc = {
			.id = V4L2_CID_POWER_LINE_FREQUENCY,
			.type = V4L2_CTRL_TYPE_MENU,
			.name = "Light frequency filter",
			.minimum = 1,
			.maximum =  2, /* 1: 50Hz, 2:60Hz */
			.step = 1,
			.default_value = 1,
			.flags = 0,
		},
		.tweak = mt9m114_s_freq,
	},
	{
		.qc = {
			.id = V4L2_CID_2A_STATUS,
			.type = V4L2_CTRL_TYPE_BITMASK,
			.name = "AE and AWB status",
			.minimum = 0,
			.maximum = V4L2_2A_STATUS_AE_READY |
				V4L2_2A_STATUS_AWB_READY,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.query = mt9m114_g_2a_status,
	},
#endif
#ifdef CONFIG_GMIN_INTEL_MID /* FIXME! for RAW Mode*/
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
		.query = mt9m114_g_exposure,
	},
#endif
#ifndef CSS15
	{
		.qc = {
			.id = V4L2_CID_EXPOSURE_ZONE_NUM,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "one-time exposure zone number",
			.minimum = 0x0,
			.maximum = 0xffff,
			.step = 0x01,
			.default_value = 0x00,
			.flags = 0,
		},
		.query = mt9m114_g_exposure_zone_num,
	},
	{
		.qc = {
			.id = V4L2_CID_EXPOSURE_METERING,
			.type = V4L2_CTRL_TYPE_MENU,
			.name = "metering",
			.minimum = 0,
			.maximum = 3,
			.step = 1,
			.default_value = 1,
			.flags = 0,
		},
		.tweak = mt9m114_s_exposure_metering,
	},
#endif
#ifdef CONFIG_GMIN_INTEL_MID /* FIXME! for RAW Mode*/
	{
		.qc = {
			.id = V4L2_CID_BIN_FACTOR_HORZ,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "horizontal binning factor",
			.minimum = 0,
			.maximum = MT9M114_BIN_FACTOR_MAX,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.query = mt9m114_g_bin_factor_x,
	},
	{
		.qc = {
			.id = V4L2_CID_BIN_FACTOR_VERT,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "vertical binning factor",
			.minimum = 0,
			.maximum = MT9M114_BIN_FACTOR_MAX,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.query = mt9m114_g_bin_factor_y,
	},
#endif
};
#define N_CONTROLS (ARRAY_SIZE(mt9m114_controls))

static struct mt9m114_control *mt9m114_find_control(__u32 id)
{
	int i;

	for (i = 0; i < N_CONTROLS; i++) {
		if (mt9m114_controls[i].qc.id == id)
			return &mt9m114_controls[i];
	}
	return NULL;
}

static int mt9m114_detect(struct mt9m114_device *dev, struct i2c_client *client)
{
	struct i2c_adapter *adapter = client->adapter;
	u32 retvalue;

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "%s: i2c error", __func__);
		return -ENODEV;
	}
	mt9m114_read_reg(client, MISENSOR_16BIT, (u32)MT9M114_PID, &retvalue);
	dev->real_model_id = retvalue;

	if (retvalue != MT9M114_MOD_ID) {
		dev_err(&client->dev, "%s: failed: client->addr = %x\n",
			__func__, client->addr);
		return -ENODEV;
	}

	return 0;
}

static int
mt9m114_s_config(struct v4l2_subdev *sd, int irq, void *platform_data)
{
	struct mt9m114_device *dev = to_mt9m114_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	if (NULL == platform_data)
		return -ENODEV;

	dev->platform_data =
	    (struct camera_sensor_platform_data *)platform_data;

	if (dev->platform_data->platform_init) {
		ret = dev->platform_data->platform_init(client);
		if (ret) {
			v4l2_err(client, "mt9m114 platform init err\n");
			return ret;
		}
	}
	ret = power_up(sd);
	if (ret) {
		v4l2_err(client, "mt9m114 power-up err");
		return ret;
	}

	/* config & detect sensor */
	ret = mt9m114_detect(dev, client);
	if (ret) {
		v4l2_err(client, "mt9m114_detect err s_config.\n");
		goto fail_detect;
	}

	ret = dev->platform_data->csi_cfg(sd, 1);
	if (ret)
		goto fail_csi_cfg;

	ret = mt9m114_set_suspend(sd);
	if (ret) {
		v4l2_err(client, "mt9m114 suspend err");
		return ret;
	}

	ret = power_down(sd);
	if (ret) {
		v4l2_err(client, "mt9m114 power down err");
		return ret;
	}

	return ret;

fail_csi_cfg:
	dev->platform_data->csi_cfg(sd, 0);
fail_detect:
	power_down(sd);
	dev_err(&client->dev, "sensor power-gating failed\n");
	return ret;
}

static int mt9m114_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	struct mt9m114_control *ctrl = mt9m114_find_control(qc->id);

	if (ctrl == NULL)
		return -EINVAL;
	*qc = ctrl->qc;
	return 0;
}

/* Horizontal flip the image. */
static int mt9m114_t_hflip(struct v4l2_subdev *sd, int value)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	struct mt9m114_device *dev = to_mt9m114_sensor(sd);
	int err;
	/* set for direct mode */
	err = mt9m114_write_reg(c, MISENSOR_16BIT, 0x098E, 0xC850);
	if (value) {
		/* enable H flip ctx A */
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC850, 0x01, 0x01);
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC851, 0x01, 0x01);
		/* ctx B */
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC888, 0x01, 0x01);
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC889, 0x01, 0x01);

		err += misensor_rmw_reg(c, MISENSOR_16BIT, MISENSOR_READ_MODE,
					MISENSOR_HFLIP_MASK, MISENSOR_FLIP_EN);

		dev->bpat = MT9M114_BPAT_GRGRBGBG;
	} else {
		/* disable H flip ctx A */
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC850, 0x01, 0x00);
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC851, 0x01, 0x00);
		/* ctx B */
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC888, 0x01, 0x00);
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC889, 0x01, 0x00);

		err += misensor_rmw_reg(c, MISENSOR_16BIT, MISENSOR_READ_MODE,
					MISENSOR_HFLIP_MASK, MISENSOR_FLIP_DIS);

		dev->bpat = MT9M114_BPAT_BGBGGRGR;
	}

	err += mt9m114_write_reg(c, MISENSOR_8BIT, 0x8404, 0x06);
	udelay(10);

	return !!err;
}

/* Vertically flip the image */
static int mt9m114_t_vflip(struct v4l2_subdev *sd, int value)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	int err;
	/* set for direct mode */
	err = mt9m114_write_reg(c, MISENSOR_16BIT, 0x098E, 0xC850);
	if (value >= 1) {
		/* enable H flip - ctx A */
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC850, 0x02, 0x01);
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC851, 0x02, 0x01);
		/* ctx B */
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC888, 0x02, 0x01);
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC889, 0x02, 0x01);

		err += misensor_rmw_reg(c, MISENSOR_16BIT, MISENSOR_READ_MODE,
					MISENSOR_VFLIP_MASK, MISENSOR_FLIP_EN);
	} else {
		/* disable H flip - ctx A */
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC850, 0x02, 0x00);
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC851, 0x02, 0x00);
		/* ctx B */
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC888, 0x02, 0x00);
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC889, 0x02, 0x00);

		err += misensor_rmw_reg(c, MISENSOR_16BIT, MISENSOR_READ_MODE,
					MISENSOR_VFLIP_MASK, MISENSOR_FLIP_DIS);
	}

	err += mt9m114_write_reg(c, MISENSOR_8BIT, 0x8404, 0x06);
	udelay(10);

	return !!err;
}
#ifdef CONFIG_GMIN_INTEL_MID /* FIXME! for RAW Mode*/
static int mt9m114_s_parm(struct v4l2_subdev *sd,
			struct v4l2_streamparm *param)
{
	return 0;
}

static int mt9m114_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *interval)
{
	struct mt9m114_device *dev = to_mt9m114_sensor(sd);

	interval->interval.numerator = 1;
	interval->interval.denominator = mt9m114_res[dev->res].fps;

	return 0;
}
#endif
static int mt9m114_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct mt9m114_control *octrl = mt9m114_find_control(ctrl->id);
	int ret;

	if (octrl == NULL)
		return -EINVAL;

	ret = octrl->query(sd, &ctrl->value);
	if (ret < 0)
		return ret;

	return 0;
}

static int mt9m114_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct mt9m114_control *octrl = mt9m114_find_control(ctrl->id);
	int ret;

	if (!octrl || !octrl->tweak)
		return -EINVAL;

	ret = octrl->tweak(sd, ctrl->value);
	if (ret < 0)
		return ret;

	return 0;
}

static int mt9m114_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret;
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	struct mt9m114_device *dev = to_mt9m114_sensor(sd);
	struct atomisp_exposure exposure;

	if (enable) {
		ret = mt9m114_write_reg_array(c, mt9m114_chgstat_reg,
					POST_POLLING);
		if (ret < 0)
			return ret;

		if (dev->first_exp > MT9M114_MAX_FIRST_EXP) {
			exposure.integration_time[0] = dev->first_exp;
			exposure.gain[0] = dev->first_gain;
			exposure.gain[1] = dev->first_diggain;
			mt9m114_s_exposure(sd, &exposure);
		}
		dev->streamon = 1;

#ifndef CONFIG_GMIN_INTEL_MID /* FIXME! for irrational!*/
		ret = mt9m114_set_streaming(sd);
#endif
	} else {
		dev->streamon = 0;
		ret = mt9m114_set_suspend(sd);
	}

	return ret;
}

static int
mt9m114_enum_framesizes(struct v4l2_subdev *sd, struct v4l2_frmsizeenum *fsize)
{
	unsigned int index = fsize->index;

	if (index >= N_RES)
		return -EINVAL;

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = mt9m114_res[index].width;
	fsize->discrete.height = mt9m114_res[index].height;

	/* FIXME: Wrong way to know used mode */
	fsize->reserved[0] = mt9m114_res[index].used;

	return 0;
}

static int mt9m114_enum_frameintervals(struct v4l2_subdev *sd,
				       struct v4l2_frmivalenum *fival)
{
	unsigned int index = fival->index;
	int i;

	if (index >= N_RES)
		return -EINVAL;

	/* find out the first equal or bigger size */
	for (i = 0; i < N_RES; i++) {
		if ((mt9m114_res[i].width >= fival->width) &&
		    (mt9m114_res[i].height >= fival->height))
			break;
	}
	if (i == N_RES)
		i--;

	index = i;

	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fival->discrete.numerator = 1;
	fival->discrete.denominator = mt9m114_res[index].fps;

	return 0;
}
#ifndef CONFIG_GMIN_INTEL_MID /* FIXME! */
static int
mt9m114_g_chip_ident(struct v4l2_subdev *sd, struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_MT9M114, 0);
}
#endif
static int mt9m114_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_fh *fh,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index)
		return -EINVAL;
#ifndef CONFIG_GMIN_INTEL_MID /* FIXME! for RAW Mode*/
	code->code = V4L2_MBUS_FMT_UYVY8_1X16;
#else
	code->code = V4L2_MBUS_FMT_SGRBG10_1X10;
#endif

	return 0;
}

static int mt9m114_enum_frame_size(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh,
	struct v4l2_subdev_frame_size_enum *fse)
{

	unsigned int index = fse->index;


	if (index >= N_RES)
		return -EINVAL;

	fse->min_width = mt9m114_res[index].width;
	fse->min_height = mt9m114_res[index].height;
	fse->max_width = mt9m114_res[index].width;
	fse->max_height = mt9m114_res[index].height;

	return 0;
}

static struct v4l2_mbus_framefmt *
__mt9m114_get_pad_format(struct mt9m114_device *sensor,
			 struct v4l2_subdev_fh *fh, unsigned int pad,
			 enum v4l2_subdev_format_whence which)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->sd);

	if (pad != 0) {
		dev_err(&client->dev,  "%s err. pad %x\n", __func__, pad);
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

static int
mt9m114_get_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct mt9m114_device *snr = to_mt9m114_sensor(sd);
	struct v4l2_mbus_framefmt *format =
			__mt9m114_get_pad_format(snr, fh, fmt->pad, fmt->which);

	if (format == NULL)
		return -EINVAL;
	fmt->format = *format;

	return 0;
}

static int
mt9m114_set_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct mt9m114_device *snr = to_mt9m114_sensor(sd);
	struct v4l2_mbus_framefmt *format =
			__mt9m114_get_pad_format(snr, fh, fmt->pad, fmt->which);

	if (format == NULL)
		return -EINVAL;

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		snr->format = fmt->format;

	return 0;
}

static int mt9m114_g_skip_frames(struct v4l2_subdev *sd, u32 *frames)
{
	int index;
	struct mt9m114_device *snr = to_mt9m114_sensor(sd);

	if (frames == NULL)
		return -EINVAL;

	for (index = 0; index < N_RES; index++) {
		if (mt9m114_res[index].res == snr->res)
			break;
	}

	if (index >= N_RES)
		return -EINVAL;

	*frames = mt9m114_res[index].skip_frames;

	return 0;
}
static const struct v4l2_subdev_video_ops mt9m114_video_ops = {
#ifdef CONFIG_GMIN_INTEL_MID 
	.s_parm = mt9m114_s_parm,
#endif
	.try_mbus_fmt = mt9m114_try_mbus_fmt,
	.s_mbus_fmt = mt9m114_set_mbus_fmt,
	.g_mbus_fmt = mt9m114_get_mbus_fmt,
	.s_stream = mt9m114_s_stream,
	.enum_framesizes = mt9m114_enum_framesizes,
	.enum_frameintervals = mt9m114_enum_frameintervals,
#ifdef CONFIG_GMIN_INTEL_MID 
	.g_frame_interval = mt9m114_g_frame_interval,
#endif
};

static struct v4l2_subdev_sensor_ops mt9m114_sensor_ops = {
	.g_skip_frames	= mt9m114_g_skip_frames,
};

static const struct v4l2_subdev_core_ops mt9m114_core_ops = {
#ifndef CONFIG_GMIN_INTEL_MID 
	.g_chip_ident = mt9m114_g_chip_ident,
#endif
	.queryctrl = mt9m114_queryctrl,
	.g_ctrl = mt9m114_g_ctrl,
	.s_ctrl = mt9m114_s_ctrl,
	.s_power = mt9m114_s_power,
#ifdef CONFIG_GMIN_INTEL_MID 
	.ioctl = mt9m114_ioctl,
#endif
};

/* REVISIT: Do we need pad operations? */
static const struct v4l2_subdev_pad_ops mt9m114_pad_ops = {
	.enum_mbus_code = mt9m114_enum_mbus_code,
	.enum_frame_size = mt9m114_enum_frame_size,
	.get_fmt = mt9m114_get_pad_format,
	.set_fmt = mt9m114_set_pad_format,
#ifndef CSS15
	.set_selection = mt9m114_s_exposure_selection,
#endif
};

static const struct v4l2_subdev_ops mt9m114_ops = {
	.core = &mt9m114_core_ops,
	.video = &mt9m114_video_ops,
	.pad = &mt9m114_pad_ops,
	.sensor = &mt9m114_sensor_ops,
};

static const struct media_entity_operations mt9m114_entity_ops = {
	.link_setup = NULL,
};


static int mt9m114_remove(struct i2c_client *client)
{
	struct mt9m114_device *dev;
	struct v4l2_subdev *sd = i2c_get_clientdata(client);

	dev = container_of(sd, struct mt9m114_device, sd);
	dev->platform_data->csi_cfg(sd, 0);
	if (dev->platform_data->platform_deinit)
		dev->platform_data->platform_deinit();
	v4l2_device_unregister_subdev(sd);
	media_entity_cleanup(&dev->sd.entity);
	kfree(dev);
	return 0;
}

static int mt9m114_probe(struct i2c_client *client,
		       const struct i2c_device_id *id)
{
	struct mt9m114_device *dev;
	int ret = 0;
#ifdef CONFIG_GMIN_INTEL_MID 
	void *pdata;
#endif
	/* Setup sensor configuration structure */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&client->dev, "out of memory\n");
		return -ENOMEM;
	}

	v4l2_i2c_subdev_init(&dev->sd, client, &mt9m114_ops);
#ifdef CONFIG_GMIN_INTEL_MID 
	pdata = client->dev.platform_data;
	if (ACPI_COMPANION(&client->dev))
		pdata = gmin_camera_platform_data(&dev->sd,
						  ATOMISP_INPUT_FORMAT_RAW_10,
						  atomisp_bayer_order_grbg);
	if (pdata)
		ret = mt9m114_s_config(&dev->sd, client->irq, pdata);
	if (!pdata || ret) {
		v4l2_device_unregister_subdev(&dev->sd);
		kfree(dev);
		return ret;
	}

	ret = atomisp_register_i2c_module(&dev->sd, pdata, RAW_CAMERA);
	if (ret) {
		v4l2_device_unregister_subdev(&dev->sd);
		kfree(dev);
	}
#else
	if (client->dev.platform_data) {
		ret = mt9m114_s_config(&dev->sd, client->irq,
				       client->dev.platform_data);
		if (ret) {
			v4l2_device_unregister_subdev(&dev->sd);
			kfree(dev);
			return ret;
		}
	}
#endif
	/*TODO add format code here*/
	dev->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->pad.flags = MEDIA_PAD_FL_SOURCE;
#ifdef CONFIG_GMIN_INTEL_MID 
	dev->format.code = V4L2_MBUS_FMT_SGRBG10_1X10;
	dev->sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
#endif
	/* REVISIT: Do we need media controller? */
	ret = media_entity_init(&dev->sd.entity, 1, &dev->pad, 0);
	if (ret) {
		mt9m114_remove(client);
		return ret;
	}
#ifndef CONFIG_GMIN_INTEL_MID 
	/* set res index to be invalid */
	dev->res = -1;
#endif
	return 0;
}

MODULE_DEVICE_TABLE(i2c, mt9m114_id);
#ifdef CONFIG_GMIN_INTEL_MID 
static struct acpi_device_id mt9m114_acpi_match[] = {
	{ "INT33F0" },
	{ "CRMT1040" },
        {},
};
MODULE_DEVICE_TABLE(acpi, mt9m114_acpi_match);
#endif
static struct i2c_driver mt9m114_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "mt9m114",
#ifdef CONFIG_GMIN_INTEL_MID 
		.acpi_match_table = ACPI_PTR(mt9m114_acpi_match),
#endif
	},
	.probe = mt9m114_probe,
	.remove = mt9m114_remove,
	.id_table = mt9m114_id,
};

static __init int init_mt9m114(void)
{
	return i2c_add_driver(&mt9m114_driver);
}

static __exit void exit_mt9m114(void)
{
	i2c_del_driver(&mt9m114_driver);
}

module_init(init_mt9m114);
module_exit(exit_mt9m114);

MODULE_AUTHOR("Shuguang Gong <Shuguang.gong@intel.com>");
MODULE_LICENSE("GPL");
