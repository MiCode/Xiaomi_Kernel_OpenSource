/*
 * Driver for MAXIM CMOS Image Sensor from MAXIM
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/videodev2.h>
#include <linux/module.h>
#include <linux/v4l2-mediabus.h>
#include <linux/media-bus-format.h>

#include <linux/clk.h>
#include <media/soc_camera.h>
#include <media/v4l2-clk.h>
#include <media/v4l2-subdev.h>

/*
 * About MAX resolution, cropping and binning:
 * This sensor supports it all, at least in the feature description.
 * Unfortunately, no combination of appropriate registers settings could make
 * the chip work the intended way. As it works with predefined register lists,
 * some undocumented registers are presumably changed there to achieve their
 * goals.
 * This driver currently only works for resolutions up to 720 lines with a
 * 1:1 scale. Hopefully these restrictions will be removed in the future.
 */
#define MAXIM_MAX_WIDTH	2592
#define MAXIM_MAX_HEIGHT	720
#define MAX9286_ADDR 0x90
#define MAX9286_ID 0x40
#define MAX_REG_LEN 4
#define MAX9271_INIT_ADDR 0x80
#define MAX9271_ALL_ADDR 0x8A
#define MAX9271_CH0_ADDR 0x82
#define MAX9271_CH1_ADDR 0x84
#define MAX9271_CH2_ADDR 0x86
#define MAX9271_CH3_ADDR 0x88
#define OV490_CH0_ADDR 0x62
#define OV490_INIT_ADDR 0x48
#define OV490_CH0_MAP_ADDR 0x60
#define OV490_CH1_MAP_ADDR 0x62
#define OV490_CH2_MAP_ADDR 0x64
#define OV490_CH3_MAP_ADDR 0x66
#define MAX9286_1CH_LANE 1
//#define CAB888_TEST_PATTERN


#define OV490_ID 0xB888
#define MAX9286_LINK_CONFIG_REG 0x00
#define MAX9286_ID_REG   0x1E
#define MAX9286_LOCK_REG 0x27
#define MAX9286_LINK_REG 0x49

/* minimum extra blanking */
#define BLANKING_EXTRA_WIDTH		500
#define BLANKING_EXTRA_HEIGHT		20

/*
 * the sensor's autoexposure is buggy when setting total_height low.
 * It tries to expose longer than 1 frame period without taking care of it
 * and this leads to weird output. So we set 1000 lines as minimum.
 */
#define BLANKING_MIN_HEIGHT	1000
#define SENSOR_ID               (0x86)

