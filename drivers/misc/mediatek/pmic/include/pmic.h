/*
 * Copyright (C) 2017 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _PMIC_SW_H_
#define _PMIC_SW_H_

#define PMIC_DEBUG

#include <linux/smp.h>
#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/printk.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/consumer.h>

#include <mach/upmu_hw.h>
#include <mach/upmu_sw.h>

#include "mtk_pmic_common.h"

#ifdef CONFIG_MTK_PMIC_CHIP_MT6353
#include "mt6353/mtk_pmic_info.h"
#endif
#ifdef CONFIG_MTK_PMIC_CHIP_MT6335
#include "mt6335/mtk_pmic_info.h"
#endif
#ifdef CONFIG_MTK_PMIC_CHIP_MT6355
#include "mt6355/mtk_pmic_info.h"
#endif
#ifdef CONFIG_MTK_PMIC_CHIP_MT6356
#include "mt6356/mtk_pmic_info.h"
#endif
#ifdef CONFIG_MTK_PMIC_CHIP_MT6357
#include "mt6357/mtk_pmic_info.h"
#endif


#define PMIC_EN REGULATOR_CHANGE_STATUS
#define PMIC_VOL REGULATOR_CHANGE_VOLTAGE
#define PMIC_EN_VOL 9

#if defined(MTK_EVB_PLATFORM) || defined(CONFIG_FPGA_EARLY_PORTING)
#define ENABLE_ALL_OC_IRQ 0
#else
#define ENABLE_ALL_OC_IRQ 1
#endif

/*
 * PMIC EXTERN VARIABLE
 */
/*----- LOW_BATTERY_PROTECT -----*/
extern int g_lowbat_int_bottom;
extern int g_low_battery_level;
/*----- BATTERY_OC_PROTECT -----*/
extern int g_battery_oc_level;
/* for update VBIF28 by AUXADC */
extern unsigned int g_pmic_pad_vbif28_vol;
/* for chip version used */
extern unsigned int g_pmic_chip_version;

/*
 * PMIC EXTERN FUNCTIONS
 */
/*----- BATTERY_OC_PROTECT -----*/
extern void exec_battery_oc_callback(BATTERY_OC_LEVEL battery_oc_level);
extern void bat_oc_h_en_setting(int en_val);
extern void bat_oc_l_en_setting(int en_val);
/*----- CHRDET_PROTECT -----*/
#ifdef CONFIG_MTK_KERNEL_POWER_OFF_CHARGING
extern unsigned int upmu_get_rgs_chrdet(void);
#endif
/*----- PMIC thread -----*/
extern void pmic_enable_charger_detection_int(int x);
/*----- LDO OC  -----*/
extern void msdc_sd_power_off(void);
/*----- REGULATOR -----*/
extern int mtk_regulator_init(struct platform_device *dev);
extern unsigned int pmic_config_interface_buck_vsleep_check(unsigned int RegNum,
	unsigned int val, unsigned int MASK, unsigned int SHIFT);
extern void pmic_regulator_debug_init(
	struct platform_device *dev, struct dentry *debug_dir);
extern void pmic_regulator_suspend(void);
extern void pmic_regulator_resume(void);
/*----- EFUSE -----*/
extern int pmic_read_VMC_efuse(void);
/*----- Others -----*/
extern int PMIC_POWER_HOLD(unsigned int hold);
extern void mt_power_off(void);
extern const PMU_FLAG_TABLE_ENTRY pmu_flags_table[];
extern unsigned int bat_get_ui_percentage(void);
extern signed int fgauge_read_IM_current(void *data);
extern void pmic_auxadc_lock(void);
extern void pmic_auxadc_unlock(void);
extern unsigned int bat_get_ui_percentage(void);
extern signed int fgauge_read_v_by_d(int d_val);
extern signed int fgauge_read_r_bat_by_v(signed int voltage);
/*extern PMU_ChargerStruct BMT_status;*//*have defined in battery_common.h */
extern void kpd_pwrkey_pmic_handler(unsigned long pressed);
extern void kpd_pmic_rstkey_handler(unsigned long pressed);
extern int is_mt6311_sw_ready(void);
extern int is_mt6311_exist(void);
extern int get_mt6311_i2c_ch_num(void);
/*extern bool crystal_exist_status(void);*//*have defined in mtk_rtc.h */
#if !defined CONFIG_MTK_LEGACY
extern void pmu_drv_tool_customization_init(void);
#endif
extern int batt_init_cust_data(void);
#ifdef CONFIG_MTK_RTC
extern void rtc_enable_k_eosc(void);
#endif

extern unsigned int mt_gpio_to_irq(unsigned int gpio);
extern int mt_gpio_set_debounce(unsigned int gpio, unsigned int debounce);
extern unsigned int upmu_get_rgs_chrdet(void);
#ifdef CONFIG_MTK_PMIC_COMMON
extern int PMIC_check_battery(void);
extern int PMIC_check_wdt_status(void);
extern int PMIC_check_pwrhold_status(void);
extern void PMIC_LP_INIT_SETTING(void);
extern int PMIC_MD_INIT_SETTING_V1(void);
extern void PMIC_PWROFF_SEQ_SETTING(void);
extern int pmic_tracking_init(void);
#endif
extern unsigned int PMIC_CHIP_VER(void);
/*---------------------------------------------------*/

struct regulator;

struct mtk_regulator_vosel {
	unsigned int def_sel; /*-- default vosel --*/
	unsigned int cur_sel; /*-- current vosel --*/
	bool restore;
};
struct mtk_regulator {
	struct regulator_desc desc;
	struct regulator_init_data init_data;
	struct regulator_config config;
	struct device_attribute en_att;
	struct device_attribute voltage_att;
	struct regulator_dev *rdev;
	PMU_FLAGS_LIST_ENUM en_reg;
	PMU_FLAGS_LIST_ENUM vol_reg;
	PMU_FLAGS_LIST_ENUM qi_en_reg;
	PMU_FLAGS_LIST_ENUM qi_vol_reg;
	const void *pvoltages;
	const void *idxs;
	bool isUsedable;
	struct regulator *reg;
	int vsleep_en_saved;
	/*--- Add to record selector ---*/
	struct mtk_regulator_vosel vosel;
	/*--- BUCK/LDO ---*/
	const char *type;
	unsigned int (*en_cb)(unsigned int parm);
	unsigned int (*vol_cb)(unsigned int parm);
	unsigned int (*da_en_cb)(void);
	unsigned int (*da_vol_cb)(void);
};

#endif				/* _PMIC_SW_H_ */
