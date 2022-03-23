// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
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
#if IS_ENABLED(CONFIG_MTK_DVFSRC)
#include <helio-dvfsrc.h>
#endif
#include "cpu_ctrl.h"
#include "ccci_hif_internal.h"
#include "ccci_platform.h"
#include "ccci_core.h"
#include "mtk_ppm_api.h"
#if !defined(CONFIG_MACH_MT6771)
#include <linux/soc/mediatek/mtk-pm-qos.h>
#endif
#define CALC_DELTA		(1000)
#define MAX_C_NUM		(4)

struct spd_ds_ref {
	int cx_freq[MAX_C_NUM]; /* Cluster 0 ~ 4 */
	int dram_frq_lvl;
	u32 irq_affinity;
	u32 task_affinity;
	u32 rps;
};

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

void mtk_ccci_add_dl_pkt_size(int size)
{
	if (size <= 0)
		return;
	s_dl_mon.curr_bytes += (u64)size;
	if (!s_speed_mon_on) {
		s_speed_mon_on = 1;
		//fix KE
		//wake_up_all(&s_mon_wq);
	}
}

void mtk_ccci_add_ul_pkt_size(int size)
{
	if (size <= 0)
		return;
	s_ul_mon.curr_bytes += (u64)size;
	if (!s_speed_mon_on) {
		s_speed_mon_on = 1;
		//fix KE
		//wake_up_all(&s_mon_wq);
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
		/* Need fix,API change */
		//update_userlimit_cpu_freq(CPU_KIR_CCCI, s_cluster_num, s_pld);
		CCCI_REPEAT_LOG(-1, "Speed", "%s new setting\r\n", __func__);
		for (i = 0; i < s_cluster_num; i++)
			CCCI_REPEAT_LOG(-1, "Speed", "c%d:%d\r\n", i,
					s_pld[i].min);
	}
}


/* DRAM qos */
/* Need fix,API change */
//static struct pm_qos_request s_ddr_opp_req;
static void dram_freq_rta_action(int lvl)
{
	static int curr_lvl = -1;

	if (curr_lvl != lvl) {
		curr_lvl = lvl;
		switch (lvl) {
		case 0:
			/* Need fix,API change */
			//mtk_pm_qos_update_request(&s_ddr_opp_req, DDR_OPP_0);
			CCCI_REPEAT_LOG(-1, "Speed", "%s:DDR_OPP_0\r\n",
						__func__);
			break;
		case 1:
			//mtk_pm_qos_update_request(&s_ddr_opp_req, DDR_OPP_1);
			CCCI_REPEAT_LOG(-1, "Speed", "%s:DDR_OPP_1\r\n",
						__func__);
			break;
		case -1:
			//mtk_pm_qos_update_request(&s_ddr_opp_req, DDR_OPP_UNREQ);
			CCCI_REPEAT_LOG(-1, "Speed", "%s:DDR_OPP_UNREQ\r\n",
						__func__);
			break;
		default:
			break;
		}
	}
}

static int dl_speed_hint(u64 speed, int *in_idx, struct spd_ds_ref *cfg)
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
	cfg->cx_freq[0] = s_dl_dvfs_tbl[new_idx].c0_freq;
	cfg->cx_freq[1] = s_dl_dvfs_tbl[new_idx].c1_freq;
	cfg->cx_freq[2] = s_dl_dvfs_tbl[new_idx].c2_freq;
	cfg->cx_freq[3] = s_dl_dvfs_tbl[new_idx].c3_freq;
	/* DRAM freq hint*/
	cfg->dram_frq_lvl = s_dl_dvfs_tbl[new_idx].dram_lvl;
	/* irq affinity */
	cfg->irq_affinity = s_dl_dvfs_tbl[new_idx].irq_affinity;
	/* task affinity */
	cfg->task_affinity = s_dl_dvfs_tbl[new_idx].task_affinity;
	/* rps */
	cfg->rps = s_dl_dvfs_tbl[new_idx].rps;

	*in_idx = new_idx;
	return 1;
}

