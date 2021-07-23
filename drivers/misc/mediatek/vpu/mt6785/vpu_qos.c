/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include <linux/timer.h>
#include <linux/module.h>
#include <linux/pm_qos.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#ifdef ENABLE_EARA_QOS
#include <mtk_qos_bound.h>
#endif
#include "vpu_qos.h"
#include "vpu_cmn.h"
#include "vpu_dbg.h"

#define LINE_TAG LOG_DBG("[vpu][qos] %s %d\n", __func__, __LINE__)

/* polling interval 16 ms */
//#define VPU_SELF_WORKQUEUE
#define DEFAUTL_QOS_POLLING_TIME 16
#define MTK_QOS_BUF_SIZE QOS_BOUND_BUF_SIZE //define in mtk_qos_bound.h

#define get_qosbound_enum(x) ((x == 0) ? QOS_SMIBM_VPU0 : QOS_SMIBM_VPU1)

struct qos_counter {
	struct timer_list qos_timer;
	struct pm_qos_request qos_req;
	struct list_head list;
	struct mutex list_mtx;

	int32_t last_peak_val;
	unsigned int last_idx;
	int core;
	int wait_ms;
};

struct cmd_qos {
	int32_t total_bw;
	unsigned int last_idx;
	unsigned int count;

	struct list_head list;
	struct mutex mtx;
};

/* workqueue */
#ifdef VPU_SELF_WORKQUEUE
DEFINE_MUTEX(wq_mtx);
struct qos_workqueue {
	struct workqueue_struct *qos_queue;
	unsigned int ref_cnt;
};
static struct qos_workqueue qos_wq;
#endif

static struct qos_counter qos_counter[MTK_VPU_CORE];
static struct work_struct qos_work[MTK_VPU_CORE];


//-----
static inline void enque_qos_wq(struct work_struct *work)
{
#ifdef VPU_SELF_WORKQUEUE
	if (qos_wq.qos_queue == NULL || work == NULL) {
		LOG_ERR("[vpu][qos] enque qos wq fail\n");
		return;
	}
	queue_work(qos_wq.qos_queue, work);
#else
	schedule_work(work);
#endif
}

static inline struct qos_counter *get_qos_counter(unsigned int core)
{
	return &qos_counter[core];
}

//-----
static int add_qos_request(struct pm_qos_request *req)
{
	pm_qos_add_request(req, PM_QOS_APU_MEMORY_BANDWIDTH,
		PM_QOS_DEFAULT_VALUE);
	return 0;
}

static void update_qos_request(struct pm_qos_request *req, int32_t val)
{
	if (g_vpu_log_level > VpuLogThre_PERFORMANCE)
		LOG_INF("[vpu][qos] %s, peakBw=%d -\n", __func__, val);

	pm_qos_update_request(req, val);
}

static int destroy_qos_request(struct pm_qos_request *req)
{
	pm_qos_update_request(req, PM_QOS_APU_MEMORY_BANDWIDTH_DEFAULT_VALUE);
	pm_qos_remove_request(req);
	return 0;
}

//-----
/* called by timer up, update average bw according idx/last_idx */
static int update_cmd_qos(struct qos_counter *counter,
	struct qos_bound *qos_info)
{
	struct cmd_qos *cmd_qos = NULL;
	unsigned int idx = 0;
	int qos_smi_idx = 0;

	LOG_DBG("[vpu][qos] %s +\n", __func__);

	/* get qos smibm enum */
	qos_smi_idx = get_qosbound_enum(counter->core);

	mutex_lock(&counter->list_mtx);
	/* get first entry */
	if (!list_empty(&counter->list)) {
		cmd_qos = list_first_entry(&counter->list,
			struct cmd_qos, list);
		if (cmd_qos != NULL) {
			/* sum current bw value to cmd_qos */
			mutex_lock(&cmd_qos->mtx);
			idx = cmd_qos->last_idx;
			do {
				cmd_qos->total_bw +=
				qos_info->stats[idx].smibw_mon[qos_smi_idx];
				cmd_qos->count++;
				idx = (idx + 1) % MTK_QOS_BUF_SIZE;
			} while (idx != qos_info->idx);
			LOG_DBG("[vpu][qos] %s, idx(%d ~ %d)\n",
				__func__, cmd_qos->last_idx, idx);
			/* update last idx */
			cmd_qos->last_idx = idx;
			mutex_unlock(&cmd_qos->mtx);
		}
	} else {
		mutex_unlock(&counter->list_mtx);
		return 0;
	}
	mutex_unlock(&counter->list_mtx);

