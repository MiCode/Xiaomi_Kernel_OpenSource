/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/ktime.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <mt-plat/mtk_chip.h>

/* local include */
#include "mtk_upower.h"
#include "mtk_unified_power_data.h"
#include "mtk_devinfo.h"

/* #include "mtk_eem.h"		*/
/* #include "upmu_common.h"	*/

#ifndef EARLY_PORTING_SPOWER
#include "mtk_common_static_power.h"
#endif

#undef  BIT
#define BIT(bit)	(1U << (bit))
#define MSB(range)	(1 ? range)
#define LSB(range)	(0 ? range)

/**
 * Set value at MSB:LSB. For example, BITS(7:3, 0x5A)
 * will return a value where bit 3 to bit 7 is 0x5A
 * @r:	Range in the form of MSB:LSB
 */
/* BITS(MSB:LSB, value) => Set value at MSB:LSB  */
#define BITS(r, val)	((val << LSB(r)) & BITMASK(r))
#define GET_BITS_VAL(_bits_, _val_)   (((_val_) & (BITMASK(_bits_))) >> ((0) ? _bits_))

struct upower_tbl final_upower_tbl[NR_UPOWER_BANK] = {};

int degree_set[NR_UPOWER_DEGREE] = {
	UPOWER_DEGREE_0,
	UPOWER_DEGREE_1,
	UPOWER_DEGREE_2,
	UPOWER_DEGREE_3,
	UPOWER_DEGREE_4,
	UPOWER_DEGREE_5,
};


/* collect all the raw tables */
#define INIT_UPOWER_TBL_INFOS(name, tbl) {__stringify(name), &tbl}
struct upower_tbl_info upower_tbl_infos_list[NR_UPOWER_TBL_LIST][NR_UPOWER_BANK] = {
	[0] = {	/* MT6739 FY */
		INIT_UPOWER_TBL_INFOS(UPOWER_BANK_LL,		upower_tbl_ll_FY),
		INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_LL,	upower_tbl_cluster_ll_FY),
	},
	[1] = {	/* MT6739 SB (turbo) */
		INIT_UPOWER_TBL_INFOS(UPOWER_BANK_LL,		upower_tbl_ll_SB),
		INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_LL,	upower_tbl_cluster_ll_SB),
	},
	[2] = {	/* MT6739 (WM) */
		INIT_UPOWER_TBL_INFOS(UPOWER_BANK_LL,		upower_tbl_ll_WM),
		INIT_UPOWER_TBL_INFOS(UPOWER_BANK_CLS_LL,	upower_tbl_cluster_ll_WM),
	},
};

/* Upower will know how to apply voltage that comes from EEM */
unsigned char upower_recognize_by_eem[NR_UPOWER_BANK] = {
	UPOWER_BANK_LL,		/* LL EEM apply voltage to LL upower bank */
	UPOWER_BANK_LL,		/* LL EEM apply voltage to CLS_LL upower bank */
};

/* Used for rcu lock, points to all the raw tables list*/
struct upower_tbl_info *p_upower_tbl_infos = &upower_tbl_infos_list[0][0];

#ifndef EARLY_PORTING_SPOWER
int upower_bank_to_spower_bank(int upower_bank)
{
	int ret;

	switch (upower_bank) {
	case UPOWER_BANK_LL:
		ret = MTK_SPOWER_CPULL;
		break;
	case UPOWER_BANK_CLS_LL:
		ret = MTK_SPOWER_CPULL_CLUSTER;
		break;
	default:
		ret = -1;
		break;
	}
	return ret;
}
#endif

#if 0 /* no bank L in mt6739 */
static void upower_scale_l_cap(void)
{
	unsigned int cap;
	struct upower_tbl *tbl;

	/* get L opp0's cap and calculate scaling ratio */
	/* ratio = round_up(1024 * 1000 / opp0 cap) */
	/* new cap = orig cap * ratio / 1000 */
	tbl = upower_tbl_infos[UPOWER_BANK_L].p_upower_tbl;
	cap = tbl->row[UPOWER_OPP_NUM - 1].cap;

	/* if L opp0's cap is 1024, no need to scale cap anymore */
	if (cap != 1024)
		WARN_ON(1);	/* For MT6739 the cap should be already 1024 */
}
#endif

/****************************************************
 * According to chip version get the raw upower tbl *
 * and let upower_tbl_infos points to it.           *
 * Choose a non used upower tbl location and let    *
 * upower_tbl_ref points to it to store target      *
 * power tbl.                                       *
 ***************************************************/

#if 0  /* use the CPUDVFS API instead */
#define SEG_EFUSE		30
#define TURBO_EFUSE		29		/* 590 */
static unsigned short get_cpu_level(void)
{
	unsigned int seg_code = 0;
	unsigned int turbo_code = 0;

	seg_code = get_devinfo_with_index(SEG_EFUSE);
	seg_code = GET_BITS_VAL(6:0, seg_code);

	turbo_code = get_devinfo_with_index(TURBO_EFUSE);
	turbo_code = GET_BITS_VAL(21:20, turbo_code);

	if (seg_code == 0x4 && turbo_code == 0x3)
		return 1;
	return 0;
}
#endif

int cpu_cluster_mapping(unsigned int cpu)
{
	enum upower_bank bank = UPOWER_BANK_LL;

	if (cpu < 4) /* cpu 0-3 */
		bank = UPOWER_BANK_LL;
	else if (cpu < 8) /* cpu 4-7 */
		bank = UPOWER_BANK_LL + 1;
	else if (cpu < 10) /* cpu 8-9 */
		bank = UPOWER_BANK_LL + 2;

	return bank;
}

void get_original_table(void)
{
	unsigned int i, j;
	unsigned int idx = 0; /* default use MT6739_FY */

	idx = _mt_cpufreq_get_cpu_level();
	if (idx >= NR_UPOWER_TBL_LIST)
		idx = 0;

	/* get location of reference table */
	upower_tbl_infos = &upower_tbl_infos_list[idx][0];

	/* get location of target table */
	upower_tbl_ref = &final_upower_tbl[0];

	upower_debug("idx %d dest:%p, src:%p\n",
			(idx+1) % NR_UPOWER_TBL_LIST, upower_tbl_ref, upower_tbl_infos);

	/* If disable upower, ptr will point to original upower table */
	p_upower_tbl_infos = upower_tbl_infos;
	/*
	*  Clear volt fields before eem run.                                  *
	*  If eem is enabled, it will apply volt into it. If eem is disabled, *
	*  the values of volt are 0 , and upower will apply orig volt into it *
	*/
	for (i = 0; i < NR_UPOWER_BANK; i++) {
		for (j = 0; j < UPOWER_OPP_NUM; j++)
			upower_tbl_ref[i].row[j].volt = 0;
	}
	for (i = 0; i < NR_UPOWER_BANK; i++)
		upower_debug("bank[%d] dest:%p dyn_pwr:%u, volt[0]%u\n", i, &upower_tbl_ref[i],
				upower_tbl_ref[i].row[0].dyn_pwr, upower_tbl_ref[i].row[0].volt);

	/* Not support L+ now, scale L and cluster L cap to 1024 */
#if 0
	upower_scale_l_cap();
#endif

}

MODULE_DESCRIPTION("MediaTek Unified Power Driver v0.0");
MODULE_LICENSE("GPL");
