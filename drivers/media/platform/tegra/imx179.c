/*
 * imx179.c - imx179 sensor driver
 *
 * Copyright (c) 2014, NVIDIA CORPORATION, All Rights Reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http:
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#include <linux/regmap.h>
#include <media/imx179.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/sysedp.h>

#include <linux/kernel.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#include "nvc_utilities.h"

struct imx179_reg {
	u16 addr;
	u8 val;
};

struct imx179_info {
	struct miscdevice		miscdev_info;
	int				mode;
	struct imx179_power_rail	power;
	struct imx179_otp               otp_data;
	struct imx179_sensordata	sensor_data;
	struct i2c_client		*i2c_client;
	struct imx179_platform_data	*pdata;
	struct clk			*mclk;
	struct regmap			*regmap;
	struct mutex			imx179_camera_lock;
	atomic_t			in_use;
	struct sysedp_consumer *sysedpc;
};

static const struct regmap_config sensor_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};

#define IMX179_TABLE_WAIT_MS 0
#define IMX179_TABLE_END 1
#define IMX179_MAX_RETRIES 3
#define IMX179_WAIT_MS 3

#define MAX_BUFFER_SIZE 32
#define IMX179_FRAME_LENGTH_ADDR_MSB 0x0340
#define IMX179_FRAME_LENGTH_ADDR_LSB 0x0341
#define IMX179_COARSE_TIME_ADDR_MSB 0x0202
#define IMX179_COARSE_TIME_ADDR_LSB 0x0203
#define IMX179_GAIN_ADDR 0x0205

static struct imx179_reg mode_3280x2460[] = {
	/* software reset */
	{0x0100, 0x00},
	{IMX179_TABLE_WAIT_MS, IMX179_WAIT_MS},

	/* global settings */
	{0x0101, 0x00},
	{0x0202, 0x09},
	{0x0203, 0xCA},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0305, 0x06},
	{0x0309, 0x05},
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0xA2},
	{0x0340, 0x09},
	{0x0341, 0xCE},
	{0x0342, 0x0D},
	{0x0343, 0x70},
	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x00},
	{0x0347, 0x02},
	{0x0348, 0x0C},
	{0x0349, 0xCF},
	{0x034A, 0x09},
	{0x034B, 0x9D},
	{0x034C, 0x0C},
	{0x034D, 0xD0},
	{0x034E, 0x09},
	{0x034F, 0x9C},
	{0x0383, 0x01},
	{0x0387, 0x01},
	{0x0390, 0x00},
	{0x0401, 0x00},
	{0x0405, 0x10},
	{0x3020, 0x10},
	{0x3041, 0x15},
	{0x3042, 0x87},
	{0x3089, 0x4F},
	{0x3302, 0x01},
	{0x3309, 0x9A},
	{0x3344, 0x57},
	{0x3345, 0x1F},
	{0x3362, 0x0A},
	{0x3363, 0x0A},
	{0x3364, 0x00},
	{0x3368, 0x18},
	{0x3369, 0x00},
	{0x3370, 0x77},
	{0x3371, 0x2F},
	{0x3372, 0x4F},
	{0x3373, 0x2F},
	{0x3374, 0x2F},
	{0x3375, 0x37},
	{0x3376, 0x9F},
	{0x3377, 0x37},
	{0x33C8, 0x00},
	{0x33D4, 0x0C},
	{0x33D5, 0xD0},
	{0x33D6, 0x09},
	{0x33D7, 0x9C},
	{0x4100, 0x0E},
	{0x4108, 0x01},
	{0x4109, 0x7C},

	{0x0100, 0x01},
	{IMX179_TABLE_WAIT_MS, IMX179_WAIT_MS},
	{IMX179_TABLE_END, 0x00}
};

static struct imx179_reg mode_1920x1080[] = {
	/* software reset */
	{0x0100, 0x00},
	{IMX179_TABLE_WAIT_MS, IMX179_WAIT_MS},