	LOG_DBG("[vpu][qos] %s -, total bw(%d)\n", __func__, cmd_qos->total_bw);

	return 0;
}

static int enque_cmd_qos(struct qos_counter *counter)
{
	struct qos_bound *qos_info = NULL;
	struct cmd_qos *cmd_qos = NULL;

	LOG_DBG("[vpu][qos] %s +\n", __func__);

	/* alloc cmd_qos */
	cmd_qos = kzalloc(sizeof(struct cmd_qos), GFP_KERNEL);
	if (cmd_qos == NULL) {
		LOG_ERR("[vpu][qos] %s alloc cmd_qos fail\n", __func__);
		return -1;
	};

	/* get qos information */
	qos_info = get_qos_bound();
	if (qos_info == NULL) {
		LOG_ERR("[vpu][qos] %s get info fail\n", __func__);
		kfree(cmd_qos);
		return -1;
	}

	/* init cmd_qos */
	mutex_init(&cmd_qos->mtx);

	/* critical session, modify cmd_qos's idx */
	mutex_lock(&cmd_qos->mtx);
	cmd_qos->last_idx = qos_info->idx;
	mutex_unlock(&cmd_qos->mtx);

	/* add to counter's list */
	mutex_lock(&counter->list_mtx);
	list_add_tail(&cmd_qos->list, &counter->list);
	mutex_unlock(&counter->list_mtx);

	LOG_DBG("[vpu][qos] %s -\n", __func__);

	return 0;
}

static int deque_cmd_qos(struct qos_counter *counter)
{
	struct cmd_qos *cmd_qos = NULL;
	struct qos_bound *qos_info = NULL;
	int qos_smi_idx = 0;
	int avg_bw = 0;
	unsigned int idx = 0;

	LOG_DBG("[vpu][qos] %s +\n", __func__);

	/* pop out cmd_qos from the head of list */
	mutex_lock(&counter->list_mtx);
	if (!list_empty(&counter->list)) {
		cmd_qos = list_first_entry(&counter->list,
			struct cmd_qos, list);
		if (cmd_qos == NULL) {
			LOG_ERR("[vpu][qos] %s get first entry fail\n",
				__func__);
			mutex_unlock(&counter->list_mtx);
			return 0;
		}
	} else {
		LOG_ERR("[vpu][qos] %s empty list\n", __func__);
		mutex_unlock(&counter->list_mtx);
		return 0;
	}
	list_del(&cmd_qos->list);
	mutex_unlock(&counter->list_mtx);

	LOG_DBG("[vpu][qos] %s cmd_qos = %p\n", __func__, cmd_qos);

	/* get qos information */
	qos_info = get_qos_bound();
	if (qos_info == NULL) {
		LOG_ERR("[vpu][qos] %s get info fail\n", __func__);
		return 0;
	}

	/* get qos smibm enum */
	qos_smi_idx = get_qosbound_enum(counter->core);

	/* sum the last bw */
	mutex_lock(&cmd_qos->mtx);
	idx = cmd_qos->last_idx;
	do {
		cmd_qos->total_bw +=
			qos_info->stats[idx].smibw_mon[qos_smi_idx];
		cmd_qos->count++;
		idx = (idx + 1) % MTK_QOS_BUF_SIZE;
	} while (idx != qos_info->idx);
	LOG_DBG("[vpu][qos] %s, idx(%d ~ %d)\n", __func__,
		cmd_qos->last_idx, idx);
	cmd_qos->last_idx = idx;

	mutex_unlock(&cmd_qos->mtx);

	/* average bw */
	if (cmd_qos->count != 0) {
		avg_bw = cmd_qos->total_bw / cmd_qos->count;
	} else {
		avg_bw = cmd_qos->total_bw;
	};

	if (g_vpu_log_level > VpuLogThre_PERFORMANCE) {
		LOG_INF("[vpu][qos] cmd:bw(%d/%d)\n",
			avg_bw, cmd_qos->total_bw);
	}

	/* free cmd_qos */
	kfree(cmd_qos);

	LOG_DBG("[vpu][qos] %s -\n", __func__);

	return avg_bw;
}

