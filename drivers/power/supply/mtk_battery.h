/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author Wy Chuang<wy.chuang@mediatek.com>
 */

#ifndef __MTK_BATTERY_INTF_H__
#define __MTK_BATTERY_INTF_H__

#include <linux/alarmtimer.h>
#include <linux/hrtimer.h>
#include <linux/nvmem-consumer.h>
#include <linux/power_supply.h>
#include <linux/sysfs.h>
#include <linux/wait.h>
#include "mtk_gauge.h"

#define UNIT_TRANS_10	10
#define UNIT_TRANS_100	100
#define UNIT_TRANS_1000	1000
#define UNIT_TRANS_60	60
#define MAX_TABLE		10

#define BAT_SYSFS_FIELD_RW(_name, _prop)	\
{									 \
	.attr	= __ATTR(_name, 0644, bat_sysfs_show, bat_sysfs_store),\
	.prop	= _prop,	\
	.set	= _name##_set,						\
	.get	= _name##_get,						\
}

#define BAT_SYSFS_FIELD_RO(_name, _prop)	\
{			\
	.attr   = __ATTR(_name, 0444, bat_sysfs_show, bat_sysfs_store),\
	.prop   = _prop,				  \
	.get	= _name##_get,						\
}

#define BAT_SYSFS_FIELD_WO(_name, _prop)	\
{								   \
	.attr	= __ATTR(_name, 0200, bat_sysfs_show, bat_sysfs_store),\
	.prop	= _prop,	\
	.set	= _name##_set,						\
}

enum battery_property {
	BAT_PROP_TEST,
	BAT_PROP_TEMPERATURE,
	BAT_PROP_COULOMB_INT_GAP,
	BAT_PROP_UISOC_HT_INT_GAP,
	BAT_PROP_UISOC_LT_INT_GAP,
	BAT_PROP_ENABLE_UISOC_HT_INT,
	BAT_PROP_ENABLE_UISOC_LT_INT,
	BAT_PROP_UISOC,
	BAT_PROP_DISABLE,
	BAT_PROP_C_BATTERY_INT_GAP,
	BAT_PROP_INIT_DONE,
	BAT_PROP_FG_RESET,
};

struct battery_data {
	struct power_supply_desc psd;
	struct power_supply_config psy_cfg;
	struct power_supply *psy;
	int bat_status;
	int bat_health;
	int bat_present;
	int bat_technology;
	int bat_capacity;
	/* Add for Battery Service */
	int bat_batt_vol;
	int bat_batt_temp;
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
	int r_i_sense;	/*is it used?*/
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
	int dlpt_ui_remap_en;

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

};

struct fuelgauge_temperature {
	signed int BatteryTemp;
	signed int TemperatureR;
};

struct fuelgauge_profile_struct {
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
	struct fuelgauge_profile_struct fg_profile[100];
};


struct fuel_gauge_table_custom_data {
	/* cust_battery_meter_table.h */
	int active_table_number;
	struct fuel_gauge_table fg_profile[MAX_TABLE];

	int temperature_tb0;
	int fg_profile_temperature_0_size;
	struct fuelgauge_profile_struct fg_profile_temperature_0[100];

	int temperature_tb1;
	int fg_profile_temperature_1_size;
	struct fuelgauge_profile_struct fg_profile_temperature_1[100];
};

struct fgd_cmd_param_t_custom {
	struct fuel_gauge_custom_data fg_cust_data;
	struct fuel_gauge_table_custom_data fg_table_cust_data;
};

/* coulomb service */
struct gauge_consumer {
	char *name;
	struct device *dev;
	long start;
	long end;
	int variable;

	int (*callback)(struct gauge_consumer *consumer);
	struct list_head list;
};

struct mtk_coulomb_service {
	struct list_head coulomb_head_plus;
	struct list_head coulomb_head_minus;
	struct mutex coulomb_lock;
	struct mutex hw_coulomb_lock;
	unsigned long reset_coulomb;
	spinlock_t slock;
	struct wakeup_source wlock;
	wait_queue_head_t wait_que;
	bool coulomb_thread_timeout;
	int fgclog_level;
	int pre_coulomb;
	bool init;
};

struct battery_temperature_table {
	int type;
	unsigned int rbat_pull_up_r;
	unsigned int rbat_pull_up_volt;
	unsigned int bif_ntc_r;
};

enum Fg_interrupt_flags {
	FG_INTR_0 = 0,
	FG_INTR_TIMER_UPDATE  = 1,
	FG_INTR_BAT_CYCLE = 2,
	FG_INTR_CHARGER_OUT = 4,
	FG_INTR_CHARGER_IN = 8,
	FG_INTR_FG_TIME = 16,
	FG_INTR_BAT_INT1_HT = 32,
	FG_INTR_BAT_INT1_LT = 64,
	FG_INTR_BAT_INT2_HT = 128,
	FG_INTR_BAT_INT2_LT = 256,
	FG_INTR_BAT_TMP_HT = 512,
	FG_INTR_BAT_TMP_LT = 1024,
	FG_INTR_BAT_TIME_INT = 2048,
	FG_INTR_NAG_C_DLTV = 4096,
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

struct mtk_battery_algo {
	int last_temp;
	int T_table;
	int T_table_c;

	/*soc only follows c_soc */
	int soc;

	/* tempeture related*/
	int fg_bat_tmp_c_gap;

