/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2011 Unisoc Co., Ltd.
 * Changhua.Zhang <Changhua.Zhang@unisoc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/power_supply.h>

enum sprd_fgu_int_command {
	SPRD_FGU_VOLT_LOW_INT_CMD = 0,
	SPRD_FGU_VOLT_HIGH_INT_CMD,
	SPRD_FGU_CLBCNT_DELTA_INT_CMD,
	SPRD_FGU_POWER_LOW_CNT_INT_CMD,
	SPRD_FGU_RELAX_CNT_INT_CMD,
};

enum sprd_fgu_int_event {
	SPRD_FGU_VOLT_LOW_INT_EVENT = 0,
	SPRD_FGU_VOLT_HIGH_INT_EVENT,
	SPRD_FGU_CLBCNT_DELTA_INT_EVENT,
	SPRD_FGU_POWER_LOW_CNT_INT_EVENT,
	SPRD_FGU_RELAX_CNT_INT_EVENT,
};

enum sprd_fgu_sts_command {
	SPRD_FGU_CURT_LOW_STS_CMD = 0,
	SPRD_FGU_POWER_LOW_STS_CMD,
	SPRD_FGU_INVALID_POCV_STS_CMD,
	SPRD_FGU_BATTERY_FLAG_STS_CMD,
	SPRD_FGU_CLK_SEL_FGU_STS_CMD,
};

enum sprd_fgu_dump_fgu_info_level {
	DUMP_FGU_INFO_LEVEL_0 = 0,
	DUMP_FGU_INFO_LEVEL_1,
	DUMP_FGU_INFO_LEVEL_2,
	DUMP_FGU_INFO_LEVEL_3,
	DUMP_FGU_INFO_LEVEL_4,
	DUMP_FGU_INFO_LEVEL_MAX,
};

struct sprd_fgu_variant_data {
	u32 module_en;
	u32 clk_en;
	u32 fgu_cal;
	u32 fgu_cal_shift;
};

struct sprd_fgu_sleep_capacity_calibration {
	bool support_slp_calib;
	int suspend_ocv_uv;
	int resume_ocv_uv;
	int suspend_cc_uah;
	int resume_cc_uah;
	s64 suspend_time;
	s64 resume_time;
	int resume_ocv_cap;

	/* UMP518 relax cnt function define */
	int relax_cur_threshold;
	int relax_state_time_threshold;
	int power_low_counter_threshold;
	bool power_low_cnt_int_ocurred;

	/* UMP96XX/SC27XX relax cnt function define */
	int relax_cnt_threshold;
	bool relax_cnt_int_ocurred;
};

/*
 * struct sprd_fgu_info: describe the sprd fgu device
 * @regmap: regmap for register access
 * @base: the base offset for the controller
 * @dev: platform device
 * @cur_1mv_adc: ADC0 value corresponding to 1 mV
 * @cur_1000ma_adc: ADC0 value corresponding to 1000 mA
 * @cur_1code_lsb: ADC0 current value corresponding to 1 code
 * @vol_1000mv_adc: ADC1 value corresponding to 1000 mV
 * @cur_zero_point_adc: ADC0 current zero point adc value
 * @calib_resist: the real resistance of coulomb counter chip in uOhm
 * @standard_calib_resist: the standard resistance of coulomb counter chip in uOhm
 * @lock: protect the structure
 * @ops: pointer of sprd fgu device
 * @slp_cap_calib: struct of sleep capacity calib
 * @pdata: struct of pdata
 */
struct sprd_fgu_info {
	struct regmap *regmap;
	u32 base;
	struct device *dev;
	int cur_1mv_adc;
	int cur_1000ma_adc;
	int cur_1code_lsb;
	int vol_1000mv_adc;
	int cur_zero_point_adc;
	int calib_resist;
	int standard_calib_resist;
	struct mutex lock;
	struct mutex magic_lock;
	struct sprd_fgu_device_ops *ops;
	struct sprd_fgu_sleep_capacity_calibration slp_cap_calib;
	const struct sprd_fgu_variant_data *pdata;
	bool real_time_calib;
	bool use_miscdata;
	bool capacity_remap_enable;
	bool capacity_hc_enable;
	int batt_ocv_mv_magic;
	int batt_ocv_temp_magic;
	int batt_shutdown_temp_magic;
	int batt_cc_mah_magic;
};

