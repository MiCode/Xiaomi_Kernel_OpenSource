// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#include "cmdq_mdp_common.h"
#include "cmdq_device.h"
#include "cmdq_record.h"
#include "cmdq_reg.h"
#include "cmdq_sec.h"
#if IS_ENABLED(CONFIG_MMPROFILE)
#include "cmdq_mmp.h"
#endif
#ifdef CMDQ_COMMON_ENG_SUPPORT
#include "cmdq_engine_common.h"
#else
#include "cmdq_engine.h"
#endif
#ifdef CONFIG_MTK_SMI_EXT
#include "smi_public.h"
#endif

#include <linux/of.h>
#include <linux/slab.h>
#include <linux/pm_qos.h>
#include <linux/math64.h>
#include "cmdq_mdp_pmqos.h"
#ifdef CONFIG_MTK_SMI_EXT
#include <mmdvfs_pmqos.h>
#endif

#include "cmdq_helper_ext.h"

#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/iopoll.h>
#include <linux/notifier.h>
#include <linux/sched/clock.h>
#include <linux/pm_runtime.h>

enum mdp_platforms {
	MDP_MT6779 = 0,
	MDP_MT6761 = 1,
};

static struct pm_qos_request mdp_bw_qos_request[MDP_TOTAL_THREAD];
static struct pm_qos_request mdp_clk_qos_request[MDP_TOTAL_THREAD];
static struct pm_qos_request isp_bw_qos_request[MDP_TOTAL_THREAD];
static struct pm_qos_request isp_clk_qos_request[MDP_TOTAL_THREAD];
#ifdef CONFIG_MTK_SMI_EXT
static u64 g_freq_steps[MAX_FREQ_STEP];
static u32 step_size;
#endif

#define CMDQ_LOG_PMQOS(string, args...) \
do {			\
	if (cmdq_core_should_pmqos_log()) { \
		pr_notice("[CMDQ][MDP]"string, ##args); \
	} \
} while (0)


#define DP_TIMER_GET_DURATION_IN_US(start, end, duration)		\
do {									\
	u64 time1;							\
	u64 time2;							\
									\
	time1 = (u64)(start.tv_sec) * 1000000 +				\
		(u64)(start.tv_usec);					\
	time2 = (u64)(end.tv_sec) * 1000000   +				\
		(u64)(end.tv_usec);					\
									\
	duration = (s32)(time2 - time1);				\
									\
	if (duration <= 0)						\
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
static struct cmdqMDPFuncStruct mdp_funcs;
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
};
static struct mdp_context mdp_ctx;

static DEFINE_MUTEX(mdp_clock_mutex);
static DEFINE_MUTEX(mdp_task_mutex);
static DEFINE_MUTEX(mdp_thread_mutex);
static DEFINE_MUTEX(mdp_resource_mutex);

/* thread acquire notification */
static wait_queue_head_t mdp_thread_dispatch;

static struct notifier_block mdp_status_dump_notify;

/* use to generate [CMDQ_ENGINE_ENUM_id and name] mapping for status print */
#define CMDQ_FOREACH_MODULE_PRINT(ACTION)\
{		\
ACTION(CMDQ_ENG_ISP_IMGI,   ISP_IMGI)	\
ACTION(CMDQ_ENG_MDP_RDMA0,  MDP_RDMA0)	\
ACTION(CMDQ_ENG_MDP_RDMA1,  MDP_RDMA1)	\
ACTION(CMDQ_ENG_MDP_RSZ0,   MDP_RSZ0)	\
ACTION(CMDQ_ENG_MDP_RSZ1,   MDP_RSZ1)	\
ACTION(CMDQ_ENG_MDP_RSZ2,   MDP_RSZ2)	\
ACTION(CMDQ_ENG_MDP_TDSHP0, MDP_TDSHP0)	\
ACTION(CMDQ_ENG_MDP_TDSHP1, MDP_TDSHP1)	\
ACTION(CMDQ_ENG_MDP_COLOR0, MDP_COLOR0) \
ACTION(CMDQ_ENG_MDP_WROT0,  MDP_WROT0)	\
ACTION(CMDQ_ENG_MDP_WROT1,  MDP_WROT1)	\
ACTION(CMDQ_ENG_MDP_WDMA,   MDP_WDMA)	\
}

/* MDP common kernel logic */

struct EngineStruct *cmdq_mdp_get_engines(void)
{
	return mdp_ctx.engine;
}
EXPORT_SYMBOL(cmdq_mdp_get_engines);

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
EXPORT_SYMBOL(cmdq_mdp_fix_command_scenario_for_user_space);

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
EXPORT_SYMBOL(cmdq_mdp_is_request_from_user_space);

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
EXPORT_SYMBOL(cmdq_mdp_query_usage);

static void cmdq_mdp_common_clock_enable(void)
{
	s32 smi_ref = atomic_inc_return(&mdp_ctx.mdp_smi_usage);

	CMDQ_MSG("[CLOCK]MDP SMI clock enable %d\n", smi_ref);
	cmdq_mdp_get_func()->mdpEnableCommonClock(true);

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->MDP_clock_smi,
		MMPROFILE_FLAG_PULSE, smi_ref, 1);
}

static void cmdq_mdp_common_clock_disable(void)
{
	s32 smi_ref = atomic_dec_return(&mdp_ctx.mdp_smi_usage);

	CMDQ_MSG("[CLOCK]MDP SMI clock disable %d\n", smi_ref);
	cmdq_mdp_get_func()->mdpEnableCommonClock(false);

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->MDP_clock_smi,
		MMPROFILE_FLAG_PULSE, smi_ref, 0);
}

static s32 cmdq_mdp_clock_enable(u64 engine_flag)
{
	s32 ret;

	mutex_lock(&mdp_clock_mutex);

	CMDQ_MSG("[CLOCK]%s engine:0x%llx\n", __func__, engine_flag);

	/* common clock enable when get enabled engine,
	 * thus only enable mdp engine clocks.
	 */
	ret = cmdq_mdp_get_func()->mdpClockOn(engine_flag);

	mutex_unlock(&mdp_clock_mutex);

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->MDP_clock_on,
		MMPROFILE_FLAG_PULSE, (u32)(engine_flag >> 32),
		(u32)engine_flag);

	return ret;
}

