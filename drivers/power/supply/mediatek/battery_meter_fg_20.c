/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h> /* For init/exit macros */
#include <linux/interrupt.h>
#include <linux/module.h> /* For MODULE_ marcros  */
#include <linux/platform_device.h>
#include <linux/spinlock.h>

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/time.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

#include <linux/kernel.h>
#include <linux/netlink.h>
#include <linux/reboot.h>
#include <linux/skbuff.h>
#include <linux/socket.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <net/genetlink.h>
#include <net/sock.h>

#include <mt-plat/mtk_boot.h>
#include <mt-plat/mtk_rtc.h>

#include <mach/mtk_battery_meter.h>
#include <mach/mtk_battery_meter_table.h>
#include <mach/mtk_pmic.h>
#include <mt-plat/battery_common.h>
#include <mt-plat/battery_meter.h>
#include <mt-plat/battery_meter_hal.h>

#include <mt-plat/upmu_common.h>

/* ============================================================ // */
/* define */
/* ============================================================ // */
#define PROFILE_SIZE 4

static DEFINE_MUTEX(FGADC_mutex);

int Enable_FGADC_LOG = BMLOG_INFO_LEVEL;

#define NETLINK_FGD 26
#define CUST_SETTING_VERSION 0x100000
#define FGD_CHECK_VERSION 0x100001

/* ============================================================ // */
/* global variable */
/* ============================================================ // */
BATTERY_METER_CONTROL battery_meter_ctrl;

kal_bool gFG_Is_Charging = KAL_FALSE;
kal_bool gFG_Is_Charging_init = KAL_FALSE;

signed int g_auxadc_solution;
unsigned int g_spm_timer = 600;
bool bat_spm_timeout;
struct timespec g_sleep_total_time;

#ifdef MTK_ENABLE_AGING_ALGORITHM
unsigned int suspend_total_time;
#endif

unsigned int add_time;
signed int g_booting_vbat;
/*static unsigned int temperature_change = 1;*/

static struct sock *daemo_nl_sk;
static void nl_send_to_user(u32 pid, int seq, struct fgd_nl_msg_t *reply_msg);
static u_int g_fgd_pid;
static unsigned int g_fgd_version = -1;
static kal_bool init_flag;

void battery_meter_set_init_flag(kal_bool flag)
{
	init_flag = flag;
}

void battery_meter_reset_sleep_time(void)
{
	g_sleep_total_time.tv_sec = 0;
	g_sleep_total_time.tv_nsec = 0;
}

/* PMIC AUXADC Related Variable */
int g_R_BAT_SENSE = R_BAT_SENSE;
int g_R_I_SENSE = R_I_SENSE;
int g_R_CHARGER_1 = R_CHARGER_1;
int g_R_CHARGER_2 = R_CHARGER_2;

int gFG_result = 1;
int gFG_plugout_status;
int gFG_result_soc;

int fix_coverity1;
int fix_coverity2;
/* HW FG */
#ifndef DIFFERENCE_HWOCV_RTC
#define DIFFERENCE_HWOCV_RTC 30 /* 30% difference */
#endif

#ifndef DIFFERENCE_HWOCV_SWOCV
#define DIFFERENCE_HWOCV_SWOCV 15 /* 105% difference */
#endif

#ifndef DIFFERENCE_SWOCV_RTC
#define DIFFERENCE_SWOCV_RTC 10 /* 10% difference */
#endif

#ifndef DIFFERENCE_HWOCV_VBAT
#define DIFFERENCE_HWOCV_VBAT 30
#endif

#ifndef DIFFERENCE_VBAT_RTC
#define DIFFERENCE_VBAT_RTC 30
#endif

#ifndef DIFFERENCE_SWOCV_RTC_POS
#define DIFFERENCE_SWOCV_RTC_POS 15
#endif

#ifndef MAX_SWOCV
#define MAX_SWOCV 5 /* 5% maximum */
#endif

/* SW Fuel Gauge */
#ifndef MAX_HWOCV
#define MAX_HWOCV 5
#endif

#ifndef MAX_VBAT
#define MAX_VBAT 90
#endif

#ifndef Q_MAX_SYS_VOLTAGE
#define Q_MAX_SYS_VOLTAGE 3300
#endif

#ifndef CUST_TRACKING_GAP
#define CUST_TRACKING_GAP 15
#endif

#ifndef CUST_TRACKINGOFFSET
#define CUST_TRACKINGOFFSET 0
#endif

#ifndef CUST_TRACKINGEN
#define CUST_TRACKINGEN 0
#endif

/* smooth time tracking */
signed int gFG_coulomb_act_time = -1;
signed int gFG_coulomb_act_pre;
signed int gFG_coulomb_act_diff;
signed int gFG_coulomb_act_diff_time;
signed int gFG_coulomb_is_charging;

signed int gFG_DOD0_init;
signed int gFG_DOD0;
signed int gFG_DOD1;
signed int gFG_DOD_B;
signed int gFG_coulomb;
signed int gFG_coulomb_act;
signed int gFG_voltage;
signed int gFG_current;
signed int gFG_current_init;
signed int gFG_capacity;
signed int gFG_capacity_by_c = -1;
signed int gFG_capacity_by_c_init;
signed int gFG_capacity_by_v;
signed int gFG_capacity_by_v_init;
signed int gFG_temp = 100;
signed int gFG_temp_avg = 100;
signed int gFG_temp_avg_init = 100;
signed int gFG_resistance_bat;
signed int gFG_compensate_value;
signed int gFG_ori_voltage;
signed int gFG_BATT_CAPACITY;
signed int gFG_voltage_init;
signed int gFG_current_auto_detect_R_fg_total;
signed int gFG_current_auto_detect_R_fg_count;
signed int gFG_current_auto_detect_R_fg_result;
signed int gFG_15_vlot = 3700;
signed int gFG_BATT_CAPACITY_high_current = 1200;
signed int gFG_BATT_CAPACITY_aging = 1200;
signed int gFG_vbat;
signed int gFG_swocv;
signed int gFG_hwocv;
signed int gFG_vbat_soc;
signed int gFG_hw_soc;
signed int gFG_sw_soc;
#ifdef USING_SMOOTH_UI_SOC2
signed int temp_UI_SOC2 = -1;
signed int pre_UI_SOC2;
signed int UI_SOC3;
signed int pre_cc_act;
#endif

/* voltage mode */
signed int gfg_percent_check_point = 50;
signed int volt_mode_update_timer;
signed int volt_mode_update_time_out = 6; /* 1mins */

/* EM */
signed int g_fg_dbg_bat_volt;
signed int g_fg_dbg_bat_current;
signed int g_fg_dbg_bat_zcv;
signed int g_fg_dbg_bat_temp;
signed int g_fg_dbg_bat_r;
signed int g_fg_dbg_bat_car;
signed int g_fg_dbg_bat_qmax;
signed int g_fg_dbg_d0;
signed int g_fg_dbg_d1;
signed int g_fg_dbg_percentage;
signed int g_fg_dbg_percentage_fg;
signed int g_fg_dbg_percentage_voltmode;

signed int FGvbatVoltageBuffer[FG_VBAT_AVERAGE_SIZE];
signed int FGbatteryIndex;
signed int FGbatteryVoltageSum;
signed int gFG_voltage_AVG;
signed int gFG_vbat_offset;
signed int vbat_offset_counter;
#ifdef Q_MAX_BY_CURRENT
signed int FGCurrentBuffer[FG_CURRENT_AVERAGE_SIZE];
signed int FGCurrentIndex;
signed int FGCurrentSum;
#endif
signed int gFG_current_AVG;
signed int g_tracking_point = CUST_TRACKING_POINT;
signed int g_rtc_fg_soc;
signed int g_I_SENSE_offset;

/* SW FG */
signed int oam_v_ocv_init;
signed int oam_v_ocv_1;
signed int oam_v_ocv_2;
signed int oam_r_1;
signed int oam_r_2;
signed int oam_d0;
signed int oam_i_ori;
signed int oam_i_1;
signed int oam_i_2;
signed int oam_car_1;
signed int oam_car_2;
signed int oam_d_1 = 1;
signed int oam_d_2 = 1;
signed int oam_d_3 = 1;
signed int oam_d_3_pre;
signed int oam_d_4;
signed int oam_d_4_pre;
signed int oam_d_5;
signed int oam_init_i;
signed int oam_run_i;
signed int d5_count;
signed int d5_count_time = 60;
signed int d5_count_time_rate = 1;
signed int g_d_hw_ocv;
signed int g_vol_bat_hw_ocv;

/* SW FG 2.0 */
signed int oam_v_ocv;
signed int oam_r;
signed int swfg_ap_suspend_time;
signed int ap_suspend_car;
struct timespec ap_suspend_time;
signed int total_suspend_times;
signed int this_suspend_times;
signed int last_hwocv;
signed int last_i;
signed int hwocv_token;
signed int is_hwocv_update;

signed int g_hw_ocv_before_sleep;
struct timespec suspend_time, car_time;
signed int g_sw_vbat_temp;
struct timespec last_oam_run_time;
/*static signed int coulomb_before_sleep = 0x123456;*/
#if !defined(CONFIG_POWER_EXT)
static signed int last_time = 1;
#endif
/* aging mechanism */
#ifdef MTK_ENABLE_AGING_ALGORITHM

#ifndef SUSPEND_CURRENT_CHECK_THRESHOLD
#define SUSPEND_CURRENT_CHECK_THRESHOLD 100 /* 10mA */
#endif

#ifndef DIFFERENCE_VOLTAGE_UPDATE
#define DIFFERENCE_VOLTAGE_UPDATE 20 /* 20mV */
#endif

#ifndef OCV_RECOVER_TIME
#define OCV_RECOVER_TIME 2100
#endif

#ifndef AGING1_UPDATE_SOC
#define AGING1_UPDATE_SOC 30
#endif

#ifndef AGING1_LOAD_SOC
#define AGING1_LOAD_SOC 70
#endif

#ifndef MIN_DOD_DIFF_THRESHOLD
#define MIN_DOD_DIFF_THRESHOLD 40
#endif

#ifndef MIN_DOD2_DIFF_THRESHOLD
#define MIN_DOD2_DIFF_THRESHOLD 70
#endif

#ifndef CHARGE_TRACKING_TIME
#define CHARGE_TRACKING_TIME 60
#endif

#ifndef DISCHARGE_TRACKING_TIME
#define DISCHARGE_TRACKING_TIME 10
#endif

static signed int suspend_current_threshold = SUSPEND_CURRENT_CHECK_THRESHOLD;
static signed int ocv_check_time = OCV_RECOVER_TIME;

static signed int difference_voltage_update = DIFFERENCE_VOLTAGE_UPDATE;
static signed int aging1_load_soc = AGING1_LOAD_SOC;
static signed int aging1_update_soc = AGING1_UPDATE_SOC;
static signed int shutdown_system_voltage = SHUTDOWN_SYSTEM_VOLTAGE;
static signed int charge_tracking_time = CHARGE_TRACKING_TIME;
static signed int discharge_tracking_time = DISCHARGE_TRACKING_TIME;

#endif /* aging mechanism */

#ifndef RECHARGE_TOLERANCE
#define RECHARGE_TOLERANCE 10
#endif

/*static signed int recharge_tolerance = RECHARGE_TOLERANCE;*/

#ifdef SHUTDOWN_GAUGE0
static signed int shutdown_gauge0 = 1;
#else
static signed int shutdown_gauge0;
#endif

#ifdef SHUTDOWN_GAUGE1_MINS
static signed int shutdown_gauge1_xmins = 1;
#else
static signed int shutdown_gauge1_xmins;
#endif

#ifndef FG_CURRENT_INIT_VALUE
#define FG_CURRENT_INIT_VALUE 3500
#endif

#ifndef FG_MIN_CHARGING_SMOOTH_TIME
#define FG_MIN_CHARGING_SMOOTH_TIME 40
#endif

#ifndef APSLEEP_MDWAKEUP_CAR
#define APSLEEP_MDWAKEUP_CAR 5240
#endif

#ifndef AP_MDSLEEP_CAR
#define AP_MDSLEEP_CAR 30
#endif

#ifndef APSLEEP_BATTERY_VOLTAGE_COMPENSATE
#define APSLEEP_BATTERY_VOLTAGE_COMPENSATE 150
#endif

static signed int shutdown_gauge1_mins = SHUTDOWN_GAUGE1_MINS;

signed int gFG_battery_cycle;
signed int gFG_aging_factor_1 = 100;
signed int gFG_aging_factor_2 = 100;
signed int gFG_loading_factor1 = 100;
signed int gFG_loading_factor2 = 100;
/* battery info */

signed int gFG_coulomb_cyc;
signed int gFG_coulomb_aging;
signed int gFG_pre_coulomb_count = 0x12345678;
#ifdef MTK_BATTERY_LIFETIME_DATA_SUPPORT
signed int gFG_max_voltage;
signed int gFG_min_voltage = 10000;
signed int gFG_max_current;
signed int gFG_min_current;
signed int gFG_max_temperature = -20;
signed int gFG_min_temperature = 100;

#endif /* battery info */

unsigned int g_sw_fg_version = 150327;
static signed int gFG_daemon_log_level = BM_DAEMON_DEFAULT_LOG_LEVEL;
static unsigned char gDisableFG;

/* ============================================================ // */
/* function prototype */
/* ============================================================ // */

int __attribute__((weak)) PMIC_IMM_GetCurrent(void)
{
	battery_log(BAT_LOG_FULL, "wait for auxadc porting\n");
	return 0;
}

struct battery_meter_table_custom_data {

	/* cust_battery_meter_table.h */
	int battery_profile_t0_size;
	BATTERY_PROFILE_STRUCT battery_profile_t0[100];
	int battery_profile_t1_size;
	BATTERY_PROFILE_STRUCT battery_profile_t1[100];
	int battery_profile_t2_size;
	BATTERY_PROFILE_STRUCT battery_profile_t2[100];
	int battery_profile_t3_size;
	BATTERY_PROFILE_STRUCT battery_profile_t3[100];
	int battery_profile_temperature_size;
	BATTERY_PROFILE_STRUCT battery_profile_temperature[100];

	int r_profile_t0_size;
	R_PROFILE_STRUCT r_profile_t0[100];
	int r_profile_t1_size;
	R_PROFILE_STRUCT r_profile_t1[100];
	int r_profile_t2_size;
	R_PROFILE_STRUCT r_profile_t2[100];
	int r_profile_t3_size;
	R_PROFILE_STRUCT r_profile_t3[100];
	int r_profile_temperature_size;
	R_PROFILE_STRUCT r_profile_temperature[100];
};

struct battery_meter_custom_data batt_meter_cust_data;
struct battery_meter_table_custom_data batt_meter_table_cust_data;

/* Temperature window size */
#define TEMP_AVERAGE_SIZE 30
kal_bool gFG_Is_offset_init = KAL_FALSE;

#ifdef MTK_MULTI_BAT_PROFILE_SUPPORT
unsigned int g_fg_battery_id;

#ifdef MTK_GET_BATTERY_ID_BY_AUXADC
void fgauge_get_profile_id(void)
{
	int id_volt = 0;
	int id = 0;
	int ret = 0;

	ret = IMM_GetOneChannelValue_Cali(BATTERY_ID_CHANNEL_NUM, &id_volt);
	if (ret != 0)
		bm_info("[%s]id_volt read fail\n", __func__);
	else
		bm_info("[%s]id_volt = %d\n", __func__, id_volt);

	if ((sizeof(g_battery_id_voltage) / sizeof(signed int)) !=
	    TOTAL_BATTERY_NUMBER) {
		bm_info("[%s]error! voltage range incorrect!\n",
		__func__);
		return;
	}

	for (id = 0; id < TOTAL_BATTERY_NUMBER; id++) {
		if (id_volt < g_battery_id_voltage[id]) {
			g_fg_battery_id = id;
			break;
		} else if (g_battery_id_voltage[id] == -1) {
			g_fg_battery_id = TOTAL_BATTERY_NUMBER - 1;
		}
	}

	bm_info("[%s]Battery id (%d)\n", __func__, g_fg_battery_id);
}
#elif defined(MTK_GET_BATTERY_ID_BY_GPIO)
void fgauge_get_profile_id(void)
{
	g_fg_battery_id = 0;
}
#else
void fgauge_get_profile_id(void)
{
	g_fg_battery_id = 0;
}
#endif
#endif

