/*
 * Support for GalaxyCore GC2155 2M camera sensor.
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
#include <linux/acpi.h>
#include <linux/atomisp_gmin_platform.h>

#include "gc2155.h"

static struct kobject *front_camera_dev_info_kobj;
static u16 front_camera_sensorid = 0;

static ssize_t sensor_vendor_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	char *str = buf;
	str += sprintf(str, "%s\n", "GalaxyCore");
	return (str - buf);
}

static ssize_t sensor_id_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	char *str = buf;
	str += sprintf(str, "%s%x\n", "GC", front_camera_sensorid);
	return (str - buf);
}

static struct kobj_attribute sensor_vendor_attr = {
	.attr = {
		.name = "vendor",
		.mode = 0644,
	},
	.show = sensor_vendor_show,
};

static struct kobj_attribute sensor_id_attr = {
	.attr = {
		.name = "sensor_id",
		.mode = 0644,
	},
	.show = sensor_id_show,
};

static struct attribute * sensor_group[] = {
	&sensor_vendor_attr.attr,
	&sensor_id_attr.attr,
	NULL,
};

static struct attribute_group sensor_attr_group = {
	.attrs = sensor_group,
};

/* i2c read/write stuff */
static int gc2155_read_reg(struct i2c_client *client,
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

	if (data_length != GC2155_8BIT) {
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

	msg[1].addr = client->addr;
	msg[1].len = data_length;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = data+1;

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
	if (data_length == GC2155_8BIT)
		*val = (u8)data[1];

	return 0;
}

static int gc2155_i2c_write(struct i2c_client *client, u16 len, u8 *data)
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

static int gc2155_write_reg(struct i2c_client *client, u16 data_length,
		u8 reg, u8 val)
{
	int ret;
	unsigned char data[2] = {0};
	u8 *wreg = (u8 *)data;
	const u16 len = data_length + sizeof(u8); /* 8-bit address + data */

	if (data_length != GC2155_8BIT) {
		dev_err(&client->dev,
				"%s error, invalid data_length\n", __func__);
		return -EINVAL;
	}

	/* high byte goes out first */
	*wreg = (u8)(reg & 0xff);

	if (data_length == GC2155_8BIT) {
		data[1] = (u8)(val);
	}

	ret = gc2155_i2c_write(client, len, data);
	if (ret)
		dev_err(&client->dev,
				"write error: wrote 0x%x to offset 0x%x error %d",
				val, reg, ret);

	return ret;
}

/*
 * gc2155_write_reg_array - Initializes a list of GC2155 registers
 * @client: i2c driver client structure
 * @reglist: list of registers to be written
 *
 * This function initializes a list of registers. When consecutive addresses
 * are found in a row on the list, this function creates a buffer and sends
 * consecutive data in a single i2c_transfer().
 *
 * __gc2155_flush_reg_array, __gc2155_buf_reg_array() and
 * __gc2155_write_reg_is_consecutive() are internal functions to
 * gc2155_write_reg_array_fast() and should be not used anywhere else.
 *
 */

static int __gc2155_flush_reg_array(struct i2c_client *client,
		struct gc2155_write_ctrl *ctrl)
{
	u16 size;

	if (ctrl->index == 0)
		return 0;

	size = sizeof(u8) + ctrl->index; /* 8-bit address + data */
	ctrl->buffer.addr = (u8)(ctrl->buffer.addr);
	ctrl->index = 0;

	return gc2155_i2c_write(client, size, (u8 *)&ctrl->buffer);
}

static int __gc2155_buf_reg_array(struct i2c_client *client,
		struct gc2155_write_ctrl *ctrl,
		const struct gc2155_reg *next)
{
	int size;

	switch (next->type) {
		case GC2155_8BIT:
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
	if (ctrl->index + sizeof(u8) >= GC2155_MAX_WRITE_BUF_SIZE)
		return __gc2155_flush_reg_array(client, ctrl);

	return 0;
}

static int __gc2155_write_reg_is_consecutive(struct i2c_client *client,
		struct gc2155_write_ctrl *ctrl,
		const struct gc2155_reg *next)
{
	if (ctrl->index == 0)
		return 1;

	return ctrl->buffer.addr + ctrl->index == next->reg;
}

static int gc2155_write_reg_array(struct i2c_client *client,
		const struct gc2155_reg *reglist)
{
	const struct gc2155_reg *next = reglist;
	struct gc2155_write_ctrl ctrl;
	int err;

	ctrl.index = 0;
	for (; next->type != GC2155_TOK_TERM; next++) {
		switch (next->type & GC2155_TOK_MASK) {
			case GC2155_TOK_DELAY:
				err = __gc2155_flush_reg_array(client, &ctrl);
				if (err)
					return err;
				msleep(next->val);
				break;
			default:
				/*
				 * If next address is not consecutive, data needs to be
				 * flushed before proceed.
				 */
				if (!__gc2155_write_reg_is_consecutive(client, &ctrl,
							next)) {
					err = __gc2155_flush_reg_array(client, &ctrl);
					if (err)
						return err;
				}
				err = __gc2155_buf_reg_array(client, &ctrl, next);
				if (err) {
					dev_err(&client->dev, "%s: write error, aborted\n",
							__func__);
					return err;
				}
				break;
		}
	}

	return __gc2155_flush_reg_array(client, &ctrl);
}
static int gc2155_g_focal(struct v4l2_subdev *sd, s32 *val)
{
	*val = (GC2155_FOCAL_LENGTH_NUM << 16) | GC2155_FOCAL_LENGTH_DEM;
	return 0;
}

static int gc2155_g_fnumber(struct v4l2_subdev *sd, s32 *val)
{
	/*const f number for imx*/
	*val = (GC2155_F_NUMBER_DEFAULT_NUM << 16) | GC2155_F_NUMBER_DEM;
	return 0;
}

static int gc2155_g_fnumber_range(struct v4l2_subdev *sd, s32 *val)
{
	*val = (GC2155_F_NUMBER_DEFAULT_NUM << 24) |
		(GC2155_F_NUMBER_DEM << 16) |
		(GC2155_F_NUMBER_DEFAULT_NUM << 8) | GC2155_F_NUMBER_DEM;
	return 0;
}

static int gc2155_g_bin_factor_x(struct v4l2_subdev *sd, s32 *val)
{
	struct gc2155_device *dev = to_gc2155_sensor(sd);

	*val = gc2155_res[dev->fmt_idx].bin_factor_x;

	return 0;
}

static int gc2155_g_bin_factor_y(struct v4l2_subdev *sd, s32 *val)
{
	struct gc2155_device *dev = to_gc2155_sensor(sd);

	*val = gc2155_res[dev->fmt_idx].bin_factor_y;

	return 0;
}

static int gc2155_get_intg_factor(struct i2c_client *client,
		struct camera_mipi_info *info,
		const struct gc2155_resolution *res)
{
	struct atomisp_sensor_mode_data *buf = &info->data;
	unsigned int mclk_freq_hz = 19200000;
	unsigned int hb, vb, sh_delay;
	u8 pll_div;
	u8 reg_val, reg_val2;
	int ret;

	if (info == NULL)
		return -EINVAL;

	ret = gc2155_read_reg(client, GC2155_8BIT,  REG_RST_AND_PG_SELECT, &reg_val);
	if (ret)
		return ret;
	pr_info("page = %d\n", reg_val);

	/* vt_pix_clk_freq_mhz. TODO: don't know the real value exactly. */
	ret = gc2155_read_reg(client, GC2155_8BIT,  0xf8, &pll_div);
	if (ret)
		return ret;

	pll_div = pll_div & 0x1f;
	pr_info("pll_div = %d\n",pll_div);

	buf->vt_pix_clk_freq_mhz = (mclk_freq_hz >> 1) * (pll_div + 1);
	pr_info("vt_pix_clk_freq_mhz = %u\n", buf->vt_pix_clk_freq_mhz);

	ret = gc2155_write_reg(client, GC2155_8BIT, REG_RST_AND_PG_SELECT, 0x0);
	if (ret) {
		dev_err(&client->dev,"Page switch fail\n");
		return ret;
	}

	/* get integration time. DIT doesn't use these vales. */
	buf->coarse_integration_time_min = GC2155_COARSE_INTG_TIME_MIN;
	buf->coarse_integration_time_max_margin =
		GC2155_COARSE_INTG_TIME_MAX_MARGIN;

	buf->fine_integration_time_min = GC2155_FINE_INTG_TIME_MIN;
	buf->fine_integration_time_max_margin =
		GC2155_FINE_INTG_TIME_MAX_MARGIN;

	buf->fine_integration_time_def = GC2155_FINE_INTG_TIME_MIN;

	/* crop_horizontal_start */
	ret = gc2155_read_reg(client, GC2155_8BIT, REG_COL_START_H, &reg_val);
	ret |= gc2155_read_reg(client, GC2155_8BIT, REG_COL_START_L, &reg_val2);

	if (ret) {
		pr_info("Read COL_START fail\n");
		return ret;
	} else {
		buf->crop_horizontal_start = ((reg_val << 8) & 0x0700) | reg_val2;
		pr_info("crop_horizontal_start = %d\n", buf->crop_horizontal_start);
	}

	/* crop_vertical_start */
	ret = gc2155_read_reg(client, GC2155_8BIT, REG_ROW_START_H, &reg_val);
	ret |= gc2155_read_reg(client, GC2155_8BIT, REG_ROW_START_L, &reg_val2);

	if (ret) {
		pr_info("Read ROW_START fail\n");
		return ret;
	} else {
		buf->crop_vertical_start = ((reg_val << 8) & 0x0700) | reg_val2;
		pr_info("crop_vertical_start = %d\n", buf->crop_vertical_start);
	}

	/* output_width */
	ret = gc2155_read_reg(client, GC2155_8BIT, REG_WIN_WIDTH_H, &reg_val);
	ret |= gc2155_read_reg(client, GC2155_8BIT, REG_WIN_WIDTH_L, &reg_val2);
	if (ret) {
		pr_info("Read WIN_WIDTH fail\n");
		return ret;
	} else {
		buf->output_width = ((reg_val << 8) & 0x0700) | reg_val2;
		pr_info("output_width = %d\n", buf->output_width);
	}

	/* crop_horizontal_end */
	buf->crop_horizontal_end =
		buf->crop_horizontal_start + buf->output_width - 1;
	pr_info("crop_horizontal_end = %d\n",buf->crop_horizontal_end);

	/* output_height */
	ret = gc2155_read_reg(client, GC2155_8BIT, REG_WIN_HEIGHT_H, &reg_val);
	ret |= gc2155_read_reg(client, GC2155_8BIT, REG_WIN_HEIGHT_L, &reg_val2);
	if (ret) {
		pr_info("Read WIN_HEIGHT fail\n");
		return ret;
	} else {
		buf->output_height = ((reg_val << 8) & 0x0700) | reg_val2;
		pr_info("output_height = %d\n", buf->output_height);
	}

	/* crop_vertical_end */
	buf->crop_vertical_end =
		buf->crop_vertical_start + buf->output_height - 1;
	pr_info("crop_vertical_end = %d\n", buf->crop_vertical_end);

	/* H Blank */
	ret = gc2155_read_reg(client, GC2155_8BIT, REG_H_BLANK_H, &reg_val);
	ret |= gc2155_read_reg(client, GC2155_8BIT, REG_H_BLANK_L, &reg_val2);
	if (ret) {
		pr_info("Read H_BLANK fail\n");
		return ret;
	} else {
		hb = ((reg_val << 8) & 0x0F00) | reg_val2;
		pr_info("hb = %d\n", hb);
	}

	/* Sh_delay */
	ret = gc2155_read_reg(client, GC2155_8BIT, REG_SH_DELAY_H, &reg_val);
	ret |= gc2155_read_reg(client, GC2155_8BIT, REG_SH_DELAY_L, &reg_val2);
	if (ret) {
		pr_info("Read SH_DELAY fail\n");
		return ret;
	} else {
		sh_delay = ((reg_val << 8) & 0x0300) | reg_val2;
		pr_info("sh_delay = %d\n", sh_delay);
	}

	/* line_length_pck(row_time):
	 *  row_time = Hb + Sh_delay + win_width + 4.
	 *
	 *   Hb: HBlank or dummy pixel, Setting by register P0:0x05 and P0:0x06.
	 *   Sh_delay: Setting by registerP0:0x11[9:8], P0:0x12[7:0].
	 *   win_width: Setting by register 0x0f and P0:0x10, win_width = 1600,
	 *   final_output_width + 8. So for UXGA, we should set win_width as 1616.
	 */
	buf->line_length_pck = (hb + sh_delay + (buf->output_width + 16)/2 + 4) << 1;
	pr_info("line_length_pck = %d\n", buf->line_length_pck);

	/* V Blank */
	ret = gc2155_read_reg(client, GC2155_8BIT, REG_V_BLANK_H, &reg_val);
	ret |= gc2155_read_reg(client, GC2155_8BIT, REG_V_BLANK_L, &reg_val2);
	if (ret) {
		pr_info("Read V_BLANK fail\n");
		return ret;
	} else {
		vb = ((reg_val << 8) & 0x1F00) | reg_val2;
		pr_info("vb = %d\n", vb);
	}

	/* frame_length_lines (Frame time, Ft)
	 * Ft = VB + Vt + 8 (unit is row_time)
	 *  VB = Bt + St + Et, Vblank/Dummy line, from P0:0x07 and P0:0x08.
	 *   Bt: Blank time, VSYNC no active time.
	 *   St: Start time, setting by register P0:0x13
	 *   Et: End time, setting by register P0:0x14
	 *  Vt: valid line time. UXGA is 1200, Vt = win_height - 8, win_height
	 *      is setting by register P0:0x0d and P0:0x0e(1232).
	 */
	buf->frame_length_lines = vb + buf->output_height;
	pr_info("frame_length_lines = %d\n",buf->frame_length_lines);

	buf->read_mode = res->bin_mode;
	buf->binning_factor_x = res->bin_factor_x;
	buf->binning_factor_y = res->bin_factor_x;

	return 0;
}

static long gc2155_s_exposure(struct v4l2_subdev *sd,
					struct atomisp_exposure *exposure)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u8 tmp, lsc;
	int ret = 0;

	unsigned int coarse_integration = 0;

	//unsigned int AnalogGain, DigitalGain;

	u8 expo_coarse_h,expo_coarse_l;
	unsigned int gain, a_gain, d_gain;

	dev_err(&client->dev, "%s(0x%X 0x%X 0x%X)\n", __func__,
		exposure->integration_time[0], exposure->gain[0], exposure->gain[1]);

	coarse_integration = exposure->integration_time[0];


	gain = exposure->gain[0];
	
	if (gain < 16) gain = 16;
	if (gain > 255) gain = 255;

	if (gain < 23) {
		a_gain = 0x0; //0x25 analog gain
		d_gain = (gain*2  - 32) | 0x20; //0xb1 digital gain
	} else if (gain < 32) {
		a_gain = 0x1;
		d_gain = ((gain* 10 / 7) - 32) | 0x20;
	} else if (gain < 46) {
		a_gain = 0x2;
		d_gain = (gain - 32) | 0x20;
	} else if (gain < 64) {
		a_gain = 0x3;
		d_gain = ((gain* 10 / 14) - 32) | 0x20;
	} else if (gain < 91) {
		a_gain = 0x4;
		d_gain = ((gain / 2) - 32) | 0x20;
	} else if (gain < 128) {
		a_gain = 0x5;
		d_gain = ((gain* 10 / 28) - 32) | 0x20;
	} else if (gain < 192) {
		a_gain = 0x6;
		d_gain = ((gain / 4) - 32) | 0x20;
	} else if (gain < 256) {
		a_gain = 0x6;
		d_gain = ((gain / 4) - 48) | 0x30;
	}
	printk("%s real %d a_gain %x d_gain %x\n", __func__, gain, a_gain, d_gain);

	expo_coarse_h = (u8)((coarse_integration >> 8) & 0x1F);
	expo_coarse_l = (u8)(coarse_integration & 0xff);


	ret = gc2155_read_reg(client, GC2155_8BIT,  REG_RST_AND_PG_SELECT, &tmp);


	ret = gc2155_write_reg(client, GC2155_8BIT, REG_RST_AND_PG_SELECT, 0x0);

	ret = gc2155_read_reg(client, GC2155_8BIT,  REG_RST_AND_PG_SELECT, &tmp);


	ret = gc2155_write_reg(client, GC2155_8BIT, REG_EXPO_COARSE_H, expo_coarse_h);
	ret = gc2155_write_reg(client, GC2155_8BIT, REG_EXPO_COARSE_L, expo_coarse_l);
	ret = gc2155_read_reg(client, GC2155_8BIT,  0x80, &lsc);

	if (ret) {
		 v4l2_err(client, "%s: fail to set exposure time\n", __func__);
		 return -EINVAL;
	}

	/* Set Digital gain
	 * Controlled by AEC, can be manually controlled when disable AEC
	 * P0:0xb1 Auto_pregain
	 * P0:0xb2 Auto_postgain
	 */
	/* Set Analog Gain
	 *  Aec close: p0:0xb6 [0]
	 *  Set gain to P0:0x25
	 *  000: 1X
	 *  001: 1.4X
	 *  010: 2X
	 *  011: 2.8X
	 *  100: 4X
	 */

	gc2155_write_reg(client, GC2155_8BIT, 0x25, a_gain);
	gc2155_write_reg(client, GC2155_8BIT, 0xb1, d_gain);

	if (ret) {
		v4l2_err(client, "%s: fail to set AnalogGainToWrite\n", __func__);
		return -EINVAL;
	}

	return ret;
}

/* TO DO */
static int gc2155_v_flip(struct v4l2_subdev *sd, s32 value)
{
	return 0;
}

/* TO DO */
static int gc2155_h_flip(struct v4l2_subdev *sd, s32 value)
{
	return 0;
}

static long gc2155_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{

	switch (cmd) {
		case ATOMISP_IOC_S_EXPOSURE:
			return gc2155_s_exposure(sd, arg);
		default:
			return -EINVAL;
	}
	return 0;
}

/* This returns the exposure time being used. This should only be used
   for filling in EXIF data, not for actual image processing. */
static int gc2155_q_exposure(struct v4l2_subdev *sd, s32 *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u8 reg_v;
	int ret;

	/* get exposure */
	ret = gc2155_read_reg(client, GC2155_8BIT,
			GC2155_AEC_PK_EXPO_L,
			&reg_v);
	if (ret)
		goto err;

	*value = reg_v;
	ret = gc2155_read_reg(client, GC2155_8BIT,
			GC2155_AEC_PK_EXPO_H,
			&reg_v);
	if (ret)
		goto err;

	*value = *value + (reg_v << 8);
	//pr_info("gc2155_q_exposure %d\n", *value);
err:
	return ret;
}
struct gc2155_control gc2155_controls[] = {
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
		.query = gc2155_q_exposure,
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
		.tweak = gc2155_v_flip,
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
		.tweak = gc2155_h_flip,
	},
	{
		.qc = {
			.id = V4L2_CID_FOCAL_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focal length",
			.minimum = GC2155_FOCAL_LENGTH_DEFAULT,
			.maximum = GC2155_FOCAL_LENGTH_DEFAULT,
			.step = 0x01,
			.default_value = GC2155_FOCAL_LENGTH_DEFAULT,
			.flags = 0,
		},
		.query = gc2155_g_focal,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number",
			.minimum = GC2155_F_NUMBER_DEFAULT,
			.maximum = GC2155_F_NUMBER_DEFAULT,
			.step = 0x01,
			.default_value = GC2155_F_NUMBER_DEFAULT,
			.flags = 0,
		},
		.query = gc2155_g_fnumber,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_RANGE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number range",
			.minimum = GC2155_F_NUMBER_RANGE,
			.maximum =  GC2155_F_NUMBER_RANGE,
			.step = 0x01,
			.default_value = GC2155_F_NUMBER_RANGE,
			.flags = 0,
		},
		.query = gc2155_g_fnumber_range,
	},
	{
		.qc = {
			.id = V4L2_CID_BIN_FACTOR_HORZ,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "horizontal binning factor",
			.minimum = 0,
			.maximum = GC2155_BIN_FACTOR_MAX,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.query = gc2155_g_bin_factor_x,
	},
	{
		.qc = {
			.id = V4L2_CID_BIN_FACTOR_VERT,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "vertical binning factor",
			.minimum = 0,
			.maximum = GC2155_BIN_FACTOR_MAX,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.query = gc2155_g_bin_factor_y,
	},
};
#define N_CONTROLS (ARRAY_SIZE(gc2155_controls))

static struct gc2155_control *gc2155_find_control(u32 id)
{
	int i;

	for (i = 0; i < N_CONTROLS; i++)
		if (gc2155_controls[i].qc.id == id)
			return &gc2155_controls[i];
	return NULL;
}

static int gc2155_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	struct gc2155_control *ctrl = gc2155_find_control(qc->id);
	struct gc2155_device *dev = to_gc2155_sensor(sd);

	if (ctrl == NULL)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	*qc = ctrl->qc;
	mutex_unlock(&dev->input_lock);

	return 0;
}

