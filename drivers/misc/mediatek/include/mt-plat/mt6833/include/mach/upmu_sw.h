/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/
#ifndef _MT_PMIC_UPMU_SW_H_
#define _MT_PMIC_UPMU_SW_H_

#include <mach/upmu_hw.h>

#define AUXADC_SUPPORT_IMM_CURRENT_MODE
#define BATTERY_SW_INIT
#define RBAT_PULL_UP_VOLT_BY_BIF
/* #define INIT_BAT_CUR_FROM_PTIM */

#define FG_RG_INT_EN_CHRDET	INT_CHRDET

#define FG_RG_INT_EN_BAT2_H  INT_BAT2_H
#define FG_RG_INT_EN_BAT2_L  INT_BAT2_L

#define FG_RG_INT_EN_BAT_TEMP_H INT_BAT_TEMP_H
#define FG_RG_INT_EN_BAT_TEMP_L INT_BAT_TEMP_L

#define FG_RG_INT_EN_NAG_C_DLTV INT_NAG_C_DLTV

#define FG_BAT0_INT_H_NO INT_FG_BAT_H
#define FG_BAT0_INT_L_NO INT_FG_BAT_L
#define FG_BAT_INT_H_NO INT_FG_BAT_H
#define FG_BAT_INT_L_NO INT_FG_BAT_L


#define FG_BAT1_INT_H_NO INT_FG_BAT_H
#define FG_BAT1_INT_L_NO INT_FG_BAT_L

#define FG_CUR_H_NO INT_FG_CUR_H
#define FG_CUR_L_NO INT_FG_CUR_L

#define FG_ZCV_NO INT_FG_ZCV
#define FG_N_CHARGE_L_NO INT_FG_N_CHARGE_L
#define FG_IAVG_H_NO INT_FG_IAVG_H
#define FG_IAVG_L_NO INT_FG_IAVG_L
#define FG_TIME_NO INT_ENUM_MAX
#define FG_BAT_PLUGOUT_NO INT_BATON_BAT_OUT

/* =============================================================================
 * Low battery level define
 * =============================================================================
 */
#define LOW_BATTERY_LEVEL enum LOW_BATTERY_LEVEL_TAG
#define LOW_BATTERY_PRIO enum LOW_BATTERY_PRIO_TAG

enum LOW_BATTERY_LEVEL_TAG {
	LOW_BATTERY_LEVEL_0 = 0,
	LOW_BATTERY_LEVEL_1 = 1,
	LOW_BATTERY_LEVEL_2 = 2
};

enum LOW_BATTERY_PRIO_TAG {
	LOW_BATTERY_PRIO_CPU_B = 0,
	LOW_BATTERY_PRIO_CPU_L = 1,
	LOW_BATTERY_PRIO_GPU = 2,
	LOW_BATTERY_PRIO_MD = 3,
	LOW_BATTERY_PRIO_MD5 = 4,
	LOW_BATTERY_PRIO_FLASHLIGHT = 5,
	LOW_BATTERY_PRIO_VIDEO = 6,
	LOW_BATTERY_PRIO_WIFI = 7,
	LOW_BATTERY_PRIO_BACKLIGHT = 8
};

extern void (*low_battery_callback)(LOW_BATTERY_LEVEL tag);
extern void register_low_battery_notify(
			void (*low_battery_callback)(LOW_BATTERY_LEVEL tag),
			LOW_BATTERY_PRIO prio_val);
extern void register_low_battery_notify_ext(
			void (*low_battery_callback)(LOW_BATTERY_LEVEL tag),
			LOW_BATTERY_PRIO prio_val);

/* =============================================================================
 * Battery OC level define
 * =============================================================================
 */
#define BATTERY_OC_LEVEL enum BATTERY_OC_LEVEL_TAG
#define BATTERY_OC_PRIO enum BATTERY_OC_PRIO_TAG

enum BATTERY_OC_LEVEL_TAG {
	BATTERY_OC_LEVEL_0 = 0,
	BATTERY_OC_LEVEL_1 = 1
};

enum BATTERY_OC_PRIO_TAG {
	BATTERY_OC_PRIO_CPU_B = 0,
	BATTERY_OC_PRIO_CPU_L = 1,
	BATTERY_OC_PRIO_GPU = 2,
	BATTERY_OC_PRIO_MD = 3,
	BATTERY_OC_PRIO_MD5 = 4,
	BATTERY_OC_PRIO_FLASHLIGHT = 5,
	BATTERY_OC_PRIO_CHARGER = 6
};

extern void (*battery_oc_callback)(BATTERY_OC_LEVEL tag);
extern void register_battery_oc_notify(
			void (*battery_oc_callback)(BATTERY_OC_LEVEL tag),
			BATTERY_OC_PRIO prio_val);

/* =============================================================================
 * Battery percent define
 * =============================================================================
 */
