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

/**********************************************
 * unified_power.h
 * This header file includes:
 * 1. Global configs for unified power driver
 * 2. Global macros
 * 3. Declarations of enums and main data structures
 * 4. Extern global variables
 * 5. Extern global APIs
 *********************************************
 */
#ifndef MTK_UNIFIED_POWER_H
#define MTK_UNIFIED_POWER_H

#ifdef __cplusplus
extern "C" {
#endif

#define UPOWER_OPP_NUM 16
#define UPOWER_DEGREE_0 85
#define UPOWER_DEGREE_1 75
#define UPOWER_DEGREE_2 65
#define UPOWER_DEGREE_3 55
#define UPOWER_DEGREE_4 45
#define UPOWER_DEGREE_5 25

#define NR_UPOWER_DEGREE 6
#define DEFAULT_LKG_IDX 0
#define UPOWER_FUNC_CODE_EFUSE_INDEX 120
#define NR_UPOWER_CSTATES 2  /* only use c0, c1 */
#define UPOWER_C1_VOLT 50000 /* 0.5v */
#define UPOWER_C1_IDX 1      /* idx of c1 in idle_states[][idx] */

/* upower banks */
enum upower_bank {
	UPOWER_BANK_LL,
	UPOWER_BANK_L,
	UPOWER_BANK_B,
	UPOWER_BANK_CLS_LL,
	UPOWER_BANK_CLS_L,
	UPOWER_BANK_CLS_B,
	UPOWER_BANK_CCI,

	NR_UPOWER_BANK,
};

#define UPOWER_BANK_CLS_BASE UPOWER_BANK_CLS_LL

/* for upower_get_power() to get the target power */
enum upower_dtype {
	UPOWER_DYN,
	UPOWER_LKG,
	UPOWER_CPU_STATES,

	NR_UPOWER_DTYPE,
};

/***************************
 * Basic Data Declarations *
 **************************/
/* 8bytes + 4bytes + 4bytes + 24bytes = 40 bytes*/
/* but compiler will align to 40 bytes for computing more faster */
/* if a table has 16 opps --> 40*16= 640 bytes*/
struct upower_tbl_row {
	unsigned long long cap;
	unsigned int volt;			/* 10uv */
	unsigned int dyn_pwr;			/* uw */
	unsigned int lkg_pwr[NR_UPOWER_DEGREE]; /* uw */
};

/* struct idle_state defined at sched.h */
/* sizeof(struct upower_tbl) = 5264bytes */
struct upower_tbl {
	struct upower_tbl_row row[UPOWER_OPP_NUM];
	unsigned int lkg_idx;
	unsigned int row_num;
	struct idle_state idle_states[NR_UPOWER_DEGREE][NR_UPOWER_CSTATES];
	unsigned int nr_idle_states;
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

/***************************
 * APIs                    *
 **************************/
/* PPM */
extern unsigned int upower_get_power(enum upower_bank bank, unsigned int opp,
				     enum upower_dtype type);
/* EAS */
extern struct upower_tbl_info **upower_get_tbl(void);
extern struct upower_tbl *upower_get_core_tbl(unsigned int cpu);
/* EEM */
extern void upower_update_volt_by_eem(enum upower_bank bank, unsigned int *volt,
				      unsigned int opp_num);
extern void upower_update_degree_by_eem(enum upower_bank bank, int deg);

#ifdef __cplusplus
}
#endif

#endif