/* imx control set/get */
static int gc2155_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct gc2155_control *s_ctrl;
	struct gc2155_device *dev = to_gc2155_sensor(sd);
	int ret;

	if (!ctrl)
		return -EINVAL;

	s_ctrl = gc2155_find_control(ctrl->id);
	if ((s_ctrl == NULL) || (s_ctrl->query == NULL))
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	ret = s_ctrl->query(sd, &ctrl->value);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int gc2155_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct gc2155_control *octrl = gc2155_find_control(ctrl->id);
	struct gc2155_device *dev = to_gc2155_sensor(sd);
	int ret;

	if ((octrl == NULL) || (octrl->tweak == NULL))
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	ret = octrl->tweak(sd, ctrl->value);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int gc2155_init(struct v4l2_subdev *sd)
{
	int ret;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct gc2155_device *dev = to_gc2155_sensor(sd);

	mutex_lock(&dev->input_lock);

	/* set inital registers */
	ret  = gc2155_write_reg_array(client, gc2155_reset_register);

	/* restore settings */
	gc2155_res = gc2155_res_preview;
	N_RES = N_RES_PREVIEW;

	mutex_unlock(&dev->input_lock);

	return 0;
}

static int power_ctrl(struct v4l2_subdev *sd, bool flag)
{
       int ret = 0;
       struct gc2155_device *dev = to_gc2155_sensor(sd);
       if (!dev || !dev->platform_data)
               return -ENODEV;

       /* Non-gmin platforms use the legacy callback */
       if (dev->platform_data->power_ctrl)
               return dev->platform_data->power_ctrl(sd, flag);

       /* Timings and sequencing from original CTS gc2155 driver */
       if (flag) {
               ret |= dev->platform_data->v1p8_ctrl(sd, 0);
               ret |= dev->platform_data->v2p8_ctrl(sd, 0);
	       mdelay(50);

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
       int ret = 0;
       struct gc2155_device *dev = to_gc2155_sensor(sd);

       if (!dev || !dev->platform_data)
               return -ENODEV;

       /* Non-gmin platforms use the legacy callback */
       if (dev->platform_data->gpio_ctrl)
               return dev->platform_data->gpio_ctrl(sd, flag);

	/* GPIO0 == "reset" (active low), GPIO1 == "power down" */
	if (flag) {
		/* Per datasheet, PWRDWN comes before RST in both
		 * directions */
		ret |= dev->platform_data->gpio1_ctrl(sd, 0);
		usleep_range(10000, 15000);
		ret |= dev->platform_data->gpio0_ctrl(sd, 1);
		usleep_range(10000, 15000);
	} else {
		ret = dev->platform_data->gpio1_ctrl(sd, 1);
		usleep_range(10000, 15000);
		ret |= dev->platform_data->gpio0_ctrl(sd, 0);
		usleep_range(10000, 15000);
	}

	return ret;
}

static int power_down(struct v4l2_subdev *sd);

static int power_up(struct v4l2_subdev *sd)
{
	struct gc2155_device *dev = to_gc2155_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

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

	msleep(2);

	/* gpio ctrl */
	ret = gpio_ctrl(sd, 1);
	if (ret) {
		ret = gpio_ctrl(sd, 1);
		if (ret)
			goto fail_gpio;
	}

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
	struct gc2155_device *dev = to_gc2155_sensor(sd);
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

	/* flis clock control */
	ret = dev->platform_data->flisclk_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "flisclk failed\n");

	/* power control */
	ret = power_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "vprog failed.\n");

	return ret;
}