#define BATTERY_PERCENT_LEVEL enum BATTERY_PERCENT_LEVEL_TAG
#define BATTERY_PERCENT_PRIO enum BATTERY_PERCENT_PRIO_TAG

enum BATTERY_PERCENT_LEVEL_TAG {
	BATTERY_PERCENT_LEVEL_0 = 0,
	BATTERY_PERCENT_LEVEL_1 = 1
};

enum BATTERY_PERCENT_PRIO_TAG {
	BATTERY_PERCENT_PRIO_CPU_B = 0,
	BATTERY_PERCENT_PRIO_CPU_L = 1,
	BATTERY_PERCENT_PRIO_GPU = 2,
	BATTERY_PERCENT_PRIO_MD = 3,
	BATTERY_PERCENT_PRIO_MD5 = 4,
	BATTERY_PERCENT_PRIO_FLASHLIGHT = 5,
	BATTERY_PERCENT_PRIO_VIDEO = 6,
	BATTERY_PERCENT_PRIO_WIFI = 7,
	BATTERY_PERCENT_PRIO_BACKLIGHT = 8
};

extern void (*battery_percent_callback)(BATTERY_PERCENT_LEVEL tag);
extern void
register_battery_percent_notify(
		void (*battery_percent_callback)(BATTERY_PERCENT_LEVEL tag),
		BATTERY_PERCENT_PRIO prio_val);

/*=============================================================================
 * DLPT define
 *=============================================================================
 */
#define DLPT_PRIO enum DLPT_PRIO_TAG

enum DLPT_PRIO_TAG {
	DLPT_PRIO_PBM = 0,
	DLPT_PRIO_CPU_B = 1,
	DLPT_PRIO_CPU_L = 2,
	DLPT_PRIO_GPU = 3,
	DLPT_PRIO_MD = 4,
	DLPT_PRIO_MD5 = 5,
	DLPT_PRIO_FLASHLIGHT = 6,
	DLPT_PRIO_VIDEO = 7,
	DLPT_PRIO_WIFI = 8,
	DLPT_PRIO_BACKLIGHT = 9
};

extern void (*dlpt_callback)(unsigned int val);
extern void register_dlpt_notify(
		void (*dlpt_callback)(unsigned int val), DLPT_PRIO prio_val);
extern const PMU_FLAG_TABLE_ENTRY pmu_flags_table[];

extern unsigned short is_battery_remove;
extern unsigned short is_wdt_reboot_pmic;
extern unsigned short is_wdt_reboot_pmic_chk;

/*=============================================================================
 * PMIC IRQ ENUM define
 *=============================================================================
 */