int __batt_meter_init_cust_data_from_cust_header(struct platform_device *dev)
{
	/* cust_battery_meter_table.h */

	batt_meter_table_cust_data.battery_profile_t0_size =
		sizeof(battery_profile_t0) / sizeof(BATTERY_PROFILE_STRUCT);

	memcpy(&batt_meter_table_cust_data.battery_profile_t0,
	       &battery_profile_t0, sizeof(battery_profile_t0));

	batt_meter_table_cust_data.battery_profile_t1_size =
		sizeof(battery_profile_t1) / sizeof(BATTERY_PROFILE_STRUCT);

	memcpy(&batt_meter_table_cust_data.battery_profile_t1,
	       &battery_profile_t1, sizeof(battery_profile_t1));

	batt_meter_table_cust_data.battery_profile_t2_size =
		sizeof(battery_profile_t2) / sizeof(BATTERY_PROFILE_STRUCT);

	memcpy(&batt_meter_table_cust_data.battery_profile_t2,
	       &battery_profile_t2, sizeof(battery_profile_t2));

	batt_meter_table_cust_data.battery_profile_t3_size =
		sizeof(battery_profile_t3) / sizeof(BATTERY_PROFILE_STRUCT);

	memcpy(&batt_meter_table_cust_data.battery_profile_t3,
	       &battery_profile_t3, sizeof(battery_profile_t3));

	batt_meter_table_cust_data.r_profile_t0_size =
		sizeof(r_profile_t0) / sizeof(R_PROFILE_STRUCT);

	memcpy(&batt_meter_table_cust_data.r_profile_t0, &r_profile_t0,
	       sizeof(r_profile_t0));

	batt_meter_table_cust_data.r_profile_t1_size =
		sizeof(r_profile_t1) / sizeof(R_PROFILE_STRUCT);

	memcpy(&batt_meter_table_cust_data.r_profile_t1, &r_profile_t1,
	       sizeof(r_profile_t1));

	batt_meter_table_cust_data.r_profile_t2_size =
		sizeof(r_profile_t2) / sizeof(R_PROFILE_STRUCT);

	memcpy(&batt_meter_table_cust_data.r_profile_t2, &r_profile_t2,
	       sizeof(r_profile_t2));
	batt_meter_table_cust_data.r_profile_t3_size =
		sizeof(r_profile_t3) / sizeof(R_PROFILE_STRUCT);

	memcpy(&batt_meter_table_cust_data.r_profile_t3, &r_profile_t3,
	       sizeof(r_profile_t3));

/* cust_battery_meter.h */

#if defined(SOC_BY_HW_FG)
	batt_meter_cust_data.soc_flow = HW_FG;
#elif defined(SOC_BY_SW_FG)
	batt_meter_cust_data.soc_flow = SW_FG;
#elif defined(SOC_BY_AUXADC)
	batt_meter_cust_data.soc_flow = AUXADC;
#endif

#if defined(HW_FG_FORCE_USE_SW_OCV)
	batt_meter_cust_data.hw_fg_force_use_sw_ocv = 1;
#else  /* #if defined(HW_FG_FORCE_USE_SW_OCV) */
	batt_meter_cust_data.hw_fg_force_use_sw_ocv = 0;
#endif /* #if defined(HW_FG_FORCE_USE_SW_OCV) */

	/* ADC resister */
	batt_meter_cust_data.r_bat_sense = R_BAT_SENSE;
	g_R_BAT_SENSE = R_BAT_SENSE;
	batt_meter_cust_data.r_i_sense = R_I_SENSE;
	g_R_I_SENSE = R_I_SENSE;
	batt_meter_cust_data.r_charger_1 = R_CHARGER_1;
	g_R_CHARGER_1 = R_CHARGER_1;
	batt_meter_cust_data.r_charger_2 = R_CHARGER_2;
	g_R_CHARGER_2 = R_CHARGER_2;

	batt_meter_cust_data.temperature_t0 = TEMPERATURE_T0;
	batt_meter_cust_data.temperature_t1 = TEMPERATURE_T1;
	batt_meter_cust_data.temperature_t2 = TEMPERATURE_T2;
	batt_meter_cust_data.temperature_t3 = TEMPERATURE_T3;
	batt_meter_cust_data.temperature_t = TEMPERATURE_T;

	batt_meter_cust_data.fg_meter_resistance = FG_METER_RESISTANCE;

	/* Qmax for battery  */
	batt_meter_cust_data.q_max_pos_50 = Q_MAX_POS_50;
	batt_meter_cust_data.q_max_pos_25 = Q_MAX_POS_25;
	batt_meter_cust_data.q_max_pos_0 = Q_MAX_POS_0;
	batt_meter_cust_data.q_max_neg_10 = Q_MAX_NEG_10;
	batt_meter_cust_data.q_max_pos_50_h_current = Q_MAX_POS_50_H_CURRENT;
	batt_meter_cust_data.q_max_pos_25_h_current = Q_MAX_POS_25_H_CURRENT;
	batt_meter_cust_data.q_max_pos_0_h_current = Q_MAX_POS_0_H_CURRENT;
	batt_meter_cust_data.q_max_neg_10_h_current = Q_MAX_NEG_10_H_CURRENT;
	batt_meter_cust_data.oam_d5 = OAM_D5; /* 1 : D5,   0: D2 */

#if defined(CHANGE_TRACKING_POINT)
	batt_meter_cust_data.change_tracking_point = 1;
#else  /* #if defined(CHANGE_TRACKING_POINT) */
	batt_meter_cust_data.change_tracking_point = 0;
#endif /* #if defined(CHANGE_TRACKING_POINT) */
	batt_meter_cust_data.cust_tracking_point = CUST_TRACKING_POINT;
	g_tracking_point = CUST_TRACKING_POINT;
	batt_meter_cust_data.cust_r_sense = CUST_R_SENSE;
	batt_meter_cust_data.cust_hw_cc = CUST_HW_CC;
	batt_meter_cust_data.aging_tuning_value = AGING_TUNING_VALUE;
	batt_meter_cust_data.cust_r_fg_offset = CUST_R_FG_OFFSET;
	batt_meter_cust_data.ocv_board_compesate = OCV_BOARD_COMPESATE;
	batt_meter_cust_data.r_fg_board_base = R_FG_BOARD_BASE;
	batt_meter_cust_data.r_fg_board_slope = R_FG_BOARD_SLOPE;
	batt_meter_cust_data.car_tune_value = CAR_TUNE_VALUE;

	/* HW Fuel gague  */
	batt_meter_cust_data.current_detect_r_fg = CURRENT_DETECT_R_FG;
	batt_meter_cust_data.minerroroffset = MinErrorOffset;
	batt_meter_cust_data.fg_vbat_average_size = FG_VBAT_AVERAGE_SIZE;
	batt_meter_cust_data.r_fg_value = R_FG_VALUE;

	batt_meter_cust_data.difference_hwocv_rtc = DIFFERENCE_HWOCV_RTC;
	batt_meter_cust_data.difference_hwocv_swocv = DIFFERENCE_HWOCV_SWOCV;
	batt_meter_cust_data.difference_swocv_rtc = DIFFERENCE_SWOCV_RTC;
	batt_meter_cust_data.max_swocv = MAX_SWOCV;

	batt_meter_cust_data.max_hwocv = MAX_HWOCV;
	batt_meter_cust_data.max_vbat = MAX_VBAT;
	batt_meter_cust_data.difference_hwocv_vbat = DIFFERENCE_HWOCV_VBAT;

	batt_meter_cust_data.difference_vbat_rtc = DIFFERENCE_VBAT_RTC;
	batt_meter_cust_data.difference_swocv_rtc_pos =
		DIFFERENCE_SWOCV_RTC_POS;

	batt_meter_cust_data.suspend_current_threshold =
		SUSPEND_CURRENT_CHECK_THRESHOLD;
	batt_meter_cust_data.ocv_check_time = OCV_RECOVER_TIME;

	batt_meter_cust_data.shutdown_system_voltage = SHUTDOWN_SYSTEM_VOLTAGE;
	batt_meter_cust_data.recharge_tolerance = RECHARGE_TOLERANCE;

#if defined(FIXED_TBAT_25)
	batt_meter_cust_data.fixed_tbat_25 = 1;
#else  /* #if defined(FIXED_TBAT_25) */
	batt_meter_cust_data.fixed_tbat_25 = 0;
#endif /* #if defined(FIXED_TBAT_25) */

	batt_meter_cust_data.batterypseudo100 = BATTERYPSEUDO100;
	batt_meter_cust_data.batterypseudo1 = BATTERYPSEUDO1;

	/* Dynamic change wake up period of battery thread when suspend */
	batt_meter_cust_data.vbat_normal_wakeup = VBAT_NORMAL_WAKEUP;
	batt_meter_cust_data.vbat_low_power_wakeup = VBAT_LOW_POWER_WAKEUP;
	batt_meter_cust_data.normal_wakeup_period = NORMAL_WAKEUP_PERIOD;
	/* _g_bat_sleep_total_time = NORMAL_WAKEUP_PERIOD; */
	batt_meter_cust_data.low_power_wakeup_period = LOW_POWER_WAKEUP_PERIOD;
	batt_meter_cust_data.close_poweroff_wakeup_period =
		CLOSE_POWEROFF_WAKEUP_PERIOD;

#if defined(INIT_SOC_BY_SW_SOC)
	batt_meter_cust_data.init_soc_by_sw_soc = 1;
#else  /* #if defined(INIT_SOC_BY_SW_SOC) */
	batt_meter_cust_data.init_soc_by_sw_soc = 0;
#endif /* #if defined(INIT_SOC_BY_SW_SOC) */
#if defined(SYNC_UI_SOC_IMM)
	batt_meter_cust_data.sync_ui_soc_imm = 1;
#else /* #if defined(SYNC_UI_SOC_IMM) */
	batt_meter_cust_data.sync_ui_soc_imm = 0;
#endif /* #if defined(SYNC_UI_SOC_IMM) */
#if defined(MTK_ENABLE_AGING_ALGORITHM)
	batt_meter_cust_data.mtk_enable_aging_algorithm = 1;
#else  /* #if defined(MTK_ENABLE_AGING_ALGORITHM) */
	batt_meter_cust_data.mtk_enable_aging_algorithm = 0;
#endif /* #if defined(MTK_ENABLE_AGING_ALGORITHM) */
#if defined(MD_SLEEP_CURRENT_CHECK)
	batt_meter_cust_data.md_sleep_current_check = 1;
#else  /* #if defined(MD_SLEEP_CURRENT_CHECK) */
	batt_meter_cust_data.md_sleep_current_check = 0;
#endif /* #if defined(MD_SLEEP_CURRENT_CHECK) */
#if defined(Q_MAX_BY_CURRENT)
	batt_meter_cust_data.q_max_by_current = 1;
#elif defined(Q_MAX_BY_SYS)
	batt_meter_cust_data.q_max_by_current = 2;
#else /* #if defined(Q_MAX_BY_CURRENT) */
	batt_meter_cust_data.q_max_by_current = 0;
#endif /* #if defined(Q_MAX_BY_CURRENT) */
	batt_meter_cust_data.q_max_sys_voltage = Q_MAX_SYS_VOLTAGE;

#if defined(CONFIG_MTK_EMBEDDED_BATTERY)
	batt_meter_cust_data.embedded_battery = 1;
#else
	batt_meter_cust_data.embedded_battery = 0;
#endif

#if defined(SHUTDOWN_GAUGE0)
	batt_meter_cust_data.shutdown_gauge0 = 1;
#else /* #if defined(SHUTDOWN_GAUGE0) */
	batt_meter_cust_data.shutdown_gauge0 = 0;
#endif /* #if defined(SHUTDOWN_GAUGE0) */
#if defined(SHUTDOWN_GAUGE1_XMINS)
	batt_meter_cust_data.shutdown_gauge1_xmins = 1;
#else  /* #if defined(SHUTDOWN_GAUGE1_XMINS) */
	batt_meter_cust_data.shutdown_gauge1_xmins = 0;
#endif /* #if defined(SHUTDOWN_GAUGE1_XMINS) */
	batt_meter_cust_data.shutdown_gauge1_mins = SHUTDOWN_GAUGE1_MINS;

	batt_meter_cust_data.tracking_gap = CUST_TRACKING_GAP;
	batt_meter_cust_data.trackingoffset = CUST_TRACKINGOFFSET;
	batt_meter_cust_data.trackingen = CUST_TRACKINGEN;
	batt_meter_cust_data.min_charging_smooth_time =
		FG_MIN_CHARGING_SMOOTH_TIME;

	batt_meter_cust_data.apsleep_battery_voltage_compensate =
		APSLEEP_BATTERY_VOLTAGE_COMPENSATE;

	batt_meter_cust_data.bat_task_period = BAT_TASK_PERIOD;

	return 0;
}

int __batt_meter_init_cust_data_from_dt(struct platform_device *dev)
{
	struct device_node *np = dev->dev.of_node;
	unsigned int val;

	bm_debug("%s\n", __func__);

	if (of_property_read_u32(np, "hw_fg_force_use_sw_ocv", &val)) {
		batt_meter_cust_data.hw_fg_force_use_sw_ocv = (int)val;
		bm_debug("Get hw_fg_force_use_sw_ocv: %d\n",
			 batt_meter_cust_data.hw_fg_force_use_sw_ocv);
	} else {
		bm_err("Get hw_fg_force_use_sw_ocv failed\n");
	}

	if (of_property_read_u32(np, "r_bat_sense", &val)) {
		batt_meter_cust_data.r_bat_sense = (int)val;
		bm_debug("Get r_bat_sense: %d\n",
			 batt_meter_cust_data.r_bat_sense);
	} else {
		bm_err("Get r_bat_sense failed\n");
	}

	if (of_property_read_u32(np, "r_i_sense", &val)) {
		batt_meter_cust_data.r_i_sense = (int)val;
		bm_debug("Get r_i_sense: %d\n", batt_meter_cust_data.r_i_sense);
	} else {
		bm_err("Get r_i_sense failed\n");
	}

	if (of_property_read_u32(np, "r_charger_1", &val)) {
		batt_meter_cust_data.r_charger_1 = (int)val;
		bm_debug("Get r_charger_1: %d\n",
			 batt_meter_cust_data.r_charger_1);
	} else {
		bm_err("Get r_charger_1 failed\n");
	}

	if (of_property_read_u32(np, "r_charger_2", &val)) {
		batt_meter_cust_data.r_charger_2 = (int)val;
		bm_debug("Get r_charger_2: %d\n",
			 batt_meter_cust_data.r_charger_2);
	} else {
		bm_err("Get r_charger_2 failed\n");
	}

	if (of_property_read_u32(np, "temperature_t0", &val)) {
		batt_meter_cust_data.temperature_t0 = (int)val;
		bm_debug("Get temperature_t0: %d\n",
			 batt_meter_cust_data.temperature_t0);
	} else {
		bm_err("Get temperature_t0 failed\n");
	}

	if (of_property_read_u32(np, "temperature_t1", &val)) {
		batt_meter_cust_data.temperature_t1 = (int)val;
		bm_debug("Get temperature_t1: %d\n",
			 batt_meter_cust_data.temperature_t1);
	} else {
		bm_err("Get temperature_t1 failed\n");
	}

	if (of_property_read_u32(np, "temperature_t2", &val)) {
		batt_meter_cust_data.temperature_t2 = (int)val;
		bm_debug("Get temperature_t2: %d\n",
			 batt_meter_cust_data.temperature_t2);
	} else {
		bm_err("Get temperature_t2 failed\n");
	}

	if (of_property_read_u32(np, "temperature_t3", &val)) {
		batt_meter_cust_data.temperature_t3 = (int)val;
		bm_debug("Get temperature_t3: %d\n",
			 batt_meter_cust_data.temperature_t3);
	} else {
		bm_err("Get temperature_t3 failed\n");
	}

	if (of_property_read_u32(np, "temperature_t", &val)) {
		batt_meter_cust_data.temperature_t = (int)val;
		bm_debug("Get temperature_t: %d\n",
			 batt_meter_cust_data.temperature_t);
	} else {
		bm_err("Get temperature_t failed\n");
	}

	if (of_property_read_u32(np, "fg_meter_resistance", &val)) {
		batt_meter_cust_data.fg_meter_resistance = (int)val;
		bm_debug("Get fg_meter_resistance: %d\n",
			 batt_meter_cust_data.fg_meter_resistance);
	} else {
		bm_err("Get fg_meter_resistance failed\n");
	}

	if (of_property_read_u32(np, "q_max_pos_50", &val)) {
		batt_meter_cust_data.q_max_pos_50 = (int)val;
		bm_debug("Get q_max_pos_50: %d\n",
			 batt_meter_cust_data.q_max_pos_50);
	} else {
		bm_err("Get q_max_pos_50 failed\n");
	}

	if (of_property_read_u32(np, "q_max_pos_25", &val)) {
		batt_meter_cust_data.q_max_pos_25 = (int)val;
		bm_debug("Get q_max_pos_25: %d\n",
			 batt_meter_cust_data.q_max_pos_25);
	} else {
		bm_err("Get q_max_pos_25 failed\n");
	}

	if (of_property_read_u32(np, "q_max_pos_0", &val)) {
		batt_meter_cust_data.q_max_pos_0 = (int)val;
		bm_debug("Get q_max_pos_0: %d\n",
			 batt_meter_cust_data.q_max_pos_0);
	} else {
		bm_err("Get q_max_pos_0 failed\n");
	}

	if (of_property_read_u32(np, "q_max_neg_10", &val)) {
		batt_meter_cust_data.q_max_neg_10 = (int)val;
		bm_debug("Get q_max_neg_10: %d\n",
			 batt_meter_cust_data.q_max_neg_10);
	} else {
		bm_err("Get q_max_neg_10 failed\n");
	}

	if (of_property_read_u32(np, "q_max_pos_50_h_current", &val)) {
		batt_meter_cust_data.q_max_pos_50_h_current = (int)val;
		bm_debug("Get q_max_pos_50_h_current: %d\n",
			 batt_meter_cust_data.q_max_pos_50_h_current);
	} else {
		bm_err("Get q_max_pos_50_h_current failed\n");
	}

	if (of_property_read_u32(np, "q_max_pos_25_h_current", &val)) {
		batt_meter_cust_data.q_max_pos_25_h_current = (int)val;
		bm_debug("Get q_max_pos_25_h_current: %d\n",
			 batt_meter_cust_data.q_max_pos_25_h_current);
	} else {
		bm_err("Get q_max_pos_25_h_current failed\n");
	}

	if (of_property_read_u32(np, "q_max_pos_0_h_current", &val)) {
		batt_meter_cust_data.q_max_pos_0_h_current = (int)val;
		bm_debug("Get q_max_pos_0_h_current: %d\n",
			 batt_meter_cust_data.q_max_pos_0_h_current);
	} else {
		bm_err("Get q_max_pos_0_h_current failed\n");
	}

	if (of_property_read_u32(np, "q_max_neg_10_h_current", &val)) {
		batt_meter_cust_data.q_max_neg_10_h_current = (int)val;
		bm_debug("Get q_max_neg_10_h_current: %d\n",
			 batt_meter_cust_data.q_max_neg_10_h_current);
	} else {
		bm_err("Get q_max_neg_10_h_current failed\n");
	}

	if (of_property_read_u32(np, "oam_d5", &val)) {
		batt_meter_cust_data.oam_d5 = (int)val;
		bm_debug("Get oam_d5: %d\n", batt_meter_cust_data.oam_d5);
	} else {
		bm_err("Get oam_d5 failed\n");
	}

	if (of_property_read_u32(np, "change_tracking_point", &val)) {
		batt_meter_cust_data.change_tracking_point = (int)val;
		bm_debug("Get change_tracking_point: %d\n",
			 batt_meter_cust_data.change_tracking_point);
	} else {
		bm_err("Get change_tracking_point failed\n");
	}

	if (of_property_read_u32(np, "cust_tracking_point", &val)) {
		batt_meter_cust_data.cust_tracking_point = (int)val;
		bm_debug("Get cust_tracking_point: %d\n",
			 batt_meter_cust_data.cust_tracking_point);
	} else {
		bm_err("Get cust_tracking_point failed\n");
	}

	if (of_property_read_u32(np, "cust_r_sense", &val)) {
		batt_meter_cust_data.cust_r_sense = (int)val;
		bm_debug("Get cust_r_sense: %d\n",
			 batt_meter_cust_data.cust_r_sense);
	} else {
		bm_err("Get cust_r_sense failed\n");
	}

	if (of_property_read_u32(np, "cust_hw_cc", &val)) {
		batt_meter_cust_data.cust_hw_cc = (int)val;
		bm_debug("Get cust_hw_cc: %d\n",
			 batt_meter_cust_data.cust_hw_cc);
	} else {
		bm_err("Get cust_hw_cc failed\n");
	}

	if (of_property_read_u32(np, "aging_tuning_value", &val)) {
		batt_meter_cust_data.aging_tuning_value = (int)val;
		bm_debug("Get aging_tuning_value: %d\n",
			 batt_meter_cust_data.aging_tuning_value);
	} else {
		bm_err("Get aging_tuning_value failed\n");
	}

	if (of_property_read_u32(np, "cust_r_fg_offset", &val)) {
		batt_meter_cust_data.cust_r_fg_offset = (int)val;
		bm_debug("Get cust_r_fg_offset: %d\n",
			 batt_meter_cust_data.cust_r_fg_offset);
	} else {
		bm_err("Get cust_r_fg_offset failed\n");
	}

	if (of_property_read_u32(np, "ocv_board_compesate", &val)) {
		batt_meter_cust_data.ocv_board_compesate = (int)val;
		bm_debug("Get ocv_board_compesate: %d\n",
			 batt_meter_cust_data.ocv_board_compesate);
	} else {
		bm_err("Get ocv_board_compesate failed\n");
	}

	if (of_property_read_u32(np, "r_fg_board_base", &val)) {
		batt_meter_cust_data.r_fg_board_base = (int)val;
		bm_debug("Get r_fg_board_base: %d\n",
			 batt_meter_cust_data.r_fg_board_base);
	} else {
		bm_err("Get r_fg_board_base failed\n");
	}

	if (of_property_read_u32(np, "r_fg_board_slope", &val)) {
		batt_meter_cust_data.r_fg_board_slope = (int)val;
		bm_debug("Get r_fg_board_slope: %d\n",
			 batt_meter_cust_data.r_fg_board_slope);
	} else {
		bm_err("Get r_fg_board_slope failed\n");
	}

	if (of_property_read_u32(np, "car_tune_value", &val)) {
		batt_meter_cust_data.car_tune_value = (int)val;
		bm_debug("Get car_tune_value: %d\n",
			 batt_meter_cust_data.car_tune_value);
	} else {
		bm_err("Get car_tune_value failed\n");
	}

	if (of_property_read_u32(np, "current_detect_r_fg", &val)) {
		batt_meter_cust_data.current_detect_r_fg = (int)val;
		bm_debug("Get current_detect_r_fg: %d\n",
			 batt_meter_cust_data.current_detect_r_fg);
	} else {
		bm_err("Get current_detect_r_fg failed\n");
	}

	if (of_property_read_u32(np, "minerroroffset", &val)) {
		batt_meter_cust_data.minerroroffset = (int)val;
		bm_debug("Get minerroroffset: %d\n",
			 batt_meter_cust_data.minerroroffset);
	} else {
		bm_err("Get minerroroffset failed\n");
	}

	if (of_property_read_u32(np, "fg_vbat_average_size", &val)) {
		batt_meter_cust_data.fg_vbat_average_size = (int)val;
		bm_debug("Get fg_vbat_average_size: %d\n",
			 batt_meter_cust_data.fg_vbat_average_size);
	} else {
		bm_err("Get fg_vbat_average_size failed\n");
	}

	if (of_property_read_u32(np, "r_fg_value", &val)) {
		batt_meter_cust_data.r_fg_value = (int)val;
		bm_debug("Get r_fg_value: %d\n",
			 batt_meter_cust_data.r_fg_value);
	} else {
		bm_err("Get r_fg_value failed\n");
	}

	/* TODO: update dt for new parameters */

	if (of_property_read_u32(np, "difference_hwocv_rtc", &val)) {
		batt_meter_cust_data.difference_hwocv_rtc = (int)val;
		bm_debug("Get difference_hwocv_rtc: %d\n",
			 batt_meter_cust_data.difference_hwocv_rtc);
	} else {
		bm_err("Get difference_hwocv_rtc failed\n");
	}

	if (of_property_read_u32(np, "difference_hwocv_swocv", &val)) {
		batt_meter_cust_data.difference_hwocv_swocv = (int)val;
		bm_debug("Get difference_hwocv_swocv: %d\n",
			 batt_meter_cust_data.difference_hwocv_swocv);
	} else {
		bm_err("Get difference_hwocv_swocv failed\n");
	}

	if (of_property_read_u32(np, "difference_swocv_rtc", &val)) {
		batt_meter_cust_data.difference_swocv_rtc = (int)val;
		bm_debug("Get difference_swocv_rtc: %d\n",
			 batt_meter_cust_data.difference_swocv_rtc);
	} else {
		bm_err("Get difference_swocv_rtc failed\n");
	}

	if (of_property_read_u32(np, "max_swocv", &val)) {
		batt_meter_cust_data.max_swocv = (int)val;
		bm_debug("Get max_swocv: %d\n", batt_meter_cust_data.max_swocv);
	} else {
		bm_err("Get max_swocv failed\n");
	}

	if (of_property_read_u32(np, "max_hwocv", &val)) {
		batt_meter_cust_data.max_hwocv = (int)val;
		bm_debug("Get max_hwocv: %d\n", batt_meter_cust_data.max_hwocv);
	} else {
		bm_err("Get max_hwocv failed\n");
	}

	if (of_property_read_u32(np, "max_vbat", &val)) {
		batt_meter_cust_data.max_vbat = (int)val;
		bm_debug("Get max_vbat: %d\n", batt_meter_cust_data.max_vbat);
	} else {
		bm_err("Get max_vbat failed\n");
	}

	if (of_property_read_u32(np, "difference_hwocv_vbat", &val)) {
		batt_meter_cust_data.difference_hwocv_vbat = (int)val;
		bm_debug("Get difference_hwocv_vbat: %d\n",
			 batt_meter_cust_data.difference_hwocv_vbat);
	} else {
		bm_err("Get difference_hwocv_vbat failed\n");
	}

	if (of_property_read_u32(np, "suspend_current_threshold", &val)) {
		batt_meter_cust_data.suspend_current_threshold = (int)val;
		bm_debug("Get suspend_current_threshold: %d\n",
			 batt_meter_cust_data.suspend_current_threshold);
	} else {
		bm_err("Get suspend_current_threshold failed\n");
	}

	if (of_property_read_u32(np, "ocv_check_time", &val)) {
		batt_meter_cust_data.ocv_check_time = (int)val;
		bm_debug("Get ocv_check_time: %d\n",
			 batt_meter_cust_data.ocv_check_time);
	} else {
		bm_err("Get ocv_check_time failed\n");
	}

	if (of_property_read_u32(np, "fixed_tbat_25", &val)) {
		batt_meter_cust_data.fixed_tbat_25 = (int)val;
		bm_debug("Get fixed_tbat_25: %d\n",
			 batt_meter_cust_data.fixed_tbat_25);
	} else {
		bm_err("Get fixed_tbat_25 failed\n");
	}

	batt_meter_cust_data.batterypseudo100 = BATTERYPSEUDO100;
	batt_meter_cust_data.batterypseudo1 = BATTERYPSEUDO1;

	if (of_property_read_u32(np, "vbat_normal_wakeup", &val)) {
		batt_meter_cust_data.vbat_normal_wakeup = (int)val;
		bm_debug("Get vbat_normal_wakeup: %d\n",
			 batt_meter_cust_data.vbat_normal_wakeup);
	} else {
		bm_err("Get vbat_normal_wakeup failed\n");
	}

	if (of_property_read_u32(np, "vbat_low_power_wakeup", &val)) {
		batt_meter_cust_data.vbat_low_power_wakeup = (int)val;
		bm_debug("Get vbat_low_power_wakeup: %d\n",
			 batt_meter_cust_data.vbat_low_power_wakeup);
	} else {
		bm_err("Get vbat_low_power_wakeup failed\n");
	}

	if (of_property_read_u32(np, "normal_wakeup_period", &val)) {
		batt_meter_cust_data.normal_wakeup_period = (int)val;
		bm_debug("Get normal_wakeup_period: %d\n",
			 batt_meter_cust_data.normal_wakeup_period);
	} else {
		bm_err("Get normal_wakeup_period failed\n");
	}

	if (of_property_read_u32(np, "low_power_wakeup_period", &val)) {
		batt_meter_cust_data.low_power_wakeup_period = (int)val;
		bm_debug("Get low_power_wakeup_period: %d\n",
			 batt_meter_cust_data.low_power_wakeup_period);
	} else {
		bm_err("Get low_power_wakeup_period failed\n");
	}

	if (of_property_read_u32(np, "close_poweroff_wakeup_period", &val)) {
		batt_meter_cust_data.close_poweroff_wakeup_period = (int)val;
		bm_debug("Get close_poweroff_wakeup_period: %d\n",
			 batt_meter_cust_data.close_poweroff_wakeup_period);
	} else {
		bm_err("Get close_poweroff_wakeup_period failed\n");
	}

	if (of_property_read_u32(np, "init_soc_by_sw_soc", &val)) {
		batt_meter_cust_data.init_soc_by_sw_soc = (int)val;
		bm_debug("Get init_soc_by_sw_soc: %d\n",
			 batt_meter_cust_data.init_soc_by_sw_soc);
	} else {
		bm_err("Get init_soc_by_sw_soc failed\n");
	}

	if (of_property_read_u32(np, "sync_ui_soc_imm", &val)) {
		batt_meter_cust_data.sync_ui_soc_imm = (int)val;
		bm_debug("Get sync_ui_soc_imm: %d\n",
			 batt_meter_cust_data.sync_ui_soc_imm);
	} else {
		bm_err("Get sync_ui_soc_imm failed\n");
	}

	if (of_property_read_u32(np, "mtk_enable_aging_algorithm", &val)) {
		batt_meter_cust_data.mtk_enable_aging_algorithm = (int)val;
		bm_debug("Get mtk_enable_aging_algorithm: %d\n",
			 batt_meter_cust_data.mtk_enable_aging_algorithm);
	} else {
		bm_err("Get mtk_enable_aging_algorithm failed\n");
	}

	if (of_property_read_u32(np, "md_sleep_current_check", &val)) {
		batt_meter_cust_data.md_sleep_current_check = (int)val;
		bm_debug("Get md_sleep_current_check: %d\n",
			 batt_meter_cust_data.md_sleep_current_check);
	} else {
		bm_err("Get md_sleep_current_check failed\n");
	}

	if (of_property_read_u32(np, "q_max_by_current", &val)) {
		batt_meter_cust_data.q_max_by_current = (int)val;
		bm_debug("Get q_max_by_current: %d\n",
			 batt_meter_cust_data.q_max_by_current);
	} else {
		bm_err("Get q_max_by_current failed\n");
	}
	if (of_property_read_u32(np, "q_max_sys_voltage", &val)) {
		batt_meter_cust_data.q_max_sys_voltage = (int)val;
		bm_debug("Get q_max_sys_voltage: %d\n",
			 batt_meter_cust_data.q_max_sys_voltage);
	} else {
		bm_err("Get q_max_sys_voltage failed\n");
	}

	if (of_property_read_u32(np, "shutdown_gauge0", &val)) {
		batt_meter_cust_data.shutdown_gauge0 = (int)val;
		bm_debug("Get shutdown_gauge0: %d\n",
			 batt_meter_cust_data.shutdown_gauge0);
	} else {
		bm_err("Get shutdown_gauge0 failed\n");
	}

	if (of_property_read_u32(np, "shutdown_gauge1_xmins", &val)) {
		batt_meter_cust_data.shutdown_gauge1_xmins = (int)val;
		bm_debug("Get shutdown_gauge1_xmins: %d\n",
			 batt_meter_cust_data.shutdown_gauge1_xmins);
	} else {
		bm_err("Get shutdown_gauge1_xmins failed\n");
	}

	if (of_property_read_u32(np, "shutdown_gauge1_mins", &val)) {
		batt_meter_cust_data.shutdown_gauge1_mins = (int)val;
		bm_debug("Get shutdown_gauge1_mins: %d\n",
			 batt_meter_cust_data.shutdown_gauge1_mins);
	} else {
		bm_err("Get shutdown_gauge1_mins failed\n");
	}

	return 0;
}

