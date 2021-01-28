// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#include "mtk_cpufreq_debug.h"

enum cpu_dvfs_state {
	CPU_DVFS_LL_IS_DOING_DVFS = 0,
	CPU_DVFS_L_IS_DOING_DVFS,
	CPU_DVFS_B_IS_DOING_DVFS,
	CPU_DVFS_CCI_IS_DOING_DVFS,
};

void aee_record_cpu_dvfs_in(struct mt_cpu_dvfs *p)
{
#ifdef CONFIG_MTK_RAM_CONSOLE
	if (p->id == MT_CPU_DVFS_LL)
		aee_rr_rec_cpu_dvfs_status(aee_rr_curr_cpu_dvfs_status() |
					   (1 << CPU_DVFS_LL_IS_DOING_DVFS));
#ifndef ONE_CLUSTER
	else if (p->id == MT_CPU_DVFS_L)
		aee_rr_rec_cpu_dvfs_status(aee_rr_curr_cpu_dvfs_status() |
					   (1 << CPU_DVFS_L_IS_DOING_DVFS));
	else if (p->id == MT_CPU_DVFS_CCI)
		aee_rr_rec_cpu_dvfs_status(aee_rr_curr_cpu_dvfs_status() |
					   (1 << CPU_DVFS_CCI_IS_DOING_DVFS));
	else	/* B */
		aee_rr_rec_cpu_dvfs_status(aee_rr_curr_cpu_dvfs_status() |
					   (1 << CPU_DVFS_B_IS_DOING_DVFS));
#endif
#endif
}

void aee_record_cpu_dvfs_out(struct mt_cpu_dvfs *p)
{
#ifdef CONFIG_MTK_RAM_CONSOLE
	if (p->id == MT_CPU_DVFS_LL)
		aee_rr_rec_cpu_dvfs_status(aee_rr_curr_cpu_dvfs_status() &
					   ~(1 << CPU_DVFS_LL_IS_DOING_DVFS));
#ifndef ONE_CLUSTER
	else if (p->id == MT_CPU_DVFS_L)
		aee_rr_rec_cpu_dvfs_status(aee_rr_curr_cpu_dvfs_status() &
					   ~(1 << CPU_DVFS_L_IS_DOING_DVFS));
	else if (p->id == MT_CPU_DVFS_CCI)
		aee_rr_rec_cpu_dvfs_status(aee_rr_curr_cpu_dvfs_status() &
					   ~(1 << CPU_DVFS_CCI_IS_DOING_DVFS));
	else	/* B */
		aee_rr_rec_cpu_dvfs_status(aee_rr_curr_cpu_dvfs_status() &
					   ~(1 << CPU_DVFS_B_IS_DOING_DVFS));
#endif
#endif
}

void aee_record_cpu_dvfs_step(unsigned int step)	/* step: 0~15 */
{
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_cpu_dvfs_step((aee_rr_curr_cpu_dvfs_step() & 0xF0) | step);
#endif
}

void aee_record_cci_dvfs_step(unsigned int step)	/* step: 0~15 */
{
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_cpu_dvfs_step(
		(aee_rr_curr_cpu_dvfs_step() & 0x0F) | (step << 4));
#endif
}

void aee_record_cpu_dvfs_cb(unsigned int step)	/* step: 0~255 */
{
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_cpu_dvfs_cb(step);
#endif
}

void aee_record_cpufreq_cb(unsigned int step)	/* step: 0~255 */
{
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_cpufreq_cb(step);
#endif
}

void aee_record_cpu_volt(struct mt_cpu_dvfs *p, unsigned int volt)
{
#ifdef CONFIG_MTK_RAM_CONSOLE
	struct buck_ctrl_t *vproc_p = id_to_buck_ctrl(p->Vproc_buck_id);

	if (p->Vproc_buck_id == 0)
		aee_rr_rec_cpu_dvfs_vproc_big(
				vproc_p->buck_ops->transfer2pmicval(volt));
	else
		aee_rr_rec_cpu_dvfs_vproc_little(
				vproc_p->buck_ops->transfer2pmicval(volt));
#endif
}

void aee_record_freq_idx(struct mt_cpu_dvfs *p, int idx)	/* idx: 0~15 */
{
#ifdef CONFIG_MTK_RAM_CONSOLE
	if (p->id == MT_CPU_DVFS_LL)
		aee_rr_rec_cpu_dvfs_oppidx(
			(aee_rr_curr_cpu_dvfs_oppidx() & 0xF0) | idx);
#ifndef ONE_CLUSTER
	else if (p->id == MT_CPU_DVFS_L)
		aee_rr_rec_cpu_dvfs_oppidx(
			(aee_rr_curr_cpu_dvfs_oppidx() & 0x0F) | (idx << 4));
	else if (p->id == MT_CPU_DVFS_CCI)
		aee_rr_rec_cpu_dvfs_cci_oppidx(
			(aee_rr_curr_cpu_dvfs_cci_oppidx() & 0xF0) | idx);
	else	/* B */
		aee_rr_rec_cpu_dvfs_status(
			(aee_rr_curr_cpu_dvfs_status() & 0x0F) | (idx << 4));
#endif
#endif
}

void _mt_cpufreq_aee_init(void)
{
#ifdef CONFIG_MTK_RAM_CONSOLE
	aee_rr_rec_cpu_dvfs_vproc_big(0x0);
	aee_rr_rec_cpu_dvfs_vproc_little(0x0);
	aee_rr_rec_cpu_dvfs_oppidx(0xFF);
	aee_rr_rec_cpu_dvfs_cci_oppidx(0x0F);
	aee_rr_rec_cpu_dvfs_status(0xF0);
	aee_rr_rec_cpu_dvfs_step(0x0);
	aee_rr_rec_cpu_dvfs_cb(0x0);
	aee_rr_rec_cpufreq_cb(0x0);
#endif
}
