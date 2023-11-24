/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CHARGER_H
#define __MTK_CHARGER_H

#include <linux/alarmtimer.h>
#include "charger_class.h"
#include "adapter_class.h"
#include "mtk_charger_algorithm_class.h"
#include <linux/power_supply.h>
#include "mtk_smartcharging.h"

/*N17 code for screen_state by xm liluting at 2023/07/11 start*/
#include <linux/notifier.h>
#include "../../gpu/drm/mediatek/mediatek_v2/mtk_disp_notify.h"
struct chg_screen_monitor {
       struct notifier_block charger_panel_notifier;
       int screen_state;
};
/*N17 code for screen_state by xm liluting at 2023/07/11 end*/

#define CHARGING_INTERVAL 10
/*N17 code for HQHW-4654 by tongjiacheng at 2023/08/04 start*/
#define CHARGING_FULL_INTERVAL 10
#define CHARGING_100_PERCENT_INTERVAL  4
/*N17 code for HQHW-4654 by tongjiacheng at 2023/08/04 end*/
/*N17 code for HQHW-4728 by yeyinzi at 2023/08/07 start*/
#define CHARGING_ABNORMAL_TEMP_INTERVAL  4
/*N17 code for HQHW-4728 by yeyinzi at 2023/08/07 end*/
#define CHRLOG_ERROR_LEVEL	1
#define CHRLOG_INFO_LEVEL	2
#define CHRLOG_DEBUG_LEVEL	3

#define SC_TAG "smartcharging"

extern int chr_get_debug_level(void);

