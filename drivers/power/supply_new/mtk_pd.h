/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_PD_H
#define __MTK_PD_H

#include "mtk_charger_algorithm_class.h"


#define V_CHARGER_MIN 4600000 /* 4.6 V */

/* pd */
#define PD_VBUS_UPPER_BOUND		10000000	/* uv */
#define PD_VBUS_LOW_BOUND		5000000	/* uv */
#define PD_FAIL_CURRENT			500000	/* 500mA */

#define PD_SC_INPUT_CURRENT		3000000	/* 3000mA */
#define PD_SC_CHARGER_CURRENT	3000000	/* 3000mA */

/* dual charger in series */
#define PD_DCS_INPUT_CURRENT 3200000
#define PD_DCS_CHG1_CHARGER_CURRENT 1500000
#define PD_DCS_CHG2_CHARGER_CURRENT 1500000

#define PD_STOP_BATTERY_SOC 80


/* dual charger */
#define SLAVE_MIVR_DIFF 100000

#define VSYS_WATT 5000000
#define IBUS_ERR 14

#define DISABLE_VBAT_THRESHOLD -1

#define PD_ERROR_LEVEL	1
#define PD_INFO_LEVEL	2
#define PD_DEBUG_LEVEL	3

extern int pd_get_debug_level(void);
#define pd_err(fmt, args...)					\
do {								\
	if (pd_get_debug_level() >= PD_ERROR_LEVEL) {	\
		pr_notice(fmt, ##args);				\
	}							\
} while (0)

#define pd_info(fmt, args...)					\
do {								\
	if (pd_get_debug_level() >= PD_INFO_LEVEL) { \
		pr_notice(fmt, ##args);				\
	}							\
} while (0)

#define pd_dbg(fmt, args...)					\
do {								\
	if (pd_get_debug_level() >= PD_DEBUG_LEVEL) {	\
		pr_notice(fmt, ##args);				\
	}							\
} while (0)

enum pd_state_enum {
	PD_HW_UNINIT = 0,
	PD_HW_FAIL,
	PD_HW_READY,
	PD_TA_NOT_SUPPORT,
	PD_RUN,
	PD_TUNING,
	PD_POSTCC,
};

#define PD_CAP_MAX_NR 10

struct pd_power_cap {
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

struct mtk_pd {
	struct platform_device *pdev;
	struct chg_alg_device *alg;
	int state;
	struct mutex access_lock;
	struct mutex data_lock;
	struct power_supply *bat_psy;

	int vbat_threshold; /* For checking Ready */
	int ref_vbat; /* Vbat with cable in */
	int cv;
	int old_cv;
	int pd_6pin_en;
	int stop_6pin_re_en;
	int pd_input_current;
	int pd_charging_current;

	int input_current_limit1;
	int input_current_limit2;
	int charging_current_limit1;
	int charging_current_limit2;

	int input_current1;
	int input_current2;
	int charging_current1;
	int charging_current2;

	/* dtsi setting */
	int vbus_l;
	int vbus_h;
	int vsys_watt;
	int ibus_err;
	int slave_mivr_diff;
	int min_charger_voltage;
	int pd_stop_battery_soc;

	/* single charger dtsi setting*/
	int sc_input_current;
	int sc_charger_current;

	/* dual charger in series dtsi setting*/
	int dcs_input_current;
	int dcs_chg1_charger_current;
	int dcs_chg2_charger_current;

	int dual_polling_ieoc;

	struct pd_power_cap cap;
	int pdc_max_watt;
	int pdc_max_watt_setting;

	bool check_impedance;
	int pd_cap_max_watt;
	int pd_idx;
	int pd_reset_idx;
	int pd_boost_idx;
	int pd_buck_idx;

	int ta_vchr_org;
	bool to_check_chr_type;
	bool to_tune_ta_vchr;
	bool is_cable_out_occur;
	bool is_connect;
	bool is_enabled;


};

extern int pd_hal_init_hardware(struct chg_alg_device *alg);
extern int pd_hal_is_pd_adapter_ready(struct chg_alg_device *alg);
extern int pd_hal_get_adapter_cap(struct chg_alg_device *alg,
	struct pd_power_cap *cap);
extern int pd_hal_get_vbus(struct chg_alg_device *alg);
extern int pd_hal_get_ibus(struct chg_alg_device *alg, int *ibus);
extern int pd_hal_get_mivr_state(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool *in_loop);
extern int pd_hal_get_mivr(struct chg_alg_device *alg,
	enum chg_idx chgidx, int *mivr1);
extern int pd_hal_get_charger_cnt(struct chg_alg_device *alg);
extern bool pd_hal_is_chip_enable(struct chg_alg_device *alg,
	enum chg_idx chgidx);
extern int pd_hal_enable_vbus_ovp(struct chg_alg_device *alg, bool enable);
extern int pd_hal_set_mivr(struct chg_alg_device *alg,
	enum chg_idx chgidx, int uV);
extern int pd_hal_get_input_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 *ua);
extern int pd_hal_set_cv(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 uv);
extern int pd_hal_set_charging_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 ua);
extern int pd_hal_set_input_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 ua);
extern int pd_hal_set_adapter_cap(struct chg_alg_device *alg,
	int mV, int mA);
extern int pd_hal_is_charger_enable(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool *en);
extern int pd_hal_get_charging_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 *ua);
extern int pd_hal_get_min_charging_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 *uA);
extern int pd_hal_get_min_input_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 *uA);
extern int pd_hal_safety_check(struct chg_alg_device *alg,
	int ieoc);
extern int pd_hal_vbat_mon_en(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool en);
extern int pd_hal_set_eoc_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 uA);
extern int pd_hal_enable_termination(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool enable);
extern int pd_hal_enable_charger(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool en);
extern int pd_hal_charger_enable_chip(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool enable);
extern int pd_hal_get_uisoc(struct chg_alg_device *alg);
extern int pd_hal_get_log_level(struct chg_alg_device *alg);
#endif /* __MTK_PD_H */
