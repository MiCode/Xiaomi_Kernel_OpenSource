/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_PE2_H
#define __MTK_PE2_H

#include "mtk_charger_algorithm_class.h"

/* pe2.0 */
#define PE20_ICHG_LEAVE_THRESHOLD 1000000 /* uA */
#define TA_START_BATTERY_SOC	0
#define TA_STOP_BATTERY_SOC	85
#define PE20_V_CHARGER_MIN 4600000 /* 4.6 V */

#define PE2_INPUT_CURRENT		3200000
#define PE2_CHARGING_CURRENT	3000000


/*dual charger */
#define PE2_SLAVE_MIVR_DIFF 100000


/* cable measurement impedance */
#define PE2_CABLE_IMP_THRESHOLD 699
#define PE2_VBAT_CABLE_IMP_THRESHOLD 3900000 /* uV */


#define ECABLEOUT	1	/* cable out */
#define EHAL		2	/* hal operation error */


#define PE2_ERROR_LEVEL	1
#define PE2_INFO_LEVEL	2
#define PE2_DEBUG_LEVEL	3

extern int pe2_get_debug_level(void);
#define pe2_err(fmt, args...)					\
do {								\
	if (pe2_get_debug_level() >= PE2_ERROR_LEVEL) {	\
		pr_notice(fmt, ##args);				\
	}							\
} while (0)

#define pe2_info(fmt, args...)					\
do {								\
	if (pe2_get_debug_level() >= PE2_INFO_LEVEL) { \
		pr_notice(fmt, ##args);			\
	}							\
} while (0)

#define pe2_dbg(fmt, args...)					\
do {								\
	if (pe2_get_debug_level() >= PE2_DEBUG_LEVEL) {	\
		pr_notice(fmt, ##args);				\
	}							\
} while (0)


enum pe2_state_enum {
	PE2_HW_UNINIT = 0,
	PE2_HW_FAIL,
	PE2_HW_READY,
	PE2_TA_NOT_SUPPORT,
	PE2_RUN,
	PE2_DONE
};

struct pe20_profile {
	unsigned int vbat;
	unsigned int vchr;
};

struct mtk_pe20 {
	struct platform_device *pdev;
	struct chg_alg_device *alg;

	struct mutex access_lock;
	struct wakeup_source *suspend_lock;
	struct mutex cable_out_lock;
	bool is_cable_out_occur; /* Plug out happened while detect PE+20 */

	int ta_vchr_org;
	int idx;
	int vbus;
	struct pe20_profile profile[10];

	int vbat_orig; /* Measured VBAT before cable impedance measurement */
	int aicr_cable_imp; /* AICR to set after cable impedance measurement */

	/* pe2.0 */
	int pe20_ichg_level_threshold;	/* ma */
	int ta_start_battery_soc;
	int ta_stop_battery_soc;
	int min_charger_voltage;
	int pe2_input_current;
	int pe2_charger_current;

	/* dual charger */
	int pe2_slave_mivr_diff;

	/* cable measurement impedance */
	int cable_imp_threshold;
	int vbat_cable_imp_threshold;

	int cv;
	int input_current_limit;
	int charging_current_limit;
	int input_current;
	int charging_current;

	enum pe2_state_enum state;

};

extern int pe2_hal_init_hardware(struct chg_alg_device *alg);
extern int pe2_hal_set_efficiency_table(struct chg_alg_device *alg);
extern int pe2_hal_get_uisoc(struct chg_alg_device *alg);
extern int pe2_hal_get_charger_type(struct chg_alg_device *alg);
extern int pe2_hal_set_mivr(struct chg_alg_device *alg,
	enum chg_idx chgidx, int uV);

extern int pe2_hal_get_charger_cnt(struct chg_alg_device *alg);
extern bool pe2_hal_is_chip_enable(struct chg_alg_device *alg,
	enum chg_idx chgidx);
extern int pe2_hal_enable_chip(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool en);
extern int pe2_hal_reset_ta(struct chg_alg_device *alg, enum chg_idx chgidx);
extern int pe2_hal_get_vbus(struct chg_alg_device *alg);
extern int pe2_hal_get_vbat(struct chg_alg_device *alg);
extern int pe2_hal_get_ibat(struct chg_alg_device *alg);
extern int pe2_hal_enable_cable_drop_comp(struct chg_alg_device *alg,
	bool en);
extern int pe2_hal_set_charging_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 ua);
extern int pe2_hal_set_input_current(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 ua);
extern int pe2_hal_get_mivr_state(struct chg_alg_device *alg,
	enum chg_idx chgidx, bool *in_loop);
extern int pe2_hal_send_ta20_current_pattern(struct chg_alg_device *alg,
					  u32 uV);
extern int pe2_hal_enable_vbus_ovp(struct chg_alg_device *alg, bool enable);
extern int pe2_hal_set_cv(struct chg_alg_device *alg,
	enum chg_idx chgidx, u32 uv);

#endif /* __MTK_PE2_H */

