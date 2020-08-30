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

#include <linux/list.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched/clock.h> /* local_clock() */
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/random.h>
#include <linux/syscore_ops.h>

#include <linux/pm_qos.h>
#include <helio-dvfsrc.h>
#include "cpu_ctrl.h"
#include "ccci_hif_internal.h"
#include "ccci_platform.h"
#include "ccci_core.h"

#define CALC_DELTA		(1000)
#define MAX_C_NUM		(2)

static struct ppm_limit_data *s_pld;
static int s_cluster_num;
static struct dvfs_ref const *s_dvfs_ref_tbl;
static int s_dvfs_tbl_items_num;

struct speed_mon {
	u64 curr_bytes;
	u64 ref_bytes;
};
static struct speed_mon s_dl_mon, s_ul_mon;

static int s_speed_mon_on;
static wait_queue_head_t s_mon_wq;

void mtk_ccci_add_dl_pkt_size(int size)
{
	if (size <= 0)
		return;
	s_dl_mon.curr_bytes += (u64)size;
	if (!s_speed_mon_on) {
		s_speed_mon_on = 1;
		wake_up_all(&s_mon_wq);
	}
}

void mtk_ccci_add_ul_pkt_size(int size)
{
	if (size <= 0)
		return;
	s_ul_mon.curr_bytes += (u64)size;
	if (!s_speed_mon_on) {
		s_speed_mon_on = 1;
		wake_up_all(&s_mon_wq);
	}
}

void __weak mtk_ccci_affinity_rta(u32 irq_cpus, u32 task_cpus, int cpu_nr)
{
	static int show_cnt = 10;

	if (show_cnt) {
		show_cnt--;
		CCCI_REPEAT_LOG(-1, "Speed", "%s w-\r\n", __func__);
	}
}


/* CPU frequency adjust */
static void cpu_freq_rta_action(int update, int tbl[], int cnum)
{
	int i, num, same;

	num = (s_cluster_num <= cnum) ? s_cluster_num : cnum;
	same = 1;

	for (i = 0; i < num; i++) {
		if (s_pld[i].min != tbl[i]) {
			same = 0;
			s_pld[i].min = tbl[i];
		}
	}

	if (!same) {
		update_userlimit_cpu_freq(CPU_KIR_CCCI, s_cluster_num, s_pld);
		CCCI_REPEAT_LOG(-1, "Speed", "%s new setting\r\n", __func__);
		for (i = 0; i < s_cluster_num; i++)
			CCCI_REPEAT_LOG(-1, "Speed", "c%d:%d\r\n", i,
					s_pld[i].min);
	}
}


/* DRAM qos */
static struct pm_qos_request s_ddr_opp_req;
static void dram_freq_rta_action(int lvl)
{
	static int curr_lvl = -1;

	if (curr_lvl != lvl) {
		curr_lvl = lvl;
		switch (lvl) {
		case 0:
			pm_qos_update_request(&s_ddr_opp_req, DDR_OPP_0);
			CCCI_REPEAT_LOG(-1, "Speed", "%s:DDR_OPP_0\r\n",
						__func__);
			break;
		case 1:
			pm_qos_update_request(&s_ddr_opp_req, DDR_OPP_1);
			CCCI_REPEAT_LOG(-1, "Speed", "%s:DDR_OPP_1\r\n",
						__func__);
			break;
		case -1:
			pm_qos_update_request(&s_ddr_opp_req, DDR_OPP_UNREQ);
			CCCI_REPEAT_LOG(-1, "Speed", "%s:DDR_OPP_UNREQ\r\n",
						__func__);
			break;
		default:
			break;
		}
	}
}

static int notify_data_speed_hint(u64 dl_speed, u64 ul_speed, int curr_idx)
{
	int freq_ref[MAX_C_NUM];
	int i;
	int new_idx;
	u64 speed;

	if ((!s_dvfs_ref_tbl) || (!s_dvfs_tbl_items_num))
		return curr_idx;

	speed = dl_speed + ul_speed;
	new_idx = s_dvfs_tbl_items_num - 1;

	for (i = 0; i < s_dvfs_tbl_items_num; i++) {
		if (speed >= s_dvfs_ref_tbl[i].speed) {
			new_idx = i;
			break;
		}
	}

	if (new_idx == curr_idx)
		return curr_idx;

	if ((new_idx > curr_idx) &&
		((speed + 200000000LL) > s_dvfs_ref_tbl[curr_idx].speed))
		/* avoid pingpang */
		return curr_idx;

	/* CPU freq */
	freq_ref[0] = s_dvfs_ref_tbl[new_idx].c0_freq;
	freq_ref[1] = s_dvfs_ref_tbl[new_idx].c1_freq;
	cpu_freq_rta_action(1, freq_ref, MAX_C_NUM);
	/* DRAM freq */
	dram_freq_rta_action(s_dvfs_ref_tbl[new_idx].dram_lvl);
	/* CPU affinity */
	mtk_ccci_affinity_rta(s_dvfs_ref_tbl[new_idx].irq_affinity,
				s_dvfs_ref_tbl[new_idx].task_affinity, 8);
	/* RPS */
	set_ccmni_rps(s_dvfs_ref_tbl[new_idx].rps);

	return new_idx;
}

