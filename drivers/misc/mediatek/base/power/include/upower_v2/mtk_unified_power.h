/*
 * Copyright (C) 2020 MediaTek Inc.
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

#ifndef MTK_UNIFIED_POWER_H
#define MTK_UNIFIED_POWER_H

#include <linux/sched/topology.h>
#ifdef __cplusplus
extern "C" {
#endif
#include <linux/sched.h>
#if defined(CONFIG_MACH_MT6759)
#include "mtk_unified_power_mt6759.h"
#endif

#if defined(CONFIG_MACH_MT6763)
#include "mtk_unified_power_mt6763.h"
#endif

#if defined(CONFIG_MACH_MT6758)
#include "mtk_unified_power_mt6758.h"
#endif

#if defined(CONFIG_MACH_MT6739)
#include "mtk_unified_power_mt6739.h"
#endif

#if defined(CONFIG_MACH_MT6765)
#include "mtk_unified_power_mt6765.h"
#endif

#if defined(CONFIG_MACH_MT6771)
#include "mtk_unified_power_mt6771.h"
#endif

#if defined(CONFIG_MACH_MT6775)
#include "mtk_unified_power_mt6775.h"
#endif

#if defined(CONFIG_MACH_MT6768)
#include "mtk_unified_power_mt6768.h"
#endif

#if defined(CONFIG_MACH_MT6785)
#include "mtk_unified_power_mt6785.h"
#endif

#if defined(CONFIG_MACH_MT6885)
#if defined(CONFIG_MTK_SCHED_MULTI_GEARS)
#include "mtk_unified_power_mt6893.h"
#else
#include "mtk_unified_power_mt6885.h"
#endif
#endif

#if defined(CONFIG_MACH_MT6893)
#include "mtk_unified_power_mt6893.h"
#endif

#if defined(CONFIG_MACH_MT6873)
#include "mtk_unified_power_mt6873.h"
#endif

#if defined(CONFIG_MACH_MT6853)
#include "mtk_unified_power_mt6853.h"
#endif

#if defined(CONFIG_MACH_MT6833)
#include "mtk_unified_power_mt6833.h"
#endif

#if defined(CONFIG_MACH_MT8168)
#include "mtk_unified_power_mt8168.h"
#endif

#define UPOWER_TAG "[UPOWER]"

#define upower_error(fmt, args...) pr_debug(UPOWER_TAG fmt, ##args)

#if UPOWER_LOG
	#define upower_debug(fmt, args...) pr_debug(UPOWER_TAG fmt, ##args)
#else
	#define upower_debug(fmt, args...)
#endif

/***************************
 * Basic Data Declarations *
 **************************/
/* 8bytes + 4bytes + 4bytes + 24bytes = 40 bytes*/
/* but compiler will align to 40 bytes for computing more faster */
/* if a table has 16 opps --> 40*16= 640 bytes*/
struct upower_tbl_row {
	unsigned long cap;
	unsigned int volt; /* 10uv */
	unsigned int dyn_pwr; /* uw */
	unsigned int pwr_efficiency; /* uw */
	unsigned int lkg_pwr[NR_UPOWER_DEGREE]; /* uw */
};

/* struct idle_state defined at sched.h */
/* sizeof(struct upower_tbl) = 5264bytes */
struct upower_tbl {
	struct upower_tbl_row row[UPOWER_OPP_NUM];
	unsigned int lkg_idx;
	unsigned int row_num;
	unsigned int max_efficiency;
	unsigned int min_efficiency;
	struct idle_state idle_states[NR_UPOWER_DEGREE][NR_UPOWER_CSTATES];
	unsigned int nr_idle_states;
	int turn_point;
};

struct upower_tbl_info {
	const char *name;
	struct upower_tbl *p_upower_tbl;
};

/***************************
 * Global variables        *
 **************************/
extern struct upower_tbl *upower_tbl_ref; /* upower table reference to sram*/
extern int degree_set[NR_UPOWER_DEGREE];
/* collect all the raw tables */
extern struct upower_tbl_info *upower_tbl_infos;
/* points to upower_tbl_infos[] */
extern struct upower_tbl_info *p_upower_tbl_infos;
extern unsigned char upower_enable;
extern unsigned char upower_recognize_by_eem[NR_UPOWER_BANK];
void set_sched_turn_point_cap(void);

/***************************
 * APIs                    *
 **************************/
/* PPM */
extern unsigned int upower_get_power(enum upower_bank bank, unsigned int opp,
		enum upower_dtype type);
/* EAS */
extern struct upower_tbl_info **upower_get_tbl(void);
extern int upower_get_turn_point(void);
extern struct upower_tbl *upower_get_core_tbl(unsigned int cpu);
/* EEM */
extern void upower_update_volt_by_eem(enum upower_bank bank, unsigned int *volt,
		unsigned int opp_num);
extern void upower_update_degree_by_eem(enum upower_bank bank, int deg);

/* platform part */
extern int upower_bank_to_spower_bank(int upower_bank);
extern void get_original_table(void);
extern void upower_update_L_plus_cap(void);
extern void upower_update_L_plus_lkg_pwr(void);

#ifdef UPOWER_RCU_LOCK
extern void upower_read_lock(void);
extern void upower_read_unlock(void);
#endif

#ifdef UPOWER_PROFILE_API_TIME
enum {
	GET_PWR,
	GET_TBL_PTR,
	UPDATE_TBL_PTR,

	TEST_NUM
};
extern void upower_get_start_time_us(unsigned int type);
extern void upower_get_diff_time_us(unsigned int type);
extern void print_diff_results(unsigned int type);
#endif

#ifdef SUPPORT_UPOWER_DCONFIG
extern void upower_by_doe(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
