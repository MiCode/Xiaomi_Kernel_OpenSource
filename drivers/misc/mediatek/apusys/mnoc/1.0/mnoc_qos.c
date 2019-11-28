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
#include "mnoc_option.h"

#if MNOC_TIME_PROFILE
unsigned long sum_start, sum_suspend, sum_end, sum_work_func;
unsigned int cnt_start, cnt_suspend, cnt_end, cnt_work_func;
#endif

#if MNOC_QOS_ENABLE
#include <mtk_qos_bound.h>
#include <mtk_qos_sram.h>


#define DEFAUTL_QOS_POLLING_TIME (16)
/* define in mtk_qos_bound.h */
#define MTK_QOS_BUF_SIZE QOS_BOUND_BUF_SIZE

/* assume QOS_SMIBM_VPU0 is the first entry in qos_smibm_type for APUSYS */
#define APUSYS_QOSBOUND_START (QOS_SMIBM_VPU0)
#define get_qosbound_enum(x) (APUSYS_QOSBOUND_START + x)

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

	int total_bw;
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

/* to prevent pm qos request value stay at last cmd's peak bw */
void apu_pm_qos_off(void)
{
	int i = 0;

	for (i = 0; i < NR_APU_QOS_ENGINE; i++) {
		update_qos_request(&(engine_pm_qos_counter[i].qos_req),
			PM_QOS_APU_MEMORY_BANDWIDTH_DEFAULT_VALUE);
	}
}

static void update_cmd_qos(struct qos_bound *qos_info, struct cmd_qos *cmd_qos)
{
	int idx = 0, qos_smi_idx = 0;

	/* sample device has no BW */
	if (cmd_qos->core < NR_APU_QOS_ENGINE)
		qos_smi_idx = get_qosbound_enum(cmd_qos->core);

	/* sum current bw value to cmd_qos */
	mutex_lock(&cmd_qos->mtx);
	idx = cmd_qos->last_idx;
	while (idx != ((qos_info->idx + 1) % MTK_QOS_BUF_SIZE)) {
		if (cmd_qos->core < NR_APU_QOS_ENGINE)
			cmd_qos->total_bw +=
			qos_info->stats[idx].smibw_mon[qos_smi_idx];
		cmd_qos->count++;
		idx = (idx + 1) % MTK_QOS_BUF_SIZE;
	}
	LOG_DETAIL("(0x%llx/0x%llx)idx(%d ~ %d)\n", cmd_qos->cmd_id,
		cmd_qos->sub_cmd_id, cmd_qos->last_idx, idx);
	/* update last idx */
	cmd_qos->last_idx = idx;
	mutex_unlock(&cmd_qos->mtx);
}

/* called by timer up, update average bw according to idx/last_idx */
static int update_cmd_qos_list_locked(struct qos_bound *qos_info)
{
	struct cmd_qos *cmd_qos = NULL;
	struct qos_counter *counter = &qos_counter;

	LOG_DETAIL("+\n");

	/* mutex_lock(&counter->list_mtx); */

	list_for_each_entry(cmd_qos, &counter->list, list) {
		if (cmd_qos->status == CMD_RUNNING)
			update_cmd_qos(qos_info, cmd_qos);
	}

	/* mutex_unlock(&counter->list_mtx); */

	LOG_DETAIL("-\n");

	return 0;
}

