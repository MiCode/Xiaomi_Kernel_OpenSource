/*
 * ov9726.c - ov9726 sensor driver
 *
 * Copyright (c) 2011-2014, NVIDIA Corporation. All Rights Reserved.
 *
 * Contributors:
 *	  Charlie Huang <chahuang@nvidia.com>
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>

#include <media/ov9726.h>
#include <media/nvc.h>

#define OV9726_FUSE_ID_SIZE	5

struct ov9726_power_rail {
	struct regulator *sen_1v8_reg;
	struct regulator *sen_2v8_reg;
};

struct ov9726_devinfo {
	struct miscdevice		miscdev_info;
	struct i2c_client		*i2c_client;
	struct ov9726_platform_data	*pdata;
	struct clk			*mclk;
	struct ov9726_power_rail	power_rail;
	atomic_t			in_use;
	__u32				mode;
	struct ov9726_reg		grphold_temp[10];
	struct nvc_fuseid		fuse_id;
};

static struct ov9726_reg mode_1280x720[] = {
	/*
	(1) clock setting
	clock formula:	(Ref_clk / pre_pll_clk_div) * pll_multiplier /
			vt_sys_clk_div / vt_pix_clk_div / divmip
	input clk at        24Mhz
	pre_pll_clk_div     0305[3:0] => 4
	pll_multiplier      0307[6:0] => 100
	vt_sys_clk_div      0303[3:0] => 1
	vt_pix_clk_div      0301[3:0] => 10
	divmip              3010[2:0] => 1

	Overall timing:
	line length:  1664 (reg 0x342/0x343)
	frame length: 840  (reg 0x340/0x341)
	coarse integration time: 835 lines (reg 0x202/0x203) => change to 836

	visible pixels: (0,40) - (1280, 720+40) with size 1280x720
	Output pixels (1280x720)

	Frame rate if MCLK=24MHz:
	24Mhz/4 *100/1/10/1 = 60 Mhz
	60Mhz/1664/840 = 42.9 fps
	*/

	{0x0103, 0x01},

	{OV9726_TABLE_WAIT_MS, 10},

	{0x3026, 0x00},
	{0x3027, 0x00},
	{0x3705, 0x45},
	{0x3603, 0xaa},
	{0x3632, 0x2f},
	{0x3620, 0x66},
	{0x3621, 0xc0},
	{0x0202, 0x03},
	{0x0203, 0x13},
	{0x3833, 0x04},
	{0x3835, 0x02},
	{0x4702, 0x04},
	{0x4704, 0x00},
	{0x4706, 0x08},
	{0x5052, 0x01},
	{0x3819, 0x6c},
	{0x3817, 0x94},
	{0x404e, 0x7e},
	{0x3601, 0x40},
	{0x3610, 0xa0},
	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x00},
	{0x0347, 0x28},

	{0x034c, 0x05},
	{0x034d, 0x00},
	{0x034e, 0x02},
	{0x034f, 0xd8},
	{0x3002, 0x00},
	{0x3004, 0x00},
	{0x3005, 0x00},
	{0x4800, 0x44},
	{0x4801, 0x0f},
	{0x4803, 0x05},
	{0x4601, 0x16},
	{0x3014, 0x05},
	{0x0101, 0x01},
	{0x3707, 0x14},
	{0x3622, 0x9f},
	{0x4002, 0x45},
	{0x5001, 0x00},
	{0x3406, 0x01},
	{0x3503, 0x17},
	{0x0205, 0x3f},
	{0x0100, 0x01},
	{0x0112, 0x0a},
	{0x0113, 0x0a},
	{0x3013, 0x20},
	{0x4837, 0x2f},
	{0x3615, 0xf0},
	{0x0340, 0x03},
	{0x0341, 0x48},
	{0x0342, 0x06},
	{0x0343, 0x80},
	{0x3702, 0x1e},
	{0x3703, 0x3c},
	{0x3704, 0x0e},

	{0x3104, 0x20},
	{0x0305, 0x04},
	{0x0307, 0x46},
	{0x0303, 0x01},
	{0x0301, 0x0a},
	{0x3010, 0x01},
	{0x460e, 0x00},

	{0x5000, 0x00},
	{0x5002, 0x00},
	{0x3017, 0xd2},
	{0x3018, 0x69},
	{0x3019, 0x96},
	{0x5047, 0x61},
	{0x3604, 0x1c},
	{0x3602, 0x10},
	{0x3612, 0x21},
	{0x3630, 0x0a},
	{0x3631, 0x53},
	{0x3633, 0x70},
	{0x4005, 0x1a},
	{0x4009, 0x10},

	{OV9726_TABLE_END, 0x0000}
};

