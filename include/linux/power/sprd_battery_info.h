/*
 * Copyright (C) 2011 Unisoc Co., Ltd.
 * Jinfeng.lin <Jinfeng.Lin1@unisoc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _SPRD_BATTERY_INFO_H
#define _SPRD_BATTERY_INFO_H

#include <linux/device.h>

#define SPRD_BATTERY_INFO_RESISTENCE_TEMP_MAX	20
#define SPRD_BATTERY_OCV_TEMP_MAX		20
#define SPRD_BATTERY_CYCLES_FCC_COLS_MAX	8
#define SPRD_BATTERY_COMP_TEMP_MAX		10
#define SPRD_BATTERY_BCL_VBAT_THRES_MAX		10

#define SPRD_CYCLES_AGINGFACTOR_COLS_MAX	8
#define SPRD_TEMP_FCC_COLS_MAX			8
#define SPRD_TEMP_ZP_VOL_COLS_MAX		8
#define SPRD_TEMP_BAT_VOL_COLS_MAX		8
#define SPRD_CAP_TEMP_ROWS_MAX			150
#define SPRD_CAP_TEMP_COLS_MAX			8
#define SPRD_OCV_TEMP_ROWS_MAX			150
#define SPRD_OCV_TEMP_COLS_MAX			8
#define SPRD_BAT_AGING_ID_DEFAULT		1000
#define SPRD_BAT_AGING_ID_NZ2Z			1100

#define POWER_SUPPLY_PROP_SOH			128
#define SPRD_BATTERY_STEP_CHG_TABLE_NAME	"step-chg-jeita-table"

enum sprd_battery_jeita_types {
	SPRD_BATTERY_JEITA_DCP = 0,
	SPRD_BATTERY_JEITA_SDP,
	SPRD_BATTERY_JEITA_CDP,
	SPRD_BATTERY_JEITA_UNKNOWN,
	SPRD_BATTERY_JEITA_FCHG,
	SPRD_BATTERY_JEITA_FLASH,
	SPRD_BATTERY_JEITA_WL_BPP,
	SPRD_BATTERY_JEITA_WL_EPP,
	SPRD_BATTERY_JEITA_MAX,
};

static const char * const sprd_battery_jeita_type_names[] = {
	[SPRD_BATTERY_JEITA_UNKNOWN] = "unknown-jeita-temp-table",
	[SPRD_BATTERY_JEITA_SDP] = "sdp-jeita-temp-table",
	[SPRD_BATTERY_JEITA_CDP] = "cdp-jeita-temp-table",
	[SPRD_BATTERY_JEITA_DCP] = "dcp-jeita-temp-table",
	[SPRD_BATTERY_JEITA_FCHG] = "fchg-jeita-temp-table",
	[SPRD_BATTERY_JEITA_FLASH] = "flash-jeita-temp-table",
	[SPRD_BATTERY_JEITA_WL_BPP] = "wl-bpp-jeita-temp-table",
	[SPRD_BATTERY_JEITA_WL_EPP] = "wl-epp-jeita-temp-table",
};

struct sprd_battery_jeita_table {
	int temp;
	int recovery_temp;
	int current_ua;
	int term_volt;
};

struct sprd_battery_step_chg_table {
	int jeita_inr;
	int current_ua;
	int term_volt;
};

struct sprd_battery_cycles_fcc_table {
	int cols;
	int cycles[SPRD_BATTERY_CYCLES_FCC_COLS_MAX];	/* charge cycles */
	int fcc_uah[SPRD_BATTERY_CYCLES_FCC_COLS_MAX];	/* fcc_uah */
};

struct sprd_battery_ir_compensation {
	/* microOhms */
	int rc_uohm;
	int us_upper_limit_uv;
	int cv_upper_limit_offset_uv;
};

/* microAmps */
struct sprd_battery_charge_current {
	int sdp_limit;
	int sdp_cur;
	int dcp_limit;
	int dcp_cur;
	int cdp_limit;
	int cdp_cur;
	int unknown_limit;
	int unknown_cur;
	int fchg_limit;
	int fchg_cur;
	int flash_limit;
	int flash_cur;
	int wl_bpp_cur;
	int wl_bpp_limit;
	int wl_epp_cur;
	int wl_epp_limit;
};

