/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_PE4_H
#define __MTK_PE4_H

#include "mtk_charger_algorithm_class.h"


#define V_CHARGER_MIN 4600000 /* 4.6 V */

/* pe4 */
#define PE40_MAX_VBUS 11000
#define PE40_MAX_IBUS 3000
#define HIGH_TEMP_TO_LEAVE_PE40 46
#define HIGH_TEMP_TO_ENTER_PE40 39
#define LOW_TEMP_TO_LEAVE_PE40 10
#define LOW_TEMP_TO_ENTER_PE40 10
#define IBUS_ERR 14

/*dual charger */
#define PE4_SLAVE_MIVR_DIFF 100000

#define DISABLE_VBAT_THRESHOLD -1

#define PE4_ERROR_LEVEL	1
#define PE4_INFO_LEVEL	2
#define PE4_DEBUG_LEVEL	3

extern int pe4_get_debug_level(void);
#define pe4_err(fmt, args...)					\
do {								\
	if (pe4_get_debug_level() >= PE4_ERROR_LEVEL) {	\
		pr_notice(fmt, ##args);				\
	}							\
} while (0)

#define pe4_info(fmt, args...)					\
do {								\
	if (pe4_get_debug_level() >= PE4_INFO_LEVEL) { \
		pr_notice(fmt, ##args);				\
	}							\
} while (0)

#define pe4_dbg(fmt, args...)					\
do {								\
	if (pe4_get_debug_level() >= PE4_DEBUG_LEVEL) {	\
		pr_notice(fmt, ##args);				\
	}							\
} while (0)

#define PD_CAP_MAX_NR 10

struct pe4_pps_status {
	int output_mv;	/* 0xffff means no support */
	int output_ma;	/* 0xff means no support */
	uint8_t real_time_flags;
};

enum adapter_pe4_return_value {
	MTK_ADAPTER_PE4_OK = 0,
	MTK_ADAPTER_PE4_NOT_SUPPORT,
	MTK_ADAPTER_PE4_TIMEOUT,
	MTK_ADAPTER_PE4_REJECT,
	MTK_ADAPTER_PE4_ERROR,
	MTK_ADAPTER_PE4_ADJUST,
};

struct pe4_power_cap {
	uint8_t selected_cap_idx;
	uint8_t nr;
	uint8_t pdp;
	uint8_t pwr_limit[PD_CAP_MAX_NR];
	int max_mv[PD_CAP_MAX_NR];
	int min_mv[PD_CAP_MAX_NR];
	int ma[PD_CAP_MAX_NR];
	int maxwatt[PD_CAP_MAX_NR];
	int minwatt[PD_CAP_MAX_NR];
	uint8_t type[PD_CAP_MAX_NR];
	int info[PD_CAP_MAX_NR];
};

enum pe4_state_enum {
	PE4_HW_UNINIT = 0,
	PE4_HW_FAIL,
	PE4_HW_READY,
	PE4_TA_NOT_SUPPORT,
	PE4_INIT,
	PE4_RUN,
	PE4_TUNING,
	PE4_POSTCC,
};

struct pe4_adapter_status {
	int temperature;
	bool ocp;
	bool otp;
	bool ovp;
};

struct mtk_pe40 {
	struct platform_device *pdev;
	struct chg_alg_device *alg;
	struct mutex access_lock;
	struct mutex data_lock;
	struct wakeup_source *suspend_lock;

	bool can_query;
	int state;
	struct pe4_power_cap cap;
	struct power_supply *bat_psy;

	int avbus;
	int vbus;
	int ibus;
	int watt;
	int vbat_threshold; /* For checking Ready */
	int ref_vbat; /* Vbat with cable in */

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
	/* limitation by cable*/
	int pe4_input_current_limit;
	int pe4_input_current_limit_setting;

	/* module parameters */
	int cv;
	int old_cv;
	int pe4_6pin_en;
	int stop_6pin_re_en;
	int input_current_limit1;
	int input_current_limit2;
	int charging_current_limit1;
	int charging_current_limit2;

	/* Current setting value */
	int charger_current1;
	int charger_current2;
	int input_current1;
	int input_current2;

	int polling_interval;

	/* dtsi */
	/* single charger */
	int sc_charger_current;
	int sc_input_current;
	/* dual charger in series */
	int dcs_input_current;
	int dcs_chg1_charger_current;
	int dcs_chg2_charger_current;

	int min_charger_voltage;
	int pe40_max_vbus;
	int pe40_max_ibus;
	int pe40_stop_battery_soc;
	int high_temp_to_leave_pe40;
	int high_temp_to_enter_pe40;
	int low_temp_to_leave_pe40;
	int low_temp_to_enter_pe40;
	int ibus_err;
	int dual_polling_ieoc;
	int slave_mivr_diff;

	/* pe4.0 cable impedance threshold (mohm) */
	u32 pe40_r_cable_1a_lower;
	u32 pe40_r_cable_2a_lower;
	u32 pe40_r_cable_3a_lower;

};

extern int pe4_hal_init_hardware(struct chg_alg_device *alg);
extern int pe4_hal_enable_vbus_ovp(struct chg_alg_device *alg, bool enable);
extern int pe4_hal_get_uisoc(struct chg_alg_device *alg);
extern int pe4_hal_is_pd_adapter_ready(struct chg_alg_device *alg);
extern int pe4_hal_get_battery_temperature(struct chg_alg_device *alg);
extern int pe4_hal_set_input_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 ua);
extern int pe4_hal_set_charging_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 ua);
extern int pe4_hal_1st_set_adapter_cap(struct chg_alg_device *alg,
	int mV, int mA);
extern int pe4_hal_set_adapter_cap(struct chg_alg_device *alg,
	int mV, int mA);
extern int pe4_hal_set_adapter_cap_end(struct chg_alg_device *alg,
	int mV, int mA);
extern int pe40_hal_get_adapter_status(struct chg_alg_device *alg,
	struct pe4_adapter_status *pe4_sta);
extern int pe4_hal_get_adapter_cap(struct chg_alg_device *alg,
	struct pe4_power_cap *cap);
extern int pe4_hal_get_input_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 *ua);
extern int pe4_hal_enable_powerpath(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool enable);
extern int pe4_hal_force_disable_powerpath(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool disable);
extern int pe4_hal_get_charger_cnt(struct chg_alg_device *alg);
extern bool pe4_hal_is_chip_enable(struct chg_alg_device *alg,
	enum chg_idx chgidx);
