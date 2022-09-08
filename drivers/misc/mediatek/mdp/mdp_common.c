// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#include "mdp_common.h"
#include "cmdq_device.h"
#include "cmdq_record.h"
#include "cmdq_reg.h"
#if IS_ENABLED(CONFIG_MMPROFILE)
#include "cmdq_mmp.h"
#endif
#ifdef MDP_COMMON_ENG_SUPPORT
#include "mdp_engine_common.h"
#endif

#include <linux/slab.h>
#include <linux/pm_qos.h>
#include <linux/math64.h>
#include "mdp_pmqos.h"

#include "cmdq_helper_ext.h"

#include <linux/dmapool.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/iopoll.h>
#include <linux/mailbox_controller.h>
#include <linux/notifier.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/sched/clock.h>
#include <linux/timekeeping.h>
//#include <linux/interconnect-provider.h>
#include "mtk-interconnect.h"
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include <soc/mediatek/smi.h>

#ifdef CMDQ_SECURE_PATH_SUPPORT
#include <cmdq-sec.h>
#include "mtk_heap.h"
#endif

#ifdef MDP_MMPATH
#ifndef CREATE_TRACE_POINTS
#define CREATE_TRACE_POINTS
#endif

#include "mmpath.h"
#endif	/* MDP_MMPATH */


#include <cmdq-util.h>
#include <soc/mediatek/smi.h>

/* mmdvfs with regulator */
struct regulator *mdp_mmdvfs_reg;
struct regulator *isp_mmdvfs_reg;
u64 *mdp_pmqos_freq;
u64 *isp_pmqos_freq;
int *mdp_volts;
int *isp_volts;
int mdp_pmqos_opp_num;
u64 mdp_current_freq[MDP_TOTAL_THREAD];

#define CMDQ_LOG_PMQOS(string, args...) \
do {			\
	if (cmdq_core_should_pmqos_log()) { \
		pr_notice("[MDP][PMQOS]"string, ##args); \
	} \
} while (0)


#define DP_TIMER_GET_DURATION_IN_US(start, end, duration)		\
do {									\
	u64 time1;							\
	u64 time2;							\
									\
	time1 = (u64)(start.tv_sec) * 1000000000 +			\
		(u64)(start.tv_nsec);					\
	time2 = (u64)(end.tv_sec) * 1000000000   +			\
		(u64)(end.tv_nsec);					\
									\
	if (time1 >= time2)						\
		duration = 1;						\
	else								\
		duration = (s32)div_u64(time2 - time1, 1000);		\
	if (duration == 0)						\
		duration = 1;						\
} while (0)

#define DP_BANDWIDTH(data, pixel, throughput, bandwidth)		\
do {									\
	u64 numerator;							\
	u64 denominator;						\
									\
	/* ocucpied bw efficiency is 1.33 while accessing DRAM */	\
	numerator =							\
		(u64)(div_u64((u64)(data) * 4 * (u64)(throughput), 3));	\
	denominator = (u64)(pixel);					\
	if (denominator == 0)						\
		denominator = 1;					\
	bandwidth = (u32)(div_u64(numerator, denominator));		\
} while (0)

struct mdp_task {
	char callerName[TASK_COMM_LEN];
	char userDebugStr[DEBUG_STR_LEN];
};
static struct mdp_task mdp_tasks[MDP_MAX_TASK_NUM];
static int mdp_tasks_idx;
static long cmdq_mmsys_base;

#define MDP_THREAD_COUNT ( \
	CMDQ_MAX_THREAD_COUNT - MDP_THREAD_START)

struct mdp_thread {
	u32 task_count;
	u64 engine_flag;
	bool acquired;
	bool allow_dispatch;
	bool secure;
};

struct mdp_pq_readback {
	u16 dre30_hist_sram_start;
	u16 rb_thread_id;
	u16 rb_event_lock;
	u16 rb_event_unlock;
};

struct mdp_context {
	struct list_head tasks_wait;	/* task waiting for available thread */
	struct mdp_thread thread[CMDQ_MAX_THREAD_COUNT];
	struct EngineStruct engine[CMDQ_MAX_ENGINE_COUNT];

	/* Resource manager information */
	struct list_head resource_list;	/* all resource list */

	/* delay resource check workqueue */
	struct workqueue_struct *resource_check_queue;

	/* consume task from wait list */
	struct work_struct handle_consume_item;
	struct workqueue_struct *handle_consume_queue;

	/* smi clock usage */
	atomic_t mdp_smi_usage;

	struct device *larb;
	struct mdp_pq_readback pq_readback;
};
static struct mdp_context mdp_ctx;
static struct cmdq_buf_pool mdp_pool;
atomic_t mdp_pool_cnt;
static u32 mdp_pool_limit = 256;

static DEFINE_MUTEX(mdp_clock_mutex);
static DEFINE_MUTEX(mdp_task_mutex);
static DEFINE_MUTEX(mdp_thread_mutex);
static DEFINE_MUTEX(mdp_resource_mutex);

/* thread acquire notification */
static wait_queue_head_t mdp_thread_dispatch;

static struct notifier_block mdp_status_dump_notify;

#define LEGACY_DRE30_HIST_SRAM_START	1024

/* MDP common kernel logic */

struct EngineStruct *cmdq_mdp_get_engines(void)
{
	return mdp_ctx.engine;
}

static void cmdq_mdp_reset_engine_struct(void)
{
	int index;

	/* Reset engine status */
	for (index = 0; index < CMDQ_MAX_ENGINE_COUNT; index++)
		mdp_ctx.engine[index].currOwner = CMDQ_INVALID_THREAD;
}

static void cmdq_mdp_reset_thread_struct(void)
{
	int index;

	/* Reset thread status */
	memset(mdp_ctx.thread, 0, sizeof(mdp_ctx.thread[0]) *
		ARRAY_SIZE(mdp_ctx.thread));
	for (index = 0; index < ARRAY_SIZE(mdp_ctx.thread); index++)
		mdp_ctx.thread[index].allow_dispatch = true;

#ifdef CMDQ_SECURE_PATH_SUPPORT
	for (index = CMDQ_MIN_SECURE_THREAD_ID;
		index < CMDQ_MIN_SECURE_THREAD_ID +
		CMDQ_MAX_SECURE_THREAD_COUNT; index++)
		mdp_ctx.thread[index].secure = true;
#endif
}

void cmdq_mdp_delay_check_unlock(const u64 engine_not_use)
{
	/* Check engine in enginesNotUsed */
	struct ResourceUnitStruct *res = NULL;

	list_for_each_entry(res, &mdp_ctx.resource_list, list_entry) {
		if (!(engine_not_use & res->engine_flag))
			continue;

		mutex_lock(&mdp_resource_mutex);
		/* find matched engine become not used*/
		if (!res->used) {
			/* resource is not used but we got engine is released!
			 * log as error and still continue
			 */
			CMDQ_ERR(
				"[Res]resource will delay but not used, engine:0x%llx\n",
				res->engine_flag);
		}
		/* Cancel previous delay task if existed */
		if (res->delaying) {
			res->delaying = false;
			cancel_delayed_work(&res->delayCheckWork);
		}
		/* Start a new delay task */
		CMDQ_VERBOSE("[Res]queue delay unlock resource engine:0x%llx\n",
			engine_not_use);
		queue_delayed_work(mdp_ctx.resource_check_queue,
			&res->delayCheckWork,
			CMDQ_DELAY_RELEASE_RESOURCE_MS);
		res->delay = sched_clock();
		res->delaying = true;
		mutex_unlock(&mdp_resource_mutex);
	}
}

void cmdq_mdp_fix_command_scenario_for_user_space(
	struct cmdqCommandStruct *command)
{
	if (command->scenario == CMDQ_SCENARIO_USER_DISP_COLOR ||
		command->scenario == CMDQ_SCENARIO_USER_MDP) {
		CMDQ_VERBOSE("user space request, scenario:%d\n",
			command->scenario);
	} else {
		CMDQ_VERBOSE(
			"[WARNING]fix user space request to CMDQ_SCENARIO_USER_SPACE\n");
		command->scenario = CMDQ_SCENARIO_USER_SPACE;
	}
}

bool cmdq_mdp_is_request_from_user_space(
	const enum CMDQ_SCENARIO_ENUM scenario)
{
	switch (scenario) {
	case CMDQ_SCENARIO_USER_DISP_COLOR:
	case CMDQ_SCENARIO_USER_MDP:
	case CMDQ_SCENARIO_USER_SPACE:	/* phased out */
		return true;
	default:
		return false;
	}
	return false;
}

s32 cmdq_mdp_query_usage(s32 *counters)
{
	struct EngineStruct *engine;
	s32 index;

	engine = mdp_ctx.engine;

	mutex_lock(&mdp_thread_mutex);
	for (index = 0; index < CMDQ_MAX_ENGINE_COUNT; index++)
		counters[index] = engine[index].userCount;
	mutex_unlock(&mdp_thread_mutex);

	return 0;
}

s32 cmdq_mdp_get_smi_usage(void)
{
	return atomic_read(&mdp_ctx.mdp_smi_usage);
}

static void cmdq_mdp_common_clock_enable(u64 engine_flag)
{
	s32 smi_ref = atomic_inc_return(&mdp_ctx.mdp_smi_usage);

	CMDQ_LOG_CLOCK("%s MDP SMI clock enable %d, engine_flag:%llx\n",
		__func__, smi_ref, engine_flag);
	cmdq_mdp_get_func()->mdpEnableCommonClock(true, engine_flag);

	CMDQ_PROF_MMP(mdp_mmp_get_event()->MDP_clock_smi,
		MMPROFILE_FLAG_PULSE, smi_ref, 1);
}

static void cmdq_mdp_common_clock_disable(u64 engine_flag)
{
	s32 smi_ref = atomic_dec_return(&mdp_ctx.mdp_smi_usage);

	CMDQ_LOG_CLOCK("%s MDP SMI clock disable %d, engine_flag:%llx\n",
		__func__, smi_ref, engine_flag);
	cmdq_mdp_get_func()->mdpEnableCommonClock(false, engine_flag);

	CMDQ_PROF_MMP(mdp_mmp_get_event()->MDP_clock_smi,
		MMPROFILE_FLAG_PULSE, smi_ref, 0);
}

static s32 cmdq_mdp_clock_enable(u64 engine_flag)
{
	s32 ret;

	mutex_lock(&mdp_clock_mutex);

	CMDQ_LOG_CLOCK("%s engine:0x%llx\n", __func__, engine_flag);

	/* common clock enable when get enabled engine,
	 * thus only enable mdp engine clocks.
	 */
	ret = cmdq_mdp_get_func()->mdpClockOn(engine_flag);

	mutex_unlock(&mdp_clock_mutex);

	CMDQ_PROF_MMP(mdp_mmp_get_event()->MDP_clock_on,
		MMPROFILE_FLAG_PULSE, (u32)(engine_flag >> 32),
		(u32)engine_flag);

	return ret;
}

static s32 cmdq_mdp_clock_disable(u64 engine_flag)
{
	s32 ret;

	CMDQ_LOG_CLOCK("%s engine:0x%llx\n", __func__, engine_flag);

	mutex_lock(&mdp_clock_mutex);

	ret = cmdq_mdp_get_func()->mdpClockOff(engine_flag);

	CMDQ_PROF_MMP(mdp_mmp_get_event()->MDP_clock_off,
	      MMPROFILE_FLAG_PULSE, (u32)(engine_flag >> 32),
	      (u32)engine_flag);

	mutex_unlock(&mdp_clock_mutex);

	return ret;
}

void cmdq_mdp_reset_resource(void)
{
	struct ResourceUnitStruct *resource;

	list_for_each_entry(resource, &mdp_ctx.resource_list,
		list_entry) {
		mutex_lock(&mdp_resource_mutex);
		if (resource->lend) {
			CMDQ_LOG("[Res]Client is already lend, event:%d\n",
				resource->lockEvent);
			cmdqCoreClearEvent(resource->lockEvent);
		}
		mutex_unlock(&mdp_resource_mutex);
	}
}

/* Use CMDQ as Resource Manager */
void cmdq_mdp_unlock_resource(struct work_struct *workItem)
{
	struct ResourceUnitStruct *res = NULL;
	struct delayed_work *delayedWorkItem = NULL;
	s32 status = 0;

	delayedWorkItem = container_of(workItem, struct delayed_work, work);
	res = container_of(delayedWorkItem, struct ResourceUnitStruct,
		delayCheckWork);

	mutex_lock(&mdp_resource_mutex);

	CMDQ_MSG("[Res]unlock resource with engine:0x%llx\n",
		res->engine_flag);
	if (res->used && res->delaying) {
		res->unlock = sched_clock();
		res->used = false;
		res->delaying = false;
		/* delay time is reached and unlock resource */
		if (!res->availableCB) {
			/* print error message */
			CMDQ_LOG("[Res]available CB func is NULL, event:%d\n",
				res->lockEvent);
		} else {
			CmdqResourceAvailableCB cb_func = res->availableCB;

			/* before call callback, release lock at first */
			mutex_unlock(&mdp_resource_mutex);
			status = cb_func(res->lockEvent);
			mutex_lock(&mdp_resource_mutex);

			if (status < 0) {
				/* Error status print */
				CMDQ_ERR("[Res]available CB %d fail:%d\n",
					res->lockEvent, status);
			}
		}
	}
	mutex_unlock(&mdp_resource_mutex);
}

void cmdq_mdp_init_resource(u32 engine_id,
	enum cmdq_event res_event)
{
	struct ResourceUnitStruct *res;

	res = kzalloc(sizeof(struct ResourceUnitStruct), GFP_KERNEL);
	if (!res) {
		CMDQ_ERR("not enough mem for resource delay work\n");
		return;
	}

	CMDQ_LOG("[Res]Init resource engine:%u event:%u\n",
		engine_id, res_event);

	res->engine_id = engine_id;
	res->engine_flag = (1LL << engine_id);
	res->lockEvent = res_event;
	INIT_DELAYED_WORK(&res->delayCheckWork, cmdq_mdp_unlock_resource);
	INIT_LIST_HEAD(&res->list_entry);
	list_add_tail(&res->list_entry, &mdp_ctx.resource_list);
}

void cmdq_mdp_enable_res(u64 engine_flag, bool enable)
{
	struct ResourceUnitStruct *res = NULL;

	mutex_lock(&mdp_clock_mutex);

	list_for_each_entry(res, &mdp_ctx.resource_list, list_entry) {
		if (!(res->engine_flag & engine_flag))
			continue;

		CMDQ_LOG("[Res]resource clock engine:0x%llx enable:%s\n",
			engine_flag, enable ? "true" : "false");
		cmdq_mdp_get_func()->enableMdpClock(enable, res->engine_id);
		break;
	}

	mutex_unlock(&mdp_clock_mutex);
}

static void cmdq_mdp_lock_res_impl(struct ResourceUnitStruct *res,
	u64 engine_flag, bool from_notify)
{
	mutex_lock(&mdp_resource_mutex);

	/* find matched engine */
	if (from_notify)
		res->notify = sched_clock();
	else
		res->lock = sched_clock();

	if (!res->used) {
		/* First time used */
		s32 status;

		CMDQ_MSG("[Res]Lock res engine:0x%llx notify:%d release\n",
			engine_flag, from_notify);

		res->used = true;
		if (!res->releaseCB) {
			CMDQ_LOG("[Res]release CB func is NULL, event:%d\n",
				res->lockEvent);
		} else {
			CmdqResourceReleaseCB cb_func = res->releaseCB;

			/* release mutex before callback */
			mutex_unlock(&mdp_resource_mutex);
			status = cb_func(res->lockEvent);
			mutex_lock(&mdp_resource_mutex);

			if (status < 0) {
				/* Error status print */
				CMDQ_ERR("[Res]release CB %d fail:%d\n",
					res->lockEvent, status);
			}
		}
	} else {
		CMDQ_VERBOSE(
			"[Res]resource already in use engine:0x%llx notify:%d\n",
			engine_flag, from_notify);
		/* Cancel previous delay task if existed */
		if (res->delaying) {
			res->delaying = false;
			cancel_delayed_work(&res->delayCheckWork);
		}
	}
	mutex_unlock(&mdp_resource_mutex);
}

/* Use CMDQ as Resource Manager */
void cmdq_mdp_lock_resource(u64 engine_flag, bool from_notify)
{
	struct ResourceUnitStruct *res = NULL;

	list_for_each_entry(res, &mdp_ctx.resource_list, list_entry) {
		if (engine_flag & res->engine_flag)
			cmdq_mdp_lock_res_impl(res, engine_flag, from_notify);
	}
}

bool cmdq_mdp_acquire_resource(enum cmdq_event res_event,
	u64 *engine_flag_out)
{
	struct ResourceUnitStruct *res = NULL;
	bool result = false;

	list_for_each_entry(res, &mdp_ctx.resource_list, list_entry) {
		if (res_event != res->lockEvent)
			continue;

		mutex_lock(&mdp_resource_mutex);
		/* find matched resource */
		result = !res->used;
		if (result && !res->lend) {
			CMDQ_MSG("[Res]Acquire successfully event:%d\n",
				res_event);
			cmdqCoreClearEvent(res_event);
			res->acquire = sched_clock();
			res->lend = true;
			*engine_flag_out |= res->engine_flag;
		}
		mutex_unlock(&mdp_resource_mutex);
		break;
	}
	return result;
}

void cmdq_mdp_release_resource(enum cmdq_event res_event,
	u64 *engine_flag_out)
{
	struct ResourceUnitStruct *res = NULL;

	CMDQ_MSG("[Res]Release resource with event:%d\n", res_event);
	list_for_each_entry(res, &mdp_ctx.resource_list, list_entry) {
		if (res_event != res->lockEvent)
			continue;
		mutex_lock(&mdp_resource_mutex);
		/* find matched resource */
		if (res->lend) {
			res->release = sched_clock();
			res->lend = false;
			*engine_flag_out |= res->engine_flag;
		}
		mutex_unlock(&mdp_resource_mutex);
		break;
	}
}

void cmdq_mdp_set_resource_callback(enum cmdq_event res_event,
	CmdqResourceAvailableCB res_available,
	CmdqResourceReleaseCB res_release)
{
	struct ResourceUnitStruct *res = NULL;

	CMDQ_VERBOSE(
		"[Res]Set resource callback with event:%d available:%ps release:%ps\n",
		res_event, res_available, res_release);
	list_for_each_entry(res, &mdp_ctx.resource_list, list_entry) {
		if (res_event != res->lockEvent)
			continue;

		CMDQ_MSG("[Res]Set resource callback ok!\n");
		mutex_lock(&mdp_resource_mutex);
		/* find matched resource */
		res->availableCB = res_available;
		res->releaseCB = res_release;
		mutex_unlock(&mdp_resource_mutex);
		break;
	}
}

static u64 cmdq_mdp_get_engine_flag_for_enable_clock(
	u64 engine_flag, s32 thread_id)
{
	struct EngineStruct *engine = mdp_ctx.engine;
	struct mdp_thread *thread = mdp_ctx.thread;
	u64 engine_flag_clk = 0;
	u32 index;

	for (index = 0; index < CMDQ_MAX_ENGINE_COUNT; index++) {
		if (!(engine_flag & (1LL << index)))
			continue;

		if (engine[index].userCount <= 0) {
			engine[index].currOwner = thread_id;
			engine_flag_clk |= (1LL << index);
			/* also assign engine flag into ThreadStruct */
			thread[thread_id].engine_flag |= (1LL << index);
		}

		engine[index].userCount++;
	}

	return engine_flag_clk;
}

static void cmdq_mdp_lock_thread(struct cmdqRecStruct *handle)
{
	u64 engine_flag = handle->engineFlag;
	s32 thread = handle->thread;

	if (unlikely(thread < 0)) {
		CMDQ_ERR("%s invalid thread:%d engine:0x%llx\n",
			__func__, thread, engine_flag);
		return;
	}
	/* engine clocks enable flag decide here but call clock on before flush
	 * common clock enable here to avoid disable when mdp engines still
	 * need use for later tasks
	 */
	CMDQ_MSG("%s handle:0x%p pkt:0x%p engine:0x%016llx\n",
		__func__, handle, handle->pkt, handle->engineFlag);
	cmdq_mdp_common_clock_enable(handle->engineFlag);

	CMDQ_PROF_START(current->pid, __func__);

	handle->engine_clk = cmdq_mdp_get_engine_flag_for_enable_clock(
		engine_flag, thread);

	/* make this thread can be dispath again */
	mdp_ctx.thread[thread].allow_dispatch = true;
	mdp_ctx.thread[thread].task_count++;

	/* assign client since mdp acquire thread after create pkt */
	handle->pkt->cl = cmdq_helper_mbox_client(thread);

	if (!handle->secData.is_secure)
		cmdq_mbox_enable(((struct cmdq_client *) handle->pkt->cl)->chan);
#ifdef CMDQ_SECURE_PATH_SUPPORT
	else
		cmdq_sec_mbox_enable(((struct cmdq_client *) handle->pkt->cl)->chan);
#endif

	CMDQ_PROF_END(current->pid, __func__);
}

static u64 cmdq_mdp_get_not_used_engine(const u64 engine_flag)
{
	struct EngineStruct *engine = mdp_ctx.engine;
	struct mdp_thread *thread = mdp_ctx.thread;
	u64 engine_not_use = 0LL;
	s32 index;
	s32 owner_thd = CMDQ_INVALID_THREAD;

	for (index = 0; index < CMDQ_MAX_ENGINE_COUNT; index++) {
		if (!(engine_flag & (1LL << index)))
			continue;

		engine[index].userCount--;
		if (engine[index].userCount <= 0) {
			engine_not_use |= (1LL << index);
			owner_thd = engine[index].currOwner;
			/* remove engine flag in assigned pThread */
			thread[owner_thd].engine_flag &= ~(1LL << index);
			engine[index].currOwner = CMDQ_INVALID_THREAD;
		}
	}
	CMDQ_VERBOSE("%s engine not use:0x%llx\n", __func__, engine_not_use);
	return engine_not_use;
}