#define max9286_info(fmt, args...)                \
		pr_info("[max9286][info] %s %d: " fmt "\n",\
			__func__, __LINE__, ##args)

#define max9286_err(fmt, args...)                \
		pr_info("[max9286][error] %s %d: " fmt "\n",\
			__func__, __LINE__, ##args)

struct reg_val_ops {
	u16 slave_addr;
	u8 reg[MAX_REG_LEN];
	u8 val;
	u8 reg_len;
	int (*i2c_ops)(struct i2c_client *client, u16 slave_addr,
		u8 *reg, u8 reg_len, u8 *val);
};

static int i2c_write(struct i2c_client *client, u16 slave_addr,
		u8 *reg, u8 reg_len, u8 *val);
static int i2c_read(struct i2c_client *client, u16 slave_addr,
		u8 *reg, u8 reg_len, u8 *val);
static int read_max9286_id(struct i2c_client *client, u8 *id_val);
static int max9286_write_array(struct i2c_client *client,
		struct reg_val_ops *cmd, int len);

/* Supported resolutions */
enum max9286_width {
	W_TESTWIDTH	= 720,
	W_WIDTH		= 736,
};

enum max9286_height {
	H_1CHANNEL	= 480,
	H_4CHANNEL	= 1920,
};

struct max9286_win_size {
	enum max9286_width width;
	enum max9286_height height;
};

static const struct max9286_win_size max9286_supported_win_sizes[] = {
	{ W_TESTWIDTH,	H_1CHANNEL },
	{ W_TESTWIDTH,	H_4CHANNEL },
	{ W_WIDTH,	H_1CHANNEL },
	{ W_WIDTH,	H_4CHANNEL },
};

static int is_testpattern;

struct sensor_addr {
	u16 ser_init_addr;
	u16 ser_last_addr;
	u16 isp_init_addr;
	u16 isp_map_addr;
};

#define CAB888_WIDTH 1280
#define CAB888_HEIGHT 720

static struct reg_val_ops MAX9286_CAB888_4ch_4lane_init_cmd[] = {
	{MAX9286_ADDR,      {0x0D}, 0x03,               0x01, i2c_write},
	{MAX9286_ADDR,      {0x3F}, 0x4F,               0x01, i2c_write},
	{MAX9286_ADDR,      {0x3B}, 0x1E,               0x01, i2c_write},
	{MAX9286_ADDR,      {0x28}, 0x00,               0x01, i2c_read},
	{MAX9286_ADDR,      {0x28}, 0x00,               0x01, i2c_read},
	{MAX9286_ADDR,      {0x29}, 0x00,               0x01, i2c_read},
	{MAX9286_ADDR,      {0x29}, 0x00,               0x01, i2c_read},
	{MAX9286_ADDR,      {0x2A}, 0x00,               0x01, i2c_read},
	{MAX9286_ADDR,      {0x2A}, 0x00,               0x01, i2c_read},
	{MAX9286_ADDR,      {0x2B}, 0x00,               0x01, i2c_read},
	{MAX9286_ADDR,      {0x2B}, 0x00,               0x01, i2c_read},
	{MAX9271_INIT_ADDR, {0x08}, 0x01,               0x01, i2c_write},
	{MAX9271_INIT_ADDR, {0x06}, 0xA0,               0x01, i2c_write},
	{MAX9286_ADDR,      {0x15}, 0x13,               0x01, i2c_write},
	{MAX9271_INIT_ADDR, {0x04}, 0x43,               0x01, i2c_write},
	{MAX9286_ADDR,      {0x3B}, 0x19,               0x01, i2c_write},
	{MAX9286_ADDR,      {0x12}, 0xF3,               0x01, i2c_write},
	{MAX9286_ADDR,      {0x01}, 0x02,               0x01, i2c_write},
	{MAX9286_ADDR,      {0x02}, 0x20,               0x01, i2c_write},
	{MAX9286_ADDR,      {0x63}, 0x00,               0x01, i2c_write},
	{MAX9286_ADDR,      {0x64}, 0x00,               0x01, i2c_write},
	{MAX9286_ADDR,      {0x05}, 0x19,               0x01, i2c_write},
	{MAX9286_ADDR,      {0x00}, 0xEF,               0x01, i2c_write},
	{MAX9286_ADDR,      {0x0A}, 0xFF,               0x01, i2c_write},
	{MAX9271_INIT_ADDR, {0x07}, 0x84,               0x01, i2c_write},

	{MAX9286_ADDR,      {0x0A}, 0xF1,               0x01, i2c_write},
	{MAX9271_INIT_ADDR, {0x00}, MAX9271_CH0_ADDR,   0x01, i2c_write},
	{MAX9271_CH0_ADDR,  {0x07}, 0x84,               0x01, i2c_write},
	{MAX9271_CH0_ADDR,  {0x09}, 0x62,               0x01, i2c_write},
	{MAX9271_CH0_ADDR,  {0x0A}, OV490_CH0_MAP_ADDR, 0x01, i2c_write},
	{MAX9271_CH0_ADDR,  {0x0B}, 0x8A,               0x01, i2c_write},
	{MAX9271_CH0_ADDR,  {0x0C}, MAX9271_CH0_ADDR,   0x01, i2c_write},

	{MAX9286_ADDR,      {0x0A}, 0xF2,               0x01, i2c_write},
	{MAX9271_INIT_ADDR, {0x00}, MAX9271_CH1_ADDR,   0x01, i2c_write},
	{MAX9271_CH1_ADDR,  {0x07}, 0x84,               0x01, i2c_write},
	{MAX9271_CH1_ADDR,  {0x09}, 0x62,               0x01, i2c_write},
	{MAX9271_CH1_ADDR,  {0x0A}, OV490_CH1_MAP_ADDR, 0x01, i2c_write},
	{MAX9271_CH1_ADDR,  {0x0B}, 0x8A,               0x01, i2c_write},
	{MAX9271_CH1_ADDR,  {0x0C}, MAX9271_CH1_ADDR,   0x01, i2c_write},

	{MAX9286_ADDR,      {0x0A}, 0xF4,               0x01, i2c_write},
	{MAX9271_INIT_ADDR, {0x00}, MAX9271_CH2_ADDR,   0x01, i2c_write},
	{MAX9271_CH2_ADDR,  {0x07}, 0x84,               0x01, i2c_write},
	{MAX9271_CH2_ADDR,  {0x09}, 0x62,               0x01, i2c_write},
	{MAX9271_CH2_ADDR,  {0x0A}, OV490_CH2_MAP_ADDR, 0x01, i2c_write},
	{MAX9271_CH2_ADDR,  {0x0B}, 0x8A,               0x01, i2c_write},
	{MAX9271_CH2_ADDR,  {0x0C}, MAX9271_CH2_ADDR,   0x01, i2c_write},

	{MAX9286_ADDR,      {0x0A}, 0xF8,               0x01, i2c_write},
	{MAX9271_INIT_ADDR, {0x00}, MAX9271_CH3_ADDR,   0x01, i2c_write},
	{MAX9271_CH3_ADDR,  {0x07}, 0x84,               0x01, i2c_write},
	{MAX9271_CH3_ADDR,  {0x09}, 0x68,               0x01, i2c_write},
	{MAX9271_CH3_ADDR,  {0x0A}, OV490_CH3_MAP_ADDR, 0x01, i2c_write},
	{MAX9271_CH3_ADDR,  {0x0B}, 0x8A,               0x01, i2c_write},
	{MAX9271_CH3_ADDR,  {0x0C}, MAX9271_CH3_ADDR,   0x01, i2c_write},
	{MAX9286_ADDR,      {0x0A}, 0xFF,               0x01, i2c_write},
	{MAX9271_ALL_ADDR,  {0x04}, 0x83,               0x01, i2c_write},
	{MAX9286_ADDR,      {0x15}, 0x9B,               0x01, i2c_write},
};

static struct reg_val_ops MAX9286_CAB888_1ch_1lane_init_cmd[] = {
	{MAX9286_ADDR,     {0x0D},       0x03,             0x01, i2c_write},
	{MAX9286_ADDR,     {0x3F},       0x4F,             0x01, i2c_write},
	{MAX9286_ADDR,     {0x3B},       0x1E,             0x01, i2c_write},
	{0x80,             {0x04},       0x07,             0x01, i2c_write},
	{0x80,             {0x06},       0xA0,             0x01, i2c_write},
	{MAX9286_ADDR,     {0x15},       0x03,             0x01, i2c_write},
#if MAX9286_1CH_LANE == 1
	{MAX9286_ADDR,     {0x12},       0x33,             0x01, i2c_write},
#elif MAX9286_1CH_LANE == 4
	{MAX9286_ADDR,     {0x12},       0xF3,             0x01, i2c_write},
#endif
	{MAX9286_ADDR,     {0x01},       0xE2,             0x01, i2c_write},
	{MAX9286_ADDR,     {0x00},       0x81,             0x01, i2c_write},
	{MAX9286_ADDR,     {0x0A},       0xF1,             0x01, i2c_write},
	{0x80,             {0x00},       MAX9271_CH0_ADDR, 0x01, i2c_write},
	{MAX9271_CH0_ADDR, {0x07},       0x84,             0x01, i2c_write},
	{MAX9271_CH0_ADDR, {0x09},       OV490_CH0_ADDR,   0x01, i2c_write},
	{MAX9271_CH0_ADDR, {0x0A},       OV490_INIT_ADDR,  0x01, i2c_write},
	{MAX9271_CH0_ADDR, {0x0B},       0x8A,             0x01, i2c_write},
	{MAX9271_CH0_ADDR, {0x0C},       MAX9271_CH0_ADDR, 0x01, i2c_write},
	{MAX9286_ADDR,     {0x34},       0xB6,             0x01, i2c_write},
	{MAX9271_CH0_ADDR, {0x04},       0xC7,             0x01, i2c_write},
	{MAX9286_ADDR,     {0x15},       0x0B,             0x01, i2c_write},
#ifdef CAB888_TEST_PATTERN
	{OV490_CH0_ADDR,   {0xFF, 0xFD}, 0x80,             0x02, i2c_write},
	{OV490_CH0_ADDR,   {0xFF, 0xFE}, 0x19,             0x02, i2c_write},
	{OV490_CH0_ADDR,   {0x50, 0x00}, 0x03,             0x02, i2c_write},
	{OV490_CH0_ADDR,   {0xFF, 0xFE}, 0x80,             0x02, i2c_write},
	{OV490_CH0_ADDR,   {0x00, 0xC0}, 0xD6,             0x02, i2c_write},
#endif
};

struct max9286_datafmt {
	int code;
	enum v4l2_colorspace colorspace;
};

struct max9286 {
	struct v4l2_subdev		subdev;
	const struct max9286_datafmt	*fmt;
	struct v4l2_rect                crop_rect;
	struct v4l2_clk			*clk;

	/* blanking information */
	int total_width;
	int total_height;
};

static const struct max9286_datafmt max9286_colour_fmts[] = {
	{MEDIA_BUS_FMT_YUYV8_2X8},
};

static struct max9286 *to_max9286(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client), struct max9286, subdev);
}

