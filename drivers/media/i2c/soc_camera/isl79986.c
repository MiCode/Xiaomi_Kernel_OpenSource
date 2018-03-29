/*
 * Driver for INTERSIL CMOS Image Sensor from INTERSIL
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
#include <linux/clk.h>
#include <media/soc_camera.h>
#include <media/v4l2-clk.h>
#include <media/v4l2-subdev.h>

/*
 * About INTERSIL resolution, cropping and binning:
 * This sensor supports it all, at least in the feature description.
 * Unfortunately, no combination of appropriate registers settings could make
 * the chip work the intended way. As it works with predefined register lists,
 * some undocumented registers are presumably changed there to achieve their
 * goals.
 * This driver currently only works for resolutions up to 720 lines with a
 * 1:1 scale. Hopefully these restrictions will be removed in the future.
 */
#define INTERSIL_MAX_WIDTH	2592
#define INTERSIL_MAX_HEIGHT	720

/* minimum extra blanking */
#define BLANKING_EXTRA_WIDTH		500
#define BLANKING_EXTRA_HEIGHT		20

/*
 * the sensor's autoexposure is buggy when setting total_height low.
 * It tries to expose longer than 1 frame period without taking care of it
 * and this leads to weird output. So we set 1000 lines as minimum.
 */
#define BLANKING_MIN_HEIGHT		1000
#define SENSOR_ID                   (0x86)

/* Supported resolutions */
enum isl79986_width {
	W_TESTWIDTH	= 720,
	W_WIDTH		= 736,
};

enum isl79986_height {
	H_1CHANNEL	= 480,
	H_4CHANNEL	= 1920,
};

struct isl79986_win_size {
	enum isl79986_width width;
	enum isl79986_height height;
};

static const struct isl79986_win_size isl79986_supported_win_sizes[] = {
	{ W_TESTWIDTH,	H_1CHANNEL },
	{ W_TESTWIDTH,	H_4CHANNEL },
	{ W_WIDTH,	H_1CHANNEL },
	{ W_WIDTH,	H_4CHANNEL },
};

static int is_testpattern;

struct regval_list {
	u16 reg_num;
	u8 value;
};

/*INTERSIL for 480I setting*/
static const struct regval_list isl79986_default_regs_init[] = {
	{ 0xff, 0x0f },
	{ 0x37, 0x47 },
	{ 0x39, 0x02 },
	{ 0x33, 0x85 },
	{ 0x2f, 0xe6 },
	{ 0xff, 0x05 },
	{ 0x0a, 0x62 },
	{ 0x11, 0xa0 },
	{ 0x34, 0x10 },
	{ 0x00, 0x02 },
	{ 0xff, 0x00 },
	{ 0x03, 0x00 },
	{ 0x04, 0x08 },
	{ 0x08, 0x1f },
	{ 0x07, 0x12 },
	{ 0x03, 0x03 },
	{ 0x04, 0x0a },
	{ 0xff, 0x01 },
	{ 0x43, 0x00 },
	/* soft reset */
	{ 0xff, 0x05 },
	{ 0x00, 0x80 },
	{ 0xff, 0x00 },
	{ 0x02, 0x1f },
	/* resume */
	{ 0xff, 0x00 },
	{ 0x02, 0x00 },
	{ 0xff, 0x05 },
	{ 0x00, 0x00 },
	/* reset to 1 channel*/
	{ 0xff, 0x00 },
	{ 0x07, 0x10 },
	{ 0x08, 0x1f },
	{ 0xff, 0x05 },
	{ 0x04, 0xe4 },
	/* regs list end flag reg_num = 0x100 */
	{ 0x100, 0xff }
};

static const struct regval_list isl79986_4_channel_reg_init[] = {
	{ 0xff, 0x0f },
	{ 0x33, 0x85 },
	{ 0x2f, 0xe6 },
	{ 0xff, 0x05 },
	{ 0x0a, 0x67 },
	{ 0x11, 0xa0 },
	{ 0x34, 0x10 },
	{ 0x00, 0x02 },
	{ 0xff, 0x00 },
	{ 0x03, 0x00 },
	{ 0x04, 0x08 },
	{ 0x08, 0x1f },
	{ 0x07, 0x12 },
	{ 0x03, 0x03 },
	{ 0x04, 0x0a },
	/* fix screen tingle */
	{ 0xff, 0x01 },
	{ 0x43, 0x00 },
	{ 0xff, 0x02 },
	{ 0x43, 0x00 },
	{ 0xff, 0x03 },
	{ 0x43, 0x00 },
	{ 0xff, 0x04 },
	{ 0x43, 0x00 },
	/* soft reset */
	{ 0xff, 0x05 },
	{ 0x00, 0x80 },
	{ 0xff, 0x00 },
	{ 0x02, 0x1f },
	/* resume */
	{ 0xff, 0x00 },
	{ 0x02, 0x00 },
	{ 0xff, 0x05 },
	{ 0x00, 0x00 },
	/* regs list end flag reg_num = 0x100 */
	{ 0x100, 0xff }
};