	/* CSOC related */
	int fg_c_d0_ocv;
	int fg_c_d0_dod;
	int fg_c_d0_soc;
	int fg_c_dod;
	int fg_c_soc;
	int fg_bat_int1_gap;
	int prev_car_bat0;

	/* UI related */
	int rtc_ui_soc;
	int ui_soc;
	int ui_d0_soc;
	int vboot;
	int vboot_c;
	int qmax_t_0ma; /* 0.1mA */
	int qmax_t_0ma_tb1; /* 0.1mA */
	int qmax_t_0ma_h;
	int qmax_t_Nma_h;
	int quse_tb0;
	int quse_tb1;
	int car;
	int batterypseudo1_h;
	int batterypseudo100;
	int shutdown_hl_zcv;
	int qmax_t_0ma_h_tb1;
	int qmax_t_Nma_h_tb1;
	int qmax_t_aging;
	int aging_factor;
	int fg_resistance_bat;
	int DC_ratio;
	int ht_gap;
	int lt_gap;
	int low_tracking_enable;
	int fg_vbat2_lt;
	int fg_vbat2_ht;

	/* Interrupt control */
	int uisoc_ht_en;
	int uisoc_lt_en;
};

struct mtk_battery {
	/*linux driver related*/
	struct device *dev;
	wait_queue_head_t  wait_que;
	unsigned int fg_update_flag;
	struct hrtimer fg_hrtimer;
	struct mutex ops_lock;

	struct battery_data bs_data;
	struct mtk_coulomb_service cs;
	struct mtk_gauge *gauge;
	struct mtk_battery_algo algo;

	/* adb */
	int fixed_bat_tmp;
	int fixed_uisoc;

	/*battery flag*/
	bool init_flag;
	bool is_probe_done;
	bool disableGM30;

	/*battery status*/
	int soc;
	int ui_soc;
	struct timespec uisoc_oldtime;
	int d_saved_car;

	/*battery interrupt*/
	/* coulomb interrupt */
	int coulomb_int_gap;
	int coulomb_int_ht;
	int coulomb_int_lt;
	struct gauge_consumer coulomb_plus;
	struct gauge_consumer coulomb_minus;

	/* uisoc interrupt */
	int uisoc_int_ht_gap;
	int uisoc_int_lt_gap;
	int uisoc_int_ht_en;
	int uisoc_int_lt_en;
	struct gauge_consumer uisoc_plus;
	struct gauge_consumer uisoc_minus;

	/* gauge timer */
	struct alarm tracking_timer;
	struct work_struct tracking_timer_work;
	struct alarm one_percent_timer;
	struct work_struct one_percent_timer_work;

	/*custom related*/
	int battery_id;
	struct fuel_gauge_custom_data fg_cust_data;
	struct fuel_gauge_table_custom_data fg_table_cust_data;
	struct fgd_cmd_param_t_custom fg_data;
	/* hwocv swocv */
	int ext_hwocv_swocv;
	int ext_hwocv_swocv_lt;
	int ext_hwocv_swocv_lt_temp;
	/* battery temperature table */
	int no_bat_temp_compensate;
	int enable_tmp_intr_suspend;
	struct battery_temperature_table rbat;
	struct fuelgauge_temperature *tmp_table;

};

struct mtk_battery_sysfs_field_info {
	struct device_attribute attr;
	enum battery_property prop;
	int (*set)(struct mtk_battery *gm,
		struct mtk_battery_sysfs_field_info *attr, int val);
	int (*get)(struct mtk_battery *gm,
		struct mtk_battery_sysfs_field_info *attr, int *val);
};

/* coulomb service */
extern void gauge_coulomb_service_init(struct platform_device *pdev);
extern void gauge_coulomb_consumer_init(struct gauge_consumer *coulomb,
	struct device *dev, char *name);
extern void gauge_coulomb_start(struct gauge_consumer *coulomb, int car);
extern void gauge_coulomb_stop(struct gauge_consumer *coulomb);
extern void gauge_coulomb_dump_list(struct mtk_battery *gm);
extern void gauge_coulomb_before_reset(struct mtk_battery *gm);
extern void gauge_coulomb_after_reset(struct mtk_battery *gm);
/* coulomb sub system end */

/*mtk_battery.c */
extern int force_get_tbat(struct mtk_battery *gm, bool update);
extern int force_get_tbat_internal(struct mtk_battery *gm, bool update);
extern int wakeup_fg_algo_cmd(struct mtk_battery *gm,
	unsigned int flow_state, int cmd, int para1);
extern int wakeup_fg_algo(struct mtk_battery *gm, unsigned int flow_state);

extern int gauge_get_int_property(enum gauge_property gp);
extern int gauge_get_property(enum gauge_property gp,
			    int *val);
extern int gauge_set_property(enum gauge_property gp,
			    int val);
/*mtk_battery.c end */

/*mtk_battery_core.c */
extern void mtk_battery_core_init(struct platform_device *dev);
extern struct mtk_battery *get_mtk_battery(void);
extern int battery_get_property(enum battery_property bp, int *val);
extern int battery_get_int_property(enum battery_property bp);
extern int battery_set_property(enum battery_property bp, int val);
/*mtk_battery_core.c end */

/* mtk_battery_algo.c */
extern void battery_algo_init(struct mtk_battery *gm);
extern void do_fg_algo(struct mtk_battery *gm, unsigned int intr_num);
/* mtk_battery_algo.c end */
extern void gm_dump(struct mtk_gauge *gauge);

#endif /* __MTK_BATTERY_INTF_H__ */
