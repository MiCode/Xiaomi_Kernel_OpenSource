// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2011 Unisoc Co., Ltd.
 * Changhua.Zhang <Changhua.Zhang@unisoc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/gpio/consumer.h>
#include <linux/iio/consumer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/power_supply.h>
#include <linux/power/charger-manager.h>
#include <linux/power/sprd_battery_info.h>
#include <linux/power/sprd_fuel_gauge_core.h>
#include <linux/regmap.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/usb/phy.h>
#include "battery_secret/battery_secret_class.h"
#include "battery_secret/battery_secret_logic.h"
#include "battery_secret/secret_common.h"

/* third fuel */
#define THIRD_FUEL_NAME					"sprd_third_fuel"
#define SPRD_THIRD_FUEL_TEMP_MIN			(-400)
#define SPRD_THIRD_FUEL_TEMP_MAX			1200
/* rtc reg default value */
#define SPRD_FGU_DEFAULT_CAP				GENMASK(11, 0)
#define SPRD_FGU_OCV_REG_DEFAUL				GENMASK(15, 0)
#define SPRD_FGU_BATT_OCV_DEFAUL			GENMASK(13, 0)
#define SPRD_FGU_INIT_CC_DEFAUL				GENMASK(13, 0)

#define SPRD_FGU_NORMAL_POWERON				0x5
#define SPRD_FGU_RTC2_RESET_VALUE			0xA05
/* uusoc vbat */
#define SPRD_FGU_LOW_VBAT_3450			3450
#define SPRD_FGU_LOW_VBAT_3400			3400

/* sleep calib */
#define SPRD_FGU_SLP_CAP_CALIB_SLP_TIME			300
#define SPRD_FGU_CAP_CALIB_TEMP_LOW			100
#define SPRD_FGU_CAP_CALIB_TEMP_HI			450
#define SPRD_FGU_CAP_CALIB_TEMP_MIN			-100
#define SPRD_FGU_CAP_CALIB_NORMAL_TEMP_THR		100
#define SPRD_FGU_CAP_CALIB_LOW_TEMP_THR			0
#define SPRD_FGU_SR_ARRAY_LEN				200
#define SPRD_FGU_SR_STOP_CHARGE_TIMES			(30 * 60)
#define SPRD_FGU_SR_SLEEP_MIN_TIME_S			(10 * 60)
#define SPRD_FGU_SR_AWAKE_MAX_TIME_S			90
#define SPRD_FGU_SR_AWAKE_BIG_CUR_MAX_TIME_S		30
#define SPRD_FGU_SR_SLEEP_AVG_CUR_MA			30
#define SPRD_FGU_SR_SLEEP_AWAKE_AVG_CUR_MA		50
#define SPRD_FGU_SR_EASILY_SLEEP_AVG_CUR_MA		100
#define SPRD_FGU_SR_COARSE_SLEEP_AVG_CUR_MA		150
#define SPRD_FGU_SR_AWAKE_AVG_CUR_MA			200
#define SPRD_FGU_SR_LAST_SLEEP_TIME_S			(4 * 60)
#define SPRD_FGU_SR_EASILY_LAST_SLEEP_TIME_S		(2 * 60)
#define SPRD_FGU_SR_COARSE_LAST_SLEEP_TIME_S		10
#define SPRD_FGU_SR_LAST_AWAKE_TIME_S			30
#define SPRD_FGU_SR_EASILY_LAST_AWAKE_TIME_S		60
#define SPRD_FGU_SR_DUTY_RATIO				95
#define SPRD_FGU_SR_EASILY_DUTY_RATIO			80
#define SPRD_FGU_SR_COARSE_DUTY_RATIO			5
#define SPRD_FGU_SR_TOTAL_TIME_S			(30 * 60)
#define SPRD_FGU_SR_VALID_VOL_CNT			3
#define SPRD_FGU_SR_VALID_MAX_RANGE			50
#define SPRD_FGU_SR_MAX_VOL_MV				4500
#define SPRD_FGU_SR_MIN_VOL_MV				3400
#define SPRD_FGU_SR_CALIB_WAKE_UP_MS			1000
#define SPRD_FGU_SR_CALIB_VALID_TIME			(1 * 60)
#define SPRD_FGU_CALIB_PRECI_H_DENS_NOR_TEMP_CAP_DIFF	100
#define SPRD_FGU_CALIB_PRECI_H_DENS_LOW_TEMP_CAP_DIFF	120
#define SPRD_FGU_CALIB_PRECI_H_DENS_COOL_TEMP_CAP_DIFF	150
#define SPRD_FGU_CALIB_MODER_L_DENS_NOR_TEMP_CAP_DIFF	30
#define SPRD_FGU_CALIB_MODER_L_DENS_LOW_TEMP_CAP_DIFF	40
#define SPRD_FGU_CALIB_MODER_L_DENS_COOL_TEMP_CAP_DIFF	70
#define SPRD_FGU_CALIB_MODER_H_DENS_NOR_TEMP_CAP_DIFF	100
#define SPRD_FGU_CALIB_MODER_H_DENS_LOW_TEMP_CAP_DIFF	120
#define SPRD_FGU_CALIB_MODER_H_DENS_COOL_TEMP_CAP_DIFF	150
#define SPRD_FGU_CALIB_COAR_L_DENS_NOR_TEMP_CAP_DIFF	40
#define SPRD_FGU_CALIB_COAR_L_DENS_LOW_TEMP_CAP_DIFF	60
#define SPRD_FGU_CALIB_COAR_L_DENS_COOL_TEMP_CAP_DIFF	80
#define SPRD_FGU_CALIB_COAR_H_DENS_NOR_TEMP_CAP_DIFF	200
#define SPRD_FGU_CALIB_COAR_H_DENS_LOW_TEMP_CAP_DIFF	300
#define SPRD_FGU_CALIB_COAR_H_DENS_COOL_TEMP_CAP_DIFF	400
/* track cap */
#define SPRD_FGU_TRACK_HIGH_TEMP_THRESHOLD		450
#define SPRD_FGU_TRACK_LOW_TEMP_THRESHOLD		150
#define SPRD_FGU_TRACK_TIMEOUT_THRESHOLD		(12 * 3600)
#define SPRD_FGU_TRACK_NEW_OCV_VALID_THRESHOLD		(30 * 60)
#define SPRD_FGU_TRACK_LEARNED_CAP_HTHRESHOLD		350
#define SPRD_FGU_TRACK_START_CAP_THRESHOLD		650
#define SPRD_FGU_TRACK_WAKE_UP_MS			16000
#define SPRD_FGU_TRACK_UPDATING_WAKE_UP_MS		200
#define SPRD_FGU_TRACK_DONE_WAKE_UP_MS			1000
#define SPRD_FGU_TRACK_OCV_VALID_TIME			15
#define SPRD_FGU_TRACK_CHARGE_CYCLE_DIFF		200000
#define SPRD_FGU_TRACK_LIMIT_PERCENT			30
#define SPRD_FGU_CAPACITY_TRACK_0S			0
#define SPRD_FGU_CAPACITY_TRACK_3S			3
#define SPRD_FGU_CAPACITY_TRACK_15S			15
#define SPRD_FGU_CAPACITY_TRACK_100S			100
#define SPRD_FGU_PROBE_TIMEOUT				msecs_to_jiffies(500)
/* unuse cap */
#define SPRD_FGU_RESIST_ALG_REIST_CNT			40
#define SPRD_FGU_RESIST_ALG_OCV_GAP_UV			20000
#define SPRD_FGU_RESIST_ALG_OCV_CNT			10
#define SPRD_FGU_RBAT_CMP_MOH				10
/* RTC OF 2021-08-06 15 : 44*/
#define SPRD_FGU_MISCDATA_RTC_TIME			(1621355101)
#define SPRD_FGU_SHUTDOWN_TIME				(15 * 60)
#define SPRD_FGU_SHUTDOWN_DAY_TIME			(24 * 60 * 60)
/* debug value config */
#define SPRD_FGU_DEBUG_TEMP_CELSIUS			200
#define SPRD_FGU_DEBUG_VBAT_NOW_UV			4000000
#define SPRD_FGU_DEBUG_CUR_NOW_UV			1000000
#define SPRD_FGU_DEBUG_VBUS_UV				5000000
#define SPRD_FGU_DEBUG_OCV_UV				4000000
/* others define */
#define SPRD_FGU_NORMAL_WORK_10S			10
#define SPRD_FGU_QUICKEN_WORK_5S			5
#define SPRD_FGU_CAP_CALC_WORK_LOW_TEMP			50
#define SPRD_FGU_CAP_CALC_WORK_LOW_CAP			50
#define SPRD_FGU_CAP_CALC_WORK_BIG_CURRENT		3000
#define SPRD_FGU_POCV_VOLT_THRESHOLD			3400
#define SPRD_FGU_UUOCV_VOLT_THRESHOLD			3800
#define SPRD_FGU_POCI_VALID_THRESHOLD			150
#define SPRD_FGU_POCV_VALID_TIMER_THRESHOLD		(3 * 60)
#define SPRD_FGU_REBOOT_POCV_VALID_TIMER_THR		(1 * 60)
#define SPRD_FGU_ABS_START_CHARGE_TIMES			(20 * 60)
#define SPRD_FGU_ABS_FULL_START_CHARGE_TIMES		(10 * 60)
#define SPRD_FGU_TEMP_BUFF_CNT				10
#define SPRD_FGU_LOW_TEMP_REGION			100
#define SPRD_FGU_CAP_REMAP_TEMP_REGION			(-200)
#define SPRD_FGU_CURRENT_BUFF_CNT			8
#define SPRD_FGU_DISCHG_CNT				4
#define SPRD_FGU_VOLTAGE_BUFF_CNT			8
#define SPRD_FGU_MAGIC_NUMBER				0x5a5aa5a5
#define SPRD_FGU_MAGIC_NUMBER2				0xa5a55a5a
#define SPRD_FGU_DEBUG_EN_CMD				0x5a5aa5a5
#define SPRD_FGU_DEBUG_DIS_CMD				0x5a5a5a5a
#define SPRD_FGU_GOOD_HEALTH_CMD			0x7f7f7f7f
#define SPRD_FGU_FCC_PERCENT				1000
#define SPRD_FGU_RESERVE_SOC				10
#define SPRD_FGU_HC_SOC					10
#define SPRD_FGU_EXTCON_SINK				3
#define SPRD_FGU_GET_CHG_TYPE_RETRY_CNT			30
#define SPRD_FGU_IS_SWITCH_BAT_PARA_VOL_THRES		4100
#define SPRD_FGU_REG_MAX				0x260
#define SPRD_FGU_FULL_PERCENT				100
#define SPRD_FGU_LP_OCV_CALIB_VALID_TIMES		(60 * 60)
#define SPRD_FGU_BOOT_OCV_CALIB_VALID_TIMES		(30 * 60)
#define SPRD_FGU_UUOCV_OFFSET_VOL_UV			(0 * 1000)
#define SPRD_FGU_RT_CALIB_INFO_TABLE_LEN		400
#define SPRD_FGU_RT_FULL_CALIB_INFO_TABLE_LEN		5
#define SPRD_FGU_RT_FULL_CALIB_MAX_IBAT_UA		(1800 * 1000)
#define SPRD_FGU_RT_FULL_CALIB_IBAT_DELAT_THD_UA	(500 * 1000)
#define SPRD_FGU_RT_FULL_CALIB_VBAT_DELTA_THD_UV	(30 * 1000)
#define SPRD_FGU_VBAT_THRES_COLS_MAX			20
#define SPRD_FGU_VBAT_DEF_THRES_MAX			4800
#define SPRD_FGU_VBAT_DEF_THRES_MIN			3450

#define SPRD_FGU_BOOT_ACCUR_CALIB_TEMP			200
#define SPRD_FGU_DELTA_SOC_BUFF_SIZE			20

#define SPRD_FGU_IBAT_AVG_BUFF_CNT			10
#define SPRD_FGU_CHARGING_IAVG_MA			200
#define SPRD_FGU_FULL_CHARGING_MA			20
#define SPRD_FGU_CHG_TERM_LOOP_TRIG_CNT			3
#define SPRD_FGU_CHG_TERM_LOOP_NUM_SUM			8
#define SPRD_FGU_FULL_OCV_TRIG_CNT			3
#define SPRD_FGU_FULL_OCV_VALID_ENTER_TIME		(30 * 60)
#define SPRD_FGU_STOP_CHG_VALID_ENTER_TIME		(30 * 60)
#define SPRD_FGU_ADJ_UUIBAT_VALLID_ENTER_TIME		(500)
#define SPRD_FGU_FULL_OCV_VALID_CHECK_TIME		15
#define SPRD_FGU_FULL_OCV_INTERVAL_TIME			15
#define SPRD_FGU_OCV_INTERVAL_TIME			(10 * 60)
#define SPRD_FGU_DISCHG_TERM_LOOP_TRIG_CNT		3
#define SPRD_FGU_DISCHG_TERM_LOOP_NUM_SUM		6
#define SPRD_FGU_DISCHG_TERM_VOL_OFFSET			5
#define SPRD_FGU_TEMP_DEFAULT				250
#define SPRD_FGU_TEMP_BOOT_THD				100
#define SPRD_FGU_DEFAULT_ZP_VOL_MV			3400
#define SPRD_FGU_STOP_CHG_H_TEMP_THR			550
#define SPRD_FGU_STOP_CHG_L_TEMP_THR			0
#define SPRD_FGU_ADJUST_UUIBAT_RATIO			100
#define SPRD_FGU_ADJUST_UUIBAT_CUR_THR			-1000

#define SPRD_FGU_RT_CALIB_CUR1_TIME			(5 * 60)
#define SPRD_FGU_RT_CALIB_CUR2_TIME			(15 * 60)
#define SPRD_FGU_RT_CALIB_CUR3_TIME			(30 * 60)
#define SPRD_FGU_ADJ_UUIBAT_CUR_TIME			(5 * 60)
#define SPRD_FGU_RT_CALIB_CUR_THD_MA			(1500)
#define SPRD_FGU_RT_CALIB_CHG_CUR_THD_MA		(2000)
#define SPRD_FGU_RT_CALIB_DIS_CHG_CUR_THD_MA		(-1500)
#define SPRD_FGU_RT_CALIB_OCV_VALID_CNT			(3)
#define SPRD_FGU_RT_CALI_TEMP_15_THD			(150)
#define SPRD_FGU_RT_CALI_TEMP_0_THD			(0)
#define SPRD_FGU_BATT_OCV_UV_LOW_LIMIT			2000000
#define SPRD_FGU_BATT_OCV_UV_UP_LIMIT			5000000

#define SW_LOW_BAT_UVLO_CONF_MV  			3350
#define USB_ONLINE_LOW_BAT_CONF_MV			3300

#define OCV_ERR			BIT(0)
#define VBAT_ERR		BIT(1)
#define VBAT_AVG_ERR		BIT(2)
#define VBAT_CUR_ERR		BIT(3)
#define VBAT_CUR_AVG_ERR	BIT(4)

struct power_supply_vol_temp_table {
	int vol;	/* microVolts */
	int temp;	/* celsius */
};

enum sprd_fgu_track_state {
	CAP_TRACK_INIT,
	CAP_TRACK_IDLE,
	CAP_TRACK_UPDATING,
	CAP_TRACK_DONE,
	CAP_TRACK_ERR,
};

enum sprd_fgu_track_mode {
	CAP_TRACK_MODE_UNKNOWN,
	CAP_TRACK_MODE_SW_OCV,
	CAP_TRACK_MODE_POCV,
	CAP_TRACK_MODE_LP_OCV,
};

enum sprd_fgu_sr_calib_mode {
	SR_CALIB_NONE,
	SR_CALIB_PRECISION,
	SR_CALIB_MODERATE,
	SR_CALIB_COARSE,
	SR_CALIB_MAX,
};

enum sprd_fgu_chg_term_enable_mode {
	CHG_TERM_ENABLE_MODE_OFF,
	CHG_TERM_ENABLE_MODE_IBAT = BIT(1),
	CHG_TERM_ENABLE_MODE_SOC = BIT(2),
	CHG_TERM_ENABLE_MODE_ABS_IBAT = BIT(3),
};

struct sprd_fgu_ocv_info {
	s64 ocv_time_stamp;
	int ocv_uv;
	bool valid;
	bool is_low_density;
};

struct sprd_fgu_track_capacity {
	enum sprd_fgu_track_state state;
	bool clear_cap_flag;
	int start_cc_mah;
	int start_cap;
	int start_ocv;
	int end_cap;
	int delta_cap;
	int end_vol;
	int end_cur;
	s64 start_time;
	bool full_chg_track_enable;
	bool lpocv_learned_done;
	int learned_mah;
	int boot_uah;
	int start_aging_bat_id;
	struct sprd_fgu_ocv_info lpocv_info;
	struct sprd_fgu_ocv_info pocv_info;
	density_ocv_table *dens_ocv_table;
	int dens_ocv_table_len;
	enum sprd_fgu_track_mode mode;
};

struct sprd_fgu_debug_info {
	bool temp_debug_en;
	bool vbat_now_debug_en;
	bool ocv_debug_en;
	bool cur_now_debug_en;
	bool batt_present_debug_en;
	bool chg_vol_debug_en;
	bool batt_health_debug_en;
	bool smooth_soc_debug_en;
	bool charge_full_debug_en;

	int debug_temp;
	int debug_vbat_now;
	int debug_ocv;
	int debug_cur_now;
	bool debug_batt_present;
	int debug_chg_vol;
	int debug_batt_health;

	int sel_reg_id;
};

struct sprd_fgu_sysfs {
	char *name;
	struct attribute_group attr_g;
	struct device_attribute attr_sprd_fgu_dump_info;
	struct device_attribute attr_sprd_fgu_sel_reg_id;
	struct device_attribute attr_sprd_fgu_reg_val;
	struct device_attribute attr_sprd_fgu_enable_sleep_calib;
	struct device_attribute attr_sprd_fgu_relax_cnt_th;
	struct device_attribute attr_sprd_fgu_relax_cur_th;
	struct device_attribute attr_sprd_fgu_real_time_calib;
	struct device_attribute attr_sprd_fgu_capacity_remap_enable;
	struct device_attribute attr_sprd_fgu_capacity_hc_enable;
	struct attribute *attrs[10];

	struct sprd_fgu_data *data;
};

/*
 * struct sprd_fgu_cap_remap_table
 * @cnt: record the counts of battery capacity of this scope
 * @lcap: the lower boundary of the capacity scope before transfer
 * @hcap: the upper boundary of the capacity scope before transfer
 * @lb: the lower boundary of the capacity scope after transfer
 * @hb: the upper boundary of the capacity scope after transfer
 */
struct sprd_fgu_cap_remap_table {
	int cnt;
	int lcap;
	int hcap;
	int lb;
	int hb;
};

struct sprd_fgu_vbat_info {
	int full_alarm_table_len;
	int ocv_uv;
	int vbat_mv;
	int vbat_avg_mv;
	int vbat_cur_ma;
	int vbat_cur_avg_ma;
	int zp_vol_uv;
	int vbat_avg_mv_dy;
	unsigned int vbat_err;
	bool absolute_charger_mode;
};

struct sprd_fgu_rt_calib_info {
	int rm_soc;
	int rm_ocv_uv;
	int calib_soc;
	int calib_ocv_uv;
	int vbat_avg_mv;
	int cur_avg_ma;
	int work_cur_avg_ma;
	int work_times;
	int delta_ocv_uv;
	int batt_temp;
};

struct sprd_fgu_rt_full_calib_info {
	int calib_ocv_soc;
	int calib_ocv_uv;
	int vbat_avg_mv;
	int cur_avg_ma;
	int work_cur_avg_ma;
	int work_times;
	int batt_soc;
	int batt_cali_ocv_soc;
	int calib_ocv_uah;
};

struct sprd_fgu_adj_uuibat_dischg_calib_info {
	int cur_avg_ma;
	int work_cur_avg_ma;
	int work_times;
	int batt_temp;
};

/*
 * struct sprd_fgu_data: describe the FGU device
 * @regmap: regmap for register access
 * @dev: platform device
 * @battery: battery power supply
 * @base: the base offset for the controller
 * @lock: protect the structure
 * @gpiod: GPIO for battery detection
 * @channel: IIO channel to get battery temperature
 * @charge_chan: IIO channel to get charge voltage
 * @internal_resist: the battery internal resistance in mOhm
 * @total_mah: the total capacity of the battery in mAh
 * @init_cap: the initial capacity of the battery in mAh
 * @max_volt_uv: the maximum constant input voltage in millivolt
 * @min_volt_uv: the minimum drained battery voltage in microvolt
 * @boot_volt_uv: the voltage measured during boot in microvolt
 * @table_len: the capacity table length
 * @temp_table_len: temp_table length
 * @cap_table_len：the capacity temperature table length
 * @resist_table_len: the resistance table length
 * @comp_resistance: the coulomb counter internal and the board ground resistance
 * @index: record temp_buff array index
 * @temp_buff: record the battery temperature for each measurement
 * @bat_temp: the battery temperature
 * @temp_table: the NTC voltage table with corresponding battery temperature
 * @cap_temp_table: the capacity table with corresponding temperature
 * @resist_table: resistance percent table with corresponding temperature
 */

#define BATT_MA_AVG_SAMPLES	8
struct batt_params {
	bool			update_now;
	int			batt_raw_soc;
	int			batt_soc;
	int			samples_num;
	int			samples_index;
	int			batt_ma_avg_samples[BATT_MA_AVG_SAMPLES];
	int			batt_ma_avg;
	int			batt_ma_prev;
	int			batt_ma;
	int			batt_mv;
	int			batt_temp;
	int			batt_rmc;/* Remaining capacity */
	int			batt_volt;
	int			batt_curr;
	int			batt_status;
	ktime_t		last_soc_change_time;
};

struct sprd_fgu_data {
	struct regmap *regmap;
	struct device *dev;
	struct power_supply *battery;
	struct power_supply *batt_psy;
	u32 base;
	struct mutex lock;
	struct mutex discharge_lock;
	struct gpio_desc *gpiod;
	struct iio_channel *channel;
	struct iio_channel *charge_chan;
	struct sprd_fgu_vbat_info vbat_info;
	struct sprd_fgu_rt_calib_info calib_info[SPRD_FGU_RT_CALIB_INFO_TABLE_LEN];
	struct sprd_fgu_rt_full_calib_info full_calib_info[SPRD_FGU_RT_CALIB_INFO_TABLE_LEN];
	struct sprd_fgu_adj_uuibat_dischg_calib_info dischg_calib_info[SPRD_FGU_RT_CALIB_INFO_TABLE_LEN];
	int rt_full_calib_index;
	int rt_full_calib_valid_cnt;
	int rt_full_calib_mix_soc;
	int rt_full_adj_calib_ocv_soc;
	int rt_calib_index;
	int rt_calib_valid_cnt;
	int rt_calib_start_time;
	int dischg_calib_index;
	int dischg_calib_valid_cnt;
	int dischg_calib_start_time;
	int dischg_dynamic_zp_uv;
	bool rt_calib_vaild;
	bool bat_present;
	bool last_bat_present;
	int internal_resist;
	int total_mah;
	int design_mah;
	int charge_full_mah;
	int shutdown_voltage_mv;
	int abs_zero_point_uv;
	int boot_cap;
	int pocv_time;
	int temp_cap;
	int smooth_soc;
	int smooth_soc_decimal;
	int ocv_adjust_decimal;
	int vbat_adjust_decimal;
	int bat_soc;
	int bat_soh;
	int last_batt_soh;
	int last_batt_cycle;
	int last_chip_soh;
	int last_chip_cycle;
	int uusoc_mah;
	int init_mah;
	int cc_uah;
	int max_volt_uv;
	int min_volt_uv;
	int boot_volt_uv;
	int table_len;
	int temp_table_len;
	int resist_table_len;
	int fullcap_table_len;
	int cap_calib_dens_ocv_table_len;
	int work_cycle;
	unsigned int comp_resistance;
	unsigned int com_pcb_resistance;
	unsigned int fullbatt_uV;
	unsigned int fullbatt_uA;
	int trigger_cnt;
	int batt_ovp_threshold;
	int index;
	int temp_buff[SPRD_FGU_TEMP_BUFF_CNT];
	int cur_now_buff[SPRD_FGU_CURRENT_BUFF_CNT];
	bool dischg_trend[SPRD_FGU_DISCHG_CNT];
	int delta_soc_value[SPRD_FGU_DELTA_SOC_BUFF_SIZE];
	int bat_temp;
	int last_bat_temp;
	bool online;
	bool is_first_poweron;
	bool is_ovp;
	bool invalid_pocv;
	bool sw_pocv_flag;
	bool volt_low_flag;
	int poci_ma;
	u32 chg_type;
	int cap_remap_total_cnt;
	int cap_remap_full_percent;
	int cap_remap_table_len;
	struct sprd_fgu_cap_remap_table *cap_remap_table;
	struct sprd_fgu_track_capacity track;
	struct power_supply_vol_temp_table *temp_table;
	struct sprd_battery_temp_fullcap_table *temp_fullcap_table;
	struct power_supply_resistance_temp_table *resist_table;
	struct usb_phy *usb_phy;
	struct notifier_block usb_notify;
	int chg_sts;
	int last_chg_sts;
	struct sprd_fgu_debug_info debug_info;
	density_ocv_table *cap_calib_dens_ocv_table;
	struct sprd_battery_cycles_fcc_table *battery_cycles_fcc_table;

	struct sprd_fgu_sysfs *sysfs;
	struct delayed_work cap_track_work;
	struct delayed_work fgu_work;
	struct w1_data *w1sec_info;
	struct delayed_work update_delay_work;
	struct sprd_fgu_info *fgu_info;
	struct batt_params	param;
	bool first_flag;
	int raw_soc;

	/* typec extcon */
	struct extcon_dev *edev;
	struct notifier_block pd_swap_notify;
	struct notifier_block extcon_nb;
	struct work_struct typec_extcon_work;
	bool is_sink;
	bool use_typec_extcon;

	bool support_debug_log;

	/* boot capacity calibration */
	s64 shutdown_rtc_time;
	bool is_reboot;

	/* charge cycle */
	int charge_cycle;
	int fake_temp;

	/* battery aging function */
	bool support_bat_aging;
	int last_aging_bat_id;
	bool dynamic_update_bat_para_flag;
	int bat_fcc_aging_ratio;

	/* battery high/low int thers table */
	int *vbat_thres_temp_table;
	int vbat_thres_temp_table_len;
	int **high_int_thres_table;
	int high_int_thres_cols;
	int **low_int_thres_table;
	int low_int_thres_cols;
	int vbat_level;

	int work_enter_cc_uah;
	int work_exit_cc_uah;
	int last_cc_uah;
	s64 work_enter_times;
	s64 work_exit_times;
	s64 work_cali_exit_times;
	s64 start_charge_times;

	/* sleep resume calibration */
	s64 awake_times;
	s64 sleep_times;
	s64 stop_charge_times;
	int sleep_cc_uah;
	int awake_cc_uah;
	int awake_avg_cur_ma;
	int sr_time_sleep[SPRD_FGU_SR_ARRAY_LEN];
	int sr_time_awake[SPRD_FGU_SR_ARRAY_LEN];
	int sr_avg_cur_ma_sleep[SPRD_FGU_SR_ARRAY_LEN];
	int sr_avg_cur_ma_awake[SPRD_FGU_SR_ARRAY_LEN];
	int sr_index_sleep;
	int sr_index_awake;
	int sr_index_number;
	int sr_avg_cur_ma;
	int sr_ocv_uv;
	int sr_ocv_cur_ua;
	enum sprd_fgu_sr_calib_mode sr_calib_mode;
	bool sr_resume_temp_update;
	bool sr_resume_need_calib;

	/* use third fuel */
	bool use_battery_temp;
	int third_fuel_nack_cnt;

	struct completion probe_init;
	bool probe_initialized;
	bool is_pre_full;

	/* capacity level */
  	int batt_cap_level_critical;

	/* new mode */
	int batt_ocv_uv;
	int batt_rm_uah;
	int batt_rm_soc;
	int batt_raw_rm_soc;
	int batt_ocv_temp;
	s64 batt_ocv_times;
	s64 cali_ocv_times;
	int batt_ibat_avg_ma;
	int batt_fcc;
	int batt_soc_uah;
	int batt_soc;
	int batt_uuuah;
	int batt_cc_uah;
	int init_cc_uah;
	int err_cc_uah;
	bool err_cc_adj_en;
	int err_fcc_uah;
	int err_cutoff_uah;
	int err_shutdown_uah;
	int chg_term_loop_mode;
	int chg_term_loop_trigger_cnt;
	int chg_term_loop_enable_mode;
	int dischg_term_loop_mode;
	int dischg_term_loop_trigger_cnt;
	int dischg_term_loop_first_in;
	int chg_term_loop_first_in;
	int dischg_term_loop_first2_in;
	int dischg_term_slow_down_ratio;
	int chg_term_voltage_uv;
	int chg_term_currnet_uA;
	int new_chg_term_voltage_uv;
	int new_chg_term_currnet_uA;
	int ibat_full_entry_ua;
	int pre_uusoc;
	int raw_uusoc;
	int raw_uuocv_uv;
	int pre_delta_soc;
	int cutoff_vbat_avg_mv;
	int capacity_hc;
	int calc_capacity_hc;
	int temp_default_fcc_mah;
	int chg_loss_soc;
	int boot_temp_thd;
	int shutdown_time_thd;

	int batt_ibat_avg_buff[SPRD_FGU_IBAT_AVG_BUFF_CNT];
	int chg_term_loop_ibat[SPRD_FGU_CHG_TERM_LOOP_NUM_SUM];
	int dischg_term_loop_vbat[SPRD_FGU_DISCHG_TERM_LOOP_NUM_SUM];
	int batt_ibat_avg_index;
	int chg_term_loop_ibat_avg_index;
	int dischg_term_loop_vbat_avg_index;
	int ibat_avg_work_cycle_ma;
	int vbat_avg_work_cycle_ma;

	int full_ocv_start;
	int full_ocv_cnt;
	s64 full_ocv_enter_times;
	int full_ocv_uv;
	int err_rt_cc_uah;
	int delta_cc_uah;

	bool charge_ocv_start;
	s64 charge_ocv_enter_times;

	int delta_soc_index;
	int delta_soc_sum;
	int delta_soc_count;

	struct sprd_cycles_agingfactor_lut *battery_cycles_agingfactor_lut;
	struct sprd_temp_fcc_lut *battery_temp_fcc_lut;
	struct sprd_temp_fcc_lut *battery_adjust_temp_fcc_lut;
	struct sprd_temp_hc_lut *battery_temp_hc_lut;
	struct sprd_temp_chg_loss_lut *battery_temp_chg_loss_lut;
	struct sprd_fcc_cycle_ratio_lut *battery_fcc_cycle_ratio_lut;
	struct sprd_temp_zp_voltage_lut *battery_temp_zp_vol_lut;
	struct sprd_temp_abs_zp_voltage_lut *battery_temp_abs_zp_vol_lut;
	struct sprd_temp_bat_voltage_lut *battery_temp_bat_vol_lut;
	struct sprd_cap_temp_ocv_lut *battery_cap_temp_ocv_lut;
	struct sprd_cap_temp_ocv_cycle_lut *battery_cap_temp_ocv_cycle_lut;
	struct sprd_cap_temp_ibat_lut *battery_cap_temp_ibat_lut;
	struct sprd_cap_temp_resist_lut *battery_cap_temp_resist_lut;
	struct sprd_cap_temp_resist_ratio_lut
		*battery_cap_temp_resist_ratio_lut[SPRD_BATTERY_OCV_TEMP_MAX];
	int battery_cap_temp_resist_ratio_lut_len;
	int battery_cap_temp_resist_ratio_table[SPRD_BATTERY_OCV_TEMP_MAX];
	int erp_config;
};

static int batt_cycle[20][2] = {
	{0,     100}, {101,   200}, {201,   300}, {301,   400},
	{401,   500}, {501,   600}, {601,   700}, {701,   800},
	{801,   900}, {901,  1000}, {1001, 1050}, {1051, 1100},
	{1101, 1150}, {1151, 1200}, {1201, 1250}, {1251, 1300},
	{1301, 1350}, {1351, 1400}, {1401, 1450}, {1451, 1500},
};

/*static int batt_soh[20] = {
	100, 99, 98, 97, 96, 95, 94, 93, 92, 91,
	90,  89, 88, 87, 86, 85, 84, 83, 82, 81,
};*/

static bool charge_mode;
static bool cali_or_auto_mode;
static int sprd_fgu_get_bat_para_table(struct sprd_fgu_data *data, int aging_bat_id);
static bool sprd_fgu_discharging_trend(struct sprd_fgu_data *data);
static void sprd_fgu_adjust_fcc(struct sprd_fgu_data *data, int learned_mah, int temp);
static int sprd_fgu_get_rbat(struct sprd_fgu_data *data, int batt_tbat, int soc, int cycles);
static void sprd_fgu_get_vbat_info(struct sprd_fgu_data *data);
static int sprd_fgu_get_zp_voltage_by_temp(struct sprd_fgu_data *data, int batt_tbat);
static int sprd_fgu_get_abs_zp_voltage_by_temp(struct sprd_fgu_data *data, int batt_tbat);
static int sprd_fgu_get_temp(struct sprd_fgu_data *data, int *temp);
static int sprd_fgu_get_soc_by_ocv(struct sprd_fgu_data *data, int batt_ocv, int batt_tbat);
static int sprd_fgu_get_ocv_by_soc(struct sprd_fgu_data *data, int batt_tbat, int soc, int cycle);
static int interpolate_ocv2cap(struct sprd_fgu_data *data, int temp, int ocv);
extern int sprd_battery_parse_cmdline_match_by_split(char *match_str, char* split, char *result, int size);

static int linear_interpolate(int y1, int x1, int y2, int x2, int x)
{
	if ((y1 == y2) || (x == x1))
		return y1;
	if ((x1 == x2) || (x == x2))
		return y2;

	return y1 + (y2 - y1) * (x - x1) / (x2 - x1);
}

static bool is_between(int val, int low, int high)
{
	if (val > high || val < low)
		return false;

	return true;
}

static inline int sprd_fgu_uah2current(int uah, int times)
{
	/* To avoid data overflow, divide uah by 100 firstly */
	return DIV_ROUND_CLOSEST(uah * 36, times * 10);
}

static inline void sprd_fgu_battery_soc_limit(struct sprd_fgu_data *data,
					      int *cap, bool from_smooth)
{
	int zero_point_mv, vbat_avg_mv, abs_zero_point_uv, abs_zero_point_mv;
	int delta_vol_mv, shutdown_safe_vol_mv = 100, org_cap = *cap;

	if (from_smooth)
		vbat_avg_mv = data->cutoff_vbat_avg_mv;
	else
		vbat_avg_mv = data->vbat_info.vbat_avg_mv;

	zero_point_mv = data->vbat_info.zp_vol_uv / 1000;
	if (org_cap < 5 && vbat_avg_mv > zero_point_mv)
		*cap = 5;

	abs_zero_point_uv = sprd_fgu_get_abs_zp_voltage_by_temp(data, data->bat_temp);
	abs_zero_point_uv -= data->dischg_dynamic_zp_uv;
	if (abs_zero_point_uv > 0) {
		abs_zero_point_mv = abs_zero_point_uv / 1000;
		if (org_cap < 5 && vbat_avg_mv > abs_zero_point_mv) {
			dev_info(data->dev, "abs_zero_point_uv = %d, vbat_avg_mv = %d, org_cap = %d\n",
				abs_zero_point_uv, vbat_avg_mv, org_cap);
			*cap = 5;
		}
	}

	if (!from_smooth)
		return;

	delta_vol_mv = zero_point_mv - (data->shutdown_voltage_mv + shutdown_safe_vol_mv);
	delta_vol_mv = clamp(delta_vol_mv, 0, 100);

	if (org_cap <= 40 && delta_vol_mv > 30 && data->smooth_soc >= org_cap + 10 &&
	    vbat_avg_mv >= (zero_point_mv - delta_vol_mv)) {
		*cap = org_cap + 10;
		*cap = (*cap * 40 / 50);
		*cap = clamp(*cap, 10, 1000);
	} else if (org_cap <= 5 && data->smooth_soc >= 5 &&
		   vbat_avg_mv >= (zero_point_mv - delta_vol_mv)) {
		*cap = 5;
	} else if (org_cap <= 5 && data->smooth_soc > 1 && data->smooth_soc <= 5 &&
		   org_cap < data->smooth_soc && *cap < data->smooth_soc &&
		   vbat_avg_mv >= (zero_point_mv - 30)) {
		*cap = data->smooth_soc;
	}
}

static int sprd_fgu_vol2temp(struct power_supply_vol_temp_table *table,
			     int table_len, int vol_uv)
{
	int i, temp;

	for (i = 0; i < table_len; i++) {
		if (vol_uv > table[i].vol)
			break;
	}

	if (i > 0 && i < table_len) {
		temp = linear_interpolate(table[i].temp, table[i].vol,
					  table[i - 1].temp, table[i - 1].vol, vol_uv);
	} else if (i == 0) {
		temp = table[0].temp;
	} else {
		temp = table[table_len - 1].temp;
	}

	return temp - 1000;
}

static void sprd_fgu_cap_remap_init_boundary(struct sprd_fgu_data *data, int index)
{

	if (index == 0) {
		data->cap_remap_table[index].lb = (data->cap_remap_table[index].lcap) * 1000;
		data->cap_remap_total_cnt = data->cap_remap_table[index].lcap;
	} else {
		data->cap_remap_table[index].lb = data->cap_remap_table[index - 1].hb +
			(data->cap_remap_table[index].lcap -
			 data->cap_remap_table[index - 1].hcap) * 1000;
		data->cap_remap_total_cnt += (data->cap_remap_table[index].lcap -
					      data->cap_remap_table[index - 1].hcap);
	}

	data->cap_remap_table[index].hb = data->cap_remap_table[index].lb +
		(data->cap_remap_table[index].hcap - data->cap_remap_table[index].lcap) *
		data->cap_remap_table[index].cnt * 1000;

	data->cap_remap_total_cnt +=
		(data->cap_remap_table[index].hcap - data->cap_remap_table[index].lcap) *
		data->cap_remap_table[index].cnt;

	dev_dbg(data->dev, "%s, cap_remap_table[%d].lb =%d,cap_remap_table[%d].hb = %d\n",
		 __func__, index, data->cap_remap_table[index].lb, index,
		 data->cap_remap_table[index].hb);
}

static int sprd_fgu_init_cap_remap_table(struct sprd_fgu_data *data)
{
	struct device_node *np = data->dev->of_node;
	const __be32 *list;
	int i, size;

	list = of_get_property(np, "fgu-cap-remap-table", &size);
	if (!list || !size) {
		dev_err(data->dev, "%s  get fgu-cap-remap-table fail\n", __func__);
		return 0;
	}
	data->cap_remap_table_len = (u32)size / (3 * sizeof(__be32));
	data->cap_remap_table = devm_kzalloc(data->dev, sizeof(struct sprd_fgu_cap_remap_table) *
				(data->cap_remap_table_len + 1), GFP_KERNEL);
	if (!data->cap_remap_table)
		return -ENOMEM;

	for (i = 0; i < data->cap_remap_table_len; i++) {
		data->cap_remap_table[i].lcap = be32_to_cpu(*list++);
		data->cap_remap_table[i].hcap = be32_to_cpu(*list++);
		data->cap_remap_table[i].cnt = be32_to_cpu(*list++);

		sprd_fgu_cap_remap_init_boundary(data, i);

		dev_dbg(data->dev, "cap_remap_table[%d].lcap= %d,cap_remap_table[%d].hcap = %d, cap_remap_table[%d].cnt= %d\n",
			i, data->cap_remap_table[i].lcap, i, data->cap_remap_table[i].hcap,
			i, data->cap_remap_table[i].cnt);
	}

	if (data->cap_remap_table[data->cap_remap_table_len - 1].hcap != 1000)
		data->cap_remap_total_cnt +=
			(1000 - data->cap_remap_table[data->cap_remap_table_len - 1].hcap);

	dev_dbg(data->dev, "cap_remap_total_cnt =%d, cap_remap_table_len = %d\n",
		data->cap_remap_total_cnt, data->cap_remap_table_len);

	return 0;
}

static int sprd_fgu_parse_battery_cycles_fcc_table(struct sprd_fgu_data *data)
{
	struct device_node *np = data->dev->of_node;
	int len, battery_id;
	char *cycle_str;

	data->battery_cycles_fcc_table =
		devm_kzalloc(data->dev, sizeof(struct sprd_battery_cycles_fcc_table), GFP_KERNEL);

	battery_id = sprd_battery_parse_battery_id(data->battery);
	cycle_str = kasprintf(GFP_KERNEL, "bat-%d-aging-cycles", battery_id);
	len = of_property_count_u32_elems(np, cycle_str);

	if (len < 0 && len != -EINVAL) {
		data->battery_cycles_fcc_table->cols = len;
		return len;
	} else if (len > SPRD_BATTERY_CYCLES_FCC_COLS_MAX) {
		dev_err(data->dev, "too many cycles values\n");
		data->battery_cycles_fcc_table->cols = -EINVAL;
		return -EINVAL;
	} else if (len > 0) {
		data->battery_cycles_fcc_table->cols = len;
		of_property_read_u32_array(np, cycle_str,
					   data->battery_cycles_fcc_table->cycles, len);
	}

	return 0;
}

/*
 * sprd_fgu_capacity_remap - remap fuel_cap
 * Return the remapped cap
 */
static int sprd_fgu_capacity_remap(struct sprd_fgu_data *data, int fuel_cap)
{
	int i, temp, cap = 0;

	if (!data->fgu_info->capacity_remap_enable)
		return fuel_cap;

	if (!data->cap_remap_table) {
		if (fuel_cap >= 150)
			fuel_cap = DIV_ROUND_CLOSEST((fuel_cap - SPRD_FGU_RESERVE_SOC) * 1000,
						     1000 - SPRD_FGU_RESERVE_SOC);
		if (fuel_cap > 650)
			fuel_cap = fuel_cap - SPRD_FGU_HC_SOC;
		else if (fuel_cap > 150)
			fuel_cap = fuel_cap - SPRD_FGU_HC_SOC / 2;
		return fuel_cap;
	}

	if (fuel_cap < 0) {
		fuel_cap = 0;
		return 0;
	} else if (fuel_cap >  SPRD_FGU_FCC_PERCENT) {
		fuel_cap  = SPRD_FGU_FCC_PERCENT;
		return fuel_cap;
	}

	temp = fuel_cap * data->cap_remap_total_cnt;

	for (i = 0; i < data->cap_remap_table_len; i++) {
		if (temp <= data->cap_remap_table[i].lb) {
			if (i == 0)
				cap = DIV_ROUND_CLOSEST(temp, 1000);
			else
				cap = DIV_ROUND_CLOSEST((temp -
					data->cap_remap_table[i - 1].hb), 1000) +
					data->cap_remap_table[i - 1].hcap;
			break;
		} else if (temp <= data->cap_remap_table[i].hb) {
			cap = DIV_ROUND_CLOSEST((temp - data->cap_remap_table[i].lb),
						data->cap_remap_table[i].cnt * 1000)
				+ data->cap_remap_table[i].lcap;
			break;
		}

		if (i == data->cap_remap_table_len - 1 && temp > data->cap_remap_table[i].hb)
			cap = DIV_ROUND_CLOSEST((temp - data->cap_remap_table[i].hb), 1000)
				+ data->cap_remap_table[i].hcap;

	}

	return cap;
}

static int sprd_fgu_get_boot_mode(struct sprd_fgu_data *data)
{
	struct device_node *cmdline_node;
	const char *cmd_line;
	int ret = 0;

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmd_line);
	if (ret)
		return ret;

	if (strstr(cmd_line, "boot.mode=charger"))
		charge_mode =  true;
	else if (strstr(cmd_line, "boot.mode=cali") ||
		 strstr(cmd_line, "boot.mode=autotest"))
		cali_or_auto_mode = true;

	dev_info(data->dev, "charge_mode = %d, cali_or_auto_mode = %d\n",
		 charge_mode, cali_or_auto_mode);

	return ret;
}

static int sprd_fgu_parse_cmdline_match(struct sprd_fgu_data *data, char *match_str,
					char *result, int size)
{
	struct device_node *cmdline_node = NULL;
	const char *cmdline;
	char *match, *match_end;
	int len, match_str_len, ret;

	if (!result || !match_str)
		return -EINVAL;

	memset(result, '\0', size);
	match_str_len = strlen(match_str);

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmdline);
	if (ret) {
		dev_warn(data->dev, "%s failed to read bootargs\n", __func__);
		return -EINVAL;
	}

	match = strstr(cmdline, match_str);
	if (!match) {
		dev_warn(data->dev, "Match: %s fail in cmdline\n", match_str);
		return -EINVAL;
	}

	match_end = strstr((match + match_str_len), " ");
	if (!match_end) {
		dev_warn(data->dev, "Match end of : %s fail in cmdline\n", match_str);
		return -EINVAL;
	}

	len = match_end - (match + match_str_len);
	if (len < 0) {
		dev_warn(data->dev, "Match cmdline :%s fail, len = %d\n", match_str, len);
		return -EINVAL;
	}

	memcpy(result, (match + match_str_len), len);

	return 0;
}

static void sprd_fgu_parse_shutdown_rtc_time(struct sprd_fgu_data *data)
{
	char result[32] = {};
	int ret;
	char *str;

	str = "charge.shutdown_rtc_time=";
	data->shutdown_rtc_time = -1;
	ret = sprd_fgu_parse_cmdline_match(data, str, result, sizeof(result));
	if (!ret) {
		ret = kstrtoll(result, 10, &data->shutdown_rtc_time);
		if (ret) {
			data->shutdown_rtc_time = -1;
			dev_err(data->dev, "Covert shutdown_rtc_time fail, ret = %d, result = %s",
				ret, result);
		}
	}
}

static void sprd_fgu_parse_bootcause(struct sprd_fgu_data *data)
{
	char result[32] = {};
	int ret;
	char *str;

	str = "bootcause";
	ret = sprd_fgu_parse_cmdline_match(data, str, result, sizeof(result));
	if (!ret) {
		if (!strstr(result, "Reboot"))
			data->is_reboot = false;
		else
			data->is_reboot = true;
	}
}

static int sprd_fgu_get_aging_bat_id(struct sprd_fgu_data *data)
{
	struct charger_manager *cm = NULL;
	static struct power_supply *batt_psy = NULL;
	int aging_bat_id = 0, i, cols, temp_charge_cycle;

	if (!data->support_bat_aging)
		return aging_bat_id;

	cols = data->battery_cycles_fcc_table->cols;
	if (cols <= 0)
		return aging_bat_id;

	if (!batt_psy) {
		dev_err(data->dev, "battery psy null\n");
		batt_psy = power_supply_get_by_name("battery");
	}
	if (!batt_psy) {
		temp_charge_cycle = -EINVAL;
		dev_err(data->dev, "battery psy null\n");
	} else {
		cm = power_supply_get_drvdata(batt_psy);
		if (!cm) {
			temp_charge_cycle = -EINVAL;
			dev_err(data->dev, "get charger manager failed\n");
		} else {
			temp_charge_cycle = cm->fake_charge_cycle;
			dev_info(data->dev, "get fake cycle:%d from charger manager\n", temp_charge_cycle);
		}
	}
	if (temp_charge_cycle < 0)
		temp_charge_cycle = data->charge_cycle;

	if (temp_charge_cycle <= data->battery_cycles_fcc_table->cycles[0] * 1000)
		return 0;

	if (temp_charge_cycle >= data->battery_cycles_fcc_table->cycles[cols - 1] * 1000)
		return cols - 1;

	for (i = 1; i < cols; i++) {
		if (temp_charge_cycle < data->battery_cycles_fcc_table->cycles[i] * 1000) {
			aging_bat_id = i - 1;
			break;
		}
    }

	dev_info(data->dev, "aging_bat_id = %d\n", aging_bat_id);

	return aging_bat_id;
}

static void sprd_fgu_parse_chip_cycle_soh(struct sprd_fgu_data *data)
{
	int i;
	struct secret_device *secdev = NULL;

	data->last_chip_soh   = -EINVAL;
	data->last_chip_cycle = -EINVAL;
	if (CHIP_ONLINE == ll_check_secret_chip_online()) {
		secdev = get_secret_by_name(MASTER_SECRET);
		if (!secdev) {
			dev_err(data->dev, "search master secret device failed\n");
			return;
		}
		data->w1sec_info = dev_get_drvdata(&secdev->dev);
		if (!data->w1sec_info) {
			dev_err(data->dev, "search w1 secret device failed\n");
			return;
		}
		data->last_chip_cycle = data->w1sec_info->cycle_count_curr;
		for (i = 0; i < ARRAY_SIZE(batt_cycle); i++) {
			if (data->last_chip_cycle >= batt_cycle[i][0] && data->last_chip_cycle <= batt_cycle[i][1])
				break;
		}
		data->last_chip_soh = 100 - i;
		dev_info(data->dev, "initial chip cycle:%d, soh: %d\n", data->last_chip_cycle, data->last_chip_soh);
	} else
		dev_info(data->dev, "battery chip offline\n");
}

static void sprd_fgu_parse_charge_cycle(struct sprd_fgu_data *data)
{
	char result[32] = {};
	int i, ret;
	char *str;

	str = "charge.charge_cycle=";
	data->charge_cycle = -1;
	data->last_batt_soh = 100;
	data->last_batt_cycle = 0;
	sprd_fgu_parse_chip_cycle_soh(data);
	ret = sprd_fgu_parse_cmdline_match(data, str, result, sizeof(result));
	if (!ret) {
		ret = kstrtoint(result, 10, &data->charge_cycle);
		if (ret) {
			data->charge_cycle = -1;
			dev_err(data->dev, "Covert charge_cycle fail, ret = %d, result = %s\n",
				ret, result);
		} else if (data->charge_cycle >= 0) {
			data->last_batt_cycle = data->charge_cycle / 1000;
			for (i = 0; i < ARRAY_SIZE(batt_cycle); i++) {
				if (data->last_batt_cycle >= batt_cycle[i][0] && data->last_batt_cycle <= batt_cycle[i][1])
					break;
			}
			data->bat_soh = 100 - i;
			data->last_batt_soh = data->bat_soh;
			dev_info(data->dev, "fuel gauge soh:%d, cycle:%d\n", data->last_batt_soh, data->last_batt_cycle);
		}
	}

	data->dynamic_update_bat_para_flag = false;
}

static void sprd_fgu_parse_learned_mah(struct sprd_fgu_data *data)
{
	char result[32] = {};
	int ret, learned_uah;
	char *str;

	str = "charge.total_mah=";
	learned_uah = -1;
	ret = sprd_fgu_parse_cmdline_match(data, str, result, sizeof(result));
	if (!ret) {
		ret = kstrtoint(result, 10, &learned_uah);
		if (ret) {
			learned_uah = -1;
			dev_err(data->dev, "Covert learned_mah fail, ret = %d, result = %s\n",
				ret, result);
		}
	}

	if (learned_uah <= 100000)
		data->track.learned_mah = -1;
	else
		data->track.learned_mah = learned_uah / 1000;
}

static void sprd_fgu_parse_cmdline(struct sprd_fgu_data *data)
{
	/* parse shutdown rtc time */
	sprd_fgu_parse_shutdown_rtc_time(data);

	/* parse charge cycle */
	sprd_fgu_parse_charge_cycle(data);

	/* parse reboot cause */
	sprd_fgu_parse_bootcause(data);

	/* parse learned total mah */
	sprd_fgu_parse_learned_mah(data);

	dev_info(data->dev, "shutdown_rtc_time = %lld, charge_cycle = %d, learned_mah = %d, is_reboot = %d\n",
		 data->shutdown_rtc_time, data->charge_cycle,
		 data->track.learned_mah, data->is_reboot);
}

static int sprd_fgu_get_rtc_time(struct sprd_fgu_data *data, s64 *time)
{
	struct rtc_time tm;
	struct rtc_device *rtc;
	int ret;

	rtc = alarmtimer_get_rtcdev();
	if (!rtc) {
		dev_err(data->dev, "NO RTC dev!!!\n");
		return -EINVAL;
	}

	ret = rtc_read_time(rtc, &tm);
	if (ret) {
		dev_err(data->dev, "failed to read rtc time, ret = %d\n", ret);
		return ret;
	}

	*time = rtc_tm_to_time64(&tm);

	return 0;
}

static int sprd_fgu_temp2full_percent(struct sprd_fgu_data *data, int *cap)
{
	int i;

	if (!data->temp_fullcap_table || data->fullcap_table_len == 0)
		return SPRD_FGU_FULL_PERCENT;

	if (*cap < 700)
		return SPRD_FGU_FULL_PERCENT;

	for (i = data->fullcap_table_len - 1; i >= 0; i--) {
		if (data->bat_temp < data->temp_fullcap_table[i].temp)
			return data->temp_fullcap_table[i].advance_fullcap;
	}

	return SPRD_FGU_FULL_PERCENT;
}

static void sprd_fgu_adjust_smooth_rate(struct sprd_fgu_data *data,
					int *cap, int org_cap, int *diff_ratio)
{
	int gap, delta_soc, comp_soc;
	int delta_soc_thd = 20, delta_soc_limit = 40;
	int pre_uusoc_min = 500, pre_uusoc_thd = 150, pre_uusoc_temp;

	if (org_cap < 150 || data->dischg_term_loop_mode == 1)
		return;

	if (data->bat_temp >= 200)
		pre_uusoc_thd = 40;
	else if(data->bat_temp >= 50)
		pre_uusoc_thd = 50;
	else if(data->bat_temp >= 0)
		pre_uusoc_thd = 100;
	else if(data->bat_temp >= -100)
		pre_uusoc_thd = 180;
	else
		pre_uusoc_thd = 180;

	gap = data->smooth_soc - org_cap;
	if (data->pre_uusoc >= pre_uusoc_thd) {
		pre_uusoc_temp = 1000 - data->pre_uusoc + pre_uusoc_thd;
		pre_uusoc_temp = clamp(pre_uusoc_temp, pre_uusoc_min, 1000);
		*diff_ratio = 1000 * 1000 / pre_uusoc_temp;
	}

	if (org_cap >= 350) {
		delta_soc_thd = 10;
		delta_soc_limit = 50;
	}

	if (data->bat_temp > 150)
		comp_soc = 0;
	else if (data->bat_temp > 0)
		comp_soc = 10;
	else
		comp_soc = 20;

	delta_soc_limit += comp_soc;
	delta_soc = gap - delta_soc_thd - (org_cap - *cap);
	delta_soc = clamp(delta_soc, 0, delta_soc_limit);
	*cap -= delta_soc;
}

static void sprd_fgu_smooth_to_soc(struct sprd_fgu_data *data, int *cap, int normal_cap_diff)
{
	int smooth_cap_diff, adjust_step = 10, cap_info, org_smooth_soc = data->smooth_soc;
	int diff_ratio = 1000, gap, org_cap = *cap;

	if (normal_cap_diff == 0) {
		if (data->support_debug_log)
			dev_info(data->dev, "sprd normal_cap_diff = 0, adjust_step = NA, temp_cap = %d, smooth_cap_diff = 0, *cap = %d, smooth_soc = %d, smooth_soc_decimal = %d\n",
				 data->temp_cap, *cap, data->smooth_soc, data->smooth_soc_decimal);
		*cap = data->smooth_soc;
		if (data->smooth_soc < 5 && data->smooth_soc >= 1)
			*cap = 5;
		dev_info(data->dev, "%s smooth_soc = %d, normal_cap_diff = %d\n",
				 __func__, data->smooth_soc, normal_cap_diff);
		return;
	} else if (normal_cap_diff > 0) {// chg
		data->cap_remap_full_percent = sprd_fgu_temp2full_percent(data, cap);
		*cap = *cap * 100 / data->cap_remap_full_percent;

		if (*cap > 1000)
			*cap  = 1000;

		if (*cap >= data->smooth_soc) {
			if (*cap == 1000) {
				data->smooth_soc = *cap;
				if (data->support_debug_log)
					dev_info(data->dev, "sprd normal_cap_diff = %d, adjust_step = NA, temp_cap = %d, smooth_cap_diff = NA, *cap = %d, smooth_soc = %d, smooth_soc_decimal = %d\n",
						 normal_cap_diff, data->temp_cap, *cap,
						 data->smooth_soc, data->smooth_soc_decimal);
				return;
			} else if (*cap > 970) {
				adjust_step = 1000 - *cap;
			} else if (*cap > 920) {
				adjust_step = 30;
			} else if (*cap > 650) {
				adjust_step = 950 - *cap;
			} else if (*cap > 350) {
				adjust_step = 300;
			} else {
				adjust_step = 650 - *cap;
			}

			if (adjust_step < 10)
				adjust_step = 10;
			smooth_cap_diff =
				DIV_ROUND_CLOSEST(normal_cap_diff *
						  (adjust_step + *cap - data->smooth_soc) * 100,
						  adjust_step);
		} else {
			gap = data->smooth_soc - *cap;
			if (data->smooth_soc == 1000) {
				if (data->support_debug_log)
					dev_info(data->dev, "sprd normal_cap_diff = %d, adjust_step = NA, temp_cap = %d, smooth_cap_diff = NA, *cap = %d, smooth_soc = %d, smooth_soc_decimal = %d\n",
						 normal_cap_diff, data->temp_cap, *cap,
						 data->smooth_soc, data->smooth_soc_decimal);
				*cap = data->smooth_soc;
				return;
			} else if (data->smooth_soc  > 970) {
				adjust_step = 1000 - data->smooth_soc + gap;
			} else if (data->smooth_soc   > 920) {
				adjust_step = 30 + gap;
			} else if (data->smooth_soc   > 650) {
				adjust_step = 950 - data->smooth_soc + gap;
			} else if (data->smooth_soc   > 350) {
				adjust_step = 300 + gap;
			} else {
				adjust_step = 650 - data->smooth_soc + gap;
			}

			if (adjust_step < 10)
				adjust_step = 10;
			smooth_cap_diff =
				DIV_ROUND_CLOSEST(normal_cap_diff * adjust_step * 100,
						  (adjust_step + gap));
		}

		if (smooth_cap_diff < 0)
			smooth_cap_diff = 0;
	} else { // dischg
		if (data->bat_temp >= SPRD_FGU_CAP_REMAP_TEMP_REGION) {
			cap_info = *cap;
			*cap = sprd_fgu_capacity_remap(data, cap_info);
			if (data->support_debug_log)
				dev_info(data->dev, "sprd cap_info = %d, *cap = %d\n", cap_info,
					 *cap);
		}
		sprd_fgu_battery_soc_limit(data, cap, true);

		if (*cap <= data->smooth_soc) {
			sprd_fgu_adjust_smooth_rate(data, cap, org_cap, &diff_ratio);
			if (*cap == 0) {
				data->smooth_soc = *cap;
				if (data->support_debug_log)
					dev_info(data->dev, "sprd normal_cap_diff = %d, adjust_step = NA, temp_cap = %d, smooth_cap_diff = NA, *cap = %d, smooth_soc = %d, smooth_soc_decimal = %d\n",
						 normal_cap_diff, data->temp_cap, *cap,
						 data->smooth_soc, data->smooth_soc_decimal);
				return;
			} else if (*cap < 30) {
				adjust_step = *cap;
			} else if (*cap < 80) {
				adjust_step = 30;
			} else if (*cap < 350) {
				adjust_step = *cap - 50;
			} else if (*cap < 650) {
				adjust_step = 300;
			} else {
				adjust_step = *cap - 350;
			}

			if (adjust_step < 20)
				adjust_step = 20;
			smooth_cap_diff =
				DIV_ROUND_CLOSEST(normal_cap_diff *
						  (adjust_step + data->smooth_soc - *cap) * 100,
						  adjust_step);
			smooth_cap_diff = smooth_cap_diff * diff_ratio / 1000;
		} else {
			gap = *cap - data->smooth_soc;
			if (data->smooth_soc == 0) {
				if (data->support_debug_log)
					dev_info(data->dev, "sprd normal_cap_diff = %d, adjust_step = NA, temp_cap = %d, smooth_cap_diff = NA, *cap = %d, smooth_soc = %d, smooth_soc_decimal = %d\n",
						 normal_cap_diff, data->temp_cap, *cap,
						 data->smooth_soc, data->smooth_soc_decimal);
				*cap = data->smooth_soc;
				return;
			} else if (data->smooth_soc < 30) {
				adjust_step = data->smooth_soc;
			} else if (data->smooth_soc < 80) {
				adjust_step = 30;
			} else if (data->smooth_soc < 350) {
				adjust_step = data->smooth_soc - 50;
			} else if (data->smooth_soc < 650) {
				adjust_step = 300;
			} else {
				adjust_step = data->smooth_soc - 350;
			}

			if (adjust_step < 10)
				adjust_step = 10;
			smooth_cap_diff =
				DIV_ROUND_CLOSEST(normal_cap_diff * adjust_step * 100,
						  (adjust_step + gap));
		}

		if (smooth_cap_diff > 0)
			smooth_cap_diff = 0;
	}

	data->smooth_soc += smooth_cap_diff / 1000;
	data->smooth_soc_decimal +=  smooth_cap_diff % 1000;
	if (data->smooth_soc_decimal >= 1000) {
		data->smooth_soc_decimal -= 1000;
		data->smooth_soc += 1;
	} else if (data->smooth_soc_decimal <= -1000) {
		data->smooth_soc_decimal += 1000;
		data->smooth_soc -= 1;
	}

	data->smooth_soc = clamp(data->smooth_soc, 0, SPRD_FGU_FCC_PERCENT);

	if (normal_cap_diff > 0 && *cap < org_smooth_soc && data->smooth_soc < *cap)
		data->smooth_soc = *cap;
	else if (normal_cap_diff > 0 && *cap > org_smooth_soc && data->smooth_soc > *cap)
		data->smooth_soc = *cap;
	else if (normal_cap_diff < 0 && *cap < org_smooth_soc && data->smooth_soc < *cap)
		data->smooth_soc = *cap;
	else if (normal_cap_diff < 0 && *cap > org_smooth_soc && data->smooth_soc > *cap)
		data->smooth_soc = *cap;
	else if (normal_cap_diff < 0 && *cap > data->smooth_soc && data->smooth_soc == 0)
		data->smooth_soc = 1;

	dev_info(data->dev, "sprd normal_cap_diff = %d, adjust_step = %d, temp_cap = %d, smooth_cap_diff = %d, *cap = %d, smooth_soc = %d, smooth_soc_decimal = %d, org_cap = %d, diff_ratio = %d\n",
		 normal_cap_diff, adjust_step, data->temp_cap, smooth_cap_diff, *cap,
		 data->smooth_soc, data->smooth_soc_decimal, org_cap, diff_ratio);

	*cap = data->smooth_soc;

	if (data->smooth_soc < 5 && data->smooth_soc >= 1)
		*cap = 5;
}

static int sprd_fgu_get_discharge_ocv(struct sprd_fgu_data *data, int vbat_avg_uv, int batt_tbat,
				      int ibat_avg_ma, int cycles)
{
	int ocv_uv, soc, rbat_moh, pre_rbat_moh = 0, uuocv_uv, pre_ocv_uv = 0, pre_uuocv_uv, i = 0;

	for (i = 0; i <= 100; i++) {
		soc = i * 10;
		ocv_uv = sprd_fgu_get_ocv_by_soc(data, batt_tbat, soc, false);
		rbat_moh = sprd_fgu_get_rbat(data, batt_tbat, soc, cycles) +
			data->com_pcb_resistance;
		/* chgrging ibat_avg_ma > 0; discharging ibat_avg_ma < 0 */
		uuocv_uv = vbat_avg_uv - ibat_avg_ma * rbat_moh;
		if (uuocv_uv == ocv_uv)
			break;

		if (soc == 0) {
			pre_ocv_uv = ocv_uv;
			pre_rbat_moh = rbat_moh;
		}

		if (uuocv_uv - ocv_uv < 0) {
			if (soc == 0) {
				uuocv_uv = ocv_uv;
				break;
			}
			rbat_moh = linear_interpolate(pre_rbat_moh, pre_uuocv_uv - pre_ocv_uv,
						      rbat_moh, uuocv_uv - ocv_uv, 0);
			uuocv_uv = vbat_avg_uv - ibat_avg_ma * rbat_moh;
			break;
		}
		pre_ocv_uv = ocv_uv;
		pre_uuocv_uv = uuocv_uv;
		pre_rbat_moh = rbat_moh;
	}

	dev_info(data->dev, "uuocv = %d, soc = %d, rbat = %d, pre_rbat_moh = %d, ibat_avg_ma = %d, pre_ocv = %d, ocv = %d, vbat_avg_uv = %d, i = %d\n",
		 uuocv_uv, soc, rbat_moh, pre_rbat_moh, ibat_avg_ma,
		 pre_ocv_uv, ocv_uv, vbat_avg_uv, i);

	if (i > 100) {
		uuocv_uv = ocv_uv;
		dev_dbg(data->dev, "UUOCV > VBAT_FULL\n");
	}

	return uuocv_uv;
}

static int sprd_fgu_get_charge_ocv(struct sprd_fgu_data *data, int vbat_avg_uv, int batt_tbat,
				   int ibat_avg_ma, int cycles, bool is_chg_rbat_ratio)
{
	int ocv_uv, soc, rbat_moh, pre_rbat_moh = 0, uuocv_uv, pre_ocv_uv = 0, pre_uuocv_uv, i = 0;
	int chg_rbat_ratio = 100;
	int chg_cur1 = 300, ratio1 = 100, chg_cur2 = 3000, ratio2 = 70;

	if (is_chg_rbat_ratio && ibat_avg_ma > 10) {
		if (ibat_avg_ma < chg_cur1)
			chg_rbat_ratio = ratio1;
		else if (ibat_avg_ma > chg_cur2)
			chg_rbat_ratio = ratio2;
		else
			chg_rbat_ratio = linear_interpolate(ratio1, chg_cur1,
							    ratio2, chg_cur2, ibat_avg_ma);
	}

	for (i = 100; i >= 0; i--) {
		soc = i * 10;
		ocv_uv = sprd_fgu_get_ocv_by_soc(data, batt_tbat, soc, false);
		rbat_moh = sprd_fgu_get_rbat(data, batt_tbat, soc, cycles);
		rbat_moh = rbat_moh * chg_rbat_ratio / 100 + data->com_pcb_resistance;
		/* chgrging ibat_avg_ma > 0; discharging ibat_avg_ma < 0 */
		uuocv_uv = vbat_avg_uv - ibat_avg_ma * rbat_moh;
		if (uuocv_uv == ocv_uv)
			break;

		if (soc == 1000) {
			pre_ocv_uv = ocv_uv;
			pre_rbat_moh = rbat_moh;
		}

		if (uuocv_uv - ocv_uv > 0) {
			if (soc == 1000) {
				uuocv_uv = ocv_uv;
				break;
			}
			rbat_moh = linear_interpolate(pre_rbat_moh, pre_uuocv_uv - pre_ocv_uv,
							rbat_moh, uuocv_uv - ocv_uv, 0);
			uuocv_uv = vbat_avg_uv - ibat_avg_ma * rbat_moh;
			break;
		}
		pre_ocv_uv = ocv_uv;
		pre_uuocv_uv = uuocv_uv;
		pre_rbat_moh = rbat_moh;
	}

	dev_info(data->dev, "vbat_uuocv = %d, vbat_soc = %d, vbat_rbat = %d, ibat_avg_ma = %d, vbat_pre_ocv = %d, vbat_ocv = %d, chg_rbat_ratio = %d, vbat_avg_uv = %d, i = %d\n",
	   uuocv_uv, soc, rbat_moh, ibat_avg_ma, pre_ocv_uv,
	   ocv_uv, chg_rbat_ratio, vbat_avg_uv, i);

	if (i < 0) {
		uuocv_uv = ocv_uv;
		dev_dbg(data->dev, "UUOCV > VBAT_FULL\n");
	}

	return uuocv_uv;
}

/* @val: value of battery ocv in mV*/
static int sprd_fgu_get_vbat_ocv(struct sprd_fgu_data *data, int *val)
{
	int vol_avg_mv, cur_avg_ma, ret, ocv_uv;
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	ret = fgu_info->ops->get_vbat_avg(fgu_info, &vol_avg_mv);
	if (ret)
		return ret;

	ret = fgu_info->ops->get_current_avg(fgu_info, &cur_avg_ma);
	if (ret)
		return ret;

	if (cur_avg_ma < 10)
		ocv_uv = sprd_fgu_get_discharge_ocv(data, vol_avg_mv * 1000, data->bat_temp,
						    cur_avg_ma, data->charge_cycle);
	else
		ocv_uv = sprd_fgu_get_charge_ocv(data, vol_avg_mv * 1000, data->bat_temp,
						 cur_avg_ma, data->charge_cycle, true);

	dev_info(data->dev, "%s, ocv_uv = %d, cur_avg_ma = %d, vol_avg_mv = %d\n",
		 __func__, ocv_uv, cur_avg_ma, vol_avg_mv);

	*val = ocv_uv / 1000;

	return 0;
}

static void sprd_fgu_dump_battery_info(struct sprd_fgu_data *data, char *str)
{
	int i, j, cols, rows;

	dev_info(data->dev, "%s, track.end_vol = %d\n"
		 "track.end_cur = %d, total_mah = %d, max_volt_uv = %d, internal_resist = %d, min_volt_uv = %d\n",
		 str, data->track.end_vol, data->track.end_cur,
		 data->total_mah, data->max_volt_uv,
		 data->internal_resist, data->min_volt_uv);

	dev_info(data->dev, "%s, fullbatt_uV = %d\n"
		 "fullbatt_uA = %d, batt_ovp_threshold = %d, design_mah = %d, shutdown_voltage_mv = %d, charge_full_mah = %d\n",
		 str, data->fullbatt_uV, data->fullbatt_uA,
		 data->batt_ovp_threshold, data->design_mah,
		 data->shutdown_voltage_mv, data->charge_full_mah);

	/* temp_fcc_lut */
	if (data->battery_temp_fcc_lut &&
	    data->battery_temp_fcc_lut->cols > 0) {
		cols = data->battery_temp_fcc_lut->cols;
		dev_info(data->dev, "%s, temp_fcc_lut============= start\n", __func__);
		for (i = 0; i < cols; i++) {
			dev_info(data->dev, "%s, temp_fcc_lut->fcc[%d] = %d\n",
				 __func__, i, data->battery_temp_fcc_lut->fcc[i]);
			dev_info(data->dev, "%s, temp_fcc_lut->temp[%d] = %d\n",
				 __func__, i, data->battery_temp_fcc_lut->temp[i]);
		}
		dev_info(data->dev, "%s, temp_fcc_lut============= end\n", __func__);
	}

	/* adjust temp_fcc_lut */
	if (data->battery_adjust_temp_fcc_lut &&
	    data->battery_adjust_temp_fcc_lut->cols > 0) {
		cols = data->battery_adjust_temp_fcc_lut->cols;
		dev_info(data->dev, "%s, adjust_temp_fcc_lut============= start\n", __func__);
		for (i = 0; i < cols; i++) {
			dev_info(data->dev, "%s, adjust_temp_fcc_lut->fcc[%d] = %d\n",
				 __func__, i, data->battery_adjust_temp_fcc_lut->fcc[i]);
			dev_info(data->dev, "%s, adjust_temp_fcc_lut->temp[%d] = %d\n",
				 __func__, i, data->battery_adjust_temp_fcc_lut->temp[i]);
		}
		dev_info(data->dev, "%s, adjust_temp_fcc_lut============= end\n", __func__);
	}

	/* temp_hc_lut */
	if (data->battery_temp_hc_lut &&
	    data->battery_temp_hc_lut->cols > 0) {
		cols = data->battery_temp_hc_lut->cols;
		dev_info(data->dev, "%s, temp_hc_lut============= start\n", __func__);
		for (i = 0; i < cols; i++) {
			dev_info(data->dev, "%s, temp_hc_lut->hc[%d] = %d\n",
				 __func__, i, data->battery_temp_hc_lut->hc[i]);
			dev_info(data->dev, "%s, temp_fcc_lut->temp[%d] = %d\n",
				 __func__, i, data->battery_temp_hc_lut->temp[i]);
		}
		dev_info(data->dev, "%s, temp_hc_lut============= end\n", __func__);
	}

	/* temp_zp_vol_lut */
	if (data->battery_temp_zp_vol_lut &&
	    data->battery_temp_zp_vol_lut->cols > 0) {
		cols = data->battery_temp_zp_vol_lut->cols;
		dev_info(data->dev, "%s, temp_zp_vol_lut============= start\n", __func__);
		for (i = 0; i < cols; i++) {
			dev_info(data->dev, "%s, temp_zp_vol_lut->vol[%d] = %d\n",
				 __func__, i, data->battery_temp_zp_vol_lut->vol[i]);
			dev_info(data->dev, "%s, temp_zp_vol_lut->temp[%d] = %d\n",
				 __func__, i, data->battery_temp_zp_vol_lut->temp[i]);
		}
		dev_info(data->dev, "%s, temp_zp_vol_lut============= end\n", __func__);
	}

	/* temp_abs_zp_vol_lut */
	if (data->battery_temp_abs_zp_vol_lut &&
		data->battery_temp_abs_zp_vol_lut->cols > 0) {
		cols = data->battery_temp_abs_zp_vol_lut->cols;
		dev_info(data->dev, "%s, temp_abs_zp_vol_lut============= start\n", __func__);
		for (i = 0; i < cols; i++) {
			dev_info(data->dev, "%s, temp_abs_zp_vol_lut->vol[%d] = %d\n",
				 __func__, i, data->battery_temp_abs_zp_vol_lut->vol[i]);
			dev_info(data->dev, "%s, temp_abs_zp_vol_lut->temp[%d] = %d\n",
				 __func__, i, data->battery_temp_abs_zp_vol_lut->temp[i]);
		}
		dev_info(data->dev, "%s, temp_abs_zp_vol_lut============= end\n", __func__);
	}

	/* temp_bat_vol_lut */
	if (data->battery_temp_bat_vol_lut &&
	    data->battery_temp_bat_vol_lut->cols > 0) {
		cols = data->battery_temp_bat_vol_lut->cols;
		dev_info(data->dev, "%s, temp_bat_vol_lut============= start\n", __func__);
		for (i = 0; i < cols; i++) {
			dev_info(data->dev, "%s, temp_bat_vol_lut->vol[%d] = %d\n",
				 __func__, i, data->battery_temp_bat_vol_lut->vol[i]);
			dev_info(data->dev, "%s, temp_bat_vol_lut->temp[%d] = %d\n",
				 __func__, i, data->battery_temp_bat_vol_lut->temp[i]);
		}
		dev_info(data->dev, "%s, temp_bat_vol_lut============= end\n", __func__);
	}

	/* cap_temp_ocv_lut */
	if (data->battery_cap_temp_ocv_lut &&
	    data->battery_cap_temp_ocv_lut->cols > 0 &&
	    data->battery_cap_temp_ocv_lut->rows > 0) {
		cols = data->battery_cap_temp_ocv_lut->cols;
		rows = data->battery_cap_temp_ocv_lut->rows;

		dev_info(data->dev, "%s, cap_temp_ocv_lut============= start\n", __func__);

		for (i = 0; i < cols; i++)
			dev_info(data->dev, "%s, cap_temp_ocv_lut->temp[%d] = %d\n",
				 __func__, i, data->battery_cap_temp_ocv_lut->temp[i]);

		for (i = 0; i < rows; i++)
			dev_info(data->dev, "%s, cap_temp_ocv_lut->cap[%d] = %d\n",
				 __func__, i, data->battery_cap_temp_ocv_lut->cap[i]);

		for (i = 0; i < rows; i++) {
			for (j = 0; j < cols; j++)
				dev_info(data->dev, "%s, cap_temp_ocv_lut->ocv[%d][%d] = %d\n",
				 __func__, i, j, data->battery_cap_temp_ocv_lut->ocv[i][j]);
		}
		dev_info(data->dev, "%s, cap_temp_ocv_lut============= end\n", __func__);
	}

	/* cap_temp_ocv_cycle_lut */
	if (data->battery_cap_temp_ocv_cycle_lut &&
	    data->battery_cap_temp_ocv_cycle_lut->cols > 0 &&
	    data->battery_cap_temp_ocv_cycle_lut->rows > 0) {
		cols = data->battery_cap_temp_ocv_cycle_lut->cols;
		rows = data->battery_cap_temp_ocv_cycle_lut->rows;

		dev_info(data->dev, "%s, cap_temp_ocv_cycle_lut============= start\n", __func__);

		for (i = 0; i < cols; i++)
			dev_info(data->dev, "%s, cap_temp_ocv_cycle_lut->temp[%d] = %d\n",
				 __func__, i, data->battery_cap_temp_ocv_cycle_lut->temp[i]);

		for (i = 0; i < rows; i++)
			dev_info(data->dev, "%s, cap_temp_ocv_cycle_lut->cap[%d] = %d\n",
				 __func__, i, data->battery_cap_temp_ocv_cycle_lut->cap[i]);

		for (i = 0; i < rows; i++) {
			for (j = 0; j < cols; j++)
				dev_info(data->dev, "%s, cap_temp_ocv_cycle_lut->cycle_ocv[%d][%d] = %d\n",
					 __func__, i, j,
					 data->battery_cap_temp_ocv_cycle_lut->cycle_ocv[i][j]);
		}
		dev_info(data->dev, "%s, cap_temp_ocv_cycle_lut============= end\n", __func__);
	}

	/* cap_temp_ibat_lut */
	if (data->battery_cap_temp_ibat_lut &&
	    data->battery_cap_temp_ibat_lut->cols > 0 &&
	    data->battery_cap_temp_ibat_lut->rows > 0) {
		cols = data->battery_cap_temp_ibat_lut->cols;
		rows = data->battery_cap_temp_ibat_lut->rows;

		dev_info(data->dev, "%s, cap_temp_ibat_lut============= start\n", __func__);

		for (i = 0; i < cols; i++)
			dev_info(data->dev, "%s, cap_temp_ibat_lut->temp[%d] = %d\n",
				 __func__, i, data->battery_cap_temp_ibat_lut->temp[i]);

		for (i = 0; i < rows; i++)
			dev_info(data->dev, "%s, cap_temp_ibat_lut->cap[%d] = %d\n",
				 __func__, i, data->battery_cap_temp_ibat_lut->cap[i]);

		for (i = 0; i < rows; i++) {
			for (j = 0; j < cols; j++)
				dev_info(data->dev, "%s, cap_temp_ibat_lut->ibat[%d][%d] = %d\n",
				 __func__, i, j, data->battery_cap_temp_ibat_lut->ibat[i][j]);
		}
		dev_info(data->dev, "%s, cap_temp_ibat_lut============= end\n", __func__);
	}

	/* cap_temp_resist_lut */
	if (data->battery_cap_temp_resist_lut &&
	    data->battery_cap_temp_resist_lut->cols > 0 &&
	    data->battery_cap_temp_resist_lut->rows > 0) {
		cols = data->battery_cap_temp_resist_lut->cols;
		rows = data->battery_cap_temp_resist_lut->rows;

		dev_info(data->dev, "%s, cap_temp_resist_lut============= start\n", __func__);

		for (i = 0; i < cols; i++)
			dev_info(data->dev, "%s, cap_temp_resist_lut->temp[%d] = %d\n",
				 __func__, i, data->battery_cap_temp_resist_lut->temp[i]);

		for (i = 0; i < rows; i++)
			dev_info(data->dev, "%s, cap_temp_resist_lut->cap[%d] = %d\n",
				 __func__, i, data->battery_cap_temp_resist_lut->cap[i]);

		for (i = 0; i < rows; i++) {
			for (j = 0; j < cols; j++)
				dev_info(data->dev, "%s, cap_temp_resist_lut->resist[%d][%d] = %d\n",
				 __func__, i, j, data->battery_cap_temp_resist_lut->resist[i][j]);
		}
		dev_info(data->dev, "%s, cap_temp_resist_lut============= end\n", __func__);
	}

	/* cap_temp_resist_ratio_lut */
	if (data->battery_cap_temp_resist_ratio_lut_len > 0 &&
	    data->battery_cap_temp_resist_ratio_lut[0]->cols > 0 &&
	    data->battery_cap_temp_resist_ratio_lut[0]->rows > 0) {
		int index, len = data->battery_cap_temp_resist_ratio_lut_len;
		struct sprd_cap_temp_resist_ratio_lut *cap_temp_resist_ratio_lut;

		dev_info(data->dev, "%s, cap_temp_resist_ratio_lut============= start\n", __func__);

		for (index = 0; index < len; index++) {
			cols = data->battery_cap_temp_resist_ratio_lut[index]->cols;
			rows = data->battery_cap_temp_resist_ratio_lut[index]->rows;
			cap_temp_resist_ratio_lut = data->battery_cap_temp_resist_ratio_lut[index];

			dev_info(data->dev, "%s, cap_temp_resist_ratio_lut index[%d]===== start\n",
				 __func__, index);

			dev_info(data->dev, "%s, battery_cap_temp_resist_ratio_table->[%d] = %d\n",
				 __func__, index,
				 data->battery_cap_temp_resist_ratio_table[index]);

			for (i = 0; i < cols; i++)
				dev_info(data->dev, "%s, cap_temp_resist_ratio_lut->temp[%d] = %d\n",
					 __func__, i,
					 cap_temp_resist_ratio_lut->temp[i]);

			for (i = 0; i < rows; i++)
				dev_info(data->dev, "%s, cap_temp_resist_ratio_lut->cap[%d] = %d\n",
					 __func__, i,
					 cap_temp_resist_ratio_lut->cap[i]);

			for (i = 0; i < rows; i++) {
				for (j = 0; j < cols; j++)
					dev_info(data->dev, "%s, cap_temp_resist_ratio_lut->res_ratio[%d][%d] = %d\n",
						 __func__, i, j,
						 cap_temp_resist_ratio_lut->res_ratio[i][j]);
			}

			dev_info(data->dev, "%s, cap_temp_resist_ratio_lut index[%d]======= end\n",
				 __func__, index);
		}

		dev_info(data->dev, "%s, cap_temp_resist_ratio_lut============= end\n", __func__);
	}

	/* fcc_cycle_ratio_lut */
	if (data->battery_fcc_cycle_ratio_lut &&
	    data->battery_fcc_cycle_ratio_lut->cols > 0 &&
	    data->battery_fcc_cycle_ratio_lut->rows > 0) {
		cols = data->battery_fcc_cycle_ratio_lut->cols;
		rows = data->battery_fcc_cycle_ratio_lut->rows;

		dev_info(data->dev, "%s, fcc_cycle_ratio_lut============= start\n", __func__);

		for (i = 0; i < cols; i++)
			dev_info(data->dev, "%s, fcc_cycle_ratio_lut->temp[%d] = %d\n",
				 __func__, i, data->battery_fcc_cycle_ratio_lut->temp[i]);

		for (i = 0; i < rows; i++)
			dev_info(data->dev, "%s, fcc_cycle_ratio_lut->cycle[%d] = %d\n",
				 __func__, i, data->battery_fcc_cycle_ratio_lut->cycle[i]);

		for (i = 0; i < rows; i++) {
			for (j = 0; j < cols; j++)
				dev_info(data->dev, "%s, fcc_cycle_ratio_lut->ratio[%d][%d] = %d\n",
					 __func__, i, j,
					 data->battery_fcc_cycle_ratio_lut->ratio[i][j]);
		}
		dev_info(data->dev, "%s, fcc_cycle_ratio_lut============= end\n", __func__);
	}
}

static void sprd_fgu_dump_info(struct sprd_fgu_data *data)
{
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	fgu_info->ops->dump_fgu_info(fgu_info, DUMP_FGU_INFO_LEVEL_1);

	dev_info(data->dev, "data->cc_mah = %d, Tbat = %d, uuuah = %d, track_sts = %d, battery soc = %d, cycle = %d, ocv_uv = %d, vbat_mv = %d, cur_ma = %d, vbat_avg_mv = %d, cur_avg_ma = %d, absolute_charger = %d, full_percent = %d\n",
		 data->cc_uah / 1000, data->bat_temp, data->batt_uuuah, data->track.state,
		 data->bat_soc, data->work_cycle, data->vbat_info.ocv_uv, data->vbat_info.vbat_mv,
		 data->vbat_info.vbat_cur_ma, data->vbat_info.vbat_avg_mv,
		 data->vbat_info.vbat_cur_avg_ma, data->vbat_info.absolute_charger_mode,
		 data->cap_remap_full_percent);
}

static bool sprd_fgu_is_in_low_energy_dens(struct sprd_fgu_data *data, int ocv_uv)
{
	bool is_matched = false;
	int i, len = data->cap_calib_dens_ocv_table_len;
	density_ocv_table *table = data->cap_calib_dens_ocv_table;

	if (len == 0) {
		dev_warn(data->dev, "energy density ocv table len is 0 !!!!\n");
		return is_matched;
	}

	for (i = 0; i < len; i++) {
		if (ocv_uv > table[i].engy_dens_ocv_lo &&
		    ocv_uv < table[i].engy_dens_ocv_hi) {
			dev_info(data->dev, "low ernergy dens matched, vol = %d\n", ocv_uv);
			is_matched = true;
			break;
		}
	}

	if (!is_matched)
		dev_info(data->dev, "ocv_uv[%d] is out of dens range\n", ocv_uv);

	return is_matched;
}

static int sprd_fgu_read_last_batt_ocv(struct sprd_fgu_data *data, int *batt_ocv_uv,
				       int *reg, int *magic)
{
	int ret = 0, batt_ocv_mv;
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	ret = fgu_info->ops->read_last_batt_ocv(fgu_info, &batt_ocv_mv, reg, magic);
	if (ret) {
		dev_err(data->dev, "Failed to read batt ocv mv, ret = %d\n", ret);
		return ret;
	}

	*batt_ocv_uv = batt_ocv_mv * 1000;
	dev_info(data->dev, "%s:batt_ocv_mv = %d, reg = 0x%x, magic = 0x%x\n",
		 __func__, batt_ocv_mv, *reg, *magic);

	return ret;
}

static int sprd_fgu_read_last_smooth_soc(struct sprd_fgu_data *data,
					 int *smooth_soc, int *reg, int *magic)
{
	int ret = 0, cap;
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	ret = fgu_info->ops->read_last_smooth_cap(fgu_info, &cap, reg, magic);
	if (ret) {
		dev_err(data->dev, "Failed to read smooth cap, ret = %d\n",
			ret);
		return ret;
	}

	*smooth_soc = cap;
	dev_info(data->dev, "%s:smooth_soc = %d, reg = 0x%x, magic = 0x%x\n",
		 __func__, cap, *reg, *magic);

	return ret;
}

static int sprd_fgu_save_last_batt_ocv(struct sprd_fgu_data *data, int batt_ocv_uv,
				       int first_poweron)
{
	int ret = 0, batt_ocv_mv;
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	dev_info(data->dev, "%s:batt_ocv_uv = %d\n", __func__, batt_ocv_uv);
	batt_ocv_mv = DIV_ROUND_CLOSEST(batt_ocv_uv, 1000);
	ret = fgu_info->ops->save_last_batt_ocv(fgu_info, batt_ocv_mv, first_poweron);
	if (ret) {
		dev_err(data->dev, "Failed to save batt ocv mv, ret = %d\n", ret);
		return ret;
	}

	return ret;
}

static int sprd_fgu_save_last_smooth_soc(struct sprd_fgu_data *data,
					 int smooth_soc)
{
	int ret = 0;
	struct sprd_fgu_info *fgu_info = data->fgu_info;
	static int last_smooth_soc = -1;

	if (last_smooth_soc == smooth_soc)
		return 0;
	dev_info(data->dev, "%s:smooth_soc = %d\n", __func__, smooth_soc);
	ret = fgu_info->ops->save_last_smooth_cap(fgu_info, smooth_soc);
	if (ret) {
		dev_err(data->dev, "Failed to save smooth_soc, ret = %d\n",
			ret);
		return ret;
	}
	last_smooth_soc = smooth_soc;

	return ret;
}

static int sprd_fgu_read_last_cc_uah(struct sprd_fgu_data *data, int *cc_uah,
				     int *reg, int *magic)
{
	int ret = 0;
	int cc_mah;
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	ret = fgu_info->ops->read_last_batt_cc_mah(fgu_info, &cc_mah, reg, magic);
	if (ret) {
		dev_err(data->dev, "Failed to read batt cc mah, ret = %d\n", ret);
		return ret;
	}

	*cc_uah = cc_mah * 1000;
	dev_info(data->dev, "%s:cc_mah = %d\n", __func__, cc_mah);

	return ret;
}

static int sprd_fgu_save_last_cc_uah(struct sprd_fgu_data *data, int cc_uah)
{
	int ret = 0;
	int cc_mah;
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	dev_info(data->dev, "%s:cc_uah = %d\n", __func__, cc_uah);
	cc_mah = DIV_ROUND_CLOSEST(cc_uah, 1000);
	ret = fgu_info->ops->save_last_batt_cc_mah(fgu_info, cc_mah);
	if (ret) {
		dev_err(data->dev, "Failed to save batt cc mah, ret = %d\n", ret);
		return ret;
	}

	return ret;
}

static int sprd_fgu_read_last_batt_temp(struct sprd_fgu_data *data, int *batt_temp,
				     int *reg, int *magic)
{
	int ret = 0;
	int tbatt;
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	ret = fgu_info->ops->read_last_batt_temp(fgu_info, &tbatt, reg, magic);
	if (ret) {
		dev_err(data->dev, "Failed to read batt temp, ret = %d\n", ret);
		return ret;
	}

	*batt_temp = tbatt;
	dev_info(data->dev, "%s:batt_temp = %d\n", __func__, tbatt);

	return ret;
}

static int sprd_fgu_save_last_batt_temp(struct sprd_fgu_data *data, int batt_temp)
{
	int ret = 0;
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	dev_info(data->dev, "%s:batt_temp = %d\n", __func__, batt_temp);
	ret = fgu_info->ops->save_last_batt_temp(fgu_info, batt_temp);
	if (ret) {
		dev_err(data->dev, "Failed to save batt temp, ret = %d\n", ret);
		return ret;
	}

	return ret;
}

static int sprd_fgu_read_last_shutdown_batt_temp(struct sprd_fgu_data *data, int *batt_temp,
						 int *reg, int *magic)
{
	int ret = 0;
	int tbatt;
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	ret = fgu_info->ops->read_last_shutdown_batt_temp(fgu_info, &tbatt, reg, magic);
	if (ret) {
		dev_err(data->dev, "Failed to read batt temp, ret = %d\n", ret);
		return ret;
	}

	*batt_temp = tbatt;
	dev_info(data->dev, "%s:batt_temp = %d\n", __func__, tbatt);

	return ret;
}

static int sprd_fgu_save_last_shutdown_batt_temp(struct sprd_fgu_data *data, int batt_temp)
{
	int ret = 0;
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	dev_info(data->dev, "%s:batt_temp = %d\n", __func__, batt_temp);
	ret = fgu_info->ops->save_last_shutdown_batt_temp(fgu_info, batt_temp);
	if (ret) {
		dev_err(data->dev, "Failed to save batt temp, ret = %d\n", ret);
		return ret;
	}

	return ret;
}

static int sprd_fgu_save_last_ui_soc(struct sprd_fgu_data *data, int cap)
{
	int ret = 0;
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	ret = fgu_info->ops->save_last_cap(fgu_info, cap);
	if (ret) {
		dev_err(data->dev, "Failed to save last ui cap, ret = %d\n", ret);
		return ret;
	}

	return ret;
}

static int interpolate_fcc(struct sprd_fgu_data *data, int temp)
{
	int i, fcc_uah, cols;

	if (temp <= data->battery_temp_fcc_lut->temp[0] * 10)
		return data->battery_temp_fcc_lut->fcc[0];

	cols = data->battery_temp_fcc_lut->cols;
	if (temp >= data->battery_temp_fcc_lut->temp[cols - 1] * 10)
		return data->battery_temp_fcc_lut->fcc[cols - 1];

	for (i = 1; i < cols; i++) {
		if (temp < data->battery_temp_fcc_lut->temp[i] * 10)
			break;
	}

	if (temp == data->battery_temp_fcc_lut->temp[i - 1] * 10) {
		fcc_uah = data->battery_temp_fcc_lut->fcc[i - 1];
	} else {
		fcc_uah = linear_interpolate(data->battery_temp_fcc_lut->fcc[i - 1],
					     data->battery_temp_fcc_lut->temp[i - 1] * 10,
					     data->battery_temp_fcc_lut->fcc[i],
					     data->battery_temp_fcc_lut->temp[i] * 10, temp);
	}

	return fcc_uah;
}

static int interpolate_adjust_fcc(struct sprd_fgu_data *data, int temp)
{
	int i, fcc_uah, cols;

	if (temp <= data->battery_adjust_temp_fcc_lut->temp[0] * 10)
		return data->battery_adjust_temp_fcc_lut->fcc[0];

	cols = data->battery_adjust_temp_fcc_lut->cols;
	if (temp >= data->battery_adjust_temp_fcc_lut->temp[cols - 1] * 10)
		return data->battery_adjust_temp_fcc_lut->fcc[cols - 1];

	for (i = 1; i < cols; i++) {
		if (temp < data->battery_adjust_temp_fcc_lut->temp[i] * 10)
			break;
	}

	if (temp == data->battery_adjust_temp_fcc_lut->temp[i - 1] * 10) {
		fcc_uah = data->battery_adjust_temp_fcc_lut->fcc[i - 1];
	} else {
		fcc_uah = linear_interpolate(data->battery_adjust_temp_fcc_lut->fcc[i - 1],
					     data->battery_adjust_temp_fcc_lut->temp[i - 1] * 10,
					     data->battery_adjust_temp_fcc_lut->fcc[i],
					     data->battery_adjust_temp_fcc_lut->temp[i] * 10, temp);
	}

	return fcc_uah;
}

static int interpolate_hc(struct sprd_fgu_data *data, int temp)
{
	int i, hide_capacity, cols;

	if (temp <= data->battery_temp_hc_lut->temp[0] * 10)
		return data->battery_temp_hc_lut->hc[0];

	cols = data->battery_temp_hc_lut->cols;
	if (temp >= data->battery_temp_hc_lut->temp[cols - 1] * 10)
		return data->battery_temp_hc_lut->hc[cols - 1];

	for (i = 1; i < cols; i++) {
		if (temp < data->battery_temp_hc_lut->temp[i] * 10)
			break;
	}

	if (temp == data->battery_temp_hc_lut->temp[i - 1] * 10) {
		hide_capacity = data->battery_temp_hc_lut->hc[i - 1];
	} else {
		hide_capacity = linear_interpolate(data->battery_temp_hc_lut->hc[i - 1],
						   data->battery_temp_hc_lut->temp[i - 1] * 10,
						   data->battery_temp_hc_lut->hc[i],
						   data->battery_temp_hc_lut->temp[i] * 10, temp);
	}

	return hide_capacity;
}

static int interpolate_chg_loss_uah(struct sprd_fgu_data *data, int temp)
{
	int i, chg_loss_uah, cols;

	if (!data->battery_temp_chg_loss_lut)
		return 0;

	if (temp <= data->battery_temp_chg_loss_lut->temp[0] * 10)
		return data->battery_temp_chg_loss_lut->chg_loss_uah[0];

	cols = data->battery_temp_chg_loss_lut->cols;
	if (temp >= data->battery_temp_chg_loss_lut->temp[cols - 1] * 10)
		return data->battery_temp_chg_loss_lut->chg_loss_uah[cols - 1];

	for (i = 1; i < cols; i++) {
		if (temp < data->battery_temp_chg_loss_lut->temp[i] * 10)
			break;
	}

	if (temp == data->battery_temp_chg_loss_lut->temp[i - 1] * 10) {
		chg_loss_uah = data->battery_temp_chg_loss_lut->chg_loss_uah[i - 1];
	} else {
		chg_loss_uah =
			linear_interpolate(data->battery_temp_chg_loss_lut->chg_loss_uah[i - 1],
					   data->battery_temp_chg_loss_lut->temp[i - 1] * 10,
					   data->battery_temp_chg_loss_lut->chg_loss_uah[i],
					   data->battery_temp_chg_loss_lut->temp[i] * 10, temp);
	}

	return chg_loss_uah;
}

static int interpolate_chg_loss_soc(struct sprd_fgu_data *data, int temp)
{
	int i, chg_loss_soc, cols;

	if (!data->battery_temp_chg_loss_lut)
		return 0;

	if (temp <= data->battery_temp_chg_loss_lut->temp[0] * 10)
		return data->battery_temp_chg_loss_lut->soc_loss[0];

	cols = data->battery_temp_chg_loss_lut->cols;
	if (temp >= data->battery_temp_chg_loss_lut->temp[cols - 1] * 10)
		return data->battery_temp_chg_loss_lut->soc_loss[cols - 1];

	for (i = 1; i < cols; i++) {
		if (temp < data->battery_temp_chg_loss_lut->temp[i] * 10)
			break;
	}

	if (temp == data->battery_temp_chg_loss_lut->temp[i - 1] * 10) {
		chg_loss_soc = data->battery_temp_chg_loss_lut->soc_loss[i - 1];
	} else {
		chg_loss_soc =
			linear_interpolate(data->battery_temp_chg_loss_lut->soc_loss[i - 1],
					   data->battery_temp_chg_loss_lut->temp[i - 1] * 10,
					   data->battery_temp_chg_loss_lut->soc_loss[i],
					   data->battery_temp_chg_loss_lut->temp[i] * 10, temp);
	}

	return chg_loss_soc;
}

static int interpolate_zero_point_voltage(struct sprd_fgu_data *data, int temp)
{
	int i, zp_vol_uv, cols;

	if (temp <= data->battery_temp_zp_vol_lut->temp[0] * 10)
		return data->battery_temp_zp_vol_lut->vol[0];

	cols = data->battery_temp_zp_vol_lut->cols;
	if (temp >= data->battery_temp_zp_vol_lut->temp[cols - 1] * 10)
		return data->battery_temp_zp_vol_lut->vol[cols - 1];

	for (i = 1; i < cols; i++) {
		if (temp < data->battery_temp_zp_vol_lut->temp[i] * 10)
			break;
	}

	if (temp == data->battery_temp_zp_vol_lut->temp[i - 1] * 10) {
		zp_vol_uv = data->battery_temp_zp_vol_lut->vol[i - 1];
	} else {
		zp_vol_uv = linear_interpolate(data->battery_temp_zp_vol_lut->vol[i - 1],
					       data->battery_temp_zp_vol_lut->temp[i - 1] * 10,
					       data->battery_temp_zp_vol_lut->vol[i],
					       data->battery_temp_zp_vol_lut->temp[i] * 10, temp);
	}

	return zp_vol_uv;
}

static int interpolate_abs_zero_point_voltage(struct sprd_fgu_data *data, int temp)
{
	int i, abs_zp_vol_uv, cols;

	if (temp <= data->battery_temp_abs_zp_vol_lut->temp[0] * 10)
		return data->battery_temp_abs_zp_vol_lut->vol[0];

	cols = data->battery_temp_abs_zp_vol_lut->cols;
	if (temp >= data->battery_temp_abs_zp_vol_lut->temp[cols - 1] * 10)
		return data->battery_temp_abs_zp_vol_lut->vol[cols - 1];

	for (i = 1; i < cols; i++) {
		if (temp < data->battery_temp_abs_zp_vol_lut->temp[i] * 10)
			break;
	}

	if (temp == data->battery_temp_abs_zp_vol_lut->temp[i - 1] * 10) {
		abs_zp_vol_uv = data->battery_temp_abs_zp_vol_lut->vol[i - 1];
	} else {
		abs_zp_vol_uv =
			linear_interpolate(data->battery_temp_abs_zp_vol_lut->vol[i - 1],
					   data->battery_temp_abs_zp_vol_lut->temp[i - 1] * 10,
					   data->battery_temp_abs_zp_vol_lut->vol[i],
					   data->battery_temp_abs_zp_vol_lut->temp[i] * 10, temp);
	}

	return abs_zp_vol_uv;
}

static int interpolate_bat_voltage(struct sprd_fgu_data *data, int temp)
{
	int i, bat_vol_uv, cols;

	if (temp <= data->battery_temp_bat_vol_lut->temp[0] * 10)
		return data->battery_temp_bat_vol_lut->vol[0];

	cols = data->battery_temp_bat_vol_lut->cols;
	if (temp >= data->battery_temp_bat_vol_lut->temp[cols - 1] * 10)
		return data->battery_temp_bat_vol_lut->vol[cols - 1];

	for (i = 1; i < cols; i++) {
		if (temp < data->battery_temp_bat_vol_lut->temp[i] * 10)
			break;
	}

	if (temp == data->battery_temp_bat_vol_lut->temp[i - 1] * 10) {
		bat_vol_uv = data->battery_temp_bat_vol_lut->vol[i - 1];
	} else {
		bat_vol_uv = linear_interpolate(data->battery_temp_bat_vol_lut->vol[i - 1],
					       data->battery_temp_bat_vol_lut->temp[i - 1] * 10,
					       data->battery_temp_bat_vol_lut->vol[i],
					       data->battery_temp_bat_vol_lut->temp[i] * 10, temp);
	}

	return bat_vol_uv;
}

static int interpolate_cap2ocv(struct sprd_fgu_data *data, int cap, int temp)
{
	int i, j, rows, cols, cols_ocv1, cols_ocv2, ocv_uv;

	cap  = clamp(cap, 0, 1000);

	if (!data->battery_cap_temp_ocv_lut)
		return 0;

	rows = data->battery_cap_temp_ocv_lut->rows;
	cols = data->battery_cap_temp_ocv_lut->cols;

	for (i = 0; i < rows; i++) {
		if (cap > data->battery_cap_temp_ocv_lut->cap[i] * 10)
			break;
	}
	if (cap > data->battery_cap_temp_ocv_lut->cap[0] * 10) {
		i = 1;
		cap = data->battery_cap_temp_ocv_lut->cap[0] * 10;
	}
	if (cap <= data->battery_cap_temp_ocv_lut->cap[rows - 1] * 10)
		cap = data->battery_cap_temp_ocv_lut->cap[rows - 1] * 10;

	for (j = 0; j < cols; j++) {
		if (temp < data->battery_cap_temp_ocv_lut->temp[j] * 10)
			break;
	}
	if (temp < data->battery_cap_temp_ocv_lut->temp[0] * 10) {
		j = 1;
		temp = data->battery_cap_temp_ocv_lut->temp[0] * 10;
	}
	if (temp >= data->battery_cap_temp_ocv_lut->temp[cols - 1] * 10)
		temp = data->battery_cap_temp_ocv_lut->temp[cols - 1] * 10;

	if (cap == data->battery_cap_temp_ocv_lut->cap[i - 1] * 10) {
		ocv_uv = linear_interpolate(data->battery_cap_temp_ocv_lut->ocv[i - 1][j - 1],
					    data->battery_cap_temp_ocv_lut->temp[j - 1] * 10,
					    data->battery_cap_temp_ocv_lut->ocv[i - 1][j],
					    data->battery_cap_temp_ocv_lut->temp[j] * 10, temp);
	} else if (temp == data->battery_cap_temp_ocv_lut->temp[j - 1] * 10) {
		ocv_uv = linear_interpolate(data->battery_cap_temp_ocv_lut->ocv[i - 1][j - 1],
					    data->battery_cap_temp_ocv_lut->cap[i - 1] * 10,
					    data->battery_cap_temp_ocv_lut->ocv[i][j - 1],
					    data->battery_cap_temp_ocv_lut->cap[i] * 10, cap);
	} else {
		/* fixed temp[j - 1] celsius ocv value */
		cols_ocv1 = linear_interpolate(data->battery_cap_temp_ocv_lut->ocv[i - 1][j - 1],
					       data->battery_cap_temp_ocv_lut->cap[i - 1] * 10,
					       data->battery_cap_temp_ocv_lut->ocv[i][j - 1],
					       data->battery_cap_temp_ocv_lut->cap[i] * 10, cap);

		/* fixed temp[j] celsius ocv value */
		cols_ocv2 = linear_interpolate(data->battery_cap_temp_ocv_lut->ocv[i - 1][j],
					       data->battery_cap_temp_ocv_lut->cap[i - 1] * 10,
					       data->battery_cap_temp_ocv_lut->ocv[i][j],
					       data->battery_cap_temp_ocv_lut->cap[i] * 10, cap);

		ocv_uv = linear_interpolate(cols_ocv1,
					    data->battery_cap_temp_ocv_lut->temp[j - 1] * 10,
					    cols_ocv2,
					    data->battery_cap_temp_ocv_lut->temp[j] * 10, temp);
	}

	return ocv_uv;
}

static int interpolate_cycle_cap2ocv(struct sprd_fgu_data *data, int cap, int temp)
{
	int i, j, rows, cols, cols_ocv1, cols_ocv2, ocv_uv;
	struct sprd_cap_temp_ocv_cycle_lut *table;

	cap  = clamp(cap, 0, 1000);

	if (!data->battery_cap_temp_ocv_cycle_lut)
		return 0;

	table = data->battery_cap_temp_ocv_cycle_lut;

	rows = table->rows;
	cols = table->cols;

	for (i = 0; i < rows; i++) {
		if (cap > table->cap[i] * 10)
			break;
	}
	if (cap > table->cap[0] * 10) {
		i = 1;
		cap = table->cap[0] * 10;
	}
	if (cap <= table->cap[rows - 1] * 10)
		cap = table->cap[rows - 1] * 10;

	for (j = 0; j < cols; j++) {
		if (temp < table->temp[j] * 10)
			break;
	}
	if (temp < table->temp[0] * 10) {
		j = 1;
		temp = table->temp[0] * 10;
	}
	if (temp >= table->temp[cols - 1] * 10)
		temp = table->temp[cols - 1] * 10;

	if (cap == table->cap[i - 1] * 10) {
		ocv_uv = linear_interpolate(table->cycle_ocv[i - 1][j - 1],
					    table->temp[j - 1] * 10,
					    table->cycle_ocv[i - 1][j],
					    table->temp[j] * 10, temp);
	} else if (temp == table->temp[j - 1] * 10) {
		ocv_uv = linear_interpolate(table->cycle_ocv[i - 1][j - 1],
					    table->cap[i - 1] * 10,
					    table->cycle_ocv[i][j - 1],
					    table->cap[i] * 10, cap);
	} else {
		/* fixed temp[j - 1] celsius ocv value */
		cols_ocv1 = linear_interpolate(table->cycle_ocv[i - 1][j - 1],
					       table->cap[i - 1] * 10,
					       table->cycle_ocv[i][j - 1],
					       table->cap[i] * 10, cap);

		/* fixed temp[j] celsius ocv value */
		cols_ocv2 = linear_interpolate(table->cycle_ocv[i - 1][j],
					       table->cap[i - 1] * 10,
					       table->cycle_ocv[i][j],
					       table->cap[i] * 10, cap);

		ocv_uv = linear_interpolate(cols_ocv1, table->temp[j - 1] * 10,
					    cols_ocv2, table->temp[j] * 10, temp);
	}

	return ocv_uv;
}

static int interpolate_cap2ibat(struct sprd_fgu_data *data, int cap, int temp)
{
	int i, j, rows, cols, cols_ibat1, cols_ibat2, ibat_ua;
	struct sprd_cap_temp_ibat_lut *table;

	cap  = clamp(cap, 0, 1000);

	if (!data->battery_cap_temp_ibat_lut)
		return 0;

	table = data->battery_cap_temp_ibat_lut;

	rows = table->rows;
	cols = table->cols;

	for (i = 0; i < rows; i++) {
		if (cap > table->cap[i] * 10)
			break;
	}
	if (cap > table->cap[0] * 10) {
		i = 1;
		cap = table->cap[0] * 10;
	}
	if (cap <= table->cap[rows - 1] * 10)
		cap = table->cap[rows - 1] * 10;

	for (j = 0; j < cols; j++) {
		if (temp < table->temp[j] * 10)
			break;
	}
	if (temp < table->temp[0] * 10) {
		j = 1;
		temp = table->temp[0] * 10;
	}
	if (temp >= table->temp[cols - 1] * 10)
		temp = table->temp[cols - 1] * 10;

	if (cap == table->cap[i - 1] * 10) {
		ibat_ua = linear_interpolate(table->ibat[i - 1][j - 1],
					     table->temp[j - 1] * 10,
					     table->ibat[i - 1][j],
					     table->temp[j] * 10, temp);
	} else if (temp == table->temp[j - 1] * 10) {
		ibat_ua = linear_interpolate(table->ibat[i - 1][j - 1],
					     table->cap[i - 1] * 10,
					     table->ibat[i][j - 1],
					     table->cap[i] * 10, cap);
	} else {
		/* fixed temp[j - 1] celsius ocv value */
		cols_ibat1 = linear_interpolate(table->ibat[i - 1][j - 1],
						table->cap[i - 1] * 10,
						table->ibat[i][j - 1],
						table->cap[i] * 10, cap);

		/* fixed temp[j] celsius ocv value */
		cols_ibat2 = linear_interpolate(table->ibat[i - 1][j],
						table->cap[i - 1] * 10,
						table->ibat[i][j],
						table->cap[i] * 10, cap);

		ibat_ua = linear_interpolate(cols_ibat1, table->temp[j - 1] * 10,
					     cols_ibat2, table->temp[j] * 10, temp);
	}

	return ibat_ua;
}

static int interpolate_ocv2cap(struct sprd_fgu_data *data, int temp, int ocv)
{
	int i, j, k, l, rows, cols, rows_ocv, cap;
	int interpolate_ocv[SPRD_CAP_TEMP_ROWS_MAX] = {0};

	if (!data->battery_cap_temp_ocv_lut)
		return 0;

	rows = data->battery_cap_temp_ocv_lut->rows;
	cols = data->battery_cap_temp_ocv_lut->cols;

	for (j = 0; j < cols; j++) {
		if (temp < data->battery_cap_temp_ocv_lut->temp[j] * 10)
			break;
	}

	if (temp <= data->battery_cap_temp_ocv_lut->temp[0] * 10) {
		j = 1;
		temp = data->battery_cap_temp_ocv_lut->temp[0] * 10;
	}

	if (temp >= data->battery_cap_temp_ocv_lut->temp[cols - 1] * 10)
		temp = data->battery_cap_temp_ocv_lut->temp[cols - 1] * 10;

	if (temp == data->battery_cap_temp_ocv_lut->temp[j - 1] * 10) {
		if (ocv >= data->battery_cap_temp_ocv_lut->ocv[0][j - 1])
			return data->battery_cap_temp_ocv_lut->cap[0] * 10;

		if (ocv <= data->battery_cap_temp_ocv_lut->ocv[rows - 1][j - 1])
			return data->battery_cap_temp_ocv_lut->cap[rows - 1] * 10;

		for (l = 1; l < rows; l++) {
			if (ocv > data->battery_cap_temp_ocv_lut->ocv[l][j - 1])
				break;
		}

		cap = linear_interpolate(data->battery_cap_temp_ocv_lut->cap[l - 1] * 10,
					 data->battery_cap_temp_ocv_lut->ocv[l - 1][j - 1],
					 data->battery_cap_temp_ocv_lut->cap[l] * 10,
					 data->battery_cap_temp_ocv_lut->ocv[l][j - 1], ocv);

		return cap;
	}

	for (i = 0; i < rows; i++) {
		rows_ocv = linear_interpolate(data->battery_cap_temp_ocv_lut->ocv[i][j - 1],
					      data->battery_cap_temp_ocv_lut->temp[j - 1] * 10,
					      data->battery_cap_temp_ocv_lut->ocv[i][j],
					      data->battery_cap_temp_ocv_lut->temp[j] * 10, temp);

		interpolate_ocv[i] = rows_ocv;
	}

	if (ocv >= interpolate_ocv[0])
		return data->battery_cap_temp_ocv_lut->cap[0] * 10;

	if (ocv <= interpolate_ocv[rows - 1])
		return data->battery_cap_temp_ocv_lut->cap[rows - 1] * 10;

	for (k = 1; k < rows; k++) {
		if (ocv > interpolate_ocv[k])
			break;
	}

	cap = linear_interpolate(data->battery_cap_temp_ocv_lut->cap[k - 1] * 10,
				 interpolate_ocv[k - 1],
				 data->battery_cap_temp_ocv_lut->cap[k] * 10,
				 interpolate_ocv[k], ocv);

	return cap;
}

static int interpolate_cycle_ocv2cap(struct sprd_fgu_data *data, int temp, int ocv)
{
	int i, j, k, l, rows, cols, rows_ocv, cap;
	int interpolate_cycle_ocv[SPRD_CAP_TEMP_ROWS_MAX] = {0};
	struct sprd_cap_temp_ocv_cycle_lut *table;

	if (!data->battery_cap_temp_ocv_cycle_lut)
		return 0;

	table = data->battery_cap_temp_ocv_cycle_lut;

	rows = table->rows;
	cols = table->cols;

	for (j = 0; j < cols; j++) {
		if (temp < table->temp[j] * 10)
			break;
	}

	if (temp < table->temp[0] * 10) {
		j = 1;
		temp = table->temp[0] * 10;
	}

	if (temp >= table->temp[cols - 1] * 10)
		temp = table->temp[cols - 1] * 10;

	if (temp == table->temp[j - 1] * 10) {
		if (ocv >= table->cycle_ocv[0][j - 1])
			return table->cap[0] * 10;

		if (ocv <= table->cycle_ocv[rows - 1][j - 1])
			return table->cap[rows - 1] * 10;

		for (l = 1; l < rows; l++) {
			if (ocv > table->cycle_ocv[l][j - 1])
				break;
		}

		cap = linear_interpolate(table->cap[l - 1] * 10,
					 table->cycle_ocv[l - 1][j - 1],
					 table->cap[l] * 10,
					 table->cycle_ocv[l][j - 1], ocv);

		return cap;
	}

	for (i = 0; i < rows; i++) {
		rows_ocv = linear_interpolate(table->cycle_ocv[i][j - 1],
					      table->temp[j - 1] * 10,
					      table->cycle_ocv[i][j],
					      table->temp[j] * 10, temp);

		interpolate_cycle_ocv[i] = rows_ocv;
	}

	if (ocv >= interpolate_cycle_ocv[0])
		return table->cap[0] * 10;

	if (ocv <= interpolate_cycle_ocv[rows - 1])
		return table->cap[rows - 1] * 10;

	for (k = 1; k < rows; k++) {
		if (ocv > interpolate_cycle_ocv[k])
			break;
	}

	cap = linear_interpolate(table->cap[k - 1] * 10,
				 interpolate_cycle_ocv[k - 1],
				 table->cap[k] * 10,
				 interpolate_cycle_ocv[k], ocv);

	return cap;
}

static int interpolate_ibat2cap(struct sprd_fgu_data *data, int temp, int ibat)
{
	int i, j, k, l, rows, cols, rows_ibat, cap;
	int interpolate_ibat[SPRD_CAP_TEMP_ROWS_MAX] = {0};

	if (!data->battery_cap_temp_ibat_lut)
		return 1000;

	rows = data->battery_cap_temp_ibat_lut->rows;
	cols = data->battery_cap_temp_ibat_lut->cols;

	for (j = 0; j < cols; j++) {
		if (temp < data->battery_cap_temp_ibat_lut->temp[j] * 10)
			break;
	}

	if (temp < data->battery_cap_temp_ibat_lut->temp[0] * 10) {
		j = 1;
		temp = data->battery_cap_temp_ibat_lut->temp[0] * 10;
	}

	if (temp >= data->battery_cap_temp_ibat_lut->temp[cols - 1] * 10)
		temp = data->battery_cap_temp_ibat_lut->temp[cols - 1] * 10;

	if (temp == data->battery_cap_temp_ibat_lut->temp[j - 1] * 10) {
		if (ibat <= data->battery_cap_temp_ibat_lut->ibat[0][j - 1])
			return data->battery_cap_temp_ibat_lut->cap[0] * 10;

		if (ibat >= data->battery_cap_temp_ibat_lut->ibat[rows - 1][j - 1])
			return data->battery_cap_temp_ibat_lut->cap[rows - 1] * 10;

		for (l = 1; l < rows; l++) {
			if (ibat < data->battery_cap_temp_ibat_lut->ibat[l][j - 1])
				break;
		}

		cap = linear_interpolate(data->battery_cap_temp_ibat_lut->cap[l - 1] * 10,
					 data->battery_cap_temp_ibat_lut->ibat[l - 1][j - 1],
					 data->battery_cap_temp_ibat_lut->cap[l] * 10,
					 data->battery_cap_temp_ibat_lut->ibat[l][j - 1], ibat);

		return cap;
	}

	for (i = 0; i < rows; i++) {
		rows_ibat = linear_interpolate(data->battery_cap_temp_ibat_lut->ibat[i][j - 1],
					      data->battery_cap_temp_ocv_lut->temp[j - 1] * 10,
					      data->battery_cap_temp_ibat_lut->ibat[i][j],
					      data->battery_cap_temp_ocv_lut->temp[j] * 10, temp);

		interpolate_ibat[i] = rows_ibat;
	}

	if (ibat <= interpolate_ibat[0])
		return data->battery_cap_temp_ibat_lut->cap[0] * 10;

	if (ibat >= interpolate_ibat[rows - 1])
		return data->battery_cap_temp_ibat_lut->cap[rows - 1] * 10;

	for (k = 1; k < rows; k++) {
		if (ibat < interpolate_ibat[k])
			break;
	}

	cap = linear_interpolate(data->battery_cap_temp_ibat_lut->cap[k - 1] * 10,
				 interpolate_ibat[k - 1],
				 data->battery_cap_temp_ibat_lut->cap[k] * 10,
				 interpolate_ibat[k], ibat);

	return cap;
}

/* to do  check */
static int interpolate_cap2resist(struct sprd_fgu_data *data, int cap, int temp)
{
	int i, j, rows, cols, cols_resist1, cols_resist2, resist_moh;
	struct sprd_cap_temp_resist_lut *table;

	if (cap > 1000)
		cap = 1000;
	else if (cap < 0)
		cap = 0;

	table = data->battery_cap_temp_resist_lut;
	rows = table->rows;
	cols = table->cols;

	for (i = 0; i < rows; i++) {
		if (cap > table->cap[i] * 10)
			break;
	}
	if (cap > table->cap[0] * 10) {
		i = 1;
		cap = table->cap[0] * 10;
	}
	if (cap <= table->cap[rows - 1] * 10)
		cap = table->cap[rows - 1] * 10;

	for (j = 0; j < cols; j++) {
		if (temp < table->temp[j] * 10)
			break;
	}
	if (temp < table->temp[0] * 10) {
		j = 1;
		temp = table->temp[0] * 10;
	}
	if (temp >= table->temp[cols - 1] * 10)
		temp = table->temp[cols - 1] * 10;

	if (cap == table->cap[i - 1] * 10) {
		resist_moh = linear_interpolate(table->resist[i - 1][j - 1],
						table->temp[j - 1] * 10,
						table->resist[i - 1][j],
						table->temp[j] * 10, temp);
	} else if (temp == table->temp[j - 1] * 10) {
		resist_moh = linear_interpolate(table->resist[i - 1][j - 1],
						table->cap[i - 1] * 10,
						table->resist[i][j - 1],
						table->cap[i] * 10, cap);
	} else {
		/* fixed temp[j - 1] celsius resist value */
		cols_resist1 = linear_interpolate(table->resist[i - 1][j - 1],
						  table->cap[i - 1] * 10,
						  table->resist[i][j - 1],
						  table->cap[i] * 10, cap);

		/* fixed temp[j] celsius resist value */
		cols_resist2 = linear_interpolate(table->resist[i - 1][j],
						  table->cap[i - 1] * 10,
						  table->resist[i][j],
						  table->cap[i] * 10, cap);

		resist_moh = linear_interpolate(cols_resist1, table->temp[j - 1] * 10,
						cols_resist2, table->temp[j] * 10, temp);
	}

	return resist_moh;
}

/* to do  check */
static int interpolate_cap2resist_ratio(struct sprd_fgu_data *data, int cap, int temp, int index)
{
	int i, j, rows, cols, cols_res_ratio1, cols_res_ratio2, resist_ratio;
	struct sprd_cap_temp_resist_ratio_lut *table;

	if (cap > 1000)
		cap = 1000;
	else if (cap < 0)
		cap = 0;

	if (index < 0 || index >= data->battery_cap_temp_resist_ratio_lut_len) {
		dev_err(data->dev, "%s,index = %d, error!\n", __func__, index);
		return 1000;
	}

	table = data->battery_cap_temp_resist_ratio_lut[index];

	rows = table->rows;
	cols = table->cols;

	for (i = 0; i < rows; i++) {
		if (cap > table->cap[i] * 10)
			break;
	}
	if (cap > table->cap[0] * 10) {
		i = 1;
		cap = table->cap[0] * 10;
	}
	if (cap <= table->cap[rows - 1] * 10)
		cap = table->cap[rows - 1] * 10;

	for (j = 0; j < cols; j++) {
		if (temp < table->temp[j] * 10)
			break;
	}
	if (temp < table->temp[0] * 10) {
		j = 1;
		temp = table->temp[0] * 10;
	}
	if (temp >= table->temp[cols - 1] * 10)
		temp = table->temp[cols - 1] * 10;

	if (cap == table->cap[i - 1] * 10 &&
		temp == table->temp[j - 1] * 10) {
		return table->res_ratio[i - 1][j - 1];
	} else if (cap == table->cap[i - 1] * 10) {
		resist_ratio = linear_interpolate(table->res_ratio[i - 1][j - 1],
						table->temp[j - 1] * 10,
						table->res_ratio[i - 1][j],
						table->temp[j] * 10, temp);
	} else if (temp == table->temp[j - 1] * 10) {
		resist_ratio = linear_interpolate(table->res_ratio[i - 1][j - 1],
						table->cap[i - 1] * 10,
						table->res_ratio[i][j - 1],
						table->cap[i] * 10, cap);
	} else {
		/* fixed temp[j - 1] celsius resist value */
		cols_res_ratio1 = linear_interpolate(table->res_ratio[i - 1][j - 1],
						  table->cap[i - 1] * 10,
						  table->res_ratio[i][j - 1],
						  table->cap[i] * 10, cap);

		/* fixed temp[j] celsius resist value */
		cols_res_ratio2 = linear_interpolate(table->res_ratio[i - 1][j],
						  table->cap[i - 1] * 10,
						  table->res_ratio[i][j],
						  table->cap[i] * 10, cap);

		resist_ratio = linear_interpolate(cols_res_ratio1, table->temp[j - 1] * 10,
						cols_res_ratio2, table->temp[j] * 10, temp);
	}

	return resist_ratio;
}

static int interpolate_cycle2ratio(struct sprd_fgu_data *data, int cycle, int temp)
{
	int i, j, rows, cols, cols_ratio1, cols_ratio2, ratio;
	struct sprd_fcc_cycle_ratio_lut *table;

	if (!data->battery_fcc_cycle_ratio_lut)
		return 1000;

	if (cycle < 0)
		cycle = 0;

	table = data->battery_fcc_cycle_ratio_lut;
	rows = table->rows;
	cols = table->cols;

	for (i = 0; i < rows; i++) {
		if (cycle > table->cycle[i] * 1000)
			break;
	}
	if (cycle > table->cycle[0] * 1000) {
		i = 1;
		cycle = table->cycle[0] * 1000;
	}
	if (cycle <= table->cycle[rows - 1] * 1000)
		cycle = table->cycle[rows - 1] * 1000;

	for (j = 0; j < cols; j++) {
		if (temp < table->temp[j] * 10)
			break;
	}
	if (temp < table->temp[0] * 10) {
		j = 1;
		temp = table->temp[0] * 10;
	}
	if (temp >= table->temp[cols - 1] * 10)
		temp = table->temp[cols - 1] * 10;

	if (cycle == table->cycle[i - 1] * 1000) {
		ratio = linear_interpolate(table->ratio[i - 1][j - 1],
					   table->temp[j - 1] * 10,
					   table->ratio[i - 1][j],
					   table->temp[j] * 10, temp);
	} else if (temp == table->temp[j - 1] * 10) {
		ratio = linear_interpolate(table->ratio[i - 1][j - 1],
					   table->cycle[i - 1] * 1000,
					   table->ratio[i][j - 1],
					   table->cycle[i] * 1000, cycle);
	} else {
		/* fixed temp[j - 1] celsius resist value */
		cols_ratio1 = linear_interpolate(table->ratio[i - 1][j - 1],
						 table->cycle[i - 1] * 1000,
						 table->ratio[i][j - 1],
						 table->cycle[i] * 1000, cycle);

		/* fixed temp[j] celsius resist value */
		cols_ratio2 = linear_interpolate(table->ratio[i - 1][j],
						 table->cycle[i - 1] * 1000,
						 table->ratio[i][j],
						 table->cycle[i] * 1000, cycle);

		ratio = linear_interpolate(cols_ratio1, table->temp[j - 1] * 10,
					   cols_ratio2, table->temp[j] * 10, temp);
	}

	return ratio;
}

static int sprd_fgu_get_soc_by_ocv(struct sprd_fgu_data *data, int batt_ocv, int batt_tbat)
{
	int soc;

	/* 0% - 100% => 0% - 1000% */
	if (data->battery_cap_temp_ocv_cycle_lut)
		soc = interpolate_cycle_ocv2cap(data, batt_tbat, batt_ocv);
	else
		soc = interpolate_ocv2cap(data, batt_tbat, batt_ocv);

	return soc;
}

static int sprd_fgu_get_ccuah(struct sprd_fgu_data *data, int *cc_uah)
{
	int ret;
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	ret = fgu_info->ops->get_cc_uah(fgu_info, cc_uah, true);
	if (ret) {
		dev_err(data->dev, "failed to get cc uah!\n");
		return ret;
	}

	data->cc_uah = *cc_uah;

	*cc_uah = *cc_uah + data->init_cc_uah - data->err_cc_uah -
		data->err_fcc_uah - data->err_cutoff_uah - data->err_shutdown_uah;
	if (data->fgu_info->real_time_calib)
		*cc_uah -= data->err_rt_cc_uah;

	dev_info(data->dev, "cc_uah = %d, init_cc_uah = %d, err_cc_uah = %d, err_fcc_uah = %d, err_cutoff_uah = %d, err_rt_cc_uah = %d, real_time_calib = %d, err_shutdown_uah = %d\n",
			*cc_uah, data->init_cc_uah, data->err_cc_uah, data->err_fcc_uah,
			data->err_cutoff_uah, data->err_rt_cc_uah,
			data->fgu_info->real_time_calib, data->err_shutdown_uah);

	return 0;
}

static int sprd_fgu_get_fcc_uah_by_temp(struct sprd_fgu_data *data, int batt_tbat)
{
	int fcc_uah = 0;

	fcc_uah = interpolate_fcc(data, batt_tbat);

	return fcc_uah;
}

static int sprd_fgu_get_chg_loss_uah_by_temp(struct sprd_fgu_data *data, int batt_tbat)
{
	int chg_loss_uah = 0;

	chg_loss_uah = interpolate_chg_loss_uah(data, batt_tbat);

	return chg_loss_uah;
}

static int sprd_fgu_get_chg_loss_soc_by_temp(struct sprd_fgu_data *data, int batt_tbat)
{
	int chg_loss_soc = 0;

	chg_loss_soc = interpolate_chg_loss_soc(data, batt_tbat);

	return chg_loss_soc;
}

static int sprd_fgu_get_hc_by_temp(struct sprd_fgu_data *data, int batt_tbat)
{
	int hc = data->capacity_hc;

	if (!data->battery_temp_hc_lut)
		return hc;

	hc = interpolate_hc(data, batt_tbat);

	return hc;
}

static int sprd_fgu_get_fcc_scale_ratio_by_cycle(struct sprd_fgu_data *data, int cycles)
{
	int scale_ratio = 1000;

	scale_ratio = interpolate_cycle2ratio(data, cycles, data->bat_temp);
	dev_info(data->dev, "%s: scale_ratio = %d, cycles = %d\n", __func__, scale_ratio, cycles);

	return scale_ratio;
}

static int sprd_fgu_adjust_fcc_uah_by_temp(struct sprd_fgu_data *data, int batt_tbat)
{
	int fcc_uah = 0;

	fcc_uah = interpolate_adjust_fcc(data, batt_tbat);

	return fcc_uah;
}

static int sprd_fgu_get_fcc_uah(struct sprd_fgu_data *data, int batt_tbat, int cycles)
{
	int normal_fcc, adjust_fcc;
	int scale_ratio;

	if (!data->battery_adjust_temp_fcc_lut) {
		/* currently go here */
		normal_fcc = sprd_fgu_get_fcc_uah_by_temp(data, batt_tbat);
		scale_ratio = sprd_fgu_get_fcc_scale_ratio_by_cycle(data, cycles);

		adjust_fcc = normal_fcc / 10 * scale_ratio / 100;
//		dev_info(data->dev, "%s: normal_fcc = %d, scale_ratio = %d, adjust_fcc = %d\n",
//			 __func__, normal_fcc, scale_ratio, adjust_fcc);
	} else {
		adjust_fcc = sprd_fgu_adjust_fcc_uah_by_temp(data, batt_tbat);
//		dev_info(data->dev, "%s: adjust_fcc = %d\n", __func__, adjust_fcc);
	}

	return adjust_fcc;
}

static int sprd_fgu_get_ocv_by_soc(struct sprd_fgu_data *data, int batt_tbat, int soc, int cycle)
{
	int ocv_uv = 0;

	if (cycle && data->battery_cap_temp_ocv_cycle_lut)
		ocv_uv = interpolate_cycle_cap2ocv(data, soc, batt_tbat);
	else
		ocv_uv = interpolate_cap2ocv(data, soc, batt_tbat);

	return ocv_uv;
}

static void sprd_fgu_get_calib_info(struct sprd_fgu_data *data)
{
	struct sprd_fgu_vbat_info *vbat_info = &data->vbat_info;
	struct sprd_fgu_rt_calib_info *calib_info = &data->calib_info[data->rt_calib_index];

	calib_info->rm_soc = data->batt_rm_soc;
	calib_info->rm_ocv_uv = sprd_fgu_get_ocv_by_soc(data, data->bat_temp,
							data->batt_rm_soc, true);
	calib_info->calib_soc = sprd_fgu_get_soc_by_ocv(data, vbat_info->ocv_uv, data->bat_temp);
	calib_info->work_times = max(data->work_enter_times - data->work_exit_times, 1);
	calib_info->calib_ocv_uv = vbat_info->ocv_uv;
	calib_info->vbat_avg_mv = vbat_info->vbat_avg_mv;
	calib_info->cur_avg_ma = vbat_info->vbat_cur_avg_ma;
	calib_info->work_cur_avg_ma = data->ibat_avg_work_cycle_ma;
	calib_info->delta_ocv_uv = calib_info->calib_ocv_uv - calib_info->rm_ocv_uv;
	calib_info->batt_temp = data->bat_temp;

	data->rt_calib_index++;
	data->rt_calib_valid_cnt++;
	if (data->rt_calib_index >= SPRD_FGU_RT_CALIB_INFO_TABLE_LEN)
		data->rt_calib_index = 0;
	if (data->rt_calib_valid_cnt >= SPRD_FGU_RT_CALIB_INFO_TABLE_LEN)
		data->rt_calib_valid_cnt = SPRD_FGU_RT_CALIB_INFO_TABLE_LEN;
}

static void sprd_fgu_get_full_calib_info(struct sprd_fgu_data *data, int uuuah1, int uuuah)
{
	struct sprd_fgu_vbat_info *vbat_info = &data->vbat_info;
	struct sprd_fgu_rt_full_calib_info *calib_info =
		&data->full_calib_info[data->rt_full_calib_index];
	int soc;

	calib_info->calib_ocv_soc = sprd_fgu_get_soc_by_ocv(data, vbat_info->ocv_uv,
							    data->bat_temp);
	calib_info->work_times = max(data->work_enter_times - data->work_exit_times, 1);
	calib_info->calib_ocv_uv = vbat_info->ocv_uv;
	calib_info->vbat_avg_mv = vbat_info->vbat_avg_mv;
	calib_info->cur_avg_ma = vbat_info->vbat_cur_avg_ma;
	calib_info->work_cur_avg_ma = data->ibat_avg_work_cycle_ma;
	calib_info->batt_soc = data->batt_soc;

	calib_info->calib_ocv_uah = data->batt_fcc / 1000 * calib_info->calib_ocv_soc;

	if ((data->batt_fcc - uuuah + data->delta_cc_uah) > 0)
		soc = DIV_ROUND_CLOSEST((calib_info->calib_ocv_uah - uuuah1 +
					 data->delta_cc_uah) * 10,
					(data->batt_fcc - uuuah + data->delta_cc_uah) / 100);
	else
		soc = 0;

	soc = clamp(soc, 0, 1000);
	calib_info->batt_cali_ocv_soc = soc;
	dev_info(data->dev, "calib_ocv_soc = %d, work_times = %d, calib_ocv_uv = %d, vbat_avg_mv = %d, cur_avg_ma = %d, work_cur_avg_ma = %d, batt_soc = %d, batt_cali_ocv_soc = %d, calib_ocv_uah = %d\n",
		 calib_info->calib_ocv_soc, calib_info->work_times, calib_info->calib_ocv_uv,
		 calib_info->vbat_avg_mv, calib_info->cur_avg_ma, calib_info->work_cur_avg_ma,
		 calib_info->batt_soc, calib_info->batt_cali_ocv_soc, calib_info->calib_ocv_uah);

	data->rt_full_calib_index++;
	data->rt_full_calib_valid_cnt++;
	if (data->rt_full_calib_index >= SPRD_FGU_RT_FULL_CALIB_INFO_TABLE_LEN)
		data->rt_full_calib_index = 0;
	if (data->rt_full_calib_valid_cnt >= SPRD_FGU_RT_FULL_CALIB_INFO_TABLE_LEN)
		data->rt_full_calib_valid_cnt = SPRD_FGU_RT_FULL_CALIB_INFO_TABLE_LEN;
}

static void sprd_fgu_get_dischg_calib_info(struct sprd_fgu_data *data)
{
	struct sprd_fgu_adj_uuibat_dischg_calib_info *calib_info = &data->dischg_calib_info[data->dischg_calib_index];

	calib_info->work_times = max(data->work_enter_times - data->work_exit_times, 1);
	calib_info->work_cur_avg_ma = data->ibat_avg_work_cycle_ma;

	data->dischg_calib_index++;
	data->dischg_calib_valid_cnt++;
	if (data->dischg_calib_index >= SPRD_FGU_RT_CALIB_INFO_TABLE_LEN)
		data->dischg_calib_index = 0;
	if (data->dischg_calib_valid_cnt >= SPRD_FGU_RT_CALIB_INFO_TABLE_LEN)
		data->dischg_calib_valid_cnt = SPRD_FGU_RT_CALIB_INFO_TABLE_LEN;
}

static int sprd_fgu_get_dischg_info(struct sprd_fgu_data *data)
{
	struct timespec64 cur_time;
	s64 stop_charge_time;

	cur_time = ktime_to_timespec64(ktime_get_boottime());
	stop_charge_time = cur_time.tv_sec - data->stop_charge_times;

	if ((stop_charge_time < SPRD_FGU_ADJ_UUIBAT_VALLID_ENTER_TIME) ||
		data->chg_sts == POWER_SUPPLY_STATUS_CHARGING
		// || data->bat_temp > 0
		) {
		data->dischg_calib_index = 0;
		data->dischg_calib_valid_cnt = 0;
		return 1;
	}

	sprd_fgu_get_dischg_calib_info(data);

	return 0;
}

static void sprd_fgu_get_dischg_calib_info_avg_cur(struct sprd_fgu_data *data, int *cur1_ma)
{
	int total_time = 0, total_cur_ma = 0, cnt = 0, valid_count = 0;
	int cur1_avg_ma = SPRD_FGU_MAGIC_NUMBER;

	int last_calib_info_index = (data->dischg_calib_index - 1 < 0) ?
		SPRD_FGU_RT_CALIB_INFO_TABLE_LEN - 1 : data->dischg_calib_index - 1;
	struct sprd_fgu_adj_uuibat_dischg_calib_info *calib_info = &data->dischg_calib_info[last_calib_info_index];

	if (data->dischg_calib_valid_cnt >= SPRD_FGU_RT_CALIB_INFO_TABLE_LEN)
		valid_count = SPRD_FGU_RT_CALIB_INFO_TABLE_LEN;
	else
		valid_count = data->dischg_calib_index;

	do {
		total_time += calib_info->work_times;
		total_cur_ma += calib_info->work_cur_avg_ma * calib_info->work_times;

		last_calib_info_index--;

		last_calib_info_index = (last_calib_info_index < 0) ?
				SPRD_FGU_RT_CALIB_INFO_TABLE_LEN - 1 : last_calib_info_index;

		calib_info = &data->dischg_calib_info[last_calib_info_index];

		cnt++;
		if (total_time >= SPRD_FGU_ADJ_UUIBAT_CUR_TIME && cur1_avg_ma == SPRD_FGU_MAGIC_NUMBER)
			cur1_avg_ma = total_cur_ma / total_time;
	} while (cnt < valid_count && total_time < SPRD_FGU_ADJ_UUIBAT_CUR_TIME);

	if (cur1_avg_ma)
		*cur1_ma = cur1_avg_ma;

	dev_info(data->dev, "%s dischg calib: total_time = %d, total_cur_ma = %d, cur1_avg_ma = %d,  cnt = %d, valid_count = %d!!!\n",
		 __func__, total_time, total_cur_ma, cur1_avg_ma, cnt, valid_count);
}

static void sprd_fgu_get_calib_info_avg_cur(struct sprd_fgu_data *data, int *cur1_ma,
					    int *cur2_ma, int *cur3_ma)
{
	int total_time = 0, total_cur_ma = 0, cnt = 0, valid_count = 0;
	int cur1_avg_ma = SPRD_FGU_MAGIC_NUMBER, cur2_avg_ma = SPRD_FGU_MAGIC_NUMBER;
	int cur3_avg_ma = SPRD_FGU_MAGIC_NUMBER;

	int last_calib_info_index = (data->rt_calib_index - 1 < 0) ?
		SPRD_FGU_RT_CALIB_INFO_TABLE_LEN - 1 : data->rt_calib_index - 1;
	struct sprd_fgu_rt_calib_info *calib_info = &data->calib_info[last_calib_info_index];

	if (data->rt_calib_valid_cnt >= SPRD_FGU_RT_CALIB_INFO_TABLE_LEN)
		valid_count = SPRD_FGU_RT_CALIB_INFO_TABLE_LEN;
	else
		valid_count = data->rt_calib_index;

	do {
		total_time += calib_info->work_times;
		total_cur_ma += calib_info->work_cur_avg_ma * calib_info->work_times;

		last_calib_info_index--;

		last_calib_info_index = (last_calib_info_index < 0) ?
				SPRD_FGU_RT_CALIB_INFO_TABLE_LEN - 1 : last_calib_info_index;

		calib_info = &data->calib_info[last_calib_info_index];

		cnt++;
		if (total_time >= SPRD_FGU_RT_CALIB_CUR1_TIME &&
		    cur1_avg_ma == SPRD_FGU_MAGIC_NUMBER)
			cur1_avg_ma = total_cur_ma / total_time;

		if (total_time >= SPRD_FGU_RT_CALIB_CUR2_TIME &&
		    cur2_avg_ma == SPRD_FGU_MAGIC_NUMBER)
			cur2_avg_ma = total_cur_ma / total_time;

		if (total_time >= SPRD_FGU_RT_CALIB_CUR3_TIME &&
		    cur3_avg_ma == SPRD_FGU_MAGIC_NUMBER)
			cur3_avg_ma = total_cur_ma / total_time;
	} while (cnt < valid_count && total_time < SPRD_FGU_RT_CALIB_CUR3_TIME);

	if (cur1_avg_ma)
		*cur1_ma = cur1_avg_ma;
	if (cur2_avg_ma)
		*cur2_ma = cur2_avg_ma;
	if (cur3_avg_ma)
		*cur3_ma = cur3_avg_ma;

	dev_info(data->dev, "%s RT calib: total_time = %d, total_cur_ma = %d, cur1_avg_ma = %d, cur2_avg_ma = %d, cur3_avg_ma = %d,  cnt = %d, valid_count = %d!!!\n",
		 __func__, total_time, total_cur_ma, cur1_avg_ma, cur2_avg_ma,
		 cur3_avg_ma, cnt, valid_count);
}

static int sprd_fgu_get_rt_calib_ratio(struct sprd_fgu_data *data, int cur_ma,
				       int cur_time, int *delta_ma)
{
	int total_time = 0, cnt = 0, valid_count = 0, work_times = 0, acc_time = 0;
	int ratio = 0, temp_ratio = 0, delta_ratio = 15, delta_cur_ma = 100;
	int delta_temp_ma, work_cycle_ma;

	int last_calib_info_index = (data->rt_calib_index - 1 < 0) ?
		SPRD_FGU_RT_CALIB_INFO_TABLE_LEN - 1 : data->rt_calib_index - 1;
	struct sprd_fgu_rt_calib_info *calib_info = &data->calib_info[last_calib_info_index];

	if (cur_ma == SPRD_FGU_MAGIC_NUMBER)
		return ratio;

	if (cur_ma < 0) {
		delta_cur_ma = -100;
		delta_cur_ma = min(delta_cur_ma, cur_ma * delta_ratio / 100);
		delta_temp_ma = cur_ma - delta_cur_ma;
		if (delta_temp_ma > 0)
			delta_temp_ma = 0;
	} else {
		delta_cur_ma = max(delta_cur_ma, cur_ma * delta_ratio / 100);
		delta_temp_ma = cur_ma - delta_cur_ma;
		if (delta_temp_ma < 0)
			delta_temp_ma = 0;
	}

	*delta_ma = delta_cur_ma;

	if (data->rt_calib_valid_cnt >= SPRD_FGU_RT_CALIB_INFO_TABLE_LEN)
		valid_count = SPRD_FGU_RT_CALIB_INFO_TABLE_LEN;
	else
		valid_count = data->rt_calib_index;

	do {
		total_time += calib_info->work_times;
		work_times = calib_info->work_times;
		work_cycle_ma = calib_info->work_cur_avg_ma;

		last_calib_info_index--;

		last_calib_info_index = (last_calib_info_index < 0) ?
				SPRD_FGU_RT_CALIB_INFO_TABLE_LEN - 1 : last_calib_info_index;

		calib_info = &data->calib_info[last_calib_info_index];

		cnt++;

		if (abs(work_cycle_ma) <= abs(cur_ma + delta_cur_ma) &&
		    abs(work_cycle_ma) >= abs(delta_temp_ma))
			acc_time += work_times;

		if (total_time >= cur_time)
			temp_ratio = acc_time * 100 / total_time;

	} while (cnt < valid_count && total_time < cur_time);

	if (temp_ratio)
		ratio = temp_ratio;

	dev_info(data->dev, "%s RT calib: total_time = %d, ratio = %d, temp_ratio = %d, acc_time = %d\n",
		 __func__, total_time, ratio, temp_ratio, acc_time);
	return ratio;
}

static int sprd_fgu_get_dischg_calib_ratio(struct sprd_fgu_data *data, int cur_ma, int cur_time)
{
	int total_time = 0, cnt = 0, valid_count = 0, work_times = 0, acc_time = 0;
	int ratio = 0, temp_ratio = 0, delta_cur_ma = -300, work_cycle_ma;

	int last_calib_info_index = (data->dischg_calib_index - 1 < 0) ?
		SPRD_FGU_RT_CALIB_INFO_TABLE_LEN - 1 : data->dischg_calib_index - 1;
	struct sprd_fgu_adj_uuibat_dischg_calib_info *calib_info = &data->dischg_calib_info[last_calib_info_index];

	if (cur_ma == SPRD_FGU_MAGIC_NUMBER)
		return ratio;

	if (data->dischg_calib_valid_cnt >= SPRD_FGU_RT_CALIB_INFO_TABLE_LEN)
		valid_count = SPRD_FGU_RT_CALIB_INFO_TABLE_LEN;
	else
		valid_count = data->dischg_calib_index;

	do {
		total_time += calib_info->work_times;
		work_times = calib_info->work_times;
		work_cycle_ma = calib_info->work_cur_avg_ma ;

		last_calib_info_index--;

		last_calib_info_index = (last_calib_info_index < 0) ?
				SPRD_FGU_RT_CALIB_INFO_TABLE_LEN - 1 : last_calib_info_index ;

		calib_info = &data->dischg_calib_info[last_calib_info_index];

		cnt++;

		if (work_cycle_ma > (cur_ma + delta_cur_ma) && work_cycle_ma < (cur_ma - delta_cur_ma))
			acc_time += work_times;

		if (total_time >= cur_time)
			temp_ratio = acc_time * 100 / total_time;

	} while (cnt < valid_count && total_time < cur_time);

	if (temp_ratio)
		ratio = temp_ratio;

	dev_info(data->dev, "%s RT calib: total_time = %d, ratio = %d, temp_ratio = %d, acc_time = %d\n", __func__, total_time, ratio, temp_ratio, acc_time);
	return ratio;
}

static void sprd_fgu_rt_calib(struct sprd_fgu_data *data, int delta_ocv_thd_mv,
			      int delta_cur_ma, int cur_ma)
{
	int ocv[SPRD_FGU_RT_CALIB_OCV_VALID_CNT], cur[SPRD_FGU_RT_CALIB_OCV_VALID_CNT];
	int rm_ocv_uv, calib_ocv_uv, delta_ocv_uv[SPRD_FGU_RT_CALIB_OCV_VALID_CNT], calib_cur_ma;
	int cnt = 0, valid_count = 0, ocv_valid_cnt = SPRD_FGU_RT_CALIB_OCV_VALID_CNT;
	int last_calib_info_index = (data->rt_calib_index - 1 < 0) ?
		SPRD_FGU_RT_CALIB_INFO_TABLE_LEN - 1 : data->rt_calib_index - 1;
	int ocv_avg = 0, cur_avg = 0, delta_ocv_avg = 0, i, new_rm_ocv_uv, fcc_mah;
	int rm_soc, new_rm_soc;
	int delta_ocv_thd_uv = delta_ocv_thd_mv * 1000, delta_temp_ma, delta_temp_avg_ma;
	bool rm_ocv_is_low, calib_ocv_is_low;
	struct sprd_fgu_rt_calib_info *calib_info = &data->calib_info[last_calib_info_index];

	if (data->rt_calib_valid_cnt >= SPRD_FGU_RT_CALIB_INFO_TABLE_LEN)
		valid_count = SPRD_FGU_RT_CALIB_INFO_TABLE_LEN;
	else
		valid_count = data->rt_calib_index;

	if (valid_count < ocv_valid_cnt)
		return;

	rm_ocv_uv = calib_info->rm_ocv_uv;

	calib_ocv_uv = calib_info->calib_ocv_uv;
	calib_cur_ma = calib_info->cur_avg_ma;

	do {
		ocv[cnt] = calib_info->calib_ocv_uv;
		cur[cnt] = calib_info->work_cur_avg_ma;
		delta_ocv_uv[cnt] = calib_info->delta_ocv_uv;

		last_calib_info_index--;

		last_calib_info_index = (last_calib_info_index < 0) ?
				SPRD_FGU_RT_CALIB_INFO_TABLE_LEN - 1 : last_calib_info_index;

		calib_info = &data->calib_info[last_calib_info_index];

		cnt++;
	} while (cnt < ocv_valid_cnt);


	for (i = 0; i < ocv_valid_cnt; i++) {
		ocv_avg += ocv[i];
		cur_avg += cur[i];
		delta_ocv_avg += delta_ocv_uv[i];
	}

	ocv_avg /= ocv_valid_cnt;
	cur_avg /= ocv_valid_cnt;
	delta_ocv_avg /= ocv_valid_cnt;
	delta_temp_avg_ma = cur_avg - delta_cur_ma;
	delta_temp_ma = cur_ma - delta_cur_ma;
	if (delta_cur_ma < 0) {
		if (delta_temp_ma > 0)
			delta_temp_ma = 0;
		if (delta_temp_avg_ma > 0)
			delta_temp_avg_ma = 0;
	} else {
		if (delta_temp_ma < 0)
			delta_temp_ma = 0;
		if (delta_temp_avg_ma < 0)
			delta_temp_avg_ma = 0;
	}

	for (i = 0; i < ocv_valid_cnt; i++) {
		if (abs(cur[i]) > abs(cur_avg + delta_cur_ma) ||
		    abs(cur[i]) < abs(delta_temp_avg_ma))
			return;
		if (abs(ocv[i] - ocv_avg) / 1000 > 20)
			return;
		if (abs(cur[i]) > abs(cur_ma + delta_cur_ma) ||
		    abs(cur[i]) < abs(delta_temp_ma))
			return;
	}

	if (abs(delta_ocv_avg) <= delta_ocv_thd_uv ||
	   abs(calib_ocv_uv - rm_ocv_uv) <= delta_ocv_thd_uv)
		return;

	if (abs(calib_cur_ma) > abs(cur_ma + delta_cur_ma) ||
	    abs(calib_cur_ma) < abs(delta_temp_ma))
		return;

	rm_ocv_is_low = sprd_fgu_is_in_low_energy_dens(data, rm_ocv_uv);
	calib_ocv_is_low = sprd_fgu_is_in_low_energy_dens(data, calib_ocv_uv);

	if ((!rm_ocv_is_low && !calib_ocv_is_low) || !calib_ocv_is_low) {
		delta_ocv_thd_uv = delta_ocv_thd_uv * 3 / 2;
	} else if (!rm_ocv_is_low) {
		delta_ocv_thd_uv = delta_ocv_thd_uv + (15000);
		if (data->bat_temp < SPRD_FGU_RT_CALI_TEMP_15_THD)
			delta_ocv_thd_uv = delta_ocv_thd_uv + (30000);
	}

	if (calib_ocv_uv > rm_ocv_uv + delta_ocv_thd_uv)
		new_rm_ocv_uv = calib_ocv_uv - delta_ocv_thd_uv;
	else if (calib_ocv_uv + delta_ocv_thd_uv < rm_ocv_uv)
		new_rm_ocv_uv = calib_ocv_uv + delta_ocv_thd_uv;
	else
		return;

	rm_soc = sprd_fgu_get_soc_by_ocv(data, rm_ocv_uv, data->bat_temp);
	new_rm_soc = sprd_fgu_get_soc_by_ocv(data, new_rm_ocv_uv, data->bat_temp);
	if (rm_soc == new_rm_soc)
		return;

	fcc_mah = data->batt_fcc / 1000;

	/* 0% - 100% => 0% - 1000% */
	data->rt_calib_vaild = true;
	data->err_rt_cc_uah += (rm_soc - new_rm_soc) * fcc_mah;
	dev_info(data->dev, "rm_soc = %d, rm_ocv_uv = %d, new_rm_soc = %d, new_rm_ocv_uv = %d, fcc_mah = %d, err_rt_cc_uah = %d, delta_ocv_thd_uv = %d, calib_ocv_uv = %d, calib_cur_ma = %d\n",
		 rm_soc, rm_ocv_uv, new_rm_soc, new_rm_ocv_uv, fcc_mah,
		 data->err_rt_cc_uah, delta_ocv_thd_uv, calib_ocv_uv, calib_cur_ma);
}

static int sprd_fgu_get_calib_delta_ocv_thd(struct sprd_fgu_data *data, int cur1_ma,
					    int cur2_ma, int cur3_ma, int ratio1,
					    int ratio2, int ratio3, int delta1_ma)
{
	int calib_delta_ocv_thd_mv = 0, match = 0, match2 = 0, match3 = 0, match4 = 0;
	int delta_temp_ma;

	delta_temp_ma = cur1_ma - delta1_ma;
	if (delta1_ma < 0) {
		if (delta_temp_ma > 0)
			delta_temp_ma = 0;
	} else {
		if (delta_temp_ma < 0)
			delta_temp_ma = 0;
	}

	if (abs(cur3_ma) > abs(delta_temp_ma) && abs(cur3_ma) < abs(cur1_ma + delta1_ma))
		match = 1;
	if (abs(cur2_ma) > abs(delta_temp_ma) && abs(cur2_ma) < abs(cur1_ma + delta1_ma))
		match2 = 1;
	if (data->bat_temp < SPRD_FGU_RT_CALI_TEMP_15_THD)
		match3 = 1;
	if (abs(cur3_ma) < 100 && ratio3 >= 80)
		match4 = 1;

	if (abs(cur1_ma) < 100 && abs(cur2_ma) < 100 && ratio1 >= 90 && ratio2 >= 85) {
		calib_delta_ocv_thd_mv = 40;
		if (match4)
			calib_delta_ocv_thd_mv = 30;
		if (match3) {
			calib_delta_ocv_thd_mv = 50;
			if (match4)
				calib_delta_ocv_thd_mv = 40;
		}
	} else if (abs(cur1_ma) < 200 && abs(cur2_ma) < 200 && ratio1 >= 90 && ratio2 >= 85) {
		calib_delta_ocv_thd_mv = 40;
		if (match)
			calib_delta_ocv_thd_mv = 30;
		if (match3) {
			calib_delta_ocv_thd_mv = 50;
			if (match)
				calib_delta_ocv_thd_mv = 40;
		}
	} else if (abs(cur1_ma) < 400 && abs(cur2_ma) < 400 &&
		   match2 && ratio1 >= 90 && ratio2 >= 85) {
		calib_delta_ocv_thd_mv = 50;
		if (match)
			calib_delta_ocv_thd_mv = 40;
		if (match3) {
			calib_delta_ocv_thd_mv = 60;
			if (match)
				calib_delta_ocv_thd_mv = 50;
		}
	} else if (abs(cur1_ma) < 600 && abs(cur2_ma) < 600 &&
		   match2 && ratio1 >= 90 && ratio2 >= 85) {
		calib_delta_ocv_thd_mv = 50;
		if (match)
			calib_delta_ocv_thd_mv = 40;
		if (match3) {
			calib_delta_ocv_thd_mv = 60;
			if (match)
				calib_delta_ocv_thd_mv = 50;
		}
	} else if (abs(cur1_ma) < 1000 && abs(cur2_ma) < 1000 &&
		   match2 && ratio1 >= 90 && ratio2 >= 85) {
		calib_delta_ocv_thd_mv = 60;
		if (match)
			calib_delta_ocv_thd_mv = 50;
		if (match3)
			calib_delta_ocv_thd_mv = 150;
	} else if (abs(cur1_ma) < SPRD_FGU_RT_CALIB_CUR_THD_MA &&
		   abs(cur2_ma) < SPRD_FGU_RT_CALIB_CUR_THD_MA &&
		   match2 && ratio1 >= 90 && ratio2 >= 85) {
		calib_delta_ocv_thd_mv = 70;
		if (match)
			calib_delta_ocv_thd_mv = 60;
		if (match3)
			calib_delta_ocv_thd_mv = 150;
	} else if (abs(cur1_ma) < SPRD_FGU_RT_CALIB_CHG_CUR_THD_MA &&
		   abs(cur2_ma) < SPRD_FGU_RT_CALIB_CHG_CUR_THD_MA &&
		   match2 && ratio1 >= 90 && ratio2 >= 85) {
		calib_delta_ocv_thd_mv = 70;
		if (match)
			calib_delta_ocv_thd_mv = 60;
		if (match3)
			calib_delta_ocv_thd_mv = 200;
	}

	return calib_delta_ocv_thd_mv;
}

static void sprd_fgu_adjust_rm_soc(struct sprd_fgu_data *data)
{
	int rm_ocv_uv, calib_ocv_soc, calc_rbat;
	struct sprd_fgu_vbat_info *vbat_info = &data->vbat_info;
	int cur1_ma, cur2_ma, cur3_ma;
	int delta1_ma = 0, ratio1 = 0, ratio2 = 0, ratio3 = 0, calib_delta_ocv_thd_mv = 0;
	int vbat_ocv_soc = 0;

	calib_ocv_soc = sprd_fgu_get_soc_by_ocv(data, vbat_info->ocv_uv, data->bat_temp);
	rm_ocv_uv = sprd_fgu_get_ocv_by_soc(data, data->bat_temp, data->batt_rm_soc, true);
	vbat_ocv_soc = sprd_fgu_get_soc_by_ocv(data, vbat_info->vbat_avg_mv * 1000,
					       data->bat_temp);
	calc_rbat = DIV_ROUND_CLOSEST(data->vbat_info.vbat_avg_mv * 1000 - rm_ocv_uv,
		data->vbat_info.vbat_cur_avg_ma);

	dev_info(data->dev, "calib_ocv_uv = %d, calib_ocv_soc = %d, rm_ocv_uv = %d, batt_rm_soc = %d, vbat_cur_ma = %d, vbat_avg_mv = %d, vbat_ocv_soc = %d, bat_temp = %d, calc_rbat = %d\n",
		 vbat_info->ocv_uv, calib_ocv_soc, rm_ocv_uv, data->batt_rm_soc,
		 vbat_info->vbat_cur_avg_ma, vbat_info->vbat_avg_mv, vbat_ocv_soc,
		 data->bat_temp, calc_rbat);

	if (data->rt_calib_start_time == 0 || data->chg_sts != data->last_chg_sts) {
		data->rt_calib_start_time = data->work_enter_times;
		data->rt_calib_index = 0;
		data->rt_calib_valid_cnt = 0;
	}

	data->last_chg_sts = data->chg_sts;

	if (data->chg_sts == POWER_SUPPLY_STATUS_CHARGING &&
	    vbat_info->vbat_cur_avg_ma > 10 && vbat_info->vbat_cur_ma > 10 &&
	    data->ibat_avg_work_cycle_ma > 10) {
		if (!data->start_charge_times)
			data->start_charge_times = data->work_enter_times;
	} else {
		data->start_charge_times = 0;
	}

	dev_info(data->dev, "batt_rm_soc = %d, chg_sts = %d, vbat_cur_avg_ma = %d, work_enter_times = %lld, stop_charge_times = %lld, start_charge_times = %lld\n",
		 data->batt_rm_soc, data->chg_sts, vbat_info->vbat_cur_avg_ma,
		 data->work_enter_times, data->stop_charge_times, data->start_charge_times);

	if (data->chg_sts == POWER_SUPPLY_STATUS_DISCHARGING && vbat_info->vbat_cur_avg_ma < 0 &&
	    vbat_info->vbat_cur_ma < 0 && data->ibat_avg_work_cycle_ma < 0 &&
	    data->work_enter_times - data->stop_charge_times >= SPRD_FGU_SR_STOP_CHARGE_TIMES) {
		vbat_ocv_soc = sprd_fgu_get_soc_by_ocv(data, (vbat_info->vbat_avg_mv - 20) * 1000,
						       data->bat_temp);
		if (data->batt_rm_soc < vbat_ocv_soc) {
			/* 0% - 100% => 0% - 1000% */
			data->rt_calib_vaild = true;
			data->err_rt_cc_uah += (data->batt_rm_soc - vbat_ocv_soc) *
				(data->batt_fcc / 1000);
			dev_info(data->dev, "dischg_vbat_ocv_soc = %d, batt_rm_soc = %d, fcc_uah = %d, err_rt_cc_uah = %d\n",
				 vbat_ocv_soc, data->batt_rm_soc, data->batt_fcc,
				 data->err_rt_cc_uah);
			return;
		}
	}

	if (data->chg_sts == POWER_SUPPLY_STATUS_CHARGING && vbat_info->vbat_cur_avg_ma > 0 &&
	    vbat_info->vbat_cur_ma > 0 && data->ibat_avg_work_cycle_ma > 0 &&
	    data->start_charge_times != 0 && data->work_enter_times - data->start_charge_times >=
	    SPRD_FGU_ABS_START_CHARGE_TIMES) {
		vbat_ocv_soc = sprd_fgu_get_soc_by_ocv(data, (vbat_info->vbat_avg_mv + 20) * 1000,
						       data->bat_temp);
		if (data->batt_rm_soc > vbat_ocv_soc) {
			/* 0% - 100% => 0% - 1000% */
			data->rt_calib_vaild = true;
			data->err_rt_cc_uah += (data->batt_rm_soc - vbat_ocv_soc) *
				(data->batt_fcc / 1000);
			dev_info(data->dev, "chg_vbat_ocv_soc = %d, batt_rm_soc = %d, fcc_uah = %d, err_rt_cc_uah = %d\n",
				 vbat_ocv_soc, data->batt_rm_soc, data->batt_fcc,
				 data->err_rt_cc_uah);
			return;
		}
	}

	if ((data->chg_sts == POWER_SUPPLY_STATUS_DISCHARGING &&
	     data->bat_temp < SPRD_FGU_RT_CALI_TEMP_0_THD) ||
	    (data->chg_sts == POWER_SUPPLY_STATUS_CHARGING &&
	     data->bat_temp < SPRD_FGU_RT_CALI_TEMP_15_THD)) {
		data->rt_calib_index = 0;
		data->rt_calib_valid_cnt = 0;
		return;
	}

	if (data->chg_sts == POWER_SUPPLY_STATUS_DISCHARGING &&
	    (data->work_enter_times - data->rt_calib_start_time) < 600)
		return;

	if (data->chg_sts == POWER_SUPPLY_STATUS_CHARGING &&
	    ((data->work_enter_times - data->rt_calib_start_time) < 500 ||
	     data->ibat_avg_work_cycle_ma < 0))
		return;

	sprd_fgu_get_calib_info(data);

	if ((data->cali_ocv_times != 0 &&
	     data->work_enter_times - data->cali_ocv_times < (60 * 60 * 3)) ||
	    (data->batt_ocv_times != 0 &&
	     data->work_enter_times - data->batt_ocv_times < (60 * 60 * 5)))
		return;

	sprd_fgu_get_calib_info_avg_cur(data, &cur1_ma, &cur2_ma, &cur3_ma);

	if ((cur1_ma > 0 && cur1_ma > SPRD_FGU_RT_CALIB_CHG_CUR_THD_MA) ||
	    (cur1_ma < 0 && cur1_ma < SPRD_FGU_RT_CALIB_DIS_CHG_CUR_THD_MA) ||
	    cur1_ma == SPRD_FGU_MAGIC_NUMBER)
		return;

	if ((cur2_ma > 0 && cur2_ma > SPRD_FGU_RT_CALIB_CHG_CUR_THD_MA) ||
	    (cur2_ma < 0 && cur2_ma < SPRD_FGU_RT_CALIB_DIS_CHG_CUR_THD_MA) ||
	    cur2_ma == SPRD_FGU_MAGIC_NUMBER)
		return;

	ratio3 = sprd_fgu_get_rt_calib_ratio(data, cur3_ma,
					     SPRD_FGU_RT_CALIB_CUR3_TIME, &delta1_ma);
	ratio2 = sprd_fgu_get_rt_calib_ratio(data, cur2_ma,
					     SPRD_FGU_RT_CALIB_CUR2_TIME, &delta1_ma);
	ratio1 = sprd_fgu_get_rt_calib_ratio(data, cur1_ma,
					     SPRD_FGU_RT_CALIB_CUR1_TIME, &delta1_ma);

	calib_delta_ocv_thd_mv = sprd_fgu_get_calib_delta_ocv_thd(data, cur1_ma,
								  cur2_ma, cur3_ma,
								  ratio1, ratio2,
								  ratio3, delta1_ma);

	dev_info(data->dev, "cur1_ma = %d, cur2_ma = %d, cur3_ma = %d, calib_delta_ocv_thd_mv = %d, ratio1 = %d, ratio2 = %d, ratio3 = %d, delta_ma = %d\n",
		 cur1_ma, cur2_ma, cur3_ma, calib_delta_ocv_thd_mv,
		 ratio1, ratio2, ratio3, delta1_ma);

	if (calib_delta_ocv_thd_mv != 0)
		sprd_fgu_rt_calib(data, calib_delta_ocv_thd_mv, delta1_ma, cur1_ma);
}

static int sprd_fgu_calc_rm_uah(struct sprd_fgu_data *data)
{
	int soc, cc_uah = 0, rm_uah, ret, batt_rm_soc, batt_rm_adj_soc, fcc_mah;
	int fcc_uah, default_fcc_uah, ratio, res = 50;
	int tbat_ocv_uv, tbat_fcc_uah, ocv_tbat_fcc_uah, tbat_delta_fcc_uah, ocv_soc;

	data->batt_fcc = sprd_fgu_get_fcc_uah(data, data->bat_temp, data->charge_cycle);

	/* 0% - 100% => 0% - 1000% */
	soc = sprd_fgu_get_soc_by_ocv(data, data->batt_ocv_uv, data->bat_temp);

	ret = sprd_fgu_get_ccuah(data, &cc_uah);
	if (ret < 0) {
		dev_err(data->dev, "Failed to get cc uah, ret = %d\n", ret);
		return ret;
	}

	/* TODO: 1000 uah ? */
	if (abs(cc_uah - data->batt_cc_uah) > 1000) {
		sprd_fgu_save_last_cc_uah(data, cc_uah);
		data->batt_cc_uah = cc_uah;
	}

	fcc_mah = data->batt_fcc / 1000;
	fcc_uah = data->batt_fcc % 1000;
	rm_uah = soc * fcc_mah + DIV_ROUND_CLOSEST((soc * fcc_uah), 1000) + cc_uah;
	if (data->batt_ocv_temp > data->bat_temp) {
		tbat_ocv_uv = sprd_fgu_get_ocv_by_soc(data, data->bat_temp, 0, false);
		if (tbat_ocv_uv > data->batt_ocv_uv) {
			ocv_soc = sprd_fgu_get_soc_by_ocv(data, data->batt_ocv_uv,
							  data->batt_ocv_temp);
			ocv_tbat_fcc_uah = sprd_fgu_get_fcc_uah(data, data->batt_ocv_temp,
								data->charge_cycle);
			tbat_fcc_uah = data->batt_fcc;
			tbat_delta_fcc_uah = ocv_tbat_fcc_uah - tbat_fcc_uah;
			fcc_mah = ocv_tbat_fcc_uah / 1000;
			fcc_uah = ocv_tbat_fcc_uah % 1000;
			rm_uah = ocv_soc * fcc_mah +
				DIV_ROUND_CLOSEST((ocv_soc * fcc_uah), 1000) +
				cc_uah - tbat_delta_fcc_uah;
			dev_info(data->dev, "batt_ocv_temp = %d, bat_temp = %d, tbat_ocv_uv = %d, batt_ocv_uv = %d, ocv_tbat_fcc_uah = %d, tbat_delta_fcc_uah = %d\n",
				 data->batt_ocv_temp, data->bat_temp, tbat_ocv_uv,
				 data->batt_ocv_uv, ocv_tbat_fcc_uah, tbat_delta_fcc_uah);
		}
	}

	data->err_cc_adj_en = false;

	if (rm_uah > data->batt_fcc) {
		data->err_cc_uah += (rm_uah - data->batt_fcc);
		rm_uah = data->batt_fcc;
	} else if (rm_uah < 0) {
		if (data->batt_ibat_avg_ma < 0 && data->vbat_info.vbat_cur_ma < 0 &&
		    data->ibat_avg_work_cycle_ma < 0 && data->vbat_info.vbat_cur_avg_ma < 0 &&
		    data->chg_sts != POWER_SUPPLY_STATUS_CHARGING &&
		    (data->vbat_info.zp_vol_uv / 1000) < data->vbat_info.vbat_avg_mv &&
		    (data->vbat_info.zp_vol_uv / 1000) < data->vbat_info.vbat_mv) {
			data->err_cc_uah += rm_uah;
			data->err_cc_adj_en = true;
			rm_uah = 0;
		}
	}

	data->delta_cc_uah = 0;

	if (data->bat_temp < 0 && data->batt_ibat_avg_ma > (-400) && data->batt_ibat_avg_ma < 0 &&
		data->chg_sts != POWER_SUPPLY_STATUS_CHARGING) {
		default_fcc_uah = sprd_fgu_get_fcc_uah(data, SPRD_FGU_TEMP_DEFAULT,
						       data->charge_cycle);
		ratio = ((400 + data->batt_ibat_avg_ma) / res) * 100 / (400 / res);
		data->delta_cc_uah = (default_fcc_uah - data->batt_fcc) * ratio / 100;
	}

	data->batt_rm_uah = rm_uah;

	batt_rm_soc = DIV_ROUND_CLOSEST(rm_uah * 10, data->batt_fcc / 100);

	batt_rm_adj_soc = DIV_ROUND_CLOSEST((rm_uah + data->delta_cc_uah) * 10,
					    (data->batt_fcc + data->delta_cc_uah) / 100);

	data->batt_raw_rm_soc = batt_rm_soc;

	data->batt_rm_soc = clamp(batt_rm_soc, 0, SPRD_FGU_FCC_PERCENT);

	sprd_fgu_adjust_rm_soc(data);

	dev_info(data->dev, "rm_soc = %d, raw_rm_soc = %d, soc_ocv = %d, rm_uah = %d, fcc = %d, cc_uah = %d, batt_ocv_uv = %d, tbat = %d, err_cc_uah = %d, delta_cc_uah = %d, batt_rm_adj_soc = %d\n",
		 data->batt_rm_soc, data->batt_raw_rm_soc, soc, rm_uah,
		 data->batt_fcc, cc_uah, data->batt_ocv_uv, data->bat_temp,
		 data->err_cc_uah, data->delta_cc_uah, batt_rm_adj_soc);

	return rm_uah;
}

static void sprd_fgu_adjust_fcc(struct sprd_fgu_data *data, int learned_mah, int temp)
{
	int ratio, cols, i;
	int best_fcc_mah;

	best_fcc_mah = sprd_fgu_get_fcc_uah_by_temp(data, temp) / 1000;

	/* kuoda 1000 bei */
	ratio = DIV_ROUND_CLOSEST(learned_mah * 1000, best_fcc_mah);
	if (ratio < 700) {
		dev_err(data->dev, "%s:ratio = %d, err!!!\n", __func__, ratio);
		ratio = 700;
	} else if(ratio > 1100) {
		ratio = 1100;
		dev_err(data->dev, "%s:ratio = %d, err!!!\n", __func__, ratio);
	}

	if (!data->battery_adjust_temp_fcc_lut) {
		data->battery_adjust_temp_fcc_lut =
			devm_kzalloc(data->dev, sizeof(struct sprd_temp_fcc_lut), GFP_KERNEL);
		if (!data->battery_adjust_temp_fcc_lut)
			return;
	}

	cols = data->battery_temp_fcc_lut->cols;
	data->battery_adjust_temp_fcc_lut->cols = cols;
	for (i = 0; i < cols; i++) {
		data->battery_adjust_temp_fcc_lut->fcc[i] =
			data->battery_temp_fcc_lut->fcc[i] / 1000 * ratio;
		data->battery_adjust_temp_fcc_lut->temp[i] =
			data->battery_temp_fcc_lut->temp[i];
	}
}

static int sprd_fgu_adjust_aging_fcc(struct sprd_fgu_data *data)
{
	int ratio = data->bat_fcc_aging_ratio, cols, i;

	if (ratio < 500 || ratio > 1100)
		dev_err(data->dev, "%s:ratio = %d, err!!!\n", __func__, ratio);

	if (!data->battery_adjust_temp_fcc_lut)
		return -EINVAL;

	cols = data->battery_temp_fcc_lut->cols;
	data->battery_adjust_temp_fcc_lut->cols = cols;
	for (i = 0; i < cols; i++) {
		data->battery_adjust_temp_fcc_lut->fcc[i] =
			data->battery_temp_fcc_lut->fcc[i] / 1000 * ratio;
		data->battery_adjust_temp_fcc_lut->temp[i] =
			data->battery_temp_fcc_lut->temp[i];
	}

	return 0;
}

static int sprd_fgu_get_zp_voltage_by_temp(struct sprd_fgu_data *data, int batt_tbat)
{
	int zero_point_voltage_uv = SPRD_FGU_DEFAULT_ZP_VOL_MV * 1000;

	if (!data->battery_temp_zp_vol_lut)
		return zero_point_voltage_uv;

	zero_point_voltage_uv = interpolate_zero_point_voltage(data, batt_tbat);

	return zero_point_voltage_uv;
}

static int sprd_fgu_get_abs_zp_voltage_by_temp(struct sprd_fgu_data *data, int batt_tbat)
{
	int abs_zero_point_voltage_uv = SPRD_FGU_DEFAULT_ZP_VOL_MV * 1000;

	if (!data->battery_temp_abs_zp_vol_lut)
		return -EINVAL;

	abs_zero_point_voltage_uv = interpolate_abs_zero_point_voltage(data, batt_tbat);

	return abs_zero_point_voltage_uv;
}

static int sprd_fgu_get_rbat_ratio(struct sprd_fgu_data *data, int batt_tbat, int soc, int cycles)
{
	int rbat_ratio0, rbat_ratio1, rbat_ratio = 1000;
	int cycle0, cycle1;
	int idx = 0;
	int cycle_thd = 10 * 1000;

	if (data->battery_cap_temp_resist_ratio_lut_len == 0)
		return 1000;

	for (idx = 0; idx < data->battery_cap_temp_resist_ratio_lut_len; idx++) {
		if (cycles < data->battery_cap_temp_resist_ratio_table[idx] * 1000)
			break;
	}

	if (idx == 0) {
		if (cycles < cycle_thd)
			return 1000;
		rbat_ratio0 = 1000;
		cycle0 = 0;
		cycle1 = data->battery_cap_temp_resist_ratio_table[idx] * 1000;
		rbat_ratio1 = interpolate_cap2resist_ratio(data, soc, batt_tbat, idx);
	} else if (idx == data->battery_cap_temp_resist_ratio_lut_len) {
		rbat_ratio1 = interpolate_cap2resist_ratio(data, soc, batt_tbat, idx - 1);
		rbat_ratio = rbat_ratio1;
		goto out;
	} else {
		cycle0 = data->battery_cap_temp_resist_ratio_table[idx - 1] * 1000;
		cycle1 = data->battery_cap_temp_resist_ratio_table[idx] * 1000;

		if (cycles < cycle0 + cycle_thd) {
			rbat_ratio1 = interpolate_cap2resist_ratio(data, soc, batt_tbat, idx - 1);
			return rbat_ratio1;
		} else if (cycles > cycle1 - cycle_thd) {
			rbat_ratio1 = interpolate_cap2resist_ratio(data, soc, batt_tbat, idx);
			return rbat_ratio1;
		}
		rbat_ratio0 = interpolate_cap2resist_ratio(data, soc, batt_tbat, idx - 1);
		rbat_ratio1 = interpolate_cap2resist_ratio(data, soc, batt_tbat, idx);
	}
	rbat_ratio = linear_interpolate(rbat_ratio0, cycle0, rbat_ratio1, cycle1, cycles);
out:
	return rbat_ratio;
}

static int sprd_fgu_get_rbat(struct sprd_fgu_data *data, int batt_tbat, int soc, int cycles)
{
	int rbat_moh, rbat_ratio_moh, rbat_ratio;

	rbat_moh = interpolate_cap2resist(data, soc, batt_tbat);
	rbat_ratio = sprd_fgu_get_rbat_ratio(data, soc, batt_tbat, cycles);

	rbat_ratio_moh = rbat_moh * rbat_ratio / 1000;

	dev_dbg(data->dev, "%s:rbat = %d, rbat_ratio = %d, rbat_ratio_moh = %d\n",
		__func__, rbat_moh, rbat_ratio, rbat_ratio_moh);

	return rbat_ratio_moh;
}

static int sprd_fgu_get_delta_soc_avg_value(struct sprd_fgu_data *data, int delta_soc)
{

	if (data->delta_soc_count < SPRD_FGU_DELTA_SOC_BUFF_SIZE) {
		data->delta_soc_value[data->delta_soc_index] = delta_soc;
		data->delta_soc_sum += delta_soc;
		data->delta_soc_count++;
	} else {
		data->delta_soc_sum -= data->delta_soc_value[data->delta_soc_index];
		data->delta_soc_value[data->delta_soc_index] = delta_soc;
		data->delta_soc_sum += delta_soc;
	}
	data->delta_soc_index = (data->delta_soc_index + 1) % SPRD_FGU_DELTA_SOC_BUFF_SIZE;
	delta_soc = data->delta_soc_sum / data->delta_soc_count;

	dev_info(data->dev, "%s: count = %d, index = %d, sum = %d\n",
		 __func__, data->delta_soc_count, data->delta_soc_index, data->delta_soc_sum);
	return delta_soc;
}

static void sprd_fgu_get_vbat_delta_uusoc(struct sprd_fgu_data *data, int raw_uusoc, int *soc)
{
	int ocv_uv, vbat_soc = 0, ratio = 0, delta_soc = 0, limit_delta_soc = 0;
	int uusoc_adj_thd = 20, delta_soc_diff_max = 50, delta_soc_setp = 20;
	int vbat_avg_mv = data->vbat_info.vbat_avg_mv_dy;

	if (data->work_cycle == SPRD_FGU_NORMAL_WORK_10S)
		delta_soc_setp = 10;
	else if (data->work_cycle == SPRD_FGU_QUICKEN_WORK_5S)
		delta_soc_setp = 5;

	delta_soc_diff_max = delta_soc_setp;

	if (vbat_avg_mv >= SPRD_FGU_UUOCV_VOLT_THRESHOLD)  {
		data->delta_soc_index = 0;
		data->delta_soc_sum = 0;
		data->delta_soc_count = 0;
		data->pre_delta_soc = SPRD_FGU_MAGIC_NUMBER;
		return;
	}

	ocv_uv = sprd_fgu_get_discharge_ocv(data, vbat_avg_mv * 1000, data->bat_temp,
					     data->batt_ibat_avg_ma, data->charge_cycle);
	vbat_soc = sprd_fgu_get_soc_by_ocv(data, ocv_uv, data->bat_temp);
	delta_soc = data->batt_raw_rm_soc - vbat_soc;
	delta_soc = sprd_fgu_get_delta_soc_avg_value(data, delta_soc);

	if (delta_soc > uusoc_adj_thd)
		delta_soc -= uusoc_adj_thd;
	else if (delta_soc < -uusoc_adj_thd)
		delta_soc += uusoc_adj_thd;
	else
		delta_soc = 0;

	if (data->pre_delta_soc == SPRD_FGU_MAGIC_NUMBER)  {
		if (delta_soc > delta_soc_setp)
			data->pre_delta_soc  = delta_soc_setp;
		else if (delta_soc < -delta_soc_setp)
			data->pre_delta_soc = -delta_soc_setp;
		else
			data->pre_delta_soc = delta_soc;
	} else if (abs(data->pre_delta_soc - delta_soc) < delta_soc_diff_max) {
		data->pre_delta_soc = delta_soc;
	} else if (abs(data->pre_delta_soc - delta_soc) >= delta_soc_diff_max) {
		if (delta_soc > data->pre_delta_soc)
			data->pre_delta_soc += delta_soc_setp;
		else
			data->pre_delta_soc -= delta_soc_setp;
	}
	limit_delta_soc = data->pre_delta_soc;

	/* expand 1000 times */
	ratio = DIV_ROUND_CLOSEST((vbat_avg_mv * 1000 - data->vbat_info.zp_vol_uv),
				   SPRD_FGU_UUOCV_VOLT_THRESHOLD -
				   data->vbat_info.zp_vol_uv / 1000);
	ratio = clamp(ratio, 0, 1000);

	/* reduce 1000 times */
	*soc = raw_uusoc + (1000 - ratio * ratio / 1000) * limit_delta_soc / 1000;

	dev_info(data->dev, "%s:soc = %d, raw_uusoc = %d, ratio = %d, delta_soc = %d, limit_delta_soc = %d, pre_delta_soc = %d, vbat_soc = %d, batt_raw_rm_soc = %d\n",
		 __func__, *soc, raw_uusoc, ratio, delta_soc, limit_delta_soc,
		 data->pre_delta_soc, vbat_soc, data->batt_raw_rm_soc);
}

static int sprd_fgu_get_uusoc_uah(struct sprd_fgu_data *data, int uucocv_uv,
				  int batt_tbat, int *uuuah1)
{
	struct sprd_fgu_vbat_info *vbat_info = &data->vbat_info;
	int uuuah, fcc_mah, fcc_uah, soc = 0;
	int uusoc_diff_max = 50;
	int uusoc_step = 20;
	int limit_uusoc, raw_uusoc = 0, new_uusoc = 0;
	int capacity_hc = data->capacity_hc;

	data->calc_capacity_hc = 0;
	data->chg_loss_soc = 0;
	if (data->chg_sts == POWER_SUPPLY_STATUS_CHARGING &&
	    (data->batt_ibat_avg_ma > 0 || data->vbat_info.vbat_cur_avg_ma > 0)) {
		uuuah = sprd_fgu_get_chg_loss_uah_by_temp(data, data->bat_temp);
		soc = sprd_fgu_get_chg_loss_soc_by_temp(data, data->bat_temp);
		data->chg_loss_soc = soc;
		*uuuah1 = 0;
		dev_info(data->dev, "%s:uusoc = %d, uuc_uah = %d\n", __func__, soc, uuuah);

		return uuuah;
	}

	soc = sprd_fgu_get_soc_by_ocv(data, (uucocv_uv + SPRD_FGU_UUOCV_OFFSET_VOL_UV), batt_tbat);
	raw_uusoc = soc;

	sprd_fgu_get_vbat_delta_uusoc(data, raw_uusoc, &soc);
	new_uusoc = soc;

	data->raw_uusoc = soc;
	data->raw_uuocv_uv = uucocv_uv;
	if (data->pre_uusoc == -EINVAL || abs(data->pre_uusoc - soc) < uusoc_diff_max) {
		data->pre_uusoc = soc;
	} else if (abs(data->pre_uusoc - soc) >= uusoc_diff_max) {
		if (soc > data->pre_uusoc)
			data->pre_uusoc += uusoc_step;
		else
			data->pre_uusoc -= uusoc_step;
	}

	limit_uusoc = data->pre_uusoc;
	if (data->chg_sts != POWER_SUPPLY_STATUS_CHARGING &&
	    vbat_info->vbat_cur_ma < 0 && vbat_info->vbat_cur_avg_ma < 0 &&
	    data->fgu_info->capacity_hc_enable) {
		capacity_hc = sprd_fgu_get_hc_by_temp(data, data->bat_temp);
		data->calc_capacity_hc = capacity_hc;
		limit_uusoc = limit_uusoc + capacity_hc;
	}

	fcc_mah = data->batt_fcc / 1000;
	fcc_uah = data->batt_fcc % 1000;
	uuuah = limit_uusoc * fcc_mah + DIV_ROUND_CLOSEST((limit_uusoc * fcc_uah), 1000);
	*uuuah1 = uuuah;

	dev_info(data->dev, "%s:new_uusoc = %d, raw_uusoc = %d, uuc_uah = %d, limit_uusoc = %d, capacity_hc = %d\n",
		 __func__, new_uusoc, raw_uusoc, uuuah, limit_uusoc, capacity_hc);

	return uuuah;
}

static int sprd_fgu_adjust_uuibat(struct sprd_fgu_data *data, int ibat_avg_ma)
{
	int cur_avg_ma = 0, ratio, ret, bat_id;

	ret = sprd_fgu_get_dischg_info(data);
	if (ret)
		return ibat_avg_ma;

	sprd_fgu_get_dischg_calib_info_avg_cur(data, &cur_avg_ma);
	if ((cur_avg_ma > SPRD_FGU_ADJUST_UUIBAT_CUR_THR) || (ibat_avg_ma > SPRD_FGU_ADJUST_UUIBAT_CUR_THR))
		return ibat_avg_ma;

	ratio = sprd_fgu_get_dischg_calib_ratio(data, cur_avg_ma,
						SPRD_FGU_ADJ_UUIBAT_CUR_TIME);

	bat_id = sprd_battery_parse_battery_id(data->battery);

	if (ratio < 50)
		return ibat_avg_ma;

	if (data->bat_temp >= 200)
		data->dischg_dynamic_zp_uv = 100000;
	else if (data->bat_temp >= 100)
		data->dischg_dynamic_zp_uv = 100000;
	else if (data->bat_temp >= 0)
		data->dischg_dynamic_zp_uv = 180000;
	else if (data->bat_temp >= -100){
		data->dischg_dynamic_zp_uv = 180000;
		if(bat_id == 3){
			data->dischg_dynamic_zp_uv = 100000;
		}
		dev_info(data->dev,"bat_id = %d", bat_id);
	}
	else
		data->dischg_dynamic_zp_uv = 180000;

	if(data->smooth_soc >= 250){
		ibat_avg_ma = ibat_avg_ma * SPRD_FGU_ADJUST_UUIBAT_RATIO / 100;

		if (ibat_avg_ma > SPRD_FGU_ADJUST_UUIBAT_CUR_THR)
			ibat_avg_ma = SPRD_FGU_ADJUST_UUIBAT_CUR_THR;
	}
	return ibat_avg_ma;
}

static int sprd_fgu_calc_uuuah(struct sprd_fgu_data *data, int *uuuah1)
{
	int uuuah, zero_point_uv, uucocv_uv, ibat_avg_ma;

	ibat_avg_ma = data->batt_ibat_avg_ma;

	zero_point_uv = data->vbat_info.zp_vol_uv;

	/* if we are charging use 200 ma ibat_avg to keep uuc */
	if (ibat_avg_ma > (-SPRD_FGU_CHARGING_IAVG_MA))
		ibat_avg_ma = -SPRD_FGU_CHARGING_IAVG_MA;

	data->dischg_dynamic_zp_uv = 0;
	if (SPRD_FGU_ADJUST_UUIBAT_RATIO)
		ibat_avg_ma = sprd_fgu_adjust_uuibat(data, ibat_avg_ma);

	if (data->chg_sts == POWER_SUPPLY_STATUS_DISCHARGING ||
	    (data->batt_ibat_avg_ma < 0 && data->vbat_info.vbat_cur_avg_ma < 0))
		ibat_avg_ma = ibat_avg_ma * data->vbat_info.vbat_avg_mv / (data->vbat_info.zp_vol_uv / 1000);

	data->vbat_info.zp_vol_uv -= data->dischg_dynamic_zp_uv;
	zero_point_uv = data->vbat_info.zp_vol_uv;

	uucocv_uv = sprd_fgu_get_discharge_ocv(data, zero_point_uv, data->bat_temp,
					       ibat_avg_ma, data->charge_cycle);
	uuuah = sprd_fgu_get_uusoc_uah(data, uucocv_uv, data->bat_temp, uuuah1);

	dev_info(data->dev, "%s:uuc_ocv = %d, uuc_uah = %d, ibat_avg = %d, zero_point = %d, dischg_dynamic_zp_uv = %d\n",
		 __func__, uucocv_uv, uuuah, ibat_avg_ma, zero_point_uv, data->dischg_dynamic_zp_uv);

	data->batt_uuuah = uuuah;

	return uuuah;
}

static int sprd_fgu_calc_batt_soc(struct sprd_fgu_data *data, int rm_uah)
{
	int soc, uuuah = 0, uuuah1;

	uuuah = sprd_fgu_calc_uuuah(data, &uuuah1);

	if ((data->batt_fcc - uuuah + data->delta_cc_uah) > 0)
		soc = DIV_ROUND_CLOSEST((rm_uah - uuuah1 + data->delta_cc_uah) * 10,
					(data->batt_fcc - uuuah + data->delta_cc_uah) / 100);
	else
		soc = 0;

	dev_info(data->dev, "rm_uah = %d, uuc_uah = %d, soc = %d, delta_cc_uah = %d, uuuah1 = %d\n",
		 rm_uah, uuuah, soc, data->delta_cc_uah, uuuah1);

	soc = clamp(soc, 0, SPRD_FGU_FCC_PERCENT);

	data->batt_soc = soc;

	sprd_fgu_get_full_calib_info(data, uuuah1, uuuah);

	return soc;
}

static int sprd_fgu_get_average_full_ibat_dy(struct sprd_fgu_data *data, int ibat, int mode)
{
	int i, min, max;
	int sum = 0, avg_num = 0, cnt = 0, index = 0, avg_ibat;
	static int num;

	if (data->chg_term_loop_ibat[0] == SPRD_FGU_MAGIC_NUMBER) {
		for (i = 0; i < SPRD_FGU_CHG_TERM_LOOP_NUM_SUM; i++)
			data->chg_term_loop_ibat[i] = ibat;

		data->chg_term_loop_ibat_avg_index++;
		num++;
		return ibat;
	}

	data->chg_term_loop_ibat[data->chg_term_loop_ibat_avg_index] = ibat;
	num++;
	avg_num = SPRD_FGU_CHG_TERM_LOOP_TRIG_CNT;
	if (!mode && num > SPRD_FGU_CHG_TERM_LOOP_TRIG_CNT)
		num = SPRD_FGU_CHG_TERM_LOOP_TRIG_CNT;

	if (num > SPRD_FGU_CHG_TERM_LOOP_NUM_SUM)
		num = SPRD_FGU_CHG_TERM_LOOP_NUM_SUM;

	if (num > SPRD_FGU_CHG_TERM_LOOP_TRIG_CNT)
		avg_num = num;

	index = data->chg_term_loop_ibat_avg_index;

	min = max = data->chg_term_loop_ibat[index];
	do {
		if (avg_num >= SPRD_FGU_CHG_TERM_LOOP_NUM_SUM) {
			if (data->chg_term_loop_ibat[index] > max)
				max = data->chg_term_loop_ibat[index];

			if (data->chg_term_loop_ibat[index] < min)
				min = data->chg_term_loop_ibat[index];
		}
		sum += data->chg_term_loop_ibat[index];
		index--;
		index = index < 0 ? (SPRD_FGU_CHG_TERM_LOOP_NUM_SUM - 1) : index;
		cnt++;
	} while (cnt < avg_num);

	if (avg_num >= SPRD_FGU_CHG_TERM_LOOP_NUM_SUM)
		avg_ibat = (sum - max - min) / (avg_num - 2);
	else
		avg_ibat = sum / avg_num;
	data->chg_term_loop_ibat_avg_index++;
	if (data->chg_term_loop_ibat_avg_index >= SPRD_FGU_CHG_TERM_LOOP_NUM_SUM)
		data->chg_term_loop_ibat_avg_index = 0;

	return avg_ibat;
}

static int sprd_fgu_get_average_vbat_dy(struct sprd_fgu_data *data, int vbat, int mode)
{
	int i, min, max;
	int sum = 0, avg_num = 0, cnt = 0, index = 0, avg_vbat;
	static int num;

	if (data->dischg_term_loop_vbat[0] == SPRD_FGU_MAGIC_NUMBER) {
		for (i = 0; i < SPRD_FGU_DISCHG_TERM_LOOP_NUM_SUM; i++)
			data->dischg_term_loop_vbat[i] = vbat;

		data->dischg_term_loop_vbat_avg_index++;
		num++;
		return vbat;
	}

	data->dischg_term_loop_vbat[data->dischg_term_loop_vbat_avg_index] = vbat;
	num++;
	avg_num = SPRD_FGU_DISCHG_TERM_LOOP_TRIG_CNT;
	if (!mode && num > SPRD_FGU_DISCHG_TERM_LOOP_TRIG_CNT)
		num = SPRD_FGU_DISCHG_TERM_LOOP_TRIG_CNT;

	if (num > SPRD_FGU_DISCHG_TERM_LOOP_NUM_SUM)
		num = SPRD_FGU_DISCHG_TERM_LOOP_NUM_SUM;

	if (num > SPRD_FGU_DISCHG_TERM_LOOP_TRIG_CNT)
		avg_num = num;

	index = data->dischg_term_loop_vbat_avg_index;

	min = max = data->dischg_term_loop_vbat[index];
	do {
		if (avg_num >= SPRD_FGU_DISCHG_TERM_LOOP_NUM_SUM) {
			if (data->dischg_term_loop_vbat[index] > max)
				max = data->dischg_term_loop_vbat[index];

			if (data->dischg_term_loop_vbat[index] < min)
				min = data->dischg_term_loop_vbat[index];
		}
		sum += data->dischg_term_loop_vbat[index];
		index--;
		index = index < 0 ? (SPRD_FGU_DISCHG_TERM_LOOP_NUM_SUM - 1) : index;
		cnt++;
	} while (cnt < avg_num);

	if (avg_num >= SPRD_FGU_DISCHG_TERM_LOOP_NUM_SUM)
		avg_vbat = (sum - max - min) / (avg_num - 2);
	else
		avg_vbat = sum / avg_num;
	data->dischg_term_loop_vbat_avg_index++;
	if (data->dischg_term_loop_vbat_avg_index >= SPRD_FGU_DISCHG_TERM_LOOP_NUM_SUM)
		data->dischg_term_loop_vbat_avg_index = 0;

	return avg_vbat;
}

static int sprd_fgu_get_bat_voltage_by_temp(struct sprd_fgu_data *data, int batt_tbat)
{
	int bat_voltage_uv = 3400000;

	if (!data->battery_temp_bat_vol_lut)
		return bat_voltage_uv;

	bat_voltage_uv = interpolate_bat_voltage(data, batt_tbat);

	return bat_voltage_uv;
}

static void sprd_fgu_set_chg_term_enable_mode(struct sprd_fgu_data *data, int mode, int mask)
{
	data->chg_term_loop_enable_mode &= (~mask);
	data->chg_term_loop_enable_mode |= mode;
}

static int sprd_fgu_get_chg_term_loop_mode(struct sprd_fgu_data *data, int ibat_avg_ua,
					   int batt_soc, int *ibat_thd_e)
{
	int rows, ibat_thd, soc_thd, vbat_avg_uv, vbat_now_uv, ibat_soc_thd;
	bool is_ibat_lut_mode = true;
	int ibat_full_entry_ua, vbat_full_entry_uv;

	dev_dbg(data->dev, "bat_temp = %d, chg_sts = %d, ibat_avg_work_cycle_ma = %d\n",
		data->bat_temp, data->chg_sts, data->ibat_avg_work_cycle_ma);

	if (data->bat_temp <= 0 ||
	    data->chg_sts != POWER_SUPPLY_STATUS_CHARGING ||
	    (data->chg_sts == POWER_SUPPLY_STATUS_CHARGING &&
	     data->ibat_avg_work_cycle_ma <= 0) ||
	    !data->chg_term_loop_enable_mode) {
		data->chg_term_loop_mode = 0;
		data->chg_term_loop_trigger_cnt = 0;
		return 0;
	}

	if (!data->battery_cap_temp_ibat_lut ||
	    !(data->chg_term_loop_enable_mode &
	      (CHG_TERM_ENABLE_MODE_IBAT | CHG_TERM_ENABLE_MODE_SOC)))
		is_ibat_lut_mode = false;

	vbat_avg_uv = data->vbat_info.vbat_avg_mv * 1000;
	vbat_now_uv = data->vbat_info.vbat_mv * 1000;

	if (!is_ibat_lut_mode &&
	    (data->chg_term_loop_enable_mode & CHG_TERM_ENABLE_MODE_ABS_IBAT)) {
		if (!data->chg_term_currnet_uA || !data->chg_term_voltage_uv) {
			data->chg_term_currnet_uA = data->new_chg_term_currnet_uA;
			data->chg_term_voltage_uv = data->new_chg_term_voltage_uv;
		}
		ibat_full_entry_ua = data->chg_term_currnet_uA +
			SPRD_FGU_RT_FULL_CALIB_IBAT_DELAT_THD_UA;
		vbat_full_entry_uv = data->chg_term_voltage_uv -
			SPRD_FGU_RT_FULL_CALIB_VBAT_DELTA_THD_UV;
		data->ibat_full_entry_ua = ibat_full_entry_ua;
		if (ibat_full_entry_ua > SPRD_FGU_RT_FULL_CALIB_MAX_IBAT_UA)
			ibat_full_entry_ua = SPRD_FGU_RT_FULL_CALIB_MAX_IBAT_UA;

		if (data->chg_term_currnet_uA != data->new_chg_term_currnet_uA ||
			data->chg_term_voltage_uv != data->new_chg_term_voltage_uv) {
			data->chg_term_loop_mode = 0;
			data->chg_term_loop_trigger_cnt = 0;
		}

		if (ibat_avg_ua <= ibat_full_entry_ua && vbat_avg_uv >= vbat_full_entry_uv &&
			vbat_now_uv >= vbat_full_entry_uv) {
			if (data->chg_term_loop_mode == 3)
				return data->chg_term_loop_mode;

			if (++data->chg_term_loop_trigger_cnt >= SPRD_FGU_CHG_TERM_LOOP_TRIG_CNT) {
				data->chg_term_loop_mode = 3;
				return data->chg_term_loop_mode;
			}
			return 0;
		}

		data->chg_term_loop_mode = 0;
		data->chg_term_loop_trigger_cnt = 0;

		return 0;
	}

	if (!is_ibat_lut_mode) {
		data->chg_term_loop_mode = 0;
		data->chg_term_loop_trigger_cnt = 0;
		return 0;
	}

	*ibat_thd_e = ibat_thd = interpolate_cap2ibat(data, 0, data->bat_temp);

	if (data->chg_term_loop_mode && vbat_avg_uv >= data->fullbatt_uV &&
	    vbat_now_uv >= data->fullbatt_uV)
		return data->chg_term_loop_mode;

	rows = data->battery_cap_temp_ibat_lut->rows;
	soc_thd = data->battery_cap_temp_ibat_lut->cap[rows - 1] * 10;
	ibat_soc_thd = interpolate_cap2ibat(data, batt_soc, data->bat_temp);
	dev_info(data->dev, "ibat_thd = %d, soc_thd = %d, vbat_avg_uv = %d, vbat_now_uv = %d, batt_soc = %d, ibat_avg_ua = %d\n",
		 ibat_thd, soc_thd, vbat_avg_uv, vbat_now_uv, batt_soc, ibat_avg_ua);

	/* BATT_SOC  < 95% &&  IBAT_NOW < IBAT_95 MODE = 1 */
	if (((batt_soc < soc_thd && ibat_avg_ua < ibat_thd) ||
	     (batt_soc >= soc_thd && ibat_avg_ua < ibat_soc_thd)) &&
	    vbat_avg_uv >= data->fullbatt_uV && vbat_now_uv >= data->fullbatt_uV) {
		if (++data->chg_term_loop_trigger_cnt >= SPRD_FGU_CHG_TERM_LOOP_TRIG_CNT) {
			data->chg_term_loop_mode = 1;
			return data->chg_term_loop_mode;
		}
		return 0;
	}

	/* BATT_SOC  >= 95% && IBAT_NOW > IBAT_95 MODE = 2 */
	if ((batt_soc > soc_thd && ibat_avg_ua > ibat_thd) ||
	    (batt_soc >= soc_thd && ibat_avg_ua > ibat_soc_thd &&
	     ibat_avg_ua <= ibat_thd && vbat_avg_uv >= data->fullbatt_uV &&
	     vbat_now_uv >= data->fullbatt_uV)) {
		if (++data->chg_term_loop_trigger_cnt >= SPRD_FGU_CHG_TERM_LOOP_TRIG_CNT) {
			data->chg_term_loop_mode = 2;
			return data->chg_term_loop_mode;
		}
		return 0;
	}

	data->chg_term_loop_mode = 0;
	data->chg_term_loop_trigger_cnt = 0;

	return 0;
}

static void sprd_fgu_chg_term_loop_ibat_adjust(struct sprd_fgu_data *data,
					       int ibat_avg_ua, int ibat_soc)
{
	int new_rm_uah = 0, new_rm_soc = data->batt_rm_soc, ibat_soc_adj_thd;

	if (data->rt_calib_vaild)
		return;

	if (data->bat_temp >= SPRD_FGU_TEMP_DEFAULT)
		ibat_soc_adj_thd = 30;
	else if (data->bat_temp >= SPRD_FGU_LOW_TEMP_REGION)
		ibat_soc_adj_thd = 40;
	else
		ibat_soc_adj_thd = 50;

	if ((data->batt_rm_soc + data->chg_loss_soc) > (ibat_soc + ibat_soc_adj_thd)) {
		new_rm_soc = ibat_soc + ibat_soc_adj_thd - data->chg_loss_soc;
		new_rm_uah = data->batt_fcc / 1000 * new_rm_soc;
		data->err_fcc_uah += data->batt_rm_uah - new_rm_uah;
	} else if ((data->batt_rm_soc + data->chg_loss_soc) < (ibat_soc - ibat_soc_adj_thd)) {
		new_rm_soc = ibat_soc - ibat_soc_adj_thd - data->chg_loss_soc;
		new_rm_uah = data->batt_fcc / 1000 * new_rm_soc;
		data->err_fcc_uah += data->batt_rm_uah - new_rm_uah;
	}

	dev_info(data->dev, "batt_rm_soc = %d, new_rm_soc = %d, new_rm_uah = %d, batt_rm_uah = %d, ibat_soc_adj_thd = %d, chg_loss_soc = %d\n",
		data->batt_rm_soc, new_rm_soc, new_rm_uah, data->batt_rm_uah,
		ibat_soc_adj_thd, data->chg_loss_soc);
}

// TODO
static int sprd_fgu_chg_term_loop_ibat(struct sprd_fgu_data *data, int batt_soc,
				       int ibat_thd_e, int ibat_avg_ua)
{
	int ibat_now_ua, ibat_full, ibat_e_ua, ibat_soc;
	int ratio, cali_soc;

	if (!(data->chg_term_loop_enable_mode & CHG_TERM_ENABLE_MODE_IBAT))
		return batt_soc;

	ibat_now_ua = ibat_avg_ua;
	ibat_full = data->fullbatt_uA;
	ibat_e_ua = ibat_thd_e;
	if (ibat_now_ua < ibat_full)
		ibat_now_ua = ibat_full;
	if (ibat_now_ua > ibat_e_ua) {
		data->chg_term_loop_mode = 0;
		data->chg_term_loop_trigger_cnt = 0;
		dev_info(data->dev, "ibat_now_ua > ibat_e_ua, exit chg_term_loop_mode 1\n");
		return batt_soc;
	}
	ibat_soc = interpolate_ibat2cap(data, data->bat_temp, ibat_now_ua);

	sprd_fgu_chg_term_loop_ibat_adjust(data, ibat_avg_ua, ibat_soc);

	/* kuo da 1000 bei */
	ratio = DIV_ROUND_CLOSEST((ibat_now_ua - ibat_full) * 1000, (ibat_e_ua - ibat_full));
	dev_info(data->dev, "un limit ratio = %d\n", ratio);
	ratio = clamp(ratio, 0, 1000);
	cali_soc = (batt_soc * ratio + ibat_soc * (1000 - ratio)) / 1000;
	cali_soc = clamp(cali_soc, 0, 1000);
	dev_info(data->dev, "ratio = %d, cali_soc = %d, ibat_now_ua = %d, ibat_full = %d, ibat_e_ua = %d, ibat_soc = %d\n",
		 ratio, cali_soc, ibat_now_ua, ibat_full, ibat_e_ua, ibat_soc);

	return cali_soc;
}

// TODO
static int sprd_fgu_chg_term_loop_abs_ibat(struct sprd_fgu_data *data, int batt_soc,
					   int ibat_avg_ua, int last_mode)
{
	int ibat_now_ma, ibat_full_ma, ibat_soc;
	int ratio, cali_soc;
	static int k, b, batt_soc_e, ibat_thd_e_ma, batt_temp_e, ibat_full_entry_ma;

	if (!(data->chg_term_loop_enable_mode & CHG_TERM_ENABLE_MODE_ABS_IBAT))
		return batt_soc;

	cali_soc = batt_soc;
	ibat_full_ma = data->new_chg_term_currnet_uA / 1000;
	ibat_now_ma = ibat_avg_ua / 1000;

	dev_info(data->dev, "batt_soc = %d, ibat_full_ma = %d, ibat_now_ma = %d, smooth_soc = %d\n",
		 batt_soc, ibat_full_ma, ibat_now_ma, data->smooth_soc);

	if (last_mode != 3 || (ibat_now_ma >= ibat_thd_e_ma * 103 / 100) ||
		data->chg_term_loop_first_in == 0 || batt_soc < (batt_soc_e - 5) ||
		abs(data->bat_temp - batt_temp_e) > 50) {
		batt_soc_e = batt_soc;
		ibat_thd_e_ma = ibat_now_ma;
		ibat_full_entry_ma = data->ibat_full_entry_ua / 1000;
		if (batt_soc_e == SPRD_FGU_FCC_PERCENT)
			return cali_soc;

		k = (1000 - batt_soc_e) * 10000 / (ibat_full_ma - ibat_thd_e_ma);
		b = 1000 * 10000 - k * ibat_full_ma;

		batt_temp_e = data->bat_temp;
		data->chg_term_loop_first_in = 1;

		return cali_soc;
	}

	if (batt_soc_e == SPRD_FGU_FCC_PERCENT)
		return batt_soc_e;

	ibat_soc = DIV_ROUND_CLOSEST(k * ibat_now_ma + b, 10000);
	if (ibat_now_ma < ibat_full_ma)
		ibat_now_ma = ibat_full_ma;
	ratio = DIV_ROUND_CLOSEST((ibat_now_ma - ibat_full_ma) * 1000,
				  (ibat_full_entry_ma - ibat_full_ma));
	dev_info(data->dev, "un limit ratio = %d\n", ratio);
	ratio = clamp(ratio, 0, 1000);
	cali_soc = DIV_ROUND_CLOSEST(ibat_soc * (1000 - ratio) + batt_soc * ratio, 1000);

	dev_info(data->dev, "k = %d, b = %d, ratio = %d, cali_soc = %d, ibat_thd_e_ma = %d, ibat_full_ma = %d, ibat_now_ma = %d, ibat_soc = %d\n",
		 k, b, ratio, cali_soc, ibat_thd_e_ma, ibat_full_ma, ibat_now_ma, ibat_soc);

	return cali_soc;
}

// TODO

static int sprd_fgu_chg_term_loop_soc(struct sprd_fgu_data *data, int batt_soc, int last_mode)
{
	int ibat_thd, vbat_avg_uv, vbat_now_uv, ibat_avg_ua, cali_soc, ret, cur_cc_uah;
	int ibat_e_ua = 0;
	int batt_cali_dalta, delta_cccap;
	static int batt_soc_e, cc_uah_e, acc_time, time_need, batt_soc_next, trigger_cnt, fcc_uah;

	if (!(data->chg_term_loop_enable_mode & CHG_TERM_ENABLE_MODE_SOC))
		return batt_soc;

	cali_soc = batt_soc;

	if (last_mode != 2) {
		batt_soc_e = batt_soc;
		if (batt_soc_e == SPRD_FGU_FCC_PERCENT)
			return batt_soc_e;

		trigger_cnt = 0;
		ret = data->fgu_info->ops->get_cc_uah(data->fgu_info, &cc_uah_e, false);
		if (ret) {
			dev_err(data->dev, "failed to get cc uah!\n");
			return cali_soc;
		}
		batt_soc_next = (batt_soc_e + 10) / 10 * 10;
		ibat_e_ua = interpolate_cap2ibat(data, batt_soc_e, data->bat_temp);
		fcc_uah = data->batt_fcc;
		//(batt_soc_next - batt_soc_e) * fcc_uah / 1000 /1000 * 3600 / (ibat_e_ua / 1000)
		time_need = (batt_soc_next - batt_soc_e) * fcc_uah / 10 * 36 / (ibat_e_ua);// ?
		acc_time = 0;
		dev_info(data->dev, "batt_soc_e = %d. batt_soc_next = %d, ibat_e_ua = %d, time_need= %d\n",
			 batt_soc_e, batt_soc_next, ibat_e_ua, time_need);

		return cali_soc;
	}

	if (batt_soc_e == SPRD_FGU_FCC_PERCENT)
		return batt_soc_e;

	acc_time += data->work_enter_times - data->work_exit_times;
	batt_cali_dalta = (batt_soc_next - batt_soc_e) * acc_time / time_need;

	ret = data->fgu_info->ops->get_cc_uah(data->fgu_info, &cur_cc_uah, false);
	if (ret) {
		dev_err(data->dev, "failed to get cc uah!\n");
		return cali_soc;
	}

	delta_cccap = (cur_cc_uah - cc_uah_e) * 10 / (fcc_uah / 100);
	if (delta_cccap < 0)
		delta_cccap = 0;

	cali_soc = batt_soc_e + min(batt_cali_dalta, delta_cccap);
	cali_soc = clamp(cali_soc, 0, 1000);

	if (cali_soc < 1000 && cali_soc >= batt_soc_next) {
		batt_soc_e = cali_soc;
		ret = data->fgu_info->ops->get_cc_uah(data->fgu_info, &cc_uah_e, false);
		if (ret) {
			dev_err(data->dev, "failed to get cc uah!\n");
			return cali_soc;
		}
		batt_soc_next = (batt_soc_e + 10) / 10 * 10;
		ibat_e_ua = interpolate_cap2ibat(data, batt_soc_e, data->bat_temp);
		fcc_uah = data->batt_fcc;
		time_need = (batt_soc_next - batt_soc_e) * fcc_uah / 10 * 36 / (ibat_e_ua);// ?
		acc_time = 0;
	}

	dev_info(data->dev, "batt_soc_e = %d. batt_soc_next = %d, ibat_e_ua = %d, time_need= %d, acc_time = %d, batt_cali_dalta = %d, delta_cccap = %d\n",
		 batt_soc_e, batt_soc_next, ibat_e_ua, time_need,
		 acc_time, batt_cali_dalta, delta_cccap);
	ibat_thd = interpolate_cap2ibat(data, cali_soc, data->bat_temp);
	vbat_avg_uv = data->vbat_info.vbat_avg_mv * 1000;
	vbat_now_uv = data->vbat_info.vbat_mv * 1000;
	ibat_avg_ua = data->ibat_avg_work_cycle_ma * 1000;

	if (ibat_avg_ua <= ibat_thd && vbat_avg_uv >= data->fullbatt_uV &&
	    vbat_now_uv >= data->fullbatt_uV && ++trigger_cnt >= SPRD_FGU_CHG_TERM_LOOP_TRIG_CNT) {
		data->chg_term_loop_mode = 1;
		dev_info(data->dev, "chg_term_loop_mode from 2 switch to 1\n");
	}

	return cali_soc;
}

static int sprd_fgu_full_delta_cap(struct sprd_fgu_data *data, int soc, int diff_cap,
				   int delta_cap, int ibat_avg_ua, int chg_term_loop_mode)
{
	int step_num, step_cap, work_cycle, fcc_mah, ibat_ma = ibat_avg_ua / 1000, up_limit;
	int charge_soc = 1000 - soc;

	if (delta_cap <= 0) {
		dev_info(data->dev, "org delta_cap = %d\n", delta_cap);
		return 0;
	}

	work_cycle = data->work_cycle;
	if (!work_cycle)
		work_cycle = SPRD_FGU_NORMAL_WORK_10S;
	if (ibat_ma <= 0)
		ibat_ma = SPRD_FGU_CHARGING_IAVG_MA;

	up_limit = (data->work_enter_times - data->work_cali_exit_times + 1) / work_cycle;

	fcc_mah = data->batt_fcc / 1000;
	step_num = charge_soc * fcc_mah / 1000 * 3600 / abs(ibat_ma) / work_cycle;

	dev_info(data->dev, "fcc_mah = %d, soc = %d, ibat_ma = %d, work_cycle = %d, up_limit = %d\n",
		 fcc_mah, soc, ibat_ma, work_cycle, up_limit);
	if (step_num == 0) {
		step_cap = clamp(delta_cap, 0, (up_limit * 10));
		dev_info(data->dev, "clamp step_cap = %d\n", step_cap);
		return step_cap;
	}

	step_cap = delta_cap / step_num;

	dev_info(data->dev, "org step_cap = %d\n", step_cap);

	if (diff_cap > 0) {
		if (chg_term_loop_mode == 3)
			step_cap = clamp(step_cap, (2 * up_limit), (10 * up_limit));
		else
			step_cap = clamp(step_cap, (1 * up_limit), (10 * up_limit));
	} else if (diff_cap == 0) {
		step_cap = clamp(step_cap, (3 * up_limit), (10 * up_limit));
	}

	if (step_cap > delta_cap)
		step_cap = delta_cap;

	dev_info(data->dev, "calc step_cap = %d\n", step_cap);

	return step_cap;
}

static int sprd_fgu_get_chg_full_adj_mode(struct sprd_fgu_data *data, int batt_soc,
					  int *new_ocv_soc, int *cur_ma)
{
	int ocv_soc[SPRD_FGU_RT_FULL_CALIB_INFO_TABLE_LEN];
	int cur[SPRD_FGU_RT_FULL_CALIB_INFO_TABLE_LEN];
	int cnt = 0, valid_count = 0, ocv_valid_cnt = SPRD_FGU_RT_FULL_CALIB_INFO_TABLE_LEN;
	int last_calib_info_index = (data->rt_full_calib_index - 1 < 0) ?
		SPRD_FGU_RT_FULL_CALIB_INFO_TABLE_LEN - 1 : data->rt_full_calib_index - 1;
	int i, cur_thd_ma, cur_avg = 0;
	struct sprd_fgu_rt_full_calib_info *calib_info =
		&data->full_calib_info[last_calib_info_index];

	if (data->rt_full_calib_valid_cnt >= SPRD_FGU_RT_FULL_CALIB_INFO_TABLE_LEN)
		valid_count = SPRD_FGU_RT_FULL_CALIB_INFO_TABLE_LEN;
	else
		valid_count = data->rt_full_calib_index;

	if (valid_count < ocv_valid_cnt)
		return 0;

	*new_ocv_soc = calib_info->batt_cali_ocv_soc;
	*cur_ma = calib_info->cur_avg_ma;

	do {
		ocv_soc[cnt] = calib_info->calib_ocv_soc;
		cur[cnt] = calib_info->work_cur_avg_ma;
		last_calib_info_index--;

		last_calib_info_index = (last_calib_info_index < 0) ?
			SPRD_FGU_RT_FULL_CALIB_INFO_TABLE_LEN - 1 : last_calib_info_index;

		calib_info = &data->full_calib_info[last_calib_info_index];

		cnt++;
	} while (cnt < ocv_valid_cnt);

	for (i = 0; i < ocv_valid_cnt; i++)
		cur_avg += cur[i];

	cur_avg /= ocv_valid_cnt;
	cur_thd_ma = cur_avg * 10 / 100;
	cur_thd_ma  = clamp(cur_thd_ma, 100, 300);

	for (i = 1; i < ocv_valid_cnt; i++) {
		if (abs(ocv_soc[i] - ocv_soc[i - 1]) > 10)
			return 0;
		if (abs(cur[i] - cur[i - 1]) > cur_thd_ma)
			return 0;
	}

	return 1;
}

static int sprd_fgu_chg_adjust_batt_soc(struct sprd_fgu_data *data, int batt_soc)
{
	int mode, ocv_soc, ratio, ratio1, ratio2, cur_ma, mix_soc;
	static int chg_full_adj_mode;
	struct sprd_fgu_rt_full_calib_info *calib_info =
		&data->full_calib_info[data->rt_full_calib_index];
	int chg_cur1 = 500, ratio_up_limit = 800, chg_cur2 = 3000, ratio_low_limit = 200;
	int batt_soc1 = 850, batt_soc2 = 1000, delta_uah, delta_soc;

	if (data->start_charge_times == 0 || (data->work_enter_times - data->start_charge_times <
	    SPRD_FGU_ABS_FULL_START_CHARGE_TIMES)) {
		chg_full_adj_mode = 0;
		return batt_soc;
	}

	if (batt_soc < 850 && calib_info->batt_cali_ocv_soc < 850) {
		chg_full_adj_mode = 0;
		return batt_soc;
	}

	mode = sprd_fgu_get_chg_full_adj_mode(data, batt_soc, &ocv_soc, &cur_ma);

	if (chg_full_adj_mode == 0 && !mode) {
		chg_full_adj_mode = 0;
		return batt_soc;
	}

	if (chg_full_adj_mode == 0) {
		chg_full_adj_mode = mode;
		data->rt_full_adj_calib_ocv_soc = ocv_soc;
	}

	delta_uah = DIV_ROUND_CLOSEST(calib_info->work_cur_avg_ma * calib_info->work_times * 10,
				      36);
	delta_soc = DIV_ROUND_CLOSEST(delta_uah * 10, data->batt_fcc / 100);
	delta_soc = delta_soc * 3;
	if (delta_soc  < 3)
		delta_soc = 3;

	if (data->rt_full_adj_calib_ocv_soc - ocv_soc > delta_soc)
		data->rt_full_adj_calib_ocv_soc -= delta_soc;
	else if (ocv_soc - data->rt_full_adj_calib_ocv_soc > delta_soc)
		data->rt_full_adj_calib_ocv_soc += delta_soc;
	else
		data->rt_full_adj_calib_ocv_soc = ocv_soc;

	if (cur_ma < chg_cur1)
		ratio1 = ratio_up_limit;
	else if (cur_ma > chg_cur2)
		ratio1 = ratio_low_limit;
	else
		ratio1 = linear_interpolate(ratio_up_limit, chg_cur1,
					    ratio_low_limit, chg_cur2, cur_ma);


	if (batt_soc < batt_soc1)
		ratio2 = ratio_low_limit;
	else if (batt_soc > batt_soc2)
		ratio2 = ratio_up_limit;
	else
		ratio2 = linear_interpolate(ratio_low_limit, batt_soc1,
					    ratio_up_limit, batt_soc2, batt_soc);

	ratio = max(ratio1, ratio2);

	mix_soc = DIV_ROUND_CLOSEST((batt_soc * (1000 - ratio) +
				     data->rt_full_adj_calib_ocv_soc * (ratio)), 1000);

	data->rt_full_calib_mix_soc = mix_soc;

	dev_info(data->dev, "batt_soc = %d, chg_full_adj_mode = %d, ocv_soc = %d, cur_ma = %d, ratio1 = %d, ratio2 = %d, ratio = %d, mix_soc = %d, rt_full_adj_calib_ocv_soc = %d, delta_soc = %d\n",
		 batt_soc, chg_full_adj_mode, ocv_soc, cur_ma, ratio1, ratio2,
		 ratio, mix_soc, data->rt_full_adj_calib_ocv_soc, delta_soc);

	return data->rt_full_calib_mix_soc;
}

static int sprd_fgu_chg_term_loop(struct sprd_fgu_data *data, int batt_soc, int *charge_diff_cap)
{
	int chg_term_loop_mode = 0, full_cali_soc, ibat_thd_e, ibat_avg_ua;
	int delta_cali_soc, step_cap;
	static int last_full_cali_soc = SPRD_FGU_MAGIC_NUMBER, last_acc_delta_cali_soc;
	static int last_chg_term_loop_mode = SPRD_FGU_MAGIC_NUMBER;

	batt_soc = sprd_fgu_chg_adjust_batt_soc(data, batt_soc);
	full_cali_soc = batt_soc;

	if (last_chg_term_loop_mode == SPRD_FGU_MAGIC_NUMBER)
		last_chg_term_loop_mode = chg_term_loop_mode;
	if (last_full_cali_soc == SPRD_FGU_MAGIC_NUMBER)
		last_full_cali_soc = full_cali_soc;

	ibat_avg_ua = sprd_fgu_get_average_full_ibat_dy(data, data->ibat_avg_work_cycle_ma,
							data->chg_term_loop_mode) * 1000;
	chg_term_loop_mode = sprd_fgu_get_chg_term_loop_mode(data, ibat_avg_ua,
							     batt_soc, &ibat_thd_e);
	if (chg_term_loop_mode == 3 || chg_term_loop_mode == 1) {
		if (chg_term_loop_mode == 3) {
			full_cali_soc =
				sprd_fgu_chg_term_loop_abs_ibat(data, batt_soc, ibat_avg_ua,
								last_chg_term_loop_mode);
		} else if (chg_term_loop_mode == 1) {
			data->chg_term_loop_first_in = 0;
			full_cali_soc = sprd_fgu_chg_term_loop_ibat(data, batt_soc,
								    ibat_thd_e, ibat_avg_ua);
		}
		if (chg_term_loop_mode == 3 || chg_term_loop_mode == 1) {
			if (full_cali_soc >= last_full_cali_soc &&
			    full_cali_soc <= data->smooth_soc) {
				dev_info(data->dev, "full1, full_cali_soc = %d, last_full_cali_soc = %d, smooth_soc = %d\n",
					 full_cali_soc, last_full_cali_soc, data->smooth_soc);
				last_full_cali_soc = full_cali_soc;
				last_acc_delta_cali_soc = 0;
			} else if (full_cali_soc >= last_full_cali_soc &&
				   last_full_cali_soc <= data->smooth_soc) {
				dev_info(data->dev, "full2, full_cali_soc = %d, last_full_cali_soc = %d, smooth_soc = %d\n",
					 full_cali_soc, last_full_cali_soc, data->smooth_soc);
				last_full_cali_soc = data->smooth_soc;
				last_acc_delta_cali_soc = 0;
			} else if (full_cali_soc <= last_full_cali_soc &&
				   full_cali_soc <= data->smooth_soc) {
				dev_info(data->dev, "full3, full_cali_soc = %d, last_full_cali_soc = %d, smooth_soc = %d\n",
					 full_cali_soc, last_full_cali_soc, data->smooth_soc);
				last_full_cali_soc = full_cali_soc;
				last_acc_delta_cali_soc = 0;
			}

			delta_cali_soc = (full_cali_soc - last_full_cali_soc) * 10 +
				last_acc_delta_cali_soc - *charge_diff_cap;
			if (full_cali_soc > data->smooth_soc &&
			    delta_cali_soc > (full_cali_soc - data->smooth_soc) * 10)
				delta_cali_soc = (full_cali_soc - data->smooth_soc) * 10;
			step_cap =
				sprd_fgu_full_delta_cap(data, full_cali_soc, *charge_diff_cap,
							delta_cali_soc, ibat_avg_ua,
							chg_term_loop_mode);
			*charge_diff_cap += step_cap;
			last_acc_delta_cali_soc = delta_cali_soc - step_cap;
			dev_info(data->dev, "full_cali_soc = %d. last_full_cali_soc = %d, last_acc_delta_cali_soc = %d. *charge_diff_cap = %d, delta_cali_soc = %d, step_cap = %d\n",
				 full_cali_soc, last_full_cali_soc, last_acc_delta_cali_soc,
				 *charge_diff_cap, delta_cali_soc, step_cap);
			if (last_acc_delta_cali_soc < 0)
				last_acc_delta_cali_soc = 0;
		} else {
			last_acc_delta_cali_soc = 0;
		}
	} else if (chg_term_loop_mode == 2) {
		data->chg_term_loop_first_in = 0;
		full_cali_soc = sprd_fgu_chg_term_loop_soc(data, batt_soc,
							   last_chg_term_loop_mode);
		if (last_chg_term_loop_mode == 2) {
			if (full_cali_soc < last_full_cali_soc)
				full_cali_soc = last_full_cali_soc;
			*charge_diff_cap = (full_cali_soc - last_full_cali_soc) * 10;
		}
	} else {
		data->chg_term_loop_first_in = 0;
		last_acc_delta_cali_soc = 0;
	}

	last_full_cali_soc = full_cali_soc;
	last_chg_term_loop_mode = chg_term_loop_mode;
	dev_info(data->dev, "batt_soc = %d, full_cali_soc = %d, chg_term_loop_mode = %d, chg_term_loop_enable_mode = %d, ibat_avg_ua = %d\n",
		 batt_soc, full_cali_soc, chg_term_loop_mode,
		 data->chg_term_loop_enable_mode, ibat_avg_ua);

	return full_cali_soc;
}

static int sprd_fgu_dischg_term_loop_vbat(struct sprd_fgu_data *data, int batt_soc,
					  int last_mode, int vbat_avg_mv,
					  int vbat_cutoff_mv, bool vbat_low)
{
	int cali_soc, cali_soc_1, vbat_now_mv = vbat_avg_mv, ratio;
	static int k, b, vbat_thd_mv, batt_soc_e, batt_temp_e, vbat_cutoff_k_mv;

	cali_soc = batt_soc;

	dev_info(data->dev, "batt_soc = %d, vbat_now_mv = %d, vbat_cutoff_mv = %d, smooth_soc = %d, vbat_low = %d\n",
		 batt_soc, vbat_now_mv, vbat_cutoff_mv, data->smooth_soc, vbat_low);

	if (vbat_low)
		return 0;

	if (last_mode != 1 || data->dischg_term_loop_first_in == 0 ||
	    vbat_now_mv > (vbat_thd_mv + 10) || batt_soc > (batt_soc_e + 5) ||
	    abs(data->bat_temp - batt_temp_e) > 50) {
		if (cali_soc == 0 && data->smooth_soc == 0)
			return cali_soc;

		vbat_thd_mv = vbat_now_mv;
		if (vbat_thd_mv <= vbat_cutoff_mv)
			return cali_soc;

		if (vbat_thd_mv <= vbat_cutoff_mv + 50)
			vbat_thd_mv = vbat_cutoff_mv + 50;

		batt_soc_e = batt_soc;
		if (cali_soc == 0 && data->smooth_soc != 0)
			batt_soc_e = data->smooth_soc;
		k = batt_soc_e * 10000 / (vbat_thd_mv - vbat_cutoff_mv);//todo
		b = -k * vbat_cutoff_mv;//todo
		batt_temp_e = data->bat_temp;
		vbat_cutoff_k_mv = vbat_cutoff_mv;

		data->dischg_term_loop_first_in = 1;

		return cali_soc;
	}

	vbat_now_mv += SPRD_FGU_DISCHG_TERM_VOL_OFFSET;
	cali_soc_1 = DIV_ROUND_CLOSEST(k * vbat_now_mv + b, 10000);
	dev_info(data->dev, "un limit cali_soc_1 = %d\n", cali_soc_1);
	cali_soc_1 = clamp(cali_soc_1, 0, 1000);
	ratio = DIV_ROUND_CLOSEST((vbat_now_mv - vbat_cutoff_k_mv) * 1000,
				  (vbat_thd_mv - vbat_cutoff_k_mv));
	dev_info(data->dev, "un limit ratio = %d\n", ratio);
	ratio = clamp(ratio, 0, 1000);

	if (batt_soc < 5 && vbat_avg_mv > vbat_cutoff_k_mv)
		batt_soc = 5;

	cali_soc = DIV_ROUND_CLOSEST(batt_soc * ratio +  cali_soc_1 * (1000 - ratio), 1000);

	dev_info(data->dev, "batt_soc = %d, cali_soc = %d, cali_soc_1 = %d, ratio = %d, k = %d, vbat_now_mv = %d, b = %d, batt_soc_e = %d, vbat_thd_mv = %d, vbat_cutoff_k_mv = %d\n",
		 batt_soc, cali_soc, cali_soc_1, ratio, k, vbat_now_mv, b,
		 batt_soc_e, vbat_thd_mv, vbat_cutoff_k_mv);

	if (cali_soc <= 0 && vbat_now_mv > vbat_cutoff_k_mv)
		cali_soc = 1;
	else if (cali_soc <= 0 && vbat_now_mv <= vbat_cutoff_k_mv)
		cali_soc = 0;

	if (cali_soc < 1)
		cali_soc = 1;

	return cali_soc;
}

static int sprd_fgu_dischg_term_loop_soc(struct sprd_fgu_data *data, int batt_soc,
					 int last_mode, int vbat_avg_mv, bool vbat_low)
{
	int cali_soc;
	static int start_cc_uah, last_err_cutoff_uah;

	cali_soc = batt_soc;

	if (last_mode != 2) {
		start_cc_uah = data->cc_uah;
		last_err_cutoff_uah = data->err_cutoff_uah;
		data->dischg_term_slow_down_ratio = 6;
		return cali_soc;
	}

	if (data->err_cc_adj_en)
		return cali_soc;

	if (start_cc_uah)
		data->err_cutoff_uah = (data->cc_uah - start_cc_uah) *
		(10 - data->dischg_term_slow_down_ratio) / 10 + last_err_cutoff_uah;

	dev_info(data->dev, "start_cc_uah = %d, cc_uah = %d, dischg_term_slow_down_ratio = %d, err_cutoff_uah = %d, last_err_cutoff_uah = %d\n",
		 start_cc_uah, data->cc_uah, data->dischg_term_slow_down_ratio,
		 data->err_cutoff_uah, last_err_cutoff_uah);

	return cali_soc;
}

static int sprd_fgu_get_dischg_term_loop_mode(struct sprd_fgu_data *data, int vbat_avg_mv,
					      int batt_soc, int vbat_thd_mv)
{
	int soc_thd = 35 - data->calc_capacity_hc;

	soc_thd = clamp(soc_thd, 25, 1000);

	dev_info(data->dev, "bat_temp = %d, chg_sts = %d, vbat_avg_mv = %d, vbat_thd_mv = %d, soc_thd = %d\n",
		data->bat_temp, data->chg_sts, vbat_avg_mv, vbat_thd_mv, soc_thd);

	if (data->bat_temp < -200 || !data->battery_temp_bat_vol_lut) {
		data->dischg_term_loop_mode = 0;
		data->dischg_term_loop_trigger_cnt = 0;
		return 0;
	}

	if (data->chg_sts == POWER_SUPPLY_STATUS_CHARGING ||
	    data->ibat_avg_work_cycle_ma > 0) {
		data->dischg_term_loop_mode = 0;
		data->dischg_term_loop_trigger_cnt = 0;
		return 0;
	}

	if (batt_soc < soc_thd && vbat_avg_mv > vbat_thd_mv) {
		if (++data->dischg_term_loop_trigger_cnt >= SPRD_FGU_DISCHG_TERM_LOOP_TRIG_CNT) {
			data->dischg_term_loop_mode = 2;
			return data->dischg_term_loop_mode;
		}
		return 0;
	}

	if (vbat_avg_mv <= vbat_thd_mv) {
		if (++data->dischg_term_loop_trigger_cnt >= SPRD_FGU_DISCHG_TERM_LOOP_TRIG_CNT) {
			data->dischg_term_loop_mode = 1;
			return data->dischg_term_loop_mode;
		}
		return 0;
	}

	data->dischg_term_loop_mode = 0;
	data->dischg_term_loop_trigger_cnt = 0;

	return 0;
}

static int sprd_fgu_cutoff_delta_cap(struct sprd_fgu_data *data, int soc,
				     int diff_cap, int delta_cap)
{
	int step_num, step_cap, work_cycle, fcc_mah, ibat_ma = data->batt_ibat_avg_ma, up_limit;
	int soc_offset_0 = 6, soc_offset_1 = 10;

	if (delta_cap >= 0 || (diff_cap == 0 && soc >= data->smooth_soc)) {
		dev_info(data->dev, "org delta_cap = %d\n", delta_cap);
		return 0;
	}

	work_cycle = data->work_cycle;
	if (!work_cycle)
		work_cycle = SPRD_FGU_NORMAL_WORK_10S;

	up_limit = (data->work_enter_times - data->work_cali_exit_times + 1) / work_cycle;

	fcc_mah = data->batt_fcc / 1000;

	if (data->bat_temp <= 0 && soc <= 40 && (soc + soc_offset_1) <= data->smooth_soc) {
		soc += soc_offset_1;
		soc = clamp(soc, 0, data->smooth_soc);
	} else if (data->bat_temp >= 0 && soc <= 30 && (soc + soc_offset_0) <= data->smooth_soc) {
		soc += soc_offset_0;
		soc = clamp(soc, 0, data->smooth_soc);
	}

	soc = clamp(soc, 5, 1000);
	step_num = soc * fcc_mah / 1000 * 3600 / abs(ibat_ma) / work_cycle;

	dev_info(data->dev, "fcc_mah = %d, soc = %d, ibat_ma = %d, work_cycle = %d, up_limit = %d, step_num = %d\n",
		 fcc_mah, soc, ibat_ma, work_cycle, up_limit, step_num);
	if (step_num == 0) {
		step_cap = clamp(delta_cap, -(up_limit * 30), 0);
		dev_info(data->dev, "clamp step_cap = %d\n", step_cap);
		return step_cap;
	}

	step_cap = delta_cap / step_num;

	dev_info(data->dev, "org step_cap = %d\n", step_cap);

	if (diff_cap < 0) {
		if (soc >= data->smooth_soc)
			step_cap = clamp(step_cap, -(1 * up_limit), -(1 * up_limit));
		else if (soc >= 100)
			step_cap = clamp(step_cap, -(15 * up_limit), -(1 * up_limit));
		else
			step_cap = clamp(step_cap, -(15 * up_limit), -(2 * up_limit));
	} else if (diff_cap == 0) {
		if (soc >= 100)
			step_cap = clamp(step_cap, -(15 * up_limit), -(2 * up_limit));
		else
			step_cap = clamp(step_cap, -(15 * up_limit), -(3 * up_limit));
	}

	if (step_cap < delta_cap)
		step_cap = delta_cap;

	dev_info(data->dev, "calc step_cap = %d\n", step_cap);

	return step_cap;
}

static void sprd_fgu_cutoff_cali(struct sprd_fgu_data *data, int cutoff_cali_soc, bool vbat_low)
{
	int soc = 0, err_soc = 0, err_uah;
	bool low_energy_dens = false;

	if (data->rt_calib_vaild)
		return;

	if (data->bat_temp <= SPRD_FGU_CAP_CALIB_TEMP_MIN)
		return;

	if (sprd_fgu_is_in_low_energy_dens(data, data->raw_uuocv_uv))
		low_energy_dens = true;

	if (data->bat_temp >= 200) {
		soc = 30;
		if (!low_energy_dens)
			soc = 50;
	} else if (data->bat_temp >= 100) {
		soc = 50;
		if (!low_energy_dens)
			soc = 70;
	} else if (data->bat_temp >= 0 && data->bat_temp <= 100) {
		soc = 70;
		if (!low_energy_dens)
			soc = 100;
	} else {
		soc = 100;
		if (!low_energy_dens)
			soc = 120;
	}

	if (vbat_low && cutoff_cali_soc == 0 && data->batt_soc > 0 &&
	    data->raw_uusoc == data->pre_uusoc) {
		if (data->batt_rm_soc > (data->pre_uusoc + soc)) {
			err_soc = data->batt_rm_soc - (data->pre_uusoc + soc);

			err_uah = DIV_ROUND_CLOSEST((err_soc * data->batt_fcc), 1000);
			data->err_shutdown_uah += err_uah;
			dev_info(data->dev, "calc err_uah = %d\n", err_uah);
		}
	}
}

static int sprd_fgu_dischg_term_loop(struct sprd_fgu_data *data, int batt_soc,
				     int *charge_diff_cap)
{
	int dischg_term_loop_mode = 0, cutoff_cali_soc, vbat_avg_mv, delta_cali_soc;
	int org_charge_diff_cap, vbat_cutoff_mv, vbat_cutoff_uv;
	int recalc_charge_diff_cap = 0, vbat_thd_mv, vbat_thd_uv, step_cap;
	bool vbat_low = false;
	static int last_cutoff_cali_soc = SPRD_FGU_MAGIC_NUMBER, last_acc_delta_cali_soc;
	static int last_dischg_term_loop_mode = SPRD_FGU_MAGIC_NUMBER, trigger_cnt;
	static int change_soc = SPRD_FGU_MAGIC_NUMBER;
	struct sprd_fgu_vbat_info *vbat_info = &data->vbat_info;

	cutoff_cali_soc = batt_soc;
	org_charge_diff_cap = *charge_diff_cap;

	if (last_dischg_term_loop_mode == SPRD_FGU_MAGIC_NUMBER)
		last_dischg_term_loop_mode = dischg_term_loop_mode;
	if (last_cutoff_cali_soc == SPRD_FGU_MAGIC_NUMBER)
		last_cutoff_cali_soc = cutoff_cali_soc;

	vbat_avg_mv = vbat_info->vbat_avg_mv_dy;
	vbat_cutoff_uv = data->vbat_info.zp_vol_uv;
	vbat_thd_uv = sprd_fgu_get_bat_voltage_by_temp(data, data->bat_temp);
	vbat_cutoff_mv = vbat_cutoff_uv / 1000;
	vbat_thd_mv = vbat_thd_uv / 1000;
	data->cutoff_vbat_avg_mv = vbat_avg_mv;
	if (vbat_avg_mv + SPRD_FGU_DISCHG_TERM_VOL_OFFSET < vbat_cutoff_mv)
		trigger_cnt++;
	else
		trigger_cnt = 0;

	if (trigger_cnt >= 3)
		vbat_low = true;
	dischg_term_loop_mode = sprd_fgu_get_dischg_term_loop_mode(data, vbat_avg_mv,
								   batt_soc, vbat_thd_mv);
	if (dischg_term_loop_mode == 1) {
		cutoff_cali_soc =
			sprd_fgu_dischg_term_loop_vbat(data, batt_soc, last_dischg_term_loop_mode,
						       vbat_avg_mv, vbat_cutoff_mv, vbat_low);
		data->dischg_term_loop_first2_in = 0;
		data->dischg_term_slow_down_ratio = 10;
		if (last_dischg_term_loop_mode == 1) {
			if (cutoff_cali_soc <= last_cutoff_cali_soc) {
				dev_info(data->dev, "cutoff1, cutoff_cali_soc = %d, last_cutoff_cali_soc = %d, smooth_soc = %d\n",
					 cutoff_cali_soc, last_cutoff_cali_soc, data->smooth_soc);
				if (change_soc != SPRD_FGU_MAGIC_NUMBER &&
				    cutoff_cali_soc <= change_soc) {
					last_cutoff_cali_soc = change_soc;
					change_soc = SPRD_FGU_MAGIC_NUMBER;
				}
				if (cutoff_cali_soc <= data->smooth_soc &&
				    last_cutoff_cali_soc >= data->smooth_soc) {
					last_cutoff_cali_soc = data->smooth_soc;
					last_acc_delta_cali_soc = 0;
				}
			} else if ((cutoff_cali_soc < data->smooth_soc) ||
				   (last_cutoff_cali_soc > data->smooth_soc)) {
				dev_info(data->dev, "cutoff2, cutoff_cali_soc = %d, last_cutoff_cali_soc = %d, smooth_soc = %d, last_acc_delta_cali_soc = %d\n",
					 cutoff_cali_soc, last_cutoff_cali_soc,
					 data->smooth_soc, last_acc_delta_cali_soc);
				if (!last_acc_delta_cali_soc) {
					recalc_charge_diff_cap = true;
					if (change_soc == SPRD_FGU_MAGIC_NUMBER)
						change_soc = last_cutoff_cali_soc;
				}
			} else if (last_cutoff_cali_soc <= data->smooth_soc &&
				   cutoff_cali_soc >= data->smooth_soc) {
				dev_info(data->dev, "cutoff3, cutoff_cali_soc = %d, last_cutoff_cali_soc = %d, smooth_soc = %d\n",
					 cutoff_cali_soc, last_cutoff_cali_soc, data->smooth_soc);
				last_cutoff_cali_soc = data->smooth_soc;
				last_acc_delta_cali_soc = 0;
				recalc_charge_diff_cap = true;
				if (change_soc == SPRD_FGU_MAGIC_NUMBER)
					change_soc = last_cutoff_cali_soc;
			} else if (last_cutoff_cali_soc == 0 && cutoff_cali_soc == 0 &&
				   data->smooth_soc > 0) {
				if (!last_acc_delta_cali_soc)
					recalc_charge_diff_cap = true;
			}

			if (!recalc_charge_diff_cap && change_soc == SPRD_FGU_MAGIC_NUMBER) {
				delta_cali_soc = (cutoff_cali_soc - last_cutoff_cali_soc) * 10 +
					last_acc_delta_cali_soc - *charge_diff_cap;
				if (cutoff_cali_soc < data->smooth_soc && delta_cali_soc <
				    (cutoff_cali_soc - data->smooth_soc) * 10)
					delta_cali_soc = (cutoff_cali_soc - data->smooth_soc) * 10;
				step_cap = sprd_fgu_cutoff_delta_cap(data, cutoff_cali_soc,
								     *charge_diff_cap,
								     delta_cali_soc);
				*charge_diff_cap += step_cap;
				last_acc_delta_cali_soc = delta_cali_soc - step_cap;
				dev_info(data->dev, "cutoff_cali_soc = %d. last_cutoff_cali_soc = %d, last_acc_delta_cali_soc = %d. *charge_diff_cap = %d, delta_cali_soc = %d, step_cap=%d\n",
					 cutoff_cali_soc, last_cutoff_cali_soc,
					 last_acc_delta_cali_soc, *charge_diff_cap,
					 delta_cali_soc, step_cap);
				if (last_acc_delta_cali_soc > 0)
					last_acc_delta_cali_soc = 0;
			}
		}
	} else if (dischg_term_loop_mode == 2) {
		cutoff_cali_soc = sprd_fgu_dischg_term_loop_soc(data, batt_soc,
								last_dischg_term_loop_mode,
								vbat_avg_mv, vbat_low);
		data->dischg_term_loop_first_in = 0;
		change_soc = SPRD_FGU_MAGIC_NUMBER;
		last_acc_delta_cali_soc = 0;
	} else {
		data->dischg_term_loop_first_in = 0;
		data->dischg_term_loop_first2_in = 0;
		data->dischg_term_slow_down_ratio = 10;
		trigger_cnt = 0;
		change_soc = SPRD_FGU_MAGIC_NUMBER;
		last_acc_delta_cali_soc = 0;
	}

	last_cutoff_cali_soc = cutoff_cali_soc;
	last_dischg_term_loop_mode = dischg_term_loop_mode;

	if (dischg_term_loop_mode != 0)
		sprd_fgu_cutoff_cali(data, cutoff_cali_soc, vbat_low);

	dev_info(data->dev, "batt_soc = %d, cutoff_cali_soc = %d, dischg_term_loop_mode = %d, vbat_avg_mv = %d, charge_diff_cap = %d, org_charge_diff_cap = %d, change_soc = %d\n",
		 batt_soc, cutoff_cali_soc, dischg_term_loop_mode, vbat_avg_mv,
		 *charge_diff_cap, org_charge_diff_cap, change_soc);

	return cutoff_cali_soc;
}

static int sprd_fgu_calc_soc(struct sprd_fgu_data *data, int *charge_diff_cap)
{
	int soc, rm_uah, cali_soc;

	rm_uah = sprd_fgu_calc_rm_uah(data);

	soc = sprd_fgu_calc_batt_soc(data, rm_uah);

	cali_soc = sprd_fgu_chg_term_loop(data, soc, charge_diff_cap);

	cali_soc = sprd_fgu_dischg_term_loop(data, cali_soc, charge_diff_cap);

	return cali_soc;
}

static void sprd_fgu_calc_cycles(struct sprd_fgu_data *data)
{
	int i;
	static int last_cc_uah = SPRD_FGU_MAGIC_NUMBER;
	int cur_cc_uah, ret, fcc_uah, delta_cap, delta_rm_uah = 0;
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	ret = fgu_info->ops->get_cc_uah(fgu_info, &cur_cc_uah, false);
	if (ret) {
		dev_err(data->dev, "failed to get cc uah!\n");
		return;
	}

	if (last_cc_uah == SPRD_FGU_MAGIC_NUMBER) {
		last_cc_uah = cur_cc_uah;
		return;
	}

	fcc_uah = sprd_fgu_get_fcc_uah(data, data->bat_temp, data->charge_cycle);

	delta_cap = (cur_cc_uah - last_cc_uah) / (fcc_uah/ 1000);

	if (delta_cap > 0) {
		delta_rm_uah = (cur_cc_uah - last_cc_uah) %  (fcc_uah / 1000);
		data->charge_cycle += delta_cap;
		last_cc_uah = cur_cc_uah - delta_rm_uah;
	} else if (delta_cap < 0) {
		last_cc_uah = cur_cc_uah;
	}
	dev_info(data->dev, "%s:delta_cap = %d, cur_cc_uah = %d, last_cc_uah = %d, delta_rm_uah = %d, fcc_uah = %d\n",
		__func__, delta_cap, cur_cc_uah, last_cc_uah, delta_rm_uah, fcc_uah);

	if (data->charge_cycle >= 0 && data->charge_cycle / 1000 > data->last_batt_cycle) {
		pr_info("battery cycle changed, last cc:%d, current cc: %d\n", data->last_batt_cycle, data->charge_cycle / 1000);
		data->last_batt_cycle = data->charge_cycle / 1000;
		for (i = 0; i < ARRAY_SIZE(batt_cycle); i++) {
			if (data->last_batt_cycle >= batt_cycle[i][0] && data->last_batt_cycle <= batt_cycle[i][1])
				break;
		}
		data->bat_soh = 100 - i;
		if (data->bat_soh < data->last_batt_soh) {
			pr_info("new battery soh:%d\n", data->bat_soh);
			data->last_batt_soh = data->bat_soh;
		}
		// check wether battery secret chip present
		if (CHIP_ONLINE == ll_check_secret_chip_online()) {
			// chip unavailable
			if (data->last_chip_cycle < 0) {
				pr_err("invalid last_chip_cycle:%d\n", data->last_chip_cycle);
				return;
			}
			data->last_chip_cycle++;
			pr_info("update cycle count to chip\n");
			schedule_delayed_work(&data->update_delay_work, 0);
		}
	}
}

static void update_cycle_work(struct work_struct *work)
{
	static int i, new_soh;
	static struct secret_device *master = NULL;
	struct sprd_fgu_data *data = container_of(work, struct sprd_fgu_data, update_delay_work.work);

	// battery not present
	if (!data->bat_present) {
		pr_err("battery not found\n");
		return;
	}
	if (!master)
		master = get_secret_by_name(MASTER_SECRET);
	if (IS_ERR_OR_NULL(master)) {
		pr_err("Failed to get master secret device\n");
		schedule_delayed_work(&data->update_delay_work, msecs_to_jiffies(500));
		return;
	}
	lc_set_cycle_count(data->last_chip_cycle);
	// chip unavailable
	if (data->last_chip_soh < 0) {
		pr_err("invalid last_chip_soh:%d\n", data->last_chip_soh);
		return;
	}
	for (i = 0; i < ARRAY_SIZE(batt_cycle); i++) {
		if (data->last_chip_cycle >= batt_cycle[i][0] && data->last_chip_cycle <= batt_cycle[i][1])
			break;
	}
	new_soh = 100 - i;
	if (new_soh < data->last_chip_soh) {
		pr_info("soh changed, %d->%d\n", data->last_chip_soh, new_soh);
		data->last_chip_soh = new_soh;
		lc_set_rawsoh(data->last_chip_soh);
	}
}

static int sprd_fgu_calc_delta_cap(struct sprd_fgu_data *data)
{
	static int last_cc_uah = SPRD_FGU_MAGIC_NUMBER;
	int cur_cc_uah, ret, fcc_uah, delta_cap, delta_rm_uah;
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	ret = fgu_info->ops->get_cc_uah(fgu_info, &cur_cc_uah, false);
	if (ret) {
		dev_err(data->dev, "failed to get cc uah!\n");
		return 0;
	}

	if (last_cc_uah == SPRD_FGU_MAGIC_NUMBER) {
		last_cc_uah = cur_cc_uah;
		return 0;
	}

	fcc_uah = sprd_fgu_get_fcc_uah(data, data->bat_temp, data->charge_cycle);
	/* delta_cap = DIV_ROUND_CLOSEST((cur_cc_uah - last_cc_uah), fcc_uah / 1000) */
	/* delta_cap *= 10 */
	delta_cap = (cur_cc_uah - last_cc_uah) * 10 / (fcc_uah / 1000);
	delta_rm_uah = (cur_cc_uah - last_cc_uah) * 10 % (fcc_uah / 1000);
	dev_info(data->dev, "%s:raw delta_cap = %d, delta_rm_uah = %d, cur_cc_uah = %d, last_cc_uah = %d\n",
		 __func__, delta_cap, delta_rm_uah, cur_cc_uah, last_cc_uah);

	if (data->dischg_term_slow_down_ratio == 0)
		data->dischg_term_slow_down_ratio = 10;
	if (abs(delta_cap) >= 3 * 10 /  data->dischg_term_slow_down_ratio) {
		last_cc_uah = cur_cc_uah - delta_rm_uah / 10;
		delta_cap = delta_cap * data->dischg_term_slow_down_ratio / 10;
		dev_info(data->dev, "%s:delta_cap = %d, dischg_term_slow_down_ratio = %d\n",
			 __func__, delta_cap, data->dischg_term_slow_down_ratio);
	} else {
		delta_cap = 0;
		dev_info(data->dev, "%s:delta_cap = %d\n", __func__, delta_cap);
	}

	return delta_cap;
}

static int sprd_fgu_get_lp_ocv_from_fifo(struct sprd_fgu_data *data, int *val, int *cur,
					 int current_thd_ma)
{
	int ret, i, cur_ma = 0x7fffffff, total_vol_mv = 0, total_cur_ma = 0, valid_cnt = 0;
	int vbat_range_mv = 0, vbat_min_mv = -1, vbat_max_mv = -1;
	u32 vol_mv = 0;
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	for (i = SPRD_FGU_VOLTAGE_BUFF_CNT - 1; i >= 0; i--) {
		vol_mv = 0;
		ret = fgu_info->ops->get_vbat_buf(fgu_info, i, &vol_mv);
		if (ret) {
			dev_info(data->dev, "%s sr_get_ocv: fail to get vbat_buf[%d]\n",
				 __func__, i);
			continue;
		}

		cur_ma = 0x7fffffff;
		ret = fgu_info->ops->get_current_buf(fgu_info, i, &cur_ma);
		if (ret) {
			dev_info(data->dev, "%s sr_get_ocv: fail to get cur_buf[%d]\n",
				 __func__, i);
			continue;
		}

		if (abs(cur_ma) > current_thd_ma) {
			dev_info(data->dev, "%s sr_get_ocv: get cur[%d] is invalid = %dmA\n",
				 __func__, i, cur_ma);
			continue;
		}

		if (vol_mv > SPRD_FGU_SR_MAX_VOL_MV || vol_mv < SPRD_FGU_SR_MIN_VOL_MV) {
			dev_info(data->dev, "%s sr_get_ocv: get vol[%d] is invalid = %dmV\n",
				 __func__, i, vol_mv);
			continue;
		}

		if (vbat_min_mv == -1 || vbat_min_mv > vol_mv)
			vbat_min_mv = vol_mv;

		if (vbat_max_mv == -1 || vbat_max_mv < vol_mv)
			vbat_max_mv = vol_mv;

		dev_info(data->dev, "%s sr_get_ocv: get index:[%d] valid current = %dmA, valid voltage = %dmV\n",
			 __func__, i, cur_ma, vol_mv);
		total_vol_mv += vol_mv;
		total_cur_ma += cur_ma;
		valid_cnt++;
	}

	if (valid_cnt < SPRD_FGU_SR_VALID_VOL_CNT) {
		dev_info(data->dev, "%s sr_get_ocv: fail to get cur and vol: cur = %dmA, vol = %dmV or valid_cnt = %d < %d!!!\n",
			 __func__, cur_ma, vol_mv, valid_cnt, SPRD_FGU_SR_VALID_VOL_CNT);
		return -EINVAL;
	}

	vbat_range_mv = vbat_max_mv - vbat_min_mv;
	if (vbat_range_mv > SPRD_FGU_SR_VALID_MAX_RANGE) {
		dev_info(data->dev, "%s sr_get_ocv: fail to get cur and vol: vbat_min = %dmV, vbat_max = %dmV, vbat_range =%d > %d!!!\n",
			 __func__, vbat_min_mv, vbat_max_mv, vbat_range_mv,
			 SPRD_FGU_SR_VALID_MAX_RANGE);
		return -EINVAL;
	}

	*val = total_vol_mv * 1000 / valid_cnt;
	*cur = total_cur_ma * 1000 / valid_cnt;

	dev_info(data->dev, "%s sr_get_ocv: vol = %duV, cur = %duA, valid_cnt = %d\n",
		  __func__, *val, *cur, valid_cnt);

	return 0;
}

static void sprd_fgu_calib_batt_ocv(struct sprd_fgu_data *data, int cap,
				    int ocv_uv, int first_poweron)
{
	int zero_soc_ocv_uv = 0, fcc1_uah, fcc2_uah, soc, delta_cc_uah = 0;
	struct timespec64 cur_time;

	cur_time = ktime_to_timespec64(ktime_get_boottime());
	if (ocv_uv) {
		zero_soc_ocv_uv = sprd_fgu_get_ocv_by_soc(data, data->bat_temp, 0, false);
		if (ocv_uv < zero_soc_ocv_uv && data->bat_temp < 100) {
			fcc1_uah = sprd_fgu_get_fcc_uah(data, data->bat_temp, data->charge_cycle);
			fcc2_uah = sprd_fgu_get_fcc_uah(data, SPRD_FGU_TEMP_DEFAULT,
							data->charge_cycle);
			soc = sprd_fgu_get_soc_by_ocv(data, ocv_uv, SPRD_FGU_TEMP_DEFAULT);
			delta_cc_uah = -(fcc2_uah - fcc1_uah - (fcc2_uah / 1000 * soc));
		}
		data->batt_ocv_uv = ocv_uv;
		if (!first_poweron)
			data->batt_ocv_times = cur_time.tv_sec;
	} else {
		data->batt_ocv_uv = sprd_fgu_get_ocv_by_soc(data, data->bat_temp, cap, true);
	}
	if (!first_poweron)
		data->cali_ocv_times = cur_time.tv_sec;
	data->init_cc_uah = delta_cc_uah;
	data->err_cc_uah = 0;
	data->err_fcc_uah = 0;
	data->err_cutoff_uah = 0;
	data->err_rt_cc_uah = 0;
	data->batt_ocv_temp = data->bat_temp;
	data->fgu_info->ops->adjust_cap(data->fgu_info, 0);
	sprd_fgu_save_last_batt_ocv(data, data->batt_ocv_uv, first_poweron);
	sprd_fgu_save_last_batt_temp(data, data->bat_temp);
	sprd_fgu_save_last_shutdown_batt_temp(data, data->bat_temp);
	sprd_fgu_save_last_cc_uah(data, delta_cc_uah);
}

static void sprd_fgu_check_full_ocv(struct sprd_fgu_data *data)
{
	struct timespec64 cur_time;
	struct sprd_fgu_vbat_info *vbat_info = &data->vbat_info;
	static int full_interval_cnt_0, full_interval_cnt_1;
	static bool full_ocv_flag;
	int ret, full_ocv_uv, ocv_uv, full_ocv_cur_ua;
	s64 full_times;

	dev_info(data->dev, "full_ocv_start = %d, chg_sts = %d, online = %d\n",
		 data->full_ocv_start, data->chg_sts, data->online);

	if (!data->full_ocv_start) {
		full_interval_cnt_0 = 0;
		full_interval_cnt_1 = 0;
		full_ocv_flag = false;
		data->full_ocv_uv = 0;
		return;
	}

	if (data->chg_sts == POWER_SUPPLY_STATUS_CHARGING)
		data->full_ocv_cnt++;
	else
		data->full_ocv_cnt = 0;
	if (data->full_ocv_cnt >= SPRD_FGU_FULL_OCV_TRIG_CNT || !data->online) {
		data->full_ocv_start = false;
		data->full_ocv_cnt = 0;
		data->full_ocv_uv = 0;
		full_ocv_flag = false;
		full_interval_cnt_0 = 0;
		full_interval_cnt_1 = 0;
		dev_info(data->dev, "%s: full_ocv_cnt = %d, exit\n", __func__, data->full_ocv_cnt);
		return;
	}

	cur_time = ktime_to_timespec64(ktime_get_boottime());

	if (abs(data->ibat_avg_work_cycle_ma) > SPRD_FGU_FULL_CHARGING_MA ||
	    abs(vbat_info->vbat_cur_avg_ma) > SPRD_FGU_FULL_CHARGING_MA ||
	    abs(vbat_info->vbat_cur_ma) > SPRD_FGU_FULL_CHARGING_MA) {
		data->full_ocv_enter_times = cur_time.tv_sec;
		full_ocv_flag = false;
		full_interval_cnt_0 = 0;
		full_interval_cnt_1 = 0;
		data->full_ocv_uv = 0;
		return;
	}

	full_times = cur_time.tv_sec - data->full_ocv_enter_times;

	if (full_times >= SPRD_FGU_FULL_OCV_VALID_ENTER_TIME)
		full_ocv_flag = true;

	if (!full_ocv_flag && full_times < SPRD_FGU_FULL_OCV_VALID_CHECK_TIME +
	    full_interval_cnt_0 * SPRD_FGU_FULL_OCV_INTERVAL_TIME)
		return;
	if (!full_ocv_flag)
		full_interval_cnt_0++;

	if (full_ocv_flag && full_times < SPRD_FGU_FULL_OCV_VALID_ENTER_TIME +
	    full_interval_cnt_1 * SPRD_FGU_OCV_INTERVAL_TIME)
		return;
	if (full_ocv_flag)
		full_interval_cnt_1++;

	ret = sprd_fgu_get_lp_ocv_from_fifo(data, &full_ocv_uv, &full_ocv_cur_ua,
					    SPRD_FGU_FULL_CHARGING_MA);
	if (ret) {
		dev_err(data->dev, "%s full calib: failed get full_ocv_uv!!\n", __func__);
		return;
	}

	if (!full_ocv_flag) {
		if (abs(full_ocv_uv - data->full_ocv_uv) < 2000)
			return;
		ocv_uv = sprd_fgu_get_ocv_by_soc(data, data->bat_temp,
						 SPRD_FGU_FCC_PERCENT, true);
		if (full_ocv_uv >= ocv_uv)
			return;
	}

	if (full_ocv_flag) {
		if (is_between(data->bat_temp, SPRD_FGU_TRACK_LOW_TEMP_THRESHOLD,
			       SPRD_FGU_TRACK_HIGH_TEMP_THRESHOLD)) {
			data->track.lpocv_info.valid = true;
			data->track.lpocv_info.ocv_uv = full_ocv_uv;
			data->track.lpocv_info.ocv_time_stamp = cur_time.tv_sec;
		}
	}

	data->full_ocv_uv = full_ocv_uv;
	dev_info(data->dev, "%s full calib: full_ocv_uv = %d\n", __func__, full_ocv_uv);

	sprd_fgu_calib_batt_ocv(data, 0, data->full_ocv_uv, 0);
}

static void sprd_fgu_check_charge_ocv(struct sprd_fgu_data *data)
{
	struct timespec64 cur_time;
	struct sprd_fgu_vbat_info *vbat_info = &data->vbat_info;
	static int chg_interval_cnt;
	int ret, stop_chg_ocv_uv, stop_chg_ocv_cur_ua, stop_ocv_cap, high_dens_threshold = 0;
	s64 stop_charge_time;

	dev_info(data->dev, "charge_ocv_start = %d, chg_sts = %d, online = %d, bat_temp = %d\n",
		 data->charge_ocv_start, data->chg_sts, data->online, data->bat_temp);

	if (!data->charge_ocv_start) {
		chg_interval_cnt = 0;
		return;
	}

	if (data->full_ocv_start ||
	    data->chg_sts == POWER_SUPPLY_STATUS_CHARGING || !data->online) {
		data->charge_ocv_start = false;
		chg_interval_cnt = 0;
		return;
	}

	cur_time = ktime_to_timespec64(ktime_get_boottime());

	if (abs(data->ibat_avg_work_cycle_ma) > SPRD_FGU_FULL_CHARGING_MA ||
	    abs(vbat_info->vbat_cur_avg_ma) > SPRD_FGU_FULL_CHARGING_MA ||
	    abs(vbat_info->vbat_cur_ma) > SPRD_FGU_FULL_CHARGING_MA) {
		data->charge_ocv_enter_times = cur_time.tv_sec;
		chg_interval_cnt = 0;
		return;
	}

	if (!is_between(data->bat_temp, SPRD_FGU_STOP_CHG_L_TEMP_THR,
			SPRD_FGU_STOP_CHG_H_TEMP_THR)) {
		data->charge_ocv_enter_times = cur_time.tv_sec;
		chg_interval_cnt = 0;
		return;
	}

	stop_charge_time = cur_time.tv_sec - data->charge_ocv_enter_times;

	if (stop_charge_time < SPRD_FGU_STOP_CHG_VALID_ENTER_TIME +
	    chg_interval_cnt * SPRD_FGU_STOP_CHG_VALID_ENTER_TIME)
		return;

	chg_interval_cnt++;

	ret = sprd_fgu_get_lp_ocv_from_fifo(data, &stop_chg_ocv_uv, &stop_chg_ocv_cur_ua,
					    SPRD_FGU_FULL_CHARGING_MA);
	if (ret) {
		dev_err(data->dev, "%s full calib: failed get stop_chg_ocv_uv!!\n", __func__);
		return;
	}

	dev_info(data->dev, "%s stop_chg calib: stop_chg_ocv_uv = %d, stop_charge_time = %lld, chg_interval_cnt = %d\n",
		 __func__, stop_chg_ocv_uv, stop_charge_time, chg_interval_cnt);

	if (sprd_fgu_is_in_low_energy_dens(data, stop_chg_ocv_uv)) {
		if (is_between(data->bat_temp, SPRD_FGU_TRACK_LOW_TEMP_THRESHOLD,
			       SPRD_FGU_TRACK_HIGH_TEMP_THRESHOLD)) {
			data->track.lpocv_info.valid = true;
			data->track.lpocv_info.ocv_uv = stop_chg_ocv_uv;
			data->track.lpocv_info.ocv_time_stamp = cur_time.tv_sec;
		}
		sprd_fgu_calib_batt_ocv(data, 0, stop_chg_ocv_uv, 0);
	} else {
		stop_ocv_cap = sprd_fgu_get_soc_by_ocv(data, stop_chg_ocv_uv, data->bat_temp);

		if (data->bat_temp >= SPRD_FGU_CAP_CALIB_NORMAL_TEMP_THR)
			high_dens_threshold = SPRD_FGU_CALIB_PRECI_H_DENS_NOR_TEMP_CAP_DIFF;
		else if (data->bat_temp >= SPRD_FGU_CAP_CALIB_LOW_TEMP_THR)
			high_dens_threshold = SPRD_FGU_CALIB_PRECI_H_DENS_LOW_TEMP_CAP_DIFF;
		else
			high_dens_threshold = SPRD_FGU_CALIB_PRECI_H_DENS_COOL_TEMP_CAP_DIFF;

		if (stop_ocv_cap - high_dens_threshold > data->batt_rm_soc) {
			data->batt_rm_soc = stop_ocv_cap - high_dens_threshold;
			sprd_fgu_calib_batt_ocv(data, data->batt_rm_soc, 0, 0);
		} else if (stop_ocv_cap + high_dens_threshold < data->batt_rm_soc) {
			data->batt_rm_soc = stop_ocv_cap + high_dens_threshold;
			sprd_fgu_calib_batt_ocv(data, data->batt_rm_soc, 0, 0);
		}
	}
}

static int sprd_fgu_get_boot_voltage(struct sprd_fgu_data *data, int *pocv_uv)
{
	int vol_mv, oci_ma, ret, ocv_mv, fgu_sts;
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	ret = fgu_info->ops->get_poci(fgu_info, &oci_ma);
	if (ret) {
		dev_err(data->dev, "Failed to get poci, ret = %d\n", ret);
		return ret;
	}

	ret = fgu_info->ops->get_pocv(fgu_info, &vol_mv);
	if (ret) {
		dev_err(data->dev, "Failed to get pocv, ret = %d\n", ret);
		return ret;
	}

	*pocv_uv = vol_mv * 1000;

	ret = fgu_info->ops->get_fgu_sts(fgu_info, SPRD_FGU_INVALID_POCV_STS_CMD, &fgu_sts);
	if (ret)
		return ret;

	if (fgu_info->ops->get_pocv_times)
		ret = fgu_info->ops->get_pocv_times(fgu_info, &data->pocv_time);

	data->poci_ma = oci_ma;
	data->invalid_pocv = !!fgu_sts;
	if (vol_mv < SPRD_FGU_POCV_VOLT_THRESHOLD || data->invalid_pocv ||
	    data->pocv_time > SPRD_FGU_POCV_VALID_TIMER_THRESHOLD ||
	    data->poci_ma > SPRD_FGU_POCI_VALID_THRESHOLD || data->is_reboot) {
		dev_info(data->dev, "pocv is %s\n", data->invalid_pocv ? "invalid" : "valid");
		ret = sprd_fgu_get_vbat_ocv(data, &ocv_mv);
		if (ret) {
			dev_err(data->dev, "Failed to read volt, ret = %d\n", ret);
			return ret;
		}
		*pocv_uv = ocv_mv * 1000;
		data->sw_pocv_flag = true;
	}
	dev_info(data->dev, "oci_ma = %d, vol_mv = %d, pocv = %d, pocv_time = %d, sw_pocv_flag = %d\n",
		 oci_ma, vol_mv, *pocv_uv, data->pocv_time, data->sw_pocv_flag);

	return 0;
}

static void sprd_fgu_boot_cap_calibration(struct sprd_fgu_data *data, int pocv_cap,
					  int pocv_uv, int *cap, bool *is_boot_calib)
{
	s64 cur_time, shutdown_time;
	int ret, high_dens_threshold = 0, pocv_err_cap = 0, org_cap = *cap;

	if (data->is_reboot && (!data->pocv_time || data->pocv_time >
				SPRD_FGU_REBOOT_POCV_VALID_TIMER_THR)) {
		dev_warn(data->dev, "Boot calib: is_reboot, not support boot calibration !!!!\n");
		return;
	}

	if (data->sw_pocv_flag) {
		dev_warn(data->dev, "Boot calib: sw pocv not support boot calibration!\n");
		return;
	}

	if (data->shutdown_rtc_time == 0 || data->shutdown_rtc_time == -1 ||
	    data->shutdown_rtc_time < SPRD_FGU_MISCDATA_RTC_TIME) {
		dev_err(data->dev, "Boot calib: shutdown_rtc_time = %lld not meet\n",
			data->shutdown_rtc_time);
		return;
	}

	if (data->bat_temp <= SPRD_FGU_CAP_CALIB_TEMP_MIN) {
		dev_err(data->dev, "Boot calib: temp = %d out range\n", data->bat_temp);
		return;
	}

	ret = sprd_fgu_get_rtc_time(data, &cur_time);
	if (ret)
		return;

	if (cur_time < SPRD_FGU_MISCDATA_RTC_TIME) {
		dev_err(data->dev, "Boot calib: current rtc time = %lld less than %d\n",
			cur_time, SPRD_FGU_MISCDATA_RTC_TIME);
		return;
	}

	shutdown_time = cur_time - data->shutdown_rtc_time;
	if (shutdown_time < SPRD_FGU_SHUTDOWN_TIME) {
		dev_err(data->dev, "Boot calib: shutdown time = %lld not meet\n", shutdown_time);
		return;
	}

	if (sprd_fgu_is_in_low_energy_dens(data, pocv_uv)) {
		dev_info(data->dev, "Boot calib: pocv_uv is in low energy dens!!!!\n");
		data->track.pocv_info.is_low_density = true;
	}

	if (data->bat_temp >= SPRD_FGU_CAP_CALIB_NORMAL_TEMP_THR)
		high_dens_threshold = SPRD_FGU_CALIB_PRECI_H_DENS_NOR_TEMP_CAP_DIFF;
	else if (data->bat_temp >= SPRD_FGU_CAP_CALIB_LOW_TEMP_THR)
		high_dens_threshold = SPRD_FGU_CALIB_PRECI_H_DENS_LOW_TEMP_CAP_DIFF;
	else
		high_dens_threshold = SPRD_FGU_CALIB_PRECI_H_DENS_COOL_TEMP_CAP_DIFF;

	if (data->track.pocv_info.is_low_density) {
		if (is_between(data->bat_temp, SPRD_FGU_TRACK_LOW_TEMP_THRESHOLD,
			       SPRD_FGU_TRACK_HIGH_TEMP_THRESHOLD)) {
			data->track.pocv_info.is_low_density = false;
			data->track.pocv_info.valid = true;
			data->track.pocv_info.ocv_uv = pocv_uv;
			data->track.pocv_info.ocv_time_stamp = cur_time;
		}
		*is_boot_calib = true;
		sprd_fgu_calib_batt_ocv(data, 0, pocv_uv, 0);
	} else {
		if (pocv_cap + high_dens_threshold < *cap) {
			*cap = pocv_cap + high_dens_threshold;
			*is_boot_calib = true;
			sprd_fgu_calib_batt_ocv(data, *cap, 0, 0);
		} else {
			pocv_err_cap =
				sprd_fgu_get_soc_by_ocv(data, pocv_uv - 30 * 1000, data->bat_temp);

			if (*cap < pocv_err_cap &&
			    pocv_cap - high_dens_threshold <= pocv_err_cap) {
				*cap = pocv_err_cap;
				*is_boot_calib = true;
				sprd_fgu_calib_batt_ocv(data, *cap, 0, 0);
			} else if (pocv_cap - high_dens_threshold > *cap) {
				*cap = pocv_cap - high_dens_threshold;
				*is_boot_calib = true;
				sprd_fgu_calib_batt_ocv(data, *cap, 0, 0);
			}
		}
	}

	dev_info(data->dev, "Boot calib: pocv_cap = %d, new_rm_soc = %d, org_cap = %d, high_dens_threshold = %d, pocv_err_cap = %d\n",
		 pocv_cap, *cap, org_cap, high_dens_threshold, pocv_err_cap);
}

static void sprd_fgu_boot_accuracy_calib(struct sprd_fgu_data *data)
{
	struct sprd_fgu_vbat_info *vbat_info = &data->vbat_info;
	int vbat_soc;

	if (data->bat_temp < SPRD_FGU_BOOT_ACCUR_CALIB_TEMP) {
		dev_warn(data->dev, "Boot accuracy calib: batt temp not support boot_accuracy calibration!\n");
		return;
	}

	if (vbat_info->vbat_cur_ma >= 0 || vbat_info->vbat_cur_avg_ma >= 0) {
		dev_warn(data->dev, "Boot accuracy calib: batt cur not support boot_accuracy calibration!\n");
		return;
	}

	vbat_soc = sprd_fgu_get_soc_by_ocv(data, vbat_info->vbat_avg_mv * 1000, data->bat_temp);

	if (data->batt_raw_rm_soc < 0 && vbat_soc > data->batt_rm_soc) {
		data->err_cc_uah += data->batt_rm_uah;
		dev_info(data->dev, "Boot accuracy calib: vbat_soc = %d, vbat_avg_mv = %d, batt_rm_soc = %d, batt_rm_uah = %d, err_cc_uah = %d\n",
			 vbat_soc, vbat_info->vbat_avg_mv, data->batt_rm_soc,
			 data->batt_rm_uah, data->err_cc_uah);
	}
}

static bool sprd_fgu_boot_cap_update(struct sprd_fgu_data *data)
{
	s64 cur_time, shutdown_time, shutdown_time_thd;
	int ret;

	if (data->shutdown_rtc_time == 0 || data->shutdown_rtc_time == -1 ||
	    data->shutdown_rtc_time < SPRD_FGU_MISCDATA_RTC_TIME || data->shutdown_time_thd == 0) {
		dev_err(data->dev, "Boot update: shutdown_rtc_time = %lld not meet\n",
		data->shutdown_rtc_time);
		return false;
	}

	ret = sprd_fgu_get_rtc_time(data, &cur_time);
	if (ret)
		return false;

	if (cur_time < SPRD_FGU_MISCDATA_RTC_TIME) {
		dev_err(data->dev, "Boot update: current rtc time = %lld less than %d\n",
			cur_time, SPRD_FGU_MISCDATA_RTC_TIME);
		return false;
	}

	shutdown_time_thd = data->shutdown_time_thd * SPRD_FGU_SHUTDOWN_DAY_TIME;

	shutdown_time = cur_time - data->shutdown_rtc_time;
	if (shutdown_time < shutdown_time_thd) {
		dev_err(data->dev, "Boot update: shutdown time = %lld not meet\n", shutdown_time);
		return false;
	}

	return true;
}

static bool sprd_fgu_check_boot_ocv_invalid(struct sprd_fgu_data *data, int ocv_reg, int ocv_magic,
					    int ccmah_reg, int ccmah_magic)
{
	if ((ocv_reg == SPRD_FGU_OCV_REG_DEFAUL || ccmah_reg == SPRD_FGU_OCV_REG_DEFAUL) ||
	    (ocv_magic != ccmah_magic) || (ocv_magic == 0x03) || (ocv_magic == 0) ||
	    (ocv_reg == 0) || (ocv_reg == -1) ||
	    (ccmah_reg == 0) || (ccmah_reg == -1) ||
	    (data->batt_ocv_uv == SPRD_FGU_BATT_OCV_DEFAUL) ||
	    (data->batt_ocv_uv < SPRD_FGU_BATT_OCV_UV_LOW_LIMIT) ||
	    (data->batt_ocv_uv > SPRD_FGU_BATT_OCV_UV_UP_LIMIT) ||
	    (data->init_cc_uah == SPRD_FGU_INIT_CC_DEFAUL))
		return true;

	return false;
}

static bool sprd_fgu_check_smooth_soc_invalid(struct sprd_fgu_data *data,
							int smooth_soc_reg,
							int smooth_soc_magic,
                                                        int ocv_magic,
							int last_smooth_soc)
{
	if (smooth_soc_reg == SPRD_FGU_DEFAULT_CAP || (smooth_soc_magic == 0) ||
	    (smooth_soc_reg == 0) || (smooth_soc_reg == -1) || (ocv_magic != smooth_soc_magic) ||
            (last_smooth_soc < 0) || (last_smooth_soc > 1000))
		return true;

	return false;
}

static int sprd_fgu_boot_not_first_poweron(struct sprd_fgu_data *data,
					   int pocv_uv, int pocv_cap)
{
	struct sprd_fgu_info *fgu_info = data->fgu_info;
	int ret, boot_soc, ocv_reg = 0, ocv_magic = 0, ccmah_reg = 0, ccmah_magic = 0;
	int smooth_soc_reg = 0, smooth_soc_magic = 0;
	int tbatt_reg, tbatt_magic, tbat_reg, tbat_magic, boot_cap_calib_soc;
	int charge_diff_cap, last_batt_temp, cap, last_smooth_soc;
	bool boot_ocv_invalid = false, is_boot_calib = false, smooth_soc_invalid = false;

	ret = fgu_info->ops->read_last_cap(fgu_info, &cap);
	if (ret) {
		dev_err(data->dev, "Failed to read last cap, ret = %d\n", ret);
		return ret;
	}

	sprd_fgu_read_last_batt_ocv(data, &data->batt_ocv_uv, &ocv_reg, &ocv_magic);
	sprd_fgu_read_last_cc_uah(data, &data->init_cc_uah, &ccmah_reg, &ccmah_magic);
	sprd_fgu_read_last_batt_temp(data, &data->batt_ocv_temp, &tbatt_reg, &tbatt_magic);
	sprd_fgu_read_last_shutdown_batt_temp(data, &last_batt_temp, &tbat_reg, &tbat_magic);
	sprd_fgu_read_last_smooth_soc(data, &last_smooth_soc, &smooth_soc_reg, &smooth_soc_magic);
	dev_info(data->dev, "%s:ocv_magic = 0x%x, ccmah_magic = 0x%x, tbatt_magic = 0x%x, tbat_magic = 0x%x smooth_soc_magic = 0x%x\n",
		 __func__, ocv_magic, ccmah_magic, tbatt_magic, tbat_magic, smooth_soc_magic);
	dev_info(data->dev, "%s:ocv_reg = 0x%x, ccmah_reg = 0x%x, tbatt_reg = 0x%x, tbat_reg = 0x%x, smooth_soc_reg = 0x%x\n",
		 __func__, ocv_reg, ccmah_reg, tbatt_reg, tbat_reg, smooth_soc_reg);
	dev_info(data->dev, "%s:batt_ocv_uv = %d, init_cc_uah = %d, batt_ocv_temp = %d, last_batt_temp = %d last_smooth_soc = %d\n",
		 __func__, data->batt_ocv_uv, data->init_cc_uah,
		 data->batt_ocv_temp, last_batt_temp, last_smooth_soc);
	boot_ocv_invalid = sprd_fgu_check_boot_ocv_invalid(data, ocv_reg, ocv_magic,
							   ccmah_reg, ccmah_magic);
	if (boot_ocv_invalid) {
		dev_info(data->dev, "%s:last_batt_ocv error, use pocv_uv = %d, sw_pocv_flag = %d\n",
			 __func__, pocv_uv, data->sw_pocv_flag);
		data->batt_ocv_uv = pocv_uv;
		sprd_fgu_calib_batt_ocv(data, 0, data->batt_ocv_uv, 1);
		data->sw_pocv_flag = true;
	}

	boot_soc = sprd_fgu_calc_soc(data, &charge_diff_cap);
	dev_info(data->dev, "%s:boot_soc = %d\n", __func__, boot_soc);

	data->boot_cap = cap;
	if ((!boot_ocv_invalid && data->boot_temp_thd > 0 &&
	     abs(last_batt_temp - data->bat_temp) > data->boot_temp_thd) ||
	    (cap < 0 || cap > 1000))
		data->boot_cap = boot_soc;

	sprd_fgu_battery_soc_limit(data, &data->boot_cap, false);
	data->smooth_soc = data->boot_cap;

	smooth_soc_invalid = sprd_fgu_check_smooth_soc_invalid(data, smooth_soc_reg, smooth_soc_magic, ocv_magic, last_smooth_soc);
    if(!boot_ocv_invalid && !smooth_soc_invalid)
            data->smooth_soc = last_smooth_soc;
    else
            data->smooth_soc = boot_soc;

	boot_cap_calib_soc = data->batt_rm_soc;
	if (data->batt_raw_rm_soc < 0)
		boot_cap_calib_soc = data->batt_raw_rm_soc;

	sprd_fgu_boot_cap_calibration(data, pocv_cap, pocv_uv,
				      &boot_cap_calib_soc, &is_boot_calib);
	data->batt_rm_soc = boot_cap_calib_soc;

	if (!is_boot_calib)
		sprd_fgu_boot_accuracy_calib(data);

	dev_info(data->dev, "boot_ui_cap = %d, boot_rm_soc = %d, shutdown_ui_cap = %d, battery_soc = %d\n",
		 data->boot_cap, data->batt_rm_soc, cap, boot_soc);

	return fgu_info->ops->save_boot_mode(fgu_info, SPRD_FGU_NORMAL_POWERON);
}

static int sprd_fgu_boot_first_poweron(struct sprd_fgu_data *data,
				       int pocv_uv, int pocv_cap)
{
	struct sprd_fgu_info *fgu_info = data->fgu_info;
	int ret, boot_soc;
	int charge_diff_cap;

	sprd_fgu_calib_batt_ocv(data, 0, pocv_uv, 1);
	boot_soc = sprd_fgu_calc_soc(data, &charge_diff_cap);
	data->batt_ocv_temp = data->bat_temp;
	dev_info(data->dev, "boot_soc = %d\n", boot_soc);

	data->boot_cap = sprd_fgu_capacity_remap(data, boot_soc);
	sprd_fgu_battery_soc_limit(data, &data->boot_cap, false);
	data->smooth_soc = data->boot_cap;
	ret = sprd_fgu_save_last_ui_soc(data, data->boot_cap);
	if (ret)
		return ret;


	ret = sprd_fgu_save_last_smooth_soc(data, data->smooth_soc);
	if (ret)
		return ret;

	data->is_first_poweron = true;
	dev_info(data->dev, "First_poweron: pocv_uv = %d, pocv_cap = %d, boot_ui_cap = %d, sw_pocv_flag = %d\n",
		 pocv_uv, pocv_cap, data->boot_cap, data->sw_pocv_flag);

	return fgu_info->ops->save_boot_mode(fgu_info, SPRD_FGU_NORMAL_POWERON);
}

/*
 * When system boots on, we can not read battery capacity from coulomb
 * registers, since now the coulomb registers are invalid. So we should
 * calculate the battery open circuit voltage, and get current battery
 * capacity according to the capacity table.
 */
static int sprd_fgu_get_boot_capacity(struct sprd_fgu_data *data)
{
	int pocv_uv, ret, pocv_cap;
	struct sprd_fgu_info *fgu_info = data->fgu_info;
	bool is_first_poweron = fgu_info->ops->is_first_poweron(fgu_info);

	ret = sprd_fgu_get_boot_voltage(data, &pocv_uv);
	if (ret) {
		dev_err(data->dev, "Failed to get boot voltage, ret = %d\n", ret);
		return ret;
	}
	data->boot_volt_uv = pocv_uv;
	data->vbat_info.vbat_mv = data->boot_volt_uv / 1000;
	data->batt_ibat_avg_ma = SPRD_FGU_CHARGING_IAVG_MA;

	/*
	 * Parse the capacity table to look up the correct capacity percent
	 * according to current battery's corresponding OCV values.
	 */
	pocv_cap = sprd_fgu_get_soc_by_ocv(data, pocv_uv, data->bat_temp);

	sprd_fgu_get_vbat_info(data);

	if (sprd_fgu_boot_cap_update(data) && !data->invalid_pocv && !data->sw_pocv_flag) {
		is_first_poweron = true;
		dev_info(data->dev, "shutdown %d days, use pocv for startup cap\n",
			 data->shutdown_time_thd);
	}

	/*
	 * If system is not the first power on, we should use the last saved
	 * battery capacity as the initial battery capacity. Otherwise we should
	 * re-calculate the initial battery capacity.
	 */

	if (!is_first_poweron)
		return sprd_fgu_boot_not_first_poweron(data, pocv_uv, pocv_cap);
	else
		return sprd_fgu_boot_first_poweron(data, pocv_uv, pocv_cap);
}

static void sprd_fgu_get_vbat_info(struct sprd_fgu_data *data)
{
	int ret, ocv_mv;
	struct sprd_fgu_info *fgu_info = data->fgu_info;
	struct sprd_fgu_vbat_info *vbat_info = &data->vbat_info;

	ret = sprd_fgu_get_vbat_ocv(data, &ocv_mv);
	if (ret) {
		dev_err(data->dev, "get_vbat_ocv error.\n");
		vbat_info->vbat_err |= OCV_ERR;
	} else {
		vbat_info->ocv_uv = ocv_mv * 1000;
	}

	ret = fgu_info->ops->get_vbat_now(fgu_info, &vbat_info->vbat_mv);
	if (ret) {
		dev_err(data->dev, "get_vbat_now error, ret = %d\n", ret);
		vbat_info->vbat_err |= VBAT_ERR;
	}

	ret = fgu_info->ops->get_current_now(fgu_info, &vbat_info->vbat_cur_ma);
	if (ret) {
		dev_err(data->dev, "get_current_now error, ret = %d\n", ret);
		vbat_info->vbat_err |= VBAT_CUR_ERR;
	}

	ret = fgu_info->ops->get_vbat_avg(fgu_info, &vbat_info->vbat_avg_mv);
	if (ret) {
		dev_err(data->dev, "get_vbat_avg error.\n");
		vbat_info->vbat_err |= VBAT_AVG_ERR;
	}

	ret = fgu_info->ops->get_current_avg(fgu_info, &vbat_info->vbat_cur_avg_ma);
	if (ret) {
		dev_err(data->dev, "get_current_avg error, ret = %d\n", ret);
		vbat_info->vbat_err |= VBAT_CUR_AVG_ERR;
	}

	if (!sprd_fgu_discharging_trend(data) && data->chg_sts == POWER_SUPPLY_STATUS_CHARGING)
		vbat_info->absolute_charger_mode = true;
	else
		vbat_info->absolute_charger_mode = false;

	vbat_info->zp_vol_uv = sprd_fgu_get_zp_voltage_by_temp(data, data->bat_temp);

	vbat_info->vbat_avg_mv_dy = sprd_fgu_get_average_vbat_dy(data, vbat_info->vbat_avg_mv,
								 data->dischg_term_loop_mode);
}

#define FFC_SMOOTH_LEN		3
struct ffc_smooth {
	int curr_lim;
	int time;
};
struct ffc_smooth ffc_dischg_smooth_normal[FFC_SMOOTH_LEN] = {
	{ 1000, 5000 },
	{ 1500, 3000 },
	{ 2000, 1000 },
};
struct ffc_smooth ffc_dischg_smooth_low[FFC_SMOOTH_LEN] = {
	{ 900, 5000 },
	{ 1200, 3000 },
	{ 1500, 1000 },
};

static int calc_delta_time(ktime_t time_last, int *delta_time)
{
	ktime_t time_now;

	time_now = ktime_get();

	*delta_time = ktime_ms_delta(time_now, time_last);
	if (*delta_time < 0)
		*delta_time = 0;

	return 0;
}

static void battery_soc_smooth_tracking_not_eea(struct sprd_fgu_data *data)
{
	static int system_soc, last_system_soc;
	int soc_changed = 0, unit_time = 10000, delta_time = 0, soc_delta = 0;
	static ktime_t last_change_time = -1;
	int change_delta = 0;
	int i = 0;
	static int ibat_pos_count = 0;
	struct timespec64 time;
	ktime_t tmp_time = 0;
	struct power_supply *bat_psy = NULL;
	union power_supply_propval val;
	int ret = 0;
	int raw_soc = 0;

	bat_psy = power_supply_get_by_name("battery");
	if (IS_ERR_OR_NULL(bat_psy)) {
		pr_err("%s, get battery psy fail\n", __func__);
		data->param.batt_status = POWER_SUPPLY_STATUS_DISCHARGING;
		goto skip;
	}

	ret = power_supply_get_property(bat_psy, POWER_SUPPLY_PROP_STATUS, &val);
	if(ret){
		dev_err(data->dev, "Failed to get battery status, ret = %d\n", ret);
		data->param.batt_status = POWER_SUPPLY_STATUS_DISCHARGING;
		goto skip;
	}

	data->param.batt_status = val.intval;

skip:

	tmp_time = ktime_get_boottime();
	time = ktime_to_timespec64(tmp_time);
	raw_soc = data->param.batt_raw_soc * 100;

	if ((data->param.batt_ma > 0) && (ibat_pos_count < 10))
		ibat_pos_count++;
	else if (data->param.batt_ma <= 0)
		ibat_pos_count = 0;

	/*Map system_soc value according to raw_soc */
	if (raw_soc >= 9700)
		system_soc = 100;
	else {
		system_soc = ((raw_soc + 96) / 97);
		if (system_soc > 99)
			system_soc = 100;
	}

	/*Get the initial value for the first time */
	if (last_change_time == -1) {
		last_change_time = ktime_get();
		if (system_soc != 0){
			last_system_soc = system_soc;
		}else
			last_system_soc = data->param.batt_raw_soc;
	}

	if ((data->param.batt_status == POWER_SUPPLY_STATUS_DISCHARGING ||
		data->param.batt_status == POWER_SUPPLY_STATUS_NOT_CHARGING) &&
		last_system_soc >= 1) {
		if(data->param.batt_temp > 0){
			for (i = FFC_SMOOTH_LEN - 1; i >= 0; i--) {
				if (abs(data->param.batt_ma_avg) > ffc_dischg_smooth_normal[i].curr_lim) {
					unit_time = ffc_dischg_smooth_normal[i].time;
					break;
				}
			}
		}else{
			for (i = FFC_SMOOTH_LEN - 1; i >= 0; i--) {
				if (abs(data->param.batt_ma_avg) > ffc_dischg_smooth_low[i].curr_lim) {
					unit_time = ffc_dischg_smooth_low[i].time;
					break;
				}
			}
		}
		pr_info("%s: adjust smooth unit_time=%d batt_ma_avg(abs)=%d, batt_temp:%d \n", __func__,
			unit_time, abs(data->param.batt_ma_avg), data->param.batt_temp);
	}

	/*If the soc jump, will smooth one cap every 10S */
	soc_delta = abs(system_soc - last_system_soc);
	if (soc_delta > 1 || (data->param.batt_volt < 3400 && system_soc > 0) ||
		(unit_time != 10000 && soc_delta == 1)) {
		//unit_time != 10000 && soc_delta == 1 fix low temperature 2% jump to 0%
		calc_delta_time(last_change_time, &change_delta);
		delta_time = change_delta / unit_time;
          	pr_info("%s: fg jump: last_change_time: %lld, change_delta:%d, unit_time:%d, delta_time:%d\n", __func__,
			last_change_time, change_delta, unit_time, delta_time);
		if (delta_time < 0) {
			last_change_time = ktime_get();
			delta_time = 0;
		}

		soc_changed = min(1, delta_time);
		if (soc_changed) {
			if (data->param.batt_status == POWER_SUPPLY_STATUS_CHARGING) {
				if(system_soc > last_system_soc) {
					system_soc = last_system_soc + soc_changed;
				} else if (system_soc < last_system_soc) {
					system_soc = last_system_soc - soc_changed;
				}
			} else if (data->param.batt_status == POWER_SUPPLY_STATUS_DISCHARGING && system_soc < last_system_soc) {
				system_soc = last_system_soc - soc_changed;
			}
		} else {
			system_soc = last_system_soc;
		}
		pr_info("%s: fg jump: smooth soc_changed=%d, system_soc = %d, last_system_soc = %d\n", __func__,
				soc_changed, system_soc, last_system_soc);
	}

	if (system_soc < last_system_soc)
		system_soc = last_system_soc - 1;
	/*Avoid mismatches between charging status and soc changes  */
	if ((data->param.batt_status == POWER_SUPPLY_STATUS_DISCHARGING) && (system_soc > last_system_soc)) {
		system_soc = last_system_soc;
	}
	pr_info("%s: smooth_new:sys_soc:%d last_sys_soc:%d soc_delta:%d charging_status:%d unit_time:%d batt_ma_avg=%d\n", __func__,
		system_soc, last_system_soc, soc_delta, data->param.batt_status,
		unit_time, data->param.batt_ma_avg);

	if (system_soc != last_system_soc) {
		last_change_time = ktime_get();
		last_system_soc = system_soc;
	}
	if (system_soc > 100)
		system_soc = 100;
	if (system_soc < 0)
		system_soc = 0;

	if ((system_soc == 0) &&
		((data->param.batt_volt >= 3400) || ((time.tv_sec <= 20)))) {
		system_soc = 1;
		pr_info("uisoc::hold 1 when volt > 3400mv. \n");
	}

	data->param.batt_soc = system_soc;

	return;
}

#define CAP_DIFF 10
static int sprd_fgu_get_capacity(struct sprd_fgu_data *data, int *cap)
{
	int charge_diff_cap;
	static int last_cap = -1;

	data->rt_calib_vaild = false;
	sprd_fgu_get_vbat_info(data);
	sprd_fgu_calc_cycles(data);
	charge_diff_cap = sprd_fgu_calc_delta_cap(data);
	*cap = sprd_fgu_calc_soc(data, &charge_diff_cap);
	sprd_fgu_smooth_to_soc(data, cap, charge_diff_cap);
	sprd_fgu_save_last_smooth_soc(data, data->smooth_soc);

	if(last_cap == -1)
		last_cap = *cap;

	if(*cap - last_cap >= CAP_DIFF)
		*cap = last_cap + CAP_DIFF;
	else if (*cap - last_cap < (-CAP_DIFF))
		*cap = last_cap - CAP_DIFF;

	last_cap = *cap;

	data->raw_soc = *cap;
	dev_info(data->dev, "raw_soc_x10 in FGU:%d, rsoc:%d \n", data->raw_soc, DIV_ROUND_CLOSEST(data->raw_soc, 10));

	if (!data->erp_config) {
		/*add for xm smooth algorithm*/
		data->param.batt_ma = data->vbat_info.vbat_cur_ma;
		data->param.batt_ma_avg = data->vbat_info.vbat_cur_avg_ma;
		data->param.batt_temp = data->bat_temp;
		data->param.batt_volt = data->vbat_info.vbat_mv;
		data->param.batt_raw_soc = data->raw_soc / 10;
		battery_soc_smooth_tracking_not_eea(data);
		*cap = data->param.batt_soc * 10;
	}

	return 0;
}

static int sprd_fgu_get_charge_vol(struct sprd_fgu_data *data, int *val)
{
	int ret, vol_mv;

	ret = iio_read_channel_processed(data->charge_chan, &vol_mv);
	if (ret < 0)
		return ret;

	*val = vol_mv;
	return 0;
}

static int sprd_fgu_get_average_temp(struct sprd_fgu_data *data, int temp)
{
	int i, min, max;
	int sum = 0;

	if (data->temp_buff[0] == -500) {
		for (i = 0; i < SPRD_FGU_TEMP_BUFF_CNT; i++)
			data->temp_buff[i] = temp;
		return temp;
	}

	if (data->index >= SPRD_FGU_TEMP_BUFF_CNT)
		data->index = 0;

	data->temp_buff[data->index++] = temp;
	min = max = data->temp_buff[0];

	for (i = 0; i < SPRD_FGU_TEMP_BUFF_CNT; i++) {
		if (data->temp_buff[i] > max)
			max = data->temp_buff[i];

		if (data->temp_buff[i] < min)
			min = data->temp_buff[i];

		sum += data->temp_buff[i];
	}

	sum = sum - max - min;

	return sum / (SPRD_FGU_TEMP_BUFF_CNT - 2);
}

static int sprd_fgu_get_third_fuel_temp(struct sprd_fgu_data *data, int *temp)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name(THIRD_FUEL_NAME);
	if (!psy) {
		dev_err(data->dev, "Failed to find psy of sprd_third_fuel\n");
		return -ENODEV;
	}

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TEMP, &val);
	power_supply_put(psy);
	if (ret) {
		dev_err(data->dev, "Failed to get temp of sprd_third_fuel, ret = %d\n", ret);
		return ret;
	}

	dev_info(data->dev, "sprd_third_fuel temp = %d\n", val.intval);

	if (val.intval >= SPRD_THIRD_FUEL_TEMP_MIN && val.intval <= SPRD_THIRD_FUEL_TEMP_MAX) {
		*temp = sprd_fgu_get_average_temp(data, val.intval);
		data->bat_temp = *temp;
		if (data->debug_info.temp_debug_en)
			data->bat_temp = data->debug_info.debug_temp;
	} else {
		*temp = data->bat_temp;
		if (data->debug_info.temp_debug_en)
			*temp = data->debug_info.debug_temp;
	}

	return 0;
}

static void sprd_fgu_third_fuel_battery_detection(struct sprd_fgu_data *data)
{
	if (!data->use_battery_temp)
		return;

	if (data->gpiod)
		return;

	if (data->bat_present != data->last_bat_present) {
		data->last_bat_present = data->bat_present;
		power_supply_changed(data->battery);
		cm_notify_event(data->battery,
				data->bat_present ? CM_EVENT_BATT_IN : CM_EVENT_BATT_OUT,
				NULL);
	}
}

static void sprd_fgu_check_third_fuel_present(struct sprd_fgu_data *data)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	if (data->gpiod)
		return;

	if (!data->use_battery_temp)
		return;

	psy = power_supply_get_by_name(THIRD_FUEL_NAME);
	if (!psy) {
		data->bat_present = false;
		data->last_bat_present = data->bat_present;
		dev_err(data->dev, "Failed to find psy of sprd_third_fuel\n");
		return;
	}

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TEMP, &val);
	power_supply_put(psy);
	if (ret < 0) {
		data->bat_present = false;
		data->last_bat_present = data->bat_present;
		dev_err(data->dev, "Failed to get temp of sprd_third_fuel, ret = %d\n", ret);
		return;
	}

	data->bat_present = true;
	data->last_bat_present = data->bat_present;
	dev_info(data->dev, "sprd_third_fuel ack present\n");
}

static int sprd_fgu_get_temp(struct sprd_fgu_data *data, int *temp)
{
	int vol_ntc_uv, vol_adc_mv, ret;
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	if (data->use_battery_temp) {
		ret = sprd_fgu_get_third_fuel_temp(data, temp);
		if (!ret) {
			data->third_fuel_nack_cnt = 0;
			data->bat_present = true;
		} else if (ret < 0 && data->bat_present) {
			data->third_fuel_nack_cnt++;
			if (data->third_fuel_nack_cnt > 2)
				data->bat_present = false;
		}

		if (data->probe_initialized)
			return ret;
		else
			return 0;
	}

	ret = iio_read_channel_processed(data->channel, &vol_adc_mv);
	if (ret < 0)
		return ret;

	vol_ntc_uv = vol_adc_mv * 1000;
	if (data->comp_resistance) {
		int bat_current_ma, resistance_vol, calib_resistance_vol, temp_vol;

		ret = fgu_info->ops->get_current_now(fgu_info, &bat_current_ma);
		if (ret) {
			dev_err(data->dev, "failed to get battery current\n");
			return ret;
		}

		resistance_vol = bat_current_ma * data->comp_resistance;
		resistance_vol = DIV_ROUND_CLOSEST(resistance_vol, 10);
		calib_resistance_vol = bat_current_ma * (fgu_info->calib_resist / 10);
		calib_resistance_vol =
			DIV_ROUND_CLOSEST(calib_resistance_vol, 1000) + resistance_vol;

		temp_vol = (vol_ntc_uv / 10 - resistance_vol) * calib_resistance_vol;
		temp_vol = DIV_ROUND_CLOSEST(temp_vol, (187500 - calib_resistance_vol));

		vol_ntc_uv = temp_vol * 10 + vol_ntc_uv - resistance_vol * 10;
		if (vol_ntc_uv < 0)
			vol_ntc_uv = 0;
	}

	if (data->temp_table_len > 0) {
		*temp = sprd_fgu_vol2temp(data->temp_table, data->temp_table_len, vol_ntc_uv);
		*temp = sprd_fgu_get_average_temp(data, *temp);
	} else {
		*temp = 200;
	}

	data->bat_temp = *temp;

	if (data->debug_info.temp_debug_en)
		data->bat_temp = data->debug_info.debug_temp;

	return 0;
}

static void sprd_fgu_get_health(struct sprd_fgu_data *data, int *health)
{
	if (data->is_ovp)
		*health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	else
		*health = POWER_SUPPLY_HEALTH_GOOD;
}

static int sprd_fgu_suspend_calib_check_chg_sts(struct sprd_fgu_data *data)
{
	int ret = -EINVAL;

	if (data->chg_sts != POWER_SUPPLY_STATUS_NOT_CHARGING &&
	    data->chg_sts != POWER_SUPPLY_STATUS_DISCHARGING) {
		dev_info(data->dev, "Suspend calib charging status = %d, not meet conditions\n",
			 data->chg_sts);
		return ret;
	}

	return 0;
}

static int sprd_fgu_suspend_calib_check_temp(struct sprd_fgu_data *data)
{
	int ret, temp;

	ret = sprd_fgu_get_temp(data, &temp);
	if (ret) {
		dev_err(data->dev, "Suspend calib failed to temp, ret = %d\n", ret);
		return ret;
	}
	data->bat_temp = temp;

	if (temp < SPRD_FGU_CAP_CALIB_TEMP_LOW || temp > SPRD_FGU_CAP_CALIB_TEMP_HI) {
		dev_err(data->dev, "Suspend calib  temp = %d out range\n", temp);
		ret = -EINVAL;
	}

	dev_info(data->dev, "%s, temp = %d\n", __func__, temp);

	return ret;
}

static int sprd_fgu_suspend_calib_check_sleep_time(struct sprd_fgu_data *data)
{
	struct timespec64 cur_time;
	s64 cur_times;
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	cur_time = ktime_to_timespec64(ktime_get_boottime());
	cur_times = cur_time.tv_sec;

	fgu_info->slp_cap_calib.resume_time = cur_times;

	dev_info(data->dev, "%s, resume_time = %lld, suspend_time = %lld\n",
		 __func__, fgu_info->slp_cap_calib.resume_time,
		 fgu_info->slp_cap_calib.suspend_time);

	/* sleep time > 300s */
	if ((fgu_info->slp_cap_calib.resume_time - fgu_info->slp_cap_calib.suspend_time <
	    SPRD_FGU_SLP_CAP_CALIB_SLP_TIME) ||
	    fgu_info->slp_cap_calib.suspend_time == 0) {
		dev_info(data->dev, "suspend time not meet: suspend_time = %lld, resume_time = %lld\n",
			 fgu_info->slp_cap_calib.suspend_time,
			 fgu_info->slp_cap_calib.resume_time);
		return -EINVAL;
	}

	return 0;
}

static int sprd_fgu_suspend_calib_check_sleep_cur(struct sprd_fgu_data *data)
{
	int cc_uah, times, sleep_cur_ma = 0, ret = 0;
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	ret = fgu_info->ops->get_cc_uah(fgu_info, &fgu_info->slp_cap_calib.resume_cc_uah, false);
	if (ret)
		return ret;

	cc_uah = fgu_info->slp_cap_calib.suspend_cc_uah - fgu_info->slp_cap_calib.resume_cc_uah;
	times = (int)(fgu_info->slp_cap_calib.resume_time -  fgu_info->slp_cap_calib.suspend_time);
	sleep_cur_ma = sprd_fgu_uah2current(cc_uah, times);

	dev_info(data->dev, "%s, suspend_cc_uah = %d, resume_cc_uah = %d, cc_uah = %d, times = %d, sleep_cur_ma = %d\n",
		 __func__, fgu_info->slp_cap_calib.suspend_cc_uah,
		 fgu_info->slp_cap_calib.resume_cc_uah, cc_uah, times, sleep_cur_ma);

	if (abs(sleep_cur_ma) > fgu_info->slp_cap_calib.relax_cur_threshold) {
		dev_info(data->dev, "Sleep calib sleep current = %d, not meet conditions\n",
			 sleep_cur_ma);
		return -EINVAL;
	}

	return ret;
}

static int sprd_fgu_suspend_calib_get_ocv(struct sprd_fgu_data *data)
{
	int ret, i, cur_ma = 0x7fffffff;
	u32 vol_mv = 0;
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	for (i = SPRD_FGU_VOLTAGE_BUFF_CNT - 1; i >= 0; i--) {
		vol_mv = 0;
		ret = fgu_info->ops->get_vbat_buf(fgu_info, i, &vol_mv);
		if (ret) {
			dev_info(data->dev, "Sleep calib fail to get vbat_buf[%d]\n", i);
			continue;
		}

		cur_ma = 0x7fffffff;
		ret = fgu_info->ops->get_current_buf(fgu_info, i, &cur_ma);
		if (ret) {
			dev_info(data->dev, "Sleep calib fail to get cur_buf[%d]\n", i);
			continue;
		}

		if (abs(cur_ma) < fgu_info->slp_cap_calib.relax_cur_threshold) {
			dev_info(data->dev, "Sleep calib get cur[%d] = %d meet condition\n",
				 i, cur_ma);
			break;
		}
	}

	if (vol_mv == 0 || cur_ma == 0x7fffffff) {
		dev_info(data->dev, "Sleep calib fail to get cur and vol: cur = %d, vol = %d\n",
			 cur_ma, vol_mv);
		return -EINVAL;
	}

	dev_info(data->dev, "Sleep calib vol = %d, cur = %d, i = %d\n",
		 vol_mv, cur_ma, i);

	fgu_info->slp_cap_calib.resume_ocv_uv = vol_mv * 1000;

	return 0;
}

static void sprd_fgu_suspend_calib_cap_calib(struct sprd_fgu_data *data)
{
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	sprd_fgu_calib_batt_ocv(data, 0, fgu_info->slp_cap_calib.resume_ocv_uv, 0);

	if (is_between(data->bat_temp, SPRD_FGU_TRACK_LOW_TEMP_THRESHOLD,
		       SPRD_FGU_TRACK_HIGH_TEMP_THRESHOLD)) {
		data->track.lpocv_info.valid = true;
		data->track.lpocv_info.ocv_uv = fgu_info->slp_cap_calib.resume_ocv_uv;
		data->track.lpocv_info.ocv_time_stamp = fgu_info->slp_cap_calib.resume_time;
	}
}

static void sprd_fgu_suspend_calib_check(struct sprd_fgu_data *data)
{
	int ret;
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	if (!fgu_info->slp_cap_calib.support_slp_calib)
		return;

	ret = sprd_fgu_suspend_calib_check_chg_sts(data);
	if (ret)
		return;

	ret = fgu_info->ops->suspend_calib_check_relax_counter_sts(fgu_info);
	if (ret)
		return;

	ret = sprd_fgu_suspend_calib_check_sleep_time(data);
	if (ret)
		return;

	ret = sprd_fgu_suspend_calib_check_sleep_cur(data);
	if (ret)
		return;

	ret = sprd_fgu_suspend_calib_check_temp(data);
	if (ret)
		return;

	ret = sprd_fgu_suspend_calib_get_ocv(data);
	if (ret)
		return;

	if (!sprd_fgu_is_in_low_energy_dens(data, fgu_info->slp_cap_calib.resume_ocv_uv))
		return;

	sprd_fgu_suspend_calib_cap_calib(data);

	// todo:
}

static void sprd_fgu_suspend_calib_config(struct sprd_fgu_data *data)
{
	struct timespec64 cur_time;
	s64 cur_times;
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	if (!fgu_info->slp_cap_calib.support_slp_calib)
		return;

	cur_time = ktime_to_timespec64(ktime_get_boottime());
	cur_times = cur_time.tv_sec;

	fgu_info->slp_cap_calib.suspend_time =  cur_times;
	fgu_info->ops->get_cc_uah(fgu_info, &fgu_info->slp_cap_calib.suspend_cc_uah, false);

	fgu_info->ops->relax_mode_config(fgu_info);
}

static int sprd_fgu_batt_ovp_threshold_config(struct sprd_fgu_data *data)
{
	int ret = 0;
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	ret = fgu_info->ops->set_high_overload(fgu_info, data->batt_ovp_threshold);
	if (ret) {
		dev_err(data->dev, "failed to set fgu high overload\n");
		return ret;
	}

	ret = fgu_info->ops->enable_fgu_int(fgu_info, SPRD_FGU_VOLT_HIGH_INT_CMD, true);
	if (ret)
		return ret;

	data->is_ovp = false;
	dev_info(data->dev, "%s %d overload threshold config done!\n", __func__, __LINE__);

	return ret;
}

static bool sprd_fgu_probe_is_ready(struct sprd_fgu_data *data)
{
	unsigned long timeout;

	if (unlikely(!data->probe_initialized)) {
		timeout = wait_for_completion_timeout(&data->probe_init, SPRD_FGU_PROBE_TIMEOUT);
		if (!timeout) {
			dev_err(data->dev, "%s wait probe timeout\n", __func__);
			return false;
		}
	}

	return true;
}

static int sprd_fgu_get_current_now(struct sprd_fgu_data *data, union power_supply_propval *val)
{
	int ret = 0, value = 0;
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	if (data->debug_info.cur_now_debug_en) {
		val->intval = data->debug_info.debug_cur_now;
		return 0;
	}

	ret = fgu_info->ops->get_current_now(fgu_info, &value);
	if (ret) {
		dev_err(data->dev, "%s,fail to get cur_ma, ret = %d\n", __func__, ret);
		return ret;
	}

	val->intval = value * 1000;

	return ret;
}

static int sprd_fgu_get_batt_ocv_mv(struct sprd_fgu_data *data, union power_supply_propval *val)
{
	int ret = 0, batt_ocv_uv, reg, magic;

	ret = sprd_fgu_read_last_batt_ocv(data, &batt_ocv_uv, &reg, &magic);
	if (ret) {
		dev_err(data->dev, "%s,fail to get last_batt_ocv, ret = %d\n", __func__, ret);
		return ret;
	}

	val->intval = reg;

	return ret;
}

static int sprd_fgu_get_batt_ocv_temp(struct sprd_fgu_data *data, union power_supply_propval *val)
{
	int ret = 0, batt_ocv_temp, reg, magic;

	ret = sprd_fgu_read_last_batt_temp(data, &batt_ocv_temp, &reg, &magic);
	if (ret) {
		dev_err(data->dev, "%s,fail to get last_batt_temp, ret = %d\n", __func__, ret);
		return ret;
	}

	val->intval = reg;

	return ret;
}

static int sprd_fgu_get_shutdown_batt_temp(struct sprd_fgu_data *data,
					   union power_supply_propval *val)
{
	int ret = 0, shutdown_batt_temp, reg, magic;

	ret = sprd_fgu_read_last_shutdown_batt_temp(data, &shutdown_batt_temp, &reg, &magic);
	if (ret) {
		dev_err(data->dev, "%s,fail to get last_shutdown_batt_temp, ret = %d\n",
			__func__, ret);
		return ret;
	}

	val->intval = reg;

	return ret;
}

static int sprd_fgu_get_batt_cc_uah(struct sprd_fgu_data *data, union power_supply_propval *val)
{
	int ret = 0, batt_cc_uah, reg, magic;

	ret = sprd_fgu_read_last_cc_uah(data, &batt_cc_uah, &reg, &magic);
	if (ret) {
		dev_err(data->dev, "%s,fail to get last_cc_uah, ret = %d\n", __func__, ret);
		return ret;
	}

	val->intval = reg;

	return ret;
}

static int sprd_fgu_get_property(struct power_supply *psy,
				 enum power_supply_property psp,
				 union power_supply_propval *val)
{
	struct sprd_fgu_data *data = power_supply_get_drvdata(psy);
	struct sprd_fgu_info *fgu_info;
	int ret = 0, value = 0;

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}
	fgu_info = data->fgu_info;

	if (unlikely(!psy->initialized && atomic_read(&psy->use_cnt) > 0)) {
		dev_err(data->dev, "%s psy is not ready\n", __func__);
		return -ENODEV;
	}

	if (!sprd_fgu_probe_is_ready(data)) {
		dev_err(data->dev, "%s wait probe timeout\n", __func__);
		return -EINVAL;
	}

	if (psp == POWER_SUPPLY_PROP_CURRENT_NOW)
		return sprd_fgu_get_current_now(data, val);

	if ((psp == POWER_SUPPLY_PROP_POWER_NOW ||
	     psp == POWER_SUPPLY_PROP_TEMP_AMBIENT ||
	     psp == POWER_SUPPLY_PROP_ENERGY_NOW ||
	     psp == POWER_SUPPLY_PROP_TEMP_ALERT_MIN) &&
	    !data->fgu_info->use_miscdata) {
		val->intval = SPRD_FGU_MAGIC_NUMBER;
		return 0;
	}

	mutex_lock(&data->lock);

	switch ((u32)psp) {
	case POWER_SUPPLY_PROP_HEALTH:
		if (data->debug_info.batt_health_debug_en) {
			val->intval = data->debug_info.debug_batt_health;
			break;
		}

		sprd_fgu_get_health(data, &value);
		val->intval = value;
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = data->bat_present;

		if (data->debug_info.batt_present_debug_en)
			val->intval = data->debug_info.debug_batt_present;

		break;

	case POWER_SUPPLY_PROP_TEMP:
		if (data->fake_temp != -9999) {
			val->intval = data->fake_temp;
			break;
		}
		if (data->debug_info.temp_debug_en)
			val->intval = data->debug_info.debug_temp;
		else if (data->temp_table_len <= 0 ||
			 (data->bat_present == 0 && cali_or_auto_mode))
			val->intval = 200;
		else
			val->intval = data->bat_temp;

		break;

	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
		break;

	case POWER_SUPPLY_PROP_CAPACITY:
		if (val->intval == CM_BOOT_CAPACITY) {
			val->intval = data->boot_cap;
			break;
		}
		else if (val->intval == CM_RAW_CAPACITY) {
			val->intval = data->raw_soc;
			break;
		}
		val->intval = data->bat_soc;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		ret = fgu_info->ops->get_vbat_avg(fgu_info, &value);
		if (ret)
			goto error;

		val->intval = value * 1000;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (data->debug_info.vbat_now_debug_en) {
			val->intval = data->debug_info.debug_vbat_now;
			break;
		}

		ret = fgu_info->ops->get_vbat_now(fgu_info, &value);
		if (ret)
			goto error;

		val->intval = value * 1000;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		if (data->debug_info.ocv_debug_en) {
			val->intval = data->debug_info.debug_ocv;
			break;
		}

		ret = sprd_fgu_get_vbat_ocv(data, &value);
		if (ret)
			goto error;

		val->intval = value * 1000;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		if (data->debug_info.chg_vol_debug_en) {
			val->intval = data->debug_info.debug_chg_vol;
			break;
		}

		ret = sprd_fgu_get_charge_vol(data, &value);
		if (ret)
			goto error;

		val->intval = value * 1000;
		break;

	case POWER_SUPPLY_PROP_CURRENT_AVG:
		ret = fgu_info->ops->get_current_avg(fgu_info, &value);
		if (ret)
			goto error;

		val->intval = value * 1000;
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = data->design_mah * 1000;
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval = data->total_mah * 1000;
		break;

	case POWER_SUPPLY_PROP_SOH:
		if (CHIP_ONLINE == ll_check_secret_chip_online()) {
			val->intval = lc_get_rawsoh();
			if (val->intval < 0) {
				val->intval = data->last_batt_soh;
				pr_err("use platform soh:%d\n", data->last_batt_soh);
			}
		} else {
			val->intval = data->last_batt_soh;
			pr_info("chip offline, use last_batt_soh:%d\n", data->last_batt_soh);
		}
		break;

	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		val->intval = data->charge_cycle;
		break;

	case POWER_SUPPLY_PROP_POWER_NOW:
		ret = sprd_fgu_get_batt_ocv_mv(data, val);
		if (ret)
			val->intval = 0;
		break;

	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		ret = sprd_fgu_get_batt_ocv_temp(data, val);
		if (ret)
			val->intval = 0;
		break;

	case POWER_SUPPLY_PROP_TEMP_ALERT_MIN:
		ret = sprd_fgu_get_shutdown_batt_temp(data, val);
		if (ret)
			val->intval = 0;
		break;

	case POWER_SUPPLY_PROP_ENERGY_NOW:
		ret = sprd_fgu_get_batt_cc_uah(data, val);
		if (ret)
			val->intval = 0;
		break;

	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		if (data->batt_cap_level_critical){
			val->intval = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
			pr_debug("%s fgu critical level true\n", __func__);
		} else
			val->intval = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
		break;

	default:
		ret = -EINVAL;
		break;
	}

error:
	mutex_unlock(&data->lock);
	return ret;
}

static int sprd_fgu_set_property(struct power_supply *psy,
				 enum power_supply_property psp,
				 const union power_supply_propval *val)
{
	struct sprd_fgu_data *data = power_supply_get_drvdata(psy);
	int ret = 0, ui_cap, chg_loss_soc;
	struct sprd_fgu_info *fgu_info;
	struct timespec64 cur_time;

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}
	fgu_info = data->fgu_info;

	if (unlikely(!psy->initialized && atomic_read(&psy->use_cnt) > 0)) {
		dev_err(data->dev, "%s psy is not ready\n", __func__);
		return -ENODEV;
	}

	if (!sprd_fgu_probe_is_ready(data)) {
		dev_err(data->dev, "%s wait probe timeout\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&data->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CAPACITY:
		dev_dbg(data->dev, "%s:line%d ui_cap = %d,\n",
			__func__, __LINE__, val->intval);
		ret = sprd_fgu_save_last_ui_soc(data, val->intval);
		if (ret < 0)
			dev_err(data->dev, "failed to save battery capacity\n");

		ret = fgu_info->ops->read_last_cap(fgu_info, &ui_cap);
		if (ret < 0) {
			ui_cap = -1;
			dev_err(data->dev, "failed to read ui capacity\n");
		}

		if (val->intval != ui_cap) {
			dev_info(data->dev, "ui cap save failed, save it again! save_cap = %d, read_cap = %d\n",
				 val->intval, ui_cap);
			ret = sprd_fgu_save_last_ui_soc(data, val->intval);
			if (ret < 0)
				dev_err(data->dev, "%d failed to save battery capacity\n",
					__LINE__);
		}

		break;

	case POWER_SUPPLY_PROP_STATUS:
		data->chg_sts = val->intval;
		if (!data->online && data->chg_sts == POWER_SUPPLY_STATUS_CHARGING) {
			data->chg_sts = POWER_SUPPLY_STATUS_DISCHARGING;
			dev_info(data->dev, "online = %d, chg_sts = %d\n",
				 data->online, data->chg_sts);
		}
		if (data->chg_sts != POWER_SUPPLY_STATUS_CHARGING) {
			data->charge_ocv_start = true;
			cur_time = ktime_to_timespec64(ktime_get_boottime());
			data->stop_charge_times = cur_time.tv_sec;
			dev_info(data->dev, "stop_charge_times = %lld\n", data->stop_charge_times);
			data->charge_ocv_enter_times = cur_time.tv_sec;
		}
		break;

	case POWER_SUPPLY_PROP_CALIBRATE:
		if (val->intval == SPRD_FGU_DEBUG_EN_CMD) {
			dev_info(data->dev, "calibrate init_cap with smooth_soc\n");
			data->debug_info.smooth_soc_debug_en = true;
			break;
		} else if (val->intval == SPRD_FGU_DEBUG_DIS_CMD) {
			dev_info(data->dev, "calibrate only init_cap\n");
			data->debug_info.smooth_soc_debug_en = false;
			break;
		} else if (val->intval > 1000 || val->intval < 0) {
			dev_info(data->dev, "calibrate val error\n");
			break;
		}

		if (val->intval == 1000 && !data->full_ocv_start) {
			data->full_ocv_start = true;
			cur_time = ktime_to_timespec64(ktime_get_boottime());
			data->full_ocv_enter_times = cur_time.tv_sec;
		}

		chg_loss_soc = sprd_fgu_get_chg_loss_soc_by_temp(data, data->bat_temp);
		sprd_fgu_calib_batt_ocv(data, val->intval - chg_loss_soc, 0, 0);
		dev_info(data->dev, "%s:line%d batt_ocv_uv = %d, chg_loss_soc = %d\n",
			 __func__, __LINE__, data->batt_ocv_uv, chg_loss_soc);

		dev_info(data->dev, "%s:line%d calib cap = %d\n", __func__, __LINE__, val->intval);
		if (!data->debug_info.smooth_soc_debug_en) {
			data->smooth_soc = val->intval;
			data->smooth_soc_decimal = 0;
			sprd_fgu_save_last_smooth_soc(data, data->smooth_soc);
		}
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		data->design_mah = val->intval / 1000;
		break;

	case POWER_SUPPLY_PROP_CHARGE_FULL:
		if (val->intval == SPRD_FGU_DEBUG_EN_CMD) {
			data->debug_info.charge_full_debug_en = true;
			dev_info(data->dev, "Change full to debug mode\n");
			break;
		} else if (val->intval == SPRD_FGU_DEBUG_DIS_CMD) {
			data->debug_info.charge_full_debug_en = false;
			dev_info(data->dev, "Recovery Change full to normal mode\n");
			break;
		}

		if (data->debug_info.charge_full_debug_en) {
			sprd_fgu_adjust_fcc(data, val->intval / 1000, SPRD_FGU_TEMP_DEFAULT);
			data->total_mah = sprd_fgu_get_fcc_uah(data, SPRD_FGU_TEMP_DEFAULT,
					  data->charge_cycle) / 1000;
			dev_info(data->dev, "Change full to debug fcc = %d, total_mah = %d\n",
				 val->intval / 1000, data->total_mah);
		}
		break;

	case POWER_SUPPLY_PROP_TEMP:
		pr_err("sprd_fgu_set_property fake_temp = %d\n", val->intval);
		data->fake_temp = val->intval;
		if(!data->batt_psy)
                	data->batt_psy = power_supply_get_by_name("battery");
        	if(data->batt_psy)
			power_supply_changed(data->batt_psy);

		if (val->intval == SPRD_FGU_DEBUG_EN_CMD) {
			dev_info(data->dev, "Change battery temperature to debug mode\n");
			data->debug_info.temp_debug_en = true;
			data->debug_info.debug_temp = SPRD_FGU_DEBUG_TEMP_CELSIUS;
			break;
		} else if (val->intval == SPRD_FGU_DEBUG_DIS_CMD) {
			dev_info(data->dev, "Recovery battery temperature to normal mode\n");
			data->debug_info.temp_debug_en = false;
			break;
		} else if (!data->debug_info.temp_debug_en) {
			dev_info(data->dev, "Battery temperature not in debug mode\n");
			break;
		}

		data->debug_info.debug_temp = val->intval;
		dev_info(data->dev, "Battery debug temperature = %d\n", val->intval);
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		if (val->intval == SPRD_FGU_DEBUG_EN_CMD) {
			dev_info(data->dev, "Change battery present to debug mode\n");
			data->debug_info.debug_batt_present = true;
			data->debug_info.batt_present_debug_en = true;
			break;
		} else if (val->intval == SPRD_FGU_DEBUG_DIS_CMD) {
			dev_info(data->dev, "Recovery battery present to normal mode\n");
			data->debug_info.batt_present_debug_en = false;
			break;
		} else if (!data->debug_info.batt_present_debug_en) {
			dev_info(data->dev, "Battery present not in debug mode\n");
			break;
		}

		data->debug_info.debug_batt_present = !!val->intval;
		mutex_unlock(&data->lock);
		cm_notify_event(data->battery, data->debug_info.debug_batt_present ?
				CM_EVENT_BATT_IN : CM_EVENT_BATT_OUT, NULL);
		dev_info(data->dev, "Battery debug present = %d\n", !!val->intval);
		return ret;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (val->intval == SPRD_FGU_DEBUG_EN_CMD) {
			dev_info(data->dev, "Change voltage_now to debug mode\n");
			data->debug_info.debug_vbat_now = SPRD_FGU_DEBUG_VBAT_NOW_UV;
			data->debug_info.vbat_now_debug_en = true;
			break;
		} else if (val->intval == SPRD_FGU_DEBUG_DIS_CMD) {
			dev_info(data->dev, "Recovery voltage_now to normal mode\n");
			data->debug_info.vbat_now_debug_en = false;
			data->debug_info.debug_vbat_now = 0;
			break;
		} else if (!data->debug_info.vbat_now_debug_en) {
			dev_info(data->dev, "Voltage_now not in debug mode\n");
			break;
		}

		data->debug_info.debug_vbat_now = val->intval;
		dev_info(data->dev, "Battery debug voltage_now = %d\n", val->intval);
		break;

	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (val->intval == SPRD_FGU_DEBUG_EN_CMD) {
			dev_info(data->dev, "Change current_now to debug mode\n");
			data->debug_info.debug_cur_now = SPRD_FGU_DEBUG_CUR_NOW_UV;
			data->debug_info.cur_now_debug_en = true;
			break;
		} else if (val->intval == SPRD_FGU_DEBUG_DIS_CMD) {
			dev_info(data->dev, "Recovery current_now to normal mode\n");
			data->debug_info.cur_now_debug_en = false;
			data->debug_info.debug_cur_now = 0;
			break;
		} else if (!data->debug_info.cur_now_debug_en) {
			dev_info(data->dev, "Current_now not in debug mode\n");
			break;
		}

		data->debug_info.debug_cur_now = val->intval;
		dev_info(data->dev, "Battery debug current_now = %d\n", val->intval);
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		if (val->intval == SPRD_FGU_DEBUG_EN_CMD) {
			dev_info(data->dev, "Change charge voltage to debug mode\n");
			data->debug_info.debug_chg_vol = SPRD_FGU_DEBUG_VBUS_UV;
			data->debug_info.chg_vol_debug_en = true;
			break;
		} else if (val->intval == SPRD_FGU_DEBUG_DIS_CMD) {
			dev_info(data->dev, "Recovery charge voltage to normal mode\n");
			data->debug_info.chg_vol_debug_en = false;
			data->debug_info.debug_chg_vol = 0;
			break;
		} else if (!data->debug_info.chg_vol_debug_en) {
			dev_info(data->dev, "Charge voltage not in debug mode\n");
			break;
		}

		data->debug_info.debug_chg_vol = val->intval;
		dev_info(data->dev, "Battery debug charge voltage = %d\n", val->intval);
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		if (val->intval == SPRD_FGU_DEBUG_EN_CMD) {
			dev_info(data->dev, "Change OCV voltage to debug mode\n");
			data->debug_info.debug_ocv = SPRD_FGU_DEBUG_OCV_UV;
			data->debug_info.ocv_debug_en = true;
			break;
		} else if (val->intval == SPRD_FGU_DEBUG_DIS_CMD) {
			dev_info(data->dev, "Recovery OCV voltage to normal mode\n");
			data->debug_info.ocv_debug_en = false;
			data->debug_info.debug_ocv = 0;
			break;
		} else if (!data->debug_info.ocv_debug_en) {
			dev_info(data->dev, "OCV voltage not in debug mode\n");
			break;
		}

		data->debug_info.debug_ocv = val->intval;
		dev_info(data->dev, "Battery debug OCV voltage = %d\n", val->intval);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if (val->intval == SPRD_FGU_GOOD_HEALTH_CMD) {
			data->is_ovp = false;
			ret = fgu_info->ops->enable_fgu_int(fgu_info,
							    SPRD_FGU_VOLT_HIGH_INT_CMD, true);
			break;
		} else if (val->intval == SPRD_FGU_DEBUG_EN_CMD) {
			dev_info(data->dev, "Change Battery Health to debug mode\n");
			data->debug_info.batt_health_debug_en = true;
			data->debug_info.debug_batt_health = 1;
			break;
		} else if (val->intval == SPRD_FGU_DEBUG_DIS_CMD) {
			dev_info(data->dev, "Recovery  Battery Health to normal mode\n");
			data->debug_info.batt_health_debug_en = false;
			data->debug_info.debug_batt_health = 1;
			break;
		} else if (!data->debug_info.batt_health_debug_en) {
			dev_info(data->dev, "OCV  Battery Health not in debug mode\n");
			break;
		}

		data->debug_info.debug_batt_health = val->intval;
		dev_info(data->dev, "Battery debug  Battery Health = %#x\n", val->intval);
		break;

	default:
		ret = -EINVAL;
	}

	mutex_unlock(&data->lock);
	return ret;
}

static void sprd_fgu_external_power_changed(struct power_supply *psy)
{
	struct sprd_fgu_data *data = power_supply_get_drvdata(psy);

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	power_supply_changed(data->battery);
}

static int sprd_fgu_property_is_writeable(struct power_supply *psy,
					  enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_CALIBRATE:
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
	case POWER_SUPPLY_PROP_HEALTH:
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		return 1;

	default:
		return 0;
	}
}

static enum power_supply_property sprd_fgu_props[] = {
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CALIBRATE,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_POWER_NOW,
	POWER_SUPPLY_PROP_TEMP_AMBIENT,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_TEMP_ALERT_MIN,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
};

static const struct power_supply_desc sprd_fgu_desc = {
	.name			= "sc27xx-fgu",
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	.properties		= sprd_fgu_props,
	.num_properties		= ARRAY_SIZE(sprd_fgu_props),
	.get_property		= sprd_fgu_get_property,
	.set_property		= sprd_fgu_set_property,
	.external_power_changed	= sprd_fgu_external_power_changed,
	.property_is_writeable	= sprd_fgu_property_is_writeable,
	.no_thermal		= true,
};

static bool sprd_fgu_discharging_current_trend(struct sprd_fgu_data *data)
{
	int i, ret, cur_ma = 0;
	bool is_discharging = true;
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	if (data->cur_now_buff[SPRD_FGU_CURRENT_BUFF_CNT - 1] == SPRD_FGU_MAGIC_NUMBER) {
		is_discharging = false;
		for (i = 0; i < SPRD_FGU_CURRENT_BUFF_CNT; i++) {
			ret = fgu_info->ops->get_current_buf(fgu_info, i, &cur_ma);
			if (ret) {
				dev_err(data->dev, "fail to init cur_now_buff[%d]\n", i);
				return is_discharging;
			}

			data->cur_now_buff[i] = cur_ma;
		}

		return is_discharging;
	}

	for (i = 0; i < SPRD_FGU_CURRENT_BUFF_CNT; i++) {
		if (data->cur_now_buff[i] > 0)
			is_discharging = false;
	}

	for (i = 0; i < SPRD_FGU_CURRENT_BUFF_CNT; i++) {
		ret = fgu_info->ops->get_current_buf(fgu_info, i, &cur_ma);
		if (ret) {
			dev_err(data->dev, "fail to get cur_now_buff[%d]\n", i);
			data->cur_now_buff[SPRD_FGU_CURRENT_BUFF_CNT - 1] =
				SPRD_FGU_MAGIC_NUMBER;
			is_discharging = false;
			return is_discharging;
		}

		data->cur_now_buff[i] = cur_ma;
		if (data->cur_now_buff[i] > 0)
			is_discharging = false;
	}

	return is_discharging;
}

static bool sprd_fgu_discharging_cc_uah_trend(struct sprd_fgu_data *data)
{
	struct sprd_fgu_info *fgu_info = data->fgu_info;
	int cur_cc_uah, ret;

	ret = fgu_info->ops->get_cc_uah(fgu_info, &cur_cc_uah, false);
	if (ret) {
		dev_err(data->dev, "%s failed get cur_cc_uah!!\n", __func__);
		return false;
	}

	return (data->last_cc_uah != SPRD_FGU_MAGIC_NUMBER) &&
		(data->last_cc_uah > cur_cc_uah) ? true : false;
}

static bool sprd_fgu_discharging_trend(struct sprd_fgu_data *data)
{
	bool discharging = true;
	static int dischg_cnt;
	int i;

	mutex_lock(&data->discharge_lock);
	if (dischg_cnt >= SPRD_FGU_DISCHG_CNT)
		dischg_cnt = 0;

	if (!sprd_fgu_discharging_current_trend(data)) {
		discharging =  false;
		goto charging;
	}

	if (!sprd_fgu_discharging_cc_uah_trend(data)) {
		discharging =  false;
		goto charging;
	}

	data->dischg_trend[dischg_cnt++] = true;

	for (i = 0; i < SPRD_FGU_DISCHG_CNT; i++) {
		if (!data->dischg_trend[i]) {
			discharging =  false;
			mutex_unlock(&data->discharge_lock);
			return discharging;
		}
	}

	if (data->chg_sts == POWER_SUPPLY_STATUS_CHARGING && discharging)
		dev_info(data->dev, "%s: discharging\n", __func__);

	mutex_unlock(&data->discharge_lock);
	return discharging;

charging:
	data->dischg_trend[dischg_cnt++] = false;
	mutex_unlock(&data->discharge_lock);
	return discharging;
}

static void sprd_fgu_update_alarm_cap(struct sprd_fgu_data *data)
{
	if (data->vbat_info.vbat_err & OCV_ERR)
		return;

	if (data->vbat_info.ocv_uv <= data->min_volt_uv) {
		/*
		 * After adjusting the battery capacity, we should set the
		 * lowest alarm voltage instead.
		 */
		data->min_volt_uv = sprd_fgu_get_ocv_by_soc(data, SPRD_FGU_TEMP_DEFAULT, 1, false);
	}
}

static void sprd_fgu_vbat_int_thres_switch(struct sprd_fgu_data *data,
					   enum sprd_fgu_int_command int_cmd)
{
	struct sprd_fgu_info *fgu_info = data->fgu_info;
	int i;
	int row_size = data->vbat_thres_temp_table_len;

	for (i = 0; i < row_size; i++) {
		if (data->bat_temp < data->vbat_thres_temp_table[i])
			break;
	}

	if (i == row_size)
		i = row_size - 1;

	if (int_cmd == SPRD_FGU_VOLT_LOW_INT_CMD)
		data->vbat_level++;
	else if (int_cmd == SPRD_FGU_VOLT_HIGH_INT_CMD)
		data->vbat_level--;

	if (data->vbat_level == 0) {
		fgu_info->ops->set_low_overload(fgu_info, data->min_volt_uv / 1000);
		fgu_info->ops->set_high_overload(fgu_info, data->batt_ovp_threshold);
		goto out;
	}

	fgu_info->ops->set_low_overload(fgu_info,
					data->low_int_thres_table[i][data->vbat_level - 1]);
	fgu_info->ops->set_high_overload(fgu_info,
					 data->high_int_thres_table[i][data->vbat_level - 1]);

out:
	fgu_info->ops->enable_fgu_int(fgu_info, int_cmd, true);
	dev_info(data->dev, "%s %d, i = %d, vbat_level = %d\n",
		 __func__, __LINE__, i, data->vbat_level);
}

static int sprd_fgu_set_vbat_level(struct sprd_fgu_data *data, int vbat_level)
{
	int ret;
	union power_supply_propval val;
	struct power_supply *bcl_psy;

	bcl_psy = power_supply_get_by_name("sprd_bcl");
	if (!bcl_psy) {
		dev_err(data->dev, "Cannot find bcl power supply\n");
		return -EINVAL;
	}

	val.intval = vbat_level;
	ret = power_supply_set_property(bcl_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
	power_supply_put(bcl_psy);
	if (ret)
		dev_err(data->dev, "fail to set vbat_level = %d, ret = %d\n", vbat_level, ret);

	return ret;
}

static irqreturn_t sprd_fgu_interrupt(int irq, void *dev_id)
{
	struct sprd_fgu_data *data = dev_id;
	int ret, vbat_mv;
	u32 status = 0;
	struct sprd_fgu_info *fgu_info;
	struct sprd_fgu_vbat_info *vbat_info = &data->vbat_info;

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return IRQ_HANDLED;
	}
	fgu_info = data->fgu_info;

	if (!sprd_fgu_probe_is_ready(data)) {
		dev_err(data->dev, "%s wait probe timeout\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&data->lock);

	ret = fgu_info->ops->get_fgu_int(fgu_info, &status);
	if (ret) {
		dev_err(data->dev, "%s, failed to get fgu int, ret = %d\n", __func__, ret);
		goto out;
	}

	dev_info(data->dev, "%s, fgu int status = 0x%x\n", __func__, status);

	if (fgu_info->ops->clr_fgu_int_all) {
		ret = fgu_info->ops->clr_fgu_int_all(fgu_info, status);
		if (ret)
			dev_err(data->dev, "%s, failed to clr fgu int, ret = %d\n", __func__, ret);
	}

	if (status & BIT(SPRD_FGU_RELAX_CNT_INT_EVENT)) {
		fgu_info->slp_cap_calib.relax_cnt_int_ocurred = true;
		dev_info(data->dev, "%s, relax_cnt_int ocurred!!\n", __func__);
	}

	if (status & BIT(SPRD_FGU_POWER_LOW_CNT_INT_EVENT)) {
		fgu_info->slp_cap_calib.power_low_cnt_int_ocurred = true;
		dev_info(data->dev, "%s, power_low_cnt_int ocurred!!\n", __func__);
	}

	if (status & BIT(SPRD_FGU_VOLT_HIGH_INT_EVENT)) {
		fgu_info->ops->enable_fgu_int(fgu_info, SPRD_FGU_VOLT_HIGH_INT_CMD, false);
		ret = fgu_info->ops->get_vbat_now(fgu_info, &vbat_mv);
		if (ret)
			dev_info(data->dev, "get_vbat_now error, ret = %d\n", ret);

		if (data->vbat_level == 0) {
			data->is_ovp = true;
			mutex_unlock(&data->lock);
			cm_notify_event(data->battery, CM_EVENT_BATT_OVERVOLTAGE, NULL);
			mutex_lock(&data->lock);
		} else if (data->high_int_thres_cols > 0 && data->low_int_thres_cols > 0) {
			if (data->vbat_level > 0) {
				sprd_fgu_vbat_int_thres_switch(data, SPRD_FGU_VOLT_HIGH_INT_CMD);
				ret = sprd_fgu_set_vbat_level(data, data->vbat_level);
				if (!ret)
					power_supply_changed(data->battery);
			}
		}
		dev_info(data->dev, "volt_high_int ocurred, vbat_mv = %d\n", vbat_mv);
	}

	if (status & BIT(SPRD_FGU_CLBCNT_DELTA_INT_EVENT))
		dev_info(data->dev, "clbcnt delta int ocurred!!\n");

	if (status & BIT(SPRD_FGU_VOLT_LOW_INT_EVENT)) {
		fgu_info->ops->enable_fgu_int(fgu_info, SPRD_FGU_VOLT_LOW_INT_CMD, false);
		/*
		 * When low overload voltage interrupt happens, we should calibrate the
		 * battery capacity in lower voltage stage.
		 */
		data->volt_low_flag = true;
		schedule_delayed_work(&data->fgu_work, 0);
		dev_info(data->dev, "volt_low_int ocurred!!\n");

		if (data->high_int_thres_cols > 0 && data->low_int_thres_cols > 0) {
			if (data->vbat_level < data->low_int_thres_cols) {
				sprd_fgu_vbat_int_thres_switch(data, SPRD_FGU_VOLT_LOW_INT_CMD);
				ret = sprd_fgu_set_vbat_level(data, data->vbat_level);
				if (!ret)
					power_supply_changed(data->battery);
			}
		}
		dev_info(data->dev, "volt_low_int ocurred, vbat_mv = %d\n", vbat_info->vbat_mv);
	}

out:
	mutex_unlock(&data->lock);

	return IRQ_HANDLED;
}

static irqreturn_t sprd_fgu_bat_detection(int irq, void *dev_id)
{
	struct sprd_fgu_data *data = dev_id;
	int state;
	struct sprd_fgu_info *fgu_info;

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return IRQ_HANDLED;
	}
	fgu_info = data->fgu_info;

	if (!sprd_fgu_probe_is_ready(data)) {
		dev_err(data->dev, "%s wait probe timeout\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&data->lock);

	state = gpiod_get_value_cansleep(data->gpiod);
	if (state < 0) {
		dev_err(data->dev, "failed to get gpio state\n");
		mutex_unlock(&data->lock);
		return IRQ_RETVAL(state);
	}

	data->bat_present = !!state;

	if (!data->bat_present)
		fgu_info->ops->enable_fgu_int(fgu_info, SPRD_FGU_VOLT_LOW_INT_CMD, false);

	mutex_unlock(&data->lock);

	power_supply_changed(data->battery);

	cm_notify_event(data->battery,
			data->bat_present ? CM_EVENT_BATT_IN : CM_EVENT_BATT_OUT,
			NULL);

	return IRQ_HANDLED;
}

static void sprd_fgu_disable(void *_data)
{
	struct sprd_fgu_data *data = _data;
	struct sprd_fgu_info *fgu_info;

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}
	fgu_info = data->fgu_info;

	fgu_info->ops->enable_fgu_module(fgu_info, false);
}

static void sprd_fgu_typec_extcon_work(struct work_struct *data)
{
	struct sprd_fgu_data *fgu_data =
		container_of(data, struct sprd_fgu_data, typec_extcon_work);
	int retry_cnt = SPRD_FGU_GET_CHG_TYPE_RETRY_CNT;

	if (fgu_data->use_typec_extcon && fgu_data->online) {
		/* if use typec extcon notify charger,
		 * wait for BC1.2 detect charger type.
		 */
		while (fgu_data->online && retry_cnt > 0) {
			if (fgu_data->usb_phy->chg_type != UNKNOWN_TYPE)
				break;
			retry_cnt--;
			msleep(50);
		}
		dev_info(fgu_data->dev, "retry_cnt = %d\n", retry_cnt);

	}

	switch (fgu_data->usb_phy->chg_type) {
	case SDP_TYPE:
		fgu_data->chg_type = POWER_SUPPLY_USB_TYPE_SDP;
		break;

	case DCP_TYPE:
		fgu_data->chg_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;

	case CDP_TYPE:
		fgu_data->chg_type = POWER_SUPPLY_USB_TYPE_CDP;
		break;

	default:
		fgu_data->chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
	}

	dev_info(fgu_data->dev, "charger type = %d\n", fgu_data->usb_phy->chg_type);
}

static int sprd_fgu_extcon_event(struct notifier_block *nb,
				 unsigned long event, void *param)
{
	struct sprd_fgu_data *data = container_of(nb, struct sprd_fgu_data, extcon_nb);
	int state = 0;

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return NOTIFY_OK;
	}

	state = extcon_get_state(data->edev, SPRD_FGU_EXTCON_SINK);
	if (state < 0) {
		dev_err(data->dev, "failed to get extcon sink state（%d）\n", state);
		return NOTIFY_OK;
	}

	if (data->is_sink == state)
		return NOTIFY_OK;

	data->is_sink = state;

	if (data->is_sink)
		data->online = true;
	else
		data->online = false;

	if (!data->online) {
		data->full_ocv_start = false;
		data->charge_ocv_start = false;
	}

	data->chg_term_loop_mode = 0;
	data->chg_term_loop_trigger_cnt = 0;
	data->dischg_term_loop_mode = 0;
	data->dischg_term_loop_trigger_cnt = 0;

	schedule_work(&data->typec_extcon_work);
	return NOTIFY_OK;
}

static int sprd_fgu_usb_change(struct notifier_block *nb, unsigned long limit, void *info)
{
	u32 type;
	struct sprd_fgu_data *data =
		container_of(nb, struct sprd_fgu_data, usb_notify);

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return NOTIFY_OK;
	}

	pm_stay_awake(data->dev);

	if (limit)
		data->online = true;
	else
		data->online = false;

	if (!data->online) {
		data->full_ocv_start = false;
		data->charge_ocv_start = false;
	}

	type = data->usb_phy->chg_type;

	data->chg_term_loop_mode = 0;
	data->chg_term_loop_trigger_cnt = 0;
	data->dischg_term_loop_mode = 0;
	data->dischg_term_loop_trigger_cnt = 0;

	switch (type) {
	case SDP_TYPE:
		data->chg_type = POWER_SUPPLY_USB_TYPE_SDP;
		break;

	case DCP_TYPE:
		data->chg_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;

	case CDP_TYPE:
		data->chg_type = POWER_SUPPLY_USB_TYPE_CDP;
		break;

	default:
		data->chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
	}

	pm_relax(data->dev);

	return NOTIFY_OK;
}

static bool sprd_fgu_cap_track_is_pocv_valid(struct sprd_fgu_data *data, int *ocv_uv,
					     struct sprd_fgu_ocv_info *ocv_info)
{
	s64 cur_time;
	int ret;

	if (!ocv_info->valid)
		return false;

	ret = sprd_fgu_get_rtc_time(data, &cur_time);
	if (ret)
		return false;

	ocv_info->valid = false;

	if (cur_time - ocv_info->ocv_time_stamp > SPRD_FGU_TRACK_OCV_VALID_TIME) {
		dev_info(data->dev, "%d capacity track ocv is invalid cur_time = %lld, ocv_time_stamp = %lld\n",
			 __LINE__, cur_time, ocv_info->ocv_time_stamp);
		return false;
	}

	*ocv_uv = ocv_info->ocv_uv;

	return true;
}

static bool sprd_fgu_cap_track_is_ocv_valid(struct sprd_fgu_data *data, int *ocv_uv,
					    struct sprd_fgu_ocv_info *ocv_info, bool is_clear)
{
	struct timespec64 cur_time;
	s64 cur_times;

	if (!ocv_info->valid)
		return false;

	cur_time = ktime_to_timespec64(ktime_get_boottime());
	cur_times  = cur_time.tv_sec;

	if (is_clear)
		ocv_info->valid = false;

	if (cur_times - ocv_info->ocv_time_stamp > SPRD_FGU_TRACK_OCV_VALID_TIME) {
		dev_info(data->dev, "%d capacity track ocv is invalid cur_time = %lld, ocv_time_stamp = %lld\n",
			 __LINE__, cur_times, ocv_info->ocv_time_stamp);
		return false;
	}

	*ocv_uv = ocv_info->ocv_uv;

	return true;
}

/* TODO */
static bool sprd_fgu_cap_track_is_sw_ocv_valid(struct sprd_fgu_data *data, int *ocv_uv)
{
	return false;
}

static bool sprd_fgu_is_meet_cap_track_start_conditon(struct sprd_fgu_data *data, int *ocv_uv)
{
	if (sprd_fgu_cap_track_is_pocv_valid(data, ocv_uv, &data->track.pocv_info)) {
		data->track.mode = CAP_TRACK_MODE_POCV;
		dev_info(data->dev, "capacity track pocv = %d meet start condition", *ocv_uv);
	} else if (sprd_fgu_cap_track_is_ocv_valid(data, ocv_uv, &data->track.lpocv_info, true)) {
		data->track.mode = CAP_TRACK_MODE_LP_OCV;
		dev_info(data->dev, "capacity track lpocv = %d meet start condition", *ocv_uv);
	} else if (sprd_fgu_cap_track_is_sw_ocv_valid(data, ocv_uv)) {
		data->track.mode = CAP_TRACK_MODE_SW_OCV;
		dev_info(data->dev, "capacity track sw ocv = %d  meet start condition", *ocv_uv);
	} else {
		return false;
	}

	return true;
}

static bool sprd_fgu_is_new_cap_track_start_conditon_meet(struct sprd_fgu_data *data)
{
	int ocv_uv, cap;

	if (!sprd_fgu_cap_track_is_ocv_valid(data, &ocv_uv, &data->track.lpocv_info, false))
		return false;

	cap = sprd_fgu_get_soc_by_ocv(data, ocv_uv, data->bat_temp);
	if (cap > SPRD_FGU_TRACK_START_CAP_THRESHOLD)
		return false;

	if ((data->track.mode == CAP_TRACK_MODE_LP_OCV) &&
	    ((ktime_divns(ktime_get_boottime(), NSEC_PER_SEC) -
	      data->track.start_time) < SPRD_FGU_TRACK_NEW_OCV_VALID_THRESHOLD))
		return false;

	return true;
}

static bool sprd_fgu_cap_track_is_meet_end_conditon(struct sprd_fgu_data *data)
{
	int i, ret, cur_now = 0, vol_now = 0;
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	for (i = 0; i < 5; i++) {
		ret = fgu_info->ops->get_current_buf(fgu_info, i, &cur_now);
		if (ret)
			return false;

		ret = fgu_info->ops->get_vbat_buf(fgu_info, i, &vol_now);
		if (ret)
			return false;

		if (cur_now < 0 || cur_now > data->track.end_cur || vol_now < data->track.end_vol)
			return false;
	}

	return true;
}

static void sprd_fgu_cap_track_state_init(struct sprd_fgu_data *data, int *cycle)
{
	int design_mah, learned_mah;

	design_mah = data->design_mah;
	learned_mah = data->track.learned_mah;

	data->track.state = CAP_TRACK_IDLE;

	if (data->track.pocv_info.valid)
		*cycle = SPRD_FGU_CAPACITY_TRACK_0S;

	if (learned_mah <= 0) {
		dev_err(data->dev, "[init] learned_mah is invalid.\n");
		return;
	}

	if ((learned_mah > design_mah) && ((learned_mah - design_mah) > design_mah / 10))
		learned_mah = design_mah * 11 / 10;

	else if ((design_mah > learned_mah) && ((design_mah - learned_mah) > design_mah * 3 / 10))
		learned_mah = design_mah * 7 / 10;

	data->total_mah = learned_mah;
}

static void sprd_fgu_cap_track_state_idle(struct sprd_fgu_data *data, int *cycle)
{
	int ret, cc_uah, ocv_uv = 0;
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	if (!data->bat_present) {
		*cycle = SPRD_FGU_CAPACITY_TRACK_100S;
		dev_dbg(data->dev, "[idle] battery is not present, monitor later.\n");
		return;
	}

	if (!sprd_fgu_is_meet_cap_track_start_conditon(data, &ocv_uv))
		return;

	if (data->track.mode != CAP_TRACK_MODE_POCV)
		data->track.boot_uah = 0;

	data->track.start_cap = sprd_fgu_get_soc_by_ocv(data, ocv_uv, data->bat_temp);
	data->track.start_ocv = ocv_uv;

	/*
	 * When the capacity tracking start condition is met, the battery is almost empty,
	 * so we set a starting threshold, if it is greater than it will not enable
	 * the capacity tracking function, now we set the capacity tracking monitor
	 * initial percentage threshold to 60%.
	 */
	if (data->track.start_cap > SPRD_FGU_TRACK_START_CAP_THRESHOLD) {
		dev_dbg(data->dev, "[idle] start_cap = %d does not satisfy the track start condition\n",
			data->track.start_cap);
		data->track.start_cap = 0;
		return;
	}

	ret = fgu_info->ops->get_cc_uah(fgu_info, &cc_uah, false);
	if (ret) {
		dev_err(data->dev, "[idle] failed to get start cc_uah.\n");
		return;
	}

	data->track.start_time = ktime_divns(ktime_get_boottime(), NSEC_PER_SEC);
	data->track.start_cc_mah = cc_uah / 1000 + data->track.boot_uah / 1000;
	data->track.start_aging_bat_id = data->last_aging_bat_id;
	data->track.state = CAP_TRACK_UPDATING;

	dev_info(data->dev, "[idle] start_time = %lld, start_cc_mah = %d, start_cap = %d, boot_mah = %d\n",
		 data->track.start_time, cc_uah / 1000,
		 data->track.start_cap, data->track.boot_uah / 1000);
}

static void sprd_fgu_cap_track_state_updating(struct sprd_fgu_data *data, int *cycle)
{
	int ibat_avg_ma, vbat_avg_mv, ibat_now_ma, ret;
	struct sprd_fgu_info *fgu_info = data->fgu_info;
	int lpocv, cap, temp_start_cap;

	if (!data->bat_present) {
		*cycle = SPRD_FGU_CAPACITY_TRACK_100S;
		data->track.state = CAP_TRACK_IDLE;
		dev_err(data->dev, "[updating] battery is not present, return to idle state.\n");
		return;
	}

	if (data->bat_temp > SPRD_FGU_TRACK_HIGH_TEMP_THRESHOLD ||
	    data->bat_temp < SPRD_FGU_TRACK_LOW_TEMP_THRESHOLD) {
		*cycle = SPRD_FGU_CAPACITY_TRACK_100S;
		dev_dbg(data->dev, "[updating] exceed temp range, monitor capacity track later.\n");
		return;
	}

	if ((ktime_divns(ktime_get_boottime(), NSEC_PER_SEC) -
	     data->track.start_time) > SPRD_FGU_TRACK_TIMEOUT_THRESHOLD) {
		data->track.state = CAP_TRACK_IDLE;
		dev_err(data->dev, "[updating] capacity tracktime out.\n");
		return;
	}

	if (data->track.start_aging_bat_id != data->last_aging_bat_id) {
		data->track.state = CAP_TRACK_IDLE;
		dev_dbg(data->dev, "[updating] bat aging table change occurs, need to stop capacity track!\n");
		return;
	}

	if (sprd_fgu_cap_track_is_ocv_valid(data, &lpocv, &data->track.lpocv_info, false)) {
		cap = sprd_fgu_get_soc_by_ocv(data, lpocv, data->bat_temp);

		temp_start_cap = sprd_fgu_get_soc_by_ocv(data, data->track.start_ocv,
							 data->bat_temp);

		if ((cap - data->track.start_cap) >= SPRD_FGU_TRACK_LEARNED_CAP_HTHRESHOLD) {
			data->track.end_cap = cap;
			dev_info(data->dev, "[updating] capacity track finish condition-1 is meet, end_cap = %d.\n",
				 cap);
			pm_wakeup_event(data->dev, SPRD_FGU_TRACK_DONE_WAKE_UP_MS);
			data->track.lpocv_learned_done = true;
			data->track.delta_cap = data->track.end_cap - temp_start_cap;
			data->track.state = CAP_TRACK_DONE;
			*cycle = SPRD_FGU_CAPACITY_TRACK_0S;
			return;
		} else if (sprd_fgu_is_new_cap_track_start_conditon_meet(data)) {
			dev_info(data->dev, "[updating] capacity track new start condition meet");
			pm_wakeup_event(data->dev, SPRD_FGU_TRACK_UPDATING_WAKE_UP_MS);
			*cycle = SPRD_FGU_CAPACITY_TRACK_0S;
			data->track.state = CAP_TRACK_IDLE;
			return;
		}
	}

	if (!data->track.full_chg_track_enable)
		return;

	if (!data->track.end_vol || !data->track.end_cur) {
		dev_err(data->dev, "[updating] Not support fgu track end condition. end_vol = %d, end_cur = %d\n",
			data->track.end_vol, data->track.end_cur);
		return;
	}

	ret = fgu_info->ops->get_current_avg(fgu_info, &ibat_avg_ma);
	if (ret) {
		dev_err(data->dev, "failed to get ibat average current.\n");
		return;
	}

	ret = fgu_info->ops->get_current_now(fgu_info, &ibat_now_ma);
	if (ret) {
		dev_err(data->dev, "failed to get ibat current now.\n");
		return;
	}

	ret = fgu_info->ops->get_vbat_avg(fgu_info, &vbat_avg_mv);
	if (ret) {
		dev_err(data->dev, "failed to get battery voltage.\n");
		return;
	}

	if (vbat_avg_mv > data->track.end_vol &&
	    (ibat_avg_ma >= 0 && ibat_avg_ma < data->track.end_cur) &&
	    (ibat_now_ma >= 0 && ibat_now_ma < data->track.end_cur)) {
		dev_info(data->dev, "[updating] capacity track finish condition-2 is meet.\n");
		pm_wakeup_event(data->dev, SPRD_FGU_TRACK_DONE_WAKE_UP_MS);
		temp_start_cap = sprd_fgu_get_soc_by_ocv(data, data->track.start_ocv,
							 data->bat_temp);
		data->track.end_cap = SPRD_FGU_FCC_PERCENT;
		data->track.delta_cap = data->track.end_cap - temp_start_cap;
		data->track.state = CAP_TRACK_DONE;
		if (data->track.delta_cap < SPRD_FGU_TRACK_LEARNED_CAP_HTHRESHOLD) {
			dev_info(data->dev, "[updating] capacity track not meet 40 percent update threshold.\n");
			data->track.state = CAP_TRACK_IDLE;
		}
		*cycle = SPRD_FGU_CAPACITY_TRACK_0S;
	}
}

static void sprd_fgu_cap_track_state_done(struct sprd_fgu_data *data, int *cycle)
{
	int ret, ibat_avg_ma = 0, vbat_avg_mv = 0, ibat_now_ma = 0;
	int delta_mah, total_mah, design_mah, end_mah, cur_cc_uah;
	struct sprd_fgu_info *fgu_info = data->fgu_info;
	int remain_mah, diff_mah, wt_diff_mah, wt_diff_raw_mah, start_mah;
	int temp_start_cap, limit_mah;

	*cycle = SPRD_FGU_CAPACITY_TRACK_3S;

	if (!data->bat_present) {
		*cycle = SPRD_FGU_CAPACITY_TRACK_100S;
		data->track.state = CAP_TRACK_IDLE;
		dev_err(data->dev, "[done] battery is not present, return to idle state.\n");
		return;
	}

	if (data->bat_temp > SPRD_FGU_TRACK_HIGH_TEMP_THRESHOLD ||
	    data->bat_temp < SPRD_FGU_TRACK_LOW_TEMP_THRESHOLD) {
		data->track.state = CAP_TRACK_UPDATING;
		*cycle = SPRD_FGU_CAPACITY_TRACK_15S;
		dev_err(data->dev, "[done] exceed temp range, return to updating state.\n");
		return;
	}

	total_mah = sprd_fgu_get_fcc_uah(data, data->bat_temp, data->charge_cycle) / 1000;
	design_mah = data->design_mah;

	ret = fgu_info->ops->get_cc_uah(fgu_info, &cur_cc_uah, false);
	if (ret) {
		dev_err(data->dev, "[done] failed to get cur_cc_uah.\n");
		return;
	}

	temp_start_cap = sprd_fgu_get_soc_by_ocv(data, data->track.start_ocv, data->bat_temp);
	start_mah = DIV_ROUND_CLOSEST(total_mah * temp_start_cap, 1000);

	if (data->track.lpocv_learned_done) {
		data->track.lpocv_learned_done = false;
		delta_mah = cur_cc_uah / 1000 - data->track.start_cc_mah;
		remain_mah = DIV_ROUND_CLOSEST((SPRD_FGU_FCC_PERCENT -
						data->track.end_cap) * total_mah, 1000);
		diff_mah = start_mah + delta_mah + remain_mah - total_mah;
		dev_info(data->dev, "[done] %d start_cap = %d, temp_start_mah = %d, delta_mah = %d, remain_mah = %d, diff_mah = %d\n",
			 __LINE__, temp_start_cap, start_mah,
			 delta_mah, remain_mah, diff_mah);
		goto lpocv_learned_done;
	}

	ret = fgu_info->ops->get_current_avg(fgu_info, &ibat_avg_ma);
	if (ret) {
		dev_err(data->dev, "failed to get battery current.\n");
		return;
	}

	ret = fgu_info->ops->get_current_now(fgu_info, &ibat_now_ma);
	if (ret) {
		dev_err(data->dev, "failed to get now current.\n");
		return;
	}

	ret = fgu_info->ops->get_vbat_avg(fgu_info, &vbat_avg_mv);
	if (ret) {
		dev_err(data->dev, "failed to get battery voltage.\n");
		return;
	}

	if (!sprd_fgu_cap_track_is_meet_end_conditon(data)) {
		if (vbat_avg_mv > data->track.end_vol &&
		    (ibat_avg_ma >= 0 && ibat_avg_ma < data->track.end_cur) &&
		    (ibat_now_ma >= 0 && ibat_now_ma < data->track.end_cur)) {
			*cycle = SPRD_FGU_CAPACITY_TRACK_3S;
		} else {
			*cycle = SPRD_FGU_CAPACITY_TRACK_15S;
			data->track.state = CAP_TRACK_UPDATING;
			dev_info(data->dev, "[done] does not meet end conditons-2, return to updating status, vbat_avg_mv = %d, ibat_avg_ma = %d, ibat_now_ma = %d\n",
				 vbat_avg_mv, ibat_avg_ma, ibat_now_ma);
		}

		return;
	}

	delta_mah = cur_cc_uah / 1000 - data->track.start_cc_mah;
	diff_mah = start_mah + delta_mah - total_mah;
	dev_info(data->dev, "[done] %d temp_start_cap = %d, start_mah = %d, delta_mah = %d, diff_mah = %d\n",
		 __LINE__, temp_start_cap, start_mah, delta_mah, diff_mah);

lpocv_learned_done:
	wt_diff_raw_mah = DIV_ROUND_CLOSEST(diff_mah * data->track.delta_cap, 1000);

	limit_mah = total_mah * SPRD_FGU_TRACK_LIMIT_PERCENT / SPRD_FGU_FCC_PERCENT;
	wt_diff_mah = clamp(wt_diff_raw_mah, -limit_mah, limit_mah);

	end_mah = total_mah + wt_diff_mah;

	dev_info(data->dev, "[done] Capacity track end: diff_mah = %d, delta_cap = %d, wt_diff_mah = %d, wt_diff_raw_mah = %d, end_mah = %d, limit_mah = %d, total_mah = %d\n",
		 diff_mah, data->track.delta_cap, wt_diff_mah, wt_diff_raw_mah,
		 end_mah, limit_mah, total_mah);

	data->track.state = CAP_TRACK_IDLE;
	if ((((end_mah > design_mah) && ((end_mah - design_mah) < design_mah / 10)) ||
	    ((design_mah > end_mah) && ((design_mah - end_mah) < design_mah / 2))) &&
	    data->track.start_aging_bat_id == data->last_aging_bat_id) {
		sprd_fgu_adjust_fcc(data, end_mah, data->bat_temp);
		data->total_mah = sprd_fgu_get_fcc_uah(data, SPRD_FGU_TEMP_DEFAULT,
						       data->charge_cycle) / 1000;
		pm_wakeup_event(data->dev, SPRD_FGU_TRACK_WAKE_UP_MS);
		dev_info(data->dev, "track capacity done: end_mah = %d, diff_mah = %d\n",
			 end_mah, (end_mah - total_mah));
	} else {
		dev_info(data->dev, "less than half standard capacity or start_aging_bat_id = %d is not equal to last_aging_bat_id = %d.\n",
			 data->track.start_aging_bat_id, data->last_aging_bat_id);
	}
}

static int sprd_fgu_cap_track_state_machine(struct sprd_fgu_data *data)
{
	int cycle = SPRD_FGU_CAPACITY_TRACK_15S;

	switch (data->track.state) {
	case CAP_TRACK_INIT:
		sprd_fgu_cap_track_state_init(data, &cycle);
		break;
	case CAP_TRACK_IDLE:
		sprd_fgu_cap_track_state_idle(data, &cycle);
		break;
	case CAP_TRACK_UPDATING:
		sprd_fgu_cap_track_state_updating(data, &cycle);
		break;
	case CAP_TRACK_DONE:
		sprd_fgu_cap_track_state_done(data, &cycle);
		break;
	case CAP_TRACK_ERR:
		dev_err(data->dev, "track status error\n");
		break;

	default:
		break;
	}

	return cycle;
}

static bool sprd_fgu_is_switch_bat_para(struct sprd_fgu_data *data, bool force_update)
{
	int aging_bat_id, vbat_avg_mv, ret;
	bool is_need_switch = false;
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	aging_bat_id = sprd_fgu_get_aging_bat_id(data);
	if (aging_bat_id != data->last_aging_bat_id && force_update) {
		data->last_aging_bat_id = aging_bat_id;
		is_need_switch = true;
		goto out;
	}
	if (aging_bat_id != data->last_aging_bat_id) {
		ret = fgu_info->ops->get_vbat_avg(fgu_info, &vbat_avg_mv);
		if (ret) {
			dev_err(data->dev, "%d failed to get vbat_avg_mv\n", __LINE__);
			goto out;
		}

//		if (vbat_avg_mv < SPRD_FGU_IS_SWITCH_BAT_PARA_VOL_THRES) {
			data->last_aging_bat_id = aging_bat_id;
			is_need_switch = true;
//		}
	}

out:
	dev_info(data->dev, "%s %d is_need_switch = %d\n",
		 __func__, __LINE__, is_need_switch);

	return is_need_switch;
}

static void sprd_fgu_bat_aging_algo(struct sprd_fgu_data *data, bool force_update)
{
	int ret = 0, fcc_mah, aging_bat_id;
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	dev_info(data->dev, "%s %d charge_cycle = %d\n",
		 __func__, __LINE__, data->charge_cycle);

	if (!sprd_fgu_is_switch_bat_para(data, force_update))
		return;

	if (!force_update)
		data->dynamic_update_bat_para_flag = true;

	aging_bat_id = data->last_aging_bat_id;
	if (!force_update && !data->last_aging_bat_id)
		aging_bat_id = SPRD_BAT_AGING_ID_NZ2Z;

	fcc_mah = data->temp_default_fcc_mah;
	data->bat_fcc_aging_ratio = DIV_ROUND_CLOSEST(data->track.learned_mah * 1000, fcc_mah);

	dev_info(data->dev, "%s bat_fcc_aging_ratio = %d, learned_mah = %d, fcc_mah = %d\n",
		 __func__, data->bat_fcc_aging_ratio, data->track.learned_mah, fcc_mah);

	ret = sprd_fgu_get_bat_para_table(data, aging_bat_id);
	if (ret)
		return;

	/*
	 * Set the capacity delta threshold, that means when the capacity
	 * change is multiples of the delta threshold, the controller
	 * will generate one interrupt to notify the users to update the battery
	 * capacity. Now we set the 1% capacity value.
	 */
	fgu_info->ops->set_cap_delta_thre(fgu_info, data->total_mah, 10);
	cm_notify_event(data->battery, CM_EVENT_BATT_AGING, NULL);
}

static int sprd_fgu_cap_calc_work_cycle(struct sprd_fgu_data *data)
{
	int cur_ma = 0, delta_cc_uah;
	int work_cycle = SPRD_FGU_NORMAL_WORK_10S;
	s64 times;

	sprd_fgu_third_fuel_battery_detection(data);

	if (data->bat_temp < SPRD_FGU_CAP_CALC_WORK_LOW_TEMP ||
	    data->bat_soc < SPRD_FGU_CAP_CALC_WORK_LOW_CAP ||
	    data->batt_soc < SPRD_FGU_CAP_CALC_WORK_LOW_CAP ||
	    data->vbat_info.vbat_avg_mv < SPRD_FGU_LOW_VBAT_3450) {
		if(data->bat_temp < 0 && data->bat_soc < 350){
			dev_dbg(data->dev, "battery is in low temp, loading is too high, should more quick update FGU\n");
			if(data->vbat_info.vbat_cur_avg_ma <= -1500)
				work_cycle = 5;//5s
			else if(data->vbat_info.vbat_cur_avg_ma < -1200)
				work_cycle = 5;//5s
		}else{
			dev_dbg(data->dev, "temp = %d, battery soc = %d, should quick update FGU\n", data->bat_temp, data->bat_soc);
			work_cycle = SPRD_FGU_QUICKEN_WORK_5S;
		}

		return work_cycle;
	}

	if (data->work_exit_times != 0 && data->work_enter_cc_uah > data->work_exit_cc_uah) {
		times = data->work_enter_times - data->work_exit_times;
		if (times != 0) {
			delta_cc_uah = data->work_enter_cc_uah - data->work_exit_cc_uah;
			cur_ma = sprd_fgu_uah2current(delta_cc_uah, times);
			if (cur_ma > SPRD_FGU_CAP_CALC_WORK_BIG_CURRENT) {
				dev_info(data->dev, "%s cur_ma = %d!!\n", __func__, cur_ma);
				work_cycle = SPRD_FGU_QUICKEN_WORK_5S;
			}
		}
	}

	return work_cycle;
}

static int sprd_fgu_get_average_ibat(struct sprd_fgu_data *data, int ibat)
{
	int i, min, max;
	int sum = 0;

	if (data->batt_ibat_avg_buff[0] == SPRD_FGU_MAGIC_NUMBER) {
		for (i = 0; i < SPRD_FGU_IBAT_AVG_BUFF_CNT; i++)
			data->batt_ibat_avg_buff[i] = ibat;
	}

	if (data->batt_ibat_avg_index >= SPRD_FGU_IBAT_AVG_BUFF_CNT)
		data->batt_ibat_avg_index = 0;

	data->batt_ibat_avg_buff[data->batt_ibat_avg_index++] = ibat;
	min = max = data->batt_ibat_avg_buff[0];

	for (i = 0; i < SPRD_FGU_IBAT_AVG_BUFF_CNT; i++) {
		if (data->batt_ibat_avg_buff[i] > max)
			max = data->batt_ibat_avg_buff[i];

		if (data->batt_ibat_avg_buff[i] < min)
			min = data->batt_ibat_avg_buff[i];

		sum += data->batt_ibat_avg_buff[i];
	}

	sum = sum - max - min;

	return sum / (SPRD_FGU_IBAT_AVG_BUFF_CNT - 2);
}

static int sprd_fgu_calc_ibat_avg(struct sprd_fgu_data *data)
{
	int cur_ma = 0, delta_cc_uah, ret = 0;
	s64 times;
	static int first_enter = 1;
	struct sprd_fgu_info *fgu_info;

	fgu_info = data->fgu_info;
	if (first_enter) {
		ret = fgu_info->ops->get_current_avg(fgu_info, &cur_ma);
		if (ret) {
			dev_err(data->dev, "get_current_avg error, ret = %d\n", ret);
			return ret;
		}
		data->batt_ibat_avg_ma = cur_ma;
		data->ibat_avg_work_cycle_ma = cur_ma;
		first_enter = 0;
	} else {
		if (data->work_exit_times != 0) {
			times = data->work_enter_times - data->work_exit_times;
			if (times != 0) {
				delta_cc_uah = data->work_enter_cc_uah - data->work_exit_cc_uah;
				cur_ma = sprd_fgu_uah2current(delta_cc_uah, times);
				data->ibat_avg_work_cycle_ma = cur_ma;
				dev_dbg(data->dev, "%s: cur_ma = %d, delta_cc_uah = %d, times = %lld\n",
					__func__, cur_ma, delta_cc_uah, times);
				data->batt_ibat_avg_ma = sprd_fgu_get_average_ibat(data, cur_ma);
			}
		}
	}

	dev_info(data->dev, "%s: batt_ibat_avg_ma = %d, ibat_avg_work_cycle_ma = %d, work_exit_times = %lld\n",
		 __func__, data->batt_ibat_avg_ma, data->ibat_avg_work_cycle_ma,
		 data->work_exit_times);

	return ret;
}

static void sprd_fgu_sr_dens_thr_set_calib(struct sprd_fgu_data *data,
					   int sr_ocv_cap, int dens_thd)
{
	int vbat_soc = 0;

	if (sr_ocv_cap + dens_thd < data->batt_rm_soc) {
		data->batt_rm_soc = sr_ocv_cap + dens_thd;
		sprd_fgu_calib_batt_ocv(data, data->batt_rm_soc, 0, 0);
	} else {
		vbat_soc = sprd_fgu_get_soc_by_ocv(data, data->sr_ocv_uv -
						   30 * 1000, data->bat_temp);

		if (data->batt_rm_soc < vbat_soc && sr_ocv_cap - dens_thd <= vbat_soc) {
			data->batt_rm_soc = vbat_soc;
			sprd_fgu_calib_batt_ocv(data, data->batt_rm_soc, 0, 0);
		} else if (sr_ocv_cap - dens_thd > data->batt_rm_soc) {
			data->batt_rm_soc = sr_ocv_cap - dens_thd;
			sprd_fgu_calib_batt_ocv(data, data->batt_rm_soc, 0, 0);
		}
	}

	dev_info(data->dev, "%s, sr_ocv_cap = %d, dens_thd = %d, batt_rm_soc = %d\n, vbat_soc = %d",
		 __func__, sr_ocv_cap, dens_thd, data->batt_rm_soc, vbat_soc);
}

static void sprd_fgu_sr_precision_cap_calib(struct sprd_fgu_data *data)
{
	int sr_ocv_cap;
	struct timespec64 cur_time;
	s64 cur_times;
	int high_dens_threshold = 0;

	cur_time = ktime_to_timespec64(ktime_get_boottime());
	cur_times = cur_time.tv_sec;

	sr_ocv_cap = sprd_fgu_get_soc_by_ocv(data, data->sr_ocv_uv, data->bat_temp);

	if (data->bat_temp >= SPRD_FGU_CAP_CALIB_NORMAL_TEMP_THR)
		high_dens_threshold = SPRD_FGU_CALIB_PRECI_H_DENS_NOR_TEMP_CAP_DIFF;
	else if (data->bat_temp >= SPRD_FGU_CAP_CALIB_LOW_TEMP_THR)
		high_dens_threshold = SPRD_FGU_CALIB_PRECI_H_DENS_LOW_TEMP_CAP_DIFF;
	else
		high_dens_threshold = SPRD_FGU_CALIB_PRECI_H_DENS_COOL_TEMP_CAP_DIFF;

	if (data->track.lpocv_info.is_low_density) {
		if (is_between(data->bat_temp, SPRD_FGU_TRACK_LOW_TEMP_THRESHOLD,
			       SPRD_FGU_TRACK_HIGH_TEMP_THRESHOLD)) {
			data->track.lpocv_info.is_low_density = false;
			data->track.lpocv_info.valid = true;
			data->track.lpocv_info.ocv_uv = data->sr_ocv_uv;
			data->track.lpocv_info.ocv_time_stamp = cur_times;
		}
		sprd_fgu_calib_batt_ocv(data, 0, data->sr_ocv_uv, 0);
		dev_info(data->dev, "%s, sr_ocv_uv = %d, sr_ocv_cap = %d\n",
			__func__, data->sr_ocv_uv, sr_ocv_cap);
	} else {
		sprd_fgu_sr_dens_thr_set_calib(data, sr_ocv_cap, high_dens_threshold);
	}
}

static void sprd_fgu_sr_moderate_cap_calib(struct sprd_fgu_data *data)
{
	int sr_ocv_cap;
	int low_dens_threshold = 0, high_dens_threshold = 0;

	sr_ocv_cap = sprd_fgu_get_soc_by_ocv(data, data->sr_ocv_uv, data->bat_temp);

	if (data->bat_temp >= SPRD_FGU_CAP_CALIB_NORMAL_TEMP_THR) {
		low_dens_threshold = SPRD_FGU_CALIB_MODER_L_DENS_NOR_TEMP_CAP_DIFF;
		high_dens_threshold = SPRD_FGU_CALIB_MODER_H_DENS_NOR_TEMP_CAP_DIFF;
	} else if (data->bat_temp >= SPRD_FGU_CAP_CALIB_LOW_TEMP_THR) {
		low_dens_threshold = SPRD_FGU_CALIB_MODER_L_DENS_LOW_TEMP_CAP_DIFF;
		high_dens_threshold = SPRD_FGU_CALIB_MODER_H_DENS_LOW_TEMP_CAP_DIFF;
	} else {
		low_dens_threshold = SPRD_FGU_CALIB_MODER_L_DENS_COOL_TEMP_CAP_DIFF;
		high_dens_threshold = SPRD_FGU_CALIB_MODER_H_DENS_COOL_TEMP_CAP_DIFF;
	}

	if (data->track.lpocv_info.is_low_density) {
		data->track.lpocv_info.is_low_density = false;
		sprd_fgu_sr_dens_thr_set_calib(data, sr_ocv_cap, low_dens_threshold);
	} else {
		sprd_fgu_sr_dens_thr_set_calib(data, sr_ocv_cap, high_dens_threshold);
	}
}

static void sprd_fgu_sr_coarse_cap_calib(struct sprd_fgu_data *data)
{
	int sr_ocv_cap;
	int low_dens_threshold = 0, high_dens_threshold = 0;

	sr_ocv_cap = sprd_fgu_get_soc_by_ocv(data, data->sr_ocv_uv, data->bat_temp);

	if (data->bat_temp >= SPRD_FGU_CAP_CALIB_NORMAL_TEMP_THR) {
		low_dens_threshold = SPRD_FGU_CALIB_COAR_L_DENS_NOR_TEMP_CAP_DIFF;
		high_dens_threshold = SPRD_FGU_CALIB_COAR_H_DENS_NOR_TEMP_CAP_DIFF;
	} else if (data->bat_temp >= SPRD_FGU_CAP_CALIB_LOW_TEMP_THR) {
		low_dens_threshold = SPRD_FGU_CALIB_COAR_L_DENS_LOW_TEMP_CAP_DIFF;
		high_dens_threshold = SPRD_FGU_CALIB_COAR_H_DENS_LOW_TEMP_CAP_DIFF;
	} else {
		low_dens_threshold = SPRD_FGU_CALIB_COAR_L_DENS_COOL_TEMP_CAP_DIFF;
		high_dens_threshold = SPRD_FGU_CALIB_COAR_H_DENS_COOL_TEMP_CAP_DIFF;
	}

	if (data->track.lpocv_info.is_low_density) {
		data->track.lpocv_info.is_low_density = false;
		sprd_fgu_sr_dens_thr_set_calib(data, sr_ocv_cap, low_dens_threshold);
	} else {
		sprd_fgu_sr_dens_thr_set_calib(data, sr_ocv_cap, high_dens_threshold);
	}
}

static void sprd_fgu_sr_suspend_resume_calib(struct sprd_fgu_data *data)
{
	int ocv_calib_times, ocv_uv, zero_soc_ocv_uv, ocv_valid_time;
	s64 resume_calib_valid_time;

	if (data->bat_temp <= SPRD_FGU_CAP_CALIB_TEMP_MIN)
		return;

	resume_calib_valid_time = data->work_enter_times - data->awake_times;
	if (!data->sr_resume_need_calib ||
	    resume_calib_valid_time > SPRD_FGU_SR_CALIB_VALID_TIME) {
		data->sr_resume_need_calib = false;
		return;
	}

	zero_soc_ocv_uv = sprd_fgu_get_ocv_by_soc(data, data->bat_temp, 0, false);
	ocv_uv = sprd_fgu_get_discharge_ocv(data, data->sr_ocv_uv, data->bat_temp,
					    data->sr_ocv_cur_ua / 1000, data->charge_cycle);

	data->sr_ocv_uv = ocv_uv;
	dev_info(data->dev, "ocv_uv = %d, zero_soc_ocv_uv = %d, sr_ocv_uv = %d, sr_ocv_cur_ua = %d\n",
		 ocv_uv, zero_soc_ocv_uv, data->sr_ocv_uv, data->sr_ocv_cur_ua);

	if (sprd_fgu_is_in_low_energy_dens(data, ocv_uv)) {
		dev_info(data->dev, "suspend calib: vol_uv is in low energy dens!!!!\n");
		data->track.lpocv_info.is_low_density = true;
	}

	data->sr_resume_need_calib = false;
	ocv_calib_times = data->awake_times - data->batt_ocv_times;
	if (!data->batt_ocv_times)
		ocv_valid_time = SPRD_FGU_BOOT_OCV_CALIB_VALID_TIMES;
	else
		ocv_valid_time = SPRD_FGU_LP_OCV_CALIB_VALID_TIMES;

	if (data->sr_calib_mode == SR_CALIB_PRECISION &&
	   ((data->track.lpocv_info.is_low_density &&
	    ocv_calib_times >= SPRD_FGU_OCV_INTERVAL_TIME) ||
	   (!data->track.lpocv_info.is_low_density &&
	    ocv_calib_times >= ocv_valid_time)))
		sprd_fgu_sr_precision_cap_calib(data);
	else if (data->sr_calib_mode == SR_CALIB_MODERATE &&
	    ocv_calib_times >= ocv_valid_time)
		sprd_fgu_sr_moderate_cap_calib(data);
	else if (data->sr_calib_mode == SR_CALIB_COARSE &&
	    ocv_calib_times >= ocv_valid_time)
		sprd_fgu_sr_coarse_cap_calib(data);
}

static bool battery_get_psy(struct sprd_fgu_data *data)
{
	data->batt_psy = power_supply_get_by_name("battery");
	if (!data->batt_psy) {
		dev_err(data->dev, "failed to get batt_psy\n");
		return false;
	}
	return true;
}

static void low_vbat_power_off(struct sprd_fgu_data *data)
{
	int ret;
	int force_shutdown_volt = SW_LOW_BAT_UVLO_CONF_MV;
	static int count = 0;
	struct sprd_fgu_info *fgu_info = data->fgu_info;
	struct sprd_fgu_vbat_info *vbat_info = &data->vbat_info;
	int abs_zero_point_uv = 0;

	if (!data->batt_psy) {
		if (!battery_get_psy(data)) {
			dev_err(data->dev, "%s failed to get battery psy\n", __func__);
			return;
		}
	}
	if (data->online)
		force_shutdown_volt = USB_ONLINE_LOW_BAT_CONF_MV;

	abs_zero_point_uv = sprd_fgu_get_abs_zp_voltage_by_temp(data, data->bat_temp);
	abs_zero_point_uv -= data->dischg_dynamic_zp_uv;

	force_shutdown_volt = (abs_zero_point_uv + 100000) / 1000;// 3.3V
	if(force_shutdown_volt >= 3300)
		force_shutdown_volt = USB_ONLINE_LOW_BAT_CONF_MV;
	else if(force_shutdown_volt < 3150)
		force_shutdown_volt = 3200;

	if(data->bat_temp < 0 && data->bat_temp >= -150){
		force_shutdown_volt = 3150;
	}
	else if(data->bat_temp < -150)
		force_shutdown_volt = 3100;

	// check vbat<=3.25V 4 times
	do {
		if (vbat_info->vbat_mv <= force_shutdown_volt) {
			count++;
			msleep(1000);
			ret = fgu_info->ops->get_vbat_now(fgu_info, &vbat_info->vbat_mv);
			continue;
		} else {
			count = 0;
			break;
		}
	} while (count < 5);
	if (count > 4) {
		if ((data->bat_temp < 0 && data->vbat_info.vbat_cur_avg_ma < -1200)
			|| (data->bat_temp < -150 && data->vbat_info.vbat_cur_avg_ma < -500)) {
			if(data->bat_soc <= 15)
				data->batt_cap_level_critical = 1;
			else
				data->batt_cap_level_critical = 0;
		} else {
			data->batt_cap_level_critical = 1;
		}
		power_supply_changed(data->batt_psy);
		return;
	}
}

static void sprd_fgu_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sprd_fgu_data *data = container_of(dwork, struct sprd_fgu_data, fgu_work);
	int temp, ret = 0, work_cycle = SPRD_FGU_NORMAL_WORK_10S, bat_soc;
	int sleep_time, i, temp_update_num = 1;
	struct sprd_fgu_info *fgu_info;
	struct timespec64 cur_time;

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}
	fgu_info = data->fgu_info;

	if (data->sr_resume_temp_update) {
		sleep_time = data->awake_times - data->sleep_times;
		temp_update_num = sleep_time / data->work_cycle;
		temp_update_num = clamp(temp_update_num, 1, SPRD_FGU_TEMP_BUFF_CNT);
		data->sr_resume_temp_update = false;
		dev_info(data->dev, "sleep_time = %d, temp_update_num = %d\n",
			 sleep_time, temp_update_num);
	}

	if (data->volt_low_flag) {
		data->volt_low_flag = false;
		sprd_fgu_update_alarm_cap(data);
		power_supply_changed(data->battery);
	}

	for (i = 0; i < temp_update_num; i++)
		ret |= sprd_fgu_get_temp(data, &temp);
	if (ret) {
		dev_err(data->dev, "failed to get temp, ret = %d\n", ret);
		goto out;
	}

	if (abs(data->last_bat_temp - data->bat_temp) > 10) {
		sprd_fgu_save_last_shutdown_batt_temp(data, data->bat_temp);
		data->last_bat_temp = data->bat_temp;
	}

	ret = fgu_info->ops->get_cc_uah(fgu_info, &data->work_enter_cc_uah, false);
	if (ret) {
		dev_err(data->dev, "failed get work_enter_cc_uah!!\n");
		goto out;
	}

	cur_time = ktime_to_timespec64(ktime_get_boottime());
	data->work_enter_times = cur_time.tv_sec;

	ret = sprd_fgu_calc_ibat_avg(data);
	if (ret)
		dev_err(data->dev, "failed get ibat avg!!\n");

	ret = sprd_fgu_get_capacity(data, &bat_soc);
	if (ret) {
		dev_err(data->dev, "failed get capacity!!\n");
		goto out;
	}
	data->bat_soc = bat_soc;

	low_vbat_power_off(data);

	sprd_fgu_sr_suspend_resume_calib(data);

	if (data->support_bat_aging)
		sprd_fgu_bat_aging_algo(data, false);

	work_cycle = sprd_fgu_cap_calc_work_cycle(data);
	ret = fgu_info->ops->get_cc_uah(fgu_info, &data->work_exit_cc_uah, false);
	if (ret) {
		dev_err(data->dev, "failed get work_exit_cc_uah!!\n");
		goto out;
	}

	cur_time = ktime_to_timespec64(ktime_get_boottime());
	data->work_exit_times = cur_time.tv_sec;
	if (data->work_enter_times - data->work_cali_exit_times + 1 >= data->work_cycle)
		data->work_cali_exit_times = cur_time.tv_sec;

	data->last_cc_uah = data->work_exit_cc_uah;

	mutex_lock(&data->lock);
	sprd_fgu_check_full_ocv(data);
	sprd_fgu_check_charge_ocv(data);
	mutex_unlock(&data->lock);

out:
	data->work_cycle = work_cycle;
	sprd_fgu_dump_info(data);
	schedule_delayed_work(&data->fgu_work, msecs_to_jiffies(data->work_cycle * 1000));
}

static void sprd_fgu_cap_track_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sprd_fgu_data *data = container_of(dwork,
			struct sprd_fgu_data, cap_track_work);
	int work_cycle = SPRD_FGU_CAPACITY_TRACK_15S;

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	work_cycle = sprd_fgu_cap_track_state_machine(data);

	schedule_delayed_work(&data->cap_track_work, msecs_to_jiffies(work_cycle * 1000));
}

static int sprd_fgu_register_usb_notify(struct sprd_fgu_data *data)
{
	int ret = 0;

	data->usb_phy = devm_usb_get_phy_by_phandle(data->dev, "phys", 0);
	if (IS_ERR(data->usb_phy)) {
		dev_err(data->dev, "failed to find USB phy, ret = %ld\n", PTR_ERR(data->usb_phy));
		return -EPROBE_DEFER;
	}

	if (data->use_typec_extcon) {
		data->edev = extcon_get_edev_by_phandle(data->dev, 0);
		if (IS_ERR(data->edev)) {
			ret = PTR_ERR(data->edev);
			dev_err(data->dev, "failed to find vbus extcon device, ret = %d.\n", ret);
			ret = -EPROBE_DEFER;
			return ret;
		}
		INIT_WORK(&data->typec_extcon_work, sprd_fgu_typec_extcon_work);
		data->extcon_nb.notifier_call = sprd_fgu_extcon_event;
		ret = devm_extcon_register_notifier_all(data->dev, data->edev, &data->extcon_nb);
		if (ret) {
			dev_err(data->dev, "Can't register typec extcon\n");
			ret = -EINVAL;
			return ret;
		}
	} else {
		data->usb_notify.notifier_call = sprd_fgu_usb_change;
		ret = usb_register_notifier(data->usb_phy, &data->usb_notify);
		if (ret)
			dev_err(data->dev, "failed to register notifier:%d\n", ret);
	}

	return ret;
}

static ssize_t sprd_fgu_dump_info_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct sprd_fgu_sysfs *sysfs =
		container_of(attr, struct sprd_fgu_sysfs,
			     attr_sprd_fgu_dump_info);
	struct sprd_fgu_data *data = sysfs->data;

	if (!data) {
		dev_err(dev, "%s sprd_fgu_data is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s sprd_fgu_data is null\n", __func__);
	}

	data->support_debug_log = !data->support_debug_log;

	sprd_fgu_dump_battery_info(data, "dump_info");

	return snprintf(buf, PAGE_SIZE, "[batt present:%d];\n[total_mah:%d];\n"
			"[cc_mah:%d];\n[boot_cap:%d];\n"
			"[max_volt:%d];\n[min_volt:%d];\n"
			"[boot_vol:%d];\n[bat_temp:%d];\n[online:%d];\n"
			"[is_first_poweron:%d];\n[chg_type:%d]\n[support_debug_log:%d]\n",
			data->bat_present, data->total_mah, data->cc_uah / 1000,
			data->boot_cap, data->max_volt_uv,
			data->min_volt_uv,
			data->boot_volt_uv, data->bat_temp, data->online,
			data->is_first_poweron, data->chg_type, data->support_debug_log);
}

static ssize_t sprd_fgu_sel_reg_id_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct sprd_fgu_sysfs *sysfs =
		container_of(attr, struct sprd_fgu_sysfs,
			     attr_sprd_fgu_sel_reg_id);
	struct sprd_fgu_data *data = sysfs->data;

	if (!data) {
		dev_err(dev, "%s sprd_fgu_data is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s sprd_fgu_data is null\n", __func__);
	}

	return snprintf(buf, PAGE_SIZE, "[sel_reg_id:0x%x]\n", data->debug_info.sel_reg_id);
}

static ssize_t sprd_fgu_sel_reg_id_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct sprd_fgu_sysfs *sysfs =
		container_of(attr, struct sprd_fgu_sysfs,
			     attr_sprd_fgu_sel_reg_id);
	struct sprd_fgu_data *data = sysfs->data;
	u32 val;
	int ret;

	if (!data) {
		dev_err(dev, "%s sprd_fgu_data is null\n", __func__);
		return count;
	}

	ret =  kstrtouint(buf, 16, &val);
	if (ret) {
		dev_err(data->dev, "fail to get addr, ret = %d\n", ret);
		return count;
	}

	if (val > SPRD_FGU_REG_MAX) {
		dev_err(data->dev, "val = %d, out of SPRD_FGU_REG_MAX\n", val);
		return count;
	}

	data->debug_info.sel_reg_id = val;

	return count;
}

static ssize_t sprd_fgu_reg_val_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct sprd_fgu_sysfs *sysfs =
		container_of(attr, struct sprd_fgu_sysfs,
			     attr_sprd_fgu_reg_val);
	struct sprd_fgu_data *data = sysfs->data;
	struct sprd_fgu_info *fgu_info;
	u32 reg_val;
	int ret;

	if (!data) {
		dev_err(dev, "%s sprd_fgu_data is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s sprd_fgu_data is null\n", __func__);
	}

	fgu_info = data->fgu_info;
	ret = fgu_info->ops->get_reg_val(fgu_info, data->debug_info.sel_reg_id, &reg_val);
	if (ret)
		return snprintf(buf, PAGE_SIZE, "Fail to read [REG_0x%x], ret = %d\n",
				data->debug_info.sel_reg_id, ret);

	return snprintf(buf, PAGE_SIZE, "[REG_0x%x][0x%x]\n",
			data->debug_info.sel_reg_id, reg_val);
}

static ssize_t sprd_fgu_reg_val_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct sprd_fgu_sysfs *sysfs =
		container_of(attr, struct sprd_fgu_sysfs,
			     attr_sprd_fgu_reg_val);
	struct sprd_fgu_data *data = sysfs->data;
	struct sprd_fgu_info *fgu_info = data->fgu_info;
	u32 reg_val;
	int ret;

	if (!data) {
		dev_err(dev, "%s sprd_fgu_data is null\n", __func__);
		return count;
	}

	ret =  kstrtouint(buf, 16, &reg_val);
	if (ret) {
		dev_err(data->dev, "fail to get addr, ret = %d\n", ret);
		return count;
	}

	dev_info(data->dev, "Try to set [REG_0x%x][0x%x]\n", data->debug_info.sel_reg_id, reg_val);

	ret = fgu_info->ops->set_reg_val(fgu_info, data->debug_info.sel_reg_id, reg_val);
	if (ret)
		dev_err(data->dev, "fail to set [REG_0x%x][0x%x], ret = %d\n",
			data->debug_info.sel_reg_id, reg_val, ret);

	return count;
}

static ssize_t sprd_fgu_enable_sleep_calib_show(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	struct sprd_fgu_sysfs *sysfs =
		container_of(attr, struct sprd_fgu_sysfs,
			     attr_sprd_fgu_enable_sleep_calib);
	struct sprd_fgu_data *data = sysfs->data;
	struct sprd_fgu_info *fgu_info;

	if (!data) {
		dev_err(dev, "%s sprd_fgu_data is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s sprd_fgu_data is null\n", __func__);
	}

	fgu_info = data->fgu_info;
	return snprintf(buf, PAGE_SIZE, "capacity sleep calibration function [%s]\n",
			fgu_info->slp_cap_calib.support_slp_calib ? "Enabled" : "Disabled");
}

static ssize_t sprd_fgu_enable_sleep_calib_store(struct device *dev,
						 struct device_attribute *attr,
						 const char *buf, size_t count)
{
	struct sprd_fgu_sysfs *sysfs =
		container_of(attr, struct sprd_fgu_sysfs,
			     attr_sprd_fgu_enable_sleep_calib);
	struct sprd_fgu_data *data = sysfs->data;
	struct sprd_fgu_info *fgu_info;
	bool enbale_slp_calib;
	int ret;

	if (!data) {
		dev_err(dev, "%s sprd_fgu_data is null\n", __func__);
		return count;
	}

	fgu_info = data->fgu_info;
	ret =  kstrtobool(buf, &enbale_slp_calib);
	if (ret) {
		dev_err(data->dev, "fail to get sleep_calib info, ret = %d\n", ret);
		return count;
	}

	fgu_info->slp_cap_calib.support_slp_calib = enbale_slp_calib;

	dev_info(data->dev, "Try to [%s] capacity sleep calibration function\n",
		 fgu_info->slp_cap_calib.support_slp_calib ? "Enabled" : "Disabled");

	return count;
}

static ssize_t sprd_fgu_relax_cnt_th_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct sprd_fgu_sysfs *sysfs =
		container_of(attr, struct sprd_fgu_sysfs,
			     attr_sprd_fgu_relax_cnt_th);
	struct sprd_fgu_data *data = sysfs->data;
	struct sprd_fgu_info *fgu_info;

	if (!data) {
		dev_err(dev, "%s sprd_fgu_data is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s sprd_fgu_data is null\n", __func__);
	}

	fgu_info = data->fgu_info;
	return snprintf(buf, PAGE_SIZE, "[power_low_cnt_th][%d]\n",
			fgu_info->slp_cap_calib.power_low_counter_threshold);
}

static ssize_t sprd_fgu_relax_cnt_th_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct sprd_fgu_sysfs *sysfs =
		container_of(attr, struct sprd_fgu_sysfs,
			     attr_sprd_fgu_relax_cnt_th);
	struct sprd_fgu_data *data = sysfs->data;
	struct sprd_fgu_info *fgu_info;
	u32 power_low_cnt;
	int ret;

	if (!data) {
		dev_err(dev, "%s sprd_fgu_data is null\n", __func__);
		return count;
	}

	fgu_info = data->fgu_info;
	ret =  kstrtouint(buf, 10, &power_low_cnt);
	if (ret) {
		dev_err(data->dev, "fail to get power_low_cnt info, ret = %d\n", ret);
		return count;
	}

	fgu_info->slp_cap_calib.power_low_counter_threshold = power_low_cnt;

	dev_info(data->dev, "Try to set [power_low_cnt_th] to [%d]\n",
		 fgu_info->slp_cap_calib.power_low_counter_threshold);

	return count;
}

static ssize_t sprd_fgu_relax_cur_th_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct sprd_fgu_sysfs *sysfs =
		container_of(attr, struct sprd_fgu_sysfs,
			     attr_sprd_fgu_relax_cur_th);
	struct sprd_fgu_data *data = sysfs->data;
	struct sprd_fgu_info *fgu_info;

	if (!data) {
		dev_err(dev, "%s sprd_fgu_data is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s sprd_fgu_data is null\n", __func__);
	}

	fgu_info = data->fgu_info;
	return snprintf(buf, PAGE_SIZE, "[relax_cur_th][%d]\n",
			fgu_info->slp_cap_calib.relax_cur_threshold);
}

static ssize_t sprd_fgu_relax_cur_th_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct sprd_fgu_sysfs *sysfs =
		container_of(attr, struct sprd_fgu_sysfs,
			     attr_sprd_fgu_relax_cur_th);
	struct sprd_fgu_data *data = sysfs->data;
	struct sprd_fgu_info *fgu_info;
	u32 relax_cur;
	int ret;

	if (!data) {
		dev_err(dev, "%s sprd_fgu_data is null\n", __func__);
		return count;
	}

	fgu_info = data->fgu_info;
	ret =  kstrtouint(buf, 10, &relax_cur);
	if (ret) {
		dev_err(data->dev, "fail to get relax_cur info, ret = %d\n", ret);
		return count;
	}

	fgu_info->slp_cap_calib.relax_cur_threshold = relax_cur;

	dev_info(data->dev, "Try to set [relax_cur_th] to [%d]\n",
		 fgu_info->slp_cap_calib.relax_cur_threshold);

	return count;
}

static ssize_t sprd_fgu_real_time_calib_show(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct sprd_fgu_sysfs *sysfs =
	   container_of(attr, struct sprd_fgu_sysfs,
			attr_sprd_fgu_real_time_calib);
	struct sprd_fgu_data *data = sysfs->data;
	struct sprd_fgu_info *fgu_info;

	if (!data) {
		dev_err(dev, "%s sprd_fgu_data is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s sprd_fgu_data is null\n", __func__);
	}

	fgu_info = data->fgu_info;
	return snprintf(buf, PAGE_SIZE, "[real_time_calib][%d]\n",
			fgu_info->real_time_calib);
}

static ssize_t sprd_fgu_real_time_calib_store(struct device *dev,
					      struct device_attribute *attr,
					      const char *buf, size_t count)
{
	struct sprd_fgu_sysfs *sysfs =
	   container_of(attr, struct sprd_fgu_sysfs,
			attr_sprd_fgu_real_time_calib);
	struct sprd_fgu_data *data = sysfs->data;
	struct sprd_fgu_info *fgu_info;
	bool real_time_calib_enabled;
	int ret;

	if (!data) {
		dev_err(dev, "%s sprd_fgu_data is null\n", __func__);
		return count;
	}

	fgu_info = data->fgu_info;
	ret = kstrtobool(buf, &real_time_calib_enabled);
	if (ret) {
		dev_err(data->dev, "fail to get real_time_calib info, ret = %d\n", ret);
		return count;
	}

	fgu_info->real_time_calib = real_time_calib_enabled;

	dev_info(data->dev, "Try to set [real_time_calib] to [%d]\n",
		 fgu_info->real_time_calib);

	return count;
}

static ssize_t sprd_fgu_capacity_remap_enable_show(struct device *dev,
						   struct device_attribute *attr,
						   char *buf)
{
	struct sprd_fgu_sysfs *sysfs =
	   container_of(attr, struct sprd_fgu_sysfs,
			attr_sprd_fgu_capacity_remap_enable);
	struct sprd_fgu_data *data = sysfs->data;
	struct sprd_fgu_info *fgu_info;

	if (!data) {
		dev_err(dev, "%s sprd_fgu_data is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s sprd_fgu_data is null\n", __func__);
	}

	fgu_info = data->fgu_info;
	return snprintf(buf, PAGE_SIZE, "[capacity_remap_enable][%d]\n",
			fgu_info->capacity_remap_enable);
}

static ssize_t sprd_fgu_capacity_remap_enable_store(struct device *dev,
						    struct device_attribute *attr,
						    const char *buf, size_t count)
{
	struct sprd_fgu_sysfs *sysfs =
	   container_of(attr, struct sprd_fgu_sysfs,
			attr_sprd_fgu_capacity_remap_enable);
	struct sprd_fgu_data *data = sysfs->data;
	struct sprd_fgu_info *fgu_info;
	bool capacity_remap_enabled;
	int ret;

	if (!data) {
		dev_err(dev, "%s sprd_fgu_data is null\n", __func__);
		return count;
	}

	fgu_info = data->fgu_info;
	ret = kstrtobool(buf, &capacity_remap_enabled);
	if (ret) {
		dev_err(data->dev, "fail to get capacity_remap_enable info, ret = %d\n", ret);
		return count;
	}

	fgu_info->capacity_remap_enable = capacity_remap_enabled;

	dev_info(data->dev, "Try to set [capacity_remap_enable] to [%d]\n",
		 fgu_info->capacity_remap_enable);

	return count;
}

static ssize_t sprd_fgu_capacity_hc_enable_show(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	struct sprd_fgu_sysfs *sysfs =
	   container_of(attr, struct sprd_fgu_sysfs,
			attr_sprd_fgu_capacity_hc_enable);
	struct sprd_fgu_data *data = sysfs->data;
	struct sprd_fgu_info *fgu_info;

	if (!data) {
		dev_err(dev, "%s sprd_fgu_data is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s sprd_fgu_data is null\n", __func__);
	}

	fgu_info = data->fgu_info;
	return snprintf(buf, PAGE_SIZE, "[capacity_hc_enable][%d]\n",
			fgu_info->capacity_hc_enable);
}

static ssize_t sprd_fgu_capacity_hc_enable_store(struct device *dev,
						 struct device_attribute *attr,
						 const char *buf, size_t count)
{
	struct sprd_fgu_sysfs *sysfs =
	   container_of(attr, struct sprd_fgu_sysfs,
			attr_sprd_fgu_capacity_hc_enable);
	struct sprd_fgu_data *data = sysfs->data;
	struct sprd_fgu_info *fgu_info;
	bool capacity_hc_enabled;
	int ret;

	if (!data) {
		dev_err(dev, "%s sprd_fgu_data is null\n", __func__);
		return count;
	}

	fgu_info = data->fgu_info;
	ret = kstrtobool(buf, &capacity_hc_enabled);
	if (ret) {
		dev_err(data->dev, "fail to get capacity_hc_enable info, ret = %d\n", ret);
		return count;
	}

	fgu_info->capacity_hc_enable = capacity_hc_enabled;

	dev_info(data->dev, "Try to set [capacity_hc_enable] to [%d]\n",
		 fgu_info->capacity_hc_enable);

	return count;
}

/*
 * SYSFS interfaces:
 * /sys/devices/platform/soc/soc:aon/<num.spi>/spi_master/spi4/spi4.num/<num.spi>:pmic@0:fgu@800 \
 * /power_supply/sc27xx-fgu/debug/dump_info           [ro]     [debug]
 * /sys/devices/platform/soc/soc:aon/<num.spi>/spi_master/spi4/spi4.num/<num.spi>:pmic@0:fgu@800 \
 * /power_supply/sc27xx-fgu/debug/enable_sleep_calib  [rw]     [debug]
 * /sys/devices/platform/soc/soc:aon/<num.spi>/spi_master/spi4/spi4.num/<num.spi>:pmic@0:fgu@800 \
 * /power_supply/sc27xx-fgu/debug/reg_val             [rw]     [debug]
 * /sys/devices/platform/soc/soc:aon/<num.spi>/spi_master/spi4/spi4.num/<num.spi>:pmic@0:fgu@800 \
 * /power_supply/sc27xx-fgu/debug/relax_cnt_th        [rw]     [debug]
 * /sys/devices/platform/soc/soc:aon/<num.spi>/spi_master/spi4/spi4.num/<num.spi>:pmic@0:fgu@800 \
 * /power_supply/sc27xx-fgu/debug/relax_cur_th        [rw]     [debug]
 * /sys/devices/platform/soc/soc:aon/<num.spi>/spi_master/spi4/spi4.num/<num.spi>:pmic@0:fgu@800 \
 * /power_supply/sc27xx-fgu/debug/sel_reg_id          [rw]     [debug]
 */
static int sprd_fgu_register_sysfs(struct sprd_fgu_data *data)
{
	struct sprd_fgu_sysfs *sysfs;
	int ret;

	sysfs = devm_kzalloc(data->dev, sizeof(*sysfs), GFP_KERNEL);
	if (!sysfs)
		return -ENOMEM;

	data->sysfs = sysfs;
	sysfs->data = data;
	sysfs->name = "sprd_fgu_sysfs";
	sysfs->attrs[0] = &sysfs->attr_sprd_fgu_dump_info.attr;
	sysfs->attrs[1] = &sysfs->attr_sprd_fgu_sel_reg_id.attr;
	sysfs->attrs[2] = &sysfs->attr_sprd_fgu_reg_val.attr;
	sysfs->attrs[3] = &sysfs->attr_sprd_fgu_enable_sleep_calib.attr;
	sysfs->attrs[4] = &sysfs->attr_sprd_fgu_relax_cnt_th.attr;
	sysfs->attrs[5] = &sysfs->attr_sprd_fgu_relax_cur_th.attr;
	sysfs->attrs[6] = &sysfs->attr_sprd_fgu_real_time_calib.attr;
	sysfs->attrs[7] = &sysfs->attr_sprd_fgu_capacity_remap_enable.attr;
	sysfs->attrs[8] = &sysfs->attr_sprd_fgu_capacity_hc_enable.attr;
	sysfs->attrs[9] = NULL;
	sysfs->attr_g.name = "debug";
	sysfs->attr_g.attrs = sysfs->attrs;

	sysfs_attr_init(&sysfs->attr_sprd_fgu_dump_info.attr);
	sysfs->attr_sprd_fgu_dump_info.attr.name = "dump_info";
	sysfs->attr_sprd_fgu_dump_info.attr.mode = 0444;
	sysfs->attr_sprd_fgu_dump_info.show = sprd_fgu_dump_info_show;

	sysfs_attr_init(&sysfs->attr_sprd_fgu_sel_reg_id.attr);
	sysfs->attr_sprd_fgu_sel_reg_id.attr.name = "sel_reg_id";
	sysfs->attr_sprd_fgu_sel_reg_id.attr.mode = 0644;
	sysfs->attr_sprd_fgu_sel_reg_id.show = sprd_fgu_sel_reg_id_show;
	sysfs->attr_sprd_fgu_sel_reg_id.store = sprd_fgu_sel_reg_id_store;

	sysfs_attr_init(&sysfs->attr_sprd_fgu_reg_val.attr);
	sysfs->attr_sprd_fgu_reg_val.attr.name = "reg_val";
	sysfs->attr_sprd_fgu_reg_val.attr.mode = 0644;
	sysfs->attr_sprd_fgu_reg_val.show = sprd_fgu_reg_val_show;
	sysfs->attr_sprd_fgu_reg_val.store = sprd_fgu_reg_val_store;

	sysfs_attr_init(&sysfs->attr_sprd_fgu_enable_sleep_calib.attr);
	sysfs->attr_sprd_fgu_enable_sleep_calib.attr.name = "enable_sleep_calib";
	sysfs->attr_sprd_fgu_enable_sleep_calib.attr.mode = 0644;
	sysfs->attr_sprd_fgu_enable_sleep_calib.show = sprd_fgu_enable_sleep_calib_show;
	sysfs->attr_sprd_fgu_enable_sleep_calib.store = sprd_fgu_enable_sleep_calib_store;

	sysfs_attr_init(&sysfs->attr_sprd_fgu_relax_cnt_th.attr);
	sysfs->attr_sprd_fgu_relax_cnt_th.attr.name = "relax_cnt_th";
	sysfs->attr_sprd_fgu_relax_cnt_th.attr.mode = 0644;
	sysfs->attr_sprd_fgu_relax_cnt_th.show = sprd_fgu_relax_cnt_th_show;
	sysfs->attr_sprd_fgu_relax_cnt_th.store = sprd_fgu_relax_cnt_th_store;

	sysfs_attr_init(&sysfs->attr_sprd_fgu_relax_cur_th.attr);
	sysfs->attr_sprd_fgu_relax_cur_th.attr.name = "relax_cur_th";
	sysfs->attr_sprd_fgu_relax_cur_th.attr.mode = 0644;
	sysfs->attr_sprd_fgu_relax_cur_th.show = sprd_fgu_relax_cur_th_show;
	sysfs->attr_sprd_fgu_relax_cur_th.store = sprd_fgu_relax_cur_th_store;

	sysfs_attr_init(&sysfs->attr_sprd_fgu_real_time_calib.attr);
	sysfs->attr_sprd_fgu_real_time_calib.attr.name = "real_time_calib";
	sysfs->attr_sprd_fgu_real_time_calib.attr.mode = 0644;
	sysfs->attr_sprd_fgu_real_time_calib.show = sprd_fgu_real_time_calib_show;
	sysfs->attr_sprd_fgu_real_time_calib.store = sprd_fgu_real_time_calib_store;

	sysfs_attr_init(&sysfs->attr_sprd_fgu_capacity_remap_enable.attr);
	sysfs->attr_sprd_fgu_capacity_remap_enable.attr.name = "capacity_remap_enable";
	sysfs->attr_sprd_fgu_capacity_remap_enable.attr.mode = 0644;
	sysfs->attr_sprd_fgu_capacity_remap_enable.show = sprd_fgu_capacity_remap_enable_show;
	sysfs->attr_sprd_fgu_capacity_remap_enable.store = sprd_fgu_capacity_remap_enable_store;

	sysfs_attr_init(&sysfs->attr_sprd_fgu_capacity_hc_enable.attr);
	sysfs->attr_sprd_fgu_capacity_hc_enable.attr.name = "capacity_hc_enable";
	sysfs->attr_sprd_fgu_capacity_hc_enable.attr.mode = 0644;
	sysfs->attr_sprd_fgu_capacity_hc_enable.show = sprd_fgu_capacity_hc_enable_show;
	sysfs->attr_sprd_fgu_capacity_hc_enable.store = sprd_fgu_capacity_hc_enable_store;

	ret = sysfs_create_group(&data->battery->dev.kobj, &sysfs->attr_g);
	if (ret < 0)
		dev_err(data->dev, "Cannot create sysfs , ret = %d\n", ret);

	return ret;
}

static void sprd_fgu_get_battery_cap_temp_ocv_lut(struct sprd_fgu_data *data,
						  struct sprd_battery_info *info)
{
	int i, j, rows, cols;

	data->battery_cap_temp_ocv_lut =
		devm_kzalloc(data->dev, sizeof(struct sprd_cap_temp_ocv_lut), GFP_KERNEL);
	if (!data->battery_cap_temp_ocv_lut)
		return;

	cols = info->battery_cap_temp_ocv_lut->cols;
	rows = info->battery_cap_temp_ocv_lut->rows;
	data->battery_cap_temp_ocv_lut->cols = cols;
	data->battery_cap_temp_ocv_lut->rows = rows;
	for (i = 0; i < cols; i++)
		data->battery_cap_temp_ocv_lut->temp[i] = info->battery_cap_temp_ocv_lut->temp[i];

	for (i = 0; i < rows; i++)
		data->battery_cap_temp_ocv_lut->cap[i] = info->battery_cap_temp_ocv_lut->cap[i];

	for (i = 0; i < rows; i++) {
		for (j = 0; j < cols; j++)
			data->battery_cap_temp_ocv_lut->ocv[i][j] =
				info->battery_cap_temp_ocv_lut->ocv[i][j];
	}
}

static void sprd_fgu_get_battery_cap_temp_ocv_cycle_lut(struct sprd_fgu_data *data,
							struct sprd_battery_info *info)
{
	int i, j, rows, cols;

	data->battery_cap_temp_ocv_cycle_lut =
		devm_kzalloc(data->dev, sizeof(struct sprd_cap_temp_ocv_cycle_lut), GFP_KERNEL);
	if (!data->battery_cap_temp_ocv_cycle_lut)
		return;

	cols = info->battery_cap_temp_ocv_cycle_lut->cols;
	rows = info->battery_cap_temp_ocv_cycle_lut->rows;
	data->battery_cap_temp_ocv_cycle_lut->cols = cols;
	data->battery_cap_temp_ocv_cycle_lut->rows = rows;
	for (i = 0; i < cols; i++)
		data->battery_cap_temp_ocv_cycle_lut->temp[i] =
			info->battery_cap_temp_ocv_cycle_lut->temp[i];

	for (i = 0; i < rows; i++)
		data->battery_cap_temp_ocv_cycle_lut->cap[i] =
			info->battery_cap_temp_ocv_cycle_lut->cap[i];

	for (i = 0; i < rows; i++) {
		for (j = 0; j < cols; j++)
			data->battery_cap_temp_ocv_cycle_lut->cycle_ocv[i][j] =
				info->battery_cap_temp_ocv_cycle_lut->cycle_ocv[i][j];
	}
}

static void sprd_fgu_get_battery_cap_temp_ibat_lut(struct sprd_fgu_data *data,
						   struct sprd_battery_info *info)
{
	int i, j, rows, cols;


	data->battery_cap_temp_ibat_lut =
		devm_kzalloc(data->dev, sizeof(struct sprd_cap_temp_ibat_lut), GFP_KERNEL);
	if (!data->battery_cap_temp_ibat_lut)
		return;

	cols = info->battery_cap_temp_ibat_lut->cols;
	rows = info->battery_cap_temp_ibat_lut->rows;
	data->battery_cap_temp_ibat_lut->cols = cols;
	data->battery_cap_temp_ibat_lut->rows = rows;
	for (i = 0; i < cols; i++)
		data->battery_cap_temp_ibat_lut->temp[i] =
			info->battery_cap_temp_ibat_lut->temp[i];

	for (i = 0; i < rows; i++)
		data->battery_cap_temp_ibat_lut->cap[i] =
			info->battery_cap_temp_ibat_lut->cap[i];

	for (i = 0; i < rows; i++) {
		for (j = 0; j < cols; j++)
			data->battery_cap_temp_ibat_lut->ibat[i][j] =
				info->battery_cap_temp_ibat_lut->ibat[i][j];
	}
}

static void sprd_fgu_get_battery_cap_temp_resist_lut(struct sprd_fgu_data *data,
						     struct sprd_battery_info *info)
{
	int i, j, rows, cols;

	data->battery_cap_temp_resist_lut =
		devm_kzalloc(data->dev, sizeof(struct sprd_cap_temp_resist_lut), GFP_KERNEL);
	if (!data->battery_cap_temp_resist_lut)
		return;

	cols = info->battery_cap_temp_resist_lut->cols;
	rows = info->battery_cap_temp_resist_lut->rows;
	data->battery_cap_temp_resist_lut->cols = cols;
	data->battery_cap_temp_resist_lut->rows = rows;
	for (i = 0; i < cols; i++)
		data->battery_cap_temp_resist_lut->temp[i] =
			info->battery_cap_temp_resist_lut->temp[i];

	for (i = 0; i < rows; i++)
		data->battery_cap_temp_resist_lut->cap[i] =
			info->battery_cap_temp_resist_lut->cap[i];

	for (i = 0; i < rows; i++) {
		for (j = 0; j < cols; j++)
			data->battery_cap_temp_resist_lut->resist[i][j] =
				info->battery_cap_temp_resist_lut->resist[i][j];
	}
}

static void sprd_fgu_get_battery_cap_temp_resist_ratio_lut(struct sprd_fgu_data *data,
							   struct sprd_battery_info *info)
{
	int i, j, rows, cols, index, len;
	struct sprd_cap_temp_resist_ratio_lut *info_table, *data_table;

	len = info->battery_cap_temp_resist_ratio_lut_len;
	data->battery_cap_temp_resist_ratio_lut_len = len;
	for (index = 0; index < len; index++) {

		data->battery_cap_temp_resist_ratio_lut[index] =
			 devm_kzalloc(data->dev, sizeof(struct sprd_cap_temp_resist_ratio_lut),
				      GFP_KERNEL);
		if (!data->battery_cap_temp_resist_ratio_lut[index])
			return;

		cols = info->battery_cap_temp_resist_ratio_lut[index]->cols;
		rows = info->battery_cap_temp_resist_ratio_lut[index]->rows;
		data->battery_cap_temp_resist_ratio_lut[index]->cols = cols;
		data->battery_cap_temp_resist_ratio_lut[index]->rows = rows;
		info_table = info->battery_cap_temp_resist_ratio_lut[index];
		data_table = data->battery_cap_temp_resist_ratio_lut[index];
		for (i = 0; i < cols; i++)
			data_table->temp[i] = info_table->temp[i];

		for (i = 0; i < rows; i++)
			data_table->cap[i] = info_table->cap[i];

		for (i = 0; i < rows; i++) {
			for (j = 0; j < cols; j++)
				data_table->res_ratio[i][j] = info_table->res_ratio[i][j];
		}

		data->battery_cap_temp_resist_ratio_table[index] =
			info->battery_cap_temp_resist_ratio_table[index];
	}
}

static void sprd_fgu_get_battery_temp_zp_vol_lut(struct sprd_fgu_data *data,
						 struct sprd_battery_info *info)
{
	int i, cols;

	data->battery_temp_zp_vol_lut =
		devm_kzalloc(data->dev, sizeof(struct sprd_temp_zp_voltage_lut), GFP_KERNEL);
	if (!data->battery_temp_zp_vol_lut)
		return;

	cols = info->battery_temp_zp_vol_lut->cols;
	data->battery_temp_zp_vol_lut->cols = cols;
	for (i = 0; i < cols; i++) {
		data->battery_temp_zp_vol_lut->vol[i] = info->battery_temp_zp_vol_lut->vol[i];
		data->battery_temp_zp_vol_lut->temp[i] = info->battery_temp_zp_vol_lut->temp[i];
	}
}

static void sprd_fgu_get_battery_temp_abs_zp_vol_lut(struct sprd_fgu_data *data,
						     struct sprd_battery_info *info)
{
	int i, cols;

	data->battery_temp_abs_zp_vol_lut =
		devm_kzalloc(data->dev, sizeof(struct sprd_temp_abs_zp_voltage_lut), GFP_KERNEL);
	if (!data->battery_temp_abs_zp_vol_lut)
		return;

	cols = info->battery_temp_abs_zp_vol_lut->cols;
	data->battery_temp_abs_zp_vol_lut->cols = cols;
	for (i = 0; i < cols; i++) {
		data->battery_temp_abs_zp_vol_lut->vol[i] =
			info->battery_temp_abs_zp_vol_lut->vol[i];
		data->battery_temp_abs_zp_vol_lut->temp[i] =
			info->battery_temp_abs_zp_vol_lut->temp[i];
	}
}

static void sprd_fgu_get_battery_temp_bat_vol_lut(struct sprd_fgu_data *data,
						  struct sprd_battery_info *info)
{
	int i, cols;

	data->battery_temp_bat_vol_lut =
		devm_kzalloc(data->dev, sizeof(struct sprd_temp_bat_voltage_lut), GFP_KERNEL);
	if (!data->battery_temp_bat_vol_lut)
		return;

	cols = info->battery_temp_bat_vol_lut->cols;
	data->battery_temp_bat_vol_lut->cols = cols;
	for (i = 0; i < cols; i++) {
		data->battery_temp_bat_vol_lut->vol[i] = info->battery_temp_bat_vol_lut->vol[i];
		data->battery_temp_bat_vol_lut->temp[i] = info->battery_temp_bat_vol_lut->temp[i];
	}
}


static void sprd_fgu_get_battery_temp_fcc_lut(struct sprd_fgu_data *data,
					      struct sprd_battery_info *info)
{
	int i, cols;

	data->battery_temp_fcc_lut =
		devm_kzalloc(data->dev, sizeof(struct sprd_temp_fcc_lut), GFP_KERNEL);
	if (!data->battery_temp_fcc_lut)
		return;

	cols = info->battery_temp_fcc_lut->cols;
	data->battery_temp_fcc_lut->cols = cols;
	for (i = 0; i < cols; i++) {
		data->battery_temp_fcc_lut->fcc[i] = info->battery_temp_fcc_lut->fcc[i];
		data->battery_temp_fcc_lut->temp[i] = info->battery_temp_fcc_lut->temp[i];
	}
}

static void sprd_fgu_get_battery_temp_hc_lut(struct sprd_fgu_data *data,
					     struct sprd_battery_info *info)
{
	int i, cols;

	data->battery_temp_hc_lut =
		devm_kzalloc(data->dev, sizeof(struct sprd_temp_hc_lut), GFP_KERNEL);
	if (!data->battery_temp_hc_lut)
		return;

	cols = info->battery_temp_hc_lut->cols;
	data->battery_temp_hc_lut->cols = cols;
	for (i = 0; i < cols; i++) {
		data->battery_temp_hc_lut->hc[i] = info->battery_temp_hc_lut->hc[i];
		data->battery_temp_hc_lut->temp[i] = info->battery_temp_hc_lut->temp[i];
	}
}

static void sprd_fgu_get_battery_temp_chg_loss_lut(struct sprd_fgu_data *data,
						   unsigned int fullbatt_uA,
						   int fullbatt_uv)
{
	int i, cols;
	unsigned int fullbatt_mA = fullbatt_uA / 1000;
	int fullbatt_mv = fullbatt_uv / 1000;
	int ocv_uv, soc, temp, fcc_uah, fcc_mah, soc_loss, uah_loss;

	if (!data->battery_temp_chg_loss_lut) {
		data->battery_temp_chg_loss_lut =
			devm_kzalloc(data->dev, sizeof(struct sprd_temp_chg_loss_lut), GFP_KERNEL);
		if (!data->battery_temp_chg_loss_lut)
			return;
	}

	dev_info(data->dev, "fullbatt_mA = %d, fullbatt_mv = %d\n",
		 fullbatt_mA, fullbatt_mv);

	data->new_chg_term_currnet_uA = fullbatt_uA;
	data->new_chg_term_voltage_uv = fullbatt_uv;
	cols = data->battery_temp_fcc_lut->cols;
	data->battery_temp_chg_loss_lut->cols = cols;
	for (i = 0; i < cols; i++)
		data->battery_temp_chg_loss_lut->temp[i] = data->battery_temp_fcc_lut->temp[i];

	for (i = 0; i < cols; i++) {
		temp = data->battery_temp_chg_loss_lut->temp[i] * 10;
		ocv_uv = sprd_fgu_get_charge_ocv(data, fullbatt_mv * 1000, temp,
						 fullbatt_mA, data->charge_cycle, false);
		soc = sprd_fgu_get_soc_by_ocv(data, ocv_uv, temp);
		fcc_uah = sprd_fgu_get_fcc_uah(data, temp, data->charge_cycle);
		fcc_mah = fcc_uah / 1000;
		soc_loss = 1000 - soc;
		soc_loss = clamp(soc_loss, 0, 500);
		uah_loss = fcc_mah * soc_loss;
		data->battery_temp_chg_loss_lut->chg_loss_uah[i] = uah_loss;
		data->battery_temp_chg_loss_lut->ocv[i] = ocv_uv;
		data->battery_temp_chg_loss_lut->soc_loss[i] = soc_loss;
		dev_info(data->dev, "i = %d:temp = %d, ocv_uv = %d, soc = %d, fcc_uah = %d, soc_loss = %d, uah_loss = %d\n",
			 i, temp, ocv_uv, soc, fcc_uah, soc_loss, uah_loss);
	}
}

static void sprd_fgu_get_battery_cycle_temp_ratio_lut(struct sprd_fgu_data *data,
						      struct sprd_battery_info *info)
{
	int i, j, rows, cols;

	data->battery_fcc_cycle_ratio_lut =
		devm_kzalloc(data->dev, sizeof(struct sprd_fcc_cycle_ratio_lut), GFP_KERNEL);
	if (!data->battery_fcc_cycle_ratio_lut)
		return;

	cols = info->battery_fcc_cycle_ratio_lut->cols;
	rows = info->battery_fcc_cycle_ratio_lut->rows;
	data->battery_fcc_cycle_ratio_lut->cols = cols;
	data->battery_fcc_cycle_ratio_lut->rows = rows;
	for (i = 0; i < cols; i++)
		data->battery_fcc_cycle_ratio_lut->temp[i] =
			info->battery_fcc_cycle_ratio_lut->temp[i];

	for (i = 0; i < rows; i++)
		data->battery_fcc_cycle_ratio_lut->cycle[i] =
			info->battery_fcc_cycle_ratio_lut->cycle[i];

	for (i = 0; i < rows; i++) {
		for (j = 0; j < cols; j++)
			data->battery_fcc_cycle_ratio_lut->ratio[i][j] =
				info->battery_fcc_cycle_ratio_lut->ratio[i][j];
	}
}

static int sprd_fgu_parse_sprd_battery_info(struct sprd_fgu_data *data,
					    struct sprd_battery_info *info)
{
	int i;

	if (info->battery_cap_temp_ocv_lut &&
	    info->battery_cap_temp_ocv_lut->cols > 0 &&
	    info->battery_cap_temp_ocv_lut->rows > 0)
		sprd_fgu_get_battery_cap_temp_ocv_lut(data, info);
	if (!data->battery_cap_temp_ocv_lut)
		return -ENOMEM;

	if (info->battery_cap_temp_ibat_lut &&
	    info->battery_cap_temp_ibat_lut->cols > 0 &&
	    info->battery_cap_temp_ibat_lut->rows > 0) {
		sprd_fgu_get_battery_cap_temp_ibat_lut(data, info);
		if (!data->battery_cap_temp_ibat_lut)
			return -ENOMEM;
	}

	if (info->battery_cap_temp_ocv_cycle_lut &&
	    info->battery_cap_temp_ocv_cycle_lut->cols > 0 &&
	    info->battery_cap_temp_ocv_cycle_lut->rows > 0) {
		sprd_fgu_get_battery_cap_temp_ocv_cycle_lut(data, info);
		if (!data->battery_cap_temp_ocv_cycle_lut)
			return -ENOMEM;
	}

	if (info->battery_temp_fcc_lut && info->battery_temp_fcc_lut->cols > 0)
		sprd_fgu_get_battery_temp_fcc_lut(data, info);
	if (!data->battery_temp_fcc_lut)
		return -ENOMEM;

	if (info->battery_temp_hc_lut && info->battery_temp_hc_lut->cols > 0) {
		sprd_fgu_get_battery_temp_hc_lut(data, info);
		if (!data->battery_temp_hc_lut)
			return -ENOMEM;
	}

	if (info->battery_fcc_cycle_ratio_lut &&
	    info->battery_fcc_cycle_ratio_lut->cols > 0 &&
	    info->battery_fcc_cycle_ratio_lut->rows > 0) {
		sprd_fgu_get_battery_cycle_temp_ratio_lut(data, info);
		if (!data->battery_fcc_cycle_ratio_lut)
			return -ENOMEM;
	}

	if (info->battery_temp_zp_vol_lut && info->battery_temp_zp_vol_lut->cols > 0) {
		sprd_fgu_get_battery_temp_zp_vol_lut(data, info);
		if (!data->battery_temp_zp_vol_lut)
			return -ENOMEM;
	}

	if (info->battery_temp_abs_zp_vol_lut && info->battery_temp_abs_zp_vol_lut->cols > 0) {
		sprd_fgu_get_battery_temp_abs_zp_vol_lut(data, info);
		if (!data->battery_temp_abs_zp_vol_lut)
			return -ENOMEM;
	}

	if (info->battery_temp_bat_vol_lut && info->battery_temp_bat_vol_lut->cols > 0)
		sprd_fgu_get_battery_temp_bat_vol_lut(data, info);
	if (!data->battery_temp_bat_vol_lut)
		return -ENOMEM;

	if (info->battery_cap_temp_resist_lut &&
	    info->battery_cap_temp_resist_lut->cols > 0 &&
	    info->battery_cap_temp_resist_lut->rows > 0)
		sprd_fgu_get_battery_cap_temp_resist_lut(data, info);
	if (!data->battery_cap_temp_resist_lut)
		return -ENOMEM;

	if (info->battery_cap_temp_resist_ratio_lut_len > 0 &&
	    info->battery_cap_temp_resist_ratio_lut[0]->cols > 0 &&
	    info->battery_cap_temp_resist_ratio_lut[0]->rows > 0) {
		sprd_fgu_get_battery_cap_temp_resist_ratio_lut(data, info);
		if (!data->battery_cap_temp_resist_ratio_lut[0])
			return -ENOMEM;
	}

	data->vbat_thres_temp_table_len = info->vbat_thres_temp_table_len;
	if (data->vbat_thres_temp_table_len > 0) {
		data->vbat_thres_temp_table =
			devm_kmemdup(data->dev, info->vbat_thres_temp_table,
				     (u32)data->vbat_thres_temp_table_len * sizeof(int),
				     GFP_KERNEL);
		if (!data->vbat_thres_temp_table)
			return -ENOMEM;
	}

	data->high_int_thres_cols = info->high_int_thres_cols;
	data->low_int_thres_cols = info->low_int_thres_cols;
	if (data->high_int_thres_cols > 0 && data->low_int_thres_cols > 0) {
		data->high_int_thres_table =
			devm_kzalloc(data->dev, (u32)data->vbat_thres_temp_table_len *
				     sizeof(int), GFP_KERNEL);
		if (!data->high_int_thres_table) {
			dev_err(data->dev, "Fail to alloc high_int_thres_table\n");
			return -ENOMEM;
		}
		data->low_int_thres_table =
			devm_kzalloc(data->dev, (u32)data->vbat_thres_temp_table_len *
				     sizeof(int), GFP_KERNEL);
		if (!data->high_int_thres_table) {
			dev_err(data->dev, "Fail to alloc high_int_thres_table\n");
			return -ENOMEM;
		}

		for (i = 0; i < data->vbat_thres_temp_table_len; i++) {
			data->high_int_thres_table[i] =
				devm_kmemdup(data->dev,
					     info->high_int_thres_table[i],
					     (u32)data->high_int_thres_cols *
					     sizeof(int),
					     GFP_KERNEL);
			if (!data->high_int_thres_table[i]) {
				dev_err(data->dev, "data->high_int_thres_table[%d]\n", i);
				return -ENOMEM;
			}

			data->low_int_thres_table[i] =
				devm_kmemdup(data->dev,
					     info->low_int_thres_table[i],
					     (u32)data->low_int_thres_cols *
					     sizeof(int),
					     GFP_KERNEL);
			if (!data->low_int_thres_table[i]) {
				dev_err(data->dev, "data->low_int_thres_table[%d]\n", i);
				return -ENOMEM;
			}
		}
	}

	if (info->fullbatt_voltage_uv > 0)
		data->fullbatt_uV = info->fullbatt_voltage_uv;
	if (info->fullbatt_current_uA > 0)
		data->fullbatt_uA = info->fullbatt_current_uA;

	data->temp_table_len = info->battery_vol_temp_table_len;
	if (data->temp_table_len > 0) {
		data->temp_table = devm_kmemdup(data->dev, info->battery_vol_temp_table,
						data->temp_table_len *
						sizeof(struct sprd_battery_vol_temp_table),
						GFP_KERNEL);
		if (!data->temp_table)
			return -ENOMEM;
	}

	data->fullcap_table_len = info->battery_temp_fullcap_table_len;
	if (data->fullcap_table_len > 0) {
		data->temp_fullcap_table =
			devm_kmemdup(data->dev, info->battery_temp_fullcap_table,
				     data->fullcap_table_len *
				     sizeof(struct sprd_battery_temp_fullcap_table),
				     GFP_KERNEL);
		if (!data->temp_fullcap_table)
			return -ENOMEM;
	}

	data->resist_table_len = info->battery_temp_resist_table_len;
	if (data->resist_table_len > 0) {
		data->resist_table =
			devm_kmemdup(data->dev, info->battery_temp_resist_table,
				     data->resist_table_len *
				     sizeof(struct sprd_battery_resistance_temp_table),
				     GFP_KERNEL);
		if (!data->resist_table)
			return -ENOMEM;
	}

	data->cap_calib_dens_ocv_table_len = info->cap_calib_dens_ocv_table_len;
	if (data->cap_calib_dens_ocv_table_len > 0) {
		data->cap_calib_dens_ocv_table =
			devm_kmemdup(data->dev, info->cap_calib_dens_ocv_table,
				     (u32)data->cap_calib_dens_ocv_table_len *
				     sizeof(density_ocv_table),
				     GFP_KERNEL);
		if (!data->cap_calib_dens_ocv_table) {
			dev_err(data->dev, "data->cap_calib_dens_ocv_table is null\n");
			return -ENOMEM;
		}
	}

	if (info->fullbatt_track_end_voltage_uv > 0)
		data->track.end_vol = info->fullbatt_track_end_voltage_uv / 1000;
	else
		dev_warn(data->dev, "no fgu track.end_vol support\n");

	if (info->fullbatt_track_end_current_uA > 0)
		data->track.end_cur = info->fullbatt_track_end_current_uA / 1000;
	else
		dev_warn(data->dev, "no fgu track.end_cur support\n");

	if (info->batt_ovp_threshold_uv > 0) {
		data->batt_ovp_threshold = info->batt_ovp_threshold_uv / 1000;
	} else {
		data->batt_ovp_threshold = SPRD_FGU_VBAT_DEF_THRES_MAX;
		dev_warn(data->dev, "fgu battery ovp threshold is not define, use default value\n");
	}

	data->temp_default_fcc_mah = sprd_fgu_get_fcc_uah_by_temp(data,
								  SPRD_FGU_TEMP_DEFAULT) / 1000;
	data->total_mah = data->temp_default_fcc_mah;

	if (info->charge_full_design_uah > 0)
		data->design_mah = info->charge_full_design_uah / 1000;
	else
		data->design_mah = data->temp_default_fcc_mah;

	if (info->shutdown_voltage_uv > 0)
		data->shutdown_voltage_mv = info->shutdown_voltage_uv / 1000;
	else
		dev_warn(data->dev, "no fgu shutdown_voltage_mv support\n");

	if (info->charge_full_uah > 0)
		data->charge_full_mah = info->charge_full_uah / 1000;
	else
		dev_warn(data->dev, "no fgu charge_full_uah support\n");

	if (data->dynamic_update_bat_para_flag) {
		if (data->track.learned_mah > 0) {
			if (!sprd_fgu_adjust_aging_fcc(data)) {
				data->total_mah =
					sprd_fgu_adjust_fcc_uah_by_temp(data,
									SPRD_FGU_TEMP_DEFAULT) /
					1000;
				data->track.learned_mah = data->total_mah;
			}
		}
	}

	if (info->constant_charge_voltage_max_uv > 0)
		data->max_volt_uv = info->constant_charge_voltage_max_uv;
	else
		dev_warn(data->dev, "no fgu constant_charge_voltage_max_uv support\n");

	if (info->factory_internal_resistance_uohm > 0)
		data->internal_resist = info->factory_internal_resistance_uohm / 1000;
	else
		dev_warn(data->dev, "no fgu factory_internal_resistance_uohm support\n");

	if (info->voltage_min_design_uv > 0) {
		data->min_volt_uv = info->voltage_min_design_uv;
	} else {
		data->min_volt_uv = SPRD_FGU_VBAT_DEF_THRES_MIN;
		dev_warn(data->dev, "fgu voltage_min_design_uv is not define, use default value\n");
	}

	if (data->dynamic_update_bat_para_flag)
		sprd_fgu_get_battery_temp_chg_loss_lut(data, data->fullbatt_uA, data->max_volt_uv);

	if (data->support_debug_log)
		sprd_fgu_dump_battery_info(data, "parse_resistance_table");

	return 0;
}

static int sprd_fgu_get_bat_para_table(struct sprd_fgu_data *data, int aging_bat_id)
{
	int ret = 0;
	struct sprd_battery_info info = {0};

	ret = sprd_battery_get_battery_info(data->battery, &info, aging_bat_id);
	if (ret) {
		sprd_battery_put_battery_info(data->battery, &info);
		dev_err(data->dev, "failed to get sprd battery information\n");
		return ret;
	}

	ret = sprd_fgu_parse_sprd_battery_info(data, &info);
	sprd_battery_put_battery_info(data->battery, &info);
	if (ret)
		dev_err(data->dev, "failed to parse battery information, ret = %d\n", ret);

	return ret;
}

static int sprd_fgu_hw_config(struct sprd_fgu_data *data)
{
	int ret = 0;
	struct sprd_fgu_info *fgu_info = data->fgu_info;

	/* Enable the FGU module and FGU RTC clock to make it work*/
	ret = fgu_info->ops->enable_fgu_module(fgu_info, true);
	if (ret)
		return ret;

	ret = fgu_info->ops->clr_fgu_int(fgu_info);
	if (ret) {
		dev_err(data->dev, "failed to clear interrupt status\n");
		return ret;
	}

	/*
	 * Set the voltage low overload threshold, which means when the battery
	 * voltage is lower than this threshold, the controller will generate
	 * one interrupt to notify.
	 */
	ret = fgu_info->ops->set_low_overload(fgu_info, data->min_volt_uv / 1000);
	if (ret) {
		dev_err(data->dev, "failed to set fgu low overload\n");
		return ret;
	}
	fgu_info->ops->enable_fgu_int(fgu_info, SPRD_FGU_VOLT_LOW_INT_CMD, true);

	/*
	 * Set the capacity delta threshold, that means when the capacity
	 * change is multiples of the delta threshold, the controller
	 * will generate one interrupt to notify the users to update the battery
	 * capacity. Now we set the 1% capacity value.
	 */
	ret = fgu_info->ops->set_cap_delta_thre(fgu_info, data->total_mah, 10);
	if (ret)
		return ret;

	ret = fgu_info->ops->enable_relax_cnt_mode(fgu_info);
	if (ret) {
		dev_err(data->dev, "Fail to enable RELAX_CNT_MODE, re= %d\n", ret);
		return ret;
	}

	if (data->batt_ovp_threshold) {
		ret = sprd_fgu_batt_ovp_threshold_config(data);
		if (ret)
			dev_err(data->dev, "failed to set overload thershold config\n");
	}

	data->vbat_level = 0;

	return ret;
}

static int sprd_fgu_hw_init(struct sprd_fgu_data *data)
{
	int i, ret;
	struct sprd_fgu_info *fgu_info = data->fgu_info;
	struct timespec64 cur_time;

	data->cur_now_buff[SPRD_FGU_CURRENT_BUFF_CNT - 1] = SPRD_FGU_MAGIC_NUMBER;

	/*
	 * We should give a initial temperature value of temp_buff.
	 */
	data->temp_buff[0] = -500;
	data->batt_ibat_avg_buff[0] = SPRD_FGU_MAGIC_NUMBER;
	data->chg_term_loop_ibat[0] = SPRD_FGU_MAGIC_NUMBER;
	data->dischg_term_loop_vbat[0] = SPRD_FGU_MAGIC_NUMBER;

	if (data->support_bat_aging) {
		ret = sprd_fgu_parse_battery_cycles_fcc_table(data);
		if (ret) {
			dev_err(data->dev, "%s failed to parse battery_cycles_fcc_table!!!\n",
				__func__);
			data->support_bat_aging = false;
		}
	}

	sprd_fgu_parse_cmdline(data);

	ret = sprd_fgu_get_bat_para_table(data, SPRD_BAT_AGING_ID_DEFAULT);
	if (ret)
		return ret;

	if (data->support_bat_aging)
		sprd_fgu_bat_aging_algo(data, true);

	if (data->track.learned_mah == data->temp_default_fcc_mah) {
		dev_info(data->dev, "learned_mah = %d, temp_default_fcc_mah = %d, learned_mah = -1\n",
			 data->track.learned_mah, data->temp_default_fcc_mah);
		data->track.learned_mah = -1;
	}

	if (data->track.learned_mah != -1)
		sprd_fgu_adjust_fcc(data, data->track.learned_mah, SPRD_FGU_TEMP_DEFAULT);

	sprd_fgu_get_battery_temp_chg_loss_lut(data, data->fullbatt_uA, data->max_volt_uv);

	ret = fgu_info->ops->fgu_calibration(fgu_info);
	if (ret) {
		dev_err(data->dev, "failed to calibrate fgu, ret = %d\n", ret);
		return ret;
	}

	ret = sprd_fgu_hw_config(data);
	if (ret)
		goto disable_fgu;

	for (i = 0; i < SPRD_FGU_TEMP_BUFF_CNT; i++)
		ret |= sprd_fgu_get_temp(data, &data->bat_temp);
	if (ret) {
		dev_err(data->dev, "failed to get battery temperature\n");
		goto disable_fgu;
	}
	data->last_bat_temp = 1000;

	ret = fgu_info->ops->get_cc_uah(fgu_info, &data->track.boot_uah, true);
	if (ret)
		dev_info(data->dev, "failed to get boot clbcnt, ret = %d\n", ret);

	ret = fgu_info->ops->reset_cc_mah(fgu_info, data->total_mah, 0);
	if (ret) {
		dev_err(data->dev, "failed to reset cc mah!\n");
		goto disable_fgu;
	}

	/*
	 * Get the boot battery capacity when system powers on, which is used to
	 * initialize the coulomb counter. After that, we can read the coulomb
	 * counter to measure the battery capacity.
	 */
	ret = sprd_fgu_get_boot_capacity(data);
	if (ret) {
		dev_err(data->dev, "failed to get boot capacity\n");
		goto disable_fgu;
	}

	/*
	 * Convert battery capacity to the corresponding initial coulomb counter
	 * and set into coulomb counter registers.
	 */

	cur_time = ktime_to_timespec64(ktime_get_boottime());
	data->awake_times = data->stop_charge_times = cur_time.tv_sec;
	data->awake_cc_uah = 0;
	dev_info(data->dev, "suspend calib: current time_stamp = %lld\n",
		 data->awake_times);

	return 0;

disable_fgu:
	fgu_info->ops->enable_fgu_module(fgu_info, false);

	return ret;
}

static int sprd_fgu_info_register(struct sprd_fgu_data *data)
{
	int ret = 0;

#if IS_ENABLED(CONFIG_FUEL_GAUGE_SC27XX)
	data->fgu_info = sc27xx_fgu_info_register(data->dev);
	if (IS_ERR(data->fgu_info)) {
		dev_err(data->dev, "failed to get fgu_info!!!\n");
		return -EPROBE_DEFER;
	}
#elif IS_ENABLED(CONFIG_FUEL_GAUGE_UMP96XX)
	data->fgu_info = ump96xx_fgu_info_register(data->dev);
	if (IS_ERR(data->fgu_info)) {
		dev_err(data->dev, "failed to get fgu_info!!!\n");
		return -EPROBE_DEFER;
	}
#else
	dev_err(data->dev, "failed to get pmic macro define!!!\n");
	return -EINVAL;
#endif

	return ret;
}


static void sprd_fgu_detect_status(struct sprd_fgu_data *data)
{
	int state = 0;
	u32 type;

	if (data->use_typec_extcon) {
		state = extcon_get_state(data->edev, SPRD_FGU_EXTCON_SINK);
		if (state < 0) {
			dev_err(data->dev, "failed to get extcon sink state（%d）\n", state);
			return;
		}

		if (state == 0)
			return;

		data->is_sink = state;

		if (data->is_sink)
			data->online = true;

		schedule_work(&data->typec_extcon_work);
	} else {
		if (data->usb_phy->chg_state != USB_CHARGER_PRESENT)
			return;

		data->online = true;

		type = data->usb_phy->chg_type;

		switch (type) {
		case SDP_TYPE:
			data->chg_type = POWER_SUPPLY_USB_TYPE_SDP;
			break;

		case DCP_TYPE:
			data->chg_type = POWER_SUPPLY_USB_TYPE_DCP;
			break;

		case CDP_TYPE:
			data->chg_type = POWER_SUPPLY_USB_TYPE_CDP;
			break;

		default:
			data->chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		}
	}

	dev_info(data->dev, "%s:online = %d\n", __func__, data->online);
}

static int sprd_fgu_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np;
	struct power_supply_config fgu_cfg = { };
	struct sprd_fgu_data *data;
	int ret, irq;
	char result[32] = {0};
	int id = 0;

	if (!dev) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	np = pdev->dev.of_node;
	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->last_cc_uah = SPRD_FGU_MAGIC_NUMBER;
	data->chg_sts = POWER_SUPPLY_STATUS_DISCHARGING;
	data->last_chg_sts = POWER_SUPPLY_STATUS_DISCHARGING;
	data->pre_uusoc = -EINVAL;
	data->pre_delta_soc = SPRD_FGU_MAGIC_NUMBER;
	data->batt_rm_soc = -EINVAL;
	data->fake_temp = -9999;
	data->batt_cap_level_critical = 0;
	data->first_flag = true;

	sprd_fgu_set_chg_term_enable_mode(data, (CHG_TERM_ENABLE_MODE_IBAT |
						 CHG_TERM_ENABLE_MODE_SOC|
						 CHG_TERM_ENABLE_MODE_ABS_IBAT),
					  (CHG_TERM_ENABLE_MODE_IBAT |
					   CHG_TERM_ENABLE_MODE_SOC |
					   CHG_TERM_ENABLE_MODE_ABS_IBAT));

	data->dev = &pdev->dev;
	platform_set_drvdata(pdev, data);

	data->regmap = dev_get_regmap(dev->parent, NULL);
	if (!data->regmap) {
		dev_err(dev, "failed to get regmap\n");
		return -ENODEV;
	}

	data->use_typec_extcon = device_property_read_bool(dev, "use-typec-extcon");
	data->use_battery_temp = device_property_read_bool(dev, "use-battery-temp");

	data->track.full_chg_track_enable = device_property_read_bool(dev, "full-chg-track-enable");

	/* init usb notify */
	ret = sprd_fgu_register_usb_notify(data);
	if (ret)
		return ret;

	data->channel = devm_iio_channel_get(dev, "bat-temp");
	if (IS_ERR(data->channel)) {
		dev_err(dev, "failed to get IIO channel, ret = %ld\n", PTR_ERR(data->channel));
		return PTR_ERR(data->channel);
	}

	data->charge_chan = devm_iio_channel_get(dev, "charge-vol");
	if (IS_ERR(data->charge_chan)) {
		dev_err(dev, "failed to get charge IIO channel, ret = %ld\n",
			PTR_ERR(data->charge_chan));
		return PTR_ERR(data->charge_chan);
	}

	ret = sprd_fgu_init_cap_remap_table(data);
	if (ret)
		dev_err(dev, "%s init cap remap table fail\n", __func__);

	ret = sprd_fgu_info_register(data);
	if (ret)
		return ret;

	ret = device_property_read_u32(dev,
				       "sprd,comp-resistance-mohm",
				       &data->comp_resistance);
	if (ret)
		dev_warn(dev, "no fgu compensated resistance support\n");

	ret = device_property_read_u32(dev,
				       "sprd,com-pcb-resistance-mohm",
				       &data->com_pcb_resistance);
	if (ret) {
		data->com_pcb_resistance = SPRD_FGU_RBAT_CMP_MOH;
		dev_warn(dev, "no fgu pcb compensated resistance defined\n");
	}

	ret = device_property_read_u32(dev, "sprd,capacity-hc", &data->capacity_hc);
	if (ret)
		dev_warn(dev, "no fgu capacity_hc support\n");

	ret = device_property_read_u32(dev, "sprd,boot-cap-temp-thd", &data->boot_temp_thd);
	if (ret)
		dev_warn(dev, "no fgu boot_temp_thd support\n");
	if (ret == 0 && data->boot_temp_thd > 0)
		data->boot_temp_thd *= 10;
	if (data->boot_temp_thd > 0 && data->boot_temp_thd < SPRD_FGU_TEMP_BOOT_THD) {
		dev_info(data->dev, "%s:boot_temp_thd = %d, adjust\n",
			 __func__, data->boot_temp_thd);
		data->boot_temp_thd = SPRD_FGU_TEMP_BOOT_THD;
	}

	ret = device_property_read_u32(dev, "sprd,boot-cap-shutdown-time-thd",
				       &data->shutdown_time_thd);
	if (ret)
		dev_warn(dev, "no fgu shutdown_time_thd support\n");

	ret = sprd_fgu_get_boot_mode(data);
	if (ret)
		dev_warn(dev, "get_boot_mode can't not parse bootargs property\n");

	data->support_bat_aging =
		device_property_read_bool(&pdev->dev, "sprd,bat-aging");
	if (!data->support_bat_aging)
		dev_info(&pdev->dev, "Do not support battery aging function\n");

	data->gpiod = devm_gpiod_get(&pdev->dev, "bat-detect", GPIOD_IN);
	if (IS_ERR(data->gpiod)) {
		ret = PTR_ERR(data->gpiod);
		if (ret == -ENOENT) {
			dev_err(dev, "NO battery detection GPIO, ignore bat detect GPIO\n");
			data->gpiod = NULL;
		} else {
			dev_err(dev, "failed to get battery detection GPIO\n");
			return -ENXIO;
		}
	}

	if (data->gpiod) {
		ret = gpiod_get_value_cansleep(data->gpiod);
		if (ret < 0) {
			dev_err(dev, "failed to get gpio state\n");
			return ret;
		}

		data->bat_present = !!ret;
	}
	mutex_init(&data->lock);
	mutex_init(&data->discharge_lock);
	init_completion(&data->probe_init);
	sprd_fgu_check_third_fuel_present(data);

	fgu_cfg.drv_data = data;
	fgu_cfg.of_node = np;
	data->battery = devm_power_supply_register(dev, &sprd_fgu_desc, &fgu_cfg);
	if (IS_ERR(data->battery)) {
		dev_err(dev, "failed to register power supply");
		ret = -ENXIO;
		goto err;
	}

	ret = devm_add_action_or_reset(dev, sprd_fgu_disable, data);
	if (ret) {
		dev_err(dev, "failed to add fgu disable action\n");
		goto err;
	}

	ret = sprd_fgu_hw_init(data);
	if (ret) {
		dev_err(dev, "failed to initialize fgu hardware\n");
		goto err;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "no irq resource specified\n");
		ret = irq;
		goto err;
	}

	ret = devm_request_threaded_irq(dev, irq, NULL,	sprd_fgu_interrupt,
					IRQF_NO_SUSPEND | IRQF_ONESHOT,
					pdev->name, data);
	if (ret) {
		dev_err(dev, "failed to request fgu IRQ\n");
		goto err;
	}

	if (data->gpiod) {
		irq = gpiod_to_irq(data->gpiod);
		if (irq < 0) {
			dev_err(dev, "failed to translate GPIO to IRQ\n");
			ret = irq;
			goto err;
		}

		ret = devm_request_threaded_irq(dev, irq, NULL,
						sprd_fgu_bat_detection,
						IRQF_ONESHOT | IRQF_TRIGGER_RISING |
						IRQF_TRIGGER_FALLING,
						pdev->name, data);
		if (ret) {
			dev_err(dev, "failed to request IRQ\n");
			goto err;
		}
	}

	device_init_wakeup(dev, true);
	if (!cali_or_auto_mode)
		pm_wakeup_event(data->dev, SPRD_FGU_TRACK_WAKE_UP_MS);

	data->track.state = CAP_TRACK_INIT;
	INIT_DELAYED_WORK(&data->cap_track_work, sprd_fgu_cap_track_work);
	INIT_DELAYED_WORK(&data->fgu_work, sprd_fgu_work);
	INIT_DELAYED_WORK(&data->update_delay_work, update_cycle_work);
	schedule_delayed_work(&data->cap_track_work, 0);
	schedule_delayed_work(&data->fgu_work,
			      msecs_to_jiffies(SPRD_FGU_NORMAL_WORK_10S * 1000));

	sprd_fgu_dump_info(data);

	ret = sprd_fgu_register_sysfs(data);
	if (ret)
		dev_err(&pdev->dev, "register sysfs fail, ret = %d\n", ret);

	data->fgu_info->real_time_calib = true;
	data->fgu_info->capacity_remap_enable = true;
	data->fgu_info->capacity_hc_enable = true;

	/*
	 * Fuel gauge unit initialization requires an initial
	 * battery soc value.
	 */
	ret = sprd_battery_parse_cmdline_match_by_split("bat.id=", " ", result, sizeof(result));
	if (!ret && !(ret = kstrtoint(result, 10, &id)) && (id == -1))
		data->erp_config = 1;

	ret = sprd_fgu_get_capacity(data, &data->bat_soc);
	if (ret)
		dev_err(data->dev, "%s failed get capacity!\n", __func__);

	data->probe_initialized = true;
	complete_all(&data->probe_init);
	sprd_fgu_detect_status(data);
	dev_info(data->dev, "use_typec_extcon = %d\n", data->use_typec_extcon);
	dev_info(data->dev, "%s:line%d probe successfully\n", __func__, __LINE__);

	return 0;

err:
	sprd_fgu_disable(data);
	mutex_destroy(&data->lock);
	mutex_destroy(&data->discharge_lock);
	return ret;
}

static void sprd_fgu_clear_sr_time_array(struct sprd_fgu_data *data)
{
	memset(&data->sr_time_sleep, 0, sizeof(data->sr_time_sleep));
	memset(&data->sr_time_awake, 0, sizeof(data->sr_time_awake));
	memset(&data->sr_avg_cur_ma_sleep, 0, sizeof(data->sr_avg_cur_ma_sleep));
	memset(&data->sr_avg_cur_ma_awake, 0, sizeof(data->sr_avg_cur_ma_awake));
	data->sr_index_sleep = 0;
	data->sr_index_awake = 0;
	data->sr_index_number = 0;
}

static int sprd_fgu_sr_get_duty_ratio(struct sprd_fgu_data *data, int *avg_cur_ma)
{
	int total_sleep_time = 0, total_awake_time = 0, cnt = 0, duty_ratio = 0, count = 0;
	int easily_duty_ratio = 0, normal_duty_ratio = 0, coarse_duty_ratio = 0, total_time = 0;
	int easily_cur_ma = 0, normal_cur_ma = 0, coarse_cur_ma = 0;
	int sleep_time = 0, awake_time = 0, sleep_cur = 0, awake_cur = 0;
	int cur_ma = 0, total_cur_ma = 0;

	int last_sleep_idx = (data->sr_index_sleep - 1 < 0) ?
		SPRD_FGU_SR_ARRAY_LEN - 1 : data->sr_index_sleep - 1;
	int last_awake_idx = (data->sr_index_awake - 1 < 0) ?
		SPRD_FGU_SR_ARRAY_LEN - 1 : data->sr_index_awake - 1;

	if (data->sr_index_number >= SPRD_FGU_SR_ARRAY_LEN)
		count = SPRD_FGU_SR_ARRAY_LEN;
	else
		count = data->sr_index_number;

	do {
		sleep_time = data->sr_time_sleep[last_sleep_idx];
		awake_time = data->sr_time_awake[last_awake_idx];
		total_sleep_time += sleep_time;
		total_awake_time += awake_time;
		sleep_cur = data->sr_avg_cur_ma_sleep[last_sleep_idx];
		awake_cur = data->sr_avg_cur_ma_awake[last_awake_idx];

		last_sleep_idx--;
		last_awake_idx--;

		last_sleep_idx = (last_sleep_idx < 0) ?
			SPRD_FGU_SR_ARRAY_LEN - 1 : last_sleep_idx;
		last_awake_idx = (last_awake_idx < 0) ?
			SPRD_FGU_SR_ARRAY_LEN - 1 : last_awake_idx;

		cnt++;
		total_time = total_sleep_time + total_awake_time;
		total_cur_ma += (sleep_time * sleep_cur + awake_time * awake_cur);

		if (total_time >= SPRD_FGU_SR_TOTAL_TIME_S) {
			duty_ratio = total_sleep_time * 100 / total_time;
			cur_ma = total_cur_ma / total_time;
		}

		if (!coarse_duty_ratio && duty_ratio >= SPRD_FGU_SR_COARSE_DUTY_RATIO) {
			coarse_duty_ratio = duty_ratio;
			coarse_cur_ma = cur_ma;
		}
		if (!easily_duty_ratio && duty_ratio >= SPRD_FGU_SR_EASILY_DUTY_RATIO) {
			easily_duty_ratio = duty_ratio;
			easily_cur_ma = cur_ma;
		}

		if (duty_ratio >= SPRD_FGU_SR_DUTY_RATIO) {
			normal_duty_ratio = duty_ratio;
			normal_cur_ma = cur_ma;
			break;
		}

	} while (cnt <= count);

	if (normal_duty_ratio) {
		duty_ratio = normal_duty_ratio;
		*avg_cur_ma = normal_cur_ma;
	} else if (easily_duty_ratio) {
		duty_ratio = easily_duty_ratio;
		*avg_cur_ma = easily_cur_ma;
	} else if (coarse_duty_ratio) {
		duty_ratio = coarse_duty_ratio;
		*avg_cur_ma = coarse_cur_ma;
	}

	dev_info(data->dev, "%s suspend calib: sr_avg_cur_ma = %d, total_awake_time = %d, total_sleep_time = %d, total_time = %d, duty_ratio = %d, easily_duty_ratio = %d, normal_duty_ratio = %d, cnt = %d, count = %d!!!\n",
		 __func__, *avg_cur_ma, total_awake_time, total_sleep_time,
		 total_time, duty_ratio, easily_duty_ratio, normal_duty_ratio, cnt, count);

	return duty_ratio;
}

static void sprd_fgu_sr_calib_resume_check(struct sprd_fgu_data *data, int sleep_time)
{
	struct sprd_fgu_info *fgu_info = data->fgu_info;
	int ret = 0, last_awake_time = 0, last_sleep_time = 0, duty_ratio = 0;
	int vol_uv = 0, cur_ua = 0;
	int sleep_delta_cc_uah, avg_cur_ma;
	bool single_sleep_flag = false;
	static int sr_last_calib_mode = SR_CALIB_MAX;
	static s64 last_calib_mode_time;

	ret = fgu_info->ops->get_cc_uah(fgu_info, &data->awake_cc_uah, false);
	if (ret) {
		dev_err(data->dev, "%s suspend calib: failed get awake_cc_uah!!\n", __func__);
		return;
	}

	sleep_delta_cc_uah = data->awake_cc_uah - data->sleep_cc_uah;
	avg_cur_ma = sprd_fgu_uah2current(sleep_delta_cc_uah, sleep_time);
	if (abs(avg_cur_ma) > SPRD_FGU_SR_EASILY_SLEEP_AVG_CUR_MA) {
		sprd_fgu_clear_sr_time_array(data);
		dev_info(data->dev, "suspend calib: sleep_cur_ma = %dmA > %dmA need to clear_sr_time_array!!!\n",
			 abs(avg_cur_ma), SPRD_FGU_SR_EASILY_SLEEP_AVG_CUR_MA);
		return;
	}

	if (sleep_time >= 0) {
		data->sr_time_sleep[data->sr_index_sleep] = sleep_time;
		data->sr_avg_cur_ma_sleep[data->sr_index_sleep] = avg_cur_ma;
		data->sr_index_sleep++;
		data->sr_index_sleep = data->sr_index_sleep % SPRD_FGU_SR_ARRAY_LEN;
	} else {
		dev_err(data->dev, "%s suspend calib: sleep_time = %d, is not meet!!!\n",
			__func__, sleep_time);
	}

	if (sleep_time >= SPRD_FGU_SR_SLEEP_MIN_TIME_S)
		single_sleep_flag = true;

	if ((data->awake_times - data->stop_charge_times) <= SPRD_FGU_SR_STOP_CHARGE_TIMES) {
		dev_info(data->dev, "suspend calib: cur_times = %lld, stop_charge_times = %lld, is not meet!!!\n",
			 data->awake_times, data->stop_charge_times);
		return;
	}

	if (data->bat_temp <= SPRD_FGU_CAP_CALIB_TEMP_MIN)
		return;

	/* get last awake time */
	if (data->sr_index_awake >= 0 && data->sr_index_awake < SPRD_FGU_SR_ARRAY_LEN) {
		last_awake_time = (data->sr_index_awake - 1 < 0) ?
			data->sr_time_awake[SPRD_FGU_SR_ARRAY_LEN - 1] :
			data->sr_time_awake[data->sr_index_awake - 1];
	}

	/* get last sleep time */
	if (data->sr_index_sleep >= 0 && data->sr_index_sleep < SPRD_FGU_SR_ARRAY_LEN) {
		last_sleep_time = (data->sr_index_sleep - 1 < 0) ?
			data->sr_time_sleep[SPRD_FGU_SR_ARRAY_LEN - 1] :
			data->sr_time_sleep[data->sr_index_sleep - 1];
	}

	if (!single_sleep_flag && (last_sleep_time < SPRD_FGU_SR_COARSE_LAST_SLEEP_TIME_S ||
	    last_awake_time > SPRD_FGU_SR_EASILY_LAST_AWAKE_TIME_S)) {
		dev_info(data->dev, "suspend calib: single_sleep_flag = %d, last_sleep_time = %d, last_awake_time = %d, data->sr_index_number = %d, is not meet!!!\n",
			 single_sleep_flag, last_sleep_time,
			 last_awake_time, data->sr_index_number);
		return;
	}

	duty_ratio = sprd_fgu_sr_get_duty_ratio(data, &data->sr_avg_cur_ma);

	if (single_sleep_flag && abs(avg_cur_ma) <= SPRD_FGU_SR_SLEEP_AVG_CUR_MA) {
		data->sr_calib_mode = SR_CALIB_PRECISION;
		data->sr_avg_cur_ma = avg_cur_ma;
		dev_info(data->dev, "suspend calib: sr_calib_resume is meet single calib mode!!!\n");
	} else if (last_sleep_time >= SPRD_FGU_SR_LAST_SLEEP_TIME_S &&
		   last_awake_time <= SPRD_FGU_SR_LAST_AWAKE_TIME_S &&
		   abs(data->awake_avg_cur_ma) <= SPRD_FGU_SR_AWAKE_AVG_CUR_MA &&
		   duty_ratio >= SPRD_FGU_SR_DUTY_RATIO &&
		   abs(data->sr_avg_cur_ma) <= SPRD_FGU_SR_SLEEP_AWAKE_AVG_CUR_MA &&
		   abs(avg_cur_ma) <= SPRD_FGU_SR_SLEEP_AVG_CUR_MA) {
		data->sr_calib_mode = SR_CALIB_PRECISION;
		dev_info(data->dev, "suspend calib: sr_calib_resume is meet precision calib mode!!!\n");
	} else if (last_sleep_time >= SPRD_FGU_SR_EASILY_LAST_SLEEP_TIME_S &&
		   last_awake_time <= SPRD_FGU_SR_EASILY_LAST_AWAKE_TIME_S &&
		   abs(data->awake_avg_cur_ma) <= SPRD_FGU_SR_AWAKE_AVG_CUR_MA &&
		   duty_ratio >= SPRD_FGU_SR_EASILY_DUTY_RATIO &&
		   abs(data->sr_avg_cur_ma) <= SPRD_FGU_SR_EASILY_SLEEP_AVG_CUR_MA &&
		   abs(avg_cur_ma) <= SPRD_FGU_SR_EASILY_SLEEP_AVG_CUR_MA) {
		data->sr_calib_mode = SR_CALIB_MODERATE;
		dev_info(data->dev, "suspend calib: sr_calib_resume is meet moderate calib mode!!!\n");
	} else if (last_sleep_time >= SPRD_FGU_SR_COARSE_LAST_SLEEP_TIME_S &&
		   duty_ratio >= SPRD_FGU_SR_COARSE_DUTY_RATIO &&
		   abs(data->sr_avg_cur_ma) <= SPRD_FGU_SR_COARSE_SLEEP_AVG_CUR_MA &&
		   abs(avg_cur_ma) <= SPRD_FGU_SR_EASILY_SLEEP_AVG_CUR_MA) {
		data->sr_calib_mode = SR_CALIB_COARSE;
		dev_info(data->dev, "suspend calib: sr_calib_resume is meet coarse calib mode!!!\n");
	} else {
		dev_info(data->dev, "suspend calib is not meet update ocv!!!: sleep_time = %d, current cc_uah = %d, sleep_cc_uah = %d, sleep_delta_cc_uah = %d, sleep_avg_cur_ma = %d\n",
			 sleep_time, data->sleep_cc_uah, data->awake_cc_uah,
			 sleep_delta_cc_uah, avg_cur_ma);
		return;
	}

	dev_info(data->dev, "suspend calib: sleep_time = %d, current cc_uah = %d, sleep_cc_uah = %d, sleep_delta_cc_uah = %d, sleep_avg_cur_ma = %d\n",
		 sleep_time, data->sleep_cc_uah, data->awake_cc_uah,
		 sleep_delta_cc_uah, avg_cur_ma);

	ret = sprd_fgu_get_lp_ocv_from_fifo(data, &vol_uv, &cur_ua, SPRD_FGU_SR_SLEEP_AVG_CUR_MA);
	if (ret) {
		dev_err(data->dev, "suspend calib: failed get sr_ocv_uv!!\n");
		return;
	}

	data->sr_ocv_uv = vol_uv;
	data->sr_ocv_cur_ua = cur_ua;
	data->sr_resume_need_calib = true;
	if (data->sr_calib_mode >= sr_last_calib_mode &&
	    data->awake_times - last_calib_mode_time < SPRD_FGU_OCV_INTERVAL_TIME) {
		dev_info(data->dev, "suspend calib: same calib_mode or no need to update\n");
		return;
	}
	sr_last_calib_mode = data->sr_calib_mode;
	last_calib_mode_time = data->awake_times;
	pm_wakeup_event(data->dev, SPRD_FGU_SR_CALIB_WAKE_UP_MS);
}

#if IS_ENABLED(CONFIG_PM_SLEEP)
static int sprd_fgu_resume(struct device *dev)
{
	struct sprd_fgu_data *data = dev_get_drvdata(dev);
	struct sprd_fgu_info *fgu_info;
	struct timespec64 cur_time;
	s64 cur_times;
	int ret = 0, sleep_time = 0;

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}
	fgu_info = data->fgu_info;

	cur_time = ktime_to_timespec64(ktime_get_boottime());
	cur_times = data->awake_times = cur_time.tv_sec;
	sleep_time = cur_times - data->sleep_times;

	sprd_fgu_sr_calib_resume_check(data, sleep_time);

	sprd_fgu_suspend_calib_check(data);

	ret = fgu_info->ops->enable_fgu_int(fgu_info, SPRD_FGU_CLBCNT_DELTA_INT_CMD, false);
	if (ret) {
		dev_err(data->dev, "failed to disable clbcnt delta interrupt\n");
		return ret;
	}

	data->sr_resume_temp_update = true;
	data->param.update_now = true;
	schedule_delayed_work(&data->fgu_work, 0);
	schedule_delayed_work(&data->cap_track_work, 0);

	return 0;
}

static void sprd_fgu_sr_calib_suspend_check(struct sprd_fgu_data *data)
{
	struct sprd_fgu_info *fgu_info = data->fgu_info;
	struct timespec64 cur_time;
	s64 cur_times, awake_time = 0;
	int awake_delta_cc_uah, ret = 0;

	cur_time = ktime_to_timespec64(ktime_get_boottime());
	cur_times = data->sleep_times = cur_time.tv_sec;
	awake_time = cur_times - data->awake_times;
	ret = fgu_info->ops->get_cc_uah(fgu_info, &data->sleep_cc_uah, false);
	if (ret) {
		dev_err(data->dev, "%s suspend calib: failed get sleep_cc_uah!!\n", __func__);
		return;
	}

	if (awake_time > 0) {
		awake_delta_cc_uah = data->sleep_cc_uah - data->awake_cc_uah;
		data->awake_avg_cur_ma = sprd_fgu_uah2current(awake_delta_cc_uah, awake_time);
		dev_info(data->dev, "%s suspend calib: current time_stamp = %lld, awake_time = %lld, cureent cc_uah = %d, awake_delta_cc_uah = %d, awake_avg_cur_ma = %d\n",
			 __func__, cur_times, awake_time, data->sleep_cc_uah,
			 awake_delta_cc_uah, data->awake_avg_cur_ma);
	} else if (awake_time == 0) {
		awake_time = 1;
		if (fgu_info->ops->get_current_now(fgu_info, &data->awake_avg_cur_ma))
			data->awake_avg_cur_ma = -200;
		dev_info(data->dev, "suspend calib: awake_time = 0!!!\n");
	}

	if (awake_time > SPRD_FGU_SR_AWAKE_MAX_TIME_S ||
	    (abs(data->awake_avg_cur_ma) > SPRD_FGU_SR_AWAKE_AVG_CUR_MA &&
	     awake_time > SPRD_FGU_SR_AWAKE_BIG_CUR_MAX_TIME_S)){
		sprd_fgu_clear_sr_time_array(data);
		dev_info(data->dev, "%s suspend calib: awake_time = %llds > %ds, or awake_avg_cur_ma = %dmA > %dmA and awake_time = %llds > %ds, need to clear_sr_time_array!\n",
			 __func__, awake_time, SPRD_FGU_SR_AWAKE_MAX_TIME_S,
			 abs(data->awake_avg_cur_ma), SPRD_FGU_SR_AWAKE_AVG_CUR_MA,
			 awake_time, SPRD_FGU_SR_AWAKE_BIG_CUR_MAX_TIME_S);
	} else if (awake_time > 0) {
		data->sr_time_awake[data->sr_index_awake] = awake_time;
		data->sr_avg_cur_ma_awake[data->sr_index_awake] = data->awake_avg_cur_ma;
		data->sr_index_awake++;
		data->sr_index_number++;
		if (data->sr_index_number >= SPRD_FGU_SR_ARRAY_LEN)
			data->sr_index_number = SPRD_FGU_SR_ARRAY_LEN;
		data->sr_index_awake = data->sr_index_awake % SPRD_FGU_SR_ARRAY_LEN;
	} else {
		dev_err(data->dev, "%s suspend calib: awake_time = %lld, not meet!!!\n",
			__func__, awake_time);
	}
}


static int sprd_fgu_suspend(struct device *dev)
{
	struct sprd_fgu_data *data = dev_get_drvdata(dev);
	int ret, ocv_uv;
	struct sprd_fgu_info *fgu_info;

	if (!data) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}
	fgu_info = data->fgu_info;

	sprd_fgu_sr_calib_suspend_check(data);

	/*
	 * If we are charging, then no need to enable the FGU interrupts to
	 * adjust the battery capacity.
	 */
	if (data->chg_sts == POWER_SUPPLY_STATUS_CHARGING ||
	    data->chg_sts == POWER_SUPPLY_STATUS_FULL)
		return 0;

	ret = sprd_fgu_get_vbat_ocv(data, &ocv_uv);
	if (ret)
		goto disable_int;

	ocv_uv *= 1000;

	/*
	 * If current OCV is less than the minimum voltage, we should enable the
	 * coulomb counter threshold interrupt to notify events to adjust the
	 * battery capacity.
	 */
	if (ocv_uv < data->min_volt_uv) {
		ret = fgu_info->ops->enable_fgu_int(fgu_info, SPRD_FGU_CLBCNT_DELTA_INT_CMD, true);
		if (ret) {
			dev_err(data->dev, "failed to enable coulomb threshold int\n");
			goto disable_int;
		}
	}

	cancel_delayed_work_sync(&data->cap_track_work);
	cancel_delayed_work_sync(&data->fgu_work);

	data->sr_resume_temp_update = false;
	data->sr_resume_need_calib = false;
	sprd_fgu_suspend_calib_config(data);

	return 0;

disable_int:
	fgu_info->ops->enable_fgu_int(fgu_info, SPRD_FGU_VOLT_LOW_INT_CMD, false);
	return ret;
}
#endif

static void sprd_fgu_shutdown(struct platform_device *pdev)
{
	struct sprd_fgu_data *data = platform_get_drvdata(pdev);
	int ret, batt_cc_uah;

	if (!data)
		return;

	cancel_delayed_work_sync(&data->cap_track_work);
	cancel_delayed_work_sync(&data->fgu_work);

	ret = sprd_fgu_get_ccuah(data, &batt_cc_uah);
	if (ret < 0)
		dev_err(data->dev, "%s:Failed to get cc uah, ret = %d\n", __func__, ret);

	sprd_fgu_save_last_cc_uah(data, batt_cc_uah);
	sprd_fgu_save_last_shutdown_batt_temp(data, data->bat_temp);

	usleep_range(2000, 2100);
}

static const struct dev_pm_ops sprd_fgu_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sprd_fgu_suspend, sprd_fgu_resume)
};

static const struct of_device_id sprd_fgu_of_match[] = {
	{ .compatible = "sprd,ump965x-fgu-v2", },
	{ .compatible = "sprd,ump9360-fgu-v2", },
	{ .compatible = "sprd,sc27xx-fgu-v2", },
	{ .compatible = "sprd,ump9620-fgu-v2", },
	{ }
};
MODULE_DEVICE_TABLE(of, sprd_fgu_of_match);

static struct platform_driver sprd_fgu_driver = {
	.shutdown = sprd_fgu_shutdown,
	.probe = sprd_fgu_probe,
	.driver = {
		.name = "sprd-fgu-v2",
		.of_match_table = sprd_fgu_of_match,
		.pm = &sprd_fgu_pm_ops,
	}
};

module_platform_driver(sprd_fgu_driver);

MODULE_DESCRIPTION("Spreadtrum PMICs Fual Gauge Unit Driver");
MODULE_LICENSE("GPL");