int batt_meter_init_cust_data(struct platform_device *dev)
{
	/* #ifdef CONFIG_OF */
	/* return __batt_meter_init_cust_data_from_dt(dev); */
	/* #else */
	return __batt_meter_init_cust_data_from_cust_header(dev);
	/* #endif */
}

int get_r_fg_value(void)
{
	return (R_FG_VALUE + CUST_R_FG_OFFSET);
}

#ifdef MTK_MULTI_BAT_PROFILE_SUPPORT
int BattThermistorConverTemp(int Res)
{
	int i = 0;
	int RES1 = 0, RES2 = 0;
	int TBatt_Value = -200, TMP1 = 0, TMP2 = 0;
	BATT_TEMPERATURE *batt_temperature_table =
		&Batt_Temperature_Table[g_fg_battery_id];

	if (Res >= batt_temperature_table[0].TemperatureR) {
		TBatt_Value = -20;
	} else if (Res <= batt_temperature_table[16].TemperatureR) {
		TBatt_Value = 60;
	} else {
		RES1 = batt_temperature_table[0].TemperatureR;
		TMP1 = batt_temperature_table[0].BatteryTemp;

		for (i = 0; i <= 16; i++) {
			if (Res >= batt_temperature_table[i].TemperatureR) {
				RES2 = batt_temperature_table[i].TemperatureR;
				TMP2 = batt_temperature_table[i].BatteryTemp;
				break;
			}

			RES1 = batt_temperature_table[i].TemperatureR;
			TMP1 = batt_temperature_table[i].BatteryTemp;
		}

		TBatt_Value = (((Res - RES2) * TMP1) + ((RES1 - Res) * TMP2)) /
			      (RES1 - RES2);
	}

	return TBatt_Value;
}

signed int fgauge_get_Q_max(signed short temperature)
{
	signed int ret_Q_max = 0;
	signed int low_temperature = 0, high_temperature = 0;
	signed int low_Q_max = 0, high_Q_max = 0;
	signed int tmp_Q_max_1 = 0, tmp_Q_max_2 = 0;

	if (temperature <= TEMPERATURE_T1) {
		low_temperature = (-10);
		tmp_Q_max_1 = g_Q_MAX_NEG_10[g_fg_battery_id];
		high_temperature = TEMPERATURE_T1;
		tmp_Q_max_2 = g_Q_MAX_POS_0[g_fg_battery_id];

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else if (temperature <= TEMPERATURE_T2) {
		low_temperature = TEMPERATURE_T1;
		tmp_Q_max_1 = g_Q_MAX_POS_0[g_fg_battery_id];
		high_temperature = TEMPERATURE_T2;
		tmp_Q_max_2 = g_Q_MAX_POS_25[g_fg_battery_id];

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else {
		low_temperature = TEMPERATURE_T2;
		tmp_Q_max_1 = g_Q_MAX_POS_25[g_fg_battery_id];
		high_temperature = TEMPERATURE_T3;
		tmp_Q_max_2 = g_Q_MAX_POS_50[g_fg_battery_id];

		if (temperature > high_temperature)
			temperature = high_temperature;
	}

	if (tmp_Q_max_1 <= tmp_Q_max_2) {
		low_Q_max = tmp_Q_max_1;
		high_Q_max = tmp_Q_max_2;
		ret_Q_max = low_Q_max +
			    ((((temperature - low_temperature) *
			       (high_Q_max - low_Q_max) * 10) /
				      (high_temperature - low_temperature) +
			      5) /
			     10);
	} else {
		low_Q_max = tmp_Q_max_2;
		high_Q_max = tmp_Q_max_1;
		ret_Q_max = low_Q_max +
			    ((((high_temperature - temperature) *
			       (high_Q_max - low_Q_max) * 10) /
				      (high_temperature - low_temperature) +
			      5) /
			     10);
	}

	bm_trace("[%s] Q_max = %d\r\n", __func__, ret_Q_max);

	return ret_Q_max;
}

signed int fgauge_get_Q_max_high_current(signed short temperature)
{
	signed int ret_Q_max = 0;
	signed int low_temperature = 0, high_temperature = 0;
	signed int low_Q_max = 0, high_Q_max = 0;
	signed int tmp_Q_max_1 = 0, tmp_Q_max_2 = 0;

	if (temperature <= TEMPERATURE_T1) {
		low_temperature = (-10);
		tmp_Q_max_1 = g_Q_MAX_NEG_10_H_CURRENT[g_fg_battery_id];
		high_temperature = TEMPERATURE_T1;
		tmp_Q_max_2 = g_Q_MAX_POS_0_H_CURRENT[g_fg_battery_id];

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else if (temperature <= TEMPERATURE_T2) {
		low_temperature = TEMPERATURE_T1;
		tmp_Q_max_1 = g_Q_MAX_POS_0_H_CURRENT[g_fg_battery_id];
		high_temperature = TEMPERATURE_T2;
		tmp_Q_max_2 = g_Q_MAX_POS_25_H_CURRENT[g_fg_battery_id];

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else {
		low_temperature = TEMPERATURE_T2;
		tmp_Q_max_1 = g_Q_MAX_POS_25_H_CURRENT[g_fg_battery_id];
		high_temperature = TEMPERATURE_T3;
		tmp_Q_max_2 = g_Q_MAX_POS_50_H_CURRENT[g_fg_battery_id];

		if (temperature > high_temperature)
			temperature = high_temperature;
	}

	if (tmp_Q_max_1 <= tmp_Q_max_2) {
		low_Q_max = tmp_Q_max_1;
		high_Q_max = tmp_Q_max_2;
		ret_Q_max = low_Q_max +
			    ((((temperature - low_temperature) *
			       (high_Q_max - low_Q_max) * 10) /
				      (high_temperature - low_temperature) +
			      5) /
			     10);
	} else {
		low_Q_max = tmp_Q_max_2;
		high_Q_max = tmp_Q_max_1;
		ret_Q_max = low_Q_max +
			    ((((high_temperature - temperature) *
			       (high_Q_max - low_Q_max) * 10) /
				      (high_temperature - low_temperature) +
			      5) /
			     10);
	}

	bm_trace("[%s] Q_max = %d\r\n", __func__, ret_Q_max);

	return ret_Q_max;
}

#else

int BattThermistorConverTemp(int Res)
{
	int i = 0;
	int RES1 = 0, RES2 = 0;
	int TBatt_Value = -200, TMP1 = 0, TMP2 = 0;

	if (Res >= Batt_Temperature_Table[0].TemperatureR) {
		TBatt_Value = -20;
	} else if (Res <= Batt_Temperature_Table[16].TemperatureR) {
		TBatt_Value = 60;
	} else {
		RES1 = Batt_Temperature_Table[0].TemperatureR;
		TMP1 = Batt_Temperature_Table[0].BatteryTemp;

		for (i = 0; i <= 16; i++) {
			if (Res >= Batt_Temperature_Table[i].TemperatureR) {
				RES2 = Batt_Temperature_Table[i].TemperatureR;
				TMP2 = Batt_Temperature_Table[i].BatteryTemp;
				break;
			}
			RES1 = Batt_Temperature_Table[i].TemperatureR;
			TMP1 = Batt_Temperature_Table[i].BatteryTemp;
		}

		TBatt_Value = (((Res - RES2) * TMP1) + ((RES1 - Res) * TMP2)) /
			      (RES1 - RES2);
	}

	return TBatt_Value;
}

signed int fgauge_get_Q_max(signed short temperature)
{
	signed int ret_Q_max = 0;
	signed int low_temperature = 0, high_temperature = 0;
	signed int low_Q_max = 0, high_Q_max = 0;
	signed int tmp_Q_max_1 = 0, tmp_Q_max_2 = 0;

	if (temperature <= TEMPERATURE_T1) {
		low_temperature = (-10);
		tmp_Q_max_1 = Q_MAX_NEG_10;
		high_temperature = TEMPERATURE_T1;
		tmp_Q_max_2 = Q_MAX_POS_0;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else if (temperature <= TEMPERATURE_T2) {
		low_temperature = TEMPERATURE_T1;
		tmp_Q_max_1 = Q_MAX_POS_0;
		high_temperature = TEMPERATURE_T2;
		tmp_Q_max_2 = Q_MAX_POS_25;

		/* fix coverity wont reach here */
		/* if (temperature < low_temperature)  */
		/*	temperature = low_temperature; */

	} else {
		low_temperature = TEMPERATURE_T2;
		tmp_Q_max_1 = Q_MAX_POS_25;
		high_temperature = TEMPERATURE_T3;
		tmp_Q_max_2 = Q_MAX_POS_50;

		if (temperature > high_temperature)
			temperature = high_temperature;
	}

	if (tmp_Q_max_1 <= tmp_Q_max_2) {
		low_Q_max = tmp_Q_max_1;
		high_Q_max = tmp_Q_max_2;
		ret_Q_max = low_Q_max +
			    ((((temperature - low_temperature) *
			       (high_Q_max - low_Q_max) * 10) /
				      (high_temperature - low_temperature) +
			      5) /
			     10);
	}
	/* fix coverity */
	tmp_Q_max_1 = tmp_Q_max_1 + fix_coverity1;
	tmp_Q_max_2 = tmp_Q_max_2 + fix_coverity2;
	if (tmp_Q_max_1 > tmp_Q_max_2) {
		low_Q_max = tmp_Q_max_2;
		high_Q_max = tmp_Q_max_1;
		ret_Q_max = low_Q_max +
			    ((((high_temperature - temperature) *
			       (high_Q_max - low_Q_max) * 10) /
				      (high_temperature - low_temperature) +
			      5) /
			     10);
	}

	bm_trace("[%s] Q_max = %d\r\n", __func__, ret_Q_max);

	return ret_Q_max;
}

signed int fgauge_get_Q_max_high_current(signed short temperature)
{
	signed int ret_Q_max = 0;
	signed int low_temperature = 0, high_temperature = 0;
	signed int low_Q_max = 0, high_Q_max = 0;
	signed int tmp_Q_max_1 = 0, tmp_Q_max_2 = 0;

	if (temperature <= TEMPERATURE_T1) {
		low_temperature = (-10);
		tmp_Q_max_1 = Q_MAX_NEG_10_H_CURRENT;
		high_temperature = TEMPERATURE_T1;
		tmp_Q_max_2 = Q_MAX_POS_0_H_CURRENT;

		if (temperature < low_temperature)
			temperature = low_temperature;

	} else if (temperature <= TEMPERATURE_T2) {
		low_temperature = TEMPERATURE_T1;
		tmp_Q_max_1 = Q_MAX_POS_0_H_CURRENT;
		high_temperature = TEMPERATURE_T2;
		tmp_Q_max_2 = Q_MAX_POS_25_H_CURRENT;

		/* fix coverity , should not reach here */
		/* if (temperature < low_temperature)   */
		/*	temperature = low_temperature;  */

	} else {
		low_temperature = TEMPERATURE_T2;
		tmp_Q_max_1 = Q_MAX_POS_25_H_CURRENT;
		high_temperature = TEMPERATURE_T3;
		tmp_Q_max_2 = Q_MAX_POS_50_H_CURRENT;

		if (temperature > high_temperature)
			temperature = high_temperature;
	}

	if (tmp_Q_max_1 <= tmp_Q_max_2) {
		low_Q_max = tmp_Q_max_1;
		high_Q_max = tmp_Q_max_2;
		ret_Q_max = low_Q_max +
			    ((((temperature - low_temperature) *
			       (high_Q_max - low_Q_max) * 10) /
				      (high_temperature - low_temperature) +
			      5) /
			     10);
	}
	/* fix coverity */
	tmp_Q_max_1 = tmp_Q_max_1 + fix_coverity1;
	tmp_Q_max_2 = tmp_Q_max_2 + fix_coverity2;

	if (tmp_Q_max_1 > tmp_Q_max_2) {
		low_Q_max = tmp_Q_max_2;
		high_Q_max = tmp_Q_max_1;
		ret_Q_max = low_Q_max +
			    ((((high_temperature - temperature) *
			       (high_Q_max - low_Q_max) * 10) /
				      (high_temperature - low_temperature) +
			      5) /
			     10);
	}

	bm_trace("[%s] Q_max = %d\r\n", __func__, ret_Q_max);

	return ret_Q_max;
}

#endif

int BattVoltToTemp(int dwVolt)
{
	long long TRes_temp;
	long long TRes;
	int sBaTTMP = -100;

	/* TRes_temp = ((long long)RBAT_PULL_UP_R*(long long)dwVolt) / */
	/* (RBAT_PULL_UP_VOLT-dwVolt); */
	/* TRes = (TRes_temp * (long long)RBAT_PULL_DOWN_R)/((long */
	/* long)RBAT_PULL_DOWN_R - TRes_temp); */

	TRes_temp = (RBAT_PULL_UP_R * (long long)dwVolt);
#ifdef RBAT_PULL_UP_VOLT_BY_BIF
	do_div(TRes_temp, (pmic_get_vbif28_volt() - dwVolt));
/* bm_debug("[RBAT_PULL_UP_VOLT_BY_BIF] vbif28:%d\n",pmic_get_vbif28_volt()); */
#else
	do_div(TRes_temp, (RBAT_PULL_UP_VOLT - dwVolt));
#endif

#ifdef RBAT_PULL_DOWN_R
	TRes = (TRes_temp * RBAT_PULL_DOWN_R);
	do_div(TRes, abs(RBAT_PULL_DOWN_R - TRes_temp));
#else
	TRes = TRes_temp;
#endif

	/* convert register to temperature */
	sBaTTMP = BattThermistorConverTemp((int)TRes);

#ifdef RBAT_PULL_UP_VOLT_BY_BIF
	bm_debug("[%s] %d %d\n", __func__, RBAT_PULL_UP_R,
		 pmic_get_vbif28_volt());
#else
	bm_debug("[%s] %d\n", __func__, RBAT_PULL_UP_R);
#endif

	return sBaTTMP;
}

int force_get_tbat(kal_bool update)
{
#if defined(CONFIG_POWER_EXT) || defined(FIXED_TBAT_25)
	bm_debug("[%s] fixed TBAT=25 t\n", __func__);
	return 25;
#else

	int bat_temperature_volt = 0;
	int bat_temperature_val = 0;
	static int pre_bat_temperature_val = -1;
	int fg_r_value = 0;
	signed int fg_current_temp = 0;
	kal_bool fg_current_state = KAL_FALSE;
	int bat_temperature_volt_temp = 0;
	int ret = 0;

	if (batt_meter_cust_data.fixed_tbat_25) {
		bm_err("[%s] fixed TBAT=25 t\n", __func__);
		return 25;
	}

	if (update == KAL_TRUE || pre_bat_temperature_val == -1) {
		/* Get V_BAT_Temperature */
		bat_temperature_volt = 2;
		ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_BAT_TEMP,
					 &bat_temperature_volt);

		if (bat_temperature_volt != 0) {
#if defined(SOC_BY_HW_FG)
			fg_r_value = get_r_fg_value();

			ret = battery_meter_ctrl(
				BATTERY_METER_CMD_GET_HW_FG_CURRENT,
				&fg_current_temp);
			ret = battery_meter_ctrl(
				BATTERY_METER_CMD_GET_HW_FG_CURRENT_SIGN,
				&fg_current_state);
			fg_current_temp = fg_current_temp / 10;

			if (fg_current_state == KAL_TRUE) {
				bat_temperature_volt_temp =
					bat_temperature_volt;
				bat_temperature_volt =
					bat_temperature_volt -
					((fg_current_temp * fg_r_value) / 1000);
			} else {
				bat_temperature_volt_temp =
					bat_temperature_volt;
				bat_temperature_volt =
					bat_temperature_volt +
					((fg_current_temp * fg_r_value) / 1000);
			}
#endif

			bat_temperature_val =
				BattVoltToTemp(bat_temperature_volt);
		}

#ifdef CONFIG_MTK_BIF_SUPPORT
		battery_charging_control(CHARGING_CMD_GET_BIF_TBAT,
					 &bat_temperature_val);
#endif

		if ((bat_temperature_val > 55) || (bat_temperature_val < 10)) {
			bm_err("[%s] %d,%d,%d,%d,%d,%d\n", __func__
			       bat_temperature_volt_temp, bat_temperature_volt,
			       fg_current_state, fg_current_temp, fg_r_value,
			       bat_temperature_val);
		}
		pre_bat_temperature_val = bat_temperature_val;
	} else {
		bat_temperature_val = pre_bat_temperature_val;
	}
	return bat_temperature_val;
#endif
}
EXPORT_SYMBOL(force_get_tbat);

#if defined(SOC_BY_HW_FG)
void update_fg_dbg_tool_value(void)
{
	g_fg_dbg_bat_volt = gFG_voltage_init;

	if (gFG_Is_Charging == KAL_TRUE)
		g_fg_dbg_bat_current = 1 - gFG_current - 1;
	else
		g_fg_dbg_bat_current = gFG_current;

	g_fg_dbg_bat_zcv = gFG_voltage;

	g_fg_dbg_bat_temp = gFG_temp;

	g_fg_dbg_bat_r = gFG_resistance_bat;

	g_fg_dbg_bat_car = gFG_coulomb_act;

	g_fg_dbg_bat_qmax = gFG_BATT_CAPACITY_aging;

	g_fg_dbg_d0 = gFG_DOD0;

	g_fg_dbg_d1 = gFG_DOD1;

	g_fg_dbg_percentage = bat_get_ui_percentage();

	g_fg_dbg_percentage_fg = gFG_capacity_by_c;

	g_fg_dbg_percentage_voltmode = gfg_percent_check_point;
}

void fgauge_algo_run_get_init_data(void)
{
#if defined(INIT_BAT_CUR_FROM_PTIM)
	unsigned int bat = 0;
	signed int cur = 0;
#else
	int ret = 0;
#endif
	kal_bool charging_enable = KAL_FALSE;

#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING) && !defined(SWCHR_POWER_PATH)
	if (get_boot_mode() != LOW_POWER_OFF_CHARGING_BOOT)
#endif
		/*stop charging for vbat measurement */
		battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);

	msleep(50);
/* 1. Get Raw Data */
#if defined(INIT_BAT_CUR_FROM_PTIM)
	do_ptim_ex(true, &bat, &cur);
	gFG_voltage_init = bat / 10;
	gFG_current_init = abs(cur);
	if (cur > 0)
		gFG_Is_Charging_init = KAL_FALSE;
	else
		gFG_Is_Charging_init = KAL_TRUE;
#else
	gFG_voltage_init = battery_meter_get_battery_voltage(KAL_TRUE);
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT,
				 &gFG_current_init);
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT_SIGN,
				 &gFG_Is_Charging_init);
#endif
	charging_enable = KAL_TRUE;
	battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);
	bm_info("1.[%s](gFG_voltage_init %d, gFG_current_init %d, gFG_Is_Charging_init %d)\n",
		__func__, gFG_voltage_init,
		gFG_current_init, gFG_Is_Charging_init);
	mt_battery_set_init_vol(gFG_voltage_init);
}
#endif