void cmdq_mdp_unlock_thread(struct cmdqRecStruct *handle)
{
	u64 engine_flag = handle->engineFlag;
	s32 thread = handle->thread;

	if (unlikely(thread < 0)) {
		CMDQ_ERR("%s invalid thread:%d engine:0x%llx\n",
			__func__, thread, engine_flag);
		return;
	}
	mutex_lock(&mdp_thread_mutex);

	/* get not use engine using engine flag for disable clock. */
	handle->engine_clk = cmdq_mdp_get_not_used_engine(engine_flag);

	if (!mdp_ctx.thread[thread].task_count)
		CMDQ_ERR(
			"count fatal error thread:%u count:%u allow:%s acquire:%s\n",
			thread, mdp_ctx.thread[thread].task_count,
			mdp_ctx.thread[thread].allow_dispatch ?
			"true" : "false",
			mdp_ctx.thread[thread].acquired ? "true" : "false");
	mdp_ctx.thread[thread].task_count--;

	/* if no task on thread, release to cmdq core */
	/* no need to release thread since secure path use static thread */
	if (!mdp_ctx.thread[thread].task_count && !handle->secData.is_secure) {
		cmdq_core_release_thread(handle->scenario, thread);
		mdp_ctx.thread[thread].acquired = false;
	}

	cmdq_mdp_delay_check_unlock(handle->engine_clk);

	mutex_unlock(&mdp_thread_mutex);
}

static void cmdq_mdp_handle_prepare(struct cmdqRecStruct *handle)
{
	if (handle->thread == CMDQ_INVALID_THREAD) {
		/* not expect call without thread during ending */
		CMDQ_ERR("handle:0x%p with invalid thread engine:0x%llx\n",
			handle, handle->engineFlag);
	}
}

static void cmdq_mdp_handle_unprepare(struct cmdqRecStruct *handle)
{
	if (handle->thread == CMDQ_INVALID_THREAD) {
		/* not expect call without thread during ending */
		CMDQ_ERR("handle:0x%p with invalid thread engine:0x%llx\n",
			handle, handle->engineFlag);
		return;
	}

	/* only handle if this handle run by mdp flush and thread
	 * unlock thread usage when cmdq ending this handle
	 */
	cmdq_mdp_unlock_thread(handle);
}

static void cmdq_mdp_handle_stop(struct cmdqRecStruct *handle)
{
	if (!handle) {
		CMDQ_ERR("%s empty handle\n", __func__);
		return;
	}

	/* make sure smi clock off at last */
	mutex_lock(&mdp_thread_mutex);

	if (!handle->secData.is_secure)
		cmdq_mbox_disable(((struct cmdq_client *) handle->pkt->cl)->chan);
#ifdef CMDQ_SECURE_PATH_SUPPORT
	else
		cmdq_sec_mbox_disable(((struct cmdq_client *)handle->pkt->cl)->chan);
#endif

	cmdq_mdp_common_clock_disable(handle->engineFlag);

	mutex_unlock(&mdp_thread_mutex);
}


#ifdef CMDQ_SECURE_PATH_SUPPORT
static s32 cmdq_mdp_check_engine_waiting_unlock(struct cmdqRecStruct *handle)
{
	const u32 max_thd = cmdq_dev_get_thread_count();
	u32 i;

	for (i = MDP_THREAD_START; i < max_thd; i++) {
		if (!(mdp_ctx.thread[i].engine_flag & handle->engineFlag))
			continue;
		/* same secure path, can be dispatch */
		if (mdp_ctx.thread[i].task_count &&
			handle->secData.is_secure != mdp_ctx.thread[i].secure) {
			CMDQ_LOG(
				"sec engine busy %u count:%u engine:%#llx & %#llx\n",
				i, mdp_ctx.thread[i].task_count,
				mdp_ctx.thread[i].engine_flag,
				handle->engineFlag);
			return -EBUSY;
		}
	}

	/* same engine does not exist in working threads */
	return 0;
}
#endif

/* check if engine conflict when thread dispatch
 * Parameter:
 *	task: [IN] current check task with engine flag and secure flag.
 *	forceLog: [IN] print debug log
 *	*pThreadOut:
 *         [IN] prefer thread. please pass CMDQ_INVALID_THREAD if no prefere
 *         [OUT] dispatch thread result
 * Return:
 *     0 for success; else the error code is returned
 */
static bool cmdq_mdp_check_engine_conflict(
	struct cmdqRecStruct *handle, s32 *thread_out)
{
	struct EngineStruct *engine_list = mdp_ctx.engine;
	u32 free, i;
	s32 thread;
	u64 engine_flag;
	bool conflict = false;

	engine_flag = handle->engineFlag;
	thread = *thread_out;
	free = thread == CMDQ_INVALID_THREAD ?
		0xFFFFFFFF : 0xFFFFFFFF & (~(0x1 << thread));

	/* check if engine conflict */
	for (i = 0; i < CMDQ_MAX_ENGINE_COUNT && engine_flag != 0; i++) {
		if (!(engine_flag & (0x1LL << i)))
			continue;

		if (engine_list[i].currOwner == CMDQ_INVALID_THREAD) {
			continue;
		} else if (thread == CMDQ_INVALID_THREAD) {
			thread = engine_list[i].currOwner;
			free &= ~(0x1 << thread);
		} else if (thread != engine_list[i].currOwner) {
			/* Partial HW occupied by different threads,
			 * we need to wait.
			 */
			conflict = true;
			thread = CMDQ_INVALID_THREAD;
			CMDQ_MSG(
				"engine conflict handle:0x%p engine:0x%llx conflict engine idx:%u thd:0x%x free:0x%08x owner:%d\n",
				handle, handle->engineFlag, i,
				thread, free, engine_list[i].currOwner);
			break;
		}

		engine_flag &= ~(0x1LL << i);
	}

	*thread_out = thread;
	return conflict;
}

#ifdef CMDQ_SECURE_PATH_SUPPORT
static s32 cmdq_mdp_get_sec_thread(void)
{
	return CMDQ_THREAD_SEC_MDP;
}
#endif

static s32 cmdq_mdp_find_free_thread(struct cmdqRecStruct *handle)
{
	bool conflict;
	s32 thread = CMDQ_INVALID_THREAD;
	u32 index;
	struct mdp_thread *threads;
	const u32 max_thd = cmdq_dev_get_thread_count();

#ifdef CMDQ_SECURE_PATH_SUPPORT
	if (cmdq_mdp_check_engine_waiting_unlock(handle) < 0)
		return CMDQ_INVALID_THREAD;

	if (handle->secData.is_secure)
		return cmdq_mdp_get_sec_thread();
#endif
	conflict = cmdq_mdp_check_engine_conflict(handle, &thread);
	if (conflict) {
		CMDQ_LOG(
			"engine conflict handle:0x%p engine:0x%llx thread:%d\n",
			handle, handle->engineFlag, thread);
		return CMDQ_INVALID_THREAD;
	}
	/* same engine used in current thread, use it */
	if (thread != CMDQ_INVALID_THREAD)
		return thread;

	/* dispatch from free threads */
	threads = mdp_ctx.thread;
	for (index = MDP_THREAD_START; index < max_thd; index++) {

#if defined(CMDQ_SECURE_PATH_SUPPORT)
		/* This thread is dedicated for readback command */
		if (index == mdp_ctx.pq_readback.rb_thread_id)
			continue;
#endif

		if (!threads[index].acquired || threads[index].engine_flag ||
			threads[index].task_count ||
			!threads[index].allow_dispatch) {
			CMDQ_MSG(
				"thread not available:%d eng:0x%llx count:%u allow:%u\n",
				index, threads[index].engine_flag,
				threads[index].task_count,
				threads[index].allow_dispatch);
			continue;
		}

		thread = index;
		threads[index].allow_dispatch = false;
		CMDQ_ERR("got thread:%d handle:0x%p which is not possible\n",
			index, handle);
		break;
	}

	/* if we still not have thread, ask cmdq core to get new one */
	if (thread == CMDQ_INVALID_THREAD) {
		thread = cmdq_core_acquire_thread(handle->scenario, true);
		if (thread != CMDQ_INVALID_THREAD) {
			threads[thread].acquired = true;
			threads[thread].allow_dispatch = false;
		} else if (!handle->engineFlag) {
			/* for engine flag empty, assign acquired thread */
			for (index = MDP_THREAD_START;
				index < max_thd; index++) {
				if (!threads[index].acquired)
					continue;
				thread = index;
			}
		}
		CMDQ_MSG("acquire thread:%d\n", thread);
	}

	return thread;
}

static s32 cmdq_mdp_consume_handle(void)
{
	s32 err;
	struct cmdqRecStruct *handle, *temp;
	u32 index;
	bool acquired = false;
	struct CmdqCBkStruct *callback = cmdq_core_get_group_cb();
	bool secure_run = false;
#ifdef CMDQ_SECURE_PATH_SUPPORT
	struct ContextStruct *ctx;
	u32 task_cnt;
#endif

	/* operation for tasks_wait list need task mutex */
	mutex_lock(&mdp_task_mutex);

	CMDQ_MSG("%s\n", __func__);

	CMDQ_PROF_MMP(mdp_mmp_get_event()->consume_done, MMPROFILE_FLAG_START,
		current->pid, 0);

	handle = list_first_entry_or_null(&mdp_ctx.tasks_wait, struct cmdqRecStruct,
		list_entry);
	if (handle)
		secure_run = handle->secData.is_secure;

	/* loop waiting list for pending handles */
	list_for_each_entry_safe(handle, temp, &mdp_ctx.tasks_wait,
		list_entry) {
		/* operations for thread list need thread lock */
		mutex_lock(&mdp_thread_mutex);

		if (secure_run != handle->secData.is_secure) {
			mutex_unlock(&mdp_thread_mutex);
			CMDQ_LOG(
				"skip secure inorder handle:%p engine:%#llx sec:%s\n",
				handle, handle->engineFlag,
				handle->secData.is_secure ? "true" : "false");
			break;
		}

#ifdef CMDQ_SECURE_PATH_SUPPORT
		if (handle->secData.is_secure) {
			ctx = cmdq_core_get_context();
			task_cnt = ctx->thread[(u32)cmdq_mdp_get_sec_thread()].handle_count;
			/* sec thread and more than 4 task -> queue the task */
			if (task_cnt + 1 > CMDQ_MAX_TASK_CNT_ON_THREAD) {
				mutex_unlock(&mdp_thread_mutex);
				CMDQ_ERR("%s drop new task since there will be more than %d\n",
					__func__, CMDQ_MAX_TASK_CNT_ON_THREAD);
				break;
			}
		}
#endif

		handle->thread = cmdq_mdp_find_free_thread(handle);
		if (handle->thread == CMDQ_INVALID_THREAD) {
			/* no available thread, keep wait */
			mutex_unlock(&mdp_thread_mutex);
			CMDQ_MSG(
				"fail to get thread handle:0x%p engine:0x%llx\n",
				handle, handle->engineFlag);
			continue;
		}

		if (handle->thread < 0) {
			CMDQ_ERR("%s handle->thread:%d is invalid!!!\n",
				__func__, handle->thread);
		}

		/* lock thread for counting and clk */
		cmdq_mdp_lock_thread(handle);
		mutex_unlock(&mdp_thread_mutex);

		/* remove from list */
		list_del_init(&handle->list_entry);

		CMDQ_MSG(
			"%s dispatch thread:%d for handle:0x%p engine:0x%llx thread engine:0x%llx\n",
			__func__, handle->thread, handle,
			handle->engineFlag,
			handle->thread >= 0 ?
			mdp_ctx.thread[handle->thread].engine_flag : 0);

		/* callback task for tracked group */
		for (index = 0; index < cmdq_mdp_get_func()->getGroupMax();
			++index) {
			if (!callback[index].trackTask)
				continue;

			CMDQ_MSG("track task group %d with task:0x%p\n",
				index, handle);
			if (!cmdq_core_is_group_flag(index,
				handle->engineFlag))
				continue;
			CMDQ_MSG("track task group %d flag:0x%llx\n",
				index, handle->engineFlag);
			callback[index].trackTask(handle);
		}

		/* flush handle */
		err = cmdq_pkt_flush_async_ex(handle, 0, 0, false);
		if (err < 0) {
			/* change state so waiting thread may release it */
			CMDQ_ERR("fail to flush handle:0x%p\n",
				handle);
			continue;
		}

		/* some task is ready to run */
		acquired = true;
	}

	CMDQ_PROF_MMP(mdp_mmp_get_event()->consume_done, MMPROFILE_FLAG_END,
		current->pid, 0);

	mutex_unlock(&mdp_task_mutex);

	CMDQ_MSG("%s end acquired:%s\n", __func__, acquired ? "true" : "false");

	if (acquired) {
		/* notify some task's SW thread to change their waiting state.
		 * (if they already called cmdq_mdp_wait)
		 */
		wake_up_all(&mdp_thread_dispatch);
	}

	return 0;
}

static void cmdq_mdp_consume_wait_item(struct work_struct *ignore)
{
	s32 err = cmdq_mdp_consume_handle();

	if (err < 0)
		CMDQ_ERR("consume handle in worker fail:%d\n", err);
}

void cmdq_mdp_add_consume_item(void)
{
	if (!work_pending(&mdp_ctx.handle_consume_item)) {
		CMDQ_PROF_MMP(mdp_mmp_get_event()->consume_add,
			MMPROFILE_FLAG_PULSE, 0, 0);
		queue_work(mdp_ctx.handle_consume_queue,
			&mdp_ctx.handle_consume_item);
	}
}

static s32 cmdq_mdp_copy_cmd_to_task(struct cmdqRecStruct *handle,
	void *src, u32 size, bool user_space)
{
	return cmdq_pkt_copy_cmd(handle, src, size, user_space);
}

static void cmdq_mdp_store_debug(struct cmdqCommandStruct *desc,
	struct cmdqRecStruct *handle)
{
	s32 len;

	if (!desc->userDebugStr || !desc->userDebugStrLen)
		return;

	handle->user_debug_str = kzalloc(desc->userDebugStrLen + 1, GFP_KERNEL);
	if (!handle->user_debug_str) {
		CMDQ_ERR("allocate user debug memory failed, size:%d\n",
			desc->userDebugStrLen);
		return;
	}

	len = strncpy_from_user(handle->user_debug_str,
		(const char *)(unsigned long)desc->userDebugStr,
		desc->userDebugStrLen);
	if (len < 0) {
		CMDQ_ERR("copy user debug memory failed, size:%d\n",
			desc->userDebugStrLen);
		return;
	}

	CMDQ_MSG("user debug string:%s\n", handle->user_debug_str);
}

#ifdef CMDQ_SECURE_PATH_SUPPORT
#define CMDQ_ISP_MSG2(name) \
{ \
	.va = iwc_msg2->name, \
	.sz = &(iwc_msg2->name##_size), \
}

#define CMDQ_ISP_BUFS_MSG1(name) \
{ \
	.va = iwc_msg1->name, \
	.sz = &(iwc_msg1->name##_size), \
}

const u32 isp_iwc_buf_size[] = {
	CMDQ_SEC_ISP_CQ_SIZE,
	CMDQ_SEC_ISP_VIRT_SIZE,
	CMDQ_SEC_ISP_TILE_SIZE,
	CMDQ_SEC_ISP_BPCI_SIZE,
	CMDQ_SEC_ISP_LSCI_SIZE,
	CMDQ_SEC_ISP_LCEI_SIZE,
	CMDQ_SEC_ISP_DEPI_SIZE,
	CMDQ_SEC_ISP_DMGI_SIZE,
};

static void cmdq_mdp_fill_isp_meta(struct cmdqSecIspMeta *meta,
	struct iwc_cq_meta *iwc_msg1,
	struct iwc_cq_meta2 *iwc_msg2,
	bool from_userspace)
{
	u32 i;
	struct iwc_meta_buf {
		u32 *va;
		u32 *sz;
	} bufs[ARRAY_SIZE(meta->ispBufs)] = {
		CMDQ_ISP_BUFS_MSG1(isp_cq_desc),
		CMDQ_ISP_BUFS_MSG1(isp_cq_virt),
		CMDQ_ISP_BUFS_MSG1(isp_tile),
		CMDQ_ISP_BUFS_MSG1(isp_bpci),
		CMDQ_ISP_BUFS_MSG1(isp_lsci),
		CMDQ_ISP_MSG2(isp_lcei),
		CMDQ_ISP_BUFS_MSG1(isp_depi),
		CMDQ_ISP_BUFS_MSG1(isp_dmgi),
	};

	memcpy(&iwc_msg2->handles, meta, sizeof(iwc_msg2->handles));

	for (i = 0; i < ARRAY_SIZE(meta->ispBufs); i++) {
		if (!meta->ispBufs[i].va || !meta->ispBufs[i].size)
			continue;

		if (meta->ispBufs[i].size > isp_iwc_buf_size[i]) {
			CMDQ_ERR("isp buf %u size:%llu max:%u\n",
				i, meta->ispBufs[i].size,
				isp_iwc_buf_size[i]);
			*bufs[i].sz = 0;
			continue;
		}

		*bufs[i].sz = meta->ispBufs[i].size;
		if (from_userspace) {
			if (copy_from_user(bufs[i].va,
				CMDQ_U32_PTR(meta->ispBufs[i].va),
				meta->ispBufs[i].size))
				CMDQ_ERR("fail to copy ispBufs [%d]\n", i);
		} else
			memcpy(bufs[i].va,
				(void *)(unsigned long)(meta->ispBufs[i].va),
				meta->ispBufs[i].size);
	}
}
#endif

static s32 cmdq_mdp_setup_sec(struct cmdqCommandStruct *desc,
	struct cmdqRecStruct *handle)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	u64 dapc, port;
	enum cmdq_sec_meta_type meta_type = CMDQ_METAEX_NONE;
	struct cmdq_client *cl;

	if (!desc->secData.is_secure)
		return 0;

	dapc = cmdq_mdp_get_func()->mdpGetSecEngine(
		desc->secData.enginesNeedDAPC);
	port = cmdq_mdp_get_func()->mdpGetSecEngine(
		desc->secData.enginesNeedPortSecurity);

	cmdq_task_set_secure(handle, desc->secData.is_secure);

	/* force assign client, since backup cookie must call before flush,
	 * and it is necessary to know client first before append backup code.
	 */
	cl = cmdq_helper_mbox_client(handle->thread);
	if (unlikely(!cl)) {
		CMDQ_ERR("%s: secure client is invalid, thread:%d\n", __func__, handle->thread);
		return -EINVAL;
	}
	handle->pkt->cl = (void *)cl;
	handle->pkt->dev = cl->chan->mbox->dev;

	if (desc->secData.ispMeta.ispBufs[0].size) {
		handle->sec_isp_msg1 = vzalloc(sizeof(struct iwc_cq_meta));
		handle->sec_isp_msg2 = vzalloc(sizeof(struct iwc_cq_meta2));
		if (!handle->sec_isp_msg1 || !handle->sec_isp_msg2) {
			CMDQ_ERR("fail to alloc isp msg\n");
			vfree(handle->sec_isp_msg1);
			vfree(handle->sec_isp_msg2);
			return -ENOMEM;
		}
		cmdq_mdp_fill_isp_meta(&desc->secData.ispMeta,
			handle->sec_isp_msg1, handle->sec_isp_msg2, false);
		meta_type = CMDQ_METAEX_CQ;
		cmdq_sec_pkt_set_payload(handle->pkt, 1,
			sizeof(struct iwc_cq_meta), handle->sec_isp_msg1);
		cmdq_sec_pkt_set_payload(handle->pkt, 2,
			sizeof(struct iwc_cq_meta2), handle->sec_isp_msg2);
	}

	cmdq_sec_pkt_set_data(handle->pkt, dapc, port,
		CMDQ_SEC_USER_MDP, meta_type);

	if (desc->secData.addrMetadataCount >=
		CMDQ_IWC_MAX_ADDR_LIST_LENGTH) {
		CMDQ_ERR("addrMetadataCount %u reach the max %u\n",
			 desc->secData.addrMetadataCount,
			 CMDQ_IWC_MAX_ADDR_LIST_LENGTH);
		return -EFAULT;
	}

	cmdq_sec_pkt_assign_metadata(handle->pkt,
		desc->secData.addrMetadataCount,
		CMDQ_U32_PTR(desc->secData.addrMetadatas));

	if (handle->pkt->cmd_buf_size) {
		u32 cnt = handle->pkt->cmd_buf_size / CMDQ_INST_SIZE;
		struct cmdq_sec_data *data = handle->pkt->sec_data;
		struct iwcCmdqAddrMetadata_t *addr =
			(struct iwcCmdqAddrMetadata_t *)
			(unsigned long)data->addrMetadatas;
		const u32 max_inst = CMDQ_CMD_BUFFER_SIZE / CMDQ_INST_SIZE - 1;
		u32 i;

		for (i = 0; i < data->addrMetadataCount; i++) {
			u32 idx = addr[i].instrIndex;

			addr[i].instrIndex += cnt;
			/* adjumst for buffer jump */
			addr[i].instrIndex += addr[i].instrIndex / max_inst;

			CMDQ_MSG("meta index change from:%u to:%u\n",
				idx, addr[i].instrIndex);
		}
	}
#endif
	return 0;
}

s32 cmdq_mdp_handle_create(struct cmdqRecStruct **handle_out)
{
	struct cmdqRecStruct *handle = NULL;
	s32 status;

	status = cmdq_task_create(CMDQ_SCENARIO_USER_MDP, &handle);
	if (status < 0) {
		CMDQ_ERR("%s task create fail: %d\n", __func__, status);
		return status;
	}

	handle->pkt->cur_pool.pool = mdp_pool.pool;
	handle->pkt->cur_pool.cnt = mdp_pool.cnt;
	handle->pkt->cur_pool.limit = mdp_pool.limit;

	/* assign handle for mdp */
	*handle_out = handle;

	return 0;
}

#if defined(CMDQ_SECURE_PATH_SUPPORT)
static void cmdq_mdp_config_readback_sec(struct cmdqRecStruct *handle)
{
	struct cmdq_sec_data *data =
		(struct cmdq_sec_data *)handle->pkt->sec_data;
	u32 i;

	data->mdp_extension = handle->mdp_extension;
	data->readback_cnt = handle->readback_cnt;

	CMDQ_MSG("%s engine:%llx, mdp_extension:%d, readback_cnt:%d\n", __func__,
		handle->engineFlag,
		handle->mdp_extension,
		handle->readback_cnt);

	for (i = 0; i < handle->readback_cnt; i++) {
		data->readback_engs[i].engine =
			handle->readback_engs[i].engine;
		data->readback_engs[i].start = handle->readback_engs[i].start;
		data->readback_engs[i].count = handle->readback_engs[i].count;
		data->readback_engs[i].param = handle->readback_engs[i].param;

		CMDQ_MSG("%s idx:%u offset:%#x(%u) engine:%u param:%#x\n",
			__func__, i, data->readback_engs[i].start,
			data->readback_engs[i].count,
			data->readback_engs[i].engine,
			data->readback_engs[i].param);
	}
}

s32 cmdq_mdp_config_readback_thread(struct cmdqRecStruct *handle)
{
	s32 err;
	struct cmdq_client *cl_rb = NULL;

	if (!handle)
		return 0;

	/* Assign static normal thread */
	if (handle->scenario == CMDQ_SCENARIO_USER_MDP &&
		handle->secData.is_secure)  {
		handle->thread_rb = mdp_ctx.pq_readback.rb_thread_id;
	}

	CMDQ_MSG("%s engine:%llx, handle->thread_rb:%d, readback_cnt:%d\n", __func__,
		handle->engineFlag,
		handle->thread_rb,
		handle->readback_cnt);

	if (handle->thread_rb != CMDQ_INVALID_THREAD) {
		cl_rb = cmdq_helper_mbox_client(handle->thread_rb);
		handle->pkt_rb = cmdq_pkt_create(cl_rb);

		if (IS_ERR(handle->pkt_rb)) {
			err = PTR_ERR(handle->pkt_rb);
			CMDQ_ERR("creat pkt_rb fail err:%d\n", err);
			handle->pkt_rb = NULL;
			return err;
		}
		handle->pkt_rb->cl = (void *)cl_rb;
	}
	return 0;
}
#endif

s32 cmdq_mdp_handle_sec_setup(struct cmdqSecDataStruct *secData,
			struct cmdqRecStruct *handle)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	u64 dapc, port;
	enum cmdq_sec_meta_type meta_type = CMDQ_METAEX_NONE;
	void *user_addr_meta = NULL;
	void *addr_meta = NULL;
	u32 addr_meta_size;
	struct cmdq_client *cl = NULL;
	bool is_sec_meta_data_support;

	/* set secure data */
	handle->secStatus = NULL;
	if (!secData || !secData->is_secure)
		return 0;

	is_sec_meta_data_support =
		cmdq_mdp_get_func()->mdpSvpSupportMetaData();

	CMDQ_MSG("%s start:%d, %d, %d\n",
		__func__, secData->is_secure,
		secData->addrMetadataCount, is_sec_meta_data_support);

	if (is_sec_meta_data_support && !secData->addrMetadataCount) {
		CMDQ_ERR(
			"[secData]mismatch is_secure %d and addrMetadataCount %d\n",
			secData->is_secure,
			secData->addrMetadataCount);
		return -EINVAL;
	}

	if (secData->addrMetadataCount >= CMDQ_IWC_MAX_ADDR_LIST_LENGTH) {
		CMDQ_ERR(
			"[secData]addrMetadataCount %u reach the max %u\n",
			 secData->addrMetadataCount, CMDQ_IWC_MAX_ADDR_LIST_LENGTH);
		return -EINVAL;
	}
	user_addr_meta = CMDQ_U32_PTR(secData->addrMetadatas);

	dapc = cmdq_mdp_get_func()->mdpGetSecEngine(
		secData->enginesNeedDAPC);
	port = cmdq_mdp_get_func()->mdpGetSecEngine(
		secData->enginesNeedPortSecurity);

	cmdq_task_set_secure(handle, secData->is_secure);

	/* force assign client, since backup cookie must call before flush,
	 * and it is necessary to know client first before append backup code.
	 */
	cl = cmdq_helper_mbox_client(handle->thread);
	if (unlikely(!cl)) {
		CMDQ_ERR("%s: secure client is invalid, thread:%d\n", __func__, handle->thread);
		return -EINVAL;
	}
	handle->pkt->cl = (void *)cl;
	handle->pkt->dev = cl->chan->mbox->dev;

	if (secData->ispMeta.ispBufs[0].size) {
		handle->sec_isp_msg1 = vzalloc(sizeof(struct iwc_cq_meta));
		handle->sec_isp_msg2 = vzalloc(sizeof(struct iwc_cq_meta2));
		if (!handle->sec_isp_msg1 || !handle->sec_isp_msg2) {
			CMDQ_ERR("fail to alloc isp msg\n");
			vfree(handle->sec_isp_msg1);
			vfree(handle->sec_isp_msg2);
			return -ENOMEM;
		}
		if (is_sec_meta_data_support)
			cmdq_mdp_cmdqSecIspMeta_fd_to_handle(&secData->ispMeta);
		cmdq_mdp_fill_isp_meta(&secData->ispMeta,
			handle->sec_isp_msg1, handle->sec_isp_msg2, true);
		meta_type = CMDQ_METAEX_CQ;
		cmdq_sec_pkt_set_payload(handle->pkt, 1,
			sizeof(struct iwc_cq_meta), handle->sec_isp_msg1);
		cmdq_sec_pkt_set_payload(handle->pkt, 2,
			sizeof(struct iwc_cq_meta2), handle->sec_isp_msg2);
	}

	cmdq_sec_pkt_set_data(handle->pkt, dapc, port,
		CMDQ_SEC_USER_MDP, meta_type);

	if (is_sec_meta_data_support) {
		addr_meta_size = secData->addrMetadataCount *
			sizeof(struct cmdqSecAddrMetadataStruct);
		addr_meta = kmalloc(addr_meta_size, GFP_KERNEL);
		if (!addr_meta) {
			CMDQ_ERR("%s: allocate size fail:%u\n",
				__func__, addr_meta_size);
			return -ENOMEM;
		}

		if (copy_from_user(addr_meta, user_addr_meta, addr_meta_size)) {
			CMDQ_ERR("%s: fail to copy user addr meta\n", __func__);
			kfree(addr_meta);
			return -EFAULT;
		}
		cmdq_mdp_init_secure_id(addr_meta, secData->addrMetadataCount);
		cmdq_sec_pkt_assign_metadata(handle->pkt,
			secData->addrMetadataCount,
			addr_meta);
	}

	cmdq_sec_pkt_set_mtee(handle->pkt,
		cmdq_mdp_get_func()->mdpIsMtee(handle));

	/* config handle->pkt_rb and handle->thread_rb */
	cmdq_mdp_config_readback_thread(handle);

	CMDQ_MSG("%s done, handle:%p mtee:%d dapc:%#llx port:%#llx engine:%#llx\n",
		__func__, handle,
		((struct cmdq_sec_data *)handle->pkt->sec_data)->mtee,
		dapc, port, handle->engineFlag);

	kfree(addr_meta);
	return 0;
#else
	return 0;
#endif
}

