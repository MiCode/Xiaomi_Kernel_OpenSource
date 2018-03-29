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

#ifndef _MT6397_PMIC_UPMU_SW_H_
#define _MT6397_PMIC_UPMU_SW_H_

#include <linux/module.h>
#include <mach/upmu_hw.h>

#define PMIC_ISINK_RSV2_ISINK0_MASK 0x1
#define PMIC_ISINK_RSV2_ISINK0_SHIFT 13
#define PMIC_ISINK_RSV2_ISINK1_MASK 0x1
#define PMIC_ISINK_RSV2_ISINK1_SHIFT 14
#define PMIC_ISINK_RSV2_ISINK2_MASK 0x1
#define PMIC_ISINK_RSV2_ISINK2_SHIFT 15

/* ============================================================================== */
/* Low battery level define */
/* ============================================================================== */
typedef enum LOW_BATTERY_LEVEL_TAG {
	LOW_BATTERY_LEVEL_0 = 0,
	LOW_BATTERY_LEVEL_1 = 1,
	LOW_BATTERY_LEVEL_2 = 2
} LOW_BATTERY_LEVEL;

typedef enum LOW_BATTERY_PRIO_TAG {
	LOW_BATTERY_PRIO_CPU_B = 0,
	LOW_BATTERY_PRIO_CPU_L = 1,
	LOW_BATTERY_PRIO_GPU = 2,
	LOW_BATTERY_PRIO_MD = 3,
	LOW_BATTERY_PRIO_MD5 = 4,
	LOW_BATTERY_PRIO_FLASHLIGHT = 5,
	LOW_BATTERY_PRIO_VIDEO = 6,
	LOW_BATTERY_PRIO_WIFI = 7,
	LOW_BATTERY_PRIO_BACKLIGHT = 8
} LOW_BATTERY_PRIO;

extern void (*low_battery_callback)(LOW_BATTERY_LEVEL);
extern void register_low_battery_notify(void (*low_battery_callback) (LOW_BATTERY_LEVEL),
					LOW_BATTERY_PRIO prio_val);


/* ============================================================================== */
/* Battery OC level define */
/* ============================================================================== */
typedef enum BATTERY_OC_LEVEL_TAG {
	BATTERY_OC_LEVEL_0 = 0,
	BATTERY_OC_LEVEL_1 = 1
} BATTERY_OC_LEVEL;

typedef enum BATTERY_OC_PRIO_TAG {
	BATTERY_OC_PRIO_CPU_B = 0,
	BATTERY_OC_PRIO_CPU_L = 1,
	BATTERY_OC_PRIO_GPU = 2
} BATTERY_OC_PRIO;

extern void (*battery_oc_callback)(BATTERY_OC_LEVEL);
extern void register_battery_oc_notify(void (*battery_oc_callback) (BATTERY_OC_LEVEL),
				       BATTERY_OC_PRIO prio_val);


/* ============================================================================== */
/* Battery percent define */
/* ============================================================================== */
typedef enum BATTERY_PERCENT_LEVEL_TAG {
	BATTERY_PERCENT_LEVEL_0 = 0,
	BATTERY_PERCENT_LEVEL_1 = 1
} BATTERY_PERCENT_LEVEL;

typedef enum BATTERY_PERCENT_PRIO_TAG {
	BATTERY_PERCENT_PRIO_CPU_B = 0,
	BATTERY_PERCENT_PRIO_CPU_L = 1,
	BATTERY_PERCENT_PRIO_GPU = 2,
	BATTERY_PERCENT_PRIO_MD = 3,
	BATTERY_PERCENT_PRIO_MD5 = 4,
	BATTERY_PERCENT_PRIO_FLASHLIGHT = 5,
	BATTERY_PERCENT_PRIO_VIDEO = 6,
	BATTERY_PERCENT_PRIO_WIFI = 7,
	BATTERY_PERCENT_PRIO_BACKLIGHT = 8
} BATTERY_PERCENT_PRIO;

extern void (*battery_percent_callback)(BATTERY_PERCENT_LEVEL);
extern void
register_battery_percent_notify(void (*battery_percent_callback) (BATTERY_PERCENT_LEVEL),
				BATTERY_PERCENT_PRIO prio_val);

extern unsigned int pmic_config_interface(unsigned int RegNum, unsigned int val, unsigned int MASK, unsigned int SHIFT);
extern unsigned int pmic_read_interface(unsigned int RegNum, unsigned int *val, unsigned int MASK, unsigned int SHIFT);
extern void pmic_lock(void);
extern void pmic_unlock(void);
extern void pmic_smp_lock(void);
extern void pmic_smp_unlock(void);

/* ============================================================================== */
/* Inline upmu_ function define */
/* ============================================================================== */