static int gc2155_s_power(struct v4l2_subdev *sd, int on)
{
	int ret;

	pr_info("%s: on %d\n", __func__, on);
	if (on == 0)
		return power_down(sd);
	else {
		ret = power_up(sd);
		if (!ret)
			return gc2155_init(sd);
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
static int distance(struct gc2155_resolution *res, u32 w, u32 h)
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
	struct gc2155_resolution *tmp_res = NULL;

	for (i = 0; i < N_RES; i++) {
		tmp_res = &gc2155_res[i];
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
		if (w != gc2155_res[i].width)
			continue;
		if (h != gc2155_res[i].height)
			continue;

		return i;
	}

	return -1;
}

static int gc2155_try_mbus_fmt(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt)
{
	int idx;

	if (!fmt)
		return -EINVAL;
	idx = nearest_resolution_index(fmt->width,
			fmt->height);
	if (idx == -1) {
		/* return the largest resolution */
		fmt->width = gc2155_res[N_RES - 1].width;
		fmt->height = gc2155_res[N_RES - 1].height;
	} else {
		fmt->width = gc2155_res[idx].width;
		fmt->height = gc2155_res[idx].height;
	}
	fmt->code = V4L2_MBUS_FMT_SGRBG10_1X10;

	return 0;
}

/* TODO: remove it. */
static int startup(struct v4l2_subdev *sd)
{
	struct gc2155_device *dev = to_gc2155_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	ret = gc2155_write_reg_array(client, gc2155_res[dev->fmt_idx].regs);
	if (ret) {
		dev_err(&client->dev, "gc2155 write register err.\n");
		return ret;
	}

	return ret;
}

static int gc2155_s_mbus_fmt(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt)
{
	struct gc2155_device *dev = to_gc2155_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camera_mipi_info *gc2155_info = NULL;
	int ret = 0;

	gc2155_info = v4l2_get_subdev_hostdata(sd);
	if (gc2155_info == NULL)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	ret = gc2155_try_mbus_fmt(sd, fmt);
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

	ret = startup(sd);
	if (ret) {
		dev_err(&client->dev, "gc2155 startup err\n");
		goto err;
	}

	ret = gc2155_get_intg_factor(client, gc2155_info,
			&gc2155_res[dev->fmt_idx]);
	if (ret) {
		dev_err(&client->dev, "failed to get integration_factor\n");
		goto err;
	}

err:
	mutex_unlock(&dev->input_lock);
	return ret;
}
static int gc2155_g_mbus_fmt(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt)
{
	struct gc2155_device *dev = to_gc2155_sensor(sd);

	if (!fmt)
		return -EINVAL;

	fmt->width = gc2155_res[dev->fmt_idx].width;
	fmt->height = gc2155_res[dev->fmt_idx].height;
	fmt->code = V4L2_MBUS_FMT_SGRBG10_1X10;

	return 0;
}

static int gc2155_detect(struct i2c_client *client)
{
	struct i2c_adapter *adapter = client->adapter;
	int ret;
	u8 chipid_H,chipid_L;
	u16 id;

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -ENODEV;

	ret = gc2155_read_reg(client, GC2155_8BIT, REG_CHIP_ID_H, &chipid_H);
	if (ret) {
		dev_err(&client->dev, "read sensor chipid_H failed\n");
		return -ENODEV;
	}

	ret = gc2155_read_reg(client, GC2155_8BIT, REG_CHIP_ID_L, &chipid_L);
	if (ret) {
		dev_err(&client->dev, "read sensor chipid_L failed\n");
		return -ENODEV;
	}

	id = ((chipid_H << 8) & 0xff00) | chipid_L;
	if (id != GC2155_ID) {
		dev_err(&client->dev, "sensor ID error, read id = 0x%x, target id = 0x%x\n", id, GC2155_ID);
		return -ENODEV;
	}

	/* The first time enters the _detect that front_camera_dev_info_kobj
	   is a NULL pointer. Then kobject_create_and_add will assign the buf address
	   to the front_camera_dev_info_kobj. So the second time enters the gc2155_detect
	   that front_camera_dev_info_kobj has buf address's value.
	 */
	front_camera_sensorid = id;
	if (front_camera_dev_info_kobj == NULL) {
		front_camera_dev_info_kobj = kobject_create_and_add("dev-info_front-camera", NULL);

		if (front_camera_dev_info_kobj == NULL) {
			dev_err(&client->dev, "%s: Create front_camera_dev_info_kobj failed\n", __func__);
		} else {
			ret = sysfs_create_group(front_camera_dev_info_kobj, &sensor_attr_group);
			if (ret)
				dev_err(&client->dev, "%s: Create camera_attr_group failed\n", __func__);
		}
        }

	dev_dbg(&client->dev, "detect gc2155 success\n");

	return 0;
}

static int gc2155_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct gc2155_device *dev = to_gc2155_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	mutex_lock(&dev->input_lock);

	if (enable) {
		pr_info("reset register.\n");
		/* enable per frame MIPI and sensor ctrl reset  */
		ret = gc2155_write_reg(client, GC2155_8BIT,  GC2155_REG_RST_AND_PG_SELECT, 0x30);
		if (ret) {
			dev_err(&client->dev, "reset register fail\n");
			mutex_unlock(&dev->input_lock);
			return ret;
		}
	}

	/*pege selsct*/
	ret = gc2155_write_reg(client, GC2155_8BIT,  GC2155_REG_RST_AND_PG_SELECT, 0x03);
	if (ret) {
		dev_err(&client->dev, "pege selsct fail\n");
		mutex_unlock(&dev->input_lock);
		return ret;
	}

	/*stream on/off*/
	ret = gc2155_write_reg(client, GC2155_8BIT, GC2155_SW_STREAM,
			enable ? GC2155_START_STREAMING :
			GC2155_STOP_STREAMING);
	if (ret) {
		dev_err(&client->dev, "streaming %d fail\n", enable);
		mutex_unlock(&dev->input_lock);
		return ret;
	}

	mutex_unlock(&dev->input_lock);
	return ret;
}

