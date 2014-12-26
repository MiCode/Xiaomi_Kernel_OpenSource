/*
 * Support for mipi CSI data generator.
 *
 * Copyright (c) 2014 Intel Corporation. All Rights Reserved.
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

#include <asm/div64.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "pixter.h"

#define to_pixter_dev(sd) container_of(sd, struct pixter_device, sd)
#define dev_off(m) offsetof(struct pixter_device, m)

static struct regmap_config pixter_reg_config = {
	.reg_bits = 8,
	.val_bits = 32,
	.val_format_endian = REGMAP_ENDIAN_NATIVE,
};

static struct pixter_format_bridge format_bridge[] = {
	{"", 0, ATOMISP_INPUT_FORMAT_BINARY_8, 8},
	{"RGGB10", V4L2_MBUS_FMT_SRGGB10_1X10, ATOMISP_INPUT_FORMAT_RAW_10, 10},
	{"GRBG10", V4L2_MBUS_FMT_SGRBG10_1X10, ATOMISP_INPUT_FORMAT_RAW_10, 10},
	{"GBRG10", V4L2_MBUS_FMT_SGBRG10_1X10, ATOMISP_INPUT_FORMAT_RAW_10, 10},
	{"BGGR10", V4L2_MBUS_FMT_SBGGR10_1X10, ATOMISP_INPUT_FORMAT_RAW_10, 10},
	{"RGGB8", V4L2_MBUS_FMT_SRGGB8_1X8, ATOMISP_INPUT_FORMAT_RAW_8, 8},
	{"GRBG8", V4L2_MBUS_FMT_SGRBG8_1X8, ATOMISP_INPUT_FORMAT_RAW_8, 8},
	{"GBRG8", V4L2_MBUS_FMT_SGBRG8_1X8, ATOMISP_INPUT_FORMAT_RAW_8, 8},
	{"BGGR8", V4L2_MBUS_FMT_SBGGR8_1X8, ATOMISP_INPUT_FORMAT_RAW_8, 8},
	{"YUV422_8", V4L2_MBUS_FMT_UYVY8_1X16,
		ATOMISP_INPUT_FORMAT_YUV422_8, 16},
	{"YUV420_8", 0x8001/*For YUV420*/, ATOMISP_INPUT_FORMAT_YUV420_8, 16},
};

static u32 port_to_channel[4] = {1, 0, 2, 0};

static struct pixter_dbgfs dbgfs[] = {
	{"root", NULL, DBGFS_DIR, 0, 0},
	{"fps", "root", DBGFS_DIR, 0, 0},
	{"blank", "root", DBGFS_DIR, 0, 0},
	{"timing", "root", DBGFS_DIR, 0, 0},
	{"fps_ovrd", "fps", DBGFS_FILE, PIXTER_RW, dev_off(dbg_fps.fps_ovrd)},
	{"fps", "fps", DBGFS_FILE, PIXTER_RW, dev_off(dbg_fps.fps)},
	{"blank_ovrd", "blank",
		DBGFS_FILE, PIXTER_RW, dev_off(dbg_blank.blank_ovrd)},
	{"h_blank", "blank", DBGFS_FILE, PIXTER_RW, dev_off(dbg_blank.h_blank)},
	{"v_blank_pre", "blank",
		DBGFS_FILE, PIXTER_RW, dev_off(dbg_blank.v_blank_pre)},
	{"v_blank_post", "blank",
		DBGFS_FILE, PIXTER_RW, dev_off(dbg_blank.v_blank_post)},
	{"mipi_clk", "timing",
		DBGFS_FILE, PIXTER_RW, dev_off(dbg_timing.mipi_clk)},
	{"cont_hs_clk", "timing",
		DBGFS_FILE, PIXTER_RW, dev_off(dbg_timing.cont_hs_clk)},
	{"timing_ovrd", "timing",
		DBGFS_FILE, PIXTER_RW, dev_off(dbg_timing.timing_ovrd)},
	{"pre", "timing", DBGFS_FILE, PIXTER_RW, dev_off(dbg_timing.pre)},
	{"post", "timing", DBGFS_FILE, PIXTER_RW, dev_off(dbg_timing.post)},
	{"gap", "timing", DBGFS_FILE, PIXTER_RW, dev_off(dbg_timing.gap)},
	{"ck_lpx", "timing", DBGFS_FILE, PIXTER_RW, dev_off(dbg_timing.ck_lpx)},
	{"ck_prep", "timing",
		DBGFS_FILE, PIXTER_RW, dev_off(dbg_timing.ck_prep)},
	{"ck_zero", "timing",
		DBGFS_FILE, PIXTER_RW, dev_off(dbg_timing.ck_zero)},
	{"ck_trail", "timing",
		DBGFS_FILE, PIXTER_RW, dev_off(dbg_timing.ck_trail)},
	{"dat_lpx", "timing",
		DBGFS_FILE, PIXTER_RW, dev_off(dbg_timing.dat_lpx)},
	{"dat_prep", "timing",
		DBGFS_FILE, PIXTER_RW, dev_off(dbg_timing.dat_prep)},
	{"dat_zero", "timing",
		DBGFS_FILE, PIXTER_RW, dev_off(dbg_timing.dat_zero)},
	{"dat_trail", "timing",
		DBGFS_FILE, PIXTER_RW, dev_off(dbg_timing.dat_trail)},
	{"twakeup", "timing",
		DBGFS_FILE, PIXTER_RW, dev_off(dbg_timing.twakeup)},
	{"mipi_lanes_num", "timing",
		DBGFS_FILE, PIXTER_RONLY, dev_off(dbg_timing.mipi_lanes_num)},
};