typedef struct sprd_battery_energy_density_ocv_table {
	int engy_dens_ocv_hi;
	int engy_dens_ocv_lo;
} density_ocv_table;

struct sprd_battery_ocv_table {
	int ocv;	/* microVolts */
	int capacity;	/* percent */
};

struct sprd_battery_resistance_temp_table {
	int temp;	/* celsius */
	int resistance;	/* internal resistance percent */
};

struct sprd_battery_cur_comp_table {
	int current_ma;	/* microAmps */
	int compensate;	/* percent */
};

struct sprd_battery_vol_temp_table {
	int vol;	/* microVolts */
	int temp;	/* celsius */
};

struct sprd_battery_temp_cap_table {
	int temp;	/* celsius */
	int cap;	/* capacity percentage */
};

struct sprd_cycles_agingfactor_lut {
	int cols;
	int cycles[SPRD_CYCLES_AGINGFACTOR_COLS_MAX];	/* charge cycles */
	int agingfactor[SPRD_CYCLES_AGINGFACTOR_COLS_MAX];	/* percent */
};

struct sprd_temp_fcc_lut {
	int cols;
	int temp[SPRD_TEMP_FCC_COLS_MAX];	/* celsius */
	int fcc[SPRD_TEMP_FCC_COLS_MAX];	/* uah */
};

struct sprd_temp_hc_lut {
	int cols;
	int temp[SPRD_TEMP_FCC_COLS_MAX];	/* celsius */
	int hc[SPRD_TEMP_FCC_COLS_MAX];	/* percent */
};

struct sprd_temp_chg_loss_lut {
	int cols;
	int temp[SPRD_TEMP_FCC_COLS_MAX];	/* celsius */
	int chg_loss_uah[SPRD_TEMP_FCC_COLS_MAX];	/* microVolts */
	int ocv[SPRD_TEMP_FCC_COLS_MAX];	/* microVolts */
	int soc_loss[SPRD_TEMP_FCC_COLS_MAX];	/* percent */
};

struct sprd_fcc_cycle_ratio_lut {
	int rows;
	int cols;
	int cycle[SPRD_CAP_TEMP_ROWS_MAX];	/* charge cycle */
	int temp[SPRD_CAP_TEMP_COLS_MAX];	/* celsius */
	int ratio[SPRD_CAP_TEMP_ROWS_MAX][SPRD_CAP_TEMP_COLS_MAX];
};

struct sprd_temp_zp_voltage_lut {
	int cols;
	int temp[SPRD_TEMP_ZP_VOL_COLS_MAX];	/* celsius */
	int vol[SPRD_TEMP_ZP_VOL_COLS_MAX];	/* microVolts */
};

struct sprd_temp_abs_zp_voltage_lut {
	int cols;
	int temp[SPRD_TEMP_ZP_VOL_COLS_MAX];	/* celsius */
	int vol[SPRD_TEMP_ZP_VOL_COLS_MAX];	/* microVolts */
};

struct sprd_temp_bat_voltage_lut {
	int cols;
	int temp[SPRD_TEMP_BAT_VOL_COLS_MAX];	/* celsius */
	int vol[SPRD_TEMP_BAT_VOL_COLS_MAX];	/* microVolts */
};

struct sprd_cap_temp_ocv_lut {
	int rows;
	int cols;
	int cap[SPRD_CAP_TEMP_ROWS_MAX];	/* capacity percentage */
	int temp[SPRD_CAP_TEMP_COLS_MAX];	/* celsius */
	int ocv[SPRD_CAP_TEMP_ROWS_MAX][SPRD_CAP_TEMP_COLS_MAX];  /* microVolts */
};

struct sprd_cap_temp_ocv_cycle_lut {
	int rows;
	int cols;
	int cap[SPRD_CAP_TEMP_ROWS_MAX];	/* capacity percentage */
	int temp[SPRD_CAP_TEMP_COLS_MAX];	/* celsius */
	int cycle_ocv[SPRD_CAP_TEMP_ROWS_MAX][SPRD_CAP_TEMP_COLS_MAX];  /* microVolts */
};

struct sprd_cap_temp_ibat_lut {
	int rows;
	int cols;
	int cap[SPRD_CAP_TEMP_ROWS_MAX];	/* capacity percentage */
	int temp[SPRD_CAP_TEMP_COLS_MAX];	/* celsius */
	int ibat[SPRD_CAP_TEMP_ROWS_MAX][SPRD_CAP_TEMP_COLS_MAX];  /* microA */
};