void cmdq_mdp_cmdqSecIspMeta_fd_to_handle(struct cmdqSecIspMeta *ispMeta)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	struct dma_buf *buf;

	if (ispMeta->BpciHandle) {
		buf = dma_buf_get(ispMeta->BpciHandle);
		ispMeta->BpciHandle = dmabuf_to_secure_handle(buf);
		dma_buf_put(buf);
	}
	if (ispMeta->LsciHandle) {
		buf = dma_buf_get(ispMeta->LsciHandle);
		ispMeta->LsciHandle = dmabuf_to_secure_handle(buf);
		dma_buf_put(buf);
	}
	if (ispMeta->LceiHandle) {
		buf = dma_buf_get(ispMeta->LceiHandle);
		ispMeta->LceiHandle = dmabuf_to_secure_handle(buf);
		dma_buf_put(buf);
	}
	if (ispMeta->DepiHandle) {
		buf = dma_buf_get(ispMeta->DepiHandle);
		ispMeta->DepiHandle = dmabuf_to_secure_handle(buf);
		dma_buf_put(buf);
	}
	if (ispMeta->DmgiHandle) {
		buf = dma_buf_get(ispMeta->DmgiHandle);
		ispMeta->DmgiHandle = dmabuf_to_secure_handle(buf);
		dma_buf_put(buf);
	}
#endif
}

void cmdq_mdp_init_secure_id(void *meta_array, u32 count)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	u32 i;
	struct dma_buf *buf = NULL;
	uint32_t sec_id = 0;
	uint32_t sec_handle = 0;
	struct cmdqSecAddrMetadataStruct *secMetadatas =
			(struct cmdqSecAddrMetadataStruct *)meta_array;

	for (i = 0; i < count; i++) {
		secMetadatas[i].useSecIdinMeta = 1;
		if (secMetadatas[i].baseHandle <= 0) {
			secMetadatas[i].sec_id = 0;
			continue;
		}

		buf = dma_buf_get(secMetadatas[i].baseHandle);
		sec_id = dmabuf_to_sec_id(buf, &sec_handle);
		CMDQ_MSG("%s,port:%d,baseHandle:%#llx,sec_id:%d,sec_handle:%#x",
				__func__, secMetadatas[i].port,
				secMetadatas[i].baseHandle,
				sec_id,
				sec_handle);
		secMetadatas[i].baseHandle = (uint64_t)sec_handle;
		secMetadatas[i].sec_id = sec_id;
		dma_buf_put(buf);
	}
#endif
}

s32 cmdq_mdp_update_sec_addr_index(struct cmdqRecStruct *handle,
	u32 sec_handle, u32 index, u32 instr_index)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	struct cmdq_sec_data *data = handle->pkt->sec_data;
	struct iwcCmdqAddrMetadata_t *addr = NULL;

	if (!data) {
		CMDQ_ERR("%s invalid index %d, pkt no sec\n", __func__, index);
		return -EINVAL;
	}
	if (index >= data->addrMetadataCount) {
		CMDQ_ERR("%s invalid index %d >= %d\n", __func__,
			index, data->addrMetadataCount);
		return -EINVAL;
	}
	addr = (struct iwcCmdqAddrMetadata_t *)
		(unsigned long)data->addrMetadatas;
	addr[index].instrIndex = instr_index;
	CMDQ_MSG("%s update %x[%d] to:%d\n", __func__,
		sec_handle, index, instr_index);
#endif
	return 0;
}

u32 cmdq_mdp_handle_get_instr_count(struct cmdqRecStruct *handle)
{
	return handle->pkt->cmd_buf_size / CMDQ_INST_SIZE;
}

s32 cmdq_mdp_handle_flush(struct cmdqRecStruct *handle)
{
	s32 status;

	CMDQ_TRACE_FORCE_BEGIN("%s %llx\n", __func__, handle->engineFlag);

#if defined(CMDQ_SECURE_PATH_SUPPORT)
	if (handle->secData.is_secure) {
		/* insert backup cookie cmd */
		handle->thread = CMDQ_INVALID_THREAD;

		/* Passing readback required data */
		cmdq_mdp_config_readback_sec(handle);
	}
#endif

	/* finalize it */
	CMDQ_MSG("%s finalize\n", __func__);
	handle->finalized = true;

	cmdq_pkt_finalize(handle->pkt);

	/* Dispatch handle to get correct thread or wait in list.
	 * Task may flush directly if no engine conflict and no waiting task
	 * holds same engines.
	 */
	CMDQ_MSG("%s flush impl\n", __func__);

	status = cmdq_mdp_flush_async_impl(handle);
	CMDQ_TRACE_FORCE_END();

	return status;
}

void cmdq_mdp_op_readback(struct cmdqRecStruct *handle, u16 engine,
	dma_addr_t addr, u32 param)
{
	mdp_funcs.mdpComposeReadback(handle, engine, addr, param);
}

s32 cmdq_mdp_flush_async(struct cmdqCommandStruct *desc, bool user_space,
	struct cmdqRecStruct **handle_out)
{
	struct cmdqRecStruct *handle;
	struct task_private *private;
	s32 err;
	u32 copy_size;

	CMDQ_TRACE_FORCE_BEGIN("%s %llx\n",
		__func__, desc->engineFlag);

	cmdq_task_create(desc->scenario, &handle);
	/* force assign buffer pool since mdp task assign clients later
	 * but allocate instruction buffer before do it.
	 */
	handle->pkt->cur_pool.pool = mdp_pool.pool;
	handle->pkt->cur_pool.cnt = mdp_pool.cnt;
	handle->pkt->cur_pool.limit = mdp_pool.limit;

	/* set secure data */
	handle->secStatus = NULL;
	cmdq_mdp_setup_sec(desc, handle);

	handle->pkt->priority = desc->priority;
	cmdq_mdp_store_debug(desc, handle);

	private = (struct task_private *)CMDQ_U32_PTR(desc->privateData);
	if (private)
		handle->node_private = private->node_private_data;

	if (desc->prop_size && desc->prop_addr &&
		desc->prop_size < CMDQ_MAX_USER_PROP_SIZE) {
		handle->prop_addr = kzalloc(desc->prop_size, GFP_KERNEL);
		memcpy(handle->prop_addr, (void *)CMDQ_U32_PTR(desc->prop_addr),
			desc->prop_size);
		handle->prop_size = desc->prop_size;
	} else {
		handle->prop_addr = NULL;
		handle->prop_size = 0;
	}

	CMDQ_SYSTRACE_BEGIN("%s copy command\n", __func__);
	copy_size = desc->blockSize - 2 * CMDQ_INST_SIZE;
	if (copy_size > 0) {
		err = cmdq_mdp_copy_cmd_to_task(handle,
			(void *)(unsigned long)desc->pVABase,
			copy_size, user_space);
		if (err < 0)
			goto flush_err_end;
	}
	CMDQ_SYSTRACE_END();

	CMDQ_SYSTRACE_BEGIN("%s check valid %u\n", __func__, copy_size);
	if (user_space && !cmdq_core_check_user_valid(
		(void *)(unsigned long)desc->pVABase, copy_size)) {
		CMDQ_SYSTRACE_END();
		err = -EFAULT;
		goto flush_err_end;
	}
	CMDQ_SYSTRACE_END();

	if (desc->regRequest.count &&
			desc->regRequest.count <= CMDQ_MAX_DUMP_REG_COUNT &&
			desc->regRequest.regAddresses) {
		err = cmdq_task_append_backup_reg(handle,
			desc->regRequest.count,
			(u32 *)(unsigned long)desc->regRequest.regAddresses);
		if (err < 0)
			goto flush_err_end;
	}

#ifdef CMDQ_SECURE_PATH_SUPPORT
	if (handle->secData.is_secure) {
		/* insert backup cookie cmd */
		cmdq_sec_insert_backup_cookie(handle->pkt);
		handle->thread = CMDQ_INVALID_THREAD;
	}
#endif

	CMDQ_SYSTRACE_BEGIN("%s copy cmd\n", __func__);
	err = cmdq_mdp_copy_cmd_to_task(handle,
		(void *)(unsigned long)desc->pVABase + copy_size,
		2 * CMDQ_INST_SIZE, user_space);
	if (err < 0) {
		CMDQ_SYSTRACE_END();
		goto flush_err_end;
	}
	CMDQ_SYSTRACE_END();

	/* mark finalized since we copy it */
	handle->finalized = true;

	/* assign handle for mdp */
	*handle_out = handle;

	/* Dispatch handle to get correct thread or wait in list.
	 * Task may flush directly if no engine conflict and no waiting task
	 * holds same engines.
	 */
	err = cmdq_mdp_flush_async_impl(handle);
	CMDQ_TRACE_FORCE_END();
	return 0;

flush_err_end:
	CMDQ_TRACE_FORCE_END();
	cmdq_task_destroy(handle);

	return err;
}

s32 cmdq_mdp_flush_async_impl(struct cmdqRecStruct *handle)
{
	struct list_head *insert_pos = &mdp_ctx.tasks_wait;
	struct cmdqRecStruct *entry;

	CMDQ_MSG("dispatch handle:0x%p\n", handle);

	/* set handle life cycle callback */
	handle->prepare = cmdq_mdp_handle_prepare;
	handle->unprepare = cmdq_mdp_handle_unprepare;
	handle->stop = cmdq_mdp_handle_stop;

	/* lock resource to make sure task own it after dispatch to hw */
	cmdq_mdp_lock_resource(handle->engineFlag, false);

	/* change state to waiting before insert to prevent
	 * other thread consume immediately
	 */
	handle->state = TASK_STATE_WAITING;

	/* assign handle into waiting list by priority */
	CMDQ_MSG("assign handle into waiting list:0x%p\n", handle);
	mutex_lock(&mdp_task_mutex);
	list_for_each_entry(entry, &mdp_ctx.tasks_wait, list_entry) {
		if (entry->pkt->priority < handle->pkt->priority)
			break;
		insert_pos = &entry->list_entry;
	}
	list_add(&handle->list_entry, insert_pos);
	mutex_unlock(&mdp_task_mutex);

	/* run consume to run task in thread */
	CMDQ_MSG("cmdq_mdp_consume_handle:0x%p\n", handle);
	cmdq_mdp_consume_handle();

	return 0;
}

struct cmdqRecStruct *cmdq_mdp_get_valid_handle(unsigned long job)
{
	struct cmdqRecStruct *handle = NULL, *entry;

	mutex_lock(&mdp_task_mutex);
	list_for_each_entry(entry, &mdp_ctx.tasks_wait, list_entry) {
		if ((void *)job == entry) {
			handle = entry;
			break;
		}
	}

	if (!handle)
		handle = cmdq_core_get_valid_handle(job);

	mutex_unlock(&mdp_task_mutex);

	return handle;
}

s32 cmdq_mdp_wait(struct cmdqRecStruct *handle,
	struct cmdqRegValueStruct *results)
{
	s32 status, waitq;
	u32 i;
	u64 exec_cost;

	CMDQ_TRACE_FORCE_BEGIN("%s %d %llx\n",
		__func__, handle->thread, handle->engineFlag);

	/* we have to wait handle has valid thread first */
	if (handle->thread == CMDQ_INVALID_THREAD) {
		CMDQ_LOG("pid:%d handle:0x%p wait for valid thread first\n",
			current->pid, handle);

		/* wait for acquire thread
		 * (this is done by cmdq_mdp_consume_handle
		 */
		waitq = wait_event_timeout(mdp_thread_dispatch,
			(handle->thread != CMDQ_INVALID_THREAD),
			msecs_to_jiffies(CMDQ_ACQUIRE_THREAD_TIMEOUT_MS));

		if (waitq == 0 || handle->thread == CMDQ_INVALID_THREAD) {
			mutex_lock(&mdp_task_mutex);
			/* it's possible that the task was just consumed now.
			 * so check again.
			 */
			if (handle->thread == CMDQ_INVALID_THREAD) {
				CMDQ_ERR(
					"handle 0x%p timeout with invalid thread\n",
					handle);
				/* remove from waiting list,
				 * so that it won't be consumed in the future
				 */
				list_del_init(&handle->list_entry);
				mutex_unlock(&mdp_task_mutex);
				CMDQ_TRACE_FORCE_END();
				return -ETIMEDOUT;
			}
			/* valid thread, so we keep going */
			mutex_unlock(&mdp_task_mutex);
		}
	}

	CMDQ_MSG("%s wait handle:0x%p thread:%d\n",
		__func__, handle, handle->thread);

	/* wait handle flush done */
	exec_cost = sched_clock();
	status = cmdq_pkt_wait_flush_ex_result(handle);
	exec_cost = div_s64(sched_clock() - exec_cost, 1000);
	if (exec_cost > 150000)
		CMDQ_LOG("[warn]wait flush result cost:%lluus handle:0x%p\n",
			exec_cost, handle);

	if (results && results->count &&
		results->count <= CMDQ_MAX_DUMP_REG_COUNT) {
		CMDQ_SYSTRACE_BEGIN("%s assign regs\n", __func__);
		/* clear results */
		memset(CMDQ_U32_PTR(results->regValues), 0,
			results->count * sizeof(CMDQ_U32_PTR(
			results->regValues)[0]));

		mutex_lock(&mdp_task_mutex);
		for (i = 0; i < results->count && i < handle->reg_count; i++)
			CMDQ_U32_PTR(results->regValues)[i] =
				handle->reg_values[i];
		mutex_unlock(&mdp_task_mutex);
		CMDQ_SYSTRACE_END();
	}

	/* consume again since maybe more conflict task in waiting */
	cmdq_mdp_add_consume_item();

	CMDQ_TRACE_FORCE_END();

	return status;
}

s32 cmdq_mdp_flush(struct cmdqCommandStruct *desc, bool user_space)
{
	struct cmdqRecStruct *handle = NULL;
	s32 status;

	status = cmdq_mdp_flush_async(desc, user_space, &handle);
	if (!handle || status < 0) {
		CMDQ_ERR("mdp flush async failed:%d\n", status);
		return status;
	}

	status = cmdq_mdp_wait(handle, &desc->regValue);
	if (status < 0)
		CMDQ_ERR("mdp flush wait failed:%d handle:0x%p thread:%d\n",
			status, handle, handle->thread);
	cmdq_task_destroy(handle);

	return status;
}

static void cmdq_mdp_pool_create(void)
{
	if (unlikely(mdp_pool.pool)) {
		cmdq_msg("mdp buffer pool already created");
		return;
	}

	if (unlikely(!cmdq_mbox_dev_get())) {
		CMDQ_ERR("%s mbox device is not exist\n", __func__);
		return;
	}

	mdp_pool.pool = dma_pool_create("mdp", cmdq_mbox_dev_get(),
		CMDQ_BUF_ALLOC_SIZE, 0, 0);
	atomic_set(mdp_pool.cnt, 0);
}

static void cmdq_mdp_pool_clear(void)
{
	/* check pool still in use */
	if (unlikely((atomic_read(mdp_pool.cnt)))) {
		cmdq_msg("mdp buffers still in use:%d",
			atomic_read(mdp_pool.cnt));
		return;
	}

	dma_pool_destroy(mdp_pool.pool);
	mdp_pool.pool = NULL;
}

void cmdq_mdp_suspend(void)
{
	if (atomic_read(&mdp_ctx.mdp_smi_usage)) {
		CMDQ_ERR("%s smi clk usage:%d\n",
			__func__, (s32)atomic_read(&mdp_ctx.mdp_smi_usage));
		cmdq_mdp_dump_thread_usage();
		cmdq_mdp_get_func()->mdpDumpEngineUsage(
			(struct EngineStruct *)mdp_ctx.engine);
	}

	cmdq_mdp_pool_clear();
}

void cmdq_mdp_resume(void)
{
	cmdq_mdp_pool_create();

	/* during suspending, there may be queued tasks.
	 * we should process them if any.
	 */
	cmdq_mdp_add_consume_item();
}

void cmdq_mdp_release_task_by_file_node(void *file_node)
{
	struct cmdqRecStruct *handle, *temp;

	/* Since the file node is closed, there is no way
	 * user space can issue further "wait_and_close" request,
	 * so we must auto-release running/waiting tasks
	 * to prevent resource leakage
	 */

	/* walk through active and waiting lists and release them */
	mutex_lock(&mdp_task_mutex);

	list_for_each_entry_safe(handle, temp, &mdp_ctx.tasks_wait,
		list_entry) {
		if (handle->node_private != file_node)
			continue;
		CMDQ_LOG(
			"[warn]waiting handle 0x%p release because file node 0x%p closed\n",
			handle, file_node);

		/* since we already inside mutex,
		 * and these WAITING tasks will not be consumed
		 * (acquire thread / exec)
		 * we can release them directly.
		 * note that we use unlocked version since we already
		 * hold mdp_task_mutex.
		 */
		list_del_init(&handle->list_entry);
		cmdq_task_destroy(handle);
	}

	/* ask core to auto release by file node
	 * note the core may lock more mutex
	 */
	cmdq_core_release_handle_by_file_node(file_node);

	mutex_unlock(&mdp_task_mutex);
}

