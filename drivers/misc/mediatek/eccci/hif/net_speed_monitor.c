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
#define MAX_C_NUM		(4)

static struct ppm_limit_data *s_pld;
static int s_cluster_num;
static struct dvfs_ref const *s_dl_dvfs_tbl;
static int s_dl_dvfs_items_num;
static struct dvfs_ref const *s_ul_dvfs_tbl;
static int s_ul_dvfs_items_num;

struct speed_mon {
	u64 curr_bytes;
	u64 ref_bytes;
};
static struct speed_mon s_dl_mon, s_ul_mon;

static int s_speed_mon_on;
static wait_queue_head_t s_mon_wq;

struct common_cfg_para {
	int c_frq[MAX_C_NUM];
	int dram_frq_lvl;
};

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

static int s_show_speed_inf;
int mtk_ccci_toggle_net_speed_log(void)
{
	s_show_speed_inf = !s_show_speed_inf;
	return s_show_speed_inf;
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
static void cpu_freq_rta_action(int update, const int tbl[], int cnum)
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

static int dl_speed_hint(u64 speed, int *in_idx, struct common_cfg_para *cfg)
{
	int i;
	int new_idx, idx;
	int middle_speed;

	if ((!s_dl_dvfs_tbl) || (!s_dl_dvfs_items_num) || (!in_idx))
		return -1;

	new_idx = s_dl_dvfs_items_num - 1;
	idx = *in_idx;

	for (i = 0; i < s_dl_dvfs_items_num; i++) {
		if (speed >= s_dl_dvfs_tbl[i].speed) {
			new_idx = i;
			break;
		}
	}

	if (new_idx == idx)
		return 0; /* No change */

	if (new_idx == (idx + 1)) {
		middle_speed = s_dl_dvfs_tbl[new_idx].speed;
		middle_speed += s_dl_dvfs_tbl[idx].speed;
		middle_speed = middle_speed/2;
		if (speed >  middle_speed)
			return 0;
	}

	/* CPU freq hint*/
	cfg->c_frq[0] = s_dl_dvfs_tbl[new_idx].c0_freq;
	cfg->c_frq[1] = s_dl_dvfs_tbl[new_idx].c1_freq;
	cfg->c_frq[2] = s_dl_dvfs_tbl[new_idx].c2_freq;
	cfg->c_frq[3] = s_dl_dvfs_tbl[new_idx].c3_freq;
	/* DRAM freq hint*/
	cfg->dram_frq_lvl = s_dl_dvfs_tbl[new_idx].dram_lvl;
	/* CPU affinity */
	mtk_ccci_affinity_rta(s_dl_dvfs_tbl[new_idx].irq_affinity,
				s_dl_dvfs_tbl[new_idx].task_affinity, 8);
	/* RPS */
	set_ccmni_rps(s_dl_dvfs_tbl[new_idx].rps);

	*in_idx = new_idx;
	return 1;
}

static int ul_speed_hint(u64 speed, int *in_idx, struct common_cfg_para *cfg)
{
	int i;
	int new_idx, idx;
	int middle_speed;

	if ((!s_ul_dvfs_tbl) || (!s_ul_dvfs_items_num) || (!in_idx))
		return -1;

	new_idx = s_ul_dvfs_items_num - 1;
	idx = *in_idx;

	for (i = 0; i < s_ul_dvfs_items_num; i++) {
		if (speed >= s_ul_dvfs_tbl[i].speed) {
			new_idx = i;
			break;
		}
	}

	if (new_idx == idx)
		return 0; /* No change */

	if (new_idx  == (idx + 1)) {
		middle_speed = s_ul_dvfs_tbl[new_idx].speed;
		middle_speed += s_ul_dvfs_tbl[idx].speed;
		middle_speed = middle_speed/2;
		if (speed >  middle_speed)
			return 0;
	}

	/* CPU freq hint*/
	cfg->c_frq[0] = s_ul_dvfs_tbl[new_idx].c0_freq;
	cfg->c_frq[1] = s_ul_dvfs_tbl[new_idx].c1_freq;
	cfg->c_frq[2] = s_ul_dvfs_tbl[new_idx].c2_freq;
	cfg->c_frq[3] = s_ul_dvfs_tbl[new_idx].c3_freq;
	/* DRAM freq hint*/
	cfg->dram_frq_lvl = s_ul_dvfs_tbl[new_idx].dram_lvl;

	*in_idx = new_idx;

	return 1;
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

	if (delta == 0)
		return 0;

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

struct dvfs_ref * __weak mtk_ccci_get_dvfs_table(int is_ul, int *tbl_num)
{
	if (tbl_num)
		*tbl_num = 0;
	return NULL;
}

static char s_dl_speed_str[32], s_ul_speed_str[32];
static int s_dl_cpu_freq[MAX_C_NUM], s_ul_cpu_freq[MAX_C_NUM];
static int s_final_cpu_freq[MAX_C_NUM];
static int s_dl_dram_lvl, s_ul_dram_lvl, s_dram_lvl;
static void dvfs_cal_for_md_net(u64 dl_speed, u64 ul_speed)
{
	static int dl_idx, ul_idx;
	int dl_change, ul_change;
	struct common_cfg_para cfg;
	int i;

	for (i = 0; i < MAX_C_NUM; i++)
		cfg.c_frq[i] = -1;
	cfg.dram_frq_lvl = -1;
	ul_change = ul_speed_hint(ul_speed, &ul_idx, &cfg);
	if (ul_change > 0) {
		for (i = 0; i < MAX_C_NUM; i++)
			s_ul_cpu_freq[i] = cfg.c_frq[i];
		s_ul_dram_lvl = cfg.dram_frq_lvl;
	}

	for (i = 0; i < MAX_C_NUM; i++)
		cfg.c_frq[i] = -1;
	cfg.dram_frq_lvl = -1;
	dl_change = dl_speed_hint(dl_speed, &dl_idx, &cfg);
	if (dl_change > 0) {
		for (i = 0; i < MAX_C_NUM; i++)
			s_dl_cpu_freq[i] = cfg.c_frq[i];
		s_dl_dram_lvl = cfg.dram_frq_lvl;
	}

	if ((dl_change > 0) || (ul_change > 0)) {
		for (i = 0; i < MAX_C_NUM; i++) {
			if (s_dl_cpu_freq[i] <= s_ul_cpu_freq[i])
				s_final_cpu_freq[i] = s_ul_cpu_freq[i];
			else
				s_final_cpu_freq[i] = s_dl_cpu_freq[i];
		}
		cpu_freq_rta_action(1, s_final_cpu_freq, MAX_C_NUM);

		if ((s_dl_dram_lvl >= 0) && (s_ul_dram_lvl >= 0)) {
			if (s_dl_dram_lvl < s_ul_dram_lvl)
				s_dram_lvl = s_dl_dram_lvl;
			else
				s_dram_lvl = s_ul_dram_lvl;
		} else if (s_dl_dram_lvl >= 0)
			s_dram_lvl = s_dl_dram_lvl;
		else if (s_ul_dram_lvl >= 0)
			s_dram_lvl = s_ul_dram_lvl;
		else
			s_dram_lvl = -1;
		dram_freq_rta_action(s_dram_lvl);
	}

	if (s_show_speed_inf) {
		get_speed_str(dl_speed, s_dl_speed_str, 32);
		get_speed_str(ul_speed, s_ul_speed_str, 32);
		pr_info("[SPD]UL[%d:%s], DL[%d:%s]{c0:%d|c1:%d|c2:%d|c3:%d|d:%d}\r\n",
				ul_idx, s_ul_speed_str, dl_idx, s_dl_speed_str,
				s_final_cpu_freq[0], s_final_cpu_freq[1],
				s_final_cpu_freq[2], s_final_cpu_freq[3],
				s_dram_lvl);
	}
}


static int speed_monitor_thread(void *arg)
{
	int cnt, ret;
	u64 curr_tick;
	u64 last_tick;
	u64 delta, dl_speed, ul_speed;

	s_dl_dvfs_tbl = mtk_ccci_get_dvfs_table(0, &s_dl_dvfs_items_num);
	s_ul_dvfs_tbl = mtk_ccci_get_dvfs_table(1, &s_ul_dvfs_items_num);

	while (1) {
		ret = wait_event_interruptible(s_mon_wq,
				(s_speed_mon_on || kthread_should_stop()));
		if (kthread_should_stop())
			break;
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
			dvfs_cal_for_md_net(dl_speed, ul_speed);

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
	for (i = 0; i < MAX_C_NUM; i++) {
		s_dl_cpu_freq[i] = -1;
		s_ul_cpu_freq[i] = -1;
		s_final_cpu_freq[i] = -1;
	}
	s_dl_dram_lvl = -1;
	s_ul_dram_lvl = -1;
	s_dram_lvl = -1;

	return 0;
}

