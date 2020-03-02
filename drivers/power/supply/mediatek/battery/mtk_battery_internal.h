/*
 * Copyright (C) 2016 MediaTek Inc.
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

#ifndef __MTK_BATTERY_INTF_H__
#define __MTK_BATTERY_INTF_H__

#ifndef _DEA_MODIFY_
#include <linux/power_supply.h>
#include <pmic_lbat_service.h>
#else
#include "module_hrtimer.h"
#include "mtk_battery.h"
#endif

#include <mtk_gauge_time_service.h>
#include <mtk_gauge_class.h>


/* ============================================================ */
/* Define Macro Value */
/* ============================================================ */
#define FGD_NL_MSG_T_HDR_LEN 28
#define FGD_NL_MAGIC 2015060303
#define FGD_NL_MSG_MAX_LEN 9200

#define UNIT_TRANS_10	10

#define UNIT_TRANS_100	100
#define UNIT_TRANS_1000	1000
#define UNIT_TRANS_60 60

#define MAX_TABLE 10

/* ============================================================ */
/* power misc related */
/* ============================================================ */
#define BAT_VOLTAGE_LOW_BOUND 3400
#define BAT_VOLTAGE_HIGH_BOUND 3450
#define LOW_TMP_BAT_VOLTAGE_LOW_BOUND 3200
#define SHUTDOWN_TIME 40
#define AVGVBAT_ARRAY_SIZE 30
#define INIT_VOLTAGE 3450
#define BATTERY_SHUTDOWN_TEMPERATURE 60

/* ============================================================ */
/* typedef and Struct*/
/* ============================================================ */
#define BMLOG_ERROR_LEVEL   3
#define BMLOG_WARNING_LEVEL 4
#define BMLOG_NOTICE_LEVEL  5
#define BMLOG_INFO_LEVEL    6
#define BMLOG_DEBUG_LEVEL   7
#define BMLOG_TRACE_LEVEL   8

