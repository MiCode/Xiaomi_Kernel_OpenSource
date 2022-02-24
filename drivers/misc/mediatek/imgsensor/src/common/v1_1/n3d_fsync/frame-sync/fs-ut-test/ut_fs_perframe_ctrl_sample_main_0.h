/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "ut_fs_streaming_info.h"
#include "ut_fs_perframe_ctrl_sample_def.h"


struct fs_perframe_st frameCtrl_main0[] = {
	/* Head */
	{
		.sensor_idx = 0,
		.sensor_id = 0x0586,
		.min_fl_lc = US_TO_LC(33350, 10469),
		.shutter_lc = US_TO_LC(19997, 10469),
		.flicker_en = 0,
		.pclk = 752000000,
		.linelength = 7872,
		.margin_lc = 48,
		.lineTimeInNs = 10469,
	},

	{
		.sensor_idx = 0,
		.sensor_id = 0x0586,
		.min_fl_lc = US_TO_LC(33350, 10469),
		.shutter_lc = US_TO_LC(19997, 10469),
		.flicker_en = 1,
		.pclk = 752000000,
		.linelength = 7872,
		.margin_lc = 48,
		.lineTimeInNs = 10469,
	},
};


struct ut_perframe_ctrl_sample_st pf_settings_main0[] = {
	/* Head */
	{
		.pf_settings = &frameCtrl_main0[1],
		.vsyncs = 3,
		.last_vsync_timestamp = 236656572,
	},

	{
		.pf_settings = &frameCtrl_main0[1],
		.vsyncs = 2,
		.last_vsync_timestamp = 236720817,
	},

	{
		.pf_settings = &frameCtrl_main0[0],
		.vsyncs = 1,
		.last_vsync_timestamp = 236768745,
	},

	{
		.pf_settings = &frameCtrl_main0[0],
		.vsyncs = 1,
		.last_vsync_timestamp = 236816673,
	},
};