enum PMIC_IRQ_ENUM {
	INT_VPU_OC,
	SP_BUCK_TOP_START = INT_VPU_OC,
	INT_VCORE_OC,
	INT_VGPU11_OC,
	INT_VGPU12_OC,
	INT_VMODEM_OC,
	INT_VPROC1_OC,
	INT_VPROC2_OC,
	INT_VS1_OC,
	INT_VS2_OC,
	INT_VPA_OC,
	NO_USE_0_10,
	NO_USE_0_11,
	NO_USE_0_12,
	NO_USE_0_13,
	NO_USE_0_14,
	NO_USE_0_15,
	INT_VFE28_OC,
	SP_LDO_TOP_START = INT_VFE28_OC,
	INT_VXO22_OC,
	INT_VRF18_OC,
	INT_VRF12_OC,
	INT_VEFUSE_OC,
	INT_VCN33_1_OC,
	INT_VCN33_2_OC,
	INT_VCN13_OC,
	INT_VCN18_OC,
	INT_VA09_OC,
	INT_VCAMIO_OC,
	INT_VA12_OC,
	INT_VAUX18_OC,
	INT_VAUD18_OC,
	INT_VIO18_OC,
	INT_VSRAM_PROC1_OC,
	INT_VSRAM_PROC2_OC,
	INT_VSRAM_OTHERS_OC,
	INT_VSRAM_MD_OC,
	INT_VEMC_OC,
	INT_VSIM1_OC,
	INT_VSIM2_OC,
	INT_VUSB_OC,
	INT_VRFCK_OC,
	INT_VBBCK_OC,
	INT_VBIF28_OC,
	INT_VIBR_OC,
	INT_VIO28_OC,
	INT_VM18_OC,
	INT_VUFS_OC,
	NO_USE_2_14,
	NO_USE_2_15,
	INT_PWRKEY,
	SP_PSC_TOP_START = INT_PWRKEY,
	INT_HOMEKEY,
	INT_PWRKEY_R,
	INT_HOMEKEY_R,
	INT_NI_LBAT_INT,
	INT_CHRDET_EDGE,
	NO_USE_3_6,
	NO_USE_3_7,
	NO_USE_3_8,
	NO_USE_3_9,
	NO_USE_3_10,
	NO_USE_3_11,
	NO_USE_3_12,
	NO_USE_3_13,
	NO_USE_3_14,
	NO_USE_3_15,
	INT_RTC,
	SP_SCK_TOP_START = INT_RTC,
	NO_USE_4_1,
	NO_USE_4_2,
	NO_USE_4_3,
	NO_USE_4_4,
	NO_USE_4_5,
	NO_USE_4_6,
	NO_USE_4_7,
	NO_USE_4_8,
	NO_USE_4_9,
	NO_USE_4_10,
	NO_USE_4_11,
	NO_USE_4_12,
	NO_USE_4_13,
	NO_USE_4_14,
	NO_USE_4_15,
	INT_FG_BAT_H,
	SP_BM_TOP_START = INT_FG_BAT_H,
	INT_FG_BAT_L,
	INT_FG_CUR_H,
	INT_FG_CUR_L,
	INT_FG_ZCV,
	NO_USE_5_5,
	NO_USE_5_6,
	INT_FG_N_CHARGE_L,
	INT_FG_IAVG_H,
	INT_FG_IAVG_L,
	NO_USE_5_10,
	INT_FG_DISCHARGE,
	INT_FG_CHARGE,
	NO_USE_5_13,
	NO_USE_5_14,
	NO_USE_5_15,
	INT_BATON_LV,
	NO_USE_6_1,
	INT_BATON_BAT_IN,
	INT_BATON_BAT_OUT,
	INT_BIF,
	NO_USE_6_5,
	NO_USE_6_6,
	NO_USE_6_7,
	NO_USE_6_8,
	NO_USE_6_9,
	NO_USE_6_10,
	NO_USE_6_11,
	NO_USE_6_12,
	NO_USE_6_13,
	NO_USE_6_14,
	NO_USE_6_15,
	INT_BAT_H,
	SP_HK_TOP_START = INT_BAT_H,
	INT_BAT_L,
	INT_BAT2_H,
	INT_BAT2_L,
	INT_BAT_TEMP_H,
	INT_BAT_TEMP_L,
	INT_THR_H,
	INT_THR_L,
	INT_AUXADC_IMP,
	INT_NAG_C_DLTV,
	NO_USE_7_10,
	NO_USE_7_11,
	NO_USE_7_12,
	NO_USE_7_13,
	NO_USE_7_14,
	NO_USE_7_15,
	INT_AUDIO,
	SP_AUD_TOP_START = INT_AUDIO,
	NO_USE_8_1,
	NO_USE_8_2,
	NO_USE_8_3,
	NO_USE_8_4,
	INT_ACCDET,
	INT_ACCDET_EINT0,
	INT_ACCDET_EINT1,
	NO_USE_8_8,
	NO_USE_8_9,
	NO_USE_8_10,
	NO_USE_8_11,
	NO_USE_8_12,
	NO_USE_8_13,
	NO_USE_8_14,
	NO_USE_8_15,
	INT_SPI_CMD_ALERT,
	SP_MISC_TOP_START = INT_SPI_CMD_ALERT,
	NO_USE_9_1,
	NO_USE_9_2,
	NO_USE_9_3,
	NO_USE_9_4,
	NO_USE_9_5,
	NO_USE_9_6,
	NO_USE_9_7,
	NO_USE_9_8,
	NO_USE_9_9,
	NO_USE_9_10,
	NO_USE_9_11,
	NO_USE_9_12,
	NO_USE_9_13,
	NO_USE_9_14,
	NO_USE_9_15,
	INT_ENUM_MAX,
};

/*=============================================================================
 * PMIC auxadc define
 *=============================================================================
 */
extern signed int g_I_SENSE_offset;
extern void pmic_auxadc_init(void);
extern void pmic_auxadc_lock(void);
extern void pmic_auxadc_unlock(void);
extern void mt_power_off(void);
/*=============================================================================
 * PMIC fg define
 *=============================================================================
 */
extern unsigned int bat_get_ui_percentage(void);
extern signed int fgauge_read_v_by_d(int d_val);
extern signed int fgauge_read_r_bat_by_v(signed int voltage);
extern signed int fgauge_read_IM_current(void *data);
extern void kpd_pwrkey_pmic_handler(unsigned long pressed);
extern void kpd_pmic_rstkey_handler(unsigned long pressed);
extern int is_mt6311_sw_ready(void);
extern int is_mt6311_exist(void);
extern int get_mt6311_i2c_ch_num(void);
/*extern bool crystal_exist_status(void);*/
#if defined CONFIG_MTK_LEGACY
extern void pmu_drv_tool_customization_init(void);
#endif
extern int batt_init_cust_data(void);
extern void PMIC_INIT_SETTING_V1(void);

extern int do_ptim_ex(bool isSuspend, unsigned int *bat, signed int *cur);

#endif /* _MT_PMIC_UPMU_SW_H_ */
