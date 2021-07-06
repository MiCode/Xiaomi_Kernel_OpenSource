/*
 * Copyright (C) 2018 MediaTek Inc.
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
#include <linux/kernel.h>
#include <linux/delay.h>

#include <mtk_mcdi.h>
#include <mtk_mcdi_governor.h>
#include <mtk_mcdi_util.h>

#include <mtk_mcdi_plat.h>
#include <mtk_mcdi_reg.h>

#include <mt-plat/mtk_secure_api.h>
#include <trace/events/mtk_idle_event.h>

#ifdef MCDI_CPC_MODE

#define CPC_RET_RETRY      0  /* 2'b00 */
#define CPC_RET_SUCCESS    1  /* 2'b01 */
#define CPC_RET_GIVE_UP    2  /* 2'b10 */

#define udelay_and_update_wait_time(dur, t) \
		{ udelay(t); (dur) += (t); }
#define mdelay_and_update_wait_time(dur, t) \
		{ mdelay(t); (dur) += (1000 * (t)); }

static void __release_last_core_prot(unsigned int clr)
{
	mt_secure_call(MTK_SIP_KERNEL_MCDI_ARGS,
			MCDI_SMC_EVENT_LAST_CORE_CLR,
			clr, 0, 0);
}

void release_last_core_prot(void)
{
	__release_last_core_prot(1U << 9);
	mcdi_cpc_auto_off_counter_resume();
}

void release_cluster_last_core_prot(void)
{
	if (cpc_cpusys_off_hw_prot())
		return;

	/* Avoid the mask was not cleared while ATF abort */
	if (mcdi_read(CPC_PWR_ON_MASK))
		__release_last_core_prot(1U << 8);
}

#ifdef NON_SECURE_REQ
#define last_core_req(req)	mcdi_write(CPC_LAST_CORE_REQ, req)
#else
#define last_core_req(req)					\
		mt_secure_call(MTK_SIP_KERNEL_MCDI_ARGS,	\
				MCDI_SMC_EVENT_LAST_CORE_REQ,	\
				req,				\
				0, 0)
#endif

int acquire_last_core_prot(int cpu)
{
	unsigned int ret;
	unsigned int dur_us = 0;

	do {
		last_core_req(mcusys_last_core_req);

		udelay_and_update_wait_time(dur_us, 2);

		if (!is_last_core_in_mcusys(cpu) || dur_us > 1000) {
			__release_last_core_prot(1U << 9);
			ret = CPC_RET_GIVE_UP;
			break;
		}

		ret = get_mcusys_last_core_ack(
				mcdi_read(CPC_MCUSYS_LAST_CORE_RESP));

	} while (ret == CPC_RET_RETRY);

	if (ret == CPC_RET_SUCCESS) {
		mcdi_cpc_auto_off_counter_suspend();
		return 0;
	} else if (ret == CPC_RET_GIVE_UP) {
		any_core_cpu_cond_inc(MULTI_CORE_CNT);
	}

	return -1;
}

int acquire_cluster_last_core_prot(int cpu)
{
	unsigned int ret;
	unsigned int dur_us = 0;

	if (cpc_cpusys_off_hw_prot())
		return 0;

	do {
		last_core_req(cpusys_last_core_req);

		udelay_and_update_wait_time(dur_us, 2);

		if (!is_last_core_in_cluster(cpu) || dur_us > 1000) {
			__release_last_core_prot(1U << 8);
			ret = CPC_RET_GIVE_UP;
			break;
		}

		ret = get_cpusys_last_core_ack(
				mcdi_read(CPC_CPUSYS_LAST_CORE_RESP));

	} while (ret == CPC_RET_RETRY);

	return ret == CPC_RET_SUCCESS ? 0 : -1;
}

void mcdi_ap_ready(void)
{
	mcdi_mbox_write(MCDI_MBOX_AP_READY, 1);
}

#else

static bool is_cpu_pwr_on_event_pending(void)
{
	return (!(mcdi_mbox_read(MCDI_MBOX_PENDING_ON_EVENT) == 0));
}

static bool mcdi_cpu_cluster_on_off_stat_check(int cpu)
{
	bool ret = false;
	unsigned int on_off_stat = 0;
	unsigned int check_mask;
	unsigned int cpu_mask;
	unsigned int cluster_mask;

	get_mcdi_avail_mask(&cpu_mask, &cluster_mask);
	check_mask = (get_pwr_stat_check_map(CPU_CLUSTER, cpu)
					& ((cluster_mask << 16) | cpu_mask));

	on_off_stat = mcdi_get_raw_pwr_sta();

	if (on_off_stat == check_mask)
		ret = true;

	trace_mcdi_cpu_cluster_stat_rcuidle(cpu, on_off_stat, check_mask);

	return ret;
}

