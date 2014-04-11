/*
 * Support for Pixter MIPI CSI simulator.
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

#ifndef __PIXTER_H__
#define __PIXTER_H__

#include <linux/atomisp_platform.h>
#include <linux/regmap.h>
#include <linux/debugfs.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#define PIXTER_0	"pixter_0"
#define PIXTER_1	"pixter_1"
#define PIXTER_2	"pixter_2"
#define PIXTER_DRV	"pixter_drv"

#define PIXTER_MAX_RATIO_MISMATCH	10
#define PIXTER_MAX_BITRATE_MBPS	6000 /* 1.5Gb x 4 lanes */
#define PIXTER_DEF_CLOCK	400000000 /* 400MHz */
#define PIXTER_DEF_VBPRE	0x60

#define PIXTER_I2C_ADDR		0x0
#define PIXTER_I2C_DATA_W	0x20
#define PIXTER_I2C_DATA_R	0x40

#define PIXTER_CHANNEL_BASE(c)	(((c) + 1) * 0x200)

#define PIXTER_MAGIC		0x54584950
#define PIXTER_CPX_CTRL		0x14
#define PIXTER_SDRAM_BASE	0x20

#define PIXTER_CH_SRC_DEST	(PIXTER_CHANNEL_BASE(c) + 0x0)
#define PIXTER_DFT_CTRL(c)	(PIXTER_CHANNEL_BASE(c) + 0x40)
#define PIXTER_RDR_START(c)	(PIXTER_CHANNEL_BASE(c) + 0x48)
#define PIXTER_RDR_END(c)	(PIXTER_CHANNEL_BASE(c) + 0x4C)
#define PIXTER_VERT_BLANK(c)	(PIXTER_CHANNEL_BASE(c) + 0x54)
#define PIXTER_TX_CTRL(c)	(PIXTER_CHANNEL_BASE(c) + 0x100)
#define PIXTER_TX_CTRL_NNS(c)	(PIXTER_CHANNEL_BASE(c) + 0x104)
#define PIXTER_TX_STATUS(c)	(PIXTER_CHANNEL_BASE(c) + 0x108)
#define PIXTER_TX_CSI2_CTRL(c)	(PIXTER_CHANNEL_BASE(c) + 0x110)
#define PIXTER_TX_CTRL_TIMING(c)	(PIXTER_CHANNEL_BASE(c) + 0x114)
#define PIXTER_TX_CK_TIMING(c)	(PIXTER_CHANNEL_BASE(c) + 0x118)
#define PIXTER_TX_DAT_TIMING(c)	(PIXTER_CHANNEL_BASE(c) + 0x11C)
#define PIXTER_TX_ULPS_TIMING(c)	(PIXTER_CHANNEL_BASE(c) + 0x120)

#define PIXTER_TX_READY		0x1
#define PIXTER_SRC_DEST_DEF	0x110 /* Select DFT mode. Enable CSI2 output */
#define PIXTER_DFT_BLOCK_MODE  0x2

#define PIXTER_MAGIC_ADDR	0x80000000
#define PIXTER_SETTING_NUM	0x80000004
#define PIXTER_SETTING_START	0x80000008

enum pixter_image_format {
	PIXTER_UNKNOWN_FMT,
	PIXTER_RGGB10,
	PIXTER_GRBG10,
	PIXTER_GBRG10,
	PIXTER_BGGR10,
	PIXTER_RGGB8,
	PIXTER_GRBG8,
	PIXTER_GBRG8,
	PIXTER_BGGR8,
	PIXTER_YUV422_8,
	PIXTER_YUV420_8
};

enum pixter_dbgfs_type {
	DBGFS_DIR,
	DBGFS_FILE
};

struct pixter_format_bridge {
	char *name;
	enum v4l2_mbus_pixelcode v4l2_format;
	enum atomisp_input_format atomisp_format;
	u32 bpp;
};

struct pixter_vc_setting {
	u32 width;
	u32 height;
	u32 fps;
	enum pixter_image_format format;
};

struct pixter_setting {
	u32 start;
	u32 end;
	u32 valid_vc_num;
	u32 def_vc;
	u32 block_mode;
	struct pixter_vc_setting vc[4];
};

struct pixter_fps {
	u32 fps_ovrd;
	u32 fps;
};

struct pixter_blank {
	u32 blank_ovrd;
	u32 h_blank;
	u32 v_blank_pre;
	u32 v_blank_post;
};

struct pixter_timing {
	u32 mipi_clk;
	u32 cont_hs_clk;
	u32 timing_ovrd;
	u32 pre;
	u32 post;
	u32 gap;
	u32 ck_lpx;
	u32 ck_prep;
	u32 ck_zero;
	u32 ck_trail;
	u32 dat_lpx;
	u32 dat_prep;
	u32 dat_zero;
	u32 dat_trail;
	u32 twakeup;
};

struct pixter_dbgfs {
	char *name;
	char *parent;
	enum pixter_dbgfs_type type;
	u32  offset;
};

struct pixter_dbgfs_data {
	struct pixter_device *dev;
	struct dentry *entry;
	void *ptr;
};

/* pixter device structure */
struct pixter_device {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_mbus_framefmt format;
	struct camera_sensor_platform_data *platform_data;
	const struct atomisp_camera_caps *caps;
	struct camera_mipi_info *mipi_info;
	struct mutex input_lock;
	struct regmap *regmap;
	struct v4l2_ctrl_handler ctrl_handler;

	u32 setting_num;
	u32 cur_setting;
	u32 cur_ch;
	struct pixter_vc_setting vc_setting[4];
	struct pixter_setting *settings;
	u32 *setting_en;

	struct pixter_fps dbg_fps;
	struct pixter_blank dbg_blank;
	struct pixter_timing dbg_timing;
	struct pixter_dbgfs_data *dbgfs_data;
};

#endif
