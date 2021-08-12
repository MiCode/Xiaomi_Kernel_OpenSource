/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _UT_FS_TEST_CONFIG_H
#define _UT_FS_TEST_CONFIG_H

#include "ut_fs_streaming_info.h"
#include "ut_fs_perframe_ctrl_info.h"


/* CCU timer tick to us factor */
#define TICK_FACTOR 13


/******************************************************************************/
struct ut_fs_test_sensor_cfg {
	char *sensor_name;
	unsigned int sensor_idx;
	unsigned int tg;
	struct fs_streaming_st *sensor;
	struct fs_perframe_st *mode;
	unsigned int mode_idx;
	unsigned int first_vts_value;
};


struct ut_fs_test_ext_ctrl_cfg {
	unsigned int sensor_idx;
	unsigned int do_ext_fl_at_n_run;
	unsigned int ext_fl_us;
	unsigned int do_seamless_switch_at_n_run;
};


struct ut_fs_test_env_cfg {
	unsigned int run_times;
	unsigned int sync_th;
	unsigned int passed_vsync;
	unsigned int passed_vsync_ratio;
	unsigned int passed_vsync_max_cnt;
	unsigned int lock_exp;
	unsigned int lock_exp_table_idx;
	unsigned int lock_flk;

	/* EXT CTRL */
	struct ut_fs_test_ext_ctrl_cfg *ext_ctrls;
};


struct ut_fs_test_list {
	char *test_name;
	unsigned int sync_tag;
	struct ut_fs_test_sensor_cfg *sensor_cfg;
	struct ut_fs_test_env_cfg *env_cfg;
	unsigned int exe_all_skip_ext_ctrl_test;
};
/******************************************************************************/





/******************************************************************************/
// for ut behavior testing
// Must be check when evey times you run the test program
/******************************************************************************/
struct ut_fs_test_sensor_cfg main0 = {
	.sensor_idx = 0,
	.sensor = &imx586
};


struct ut_fs_test_sensor_cfg main1 = {
	.sensor_idx = 2,
	.sensor = &s5k3m5sx
};


struct ut_fs_test_sensor_cfg main2 = {
	.sensor_idx = 4,
	.sensor = &imx586
};
/******************************************************************************/





/******************************************************************************/
// for ut execute test item
//    sensor_cfg
/******************************************************************************/

/* FL_act_delay : Normal(N+1) with Normal(N+1) */
struct ut_fs_test_sensor_cfg sensor_cfg_01[] = {
	{
		.sensor_name = "s5k3m5sx",
		.sensor_idx = 0,
		.tg = 2,
		.sensor = &s5k3m5sx,
		.mode = s5k3m5sx_sensor_mode,
		.mode_idx = 0,
		.first_vts_value = 100000,
	},

	{
		.sensor_name = "s5k3m5sx",
		.sensor_idx = 2,
		.tg = 1,
		.sensor = &s5k3m5sx,
		.mode = s5k3m5sx_sensor_mode,
		.mode_idx = 0,
		.first_vts_value = 120000,
	},

	/* End */
	{
		.sensor_idx = 255,
		.tg = 255,
		.sensor = NULL,
		.mode = NULL,
	}
};


/* FL_act_delay : Normal(N+2) with Normal(N+2) */
struct ut_fs_test_sensor_cfg sensor_cfg_02[] = {
	{
		.sensor_name = "imx586",
		.sensor_idx = 0,
		.tg = 2,
		.sensor = &imx586,
		.mode = imx586_sensor_mode,
		.mode_idx = 0,
		.first_vts_value = 100000,
	},

	{
		.sensor_name = "imx586",
		.sensor_idx = 2,
		.tg = 1,
		.sensor = &imx586,
		.mode = imx586_sensor_mode,
		.mode_idx = 0,
		.first_vts_value = 120000,
	},

	/* End */
	{
		.sensor_idx = 255,
		.tg = 255,
		.sensor = NULL,
		.mode = NULL,
	}
};


/* FL_act_delay : Normal(N+2) with Normal(N+1) */
struct ut_fs_test_sensor_cfg sensor_cfg_03[] = {
	{
		.sensor_name = "imx586",
		.sensor_idx = 0,
		.tg = 2,
		.sensor = &imx586,
		.mode = imx586_sensor_mode,
		.mode_idx = 0,
		.first_vts_value = 100000,
	},

