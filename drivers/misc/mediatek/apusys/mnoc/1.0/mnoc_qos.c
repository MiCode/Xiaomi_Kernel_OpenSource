/*
 * Copyright (C) 2019 MediaTek Inc.
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

/*
 *=============================================================
 * Include files
 *=============================================================
 */

/* system includes */
#include <linux/pm_qos.h>
#include <linux/mutex.h>
#include <linux/timekeeping.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

#include "mnoc_drv.h"
#include "mnoc_hw.h"

#if MNOC_QOS_ENABLE
#include <mtk_qos_bound.h>
#include <mtk_qos_sram.h>


#define DEFAUTL_QOS_POLLING_TIME (16)
/* define in mtk_qos_bound.h */
#define MTK_QOS_BUF_SIZE QOS_BOUND_BUF_SIZE

/* assume QOS_SMIBM_VPU0 is the first entry in qos_smibm_type for APUSYS */
#if 0
#define APUSYS_QOSBOUND_START (QOS_SMIBM_VPU0)
#define get_qosbound_enum(x) (APUSYS_QOSBOUND_START + x)
#else
#define APUSYS_QOSBOUND_START (QOS_SMIBM_VPU0)
#define get_qosbound_enum(x) (APUSYS_QOSBOUND_START)
#endif

enum apu_qos_cmd_status {
	CMD_RUNNING,
	CMD_BLOCKED,

	NR_APU_QOS_CMD_STATUS
};

struct qos_counter {
	struct timer_list qos_timer;
	struct list_head list;
	struct mutex list_mtx;

	int wait_ms;
};

struct engine_pm_qos_counter {
	struct pm_qos_request qos_req;

	int32_t last_peak_val;
	unsigned int last_idx;
	unsigned int core;
};

struct cmd_qos {
	uint64_t cmd_id;
	uint64_t sub_cmd_id;
	unsigned int core;
	unsigned int status; /* running/blocked */

	int32_t total_bw;
	unsigned int last_idx;
	unsigned int count;

	struct list_head list;
	struct mutex mtx;
};

static struct qos_counter qos_counter;
static struct work_struct qos_work;
static struct engine_pm_qos_counter engine_pm_qos_counter[NR_APU_QOS_ENGINE];

/* indicate engine running or not based on cmd cntr for pm qos */
/* increase 1 when cmd enque, decrease 1 when cmd dequeue */
static int engine_cmd_cntr[NR_APU_QOS_ENGINE];
bool qos_timer_exist;
struct mutex qos_timer_exist_mtx;


/* register to apusys power on callback */
void notify_sspm_apusys_on(void)
{
	LOG_DEBUG("+\n");
	qos_sram_write(APU_CLK, 1);
	LOG_DEBUG("-\n");
}

/* register to apusys power off callback(before power off) */
void notify_sspm_apusys_off(void)
{
	int bw_nord = 0;

	LOG_DEBUG("+\n");

	qos_sram_write(APU_CLK, 0);
	while (bw_nord == 0) {
		bw_nord = qos_sram_read(APU_BW_NORD);
		udelay(500);
		LOG_DEBUG("wait SSPM bw_nord");
	}

	LOG_DEBUG("-\n");
}

static inline void enque_qos_wq(struct work_struct *work)
{
	schedule_work(work);
}

static int add_qos_request(struct pm_qos_request *req)
{
	pm_qos_add_request(req, PM_QOS_APU_MEMORY_BANDWIDTH,
		PM_QOS_DEFAULT_VALUE);
	return 0;
}

static void update_qos_request(struct pm_qos_request *req, int32_t val)
{
	LOG_DEBUG("bw = %d\n", val);
	pm_qos_update_request(req, val);
}

static int destroy_qos_request(struct pm_qos_request *req)
{
	pm_qos_update_request(req, PM_QOS_APU_MEMORY_BANDWIDTH_DEFAULT_VALUE);
	pm_qos_remove_request(req);
	return 0;
}