static struct ov9726_reg mode_1280x800[] = {
	{0x0103, 0x01},

	{OV9726_TABLE_WAIT_MS, 10},

	{0x3026, 0x00},
	{0x3027, 0x00},
	{0x3705, 0x45},
	{0x3603, 0xaa},
	{0x3632, 0x2f},
	{0x3620, 0x66},
	{0x3621, 0xc0},
	{0x0202, 0x03},
	{0x0203, 0x13},
	{0x3833, 0x04},
	{0x3835, 0x02},
	{0x4702, 0x04},
	{0x4704, 0x00},
	{0x4706, 0x08},
	{0x5052, 0x01},
	{0x3819, 0x6c},
	{0x3817, 0x94},
	{0x404e, 0x7e},
	{0x3601, 0x40},
	{0x3610, 0xa0},

	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x00},
	{0x0347, 0x00},
	{0x034c, 0x05},
	{0x034d, 0x10},
	{0x034e, 0x03},
	{0x034f, 0x28},

	{0x3002, 0x00},
	{0x3004, 0x00},
	{0x3005, 0x00},
	{0x4800, 0x44},
	{0x4801, 0x0f},
	{0x4803, 0x05},
	{0x4601, 0x16},
	{0x3014, 0x05},
	{0x0101, 0x01},
	{0x3707, 0x14},
	{0x3622, 0x9f},
	{0x4002, 0x45},
	{0x5001, 0x00},
	{0x3406, 0x01},
	{0x3503, 0x17},
	{0x0205, 0x3f},
	{0x0100, 0x01},
	{0x0112, 0x0a},
	{0x0113, 0x0a},
	{0x3013, 0x20},
	{0x4837, 0x2f},
	{0x3615, 0xf0},
	{0x0340, 0x03},
	{0x0341, 0x48},
	{0x0342, 0x06},
	{0x0343, 0x80},
	{0x3702, 0x1e},
	{0x3703, 0x3c},
	{0x3704, 0x0e},

	{0x3104, 0x20},
	{0x0305, 0x04},
	{0x0307, 0x46},
	{0x0303, 0x01},
	{0x0301, 0x0a},
	{0x3010, 0x01},
	{0x460e, 0x00},

	{0x5000, 0x00},
	{0x5002, 0x00},
	{0x3017, 0xd2},
	{0x3018, 0x69},
	{0x3019, 0x96},
	{0x5047, 0x61},
	{0x3604, 0x1c},
	{0x3602, 0x10},
	{0x3612, 0x21},
	{0x3630, 0x0a},
	{0x3631, 0x53},
	{0x3633, 0x70},
	{0x4005, 0x1a},
	{0x4009, 0x10},

	{OV9726_TABLE_END, 0x0000}
};

enum {
	OV9726_MODE_1280x720,
	OV9726_MODE_1280x800,
};

static struct ov9726_reg *mode_table[] = {
	[OV9726_MODE_1280x720] = mode_1280x720,
	[OV9726_MODE_1280x800] = mode_1280x800,
};

static inline void
msleep_range(unsigned int delay_base)
{
	usleep_range(delay_base*1000, delay_base*1000 + 500);
}

static void ov9726_mclk_disable(struct ov9726_devinfo *info)
{
	dev_dbg(&info->i2c_client->dev, "%s: disable MCLK\n", __func__);
	clk_disable_unprepare(info->mclk);
}

static int ov9726_mclk_enable(struct ov9726_devinfo *info)
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