/* gc2155 enum frame size, frame intervals */
static int gc2155_enum_framesizes(struct v4l2_subdev *sd,
		struct v4l2_frmsizeenum *fsize)
{
	unsigned int index = fsize->index;

	if (index >= N_RES)
		return -EINVAL;

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = gc2155_res[index].width;
	fsize->discrete.height = gc2155_res[index].height;
	fsize->reserved[0] = gc2155_res[index].used;

	return 0;
}

static int gc2155_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *fival)
{
	unsigned int index = fival->index;

	if (index >= N_RES)
		return -EINVAL;

	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fival->width = gc2155_res[index].width;
	fival->height = gc2155_res[index].height;
	fival->discrete.numerator = 1;
	fival->discrete.denominator = gc2155_res[index].fps;

	return 0;
}

static int gc2155_enum_mbus_fmt(struct v4l2_subdev *sd,
		unsigned int index,
		enum v4l2_mbus_pixelcode *code)
{
	*code = V4L2_MBUS_FMT_SGRBG10_1X10;

	return 0;
}

static int gc2155_s_config(struct v4l2_subdev *sd,
		int irq, void *platform_data)
{
	struct gc2155_device *dev = to_gc2155_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	if (platform_data == NULL)
		return -ENODEV;

	dev->platform_data =
		(struct camera_sensor_platform_data *)platform_data;

	mutex_lock(&dev->input_lock);
	/* power off the module, then power on it in future
	 * as first power on by board may not fulfill the
	 * power on sequqence needed by the module
	 */

	if (dev->platform_data->platform_init) {
		ret = dev->platform_data->platform_init(client);
		if (ret) {
			dev_err(&client->dev, "platform init err\n");
			return ret;
		}
	}
	ret = power_down(sd);
	if (ret) {
		dev_err(&client->dev, "gc2155 power-off err.\n");
		goto fail_power_off;
	}
	msleep(20);

	ret = power_up(sd);
	if (ret) {
		dev_err(&client->dev, "gc2155 power-up err.\n");
		goto fail_power_on;
	}

	ret = dev->platform_data->csi_cfg(sd, 1);
	if (ret)
		goto fail_csi_cfg;

	/* config & detect sensor */
	ret = gc2155_detect(client);
	if (ret) {
		dev_err(&client->dev, "gc2155_detect err s_config.\n");
		goto fail_csi_cfg;
	}

	/* turn off sensor, after probed */
	ret = power_down(sd);
	if (ret) {
		dev_err(&client->dev, "gc2155 power-off err.\n");
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
	mutex_unlock(&dev->input_lock);
	return ret;
}

static int gc2155_g_parm(struct v4l2_subdev *sd,
		struct v4l2_streamparm *param)
{
	struct gc2155_device *dev = to_gc2155_sensor(sd);
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
			gc2155_res[dev->fmt_idx].fps;
	}
	return 0;
}

