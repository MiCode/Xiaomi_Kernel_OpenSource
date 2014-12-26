/*
 * Support for s5k8aay CMOS camera sensor.
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

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/types.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <linux/atomisp_platform.h>
#include <linux/libmsrlisthelper.h>

#define S5K8AAY_TOK_16BIT	2
#define S5K8AAY_TOK_TERM	0xf0	/* terminating token for reg list */
#define S5K8AAY_TOK_DELAY	0xfe	/* delay token for reg list */

#define S5K8AAY_FORMAT		V4L2_MBUS_FMT_UYVY8_1X16

struct s5k8aay_reg {
	u8 tok;
	u16 reg;
	u16 val;
};

struct s5k8aay_resolution {
	u8 *desc;
	unsigned int width;
	unsigned int height;
	unsigned int skip_frames;
	const struct s5k8aay_reg *mode_regs;
};

#include "s5k8aay_settings.h"

#define S5K8AAY_REG_CHIP_ID			0x00000040
#define S5K8AAY_REG_CHIP_ID_VAL			0x08AA
#define S5K8AAY_REG_ROM_REVISION		0x00000042 /* 0x00A0 / 0x00B0 */
#define S5K8AAY_REG_TC_IPRM_ERRORINFO		0x70000166
#define S5K8AAY_REG_TC_GP_ERRORPREVCONFIG	0x700001AE
#define S5K8AAY_REG_TC_GP_ERRORCAPCONFIG	0x700001B4
#define S5K8AAY_REG_TC_IPRM_INITHWERR		0x70000144
#define S5K8AAY_REG_TC_PZOOM_ERRORZOOM		0x700003A2
#define S5K8AAY_REG_TNP_SVNVERSION		0x700027C0
#define S5K8AAY_REG_TC_GP_ENABLEPREVIEW		0x7000019e
#define S5K8AAY_REG_TC_GP_ENABLEPREVIEWCHANGED	0x700001a0
#define S5K8AAY_REG_MON_AAIO_PREVACQCTX_T_LEI_EXP   0x700020dc

#define S5K8AAY_R16_AHB_MSB_ADDR_PTR		0xfcfc

#define S5K8AAY_FOCAL_LENGTH_NUM		167 /* 1.67mm */
#define S5K8AAY_FOCAL_LENGTH_DEM		100
#define S5K8AAY_F_NUMBER_NUM			26  /* 2.6 */
#define S5K8AAY_F_NUMBER_DEM			10

#define S5K8AAY_FOCAL_LENGTH	    ((S5K8AAY_FOCAL_LENGTH_NUM << 16) | \
		S5K8AAY_FOCAL_LENGTH_DEM)

#define S5K8AAY_F_NUMBER_ABSOLUTE   ((S5K8AAY_F_NUMBER_NUM << 16) | \
		S5K8AAY_F_NUMBER_DEM)

#define S5K8AAY_F_NUMBER_RANGE	    ((S5K8AAY_F_NUMBER_NUM << 24) | \
		(S5K8AAY_F_NUMBER_DEM << 16) | \
		(S5K8AAY_F_NUMBER_NUM << 8) | \
		S5K8AAY_F_NUMBER_DEM)

#define to_s5k8aay_sensor(_sd) container_of(_sd, struct s5k8aay_device, sd)

struct s5k8aay_device {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_mbus_framefmt format;
	struct camera_sensor_platform_data *platform_data;
	struct mutex input_lock;
	struct mutex power_lock;
	bool streaming;
	int fmt_idx;
	struct v4l2_ctrl_handler ctrl_handler;
	const struct firmware *fw;
};

/*
 * Biggest resolution should be last.
 *
 * NOTE: Currently sensor outputs only one size per aspect ratio. */