static int enque_cmd_qos(uint64_t cmd_id,
	uint64_t sub_cmd_id, int core)
{
	struct qos_counter *counter = &qos_counter;
	struct qos_bound *qos_info = NULL;
	struct cmd_qos *cmd_qos = NULL;

	LOG_DEBUG("+\n");

	/* alloc cmd_qos */
	cmd_qos = kzalloc(sizeof(struct cmd_qos), GFP_KERNEL);
	if (cmd_qos == NULL) {
		LOG_ERR("alloc cmd_qos(0x%llx/0x%llx) fail\n",
			cmd_id, sub_cmd_id);
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
	/* struct qos_counter *counter = &qos_counter; */
	int avg_bw = 0;

	LOG_DEBUG("+\n");

	/* delete cmd_qos from counter's list */
	/* mutex_lock(&counter->list_mtx); */
	list_del(&cmd_qos->list);
	/* mutex_unlock(&counter->list_mtx); */

	LOG_DEBUG("cmd_qos = %p\n", cmd_qos);

	/* average bw */
	if (cmd_qos->count != 0) {
		avg_bw = cmd_qos->total_bw / cmd_qos->count;
	} else {
		avg_bw = cmd_qos->total_bw;
	};

	LOG_DEBUG("cmd(0x%llx/0x%llx):bw(%d/%d)\n", cmd_qos->cmd_id,
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
	int i = 0, idx = 0;
	unsigned int peak_bw = 0;
#if MNOC_TIME_PROFILE
	struct timeval begin, end;
	unsigned long val;
#endif

	LOG_DETAIL("+\n");

#if MNOC_TIME_PROFILE
	do_gettimeofday(&begin);
#endif

	/* get qos bound */
	qos_info = get_qos_bound();
	if (qos_info == NULL) {
		LOG_ERR("get qos_info fail\n");
		return;
	}

	for (i = 0; i < NR_APU_QOS_ENGINE; i++)	{
		peak_bw = 0;
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
		LOG_DETAIL("idx[%d](%d ~ %d)\n", i, counter->last_idx, idx);
		counter->last_idx = idx;

		/* update peak bw */
		if (counter->last_peak_val != peak_bw) {
			counter->last_peak_val = peak_bw;
			update_qos_request(&counter->qos_req, peak_bw);
		}

		LOG_DETAIL("peakbw[%d]=%d\n", i, peak_bw);
	}

	mutex_lock(&(qos_counter.list_mtx));
	update_cmd_qos_list_locked(qos_info);
	mutex_unlock(&(qos_counter.list_mtx));

#if MNOC_TIME_PROFILE
	do_gettimeofday(&end);
	val = (end.tv_sec - begin.tv_sec) * 1000000;
	val += (end.tv_usec - begin.tv_usec);
	/* LOG_DEBUG("val = %d us\n", val); */
	sum_work_func += val;
	cnt_work_func += 1;
#endif

	LOG_DETAIL("-\n");
}

static void qos_timer_func(unsigned long arg)
{
	struct qos_counter *counter = &qos_counter;

	LOG_DETAIL("+\n");

	/* queue work because mutex sleep must be happened */
	enque_qos_wq(&qos_work);
	mod_timer(&counter->qos_timer,
		jiffies + msecs_to_jiffies(DEFAUTL_QOS_POLLING_TIME));

	LOG_DETAIL("-\n");
}

/*
 * create timer to count current bandwidth of apu engines each 16ms
 * timer will schedule work to wq when time's up
 * @call at insertion to empty cmd_qos list
 * must call with list_mtx locked
 */
static void apu_qos_timer_start(struct qos_bound *qos_info)
{
	struct qos_counter *counter = &qos_counter;
	int i;

	LOG_DEBUG("+\n");

	for (i = 0; i < NR_APU_QOS_ENGINE; i++) {
		engine_pm_qos_counter[i].last_peak_val = 0;
		engine_pm_qos_counter[i].last_idx = qos_info->idx;
	}

	/* setup timer */
	init_timer(&counter->qos_timer);
	counter->qos_timer.function = &qos_timer_func;
	counter->qos_timer.data = 0;
	counter->qos_timer.expires =
		jiffies + msecs_to_jiffies(DEFAUTL_QOS_POLLING_TIME);
	/* record wait time in counter */
	counter->wait_ms = DEFAUTL_QOS_POLLING_TIME;
	add_timer(&counter->qos_timer);

	qos_timer_exist = true;

	LOG_DEBUG("-\n");
}

/*
 * delete timer
 * update pm qos request to default value
 * @call at deletion from cmd_qos list and result to list empty
 * must call with list_mtx locked
 */
static void apu_qos_timer_end(void)
{
	struct qos_counter *counter = &qos_counter;

	LOG_DEBUG("+\n");

	if (qos_timer_exist) {
		qos_timer_exist = false;
		del_timer_sync(&counter->qos_timer);
	}

	LOG_DEBUG("-\n");
}

/*
 * called when apusys enter suspend
 */
void apu_qos_suspend(void)
{
	struct qos_counter *counter = &qos_counter;
	struct cmd_qos *pos;
	struct qos_bound *qos_info = NULL;

	LOG_DEBUG("+\n");

	/* get qos information */
	qos_info = get_qos_bound();
	if (qos_info == NULL) {
		LOG_ERR("get info fail\n");
		return;
	}

	mutex_lock(&counter->list_mtx);

	/* no need to do suspend if no cmd exist */
	if (list_empty(&counter->list)) {
		mutex_unlock(&counter->list_mtx);
		return;
	}

	apu_qos_timer_end();

	list_for_each_entry(pos, &counter->list, list) {
		/* update running cmd to latest state before enter suspend */
		if (pos->status == CMD_RUNNING)
			update_cmd_qos(qos_info, pos);
	}

	mutex_unlock(&counter->list_mtx);

	LOG_DEBUG("-\n");
}

/*
 * called when apusys resume
 */
void apu_qos_resume(void)
{
	struct qos_counter *counter = &qos_counter;
	struct cmd_qos *pos;
	struct qos_bound *qos_info = NULL;

	LOG_DEBUG("+\n");

	/* get qos information */
	qos_info = get_qos_bound();
	if (qos_info == NULL) {
		LOG_ERR("get info fail\n");
		return;
	}

	mutex_lock(&counter->list_mtx);

	/* no need to do suspend if no cmd exist */
	if (list_empty(&counter->list)) {
		mutex_unlock(&counter->list_mtx);
		return;
	}

	list_for_each_entry(pos, &counter->list, list) {
		if (pos->status == CMD_RUNNING) {
			/* update last_idx to current pm qos idx */
			mutex_lock(&pos->mtx);
			pos->last_idx = qos_info->idx;
			mutex_unlock(&pos->mtx);
		}
	}

	apu_qos_timer_start(qos_info);

	mutex_unlock(&counter->list_mtx);

	LOG_DEBUG("-\n");
}

/*
 * enque cmd to qos_counter's linked list
 * if list is empty before enqueue, start qos timer
 */
int apu_cmd_qos_start(uint64_t cmd_id, uint64_t sub_cmd_id,
	int dev_type, int dev_core)
{
	struct qos_counter *counter = &qos_counter;
	struct cmd_qos *pos;
	struct qos_bound *qos_info = NULL;
	int core;
#if MNOC_TIME_PROFILE
	struct timeval begin, end;
	unsigned long val;
#endif

	LOG_DEBUG("+\n");

#if MNOC_TIME_PROFILE
	do_gettimeofday(&begin);
#endif

	core = apusys_dev_to_core_id(dev_type, dev_core);

	if (core == -1) {
		LOG_ERR("Invalid device(%d/%d)", dev_type, dev_core);
		return -1;
	}

	/* get qos information */
	qos_info = get_qos_bound();
	if (qos_info == NULL) {
		LOG_ERR("get info fail\n");
		return -1;
	}

	mutex_lock(&counter->list_mtx);

	/* start timer if cmd list empty */
	if (list_empty(&counter->list))
		apu_qos_timer_start(qos_info);

	list_for_each_entry(pos, &counter->list, list) {
		/* search if cmd already exist */
		if (pos->cmd_id == cmd_id && pos->sub_cmd_id == sub_cmd_id
			&& pos->core == core) {
			LOG_DEBUG("resume cmd(0x%llx/0x%llx/%d)\n",
				cmd_id, sub_cmd_id, core);
			mutex_lock(&pos->mtx);
			pos->status = CMD_RUNNING;
			mutex_unlock(&pos->mtx);

			mutex_unlock(&counter->list_mtx);
			return 0;
		}
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
	mutex_lock(&counter->list_mtx);
	val = (end.tv_sec - begin.tv_sec) * 1000000;
	val += (end.tv_usec - begin.tv_usec);
	/* LOG_INFO("val = %d us\n", val); */
	sum_start += val;
	cnt_start += 1;
	mutex_unlock(&counter->list_mtx);
#endif

	LOG_DEBUG("-\n");

	return 0;
}
EXPORT_SYMBOL(apu_cmd_qos_start);

/*
 * suspend cmd due to preemption
 * set cmd status from CMD_RUNNING to CMD_BLOCKED
 */
int apu_cmd_qos_suspend(uint64_t cmd_id, uint64_t sub_cmd_id,
	int dev_type, int dev_core)
{
	struct qos_counter *counter = &qos_counter;
	struct cmd_qos *cmd_qos = NULL, *pos;
	struct qos_bound *qos_info = NULL;
	int core;
#if MNOC_TIME_PROFILE
	struct timeval begin, end;
	unsigned long val;
#endif

	LOG_DEBUG("+\n");

#if MNOC_TIME_PROFILE
	do_gettimeofday(&begin);
#endif

	core = apusys_dev_to_core_id(dev_type, dev_core);

	/* get qos information */
	qos_info = get_qos_bound();
	if (qos_info == NULL) {
		LOG_ERR("get info fail\n");
		return -1;
	}

	mutex_lock(&counter->list_mtx);

	list_for_each_entry(pos, &counter->list, list) {
		if (pos->cmd_id == cmd_id && pos->sub_cmd_id == sub_cmd_id
			&& pos->core == core) {
			cmd_qos = pos;
			break;
		}
	}
	if (cmd_qos == NULL) {
		LOG_ERR("Can not find cmd(0x%llx/0x%llx/%d)\n",
			cmd_id, sub_cmd_id, core);
		mutex_unlock(&counter->list_mtx);
		return -1;
	} else if (cmd_qos->status == CMD_BLOCKED) {
		LOG_ERR("cmd(0x%llx/0x%llx/%d) already in suspend\n",
			cmd_id, sub_cmd_id, core);
		mutex_unlock(&counter->list_mtx);
		return -1;
	}

	mutex_lock(&cmd_qos->mtx);
	cmd_qos->status = CMD_BLOCKED;
	mutex_unlock(&cmd_qos->mtx);

	/* update cmd qos of preempted cmd to latest status */
	update_cmd_qos(qos_info, cmd_qos);

	mutex_unlock(&counter->list_mtx);

	LOG_DEBUG("-\n");

#if MNOC_TIME_PROFILE
	do_gettimeofday(&end);
	mutex_lock(&counter->list_mtx);
	val = (end.tv_sec - begin.tv_sec) * 1000000;
	val += (end.tv_usec - begin.tv_usec);
	/* LOG_INFO("val = %d us\n", val); */
	sum_suspend += val;
	cnt_suspend += 1;
	mutex_unlock(&counter->list_mtx);
#endif

	return 0;
}
EXPORT_SYMBOL(apu_cmd_qos_suspend);

/*
 * deque cmd from qos_counter's linked list
 * if list becomes empty after dequeue, delete qos timer
 */
int apu_cmd_qos_end(uint64_t cmd_id, uint64_t sub_cmd_id,
	int dev_type, int dev_core)
{
	struct qos_counter *counter = &qos_counter;
	struct cmd_qos *cmd_qos = NULL, *pos;
	struct qos_bound *qos_info = NULL;
	int core;
	int bw = 0, total_bw = 0, total_count = 0;
#if MNOC_TIME_PROFILE
	struct timeval begin, end;
	unsigned long val;
#endif

	LOG_DEBUG("+\n");

#if MNOC_TIME_PROFILE
	do_gettimeofday(&begin);
#endif

	core = apusys_dev_to_core_id(dev_type, dev_core);

	/* get qos information */
	qos_info = get_qos_bound();
	if (qos_info == NULL) {
		LOG_ERR("get info fail\n");
		return -1;
	}

	mutex_lock(&counter->list_mtx);

	list_for_each_entry(pos, &counter->list, list) {
		if (pos->cmd_id == cmd_id && pos->sub_cmd_id == sub_cmd_id
			&& pos->core == core) {
			cmd_qos = pos;
			/* core = cmd_qos->core; */
			break;
		}
	}
	if (cmd_qos == NULL) {
		LOG_ERR("Can not find cmd(0x%llx/0x%llx/%d)\n",
			cmd_id, sub_cmd_id, core);
		mutex_unlock(&counter->list_mtx);
		return -1;
	}

	/* update all cmd qos info */
	update_cmd_qos_list_locked(qos_info);

	total_bw = cmd_qos->total_bw;
	total_count = cmd_qos->count;

	/* deque cmd to counter's list */
	bw = deque_cmd_qos(cmd_qos);

	/* delete timer if cmd list empty */
	if (list_empty(&counter->list))
		apu_qos_timer_end();

	/* due to preemption,
	 * there may be multiple cmds running on the same core,
	 * need to subtract total_bw and total_count from all cmds
	 * running on the same core to prevent recalculation
	 */
	list_for_each_entry(cmd_qos, &counter->list, list) {
		if (cmd_qos->core == core) {
			mutex_lock(&cmd_qos->mtx);

			if (cmd_qos->total_bw < total_bw) {
				/* ignore sample device */
				if (cmd_qos->core < NR_APU_QOS_ENGINE)
					LOG_ERR(
						"cmd(0x%llx/0x%llx/%d) total_bw(%d) < %d",
						cmd_qos->cmd_id,
						cmd_qos->sub_cmd_id,
						cmd_qos->core,
						cmd_qos->total_bw, total_bw);
				cmd_qos->total_bw = 0;
			} else
				cmd_qos->total_bw -= total_bw;

			if (cmd_qos->count < total_count) {
				/* ignore sample device */
				if (cmd_qos->core < NR_APU_QOS_ENGINE)
					LOG_ERR(
						"cmd(0x%llx/0x%llx/%d) count(%d) < %d",
						cmd_qos->cmd_id,
						cmd_qos->sub_cmd_id,
						cmd_qos->core,
						cmd_qos->count, total_count);
				cmd_qos->count = 0;
			} else
				cmd_qos->count -= total_count;

			/* workaround to prevent including last
			 * cmd's bw due to monitor delay 1.26 ms
			 */
			cmd_qos->last_idx =
				(qos_info->idx + 1) % MTK_QOS_BUF_SIZE;

			mutex_unlock(&cmd_qos->mtx);
		}
	}

	mutex_unlock(&counter->list_mtx);

	if (!qos_timer_exist) {
		/* make sure no work_func running after timer delete */
		cancel_work_sync(&qos_work);
		apu_pm_qos_off();
	}

	LOG_DEBUG("-\n");

#if MNOC_TIME_PROFILE
	do_gettimeofday(&end);
	mutex_lock(&counter->list_mtx);
	val = (end.tv_sec - begin.tv_sec) * 1000000;
	val += (end.tv_usec - begin.tv_usec);
	/* LOG_INFO("val = %d us\n", val); */
	sum_end += val;
	cnt_end += 1;
	mutex_unlock(&counter->list_mtx);
#endif

	/* return 1 if bw = 0 (eara requirement) */
	return bw == 0 ? 1 : bw;
}
EXPORT_SYMBOL(apu_cmd_qos_end);

/*
 * create qos workqueue for count bandwidth
 * @call at module init
 */
void apu_qos_counter_init(void)
{
	int i = 0;
	struct engine_pm_qos_counter *counter = NULL;

	LOG_DEBUG("+\n");

	qos_timer_exist = false;

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

#if MNOC_TIME_PROFILE
	sum_start = 0;
	sum_suspend = 0;
	sum_end = 0;
	sum_work_func = 0;
	cnt_start = 0;
	cnt_suspend = 0;
	cnt_end = 0;
	cnt_work_func = 0;
#endif

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

	apu_qos_timer_end();

	list_for_each_entry_safe(cmd_qos, pos, &(qos_counter.list), list) {
		deque_cmd_qos(cmd_qos);
	}

	mutex_unlock(&(qos_counter.list_mtx));

	/* make sure no work_func running after module exit */
	cancel_work_sync(&qos_work);

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

	mutex_lock(&(qos_counter.list_mtx));
	list_for_each_entry(cmd_qos, &counter->list, list) {
		seq_printf(m, "cmd(0x%llx/0x%llx):\n",
			cmd_qos->cmd_id, cmd_qos->sub_cmd_id);
		seq_printf(m, "core = %d, status = %d\n",
			cmd_qos->core, cmd_qos->status);
		seq_printf(m, "total_bw = %d, last_idx = %d, count = %d\n",
			cmd_qos->total_bw, cmd_qos->last_idx, cmd_qos->count);
	}
	mutex_unlock(&(qos_counter.list_mtx));
}

#else

void notify_sspm_apusys_on(void)
{
}

void notify_sspm_apusys_off(void)
{
}

void apu_pm_qos_off(void)
{
}

void apu_qos_suspend(void)
{
}

void apu_qos_resume(void)
{
}

int apu_cmd_qos_start(uint64_t cmd_id, uint64_t sub_cmd_id,
	int dev_type, int dev_core)
{
	return 0;
}
EXPORT_SYMBOL(apu_cmd_qos_start);

int apu_cmd_qos_suspend(uint64_t cmd_id,
	uint64_t sub_cmd_id)
{
	return 0;
}
EXPORT_SYMBOL(apu_cmd_qos_suspend);

int apu_cmd_qos_end(uint64_t cmd_id, uint64_t sub_cmd_id)
{
	return 0;
}
EXPORT_SYMBOL(apu_cmd_qos_end);

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