static int gc2155_s_parm(struct v4l2_subdev *sd,
		struct v4l2_streamparm *param)
{
	struct gc2155_device *dev = to_gc2155_sensor(sd);
	dev->run_mode = param->parm.capture.capturemode;

	mutex_lock(&dev->input_lock);
	switch (dev->run_mode) {
		case CI_MODE_VIDEO:
			gc2155_res = gc2155_res_video;
			N_RES = N_RES_VIDEO;
			break;
		case CI_MODE_STILL_CAPTURE:
			gc2155_res = gc2155_res_still;
			N_RES = N_RES_STILL;
			break;
		default:
			gc2155_res = gc2155_res_preview;
			N_RES = N_RES_PREVIEW;
	}
	mutex_unlock(&dev->input_lock);
	return 0;
}

static int gc2155_g_frame_interval(struct v4l2_subdev *sd,
		struct v4l2_subdev_frame_interval *interval)
{
	struct gc2155_device *dev = to_gc2155_sensor(sd);

	interval->interval.numerator = 1;
	interval->interval.denominator = gc2155_res[dev->fmt_idx].fps;

	return 0;
}

static int gc2155_enum_mbus_code(struct v4l2_subdev *sd,
		struct v4l2_subdev_fh *fh,
		struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index >= MAX_FMTS)
		return -EINVAL;

	code->code = V4L2_MBUS_FMT_SGRBG10_1X10;
	return 0;
}