extern int pe4_hal_enable_charger(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool en);
extern int pe40_hal_get_adapter_output(struct chg_alg_device *alg,
	struct pe4_pps_status *pe4_status);
extern int pe4_hal_get_vbus(struct chg_alg_device *alg);
extern int pe4_hal_get_vbat(struct chg_alg_device *alg);
extern int pe4_hal_get_ibus(struct chg_alg_device *alg, int *ibus);
extern int pe4_hal_get_ibat(struct chg_alg_device *alg);
extern int pe4_hal_dump_registers(struct chg_alg_device *alg);
extern int pe4_hal_set_mivr(struct chg_alg_device *alg,
	enum chg_idx chgidx, int uV);
extern int pe4_hal_get_mivr(struct chg_alg_device *alg,
	enum chg_idx chgidx, int *mivr1);
extern int pe4_hal_get_mivr_state(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool *in_loop);
extern int pe4_hal_charger_enable_chip(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool enable);
extern int pe4_hal_is_charger_enable(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool *en);
extern int pe4_hal_get_charging_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 *ua);
extern int pe4_hal_get_min_charging_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 *uA);
extern int pe4_hal_set_eoc_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 uA);
extern int pe4_hal_get_min_input_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 *uA);
extern int pe4_hal_safety_check(struct chg_alg_device *alg,
	int ieoc);
extern int pe4_hal_vbat_mon_en(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool en);
extern int pe4_hal_set_cv(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 uv);
extern int pe4_hal_set_cv(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 uv);
extern int pe4_hal_enable_termination(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool enable);
extern int pe4_hal_reset_eoc_state(struct chg_alg_device *alg);
extern int pe4_hal_get_log_level(struct chg_alg_device *alg);
#endif /* __MTK_PE4_H */