static const struct s5k8aay_resolution const s5k8aay_res_modes[] = {
	{
		.desc = "s5k8aay_1056x864",
		.width = 1056,
		.height = 864,
		.skip_frames = 3,
		.mode_regs = s5k8aay_regs_19_1056x864,
	},
	{
		.desc = "s5k8aay_1200x800",
		.width = 1200,
		.height = 800,
		.skip_frames = 3,
		.mode_regs = s5k8aay_regs_19_1200x800,
	},
	{
		.desc = "s5k8aay_1280x720",
		.width = 1280,
		.height = 720,
		.skip_frames = 3,
		.mode_regs = s5k8aay_regs_19_1280x720,
	},
	{
		.desc = "s5k8aay_1280x960",
		.width = 1280,
		.height = 960,
		.skip_frames = 3,
		.mode_regs = s5k8aay_regs_19_1280x960,
	}
};

static int
s5k8aay_simple_read16(struct i2c_client *client, int reg, u16 *val)
{
	unsigned char buffer[] = {
		reg >> 8, reg & 0xff
	};
	struct i2c_msg msg[] = { {
		.addr = client->addr,
		.len = ARRAY_SIZE(buffer),
		.flags = 0,
		.buf = buffer,
	}, {
		.addr = client->addr,
		.len = ARRAY_SIZE(buffer),
		.flags = I2C_M_RD,
		.buf = buffer,
	} };
	int err;

	err = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
	if (err < 0) {
		dev_err(&client->dev,
			"read from offset 0x%x error %d", reg, err);
		return err;
	}

	*val = buffer[1] + (buffer[0] << 8);
	return 0;
}

static int
s5k8aay_simple_write16(struct i2c_client *client, int reg, int val)
{
	unsigned char buffer[] = {
		reg >> 8, reg & 0xff,
		val >> 8, val & 0xff
	};
	struct i2c_msg msg = {
		.addr = client->addr,
		.len = ARRAY_SIZE(buffer),
		.flags = 0,
		.buf = buffer,
	};
	int err;

	err = i2c_transfer(client->adapter, &msg, 1);
	if (err < 0)
		dev_err(&client->dev,
			"write error: wrote 0x%x to offset 0x%x error %d",
			val, reg, err);
	return 0;
}

static int s5k8aay_write_array(struct i2c_client *client,
			       const struct s5k8aay_reg *reglist)
{
	const struct s5k8aay_reg *next = reglist;
	int ret;

	for (; next->tok != S5K8AAY_TOK_TERM; next++) {
		if (next->tok == S5K8AAY_TOK_DELAY) {
			usleep_range(next->val * 1000, next->val * 1000);
			continue;
		}
		ret = s5k8aay_simple_write16(client, next->reg, next->val);
		if (ret) {
			dev_err(&client->dev, "register write failed\n");
			return ret;
		}
	}

	return 0;
}

static int s5k8aay_write(struct i2c_client *client, u32 addr, u16 val)
{
	int ret;

	ret = s5k8aay_simple_write16(client, S5K8AAY_R16_AHB_MSB_ADDR_PTR,
				     addr >> 16);
	if (ret < 0)
		return ret;

	return s5k8aay_simple_write16(client, addr & 0xffff, val);
}

static int s5k8aay_read(struct i2c_client *client, u32 addr, u16 *val)
{
	int ret;

	ret = s5k8aay_simple_write16(client, S5K8AAY_R16_AHB_MSB_ADDR_PTR,
				     addr >> 16);
	if (ret < 0)
		return ret;

	return s5k8aay_simple_read16(client, addr & 0xffff, val);
}

static int s5k8aay_check_error(struct s5k8aay_device *dev,
			       struct i2c_client *client)
{
	static struct {
		char *error;
		int address;
	} error_codes[] = {
		{ "ErrorInfo",		S5K8AAY_REG_TC_IPRM_ERRORINFO },
		{ "ErrorPrevConfig",	S5K8AAY_REG_TC_GP_ERRORPREVCONFIG },
		{ "ErrorCapConfig",	S5K8AAY_REG_TC_GP_ERRORCAPCONFIG },
		{ "InitHwErr",		S5K8AAY_REG_TC_IPRM_INITHWERR },
		{ "ErrorZoom",		S5K8AAY_REG_TC_PZOOM_ERRORZOOM },
		{ "TnP_SvnVersion",	S5K8AAY_REG_TNP_SVNVERSION },
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(error_codes); i++) {
		u16 v;
		int ret = s5k8aay_read(client, error_codes[i].address, &v);
		if (ret)
			return ret;
		dev_info(&client->dev, "%s: %i\n", error_codes[i].error, v);
	}

