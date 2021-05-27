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
#include <linux/cpufreq.h>
#include <linux/interconnect.h>
#include <mt-plat/dvfsrc-exp.h>
#include <linux/of.h>
#include "ccci_hif_internal.h"
#include "ccci_platform.h"
#include "ccci_core.h"
#include "ccci_hif_dpmaif_comm.h"


#define TAG "Speed"

#define CALC_DELTA		(1000)
#define MAX_C_NUM		(4)

static int net_spd_status;
static int ap_plat;
static int s_cluster_num;
static int *target_freq;
static struct freq_qos_request *tchbst_rq;
static int *target_freq;
static struct device_node *np_node;
static struct icc_path *net_icc_path;
static struct dvfs_ref const *s_dl_dvfs_tbl;
static int s_dl_dvfs_items_num;
static struct dvfs_ref const *s_ul_dvfs_tbl;
static int s_ul_dvfs_items_num;


struct dvfs_ref {
	u64 speed;
	int c0_freq; /* Cluster 0 */
	int c1_freq; /* Cluster 1 */
	int c2_freq; /* Cluster 2 */
	int c3_freq; /* Cluster 3 */
	u8 dram_lvl;
	u8 irq_affinity;
	u8 task_affinity;
	u8 rps;
};

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

/* APK setting */
static  struct dvfs_ref s_dl_dvfs_tbl_6873[] = {
	/*speed, cluster0, cluster1, cluster2, cluster3, dram, isr, push, rps*/
	{1700000000LL, 1530000, 1526000, -1, -1, 0, 0x02, 0xF0, 0xF0},
	{1350000000LL, 1530000, 1526000, -1, -1, 1, 0x02, 0xF0, 0xF0},
	{1000000000LL, 1300000, 1406000, -1, -1, 1, 0x02, 0xF0, 0xF0},
	{450000000LL, 1200000, 1406000, -1, -1, 1, 0x02, 0xF0, 0xF0},
	{230000000LL, 1181000, -1, -1, -1, 1, 0xFF, 0xFF, 0x0D},
	{5000000LL, -1, -1, -1, -1, 1, 0xFF, 0xFF, 0x0D},
	/* normal */
	{0LL, -1, -1, -1, -1, -1, 0xFF, 0xFF, 0x0D},
};

static  struct dvfs_ref s_ul_dvfs_tbl_6873[] = {
	/*speed, cluster0, cluster1, cluster2, cluster3, dram, isr, push, rps*/
	{600000000LL, 2700000, 2706000, -1, -1, 0, 0x02, 0xF0, 0xF0},
	{500000000LL, 1700000, 1706000, -1, -1, 0, 0x02, 0xF0, 0xF0},
	{300000000LL, 1500000, 1500000, -1, -1, 1, 0xFF, 0xFF, 0x0D},
	{250000000LL, -1, -1, -1, -1, -1, 0xFF, 0xFF, 0x0D},
	/* normal */
	{0LL, -1, -1, -1, -1, -1, 0xFF, 0xFF, 0x0D},
};

/* APK setting */
static  struct dvfs_ref s_dl_dvfs_tbl_6853[] = {
	/*speed, cluster0, cluster1, cluster2, cluster3, dram, isr, push, rps*/
	{1700000000LL, 1530000, 1526000, -1, -1, 0, 0x02, 0xC0, 0xC0},
	{1350000000LL, 1530000, 1526000, -1, -1, 1, 0x02, 0xC0, 0xC0},
	{1000000000LL, 1300000, 1406000, -1, -1, 1, 0x02, 0xC0, 0xC0},
	{450000000LL, 1200000, 1406000, -1, -1, 1, 0x02, 0xC0, 0xC0},
	{230000000LL, 1181000, -1, -1, -1, 1, 0xFF, 0xFF, 0x3D},
	{50000000LL, -1, -1, -1, -1, 1, 0xFF, 0xFF, 0x0D},
	/* normal */
	{0LL, -1, -1, -1, -1, -1, 0xFF, 0xFF, 0x0D},
};