static inline unsigned int upmu_get_rgs_vcdt_hv_det(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHR_CON0),
				  (&val),
				  (unsigned int) (PMIC_RGS_VCDT_HV_DET_MASK),
				  (unsigned int) (PMIC_RGS_VCDT_HV_DET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rgs_vcdt_lv_det(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHR_CON0),
				  (&val),
				  (unsigned int) (PMIC_RGS_VCDT_LV_DET_MASK),
				  (unsigned int) (PMIC_RGS_VCDT_LV_DET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rgs_chrdet(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHR_CON0),
				  (&val),
				  (unsigned int) (PMIC_RGS_CHRDET_MASK), (unsigned int) (PMIC_RGS_CHRDET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_chr_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CHR_EN_MASK), (unsigned int) (PMIC_RG_CHR_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_csdac_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CSDAC_EN_MASK), (unsigned int) (PMIC_RG_CSDAC_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pchr_automode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PCHR_AUTOMODE_MASK),
				    (unsigned int) (PMIC_RG_PCHR_AUTOMODE_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rgs_chr_ldo_det(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHR_CON0),
				  (&val),
				  (unsigned int) (PMIC_RGS_CHR_LDO_DET_MASK),
				  (unsigned int) (PMIC_RGS_CHR_LDO_DET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vcdt_hv_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCDT_HV_EN_MASK),
				    (unsigned int) (PMIC_RG_VCDT_HV_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcdt_hv_vth(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCDT_HV_VTH_MASK),
				    (unsigned int) (PMIC_RG_VCDT_HV_VTH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcdt_lv_vth(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCDT_LV_VTH_MASK),
				    (unsigned int) (PMIC_RG_VCDT_LV_VTH_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rgs_vbat_cc_det(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHR_CON2),
				  (&val),
				  (unsigned int) (PMIC_RGS_VBAT_CC_DET_MASK),
				  (unsigned int) (PMIC_RGS_VBAT_CC_DET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rgs_vbat_cv_det(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHR_CON2),
				  (&val),
				  (unsigned int) (PMIC_RGS_VBAT_CV_DET_MASK),
				  (unsigned int) (PMIC_RGS_VBAT_CV_DET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rgs_cs_det(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHR_CON2),
				  (&val),
				  (unsigned int) (PMIC_RGS_CS_DET_MASK), (unsigned int) (PMIC_RGS_CS_DET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_cs_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CS_EN_MASK), (unsigned int) (PMIC_RG_CS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vbat_cc_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VBAT_CC_EN_MASK),
				    (unsigned int) (PMIC_RG_VBAT_CC_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vbat_cv_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VBAT_CV_EN_MASK),
				    (unsigned int) (PMIC_RG_VBAT_CV_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vbat_cc_vth(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VBAT_CC_VTH_MASK),
				    (unsigned int) (PMIC_RG_VBAT_CC_VTH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vbat_cv_vth(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VBAT_CV_VTH_MASK),
				    (unsigned int) (PMIC_RG_VBAT_CV_VTH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_cs_vth(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CS_VTH_MASK), (unsigned int) (PMIC_RG_CS_VTH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pchr_toltc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PCHR_TOLTC_MASK),
				    (unsigned int) (PMIC_RG_PCHR_TOLTC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pchr_tohtc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PCHR_TOHTC_MASK),
				    (unsigned int) (PMIC_RG_PCHR_TOHTC_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rgs_vbat_ov_det(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHR_CON6),
				  (&val),
				  (unsigned int) (PMIC_RGS_VBAT_OV_DET_MASK),
				  (unsigned int) (PMIC_RGS_VBAT_OV_DET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vbat_ov_deg(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VBAT_OV_DEG_MASK),
				    (unsigned int) (PMIC_RG_VBAT_OV_DEG_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vbat_ov_vth(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VBAT_OV_VTH_MASK),
				    (unsigned int) (PMIC_RG_VBAT_OV_VTH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vbat_ov_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VBAT_OV_EN_MASK),
				    (unsigned int) (PMIC_RG_VBAT_OV_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rgs_baton_undet(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHR_CON7),
				  (&val),
				  (unsigned int) (PMIC_RGS_BATON_UNDET_MASK),
				  (unsigned int) (PMIC_RGS_BATON_UNDET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_baton_ht_trim_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BATON_HT_TRIM_SET_MASK),
				    (unsigned int) (PMIC_RG_BATON_HT_TRIM_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_baton_ht_trim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BATON_HT_TRIM_MASK),
				    (unsigned int) (PMIC_RG_BATON_HT_TRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_baton_tdet_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_BATON_TDET_EN_MASK),
				    (unsigned int) (PMIC_BATON_TDET_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_baton_ht_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BATON_HT_EN_MASK),
				    (unsigned int) (PMIC_RG_BATON_HT_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_baton_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BATON_EN_MASK), (unsigned int) (PMIC_RG_BATON_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_csdac_data(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CSDAC_DATA_MASK),
				    (unsigned int) (PMIC_RG_CSDAC_DATA_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_frc_csvth_usbdl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_FRC_CSVTH_USBDL_MASK),
				    (unsigned int) (PMIC_RG_FRC_CSVTH_USBDL_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rgs_otg_bvalid_det(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHR_CON10),
				  (&val),
				  (unsigned int) (PMIC_RGS_OTG_BVALID_DET_MASK),
				  (unsigned int) (PMIC_RGS_OTG_BVALID_DET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_otg_bvalid_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OTG_BVALID_EN_MASK),
				    (unsigned int) (PMIC_RG_OTG_BVALID_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pchr_flag_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PCHR_FLAG_EN_MASK),
				    (unsigned int) (PMIC_RG_PCHR_FLAG_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rgs_pchr_flag_out(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHR_CON10),
				  (&val),
				  (unsigned int) (PMIC_RGS_PCHR_FLAG_OUT_MASK),
				  (unsigned int) (PMIC_RGS_PCHR_FLAG_OUT_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_pchr_flag_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PCHR_FLAG_SEL_MASK),
				    (unsigned int) (PMIC_RG_PCHR_FLAG_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pchr_ft_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PCHR_FT_CTRL_MASK),
				    (unsigned int) (PMIC_RG_PCHR_FT_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pchr_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PCHR_RST_MASK), (unsigned int) (PMIC_RG_PCHR_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_csdac_testmode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CSDAC_TESTMODE_MASK),
				    (unsigned int) (PMIC_RG_CSDAC_TESTMODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pchr_testmode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PCHR_TESTMODE_MASK),
				    (unsigned int) (PMIC_RG_PCHR_TESTMODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_chrwdt_wr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CHRWDT_WR_MASK), (unsigned int) (PMIC_RG_CHRWDT_WR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_chrwdt_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CHRWDT_EN_MASK), (unsigned int) (PMIC_RG_CHRWDT_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_chrwdt_td(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CHRWDT_TD_MASK), (unsigned int) (PMIC_RG_CHRWDT_TD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pchr_rv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PCHR_RV_MASK), (unsigned int) (PMIC_RG_PCHR_RV_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rgs_chrwdt_out(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHR_CON15),
				  (&val),
				  (unsigned int) (PMIC_RGS_CHRWDT_OUT_MASK),
				  (unsigned int) (PMIC_RGS_CHRWDT_OUT_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_chrwdt_flag_wr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CHRWDT_FLAG_WR_MASK),
				    (unsigned int) (PMIC_RG_CHRWDT_FLAG_WR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_chrwdt_int_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CHRWDT_INT_EN_MASK),
				    (unsigned int) (PMIC_RG_CHRWDT_INT_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_adcin_vchr_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ADCIN_VCHR_EN_MASK),
				    (unsigned int) (PMIC_ADCIN_VCHR_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_adcin_vsen_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ADCIN_VSEN_EN_MASK),
				    (unsigned int) (PMIC_ADCIN_VSEN_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_adcin_vbat_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ADCIN_VBAT_EN_MASK),
				    (unsigned int) (PMIC_ADCIN_VBAT_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adcin_vsen_ext_baton_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADCIN_VSEN_EXT_BATON_EN_MASK),
				    (unsigned int) (PMIC_RG_ADCIN_VSEN_EXT_BATON_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_adcin_vsen_mux_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ADCIN_VSEN_MUX_EN_MASK),
				    (unsigned int) (PMIC_ADCIN_VSEN_MUX_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_usbdl_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_USBDL_SET_MASK), (unsigned int) (PMIC_RG_USBDL_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_usbdl_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_USBDL_RST_MASK), (unsigned int) (PMIC_RG_USBDL_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_uvlo_vthl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_UVLO_VTHL_MASK), (unsigned int) (PMIC_RG_UVLO_VTHL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_bgr_unchop(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON17),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BGR_UNCHOP_MASK),
				    (unsigned int) (PMIC_RG_BGR_UNCHOP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_bgr_unchop_ph(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON17),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BGR_UNCHOP_PH_MASK),
				    (unsigned int) (PMIC_RG_BGR_UNCHOP_PH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_bgr_rsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON17),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BGR_RSEL_MASK), (unsigned int) (PMIC_RG_BGR_RSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rgs_bc11_cmp_out(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHR_CON18),
				  (&val),
				  (unsigned int) (PMIC_RGS_BC11_CMP_OUT_MASK),
				  (unsigned int) (PMIC_RGS_BC11_CMP_OUT_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_bc11_vsrc_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BC11_VSRC_EN_MASK),
				    (unsigned int) (PMIC_RG_BC11_VSRC_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_bc11_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BC11_RST_MASK), (unsigned int) (PMIC_RG_BC11_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_bc11_bb_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BC11_BB_CTRL_MASK),
				    (unsigned int) (PMIC_RG_BC11_BB_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_bc11_bias_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BC11_BIAS_EN_MASK),
				    (unsigned int) (PMIC_RG_BC11_BIAS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_bc11_ipu_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BC11_IPU_EN_MASK),
				    (unsigned int) (PMIC_RG_BC11_IPU_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_bc11_ipd_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BC11_IPD_EN_MASK),
				    (unsigned int) (PMIC_RG_BC11_IPD_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_bc11_cmp_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BC11_CMP_EN_MASK),
				    (unsigned int) (PMIC_RG_BC11_CMP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_bc11_vref_vth(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BC11_VREF_VTH_MASK),
				    (unsigned int) (PMIC_RG_BC11_VREF_VTH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_csdac_stp_dec(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON20),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CSDAC_STP_DEC_MASK),
				    (unsigned int) (PMIC_RG_CSDAC_STP_DEC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_csdac_stp_inc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON20),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CSDAC_STP_INC_MASK),
				    (unsigned int) (PMIC_RG_CSDAC_STP_INC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_csdac_stp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON21),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CSDAC_STP_MASK), (unsigned int) (PMIC_RG_CSDAC_STP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_csdac_dly(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON21),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CSDAC_DLY_MASK), (unsigned int) (PMIC_RG_CSDAC_DLY_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_chrind_dimming(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON22),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CHRIND_DIMMING_MASK),
				    (unsigned int) (PMIC_RG_CHRIND_DIMMING_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_chrind_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON22),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CHRIND_ON_MASK), (unsigned int) (PMIC_RG_CHRIND_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_low_ich_db(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON22),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LOW_ICH_DB_MASK),
				    (unsigned int) (PMIC_RG_LOW_ICH_DB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_ulc_det_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON23),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ULC_DET_EN_MASK),
				    (unsigned int) (PMIC_RG_ULC_DET_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_hwcv_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON23),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_HWCV_EN_MASK), (unsigned int) (PMIC_RG_HWCV_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_tracking_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON23),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TRACKING_EN_MASK),
				    (unsigned int) (PMIC_RG_TRACKING_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_csdac_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON23),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CSDAC_MODE_MASK),
				    (unsigned int) (PMIC_RG_CSDAC_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcdt_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON23),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCDT_MODE_MASK), (unsigned int) (PMIC_RG_VCDT_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_cv_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON23),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CV_MODE_MASK), (unsigned int) (PMIC_RG_CV_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_ichrg_trim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON24),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ICHRG_TRIM_MASK),
				    (unsigned int) (PMIC_RG_ICHRG_TRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_bgr_trim_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON24),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BGR_TRIM_EN_MASK),
				    (unsigned int) (PMIC_RG_BGR_TRIM_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_bgr_trim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON25),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BGR_TRIM_MASK), (unsigned int) (PMIC_RG_BGR_TRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_ovp_trim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON26),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OVP_TRIM_MASK), (unsigned int) (PMIC_RG_OVP_TRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_bgr_test_rstb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON27),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BGR_TEST_RSTB_MASK),
				    (unsigned int) (PMIC_RG_BGR_TEST_RSTB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_bgr_test_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON27),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BGR_TEST_EN_MASK),
				    (unsigned int) (PMIC_RG_BGR_TEST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_qi_bgr_ext_buf_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON27),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_QI_BGR_EXT_BUF_EN_MASK),
				    (unsigned int) (PMIC_QI_BGR_EXT_BUF_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_chr_osc_trim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON27),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_CHR_OSC_TRIM_MASK), (unsigned int) (PMIC_CHR_OSC_TRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dac_usbdl_max(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON28),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DAC_USBDL_MAX_MASK),
				    (unsigned int) (PMIC_RG_DAC_USBDL_MAX_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pchr_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHR_CON29),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PCHR_RSV_MASK), (unsigned int) (PMIC_RG_PCHR_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_cid(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CID),
		(&val), (unsigned int) (PMIC_CID_MASK), (unsigned int) (PMIC_CID_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_strup_6m_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_STRUP_6M_PDN_MASK),
				    (unsigned int) (PMIC_RG_STRUP_6M_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_accdet_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ACCDET_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_ACCDET_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auxadc_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUXADC_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_AUXADC_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smps_ck_div_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMPS_CK_DIV_PDN_MASK),
				    (unsigned int) (PMIC_RG_SMPS_CK_DIV_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smps_ck_div2_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMPS_CK_DIV2_PDN_MASK),
				    (unsigned int) (PMIC_RG_SMPS_CK_DIV2_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_div_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_DIV_PDN_MASK),
				    (unsigned int) (PMIC_RG_SPK_DIV_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_pwm_div_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_PWM_DIV_PDN_MASK),
				    (unsigned int) (PMIC_RG_SPK_PWM_DIV_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_rtc_mclk_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_RTC_MCLK_PDN_MASK),
				    (unsigned int) (PMIC_RG_RTC_MCLK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_bst_drv_1m_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BST_DRV_1M_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_BST_DRV_1M_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_fgadc_ana_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_FGADC_ANA_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_FGADC_ANA_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_fgadc_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_FGADC_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_FGADC_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pwmoc_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PWMOC_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_PWMOC_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_SPK_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_aud_13m_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUD_13M_PDN_MASK),
				    (unsigned int) (PMIC_RG_AUD_13M_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_aud_26m_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUD_26M_PDN_MASK),
				    (unsigned int) (PMIC_RG_AUD_26M_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_top_ckpdn2_rsv_15(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TOP_CKPDN2_RSV_15_MASK),
				    (unsigned int) (PMIC_RG_TOP_CKPDN2_RSV_15_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_rtc_75k_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_RTC_75K_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_RTC_75K_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_strup_32k_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_STRUP_32K_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_STRUP_32K_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_buck_1m_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BUCK_1M_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_BUCK_1M_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_buck32k_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BUCK32K_PDN_MASK),
				    (unsigned int) (PMIC_RG_BUCK32K_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_buck_ana_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BUCK_ANA_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_BUCK_ANA_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_buck_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BUCK_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_BUCK_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_chr1m_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CHR1M_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_CHR1M_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_drv_32k_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DRV_32K_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_DRV_32K_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_intrp_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INTRP_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_INTRP_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_ldostb_1m_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LDOSTB_1M_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_LDOSTB_1M_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pchr_32k_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PCHR_32K_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_PCHR_32K_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_rtc_32k_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_RTC_32K_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_RTC_32K_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_strup_75k_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_STRUP_75K_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_STRUP_75K_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_fqmtr_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_FQMTR_PDN_MASK), (unsigned int) (PMIC_RG_FQMTR_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_rtc32k_1v8_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_RTC32K_1V8_PDN_MASK),
				    (unsigned int) (PMIC_RG_RTC32K_1V8_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_top_gpio_ckpdn_rsv_15_14(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_GPIO_CKPDN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TOP_GPIO_CKPDN_RSV_15_14_MASK),
				    (unsigned int) (PMIC_RG_TOP_GPIO_CKPDN_RSV_15_14_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_gpio32k_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_GPIO_CKPDN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_GPIO32K_PDN_MASK),
				    (unsigned int) (PMIC_RG_GPIO32K_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_gpio26m_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_GPIO_CKPDN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_GPIO26M_PDN_MASK),
				    (unsigned int) (PMIC_RG_GPIO26M_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_top_rst_con_rsv_15_9(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TOP_RST_CON_RSV_15_9_MASK),
				    (unsigned int) (PMIC_RG_TOP_RST_CON_RSV_15_9_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_fqmtr_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_FQMTR_RST_MASK), (unsigned int) (PMIC_RG_FQMTR_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_rtc_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_RTC_RST_MASK), (unsigned int) (PMIC_RG_RTC_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_driver_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DRIVER_RST_MASK),
				    (unsigned int) (PMIC_RG_DRIVER_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_RST_MASK), (unsigned int) (PMIC_RG_SPK_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_accdet_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ACCDET_RST_MASK),
				    (unsigned int) (PMIC_RG_ACCDET_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_fgadc_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_FGADC_RST_MASK), (unsigned int) (PMIC_RG_FGADC_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audio_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDIO_RST_MASK), (unsigned int) (PMIC_RG_AUDIO_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auxadc_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUXADC_RST_MASK),
				    (unsigned int) (PMIC_RG_AUXADC_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_man_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_MAN_RST_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_MAN_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_wrp_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (WRP_CKPDN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_WRP_PDN_MASK), (unsigned int) (PMIC_RG_WRP_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_wrp_32k_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (WRP_CKPDN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_WRP_32K_PDN_MASK),
				    (unsigned int) (PMIC_RG_WRP_32K_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_wrp_eint_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (WRP_CKPDN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_WRP_EINT_PDN_MASK),
				    (unsigned int) (PMIC_RG_WRP_EINT_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_wrp_kp_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (WRP_CKPDN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_WRP_KP_PDN_MASK),
				    (unsigned int) (PMIC_RG_WRP_KP_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_wrp_pwm_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (WRP_CKPDN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_WRP_PWM_PDN_MASK),
				    (unsigned int) (PMIC_RG_WRP_PWM_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_wrp_i2c2_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (WRP_CKPDN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_WRP_I2C2_PDN_MASK),
				    (unsigned int) (PMIC_RG_WRP_I2C2_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_wrp_i2c1_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (WRP_CKPDN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_WRP_I2C1_PDN_MASK),
				    (unsigned int) (PMIC_RG_WRP_I2C1_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_wrp_i2c0_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (WRP_CKPDN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_WRP_I2C0_PDN_MASK),
				    (unsigned int) (PMIC_RG_WRP_I2C0_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline int upmu_set_rg_wrp_i2c_pdn(unsigned int val, unsigned int n_i2c)
{
	int err;

	if (n_i2c >= 3)
		return -ENODEV;
	pmic_lock();
	err = pmic_config_interface(WRP_CKPDN, val, 1, n_i2c);
	pmic_unlock();
	if (err)
		err = -EIO;
	return err;
}

static inline void upmu_set_rg_wrp_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (WRP_RST_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_WRP_RST_MASK), (unsigned int) (PMIC_RG_WRP_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_eint_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (WRP_RST_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EINT_RST_MASK), (unsigned int) (PMIC_RG_EINT_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_kp_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (WRP_RST_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_KP_RST_MASK), (unsigned int) (PMIC_RG_KP_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pwm_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (WRP_RST_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PWM_RST_MASK), (unsigned int) (PMIC_RG_PWM_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_i2c2_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (WRP_RST_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_I2C2_RST_MASK), (unsigned int) (PMIC_RG_I2C2_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_i2c1_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (WRP_RST_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_I2C1_RST_MASK), (unsigned int) (PMIC_RG_I2C1_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_i2c0_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (WRP_RST_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_I2C0_RST_MASK), (unsigned int) (PMIC_RG_I2C0_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pwrkey_rst_td(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_MISC),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PWRKEY_RST_TD_MASK),
				    (unsigned int) (PMIC_RG_PWRKEY_RST_TD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pwrrst_tmr_dis(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_MISC),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PWRRST_TMR_DIS_MASK),
				    (unsigned int) (PMIC_RG_PWRRST_TMR_DIS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pwrkey_rst_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_MISC),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PWRKEY_RST_EN_MASK),
				    (unsigned int) (PMIC_RG_PWRKEY_RST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_homekey_rst_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_MISC),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_HOMEKEY_RST_EN_MASK),
				    (unsigned int) (PMIC_RG_HOMEKEY_RST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_rst_part_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_MISC),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_RST_PART_SEL_MASK),
				    (unsigned int) (PMIC_RG_RST_PART_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_top_rst_misc_rsv_3(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_MISC),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TOP_RST_MISC_RSV_3_MASK),
				    (unsigned int) (PMIC_RG_TOP_RST_MISC_RSV_3_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_strup_man_rst_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_MISC),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_STRUP_MAN_RST_EN_MASK),
				    (unsigned int) (PMIC_RG_STRUP_MAN_RST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_sysrstb_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_MISC),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SYSRSTB_EN_MASK),
				    (unsigned int) (PMIC_RG_SYSRSTB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_ap_rst_dis(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_RST_MISC),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AP_RST_DIS_MASK),
				    (unsigned int) (PMIC_RG_AP_RST_DIS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_osc_sel_align_dis(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OSC_SEL_ALIGN_DIS_MASK),
				    (unsigned int) (PMIC_RG_OSC_SEL_ALIGN_DIS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spitxck_inv_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_rg_spitxck_inv_sel_MASK),
				    (unsigned int) (PMIC_rg_spitxck_inv_sel_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_osc_hw_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OSC_HW_SEL_MASK),
				    (unsigned int) (PMIC_RG_OSC_HW_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_clksq_hw_auto_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CLKSQ_HW_AUTO_EN_MASK),
				    (unsigned int) (PMIC_RG_CLKSQ_HW_AUTO_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_srclkperi_hw_auto_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SRCLKPERI_HW_AUTO_EN_MASK),
				    (unsigned int) (PMIC_RG_SRCLKPERI_HW_AUTO_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_srclkmd2_hw_auto_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SRCLKMD2_HW_AUTO_EN_MASK),
				    (unsigned int) (PMIC_RG_SRCLKMD2_HW_AUTO_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_srcvolt_hw_auto_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SRCVOLT_HW_AUTO_EN_MASK),
				    (unsigned int) (PMIC_RG_SRCVOLT_HW_AUTO_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_osc_sel_auto(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OSC_SEL_AUTO_MASK),
				    (unsigned int) (PMIC_RG_OSC_SEL_AUTO_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_top_ckcon1_rsv_07(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TOP_CKCON1_RSV_07_MASK),
				    (unsigned int) (PMIC_RG_TOP_CKCON1_RSV_07_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smps_div2_src_autoff_dis(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMPS_DIV2_SRC_AUTOFF_DIS_MASK),
				    (unsigned int) (PMIC_RG_SMPS_DIV2_SRC_AUTOFF_DIS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smps_autoff_dis(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMPS_AUTOFF_DIS_MASK),
				    (unsigned int) (PMIC_RG_SMPS_AUTOFF_DIS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_clksq_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CLKSQ_EN_MASK), (unsigned int) (PMIC_RG_CLKSQ_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_srclkperi_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SRCLKPERI_EN_MASK),
				    (unsigned int) (PMIC_RG_SRCLKPERI_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_srclkmd2_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SRCLKMD2_EN_MASK),
				    (unsigned int) (PMIC_RG_SRCLKMD2_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_srcvolt_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SRCVOLT_EN_MASK),
				    (unsigned int) (PMIC_RG_SRCVOLT_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_osc_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OSC_SEL_MASK), (unsigned int) (PMIC_RG_OSC_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_fqmtr_cksel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_FQMTR_CKSEL_MASK),
				    (unsigned int) (PMIC_RG_FQMTR_CKSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_accdet_cksel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ACCDET_CKSEL_MASK),
				    (unsigned int) (PMIC_RG_ACCDET_CKSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_fg_ana_cksel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_FG_ANA_CKSEL_MASK),
				    (unsigned int) (PMIC_RG_FG_ANA_CKSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_regck_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_REGCK_SEL_MASK), (unsigned int) (PMIC_RG_REGCK_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_buck_2m_sel_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BUCK_2M_SEL_EN_MASK),
				    (unsigned int) (PMIC_RG_BUCK_2M_SEL_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vca15_6m_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_6M_SEL_MASK), (unsigned int) (PMIC_VCA15_6M_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcore_6m_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_6M_SEL_MASK), (unsigned int) (PMIC_VCORE_6M_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_div_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_DIV_SEL_MASK),
				    (unsigned int) (PMIC_RG_SPK_DIV_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_pwm_div_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_PWM_DIV_SEL_MASK),
				    (unsigned int) (PMIC_RG_SPK_PWM_DIV_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auxadc_div_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUXADC_DIV_SEL_MASK),
				    (unsigned int) (PMIC_RG_AUXADC_DIV_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_auxadc_tstsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_AUXADC_TSTSEL_MASK),
				    (unsigned int) (PMIC_AUXADC_TSTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_pmu75k_tst_dis(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_PMU75K_TST_DIS_MASK),
				    (unsigned int) (PMIC_PMU75K_TST_DIS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_smps_tst_dis(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SMPS_TST_DIS_MASK), (unsigned int) (PMIC_SMPS_TST_DIS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_chr1m_tst_dis(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_CHR1M_TST_DIS_MASK),
				    (unsigned int) (PMIC_CHR1M_TST_DIS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_aud26m_tst_dis(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_AUD26M_TST_DIS_MASK),
				    (unsigned int) (PMIC_AUD26M_TST_DIS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rtc32k_tst_dis(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RTC32K_TST_DIS_MASK),
				    (unsigned int) (PMIC_RTC32K_TST_DIS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fg_tst_dis(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_TST_DIS_MASK), (unsigned int) (PMIC_FG_TST_DIS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_tst_dis(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_TST_DIS_MASK), (unsigned int) (PMIC_SPK_TST_DIS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_chr1m_tstsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_CHR1M_TSTSEL_MASK), (unsigned int) (PMIC_CHR1M_TSTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_smps_tstsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SMPS_TSTSEL_MASK), (unsigned int) (PMIC_SMPS_TSTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_pmu75k_tstsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_PMU75K_TSTSEL_MASK),
				    (unsigned int) (PMIC_PMU75K_TSTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_aud26m_tstsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_AUD26M_TSTSEL_MASK),
				    (unsigned int) (PMIC_AUD26M_TSTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rtc32k_tstsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RTC32K_TSTSEL_MASK),
				    (unsigned int) (PMIC_RTC32K_TSTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fg_tstsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_TSTSEL_MASK), (unsigned int) (PMIC_FG_TSTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_tstsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_TSTSEL_MASK), (unsigned int) (PMIC_SPK_TSTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_top_cktst2_rsv_15_10(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TOP_CKTST2_RSV_15_10_MASK),
				    (unsigned int) (PMIC_RG_TOP_CKTST2_RSV_15_10_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_dcxo_tstsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_DCXO_TSTSEL_MASK), (unsigned int) (PMIC_DCXO_TSTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_dcxo_tst_dis(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_DCXO_TST_DIS_MASK), (unsigned int) (PMIC_DCXO_TST_DIS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_osc32_cksel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_OSC32_CKSEL_MASK), (unsigned int) (PMIC_OSC32_CKSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_xosc32_tstsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_XOSC32_TSTSEL_MASK),
				    (unsigned int) (PMIC_XOSC32_TSTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_xosc32_tst_dis(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_XOSC32_TST_DIS_MASK),
				    (unsigned int) (PMIC_XOSC32_TST_DIS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pchr_test_ck_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PCHR_TEST_CK_SEL_MASK),
				    (unsigned int) (PMIC_RG_PCHR_TEST_CK_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_strup_75k_26m_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_STRUP_75K_26M_SEL_MASK),
				    (unsigned int) (PMIC_RG_STRUP_75K_26M_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_tstsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_TSTSEL_MASK),
				    (unsigned int) (PMIC_ACCDET_TSTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_ck1m2m_tstsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_CK1M2M_TSTSEL_MASK),
				    (unsigned int) (PMIC_CK1M2M_TSTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_bgr_test_ck_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKTST2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_BGR_TEST_CK_EN_MASK),
				    (unsigned int) (PMIC_BGR_TEST_CK_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vdrm_deg_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_DEG_EN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_DEG_EN_MASK), (unsigned int) (PMIC_VDRM_DEG_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_deg_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_DEG_EN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_DEG_EN_MASK),
				    (unsigned int) (PMIC_VSRMCA7_DEG_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_deg_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_DEG_EN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_DEG_EN_MASK), (unsigned int) (PMIC_VPCA7_DEG_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_deg_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_DEG_EN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_DEG_EN_MASK), (unsigned int) (PMIC_VIO18_DEG_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgpu_deg_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_DEG_EN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_DEG_EN_MASK), (unsigned int) (PMIC_VGPU_DEG_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcore_deg_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_DEG_EN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_DEG_EN_MASK), (unsigned int) (PMIC_VCORE_DEG_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_deg_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_DEG_EN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_DEG_EN_MASK),
				    (unsigned int) (PMIC_VSRMCA15_DEG_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vca15_deg_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_DEG_EN),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_DEG_EN_MASK), (unsigned int) (PMIC_VCA15_DEG_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_oc_gear_bvalid_det(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_CTL0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_OC_GEAR_BVALID_DET_MASK),
				    (unsigned int) (PMIC_OC_GEAR_BVALID_DET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_oc_gear_vbaton_undet(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_CTL0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_OC_GEAR_VBATON_UNDET_MASK),
				    (unsigned int) (PMIC_OC_GEAR_VBATON_UNDET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_oc_gear_ldo(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_CTL0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_OC_GEAR_LDO_MASK), (unsigned int) (PMIC_OC_GEAR_LDO_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgpu_oc_wnd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_CTL1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_OC_WND_MASK), (unsigned int) (PMIC_VGPU_OC_WND_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgpu_oc_thd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_CTL1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_OC_THD_MASK), (unsigned int) (PMIC_VGPU_OC_THD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcore_oc_wnd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_CTL1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_OC_WND_MASK), (unsigned int) (PMIC_VCORE_OC_WND_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcore_oc_thd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_CTL1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_OC_THD_MASK), (unsigned int) (PMIC_VCORE_OC_THD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_oc_wnd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_CTL1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_OC_WND_MASK),
				    (unsigned int) (PMIC_VSRMCA15_OC_WND_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_oc_thd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_CTL1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_OC_THD_MASK),
				    (unsigned int) (PMIC_VSRMCA15_OC_THD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vca15_oc_wnd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_CTL1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_OC_WND_MASK), (unsigned int) (PMIC_VCA15_OC_WND_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vca15_oc_thd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_CTL1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_OC_THD_MASK), (unsigned int) (PMIC_VCA15_OC_THD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vdrm_oc_wnd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_CTL2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_OC_WND_MASK), (unsigned int) (PMIC_VDRM_OC_WND_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vdrm_oc_thd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_CTL2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_OC_THD_MASK), (unsigned int) (PMIC_VDRM_OC_THD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_oc_wnd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_CTL2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_OC_WND_MASK),
				    (unsigned int) (PMIC_VSRMCA7_OC_WND_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_oc_thd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_CTL2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_OC_THD_MASK),
				    (unsigned int) (PMIC_VSRMCA7_OC_THD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_oc_wnd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_CTL2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_OC_WND_MASK), (unsigned int) (PMIC_VPCA7_OC_WND_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_oc_thd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_CTL2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_OC_THD_MASK), (unsigned int) (PMIC_VPCA7_OC_THD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_oc_wnd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_CTL2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_OC_WND_MASK), (unsigned int) (PMIC_VIO18_OC_WND_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_oc_thd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (OC_CTL2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_OC_THD_MASK), (unsigned int) (PMIC_VIO18_OC_THD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_int_rsv_15_8(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_RSV),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_INT_RSV_15_8_MASK), (unsigned int) (PMIC_INT_RSV_15_8_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_ivgen_ext_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_RSV),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_IVGEN_EXT_EN_MASK), (unsigned int) (PMIC_IVGEN_EXT_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pwrkey_rstb_int_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_RSV),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PWRKEY_RSTB_INT_SEL_MASK),
				    (unsigned int) (PMIC_RG_PWRKEY_RSTB_INT_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pwrkey_int_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_RSV),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PWRKEY_INT_SEL_MASK),
				    (unsigned int) (PMIC_RG_PWRKEY_INT_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_homekey_int_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_RSV),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_HOMEKEY_INT_SEL_MASK),
				    (unsigned int) (PMIC_RG_HOMEKEY_INT_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_polarity_bvalid_det(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_RSV),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_POLARITY_BVALID_DET_MASK),
				    (unsigned int) (PMIC_POLARITY_BVALID_DET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_polarity_vbaton_undet(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_RSV),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_POLARITY_VBATON_UNDET_MASK),
				    (unsigned int) (PMIC_POLARITY_VBATON_UNDET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_polarity(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_RSV),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_POLARITY_MASK), (unsigned int) (PMIC_POLARITY_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_mon_grp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TEST_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_MON_GRP_SEL_MASK),
				    (unsigned int) (PMIC_RG_MON_GRP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_mon_flag_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TEST_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_MON_FLAG_SEL_MASK),
				    (unsigned int) (PMIC_RG_MON_FLAG_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_test_spk_pwm(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TEST_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TEST_SPK_PWM_MASK),
				    (unsigned int) (PMIC_RG_TEST_SPK_PWM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_test_spk(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TEST_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TEST_SPK_MASK), (unsigned int) (PMIC_RG_TEST_SPK_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_test_strup(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TEST_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TEST_STRUP_MASK),
				    (unsigned int) (PMIC_RG_TEST_STRUP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TEST_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_MODE_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_nandtree_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TEST_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_NANDTREE_MODE_MASK),
				    (unsigned int) (PMIC_RG_NANDTREE_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_test_auxadc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TEST_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TEST_AUXADC_MASK),
				    (unsigned int) (PMIC_RG_TEST_AUXADC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_test_fgpll(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TEST_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TEST_FGPLL_MASK),
				    (unsigned int) (PMIC_RG_TEST_FGPLL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_test_fg(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TEST_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TEST_FG_MASK), (unsigned int) (PMIC_RG_TEST_FG_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_test_aud(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TEST_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TEST_AUD_MASK), (unsigned int) (PMIC_RG_TEST_AUD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_test_wrap(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TEST_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TEST_WRAP_MASK), (unsigned int) (PMIC_RG_TEST_WRAP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_test_io_fg_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TEST_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TEST_IO_FG_SEL_MASK),
				    (unsigned int) (PMIC_RG_TEST_IO_FG_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_test_classd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TEST_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TEST_CLASSD_MASK),
				    (unsigned int) (PMIC_RG_TEST_CLASSD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_test_driver(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TEST_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TEST_DRIVER_MASK),
				    (unsigned int) (PMIC_RG_TEST_DRIVER_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_test_boost(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TEST_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TEST_BOOST_MASK),
				    (unsigned int) (PMIC_RG_TEST_BOOST_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_vrtc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STATUS0),
				  (&val),
				  (unsigned int) (PMIC_VRTC_STATUS_MASK), (unsigned int) (PMIC_VRTC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_status_vtcxo_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STATUS0),
				  (&val),
				  (unsigned int) (PMIC_STATUS_VTCXO_EN_MASK),
				  (unsigned int) (PMIC_STATUS_VTCXO_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_status_vusb_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STATUS0),
				  (&val),
				  (unsigned int) (PMIC_STATUS_VUSB_EN_MASK),
				  (unsigned int) (PMIC_STATUS_VUSB_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_status_vdrm_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STATUS0),
				  (&val),
				  (unsigned int) (PMIC_STATUS_VDRM_EN_MASK),
				  (unsigned int) (PMIC_STATUS_VDRM_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_status_vsrmca7_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STATUS0),
				  (&val),
				  (unsigned int) (PMIC_STATUS_VSRMCA7_EN_MASK),
				  (unsigned int) (PMIC_STATUS_VSRMCA7_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_status_vpca7_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STATUS0),
				  (&val),
				  (unsigned int) (PMIC_STATUS_VPCA7_EN_MASK),
				  (unsigned int) (PMIC_STATUS_VPCA7_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_status_vio18_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STATUS0),
				  (&val),
				  (unsigned int) (PMIC_STATUS_VIO18_EN_MASK),
				  (unsigned int) (PMIC_STATUS_VIO18_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_status_vgpu_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STATUS0),
				  (&val),
				  (unsigned int) (PMIC_STATUS_VGPU_EN_MASK),
				  (unsigned int) (PMIC_STATUS_VGPU_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_status_vcore_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STATUS0),
				  (&val),
				  (unsigned int) (PMIC_STATUS_VCORE_EN_MASK),
				  (unsigned int) (PMIC_STATUS_VCORE_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_status_vsrmca15_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STATUS0),
				  (&val),
				  (unsigned int) (PMIC_STATUS_VSRMCA15_EN_MASK),
				  (unsigned int) (PMIC_STATUS_VSRMCA15_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_status_vca15_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STATUS0),
				  (&val),
				  (unsigned int) (PMIC_STATUS_VCA15_EN_MASK),
				  (unsigned int) (PMIC_STATUS_VCA15_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_status_va28_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STATUS1),
				  (&val),
				  (unsigned int) (PMIC_STATUS_VA28_EN_MASK),
				  (unsigned int) (PMIC_STATUS_VA28_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_status_vcama_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STATUS1),
				  (&val),
				  (unsigned int) (PMIC_STATUS_VCAMA_EN_MASK),
				  (unsigned int) (PMIC_STATUS_VCAMA_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_status_vemc_3v3_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STATUS1),
				  (&val),
				  (unsigned int) (PMIC_STATUS_VEMC_3V3_EN_MASK),
				  (unsigned int) (PMIC_STATUS_VEMC_3V3_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_status_vgp1_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STATUS1),
				  (&val),
				  (unsigned int) (PMIC_STATUS_VCAMD_EN_MASK),
				  (unsigned int) (PMIC_STATUS_VCAMD_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_status_vgp2_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STATUS1),
				  (&val),
				  (unsigned int) (PMIC_STATUS_VCAMIO_EN_MASK),
				  (unsigned int) (PMIC_STATUS_VCAMIO_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_status_vgp3_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STATUS1),
				  (&val),
				  (unsigned int) (PMIC_STATUS_VCAMAF_EN_MASK),
				  (unsigned int) (PMIC_STATUS_VCAMAF_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_status_vgp4_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STATUS1),
				  (&val),
				  (unsigned int) (PMIC_STATUS_VGP4_EN_MASK),
				  (unsigned int) (PMIC_STATUS_VGP4_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_status_vgp5_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STATUS1),
				  (&val),
				  (unsigned int) (PMIC_STATUS_VGP5_EN_MASK),
				  (unsigned int) (PMIC_STATUS_VGP5_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_status_vgp6_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STATUS1),
				  (&val),
				  (unsigned int) (PMIC_STATUS_VGP6_EN_MASK),
				  (unsigned int) (PMIC_STATUS_VGP6_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_status_vibr_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STATUS1),
				  (&val),
				  (unsigned int) (PMIC_STATUS_VIBR_EN_MASK),
				  (unsigned int) (PMIC_STATUS_VIBR_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_status_vio28_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STATUS1),
				  (&val),
				  (unsigned int) (PMIC_STATUS_VIO28_EN_MASK),
				  (unsigned int) (PMIC_STATUS_VIO28_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_status_vmc_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STATUS1),
				  (&val),
				  (unsigned int) (PMIC_STATUS_VMC_EN_MASK), (unsigned int) (PMIC_STATUS_VMC_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_status_vmch_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STATUS1),
				  (&val),
				  (unsigned int) (PMIC_STATUS_VMCH_EN_MASK),
				  (unsigned int) (PMIC_STATUS_VMCH_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_vsrmca15_pg_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (PGSTATUS),
				  (&val),
				  (unsigned int) (PMIC_VSRMCA15_PG_DEB_MASK),
				  (unsigned int) (PMIC_VSRMCA15_PG_DEB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_vpca15_pg_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (PGSTATUS),
				  (&val),
				  (unsigned int) (PMIC_VPCA15_PG_DEB_MASK), (unsigned int) (PMIC_VPCA15_PG_DEB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_vmch_pg_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (PGSTATUS),
				  (&val),
				  (unsigned int) (PMIC_VMCH_PG_DEB_MASK), (unsigned int) (PMIC_VMCH_PG_DEB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_vmc_pg_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (PGSTATUS),
				  (&val),
				  (unsigned int) (PMIC_VMC_PG_DEB_MASK), (unsigned int) (PMIC_VMC_PG_DEB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_vemc_pg_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (PGSTATUS),
				  (&val),
				  (unsigned int) (PMIC_VEMC_PG_DEB_MASK), (unsigned int) (PMIC_VEMC_PG_DEB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_vtcxo_pg_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (PGSTATUS),
				  (&val),
				  (unsigned int) (PMIC_VTCXO_PG_DEB_MASK), (unsigned int) (PMIC_VTCXO_PG_DEB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_vio28_pg_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (PGSTATUS),
				  (&val),
				  (unsigned int) (PMIC_VIO28_PG_DEB_MASK), (unsigned int) (PMIC_VIO28_PG_DEB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_va28_pg_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (PGSTATUS),
				  (&val),
				  (unsigned int) (PMIC_VA28_PG_DEB_MASK), (unsigned int) (PMIC_VA28_PG_DEB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_vio18_pg_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (PGSTATUS),
				  (&val),
				  (unsigned int) (PMIC_VIO18_PG_DEB_MASK), (unsigned int) (PMIC_VIO18_PG_DEB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_vdrm_pg_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (PGSTATUS),
				  (&val),
				  (unsigned int) (PMIC_VDRM_PG_DEB_MASK), (unsigned int) (PMIC_VDRM_PG_DEB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_vcore_pg_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (PGSTATUS),
				  (&val),
				  (unsigned int) (PMIC_VCORE_PG_DEB_MASK), (unsigned int) (PMIC_VCORE_PG_DEB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_vsrmca7_pg_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (PGSTATUS),
				  (&val),
				  (unsigned int) (PMIC_VSRMCA7_PG_DEB_MASK),
				  (unsigned int) (PMIC_VSRMCA7_PG_DEB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_vpca7_pg_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (PGSTATUS),
				  (&val),
				  (unsigned int) (PMIC_VPCA7_PG_DEB_MASK), (unsigned int) (PMIC_VPCA7_PG_DEB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rtc_xtal_det_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (CHRSTATUS),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_rtc_xtal_det_rsv_MASK),
				    (unsigned int) (PMIC_rtc_xtal_det_rsv_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rtc_xtal_det_done(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHRSTATUS),
				  (&val),
				  (unsigned int) (PMIC_rtc_xtal_det_done_MASK),
				  (unsigned int) (PMIC_rtc_xtal_det_done_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_ro_baton_undet(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHRSTATUS),
				  (&val),
				  (unsigned int) (PMIC_RO_BATON_UNDET_MASK),
				  (unsigned int) (PMIC_RO_BATON_UNDET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_pchr_chrdet(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHRSTATUS),
				  (&val),
				  (unsigned int) (PMIC_PCHR_CHRDET_MASK), (unsigned int) (PMIC_PCHR_CHRDET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_vbat_ov(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHRSTATUS),
				  (&val), (unsigned int) (PMIC_VBAT_OV_MASK), (unsigned int) (PMIC_VBAT_OV_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_pwrkey_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHRSTATUS),
				  (&val),
				  (unsigned int) (PMIC_PWRKEY_DEB_MASK), (unsigned int) (PMIC_PWRKEY_DEB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_usbdl(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHRSTATUS),
				  (&val), (unsigned int) (PMIC_USBDL_MASK), (unsigned int) (PMIC_USBDL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_pwrkey_rst_b_int(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHRSTATUS),
				  (&val),
				  (unsigned int) (PMIC_PWRKEY_RST_B_INT_MASK),
				  (unsigned int) (PMIC_PWRKEY_RST_B_INT_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_pmu_test_mode_scan(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (CHRSTATUS),
				  (&val),
				  (unsigned int) (PMIC_PMU_TEST_MODE_SCAN_MASK),
				  (unsigned int) (PMIC_PMU_TEST_MODE_SCAN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vtcxo(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS0),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VTCXO_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VTCXO_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vusb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS0),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VUSB_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VUSB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vdrm(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS0),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VDRM_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VDRM_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vsrmca7(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS0),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VSRMCA7_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VSRMCA7_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vpca7(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS0),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VPCA7_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VPCA7_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vio18(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS0),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VIO18_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VIO18_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vgpu(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS0),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VGPU_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VGPU_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vcore(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS0),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VCORE_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VCORE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vsrmca15(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS0),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VSRMCA15_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VSRMCA15_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vca15(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS0),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VCA15_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VCA15_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_va28(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS1),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VA28_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VA28_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vcama(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS1),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VCAMA_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VCAMA_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vemc_3v3(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS1),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VEMC_3V3_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VEMC_3V3_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vgp1(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS1),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VCAMD_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VCAMD_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vgp2(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS1),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VCAMIO_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VCAMIO_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vgp3(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS1),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VCAMAF_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VCAMAF_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vgp4(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS1),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VGP4_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VGP4_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vgp5(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS1),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VGP5_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VGP5_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vgp6(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS1),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VGP6_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VGP6_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vibr(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS1),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VIBR_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VIBR_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vio28(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS1),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VIO28_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VIO28_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vmc(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS1),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VMC_MASK), (unsigned int) (PMIC_OC_STATUS_VMC_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_oc_status_vmch(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS1),
				  (&val),
				  (unsigned int) (PMIC_OC_STATUS_VMCH_MASK),
				  (unsigned int) (PMIC_OC_STATUS_VMCH_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_homekey_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS2),
				  (&val),
				  (unsigned int) (PMIC_HOMEKEY_DEB_MASK), (unsigned int) (PMIC_HOMEKEY_DEB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_spk_oc_det_d_r(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS2),
				  (&val),
				  (unsigned int) (PMIC_SPK_OC_DET_D_R_MASK),
				  (unsigned int) (PMIC_SPK_OC_DET_D_R_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_spk_oc_det_d_l(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS2),
				  (&val),
				  (unsigned int) (PMIC_SPK_OC_DET_D_L_MASK),
				  (unsigned int) (PMIC_SPK_OC_DET_D_L_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_spk_oc_det_ab_r(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS2),
				  (&val),
				  (unsigned int) (PMIC_SPK_OC_DET_AB_R_MASK),
				  (unsigned int) (PMIC_SPK_OC_DET_AB_R_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_spk_oc_det_ab_l(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (OCSTATUS2),
				  (&val),
				  (unsigned int) (PMIC_SPK_OC_DET_AB_L_MASK),
				  (unsigned int) (PMIC_SPK_OC_DET_AB_L_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_top_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (HDMI_PAD_IE),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TOP_RSV_MASK), (unsigned int) (PMIC_RG_TOP_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_hdmi_pad_ie(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (HDMI_PAD_IE),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_HDMI_PAD_IE_MASK),
				    (unsigned int) (PMIC_RG_HDMI_PAD_IE_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_test_out_l(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (TEST_OUT_L),
				  (&val),
				  (unsigned int) (PMIC_TEST_OUT_L_MASK), (unsigned int) (PMIC_TEST_OUT_L_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_test_out_h(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (TEST_OUT_H),
				  (&val),
				  (unsigned int) (PMIC_TEST_OUT_H_MASK), (unsigned int) (PMIC_TEST_OUT_H_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_hdmi_tdsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TDSEL_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_HDMI_TDSEL_MASK),
				    (unsigned int) (PMIC_RG_HDMI_TDSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pmu_tdsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TDSEL_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PMU_TDSEL_MASK), (unsigned int) (PMIC_RG_PMU_TDSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spi_tdsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TDSEL_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPI_TDSEL_MASK), (unsigned int) (PMIC_RG_SPI_TDSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_i2s_tdsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TDSEL_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_I2S_TDSEL_MASK), (unsigned int) (PMIC_RG_I2S_TDSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_kp_tdsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TDSEL_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_KP_TDSEL_MASK), (unsigned int) (PMIC_RG_KP_TDSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pwm_tdsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TDSEL_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PWM_TDSEL_MASK), (unsigned int) (PMIC_RG_PWM_TDSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_i2c_tdsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TDSEL_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_I2C_TDSEL_MASK), (unsigned int) (PMIC_RG_I2C_TDSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_simap_tdsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TDSEL_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SIMAP_TDSEL_MASK),
				    (unsigned int) (PMIC_RG_SIMAP_TDSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_hdmi_rdsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RDSEL_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_HDMI_RDSEL_MASK),
				    (unsigned int) (PMIC_RG_HDMI_RDSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pmu_rdsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RDSEL_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PMU_RDSEL_MASK), (unsigned int) (PMIC_RG_PMU_RDSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spi_rdsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RDSEL_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPI_RDSEL_MASK), (unsigned int) (PMIC_RG_SPI_RDSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_i2s_rdsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RDSEL_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_I2S_RDSEL_MASK), (unsigned int) (PMIC_RG_I2S_RDSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_kp_rdsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RDSEL_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_KP_RDSEL_MASK), (unsigned int) (PMIC_RG_KP_RDSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pwm_rdsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RDSEL_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PWM_RDSEL_MASK), (unsigned int) (PMIC_RG_PWM_RDSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_i2c_rdsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RDSEL_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_I2C_RDSEL_MASK), (unsigned int) (PMIC_RG_I2C_RDSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_simap_rdsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RDSEL_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SIMAP_RDSEL_MASK),
				    (unsigned int) (PMIC_RG_SIMAP_RDSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt15(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT15_MASK), (unsigned int) (PMIC_RG_SMT15_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt14(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT14_MASK), (unsigned int) (PMIC_RG_SMT14_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt13(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT13_MASK), (unsigned int) (PMIC_RG_SMT13_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT12_MASK), (unsigned int) (PMIC_RG_SMT12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt11(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT11_MASK), (unsigned int) (PMIC_RG_SMT11_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt10(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT10_MASK), (unsigned int) (PMIC_RG_SMT10_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt9(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT9_MASK), (unsigned int) (PMIC_RG_SMT9_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt8(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT8_MASK), (unsigned int) (PMIC_RG_SMT8_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt7(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT7_MASK), (unsigned int) (PMIC_RG_SMT7_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt6(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT6_MASK), (unsigned int) (PMIC_RG_SMT6_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt5(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT5_MASK), (unsigned int) (PMIC_RG_SMT5_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt4(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT4_MASK), (unsigned int) (PMIC_RG_SMT4_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt3(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT3_MASK), (unsigned int) (PMIC_RG_SMT3_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT2_MASK), (unsigned int) (PMIC_RG_SMT2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT1_MASK), (unsigned int) (PMIC_RG_SMT1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT0_MASK), (unsigned int) (PMIC_RG_SMT0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt31(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT31_MASK), (unsigned int) (PMIC_RG_SMT31_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt30(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT30_MASK), (unsigned int) (PMIC_RG_SMT30_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt29(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT29_MASK), (unsigned int) (PMIC_RG_SMT29_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt28(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT28_MASK), (unsigned int) (PMIC_RG_SMT28_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt27(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT27_MASK), (unsigned int) (PMIC_RG_SMT27_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt26(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT26_MASK), (unsigned int) (PMIC_RG_SMT26_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt25(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT25_MASK), (unsigned int) (PMIC_RG_SMT25_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt24(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT24_MASK), (unsigned int) (PMIC_RG_SMT24_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt23(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT23_MASK), (unsigned int) (PMIC_RG_SMT23_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt22(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT22_MASK), (unsigned int) (PMIC_RG_SMT22_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt21(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT21_MASK), (unsigned int) (PMIC_RG_SMT21_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt20(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT20_MASK), (unsigned int) (PMIC_RG_SMT20_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt19(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT19_MASK), (unsigned int) (PMIC_RG_SMT19_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt18(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT18_MASK), (unsigned int) (PMIC_RG_SMT18_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt17(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT17_MASK), (unsigned int) (PMIC_RG_SMT17_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt16(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT16_MASK), (unsigned int) (PMIC_RG_SMT16_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt47(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT47_MASK), (unsigned int) (PMIC_RG_SMT47_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt46(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT46_MASK), (unsigned int) (PMIC_RG_SMT46_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt45(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT45_MASK), (unsigned int) (PMIC_RG_SMT45_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt44(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT44_MASK), (unsigned int) (PMIC_RG_SMT44_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt43(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT43_MASK), (unsigned int) (PMIC_RG_SMT43_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt42(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT42_MASK), (unsigned int) (PMIC_RG_SMT42_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt41(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT41_MASK), (unsigned int) (PMIC_RG_SMT41_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt40(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT40_MASK), (unsigned int) (PMIC_RG_SMT40_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt39(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT39_MASK), (unsigned int) (PMIC_RG_SMT39_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt38(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT38_MASK), (unsigned int) (PMIC_RG_SMT38_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt37(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT37_MASK), (unsigned int) (PMIC_RG_SMT37_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt36(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT36_MASK), (unsigned int) (PMIC_RG_SMT36_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt35(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT35_MASK), (unsigned int) (PMIC_RG_SMT35_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt34(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT34_MASK), (unsigned int) (PMIC_RG_SMT34_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt33(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT33_MASK), (unsigned int) (PMIC_RG_SMT33_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt32(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT32_MASK), (unsigned int) (PMIC_RG_SMT32_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_homekey_pden(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_HOMEKEY_PDEN_MASK),
				    (unsigned int) (PMIC_RG_HOMEKEY_PDEN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_homekey_puen(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_HOMEKEY_PUEN_MASK),
				    (unsigned int) (PMIC_RG_HOMEKEY_PUEN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt50(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT50_MASK), (unsigned int) (PMIC_RG_SMT50_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt49(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT49_MASK), (unsigned int) (PMIC_RG_SMT49_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_smt48(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (GPIO_SMT_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMT48_MASK), (unsigned int) (PMIC_RG_SMT48_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_srclken_peri(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_SRCLKEN_PERI_MASK),
				    (unsigned int) (PMIC_RG_OCTL_SRCLKEN_PERI_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_srcvolten(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_SRCVOLTEN_MASK),
				    (unsigned int) (PMIC_RG_OCTL_SRCVOLTEN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_int(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_INT_MASK), (unsigned int) (PMIC_RG_OCTL_INT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_homekey(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_HOMEKEY_MASK),
				    (unsigned int) (PMIC_RG_OCTL_HOMEKEY_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_spi_clk(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_SPI_CLK_MASK),
				    (unsigned int) (PMIC_RG_OCTL_SPI_CLK_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_wrap_event(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_WRAP_EVENT_MASK),
				    (unsigned int) (PMIC_RG_OCTL_WRAP_EVENT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_rtc_32k1v8(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_RTC_32K1V8_MASK),
				    (unsigned int) (PMIC_RG_OCTL_RTC_32K1V8_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_spi_miso(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_SPI_MISO_MASK),
				    (unsigned int) (PMIC_RG_OCTL_SPI_MISO_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_spi_mosi(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_SPI_MOSI_MASK),
				    (unsigned int) (PMIC_RG_OCTL_SPI_MOSI_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_spi_csn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_SPI_CSN_MASK),
				    (unsigned int) (PMIC_RG_OCTL_SPI_CSN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_aud_mosi(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_AUD_MOSI_MASK),
				    (unsigned int) (PMIC_RG_OCTL_AUD_MOSI_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_aud_miso(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_AUD_MISO_MASK),
				    (unsigned int) (PMIC_RG_OCTL_AUD_MISO_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_aud_clk(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_AUD_ClK_MASK),
				    (unsigned int) (PMIC_RG_OCTL_AUD_ClK_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_col2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_COL2_MASK), (unsigned int) (PMIC_RG_OCTL_COL2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_col1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_COL1_MASK), (unsigned int) (PMIC_RG_OCTL_COL1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_col0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_COL0_MASK), (unsigned int) (PMIC_RG_OCTL_COL0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_col6(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_COL6_MASK), (unsigned int) (PMIC_RG_OCTL_COL6_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_col5(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_COL5_MASK), (unsigned int) (PMIC_RG_OCTL_COL5_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_col4(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_COL4_MASK), (unsigned int) (PMIC_RG_OCTL_COL4_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_col3(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_COL3_MASK), (unsigned int) (PMIC_RG_OCTL_COL3_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_row2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_ROW2_MASK), (unsigned int) (PMIC_RG_OCTL_ROW2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_row1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_ROW1_MASK), (unsigned int) (PMIC_RG_OCTL_ROW1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_row0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_ROW0_MASK), (unsigned int) (PMIC_RG_OCTL_ROW0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_col7(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_COL7_MASK), (unsigned int) (PMIC_RG_OCTL_COL7_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_row6(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_ROW6_MASK), (unsigned int) (PMIC_RG_OCTL_ROW6_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_row5(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_ROW5_MASK), (unsigned int) (PMIC_RG_OCTL_ROW5_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_row4(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_ROW4_MASK), (unsigned int) (PMIC_RG_OCTL_ROW4_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_row3(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_ROW3_MASK), (unsigned int) (PMIC_RG_OCTL_ROW3_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_pwm(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_PWM_MASK), (unsigned int) (PMIC_RG_OCTL_PWM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_vmsel2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_VMSEL2_MASK),
				    (unsigned int) (PMIC_RG_OCTL_VMSEL2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_vmsel1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_VMSEL1_MASK),
				    (unsigned int) (PMIC_RG_OCTL_VMSEL1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_row7(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_ROW7_MASK), (unsigned int) (PMIC_RG_OCTL_ROW7_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_sda1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_SDA1_MASK), (unsigned int) (PMIC_RG_OCTL_SDA1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_scl1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_SCL1_MASK), (unsigned int) (PMIC_RG_OCTL_SCL1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_sda0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_SDA0_MASK), (unsigned int) (PMIC_RG_OCTL_SDA0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_scl0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_SCL0_MASK), (unsigned int) (PMIC_RG_OCTL_SCL0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_sda2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_SDA2_MASK), (unsigned int) (PMIC_RG_OCTL_SDA2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_scl2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_SCL2_MASK), (unsigned int) (PMIC_RG_OCTL_SCL2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_cec(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_CEC_MASK), (unsigned int) (PMIC_RG_OCTL_CEC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_htplg(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_HTPLG_MASK),
				    (unsigned int) (PMIC_RG_OCTL_HTPLG_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_hdmisd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_HDMISD_MASK),
				    (unsigned int) (PMIC_RG_OCTL_HDMISD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_hdmisck(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_HDMISCK_MASK),
				    (unsigned int) (PMIC_RG_OCTL_HDMISCK_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_simls2_srst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_SIMLS2_SRST_MASK),
				    (unsigned int) (PMIC_RG_OCTL_SIMLS2_SRST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_octl_simls2_sclk(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DRV_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCTL_SIMLS2_SCLK_MASK),
				    (unsigned int) (PMIC_RG_OCTL_SIMLS2_SCLK_SHIFT)
	    );
	pmic_unlock();
}

#if 1
static inline void upmu_set_rg_int_en_ov(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_OV_MASK), (unsigned int) (PMIC_RG_INT_EN_OV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_chrdet(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_CHRDET_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_CHRDET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_bvalid_det(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_BVALID_DET_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_BVALID_DET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_vbaton_undet(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_VBATON_UNDET_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_VBATON_UNDET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_thr_h(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_THR_H_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_THR_H_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_thr_l(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_THR_L_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_THR_L_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_pwrkey(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_PWRKEY_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_PWRKEY_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_watchdog(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_WATCHDOG_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_WATCHDOG_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_fg_bat_h(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_FG_BAT_H_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_FG_BAT_H_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_fg_bat_l(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_FG_BAT_L_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_FG_BAT_L_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_bat_h(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_BAT_H_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_BAT_H_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_bat_l(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_BAT_L_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_BAT_L_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_spkr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_SPKR_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_SPKR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_spkl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_SPKL_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_SPKL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_spkr_ab(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_SPKR_AB_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_SPKR_AB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_spkl_ab(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_SPKL_AB_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_SPKL_AB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_vdrm(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_VDRM_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_VDRM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_vsrmca7(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_VSRMCA7_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_VSRMCA7_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_vpca7(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_VPCA7_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_VPCA7_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_vio18(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_VIO18_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_VIO18_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_vgpu(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_VGPU_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_VGPU_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_vcore(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_VCORE_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_VCORE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_vsrmca15(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_VSRMCA15_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_VSRMCA15_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_vca15(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_VCA15_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_VCA15_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_hdmi_cec(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_HDMI_CEC_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_HDMI_CEC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_hdmi_sifm(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_HDMI_SIFM_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_HDMI_SIFM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_pwrkey_rstb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_PWRKEY_RSTB_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_PWRKEY_RSTB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_rtc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_RTC_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_RTC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_audio(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_AUDIO_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_AUDIO_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_accdet(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_ACCDET_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_ACCDET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_homekey(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_HOMEKEY_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_HOMEKEY_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_int_en_ldo(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (INT_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INT_EN_LDO_MASK),
				    (unsigned int) (PMIC_RG_INT_EN_LDO_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rg_int_status_ov(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_OV_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_OV_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_chrdet(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_CHRDET_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_CHRDET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_bvalid_det(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_BVALID_DET_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_BVALID_DET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_vbaton_undet(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_VBATON_UNDET_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_VBATON_UNDET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_thr_h(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_THR_H_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_THR_H_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_thr_l(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_THR_L_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_THR_L_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_pwrkey(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_PWRKEY_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_PWRKEY_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_watchdog(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_WATCHDOG_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_WATCHDOG_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_fg_bat_h(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_FG_BAT_H_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_FG_BAT_H_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_fg_bat_l(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_FG_BAT_L_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_FG_BAT_L_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_bat_h(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_BAT_H_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_BAT_H_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_bat_l(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_BAT_L_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_BAT_L_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_spkr(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_SPKR_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_SPKR_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_spkl(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_SPKL_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_SPKL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_spkr_ab(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_SPKR_AB_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_SPKR_AB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_spkl_ab(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS0),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_SPKL_AB_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_SPKL_AB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_vdrm(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_VDRM_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_VDRM_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_vsrmca7(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_VSRMCA7_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_VSRMCA7_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_vpca7(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_VPCA7_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_VPCA7_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_vio18(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_VIO18_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_VIO18_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_vgpu(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_VGPU_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_VGPU_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_vcore(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_VCORE_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_VCORE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_vsrmca15(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_VSRMCA15_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_VSRMCA15_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_vca15(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_VCA15_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_VCA15_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_hdmi_cec(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_HDMI_CEC_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_HDMI_CEC_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_hdmi_sifm(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_HDMI_SIFM_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_HDMI_SIFM_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_pwrkey_rstb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_PWRKEY_RSTB_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_PWRKEY_RSTB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_rtc(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_RTC_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_RTC_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_audio(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_AUDIO_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_AUDIO_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_accdet(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_ACCDET_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_ACCDET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_homekey(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_HOMEKEY_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_HOMEKEY_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_int_status_ldo(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (INT_STATUS1),
				  (&val),
				  (unsigned int) (PMIC_RG_INT_STATUS_LDO_MASK),
				  (unsigned int) (PMIC_RG_INT_STATUS_LDO_SHIFT)
	    );
	pmic_unlock();

	return val;
}
#endif

static inline void upmu_set_fqmtr_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FQMTR_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FQMTR_EN_MASK), (unsigned int) (PMIC_FQMTR_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fqmtr_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FQMTR_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FQMTR_RST_MASK), (unsigned int) (PMIC_FQMTR_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_fqmtr_busy(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (FQMTR_CON0),
				  (&val),
				  (unsigned int) (PMIC_FQMTR_BUSY_MASK), (unsigned int) (PMIC_FQMTR_BUSY_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_fqmtr_tcksel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FQMTR_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FQMTR_TCKSEL_MASK), (unsigned int) (PMIC_FQMTR_TCKSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fqmtr_winset(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FQMTR_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FQMTR_WINSET_MASK), (unsigned int) (PMIC_FQMTR_WINSET_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_fqmtr_data(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (FQMTR_CON2),
				  (&val),
				  (unsigned int) (PMIC_FQMTR_DATA_MASK), (unsigned int) (PMIC_FQMTR_DATA_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_efuse_addr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_ADDR_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_ADDR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_prog(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_PROG_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_PROG_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_EN_MASK), (unsigned int) (PMIC_RG_EFUSE_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_pkey(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_PKEY_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_PKEY_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_rd_trig(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_RD_TRIG_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_RD_TRIG_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_rd_rdy_bypass(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_RD_RDY_BYPASS_MASK),
				    (unsigned int) (PMIC_RG_RD_RDY_BYPASS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_skip_efuse_out(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SKIP_EFUSE_OUT_MASK),
				    (unsigned int) (PMIC_RG_SKIP_EFUSE_OUT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_prog_src(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_PROG_SRC_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_PROG_SRC_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rg_efuse_busy(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_CON6),
				  (&val),
				  (unsigned int) (PMIC_RG_EFUSE_BUSY_MASK), (unsigned int) (PMIC_RG_EFUSE_BUSY_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_efuse_rd_ack(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_CON6),
				  (&val),
				  (unsigned int) (PMIC_RG_EFUSE_RD_ACK_MASK),
				  (unsigned int) (PMIC_RG_EFUSE_RD_ACK_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_efuse_val_0_15(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_0_15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_0_15_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_0_15_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_val_16_31(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_16_31),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_16_31_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_16_31_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_val_32_47(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_32_47),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_32_47_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_32_47_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_val_48_63(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_48_63),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_48_63_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_48_63_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_val_64_79(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_64_79),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_64_79_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_64_79_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_val_80_95(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_80_95),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_80_95_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_80_95_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_val_96_111(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_96_111),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_96_111_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_96_111_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_val_112_127(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_112_127),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_112_127_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_112_127_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_val_128_143(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_128_143),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_128_143_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_128_143_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_val_144_159(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_144_159),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_144_159_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_144_159_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_val_160_175(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_160_175),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_160_175_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_160_175_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_val_176_191(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_176_191),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_176_191_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_176_191_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_val_192_207(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_192_207),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_192_207_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_192_207_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_val_208_223(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_208_223),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_208_223_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_208_223_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_val_224_239(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_224_239),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_224_239_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_224_239_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_val_240_255(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_240_255),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_240_255_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_240_255_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_val_256_271(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_256_271),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_256_271_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_256_271_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_val_272_287(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_272_287),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_272_287_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_272_287_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_val_288_303(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_288_303),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_288_303_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_288_303_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_efuse_val_304_319(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (EFUSE_VAL_304_319),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_304_319_MASK),
				    (unsigned int) (PMIC_RG_EFUSE_VAL_304_319_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rg_efuse_dout_0_15(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_0_15),
				  (&val),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_0_15_MASK),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_0_15_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_efuse_dout_16_31(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_16_31),
				  (&val),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_16_31_MASK),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_16_31_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_efuse_dout_32_47(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_32_47),
				  (&val),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_32_47_MASK),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_32_47_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_efuse_dout_48_63(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_48_63),
				  (&val),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_48_63_MASK),
				  (unsigned int) (PMIC_RG_EFUSE_DOUT_48_63_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_spi_con(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPI_CON),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPI_CON_MASK), (unsigned int) (PMIC_SPI_CON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auxadc_div_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUXADC_DIV_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_AUXADC_DIV_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_rtc_75k_div4_ck_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKPDN3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_RTC_75K_DIV4_CK_PDN_MASK),
				    (unsigned int) (PMIC_RG_RTC_75K_DIV4_CK_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_wrap_event_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_WRAP_EVENT_EN_MASK),
				    (unsigned int) (PMIC_RG_WRAP_EVENT_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_wrap_en_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_WRAP_EN_SEL_MASK),
				    (unsigned int) (PMIC_RG_WRAP_EN_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_SEL_MASK), (unsigned int) (PMIC_RG_DCXO_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_eint_ck_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EINT_CK_SEL_MASK),
				    (unsigned int) (PMIC_RG_EINT_CK_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_aud_ck_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (TOP_CKCON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUD_CK_SEL_MASK),
				    (unsigned int) (PMIC_RG_AUD_CK_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_efuse_dout_64_79(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_64_79),
				  (&val),
				  (unsigned int) (PMIC_EFUSE_DOUT_64_79_MASK),
				  (unsigned int) (PMIC_EFUSE_DOUT_64_79_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_efuse_dout_80_95(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_80_95),
				  (&val),
				  (unsigned int) (PMIC_EFUSE_DOUT_80_95_MASK),
				  (unsigned int) (PMIC_EFUSE_DOUT_80_95_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_efuse_dout_96_111(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_96_111),
				  (&val),
				  (unsigned int) (PMIC_EFUSE_DOUT_96_111_MASK),
				  (unsigned int) (PMIC_EFUSE_DOUT_96_111_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_efuse_dout_112_127(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_112_127),
				  (&val),
				  (unsigned int) (PMIC_EFUSE_DOUT_112_127_MASK),
				  (unsigned int) (PMIC_EFUSE_DOUT_112_127_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_efuse_dout_128_143(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_128_143),
				  (&val),
				  (unsigned int) (PMIC_EFUSE_DOUT_128_143_MASK),
				  (unsigned int) (PMIC_EFUSE_DOUT_128_143_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_efuse_dout_144_159(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_144_159),
				  (&val),
				  (unsigned int) (PMIC_EFUSE_DOUT_144_159_MASK),
				  (unsigned int) (PMIC_EFUSE_DOUT_144_159_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_efuse_dout_160_175(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_160_175),
				  (&val),
				  (unsigned int) (PMIC_EFUSE_DOUT_160_175_MASK),
				  (unsigned int) (PMIC_EFUSE_DOUT_160_175_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_efuse_dout_176_191(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_176_191),
				  (&val),
				  (unsigned int) (PMIC_EFUSE_DOUT_176_191_MASK),
				  (unsigned int) (PMIC_EFUSE_DOUT_176_191_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_efuse_dout_192_207(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_192_207),
				  (&val),
				  (unsigned int) (PMIC_EFUSE_DOUT_192_207_MASK),
				  (unsigned int) (PMIC_EFUSE_DOUT_192_207_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_efuse_dout_208_223(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_208_223),
				  (&val),
				  (unsigned int) (PMIC_EFUSE_DOUT_208_223_MASK),
				  (unsigned int) (PMIC_EFUSE_DOUT_208_223_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_efuse_dout_224_239(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_224_239),
				  (&val),
				  (unsigned int) (PMIC_EFUSE_DOUT_224_239_MASK),
				  (unsigned int) (PMIC_EFUSE_DOUT_224_239_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_efuse_dout_240_255(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_240_255),
				  (&val),
				  (unsigned int) (PMIC_EFUSE_DOUT_240_255_MASK),
				  (unsigned int) (PMIC_EFUSE_DOUT_240_255_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_efuse_dout_256_271(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_256_271),
				  (&val),
				  (unsigned int) (PMIC_EFUSE_DOUT_256_271_MASK),
				  (unsigned int) (PMIC_EFUSE_DOUT_256_271_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_efuse_dout_272_287(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_272_287),
				  (&val),
				  (unsigned int) (PMIC_EFUSE_DOUT_272_287_MASK),
				  (unsigned int) (PMIC_EFUSE_DOUT_272_287_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_efuse_dout_288_303(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_288_303),
				  (&val),
				  (unsigned int) (PMIC_EFUSE_DOUT_288_303_MASK),
				  (unsigned int) (PMIC_EFUSE_DOUT_288_303_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_efuse_dout_304_319(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (EFUSE_DOUT_304_319),
				  (&val),
				  (unsigned int) (PMIC_EFUSE_DOUT_304_319_MASK),
				  (unsigned int) (PMIC_EFUSE_DOUT_304_319_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_smps_testmode_b(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SMPS_TESTMODE_B_MASK),
				    (unsigned int) (PMIC_RG_SMPS_TESTMODE_B_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vsrmca7_dig_mon(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (BUCK_CON1),
				  (&val),
				  (unsigned int) (PMIC_QI_VSRMCA7_DIG_MON_MASK),
				  (unsigned int) (PMIC_QI_VSRMCA7_DIG_MON_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vpca7_dig_mon(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (BUCK_CON1),
				  (&val),
				  (unsigned int) (PMIC_QI_VPCA7_DIG_MON_MASK),
				  (unsigned int) (PMIC_QI_VPCA7_DIG_MON_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vsrmca15_dig_mon(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (BUCK_CON1),
				  (&val),
				  (unsigned int) (PMIC_QI_VSRMCA15_DIG_MON_MASK),
				  (unsigned int) (PMIC_QI_VSRMCA15_DIG_MON_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vca15_dig_mon(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (BUCK_CON1),
				  (&val),
				  (unsigned int) (PMIC_QI_VCA15_DIG_MON_MASK),
				  (unsigned int) (PMIC_QI_VCA15_DIG_MON_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vcore_dig_mon(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (BUCK_CON2),
				  (&val),
				  (unsigned int) (PMIC_QI_VCORE_DIG_MON_MASK),
				  (unsigned int) (PMIC_QI_VCORE_DIG_MON_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vdrm_dig_mon(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (BUCK_CON2),
				  (&val),
				  (unsigned int) (PMIC_QI_VDRM_DIG_MON_MASK),
				  (unsigned int) (PMIC_QI_VDRM_DIG_MON_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vgpu_dig_mon(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (BUCK_CON2),
				  (&val),
				  (unsigned int) (PMIC_QI_VGPU_DIG_MON_MASK),
				  (unsigned int) (PMIC_QI_VGPU_DIG_MON_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_buck_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_BUCK_RSV_MASK), (unsigned int) (PMIC_BUCK_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vpca7_trimh(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPCA7_TRIMH_MASK),
				    (unsigned int) (PMIC_RG_VPCA7_TRIMH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vpca7_triml(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPCA7_TRIML_MASK),
				    (unsigned int) (PMIC_RG_VPCA7_TRIML_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vca15_trimh(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCA15_TRIMH_MASK),
				    (unsigned int) (PMIC_RG_VCA15_TRIMH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vca15_triml(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCA15_TRIML_MASK),
				    (unsigned int) (PMIC_RG_VCA15_TRIML_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsrmca7_trimh(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSRMCA7_TRIMH_MASK),
				    (unsigned int) (PMIC_RG_VSRMCA7_TRIMH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsrmca7_triml(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSRMCA7_TRIML_MASK),
				    (unsigned int) (PMIC_RG_VSRMCA7_TRIML_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsrmca15_trimh(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSRMCA15_TRIMH_MASK),
				    (unsigned int) (PMIC_RG_VSRMCA15_TRIMH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsrmca15_triml(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSRMCA15_TRIML_MASK),
				    (unsigned int) (PMIC_RG_VSRMCA15_TRIML_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcore_trimh(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCORE_TRIMH_MASK),
				    (unsigned int) (PMIC_RG_VCORE_TRIMH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcore_triml(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCORE_TRIML_MASK),
				    (unsigned int) (PMIC_RG_VCORE_TRIML_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgpu_trimh(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGPU_TRIMH_MASK),
				    (unsigned int) (PMIC_RG_VGPU_TRIMH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgpu_triml(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGPU_TRIML_MASK),
				    (unsigned int) (PMIC_RG_VGPU_TRIML_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vdrm_trimh(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VDRM_TRIMH_MASK),
				    (unsigned int) (PMIC_RG_VDRM_TRIMH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vdrm_triml(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VDRM_TRIML_MASK),
				    (unsigned int) (PMIC_RG_VDRM_TRIML_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_qi_vcore_vsleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_QI_VCORE_VSLEEP_MASK),
				    (unsigned int) (PMIC_QI_VCORE_VSLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_qi_vgpu_vsleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_QI_VGPU_VSLEEP_MASK),
				    (unsigned int) (PMIC_QI_VGPU_VSLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_qi_vsrmca7_vsleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_QI_VSRMCA7_VSLEEP_MASK),
				    (unsigned int) (PMIC_QI_VSRMCA7_VSLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_qi_vsrmca15_vsleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_QI_VSRMCA15_VSLEEP_MASK),
				    (unsigned int) (PMIC_QI_VSRMCA15_VSLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_qi_vpca7_vsleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_QI_VPCA7_VSLEEP_MASK),
				    (unsigned int) (PMIC_QI_VPCA7_VSLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_qi_vca15_vsleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_QI_VCA15_VSLEEP_MASK),
				    (unsigned int) (PMIC_QI_VCA15_VSLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_qi_vdrm_vsleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_QI_VDRM_VSLEEP_MASK),
				    (unsigned int) (PMIC_QI_VDRM_VSLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vca15_zxos_trim2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCA15_ZXOS_TRIM2_MASK),
				    (unsigned int) (PMIC_RG_VCA15_ZXOS_TRIM2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vca15_zxos_trim1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCA15_ZXOS_TRIM1_MASK),
				    (unsigned int) (PMIC_RG_VCA15_ZXOS_TRIM1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vca15_csl2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCA15_CSL2_MASK),
				    (unsigned int) (PMIC_RG_VCA15_CSL2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vca15_csl1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCA15_CSL1_MASK),
				    (unsigned int) (PMIC_RG_VCA15_CSL1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vca15_csr2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCA15_CSR2_MASK),
				    (unsigned int) (PMIC_RG_VCA15_CSR2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vca15_csr1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCA15_CSR1_MASK),
				    (unsigned int) (PMIC_RG_VCA15_CSR1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vca15_cc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCA15_CC_MASK), (unsigned int) (PMIC_RG_VCA15_CC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vca15_rzsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCA15_RZSEL_MASK),
				    (unsigned int) (PMIC_RG_VCA15_RZSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vca15_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCA15_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VCA15_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vca15_modeset(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCA15_MODESET_MASK),
				    (unsigned int) (PMIC_RG_VCA15_MODESET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vca15_acgb_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCA15_ACGB_EN_MASK),
				    (unsigned int) (PMIC_RG_VCA15_ACGB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vca15_slp2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCA15_SLP2_MASK),
				    (unsigned int) (PMIC_RG_VCA15_SLP2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vca15_slp1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCA15_SLP1_MASK),
				    (unsigned int) (PMIC_RG_VCA15_SLP1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vca15_zx_os2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCA15_ZX_OS2_MASK),
				    (unsigned int) (PMIC_RG_VCA15_ZX_OS2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vca15_zx_os1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCA15_ZX_OS1_MASK),
				    (unsigned int) (PMIC_RG_VCA15_ZX_OS1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vca15_csm2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCA15_CSM2_MASK),
				    (unsigned int) (PMIC_RG_VCA15_CSM2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vca15_csm1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCA15_CSM1_MASK),
				    (unsigned int) (PMIC_RG_VCA15_CSM1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vca15_rsv1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCA15_RSV1_MASK),
				    (unsigned int) (PMIC_RG_VCA15_RSV1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vca15_track_on_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_TRACK_ON_CTRL_MASK),
				    (unsigned int) (PMIC_VCA15_TRACK_ON_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vca15_burst_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_BURST_CTRL_MASK),
				    (unsigned int) (PMIC_VCA15_BURST_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vca15_dlc_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_DLC_CTRL_MASK),
				    (unsigned int) (PMIC_VCA15_DLC_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vca15_vosel_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_VOSEL_CTRL_MASK),
				    (unsigned int) (PMIC_VCA15_VOSEL_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vca15_en_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_EN_CTRL_MASK),
				    (unsigned int) (PMIC_VCA15_EN_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vca15_burst_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_BURST_SEL_MASK),
				    (unsigned int) (PMIC_VCA15_BURST_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vca15_dlc_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_DLC_SEL_MASK),
				    (unsigned int) (PMIC_VCA15_DLC_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vca15_vosel_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_VOSEL_SEL_MASK),
				    (unsigned int) (PMIC_VCA15_VOSEL_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vca15_en_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_EN_SEL_MASK), (unsigned int) (PMIC_VCA15_EN_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vca15_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VCA15_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VCA15_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VCA15_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vca15_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VCA15_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VCA15_MODE_MASK), (unsigned int) (PMIC_QI_VCA15_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vca15_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VCA15_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VCA15_EN_MASK), (unsigned int) (PMIC_QI_VCA15_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vca15_stb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VCA15_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VCA15_STB_MASK), (unsigned int) (PMIC_QI_VCA15_STB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vca15_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_STBTD_MASK), (unsigned int) (PMIC_VCA15_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vca15_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_smp_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_EN_MASK), (unsigned int) (PMIC_VCA15_EN_SHIFT)
	    );
	pmic_smp_unlock();
}

static inline void upmu_set_vca15_en_nolock(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int) (VCA15_CON7),
					   (unsigned int) (val),
					   (unsigned int) (PMIC_VCA15_EN_MASK), (unsigned int) (PMIC_VCA15_EN_SHIFT)
	    );
}

static inline void upmu_set_vca15_sfchg_ren(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_SFCHG_REN_MASK),
				    (unsigned int) (PMIC_VCA15_SFCHG_REN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vca15_sfchg_rrate(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_SFCHG_RRATE_MASK),
				    (unsigned int) (PMIC_VCA15_SFCHG_RRATE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vca15_sfchg_fen(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_SFCHG_FEN_MASK),
				    (unsigned int) (PMIC_VCA15_SFCHG_FEN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vca15_sfchg_frate(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_SFCHG_FRATE_MASK),
				    (unsigned int) (PMIC_VCA15_SFCHG_FRATE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vca15_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_VOSEL_MASK), (unsigned int) (PMIC_VCA15_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vca15_vosel_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_VOSEL_ON_MASK),
				    (unsigned int) (PMIC_VCA15_VOSEL_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vca15_vosel_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_VOSEL_SLEEP_MASK),
				    (unsigned int) (PMIC_VCA15_VOSEL_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_ni_vca15_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VCA15_CON12),
				  (&val),
				  (unsigned int) (PMIC_NI_VCA15_VOSEL_MASK),
				  (unsigned int) (PMIC_NI_VCA15_VOSEL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vca15_burst(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VCA15_CON13),
				  (&val),
				  (unsigned int) (PMIC_QI_VCA15_BURST_MASK),
				  (unsigned int) (PMIC_QI_VCA15_BURST_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vca15_burst_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_BURST_SLEEP_MASK),
				    (unsigned int) (PMIC_VCA15_BURST_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vca15_burst_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_BURST_ON_MASK),
				    (unsigned int) (PMIC_VCA15_BURST_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vca15_burst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_BURST_MASK), (unsigned int) (PMIC_VCA15_BURST_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vca15_dlc(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VCA15_CON14),
				  (&val),
				  (unsigned int) (PMIC_QI_VCA15_DLC_MASK), (unsigned int) (PMIC_QI_VCA15_DLC_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vca15_dlc_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_DLC_SLEEP_MASK),
				    (unsigned int) (PMIC_VCA15_DLC_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vca15_dlc_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_DLC_ON_MASK), (unsigned int) (PMIC_VCA15_DLC_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vca15_dlc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_DLC_MASK), (unsigned int) (PMIC_VCA15_DLC_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vca15_dlc_n(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VCA15_CON15),
				  (&val),
				  (unsigned int) (PMIC_QI_VCA15_DLC_N_MASK),
				  (unsigned int) (PMIC_QI_VCA15_DLC_N_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vca15_dlc_n_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_DLC_N_SLEEP_MASK),
				    (unsigned int) (PMIC_VCA15_DLC_N_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vca15_dlc_n_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_DLC_N_ON_MASK),
				    (unsigned int) (PMIC_VCA15_DLC_N_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vca15_dlc_n(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_DLC_N_MASK), (unsigned int) (PMIC_VCA15_DLC_N_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vca15_bursth(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VCA15_CON16),
				  (&val),
				  (unsigned int) (PMIC_QI_VCA15_BURSTH_MASK),
				  (unsigned int) (PMIC_QI_VCA15_BURSTH_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vca15_burstl(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VCA15_CON17),
				  (&val),
				  (unsigned int) (PMIC_QI_VCA15_BURSTL_MASK),
				  (unsigned int) (PMIC_QI_VCA15_BURSTL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_ni_vca15_vsleep_sel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VCA15_CON18),
				  (&val),
				  (unsigned int) (PMIC_NI_VCA15_VSLEEP_SEL_MASK),
				  (unsigned int) (PMIC_NI_VCA15_VSLEEP_SEL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_ni_vca15_r2r_pdn(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VCA15_CON18),
				  (&val),
				  (unsigned int) (PMIC_NI_VCA15_R2R_PDN_MASK),
				  (unsigned int) (PMIC_NI_VCA15_R2R_PDN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vca15_vsleep_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_VSLEEP_SEL_MASK),
				    (unsigned int) (PMIC_VCA15_VSLEEP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vca15_r2r_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_R2R_PDN_MASK),
				    (unsigned int) (PMIC_VCA15_R2R_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vca15_vsleep_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_VSLEEP_EN_MASK),
				    (unsigned int) (PMIC_VCA15_VSLEEP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_ni_vca15_vosel_trans(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VCA15_CON18),
				  (&val),
				  (unsigned int) (PMIC_NI_VCA15_VOSEL_TRANS_MASK),
				  (unsigned int) (PMIC_NI_VCA15_VOSEL_TRANS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vca15_vosel_trans_once(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_VOSEL_TRANS_ONCE_MASK),
				    (unsigned int) (PMIC_VCA15_VOSEL_TRANS_ONCE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vca15_vosel_trans_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_VOSEL_TRANS_EN_MASK),
				    (unsigned int) (PMIC_VCA15_VOSEL_TRANS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vca15_transtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCA15_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCA15_TRANSTD_MASK),
				    (unsigned int) (PMIC_VCA15_TRANSTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsrmca15_zxos_trim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSRMCA15_ZXOS_TRIM_MASK),
				    (unsigned int) (PMIC_RG_VSRMCA15_ZXOS_TRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsrmca15_zx_os(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSRMCA15_ZX_OS_MASK),
				    (unsigned int) (PMIC_RG_VSRMCA15_ZX_OS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsrmca15_csl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSRMCA15_CSL_MASK),
				    (unsigned int) (PMIC_RG_VSRMCA15_CSL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsrmca15_csr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSRMCA15_CSR_MASK),
				    (unsigned int) (PMIC_RG_VSRMCA15_CSR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsrmca15_cc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSRMCA15_CC_MASK),
				    (unsigned int) (PMIC_RG_VSRMCA15_CC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsrmca15_rzsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSRMCA15_RZSEL_MASK),
				    (unsigned int) (PMIC_RG_VSRMCA15_RZSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsrmca15_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSRMCA15_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VSRMCA15_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsrmca15_modeset(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSRMCA15_MODESET_MASK),
				    (unsigned int) (PMIC_RG_VSRMCA15_MODESET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsrmca15_csm(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSRMCA15_CSM_MASK),
				    (unsigned int) (PMIC_RG_VSRMCA15_CSM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsrmca15_smrip_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSRMCA15_SMRIP_EN_MASK),
				    (unsigned int) (PMIC_RG_VSRMCA15_SMRIP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vsrmca15_slpo_out(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSRMCA15_CON3),
				  (&val),
				  (unsigned int) (PMIC_QI_VSRMCA15_SLPO_OUT_MASK),
				  (unsigned int) (PMIC_QI_VSRMCA15_SLPO_OUT_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vsrmca15_slp(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSRMCA15_CON3),
				  (&val),
				  (unsigned int) (PMIC_QI_VSRMCA15_SLP_MASK),
				  (unsigned int) (PMIC_QI_VSRMCA15_SLP_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vsrmca15_sawcal_td(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_SAWCAL_TD_MASK),
				    (unsigned int) (PMIC_VSRMCA15_SAWCAL_TD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_slp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_SLP_MASK), (unsigned int) (PMIC_VSRMCA15_SLP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsrmca15_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSRMCA15_RSV_MASK),
				    (unsigned int) (PMIC_RG_VSRMCA15_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_track_sleep_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_TRACK_SLEEP_CTRL_MASK),
				    (unsigned int) (PMIC_VSRMCA15_TRACK_SLEEP_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_track_on_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_TRACK_ON_CTRL_MASK),
				    (unsigned int) (PMIC_VSRMCA15_TRACK_ON_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_burst_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_BURST_CTRL_MASK),
				    (unsigned int) (PMIC_VSRMCA15_BURST_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_dlc_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_DLC_CTRL_MASK),
				    (unsigned int) (PMIC_VSRMCA15_DLC_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_vosel_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_VOSEL_CTRL_MASK),
				    (unsigned int) (PMIC_VSRMCA15_VOSEL_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_en_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_EN_CTRL_MASK),
				    (unsigned int) (PMIC_VSRMCA15_EN_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_burst_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_BURST_SEL_MASK),
				    (unsigned int) (PMIC_VSRMCA15_BURST_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_dlc_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_DLC_SEL_MASK),
				    (unsigned int) (PMIC_VSRMCA15_DLC_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_vosel_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_VOSEL_SEL_MASK),
				    (unsigned int) (PMIC_VSRMCA15_VOSEL_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_en_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_EN_SEL_MASK),
				    (unsigned int) (PMIC_VSRMCA15_EN_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vsrmca15_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSRMCA15_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VSRMCA15_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VSRMCA15_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vsrmca15_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSRMCA15_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VSRMCA15_MODE_MASK),
				  (unsigned int) (PMIC_QI_VSRMCA15_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vsrmca15_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSRMCA15_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VSRMCA15_EN_MASK),
				  (unsigned int) (PMIC_QI_VSRMCA15_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vsrmca15_stb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSRMCA15_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VSRMCA15_STB_MASK),
				  (unsigned int) (PMIC_QI_VSRMCA15_STB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vsrmca15_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_STBTD_MASK),
				    (unsigned int) (PMIC_VSRMCA15_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_smp_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_EN_MASK), (unsigned int) (PMIC_VSRMCA15_EN_SHIFT)
	    );
	pmic_smp_unlock();
}

static inline void upmu_set_vsrmca15_en_nolock(unsigned int val)
{
	unsigned int ret = 0;

	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON7),
					   (unsigned int) (val),
					   (unsigned int) (PMIC_VSRMCA15_EN_MASK),
					   (unsigned int) (PMIC_VSRMCA15_EN_SHIFT)
	    );
}

static inline void upmu_set_vsrmca15_sfchg_ren(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_SFCHG_REN_MASK),
				    (unsigned int) (PMIC_VSRMCA15_SFCHG_REN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_sfchg_rrate(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_SFCHG_RRATE_MASK),
				    (unsigned int) (PMIC_VSRMCA15_SFCHG_RRATE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_sfchg_fen(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_SFCHG_FEN_MASK),
				    (unsigned int) (PMIC_VSRMCA15_SFCHG_FEN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_sfchg_frate(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_SFCHG_FRATE_MASK),
				    (unsigned int) (PMIC_VSRMCA15_SFCHG_FRATE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_VOSEL_MASK),
				    (unsigned int) (PMIC_VSRMCA15_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_vosel_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_VOSEL_ON_MASK),
				    (unsigned int) (PMIC_VSRMCA15_VOSEL_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_vosel_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_VOSEL_SLEEP_MASK),
				    (unsigned int) (PMIC_VSRMCA15_VOSEL_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_ni_vsrmca15_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSRMCA15_CON12),
				  (&val),
				  (unsigned int) (PMIC_NI_VSRMCA15_VOSEL_MASK),
				  (unsigned int) (PMIC_NI_VSRMCA15_VOSEL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vsrmca15_burst(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSRMCA15_CON13),
				  (&val),
				  (unsigned int) (PMIC_QI_VSRMCA15_BURST_MASK),
				  (unsigned int) (PMIC_QI_VSRMCA15_BURST_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vsrmca15_burst_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_BURST_SLEEP_MASK),
				    (unsigned int) (PMIC_VSRMCA15_BURST_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_burst_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_BURST_ON_MASK),
				    (unsigned int) (PMIC_VSRMCA15_BURST_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_burst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_BURST_MASK),
				    (unsigned int) (PMIC_VSRMCA15_BURST_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vsrmca15_dlc(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSRMCA15_CON14),
				  (&val),
				  (unsigned int) (PMIC_QI_VSRMCA15_DLC_MASK),
				  (unsigned int) (PMIC_QI_VSRMCA15_DLC_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vsrmca15_dlc_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_DLC_SLEEP_MASK),
				    (unsigned int) (PMIC_VSRMCA15_DLC_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_dlc_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_DLC_ON_MASK),
				    (unsigned int) (PMIC_VSRMCA15_DLC_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_dlc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_DLC_MASK), (unsigned int) (PMIC_VSRMCA15_DLC_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vsrmca15_dlc_n(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSRMCA15_CON15),
				  (&val),
				  (unsigned int) (PMIC_QI_VSRMCA15_DLC_N_MASK),
				  (unsigned int) (PMIC_QI_VSRMCA15_DLC_N_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vsrmca15_dlc_n_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_DLC_N_SLEEP_MASK),
				    (unsigned int) (PMIC_VSRMCA15_DLC_N_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_dlc_n_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_DLC_N_ON_MASK),
				    (unsigned int) (PMIC_VSRMCA15_DLC_N_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_dlc_n(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_DLC_N_MASK),
				    (unsigned int) (PMIC_VSRMCA15_DLC_N_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vsrmca15_bursth(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSRMCA15_CON16),
				  (&val),
				  (unsigned int) (PMIC_QI_VSRMCA15_BURSTH_MASK),
				  (unsigned int) (PMIC_QI_VSRMCA15_BURSTH_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vsrmca15_burstl(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSRMCA15_CON17),
				  (&val),
				  (unsigned int) (PMIC_QI_VSRMCA15_BURSTL_MASK),
				  (unsigned int) (PMIC_QI_VSRMCA15_BURSTL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_ni_vsrmca15_vsleep_sel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSRMCA15_CON18),
				  (&val),
				  (unsigned int) (PMIC_NI_VSRMCA15_VSLEEP_SEL_MASK),
				  (unsigned int) (PMIC_NI_VSRMCA15_VSLEEP_SEL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_ni_vsrmca15_r2r_pdn(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSRMCA15_CON18),
				  (&val),
				  (unsigned int) (PMIC_NI_VSRMCA15_R2R_PDN_MASK),
				  (unsigned int) (PMIC_NI_VSRMCA15_R2R_PDN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vsrmca15_vsleep_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_VSLEEP_SEL_MASK),
				    (unsigned int) (PMIC_VSRMCA15_VSLEEP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_r2r_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_R2R_PDN_MASK),
				    (unsigned int) (PMIC_VSRMCA15_R2R_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_vsleep_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_VSLEEP_EN_MASK),
				    (unsigned int) (PMIC_VSRMCA15_VSLEEP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_ni_vsrmca15_vosel_trans(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSRMCA15_CON18),
				  (&val),
				  (unsigned int) (PMIC_NI_VSRMCA15_VOSEL_TRANS_MASK),
				  (unsigned int) (PMIC_NI_VSRMCA15_VOSEL_TRANS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vsrmca15_vosel_trans_once(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_VOSEL_TRANS_ONCE_MASK),
				    (unsigned int) (PMIC_VSRMCA15_VOSEL_TRANS_ONCE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_vosel_trans_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_VOSEL_TRANS_EN_MASK),
				    (unsigned int) (PMIC_VSRMCA15_VOSEL_TRANS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_transtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_TRANSTD_MASK),
				    (unsigned int) (PMIC_VSRMCA15_TRANSTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_vosel_offset(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_VOSEL_OFFSET_MASK),
				    (unsigned int) (PMIC_VSRMCA15_VOSEL_OFFSET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_vosel_delta(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_VOSEL_DELTA_MASK),
				    (unsigned int) (PMIC_VSRMCA15_VOSEL_DELTA_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_vosel_on_hb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON20),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_VOSEL_ON_HB_MASK),
				    (unsigned int) (PMIC_VSRMCA15_VOSEL_ON_HB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_vosel_on_lb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON20),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_VOSEL_ON_LB_MASK),
				    (unsigned int) (PMIC_VSRMCA15_VOSEL_ON_LB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_vosel_sleep_lb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA15_CON21),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_VOSEL_SLEEP_LB_MASK),
				    (unsigned int) (PMIC_VSRMCA15_VOSEL_SLEEP_LB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcore_zxos_trim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCORE_ZXOS_TRIM_MASK),
				    (unsigned int) (PMIC_RG_VCORE_ZXOS_TRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcore_zx_os(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCORE_ZX_OS_MASK),
				    (unsigned int) (PMIC_RG_VCORE_ZX_OS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcore_csl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCORE_CSL_MASK), (unsigned int) (PMIC_RG_VCORE_CSL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcore_csr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCORE_CSR_MASK), (unsigned int) (PMIC_RG_VCORE_CSR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcore_cc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCORE_CC_MASK), (unsigned int) (PMIC_RG_VCORE_CC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcore_rzsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCORE_RZSEL_MASK),
				    (unsigned int) (PMIC_RG_VCORE_RZSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcore_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCORE_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VCORE_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcore_modeset(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCORE_MODESET_MASK),
				    (unsigned int) (PMIC_RG_VCORE_MODESET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcore_csm(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCORE_CSM_MASK), (unsigned int) (PMIC_RG_VCORE_CSM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcore_avp_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCORE_AVP_EN_MASK),
				    (unsigned int) (PMIC_RG_VCORE_AVP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcore_avp_os(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCORE_AVP_OS_MASK),
				    (unsigned int) (PMIC_RG_VCORE_AVP_OS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcore_slp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCORE_SLP_MASK), (unsigned int) (PMIC_RG_VCORE_SLP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcore_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCORE_RSV_MASK), (unsigned int) (PMIC_RG_VCORE_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcore_burst_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_BURST_CTRL_MASK),
				    (unsigned int) (PMIC_VCORE_BURST_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcore_dlc_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_DLC_CTRL_MASK),
				    (unsigned int) (PMIC_VCORE_DLC_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcore_vosel_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_VOSEL_CTRL_MASK),
				    (unsigned int) (PMIC_VCORE_VOSEL_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcore_en_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_EN_CTRL_MASK),
				    (unsigned int) (PMIC_VCORE_EN_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcore_burst_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_BURST_SEL_MASK),
				    (unsigned int) (PMIC_VCORE_BURST_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcore_dlc_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_DLC_SEL_MASK),
				    (unsigned int) (PMIC_VCORE_DLC_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcore_vosel_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_VOSEL_SEL_MASK),
				    (unsigned int) (PMIC_VCORE_VOSEL_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcore_en_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_EN_SEL_MASK), (unsigned int) (PMIC_VCORE_EN_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vcore_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VCORE_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VCORE_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VCORE_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vcore_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VCORE_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VCORE_MODE_MASK), (unsigned int) (PMIC_QI_VCORE_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vcore_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VCORE_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VCORE_EN_MASK), (unsigned int) (PMIC_QI_VCORE_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vcore_stb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VCORE_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VCORE_STB_MASK), (unsigned int) (PMIC_QI_VCORE_STB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vcore_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_EN_MASK), (unsigned int) (PMIC_VCORE_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcore_sfchg_ren(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_SFCHG_REN_MASK),
				    (unsigned int) (PMIC_VCORE_SFCHG_REN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcore_sfchg_rrate(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_SFCHG_RRATE_MASK),
				    (unsigned int) (PMIC_VCORE_SFCHG_RRATE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcore_sfchg_fen(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_SFCHG_FEN_MASK),
				    (unsigned int) (PMIC_VCORE_SFCHG_FEN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcore_sfchg_frate(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_SFCHG_FRATE_MASK),
				    (unsigned int) (PMIC_VCORE_SFCHG_FRATE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcore_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_VOSEL_MASK), (unsigned int) (PMIC_VCORE_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcore_vosel_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_VOSEL_ON_MASK),
				    (unsigned int) (PMIC_VCORE_VOSEL_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcore_vosel_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_VOSEL_SLEEP_MASK),
				    (unsigned int) (PMIC_VCORE_VOSEL_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_ni_vcore_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VCORE_CON12),
				  (&val),
				  (unsigned int) (PMIC_NI_VCORE_VOSEL_MASK),
				  (unsigned int) (PMIC_NI_VCORE_VOSEL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vcore_burst(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VCORE_CON13),
				  (&val),
				  (unsigned int) (PMIC_QI_VCORE_BURST_MASK),
				  (unsigned int) (PMIC_QI_VCORE_BURST_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vcore_burst_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_BURST_SLEEP_MASK),
				    (unsigned int) (PMIC_VCORE_BURST_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcore_burst_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_BURST_ON_MASK),
				    (unsigned int) (PMIC_VCORE_BURST_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcore_burst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_BURST_MASK), (unsigned int) (PMIC_VCORE_BURST_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vcore_dlc(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VCORE_CON14),
				  (&val),
				  (unsigned int) (PMIC_QI_VCORE_DLC_MASK), (unsigned int) (PMIC_QI_VCORE_DLC_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vcore_dlc_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_DLC_SLEEP_MASK),
				    (unsigned int) (PMIC_VCORE_DLC_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcore_dlc_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_DLC_ON_MASK), (unsigned int) (PMIC_VCORE_DLC_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcore_dlc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_DLC_MASK), (unsigned int) (PMIC_VCORE_DLC_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vcore_dlc_n(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VCORE_CON15),
				  (&val),
				  (unsigned int) (PMIC_QI_VCORE_DLC_N_MASK),
				  (unsigned int) (PMIC_QI_VCORE_DLC_N_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vcore_dlc_n_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_DLC_N_SLEEP_MASK),
				    (unsigned int) (PMIC_VCORE_DLC_N_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcore_dlc_n_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_DLC_N_ON_MASK),
				    (unsigned int) (PMIC_VCORE_DLC_N_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcore_dlc_n(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_DLC_N_MASK), (unsigned int) (PMIC_VCORE_DLC_N_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vcore_bursth(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VCORE_CON16),
				  (&val),
				  (unsigned int) (PMIC_QI_VCORE_BURSTH_MASK),
				  (unsigned int) (PMIC_QI_VCORE_BURSTH_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vcore_burstl(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VCORE_CON17),
				  (&val),
				  (unsigned int) (PMIC_QI_VCORE_BURSTL_MASK),
				  (unsigned int) (PMIC_QI_VCORE_BURSTL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_ni_vcore_vsleep_sel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VCORE_CON18),
				  (&val),
				  (unsigned int) (PMIC_NI_VCORE_VSLEEP_SEL_MASK),
				  (unsigned int) (PMIC_NI_VCORE_VSLEEP_SEL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_ni_vcore_r2r_pdn(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VCORE_CON18),
				  (&val),
				  (unsigned int) (PMIC_NI_VCORE_R2R_PDN_MASK),
				  (unsigned int) (PMIC_NI_VCORE_R2R_PDN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vcore_vsleep_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_VSLEEP_SEL_MASK),
				    (unsigned int) (PMIC_VCORE_VSLEEP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcore_r2r_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_R2R_PDN_MASK),
				    (unsigned int) (PMIC_VCORE_R2R_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcore_vsleep_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_VSLEEP_EN_MASK),
				    (unsigned int) (PMIC_VCORE_VSLEEP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_ni_vcore_vosel_trans(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VCORE_CON18),
				  (&val),
				  (unsigned int) (PMIC_NI_VCORE_VOSEL_TRANS_MASK),
				  (unsigned int) (PMIC_NI_VCORE_VOSEL_TRANS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vcore_vosel_trans_once(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_VOSEL_TRANS_ONCE_MASK),
				    (unsigned int) (PMIC_VCORE_VOSEL_TRANS_ONCE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcore_vosel_trans_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_VOSEL_TRANS_EN_MASK),
				    (unsigned int) (PMIC_VCORE_VOSEL_TRANS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcore_transtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VCORE_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_TRANSTD_MASK),
				    (unsigned int) (PMIC_VCORE_TRANSTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgpu_zxos_trim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGPU_ZXOS_TRIM_MASK),
				    (unsigned int) (PMIC_RG_VGPU_ZXOS_TRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgpu_zx_os(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGPU_ZX_OS_MASK),
				    (unsigned int) (PMIC_RG_VGPU_ZX_OS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgpu_csl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGPU_CSL_MASK), (unsigned int) (PMIC_RG_VGPU_CSL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgpu_csr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGPU_CSR_MASK), (unsigned int) (PMIC_RG_VGPU_CSR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgpu_cc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGPU_CC_MASK), (unsigned int) (PMIC_RG_VGPU_CC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgpu_rzsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGPU_RZSEL_MASK),
				    (unsigned int) (PMIC_RG_VGPU_RZSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgpu_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGPU_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VGPU_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgpu_modeset(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGPU_MODESET_MASK),
				    (unsigned int) (PMIC_RG_VGPU_MODESET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgpu_csm(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGPU_CSM_MASK), (unsigned int) (PMIC_RG_VGPU_CSM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgpu_avp_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGPU_AVP_EN_MASK),
				    (unsigned int) (PMIC_RG_VGPU_AVP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgpu_avp_os(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGPU_AVP_OS_MASK),
				    (unsigned int) (PMIC_RG_VGPU_AVP_OS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgpu_slp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGPU_SLP_MASK), (unsigned int) (PMIC_RG_VGPU_SLP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgpu_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGPU_RSV_MASK), (unsigned int) (PMIC_RG_VGPU_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgpu_burst_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_BURST_CTRL_MASK),
				    (unsigned int) (PMIC_VGPU_BURST_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgpu_dlc_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_DLC_CTRL_MASK),
				    (unsigned int) (PMIC_VGPU_DLC_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgpu_vosel_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_VOSEL_CTRL_MASK),
				    (unsigned int) (PMIC_VGPU_VOSEL_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgpu_en_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_EN_CTRL_MASK), (unsigned int) (PMIC_VGPU_EN_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgpu_burst_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_BURST_SEL_MASK),
				    (unsigned int) (PMIC_VGPU_BURST_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgpu_dlc_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_DLC_SEL_MASK), (unsigned int) (PMIC_VGPU_DLC_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgpu_vosel_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_VOSEL_SEL_MASK),
				    (unsigned int) (PMIC_VGPU_VOSEL_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgpu_en_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_EN_SEL_MASK), (unsigned int) (PMIC_VGPU_EN_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vgpu_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VGPU_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VGPU_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VGPU_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vgpu_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VGPU_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VGPU_MODE_MASK), (unsigned int) (PMIC_QI_VGPU_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vgpu_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VGPU_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VGPU_EN_MASK), (unsigned int) (PMIC_QI_VGPU_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vgpu_stb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VGPU_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VGPU_STB_MASK), (unsigned int) (PMIC_QI_VGPU_STB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vgpu_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_STBTD_MASK), (unsigned int) (PMIC_VGPU_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgpu_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_EN_MASK), (unsigned int) (PMIC_VGPU_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgpu_sfchg_ren(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_SFCHG_REN_MASK),
				    (unsigned int) (PMIC_VGPU_SFCHG_REN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgpu_sfchg_rrate(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_SFCHG_RRATE_MASK),
				    (unsigned int) (PMIC_VGPU_SFCHG_RRATE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgpu_sfchg_fen(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_SFCHG_FEN_MASK),
				    (unsigned int) (PMIC_VGPU_SFCHG_FEN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgpu_sfchg_frate(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_SFCHG_FRATE_MASK),
				    (unsigned int) (PMIC_VGPU_SFCHG_FRATE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgpu_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_VOSEL_MASK), (unsigned int) (PMIC_VGPU_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgpu_vosel_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_VOSEL_ON_MASK),
				    (unsigned int) (PMIC_VGPU_VOSEL_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgpu_vosel_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_VOSEL_SLEEP_MASK),
				    (unsigned int) (PMIC_VGPU_VOSEL_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_ni_vgpu_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VGPU_CON12),
				  (&val),
				  (unsigned int) (PMIC_NI_VGPU_VOSEL_MASK), (unsigned int) (PMIC_NI_VGPU_VOSEL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vgpu_burst(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VGPU_CON13),
				  (&val),
				  (unsigned int) (PMIC_QI_VGPU_BURST_MASK), (unsigned int) (PMIC_QI_VGPU_BURST_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vgpu_burst_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_BURST_SLEEP_MASK),
				    (unsigned int) (PMIC_VGPU_BURST_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgpu_burst_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_BURST_ON_MASK),
				    (unsigned int) (PMIC_VGPU_BURST_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgpu_burst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_BURST_MASK), (unsigned int) (PMIC_VGPU_BURST_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vgpu_dlc(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VGPU_CON14),
				  (&val),
				  (unsigned int) (PMIC_QI_VGPU_DLC_MASK), (unsigned int) (PMIC_QI_VGPU_DLC_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vgpu_dlc_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_DLC_SLEEP_MASK),
				    (unsigned int) (PMIC_VGPU_DLC_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgpu_dlc_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_DLC_ON_MASK), (unsigned int) (PMIC_VGPU_DLC_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgpu_dlc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_DLC_MASK), (unsigned int) (PMIC_VGPU_DLC_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vgpu_dlc_n(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VGPU_CON15),
				  (&val),
				  (unsigned int) (PMIC_QI_VGPU_DLC_N_MASK), (unsigned int) (PMIC_QI_VGPU_DLC_N_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vgpu_dlc_n_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_DLC_N_SLEEP_MASK),
				    (unsigned int) (PMIC_VGPU_DLC_N_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgpu_dlc_n_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_DLC_N_ON_MASK),
				    (unsigned int) (PMIC_VGPU_DLC_N_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgpu_dlc_n(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_DLC_N_MASK), (unsigned int) (PMIC_VGPU_DLC_N_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vgpu_bursth(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VGPU_CON16),
				  (&val),
				  (unsigned int) (PMIC_QI_VGPU_BURSTH_MASK),
				  (unsigned int) (PMIC_QI_VGPU_BURSTH_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vgpu_burstl(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VGPU_CON17),
				  (&val),
				  (unsigned int) (PMIC_QI_VGPU_BURSTL_MASK),
				  (unsigned int) (PMIC_QI_VGPU_BURSTL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_ni_vgpu_vsleep_sel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VGPU_CON18),
				  (&val),
				  (unsigned int) (PMIC_NI_VGPU_VSLEEP_SEL_MASK),
				  (unsigned int) (PMIC_NI_VGPU_VSLEEP_SEL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_ni_vgpu_r2r_pdn(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VGPU_CON18),
				  (&val),
				  (unsigned int) (PMIC_NI_VGPU_R2R_PDN_MASK),
				  (unsigned int) (PMIC_NI_VGPU_R2R_PDN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vgpu_vsleep_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_VSLEEP_SEL_MASK),
				    (unsigned int) (PMIC_VGPU_VSLEEP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgpu_r2r_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_R2R_PDN_MASK), (unsigned int) (PMIC_VGPU_R2R_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgpu_vsleep_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_VSLEEP_EN_MASK),
				    (unsigned int) (PMIC_VGPU_VSLEEP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_ni_vgpu_vosel_trans(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VGPU_CON18),
				  (&val),
				  (unsigned int) (PMIC_NI_VGPU_VOSEL_TRANS_MASK),
				  (unsigned int) (PMIC_NI_VGPU_VOSEL_TRANS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vgpu_vosel_trans_once(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_VOSEL_TRANS_ONCE_MASK),
				    (unsigned int) (PMIC_VGPU_VOSEL_TRANS_ONCE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgpu_vosel_trans_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_VOSEL_TRANS_EN_MASK),
				    (unsigned int) (PMIC_VGPU_VOSEL_TRANS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgpu_transtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VGPU_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGPU_TRANSTD_MASK), (unsigned int) (PMIC_VGPU_TRANSTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vio18_trim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIO18_TRIM_MASK),
				    (unsigned int) (PMIC_RG_VIO18_TRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vio18_zx_os(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIO18_ZX_OS_MASK),
				    (unsigned int) (PMIC_RG_VIO18_ZX_OS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vio18_slew(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIO18_SLEW_MASK),
				    (unsigned int) (PMIC_RG_VIO18_SLEW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vio18_slew_nmos(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIO18_SLEW_NMOS_MASK),
				    (unsigned int) (PMIC_RG_VIO18_SLEW_NMOS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vio18_csl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIO18_CSL_MASK), (unsigned int) (PMIC_RG_VIO18_CSL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vio18_csr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIO18_CSR_MASK), (unsigned int) (PMIC_RG_VIO18_CSR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vio18_cc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIO18_CC_MASK), (unsigned int) (PMIC_RG_VIO18_CC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vio18_rzsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIO18_RZSEL_MASK),
				    (unsigned int) (PMIC_RG_VIO18_RZSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vio18_csmir(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIO18_CSMIR_MASK),
				    (unsigned int) (PMIC_RG_VIO18_CSMIR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vio18_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIO18_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VIO18_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vio18_modeset(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIO18_MODESET_MASK),
				    (unsigned int) (PMIC_RG_VIO18_MODESET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vio18_slp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIO18_SLP_MASK), (unsigned int) (PMIC_RG_VIO18_SLP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vio18_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIO18_RSV_MASK), (unsigned int) (PMIC_RG_VIO18_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_burst_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_BURST_CTRL_MASK),
				    (unsigned int) (PMIC_VIO18_BURST_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_dlc_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_DLC_CTRL_MASK),
				    (unsigned int) (PMIC_VIO18_DLC_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_vosel_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_VOSEL_CTRL_MASK),
				    (unsigned int) (PMIC_VIO18_VOSEL_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_en_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_EN_CTRL_MASK),
				    (unsigned int) (PMIC_VIO18_EN_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_burst_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_BURST_SEL_MASK),
				    (unsigned int) (PMIC_VIO18_BURST_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_dlc_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_DLC_SEL_MASK),
				    (unsigned int) (PMIC_VIO18_DLC_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_vosel_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_VOSEL_SEL_MASK),
				    (unsigned int) (PMIC_VIO18_VOSEL_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_en_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_EN_SEL_MASK), (unsigned int) (PMIC_VIO18_EN_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vio18_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VIO18_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VIO18_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VIO18_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vio18_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VIO18_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VIO18_MODE_MASK), (unsigned int) (PMIC_QI_VIO18_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vio18_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VIO18_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VIO18_EN_MASK), (unsigned int) (PMIC_QI_VIO18_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vio18_stb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VIO18_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VIO18_STB_MASK), (unsigned int) (PMIC_QI_VIO18_STB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vio18_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_EN_MASK), (unsigned int) (PMIC_VIO18_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_sfchg_ren(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_SFCHG_REN_MASK),
				    (unsigned int) (PMIC_VIO18_SFCHG_REN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_sfchg_rrate(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_SFCHG_RRATE_MASK),
				    (unsigned int) (PMIC_VIO18_SFCHG_RRATE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_sfchg_fen(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_SFCHG_FEN_MASK),
				    (unsigned int) (PMIC_VIO18_SFCHG_FEN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_sfchg_frate(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_SFCHG_FRATE_MASK),
				    (unsigned int) (PMIC_VIO18_SFCHG_FRATE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_VOSEL_MASK), (unsigned int) (PMIC_VIO18_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_vosel_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_VOSEL_ON_MASK),
				    (unsigned int) (PMIC_VIO18_VOSEL_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_vosel_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_VOSEL_SLEEP_MASK),
				    (unsigned int) (PMIC_VIO18_VOSEL_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_ni_vio18_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VIO18_CON12),
				  (&val),
				  (unsigned int) (PMIC_NI_VIO18_VOSEL_MASK),
				  (unsigned int) (PMIC_NI_VIO18_VOSEL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vio18_burst(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VIO18_CON13),
				  (&val),
				  (unsigned int) (PMIC_QI_VIO18_BURST_MASK),
				  (unsigned int) (PMIC_QI_VIO18_BURST_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vio18_dlc(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VIO18_CON14),
				  (&val),
				  (unsigned int) (PMIC_QI_VIO18_DLC_MASK), (unsigned int) (PMIC_QI_VIO18_DLC_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vio18_dlc_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_DLC_SLEEP_MASK),
				    (unsigned int) (PMIC_VIO18_DLC_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_dlc_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_DLC_ON_MASK), (unsigned int) (PMIC_VIO18_DLC_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_dlc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_DLC_MASK), (unsigned int) (PMIC_VIO18_DLC_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vio18_dlc_n(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VIO18_CON15),
				  (&val),
				  (unsigned int) (PMIC_QI_VIO18_DLC_N_MASK),
				  (unsigned int) (PMIC_QI_VIO18_DLC_N_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vio18_dlc_n_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_DLC_N_SLEEP_MASK),
				    (unsigned int) (PMIC_VIO18_DLC_N_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_dlc_n_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_DLC_N_ON_MASK),
				    (unsigned int) (PMIC_VIO18_DLC_N_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_dlc_n(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_DLC_N_MASK), (unsigned int) (PMIC_VIO18_DLC_N_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vio18_bursth(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VIO18_CON16),
				  (&val),
				  (unsigned int) (PMIC_QI_VIO18_BURSTH_MASK),
				  (unsigned int) (PMIC_QI_VIO18_BURSTH_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vio18_bursth_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_BURSTH_SLEEP_MASK),
				    (unsigned int) (PMIC_VIO18_BURSTH_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_bursth_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_BURSTH_ON_MASK),
				    (unsigned int) (PMIC_VIO18_BURSTH_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_bursth(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_BURSTH_MASK), (unsigned int) (PMIC_VIO18_BURSTH_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vio18_burstl(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VIO18_CON17),
				  (&val),
				  (unsigned int) (PMIC_QI_VIO18_BURSTL_MASK),
				  (unsigned int) (PMIC_QI_VIO18_BURSTL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vio18_burstl_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON17),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_BURSTL_SLEEP_MASK),
				    (unsigned int) (PMIC_VIO18_BURSTL_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_burstl_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON17),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_BURSTL_ON_MASK),
				    (unsigned int) (PMIC_VIO18_BURSTL_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_burstl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON17),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_BURSTL_MASK), (unsigned int) (PMIC_VIO18_BURSTL_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vio18_sleep_pdn(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VIO18_CON18),
				  (&val),
				  (unsigned int) (PMIC_QI_VIO18_SLEEP_PDN_MASK),
				  (unsigned int) (PMIC_QI_VIO18_SLEEP_PDN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vio18_sleep_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_SLEEP_PDN_MASK),
				    (unsigned int) (PMIC_VIO18_SLEEP_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_vsleep_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VIO18_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_VSLEEP_EN_MASK),
				    (unsigned int) (PMIC_VIO18_VSLEEP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vpca7_zxos_trim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPCA7_ZXOS_TRIM_MASK),
				    (unsigned int) (PMIC_RG_VPCA7_ZXOS_TRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vpca7_zx_os(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPCA7_ZX_OS_MASK),
				    (unsigned int) (PMIC_RG_VPCA7_ZX_OS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vpca7_csl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPCA7_CSL_MASK), (unsigned int) (PMIC_RG_VPCA7_CSL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vpca7_csr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPCA7_CSR_MASK), (unsigned int) (PMIC_RG_VPCA7_CSR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vpca7_cc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPCA7_CC_MASK), (unsigned int) (PMIC_RG_VPCA7_CC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vpca7_rzsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPCA7_RZSEL_MASK),
				    (unsigned int) (PMIC_RG_VPCA7_RZSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vpca7_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPCA7_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VPCA7_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vpca7_modeset(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPCA7_MODESET_MASK),
				    (unsigned int) (PMIC_RG_VPCA7_MODESET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vpca7_csm(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPCA7_CSM_MASK), (unsigned int) (PMIC_RG_VPCA7_CSM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vpca7_smrip_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPCA7_SMRIP_EN_MASK),
				    (unsigned int) (PMIC_RG_VPCA7_SMRIP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vpca7_slpo_out(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPCA7_CON3),
				  (&val),
				  (unsigned int) (PMIC_QI_VPCA7_SLPO_OUT_MASK),
				  (unsigned int) (PMIC_QI_VPCA7_SLPO_OUT_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vpca7_slp(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPCA7_CON3),
				  (&val),
				  (unsigned int) (PMIC_QI_VPCA7_SLP_MASK), (unsigned int) (PMIC_QI_VPCA7_SLP_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vpca7_sawcal_td(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_SAWCAL_TD_MASK),
				    (unsigned int) (PMIC_VPCA7_SAWCAL_TD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_slp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_SLP_MASK), (unsigned int) (PMIC_VPCA7_SLP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vpca7_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VPCA7_RSV_MASK), (unsigned int) (PMIC_RG_VPCA7_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_track_on_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_TRACK_ON_CTRL_MASK),
				    (unsigned int) (PMIC_VPCA7_TRACK_ON_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_burst_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_BURST_CTRL_MASK),
				    (unsigned int) (PMIC_VPCA7_BURST_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_dlc_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_DLC_CTRL_MASK),
				    (unsigned int) (PMIC_VPCA7_DLC_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_vosel_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_VOSEL_CTRL_MASK),
				    (unsigned int) (PMIC_VPCA7_VOSEL_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_en_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_EN_CTRL_MASK),
				    (unsigned int) (PMIC_VPCA7_EN_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_burst_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_BURST_SEL_MASK),
				    (unsigned int) (PMIC_VPCA7_BURST_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_dlc_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_DLC_SEL_MASK),
				    (unsigned int) (PMIC_VPCA7_DLC_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_vosel_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_VOSEL_SEL_MASK),
				    (unsigned int) (PMIC_VPCA7_VOSEL_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_en_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_EN_SEL_MASK), (unsigned int) (PMIC_VPCA7_EN_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vpca7_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPCA7_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VPCA7_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VPCA7_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vpca7_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPCA7_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VPCA7_MODE_MASK), (unsigned int) (PMIC_QI_VPCA7_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vpca7_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPCA7_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VPCA7_EN_MASK), (unsigned int) (PMIC_QI_VPCA7_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vpca7_stb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPCA7_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VPCA7_STB_MASK), (unsigned int) (PMIC_QI_VPCA7_STB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vpca7_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_STBTD_MASK), (unsigned int) (PMIC_VPCA7_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_EN_MASK), (unsigned int) (PMIC_VPCA7_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_sfchg_ren(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_SFCHG_REN_MASK),
				    (unsigned int) (PMIC_VPCA7_SFCHG_REN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_sfchg_rrate(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_SFCHG_RRATE_MASK),
				    (unsigned int) (PMIC_VPCA7_SFCHG_RRATE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_sfchg_fen(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_SFCHG_FEN_MASK),
				    (unsigned int) (PMIC_VPCA7_SFCHG_FEN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_sfchg_frate(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_SFCHG_FRATE_MASK),
				    (unsigned int) (PMIC_VPCA7_SFCHG_FRATE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_VOSEL_MASK), (unsigned int) (PMIC_VPCA7_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_vosel_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_VOSEL_ON_MASK),
				    (unsigned int) (PMIC_VPCA7_VOSEL_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_vosel_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_VOSEL_SLEEP_MASK),
				    (unsigned int) (PMIC_VPCA7_VOSEL_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_ni_vpca7_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPCA7_CON12),
				  (&val),
				  (unsigned int) (PMIC_NI_VPCA7_VOSEL_MASK),
				  (unsigned int) (PMIC_NI_VPCA7_VOSEL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vpca7_burst(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPCA7_CON13),
				  (&val),
				  (unsigned int) (PMIC_QI_VPCA7_BURST_MASK),
				  (unsigned int) (PMIC_QI_VPCA7_BURST_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vpca7_burst_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_BURST_SLEEP_MASK),
				    (unsigned int) (PMIC_VPCA7_BURST_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_burst_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_BURST_ON_MASK),
				    (unsigned int) (PMIC_VPCA7_BURST_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_burst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_BURST_MASK), (unsigned int) (PMIC_VPCA7_BURST_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vpca7_dlc(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPCA7_CON14),
				  (&val),
				  (unsigned int) (PMIC_QI_VPCA7_DLC_MASK), (unsigned int) (PMIC_QI_VPCA7_DLC_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vpca7_dlc_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_DLC_SLEEP_MASK),
				    (unsigned int) (PMIC_VPCA7_DLC_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_dlc_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_DLC_ON_MASK), (unsigned int) (PMIC_VPCA7_DLC_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_dlc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_DLC_MASK), (unsigned int) (PMIC_VPCA7_DLC_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vpca7_dlc_n(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPCA7_CON15),
				  (&val),
				  (unsigned int) (PMIC_QI_VPCA7_DLC_N_MASK),
				  (unsigned int) (PMIC_QI_VPCA7_DLC_N_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vpca7_dlc_n_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_DLC_N_SLEEP_MASK),
				    (unsigned int) (PMIC_VPCA7_DLC_N_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_dlc_n_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_DLC_N_ON_MASK),
				    (unsigned int) (PMIC_VPCA7_DLC_N_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_dlc_n(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_DLC_N_MASK), (unsigned int) (PMIC_VPCA7_DLC_N_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vpca7_bursth(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPCA7_CON16),
				  (&val),
				  (unsigned int) (PMIC_QI_VPCA7_BURSTH_MASK),
				  (unsigned int) (PMIC_QI_VPCA7_BURSTH_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vpca7_burstl(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPCA7_CON17),
				  (&val),
				  (unsigned int) (PMIC_QI_VPCA7_BURSTL_MASK),
				  (unsigned int) (PMIC_QI_VPCA7_BURSTL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_ni_vpca7_vsleep_sel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPCA7_CON18),
				  (&val),
				  (unsigned int) (PMIC_NI_VPCA7_VSLEEP_SEL_MASK),
				  (unsigned int) (PMIC_NI_VPCA7_VSLEEP_SEL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_ni_vpca7_r2r_pdn(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPCA7_CON18),
				  (&val),
				  (unsigned int) (PMIC_NI_VPCA7_R2R_PDN_MASK),
				  (unsigned int) (PMIC_NI_VPCA7_R2R_PDN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vpca7_vsleep_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_VSLEEP_SEL_MASK),
				    (unsigned int) (PMIC_VPCA7_VSLEEP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_r2r_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_R2R_PDN_MASK),
				    (unsigned int) (PMIC_VPCA7_R2R_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_vsleep_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_VSLEEP_EN_MASK),
				    (unsigned int) (PMIC_VPCA7_VSLEEP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_ni_vpca7_vosel_trans(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VPCA7_CON18),
				  (&val),
				  (unsigned int) (PMIC_NI_VPCA7_VOSEL_TRANS_MASK),
				  (unsigned int) (PMIC_NI_VPCA7_VOSEL_TRANS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vpca7_vosel_trans_once(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_VOSEL_TRANS_ONCE_MASK),
				    (unsigned int) (PMIC_VPCA7_VOSEL_TRANS_ONCE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_vosel_trans_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_VOSEL_TRANS_EN_MASK),
				    (unsigned int) (PMIC_VPCA7_VOSEL_TRANS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_transtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VPCA7_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_TRANSTD_MASK),
				    (unsigned int) (PMIC_VPCA7_TRANSTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsrmca7_zxos_trim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSRMCA7_ZXOS_TRIM_MASK),
				    (unsigned int) (PMIC_RG_VSRMCA7_ZXOS_TRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsrmca7_zx_os(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSRMCA7_ZX_OS_MASK),
				    (unsigned int) (PMIC_RG_VSRMCA7_ZX_OS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsrmca7_csl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSRMCA7_CSL_MASK),
				    (unsigned int) (PMIC_RG_VSRMCA7_CSL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsrmca7_csr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSRMCA7_CSR_MASK),
				    (unsigned int) (PMIC_RG_VSRMCA7_CSR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsrmca7_cc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSRMCA7_CC_MASK),
				    (unsigned int) (PMIC_RG_VSRMCA7_CC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsrmca7_rzsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSRMCA7_RZSEL_MASK),
				    (unsigned int) (PMIC_RG_VSRMCA7_RZSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsrmca7_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSRMCA7_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VSRMCA7_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsrmca7_modeset(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSRMCA7_MODESET_MASK),
				    (unsigned int) (PMIC_RG_VSRMCA7_MODESET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsrmca7_csm(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSRMCA7_CSM_MASK),
				    (unsigned int) (PMIC_RG_VSRMCA7_CSM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsrmca7_smrip_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSRMCA7_SMRIP_EN_MASK),
				    (unsigned int) (PMIC_RG_VSRMCA7_SMRIP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vsrmca7_slpo_out(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSRMCA7_CON3),
				  (&val),
				  (unsigned int) (PMIC_QI_VSRMCA7_SLPO_OUT_MASK),
				  (unsigned int) (PMIC_QI_VSRMCA7_SLPO_OUT_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vsrmca7_slp(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSRMCA7_CON3),
				  (&val),
				  (unsigned int) (PMIC_QI_VSRMCA7_SLP_MASK),
				  (unsigned int) (PMIC_QI_VSRMCA7_SLP_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vsrmca7_sawcal_td(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_SAWCAL_TD_MASK),
				    (unsigned int) (PMIC_VSRMCA7_SAWCAL_TD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_slp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_SLP_MASK), (unsigned int) (PMIC_VSRMCA7_SLP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vsrmca7_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VSRMCA7_RSV_MASK),
				    (unsigned int) (PMIC_RG_VSRMCA7_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_track_sleep_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_TRACK_SLEEP_CTRL_MASK),
				    (unsigned int) (PMIC_VSRMCA7_TRACK_SLEEP_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_track_on_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_TRACK_ON_CTRL_MASK),
				    (unsigned int) (PMIC_VSRMCA7_TRACK_ON_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_burst_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_BURST_CTRL_MASK),
				    (unsigned int) (PMIC_VSRMCA7_BURST_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_dlc_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_DLC_CTRL_MASK),
				    (unsigned int) (PMIC_VSRMCA7_DLC_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_vosel_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_VOSEL_CTRL_MASK),
				    (unsigned int) (PMIC_VSRMCA7_VOSEL_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_en_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_EN_CTRL_MASK),
				    (unsigned int) (PMIC_VSRMCA7_EN_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_burst_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_BURST_SEL_MASK),
				    (unsigned int) (PMIC_VSRMCA7_BURST_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_dlc_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_DLC_SEL_MASK),
				    (unsigned int) (PMIC_VSRMCA7_DLC_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_vosel_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_VOSEL_SEL_MASK),
				    (unsigned int) (PMIC_VSRMCA7_VOSEL_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_en_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_EN_SEL_MASK),
				    (unsigned int) (PMIC_VSRMCA7_EN_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vsrmca7_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSRMCA7_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VSRMCA7_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VSRMCA7_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vsrmca7_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSRMCA7_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VSRMCA7_MODE_MASK),
				  (unsigned int) (PMIC_QI_VSRMCA7_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vsrmca7_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSRMCA7_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VSRMCA7_EN_MASK), (unsigned int) (PMIC_QI_VSRMCA7_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vsrmca7_stb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSRMCA7_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VSRMCA7_STB_MASK),
				  (unsigned int) (PMIC_QI_VSRMCA7_STB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vsrmca7_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_STBTD_MASK),
				    (unsigned int) (PMIC_VSRMCA7_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_EN_MASK), (unsigned int) (PMIC_VSRMCA7_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_sfchg_ren(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_SFCHG_REN_MASK),
				    (unsigned int) (PMIC_VSRMCA7_SFCHG_REN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_sfchg_rrate(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_SFCHG_RRATE_MASK),
				    (unsigned int) (PMIC_VSRMCA7_SFCHG_RRATE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_sfchg_fen(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_SFCHG_FEN_MASK),
				    (unsigned int) (PMIC_VSRMCA7_SFCHG_FEN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_sfchg_frate(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_SFCHG_FRATE_MASK),
				    (unsigned int) (PMIC_VSRMCA7_SFCHG_FRATE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_VOSEL_MASK),
				    (unsigned int) (PMIC_VSRMCA7_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_vosel_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_VOSEL_ON_MASK),
				    (unsigned int) (PMIC_VSRMCA7_VOSEL_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_vosel_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_VOSEL_SLEEP_MASK),
				    (unsigned int) (PMIC_VSRMCA7_VOSEL_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_ni_vsrmca7_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSRMCA7_CON12),
				  (&val),
				  (unsigned int) (PMIC_NI_VSRMCA7_VOSEL_MASK),
				  (unsigned int) (PMIC_NI_VSRMCA7_VOSEL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vsrmca7_burst(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSRMCA7_CON13),
				  (&val),
				  (unsigned int) (PMIC_QI_VSRMCA7_BURST_MASK),
				  (unsigned int) (PMIC_QI_VSRMCA7_BURST_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vsrmca7_burst_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_BURST_SLEEP_MASK),
				    (unsigned int) (PMIC_VSRMCA7_BURST_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_burst_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_BURST_ON_MASK),
				    (unsigned int) (PMIC_VSRMCA7_BURST_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_burst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_BURST_MASK),
				    (unsigned int) (PMIC_VSRMCA7_BURST_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vsrmca7_dlc(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSRMCA7_CON14),
				  (&val),
				  (unsigned int) (PMIC_QI_VSRMCA7_DLC_MASK),
				  (unsigned int) (PMIC_QI_VSRMCA7_DLC_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vsrmca7_dlc_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_DLC_SLEEP_MASK),
				    (unsigned int) (PMIC_VSRMCA7_DLC_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_dlc_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_DLC_ON_MASK),
				    (unsigned int) (PMIC_VSRMCA7_DLC_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_dlc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_DLC_MASK), (unsigned int) (PMIC_VSRMCA7_DLC_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vsrmca7_dlc_n(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSRMCA7_CON15),
				  (&val),
				  (unsigned int) (PMIC_QI_VSRMCA7_DLC_N_MASK),
				  (unsigned int) (PMIC_QI_VSRMCA7_DLC_N_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vsrmca7_dlc_n_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_DLC_N_SLEEP_MASK),
				    (unsigned int) (PMIC_VSRMCA7_DLC_N_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_dlc_n_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_DLC_N_ON_MASK),
				    (unsigned int) (PMIC_VSRMCA7_DLC_N_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_dlc_n(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_DLC_N_MASK),
				    (unsigned int) (PMIC_VSRMCA7_DLC_N_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vsrmca7_bursth(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSRMCA7_CON16),
				  (&val),
				  (unsigned int) (PMIC_QI_VSRMCA7_BURSTH_MASK),
				  (unsigned int) (PMIC_QI_VSRMCA7_BURSTH_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vsrmca7_burstl(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSRMCA7_CON17),
				  (&val),
				  (unsigned int) (PMIC_QI_VSRMCA7_BURSTL_MASK),
				  (unsigned int) (PMIC_QI_VSRMCA7_BURSTL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_ni_vsrmca7_vsleep_sel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSRMCA7_CON18),
				  (&val),
				  (unsigned int) (PMIC_NI_VSRMCA7_VSLEEP_SEL_MASK),
				  (unsigned int) (PMIC_NI_VSRMCA7_VSLEEP_SEL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_ni_vsrmca7_r2r_pdn(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSRMCA7_CON18),
				  (&val),
				  (unsigned int) (PMIC_NI_VSRMCA7_R2R_PDN_MASK),
				  (unsigned int) (PMIC_NI_VSRMCA7_R2R_PDN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vsrmca7_vsleep_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_VSLEEP_SEL_MASK),
				    (unsigned int) (PMIC_VSRMCA7_VSLEEP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_r2r_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_R2R_PDN_MASK),
				    (unsigned int) (PMIC_VSRMCA7_R2R_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_vsleep_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_VSLEEP_EN_MASK),
				    (unsigned int) (PMIC_VSRMCA7_VSLEEP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_ni_vsrmca7_vosel_trans(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VSRMCA7_CON18),
				  (&val),
				  (unsigned int) (PMIC_NI_VSRMCA7_VOSEL_TRANS_MASK),
				  (unsigned int) (PMIC_NI_VSRMCA7_VOSEL_TRANS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vsrmca7_vosel_trans_once(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_VOSEL_TRANS_ONCE_MASK),
				    (unsigned int) (PMIC_VSRMCA7_VOSEL_TRANS_ONCE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_vosel_trans_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_VOSEL_TRANS_EN_MASK),
				    (unsigned int) (PMIC_VSRMCA7_VOSEL_TRANS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_transtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_TRANSTD_MASK),
				    (unsigned int) (PMIC_VSRMCA7_TRANSTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_vosel_offset(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_VOSEL_OFFSET_MASK),
				    (unsigned int) (PMIC_VSRMCA7_VOSEL_OFFSET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_vosel_delta(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_VOSEL_DELTA_MASK),
				    (unsigned int) (PMIC_VSRMCA7_VOSEL_DELTA_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_vosel_on_hb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON20),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_VOSEL_ON_HB_MASK),
				    (unsigned int) (PMIC_VSRMCA7_VOSEL_ON_HB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_vosel_on_lb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON20),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_VOSEL_ON_LB_MASK),
				    (unsigned int) (PMIC_VSRMCA7_VOSEL_ON_LB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_vosel_sleep_lb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VSRMCA7_CON21),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_VOSEL_SLEEP_LB_MASK),
				    (unsigned int) (PMIC_VSRMCA7_VOSEL_SLEEP_LB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vdrm_zxos_trim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VDRM_ZXOS_TRIM_MASK),
				    (unsigned int) (PMIC_RG_VDRM_ZXOS_TRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vdrm_zx_os(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VDRM_ZX_OS_MASK),
				    (unsigned int) (PMIC_RG_VDRM_ZX_OS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vdrm_csl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VDRM_CSL_MASK), (unsigned int) (PMIC_RG_VDRM_CSL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vdrm_csr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VDRM_CSR_MASK), (unsigned int) (PMIC_RG_VDRM_CSR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vdrm_cc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VDRM_CC_MASK), (unsigned int) (PMIC_RG_VDRM_CC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vdrm_rzsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VDRM_RZSEL_MASK),
				    (unsigned int) (PMIC_RG_VDRM_RZSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vdrm_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VDRM_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VDRM_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vdrm_modeset(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VDRM_MODESET_MASK),
				    (unsigned int) (PMIC_RG_VDRM_MODESET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vdrm_csm(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VDRM_CSM_MASK), (unsigned int) (PMIC_RG_VDRM_CSM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vdrm_avp_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VDRM_AVP_EN_MASK),
				    (unsigned int) (PMIC_RG_VDRM_AVP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vdrm_avp_os(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VDRM_AVP_OS_MASK),
				    (unsigned int) (PMIC_RG_VDRM_AVP_OS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vdrm_slp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VDRM_SLP_MASK), (unsigned int) (PMIC_RG_VDRM_SLP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vdrm_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VDRM_RSV_MASK), (unsigned int) (PMIC_RG_VDRM_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vdrm_burst_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_BURST_CTRL_MASK),
				    (unsigned int) (PMIC_VDRM_BURST_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vdrm_dlc_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_DLC_CTRL_MASK),
				    (unsigned int) (PMIC_VDRM_DLC_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vdrm_vosel_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_VOSEL_CTRL_MASK),
				    (unsigned int) (PMIC_VDRM_VOSEL_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vdrm_en_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_EN_CTRL_MASK), (unsigned int) (PMIC_VDRM_EN_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vdrm_burst_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_BURST_SEL_MASK),
				    (unsigned int) (PMIC_VDRM_BURST_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vdrm_dlc_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_DLC_SEL_MASK), (unsigned int) (PMIC_VDRM_DLC_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vdrm_vosel_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_VOSEL_SEL_MASK),
				    (unsigned int) (PMIC_VDRM_VOSEL_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vdrm_en_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_EN_SEL_MASK), (unsigned int) (PMIC_VDRM_EN_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vdrm_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VDRM_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VDRM_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VDRM_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vdrm_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VDRM_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VDRM_MODE_MASK), (unsigned int) (PMIC_QI_VDRM_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vdrm_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VDRM_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VDRM_EN_MASK), (unsigned int) (PMIC_QI_VDRM_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vdrm_stb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VDRM_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VDRM_STB_MASK), (unsigned int) (PMIC_QI_VDRM_STB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vdrm_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_EN_MASK), (unsigned int) (PMIC_VDRM_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vdrm_sfchg_ren(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_SFCHG_REN_MASK),
				    (unsigned int) (PMIC_VDRM_SFCHG_REN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vdrm_sfchg_rrate(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_SFCHG_RRATE_MASK),
				    (unsigned int) (PMIC_VDRM_SFCHG_RRATE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vdrm_sfchg_fen(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_SFCHG_FEN_MASK),
				    (unsigned int) (PMIC_VDRM_SFCHG_FEN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vdrm_sfchg_frate(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_SFCHG_FRATE_MASK),
				    (unsigned int) (PMIC_VDRM_SFCHG_FRATE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vdrm_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_VOSEL_MASK), (unsigned int) (PMIC_VDRM_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vdrm_vosel_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_VOSEL_ON_MASK),
				    (unsigned int) (PMIC_VDRM_VOSEL_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vdrm_vosel_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_VOSEL_SLEEP_MASK),
				    (unsigned int) (PMIC_VDRM_VOSEL_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_ni_vdrm_vosel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VDRM_CON12),
				  (&val),
				  (unsigned int) (PMIC_NI_VDRM_VOSEL_MASK), (unsigned int) (PMIC_NI_VDRM_VOSEL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vdrm_burst(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VDRM_CON13),
				  (&val),
				  (unsigned int) (PMIC_QI_VDRM_BURST_MASK), (unsigned int) (PMIC_QI_VDRM_BURST_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vdrm_burst_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_BURST_SLEEP_MASK),
				    (unsigned int) (PMIC_VDRM_BURST_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vdrm_burst_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_BURST_ON_MASK),
				    (unsigned int) (PMIC_VDRM_BURST_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vdrm_burst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_BURST_MASK), (unsigned int) (PMIC_VDRM_BURST_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vdrm_dlc(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VDRM_CON14),
				  (&val),
				  (unsigned int) (PMIC_QI_VDRM_DLC_MASK), (unsigned int) (PMIC_QI_VDRM_DLC_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vdrm_dlc_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_DLC_SLEEP_MASK),
				    (unsigned int) (PMIC_VDRM_DLC_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vdrm_dlc_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_DLC_ON_MASK), (unsigned int) (PMIC_VDRM_DLC_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vdrm_dlc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_DLC_MASK), (unsigned int) (PMIC_VDRM_DLC_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vdrm_dlc_n(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VDRM_CON15),
				  (&val),
				  (unsigned int) (PMIC_QI_VDRM_DLC_N_MASK), (unsigned int) (PMIC_QI_VDRM_DLC_N_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vdrm_dlc_n_sleep(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_DLC_N_SLEEP_MASK),
				    (unsigned int) (PMIC_VDRM_DLC_N_SLEEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vdrm_dlc_n_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_DLC_N_ON_MASK),
				    (unsigned int) (PMIC_VDRM_DLC_N_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vdrm_dlc_n(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_DLC_N_MASK), (unsigned int) (PMIC_VDRM_DLC_N_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vdrm_bursth(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VDRM_CON16),
				  (&val),
				  (unsigned int) (PMIC_QI_VDRM_BURSTH_MASK),
				  (unsigned int) (PMIC_QI_VDRM_BURSTH_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vdrm_burstl(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VDRM_CON17),
				  (&val),
				  (unsigned int) (PMIC_QI_VDRM_BURSTL_MASK),
				  (unsigned int) (PMIC_QI_VDRM_BURSTL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_ni_vdrm_vsleep_sel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VDRM_CON18),
				  (&val),
				  (unsigned int) (PMIC_NI_VDRM_VSLEEP_SEL_MASK),
				  (unsigned int) (PMIC_NI_VDRM_VSLEEP_SEL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_ni_vdrm_r2r_pdn(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VDRM_CON18),
				  (&val),
				  (unsigned int) (PMIC_NI_VDRM_R2R_PDN_MASK),
				  (unsigned int) (PMIC_NI_VDRM_R2R_PDN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vdrm_vsleep_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_VSLEEP_SEL_MASK),
				    (unsigned int) (PMIC_VDRM_VSLEEP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vdrm_r2r_pdn(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_R2R_PDN_MASK), (unsigned int) (PMIC_VDRM_R2R_PDN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vdrm_vsleep_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_VSLEEP_EN_MASK),
				    (unsigned int) (PMIC_VDRM_VSLEEP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_ni_vdrm_vosel_trans(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (VDRM_CON18),
				  (&val),
				  (unsigned int) (PMIC_NI_VDRM_VOSEL_TRANS_MASK),
				  (unsigned int) (PMIC_NI_VDRM_VOSEL_TRANS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vdrm_vosel_trans_once(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_VOSEL_TRANS_ONCE_MASK),
				    (unsigned int) (PMIC_VDRM_VOSEL_TRANS_ONCE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vdrm_vosel_trans_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_VOSEL_TRANS_EN_MASK),
				    (unsigned int) (PMIC_VDRM_VOSEL_TRANS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vdrm_transtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (VDRM_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_TRANSTD_MASK), (unsigned int) (PMIC_VDRM_TRANSTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_k_control_smps(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_K_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_K_CONTROL_SMPS_MASK),
				    (unsigned int) (PMIC_K_CONTROL_SMPS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_k_auto_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_K_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_K_AUTO_EN_MASK), (unsigned int) (PMIC_K_AUTO_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_k_src_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_K_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_K_SRC_SEL_MASK), (unsigned int) (PMIC_K_SRC_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_k_start_manual(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_K_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_K_START_MANUAL_MASK),
				    (unsigned int) (PMIC_K_START_MANUAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_k_once(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_K_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_K_ONCE_MASK),
				    (unsigned int) (PMIC_K_ONCE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_k_once_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_K_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_K_ONCE_EN_MASK),
				    (unsigned int) (PMIC_K_ONCE_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_k_map_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_K_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_K_MAP_SEL_MASK), (unsigned int) (PMIC_K_MAP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_k_rst_done(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (BUCK_K_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_K_RST_DONE_MASK), (unsigned int) (PMIC_K_RST_DONE_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_smps_osc_cal(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (BUCK_K_CON1),
				  (&val),
				  (unsigned int) (PMIC_QI_SMPS_OSC_CAL_MASK),
				  (unsigned int) (PMIC_QI_SMPS_OSC_CAL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_k_control(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (BUCK_K_CON1),
				  (&val), (unsigned int) (PMIC_K_CONTROL_MASK), (unsigned int) (PMIC_K_CONTROL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_k_done(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (BUCK_K_CON1),
				  (&val), (unsigned int) (PMIC_K_DONE_MASK), (unsigned int) (PMIC_K_DONE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_k_result(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (BUCK_K_CON1),
				  (&val), (unsigned int) (PMIC_K_RESULT_MASK), (unsigned int) (PMIC_K_RESULT_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vtcxo_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ANALDO_CON0),
				  (&val),
				  (unsigned int) (PMIC_QI_VTCXO_EN_MASK), (unsigned int) (PMIC_QI_VTCXO_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vtcxo_srclk_en_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VTCXO_SRCLK_EN_SEL_MASK),
				    (unsigned int) (PMIC_VTCXO_SRCLK_EN_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vtcxo_on_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VTCXO_ON_CTRL_MASK),
				    (unsigned int) (PMIC_VTCXO_ON_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vtcxo_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VTCXO_EN_MASK), (unsigned int) (PMIC_RG_VTCXO_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vtcxo_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VTCXO_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VTCXO_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vtcxo_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ANALDO_CON0),
				  (&val),
				  (unsigned int) (PMIC_QI_VTCXO_MODE_MASK), (unsigned int) (PMIC_QI_VTCXO_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vtcxo_srclk_mode_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VTCXO_SRCLK_MODE_SEL_MASK),
				    (unsigned int) (PMIC_VTCXO_SRCLK_MODE_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vtcxotd_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VTCXOTD_SEL_MASK), (unsigned int) (PMIC_VTCXOTD_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vtcxo_ocfb_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VTCXO_OCFB_EN_MASK),
				    (unsigned int) (PMIC_RG_VTCXO_OCFB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vtcxo_lp_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VTCXO_LP_SET_MASK), (unsigned int) (PMIC_VTCXO_LP_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vtcxo_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VTCXO_LP_SEL_MASK), (unsigned int) (PMIC_VTCXO_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_va28_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ANALDO_CON1),
				  (&val),
				  (unsigned int) (PMIC_QI_VA28_EN_MASK), (unsigned int) (PMIC_QI_VA28_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_va28_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VA28_EN_MASK), (unsigned int) (PMIC_RG_VA28_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_va28_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VA28_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VA28_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_va28_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ANALDO_CON1),
				  (&val),
				  (unsigned int) (PMIC_QI_VA28_MODE_MASK), (unsigned int) (PMIC_QI_VA28_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_va28_srclk_mode_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VA28_SRCLK_MODE_SEL_MASK),
				    (unsigned int) (PMIC_VA28_SRCLK_MODE_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_va28_lp_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VA28_LP_SET_MASK), (unsigned int) (PMIC_VA28_LP_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_va28_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VA28_LP_SEL_MASK), (unsigned int) (PMIC_VA28_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcama_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMA_EN_MASK), (unsigned int) (PMIC_RG_VCAMA_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcama_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMA_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VCAMA_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcama_ocfb_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMA_OCFB_EN_MASK),
				    (unsigned int) (PMIC_RG_VCAMA_OCFB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vtcxo_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ANALDO_CON3),
				  (&val),
				  (unsigned int) (PMIC_QI_VTCXO_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VTCXO_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_va28_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ANALDO_CON3),
				  (&val),
				  (unsigned int) (PMIC_QI_VA28_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VA28_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vcama_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ANALDO_CON3),
				  (&val),
				  (unsigned int) (PMIC_QI_VCAMA_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VCAMA_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_va28_bist_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VA28_BIST_EN_MASK),
				    (unsigned int) (PMIC_RG_VA28_BIST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vtcxo_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VTCXO_CAL_MASK), (unsigned int) (PMIC_RG_VTCXO_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vtcxo_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VTCXO_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VTCXO_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_va28_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VA28_CAL_MASK), (unsigned int) (PMIC_RG_VA28_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_va28_ocfb_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VA28_OCFB_EN_MASK),
				    (unsigned int) (PMIC_RG_VA28_OCFB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_va28_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VA28_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VA28_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcama_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMA_CAL_MASK), (unsigned int) (PMIC_RG_VCAMA_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcama_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMA_VOSEL_MASK),
				    (unsigned int) (PMIC_RG_VCAMA_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcama_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMA_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VCAMA_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vcama_fbsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMA_FBSEL_MASK),
				    (unsigned int) (PMIC_RG_VCAMA_FBSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_aldo_reserve_1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ALDO_RESERVE_1_MASK),
				    (unsigned int) (PMIC_RG_ALDO_RESERVE_1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_aldo_reserve_2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ALDO_RESERVE_2_MASK),
				    (unsigned int) (PMIC_RG_ALDO_RESERVE_2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_analdo_rsv0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ANALDO_RSV0_MASK), (unsigned int) (PMIC_ANALDO_RSV0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_analdo_rsv1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ANALDO_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ANALDO_RSV1_MASK), (unsigned int) (PMIC_ANALDO_RSV1_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vio28_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON0),
				  (&val),
				  (unsigned int) (PMIC_QI_VIO28_EN_MASK), (unsigned int) (PMIC_QI_VIO28_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vio28_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO28_EN_MASK), (unsigned int) (PMIC_VIO28_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vio28_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIO28_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VIO28_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vio28_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON0),
				  (&val),
				  (unsigned int) (PMIC_QI_VIO28_MODE_MASK), (unsigned int) (PMIC_QI_VIO28_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vio28_srclk_mode_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO28_SRCLK_MODE_SEL_MASK),
				    (unsigned int) (PMIC_VIO28_SRCLK_MODE_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vio28_ocfb_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIO28_OCFB_EN_MASK),
				    (unsigned int) (PMIC_RG_VIO28_OCFB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio28_lp_mode_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO28_LP_MODE_SET_MASK),
				    (unsigned int) (PMIC_VIO28_LP_MODE_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio28_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO28_LP_SEL_MASK), (unsigned int) (PMIC_VIO28_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vusb_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON1),
				  (&val),
				  (unsigned int) (PMIC_QI_VUSB_EN_MASK), (unsigned int) (PMIC_QI_VUSB_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vusb_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VUSB_EN_MASK), (unsigned int) (PMIC_RG_VUSB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vusb_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VUSB_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VUSB_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vusb_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON1),
				  (&val),
				  (unsigned int) (PMIC_QI_VUSB_MODE_MASK), (unsigned int) (PMIC_QI_VUSB_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vusb_srclk_mode_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VUSB_SRCLK_MODE_SEL_MASK),
				    (unsigned int) (PMIC_VUSB_SRCLK_MODE_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vusb_ocfb_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VUSB_OCFB_EN_MASK),
				    (unsigned int) (PMIC_RG_VUSB_OCFB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vusb_lp_mode_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VUSB_LP_MODE_SET_MASK),
				    (unsigned int) (PMIC_VUSB_LP_MODE_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vusb_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VUSB_LP_SEL_MASK), (unsigned int) (PMIC_VUSB_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vmc_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON2),
				  (&val), (unsigned int) (PMIC_QI_VMC_EN_MASK), (unsigned int) (PMIC_QI_VMC_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vmc_int_dis_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMC_INT_DIS_SEL_MASK),
				    (unsigned int) (PMIC_RG_VMC_INT_DIS_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmc_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMC_EN_MASK), (unsigned int) (PMIC_RG_VMC_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmc_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMC_STBTD_MASK), (unsigned int) (PMIC_RG_VMC_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vmc_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON2),
				  (&val),
				  (unsigned int) (PMIC_QI_VMC_MODE_MASK), (unsigned int) (PMIC_QI_VMC_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vmc_srclk_mode_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VMC_SRCLK_MODE_SEL_MASK),
				    (unsigned int) (PMIC_VMC_SRCLK_MODE_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmc_ocfb_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMC_OCFB_EN_MASK),
				    (unsigned int) (PMIC_RG_VMC_OCFB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vmc_lp_mode_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VMC_LP_MODE_SET_MASK),
				    (unsigned int) (PMIC_VMC_LP_MODE_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vmc_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VMC_LP_SEL_MASK), (unsigned int) (PMIC_VMC_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vmch_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON3),
				  (&val),
				  (unsigned int) (PMIC_QI_VMCH_EN_MASK), (unsigned int) (PMIC_QI_VMCH_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vmch_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMCH_EN_MASK), (unsigned int) (PMIC_RG_VMCH_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmch_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMCH_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VMCH_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vmch_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON3),
				  (&val),
				  (unsigned int) (PMIC_QI_VMCH_MODE_MASK), (unsigned int) (PMIC_QI_VMCH_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vmch_srclk_mode_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VMCH_SRCLK_MODE_SEL_MASK),
				    (unsigned int) (PMIC_VMCH_SRCLK_MODE_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vmch_lp_mode_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VMCH_LP_MODE_SET_MASK),
				    (unsigned int) (PMIC_VMCH_LP_MODE_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vmch_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VMCH_LP_SEL_MASK), (unsigned int) (PMIC_VMCH_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vemc_3v3_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON4),
				  (&val),
				  (unsigned int) (PMIC_QI_VEMC_3V3_EN_MASK),
				  (unsigned int) (PMIC_QI_VEMC_3V3_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vemc_3v3_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VEMC_3V3_EN_MASK),
				    (unsigned int) (PMIC_RG_VEMC_3V3_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vemc_3v3_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VEMC_3V3_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VEMC_3V3_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vemc_3v3_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON4),
				  (&val),
				  (unsigned int) (PMIC_QI_VEMC_3V3_MODE_MASK),
				  (unsigned int) (PMIC_QI_VEMC_3V3_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vemc_3v3_srclk_mode_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VEMC_3V3_SRCLK_MODE_SEL_MASK),
				    (unsigned int) (PMIC_VEMC_3V3_SRCLK_MODE_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vemc_3v3_ocfb_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VEMC_3V3_OCFB_EN_MASK),
				    (unsigned int) (PMIC_RG_VEMC_3V3_OCFB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vemc_3v3_lp_mode_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VEMC_3V3_LP_MODE_SET_MASK),
				    (unsigned int) (PMIC_VEMC_3V3_LP_MODE_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vemc_3v3_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VEMC_3V3_LP_SEL_MASK),
				    (unsigned int) (PMIC_VEMC_3V3_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp1_sw_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMD_SW_EN_MASK),
				    (unsigned int) (PMIC_RG_VCAMD_SW_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp1_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMD_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VCAMD_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vgp1_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON5),
				  (&val),
				  (unsigned int) (PMIC_QI_VCAMD_MODE_MASK), (unsigned int) (PMIC_QI_VCAMD_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vgp1_srclk_mode_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCAMD_SRCLK_MODE_SEL_MASK),
				    (unsigned int) (PMIC_VCAMD_SRCLK_MODE_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp1_ocfb_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMD_OCFB_EN_MASK),
				    (unsigned int) (PMIC_RG_VCAMD_OCFB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp1_lp_mode_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCAMD_LP_MODE_SET_MASK),
				    (unsigned int) (PMIC_VCAMD_LP_MODE_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp1_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCAMD_LP_SEL_MASK), (unsigned int) (PMIC_VCAMD_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp2_sw_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMIO_SW_EN_MASK),
				    (unsigned int) (PMIC_RG_VCAMIO_SW_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp2_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMIO_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VCAMIO_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vgp2_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON6),
				  (&val),
				  (unsigned int) (PMIC_QI_VCAMIO_MODE_MASK),
				  (unsigned int) (PMIC_QI_VCAMIO_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vgp2_srclk_mode_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCAMIO_SRCLK_MODE_SEL_MASK),
				    (unsigned int) (PMIC_VCAMIO_SRCLK_MODE_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp2_ocfb_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMIO_OCFB_EN_MASK),
				    (unsigned int) (PMIC_RG_VCAMIO_OCFB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp2_lp_mode_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCAMIO_LP_MODE_SET_MASK),
				    (unsigned int) (PMIC_VCAMIO_LP_MODE_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp2_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCAMIO_LP_SEL_MASK),
				    (unsigned int) (PMIC_VCAMIO_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp3_sw_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMAF_SW_EN_MASK),
				    (unsigned int) (PMIC_RG_VCAMAF_SW_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp3_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMAF_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VCAMAF_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vgp3_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON7),
				  (&val),
				  (unsigned int) (PMIC_QI_VCAMAF_MODE_MASK),
				  (unsigned int) (PMIC_QI_VCAMAF_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vgp3_srclk_mode_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCAMAF_SRCLK_MODE_SEL_MASK),
				    (unsigned int) (PMIC_VCAMAF_SRCLK_MODE_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp3_ocfb_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMAF_OCFB_EN_MASK),
				    (unsigned int) (PMIC_RG_VCAMAF_OCFB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp3_lp_mode_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCAMAF_LP_MODE_SET_MASK),
				    (unsigned int) (PMIC_VCAMAF_LP_MODE_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp3_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCAMAF_LP_SEL_MASK),
				    (unsigned int) (PMIC_VCAMAF_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp4_sw_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP4_SW_EN_MASK),
				    (unsigned int) (PMIC_RG_VGP4_SW_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp4_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP4_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VGP4_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vgp4_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON8),
				  (&val),
				  (unsigned int) (PMIC_QI_VGP4_MODE_MASK), (unsigned int) (PMIC_QI_VGP4_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vgp4_srclk_mode_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGP4_SRCLK_MODE_SEL_MASK),
				    (unsigned int) (PMIC_VGP4_SRCLK_MODE_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp4_ocfb_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP4_OCFB_EN_MASK),
				    (unsigned int) (PMIC_RG_VGP4_OCFB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp4_lp_mode_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGP4_LP_MODE_SET_MASK),
				    (unsigned int) (PMIC_VGP4_LP_MODE_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp4_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGP4_LP_SEL_MASK), (unsigned int) (PMIC_VGP4_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp5_sw_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP5_SW_EN_MASK),
				    (unsigned int) (PMIC_RG_VGP5_SW_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp5_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP5_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VGP5_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vgp5_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON9),
				  (&val),
				  (unsigned int) (PMIC_QI_VGP5_MODE_MASK), (unsigned int) (PMIC_QI_VGP5_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vgp5_srclk_mode_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGP5_SRCLK_MODE_SEL_MASK),
				    (unsigned int) (PMIC_VGP5_SRCLK_MODE_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp5_ocfb_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP5_OCFB_EN_MASK),
				    (unsigned int) (PMIC_RG_VGP5_OCFB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp5_lp_mode_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGP5_LP_MODE_SET_MASK),
				    (unsigned int) (PMIC_VGP5_LP_MODE_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp5_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGP5_LP_SEL_MASK), (unsigned int) (PMIC_VGP5_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp6_sw_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP6_SW_EN_MASK),
				    (unsigned int) (PMIC_RG_VGP6_SW_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp6_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP6_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VGP6_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vgp6_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON10),
				  (&val),
				  (unsigned int) (PMIC_QI_VGP6_MODE_MASK), (unsigned int) (PMIC_QI_VGP6_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vgp6_srclk_mode_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGP6_SRCLK_MODE_SEL_MASK),
				    (unsigned int) (PMIC_VGP6_SRCLK_MODE_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp6_ocfb_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP6_OCFB_EN_MASK),
				    (unsigned int) (PMIC_RG_VGP6_OCFB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp6_lp_mode_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGP6_LP_MODE_SET_MASK),
				    (unsigned int) (PMIC_VGP6_LP_MODE_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp6_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGP6_LP_SEL_MASK), (unsigned int) (PMIC_VGP6_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vibr_mid_state(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIBR_MID_STATE_MASK),
				    (unsigned int) (PMIC_RG_VIBR_MID_STATE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vibr_mst_time(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIBR_MST_TIME_MASK),
				    (unsigned int) (PMIC_RG_VIBR_MST_TIME_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vibr_sw_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIBR_SW_MODE_MASK),
				    (unsigned int) (PMIC_RG_VIBR_SW_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vibr_fr_ori(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIBR_FR_ORI_MASK),
				    (unsigned int) (PMIC_RG_VIBR_FR_ORI_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vrtc_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON12),
				  (&val),
				  (unsigned int) (PMIC_QI_VRTC_EN_MASK), (unsigned int) (PMIC_QI_VRTC_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vrtc_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VRTC_EN_MASK), (unsigned int) (PMIC_VRTC_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vio28_bist_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIO28_BIST_EN_MASK),
				    (unsigned int) (PMIC_RG_VIO28_BIST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmch_bist_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMCH_BIST_EN_MASK),
				    (unsigned int) (PMIC_RG_VMCH_BIST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vrtc_bist_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VRTC_BIST_EN_MASK),
				    (unsigned int) (PMIC_RG_VRTC_BIST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vio28_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON14),
				  (&val),
				  (unsigned int) (PMIC_QI_VIO28_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VIO28_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vusb_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON14),
				  (&val),
				  (unsigned int) (PMIC_QI_VUSB_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VUSB_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vmc_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON14),
				  (&val),
				  (unsigned int) (PMIC_QI_VMC_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VMC_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vmch_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON14),
				  (&val),
				  (unsigned int) (PMIC_QI_VMCH_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VMCH_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vemc_3v3_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON14),
				  (&val),
				  (unsigned int) (PMIC_QI_VEMC_3V3_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VEMC_3V3_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vgp1_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON14),
				  (&val),
				  (unsigned int) (PMIC_QI_VCAMD_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VCAMD_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vgp2_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON14),
				  (&val),
				  (unsigned int) (PMIC_QI_VCAMIO_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VCAMIO_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vgp3_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON14),
				  (&val),
				  (unsigned int) (PMIC_QI_VCAMAF_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VCAMAF_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vgp4_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON14),
				  (&val),
				  (unsigned int) (PMIC_QI_VGP4_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VGP4_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vgp5_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON14),
				  (&val),
				  (unsigned int) (PMIC_QI_VGP5_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VGP5_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vgp6_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON14),
				  (&val),
				  (unsigned int) (PMIC_QI_VGP6_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VGP6_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_qi_vibr_oc_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON14),
				  (&val),
				  (unsigned int) (PMIC_QI_VIBR_OC_STATUS_MASK),
				  (unsigned int) (PMIC_QI_VIBR_OC_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_vio28_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIO28_CAL_MASK), (unsigned int) (PMIC_RG_VIO28_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vio28_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIO28_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VIO28_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vusb_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VUSB_CAL_MASK), (unsigned int) (PMIC_RG_VUSB_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vusb_stb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VUSB_STB_SEL_MASK),
				    (unsigned int) (PMIC_RG_VUSB_STB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vusb_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VUSB_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VUSB_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmch_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON17),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMCH_CAL_MASK), (unsigned int) (PMIC_RG_VMCH_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmch_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON17),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMCH_VOSEL_MASK),
				    (unsigned int) (PMIC_RG_VMCH_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmch_stb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON17),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMCH_STB_SEL_MASK),
				    (unsigned int) (PMIC_RG_VMCH_STB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmch_db_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON17),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMCH_DB_EN_MASK),
				    (unsigned int) (PMIC_RG_VMCH_DB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmch_ocfb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON17),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMCH_OCFB_MASK), (unsigned int) (PMIC_RG_VMCH_OCFB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmch_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON17),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMCH_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VMCH_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vemc_3v3_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VEMC_3V3_CAL_MASK),
				    (unsigned int) (PMIC_RG_VEMC_3V3_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vemc_3v3_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VEMC_3V3_VOSEL_MASK),
				    (unsigned int) (PMIC_RG_VEMC_3V3_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vemc_3v3_stb_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VEMC_3V3_STB_CAL_MASK),
				    (unsigned int) (PMIC_RG_VEMC_3V3_STB_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vemc_3v3_dl_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VEMC_3V3_DL_EN_MASK),
				    (unsigned int) (PMIC_RG_VEMC_3V3_DL_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vemc_3v3_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VEMC_3V3_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VEMC_3V3_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp1_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMD_CAL_MASK), (unsigned int) (PMIC_RG_VCAMD_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp1_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMD_VOSEL_MASK),
				    (unsigned int) (PMIC_RG_VCAMD_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp1_stb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMD_STB_SEL_MASK),
				    (unsigned int) (PMIC_RG_VCAMD_STB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp1_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMD_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VCAMD_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp2_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON20),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMIO_CAL_MASK),
				    (unsigned int) (PMIC_RG_VCAMIO_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp2_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON20),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMIO_VOSEL_MASK),
				    (unsigned int) (PMIC_RG_VCAMIO_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp2_stb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON20),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMIO_STB_SEL_MASK),
				    (unsigned int) (PMIC_RG_VCAMIO_STB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp2_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON20),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMIO_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VCAMIO_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp3_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON21),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMAF_CAL_MASK),
				    (unsigned int) (PMIC_RG_VCAMAF_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp3_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON21),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMAF_VOSEL_MASK),
				    (unsigned int) (PMIC_RG_VCAMAF_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp3_stb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON21),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMAF_STB_SEL_MASK),
				    (unsigned int) (PMIC_RG_VCAMAF_STB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp3_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON21),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VCAMAF_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VCAMAF_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp4_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON22),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP4_CAL_MASK), (unsigned int) (PMIC_RG_VGP4_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp4_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON22),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP4_VOSEL_MASK),
				    (unsigned int) (PMIC_RG_VGP4_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp4_stb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON22),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP4_STB_SEL_MASK),
				    (unsigned int) (PMIC_RG_VGP4_STB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp4_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON22),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP4_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VGP4_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp5_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON23),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP5_CAL_MASK), (unsigned int) (PMIC_RG_VGP5_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp5_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON23),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP5_VOSEL_MASK),
				    (unsigned int) (PMIC_RG_VGP5_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp5_stb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON23),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP5_STB_SEL_MASK),
				    (unsigned int) (PMIC_RG_VGP5_STB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp5_ndis_en_int(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON23),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP5_NDIS_EN_INT_MASK),
				    (unsigned int) (PMIC_RG_VGP5_NDIS_EN_INT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp5_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON23),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP5_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VGP5_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vibr_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON24),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIBR_EN_MASK), (unsigned int) (PMIC_RG_VIBR_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vibr_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON24),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIBR_STBTD_MASK),
				    (unsigned int) (PMIC_RG_VIBR_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_vibr_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (DIGLDO_CON24),
				  (&val),
				  (unsigned int) (PMIC_QI_VIBR_MODE_MASK), (unsigned int) (PMIC_QI_VIBR_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vibr_srclk_mode_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON24),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIBR_SRCLK_MODE_SEL_MASK),
				    (unsigned int) (PMIC_VIBR_SRCLK_MODE_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vibr_ther_shen_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON24),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIBR_THER_SHEN_EN_MASK),
				    (unsigned int) (PMIC_VIBR_THER_SHEN_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vibr_lp_mode_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON24),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIBR_LP_MODE_SET_MASK),
				    (unsigned int) (PMIC_VIBR_LP_MODE_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vibr_lp_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON24),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIBR_LP_SEL_MASK), (unsigned int) (PMIC_VIBR_LP_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vibr_vocal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON25),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIBR_VOCAL_MASK),
				    (unsigned int) (PMIC_RG_VIBR_VOCAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vibr_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON25),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIBR_VOSEL_MASK),
				    (unsigned int) (PMIC_RG_VIBR_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vibr_pwdb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON25),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIBR_PWDB_MASK), (unsigned int) (PMIC_RG_VIBR_PWDB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vibr_drv_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON25),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIBR_DRV_SEL_MASK),
				    (unsigned int) (PMIC_RG_VIBR_DRV_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vibr_stb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON25),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIBR_STB_SEL_MASK),
				    (unsigned int) (PMIC_RG_VIBR_STB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vibr_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON25),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VIBR_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VIBR_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vrtc_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON26),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VRTC_CAL_MASK), (unsigned int) (PMIC_RG_VRTC_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vrtc_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON26),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VRTC_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VRTC_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_ldo_ft(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON27),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LDO_FT_MASK), (unsigned int) (PMIC_RG_LDO_FT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_digldo_rsv0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON27),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_DIGLDO_RSV0_MASK), (unsigned int) (PMIC_DIGLDO_RSV0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_digldo_rsv1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON27),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_DIGLDO_RSV1_MASK), (unsigned int) (PMIC_DIGLDO_RSV1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp1_srclk_en_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON28),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCAMD_SRCLK_EN_SEL_MASK),
				    (unsigned int) (PMIC_VCAMD_SRCLK_EN_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp2_srclk_en_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON28),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCAMIO_SRCLK_EN_SEL_MASK),
				    (unsigned int) (PMIC_VCAMIO_SRCLK_EN_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp3_srclk_en_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON28),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCAMAF_SRCLK_EN_SEL_MASK),
				    (unsigned int) (PMIC_VCAMAF_SRCLK_EN_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp4_srclk_en_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON28),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGP4_SRCLK_EN_SEL_MASK),
				    (unsigned int) (PMIC_VGP4_SRCLK_EN_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp6_srclk_en_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON28),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGP6_SRCLK_EN_SEL_MASK),
				    (unsigned int) (PMIC_VGP6_SRCLK_EN_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp5_srclk_en_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON28),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGP5_SRCLK_EN_SEL_MASK),
				    (unsigned int) (PMIC_VGP5_SRCLK_EN_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmc_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON29),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMC_CAL_MASK), (unsigned int) (PMIC_RG_VMC_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmc_stb_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON29),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMC_STB_CAL_MASK),
				    (unsigned int) (PMIC_RG_VMC_STB_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmc_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON29),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMC_VOSEL_MASK), (unsigned int) (PMIC_RG_VMC_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vmc_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON29),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VMC_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VMC_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp1_on_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON30),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCAMD_ON_CTRL_MASK),
				    (unsigned int) (PMIC_VCAMD_ON_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp2_on_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON30),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCAMIO_ON_CTRL_MASK),
				    (unsigned int) (PMIC_VCAMIO_ON_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp3_on_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON30),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCAMAF_ON_CTRL_MASK),
				    (unsigned int) (PMIC_VCAMAF_ON_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp4_on_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON30),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGP4_ON_CTRL_MASK), (unsigned int) (PMIC_VGP4_ON_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp5_on_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON30),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGP5_ON_CTRL_MASK), (unsigned int) (PMIC_VGP5_ON_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vgp6_on_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON30),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VGP6_ON_CTRL_MASK), (unsigned int) (PMIC_VGP6_ON_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vibr_on_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON30),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIBR_ON_CTRL_MASK), (unsigned int) (PMIC_VIBR_ON_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vibr_srclk_en_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON30),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIBR_SRCLK_EN_SEL_MASK),
				    (unsigned int) (PMIC_VIBR_SRCLK_EN_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_stb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON31),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_STB_SEL_MASK), (unsigned int) (PMIC_RG_STB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_ocfb_tdsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON31),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_OCFB_TDSEL_MASK),
				    (unsigned int) (PMIC_RG_OCFB_TDSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_36us_stbtd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON31),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_36US_STBTD_MASK),
				    (unsigned int) (PMIC_RG_36US_STBTD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_rsv_stb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON32),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_RSV_STB_SEL_MASK),
				    (unsigned int) (PMIC_RG_RSV_STB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_rsv_ldo1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON32),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_RSV_LDO1_MASK), (unsigned int) (PMIC_RG_RSV_LDO1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_rsv_ldo2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON32),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_RSV_LDO2_MASK), (unsigned int) (PMIC_RG_RSV_LDO2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp6_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON33),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP6_CAL_MASK), (unsigned int) (PMIC_RG_VGP6_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp6_vosel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON33),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP6_VOSEL_MASK),
				    (unsigned int) (PMIC_RG_VGP6_VOSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp6_stb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON33),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP6_STB_SEL_MASK),
				    (unsigned int) (PMIC_RG_VGP6_STB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vgp6_ndis_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (DIGLDO_CON33),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VGP6_NDIS_EN_MASK),
				    (unsigned int) (PMIC_RG_VGP6_NDIS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_thrdet_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_THRDET_SEL_MASK),
				    (unsigned int) (PMIC_RG_THRDET_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_thr_hwpdn_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_THR_HWPDN_EN_MASK), (unsigned int) (PMIC_THR_HWPDN_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_strup_thr_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_STRUP_THR_SEL_MASK),
				    (unsigned int) (PMIC_RG_STRUP_THR_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_thr_tmode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_THR_TMODE_MASK), (unsigned int) (PMIC_RG_THR_TMODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_thr_det_dis(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_THR_DET_DIS_MASK), (unsigned int) (PMIC_THR_DET_DIS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vref_bg(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VREF_BG_MASK), (unsigned int) (PMIC_RG_VREF_BG_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_strup_iref_trim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_STRUP_IREF_TRIM_MASK),
				    (unsigned int) (PMIC_RG_STRUP_IREF_TRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_rst_drvsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_RST_DRVSEL_MASK),
				    (unsigned int) (PMIC_RG_RST_DRVSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_en_drvsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_EN_DRVSEL_MASK), (unsigned int) (PMIC_RG_EN_DRVSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_usbdl_keydet_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_USBDL_KEYDET_EN_MASK),
				    (unsigned int) (PMIC_RG_USBDL_KEYDET_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_usbdl_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_USBDL_EN_MASK), (unsigned int) (PMIC_RG_USBDL_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pmu_lev_ungate(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PMU_LEV_UNGATE_MASK),
				    (unsigned int) (PMIC_RG_PMU_LEV_UNGATE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pmu_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PMU_RSV_MASK), (unsigned int) (PMIC_RG_PMU_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_pmu_thr_status(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STRUP_CON4),
				  (&val),
				  (unsigned int) (PMIC_PMU_THR_STATUS_MASK),
				  (unsigned int) (PMIC_PMU_THR_STATUS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_pmu_thr_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STRUP_CON4),
				  (&val),
				  (unsigned int) (PMIC_PMU_THR_DEB_MASK), (unsigned int) (PMIC_PMU_THR_DEB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_thr_test(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_THR_TEST_MASK), (unsigned int) (PMIC_THR_TEST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_strup_dig_io28_pg_force(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_DIG_IO28_PG_FORCE_MASK),
				    (unsigned int) (PMIC_STRUP_DIG_IO28_PG_FORCE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_strup_dig_io_pg_force(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_DIG_IO_PG_FORCE_MASK),
				    (unsigned int) (PMIC_STRUP_DIG_IO_PG_FORCE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rtc_xosc32_enb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RTC_XOSC32_ENB_SEL_MASK),
				    (unsigned int) (PMIC_RTC_XOSC32_ENB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rtc_xosc32_enb_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RTC_XOSC32_ENB_SW_MASK),
				    (unsigned int) (PMIC_RTC_XOSC32_ENB_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_bias_gen_en_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_BIAS_GEN_EN_SEL_MASK),
				    (unsigned int) (PMIC_BIAS_GEN_EN_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_bias_gen_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_BIAS_GEN_EN_MASK), (unsigned int) (PMIC_BIAS_GEN_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_strup_pwron_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_PWRON_SEL_MASK),
				    (unsigned int) (PMIC_STRUP_PWRON_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_strup_pwron(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_PWRON_MASK), (unsigned int) (PMIC_STRUP_PWRON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_bias_gen_en_force(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_BIAS_GEN_EN_FORCE_MASK),
				    (unsigned int) (PMIC_BIAS_GEN_EN_FORCE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_strup_pwron_force(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_PWRON_FORCE_MASK),
				    (unsigned int) (PMIC_STRUP_PWRON_FORCE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_strup_ft_ctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_FT_CTRL_MASK),
				    (unsigned int) (PMIC_STRUP_FT_CTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_pwrbb_deb_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_PWRBB_DEB_EN_MASK), (unsigned int) (PMIC_PWRBB_DEB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_dduvlo_deb_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_DDUVLO_DEB_EN_MASK),
				    (unsigned int) (PMIC_DDUVLO_DEB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca15_pg_enb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_PG_ENB_MASK),
				    (unsigned int) (PMIC_VSRMCA15_PG_ENB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca15_pg_enb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA15_PG_ENB_MASK),
				    (unsigned int) (PMIC_VPCA15_PG_ENB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vmch_pg_enb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VMCH_PG_ENB_MASK), (unsigned int) (PMIC_VMCH_PG_ENB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vmc_pg_enb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VMC_PG_ENB_MASK), (unsigned int) (PMIC_VMC_PG_ENB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vemc_pg_enb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VEMC_PG_ENB_MASK), (unsigned int) (PMIC_VEMC_PG_ENB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vtcxo_pg_enb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VTCXO_PG_ENB_MASK), (unsigned int) (PMIC_VTCXO_PG_ENB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio28_pg_enb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO28_PG_ENB_MASK), (unsigned int) (PMIC_VIO28_PG_ENB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_va28_pg_enb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VA28_PG_ENB_MASK), (unsigned int) (PMIC_VA28_PG_ENB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vio18_pg_enb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VIO18_PG_ENB_MASK), (unsigned int) (PMIC_VIO18_PG_ENB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vdrm_pg_enb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VDRM_PG_ENB_MASK), (unsigned int) (PMIC_VDRM_PG_ENB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcore_pg_enb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_PG_ENB_MASK), (unsigned int) (PMIC_VCORE_PG_ENB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_pg_enb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_PG_ENB_MASK),
				    (unsigned int) (PMIC_VSRMCA7_PG_ENB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_pg_enb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_PG_ENB_MASK), (unsigned int) (PMIC_VPCA7_PG_ENB_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_qi_osc_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STRUP_CON7),
				  (&val), (unsigned int) (PMIC_QI_OSC_EN_MASK), (unsigned int) (PMIC_QI_OSC_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_just_pwrkey_rst(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (STRUP_CON7),
				  (&val),
				  (unsigned int) (PMIC_JUST_PWRKEY_RST_MASK),
				  (unsigned int) (PMIC_JUST_PWRKEY_RST_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vsrmca15_pg_h2l_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA15_PG_H2L_EN_MASK),
				    (unsigned int) (PMIC_VSRMCA15_PG_H2L_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca15_pg_h2l_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA15_PG_H2L_EN_MASK),
				    (unsigned int) (PMIC_VPCA15_PG_H2L_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vcore_pg_h2l_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VCORE_PG_H2L_EN_MASK),
				    (unsigned int) (PMIC_VCORE_PG_H2L_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vsrmca7_pg_h2l_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VSRMCA7_PG_H2L_EN_MASK),
				    (unsigned int) (PMIC_VSRMCA7_PG_H2L_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vpca7_pg_h2l_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VPCA7_PG_H2L_EN_MASK),
				    (unsigned int) (PMIC_VPCA7_PG_H2L_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_uvlo_l2h_deb_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_UVLO_L2H_DEB_EN_MASK),
				    (unsigned int) (PMIC_UVLO_L2H_DEB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_clr_just_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_CLR_JUST_RST_MASK), (unsigned int) (PMIC_CLR_JUST_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_strup_con8_rsv0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_CON8_RSV0_MASK),
				    (unsigned int) (PMIC_STRUP_CON8_RSV0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_strup_ext_pmic_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_EXT_PMIC_SEL_MASK),
				    (unsigned int) (PMIC_STRUP_EXT_PMIC_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_strup_ext_pmic_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_EXT_PMIC_EN_MASK),
				    (unsigned int) (PMIC_STRUP_EXT_PMIC_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_strup_auxadc_rstb_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_AUXADC_RSTB_SEL_MASK),
				    (unsigned int) (PMIC_STRUP_AUXADC_RSTB_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_strup_auxadc_start_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_AUXADC_START_SEL_MASK),
				    (unsigned int) (PMIC_STRUP_AUXADC_START_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_strup_auxadc_rstb_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_AUXADC_RSTB_SW_MASK),
				    (unsigned int) (PMIC_STRUP_AUXADC_RSTB_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_strup_auxadc_start_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_AUXADC_START_SW_MASK),
				    (unsigned int) (PMIC_STRUP_AUXADC_START_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_strup_auxadc_en_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_AUXADC_EN_SEL_MASK),
				    (unsigned int) (PMIC_STRUP_AUXADC_EN_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_strup_pwroff_preoff_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_PWROFF_PREOFF_EN_MASK),
				    (unsigned int) (PMIC_STRUP_PWROFF_PREOFF_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_strup_pwroff_seq_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (STRUP_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STRUP_PWROFF_SEQ_EN_MASK),
				    (unsigned int) (PMIC_STRUP_PWROFF_SEQ_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rg_adc_rdy_c0(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC0),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_RDY_C0_MASK), (unsigned int) (PMIC_RG_ADC_RDY_C0_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c0(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC0),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_C0_MASK), (unsigned int) (PMIC_RG_ADC_OUT_C0_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_c1(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC1),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_RDY_C1_MASK), (unsigned int) (PMIC_RG_ADC_RDY_C1_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c1(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC1),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_C1_MASK), (unsigned int) (PMIC_RG_ADC_OUT_C1_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_c2(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC2),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_RDY_C2_MASK), (unsigned int) (PMIC_RG_ADC_RDY_C2_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c2(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC2),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_C2_MASK), (unsigned int) (PMIC_RG_ADC_OUT_C2_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_c3(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC3),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_RDY_C3_MASK), (unsigned int) (PMIC_RG_ADC_RDY_C3_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c3(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC3),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_C3_MASK), (unsigned int) (PMIC_RG_ADC_OUT_C3_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_c4(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC4),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_RDY_C4_MASK), (unsigned int) (PMIC_RG_ADC_RDY_C4_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c4(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC4),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_C4_MASK), (unsigned int) (PMIC_RG_ADC_OUT_C4_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_c5(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC5),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_RDY_C5_MASK), (unsigned int) (PMIC_RG_ADC_RDY_C5_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c5(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC5),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_C5_MASK), (unsigned int) (PMIC_RG_ADC_OUT_C5_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_c6(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC6),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_RDY_C6_MASK), (unsigned int) (PMIC_RG_ADC_RDY_C6_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c6(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC6),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_C6_MASK), (unsigned int) (PMIC_RG_ADC_OUT_C6_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_c7(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC7),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_RDY_C7_MASK), (unsigned int) (PMIC_RG_ADC_RDY_C7_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c7(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC7),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_C7_MASK), (unsigned int) (PMIC_RG_ADC_OUT_C7_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_wakeup_pchr(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC8),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_RDY_WAKEUP_PCHR_MASK),
				  (unsigned int) (PMIC_RG_ADC_RDY_WAKEUP_PCHR_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_wakeup_pchr(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC8),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_WAKEUP_PCHR_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_WAKEUP_PCHR_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_wakeup_swchr(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC9),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_RDY_WAKEUP_SWCHR_MASK),
				  (unsigned int) (PMIC_RG_ADC_RDY_WAKEUP_SWCHR_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_wakeup_swchr(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC9),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_WAKEUP_SWCHR_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_WAKEUP_SWCHR_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_rdy_lbat(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC10),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_RDY_LBAT_MASK),
				  (unsigned int) (PMIC_RG_ADC_RDY_LBAT_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_lbat(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC10),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_LBAT_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_LBAT_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c0_trim(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC11),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_C0_TRIM_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_C0_TRIM_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c1_trim(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC12),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_C1_TRIM_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_C1_TRIM_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c2_trim(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC13),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_C2_TRIM_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_C2_TRIM_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c3_trim(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC14),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_C3_TRIM_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_C3_TRIM_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c4_trim(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC15),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_C4_TRIM_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_C4_TRIM_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c5_trim(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC16),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_C5_TRIM_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_C5_TRIM_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c6_trim(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC17),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_C6_TRIM_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_C6_TRIM_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_c7_trim(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC18),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_C7_TRIM_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_C7_TRIM_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_wakeup_pchr_trim(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC19),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_WAKEUP_PCHR_TRIM_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_WAKEUP_PCHR_TRIM_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_wakeup_swchr_trim(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC20),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_WAKEUP_SWCHR_TRIM_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_WAKEUP_SWCHR_TRIM_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_lbat_trim(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC21),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_LBAT_TRIM_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_LBAT_TRIM_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_adc_out_avg_deci(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_ADC22),
				  (&val),
				  (unsigned int) (PMIC_RG_ADC_OUT_AVG_DECI_MASK),
				  (unsigned int) (PMIC_RG_ADC_OUT_AVG_DECI_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_spl_num(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPL_NUM_MASK), (unsigned int) (PMIC_RG_SPL_NUM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_avg_num(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AVG_NUM_MASK), (unsigned int) (PMIC_RG_AVG_NUM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_buf_pwd_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BUF_PWD_ON_MASK),
				    (unsigned int) (PMIC_RG_BUF_PWD_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_pwd_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_PWD_ON_MASK),
				    (unsigned int) (PMIC_RG_ADC_PWD_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_buf_pwd_b(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BUF_PWD_B_MASK), (unsigned int) (PMIC_RG_BUF_PWD_B_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_pwd_b(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_PWD_B_MASK), (unsigned int) (PMIC_RG_ADC_PWD_B_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auxadc_chsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUXADC_CHSEL_MASK),
				    (unsigned int) (PMIC_RG_AUXADC_CHSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auxadc_auto_str_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUXADC_AUTO_STR_EN_MASK),
				    (unsigned int) (PMIC_RG_AUXADC_AUTO_STR_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auxadc_auto_str(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUXADC_AUTO_STR_MASK),
				    (unsigned int) (PMIC_RG_AUXADC_AUTO_STR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_trim_comp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_TRIM_COMP_MASK),
				    (unsigned int) (PMIC_RG_ADC_TRIM_COMP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auxadc_bist_enb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUXADC_BIST_ENB_MASK),
				    (unsigned int) (PMIC_RG_AUXADC_BIST_ENB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auxadc_start(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUXADC_START_MASK),
				    (unsigned int) (PMIC_RG_AUXADC_START_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_lbat_debt_min(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LBAT_DEBT_MIN_MASK),
				    (unsigned int) (PMIC_RG_LBAT_DEBT_MIN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_lbat_debt_max(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LBAT_DEBT_MAX_MASK),
				    (unsigned int) (PMIC_RG_LBAT_DEBT_MAX_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_lbat_det_prd_15_0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LBAT_DET_PRD_15_0_MASK),
				    (unsigned int) (PMIC_RG_LBAT_DET_PRD_15_0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_lbat_det_prd_19_16(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LBAT_DET_PRD_19_16_MASK),
				    (unsigned int) (PMIC_RG_LBAT_DET_PRD_19_16_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rg_lbat_max_irq_b(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_CON5),
				  (&val),
				  (unsigned int) (PMIC_RG_LBAT_MAX_IRQ_B_MASK),
				  (unsigned int) (PMIC_RG_LBAT_MAX_IRQ_B_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_lbat_en_max(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LBAT_EN_MAX_MASK),
				    (unsigned int) (PMIC_RG_LBAT_EN_MAX_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_lbat_irq_en_max(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LBAT_IRQ_EN_MAX_MASK),
				    (unsigned int) (PMIC_RG_LBAT_IRQ_EN_MAX_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_lbat_volt_max(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LBAT_VOLT_MAX_MASK),
				    (unsigned int) (PMIC_RG_LBAT_VOLT_MAX_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rg_lbat_min_irq_b(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_CON6),
				  (&val),
				  (unsigned int) (PMIC_RG_LBAT_MIN_IRQ_B_MASK),
				  (unsigned int) (PMIC_RG_LBAT_MIN_IRQ_B_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_lbat_en_min(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LBAT_EN_MIN_MASK),
				    (unsigned int) (PMIC_RG_LBAT_EN_MIN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_lbat_irq_en_min(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LBAT_IRQ_EN_MIN_MASK),
				    (unsigned int) (PMIC_RG_LBAT_IRQ_EN_MIN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_lbat_volt_min(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LBAT_VOLT_MIN_MASK),
				    (unsigned int) (PMIC_RG_LBAT_VOLT_MIN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rg_lbat_debounce_count_max(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_CON7),
				  (&val),
				  (unsigned int) (PMIC_RG_LBAT_DEBOUNCE_COUNT_MAX_MASK),
				  (unsigned int) (PMIC_RG_LBAT_DEBOUNCE_COUNT_MAX_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_lbat_debounce_count_min(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_CON8),
				  (&val),
				  (unsigned int) (PMIC_RG_LBAT_DEBOUNCE_COUNT_MIN_MASK),
				  (unsigned int) (PMIC_RG_LBAT_DEBOUNCE_COUNT_MIN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_ni_comp(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (AUXADC_CON9),
				  (&val),
				  (unsigned int) (PMIC_RG_NI_COMP_MASK), (unsigned int) (PMIC_RG_NI_COMP_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_da_dac(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DA_DAC_MASK), (unsigned int) (PMIC_RG_DA_DAC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auxadc_cali(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUXADC_CALI_MASK),
				    (unsigned int) (PMIC_RG_AUXADC_CALI_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_buf_cali(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BUF_CALI_MASK), (unsigned int) (PMIC_RG_BUF_CALI_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auxadc_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUXADC_RSV_MASK),
				    (unsigned int) (PMIC_RG_AUXADC_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_da_dac_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DA_DAC_SEL_MASK),
				    (unsigned int) (PMIC_RG_DA_DAC_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_aux_out_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUX_OUT_SEL_MASK),
				    (unsigned int) (PMIC_RG_AUX_OUT_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_arb_prio_2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ARB_PRIO_2_MASK),
				    (unsigned int) (PMIC_RG_ARB_PRIO_2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_arb_prio_1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ARB_PRIO_1_MASK),
				    (unsigned int) (PMIC_RG_ARB_PRIO_1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_arb_prio_0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ARB_PRIO_0_MASK),
				    (unsigned int) (PMIC_RG_ARB_PRIO_0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_efuse_offset_ch0_trim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_EFUSE_OFFSET_CH0_TRIM_MASK),
				    (unsigned int) (PMIC_EFUSE_OFFSET_CH0_TRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_efuse_gain_ch0_trim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_EFUSE_GAIN_CH0_TRIM_MASK),
				    (unsigned int) (PMIC_EFUSE_GAIN_CH0_TRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vbuf_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VBUF_EN_MASK), (unsigned int) (PMIC_RG_VBUF_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vbuf_byp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VBUF_BYP_MASK), (unsigned int) (PMIC_RG_VBUF_BYP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vbuf_exten(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VBUF_EXTEN_MASK),
				    (unsigned int) (PMIC_RG_VBUF_EXTEN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vbuf_calen(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VBUF_CALEN_MASK),
				    (unsigned int) (PMIC_RG_VBUF_CALEN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_thermal_adc_oe(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_THERMAL_ADC_OE_MASK),
				    (unsigned int) (PMIC_RG_THERMAL_ADC_OE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_thermal_adc_ge(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_THERMAL_ADC_GE_MASK),
				    (unsigned int) (PMIC_RG_THERMAL_ADC_GE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_adc_trim_ch_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ADC_TRIM_CH_SEL_MASK),
				    (unsigned int) (PMIC_RG_ADC_TRIM_CH_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_source_ch0_norm_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SOURCE_CH0_NORM_SEL_MASK),
				    (unsigned int) (PMIC_RG_SOURCE_CH0_NORM_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_source_ch0_lbat_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUXADC_CON14),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SOURCE_CH0_LBAT_SEL_MASK),
				    (unsigned int) (PMIC_RG_SOURCE_CH0_LBAT_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_flash_rsv0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FLASH_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FLASH_RSV0_MASK), (unsigned int) (PMIC_FLASH_RSV0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_flash_dim_duty(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FLASH_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FLASH_DIM_DUTY_MASK),
				    (unsigned int) (PMIC_FLASH_DIM_DUTY_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_flash_ther_shdn_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FLASH_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FLASH_THER_SHDN_EN_MASK),
				    (unsigned int) (PMIC_FLASH_THER_SHDN_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_flash_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FLASH_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FLASH_EN_MASK), (unsigned int) (PMIC_FLASH_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_flash_dim_div(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FLASH_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FLASH_DIM_DIV_MASK),
				    (unsigned int) (PMIC_FLASH_DIM_DIV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_flash_rsv1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FLASH_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FLASH_RSV1_MASK), (unsigned int) (PMIC_FLASH_RSV1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_flash_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FLASH_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FLASH_SEL_MASK), (unsigned int) (PMIC_FLASH_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_flash_sfstren(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FLASH_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FLASH_SFSTREN_MASK),
				    (unsigned int) (PMIC_FLASH_SFSTREN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_flash_sfstr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FLASH_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FLASH_SFSTR_MASK), (unsigned int) (PMIC_FLASH_SFSTR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_flash_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FLASH_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FLASH_MODE_MASK), (unsigned int) (PMIC_FLASH_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_kpled_rsv0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (KPLED_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_KPLED_RSV0_MASK), (unsigned int) (PMIC_KPLED_RSV0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_kpled_dim_duty(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (KPLED_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_KPLED_DIM_DUTY_MASK),
				    (unsigned int) (PMIC_KPLED_DIM_DUTY_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_kpled_ther_shdn_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (KPLED_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_KPLED_THER_SHDN_EN_MASK),
				    (unsigned int) (PMIC_KPLED_THER_SHDN_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_kpled_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (KPLED_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_KPLED_EN_MASK), (unsigned int) (PMIC_KPLED_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_kpled_dim_div(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (KPLED_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_KPLED_DIM_DIV_MASK),
				    (unsigned int) (PMIC_KPLED_DIM_DIV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_kpled_rsv1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (KPLED_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_KPLED_RSV1_MASK), (unsigned int) (PMIC_KPLED_RSV1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_kpled_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (KPLED_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_KPLED_SEL_MASK), (unsigned int) (PMIC_KPLED_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_kpled_sfstren(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (KPLED_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_KPLED_SFSTREN_MASK),
				    (unsigned int) (PMIC_KPLED_SFSTREN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_kpled_sfstr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (KPLED_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_KPLED_SFSTR_MASK), (unsigned int) (PMIC_KPLED_SFSTR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_kpled_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (KPLED_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_KPLED_MODE_MASK), (unsigned int) (PMIC_KPLED_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_rsv0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_RSV0_MASK), (unsigned int) (PMIC_ISINK_RSV0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_dim0_duty(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_DIM0_DUTY_MASK),
				    (unsigned int) (PMIC_ISINK_DIM0_DUTY_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_dim0_fsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_DIM0_FSEL_MASK),
				    (unsigned int) (PMIC_ISINK_DIM0_FSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_rsv1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_RSV1_MASK), (unsigned int) (PMIC_ISINK_RSV1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_dim1_duty(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_DIM1_DUTY_MASK),
				    (unsigned int) (PMIC_ISINK_DIM1_DUTY_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_dim1_fsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_DIM1_FSEL_MASK),
				    (unsigned int) (PMIC_ISINK_DIM1_FSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_rsv2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_RSV2_MASK), (unsigned int) (PMIC_ISINK_RSV2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_rsv2_isink0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_RSV2_ISINK0_MASK),
				    (unsigned int) (PMIC_ISINK_RSV2_ISINK0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_rsv2_isink1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_RSV2_ISINK1_MASK),
				    (unsigned int) (PMIC_ISINK_RSV2_ISINK1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_rsv2_isink2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_RSV2_ISINK2_MASK),
				    (unsigned int) (PMIC_ISINK_RSV2_ISINK2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_dim2_duty(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_DIM2_DUTY_MASK),
				    (unsigned int) (PMIC_ISINK_DIM2_DUTY_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_dim2_fsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_DIM2_FSEL_MASK),
				    (unsigned int) (PMIC_ISINK_DIM2_FSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_rsv3(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_RSV3_MASK), (unsigned int) (PMIC_ISINK_RSV3_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isinks_ch2_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINKS_CH2_EN_MASK),
				    (unsigned int) (PMIC_ISINKS_CH2_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isinks_ch1_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINKS_CH1_EN_MASK),
				    (unsigned int) (PMIC_ISINKS_CH1_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isinks_ch0_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINKS_CH0_EN_MASK),
				    (unsigned int) (PMIC_ISINKS_CH0_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink_rsv4(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK_RSV4_MASK), (unsigned int) (PMIC_ISINK_RSV4_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isinks2_chop_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINKS2_CHOP_EN_MASK),
				    (unsigned int) (PMIC_ISINKS2_CHOP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isinks1_chop_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINKS1_CHOP_EN_MASK),
				    (unsigned int) (PMIC_ISINKS1_CHOP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isinks0_chop_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINKS0_CHOP_EN_MASK),
				    (unsigned int) (PMIC_ISINKS0_CHOP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isinks_ch0_step(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINKS_CH0_STEP_MASK),
				    (unsigned int) (PMIC_ISINKS_CH0_STEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink0_chop_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK0_CHOP_MODE_MASK),
				    (unsigned int) (PMIC_ISINK0_CHOP_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink0_test_reg(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK0_TEST_REG_MASK),
				    (unsigned int) (PMIC_ISINK0_TEST_REG_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isinks_ch0_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINKS_CH0_MODE_MASK),
				    (unsigned int) (PMIC_ISINKS_CH0_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isinks_ch1_step(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINKS_CH1_STEP_MASK),
				    (unsigned int) (PMIC_ISINKS_CH1_STEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink1_chop_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK1_CHOP_MODE_MASK),
				    (unsigned int) (PMIC_ISINK1_CHOP_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink1_test_reg(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK1_TEST_REG_MASK),
				    (unsigned int) (PMIC_ISINK1_TEST_REG_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isinks_ch1_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINKS_CH1_MODE_MASK),
				    (unsigned int) (PMIC_ISINKS_CH1_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isinks_ch2_step(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINKS_CH2_STEP_MASK),
				    (unsigned int) (PMIC_ISINKS_CH2_STEP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink2_chop_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK2_CHOP_MODE_MASK),
				    (unsigned int) (PMIC_ISINK2_CHOP_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink2_test_reg(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK2_TEST_REG_MASK),
				    (unsigned int) (PMIC_ISINK2_TEST_REG_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isinks_ch2_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINKS_CH2_MODE_MASK),
				    (unsigned int) (PMIC_ISINKS_CH2_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_trim_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TRIM_EN_MASK), (unsigned int) (PMIC_RG_TRIM_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_trim_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_TRIM_SEL_MASK), (unsigned int) (PMIC_RG_TRIM_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_ldo_bist(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LDO_BIST_MASK), (unsigned int) (PMIC_RG_LDO_BIST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_isinks_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ISINKS_RSV_MASK),
				    (unsigned int) (PMIC_RG_ISINKS_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isinks_breath0_trf_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINKS_BREATH0_TRF_SEL_MASK),
				    (unsigned int) (PMIC_ISINKS_BREATH0_TRF_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isinks_breath0_ton_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINKS_BREATH0_TON_SEL_MASK),
				    (unsigned int) (PMIC_ISINKS_BREATH0_TON_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isinks_breath0_toff_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINKS_BREATH0_TOFF_SEL_MASK),
				    (unsigned int) (PMIC_ISINKS_BREATH0_TOFF_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isinks_breath1_trf_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINKS_BREATH1_TRF_SEL_MASK),
				    (unsigned int) (PMIC_ISINKS_BREATH1_TRF_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isinks_breath1_ton_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINKS_BREATH1_TON_SEL_MASK),
				    (unsigned int) (PMIC_ISINKS_BREATH1_TON_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isinks_breath1_toff_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINKS_BREATH1_TOFF_SEL_MASK),
				    (unsigned int) (PMIC_ISINKS_BREATH1_TOFF_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isinks_breath2_trf_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINKS_BREATH2_TRF_SEL_MASK),
				    (unsigned int) (PMIC_ISINKS_BREATH2_TRF_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isinks_breath2_ton_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINKS_BREATH2_TON_SEL_MASK),
				    (unsigned int) (PMIC_ISINKS_BREATH2_TON_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isinks_breath2_toff_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINKS_BREATH2_TOFF_SEL_MASK),
				    (unsigned int) (PMIC_ISINKS_BREATH2_TOFF_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink0_sfstr_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK0_SFSTR_EN_MASK),
				    (unsigned int) (PMIC_ISINK0_SFSTR_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink1_sfstr_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK1_SFSTR_EN_MASK),
				    (unsigned int) (PMIC_ISINK1_SFSTR_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink2_sfstr_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK2_SFSTR_EN_MASK),
				    (unsigned int) (PMIC_ISINK2_SFSTR_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink0_sfstr_tc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK0_SFSTR_TC_MASK),
				    (unsigned int) (PMIC_ISINK0_SFSTR_TC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink1_sfstr_tc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK1_SFSTR_TC_MASK),
				    (unsigned int) (PMIC_ISINK1_SFSTR_TC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_isink2_sfstr_tc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ISINKS_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ISINK2_SFSTR_TC_MASK),
				    (unsigned int) (PMIC_ISINK2_SFSTR_TC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audaccdetrsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDACCDETRSV_MASK),
				    (unsigned int) (PMIC_RG_AUDACCDETRSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_con0_rsv1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_CON0_RSV1_MASK),
				    (unsigned int) (PMIC_ACCDET_CON0_RSV1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audaccdetvin1pulllow(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDACCDETVIN1PULLLOW_MASK),
				    (unsigned int) (PMIC_RG_AUDACCDETVIN1PULLLOW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audaccdettvdet(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDACCDETTVDET_MASK),
				    (unsigned int) (PMIC_RG_AUDACCDETTVDET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_audaccdetanaswctrl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_AUDACCDETANASWCTRL_MASK),
				    (unsigned int) (PMIC_AUDACCDETANASWCTRL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_audaccdetanaswctrl_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_AUDACCDETANASWCTRL_SEL_MASK),
				    (unsigned int) (PMIC_AUDACCDETANASWCTRL_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_con0_rsv0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_CON0_RSV0_MASK),
				    (unsigned int) (PMIC_ACCDET_CON0_RSV0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audaccdetvthcal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDACCDETVTHCAL_MASK),
				    (unsigned int) (PMIC_RG_AUDACCDETVTHCAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_seq_init(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_SEQ_INIT_MASK),
				    (unsigned int) (PMIC_ACCDET_SEQ_INIT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_EN_MASK), (unsigned int) (PMIC_ACCDET_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_mbias_pwm_idle(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_MBIAS_PWM_IDLE_MASK),
				    (unsigned int) (PMIC_ACCDET_MBIAS_PWM_IDLE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_vth_pwm_idle(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_VTH_PWM_IDLE_MASK),
				    (unsigned int) (PMIC_ACCDET_VTH_PWM_IDLE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_cmp_pwm_idle(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_CMP_PWM_IDLE_MASK),
				    (unsigned int) (PMIC_ACCDET_CMP_PWM_IDLE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_mbias_pwm_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_MBIAS_PWM_EN_MASK),
				    (unsigned int) (PMIC_ACCDET_MBIAS_PWM_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_vth_pwm_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_VTH_PWM_EN_MASK),
				    (unsigned int) (PMIC_ACCDET_VTH_PWM_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_cmp_pwm_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_CMP_PWM_EN_MASK),
				    (unsigned int) (PMIC_ACCDET_CMP_PWM_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_pwm_width(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_PWM_WIDTH_MASK),
				    (unsigned int) (PMIC_ACCDET_PWM_WIDTH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_pwm_thresh(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_PWM_THRESH_MASK),
				    (unsigned int) (PMIC_ACCDET_PWM_THRESH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_fall_delay(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_FALL_DELAY_MASK),
				    (unsigned int) (PMIC_ACCDET_FALL_DELAY_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_rise_delay(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_RISE_DELAY_MASK),
				    (unsigned int) (PMIC_ACCDET_RISE_DELAY_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_debounce0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_DEBOUNCE0_MASK),
				    (unsigned int) (PMIC_ACCDET_DEBOUNCE0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_debounce1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_DEBOUNCE1_MASK),
				    (unsigned int) (PMIC_ACCDET_DEBOUNCE1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_debounce2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_DEBOUNCE2_MASK),
				    (unsigned int) (PMIC_ACCDET_DEBOUNCE2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_debounce3(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_DEBOUNCE3_MASK),
				    (unsigned int) (PMIC_ACCDET_DEBOUNCE3_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_ival_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_IVAL_SEL_MASK),
				    (unsigned int) (PMIC_ACCDET_IVAL_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_ival_mem_in(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_IVAL_MEM_IN_MASK),
				    (unsigned int) (PMIC_ACCDET_IVAL_MEM_IN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_ival_sam_in(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_IVAL_SAM_IN_MASK),
				    (unsigned int) (PMIC_ACCDET_IVAL_SAM_IN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_ival_cur_in(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_IVAL_CUR_IN_MASK),
				    (unsigned int) (PMIC_ACCDET_IVAL_CUR_IN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_irq_clr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_IRQ_CLR_MASK),
				    (unsigned int) (PMIC_ACCDET_IRQ_CLR_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_accdet_irq(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ACCDET_CON11),
				  (&val),
				  (unsigned int) (PMIC_ACCDET_IRQ_MASK), (unsigned int) (PMIC_ACCDET_IRQ_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_accdet_pwm_en_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_PWM_EN_SW_MASK),
				    (unsigned int) (PMIC_ACCDET_PWM_EN_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_mbias_en_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_MBIAS_EN_SW_MASK),
				    (unsigned int) (PMIC_ACCDET_MBIAS_EN_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_vth_en_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_VTH_EN_SW_MASK),
				    (unsigned int) (PMIC_ACCDET_VTH_EN_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_cmp_en_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_CMP_EN_SW_MASK),
				    (unsigned int) (PMIC_ACCDET_CMP_EN_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_in_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_IN_SW_MASK), (unsigned int) (PMIC_ACCDET_IN_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_pwm_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_PWM_SEL_MASK),
				    (unsigned int) (PMIC_ACCDET_PWM_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_test_mode5(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_TEST_MODE5_MASK),
				    (unsigned int) (PMIC_ACCDET_TEST_MODE5_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_test_mode4(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_TEST_MODE4_MASK),
				    (unsigned int) (PMIC_ACCDET_TEST_MODE4_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_test_mode3(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_TEST_MODE3_MASK),
				    (unsigned int) (PMIC_ACCDET_TEST_MODE3_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_test_mode2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_TEST_MODE2_MASK),
				    (unsigned int) (PMIC_ACCDET_TEST_MODE2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_test_mode1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_TEST_MODE1_MASK),
				    (unsigned int) (PMIC_ACCDET_TEST_MODE1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_test_mode0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_TEST_MODE0_MASK),
				    (unsigned int) (PMIC_ACCDET_TEST_MODE0_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_accdet_cmp_clk(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ACCDET_CON13),
				  (&val),
				  (unsigned int) (PMIC_ACCDET_CMP_CLK_MASK),
				  (unsigned int) (PMIC_ACCDET_CMP_CLK_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_accdet_vth_clk(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ACCDET_CON13),
				  (&val),
				  (unsigned int) (PMIC_ACCDET_VTH_CLK_MASK),
				  (unsigned int) (PMIC_ACCDET_VTH_CLK_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_accdet_mbias_clk(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ACCDET_CON13),
				  (&val),
				  (unsigned int) (PMIC_ACCDET_MBIAS_CLK_MASK),
				  (unsigned int) (PMIC_ACCDET_MBIAS_CLK_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_accdet_state(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ACCDET_CON13),
				  (&val),
				  (unsigned int) (PMIC_ACCDET_STATE_MASK), (unsigned int) (PMIC_ACCDET_STATE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_accdet_mem_in(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ACCDET_CON13),
				  (&val),
				  (unsigned int) (PMIC_ACCDET_MEM_IN_MASK), (unsigned int) (PMIC_ACCDET_MEM_IN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_accdet_sam_in(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ACCDET_CON13),
				  (&val),
				  (unsigned int) (PMIC_ACCDET_SAM_IN_MASK), (unsigned int) (PMIC_ACCDET_SAM_IN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_accdet_cur_in(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ACCDET_CON13),
				  (&val),
				  (unsigned int) (PMIC_ACCDET_CUR_IN_MASK), (unsigned int) (PMIC_ACCDET_CUR_IN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_accdet_in(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ACCDET_CON13),
				  (&val), (unsigned int) (PMIC_ACCDET_IN_MASK), (unsigned int) (PMIC_ACCDET_IN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_accdet_cur_deb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ACCDET_CON14),
				  (&val),
				  (unsigned int) (PMIC_ACCDET_CUR_DEB_MASK),
				  (unsigned int) (PMIC_ACCDET_CUR_DEB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_accdet_rsv_con0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON15),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_RSV_CON0_MASK),
				    (unsigned int) (PMIC_ACCDET_RSV_CON0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_accdet_rsv_con1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ACCDET_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_ACCDET_RSV_CON1_MASK),
				    (unsigned int) (PMIC_ACCDET_RSV_CON1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_gainl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_GAINL_MASK), (unsigned int) (PMIC_RG_SPK_GAINL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_out_stage_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_OUT_STAGE_SEL_MASK),
				    (unsigned int) (PMIC_SPK_OUT_STAGE_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_ther_shdn_l_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_THER_SHDN_L_EN_MASK),
				    (unsigned int) (PMIC_SPK_THER_SHDN_L_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_oc_shdn_dl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_OC_SHDN_DL_MASK),
				    (unsigned int) (PMIC_SPK_OC_SHDN_DL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_trim_en_l(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_TRIM_EN_L_MASK),
				    (unsigned int) (PMIC_SPK_TRIM_EN_L_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spkmode_l(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPKMODE_L_MASK), (unsigned int) (PMIC_SPKMODE_L_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_en_l(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_EN_L_MASK), (unsigned int) (PMIC_SPK_EN_L_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_spk_trim_done_l(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (SPK_CON1),
				  (&val),
				  (unsigned int) (PMIC_SPK_TRIM_DONE_L_MASK),
				  (unsigned int) (PMIC_SPK_TRIM_DONE_L_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_spk_offset_l_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_OFFSET_L_MODE_MASK),
				    (unsigned int) (PMIC_SPK_OFFSET_L_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_lead_l_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_LEAD_L_SW_MASK),
				    (unsigned int) (PMIC_SPK_LEAD_L_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_offset_l_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_OFFSET_L_SW_MASK),
				    (unsigned int) (PMIC_SPK_OFFSET_L_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_spk_offset_l_ov(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (SPK_CON1),
				  (&val),
				  (unsigned int) (PMIC_SPK_OFFSET_L_OV_MASK),
				  (unsigned int) (PMIC_SPK_OFFSET_L_OV_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_spk_oc_en_l(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_OC_EN_L_MASK),
				    (unsigned int) (PMIC_RG_SPK_OC_EN_L_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spkab_oc_en_l(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPKAB_OC_EN_L_MASK),
				    (unsigned int) (PMIC_RG_SPKAB_OC_EN_L_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_test_en_l(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_TEST_EN_L_MASK),
				    (unsigned int) (PMIC_RG_SPK_TEST_EN_L_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_drc_en_l(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_DRC_EN_L_MASK),
				    (unsigned int) (PMIC_RG_SPK_DRC_EN_L_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spkrcv_en_l(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPKRCV_EN_L_MASK),
				    (unsigned int) (PMIC_RG_SPKRCV_EN_L_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spkab_obias_l(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPKAB_OBIAS_L_MASK),
				    (unsigned int) (PMIC_RG_SPKAB_OBIAS_L_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_slew_l(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_SLEW_L_MASK),
				    (unsigned int) (PMIC_RG_SPK_SLEW_L_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_force_en_l(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_FORCE_EN_L_MASK),
				    (unsigned int) (PMIC_RG_SPK_FORCE_EN_L_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_intg_rst_l(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_INTG_RST_L_MASK),
				    (unsigned int) (PMIC_RG_SPK_INTG_RST_L_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_gainr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_GAINR_MASK), (unsigned int) (PMIC_RG_SPK_GAINR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_ther_shdn_r_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_THER_SHDN_R_EN_MASK),
				    (unsigned int) (PMIC_SPK_THER_SHDN_R_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_oc_shdn_dr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_OC_SHDN_DR_MASK),
				    (unsigned int) (PMIC_SPK_OC_SHDN_DR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_trim_en_r(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_TRIM_EN_R_MASK),
				    (unsigned int) (PMIC_SPK_TRIM_EN_R_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spkmode_r(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPKMODE_R_MASK), (unsigned int) (PMIC_SPKMODE_R_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_en_r(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_EN_R_MASK), (unsigned int) (PMIC_SPK_EN_R_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_spk_trim_done_r(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (SPK_CON4),
				  (&val),
				  (unsigned int) (PMIC_SPK_TRIM_DONE_R_MASK),
				  (unsigned int) (PMIC_SPK_TRIM_DONE_R_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_spk_offset_r_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_OFFSET_R_MODE_MASK),
				    (unsigned int) (PMIC_SPK_OFFSET_R_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_lead_r_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_LEAD_R_SW_MASK),
				    (unsigned int) (PMIC_SPK_LEAD_R_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_offset_r_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_OFFSET_R_SW_MASK),
				    (unsigned int) (PMIC_SPK_OFFSET_R_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_spk_offset_r_ov(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (SPK_CON4),
				  (&val),
				  (unsigned int) (PMIC_SPK_OFFSET_R_OV_MASK),
				  (unsigned int) (PMIC_SPK_OFFSET_R_OV_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_spkpga_gainr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPKPGA_GAINR_MASK),
				    (unsigned int) (PMIC_RG_SPKPGA_GAINR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_oc_en_r(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_OC_EN_R_MASK),
				    (unsigned int) (PMIC_RG_SPK_OC_EN_R_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spkab_oc_en_r(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPKAB_OC_EN_R_MASK),
				    (unsigned int) (PMIC_RG_SPKAB_OC_EN_R_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_test_en_r(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_TEST_EN_R_MASK),
				    (unsigned int) (PMIC_RG_SPK_TEST_EN_R_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_drc_en_r(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_DRC_EN_R_MASK),
				    (unsigned int) (PMIC_RG_SPK_DRC_EN_R_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spkrcv_en_r(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPKRCV_EN_R_MASK),
				    (unsigned int) (PMIC_RG_SPKRCV_EN_R_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spkab_obias_r(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPKAB_OBIAS_R_MASK),
				    (unsigned int) (PMIC_RG_SPKAB_OBIAS_R_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_slew_r(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_SLEW_R_MASK),
				    (unsigned int) (PMIC_RG_SPK_SLEW_R_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_force_en_r(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_FORCE_EN_R_MASK),
				    (unsigned int) (PMIC_RG_SPK_FORCE_EN_R_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_intg_rst_r(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_INTG_RST_R_MASK),
				    (unsigned int) (PMIC_RG_SPK_INTG_RST_R_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_spk_ab_oc_l_deg(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (SPK_CON6),
				  (&val),
				  (unsigned int) (PMIC_SPK_AB_OC_L_DEG_MASK),
				  (unsigned int) (PMIC_SPK_AB_OC_L_DEG_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_spk_d_oc_l_deg(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (SPK_CON6),
				  (&val),
				  (unsigned int) (PMIC_SPK_D_OC_L_DEG_MASK),
				  (unsigned int) (PMIC_SPK_D_OC_L_DEG_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_spk_ab_oc_r_deg(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (SPK_CON6),
				  (&val),
				  (unsigned int) (PMIC_SPK_AB_OC_R_DEG_MASK),
				  (unsigned int) (PMIC_SPK_AB_OC_R_DEG_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_spk_d_oc_r_deg(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (SPK_CON6),
				  (&val),
				  (unsigned int) (PMIC_SPK_D_OC_R_DEG_MASK),
				  (unsigned int) (PMIC_SPK_D_OC_R_DEG_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_spk_oc_thd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_OC_THD_MASK), (unsigned int) (PMIC_SPK_OC_THD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_oc_wnd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_OC_WND_MASK), (unsigned int) (PMIC_SPK_OC_WND_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_trim_thd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_TRIM_THD_MASK), (unsigned int) (PMIC_SPK_TRIM_THD_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_trim_wnd(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_TRIM_WND_MASK), (unsigned int) (PMIC_SPK_TRIM_WND_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_trim_div(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_TRIM_DIV_MASK), (unsigned int) (PMIC_SPK_TRIM_DIV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_td3(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_TD3_MASK), (unsigned int) (PMIC_SPK_TD3_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_td2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_TD2_MASK), (unsigned int) (PMIC_SPK_TD2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_td1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_TD1_MASK), (unsigned int) (PMIC_SPK_TD1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_octh_d(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_OCTH_D_MASK),
				    (unsigned int) (PMIC_RG_SPK_OCTH_D_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spkab_ovdrv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPKAB_OVDRV_MASK),
				    (unsigned int) (PMIC_RG_SPKAB_OVDRV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_fbrc_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_FBRC_EN_MASK),
				    (unsigned int) (PMIC_RG_SPK_FBRC_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_vcm_ibsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_VCM_IBSEL_MASK),
				    (unsigned int) (PMIC_RG_SPK_VCM_IBSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_vcm_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_VCM_SEL_MASK),
				    (unsigned int) (PMIC_RG_SPK_VCM_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_en_view_clk(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_EN_VIEW_CLK_MASK),
				    (unsigned int) (PMIC_RG_SPK_EN_VIEW_CLK_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_en_view_vcm(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_EN_VIEW_VCM_MASK),
				    (unsigned int) (PMIC_RG_SPK_EN_VIEW_VCM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_ccode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_CCODE_MASK), (unsigned int) (PMIC_RG_SPK_CCODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_ibias_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_IBIAS_SEL_MASK),
				    (unsigned int) (PMIC_RG_SPK_IBIAS_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_btl_set(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON8),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_BTL_SET_MASK), (unsigned int) (PMIC_RG_BTL_SET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_test_mode1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_TEST_MODE1_MASK),
				    (unsigned int) (PMIC_SPK_TEST_MODE1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_test_mode0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_TEST_MODE0_MASK),
				    (unsigned int) (PMIC_SPK_TEST_MODE0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_vcm_fast_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_VCM_FAST_EN_MASK),
				    (unsigned int) (PMIC_SPK_VCM_FAST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_rsv0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_RSV0_MASK), (unsigned int) (PMIC_SPK_RSV0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spkpga_gainl(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPKPGA_GAINL_MASK),
				    (unsigned int) (PMIC_RG_SPKPGA_GAINL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spk_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPK_RSV_MASK), (unsigned int) (PMIC_RG_SPK_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_td_done(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_TD_DONE_MASK), (unsigned int) (PMIC_SPK_TD_DONE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_td_wait(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON10),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_TD_WAIT_MASK), (unsigned int) (PMIC_SPK_TD_WAIT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_trim_stop_l_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_TRIM_STOP_L_SW_MASK),
				    (unsigned int) (PMIC_SPK_TRIM_STOP_L_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_trim_stop_r_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_TRIM_STOP_R_SW_MASK),
				    (unsigned int) (PMIC_SPK_TRIM_STOP_R_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_trim_en_l_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_TRIM_EN_L_SW_MASK),
				    (unsigned int) (PMIC_SPK_TRIM_EN_L_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_trim_en_r_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_TRIM_EN_R_SW_MASK),
				    (unsigned int) (PMIC_SPK_TRIM_EN_R_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_outstg_en_l_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_OUTSTG_EN_L_SW_MASK),
				    (unsigned int) (PMIC_SPK_OUTSTG_EN_L_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_outstg_en_r_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_OUTSTG_EN_R_SW_MASK),
				    (unsigned int) (PMIC_SPK_OUTSTG_EN_R_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_en_l_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_EN_L_SW_MASK), (unsigned int) (PMIC_SPK_EN_L_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_en_r_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_EN_R_SW_MASK), (unsigned int) (PMIC_SPK_EN_R_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_depop_en_l_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_DEPOP_EN_L_SW_MASK),
				    (unsigned int) (PMIC_SPK_DEPOP_EN_L_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_depop_en_r_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_DEPOP_EN_R_SW_MASK),
				    (unsigned int) (PMIC_SPK_DEPOP_EN_R_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spkmode_l_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPKMODE_L_SW_MASK), (unsigned int) (PMIC_SPKMODE_L_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spkmode_r_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPKMODE_R_SW_MASK), (unsigned int) (PMIC_SPKMODE_R_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_rst_l_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_RST_L_SW_MASK), (unsigned int) (PMIC_SPK_RST_L_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_rst_r_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_RST_R_SW_MASK), (unsigned int) (PMIC_SPK_RST_R_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_vcm_fast_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_VCM_FAST_SW_MASK),
				    (unsigned int) (PMIC_SPK_VCM_FAST_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_spk_en_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (SPK_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_SPK_EN_MODE_MASK), (unsigned int) (PMIC_SPK_EN_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fg_sw_rstclr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_SW_RSTCLR_MASK), (unsigned int) (PMIC_FG_SW_RSTCLR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fg_charge_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_CHARGE_RST_MASK),
				    (unsigned int) (PMIC_FG_CHARGE_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fg_time_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_TIME_RST_MASK), (unsigned int) (PMIC_FG_TIME_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fg_offset_rst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_OFFSET_RST_MASK),
				    (unsigned int) (PMIC_FG_OFFSET_RST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fg_sw_clear(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_SW_CLEAR_MASK), (unsigned int) (PMIC_FG_SW_CLEAR_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_fg_latchdata_st(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (FGADC_CON0),
				  (&val),
				  (unsigned int) (PMIC_FG_LATCHDATA_ST_MASK),
				  (unsigned int) (PMIC_FG_LATCHDATA_ST_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_fg_sw_read_pre(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_SW_READ_PRE_MASK),
				    (unsigned int) (PMIC_FG_SW_READ_PRE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fg_sw_cr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_SW_CR_MASK), (unsigned int) (PMIC_FG_SW_CR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_fgclksrc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_FGCLKSRC_MASK), (unsigned int) (PMIC_RG_FGCLKSRC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fg_autocalrate(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_AUTOCALRATE_MASK),
				    (unsigned int) (PMIC_FG_AUTOCALRATE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fg_cal(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_CAL_MASK),
				    (unsigned int) (PMIC_FG_CAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fg_vmode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_VMODE_MASK), (unsigned int) (PMIC_FG_VMODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fg_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_ON_MASK),
				    (unsigned int) (PMIC_FG_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_fg_car_35_32(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (FGADC_CON1),
				  (&val),
				  (unsigned int) (PMIC_FG_CAR_35_32_MASK), (unsigned int) (PMIC_FG_CAR_35_32_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_fg_car_31_16(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (FGADC_CON2),
				  (&val),
				  (unsigned int) (PMIC_FG_CAR_31_16_MASK), (unsigned int) (PMIC_FG_CAR_31_16_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_fg_car_15_00(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (FGADC_CON3),
				  (&val),
				  (unsigned int) (PMIC_FG_CAR_15_00_MASK), (unsigned int) (PMIC_FG_CAR_15_00_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_fg_nter_29_16(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (FGADC_CON4),
				  (&val),
				  (unsigned int) (PMIC_FG_NTER_29_16_MASK), (unsigned int) (PMIC_FG_NTER_29_16_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_fg_nter_15_00(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (FGADC_CON5),
				  (&val),
				  (unsigned int) (PMIC_FG_NTER_15_00_MASK), (unsigned int) (PMIC_FG_NTER_15_00_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_fg_bltr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_BLTR_MASK), (unsigned int) (PMIC_FG_BLTR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fg_bftr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON7),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_BFTR_MASK), (unsigned int) (PMIC_FG_BFTR_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_fg_current_out(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (FGADC_CON8),
				  (&val),
				  (unsigned int) (PMIC_FG_CURRENT_OUT_MASK),
				  (unsigned int) (PMIC_FG_CURRENT_OUT_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_fg_adjust_offset_value(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON9),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_ADJUST_OFFSET_VALUE_MASK),
				    (unsigned int) (PMIC_FG_ADJUST_OFFSET_VALUE_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_fg_offset(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (FGADC_CON10),
				  (&val), (unsigned int) (PMIC_FG_OFFSET_MASK), (unsigned int) (PMIC_FG_OFFSET_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_inputclksel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_INPUTCLKSEL_MASK),
				    (unsigned int) (PMIC_RG_INPUTCLKSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_fganalogtest(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_FGANALOGTEST_MASK),
				    (unsigned int) (PMIC_RG_FGANALOGTEST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_spare(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON11),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SPARE_MASK), (unsigned int) (PMIC_RG_SPARE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fg_adc_autorst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_ADC_AUTORST_MASK),
				    (unsigned int) (PMIC_FG_ADC_AUTORST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fg_adj_offset_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_ADJ_OFFSET_EN_MASK),
				    (unsigned int) (PMIC_FG_ADJ_OFFSET_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vol_osr_h(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VOL_OSR_H_MASK), (unsigned int) (PMIC_VOL_OSR_H_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_vol_osr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VOL_OSR_MASK), (unsigned int) (PMIC_VOL_OSR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fg_osr_h(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_OSR_H_MASK), (unsigned int) (PMIC_FG_OSR_H_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fg_osr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON12),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_OSR_MASK),
				    (unsigned int) (PMIC_FG_OSR_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rg_fgvmode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (FGADC_CON13),
				  (&val),
				  (unsigned int) (PMIC_RG_FGVMODE_MASK), (unsigned int) (PMIC_RG_FGVMODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_fg_rst(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (FGADC_CON13),
				  (&val), (unsigned int) (PMIC_FG_RST_MASK), (unsigned int) (PMIC_FG_RST_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_fgcal_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (FGADC_CON13),
				  (&val), (unsigned int) (PMIC_FGCAL_EN_MASK), (unsigned int) (PMIC_FGCAL_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_fgadc_en(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (FGADC_CON13),
				  (&val), (unsigned int) (PMIC_FGADC_EN_MASK), (unsigned int) (PMIC_FGADC_EN_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_fg_slp_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_SLP_EN_MASK), (unsigned int) (PMIC_FG_SLP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_fg_adc_rstdetect(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (FGADC_CON13),
				  (&val),
				  (unsigned int) (PMIC_FG_ADC_RSTDETECT_MASK),
				  (unsigned int) (PMIC_FG_ADC_RSTDETECT_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_fg_h_int_sts(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (FGADC_CON13),
				  (&val),
				  (unsigned int) (PMIC_FG_H_INT_STS_MASK), (unsigned int) (PMIC_FG_H_INT_STS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_fg_l_int_sts(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (FGADC_CON13),
				  (&val),
				  (unsigned int) (PMIC_FG_L_INT_STS_MASK), (unsigned int) (PMIC_FG_L_INT_STS_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_vol_fir1bypass(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_VOL_FIR1BYPASS_MASK),
				    (unsigned int) (PMIC_VOL_FIR1BYPASS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fg_fir2bypass(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_FIR2BYPASS_MASK),
				    (unsigned int) (PMIC_FG_FIR2BYPASS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fg_fir1bypass(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON13),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_FIR1BYPASS_MASK),
				    (unsigned int) (PMIC_FG_FIR1BYPASS_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_vol_current_out(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (FGADC_CON14),
				  (&val),
				  (unsigned int) (PMIC_VOL_CURRENT_OUT_MASK),
				  (unsigned int) (PMIC_VOL_CURRENT_OUT_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_fg_cic2(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (FGADC_CON15),
				  (&val), (unsigned int) (PMIC_FG_CIC2_MASK), (unsigned int) (PMIC_FG_CIC2_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_fg_slp_cur_th(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON16),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_SLP_CUR_TH_MASK),
				    (unsigned int) (PMIC_FG_SLP_CUR_TH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fg_slp_time(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON17),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_SLP_TIME_MASK), (unsigned int) (PMIC_FG_SLP_TIME_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fg_det_time(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_DET_TIME_MASK), (unsigned int) (PMIC_FG_DET_TIME_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fg_srcvolten_ftime(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON18),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_SRCVOLTEN_FTIME_MASK),
				    (unsigned int) (PMIC_FG_SRCVOLTEN_FTIME_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fg_test_mode1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_TEST_MODE1_MASK),
				    (unsigned int) (PMIC_FG_TEST_MODE1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fg_test_mode0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_TEST_MODE0_MASK),
				    (unsigned int) (PMIC_FG_TEST_MODE0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fg_rsv1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_RSV1_MASK), (unsigned int) (PMIC_FG_RSV1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fg_vmode_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_VMODE_SW_MASK), (unsigned int) (PMIC_FG_VMODE_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fg_fgadc_en_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_FGADC_EN_SW_MASK),
				    (unsigned int) (PMIC_FG_FGADC_EN_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fg_fgcal_en_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_FGCAL_EN_SW_MASK),
				    (unsigned int) (PMIC_FG_FGCAL_EN_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fg_rst_sw(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_RST_SW_MASK), (unsigned int) (PMIC_FG_RST_SW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_fg_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (FGADC_CON19),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_FG_MODE_MASK), (unsigned int) (PMIC_FG_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_stmp_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_STMP_MODE_MASK), (unsigned int) (PMIC_STMP_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_mix_xosc32_stp_cali(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_MIX_XOSC32_STP_CALI_MASK),
				    (unsigned int) (PMIC_MIX_XOSC32_STP_CALI_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_mix_xosc32_stp_lpdrst(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_MIX_XOSC32_STP_LPDRST_MASK),
				    (unsigned int) (PMIC_MIX_XOSC32_STP_LPDRST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_mix_xosc32_stp_lpden(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_MIX_XOSC32_STP_LPDEN_MASK),
				    (unsigned int) (PMIC_MIX_XOSC32_STP_LPDEN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_mix_xosc32_stp_lpdtb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (RTC_MIX_CON0),
				  (&val),
				  (unsigned int) (PMIC_MIX_XOSC32_STP_LPDTB_MASK),
				  (unsigned int) (PMIC_MIX_XOSC32_STP_LPDTB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_mix_xosc32_stp_pwdb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_MIX_XOSC32_STP_PWDB_MASK),
				    (unsigned int) (PMIC_MIX_XOSC32_STP_PWDB_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_mix_xosc32_stp_cpdtb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (RTC_MIX_CON0),
				  (&val),
				  (unsigned int) (PMIC_MIX_XOSC32_STP_CPDTB_MASK),
				  (unsigned int) (PMIC_MIX_XOSC32_STP_CPDTB_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_mix_eosc32_opt(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_MIX_EOSC32_OPT_MASK),
				    (unsigned int) (PMIC_MIX_EOSC32_OPT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_mix_efuse_xosc32_enb_opt(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_mix_efuse_xosc32_enb_opt_MASK),
				    (unsigned int) (PMIC_mix_efuse_xosc32_enb_opt_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_mix_rtc_xosc32_enb(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (RTC_MIX_CON1),
				  (&val),
				  (unsigned int) (PMIC_mix_rtc_xosc32_enb_MASK),
				  (unsigned int) (PMIC_mix_rtc_xosc32_enb_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_mix_stp_rtc_ddlo(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (RTC_MIX_CON1),
				  (&val),
				  (unsigned int) (PMIC_mix_stp_rtc_ddlo_MASK),
				  (unsigned int) (PMIC_mix_stp_rtc_ddlo_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_mix_stp_bbwakeup(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_mix_stp_bbwakeup_MASK),
				    (unsigned int) (PMIC_mix_stp_bbwakeup_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_mix_eosc32_vct_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_MIX_EOSC32_VCT_EN_MASK),
				    (unsigned int) (PMIC_MIX_EOSC32_VCT_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_mix_eosc32_stp_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_MIX_EOSC32_STP_RSV_MASK),
				    (unsigned int) (PMIC_MIX_EOSC32_STP_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_mix_dcxo_stp_test_deglitch_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_MIX_DCXO_STP_TEST_DEGLITCH_MODE_MASK),
				    (unsigned int) (PMIC_MIX_DCXO_STP_TEST_DEGLITCH_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_mix_rtc_stp_xosc32_enb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_MIX_RTC_STP_XOSC32_ENB_MASK),
				    (unsigned int) (PMIC_MIX_RTC_STP_XOSC32_ENB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_mix_pmu_stp_ddlo_vrtc_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_MIX_PMU_STP_DDLO_VRTC_EN_MASK),
				    (unsigned int) (PMIC_MIX_PMU_STP_DDLO_VRTC_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_mix_pmu_stp_ddlo_vrtc(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_MIX_PMU_STP_DDLO_VRTC_MASK),
				    (unsigned int) (PMIC_MIX_PMU_STP_DDLO_VRTC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_mix_dcxo_stp_lvsh_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_MIX_DCXO_STP_LVSH_EN_MASK),
				    (unsigned int) (PMIC_MIX_DCXO_STP_LVSH_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_mix_eosc32_stp_chop_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RTC_MIX_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_MIX_EOSC32_STP_CHOP_EN_MASK),
				    (unsigned int) (PMIC_MIX_EOSC32_STP_CHOP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dc2ac_en_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDDAC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DC2AC_EN_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_DC2AC_EN_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_aud_dac_pwl_up_va28(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDDAC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUD_DAC_PWL_UP_VA28_MASK),
				    (unsigned int) (PMIC_RG_AUD_DAC_PWL_UP_VA28_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_aud_dac_pwr_up_va28(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDDAC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUD_DAC_PWR_UP_VA28_MASK),
				    (unsigned int) (PMIC_RG_AUD_DAC_PWR_UP_VA28_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auddacrpwrup_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDDAC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDDACRPWRUP_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDDACRPWRUP_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auddaclpwrup_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDDAC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDDACLPWRUP_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDDACLPWRUP_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audhprscdisable_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDHPRSCDISABLE_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDHPRSCDISABLE_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audhplscdisable_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDHPLSCDISABLE_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDHPLSCDISABLE_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audhsscdisable_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDHSSCDISABLE_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDHSSCDISABLE_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audhprmuxinputsel_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDHPRMUXINPUTSEL_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDHPRMUXINPUTSEL_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audhplmuxinputsel_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDHPLMUXINPUTSEL_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDHPLMUXINPUTSEL_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audhsmuxinputsel_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDHSMUXINPUTSEL_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDHSMUXINPUTSEL_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audhprpwrup_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDHPRPWRUP_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDHPRPWRUP_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audhplpwrup_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDHPLPWRUP_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDHPLPWRUP_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audhspwrup_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDHSPWRUP_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDHSPWRUP_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_linenoiseenh_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LINENOISEENH_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_LINENOISEENH_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_hpout_shortvcm_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_HPOUT_SHORTVCM_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_HPOUT_SHORTVCM_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_hpoutputreset0_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_HPOUTPUTRESET0_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_HPOUTPUTRESET0_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_hpinputreset0_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_HPINPUTRESET0_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_HPINPUTRESET0_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_hpoutputstbenh_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_HPOUTPUTSTBENH_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_HPOUTPUTSTBENH_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_hpinputstbenh_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_HPINPUTSTBENH_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_HPINPUTSTBENH_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_prechargebuf_en_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PRECHARGEBUF_EN_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_PRECHARGEBUF_EN_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audbgbon_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDBGBON_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDBGBON_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audhsstartup_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDHSSTARTUP_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDHSSTARTUP_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audhpstartup_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDHPSTARTUP_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDHPSTARTUP_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audhsbsccurrent_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDHSBSCCURRENT_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDHSBSCCURRENT_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audhprbsccurrent_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDHPRBSCCURRENT_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDHPRBSCCURRENT_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audhplbsccurrent_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDHPLBSCCURRENT_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDHPLBSCCURRENT_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_hsout_shortvcm_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_HSOUT_SHORTVCM_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_HSOUT_SHORTVCM_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_hpoutstb_rsel_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_HPOUTSTB_RSEL_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_HPOUTSTB_RSEL_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_hsoutputreset0_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_HSOUTPUTRESET0_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_HSOUTPUTRESET0_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_hsinputreset0_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_HSINPUTRESET0_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_HSINPUTRESET0_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_hsoutputstbenh_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_HSOUTPUTSTBENH_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_HSOUTPUTSTBENH_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_hsinputstbenh_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_HSINPUTSTBENH_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_HSINPUTSTBENH_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_line_pull0v_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LINE_PULL0V_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_LINE_PULL0V_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audhprfinetrim_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDHPRFINETRIM_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDHPRFINETRIM_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audhplfinetrim_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDHPLFINETRIM_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDHPLFINETRIM_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audhptrim_en_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDHPTRIM_EN_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDHPTRIM_EN_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audhprtrim_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDHPRTRIM_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDHPRTRIM_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audhpltrim_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDHPLTRIM_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDHPLTRIM_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_abidec_reserved_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ABIDEC_RESERVED_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_ABIDEC_RESERVED_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_abidec_reserved_va28(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDBUF_CFG4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ABIDEC_RESERVED_VA28_MASK),
				    (unsigned int) (PMIC_RG_ABIDEC_RESERVED_VA28_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audibiaspwrdn_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (IBIASDIST_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDIBIASPWRDN_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDIBIASPWRDN_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audbiasadj_1_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (IBIASDIST_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDBIASADJ_1_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDBIASADJ_1_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audbiasadj_0_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (IBIASDIST_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDBIASADJ_0_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDBIASADJ_0_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_chargeoption_depop_va28(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDACCDEPOP_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CHARGEOPTION_DEPOP_VA28_MASK),
				    (unsigned int) (PMIC_RG_CHARGEOPTION_DEPOP_VA28_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_depop_isel_va28(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDACCDEPOP_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DEPOP_ISEL_VA28_MASK),
				    (unsigned int) (PMIC_RG_DEPOP_ISEL_VA28_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_depop_vcmgen_en_va28(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDACCDEPOP_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DEPOP_VCMGEN_EN_VA28_MASK),
				    (unsigned int) (PMIC_RG_DEPOP_VCMGEN_EN_VA28_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_depop_rsel_va28(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDACCDEPOP_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DEPOP_RSEL_VA28_MASK),
				    (unsigned int) (PMIC_RG_DEPOP_RSEL_VA28_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_depop_ren_va28(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDACCDEPOP_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DEPOP_REN_VA28_MASK),
				    (unsigned int) (PMIC_RG_DEPOP_REN_VA28_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audivrmute_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUD_IV_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDIVRMUTE_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDIVRMUTE_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audivrmuxsel_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUD_IV_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDIVRMUXSEL_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDIVRMUXSEL_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audivrstartup_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUD_IV_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDIVRSTARTUP_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDIVRSTARTUP_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audivrpwrup_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUD_IV_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDIVRPWRUP_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDIVRPWRUP_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audivlmute_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUD_IV_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDIVLMUTE_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDIVLMUTE_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audivlmuxsel_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUD_IV_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDIVLMUXSEL_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDIVLMUXSEL_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audivlstartup_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUD_IV_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDIVLSTARTUP_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDIVLSTARTUP_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audivlpwrup_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUD_IV_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDIVLPWRUP_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDIVLPWRUP_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_sel_delay_vcore(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDCLKGEN_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SEL_DELAY_VCORE_MASK),
				    (unsigned int) (PMIC_RG_SEL_DELAY_VCORE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_sel_encoder_96k_va28(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDCLKGEN_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SEL_ENCODER_96K_VA28_MASK),
				    (unsigned int) (PMIC_RG_SEL_ENCODER_96K_VA28_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_sel_decoder_96k_va28(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDCLKGEN_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_SEL_DECODER_96K_VA28_MASK),
				    (unsigned int) (PMIC_RG_SEL_DECODER_96K_VA28_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_rstb_encoder_va28(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDCLKGEN_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_RSTB_ENCODER_VA28_MASK),
				    (unsigned int) (PMIC_RG_RSTB_ENCODER_VA28_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_rstb_decoder_va28(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDCLKGEN_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_RSTB_DECODER_VA28_MASK),
				    (unsigned int) (PMIC_RG_RSTB_DECODER_VA28_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_va28refgen_en_va28(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDLDO_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VA28REFGEN_EN_VA28_MASK),
				    (unsigned int) (PMIC_RG_VA28REFGEN_EN_VA28_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_va33refgen_en_va33(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDLDO_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VA33REFGEN_EN_VA33_MASK),
				    (unsigned int) (PMIC_RG_VA33REFGEN_EN_VA33_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vbatrefgen_en_vbat(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDLDO_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VBATREFGEN_EN_VBAT_MASK),
				    (unsigned int) (PMIC_RG_VBATREFGEN_EN_VBAT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_vbatprereg_pddis_en_vbat(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDLDO_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_VBATPREREG_PDDIS_EN_VBAT_MASK),
				    (unsigned int) (PMIC_RG_VBATPREREG_PDDIS_EN_VBAT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_lcldo_enc_remote_sense_va28(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDLDO_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LCLDO_ENC_REMOTE_SENSE_VA28_MASK),
				    (unsigned int) (PMIC_RG_LCLDO_ENC_REMOTE_SENSE_VA28_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_lcldo_enc_pddis_en_va28(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDLDO_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LCLDO_ENC_PDDIS_EN_VA28_MASK),
				    (unsigned int) (PMIC_RG_LCLDO_ENC_PDDIS_EN_VA28_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_lcldo_vosel_va33(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDLDO_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LCLDO_VOSEL_VA33_MASK),
				    (unsigned int) (PMIC_RG_LCLDO_VOSEL_VA33_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_lcldo_remote_sense_va33(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDLDO_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LCLDO_REMOTE_SENSE_VA33_MASK),
				    (unsigned int) (PMIC_RG_LCLDO_REMOTE_SENSE_VA33_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_lcldo_pddis_en_va33(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDLDO_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_LCLDO_PDDIS_EN_VA33_MASK),
				    (unsigned int) (PMIC_RG_LCLDO_PDDIS_EN_VA33_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_hcldo_vosel_va33(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDLDO_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_HCLDO_VOSEL_VA33_MASK),
				    (unsigned int) (PMIC_RG_HCLDO_VOSEL_VA33_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_hcldo_remote_sense_va33(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDLDO_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_HCLDO_REMOTE_SENSE_VA33_MASK),
				    (unsigned int) (PMIC_RG_HCLDO_REMOTE_SENSE_VA33_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_hcldo_pddis_en_va33(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDLDO_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_HCLDO_PDDIS_EN_VA33_MASK),
				    (unsigned int) (PMIC_RG_HCLDO_PDDIS_EN_VA33_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audpmu_reserved_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDLDO_CFG1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDPMU_RESERVED_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDPMU_RESERVED_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audpmu_reserved_va28(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDLDO_CFG1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDPMU_RESERVED_VA28_MASK),
				    (unsigned int) (PMIC_RG_AUDPMU_RESERVED_VA28_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audpmu_reserved_va33(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDLDO_CFG1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDPMU_RESERVED_VA33_MASK),
				    (unsigned int) (PMIC_RG_AUDPMU_RESERVED_VA33_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audpmu_reserved_vbat(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDLDO_CFG1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDPMU_RESERVED_VBAT_MASK),
				    (unsigned int) (PMIC_RG_AUDPMU_RESERVED_VBAT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_da_nvreg_en_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDNVREGGLB_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_DA_NVREG_EN_VAUDP12_MASK),
				    (unsigned int) (PMIC_DA_NVREG_EN_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_acc_dcc_sel_audglb_va28(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDNVREGGLB_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_ACC_DCC_SEL_AUDGLB_VA28_MASK),
				    (unsigned int) (PMIC_RG_ACC_DCC_SEL_AUDGLB_VA28_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audglb_pwrdn_va28(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDNVREGGLB_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDGLB_PWRDN_VA28_MASK),
				    (unsigned int) (PMIC_RG_AUDGLB_PWRDN_VA28_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_nvreg_pull0v_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDNVREGGLB_CFG0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_NVREG_PULL0V_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_NVREG_PULL0V_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_ncp_remote_sense_va18(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUD_NCP0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_NCP_REMOTE_SENSE_VA18_MASK),
				    (unsigned int) (PMIC_RG_NCP_REMOTE_SENSE_VA18_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_da_hcldo_en_va33(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUD_NCP0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_DA_HCLDO_EN_VA33_MASK),
				    (unsigned int) (PMIC_DA_HCLDO_EN_VA33_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_da_lcldo_en_va33(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUD_NCP0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_DA_LCLDO_EN_VA33_MASK),
				    (unsigned int) (PMIC_DA_LCLDO_EN_VA33_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_da_lcldo_enc_en_va28(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUD_NCP0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_DA_LCLDO_ENC_EN_VA28_MASK),
				    (unsigned int) (PMIC_DA_LCLDO_ENC_EN_VA28_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_da_vbatprereg_en_vbat(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUD_NCP0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_DA_VBATPREREG_EN_VBAT_MASK),
				    (unsigned int) (PMIC_DA_VBATPREREG_EN_VBAT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audpreampiddtest(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDPREAMP_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDPREAMPIDDTEST_MASK),
				    (unsigned int) (PMIC_RG_AUDPREAMPIDDTEST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audpreamprpgatest(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDPREAMP_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDPREAMPRPGATEST_MASK),
				    (unsigned int) (PMIC_RG_AUDPREAMPRPGATEST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audpreamplpgatest(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDPREAMP_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDPREAMPLPGATEST_MASK),
				    (unsigned int) (PMIC_RG_AUDPREAMPLPGATEST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audpreamprinputsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDPREAMP_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDPREAMPRINPUTSEL_MASK),
				    (unsigned int) (PMIC_RG_AUDPREAMPRINPUTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audpreamplinputsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDPREAMP_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDPREAMPLINPUTSEL_MASK),
				    (unsigned int) (PMIC_RG_AUDPREAMPLINPUTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audpreampron(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDPREAMP_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDPREAMPRON_MASK),
				    (unsigned int) (PMIC_RG_AUDPREAMPRON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audpreamplon(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDPREAMP_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDPREAMPLON_MASK),
				    (unsigned int) (PMIC_RG_AUDPREAMPLON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audadc3rdstagereset(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDADC3RDSTAGERESET_MASK),
				    (unsigned int) (PMIC_RG_AUDADC3RDSTAGERESET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audadc2ndstagereset(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDADC2NDSTAGERESET_MASK),
				    (unsigned int) (PMIC_RG_AUDADC2NDSTAGERESET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audadc2ndstageiddtest(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDADC2NDSTAGEIDDTEST_MASK),
				    (unsigned int) (PMIC_RG_AUDADC2NDSTAGEIDDTEST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audadc1ststageiddtest(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDADC1STSTAGEIDDTEST_MASK),
				    (unsigned int) (PMIC_RG_AUDADC1STSTAGEIDDTEST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audadcclksel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDADCCLKSEL_MASK),
				    (unsigned int) (PMIC_RG_AUDADCCLKSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audadcrinputsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDADCRINPUTSEL_MASK),
				    (unsigned int) (PMIC_RG_AUDADCRINPUTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audadclinputsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDADCLINPUTSEL_MASK),
				    (unsigned int) (PMIC_RG_AUDADCLINPUTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audadcrpwrup(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDADCRPWRUP_MASK),
				    (unsigned int) (PMIC_RG_AUDADCRPWRUP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audadclpwrup(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDADC_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDADCLPWRUP_MASK),
				    (unsigned int) (PMIC_RG_AUDADCLPWRUP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audrctunelsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDADC_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDRCTUNELSEL_MASK),
				    (unsigned int) (PMIC_RG_AUDRCTUNELSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audrctunel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDADC_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDRCTUNEL_MASK),
				    (unsigned int) (PMIC_RG_AUDRCTUNEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audadcffbypass(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDADC_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDADCFFBYPASS_MASK),
				    (unsigned int) (PMIC_RG_AUDADCFFBYPASS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audadcbypass(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDADC_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDADCBYPASS_MASK),
				    (unsigned int) (PMIC_RG_AUDADCBYPASS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audadcflashiddtest(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDADC_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDADCFLASHIDDTEST_MASK),
				    (unsigned int) (PMIC_RG_AUDADCFLASHIDDTEST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audadcrefbufiddtest(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDADC_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDADCREFBUFIDDTEST_MASK),
				    (unsigned int) (PMIC_RG_AUDADCREFBUFIDDTEST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audadcdaciddtest(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDADC_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDADCDACIDDTEST_MASK),
				    (unsigned int) (PMIC_RG_AUDADCDACIDDTEST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audadcdacfbcurrent(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDADC_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDADCDACFBCURRENT_MASK),
				    (unsigned int) (PMIC_RG_AUDADCDACFBCURRENT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audadcnodem(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDADC_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDADCNODEM_MASK),
				    (unsigned int) (PMIC_RG_AUDADCNODEM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audrctunersel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDADC_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDRCTUNERSEL_MASK),
				    (unsigned int) (PMIC_RG_AUDRCTUNERSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audrctuner(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDADC_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDRCTUNER_MASK),
				    (unsigned int) (PMIC_RG_AUDRCTUNER_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audadctestdata(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDADC_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDADCTESTDATA_MASK),
				    (unsigned int) (PMIC_RG_AUDADCTESTDATA_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audadcdacnrz(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDADC_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDADCDACNRZ_MASK),
				    (unsigned int) (PMIC_RG_AUDADCDACNRZ_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audadcfsreset(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDADC_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDADCFSRESET_MASK),
				    (unsigned int) (PMIC_RG_AUDADCFSRESET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audadcdactest(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDADC_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDADCDACTEST_MASK),
				    (unsigned int) (PMIC_RG_AUDADCDACTEST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audadcnopatest(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDADC_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDADCNOPATEST_MASK),
				    (unsigned int) (PMIC_RG_AUDADCNOPATEST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audadcwidecm(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDADC_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDADCWIDECM_MASK),
				    (unsigned int) (PMIC_RG_AUDADCWIDECM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audspareva18(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDADC_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDSPAREVA18_MASK),
				    (unsigned int) (PMIC_RG_AUDSPAREVA18_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audspareva28(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDADC_CON5),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDSPAREVA28_MASK),
				    (unsigned int) (PMIC_RG_AUDSPAREVA28_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audsparevaudp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDADC_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDSPAREVAUDP_MASK),
				    (unsigned int) (PMIC_RG_AUDSPAREVAUDP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audsparevmic(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDADC_CON6),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDSPAREVMIC_MASK),
				    (unsigned int) (PMIC_RG_AUDSPAREVMIC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audmicbiasvref(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDDIGMI_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDMICBIASVREF_MASK),
				    (unsigned int) (PMIC_RG_AUDMICBIASVREF_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audpwdbmicbias(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDDIGMI_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDPWDBMICBIAS_MASK),
				    (unsigned int) (PMIC_RG_AUDPWDBMICBIAS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auddigmicbias(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDDIGMI_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDDIGMICBIAS_MASK),
				    (unsigned int) (PMIC_RG_AUDDIGMICBIAS_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auddigmicnduty(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDDIGMI_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDDIGMICNDUTY_MASK),
				    (unsigned int) (PMIC_RG_AUDDIGMICNDUTY_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auddigmicpduty(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDDIGMI_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDDIGMICPDUTY_MASK),
				    (unsigned int) (PMIC_RG_AUDDIGMICPDUTY_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_auddigmicen(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDDIGMI_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDDIGMICEN_MASK),
				    (unsigned int) (PMIC_RG_AUDDIGMICEN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audlsbufrmute(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDLSBUF_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDLSBUFRMUTE_MASK),
				    (unsigned int) (PMIC_RG_AUDLSBUFRMUTE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audlsbufrgain(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDLSBUF_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDLSBUFRGAIN_MASK),
				    (unsigned int) (PMIC_RG_AUDLSBUFRGAIN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audlsbuflmute(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDLSBUF_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDLSBUFLMUTE_MASK),
				    (unsigned int) (PMIC_RG_AUDLSBUFLMUTE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audlsbuflgain(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDLSBUF_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDLSBUFLGAIN_MASK),
				    (unsigned int) (PMIC_RG_AUDLSBUFLGAIN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audlsbufrpwrup(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDLSBUF_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDLSBUFRPWRUP_MASK),
				    (unsigned int) (PMIC_RG_AUDLSBUFRPWRUP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audlsbuflpwrup(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDLSBUF_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDLSBUFLPWRUP_MASK),
				    (unsigned int) (PMIC_RG_AUDLSBUFLPWRUP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audlsbuf2iddtest(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDLSBUF_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDLSBUF2IDDTEST_MASK),
				    (unsigned int) (PMIC_RG_AUDLSBUF2IDDTEST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audlsbufiddtest(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDLSBUF_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDLSBUFIDDTEST_MASK),
				    (unsigned int) (PMIC_RG_AUDLSBUFIDDTEST_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audlsbufrinputsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDLSBUF_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDLSBUFRINPUTSEL_MASK),
				    (unsigned int) (PMIC_RG_AUDLSBUFRINPUTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audlsbuflinputsel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDLSBUF_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDLSBUFLINPUTSEL_MASK),
				    (unsigned int) (PMIC_RG_AUDLSBUFLINPUTSEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audencspareva18(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDENCSPARE_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDENCSPAREVA18_MASK),
				    (unsigned int) (PMIC_RG_AUDENCSPAREVA18_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audencspareva28(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDENCSPARE_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDENCSPAREVA28_MASK),
				    (unsigned int) (PMIC_RG_AUDENCSPAREVA28_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_clksq_monen_va28(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDENCCLKSQ_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_CLKSQ_MONEN_VA28_MASK),
				    (unsigned int) (PMIC_RG_CLKSQ_MONEN_VA28_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audenc_reserved(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDPREAMPGAIN_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDENC_reserved_MASK),
				    (unsigned int) (PMIC_RG_AUDENC_reserved_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audpreampr_reserved(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDPREAMPGAIN_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDPREAMPR_reserved_MASK),
				    (unsigned int) (PMIC_RG_AUDPREAMPR_reserved_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audpreamprgain(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDPREAMPGAIN_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDPREAMPRGAIN_MASK),
				    (unsigned int) (PMIC_RG_AUDPREAMPRGAIN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audpreampl_reserved(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDPREAMPGAIN_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDPREAMPL_reserved_MASK),
				    (unsigned int) (PMIC_RG_AUDPREAMPL_reserved_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audpreamplgain(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (AUDPREAMPGAIN_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDPREAMPLGAIN_MASK),
				    (unsigned int) (PMIC_RG_AUDPREAMPLGAIN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audzcdmuxsel_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ZCD_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDZCDMUXSEL_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDZCDMUXSEL_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audzcdclksel_vaudp12(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ZCD_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDZCDCLKSEL_VAUDP12_MASK),
				    (unsigned int) (PMIC_RG_AUDZCDCLKSEL_VAUDP12_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audzcdtimeoutmodesel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ZCD_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDZCDTIMEOUTMODESEL_MASK),
				    (unsigned int) (PMIC_RG_AUDZCDTIMEOUTMODESEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audzcdgainstepsize(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ZCD_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDZCDGAINSTEPSIZE_MASK),
				    (unsigned int) (PMIC_RG_AUDZCDGAINSTEPSIZE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audzcdgainsteptime(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ZCD_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDZCDGAINSTEPTIME_MASK),
				    (unsigned int) (PMIC_RG_AUDZCDGAINSTEPTIME_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audzcdenable(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ZCD_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDZCDENABLE_MASK),
				    (unsigned int) (PMIC_RG_AUDZCDENABLE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audlinegain(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ZCD_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDLINEGAIN_MASK),
				    (unsigned int) (PMIC_RG_AUDLINEGAIN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audhprgain(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ZCD_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDHPRGAIN_MASK),
				    (unsigned int) (PMIC_RG_AUDHPRGAIN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audhplgain(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ZCD_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDHPLGAIN_MASK),
				    (unsigned int) (PMIC_RG_AUDHPLGAIN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audhsgain(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ZCD_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDHSGAIN_MASK), (unsigned int) (PMIC_RG_AUDHSGAIN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audivrgain(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ZCD_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDIVRGAIN_MASK),
				    (unsigned int) (PMIC_RG_AUDIVRGAIN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_audivlgain(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (ZCD_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_AUDIVLGAIN_MASK),
				    (unsigned int) (PMIC_RG_AUDIVLGAIN_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rg_audintgain2(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ZCD_CON5),
				  (&val),
				  (unsigned int) (PMIC_RG_AUDINTGAIN2_MASK),
				  (unsigned int) (PMIC_RG_AUDINTGAIN2_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rg_audintgain1(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (ZCD_CON5),
				  (&val),
				  (unsigned int) (PMIC_RG_AUDINTGAIN1_MASK),
				  (unsigned int) (PMIC_RG_AUDINTGAIN1_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline void upmu_set_rg_divcks_chg(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (NCP_CLKDIV_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DIVCKS_CHG_MASK),
				    (unsigned int) (PMIC_RG_DIVCKS_CHG_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_divcks_on(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (NCP_CLKDIV_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DIVCKS_ON_MASK), (unsigned int) (PMIC_RG_DIVCKS_ON_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_divcks_prg(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (NCP_CLKDIV_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DIVCKS_PRG_MASK),
				    (unsigned int) (PMIC_RG_DIVCKS_PRG_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_pwd_ncp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (NCP_CLKDIV_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_PWD_NCP_MASK), (unsigned int) (PMIC_RG_PWD_NCP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_loadc2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_LOADC2_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_LOADC2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_ldo_afe_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_LDO_AFE_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_LDO_AFE_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_ldo_bb_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_LDO_BB_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_LDO_BB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_rf2_rctb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_RF2_RCTB_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_RF2_RCTB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_ldo_rf2_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_LDO_RF2_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_LDO_RF2_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_rf1_rctb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_RF1_RCTB_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_RF1_RCTB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_ldo_rf1_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_LDO_RF1_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_LDO_RF1_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_acl_target(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_ACL_TARGET_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_ACL_TARGET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_acl_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_ACL_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_ACL_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_ldo_top_byp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_LDO_TOP_BYP_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_LDO_TOP_BYP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_ldo_top_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_LDO_TOP_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_LDO_TOP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_bandgap_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_BANDGAP_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_BANDGAP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_TMP1_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_RSV_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_tmp1_c2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_TMP1_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP1_C2_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP1_C2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_tmp1_c1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_TMP1_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP1_C1_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP1_C1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_tmp1_tielow_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_TMP1_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP1_TIELOW_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP1_TIELOW_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_tmp1_prebuf_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_TMP1_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP1_PREBUF_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP1_PREBUF_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_tmp1_ldo_fpm_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_TMP1_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP1_LDO_FPM_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP1_LDO_FPM_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_tmp1_fpmbuf_bias_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_TMP1_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP1_FPMBUF_BIAS_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP1_FPMBUF_BIAS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_tmp1_cap_low(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_TMP1_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP1_CAP_LOW_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP1_CAP_LOW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_tmp1_cap_high(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_TMP1_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP1_CAP_HIGH_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP1_CAP_HIGH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_tmp1_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_TMP1_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP1_RSV_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP1_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_tmp1_icont(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_TMP1_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP1_ICONT_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP1_ICONT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_tmp2_c2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_TMP2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP2_C2_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP2_C2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_tmp2_c1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_TMP2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP2_C1_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP2_C1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_tmp2_tielow_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_TMP2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP2_TIELOW_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP2_TIELOW_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_tmp2_prebuf_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_TMP2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP2_PREBUF_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP2_PREBUF_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_tmp2_ldo_fpm_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_TMP2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP2_LDO_FPM_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP2_LDO_FPM_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_tmp2_fpmbuf_bias_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_TMP2_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP2_FPMBUF_BIAS_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP2_FPMBUF_BIAS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_tmp2_cap_low(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_TMP2_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP2_CAP_LOW_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP2_CAP_LOW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_tmp2_cap_high(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_TMP2_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP2_CAP_HIGH_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP2_CAP_HIGH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_tmp2_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_TMP2_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP2_RSV_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP2_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_tmp2_icont(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_TMP2_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP2_ICONT_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP2_ICONT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_tmp3_c2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_TMP3_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP3_C2_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP3_C2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_tmp3_c1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_TMP3_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP3_C1_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP3_C1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_tmp3_tielow_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_TMP3_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP3_TIELOW_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP3_TIELOW_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_tmp3_prebuf_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_TMP3_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP3_PREBUF_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP3_PREBUF_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_tmp3_ldo_fpm_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_TMP3_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP3_LDO_FPM_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP3_LDO_FPM_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_tmp3_fpmbuf_bias_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_TMP3_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP3_FPMBUF_BIAS_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP3_FPMBUF_BIAS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_tmp3_cap_low(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_TMP3_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP3_CAP_LOW_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP3_CAP_LOW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_tmp3_cap_high(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_TMP3_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP3_CAP_HIGH_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP3_CAP_HIGH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_tmp3_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_TMP3_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP3_RSV_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP3_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_tmp3_icont(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_TMP3_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP3_ICONT_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_TMP3_ICONT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_final_c2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_FINAL_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_FINAL_C2_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_FINAL_C2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_final_c1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_FINAL_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_FINAL_C1_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_FINAL_C1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_final_tielow_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_FINAL_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_FINAL_TIELOW_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_FINAL_TIELOW_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_final_prebuf_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_FINAL_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_FINAL_PREBUF_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_FINAL_PREBUF_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_final_ldo_fpm_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_FINAL_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_FINAL_LDO_FPM_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_FINAL_LDO_FPM_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_final_fpmbuf_bias_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_FINAL_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_FINAL_FPMBUF_BIAS_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_FINAL_FPMBUF_BIAS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_final_cap_low(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_FINAL_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_FINAL_CAP_LOW_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_FINAL_CAP_LOW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_final_cap_high(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_FINAL_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_FINAL_CAP_HIGH_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_FINAL_CAP_HIGH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_final_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_FINAL_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_FINAL_RSV_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_FINAL_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_final_icont(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_FINAL_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_FINAL_ICONT_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_FINAL_ICONT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_loadc2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_LOADC2_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_LOADC2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_ldo_afe_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_LDO_AFE_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_LDO_AFE_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_ldo_bb_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_LDO_BB_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_LDO_BB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_rf2_rctb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_RF2_RCTB_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_RF2_RCTB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_ldo_rf2_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_LDO_RF2_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_LDO_RF2_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_rf1_rctb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_RF1_RCTB_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_RF1_RCTB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_ldo_rf1_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_LDO_RF1_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_LDO_RF1_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_acl_target(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_ACL_TARGET_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_ACL_TARGET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_acl_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_ACL_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_ACL_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_ldo_top_byp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_LDO_TOP_BYP_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_LDO_TOP_BYP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_ldo_top_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_LDO_TOP_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_LDO_TOP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_bandgap_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_BANDGAP_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_BANDGAP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_TMP1_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_RSV_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_tmp1_c2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_TMP1_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP1_C2_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP1_C2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_tmp1_c1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_TMP1_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP1_C1_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP1_C1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_tmp1_tielow_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_TMP1_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP1_TIELOW_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP1_TIELOW_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_tmp1_prebuf_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_TMP1_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP1_PREBUF_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP1_PREBUF_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_tmp1_ldo_fpm_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_TMP1_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP1_LDO_FPM_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP1_LDO_FPM_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_tmp1_fpmbuf_bias_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_TMP1_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP1_FPMBUF_BIAS_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP1_FPMBUF_BIAS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_tmp1_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_TMP1_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP1_RSV_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP1_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_tmp1_icont(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_TMP1_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP1_ICONT_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP1_ICONT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_tmp2_c2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_TMP2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP2_C2_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP2_C2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_tmp2_c1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_TMP2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP2_C1_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP2_C1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_tmp2_tielow_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_TMP2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP2_TIELOW_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP2_TIELOW_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_tmp2_prebuf_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_TMP2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP2_PREBUF_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP2_PREBUF_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_tmp2_ldo_fpm_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_TMP2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP2_LDO_FPM_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP2_LDO_FPM_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_tmp2_fpmbuf_bias_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_TMP2_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP2_FPMBUF_BIAS_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP2_FPMBUF_BIAS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_tmp2_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_TMP2_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP2_RSV_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP2_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_tmp2_icont(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_TMP2_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP2_ICONT_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP2_ICONT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_tmp3_c2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_TMP3_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP3_C2_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP3_C2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_tmp3_c1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_TMP3_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP3_C1_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP3_C1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_tmp3_tielow_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_TMP3_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP3_TIELOW_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP3_TIELOW_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_tmp3_prebuf_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_TMP3_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP3_PREBUF_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP3_PREBUF_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_tmp3_ldo_fpm_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_TMP3_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP3_LDO_FPM_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP3_LDO_FPM_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_tmp3_fpmbuf_bias_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_TMP3_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP3_FPMBUF_BIAS_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP3_FPMBUF_BIAS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_tmp3_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_TMP3_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP3_RSV_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP3_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_tmp3_icont(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_TMP3_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP3_ICONT_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_TMP3_ICONT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_final_c2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_FINAL_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_FINAL_C2_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_FINAL_C2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_final_c1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_FINAL_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_FINAL_C1_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_FINAL_C1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_final_tielow_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_FINAL_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_FINAL_TIELOW_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_FINAL_TIELOW_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_final_prebuf_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_FINAL_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_FINAL_PREBUF_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_FINAL_PREBUF_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_final_ldo_fpm_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_FINAL_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_FINAL_LDO_FPM_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_FINAL_LDO_FPM_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_final_fpmbuf_bias_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_FINAL_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_FINAL_FPMBUF_BIAS_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_FINAL_FPMBUF_BIAS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_final_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_FINAL_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_FINAL_RSV_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_FINAL_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_final_icont(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_FINAL_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_FINAL_ICONT_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_FINAL_ICONT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_ldo_rf2_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_LDO_RF2_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_LDO_RF2_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_rf1_rctb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_RF1_RCTB_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_RF1_RCTB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_ldo_rf1_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_LDO_RF1_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_LDO_RF1_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_acl_target(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_ACL_TARGET_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_ACL_TARGET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_acl_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_ACL_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_ACL_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_tielow_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TIELOW_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TIELOW_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_digbuf_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_DIGBUF_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_DIGBUF_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_startup_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_STARTUP_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_STARTUP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_current_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_CURRENT_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_CURRENT_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_ldo_top_byp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_LDO_TOP_BYP_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_LDO_TOP_BYP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_ldo_top_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_LDO_TOP_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_LDO_TOP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_bandgap_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_BANDGAP_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_BANDGAP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_reset(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_RESET_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_RESET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_c2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_C2_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_C2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_RSV_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_prebuf_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_PREBUF_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_PREBUF_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_ldo_fpm_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_LDO_FPM_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_LDO_FPM_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_loadc2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_LOADC2_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_LOADC2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_ldo_afe_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_LDO_AFE_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_LDO_AFE_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_ldo_bb_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_LDO_BB_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_LDO_BB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_rf2_rctb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_RF2_RCTB_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_RF2_RCTB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_fpmbuf_bias_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_FPMBUF_BIAS_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_FPMBUF_BIAS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_ldo_buftop_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_LDO_BUFTOP_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_LDO_BUFTOP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_ldo_fpm_rctb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_LDO_FPM_RCTB_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_LDO_FPM_RCTB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_bandgap_rctb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_BANDGAP_RCTB_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_BANDGAP_RCTB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_current_rctb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_CURRENT_RCTB_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_CURRENT_RCTB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_c1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_C1_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_C1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_icont(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_ICONT_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_ICONT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_tmp1_tielow_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_TMP1_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP1_TIELOW_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP1_TIELOW_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_tmp1_c2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_TMP1_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP1_C2_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP1_C2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_tmp1_c1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_TMP1_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP1_C1_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP1_C1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_tmp1_prebuf_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_TMP1_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP1_PREBUF_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP1_PREBUF_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_tmp1_ldo_fpm_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_TMP1_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP1_LDO_FPM_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP1_LDO_FPM_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_tmp1_fpmbuf_bias_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_TMP1_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP1_FPMBUF_BIAS_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP1_FPMBUF_BIAS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_tmp1_cap_low(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_TMP1_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP1_CAP_LOW_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP1_CAP_LOW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_tmp1_cap_high(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_TMP1_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP1_CAP_HIGH_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP1_CAP_HIGH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_tmp1_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_TMP1_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP1_RSV_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP1_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_tmp1_icont(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_TMP1_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP1_ICONT_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP1_ICONT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_tmp2_tielow_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_TMP2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP2_TIELOW_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP2_TIELOW_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_tmp2_c2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_TMP2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP2_C2_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP2_C2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_tmp2_c1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_TMP2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP2_C1_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP2_C1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_tmp2_prebuf_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_TMP2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP2_PREBUF_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP2_PREBUF_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_tmp2_ldo_fpm_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_TMP2_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP2_LDO_FPM_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP2_LDO_FPM_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_tmp2_fpmbuf_bias_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_TMP2_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP2_FPMBUF_BIAS_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP2_FPMBUF_BIAS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_tmp2_cap_low(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_TMP2_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP2_CAP_LOW_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP2_CAP_LOW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_tmp2_cap_high(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_TMP2_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP2_CAP_HIGH_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP2_CAP_HIGH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_tmp2_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_TMP2_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP2_RSV_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP2_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_tmp2_icont(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_TMP2_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP2_ICONT_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP2_ICONT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_tmp3_tielow_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_TMP3_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP3_TIELOW_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP3_TIELOW_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_tmp3_c2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_TMP3_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP3_C2_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP3_C2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_tmp3_c1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_TMP3_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP3_C1_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP3_C1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_tmp3_prebuf_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_TMP3_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP3_PREBUF_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP3_PREBUF_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_tmp3_ldo_fpm_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_TMP3_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP3_LDO_FPM_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP3_LDO_FPM_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_tmp3_fpmbuf_bias_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_TMP3_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP3_FPMBUF_BIAS_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP3_FPMBUF_BIAS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_tmp3_cap_low(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_TMP3_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP3_CAP_LOW_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP3_CAP_LOW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_tmp3_cap_high(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_TMP3_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP3_CAP_HIGH_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP3_CAP_HIGH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_tmp3_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_TMP3_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP3_RSV_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP3_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_tmp3_icont(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_TMP3_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP3_ICONT_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_TMP3_ICONT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_final_tielow_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_FINAL_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_FINAL_TIELOW_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_FINAL_TIELOW_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_final_c2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_FINAL_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_FINAL_C2_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_FINAL_C2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_final_c1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_FINAL_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_FINAL_C1_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_FINAL_C1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_final_prebuf_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_FINAL_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_FINAL_PREBUF_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_FINAL_PREBUF_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_final_ldo_fpm_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_FINAL_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_FINAL_LDO_FPM_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_FINAL_LDO_FPM_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_final_fpmbuf_bias_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_FINAL_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_FINAL_FPMBUF_BIAS_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_FINAL_FPMBUF_BIAS_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_final_cap_low(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_FINAL_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_FINAL_CAP_LOW_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_FINAL_CAP_LOW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_final_cap_high(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_FINAL_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_FINAL_CAP_HIGH_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_FINAL_CAP_HIGH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_final_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_FINAL_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_FINAL_RSV_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_FINAL_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_final_icont(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_FINAL_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_FINAL_ICONT_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_FINAL_ICONT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ldo_buftop_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_LDO_BUFTOP_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_LDO_BUFTOP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ldo_rf2_reg_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_LDO_RF2_REG_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_LDO_RF2_REG_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ldo_rf1_reg_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_LDO_RF1_REG_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_LDO_RF1_REG_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ldo_dbb_reg_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_LDO_DBB_REG_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_LDO_DBB_REG_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_c1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_C1_MASK), (unsigned int) (PMIC_RG_DCXO_C1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_prebuf_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_PREBUF_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_PREBUF_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ldo_fpm_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_LDO_FPM_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_LDO_FPM_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_digbuf_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_DIGBUF_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_DIGBUF_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_startup_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_STARTUP_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_STARTUP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_current_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_CURRENT_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_CURRENT_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ldo_top_byp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_LDO_TOP_BYP_MASK),
				    (unsigned int) (PMIC_RG_DCXO_LDO_TOP_BYP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ldo_top_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_LDO_TOP_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_LDO_TOP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_bandgap_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_BANDGAP_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_BANDGAP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_reset(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_RESET_MASK),
				    (unsigned int) (PMIC_RG_DCXO_RESET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_icont(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_ICONT_MASK),
				    (unsigned int) (PMIC_RG_DCXO_ICONT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_c2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_C2_MASK), (unsigned int) (PMIC_RG_DCXO_C2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_c2_untrim(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_C2_UNTRIM_MASK),
				    (unsigned int) (PMIC_RG_DCXO_C2_UNTRIM_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_RSV_MASK), (unsigned int) (PMIC_RG_DCXO_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_acl_target(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_ACL_TARGET_MASK),
				    (unsigned int) (PMIC_RG_DCXO_ACL_TARGET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_acl_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_ACL_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_ACL_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ldo_afe_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_LDO_AFE_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_LDO_AFE_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ldo_bb_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_LDO_BB_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_LDO_BB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ldo_rf2_rctb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_LDO_RF2_RCTB_MASK),
				    (unsigned int) (PMIC_RG_DCXO_LDO_RF2_RCTB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ldo_rf2_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_LDO_RF2_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_LDO_RF2_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ldo_rf1_rctb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_LDO_RF1_RCTB_MASK),
				    (unsigned int) (PMIC_RG_DCXO_LDO_RF1_RCTB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ldo_rf1_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_LDO_RF1_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_LDO_RF1_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_tielow_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_TIELOW_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_TIELOW_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ldo_fpm_rctb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_LDO_FPM_RCTB_MASK),
				    (unsigned int) (PMIC_RG_DCXO_LDO_FPM_RCTB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_bandgap_rctb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_BANDGAP_RCTB_MASK),
				    (unsigned int) (PMIC_RG_DCXO_BANDGAP_RCTB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_current_rctb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_CURRENT_RCTB_MASK),
				    (unsigned int) (PMIC_RG_DCXO_CURRENT_RCTB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_fpmbuf_bias_en_sync(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_FPMBUF_BIAS_EN_SYNC_MASK),
				    (unsigned int) (PMIC_RG_DCXO_FPMBUF_BIAS_EN_SYNC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_cap_low_sync(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_CAP_LOW_SYNC_MASK),
				    (unsigned int) (PMIC_RG_DCXO_CAP_LOW_SYNC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_cap_high_sync(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_CAP_HIGH_SYNC_MASK),
				    (unsigned int) (PMIC_RG_DCXO_CAP_HIGH_SYNC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_c1c2_sync_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_C1C2_SYNC_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_C1C2_SYNC_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_sync_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_SYNC_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_SYNC_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_fsm_c2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_FSM_C2_MASK),
				    (unsigned int) (PMIC_RG_DCXO_FSM_C2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_ldo_rf2_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_MANUAL_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_LDO_RF2_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_LDO_RF2_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_ldo_rf1_rctb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_MANUAL_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_LDO_RF1_RCTB_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_LDO_RF1_RCTB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_ldo_rf1_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_MANUAL_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_LDO_RF1_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_LDO_RF1_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_tielow_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_MANUAL_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_TIELOW_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_TIELOW_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_icont(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_MANUAL_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_ICONT_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_ICONT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_c2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_MANUAL_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_C2_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_C2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_c1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_MANUAL_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_C1_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_C1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_prebuf_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_MANUAL_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_PREBUF_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_PREBUF_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_ldo_fpm_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_MANUAL_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_LDO_FPM_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_LDO_FPM_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_digbuf_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_MANUAL_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_DIGBUF_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_DIGBUF_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_startup_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_MANUAL_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_STARTUP_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_STARTUP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_current_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_MANUAL_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_CURRENT_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_CURRENT_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_ldo_top_byp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_MANUAL_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_LDO_TOP_BYP_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_LDO_TOP_BYP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_ldo_top_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_MANUAL_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_LDO_TOP_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_LDO_TOP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_bandgap_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_MANUAL_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_BANDGAP_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_BANDGAP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_reset(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_MANUAL_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_RESET_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_RESET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_ldo_fpm_rctb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_MANUAL_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_LDO_FPM_RCTB_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_LDO_FPM_RCTB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_bandgap_rctb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_MANUAL_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_BANDGAP_RCTB_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_BANDGAP_RCTB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_current_rctb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_MANUAL_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_CURRENT_RCTB_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_CURRENT_RCTB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_fpmbuf_bias_en_sync(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_MANUAL_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_FPMBUF_BIAS_EN_SYNC_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_FPMBUF_BIAS_EN_SYNC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_cap_low_sync(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_MANUAL_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_CAP_LOW_SYNC_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_CAP_LOW_SYNC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_cap_high_sync(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_MANUAL_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_CAP_HIGH_SYNC_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_CAP_HIGH_SYNC_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_c1c2_sync_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_MANUAL_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_C1C2_SYNC_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_C1C2_SYNC_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_sync_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_MANUAL_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_SYNC_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_SYNC_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_rsv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_MANUAL_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_RSV_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_RSV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_acl_target(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_MANUAL_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_ACL_TARGET_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_ACL_TARGET_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_acl_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_MANUAL_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_ACL_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_ACL_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_ldo_afe_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_MANUAL_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_LDO_AFE_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_LDO_AFE_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_ldo_bb_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_MANUAL_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_LDO_BB_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_LDO_BB_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_ldo_rf2_rctb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_MANUAL_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_LDO_RF2_RCTB_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_LDO_RF2_RCTB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_ldo_buftop_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_MANUAL_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_LDO_BUFTOP_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_LDO_BUFTOP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ldo_rf1_v(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_LDO_RF1_V_MASK),
				    (unsigned int) (PMIC_RG_DCXO_LDO_RF1_V_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ldo_lpm_v(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_LDO_LPM_V_MASK),
				    (unsigned int) (PMIC_RG_DCXO_LDO_LPM_V_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ldo_fpm_v(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_LDO_FPM_V_MASK),
				    (unsigned int) (PMIC_RG_DCXO_LDO_FPM_V_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ldo_top_v(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_LDO_TOP_V_MASK),
				    (unsigned int) (PMIC_RG_DCXO_LDO_TOP_V_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_vg_v(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_VG_V_MASK), (unsigned int) (PMIC_RG_DCXO_VG_V_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ldo_afe_byp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_LDO_AFE_BYP_MASK),
				    (unsigned int) (PMIC_RG_DCXO_LDO_AFE_BYP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ldo_bb_byp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_LDO_BB_BYP_MASK),
				    (unsigned int) (PMIC_RG_DCXO_LDO_BB_BYP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ldo_rf2_byp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_LDO_RF2_BYP_MASK),
				    (unsigned int) (PMIC_RG_DCXO_LDO_RF2_BYP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ldo_rf1_byp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_LDO_RF1_BYP_MASK),
				    (unsigned int) (PMIC_RG_DCXO_LDO_RF1_BYP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ldo_lpm_byp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_LDO_LPM_BYP_MASK),
				    (unsigned int) (PMIC_RG_DCXO_LDO_LPM_BYP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ldo_lpm_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_LDO_LPM_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_LDO_LPM_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_test_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_TEST_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_TEST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_atten_afe(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_ATTEN_AFE_MASK),
				    (unsigned int) (PMIC_RG_DCXO_ATTEN_AFE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_atten_bb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_ATTEN_BB_MASK),
				    (unsigned int) (PMIC_RG_DCXO_ATTEN_BB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_atten_rf2(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_ATTEN_RF2_MASK),
				    (unsigned int) (PMIC_RG_DCXO_ATTEN_RF2_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_atten_rf1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_ATTEN_RF1_MASK),
				    (unsigned int) (PMIC_RG_DCXO_ATTEN_RF1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ldo_afe_v(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_LDO_AFE_V_MASK),
				    (unsigned int) (PMIC_RG_DCXO_LDO_AFE_V_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ldo_bb_v(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_LDO_BB_V_MASK),
				    (unsigned int) (PMIC_RG_DCXO_LDO_BB_V_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ldo_rf2_v(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_LDO_RF2_V_MASK),
				    (unsigned int) (PMIC_RG_DCXO_LDO_RF2_V_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_test_sel(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_TEST_SEL_MASK),
				    (unsigned int) (PMIC_RG_DCXO_TEST_SEL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_float_cap(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_FLOAT_CAP_MASK),
				    (unsigned int) (PMIC_RG_DCXO_FLOAT_CAP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_sync_ckinv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_SYNC_CKINV_MASK),
				    (unsigned int) (PMIC_RG_DCXO_SYNC_CKINV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_sync_byp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_SYNC_BYP_MASK),
				    (unsigned int) (PMIC_RG_DCXO_SYNC_BYP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ldo_fpm_byp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_LDO_FPM_BYP_MASK),
				    (unsigned int) (PMIC_RG_DCXO_LDO_FPM_BYP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ldo_buftop_v(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_LDO_BUFTOP_V_MASK),
				    (unsigned int) (PMIC_RG_DCXO_LDO_BUFTOP_V_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ldo_buftop_byp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_LDO_BUFTOP_BYP_MASK),
				    (unsigned int) (PMIC_RG_DCXO_LDO_BUFTOP_BYP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_audio_test_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_AUDIO_TEST_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_AUDIO_TEST_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_reserved(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_RESERVED_MASK),
				    (unsigned int) (PMIC_RG_DCXO_RESERVED_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_reserved1(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_RESERVED1_MASK),
				    (unsigned int) (PMIC_RG_DCXO_RESERVED1_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_reserved0(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_RESERVED0_MASK),
				    (unsigned int) (PMIC_RG_DCXO_RESERVED0_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_fpmbuf_biasr(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_FPMBUF_BIASR_MASK),
				    (unsigned int) (PMIC_RG_DCXO_FPMBUF_BIASR_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_afe_ckinv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_AFE_CKINV_MASK),
				    (unsigned int) (PMIC_RG_DCXO_AFE_CKINV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_bb_ckinv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_BB_CKINV_MASK),
				    (unsigned int) (PMIC_RG_DCXO_BB_CKINV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_rf2_ckinv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_RF2_CKINV_MASK),
				    (unsigned int) (PMIC_RG_DCXO_RF2_CKINV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_bufmode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_BUFMODE_MASK),
				    (unsigned int) (PMIC_RG_DCXO_BUFMODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_c1c2_sync_ckinv(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_C1C2_SYNC_CKINV_MASK),
				    (unsigned int) (PMIC_RG_DCXO_C1C2_SYNC_CKINV_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_c1c2_sync_byp(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_ANALOG_CON4),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_C1C2_SYNC_BYP_MASK),
				    (unsigned int) (PMIC_RG_DCXO_C1C2_SYNC_BYP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ft_trim_option(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_FORCE_MODE0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_FT_TRIM_OPTION_MASK),
				    (unsigned int) (PMIC_RG_DCXO_FT_TRIM_OPTION_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ft_freq_trim_start(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_FORCE_MODE0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_FT_FREQ_TRIM_START_MASK),
				    (unsigned int) (PMIC_RG_DCXO_FT_FREQ_TRIM_START_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_acl_bit_swap(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_FORCE_MODE0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_ACL_BIT_SWAP_MASK),
				    (unsigned int) (PMIC_RG_DCXO_ACL_BIT_SWAP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_acl_bit_end(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_FORCE_MODE0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_ACL_BIT_END_MASK),
				    (unsigned int) (PMIC_RG_DCXO_ACL_BIT_END_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_acl_bit_start(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_FORCE_MODE0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_ACL_BIT_START_MASK),
				    (unsigned int) (PMIC_RG_DCXO_ACL_BIT_START_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_manual_acl_start(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_FORCE_MODE0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_ACL_START_MASK),
				    (unsigned int) (PMIC_RG_DCXO_MANUAL_ACL_START_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_off_32k_output(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_FORCE_MODE0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_OFF_32K_OUTPUT_MASK),
				    (unsigned int) (PMIC_RG_DCXO_OFF_32K_OUTPUT_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_test_sleep_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_FORCE_MODE0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_TEST_SLEEP_MODE_MASK),
				    (unsigned int) (PMIC_RG_DCXO_TEST_SLEEP_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_test_mode(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_FORCE_MODE0),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_TEST_MODE_MASK),
				    (unsigned int) (PMIC_RG_DCXO_TEST_MODE_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ck_chg_delay(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_FORCE_MODE1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_CK_CHG_DELAY_MASK),
				    (unsigned int) (PMIC_RG_DCXO_CK_CHG_DELAY_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ft_freq_trim_step_start(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_FORCE_MODE1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_FT_FREQ_TRIM_STEP_START_MASK),
				    (unsigned int) (PMIC_RG_DCXO_FT_FREQ_TRIM_STEP_START_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ft_freq_trim_manual(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_FORCE_MODE1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_FT_FREQ_TRIM_MANUAL_MASK),
				    (unsigned int) (PMIC_RG_DCXO_FT_FREQ_TRIM_MANUAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ft_trim_bit_end(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_FORCE_MODE1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_FT_TRIM_BIT_END_MASK),
				    (unsigned int) (PMIC_RG_DCXO_FT_TRIM_BIT_END_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ft_trim_bit_start(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_FORCE_MODE1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_FT_TRIM_BIT_START_MASK),
				    (unsigned int) (PMIC_RG_DCXO_FT_TRIM_BIT_START_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_32k_residual(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_FORCE_MODE2),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_32K_RESIDUAL_MASK),
				    (unsigned int) (PMIC_RG_DCXO_32K_RESIDUAL_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_ldo_buftop_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_LDO_BUFTOP_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_LDO_BUFTOP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_ldo_fpm_rctb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_LDO_FPM_RCTB_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_LDO_FPM_RCTB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_bandgap_rctb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_BANDGAP_RCTB_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_BANDGAP_RCTB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_current_rctb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_CURRENT_RCTB_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_CURRENT_RCTB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_c1c2_sync_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_C1C2_SYNC_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_C1C2_SYNC_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_sync_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_SYNC_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_SYNC_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_acl_bit_swap(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_ACL_BIT_SWAP_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_ACL_BIT_SWAP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_acl_bit_end(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_ACL_BIT_END_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_ACL_BIT_END_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_s2a_acl_bit_start(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_S2A_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_S2A_ACL_BIT_START_MASK),
				    (unsigned int) (PMIC_RG_DCXO_S2A_ACL_BIT_START_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_ldo_buftop_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_LDO_BUFTOP_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_LDO_BUFTOP_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_ldo_fpm_rctb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_LDO_FPM_RCTB_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_LDO_FPM_RCTB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_bandgap_rctb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_BANDGAP_RCTB_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_BANDGAP_RCTB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_current_rctb(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_CURRENT_RCTB_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_CURRENT_RCTB_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_cap_low(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_CAP_LOW_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_CAP_LOW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_cap_high(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_CAP_HIGH_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_CAP_HIGH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_c1c2_sync_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_C1C2_SYNC_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_C1C2_SYNC_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_sync_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_SYNC_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_SYNC_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_acl_bit_swap(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_ACL_BIT_SWAP_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_ACL_BIT_SWAP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_acl_bit_end(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_ACL_BIT_END_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_ACL_BIT_END_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_a2s_acl_bit_start(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_A2S_CON1),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_A2S_ACL_BIT_START_MASK),
				    (unsigned int) (PMIC_RG_DCXO_A2S_ACL_BIT_START_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_cap_low(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_CAP_LOW_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_CAP_LOW_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_cap_high(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_CAP_HIGH_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_CAP_HIGH_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_c1c2_sync_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_C1C2_SYNC_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_C1C2_SYNC_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_sync_en(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_SYNC_EN_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_SYNC_EN_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_por_length_option(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_POR_LENGTH_OPTION_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_POR_LENGTH_OPTION_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_acl_bit_swap(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_ACL_BIT_SWAP_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_ACL_BIT_SWAP_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_acl_bit_end(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_ACL_BIT_END_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_ACL_BIT_END_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_por2_acl_bit_start(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_POR2_CON3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_POR2_ACL_BIT_START_MASK),
				    (unsigned int) (PMIC_RG_DCXO_POR2_ACL_BIT_START_SHIFT)
	    );
	pmic_unlock();
}

static inline void upmu_set_rg_dcxo_ft_trim_length_option(unsigned int val)
{
	unsigned int ret = 0;

	pmic_lock();
	ret = pmic_config_interface((unsigned int) (RG_DCXO_FORCE_MODE3),
				    (unsigned int) (val),
				    (unsigned int) (PMIC_RG_DCXO_FT_TRIM_LENGTH_OPTION_MASK),
				    (unsigned int) (PMIC_RG_DCXO_FT_TRIM_LENGTH_OPTION_SHIFT)
	    );
	pmic_unlock();
}

static inline unsigned int upmu_get_rgs_dcxo_c2(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (RG_DCXO_ANALOG_RO0),
				  (&val),
				  (unsigned int) (PMIC_RGS_DCXO_C2_MASK), (unsigned int) (PMIC_RGS_DCXO_C2_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rgs_dcxo_icont(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (RG_DCXO_ANALOG_RO0),
				  (&val),
				  (unsigned int) (PMIC_RGS_DCXO_ICONT_MASK),
				  (unsigned int) (PMIC_RGS_DCXO_ICONT_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rgs_dcxo_trim2_c2(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (RG_DCXO_TRIM_RO0),
				  (&val),
				  (unsigned int) (PMIC_RGS_DCXO_TRIM2_C2_MASK),
				  (unsigned int) (PMIC_RGS_DCXO_TRIM2_C2_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rgs_dcxo_trim1_c2(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (RG_DCXO_TRIM_RO0),
				  (&val),
				  (unsigned int) (PMIC_RGS_DCXO_TRIM1_C2_MASK),
				  (unsigned int) (PMIC_RGS_DCXO_TRIM1_C2_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rgs_dcxo_ft_freq_trim_step_value(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (RG_DCXO_TRIM_RO1),
				  (&val),
				  (unsigned int) (PMIC_RGS_DCXO_FT_FREQ_TRIM_STEP_VALUE_MASK),
				  (unsigned int) (PMIC_RGS_DCXO_FT_FREQ_TRIM_STEP_VALUE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rgs_dcxo_ft_freq_trim_step_end(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (RG_DCXO_TRIM_RO1),
				  (&val),
				  (unsigned int) (PMIC_RGS_DCXO_FT_FREQ_TRIM_STEP_END_MASK),
				  (unsigned int) (PMIC_RGS_DCXO_FT_FREQ_TRIM_STEP_END_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rgs_dcxo_ft_freq_trim_end(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (RG_DCXO_TRIM_RO1),
				  (&val),
				  (unsigned int) (PMIC_RGS_DCXO_FT_FREQ_TRIM_END_MASK),
				  (unsigned int) (PMIC_RGS_DCXO_FT_FREQ_TRIM_END_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rgs_dcxo_manual_acl_end(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (RG_DCXO_TRIM_RO1),
				  (&val),
				  (unsigned int) (PMIC_RGS_DCXO_MANUAL_ACL_END_MASK),
				  (unsigned int) (PMIC_RGS_DCXO_MANUAL_ACL_END_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rgs_dcxo_xtal_mode(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (RG_DCXO_TRIM_RO1),
				  (&val),
				  (unsigned int) (PMIC_RGS_DCXO_XTAL_MODE_MASK),
				  (unsigned int) (PMIC_RGS_DCXO_XTAL_MODE_SHIFT)
	    );
	pmic_unlock();

	return val;
}

static inline unsigned int upmu_get_rgs_dcxo_trim_sel(void)
{
	unsigned int ret = 0;
	unsigned int val = 0;

	pmic_lock();
	ret = pmic_read_interface((unsigned int) (RG_DCXO_TRIM_RO1),
				  (&val),
				  (unsigned int) (PMIC_RGS_DCXO_TRIM_SEL_MASK),
				  (unsigned int) (PMIC_RGS_DCXO_TRIM_SEL_SHIFT)
	    );
	pmic_unlock();

	return val;
}

/* ADC Channel Number */
typedef enum {
	/* MT6397 */
	MT6397_AUX_BATSNS = 0x000,
	MT6397_AUX_ISENSE,
	MT6397_AUX_VCDT,
	MT6397_AUX_BATON,
	MT6397_AUX_CH4,
	MT6397_AUX_CH5,
	MT6397_AUX_CH6,
	MT6397_AUX_CH7,
} pmic_adc_ch_list_enum;

typedef enum MT_POWER_TAG {
	/* Digital LDO */
	MT65XX_POWER_LDO_VIO28 = 0,
	MT65XX_POWER_LDO_VUSB,
	MT65XX_POWER_LDO_VMC,
	MT65XX_POWER_LDO_VMCH,
	MT65XX_POWER_LDO_VEMC_3V3,
	MT65XX_POWER_LDO_VEMC_1V8,
	MT65XX_POWER_LDO_VGP1,
	MT65XX_POWER_LDO_VGP2,
	MT65XX_POWER_LDO_VGP3,
	MT65XX_POWER_LDO_VGP4,
	MT65XX_POWER_LDO_VGP5,
	MT65XX_POWER_LDO_VGP6,
	MT65XX_POWER_LDO_VSIM1,
	MT65XX_POWER_LDO_VSIM2,
	MT65XX_POWER_LDO_VIBR,
	MT65XX_POWER_LDO_VRTC,
	MT65XX_POWER_LDO_VAST,

	/* Analog LDO */
	MT65XX_POWER_LDO_VRF28,
	MT65XX_POWER_LDO_VRF28_2,
	MT65XX_POWER_LDO_VTCXO,
	MT65XX_POWER_LDO_VTCXO_2,
	MT65XX_POWER_LDO_VA,
	MT65XX_POWER_LDO_VA28,
	MT65XX_POWER_LDO_VCAMA,

	MT65XX_POWER_LDO_DEFAULT,
	MT65XX_POWER_COUNT_END,
	MT65XX_POWER_NONE = -1
} MT65XX_POWER;

#endif				/* _MT6397_PMIC_UPMU_SW_H_ */