#if defined(SOC_BY_SW_FG)
void update_fg_dbg_tool_value(void)
{
}

void fgauge_algo_run_get_init_data(void)
{
	kal_bool charging_enable = KAL_FALSE;

#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING) && !defined(SWCHR_POWER_PATH)
	if (get_boot_mode() != LOW_POWER_OFF_CHARGING_BOOT)
#endif
		/*stop charging for vbat measurement */
		battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);

	msleep(50);
	/* 1. Get Raw Data */
	gFG_voltage_init = battery_meter_get_battery_voltage(KAL_TRUE);
	gFG_current_init = FG_CURRENT_INIT_VALUE;
	gFG_Is_Charging_init = KAL_FALSE;
	charging_enable = KAL_TRUE;
	battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);
	bm_info("1.[%s](gFG_voltage_init %d, gFG_current_init %d, gFG_Is_Charging_init %d)\n",
		__func__, gFG_voltage_init,
		gFG_current_init, gFG_Is_Charging_init);
}
#endif

signed int get_dynamic_period(int first_use, int first_wakeup_time,
			      int battery_capacity_level)
{
#if defined(CONFIG_POWER_EXT)

	return first_wakeup_time;

#elif defined(SOC_BY_AUXADC) || defined(SOC_BY_SW_FG)

#if (CONFIG_MTK_GAUGE_VERSION == 20)

	signed int vbat_val = 0;

	vbat_val = g_sw_vbat_temp;

#ifdef CONFIG_MTK_POWER_EXT_DETECT
	if (bat_is_ext_power() == KAL_TRUE)
		return NORMAL_WAKEUP_PERIOD;
#endif

	if (wake_up_smooth_time == 0) {
		/* change wake up period when system suspend. */
		if (vbat_val > VBAT_NORMAL_WAKEUP)	     /* 3.6v */
			g_spm_timer = NORMAL_WAKEUP_PERIOD;    /* 90 min */
		else if (vbat_val > VBAT_LOW_POWER_WAKEUP)     /* 3.5v */
			g_spm_timer = LOW_POWER_WAKEUP_PERIOD; /* 5 min */
		else
			g_spm_timer =
				CLOSE_POWEROFF_WAKEUP_PERIOD; /* 0.5 min */
	} else
		g_spm_timer = wake_up_smooth_time;

	bm_print(BM_LOG_CRTI,
		 "vbat_val=%d, g_spm_timer=%d wake_up_smooth_time=%d\n",
		 vbat_val, g_spm_timer, wake_up_smooth_time);

	return g_spm_timer;
#else

	signed int vbat_val = 0;

	vbat_val = g_sw_vbat_temp;

#ifdef CONFIG_MTK_POWER_EXT_DETECT
	if (bat_is_ext_power() == KAL_TRUE)
		return NORMAL_WAKEUP_PERIOD;
#endif

	/* change wake up period when system suspend. */
	if (vbat_val > VBAT_NORMAL_WAKEUP)	     /* 3.6v */
		g_spm_timer = NORMAL_WAKEUP_PERIOD;    /* 90 min */
	else if (vbat_val > VBAT_LOW_POWER_WAKEUP)     /* 3.5v */
		g_spm_timer = LOW_POWER_WAKEUP_PERIOD; /* 5 min */
	else
		g_spm_timer = CLOSE_POWEROFF_WAKEUP_PERIOD; /* 0.5 min */

	bm_debug("vbat_val=%d, g_spm_timer=%d\n", vbat_val, g_spm_timer);

	return g_spm_timer;
#endif
#else
#ifdef FG_BAT_INT

	int ret = 0;
	signed int car_instant = 0;

	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR_ACT,
				 &car_instant);

	if (wake_up_smooth_time == 0)
		g_spm_timer = NORMAL_WAKEUP_PERIOD;
	else
		g_spm_timer = wake_up_smooth_time;

	bm_print(
		BM_LOG_CRTI,
		"[%s] g_spm_timer:%d wake_up_smooth_time:%d vbat:%d car:%d\r\n",
		__func__, g_spm_timer, wake_up_smooth_time,
		g_sw_vbat_temp, car_instant);
	return g_spm_timer;

#else
	signed int car_instant = 0;
	signed int current_instant = 0;
	static signed int car_sleep = 0x12345678;
	signed int car_wakeup = 0;

	signed int ret_val = -1;
	signed int I_sleep = 0;
	signed int new_time = 0;
	signed int vbat_val = 0;
	int ret = 0;

	vbat_val = g_sw_vbat_temp;

	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT,
				 &current_instant);

	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR_ACT,
				 &car_instant);

	if (car_instant < 0)
		car_instant = car_instant - (car_instant * 2);

	/* 3.6v */
	if (vbat_val > VBAT_NORMAL_WAKEUP) {
		static unsigned int pre_time;

		car_wakeup = car_instant;
		if (car_sleep > car_wakeup || car_sleep == 0x12345678) {
			car_sleep = car_wakeup;
			bm_debug("[%s] reset car_sleep\n",
			__func__);
		}
		if (last_time == 0) {
			last_time = 1;
		} else {
			/* mt_battery_update_time(&car_time); */
			/* add_time = mt_battery_get_duration_time(); */
			if (car_wakeup == car_sleep) {
				pre_time += add_time;
				last_time = pre_time;
			} else {
				last_time = pre_time + add_time;
				pre_time = 0;
			}
		}

		I_sleep = (((car_wakeup - car_sleep) * 3600) / last_time + 5) /
			  10; /* unit: second */

		if (I_sleep == 0) {
			new_time = g_spm_timer;
		} else {

			new_time = ((gFG_BATT_CAPACITY_aging *
				     battery_capacity_level * 3600) /
				    100) /
				   I_sleep;
		}

		bm_print(
			BM_LOG_CRTI,
			"[%s] car_instant=%d, car_wakeup=%d, car_sleep=%d, I_sleep=%d, gFG_BATT_CAPACITY=%d, add_time=%d, last_time=%d, new_time=%d , battery_capacity_level = %d\r\n",
			__func__, car_instant, car_wakeup, car_sleep, I_sleep,
			gFG_BATT_CAPACITY_aging, add_time, last_time, new_time,
			battery_capacity_level);
		if (new_time > 1800)
			new_time = 1800;
		ret_val = new_time;

		if (ret_val == 0)
			ret_val = first_wakeup_time;

		/* update parameter */
		car_sleep = car_wakeup;
		g_spm_timer = ret_val;
	} else if (vbat_val > VBAT_LOW_POWER_WAKEUP) { /* 3.5v */
		g_spm_timer = LOW_POWER_WAKEUP_PERIOD; /* 5 min */
	} else {
		g_spm_timer = CLOSE_POWEROFF_WAKEUP_PERIOD; /* 0.5 min */
	}

	bm_debug("vbat_val=%d, g_spm_timer=%d\n", vbat_val, g_spm_timer);
	return g_spm_timer;
#endif
#endif
}

/* ============================================================ // */
signed int battery_meter_get_battery_voltage(kal_bool update)
{
	int ret = 0;
	int val = 5;
	static int pre_val = -1;

	if (update == KAL_TRUE || pre_val == -1) {
		val = 5; /* set avg times */
		ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_BAT_SENSE,
					 &val);
		pre_val = val;
	} else {
		val = pre_val;
	}
	g_sw_vbat_temp = val;

#ifdef MTK_BATTERY_LIFETIME_DATA_SUPPORT
	if (g_sw_vbat_temp > gFG_max_voltage)
		gFG_max_voltage = g_sw_vbat_temp;

	if (g_sw_vbat_temp < gFG_min_voltage)
		gFG_min_voltage = g_sw_vbat_temp;
#endif

	return val;
}

int battery_meter_get_low_battery_interrupt_status(void)
{
	int is_lbat_int_trigger;
	int ret;

	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_LOW_BAT_INTERRUPT_STATUS,
				 &is_lbat_int_trigger);

	if (ret != 0)
		return KAL_FALSE;

	return is_lbat_int_trigger;
}

signed int battery_meter_get_charging_current_imm(void)
{
#ifdef AUXADC_SUPPORT_IMM_CURRENT_MODE
	return PMIC_IMM_GetCurrent();
#else
	int ret;
	signed int ADC_I_SENSE = 1;   /* 1 measure time */
	signed int ADC_BAT_SENSE = 1; /* 1 measure time */
	int ICharging = 0;

	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_BAT_SENSE,
				 &ADC_BAT_SENSE);
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_I_SENSE,
				 &ADC_I_SENSE);

	ICharging = (ADC_I_SENSE - ADC_BAT_SENSE + g_I_SENSE_offset) * 1000 /
		    CUST_R_SENSE;
	return ICharging;
#endif
}

signed int battery_meter_get_charging_current(void)
{
#ifdef DISABLE_CHARGING_CURRENT_MEASURE
	return 0;
#elif defined(AUXADC_SUPPORT_IMM_CURRENT_MODE)
	return PMIC_IMM_GetCurrent();
#elif !defined(EXTERNAL_SWCHR_SUPPORT)
	signed int ADC_BAT_SENSE_tmp[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
					    0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	signed int ADC_BAT_SENSE_sum = 0;
	signed int ADC_BAT_SENSE = 0;
	signed int ADC_I_SENSE_tmp[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
					  0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	signed int ADC_I_SENSE_sum = 0;
	signed int ADC_I_SENSE = 0;
	int repeat = 20;
	int i = 0;
	int j = 0;
	signed int temp = 0;
	int ICharging = 0;
	int ret = 0;
	int val = 1;

	for (i = 0; i < repeat; i++) {
		val = 1; /* set avg times */
		ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_BAT_SENSE,
					 &val);
		ADC_BAT_SENSE_tmp[i] = val;

		val = 1; /* set avg times */
		ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_I_SENSE,
					 &val);
		ADC_I_SENSE_tmp[i] = val;

		ADC_BAT_SENSE_sum += ADC_BAT_SENSE_tmp[i];
		ADC_I_SENSE_sum += ADC_I_SENSE_tmp[i];
	}

	/* sorting    BAT_SENSE */
	for (i = 0; i < repeat; i++) {
		for (j = i; j < repeat; j++) {
			if (ADC_BAT_SENSE_tmp[j] < ADC_BAT_SENSE_tmp[i]) {
				temp = ADC_BAT_SENSE_tmp[j];
				ADC_BAT_SENSE_tmp[j] = ADC_BAT_SENSE_tmp[i];
				ADC_BAT_SENSE_tmp[i] = temp;
			}
		}
	}

	bm_trace("[g_Get_I_Charging:BAT_SENSE]\r\n");
	for (i = 0; i < repeat; i++)
		bm_trace("%d,", ADC_BAT_SENSE_tmp[i]);
	bm_trace("\r\n");

	/* sorting    I_SENSE */
	for (i = 0; i < repeat; i++) {
		for (j = i; j < repeat; j++) {
			if (ADC_I_SENSE_tmp[j] < ADC_I_SENSE_tmp[i]) {
				temp = ADC_I_SENSE_tmp[j];
				ADC_I_SENSE_tmp[j] = ADC_I_SENSE_tmp[i];
				ADC_I_SENSE_tmp[i] = temp;
			}
		}
	}

	bm_trace("[g_Get_I_Charging:I_SENSE]\r\n");
	for (i = 0; i < repeat; i++)
		bm_trace("%d,", ADC_I_SENSE_tmp[i]);
	bm_trace("\r\n");

	ADC_BAT_SENSE_sum -= ADC_BAT_SENSE_tmp[0];
	ADC_BAT_SENSE_sum -= ADC_BAT_SENSE_tmp[1];
	ADC_BAT_SENSE_sum -= ADC_BAT_SENSE_tmp[18];
	ADC_BAT_SENSE_sum -= ADC_BAT_SENSE_tmp[19];
	ADC_BAT_SENSE = ADC_BAT_SENSE_sum / (repeat - 4);

	bm_trace("[g_Get_I_Charging] ADC_BAT_SENSE=%d\r\n", ADC_BAT_SENSE);

	ADC_I_SENSE_sum -= ADC_I_SENSE_tmp[0];
	ADC_I_SENSE_sum -= ADC_I_SENSE_tmp[1];
	ADC_I_SENSE_sum -= ADC_I_SENSE_tmp[18];
	ADC_I_SENSE_sum -= ADC_I_SENSE_tmp[19];
	ADC_I_SENSE = ADC_I_SENSE_sum / (repeat - 4);

	bm_trace("[g_Get_I_Charging] ADC_I_SENSE(Before)=%d\r\n", ADC_I_SENSE);

	bm_trace("[g_Get_I_Charging] ADC_I_SENSE(After)=%d\r\n", ADC_I_SENSE);

	if (ADC_I_SENSE > ADC_BAT_SENSE)
		ICharging = (ADC_I_SENSE - ADC_BAT_SENSE + g_I_SENSE_offset) *
			    1000 / CUST_R_SENSE;
	else
		ICharging = 0;

	return ICharging;
#else
	return 0;
#endif
}

signed int battery_meter_get_battery_current(void)
{
	int ret = 0;
	signed int val = 0;

	if (g_auxadc_solution == 1)
		val = oam_i_2;
	else
		ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT,
					 &val);

	return val;
}

kal_bool battery_meter_get_battery_current_sign(void)
{
	int ret = 0;
	kal_bool val = 0;

	if (g_auxadc_solution == 1)
		val = 0; /* discharging */
	else
		ret = battery_meter_ctrl(
			BATTERY_METER_CMD_GET_HW_FG_CURRENT_SIGN, &val);

	return val;
}

signed int battery_meter_get_car(void)
{
	int ret = 0;
	signed int val = 0;

	if (g_auxadc_solution == 1)
		val = oam_car_2;
	else {
		ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR_ACT,
					 &val);
		if (val < 0)
			val = (val - 5) / 10;
		else
			val = (val + 5) / 10;
	}

	return val;
}

signed int battery_meter_get_battery_temperature(void)
{
#ifdef MTK_BATTERY_LIFETIME_DATA_SUPPORT
	signed int batt_temp = force_get_tbat(KAL_TRUE);

	if (batt_temp > gFG_max_temperature)
		gFG_max_temperature = batt_temp;
	if (batt_temp < gFG_min_temperature)
		gFG_min_temperature = batt_temp;

	return batt_temp;
#else
	return force_get_tbat(KAL_TRUE);
#endif
}

signed int battery_meter_get_charger_voltage(void)
{
	int ret = 0;
	int val = 0;

	val = 5; /* set avg times */
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_CHARGER, &val);

	/* val = (((R_CHARGER_1+R_CHARGER_2)*100*val)/R_CHARGER_2)/100; */
	return val;
}

signed int battery_meter_get_battery_percentage(void)
{
#if defined(CONFIG_POWER_EXT)
	return 50;
#else

#if defined(SOC_BY_HW_FG)
	return gFG_capacity_by_c;
	/* hw fg, return gfg_percent_check_point; */
	/* voltage mode */
#else
	return gFG_capacity_by_c;
#endif
#endif
}

signed int battery_meter_initial(void)
{
#if defined(CONFIG_POWER_EXT)
	return 0;
#else
	static kal_bool meter_initilized = KAL_FALSE;

	mutex_lock(&FGADC_mutex);

	if (meter_initilized == KAL_FALSE) {
#if defined(SOC_BY_HW_FG)
		/* 1. HW initialization */
		battery_meter_ctrl(BATTERY_METER_CMD_HW_FG_INIT, NULL);

		if (wakeup_fg_algo(FG_MAIN) == -1) {
			/* fgauge_initialization(); */
			fgauge_algo_run_get_init_data();
			bm_err("[%s] SOC_BY_HW_FG not done\n",
			__func__);
		}
#endif

#if defined(SOC_BY_SW_FG)
		/* 1. HW initialization */
		battery_meter_ctrl(BATTERY_METER_CMD_HW_FG_INIT, NULL);

		if (wakeup_fg_algo(FG_MAIN) == -1) {
			/* fgauge_initialization(); */
			fgauge_algo_run_get_init_data();
			bm_err("[%s] SOC_BY_SW_FG not done\n",
			__func__);
		}
#endif

		meter_initilized = KAL_TRUE;
	}

	mutex_unlock(&FGADC_mutex);
	return 0;
#endif
}

signed int battery_meter_sync(signed int bat_i_sense_offset)
{
#if defined(CONFIG_POWER_EXT)
	return 0;
#else
	g_I_SENSE_offset = bat_i_sense_offset;
	return 0;
#endif
}

signed int battery_meter_get_battery_zcv(void)
{
#if defined(CONFIG_POWER_EXT)
	return 3987;
#else
	return gFG_voltage;
#endif
}

signed int battery_meter_get_battery_nPercent_zcv(void)
{
#if defined(CONFIG_POWER_EXT)
	return 3700;
#else
	return gFG_15_vlot;
	/* 15% zcv,  15% can be customized by */
	/* 100-g_tracking_point */
#endif
}

signed int battery_meter_get_battery_nPercent_UI_SOC(void)
{
#if defined(CONFIG_POWER_EXT)
	return 15;
#else
	return g_tracking_point; /* tracking point */
#endif
}

signed int battery_meter_get_tempR(signed int dwVolt)
{
#if defined(CONFIG_POWER_EXT)
	return 0;
#else
	int TRes;

	TRes = (RBAT_PULL_UP_R * dwVolt) / (RBAT_PULL_UP_VOLT - dwVolt);

	return TRes;
#endif
}

signed int battery_meter_get_tempV(void)
{
#if defined(CONFIG_POWER_EXT)
	return 0;
#else
	int ret = 0;
	int val = 0;

	val = 1; /* set avg times */
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_BAT_TEMP, &val);
	return val;
