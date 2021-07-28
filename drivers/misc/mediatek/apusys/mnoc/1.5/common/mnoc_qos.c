// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
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
#include <linux/platform_device.h>
#include <linux/interconnect.h>
#include <linux/seq_file.h>
#include <linux/of_device.h>

#include "mnoc_drv.h"
#include "mnoc_qos.h"

#if MNOC_TIME_PROFILE
unsigned long sum_start, sum_suspend, sum_end, sum_work_func;
unsigned int cnt_start, cnt_suspend, cnt_end, cnt_work_func;
#endif

static struct engine_pm_qos_counter engine_pm_qos_counter[NR_APU_QOS_ENGINE];

#include <mtk_qos_bound.h>
#include <mtk_qos_sram.h>
#include "apusys_power.h"


#define DEFAUTL_QOS_POLLING_TIME (16)
/* define in mtk_qos_bound.h */
#define MTK_QOS_BUF_SIZE QOS_BOUND_BUF_SIZE

/* assume QOS_SMIBM_VPU0 is the first entry in qos_smibm_type for APUSYS */
#define APUSYS_QOSBOUND_START (QOS_SMIBM_VPU0)
//#define get_qosbound_enum(x) (APUSYS_QOSBOUND_START + x)
#define get_qosbound_enum(x) (x)


#if MNOC_QOS_BOOST_ENABLE || MNOC_QOS_BOOST_ENABLE
static int nr_apu_vcore_opp;
unsigned int apu_bw_vcore_opp;
#endif

static unsigned int *apu_vcore_bw_opp_tab;

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

/* indicate engine running or not based on cmd cntr for pm qos */
/* increase 1 when cmd enque, decrease 1 when cmd dequeue */
static int engine_cmd_cntr[NR_APU_QOS_ENGINE];
static int engine_boost_val[NR_APU_QOS_ENGINE];
static bool qos_timer_exist;

#if MNOC_QOS_BOOST_ENABLE
bool apu_qos_boost_flag;
static bool apusys_on_flag;
static struct pm_qos_request apu_qos_cpu_dma_req;
struct mutex apu_qos_boost_mtx;
#endif

/* register to apusys power on callback */
static void notify_sspm_apusys_on(void)
{
	LOG_DEBUG("+\n");

	qos_sram_write(APU_CLK, 1);

	LOG_DEBUG("-\n");
}

/* register to apusys power off callback(before power off) */
static void notify_sspm_apusys_off(void)
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

static void qos_timer_func(struct timer_list *timer)
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
 * must call with list_mtx locked
 */
static void apu_qos_timer_start(void)
{
	struct qos_counter *counter = &qos_counter;
	struct qos_bound *qos_info = NULL;
	int i;

	LOG_DEBUG("+\n");

	/* get qos bound */
	qos_info = get_qos_bound();
	if (qos_info == NULL) {
		LOG_ERR("get qos_info fail\n");
		return;
	}

	for (i = 0; i < NR_APU_QOS_ENGINE; i++) {
		engine_pm_qos_counter[i].last_report_bw = 0;
		engine_pm_qos_counter[i].last_idx = get_qos_bound_idx();
	}

	/* setup timer */
	timer_setup(&counter->qos_timer, qos_timer_func, jiffies +
			msecs_to_jiffies(DEFAUTL_QOS_POLLING_TIME));
	mod_timer(&counter->qos_timer, jiffies +
			msecs_to_jiffies(DEFAUTL_QOS_POLLING_TIME));

	qos_timer_exist = true;

	LOG_DEBUG("-\n");
}

/*
 * delete timer
 * update pm qos request to default value
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

void apu_qos_on(void)
{
	LOG_DEBUG("+\n");

	notify_sspm_apusys_on();
#ifdef MNOC_QOS_DEBOUNCE
	mutex_lock(&(qos_counter.list_mtx));
	apu_qos_timer_start();
	mutex_unlock((&qos_counter.list_mtx));
#endif
#if MNOC_QOS_BOOST_ENABLE
	mutex_lock(&apu_qos_boost_mtx);
	apusys_on_flag = true;
	apu_qos_boost_start();
	mutex_unlock(&apu_qos_boost_mtx);
#endif

	LOG_DEBUG("-\n");
}

void apu_qos_off(void)
{
#ifdef MNOC_QOS_DEBOUNCE
	int i = 0;
#endif

	LOG_DEBUG("+\n");

#ifdef MNOC_QOS_DEBOUNCE
	mutex_lock(&(qos_counter.list_mtx));
	apu_qos_timer_end();
	mutex_unlock(&(qos_counter.list_mtx));
	/* make sure no work_func running after timer delete */
	cancel_work_sync(&qos_work);
	for (i = 0; i < NR_APU_QOS_ENGINE; i++)
		icc_set_bw(engine_pm_qos_counter[i].emi_icc_path, 0, 0);