	/* global settings */
	{0x0101, 0x00},
	{0x0202, 0x09},
	{0x0203, 0xCA},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0305, 0x06},
	{0x0309, 0x05},
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0xA2},
	{0x0340, 0x09},
	{0x0341, 0xCE},
	{0x0342, 0x0D},
	{0x0343, 0x70},
	{0x0344, 0x00},
	{0x0345, 0x14},
	{0x0346, 0x01},
	{0x0347, 0x40},
	{0x0348, 0x0C},
	{0x0349, 0xBB},
	{0x034A, 0x08},
	{0x034B, 0x5F},
	{0x034C, 0x07},
	{0x034D, 0x80},
	{0x034E, 0x04},
	{0x034F, 0x38},
	{0x0383, 0x01},
	{0x0387, 0x01},
	{0x0390, 0x00},
	{0x0401, 0x02},
	{0x0405, 0x1B},
	{0x3020, 0x10},
	{0x3041, 0x15},
	{0x3042, 0x87},
	{0x3089, 0x4F},
	{0x3302, 0x01},
	{0x3309, 0x9A},
	{0x3344, 0x57},
	{0x3345, 0x1F},
	{0x3362, 0x0A},
	{0x3363, 0x0A},
	{0x3364, 0x00},
	{0x3368, 0x18},
	{0x3369, 0x00},
	{0x3370, 0x77},
	{0x3371, 0x2F},
	{0x3372, 0x4F},
	{0x3373, 0x2F},
	{0x3374, 0x2F},
	{0x3375, 0x37},
	{0x3376, 0x9F},
	{0x3377, 0x37},
	{0x33C8, 0x00},
	{0x33D4, 0x0C},
	{0x33D5, 0xA8},
	{0x33D6, 0x07},
	{0x33D7, 0x20},
	{0x4100, 0x0E},
	{0x4108, 0x01},
	{0x4109, 0x7C},

	{0x0100, 0x01},
	{IMX179_TABLE_WAIT_MS, IMX179_WAIT_MS},
	{IMX179_TABLE_END, 0x00}
};
static struct imx179_reg mode_1280x720_90fps[] = {
	/* software reset */
	{0x0100, 0x00},
	{IMX179_TABLE_WAIT_MS, IMX179_WAIT_MS},

	/* global settings */
	{0x0101, 0x00},
	{0x0202, 0x03},
	{0x0203, 0x41},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0305, 0x06},
	{0x0309, 0x05},
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0xA2},
	{0x0340, 0x03},
	{0x0341, 0x45},
	{0x0342, 0x0D},
	{0x0343, 0x70},
	{0x0344, 0x01},
	{0x0345, 0x68},
	{0x0346, 0x02},
	{0x0347, 0x00},
	{0x0348, 0x0B},
	{0x0349, 0x67},
	{0x034A, 0x07},
	{0x034B, 0x9F},
	{0x034C, 0x05},
	{0x034D, 0x00},
	{0x034E, 0x02},
	{0x034F, 0xD0},
	{0x0383, 0x01},
	{0x0387, 0x01},
	{0x0390, 0x01},
	{0x0401, 0x00},
	{0x0405, 0x10},
	{0x3020, 0x10},
	{0x3041, 0x15},
	{0x3042, 0x87},
	{0x3089, 0x4F},
	{0x3302, 0x01},
	{0x3309, 0x9A},
	{0x3344, 0x57},
	{0x3345, 0x1F},
	{0x3362, 0x0A},
	{0x3363, 0x0A},
	{0x3364, 0x00},
	{0x3368, 0x18},
	{0x3369, 0x00},
	{0x3370, 0x77},
	{0x3371, 0x2F},
	{0x3372, 0x4F},
	{0x3373, 0x2F},
	{0x3374, 0x2F},
	{0x3375, 0x37},
	{0x3376, 0x9F},
	{0x3377, 0x37},
	{0x33C8, 0x00},
	{0x33D4, 0x05},
	{0x33D5, 0x00},
	{0x33D6, 0x02},
	{0x33D7, 0xD0},
	{0x4100, 0x0E},
	{0x4108, 0x01},
	{0x4109, 0x7C},

	{0x0100, 0x01},
	{IMX179_TABLE_WAIT_MS, IMX179_WAIT_MS},
	{IMX179_TABLE_END, 0x00}
};

enum {
	IMX179_MODE_3280X2460,
	IMX179_MODE_1920X1080,
	IMX179_MODE_1280X720_90FPS,
};

static struct imx179_reg *mode_table[] = {
	[IMX179_MODE_3280X2460] = mode_3280x2460,
	[IMX179_MODE_1920X1080] = mode_1920x1080,
	[IMX179_MODE_1280X720_90FPS] = mode_1280x720_90fps,
};

static inline void
msleep_range(unsigned int delay_base)
{
	usleep_range(delay_base*1000, delay_base*1000+500);
}

static inline void
imx179_get_frame_length_regs(struct imx179_reg *regs, u32 frame_length)
{
	regs->addr = IMX179_FRAME_LENGTH_ADDR_MSB;
	regs->val = (frame_length >> 8) & 0xff;
	(regs + 1)->addr = IMX179_FRAME_LENGTH_ADDR_LSB;
	(regs + 1)->val = (frame_length) & 0xff;
}