static u32 pixter_get_tx_freq_sel(u32 *freq)
{
	u32 sel;

	*freq /= 1000000; /* To MHz */
	if (*freq < 20) {
		sel = 1;
		*freq = 20;
	} else if (*freq <= 100) {
		sel = (*freq + 9) / 10 - 1;
		*freq = 20 + (sel - 1) * 10;
	} else if (*freq <= 750) {
		sel = (*freq + 24) / 25 + 5;
		*freq = 100 + (sel - 9) * 25;
	} else {
		sel = 35;
		*freq = 750;
	}
	*freq *= 1000000;

	return sel;
}

static int pixter_read_reg(struct v4l2_subdev *sd, u32 reg, u32 *val)
{
	struct pixter_device *dev = to_pixter_dev(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	if (!dev->regmap)
		return -EIO;

	ret = regmap_write(dev->regmap, PIXTER_I2C_ADDR, reg | 1);
	ret |= regmap_read(dev->regmap, PIXTER_I2C_DATA_R, val);
	if (ret) {
		dev_dbg(&client->dev, "Read reg failed. reg=0x%04X\n", reg);
		return ret;
	}
	dev_dbg(&client->dev, "read_reg[0x%04X] = 0x%08X\n",
		reg, *val);
	return ret;
}

static int pixter_write_reg(struct v4l2_subdev *sd, u32 reg, u32 val)
{
	struct pixter_device *dev = to_pixter_dev(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	if (!dev->regmap)
		return -EIO;

	ret = regmap_write(dev->regmap, PIXTER_I2C_DATA_W, val);
	ret |= regmap_write(dev->regmap, PIXTER_I2C_ADDR, reg);
	if (ret) {
		dev_dbg(&client->dev, "Write reg failed. reg=0x%04X\n", reg);
		return ret;
	}
	dev_dbg(&client->dev, "write_reg[0x%04X] = 0x%08X\n",
		reg, (u32)val);
	return ret;
}

static int pixter_read_buf(struct v4l2_subdev *sd,
			u32 addr, u32 size, void *buf)
{
	u32 i;
	int ret = 0;

	for (i = 0; i < size; i += 4) {
		ret = pixter_read_reg(sd, addr + i, (u32 *)((u8 *)buf + i));
		if (ret)
			break;
	}

	return ret;
}

static int pixter_read_mipi_timing(struct v4l2_subdev *sd)
{
	struct pixter_device *dev = to_pixter_dev(sd);
	u32 reg_val, ch;
	ch = port_to_channel[dev->mipi_info->port];
	pixter_read_reg(sd, PIXTER_TX_CTRL_TIMING(ch), &reg_val);
	dev->dbg_timing.pre = reg_val & 0x7F;
	dev->dbg_timing.post = (reg_val >> 8) & 0x7F;
	dev->dbg_timing.gap = (reg_val >> 16) & 0x7F;
	pixter_read_reg(sd, PIXTER_TX_CK_TIMING(ch), &reg_val);
	dev->dbg_timing.ck_lpx = reg_val & 0x7F;
	dev->dbg_timing.ck_prep = (reg_val >> 8) & 0x7F;
	dev->dbg_timing.ck_zero = (reg_val >> 16) & 0x7F;
	dev->dbg_timing.ck_trail = (reg_val >> 24) & 0x7F;
	pixter_read_reg(sd, PIXTER_TX_DAT_TIMING(ch), &reg_val);
	dev->dbg_timing.dat_lpx = reg_val & 0x7F;
	dev->dbg_timing.dat_prep = (reg_val >> 8) & 0x7F;
	dev->dbg_timing.dat_zero = (reg_val >> 16) & 0x7F;
	dev->dbg_timing.dat_trail = (reg_val >> 24) & 0x7F;
	pixter_read_reg(sd, PIXTER_TX_ULPS_TIMING(ch), &reg_val);
	dev->dbg_timing.twakeup = reg_val & 0x7FFFF;
	return 0;
}

static int pixter_config_rx(struct v4l2_subdev *sd)
{
	struct pixter_device *dev = to_pixter_dev(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct pixter_setting *setting;
	u32 h_blank, v_blank_pre, v_blank_post;
	u32 i, reg_val, ch, vc = 0;
	u32 line_interval;
	u32 width_bits;
	u32 bit_rate;
	u32 line_bits;

	setting = &dev->settings[dev->cur_setting];
	ch = port_to_channel[dev->mipi_info->port];

	/* Set setting start and end address in DDR SDRAM. */
	pixter_write_reg(sd, PIXTER_RDR_START(ch), setting->start);
	pixter_write_reg(sd, PIXTER_RDR_END(ch), setting->end);

	/* Set FPS. */
	if (dev->dbg_fps.fps_ovrd)
		reg_val = dev->dbg_fps.fps << 24;
	else
		reg_val = setting->vc[setting->def_vc].fps << 24;

	if (setting->block_mode)
		reg_val |= PIXTER_DFT_BLOCK_MODE;

	if (dev->caps->sensor[0].stream_num > 1) {
		/* Select the channel with the largest width. */
		for (i = 1; i < 4; i++) {
			if (setting->vc[i].width > setting->vc[vc].width)
				vc = i;
		}
	}
	width_bits = setting->vc[vc].width *
		format_bridge[setting->vc[vc].format].bpp;
	bit_rate = dev->dbg_timing.mipi_clk / 1000000 * 2 *
		dev->mipi_info->num_lanes;
	if (bit_rate == 0 || bit_rate > PIXTER_MAX_BITRATE_MBPS) {
		dev_err(&client->dev, "Invalid bit rate %dMbps.\n", bit_rate);
		return -EINVAL;
	}

	if (dev->dbg_blank.blank_ovrd) {
		h_blank = dev->dbg_blank.h_blank;
		line_bits = 1000 * (width_bits + h_blank *
			format_bridge[setting->vc[vc].format].bpp);
		line_interval = line_bits / bit_rate;
		v_blank_pre = dev->dbg_blank.v_blank_pre;
		v_blank_post = dev->dbg_blank.v_blank_post;
	} else {
		line_bits = 1200 * width_bits;
		line_interval = line_bits / bit_rate;
		v_blank_pre = PIXTER_DEF_VBPRE;
		v_blank_post = line_interval;
	}

	/* Set line interval */
	reg_val |= (line_interval / 8) << 8;
	pixter_write_reg(sd, PIXTER_DFT_CTRL(ch), reg_val);

	/* Set vertical blanking */
	reg_val = ((v_blank_post / 8) << 16) + v_blank_pre / 8;
	pixter_write_reg(sd, PIXTER_VERT_BLANK(ch), reg_val);

	return 0;
}

static int pixter_config_tx(struct v4l2_subdev *sd)
{
	struct pixter_device *dev = to_pixter_dev(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u32 reg_val, ch;
	u32 cnt = 500;

	/* Set the parameters in the tx_csi2_ctrl. */
	ch = port_to_channel[dev->mipi_info->port];
	reg_val = dev->mipi_info->num_lanes - 1;
	reg_val |= (dev->dbg_timing.cont_hs_clk << 4);
	if (!dev->dbg_timing.timing_ovrd)
		reg_val |= 1 << 8;
	pixter_write_reg(sd, PIXTER_TX_CSI2_CTRL(ch), reg_val);

	/* Set MIPI timing if overided. */
	if (dev->dbg_timing.timing_ovrd) {
		reg_val = dev->dbg_timing.pre;
		reg_val |= dev->dbg_timing.post << 8;
		reg_val |= dev->dbg_timing.gap << 16;
		pixter_write_reg(sd, PIXTER_TX_CTRL_TIMING(ch), reg_val);
		reg_val = dev->dbg_timing.ck_lpx;
		reg_val |= dev->dbg_timing.ck_prep << 8;
		reg_val |= dev->dbg_timing.ck_zero << 16;
		reg_val |= dev->dbg_timing.ck_trail << 24;
		pixter_write_reg(sd, PIXTER_TX_CK_TIMING(ch), reg_val);
		reg_val = dev->dbg_timing.dat_lpx;
		reg_val |= dev->dbg_timing.dat_prep << 8;
		reg_val |= dev->dbg_timing.dat_zero << 16;
		reg_val |= dev->dbg_timing.dat_trail << 24;
		pixter_write_reg(sd, PIXTER_TX_DAT_TIMING(ch), reg_val);
		reg_val = dev->dbg_timing.twakeup;
		pixter_write_reg(sd, PIXTER_TX_ULPS_TIMING(ch), reg_val);
	}

	/* Config MIPI clock. */
	reg_val = pixter_get_tx_freq_sel(&dev->dbg_timing.mipi_clk);
	pixter_write_reg(sd, PIXTER_TX_CTRL(ch), reg_val);
	/* Wait MIPI clock to be ready. Timeout=5s. */
	while (cnt) {
		pixter_write_reg(sd, PIXTER_TX_CTRL_NNS(ch), 1);
		pixter_read_reg(sd, PIXTER_TX_STATUS(ch), &reg_val);
		if (reg_val & PIXTER_TX_READY)
			break;
		/*usleep_range should not use min == max args*/
		usleep_range(10000, 10000 + 1);
		cnt--;
	}
	if (cnt == 0) {
		dev_err(&client->dev, "Wait MIPI clock ready timeout.\n");
		return -EBUSY;
	}
	pixter_read_mipi_timing(sd);

	return 0;
}

static void __print_mipi_timing(struct v4l2_subdev *sd)
{
	struct pixter_device *dev = to_pixter_dev(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct pixter_timing *dbg_time = &dev->dbg_timing;
	unsigned short mipi_clk = dbg_time->mipi_clk / 1000000;
	/* 1UI = 1 bit periold, in pS */
	unsigned int ui = 1000000 / (mipi_clk * 2);
	unsigned int tmp;

	dev_dbg(&client->dev, "MIPI CLK: %d MHz.\n", mipi_clk);

	dev_dbg(&client->dev, "----Pixter MIPI Parameters----\n");
	dev_dbg(&client->dev, "ck_lpx: %d.\n", dbg_time->ck_lpx);
	dev_dbg(&client->dev, "ck_prep: %d.\n", dbg_time->ck_prep);
	dev_dbg(&client->dev, "ck_zero: %d.\n", dbg_time->ck_zero);
	dev_dbg(&client->dev, "pre: %d.\n", dbg_time->pre);
	dev_dbg(&client->dev, "post: %d.\n", dbg_time->post);
	dev_dbg(&client->dev, "ck_trail: %d.\n", dbg_time->ck_trail);
	dev_dbg(&client->dev, "dat_lpx: %d.\n", dbg_time->dat_lpx);
	dev_dbg(&client->dev, "dat_prep: %d.\n", dbg_time->dat_prep);
	dev_dbg(&client->dev, "dat_zero: %d.\n", dbg_time->dat_zero);
	dev_dbg(&client->dev, "dat_trail: %d.\n", dbg_time->dat_trail);
	dev_dbg(&client->dev, "gap: %d.\n", dbg_time->gap);
	dev_dbg(&client->dev, "twakeup: %d.\n", dbg_time->twakeup);

	dev_dbg(&client->dev, "----Standard MIPI Parameters----\n");

	tmp = (dbg_time->ck_lpx + 1) * 8 * ui;
	dev_dbg(&client->dev, "CLK-LPX: %d.%d nS.\n",
			tmp / 1000, tmp % 1000);

	tmp = (dbg_time->ck_prep + 1) * 8 * ui;
	dev_dbg(&client->dev, "CLK-PREPARE: %d.%d nS.\n",
			tmp / 1000, tmp % 1000);

	tmp = (dbg_time->ck_zero + 1) * 8 * ui;
	dev_dbg(&client->dev, "CLK-ZERO: %d.%d nS.\n",
			tmp / 1000, tmp % 1000);

	tmp = (dbg_time->pre - dbg_time->ck_lpx - dbg_time->ck_zero - 3)
		* 8 * ui;
	dev_dbg(&client->dev, "CLK-PRE: %d.%d nS.\n",
			tmp / 1000, tmp % 1000);

	tmp = (dbg_time->post + 8) * 8 * ui;
	dev_dbg(&client->dev, "CLK-POST: %d.%d nS.\n",
			tmp / 1000, tmp % 1000);

	tmp = (dbg_time->ck_trail + 1) * 8 * ui;
	dev_dbg(&client->dev, "CLK-TRAIL: %d.%d nS.\n",
			tmp / 1000, tmp % 1000);

	tmp = (dbg_time->dat_lpx + 1) * 8 * ui;
	dev_dbg(&client->dev, "HS-LPX: %d.%d nS.\n",
			tmp / 1000, tmp % 1000);

	tmp = (dbg_time->dat_prep + 1) * 8 * ui;
	dev_dbg(&client->dev, "HS-PREPARE: %d.%d nS.\n",
			tmp / 1000, tmp % 1000);

	tmp = (dbg_time->dat_zero + 6) * 8 * ui;
	dev_dbg(&client->dev, "HS-ZERO: %d.%d nS.\n",
			tmp / 1000, tmp % 1000);

	tmp = (dbg_time->dat_trail + 2) * 8 * ui;
	dev_dbg(&client->dev, "HS-TRAIL: %d.%d nS.\n",
			tmp / 1000, tmp % 1000);

	tmp = (dbg_time->gap + 9) * 8 * ui;
	dev_dbg(&client->dev, "HS-EXIT: %d.%d nS.\n",
			tmp / 1000, tmp % 1000);

	tmp = (dbg_time->twakeup + 1) * 8 * ui;
	dev_dbg(&client->dev, "Wakeup: %d.%d nS.\n",
			tmp / 1000, tmp % 1000);

}

static int pixter_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct pixter_device *dev = to_pixter_dev(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;
	u32 reg_val;

	dev_dbg(&client->dev, "Set stream for pixter. enable=%d\n", enable);

	mutex_lock(&dev->input_lock);

	if (enable) {
		__print_mipi_timing(sd);
		ret = pixter_config_rx(sd);
		if (ret)
			goto out;
		ret = pixter_config_tx(sd);
		if (ret)
			goto out;
	}

	/* Enable stream output. */
	reg_val = 1 << port_to_channel[dev->mipi_info->port];
	if (!enable)
		reg_val <<= 4;
	pixter_write_reg(sd, PIXTER_CPX_CTRL, reg_val);

	if (!enable)
		memset(dev->vc_setting, 0, sizeof(dev->vc_setting));
out:
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int pixter_g_frame_interval(struct v4l2_subdev *sd,
			struct v4l2_subdev_frame_interval *interval)
{
	struct pixter_device *dev = to_pixter_dev(sd);
	struct pixter_setting *setting;

	mutex_lock(&dev->input_lock);
	setting = &dev->settings[dev->cur_setting];

	/* Return the currently selected settings' maximum frame interval */

	interval->interval.numerator = 1;
	if (dev->dbg_fps.fps_ovrd)
		interval->interval.denominator = dev->dbg_fps.fps;
	else
		interval->interval.denominator =
			setting->vc[setting->def_vc].fps;

	mutex_unlock(&dev->input_lock);

	return 0;
}

static int pixter_s_frame_interval(struct v4l2_subdev *sd,
			struct v4l2_subdev_frame_interval *interval)
{
	struct pixter_device *dev = to_pixter_dev(sd);
	struct pixter_setting *setting;

	if (interval->interval.numerator == 0)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	setting = &dev->settings[dev->cur_setting];
	setting->vc[setting->def_vc].fps = interval->interval.denominator /
		interval->interval.numerator;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static int pixter_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned int index,
				 enum v4l2_mbus_pixelcode *code)
{
	struct pixter_device *dev = to_pixter_dev(sd);
	struct pixter_setting *setting = &dev->settings[dev->cur_setting];

	*code = format_bridge[setting->vc[0].format].v4l2_format;

	return 0;
}

static u32 pixter_try_mbus_fmt_locked(struct v4l2_subdev *sd,
				      struct v4l2_mbus_framefmt *fmt)
{
	struct pixter_device *dev = to_pixter_dev(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct atomisp_input_stream_info *stream_info =
		(struct atomisp_input_stream_info *)fmt->reserved;
	struct pixter_setting *settings = dev->settings;
	struct pixter_vc_setting *vc_setting = dev->vc_setting;
	u32 vc, i, j;
	s32 idx = -1, max_idx = -1;
	s64 w0, h0, mismatch, distance;
	s64 w1 = fmt->width;
	s64 h1 = fmt->height;
	s64 min_distance = LLONG_MAX;

	dev_dbg(&client->dev, "pixter_try_mbus_fmt. size=%dx%d stream=%d\n",
		fmt->width, fmt->height, stream_info->stream);
	if (dev->caps->sensor[0].stream_num == 1)
		vc = 0;
	else
		vc = stream_info->stream;
	for (i = 0; i < dev->setting_num; i++) {
		if (dev->setting_en[i] == 0)
			continue;
		max_idx = i;
		for (j = 0; j < 4; j++) {
			if (!vc_setting[j].width)
				continue;
			if (vc_setting[j].width != settings[i].vc[j].width ||
			    vc_setting[j].height != settings[i].vc[j].height)
				break;
		}
		if (j < 4)
			continue;
		w0 = settings[i].vc[vc].width;
		h0 = settings[i].vc[vc].height;
		if (w0 < w1 || h0 < h1)
			continue;
		mismatch = abs(w0 * h1 - w1 * h0) * 8192;
		do_div(mismatch, w1 * h0);
		if (mismatch > 8192 * PIXTER_MAX_RATIO_MISMATCH / 100)
			continue;
		distance = (w0 * h1 + w1 * h0) * 8192;
		do_div(distance, w1 * h1);
		if (distance < min_distance) {
			min_distance = distance;
			idx = i;
		}
	}
	if (idx < 0 && max_idx < 0) {
		idx = dev->setting_num - 1;
		dev_warn(&client->dev, "All settings disabled, using: %dx%d\n",
				settings[idx].vc[vc].width,
				settings[idx].vc[vc].height);
	} else if (idx < 0) {
		idx = max_idx;
		dev_warn(&client->dev, "using max enabled resolution: %dx%d\n",
				settings[idx].vc[vc].width,
				settings[idx].vc[vc].height);
	}
	fmt->width = settings[idx].vc[vc].width;
	fmt->height = settings[idx].vc[vc].height;
	fmt->code = format_bridge[settings[idx].vc[vc].format].v4l2_format;
	return idx;
}

static int pixter_try_mbus_fmt(struct v4l2_subdev *sd,
			       struct v4l2_mbus_framefmt *fmt)
{
	struct pixter_device *dev = to_pixter_dev(sd);

	mutex_lock(&dev->input_lock);
	pixter_try_mbus_fmt_locked(sd, fmt);
	mutex_unlock(&dev->input_lock);

	return 0;
}

static int pixter_g_mbus_fmt(struct v4l2_subdev *sd,
			      struct v4l2_mbus_framefmt *fmt)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct pixter_device *dev = to_pixter_dev(sd);
	struct atomisp_input_stream_info *stream_info =
		(struct atomisp_input_stream_info *)fmt->reserved;
	struct pixter_setting *setting;
	u32 vc;

	if (!fmt)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	if (dev->caps->sensor[0].stream_num == 1)
		vc = 0;
	else
		vc = stream_info->stream;

	setting = &dev->settings[dev->cur_setting];
	fmt->width = setting->vc[vc].width;
	fmt->height = setting->vc[vc].height;
	fmt->code = format_bridge[setting->vc[vc].format].v4l2_format;
	mutex_unlock(&dev->input_lock);

	dev_dbg(&client->dev, "%s w:%d h:%d code: 0x%x stream: %d\n", __func__,
			fmt->width, fmt->height, fmt->code,
			stream_info->stream);

	return 0;
}

static int pixter_s_mbus_fmt(struct v4l2_subdev *sd,
			      struct v4l2_mbus_framefmt *fmt)
{
	struct pixter_device *dev = to_pixter_dev(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct atomisp_input_stream_info *stream_info =
		(struct atomisp_input_stream_info *)fmt->reserved;

	if (!fmt)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	dev->cur_setting = pixter_try_mbus_fmt_locked(sd, fmt);
	if (dev->caps->sensor[0].stream_num == 1)
		stream_info->ch_id = 0;
	else
		stream_info->ch_id = stream_info->stream;
	dev->vc_setting[stream_info->ch_id] =
		dev->settings[dev->cur_setting].vc[stream_info->ch_id];
	mutex_unlock(&dev->input_lock);
	dev_dbg(&client->dev, "%s w:%d h:%d code: 0x%x stream: %d\n", __func__,
			fmt->width, fmt->height, fmt->code,
			stream_info->stream);

	return 0;
}

static int pixter_g_register(struct v4l2_subdev *sd,
			     struct v4l2_dbg_register *reg)
{
	int ret;
	u32 reg_val;

	if (reg->size != 4)
		return -EINVAL;

	ret = pixter_read_reg(sd, reg->reg, &reg_val);
	if (ret)
		return ret;

	reg->val = reg_val;

	return 0;
}

static int pixter_s_register(struct v4l2_subdev *sd,
			     const struct v4l2_dbg_register *reg)
{
	if (reg->size != 4)
		return -EINVAL;

	return pixter_write_reg(sd, reg->reg, reg->val);
}

static long pixter_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	switch (cmd) {
	case ATOMISP_IOC_S_EXPOSURE:
		break;
	case ATOMISP_IOC_G_SENSOR_PRIV_INT_DATA:
		break;
	case VIDIOC_DBG_G_REGISTER:
		pixter_g_register(sd, arg);
		break;
	case VIDIOC_DBG_S_REGISTER:
		pixter_s_register(sd, arg);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int
pixter_enum_mbus_code(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_mbus_code_enum *code)
{
	struct pixter_device *dev = to_pixter_dev(sd);
	if (code->index >= 1)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	code->code = dev->format.code;
	mutex_unlock(&dev->input_lock);
	return 0;
}

static int
pixter_enum_frame_size(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			struct v4l2_subdev_frame_size_enum *fse)
{
	int index = fse->index;
	struct pixter_device *dev = to_pixter_dev(sd);

	mutex_lock(&dev->input_lock);
	if (index >= 1) {
		mutex_unlock(&dev->input_lock);
		return -EINVAL;
	}

	fse->min_width = dev->settings[dev->cur_setting].vc[0].width;
	fse->min_height = dev->settings[dev->cur_setting].vc[0].height;
	fse->max_width = fse->min_width;
	fse->max_height = fse->min_height;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static struct v4l2_mbus_framefmt *
__pixter_get_pad_format(struct pixter_device *sensor,
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
pixter_set_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct pixter_device *dev = to_pixter_dev(sd);

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		dev->format = fmt->format;

	return 0;
}

static int
pixter_get_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct pixter_device *dev = to_pixter_dev(sd);
	struct v4l2_mbus_framefmt *format =
			__pixter_get_pad_format(dev, fh, fmt->pad, fmt->which);

	fmt->format = *format;

	return 0;
}

static int pixter_enum_framesizes(struct v4l2_subdev *sd,
				   struct v4l2_frmsizeenum *fsize)
{
	unsigned int index = fsize->index;
	struct pixter_device *dev = to_pixter_dev(sd);

	mutex_lock(&dev->input_lock);
	if (index >= 1) {
		mutex_unlock(&dev->input_lock);
		return -EINVAL;
	}

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = dev->settings[dev->cur_setting].vc[0].width;
	fsize->discrete.height = dev->settings[dev->cur_setting].vc[0].height;
	fsize->reserved[0] = 1;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static int pixter_enum_frameintervals(struct v4l2_subdev *sd,
				       struct v4l2_frmivalenum *fival)
{
	struct pixter_device *dev = to_pixter_dev(sd);

	mutex_lock(&dev->input_lock);
	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fival->width = dev->settings[dev->cur_setting].vc[0].width;
	fival->height = dev->settings[dev->cur_setting].vc[0].height;
	fival->discrete.numerator = 1;
	fival->discrete.denominator = 30;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static int pixter_s_power(struct v4l2_subdev *sd, int on)
{
	struct pixter_device *dev = to_pixter_dev(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u32 reg_val;

	dev_dbg(&client->dev, "Set power for pixter. on=%d\n", on);
	/* Disable channel output. */
	reg_val = 1 << (port_to_channel[dev->mipi_info->port] + 4);
	pixter_write_reg(sd, PIXTER_CPX_CTRL, reg_val);
	memset(dev->vc_setting, 0, sizeof(dev->vc_setting));
	return 0;
}

static ssize_t pixter_dbgfs_read(struct file *file, char __user *buf,
					size_t size, loff_t *ppos)
{
	struct pixter_dbgfs_data *data = file->f_inode->i_private;
	struct pixter_device *dev = data->dev;
	ssize_t ret = 0;
	u32 *val = (u32 *) data->ptr;
	u32 i;

	char *str = kzalloc(1024, GFP_KERNEL);
	if (!str)
		return 0;
	if (val >= dev->setting_en &&
	    val < &dev->setting_en[dev->setting_num]) {
		u32 idx = (val - dev->setting_en);
		struct pixter_setting *setting = &dev->settings[idx];
		char sub_str[128];
		if (idx >= dev->setting_num)
			goto out;
		snprintf(str, 1024, "Valid VCs: %d\n", setting->valid_vc_num);
		for (i = 0; i < 4; i++) {
			struct pixter_vc_setting *vc =
				&setting->vc[i];
			if (vc->width == 0)
				continue;
			snprintf(sub_str, 128, "VC%d: %dx%d @ %dfps - %s\n",
				i, vc->width, vc->height, vc->fps,
				format_bridge[vc->format].name);
			strncat(str, sub_str, 1023 - strlen(str));
		}
		snprintf(sub_str, 128, "Def: VC%d\nState: %s\n",
			setting->def_vc,
			dev->setting_en[idx] ? "Enabled" : "Disabled");
		strncat(str, sub_str, 1023 - strlen(str));
	} else {
		snprintf(str, 1024, "%d\n", *val);
	}
	ret = simple_read_from_buffer(buf, size, ppos, str, strlen(str));
out:
	kfree(str);
	return ret;
}

static ssize_t pixter_dbgfs_write(struct file *file, const char __user *buf,
					size_t size, loff_t *ppos)
{
	struct pixter_dbgfs_data *data = file->f_inode->i_private;
	struct pixter_device *dev = data->dev;
	u32 *val = (u32 *) data->ptr;
	char str[16] = {0};
	ssize_t ret;
	int sf_ret;

	ret =  simple_write_to_buffer(str, 16, ppos, buf, size);
	sf_ret = sscanf(str, "%d", val);
	if (val == &dev->dbg_timing.timing_ovrd && *val == 0)
		pixter_config_tx(&dev->sd);

	return ret;
}

static const char * const ctrl_run_mode_menu[] = {
	NULL,
	"Video",
	"Still capture",
	"Continuous capture",
	"Preview",
};

static const struct v4l2_ctrl_config ctrls[] = {
	{
		.id = V4L2_CID_RUN_MODE,
		.name = "Run Mode",
		.type = V4L2_CTRL_TYPE_MENU,
		.min = 1,
		.def = 4,
		.max = 4,
		.qmenu = ctrl_run_mode_menu,
	}
};

static const struct v4l2_subdev_core_ops pixter_core_ops = {
	.s_power	= pixter_s_power,
	.queryctrl	= v4l2_subdev_queryctrl,
	.g_ctrl		= v4l2_subdev_g_ctrl,
	.s_ctrl		= v4l2_subdev_s_ctrl,
	.ioctl = pixter_ioctl,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register	= pixter_g_register,
	.s_register	= pixter_s_register,
#endif
};

static const struct v4l2_subdev_video_ops pixter_video_ops = {
	.s_stream = pixter_s_stream,
	.enum_framesizes = pixter_enum_framesizes,
	.enum_frameintervals = pixter_enum_frameintervals,
	.enum_mbus_fmt = pixter_enum_mbus_fmt,
	.try_mbus_fmt = pixter_try_mbus_fmt,
	.g_mbus_fmt = pixter_g_mbus_fmt,
	.s_mbus_fmt = pixter_s_mbus_fmt,
	.g_frame_interval = pixter_g_frame_interval,
	.s_frame_interval = pixter_s_frame_interval,
};

static const struct v4l2_subdev_pad_ops pixter_pad_ops = {
	.enum_mbus_code = pixter_enum_mbus_code,
	.enum_frame_size = pixter_enum_frame_size,
	.get_fmt = pixter_get_pad_format,
	.set_fmt = pixter_set_pad_format,
};

static const struct v4l2_subdev_ops pixter_ops = {
	.core = &pixter_core_ops,
	.video = &pixter_video_ops,
	.pad = &pixter_pad_ops,
};

static const struct media_entity_operations pixter_entity_ops = {
	.link_setup = NULL,
};

static const struct file_operations pixter_dbgfs_fops = {
	.read = pixter_dbgfs_read,
	.write = pixter_dbgfs_write,
	.llseek = generic_file_llseek,
};

static int pixter_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct pixter_device *dev = to_pixter_dev(sd);

	if (dev->sd.entity.links)
		media_entity_cleanup(&dev->sd.entity);
	dev->platform_data->csi_cfg(sd, 0);
	v4l2_device_unregister_subdev(sd);
	if (dev->dbgfs_data)
		debugfs_remove_recursive(dev->dbgfs_data[0].entry);

	return 0;
}

static int pixter_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct pixter_device *dev;
	const struct atomisp_camera_caps *caps = NULL;
	char *pixter_name = NULL;
	struct pixter_setting *settings;
	struct pixter_dbgfs_data *dbgfs_data;
	u32 reg_val, i, j;
	int ret;

	/* allocate sensor device & init sub device */
	dev = devm_kzalloc(&client->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	mutex_init(&dev->input_lock);

	dev->dbg_timing.mipi_clk = PIXTER_DEF_CLOCK;

	v4l2_i2c_subdev_init(&dev->sd, client, &pixter_ops);

	if (client->dev.platform_data) {
		dev->platform_data = client->dev.platform_data;
		ret = dev->platform_data->csi_cfg(&dev->sd, 1);
		if (ret)
			goto out_free;
		if (dev->platform_data->get_camera_caps)
			caps = dev->platform_data->get_camera_caps();
		else
			caps = atomisp_get_default_camera_caps();
		dev->caps = caps;
	}

	dev->mipi_info = v4l2_get_subdev_hostdata(&dev->sd);
	if (!dev->mipi_info) {
		dev_err(&client->dev, "Faild to get mipi info.\n");
		goto out_free;
	}

	/* Get the number of mipi lanes */
	dev->dbg_timing.mipi_lanes_num = dev->mipi_info->num_lanes;

	dev->regmap = devm_regmap_init_i2c(client,
					   &pixter_reg_config);
	if (IS_ERR(dev->regmap)) {
		ret = PTR_ERR(dev->regmap);
		dev_err(&client->dev,
			"Failed to allocate register map: %d\n", ret);
		goto out_free;
	}

	/* Load Pixter settings */
	pixter_write_reg(&dev->sd, PIXTER_SDRAM_BASE, 0);
	pixter_read_reg(&dev->sd, PIXTER_MAGIC_ADDR, &reg_val);
	if (reg_val != PIXTER_MAGIC) {
		dev_err(&client->dev,
			"PIXTER magic does not match. Got 0x%X\n", reg_val);
		ret = -EIO;
		goto out_free;
	}
	pixter_read_reg(&dev->sd, PIXTER_SETTING_NUM, &dev->setting_num);
	dev->settings = devm_kzalloc(&client->dev,
		sizeof(struct pixter_setting) *
		dev->setting_num, GFP_KERNEL);
	if (!dev->settings) {
		dev_err(&client->dev, "OOM when allocating settings.\n");
		ret = -ENOMEM;
		goto out_free;
	}
	settings = dev->settings;

	ret = pixter_read_buf(&dev->sd, PIXTER_SETTING_START,
			sizeof(struct pixter_setting) * dev->setting_num,
			settings);
	if (ret) {
		dev_err(&client->dev, "Failed to read Pixter settings\n");
		goto out_free;
	}

	/* Find settings that match the current device. */
	for (i = 0, j = 0; i < dev->setting_num; i++) {
		if (caps->sensor[0].stream_num == settings[i].valid_vc_num)
			settings[j++] = settings[i];
	}
	dev->setting_num = j;
	dev_info(&client->dev, "Setting num=%d\n", dev->setting_num);
	if (!dev->setting_num) {
		dev_err(&client->dev, "No matched settings loaded.\n");
		ret = -ENODEV;
		goto out_free;
	}

	dev->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->pad.flags = MEDIA_PAD_FL_SOURCE;
	dev->sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
	dev->format.code = format_bridge[
		settings[0].vc[settings[0].def_vc].format].v4l2_format;

	/*
	 * sd->name is updated with sensor driver name by the v4l2.
	 * change it to sensor name in this case.
	 */
	if (dev->mipi_info->port == ATOMISP_CAMERA_PORT_PRIMARY)
		pixter_name = PIXTER_0;
	else if (dev->mipi_info->port == ATOMISP_CAMERA_PORT_SECONDARY)
		pixter_name = PIXTER_1;
	else
		pixter_name = PIXTER_2;
	snprintf(dev->sd.name, sizeof(dev->sd.name), "%s %d-%04x",
		pixter_name, i2c_adapter_id(client->adapter), client->addr);

	dev_info(&client->dev, "%s dev->sd.name: %s\n", __func__, dev->sd.name);

	dev->sd.entity.ops = &pixter_entity_ops;
	dev->sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;

	ret = v4l2_ctrl_handler_init(&dev->ctrl_handler, ARRAY_SIZE(ctrls));
	if (ret)
		goto out_free;

	for (i = 0; i < ARRAY_SIZE(ctrls); i++)
		v4l2_ctrl_new_custom(&dev->ctrl_handler, &ctrls[i], NULL);

	if (dev->ctrl_handler.error) {
		ret = -EINVAL;
		goto out_free;
	}

	/* Use same lock for controls as for everything else. */
	dev->ctrl_handler.lock = &dev->input_lock;
	dev->sd.ctrl_handler = &dev->ctrl_handler;
	v4l2_ctrl_handler_setup(&dev->ctrl_handler);

	ret = media_entity_init(&dev->sd.entity, 1, &dev->pad, 0);
	if (ret)
		goto out_free;

	/* Create debugfs nodes. */
	dev->dbgfs_data = devm_kzalloc(&client->dev,
		sizeof(struct pixter_dbgfs_data) *
		(ARRAY_SIZE(dbgfs) + dev->setting_num + 1), GFP_KERNEL);
	if (!dev->dbgfs_data)
		goto out_free;
	dbgfs_data = dev->dbgfs_data;
	dbgfs_data[0].entry = debugfs_create_dir(pixter_name, NULL);
	for (i = 1; i < ARRAY_SIZE(dbgfs); i++) {
		struct dentry *parent;
		for (j = 0; j < i; j++) {
			if (!strcmp(dbgfs[i].parent, dbgfs[j].name))
				break;
		}
		if (j == i)
			continue;
		parent = dbgfs_data[j].entry;
		dbgfs_data[i].dev = dev;
		dbgfs_data[i].ptr = (u8 *)dev + dbgfs[i].offset;
		if (dbgfs[i].type == DBGFS_DIR)
			dbgfs_data[i].entry = debugfs_create_dir(dbgfs[i].name,
				parent);
		else
			dbgfs_data[i].entry = debugfs_create_file(dbgfs[i].name,
				dbgfs[i].mode, parent,
				&dbgfs_data[i], &pixter_dbgfs_fops);
	}
	/* Create setting nodes. */
	dev->setting_en = devm_kzalloc(&client->dev,
		sizeof(u32) * dev->setting_num, GFP_KERNEL);
	if (!dev->setting_en)
		goto out_free;
	dbgfs_data[i].entry = debugfs_create_dir("settings",
		dbgfs_data[0].entry);
	for (j = 0; j < dev->setting_num; j++) {
		char setting_name[32];
		u32 idx = i + j + 1;
		struct pixter_vc_setting *vc =
			&settings[j].vc[settings[j].def_vc];

		dev->setting_en[j] = 1;
		snprintf(setting_name, 32, "%d.%dx%d_%s@%d", j, vc->width,
			vc->height, format_bridge[vc->format].name, vc->fps);
		dbgfs_data[idx].dev = dev;
		dbgfs_data[idx].ptr = &dev->setting_en[j];
		dbgfs_data[idx].entry = debugfs_create_file(setting_name,
				S_IRUSR|S_IWUSR, dbgfs_data[i].entry,
				&dbgfs_data[idx], &pixter_dbgfs_fops);
	}
	pixter_read_mipi_timing(&dev->sd);

	return 0;

out_free:
	pixter_remove(client);
	return ret;
}

static const struct i2c_device_id pixter_ids[] = {
	{PIXTER_0, 0},
	{PIXTER_1, 0},
	{PIXTER_2, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, pixter_ids);

static struct i2c_driver pixter_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = PIXTER_DRV,
	},
	.probe = pixter_probe,
	.remove = pixter_remove,
	.id_table = pixter_ids,
};

module_i2c_driver(pixter_driver);

MODULE_DESCRIPTION("Pixter MIPI CSI simulator driver.");
MODULE_AUTHOR("Tianshu Qiu <tian.shu.qiu@intel.com>");
MODULE_LICENSE("GPL");