	return 0;
}

static int s5k8aay_reset(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	/* Reset */
	ret = s5k8aay_write(client, 0xd0000010, 0x0001);
	if (ret < 0)
		return ret;

	/* Clear host interrupt */
	ret = s5k8aay_write(client, 0xd0001030, 0x0000);
	if (ret < 0)
		return ret;

	/* ARM go */
	ret = s5k8aay_write(client, 0xd0000014, 0x0001);
	if (ret < 0)
		return ret;

	/* Allow startup code to run */
	usleep_range(1000, 2000);

	return 0;
}

static int s5k8aay_set_suspend(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k8aay_device *dev = to_s5k8aay_sensor(sd);
	int ret;

	dev->streaming = false;
	ret = s5k8aay_write(client, S5K8AAY_REG_TC_GP_ENABLEPREVIEW, 0x0000);
	if (ret < 0)
		return ret;

	return s5k8aay_write(client, S5K8AAY_REG_TC_GP_ENABLEPREVIEWCHANGED,
			     0x0001);
}

static int s5k8aay_write_array_list(struct v4l2_subdev *sd,
		struct s5k8aay_reg const *regs[], unsigned int size)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int i;

	for (i = 0; i < size; i++) {
		int ret = s5k8aay_write_array(client, regs[i]);
		if (ret)
			return ret;
	}
	return 0;
}

static int s5k8aay_set_streaming(struct v4l2_subdev *sd)
{
	struct s5k8aay_device *dev = to_s5k8aay_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	static struct s5k8aay_reg const *pre_regs[] = {
		s5k8aay_regs_1,
	};

	const struct s5k8aay_reg const *mode_regs[] = {
		s5k8aay_res_modes[dev->fmt_idx].mode_regs,
	};

	static struct s5k8aay_reg const *post_regs[] = {
		s5k8aay_regs_21,
	};
	int ret;

	ret = s5k8aay_write_array_list(sd, pre_regs, ARRAY_SIZE(pre_regs));
	if (ret)
		return ret;

	if (dev->fw) {
		ret = apply_msr_data(client, dev->fw);
		if (ret)
			return ret;
	}

	ret = s5k8aay_write_array_list(sd, mode_regs, ARRAY_SIZE(mode_regs));
	if (ret)
		return ret;

	ret = s5k8aay_write_array_list(sd, post_regs, ARRAY_SIZE(post_regs));
	if (ret)
		return ret;

	ret = s5k8aay_check_error(dev, client);
	if (ret)
		return ret;

	dev->streaming = true;

	return 0;
}

static int power_up(struct v4l2_subdev *sd)
{
	struct s5k8aay_device *dev = to_s5k8aay_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = dev->platform_data->power_ctrl(sd, 1);
	if (ret)
		goto fail_power;

	ret = dev->platform_data->flisclk_ctrl(sd, 1);
	if (ret)
		goto fail_clk;

	/*usleep_range should not use min == max args;*/
	usleep_range(15, 30);

	/* Release reset */
	ret = dev->platform_data->gpio_ctrl(sd, 1);
	if (ret)
		goto fail_gpio;

	/* 100 us is needed between power up and first i2c transaction. */
	usleep_range(100, 200);

	return 0;

fail_gpio:
	dev->platform_data->flisclk_ctrl(sd, 0);
fail_clk:
	dev->platform_data->power_ctrl(sd, 0);
fail_power:
	dev_err(&client->dev, "sensor power-up failed\n");

	return ret;
}

static int power_down(struct v4l2_subdev *sd)
{
	struct s5k8aay_device *dev = to_s5k8aay_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	/* gpio ctrl */
	ret = dev->platform_data->gpio_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "gpio off failed\n");

	ret = dev->platform_data->flisclk_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "flisclk off failed\n");

	/* power control */
	ret = dev->platform_data->power_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "power off failed\n");

	return ret;
}