static inline void
imx179_get_coarse_time_regs(struct imx179_reg *regs, u32 coarse_time)
{
	regs->addr = IMX179_COARSE_TIME_ADDR_MSB;
	regs->val = (coarse_time >> 8) & 0xff;
	(regs + 1)->addr = IMX179_COARSE_TIME_ADDR_LSB;
	(regs + 1)->val = (coarse_time) & 0xff;
}

static inline void
imx179_get_gain_reg(struct imx179_reg *regs, u16 gain)
{
	regs->addr = IMX179_GAIN_ADDR;
	regs->val = gain;
}

static inline int
imx179_read_reg(struct imx179_info *info, u16 addr, u8 *val)
{
	return regmap_read(info->regmap, addr, (unsigned int *) val);
}

static int
imx179_write_reg(struct imx179_info *info, u16 addr, u8 val)
{
	int err;

	err = regmap_write(info->regmap, addr, val);

	if (err)
		pr_err("%s:i2c write failed, %x = %x\n",
			__func__, addr, val);

	return err;
}

static int
imx179_write_table(struct imx179_info *info,
				 const struct imx179_reg table[],
				 const struct imx179_reg override_list[],
				 int num_override_regs)
{
	int err;
	const struct imx179_reg *next;
	int i;
	u16 val;

	for (next = table; next->addr != IMX179_TABLE_END; next++) {
		if (next->addr == IMX179_TABLE_WAIT_MS) {
			msleep_range(next->val);
			continue;
		}

		val = next->val;

		/* When an override list is passed in, replace the reg */
		/* value to write if the reg is in the list            */
		if (override_list) {
			for (i = 0; i < num_override_regs; i++) {
				if (next->addr == override_list[i].addr) {
					val = override_list[i].val;
					break;
				}
			}
		}

		err = imx179_write_reg(info, next->addr, val);
		if (err) {
			pr_err("%s:imx179_write_table:%d", __func__, err);
			return err;
		}
	}
	return 0;
}

static int imx179_get_flash_cap(struct imx179_info *info)
{
	struct imx179_flash_control *fctl;

	dev_dbg(&info->i2c_client->dev, "%s: %p\n", __func__, info->pdata);
	if (info->pdata) {
		fctl = &info->pdata->flash_cap;
		dev_dbg(&info->i2c_client->dev,
			"edg: %x, st: %x, rpt: %x, dl: %x\n",
			fctl->edge_trig_en,
			fctl->start_edge,
			fctl->repeat,
			fctl->delay_frm);

		if (fctl->enable)
			return 0;
	}
	return -ENODEV;
}

static inline int imx179_set_flash_control(
	struct imx179_info *info, struct imx179_flash_control *fc)
{
	dev_dbg(&info->i2c_client->dev, "%s\n", __func__);
	return imx179_write_reg(info, 0x0802, 0x01);
}

static int
imx179_set_mode(struct imx179_info *info, struct imx179_mode *mode)
{
	int sensor_mode;
	int err;
	struct imx179_reg reg_list[8];

	pr_info("%s: xres %u yres %u framelength %u coarsetime %u gain %u\n",
			 __func__, mode->xres, mode->yres, mode->frame_length,
			 mode->coarse_time, mode->gain);

	if (mode->xres == 3280 && mode->yres == 2460) {
		sensor_mode = IMX179_MODE_3280X2460;
	} else if (mode->xres == 1920 && mode->yres == 1080) {
		sensor_mode = IMX179_MODE_1920X1080;
	} else if (mode->xres == 1280 && mode->yres == 720) {
		sensor_mode = IMX179_MODE_1280X720_90FPS;
	} else {
		pr_err("%s: invalid resolution supplied to set mode %d %d\n",
			 __func__, mode->xres, mode->yres);
		return -EINVAL;
	}

	sysedp_set_state(info->sysedpc, 1);

	/* get a list of override regs for the asking frame length, */
	/* coarse integration time, and gain.                       */
	imx179_get_frame_length_regs(reg_list, mode->frame_length);
	imx179_get_coarse_time_regs(reg_list + 2, mode->coarse_time);
	imx179_get_gain_reg(reg_list + 4, mode->gain);

	err = imx179_write_table(info,
				mode_table[sensor_mode],
				reg_list, 5);
	if (err)
		return err;
	if (err)
		return err;

	info->mode = sensor_mode;
	pr_info("[IMX179]: stream on.\n");
	return 0;
}

