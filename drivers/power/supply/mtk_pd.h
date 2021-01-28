/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_PD_H
#define __MTK_PD_H

#include "mtk_charger_algorithm_class.h"


#define V_CHARGER_MIN 4600000 /* 4.6 V */

/* pd */
#define PD_VBUS_UPPER_BOUND 10000000	/* uv */
#define PD_VBUS_LOW_BOUND 5000000	/* uv */
#define PD_FAIL_CURRENT			500000	/* 500mA */

/* dual charger */
#define SLAVE_MIVR_DIFF 100000

#define VSYS_WATT 5000000
#define IBUS_ERR 14

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
	PD_STOP,
	PD_RUN,
	PD_DONE
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


	int cv;
	int pd_input_current;
	int pd_charging_current;
	int input_current_limit;
	int charging_current_limit;
	int input_current;
	int charging_current;

	/* dtsi setting */
	int vbus_l;
	int vbus_h;
	int vsys_watt;
	int ibus_err;
	int slave_mivr_diff;
	int min_charger_voltage;

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


#endif /* __MTK_PD_H */