/* called by timer up, update average bw according to idx/last_idx */
static int update_cmd_qos(struct qos_bound *qos_info)
{
	struct cmd_qos *cmd_qos = NULL;
	struct qos_counter *counter = &qos_counter;
	int idx = 0;
	int qos_smi_idx = 0;

	LOG_DEBUG("+\n");

	mutex_lock(&counter->list_mtx);

	/* get first entry */
	list_for_each_entry(cmd_qos, &counter->list, list) {
		if (cmd_qos->status == CMD_RUNNING) {
			/* get qos smibm enum */
			qos_smi_idx = get_qosbound_enum(cmd_qos->core);
			/* sum current bw value to cmd_qos */
			mutex_lock(&cmd_qos->mtx);
			idx = cmd_qos->last_idx;
			do {
				cmd_qos->total_bw +=
				qos_info->stats[idx].smibw_mon[qos_smi_idx];
				cmd_qos->count++;
				idx = (idx + 1) % MTK_QOS_BUF_SIZE;
			} while (idx != qos_info->idx);
			LOG_DEBUG("(%d/%d)idx(%d ~ %d)\n", cmd_qos->cmd_id,
				cmd_qos->sub_cmd_id, cmd_qos->last_idx, idx);
			/* update last idx */
			cmd_qos->last_idx = idx;
			mutex_unlock(&cmd_qos->mtx);
		}
	}

	mutex_unlock(&counter->list_mtx);

	LOG_DEBUG("total bw(%d)\n", cmd_qos->total_bw);

	LOG_DEBUG("-\n");

	return 0;
}

static int enque_cmd_qos(uint64_t cmd_id, uint64_t sub_cmd_id,
	unsigned int core)
{
	struct qos_counter *counter = &qos_counter;
	struct qos_bound *qos_info = NULL;
	struct cmd_qos *cmd_qos = NULL;

	LOG_DEBUG("+\n");

	/* alloc cmd_qos */
	cmd_qos = kzalloc(sizeof(struct cmd_qos), GFP_KERNEL);
	if (cmd_qos == NULL) {
		LOG_ERR("alloc cmd_qos(%d/%d) fail\n", cmd_id, sub_cmd_id);
		return -1;
	};

	/* get qos information */
	qos_info = get_qos_bound();
	if (qos_info == NULL) {
		LOG_ERR("get info fail\n");
		kfree(cmd_qos);
		return -1;
	}

	/* init cmd_qos */
	mutex_init(&cmd_qos->mtx);

	/* critical session, modify cmd_qos's idx */
	mutex_lock(&cmd_qos->mtx);
	cmd_qos->cmd_id = cmd_id;
	cmd_qos->sub_cmd_id = sub_cmd_id;
	cmd_qos->core = core;
	cmd_qos->status = CMD_RUNNING;
	cmd_qos->last_idx = qos_info->idx;
	mutex_unlock(&cmd_qos->mtx);

	/* add to counter's list */
	/* mutex_lock(&counter->list_mtx); */
	list_add_tail(&cmd_qos->list, &counter->list);
	/* mutex_unlock(&counter->list_mtx); */

	LOG_DEBUG("-\n");

	return 0;
}