void cmdq_mdp_dump_thread_usage(void)
{
	int index;

	CMDQ_ERR("====== MDP Threaed usage =======\n");
	for (index = 0; index < ARRAY_SIZE(mdp_ctx.thread); index++) {
		if (!mdp_ctx.thread[index].acquired)
			continue;
		CMDQ_ERR(
			"thread:%d task cnt:%u engine flag:%#llx allow dispatch:%s\n",
			index, mdp_ctx.thread[index].task_count,
			mdp_ctx.thread[index].engine_flag,
			mdp_ctx.thread[index].allow_dispatch ?
			"true" : "false");
	}
}



void cmdq_mdp_dump_resource(u32 event)
{
	struct ResourceUnitStruct *resource = NULL;

	mutex_lock(&mdp_resource_mutex);
	list_for_each_entry(resource, &mdp_ctx.resource_list, list_entry) {
		if (event != resource->lockEvent)
			continue;
		CMDQ_ERR("[Res] Dump resource with event:%d\n",
			resource->lockEvent);
		CMDQ_ERR("[Res]   notify:%llu delay:%lld\n",
			resource->notify, resource->delay);
		CMDQ_ERR("[Res]   lock:%llu unlock:%lld\n",
			resource->lock, resource->unlock);
		CMDQ_ERR("[Res]   acquire:%llu release:%lld\n",
			resource->acquire, resource->release);
		CMDQ_ERR("[Res]   isUsed:%d isLend:%d isDelay:%d\n",
			resource->used, resource->lend,
			resource->delaying);
		if (!resource->releaseCB)
			CMDQ_ERR("[Res] release CB func is NULL\n");
		break;
	}
	mutex_unlock(&mdp_resource_mutex);
}

static s32 cmdq_mdp_dump_common(u64 engineFlag, int level)
{
	cmdq_mdp_dump_thread_usage();
	cmdq_mdp_get_func()->mdpDumpEngineUsage(mdp_ctx.engine);

	return cmdq_mdp_get_func()->mdpDumpInfo(engineFlag, level);
}

static void cmdq_mdp_dump_resource_in_status(struct seq_file *m)
{
	struct ResourceUnitStruct *resource = NULL;

	if (!m)
		return;

	mutex_lock(&mdp_resource_mutex);
	list_for_each_entry(resource, &mdp_ctx.resource_list, list_entry) {
		seq_printf(m, "[Res] Dump resource with event:%d\n",
			resource->lockEvent);
		seq_printf(m, "[Res]   notify:%llu delay:%lld\n",
			resource->notify, resource->delay);
		seq_printf(m, "[Res]   lock:%llu unlock:%lld\n",
			resource->lock, resource->unlock);
		seq_printf(m, "[Res]   acquire:%llu release:%lld\n",
			resource->acquire, resource->release);
		seq_printf(m, "[Res]   isUsed:%d isLend:%d isDelay:%d\n",
			resource->used, resource->lend,
			resource->delaying);
		if (!resource->releaseCB)
			seq_puts(m, "[Res] release CB func is NULL\n");
	}
	mutex_unlock(&mdp_resource_mutex);
}

int cmdq_mdp_status_dump(struct notifier_block *nb,
	unsigned long action, void *data)
{
	struct seq_file *m = (struct seq_file *)data;

	cmdq_mdp_dump_resource_in_status(m);

	return 0;
}

static void mdp_parse_opp(struct platform_device *pdev, const char *ref,
	int opp_num, u64 **freq_out, int **volt_out)
{
	struct device_node *np, *child_np = NULL;
	u64 *speeds;
	u64 freq;
	u32 index = 0;
	int volt, *volts;

	np = of_parse_phandle(pdev->dev.of_node, ref, 0);
	if (!np) {
		CMDQ_ERR("%s fail to parse opp:%s\n", __func__, ref);
		return;
	}

	speeds = kcalloc(opp_num, sizeof(u64), GFP_KERNEL);
	if (!speeds) {
		CMDQ_ERR("%s alloc freq array fail and skip\n", __func__);
		return;
	}

	volts = kcalloc(opp_num, sizeof(u64), GFP_KERNEL);
	if (!volts) {
		CMDQ_ERR("%s alloc volt array fail and skip\n", __func__);
		kfree(speeds);
		return;
	}

	do {
		child_np = of_get_next_available_child(np, child_np);
		if (!child_np)
			break;

		/* available freq is stored in speeds[index] */
		of_property_read_u64(child_np, "opp-hz", &freq);
		do_div(freq, 1000000);
		speeds[index] = freq;

		/* available voltage is stored in volts[i]*/
		of_property_read_u32(child_np, "opp-microvolt", &volt);
		volts[index] = volt;

		CMDQ_LOG("%s %u: %llu(Mhz) %d\n", ref, index, speeds[index], volts[index]);

		index++;
	} while (index < opp_num);

	of_node_put(np);

	*freq_out = speeds;
	*volt_out = volts;
}

static void cmdq_mdp_init_pmqos(struct platform_device *pdev)
{
	u32 i = 0;

	for (i = 0; i < CMDQ_MAX_THREAD_COUNT; i++) {
		if (!cmdq_helper_mbox_client(i))
			continue;
		if (i < MDP_THREAD_START) {
			CMDQ_LOG("[warn]mdp uses thread:%u but qos start:%u\n",
				i, MDP_THREAD_START);
			continue;
		}
		cmdq_mdp_get_func()->qosInit(pdev, i - MDP_THREAD_START);
	}

	/* Create opp table from dts */
	CMDQ_LOG("%s Create opp table : mdp-opp\n", __func__);
	dev_pm_opp_of_add_table_indexed(&pdev->dev, 0);

	if (cmdq_mdp_get_func()->mdpIsCaminSupport()) {
		CMDQ_LOG("%s Create opp table : isp-opp\n", __func__);
		dev_pm_opp_of_add_table_indexed(&pdev->dev, 1);
	}

	/* number of available opp */
	mdp_pmqos_opp_num = dev_pm_opp_get_opp_count(&pdev->dev);
	CMDQ_LOG("%s opp count:%d\n", __func__, mdp_pmqos_opp_num);

	if (mdp_pmqos_opp_num > 0) {
		mdp_parse_opp(pdev, "mdp-opp", mdp_pmqos_opp_num,
			&mdp_pmqos_freq, &mdp_volts);

		if (cmdq_mdp_get_func()->mdpIsCaminSupport())
			mdp_parse_opp(pdev, "isp-opp", mdp_pmqos_opp_num,
				&isp_pmqos_freq, &isp_volts);
	}

	/* Get regulator instance by name */
	mdp_mmdvfs_reg = devm_regulator_get(&pdev->dev, "mdp-dvfsrc-vcore");

	if (cmdq_mdp_get_func()->mdpIsCaminSupport())
		isp_mmdvfs_reg = devm_regulator_get(&pdev->dev, "isp-dvfsrc-vcore");

}

static int cmdq_mdp_init_larb(struct platform_device *pdev)
{
	struct device_node *node;
	struct platform_device *larb_pdev;

	/* get larb node from dts */
	node = of_parse_phandle(pdev->dev.of_node, "mediatek,larb", 0);
	if (!node) {
		CMDQ_ERR("%s fail to parse mediatek,larb\n", __func__);
		return -EINVAL;
	}

	larb_pdev = of_find_device_by_node(node);
	if (WARN_ON(!larb_pdev)) {
		of_node_put(node);
		CMDQ_ERR("%s no larb support\n", __func__);
		return -EINVAL;
	}
	of_node_put(node);

	mdp_ctx.larb = &larb_pdev->dev;

	CMDQ_LOG("%s success\n", __func__);

	return 0;
}

static int cmdq_mdp_init_pq_readback(struct platform_device *pdev)
{
	int ret;
	u16 hist_sram_start = 0;
	u16 rb_thread_id = 0, rb_event_lock = 0, rb_event_unlock = 0;

	ret = of_property_read_u16(pdev->dev.of_node,
		"dre30_hist_sram_start", &hist_sram_start);
	if (ret != 0 || !hist_sram_start)
		hist_sram_start = LEGACY_DRE30_HIST_SRAM_START;

	mdp_ctx.pq_readback.dre30_hist_sram_start = hist_sram_start;

#if defined(CMDQ_SECURE_PATH_SUPPORT)
	ret = of_property_read_u16(pdev->dev.of_node,
		"pq_rb_thread_id", &rb_thread_id);
	if (ret != 0)
		CMDQ_MSG("pq_rb_thread_id is not defined\n");
	mdp_ctx.pq_readback.rb_thread_id = rb_thread_id;

	ret = of_property_read_u16(pdev->dev.of_node,
		"pq_rb_event_lock", &rb_event_lock);
	if (ret != 0)
		CMDQ_MSG("pq_rb_event_lock is not defined\n");
	mdp_ctx.pq_readback.rb_event_lock = rb_event_lock;

	ret = of_property_read_u16(pdev->dev.of_node,
		"pq_rb_event_unlock", &rb_event_unlock);
	if (ret != 0)
		CMDQ_MSG("pq_rb_event_unlock is not defined\n");
	mdp_ctx.pq_readback.rb_event_unlock = rb_event_unlock;
#endif

	CMDQ_LOG("%s thd:%d, hist:%d, lock:%d, unlock:%d\n", __func__,
		rb_thread_id, hist_sram_start, rb_event_lock, rb_event_unlock);

	return 0;
}

void cmdq_mdp_init(struct platform_device *pdev)
{
	struct cmdqMDPFuncStruct *mdp_func = cmdq_mdp_get_func();

	CMDQ_LOG("%s ++\n", __func__);

	/* Register MDP callback */
	cmdqCoreRegisterCB(mdp_func->getGroupMdp(), cmdq_mdp_clock_enable,
		cmdq_mdp_dump_common, mdp_func->mdpResetEng,
		cmdq_mdp_clock_disable);

	cmdqCoreRegisterErrorResetCB(mdp_func->getGroupMdp(),
		mdp_func->errorReset);

	/* Register module dispatch callback */
	cmdqCoreRegisterDispatchModCB(mdp_func->getGroupMdp(),
		mdp_func->dispatchModule);

	/* Register restore task */
	cmdqCoreRegisterTrackTaskCB(mdp_func->getGroupMdp(),
		mdp_func->trackTask);

	init_waitqueue_head(&mdp_thread_dispatch);

	/* some fields has non-zero initial value */
	cmdq_mdp_reset_engine_struct();
	cmdq_mdp_reset_thread_struct();

	mdp_ctx.resource_check_queue =
		create_singlethread_workqueue("cmdq_resource");
	INIT_LIST_HEAD(&mdp_ctx.tasks_wait);
	INIT_LIST_HEAD(&mdp_ctx.resource_list);
	INIT_WORK(&mdp_ctx.handle_consume_item, cmdq_mdp_consume_wait_item);
	mdp_ctx.handle_consume_queue =
		create_singlethread_workqueue("cmdq_mdp_task");

	mdp_status_dump_notify.notifier_call = cmdq_mdp_status_dump;
	cmdq_core_register_status_dump(&mdp_status_dump_notify);

	/* Initialize Resource via device tree */
	cmdq_dev_init_resource(cmdq_mdp_init_resource);

	/* MDP initialization setting */
	cmdq_mdp_get_func()->mdpInitialSet(pdev);

	cmdq_mdp_init_pmqos(pdev);

	mdp_pool.limit = &mdp_pool_limit;
	mdp_pool.cnt = &mdp_pool_cnt;

	cmdq_mdp_pool_create();

	/* config pq readback setting from dts */
	cmdq_mdp_init_pq_readback(pdev);

	CMDQ_LOG("%s --\n", __func__);

}

void cmdq_mdp_deinit(void)
{
	cmdq_mdp_pool_clear();
}

/* Platform dependent function */

struct RegDef {
	int offset;
	const char *name;
};

void cmdq_mdp_dump_mmsys_config_virtual(const struct cmdqRecStruct *handle)
{
	/* Do Nothing */
}

/* Initialization & de-initialization MDP base VA */
void cmdq_mdp_init_module_base_VA_virtual(void)
{
	/* Do Nothing */
}

void cmdq_mdp_deinit_module_base_VA_virtual(void)
{
	/* Do Nothing */
}

/* query MDP clock is on  */
bool cmdq_mdp_clock_is_on_virtual(u32 engine)
{
	return false;
}

/* enable MDP clock  */
void cmdq_mdp_enable_clock_virtual(bool enable, u32 engine)
{
	/* Do Nothing */
}

/* Common Clock Framework */
void cmdq_mdp_init_module_clk_virtual(void)
{
	/* Do Nothing */
}

/* MDP engine dump */
void cmdq_mdp_dump_rsz_virtual(const unsigned long base, const char *label)
{
	u32 value[8] = { 0 };
	u32 request[8] = { 0 };
	u32 state = 0;

	value[0] = CMDQ_REG_GET32(base + 0x004);
	value[1] = CMDQ_REG_GET32(base + 0x00C);
	value[2] = CMDQ_REG_GET32(base + 0x010);
	value[3] = CMDQ_REG_GET32(base + 0x014);
	value[4] = CMDQ_REG_GET32(base + 0x018);
	CMDQ_REG_SET32(base + 0x040, 0x00000001);
	value[5] = CMDQ_REG_GET32(base + 0x044);
	CMDQ_REG_SET32(base + 0x040, 0x00000002);
	value[6] = CMDQ_REG_GET32(base + 0x044);
	CMDQ_REG_SET32(base + 0x040, 0x00000003);
	value[7] = CMDQ_REG_GET32(base + 0x044);

	CMDQ_ERR(
		"=============== [CMDQ] %s Status ====================================\n",
		label);
	CMDQ_ERR(
		"RSZ_CONTROL: 0x%08x, RSZ_INPUT_IMAGE: 0x%08x RSZ_OUTPUT_IMAGE: 0x%08x\n",
		 value[0], value[1], value[2]);
	CMDQ_ERR(
		"RSZ_HORIZONTAL_COEFF_STEP: 0x%08x, RSZ_VERTICAL_COEFF_STEP: 0x%08x\n",
		value[3], value[4]);
	CMDQ_ERR(
		"RSZ_DEBUG_1: 0x%08x, RSZ_DEBUG_2: 0x%08x, RSZ_DEBUG_3: 0x%08x\n",
		value[5], value[6], value[7]);

	/* parse state
	 * .valid=1/request=1: upstream module sends data
	 * .ready=1: downstream module receives data
	 */
	state = value[6] & 0xF;
	request[0] = state & (0x1);	/* out valid */
	request[1] = (state & (0x1 << 1)) >> 1;	/* out ready */
	request[2] = (state & (0x1 << 2)) >> 2;	/* in valid */
	request[3] = (state & (0x1 << 3)) >> 3;	/* in ready */
	request[4] = (value[1] & 0x1FFF);	/* input_width */
	request[5] = (value[1] >> 16) & 0x1FFF;	/* input_height */
	request[6] = (value[2] & 0x1FFF);	/* output_width */
	request[7] = (value[2] >> 16) & 0x1FFF;	/* output_height */

	CMDQ_ERR("RSZ inRdy,inRsq,outRdy,outRsq: %d,%d,%d,%d (%s)\n",
		request[3], request[2], request[1], request[0],
		cmdq_mdp_get_rsz_state(state));
	CMDQ_ERR(
		"RSZ input_width,input_height,output_width,output_height: %d,%d,%d,%d\n",
		request[4], request[5], request[6], request[7]);
}

void cmdq_mdp_dump_tdshp_virtual(const unsigned long base, const char *label)
{
	u32 value[8] = { 0 };

	value[0] = CMDQ_REG_GET32(base + 0x114);
	value[1] = CMDQ_REG_GET32(base + 0x11C);
	value[2] = CMDQ_REG_GET32(base + 0x104);
	value[3] = CMDQ_REG_GET32(base + 0x108);
	value[4] = CMDQ_REG_GET32(base + 0x10C);
	value[5] = CMDQ_REG_GET32(base + 0x120);
	value[6] = CMDQ_REG_GET32(base + 0x128);
	value[7] = CMDQ_REG_GET32(base + 0x110);

	CMDQ_ERR(
		"=============== [CMDQ] %s Status ====================================\n",
		label);
	CMDQ_ERR("TDSHP INPUT_CNT: 0x%08x, OUTPUT_CNT: 0x%08x\n",
		value[0], value[1]);
	CMDQ_ERR("TDSHP INTEN: 0x%08x, INTSTA: 0x%08x, 0x10C: 0x%08x\n",
		value[2], value[3], value[4]);
	CMDQ_ERR("TDSHP CFG: 0x%08x, IN_SIZE: 0x%08x, OUT_SIZE: 0x%08x\n",
		value[7], value[5], value[6]);
}

/* MDP callback function */
s32 cmdqMdpClockOn_virtual(u64 engineFlag)
{
	return 0;
}

s32 cmdqMdpDumpInfo_virtual(u64 engineFlag, int level)
{
	return 0;
}

s32 cmdqMdpResetEng_virtual(u64 engineFlag)
{
	return 0;
}

s32 cmdqMdpClockOff_virtual(u64 engineFlag)
{
	return 0;
}

static s32 mdp_engine_dummy(struct EngineStruct *engine_list)
{
	return 0;
}

static bool mdp_check_handle_dummy(struct cmdqRecStruct *handle)
{
	return false;
}

/* MDP Initialization setting */
void cmdqMdpInitialSetting_virtual(struct platform_device *pdev)
{
	/* common larb init only support 1 larb */
	cmdq_mdp_init_larb(pdev);
}

/* test MDP clock function */
u32 cmdq_mdp_rdma_get_reg_offset_src_addr_virtual(void)
{
	return 0;
}

u32 cmdq_mdp_wrot_get_reg_offset_dst_addr_virtual(void)
{
	return 0;
}

u32 cmdq_mdp_wdma_get_reg_offset_dst_addr_virtual(void)
{
	return 0;
}

void testcase_clkmgr_mdp_virtual(void)
{
}

const char *cmdq_mdp_dispatch_virtual(u64 engineFlag)
{
	return "MDP";
}

void cmdq_mdp_trackTask_virtual(const struct cmdqRecStruct *task)
{
	if (unlikely(mdp_tasks_idx < 0)) {
		CMDQ_ERR("%s mdp_tasks_idx:%d\n", __func__, mdp_tasks_idx);
		return;
	}

	if (task) {
		memcpy(mdp_tasks[mdp_tasks_idx].callerName,
			task->caller_name, sizeof(task->caller_name));
		if (task->user_debug_str)
			memcpy(mdp_tasks[mdp_tasks_idx].userDebugStr,
				task->user_debug_str,
				(u32)strlen(task->user_debug_str) + 1);
		else
			mdp_tasks[mdp_tasks_idx].userDebugStr[0] = '\0';
	} else {
		mdp_tasks[mdp_tasks_idx].callerName[0] = '\0';
		mdp_tasks[mdp_tasks_idx].userDebugStr[0] = '\0';
	}

	CMDQ_MSG("[Track]caller: %s\n",
		mdp_tasks[mdp_tasks_idx].callerName);
	CMDQ_MSG("[Track]DebugStr: %s\n",
		mdp_tasks[mdp_tasks_idx].userDebugStr);
	CMDQ_MSG("[Track]Index: %d\n",
		mdp_tasks_idx);

	mdp_tasks_idx = (mdp_tasks_idx + 1) % MDP_MAX_TASK_NUM;
}

const char *cmdq_mdp_parse_handle_error_module_by_hwflag_virtual(
	const struct cmdqRecStruct *handle)
{
	return "MDP";
}

u64 cmdq_mdp_get_engine_group_bits_virtual(u32 engine_group)
{
	return 0;
}

void cmdq_mdp_error_reset_virtual(u64 engineFlag)
{
}

long cmdq_mdp_get_module_base_VA_MMSYS_CONFIG(void)
{
	return cmdq_mmsys_base;
}

static void cmdq_mdp_enable_common_clock_virtual(bool enable, u64 engine_flag)
{
#ifdef CMDQ_PWR_AWARE
#if IS_ENABLED(CONFIG_MTK_SMI)
	int ret = 0;

	if (!mdp_ctx.larb) {
		CMDQ_ERR("%s smi larb not support\n", __func__);
		return;
	}

	if (enable)
		ret = mtk_smi_larb_get(mdp_ctx.larb);
	else
		mtk_smi_larb_put(mdp_ctx.larb);

	if (ret)
		CMDQ_ERR("%s %s fail ret:%d\n",
			__func__, enable ? "enable" : "disable", ret);
#endif	/* CONFIG_MTK_SMI */
#endif	/* CMDQ_PWR_AWARE */
}

/* Common Code */

void cmdq_mdp_map_mmsys_VA(void)
{
	cmdq_mmsys_base = cmdq_dev_alloc_reference_VA_by_name("mmsys_config");
}

void cmdq_mdp_unmap_mmsys_VA(void)
{
	cmdq_dev_free_module_base_VA(cmdq_mmsys_base);
}

static void mdp_request_voltage(unsigned long frequency, bool is_mdp)
{
	int low_volt, ret = 0;
	int index = 0;
	u64 *freqs = is_mdp ? mdp_pmqos_freq : isp_pmqos_freq;
	int *volts = is_mdp ? mdp_volts : isp_volts;

	if (!frequency) {
		low_volt = 0;
	} else {
		for (index = 0; index < mdp_pmqos_opp_num; index++) {
			if (frequency <= freqs[index])
				break;
		}

		if (index == mdp_pmqos_opp_num)
			index--;
		low_volt = volts[index];
	}
	CMDQ_LOG_PMQOS("%s is_mdp %d, frequency %lu, low_volt %d\n",
		__func__, is_mdp, frequency, low_volt);

	if (is_mdp) {
		ret = regulator_set_voltage(mdp_mmdvfs_reg, low_volt, INT_MAX);
		if (ret)
			CMDQ_ERR("%s regulator_set_voltage(mdp) fail ret:%d\n",
				__func__, ret);
	} else {
		if (cmdq_mdp_get_func()->mdpIsCaminSupport()) {
			ret = regulator_set_voltage(isp_mmdvfs_reg, low_volt, INT_MAX);
			if (ret)
				CMDQ_ERR("%s regulator_set_voltage(isp) fail ret:%d\n",
					__func__, ret);
		}
	}
}

