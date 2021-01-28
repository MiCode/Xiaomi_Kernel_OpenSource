/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_PE_40_H
#define __MTK_PE_40_H

#define ADAPTER_CAP_MAX_NR 10

enum {
	PE40_INIT,
	PE40_CC,
};

struct pps_status {
	int output_mv;	/* 0xffff means no support */
	int output_ma;	/* 0xff means no support */
	uint8_t real_time_flags;
};

struct pps_cap {
	uint8_t selected_cap_idx;
	uint8_t nr;
	uint8_t pdp;
	uint8_t pwr_limit[ADAPTER_CAP_MAX_NR];
	int max_mv[ADAPTER_CAP_MAX_NR];
	int min_mv[ADAPTER_CAP_MAX_NR];
	int ma[ADAPTER_CAP_MAX_NR];
	int maxwatt[ADAPTER_CAP_MAX_NR];
	int minwatt[ADAPTER_CAP_MAX_NR];
	uint8_t type[ADAPTER_CAP_MAX_NR];
	int info[ADAPTER_CAP_MAX_NR];
};

struct ta_status {
	int temperature;
	bool ocp;
	bool otp;
	bool ovp;
};

struct pe40_data {
	int input_current_limit;
	int charging_current_limit;
	int battery_cv;
	int min_charger_voltage;
	int pe40_max_vbus;
	int pe40_max_ibus;
	int ibus_err;
	int high_temp_to_enter_pe40;
	int low_temp_to_enter_pe40;
	int high_temp_to_leave_pe40;
	int low_temp_to_leave_pe40;
	int pe40_r_cable_3a_lower;
	int pe40_r_cable_2a_lower;
	int pe40_r_cable_1a_lower;
};

struct pe40 {
	int state;
	bool is_connect;
	bool can_query;
	struct pps_cap cap;
	struct pe40_data data;

	int avbus;
	int vbus;
	int ibus;
	int watt;

	int r_sw;
	int r_cable;
	int r_cable_1;
	int r_cable_2;

	int pmic_vbus;
	int TA_vbus;
	int vbus_cali;

	int max_charger_ibus;
	int max_vbus;
	int max_ibus;

	int pe4_input_current_limit;			/* CIM */
	int pe4_input_current_limit_setting;	/* TA */
};

extern int pe40_init(void);
extern bool pe40_is_ready(void);
extern int pe40_stop(void);
extern int pe40_run(void);
extern int pe40_set_data(struct pe40_data data);
extern struct pe40_data *pe40_get_data(void);

#endif /* __MTK_PE_40_H */