#endif
}

signed int battery_meter_get_VSense(void)
{
#if defined(CONFIG_POWER_EXT)
	return 0;
#else
	int ret = 0;
	int val = 0;

	val = 1; /* set avg times */
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_ADC_V_I_SENSE, &val);
	return val;
#endif
}

signed int battery_meter_get_QMAX25(void)
{
	return batt_meter_cust_data.q_max_pos_25;
}

#ifdef USING_SMOOTH_UI_SOC2
void battery_meter_smooth_uisoc2(void)
{
	static int init_flag = -1;
	signed int smooth_cc = 0;
	signed int cc_act = 0;
	signed int cc_act_delta = 0;

	if (temp_UI_SOC2 == -1)
		return;

	if (init_flag == -1) {
		UI_SOC3 = temp_UI_SOC2;
		pre_UI_SOC2 = temp_UI_SOC2;
		init_flag = 1;
	}
	battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR_ACT, &cc_act);

	if (gFG_coulomb_is_charging == 1) {
		/* charging */
		cc_act_delta = abs(cc_act - pre_cc_act);
		smooth_cc = (100 - temp_UI_SOC2) * gFG_BATT_CAPACITY_aging /
			    (100 - UI_SOC3) / 100 * 6;
		if ((cc_act_delta > smooth_cc) && UI_SOC3 < 100) {
			UI_SOC3++;
			pre_cc_act = cc_act;
			bm_debug("smooth_UISOC2-###+++UI_SOC3=%d\n", UI_SOC3);
		}
	} else {
		/* discharging */
		/* reset pre_cc_act if UI_SOC2 changed */
		if (temp_UI_SOC2 != pre_UI_SOC2) {
			pre_UI_SOC2 = temp_UI_SOC2;
			pre_cc_act = cc_act;
		}
		/* get delta coulomb count every persent */
		cc_act_delta = abs(cc_act - pre_cc_act);
		smooth_cc = temp_UI_SOC2 * gFG_BATT_CAPACITY_aging / UI_SOC3 /
			    100 * 15;
		if ((cc_act_delta > smooth_cc) && UI_SOC3 > 0) {
			UI_SOC3--;
			pre_cc_act = cc_act;
			bm_debug("smooth_UISOC2-###---UI_SOC3=%d\n", UI_SOC3);
		}
	}

	/* using UI_SOC2 when it not keeping,full,shutdown case */
	if ((temp_UI_SOC2 <= UI_SOC3) || (BMT_status.bat_full == KAL_TRUE) ||
	    (temp_UI_SOC2 == 1))
		UI_SOC3 = temp_UI_SOC2;

	BMT_status.UI_SOC2 = UI_SOC3;

	bm_debug("smooth_UISOC2-@@@@@@@%d,%d,%d,%d,%d,%d,%d,%d\n",
		 gFG_coulomb_is_charging, temp_UI_SOC2, UI_SOC3, smooth_cc,
		 cc_act_delta, pre_cc_act, cc_act, gFG_BATT_CAPACITY_aging);
}
#endif

/* ============================================================ // */
static ssize_t fgadc_log_write(struct file *filp, const char __user *buff,
			       size_t len, loff_t *data)
{
	char proc_fgadc_data;

	if ((len <= 0) || copy_from_user(&proc_fgadc_data, buff, 1)) {
		bm_debug("%s error.\n", __func__);
		return -EFAULT;
	}

	if (proc_fgadc_data == '1') {
		bm_debug("enable FGADC driver log system\n");
		Enable_FGADC_LOG = 1;
	} else if (proc_fgadc_data == '2') {
		bm_debug("enable FGADC driver log system:2\n");
		Enable_FGADC_LOG = 2;
	} else if (proc_fgadc_data == '3') {
		bm_debug("enable FGADC driver log system:3\n");
		Enable_FGADC_LOG = 3;
	} else if (proc_fgadc_data == '4') {
		bm_debug("enable FGADC driver log system:4\n");
		Enable_FGADC_LOG = 4;
	} else if (proc_fgadc_data == '5') {
		bm_debug("enable FGADC driver log system:5\n");
		Enable_FGADC_LOG = 5;
	} else if (proc_fgadc_data == '6') {
		bm_debug("enable FGADC driver log system:6\n");
		Enable_FGADC_LOG = 6;
	} else if (proc_fgadc_data == '7') {
		bm_debug("enable FGADC driver log system:7\n");
		Enable_FGADC_LOG = 7;
	} else if (proc_fgadc_data == '8') {
		bm_debug("enable FGADC driver log system:8\n");
		Enable_FGADC_LOG = 8;
	} else {
		bm_debug("Disable FGADC driver log system\n");
		Enable_FGADC_LOG = 0;
	}

	return len;
}

static const struct file_operations fgadc_proc_fops = {
	.write = fgadc_log_write,
};

int init_proc_log_fg(void)
{
	int ret = 0;

#if 1
	proc_create("fgadc_log", 0644, NULL, &fgadc_proc_fops);
	bm_debug("proc_create fgadc_proc_fops\n");
#else
	proc_entry_fgadc = create_proc_entry("fgadc_log", 0644, NULL);

	if (proc_entry_fgadc == NULL) {
		ret = -ENOMEM;
		bm_debug("%s: Couldn't create proc entry\n", __func__);
	} else {
		proc_entry_fgadc->write_proc = fgadc_log_write;
		bm_debug("%s loaded.\n", __func__);
	}
#endif

	return ret;
}

#ifdef MTK_BATTERY_LIFETIME_DATA_SUPPORT


#ifdef CUSTOM_BATTERY_CYCLE_AGING_DATA

signed int get_battery_aging_factor(signed int cycle)
{
	signed int i, f1, f2, c1, c2;
	signed int saddles;

	saddles = sizeof(battery_aging_table) / sizeof(BATTERY_CYCLE_STRUCT);

	for (i = 0; i < saddles; i++) {
		if (battery_aging_table[i].cycle == cycle)
			return battery_aging_table[i].aging_factor;

		if (battery_aging_table[i].cycle > cycle) {
			if (i == 0)
				return 100;

			if (battery_aging_table[i].aging_factor >
			    battery_aging_table[i - 1].aging_factor) {
				f1 = battery_aging_table[i].aging_factor;
				f2 = battery_aging_table[i - 1].aging_factor;
				c1 = battery_aging_table[i].cycle;
				c2 = battery_aging_table[i - 1].cycle;
				return (f2 +
					((cycle - c2) * (f1 - f2)) / (c1 - c2));
			}

			f1 = battery_aging_table[i - 1].aging_factor;
			f2 = battery_aging_table[i].aging_factor;
			c1 = battery_aging_table[i].cycle;
			c2 = battery_aging_table[i - 1].cycle;
			return (f2 + ((cycle - c2) * (f1 - f2)) / (c1 - c2));
		}
	}

	return battery_aging_table[saddles - 1].aging_factor;
}

#endif

static ssize_t show_FG_Battery_Cycle(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	bm_debug("[FG] gFG_battery_cycle  : %d\n", gFG_battery_cycle);
	return sprintf(buf, "%d\n", gFG_battery_cycle);
}

static ssize_t store_FG_Battery_Cycle(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	signed int cycle;

#ifdef CUSTOM_BATTERY_CYCLE_AGING_DATA
	signed int aging_capacity;
	signed int factor;
#endif

	if (kstrtoint(buf, 10, &cycle) == 0) {
		bm_debug("[FG] update battery cycle count: %d\n", cycle);
		gFG_battery_cycle = cycle;

#ifdef CUSTOM_BATTERY_CYCLE_AGING_DATA
		/* perform cycle aging calculation */

		factor = get_battery_aging_factor(gFG_battery_cycle);
		if (factor > 0 && factor < 100) {
			bm_debug("[FG] cycle count to aging factor %d\n",
				 factor);
			aging_capacity = gFG_BATT_CAPACITY * factor / 100;
			if (aging_capacity < gFG_BATT_CAPACITY_aging) {
				bm_debug(
					"[FG] update gFG_BATT_CAPACITY_aging to %d\n",
					aging_capacity);
				gFG_BATT_CAPACITY_aging = aging_capacity;
			}
		}
#endif
	} else {
		bm_debug("[FG] format error!\n");
	}
	return size;
}

static DEVICE_ATTR(FG_Battery_Cycle, 0664, show_FG_Battery_Cycle,
		   store_FG_Battery_Cycle);


static ssize_t show_FG_Max_Battery_Voltage(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	bm_debug("[FG] gFG_max_voltage  : %d\n", gFG_max_voltage);
	return sprintf(buf, "%d\n", gFG_max_voltage);
}

static ssize_t store_FG_Max_Battery_Voltage(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t size)
{
	signed int voltage;

	if (kstrtoint(buf, 10, &voltage) == 0) {
		if (voltage > gFG_max_voltage) {
			bm_debug("[FG] update battery max voltage: %d\n",
				 voltage);
			gFG_max_voltage = voltage;
		}
	} else {
		bm_debug("[FG] format error!\n");
	}
	return size;
}

static DEVICE_ATTR(FG_Max_Battery_Voltage, 0664, show_FG_Max_Battery_Voltage,
		   store_FG_Max_Battery_Voltage);


static ssize_t show_FG_Min_Battery_Voltage(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	bm_debug("[FG] gFG_min_voltage  : %d\n", gFG_min_voltage);
	return sprintf(buf, "%d\n", gFG_min_voltage);
}

static ssize_t store_FG_Min_Battery_Voltage(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t size)
{
	signed int voltage;

	if (kstrtoint(buf, 10, &voltage) == 0) {
		if (voltage < gFG_min_voltage) {
			bm_debug("[FG] update battery min voltage: %d\n",
				 voltage);
			gFG_min_voltage = voltage;
		}
	} else {
		bm_debug("[FG] format error!\n");
	}
	return size;
}

static DEVICE_ATTR(FG_Min_Battery_Voltage, 0664, show_FG_Min_Battery_Voltage,
		   store_FG_Min_Battery_Voltage);


static ssize_t show_FG_Max_Battery_Current(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	bm_debug("[FG] gFG_max_current  : %d\n", gFG_max_current);
	return sprintf(buf, "%d\n", gFG_max_current);
}

static ssize_t store_FG_Max_Battery_Current(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t size)
{
	signed int bat_current;

	if (kstrtoint(buf, 10, &bat_current) == 0) {
		if (bat_current > gFG_max_current) {
			bm_debug("[FG] update battery max current: %d\n",
				 bat_current);
			gFG_max_current = bat_current;
		}
	} else {
		bm_debug("[FG] format error!\n");
	}
	return size;
}

static DEVICE_ATTR(FG_Max_Battery_Current, 0664, show_FG_Max_Battery_Current,
		   store_FG_Max_Battery_Current);


static ssize_t show_FG_Min_Battery_Current(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	bm_debug("[FG] gFG_min_current  : %d\n", gFG_min_current);
	return sprintf(buf, "%d\n", gFG_min_current);
}

static ssize_t store_FG_Min_Battery_Current(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t size)
{
	signed int bat_current;

	if (kstrtoint(buf, 10, &bat_current) == 0) {
		if (bat_current < gFG_min_current) {
			bm_debug("[FG] update battery min current: %d\n",
				 bat_current);
			gFG_min_current = bat_current;
		}
	} else {
		bm_debug("[FG] format error!\n");
	}
	return size;
}

static DEVICE_ATTR(FG_Min_Battery_Current, 0664, show_FG_Min_Battery_Current,
		   store_FG_Min_Battery_Current);


static ssize_t show_FG_Max_Battery_Temperature(struct device *dev,
					       struct device_attribute *attr,
					       char *buf)
{
	bm_debug("[FG] gFG_max_temperature  : %d\n", gFG_max_temperature);
	return sprintf(buf, "%d\n", gFG_max_temperature);
}

static ssize_t store_FG_Max_Battery_Temperature(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t size)
{
	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {
		if (temp > gFG_max_temperature) {
			bm_debug("[FG] update battery max temp: %d\n", temp);
			gFG_max_temperature = temp;
		}
	} else {
		bm_debug("[FG] format error!\n");
	}
	return size;
}

static DEVICE_ATTR(FG_Max_Battery_Temperature, 0664,
		   show_FG_Max_Battery_Temperature,
		   store_FG_Max_Battery_Temperature);


static ssize_t show_FG_Min_Battery_Temperature(struct device *dev,
					       struct device_attribute *attr,
					       char *buf)
{
	bm_debug("[FG] gFG_min_temperature  : %d\n", gFG_min_temperature);
	return sprintf(buf, "%d\n", gFG_min_temperature);
}

static ssize_t store_FG_Min_Battery_Temperature(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t size)
{
	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {
		if (temp < gFG_min_temperature) {
			bm_debug("[FG] update battery min temp: %d\n", temp);
			gFG_min_temperature = temp;
		}
	} else {
		bm_debug("[FG] format error!\n");
	}
	return size;
}

static DEVICE_ATTR(FG_Min_Battery_Temperature, 0664,
		   show_FG_Min_Battery_Temperature,
		   store_FG_Min_Battery_Temperature);


static ssize_t show_FG_Aging_Factor(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	bm_debug("[FG] gFG_aging_factor_1  : %d\n", gFG_aging_factor_1);
	return sprintf(buf, "%d\n", gFG_aging_factor_1);
}

static ssize_t store_FG_Aging_Factor(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	signed int factor;
	signed int aging_capacity;

	if (kstrtoint(buf, 10, &factor) == 0) {
		if (factor <= 100 && factor >= 0) {
			bm_print(
				BM_LOG_CRTI,
				"[FG] update battery aging factor: old(%d), new(%d)\n",
				gFG_aging_factor_1, factor);

			gFG_aging_factor_1 = factor;

			if (gFG_aging_factor_1 != 100) {
				aging_capacity = gFG_BATT_CAPACITY *
						 gFG_aging_factor_1 / 100;
				if (aging_capacity < gFG_BATT_CAPACITY_aging) {
					bm_print(
						BM_LOG_CRTI,
						"[FG] update gFG_BATT_CAPACITY_aging to %d\n",
						aging_capacity);
					gFG_BATT_CAPACITY_aging =
						aging_capacity;
				}
			}
		}
	} else {
		bm_debug("[FG] format error!\n");
	}

	return size;
}

static DEVICE_ATTR(FG_Aging_Factor, 0664, show_FG_Aging_Factor,
		   store_FG_Aging_Factor);


#endif

static ssize_t show_FG_Current(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	signed int ret = 0;
	signed int fg_current_inout_battery = 0;
	signed int val = 0;
	kal_bool is_charging = 0;

	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT, &val);
	ret = battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT_SIGN,
				 &is_charging);

	if (is_charging == KAL_TRUE)
		fg_current_inout_battery = 0 - val;
	else
		fg_current_inout_battery = val;

	bm_debug("[FG] gFG_current_inout_battery : %d\n",
		 fg_current_inout_battery);
	return sprintf(buf, "%d\n", fg_current_inout_battery);
}

static ssize_t store_FG_Current(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_Current, 0664, show_FG_Current, store_FG_Current);

static ssize_t show_FG_g_fg_dbg_bat_volt(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	bm_debug("[FG] g_fg_dbg_bat_volt : %d\n", g_fg_dbg_bat_volt);
	return sprintf(buf, "%d\n", g_fg_dbg_bat_volt);
}

static ssize_t store_FG_g_fg_dbg_bat_volt(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_bat_volt, 0664, show_FG_g_fg_dbg_bat_volt,
		   store_FG_g_fg_dbg_bat_volt);

static ssize_t show_FG_g_fg_dbg_bat_current(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	bm_debug("[FG] g_fg_dbg_bat_current : %d\n", g_fg_dbg_bat_current);
	return sprintf(buf, "%d\n", g_fg_dbg_bat_current);
}

static ssize_t store_FG_g_fg_dbg_bat_current(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_bat_current, 0664, show_FG_g_fg_dbg_bat_current,
		   store_FG_g_fg_dbg_bat_current);

static ssize_t show_FG_g_fg_dbg_bat_zcv(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	bm_debug("[FG] g_fg_dbg_bat_zcv : %d\n", g_fg_dbg_bat_zcv);
	return sprintf(buf, "%d\n", g_fg_dbg_bat_zcv);
}

static ssize_t store_FG_g_fg_dbg_bat_zcv(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_bat_zcv, 0664, show_FG_g_fg_dbg_bat_zcv,
		   store_FG_g_fg_dbg_bat_zcv);

static ssize_t show_FG_g_fg_dbg_bat_temp(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	bm_debug("[FG] g_fg_dbg_bat_temp : %d\n", g_fg_dbg_bat_temp);
	return sprintf(buf, "%d\n", g_fg_dbg_bat_temp);
}

static ssize_t store_FG_g_fg_dbg_bat_temp(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_bat_temp, 0664, show_FG_g_fg_dbg_bat_temp,
		   store_FG_g_fg_dbg_bat_temp);

static ssize_t show_FG_g_fg_dbg_bat_r(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	bm_debug("[FG] g_fg_dbg_bat_r : %d\n", g_fg_dbg_bat_r);
	return sprintf(buf, "%d\n", g_fg_dbg_bat_r);
}

static ssize_t store_FG_g_fg_dbg_bat_r(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_bat_r, 0664, show_FG_g_fg_dbg_bat_r,
		   store_FG_g_fg_dbg_bat_r);

static ssize_t show_FG_g_fg_dbg_bat_car(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	bm_debug("[FG] g_fg_dbg_bat_car : %d\n", g_fg_dbg_bat_car);
	return sprintf(buf, "%d\n", g_fg_dbg_bat_car);
}

static ssize_t store_FG_g_fg_dbg_bat_car(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_bat_car, 0664, show_FG_g_fg_dbg_bat_car,
		   store_FG_g_fg_dbg_bat_car);

static ssize_t show_FG_g_fg_dbg_bat_qmax(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	bm_debug("[FG] g_fg_dbg_bat_qmax : %d\n", g_fg_dbg_bat_qmax);
	return sprintf(buf, "%d\n", g_fg_dbg_bat_qmax);
}

static ssize_t store_FG_g_fg_dbg_bat_qmax(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_bat_qmax, 0664, show_FG_g_fg_dbg_bat_qmax,
		   store_FG_g_fg_dbg_bat_qmax);

static ssize_t show_FG_g_fg_dbg_d0(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	bm_debug("[FG] g_fg_dbg_d0 : %d\n", g_fg_dbg_d0);
	return sprintf(buf, "%d\n", g_fg_dbg_d0);
}

static ssize_t store_FG_g_fg_dbg_d0(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_d0, 0664, show_FG_g_fg_dbg_d0,
		   store_FG_g_fg_dbg_d0);

static ssize_t show_FG_g_fg_dbg_d1(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	bm_debug("[FG] g_fg_dbg_d1 : %d\n", g_fg_dbg_d1);
	return sprintf(buf, "%d\n", g_fg_dbg_d1);
}

static ssize_t store_FG_g_fg_dbg_d1(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_d1, 0664, show_FG_g_fg_dbg_d1,
		   store_FG_g_fg_dbg_d1);

static ssize_t show_FG_g_fg_dbg_percentage(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	bm_debug("[FG] g_fg_dbg_percentage : %d\n", g_fg_dbg_percentage);
	return sprintf(buf, "%d\n", g_fg_dbg_percentage);
}

static ssize_t store_FG_g_fg_dbg_percentage(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_percentage, 0664, show_FG_g_fg_dbg_percentage,
		   store_FG_g_fg_dbg_percentage);

static ssize_t show_FG_g_fg_dbg_percentage_uisoc(struct device *dev,
						 struct device_attribute *attr,
						 char *buf)
{
	bm_debug("[FG] g_fg_dbg_percentage :%d\n", BMT_status.UI_SOC);
	return sprintf(buf, "%d\n", BMT_status.UI_SOC);
}

static ssize_t store_FG_g_fg_dbg_percentage_uisoc(struct device *dev,
						  struct device_attribute *attr,
						  const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_percentage_uisoc, 0664,
		   show_FG_g_fg_dbg_percentage_uisoc,
		   store_FG_g_fg_dbg_percentage_uisoc);


static ssize_t show_FG_g_fg_dbg_percentage_fg(struct device *dev,
					      struct device_attribute *attr,
					      char *buf)
{
	bm_debug("[FG] g_fg_dbg_percentage_fg : %d\n", g_fg_dbg_percentage_fg);
	return sprintf(buf, "%d\n", g_fg_dbg_percentage_fg);
}

static ssize_t store_FG_g_fg_dbg_percentage_fg(struct device *dev,
					       struct device_attribute *attr,
					       const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_percentage_fg, 0664,
		   show_FG_g_fg_dbg_percentage_fg,
		   store_FG_g_fg_dbg_percentage_fg);

static ssize_t
show_FG_g_fg_dbg_percentage_voltmode(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	bm_debug("[FG] g_fg_dbg_percentage_voltmode : %d\n",
		 g_fg_dbg_percentage_voltmode);
	return sprintf(buf, "%d\n", g_fg_dbg_percentage_voltmode);
}