static inline int
ov9726_power_init(struct ov9726_devinfo *dev)
{
	struct i2c_client *i2c_client = dev->i2c_client;
	int err = 0;

	dev->power_rail.sen_1v8_reg = regulator_get(&i2c_client->dev, "dovdd");
	if (IS_ERR(dev->power_rail.sen_1v8_reg)) {
		dev_err(&i2c_client->dev, "%s: failed to get vdd\n",
			__func__);
		err = PTR_ERR(dev->power_rail.sen_1v8_reg);
		goto ov9726_power_init_end;
	}

	dev->power_rail.sen_2v8_reg = regulator_get(&i2c_client->dev, "avdd");
	if (IS_ERR(dev->power_rail.sen_2v8_reg)) {
		dev_err(&i2c_client->dev, "%s: failed to get vaa\n",
			__func__);
		err = PTR_ERR(dev->power_rail.sen_2v8_reg);

		regulator_put(dev->power_rail.sen_1v8_reg);
		dev->power_rail.sen_1v8_reg = NULL;
	}

ov9726_power_init_end:
	return err;
}

static inline void
ov9726_power_release(struct ov9726_devinfo *dev)
{
	regulator_put(dev->power_rail.sen_1v8_reg);
	regulator_put(dev->power_rail.sen_2v8_reg);
}

static int
ov9726_power(struct ov9726_devinfo *dev, bool pwr_on)
{
	struct i2c_client *i2c_client = dev->i2c_client;
	int rst_active_state = dev->pdata->rst_low_active ? 0 : 1;
	int pwdn_active_state = dev->pdata->pwdn_low_active ? 0 : 1;
	int ret = 0;

	dev_info(&i2c_client->dev, "%s %s\n", __func__, pwr_on ? "on" : "off");

	if (pwr_on) {
		ret = ov9726_mclk_enable(dev);
		if (ret) {
			dev_err(&i2c_client->dev, "%s: failed to enable mclk\n",
				__func__);
			goto fail_mclk;
		}

		/* pull low the RST pin of ov9726 first */
		gpio_set_value(dev->pdata->gpio_rst, rst_active_state);
		msleep_range(1);
		/* Plug 1.8V and 2.8V power to sensor */
		ret = regulator_enable(dev->power_rail.sen_1v8_reg);
		if (ret) {
			dev_err(&i2c_client->dev, "%s: failed to enable vdd\n",
				__func__);
			goto fail_regulator_1v8_reg;
		}

		msleep_range(20);

		ret = regulator_enable(dev->power_rail.sen_2v8_reg);
		if (ret) {
			dev_err(&i2c_client->dev, "%s: failed to enable vaa\n",
				__func__);
			goto fail_regulator_2v8_reg;
		}
		msleep_range(1);
		/* turn on ov9726 */
		gpio_set_value(dev->pdata->gpio_pwdn, !pwdn_active_state);

		msleep_range(5);
		/* release RST pin */
		gpio_set_value(dev->pdata->gpio_rst, !rst_active_state);
		msleep_range(20);

		/* Board specific power-on sequence */
		dev->pdata->power_on(&i2c_client->dev);
	} else {
		/* pull low the RST pin of ov9726 */
		gpio_set_value(dev->pdata->gpio_rst, rst_active_state);
		msleep_range(1);
		/* turn off ov9726 */
		gpio_set_value(dev->pdata->gpio_pwdn, pwdn_active_state);
		msleep_range(1);

		/* Unplug 1.8V and 2.8V power from sensor */
		regulator_disable(dev->power_rail.sen_2v8_reg);
		regulator_disable(dev->power_rail.sen_1v8_reg);

		/* Board specific power-down sequence */
		dev->pdata->power_off(&i2c_client->dev);
		ov9726_mclk_disable(dev);
	}

	return 0;

fail_regulator_2v8_reg:
	regulator_put(dev->power_rail.sen_2v8_reg);
	dev->power_rail.sen_2v8_reg = NULL;
	regulator_disable(dev->power_rail.sen_1v8_reg);
fail_regulator_1v8_reg:
	regulator_put(dev->power_rail.sen_1v8_reg);
	dev->power_rail.sen_1v8_reg = NULL;
fail_mclk:
	return ret;
}

