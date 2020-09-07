/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_PE_INTF_H
#define __MTK_PE_INTF_H

#include "mtk_charger_algorithm_class.h"

#define TA_AC_12V_INPUT_CURRENT 3200000
#define TA_AC_9V_INPUT_CURRENT	3200000
#define TA_AC_7V_INPUT_CURRENT	3200000
#define PE_ICHG_LEAVE_THRESHOLD 1000000 /* uA */
#define TA_START_BATTERY_SOC	0
#define TA_STOP_BATTERY_SOC	85
#define PE_V_CHARGER_MIN 4600000 /* 4.6 V */

#define PE_INPUT_CURRENT		3200000
#define PE_CHARGING_CURRENT	3000000


/*dual charger */
#define PE_SLAVE_MIVR_DIFF 100000

#define ECABLEOUT	1	/* cable out */
#define EHAL		2	/* hal operation error */

#define PE_ERROR_LEVEL	1
#define PE_INFO_LEVEL	2
#define PE_DEBUG_LEVEL	3

extern int pe_get_debug_level(void);
#define pe_err(fmt, args...)					\
do {								\
	if (pe_get_debug_level() >= PE_ERROR_LEVEL) {	\
		pr_notice(fmt, ##args);				\
	}							\
} while (0)

#define pe_info(fmt, args...)					\
do {								\
	if (pe_get_debug_level() >= PE_INFO_LEVEL) { \
		pr_notice(fmt, ##args);				\
	}							\
} while (0)

#define pe_dbg(fmt, args...)					\
do {								\
	if (pe_get_debug_level() >= PE_DEBUG_LEVEL) {	\
		pr_notice(fmt, ##args);				\
	}							\
} while (0)


enum pe_state_enum {
	PE_HW_UNINIT = 0,
	PE_HW_FAIL,
	PE_HW_READY,
	PE_TA_NOT_SUPPORT,
	PE_RUN,
	PE_DONE
};

struct mtk_pe {
	struct platform_device *pdev;
	struct chg_alg_device *alg;
	struct mutex access_lock;
	struct mutex cable_out_lock;
	struct wakeup_source *suspend_lock;
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
	int ta_ac_charger_current;

	bool ta_12v_support;
	bool ta_9v_support;

	int cv;
	int input_current_limit;
	int charging_current_limit;
	int input_current;
	int charging_current;

/* dual charger */
	int pe_slave_mivr_diff;


};

extern int pe_hal_init_hardware(struct chg_alg_device *alg);
extern int pe_hal_get_vbus(struct chg_alg_device *alg);
extern int pe_hal_get_ibat(struct chg_alg_device *alg);
extern int pe_hal_get_charging_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 *ua);
extern int pe_hal_enable_vbus_ovp(struct chg_alg_device *alg,
	bool enable);
extern int pe_hal_enable_charging(struct chg_alg_device *alg,
	bool enable);
extern int pe_hal_set_mivr(struct chg_alg_device *alg,
	enum chg_idx chgidx, int uV);
extern int pe_hal_get_uisoc(struct chg_alg_device *alg);
extern int pe_hal_get_charger_type(struct chg_alg_device *alg);
extern int pe_hal_send_ta_current_pattern(struct chg_alg_device *alg,
					  bool increase);
extern int pe_hal_get_charger_cnt(struct chg_alg_device *alg);
extern bool pe_hal_is_chip_enable(struct chg_alg_device *alg,
	enum chg_idx chgidx);
extern int pe_hal_enable_chip(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool en);
extern int pe_hal_reset_ta(struct chg_alg_device *alg,
	enum chg_idx chgidx);
extern int pe_hal_set_cv(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 uv);
extern int pe_hal_set_charging_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 ua);
extern int pe_hal_set_input_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 ua);



#endif /* __MTK_PE_INTF_H */