static void mdp_update_voltage(u32 thread_id, u64 freq, bool is_mdp)
{
	u32 i;
	unsigned long max_freq = 0;
	CMDQ_LOG_PMQOS("%s thread %u freq %llu, is_mdp %u\n",
		__func__, thread_id, freq, is_mdp);

	CMDQ_SYSTRACE_BEGIN("%s %u %llu\n", __func__, thread_id, freq);
	mdp_current_freq[thread_id] = freq;

	/* scan for max freq */
	for (i = 0; i < ARRAY_SIZE(mdp_current_freq); i++)
		max_freq = max((unsigned long long)(max_freq), mdp_current_freq[i]);
	/* update voltage by clock frequency */
	mdp_request_voltage(max_freq, is_mdp);
	CMDQ_SYSTRACE_END();
}

#define mdp_t(_act_throughput)	\
	min((_act_throughput), mdp_pmqos_freq[mdp_pmqos_opp_num - 1])

#define isp_t(_act_throughput)	\
	min((_act_throughput), isp_pmqos_freq[mdp_pmqos_opp_num - 1])

static void cmdq_mdp_begin_task_virtual(struct cmdqRecStruct *handle,
	struct cmdqRecStruct **handle_list, u32 size)
{
	struct mdp_pmqos *mdp_curr_pmqos;
	struct mdp_pmqos *target_pmqos = NULL;
	struct mdp_pmqos *mdp_list_pmqos;
	struct mdp_pmqos_record *pmqos_curr_record;
	struct mdp_pmqos_record *pmqos_list_record;
	s32 i = 0;
	struct timespec64 curr_time;
	s32 numerator;
	s32 denominator;
	u32 thread_id = handle->thread - MDP_THREAD_START;
	u32 max_throughput = 0;
	u32 isp_throughput = 0;
	u32 mdp_throughput = 0;
	u32 isp_curr_bandwidth = 0;
	u32 mdp_curr_bandwidth = 0;
	u32 total_pixel = 0;
	bool first_task = true;
	bool expired;

#ifdef MDP_MMPATH
	/* For MMpath */
	uint32_t *addr1 = NULL;
#endif	/* MDP_MMPATH */

	CMDQ_SYSTRACE_BEGIN("%s %u\n", __func__, size);

	if (unlikely(!handle_list))
		goto done;

	/* check engine status */
	cmdq_mdp_get_func()->CheckHwStatus(handle);

	if (!handle->prop_addr)
		goto done;

	if (cmdq_mdp_get_func()->mdpIsCaminSupport() && !isp_pmqos_freq) {
		CMDQ_ERR("%s isp_pmqos_freq not available\n", __func__);
		goto done;
	}

	if (!mdp_pmqos_freq) {
		CMDQ_ERR("%s mdp_pmqos_freq not available\n", __func__);
		goto done;
	}

	pmqos_curr_record =
		kzalloc(sizeof(struct mdp_pmqos_record), GFP_KERNEL);
	if (unlikely(!pmqos_curr_record)) {
		CMDQ_ERR("alloc pmqos_curr_record fail\n");
		return;
	}
	handle->user_private = pmqos_curr_record;

	ktime_get_real_ts64(&curr_time);

	mdp_curr_pmqos = (struct mdp_pmqos *)handle->prop_addr;
	pmqos_curr_record->submit_tm = curr_time;
	pmqos_curr_record->end_tm.tv_sec = mdp_curr_pmqos->tv_sec;
	pmqos_curr_record->end_tm.tv_nsec = mdp_curr_pmqos->tv_usec * 1000;

	expired = curr_time.tv_sec > mdp_curr_pmqos->tv_sec ||
		(curr_time.tv_sec == mdp_curr_pmqos->tv_sec &&
		curr_time.tv_nsec > mdp_curr_pmqos->tv_usec * 1000);

	CMDQ_LOG_PMQOS(
		"%s%s handle:%p engine:%#llx thread:%d cur:%lu.%lu end:%lu.%lu list size:%u, is_mdp %d\n",
		__func__, expired ? " expired" : "",
		handle, handle->engineFlag, handle->thread,
		curr_time.tv_sec, curr_time.tv_nsec,
		mdp_curr_pmqos->tv_sec, mdp_curr_pmqos->tv_usec * 1000,
		size,
		cmdq_mdp_get_func()->mdpIsIspCamin(handle) ?  false : true);

	CMDQ_LOG_PMQOS(
		"mdp %u pixel, mdp %u byte, isp %u pixel, isp %u byte, submit %06ld ns, end %06ld ns\n",
		mdp_curr_pmqos->mdp_total_pixel,
		mdp_curr_pmqos->mdp_total_datasize,
		mdp_curr_pmqos->isp_total_pixel,
		mdp_curr_pmqos->isp_total_datasize,
		pmqos_curr_record->submit_tm.tv_nsec,
		pmqos_curr_record->end_tm.tv_nsec);

	if (size > 1) {/*handle_list includes the current task*/
		for (i = 0; i < size; i++) {
			struct cmdqRecStruct *curTask = handle_list[i];

			if (!curTask)
				continue;

			if (!curTask->user_private)
				continue;

			mdp_list_pmqos = (struct mdp_pmqos *)curTask->prop_addr;
			pmqos_list_record =
			    (struct mdp_pmqos_record *)curTask->user_private;
			total_pixel = max(mdp_list_pmqos->mdp_total_pixel,
					mdp_list_pmqos->isp_total_pixel);

			if (first_task) {
				target_pmqos = mdp_list_pmqos;
				DP_TIMER_GET_DURATION_IN_US(
					pmqos_list_record->submit_tm,
					pmqos_list_record->end_tm, denominator);
				DP_TIMER_GET_DURATION_IN_US(
					pmqos_curr_record->submit_tm,
					pmqos_list_record->end_tm, numerator);
				if (denominator == numerator)
					pmqos_list_record->mdp_throughput =
						(total_pixel / denominator);
				else
					pmqos_list_record->mdp_throughput =
						(total_pixel / numerator) -
						(total_pixel / denominator);
				max_throughput =
					pmqos_list_record->mdp_throughput;
				first_task = false;
			} else {
				struct cmdqRecStruct *prevTask =
					handle_list[i - 1];
				struct mdp_pmqos *mdp_prev_pmqos;
				struct mdp_pmqos_record *mdp_prev_record;

				if (!prevTask)
					continue;

				mdp_prev_pmqos =
					(struct mdp_pmqos *)prevTask->prop_addr;
				mdp_prev_record =
					(struct mdp_pmqos_record *)
					prevTask->user_private;
				if (!mdp_prev_record)
					continue;
				DP_TIMER_GET_DURATION_IN_US(
					pmqos_curr_record->submit_tm,
					pmqos_list_record->end_tm, denominator);
				DP_TIMER_GET_DURATION_IN_US(
					pmqos_curr_record->submit_tm,
					mdp_prev_record->end_tm, numerator);

				pmqos_list_record->mdp_throughput =
					(mdp_prev_record->mdp_throughput *
					numerator / denominator) +
					(total_pixel / denominator);
				if (pmqos_list_record->mdp_throughput >
				max_throughput)
					max_throughput =
					pmqos_list_record->mdp_throughput;
			}
			CMDQ_LOG_PMQOS(
				"list[%d] mdp %u pixel %u byte, isp %u pixel %u byte, submit %06ld ns, end %06ld ns, max_tput %u, total_pixel %u (%d %d)\n",
				i,
				mdp_list_pmqos->mdp_total_pixel,
				mdp_list_pmqos->mdp_total_datasize,
				mdp_list_pmqos->isp_total_pixel,
				mdp_list_pmqos->isp_total_datasize,
				pmqos_list_record->submit_tm.tv_nsec,
				pmqos_list_record->end_tm.tv_nsec,
				max_throughput, total_pixel,
				denominator, numerator);
		}
	} else {
		DP_TIMER_GET_DURATION_IN_US(pmqos_curr_record->submit_tm,
			pmqos_curr_record->end_tm, denominator);
		total_pixel = max(mdp_curr_pmqos->mdp_total_pixel, mdp_curr_pmqos->isp_total_pixel);
		pmqos_curr_record->mdp_throughput =
			total_pixel / denominator;
		target_pmqos = mdp_curr_pmqos;
		max_throughput = pmqos_curr_record->mdp_throughput;
	}

	if (!target_pmqos) {
		CMDQ_ERR("%s no target_pmqos handle:%p engine:%#llx thread:%d\n",
			__func__, handle, handle->engineFlag, handle->thread);
		goto done;
	}

	total_pixel = max(target_pmqos->mdp_total_pixel, target_pmqos->isp_total_pixel);

	CMDQ_LOG_PMQOS(
		"[%d]begin task, is_mdp %d, target_pmqos max_throughput %u total_pixel %u\n",
		thread_id, cmdq_mdp_get_func()->mdpIsIspCamin(handle) ?  false : true,
		max_throughput, total_pixel);

	/* update isp bandwidth and clock */
	if (target_pmqos->isp_total_datasize && isp_pmqos_freq) {
		isp_throughput = isp_t((unsigned long long)(max_throughput));

		for (i = 0; i < min_t(u32, PMQOS_ISP_PORT_NUM, target_pmqos->qos2_isp_count);
			i++) {
			struct icc_path *port_path =
				cmdq_mdp_get_func()->qosGetPath(
				thread_id, target_pmqos->qos2_isp_port[i]);

			DP_BANDWIDTH(target_pmqos->qos2_isp_bandwidth[i],
				total_pixel,
				isp_throughput,
				isp_curr_bandwidth);

			cmdq_mdp_get_func()->qosCheckBWLimit(thread_id,
				mdp_curr_pmqos->qos2_isp_port[i],
				mdp_curr_pmqos->qos2_isp_bandwidth[i],
				isp_curr_bandwidth);

			CMDQ_LOG_PMQOS(
				"[%d]begin task, update target isp-bw of port[%d](0x%x) from %u to %u\n",
				thread_id, i,
				target_pmqos->qos2_isp_port[i],
				target_pmqos->qos2_isp_bandwidth[i],
				isp_curr_bandwidth);
			mtk_icc_set_bw(port_path, MBps_to_icc(isp_curr_bandwidth), 0);
		}
		mdp_update_voltage(thread_id, isp_throughput, false);
	}

	/* update mdp bandwidth and clock */
	if (target_pmqos->mdp_total_datasize) {
		mdp_throughput = mdp_t((unsigned long long)(max_throughput));

		for (i = 0; i < min_t(u32, PMQOS_MDP_PORT_NUM, target_pmqos->qos2_mdp_count);
			i++) {
			u32 port = cmdq_mdp_get_func()->qosTransPort(
					target_pmqos->qos2_mdp_port[i]);
			struct icc_path *port_path =
				cmdq_mdp_get_func()->qosGetPath(thread_id,
				port);

			DP_BANDWIDTH(target_pmqos->qos2_mdp_bandwidth[i],
				target_pmqos->mdp_total_pixel,
				mdp_throughput,
				mdp_curr_bandwidth);

			CMDQ_LOG_PMQOS(
				"[%d]begin task, update target mdp-bw of port[%d](0x%x) from %u to %u\n",
				thread_id, i,
				target_pmqos->qos2_mdp_port[i],
				target_pmqos->qos2_mdp_bandwidth[i],
				mdp_curr_bandwidth);
			mtk_icc_set_bw(port_path, MBps_to_icc(mdp_curr_bandwidth), 0);
		}

		mdp_throughput = max(isp_throughput, mdp_throughput);
		mdp_update_voltage(thread_id, mdp_throughput, true);
	}

#ifdef MDP_MMPATH
	if (!handle->prop_addr)
		goto done;
	mdp_curr_pmqos = (struct mdp_pmqos *)handle->prop_addr;

	do {
		if (mdp_curr_pmqos->mdpMMpathStringSize <= 0)
			break;
		addr1 = kcalloc(mdp_curr_pmqos->mdpMMpathStringSize,
			sizeof(char), GFP_KERNEL);
		if (!addr1) {
			mdp_curr_pmqos->mdpMMpathStringSize = 0;
			CMDQ_ERR("[MDP] fail to alloc mmpath buf\n");
			break;
		}
		if (copy_from_user
			(addr1, CMDQ_U32_PTR(mdp_curr_pmqos->mdpMMpathString),
			mdp_curr_pmqos->mdpMMpathStringSize * sizeof(char))) {
			mdp_curr_pmqos->mdpMMpathStringSize = 0;
			CMDQ_MSG("[MDP] fail to copy user mmpath log\n");
			kfree(addr1);
			break;
		}
		mdp_curr_pmqos->mdpMMpathString = (unsigned long)addr1;
		if (mdp_curr_pmqos->mdpMMpathStringSize > 0)
			trace_MMPath((char *)CMDQ_U32_PTR(
				mdp_curr_pmqos->mdpMMpathString));

	} while (0);
#endif	/* MDP_MMPATH */

done:
	CMDQ_SYSTRACE_END();
}

static void cmdq_mdp_isp_begin_task_virtual(struct cmdqRecStruct *handle,
	struct cmdqRecStruct **handle_list, u32 size)
{

	if (!cmdq_mdp_get_func()->mdpIsIspImg(handle))
		return;

	CMDQ_LOG_PMQOS("enter %s handle:0x%p engine:0x%llx\n", __func__,
		handle, handle->engineFlag);
	cmdq_mdp_begin_task_virtual(handle, handle_list, size);
}

static void cmdq_mdp_end_task_virtual(struct cmdqRecStruct *handle,
	struct cmdqRecStruct **handle_list, u32 size)
{
	struct mdp_pmqos *mdp_curr_pmqos;
	struct mdp_pmqos *target_pmqos = NULL;
	struct mdp_pmqos *mdp_list_pmqos;
	struct mdp_pmqos_record *pmqos_curr_record;
	struct mdp_pmqos_record *pmqos_list_record;
	s32 i = 0;
	struct timespec64 curr_time;
	s32 denominator;
	u32 thread_id = handle->thread - MDP_THREAD_START;
	u32 max_throughput = 0;
	u32 pre_throughput = 0;
	u32 isp_throughput = 0;
	u32 mdp_throughput = 0;
	bool trigger = false;
	bool first_task = true;
	s32 overdue;
	u32 isp_curr_bandwidth = 0;
	u32 mdp_curr_bandwidth = 0;
	u32 curr_pixel_size = 0;
	u32 total_pixel = 0;
	bool expired;
	ktime_get_real_ts64(&curr_time);

	if (!handle->prop_addr)
		return;

	if (cmdq_mdp_get_func()->mdpIsCaminSupport() && !isp_pmqos_freq) {
		CMDQ_ERR("%s isp_pmqos_freq not available\n", __func__);
		return;
	}

	if (!mdp_pmqos_freq) {
		CMDQ_ERR("%s mdp_pmqos_freq not available\n", __func__);
		return;
	}

	mdp_curr_pmqos = (struct mdp_pmqos *)handle->prop_addr;
	pmqos_curr_record = (struct mdp_pmqos_record *)handle->user_private;
	if (unlikely(!pmqos_curr_record)) {
		CMDQ_ERR("alloc pmqos_curr_record fail\n");
		return;
	}
	pmqos_curr_record->submit_tm = curr_time;

	expired = curr_time.tv_sec > mdp_curr_pmqos->tv_sec ||
		(curr_time.tv_sec == mdp_curr_pmqos->tv_sec &&
		curr_time.tv_nsec > mdp_curr_pmqos->tv_usec * 1000);
	CMDQ_LOG_PMQOS(
		"%s%s handle:%p engine:%#llx thread:%d cur:%lu.%lu end:%lu.%lu list size:%u mdp:%u isp:%u\n",
		__func__, expired ? " expired" : "",
		handle, handle->engineFlag, handle->thread,
		curr_time.tv_sec, curr_time.tv_nsec * 1000,
		mdp_curr_pmqos->tv_sec, mdp_curr_pmqos->tv_usec,
		size,
		mdp_curr_pmqos->mdp_total_pixel,
		mdp_curr_pmqos->isp_total_pixel);

	for (i = 0; i < size; i++) {
		struct cmdqRecStruct *curTask = handle_list[i];

		if (!curTask)
			continue;

		if (!curTask->user_private)
			continue;

		mdp_list_pmqos = (struct mdp_pmqos *)curTask->prop_addr;
		pmqos_list_record =
			(struct mdp_pmqos_record *)curTask->user_private;

		if (first_task) {
			target_pmqos = mdp_list_pmqos;
			curr_pixel_size = max(mdp_list_pmqos->mdp_total_pixel,
						mdp_list_pmqos->isp_total_pixel);
			first_task = false;
		}

		if (pmqos_curr_record->mdp_throughput <
			pmqos_list_record->mdp_throughput) {
			if (max_throughput <
				pmqos_list_record->mdp_throughput) {
				max_throughput =
					pmqos_list_record->mdp_throughput;

				DP_TIMER_GET_DURATION_IN_US(
					pmqos_curr_record->submit_tm,
					pmqos_list_record->end_tm, overdue);
				if (overdue == 1) {
					trigger = true;
					break;
				}
			}
			continue;
		}
		trigger = true;
		break;
	}
	first_task = true;
	/*handle_list excludes the current task*/
	if (size > 0 && trigger) {
		for (i = 0; i < size; i++) {
			struct cmdqRecStruct *curTask = handle_list[i];

			if (!curTask)
				continue;

			if (!curTask->user_private)
				continue;

			mdp_list_pmqos = (struct mdp_pmqos *)curTask->prop_addr;
			pmqos_list_record =
				(struct mdp_pmqos_record *)
					curTask->user_private;

			total_pixel = max(mdp_list_pmqos->mdp_total_pixel,
					mdp_list_pmqos->isp_total_pixel);

			if (first_task) {
				DP_TIMER_GET_DURATION_IN_US(
					pmqos_list_record->submit_tm,
					pmqos_list_record->end_tm, denominator);
				pmqos_list_record->mdp_throughput =
					total_pixel / denominator;
				max_throughput =
					pmqos_list_record->mdp_throughput;

				first_task = false;
				pre_throughput = max_throughput;
			} else {
				if (!pre_throughput)
					continue;
				DP_TIMER_GET_DURATION_IN_US(
					pmqos_curr_record->submit_tm,
					pmqos_list_record->end_tm, denominator);
				pmqos_list_record->mdp_throughput =
					pre_throughput +
					(total_pixel / denominator);
				pre_throughput =
					pmqos_list_record->mdp_throughput;
				if (pmqos_list_record->mdp_throughput >
					max_throughput)
					max_throughput =
					pmqos_list_record->mdp_throughput;
			}
			CMDQ_LOG_PMQOS(
				"list[%d] mdp %u MHz, mdp %u pixel, mdp %u byte, isp %u pixel, isp %u byte, submit %06ld ns, end %06ld ns, max_tput %u\n",
				i, pmqos_list_record->mdp_throughput,
				mdp_list_pmqos->mdp_total_pixel,
				mdp_list_pmqos->mdp_total_datasize,
				mdp_list_pmqos->isp_total_pixel,
				mdp_list_pmqos->isp_total_datasize,
				pmqos_list_record->submit_tm.tv_nsec,
				pmqos_list_record->end_tm.tv_nsec,
				max_throughput);
		}
	}

	CMDQ_LOG_PMQOS(
		"[%d]end task, is_mdp %d, max_tput %u, pixel mdp %u -> %u isp %u -> %u, data mdp %u -> %u isp %u -> %u\n",
		thread_id, cmdq_mdp_get_func()->mdpIsIspCamin(handle) ?  false : true,
		max_throughput,
		mdp_curr_pmqos->mdp_total_pixel,
		target_pmqos ? target_pmqos->mdp_total_pixel : 0,
		mdp_curr_pmqos->isp_total_pixel,
		target_pmqos ? target_pmqos->isp_total_pixel : 0,
		mdp_curr_pmqos->mdp_total_datasize,
		target_pmqos ? target_pmqos->mdp_total_datasize : 0,
		mdp_curr_pmqos->isp_total_datasize,
		target_pmqos ? target_pmqos->isp_total_datasize : 0);

	kfree(handle->user_private);
	handle->user_private = NULL;

	/* update isp bandwidth and clock */
	if (target_pmqos && target_pmqos->isp_total_datasize && isp_pmqos_freq) {
		isp_throughput = isp_t((unsigned long long)(max_throughput));

		/* turn off current first */
		for (i = 0; i < min_t(u32, PMQOS_ISP_PORT_NUM, mdp_curr_pmqos->qos2_isp_count);
			i++) {
			struct icc_path *port_path =
				cmdq_mdp_get_func()->qosGetPath(thread_id,
				mdp_curr_pmqos->qos2_isp_port[i]);

			CMDQ_LOG_PMQOS(
				"[%d]end task, clear curr isp-bw of port[%d](0x%x) to 0\n",
				thread_id, i, mdp_curr_pmqos->qos2_isp_port[i]);
			mtk_icc_set_bw(port_path, 0, 0);
		}

		/* turn on next ports */
		for (i = 0; i < min_t(u32, PMQOS_ISP_PORT_NUM, target_pmqos->qos2_isp_count);
			i++) {
			struct icc_path *port_path =
				cmdq_mdp_get_func()->qosGetPath(thread_id,
				target_pmqos->qos2_isp_port[i]);

			DP_BANDWIDTH(target_pmqos->qos2_isp_bandwidth[i],
				target_pmqos->isp_total_pixel,
				isp_throughput,
				isp_curr_bandwidth);

			cmdq_mdp_get_func()->qosCheckBWLimit(thread_id,
				mdp_curr_pmqos->qos2_isp_port[i],
				mdp_curr_pmqos->qos2_isp_bandwidth[i],
				isp_curr_bandwidth);

			CMDQ_LOG_PMQOS(
				"[%d]end task, update target isp-bw of port[%d](0x%x) from %u to %u\n",
				thread_id, i,
				mdp_curr_pmqos->qos2_isp_port[i],
				target_pmqos->qos2_isp_bandwidth[i],
				isp_curr_bandwidth);
			mtk_icc_set_bw(port_path, MBps_to_icc(isp_curr_bandwidth), 0);
		}
		mdp_update_voltage(thread_id, isp_throughput, false);

	} else if (mdp_curr_pmqos->isp_total_datasize) {
		CMDQ_LOG_PMQOS("[%d]end task, clear isp bandwidth and clock\n", thread_id);
		cmdq_mdp_get_func()->qosClearAllIsp(thread_id);
		mdp_update_voltage(thread_id, 0, false);
	}