static int i2c_read(struct i2c_client *client, u16 slave_addr,
		u8 *reg, u8 reg_len, u8 *val)
{
	int ret = 0;
	u8 *data = NULL;
	struct i2c_msg msg[2];

	if (!reg || !val || (reg_len <= 0)) {
		max9286_err("reg/val/reg_len is %x/%x/%d", *reg, *val, reg_len);
		return -EINVAL;
	}

	data = kzalloc(reg_len + 1, GFP_KERNEL);
	if (!data)
		return -ENOSPC;

	memcpy(data, reg, reg_len);
	memset(msg, 0, sizeof(msg));

	msg[0].addr = (slave_addr >> 1);
	msg[0].flags = 0;
	msg[0].len = reg_len;
	msg[0].buf = data;

	msg[1].addr = (slave_addr >> 1);
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = data + reg_len;

	client->addr = (slave_addr >> 1);

	ret = i2c_transfer(client->adapter, msg, 2);

	*val = *(data + reg_len);
	kfree(data);
	data = NULL;

	return ret;
}

static int i2c_write(struct i2c_client *client, u16 slave_addr,
		u8 *reg, u8 reg_len, u8 *val)
{
	u8 *data = NULL;
	int ret = 0;
	struct i2c_msg msg;

	if (!reg || !val || (reg_len <= 0)) {
		max9286_err("reg/val/reg_len is %x/%x/%d", *reg, *val, reg_len);
		return -EINVAL;
	}

	data = kzalloc(reg_len + 1, GFP_KERNEL);
	if (!data)
		return -ENOSPC;

	memcpy(data, reg, reg_len);
	*(data + reg_len) = *val;

	memset(&msg, 0, sizeof(msg));
	msg.addr = (slave_addr >> 1);
	msg.flags = 0;
	msg.len = reg_len + 1;
	msg.buf = data;

	client->addr = (slave_addr >> 1);
	ret = i2c_transfer(client->adapter, &msg, 1);

	kfree(data);
	data = NULL;
	return ret;
}