struct sprd_cap_temp_resist_lut {
	int rows;
	int cols;
//	int ocv[SPRD_OCV_TEMP_ROWS_MAX];	/* microVolts */
	int cap[SPRD_CAP_TEMP_ROWS_MAX];	/* capacity percentage */
	int temp[SPRD_OCV_TEMP_COLS_MAX];	/* celsius */
	int resist[SPRD_CAP_TEMP_ROWS_MAX][SPRD_OCV_TEMP_COLS_MAX];  /* microOhms */
};

struct sprd_cap_temp_resist_ratio_lut {
	int rows;
	int cols;
	int cap[SPRD_CAP_TEMP_ROWS_MAX];	/* capacity percentage */
	int temp[SPRD_OCV_TEMP_COLS_MAX];	/* celsius */
	int res_ratio[SPRD_CAP_TEMP_ROWS_MAX][SPRD_OCV_TEMP_COLS_MAX];
};

struct sprd_battery_temp_fullcap_table {
	int temp;	/* celsius */
	int advance_fullcap;	/* advance fullcap percentage */
};

struct sprd_battery_temp_fcc_table {
	int temp;	/* celsius */
	int fcc;	/* fcc uah */
};

struct sprd_battery_temp_cap_ocv_table {
	int temp;	/* celsius */
	int cap;	/* capacity percentage */
	int ocv;	/* microVolts */
};

struct sprd_battery_id_info {
	int battery_id;	/* battery_id */
	int charge_cycle;	/* charge_cycle */
	int aging_bat_id;	/* aging_bat_id */
	struct device_node *battery_np;
};

struct sprd_battery_full_alarm_table {
	unsigned int full_alarm_cap;	/* full alarm cap */
	unsigned int full_alarm_cur;	/* full alarm current */
	unsigned int full_alarm_cur_min;	/* full alarm min current */
	unsigned int full_alarm_cur_max1;	/* full alarm max1 current */
	unsigned int full_alarm_cur_max2;	/* full alarm max2 current */
};

struct sprd_battery_info {
	const char *vendor_cycle_range;
	/* microAmp-hours */
	int charge_full_design_uah;
	/* microAmp-hours */
	int charge_full_uah;
	/* microVolts */
	int voltage_min_design_uv;
	/* microVolts */
	int batt_ovp_threshold_uv;
	/* microAmps */
	int precharge_current_ua;
	/* microAmps */
	int charge_term_current_ua;
	/* microAmps */
	int constant_charge_current_max_ua;
	/* microVolts */
	int constant_charge_voltage_max_uv;
	/* microVolts */
	int fullbatt_voltage_offset_uv;
	/* microOhms */
	int factory_internal_resistance_uohm;
	/* microVolts */
	int fast_charge_ocv_threshold_uv;
	/* microOhms */
	int connector_resistance_uohm;

	/* microVolts */
	unsigned int fullbatt_voltage_uv;
	/* microAmps */
	unsigned int fullbatt_current_uA;
	/* microAmps */
	unsigned int first_fullbatt_current_uA;
	/* microVolts */
	int fullbatt_track_end_voltage_uv;
	/* microAmps */
	int fullbatt_track_end_current_uA;
	/* microVolts */
	int first_capacity_calibration_voltage_uv;
	/* microVolts */
	unsigned int shutdown_voltage_uv;
	/* percentage */
	int first_capacity_calibration_capacity;

	int bat_temp_len;
	/* celsius */
	int battery_internal_resistance_temp_table[SPRD_BATTERY_INFO_RESISTENCE_TEMP_MAX];
	int battery_internal_resistance_temp_table_len;
	int battery_ocv_temp_table[SPRD_BATTERY_OCV_TEMP_MAX];
	int battery_cap_temp_resist_ratio_table[SPRD_BATTERY_OCV_TEMP_MAX];
	int vbat_thres_temp_table[SPRD_BATTERY_BCL_VBAT_THRES_MAX];
	int vbat_thres_temp_table_len;
	/* Milliohm */
	int *battery_internal_resistance_table[SPRD_BATTERY_INFO_RESISTENCE_TEMP_MAX];
	int battery_internal_resistance_table_len[SPRD_BATTERY_INFO_RESISTENCE_TEMP_MAX];
	/* microVolts */
	int *battery_internal_resistance_ocv_table;
	int battery_internal_resistance_ocv_table_len;