static int __s5k8aay_s_power(struct v4l2_subdev *sd, int power)
{
	if (power == 0) {
		return power_down(sd);
	} else {
		int ret = power_up(sd);
		if (ret)
			return ret;

		ret = s5k8aay_reset(sd);
		if (ret)
			return ret;
	}

	return 0;
}

static int s5k8aay_s_power(struct v4l2_subdev *sd, int power)
{
	struct s5k8aay_device *dev = to_s5k8aay_sensor(sd);
	int ret;

	mutex_lock(&dev->power_lock);
	ret = __s5k8aay_s_power(sd, power);
	if (ret)
		goto out;

	ret = v4l2_ctrl_handler_setup(&dev->ctrl_handler);
	if (ret)
		goto out;

out:
	mutex_unlock(&dev->power_lock);

	return ret;
}

#define ASPECT_RATIO(w, h)  (((w) << 13) / (h))

static bool check_aspect_ratio(struct s5k8aay_resolution const *res,
		unsigned int width, unsigned int height)
{
	return ASPECT_RATIO(res->width, res->height) == ASPECT_RATIO(width,
			height);
}

/*
 * Returns the nearest higher resolution index.
 * @w: width
 * @h: height
 * matching is done based on aspect ratio.
 * If the aspect ratio cannot be matched to any index, -1 is returned.
 */
static int nearest_resolution_index(struct v4l2_subdev *sd, int w, int h)
{
	int i;
	int idx = -1;

	for (i = 0; i < ARRAY_SIZE(s5k8aay_res_modes); i++) {
		if (check_aspect_ratio(&s5k8aay_res_modes[i], w, h)) {
			idx = i;
			break;
		}
	}
	return idx;
}

static int s5k8aay_try_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	int idx;
	const struct s5k8aay_resolution *biggest =
		&s5k8aay_res_modes[ARRAY_SIZE(s5k8aay_res_modes) - 1];

	if ((fmt->width > biggest->width) ||
	    (fmt->height > biggest->height)) {
		fmt->width = biggest->width;
		fmt->height = biggest->height;
	} else {
		/* Find nearest resolution with same aspect ratio. */
		idx = nearest_resolution_index(sd, fmt->width, fmt->height);

		/* Same aspect ratio was not found from the list.
		 * Take last defined resolution. */
		if (idx == -1) {
			idx = ARRAY_SIZE(s5k8aay_res_modes) - 1;
			WARN_ONCE(1, "Correct aspect ratio was not found.");
		}

		fmt->width = s5k8aay_res_modes[idx].width;
		fmt->height = s5k8aay_res_modes[idx].height;
	}

	fmt->code = S5K8AAY_FORMAT;
	return 0;
}

static int s5k8aay_get_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	struct s5k8aay_device *dev = to_s5k8aay_sensor(sd);

	mutex_lock(&dev->input_lock);
	fmt->width = s5k8aay_res_modes[dev->fmt_idx].width;
	fmt->height = s5k8aay_res_modes[dev->fmt_idx].height;
	fmt->code = S5K8AAY_FORMAT;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static int s5k8aay_set_mbus_fmt(struct v4l2_subdev *sd,
			      struct v4l2_mbus_framefmt *fmt)
{
	struct s5k8aay_device *dev = to_s5k8aay_sensor(sd);
	int ret;
	int tmp_idx;

	mutex_lock(&dev->input_lock);

	if (dev->streaming) {
		ret = -EBUSY;
		goto out;
	}

	ret = s5k8aay_try_mbus_fmt(sd, fmt);
	if (ret)
		goto out;

	tmp_idx = nearest_resolution_index(sd, fmt->width, fmt->height);
	/* Sanity check */
	if (unlikely(tmp_idx == -1)) {
		ret = -EINVAL;
		goto out;
	}
	dev->fmt_idx = tmp_idx;

out:
	mutex_unlock(&dev->input_lock);
	return ret;
}