static ssize_t
store_FG_g_fg_dbg_percentage_voltmode(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(FG_g_fg_dbg_percentage_voltmode, 0664,
		   show_FG_g_fg_dbg_percentage_voltmode,
		   store_FG_g_fg_dbg_percentage_voltmode);

#ifdef MTK_ENABLE_AGING_ALGORITHM
static ssize_t show_FG_suspend_current_threshold(struct device *dev,
						 struct device_attribute *attr,
						 char *buf)
{
	bm_debug("[FG] show suspend_current_threshold : %d\n",
		 suspend_current_threshold);
	return sprintf(buf, "%d\n", suspend_current_threshold);
}

static ssize_t store_FG_suspend_current_threshold(struct device *dev,
						  struct device_attribute *attr,
						  const char *buf, size_t size)
{

	unsigned int val = 0;
	int ret;

	bm_debug("[%s]\n", __func__);

	if (buf != NULL && size != 0) {
		bm_debug("[%s] buf is %s\n",
			 __func__, buf);
		ret = kstrtouint(buf, 10, &val);
		if (val < 100)
			val = 100;
		suspend_current_threshold = val;
		bm_debug(
			"[%s] suspend_current_threshold=%d\n",
			__func__, suspend_current_threshold);
	}
	return size;
}

static DEVICE_ATTR(FG_suspend_current_threshold, 0664,
		   show_FG_suspend_current_threshold,
		   store_FG_suspend_current_threshold);


static ssize_t show_FG_ocv_check_time(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	bm_debug("[FG] show %s : %d\n", __func__, ocv_check_time);
	return sprintf(buf, "%d\n", ocv_check_time);
}

static ssize_t store_FG_ocv_check_time(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret = 0;

	bm_debug("[%s]\n", __func__);

	if (buf != NULL && size != 0) {
		bm_debug("[%s] buf is %s\n", __func__, buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 100)
			val = 100;
		ocv_check_time = val;
		bm_debug("[store_ocv_check_time] ocv_check_time=%d\n",
			 ocv_check_time);
	}
	return size;
}

static DEVICE_ATTR(FG_ocv_check_time, 0664, show_FG_ocv_check_time,
		   store_FG_ocv_check_time);

static ssize_t show_FG_difference_voltage_update(struct device *dev,
						 struct device_attribute *attr,
						 char *buf)
{
	bm_debug("[FG] show ocv_check_time : %d\n", ocv_check_time);
	return sprintf(buf, "%d\n", ocv_check_time);
}

static ssize_t store_FG_difference_voltage_update(struct device *dev,
						  struct device_attribute *attr,
						  const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret = 0;

	bm_debug("[%s]\n", __func__);

	if (buf != NULL && size != 0) {
		bm_debug("[%s] buf is %s\n",
			 __func__, buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 0)
			val = 0;
		difference_voltage_update = val;
		bm_debug(
			"[%s] difference_voltage_update=%d\n",
			__func__, difference_voltage_update);
	}
	return size;
}

static DEVICE_ATTR(FG_difference_voltage_update, 0664,
		   show_FG_difference_voltage_update,
		   store_FG_difference_voltage_update);

static ssize_t show_FG_aging1_load_soc(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	bm_debug("[FG] show aging1_load_soc : %d\n", aging1_load_soc);
	return sprintf(buf, "%d\n", aging1_load_soc);
}

static ssize_t store_FG_aging1_load_soc(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	unsigned int val = 0;
	int ret;

	bm_debug("[%s]\n", __func__);

	if (buf != NULL && size != 0) {
		bm_debug("[%s] buf is %s\n", __func__, buf);
		ret = kstrtouint(buf, 10, &val);
		if (val < 0)
			val = 0;
		if (val > 100)
			val = 100;
		aging1_load_soc = val;
		bm_debug("[store_aging1_load_soc] aging1_load_soc=%d\n",
			 aging1_load_soc);
	}
	return size;
}

static DEVICE_ATTR(FG_aging1_load_soc, 0664, show_FG_aging1_load_soc,
		   store_FG_aging1_load_soc);

static ssize_t show_FG_aging1_update_soc(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	bm_debug("[FG] show %s : %d\n", __func__, aging1_update_soc);
	return sprintf(buf, "%d\n", aging1_update_soc);
}

static ssize_t store_FG_aging1_update_soc(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	unsigned int val = 0;
	int ret;

	bm_debug("[%s]\n", __func__);

	if (buf != NULL && size != 0) {
		bm_debug("[%s] buf is %s\n", __func__, buf);
		ret = kstrtouint(buf, 10, &val);
		if (val < 0)
			val = 0;
		if (val > 100)
			val = 100;
		aging1_update_soc = val;
		bm_debug("[%s] aging1_update_soc=%d\n",
			 __func__, aging1_update_soc);
	}
	return size;
}

static DEVICE_ATTR(FG_aging1_update_soc, 0664, show_FG_aging1_update_soc,
		   store_FG_aging1_update_soc);

static ssize_t show_FG_shutdown_system_voltage(struct device *dev,
					       struct device_attribute *attr,
					       char *buf)
{
	bm_debug("[FG] show %s : %d\n",
		 __func__, shutdown_system_voltage);
	return sprintf(buf, "%d\n", shutdown_system_voltage);
}

static ssize_t store_FG_shutdown_system_voltage(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t size)
{
	unsigned int val = 0;
	int ret;

	bm_debug("[shutdown_system_voltage]\n");

	if (buf != NULL && size != 0) {
		bm_debug("[shutdown_system_voltage] buf is %s\n", buf);
		ret = kstrtouint(buf, 10, &val);
		if (val < 0)
			val = 0;
		shutdown_system_voltage = val;
		bm_debug(
			"[shutdown_system_voltage] shutdown_system_voltage=%d\n",
			shutdown_system_voltage);
	}
	return size;
}

static DEVICE_ATTR(FG_shutdown_system_voltage, 0664,
		   show_FG_shutdown_system_voltage,
		   store_FG_shutdown_system_voltage);

static ssize_t show_FG_charge_tracking_time(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	bm_debug("[FG] show charge_tracking_time : %d\n", charge_tracking_time);
	return sprintf(buf, "%d\n", charge_tracking_time);
}

static ssize_t store_FG_charge_tracking_time(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t size)
{
	unsigned int val = 0;
	int ret;

	bm_debug("[charge_tracking_time]\n");

	if (buf != NULL && size != 0) {
		bm_debug("[charge_tracking_time] buf is %s\n", buf);
		ret = kstrtouint(buf, 10, &val);
		if (val < 0)
			val = 0;
		charge_tracking_time = val;
		bm_debug("[charge_tracking_time] charge_tracking_time=%d\n",
			 charge_tracking_time);
	}
	return size;
}

static DEVICE_ATTR(FG_charge_tracking_time, 0664, show_FG_charge_tracking_time,
		   store_FG_charge_tracking_time);

static ssize_t show_FG_discharge_tracking_time(struct device *dev,
					       struct device_attribute *attr,
					       char *buf)
{
	bm_debug("[FG] show discharge_tracking_time : %d\n",
		 discharge_tracking_time);
	return sprintf(buf, "%d\n", discharge_tracking_time);
}

static ssize_t store_FG_discharge_tracking_time(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t size)
{
	unsigned int val = 0;
	int ret;

	bm_debug("[discharge_tracking_time]\n");

	if (buf != NULL && size != 0) {
		bm_debug("[discharge_tracking_time] buf is %s\n", buf);
		ret = kstrtouint(buf, 10, &val);
		if (val < 0)
			val = 0;
		discharge_tracking_time = val;
		bm_debug(
			"[discharge_tracking_time] discharge_tracking_time=%d\n",
			discharge_tracking_time);
	}
	return size;
}

static DEVICE_ATTR(FG_discharge_tracking_time, 0664,
		   show_FG_discharge_tracking_time,
		   store_FG_discharge_tracking_time);
#endif
static ssize_t show_FG_shutdown_gauge0(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	bm_debug("[FG] show shutdown_gauge0 : %d\n", shutdown_gauge0);
	return sprintf(buf, "%d\n", shutdown_gauge0);
}

static ssize_t store_FG_shutdown_gauge0(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	unsigned int val = 0;
	int ret;

	bm_debug("[shutdown_gauge0]\n");

	if (buf != NULL && size != 0) {
		bm_debug("[shutdown_gauge0] buf is %s\n", buf);
		ret = kstrtouint(buf, 10, &val);
		if (val < 0)
			val = 0;
		shutdown_gauge0 = val;
		bm_debug("[shutdown_gauge0] shutdown_gauge0=%d\n",
			 shutdown_gauge0);
	}
	return size;
}

static DEVICE_ATTR(FG_shutdown_gauge0, 0664, show_FG_shutdown_gauge0,
		   store_FG_shutdown_gauge0);

static ssize_t show_FG_shutdown_gauge1_xmins(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	bm_debug("[FG] show shutdown_gauge1_xmins : %d\n",
		 shutdown_gauge1_xmins);
	return sprintf(buf, "%d\n", shutdown_gauge1_xmins);
}

static ssize_t store_FG_shutdown_gauge1_xmins(struct device *dev,
					      struct device_attribute *attr,
					      const char *buf, size_t size)
{
	unsigned int val = 0;
	int ret;

	bm_debug("[shutdown_gauge1_xmins]\n");

	if (buf != NULL && size != 0) {
		bm_debug("[shutdown_gauge1_xmins] buf is %s\n", buf);
		ret = kstrtouint(buf, 10, &val);
		if (val < 0)
			val = 0;
		shutdown_gauge1_xmins = val;
		bm_debug("[shutdown_gauge1_xmins] shutdown_gauge1_xmins=%d\n",
			 shutdown_gauge1_xmins);
	}
	return size;
}

static DEVICE_ATTR(FG_shutdown_gauge1_xmins, 0664,
		   show_FG_shutdown_gauge1_xmins,
		   store_FG_shutdown_gauge1_xmins);

static ssize_t show_FG_shutdown_gauge1_mins(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	bm_debug("[FG] show shutdown_gauge1_mins : %d\n", shutdown_gauge1_mins);
	return sprintf(buf, "%d\n", shutdown_gauge1_mins);
}

static ssize_t store_FG_shutdown_gauge1_mins(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t size)
{
	unsigned int val = 0;
	int ret;

	bm_debug("[shutdown_gauge1_mins]\n");

	if (buf != NULL && size != 0) {
		bm_debug("[shutdown_gauge1_mins] buf is %s\n", buf);
		ret = kstrtouint(buf, 10, &val);
		if (val < 0)
			val = 0;
		shutdown_gauge1_mins = val;
		bm_debug("[shutdown_gauge1_mins] shutdown_gauge1_mins=%d\n",
			 shutdown_gauge1_mins);
	}
	return size;
}

static DEVICE_ATTR(FG_shutdown_gauge1_mins, 0664, show_FG_shutdown_gauge1_mins,
		   store_FG_shutdown_gauge1_mins);

static ssize_t show_FG_daemon_log_level(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	bm_trace("[FG] show FG_daemon_log_level : %d\n", gFG_daemon_log_level);
	return sprintf(buf, "%d\n", gFG_daemon_log_level);
}

static ssize_t store_FG_daemon_log_level(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret;

	bm_debug("[FG_daemon_log_level]\n");

	if (buf != NULL && size != 0) {
		bm_debug("[FG_daemon_log_level] buf is %s\n", buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 0) {
			bm_debug("[FG_daemon_log_level] val is %d ??\n",
				 (int)val);
			val = 0;
		}
		gFG_daemon_log_level = val;
		bm_debug("[FG_daemon_log_level] gFG_daemon_log_level=%d\n",
			 gFG_daemon_log_level);
	}
	return size;
}

static DEVICE_ATTR(FG_daemon_log_level, 0664, show_FG_daemon_log_level,
		   store_FG_daemon_log_level);

static ssize_t show_FG_daemon_disable(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	bm_trace("[FG] show FG_daemon_log_level : %d\n", gDisableFG);
	return sprintf(buf, "%d\n", gDisableFG);
}

static ssize_t store_FG_daemon_disable(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t size)
{

	bm_debug("[disable FG daemon]\n");
	BMT_status.UI_SOC2 = 50;
	if (!g_battery_soc_ready) {
		g_battery_soc_ready = KAL_TRUE;
		gfg_percent_check_point = 50;
	}

	bat_update_thread_wakeup();

	gDisableFG = 1;

	return size;
}

static DEVICE_ATTR(FG_daemon_disable, 0664, show_FG_daemon_disable,
		   store_FG_daemon_disable);

static ssize_t show_FG_drv_force25c(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	bm_debug("[FG] show FG_drv_force25c : %d\n",
		 batt_meter_cust_data.fixed_tbat_25);
	return sprintf(buf, "%d\n", batt_meter_cust_data.fixed_tbat_25);
}

static ssize_t store_FG_drv_force25c(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	unsigned long val = 0;
	int ret;

	bm_debug("[Enable FG_drv_force25c]\n");
	batt_meter_cust_data.fixed_tbat_25 = 1;

	if (buf != NULL && size != 0) {
		bm_debug("[FG_drv_force25c] buf is %s\n", buf);
		ret = kstrtoul(buf, 10, &val);
		if (val < 0) {
			bm_debug("[FG_drv_force25c] val is %d ??\n", (int)val);
			val = 0;
		}
		batt_meter_cust_data.fixed_tbat_25 = val;
		bm_debug("[FG_drv_force25c] fixed_tbat_25=%d, ret=%d\n",
			 batt_meter_cust_data.fixed_tbat_25, ret);
	}

	return size;
}

static DEVICE_ATTR(FG_drv_force25c, 0664, show_FG_drv_force25c,
		   store_FG_drv_force25c);
#ifdef FG_BAT_INT
unsigned char reset_fg_bat_int = KAL_TRUE;

signed int fg_bat_int_coulomb_pre;
signed int fg_bat_int_coulomb;

void fg_bat_int_handler(void)
{
	battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR_ACT,
			   &fg_bat_int_coulomb);
	battery_log(BAT_LOG_CRTI, "%s %d %d\n", __func__,
		    fg_bat_int_coulomb_pre, fg_bat_int_coulomb);

	reset_fg_bat_int = KAL_TRUE;
	if (bat_is_charger_exist() == KAL_FALSE) {
		battery_log(BAT_LOG_CRTI, "wake up user space >>\n");
		/* self_correct_dod_scheme(duration_time); */
		wakeup_fg_algo(FG_RESUME);
	}
}

signed int battery_meter_set_columb_interrupt(unsigned int val)
{
	battery_log(BAT_LOG_FULL, "%s=%d\n",
		    __func__, val);
	battery_meter_ctrl(BATTERY_METER_CMD_SET_COLUMB_INTERRUPT, &val);
	return 0;
}

#endif

static int battery_meter_probe(struct platform_device *dev)
{
	int ret_device_file = 0;
#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
	char *temp_strptr;
#endif

	bm_info("[%s] probe\n", __func__);
	/* select battery meter control method */
	battery_meter_ctrl = bm_ctrl_cmd;
#if defined(CONFIG_MTK_KERNEL_POWER_OFF_CHARGING)
	if (get_boot_mode() == LOW_POWER_OFF_CHARGING_BOOT ||
	    get_boot_mode() == KERNEL_POWER_OFF_CHARGING_BOOT) {
		temp_strptr =
			kzalloc(strlen(saved_command_line) +
					strlen(" androidboot.mode=charger") + 1,
				GFP_KERNEL);
		strncpy(temp_strptr, saved_command_line,
			strlen(saved_command_line));
		strncat(temp_strptr, " androidboot.mode=charger",
			strlen(" androidboot.mode=charger") + 1);
		saved_command_line = temp_strptr;
	}
#endif
	/* LOG System Set */
	init_proc_log_fg();

	/* last_oam_run_time = rtc_read_hw_time(); */
	getrawmonotonic(&last_oam_run_time);
	/* Create File For FG UI DEBUG */
	ret_device_file = device_create_file(&(dev->dev), &dev_attr_FG_Current);
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_volt);
	ret_device_file = device_create_file(&(dev->dev),
					     &dev_attr_FG_g_fg_dbg_bat_current);
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_zcv);
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_temp);
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_r);
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_car);
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_bat_qmax);
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_d0);
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_FG_g_fg_dbg_d1);
	ret_device_file = device_create_file(&(dev->dev),
					     &dev_attr_FG_g_fg_dbg_percentage);
	ret_device_file = device_create_file(
		&(dev->dev), &dev_attr_FG_g_fg_dbg_percentage_fg);
	ret_device_file = device_create_file(
		&(dev->dev), &dev_attr_FG_g_fg_dbg_percentage_uisoc);
	ret_device_file = device_create_file(
		&(dev->dev), &dev_attr_FG_g_fg_dbg_percentage_voltmode);
#ifdef MTK_ENABLE_AGING_ALGORITHM
	ret_device_file = device_create_file(
		&(dev->dev), &dev_attr_FG_suspend_current_threshold);
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_FG_ocv_check_time);
	ret_device_file = device_create_file(
		&(dev->dev), &dev_attr_FG_difference_voltage_update);
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_FG_aging1_load_soc);
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_FG_aging1_update_soc);
	ret_device_file = device_create_file(
		&(dev->dev), &dev_attr_FG_shutdown_system_voltage);
	ret_device_file = device_create_file(&(dev->dev),
					     &dev_attr_FG_charge_tracking_time);
	ret_device_file = device_create_file(
		&(dev->dev), &dev_attr_FG_discharge_tracking_time);

#endif
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_FG_shutdown_gauge0);
	ret_device_file = device_create_file(
		&(dev->dev), &dev_attr_FG_shutdown_gauge1_xmins);
	ret_device_file = device_create_file(&(dev->dev),
					     &dev_attr_FG_shutdown_gauge1_mins);

#ifdef MTK_BATTERY_LIFETIME_DATA_SUPPORT
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_FG_Battery_Cycle);
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_FG_Aging_Factor);
	ret_device_file = device_create_file(&(dev->dev),
					     &dev_attr_FG_Max_Battery_Voltage);
	ret_device_file = device_create_file(&(dev->dev),
					     &dev_attr_FG_Min_Battery_Voltage);
	ret_device_file = device_create_file(&(dev->dev),
					     &dev_attr_FG_Max_Battery_Current);
	ret_device_file = device_create_file(&(dev->dev),
					     &dev_attr_FG_Min_Battery_Current);
	ret_device_file = device_create_file(
		&(dev->dev), &dev_attr_FG_Max_Battery_Temperature);
	ret_device_file = device_create_file(
		&(dev->dev), &dev_attr_FG_Min_Battery_Temperature);
#endif
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_FG_daemon_log_level);
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_FG_daemon_disable);
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_FG_drv_force25c);

	batt_meter_init_cust_data(dev);

#if defined(FG_BAT_INT)
	pmic_register_interrupt_callback(FG_BAT_INT_L_NO, fg_bat_int_handler);
	pmic_register_interrupt_callback(FG_BAT_INT_H_NO, fg_bat_int_handler);
#endif

	return 0;
}

static int battery_meter_remove(struct platform_device *dev)
{
	bm_debug("[%s]\n", __func__);
	return 0;
}

static void battery_meter_shutdown(struct platform_device *dev)
{
}

static int battery_meter_suspend(struct platform_device *dev,
				 pm_message_t state)
{
	/* -- hibernation path */
	if (state.event == PM_EVENT_FREEZE) {
		pr_notice("[%s] %p:%p\n", __func__, battery_meter_ctrl,
			&bm_ctrl_cmd);
		battery_meter_ctrl = bm_ctrl_cmd;
	}
/* -- end of hibernation path */

#if defined(FG_BAT_INT)
#if defined(CONFIG_POWER_EXT)
#elif defined(SOC_BY_HW_FG)
	if (reset_fg_bat_int == KAL_TRUE) {
		battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR_ACT,
				   &fg_bat_int_coulomb_pre);
		bm_notice(
			"[%s]enable battery_meter_set_columb_interrupt %d\n",
			__func__, batt_meter_cust_data.q_max_pos_25);
		battery_meter_set_columb_interrupt(
			batt_meter_cust_data.q_max_pos_25 / 100);
		/*battery_meter_set_columb_interrupt(1); */
		reset_fg_bat_int = KAL_FALSE;
	} else {
		bm_notice(
			"[%s]do not enable battery_meter_set_columb_interrupt %d\n",
			__func__, batt_meter_cust_data.q_max_pos_25);
		battery_meter_set_columb_interrupt(0x1ffff);
	}
#endif
#else
#endif /* #if defined(FG_BAT_INT) */

#if defined(CONFIG_POWER_EXT)

#elif defined(SOC_BY_SW_FG) || defined(SOC_BY_HW_FG)
	{

#if defined(SOC_BY_SW_FG)
		{

			get_monotonic_boottime(&ap_suspend_time);
		}
#endif

		battery_meter_ctrl(BATTERY_METER_CMD_GET_REFRESH_HW_OCV,
				   &hwocv_token);

#ifdef MTK_POWER_EXT_DETECT
		if (bat_is_ext_power() == KAL_TRUE)
			return 0;
#endif

		mt_battery_update_time(&car_time, CAR_TIME);
		add_time = mt_battery_get_duration_time(CAR_TIME);
		if ((g_sleep_total_time.tv_sec < g_spm_timer) &&
		    g_sleep_total_time.tv_sec != 0) {
			if (wake_up_smooth_time == 0)
				return 0;
			else if (g_sleep_total_time.tv_sec <
				 wake_up_smooth_time)
				return 0;
		}
		battery_meter_reset_sleep_time();
		battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_OCV,
				   &g_hw_ocv_before_sleep);
		bm_info("[%s]sleep_total_time = %d, last_time = %d\n",
			__func__,
			(int)g_sleep_total_time.tv_sec, last_time);
	}
