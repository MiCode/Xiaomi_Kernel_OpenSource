// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

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

#include "net_speed.h"

#define DL_QUE_NUM 1
#define UL_QUE_NUM 3

#define MAX_CALL_BACK_FUNC_NUM	5


enum {
	SPD_DL = 0,
	SPD_UL = 1,
};

struct speed_cal_t {
	u32                   m_speed_cal_en;
	//struct kthread_worker m_kworker;
	//struct kthread_work   m_kwork;
	struct task_struct   *m_kworker_thread;
	wait_queue_head_t     m_speed_wq;
};

struct speed_t {
	u64 m_curr_bytes;
	u64 m_s_1s;
	u64 m_s_500ms;
	u64 m_sample_e;

	u64 m_speed;
	u64 m_speed_500ms;
};


static struct speed_cal_t s_spd_mon;
static struct speed_t s_spd_ctl[DL_QUE_NUM + UL_QUE_NUM];
static char s_dl_speed_inf[32 * DL_QUE_NUM];
static char s_ul_speed_inf[32 * UL_QUE_NUM];
static int s_show;
static spd_fun s_spd_func_1s[MAX_CALL_BACK_FUNC_NUM];
static total_spd_fun s_spd_dl_func_1s[MAX_CALL_BACK_FUNC_NUM];
static spd_fun s_spd_func_500ms[MAX_CALL_BACK_FUNC_NUM];
static unsigned int s_spd_cb_num_1s, s_spd_dl_cb_num_1s, s_spd_cb_num_500ms;


static struct speed_t *get_speed_ctl(int qno, int dir)
{
	if ((dir == SPD_DL) && (qno < DL_QUE_NUM))
		return &s_spd_ctl[qno];

	if ((dir == SPD_UL) && (qno < UL_QUE_NUM))
		return &s_spd_ctl[qno + DL_QUE_NUM];

	return NULL;
}

static void actions_per_1s(void)
{
	u64 dl_speed[DL_QUE_NUM];
	u64 ul_speed[UL_QUE_NUM];
	struct speed_t *ctl;
	int i, ul_speed_total = 0, dl_speed_total = 0;

	if (s_spd_cb_num_1s || s_spd_dl_cb_num_1s) {
		/* Copy speed info */
		for (i = 0; i < DL_QUE_NUM; i++) {
			ctl = get_speed_ctl(i, SPD_DL);
			if (ctl)
				dl_speed[i] = ctl->m_speed;
			else
				dl_speed[i] = 0;

			dl_speed_total += dl_speed[i];
		}
		for (i = 0; i < UL_QUE_NUM; i++) {
			ctl = get_speed_ctl(i, SPD_UL);
			if (ctl)
				ul_speed[i] = ctl->m_speed;
			else
				ul_speed[i] = 0;

			ul_speed_total += ul_speed[i];
		}
		/* Trigger call back function */
		for (i = 0; i < s_spd_cb_num_1s; i++)
			s_spd_func_1s[i](dl_speed, DL_QUE_NUM,
					ul_speed, UL_QUE_NUM);

		for (i = 0; i < s_spd_dl_cb_num_1s; i++)
			s_spd_dl_func_1s[i](ul_speed_total, dl_speed_total);

	}
}

static void actions_per_500ms(void)
{
	u64 dl_speed[DL_QUE_NUM];
	u64 ul_speed[UL_QUE_NUM];
	struct speed_t *ctl;
	int i;

	if (s_spd_cb_num_500ms) {
		/* Copy speed info */
		for (i = 0; i < DL_QUE_NUM; i++) {
			ctl = get_speed_ctl(i, SPD_DL);
			if (ctl)
				dl_speed[i] = ctl->m_speed_500ms;
			else
				dl_speed[i] = 0;
		}
		for (i = 0; i < UL_QUE_NUM; i++) {
			ctl = get_speed_ctl(i, SPD_UL);
			if (ctl)
				ul_speed[i] = ctl->m_speed_500ms;
			else
				ul_speed[i] = 0;
		}
		/* Trigger call back function */
		for (i = 0; i < s_spd_cb_num_500ms; i++)
			s_spd_func_500ms[i](dl_speed, DL_QUE_NUM,
						ul_speed, UL_QUE_NUM);
	}
}