static  struct dvfs_ref s_ul_dvfs_tbl_6853[] = {
	/*speed, cluster0, cluster1, cluster2, cluster3, dram, isr, push, rps*/
	{600000000LL, 2700000, 2706000, -1, -1, 0, 0x02, 0xF0, 0xF0},
	{500000000LL, 1700000, 1706000, -1, -1, 0, 0x02, 0xF0, 0xF0},
	{300000000LL, 1500000, 1500000, -1, -1, 1, 0xFF, 0xFF, 0x0D},
	{250000000LL, -1, -1, -1, -1, -1, 0xFF, 0xFF, 0x0D},
	/* normal */
	{0LL, -1, -1, -1, -1, -1, 0xFF, 0xFF, 0x0D},
};

struct dvfs_ref *mtk_ccci_get_dvfs_table(int is_ul, int *tbl_num)
{
	struct dvfs_ref *ret;

	if (!tbl_num)
		return NULL;

	switch (ap_plat) {
	case 6853:
		if (is_ul) {
			/* Query UL settings */
			*tbl_num = (int)ARRAY_SIZE(s_ul_dvfs_tbl_6853);
			ret = s_ul_dvfs_tbl_6853;
		} else {
			/* DL settings */
			*tbl_num = (int)ARRAY_SIZE(s_dl_dvfs_tbl_6853);
			ret = s_dl_dvfs_tbl_6853;
		}
		break;
	case 6873:
	default:
		if (is_ul) {
			/* Query UL settings */
			*tbl_num = (int)ARRAY_SIZE(s_ul_dvfs_tbl_6873);
			ret = s_ul_dvfs_tbl_6873;
		} else {
			/* DL settings */
			*tbl_num = (int)ARRAY_SIZE(s_dl_dvfs_tbl_6873);
			ret = s_dl_dvfs_tbl_6873;
		}
		break;
	}

	return ret;
}

void mtk_ccci_add_dl_pkt_size(int size)
{
	if (size <= 0 || !net_spd_status)
		return;
	s_dl_mon.curr_bytes += (u64)size;
	if (!s_speed_mon_on) {
		s_speed_mon_on = 1;
		wake_up_all(&s_mon_wq);
	}
}