static int read_max9286_id(struct i2c_client *client, u8 *id_val)
{
	int ret = 0;
	u8 max9286_id_reg = MAX9286_ID_REG;

	ret = i2c_read(client, MAX9286_ADDR, &max9286_id_reg, 1, id_val);
	if (ret != 2) {
		max9286_err("ret=%d", ret);
		return -EIO;
	}

	if (*id_val != MAX9286_ID) {
		max9286_err("max9286 ID not match. Default is %x but read from register is %x. ret=%d",
			MAX9286_ID, *id_val, ret);
		ret = -ENODEV;
	} else {
		max9286_info("max9286 ID match. Default is %x and read from register is %x. ret=%d",
			MAX9286_ID, *id_val, ret);
	}

	return ret;
}

static int max9286_write_array(struct i2c_client *client,
		struct reg_val_ops *cmd, int len)
{
	int ret = 0;
	int index = 0;

	for (; index < len; ++index) {
		ret = i2c_write(client, cmd[index].slave_addr,
			cmd[index].reg, cmd[index].reg_len, &(cmd[index].val));
		if (ret != 1)
			max9286_err("ret = %d index = %d", ret, index);
	}

	return ret;
}

static int max9286_set_link_config(struct i2c_client *client, u8 *val)
{
	int ret = 0;
	u8 link_config_reg = MAX9286_LINK_CONFIG_REG;

	ret = i2c_write(client, MAX9286_ADDR, &link_config_reg, 1, val);
	if (ret != 1) {
		max9286_info("link config set fail. ret = %d", ret);
		return -EIO;
	}

	return ret;
}

