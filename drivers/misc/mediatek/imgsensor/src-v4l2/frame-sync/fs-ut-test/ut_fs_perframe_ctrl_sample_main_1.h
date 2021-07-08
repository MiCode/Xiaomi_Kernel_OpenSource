/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "ut_fs_streaming_info.h"
#include "ut_fs_perframe_ctrl_sample_def.h"


struct fs_perframe_st frameCtrl_main1[] = {
	/* Head */
	{
		.sensor_idx = 2,
		.sensor_id = 0x30D5,
		.min_fl_lc = US_TO_LC(33350, 10059),
		.shutter_lc = US_TO_LC(19997, 10059),
		.flicker_en = 0,
		.pclk = 482000000,
		.linelength = 4848,
		.margin_lc = 4,
		.lineTimeInNs = 10059,
	},

	{
		.sensor_idx = 2,
		.sensor_id = 0x30D5,
		.min_fl_lc = US_TO_LC(33350, 10059),
		.shutter_lc = US_TO_LC(19997, 10059),
		.flicker_en = 1,
		.pclk = 482000000,
		.linelength = 4848,
		.margin_lc = 4,
		.lineTimeInNs = 10059,
	}
};


struct ut_perframe_ctrl_sample_st pf_settings_main1[] = {
	/* Head */
	{
		.pf_settings = &frameCtrl_main1[0],
		.vsyncs = 3,
		.last_vsync_timestamp = 236670715,
	},

	{
		.pf_settings = &frameCtrl_main1[0],
		.vsyncs = 1,
		.last_vsync_timestamp = 236704050,
	},

	{
		.pf_settings = &frameCtrl_main1[0],
		.vsyncs = 2,
		.last_vsync_timestamp = 236770765,
	},

	{
		.pf_settings = &frameCtrl_main1[0],
		.vsyncs = 1,
		.last_vsync_timestamp = 236804124,
	},
};