#define bm_err(fmt, args...)   \
do {\
	if (bat_get_debug_level() >= BMLOG_ERROR_LEVEL) {\
		pr_notice(fmt, ##args); \
	} \
} while (0)

#define bm_warn(fmt, args...)   \
do {\
	if (bat_get_debug_level() >= BMLOG_WARNING_LEVEL) {\
		pr_notice(fmt, ##args); \
	}								   \
} while (0)

#define bm_notice(fmt, args...)   \
do {\
	if (bat_get_debug_level() >= BMLOG_NOTICE_LEVEL) {\
		pr_notice(fmt, ##args); \
	}								   \
} while (0)

#define bm_info(fmt, args...)   \
do {\
	if (bat_get_debug_level() >= BMLOG_INFO_LEVEL) {\
		pr_notice(fmt, ##args); \
	}								   \
} while (0)

#define bm_debug(fmt, args...)   \
do {\
	if (bat_get_debug_level() >= BMLOG_DEBUG_LEVEL) {\
		pr_notice(fmt, ##args); \
	}								   \
} while (0)

#define bm_trace(fmt, args...)\
do {\
	if (bat_get_debug_level() >= BMLOG_TRACE_LEVEL) {\
		pr_notice(fmt, ##args);\
	}						\
} while (0)

#define BM_DAEMON_DEFAULT_LOG_LEVEL 3

enum gauge_hw_version {
	GAUGE_HW_V1000 = 1000,
	GAUGE_HW_V2000 = 2000,
	GAUGE_HW_V2001 = 2001,

	GAUGE_HW_MAX
};

enum daemon_req_hw_data {
	HW_INFO_NORETURN = 0,
	HW_INFO_SHUTDOWN_CAR = 1,
	HW_INFO_NCAR = 2,
};


enum Fg_daemon_cmds {
	FG_DAEMON_CMD_PRINT_LOG,
	FG_DAEMON_CMD_SET_DAEMON_PID,
	FG_DAEMON_CMD_GET_CUSTOM_SETTING,
	FG_DAEMON_CMD_GET_DATA,
	FG_DAEMON_CMD_IS_BAT_EXIST,
	FG_DAEMON_CMD_GET_INIT_FLAG,
	FG_DAEMON_CMD_SET_INIT_FLAG,
	FG_DAEMON_CMD_NOTIFY_DAEMON,
	FG_DAEMON_CMD_CHECK_FG_DAEMON_VERSION,
	FG_DAEMON_CMD_FGADC_RESET,
	FG_DAEMON_CMD_GET_TEMPERTURE,
	FG_DAEMON_CMD_GET_RAC,
	FG_DAEMON_CMD_GET_PTIM_VBAT,
	FG_DAEMON_CMD_GET_PTIM_I,
	FG_DAEMON_CMD_IS_CHARGER_EXIST,
	FG_DAEMON_CMD_GET_HW_OCV,
	FG_DAEMON_CMD_GET_FG_HW_CAR,
	FG_DAEMON_CMD_SET_FG_BAT_INT1_GAP,
	FG_DAEMON_CMD_SET_FG_BAT_TMP_GAP,
	FG_DAEMON_CMD_SET_FG_BAT_INT2_HT_GAP,
	FG_DAEMON_CMD_ENABLE_FG_BAT_INT2_HT,
	FG_DAEMON_CMD_SET_FG_BAT_INT2_LT_GAP,
	FG_DAEMON_CMD_ENABLE_FG_BAT_INT2_LT,
	FG_DAEMON_CMD_IS_BAT_PLUGOUT,
	FG_DAEMON_CMD_IS_BAT_CHARGING,
	FG_DAEMON_CMD_GET_CHARGER_STATUS,
	FG_DAEMON_CMD_SET_SW_OCV,
	FG_DAEMON_CMD_GET_SHUTDOWN_DURATION_TIME,
	FG_DAEMON_CMD_GET_BAT_PLUG_OUT_TIME,
	FG_DAEMON_CMD_GET_IS_FG_INITIALIZED,
	FG_DAEMON_CMD_SET_IS_FG_INITIALIZED,
	FG_DAEMON_CMD_SET_FG_RESET_RTC_STATUS,
	FG_DAEMON_CMD_IS_HWOCV_UNRELIABLE,
	FG_DAEMON_CMD_GET_FG_CURRENT_AVG,
	FG_DAEMON_CMD_SET_FG_TIME,
	FG_DAEMON_CMD_GET_FG_TIME,
	FG_DAEMON_CMD_GET_ZCV,
	FG_DAEMON_CMD_GET_FG_SW_CAR_NAFG_CNT,
	FG_DAEMON_CMD_GET_FG_SW_CAR_NAFG_DLTV,
	FG_DAEMON_CMD_GET_FG_SW_CAR_NAFG_C_DLTV,
	FG_DAEMON_CMD_SET_NAG_ZCV,
	FG_DAEMON_CMD_SET_NAG_ZCV_EN,
	FG_DAEMON_CMD_SET_NAG_C_DLTV,
	FG_DAEMON_CMD_SET_ZCV_INTR,
	FG_DAEMON_CMD_SET_FG_QUSE,/*remove*/
	FG_DAEMON_CMD_SET_FG_RESISTANCE,/*remove*/
	FG_DAEMON_CMD_SET_FG_DC_RATIO,
	FG_DAEMON_CMD_SET_BATTERY_CYCLE_THRESHOLD,
	FG_DAEMON_CMD_SOFF_RESET,
	FG_DAEMON_CMD_NCAR_RESET,
	FG_DAEMON_CMD_GET_IMIX,
	FG_DAEMON_CMD_GET_AGING_FACTOR_CUST,
	FG_DAEMON_CMD_GET_D0_C_SOC_CUST,
	FG_DAEMON_CMD_GET_D0_V_SOC_CUST,
	FG_DAEMON_CMD_GET_UISOC_CUST,
	FG_DAEMON_CMD_IS_KPOC,
	FG_DAEMON_CMD_GET_NAFG_VBAT,
	FG_DAEMON_CMD_GET_HW_INFO,
	FG_DAEMON_CMD_SET_KERNEL_SOC,
	FG_DAEMON_CMD_SET_KERNEL_UISOC,
	FG_DAEMON_CMD_SET_KERNEL_INIT_VBAT,
	FG_DAEMON_CMD_SET_BAT_PLUGOUT_INTR,
	FG_DAEMON_CMD_SET_IAVG_INTR,
	FG_DAEMON_CMD_SET_FG_SHUTDOWN_COND,
	FG_DAEMON_CMD_GET_FG_SHUTDOWN_COND,
	FG_DAEMON_CMD_ENABLE_FG_VBAT_L_INT,
	FG_DAEMON_CMD_ENABLE_FG_VBAT_H_INT,
	FG_DAEMON_CMD_SET_FG_VBAT_L_TH,
	FG_DAEMON_CMD_SET_FG_VBAT_H_TH,
	FG_DAEMON_CMD_SET_CAR_TUNE_VALUE,
	FG_DAEMON_CMD_GET_FG_CURRENT_IAVG_VALID,
	FG_DAEMON_CMD_GET_RTC_UI_SOC,
	FG_DAEMON_CMD_SET_RTC_UI_SOC,
	FG_DAEMON_CMD_GET_CON0_SOC,
	FG_DAEMON_CMD_SET_CON0_SOC,
	FG_DAEMON_CMD_GET_NVRAM_FAIL_STATUS,
	FG_DAEMON_CMD_SET_NVRAM_FAIL_STATUS,
	FG_DAEMON_CMD_SET_FG_BAT_TMP_C_GAP,
	FG_DAEMON_CMD_IS_BATTERY_CYCLE_RESET,
	FG_DAEMON_CMD_GET_RTC_TWO_SEC_REBOOT,
	FG_DAEMON_CMD_GET_RTC_INVALID,
	FG_DAEMON_CMD_GET_VBAT,
	FG_DAEMON_CMD_GET_DISABLE_NAFG,
	FG_DAEMON_CMD_DUMP_LOG,
	FG_DAEMON_CMD_SEND_DATA,
	FG_DAEMON_CMD_COMMUNICATION_INT,

	FG_DAEMON_CMD_FROM_USER_NUMBER
};

/* kernel cmd */
enum Fg_kernel_cmds {
	FG_KERNEL_CMD_NO_ACTION,
	FG_KERNEL_CMD_DUMP_REGULAR_LOG,
	FG_KERNEL_CMD_DISABLE_NAFG,
	FG_KERNEL_CMD_DUMP_LOG,
	FG_KERNEL_CMD_UISOC_UPDATE_TYPE,
	FG_KERNEL_CMD_CHANG_LOGLEVEL,
	FG_KERNEL_CMD_REQ_ALGO_DATA,

	FG_KERNEL_CMD_FROM_USER_NUMBER

};

enum Fg_interrupt_flags {
	FG_INTR_0 = 0,
	FG_INTR_TIMER_UPDATE  = 1,
	FG_INTR_BAT_CYCLE = 2,
	FG_INTR_CHARGER_OUT = 4,
	FG_INTR_CHARGER_IN = 8,
	FG_INTR_FG_TIME =		16,
	FG_INTR_BAT_INT1_HT =	32,
	FG_INTR_BAT_INT1_LT =	64,
	FG_INTR_BAT_INT2_HT =	128,
	FG_INTR_BAT_INT2_LT =	256,
	FG_INTR_BAT_TMP_HT =	512,
	FG_INTR_BAT_TMP_LT =	1024,
	FG_INTR_BAT_TIME_INT =	2048,
	FG_INTR_NAG_C_DLTV =	4096,
	FG_INTR_FG_ZCV = 8192,
	FG_INTR_SHUTDOWN = 16384,
	FG_INTR_RESET_NVRAM = 32768,
	FG_INTR_BAT_PLUGOUT = 65536,
	FG_INTR_IAVG = 0x20000,
	FG_INTR_VBAT2_L = 0x40000,
	FG_INTR_VBAT2_H = 0x80000,
	FG_INTR_CHR_FULL = 0x100000,
	FG_INTR_DLPT_SD = 0x200000,
	FG_INTR_BAT_TMP_C_HT = 0x400000,
	FG_INTR_BAT_TMP_C_LT = 0x800000,
	FG_INTR_BAT_INT1_CHECK = 0x1000000,
	FG_INTR_KERNEL_CMD = 0x2000000,

};

struct fgd_nl_msg_t {
	unsigned int fgd_cmd;
	unsigned int fgd_subcmd;
	unsigned int fgd_subcmd_para1;
	unsigned int fgd_subcmd_para2;
	unsigned int fgd_data_len;
	unsigned int fgd_ret_data_len;
	unsigned int identity;
	char fgd_data[FGD_NL_MSG_MAX_LEN];
};

/* get data type */
enum Fg_data_type {
	FUEL_GAUGE_TABLE_CUSTOM_DATA,
	FGD_CMD_PARAM_T_CUSTOM,

	FG_DATA_TYPE_NUMBER
};

struct fgd_cmd_param_t_6 {
	unsigned int type;
	unsigned int total_size;
	unsigned int size;
	unsigned int idx;
	char input[2048];
};

struct fgd_cmd_param_t_7 {
	int type;
	int input;
	int output;
	int status;
};

enum daemon_cmd_int_data {
	FG_GET_NORETURN = 0,
	FG_GET_SHUTDOWN_CAR = 1,
	FG_GET_NCAR = 2,
	FG_GET_CURR_1 = 3,
	FG_GET_CURR_2 = 4,
	FG_GET_REFRESH = 5,
	FG_GET_MAX,
	FG_SET_ANCHOR = 999,
	FG_SET_SOC = FG_SET_ANCHOR + 1,
	FG_SET_C_D0_SOC = FG_SET_ANCHOR + 2,
	FG_SET_V_D0_SOC = FG_SET_ANCHOR + 3,
	FG_SET_C_SOC = FG_SET_ANCHOR + 4,
	FG_SET_V_SOC = FG_SET_ANCHOR + 5,
	FG_SET_QMAX_T_AGING = FG_SET_ANCHOR + 6,
	FG_SET_SAVED_CAR = FG_SET_ANCHOR + 7,
	FG_SET_AGING_FACTOR = FG_SET_ANCHOR + 8,
	FG_SET_QMAX = FG_SET_ANCHOR + 9,
	FG_SET_BAT_CYCLES = FG_SET_ANCHOR + 10,
	FG_SET_NCAR = FG_SET_ANCHOR + 11,
	FG_SET_OCV_mah = FG_SET_ANCHOR + 12,
	FG_SET_OCV_Vtemp = FG_SET_ANCHOR + 13,
	FG_SET_OCV_SOC = FG_SET_ANCHOR + 14,
	FG_SET_DATA_MAX,
};

struct fuel_gauge_custom_data {

	int versionID1;
	int versionID2;
	int versionID3;
	int fg_get_max;
	int fg_set_max;
	int hardwareVersion;

	int low_temp_mode;
	int low_temp_mode_temp;

	/* Qmax for battery  */
	int q_max_L_current;
	int q_max_H_current;
	int q_max_sys_voltage;

	int pseudo1_en;
	int pseudo100_en;
	int pseudo100_en_dis;
	int pseudo1_iq_offset;

	/* vboot related */
	int qmax_sel;
	int iboot_sel;
	int shutdown_system_iboot;

	/* multi temp gauge 0% */
	int multi_temp_gauge0;

	/* hw related */
	int car_tune_value;
	int fg_meter_resistance;
	int com_fg_meter_resistance;
	int r_fg_value;
	int com_r_fg_value;
	int mtk_chr_exist;

	/* Aging Compensation 1*/
	int aging_one_en;
	int aging1_update_soc;
	int aging1_load_soc;
	int aging_temp_diff;
	int aging_100_en;
	int difference_voltage_update;

	/* Aging Compensation 2*/
	int aging_two_en;

	/* Aging Compensation 3*/
	int aging_third_en;


	/* ui_soc */
	int diff_soc_setting;
	int keep_100_percent;
	int difference_full_cv;
	int diff_bat_temp_setting;
	int diff_bat_temp_setting_c;
	int discharge_tracking_time;
	int charge_tracking_time;
	int difference_fullocv_vth;
	int difference_fullocv_ith;
	int charge_pseudo_full_level;
	int over_discharge_level;
	int full_tracking_bat_int2_multiply;

	/* threshold */
	int hwocv_swocv_diff;	/* 0.1 mv */
	int hwocv_swocv_diff_lt;	/* 0.1 mv */
	int hwocv_swocv_diff_lt_temp;	/* degree */
	int hwocv_oldocv_diff;	/* 0.1 mv */
	int hwocv_oldocv_diff_chr;	/* 0.1 mv */
	int swocv_oldocv_diff;	/* 0.1 mv */
	int swocv_oldocv_diff_chr;	/* 0.1 mv */
	int vbat_oldocv_diff;	/* 0.1 mv */
	int tnew_told_pon_diff;	/* degree */
	int tnew_told_pon_diff2;/* degree */
	int pmic_shutdown_time;	/* secs */
	int bat_plug_out_time;	/* min */
	int swocv_oldocv_diff_emb;	/* 0.1 mv */
	int vir_oldocv_diff_emb;	/* 0.1 mv */
	int vir_oldocv_diff_emb_lt;
	int vir_oldocv_diff_emb_tmp;

	/* fgc & fgv threshold */
	int difference_fgc_fgv_th1;
	int difference_fgc_fgv_th2;
	int difference_fgc_fgv_th3;
	int difference_fgc_fgv_th_soc1;
	int difference_fgc_fgv_th_soc2;
	int nafg_time_setting;
	int nafg_ratio;
	int nafg_ratio_en;
	int nafg_ratio_tmp_thr;
	int nafg_resistance;

	/* mode select */
	int pmic_shutdown_current;
	int pmic_shutdown_sw_en;
	int force_vc_mode;
	int embedded_sel;
	int loading_1_en;
	int loading_2_en;
	int diff_iavg_th;

	/* ADC resister */
	int r_bat_sense;	/*is it used?*/
	int r_i_sense;		/*is it used?*/
	int r_charger_1;
	int r_charger_2;

	/* pre_tracking */
	int fg_pre_tracking_en;
	int vbat2_det_time;
	int vbat2_det_counter;
	int vbat2_det_voltage1;
	int vbat2_det_voltage2;
	int vbat2_det_voltage3;

	int shutdown_1_time;
	int shutdown_gauge0;
	int shutdown_gauge1_xmins;
	int shutdown_gauge1_mins;
	int shutdown_gauge0_voltage;
	int shutdown_gauge1_vbat_en;
	int shutdown_gauge1_vbat;

	/* ZCV update */
	int zcv_suspend_time;
	int sleep_current_avg;

	int dc_ratio_sel;
	int dc_r_cnt;

	int pseudo1_sel;

	/* using current to limit uisoc in 100% case */
	int ui_full_limit_en;
	int ui_full_limit_soc0;
	int ui_full_limit_ith0;
	int ui_full_limit_soc1;
	int ui_full_limit_ith1;
	int ui_full_limit_soc2;
	int ui_full_limit_ith2;
	int ui_full_limit_soc3;
	int ui_full_limit_ith3;
	int ui_full_limit_soc4;
	int ui_full_limit_ith4;
	int ui_full_limit_time;

	/* using voltage to limit uisoc in 1% case */
	int ui_low_limit_en;
	int ui_low_limit_soc0;
	int ui_low_limit_vth0;
	int ui_low_limit_soc1;
	int ui_low_limit_vth1;
	int ui_low_limit_soc2;
	int ui_low_limit_vth2;
	int ui_low_limit_soc3;
	int ui_low_limit_vth3;
	int ui_low_limit_soc4;
	int ui_low_limit_vth4;
	int ui_low_limit_time;

	int d0_sel;
	int dod_init_sel;
	int aging_sel;
	int fg_tracking_current;
	int fg_tracking_current_iboot_en;
	int ui_fast_tracking_en;
	int ui_fast_tracking_gap;
	int bat_par_i;
	int c_old_d0;
	int v_old_d0;
	int c_soc;
	int v_soc;
	int ui_old_soc;

	int aging_factor_min;
	int aging_factor_diff;
	int keep_100_percent_minsoc;
	int battery_tmp_to_disable_gm30;
	int battery_tmp_to_disable_nafg;
	int battery_tmp_to_enable_nafg;
	int disable_nafg;

	int zcv_car_gap_percentage;
	int uisoc_update_type;

	/* boot status */
	int pl_charger_status;
	int power_on_car_chr;
	int power_on_car_nochr;
	int shutdown_car_ratio;

	/* log_level */
	int daemon_log_level;
	int record_log;

};

struct FUELGAUGE_TEMPERATURE {
	signed int BatteryTemp;
	signed int TemperatureR;
};

struct FUELGAUGE_PROFILE_STRUCT {
	unsigned int mah;
	unsigned short voltage;
	unsigned short resistance; /* Ohm*/
	unsigned short resistance2; /* Ohm*/
	unsigned short percentage;
};

struct fuel_gauge_table {
	int temperature;
	int q_max;
	int q_max_h_current;
	int pseudo1;
	int pseudo100;
	int pmic_min_vol;
	int pon_iboot;
	int qmax_sys_vol;
	int shutdown_hl_zcv;

	int size;
	struct FUELGAUGE_PROFILE_STRUCT fg_profile[100];
};


struct fuel_gauge_table_custom_data {
	/* cust_battery_meter_table.h */
	int active_table_number;
	struct fuel_gauge_table fg_profile[MAX_TABLE];

	int temperature_tb0;
	int fg_profile_temperature_0_size;
	struct FUELGAUGE_PROFILE_STRUCT fg_profile_temperature_0[100];

	int temperature_tb1;
	int fg_profile_temperature_1_size;
	struct FUELGAUGE_PROFILE_STRUCT fg_profile_temperature_1[100];
};

struct fgd_cmd_param_t_custom {
	struct fuel_gauge_custom_data fg_cust_data;
	struct fuel_gauge_table_custom_data fg_table_cust_data;
};


struct battery_data {
	struct power_supply_desc psd;
	struct power_supply *psy;
	int BAT_STATUS;
	int BAT_HEALTH;
	int BAT_PRESENT;
	int BAT_TECHNOLOGY;
	int BAT_CAPACITY;
	/* Add for Battery Service */
	int BAT_batt_vol;
	int BAT_batt_temp;
};

struct BAT_EC_Struct {
	int fixed_temp_en;
	int fixed_temp_value;
	int debug_rac_en;
	int debug_rac_value;
	int debug_ptim_v_en;
	int debug_ptim_v_value;
	int debug_ptim_r_en;
	int debug_ptim_r_value;
	int debug_ptim_r_value_sign;
	int debug_fg_curr_en;
	int debug_fg_curr_value;
	int debug_bat_id_en;
	int debug_bat_id_value;
	int debug_d0_c_en;
	int debug_d0_c_value;
	int debug_d0_v_en;
	int debug_d0_v_value;
	int debug_uisoc_en;
	int debug_uisoc_value;
	int debug_kill_daemontest;
};

struct battery_temperature_table {
	int type;
	unsigned int rbat_pull_up_r;
	unsigned int rbat_pull_up_volt;
	unsigned int bif_ntc_r;
};

struct simulator_log {
	int bat_full_int;
	int dlpt_sd_int;
	int chr_in_int;
	int zcv_int;
	int zcv_current;
	int zcv;
	int chr_status;
	int ptim_bat;
	int ptim_cur;
	int ptim_is_charging;

	int phone_state;

	/* initial */
	int fg_reset;

	int car_diff;


	/* rtc */
	int is_gauge_initialized;
	int rtc_ui_soc;
	int is_rtc_invalid;
	int is_bat_plugout;
	int bat_plugout_time;

	/* system info */
	int twosec_reboot;
	int pl_charging_status;
	int moniter_plchg_status;
	int bat_plug_status;
	int is_nvram_fail_mode;
	int con0_soc;

};

struct mtk_battery {

	struct gauge_device *gdev;

/*linux driver related*/
	wait_queue_head_t  wait_que;
	unsigned int fg_update_flag;
	struct hrtimer fg_hrtimer;
	struct mutex fg_mutex;
	struct mutex notify_mutex;
	struct srcu_notifier_head gm_notify;

/*custom related*/
	int battery_id;

/*simulator log*/
	struct simulator_log log;

/*daemon related*/
	struct sock *daemo_nl_sk;
	u_int g_fgd_pid;

/* gauge hw status
 * exchange data between hw & sw
 */
	struct gauge_hw_status hw_status;

/* log */
	int log_level;
	int d_log_level;

/* for test */
	struct BAT_EC_Struct Bat_EC_ctrl;
	int BAT_EC_cmd;
	int BAT_EC_param;

/*battery status*/
	int soc;
	int ui_soc;
	int d_saved_car;

/*battery flag*/
	bool init_flag;
	bool is_probe_done;
	bool disable_nafg_int;
	bool disableGM30;
	bool disable_mtkbattery;
	bool cmd_disable_nafg;
	bool ntc_disable_nafg;

/*battery plug out*/
	bool disable_plug_int;


/* adb */
	int fixed_bat_tmp;
	int fixed_uisoc;

	struct charger_consumer *pbat_consumer;
	struct notifier_block bat_nb;

/* ptim */
	int ptim_vol;
	int ptim_curr;

/* proc */
	int proc_cmd_id;
	unsigned int proc_subcmd;
	unsigned int proc_subcmd_para1;
	char proc_log[4096];

/*battery interrupt*/
	int fg_bat_int1_gap;
	int fg_bat_int1_ht;
	int fg_bat_int1_lt;

	int fg_bat_int2_ht;
	int fg_bat_int2_lt;
	int fg_bat_int2_ht_en;
	int fg_bat_int2_lt_en;

	int fg_bat_tmp_int_gap;
	int fg_bat_tmp_c_int_gap;
	int fg_bat_tmp_ht;
	int fg_bat_tmp_lt;
	int fg_bat_tmp_c_ht;
	int fg_bat_tmp_c_lt;
	int fg_bat_tmp_int_ht;
	int fg_bat_tmp_int_lt;

/* battery cycle */
	bool is_reset_battery_cycle;
	int bat_cycle;
	int bat_cycle_thr;
	int bat_cycle_car;
	int bat_cycle_ncar;

/* cust req ocv data */
	int algo_qmax;
	int algo_req_ocv;
	int algo_ocv_to_mah;
	int algo_ocv_to_soc;
	int algo_vtemp;

	int aging_factor;

	struct timespec uisoc_oldtime;

	signed int ptim_lk_v;
	signed int ptim_lk_i;
	int lk_boot_coulomb;
	int pl_bat_vol;
	int pl_shutdown_time;
	int pl_two_sec_reboot;
	int plug_miss_count;

	struct gtimer tracking_timer;
	struct gtimer one_percent_timer;

	struct gauge_consumer coulomb_plus;
	struct gauge_consumer coulomb_minus;
	struct gauge_consumer soc_plus;
	struct gauge_consumer soc_minus;

	struct timespec chr_full_handler_time;

	/*sw average current*/
	struct timespec sw_iavg_time;
	int sw_iavg_car;
	int sw_iavg;
	int sw_iavg_ht;
	int sw_iavg_lt;
	int sw_iavg_gap;

	/*sw low battery interrupt*/
	struct lbat_user lowbat_service;
	int sw_low_battery_ht_en;
	int sw_low_battery_ht_threshold;
	int sw_low_battery_lt_en;
	int sw_low_battery_lt_threshold;
	struct mutex sw_low_battery_mutex;

	/*nafg monitor */
	int last_nafg_cnt;
	struct timespec last_nafg_update_time;
	bool is_nafg_broken;

	/* battery temperature table */
	struct battery_temperature_table rbat;

	struct fgd_cmd_param_t_custom fg_data;
};


/* mtk_power_misc */
enum {
	NORMAL = 0,
	OVERHEAT,
	SOC_ZERO_PERCENT,
	UISOC_ONE_PERCENT,
	LOW_BAT_VOLT,
	DLPT_SHUTDOWN,
	SHUTDOWN_FACTOR_MAX
};

extern struct mtk_battery gm;
extern struct battery_data battery_main;
extern struct fuel_gauge_custom_data fg_cust_data;
extern struct fuel_gauge_table_custom_data fg_table_cust_data;
extern struct gauge_hw_status FG_status;
extern struct FUELGAUGE_TEMPERATURE Fg_Temperature_Table[];

extern int wakeup_fg_algo_cmd(unsigned int flow_state, int cmd, int para1);
extern int wakeup_fg_algo(unsigned int flow_state);

/* mtk_power_misc.c */
extern void mtk_power_misc_init(struct platform_device *pdev);
extern void notify_fg_shutdown(void);
extern int set_shutdown_cond(int shutdown_cond);
extern int disable_shutdown_cond(int shutdown_cond);
extern int get_shutdown_cond(void);
extern void set_shutdown_vbat_lt(int lt, int lt2);
extern void set_shutdown_cond_flag(int flag);
extern int get_shutdown_cond_flag(void);
/* end mtk_power_misc.c */

/* mtk_battery.c */
extern bool is_battery_init_done(void);
extern int force_get_tbat(bool update);
extern int bat_get_debug_level(void);
extern bool is_kernel_power_off_charging(void);
extern bool is_fg_disabled(void);
extern void notify_fg_dlpt_sd(void);
extern bool fg_interrupt_check(void);
extern void bmd_ctrl_cmd_from_user(void *nl_data, struct fgd_nl_msg_t *ret_msg);
extern int interpolation(int i1, int b1, int i2, int b2, int i);
extern struct mtk_battery *get_mtk_battery(void);
extern void battery_update_psd(struct battery_data *bat_data);
extern int wakeup_fg_algo(unsigned int flow_state);
extern int wakeup_fg_algo_cmd(unsigned int flow_state, int cmd, int para1);
extern int wakeup_fg_algo_atomic(unsigned int flow_state);
extern unsigned int TempToBattVolt(int temp, int update);
extern int fg_get_battery_temperature_for_zcv(void);
extern int battery_get_charger_zcv(void);
extern bool is_fg_disabled(void);
extern int battery_notifier(int event);


/* pmic */
extern int pmic_get_battery_voltage(void);
extern int pmic_get_v_bat_temp(void);
extern unsigned int upmu_get_rgs_chrdet(void);
extern int pmic_get_ibus(void);
extern int pmic_is_bif_exist(void);
extern int pmic_get_vbus(void);
extern bool pmic_is_battery_exist(void);

/* usb*/
extern bool mt_usb_is_device(void);

/* gauge hal */
extern void battery_dump_nag(void);

/* gauge interface */
extern bool gauge_get_current(int *bat_current);
extern int gauge_get_average_current(bool *valid);
extern int gauge_get_coulomb(void);
extern int gauge_set_coulomb_interrupt1_ht(int car);
extern int gauge_set_coulomb_interrupt1_lt(int car);
extern int gauge_get_ptim_current(int *ptim_current, bool *is_charging);
extern int gauge_get_hw_version(void);
extern int gauge_set_nag_en(int nafg_zcv_en);
extern int gauge_get_nag_vbat(void);

/* mtk_battery_recovery.c */
extern void battery_recovery_init(void);
extern void wakeup_fg_algo_recovery(unsigned int cmd);

/* DLPT */
extern int do_ptim_gauge(
	bool isSuspend, unsigned int *bat, signed int *cur, bool *is_charging);
extern int get_rac(void);
extern int get_imix(void);

/* functions for fg hal */
extern void set_hw_ocv_unreliable(bool _flag_unreliable);


/* mtk_battery_core.c */
extern void mtk_battery_init(struct platform_device *dev);
extern void mtk_battery_last_init(struct platform_device *dev);
extern void fg_bat_temp_int_internal(void);
extern void fgauge_get_profile_id(void);
extern void battery_update(struct battery_data *bat_data);
extern void fg_custom_init_from_header(void);
extern void notify_fg_chr_full(void);
extern void fg_update_sw_iavg(void);
extern void fg_bat_temp_int_sw_check(void);
extern void fg_update_sw_low_battery_check(unsigned int thd);
extern void fg_sw_bat_cycle_accu(void);
extern void fg_ocv_query_soc(int ocv);

/* GM3 simulator */
extern void gm3_log_init(void);
extern void gm3_log_notify(unsigned int interrupt);
extern void gm3_log_dump(void);


/* query function , review */
extern struct BAT_EC_Struct *get_ec(void);

/* GM25 Plug out API */
int en_intr_VBATON_UNDET(int en);
int reg_VBATON_UNDET(void (*callback)(void));


#endif /* __MTK_BATTERY_INTF_H__ */