	struct sprd_battery_charge_current cur;
	density_ocv_table *cap_calib_dens_ocv_table;
	int cap_calib_dens_ocv_table_len;

	density_ocv_table *cap_track_dens_ocv_table;
	int cap_track_dens_ocv_table_len;

	struct sprd_battery_ocv_table *battery_ocv_table[SPRD_BATTERY_OCV_TEMP_MAX];
	int battery_ocv_table_len[SPRD_BATTERY_OCV_TEMP_MAX];

	struct sprd_battery_vol_temp_table *battery_vol_temp_table;
	int battery_vol_temp_table_len;

	struct sprd_battery_temp_cap_table *battery_temp_cap_table;
	int battery_temp_cap_table_len;

	struct sprd_battery_temp_fullcap_table *battery_temp_fullcap_table;
	int battery_temp_fullcap_table_len;

	struct sprd_battery_resistance_temp_table *battery_temp_resist_table;
	int battery_temp_resist_table_len;

	int *high_int_thres_table[SPRD_BATTERY_BCL_VBAT_THRES_MAX];
	int high_int_thres_cols;
	int *low_int_thres_table[SPRD_BATTERY_BCL_VBAT_THRES_MAX];
	int low_int_thres_cols;

	struct sprd_battery_full_alarm_table *battery_full_alarm_table;
	int battery_full_alarm_table_len;

	/* new mode */
	struct sprd_cycles_agingfactor_lut *battery_cycles_agingfactor_lut;

	struct sprd_temp_fcc_lut *battery_temp_fcc_lut;

	struct sprd_temp_fcc_lut *battery_adjust_temp_fcc_lut;

	struct sprd_temp_hc_lut *battery_temp_hc_lut;

	struct sprd_temp_zp_voltage_lut *battery_temp_zp_vol_lut;

	struct sprd_temp_abs_zp_voltage_lut *battery_temp_abs_zp_vol_lut;

	struct sprd_temp_bat_voltage_lut *battery_temp_bat_vol_lut;

	struct sprd_cap_temp_ocv_lut *battery_cap_temp_ocv_lut;

	struct sprd_cap_temp_ocv_cycle_lut *battery_cap_temp_ocv_cycle_lut;

	struct sprd_cap_temp_ibat_lut *battery_cap_temp_ibat_lut;

	struct sprd_cap_temp_resist_lut *battery_cap_temp_resist_lut;

	struct sprd_fcc_cycle_ratio_lut *battery_fcc_cycle_ratio_lut;
	struct sprd_cap_temp_resist_ratio_lut
		*battery_cap_temp_resist_ratio_lut[SPRD_BATTERY_OCV_TEMP_MAX];
	int battery_cap_temp_resist_ratio_lut_len;
	struct sprd_battery_jeita_table *jeita_table[SPRD_BATTERY_JEITA_MAX];
	u32 max_current_jeita_index[SPRD_BATTERY_JEITA_MAX];
	u32 sprd_battery_jeita_size[SPRD_BATTERY_JEITA_MAX];

	struct sprd_battery_step_chg_table *step_chg_table;
	u32 sprd_battery_step_chg_size;

	struct sprd_battery_ir_compensation ir;
	u32 remap_full_percent_num;
	u32 *batt_full_percent;
};

extern void sprd_battery_put_battery_info(struct power_supply *psy,
					  struct sprd_battery_info *info);
extern int sprd_battery_get_battery_info(struct power_supply *psy,
					 struct sprd_battery_info *info, int dynamic_aging_bat_id);
extern void sprd_battery_find_resistance_table(struct power_supply *psy,
					       int **resistance_table,
					       int table_len, int *temp_table,
					       int temp_len, int temp,
					       int *target_table);
extern int sprd_battery_parse_battery_id(struct power_supply *psy);
extern int sprd_battery_get_aging_bat_id(struct power_supply *psy, int charge_cycle);
extern struct sprd_battery_ocv_table *
sprd_battery_find_ocv2cap_table(struct sprd_battery_info *info,
				int temp, int *table_len);

#endif /* _SPRD_BATTERY_INFO_H */

