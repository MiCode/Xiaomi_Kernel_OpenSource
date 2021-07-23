/*
 * Copyright (C) 2017 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MTK_BATTERY_RECOVERY_H__
#define __MTK_BATTERY_RECOVERY_H__

void fg_construct_table_by_temp(bool update, int table_idx);
void fg_construct_vboot(int table_idx);

/* other API may use */
void imix_error_calibration(void);

/* CSOC related */
void fg_update_c_dod(void);

/* communication function */
void wakeup_fg_algo_recovery(unsigned int intr_num);
void fgr_SEND_to_kernel(int cmd, int *send_data, int *recive_data);

/* get data API */
int get_fg_hw_car(void);
int get_rtc_ui_soc(void);
int get_ptimrac(void);
int get_ptim_vbat(void);
int get_ptim_i(void);
int get_hw_info(int intr_no);
unsigned int get_vbat(void);
int get_charger_exist(void);
int get_charger_status(void);
int get_imix_r(void);
int get_con0_soc(void);
int get_d0_c_soc_cust(void);
int get_uisoc_cust(void);

/* set data to kernel*/
int fg_adc_reset(void);
void set_fg_bat_int1_gap(int gap);
void set_fg_bat_int2_ht_gap(int gap); /* ui ht */
void set_fg_bat_int2_lt_gap(int gap); /* ui lt */
void enable_fg_bat_int2_ht(int en);	/* ui ht interrupt */
void enable_fg_bat_int2_lt(int en);	/* ui lt interrupt */
int set_kernel_soc(int _soc);
int set_kernel_uisoc(int _uisoc);
int set_kernel_init_vbat(int _vbat);
void set_fg_bat_tmp_c_gap(int tmp);	/* set c temperture gap */
void set_init_flow_done(int flag);
void set_rtc_ui_soc(int rtc_ui_soc);
void set_con0_soc(int rtc_soc);
void set_fg_time(int _time);
void set_enter_recovery(int flag);
void enable_fg_vbat2_h_int(int en);
void enable_fg_vbat2_l_int(int en);
void set_fg_vbat2_h_th(int thr);
void set_fg_vbat2_l_th(int thr);

/* interrupt handler */
void fg_set_int1(void);	/* Initialize */
void fg_bat_int1_handler(void);	/* c_soc */
void fg_bat_int2_handler(int source);	/* UI_soc */
void fg_int_end_flow(unsigned int intr_no);	/* regular flow */
void fg_temp_c_int_handler(void);
void fgr_bat_int2_h_handler(void);
void fgr_bat_int2_l_handler(void);
void fg_time_handler(void);
void fgr_shutdown_int_handler(void);
void dlpt_sd_handler(void);
void fgr_vbat2_h_int_handler(void);
void fgr_vbat2_l_int_handler(void);

/* construct table */
void fg_construct_table_by_temp(bool update, int table_idx);
void fg_construct_vboot(int table_idx);

/* other API may use */
void imix_error_calibration(void);

/* CSOC related */
void fg_update_c_dod(void);

#endif /* __MTK_BATTERY_RECOVERY_H__ */