static int max9286_get_lock_status(struct i2c_client *client, u8 *val)
{
	int ret = 0;
	u8 lock_reg = MAX9286_LOCK_REG;

	ret = i2c_read(client, MAX9286_ADDR, &lock_reg, 1, val);
	if (ret != 2) {
		max9286_err("ret=%d", ret);
		return -EIO;
	}

	if (*val & 0x80) {
		max9286_info("camera links are locked");
	} else {
		max9286_err("camera links are not locked");
		ret =  -ENODEV;
	}

	return ret;
}

static int max9286_get_link(struct i2c_client *client, u8 *val)
{
	int ret = 0;
	u8 link_reg = MAX9286_LINK_REG;

	ret = i2c_read(client, MAX9286_ADDR, &link_reg, 1, val);
	if (ret != 2) {
		max9286_err("ret=%d", ret);
		ret = -EIO;
	}

	return ret;
}

static int max9286_camera_init(struct i2c_client *client)
{
	int ret = 0;
	u8 max9286_id_val = 0;
	u8 lock_reg_val = 0;
	u8 link_config_reg_val = 0x81;
	u8 link_reg_val = 0;
	u8 ch_link_status[4] = {0};
	int index = 0;
	int read_cnt = 0;
	int link_cnt = 0;

	/*MAX9286 ID confirm*/
	for (read_cnt = 0; read_cnt < 3; ++read_cnt) {
		ret = read_max9286_id(client, &max9286_id_val);
		if (ret == 2) {
			break;
		} else if ((read_cnt == 2) && (ret != 2)) {
			max9286_err("read max9286 ID time out");
			return -EIO;
		}

		mdelay(10);
	}

	/*check video link*/
	for (read_cnt = 0; read_cnt < 3; ++read_cnt) {
		ret = max9286_get_link(client, &link_reg_val);
		if (ret == 2 && link_reg_val) {
			break;
		} else if ((read_cnt == 2) && (ret != 2) && !link_reg_val) {
			max9286_err("no camera linked link_reg_val=%x",
				link_reg_val);
			return -ENODEV;
		}

		mdelay(10);
	}

	for (index = 0; index < 4; ++index) {
		ch_link_status[index] = link_reg_val & 0x1;
		link_reg_val >>= 1;

		if (ch_link_status[index]) {
			++link_cnt;
			max9286_info("channel %d linked", index);
		} else {
			max9286_info("channel %d not linked", index);
		}
	}

	/* If link_cnt < 4. Link config should be set or lock status error */
	if (link_cnt == 1) {
		ret = max9286_set_link_config(client, &link_config_reg_val);
		if (ret != 1)
			return -EIO;

	}

	/* check camera links are locked or not */
	for (read_cnt = 0; read_cnt < 3; ++read_cnt) {
		ret = max9286_get_lock_status(client, &lock_reg_val);
		if (ret == 2) {
			break;
		} else if ((read_cnt == 2) && (ret != 2)) {
			max9286_err("read lock status time out");
			return -EIO;
		}

		mdelay(10);
	}

	/*init max9286 command*/
	if (link_cnt == 1) {
		ret = max9286_write_array(client,
				MAX9286_CAB888_1ch_1lane_init_cmd,
				ARRAY_SIZE(MAX9286_CAB888_1ch_1lane_init_cmd));
	} else if (link_cnt == 4) {
		ret = max9286_write_array(client,
				MAX9286_CAB888_4ch_4lane_init_cmd,
				ARRAY_SIZE(MAX9286_CAB888_4ch_4lane_init_cmd));
	} else {
		max9286_err("error link cnt %d", link_cnt);
		ret = -EINVAL;
	}

	return ret;
}

