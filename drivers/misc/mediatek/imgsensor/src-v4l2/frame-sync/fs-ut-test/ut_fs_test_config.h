/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _UT_FS_TEST_CONFIG_H
#define _UT_FS_TEST_CONFIG_H

#include "ut_fs_streaming_info.h"
#include "ut_fs_perframe_ctrl_info.h"


/* CCU timer tick to us factor (for UT testing, set to 1) */
#define TICK_FACTOR 1


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


struct ut_fs_test_n_1_mode_cfg {
	unsigned int sensor_idx;
	// unsigned int assign_frame_tag:1;
	unsigned int n;
	unsigned int on_min_fl_us;
	unsigned int off_min_fl_us;
	unsigned int turn_on_at_n_run;
	unsigned int turn_off_at_n_run;
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
	unsigned int auto_test_must_run:1;
	unsigned int sync_tag;

	/* 0: depend on UT config */
	/* 1: Force CTRL Pair / 2: Force SA(StandAlone) */
	unsigned int alg_method;

	struct ut_fs_test_sensor_cfg *sensor_cfg;
	struct ut_fs_test_env_cfg *env_cfg;

	struct ut_fs_test_n_1_mode_cfg *n_1_cfg;

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
	.sensor = &imx481
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
		//.first_vts_value = 100000,
		.first_vts_value = 110000,
	},

	{
		.sensor_name = "s5k3m5sx",
		.sensor_idx = 2,
		.tg = 1,
		.sensor = &s5k3m5sx,
		.mode = s5k3m5sx_sensor_mode,
		.mode_idx = 0,
		//.first_vts_value = 110000,
		.first_vts_value = 100000,
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
		//.first_vts_value = 100000,
		.first_vts_value = 110000,
	},

	{
		.sensor_name = "imx586",
		.sensor_idx = 2,
		.tg = 1,
		.sensor = &imx586,
		.mode = imx586_sensor_mode,
		.mode_idx = 0,
		//.first_vts_value = 110000,
		.first_vts_value = 100000,
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
		//.first_vts_value = 100000,
		.first_vts_value = 110000,
	},

	{
		.sensor_name = "s5k3m5sx",
		.sensor_idx = 2,
		.tg = 1,
		.sensor = &s5k3m5sx,
		.mode = s5k3m5sx_sensor_mode,
		.mode_idx = 0,
		//.first_vts_value = 110000,
		.first_vts_value = 100000,
	},

	/* End */
	{
		.sensor_idx = 255,
		.tg = 255,
		.sensor = NULL,
		.mode = NULL,
	}
};


/* FL_act_delay : Normal(N+1) with Normal(N+2) */
struct ut_fs_test_sensor_cfg sensor_cfg_04[] = {
	{
		.sensor_name = "s5k3m5sx",
		.sensor_idx = 0,
		.tg = 2,
		.sensor = &s5k3m5sx,
		.mode = s5k3m5sx_sensor_mode,
		.mode_idx = 0,
		.first_vts_value = 100000,
		//.first_vts_value = 110000,
	},