	/* update mdp bandwidth and clock */
	if (target_pmqos && target_pmqos->mdp_total_datasize) {
		mdp_throughput = mdp_t((unsigned long long)(max_throughput));

		/* turn off current first */
		for (i = 0; i < min_t(u32, PMQOS_MDP_PORT_NUM, mdp_curr_pmqos->qos2_mdp_count);
			i++) {
			u32 port = cmdq_mdp_get_func()->qosTransPort(
				mdp_curr_pmqos->qos2_mdp_port[i]);
			struct icc_path *port_path =
				cmdq_mdp_get_func()->qosGetPath(thread_id,
				port);

			CMDQ_LOG_PMQOS(
				"[%d]end task, clear curr mdp-bw of port[%d](0x%x) to 0\n",
				thread_id, i, mdp_curr_pmqos->qos2_mdp_port[i]);
			mtk_icc_set_bw(port_path, 0, 0);
		}

		/* turn on next ports */
		for (i = 0; i < min_t(u32, PMQOS_MDP_PORT_NUM, target_pmqos->qos2_mdp_count);
			i++) {
			u32 port = cmdq_mdp_get_func()->qosTransPort(
					target_pmqos->qos2_mdp_port[i]);
			struct icc_path *port_path =
				cmdq_mdp_get_func()->qosGetPath(thread_id,
				port);

			DP_BANDWIDTH(target_pmqos->qos2_mdp_bandwidth[i],
				target_pmqos->mdp_total_pixel,
				mdp_throughput,
				mdp_curr_bandwidth);

			CMDQ_LOG_PMQOS(
				"[%d]end task, update target mdp-bw of port[%d](0x%x) from %u to %u\n",
				thread_id, i,
				mdp_curr_pmqos->qos2_mdp_port[i],
				mdp_curr_pmqos->qos2_mdp_bandwidth[i],
				mdp_curr_bandwidth);
			mtk_icc_set_bw(port_path, MBps_to_icc(mdp_curr_bandwidth), 0);
		}

		mdp_throughput = max(isp_throughput, mdp_throughput);
		mdp_update_voltage(thread_id, mdp_throughput, true);
	} else if (mdp_curr_pmqos->mdp_total_datasize) {
		CMDQ_LOG_PMQOS("[%d]end task, clear mdp bandwidth and clock\n", thread_id);
		cmdq_mdp_get_func()->qosClearAll(thread_id);
		mdp_update_voltage(thread_id, 0, true);
	}

#ifdef MDP_MMPATH
	if (handle->prop_addr) {
		mdp_curr_pmqos = (struct mdp_pmqos *)handle->prop_addr;
		if (mdp_curr_pmqos->mdpMMpathStringSize > 0) {
			kfree(CMDQ_U32_PTR(mdp_curr_pmqos->mdpMMpathString));
			mdp_curr_pmqos->mdpMMpathString = 0;
			mdp_curr_pmqos->mdpMMpathStringSize = 0;
		}
	}
#endif	/* MDP_MMPATH */
}

static void cmdq_mdp_isp_end_task_virtual(struct cmdqRecStruct *handle,
	struct cmdqRecStruct **handle_list, u32 size)
{
	if (!cmdq_mdp_get_func()->mdpIsIspImg(handle))
		return;

	cmdq_mdp_end_task_virtual(handle, handle_list, size);
}

static void cmdq_mdp_check_hw_status_virtual(struct cmdqRecStruct *handle)
{
	/* Do nothing */
}

u64 cmdq_mdp_get_secure_engine_virtual(u64 engine_flag)
{
	return 0;
}

void cmdq_mdp_resolve_token_virtual(u64 engine_flag,
	const struct cmdqRecStruct *task)
{
}

const char *mdp_parse_mod(struct cmdqRecStruct *handle)
{
	return "MDP";
}

u32 cmdq_mdp_qos_translate_port_virtual(u32 engine_id)
{
	return 0;
}

static void mdp_qos_init_virtual(struct platform_device *pdev, u32 thread_id)
{
}

static void *mdp_qos_get_path_virtual(u32 thread_id, u32 port)
{
	return NULL;
}

static void mdp_qos_clear_all_virtual(u32 thread_id)
{
}

static void mdp_qos_check_bw_limit_virtual(u32 thread_id,
	u32 port, u32 user_bw, u32 set_bw)
{
}

static u32 mdp_get_dummy(void)
{
	CMDQ_ERR("%s not support dummy group\n", __func__);
	return 0;
}

static u32 mdp_get_dummy_mdp(void)
{
	CMDQ_ERR("%s not support dummy group\n", __func__);
	return 0;
}

static u32 mdp_get_dummy_isp(void)
{
	CMDQ_MSG("%s not support dummy group\n", __func__);
	return U32_MAX;
}

static u32 mdp_get_dummy_wpe(void)
{
	CMDQ_MSG("%s not support dummy group\n", __func__);
	return 0;
}

static const char **const mdp_get_engine_group_name(void)
{
	return NULL;
}

static phys_addr_t *mdp_get_engine_base_dummy(void)
{
	CMDQ_ERR("%s no platform\n", __func__);
	return NULL;
}

static u32 mdp_get_engine_base_count_dummy(void)
{
	CMDQ_ERR("%s no platform\n", __func__);
	return 0;
}

void cmdq_mdp_compose_readback_virtual(struct cmdqRecStruct *handle,
	u16 engine, dma_addr_t dma, u32 param)
{
	CMDQ_ERR("%s not implement\n", __func__);
}

u16 mdp_get_rb_event_lock(void)
{
#if defined(CMDQ_SECURE_PATH_SUPPORT)
	return mdp_ctx.pq_readback.rb_event_lock;
#else
	CMDQ_ERR("%s not implement\n", __func__);
	return 0;
#endif
}

u16 mdp_get_rb_event_unlock(void)
{
#if defined(CMDQ_SECURE_PATH_SUPPORT)
	return mdp_ctx.pq_readback.rb_event_unlock;
#else
	CMDQ_ERR("%s not implement\n", __func__);
	return 0;
#endif
}

#define MDP_AAL_SRAM_CFG	0x0C4
#define MDP_AAL_SRAM_STATUS	0x0C8
#define MDP_AAL_SRAM_RW_IF_2	0x0D4
#define MDP_AAL_SRAM_RW_IF_3	0x0D8
#define MDP_AAL_DUAL_PIPE_00	0x500
#define MDP_AAL_DUAL_PIPE_08	0x544

#define MDP_AAL_SRAM_RW_IF_2_MASK	0x01FFF
#define MDP_AAL_SRAM_CNT		768
#define MDP_AAL_SRAM_STATUS_BIT		BIT(17)
#define MDP_AAL_DRE_BITS(_param)	(_param & 0xF)
#define MDP_AAL_MULTIPLE_BITS(_param)	((_param >> 4) & 1)

static void mdp_readback_aal_virtual(struct cmdqRecStruct *handle,
	u16 engine, phys_addr_t base, dma_addr_t pa, u32 param, u32 pipe)
{
	struct mdp_readback_engine *rb =
		&handle->readback_engs[handle->readback_cnt];
	struct cmdq_pkt *pkt = handle->pkt;
	u32 dre = MDP_AAL_DRE_BITS(param);
	u32 multiple = MDP_AAL_MULTIPLE_BITS(param);
	dma_addr_t begin_pa;
	u32 offset, condi_offset;
	u32 *condi_inst;
	u16 hist_sram_start = mdp_ctx.pq_readback.dre30_hist_sram_start;

#if defined(CMDQ_SECURE_PATH_SUPPORT)
	const u16 rb_event_lock = mdp_ctx.pq_readback.rb_event_lock;
	const u16 rb_event_unlock = mdp_ctx.pq_readback.rb_event_unlock;
#endif

	const u16 idx_addr = CMDQ_THR_SPR_IDX1;
	const u16 idx_val = CMDQ_THR_SPR_IDX2;
	const u16 idx_out_spr = CMDQ_THR_SPR_IDX3;

	/* pipe 0: P6 (R12+R13)
	 * pipe 1: P7 (R14+R15)
	 */
	u16 idx_out = CMDQ_GPR_CNT_ID + CMDQ_GPR_R12;
	u16 idx_out64 = CMDQ_GPR_CNT_ID + CMDQ_GPR_P6;

	struct cmdq_operand lop, rop;

	CMDQ_LOG_PQ("%s buffer:%lx engine:%hu dre:%u\n",
		__func__, (unsigned long)pa, engine, dre);

	if (pipe == 1) {
		idx_out = CMDQ_GPR_CNT_ID + CMDQ_GPR_R14;
		idx_out64 = CMDQ_GPR_CNT_ID + CMDQ_GPR_P7;
	}

	rb->start = pa;
	rb->count = 768;
	if (multiple)
		rb->count += 16;

	rb->engine = engine;
	rb->param = param;
	handle->readback_cnt++;
	handle->mdp_extension |= 1LL << DP_CMDEXT_AAL_DRE;
	if (multiple)
		handle->mdp_extension |= 1LL << DP_CMDEXT_AAL_MULTIPIPE;

#if defined(CMDQ_SECURE_PATH_SUPPORT)
	/* secure pq readback is implemented in normal world */
	if (handle->secData.is_secure) {
		pkt = handle->pkt_rb;
		cmdq_op_wait_event_readback(handle, rb_event_lock);
	}
#else
	/* secure pq readback is implemented in secure world */
	if (handle->secData.is_secure)
		return;
#endif

	/* following part read back aal histogram */
	cmdq_pkt_write_value_addr(pkt, base + MDP_AAL_SRAM_CFG,
		(dre << 6) | (dre << 5) | BIT(4), GENMASK(6, 4));

	/* init sprs
	 * spr1 = AAL_SRAM_START
	 * gpr_p4 = out_pa
	 */
	cmdq_pkt_assign_command(pkt, idx_addr, hist_sram_start);
	cmdq_pkt_assign_command(pkt, idx_out_spr, (u32)pa);
	cmdq_pkt_assign_command(pkt, idx_out + 1, (u32)DO_SHIFT_RIGHT(pa, 32));

	/* loop again here */
	begin_pa = cmdq_pkt_get_curr_buf_pa(pkt);

	/* config aal sram addr and poll */
	cmdq_pkt_write_reg_addr(pkt, base + MDP_AAL_SRAM_RW_IF_2,
		idx_addr, U32_MAX);
	/* use gpr low as poll gpr */
	cmdq_pkt_poll_addr(pkt, MDP_AAL_SRAM_STATUS_BIT,
		base + MDP_AAL_SRAM_STATUS,
		MDP_AAL_SRAM_STATUS_BIT, idx_out - CMDQ_GPR_CNT_ID);
	/* read to value gpr */
	cmdq_pkt_read_addr(pkt, base + MDP_AAL_SRAM_RW_IF_3, idx_val);
	/* and now assign addr low 32bit from spr to idx_out gpr */
	lop.reg = true;
	lop.idx = idx_out_spr;
	rop.reg = false;
	rop.value = 0;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_out, &lop, &rop);
	/* write value src spr to dst gpr */
	cmdq_pkt_write_reg_indriect(pkt, idx_out64, idx_val, U32_MAX);

	/* jump forward end if sram is last one
	 * if spr1 >= 4096 + 4 * 767
	 */
	lop.reg = true;
	lop.idx = idx_addr;
	rop.reg = false;
	rop.value = hist_sram_start + 4 * (MDP_AAL_SRAM_CNT - 1);
	cmdq_pkt_assign_command(pkt, CMDQ_THR_SPR_IDX0, 0);
	condi_offset = pkt->cmd_buf_size - CMDQ_INST_SIZE;
	cmdq_pkt_cond_jump_abs(pkt, CMDQ_THR_SPR_IDX0, &lop, &rop,
		CMDQ_GREATER_THAN_AND_EQUAL);

	/* inc src addr */
	lop.reg = true;
	lop.idx = idx_addr;
	rop.reg = false;
	rop.value = 4;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_addr, &lop, &rop);
	/* inc outut pa */
	lop.reg = true;
	lop.idx = idx_out_spr;
	rop.reg = false;
	rop.value = 4;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_out_spr, &lop, &rop);

	cmdq_pkt_jump_addr(pkt, begin_pa);

	condi_inst = (u32 *)cmdq_pkt_get_va_by_offset(pkt, condi_offset);
	if (unlikely(!condi_inst)) {
		CMDQ_ERR("%s wrong offset %u\n", __func__, condi_offset);
		return;
	}
	if (condi_inst[1] == 0x10000001) {
		condi_inst = (u32 *)cmdq_pkt_get_va_by_offset(pkt,
			condi_offset + CMDQ_INST_SIZE);
		if (unlikely(!condi_inst)) {
			CMDQ_ERR("%s wrong offset %u.\n", __func__, condi_offset);
			return;
		}
	}
	*condi_inst = (u32)CMDQ_REG_SHIFT_ADDR(cmdq_pkt_get_curr_buf_pa(pkt));

	pa = pa + MDP_AAL_SRAM_CNT * 4;
	if (multiple) {
		u32 i;

		offset = 0;
		for (i = 0; i < 8; i++) {
			cmdq_pkt_mem_move(pkt, NULL,
				base + MDP_AAL_DUAL_PIPE_00 + i * 4,
				pa + offset, CMDQ_THR_SPR_IDX3);
			offset += 4;
		}

		for (i = 0; i < 8; i++) {
			cmdq_pkt_mem_move(pkt, NULL,
				base + MDP_AAL_DUAL_PIPE_08 + i * 4,
				pa + offset, CMDQ_THR_SPR_IDX3);
			offset += 4;
		}
	}

#if defined(CMDQ_SECURE_PATH_SUPPORT)
	if (handle->secData.is_secure)
		cmdq_op_set_event_readback(handle, rb_event_unlock);
#endif

	CMDQ_LOG_PQ("%s done, handle:%p, engine:%hu, mdp_extension:%x, readback_cnt:%d\n",
		__func__, handle, engine, handle->mdp_extension, handle->readback_cnt);

}

#define MDP_HDR_HIST_DATA 0x0E0
#define MDP_HDR_LBOX_DET_4 0x104
#define HDR_TONE_MAP_S14 0x0D0
#define HDR_GAIN_TABLE_2 0x0F0
#define MDP_HDR_HIST_CNT 57

static void mdp_readback_hdr_virtual(struct cmdqRecStruct *handle,
	u16 engine, phys_addr_t base, dma_addr_t pa, u32 param, u32 pipe)
{
	struct mdp_readback_engine *rb =
		&handle->readback_engs[handle->readback_cnt];
	struct cmdq_pkt *pkt = handle->pkt;
	dma_addr_t begin_pa;
	u32 condi_offset;
	u32 *condi_inst;

#if defined(CMDQ_SECURE_PATH_SUPPORT)
	u16 rb_event_lock = mdp_ctx.pq_readback.rb_event_lock;
	u16 rb_event_unlock = mdp_ctx.pq_readback.rb_event_unlock;
#endif

	const u16 idx_counter = CMDQ_THR_SPR_IDX1;
	const u16 idx_val = CMDQ_THR_SPR_IDX2;
	/* pipe 0: P6 (R12+R13)
	 * pipe 1: P7 (R14+R15)
	 */
	u16 idx_out = CMDQ_GPR_CNT_ID + CMDQ_GPR_R12;
	u16 idx_out64 = CMDQ_GPR_CNT_ID + CMDQ_GPR_P6;
	struct cmdq_operand lop, rop;

	CMDQ_LOG_PQ("%s handle:%p, buffer:%lx engine:%hu, secData.is_secure:%d\n",
		__func__, handle, (unsigned long)pa, engine, handle->secData.is_secure);

	if (pipe == 1) {
		idx_out = CMDQ_GPR_CNT_ID + CMDQ_GPR_R14;
		idx_out64 = CMDQ_GPR_CNT_ID + CMDQ_GPR_P7;
	}

	rb->start = pa;
	rb->count = 58;
	rb->engine = engine;
	rb->param = param;
	handle->readback_cnt++;
	handle->mdp_extension |= 1LL << DP_CMDEXT_HDR;

#if defined(CMDQ_SECURE_PATH_SUPPORT)
	/* secure pq readback is implemented in normal world */
	if (handle->secData.is_secure) {
		pkt = handle->pkt_rb;
		cmdq_op_wait_event_readback(handle, rb_event_lock);
	}
#else
	/* secure pq readback is implemented in secure world */
	if (handle->secData.is_secure)
		return;
#endif

	/* readback to this pa */
	cmdq_pkt_move(pkt, idx_out64 - CMDQ_GPR_CNT_ID, pa);

	/* counter init to 0 */
	cmdq_pkt_assign_command(pkt, idx_counter, 0);

	/* loop again here */
	begin_pa = cmdq_pkt_get_curr_buf_pa(pkt);

	/* read to value gpr */
	cmdq_pkt_read_addr(pkt, base + MDP_HDR_HIST_DATA, idx_val);
	/* write value src spr to dst gpr */
	cmdq_pkt_write_reg_indriect(pkt, idx_out64, idx_val, U32_MAX);

	/* jump forward end if match
	 * if spr1 >= 57 - 1
	 */
	lop.reg = true;
	lop.idx = idx_counter;
	rop.reg = false;
	rop.value =  MDP_HDR_HIST_CNT - 1;
	cmdq_pkt_assign_command(pkt, CMDQ_THR_SPR_IDX0, 0);
	condi_offset = pkt->cmd_buf_size - CMDQ_INST_SIZE;
	cmdq_pkt_cond_jump_abs(pkt, CMDQ_THR_SPR_IDX0, &lop, &rop,
		CMDQ_GREATER_THAN_AND_EQUAL);

	/* inc counter */
	lop.reg = true;
	lop.idx = idx_counter;
	rop.reg = false;
	rop.value = 1;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_counter, &lop, &rop);
	/* inc outut pa */
	lop.reg = true;
	lop.idx = idx_out;
	rop.reg = false;
	rop.value = 4;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, idx_out, &lop, &rop);

	cmdq_pkt_jump_addr(pkt, begin_pa);
	condi_inst = (u32 *)cmdq_pkt_get_va_by_offset(pkt, condi_offset);
	if (unlikely(!condi_inst)) {
		CMDQ_ERR("%s wrong offset %u\n", __func__, condi_offset);
		return;
	}
	if (condi_inst[1] == 0x10000001) {
		condi_inst = (u32 *)cmdq_pkt_get_va_by_offset(pkt,
			condi_offset + 8);
		if (unlikely(!condi_inst)) {
			CMDQ_ERR("%s wrong offset %u.\n", __func__, condi_offset);
			return;
		}
	}

	*condi_inst = (u32)CMDQ_REG_SHIFT_ADDR(cmdq_pkt_get_curr_buf_pa(pkt));

	pa = pa + MDP_HDR_HIST_CNT * 4;
	cmdq_pkt_mem_move(pkt, NULL, base + MDP_HDR_LBOX_DET_4, pa,
		CMDQ_THR_SPR_IDX3);

#if defined(CMDQ_SECURE_PATH_SUPPORT)
	if (handle->secData.is_secure)
		cmdq_op_set_event_readback(handle, rb_event_unlock);
#endif

	CMDQ_LOG_PQ("%s done, handle:%p, engine:%hu, mdp_extension:%x, readback_cnt:%d\n",
		__func__, handle, engine, handle->mdp_extension, handle->readback_cnt);

}

static s32 mdp_get_rdma_idx_virtual(u32 eng_base)
{
	CMDQ_ERR("%s no support\n", __func__);
	return -1;
}

static u16 mdp_get_reg_msb_offset_virtual(u32 eng_base, u16 offset)
{
	CMDQ_ERR("%s no support\n", __func__);
	return 0;
}

static bool mdp_check_camin_support_virtual(void)
{
	return true;
}

static bool mdp_vcp_pq_readback_support_virtual(void)
{
	return false;
}

void mdp_vcp_pq_readback_virtual(struct cmdqRecStruct *handle,
	u16 engine, u32 vcp_offset, u32 count)
{
	CMDQ_ERR("%s no support\n", __func__);
}

static bool mdp_svp_support_meta_data_virtual(void)
{
	return true;
}

void cmdq_mdp_virtual_function_setting(void)
{
	struct cmdqMDPFuncStruct *pFunc;

	pFunc = &mdp_funcs;

	pFunc->dumpMMSYSConfig = cmdq_mdp_dump_mmsys_config_virtual;

	pFunc->initModuleBaseVA = cmdq_mdp_init_module_base_VA_virtual;
	pFunc->deinitModuleBaseVA = cmdq_mdp_deinit_module_base_VA_virtual;

	pFunc->mdpClockIsOn = cmdq_mdp_clock_is_on_virtual;
	pFunc->enableMdpClock = cmdq_mdp_enable_clock_virtual;
	pFunc->initModuleCLK = cmdq_mdp_init_module_clk_virtual;

	pFunc->mdpDumpRsz = cmdq_mdp_dump_rsz_virtual;
	pFunc->mdpDumpTdshp = cmdq_mdp_dump_tdshp_virtual;

	pFunc->mdpClockOn = cmdqMdpClockOn_virtual;
	pFunc->mdpDumpInfo = cmdqMdpDumpInfo_virtual;
	pFunc->mdpResetEng = cmdqMdpResetEng_virtual;
	pFunc->mdpClockOff = cmdqMdpClockOff_virtual;
	pFunc->mdpIsModuleSuspend = mdp_engine_dummy;
	pFunc->mdpDumpEngineUsage = mdp_engine_dummy;

	pFunc->mdpIsMtee = mdp_check_handle_dummy;
	pFunc->mdpIsIspImg = mdp_check_handle_dummy;
	pFunc->mdpIsIspCamin = mdp_check_handle_dummy;
	pFunc->mdpInitialSet = cmdqMdpInitialSetting_virtual;

	pFunc->rdmaGetRegOffsetSrcAddr =
		cmdq_mdp_rdma_get_reg_offset_src_addr_virtual;
	pFunc->wrotGetRegOffsetDstAddr =
		cmdq_mdp_wrot_get_reg_offset_dst_addr_virtual;
	pFunc->wdmaGetRegOffsetDstAddr =
		cmdq_mdp_wdma_get_reg_offset_dst_addr_virtual;
	pFunc->testcaseClkmgrMdp = testcase_clkmgr_mdp_virtual;

	pFunc->dispatchModule = cmdq_mdp_dispatch_virtual;

	pFunc->trackTask = cmdq_mdp_trackTask_virtual;
	pFunc->parseErrModByEngFlag =
		cmdq_mdp_parse_handle_error_module_by_hwflag_virtual;
	pFunc->getEngineGroupBits = cmdq_mdp_get_engine_group_bits_virtual;
	pFunc->errorReset = cmdq_mdp_error_reset_virtual;
	pFunc->mdpEnableCommonClock = cmdq_mdp_enable_common_clock_virtual;
	pFunc->beginTask = cmdq_mdp_begin_task_virtual;
	pFunc->endTask = cmdq_mdp_end_task_virtual;
	pFunc->beginISPTask = cmdq_mdp_isp_begin_task_virtual;
	pFunc->endISPTask = cmdq_mdp_isp_end_task_virtual;
	pFunc->CheckHwStatus = cmdq_mdp_check_hw_status_virtual;
	pFunc->mdpGetSecEngine = cmdq_mdp_get_secure_engine_virtual;
	pFunc->resolve_token = cmdq_mdp_resolve_token_virtual;
	pFunc->mdpParseMod = mdp_parse_mod;

	pFunc->qosTransPort = cmdq_mdp_qos_translate_port_virtual;
	pFunc->qosInit = mdp_qos_init_virtual;
	pFunc->qosGetPath = mdp_qos_get_path_virtual;
	pFunc->qosClearAll = mdp_qos_clear_all_virtual;
	pFunc->qosClearAllIsp = mdp_qos_clear_all_virtual;
	pFunc->qosCheckBWLimit = mdp_qos_check_bw_limit_virtual;

	pFunc->getGroupMax = mdp_get_dummy;
	pFunc->getGroupIsp = mdp_get_dummy_isp;
	pFunc->getGroupMdp = mdp_get_dummy_mdp;
	pFunc->getGroupWpe = mdp_get_dummy_wpe;
	pFunc->getEngineGroupName = mdp_get_engine_group_name;
	pFunc->mdpComposeReadback = cmdq_mdp_compose_readback_virtual;
	pFunc->getEngineBase = mdp_get_engine_base_dummy;
	pFunc->getEngineBaseCount = mdp_get_engine_base_count_dummy;
	pFunc->mdpReadbackAal = mdp_readback_aal_virtual;
	pFunc->mdpReadbackHdr = mdp_readback_hdr_virtual;
	pFunc->getRDMAIndex = mdp_get_rdma_idx_virtual;
	pFunc->getRegMSBOffset = mdp_get_reg_msb_offset_virtual;
	pFunc->mdpIsCaminSupport = mdp_check_camin_support_virtual;
	pFunc->mdpVcpPQReadbackSupport = mdp_vcp_pq_readback_support_virtual;
	pFunc->mdpVcpPQReadback = mdp_vcp_pq_readback_virtual;
	pFunc->mdpSvpSupportMetaData = mdp_svp_support_meta_data_virtual;
	pFunc->mdpGetReadbackEventLock = mdp_get_rb_event_lock;
	pFunc->mdpGetReadbackEventUnlock = mdp_get_rb_event_unlock;
}