static inline void
ov9726_get_frame_length_regs(struct ov9726_reg *regs, u32 frame_length)
{
	regs->addr = OV9726_REG_FRAME_LENGTH_HI;
	regs->val = (frame_length >> 8) & 0xff;
	regs++;
	regs->addr = OV9726_REG_FRAME_LENGTH_LO;
	regs->val = frame_length & 0xff;
}

static inline void
ov9726_get_coarse_time_regs(struct ov9726_reg *regs, u32 coarse_time)
{
	regs->addr = OV9726_REG_COARSE_TIME_HI;
	regs->val = (coarse_time >> 8) & 0xff;
	regs++;
	regs->addr = OV9726_REG_COARSE_TIME_LO;
	regs->val = coarse_time & 0xff;
}

static inline void
ov9726_get_gain_reg(struct ov9726_reg *regs, u16 gain)
{
	regs->addr = OV9726_REG_GAIN_HI;
	regs->val = (gain >> 8) & 0xff;
	regs++;
	regs->addr = OV9726_REG_GAIN_LO;
	regs->val = gain & 0xff;
}

static int
ov9726_read_reg8(struct i2c_client *client, u16 addr, u8 *val)
{
	int	err;
	struct i2c_msg  msg[2];
	unsigned char   data[3];

	if (!client->adapter)
		return -ENODEV;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = data;

	/* high byte goes out first */
	data[0] = (u8)(addr >> 8);
	data[1] = (u8)(addr & 0xff);

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = data + 2;

	err = i2c_transfer(client->adapter, msg, 2);

	if (err != 2)
		err = -EINVAL;
	else {
		*val = data[2];
		err = 0;
	}

	return err;
}

static int
ov9726_write_reg8(struct i2c_client *client, u16 addr, u8 val)
{
	int		err;
	struct i2c_msg	msg;
	unsigned char	data[3];
	int		retry = 0;

	if (!client->adapter)
		return -ENODEV;

	data[0] = (u8)(addr >> 8);
	data[1] = (u8)(addr & 0xff);
	data[2] = (u8)(val & 0xff);

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 3;
	msg.buf = data;

	do {
		err = i2c_transfer(client->adapter, &msg, 1);
		if (err == 1)
			break;

		retry++;
		dev_err(&client->dev,
			"ov9726: i2c transfer failed, retrying %x %x\n",
			addr, val);
		msleep_range(3);
	} while (retry <= OV9726_MAX_RETRIES);

	return (err != 1);
}

static int
ov9726_write_reg16(struct i2c_client *client, u16 addr, u16 val)
{
	int		count;
	struct i2c_msg  msg;
	unsigned char   data[4];
	int		retry = 0;

	if (!client->adapter)
		return -ENODEV;

	data[0] = (u8)(addr >> 8);
	data[1] = (u8)(addr & 0xff);
	data[2] = (u8)(val >> 8);
	data[3] = (u8)(val & 0xff);

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 4;
	msg.buf = data;

	do {
		count = i2c_transfer(client->adapter, &msg, 1);
		if (count == 1)
			return 0;

		retry++;
		dev_err(&client->dev,
			"ov9726: i2c transfer failed, retrying %x %x %x\n",
			addr, val, count);
		msleep_range(3);
	} while (retry <= OV9726_MAX_RETRIES);

	return -EIO;
}

static int
ov9726_write_table(
	struct i2c_client *client,
	struct ov9726_reg table[],
	struct ov9726_reg override_list[],
	int num_override_regs)
{
	const struct ov9726_reg	*next;
	int			err = 0;
	int			i;
	u16			val;

	dev_info(&client->dev, "ov9726_write_table\n");

	for (next = table; next->addr != OV9726_TABLE_END; next++) {

		if (next->addr == OV9726_TABLE_WAIT_MS) {
			msleep_range(next->val);
			continue;
		}

		val = next->val;

		/* When an override list is passed in, replace the reg */
		/* value to write if the reg is in the list */
		if (override_list) {
			for (i = 0; i < num_override_regs; i++) {
				if (next->addr == override_list[i].addr) {
					val = override_list[i].val;
					break;
				}
			}
		}

		err = ov9726_write_reg8(client, next->addr, val);
		if (err)
			break;
	}

	return err;
}

