/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _MT_VCORE_DVFS_
#define _MT_VCORE_DVFS_

#include <linux/kernel.h>

#define OLD_VCORE_DVFS_FORMAT	0


#include <mt-plat/mt_smi.h>

#include <linux/irq.h>
#include <linux/irqdesc.h>


/**************************************
 * Config and Parameter
 **************************************/
#define PMIC_VCORE_AO_VOSEL_ON		0x36c	/* VCORE2 */
#define PMIC_VCORE_PDN_VOSEL_ON		0x24e	/* VDVFS11 */

#define VCORE_BASE_UV		700000
#define VCORE_STEP_UV		6250

#define VCORE_INVALID		0x80
#define FREQ_INVALID		0

enum opp_index {
	OPPI_0,			/* 0: Vcore 1.125, DDR 1866 */
	OPPI_1,			/* 1: Vcore 1.0, DDR 1600 */
	NUM_OPPS
};
#define OPPI_UNREQ		-1
#define OPPI_PERF		OPPI_0
#define OPPI_LOW_PWR		OPPI_1

/**************************************
 * Define and Declare
 **************************************/
#define ERR_VCORE_DVS		1
#define ERR_DDR_DFS		2
#define ERR_AXI_DFS		3
#define ERR_MM_DFS		4

enum dvfs_kicker {
	KR_LATE_INIT,		/* 0 */
	KR_SDIO_AUTOK,		/* 1 */
	KR_SYSFS,		/* 2 */
	KR_MM_SCEN,		/* 3 */
	KR_GPU_DVFS,	/* 4 */
	KR_EMI_MON,		/* 5 */
	NUM_KICKERS
};

extern unsigned int dtcm_ready; /* at mt_dramc.c for DRAM ready */
extern unsigned int md32_mobile_log_ready; /* MD32 is ready */

/* for MM and EMI_Mon */
extern int vcorefs_request_dvfs_opp(enum dvfs_kicker kicker, int index);

#if OLD_VCORE_DVFS_FORMAT
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
#else

/* #include <mt_sd_func.h> */
extern int sdio_stop_transfer(void);
extern int sdio_start_ot_transfer(void);
extern int autok_abort_action(void);
extern int autok_is_vol_done(unsigned int voltage, int id);


/* mmdvfs_mgr.c */
extern int mmdvfs_is_default_step_need_perf(void);
extern void mmdvfs_mm_clock_switch_notify(int is_before, int is_to_high);


/* #include <primary_display.h> */
extern u32 DISP_GetScreenWidth(void);
extern u32 DISP_GetScreenHeight(void);

extern  void mask_irq(struct irq_desc *desc);
extern  void unmask_irq(struct irq_desc *desc);

#endif


extern int vcore_dvfs_config_speed(int hispeed);

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