static int ul_speed_hint(u64 speed, int *in_idx, struct spd_ds_ref *cfg)
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
	cfg->cx_freq[0] = s_ul_dvfs_tbl[new_idx].c0_freq;
	cfg->cx_freq[1] = s_ul_dvfs_tbl[new_idx].c1_freq;
	cfg->cx_freq[2] = s_ul_dvfs_tbl[new_idx].c2_freq;
	cfg->cx_freq[3] = s_ul_dvfs_tbl[new_idx].c3_freq;
	/* DRAM freq hint*/
	cfg->dram_frq_lvl = s_ul_dvfs_tbl[new_idx].dram_lvl;
	/* irq affinity */
	cfg->irq_affinity = s_ul_dvfs_tbl[new_idx].irq_affinity;
	/* task affinity */
	cfg->task_affinity = s_ul_dvfs_tbl[new_idx].task_affinity;
	/* rps */
	cfg->rps = s_ul_dvfs_tbl[new_idx].rps;

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
static int s_final_cpu_freq[MAX_C_NUM];
static int s_dl_dram_lvl, s_ul_dram_lvl, s_dram_lvl;
static int s_dram_lvl;
static struct spd_ds_ref s_dl_ref, s_ul_ref;
unsigned int s_isr_affinity, s_task_affinity, s_rps;

static void dvfs_cal_for_md_net(u64 dl_speed, u64 ul_speed)
{
	static int dl_idx, ul_idx;
	int dl_change, ul_change;
	int i;

	ul_change = ul_speed_hint(ul_speed, &ul_idx, &s_ul_ref);
	dl_change = dl_speed_hint(dl_speed, &dl_idx, &s_dl_ref);

	if ((dl_change > 0) || (ul_change > 0)) {
		/* CPU cluster frequency setting */
		for (i = 0; i < MAX_C_NUM; i++) {
			if (s_ul_ref.cx_freq[i] <= s_dl_ref.cx_freq[i])
				s_final_cpu_freq[i] = s_dl_ref.cx_freq[i];
			else
				s_final_cpu_freq[i] = s_ul_ref.cx_freq[i];
		}
		cpu_freq_rta_action(1, s_final_cpu_freq, MAX_C_NUM);

		/* DRAM frequency setting */
		s_dl_dram_lvl = s_dl_ref.dram_frq_lvl;
		s_ul_dram_lvl = s_ul_ref.dram_frq_lvl;
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

		/* CPU affinity setting */
		s_isr_affinity = s_ul_ref.irq_affinity & s_dl_ref.irq_affinity;
		if (!s_isr_affinity) {
			pr_info("[SPD]ISR affinity: [ul:%x|dl:%x]\r\n",
				s_ul_ref.irq_affinity,
				s_dl_ref.irq_affinity);
			s_isr_affinity = 0xFF;
		}
		s_task_affinity =
			s_ul_ref.task_affinity & s_dl_ref.task_affinity;
		if (!s_task_affinity) {
			pr_info("[SPD]Task affinity: [ul:%x|dl:%x]\r\n",
				s_ul_ref.task_affinity,
				s_dl_ref.task_affinity);
			s_task_affinity = 0xFF;
		}
		mtk_ccci_affinity_rta(s_isr_affinity, s_task_affinity, 8);

		/* RPS setting */
		s_rps = s_dl_ref.rps;
		if (s_ul_ref.rps & 0xC0)
			s_rps = s_ul_ref.rps;
		/* Need fix,API change */
		//set_ccmni_rps(s_rps);
	}

	if (s_show_speed_inf) {
		get_speed_str(dl_speed, s_dl_speed_str, 32);
		get_speed_str(ul_speed, s_ul_speed_str, 32);
		pr_info("[SPD]UL[%d:%s], DL[%d:%s]{c0:%d|c1:%d|c2:%d|c3:%d|d:%d|i:0x%x|p:0x%x|r:0x%x}\r\n",
				ul_idx, s_ul_speed_str, dl_idx, s_dl_speed_str,
				s_final_cpu_freq[0], s_final_cpu_freq[1],
				s_final_cpu_freq[2], s_final_cpu_freq[3],
				s_dram_lvl, s_isr_affinity, s_task_affinity,
				s_rps);
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

	while (!kthread_should_stop()) {
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
	/* Need fix,API change */
	int i;

/* Need fix,API change */
#if 0
	mtk_pm_qos_add_request(&s_ddr_opp_req, MTK_PM_QOS_DDR_OPP,
				MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE);
	init_waitqueue_head(&s_mon_wq);
#endif
	kthread_run(speed_monitor_thread, NULL, "ccci_net_speed_monitor");
	//s_cluster_num = arch_get_nr_clusters();
	s_pld = kcalloc(s_cluster_num, sizeof(struct ppm_limit_data),
				GFP_KERNEL);
	if (s_pld) {
		for (i = 0; i < s_cluster_num; i++) {
			s_pld[i].min = -1;
			s_pld[i].max = -1;
		}
	}

	s_dl_dram_lvl = -1;
	s_ul_dram_lvl = -1;
	s_dram_lvl = -1;

	return 0;
}

