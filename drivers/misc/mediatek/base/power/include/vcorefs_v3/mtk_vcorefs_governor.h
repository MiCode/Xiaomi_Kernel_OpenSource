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

#ifndef _MTK_VCOREFS_GOVERNOR_H
#define _MTK_VCOREFS_GOVERNOR_H

#define VCPREFS_TAG "[VcoreFS] "
#define vcorefs_err vcorefs_info
#define vcorefs_crit vcorefs_info
#define vcorefs_warn vcorefs_info
#define vcorefs_debug vcorefs_info

#define vcorefs_info(fmt, args...)	\
	pr_notice(VCPREFS_TAG""fmt, ##args)

/* Uses for DVFS Request */
#define vcorefs_crit_mask(log_mask, kicker, fmt, args...)	\
do {								\
	if (log_mask & (1U << kicker))				\
		;						\
	else							\
		vcorefs_crit(fmt, ##args);			\
} while (0)

struct kicker_config {
	int kicker;
	int opp;
	int dvfs_opp;
};

enum dvfs_kicker {
	KIR_MM = 0,
	KIR_DCS,
	KIR_UFO,
	KIR_PERF,
	KIR_EFUSE,
	KIR_PASR,
	KIR_SDIO,
	KIR_USB,
	KIR_GPU,
	KIR_APCCCI,
	KIR_BOOTUP,
	KIR_FBT,
	KIR_WIFI,
	KIR_TLC,
	KIR_SYSFS,
	KIR_MM_NON_FORCE,
	KIR_SYSFS_N,
	KIR_SYSFSX,
	NUM_KICKER,

	/* internal kicker */
	KIR_LATE_INIT,
	KIR_AUTOK_EMMC,
	KIR_AUTOK_SDIO,
	KIR_AUTOK_SD,
	LAST_KICKER,
};

#if defined(CONFIG_MACH_MT6758)
enum dvfs_opp {
	OPP_UNREQ = -1,
	OPP_0 = 0,
	OPP_1,
	OPP_2,
	NUM_OPP,
};
#else
enum dvfs_opp {
	OPP_UNREQ = -1,
	OPP_0 = 0,
	OPP_1,
	OPP_2,
	OPP_3,
	NUM_OPP,
};
#endif

struct opp_profile {
	int vcore_uv;
	int ddr_khz;
};

/* boot up opp for SPM init */
#define BOOT_UP_OPP             OPP_0

/* target OPP when feature enable */
#if defined(CONFIG_MACH_MT6759)
#define LATE_INIT_OPP	(NUM_OPP - 2) /* for hwc enabled display temp-fix */
#elif defined(CONFIG_MACH_MT6758)
#define LATE_INIT_OPP	(NUM_OPP - 1) /* note it is 3 OPP only */
#elif defined(CONFIG_MACH_MT6763)
#define LATE_INIT_OPP	(NUM_OPP - 1)
#else
#define LATE_INIT_OPP	(NUM_OPP - 1)
#endif

/* need autok in MSDC group */
#define AUTOK_KIR_GROUP					\
			((1U << KIR_AUTOK_EMMC) |		\
			(1U << KIR_AUTOK_SDIO) |		\
			(1U << KIR_AUTOK_SD))

/*
 * VOUT selection in normal mode (SW mode)
 * VOUT = 0.40000V + 6.25mV * VOSEL for PMIC MT6335
 * VOUT = 0.50000V + 6.25mV * VOSEL for PMIC MT6356
 * VOUT = 0.40625V + 6.25mV * VOSEL for PMIC MT6355
 */
#define PMIC_VCORE_ADDR         PMIC_RG_BUCK_VCORE_VOSEL

#if defined(CONFIG_MACH_MT6799)          /* PMIC MT6335 */
#define VCORE_BASE_UV           400000
#elif defined(CONFIG_MACH_MT6763)        /* PMIC MT6356 */
#define VCORE_BASE_UV           500000
#elif defined(CONFIG_MACH_MT6759) || defined(CONFIG_MACH_MT6758)
/* PMIC MT6355 */
#define VCORE_BASE_UV           406250
#else
#error "Not set pmic config properly!"
#endif

#define VCORE_STEP_UV           6250
#define VCORE_INVALID           0x80

#define vcore_uv_to_pmic(uv)	/* pmic >= uv */	\
	((((uv) - VCORE_BASE_UV) + (VCORE_STEP_UV - 1)) / VCORE_STEP_UV)

#define vcore_pmic_to_uv(pmic)	\
	(((pmic) * VCORE_STEP_UV) + VCORE_BASE_UV)


extern int kicker_table[LAST_KICKER];

/* Governor extern API */
extern bool is_vcorefs_feature_enable(void);
bool vcorefs_vcore_dvs_en(void);
bool vcorefs_dram_dfs_en(void);
bool vcorefs_mm_clk_en(void);
bool vcorefs_i_hwpath_en(void);
int vcorefs_module_init(void);
extern int vcorefs_get_num_opp(void);
extern int vcorefs_get_sw_opp(void);
extern int vcorefs_get_curr_vcore(void);
extern int vcorefs_get_curr_ddr(void);
extern void vcorefs_update_opp_table(void);
extern char *governor_get_kicker_name(int id);
extern char *vcorefs_get_opp_table_info(char *p);
extern int vcorefs_output_kicker_id(char *name);
extern int governor_debug_store(const char *p);
extern int vcorefs_late_init_dvfs(void);
extern int kick_dvfs_by_opp_index(struct kicker_config *krconf);
extern char *governor_get_dvfs_info(char *p);
extern int vcorefs_get_vcore_by_steps(u32 opp);
extern void vcorefs_init_opp_table(void);
extern void governor_autok_manager(void);
extern bool governor_autok_check(int kicker);
extern bool governor_autok_lock_check(int kicker, int opp);
extern int vcorefs_get_hw_opp(void);
extern int vcorefs_enable_debug_isr(bool enable);

#endif	/* _MTK_VCOREFS_GOVERNOR_H */