static int
imx179_get_status(struct imx179_info *info, u8 *dev_status)
{
	*dev_status = 0;
	return 0;
}

static int
imx179_set_frame_length(struct imx179_info *info, u32 frame_length,
						 bool group_hold)
{
	struct imx179_reg reg_list[2];
	int i = 0;
	int ret;

	imx179_get_frame_length_regs(reg_list, frame_length);
	if (group_hold) {
		ret = imx179_write_reg(info, 0x0104, 0x01);
		if (ret)
			return ret;
	}

	for (i = 0; i < 2; i++) {
		ret = imx179_write_reg(info, reg_list[i].addr,
			 reg_list[i].val);
		if (ret)
			return ret;
	}

	if (group_hold) {
		ret = imx179_write_reg(info, 0x0104, 0x0);
		if (ret)
			return ret;
	}

	return 0;
}

static int
imx179_set_coarse_time(struct imx179_info *info, u32 coarse_time,
						 bool group_hold)
{
	int ret;

	struct imx179_reg reg_list[2];
	int i = 0;

	imx179_get_coarse_time_regs(reg_list, coarse_time);
	if (group_hold) {
		ret = imx179_write_reg(info, 0x104, 0x01);
		if (ret)
			return ret;
	}

	for (i = 0; i < 2; i++) {
		ret = imx179_write_reg(info, reg_list[i].addr,
			 reg_list[i].val);
		if (ret)
			return ret;
	}

	if (group_hold) {
		ret = imx179_write_reg(info, 0x104, 0x0);
		if (ret)
			return ret;
	}
	return 0;
}

static int
imx179_set_gain(struct imx179_info *info, u16 gain, bool group_hold)
{
	int ret;
	struct imx179_reg reg_list;

	imx179_get_gain_reg(&reg_list, gain);

	if (group_hold) {
		ret = imx179_write_reg(info, 0x104, 0x1);
		if (ret)
			return ret;
	}

	ret = imx179_write_reg(info, reg_list.addr, reg_list.val);
	if (ret)
		return ret;

	if (group_hold) {
		ret = imx179_write_reg(info, 0x104, 0x0);
		if (ret)
			return ret;
	}
	return 0;
}

static int
imx179_set_group_hold(struct imx179_info *info, struct imx179_ae *ae)
{
	int ret;
	int count = 0;
	bool group_hold_enabled = false;


	if (ae->gain_enable)
		count++;
	if (ae->coarse_time_enable)
		count++;
	if (ae->frame_length_enable)
		count++;
	if (count >= 2)
		group_hold_enabled = true;

	if (group_hold_enabled) {
		ret = imx179_write_reg(info, 0x104, 0x1);
		if (ret)
			return ret;
	}

	if (ae->gain_enable)
		imx179_set_gain(info, ae->gain, false);
	if (ae->coarse_time_enable)
		imx179_set_coarse_time(info, ae->coarse_time, true);
	if (ae->frame_length_enable)
		imx179_set_frame_length(info, ae->frame_length, false);

	if (group_hold_enabled) {
		ret = imx179_write_reg(info, 0x104, 0x0);
		if (ret)
			return ret;
	}

	return 0;
}

static int imx179_get_sensor_id(struct imx179_info *info)
{
	int ret = 0;
	int i;
	u8 bak = 0;

	pr_info("%s\n", __func__);
	if (info->sensor_data.fuse_id_size)
		return 0;

	return ret;
}

static int imx179_get_otp_vendor(struct imx179_info *info)
{
	int i;
	u8 bak = 0;
	u8 baj = 0;
	u8 *otp = info->otp_data.otp_data;
	info->otp_data.otp_size = 0;
	otp[0] = 0;

	imx179_write_reg(info, 0x0100, 0x00);
	imx179_write_reg(info, 0x3382, 0x05);
	imx179_write_reg(info, 0x3383, 0xa0);
	imx179_write_reg(info, 0x3368, 0x24);
	imx179_write_reg(info, 0x3369, 0x00);


	imx179_write_reg(info, 0x3380, 0x08);
	imx179_write_reg(info, 0x3400, 0x01);
	imx179_write_reg(info, 0x3402, 0x00);
	udelay(10);
	for (i = 0x3411; i >= 0x340B; i -= 0x06) {
		imx179_read_reg(info, i, &bak);
		imx179_read_reg(info, i+1, &baj);
		if (bak != 0)
			break;
	}
	pr_info("imx179 module vendor ID: 0x%02x %02x\n", bak, baj);
	if ((bak == 0x08) && (baj == 0x06)) {
		otp[0] = 0;
	} else if ((bak == 0x18) && (baj == 0x06)) {
		otp[0] = 1;
	} else if ((bak == 0x88) && (baj == 0x88)) {
		otp[0] = 2;
	}
	info->otp_data.otp_size = 1;
	return 0;
}