static int deque_cmd_qos(struct cmd_qos *cmd_qos)
{
	struct qos_bound *qos_info = NULL;
	/* struct qos_counter *counter = &qos_counter; */
	int qos_smi_idx = 0;
	int avg_bw = 0;
	int idx = 0;

	LOG_DEBUG("+\n");

	/* delete cmd_qos from counter's list */
	/* mutex_lock(&counter->list_mtx); */
	list_del(&cmd_qos->list);
	/* mutex_unlock(&counter->list_mtx); */

	LOG_DEBUG("cmd_qos = %p\n", cmd_qos);

	/* get qos information */
	qos_info = get_qos_bound();
	if (qos_info == NULL) {
		LOG_ERR("get info fail\n");
		return 0;
	}

	/* get qos smibm enum */
	qos_smi_idx = get_qosbound_enum(cmd_qos->core);

	/* sum the last bw */
	mutex_lock(&cmd_qos->mtx);
	idx = cmd_qos->last_idx;
	do {
		cmd_qos->total_bw +=
			qos_info->stats[idx].smibw_mon[qos_smi_idx];
		cmd_qos->count++;
		idx = (idx + 1) % MTK_QOS_BUF_SIZE;
	} while (idx != qos_info->idx);
	LOG_DEBUG("idx(%d ~ %d)\n", cmd_qos->last_idx, idx);
	cmd_qos->last_idx = idx;

	mutex_unlock(&cmd_qos->mtx);

	/* average bw */
	if (cmd_qos->count != 0) {
		avg_bw = cmd_qos->total_bw / cmd_qos->count;
	} else {
		avg_bw = cmd_qos->total_bw;
	};

	LOG_DEBUG("cmd(%d/%d):bw(%d/%d)\n", cmd_qos->cmd_id,
		cmd_qos->sub_cmd_id, avg_bw, cmd_qos->total_bw);

	/* free cmd_qos */
	kfree(cmd_qos);

	LOG_DEBUG("-\n");

	return avg_bw;
}

static void qos_work_func(struct work_struct *work)
{
	struct qos_bound *qos_info = NULL;
	struct engine_pm_qos_counter *counter = NULL;
	int qos_smi_idx = 0;
	int idx = 0;
	unsigned int peak_bw = 0;
	int i = 0;
#if MNOC_TIME_PROFILE
	struct timeval begin, end;
	unsigned long val;
#endif

	LOG_DEBUG("+\n");

#if MNOC_TIME_PROFILE
	do_gettimeofday(&begin);
#endif

	mutex_lock(&qos_timer_exist_mtx);

	if (!qos_timer_exist) {
		mutex_unlock(&qos_timer_exist_mtx);
		return;
	}


	/* get qos bound */
	qos_info = get_qos_bound();
	if (qos_info == NULL) {
		LOG_ERR("get qos_info fail\n");
		return;
	}

	for (i = 0; i < NR_APU_QOS_ENGINE; i++)	{
		counter = &engine_pm_qos_counter[i];
		qos_smi_idx = get_qosbound_enum(i);
		/* find peak bandwidth consumption */
		idx = counter->last_idx;
		do {
			idx = (idx + 1) % MTK_QOS_BUF_SIZE;
			peak_bw = peak_bw >
				qos_info->stats[idx].smibw_mon[qos_smi_idx] ?
				peak_bw :
				qos_info->stats[idx].smibw_mon[qos_smi_idx];
		} while (idx != qos_info->idx);
		LOG_DEBUG("idx[%d](%d ~ %d)\n", i, counter->last_idx, idx);
		counter->last_idx = idx;

		/* update peak bw */
		if (counter->last_peak_val != peak_bw) {
			counter->last_peak_val = peak_bw;
			update_qos_request(&counter->qos_req, peak_bw);
		}

		LOG_DEBUG("peakbw[%d]=%d\n", i, peak_bw);
	}

	update_cmd_qos(qos_info);

	mutex_unlock(&qos_timer_exist_mtx);

#if MNOC_TIME_PROFILE
	do_gettimeofday(&end);
	val = (end.tv_sec - begin.tv_sec) * 1000000;
	val += (end.tv_usec - begin.tv_usec);
	LOG_ERR("val = %d us\n", val);
#endif

	LOG_DEBUG("-\n");
}

static void qos_timer_func(unsigned long arg)
{
	struct qos_counter *counter = &qos_counter;

	LOG_DEBUG("+\n");

	/* queue work because mutex sleep must be happened */
	enque_qos_wq(&qos_work);
	mod_timer(&counter->qos_timer,
		jiffies + msecs_to_jiffies(DEFAUTL_QOS_POLLING_TIME));

	LOG_DEBUG("-\n");
}