static int max9286_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	cfg->type = V4L2_MBUS_CSI2;
	cfg->flags = V4L2_MBUS_CSI2_1_LANE | V4L2_MBUS_CSI2_CHANNEL_0 |
		     V4L2_MBUS_CSI2_CONTINUOUS_CLOCK;

	return 0;
}

static int max9286_s_stream(struct v4l2_subdev *sd, int enable)
{
	return 0;
}

static int max9286_s_mbus_config(struct v4l2_subdev *sd,
			     const struct v4l2_mbus_config *cfg)
{
	return 0;
}

static int max9286_s_power(struct v4l2_subdev *sd, int on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct soc_camera_subdev_desc *ssdd = soc_camera_i2c_to_desc(client);
	struct max9286 *priv = to_max9286(client);

	if (!on)
		return soc_camera_power_off(&client->dev, ssdd, priv->clk);

	return soc_camera_power_on(&client->dev, ssdd, priv->clk);
}

static int max9286_enum_mbus_code(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg,
		struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->pad ||
	    (unsigned int)code->index >= ARRAY_SIZE(max9286_colour_fmts))
		return -EINVAL;

	code->code = max9286_colour_fmts[code->index].code;
	return 0;
}

static const struct max9286_datafmt *max9286_find_datafmt(u32 code)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(max9286_colour_fmts); i++)
		if (max9286_colour_fmts[i].code == code)
			return max9286_colour_fmts + i;

	return NULL;
}

static int max9286_set_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg,
		struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *mf = &format->format;
	const struct max9286_datafmt *fmt = max9286_find_datafmt(mf->code);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct max9286 *priv = to_max9286(client);

	if (format->pad)
		return -EINVAL;

	if (!fmt) {
		/* MIPI CSI could have changed the format, double-check */
		if (format->which == V4L2_SUBDEV_FORMAT_ACTIVE)
			return -EINVAL;
		mf->code	= max9286_colour_fmts[0].code;
		mf->colorspace	= max9286_colour_fmts[0].colorspace;
	}

	mf->width	= CAB888_WIDTH;
	mf->height	= CAB888_HEIGHT;
	mf->field	= V4L2_FIELD_NONE;

	if (format->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		priv->fmt = max9286_find_datafmt(mf->code);
	else
		cfg->try_fmt = *mf;

	return 0;
}

