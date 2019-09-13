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

#include <linux/module.h>
#include "mtk_upower.h"

/* PTP will update volt in init2 isr handler */
void upower_update_volt_by_eem(enum upower_bank bank,
		unsigned int *volt, unsigned int opp_num)
{
	int i, j;
	int index = opp_num;

	upower_debug("(%s) bank = %d, opp_num = %d\n", __func__, bank, opp_num);

	if (bank >= NR_UPOWER_BANK) {
		upower_debug("(%s) No this bank\n", __func__);
		return;
	}
	for (i = 0; i < NR_UPOWER_BANK; i++) {
		if (upower_recognize_by_eem[i] == bank) {
			for (j = 0; j < opp_num; j++) {
				/* reorder idex of volt */
				index = opp_num - j - 1;
				upower_tbl_ref[i].row[index].volt = volt[j];
			}
			upower_debug("(upower bk %d)volt = %u, (eem bk %d)volt = %u\n",
				i, upower_tbl_ref[i].row[0].volt,
				bank, volt[0]);
		}
	}
}
EXPORT_SYMBOL(upower_update_volt_by_eem);

/* PTP will update volt in eem_set_eem_volt */
void upower_update_degree_by_eem(enum upower_bank bank, int deg)
{
	int idx = -1;
	int i;
	int upper;

	/* calculate upper bound first, then decide idx of degree */
	for (i = NR_UPOWER_DEGREE - 1; i > 0; i--) {
		upper = (degree_set[i] + degree_set[i-1] + 1) / 2;
		if (deg <= upper) {
			idx = i;
			break;
		}
	}
	if (idx == -1)
		idx = 0;

	upower_debug("(%s) bank = %d, deg = %d\n", __func__, bank, deg);
	if (bank >= NR_UPOWER_BANK) {
		upower_debug("(%s) No this bank\n", __func__);
		return;
	}

	for (i = 0; i < NR_UPOWER_BANK; i++) {
		if (upower_recognize_by_eem[i] == bank) {
			upower_tbl_ref[i].lkg_idx = idx;
			upower_debug("i = %d, deg = %d\n", i, deg);
		}
	}
}
EXPORT_SYMBOL(upower_update_degree_by_eem);
/* for EAS to get pointer of tbl */
struct upower_tbl_info **upower_get_tbl(void)
{
#if 0
	struct upower_tbl_info *ptr;
	#ifdef UPOWER_PROFILE_API_TIME
	upower_get_start_time_us(GET_TBL_PTR);
	#endif

	#ifdef UPOWER_RCU_LOCK
	upower_read_lock();
	ptr = rcu_dereference(p_upower_tbl_infos);
	upower_read_unlock();
	#else
	ptr = p_upower_tbl_infos;
	#endif

	#ifdef UPOWER_PROFILE_API_TIME
	upower_get_diff_time_us(GET_TBL_PTR);
	print_diff_results(GET_TBL_PTR);
	#endif

	return ptr;
#endif
	return &p_upower_tbl_infos;

}
EXPORT_SYMBOL(upower_get_tbl);

int upower_get_turn_point(void)
{
#ifndef DISABLE_TP
	struct upower_tbl *L_tbl;
	int turn_point;

	L_tbl = &upower_tbl_ref[UPOWER_BANK_L];
	turn_point = L_tbl->turn_point;
	return turn_point;
#else
	return -1;
#endif

}
EXPORT_SYMBOL(upower_get_turn_point);

/* for EAS to get pointer of core's tbl */
struct upower_tbl *upower_get_core_tbl(unsigned int cpu)
{
	struct upower_tbl *ptr_tbl;
	struct upower_tbl_info *ptr_tbl_info;
#ifdef FIRST_CLUSTER_IS_L
	enum upower_bank bank = UPOWER_BANK_L;
#else
	enum upower_bank bank = UPOWER_BANK_LL;
#endif

#if defined(CONFIG_MACH_MT6768) || defined(CONFIG_MACH_MT6785) || \
	defined(CONFIG_MACH_MT3967) || defined(CONFIG_MACH_MT6779)
	if (cpu < 6) /* cpu 0-5 */
		bank = UPOWER_BANK_LL;
	else if (cpu < 8) /* cpu 6-7 */
		bank = UPOWER_BANK_LL + 1;
	else if (cpu < 10) /* cpu 8-9 */
		bank = UPOWER_BANK_LL + 2;
#else
#ifdef FIRST_CLUSTER_IS_L
	if (cpu < 4) /* cpu 0-3 */
		bank = UPOWER_BANK_0;
	else if (cpu < 8) /* cpu 4-7 */
		bank = UPOWER_BANK_1;
#else
	if (cpu < 4) /* cpu 0-3 */
		bank = UPOWER_BANK_LL;
	else if (cpu < 8) /* cpu 4-7 */
		bank = UPOWER_BANK_LL + 1;
	else if (cpu < 10) /* cpu 8-9 */
		bank = UPOWER_BANK_LL + 2;
#endif
#endif

#ifdef UPOWER_L_PLUS
	if (cpu == UPOWER_L_PLUS_CORE)
		bank = UPOWER_BANK_L_PLUS;
#endif

	ptr_tbl_info = p_upower_tbl_infos;
	ptr_tbl = ptr_tbl_info[bank].p_upower_tbl;

	return ptr_tbl;
}
EXPORT_SYMBOL(upower_get_core_tbl);

/* for PPM to get lkg or dyn power*/
unsigned int upower_get_power(enum upower_bank bank, unsigned int opp, enum
upower_dtype type)
{
	unsigned int ret = 0;
	unsigned int idx = 0; /* lkg_idx */
	unsigned int volt_idx = UPOWER_OPP_NUM - opp - 1;
	struct upower_tbl *ptr_tbl;
	struct upower_tbl_info *ptr_tbl_info;

	#ifdef UPOWER_PROFILE_API_TIME
	upower_get_start_time_us(GET_PWR);
	#endif

	if ((opp >= UPOWER_OPP_NUM) || (type >= NR_UPOWER_DTYPE))
		return ret;

	#ifdef UPOWER_RCU_LOCK
	upower_read_lock();
	ptr_tbl_info = rcu_dereference(p_upower_tbl_infos);
	ptr_tbl = ptr_tbl_info[bank].p_upower_tbl;
	idx = ptr_tbl->lkg_idx;
	if (idx >= NR_UPOWER_DEGREE)
		idx = 0;
	ret = (type == UPOWER_DYN) ? ptr_tbl->row[volt_idx].dyn_pwr :
		(type == UPOWER_LKG) ? ptr_tbl->row[volt_idx].lkg_pwr[idx] :
		ptr_tbl->row[volt_idx].cap;
	upower_read_unlock();
	#else
	ptr_tbl_info = p_upower_tbl_infos;
	ptr_tbl = ptr_tbl_info[bank].p_upower_tbl;
	idx = ptr_tbl->lkg_idx;
	if (idx >= NR_UPOWER_DEGREE)
		idx = 0;
	ret = (type == UPOWER_DYN) ? ptr_tbl->row[volt_idx].dyn_pwr :
		(type == UPOWER_LKG) ? ptr_tbl->row[volt_idx].lkg_pwr[idx] :
		ptr_tbl->row[volt_idx].cap;
	#endif

	#ifdef UPOWER_PROFILE_API_TIME
	upower_get_diff_time_us(GET_PWR);
	print_diff_results(GET_PWR);
	#endif
	return ret;
}
EXPORT_SYMBOL(upower_get_power);