/*
 * create timer to count current bandwidth of apu engines each 16ms
 * timer will schedule work to wq when time's up
 * @call at insertion to empty cmd_qos list
 */
static void apu_qos_timer_start(void)
{
	struct qos_counter *counter = &qos_counter;

	LOG_DEBUG("+\n");

	/* setup timer */
	init_timer(&counter->qos_timer);
	counter->qos_timer.function = &qos_timer_func;
	counter->qos_timer.data = 0;
	counter->qos_timer.expires =
		jiffies + msecs_to_jiffies(DEFAUTL_QOS_POLLING_TIME);
	/* record wait time in counter */
	counter->wait_ms = DEFAUTL_QOS_POLLING_TIME;
	add_timer(&counter->qos_timer);

	LOG_DEBUG("-\n");
}

/*
 * delete timer
 * update pm qos request to default value
 * @call at deletion from cmd_qos list and result to list empty
 */
static void apu_qos_timer_end(void)
{
	int i = 0;
	struct qos_counter *counter = &qos_counter;

	LOG_DEBUG("+\n");

	/* delete timer */
	del_timer_sync(&counter->qos_timer);

	/* fixme: if update request to default value necessary? */
	for (i = 0; i < NR_APU_QOS_ENGINE; i++) {
		pm_qos_update_request(&(engine_pm_qos_counter[i].qos_req),
			PM_QOS_APU_MEMORY_BANDWIDTH_DEFAULT_VALUE);
	}

	LOG_DEBUG("-\n");
}

/*
 * enque cmd to qos_counter's linked list
 * if list is empty before enqueue, start qos timer
 * if cmd already exist, denotes the cmd need to resume
 * -> status from CMD_BLOCKED to CMD_RUNNING
 */
int apu_cmd_qos_start(uint64_t cmd_id, uint64_t sub_cmd_id, unsigned int core)
{
	struct qos_counter *counter = &qos_counter;
	struct cmd_qos *cmd_qos = NULL, *pos;
#if MNOC_TIME_PROFILE
	struct timeval begin, end;
	unsigned long val;
#endif

	LOG_DEBUG("+\n");

#if MNOC_TIME_PROFILE
	do_gettimeofday(&begin);
#endif

	if (core >= NR_APU_QOS_ENGINE) {
		LOG_ERR("core(%d) exceed max apu core num(%d)\n",
			core, NR_APU_QOS_ENGINE - 1);
		return -1;
	}

	mutex_lock(&counter->list_mtx);

	/* start timer if cmd list empty */
	if (list_empty(&counter->list))
		apu_qos_timer_start();

	list_for_each_entry(pos, &counter->list, list) {
		if (pos->cmd_id == cmd_id && pos->sub_cmd_id == sub_cmd_id) {
			cmd_qos = pos;
			break;
		}
	}
	if (cmd_qos != NULL) {
		if (cmd_qos->status != CMD_BLOCKED) {
			LOG_ERR("cmd(%d/%d) already exist\n",
				cmd_id, sub_cmd_id);
			mutex_unlock(&counter->list_mtx);
			return -1;
		}
		LOG_DEBUG("set cmd(%d/%d) to CMD_RUNNING\n",
			cmd_id, sub_cmd_id);

		mutex_lock(&cmd_qos->mtx);
		cmd_qos->status = CMD_RUNNING;
		/* Q: possible to change exection engine? */
		cmd_qos->core = core;
		mutex_unlock(&cmd_qos->mtx);

		mutex_unlock(&counter->list_mtx);
		return 0;
	}


	/* enque cmd to counter's list */
	if (enque_cmd_qos(cmd_id, sub_cmd_id, core)) {
		LOG_ERR("enque cmd qos fail\n");
		mutex_unlock(&counter->list_mtx);
		return -1;
	}

	mutex_unlock(&counter->list_mtx);

#if MNOC_TIME_PROFILE
	do_gettimeofday(&end);
	val = (end.tv_sec - begin.tv_sec) * 1000000;
	val += (end.tv_usec - begin.tv_usec);
	LOG_INFO("val = %d us\n", val);
#endif

	LOG_DEBUG("-\n");

	return 0;
}