#define chr_err(fmt, args...)					\
do {								\
	if (chr_get_debug_level() >= CHRLOG_ERROR_LEVEL) {	\
		pr_notice(fmt, ##args);				\
	}							\
} while (0)

#define chr_info(fmt, args...)					\
do {								\
	if (chr_get_debug_level() >= CHRLOG_INFO_LEVEL) {	\
		pr_notice_ratelimited(fmt, ##args);		\
	}							\
} while (0)

#define chr_debug(fmt, args...)					\
do {								\
	if (chr_get_debug_level() >= CHRLOG_DEBUG_LEVEL) {	\
		pr_notice(fmt, ##args);				\
	}							\
} while (0)

struct mtk_charger;
struct charger_data;
#define BATTERY_CV 4350000
#define V_CHARGER_MAX 6500000 /* 6.5 V */
#define V_CHARGER_MIN 4600000 /* 4.6 V */

#define USB_CHARGER_CURRENT_SUSPEND		0 /* def CONFIG_USB_IF */
#define USB_CHARGER_CURRENT_UNCONFIGURED	70000 /* 70mA */
#define USB_CHARGER_CURRENT_CONFIGURED		500000 /* 500mA */
#define USB_CHARGER_CURRENT			500000 /* 500mA */
#define AC_CHARGER_CURRENT			2050000
#define AC_CHARGER_INPUT_CURRENT		3200000
/*N17 code for HQ-290909 by miaozhichao at 2023/5/25 start*/
#define NON_STD_AC_CHARGER_CURRENT		1000000
/*N17 code for HQ-290909 by miaozhichao at 2023/5/25 end*/
#define CHARGING_HOST_CHARGER_CURRENT		650000

/* dynamic mivr */
#define V_CHARGER_MIN_1 4400000 /* 4.4 V */
#define V_CHARGER_MIN_2 4200000 /* 4.2 V */
#define MAX_DMIVR_CHARGER_CURRENT 1800000 /* 1.8 A */

/* battery warning */
#define BATTERY_NOTIFY_CASE_0001_VCHARGER
#define BATTERY_NOTIFY_CASE_0002_VBATTEMP

/* charging abnormal status */
#define CHG_VBUS_OV_STATUS	(1 << 0)
#define CHG_BAT_OT_STATUS	(1 << 1)
#define CHG_OC_STATUS		(1 << 2)
#define CHG_BAT_OV_STATUS	(1 << 3)
#define CHG_ST_TMO_STATUS	(1 << 4)
#define CHG_BAT_LT_STATUS	(1 << 5)
#define CHG_TYPEC_WD_STATUS	(1 << 6)

/* Battery Temperature Protection */
#define MIN_CHARGE_TEMP  0
#define MIN_CHARGE_TEMP_PLUS_X_DEGREE	6
#define MAX_CHARGE_TEMP  50
#define MAX_CHARGE_TEMP_MINUS_X_DEGREE	47

#define MAX_ALG_NO 10

enum bat_temp_state_enum {
	BAT_TEMP_LOW = 0,
	BAT_TEMP_NORMAL,
	BAT_TEMP_HIGH
};

enum chg_dev_notifier_events {
	EVENT_FULL,
	EVENT_RECHARGE,
	EVENT_DISCHARGE,
};

struct battery_thermal_protection_data {
	int sm;
	bool enable_min_charge_temp;
	int min_charge_temp;
	int min_charge_temp_plus_x_degree;
	int max_charge_temp;
	int max_charge_temp_minus_x_degree;
};
/*N17 code for HQ-291115 by miaozhichao at 2023/5/30 start*/
extern void do_sw_jeita_state_machine(struct mtk_charger *info);
/* sw jeita */
#define JEITA_TEMP_ABOVE_T6_CV	4100000
#define JEITA_TEMP_T5_TO_T6_CV	4100000
#define JEITA_TEMP_T4_TO_T5_CV	4480000
#define JEITA_TEMP_T3_TO_T4_CV	4480000
#define JEITA_TEMP_T2_TO_T3_CV	4480000
#define JEITA_TEMP_T1_TO_T2_CV	4480000
#define JEITA_TEMP_T0_TO_T1_CV	4480000
#define JEITA_TEMP_BELOW_T0_CV	4480000

#define JEITA_TEMP_T5_TO_T6_CC	2450000
#define JEITA_TEMP_T4_TO_T5_CC	3600000
#define JEITA_TEMP_T3_TO_T4_CC	3600000
#define JEITA_TEMP_T2_TO_T3_CC	3000000
#define JEITA_TEMP_T1_TO_T2_CC	2450000
#define JEITA_TEMP_T0_TO_T1_CC	980000
#define JEITA_TEMP_BELOW_T0_CC	490000
/*N17 code for HQ-291115 by miaozhichao at 2023/5/30 end*/

/*
 * Software JEITA
 * T0: -10 degree Celsius
 * T1: 0 degree Celsius
 * T2: 10 degree Celsius
 * T3: 45 degree Celsius
 * T4: 50 degree Celsius
 */
enum sw_jeita_state_enum {
	TEMP_BELOW_T0 = 0,
	TEMP_T0_TO_T1,
	TEMP_T1_TO_T2,
	TEMP_T2_TO_T3,
	TEMP_T3_TO_T4,
	TEMP_ABOVE_T4
};

struct sw_jeita_data {
	int sm;
	int pre_sm;
	int cv;
/*N17 code for HQ-291115 by miaozhichao at 2023/5/30 start*/
	int cc;
/*N17 code for HQ-291115 by miaozhichao at 2023/5/30 end*/
	bool charging;
	bool error_recovery_flag;
};

struct mtk_charger_algorithm {

	int (*do_algorithm)(struct mtk_charger *info);
	int (*enable_charging)(struct mtk_charger *info, bool en);
	int (*do_event)(struct notifier_block *nb, unsigned long ev, void *v);
	int (*do_dvchg1_event)(struct notifier_block *nb, unsigned long ev,
			       void *v);
	int (*do_dvchg2_event)(struct notifier_block *nb, unsigned long ev,
			       void *v);
	int (*change_current_setting)(struct mtk_charger *info);
	void *algo_data;
};

#define CYCLE_COUNT_MAX 4
struct charger_custom_data {
	int battery_cv;	/* uv */
	int max_charger_voltage;
	int max_charger_voltage_setting;
	int min_charger_voltage;

	int usb_charger_current;
	int ac_charger_current;
	int ac_charger_input_current;
	int charging_host_charger_current;
/*N17 code for HQ-290909 by miaozhichao at 2023/5/25 start*/
	int non_std_ac_charger_current;
/*N17 code for HQ-290909 by miaozhichao at 2023/5/25 end*/
/*N17 code for HQ-291115 by miaozhichao at 2023/5/30 start*/
	/* sw jeita */
	int jeita_temp_above_t6_cv;
	int jeita_temp_t5_to_t6_cv;
	int jeita_temp_t4_to_t5_cv;
	int jeita_temp_t3_to_t4_cv;
	int jeita_temp_t2_to_t3_cv;
	int jeita_temp_t1_to_t2_cv;
	int jeita_temp_t0_to_t1_cv;
	int jeita_temp_below_t0_cv;

	int jeita_temp_t5_to_t6_cc;
	int jeita_temp_t4_to_t5_cc;
	int jeita_temp_t3_to_t4_cc;
	int jeita_temp_t2_to_t3_cc;
	int jeita_temp_t1_to_t2_cc;
	int jeita_temp_t0_to_t1_cc;
	int jeita_temp_below_t0_cc;

	int temp_t6_thres;
	int temp_t5_thres;
	int temp_t4_thres;
	int temp_t4_thres_minus_x_degree;
	int temp_t3_thres;
	int temp_t3_thres_minus_x_degree;
	int temp_t2_thres;
	int temp_t2_thres_plus_x_degree;
	int temp_t1_thres;
	int temp_t1_thres_plus_x_degree;
	int temp_t0_thres;
	int temp_t0_thres_plus_x_degree;
	int temp_neg_10_thres;
/*N17 code for HQ-291115 by miaozhichao at 2023/5/30 end*/
	/* battery temperature protection */
	int mtk_temperature_recharge_support;
	int max_charge_temp;
	int max_charge_temp_minus_x_degree;
	int min_charge_temp;
	int min_charge_temp_plus_x_degree;

	/* dynamic mivr */
	int min_charger_voltage_1;
	int min_charger_voltage_2;
	int max_dmivr_charger_current;
/*N17 code for dropfv by liluting at 2023/7/3 start*/
        int cyclecount[CYCLE_COUNT_MAX];
        int dropfv_ffc[CYCLE_COUNT_MAX];
        int dropfv_noffc[CYCLE_COUNT_MAX];
/*N17 code for dropfv by liluting at 2023/7/3 end*/
};

struct charger_data {
	int input_current_limit;
	int charging_current_limit;

	int force_charging_current;
	int thermal_input_current_limit;
	int thermal_charging_current_limit;
	int disable_charging_count;
	int input_current_limit_by_aicl;
	int junction_temp_min;
	int junction_temp_max;
};

enum chg_data_idx_enum {
	CHG1_SETTING,
	CHG2_SETTING,
	DVCHG1_SETTING,
	DVCHG2_SETTING,
	CHGS_SETTING_MAX,
};

/*N17 code for low_fast_blank_flag by xm liluting at 2023/08/03 start*/
enum blank_flag{
	NROMAL = 0,
	BLACK_TO_BRIGHT = 1,
	BRIGHT = 2,
	BLACK = 3,
};
/*N17 code for low_fast_blank_flag by xm liluting at 2023/08/03 end*/

struct mtk_charger {
	struct platform_device *pdev;
	struct charger_device *chg1_dev;
	struct notifier_block chg1_nb;
	struct charger_device *chg2_dev;
	struct charger_device *dvchg1_dev;
	struct notifier_block dvchg1_nb;
	struct charger_device *dvchg2_dev;
	struct notifier_block dvchg2_nb;

	struct charger_data chg_data[CHGS_SETTING_MAX];
	struct chg_limit_setting setting;
	enum charger_configuration config;

	struct power_supply_desc psy_desc1;
	struct power_supply_config psy_cfg1;
	struct power_supply *psy1;

	struct power_supply_desc psy_desc2;
	struct power_supply_config psy_cfg2;
	struct power_supply *psy2;

	struct power_supply_desc psy_dvchg_desc1;
	struct power_supply_config psy_dvchg_cfg1;
	struct power_supply *psy_dvchg1;

	struct power_supply_desc psy_dvchg_desc2;
	struct power_supply_config psy_dvchg_cfg2;
	struct power_supply *psy_dvchg2;

	struct power_supply  *chg_psy;
	struct power_supply  *bat_psy;
	struct adapter_device *pd_adapter;
	struct notifier_block pd_nb;
	struct mutex pd_lock;
	int pd_type;
	bool pd_reset;

	u32 bootmode;
	u32 boottype;

	int chr_type;
	int usb_type;
	int usb_state;

	struct mutex cable_out_lock;
	int cable_out_cnt;

	/* system lock */
	spinlock_t slock;
	struct wakeup_source *charger_wakelock;
	struct mutex charger_lock;

	/* thread related */
	wait_queue_head_t  wait_que;
	bool charger_thread_timeout;
	unsigned int polling_interval;
	bool charger_thread_polling;

	/* alarm timer */
	struct alarm charger_timer;
	struct timespec64 endtime;
	bool is_suspend;
	struct notifier_block pm_notifier;
	ktime_t timer_cb_duration[8];

	/* notify charger user */
	struct srcu_notifier_head evt_nh;
/* N17 code for HQ-294995 by miaozhichao at 20230525 start */
	bool ship_mode;
/* N17 code for HQ-294995 by miaozhichao at 20230525 end */
	/* common info */
	int log_level;
	bool usb_unlimited;
	bool charger_unlimited;
	bool disable_charger;
	bool disable_aicl;
	int battery_temp;
	bool can_charging;
	bool cmd_discharging;
	bool safety_timeout;
	int safety_timer_cmd;
	bool vbusov_stat;
	bool is_chg_done;
	/* ATM */
	bool atm_enabled;
	const char *algorithm_name;
	struct mtk_charger_algorithm algo;

	/* dtsi custom data */
	struct charger_custom_data data;

	/* battery warning */
	unsigned int notify_code;
	unsigned int notify_test_mode;

	/* sw safety timer */
	bool enable_sw_safety_timer;
	bool sw_safety_timer_setting;
	struct timespec64 charging_begin_time;

	/* vbat monitor, 6pin bat */
	bool batpro_done;
	bool enable_vbat_mon;
	bool enable_vbat_mon_bak;
	int old_cv;
	bool stop_6pin_re_en;
	int vbat0_flag;

	/* sw jeita */
	bool enable_sw_jeita;
	struct sw_jeita_data sw_jeita;

	/* battery thermal protection */
	struct battery_thermal_protection_data thermal;

	struct chg_alg_device *alg[MAX_ALG_NO];
	struct notifier_block chg_alg_nb;
	bool enable_hv_charging;

	/* water detection */
	bool water_detected;

	bool enable_dynamic_mivr;

	/* fast charging algo support indicator */
	bool enable_fast_charging_indicator;
	unsigned int fast_charging_indicator;

	/* diasable meta current limit for testing */
	unsigned int enable_meta_current_limit;

	struct smartcharging sc;

	/*daemon related*/
	struct sock *daemo_nl_sk;
	u_int g_scd_pid;
	struct scd_cmd_param_t_1 sc_data;

	/*charger IC charging status*/
	bool is_charging;

	ktime_t uevent_time_check;

	bool force_disable_pp[CHG2_SETTING + 1];
	bool enable_pp[CHG2_SETTING + 1];
	struct mutex pp_lock[CHG2_SETTING + 1];
/* N17 code for HQ-292280 by tongjiacheng at 20230610 start */
	u8 thermal_level;
/* N17 code for HQ-292280 by tongjiacheng at 20230610 end */
        int diff_fv_val;
	/*N17 code for HQ-319688 by p-xiepengfu at 20230907 start*/
	int deltaFv;
	/*N17 code for HQ-319688 by p-xiepengfu at 20230907 end*/
/* N17 smart_batt*/
/*N17 code for HQ-305986 by xm tianye9 at 2023/07/05 start*/
	/* smart_chg, point to smart_chg array of struct mtk_battery */
	struct smart_chg *smart_charge;
	struct power_supply *battery_psy;
	int soc;
	/*N17 code for HQHW-4862 by yeyinzi at 2023/08/15 start*/
	struct delayed_work pe_stop_enable_termination_work;
	/*N17 code for HQHW-4862 by yeyinzi at 2023/08/15 end*/
	struct delayed_work xm_charge_work;
/*N17 code for HQ-305986 by xm tianye9 at 2023/07/05 end*/
        bool is_full_flag;
/*N17 code for screen_state by xm liluting at 2023/07/11 start*/
        struct chg_screen_monitor sm;
/*N17 code for screen_state by xm liluting at 2023/07/11 end*/
/*N17 code for low_fast by xm liluting at 2023/07/07 start*/
        bool first_low_plugin_flag;
        bool pps_fast_mode;
/*N17 code for low_fast by xm liluting at 2023/07/07 end*/
/*N17 code for thermal_board_temp by xm liluting at 2023/07/18 start*/
        int thermal_board_temp; /* board temp from thermal*/
	struct notifier_block chg_nb; /* charger notifier */
/*N17 code for thermal_board_temp by xm liluting at 2023/07/18 end*/
/*N17 code for cp_mode test by xm liluting at 2023/07/31 start*/
        int fake_thermal_vote_current;
/*N17 code for cp_mode test by xm liluting at 2023/07/31 end*/
/*N17 code for low_fast_blank_flag by xm liluting at 2023/08/03 start*/
        enum blank_flag b_flag;
/*N17 code for low_fast_blank_flag by xm liluting at 2023/08/03 end*/
/*N17 code for HQ-329243 by yeyinzi at 2023/09/21 start*/
	bool during_switching;
/*N17 code for HQ-329243 by yeyinzi at 2023/09/21 end*/
};
/* N17 code for HQ-292280 by tongjiacheng at 20230610 start */
static const int thermal_mitigation_pps[] = {
	6000000, 5400000, 5000000, 4500000, 4000000, 3500000, 3000000, 2700000,
	2500000, 2300000, 2100000,   1800000,    1500000,    900000,   500000,    300000
};

static const int thermal_mitigation_qc[] = {
	3000000, 2700000, 2500000, 2300000, 2100000, 1900000, 1700000, 1500000,
	1000000, 1000000, 1000000, 1000000, 1000000,    900000,   500000,    300000
};
/* N17 code for HQ-292280 by tongjiacheng at 20230610 end*/

/*N17 code for low_fast by xm liluting at 2023/07/07 start*/
static int thermal_mitigation_pps_fast[] = {
	6000000, 6000000, 6000000, 6000000, 6000000, 6000000, 3000000, 2700000,
	2500000, 2300000, 2100000,   1800000,    1500000,    900000,   500000,    300000
};
/*N17 code for low_fast by xm liluting at 2023/07/07 end*/

/*N17 code for thermal_board_temp by xm liluting at 2023/07/18 start*/
enum charger_notifier_events {
	/* thermal board temp */
	 THERMAL_BOARD_TEMP = 0,
};
/*N17 code for board_sensor_temp by xm liluting at 2023/07/18 end*/

static inline int mtk_chg_alg_notify_call(struct mtk_charger *info,
					  enum chg_alg_notifier_events evt,
					  int value)
{
	int i;
	struct chg_alg_notify notify = {
		.evt = evt,
		.value = value,
	};

	for (i = 0; i < MAX_ALG_NO; i++) {
		if (info->alg[i])
			chg_alg_notifier_call(info->alg[i], &notify);
	}
	return 0;
}

/* functions which framework needs*/
extern int mtk_basic_charger_init(struct mtk_charger *info);
extern int mtk_pulse_charger_init(struct mtk_charger *info);
extern int get_uisoc(struct mtk_charger *info);
extern int get_battery_voltage(struct mtk_charger *info);
extern int get_battery_temperature(struct mtk_charger *info);
extern int get_battery_current(struct mtk_charger *info);
extern int get_vbus(struct mtk_charger *info);
extern int get_ibat(struct mtk_charger *info);
extern int get_ibus(struct mtk_charger *info);
extern bool is_battery_exist(struct mtk_charger *info);
extern int get_charger_type(struct mtk_charger *info);
extern int get_usb_type(struct mtk_charger *info);
extern int disable_hw_ovp(struct mtk_charger *info, int en);
extern bool is_charger_exist(struct mtk_charger *info);
extern int get_charger_temperature(struct mtk_charger *info,
	struct charger_device *chg);
extern int get_charger_charging_current(struct mtk_charger *info,
	struct charger_device *chg);
extern int get_charger_input_current(struct mtk_charger *info,
	struct charger_device *chg);
extern int get_charger_zcv(struct mtk_charger *info,
	struct charger_device *chg);
extern void _wake_up_charger(struct mtk_charger *info);

/* functions for other */
extern int mtk_chg_enable_vbus_ovp(bool enable);
extern void smart_batt_set_diff_fv(int val);
extern int smart_batt_get_diff_fv(void);

/*N17 code for thermal_board_temp by xm liluting at 2023/07/18 start*/
extern struct srcu_notifier_head charger_notifier;
extern int charger_reg_notifier(struct notifier_block *nb);
extern int charger_unreg_notifier(struct notifier_block *nb);
extern int charger_notifier_call_cnain(unsigned long event,int val);
/*N17 code for thermal_board_temp by xm liluting at 2023/07/18 end*/

#endif /* __MTK_CHARGER_H */