struct cmdqMDPFuncStruct *cmdq_mdp_get_func(void)
{
	return &mdp_funcs;
}

void cmdq_mdp_enable(u64 engineFlag, u32 engine)
{
#ifdef CMDQ_PWR_AWARE
	CMDQ_VERBOSE("Test for ENG %d\n", engine);
	if (engineFlag & (1LL << engine))
		cmdq_mdp_get_func()->enableMdpClock(true, engine);
#endif
}

int cmdq_mdp_loop_reset_impl(const unsigned long resetReg,
	const u32 resetWriteValue,
	const unsigned long resetStateReg,
	const u32 resetMask,
	const u32 resetPollingValue, const s32 maxLoopCount)
{
	u32 poll_value = 0;
	s32 ret;

	CMDQ_REG_SET32(resetReg, resetWriteValue);
	/* polling with 10ms timeout */
	ret = readl_poll_timeout_atomic((void *)resetStateReg, poll_value,
		(poll_value & resetMask) == resetPollingValue, 0, 10000);
	/* return polling result */
	if (ret == -ETIMEDOUT) {
		CMDQ_ERR(
			"%s failed Reg:0x%lx writeValue:0x%08x stateReg:0x%lx mask:0x%08x pollingValue:0x%08x\n",
			__func__, resetReg, resetWriteValue, resetStateReg,
			resetMask, resetPollingValue);
		dump_stack();
		return -EFAULT;
	}
	return 0;
}

int cmdq_mdp_loop_reset(u32 engine,
	const unsigned long resetReg,
	const unsigned long resetStateReg,
	const u32 resetMask,
	const u32 resetValue, const bool pollInitResult)
{
#ifdef CMDQ_PWR_AWARE
	int resetStatus = 0;
	int initStatus = 0;

	if (cmdq_mdp_get_func()->mdpClockIsOn(engine)) {
		CMDQ_PROF_START(current->pid, __func__);
		CMDQ_PROF_MMP(mdp_mmp_get_event()->MDP_reset,
			      MMPROFILE_FLAG_START, resetReg, resetStateReg);


		/* loop reset */
		resetStatus = cmdq_mdp_loop_reset_impl(resetReg, 0x1,
			resetStateReg, resetMask, resetValue,
			CMDQ_MAX_LOOP_COUNT);

		if (pollInitResult) {
			/* loop  init */
			initStatus = cmdq_mdp_loop_reset_impl(resetReg, 0x0,
				resetStateReg, resetMask, 0x0,
				CMDQ_MAX_LOOP_COUNT);
		} else {
			/* always clear to init state no matter what
			 * polling result
			 */
			CMDQ_REG_SET32(resetReg, 0x0);
		}

		CMDQ_PROF_MMP(mdp_mmp_get_event()->MDP_reset,
			      MMPROFILE_FLAG_END, resetReg, resetStateReg);
		CMDQ_PROF_END(current->pid, __func__);

		/* retrun failed if loop failed */
		if ((resetStatus < 0) || (initStatus < 0)) {
			CMDQ_ERR(
				"Reset MDP %d failed, resetStatus:%d, initStatus:%d\n",
				 engine, resetStatus, initStatus);
			return -EFAULT;
		}
	}
#endif

	return 0;
};

void cmdq_mdp_loop_off(u32 engine,
	const unsigned long resetReg,
	const unsigned long resetStateReg,
	const u32 resetMask,
	const u32 resetValue, const bool pollInitResult)
{
#ifdef CMDQ_PWR_AWARE
	int resetStatus = 0;
	int initStatus = 0;

	if (cmdq_mdp_get_func()->mdpClockIsOn(engine)) {

		/* loop reset */
		resetStatus = cmdq_mdp_loop_reset_impl(resetReg, 0x1,
			resetStateReg, resetMask, resetValue,
			CMDQ_MAX_LOOP_COUNT);

		if (pollInitResult) {
			/* loop init */
			initStatus = cmdq_mdp_loop_reset_impl(resetReg, 0x0,
				resetStateReg, resetMask, 0x0,
				CMDQ_MAX_LOOP_COUNT);
		} else {
			/* always clear to init state no matter what polling
			 * result
			 */
			CMDQ_REG_SET32(resetReg, 0x0);
		}

		cmdq_mdp_get_func()->enableMdpClock(false, engine);

		/* retrun failed if loop failed */
		if (resetStatus < 0 || initStatus < 0) {
			CMDQ_AEE("MDP",
				"Disable 0x%lx engine failed resetStatus:%d initStatus:%d\n",
				resetReg, resetStatus, initStatus);
		}
	}
#endif
}

void cmdq_mdp_dump_venc(const unsigned long base, const char *label)
{
	if (base == 0L) {
		/* print error message */
		CMDQ_ERR("venc base VA [0x%lx] is not correct\n", base);
		return;
	}

	CMDQ_ERR("======== %s + ========\n", __func__);
	CMDQ_ERR("[0x%lx] to [0x%lx]\n", base, base + 0x1000 * 4);

	print_hex_dump(KERN_ERR, "[CMDQ][ERR][VENC]", DUMP_PREFIX_ADDRESS,
		16, 4, (void *)base, 0x1000, false);
	CMDQ_ERR("======== %s - ========\n", __func__);
}

const char *cmdq_mdp_get_rdma_state(u32 state)
{
	switch (state) {
	case 0x1:
		return "idle";
	case 0x2:
		return "wait sof";
	case 0x4:
		return "reg update";
	case 0x8:
		return "clear0";
	case 0x10:
		return "clear1";
	case 0x20:
		return "int0";
	case 0x40:
		return "int1";
	case 0x80:
		return "data running";
	case 0x100:
		return "wait done";
	case 0x200:
		return "warm reset";
	case 0x400:
		return "wait reset";
	default:
		return "";
	}
}

void cmdq_mdp_dump_rdma(const unsigned long base, const char *label)
{
	u32 value[44] = { 0 };
	u32 state = 0;
	u32 grep = 0;


	if (gCmdqRdmaPrebuiltSupport) {
		CMDQ_LOG("========== [CMDQ] %s use cmdq_util_prebuilt_dump ==========\n", label);
		cmdq_util_prebuilt_dump(0, CMDQ_TOKEN_PREBUILT_MDP_WAIT);
	}

	value[0] = CMDQ_REG_GET32(base + 0x030);
	value[1] = CMDQ_REG_GET32(base +
		cmdq_mdp_get_func()->rdmaGetRegOffsetSrcAddr());
	value[2] = CMDQ_REG_GET32(base + 0x060);
	value[3] = CMDQ_REG_GET32(base + 0x070);
	value[4] = CMDQ_REG_GET32(base + 0x078);
	value[5] = CMDQ_REG_GET32(base + 0x080);
	value[6] = CMDQ_REG_GET32(base + 0x100);
	value[7] = CMDQ_REG_GET32(base + 0x118);
	value[8] = CMDQ_REG_GET32(base + 0x130);
	value[9] = CMDQ_REG_GET32(base + 0x400);
	value[10] = CMDQ_REG_GET32(base + 0x408);
	value[11] = CMDQ_REG_GET32(base + 0x410);
	value[12] = CMDQ_REG_GET32(base + 0x418);
	value[13] = CMDQ_REG_GET32(base + 0x420);
	value[14] = CMDQ_REG_GET32(base + 0x428);
	value[15] = CMDQ_REG_GET32(base + 0x430);
	value[16] = CMDQ_REG_GET32(base + 0x438);
	value[17] = CMDQ_REG_GET32(base + 0x440);
	value[18] = CMDQ_REG_GET32(base + 0x448);
	value[19] = CMDQ_REG_GET32(base + 0x450);
	value[20] = CMDQ_REG_GET32(base + 0x458);
	value[21] = CMDQ_REG_GET32(base + 0x460);
	value[22] = CMDQ_REG_GET32(base + 0x468);
	value[23] = CMDQ_REG_GET32(base + 0x470);
	value[24] = CMDQ_REG_GET32(base + 0x478);
	value[25] = CMDQ_REG_GET32(base + 0x480);
	value[26] = CMDQ_REG_GET32(base + 0x488);
	value[27] = CMDQ_REG_GET32(base + 0x490);
	value[28] = CMDQ_REG_GET32(base + 0x498);
	value[29] = CMDQ_REG_GET32(base + 0x4A0);
	value[30] = CMDQ_REG_GET32(base + 0x4A8);
	value[31] = CMDQ_REG_GET32(base + 0x4B0);
	value[32] = CMDQ_REG_GET32(base + 0x4B8);
	value[33] = CMDQ_REG_GET32(base + 0x4C0);
	value[34] = CMDQ_REG_GET32(base + 0x4C8);
	value[35] = CMDQ_REG_GET32(base + 0x4D0);
	value[36] = CMDQ_REG_GET32(base + 0x4D8);
	value[37] = CMDQ_REG_GET32(base + 0x4E0);
	value[38] = CMDQ_REG_GET32(base + 0x038);
	value[39] = CMDQ_REG_GET32(base + 0x068);
	value[40] = CMDQ_REG_GET32(base + 0x098);
	value[41] = CMDQ_REG_GET32(base + 0x148);
	value[42] = CMDQ_REG_GET32(base + 0x150);
	value[43] = CMDQ_REG_GET32(base + 0x0);

	CMDQ_ERR(
		"=============== [CMDQ] %s Status ====================================\n",
		label);
	CMDQ_ERR(
		"RDMA_SRC_CON: 0x%08x, RDMA_SRC_BASE_0: 0x%08x, RDMA_MF_BKGD_SIZE_IN_BYTE: 0x%08x\n",
		value[0], value[1], value[2]);
	CMDQ_ERR(
		"RDMA_MF_SRC_SIZE: 0x%08x, RDMA_MF_CLIP_SIZE: 0x%08x, RDMA_MF_OFFSET_1: 0x%08x\n",
		value[3], value[4], value[5]);
	CMDQ_ERR(
		"RDMA_SRC_END_0: 0x%08x, RDMA_SRC_OFFSET_0: 0x%08x, RDMA_SRC_OFFSET_W_0: 0x%08x\n",
		value[6], value[7], value[8]);
	CMDQ_ERR(
		"RDMA_MON_STA_0: 0x%08x, RDMA_MON_STA_1: 0x%08x, RDMA_MON_STA_2: 0x%08x\n",
		value[9], value[10], value[11]);
	CMDQ_ERR(
		"RDMA_MON_STA_3: 0x%08x, RDMA_MON_STA_4: 0x%08x, RDMA_MON_STA_5: 0x%08x\n",
		value[12], value[13], value[14]);
	CMDQ_ERR(
		"RDMA_MON_STA_6: 0x%08x, RDMA_MON_STA_7: 0x%08x, RDMA_MON_STA_8: 0x%08x\n",
		value[15], value[16], value[17]);
	CMDQ_ERR(
		"RDMA_MON_STA_9: 0x%08x, RDMA_MON_STA_10: 0x%08x, RDMA_MON_STA_11: 0x%08x\n",
		value[18], value[19], value[20]);
	CMDQ_ERR(
		"RDMA_MON_STA_12: 0x%08x, RDMA_MON_STA_13: 0x%08x, RDMA_MON_STA_14: 0x%08x\n",
		value[21], value[22], value[23]);
	CMDQ_ERR(
		"RDMA_MON_STA_15: 0x%08x, RDMA_MON_STA_16: 0x%08x, RDMA_MON_STA_17: 0x%08x\n",
		value[24], value[25], value[26]);
	CMDQ_ERR(
		"RDMA_MON_STA_18: 0x%08x, RDMA_MON_STA_19: 0x%08x, RDMA_MON_STA_20: 0x%08x\n",
		value[27], value[28], value[29]);
	CMDQ_ERR(
		"RDMA_MON_STA_21: 0x%08x, RDMA_MON_STA_22: 0x%08x, RDMA_MON_STA_23: 0x%08x\n",
		value[30], value[31], value[32]);
	CMDQ_ERR(
		"RDMA_MON_STA_24: 0x%08x, RDMA_MON_STA_25: 0x%08x, RDMA_MON_STA_26: 0x%08x\n",
		value[33], value[34], value[35]);
	CMDQ_ERR(
		"RDMA_MON_STA_27: 0x%08x, RDMA_MON_STA_28: 0x%08x, RDMA_COMP_CON: 0x%08x\n",
		value[36], value[37], value[38]);
	CMDQ_ERR(
		"MDP_RDMA_MF_BKGD_SIZE_IN_PXL: 0x%08x, MDP_RDMA_MF_BKGD_H_SIZE_IN_PXL: 0x%08x\n",
		value[39], value[40]);
	CMDQ_ERR(
		"MDP_RDMA_SRC_OFFSET_WP: 0x%08x, MDP_RDMA_SRC_OFFSET_HP: 0x%08x\n",
		value[41], value[42]);
	CMDQ_ERR("RDMA_EN: 0x%08x\n", value[43]);

	/* parse state */
	CMDQ_ERR("RDMA ack:%d req:%d ufo:%d\n", (value[9] >> 11) & 0x1,
		(value[9] >> 10) & 0x1, (value[9] >> 25) & 0x1);
	state = (value[10] >> 8) & 0x7FF;
	grep = (value[10] >> 20) & 0x1;
	CMDQ_ERR("RDMA state: 0x%x (%s)\n",
		state, cmdq_mdp_get_rdma_state(state));
	CMDQ_ERR("RDMA horz_cnt: %d vert_cnt:%d\n",
		value[35] & 0xFFF, (value[35] >> 16) & 0xFFF);

	CMDQ_ERR("RDMA grep:%d => suggest to ask SMI help:%d\n", grep, grep);
}

const char *cmdq_mdp_get_rsz_state(const u32 state)
{
	switch (state) {
	case 0x5:
		return "downstream hang";	/* 0,1,0,1 */
	case 0xa:
		return "upstream hang";	/* 1,0,1,0 */
	default:
		return "";
	}
}

void cmdq_mdp_dump_rot(const unsigned long base, const char *label)
{
	u32 value[50] = { 0 };

	value[0] = CMDQ_REG_GET32(base + 0x000);
	value[1] = CMDQ_REG_GET32(base + 0x008);
	value[2] = CMDQ_REG_GET32(base + 0x00C);
	value[3] = CMDQ_REG_GET32(base + 0x024);
	value[4] = CMDQ_REG_GET32(base +
		cmdq_mdp_get_func()->wrotGetRegOffsetDstAddr());
	value[5] = CMDQ_REG_GET32(base + 0x02C);
	value[6] = CMDQ_REG_GET32(base + 0x004);
	value[7] = CMDQ_REG_GET32(base + 0x030);
	value[8] = CMDQ_REG_GET32(base + 0x078);
	value[9] = CMDQ_REG_GET32(base + 0x070);
	CMDQ_REG_SET32(base + 0x018, 0x00000100);
	value[10] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00000200);
	value[11] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00000300);
	value[12] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00000400);
	value[13] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00000500);
	value[14] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00000600);
	value[15] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00000700);
	value[16] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00000800);
	value[17] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00000900);
	value[18] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00000A00);
	value[19] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00000B00);
	value[20] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00000C00);
	value[21] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00000D00);
	value[22] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00000E00);
	value[23] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00000F00);
	value[24] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00001000);
	value[25] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00001100);
	value[26] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00001200);
	value[27] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00001300);
	value[28] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00001400);
	value[29] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00001500);
	value[30] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00001600);
	value[31] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00001700);
	value[32] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00001800);
	value[33] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00001900);
	value[34] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00001A00);
	value[35] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00001B00);
	value[36] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00001C00);
	value[37] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00001D00);
	value[38] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00001E00);
	value[39] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00001F00);
	value[40] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00002000);
	value[41] = CMDQ_REG_GET32(base + 0x0D0);
	CMDQ_REG_SET32(base + 0x018, 0x00002100);
	value[42] = CMDQ_REG_GET32(base + 0x0D0);
	value[43] = CMDQ_REG_GET32(base + 0x01C);
	value[44] = CMDQ_REG_GET32(base + 0x07C);
	value[45] = CMDQ_REG_GET32(base + 0x010);
	value[46] = CMDQ_REG_GET32(base + 0x014);
	value[47] = CMDQ_REG_GET32(base + 0x0D8);
	value[48] = CMDQ_REG_GET32(base + 0x0E0);
	value[49] = CMDQ_REG_GET32(base + 0x028);

	CMDQ_ERR(
		"=============== [CMDQ] %s Status ====================================\n",
		label);
	CMDQ_ERR(
		"ROT_CTRL: 0x%08x, ROT_MAIN_BUF_SIZE: 0x%08x, ROT_SUB_BUF_SIZE: 0x%08x\n",
		value[0], value[1], value[2]);
	CMDQ_ERR(
		"ROT_TAR_SIZE: 0x%08x, ROT_BASE_ADDR: 0x%08x, ROT_OFST_ADDR: 0x%08x\n",
		value[3], value[4], value[5]);
	CMDQ_ERR(
		"ROT_DMA_PERF: 0x%08x, ROT_STRIDE: 0x%08x, ROT_IN_SIZE: 0x%08x\n",
		value[6], value[7], value[8]);
	CMDQ_ERR(
		"ROT_EOL: 0x%08x, ROT_DBUGG_1: 0x%08x, ROT_DEBUBG_2: 0x%08x\n",
		value[9], value[10], value[11]);
	CMDQ_ERR(
		"ROT_DBUGG_3: 0x%08x, ROT_DBUGG_4: 0x%08x, ROT_DEBUBG_5: 0x%08x\n",
		value[12], value[13], value[14]);
	CMDQ_ERR(
		"ROT_DBUGG_6: 0x%08x, ROT_DBUGG_7: 0x%08x, ROT_DEBUBG_8: 0x%08x\n",
		value[15], value[16], value[17]);
	CMDQ_ERR(
		"ROT_DBUGG_9: 0x%08x, ROT_DBUGG_A: 0x%08x, ROT_DEBUBG_B: 0x%08x\n",
		value[18], value[19], value[20]);
	CMDQ_ERR(
		"ROT_DBUGG_C: 0x%08x, ROT_DBUGG_D: 0x%08x, ROT_DEBUBG_E: 0x%08x\n",
		value[21], value[22], value[23]);
	CMDQ_ERR(
		"ROT_DBUGG_F: 0x%08x, ROT_DBUGG_10: 0x%08x, ROT_DEBUBG_11: 0x%08x\n",
		value[24], value[25], value[26]);
	CMDQ_ERR(
		"ROT_DEBUG_12: 0x%08x, ROT_DBUGG_13: 0x%08x, ROT_DBUGG_14: 0x%08x\n",
		value[27], value[28], value[29]);
	CMDQ_ERR(
		"ROT_DEBUG_15: 0x%08x, ROT_DBUGG_16: 0x%08x, ROT_DBUGG_17: 0x%08x\n",
		value[30], value[31], value[32]);
	CMDQ_ERR(
		"ROT_DEBUG_18: 0x%08x, ROT_DBUGG_19: 0x%08x, ROT_DBUGG_1A: 0x%08x\n",
		value[33], value[34], value[35]);
	CMDQ_ERR(
		"ROT_DEBUG_1B: 0x%08x, ROT_DBUGG_1C: 0x%08x, ROT_DBUGG_1D: 0x%08x\n",
		value[36], value[37], value[38]);
	CMDQ_ERR(
		"ROT_DEBUG_1E: 0x%08x, ROT_DBUGG_1F: 0x%08x, ROT_DBUGG_20: 0x%08x\n",
		value[39], value[40], value[41]);
	CMDQ_ERR(
		"ROT_DEBUG_21: 0x%08x, VIDO_INT: 0x%08x, VIDO_ROT_EN: 0x%08x\n",
		value[42], value[43], value[44]);
	CMDQ_ERR("VIDO_SOFT_RST: 0x%08x, VIDO_SOFT_RST_STAT: 0x%08x\n",
		value[45], value[46]);
	CMDQ_ERR(
		"VIDO_PVRIC: 0x%08x, VIDO_PENDING_ZERO: 0x%08x, VIDO_FRAME_SIZE: 0x%08x\n",
		value[47], value[48], value[49]);
}