#endif

	return 0;
}

static int battery_meter_resume(struct platform_device *dev)
{
#if defined(CONFIG_POWER_EXT)

#elif defined(SOC_BY_SW_FG) || defined(SOC_BY_HW_FG)

	unsigned int duration_time = 0;
#ifdef MTK_POWER_EXT_DETECT
	if (bat_is_ext_power() == KAL_TRUE)
		return 0;
#endif
	mt_battery_update_time(&suspend_time, SUSPEND_TIME);

	add_time = mt_battery_get_duration_time_act(SUSPEND_TIME).tv_sec;
	g_sleep_total_time =
		timespec_add(g_sleep_total_time,
			     mt_battery_get_duration_time_act(SUSPEND_TIME));

	bm_info("[%s] sleep time = %d, duration_time = %d, wake_up_smooth_time %d, g_spm_timer = %d\n",
		__func__, (int)g_sleep_total_time.tv_sec,
		duration_time, wake_up_smooth_time, g_spm_timer);

#if defined(SOC_BY_HW_FG)
#ifdef MTK_ENABLE_AGING_ALGORITHM
	if (g_sleep_total_time.tv_sec < g_spm_timer) {
		if (wake_up_smooth_time == 0) {
			if (bat_is_charger_exist() == KAL_FALSE) {
				/* self_correct_dod_scheme(duration_time); */
				wakeup_fg_algo(FG_RESUME);
			}
			return 0;
		} else if (g_sleep_total_time.tv_sec < wake_up_smooth_time) {
			if (bat_is_charger_exist() == KAL_FALSE) {
				/* self_correct_dod_scheme(duration_time); */
				wakeup_fg_algo(FG_RESUME);
			}
			return 0;
		}
	}
#endif

	bm_info("* %s!! * suspend_time %d smooth_time %d g_spm_timer %d\n",
		__func__,
		(int)g_sleep_total_time.tv_sec, wake_up_smooth_time,
		g_spm_timer);
	bat_spm_timeout = true;

	if (g_sleep_total_time.tv_sec >= wake_up_smooth_time)
		wake_up_smooth_time = 0;

#elif defined(SOC_BY_SW_FG)

	if (bat_is_charger_exist() == KAL_FALSE) {
		unsigned int time;
		signed int voltage = 0;
		int oam_i = 0, oam_car_tmp;
		/* mt_battery_update_time(&ap_suspend_time, AP_SUSPEND_TIME); */
		/* time = mt_battery_get_duration_time(AP_SUSPEND_TIME); */
		time = add_time;
		battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_OCV, &voltage);

		total_suspend_times++;
		this_suspend_times++;

		if (voltage == hwocv_token) {
			oam_car_tmp = -time * APSLEEP_MDWAKEUP_CAR;
			bm_print(
				BM_LOG_CRTI,
				"[%s](1)time:%d bat:%d ocv:%d r:%d i:%d ocar:%d card:%d lbat:%d %d\n",
				__func__, time, voltage,
				oam_v_ocv, oam_r, oam_i,
				ap_suspend_car / 3600, oam_car_tmp / 3600,
				pmic_get_register_value(PMIC_RG_ADC_OUT_LBAT),
				pmic_get_register_value(
					PMIC_RG_LBAT_DEBOUNCE_COUNT_MIN));
			last_hwocv = 0;
			last_i = 0;
			is_hwocv_update = KAL_FALSE;
		} else {
			oam_car_tmp = -time * AP_MDSLEEP_CAR;

			last_hwocv = voltage;
			last_i = oam_i;
			is_hwocv_update = KAL_TRUE;

			bm_print(
				BM_LOG_CRTI,
				"[%s](2)time:%d bat:%d ocv:%d r:%d i:%d",
				"ocar:%d card:%d lbat:%d %d\n",
				__func__, time, voltage,
				oam_v_ocv, oam_r, oam_i,
				ap_suspend_car / 3600, oam_car_tmp / 3600,
				pmic_get_register_value(PMIC_RG_ADC_OUT_LBAT),
				pmic_get_register_value(
					PMIC_RG_LBAT_DEBOUNCE_COUNT_MIN));
		}

		swfg_ap_suspend_time = g_sleep_total_time.tv_sec;
		ap_suspend_car = ap_suspend_car + oam_car_tmp;

		if (abs(ap_suspend_car / 3600) > 100) {
			bat_spm_timeout = true;
			return 0;
		}

		if (g_sleep_total_time.tv_sec > wake_up_smooth_time &&
		    wake_up_smooth_time != 0) {
			wake_up_smooth_time = 0;
			bat_spm_timeout = true;
			return 0;
		}

		if (swfg_ap_suspend_time > 600) {
			bat_spm_timeout = true;
			return 0;
		}

		return 0;
	}
#endif
#endif

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_bat_meter_of_match[] = {
	{
		.compatible = "mediatek,bat_meter",
	},
	{},
};

MODULE_DEVICE_TABLE(of, mt_bat_meter_of_match);
#endif
struct platform_device battery_meter_device = {
	.name = "battery_meter", .id = -1,
};

static struct platform_driver battery_meter_driver = {
	.probe = battery_meter_probe,
	.remove = battery_meter_remove,
	.shutdown = battery_meter_shutdown,
	.suspend = battery_meter_suspend,
	.resume = battery_meter_resume,
	.driver = {


			.name = "battery_meter",
		},
};

static int battery_meter_dts_probe(struct platform_device *dev)
{
	int ret = 0;

	bm_err("******** %s!! ********\n", __func__);

	battery_meter_device.dev.of_node = dev->dev.of_node;
	ret = platform_device_register(&battery_meter_device);
	if (ret) {
		bm_err("****[%s] Unable to register device (%d)\n",
		       __func__, ret);
		return ret;
	}
	return 0;
}

static struct platform_driver battery_meter_dts_driver = {
	.probe = battery_meter_dts_probe,
	.remove = NULL,
	.shutdown = NULL,
	.suspend = NULL,
	.resume = NULL,
	.driver = {


			.name = "battery_meter_dts",
#ifdef CONFIG_OF
			.of_match_table = mt_bat_meter_of_match,
#endif
		},
};

void bmd_ctrl_cmd_from_user(void *nl_data, struct fgd_nl_msg_t *ret_msg)
{
	struct fgd_nl_msg_t *msg;

	msg = nl_data;

	ret_msg->fgd_cmd = msg->fgd_cmd;

	switch (msg->fgd_cmd) {
	case FG_DAEMON_CMD_GET_INIT_FLAG: {
		ret_msg->fgd_data_len += sizeof(init_flag);
		memcpy(ret_msg->fgd_data, &init_flag, sizeof(init_flag));
		bm_debug("[fg_res] init_flag = %d\n", init_flag);
	} break;

	case FG_DAEMON_CMD_GET_SOC: {
		ret_msg->fgd_data_len += sizeof(gFG_capacity_by_c);
		memcpy(ret_msg->fgd_data, &gFG_capacity_by_c,
		       sizeof(gFG_capacity_by_c));
		bm_debug("[fg_res] gFG_capacity_by_c = %d\n",
			 gFG_capacity_by_c);
	} break;

	case FG_DAEMON_CMD_GET_DOD0: {
		if (init_flag == KAL_FALSE) {
			gFG_DOD0 = dod_init_in_kernel();
			bm_print(BM_LOG_CRTI,
				 "[fg_res][D0_init_in_kernel] gFG_DOD0 = %d\n",
				 gFG_DOD0);
		}

		ret_msg->fgd_data_len += sizeof(gFG_DOD0);
		memcpy(ret_msg->fgd_data, &gFG_DOD0, sizeof(gFG_DOD0));
		bm_debug("[fg_res] gFG_DOD0 = %d\n", gFG_DOD0);
	} break;

	case FG_DAEMON_CMD_GET_DOD1: {
		ret_msg->fgd_data_len += sizeof(gFG_DOD1);
		memcpy(ret_msg->fgd_data, &gFG_DOD1, sizeof(gFG_DOD1));
		bm_debug("[fg_res] gFG_DOD1 = %d\n", gFG_DOD1);
	} break;

	case FG_DAEMON_CMD_GET_HW_OCV: {
		signed int voltage = 0;

		battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_OCV, &voltage);

		ret_msg->fgd_data_len += sizeof(voltage);
		memcpy(ret_msg->fgd_data, &voltage, sizeof(voltage));

		bm_debug("[fg_res] voltage = %d\n", voltage);
		gFG_hwocv = voltage;
	} break;

	case FG_DAEMON_CMD_GET_HW_FG_INIT_CURRENT: {
		ret_msg->fgd_data_len += sizeof(gFG_current_init);
		memcpy(ret_msg->fgd_data, &gFG_current_init,
		       sizeof(gFG_current_init));

		bm_debug("[fg_res] init fg_current = %d\n", gFG_current_init);
		gFG_current = gFG_current_init;
	} break;

	case FG_DAEMON_CMD_GET_HW_FG_CURRENT: {
		signed int fg_current = 0;

		battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT,
				   &fg_current);

		ret_msg->fgd_data_len += sizeof(fg_current);
		memcpy(ret_msg->fgd_data, &fg_current, sizeof(fg_current));

		bm_debug("[fg_res] fg_current = %d\n", fg_current);
		gFG_current = fg_current;
	} break;

	case FG_DAEMON_CMD_GET_HW_FG_INIT_CURRENT_SIGN: {
		ret_msg->fgd_data_len += sizeof(gFG_Is_Charging_init);
		memcpy(ret_msg->fgd_data, &gFG_Is_Charging_init,
		       sizeof(gFG_Is_Charging_init));

		bm_debug("[fg_res] current_state = %d\n", gFG_Is_Charging_init);
		gFG_Is_Charging = gFG_Is_Charging_init;
	} break;

	case FG_DAEMON_CMD_GET_HW_FG_CURRENT_SIGN: {
		kal_bool current_state = KAL_FALSE;

		battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CURRENT_SIGN,
				   &current_state);

		ret_msg->fgd_data_len += sizeof(current_state);
		memcpy(ret_msg->fgd_data, &current_state,
		       sizeof(current_state));

		bm_debug("[fg_res] current_state = %d\n", current_state);
		gFG_Is_Charging = current_state;
	} break;

	case FG_DAEMON_CMD_GET_HW_FG_CAR_ACT: {
		signed int fg_coulomb = 0;

		battery_meter_ctrl(BATTERY_METER_CMD_GET_HW_FG_CAR_ACT,
				   &fg_coulomb);
		ret_msg->fgd_data_len += sizeof(fg_coulomb);
		memcpy(ret_msg->fgd_data, &fg_coulomb, sizeof(fg_coulomb));

		bm_debug("[fg_res] fg_coulomb = %d\n", fg_coulomb);
		gFG_coulomb_act = fg_coulomb;
		break;
	}

	case FG_DAEMON_CMD_GET_TEMPERTURE: {
		kal_bool update;
		int temperture = 0;

		memcpy(&update, &msg->fgd_data[0], sizeof(update));
		bm_debug("[fg_res] update = %d\n", update);
		temperture = force_get_tbat(update);
		bm_debug("[fg_res] temperture = %d\n", temperture);
		ret_msg->fgd_data_len += sizeof(temperture);
		memcpy(ret_msg->fgd_data, &temperture, sizeof(temperture));
		gFG_temp = temperture;

	} break;

	case FG_DAEMON_CMD_DUMP_REGISTER:
		battery_meter_ctrl(BATTERY_METER_CMD_DUMP_REGISTER, NULL);
		break;

	case FG_DAEMON_CMD_CHARGING_ENABLE: {
		kal_bool charging_enable = KAL_FALSE;

		battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);
		ret_msg->fgd_data_len += sizeof(charging_enable);
		memcpy(ret_msg->fgd_data, &charging_enable,
		       sizeof(charging_enable));

		bm_debug("[fg_res] charging_enable = %d\n", charging_enable);
	} break;

	case FG_DAEMON_CMD_GET_BATTERY_INIT_VOLTAGE: {
		ret_msg->fgd_data_len += sizeof(gFG_voltage_init);
		memcpy(ret_msg->fgd_data, &gFG_voltage_init,
		       sizeof(gFG_voltage_init));

		bm_debug("[fg_res] voltage = %d\n", gFG_voltage_init);
	} break;

	case FG_DAEMON_CMD_GET_BATTERY_VOLTAGE: {
		signed int update;
		int voltage = 0;

		memcpy(&update, &msg->fgd_data[0], sizeof(update));
		bm_debug("[fg_res] update = %d\n", update);
		if (update == 1)
			voltage = battery_meter_get_battery_voltage(KAL_TRUE);
		else
			voltage = BMT_status.bat_vol;

		ret_msg->fgd_data_len += sizeof(voltage);
		memcpy(ret_msg->fgd_data, &voltage, sizeof(voltage));

		bm_debug("[fg_res] voltage = %d\n", voltage);
	} break;

	case FG_DAEMON_CMD_FGADC_RESET:
		bm_debug("[fg_res] fgadc_reset\n");
		battery_meter_ctrl(BATTERY_METER_CMD_HW_RESET, NULL);
#ifdef FG_BAT_INT
		reset_fg_bat_int = KAL_TRUE;
#endif
#ifdef USING_SMOOTH_UI_SOC2
		pre_cc_act = 0;
