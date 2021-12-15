/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _UT_FS_PLATFORM_ARG_H
#define _UT_FS_PLATFORM_ARG_H

#include "ut_fs_streaming_info.h"
#include "ut_fs_perframe_ctrl_sample_main_0.h"
#include "ut_fs_perframe_ctrl_sample_main_1.h"
#include "ut_fs_perframe_ctrl_sample_main_2.h"


/* CCU timer tick to us factor */
#define TICK_FACTOR 13


struct ut_fs_argument {
	unsigned int sensor_idx;
	struct fs_streaming_st *sensor;
};


/******************************************************************************/
// Must be check when evey times you run the test program
/******************************************************************************/
struct ut_fs_argument main0 = {
	.sensor_idx = 0,
	.sensor = &imx586
};


struct ut_fs_argument main1 = {
	.sensor_idx = 2,
	.sensor = &s5k3m5sx
};


struct ut_fs_argument main2 = {
	.sensor_idx = 4,
	.sensor = &imx481
};
/******************************************************************************/


#endif
