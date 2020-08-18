
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

/**
 * @file	mtk_eem_platform.c
 * @brief   Driver for EEM
 *
 */
#define __MTK_EEM_PLATFORM_C__

#include <linux/kernel.h>
#include "mtk_eem_config.h"
#include "mtk_eem.h"
#include "mtk_eem_internal_ap.h"
#include "mtk_eem_internal.h"
#include "mtk_cpufreq_api.h"

/*
 * operation of EEM detectors
 */

struct eemsn_det_ops big_det_ops = {
	.get_volt		= get_volt_cpu,
	.get_freq_table		= get_freq_table_cpu,
	.get_orig_volt_table = get_orig_volt_table_cpu,
};

unsigned int detid_to_dvfsid(struct eemsn_det *det)
{
	unsigned int cpudvfsindex;
	enum eemsn_det_id detid = det_to_id(det);

	if (detid == EEMSN_DET_L)
		cpudvfsindex = MT_CPU_DVFS_LL;
	else if (detid == EEMSN_DET_B)
		cpudvfsindex = MT_CPU_DVFS_L;
#if ENABLE_LOO_B
	else if (detid == EEMSN_DET_B_HI)
		cpudvfsindex = MT_CPU_DVFS_L;
#endif
	else
		cpudvfsindex = MT_CPU_DVFS_CCI;

#if 1
	eem_debug("[%s] id:%d, cpudvfsindex:%d\n", __func__,
		detid, cpudvfsindex);
#endif

	return cpudvfsindex;
}

/* Will return 10uV */
int get_volt_cpu(struct eemsn_det *det)
{
	unsigned int value = 0;
	enum eemsn_det_id cpudvfsindex;

	FUNC_ENTER(FUNC_LV_HELP);
	/* unit mv * 100 = 10uv */
	cpudvfsindex = detid_to_dvfsid(det);
#if SET_PMIC_VOLT_TO_DVFS
	value = mt_cpufreq_get_cur_volt(cpudvfsindex);
#endif
	FUNC_EXIT(FUNC_LV_HELP);
	eem_debug("proc voltage = %d~~~\n", value);
	return value;
}

void get_freq_table_cpu(struct eemsn_det *det)
{
	int i = 0;
	enum mt_cpu_dvfs_id cpudvfsindex;
//#if !DVT
	unsigned int curfreq = 0;
//#endif


	cpudvfsindex = detid_to_dvfsid(det);


	/* Find 2line turn point */
	if (det->isSupLoo) {
		for (i = 0; i < NR_FREQ; i++) {
			curfreq = mt_cpufreq_get_freq_by_idx
			(cpudvfsindex, i);
			if (curfreq <= det->mid_freq_khz) {
				det->turn_pt = i;
				break;
			}
		}
	}

#if 0
	eem_debug("[%s] freq_num:%d, max_freq=%d, turn_pt:%d\n",
			det->name+8, det->num_freq_tbl, det->max_freq_khz,
			det->turn_pt);

#endif
#if DVT
	for (i = 0; i < NR_FREQ; i++) {
		det->freq_tbl[i] = dvtfreq[i];
		if (det->freq_tbl[i] == 0)
			break;
	}
#else

	if (det->max_freq_khz != 0) {
		for (i = 0; i < NR_FREQ; i++) {
			det->freq_tbl[i] = PERCENT(
				mt_cpufreq_get_freq_by_idx(cpudvfsindex, i),
				det->max_freq_khz);
#if 0
			eem_error("id:%d, idx:%d, freq_tbl=%d, orgfreq:%d,\n",
				det->det_id, i, det->freq_tbl[i],
				mt_cpufreq_get_freq_by_idx(cpudvfsindex, i));
#endif
			if (det->freq_tbl[i] == 0)
				break;
		}
	}

#endif

	det->num_freq_tbl = i;

	/* Find 2line freq tbl for low bank */
	if ((det->isSupLoo)) {
		for (i = det->turn_pt; i < det->num_freq_tbl; i++) {
			det->freq_tbl[i] =
			PERCENT(mt_cpufreq_get_freq_by_idx(cpudvfsindex, i),
			det->turn_freq);
		}
	}


}


/* get original volt from cpu dvfs, and apply this table to dvfs
 *   when ptp need to restore volt
 */
void get_orig_volt_table_cpu(struct eemsn_det *det)
{
#if SET_PMIC_VOLT_TO_DVFS
	int i = 0, volt = 0;
	enum mt_cpu_dvfs_id cpudvfsindex;

	FUNC_ENTER(FUNC_LV_HELP);
	cpudvfsindex = detid_to_dvfsid(det);

	for (i = 0; i < det->num_freq_tbl; i++) {
		volt = mt_cpufreq_get_volt_by_idx(cpudvfsindex, i);

		det->volt_tbl_orig[i] =
			(unsigned char)base_ops_volt_2_pmic(det, volt);

#if 0
		eem_error("[%s]@@volt_tbl_orig[%d] = %d(0x%x)\n",
			det->name+8,
			i,
			volt,
			det->volt_tbl_orig[i]);
#endif
	}

#if 0
	/* Use signoff volt */
	memcpy(det->volt_tbl, det->volt_tbl_orig, sizeof(det->volt_tbl));
	memcpy(det->volt_tbl_init2, det->volt_tbl_orig, sizeof(det->volt_tbl));
#endif
	FUNC_EXIT(FUNC_LV_HELP);
#endif
}
/************************************************
 * common det operations for legacy and sspm ptp
 ************************************************
 */
int base_ops_volt_2_pmic(struct eemsn_det *det, int volt)
{
	return (((volt) - det->pmic_base +
		det->pmic_step - 1) / det->pmic_step);
}

int base_ops_volt_2_eem(struct eemsn_det *det, int volt)
{
	return (((volt) - det->eemsn_v_base +
		det->eemsn_step - 1) / det->eemsn_step);
}

int base_ops_pmic_2_volt(struct eemsn_det *det, int pmic_val)
{
	return (((pmic_val) * det->pmic_step) + det->pmic_base);
}

int base_ops_eem_2_pmic(struct eemsn_det *det, int eem_val)
{
	return ((((eem_val) * det->eemsn_step) + det->eemsn_v_base -
			det->pmic_base + det->pmic_step - 1) / det->pmic_step);
}
#undef __MTK_EEM_PLATFORM_C__