static s32 cmdq_mdp_clock_disable(u64 engine_flag)
{
	s32 ret;

	CMDQ_MSG("[CLOCK]%s engine:0x%llx\n", __func__, engine_flag);

	mutex_lock(&mdp_clock_mutex);

	ret = cmdq_mdp_get_func()->mdpClockOff(engine_flag);

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->MDP_clock_off,
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
EXPORT_SYMBOL(cmdq_mdp_reset_resource);

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
EXPORT_SYMBOL(cmdq_mdp_enable_res);

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
EXPORT_SYMBOL(cmdq_mdp_lock_resource);

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
EXPORT_SYMBOL(cmdq_mdp_acquire_resource);

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
EXPORT_SYMBOL(cmdq_mdp_release_resource);

void cmdq_mdp_set_resource_callback(enum cmdq_event res_event,
	CmdqResourceAvailableCB res_available,
	CmdqResourceReleaseCB res_release)
{
	struct ResourceUnitStruct *res = NULL;

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
EXPORT_SYMBOL(cmdq_mdp_set_resource_callback);

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

	/* engine clocks enable flag decide here but call clock on before flush
	 * common clock enable here to avoid disable when mdp engines still
	 * need use for later tasks
	 */
	CMDQ_MSG("%s handle:0x%p pkt:0x%p engine:0x%016llx\n",
		__func__, handle, handle->pkt, handle->engineFlag);
	cmdq_mdp_common_clock_enable();

	CMDQ_PROF_START(current->pid, __func__);

	handle->engine_clk = cmdq_mdp_get_engine_flag_for_enable_clock(
		engine_flag, thread);

	/* make this thread can be dispath again */
	mdp_ctx.thread[thread].allow_dispatch = true;
	mdp_ctx.thread[thread].task_count++;

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
EXPORT_SYMBOL(cmdq_mdp_unlock_thread);

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
	cmdq_mdp_common_clock_disable();
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
			handle->secData.is_secure != mdp_ctx.thread[i].secure)
			return -EBUSY;
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
		return handle->ctrl->get_thread_id(handle->scenario);
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

	/* operation for tasks_wait list need task mutex */
	mutex_lock(&mdp_task_mutex);

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->consume_done, MMPROFILE_FLAG_START,
		current->pid, 0);

	/* loop waiting list for pending handles */
	list_for_each_entry_safe(handle, temp, &mdp_ctx.tasks_wait,
		list_entry) {
		/* operations for thread list need thread lock */
		mutex_lock(&mdp_thread_mutex);

		handle->thread = cmdq_mdp_find_free_thread(handle);
		if (handle->thread == CMDQ_INVALID_THREAD) {
			/* no available thread, keep wait */
			mutex_unlock(&mdp_thread_mutex);
			CMDQ_MSG(
				"fail to get thread handle:0x%p engine:0x%llx\n",
				handle, handle->engineFlag);
			continue;
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
		for (index = 0; index < CMDQ_MAX_GROUP_COUNT; ++index) {
			if (!callback[index].trackTask)
				continue;

			CMDQ_MSG("track task group %d with task:0x%p\n",
				index, handle);
			if (!cmdq_core_is_group_flag(
				(enum CMDQ_GROUP_ENUM)index,
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
			CMDQ_ERR("fail to flush handle:0x%p thread:%d\n",
				handle, handle->thread);
			continue;
		}

		/* some task is ready to run */
		acquired = true;
	}

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->consume_done, MMPROFILE_FLAG_END,
		current->pid, 0);

	mutex_unlock(&mdp_task_mutex);

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
		CMDQ_PROF_MMP(cmdq_mmp_get_event()->consume_add,
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
	u32 len;

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

static s32 cmdq_mdp_setup_sec(struct cmdqCommandStruct *desc,
	struct cmdqRecStruct *handle)
{
	u32 i;

	if (!desc->secData.is_secure)
		return 0;

	cmdq_task_set_secure(handle, desc->secData.is_secure);
	handle->secData.enginesNeedDAPC = desc->secData.enginesNeedDAPC;
	handle->secData.enginesNeedPortSecurity =
		desc->secData.enginesNeedPortSecurity;
	handle->secData.addrMetadataCount = desc->secData.addrMetadataCount;

	/* copy isp meta */
	handle->secData.ispMeta = desc->secData.ispMeta;

	/* clear isp buf since free in task destroy */
	for (i = 0; i < ARRAY_SIZE(desc->secData.ispMeta.ispBufs); i++)
		desc->secData.ispMeta.ispBufs[i].va = 0;

	if (handle->secData.addrMetadataCount > 0) {
		u32 metadata_length;
		void *p_metadatas;

		metadata_length = (handle->secData.addrMetadataCount) *
			sizeof(struct cmdqSecAddrMetadataStruct);
		/* create sec data task buffer for working */
		p_metadatas = kzalloc(metadata_length, GFP_KERNEL);
		if (p_metadatas) {
			memcpy(p_metadatas, CMDQ_U32_PTR(
				desc->secData.addrMetadatas), metadata_length);
			handle->secData.addrMetadatas =
				(cmdqU32Ptr_t)(unsigned long)p_metadatas;
		} else {
			CMDQ_AEE("CMDQ",
				"Can't alloc secData buffer count:%d alloacted_size:%d\n",
				 handle->secData.addrMetadataCount,
				 metadata_length);
			return -ENOMEM;
		}
	} else {
		handle->secData.addrMetadatas = 0;
	}

	return 0;
}

s32 cmdq_mdp_flush_async(struct cmdqCommandStruct *desc, bool user_space,
	struct cmdqRecStruct **handle_out)
{
	struct cmdqRecStruct *handle;
	struct task_private *private;
	s32 err;
	u32 copy_size;

	CMDQ_TRACE_FORCE_BEGIN("%s\n", __func__);

	cmdq_task_create(desc->scenario, &handle);

	/* set secure data */
	handle->secStatus = NULL;
	cmdq_mdp_setup_sec(desc, handle);

	handle->engineFlag = desc->engineFlag;
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

	copy_size = desc->blockSize - 2 * CMDQ_INST_SIZE;
	if (copy_size > 0) {
		err = cmdq_mdp_copy_cmd_to_task(handle,
			(void *)(unsigned long)desc->pVABase,
			copy_size, user_space);
		if (err < 0) {
			cmdq_task_destroy(handle);
			CMDQ_TRACE_FORCE_END();
			return err;
		}
	}

#ifdef CMDQ_CHECK_VALID
	if (!cmdq_core_check_pkt_valid(handle->pkt))
		return -EFAULT;
#endif

	if (desc->regRequest.count &&
			desc->regRequest.count <= CMDQ_MAX_DUMP_REG_COUNT &&
			desc->regRequest.regAddresses) {
		err = cmdq_task_append_backup_reg(handle,
			desc->regRequest.count,
			(u32 *)(unsigned long)desc->regRequest.regAddresses);
		if (err < 0) {
			cmdq_task_destroy(handle);
			CMDQ_TRACE_FORCE_END();
			return err;
		}
	}

#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT) || \
	defined(CONFIG_MTK_CAM_SECURITY_SUPPORT)
	if (handle->secData.is_secure) {
		/* insert backup cookie cmd */
		cmdq_sec_insert_backup_cookie_instr(handle, handle->thread);
	}
#endif

	if (handle->profile_exec)
		cmdq_pkt_perf_end(handle->pkt);

	err = cmdq_mdp_copy_cmd_to_task(handle,
		(void *)(unsigned long)desc->pVABase + copy_size,
		2 * CMDQ_INST_SIZE, user_space);
	if (err < 0) {
		cmdq_task_destroy(handle);
		CMDQ_SYSTRACE_END();
		return err;
	}

	/* mark finalized since we copy it */
	handle->finalized = true;

	/* assign handle for mdp */
	*handle_out = handle;

	/* Dispatch handle to get correct thread or wait in list.
	 * Task may flush directly if no engine conflict and no waiting task
	 * holds same engines.
	 */
	cmdq_mdp_flush_async_impl(handle);

	CMDQ_TRACE_FORCE_END();

	return 0;
}
EXPORT_SYMBOL(cmdq_mdp_flush_async);

s32 cmdq_mdp_flush_async_impl(struct cmdqRecStruct *handle)
{
	struct list_head *insert_pos = &mdp_ctx.tasks_wait;
	struct cmdqRecStruct *entry;

	CMDQ_VERBOSE("dispatch handle:0x%p\n", handle);

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
	mutex_lock(&mdp_task_mutex);
	list_for_each_entry(entry, &mdp_ctx.tasks_wait, list_entry) {
		if (entry->pkt->priority < handle->pkt->priority)
			break;
		insert_pos = &entry->list_entry;
	}
	list_add(&handle->list_entry, insert_pos);
	mutex_unlock(&mdp_task_mutex);

	/* run consume to run task in thread */
	cmdq_mdp_consume_handle();

	return 0;
}
EXPORT_SYMBOL(cmdq_mdp_flush_async_impl);

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
EXPORT_SYMBOL(cmdq_mdp_get_valid_handle);

s32 cmdq_mdp_wait(struct cmdqRecStruct *handle,
	struct cmdqRegValueStruct *results)
{
	s32 status, waitq;
	u32 i;
	u64 exec_cost;

	CMDQ_TRACE_FORCE_BEGIN("%s\n", __func__);

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
		/* clear results */
		memset(CMDQ_U32_PTR(results->regValues), 0,
			results->count * sizeof(CMDQ_U32_PTR(
			results->regValues)[0]));

		mutex_lock(&mdp_task_mutex);
		for (i = 0; i < results->count && i < handle->reg_count; i++)
			CMDQ_U32_PTR(results->regValues)[i] =
				handle->reg_values[i];
		mutex_unlock(&mdp_task_mutex);
	}

	/* consume again since maybe more conflict task in waiting */
	cmdq_mdp_add_consume_item();

	CMDQ_TRACE_FORCE_END();

	return status;
}
EXPORT_SYMBOL(cmdq_mdp_wait);

s32 cmdq_mdp_flush(struct cmdqCommandStruct *desc, bool user_space)
{
	struct cmdqRecStruct *handle;
	s32 status;

	status = cmdq_mdp_flush_async(desc, user_space, &handle);
	if (!handle) {
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
EXPORT_SYMBOL(cmdq_mdp_flush);

void cmdq_mdp_suspend(void)
{
	if (atomic_read(&mdp_ctx.mdp_smi_usage)) {
		CMDQ_ERR("%s smi clk usage:%d\n",
			__func__, (s32)atomic_read(&mdp_ctx.mdp_smi_usage));
		cmdq_mdp_dump_thread_usage();
		cmdq_mdp_dump_engine_usage();
	}
}

void cmdq_mdp_resume(void)
{
	/* during suspending, there may be queued tasks.
	 * we should process them if any.
	 */
	cmdq_mdp_add_consume_item();
}
EXPORT_SYMBOL(cmdq_mdp_resume);

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
		if (!(handle->state != TASK_STATE_IDLE &&
			handle->node_private == file_node &&
			cmdq_mdp_is_request_from_user_space(
			handle->scenario)))
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
		cmdq_pkt_release_handle(handle);
	}

	/* ask core to auto release by file node
	 * note the core may lock more mutex
	 */
	cmdq_core_release_handle_by_file_node(file_node);

	mutex_unlock(&mdp_task_mutex);
}
EXPORT_SYMBOL(cmdq_mdp_release_task_by_file_node);

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

void cmdq_mdp_dump_engine_usage(void)
{
	struct EngineStruct *engine;
	const enum CMDQ_ENG_ENUM engine_enum[] =
		CMDQ_FOREACH_MODULE_PRINT(GENERATE_ENUM);
	static const char *const engine_names[] =
		CMDQ_FOREACH_MODULE_PRINT(GENERATE_STRING);
	u32 i;

	CMDQ_ERR("====== Engine Usage =======\n");
	for (i = 0; i < ARRAY_SIZE(engine_enum); i++) {
		engine = &mdp_ctx.engine[engine_enum[i]];
		if (engine->userCount ||
			engine->currOwner != CMDQ_INVALID_THREAD ||
			engine->failCount || engine->resetCount)
			CMDQ_ERR("%s: count:%d owner:%d fail:%d reset:%d\n",
				engine_names[i], engine->userCount,
				engine->currOwner, engine->failCount,
				engine->resetCount);
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
EXPORT_SYMBOL(cmdq_mdp_dump_resource);

static s32 cmdq_mdp_dump_common(u64 engineFlag, int level)
{
	cmdq_mdp_dump_thread_usage();
	cmdq_mdp_dump_engine_usage();

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

static void cmdq_mdp_init_pmqos(void)
{
#ifdef CONFIG_MTK_SMI_EXT
	s32 i = 0;
	s32 result = 0;
	/* INIT_LIST_HEAD(&gCmdqMdpContext.mdp_tasks);*/

	for (i = 0; i < MDP_TOTAL_THREAD; i++) {
		pm_qos_add_request(&mdp_bw_qos_request[i],
			PM_QOS_MM_MEMORY_BANDWIDTH, PM_QOS_DEFAULT_VALUE);
		pm_qos_add_request(&mdp_clk_qos_request[i],
			PM_QOS_MDP_FREQ, PM_QOS_DEFAULT_VALUE);
		pm_qos_add_request(&isp_bw_qos_request[i],
			PM_QOS_MM_MEMORY_BANDWIDTH, PM_QOS_DEFAULT_VALUE);
		pm_qos_add_request(&isp_clk_qos_request[i],
			PM_QOS_IMG_FREQ, PM_QOS_DEFAULT_VALUE);
		snprintf(mdp_bw_qos_request[i].owner,
		  sizeof(mdp_bw_qos_request[i].owner) - 1, "mdp_bw_%d", i);
		snprintf(mdp_clk_qos_request[i].owner,
		  sizeof(mdp_clk_qos_request[i].owner) - 1, "mdp_clk_%d", i);
		snprintf(isp_bw_qos_request[i].owner,
		  sizeof(isp_bw_qos_request[i].owner) - 1, "isp_bw_%d", i);
		snprintf(isp_clk_qos_request[i].owner,
		  sizeof(isp_clk_qos_request[i].owner) - 1, "isp_clk_%d", i);
	}
	/* Call mmdvfs_qos_get_freq_steps to get supported frequency */
	result = mmdvfs_qos_get_freq_steps(PM_QOS_MDP_FREQ, &g_freq_steps[0],
			&step_size);

	if (g_freq_steps[0] == 0)
		g_freq_steps[0] = 457;
	if (result < 0)
		CMDQ_ERR("get MMDVFS freq steps failed, result: %d\n", result);
#endif
}

void cmdq_mdp_init(void)
{
	struct cmdqMDPFuncStruct *mdp_func = cmdq_mdp_get_func();

	CMDQ_LOG("%s\n", __func__);

	/* Register MDP callback */
	cmdqCoreRegisterCB(CMDQ_GROUP_MDP, cmdq_mdp_clock_enable,
		cmdq_mdp_dump_common, mdp_func->mdpResetEng,
		cmdq_mdp_clock_disable);

	cmdqCoreRegisterErrorResetCB(CMDQ_GROUP_MDP, mdp_func->errorReset);

	/* Register module dispatch callback */
	cmdqCoreRegisterDispatchModCB(CMDQ_GROUP_MDP,
		mdp_func->dispatchModule);

	/* Register restore task */
	cmdqCoreRegisterTrackTaskCB(CMDQ_GROUP_MDP, mdp_func->trackTask);

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
	cmdq_mdp_get_func()->mdpInitialSet();
	cmdq_mdp_init_pmqos();
}
EXPORT_SYMBOL(cmdq_mdp_init);

void cmdq_mdp_deinit(void)
{
	s32 i = 0;

	for (i = 0; i < MDP_TOTAL_THREAD; i++) {

		pm_qos_remove_request(&isp_bw_qos_request[i]);
		pm_qos_remove_request(&isp_clk_qos_request[i]);
		pm_qos_remove_request(&mdp_bw_qos_request[i]);
		pm_qos_remove_request(&mdp_clk_qos_request[i]);
	}
}
EXPORT_SYMBOL(cmdq_mdp_deinit);

/* Platform dependent function */

struct RegDef {
	int offset;
	const char *name;
};

void cmdq_mdp_dump_mmsys_config_virtual(void)
{
	/* Do Nothing */
}

/* VENC callback function */
s32 cmdqVEncDumpInfo_virtual(u64 engineFlag, int level)
{
	return 0;
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
bool cmdq_mdp_clock_is_on_virtual(enum CMDQ_ENG_ENUM engine)
{
	return false;
}

/* enable MDP clock  */
void cmdq_mdp_enable_clock_virtual(bool enable, enum CMDQ_ENG_ENUM engine)
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

/* MDP Initialization setting */
void cmdqMdpInitialSetting_virtual(void)
{
	/* Do Nothing */
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
	const char *module = NULL;
	const u32 ISP_ONLY[2] = {
		((1LL << CMDQ_ENG_ISP_IMGI) | (1LL << CMDQ_ENG_ISP_IMG2O)),
		((1LL << CMDQ_ENG_ISP_IMGI) | (1LL << CMDQ_ENG_ISP_IMG2O) |
		 (1LL << CMDQ_ENG_ISP_IMGO))
	};

	/* common part for both normal and secure path
	 * for JPEG scenario, use HW flag is sufficient
	 */
	if (handle->engineFlag & (1LL << CMDQ_ENG_JPEG_ENC))
		module = "JPGENC";
	else if (handle->engineFlag & (1LL << CMDQ_ENG_JPEG_DEC))
		module = "JPGDEC";
	else if ((ISP_ONLY[0] == handle->engineFlag) ||
		(ISP_ONLY[1] == handle->engineFlag))
		module = "DIP_ONLY";

	/* for secure path, use HW flag is sufficient */
	do {
		if (module != NULL)
			break;

		if (!handle->secData.is_secure) {
			/* normal path, need parse current running instruciton
			 * for more detail
			 */
			break;
		} else if (CMDQ_ENG_MDP_GROUP_FLAG(handle->engineFlag)) {
			module = "MDP";
			break;
		} else if (CMDQ_ENG_DPE_GROUP_FLAG(handle->engineFlag)) {
			module = "DPE";
			break;
		} else if (CMDQ_ENG_RSC_GROUP_FLAG(handle->engineFlag)) {
			module = "RSC";
			break;
		} else if (CMDQ_ENG_GEPF_GROUP_FLAG(handle->engineFlag)) {
			module = "GEPF";
			break;
		}

		module = "CMDQ";
	} while (0);

	return module;
}

const char *cmdq_mdp_parse_error_module_by_hwflag_virtual(
	const struct cmdqRecStruct *task)
{
	const char *module = NULL;
	const u32 ISP_ONLY[2] = {
		((1LL << CMDQ_ENG_ISP_IMGI) | (1LL << CMDQ_ENG_ISP_IMG2O)),
		((1LL << CMDQ_ENG_ISP_IMGI) | (1LL << CMDQ_ENG_ISP_IMG2O) |
		 (1LL << CMDQ_ENG_ISP_IMGO))
	};

	/* common part for both normal and secure path
	 * for JPEG scenario, use HW flag is sufficient
	 */
	if (task->engineFlag & (1LL << CMDQ_ENG_JPEG_ENC))
		module = "JPGENC";
	else if (task->engineFlag & (1LL << CMDQ_ENG_JPEG_DEC))
		module = "JPGDEC";
	else if ((ISP_ONLY[0] == task->engineFlag) ||
		(ISP_ONLY[1] == task->engineFlag))
		module = "DIP_ONLY";

	/* for secure path, use HW flag is sufficient */
	do {
		if (module != NULL)
			break;

		if (!task->secData.is_secure) {
			/* normal path, need parse current running instruciton
			 * for more detail
			 */
			break;
		} else if (CMDQ_ENG_MDP_GROUP_FLAG(task->engineFlag)) {
			module = "MDP";
			break;
		} else if (CMDQ_ENG_DPE_GROUP_FLAG(task->engineFlag)) {
			module = "DPE";
			break;
		} else if (CMDQ_ENG_RSC_GROUP_FLAG(task->engineFlag)) {
			module = "RSC";
			break;
		} else if (CMDQ_ENG_GEPF_GROUP_FLAG(task->engineFlag)) {
			module = "GEPF";
			break;
		}

		module = "CMDQ";
	} while (0);

	return module;
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
EXPORT_SYMBOL(cmdq_mdp_get_module_base_VA_MMSYS_CONFIG);

static void cmdq_mdp_enable_common_clock_virtual(bool enable)
{
#ifdef CMDQ_PWR_AWARE
#ifdef CONFIG_MTK_SMI_EXT
	if (enable) {
		/* Use SMI clock API */
		smi_bus_prepare_enable(SMI_LARB0, "CMDQ");
	} else {
		/* disable, reverse the sequence */
		smi_bus_disable_unprepare(SMI_LARB0, "CMDQ");
	}
#else
	if (enable)
		pm_runtime_get_sync(cmdq_dev_get());
	else
		pm_runtime_put_sync(cmdq_dev_get());
#endif /* CONFIG_MTK_SMI_EXT */
#endif	/* CMDQ_PWR_AWARE */
}

/* Common Code */

void cmdq_mdp_map_mmsys_VA(void)
{
	cmdq_mmsys_base = cmdq_dev_alloc_reference_VA_by_name("mmsys_config");
}
EXPORT_SYMBOL(cmdq_mdp_map_mmsys_VA);

void cmdq_mdp_unmap_mmsys_VA(void)
{
	cmdq_dev_free_module_base_VA(cmdq_mmsys_base);
}
EXPORT_SYMBOL(cmdq_mdp_unmap_mmsys_VA);

static void cmdq_mdp_isp_begin_task_virtual(struct cmdqRecStruct *handle,
	struct cmdqRecStruct **handle_list, u32 size)
{
#ifdef CONFIG_MTK_SMI_EXT
	struct mdp_pmqos *isp_curr_pmqos;
	struct mdp_pmqos_record *qos_cur_rec;
	struct timeval curr_time;
	s32 diff;
	u32 thread_id = handle->thread - MDP_THREAD_START;
	u32 max_throughput = 0;
	u32 curr_bandwidth = 0;

	if (!(handle->engineFlag & (1LL << CMDQ_ENG_ISP_IMGI) &&
		handle->engineFlag & (1LL << CMDQ_ENG_ISP_IMG2O))) {
		return;
	}
	/* implement for pass2 only task */
	CMDQ_MSG("enter %s handle:0x%p engine:0x%llx\n", __func__,
		handle, handle->engineFlag);
	qos_cur_rec = kzalloc(sizeof(struct mdp_pmqos_record),
		GFP_KERNEL);
	handle->user_private = qos_cur_rec;
	do_gettimeofday(&curr_time);

	if (!handle->prop_addr)
		return;

	isp_curr_pmqos = (struct mdp_pmqos *)handle->prop_addr;
	qos_cur_rec->submit_tm = curr_time;
	qos_cur_rec->end_tm.tv_sec = isp_curr_pmqos->tv_sec;
	qos_cur_rec->end_tm.tv_usec = isp_curr_pmqos->tv_usec;

	CMDQ_MSG(
		"[MDP]isp %d pixel, isp %d byte, submit %06ld us, end %06ld us\n",
		isp_curr_pmqos->isp_total_pixel,
		isp_curr_pmqos->isp_total_datasize,
		qos_cur_rec->submit_tm.tv_usec,
		qos_cur_rec->end_tm.tv_usec);

	DP_TIMER_GET_DURATION_IN_US(qos_cur_rec->submit_tm,
		qos_cur_rec->end_tm, diff);
	max_throughput = isp_curr_pmqos->isp_total_pixel / diff;

	if (max_throughput > g_freq_steps[0]) {
		DP_BANDWIDTH(isp_curr_pmqos->isp_total_datasize,
			isp_curr_pmqos->isp_total_pixel, g_freq_steps[0],
			curr_bandwidth);
		DP_BANDWIDTH(isp_curr_pmqos->isp_total_datasize,
			isp_curr_pmqos->isp_total_pixel,
			g_freq_steps[0], curr_bandwidth);
	} else {
		DP_BANDWIDTH(isp_curr_pmqos->isp_total_datasize,
			isp_curr_pmqos->isp_total_pixel, max_throughput,
			curr_bandwidth);
		DP_BANDWIDTH(isp_curr_pmqos->isp_total_datasize,
			isp_curr_pmqos->isp_total_pixel, max_throughput,
			curr_bandwidth);
	}

	CMDQ_MSG("[MDP]ISP only curr_bandwidth %d, max_throughput %d\n",
		curr_bandwidth, max_throughput);
	/*update bandwidth*/
	pm_qos_update_request(&isp_bw_qos_request[thread_id], curr_bandwidth);
	/*update clock*/
	pm_qos_update_request(&isp_clk_qos_request[thread_id], max_throughput);
#endif
}

static void cmdq_mdp_begin_task_virtual(struct cmdqRecStruct *handle,
	struct cmdqRecStruct **handle_list, u32 size)
{
#ifdef CONFIG_MTK_SMI_EXT
	struct mdp_pmqos *mdp_curr_pmqos;
	struct mdp_pmqos *mdp_list_pmqos;
	struct mdp_pmqos_record *qos_cur_rec;
	struct mdp_pmqos_record *qos_lst_rec;
	s32 i = 0;
	struct timeval curr_time;
	s32 numerator;
	s32 denominator;
	u32 thread_id = handle->thread - MDP_THREAD_START;
	u32 max_throughput = 0;
	u32 isp_curr_bandwidth = 0;
	u32 isp_data_size = 0;
	u32 isp_curr_pixel_size = 0;
	u32 mdp_curr_bandwidth = 0;
	u32 mdp_data_size = 0;
	u32 mdp_curr_pixel_size = 0;
	bool first_task = true;

	/* check engine status */
	cmdq_mdp_get_func()->CheckHwStatus(handle);

	qos_cur_rec =
		kzalloc(sizeof(struct mdp_pmqos_record), GFP_KERNEL);
	handle->user_private = qos_cur_rec;

	do_gettimeofday(&curr_time);

	CMDQ_MSG("enter %s with handle:0x%p engine:0x%llx thread:%u\n",
		__func__, handle, handle->engineFlag, handle->thread);

	if (!handle->prop_addr)
		return;

	mdp_curr_pmqos = (struct mdp_pmqos *)handle->prop_addr;
	qos_cur_rec->submit_tm = curr_time;
	qos_cur_rec->end_tm.tv_sec = mdp_curr_pmqos->tv_sec;
	qos_cur_rec->end_tm.tv_usec = mdp_curr_pmqos->tv_usec;

	CMDQ_LOG_PMQOS(
		"[MDP]mdp %d pixel, mdp %d byte, isp %d pixel, isp %d byte, submit %06ld us, end %06ld us\n",
		mdp_curr_pmqos->mdp_total_pixel,
		mdp_curr_pmqos->mdp_total_datasize,
		mdp_curr_pmqos->isp_total_pixel,
		mdp_curr_pmqos->isp_total_datasize,
		qos_cur_rec->submit_tm.tv_usec,
		qos_cur_rec->end_tm.tv_usec);

	if (size > 1) {/*handle_list includes the current task*/
		CMDQ_MSG("size %d thread_id = %d\n", size, thread_id);
		for (i = 0; i < size; i++) {
			struct cmdqRecStruct *curTask = handle_list[i];

			if (!curTask)
				continue;

			if (!curTask->user_private)
				continue;

			mdp_list_pmqos = (struct mdp_pmqos *)curTask->prop_addr;
			qos_lst_rec =
			    (struct mdp_pmqos_record *)curTask->user_private;

			if (first_task) {
				DP_TIMER_GET_DURATION_IN_US(
					qos_lst_rec->submit_tm,
					qos_lst_rec->end_tm, denominator);
				DP_TIMER_GET_DURATION_IN_US(
					qos_cur_rec->submit_tm,
					qos_lst_rec->end_tm, numerator);
				qos_lst_rec->mdp_throughput =
					(mdp_list_pmqos->mdp_total_pixel /
					numerator) -
					(mdp_list_pmqos->mdp_total_pixel /
					denominator);
				max_throughput =
					qos_lst_rec->mdp_throughput;
				mdp_data_size =
					mdp_list_pmqos->mdp_total_datasize;
				isp_data_size =
					mdp_list_pmqos->isp_total_datasize;
				mdp_curr_pixel_size =
					mdp_list_pmqos->mdp_total_pixel;
				isp_curr_pixel_size =
					mdp_list_pmqos->isp_total_pixel;
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
					qos_cur_rec->submit_tm,
					qos_lst_rec->end_tm, denominator);
				DP_TIMER_GET_DURATION_IN_US(
					qos_cur_rec->submit_tm,
					mdp_prev_record->end_tm, numerator);

				qos_lst_rec->mdp_throughput =
					(mdp_prev_record->mdp_throughput *
					numerator / denominator) +
					(mdp_list_pmqos->mdp_total_pixel /
					denominator);
				if (qos_lst_rec->mdp_throughput >
				max_throughput)
					max_throughput =
					qos_lst_rec->mdp_throughput;
			}
			CMDQ_MSG(
				"[MDP]list mdp %d pixel, mdp %d byte, isp %d pixel, isp %d byte, submit %06ld us, end %06ld us\n",
				mdp_curr_pixel_size, mdp_data_size,
				isp_curr_pixel_size, isp_data_size,
				qos_lst_rec->submit_tm.tv_usec,
				qos_lst_rec->end_tm.tv_usec);
		}

		if (max_throughput > g_freq_steps[0]) {
			DP_BANDWIDTH(mdp_data_size, mdp_curr_pixel_size,
				g_freq_steps[0], mdp_curr_bandwidth);
			DP_BANDWIDTH(isp_data_size, isp_curr_pixel_size,
				g_freq_steps[0], isp_curr_bandwidth);
		} else {
			DP_BANDWIDTH(mdp_data_size, mdp_curr_pixel_size,
				max_throughput, mdp_curr_bandwidth);
			DP_BANDWIDTH(isp_data_size, isp_curr_pixel_size,
				max_throughput, isp_curr_bandwidth);
		}
	} else {
		DP_TIMER_GET_DURATION_IN_US(qos_cur_rec->submit_tm,
			qos_cur_rec->end_tm, denominator);
		qos_cur_rec->mdp_throughput =
			mdp_curr_pmqos->mdp_total_pixel / denominator;

		if (qos_cur_rec->mdp_throughput > g_freq_steps[0]) {
			DP_BANDWIDTH(mdp_curr_pmqos->mdp_total_datasize,
				mdp_curr_pmqos->mdp_total_pixel,
				g_freq_steps[0], mdp_curr_bandwidth);
			DP_BANDWIDTH(mdp_curr_pmqos->isp_total_datasize,
				mdp_curr_pmqos->isp_total_pixel,
				g_freq_steps[0], isp_curr_bandwidth);
		} else {
			DP_BANDWIDTH(mdp_curr_pmqos->mdp_total_datasize,
				mdp_curr_pmqos->mdp_total_pixel,
				qos_cur_rec->mdp_throughput,
				mdp_curr_bandwidth);
			DP_BANDWIDTH(mdp_curr_pmqos->isp_total_datasize,
				mdp_curr_pmqos->isp_total_pixel,
				qos_cur_rec->mdp_throughput,
				isp_curr_bandwidth);
		}
		max_throughput = qos_cur_rec->mdp_throughput;
	}

	CMDQ_MSG(
		"[MDP]mdp_curr_bandwidth %d, isp_curr_bandwidth %d, max_throughput %d\n",
		mdp_curr_bandwidth, isp_curr_bandwidth, max_throughput);

	if (handle->engineFlag & (1LL << CMDQ_ENG_MDP_CAMIN)) {
		/*update bandwidth*/
		pm_qos_update_request(&isp_bw_qos_request[thread_id],
			isp_curr_bandwidth);
		/*update clock*/
		pm_qos_update_request(&isp_clk_qos_request[thread_id],
			max_throughput);
	}

	/*update bandwidth*/
	if (mdp_curr_pmqos->mdp_total_datasize)
		pm_qos_update_request(&mdp_bw_qos_request[thread_id],
			mdp_curr_bandwidth);

	/*update clock*/
	if (mdp_curr_pmqos->mdp_total_pixel)
		pm_qos_update_request(&mdp_clk_qos_request[thread_id],
			max_throughput);

#if IS_ENABLED(CONFIG_MTK_SMI_EXT) && IS_ENABLED(CONFIG_MACH_MT6771)
	smi_larb_mon_act_cnt();
#endif
#endif
}

static void cmdq_mdp_isp_end_task_virtual(struct cmdqRecStruct *handle,
	struct cmdqRecStruct **handle_list, u32 size)
{
#ifdef CONFIG_MTK_SMI_EXT
	struct mdp_pmqos *isp_curr_pmqos;
	u32 thread_id = handle->thread - MDP_THREAD_START;

	if (!(handle->engineFlag & (1LL << CMDQ_ENG_ISP_IMGI) &&
		handle->engineFlag & (1LL << CMDQ_ENG_ISP_IMG2O))) {
		return;
	}

	if (!handle->prop_addr)
		return;

	CMDQ_MSG("enter %s with handle:0x%p engine:0x%llx\n", __func__,
		handle, handle->engineFlag);
	isp_curr_pmqos = (struct mdp_pmqos *)handle->prop_addr;
	kfree(handle->user_private);
	handle->user_private = NULL;
	/*update bandwidth*/
	if (isp_curr_pmqos->isp_total_datasize)
		pm_qos_update_request(&isp_bw_qos_request[thread_id], 0);
	/*update clock*/
	if (isp_curr_pmqos->isp_total_pixel)
		pm_qos_update_request(&isp_clk_qos_request[thread_id], 0);
#endif
}

static void cmdq_mdp_end_task_virtual(struct cmdqRecStruct *handle,
	struct cmdqRecStruct **handle_list, u32 size)
{
#ifdef CONFIG_MTK_SMI_EXT
	struct mdp_pmqos *mdp_curr_pmqos;
	struct mdp_pmqos *mdp_list_pmqos;
	struct mdp_pmqos_record *qos_cur_rec;
	struct mdp_pmqos_record *qos_lst_rec;
	s32 i = 0;
	struct timeval curr_time;
	int32_t denominator;
	uint32_t thread_id = handle->thread - MDP_THREAD_START;
	uint32_t max_throughput = 0;
	uint32_t pre_throughput = 0;
	bool trigger = false;
	bool first_task = true;
	int32_t overdue;
	uint32_t isp_curr_bandwidth = 0;
	uint32_t isp_data_size = 0;
	uint32_t isp_curr_pixel_size = 0;
	uint32_t mdp_curr_bandwidth = 0;
	uint32_t mdp_data_size = 0;
	uint32_t mdp_curr_pixel_size = 0;
	bool update_isp_throughput = false;
	bool update_isp_bandwidth = false;

#if IS_ENABLED(CONFIG_MTK_SMI_EXT) && IS_ENABLED(CONFIG_MACH_MT6771)
	smi_larb_mon_act_cnt();
#endif
	do_gettimeofday(&curr_time);
	CMDQ_MSG("enter %s with handle:0x%p engine:0x%llx\n", __func__,
		handle, handle->engineFlag);

	if (!handle->prop_addr)
		return;

	mdp_curr_pmqos = (struct mdp_pmqos *)handle->prop_addr;
	qos_cur_rec = (struct mdp_pmqos_record *)handle->user_private;
	qos_cur_rec->submit_tm = curr_time;

	for (i = 0; i < size; i++) {
		struct cmdqRecStruct *curTask = handle_list[i];

		if (!curTask)
			continue;

		if (!curTask->user_private)
			continue;

		mdp_list_pmqos = (struct mdp_pmqos *)curTask->prop_addr;
		qos_lst_rec =
			(struct mdp_pmqos_record *)curTask->user_private;

		if (handle->engineFlag & (1LL << CMDQ_ENG_MDP_CAMIN))
			update_isp_throughput = true;

		if (first_task) {
			mdp_curr_pixel_size = mdp_list_pmqos->mdp_total_pixel;
			isp_curr_pixel_size = mdp_list_pmqos->isp_total_pixel;
			mdp_data_size = mdp_list_pmqos->mdp_total_datasize;
			isp_data_size = mdp_list_pmqos->isp_total_datasize;
			if (handle->engineFlag & (1LL << CMDQ_ENG_MDP_CAMIN))
				update_isp_bandwidth = true;

			first_task = false;
		}

		if (qos_cur_rec->mdp_throughput <
			qos_lst_rec->mdp_throughput) {
			if (max_throughput <
				qos_lst_rec->mdp_throughput) {
				max_throughput =
					qos_lst_rec->mdp_throughput;

				if (max_throughput > g_freq_steps[0]) {
					DP_BANDWIDTH(mdp_data_size,
						mdp_curr_pixel_size,
						g_freq_steps[0],
						mdp_curr_bandwidth);
					DP_BANDWIDTH(isp_data_size,
						isp_curr_pixel_size,
						g_freq_steps[0],
						isp_curr_bandwidth);
				} else {
					DP_BANDWIDTH(mdp_data_size,
						mdp_curr_pixel_size,
						max_throughput,
						mdp_curr_bandwidth);
					DP_BANDWIDTH(isp_data_size,
						isp_curr_pixel_size,
						max_throughput,
						isp_curr_bandwidth);
				}
				DP_TIMER_GET_DURATION_IN_US(
					qos_cur_rec->submit_tm,
					qos_lst_rec->end_tm, overdue);
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
		CMDQ_MSG("[MDP] curr submit %06ld us, end %06ld us\n",
			qos_cur_rec->submit_tm.tv_usec,
			qos_cur_rec->end_tm.tv_usec);
		for (i = 0; i < size; i++) {
			struct cmdqRecStruct *curTask = handle_list[i];

			if (!curTask)
				continue;

			if (!curTask->user_private)
				continue;

			mdp_list_pmqos = (struct mdp_pmqos *)curTask->prop_addr;
			qos_lst_rec = (struct mdp_pmqos_record *)
				curTask->user_private;

			if (curTask->engineFlag & (1LL << CMDQ_ENG_MDP_CAMIN))
				/*if there is any DL task in list*/
				update_isp_throughput = true;

			if (first_task) {
				DP_TIMER_GET_DURATION_IN_US(
					qos_lst_rec->submit_tm,
					qos_lst_rec->end_tm, denominator);
				qos_lst_rec->mdp_throughput =
					mdp_list_pmqos->mdp_total_pixel
					/ denominator;
				max_throughput =
					qos_lst_rec->mdp_throughput;
				mdp_data_size =
					mdp_list_pmqos->mdp_total_datasize;
				isp_data_size =
					mdp_list_pmqos->isp_total_datasize;
				mdp_curr_pixel_size =
					mdp_list_pmqos->mdp_total_pixel;
				isp_curr_pixel_size =
					mdp_list_pmqos->isp_total_pixel;

				if (curTask->engineFlag &
					(1LL << CMDQ_ENG_MDP_CAMIN))
					/*next task is diect link*/
					update_isp_bandwidth = true;

				first_task = false;
				pre_throughput = max_throughput;
			} else {
				if (!pre_throughput)
					continue;
				DP_TIMER_GET_DURATION_IN_US(
					qos_cur_rec->submit_tm,
					qos_lst_rec->end_tm, denominator);
				qos_lst_rec->mdp_throughput =
					pre_throughput +
					(mdp_list_pmqos->mdp_total_pixel
					/ denominator);
				pre_throughput =
					qos_lst_rec->mdp_throughput;
				if (qos_lst_rec->mdp_throughput >
					max_throughput)
					max_throughput =
						qos_lst_rec->mdp_throughput;
			}
			CMDQ_MSG(
				"[MDP]list mdp %d MHz, mdp %d pixel, mdp %d byte, mdp %d pixel, isp %d byte, submit %06ld us, end %06ld us\n",
				qos_lst_rec->mdp_throughput,
				mdp_list_pmqos->mdp_total_pixel,
				mdp_list_pmqos->mdp_total_datasize,
				mdp_list_pmqos->isp_total_pixel,
				mdp_list_pmqos->isp_total_datasize,
				qos_lst_rec->submit_tm.tv_usec,
				qos_lst_rec->end_tm.tv_usec);
		}

		if (max_throughput > g_freq_steps[0]) {
			DP_BANDWIDTH(mdp_data_size, mdp_curr_pixel_size,
				g_freq_steps[0], mdp_curr_bandwidth);
			DP_BANDWIDTH(isp_data_size, isp_curr_pixel_size,
				g_freq_steps[0], isp_curr_bandwidth);
		} else {
			DP_BANDWIDTH(mdp_data_size, mdp_curr_pixel_size,
				max_throughput, mdp_curr_bandwidth);
			DP_BANDWIDTH(isp_data_size, isp_curr_pixel_size,
				max_throughput, isp_curr_bandwidth);
		}
	}
	CMDQ_MSG(
		"[MDP]mdp_curr_bandwidth %d, isp_curr_bandwidth %d, max_throughput %d\n",
		mdp_curr_bandwidth, isp_curr_bandwidth, max_throughput);

	kfree(handle->user_private);
	handle->user_private = NULL;

	if (update_isp_throughput) {
		/*update clock*/
		pm_qos_update_request(&isp_clk_qos_request[thread_id],
		max_throughput);
	} else {
		/*update clock*/
		if (mdp_curr_pmqos->isp_total_pixel)
			pm_qos_update_request(&isp_clk_qos_request[thread_id],
			0);
	}
	if (update_isp_bandwidth) {
		/*update bandwidth*/
		pm_qos_update_request(&isp_bw_qos_request[thread_id],
		isp_curr_bandwidth);
	} else {
		/*update bandwidth*/
		if (mdp_curr_pmqos->isp_total_datasize)
			pm_qos_update_request(&isp_bw_qos_request[thread_id],
			0);
	}
	if (mdp_curr_pmqos->mdp_total_datasize)
		pm_qos_update_request(&mdp_bw_qos_request[thread_id],
		mdp_curr_bandwidth);

	/*update clock*/
	if (mdp_curr_pmqos->mdp_total_pixel)
		pm_qos_update_request(&mdp_clk_qos_request[thread_id],
		max_throughput);
#endif
}

static void cmdq_mdp_check_hw_status_virtual(struct cmdqRecStruct *handle)
{
	/* Do nothing */
}

u64 cmdq_mdp_get_secure_engine_virtual(u64 engine_flag)
{
	return 0;
}

void cmdq_mdp_virtual_function_setting(void)
{
	struct cmdqMDPFuncStruct *pFunc;

	pFunc = &mdp_funcs;

	pFunc->dumpMMSYSConfig = cmdq_mdp_dump_mmsys_config_virtual;

	pFunc->vEncDumpInfo = cmdqVEncDumpInfo_virtual;

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
		cmdq_mdp_parse_error_module_by_hwflag_virtual;
	pFunc->parseHandleErrModByEngFlag =
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
}
EXPORT_SYMBOL(cmdq_mdp_virtual_function_setting);

struct cmdqMDPFuncStruct *cmdq_mdp_get_func(void)
{
	return &mdp_funcs;
}
EXPORT_SYMBOL(cmdq_mdp_get_func);

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
EXPORT_SYMBOL(cmdq_mdp_loop_reset_impl);

int cmdq_mdp_loop_reset(enum CMDQ_ENG_ENUM engine,
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
		CMDQ_PROF_MMP(cmdq_mmp_get_event()->MDP_reset,
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

		CMDQ_PROF_MMP(cmdq_mmp_get_event()->MDP_reset,
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
EXPORT_SYMBOL(cmdq_mdp_loop_reset);

void cmdq_mdp_loop_off(enum CMDQ_ENG_ENUM engine,
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
EXPORT_SYMBOL(cmdq_mdp_loop_off);

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
EXPORT_SYMBOL(cmdq_mdp_dump_venc);

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
EXPORT_SYMBOL(cmdq_mdp_get_rdma_state);

void cmdq_mdp_dump_rdma(const unsigned long base, const char *label)
{
	u32 value[17] = { 0 };
	u32 state = 0;
	u32 grep = 0;

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
	value[12] = CMDQ_REG_GET32(base + 0x420);
	value[13] = CMDQ_REG_GET32(base + 0x430);
	value[14] = CMDQ_REG_GET32(base + 0x440);
	value[15] = CMDQ_REG_GET32(base + 0x4D0);
	value[16] = CMDQ_REG_GET32(base + 0x0);

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
		"RDMA_MON_STA_4: 0x%08x, RDMA_MON_STA_6: 0x%08x, RDMA_MON_STA_8: 0x%08x\n",
		value[12], value[13], value[14]);
	CMDQ_ERR("RDMA_MON_STA_26: 0x%08x\n", value[15]);
	CMDQ_ERR("RDMA_EN: 0x%08x\n", value[16]);

	/* parse state */
	CMDQ_ERR("RDMA ack:%d req:%d\n", (value[9] & (1 << 11)) >> 11,
		(value[9] & (1 << 10)) >> 10);
	state = (value[10] >> 8) & 0x7FF;
	grep = (value[10] >> 20) & 0x1;
	CMDQ_ERR("RDMA state: 0x%x (%s)\n",
		state, cmdq_mdp_get_rdma_state(state));
	CMDQ_ERR("RDMA horz_cnt: %d vert_cnt:%d\n",
		value[15] & 0xFFF, (value[15] >> 16) & 0xFFF);

	CMDQ_ERR("RDMA grep:%d => suggest to ask SMI help:%d\n", grep, grep);
}
EXPORT_SYMBOL(cmdq_mdp_dump_rdma);

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
EXPORT_SYMBOL(cmdq_mdp_get_rsz_state);

void cmdq_mdp_dump_rot(const unsigned long base, const char *label)
{
	u32 value[47] = { 0 };

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
	CMDQ_ERR("ROT_DEBUG_21: 0x%08x\n", value[42]);
	CMDQ_ERR("VIDO_INT: 0x%08x, VIDO_ROT_EN: 0x%08x\n",
		value[43], value[44]);
	CMDQ_ERR("VIDO_SOFT_RST: 0x%08x, VIDO_SOFT_RST_STAT: 0x%08x\n",
		value[45], value[46]);
}
EXPORT_SYMBOL(cmdq_mdp_dump_rot);

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
EXPORT_SYMBOL(cmdq_mdp_dump_color);

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
EXPORT_SYMBOL(cmdq_mdp_get_wdma_state);

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
EXPORT_SYMBOL(cmdq_mdp_dump_wdma);

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
EXPORT_SYMBOL(cmdq_mdp_check_TF_address);

const char *cmdq_mdp_parse_handle_error_module_by_hwflag(
	const struct cmdqRecStruct *handle)
{
	return cmdq_mdp_get_func()->parseHandleErrModByEngFlag(handle);
}
EXPORT_SYMBOL(cmdq_mdp_parse_handle_error_module_by_hwflag);

void cmdq_mdp_set_platform(struct device *dev)
{
	u32 mdp_plat;
	int ret;

	ret = of_property_read_u32(dev->of_node, "mdp-platform", &mdp_plat);
	if (ret < 0) {
		CMDQ_LOG("no mdp platform support\n");
		return;
	}

	switch (mdp_plat) {
	case MDP_MT6761:
		cmdq_mdp_set_mt6761();
		break;
	case MDP_MT6779:
		cmdq_mdp_set_mt6779();
		break;
	default:
		CMDQ_ERR("unknown mdp platform id:%u\n", mdp_plat);
		break;
	}
}
EXPORT_SYMBOL(cmdq_mdp_set_platform);
