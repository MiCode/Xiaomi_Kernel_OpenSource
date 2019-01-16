#ifndef _MT_VCORE_DVFS_
#define _MT_VCORE_DVFS_

#include <linux/kernel.h>
#include <mach/mt_smi.h>

/**************************************
 * Config and Parameter
 **************************************/
#define PMIC_VCORE_AO_VOSEL_ON		0x36c	/* VCORE2 */
#define PMIC_VCORE_PDN_VOSEL_ON		0x24e	/* VDVFS11 */

#define VCORE_BASE_UV		700000
#define VCORE_STEP_UV		6250

#define VCORE_INVALID		0x80
#define FREQ_INVALID		0

#ifdef CONFIG_ARM64
enum opp_index {
	OPPI_0,			/* 0: Vcore 1.125, DDR 1600 */
	OPPI_1,			/* 1: Vcore 1.125, DDR 1466 */
	OPPI_2,			/* 2: Vcore 1.0, DDR 1333 */
	OPPI_3,			/* 3: Vcore 1.0, DDR 800 */
	NUM_OPPS
};

#define OPPI_UNREQ		-1
#define OPPI_PERF		OPPI_0
#define OPPI_PERF_2		OPPI_1
#define OPPI_LOW_PWR		OPPI_2
#define OPPI_LOW_PWR_2		OPPI_3
#else
enum opp_index {
	OPPI_0,			/* 0: Vcore 1.125, DDR 1600 */
	OPPI_1,			/* 1: Vcore 1.0, DDR 1333 */
	NUM_OPPS
};

#define OPPI_UNREQ		-1
#define OPPI_PERF		OPPI_0
#define OPPI_LOW_PWR		OPPI_1
#endif


/**************************************
 * Define and Declare
 **************************************/
#define ERR_VCORE_DVS		1
#define ERR_DDR_DFS		2
#define ERR_AXI_DFS		3
#define ERR_MM_DFS		4

#ifdef CONFIG_ARM64
enum dvfs_kicker {
	KR_LATE_INIT,		/* 0 */
	KR_SDIO_AUTOK,		/* 1 */
	KR_SYSFS,		/* 2 */
	KR_SCREEN_OFF,		/* 3 */
	KR_MM_SCEN,		/* 4 */
	KR_EMI_MON,		/* 5 */
	NUM_KICKERS
};
#else
enum dvfs_kicker {
	KR_SCREEN_ON,		/* 0 */
	KR_SCREEN_OFF,		/* 1 */
	KR_SDIO_AUTOK,		/* 2 */
	KR_SYSFS,		/* 3 */
	KR_LATE_INIT,		/* 4 */
	NUM_KICKERS
};
#endif

/* for MM and EMI_Mon */
extern int vcorefs_request_dvfs_opp(enum dvfs_kicker kicker, int index);
#ifdef MMDVFS_MMCLOCK_NOTIFICATION
extern int vcorefs_request_opp_no_mm_notify(enum dvfs_kicker kicker, int index);
#endif

/* for SDIO */
extern int vcorefs_sdio_lock_dvfs(bool in_ot);
extern u32 vcorefs_sdio_get_vcore_nml(void);
extern int vcorefs_sdio_set_vcore_nml(u32 vcore_uv);
extern int vcorefs_sdio_unlock_dvfs(bool in_ot);
extern bool vcorefs_sdio_need_multi_autok(void);

/* for CLKMGR */
extern void vcorefs_clkmgr_notify_mm_off(void);
extern void vcorefs_clkmgr_notify_mm_on(void);
extern bool vcorefs_is_stay_lv_enabled(void);	/* phased out */


/**************************************
 * Macro and Inline
 **************************************/
#define vcore_uv_to_pmic(uv)	/* pmic >= uv */	\
	((((uv) - VCORE_BASE_UV) + (VCORE_STEP_UV - 1)) / VCORE_STEP_UV)

#define vcore_pmic_to_uv(pmic)	\
	((pmic) * VCORE_STEP_UV + VCORE_BASE_UV)

#define VCORE_1_P_125_UV	1125000
#define VCORE_1_P_0_UV		1000000

#define VCORE_1_P_125		vcore_uv_to_pmic(VCORE_1_P_125_UV)	/* 0x44 */
#define VCORE_1_P_0		vcore_uv_to_pmic(VCORE_1_P_0_UV)	/* 0x30 */

#endif