void mtk_ccci_add_ul_pkt_size(int size)
{
	if (size <= 0 || !net_spd_status)
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

/* CPU frequency adjust */
static void cpu_freq_rta_action(int update, const int tbl[], int cnum)
{
	int i, num, same;

	num = (s_cluster_num <= cnum) ? s_cluster_num : cnum;
	same = 1;

	for (i = 0; i < num; i++) {
		if (target_freq[i] != tbl[i]) {
			same = 0;
			target_freq[i] = tbl[i];
		}
	}

	if (!same) {
		CCCI_REPEAT_LOG(-1, "Speed", "%s new setting\r\n", __func__);
		for (i = 0; i < s_cluster_num; i++) {
			freq_qos_update_request(&(tchbst_rq[i]),
				target_freq[i]);
			CCCI_REPEAT_LOG(-1, "Speed", "c%d:%d\r\n", i,
					target_freq[i]);
		}
	}
}


/* DRAM qos */
static void dram_freq_rta_action(int lvl)
{
	static int curr_lvl = -1;
	unsigned int peak_bw;

	if (!net_icc_path || !np_node)
		return;
	if (curr_lvl != lvl) {
		curr_lvl = lvl;
		switch (lvl) {
		case 0:
			peak_bw = dvfsrc_get_required_opp_peak_bw(np_node, 0);
			icc_set_bw(net_icc_path, 0, peak_bw);
			CCCI_REPEAT_LOG(-1, "Speed", "%s:icc_set_bw 0\n",
						__func__);
			break;
		case 1:
			peak_bw = dvfsrc_get_required_opp_peak_bw(np_node, 1);
			icc_set_bw(net_icc_path, 0, peak_bw);
			CCCI_REPEAT_LOG(-1, "Speed", "%s:icc_set_bw 1\n",
						__func__);
			break;
		case -1:
			icc_set_bw(net_icc_path, 0, 0);
			CCCI_REPEAT_LOG(-1, "Speed", "%s:icc_set_bw clear\n",
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
	int dram_frq_lvl;

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
	dram_frq_lvl = s_dl_dvfs_tbl[new_idx].dram_lvl;
	/* CPU affinity */
	if (g_md_gen == 6298)
		mtk_ccci_affinity_rta_v3(s_dl_dvfs_tbl[new_idx].irq_affinity,
					s_dl_dvfs_tbl[new_idx].task_affinity, 8);
	else
		mtk_ccci_affinity_rta_v2(s_dl_dvfs_tbl[new_idx].irq_affinity,
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
	int dram_frq_lvl;

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
	dram_frq_lvl = s_ul_dvfs_tbl[new_idx].dram_lvl;

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

static u64 speed_calculate(u64 delta, struct speed_mon *mon)
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

			dl_speed = speed_calculate(delta, &s_dl_mon);
			ul_speed = speed_calculate(delta, &s_ul_mon);
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

int mtk_ccci_speed_monitor_init(struct device *dev)
{
	int i, ret;
	int cpu;
	int num = 0;
	struct cpufreq_policy *policy;

	/* query interconnect parameters */
	if (!dev)
		return -1;
	net_icc_path = of_icc_get(dev, "icc-dpmaif-bw");
	if (!net_icc_path)
		CCCI_ERROR_LOG(-1, TAG, "Get icc-dpmaif-bw path fail\n");
	np_node = dev->of_node;
	if (!np_node) {
		CCCI_ERROR_LOG(-1, TAG, "No dpmaif driver in dtsi\n");
		return -2;
	}
	ret = of_property_read_u32(np_node,
			"mediatek,ap_plat_info", &ap_plat);
	if (ret) {
		ap_plat = 6873;
		CCCI_NORMAL_LOG(-1, TAG, "ap_plat default = %d", ap_plat);
	} else
		CCCI_NORMAL_LOG(-1, TAG, "ap_plat = %d", ap_plat);
	/* query policy number */
	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);

		if (policy) {
			pr_info("%s, policy[%d]: first:%d, min:%d, max:%d",
				__func__, num, cpu, policy->min, policy->max);
			num++;
			cpu = cpumask_last(policy->related_cpus);
		}
	}
	s_cluster_num = num;
	if (s_cluster_num == 0) {
		pr_info("%s, no policy", __func__);
		return -3;
	}
	tchbst_rq = kcalloc(s_cluster_num, sizeof(struct freq_qos_request), GFP_KERNEL);
	if (tchbst_rq == NULL) {
		CCCI_ERROR_LOG(-1, TAG, "%s tchbst_rq fail\n", __func__);
		return -4;
	}
	target_freq = kcalloc(s_cluster_num, sizeof(int), GFP_KERNEL);
	if (target_freq)
		for (i = 0; i < s_cluster_num; i++)
			target_freq[i] = -1;
	else {
		CCCI_ERROR_LOG(-1, TAG, "%s target_freq fail\n", __func__);
		kfree(tchbst_rq);
		return -5;
	}
	num = 0;
	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			continue;
		freq_qos_add_request(&policy->constraints, &(tchbst_rq[num]), FREQ_QOS_MIN, 0);
		num++;
		cpu = cpumask_last(policy->related_cpus);
	}

	for (i = 0; i < MAX_C_NUM; i++) {
		s_dl_cpu_freq[i] = -1;
		s_ul_cpu_freq[i] = -1;
		s_final_cpu_freq[i] = -1;
	}
	s_dl_dram_lvl = -1;
	s_ul_dram_lvl = -1;
	s_dram_lvl = -1;

	init_waitqueue_head(&s_mon_wq);
	kthread_run(speed_monitor_thread, NULL, "ccci_net_speed_monitor");

	net_spd_status = 1;
	return 0;
}