static void qos_work_func(struct work_struct *work)
{
	struct qos_bound *qos_info = NULL;
	struct qos_counter *counter = NULL;
	int qos_smi_idx = 0;
	int idx = 0;
	unsigned int peak_bw = 0;
	int i = 0;

	LOG_DBG("[vpu][qos] %s +\n", __func__);

	/* get qos smibm enum */
	for (i = 0; i < MTK_VPU_CORE; i++) {
		if (work == &qos_work[i]) {
			counter = get_qos_counter(i);
			break;
		}
	}
	if (counter == NULL) {
		LOG_ERR("[vpu][qos] %s get counter fail(%p)\n",
			__func__, work);
		return;
	}
	LOG_DBG("[vpu][qos] %s core(%d)\n", __func__, counter->core);

	/* get qos smibm enum */
	qos_smi_idx = get_qosbound_enum(counter->core);

	/* get qos bound */
	qos_info = get_qos_bound();
	if (qos_info == NULL) {
		LOG_ERR("[vpu][qos] %s get info fail\n", __func__);
		return;
	}

	/* find peak bandwidth consumption */
	idx = counter->last_idx;
	do {
		idx = (idx + 1) % MTK_QOS_BUF_SIZE;
		peak_bw = peak_bw >
			qos_info->stats[idx].smibw_mon[qos_smi_idx] ?
			peak_bw : qos_info->stats[idx].smibw_mon[qos_smi_idx];
	} while (idx != qos_info->idx);
	LOG_DBG("[vpu][qos] %s, idx(%d ~ %d)\n",
		__func__, counter->last_idx, idx);
	counter->last_idx = idx;

	/* update peak bw */
	if (counter->last_peak_val != peak_bw) {
		counter->last_peak_val = peak_bw;
		update_qos_request(&counter->qos_req, peak_bw);
	}

	update_cmd_qos(counter, qos_info);
	LOG_DBG("[vpu][qos] %s, peakbw=%d -\n", __func__, peak_bw);
}

static void qos_timer_func(unsigned long arg)
{
	struct qos_counter *counter = NULL;

	LOG_DBG("[vpu][qos] %s(%d) +\n", __func__, (int)arg);
	counter = get_qos_counter(arg);
	if (counter == NULL) {
		LOG_ERR("[vpu][qos] %s get counter fail\n", __func__);
		return;
	}

	/* queue work because mutex sleep must be happened */
	enque_qos_wq(&qos_work[arg]);
	mod_timer(&counter->qos_timer,
		jiffies + msecs_to_jiffies(DEFAUTL_QOS_POLLING_TIME));

	LOG_DBG("[vpu][qos] %s -\n", __func__);
}

//-----
int vpu_cmd_qos_start(int core)
{
	struct qos_counter *counter = NULL;

	LOG_DBG("[vpu][qos] %s(%d)+\n", __func__, core);

	/* get qos counter */
	counter = get_qos_counter(core);
	if (counter == NULL) {
		LOG_ERR("[vpu][qos] %s get counter fail\n", __func__);
		return -1;
	}

	/* enque cmd to counter's list */
	if (enque_cmd_qos(counter)) {
		LOG_ERR("[vpu][qos] %s enque cmd qos fail\n", __func__);
		return -1;
	}

	LOG_DBG("[vpu][qos] %s -\n", __func__);

	return 0;
}

int vpu_cmd_qos_end(int core)
{
	struct qos_counter *counter = NULL;
	int bw = 0;

	LOG_DBG("[vpu][qos] %s(%d) +\n", __func__, core);

	/* get counter */
	counter = get_qos_counter(core);
	if (counter == NULL) {
		LOG_ERR("[vpu][qos] %s get counter fail\n", __func__);
		return -1;
	}

	/* deque cmd to counter's list */
	bw = deque_cmd_qos(counter);

	LOG_DBG("[vpu][qos] %s -\n", __func__);
	/* return 1 if bw = 0 (eara requirement) */
	return bw == 0 ? 1 : bw;
}