static inline void add_pkt_bytes(struct speed_t *spd, int size)
{
	spd->m_curr_bytes += (u64)size;
	if (!s_spd_mon.m_speed_cal_en) {
		s_spd_mon.m_speed_cal_en = 1;
		//kthread_queue_work(&s_spd_mon.m_kworker, &s_spd_mon.m_kwork);
		wake_up(&s_spd_mon.m_speed_wq);
	}
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
		ret = snprintf(buf, size, "[%llu.%03lluGbps]", speed, rem);
	} else if (speed >= 1000000LL) {
		speed = speed / 1000LL;
		rem = do_div(speed, 1000);
		ret = snprintf(buf, size, "[%llu.%03lluMbps]", speed, rem);
	} else if (speed >= 1000LL) {
		rem = do_div(speed, 1000);
		ret = snprintf(buf, size, "[%llu.%03lluKbps]", speed, rem);
	} else
		ret = snprintf(buf, size, "[%llubps]", speed);
	if (ret < 0 || ret >= size) {
		pr_info("%s-%d:snprintf fail,ret=%d\n",
			__func__, __LINE__, ret);
		return -1;
	}

	return ret;
}

static u64 speed_calculate(u64 delta_bytes, s64 s, s64 e)
{
	u64 speed, delta;
#ifndef __LP64__
	u64 tmp;
#endif

	delta = e - s;
	if (delta < 100000000LL)
		return 0;

	// Max support 160Gbps
#ifdef __LP64__
	speed = (delta_bytes * 800000000LL) / (delta / 10LL);
#else
	tmp   = delta_bytes * 800000000LL;
	delta = do_div(delta, 10LL);
	speed = do_div(tmp, delta);
#endif
	return speed;
}

static u64 cal_speed_500ms_helper(struct speed_t *ctl, s64 s, s64 e)
{
	u64 delta_bytes;

	if (ctl->m_sample_e >= ctl->m_s_500ms)
		delta_bytes = ctl->m_sample_e - ctl->m_s_500ms;
	else
		delta_bytes = (~0ULL) -  ctl->m_s_500ms + ctl->m_sample_e + 1;

	if (!delta_bytes)
		return 0ULL;

	return speed_calculate(delta_bytes, s, e);
}

static u64 cal_speed_1s_helper(struct speed_t *ctl, s64 s, s64 e)
{
	u64 delta_bytes;

	if (ctl->m_sample_e >= ctl->m_s_1s)
		delta_bytes = ctl->m_sample_e - ctl->m_s_1s;
	else
		delta_bytes = (~0ULL) -  ctl->m_s_1s + ctl->m_sample_e + 1;

	if (!delta_bytes)
		return 0ULL;

	return speed_calculate(delta_bytes, s, e);
}

static inline void start_sample(void)
{
	int i;

	for (i = 0; i < (DL_QUE_NUM + UL_QUE_NUM); i++) {
		s_spd_ctl[i].m_s_1s = s_spd_ctl[i].m_curr_bytes;
		s_spd_ctl[i].m_s_500ms = s_spd_ctl[i].m_curr_bytes;
	}
}

static inline void update_start_sample(void)
{
	int i;

	for (i = 0; i < (DL_QUE_NUM + UL_QUE_NUM); i++)
		s_spd_ctl[i].m_s_1s = s_spd_ctl[i].m_sample_e;
}

static inline void update_start_sample_500ms(void)
{
	int i;

	for (i = 0; i < (DL_QUE_NUM + UL_QUE_NUM); i++)
		s_spd_ctl[i].m_s_500ms = s_spd_ctl[i].m_sample_e;
}

static inline void update_end_sample(void)
{
	int i;

	for (i = 0; i < (DL_QUE_NUM + UL_QUE_NUM); i++)
		s_spd_ctl[i].m_sample_e = s_spd_ctl[i].m_curr_bytes;
}

static inline int is_all_speed_zero(void)
{
	int i;
	int is_zero = 1;

	for (i = 0; i < (DL_QUE_NUM + UL_QUE_NUM); i++) {
		if (s_spd_ctl[i].m_speed) {
			is_zero = 0;
			break;
		}
	}

	return is_zero;
}

static void cal_speed_500ms(s64 s, s64 e)
{
	u64 speed;
	int i;

	for (i = 0; i < (DL_QUE_NUM + UL_QUE_NUM); i++) {
		speed = cal_speed_500ms_helper(&s_spd_ctl[i], s, e);
		s_spd_ctl[i].m_speed_500ms = speed;
	}
}

static void cal_speed_1s(s64 s, s64 e)
{
	u64 speed;
	int i;

	for (i = 0; i < (DL_QUE_NUM + UL_QUE_NUM); i++) {
		speed = cal_speed_1s_helper(&s_spd_ctl[i], s, e);
		s_spd_ctl[i].m_speed = speed;
	}
}

