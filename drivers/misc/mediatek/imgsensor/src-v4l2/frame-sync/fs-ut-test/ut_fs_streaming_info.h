/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _UT_FS_STREAMING_INFO_H
#define _UT_FS_STREAMING_INFO_H

#include "../frame_sync.h"


struct ut_fs_streaming_sensor_list {
	char *sensor_name;
	unsigned int sensor_idx;
	unsigned int tg;
	struct fs_streaming_st *sensor;
};


/******************************************************************************/
struct fs_streaming_st imx586 = {
	.sensor_idx = 0,
	.sensor_id = 0x0586,
	.tg = 2,
	.fl_active_delay = 3,
	.def_fl_lc = 3068,
	.max_fl_lc = 0xffff,
	.def_shutter_lc = 0x3D0,
};


struct fs_streaming_st s5k3m5sx = {
	.sensor_idx = 2,
	.sensor_id = 0x30D5,
	.tg = 1,
	.fl_active_delay = 2,
	.def_fl_lc = 3314,
	.max_fl_lc = 0xffff,
	.def_shutter_lc = 0x3D0,
};


struct fs_streaming_st imx481 = {
	.sensor_idx = 4,
	.sensor_id = 0x0481,
	.tg = 3,
	.fl_active_delay = 3,
	.def_fl_lc = 3776,
	.max_fl_lc = 0xffff,
	.def_shutter_lc = 0x3D0,
};


struct fs_streaming_st imx766 = {
	.sensor_idx = 0,
	.sensor_id = 0x0766,
	.tg = 2,
	.fl_active_delay = 3,
	.def_fl_lc = 4844,
	.max_fl_lc = 0xffff,
	.def_shutter_lc = 0x3D0,
};


/******************************************************************************/
struct ut_fs_streaming_sensor_list ut_fs_s_list[] = {
	/* Head */
	{
		.sensor_name = "imx586",
		.sensor = &imx586,
	},

	{
		.sensor_name = "s5k3m5sx",
		.sensor = &s5k3m5sx,
	},

	{
		.sensor_name = "imx481",
		.sensor = &imx481,
	},

	{
		.sensor_name = "imx766",
		.sensor = &imx766,
	},

	/* End */
	{
		.sensor_name = "NULL",
		.sensor_idx = 0,
		.tg = 255,
		.sensor = NULL,
	},
};

#endif