static int get_speed_str(u64 speed, char buf[], int size)
{
	int ret;
	u64 rem;

	if (speed >= 1000000000LL) {
#ifdef __LP64__
		speed = speed / 1000000LL;
#else
		speed = do_div(speed, 1000000LL);
#endif
		rem = do_div(speed, 1000);
		ret = snprintf(buf, size, "%llu.%03lluGbps", speed, rem);
	} else if (speed >= 1000000LL) {
		speed = speed / 1000LL;
		rem = do_div(speed, 1000);
		ret = snprintf(buf, size, "%llu.%03lluMbps", speed, rem);
	} else if (speed >= 1000LL) {
		rem = do_div(speed, 1000);
		ret = snprintf(buf, size, "%llu.%03lluKbps", speed, rem);
	} else
		ret = snprintf(buf, size, "%llubps", speed);
	if (ret < 0 || ret >= size) {
		CCCI_REPEAT_LOG(-1, "Speed",
			"%s-%d:snprintf fail,ret=%d\n",
			__func__, __LINE__, ret);
		return -1;
	}

	return ret;
}

static u64 speed_caculate(u64 delta, struct speed_mon *mon)
{
	u64 curr_byte, speed;
#ifndef __LP64__
	u64 tmp;
#endif
	curr_byte = mon->curr_bytes;
#ifdef __LP64__
	speed = (curr_byte - mon->ref_bytes) * 8000000000LL / delta;
#else
	tmp = (curr_byte - mon->ref_bytes) * 8000000000LL;
	speed = do_div(tmp, delta);
#endif
	mon->ref_bytes = curr_byte;

	return speed;
}

struct dvfs_ref * __weak mtk_ccci_get_dvfs_table(int *tbl_num)
{
	*tbl_num = 0;
	return NULL;
}

static int speed_monitor_thread(void *arg)
{
	int cnt, ret;
	u64 curr_tick;
	u64 last_tick;
	u64 delta, dl_speed, ul_speed;
	char dl_speed_str[32];
	char ul_speed_str[32];
	int idx = 0;

	s_dvfs_ref_tbl = mtk_ccci_get_dvfs_table(&s_dvfs_tbl_items_num);
	if (s_dvfs_tbl_items_num > 0)
		idx = s_dvfs_tbl_items_num - 1;

	while (1) {
		if (kthread_should_stop())
			break;

		ret = wait_event_interruptible(s_mon_wq,
				(s_speed_mon_on || kthread_should_stop()));
		if (ret == -ERESTARTSYS)
			continue;

		last_tick = sched_clock();
		cnt = 5;
		while (1) {
			msleep(CALC_DELTA);
			curr_tick = sched_clock();
			delta = curr_tick - last_tick;
			last_tick = curr_tick;

			dl_speed = speed_caculate(delta, &s_dl_mon);
			ul_speed = speed_caculate(delta, &s_ul_mon);
			idx = notify_data_speed_hint(dl_speed, ul_speed, idx);
			get_speed_str(dl_speed, dl_speed_str, 32);
			get_speed_str(ul_speed, ul_speed_str, 32);
			CCCI_REPEAT_LOG(-1, "Speed", "UL:%s, DL:%s[%d]\r\n",
					ul_speed_str, dl_speed_str, idx);

			if (!ul_speed && !dl_speed)
				cnt--;
			if (!cnt) {
				s_speed_mon_on = 0;
				break;
			}
		}
	}

	return 0;
}

int mtk_ccci_speed_monitor_init(void)
{
	int i;

	pm_qos_add_request(&s_ddr_opp_req, PM_QOS_DDR_OPP,
				PM_QOS_DDR_OPP_DEFAULT_VALUE);
	init_waitqueue_head(&s_mon_wq);
	kthread_run(speed_monitor_thread, NULL, "ccci_net_speed_monitor");
	s_cluster_num = arch_get_nr_clusters();
	s_pld = kcalloc(s_cluster_num, sizeof(struct ppm_limit_data),
				GFP_KERNEL);
	if (s_pld) {
		for (i = 0; i < s_cluster_num; i++) {
			s_pld[i].min = -1;
			s_pld[i].max = -1;
		}
	}

	return 0;
}