#endif
		break;

	case FG_DAEMON_CMD_GET_BATTERY_PLUG_STATUS: {
		int plugout_status = 0;

		battery_meter_ctrl(BATTERY_METER_CMD_GET_BATTERY_PLUG_STATUS,
				   &plugout_status);
		ret_msg->fgd_data_len += sizeof(plugout_status);
		memcpy(ret_msg->fgd_data, &plugout_status,
		       sizeof(plugout_status));

		bm_debug("[fg_res] plugout_status = %d\n", plugout_status);

		gFG_plugout_status = plugout_status;
	} break;

	case FG_DAEMON_CMD_GET_RTC_SPARE_FG_VALUE: {
		signed int rtc_fg_soc = 0;

		rtc_fg_soc = get_rtc_spare_fg_value();
		ret_msg->fgd_data_len += sizeof(rtc_fg_soc);
		memcpy(ret_msg->fgd_data, &rtc_fg_soc, sizeof(rtc_fg_soc));

		bm_debug("[fg_res] rtc_fg_soc = %d\n", rtc_fg_soc);
	} break;

	case FG_DAEMON_CMD_IS_CHARGER_EXIST: {
		kal_bool charger_exist = KAL_FALSE;

		charger_exist = bat_is_charger_exist();
		ret_msg->fgd_data_len += sizeof(charger_exist);
		memcpy(ret_msg->fgd_data, &charger_exist,
		       sizeof(charger_exist));

		bm_debug("[fg_res] charger_exist = %d\n", charger_exist);
	} break;

	case FG_DAEMON_CMD_IS_BATTERY_FULL: {
		kal_bool battery_full = KAL_FALSE;

		battery_full = BMT_status.bat_full;
		ret_msg->fgd_data_len += sizeof(battery_full);
		memcpy(ret_msg->fgd_data, &battery_full, sizeof(battery_full));

		bm_debug("[fg_res] battery_full = %d\n", battery_full);
	} break;

	case FG_DAEMON_CMD_GET_BOOT_REASON: {
		signed int boot_reason = get_boot_reason();

		ret_msg->fgd_data_len += sizeof(boot_reason);
		memcpy(ret_msg->fgd_data, &boot_reason, sizeof(boot_reason));
		bm_debug(" ret_msg->fgd_data_len %d\n", ret_msg->fgd_data_len);
		bm_debug("[fg_res] g_boot_reason = %d\n", boot_reason);
	} break;

	case FG_DAEMON_CMD_GET_CHARGING_CURRENT: {
		signed int ICharging = battery_meter_get_charging_current();

		ret_msg->fgd_data_len += sizeof(ICharging);
		memcpy(ret_msg->fgd_data, &ICharging, sizeof(ICharging));

		bm_debug("[fg_res] ICharging = %d\n", ICharging);
	} break;

	case FG_DAEMON_CMD_GET_CHARGER_VOLTAGE: {
		signed int charger_vol = battery_meter_get_charger_voltage();

		ret_msg->fgd_data_len += sizeof(charger_vol);
		memcpy(ret_msg->fgd_data, &charger_vol, sizeof(charger_vol));

		bm_debug("[fg_res] charger_vol = %d\n", charger_vol);
	} break;

	case FG_DAEMON_CMD_GET_SHUTDOWN_COND: {
		unsigned int shutdown_cond =
			0; /* mt_battery_shutdown_check(); move to user space */

		ret_msg->fgd_data_len += sizeof(shutdown_cond);
		memcpy(ret_msg->fgd_data, &shutdown_cond,
		       sizeof(shutdown_cond));

		bm_debug("[fg_res] shutdown_cond = %d\n", shutdown_cond);
	} break;

	case FG_DAEMON_CMD_GET_CUSTOM_SETTING: {
		kal_bool version;

		memcpy(&version, &msg->fgd_data[0], sizeof(version));
		bm_debug("[fg_res] version = %d\n", version);

		if (version != CUST_SETTING_VERSION) {
			bm_debug("ERROR version 0x%x, expect 0x%x\n", version,
				 CUST_SETTING_VERSION);
			break;
		}

		memcpy(ret_msg->fgd_data, &batt_meter_cust_data,
		       sizeof(batt_meter_cust_data));
		ret_msg->fgd_data_len += sizeof(batt_meter_cust_data);

		memcpy(&ret_msg->fgd_data[ret_msg->fgd_data_len],
		       &batt_meter_table_cust_data,
		       sizeof(batt_meter_table_cust_data));
		ret_msg->fgd_data_len += sizeof(batt_meter_table_cust_data);

		bm_debug(
			"k fgauge_construct_profile_init1 %d:%d %d:%d %d:%d %d:%d %d:%d\n",
			batt_meter_table_cust_data.battery_profile_t0[0]
				.percentage,
			batt_meter_table_cust_data.battery_profile_t0[0]
				.voltage,
			batt_meter_table_cust_data.battery_profile_t0[10]
				.percentage,
			batt_meter_table_cust_data.battery_profile_t0[10]
				.voltage,
			batt_meter_table_cust_data.battery_profile_t0[20]
				.percentage,
			batt_meter_table_cust_data.battery_profile_t0[20]
				.voltage,
			batt_meter_table_cust_data.battery_profile_t0[30]
				.percentage,
			batt_meter_table_cust_data.battery_profile_t0[30]
				.voltage,
			batt_meter_table_cust_data.battery_profile_t0[40]
				.percentage,
			batt_meter_table_cust_data.battery_profile_t0[40]
				.voltage);
	} break;

	case FG_DAEMON_CMD_GET_UI_SOC: {
		ret_msg->fgd_data_len += sizeof(BMT_status.UI_SOC);
		memcpy(ret_msg->fgd_data, &(BMT_status.UI_SOC),
		       sizeof(BMT_status.UI_SOC));

		bm_debug("[fg_res] ui soc = %d\n", BMT_status.UI_SOC);
	} break;

	case FG_DAEMON_CMD_GET_CV_VALUE: {
		unsigned int cv_voltage;

		cv_voltage = get_cv_voltage();
		ret_msg->fgd_data_len += sizeof(cv_voltage);
		memcpy(ret_msg->fgd_data, &cv_voltage, sizeof(cv_voltage));

		bm_debug("[fg_res] cv value = %d\n", cv_voltage);
	} break;

	case FG_DAEMON_CMD_GET_DURATION_TIME: {
		int duration_time = 0;
		BATTERY_TIME_ENUM duration_type;

		memcpy(&duration_type, &msg->fgd_data[0],
		       sizeof(duration_type));
		bm_debug("[fg_res] duration_type = %d\n", duration_type);

		duration_time = mt_battery_get_duration_time(duration_type);
		ret_msg->fgd_data_len += sizeof(duration_time);
		memcpy(ret_msg->fgd_data, &duration_time,
		       sizeof(duration_time));

		bm_debug("[fg_res] duration time = %d\n", duration_time);
	} break;

	case FG_DAEMON_CMD_GET_TRACKING_TIME: {
		ret_msg->fgd_data_len += sizeof(battery_tracking_time);
		memcpy(ret_msg->fgd_data, &battery_tracking_time,
		       sizeof(battery_tracking_time));

		bm_debug("[fg_res] tracking time = %d\n",
			 battery_tracking_time);
	} break;

	case FG_DAEMON_CMD_GET_CURRENT_TH: {
		ret_msg->fgd_data_len += sizeof(suspend_current_threshold);
		memcpy(ret_msg->fgd_data, &suspend_current_threshold,
		       sizeof(suspend_current_threshold));

		bm_debug("[fg_res] suspend_current_threshold = %d\n",
			 suspend_current_threshold);
	} break;

	case FG_DAEMON_CMD_GET_CHECK_TIME: {
		ret_msg->fgd_data_len += sizeof(ocv_check_time);
		memcpy(ret_msg->fgd_data, &ocv_check_time,
		       sizeof(ocv_check_time));

		bm_debug("[fg_res] check time = %d\n", ocv_check_time);
	} break;

	case FG_DAEMON_CMD_GET_DIFFERENCE_VOLTAGE_UPDATE: {
		ret_msg->fgd_data_len += sizeof(difference_voltage_update);
		memcpy(ret_msg->fgd_data, &difference_voltage_update,
		       sizeof(difference_voltage_update));

		bm_debug("[fg_res] difference_voltage_update = %d\n",
			 difference_voltage_update);
	} break;

	case FG_DAEMON_CMD_GET_AGING1_LOAD_SOC: {
		ret_msg->fgd_data_len += sizeof(aging1_load_soc);
		memcpy(ret_msg->fgd_data, &aging1_load_soc,
		       sizeof(aging1_load_soc));

		bm_debug("[fg_res] aging1_load_soc = %d\n", aging1_load_soc);
	} break;

	case FG_DAEMON_CMD_GET_AGING1_UPDATE_SOC: {
		ret_msg->fgd_data_len += sizeof(aging1_update_soc);
		memcpy(ret_msg->fgd_data, &aging1_update_soc,
		       sizeof(aging1_update_soc));

		bm_debug("[fg_res] aging1_update_soc = %d\n",
			 aging1_update_soc);
	} break;

	case FG_DAEMON_CMD_GET_SHUTDOWN_SYSTEM_VOLTAGE: {
		ret_msg->fgd_data_len += sizeof(shutdown_system_voltage);
		memcpy(ret_msg->fgd_data, &shutdown_system_voltage,
		       sizeof(shutdown_system_voltage));

		bm_debug("[fg_res] shutdown_system_voltage = %d\n",
			 shutdown_system_voltage);
	} break;

	case FG_DAEMON_CMD_GET_CHARGE_TRACKING_TIME: {
		ret_msg->fgd_data_len += sizeof(charge_tracking_time);
		memcpy(ret_msg->fgd_data, &charge_tracking_time,
		       sizeof(charge_tracking_time));

		bm_debug("[fg_res] charge_tracking_time = %d\n",
			 charge_tracking_time);
	} break;

	case FG_DAEMON_CMD_GET_DISCHARGE_TRACKING_TIME: {
		ret_msg->fgd_data_len += sizeof(discharge_tracking_time);
		memcpy(ret_msg->fgd_data, &discharge_tracking_time,
		       sizeof(discharge_tracking_time));

		bm_debug("[fg_res] discharge_tracking_time = %d\n",
			 discharge_tracking_time);
	} break;

	case FG_DAEMON_CMD_GET_SHUTDOWN_GAUGE0: {
		ret_msg->fgd_data_len += sizeof(shutdown_gauge0);
		memcpy(ret_msg->fgd_data, &shutdown_gauge0,
		       sizeof(shutdown_gauge0));

		bm_debug("[fg_res] shutdown_gauge0 = %d\n", shutdown_gauge0);
	} break;

	case FG_DAEMON_CMD_GET_SHUTDOWN_GAUGE1_XMINS: {
		ret_msg->fgd_data_len += sizeof(shutdown_gauge1_xmins);
		memcpy(ret_msg->fgd_data, &shutdown_gauge1_xmins,
		       sizeof(shutdown_gauge1_xmins));

		bm_debug("[fg_res] shutdown_gauge1_xmins = %d\n",
			 shutdown_gauge1_xmins);
	} break;

	case FG_DAEMON_CMD_GET_SHUTDOWN_GAUGE1_MINS: {
		ret_msg->fgd_data_len += sizeof(shutdown_gauge1_mins);
		memcpy(ret_msg->fgd_data, &shutdown_gauge1_mins,
		       sizeof(shutdown_gauge1_mins));

		bm_debug("[fg_res] shutdown_gauge1_mins = %d\n",
			 shutdown_gauge1_mins);
	} break;

	case FG_DAEMON_CMD_SET_SUSPEND_TIME:
		bm_debug("[fg_res] set suspend time\n");
		get_monotonic_boottime(&suspend_time);
		break;

	case FG_DAEMON_CMD_SET_WAKEUP_SMOOTH_TIME: {
		memcpy(&wake_up_smooth_time, &msg->fgd_data[0],
		       sizeof(wake_up_smooth_time));
		bm_debug("[fg_res] wake_up_smooth_time = %d\n",
			 wake_up_smooth_time);
	} break;

	case FG_DAEMON_CMD_SET_IS_CHARGING: {
		memcpy(&gFG_coulomb_is_charging, &msg->fgd_data[0],
		       sizeof(gFG_coulomb_is_charging));
		bm_debug("[fg_res] is_charging = %d\n",
			 gFG_coulomb_is_charging);
	} break;

	case FG_DAEMON_CMD_SET_RBAT: {
		memcpy(&gFG_resistance_bat, &msg->fgd_data[0],
		       sizeof(gFG_resistance_bat));
		bm_debug("[fg_res] gFG_resistance_bat = %d\n",
			 gFG_resistance_bat);
	} break;

	case FG_DAEMON_CMD_SET_SWOCV: {
		memcpy(&gFG_voltage, &msg->fgd_data[0], sizeof(gFG_voltage));
		bm_debug("[fg_res] gFG_voltage = %d\n", gFG_voltage);
	} break;

	case FG_DAEMON_CMD_SET_DOD0: {
		memcpy(&gFG_DOD0, &msg->fgd_data[0], sizeof(gFG_DOD0));
		bm_debug("[fg_res] gFG_DOD0 = %d\n", gFG_DOD0);
	} break;

	case FG_DAEMON_CMD_SET_DOD1: {
		memcpy(&gFG_DOD1, &msg->fgd_data[0], sizeof(gFG_DOD1));
		bm_debug("[fg_res] gFG_DOD1 = %d\n", gFG_DOD1);
	} break;

	case FG_DAEMON_CMD_SET_QMAX: {
		memcpy(&gFG_BATT_CAPACITY_aging, &msg->fgd_data[0],
		       sizeof(gFG_BATT_CAPACITY_aging));
		bm_debug("[fg_res] QMAX = %d\n", gFG_BATT_CAPACITY_aging);
	} break;

	case FG_DAEMON_CMD_SET_BATTERY_FULL: {
		signed int battery_full;

		memcpy(&battery_full, &msg->fgd_data[0], sizeof(battery_full));
		BMT_status.bat_full = (kal_bool)battery_full;
		bm_debug("[fg_res] set bat_full = %d\n", BMT_status.bat_full);
	} break;

	case FG_DAEMON_CMD_SET_RTC: {
		signed int rtcvalue;

		memcpy(&rtcvalue, &msg->fgd_data[0], sizeof(rtcvalue));
		set_rtc_spare_fg_value(rtcvalue);
		bm_notice("[fg_res] set rtc = %d\n", rtcvalue);
	} break;

	case FG_DAEMON_CMD_SET_POWEROFF: {

		bm_debug("[fg_res] FG_DAEMON_CMD_SET_POWEROFF\n");
		kernel_power_off();
	} break;

	case FG_DAEMON_CMD_SET_INIT_FLAG: {
		memcpy(&init_flag, &msg->fgd_data[0], sizeof(init_flag));

		bm_notice("[fg_res] init_flag = %d\n", init_flag);
	} break;

	case FG_DAEMON_CMD_IS_KPOC: {
		signed int kpoc = bat_is_kpoc();

		ret_msg->fgd_data_len += sizeof(kpoc);
		memcpy(ret_msg->fgd_data, &kpoc, sizeof(kpoc));
		bm_debug("[fg_res] query kpoc = %d\n", kpoc);
	} break;

	case FG_DAEMON_CMD_SET_SOC: {
		memcpy(&gFG_capacity_by_c, &msg->fgd_data[0],
		       sizeof(gFG_capacity_by_c));
		bm_debug("[fg_res] SOC = %d\n", gFG_capacity_by_c);
		BMT_status.SOC = gFG_capacity_by_c;
	} break;

	case FG_DAEMON_CMD_SET_UI_SOC: {
		signed int UI_SOC;

		memcpy(&UI_SOC, &msg->fgd_data[0], sizeof(UI_SOC));
		bm_debug("[fg_res] UI_SOC = %d\n", UI_SOC);
		BMT_status.UI_SOC = UI_SOC;
	} break;

	case FG_DAEMON_CMD_SET_UI_SOC2: {
		signed int UI_SOC;

		memcpy(&UI_SOC, &msg->fgd_data[0], sizeof(UI_SOC));
		bm_debug("[fg_res] UI_SOC2 = %d\n", UI_SOC);
#ifdef USING_SMOOTH_UI_SOC2
		temp_UI_SOC2 = UI_SOC;
#else
		BMT_status.UI_SOC2 = UI_SOC;
#endif
		if (!g_battery_soc_ready) {
			g_battery_soc_ready = KAL_TRUE;
			gfg_percent_check_point = UI_SOC;
		}

		bat_update_thread_wakeup();
		/* wake_up_bat(); */
	} break;

	case FG_DAEMON_CMD_CHECK_FG_DAEMON_VERSION: {
		memcpy(&g_fgd_version, &msg->fgd_data[0],
		       sizeof(g_fgd_version));
		bm_debug("[fg_res] g_fgd_pid = %d\n", g_fgd_version);
		if (g_fgd_version != FGD_CHECK_VERSION) {
			bm_err("bad FG_DAEMON_VERSION 0x%x, 0x%x\n",
			       FGD_CHECK_VERSION, g_fgd_version);
		} else {
			bm_debug("FG_DAEMON_VERSION OK\n");
		}
	} break;

	case FG_DAEMON_CMD_SET_DAEMON_PID: {
		memcpy(&g_fgd_pid, &msg->fgd_data[0], sizeof(g_fgd_pid));
		bm_debug("[fg_res] g_fgd_pid = %d\n", g_fgd_pid);
	} break;

	case FG_DAEMON_CMD_SET_OAM_V_OCV: {
		signed int tmp;

		memcpy(&tmp, &msg->fgd_data[0], sizeof(tmp));
		bm_print(BM_LOG_CRTI, "[fg_res] OAM_V_OCV = %d\n", tmp);
		oam_v_ocv = tmp;
	} break;

	case FG_DAEMON_CMD_SET_OAM_R: {
		signed int tmp;

		memcpy(&tmp, &msg->fgd_data[0], sizeof(tmp));
		bm_print(BM_LOG_CRTI, "[fg_res] OAM_R = %d\n", tmp);
		oam_r = tmp;
	} break;

	case FG_DAEMON_CMD_GET_SUSPEND_TIME: {
		ret_msg->fgd_data_len += sizeof(swfg_ap_suspend_time);
		memcpy(ret_msg->fgd_data, &swfg_ap_suspend_time,
		       sizeof(swfg_ap_suspend_time));
		bm_print(BM_LOG_CRTI, "[fg_res] suspend_time = %d\n",
			 swfg_ap_suspend_time);
		swfg_ap_suspend_time = 0;
	} break;

	case FG_DAEMON_CMD_GET_SUSPEND_CAR: {
		signed int car = ap_suspend_car / 3600;

		ret_msg->fgd_data_len += sizeof(car);
		memcpy(ret_msg->fgd_data, &car, sizeof(car));
		bm_print(
			BM_LOG_CRTI,
			"[fg_res] ap_suspend_car:(%d:%d) t:%d hwocv:%d ocv:%d i:%d stime:%d:%d\n",
			ap_suspend_car, car, swfg_ap_suspend_time, last_hwocv,
			oam_v_ocv, last_i, total_suspend_times,
			this_suspend_times);
		ap_suspend_car = ap_suspend_car % 3600;
		this_suspend_times = 0;
	} break;

	case FG_DAEMON_CMD_IS_HW_OCV_UPDATE: {
		ret_msg->fgd_data_len += sizeof(is_hwocv_update);
		memcpy(ret_msg->fgd_data, &is_hwocv_update,
		       sizeof(is_hwocv_update));
		bm_print(BM_LOG_CRTI, "[fg_res] is_hwocv_update = %d\n",
			 is_hwocv_update);
		is_hwocv_update = KAL_FALSE;
	} break;

	case FG_DAEMON_CMD_SET_SWSOC: {
		signed int SWSOC;

		memcpy(&SWSOC, &msg->fgd_data[0], sizeof(SWSOC));
		bm_print(BM_LOG_CRTI, "[fg_res] SWSOC = %d\n", SWSOC);
		gFG_sw_soc = SWSOC;
	} break;

	case FG_DAEMON_CMD_SET_HWSOC: {
		signed int HWSOC;

		memcpy(&HWSOC, &msg->fgd_data[0], sizeof(HWSOC));
		bm_print(BM_LOG_CRTI, "[fg_res] HWSOC = %d\n", HWSOC);
		gFG_hw_soc = HWSOC;
	} break;

	case FG_DAEMON_CMD_SET_VBATSOC: {
		signed int VBATSOC;

		memcpy(&VBATSOC, &msg->fgd_data[0], sizeof(VBATSOC));
		bm_print(BM_LOG_CRTI, "[fg_res] VBATSOC = %d\n", VBATSOC);
		gFG_vbat_soc = VBATSOC;
	} break;

	case FG_DAEMON_CMD_SET_CAR_TUNE_VALUE: {
		signed int NVRAM_CAR_TUNE_VALUE;

		memcpy(&NVRAM_CAR_TUNE_VALUE, &msg->fgd_data[0],
		       sizeof(NVRAM_CAR_TUNE_VALUE));
		bm_notice("[fg_res] NVRAM_CAR_TUNE_VALUE = %d\n",
			  NVRAM_CAR_TUNE_VALUE);
		batt_meter_cust_data.car_tune_value = NVRAM_CAR_TUNE_VALUE;
	} break;

	default:
		bm_debug("bad FG_DAEMON_CTRL_CMD_FROM_USER 0x%x\n",
			 msg->fgd_cmd);
		break;
	} /* switch() */
}

int dod_init_in_kernel(void)
{
#if defined(FORCE_D0_IN_KERNEL)
	int preD0 = 0;
	int plugout_status = 0;
	int rtc_fg_soc = 0;
	bool chargerexist = KAL_FALSE;

	battery_meter_ctrl(BATTERY_METER_CMD_GET_BATTERY_PLUG_STATUS,
			   &plugout_status);
	rtc_fg_soc = get_rtc_spare_fg_value();
	chargerexist = bat_is_charger_exist();

	if (plugout_status == 0 && chargerexist == KAL_FALSE) {
		if (rtc_fg_soc == 0) {
			preD0 = gFG_sw_soc;
			bm_print(BM_LOG_CRTI,
				 "[%s]use gFG_sw_soc\n", __func__);
		} else {
			preD0 = rtc_fg_soc;
			bm_print(BM_LOG_CRTI,
				 "[%s]use rtc_fg_soc\n", __func__);
		}
	} else {
		if ((abs(gFG_hw_soc - rtc_fg_soc) > 30) &&
		    (abs(gFG_hw_soc - gFG_sw_soc) <
		     abs(gFG_sw_soc - rtc_fg_soc))) {
			if (abs(gFG_hw_soc - gFG_sw_soc) > 10) {
				preD0 = gFG_sw_soc;
				bm_print(
					BM_LOG_CRTI,
					"[%s]use gFG_sw_soc\n", __func__);
			} else {
				/* use hw ocv; */
				preD0 = gFG_hw_soc;
				bm_print(
					BM_LOG_CRTI,
					"[%s]use gFG_hw_soc\n", __func__);
			}
		} else {
			if ((abs(rtc_fg_soc - gFG_sw_soc) > 10) ||
			    rtc_fg_soc == 0) {
				preD0 = gFG_sw_soc;
				bm_print(
					BM_LOG_CRTI,
					"[%s]use gFG_sw_soc\n", __func__);
			} else {
				preD0 = rtc_fg_soc;
				bm_print(
					BM_LOG_CRTI,
					"[%s]use rtc_fg_soc\n", __func__);
			}
		}
	}
	bm_print(
		BM_LOG_CRTI,
		"[%s] rtc_fg_soc=%d, gFG_sw_soc=%d, gFG_hw_soc=%d, preD0=%d, plugout_status=%d, chargerexist=%d\n",
		__func__, rtc_fg_soc, gFG_sw_soc,
		gFG_hw_soc, preD0, plugout_status,
		chargerexist);
	gFG_DOD0 = 100 - preD0;
#else
	gFG_DOD0 = 200;
#endif

	bm_print(BM_LOG_CRTI, "[%s]gFG_DOD0 = %d\n", __func__, gFG_DOD0);
	return gFG_DOD0;
}

static void nl_send_to_user(u32 pid, int seq, struct fgd_nl_msg_t *reply_msg)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	/* int size=sizeof(struct fgd_nl_msg_t); */
	int size = reply_msg->fgd_data_len + FGD_NL_MSG_T_HDR_LEN;
	int len = NLMSG_SPACE(size);
	void *data;
	int ret;

	skb = alloc_skb(len, GFP_ATOMIC);
	if (!skb)
		return;

	nlh = nlmsg_put(skb, pid, seq, 0, size, 0);
	data = NLMSG_DATA(nlh);
	memcpy(data, reply_msg, size);
	NETLINK_CB(skb).portid = 0;    /* from kernel */
	NETLINK_CB(skb).dst_group = 0; /* unicast */

	/* bm_debug("[Netlink] nl_reply_user: */
	/* netlink_unicast size=%d fgd_cmd=%d */
	/* pid=%d\n", */
	/* /size, reply_msg->fgd_cmd, pid); */
	ret = netlink_unicast(daemo_nl_sk, skb, pid, MSG_DONTWAIT);
	if (ret < 0) {
		bm_err("[Netlink] send failed %d\n", ret);
		return;
	}
	/*bm_debug("[Netlink] reply_user: netlink_unicast- ret=%d\n", ret); */
}

static void nl_data_handler(struct sk_buff *skb)
{
	u32 pid;
	kuid_t uid;
	int seq;
	void *data;
	struct nlmsghdr *nlh;
	struct fgd_nl_msg_t *fgd_msg, *fgd_ret_msg;
	int size = 0;

	nlh = (struct nlmsghdr *)skb->data;
	pid = NETLINK_CREDS(skb)->pid;
	uid = NETLINK_CREDS(skb)->uid;
	seq = nlh->nlmsg_seq;

	/*bm_debug("[Netlink] recv skb from user space uid:%d pid:%d */
	/* seq:%d\n",uid,pid,seq); */
	data = NLMSG_DATA(nlh);

	fgd_msg = (struct fgd_nl_msg_t *)data;

	size = fgd_msg->fgd_ret_data_len + FGD_NL_MSG_T_HDR_LEN;

	fgd_ret_msg = vmalloc(size);
	if (!fgd_ret_msg) {
		/* bm_err("Error: nl_data_handler() vmalloc fail!!!\n"); */
		return;
	}

	memset(fgd_ret_msg, 0, size);

	bmd_ctrl_cmd_from_user(data, fgd_ret_msg);
	nl_send_to_user(pid, seq, fgd_ret_msg);
	/*bm_print(BM_LOG_CRTI,"[Netlink] send to user space process done\n");*/

	vfree(fgd_ret_msg);
}

int wakeup_fg_algo(int flow_state)
{
	update_fg_dbg_tool_value();

	if (gDisableFG) {
		bm_notice("FG daemon is disabled\n");
		return -1;
	}

	if (g_fgd_pid != 0) {
		struct fgd_nl_msg_t *fgd_msg;
		int size = FGD_NL_MSG_T_HDR_LEN + sizeof(flow_state);

		fgd_msg = vmalloc(size);
		if (!fgd_msg)
			return -1;

		bm_debug("[battery_meter_driver] malloc size=%d\n", size);
		memset(fgd_msg, 0, size);
		fgd_msg->fgd_cmd = FG_DAEMON_CMD_NOTIFY_DAEMON;
		memcpy(fgd_msg->fgd_data, &flow_state, sizeof(flow_state));
		fgd_msg->fgd_data_len += sizeof(flow_state);
		nl_send_to_user(g_fgd_pid, 0, fgd_msg);
		vfree(fgd_msg);
		return 0;
	} else {
		return -1;
	}
}

static int __init battery_meter_init(void)
{
	int ret;
	/* add by willcai for the userspace  to kernelspace */
	struct netlink_kernel_cfg cfg = {
		.input = nl_data_handler,
	};
/* end */

#ifdef CONFIG_OF
/* */
#else
	ret = platform_device_register(&battery_meter_device);
	if (ret) {
		bm_err("[battery_meter_driver] Unable to device register(%d)\n",
		       ret);
		return ret;
	}
#endif

	ret = platform_driver_register(&battery_meter_driver);
	if (ret) {
		bm_err("[battery_meter_driver] Unable to register driver (%d)\n",
		       ret);
		return ret;
	}
#ifdef CONFIG_OF
	ret = platform_driver_register(&battery_meter_dts_driver);
#endif

	/* add by willcai for the userspace to kernelspace */

	/* daemo_nl_sk = netlink_kernel_create */
	/* (&init_net, NETLINK_TEST, 0, nl_data_handler, NULL, THIS_MODULE); */
	daemo_nl_sk = netlink_kernel_create(&init_net, NETLINK_FGD, &cfg);
	bm_debug("netlink_kernel_create protol= %d\n", NETLINK_FGD);

	if (daemo_nl_sk == NULL) {
		bm_err("netlink_kernel_create error\n");
		return -1;
	}
	bm_debug("netlink_kernel_create ok\n");

	bm_debug("[battery_meter_driver] Initialization : DONE\n");

	return 0;
}

#ifdef BATTERY_MODULE_INIT
device_initcall(battery_meter_init);
#else
static void __exit battery_meter_exit(void)
{
}
module_init(battery_meter_init);
/* module_exit(battery_meter_exit); */
#endif

MODULE_AUTHOR("James Lo");
MODULE_DESCRIPTION("Battery Meter Device Driver");
MODULE_LICENSE("GPL");