#endif
#if MNOC_QOS_BOOST_ENABLE
	mutex_lock(&apu_qos_boost_mtx);
	apu_qos_boost_end();
	apusys_on_flag = false;
	mutex_unlock(&apu_qos_boost_mtx);
#endif
#if APU_QOS_IPUIF_ADJUST
	apu_bw_vcore_opp = nr_apu_vcore_opp - 1;
	apu_qos_set_vcore(apu_bw_vcore_opp);
#endif
	notify_sspm_apusys_off();

	LOG_DEBUG("-\n");
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
	while (idx != ((get_qos_bound_idx() + 1) % MTK_QOS_BUF_SIZE)) {
		if (cmd_qos->core < NR_APU_QOS_ENGINE)
			cmd_qos->total_bw += get_qos_bound_apubw_mon(idx, qos_smi_idx);
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
	uint64_t sub_cmd_id, int core, uint32_t boost_val)
{
	struct qos_counter *counter = &qos_counter;
	struct qos_bound *qos_info = NULL;
	struct cmd_qos *cmd_qos = NULL;

	LOG_DEBUG("+\n");

	LOG_DEBUG("cmd_qos(0x%llx/0x%llx/%d/%d)\n",
		cmd_id, sub_cmd_id, core, boost_val);

	/* sample device has no BW */
	if (core < NR_APU_QOS_ENGINE) {
		engine_cmd_cntr[core] += 1;
		/* only allow boost val ascendance for work_func processing */
		if (engine_cmd_cntr[core] == 0 ||
			boost_val > engine_boost_val[core])
			engine_boost_val[core] = boost_val;
	}

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
	cmd_qos->last_idx = get_qos_bound_idx();
	cmd_qos->total_bw = 0;
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
	if (cmd_qos->count != 0)
		avg_bw = cmd_qos->total_bw / cmd_qos->count;
	else
		avg_bw = cmd_qos->total_bw;

	LOG_DEBUG("cmd(0x%llx/0x%llx):bw(%d/%d)\n", cmd_qos->cmd_id,
		cmd_qos->sub_cmd_id, avg_bw, cmd_qos->total_bw);

	/* sample device has no BW */
	if (cmd_qos->core < NR_APU_QOS_ENGINE) {
		engine_cmd_cntr[cmd_qos->core] -= 1;
		/*
		 * if (engine_cmd_cntr[cmd_qos->core] == 0)
		 *	engine_boost_val[cmd_qos->core] = 0;
		 */
	}

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
	int i = 0, idx = 0, current_idx;
	unsigned int peak_bw = 0, total_bw = 0, avg_bw = 0;
	unsigned int cnt = 0, bw = 0, report_bw = 0;
#if APU_QOS_IPUIF_ADJUST
	unsigned int total_apu_bw = 0, new_apu_vcore_opp = 0;
#endif
#if MNOC_TIME_PROFILE
	struct timespec begin, end;
	unsigned long val;
#endif

	LOG_DETAIL("+\n");

#if MNOC_TIME_PROFILE
	getnstimeofday(&begin);
#endif

	/* get qos bound */
	qos_info = get_qos_bound();
	if (qos_info == NULL) {
		LOG_ERR("get qos_info fail\n");
		return;
	}

	current_idx = get_qos_bound_idx();

	for (i = 0; i < NR_APU_QOS_ENGINE; i++)	{
		peak_bw = 0;
		total_bw = 0;
		cnt = 0;
		counter = &engine_pm_qos_counter[i];
		qos_smi_idx = get_qosbound_enum(i);
		/* find peak bandwidth consumption */
		idx = counter->last_idx;
		/* prevent overflow */
		if (idx == current_idx)
			continue;
		do {
			idx = (idx + 1) % MTK_QOS_BUF_SIZE;
			bw = get_qos_bound_apubw_mon(idx, qos_smi_idx);
			total_bw += bw;
			cnt++;
			peak_bw = peak_bw > bw ? peak_bw : bw;
		} while (idx != ((get_qos_bound_idx() + 1) % MTK_QOS_BUF_SIZE));

		LOG_DETAIL("idx[%d](%d ~ %d)\n", i, counter->last_idx, idx);

		counter->last_idx = idx;
		avg_bw = total_bw/cnt;

#ifdef MNOC_QOS_DEBOUNCE
		if (engine_boost_val[i] == 0)
			report_bw = avg_bw;
		else
			report_bw = peak_bw;
#else
		report_bw = peak_bw;
#endif

#if APU_QOS_IPUIF_ADJUST
		report_bw = rounddown(report_bw, 100);
		total_apu_bw += report_bw;
#endif
		/* update peak bw */
		if (counter->last_report_bw != report_bw) {
			counter->last_report_bw = report_bw;
			icc_set_bw(counter->emi_icc_path,
				   MBps_to_icc(avg_bw), MBps_to_icc(peak_bw));
		}

		LOG_DETAIL("%d: boost_val = %d, bw(%d/%d/%d)\n",
			i, engine_boost_val[i], report_bw, peak_bw, avg_bw);
	}

#if APU_QOS_IPUIF_ADJUST
	new_apu_vcore_opp = 0;
	for (i = nr_apu_vcore_opp - 1; i >= 0 ; i--) {
		if (total_apu_bw >= apu_vcore_bw_opp_tab[i])
			new_apu_vcore_opp = i;
		else
			break;
	}
#if MNOC_QOS_BOOST_ENABLE
	mutex_lock(&apu_qos_boost_mtx);
	if (new_apu_vcore_opp != apu_bw_vcore_opp &&
		apu_qos_boost_flag == false) {
		apu_bw_vcore_opp = new_apu_vcore_opp;
		apu_qos_set_vcore(apu_bw_vcore_opp);
	}
	mutex_unlock(&apu_qos_boost_mtx);
#else
	if (new_apu_vcore_opp != apu_bw_vcore_opp) {
		apu_bw_vcore_opp = new_apu_vcore_opp;
		apu_qos_set_vcore(apu_bw_vcore_opp);
	}
#endif /* MNOC_QOS_BOOST_ENABLE */
#endif /* APU_QOS_IPUIF_ADJUST */

	mutex_lock(&(qos_counter.list_mtx));
	update_cmd_qos_list_locked(qos_info);
	mutex_unlock(&(qos_counter.list_mtx));

#if MNOC_TIME_PROFILE
	getnstimeofday(&end);
	val = (end.tv_sec - begin.tv_sec) * 1000000;
	val += (end.tv_usec - begin.tv_usec);
	/* LOG_DEBUG("val = %d us\n", val); */
	sum_work_func += val;
	cnt_work_func += 1;
#endif

	LOG_DETAIL("-\n");
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
			pos->last_idx = get_qos_bound_idx();
			mutex_unlock(&pos->mtx);
		}
	}

	apu_qos_timer_start();

	mutex_unlock(&counter->list_mtx);

	LOG_DEBUG("-\n");
}