struct sprd_fgu_device_ops {
	int (*enable_fgu_module)(struct sprd_fgu_info *info, bool enable);
	int (*clr_fgu_int)(struct sprd_fgu_info *info);
	int (*clr_fgu_int_all)(struct sprd_fgu_info *info, unsigned int int_sts);
	int (*clr_fgu_int_bit)(struct sprd_fgu_info *info, enum sprd_fgu_int_command int_cmd);
	int (*get_fgu_sts)(struct sprd_fgu_info *info,
			   enum sprd_fgu_sts_command sts_cmd, int *fgu_sts);
	int (*enable_fgu_int)(struct sprd_fgu_info *info,
			      enum sprd_fgu_int_command int_cmd, bool enable);
	int (*get_fgu_int)(struct sprd_fgu_info *info, int *int_sts);
	int (*enable_relax_cnt_mode)(struct sprd_fgu_info *info);
	int (*suspend_calib_check_relax_counter_sts)(struct sprd_fgu_info *info);
	int (*cap2mah)(struct sprd_fgu_info *info, int total_mah, int cap);
	int (*set_low_overload)(struct sprd_fgu_info *info, int vol);
	int (*set_high_overload)(struct sprd_fgu_info *info, int vol);
	int (*get_vbat_now)(struct sprd_fgu_info *info, int *val);
	int (*get_vbat_avg)(struct sprd_fgu_info *info, int *val);
	int (*get_vbat_buf)(struct sprd_fgu_info *info, int index, int *val);
	int (*get_current_now)(struct sprd_fgu_info *info, int *val);
	int (*get_current_avg)(struct sprd_fgu_info *info, int *val);
	int (*get_current_buf)(struct sprd_fgu_info *info, int index, int *val);
	int (*reset_cc_mah)(struct sprd_fgu_info *info, int total_mah, int init_cap);
	int (*get_cc_uah)(struct sprd_fgu_info *info, int *cc_uah, bool is_adjust);
	int (*adjust_cap)(struct sprd_fgu_info *info, int cap);
	int (*set_cap_delta_thre)(struct sprd_fgu_info *info, int total_mah, int cap);
	int (*get_relax_cur_low)(struct sprd_fgu_info *info, int *cur_sts);
	int (*get_relax_power_low)(struct sprd_fgu_info *info, int *power_sts);
	int (*get_power_low_cnt_int)(struct sprd_fgu_info *info, int *int_sts);
	int (*relax_mode_config)(struct sprd_fgu_info *info);
	int (*fgu_calibration)(struct sprd_fgu_info *info);
	int (*get_poci)(struct sprd_fgu_info *info, int *val);
	int (*get_pocv)(struct sprd_fgu_info *info, int *val);
	bool (*is_first_poweron)(struct sprd_fgu_info *info);
	int (*save_boot_mode)(struct sprd_fgu_info *info, int boot_mode);
	int (*read_last_cap)(struct sprd_fgu_info *info, int *cap);
	int (*save_last_cap)(struct sprd_fgu_info *info, int cap);
	int (*read_normal_temperature_cap)(struct sprd_fgu_info *info, int *cap);
	int (*save_normal_temperature_cap)(struct sprd_fgu_info *info, int cap);
	int (*read_last_batt_ocv)(struct sprd_fgu_info *info, int *batt_ocv_mv,
				  int *reg, int *magic);
	int (*save_last_batt_ocv)(struct sprd_fgu_info *info, int batt_ocv_mv, int first_poweron);
	int (*read_last_batt_cc_mah)(struct sprd_fgu_info *info, int *batt_cc_mah,
				     int *reg, int *magic);
	int (*save_last_batt_cc_mah)(struct sprd_fgu_info *info, int batt_cc_mah);
	int (*read_last_batt_temp)(struct sprd_fgu_info *info, int *batt_temp,
				   int *reg, int *magic);
	int (*save_last_batt_temp)(struct sprd_fgu_info *info, int batt_temp);
	int (*read_last_shutdown_batt_temp)(struct sprd_fgu_info *info, int *batt_temp,
					    int *reg, int *magic);
	int (*save_last_shutdown_batt_temp)(struct sprd_fgu_info *info, int batt_temp);
	int (*get_reg_val)(struct sprd_fgu_info *info, int offset, int *reg_val);
	int (*set_reg_val)(struct sprd_fgu_info *info, int offset, int reg_val);
	void (*hw_init)(struct sprd_fgu_info *info, struct power_supply *psy);
	void (*dump_fgu_info)(struct sprd_fgu_info *info,
			      enum sprd_fgu_dump_fgu_info_level dump_level);
	void (*remove)(struct sprd_fgu_info *info);
	void (*shutdown)(struct sprd_fgu_info *info);
	int (*get_pocv_times)(struct sprd_fgu_info *info, int *pocv_time);
	int (*read_last_smooth_cap)(struct sprd_fgu_info *info, int *smooth_cap,
				    int *reg, int *magic);
	int (*save_last_smooth_cap)(struct sprd_fgu_info *info, int smooth_cap);
};
#if IS_ENABLED(CONFIG_FUEL_GAUGE_SC27XX)
struct sprd_fgu_info *sc27xx_fgu_info_register(struct device *dev);
#elif IS_ENABLED(CONFIG_FUEL_GAUGE_UMP96XX)
struct sprd_fgu_info *ump96xx_fgu_info_register(struct device *dev);
#endif
