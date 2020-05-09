// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include "tpd.h"

unsigned long TPD_RES_X = 480;
unsigned long TPD_RES_Y = 800;

/* #if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION)) */
int tpd_calmat[8] = { 0 };
int tpd_def_calmat[8] = { 0 };

int tpd_calmat_size = 8;
int tpd_def_calmat_size = 8;
module_param_array(tpd_calmat, int, &tpd_calmat_size, 0664);
module_param_array(tpd_def_calmat, int, &tpd_def_calmat_size, 0444);
/* #endif */
/* #ifdef TPD_TYPE_CAPACITIVE */
int tpd_type_cap;

int tpd_v_magnify_x = 10;
int tpd_v_magnify_y = 10;
module_param(tpd_v_magnify_x, int, 0664);
module_param(tpd_v_magnify_y, int, 0664);

module_param(tpd_type_cap, int, 0444);
int tpd_firmware_version[2] = { 0, 0 };

int tpd_firmware_version_size = 2;
module_param_array(tpd_firmware_version, int, &tpd_firmware_version_size, 0444);

int tpd_mode = TPD_MODE_NORMAL;
int tpd_mode_axis;
int tpd_mode_min = 400;		/* TPD_RES_Y/2; */
int tpd_mode_max = 800;		/* TPD_RES_Y; */
/* TPD_RES_X*TPD_RES_X/1600; */
int tpd_mode_keypad_tolerance = 480 * 480 / 1600;
module_param(tpd_mode, int, 0664);
module_param(tpd_mode_axis, int, 0664);
module_param(tpd_mode_min, int, 0664);
module_param(tpd_mode_max, int, 0664);
module_param(tpd_mode_keypad_tolerance, int, 0664);

int tpd_em_debounce_time0 = 1;
int tpd_em_debounce_time;	/* =0 */
int tpd_em_debounce_time1 = 4;
module_param(tpd_em_debounce_time0, int, 0664);
module_param(tpd_em_debounce_time1, int, 0664);
module_param(tpd_em_debounce_time, int, 0664);

int tpd_em_spl_num = 1;
module_param(tpd_em_spl_num, int, 0664);

int tpd_em_pressure_threshold;
module_param(tpd_em_pressure_threshold, int, 0664);

int tpd_em_auto_time_interval = 10;
module_param(tpd_em_auto_time_interval, int, 0664);

int tpd_em_sample_cnt = 16;
module_param(tpd_em_sample_cnt, int, 0664);

int tpd_load_status;
module_param(tpd_load_status, int, 0664);

int tpd_em_asamp = 1;
module_param(tpd_em_asamp, int, 0664);