	{
		.sensor_name = "imx586",
		.sensor_idx = 2,
		.tg = 1,
		.sensor = &imx586,
		.mode = imx586_sensor_mode,
		.mode_idx = 0,
		.first_vts_value = 110000,
		//.first_vts_value = 100000,
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
struct ut_fs_test_sensor_cfg sensor_cfg_05[] = {
	{
		.sensor_name = "imx766",
		.sensor_idx = 0,
		.tg = 2,
		.sensor = &imx766,
		.mode = imx766_sensor_mode,
		.mode_idx = 0,
		//.first_vts_value = 100000,
		.first_vts_value = 105000,
	},

	{
		.sensor_name = "s5k3m5sx",
		.sensor_idx = 2,
		.tg = 1,
		.sensor = &s5k3m5sx,
		.mode = s5k3m5sx_sensor_mode,
		.mode_idx = 0,
		//.first_vts_value = 110000,
		.first_vts_value = 100000,
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
struct ut_fs_test_sensor_cfg sensor_cfg_06[] = {
	{
		.sensor_name = "imx766",
		.sensor_idx = 0,
		.tg = 2,
		.sensor = &imx766,
		.mode = imx766_sensor_mode,
		.mode_idx = 1,
		//.first_vts_value = 100000,
		.first_vts_value = 110000,
	},

	{
		.sensor_name = "s5k3m5sx",
		.sensor_idx = 2,
		.tg = 1,
		.sensor = &s5k3m5sx,
		.mode = s5k3m5sx_sensor_mode,
		.mode_idx = 0,
		//.first_vts_value = 110000,
		.first_vts_value = 100000,
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
//    n_1_mode_cfg
/******************************************************************************/
/* N:1 mode cofig */
struct ut_fs_test_n_1_mode_cfg n_1_cfg_2_1_60_main_0[] = {
	{
		.sensor_idx = 0,
		.n = 2,
		.on_min_fl_us = 16675,
		.off_min_fl_us = 33350,
		.turn_on_at_n_run = 1,
		.turn_off_at_n_run = 10000,
	},

	/* END */
	{
		.sensor_idx = 0,
		.n = 0,
		.on_min_fl_us = 0,
		.off_min_fl_us = 0,
		.turn_on_at_n_run = 0,  /* 0 for end */
		.turn_off_at_n_run = 0, /* 0 for end */
	}
};


struct ut_fs_test_n_1_mode_cfg n_1_cfg_2_2_60_main_0_1[] = {
	{
		.sensor_idx = 0,
		.n = 2,
		.on_min_fl_us = 16675,
		.off_min_fl_us = 33350,
		.turn_on_at_n_run = 1,
		.turn_off_at_n_run = 10000,
	},

	{
		.sensor_idx = 2,
		.n = 2,
		.on_min_fl_us = 16675,
		.off_min_fl_us = 33350,
		.turn_on_at_n_run = 1,
		.turn_off_at_n_run = 10000,
	},

	/* END */
	{
		.sensor_idx = 0,
		.n = 0,
		.on_min_fl_us = 0,
		.off_min_fl_us = 0,
		.turn_on_at_n_run = 0,  /* 0 for end */
		.turn_off_at_n_run = 0, /* 0 for end */
	}
};
/******************************************************************************/





/******************************************************************************/
// for ut execute test item
//    ext_ctrl_cfg
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
/******************************************************************************/





/******************************************************************************/
// for ut execute test item
//    env_cfg
/******************************************************************************/
/* perframe_ctrl trigger with no lock_exp and no lock_flk */
struct ut_fs_test_env_cfg env_cfg_01 = {
	.run_times = 10000,
	.sync_th = 1000, // 550,
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
	.lock_exp_table_idx = 3, // 40005 us
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
	.lock_exp_table_idx = 3, // 40005 us
	.lock_flk = 0,
};


/* perframe_ctrl trigger with lock_exp and no lock_flk */
struct ut_fs_test_env_cfg env_cfg_05 = {
	.run_times = 10000,
	.sync_th = 550,
	.passed_vsync = 0,
	.passed_vsync_ratio = 0,
	.passed_vsync_max_cnt = 0,
	.lock_exp = 1,
	.lock_exp_table_idx = 0, // 10002 us
	.lock_flk = 0,
};


/* perframe_ctrl trigger with lock_exp and no lock_flk */
struct ut_fs_test_env_cfg env_cfg_06 = {
	.run_times = 10000,
	.sync_th = 550,
	.passed_vsync = 0,
	.passed_vsync_ratio = 0,
	.passed_vsync_max_cnt = 0,
	.lock_exp = 1,
	.lock_exp_table_idx = 1, // 19997 us
	.lock_flk = 0,
};


/* perframe_ctrl trigger with lock_exp and no lock_flk */
struct ut_fs_test_env_cfg env_cfg_06_2 = {
	.run_times = 10000,
	.sync_th = 550,
	.passed_vsync = 0,
	.passed_vsync_ratio = 0,
	.passed_vsync_max_cnt = 0,
	.lock_exp = 1,
	.lock_exp_table_idx = 2, // 29996 us
	.lock_flk = 0,
};


/* perframe_ctrl trigger with lock_exp and no lock_flk */
/* sync_th increase to 700 for 60:30/N+1:N+2 flk diff */
struct ut_fs_test_env_cfg env_cfg_07 = {
	.run_times = 10000,
	.sync_th = 700, // 550,
	.passed_vsync = 0,
	.passed_vsync_ratio = 0,
	.passed_vsync_max_cnt = 0,
	.lock_exp = 1,
	.lock_exp_table_idx = 0, // 10002 us
	.lock_flk = 0,
};


/* perframe_ctrl trigger with lock_exp and no lock_flk */
/* sync_th increase to 1000 for 60:60/N+2:N+1 flk diff */
struct ut_fs_test_env_cfg env_cfg_08 = {
	.run_times = 10000,
	.sync_th = 1000, // 550,
	.passed_vsync = 0,
	.passed_vsync_ratio = 0,
	.passed_vsync_max_cnt = 0,
	.lock_exp = 1,
	.lock_exp_table_idx = 0, // 10002 us
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
		.test_name =
			"Normal N+1 / Normal N+1, per-frame CTRL (NO lock exp / NO lock flk)",
		.auto_test_must_run = 1,
		.sync_tag = 1,
		.sensor_cfg = sensor_cfg_01,
		.env_cfg = &env_cfg_01,
	},

	{
		.test_name =
			"Normal N+2 / Normal N+2, per-frame CTRL (NO lock exp / NO lock flk) (same margin)",
		.auto_test_must_run = 1,
		.sync_tag = 1,
		.sensor_cfg = sensor_cfg_02,
		.env_cfg = &env_cfg_01,
	},

	{
		.test_name =
			"Normal N+2 / Normal N+1, per-frame CTRL (NO lock exp / NO lock flk)",
		.auto_test_must_run = 1,
		.sync_tag = 1,
		.sensor_cfg = sensor_cfg_03,
		.env_cfg = &env_cfg_01,
	},

	{
		.test_name =
			"STG 3-exp-LE N+2 / Normal N+1, per-frame CTRL (NO lock exp / NO lock flk)",
		.auto_test_must_run = 1,
		.sync_tag = 10,
		.sensor_cfg = sensor_cfg_05,
		.env_cfg = &env_cfg_01,
	},

	{
		.test_name =
#if !defined(FORCE_ADJUST_SMALLER_DIFF)
			"STG 3-exp-SE N+2 / Normal N+1, per-frame CTRL (LOCK exp-40005us / NO lock flk)",
#else
			"STG 3-exp-SE N+2 / Normal N+1, per-frame CTRL (LOCK exp-10002us / NO lock flk)",
#endif // FORCE_ADJUST_SMALLER_DIFF
		.auto_test_must_run = 1,
		.sync_tag = 18,
		.sensor_cfg = sensor_cfg_05,
#if !defined(FORCE_ADJUST_SMALLER_DIFF)
		.env_cfg = &env_cfg_04,
#else
		.env_cfg = &env_cfg_05,
#endif // FORCE_ADJUST_SMALLER_DIFF
	},

	{
		.test_name =
			"STG 2-exp-LE N+2 / Normal N+1, per-frame CTRL (NO lock exp / NO lock flk)",
		.auto_test_must_run = 1,
		.sync_tag = 10,
		.sensor_cfg = sensor_cfg_06,
		.env_cfg = &env_cfg_01,
	},

	{
		.test_name =
			"STG 2-exp-SE N+2 / Normal N+1, per-frame CTRL (LOCK exp-40005us / NO lock flk)",
		.auto_test_must_run = 1,
		.sync_tag = 18,
		.sensor_cfg = sensor_cfg_06,
		.env_cfg = &env_cfg_04,
	},

	{
		.test_name =
			"60:30 Normal N+1 / Normal N+1, per-frame CTRL (LOCK exp-10002us / NO lock flk), SA",
		.auto_test_must_run = 1,
		.alg_method = 2,
		.sync_tag = 1,
		.sensor_cfg = sensor_cfg_01,
		.env_cfg = &env_cfg_05,
		.n_1_cfg = n_1_cfg_2_1_60_main_0,
	},

	{
		.test_name =
			"60:30 Normal N+2 / Normal N+2, per-frame CTRL (LOCK exp-19997us / NO lock flk), SA (same margin)",
		.auto_test_must_run = 1,
		.alg_method = 2,
		.sync_tag = 1,
		.sensor_cfg = sensor_cfg_02,
		.env_cfg = &env_cfg_06,
		.n_1_cfg = n_1_cfg_2_1_60_main_0,
	},

	{
		.test_name =
			"60:30 Normal N+2 / Normal N+1, per-frame CTRL (LOCK exp-19997us / NO lock flk), SA",
		.auto_test_must_run = 1,
		.alg_method = 2,
		.sync_tag = 1,
		.sensor_cfg = sensor_cfg_03,
		.env_cfg = &env_cfg_06,
		.n_1_cfg = n_1_cfg_2_1_60_main_0,
	},

	{
		.test_name =
#if !defined(FORCE_ADJUST_SMALLER_DIFF)
			"60:30 Normal N+1 / Normal N+2, per-frame CTRL (LOCK exp-10002us / NO lock flk), SA",
#else
			"60:30 Normal N+1 / Normal N+2, per-frame CTRL (LOCK exp-29996us / NO lock flk), SA",
#endif // FORCE_ADJUST_SMALLER_DIFF
		.auto_test_must_run = 1,
		.alg_method = 2,
		.sync_tag = 1,
		.sensor_cfg = sensor_cfg_04,
#if !defined(FORCE_ADJUST_SMALLER_DIFF)
		.env_cfg = &env_cfg_07,
#else
		.env_cfg = &env_cfg_06_2,
#endif // FORCE_ADJUST_SMALLER_DIFF
		.n_1_cfg = n_1_cfg_2_1_60_main_0,
	},

	{
		.test_name =
			"60:60 Normal N+1 / Normal N+1, per-frame CTRL (LOCK exp-10002us / NO lock flk), SA",
		.auto_test_must_run = 1,
		.alg_method = 2,
		.sync_tag = 1,
		.sensor_cfg = sensor_cfg_01,
		.env_cfg = &env_cfg_05,
		.n_1_cfg = n_1_cfg_2_2_60_main_0_1,
	},

	{
		.test_name =
			"60:60 Normal N+2 / Normal N+2, per-frame CTRL (LOCK exp-19997us / NO lock flk), SA (same margin)",
		.auto_test_must_run = 1,
		.alg_method = 2,
		.sync_tag = 1,
		.sensor_cfg = sensor_cfg_02,
		.env_cfg = &env_cfg_06,
		.n_1_cfg = n_1_cfg_2_2_60_main_0_1,
	},

	{
		.test_name =
#if !defined(FORCE_ADJUST_SMALLER_DIFF)
			"60:60 Normal N+2 / Normal N+1, per-frame CTRL (LOCK exp-19997us / NO lock flk), SA",
#else
			"60:60 Normal N+2 / Normal N+1, per-frame CTRL (LOCK exp-10002us / NO lock flk), SA",
#endif // FORCE_ADJUST_SMALLER_DIFF
		.auto_test_must_run = 1,
		.alg_method = 2,
		.sync_tag = 1,
		.sensor_cfg = sensor_cfg_03,
#if !defined(FORCE_ADJUST_SMALLER_DIFF)
		.env_cfg = &env_cfg_08,
#else
		.env_cfg = &env_cfg_04,
#endif // FORCE_ADJUST_SMALLER_DIFF
		.n_1_cfg = n_1_cfg_2_2_60_main_0_1,
	},

	{
		.test_name =
#if !defined(FORCE_ADJUST_SMALLER_DIFF)
			"60:60 Normal N+1 / Normal N+2, per-frame CTRL (LOCK exp-10002us / NO lock flk), SA",
#else
			"60:60 Normal N+1 / Normal N+2, per-frame CTRL (LOCK exp-29996us / NO lock flk), SA",
#endif // FORCE_ADJUST_SMALLER_DIFF
		.auto_test_must_run = 1,
		.alg_method = 2,
		.sync_tag = 1,
		.sensor_cfg = sensor_cfg_04,
#if !defined(FORCE_ADJUST_SMALLER_DIFF)
		.env_cfg = &env_cfg_07,
#else
		.env_cfg = &env_cfg_06_2,
#endif // FORCE_ADJUST_SMALLER_DIFF
		.n_1_cfg = n_1_cfg_2_2_60_main_0_1,
	},

	/* NOT per-frame CTRL case (auto test not must run) */
	{
		.test_name =
			"Normal N+1 / Normal N+1, NOT per-frame CTRL (NO lock exp / NO lock flk)",
		.sync_tag = 1,
		.sensor_cfg = sensor_cfg_01,
		.env_cfg = &env_cfg_02,
	},

	{
		.test_name =
			"Normal N+2 / Normal N+2, NOT per-frame CTRL (NO lock exp / NO lock flk) (same margin)",
		.sync_tag = 1,
		.sensor_cfg = sensor_cfg_02,
		.env_cfg = &env_cfg_02,
	},

	{
		.test_name =
			"Normal N+2 / Normal N+1, NOT per-frame CTRL (LOCK exp / NO lock flk)",
		.sync_tag = 1,
		.sensor_cfg = sensor_cfg_03,
		.env_cfg = &env_cfg_03,
	},

	/* 60:30 NO lock exp case (auto test not must run) */
	{
		.test_name =
			"60:30 Normal N+1 / Normal N+1, per-frame CTRL (NO lock exp / NO lock flk)",
		.alg_method = 2,
		.sync_tag = 1,
		.sensor_cfg = sensor_cfg_01,
		.env_cfg = &env_cfg_01,
		.n_1_cfg = n_1_cfg_2_1_60_main_0,
	},

	{
		.test_name =
			"60:30 Normal N+2 / Normal N+2, per-frame CTRL (NO lock exp / NO lock flk) (same margin)",
		.alg_method = 2,
		.sync_tag = 1,
		.sensor_cfg = sensor_cfg_02,
		.env_cfg = &env_cfg_01,
		.n_1_cfg = n_1_cfg_2_1_60_main_0,
	},

	{
		.test_name =
			"60:30 Normal N+2 / Normal N+1, per-frame CTRL (NO lock exp / NO lock flk)",
		.alg_method = 2,
		.sync_tag = 1,
		.sensor_cfg = sensor_cfg_03,
		.env_cfg = &env_cfg_01,
		.n_1_cfg = n_1_cfg_2_1_60_main_0,
	},

	/* EXT CTRL case (auto test not must run) */
	{
		.test_name =
			"EXT CTRL: STG-3-exp-LE N+2 / Normal N+1, per-frame CTRL (NO lock exp / NO lock flk)",
		.sync_tag = 10,
		.sensor_cfg = sensor_cfg_05,
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