static int imx179_get_otp_data(struct imx179_info *info)
{
	int ret = 0;
	int i, j;
	u8 bak = 0;
	u8 baj = 0;
	u8 *otp = info->otp_data.otp_data;
	info->otp_data.otp_size = 0;

	imx179_write_reg(info, 0x0100, 0x00);
	imx179_write_reg(info, 0x3382, 0x05);
	imx179_write_reg(info, 0x3383, 0xa0);
	imx179_write_reg(info, 0x3368, 0x24);
	imx179_write_reg(info, 0x3369, 0x00);


	imx179_write_reg(info, 0x3380, 0x08);
	imx179_write_reg(info, 0x3400, 0x01);
	imx179_write_reg(info, 0x3402, 0x12);
	udelay(10);
	for (i = 0x3443; i >= 0x3442; i--) {
		imx179_read_reg(info, i, &bak);
		if ((bak == 0x11) || (bak == 0xEE))
			break;
	}
	pr_info("imx179 otp write result: 0x%02x", bak);

	if (bak == 0xEE)
		return ret;


	imx179_write_reg(info, 0x3380, 0x08);
	imx179_write_reg(info, 0x3400, 0x01);
	imx179_write_reg(info, 0x3402, 0);
	udelay(10);
	for (i = 0; i < 45 ; i++) {
		ret |= imx179_read_reg(info, 0x3417 + i, otp);
		otp++;
	}


	for (j = 1; j <= 11; j++) {
		imx179_write_reg(info, 0x3380, 0x08);
		imx179_write_reg(info, 0x3400, 0x01);
		imx179_write_reg(info, 0x3402, j);
		udelay(10);
		for (i = 0; i < 64 ; i++) {
			ret |= imx179_read_reg(info, 0x3404 + i, otp);
			otp++;
		}
	}

	imx179_write_reg(info, 0x3380, 0x08);
	imx179_write_reg(info, 0x3400, 0x01);
	imx179_write_reg(info, 0x3402, 12);
	udelay(10);
	for (i = 0; i < 54 ; i++) {
		ret |= imx179_read_reg(info, 0x3404 + i, otp);
		otp++;
	}
	pr_info("%s imx179 module otp data size:%d", __func__, otp - info->otp_data.otp_data);
	info->otp_data.otp_size = 803;

	return ret;
}

static void imx179_mclk_disable(struct imx179_info *info)
{
	dev_dbg(&info->i2c_client->dev, "%s: disable MCLK\n", __func__);
	clk_disable_unprepare(info->mclk);
}

static int imx179_mclk_enable(struct imx179_info *info)
{
	int err;
	unsigned long mclk_init_rate = 24000000;

	dev_dbg(&info->i2c_client->dev, "%s: enable MCLK with %lu Hz\n",
		__func__, mclk_init_rate);

	err = clk_set_rate(info->mclk, mclk_init_rate);
	if (!err)
		err = clk_prepare_enable(info->mclk);
	return err;
}