int vpu_qos_counter_start(unsigned int core)
{
	struct qos_counter *counter = NULL;

	LOG_DBG("[vpu][qos]%s(%d) +\n", __func__, core);
#ifdef VPU_SELF_WORKQUEUE
	/* init qos_queue */
	if (qos_wq.qos_queue == NULL) {
		qos_wq.qos_queue = create_workqueue("vpu_qos");
		if (qos_wq.qos_queue == NULL) {
			LOG_ERR("[vpu][qos] create workqueue fail\n");
			return -1;
		}
	}
#endif

	/* get counter */
	counter = get_qos_counter(core);
	if (counter == NULL) {
		LOG_ERR("[vpu][qos] get counter fail\n");
		return -1;
	}

	/* init counter's list */
	INIT_LIST_HEAD(&counter->list);
	mutex_init(&counter->list_mtx);

	/* setup timer */
	init_timer(&counter->qos_timer);
	counter->qos_timer.function = &qos_timer_func;
	counter->qos_timer.data = core;
	counter->qos_timer.expires =
		jiffies + msecs_to_jiffies(DEFAUTL_QOS_POLLING_TIME);
	/* record wait time in counter */
	counter->wait_ms = DEFAUTL_QOS_POLLING_TIME;
	counter->core = core;
	counter->last_idx = 0;
	counter->last_peak_val = 0;
	add_timer(&counter->qos_timer);

	LOG_DBG("[vpu][qos] %s -\n", __func__);

	return 0;
}

int vpu_qos_counter_end(unsigned int core)
{
	struct qos_counter *counter = NULL;

	LOG_DBG("[vpu][qos]%s +\n", __func__);

	/* get qos smibm enum */
	counter = get_qos_counter(core);

	/* delete timer */
	del_timer_sync(&counter->qos_timer);

	LOG_DBG("[vpu][qos] %s -\n", __func__);

	return 0;
}

int vpu_qos_counter_init(void)
{
	int i = 0;
	struct qos_counter *counter = NULL;

	LOG_DBG("[vpu][qos] %s +\n", __func__);

#ifdef VPU_SELF_WORKQUEUE
	mutex_lock(&wq_mtx);
	/* init qos_queue */
	if (qos_wq.qos_queue == NULL) {
		qos_wq.qos_queue = create_workqueue("vpu_qos");
		if (qos_wq.qos_queue == NULL) {
			LOG_ERR("[vpu][qos] create workqueue fail\n");
			mutex_unlock(&wq_mtx);
			return -1;
		}
		qos_wq.ref_cnt++;
	}
	mutex_unlock(&wq_mtx);
#endif

	/* init work and pm_qos_request */
	for (i = 0; i < MTK_VPU_CORE; i++) {
		INIT_WORK(&qos_work[i], &qos_work_func);
		/* get counter */
		counter = get_qos_counter(i);
		if (counter == NULL) {
			LOG_ERR("[vpu][qos] %s get counter(%d) fail\n",
				__func__, i);
			continue;
		}
		add_qos_request(&counter->qos_req);
	}

	return 0;
}

void vpu_qos_counter_destroy(void)
{
	int i = 0;
	struct qos_counter *counter = NULL;

	LOG_DBG("[vpu][qos] %s +\n", __func__);
#ifdef VPU_SELF_WORKQUEUE
	mutex_lock(&wq_mtx);
	qos_wq.ref_cnt--;
	if (qos_wq.ref_cnt == 0) {
		flush_workqueue(qos_wq.qos_queue);
		destroy_workqueue(qos_wq.qos_queue);
	}
	mutex_unlock(&wq_mtx);
#endif

	/* remove pm_qos_request */
	for (i = 0; i < MTK_VPU_CORE; i++) {
		counter = get_qos_counter(i);
		if (counter == NULL) {
			LOG_ERR("[vpu][qos] %s get counter(%d) fail\n",
				__func__, i);
			continue;
		}
		destroy_qos_request(&counter->qos_req);
	}
}

