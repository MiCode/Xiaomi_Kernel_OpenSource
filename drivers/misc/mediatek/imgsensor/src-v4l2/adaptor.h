/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2019 MediaTek Inc. */

#ifndef __ADAPTOR_H__
#define __ADAPTOR_H__

#include <linux/i2c.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>

#include "adaptor-def.h"
#include "adaptor-subdrv.h"

struct adaptor_ctx;

union feature_para {
	u64 u64[4];
	u32 u32[8];
	u16 u16[16];
	u8 u8[32];
};

struct sensor_mode {
	u32 id;
	u32 llp;
	u32 fll;
	u32 width;
	u32 height;
	u32 mipi_pixel_rate;
	u32 max_framerate;
	u32 pclk;
	u64 linetime_in_ns;
};

struct adaptor_hw_ops {
	int (*set)(struct adaptor_ctx *ctx, void *data, int val);
	int (*unset)(struct adaptor_ctx *ctx, void *data, int val);
	void *data;
};

struct adaptor_ctx {
	struct mutex mutex;
	struct i2c_client *i2c_client;
	struct device *dev;
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_fwnode_endpoint ep;

	/* V4L2 Controls */
	struct v4l2_ctrl_handler ctrls;
	struct v4l2_ctrl *pixel_rate;
	struct v4l2_ctrl *vblank;
	struct v4l2_ctrl *hblank;
	struct v4l2_ctrl *exposure;
	struct v4l2_ctrl *exposure_abs;
	struct v4l2_ctrl *hflip;
	struct v4l2_ctrl *vflip;
	struct v4l2_ctrl *pd_pixel_region;

	/* custom v4l2 ctrls */
	struct v4l2_ctrl *anti_flicker;
	struct v4l2_ctrl *frame_sync;
	struct v4l2_ctrl *awb_gain;
	struct v4l2_ctrl *shutter_gain_sync;
	struct v4l2_ctrl *dual_gain;
	struct v4l2_ctrl *ihdr_shutter_gain;
	struct v4l2_ctrl *hdr_shutter;
	struct v4l2_ctrl *shutter_frame_length;
	struct v4l2_ctrl *pdfocus_area;
	struct v4l2_ctrl *hdr_atr;
	struct v4l2_ctrl *hdr_tri_shutter;
	struct v4l2_ctrl *hdr_tri_gain;

	/* hw handles */
	struct clk *clk[CLK_MAXCNT];
	struct regulator *regulator[REGULATOR_MAXCNT];
	struct pinctrl *pinctrl;
	struct pinctrl_state *state[STATE_MAXCNT];
	struct adaptor_hw_ops hw_ops[HW_ID_MAXCNT];

	/* sensor */
	struct subdrv_entry *subdrv;
	struct subdrv_ctx subctx;
	struct sensor_mode mode[MODE_MAXCNT];
	struct sensor_mode *cur_mode;
	int mode_cnt;
	MSDK_SENSOR_INFO_STRUCT sensor_info;
	MSDK_SENSOR_CONFIG_STRUCT sensor_cfg;
	int fmt_code;
	int idx; /* requireed by frame-sync modules */

	/* sensor property */
	u32 location;
	u32 rotation;

	/* flags */
	unsigned int streaming:1;
	unsigned int is_sensor_init:1;

};

#endif