	{
		.sensor_name = "s5k3m5sx",
		.sensor_idx = 2,
		.tg = 1,
		.sensor = &s5k3m5sx,
		.mode = s5k3m5sx_sensor_mode,
		.mode_idx = 0,
		.first_vts_value = 120000,
	},

	/* End */
	{
		.sensor_idx = 255,
		.tg = 255,
		.sensor = NULL,
		.mode = NULL,
	}
};


/* FL_act_delay : STG-3-exp(N+2) with Normal(N+1) */
struct ut_fs_test_sensor_cfg sensor_cfg_04[] = {
	{
		.sensor_name = "imx766",
		.sensor_idx = 0,
		.tg = 2,
		.sensor = &imx766,
		.mode = imx766_sensor_mode,
		.mode_idx = 0,
		.first_vts_value = 100000,
	},

	{
		.sensor_name = "s5k3m5sx",
		.sensor_idx = 2,
		.tg = 1,
		.sensor = &s5k3m5sx,
		.mode = s5k3m5sx_sensor_mode,
		.mode_idx = 0,
		.first_vts_value = 120000,
	},

	/* End */
	{
		.sensor_idx = 255,
		.tg = 255,
		.sensor = NULL,
		.mode = NULL,
	}
};


/* FL_act_delay : STG-2-exp(N+2) with Normal(N+1) */
struct ut_fs_test_sensor_cfg sensor_cfg_05[] = {
	{
		.sensor_name = "imx766",
		.sensor_idx = 0,
		.tg = 2,
		.sensor = &imx766,
		.mode = imx766_sensor_mode,
		.mode_idx = 1,
		.first_vts_value = 100000,
	},

	{
		.sensor_name = "s5k3m5sx",
		.sensor_idx = 2,
		.tg = 1,
		.sensor = &s5k3m5sx,
		.mode = s5k3m5sx_sensor_mode,
		.mode_idx = 0,
		.first_vts_value = 120000,
	},

	/* End */
	{
		.sensor_idx = 255,
		.tg = 255,
		.sensor = NULL,
		.mode = NULL,
	}
};
/******************************************************************************/





/******************************************************************************/
// for ut execute test item
//    sensor_cfg
/******************************************************************************/
/* EXT CTRL config */
struct ut_fs_test_ext_ctrl_cfg ext_ctrl_cfg_01[] = {
	{
		.sensor_idx = 0,
		.do_ext_fl_at_n_run = 8,
		.ext_fl_us = 5000,
		.do_seamless_switch_at_n_run = 16,
	},

	/* End */
	{
		.sensor_idx = 0,
		.do_ext_fl_at_n_run = 0,
		.ext_fl_us = 0,
		.do_seamless_switch_at_n_run = 0,
	}
};


/* perframe_ctrl trigger with no lock_exp and no lock_flk */
struct ut_fs_test_env_cfg env_cfg_01 = {
	.run_times = 10000,
	.sync_th = 550,
	.passed_vsync = 0,
	.passed_vsync_ratio = 0,
	.passed_vsync_max_cnt = 0,
	.lock_exp = 0,
	.lock_flk = 0,
};


/* NOT perframe_ctrl trigger with no lock_exp and no lock_flk */
struct ut_fs_test_env_cfg env_cfg_02 = {
	.run_times = 10000,
	.sync_th = 550,
	.passed_vsync = 1,
	.passed_vsync_ratio = 40,
	.passed_vsync_max_cnt = 3,
	.lock_exp = 0,
	.lock_flk = 0,
};


/* NOT perframe_ctrl trigger with lock_exp and no lock_flk */
struct ut_fs_test_env_cfg env_cfg_03 = {
	.run_times = 10000,
	.sync_th = 550,
	.passed_vsync = 1,
	.passed_vsync_ratio = 40,
	.passed_vsync_max_cnt = 3,
	.lock_exp = 1,
	.lock_exp_table_idx = 3,
	.lock_flk = 0,
};