static int s5k8aay_detect(struct s5k8aay_device *dev, struct i2c_client *client)
{
	struct i2c_adapter *adapter = client->adapter;
	u32 ret;
	u16 id = -1, revision = -1;

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "%s: i2c error", __func__);
		return -ENODEV;
	}

	ret = s5k8aay_read(client, S5K8AAY_REG_CHIP_ID, &id);
	if (ret)
		return ret;

	ret = s5k8aay_read(client, S5K8AAY_REG_ROM_REVISION, &revision);
	if (ret)
		return ret;

	dev_info(&client->dev, "chip id 0x%4.4x, ROM revision 0x%4.4x\n",
		 id, revision);

	if (id != S5K8AAY_REG_CHIP_ID_VAL) {
		dev_err(&client->dev, "failed to detect sensor\n");
		return -ENODEV;
	}
	return 0;
}

static int
s5k8aay_s_config(struct v4l2_subdev *sd, int irq, void *platform_data)
{
	struct s5k8aay_device *dev = to_s5k8aay_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	dev->platform_data = platform_data;

	if (dev->platform_data->platform_init) {
		ret = dev->platform_data->platform_init(client);
		if (ret) {
			dev_err(&client->dev, "s5k8aay platform init err\n");
			return ret;
		}
	}

	ret = __s5k8aay_s_power(sd, 1);
	if (ret) {
		dev_err(&client->dev, "s5k8aay power-up err %i\n", ret);
		return ret;
	}

	/* config & detect sensor */
	ret = s5k8aay_detect(dev, client);
	if (ret) {
		dev_err(&client->dev, "s5k8aay_detect err s_config.\n");
		goto fail_detect;
	}

	ret = dev->platform_data->csi_cfg(sd, 1);
	if (ret)
		goto fail_csi_cfg;

	ret = s5k8aay_set_suspend(sd);
	if (ret) {
		dev_err(&client->dev, "s5k8aay suspend err");
		return ret;
	}

	ret = __s5k8aay_s_power(sd, 0);
	if (ret) {
		dev_err(&client->dev, "s5k8aay power down err");
		return ret;
	}

	return 0;

fail_csi_cfg:
	dev->platform_data->csi_cfg(sd, 0);
fail_detect:
	__s5k8aay_s_power(sd, 0);
	dev_err(&client->dev, "sensor detection failed\n");
	return ret;
}

static int s5k8aay_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct s5k8aay_device *dev = to_s5k8aay_sensor(sd);
	int ret;

	mutex_lock(&dev->input_lock);
	if (enable)
		ret = s5k8aay_set_streaming(sd);
	else
		ret = s5k8aay_set_suspend(sd);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int
s5k8aay_enum_framesizes(struct v4l2_subdev *sd, struct v4l2_frmsizeenum *fsize)
{
	struct s5k8aay_device *dev = to_s5k8aay_sensor(sd);

	if (fsize->index >= ARRAY_SIZE(s5k8aay_res_modes))
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = s5k8aay_res_modes[fsize->index].width;
	fsize->discrete.height = s5k8aay_res_modes[fsize->index].height;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static int s5k8aay_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_fh *fh,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index)
		return -EINVAL;

	code->code = V4L2_MBUS_FMT_UYVY8_1X16;

	return 0;
}

static int s5k8aay_enum_frame_size(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh,
	struct v4l2_subdev_frame_size_enum *fse)
{
	struct s5k8aay_device *dev = to_s5k8aay_sensor(sd);

	if (fse->index >= ARRAY_SIZE(s5k8aay_res_modes))
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	fse->min_width = s5k8aay_res_modes[fse->index].width;
	fse->min_height = s5k8aay_res_modes[fse->index].height;
	fse->max_width = s5k8aay_res_modes[fse->index].width;
	fse->max_height = s5k8aay_res_modes[fse->index].height;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static struct v4l2_mbus_framefmt *
__s5k8aay_get_pad_format(struct s5k8aay_device *sensor,
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
s5k8aay_get_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct s5k8aay_device *snr = to_s5k8aay_sensor(sd);
	struct v4l2_mbus_framefmt *format =
			__s5k8aay_get_pad_format(snr, fh, fmt->pad, fmt->which);

	if (format == NULL)
		return -EINVAL;

	mutex_lock(&snr->input_lock);
	fmt->format = *format;
	mutex_unlock(&snr->input_lock);

	return 0;
}

static int
s5k8aay_set_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct s5k8aay_device *snr = to_s5k8aay_sensor(sd);
	struct v4l2_mbus_framefmt *format =
			__s5k8aay_get_pad_format(snr, fh, fmt->pad, fmt->which);