void cmdq_mdp_dump_color(const unsigned long base, const char *label)
{
	u32 value[13] = { 0 };

	value[0] = CMDQ_REG_GET32(base + 0x400);
	value[1] = CMDQ_REG_GET32(base + 0x404);
	value[2] = CMDQ_REG_GET32(base + 0x408);
	value[3] = CMDQ_REG_GET32(base + 0x40C);
	value[4] = CMDQ_REG_GET32(base + 0x410);
	value[5] = CMDQ_REG_GET32(base + 0x420);
	value[6] = CMDQ_REG_GET32(base + 0xC00);
	value[7] = CMDQ_REG_GET32(base + 0xC04);
	value[8] = CMDQ_REG_GET32(base + 0xC08);
	value[9] = CMDQ_REG_GET32(base + 0xC0C);
	value[10] = CMDQ_REG_GET32(base + 0xC10);
	value[11] = CMDQ_REG_GET32(base + 0xC50);
	value[12] = CMDQ_REG_GET32(base + 0xC54);

	CMDQ_ERR(
		"=============== [CMDQ] %s Status ====================================\n",
		label);
	CMDQ_ERR("COLOR CFG_MAIN: 0x%08x\n", value[0]);
	CMDQ_ERR("COLOR PXL_CNT_MAIN: 0x%08x, LINE_CNT_MAIN: 0x%08x\n",
		value[1], value[2]);
	CMDQ_ERR(
		"COLOR WIN_X_MAIN: 0x%08x, WIN_Y_MAIN: 0x%08x, DBG_CFG_MAIN: 0x%08x\n",
		value[3], value[4],
		 value[5]);
	CMDQ_ERR("COLOR START: 0x%08x, INTEN: 0x%08x, INTSTA: 0x%08x\n",
		value[6], value[7], value[8]);
	CMDQ_ERR("COLOR OUT_SEL: 0x%08x, FRAME_DONE_DEL: 0x%08x\n",
		value[9], value[10]);
	CMDQ_ERR(
		"COLOR INTERNAL_IP_WIDTH: 0x%08x, INTERNAL_IP_HEIGHT: 0x%08x\n",
		value[11], value[12]);
}

const char *cmdq_mdp_get_wdma_state(u32 state)
{
	switch (state) {
	case 0x1:
		return "idle";
	case 0x2:
		return "clear";
	case 0x4:
		return "prepare";
	case 0x8:
		return "prepare";
	case 0x10:
		return "data running";
	case 0x20:
		return "eof wait";
	case 0x40:
		return "soft reset wait";
	case 0x80:
		return "eof done";
	case 0x100:
		return "sof reset done";
	case 0x200:
		return "frame complete";
	default:
		return "";
	}
}

void cmdq_mdp_dump_wdma(const unsigned long base, const char *label)
{
	u32 value[56] = { 0 };
	u32 state = 0;
	/* grep bit = 1, WDMA has sent request to SMI,
	 * and not receive done yet
	 */
	u32 grep = 0;
	u32 isFIFOFull = 0;	/* 1 for WDMA FIFO full */

	value[0] = CMDQ_REG_GET32(base + 0x014);
	value[1] = CMDQ_REG_GET32(base + 0x018);
	value[2] = CMDQ_REG_GET32(base + 0x028);
	value[3] = CMDQ_REG_GET32(base +
		cmdq_mdp_get_func()->wdmaGetRegOffsetDstAddr());
	value[4] = CMDQ_REG_GET32(base + 0x078);
	value[5] = CMDQ_REG_GET32(base + 0x080);
	value[6] = CMDQ_REG_GET32(base + 0x0A0);
	value[7] = CMDQ_REG_GET32(base + 0x0A8);

	CMDQ_REG_SET32(base + 0x014, (value[0] & (0x0FFFFFFF)));
	value[8] = CMDQ_REG_GET32(base + 0x014);
	value[9] = CMDQ_REG_GET32(base + 0x0AC);
	value[40] = CMDQ_REG_GET32(base + 0x0B8);
	CMDQ_REG_SET32(base + 0x014, 0x10000000 | (value[0] & (0x0FFFFFFF)));
	value[10] = CMDQ_REG_GET32(base + 0x014);
	value[11] = CMDQ_REG_GET32(base + 0x0AC);
	value[41] = CMDQ_REG_GET32(base + 0x0B8);
	CMDQ_REG_SET32(base + 0x014, 0x20000000 | (value[0] & (0x0FFFFFFF)));
	value[12] = CMDQ_REG_GET32(base + 0x014);
	value[13] = CMDQ_REG_GET32(base + 0x0AC);
	value[42] = CMDQ_REG_GET32(base + 0x0B8);
	CMDQ_REG_SET32(base + 0x014, 0x30000000 | (value[0] & (0x0FFFFFFF)));
	value[14] = CMDQ_REG_GET32(base + 0x014);
	value[15] = CMDQ_REG_GET32(base + 0x0AC);
	value[43] = CMDQ_REG_GET32(base + 0x0B8);
	CMDQ_REG_SET32(base + 0x014, 0x40000000 | (value[0] & (0x0FFFFFFF)));
	value[16] = CMDQ_REG_GET32(base + 0x014);
	value[17] = CMDQ_REG_GET32(base + 0x0AC);
	value[44] = CMDQ_REG_GET32(base + 0x0B8);
	CMDQ_REG_SET32(base + 0x014, 0x50000000 | (value[0] & (0x0FFFFFFF)));
	value[18] = CMDQ_REG_GET32(base + 0x014);
	value[19] = CMDQ_REG_GET32(base + 0x0AC);
	value[45] = CMDQ_REG_GET32(base + 0x0B8);
	CMDQ_REG_SET32(base + 0x014, 0x60000000 | (value[0] & (0x0FFFFFFF)));
	value[20] = CMDQ_REG_GET32(base + 0x014);
	value[21] = CMDQ_REG_GET32(base + 0x0AC);
	value[46] = CMDQ_REG_GET32(base + 0x0B8);
	CMDQ_REG_SET32(base + 0x014, 0x70000000 | (value[0] & (0x0FFFFFFF)));
	value[22] = CMDQ_REG_GET32(base + 0x014);
	value[23] = CMDQ_REG_GET32(base + 0x0AC);
	value[47] = CMDQ_REG_GET32(base + 0x0B8);
	CMDQ_REG_SET32(base + 0x014, 0x80000000 | (value[0] & (0x0FFFFFFF)));
	value[24] = CMDQ_REG_GET32(base + 0x014);
	value[25] = CMDQ_REG_GET32(base + 0x0AC);
	value[48] = CMDQ_REG_GET32(base + 0x0B8);
	CMDQ_REG_SET32(base + 0x014, 0x90000000 | (value[0] & (0x0FFFFFFF)));
	value[26] = CMDQ_REG_GET32(base + 0x014);
	value[27] = CMDQ_REG_GET32(base + 0x0AC);
	value[49] = CMDQ_REG_GET32(base + 0x0B8);
	CMDQ_REG_SET32(base + 0x014, 0xA0000000 | (value[0] & (0x0FFFFFFF)));
	value[28] = CMDQ_REG_GET32(base + 0x014);
	value[29] = CMDQ_REG_GET32(base + 0x0AC);
	value[50] = CMDQ_REG_GET32(base + 0x0B8);
	CMDQ_REG_SET32(base + 0x014, 0xB0000000 | (value[0] & (0x0FFFFFFF)));
	value[30] = CMDQ_REG_GET32(base + 0x014);
	value[31] = CMDQ_REG_GET32(base + 0x0AC);
	value[51] = CMDQ_REG_GET32(base + 0x0B8);
	CMDQ_REG_SET32(base + 0x014, 0xC0000000 | (value[0] & (0x0FFFFFFF)));
	value[32] = CMDQ_REG_GET32(base + 0x014);
	value[33] = CMDQ_REG_GET32(base + 0x0AC);
	value[52] = CMDQ_REG_GET32(base + 0x0B8);
	CMDQ_REG_SET32(base + 0x014, 0xD0000000 | (value[0] & (0x0FFFFFFF)));
	value[34] = CMDQ_REG_GET32(base + 0x014);
	value[35] = CMDQ_REG_GET32(base + 0x0AC);
	value[53] = CMDQ_REG_GET32(base + 0x0B8);
	CMDQ_REG_SET32(base + 0x014, 0xE0000000 | (value[0] & (0x0FFFFFFF)));
	value[36] = CMDQ_REG_GET32(base + 0x014);
	value[37] = CMDQ_REG_GET32(base + 0x0AC);
	value[54] = CMDQ_REG_GET32(base + 0x0B8);
	CMDQ_REG_SET32(base + 0x014, 0xF0000000 | (value[0] & (0x0FFFFFFF)));
	value[38] = CMDQ_REG_GET32(base + 0x014);
	value[39] = CMDQ_REG_GET32(base + 0x0AC);
	value[55] = CMDQ_REG_GET32(base + 0x0B8);

	CMDQ_ERR(
		"=============== [CMDQ] %s Status ====================================\n",
		label);
	CMDQ_ERR(
		"[CMDQ]WDMA_CFG: 0x%08x, WDMA_SRC_SIZE: 0x%08x, WDMA_DST_W_IN_BYTE = 0x%08x\n",
		value[0], value[1], value[2]);
	CMDQ_ERR(
		"[CMDQ]WDMA_DST_ADDR0: 0x%08x, WDMA_DST_UV_PITCH: 0x%08x, WDMA_DST_ADDR_OFFSET0 = 0x%08x\n",
		value[3], value[4], value[5]);
	CMDQ_ERR("[CMDQ]WDMA_STATUS: 0x%08x, WDMA_INPUT_CNT: 0x%08x\n",
		value[6], value[7]);

	/* Dump Addtional WDMA debug info */
	CMDQ_ERR("WDMA_DEBUG_0 +014: 0x%08x , +0ac: 0x%08x , +0b8: 0x%08x\n",
		value[8], value[9], value[40]);
	CMDQ_ERR("WDMA_DEBUG_1 +014: 0x%08x , +0ac: 0x%08x , +0b8: 0x%08x\n",
		value[10], value[11], value[41]);
	CMDQ_ERR("WDMA_DEBUG_2 +014: 0x%08x , +0ac: 0x%08x , +0b8: 0x%08x\n",
		value[12], value[13], value[42]);
	CMDQ_ERR("WDMA_DEBUG_3 +014: 0x%08x , +0ac: 0x%08x , +0b8: 0x%08x\n",
		value[14], value[15], value[43]);
	CMDQ_ERR("WDMA_DEBUG_4 +014: 0x%08x , +0ac: 0x%08x , +0b8: 0x%08x\n",
		value[16], value[17], value[44]);
	CMDQ_ERR("WDMA_DEBUG_5 +014: 0x%08x , +0ac: 0x%08x , +0b8: 0x%08x\n",
		value[18], value[19], value[45]);
	CMDQ_ERR("WDMA_DEBUG_6 +014: 0x%08x , +0ac: 0x%08x , +0b8: 0x%08x\n",
		value[20], value[21], value[46]);
	CMDQ_ERR("WDMA_DEBUG_7 +014: 0x%08x , +0ac: 0x%08x , +0b8: 0x%08x\n",
		value[22], value[23], value[47]);
	CMDQ_ERR("WDMA_DEBUG_8 +014: 0x%08x , +0ac: 0x%08x , +0b8: 0x%08x\n",
		value[24], value[25], value[48]);
	CMDQ_ERR("WDMA_DEBUG_9 +014: 0x%08x , +0ac: 0x%08x , +0b8: 0x%08x\n",
		value[26], value[27], value[49]);
	CMDQ_ERR("WDMA_DEBUG_A +014: 0x%08x , +0ac: 0x%08x , +0b8: 0x%08x\n",
		value[28], value[29], value[50]);
	CMDQ_ERR("WDMA_DEBUG_B +014: 0x%08x , +0ac: 0x%08x , +0b8: 0x%08x\n",
		value[30], value[31], value[51]);
	CMDQ_ERR("WDMA_DEBUG_C +014: 0x%08x , +0ac: 0x%08x , +0b8: 0x%08x\n",
		value[32], value[33], value[52]);
	CMDQ_ERR("WDMA_DEBUG_D +014: 0x%08x , +0ac: 0x%08x , +0b8: 0x%08x\n",
		value[34], value[35], value[53]);
	CMDQ_ERR("WDMA_DEBUG_E +014: 0x%08x , +0ac: 0x%08x , +0b8: 0x%08x\n",
		value[36], value[37], value[54]);
	CMDQ_ERR("WDMA_DEBUG_F +014: 0x%08x , +0ac: 0x%08x , +0b8: 0x%08x\n",
		value[38], value[39], value[55]);

	/* parse WDMA state */
	state = value[6] & 0x3FF;
	grep = (value[6] >> 13) & 0x1;
	isFIFOFull = (value[6] >> 12) & 0x1;

	CMDQ_ERR("WDMA state:0x%x (%s)\n",
		state, cmdq_mdp_get_wdma_state(state));
	CMDQ_ERR("WDMA in_req:%d in_ack:%d\n",
		(value[6] >> 15) & 0x1, (value[6] >> 14) & 0x1);

	/* note WDMA send request(i.e command) to SMI first,
	 * then SMI takes request data from WDMA FIFO
	 * if SMI dose not process request and upstream HWs
	 * such as MDP_RSZ send data to WDMA, WDMA FIFO will full finally
	 */
	CMDQ_ERR("WDMA grep:%d, FIFO full:%d\n", grep, isFIFOFull);
	CMDQ_ERR("WDMA suggest: Need SMI help:%d, Need check WDMA config:%d\n",
		grep, grep == 0 && isFIFOFull == 1);
}

void cmdq_mdp_check_TF_address(unsigned int mva, char *module)
{
	bool findTFTask = false;
	char *searchStr = NULL;
	char bufInfoKey[] = "x";
	char str2int[MDP_BUF_INFO_STR_LEN + 1] = "";
	char *callerNameEnd = NULL;
	char *callerNameStart = NULL;
	int callerNameLen = TASK_COMM_LEN;
	int taskIndex = 0;
	int bufInfoIndex = 0;
	int tfTaskIndex = -1;
	int planeIndex = 0;
	unsigned int bufInfo[MDP_PORT_BUF_INFO_NUM] = {0};
	unsigned int bufAddrStart = 0;
	unsigned int bufAddrEnd = 0;

	/* Call smi dump */
	CMDQ_ERR("[MDP] Dump SMI Info Begin\n");
	mtk_smi_dbg_hang_detect("MDP");
	CMDQ_ERR("[MDP] Dump SMI Info End\n");

	/* search track task */
	for (taskIndex = 0; taskIndex < MDP_MAX_TASK_NUM; taskIndex++) {
		searchStr = strpbrk(mdp_tasks[taskIndex].userDebugStr,
			bufInfoKey);
		bufInfoIndex = 0;

		/* catch buffer info in string and transform to integer
		 * bufInfo format:
		 * [address1, address2, address3, size1, size2, size3]
		 */
		while (searchStr != NULL && findTFTask != true) {
			strncpy(str2int, searchStr + 1, MDP_BUF_INFO_STR_LEN);
			if (kstrtoint(str2int, 16, &bufInfo[bufInfoIndex])) {
				CMDQ_ERR(
					"[MDP] buf info transform to integer failed\n");
				CMDQ_ERR("[MDP] fail string: %s\n", str2int);
			}

			searchStr = strpbrk(searchStr +
				MDP_BUF_INFO_STR_LEN + 1, bufInfoKey);
			bufInfoIndex++;

			/* check TF mva in this port or not */
			if (bufInfoIndex == MDP_PORT_BUF_INFO_NUM) {
				for (planeIndex = 0;
					planeIndex < MDP_MAX_PLANE_NUM;
					planeIndex++) {
					bufAddrStart = bufInfo[planeIndex];
					bufAddrEnd = bufAddrStart +
						bufInfo[planeIndex +
						MDP_MAX_PLANE_NUM];
					if (mva >= bufAddrStart &&
						mva < bufAddrEnd) {
						findTFTask = true;
						break;
					}
				}
				bufInfoIndex = 0;
			}
		}

		/* find TF task and keep task index */
		if (findTFTask == true) {
			tfTaskIndex = taskIndex;
			break;
		}
	}

	/* find TF task caller and return dispatch key */
	if (findTFTask == true) {
		CMDQ_ERR("[MDP] TF caller: %s\n",
			mdp_tasks[tfTaskIndex].callerName);
		CMDQ_ERR("%s\n", mdp_tasks[tfTaskIndex].userDebugStr);
		strncat(module, "_", 1);

		/* catch caller name only before - or _ */
		callerNameStart = mdp_tasks[tfTaskIndex].callerName;
		callerNameEnd = strchr(mdp_tasks[tfTaskIndex].callerName,
			'-');
		if (callerNameEnd != NULL)
			callerNameLen = callerNameEnd - callerNameStart;
		else {
			callerNameEnd = strchr(
				mdp_tasks[tfTaskIndex].callerName, '_');
			if (callerNameEnd != NULL)
				callerNameLen = callerNameEnd -
				callerNameStart;
		}
		strncat(module, mdp_tasks[tfTaskIndex].callerName,
			callerNameLen);
	}
	CMDQ_ERR("[MDP] TF Other Task\n");
	for (taskIndex = 0; taskIndex < MDP_MAX_TASK_NUM;
		taskIndex++) {
		CMDQ_ERR("[MDP] Task%d:\n", taskIndex);
		CMDQ_ERR("[MDP] Caller: %s\n",
			mdp_tasks[taskIndex].callerName);
		CMDQ_ERR("%s\n", mdp_tasks[taskIndex].userDebugStr);
	}
}

const char *cmdq_mdp_parse_handle_error_module_by_hwflag(
	const struct cmdqRecStruct *handle)
{
	return cmdq_mdp_get_func()->parseErrModByEngFlag(handle);
}

phys_addr_t cmdq_mdp_get_hw_reg(u32 base, u16 offset)
{
	u32 count;
	phys_addr_t *mdp_base;

	count = mdp_engine_base_count();
	mdp_base = mdp_engine_base_get();

	if (!count || !mdp_base) {
		CMDQ_ERR("%s count:%u base:%pa\n", __func__, count, &mdp_base);
		return 0;
	}

	if (offset > 0x1000) {
		CMDQ_ERR("%s: invalid offset:%#x\n", __func__, offset);
		return 0;
	}
	offset &= ~0x3;
	if (base >= count) {
		CMDQ_ERR("%s: invalid engine:%u, offset:%#x\n",
			__func__, base, offset);
		return 0;
	}

	return mdp_base[base] + offset;
}

phys_addr_t cmdq_mdp_get_hw_reg_msb(u32 base, u16 offset)
{
	u32 count;
	phys_addr_t *mdp_base;
	u16 reg_msb_offset;

	count = mdp_engine_base_count();
	mdp_base = mdp_engine_base_get();

	if (!count || !mdp_base) {
		CMDQ_ERR("%s count:%u base:%pa\n", __func__, count, &mdp_base);
		return 0;
	}

	if (base >= count) {
		CMDQ_ERR("%s: invalid engine:%u, offset:%#x\n", __func__, base, offset);
		return 0;
	}

	if (offset > 0x1000) {
		CMDQ_ERR("%s: invalid offset:%#x\n", __func__, offset);
		return 0;
	}

	reg_msb_offset = cmdq_mdp_get_func()->getRegMSBOffset(base, offset);

	return (mdp_base[base] + reg_msb_offset);
}


#if IS_ENABLED(CONFIG_MTK_IOMMU_V2)
u32 cmdq_mdp_get_hw_port(u32 base)
{
	if (base >= ENGBASE_COUNT) {
		CMDQ_ERR("%s: invalid engine:%u\n", __func__, base);
		return 0;
	}
	return mdp_engine_port[base];
}
#endif

s32 cmdq_mdp_get_rdma_idx(u32 base)
{
	return cmdq_mdp_get_func()->getRDMAIndex(base);
}

u32 cmdq_mdp_vcp_pq_readback_support(void)
{
	return cmdq_mdp_get_func()->mdpVcpPQReadbackSupport();
}

void cmdq_mdp_vcp_pq_readback(struct cmdqRecStruct *handle, u16 engine,
	u32 vcp_offset, u32 count)
{
	cmdq_mdp_get_func()->mdpVcpPQReadback(handle, engine, vcp_offset, count);
}

#ifdef MDP_COMMON_ENG_SUPPORT
void cmdq_mdp_platform_function_setting(void)
{
}
#endif

struct device *mdp_larb_dev_get(void)
{
	return mdp_ctx.larb;
}

static int mdp_loglevel_set(const char *val, const struct kernel_param *kp)
{
	int result, level;

	result = kstrtoint(val, 0, &level);
	if (result) {
		CMDQ_ERR("log level config fail:%d\n", result);
		return result;
	}

	if (level < 0 || level > CMDQ_LOG_LEVEL_MAX)
		level = 0;
	cmdq_core_set_log_level(level);

	return 0;
}

#define MAX_DUMP (PAGE_SIZE - 1)
static int mdp_loglevel_get(char *buf, const struct kernel_param *kp)
{
	return snprintf(buf, MAX_DUMP, "mdp log level:%d\n",
		cmdq_core_get_context()->logLevel);
}

static struct kernel_param_ops mdp_log_ops = {
	.set = mdp_loglevel_set,
	.get = mdp_loglevel_get,
};

module_param_cb(log_level, &mdp_log_ops, NULL, 0644);

static int mdp_error_get(char *buf, const struct kernel_param *kp)
{
	return cmdq_core_print_error(buf);
}

static struct kernel_param_ops mdp_error_ops = {
	.get = mdp_error_get,
};

module_param_cb(error, &mdp_error_ops, NULL, 0644);

static int mdp_profile_set(const char *val, const struct kernel_param *kp)
{
	int result, value;

	result = kstrtoint(val, 0, &value);
	if (result) {
		CMDQ_ERR("log level config fail:%d\n", result);
		return result;
	}

	if (value < 0 || value > CMDQ_PROFILE_MAX)
		value = 0;

	if (value == CMDQ_PROFILE_OFF)
		cmdq_core_get_context()->enableProfile = CMDQ_PROFILE_OFF;
	else
		cmdq_core_get_context()->enableProfile |= (1 << value);

	return 0;
}

static int mdp_profile_get(char *buf, const struct kernel_param *kp)
{
	return snprintf(buf, MAX_DUMP, "mdp profile:%d\n",
		cmdq_core_get_context()->enableProfile);
}

static struct kernel_param_ops mdp_profile_ops = {
	.set = mdp_profile_set,
	.get = mdp_profile_get,
};

module_param_cb(profile, &mdp_profile_ops, NULL, 0644);

MODULE_LICENSE("GPL v2");
