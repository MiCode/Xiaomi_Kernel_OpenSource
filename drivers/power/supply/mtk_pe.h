/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_PE_INTF_H
#define __MTK_PE_INTF_H

#define PE_ICHG_LEAVE_THRESHOLD 1000000 /* uA */
#define TA_START_BATTERY_SOC	0
#define TA_STOP_BATTERY_SOC	85
#define PE_V_CHARGER_MIN 4600000 /* 4.6 V */

enum pe_state_enum {
	PE_HW_UNINIT = 0,
	PE_HW_FAIL,
	PE_HW_READY,
	PE_TA_NOT_SUPPORT,
	PE_STOP,
	PE_RUN,
	PE_DONE
};

struct mtk_pe {
	struct mutex access_lock;
	struct mutex cable_out_lock;
	struct wakeup_source suspend_lock;
	int state;

	int ta_vchr_org; /* uA */
	bool to_tune_ta_vchr;
	bool is_cable_out_occur; /* Plug out happened while detecting PE+ */

	int pe_ichg_level_threshold;	/* ma */
	int ta_start_battery_soc;
	int ta_stop_battery_soc;
	int min_charger_voltage;

	int ta_ac_12v_input_current;
	int ta_ac_9v_input_current;
	int ta_ac_7v_input_current;
	bool ta_12v_support;
	bool ta_9v_support;

	int cv;
	int input_current_limit;
	int charging_current_limit;
	int input_current;
	int charging_current;

};




#endif /* __MTK_PE_INTF_H */