	if (format == NULL)
		return -EINVAL;

	mutex_lock(&snr->input_lock);
	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		snr->format = fmt->format;
	mutex_unlock(&snr->input_lock);

	return 0;
}

static int s5k8aay_g_skip_frames(struct v4l2_subdev *sd, u32 *frames)
{
	struct s5k8aay_device *snr = to_s5k8aay_sensor(sd);

	mutex_lock(&snr->input_lock);
	*frames = s5k8aay_res_modes[snr->fmt_idx].skip_frames;
	mutex_unlock(&snr->input_lock);

	return 0;
}


static const struct v4l2_subdev_sensor_ops s5k8aay_sensor_ops = {
	.g_skip_frames = s5k8aay_g_skip_frames,
};

static const struct v4l2_subdev_video_ops s5k8aay_video_ops = {
	.try_mbus_fmt = s5k8aay_try_mbus_fmt,
	.s_mbus_fmt = s5k8aay_set_mbus_fmt,
	.g_mbus_fmt = s5k8aay_get_mbus_fmt,
	.s_stream = s5k8aay_s_stream,
	.enum_framesizes = s5k8aay_enum_framesizes,
};

static const struct v4l2_subdev_core_ops s5k8aay_core_ops = {
	.g_ctrl = v4l2_subdev_g_ctrl,
	.s_power = s5k8aay_s_power,
};

static const struct v4l2_subdev_pad_ops s5k8aay_pad_ops = {
	.enum_mbus_code = s5k8aay_enum_mbus_code,
	.enum_frame_size = s5k8aay_enum_frame_size,
	.get_fmt = s5k8aay_get_pad_format,
	.set_fmt = s5k8aay_set_pad_format,
};

static const struct v4l2_subdev_ops s5k8aay_ops = {
	.core = &s5k8aay_core_ops,
	.video = &s5k8aay_video_ops,
	.pad = &s5k8aay_pad_ops,
	.sensor = &s5k8aay_sensor_ops,
};

static int s5k8aay_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct s5k8aay_device *dev = container_of(
		ctrl->handler, struct s5k8aay_device, ctrl_handler);
	struct i2c_client *client = v4l2_get_subdevdata(&dev->sd);
	int ret;

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE_ABSOLUTE:
		{
		u16 val;
		ret = s5k8aay_read(client,
			S5K8AAY_REG_MON_AAIO_PREVACQCTX_T_LEI_EXP, &val);
		if (ret)
			return ret;

		/* Exposure time of the previous frame (400 = 1ms)
		 * x * (1 / 400)ms */
		/* Returned value is in units of 100us */
		ctrl->val = val / 40;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct v4l2_ctrl_ops s5k8aay_ctrl_ops = {
	.g_volatile_ctrl = &s5k8aay_g_ctrl,
};

static const struct v4l2_ctrl_config ctrls[] = {
	{
		.id = V4L2_CID_FOCAL_ABSOLUTE,
		.name = "Focal length",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = S5K8AAY_FOCAL_LENGTH,
		.step = 1,
		.max = S5K8AAY_FOCAL_LENGTH,
		.def = S5K8AAY_FOCAL_LENGTH,
		.flags = V4L2_CTRL_FLAG_READ_ONLY,
	}, {
		.id = V4L2_CID_FNUMBER_ABSOLUTE,
		.name = "F-number",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = S5K8AAY_F_NUMBER_ABSOLUTE,
		.step = 1,
		.max = S5K8AAY_F_NUMBER_ABSOLUTE,
		.def = S5K8AAY_F_NUMBER_ABSOLUTE,
		.flags = V4L2_CTRL_FLAG_READ_ONLY,
	}, {
		.id = V4L2_CID_FNUMBER_RANGE,
		.name = "F-number range",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = S5K8AAY_F_NUMBER_RANGE,
		.step = 1,
		.max = S5K8AAY_F_NUMBER_RANGE,
		.def = S5K8AAY_F_NUMBER_RANGE,
		.flags = V4L2_CTRL_FLAG_READ_ONLY,
	}, {
		.id = V4L2_CID_EXPOSURE_ABSOLUTE,
		.name = "Absolute exposure",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.min = 0,
		.step = 1,
		.max = 0xffffff,
		.def = 0,
		.flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_READ_ONLY,
		.ops = &s5k8aay_ctrl_ops,
	},
};

static int s5k8aay_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct s5k8aay_device *dev = to_s5k8aay_sensor(sd);

	dev->platform_data->csi_cfg(sd, 0);
	if (dev->platform_data->platform_deinit)
		dev->platform_data->platform_deinit();
	media_entity_cleanup(&dev->sd.entity);
	v4l2_ctrl_handler_free(&dev->ctrl_handler);
	v4l2_device_unregister_subdev(sd);
	mutex_destroy(&dev->input_lock);
	if (dev->fw)
		release_msr_list(client, dev->fw);
	kfree(dev);
	return 0;
}