/*
 * suspend cmd due to preemption
 * set cmd status from CMD_RUNNING to CMD_BLOCKED
 */
int apu_cmd_qos_suspend(uint64_t cmd_id, uint64_t sub_cmd_id)
{
	struct qos_counter *counter = &qos_counter;
	struct cmd_qos *cmd_qos = NULL, *pos;
#if MNOC_TIME_PROFILE
	struct timeval begin, end;
	unsigned long val;
#endif

	LOG_DEBUG("+\n");

#if MNOC_TIME_PROFILE
	do_gettimeofday(&begin);
#endif

	mutex_lock(&counter->list_mtx);

	list_for_each_entry(pos, &counter->list, list) {
		if (pos->cmd_id == cmd_id && pos->sub_cmd_id == sub_cmd_id) {
			cmd_qos = pos;
			break;
		}
	}
	if (cmd_qos == NULL) {
		LOG_ERR("Can not find cmd(%d/%d)\n", cmd_id, sub_cmd_id);
		mutex_unlock(&counter->list_mtx);
		return -1;
	} else if (cmd_qos->status == CMD_BLOCKED) {
		LOG_ERR("cmd(%d/%d) already in suspend\n", cmd_id, sub_cmd_id);
		mutex_unlock(&counter->list_mtx);
		return -1;
	}

	mutex_lock(&cmd_qos->mtx);
	cmd_qos->status = CMD_BLOCKED;
	mutex_unlock(&cmd_qos->mtx);

	mutex_unlock(&counter->list_mtx);

	LOG_DEBUG("-\n");

#if MNOC_TIME_PROFILE
	do_gettimeofday(&end);
	val = (end.tv_sec - begin.tv_sec) * 1000000;
	val += (end.tv_usec - begin.tv_usec);
	LOG_INFO("val = %d us\n", val);
#endif

	return 0;
}

/*
 * deque cmd from qos_counter's linked list
 * if list becomes empty after dequeue, delete qos timer
 * fixme: if mutex protect range too large?
 */
int apu_cmd_qos_end(uint64_t cmd_id, uint64_t sub_cmd_id)
{
	struct qos_counter *counter = &qos_counter;
	struct cmd_qos *cmd_qos = NULL, *pos;
	int bw = 0;
#if MNOC_TIME_PROFILE
	struct timeval begin, end;
	unsigned long val;
#endif

	LOG_DEBUG("+\n");

#if MNOC_TIME_PROFILE
	do_gettimeofday(&begin);
#endif

	mutex_lock(&counter->list_mtx);

	list_for_each_entry(pos, &counter->list, list) {
		if (pos->cmd_id == cmd_id && pos->sub_cmd_id == sub_cmd_id) {
			cmd_qos = pos;
			break;
		}
	}
	if (cmd_qos == NULL) {
		LOG_ERR("Can not find cmd(%d/%d)\n", cmd_id, sub_cmd_id);
		mutex_unlock(&counter->list_mtx);
		return 1;
	}

	/* deque cmd to counter's list */
	bw = deque_cmd_qos(cmd_qos);

	/* delete timer if cmd list empty */
	if (list_empty(&counter->list))
		apu_qos_timer_end();

	mutex_unlock(&counter->list_mtx);

	LOG_DEBUG("-\n");

#if MNOC_TIME_PROFILE
	do_gettimeofday(&end);
	val = (end.tv_sec - begin.tv_sec) * 1000000;
	val += (end.tv_usec - begin.tv_usec);
	LOG_INFO("val = %d us\n", val);
#endif

	/* return 1 if bw = 0 (eara requirement) */
	return bw == 0 ? 1 : bw;
}