static int gc2155_enum_frame_size(struct v4l2_subdev *sd,
		struct v4l2_subdev_fh *fh,
		struct v4l2_subdev_frame_size_enum *fse)
{
	int index = fse->index;

	if (index >= N_RES)
		return -EINVAL;

	fse->min_width = gc2155_res[index].width;
	fse->min_height = gc2155_res[index].height;
	fse->max_width = gc2155_res[index].width;
	fse->max_height = gc2155_res[index].height;

	return 0;

}

	static struct v4l2_mbus_framefmt *
__gc2155_get_pad_format(struct gc2155_device *sensor,
		struct v4l2_subdev_fh *fh, unsigned int pad,
		enum v4l2_subdev_format_whence which)
{
	struct i2c_client *client = v4l2_get_subdevdata(&sensor->sd);

	if (pad != 0) {
		dev_err(&client->dev,
				"__gc2155_get_pad_format err. pad %x\n", pad);
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

static int gc2155_get_pad_format(struct v4l2_subdev *sd,
		struct v4l2_subdev_fh *fh,
		struct v4l2_subdev_format *fmt)
{
	struct gc2155_device *snr = to_gc2155_sensor(sd);
	struct v4l2_mbus_framefmt *format =
		__gc2155_get_pad_format(snr, fh, fmt->pad, fmt->which);
	if (!format)
		return -EINVAL;

	fmt->format = *format;
	return 0;
}

static int gc2155_set_pad_format(struct v4l2_subdev *sd,
		struct v4l2_subdev_fh *fh,
		struct v4l2_subdev_format *fmt)
{
	struct gc2155_device *snr = to_gc2155_sensor(sd);

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		snr->format = fmt->format;

	return 0;
}

static int gc2155_g_skip_frames(struct v4l2_subdev *sd, u32 *frames)
{
	struct gc2155_device *dev = to_gc2155_sensor(sd);

	mutex_lock(&dev->input_lock);
	*frames = gc2155_res[dev->fmt_idx].skip_frames;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static const struct v4l2_subdev_sensor_ops gc2155_sensor_ops = {
	.g_skip_frames	= gc2155_g_skip_frames,
};

static const struct v4l2_subdev_video_ops gc2155_video_ops = {
	.s_stream = gc2155_s_stream,
	.g_parm = gc2155_g_parm,
	.s_parm = gc2155_s_parm,
	.enum_framesizes = gc2155_enum_framesizes,
	.enum_frameintervals = gc2155_enum_frameintervals,
	.enum_mbus_fmt = gc2155_enum_mbus_fmt,
	.try_mbus_fmt = gc2155_try_mbus_fmt,
	.g_mbus_fmt = gc2155_g_mbus_fmt,
	.s_mbus_fmt = gc2155_s_mbus_fmt,
	.g_frame_interval = gc2155_g_frame_interval,
};

static const struct v4l2_subdev_core_ops gc2155_core_ops = {
	.s_power = gc2155_s_power,
	.queryctrl = gc2155_queryctrl,
	.g_ctrl = gc2155_g_ctrl,
	.s_ctrl = gc2155_s_ctrl,
	.ioctl = gc2155_ioctl,
};

static const struct v4l2_subdev_pad_ops gc2155_pad_ops = {
	.enum_mbus_code = gc2155_enum_mbus_code,
	.enum_frame_size = gc2155_enum_frame_size,
	.get_fmt = gc2155_get_pad_format,
	.set_fmt = gc2155_set_pad_format,
};

static const struct v4l2_subdev_ops gc2155_ops = {
	.core = &gc2155_core_ops,
	.video = &gc2155_video_ops,
	.pad = &gc2155_pad_ops,
	.sensor = &gc2155_sensor_ops,
};

static int gc2155_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct gc2155_device *dev = to_gc2155_sensor(sd);
	dev_dbg(&client->dev, "gc2155_remove...\n");

	dev->platform_data->csi_cfg(sd, 0);

	v4l2_device_unregister_subdev(sd);
	media_entity_cleanup(&dev->sd.entity);
	kfree(dev);

	return 0;
}

static int gc2155_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct gc2155_device *dev;
	int ret;
	void *pdata;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&client->dev, "out of memory\n");
		return -ENOMEM;
	}

	mutex_init(&dev->input_lock);

	dev->fmt_idx = 0;
	v4l2_i2c_subdev_init(&(dev->sd), client, &gc2155_ops);

	if (ACPI_COMPANION(&client->dev))
		pdata = gmin_camera_platform_data(&dev->sd,
						  ATOMISP_INPUT_FORMAT_RAW_10,
						  atomisp_bayer_order_grbg);
	else
		pdata = client->dev.platform_data;

	if (!pdata) {
		ret = -ENODEV;
		goto out_free;
        }

	ret = gc2155_s_config(&dev->sd, client->irq, pdata);
	if (ret)
		goto out_free;

	ret = atomisp_register_i2c_module(&dev->sd, pdata, RAW_CAMERA);
	if (ret)
		goto out_free;

	dev->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->pad.flags = MEDIA_PAD_FL_SOURCE;
	dev->format.code = V4L2_MBUS_FMT_SGRBG10_1X10;
	dev->sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;

	ret = media_entity_init(&dev->sd.entity, 1, &dev->pad, 0);
	if (ret)
		gc2155_remove(client);

	return ret;
out_free:
	v4l2_device_unregister_subdev(&dev->sd);
	kfree(dev);
	return ret;
}

static struct acpi_device_id gc2155_acpi_match[] = {
       { "XXGC2155" },
       {},
};
MODULE_DEVICE_TABLE(acpi, gc2155_acpi_match);

MODULE_DEVICE_TABLE(i2c, gc2155_id);
static struct i2c_driver gc2155_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = GC2155_NAME,
		.acpi_match_table = ACPI_PTR(gc2155_acpi_match)
	},
	.probe = gc2155_probe,
	.remove = gc2155_remove,
	.id_table = gc2155_id,
};

static int init_gc2155(void)
{
	return i2c_add_driver(&gc2155_driver);
}

static void exit_gc2155(void)
{

	i2c_del_driver(&gc2155_driver);
}

module_init(init_gc2155);
module_exit(exit_gc2155);

MODULE_AUTHOR("Dean, Hsieh <dean.hsieh@intel.com>");
MODULE_DESCRIPTION("A low-level driver for GalaxyCore GC2155 sensors");
MODULE_LICENSE("GPL");