static int
ov9726_set_frame_length(struct i2c_client *i2c_client, u32 frame_length)
{
	int ret;

	dev_info(&i2c_client->dev, "[%s] (0x%08x)\n", __func__,  frame_length);
	/* hold register value */
	ret = ov9726_write_reg8(i2c_client, 0x104, 0x01);
	if (ret)
		return ret;

	ret = ov9726_write_reg16(i2c_client,
				  OV9726_REG_FRAME_LENGTH_HI,
				  frame_length);

	/* release hold, update register value */
	ret |= ov9726_write_reg8(i2c_client, 0x104, 0x00);

	return ret;
}

static int
ov9726_set_coarse_time(struct i2c_client *i2c_client, u32 coarse_time)
{
	int ret;

	dev_info(&i2c_client->dev, "[%s] (0x%08x)\n", __func__,  coarse_time);
	/* hold register value */
	ret = ov9726_write_reg8(i2c_client, 0x104, 0x01);
	if (ret)
		return ret;

	ret = ov9726_write_reg16(i2c_client,
			  OV9726_REG_COARSE_TIME_HI,
			  coarse_time);

	/* release hold, update register value */
	ret |= ov9726_write_reg8(i2c_client, 0x104, 0x00);

	return ret;
}

static int ov9726_set_gain(struct i2c_client *i2c_client, u16 gain)
{
	int ret;

	/* hold register value */
	ret = ov9726_write_reg8(i2c_client, 0x104, 0x01);
	if (ret)
		return ret;

	ret = ov9726_write_reg16(i2c_client, OV9726_REG_GAIN_HI, gain);

	/* release hold, update register value */
	ret |= ov9726_write_reg8(i2c_client, 0x104, 0x00);

	return ret;
}

static int ov9726_set_group_hold(struct ov9726_devinfo *dev,
			struct ov9726_ae *ae)
{
#define OV9726_REG_PUSH8(p, a, v) \
	do { \
		(p)->addr = (a); \
		(p)->val = (v); \
		(p)++; \
	} while (0)

#define OV9726_REG_PUSH16(ptr, addr, val) do { \
		OV9726_REG_PUSH8(ptr, (addr), (val) >> 8); \
		OV9726_REG_PUSH8(ptr, (addr) + 1, (val) & 0xff); \
	} while (0)

	struct ov9726_reg *gptr = &dev->grphold_temp[0];

	if (!ae->gain_enable &&
		!ae->coarse_time_enable &&
		!ae->frame_length_enable)
		return 0;

	OV9726_REG_PUSH8(gptr, 0x0104, 0x01);
	if (ae->gain_enable)
		OV9726_REG_PUSH16(gptr,
			OV9726_REG_GAIN_HI, ae->gain);
	if (ae->coarse_time_enable)
		OV9726_REG_PUSH16(gptr,
			OV9726_REG_COARSE_TIME_HI, ae->coarse_time);
	if (ae->frame_length_enable) {
		OV9726_REG_PUSH16(gptr,
			OV9726_REG_FRAME_LENGTH_HI, ae->frame_length);
	}
	OV9726_REG_PUSH8(gptr, 0x0104, 0x00);
	OV9726_REG_PUSH8(gptr, OV9726_TABLE_END, 0x00);

	return ov9726_write_table(dev->i2c_client,
				dev->grphold_temp, NULL, 0);
}

static int ov9726_get_status(struct i2c_client *i2c_client, u8 *status)
{
	int err;

	err = ov9726_read_reg8(i2c_client, 0x003, status);
	*status = 0;
	return err;
}

static int
ov9726_set_mode(
	struct ov9726_devinfo *dev,
	struct ov9726_mode *mode)
{
	struct i2c_client	*i2c_client = dev->i2c_client;
	struct ov9726_reg	reg_override[6];
	int			err = 0;
	int			sensor_mode;

	dev_info(&i2c_client->dev, "%s.\n", __func__);