static const struct regval_list isl79986_test_pattern_regs[] = {
	/* test pattern */
	{ 0xff, 0x05 },
	{ 0x00, 0x00 },
	{ 0x0d, 0xf0 },
	/* regs list end flag reg_num = 0x100 */
	{ 0x100, 0xff }
};

static const struct regval_list isl79986_rm_test_pattern_regs[] = {
	/* test pattern */
	{ 0xff, 0x05 },
	{ 0x00, 0x00 },
	{ 0x0d, 0x00 },
	/* regs list end flag reg_num = 0x100 */
	{ 0x100, 0xff }
};

static const struct regval_list isl79986_default_regs_finalise[] = {
	{ 0x0f, 0x0f },
	{ 0x37, 0x47 },
	{ 0x39, 0x02 },
	{ 0x33, 0x85 },
	{ 0x2f, 0xe6 },
	{ 0xff, 0x05 },
	{ 0x0a, 0x62 },
	{ 0x11, 0xa0 },
	{ 0x34, 0xa0 },
	{ 0x00, 0x02 },
	{ 0xff, 0x00 },
	{ 0x03, 0x00 },
	{ 0x04, 0x08 },
	{ 0x08, 0x07 },
	{ 0x07, 0x10 },
	{ 0x03, 0x03 },
	{ 0x04, 0x0a },
	/* regs list end flag reg_num = 0x100 */
	{ 0x100, 0xff}
};

struct isl79986_datafmt {
	enum v4l2_mbus_pixelcode	code;
	enum v4l2_colorspace		colorspace;
};

struct isl79986 {
	struct v4l2_subdev		subdev;
	const struct isl79986_datafmt	*fmt;
	struct v4l2_rect                crop_rect;
	struct v4l2_clk			*clk;

	/* blanking information */
	int total_width;
	int total_height;
};

static const struct isl79986_datafmt isl79986_colour_fmts[] = {
	{V4L2_MBUS_FMT_UYVY8_2X8,},
};

static struct isl79986 *to_isl79986(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client), struct isl79986, subdev);
}

/* Find a data format by a pixel code in an array */
static const struct isl79986_datafmt
		*isl79986_find_datafmt(enum v4l2_mbus_pixelcode code)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(isl79986_colour_fmts); i++)
		if (isl79986_colour_fmts[i].code == code)
			return isl79986_colour_fmts + i;

	return NULL;
}

static int reg_read(struct i2c_client *client, u8 reg, u8 *val)
{
	int ret;
	u8 data = reg, odata;
	struct i2c_msg msg[2] = {
		{
			.addr	= client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= &data,
		},
		{
			.addr   = client->addr,
			.flags  = I2C_M_RD,
			.len    = 1,
			.buf    = &odata,
		}
	};
	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0)
		return ret;

	*val = odata;
	return ret;
}

static int reg_write(struct i2c_client *client, u8 reg, u8 val)
{
	unsigned char data[2] = {reg&0xff, val&0xff};
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 2,
		.buf	= data,
	};

	return i2c_transfer(client->adapter, &msg, 1);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int isl79986_get_register(struct v4l2_subdev *sd,
					struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	u8 val;

	if (reg->reg & ~0xffff)
		return -EINVAL;

	reg->size = 1;

	ret = reg_read(client, reg->reg, &val);
	if (!ret)
		reg->val = (__u64)val;

	return ret;
}

static int isl79986_set_register(struct v4l2_subdev *sd,
					const struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (reg->reg & ~0xffff || reg->val & ~0xff)
		return -EINVAL;

	return reg_write(client, reg->reg, reg->val);
}
#endif

static int isl79986_write_array(struct i2c_client *client,
				const struct regval_list *vals)
{
	int ret;

	while (vals->reg_num != 0x100) {
		ret = reg_write(client, vals->reg_num, vals->value);
		if (ret < 0)
			return ret;
		vals++;
	}
	return 0;
}

/* Select the nearest higher resolution for capture */
static const struct isl79986_win_size *isl79986_select_win(u32 *width, u32 *height)
{
	int i, default_size = ARRAY_SIZE(isl79986_supported_win_sizes) - 1;

	for (i = 0; i < ARRAY_SIZE(isl79986_supported_win_sizes); i++) {
		if (isl79986_supported_win_sizes[i].width  >= *width &&
			isl79986_supported_win_sizes[i].height >= *height) {
			*width = isl79986_supported_win_sizes[i].width;
			*height = isl79986_supported_win_sizes[i].height;
			return &isl79986_supported_win_sizes[i];
		}
	}
	*width = isl79986_supported_win_sizes[default_size].width;
	*height = isl79986_supported_win_sizes[default_size].height;
	return &isl79986_supported_win_sizes[default_size];
}