static int max9286_g_register(struct v4l2_subdev *sd,
		struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;
	u8 val = 0;
	u8 max9286_reg = (u8)(reg->reg);

	if (!reg)
		return -EINVAL;

	max9286_info("read max9286 reg %llx", reg->reg);
	if (reg->match.type == 0) {
		ret = i2c_read(client, MAX9286_ADDR, &max9286_reg, 1, &val);
		if (ret != 2) {
			max9286_err("ret=%d", ret);
			return -EIO;
		}
		reg->val = val;
	}

	return ret;
}

static const struct v4l2_subdev_video_ops max9286_subdev_video_ops = {
	.g_mbus_config	= max9286_g_mbus_config,
	.s_stream	= max9286_s_stream,
	.s_mbus_config = max9286_s_mbus_config,
};

static const struct v4l2_subdev_core_ops max9286_subdev_core_ops = {
	.s_power	= max9286_s_power,
	.g_register	= max9286_g_register,
};

static const struct v4l2_subdev_pad_ops max9286_subdev_pad_ops = {
	.enum_mbus_code = max9286_enum_mbus_code,
	.set_fmt	= max9286_set_fmt,
};

static const struct v4l2_subdev_ops max9286_subdev_ops = {
	.core	= &max9286_subdev_core_ops,
	.video	= &max9286_subdev_video_ops,
	.pad = &max9286_subdev_pad_ops,
};

static int max9286_probe(struct i2c_client *client,
			const struct i2c_device_id *did)
{
	struct max9286 *priv = NULL;
	struct soc_camera_subdev_desc *ssdd = soc_camera_i2c_to_desc(client);
	struct v4l2_subdev *subdev = NULL;
	int ret = 0;

	if (client->dev.of_node) {
		ssdd = devm_kzalloc(&client->dev, sizeof(*ssdd), GFP_KERNEL);
		if (!ssdd)
			return -ENOMEM;
		client->dev.platform_data = ssdd;
	}

	if (!ssdd) {
		max9286_err("MAX9286: missing platform data");
		return -EINVAL;
	}

	priv = devm_kzalloc(&client->dev, sizeof(struct max9286), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	v4l2_i2c_subdev_init(&priv->subdev, client, &max9286_subdev_ops);

	priv->fmt		= &max9286_colour_fmts[0];

	priv->crop_rect.width	= W_TESTWIDTH;
	priv->crop_rect.height	= H_1CHANNEL;
	priv->crop_rect.left	= (MAXIM_MAX_WIDTH - W_TESTWIDTH) / 2;
	priv->crop_rect.top	= (MAXIM_MAX_HEIGHT - H_1CHANNEL) / 2;
	priv->total_width = W_TESTWIDTH + BLANKING_EXTRA_WIDTH;
	priv->total_height = BLANKING_MIN_HEIGHT;

	subdev = i2c_get_clientdata(client);

	ret =  max9286_camera_init(client);
	if (ret < 0)
		return ret;

	priv->subdev.dev = &client->dev;

	return v4l2_async_register_subdev(&priv->subdev);
}

static int max9286_remove(struct i2c_client *client)
{
	struct soc_camera_subdev_desc *ssdd = soc_camera_i2c_to_desc(client);

	if (ssdd->free_bus)
		ssdd->free_bus(ssdd);

	return 0;
}

static const struct i2c_device_id max9286_id[] = {
	{ "max9286", 0},
	{ }
};
static const struct of_device_id max9286_camera_of_match[] = {
	{ .compatible = "maxim,max9286", },
	{},
};

static struct i2c_driver max9286_i2c_driver = {
	.driver = {
		.name = "max9286",
		.of_match_table = max9286_camera_of_match,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
	.probe		= max9286_probe,
	.remove		= max9286_remove,
	.id_table	= max9286_id,
};

module_param(is_testpattern, int, 0644);
MODULE_PARM_DESC(is_testpattern, "Whether the MAX9286 get test pattern data");
module_i2c_driver(max9286_i2c_driver);

MODULE_DESCRIPTION("MAXIM Camera driver");
MODULE_AUTHOR("Baoyin Zhang<baoyin.zhang@mediatek.com");
MODULE_LICENSE("GPL v2");