/*
 * enque cmd to qos_counter's linked list
 * if list is empty before enqueue, start qos timer
 */
int apu_cmd_qos_start(uint64_t cmd_id, uint64_t sub_cmd_id,
	int dev_type, int dev_core, uint32_t boost_val)
{
	struct qos_counter *counter = &qos_counter;
	struct cmd_qos *pos;
	struct qos_bound *qos_info = NULL;
	int core;
#if MNOC_TIME_PROFILE
	struct timespec begin, end;
	unsigned long val;
#endif

	LOG_DEBUG("+\n");

#if MNOC_TIME_PROFILE
	getnstimeofday(&begin);
#endif

	core = mnoc_drv.dev_2_core_id(dev_type, dev_core);
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

#ifndef MNOC_QOS_DEBOUNCE
	/* start timer if cmd list empty */
	if (list_empty(&counter->list))
		apu_qos_timer_start();
#endif

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
	if (enque_cmd_qos(cmd_id, sub_cmd_id, core, boost_val)) {
		LOG_ERR("enque cmd qos fail\n");
		mutex_unlock(&counter->list_mtx);
		return -1;
	}

	mutex_unlock(&counter->list_mtx);

#if MNOC_TIME_PROFILE
	getnstimeofday(&end);
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
	struct timespec begin, end;
	unsigned long val;
#endif

	LOG_DEBUG("+\n");

#if MNOC_TIME_PROFILE
	getnstimeofday(&begin);
#endif

	core = mnoc_drv.dev_2_core_id(dev_type, dev_core);


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
	getnstimeofday(&end);
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
#ifndef MNOC_QOS_DEBOUNCE
	int i;
#endif
#if MNOC_TIME_PROFILE
	struct timespec begin, end;
	unsigned long val;
#endif
	LOG_DEBUG("+\n");
#if MNOC_TIME_PROFILE
	getnstimeofday(&begin);
#endif

	core = mnoc_drv.dev_2_core_id(dev_type, dev_core);


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

#ifndef MNOC_QOS_DEBOUNCE
	/* delete timer if cmd list empty */
	if (list_empty(&counter->list))
		apu_qos_timer_end();
#endif

#ifdef PREEMPTION
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
				(qo_bound_idx() + 1) % MTK_QOS_BUF_SIZE;

			mutex_unlock(&cmd_qos->mtx);
		}
	}
#endif
	mutex_unlock(&counter->list_mtx);