/*
 * if ALL CPUs in other cluster is power OFF, but the other cluster is power ON,
 * means other cluster can NOT powered OFF due to residency condition failed
 * Therefore we can skip checking any core dpidle/SODI conditions
 */
static bool other_cpu_off_but_cluster_on(int cpu)
{
	unsigned int on_off_stat = 0;
	unsigned int other_cluster_check_mask;
	unsigned int cpu_check_mask;
	unsigned int cpu_mask;
	unsigned int cluster_mask;

	get_mcdi_avail_mask(&cpu_mask, &cluster_mask);

	other_cluster_check_mask =
		get_pwr_stat_check_map(CPU_CLUSTER, cpu) & (cluster_mask << 16);

	cpu_check_mask =
		get_pwr_stat_check_map(CPU_IN_OTHER_CLUSTER, cpu) & cpu_mask;

	on_off_stat = mcdi_get_raw_pwr_sta();

	if (cpu_check_mask == 0 && other_cluster_check_mask == 0)
		return false;

	if (((on_off_stat & cpu_check_mask) == cpu_check_mask)
		&& (on_off_stat & other_cluster_check_mask) == 0) {
		return true;
	}

	return false;
}

static bool is_match_cpu_cluster_criteria(int cpu)
{
	bool match = true;

	match = mcdi_cpu_cluster_on_off_stat_check(cpu)
		&& !is_cpu_pwr_on_event_pending();

	return match;
}

static bool mcdi_controller_token_get_no_pause(int cpu)
{
	bool token_get = false;
	unsigned long long loop_delay_start_us = 0;
	unsigned long long loop_delay_curr_us = 0;

	/*
	 * Try to get token from MCDI controller,
	 * which means only 1 CPU power ON
	 */

	loop_delay_start_us = idle_get_current_time_us();

	/* wait until other CPUs powered OFF */
	while (true) {

		if (is_cpu_pwr_on_event_pending()) {
			token_get = false;
			break;
		}

		if (!is_last_core_in_mcusys(cpu)) {
			token_get = false;
			break;
		}

		if (other_cpu_off_but_cluster_on(cpu)) {
			token_get = false;
			break;
		}

		if (is_match_cpu_cluster_criteria(cpu)) {
			token_get = true;
			break;
		}

		loop_delay_curr_us = idle_get_current_time_us();

		if ((loop_delay_curr_us - loop_delay_start_us) > 2000)
			break;
	}

	return token_get;
}

static bool mcdi_controller_token_get(int cpu)
{
	if (is_cpu_pwr_on_event_pending())
		return false;

	if (!is_last_core_in_mcusys(cpu))
		return false;

	if (other_cpu_off_but_cluster_on(cpu))
		return false;

	if (is_match_cpu_cluster_criteria(cpu))
		return true;

	return false;
}

static void dump_multi_core_state_ftrace(int cpu)
{
	unsigned int on_off_stat  = 0;
	unsigned int check_mask;
	unsigned int cpu_mask;
	unsigned int cluster_mask;

	get_mcdi_avail_mask(&cpu_mask, &cluster_mask);

	check_mask = (get_pwr_stat_check_map(CPU_CLUSTER, cpu)
				& ((cluster_mask << 16) | cpu_mask));

	on_off_stat = mcdi_get_raw_pwr_sta();

	trace_mcdi_multi_core_rcuidle(cpu, on_off_stat, check_mask);
}

void release_last_core_prot(void)
{
	_mcdi_task_pause(false);
}

void release_cluster_last_core_prot(void)
{
}

int acquire_last_core_prot(int cpu)
{
	bool token_get = false;
	bool mcdi_task_paused = false;

	token_get = mcdi_controller_token_get_no_pause(cpu);

	if (token_get) {

		any_core_cpu_cond_inc(PAUSE_CNT);

		_mcdi_task_pause(true);
		mcdi_task_paused = true;

		token_get = mcdi_controller_token_get(cpu);
	}

	if (!token_get) {
		dump_multi_core_state_ftrace(cpu);
		any_core_cpu_cond_inc(MULTI_CORE_CNT);
	} else {
		return 0;
	}

	if (mcdi_task_paused)
		_mcdi_task_pause(false);

	return -1;
}

int acquire_cluster_last_core_prot(int cpu)
{
	return 0;
}

void mcdi_ap_ready(void)
{
}

#endif /* MCDI_CPC_MODE */