	if (mode->xres == 1280 && mode->yres == 800)
		sensor_mode = OV9726_MODE_1280x800;
	else if (mode->xres == 1280 && mode->yres == 720)
		sensor_mode = OV9726_MODE_1280x720;
	else {
		dev_err(&i2c_client->dev,
			"%s: invalid resolution supplied to set mode %d %d\n",
			__func__, mode->xres, mode->yres);
		return -EINVAL;
	}

	ov9726_get_frame_length_regs(reg_override, mode->frame_length);
	ov9726_get_coarse_time_regs(reg_override + 2, mode->coarse_time);
	ov9726_get_gain_reg(reg_override + 4, mode->gain);

	if (dev->mode != mode->mode_id) {
		dev_info(&i2c_client->dev,
			"%s: xres %u yres %u framelen %u coarse %u gain %u\n",
			__func__, mode->xres, mode->yres, mode->frame_length,
			mode->coarse_time, mode->gain);

		err = ov9726_write_table(i2c_client,
			mode_table[sensor_mode], reg_override,
			sizeof(reg_override) / sizeof(reg_override[0]));
		if (err)
			goto ov9726_set_mode_exit;

		dev->mode = mode->mode_id;
	}

ov9726_set_mode_exit:
	return err;
}

static int ov9726_get_fuse_id(struct ov9726_devinfo *dev)
{
	int ret, i;
	__u8 bak;
	struct i2c_client *i2c_client = dev->i2c_client;

	if (dev->fuse_id.size)
		return 0;

	ret = ov9726_write_reg8(i2c_client, 0x3d21, 0x01);

	bak = 0x80;
	while (bak & 0x80)
		ret |= ov9726_read_reg8(i2c_client, 0x3d21, &bak);

	for (i = 0; i < OV9726_FUSE_ID_SIZE; i++) {
		ret |= ov9726_read_reg8(i2c_client,
				0x3d00 + i,
				&dev->fuse_id.data[i]);
	}
	ret = ov9726_write_reg8(i2c_client, 0x3d21, 0x00);

	if (!ret)
		dev->fuse_id.size = i;

	return ret;
}

static long
ov9726_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct ov9726_devinfo	*dev = file->private_data;
	struct i2c_client	*i2c_client = dev->i2c_client;
	int			 err = 0;

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(OV9726_IOCTL_SET_MODE):
	{
		struct ov9726_mode	mode;

		if (copy_from_user(&mode,
				   (const void __user *)arg,
				   sizeof(struct ov9726_mode))) {
			err = -EFAULT;
			break;
		}

		err = ov9726_set_mode(dev, &mode);

		break;
	}
	case _IOC_NR(OV9726_IOCTL_SET_FRAME_LENGTH):
		err = ov9726_set_frame_length(i2c_client, (u32)arg);
		break;
	case _IOC_NR(OV9726_IOCTL_SET_COARSE_TIME):
		err = ov9726_set_coarse_time(i2c_client, (u32)arg);
		break;
	case _IOC_NR(OV9726_IOCTL_SET_GAIN):
		err = ov9726_set_gain(i2c_client, (u16)arg);
		break;
	case _IOC_NR(OV9726_IOCTL_SET_GROUP_HOLD):
	{
		struct ov9726_ae ae;
		if (copy_from_user(&ae,
			(const void __user *)arg, sizeof(struct ov9726_ae))) {
			pr_info("%s %d\n", __func__, __LINE__);
			return -EFAULT;
		}
		err = ov9726_set_group_hold(dev, &ae);
		break;
	}
	case _IOC_NR(OV9726_IOCTL_GET_STATUS):
	{
		u8 status;

		err = ov9726_get_status(i2c_client, &status);
		if (!err) {
			if (copy_to_user((void __user *)arg,
					&status, sizeof(status)))
				err = -EFAULT;
		}
		break;
	}
	case _IOC_NR(OV9726_IOCTL_GET_FUSEID):
	{
		err = ov9726_get_fuse_id(dev);
		if (err) {
			pr_err("%s %d %d\n", __func__, __LINE__, err);
			return err;
		}
		if (copy_to_user((void __user *)arg,
				&dev->fuse_id,
				sizeof(struct nvc_fuseid))) {
			pr_err("%s: %d: fail copy fuse id to user space\n",
				__func__, __LINE__);
			return -EFAULT;
		}
		return 0;
	}
	default:
		err = -EINVAL;
		break;
	}

	return err;
}

