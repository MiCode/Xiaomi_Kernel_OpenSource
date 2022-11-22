/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "ut_fs_streaming_info.h"
#include "ut_fs_perframe_ctrl_sample_def.h"


struct fs_perframe_st frameCtrl_main2[] = {
	/* Head */
	{
		.sensor_idx = 4,
		.sensor_id = 0x0481,
		.min_fl_lc = US_TO_LC(33350, 8828),
		.shutter_lc = US_TO_LC(19997, 8828),
		.flicker_en = 1,
		.pclk = 580000000,
		.linelength = 5120,
		.margin_lc = 18,
		.lineTimeInNs = 8828,
	},

	{
		.sensor_idx = 4,
		.sensor_id = 0x0481,
		.min_fl_lc = US_TO_LC(33350, 8828),
		.shutter_lc = US_TO_LC(19997, 8828),
		.flicker_en = 1,
		.pclk = 580000000,
		.linelength = 5120,
		.margin_lc = 18,
		.lineTimeInNs = 8828,
	}
};