/* perframe_ctrl trigger with lock_exp and no lock_flk */
struct ut_fs_test_env_cfg env_cfg_04 = {
	.run_times = 10000,
	.sync_th = 550,
	.passed_vsync = 0,
	.passed_vsync_ratio = 0,
	.passed_vsync_max_cnt = 0,
	.lock_exp = 1,
	.lock_exp_table_idx = 3,
	.lock_flk = 0,
};


/* perframe_ctrl trigger with no lock_exp and no lock_flk */
struct ut_fs_test_env_cfg env_ext_ctrl_cfg_01 = {
	.run_times = 24,
	.sync_th = 550,
	.passed_vsync = 0,
	.passed_vsync_ratio = 0,
	.passed_vsync_max_cnt = 0,
	.lock_exp = 0,
	.lock_flk = 0,

	/* EXT CTRL */
	.ext_ctrls = ext_ctrl_cfg_01,
};
/******************************************************************************/

struct ut_fs_test_list test_list[] = {
	{
		.test_name = "Normal N+1 / Normal N+1, per-frame CTRL (NO lock exp / NO lock flk)",
		.sync_tag = 1,
		.sensor_cfg = sensor_cfg_01,
		.env_cfg = &env_cfg_01,
	},

	{
		.test_name =
			"Normal N+2 / Normal N+2 (same margin), per-frame CTRL (NO lock exp / NO lock flk)",
		.sync_tag = 1,
		.sensor_cfg = sensor_cfg_02,
		.env_cfg = &env_cfg_01,
	},

	{
		.test_name = "Normal N+2 / Normal N+1, per-frame CTRL (NO lock exp / NO lock flk)",
		.sync_tag = 1,
		.sensor_cfg = sensor_cfg_03,
		.env_cfg = &env_cfg_01,
	},

	{
		.test_name =
			"Normal N+1 / Normal N+1, NOT per-frame CTRL (NO lock exp / NO lock flk)",
		.sync_tag = 1,
		.sensor_cfg = sensor_cfg_01,
		.env_cfg = &env_cfg_02,
	},

	{
		.test_name =
			"Normal N+2 / Normal N+2 (same margin), NOT per-frame CTRL (NO lock exp / NO lock flk)",
		.sync_tag = 1,
		.sensor_cfg = sensor_cfg_02,
		.env_cfg = &env_cfg_02,
	},

	{
		.test_name = "Normal N+2 / Normal N+1, NOT per-frame CTRL (lock exp / NO lock flk)",
		.sync_tag = 1,
		.sensor_cfg = sensor_cfg_03,
		.env_cfg = &env_cfg_03,
	},

	{
		.test_name =
			"STG-3-exp-LE N+2 / Normal N+1, per-frame CTRL (NO lock exp / NO lock flk)",
		.sync_tag = 10,
		.sensor_cfg = sensor_cfg_04,
		.env_cfg = &env_cfg_01,
	},

	{
		.test_name =
			"STG-3-exp-SE N+2 / Normal N+1, per-frame CTRL (lock exp / NO lock flk)",
		.sync_tag = 18,
		.sensor_cfg = sensor_cfg_04,
		.env_cfg = &env_cfg_04,
	},

	{
		.test_name =
			"STG-2-exp-LE N+2 / Normal N+1, per-frame CTRL (NO lock exp / NO lock flk)",
		.sync_tag = 10,
		.sensor_cfg = sensor_cfg_05,
		.env_cfg = &env_cfg_01,
	},

	{
		.test_name =
			"STG-2-exp-SE N+2 / Normal N+1, per-frame CTRL (lock exp / NO lock flk)",
		.sync_tag = 18,
		.sensor_cfg = sensor_cfg_05,
		.env_cfg = &env_cfg_04,
	},

	{
		.test_name =
			"EXT CTRL: STG-3-exp-LE N+2 / Normal N+1, per-frame CTRL (NO lock exp / NO lock flk)",
		.sync_tag = 10,
		.sensor_cfg = sensor_cfg_04,
		.env_cfg = &env_ext_ctrl_cfg_01,
		.exe_all_skip_ext_ctrl_test = 1,
	},

	/* End */
	{
		.test_name = NULL,
		.sensor_cfg = NULL,
		.env_cfg = NULL,
	}
};


#endif