static int ov9726_open(struct inode *inode, struct file *file)
{
	struct miscdevice	*miscdev = file->private_data;
	struct ov9726_devinfo	*dev;

	dev = container_of(miscdev, struct ov9726_devinfo, miscdev_info);
	/* check if device is in use */
	if (atomic_xchg(&dev->in_use, 1))
		return -EBUSY;
	dev->mode = (__u32)-1;
	file->private_data = dev;

	ov9726_power(dev, true);

	return 0;
}

int ov9726_release(struct inode *inode, struct file *file)
{
	struct ov9726_devinfo *dev;

	dev = file->private_data;
	file->private_data = NULL;

	ov9726_power(dev, false);

	/* warn if device already released */
	WARN_ON(!atomic_xchg(&dev->in_use, 0));
	return 0;
}

static const struct file_operations ov9726_fileops = {
	.owner		= THIS_MODULE,
	.open		= ov9726_open,
	.unlocked_ioctl	= ov9726_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ov9726_ioctl,
#endif
	.release	= ov9726_release,
};

static struct miscdevice ov9726_device = {
	.name		= "ov9726",
	.minor		= MISC_DYNAMIC_MINOR,
	.fops		= &ov9726_fileops,
};

static int
ov9726_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ov9726_devinfo	*dev;
	int			err = 0;
	const char		*mclk_name;

	dev_info(&client->dev, "ov9726: probing sensor.\n");

	dev = devm_kzalloc(&client->dev,
		sizeof(struct ov9726_devinfo), GFP_KERNEL);
	if (!dev) {
		dev_err(&client->dev, "ov9726: Unable to allocate memory!\n");
		err = -ENOMEM;
		goto probe_end;
	}

	memcpy(&(dev->miscdev_info),
		&ov9726_device,
		sizeof(struct miscdevice));

	err = misc_register(&(dev->miscdev_info));
	if (err) {
		dev_err(&client->dev, "ov9726: Unable to register misc device!\n");
		goto probe_end;
	}

	dev->pdata = client->dev.platform_data;
	dev->i2c_client = client;

	mclk_name = dev->pdata->mclk_name ?
		    dev->pdata->mclk_name : "default_mclk";
	dev->mclk = devm_clk_get(&client->dev, mclk_name);
	if (IS_ERR(dev->mclk)) {
		dev_err(&client->dev, "%s: unable to get clock %s\n",
			__func__, mclk_name);
		err = PTR_ERR(dev->mclk);
		goto probe_end;
	}

	atomic_set(&dev->in_use, 0);
	i2c_set_clientdata(client, dev);

	err = ov9726_power_init(dev);

probe_end:
	if (err)
		dev_err(&client->dev, "failed.\n");

	return err;
}

static int ov9726_remove(struct i2c_client *client)
{
	struct ov9726_devinfo	*dev;

	dev = i2c_get_clientdata(client);
	i2c_set_clientdata(client, NULL);
	misc_deregister(&ov9726_device);
	ov9726_power_release(dev);

	return 0;
}

static const struct i2c_device_id ov9726_id[] = {
	{"ov9726", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, ov9726_id);

static struct i2c_driver ov9726_i2c_driver = {
	.driver	= {
		.name   = "ov9726",
		.owner  = THIS_MODULE,
		},
	.probe  = ov9726_probe,
	.remove = ov9726_remove,
	.id_table   = ov9726_id,
};

static int __init ov9726_init(void)
{
	pr_info("ov9726 sensor driver loading\n");
	return i2c_add_driver(&ov9726_i2c_driver);
}

static void __exit ov9726_exit(void)
{
	i2c_del_driver(&ov9726_i2c_driver);
}

module_init(ov9726_init);
module_exit(ov9726_exit);
MODULE_LICENSE("GPL v2");