static void show_speed_inf(void)
{
	int i, ret, used, total;
	u64 speed;
	struct speed_t *ctl;

	if (!s_show)
		return;

	total = (int)sizeof(s_dl_speed_inf);
	used = 0;
	for (i = 0; i < DL_QUE_NUM; i++) {
		ctl = get_speed_ctl(i, SPD_DL);
		if (!ctl)
			continue;
		speed = ctl->m_speed;
		ret = get_speed_str(speed, &s_dl_speed_inf[used], total - used);
		if (ret > 0)
			used += ret;
	}

	total = (int)sizeof(s_ul_speed_inf);
	used = 0;
	for (i = 0; i < UL_QUE_NUM; i++) {
		ctl = get_speed_ctl(i, SPD_UL);
		if (!ctl)
			continue;
		speed = ctl->m_speed;
		ret = get_speed_str(speed, &s_ul_speed_inf[used], total - used);
		if (ret > 0)
			used += ret;
	}
	pr_info("[SPD]> UL:%s DL:%s\n", s_ul_speed_inf, s_dl_speed_inf);
}

static int speed_calculate_in_thread(void *arg)
{
	s64 start_tick, start_tick_500ms, end_tick;
	int i = 0;
	int exit_cnt, ret;

again:
	exit_cnt = 5;
	start_sample();
	start_tick = sched_clock();
	start_tick_500ms = start_tick;
	do {
		i = 0;
		do {
			msleep(500);
			end_tick = sched_clock();
			update_end_sample();
			cal_speed_500ms(start_tick_500ms, end_tick);
			actions_per_500ms();

			update_start_sample_500ms();
			start_tick_500ms = end_tick;
			i++;
		} while (i < 2);

		cal_speed_1s(start_tick, end_tick);
		update_start_sample();
		start_tick = end_tick;

		actions_per_1s();
		show_speed_inf();

		if (is_all_speed_zero())
			exit_cnt--;
	} while (exit_cnt);

	s_spd_mon.m_speed_cal_en = 0;

	while (1) {
		ret = wait_event_interruptible(s_spd_mon.m_speed_wq,
				s_spd_mon.m_speed_cal_en);

		if (kthread_should_stop()) {
			pr_info("[%s] error: kthread_should_stop.\n",
				__func__);
			return 0;
		}

		if (s_spd_mon.m_speed_cal_en)
			goto again;
	}

	return 0;
}

//=========================================================================
// Export to external
//=========================================================================

void mtk_ccci_add_dl_pkt_bytes(u32 qno, int size)
{
	struct speed_t *spd;

	if (size <= 0)
		return;

	spd = get_speed_ctl(qno, SPD_DL);
	if (!spd)
		return;

	add_pkt_bytes(spd, size);
}

void mtk_ccci_add_ul_pkt_bytes(u32 qno, int size)
{
	struct speed_t *spd;

	if (size <= 0)
		return;

	spd = get_speed_ctl(qno, SPD_UL);
	if (!spd)
		return;

	add_pkt_bytes(spd, size);
}

int mtk_ccci_net_spd_cfg(int toggle)
{
	if (toggle)
		s_show = !s_show;

	return s_show;
}

void mtk_ccci_register_speed_1s_callback(total_spd_fun func)
{
	if ((s_spd_dl_cb_num_1s < MAX_CALL_BACK_FUNC_NUM) && func) {
		s_spd_dl_func_1s[s_spd_dl_cb_num_1s] = func;
		s_spd_dl_cb_num_1s++;

	} else
		pr_info("[ccci][spd]1s dl callback tbl full\n");
}

void mtk_ccci_register_speed_callback(spd_fun func_1s, spd_fun func_500ms)
{
	if ((s_spd_cb_num_1s < MAX_CALL_BACK_FUNC_NUM) && func_1s) {
		s_spd_func_1s[s_spd_cb_num_1s] = func_1s;
		s_spd_cb_num_1s++;
	} else
		pr_info("[ccci][spd]1s callback tbl full\n");

	if ((s_spd_cb_num_500ms < MAX_CALL_BACK_FUNC_NUM) && func_500ms) {
		s_spd_func_500ms[s_spd_cb_num_500ms] = func_500ms;
		s_spd_cb_num_500ms++;
	} else
		pr_info("[ccci][spd]500ms callback ptr full\n");
}

int mtk_ccci_net_speed_init(void)
{
	init_waitqueue_head(&s_spd_mon.m_speed_wq);

	s_spd_mon.m_kworker_thread = kthread_run(
			speed_calculate_in_thread, NULL, "ccci_spd");
	if (IS_ERR(s_spd_mon.m_kworker_thread)) {
		pr_info("[%s] error: kthread_run() fail.\n",
				__func__);
		return -1;
	}

	return 0;
}