#ifndef MNOC_QOS_DEBOUNCE
	if (!qos_timer_exist) {
		/* make sure no work_func running after timer delete */
		cancel_work_sync(&qos_work);
		for (i = 0; i < NR_APU_QOS_ENGINE; i++)
			icc_set_bw(engine_pm_qos_counter[i].emi_icc_path, 0, 0);
	}
#if MNOC_QOS_BOOST_ENABLE
	mutex_lock(&apu_qos_boost_mtx);
	if (apu_qos_boost_flag == false) {
		apu_bw_vcore_opp = nr_apu_vcore_opp - 1;
		apu_qos_set_vcore(apu_bw_vcore_opp);
	}
	mutex_unlock(&apu_qos_boost_mtx);
#endif
#endif
	LOG_DEBUG("-\n");

#if MNOC_TIME_PROFILE
	getnstimeofday(&end);
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

void apu_qos_boost_start(void)
{
	struct engine_pm_qos_counter *counter = NULL;

	LOG_DEBUG("+\n");
	counter = &engine_pm_qos_counter[0];
#if MNOC_QOS_BOOST_ENABLE
/* 6885: ~16G, 6873/6853: ~8G */
	if (apu_qos_boost_flag && apusys_on_flag) {
		apu_bw_vcore_opp = 2;
		icc_set_bw(counter->emi_icc_path, 0,
			   Mbps_to_icc(apu_vcore_bw_opp_tab[apu_bw_vcore_opp]));
		cpu_latency_qos_update_request(&apu_qos_cpu_dma_req, 2);
#if APU_QOS_IPUIF_ADJUST
		apu_qos_set_vcore(apu_bw_vcore_opp);
#endif
	}
#endif
	LOG_DEBUG("-\n");
}

void apu_qos_boost_end(void)
{
	struct engine_pm_qos_counter *counter = NULL;

	LOG_DEBUG("+\n");
	counter = &engine_pm_qos_counter[0];

#if MNOC_QOS_BOOST_ENABLE
	if (apusys_on_flag) {
		apu_bw_vcore_opp = nr_apu_vcore_opp - 1;

#if APU_QOS_IPUIF_ADJUST
		apu_qos_set_vcore(apu_bw_vcore_opp);
#endif
		icc_set_bw(counter->emi_icc_path, 0, 0);
		cpu_latency_qos_update_request(&apu_qos_cpu_dma_req,
			PM_QOS_DEFAULT_VALUE);
	}
#endif
	LOG_DEBUG("-\n");
}

/*
 * create qos workqueue for count bandwidth
 * @call at module init
 */
void apu_qos_counter_init(struct device *dev)
{
	struct engine_pm_qos_counter *counter = NULL;
	struct apu_mnoc *p_mnoc = dev_get_drvdata(dev);
	struct mnoc_plat_drv *mnoc_drv = NULL;
	struct icc_path *apu_icc = of_icc_get(dev, "apu-bw");
	int i = 0;

	if (!p_mnoc) {
		dev_info(dev, "%s not get struct apu_mnoc\n", __func__);
		return;
	}
	LOG_DEBUG("+\n");

	mnoc_drv = (struct mnoc_plat_drv *)of_device_get_match_data(dev);
	if (!mnoc_drv) {
		dev_info(dev, "%s not get mnoc_plat_drv\n", __func__);
		return;
	}

	/* Get vcore bw table from platform data */
	apu_vcore_bw_opp_tab = mnoc_drv->vcore_bw_opp_tab;
	nr_apu_vcore_opp = mnoc_drv->nr_vcore_opp;

	/*
	 * put engine_pm_qos_counter to struct apu_mnoc
	 * such that mnoc_qos_sys.c can get it from dev_get_drvdata
	 */
	p_mnoc->engines = engine_pm_qos_counter;

	if (!apu_icc) {
		dev_info(dev, "%s not get apu-bw icc path\n", __func__);
		return;
	}
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
		counter->last_report_bw = 0;
		counter->last_idx = 0;
		counter->core = i;
		counter->emi_icc_path = apu_icc;
	}
#if MNOC_QOS_BOOST_ENABLE
	apu_qos_boost_flag = false;
	apusys_on_flag = false;
	mutex_init(&apu_qos_boost_mtx);
	cpu_latency_qos_add_request(&apu_qos_cpu_dma_req, PM_QOS_DEFAULT_VALUE);
#endif

#if APU_QOS_IPUIF_ADJUST
	apu_bw_vcore_opp = nr_apu_vcore_opp - 1;
#endif

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
void apu_qos_counter_destroy(struct device *dev)
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
	}
	/* relese bw icc path */
	icc_put(counter->emi_icc_path);
#if MNOC_QOS_BOOST_ENABLE
	cpu_latency_qos_update_request(&apu_qos_cpu_dma_req,
		PM_QOS_DEFAULT_VALUE);
	cpu_latency_qos_remove_request(&apu_qos_cpu_dma_req);
#endif
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