static int s5k8aay_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct s5k8aay_device *dev;
	char *msr_file_name = NULL;
	unsigned int i;
	int ret;

	if (!client->dev.platform_data) {
		dev_err(&client->dev, "no platform data\n");
		return -ENODEV;
	}

	/* Setup sensor configuration structure */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&client->dev, "out of memory\n");
		return -ENOMEM;
	}

	mutex_init(&dev->input_lock);
	mutex_init(&dev->power_lock);

	v4l2_i2c_subdev_init(&dev->sd, client, &s5k8aay_ops);

	ret = s5k8aay_s_config(&dev->sd, client->irq,
			       client->dev.platform_data);
	if (ret) {
		kfree(dev);
		return ret;
	}

	dev->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->pad.flags = MEDIA_PAD_FL_SOURCE;
	dev->sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
	dev->format.code = V4L2_MBUS_FMT_UYVY8_1X16;
	/* Set default resolution to biggest resolution. */
	dev->fmt_idx = ARRAY_SIZE(s5k8aay_res_modes) - 1;

	ret = media_entity_init(&dev->sd.entity, 1, &dev->pad, 0);
	if (ret) {
		kfree(dev);
		return ret;
	}

	ret = v4l2_ctrl_handler_init(&dev->ctrl_handler, ARRAY_SIZE(ctrls));
	if (ret) {
		s5k8aay_remove(client);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(ctrls); i++)
		v4l2_ctrl_new_custom(&dev->ctrl_handler, &ctrls[i], NULL);

	if (dev->ctrl_handler.error) {
		s5k8aay_remove(client);
		return dev->ctrl_handler.error;
	}

	/* Use same lock for controls as for everything else. */
	dev->ctrl_handler.lock = &dev->input_lock;
	dev->sd.ctrl_handler = &dev->ctrl_handler;

	if (dev->platform_data->msr_file_name)
		msr_file_name = dev->platform_data->msr_file_name();
	if (msr_file_name) {
		ret = load_msr_list(client, msr_file_name, &dev->fw);
		if (ret) {
			s5k8aay_remove(client);
			return ret;
		}
	} else
		dev_warn(&client->dev, "%s: MSR data not available", __func__);

	return ret;
}

static const struct i2c_device_id s5k8aay_id[] = {
	{ "s5k8aay", 0 },
	{}
};

MODULE_DEVICE_TABLE(i2c, s5k8aay_id);

static struct i2c_driver s5k8aay_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "s5k8aay"
	},
	.probe = s5k8aay_probe,
	.remove = s5k8aay_remove,
	.id_table = s5k8aay_id,
};

static int init_s5k8aay(void)
{
	return i2c_add_driver(&s5k8aay_driver);
}

static void exit_s5k8aay(void)
{
	i2c_del_driver(&s5k8aay_driver);
}

module_init(init_s5k8aay);
module_exit(exit_s5k8aay);

MODULE_AUTHOR("Tuukka Toivonen <tuukka.toivonen@intel.com>");
MODULE_LICENSE("GPL");