static int isl79986_try_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *mf)
{
	const struct isl79986_datafmt *fmt = isl79986_find_datafmt(mf->code);

	isl79986_select_win(&mf->width, &mf->height);
	if (!fmt) {
		mf->code	= isl79986_colour_fmts[0].code;
		mf->colorspace	= isl79986_colour_fmts[0].colorspace;
	}

	mf->field	= V4L2_FIELD_NONE;

	return 0;
}

static int isl79986_s_fmt(struct v4l2_subdev *sd,
			struct v4l2_mbus_framefmt *mf)
{
	int gpio_index;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct isl79986 *priv = to_isl79986(client);

	/* MIPI CSI could have changed the format, double-check */
	if (!isl79986_find_datafmt(mf->code))
		return -EINVAL;

	isl79986_try_fmt(sd, mf);
	priv->fmt = isl79986_find_datafmt(mf->code);

	/* camera reset and pwdn*/
	gpio_index = of_get_named_gpio(client->dev.of_node, "gpios", 0);
	gpio_direction_output(gpio_index, 1);
	gpio_direction_output(gpio_index, 0);

	if (mf->height == H_4CHANNEL)
		isl79986_write_array(client, isl79986_4_channel_reg_init);
	else if (mf->height == H_1CHANNEL)
		isl79986_write_array(client, isl79986_default_regs_init);

	if (is_testpattern)
		return isl79986_write_array(client, isl79986_test_pattern_regs);
	else
		return isl79986_write_array(client, isl79986_rm_test_pattern_regs);
}

static int isl79986_g_fmt(struct v4l2_subdev *sd,
			struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct isl79986 *priv = to_isl79986(client);

	const struct isl79986_datafmt *fmt = priv->fmt;

	mf->code	= fmt->code;
	mf->colorspace	= fmt->colorspace;
	mf->width	= priv->crop_rect.width;
	mf->height	= priv->crop_rect.height;
	mf->field	= V4L2_FIELD_NONE;

	return 0;
}

static int isl79986_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
			   enum v4l2_mbus_pixelcode *code)
{
	if (index >= ARRAY_SIZE(isl79986_colour_fmts))
		return -EINVAL;

	*code = isl79986_colour_fmts[index].code;
	return 0;
}

static int isl79986_s_crop(struct v4l2_subdev *sd, const struct v4l2_crop *a)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct isl79986 *priv = to_isl79986(client);
	struct v4l2_rect rect = a->c;
	int ret;

	v4l_bound_align_image(&rect.width, 48, INTERSIL_MAX_WIDTH, 1,
			      &rect.height, 32, INTERSIL_MAX_HEIGHT, 1, 0);

	priv->crop_rect.width	= rect.width;
	priv->crop_rect.height	= rect.height;
	priv->total_width	= rect.width + BLANKING_EXTRA_WIDTH;
	priv->total_height	= max_t(int, rect.height +
							BLANKING_EXTRA_HEIGHT,
							BLANKING_MIN_HEIGHT);
	priv->crop_rect.width		= rect.width;
	priv->crop_rect.height		= rect.height;

	ret = isl79986_write_array(client, isl79986_default_regs_init);
	if (!ret)
		ret = isl79986_write_array(client,
			isl79986_default_regs_finalise);

	return ret;
}

static int isl79986_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct isl79986 *priv = to_isl79986(client);
	struct v4l2_rect *rect = &a->c;

	if (a->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	*rect = priv->crop_rect;

	return 0;
}

static int isl79986_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
{
	a->bounds.left			= 0;
	a->bounds.top			= 0;
	a->bounds.width			= INTERSIL_MAX_WIDTH;
	a->bounds.height		= INTERSIL_MAX_HEIGHT;
	a->defrect			= a->bounds;
	a->type				= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a->pixelaspect.numerator	= 1;
	a->pixelaspect.denominator	= 1;

	return 0;
}

static int isl79986_camera_init(struct i2c_client *client)
{
	u8 sensor_id;
	int gpio_index, ret;

	/* camera reset and pwdn*/
	gpio_index = of_get_named_gpio(client->dev.of_node, "gpios", 0);
	gpio_direction_output(gpio_index, 1);
	gpio_direction_output(gpio_index, 0);

	/* Read camera sensor ID */
	ret = reg_write(client, 0xff, 0x00);
	if (ret < 1)
		return ret;
	ret = reg_read(client, 0x00, &sensor_id);
	if (ret < 2)
		return ret;
	if (sensor_id != SENSOR_ID)
		return -ENODEV;

	return isl79986_write_array(client, isl79986_default_regs_init);
}