static long
imx179_ioctl(struct file *file,
			 unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct imx179_info *info = file->private_data;

	switch (cmd) {
	case IMX179_IOCTL_SET_POWER:
		if (!info->pdata)
			break;
		if (arg && info->pdata->power_on) {
			err = imx179_mclk_enable(info);
			if (!err)
				err = info->pdata->power_on(&info->power);
			if (err < 0)
				imx179_mclk_disable(info);
		}
		if (!arg && info->pdata->power_off) {
			info->pdata->power_off(&info->power);
			imx179_mclk_disable(info);
			sysedp_set_state(info->sysedpc, 0);
		}
		break;
	case IMX179_IOCTL_SET_MODE:
	{
		struct imx179_mode mode;
		if (copy_from_user(&mode, (const void __user *)arg,
			sizeof(struct imx179_mode))) {
			pr_err("%s:Failed to get mode from user.\n", __func__);
			return -EFAULT;
		}
		return imx179_set_mode(info, &mode);
	}
	case IMX179_IOCTL_SET_FRAME_LENGTH:
		return imx179_set_frame_length(info, (u32)arg, true);
	case IMX179_IOCTL_SET_COARSE_TIME:
		return imx179_set_coarse_time(info, (u32)arg, true);
	case IMX179_IOCTL_SET_GAIN:
		return imx179_set_gain(info, (u16)arg, true);
	case IMX179_IOCTL_GET_STATUS:
	{
		u8 status;

		err = imx179_get_status(info, &status);
		if (err)
			return err;
		if (copy_to_user((void __user *)arg, &status, 1)) {
			pr_err("%s:Failed to copy status to user\n", __func__);
			return -EFAULT;
		}
		return 0;
	}
	case IMX179_IOCTL_GET_SENSORDATA:
	{
		err = imx179_get_sensor_id(info);

		if (err) {
			pr_err("%s:Failed to get fuse id info.\n", __func__);
			return err;
		}
		if (copy_to_user((void __user *)arg, &info->sensor_data,
				sizeof(struct imx179_sensordata))) {
			pr_info("%s:Failed to copy fuse id to user space\n",
				__func__);
			return -EFAULT;
		}
		return 0;
	}
	case IMX179_IOCTL_GET_OTPDATA:
	{
		err = imx179_get_otp_data(info);

		if (err) {
			pr_err("%s:Failed to get otp data.\n", __func__);
			return err;
		}
		if (copy_to_user((void __user *)arg, &info->otp_data,
				sizeof(struct imx179_otp))) {
			pr_info("%s:Failed to copy otp data to user space\n",
				__func__);
			return -EFAULT;
		}
		return 0;
	}
	case IMX179_IOCTL_GET_OTPVEND:
	{
		err = imx179_get_otp_vendor(info);
		if (copy_to_user((void __user *)arg, &info->otp_data,
				sizeof(struct imx179_otp))) {
			pr_info("%s:Failed to copy otp data to user space\n",
				__func__);
			return -EFAULT;
		}
		return 0;
	}
	case IMX179_IOCTL_SET_GROUP_HOLD:
	{
		struct imx179_ae ae;
		if (copy_from_user(&ae, (const void __user *)arg,
			sizeof(struct imx179_ae))) {
			pr_info("%s:fail group hold\n", __func__);
			return -EFAULT;
		}
		return imx179_set_group_hold(info, &ae);
	}
	case IMX179_IOCTL_SET_FLASH_MODE:
	{
		struct imx179_flash_control values;

		dev_dbg(&info->i2c_client->dev,
			"IMX179_IOCTL_SET_FLASH_MODE\n");
		if (copy_from_user(&values,
			(const void __user *)arg,
			sizeof(struct imx179_flash_control))) {
			err = -EFAULT;
			break;
		}
		err = imx179_set_flash_control(info, &values);
		break;
	}
	case IMX179_IOCTL_GET_FLASH_CAP:
		err = imx179_get_flash_cap(info);
		break;
	default:
		pr_err("%s:unknown cmd.\n", __func__);
		err = -EINVAL;
	}

	return err;
}

static int imx179_get_extra_regulators(struct imx179_power_rail *pw)
{
	if (!pw->ext_reg1) {
		pw->ext_reg1 = regulator_get(NULL, "imx179_reg1");
		if (WARN_ON(IS_ERR(pw->ext_reg1))) {
			pr_err("%s: can't get regulator imx179_reg1: %ld\n",
				__func__, PTR_ERR(pw->ext_reg1));
			pw->ext_reg1 = NULL;
			return -ENODEV;
		}
	}

	if (!pw->ext_reg2) {
		pw->ext_reg2 = regulator_get(NULL, "imx179_reg2");
		if (WARN_ON(IS_ERR(pw->ext_reg2))) {
			pr_err("%s: can't get regulator imx179_reg2: %ld\n",
				__func__, PTR_ERR(pw->ext_reg2));
			pw->ext_reg2 = NULL;
			return -ENODEV;
		}
	}

	return 0;
}

