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
#include "mt_cpufreq_internal.h"

void aee_record_cpu_dvfs_in(struct mt_cpu_dvfs *p)
{
#ifdef CONFIG_CPU_DVFS_AEE_RR_REC
	if (cpu_dvfs_is(p, MT_CPU_DVFS_LL))
		aee_rr_rec_cpu_dvfs_status(aee_rr_curr_cpu_dvfs_status() |
			(1 << CPU_DVFS_LL_IS_DOING_DVFS));
	else if (cpu_dvfs_is(p, MT_CPU_DVFS_L))
		aee_rr_rec_cpu_dvfs_status(aee_rr_curr_cpu_dvfs_status() |
			(1 << CPU_DVFS_L_IS_DOING_DVFS));
	else
		aee_rr_rec_cpu_dvfs_status(aee_rr_curr_cpu_dvfs_status() |
			(1 << CPU_DVFS_B_IS_DOING_DVFS));
#endif
}

void aee_record_cpu_dvfs_out(struct mt_cpu_dvfs *p)
{
#ifdef CONFIG_CPU_DVFS_AEE_RR_REC
	if (cpu_dvfs_is(p, MT_CPU_DVFS_LL))
		aee_rr_rec_cpu_dvfs_status(aee_rr_curr_cpu_dvfs_status() &
			~(1 << CPU_DVFS_LL_IS_DOING_DVFS));
	else if (cpu_dvfs_is(p, MT_CPU_DVFS_L))
		aee_rr_rec_cpu_dvfs_status(aee_rr_curr_cpu_dvfs_status() &
			~(1 << CPU_DVFS_L_IS_DOING_DVFS));
	else
		aee_rr_rec_cpu_dvfs_status(aee_rr_curr_cpu_dvfs_status() &
			~(1 << CPU_DVFS_B_IS_DOING_DVFS));
#endif
}

void aee_record_cpu_dvfs_step(unsigned int step)
{
#ifdef CONFIG_CPU_DVFS_AEE_RR_REC
	if (step == 0)
		aee_rr_rec_cpu_dvfs_step(aee_rr_curr_cpu_dvfs_step() & 0xF0);
	else
		aee_rr_rec_cpu_dvfs_step((aee_rr_curr_cpu_dvfs_step() & 0xF0) | (step));
#endif
}

void aee_record_cci_dvfs_step(unsigned int step)
{
#ifdef CONFIG_CPU_DVFS_AEE_RR_REC
	if (step == 0)
		aee_rr_rec_cpu_dvfs_step(aee_rr_curr_cpu_dvfs_step() & 0x0F);
	else
		aee_rr_rec_cpu_dvfs_step((aee_rr_curr_cpu_dvfs_step() & 0x0F) | (step << 4));
#endif
}

void aee_record_cpu_dvfs_cb(unsigned int step)
{
#ifdef CONFIG_CPU_DVFS_AEE_RR_REC
	if (step == 0)
		aee_rr_rec_cpu_dvfs_cb(aee_rr_curr_cpu_dvfs_cb() & 0x0);
	else
		aee_rr_rec_cpu_dvfs_cb((aee_rr_curr_cpu_dvfs_cb() & 0x0) | (step));
#endif
}

void aee_record_cpu_volt(struct mt_cpu_dvfs *p, unsigned int volt)
{
#ifdef CONFIG_CPU_DVFS_AEE_RR_REC
	struct buck_ctrl_t *vproc_p = id_to_buck_ctrl(p->Vproc_buck_id);

	if (cpu_dvfs_is(p, MT_CPU_DVFS_B))
		aee_rr_rec_cpu_dvfs_vproc_big(vproc_p->buck_ops->transfer2pmicval(volt));
	else
		aee_rr_rec_cpu_dvfs_vproc_little(vproc_p->buck_ops->transfer2pmicval(volt));
#endif
}

void aee_record_freq_idx(struct mt_cpu_dvfs *p, int idx)
{
#ifdef CONFIG_CPU_DVFS_AEE_RR_REC
	if (cpu_dvfs_is(p, MT_CPU_DVFS_LL))
		aee_rr_rec_cpu_dvfs_oppidx((aee_rr_curr_cpu_dvfs_oppidx() & 0xF0) | idx);
	else if (cpu_dvfs_is(p, MT_CPU_DVFS_L))
		aee_rr_rec_cpu_dvfs_oppidx((aee_rr_curr_cpu_dvfs_oppidx() & 0x0F) | (idx << 4));
	else if (cpu_dvfs_is(p, MT_CPU_DVFS_B))
		aee_rr_rec_cpu_dvfs_status((aee_rr_curr_cpu_dvfs_status() & 0x0F) | (idx << 4));
	else
		aee_rr_rec_cpu_dvfs_cci_oppidx((aee_rr_curr_cpu_dvfs_cci_oppidx() & 0xF0) | idx);
#endif
}

void _mt_cpufreq_aee_init(void)
{
#ifdef CONFIG_CPU_DVFS_AEE_RR_REC
	aee_rr_rec_cpu_dvfs_vproc_big(0xFF);
	aee_rr_rec_cpu_dvfs_vproc_little(0xFF);
	aee_rr_rec_cpu_dvfs_oppidx(0xFF);
	aee_rr_rec_cpu_dvfs_status(0xF0);
	aee_rr_rec_cpu_dvfs_step(0x0);
	aee_rr_rec_cpu_dvfs_cb(0x0);
#endif
}