static int isl79986_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	cfg->type = V4L2_MBUS_CSI2;
	cfg->flags = V4L2_MBUS_CSI2_1_LANE | V4L2_MBUS_CSI2_CHANNEL_0 |
					V4L2_MBUS_CSI2_CONTINUOUS_CLOCK;

	return 0;
}

static int isl79986_s_stream(struct v4l2_subdev *sd, int enable)
{
	return 0;
}

static int isl79986_s_power(struct v4l2_subdev *sd, int on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct soc_camera_subdev_desc *ssdd = soc_camera_i2c_to_desc(client);
	struct isl79986 *priv = to_isl79986(client);
	int ret;

	if (!on)
		return soc_camera_power_off(&client->dev, ssdd, priv->clk);
	ret = soc_camera_power_on(&client->dev, ssdd, priv->clk);
	if (ret < 0)
		return ret;
	return isl79986_camera_init(client);
}

static const struct v4l2_subdev_video_ops isl79986_subdev_video_ops = {
	.s_mbus_fmt	= isl79986_s_fmt,
	.g_mbus_fmt	= isl79986_g_fmt,
	.try_mbus_fmt	= isl79986_try_fmt,
	.enum_mbus_fmt	= isl79986_enum_fmt,
	.s_crop		= isl79986_s_crop,
	.g_crop		= isl79986_g_crop,
	.cropcap	= isl79986_cropcap,
	.g_mbus_config	= isl79986_g_mbus_config,
	.s_stream	= isl79986_s_stream,
};

static const struct v4l2_subdev_core_ops isl79986_subdev_core_ops = {
	.s_power	= isl79986_s_power,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register	= isl79986_get_register,
	.s_register	= isl79986_set_register,
#endif
};

static const struct v4l2_subdev_ops isl79986_subdev_ops = {
	.core	= &isl79986_subdev_core_ops,
	.video	= &isl79986_subdev_video_ops,
};

static int isl79986_probe(struct i2c_client *client,
			const struct i2c_device_id *did)
{
	struct isl79986 *priv;
	struct soc_camera_subdev_desc *ssdd = soc_camera_i2c_to_desc(client);
	struct v4l2_subdev *subdev;
	int ret;

	if (client->dev.of_node) {
		ssdd = devm_kzalloc(&client->dev, sizeof(*ssdd), GFP_KERNEL);
		if (!ssdd)
			return -ENOMEM;
		client->dev.platform_data = ssdd;
	}
	if (!ssdd) {
		dev_err(&client->dev, "INTERSIL: missing platform data!\n");
		return -EINVAL;
	}

	priv = devm_kzalloc(&client->dev, sizeof(struct isl79986), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	v4l2_i2c_subdev_init(&priv->subdev, client, &isl79986_subdev_ops);

	priv->fmt		= &isl79986_colour_fmts[0];

	priv->crop_rect.width	= W_TESTWIDTH;
	priv->crop_rect.height	= H_1CHANNEL;
	priv->crop_rect.left	= (INTERSIL_MAX_WIDTH - W_TESTWIDTH) / 2;
	priv->crop_rect.top	= (INTERSIL_MAX_HEIGHT - H_1CHANNEL) / 2;
	priv->total_width = W_TESTWIDTH + BLANKING_EXTRA_WIDTH;
	priv->total_height = BLANKING_MIN_HEIGHT;

	subdev = i2c_get_clientdata(client);
	ret = isl79986_s_power(subdev, 1);
	if (ret < 0)
		return ret;
	priv->subdev.dev = &client->dev;
	return v4l2_async_register_subdev(&priv->subdev);
}

static int isl79986_remove(struct i2c_client *client)
{
	struct soc_camera_subdev_desc *ssdd = soc_camera_i2c_to_desc(client);

	if (ssdd->free_bus)
		ssdd->free_bus(ssdd);

	return 0;
}

static const struct i2c_device_id isl79986_id[] = {
	{ "isl79986", 0},
	{ }
};
static const struct of_device_id isl79986_camera_of_match[] = {
	{ .compatible = "mediatek,isl79986", },
	{},
};

static struct i2c_driver isl79986_i2c_driver = {
	.driver = {
		.name = "isl79986",
		.of_match_table = isl79986_camera_of_match,
	},
	.probe		= isl79986_probe,
	.remove		= isl79986_remove,
	.id_table	= isl79986_id,
};

module_param(is_testpattern, int, 0644);
MODULE_PARM_DESC(is_testpattern, "Whether the intersil get test pattern data");
module_i2c_driver(isl79986_i2c_driver);

MODULE_DESCRIPTION("INTERSIL INTERSIL Camera driver");
MODULE_AUTHOR("Baoyin Zhang<baoyin.zhang@mediatek.com");
MODULE_LICENSE("GPL v2");