static int imx179_power_on(struct imx179_power_rail *pw)
{
	int err;
	struct imx179_info *info = container_of(pw, struct imx179_info, power);

	if (unlikely(WARN_ON(!pw || !pw->iovdd || !pw->avdd)))
		return -EFAULT;

	if (info->pdata->ext_reg) {
		if (imx179_get_extra_regulators(pw))
			goto imx179_poweron_fail;

		err = regulator_enable(pw->ext_reg1);
		if (unlikely(err))
			goto imx179_ext_reg1_fail;

		err = regulator_enable(pw->ext_reg2);
		if (unlikely(err))
			goto imx179_ext_reg2_fail;

	}

	gpio_set_value(info->pdata->reset_gpio, 0);
	gpio_set_value(info->pdata->af_gpio, 1);
	gpio_set_value(info->pdata->cam1_gpio, 0);
	usleep_range(10, 20);

	err = regulator_enable(pw->avdd);
	if (err)
		goto imx179_avdd_fail;

	err = regulator_enable(pw->iovdd);
	if (err)
		goto imx179_iovdd_fail;

	usleep_range(1, 2);
	gpio_set_value(info->pdata->reset_gpio, 1);
	gpio_set_value(info->pdata->cam1_gpio, 1);

	usleep_range(300, 310);

	return 1;


imx179_iovdd_fail:
	regulator_disable(pw->avdd);

imx179_avdd_fail:
	if (pw->ext_reg2)
		regulator_disable(pw->ext_reg2);

imx179_ext_reg2_fail:
	if (pw->ext_reg1)
		regulator_disable(pw->ext_reg1);
	gpio_set_value(info->pdata->af_gpio, 0);

imx179_ext_reg1_fail:
imx179_poweron_fail:
	pr_err("%s failed.\n", __func__);
	return -ENODEV;
}

static int imx179_power_off(struct imx179_power_rail *pw)
{
	struct imx179_info *info = container_of(pw, struct imx179_info, power);

	if (unlikely(WARN_ON(!pw || !pw->iovdd || !pw->avdd)))
		return -EFAULT;

	usleep_range(1, 2);
	gpio_set_value(info->pdata->cam1_gpio, 0);
	usleep_range(1, 2);

	regulator_disable(pw->iovdd);
	regulator_disable(pw->avdd);

	if (info->pdata->ext_reg) {
		regulator_disable(pw->ext_reg1);
		regulator_disable(pw->ext_reg2);
	}
	sysedp_set_state(info->sysedpc, 0);

	return 0;
}

static int
imx179_open(struct inode *inode, struct file *file)
{
	struct miscdevice	*miscdev = file->private_data;
	struct imx179_info *info;

	info = container_of(miscdev, struct imx179_info, miscdev_info);
	/* check if the device is in use */
	if (atomic_xchg(&info->in_use, 1)) {
		pr_info("%s:BUSY!\n", __func__);
		return -EBUSY;
	}

	file->private_data = info;

	return 0;
}

static int
imx179_release(struct inode *inode, struct file *file)
{
	struct imx179_info *info = file->private_data;

	file->private_data = NULL;

	/* warn if device is already released */
	WARN_ON(!atomic_xchg(&info->in_use, 0));
	return 0;
}

static int imx179_power_put(struct imx179_power_rail *pw)
{
	if (unlikely(!pw))
		return -EFAULT;

	if (likely(pw->avdd))
		regulator_put(pw->avdd);

	if (likely(pw->iovdd))
		regulator_put(pw->iovdd);

	if (likely(pw->dvdd))
		regulator_put(pw->dvdd);

	if (likely(pw->ext_reg1))
		regulator_put(pw->ext_reg1);

	if (likely(pw->ext_reg2))
		regulator_put(pw->ext_reg2);

	pw->avdd = NULL;
	pw->iovdd = NULL;
	pw->dvdd = NULL;
	pw->ext_reg1 = NULL;
	pw->ext_reg2 = NULL;

	return 0;
}

static int imx179_regulator_get(struct imx179_info *info,
	struct regulator **vreg, char vreg_name[])
{
	struct regulator *reg = NULL;
	int err = 0;

	reg = regulator_get(&info->i2c_client->dev, vreg_name);
	if (unlikely(IS_ERR(reg))) {
		dev_err(&info->i2c_client->dev, "%s %s ERR: %d\n",
			__func__, vreg_name, (int)reg);
		err = PTR_ERR(reg);
		reg = NULL;
	} else
		dev_dbg(&info->i2c_client->dev, "%s: %s\n",
			__func__, vreg_name);

	*vreg = reg;
	return err;
}

static int imx179_power_get(struct imx179_info *info)
{
	struct imx179_power_rail *pw = &info->power;
	int err = 0;

	err |= imx179_regulator_get(info, &pw->avdd, "vana"); /* ananlog 2.7v */
	err |= imx179_regulator_get(info, &pw->dvdd, "vdig"); /* digital 1.2v */
	err |= imx179_regulator_get(info, &pw->iovdd, "vif"); /* IO 1.8v */

	return err;
}

static const struct file_operations imx179_fileops = {
	.owner = THIS_MODULE,
	.open = imx179_open,
	.unlocked_ioctl = imx179_ioctl,
	.release = imx179_release,
};