/*
 * create qos workqueue for count bandwidth
 * @call at module init
 */
void apu_qos_counter_init(void)
{
	int i = 0;
	struct engine_pm_qos_counter *counter = NULL;

	LOG_DEBUG("+\n");

	mutex_init(&qos_timer_exist_mtx);

	mutex_lock(&qos_timer_exist_mtx);
	qos_timer_exist = true;
	mutex_unlock(&qos_timer_exist_mtx);

	/* init counter's list */
	INIT_LIST_HEAD(&(qos_counter.list));
	mutex_init(&(qos_counter.list_mtx));

	/* init work and pm_qos_request */
	INIT_WORK(&qos_work, &qos_work_func);
	for (i = 0; i < NR_APU_QOS_ENGINE; i++) {
		/* init engine_cmd_cntr for each engine */
		engine_cmd_cntr[i] = 0;
		counter = &engine_pm_qos_counter[i];
		if (counter == NULL) {
			LOG_ERR("get counter(%d) fail\n", i);
			continue;
		}
		counter->last_peak_val = 0;
		counter->last_idx = 0;
		counter->core = i;
		add_qos_request(&counter->qos_req);
	}

	LOG_DEBUG("-\n");
}

/*
 * delete qos request
 * @call at module exit
 */
void apu_qos_counter_destroy(void)
{
	int i = 0;
	struct engine_pm_qos_counter *counter = NULL;
	struct cmd_qos *cmd_qos, *pos;

	LOG_DEBUG("+\n");

	mutex_lock(&(qos_counter.list_mtx));

	list_for_each_entry_safe(cmd_qos, pos, &(qos_counter.list), list) {
		deque_cmd_qos(cmd_qos);
	}

	/* delete timer if cmd list empty */
	if (list_empty(&(qos_counter.list)))
		apu_qos_timer_end();

	mutex_unlock(&(qos_counter.list_mtx));

	mutex_lock(&qos_timer_exist_mtx);
	qos_timer_exist = false;
	mutex_unlock(&qos_timer_exist_mtx);

	/* remove pm_qos_request */
	for (i = 0; i < NR_APU_QOS_ENGINE; i++) {
		counter = &engine_pm_qos_counter[i];
		if (counter == NULL) {
			LOG_ERR("get counter(%d) fail\n", i);
			continue;
		}
		destroy_qos_request(&counter->qos_req);
	}

	LOG_DEBUG("-\n");
}

/* ==================== for debug ==================== */

void print_cmd_qos_list(struct seq_file *m)
{
	struct qos_counter *counter = &qos_counter;
	struct cmd_qos *cmd_qos;

	list_for_each_entry(cmd_qos, &counter->list, list) {
		seq_printf(m, "cmd(%d/%d):\n",
			cmd_qos->cmd_id, cmd_qos->sub_cmd_id);
		seq_printf(m, "core = %d, status = %d\n",
			cmd_qos->core, cmd_qos->status);
		seq_printf(m, "total_bw = %d, last_idx = %d, count = %d\n",
			cmd_qos->total_bw, cmd_qos->last_idx, cmd_qos->count);
	}
}

#else

void notify_sspm_apusys_on(void)
{
}

void notify_sspm_apusys_off(void)
{
}

int apu_cmd_qos_start(uint64_t cmd_id, uint64_t sub_cmd_id, unsigned int core)
{
	return 0;
}

int apu_cmd_qos_suspend(uint64_t cmd_id, uint64_t sub_cmd_id)
{
	return 0;
}

int apu_cmd_qos_end(uint64_t cmd_id, uint64_t sub_cmd_id)
{
	return 0;
}

void apu_qos_counter_init(void)
{
}

void apu_qos_counter_destroy(void)
{
}

void print_cmd_qos_list(struct seq_file *m)
{
}

#endif /* MNOC_QOS_ENABLE */