static struct miscdevice imx179_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "imx179",
	.fops = &imx179_fileops,
};

static struct of_device_id imx179_of_match[] = {
	{ .compatible = "nvidia,imx179", },
	{ },
};

MODULE_DEVICE_TABLE(of, imx179_of_match);

static struct imx179_platform_data *imx179_parse_dt(struct i2c_client *client)
{
	struct device_node *np = client->dev.of_node;
	struct imx179_platform_data *board_info_pdata;
	const struct of_device_id *match;

	match = of_match_device(imx179_of_match, &client->dev);
	if (!match) {
		dev_err(&client->dev, "Failed to find matching dt id\n");
		return NULL;
	}

	board_info_pdata = devm_kzalloc(&client->dev, sizeof(*board_info_pdata),
			GFP_KERNEL);
	if (!board_info_pdata) {
		dev_err(&client->dev, "Failed to allocate pdata\n");
		return NULL;
	}

	board_info_pdata->cam1_gpio = of_get_named_gpio(np, "cam1-gpios", 0);
	board_info_pdata->reset_gpio = of_get_named_gpio(np, "reset-gpios", 0);
	board_info_pdata->af_gpio = of_get_named_gpio(np, "af-gpios", 0);

	board_info_pdata->ext_reg = of_property_read_bool(np, "nvidia,ext_reg");

	board_info_pdata->power_on = imx179_power_on;
	board_info_pdata->power_off = imx179_power_off;

	return board_info_pdata;
}

static int
imx179_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct imx179_info *info;
	int err;
	const char *mclk_name;

	pr_err("[IMX179]: probing sensor.\n");

	info = devm_kzalloc(&client->dev,
			sizeof(struct imx179_info), GFP_KERNEL);
	if (!info) {
		pr_err("%s:Unable to allocate memory!\n", __func__);
		return -ENOMEM;
	}

	info->regmap = devm_regmap_init_i2c(client, &sensor_regmap_config);
	if (IS_ERR(info->regmap)) {
		dev_err(&client->dev,
			"regmap init failed: %ld\n", PTR_ERR(info->regmap));
		return -ENODEV;
	}

	if (client->dev.of_node)
		info->pdata = imx179_parse_dt(client);
	else
		info->pdata = client->dev.platform_data;

	if (!info->pdata) {
		pr_err("[IMX179]:%s:Unable to get platform data\n", __func__);
		return -EFAULT;
	}

	info->i2c_client = client;
	atomic_set(&info->in_use, 0);
	info->mode = -1;

	mclk_name = info->pdata->mclk_name ?
		    info->pdata->mclk_name : "default_mclk";
	info->mclk = devm_clk_get(&client->dev, mclk_name);
	if (IS_ERR(info->mclk)) {
		dev_err(&client->dev, "%s: unable to get clock %s\n",
			__func__, mclk_name);
		return PTR_ERR(info->mclk);
	}

	imx179_power_get(info);

	memcpy(&info->miscdev_info,
		&imx179_device,
		sizeof(struct miscdevice));

	err = misc_register(&info->miscdev_info);
	if (err) {
		pr_err("%s:Unable to register misc device!\n", __func__);
		goto imx179_probe_fail;
	}

	i2c_set_clientdata(client, info);

	info->sysedpc = sysedp_create_consumer("imx179", "imx179");

	mutex_init(&info->imx179_camera_lock);

	pr_err("[IMX179]: end of probing sensor.\n");
	return 0;

imx179_probe_fail:
	imx179_power_put(&info->power);

	return err;
}

static int
imx179_remove(struct i2c_client *client)
{
	struct imx179_info *info;
	info = i2c_get_clientdata(client);
	sysedp_free_consumer(info->sysedpc);
	misc_deregister(&imx179_device);
	mutex_destroy(&info->imx179_camera_lock);

	imx179_power_put(&info->power);

	return 0;
}

static const struct i2c_device_id imx179_id[] = {
	{ "imx179", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, imx179_id);

static struct i2c_driver imx179_i2c_driver = {
	.driver = {
		.name = "imx179",
		.owner = THIS_MODULE,
	},
	.probe = imx179_probe,
	.remove = imx179_remove,
	.id_table = imx179_id,
};

static int __init imx179_init(void)
{
	pr_info("[IMX179] sensor driver loading\n");
	return i2c_add_driver(&imx179_i2c_driver);
}

static void __exit imx179_exit(void)
{
	i2c_del_driver(&imx179_i2c_driver);
}

module_init(imx179_init);
module_exit(imx179_exit);
