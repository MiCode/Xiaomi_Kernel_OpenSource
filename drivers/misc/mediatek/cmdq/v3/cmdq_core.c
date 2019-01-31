/*
 * Copyright (C) 2015 MediaTek Inc.
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
#include "cmdq_core.h"
#include "cmdq_virtual.h"
#include "cmdq_reg.h"
#include "cmdq_struct.h"
#include "cmdq_device.h"
#include "cmdq_record.h"
#include "cmdq_sec.h"
#ifdef CMDQ_PROFILE_MMP
#include "cmdq_mmp.h"
#endif
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>
#include <linux/atomic.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/memory.h>
#include <linux/ftrace.h>
#include <linux/seq_file.h>
#include <linux/kthread.h>
#include <linux/math64.h>
#include <mt-plat/mtk_lpae.h>
#include "cmdq_record_private.h"

/* #define CMDQ_PROFILE_COMMAND_TRIGGER_LOOP */
/* #define CMDQ_ENABLE_BUS_ULTRA */

#define CMDQ_GET_COOKIE_CNT(thread) \
	(CMDQ_REG_GET32(CMDQ_THR_EXEC_CNT(thread)) & CMDQ_MAX_COOKIE_VALUE)
#define CMDQ_SYNC_TOKEN_APPEND_THR(id)     (CMDQ_SYNC_TOKEN_APPEND_THR0 + id)
#define CMDQ_PROFILE_LIMIT_0	3000000
#define CMDQ_PROFILE_LIMIT_1	10000000
#define CMDQ_PROFILE_LIMIT_2	20000000

#ifdef CMDQ_PROFILE_LOCK
static CMDQ_TIME cost_gCmdqTaskMutex;
static CMDQ_TIME cost_gCmdqClockMutex;
static CMDQ_TIME cost_gCmdqErrMutex;
static bool print_lock_cost;

#define CMDQ_PROF_MUTEX_LOCK(lock, tag)	\
{					\
CMDQ_TIME lock_cost_##tag;		\
lock_cost_##tag = sched_clock();	\
print_lock_cost = true;			\
mutex_lock(&lock);			\
print_lock_cost = false;		\
lock_cost_##tag = sched_clock() - lock_cost_##tag;	\
if (lock_cost_##tag > CMDQ_PROFILE_LIMIT_2) {		\
	CMDQ_MSG("[warn][%s]wait lock:%llu us > %ums\n",	\
		#tag, div_s64(lock_cost_##tag, 1000),		\
		CMDQ_PROFILE_LIMIT_2/1000000);		\
}					\
cost_##lock = sched_clock();		\
}

#define CMDQ_PROF_MUTEX_UNLOCK(lock, tag)	\
{						\
bool force_print_lock = print_lock_cost;	\
cost_##lock = sched_clock() - cost_##lock;	\
if (cost_##lock > CMDQ_PROFILE_LIMIT_1) {	\
	CMDQ_MSG("[warn][%s]lock cost:%llu us > %ums\n",	\
	#tag, div_s64(cost_##lock, 1000),		\
		CMDQ_PROFILE_LIMIT_1/1000000);	\
} else if (force_print_lock) {			\
	CMDQ_MSG("[%s]lock cost:%llu us < %ums\n",	\
	#tag, div_s64(cost_##lock, 1000),		\
		CMDQ_PROFILE_LIMIT_1/1000000);	\
}						\
mutex_unlock(&lock);				\
}

#define CMDQ_PROF_TIME_BEGIN(cost)	\
{					\
cost = sched_clock();			\
}

#define CMDQ_PROF_TIME_END(cost, tag)	\
{					\
cost = sched_clock() - cost;		\
if (cost > CMDQ_PROFILE_LIMIT_1) {	\
	CMDQ_MSG("[warn][%s] t_cost %llu > %ums\n",	\
		tag, div_s64(cost, 1000),			\
		CMDQ_PROFILE_LIMIT_1/1000000);	\
}					\
cost = sched_clock();			\
}

static CMDQ_TIME cost_gCmdqExecLock;
static CMDQ_TIME cost_gCmdqWriteAddrLock;
static CMDQ_TIME cost_gCmdqThreadLock;
static CMDQ_TIME cost_gCmdqRecordLock;
static CMDQ_TIME cost_g_delay_thread_lock;

#define CMDQ_PROF_SPIN_LOCK(lock, flags, tag)	\
{						\
spin_lock_irqsave(&lock, flags);		\
cost_##lock = sched_clock();			\
}

#define CMDQ_PROF_SPIN_UNLOCK(lock, flags, tag)	\
{						\
cost_##lock = sched_clock() - cost_##lock;	\
if (cost_##lock > CMDQ_PROFILE_LIMIT_0) {	\
CMDQ_MSG("[warn][%s]spin lock cost:%llu > %ums\n",	\
	#tag, (u64)div_s64(cost_##lock, 1000),		\
	(u32)(CMDQ_PROFILE_LIMIT_0/1000000));	\
}						\
spin_unlock_irqrestore(&lock, flags);		\
}

#else

#define CMDQ_PROF_MUTEX_LOCK(lock, tag)	\
{					\
mutex_lock(&lock);			\
}

#define CMDQ_PROF_MUTEX_UNLOCK(lock, tag)	\
{						\
mutex_unlock(&lock);				\
}

#define CMDQ_PROF_TIME_BEGIN(cost)	\
{					\
cost = sched_clock();			\
}

#define CMDQ_PROF_TIME_END(cost, tag)	\
{					\
}

#define CMDQ_PROF_SPIN_LOCK(lock, flags, tag)	\
{						\
spin_lock_irqsave(&lock, flags);		\
}

#define CMDQ_PROF_SPIN_UNLOCK(lock, flags, tag)	\
{						\
spin_unlock_irqrestore(&lock, flags);		\
}
#endif

/* use mutex because we don't access task list in IRQ */
/* and we may allocate memory when create list items */
static DEFINE_MUTEX(gCmdqTaskMutex);
static DEFINE_MUTEX(gCmdqClockMutex);

static DEFINE_MUTEX(gCmdqSaveBufferMutex);
/* static DEFINE_MUTEX(gCmdqWriteAddrMutex); */
static DEFINE_SPINLOCK(gCmdqWriteAddrLock);

#if defined(CMDQ_SECURE_PATH_SUPPORT) && !defined(CMDQ_SECURE_PATH_NORMAL_IRQ)
/* ensure atomic start/stop notify loop*/
static DEFINE_MUTEX(gCmdqNotifyLoopMutex);
#endif

/* These may access in IRQ so use spin lock. */
static DEFINE_SPINLOCK(gCmdqThreadLock);
static atomic_t gCmdqThreadUsage;
static atomic_t gSMIThreadUsage;
static bool gCmdqSuspended;
static DEFINE_SPINLOCK(gCmdqExecLock);
static DEFINE_SPINLOCK(gCmdqRecordLock);
static DEFINE_MUTEX(gCmdqResourceMutex);
static DEFINE_MUTEX(gCmdqErrMutex);

/* The main context structure */

/* task done notification */
static wait_queue_head_t *gCmdWaitQueue;

/* thread acquire notification */
static wait_queue_head_t gCmdqThreadDispatchQueue;

static struct ContextStruct gCmdqContext;
static struct CmdqCBkStruct gCmdqGroupCallback[CMDQ_MAX_GROUP_COUNT];
static struct CmdqDebugCBkStruct gCmdqDebugCallback;
static struct cmdq_dts_setting g_dts_setting;

static struct dma_pool *g_task_buffer_pool;
static atomic_t g_pool_buffer_count;

#ifdef CMDQ_DUMP_FIRSTERROR
struct DumpFirstErrorStruct gCmdqFirstError;
#endif

static struct DumpCommandBufferStruct gCmdqBufferDump;

struct cmdq_cmd_struct {
	void *p_va_base;	/* VA: denote CMD virtual address space */
	dma_addr_t mva_base;	/* PA: denote the PA for CMD */
	u32 sram_base;		/* SRAM base offset for CMD start */
	u32 buffer_size;	/* size of allocated command buffer */
};
static struct cmdq_cmd_struct g_delay_thread_cmd;
static bool g_delay_thread_started;
static bool g_delay_thread_inited;
static DEFINE_SPINLOCK(g_delay_thread_lock);

static bool g_cmdq_consume_again;

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

static struct cmdqDTSDataStruct gCmdqDtsData;
static struct SubsysStruct gAddOnSubsys = {.msb = 0,
				.subsysID = -1,
				.mask = 0,
				.grpName = "AddOn"};

static struct StressContextStruct gStressContext;

u32 cmdq_core_max_task_in_thread(s32 thread)
{
	s32 maxTaskNUM = CMDQ_MAX_TASK_IN_THREAD;

#ifdef CMDQ_SECURE_PATH_SUPPORT
	if (cmdq_get_func()->isSecureThread(thread) == true)
		maxTaskNUM = CMDQ_MAX_TASK_IN_SECURE_THREAD;
#endif
	return maxTaskNUM;
}

static u32 *cmdq_core_task_get_first_va(const struct TaskStruct *pTask)
{
	struct CmdBufferStruct *entry = NULL;

	if (list_empty(&pTask->cmd_buffer_list))
		return NULL;
	entry = list_first_entry(&pTask->cmd_buffer_list,
		struct CmdBufferStruct, listEntry);
	return entry->pVABase;
}

static dma_addr_t cmdq_core_task_get_last_pa(struct TaskStruct *pTask)
{
	struct CmdBufferStruct *entry = NULL;

	if (list_empty(&pTask->cmd_buffer_list))
		return 0;
	entry = list_last_entry(&pTask->cmd_buffer_list,
		struct CmdBufferStruct, listEntry);
	return entry->MVABase;
}

static u32 *cmdq_core_task_get_last_va(struct TaskStruct *pTask)
{
	struct CmdBufferStruct *entry = NULL;

	if (list_empty(&pTask->cmd_buffer_list))
		return NULL;
	entry = list_last_entry(&pTask->cmd_buffer_list,
		struct CmdBufferStruct, listEntry);
	return entry->pVABase;
}

s32 cmdq_core_suspend_HW_thread(s32 thread, u32 lineNum)
{
	s32 loop = 0;
	u32 enabled = 0;

	if (thread == CMDQ_INVALID_THREAD) {
		CMDQ_ERR("suspend invalid thread\n");
		return -EFAULT;
	}

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->thread_suspend,
		MMPROFILE_FLAG_PULSE, thread, lineNum);
	/* write suspend bit */
	CMDQ_REG_SET32(CMDQ_THR_SUSPEND_TASK(thread), 0x01);

	/* check if the thread is already disabled.
	 * if already disabled, treat as suspend successful
	 * but print error log
	 */
	enabled = CMDQ_REG_GET32(CMDQ_THR_ENABLE_TASK(thread));
	if ((0x01 & enabled) == 0) {
		CMDQ_LOG(
			"[WARNING] thread %d suspend not effective enable:%d\n",
			thread, enabled);
		return 0;
	}

	loop = 0;
	while ((CMDQ_REG_GET32(CMDQ_THR_CURR_STATUS(thread)) & 0x2) == 0x0) {
		if (loop > CMDQ_MAX_LOOP_COUNT) {
			CMDQ_AEE("CMDQ", "Suspend HW thread %d failed\n",
				thread);
			return -EFAULT;
		}
		loop++;
	}

#ifdef CONFIG_MTK_FPGA
	CMDQ_MSG("EXEC: Suspend HW thread(%d)\n", thread);
#endif

	return 0;
}

static inline void cmdq_core_resume_HW_thread(s32 thread)
{
#ifdef CONFIG_MTK_FPGA
	CMDQ_MSG("EXEC: Resume HW thread(%d)\n", thread);
#endif
	/* make sure instructions are really in DRAM */
	smp_mb();
	CMDQ_PROF_MMP(cmdq_mmp_get_event()->thread_resume,
		MMPROFILE_FLAG_PULSE, thread, __LINE__);
	CMDQ_REG_SET32(CMDQ_THR_SUSPEND_TASK(thread), 0x00);
}

inline s32 cmdq_core_reset_HW_thread(s32 thread)
{
	s32 loop = 0;

	CMDQ_MSG("Reset HW thread(%d)\n", thread);

	CMDQ_REG_SET32(CMDQ_THR_WARM_RESET(thread), 0x01);
	while (0x1 == (CMDQ_REG_GET32(CMDQ_THR_WARM_RESET(thread)))) {
		if (loop > CMDQ_MAX_LOOP_COUNT) {
			CMDQ_AEE("CMDQ", "Reset HW thread %d failed\n",
				thread);
			return -EFAULT;
		}
		loop++;
	}

	CMDQ_REG_SET32(CMDQ_THR_SLOT_CYCLES, 0x3200);
	return 0;
}

static inline s32 cmdq_core_disable_HW_thread(s32 thread)
{
	cmdq_core_reset_HW_thread(thread);

	/* Disable thread */
	CMDQ_MSG("Disable HW thread(%d)\n", thread);
	CMDQ_REG_SET32(CMDQ_THR_ENABLE_TASK(thread), 0x00);
	return 0;
}

static bool cmdq_core_is_thread_cpr(u32 argument)
{
	if (argument >= CMDQ_THR_SPR_MAX && argument < CMDQ_THR_VAR_MAX)
		return true;
	else
		return false;
}

static s32 cmdq_copy_delay_to_sram(void)
{
	u32 cpr_offset = 0;
	dma_addr_t pa = g_delay_thread_cmd.mva_base;
	u32 buffer_size = g_delay_thread_cmd.buffer_size;

	if (g_delay_thread_cmd.sram_base == 0)
		return 0;

	cpr_offset = CMDQ_CPR_OFFSET(g_delay_thread_cmd.sram_base);
	if (cmdq_task_copy_to_sram(pa, cpr_offset, buffer_size) < 0) {
		CMDQ_ERR("DELAY: copy delay thread to SRAM failed!\n");
		return -EFAULT;
	}
	CMDQ_MSG("Copy delay thread:%pa size:%u cpr_offset:0x%x\n",
		&pa, buffer_size, cpr_offset);
	return 0;
}

static s32 cmdq_delay_thread_init(void)
{
	void *p_delay_thread_buffer = NULL;
	void *p_va = NULL;
	dma_addr_t pa = 0;
	u32 buffer_size = 0;
	u32 cpr_offset = 0;
	u32 free_sram_size = (g_dts_setting.cpr_size -
		cmdq_dev_get_thread_count() * CMDQ_THR_FREE_CPR_MAX) *
		sizeof(u32);

	memset(&(g_delay_thread_cmd), 0x0, sizeof(g_delay_thread_cmd));

	if (free_sram_size >= CMDQ_DELAY_THD_SIZE) {
		if (cmdq_task_create_delay_thread_sram(&p_delay_thread_buffer,
			&buffer_size, &cpr_offset) < 0) {
			CMDQ_ERR(
				"DELAY_INIT: create delay thread in sram failed, free:%u\n",
				free_sram_size);
			return -EFAULT;
		}

		CMDQ_LOG(
			"DELAY_INIT: create delay thread in sram size:%u free:%u\n",
			buffer_size, free_sram_size);
	} else {
		if (cmdq_task_create_delay_thread_dram(&p_delay_thread_buffer,
			&buffer_size) < 0) {
			CMDQ_ERR(
				"DELAY_INIT: create delay thread in dram failed!\n");
			return -EFAULT;
		}

		CMDQ_LOG(
			"DELAY_INIT: create delay thread in dram size:%u sram size:%u\n",
			buffer_size, free_sram_size);
	}

	if (!buffer_size) {
		CMDQ_ERR("DELAY_INIT: delay thread create failed\n");
		return -EFAULT;
	}

	p_va = cmdq_core_alloc_hw_buffer(cmdq_dev_get(), buffer_size, &pa,
		GFP_KERNEL);

	memcpy(p_va, p_delay_thread_buffer, buffer_size);

	CMDQ_MSG("Set delay thread CMD START:%pa size:%u cpr_offset:0x%x\n",
		&pa, buffer_size, cpr_offset);

	g_delay_thread_cmd.mva_base = pa;
	g_delay_thread_cmd.p_va_base = p_va;
	g_delay_thread_cmd.buffer_size = buffer_size;
	if (cpr_offset > 0) {
		g_delay_thread_cmd.sram_base = CMDQ_SRAM_ADDR(cpr_offset);
		cmdq_copy_delay_to_sram();
	}

	kfree(p_delay_thread_buffer);
	g_delay_thread_inited = true;
	g_delay_thread_started = false;
	return 0;
}

static s32 cmdq_delay_thread_start(void)
{
	bool enable_prefetch;
	int thread_priority;
	u32 end_address;
	const s32 thread = CMDQ_DELAY_THREAD_ID;
	unsigned long flags;

	CMDQ_PROF_SPIN_LOCK(g_delay_thread_lock, flags, delay_thread);
	if (!g_delay_thread_inited || g_delay_thread_started ||
		atomic_read(&gCmdqThreadUsage) < 1) {
		CMDQ_PROF_SPIN_UNLOCK(g_delay_thread_lock, flags,
			delay_thread_ignore);
		return 0;
	}

	CMDQ_MSG("EXEC: new delay thread:%d\n", thread);
	if (cmdq_core_reset_HW_thread(thread) < 0) {
		CMDQ_PROF_SPIN_UNLOCK(g_delay_thread_lock, flags,
			delay_thread_fail);
		return -EFAULT;
	}

	CMDQ_REG_SET32(CMDQ_THR_INST_CYCLES(thread), 0);

	enable_prefetch = cmdq_core_thread_prefetch_size(thread) > 0;
	if (enable_prefetch) {
		CMDQ_MSG("EXEC: set delay thread:%d enable prefetch size:%d\n",
			thread, cmdq_core_thread_prefetch_size(thread));
		CMDQ_REG_SET32(CMDQ_THR_PREFETCH(thread), 0x1);
	}

	thread_priority = cmdq_get_func()->priority(CMDQ_SCENARIO_TIMER_LOOP);

	if (g_delay_thread_cmd.sram_base > 0) {
		CMDQ_MSG(
			"EXEC: set delay thread(%d) in SRAM sram_addr:%u qos:%d\n",
			 thread, g_delay_thread_cmd.sram_base, thread_priority);

		end_address = g_delay_thread_cmd.sram_base +
			g_delay_thread_cmd.buffer_size;
		CMDQ_REG_SET32(CMDQ_THR_END_ADDR(thread), end_address);
		CMDQ_REG_SET32(CMDQ_THR_CURR_ADDR(thread),
			g_delay_thread_cmd.sram_base);
	} else {
		CMDQ_MSG("EXEC: set delay thread:%d pc:%pa qos:%d\n",
			 thread, &g_delay_thread_cmd.mva_base,
			 thread_priority);

		end_address = CMDQ_PHYS_TO_AREG(g_delay_thread_cmd.mva_base +
			g_delay_thread_cmd.buffer_size);
		CMDQ_REG_SET32(CMDQ_THR_END_ADDR(thread), end_address);
		CMDQ_REG_SET32(CMDQ_THR_CURR_ADDR(thread),
			CMDQ_PHYS_TO_AREG(g_delay_thread_cmd.mva_base));
	}

	/* set thread priority, bit 0-2 for priority level; */
	CMDQ_REG_SET32(CMDQ_THR_CFG(thread), thread_priority & 0x7);

	/* For loop thread, do not enable timeout */
	CMDQ_REG_SET32(CMDQ_THR_IRQ_ENABLE(thread), 0x011);

	/* enable thread */
	CMDQ_REG_SET32(CMDQ_THR_ENABLE_TASK(thread), 0x01);

	g_delay_thread_started = true;

	CMDQ_PROF_SPIN_UNLOCK(g_delay_thread_lock, flags, delay_thread);

	return 0;
}

static s32 cmdq_delay_thread_stop(void)
{
	unsigned long flags;
	const s32 thread = CMDQ_DELAY_THREAD_ID;

	CMDQ_PROF_SPIN_LOCK(g_delay_thread_lock, flags, delay_thread_stop);
	if (g_delay_thread_started == false ||
		atomic_read(&gCmdqThreadUsage) > 1) {
		CMDQ_PROF_SPIN_UNLOCK(g_delay_thread_lock, flags,
			delay_thread_stop_ignore);
		return 0;
	}

	if (cmdq_core_suspend_HW_thread(thread, __LINE__) < 0) {
		CMDQ_PROF_SPIN_UNLOCK(g_delay_thread_lock, flags,
			delay_thread_fail_stop);
		return -EFAULT;
	}

	cmdq_core_disable_HW_thread(thread);

	g_delay_thread_started = false;
	CMDQ_PROF_SPIN_UNLOCK(g_delay_thread_lock, flags,
		delay_thread_stop_done);
	CMDQ_MSG("EXEC: stop delay thread:%d\n", thread);

	return 0;
}

static void cmdq_delay_thread_deinit(void)
{
	cmdq_core_free_hw_buffer(cmdq_dev_get(),
		g_delay_thread_cmd.buffer_size,
		g_delay_thread_cmd.p_va_base,
		g_delay_thread_cmd.mva_base);
}

void cmdq_delay_dump_thread(bool dump_sram)
{
	cmdq_core_dump_thread(CMDQ_DELAY_THREAD_ID, "INFO");
	CMDQ_LOG(
		"==Delay Thread Task, size:%u started:%d pa:%pa va:0x%p sram:%u\n",
		g_delay_thread_cmd.buffer_size, g_delay_thread_started,
		&g_delay_thread_cmd.mva_base, g_delay_thread_cmd.p_va_base,
		g_delay_thread_cmd.sram_base);
	CMDQ_LOG("Dump TPR_MASK:0x%08x\n", CMDQ_REG_GET32(CMDQ_TPR_MASK));
	cmdqCoreDumpCommandMem(g_delay_thread_cmd.p_va_base,
		g_delay_thread_cmd.buffer_size);
	CMDQ_LOG("==Delay Thread Task command END\n");
	if (dump_sram)
		cmdqCoreDebugDumpSRAM(g_delay_thread_cmd.sram_base,
			g_delay_thread_cmd.buffer_size);
}

u32 cmdq_core_get_delay_start_cpr(void)
{
	return gCmdqContext.delay_cpr_start;
}

s32 cmdq_get_delay_id_by_scenario(enum CMDQ_SCENARIO_ENUM scenario)
{
	s32 delay_id = -1;

	switch (scenario) {
	case CMDQ_SCENARIO_PRIMARY_DISP:
	/* HACK: force debug into 0/1 thread */
	case CMDQ_SCENARIO_DEBUG_PREFETCH:
		delay_id = 0;
		break;
	case CMDQ_SCENARIO_TRIGGER_LOOP:
		delay_id = 1;
		break;
	case CMDQ_SCENARIO_DEBUG:
		delay_id = 2;
		break;
	default:
		delay_id = -1;
		break;
	}

	return delay_id;
}

static inline u32 cmdq_get_subsys_id(u32 arg_a)
{
	return (arg_a & CMDQ_ARG_A_SUBSYS_MASK) >> subsys_lsb_bit;
}

u32 *cmdq_core_task_get_va_by_offset(const struct TaskStruct *pTask,
	u32 offset)
{
	u32 offset_remaind = offset;
	struct CmdBufferStruct *cmd_buffer = NULL;

	list_for_each_entry(cmd_buffer, &pTask->cmd_buffer_list, listEntry) {
		if (offset_remaind >= CMDQ_CMD_BUFFER_SIZE) {
			offset_remaind -= CMDQ_CMD_BUFFER_SIZE;
			continue;
		}

		return cmd_buffer->pVABase + (offset_remaind / sizeof(u32));
	}

	return NULL;
}

dma_addr_t cmdq_core_task_get_pa_by_offset(
	const struct TaskStruct *pTask, u32 offset)
{
	u32 offset_remaind = offset;
	struct CmdBufferStruct *cmd_buffer = NULL;

	list_for_each_entry(cmd_buffer, &pTask->cmd_buffer_list, listEntry) {
		if (offset_remaind >= CMDQ_CMD_BUFFER_SIZE) {
			offset_remaind -= CMDQ_CMD_BUFFER_SIZE;
			continue;
		}

		return cmd_buffer->MVABase + offset_remaind;
	}

	return 0;
}

u32 cmdq_core_task_get_offset_by_pa(
	const struct TaskStruct *pTask, dma_addr_t pa, u32 **va_out)
{
	struct CmdBufferStruct *entry = NULL;
	u32 offset = 0, task_pa = 0;

	if (!pTask || !pa || CMDQ_IS_END_ADDR(pa))
		return ~0;

	list_for_each_entry(entry, &pTask->cmd_buffer_list, listEntry) {
		u32 buffer_size = (list_is_last(&entry->listEntry,
			&pTask->cmd_buffer_list) ?
			CMDQ_CMD_BUFFER_SIZE - pTask->buf_available_size :
			CMDQ_CMD_BUFFER_SIZE);

		task_pa = (u32)entry->MVABase;
		if (pa >= task_pa && pa < task_pa + buffer_size) {
			u32 buffer_offset = pa - task_pa;

			offset += buffer_offset;
			if (va_out)
				*va_out = (u32 *)((u8 *)entry->pVABase +
				buffer_offset);
			return offset;
		}
		offset += buffer_size;
	}

	return ~0;
}


static void cmdq_core_replace_overwrite_instr(struct TaskStruct *pTask,
	u32 index)
{
	/* check if this is overwrite instruction */
	u32 *p_cmd_assign = (u32 *)cmdq_core_task_get_va_by_offset(pTask,
		(index - 1) * CMDQ_INST_SIZE);

	CMDQ_MSG("replace assign:0x%08x 0x%08x\n",
		p_cmd_assign[0], p_cmd_assign[1]);
	if ((p_cmd_assign[1] >> 24) == CMDQ_CODE_LOGIC &&
		((p_cmd_assign[1] >> 23) & 1) == 1 &&
		cmdq_get_subsys_id(p_cmd_assign[1]) == CMDQ_LOGIC_ASSIGN &&
		(p_cmd_assign[1] & 0xFFFF) == CMDQ_SPR_FOR_TEMP) {
		u32 overwrite_index = p_cmd_assign[0];
		dma_addr_t overwrite_pa = cmdq_core_task_get_pa_by_offset(
			pTask, overwrite_index * CMDQ_INST_SIZE);

		p_cmd_assign[0] = (u32) overwrite_pa;
		CMDQ_MSG("overwrite: original index:%d, change to PA:%pa\n",
			overwrite_index, &overwrite_pa);
	}
}

static void cmdq_core_dump_buffer(const struct TaskStruct *pTask)
{
	struct CmdBufferStruct *cmd_buffer = NULL;

	list_for_each_entry(cmd_buffer, &pTask->cmd_buffer_list, listEntry) {
		u32 last_inst_index = CMDQ_CMD_BUFFER_SIZE / sizeof(u32);
		u32 *va = cmd_buffer->pVABase + last_inst_index - 4;

		if (list_is_last(&cmd_buffer->listEntry,
			&pTask->cmd_buffer_list) &&
			pTask->pCMDEnd > cmd_buffer->pVABase &&
			pTask->pCMDEnd < cmd_buffer->pVABase +
			last_inst_index &&
			va > pTask->pCMDEnd - 3) {
			va = pTask->pCMDEnd - 3;
		}

		CMDQ_ERR(
			"va:0x%p pa:%pa last inst(0x%p):0x%08x:%08x 0x%08x:%08x\n",
			cmd_buffer->pVABase, &cmd_buffer->MVABase,
			va, va[1], va[0], va[3], va[2]);
	}
}

static void cmdq_core_v3_adjust_jump(
	struct TaskStruct *task, u32 instr_pos, u32 *va)
{
	u32 offset;

	if (va[1] & 0x1)
		return;

	offset = instr_pos * CMDQ_INST_SIZE + va[0];

	/* adjust offset due to we insert jumps between buffers */
	offset += (offset / (CMDQ_CMD_BUFFER_SIZE - CMDQ_INST_SIZE)) *
		CMDQ_INST_SIZE;
	if (offset == task->bufferSize) {
		/* offset to last instruction may not adjust,
		 * return back 1 instruction
		 */
		offset = task->bufferSize - CMDQ_INST_SIZE;
	}

	/* change to absolute to avoid jump cross buffer */
	CMDQ_MSG("Replace jump from:0x%08x to offset:%u\n", va[0], offset);
	va[1] = va[1] | 0x1;
	va[0] = CMDQ_PHYS_TO_AREG(cmdq_core_task_get_pa_by_offset(
		task, offset));
}

static void cmdq_core_v3_replace_arg(
	struct TaskStruct *task, u32 *va, u32 thread_offset,
	u32 arg_op_code, u32 inst_idx)
{
	u32 arg_header = (va[1] >> 16) & 0xFFFF;
	u32 arg_a_i = va[1] & 0xFFFF;
	u32 arg_b_i = (va[0] >> 16) & 0xFFFF;
	u32 arg_c_i = va[0] & 0xFFFF;
	u32 arg_a_type = va[1] & (1 << 23);
	u32 arg_b_type = va[1] & (1 << 22);
	u32 arg_c_type = va[1] & (1 << 21);

	if (arg_a_type != 0 && cmdq_core_is_thread_cpr(arg_a_i)) {
		arg_a_i = thread_offset + (arg_a_i - CMDQ_THR_SPR_MAX);
		va[1] = (arg_header<<16) | (arg_a_i & 0xFFFF);
	}

	if (arg_b_type != 0 && cmdq_core_is_thread_cpr(arg_b_i))
		arg_b_i = thread_offset + (arg_b_i - CMDQ_THR_SPR_MAX);

	if (arg_c_type != 0 && cmdq_core_is_thread_cpr(arg_c_i))
		arg_c_i = thread_offset + (arg_c_i - CMDQ_THR_SPR_MAX);

	va[0] = (arg_b_i<<16) | (arg_c_i & 0xFFFF);

	if (arg_op_code == CMDQ_CODE_WRITE_S && inst_idx > 0 &&
		arg_a_type != 0 && arg_a_i == CMDQ_SPR_FOR_TEMP)
		cmdq_core_replace_overwrite_instr(task, inst_idx);
}

static void cmdq_core_v3_replace_jumpc(struct TaskStruct *task,
	u32 *va, u32 inst_idx, u32 inst_pos)
{
	u32 *p_cmd_logic = (u32 *)cmdq_core_task_get_va_by_offset(task,
		(inst_idx - 1) * CMDQ_INST_SIZE);

	/* logic and jump relative maybe separate by jump cross buffer */
	if (p_cmd_logic[1] == ((CMDQ_CODE_JUMP << 24) | 1)) {
		CMDQ_MSG(
			"Re-adjust new logic instruction due to jump cross buffer:%u\n",
			inst_idx);
		p_cmd_logic = (u32 *)cmdq_core_task_get_va_by_offset(task,
			(inst_idx - 2) * CMDQ_INST_SIZE);
	}

	if ((p_cmd_logic[1] >> 24) == CMDQ_CODE_LOGIC &&
		((p_cmd_logic[1] >> 16) & 0x1F) == CMDQ_LOGIC_ASSIGN) {
		u32 jump_op = CMDQ_CODE_JUMP_C_ABSOLUTE << 24;
		u32 jump_op_header = va[1] & 0xFFFFFF;
		u32 offset_target = inst_pos * CMDQ_INST_SIZE + p_cmd_logic[0];
		u32 dest_addr_pa = 0;

		offset_target += offset_target /
			(CMDQ_CMD_BUFFER_SIZE - CMDQ_INST_SIZE) *
			CMDQ_INST_SIZE;
		dest_addr_pa = cmdq_core_task_get_pa_by_offset(
			task, offset_target);

		if (dest_addr_pa == 0) {
			cmdq_core_dump_buffer(task);
			CMDQ_AEE("CMDQ",
				"Wrong PA offset, task:0x%p, inst idx:0x%08x cmd:0x%08x:%08x addr:0x%p\n",
				task, inst_idx, p_cmd_logic[1], p_cmd_logic[0],
				p_cmd_logic);
			return;
		}

		p_cmd_logic[0] = CMDQ_PHYS_TO_AREG(dest_addr_pa);
		va[1] = jump_op | jump_op_header;

		CMDQ_MSG(
			"Replace jump_c inst idx:0x%08x(0x%08x) cmd:0x%p 0x%08x:%08x logic:0x%p 0x%08x:0x%08x",
			inst_idx, inst_pos,
			va, va[1], va[0],
			p_cmd_logic, p_cmd_logic[1], p_cmd_logic[0]);
	} else {
		/* unable to jump correctly since relative offset
		 * may cross page
		 */
		CMDQ_ERR(
			"No logic before jump, task:0x%p, inst idx:0x%08x cmd:0x%p 0x%08x:%08x logic:0x%p 0x%08x:0x%08x\n",
			task, inst_idx,
			va, va[1], va[0],
			p_cmd_logic, p_cmd_logic[1], p_cmd_logic[0]);
	}
}

static void cmdq_core_replace_v3_instr(struct TaskStruct *pTask, s32 thread)
{
	u32 i;
	u32 arg_op_code;
	u32 *p_cmd_va;
	u32 thread_offset;
	u32 *p_instr_position = CMDQ_U32_PTR(pTask->replace_instr.position);
	u32 inst_idx = 0;
	u32 boundary_idx = (pTask->bufferSize / CMDQ_INST_SIZE);

	if (pTask->replace_instr.number == 0)
		return;

	CMDQ_MSG("replace_instr.number:%u\n", pTask->replace_instr.number);
	thread_offset = CMDQ_CPR_STRAT_ID + CMDQ_THR_CPR_MAX * thread;

	for (i = 0; i < pTask->replace_instr.number; i++) {
		if ((p_instr_position[i] + 1) * CMDQ_INST_SIZE >
			pTask->commandSize) {
			CMDQ_ERR(
				"Incorrect replace instruction position, index (%d), size (%d)\n",
				p_instr_position[i], pTask->commandSize);
			break;
		}

		/* adjust instruction index due to we insert jumps
		 * between buffers
		 */
		inst_idx = p_instr_position[i] +
			p_instr_position[i] / ((CMDQ_CMD_BUFFER_SIZE /
			CMDQ_INST_SIZE) - 1);
		if (inst_idx == boundary_idx) {
			/* last jump instruction may not adjust,
			 * return back 1 instruction
			 */
			inst_idx--;
		}
		p_cmd_va = (u32 *)cmdq_core_task_get_va_by_offset(pTask,
			inst_idx * CMDQ_INST_SIZE);
		if (!p_cmd_va) {
			CMDQ_AEE("CMDQ",
				"Cannot find va, task:0x%p idx:%u/%u instruction idx:%u(%u) size:%u+%u\n",
				pTask, i, pTask->replace_instr.number,
				inst_idx, p_instr_position[i],
				pTask->bufferSize, pTask->buf_available_size);
			cmdq_core_dump_buffer(pTask);
			continue;
		}

		arg_op_code = p_cmd_va[1] >> 24;
		if (arg_op_code == CMDQ_CODE_JUMP) {
			cmdq_core_v3_adjust_jump(pTask,
				(u32)p_instr_position[i], p_cmd_va);
		} else {
			CMDQ_MSG(
				"replace instruction: (%d)0x%p:0x%08x 0x%08x\n",
				i, p_cmd_va, p_cmd_va[0], p_cmd_va[1]);
			cmdq_core_v3_replace_arg(pTask, p_cmd_va,
				thread_offset, arg_op_code, inst_idx);
		}

		if (arg_op_code == CMDQ_CODE_JUMP_C_RELATIVE) {
			cmdq_core_v3_replace_jumpc(pTask, p_cmd_va, inst_idx,
				p_instr_position[i]);
		}
	}
}

static void cmdq_core_copy_v3_struct(struct TaskStruct *pTask,
	struct cmdqCommandStruct *pCommandDesc)
{
	u32 array_num = 0;
	u32 *p_instr_position = NULL;

	pTask->replace_instr.number = pCommandDesc->replace_instr.number;
	if (pTask->replace_instr.number == 0) {
		pTask->replace_instr.position = 0;
		return;
	}

	array_num = pTask->replace_instr.number * sizeof(u32);

	p_instr_position = kzalloc(array_num, GFP_KERNEL);
	memcpy(p_instr_position, CMDQ_U32_PTR(
		pCommandDesc->replace_instr.position), array_num);
	pTask->replace_instr.position = (cmdqU32Ptr_t)(unsigned long)
		p_instr_position;
}

static void cmdq_core_release_v3_struct(struct TaskStruct *pTask)
{
	/* reset local variable setting */
	pTask->replace_instr.number = 0;
	if (pTask->replace_instr.position) {
		kfree(CMDQ_U32_PTR(pTask->replace_instr.position));
		pTask->replace_instr.position = 0;
	}
}

/* Use CMDQ as Resource Manager */
void cmdq_core_unlock_resource(struct work_struct *workItem)
{
	struct ResourceUnitStruct *res = NULL;
	struct delayed_work *delayedWorkItem = NULL;
	s32 status = 0;

	delayedWorkItem = container_of(workItem, struct delayed_work, work);
	res = container_of(delayedWorkItem, struct ResourceUnitStruct,
		delayCheckWork);

	mutex_lock(&gCmdqResourceMutex);

	CMDQ_MSG("[Res] unlock resource with engine:0x%016llx\n",
		res->engine_flag);
	if (res->used && res->delaying) {
		res->unlock = sched_clock();
		res->used = false;
		res->delaying = false;
		/* delay time is reached and unlock resource */
		if (!res->availableCB) {
			/* print error message */
			CMDQ_LOG("[Res] available CB func is NULL, event:%d\n",
				res->lockEvent);
		} else {
			CmdqResourceAvailableCB cb_func = res->availableCB;

			/* before call callback, release lock at first */
			mutex_unlock(&gCmdqResourceMutex);
			status = cb_func(res->lockEvent);
			mutex_lock(&gCmdqResourceMutex);

			if (status < 0) {
				/* Error status print */
				CMDQ_ERR(
					"[Res] available CB (%d) return fail:%d\n",
					res->lockEvent, status);
			}
		}
	}
	mutex_unlock(&gCmdqResourceMutex);
}

void cmdq_core_init_resource(u32 engine_id,
	enum CMDQ_EVENT_ENUM resourceEvent)
{
	struct ResourceUnitStruct *res;

	res = kzalloc(sizeof(struct ResourceUnitStruct), GFP_KERNEL);
	if (res) {
		res->engine_id = engine_id;
		res->engine_flag = (1LL << engine_id);
		res->lockEvent = resourceEvent;
		INIT_DELAYED_WORK(&res->delayCheckWork,
			cmdq_core_unlock_resource);
		INIT_LIST_HEAD(&(res->list_entry));
		list_add_tail(&(res->list_entry), &gCmdqContext.resourceList);
	}
}

/* engineFlag: task original engineFlag */
/* enginesNotUsed: flag which indicate Not Used engine after release task */
void cmdq_core_delay_check_unlock(u64 engineFlag, const u64 enginesNotUsed)
{
	/* Check engine in enginesNotUsed */
	struct ResourceUnitStruct *res = NULL;

	if (!cmdq_core_is_feature_on(CMDQ_FEATURE_SRAM_SHARE))
		return;

	list_for_each_entry(res, &gCmdqContext.resourceList, list_entry) {
		if (enginesNotUsed & res->engine_flag) {
			mutex_lock(&gCmdqResourceMutex);
			/* find matched engine become not used*/
			if (!res->used) {
				/* resource is not used
				 * but we got engine is released!
				 * log as error and still continue
				 */
				CMDQ_ERR(
					"[Res] resource will delay but not used, engine:0x%llx\n",
					res->engine_flag);
			}

			/* Cancel previous delay task if existed */
			if (res->delaying) {
				res->delaying = false;
				cancel_delayed_work(&res->delayCheckWork);
			}

			/* Start a new delay task */
			CMDQ_VERBOSE("[Res] queue delay unlock resource\n");
			queue_delayed_work(gCmdqContext.resourceCheckWQ,
				&res->delayCheckWork,
				CMDQ_DELAY_RELEASE_RESOURCE_MS);
			res->delay = sched_clock();
			res->delaying = true;
			mutex_unlock(&gCmdqResourceMutex);
		}
	}
}

u32 cmdq_core_get_thread_prefetch_size(s32 thread)
{
	if (thread < 0 || thread >= cmdq_dev_get_thread_count())
		return 0;

	return g_dts_setting.prefetch_size[thread];
}

struct cmdqDTSDataStruct *cmdq_core_get_whole_DTS_Data(void)
{
	return &gCmdqDtsData;
}

void cmdq_core_init_DTS_data(void)
{
	u32 i;

	memset(&(gCmdqDtsData), 0x0, sizeof(gCmdqDtsData));

	for (i = 0; i < CMDQ_SYNC_TOKEN_MAX; i++) {
		if (i <= CMDQ_MAX_HW_EVENT_COUNT) {
			/* GCE HW evevt */
			gCmdqDtsData.eventTable[i] =
				CMDQ_SYNC_TOKEN_INVALID - 1 - i;
		} else {
			/* GCE SW evevt */
			gCmdqDtsData.eventTable[i] = i;
		}
	}
}

void cmdq_core_set_event_table(enum CMDQ_EVENT_ENUM event, const s32 value)
{
	if (event >= 0 && event < CMDQ_SYNC_TOKEN_MAX)
		gCmdqDtsData.eventTable[event] = value;
}

s32 cmdq_core_get_event_value(enum CMDQ_EVENT_ENUM event)
{
	if (event < 0 || event >= CMDQ_SYNC_TOKEN_MAX)
		return -EINVAL;

	return gCmdqDtsData.eventTable[event];
}

s32 cmdq_core_reverse_event_ENUM(const u32 value)
{
	u32 eventENUM = CMDQ_SYNC_TOKEN_INVALID;
	u32 i;

	for (i = 0; i < CMDQ_SYNC_TOKEN_MAX; i++) {
		if (value == gCmdqDtsData.eventTable[i]) {
			eventENUM = i;
			break;
		}
	}

	return eventENUM;
}

static bool cmdq_core_is_valid_in_active_list(struct TaskStruct *pTask)
{
	bool isValid = true;

	do {
		if (!pTask) {
			isValid = false;
			break;
		}

		if (pTask->taskState == TASK_STATE_IDLE ||
			pTask->thread == CMDQ_INVALID_THREAD ||
			!pTask->pCMDEnd ||
			list_empty(&pTask->cmd_buffer_list)) {
			/* check CMDQ task's contain */
			isValid = false;
		}
	} while (0);

	return isValid;
}

void *cmdq_core_alloc_hw_buffer(struct device *dev, size_t size,
	dma_addr_t *dma_handle, const gfp_t flag)
{
	void *pVA;
	dma_addr_t PA;
	CMDQ_TIME alloc_cost = 0;

	do {
		PA = 0;
		pVA = NULL;

		CMDQ_PROF_START(current->pid, __func__);
		CMDQ_PROF_MMP(cmdq_mmp_get_event()->alloc_buffer,
			MMPROFILE_FLAG_START, current->pid, size);
		alloc_cost = sched_clock();

		pVA = dma_alloc_coherent(dev, size, &PA, flag);

		alloc_cost = sched_clock() - alloc_cost;
		CMDQ_PROF_MMP(cmdq_mmp_get_event()->alloc_buffer,
			MMPROFILE_FLAG_END, current->pid, alloc_cost);
		CMDQ_PROF_END(current->pid, __func__);

		if (alloc_cost > CMDQ_PROFILE_LIMIT_1) {
#if defined(__LP64__) || defined(_LP64)
			CMDQ_LOG(
				"[warn] alloc buffer (size:%zu) cost %llu us > %ums\n",
				size, alloc_cost/1000,
				CMDQ_PROFILE_LIMIT_1/1000000);
#else
			CMDQ_LOG(
				"[warn] alloc buffer (size:%zu) cost %llu us > %llums\n",
				size, div_s64(alloc_cost, 1000),
				div_s64(CMDQ_PROFILE_LIMIT_1, 1000000));
#endif
		}

	} while (0);

	*dma_handle = PA;

	CMDQ_VERBOSE("%s, pVA:0x%p, PA:%pa, PAout:%pa\n",
		__func__, pVA, &PA, &(*dma_handle));

	return pVA;
}

void cmdq_core_free_hw_buffer(struct device *dev, size_t size, void *cpu_addr,
	dma_addr_t dma_handle)
{
	CMDQ_TIME free_cost = 0;

	CMDQ_VERBOSE("%s, pVA:0x%p, PA:%pa\n",
		__func__, cpu_addr, &dma_handle);

	free_cost = sched_clock();
	dma_free_coherent(dev, size, cpu_addr, dma_handle);
	free_cost = sched_clock() - free_cost;

	if (free_cost > CMDQ_PROFILE_LIMIT_1) {
#if defined(__LP64__) || defined(_LP64)
		CMDQ_LOG(
			"[warn] free buffer (size:%zu) cost %llu us > %ums\n",
			size, free_cost/1000, CMDQ_PROFILE_LIMIT_1/1000000);
#else
		CMDQ_LOG(
			"[warn] free buffer (size:%zu) cost %llu us > %llums\n",
			size, div_s64(free_cost, 1000),
			div_s64(CMDQ_PROFILE_LIMIT_1, 1000000));
#endif
	}

}

void cmdq_core_create_buffer_pool(void)
{
	if (unlikely(g_task_buffer_pool)) {
		CMDQ_LOG("Buffer pool already create pool:%p count:%d\n",
			g_task_buffer_pool,
			(s32)atomic_read(&g_pool_buffer_count));
		return;
	}

	g_task_buffer_pool = dma_pool_create("cmdq", cmdq_dev_get(),
		PAGE_SIZE, 0, 0);
	if (!g_task_buffer_pool) {
		/* CMDQ still work even fail, only show log */
		CMDQ_ERR("Fail to create task buffer pool.\n");
	}

	CMDQ_MSG("DMA pool alloate:%p\n", g_task_buffer_pool);
}

void cmdq_core_destroy_buffer_pool(void)
{
	if (unlikely((atomic_read(&g_pool_buffer_count)))) {
		/* should not happen, print counts before reset */
		CMDQ_LOG("Task buffer still use:%d\n",
			(s32)atomic_read(&g_pool_buffer_count));
		return;
	}

	CMDQ_MSG("DMA pool destroy:%p\n", g_task_buffer_pool);

	dma_pool_destroy(g_task_buffer_pool);
	g_task_buffer_pool = NULL;
	atomic_set(&g_pool_buffer_count, 0);
}

void *cmdq_core_alloc_hw_buffer_pool(const gfp_t flag,
	dma_addr_t *dma_handle_out)
{
	void *va = NULL;
	dma_addr_t pa = 0;

	if (unlikely(!g_task_buffer_pool))
		return NULL;

	if (atomic_inc_return(&g_pool_buffer_count) > CMDQ_DMA_POOL_COUNT) {
		/* not use pool, decrease to value before call */
		atomic_dec(&g_pool_buffer_count);
		return NULL;
	}

	CMDQ_PROF_START(current->pid, __func__);
	CMDQ_PROF_MMP(cmdq_mmp_get_event()->alloc_buffer,
		MMPROFILE_FLAG_START, current->pid,
		CMDQ_CMD_BUFFER_SIZE);

	va = dma_pool_alloc(g_task_buffer_pool, flag, &pa);

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->alloc_buffer,
		MMPROFILE_FLAG_END, current->pid, 0);
	CMDQ_PROF_END(current->pid, __func__);

	if (!va) {
		CMDQ_ERR(
			"Alloc buffer from pool fail va:%p pa:%pa flag:%u pool:%p count:%d\n",
			va, &pa, (u32)flag, g_task_buffer_pool,
			(s32)atomic_read(&g_pool_buffer_count));
		atomic_dec(&g_pool_buffer_count);
	}

	*dma_handle_out = pa;

	CMDQ_VERBOSE("%s count:%d allocate:%p pool:%p\n",
		__func__, (s32)atomic_read(&g_pool_buffer_count),
		va, g_task_buffer_pool);

	return va;
}

void cmdq_core_free_hw_buffer_pool(void *va, dma_addr_t dma_handle)
{
	if (unlikely(atomic_read(&g_pool_buffer_count) <= 0 ||
		!g_task_buffer_pool))
		return;

	CMDQ_VERBOSE("%s count:%d free:%p pool:%p\n",
		__func__, (s32)atomic_read(&g_pool_buffer_count),
		va, g_task_buffer_pool);

	dma_pool_free(g_task_buffer_pool, va, dma_handle);
	atomic_dec(&g_pool_buffer_count);
}

void cmdq_core_free_reg_buffer(struct TaskStruct *task)
{
	CMDQ_VERBOSE("COMMAND: free result buf VA:0x%p PA:%pa\n",
		task->regResults, &task->regResultsMVA);

	cmdq_core_free_hw_buffer(cmdq_dev_get(),
		task->regCount * sizeof(task->regResults[0]),
		task->regResults,
		task->regResultsMVA);

	task->regResults = NULL;
	task->regResultsMVA = 0;
	task->regCount = 0;
}

void cmdq_core_alloc_reg_buffer(struct TaskStruct *task)
{
	if (task->regResults) {
		CMDQ_AEE("CMDQ", "Result is not empty, addr:0x%p task:0x%p\n",
			task->regResults, task);
		cmdq_core_free_reg_buffer(task);
	}

	task->regResults = cmdq_core_alloc_hw_buffer(cmdq_dev_get(),
		task->regCount * sizeof(task->regResults[0]),
		&task->regResultsMVA,
		GFP_KERNEL);

	CMDQ_VERBOSE("COMMAND: allocate result buf VA:0x%p PA:%pa\n",
		task->regResults, &task->regResultsMVA);
}

s32 cmdq_core_alloc_sram_buffer(size_t size,
	const char *owner_name, u32 *out_cpr_offset)
{
	u32 cpr_offset = 0;
	struct SRAMChunk *p_sram_chunk, *p_last_chunk;
	/* Normalize from byte unit to 32bit unit */
	size_t normalized_count = size / sizeof(u32);

	/* Align allocated buffer to 64bit due to instruction alignment */
	if (normalized_count % 2 != 0)
		normalized_count++;

	/* Get last entry to calculate new SRAM start address */
	if (!list_empty(&gCmdqContext.sram_allocated_list)) {
		p_last_chunk = list_last_entry(
			&gCmdqContext.sram_allocated_list,
			struct SRAMChunk, list_node);
		cpr_offset = p_last_chunk->start_offset + p_last_chunk->count;
	}

	if (cpr_offset + normalized_count > g_dts_setting.cpr_size) {
		CMDQ_ERR(
			"SRAM count is out of memory, start:%u want:%zu owner:%s\n",
			cpr_offset, normalized_count, owner_name);
		cmdq_core_dump_sram();
		return -ENOMEM;
	}

	p_sram_chunk = kzalloc(sizeof(struct SRAMChunk), GFP_KERNEL);
	if (p_sram_chunk) {
		p_sram_chunk->start_offset = cpr_offset;
		p_sram_chunk->count = normalized_count;
		strncpy(p_sram_chunk->owner, owner_name,
			sizeof(p_sram_chunk->owner) - 1);
		list_add_tail(&(p_sram_chunk->list_node),
			&gCmdqContext.sram_allocated_list);
		gCmdqContext.allocated_sram_count += normalized_count;
		CMDQ_LOG(
			"SRAM Chunk New-32bit unit: start:0x%x count:%zu Name:%s\n",
			p_sram_chunk->start_offset, p_sram_chunk->count,
			p_sram_chunk->owner);
	}

	*out_cpr_offset = cpr_offset;
	return 0;
}

void cmdq_core_free_sram_buffer(u32 cpr_offset, size_t size)
{
	struct SRAMChunk *p_sram_chunk;
	bool released = false;
	/* Normalize from byte unit to 32bit unit */
	size_t normalized_count = size / sizeof(u32);

	/* Find and remove */
	list_for_each_entry(p_sram_chunk, &gCmdqContext.sram_allocated_list,
		list_node) {
		if (p_sram_chunk->start_offset == cpr_offset &&
			p_sram_chunk->count == normalized_count) {
			CMDQ_MSG(
				"SRAM Chunk Free-32bit unit: start:0x%x count:%zu Name:%s\n",
				p_sram_chunk->start_offset,
				p_sram_chunk->count, p_sram_chunk->owner);
			list_del_init(&(p_sram_chunk->list_node));
			gCmdqContext.allocated_sram_count -= normalized_count;
			released = true;
			break;
		}
	}

	if (!released) {
		CMDQ_ERR(
			"SRAM Chunk Free-32bit unit: start:0x%x count:%zu failed\n",
			cpr_offset, normalized_count);
		cmdq_core_dump_sram();
	}
}

size_t cmdq_core_get_free_sram_size(void)
{
	return (g_dts_setting.cpr_size - gCmdqContext.allocated_sram_count) *
		sizeof(u32);
}

void cmdq_core_dump_sram(void)
{
	struct SRAMChunk *p_sram_chunk;
	s32 index = 0;

	list_for_each_entry(p_sram_chunk, &gCmdqContext.sram_allocated_list,
		list_node) {
		CMDQ_LOG(
			"SRAM Chunk(%d)-32bit unit: start:0x%x count:%zu Name:%s\n",
			index, p_sram_chunk->start_offset,
			p_sram_chunk->count, p_sram_chunk->owner);
		index++;
	}
}

s32 cmdq_core_set_secure_IRQ_status(u32 value)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	const u32 offset = CMDQ_SEC_SHARED_IRQ_RAISED_OFFSET;
	u32 *pVA;

	value = 0x0;
	if (!gCmdqContext.hSecSharedMem) {
		CMDQ_ERR("%s, shared memory is not created\n", __func__);
		return -EFAULT;
	}

	pVA = (u32 *)(gCmdqContext.hSecSharedMem->pVABase + offset);
	(*pVA) = value;

	CMDQ_VERBOSE("[shared_IRQ]set raisedIRQ:0x%08x\n", value);

	return 0;
#else
	CMDQ_ERR(
		"func:%s failed since CMDQ secure path not support in this proj\n",
		__func__);
	return -EFAULT;
#endif
}

s32 cmdq_core_get_secure_IRQ_status(void)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	const u32 offset = CMDQ_SEC_SHARED_IRQ_RAISED_OFFSET;
	u32 *pVA;
	s32 value;

	value = 0x0;
	if (!gCmdqContext.hSecSharedMem) {
		CMDQ_ERR("%s, shared memory is not created\n", __func__);
		return -EFAULT;
	}

	pVA = (u32 *) (gCmdqContext.hSecSharedMem->pVABase + offset);
	value = *pVA;

	CMDQ_VERBOSE("[shared_IRQ]IRQ raised:0x%08x\n", value);

	return value;
#else
	CMDQ_ERR(
		"func:%s failed since CMDQ secure path not support in this proj\n",
		__func__);
	return -EFAULT;
#endif
}

s32 cmdq_core_set_secure_thread_exec_counter(const s32 thread, const u32 cookie)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	const u32 offset = CMDQ_SEC_SHARED_THR_CNT_OFFSET + thread *
		sizeof(u32);
	u32 *pVA = NULL;

	if (cmdq_get_func()->isSecureThread(thread) == false) {
		CMDQ_ERR("%s, invalid param, thread:%d\n", __func__, thread);
		return -EFAULT;
	}

	if (!gCmdqContext.hSecSharedMem) {
		CMDQ_ERR("%s, shared memory is not created\n", __func__);
		return -EFAULT;
	}

	CMDQ_MSG("[shared_cookie] set thread %d CNT(%p) to %d\n",
		thread, pVA, cookie);
	pVA = (u32 *) (gCmdqContext.hSecSharedMem->pVABase + offset);
	(*pVA) = cookie;

	return 0;
#else
	CMDQ_ERR(
		"func:%s failed since CMDQ secure path not support in this proj\n",
		__func__);
	return -EFAULT;
#endif
}

s32 cmdq_core_get_secure_thread_exec_counter(const s32 thread)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	const u32 offset = CMDQ_SEC_SHARED_THR_CNT_OFFSET + thread *
		sizeof(u32);
	u32 *pVA;
	u32 value;

	if (!cmdq_get_func()->isSecureThread(thread)) {
		CMDQ_ERR("%s, invalid param, thread:%d\n",
			__func__, thread);
		return -EFAULT;
	}

	if (!gCmdqContext.hSecSharedMem) {
		CMDQ_ERR("%s, shared memory is not created\n", __func__);
		return -EFAULT;
	}

	pVA = (u32 *) (gCmdqContext.hSecSharedMem->pVABase + offset);
	value = *pVA;

	CMDQ_VERBOSE("[shared_cookie] get thread %d CNT(%p) value is %d\n",
		thread, pVA, value);

	return value;
#else
	CMDQ_ERR(
		"func:%s failed since CMDQ secure path not support in this proj\n",
		__func__);
	return -EFAULT;
#endif

}

void cmdq_core_dump_secure_task_status(void)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	const u32 task_va_offset = CMDQ_SEC_SHARED_TASK_VA_OFFSET;
	const u32 task_op_offset = CMDQ_SEC_SHARED_OP_OFFSET;

	u32 *pVA;
	s32 va_value_lo, va_value_hi, op_value;

	if (!gCmdqContext.hSecSharedMem) {
		CMDQ_ERR("%s, shared memory is not created\n", __func__);
		return;
	}

	pVA = (u32 *)(gCmdqContext.hSecSharedMem->pVABase + task_va_offset);
	va_value_lo = *pVA;

	pVA = (u32 *)(gCmdqContext.hSecSharedMem->pVABase + task_va_offset +
		sizeof(u32));
	va_value_hi = *pVA;

	pVA = (u32 *) (gCmdqContext.hSecSharedMem->pVABase + task_op_offset);
	op_value = *pVA;

	CMDQ_ERR("[shared_op_status]task VA:0x%04x%04x op:%d\n",
		va_value_hi, va_value_lo, op_value);
#else
	CMDQ_ERR(
		"func:%s failed since CMDQ secure path not support in this proj\n",
		__func__);
#endif
}

s32 cmdq_core_thread_exec_counter(const s32 thread)
{
	return (cmdq_get_func()->isSecureThread(thread) == false) ?
	    (CMDQ_GET_COOKIE_CNT(thread)) :
	    (cmdq_core_get_secure_thread_exec_counter(thread));
}

struct cmdqSecSharedMemoryStruct *cmdq_core_get_secure_shared_memory(void)
{
	return gCmdqContext.hSecSharedMem;
}

const char *cmdq_core_get_event_name_ENUM(enum CMDQ_EVENT_ENUM event)
{
	const char *eventName = "CMDQ_EVENT_UNKNOWN";
	struct cmdq_event_table *events = cmdq_event_get_table();
	u32 table_size = cmdq_event_get_table_size();
	u32 i = 0;

	for (i = 0; i < table_size; i++) {
		if (events[i].event == event)
			return events[i].event_name;
	}

	return eventName;
}

const char *cmdq_core_get_event_name(enum CMDQ_EVENT_ENUM event)
{
	const s32 eventENUM = cmdq_core_reverse_event_ENUM(event);

	return cmdq_core_get_event_name_ENUM(eventENUM);
}

void cmdqCoreClearEvent(enum CMDQ_EVENT_ENUM event)
{
	s32 eventValue = cmdq_core_get_event_value(event);

	CMDQ_MSG("clear event %d\n", eventValue);
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, eventValue);
}

void cmdqCoreSetEvent(enum CMDQ_EVENT_ENUM event)
{
	s32 eventValue = cmdq_core_get_event_value(event);

	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | eventValue);
}

u32 cmdqCoreGetEvent(enum CMDQ_EVENT_ENUM event)
{
	u32 regValue = 0;
	s32 eventValue = cmdq_core_get_event_value(event);

	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_ID, (0x3FF & eventValue));
	regValue = CMDQ_REG_GET32(CMDQ_SYNC_TOKEN_VAL);
	return regValue;
}

bool cmdq_core_support_sync_non_suspendable(void)
{
#ifdef CMDQ_USE_LEGACY
	return false;
#else
	return true;
#endif
}

ssize_t cmdqCorePrintLogLevel(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int len = 0;

	if (buf)
		len = sprintf(buf, "%d\n", gCmdqContext.logLevel);

	return len;
}

ssize_t cmdqCoreWriteLogLevel(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int len = 0;
	int value = 0;
	int status = 0;

	char textBuf[10] = { 0 };

	do {
		if (size >= 10) {
			status = -EFAULT;
			break;
		}

		len = size;
		memcpy(textBuf, buf, len);

		textBuf[len] = '\0';
		if (kstrtoint(textBuf, 10, &value) < 0) {
			status = -EFAULT;
			break;
		}

		status = len;
		if (value < 0 || value > 3)
			value = 0;

		cmdq_core_set_log_level(value);
	} while (0);

	return status;
}

ssize_t cmdqCorePrintProfileEnable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int len = 0;

	if (buf)
		len = sprintf(buf, "%d\n", gCmdqContext.enableProfile);

	return len;

}

ssize_t cmdqCoreWriteProfileEnable(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int len = 0;
	int value = 0;
	int status = 0;

	char textBuf[10] = { 0 };

	do {
		if (size >= 10) {
			status = -EFAULT;
			break;
		}

		len = size;
		memcpy(textBuf, buf, len);

		textBuf[len] = '\0';
		if (kstrtoint(textBuf, 10, &value) < 0) {
			status = -EFAULT;
			break;
		}

		status = len;
		if (value < 0 || value > 3)
			value = 0;

		gCmdqContext.enableProfile = value;
		if (value > 0)
			cmdqSecEnableProfile(true);
		else
			cmdqSecEnableProfile(false);
	} while (0);

	return status;
}

static void cmdq_core_dump_task(const struct TaskStruct *pTask)
{
	CMDQ_ERR("Task:0x%p Scenario:%d State:%d Priority:%d Flag:0x%016llx\n",
		pTask, pTask->scenario, pTask->taskState,
		pTask->priority, pTask->engineFlag);

	cmdq_core_dump_buffer(pTask);

	/* dump last Inst only when VALID command buffer
	 * otherwise data abort is happened
	 */
	if (!list_empty(&pTask->cmd_buffer_list)) {
		CMDQ_ERR(
			"CMDEnd:0x%p Command Size:%d Last Inst:0x%08x:0x%08x 0x%08x:0x%08x\n",
			pTask->pCMDEnd, pTask->commandSize,
			pTask->pCMDEnd[-3], pTask->pCMDEnd[-2],
			pTask->pCMDEnd[-1], pTask->pCMDEnd[0]);
	} else {
		CMDQ_ERR("CMDEnd:0x%p Size:%d\n",
			pTask->pCMDEnd, pTask->commandSize);
	}

	CMDQ_ERR("Buffer size:%u available size:%u\n",
		pTask->bufferSize, pTask->buf_available_size);

	CMDQ_ERR("Result buffer va:0x%p pa:%pa count:%u\n",
		pTask->regResults, &pTask->regResultsMVA, pTask->regCount);
	if (pTask->sram_base)
		CMDQ_ERR("** This is SRAM task sram base:%u\n",
			pTask->sram_base);
	CMDQ_ERR(
		"Reorder:%d Trigger:%lld Got IRQ:0x%llx Wait:%lld Finish:%lld\n",
		pTask->reorder, pTask->trigger, pTask->gotIRQ,
		pTask->beginWait, pTask->wakedUp);
	CMDQ_ERR("Caller pid:%d name:%s\n",
		pTask->callerPid, pTask->callerName);
}

void cmdq_core_dump_task_buffer_hex(struct TaskStruct *pTask)
{
	struct CmdBufferStruct *cmd_buffer = NULL;

	if (unlikely(!pTask || list_empty(&pTask->cmd_buffer_list))) {
		CMDQ_ERR("Try to dump empty task:%p\n", pTask);
		return;
	}

	list_for_each_entry(cmd_buffer, &pTask->cmd_buffer_list, listEntry) {
		print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 16, 4,
			cmd_buffer->pVABase,
			list_is_last(&cmd_buffer->listEntry,
				&pTask->cmd_buffer_list) ?
				CMDQ_CMD_BUFFER_SIZE -
				pTask->buf_available_size :
				CMDQ_CMD_BUFFER_SIZE, true);
	}
}

static bool cmdq_core_task_is_buffer_size_valid(const struct TaskStruct *pTask)
{
	return (pTask->bufferSize % CMDQ_CMD_BUFFER_SIZE ==
		(pTask->buf_available_size > 0 ?
		CMDQ_CMD_BUFFER_SIZE - pTask->buf_available_size : 0));
}

static u32 *cmdq_core_get_pc(const struct TaskStruct *pTask,
	s32 thread, u32 insts[4], u32 *pa_out)
{
	u32 curr_pc = 0L;
	u8 *inst_ptr = NULL;

	if (unlikely(!pTask || list_empty(&pTask->cmd_buffer_list) ||
		thread == CMDQ_INVALID_THREAD)) {
		CMDQ_ERR(
			"get pc failed since invalid param, pTask:0x%p thread:%d\n",
			pTask, thread);
		return NULL;
	}

	insts[2] = 0;
	insts[3] = 0;

	curr_pc = CMDQ_AREG_TO_PHYS(CMDQ_REG_GET32(
		CMDQ_THR_CURR_ADDR(thread)));
	if (unlikely(pTask->use_sram_buffer)) {
		u32 *pVABase = cmdq_core_task_get_first_va(pTask);

		inst_ptr = (u8 *)pVABase + (curr_pc - pTask->sram_base) *
			sizeof(u64);
	} else {
		cmdq_core_task_get_offset_by_pa(pTask, curr_pc,
			(u32 **)&inst_ptr);
	}

	if (inst_ptr) {
		insts[2] = CMDQ_REG_GET32(inst_ptr + 0);
		insts[3] = CMDQ_REG_GET32(inst_ptr + 4);
	}

	if (pa_out)
		*pa_out = curr_pc;

	return (u32 *)inst_ptr;
}

static bool cmdq_core_task_is_valid_pa(const struct TaskStruct *pTask,
	dma_addr_t pa)
{
	struct CmdBufferStruct *entry = NULL;
	long task_pa = 0;

	/* check if pc stay at end */
	if (CMDQ_IS_END_ADDR(pa) && pTask->pCMDEnd && CMDQ_IS_END_ADDR(
		pTask->pCMDEnd[-1]))
		return true;

	list_for_each_entry(entry, &pTask->cmd_buffer_list, listEntry) {
		task_pa = (long)entry->MVABase;
		if (pa >= task_pa && pa < task_pa +
			(list_is_last(&entry->listEntry,
			&pTask->cmd_buffer_list) ?
			CMDQ_CMD_BUFFER_SIZE - pTask->buf_available_size :
			CMDQ_CMD_BUFFER_SIZE)) {
			return true;
		}
	}
	return false;
}

static dma_addr_t cmdq_core_task_get_eoc_pa(struct TaskStruct *pTask)
{
	struct CmdBufferStruct *entry = NULL;

	if (unlikely(list_empty(&pTask->cmd_buffer_list)))
		return 0;

	/* Last buffer contains at least 2 instruction, offset directly. */
	entry = list_last_entry(&pTask->cmd_buffer_list,
		struct CmdBufferStruct, listEntry);
	return entry->MVABase + CMDQ_CMD_BUFFER_SIZE -
		pTask->buf_available_size - 2 * CMDQ_INST_SIZE;
}

static u32 *cmdq_core_task_get_append_va(struct TaskStruct *pTask)
{
	struct CmdBufferStruct *entry = NULL;
	u32 *append_va;

	if (unlikely(list_empty(&pTask->cmd_buffer_list)))
		return NULL;

	entry = list_last_entry(&pTask->cmd_buffer_list,
		struct CmdBufferStruct, listEntry);
	if (CMDQ_CMD_BUFFER_SIZE - pTask->buf_available_size >=
		3 * CMDQ_INST_SIZE) {
		/* Last buffer contains at least 3 instruction,
		 * offset directly.
		 */
		append_va = (u32 *)((u8 *)entry->pVABase +
			CMDQ_CMD_BUFFER_SIZE - pTask->buf_available_size -
			3 * CMDQ_INST_SIZE);
	} else {
		/* Last buffer only has 2 instructions,
		 * use prev buffer and skip last jump.
		 */
		entry = list_prev_entry(entry, listEntry);
		append_va = (u32 *)((u8 *)entry->pVABase +
			CMDQ_CMD_BUFFER_SIZE - 2*CMDQ_INST_SIZE);
	}
	return append_va;
}

static dma_addr_t cmdq_core_task_get_final_instr(struct TaskStruct *pTask)
{
	struct CmdBufferStruct *entry = NULL;

	if (unlikely(list_empty(&pTask->cmd_buffer_list)))
		return 0;

	/* Last buffer contains at least 2 instruction, offset directly. */
	entry = list_last_entry(&pTask->cmd_buffer_list,
		struct CmdBufferStruct, listEntry);
	return entry->MVABase + CMDQ_CMD_BUFFER_SIZE -
		pTask->buf_available_size;
}

void cmdq_core_get_task_first_buffer(struct TaskStruct *pTask,
			u32 **va_ptr, dma_addr_t *pa_handle)
{
	struct CmdBufferStruct *cmd_buffer = NULL;

	if (!list_empty(&pTask->cmd_buffer_list)) {
		cmd_buffer = list_first_entry(&pTask->cmd_buffer_list,
				struct CmdBufferStruct, listEntry);
		*va_ptr = cmd_buffer->pVABase;
		*pa_handle = cmd_buffer->MVABase;
	} else {
		*va_ptr = NULL;
		*pa_handle = 0;
	}
}

static dma_addr_t cmdq_core_task_get_first_pa(struct TaskStruct *pTask)
{
	struct CmdBufferStruct *entry = NULL;

	if (list_empty(&pTask->cmd_buffer_list))
		return 0;
	entry = list_first_entry(&pTask->cmd_buffer_list,
		struct CmdBufferStruct, listEntry);
	return entry->MVABase;
}

void cmdq_core_dump_tasks_info(void)
{
	struct TaskStruct *pTask = NULL;
	struct list_head *p = NULL;
	s32 index = 0;

	/* Remove Mutex, since this will be called in ISR */
	/* mutex_lock(&gCmdqTaskMutex); */

	CMDQ_ERR("========= Active List Task Dump =========\n");
	index = 0;
	list_for_each(p, &gCmdqContext.taskActiveList) {
		pTask = list_entry(p, struct TaskStruct, listEntry);
		CMDQ_ERR(
			"Task(%d) 0x%p Pid:%d Name:%s Scenario:%d engineFlag:0x%llx\n",
			index, pTask, pTask->callerPid, pTask->callerName,
			pTask->scenario, pTask->engineFlag);
		++index;
	}
	CMDQ_ERR("====== Total %d in Active Task =======\n", index);

	/* mutex_unlock(&gCmdqTaskMutex); */
}

int cmdq_core_print_record_title(char *_buf, int bufLen)
{
	int length = 0;
	char *buf;

	buf = _buf;

	length = snprintf(buf, bufLen,
		"index,pid,scn,flag,task_pri,is_sec,size,thr#,thr_pri,");
	bufLen -= length;
	buf += length;

	length = snprintf(buf, bufLen,
		"submit,acq_thr,irq_time,begin_wait,exec_time,buf_alloc,buf_rec,buf_rel,total_time,start,end,jump\n");
	bufLen -= length;
	buf += length;

	length = (buf - _buf);

	return length;
}

static int cmdq_core_print_record(const struct RecordStruct *pRecord,
	int index, char *_buf, int bufLen)
{
	int length = 0;
	char *unit[5] = { "ms", "ms", "ms", "ms", "ms" };
	s32 IRQTime;
	s32 execTime;
	s32 beginWaitTime;
	s32 totalTime;
	s32 acquireThreadTime;
	unsigned long rem_nsec;
	CMDQ_TIME submitTimeSec;
	char *buf;
	s32 profileMarkerCount;
	s32 i;

	rem_nsec = 0;
	submitTimeSec = pRecord->submit;
	rem_nsec = do_div(submitTimeSec, 1000000000);
	buf = _buf;

	CMDQ_GET_TIME_IN_MS(pRecord->submit, pRecord->done, totalTime);
	CMDQ_GET_TIME_IN_MS(pRecord->submit, pRecord->trigger,
		acquireThreadTime);
	CMDQ_GET_TIME_IN_MS(pRecord->submit, pRecord->beginWait,
		beginWaitTime);
	CMDQ_GET_TIME_IN_MS(pRecord->trigger, pRecord->gotIRQ, IRQTime);
	CMDQ_GET_TIME_IN_MS(pRecord->trigger, pRecord->wakedUp, execTime);

	/* detect us interval */
	if (acquireThreadTime == 0) {
		CMDQ_GET_TIME_IN_US_PART(pRecord->submit, pRecord->trigger,
			acquireThreadTime);
		unit[0] = "us";
	}
	if (IRQTime == 0) {
		CMDQ_GET_TIME_IN_US_PART(pRecord->trigger, pRecord->gotIRQ,
			IRQTime);
		unit[1] = "us";
	}
	if (beginWaitTime == 0) {
		CMDQ_GET_TIME_IN_US_PART(pRecord->submit, pRecord->beginWait,
			beginWaitTime);
		unit[2] = "us";
	}
	if (execTime == 0) {
		CMDQ_GET_TIME_IN_US_PART(pRecord->trigger, pRecord->wakedUp,
			execTime);
		unit[3] = "us";
	}
	if (totalTime == 0) {
		CMDQ_GET_TIME_IN_US_PART(pRecord->submit, pRecord->done,
			totalTime);
		unit[4] = "us";
	}

	/* pRecord->priority for task priority */
	/* when pRecord->is_secure is 0 for secure task */
	length = snprintf(buf, bufLen,
		"%4d,%5d,%2d,0x%012llx,%2d,%d,%d,%02d,%02d,",
		index, pRecord->user, pRecord->scenario, pRecord->engineFlag,
		pRecord->priority, pRecord->is_secure, pRecord->size,
		pRecord->thread,
		cmdq_get_func()->priority(pRecord->scenario));
	bufLen -= length;
	buf += length;

	length = snprintf(buf, bufLen,
		"%5llu.%06lu,%4d%s,%4d%s,%4d%s,%4d%s,%dus,%dus,%dus,%4d%s,",
		submitTimeSec, rem_nsec / 1000, acquireThreadTime, unit[0],
		IRQTime, unit[1], beginWaitTime, unit[2], execTime, unit[3],
		pRecord->durAlloc, pRecord->durReclaim, pRecord->durRelease,
		totalTime, unit[4]);
	bufLen -= length;
	buf += length;

	/* record address */
	length = snprintf(buf, bufLen,
		"0x%08x,0x%08x,0x%08x",
		pRecord->start, pRecord->end, pRecord->jump);
	bufLen -= length;
	buf += length;

	profileMarkerCount = pRecord->profileMarkerCount;
	if (profileMarkerCount > CMDQ_MAX_PROFILE_MARKER_IN_TASK)
		profileMarkerCount = CMDQ_MAX_PROFILE_MARKER_IN_TASK;

	for (i = 0; i < profileMarkerCount; i++) {
		length = snprintf(buf, bufLen, ",%s,%lld",
			pRecord->profileMarkerTag[i],
			pRecord->profileMarkerTimeNS[i]);
		bufLen -= length;
		buf += length;
	}

	length = snprintf(buf, bufLen, "\n");
	bufLen -= length;
	buf += length;

	length = (buf - _buf);
	return length;
}

int cmdqCorePrintRecordSeq(struct seq_file *m, void *v)
{
	unsigned long flags;
	s32 index;
	s32 numRec;
	struct RecordStruct record;
	char msg[160] = { 0 };

	/* we try to minimize time spent in spin lock */
	/* since record is an array so it is okay to */
	/* allow displaying an out-of-date entry. */
	CMDQ_PROF_SPIN_LOCK(gCmdqRecordLock, flags, print_record);
	numRec = gCmdqContext.recNum;
	index = gCmdqContext.lastID - 1;
	CMDQ_PROF_SPIN_UNLOCK(gCmdqRecordLock, flags, print_record);

	/* we print record in reverse order. */
	cmdq_core_print_record_title(msg, sizeof(msg));
	seq_printf(m, "%s", msg);
	for (; numRec > 0; --numRec, --index) {
		if (index >= CMDQ_MAX_RECORD_COUNT)
			index = 0;
		else if (index < 0)
			index = CMDQ_MAX_RECORD_COUNT - 1;

		CMDQ_PROF_SPIN_LOCK(gCmdqRecordLock, flags, print_record_loop);
		record = gCmdqContext.record[index];
		CMDQ_PROF_SPIN_UNLOCK(gCmdqRecordLock, flags,
			print_record_loop);

		cmdq_core_print_record(&record, index, msg, sizeof(msg));
		seq_printf(m, "%s", msg);
	}

	return 0;
}

static void cmdq_core_print_thd_usage(struct seq_file *m, void *v,
	u32 thread_idx)
{
	u32 inner;
	unsigned long flags = 0;
	u32 *va;
	dma_addr_t pa;
	u32 *pcva, pcpa;
	u32 insts[4] = {0};
	char parsed_inst[128] = { 0 };
	struct ThreadStruct *thread = &gCmdqContext.thread[thread_idx];

	if (!thread->taskCount)
		return;

	seq_printf(m, "====== Thread %d Usage =======\n", thread_idx);
	seq_printf(m, "Wait Cookie:%d Next Cookie:%d\n",
		thread->waitCookie, thread->nextCookie);

	CMDQ_PROF_SPIN_LOCK(gCmdqThreadLock, flags, print_status);

	for (inner = 0; inner < cmdq_core_max_task_in_thread(thread_idx);
		inner++) {
		struct TaskStruct *task = thread->pCurTask[inner];

		if (!task)
			continue;

		/* dump task basic info */
		seq_printf(m,
			   "Slot:%d Task:0x%p Pid:%d Name:%s Scn:%d",
			   thread_idx, task, task->callerPid,
			   task->callerName, task->scenario);

		/* here only print first buffer to reduce log */
		cmdq_core_get_task_first_buffer(task, &va, &pa);
		seq_printf(m,
			" VABase:0x%p MVABase:%pa Size:%d",
			va, &pa, task->commandSize);

		if (task->use_sram_buffer)
			seq_printf(m, " SRAM Base:%u",
				task->sram_base);

		if (task->pCMDEnd) {
			seq_printf(m,
				" Last Command:0x%08x:0x%08x",
				task->pCMDEnd[-1], task->pCMDEnd[0]);
		}

		seq_puts(m, "\n");

		/* dump PC info */
		pcva = cmdq_core_get_pc(task, thread_idx, insts, &pcpa);
		if (pcva) {
			cmdq_core_parse_instruction(
				pcva, parsed_inst, sizeof(parsed_inst));
			seq_printf(m,
				   "PC:0x%p(0x%08x) 0x%08x:0x%08x => %s",
				   pcva, pcpa, insts[2], insts[3],
				   parsed_inst);
		} else {
			seq_puts(m, "PC(VA): Not available\n");
		}
	}

	CMDQ_PROF_SPIN_UNLOCK(gCmdqThreadLock, flags, print_status);
}

int cmdqCorePrintStatusSeq(struct seq_file *m, void *v)
{
	struct EngineStruct *pEngine = NULL;
	struct TaskStruct *pTask = NULL;
	struct list_head *p = NULL;
	s32 index = 0;
	int listIdx = 0;
	const struct list_head *lists[] = {
		&gCmdqContext.taskFreeList,
		&gCmdqContext.taskActiveList,
		&gCmdqContext.taskWaitList
	};
	static const char *const listNames[] = { "Free", "Active", "Wait" };
	const enum CMDQ_ENG_ENUM engines[] =
		CMDQ_FOREACH_MODULE_PRINT(GENERATE_ENUM);
	static const char *const engineNames[] =
		CMDQ_FOREACH_MODULE_PRINT(GENERATE_STRING);
	struct list_head *p_buf = NULL;
	struct CmdBufferStruct *cmd_buffer = NULL;
	struct SRAMChunk *p_sram_chunk;
	const u32 max_thread_count = cmdq_dev_get_thread_count();

#ifdef CMDQ_DUMP_FIRSTERROR
	if (gCmdqFirstError.cmdqCount > 0) {
		unsigned long long saveTimeSec = gCmdqFirstError.savetime;
		unsigned long rem_nsec = do_div(saveTimeSec, 1000000000);
		struct tm nowTM;

		time_to_tm(gCmdqFirstError.savetv.tv_sec,
			sys_tz.tz_minuteswest * 60, &nowTM);
		seq_puts(m, "================= [CMDQ] Dump first error ================\n");
		seq_printf(m, "kernel time:[%5llu.%06lu],",
			saveTimeSec, rem_nsec / 1000);
		seq_printf(m, " UTC time:[%04ld-%02d-%02d %02d:%02d:%02d.%06ld],",
			   (nowTM.tm_year + 1900), (nowTM.tm_mon + 1),
			   nowTM.tm_mday, nowTM.tm_hour, nowTM.tm_min,
			   nowTM.tm_sec, gCmdqFirstError.savetv.tv_usec);
		seq_printf(m, " Pid:%d Name:%s\n", gCmdqFirstError.callerPid,
			gCmdqFirstError.callerName);
		seq_printf(m, "%s", gCmdqFirstError.cmdqString);
		if (gCmdqFirstError.cmdqMaxSize <= 0)
			seq_printf(m, "\nWARNING: MAX size:%d is full\n",
			CMDQ_MAX_FIRSTERROR);
		seq_puts(m, "\n\n");
	}
#endif

	/* Save command buffer dump */
	if (gCmdqBufferDump.count > 0) {
		s32 buffer_id;

		seq_puts(m, "================= [CMDQ] Dump Command Buffer =================\n");
		CMDQ_PROF_MUTEX_LOCK(gCmdqTaskMutex, dump_status_buffer);
		for (buffer_id = 0; buffer_id < gCmdqBufferDump.bufferSize;
			buffer_id++)
			seq_printf(m, "%c",
				gCmdqBufferDump.cmdqString[buffer_id]);
		CMDQ_PROF_MUTEX_UNLOCK(gCmdqTaskMutex, dump_status_buffer);
		seq_puts(m, "\n=============== [CMDQ] Dump Command Buffer END ===============\n\n\n");
	}

#ifdef CMDQ_PWR_AWARE
	/* note for constatnt format (without a % substitution),
	 * use seq_puts to speed up outputs
	 */
	seq_puts(m, "====== Clock Status =======\n");
	cmdq_get_func()->printStatusSeqClock(m);
#endif

	seq_puts(m, "====== DMA Mask Status =======\n");
	seq_printf(m, "dma_set_mask result:%d\n",
		cmdq_dev_get_dma_mask_result());

	seq_puts(m, "====== SRAM Usage Status =======\n");
	index = 0;
	list_for_each_entry(p_sram_chunk, &gCmdqContext.sram_allocated_list,
		list_node) {
		seq_printf(m, "SRAM Chunk(%d)-32bit unit: start:0x%x count:%zu Name:%s\n",
			index, p_sram_chunk->start_offset, p_sram_chunk->count,
			p_sram_chunk->owner);
		index++;
	}
	cmdq_dev_enable_gce_clock(true);
	seq_printf(m, "==Delay Task TPR_MASK:0x%08x size:%u started:%d pa:%pa va:0x%p sram:%u\n",
		CMDQ_REG_GET32(CMDQ_TPR_MASK), g_delay_thread_cmd.buffer_size,
		g_delay_thread_started, &g_delay_thread_cmd.mva_base,
		g_delay_thread_cmd.p_va_base, g_delay_thread_cmd.sram_base);
	cmdq_dev_enable_gce_clock(false);

	seq_puts(m, "====== Engine Usage =======\n");

	for (listIdx = 0; listIdx < ARRAY_SIZE(engines); ++listIdx) {
		pEngine = &gCmdqContext.engine[engines[listIdx]];
		seq_printf(m, "%s: count:%d owner:%d fail:%d reset:%d\n",
			   engineNames[listIdx],
			   pEngine->userCount,
			   pEngine->currOwner, pEngine->failCount,
			   pEngine->resetCount);
	}

	if (cmdq_core_is_feature_on(CMDQ_FEATURE_SRAM_SHARE)) {
		struct ResourceUnitStruct *pResource = NULL;

		mutex_lock(&gCmdqResourceMutex);
		list_for_each_entry(pResource, &gCmdqContext.resourceList,
			list_entry) {
			seq_printf(m, "[Res] Dump resource with event:%d\n",
				pResource->lockEvent);
			seq_printf(m, "[Res]   notify:%llu delay:%lld\n",
				pResource->notify, pResource->delay);
			seq_printf(m, "[Res]   lock:%llu unlock:%lld\n",
				pResource->lock, pResource->unlock);
			seq_printf(m, "[Res]   acquire:%llu release:%lld\n",
				pResource->acquire, pResource->release);
			seq_printf(m, "[Res]   isUsed:%d isLend:%d isDelay:%d\n",
				pResource->used, pResource->lend,
				pResource->delaying);
			if (!pResource->releaseCB)
				seq_puts(m, "[Res] release CB func is NULL\n");
		}
		mutex_unlock(&gCmdqResourceMutex);
	}

	CMDQ_PROF_MUTEX_LOCK(gCmdqTaskMutex, dump_status_list);

	/* print all tasks in both list */
	for (listIdx = 0; listIdx < ARRAY_SIZE(lists); listIdx++) {
		/* skip FreeTasks by default */
		if (!cmdq_core_should_print_msg() && listIdx == 0)
			continue;

		index = 0;
		list_for_each(p, lists[listIdx]) {
			pTask = list_entry(p, struct TaskStruct, listEntry);
			seq_printf(m, "====== %s Task(%d) 0x%p Usage =======\n",
				listNames[listIdx],
				   index, pTask);
			seq_printf(m, "State:%d Size:%d\n",
				   pTask->taskState, pTask->commandSize);

			list_for_each(p_buf, &pTask->cmd_buffer_list) {
				cmd_buffer = list_entry(p_buf,
					struct CmdBufferStruct, listEntry);
				seq_printf(m, "VABase:0x%p MVABase:%pa\n",
				   cmd_buffer->pVABase, &cmd_buffer->MVABase);
			}
			if (pTask->use_sram_buffer)
				seq_printf(m, "USE SRAM BUFFER sram base address:%u\n",
				pTask->sram_base);

			seq_printf(m, "Scenario:%d Priority:%d, Flag:0x%llx VAEnd:0x%p\n",
				   pTask->scenario, pTask->priority,
				   pTask->engineFlag, pTask->pCMDEnd);
			seq_printf(m,
				   "Reorder:%d Trigger:%lld IRQ:0x%llx Wait:%lld Wake Up:%lld\n",
				   pTask->reorder,
				   pTask->trigger, pTask->gotIRQ,
				   pTask->beginWait, pTask->wakedUp);
			++index;
		}
		seq_printf(m, "====== Total %d %s Task =======\n",
			index, listNames[listIdx]);
	}

	for (index = 0; index < max_thread_count; index++)
		cmdq_core_print_thd_usage(m, v, index);

	CMDQ_PROF_MUTEX_UNLOCK(gCmdqTaskMutex, dump_status_list);

	return 0;
}

ssize_t cmdqCorePrintError(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i;
	int length = 0;

	for (i = 0; i < gCmdqContext.errNum && i < CMDQ_MAX_ERROR_COUNT;
		i++) {
		struct ErrorStruct *pError = &gCmdqContext.error[i];
		u64 ts = pError->ts_nsec;
		unsigned long rem_nsec = do_div(ts, 1000000000);

		length += snprintf(buf + length,
				   PAGE_SIZE - length,
				   "[%5lu.%06lu] ", (unsigned long)ts,
				   rem_nsec / 1000);
		length += cmdq_core_print_record(&pError->errorRec,
			i, buf + length, PAGE_SIZE - length);
		if (length >= PAGE_SIZE)
			break;
	}

	return length;
}

static void cmdq_task_init_profile_marker_data(
	struct cmdqCommandStruct *pCommandDesc, struct TaskStruct *pTask)
{
	u32 i;

	pTask->profileMarker.count = pCommandDesc->profileMarker.count;
	pTask->profileMarker.hSlot = pCommandDesc->profileMarker.hSlot;
	for (i = 0; i < CMDQ_MAX_PROFILE_MARKER_IN_TASK; i++)
		pTask->profileMarker.tag[i] =
		pCommandDesc->profileMarker.tag[i];
}

static void cmdq_task_deinit_profile_marker_data(struct TaskStruct *pTask)
{
	if (!pTask)
		return;

	if ((pTask->profileMarker.count <= 0) ||
		(pTask->profileMarker.hSlot == 0))
		return;

	cmdq_free_mem((cmdqBackupSlotHandle)pTask->profileMarker.hSlot);
	pTask->profileMarker.hSlot = 0LL;
	pTask->profileMarker.count = 0;
}

/*  */
/* For kmemcache, initialize variables of TaskStruct (but not buffers) */
static void cmdq_core_task_ctor(void *param)
{
	struct TaskStruct *pTask = (struct TaskStruct *) param;

	CMDQ_VERBOSE("cmdq_core_task_ctor:0x%p\n", param);
	memset(pTask, 0, sizeof(struct TaskStruct));
	INIT_LIST_HEAD(&(pTask->listEntry));
	pTask->taskState = TASK_STATE_IDLE;
	pTask->thread = CMDQ_INVALID_THREAD;
}

void cmdq_task_free_buffer_impl(struct list_head *cmd_buffer_list)
{
	struct list_head *p, *n = NULL;
	struct CmdBufferStruct *cmd_buffer = NULL;

	list_for_each_safe(p, n, cmd_buffer_list) {
		cmd_buffer = list_entry(p, struct CmdBufferStruct, listEntry);
		list_del(&cmd_buffer->listEntry);

		if (cmd_buffer->use_pool)
			cmdq_core_free_hw_buffer_pool(cmd_buffer->pVABase,
				cmd_buffer->MVABase);
		else
			cmdq_core_free_hw_buffer(cmdq_dev_get(),
				CMDQ_CMD_BUFFER_SIZE,
				cmd_buffer->pVABase, cmd_buffer->MVABase);
		kfree(cmd_buffer);
	}
}

void cmdq_task_free_buffer_work(struct work_struct *work_item)
{
	struct CmdFreeWorkStruct *free_work;

	free_work = container_of(work_item, struct CmdFreeWorkStruct,
		free_buffer_work);
	cmdq_task_free_buffer_impl(&free_work->cmd_buffer_list);
	kfree(free_work);
}

void cmdq_task_free_task_command_buffer(struct TaskStruct *pTask)
{
	CMDQ_TIME startTime;
	struct CmdFreeWorkStruct *free_work_item;

	startTime = sched_clock();

	free_work_item = kzalloc(sizeof(struct CmdFreeWorkStruct), GFP_KERNEL);
	if (likely(free_work_item)) {
		list_replace_init(&pTask->cmd_buffer_list,
			&free_work_item->cmd_buffer_list);
		INIT_WORK(&free_work_item->free_buffer_work,
			cmdq_task_free_buffer_work);
		queue_work(gCmdqContext.taskAutoReleaseWQ,
			&free_work_item->free_buffer_work);
	} else {
		CMDQ_ERR(
			"Unable to start free buffer work, free directly, task:0x%p\n",
			pTask);
		cmdq_task_free_buffer_impl(&pTask->cmd_buffer_list);
		INIT_LIST_HEAD(&pTask->cmd_buffer_list);
	}

	if (pTask->use_sram_buffer) {
		u32 sram_id = CMDQ_CPR_OFFSET(pTask->sram_base);
		/* free SRAM */
		cmdq_core_free_sram_buffer(sram_id, pTask->bufferSize);
	}

	CMDQ_INC_TIME_IN_US(startTime, sched_clock(), pTask->durRelease);

	pTask->use_sram_buffer = false;
	pTask->sram_base = 0;
	pTask->buf_available_size = 0;
	pTask->bufferSize = 0;
	pTask->commandSize = 0;
	pTask->pCMDEnd = NULL;
}

static s32 cmdq_core_task_alloc_single_buffer_list(struct TaskStruct *pTask,
	struct CmdBufferStruct **new_buffer_entry_handle)
{
	s32 status = 0;
	struct CmdBufferStruct *buffer_entry = NULL;

	buffer_entry = kzalloc(sizeof(*buffer_entry), GFP_KERNEL);
	if (!buffer_entry) {
		CMDQ_ERR("allocate buffer record failed task:%p\n", pTask);
		return -ENOMEM;
	}

	/* try allocate from pool first */
	buffer_entry->pVABase = cmdq_core_alloc_hw_buffer_pool(GFP_KERNEL,
	&buffer_entry->MVABase);
	buffer_entry->use_pool = (bool)buffer_entry->pVABase;

	if (!buffer_entry->pVABase) {
		buffer_entry->pVABase = cmdq_core_alloc_hw_buffer(
			cmdq_dev_get(), CMDQ_CMD_BUFFER_SIZE,
			&buffer_entry->MVABase, GFP_KERNEL);
	}

	if (!buffer_entry->pVABase) {
		CMDQ_ERR("allocate cmd buffer of size %u failed\n",
			(u32)CMDQ_CMD_BUFFER_SIZE);
		kfree(buffer_entry);
		return -ENOMEM;
	}

	list_add_tail(&buffer_entry->listEntry, &pTask->cmd_buffer_list);

	pTask->buf_available_size = CMDQ_CMD_BUFFER_SIZE;

	if (new_buffer_entry_handle)
		*new_buffer_entry_handle = buffer_entry;

	return status;
}

/*  */
/* Allocate and initialize TaskStruct and its command buffer */
static struct TaskStruct *cmdq_core_task_create(void)
{
	struct TaskStruct *pTask = NULL;

	pTask = (struct TaskStruct *)kmem_cache_alloc(gCmdqContext.taskCache,
		GFP_KERNEL);
	if (!pTask) {
		CMDQ_AEE("CMDQ",
			"Allocate command buffer by kmem_cache_alloc failed\n");
		return NULL;
	}

	INIT_LIST_HEAD(&pTask->cmd_buffer_list);
	return pTask;
}

void cmdq_core_reset_hw_events_impl(enum CMDQ_EVENT_ENUM event)
{
	s32 value = cmdq_core_get_event_value(event);

	if (value > 0) {
		/* Reset GCE event */
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD,
			CMDQ_SYNC_TOKEN_MAX & value);
	}
}

void cmdq_core_reset_hw_events(void)
{
	int index;
	struct ResourceUnitStruct *pResource = NULL;
	const u32 max_thread_count = cmdq_dev_get_thread_count();
	struct cmdq_event_table *events = cmdq_event_get_table();
	u32 table_size = cmdq_event_get_table_size();

	/* set all defined events to 0 */
	CMDQ_MSG("cmdq_core_reset_hw_events\n");

	for (index = 0; index < table_size; index++)
		cmdq_core_reset_hw_events_impl(events[index].event);

	/* However, GRP_SET are resource flags, */
	/* by default they should be 1. */
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_GPR_SET_0);
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_GPR_SET_1);
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_GPR_SET_2);
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_GPR_SET_3);
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_GPR_SET_4);

	/* CMDQ_SYNC_RESOURCE are resource flags, */
	/* by default they should be 1. */
	cmdqCoreSetEvent(CMDQ_SYNC_RESOURCE_WROT0);
	cmdqCoreSetEvent(CMDQ_SYNC_RESOURCE_WROT1);
	list_for_each_entry(pResource, &gCmdqContext.resourceList,
		list_entry) {
		mutex_lock(&gCmdqResourceMutex);
		if (pResource->lend) {
			CMDQ_LOG("[Res] Client is already lend, event:%d\n",
				pResource->lockEvent);
			cmdqCoreClearEvent(pResource->lockEvent);
		}
		mutex_unlock(&gCmdqResourceMutex);
	}

	/* However, CMDQ_SYNC_RESOURCE are WSM lock flags, */
	/* by default they should be 1. */
	cmdqCoreSetEvent(CMDQ_SYNC_SECURE_WSM_LOCK);

	/* However, APPEND_THR are resource flags, */
	/* by default they should be 1. */
	for (index = 0; index < max_thread_count; index++)
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_APPEND_THR(index));
}

void cmdq_core_config_prefetch_gsize(void)
{
	if (g_dts_setting.prefetch_thread_count <= 4) {
		u32 i = 0, prefetch_gsize = 0, total_size = 0;

		for (i = 0; i < g_dts_setting.prefetch_thread_count; i++) {
			total_size += g_dts_setting.prefetch_size[i];
			prefetch_gsize |= (g_dts_setting.prefetch_size[i] /
				32 - 1) << (i * 4);
		}

		CMDQ_REG_SET32(CMDQ_PREFETCH_GSIZE, prefetch_gsize);
		CMDQ_MSG("prefetch gsize configure:0x%08x total size:%u\n",
			prefetch_gsize, total_size);
	}
}

void cmdq_core_reset_engine_struct(void)
{
	struct EngineStruct *pEngine;
	int index;

	/* Reset engine status */
	pEngine = gCmdqContext.engine;
	for (index = 0; index < CMDQ_MAX_ENGINE_COUNT; index++)
		pEngine[index].currOwner = CMDQ_INVALID_THREAD;
}

void cmdq_core_reset_thread_struct(void)
{
	int index;
	const u32 max_thread_count = cmdq_dev_get_thread_count();

	/* Reset thread status */
	memset(gCmdqContext.thread, 0, sizeof(*gCmdqContext.thread) *
		max_thread_count);
	for (index = 0; index < max_thread_count; index++)
		gCmdqContext.thread[index].allowDispatching = 1;
}

void cmdq_core_init_thread_work_queue(void)
{
	struct ThreadStruct *pThread;
	int index;
	const u32 max_thread_count = cmdq_dev_get_thread_count();

	gCmdqContext.taskThreadAutoReleaseWQ = kcalloc(
		max_thread_count,
		sizeof(*gCmdqContext.taskThreadAutoReleaseWQ),
		GFP_KERNEL);

	/* Initialize work queue per thread */
	pThread = &(gCmdqContext.thread[0]);
	for (index = 0; index < max_thread_count; index++) {
		gCmdqContext.taskThreadAutoReleaseWQ[index] =
		    create_singlethread_workqueue("cmdq_auto_release_thread");
	}
}

void cmdq_core_destroy_thread_work_queue(void)
{
	struct ThreadStruct *pThread;
	int index;
	const u32 max_thread_count = cmdq_dev_get_thread_count();

	/* Initialize work queue per thread */
	pThread = &(gCmdqContext.thread[0]);
	for (index = 0; index < max_thread_count; index++) {
		destroy_workqueue(
			gCmdqContext.taskThreadAutoReleaseWQ[index]);
		gCmdqContext.taskThreadAutoReleaseWQ[index] = NULL;
	}

	kfree(gCmdqContext.taskThreadAutoReleaseWQ);
}

bool cmdq_core_is_valid_group(enum CMDQ_GROUP_ENUM engGroup)
{
	/* check range */
	if (engGroup < 0 || engGroup >= CMDQ_MAX_GROUP_COUNT)
		return false;

	return true;
}

s32 cmdq_core_is_group_flag(enum CMDQ_GROUP_ENUM engGroup, u64 engineFlag)
{
	if (!cmdq_core_is_valid_group(engGroup))
		return false;

	if (cmdq_mdp_get_func()->getEngineGroupBits(engGroup) & engineFlag)
		return true;

	return false;
}

static inline u32 cmdq_core_get_task_timeout_cycle(
	struct ThreadStruct *pThread)
{
	/* if there is loop callback, this thread is in loop mode,
	 * and should not have a timeout.
	 * So pass 0 as "no timeout"
	 */

	/* return pThread->loopCallback ? 0 : CMDQ_MAX_INST_CYCLE; */

	/* HACK: disable HW timeout */
	return 0;
}

void cmdqCoreInitGroupCB(void)
{
	memset(&(gCmdqGroupCallback), 0x0, sizeof(gCmdqGroupCallback));
	memset(&(gCmdqDebugCallback), 0x0, sizeof(gCmdqDebugCallback));
}

void cmdqCoreDeinitGroupCB(void)
{
	memset(&(gCmdqGroupCallback), 0x0, sizeof(gCmdqGroupCallback));
	memset(&(gCmdqDebugCallback), 0x0, sizeof(gCmdqDebugCallback));
}

s32 cmdqCoreRegisterCB(enum CMDQ_GROUP_ENUM engGroup,
	CmdqClockOnCB clockOn, CmdqDumpInfoCB dumpInfo,
	CmdqResetEngCB resetEng, CmdqClockOffCB clockOff)
{
	struct CmdqCBkStruct *pCallback;

	if (!cmdq_core_is_valid_group(engGroup))
		return -EFAULT;

	CMDQ_MSG("Register %d group engines' callback\n", engGroup);
	CMDQ_MSG("clockOn:%pf dumpInfo:%pf\n", clockOn, dumpInfo);
	CMDQ_MSG("resetEng:%pf clockOff:%pf\n", resetEng, clockOff);

	pCallback = &(gCmdqGroupCallback[engGroup]);

	pCallback->clockOn = clockOn;
	pCallback->dumpInfo = dumpInfo;
	pCallback->resetEng = resetEng;
	pCallback->clockOff = clockOff;

	return 0;
}

s32 cmdqCoreRegisterDispatchModCB(
	enum CMDQ_GROUP_ENUM engGroup, CmdqDispatchModuleCB dispatchMod)
{
	struct CmdqCBkStruct *pCallback;

	if (!cmdq_core_is_valid_group(engGroup))
		return -EFAULT;

	CMDQ_MSG("Register %d group engines' dispatch callback\n",
		engGroup);
	pCallback = &(gCmdqGroupCallback[engGroup]);
	pCallback->dispatchMod = dispatchMod;

	return 0;
}

s32 cmdqCoreRegisterDebugRegDumpCB(
	CmdqDebugRegDumpBeginCB beginCB, CmdqDebugRegDumpEndCB endCB)
{
	CMDQ_VERBOSE("Register reg dump: begin:%p end:%p\n",
		beginCB, endCB);
	gCmdqDebugCallback.beginDebugRegDump = beginCB;
	gCmdqDebugCallback.endDebugRegDump = endCB;
	return 0;
}

s32 cmdqCoreRegisterTrackTaskCB(enum CMDQ_GROUP_ENUM engGroup,
	CmdqTrackTaskCB trackTask)
{
	struct CmdqCBkStruct *pCallback;

	if (!cmdq_core_is_valid_group(engGroup))
		return -EFAULT;

	CMDQ_MSG("Register %d group engines' callback\n", engGroup);
	CMDQ_MSG("trackTask:%pf\n", trackTask);

	pCallback = &(gCmdqGroupCallback[engGroup]);

	pCallback->trackTask = trackTask;

	return 0;
}

s32 cmdqCoreRegisterErrorResetCB(enum CMDQ_GROUP_ENUM engGroup,
	CmdqErrorResetCB errorReset)
{
	struct CmdqCBkStruct *callback;

	if (!cmdq_core_is_valid_group(engGroup))
		return -EFAULT;

	CMDQ_MSG("Register %d group engines' callback\n", engGroup);
	CMDQ_MSG("errorReset:%pf\n", errorReset);

	callback = &(gCmdqGroupCallback[engGroup]);

	callback->errorReset = errorReset;

	return 0;
}

struct TaskStruct *cmdq_core_get_task_ptr(void *task_handle)
{
	struct TaskStruct *ptr = NULL;
	struct TaskStruct *task = NULL;

	CMDQ_PROF_MUTEX_LOCK(gCmdqTaskMutex, valid_task_ptr);

	list_for_each_entry(task, &gCmdqContext.taskActiveList, listEntry) {
		if (task == task_handle && TASK_STATE_IDLE !=
			task->taskState) {
			ptr = task;
			break;
		}
	}

	if (!ptr) {
		list_for_each_entry(task, &gCmdqContext.taskWaitList,
			listEntry) {
			if (task == task_handle && TASK_STATE_WAITING ==
				task->taskState) {
				ptr = task;
				break;
			}
		}
	}

	CMDQ_PROF_MUTEX_UNLOCK(gCmdqTaskMutex, valid_task_ptr);

	return ptr;
}

static void cmdq_core_release_buffer(struct TaskStruct *pTask)
{
	CMDQ_MSG("%s start\n", __func__);
	if (pTask->regResults)
		cmdq_core_free_reg_buffer(pTask);

	if (pTask->secData.addrMetadatas) {
		kfree(CMDQ_U32_PTR(pTask->secData.addrMetadatas));
		pTask->secData.addrMetadatas = 0;
	}

	kfree(pTask->userDebugStr);
	pTask->userDebugStr = NULL;

	/* Release buffer created by v3 */
	cmdq_core_release_v3_struct(pTask);

	cmdq_task_free_task_command_buffer(pTask);

	cmdq_task_deinit_profile_marker_data(pTask);
	CMDQ_MSG("%s end\n", __func__);
}

static void cmdq_core_enable_resource_clk_unlock(
	u64 engine_flag, bool enable)
{
	struct ResourceUnitStruct *res = NULL;

	list_for_each_entry(res, &gCmdqContext.resourceList, list_entry) {
		if (!(res->engine_flag & engine_flag))
			continue;

		CMDQ_LOG(
			"[Res] resource clock engine:0x%llx enable:%s\n",
			engine_flag, enable ? "true" : "false");
		cmdq_mdp_get_func()->enableMdpClock(enable, res->engine_id);
		break;
	}
}

static void cmdq_core_release_task_unlocked(struct TaskStruct *pTask)
{
	CMDQ_MSG("%s task:%p\n", __func__, pTask);
	pTask->taskState = TASK_STATE_IDLE;
	pTask->thread = CMDQ_INVALID_THREAD;

#if defined(CMDQ_SECURE_PATH_SUPPORT)
	kfree(pTask->secStatus);
	pTask->secStatus = NULL;
#endif

	cmdq_core_release_buffer(pTask);
	kfree(pTask->privateData);
	pTask->privateData = NULL;

	/* remove from active/waiting list */
	list_del_init(&(pTask->listEntry));
	/* insert into free list. Currently we don't shrink free list. */
	list_add_tail(&(pTask->listEntry), &gCmdqContext.taskFreeList);
	CMDQ_MSG("%s end\n", __func__);
}

static void cmdq_core_release_task(struct TaskStruct *pTask)
{
	CMDQ_PROF_MUTEX_LOCK(gCmdqTaskMutex, release_task_remove);
	cmdq_core_release_task_unlocked(pTask);
	CMDQ_PROF_MUTEX_UNLOCK(gCmdqTaskMutex, release_task_remove);
}

static void cmdq_core_release_task_in_queue(struct work_struct *workItem)
{
	struct TaskStruct *pTask = container_of(workItem, struct TaskStruct,
		autoReleaseWork);

	CMDQ_MSG("-->Work QUEUE: TASK: Release task structure 0x%p begin\n",
		pTask);

	CMDQ_PROF_MUTEX_LOCK(gCmdqTaskMutex, release_task_in_queue);

	pTask->taskState = TASK_STATE_IDLE;
	pTask->thread = CMDQ_INVALID_THREAD;

	cmdq_core_release_buffer(pTask);

#if defined(CMDQ_SECURE_PATH_SUPPORT)
	kfree(pTask->secStatus);
	pTask->secStatus = NULL;
#endif

	kfree(pTask->privateData);
	pTask->privateData = NULL;

	/* remove from active/waiting list */
	list_del_init(&(pTask->listEntry));
	/* insert into free list. Currently we don't shrink free list. */
	list_add_tail(&(pTask->listEntry), &gCmdqContext.taskFreeList);

	CMDQ_PROF_MUTEX_UNLOCK(gCmdqTaskMutex, release_task_in_queue);

	CMDQ_MSG("<--Work QUEUE: TASK: Release task structure end\n");
}

static void cmdq_core_auto_release_task(struct TaskStruct *pTask)
{
	CMDQ_MSG("-->TASK: Auto release task structure 0x%p begin\n", pTask);

	if (atomic_inc_return(&pTask->useWorkQueue) != 1) {
		/* this is called via auto release work,
		 * no need to put in work queue again
		 */
		cmdq_core_release_task(pTask);
	} else {
		/* Not auto release work, use for auto release task !
		 * the work item is embeded in pTask already
		 * but we need to initialized it
		 */
		INIT_WORK(&pTask->autoReleaseWork,
			cmdq_core_release_task_in_queue);
		queue_work(gCmdqContext.taskAutoReleaseWQ,
			&pTask->autoReleaseWork);
	}

	CMDQ_MSG("<--TASK: Auto release task structure end\n");
}

/*
 * Re-fetch thread's command buffer
 * Usage:
 *     If SW motifies command buffer content after SW configed command to GCE,
 *     SW should notify GCE to re-fetch command in order to ensure
 *     inconsistent command buffer content between DRAM and GCE's SRAM.
 */
void cmdq_core_invalidate_hw_fetched_buffer(s32 thread)
{
	/* Setting HW thread PC will invoke that
	 * GCE (CMDQ HW) gives up fetched command buffer,
	 * and fetch command from DRAM to GCE's SRAM again.
	 */
	const s32 pc = CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread));

	CMDQ_REG_SET32(CMDQ_THR_CURR_ADDR(thread), pc);
}

/*
 * Non-pre-fetch thread fetch multiple instructions one time.
 * Use suspend/resume thread to restart current thread
 * after modify instruction in DRAM to force thread featch
 * new instructions again.
 */
static void cmdq_core_invalid_hw_thread(s32 thread)
{
	s32 status = cmdq_core_suspend_HW_thread(thread, __LINE__);

	if (status < 0)
		CMDQ_ERR("fail to invalid in runtime, still do resume\n");
	cmdq_core_resume_HW_thread(thread);
}

void cmdq_core_fix_command_scenario_for_user_space(
	struct cmdqCommandStruct *pCommand)
{
	if ((pCommand->scenario == CMDQ_SCENARIO_USER_DISP_COLOR)
	    || (pCommand->scenario == CMDQ_SCENARIO_USER_MDP)) {
		CMDQ_VERBOSE("user space request, scenario:%d\n",
			pCommand->scenario);
	} else {
		CMDQ_VERBOSE(
			"[WARNING]fix user space request to CMDQ_SCENARIO_USER_SPACE\n");
		pCommand->scenario = CMDQ_SCENARIO_USER_SPACE;
	}
}

bool cmdq_core_is_request_from_user_space(
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

static s32 cmdq_core_extend_new_buf(struct TaskStruct *task,
	struct CmdBufferStruct **buffer_entry_out)
{
	bool is_eoc_end = (task->bufferSize >= 2 * CMDQ_INST_SIZE &&
		((task->pCMDEnd[0] >> 24) & 0xff) == CMDQ_CODE_JUMP &&
		((task->pCMDEnd[-2] >> 24) & 0xff) == CMDQ_CODE_EOC);
	s32 status;
	struct CmdBufferStruct *buffer_entry;

	/* If last 2 instruction is EOC+JUMP,
	 * DO NOT copy last instruction to new buffer.
	 * So that we keep pCMDEnd[-2]:pCMDEnd[-3] can offset
	 * to EOC directly.
	 */
	if (is_eoc_end) {
		CMDQ_AEE("CMDQ",
			"Extend after EOC+END not support, task:0x%p inst:0x%08x:%08x size:%u\n",
			task, task->pCMDEnd[-1], task->pCMDEnd[0],
			task->bufferSize);
		return -EINVAL;
	}

	status = cmdq_core_task_alloc_single_buffer_list(
		task, &buffer_entry);
	if (status < 0)
		return status;

	/* copy last instruction to head of new buffer and
	 * use jump to replace
	 */
	buffer_entry->pVABase[0] = task->pCMDEnd[-1];
	buffer_entry->pVABase[1] = task->pCMDEnd[0];

	/* In normal case, insert jump to jump start of new buffer. */
	task->pCMDEnd[-1] = CMDQ_PHYS_TO_AREG(
		buffer_entry->MVABase);
	/* jump to absolute addr */
	task->pCMDEnd[0] = (CMDQ_CODE_JUMP << 24 | 1);

	/* update pCMDEnd to new buffer when not eoc end case */
	task->pCMDEnd = buffer_entry->pVABase + 1;
	/* update buffer size since we insert 1 jump update available size */
	task->buf_available_size -= CMDQ_INST_SIZE;
	/* +1 for jump instruction */
	task->bufferSize += CMDQ_INST_SIZE;

	if (unlikely(!cmdq_core_task_is_buffer_size_valid(task))) {
		CMDQ_AEE("CMDQ",
			"Buffer size:%u available size:%u of %u and end cmd:0x%p first va:0x%p out of sync!\n",
			task->bufferSize, task->buf_available_size,
			(u32)CMDQ_CMD_BUFFER_SIZE, task->pCMDEnd,
			cmdq_core_task_get_first_va(task));
		cmdq_core_dump_task(task);
	}

	*buffer_entry_out = buffer_entry;

	return 0;
}

static s32 cmdq_core_extend_cmd_buffer(struct TaskStruct *pTask)
{
	s32 status = 0;
	struct CmdBufferStruct *buffer_entry = NULL;
	u32 *va = NULL;

	va = cmdq_core_task_get_last_va(pTask);
	CMDQ_MSG(
		"Extend from buffer size:%u available size:%u end:0x%p last va:0x%p\n",
		pTask->bufferSize, pTask->buf_available_size,
		pTask->pCMDEnd, va);

	if (pTask->pCMDEnd) {
		/* There are instructions exist in available buffers.
		 * Check if buffer full and allocate new one.
		 */
		if (pTask->buf_available_size == 0) {
			status = cmdq_core_extend_new_buf(pTask,
				&buffer_entry);
			if (status < 0)
				return status;
		}
	} else {
		/* allocate first buffer */
		status = cmdq_core_task_alloc_single_buffer_list(
			pTask, &buffer_entry);
		if (status < 0)
			return status;

		pTask->pCMDEnd = buffer_entry->pVABase - 1;

		if (pTask->bufferSize) {
			/* no instruction, buffer size should be 0 */
			CMDQ_ERR("Task buffer size not sync, size:%u\n",
				pTask->bufferSize);
		}
	}

	va = cmdq_core_task_get_last_va(pTask);
	CMDQ_MSG(
		"Extend to buffer size:%u available size:%u end:0x%p last va:0x%p\n",
		pTask->bufferSize, pTask->buf_available_size,
		pTask->pCMDEnd, va);

	return status;
}

static void cmdq_core_append_command(struct TaskStruct *pTask,
	u32 arg_a, u32 arg_b)
{
	if (pTask->buf_available_size < CMDQ_INST_SIZE) {
		if (cmdq_core_extend_cmd_buffer(pTask) < 0)
			return;
	}

	pTask->pCMDEnd[1] = arg_b;
	pTask->pCMDEnd[2] = arg_a;
	pTask->commandSize += 1 * CMDQ_INST_SIZE;
	pTask->pCMDEnd += 2;

	pTask->bufferSize += CMDQ_INST_SIZE;
	pTask->buf_available_size -= CMDQ_INST_SIZE;
}

static void cmdq_core_dump_all_task(void)
{
	struct TaskStruct *ptr = NULL;
	struct list_head *p = NULL;

	CMDQ_PROF_MUTEX_LOCK(gCmdqTaskMutex, dump_all_task);

	CMDQ_ERR("=============== [CMDQ] All active tasks ===============\n");
	list_for_each(p, &gCmdqContext.taskActiveList) {
		ptr = list_entry(p, struct TaskStruct, listEntry);
		if (cmdq_core_is_valid_in_active_list(ptr) == true)
			cmdq_core_dump_task(ptr);
	}

	CMDQ_ERR("=============== [CMDQ] All wait tasks ===============\n");
	list_for_each(p, &gCmdqContext.taskWaitList) {
		ptr = list_entry(p, struct TaskStruct, listEntry);
		if (ptr->taskState == TASK_STATE_WAITING)
			cmdq_core_dump_task(ptr);
	}

	CMDQ_PROF_MUTEX_UNLOCK(gCmdqTaskMutex, dump_all_task);
}

static void cmdq_core_insert_backup_instr(struct TaskStruct *pTask,
	const u32 regAddr,
	const dma_addr_t writeAddress,
	const enum CMDQ_DATA_REGISTER_ENUM valueRegId,
	const enum CMDQ_DATA_REGISTER_ENUM destRegId)
{
	u32 arg_a;
	s32 subsysCode;
	u32 highAddr = 0;

	/* register to read from
	 * note that we force convert to physical reg address.
	 * if it is already physical address, it won't be affected
	 * (at least on this platform)
	 */
	arg_a = regAddr;
	subsysCode = cmdq_core_subsys_from_phys_addr(arg_a);

	if (subsysCode == CMDQ_SPECIAL_SUBSYS_ADDR) {
		CMDQ_LOG("Backup: Special handle memory base address 0x%08x\n",
			arg_a);
		/* Move extra handle APB address to destRegId */
		cmdq_core_append_command(pTask,
			(CMDQ_CODE_MOVE << 24) | ((destRegId & 0x1f) << 16) |
			(4 << 21), arg_a);
		/* Use arg-A GPR enable instruction to read destRegId value
		 * to valueRegId
		 */
		cmdq_core_append_command(pTask,
			(CMDQ_CODE_READ << 24) | ((destRegId & 0x1f) << 16) |
			(6 << 21), valueRegId);
	} else if (-1 == subsysCode) {
		CMDQ_ERR("Backup: Unsupported memory base address 0x%08x\n",
			arg_a);
	} else {
		/* Load into 32-bit GPR (R0-R15) */
		cmdq_core_append_command(pTask, (CMDQ_CODE_READ << 24) |
			(arg_a & 0xffff) | ((subsysCode & 0x1f) << 16) |
			(2 << 21), valueRegId);
	}

	/* Note that <MOVE> arg_b is 48-bit
	 * so writeAddress is split into 2 parts
	 * and we store address in 64-bit GPR (P0-P7)
	 */
	CMDQ_GET_HIGH_ADDR(writeAddress, highAddr);
	cmdq_core_append_command(pTask, (CMDQ_CODE_MOVE << 24) | highAddr |
		((destRegId & 0x1f) << 16) | (4 << 21), (u32) writeAddress);

	/* write to memory */
	cmdq_core_append_command(pTask, (CMDQ_CODE_WRITE << 24) |
		(0 & 0xffff) | ((destRegId & 0x1f) << 16) | (6 << 21),
		valueRegId);

	CMDQ_VERBOSE("COMMAND: copy reg:0x%08x to phys:%pa, GPR(%d, %d)\n",
		arg_a, &writeAddress, valueRegId, destRegId);
}

/*
 * Insert instruction to back secure threads' cookie count to normal world
 * Return:
 *     < 0, return the error code
 *     >=0, okay case, return number of bytes for inserting instruction
 */
#ifdef CMDQ_SECURE_PATH_NORMAL_IRQ
static s32 cmdq_core_insert_backup_cookie_instr(
	struct TaskStruct *pTask, s32 thread)
{
	const enum CMDQ_DATA_REGISTER_ENUM valueRegId = CMDQ_DATA_REG_DEBUG;
	const enum CMDQ_DATA_REGISTER_ENUM destRegId =
		CMDQ_DATA_REG_DEBUG_DST;
	const u32 regAddr = CMDQ_THR_EXEC_CNT_PA(thread);
	u64 addrCookieOffset = CMDQ_SEC_SHARED_THR_CNT_OFFSET + thread *
		sizeof(u32);
	u64 WSMCookieAddr = gCmdqContext.hSecSharedMem->MVABase +
		addrCookieOffset;
	const u32 subsysBit = cmdq_get_func()->getSubsysLSBArgA();
	s32 subsysCode = cmdq_core_subsys_from_phys_addr(regAddr);
	u32 highAddr = 0;

	const enum CMDQ_EVENT_ENUM regAccessToken = CMDQ_SYNC_TOKEN_GPR_SET_4;

	if (!gCmdqContext.hSecSharedMem) {
		CMDQ_ERR("%s shared memory is not created\n", __func__);
		return -EFAULT;
	}

	/* use SYNC TOKEN to make sure only 1 thread access at a time
	 * bit 0-11: wait_value
	 * bit 15: to_wait, true
	 * bit 31: to_update, true
	 * bit 16-27: update_value
	 * wait and clear
	 */
	cmdq_core_append_command(pTask, (CMDQ_CODE_WFE << 24) |
		regAccessToken, ((1 << 31) | (1 << 15) | 1));

	if (subsysCode != CMDQ_SPECIAL_SUBSYS_ADDR) {
		/* Load into 32-bit GPR (R0-R15) */
		cmdq_core_append_command(pTask,
			(CMDQ_CODE_READ << 24) | (regAddr & 0xffff) |
			((subsysCode & 0x1f) << subsysBit) | (2 << 21),
			valueRegId);
	} else {
		/* for special sw subsys addr,
		 * we don't read directly due to append command will acquire
		 * CMDQ_SYNC_TOKEN_GPR_SET_4 event again.
		 */

		/* set GPR to address */
		cmdq_core_append_command(pTask,
			(CMDQ_CODE_MOVE << 24) | ((valueRegId & 0x1f) << 16) |
			(4 << 21), regAddr);

		/* read data from address in GPR to GPR */
		cmdq_core_append_command(pTask,
			(CMDQ_CODE_READ << 24) | ((valueRegId & 0x1f) << 16) |
			(6 << 21), valueRegId);
	}

	/* cookie +1 because we get before EOC */
	cmdq_core_append_command(pTask,
		(CMDQ_CODE_LOGIC << 24) | (1 << 23) | (1 << 22) |
		(CMDQ_LOGIC_ADD << 16) |
		(valueRegId + CMDQ_GPR_V3_OFFSET),
		((valueRegId + CMDQ_GPR_V3_OFFSET) << 16) | 0x1);

	/* Note that <MOVE> arg_b is 48-bit
	 * so writeAddress is split into 2 parts
	 * and we store address in 64-bit GPR (P0-P7)
	 */
	CMDQ_GET_HIGH_ADDR(WSMCookieAddr, highAddr);
	cmdq_core_append_command(pTask,
		(CMDQ_CODE_MOVE << 24) | highAddr |
		((destRegId & 0x1f) << 16) | (4 << 21), (u32) WSMCookieAddr);

	/* write to memory */
	cmdq_core_append_command(pTask,
		(CMDQ_CODE_WRITE << 24) |
		((destRegId & 0x1f) << 16) | (6 << 21), valueRegId);

	/* set directly */
	cmdq_core_append_command(pTask, (CMDQ_CODE_WFE << 24) |
		regAccessToken, ((1 << 31) | (1 << 16)));

	return 0;
}
#endif	/* CMDQ_SECURE_PATH_NORMAL_IRQ */

/*
 * Insert instruction to backup secure threads' cookie and IRQ to normal world
 * Return:
 *     < 0, return the error code
 *     >=0, okay case, return number of bytes for inserting instruction
 */
#ifdef CMDQ_SECURE_PATH_HW_LOCK
static s32 cmdq_core_insert_secure_IRQ_instr(
	struct TaskStruct *pTask, s32 thread)
{
	const u32 originalSize = pTask->commandSize;
	const enum CMDQ_EVENT_ENUM regAccessToken = CMDQ_SYNC_TOKEN_GPR_SET_4;
	const enum CMDQ_DATA_REGISTER_ENUM valueRegId = CMDQ_DATA_REG_DEBUG;
	const enum CMDQ_DATA_REGISTER_ENUM destRegId = CMDQ_DATA_REG_DEBUG_DST;
	const u32 regAddr = CMDQ_THR_EXEC_CNT_PA(thread);
	u64 addrCookieOffset = CMDQ_SEC_SHARED_THR_CNT_OFFSET + thread *
		sizeof(u32);
	u64 WSMCookieAddr = gCmdqContext.hSecSharedMem->MVABase +
		addrCookieOffset;
	u64 WSMIRQAddr = gCmdqContext.hSecSharedMem->MVABase +
		CMDQ_SEC_SHARED_IRQ_RAISED_OFFSET;
	const u32 subsysBit = cmdq_get_func()->getSubsysLSBArgA();
	s32 subsysCode = cmdq_core_subsys_from_phys_addr(regAddr);
	s32 offset;

	if (cmdq_get_func()->isSecureThread(thread) == false) {
		CMDQ_ERR("%s, invalid param, thread:%d\n", __func__, thread);
		return -EFAULT;
	}

	if (!gCmdqContext.hSecSharedMem) {
		CMDQ_ERR("%s, shared memory is not created\n", __func__);
		return -EFAULT;
	}

	/* Shift JUMP and EOC */
	pTask->pCMDEnd[22] = pTask->pCMDEnd[0];
	pTask->pCMDEnd[21] = pTask->pCMDEnd[-1];
	pTask->pCMDEnd[20] = pTask->pCMDEnd[-2];
	pTask->pCMDEnd[19] = pTask->pCMDEnd[-3];

	pTask->pCMDEnd -= 4;

	/* use SYNC TOKEN to make sure only 1 thread access at a time */
	/* bit 0-11: wait_value */
	/* bit 15: to_wait, true */
	/* bit 31: to_update, true */
	/* bit 16-27: update_value */
	/* wait and clear */
	/* set unlock WSM resource directly */
	cmdq_core_append_command(pTask, (CMDQ_CODE_WFE << 24) |
		CMDQ_SYNC_SECURE_WSM_LOCK, ((1 << 31) | (1 << 15) | 1));

	cmdq_core_append_command(pTask, (CMDQ_CODE_WFE << 24) |
		regAccessToken, ((1 << 31) | (1 << 15) | 1));

	/* Load into 32-bit GPR (R0-R15) */
	cmdq_core_append_command(pTask, (CMDQ_CODE_READ << 24) |
		(regAddr & 0xffff) | ((subsysCode & 0x1f) << subsysBit) |
		(2 << 21), valueRegId);

	/* Note that <MOVE> arg_b is 48-bit
	 * so writeAddress is split into 2 parts
	 * and we store address in 64-bit GPR (P0-P7)
	 */
	cmdq_core_append_command(pTask, (CMDQ_CODE_MOVE << 24) |
		((WSMCookieAddr >> 32) & 0xffff) |
		((destRegId & 0x1f) << 16) | (4 << 21), (u32) WSMCookieAddr);

	/* write to memory */
	cmdq_core_append_command(pTask, (CMDQ_CODE_WRITE << 24) |
		((destRegId & 0x1f) << 16) | (6 << 21), valueRegId);

	/* Write GCE secure thread's IRQ to WSM */
	cmdq_core_append_command(pTask, (CMDQ_CODE_MOVE << 24),
		~(1 << thread));

	cmdq_core_append_command(pTask, (CMDQ_CODE_MOVE << 24) |
		((destRegId & 0x1f) << 16) | (4 << 21), WSMIRQAddr);

	cmdq_core_append_command(pTask, (CMDQ_CODE_WRITE << 24) |
		((destRegId & 0x1f) << 16) | (4 << 21) | 1, (1 << thread));

	/* set directly */
	cmdq_core_append_command(pTask, (CMDQ_CODE_WFE << 24) |
		regAccessToken, ((1 << 31) | (1 << 16)));

	/* set unlock WSM resource directly */
	cmdq_core_append_command(pTask, (CMDQ_CODE_WFE << 24) |
		CMDQ_SYNC_SECURE_WSM_LOCK, ((1 << 31) | (1 << 16)));

	/* set notify thread token directly */
	cmdq_core_append_command(pTask, (CMDQ_CODE_WFE << 24) |
		CMDQ_SYNC_SECURE_THR_EOF, ((1 << 31) | (1 << 16)));

	pTask->pCMDEnd += 4;

	/* secure thread doesn't raise IRQ */
	pTask->pCMDEnd[-3] = pTask->pCMDEnd[-3] & 0xFFFFFFFE;

	/* calculate added command length */
	offset = pTask->commandSize - originalSize;

	CMDQ_VERBOSE("%s offset:%d\n", __func__, offset);

	return offset;
}
#endif

static s32 cmdq_core_insert_secure_handle_instr(
	struct TaskStruct *pTask, s32 thread)
{
#ifdef CMDQ_SECURE_PATH_HW_LOCK
	return cmdq_core_insert_secure_IRQ_instr(pTask, thread);
#else
#ifdef CMDQ_SECURE_PATH_NORMAL_IRQ
	return cmdq_core_insert_backup_cookie_instr(pTask, thread);
#else
	return 0;
#endif
#endif
}

static void cmdq_core_reorder_task_array(
	struct ThreadStruct *pThread, s32 thread, s32 prevID)
{
	int loop, nextID, searchLoop, searchID;
	int reorderCount = 0;

	nextID = prevID + 1;
	for (loop = 1; loop < (cmdq_core_max_task_in_thread(thread) - 1);
		loop++, nextID++) {
		if (nextID >= cmdq_core_max_task_in_thread(thread))
			nextID = 0;

		if (pThread->pCurTask[nextID])
			break;

		searchID = nextID + 1;
		for (searchLoop = (loop + 1); searchLoop <
			cmdq_core_max_task_in_thread(thread);
			searchLoop++, searchID++) {
			if (searchID >= cmdq_core_max_task_in_thread(thread))
				searchID = 0;

			if (pThread->pCurTask[searchID]) {
				pThread->pCurTask[nextID] =
					pThread->pCurTask[searchID];
				pThread->pCurTask[searchID] = NULL;
				CMDQ_MSG("WAIT: reorder slot %d to slot 0%d.\n",
					     searchID, nextID);
				if ((searchLoop - loop) > reorderCount)
					reorderCount = searchLoop - loop;

				break;
			}
		}

		if (pThread->pCurTask[nextID] && CMDQ_IS_END_INSTR(
			pThread->pCurTask[nextID]->pCMDEnd)) {
			/* We reached the last task */
			CMDQ_LOG(
				"Break in last task loop:%d nextID:%d searchLoop:%d searchID:%d\n",
			loop, nextID, searchLoop, searchID);
			break;
		}
	}

	pThread->nextCookie -= reorderCount;
	CMDQ_VERBOSE("WAIT: nextcookie minus %d.\n", reorderCount);
}

static s32 cmdq_core_copy_buffer_impl(
	void *dst, void *src, const u32 size,
	const bool copyFromUser)
{
	s32 status = 0;

	if (copyFromUser == false) {
		CMDQ_VERBOSE("COMMAND: Copy kernel to 0x%p\n", dst);
		memcpy(dst, src, size);
	} else {
		CMDQ_VERBOSE("COMMAND: Copy user to 0x%p\n", dst);
		if (copy_from_user(dst, src, size)) {
			CMDQ_AEE("CMDQ",
				 "CRDISPATCH_KEY:CMDQ Fail to copy from user 0x%p, size:%d\n",
				 src, size);
			status = -ENOMEM;
		}
	}

	return status;
}

s32 cmdq_core_copy_cmd_to_task_impl(
	struct TaskStruct *pTask, void *src, const u32 size,
	const bool is_copy_from_user)
{
	s32 status = 0;
	u32 remaind_cmd_size = size;
	u32 copy_size = 0;

	while (remaind_cmd_size > 0) {
		/* extend buffer to copy more instruction */
		status = cmdq_core_extend_cmd_buffer(pTask);

		copy_size = pTask->buf_available_size > remaind_cmd_size ?
			remaind_cmd_size : pTask->buf_available_size;
		status = cmdq_core_copy_buffer_impl(pTask->pCMDEnd + 1,
			src + size - remaind_cmd_size,
			copy_size, is_copy_from_user);
		if (status < 0)
			return status;

		/* update last instruction position */
		pTask->pCMDEnd += (copy_size / sizeof(u32));
		pTask->buf_available_size -= copy_size;
		pTask->bufferSize += copy_size;
		remaind_cmd_size -= copy_size;

		if (unlikely(!cmdq_core_task_is_buffer_size_valid(pTask))) {
			/* buffer size is total size and should sync
			 * with available space
			 */
			CMDQ_AEE("CMDQ",
				"Buffer size:%u available size:%u of %u and end cmd:0x%p first va:0x%p out of sync!\n",
				pTask->bufferSize, pTask->buf_available_size,
				(u32)CMDQ_CMD_BUFFER_SIZE, pTask->pCMDEnd,
				cmdq_core_task_get_first_va(pTask));
			cmdq_core_dump_task(pTask);
		}
	}

	return status;
}

static dma_addr_t cmdq_core_get_current_pa_addr(struct TaskStruct *pTask)
{
	return (cmdq_core_task_get_last_pa(pTask) + CMDQ_CMD_BUFFER_SIZE -
		pTask->buf_available_size);
}

bool cmdq_core_verfiy_command_desc_end(struct cmdqCommandStruct *pCommandDesc)
{
	u32 *pCMDEnd = NULL;
	bool valid = true;
	bool internal_desc = pCommandDesc->privateData &&
		((struct TaskPrivateStruct *)
		(CMDQ_U32_PTR(pCommandDesc->privateData)))->internal;

	/* make sure we have sufficient command to parse */
	if (!CMDQ_U32_PTR(pCommandDesc->pVABase) ||
		pCommandDesc->blockSize < (2 * CMDQ_INST_SIZE))
		return false;

	if (cmdq_core_is_request_from_user_space(pCommandDesc->scenario)) {
		/* command buffer has not copied from user space yet,
		 * skip verify.
		 */
		return true;
	}

	pCMDEnd = CMDQ_U32_PTR(pCommandDesc->pVABase) +
	    (pCommandDesc->blockSize / sizeof(u32)) - 1;

	/* make sure the command is ended by EOC + JUMP */
	if ((pCMDEnd[-3] & 0x1) != 1 && !internal_desc) {
		CMDQ_ERR(
			"[CMD] command desc 0x%p does not throw IRQ (%08x:%08x) pEnd:%p(%p,%d)\n",
			pCommandDesc, pCMDEnd[-3], pCMDEnd[-2], pCMDEnd,
			CMDQ_U32_PTR(pCommandDesc->pVABase),
			pCommandDesc->blockSize);
		valid = false;
	}

	if (((pCMDEnd[-2] & 0xFF000000) >> 24) != CMDQ_CODE_EOC ||
	    ((pCMDEnd[0] & 0xFF000000) >> 24) != CMDQ_CODE_JUMP) {
		CMDQ_ERR(
			"[CMD] command desc 0x%p does not end in EOC+JUMP (%08x:%08x, %08x:%08x), pEnd:%p(%p, %d)\n",
			pCommandDesc, pCMDEnd[-3], pCMDEnd[-2], pCMDEnd[-1],
			pCMDEnd[0], pCMDEnd,
			CMDQ_U32_PTR(pCommandDesc->pVABase),
			pCommandDesc->blockSize);
		valid = false;
	}

	if (!valid) {
		/* invalid command, raise AEE */
		CMDQ_AEE("CMDQ", "INVALID command desc 0x%p\n", pCommandDesc);
	}

	return valid;
}

bool cmdq_core_verfiy_command_end(const struct TaskStruct *pTask)
{
	bool valid = true;
	bool noIRQ = false;
	u32 *last_inst = NULL;
	struct CmdBufferStruct *cmd_buffer = NULL;

	/* make sure we have sufficient command to parse */
	if (list_empty(&pTask->cmd_buffer_list) ||
		pTask->commandSize < (2 * CMDQ_INST_SIZE))
		return false;

#ifdef CMDQ_SECURE_PATH_HW_LOCK
	if ((pTask->pCMDEnd[-3] & 0x1) != 1 && !pTask->secData.is_secure)
		noIRQ = true;
#else
	if ((pTask->pCMDEnd[-3] & 0x1) != 1)
		noIRQ = true;
#endif

	/* make sure the command is ended by EOC + JUMP */
	if (noIRQ && !CMDQ_TASK_IS_INTERNAL(pTask)) {
		if (cmdq_get_func()->is_disp_loop(pTask->scenario)) {
			/* Allow display only loop not throw IRQ */
			CMDQ_MSG(
				"[CMD] DISP Loop pTask 0x%p does not throw IRQ (%08x:%08x)\n",
				 pTask, pTask->pCMDEnd[-3],
				 pTask->pCMDEnd[-2]);
		} else {
			CMDQ_ERR(
				"[CMD] pTask 0x%p does not throw IRQ (%08x:%08x)\n",
				 pTask, pTask->pCMDEnd[-3],
				 pTask->pCMDEnd[-2]);
			valid = false;
		}
	}
	if (((pTask->pCMDEnd[-2] & 0xFF000000) >> 24) != CMDQ_CODE_EOC ||
	    ((pTask->pCMDEnd[0] & 0xFF000000) >> 24) != CMDQ_CODE_JUMP) {
		CMDQ_ERR(
			"[CMD] pTask 0x%p does not end in EOC+JUMP (%08x:%08x, %08x:%08x)\n",
			pTask, pTask->pCMDEnd[-3], pTask->pCMDEnd[-2],
			pTask->pCMDEnd[-1], pTask->pCMDEnd[0]);
		valid = false;
	}

	/* verify end of each buffer will jump to next buffer */
	list_for_each_entry(cmd_buffer, &pTask->cmd_buffer_list, listEntry) {
		bool last_entry = list_is_last(&cmd_buffer->listEntry,
			&pTask->cmd_buffer_list);

		if (last_inst) {
			if (last_entry && pTask->pCMDEnd - 1 == last_inst) {
				/* EOC+JUMP command locate at last 2nd buffer,
				 * skip test
				 */
				break;
			}
			if ((last_inst[1] & 0x1) != 1 || last_inst[0] !=
				cmd_buffer->MVABase) {
				CMDQ_ERR(
					"Invalid task:0x%p buffer jump instruction:0x%08x:%08x next PA:%pa cmd end:0x%p\n",
					pTask, last_inst[1], last_inst[0],
					&cmd_buffer->MVABase, pTask->pCMDEnd);
				cmdq_core_dump_buffer(pTask);
				valid = false;
				break;
			}
		}

		if (!last_entry) {
			last_inst =
				&cmd_buffer->pVABase[CMDQ_CMD_BUFFER_SIZE /
				sizeof(u32) - 2];
			if (last_inst[1] >> 24 != CMDQ_CODE_JUMP) {
				CMDQ_ERR(
					"Invalid task:0x%p instruction:0x%08x:%08x is not jump\n",
					pTask, last_inst[1], last_inst[0]);
				cmdq_core_dump_buffer(pTask);
				valid = false;
				break;
			}
		}
	}

	if (!valid) {
		/* Raise AEE */
		CMDQ_AEE("CMDQ", "INVALID pTask 0x%p\n", pTask);
	}

	return valid;
}

bool cmdq_core_task_finalize_end(struct TaskStruct *pTask)
{
	dma_addr_t pa = 0;

	if (cmdq_core_verfiy_command_end(pTask) == false)
		return false;

	/* Check if necessary to jump physical addr. */
	if ((pTask->pCMDEnd[0] & 0x1) == 0 && pTask->pCMDEnd[-1] == 0x8) {
		/*
		 * JUMP to next instruction case.
		 * Set new JUMP to GCE end address
		 */
		pa = CMDQ_GCE_END_ADDR_PA;
		pTask->pCMDEnd[-1] = CMDQ_PHYS_TO_AREG(pa);
		pTask->pCMDEnd[0] = (CMDQ_CODE_JUMP << 24 | 0x1);

		CMDQ_MSG(
			"Finalize JUMP:0x%08x:%08x last pa:%pa buffer size:%d cmd size:%d line:%d\n",
			pTask->pCMDEnd[0], pTask->pCMDEnd[-1], &pa,
			pTask->bufferSize, pTask->commandSize, __LINE__);
	} else if ((pTask->pCMDEnd[0] & 0x1) == 0 &&
		*((s32 *)(pTask->pCMDEnd - 1)) ==
		(-pTask->commandSize + CMDQ_INST_SIZE)) {
		/* JUMP to head of command, loop case. */
		pa = cmdq_core_task_get_first_pa(pTask);
		pTask->pCMDEnd[-1] = CMDQ_PHYS_TO_AREG(pa);
		pTask->pCMDEnd[0] = (CMDQ_CODE_JUMP << 24 | 0x1);

		CMDQ_MSG(
			"Finalize JUMP:0x%08x:%08x first pa:%pa buffer size:%d cmd size:%d line:%d\n",
			pTask->pCMDEnd[0], pTask->pCMDEnd[-1], &pa,
			pTask->bufferSize, pTask->commandSize, __LINE__);
	} else {
		CMDQ_AEE("CMDQ",
			"Final JUMP un-expect task:0x%p inst:0x%p 0x%08x:%08x size:%u(%u)\n",
			pTask,
			pTask->pCMDEnd,
			pTask->pCMDEnd[0], pTask->pCMDEnd[-1],
			pTask->commandSize, pTask->bufferSize);
		return false;
	}

	return true;
}

static struct TaskStruct *cmdq_core_find_free_task(void)
{
	struct TaskStruct *pTask = NULL;

	CMDQ_PROF_MUTEX_LOCK(gCmdqTaskMutex, find_free_task);

	/* Pick from free list first; */
	/* create one if there is no free entry. */
	if (!list_empty(&gCmdqContext.taskFreeList)) {
		pTask = list_first_entry(&(gCmdqContext.taskFreeList),
			struct TaskStruct, listEntry);
		/* remove from free list */
		list_del_init(&(pTask->listEntry));
	}

	CMDQ_PROF_MUTEX_UNLOCK(gCmdqTaskMutex, find_free_task);

	if (!pTask)
		pTask = cmdq_core_task_create();

	return pTask;
}

static s32 cmdq_core_insert_read_reg_command(
	struct TaskStruct *pTask, struct cmdqCommandStruct *pCommandDesc)
{
	/* #define CMDQ_PROFILE_COMMAND */

	s32 status = 0;
	u32 extraBufferSize = 0;
	int i = 0;
	enum CMDQ_DATA_REGISTER_ENUM valueRegId;
	enum CMDQ_DATA_REGISTER_ENUM destRegId;
	enum CMDQ_EVENT_ENUM regAccessToken;
	const bool userSpaceRequest =
		cmdq_core_is_request_from_user_space(pTask->scenario);
	bool postInstruction = false;

	s32 subsysCode;
	u32 physAddr;

	u32 *copyCmdSrc = NULL;
	u32 copyCmdSize = 0;
	u32 cmdLoopStartAddr = 0;

	/* calculate required buffer size */
	/* we need to consider {READ, MOVE, WRITE} for each register */
	/* and the SYNC in the begin and end */
	if (pTask->regCount && pTask->regCount <= CMDQ_MAX_DUMP_REG_COUNT) {
		extraBufferSize = (3 * CMDQ_INST_SIZE * pTask->regCount) +
			(2 * CMDQ_INST_SIZE);
		/* Add move instruction count for handle Extra APB address
		 * (add move instructions)
		 */
		for (i = 0; i < pTask->regCount; ++i) {
			physAddr = CMDQ_U32_PTR(
				pCommandDesc->regRequest.regAddresses)[i];
			subsysCode = cmdq_core_subsys_from_phys_addr(physAddr);
			if (subsysCode == CMDQ_SPECIAL_SUBSYS_ADDR)
				extraBufferSize += CMDQ_INST_SIZE;
		}
	} else {
		extraBufferSize = 0;
	}

	/* init pCMDEnd */
	/* mark command end to NULL as initial state */
	pTask->pCMDEnd = NULL;

	/* Copy replace instruction position */
	cmdq_core_copy_v3_struct(pTask, pCommandDesc);

	/* backup command start mva for loop case */
	cmdLoopStartAddr = (u32)cmdq_core_get_current_pa_addr(pTask);

	/* for post instruction process we copy last 2 inst later */
	postInstruction = (pTask->regCount && extraBufferSize) ||
		pTask->secData.is_secure;

	if (likely(!pTask->loopCallback))
		postInstruction = true;

	/* Copy the commands to our DMA buffer,
	 * except last 2 instruction EOC+JUMP.
	 */
	copyCmdSrc = CMDQ_U32_PTR(pCommandDesc->pVABase);
	/* end cmd will copy after post read */
	if (postInstruction)
		copyCmdSize = pCommandDesc->blockSize - 2 * CMDQ_INST_SIZE;
	else
		copyCmdSize = pCommandDesc->blockSize;
	status = cmdq_core_copy_cmd_to_task_impl(pTask, copyCmdSrc,
		copyCmdSize, userSpaceRequest);
	if (status < 0)
		return status;

	/* make sure instructions are really in DRAM */
	smp_mb();

	/* If no read request, no post-process needed. Do verify and stop */
	if (!postInstruction) {
		if (!cmdq_core_task_finalize_end(pTask)) {
			CMDQ_ERR(
				"[CMD] with smp_mb() cmdSize:%d bufferSize:%u blockSize:%d\n",
				pTask->commandSize, pTask->bufferSize,
				pCommandDesc->blockSize);
			cmdq_core_dump_task(pTask);
			cmdq_core_dump_all_task();
		}

		return 0;
	}

	/* Backup end and append cmd */
	if (pTask->regCount) {
		CMDQ_VERBOSE("COMMAND: allocate register output section\n");

		/* allocate register output section */
		cmdq_core_alloc_reg_buffer(pTask);

		/* allocate GPR resource */
		cmdq_get_func()->getRegID(pTask->engineFlag, &valueRegId,
			&destRegId, &regAccessToken);

		/* use SYNC TOKEN to make sure only 1 thread access at a time
		 * bit 0-11: wait_value
		 * bit 15: to_wait, true
		 * bit 31: to_update, true
		 * bit 16-27: update_value
		 */

		/* wait and clear */
		cmdq_core_append_command(pTask, (CMDQ_CODE_WFE << 24) |
			regAccessToken, ((1 << 31) | (1 << 15) | 1));

		for (i = 0; i < pTask->regCount; i++) {
			cmdq_core_insert_backup_instr(pTask,
				CMDQ_U32_PTR(
				pCommandDesc->regRequest.regAddresses)[i],
				pTask->regResultsMVA +
				(i * sizeof(pTask->regResults[0])),
				valueRegId, destRegId);
		}

		/* set directly */
		cmdq_core_append_command(pTask, (CMDQ_CODE_WFE << 24) |
			regAccessToken, ((1 << 31) | (1 << 16)));
	}

	if (pTask->secData.is_secure) {
		/* Use scenario to get thread ID before acquire thread,
		 * use ID to insert secure instruction.
		 * The acquire thread process may still fail later.
		 */
		s32 thread = cmdq_get_func()->getThreadID(pTask->scenario,
			true);

		status = cmdq_core_insert_secure_handle_instr(pTask, thread);
		if (status < 0)
			return status;
	}

	if (likely(!pTask->loopCallback)) {
		cmdq_core_append_command(pTask, (CMDQ_CODE_WFE << 24) |
			CMDQ_SYNC_TOKEN_APPEND_THR(-1),
			(0 << 31) | (1 << 15) | 1);
	}

	/* copy END instructsions EOC+JUMP */
	status = cmdq_core_copy_cmd_to_task_impl(pTask,
		copyCmdSrc + (copyCmdSize / sizeof(copyCmdSrc[0])),
		2 * CMDQ_INST_SIZE, userSpaceRequest);
	if (status < 0)
		return status;

	/* make sure instructions are really in DRAM */
	smp_mb();

	if (cmdq_core_task_finalize_end(pTask) == false) {
		CMDQ_ERR(
			"[CMD] with smp_mb() cmdSize:%d bufferSize:%u blockSize:%d\n",
			pTask->commandSize, pTask->bufferSize,
			pCommandDesc->blockSize);
		cmdq_core_dump_task(pTask);
		cmdq_core_dump_all_task();
	}

	return status;
}

static struct TaskStruct *cmdq_core_acquire_task(
	struct cmdqCommandStruct *pCommandDesc,
	struct CmdqRecExtend *ext, CmdqInterruptCB loopCB,
	unsigned long loopData)
{
	struct TaskStruct *pTask = NULL;
	s32 status;
	CMDQ_TIME time_cost;

	CMDQ_MSG(
		"-->TASK: acquire task begin CMD:0x%p size:%d Eng:0x%016llx\n",
		CMDQ_U32_PTR(pCommandDesc->pVABase), pCommandDesc->blockSize,
		pCommandDesc->engineFlag);
	CMDQ_PROF_START(current->pid, __func__);
	pTask = cmdq_core_find_free_task();
	CMDQ_PROF_TIME_BEGIN(time_cost);
	do {
		struct TaskPrivateStruct *private = NULL, *desc_private = NULL;

		if (!pTask) {
			CMDQ_AEE("CMDQ", "Can't acquire task info\n");
			break;
		}

		pTask->submit = sched_clock();

		/* initialize field values */
		pTask->scenario = pCommandDesc->scenario;
		pTask->priority = pCommandDesc->priority;
		pTask->engineFlag = pCommandDesc->engineFlag;
		pTask->loopCallback = loopCB;
		pTask->loopData = loopData;
		pTask->taskState = TASK_STATE_WAITING;
		pTask->reorder = 0;
		pTask->thread = CMDQ_INVALID_THREAD;
		pTask->irqFlag = 0x0;
		pTask->durAlloc = 0;
		pTask->durReclaim = 0;
		pTask->durRelease = 0;
		pTask->dumpAllocTime = false;
		atomic_set(&pTask->useWorkQueue, 0);
		pTask->userDebugStr = NULL;
#if defined(CMDQ_SECURE_PATH_SUPPORT)
		pTask->secStatus = NULL;
#endif

		if (ext) {
			pTask->res_engine_flag_acquire =
				ext->res_engine_flag_acquire;
			pTask->res_engine_flag_release =
				ext->res_engine_flag_release;
		} else {
			pTask->res_engine_flag_acquire = 0;
			pTask->res_engine_flag_release = 0;
		}

		/* reset private data from desc */
		desc_private = (struct TaskPrivateStruct *)CMDQ_U32_PTR(
			pCommandDesc->privateData);
		if (desc_private) {
			private = kzalloc(sizeof(*private), GFP_KERNEL);
			pTask->privateData = private;
			if (private)
				*private = *desc_private;
		}

		/* secure exec data */
		pTask->secData.is_secure = pCommandDesc->secData.is_secure;
#ifdef CMDQ_SECURE_PATH_SUPPORT
		pTask->secData.enginesNeedDAPC =
			pCommandDesc->secData.enginesNeedDAPC;
		pTask->secData.enginesNeedPortSecurity =
		    pCommandDesc->secData.enginesNeedPortSecurity;
		pTask->secData.addrMetadataCount =
			pCommandDesc->secData.addrMetadataCount;
		if (pTask->secData.is_secure &&
			pTask->secData.addrMetadataCount > 0) {
			u32 metadata_length = 0;
			void *p_metadatas = NULL;

			metadata_length = (pTask->secData.addrMetadataCount) *
				sizeof(struct cmdqSecAddrMetadataStruct);
			/* create sec data task buffer for working */
			p_metadatas = kzalloc(metadata_length, GFP_KERNEL);
			if (!p_metadatas) {
				/* raise AEE first */
				CMDQ_AEE("CMDQ",
					"Can't alloc secData buffer count:%d alloacted_size:%d\n",
					 pTask->secData.addrMetadataCount,
					 metadata_length);

				/* then release task */
				cmdq_core_release_task(pTask);
				pTask = NULL;
				break;
			}
			memcpy(p_metadatas, CMDQ_U32_PTR(
				pCommandDesc->secData.addrMetadatas),
				metadata_length);
			pTask->secData.addrMetadatas =
				(cmdqU32Ptr_t)(unsigned long)p_metadatas;
		} else {
			pTask->secData.addrMetadatas = 0;
		}
#endif

		/* profile timers */
		memset(&(pTask->trigger), 0x0, sizeof(pTask->trigger));
		memset(&(pTask->gotIRQ), 0x0, sizeof(pTask->gotIRQ));
		memset(&(pTask->beginWait), 0x0, sizeof(pTask->beginWait));
		memset(&(pTask->wakedUp), 0x0, sizeof(pTask->wakedUp));

		/* profile marker */
		CMDQ_PROF_TIME_END(time_cost, "before_init_profile");
		cmdq_task_init_profile_marker_data(pCommandDesc, pTask);
		CMDQ_PROF_TIME_END(time_cost, "after_init_profile");

		pTask->commandSize = pCommandDesc->blockSize;
		pTask->regCount = pCommandDesc->regRequest.count;

		/* store caller info for debug */
		if (current) {
			pTask->callerPid = current->pid;
			memcpy(pTask->callerName, current->comm,
				sizeof(current->comm));
		}

		/* store user debug string for debug */
		if (pCommandDesc->userDebugStr &&
			pCommandDesc->userDebugStrLen > 0) {
			pTask->userDebugStr = kzalloc(
				pCommandDesc->userDebugStrLen, GFP_KERNEL);
			if (pTask->userDebugStr) {
				int len = 0;

				len = strncpy_from_user(pTask->userDebugStr,
						(const char *)(unsigned long)
						(pCommandDesc->userDebugStr),
						pCommandDesc->userDebugStrLen);
				if (len < 0) {
					CMDQ_ERR(
						"copy user debug memory failed size:%d\n",
						pCommandDesc->userDebugStrLen);
				} else if (len ==
					pCommandDesc->userDebugStrLen) {
					pTask->userDebugStr[
						pCommandDesc->userDebugStrLen -
						1] = '\0';
				}
				CMDQ_MSG("user debug string:%s\n",
					(const char *)(unsigned long)
					(pCommandDesc->userDebugStr));
			} else {
				CMDQ_ERR(
					"allocate user debug memory failed size:%d\n",
					pCommandDesc->userDebugStrLen);
			}
		}
		CMDQ_PROF_TIME_END(time_cost, "before_insert_read_reg");
		status = cmdq_core_insert_read_reg_command(pTask,
			pCommandDesc);
		CMDQ_PROF_TIME_END(time_cost, "after_insert_read_reg");
		if (status >= 0 && pCommandDesc->use_sram_buffer) {
			u32 cpr_offset = CMDQ_INVALID_CPR_OFFSET;
			dma_addr_t paBase = cmdq_core_task_get_first_pa(pTask);

			pTask->use_sram_buffer = true;
			/* handle SRAM task, copy command to SRAM at first */
			do {
				if (paBase == 0) {
					CMDQ_ERR(
						"SRAM task DMA buffer list is empty!\n");
					status = -EFAULT;
				} else {
					status = cmdq_core_alloc_sram_buffer(
						pTask->bufferSize,
						pCommandDesc->sram_owner_name,
						&cpr_offset);
					CMDQ_CHECK_AND_BREAK_STATUS(status);
					status = cmdq_task_copy_to_sram(
						paBase, cpr_offset,
						pTask->bufferSize);
					CMDQ_CHECK_AND_BREAK_STATUS(status);
				}
			} while (0);
			if (status >= 0) {
				pTask->sram_base = CMDQ_SRAM_ADDR(cpr_offset);
			} else {
				CMDQ_ERR("Prepare SRAM buffer task failed!\n");
				if (cpr_offset != CMDQ_INVALID_CPR_OFFSET)
					cmdq_core_free_sram_buffer(
						cpr_offset, pTask->bufferSize);
			}
		}
		if (status < 0) {
			/* raise AEE first */
			CMDQ_AEE("CMDQ", "Can't alloc command buffer\n");

			/* then release task */
			cmdq_core_release_task(pTask);
			pTask = NULL;
		}
	} while (0);

	/* insert into waiting list to process */
	CMDQ_PROF_MUTEX_LOCK(gCmdqTaskMutex, acquire_task_done);
	if (pTask) {
		struct list_head *insertAfter = &gCmdqContext.taskWaitList;

		struct TaskStruct *taskEntry = NULL;
		struct list_head *p = NULL;

		/* add to waiting list, keep it sorted by priority */
		/* so that we add high-priority tasks first. */
		list_for_each(p, &gCmdqContext.taskWaitList) {
			taskEntry = list_entry(p, struct TaskStruct,
				listEntry);
			/* keep the list sorted.
			 * higher priority tasks are inserted
			 * in front of the queue
			 */
			if (taskEntry->priority < pTask->priority)
				break;

			insertAfter = p;
		}

		list_add(&(pTask->listEntry), insertAfter);
	}
	CMDQ_PROF_MUTEX_UNLOCK(gCmdqTaskMutex, acquire_task_done);

	CMDQ_MSG("<--TASK: acquire task 0x%p end\n", pTask);
	CMDQ_PROF_END(current->pid, __func__);
	return pTask;
}

bool cmdq_core_is_clock_enabled(void)
{
	return (atomic_read(&gCmdqThreadUsage) > 0);
}

static void cmdq_core_enable_common_clock_locked(
	const bool enable, const u64 engineFlag,
	enum CMDQ_SCENARIO_ENUM scenario)
{
	CMDQ_VERBOSE("[CLOCK] %s CMDQ(GCE) Clock test=%d SMI %d\n",
		enable ? "enable" : "disable",
		atomic_read(&gCmdqThreadUsage),
		atomic_read(&gSMIThreadUsage));

	/* CMDQ(GCE) clock */
	if (enable) {
		s32 clock_count = atomic_inc_return(&gCmdqThreadUsage);

		if (clock_count == 1) {
			/* CMDQ init flow: */
			/* 1. clock-on */
			/* 2. reset all events */
			cmdq_get_func()->enableGCEClockLocked(enable);
			cmdq_core_reset_hw_events();
			cmdq_core_config_prefetch_gsize();
#ifdef CMDQ_ENABLE_BUS_ULTRA
			CMDQ_LOG("Enable GCE Ultra ability");
			CMDQ_REG_SET32(CMDQ_BUS_CONTROL_TYPE, 0x3);
#endif
			if (g_dts_setting.ctl_int0 > 0) {
				CMDQ_REG_SET32(CMDQ_CTL_INT0,
					g_dts_setting.ctl_int0);
				CMDQ_MSG("[CTL_INT0] set %d\n",
					g_dts_setting.ctl_int0);
			}
			/* Restore event */
			cmdq_get_func()->eventRestore();
		} else if (clock_count == 0) {
			CMDQ_ERR(
				"enable clock %s error usage:%d smi use:%d\n",
				__func__, clock_count,
				(s32)atomic_read(&gSMIThreadUsage));
		}

		/* SMI related threads common clock enable,
		 * excluding display scenario on his own
		 */
		if (!cmdq_get_func()->isDispScenario(scenario) &&
			likely(scenario != CMDQ_SCENARIO_MOVE &&
			scenario != CMDQ_SCENARIO_TIMER_LOOP)) {
			s32 smi_count = atomic_inc_return(&gSMIThreadUsage);

			if (smi_count == 1) {
				CMDQ_MSG("[CLOCK] SMI clock enable %d\n",
					smi_count);
				cmdq_mdp_get_func()->mdpEnableCommonClock(true);
			} else if (smi_count == 0) {
				CMDQ_ERR(
					"enable smi common %s error usage:%d smi use:%d\n",
					__func__, clock_count, smi_count);
			}
		}

	} else {
		s32 clock_count = atomic_dec_return(&gCmdqThreadUsage);

		if (clock_count == 0) {
			/* Backup event */
			cmdq_get_func()->eventBackup();
			/* clock-off */
			cmdq_get_func()->enableGCEClockLocked(enable);
		} else if (clock_count < 0) {
			CMDQ_ERR(
				"enable clock %s error usage:%d smi use:%d\n",
				__func__, clock_count,
				(s32)atomic_read(&gSMIThreadUsage));
		}

		/* SMI related threads common clock enable,
		 * excluding display scenario on his own
		 */
		if (!cmdq_get_func()->isDispScenario(scenario) &&
			likely(scenario != CMDQ_SCENARIO_MOVE &&
			scenario != CMDQ_SCENARIO_TIMER_LOOP)) {
			s32 smi_count = atomic_dec_return(&gSMIThreadUsage);

			if (smi_count == 0) {
				CMDQ_MSG("[CLOCK] SMI clock disable %d\n",
					smi_count);
				cmdq_mdp_get_func()->mdpEnableCommonClock(
					false);
			} else if (smi_count < 0) {
				CMDQ_ERR(
					"disable smi common %s error usage:%d smi use:%d\n",
					__func__, clock_count, smi_count);
			}
		}
	}
}

static u64 cmdq_core_get_actual_engine_flag_for_enable_clock(
	u64 engineFlag, s32 thread)
{
	struct EngineStruct *pEngine;
	struct ThreadStruct *pThread;
	u64 engines;
	s32 index;

	pEngine = gCmdqContext.engine;
	pThread = gCmdqContext.thread;
	engines = 0;
	for (index = 0; index < CMDQ_MAX_ENGINE_COUNT; index++) {
		if (engineFlag & (1LL << index)) {
			if (pEngine[index].userCount <= 0) {
				pEngine[index].currOwner = thread;
				engines |= (1LL << index);
				/* also assign engine flag into ThreadStruct */
				pThread[thread].engineFlag |= (1LL << index);
			}

			pEngine[index].userCount++;
		}
	}
	return engines;
}

static s32 gCmdqISPClockCounter;

static void cmdq_core_enable_clock(u64 engineFlag,
	s32 thread, u64 engineMustEnableClock,
	enum CMDQ_SCENARIO_ENUM scenario)
{
	const u64 engines = engineMustEnableClock;
	s32 index;
	struct CmdqCBkStruct *pCallback;
	s32 status;

	CMDQ_VERBOSE(
		"-->CLOCK: Enable flag 0x%llx thread %d begin, mustEnable:0x%llx(0x%llx)\n",
		engineFlag, thread, engineMustEnableClock, engines);

	/* enable fundamental clocks if needed */
	cmdq_core_enable_common_clock_locked(true, engineFlag, scenario);

	pCallback = gCmdqGroupCallback;

	/* ISP special check: Always call ISP on/off if this task */
	/* involves ISP. Ignore the ISP HW flags. */
	if (cmdq_core_is_group_flag(CMDQ_GROUP_ISP, engineFlag)) {
		CMDQ_VERBOSE("CLOCK: enable group %d clockOn\n",
			CMDQ_GROUP_ISP);

		if (!pCallback[CMDQ_GROUP_ISP].clockOn) {
			CMDQ_ERR(
				"CLOCK: enable group %d clockOn func NULL\n",
				CMDQ_GROUP_ISP);
		} else {
			status = pCallback[CMDQ_GROUP_ISP].clockOn(
				cmdq_mdp_get_func()->getEngineGroupBits(
				CMDQ_GROUP_ISP) & engineFlag);

#if 1
			++gCmdqISPClockCounter;
#endif

			if (status < 0) {
				/* Error status print */
				CMDQ_ERR(
					"CLOCK: enable group %d clockOn failed\n",
					CMDQ_GROUP_ISP);
			}
		}
	}

	for (index = CMDQ_MAX_GROUP_COUNT - 1; index >= 0; --index) {
		/* note that DISPSYS controls their own clock on/off */
		if (index == CMDQ_GROUP_DISP)
			continue;

		/* note that ISP is per-task on/off, not per HW flag */
		if (index == CMDQ_GROUP_ISP)
			continue;

		if (cmdq_core_is_group_flag((enum CMDQ_GROUP_ENUM) index,
			engines)) {
			CMDQ_MSG("CLOCK: enable group %d clockOn\n", index);
			if (!pCallback[index].clockOn) {
				CMDQ_LOG(
					"[WARNING]CLOCK: enable group %d clockOn func NULL\n",
					index);
				continue;
			}
			status = pCallback[index].clockOn(
				cmdq_mdp_get_func()->getEngineGroupBits(
				index) & engines);
			if (status < 0) {
				/* Error status print */
				CMDQ_ERR(
					"CLOCK: enable group %d clockOn failed\n",
					index);
			}
		}
	}

	CMDQ_MSG("<--CLOCK: Enable hardware clock end\n");
}

static s32 cmdq_core_can_start_to_acquire_HW_thread_unlocked(
	const u64 engineFlag, const bool is_secure)
{
	struct TaskStruct *pFirstWaitingTask = NULL;
	struct TaskStruct *pTempTask = NULL;
	struct list_head *p = NULL;
	bool preferSecurePath;
	s32 status = 0;
	char long_msg[CMDQ_LONGSTRING_MAX];
	u32 msg_offset;
	s32 msg_max_size;

	/* find first waiting task with OVERLAPPED engine flag with pTask */
	list_for_each(p, &gCmdqContext.taskWaitList) {
		pTempTask = list_entry(p, struct TaskStruct, listEntry);
		if (pTempTask && (engineFlag & pTempTask->engineFlag)) {
			pFirstWaitingTask = pTempTask;
			break;
		}
	}

	do {
		if (!pFirstWaitingTask) {
			/* no waiting task with overlape engine,
			 * go to dispath thread'
			 */
			break;
		}

		preferSecurePath = pFirstWaitingTask->secData.is_secure;

		if (preferSecurePath == is_secure) {
			/* same security path as first waiting task,
			 * go to start to thread dispatch
			 */
			cmdq_long_string_init(false, long_msg, &msg_offset,
				&msg_max_size);
			cmdq_long_string(long_msg, &msg_offset, &msg_max_size,
				"THREAD: is sec(%d, eng:0x%llx) as first waiting task",
				is_secure, engineFlag);
			cmdq_long_string(long_msg, &msg_offset, &msg_max_size,
				"(0x%p, eng:0x%llx), start thread dispatch.\n",
				pFirstWaitingTask,
				pFirstWaitingTask->engineFlag);
			CMDQ_MSG("%s", long_msg);
			break;
		}

		CMDQ_VERBOSE(
			"THREAD: is not the first waiting task(0x%p), yield.\n",
			pFirstWaitingTask);
		status = -EFAULT;
	} while (0);

	return status;
}

/*
 * check if engine conflict when thread dispatch
 * Parameter:
 *     engineFlag: [IN] engine flag
 *	   forceLog: [IN] print debug log
 *     is_secure: [IN] secure path
 *     *pThreadOut:
 *         [IN] prefer thread. please pass CMDQ_INVALID_THREAD if no prefere
 *         [OUT] dispatch thread result
 * Return:
 *     0 for success; else the error code is returned
 */

static bool cmdq_core_check_engine_conflict_unlocked(
	const u64 engineFlag, bool forceLog, const bool is_secure,
	s32 *pThreadOut)
{
	struct EngineStruct *pEngine;
	struct ThreadStruct *pThread;
	u32 free;
	s32 index;
	s32 thread;
	u64 engine;
	bool isEngineConflict;
	char long_msg[CMDQ_LONGSTRING_MAX];
	u32 msg_offset;
	s32 msg_max_size;

	pEngine = gCmdqContext.engine;
	pThread = gCmdqContext.thread;
	isEngineConflict = false;

	engine = engineFlag;
	thread = (*pThreadOut);
	free = (thread == CMDQ_INVALID_THREAD) ? 0xFFFFFFFF :
		0xFFFFFFFF & (~(0x1 << thread));

	/* check if engine conflict */
	for (index = 0; index < CMDQ_MAX_ENGINE_COUNT && engine; index++) {
		if (engine & (0x1LL << index)) {
			if (pEngine[index].currOwner == CMDQ_INVALID_THREAD) {
				continue;
			} else if (thread == CMDQ_INVALID_THREAD) {
				thread = pEngine[index].currOwner;
				free &= ~(0x1 << thread);
			} else if (thread != pEngine[index].currOwner) {
				/* Partial HW occupied by different threads,
				 * we need to wait.
				 */
				cmdq_long_string_init(forceLog, long_msg,
					&msg_offset, &msg_max_size);
				cmdq_long_string(long_msg, &msg_offset,
					&msg_max_size,
					"THREAD: try locate on thread %d but engine %d",
					thread, index);
				cmdq_long_string(long_msg, &msg_offset,
					&msg_max_size,
					" also occupied by thread %d, secure:%d\n",
					pEngine[index].currOwner, is_secure);
				if (forceLog)
					CMDQ_LOG("%s", long_msg);
				else
					CMDQ_VERBOSE("%s", long_msg);

				/* engine conflict! */
				isEngineConflict = true;
				thread = CMDQ_INVALID_THREAD;
				break;
			}

			engine &= ~(0x1LL << index);
		}
	}

	(*pThreadOut) = thread;
	return isEngineConflict;
}

static void cmdq_core_get_free_thread_impl(
	enum CMDQ_HW_THREAD_PRIORITY_ENUM thread_prio, s32 *thread)
{
	struct ThreadStruct *threads = gCmdqContext.thread;
	const u32 max_thread_count = cmdq_dev_get_thread_count();
	/* thread 0 - CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT
	 * are preserved for DISPSYS
	 */
	const bool disp_thread = thread_prio > CMDQ_THR_PRIO_DISPLAY_TRIGGER;
	int start_idx = disp_thread ? 0 : CMDQ_DYNAMIC_THREAD_ID_START;
	int end_idx = disp_thread ? CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT :
		max_thread_count;
	u32 index;
	unsigned long flags;

	for (index = start_idx; index < end_idx; index++) {
		CMDQ_PROF_SPIN_LOCK(gCmdqExecLock, flags,
			find_free_hw_thread);

		if (threads[index].engineFlag || threads[index].taskCount ||
			threads[index].allowDispatching != 1) {
			CMDQ_PROF_SPIN_UNLOCK(gCmdqExecLock, flags,
				find_free_hw_thread);
			continue;
		}

		CMDQ_VERBOSE(
			"THREAD: dispatch thread:%d taskCount:%d allowDispatching:%d\n",
			index, threads[index].taskCount,
			threads[index].allowDispatching);

		*thread = index;
		threads[index].allowDispatching = 0;
		CMDQ_PROF_SPIN_UNLOCK(gCmdqExecLock, flags,
			find_free_hw_thread_stop);
		break;
	}
}

static s32 cmdq_core_find_a_free_HW_thread(u64 engineFlag,
	enum CMDQ_HW_THREAD_PRIORITY_ENUM thread_prio,
	enum CMDQ_SCENARIO_ENUM scenario, bool forceLog, const bool is_secure)
{
	struct ThreadStruct *pThread = gCmdqContext.thread;
	s32 thread;
	bool isEngineConflict;
	s32 insertCookie;

	do {
		CMDQ_VERBOSE(
			"THREAD: find a free thread engine:0x%llx scenario:%d secure:%d\n",
			engineFlag, scenario, is_secure);

		/* start to dispatch?
		 * note we should not favor secure or normal path,
		 * traverse waiting list to decide that
		 * we should dispatch thread to secure or normal path
		 */
		if (cmdq_core_can_start_to_acquire_HW_thread_unlocked(
			engineFlag, is_secure) < 0) {
			thread = CMDQ_INVALID_THREAD;
			break;
		}

		/* it's okey to dispatch thread,
		 * use scenario and pTask->secure to get default thread
		 */
		thread = cmdq_get_func()->getThreadID(scenario, is_secure);

		/* check if engine conflict happened except DISP scenario */
		isEngineConflict = false;
		if (cmdq_get_func()->isDispScenario(scenario) == false) {
			isEngineConflict =
				cmdq_core_check_engine_conflict_unlocked(
				engineFlag, forceLog, is_secure, &thread);
		}
		CMDQ_VERBOSE("THREAD: isEngineConflict:%d thread:%d\n",
			isEngineConflict, thread);

#if 1				/* TODO: secure path proting */
		/* because all thread are pre-dispatched,
		 * there 2 outcome of engine conflict check:
		 * 1. pre-dispatched secure thread,
		 *    and no conflict with normal path
		 * 2. pre-dispatched secure thread,
		 *    but conflict with normal/anothor secure path
		 *
		 * no need to check get normal thread in secure path
		 */

		/* ensure not dispatch secure thread to normal task */
		if (!is_secure && (cmdq_get_func()->isSecureThread(thread))) {
			thread = CMDQ_INVALID_THREAD;
			isEngineConflict = true;
			break;
		}
#endif
		/* no enfine conflict with running thread,
		 * AND used engines have no owner
		 * try to find a free thread
		 */
		if (!isEngineConflict && thread == CMDQ_INVALID_THREAD)
			cmdq_core_get_free_thread_impl(thread_prio, &thread);

		/* no thread available now, wait for it */
		if (thread == CMDQ_INVALID_THREAD)
			break;

		/* Make sure the found thread has enough space for the task; */
		/* ThreadStruct->pCurTask has size limitation. */
		if (cmdq_core_max_task_in_thread(thread) <=
			pThread[thread].taskCount) {
			if (forceLog) {
				CMDQ_LOG(
					"THREAD: thread %d task count:%d full\n",
					 thread, pThread[thread].taskCount);
			} else {
				CMDQ_VERBOSE(
					"THREAD: thread %d task count:%d full\n",
					thread, pThread[thread].taskCount);
			}

			thread = CMDQ_INVALID_THREAD;
		} else {
			insertCookie = pThread[thread].nextCookie %
				cmdq_core_max_task_in_thread(thread);
			if (pThread[thread].pCurTask[insertCookie]) {
				if (forceLog) {
					CMDQ_LOG(
						"THREAD: thread %d nextCookie:%d already has task\n",
						thread,
						pThread[thread].nextCookie);
				} else {
					CMDQ_VERBOSE(
						"THREAD: thread %d nextCookie:%d already has task\n",
						thread,
						pThread[thread].nextCookie);
				}

				thread = CMDQ_INVALID_THREAD;
			}
		}
	} while (0);

	return thread;
}

static s32 cmdq_core_acquire_thread(struct TaskStruct *task,
	enum CMDQ_HW_THREAD_PRIORITY_ENUM thread_prio, bool forceLog)
{
	unsigned long flags;
	s32 thread;
	u64 engineMustEnableClock;

	CMDQ_PROF_START(current->pid, __func__);

	CMDQ_PROF_MUTEX_LOCK(gCmdqClockMutex, acquire_thread_clock);
	CMDQ_PROF_SPIN_LOCK(gCmdqThreadLock, flags, acquire_thread);

	thread = cmdq_core_find_a_free_HW_thread(task->engineFlag,
		thread_prio, task->scenario, forceLog,
		task->secData.is_secure);

	if (thread != CMDQ_INVALID_THREAD) {
		/* get actual engine flag.
		 * Each bit represents a engine must enable clock.
		 */
		engineMustEnableClock =
			cmdq_core_get_actual_engine_flag_for_enable_clock(
			task->engineFlag, thread);
	}

	if (thread == CMDQ_INVALID_THREAD &&
		task->secData.is_secure == true &&
		task->scenario == CMDQ_SCENARIO_USER_MDP)
		g_cmdq_consume_again = true;

	CMDQ_PROF_SPIN_UNLOCK(gCmdqThreadLock, flags, acquire_thread);

	if (thread != CMDQ_INVALID_THREAD) {
		/* enable clock */
		cmdq_core_enable_clock(task->engineFlag, thread,
			engineMustEnableClock, task->scenario);
		/* Start delay thread after first task is coming */
		cmdq_delay_thread_start();
	}

	if (task->res_engine_flag_acquire)
		cmdq_core_enable_resource_clk_unlock(
			task->res_engine_flag_acquire, true);

	CMDQ_PROF_MUTEX_UNLOCK(gCmdqClockMutex, acquire_thread_clock);

	CMDQ_PROF_END(current->pid, __func__);

	return thread;
}

static u64 cmdq_core_get_not_used_engine_flag_for_disable_clock(
	const u64 engineFlag)
{
	struct EngineStruct *pEngine;
	struct ThreadStruct *pThread;
	u64 enginesNotUsed;
	s32 index;
	s32 currOwnerThread = CMDQ_INVALID_THREAD;

	enginesNotUsed = 0LL;
	pEngine = gCmdqContext.engine;
	pThread = gCmdqContext.thread;

	for (index = 0; index < CMDQ_MAX_ENGINE_COUNT; index++) {
		if (engineFlag & (1LL << index)) {
			pEngine[index].userCount--;
			if (pEngine[index].userCount <= 0) {
				enginesNotUsed |= (1LL << index);
				currOwnerThread = pEngine[index].currOwner;
				/* remove engine flag in assigned pThread */
				pThread[currOwnerThread].engineFlag &=
					~(1LL << index);
				pEngine[index].currOwner = CMDQ_INVALID_THREAD;
			}
		}
	}
	CMDQ_VERBOSE("%s enginesNotUsed:0x%llx\n", __func__, enginesNotUsed);
	return enginesNotUsed;
}

static void cmdq_core_disable_clock(u64 engineFlag,
	const u64 enginesNotUsed, enum CMDQ_SCENARIO_ENUM scenario)
{
	s32 index;
	s32 status;
	struct CmdqCBkStruct *pCallback;

	CMDQ_VERBOSE(
		"-->CLOCK: Disable hardware clock 0x%llx begin, enginesNotUsed 0x%llx\n",
		engineFlag, enginesNotUsed);

	pCallback = gCmdqGroupCallback;

	/* ISP special check: Always call ISP on/off if this task
	 * involves ISP. Ignore the ISP HW flags ref count.
	 */
	if (cmdq_core_is_group_flag(CMDQ_GROUP_ISP, engineFlag)) {
		CMDQ_VERBOSE("CLOCK: disable group %d clockOff\n",
			CMDQ_GROUP_ISP);
		if (!pCallback[CMDQ_GROUP_ISP].clockOff) {
			CMDQ_ERR(
				"CLOCK: disable group %d clockOff func NULL\n",
				CMDQ_GROUP_ISP);
		} else {
			status = pCallback[CMDQ_GROUP_ISP].clockOff(
				cmdq_mdp_get_func()->getEngineGroupBits(
					CMDQ_GROUP_ISP) & engineFlag);

#if 1
			--gCmdqISPClockCounter;
			if (gCmdqISPClockCounter != 0) {
				/* ISP clock off */
				CMDQ_VERBOSE("CLOCK: ISP clockOff cnt:%d\n",
					gCmdqISPClockCounter);
			}
#endif

			if (status < 0) {
				CMDQ_ERR(
					"CLOCK: disable group %d clockOff failed\n",
					CMDQ_GROUP_ISP);
			}
		}
	}

	/* Turn off unused engines */
	for (index = 0; index < CMDQ_MAX_GROUP_COUNT; ++index) {
		/* note that DISPSYS controls their own clock on/off */
		if (index == CMDQ_GROUP_DISP)
			continue;

		/* note that ISP is per-task on/off, not per HW flag */
		if (index == CMDQ_GROUP_ISP)
			continue;

		if (cmdq_core_is_group_flag((enum CMDQ_GROUP_ENUM) index,
			enginesNotUsed)) {
			CMDQ_MSG(
				"CLOCK: Disable engine group %d flag:0x%llx clockOff\n",
				index, enginesNotUsed);
			if (!pCallback[index].clockOff) {
				CMDQ_LOG(
					"[WARNING]CLOCK: Disable engine group %d clockOff func NULL\n",
					index);
				continue;
			}
			status = pCallback[index].clockOff(
				cmdq_mdp_get_func()->getEngineGroupBits(
					index) & enginesNotUsed);
			if (status < 0) {
				/* Error status print */
				CMDQ_ERR(
					"CLOCK: Disable engine group %d clock failed\n",
					index);
			}
		}
	}

	/* disable fundamental clocks if needed */
	cmdq_core_enable_common_clock_locked(false, engineFlag, scenario);

	CMDQ_MSG("<--CLOCK: Disable hardware clock 0x%llx end\n", engineFlag);
}

void cmdq_core_add_consume_task(void)
{
	if (!work_pending(&gCmdqContext.taskConsumeWaitQueueItem)) {
		CMDQ_PROF_MMP(cmdq_mmp_get_event()->consume_add,
			MMPROFILE_FLAG_PULSE, 0, 0);
		queue_work(gCmdqContext.taskConsumeWQ,
			&gCmdqContext.taskConsumeWaitQueueItem);
	}
}

static void cmdq_core_release_thread(struct TaskStruct *pTask)
{
	unsigned long flags;
	const s32 thread = pTask->thread;
	const u64 engineFlag = pTask->engineFlag;
	u64 engineNotUsed = 0LL;

	if (thread == CMDQ_INVALID_THREAD)
		return;

	CMDQ_PROF_MUTEX_LOCK(gCmdqClockMutex, release_thread_clock);
	CMDQ_PROF_SPIN_LOCK(gCmdqThreadLock, flags, release_thread);

	/* get not used engines for disable clock */
	engineNotUsed =
		cmdq_core_get_not_used_engine_flag_for_disable_clock(
			engineFlag);
	pTask->thread = CMDQ_INVALID_THREAD;

	CMDQ_PROF_SPIN_UNLOCK(gCmdqThreadLock, flags, release_thread);

	/* Stop delay thread after last task is done */
	cmdq_delay_thread_stop();
	/* clock off */
	cmdq_core_disable_clock(engineFlag, engineNotUsed, pTask->scenario);
	/* Delay release resource  */
	cmdq_core_delay_check_unlock(engineFlag, engineNotUsed);

	if (pTask->res_engine_flag_release)
		cmdq_core_enable_resource_clk_unlock(
			pTask->res_engine_flag_release, false);

	CMDQ_PROF_MUTEX_UNLOCK(gCmdqClockMutex, release_thread_clock);
}

static void cmdq_core_reset_hw_engine(s32 engineFlag)
{
	struct EngineStruct *pEngine;
	u32 engines;
	s32 index;
	s32 status;
	struct CmdqCBkStruct *pCallback;

	CMDQ_MSG("Reset hardware engine begin\n");

	pEngine = gCmdqContext.engine;

	engines = 0;
	for (index = 0; index < CMDQ_MAX_ENGINE_COUNT; index++) {
		if (engineFlag & (1LL << index))
			engines |= (1LL << index);
	}

	pCallback = gCmdqGroupCallback;

	for (index = 0; index < CMDQ_MAX_GROUP_COUNT; ++index) {
		if (cmdq_core_is_group_flag((enum CMDQ_GROUP_ENUM) index,
			engines)) {
			CMDQ_MSG("Reset engine group %d clock\n", index);
			if (!pCallback[index].resetEng) {
				CMDQ_ERR(
					"Reset engine group %d clock func NULL\n",
					index);
				continue;
			}
			status = pCallback[index].resetEng(
				cmdq_mdp_get_func()->getEngineGroupBits(
					index) & engineFlag);
			if (status < 0) {
				/* Error status print */
				CMDQ_ERR("Reset engine group %d clock failed\n",
					index);
			}
		}
	}

	CMDQ_MSG("Reset hardware engine end\n");
}

void cmdq_core_set_addon_subsys(u32 msb, s32 subsys_id, u32 mask)
{
	gAddOnSubsys.msb = msb;
	gAddOnSubsys.subsysID = subsys_id;
	gAddOnSubsys.mask = mask;
	CMDQ_LOG("Set AddOn Subsys:msb:0x%08x mask:0x%08x id:%d\n",
		msb, mask, subsys_id);
}

u32 cmdq_core_subsys_to_reg_addr(u32 arg_a)
{
	const u32 subsysBit = cmdq_get_func()->getSubsysLSBArgA();
	const s32 subsys_id = (arg_a & CMDQ_ARG_A_SUBSYS_MASK) >> subsysBit;
	u32 offset = 0;
	u32 base_addr = 0;
	u32 i;

	for (i = 0; i < CMDQ_SUBSYS_MAX_COUNT; i++) {
		if (gCmdqDtsData.subsys[i].subsysID == subsys_id) {
			base_addr = gCmdqDtsData.subsys[i].msb;
			offset = arg_a & ~gCmdqDtsData.subsys[i].mask;
			break;
		}
	}

	if (!base_addr && gAddOnSubsys.subsysID > 0 &&
		subsys_id == gAddOnSubsys.subsysID) {
		base_addr = gAddOnSubsys.msb;
		offset = arg_a & ~gAddOnSubsys.mask;
	}

	return base_addr | offset;
}

const char *cmdq_core_parse_subsys_from_reg_addr(u32 reg_addr)
{
	u32 addr_base_shifted;
	const char *module = "CMDQ";
	u32 i;

	for (i = 0; i < CMDQ_SUBSYS_MAX_COUNT; i++) {
		if (gCmdqDtsData.subsys[i].subsysID == -1)
			continue;

		addr_base_shifted = reg_addr & gCmdqDtsData.subsys[i].mask;
		if (gCmdqDtsData.subsys[i].msb == addr_base_shifted) {
			module = gCmdqDtsData.subsys[i].grpName;
			break;
		}
	}

	return module;
}

s32 cmdq_core_subsys_from_phys_addr(u32 physAddr)
{
	s32 msb;
	s32 subsysID = -1;
	u32 i;

	for (i = 0; i < CMDQ_SUBSYS_MAX_COUNT; i++) {
		if (gCmdqDtsData.subsys[i].subsysID == -1)
			continue;

		msb = physAddr & gCmdqDtsData.subsys[i].mask;
		if (msb == gCmdqDtsData.subsys[i].msb) {
			subsysID = gCmdqDtsData.subsys[i].subsysID;
			break;
		}
	}

	if (subsysID == -1 && gAddOnSubsys.subsysID > 0) {
		msb = physAddr & gAddOnSubsys.mask;
		if (msb == gAddOnSubsys.msb)
			subsysID = gAddOnSubsys.subsysID;
	}

	if (subsysID == -1) {
		/* if not supported physAddr is GCE base address,
		 * then tread as special address
		 */
		msb = physAddr & GCE_BASE_PA;
		if (msb == GCE_BASE_PA)
			subsysID = CMDQ_SPECIAL_SUBSYS_ADDR;
		else
			CMDQ_ERR("unrecognized subsys, physAddr:0x%08x\n",
				physAddr);
	}
	return subsysID;
}

static const char *cmdq_core_parse_op(u32 op_code)
{
	switch (op_code) {
	case CMDQ_CODE_POLL:
		return "POLL";
	case CMDQ_CODE_WRITE:
		return "WRIT";
	case CMDQ_CODE_WFE:
		return "SYNC";
	case CMDQ_CODE_READ:
		return "READ";
	case CMDQ_CODE_MOVE:
		return "MASK";
	case CMDQ_CODE_JUMP:
		return "JUMP";
	case CMDQ_CODE_EOC:
		return "MARK";
	case CMDQ_CODE_READ_S:
		return "READ_S";
	case CMDQ_CODE_WRITE_S:
		return "WRITE_S";
	case CMDQ_CODE_WRITE_S_W_MASK:
		return "WRITE_S with mask";
	case CMDQ_CODE_LOGIC:
		return "LOGIC";
	case CMDQ_CODE_JUMP_C_RELATIVE:
		return "JUMP_C related";
	case CMDQ_CODE_JUMP_C_ABSOLUTE:
		return "JUMP_C absolute";
	}
	return NULL;
}

static const char *cmdq_core_parse_logic_sop(u32 s_op)
{
	switch (s_op) {
	case CMDQ_LOGIC_ASSIGN:
		return "=";
	case CMDQ_LOGIC_ADD:
		return "+";
	case CMDQ_LOGIC_SUBTRACT:
		return "-";
	case CMDQ_LOGIC_MULTIPLY:
		return "*";
	case CMDQ_LOGIC_XOR:
		return "^";
	case CMDQ_LOGIC_NOT:
		return "~";
	case CMDQ_LOGIC_OR:
		return "|";
	case CMDQ_LOGIC_AND:
		return "&";
	case CMDQ_LOGIC_LEFT_SHIFT:
		return "<<";
	case CMDQ_LOGIC_RIGHT_SHIFT:
		return ">>";
	}
	return NULL;
}

static const char *cmdq_core_parse_jump_c_sop(u32 s_op)
{
	switch (s_op) {
	case CMDQ_EQUAL:
		return "==";
	case CMDQ_NOT_EQUAL:
		return "!=";
	case CMDQ_GREATER_THAN_AND_EQUAL:
		return ">=";
	case CMDQ_LESS_THAN_AND_EQUAL:
		return "<=";
	case CMDQ_GREATER_THAN:
		return ">";
	case CMDQ_LESS_THAN:
		return "<";
	}
	return NULL;
}

static void cmdq_core_parse_error(const struct TaskStruct *pTask, u32 thread,
	const char **moduleName, s32 *flag, u32 *instA, u32 *instB)
{
	u32 op, arg_a, arg_b;
	s32 eventENUM;
	u32 insts[4] = { 0 };
	u32 addr = 0;
	const char *module = NULL;
	int isSMIHang = 0;

	if (unlikely(!pTask)) {
		CMDQ_ERR("No task to parse error\n");
		return;
	}

	do {
		/* confirm if SMI is hang */
		isSMIHang = cmdq_get_func()->dumpSMI(0);
		if (isSMIHang) {
			module = "SMI";
			break;
		}

		/* other cases, use instruction to judge */
		/* because scenario / HW flag are not sufficient */
		/* e.g. ISP pass 2 involves both MDP and ISP */
		/* so we need to check which instruction timeout-ed. */
		if (cmdq_core_get_pc(pTask, thread, insts, NULL)) {
			op = (insts[3] & 0xFF000000) >> 24;
			arg_a = insts[3] & (~0xFF000000);
			arg_b = insts[2];

			/* quick exam by hwflag first */
			module = cmdq_get_func()->parseErrorModule(pTask);
			if (module)
				break;

			switch (op) {
			case CMDQ_CODE_POLL:
			case CMDQ_CODE_WRITE:
				addr = cmdq_core_subsys_to_reg_addr(arg_a);
				module = cmdq_get_func()->parseModule(addr);
				break;
			case CMDQ_CODE_WFE:
				/* arg_a is the event ID */
				eventENUM = cmdq_core_reverse_event_ENUM(
					arg_a);
				module = cmdq_get_func()->moduleFromEvent(
					eventENUM, gCmdqGroupCallback,
					pTask->engineFlag);
				break;
			case CMDQ_CODE_READ:
			case CMDQ_CODE_MOVE:
			case CMDQ_CODE_JUMP:
			case CMDQ_CODE_EOC:
			default:
				module = "CMDQ";
				break;
			}
			break;
		}

		module = "CMDQ";
		break;
	} while (0);


	/* fill output parameter */
	*moduleName = module;
	*flag = pTask->irqFlag;
	*instA = insts[3];
	*instB = insts[2];

}

void cmdq_core_dump_resource_status(enum CMDQ_EVENT_ENUM resourceEvent)
{
	struct ResourceUnitStruct *pResource = NULL;

	if (!cmdq_core_is_feature_on(CMDQ_FEATURE_SRAM_SHARE))
		return;

	list_for_each_entry(pResource, &gCmdqContext.resourceList,
		list_entry) {
		if (resourceEvent == pResource->lockEvent) {
			CMDQ_ERR("[Res] Dump resource with event:%d\n",
				resourceEvent);
			mutex_lock(&gCmdqResourceMutex);
			/* find matched resource */
			CMDQ_ERR("[Res] Dump resource latest time:\n");
			CMDQ_ERR("[Res]   notify:%llu delay:%lld\n",
				pResource->notify, pResource->delay);
			CMDQ_ERR("[Res]   lock:%llu unlock:%lld\n",
				pResource->lock, pResource->unlock);
			CMDQ_ERR("[Res]   acquire:%llu release:%lld\n",
				pResource->acquire, pResource->release);
			CMDQ_ERR("[Res] isUsed:%d isLend:%d isDelay:%d\n",
				pResource->used, pResource->lend,
				pResource->delaying);
			if (!pResource->releaseCB)
				CMDQ_ERR("[Res] release CB func is NULL\n");
			mutex_unlock(&gCmdqResourceMutex);
			break;
		}
	}
}

static u32 *cmdq_core_dump_pc(const struct TaskStruct *pTask,
	s32 thread, const char *tag)
{
	u32 *pcVA = NULL, pcPA = 0;
	u32 insts[4] = { 0 };
	char parsedInstruction[128] = { 0 };

	pcVA = cmdq_core_get_pc(pTask, thread, insts, &pcPA);
	if (pcVA) {
		const u32 op = (insts[3] & 0xFF000000) >> 24;

		cmdq_core_parse_instruction(pcVA, parsedInstruction,
			sizeof(parsedInstruction));

		/* for WFE, we specifically dump the event value */
		if (op == CMDQ_CODE_WFE) {
			u32 regValue = 0;
			const u32 eventID = 0x3FF & insts[3];

			CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_ID, eventID);
			regValue = CMDQ_REG_GET32(CMDQ_SYNC_TOKEN_VAL);
			CMDQ_LOG(
				"[%s]Thread %d PC:0x%p(0x%08x) 0x%08x:0x%08x => %s value:%d",
				tag, thread, pcVA, pcPA, insts[2], insts[3],
				parsedInstruction, regValue);
		} else {
			CMDQ_LOG(
				"[%s]Thread %d PC:0x%p(0x%08x), 0x%08x:0x%08x => %s",
				tag, thread, pcVA, pcPA, insts[2], insts[3],
				parsedInstruction);
		}
	} else {
		if (pTask->secData.is_secure) {
			CMDQ_LOG(
				"[%s]Thread %d PC: HIDDEN INFO since is it's secure thread\n",
				 tag, thread);
		} else {
			CMDQ_LOG("[%s]Thread %d PC: Not available\n",
				tag, thread);
		}
	}

	return pcVA;
}

static void cmdq_core_dump_status(const char *tag)
{
	s32 coreExecThread = CMDQ_INVALID_THREAD;
	u32 value[6] = { 0 };

	value[0] = CMDQ_REG_GET32(CMDQ_CURR_LOADED_THR);
	value[1] = CMDQ_REG_GET32(CMDQ_THR_EXEC_CYCLES);
	value[2] = CMDQ_REG_GET32(CMDQ_THR_TIMEOUT_TIMER);
	value[3] = CMDQ_REG_GET32(CMDQ_BUS_CONTROL_TYPE);

	/* this returns (1 + index of least bit set) or 0 if input is 0. */
	coreExecThread = __builtin_ffs(value[0]) - 1;
	CMDQ_LOG(
		"[%s]IRQ:0x%08x Execing:%d Thread:%d CURR_LOADED_THR:0x%08x THR_EXEC_CYCLES:0x%08x\n",
		tag, CMDQ_REG_GET32(CMDQ_CURR_IRQ_STATUS),
		(0x80000000 & value[0]) ? 1 : 0, coreExecThread, value[0],
		value[1]);
	CMDQ_LOG(
		"[%s]THR_TIMER:0x%x BUS_CTRL:0x%x DEBUG:0x%x 0x%x 0x%x 0x%x\n",
		tag, value[2], value[3],
		CMDQ_REG_GET32((GCE_BASE_VA + 0xF0)),
		CMDQ_REG_GET32((GCE_BASE_VA + 0xF4)),
		CMDQ_REG_GET32((GCE_BASE_VA + 0xF8)),
		CMDQ_REG_GET32((GCE_BASE_VA + 0xFC)));
}

void cmdq_core_dump_disp_trigger_loop(const char *tag)
{
	/* we assume the first non-high-priority thread is trigger loop thread
	 * since it will start very early
	 */
	if (gCmdqContext.thread[CMDQ_DYNAMIC_THREAD_ID_START].taskCount &&
		gCmdqContext.thread[
			CMDQ_DYNAMIC_THREAD_ID_START].pCurTask[1] &&
		gCmdqContext.thread[
			CMDQ_DYNAMIC_THREAD_ID_START].loopCallback) {

		u32 regValue = 0;
		struct TaskStruct *pTask = gCmdqContext.thread[
			CMDQ_DYNAMIC_THREAD_ID_START].pCurTask[1];

		cmdq_core_dump_pc(pTask, CMDQ_DYNAMIC_THREAD_ID_START, tag);

		regValue = cmdqCoreGetEvent(CMDQ_EVENT_DISP_RDMA0_EOF);
		CMDQ_LOG("[%s]CMDQ_SYNC_TOKEN_VAL of %s is %d\n",
			tag, cmdq_core_get_event_name_ENUM(
			CMDQ_EVENT_DISP_RDMA0_EOF), regValue);
	}
}

void cmdq_core_dump_disp_trigger_loop_mini(const char *tag)
{
	/* we assume the first non-high-priority thread is trigger loop thread
	 * since it will start very early
	 */
	if (gCmdqContext.thread[CMDQ_DYNAMIC_THREAD_ID_START].taskCount &&
		gCmdqContext.thread[
		CMDQ_DYNAMIC_THREAD_ID_START].pCurTask[1] &&
		gCmdqContext.thread[
		CMDQ_DYNAMIC_THREAD_ID_START].loopCallback) {

		struct TaskStruct *pTask = gCmdqContext.thread[
			CMDQ_DYNAMIC_THREAD_ID_START].pCurTask[1];

		cmdq_core_dump_pc(pTask, CMDQ_DYNAMIC_THREAD_ID_START, tag);
	}
}

static void cmdq_core_dump_thread_pc(const s32 thread)
{
	s32 i = 0;
	struct ThreadStruct *pThread;
	struct TaskStruct *pTask = NULL;
	u32 *pcVA = NULL, pcPA = 0;
	u32 insts[4] = { 0 };
	char parsedInstruction[128] = { 0 };

	if (thread == CMDQ_INVALID_THREAD)
		return;

	if (thread == CMDQ_DELAY_THREAD_ID) {
		pcPA = CMDQ_AREG_TO_PHYS(CMDQ_REG_GET32(
			CMDQ_THR_CURR_ADDR(thread)));

		CMDQ_LOG(
			"==Delay Thread Task size:%u started:%d pc:0x%x pa:%pa va:0x%p sram:0x%x\n",
			g_delay_thread_cmd.buffer_size,
			g_delay_thread_started, pcPA,
			&g_delay_thread_cmd.mva_base,
			g_delay_thread_cmd.p_va_base,
			g_delay_thread_cmd.sram_base);

		if (pcPA == 0)
			return;

		if (g_delay_thread_cmd.sram_base > 0)
			pcVA = (u32 *)((u8 *) g_delay_thread_cmd.p_va_base +
				(pcPA - g_delay_thread_cmd.sram_base));
		else
			pcVA = (u32 *)((u8 *) g_delay_thread_cmd.p_va_base +
				(pcPA - g_delay_thread_cmd.mva_base));

		if (pcVA) {
			insts[2] = CMDQ_REG_GET32(pcVA + 0);
			insts[3] = CMDQ_REG_GET32(pcVA + 1);
		}
	} else {
		pThread = &(gCmdqContext.thread[thread]);

		for (i = 0; i < cmdq_core_max_task_in_thread(thread); i++) {
			pTask = pThread->pCurTask[i];

			if (!pTask)
				continue;

			pcVA = cmdq_core_get_pc(pTask, thread, insts, &pcPA);
			if (pcVA)
				break;
		}
	}

	if (pcVA) {
		const u32 op = (insts[3] & 0xFF000000) >> 24;

		cmdq_core_parse_instruction(pcVA, parsedInstruction,
						sizeof(parsedInstruction));

		/* for wait event case, dump token value */
		/* for WFE, we specifically dump the event value */
		if (op == CMDQ_CODE_WFE) {
			u32 regValue = 0;
			const u32 eventID = 0x3FF & insts[3];

			CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_ID, eventID);
			regValue = CMDQ_REG_GET32(CMDQ_SYNC_TOKEN_VAL);
			CMDQ_LOG(
				"[INFO]task:%p(ID:%d) thread:%d PC:0x%p(0x%08x) 0x%08x:0x%08x => %s value:%d\n",
				pTask, i, thread, pcVA, pcPA,
				insts[2], insts[3], parsedInstruction,
				regValue);
		} else {
			CMDQ_LOG(
				"[INFO]task:%p(ID:%d) thread:%d PC:0x%p(0x%08x) 0x%08x:0x%08x => %s\n",
				pTask, i, thread, pcVA, pcPA,
				insts[2], insts[3], parsedInstruction);
		}
	}
}

static void cmdq_core_dump_task_in_thread(const s32 thread,
	const bool fullTatskDump, const bool dumpCookie,
	const bool dumpCmd)
{
	struct ThreadStruct *pThread;
	struct TaskStruct *pDumpTask;
	s32 index;
	u32 value[4] = { 0 };
	u32 cookie;
	u32 *pVABase = NULL;
	dma_addr_t MVABase = 0;

	if (thread == CMDQ_INVALID_THREAD)
		return;

	pThread = &(gCmdqContext.thread[thread]);
	pDumpTask = NULL;

	CMDQ_ERR(
		"=============== [CMDQ] All Task in Error Thread %d ===============\n",
		thread);
	cookie = cmdq_core_thread_exec_counter(thread);
	if (dumpCookie) {
		CMDQ_ERR(
			"Curr Cookie:%d Wait Cookie:%d Next Cookie:%d Task Count:%d engineFlag:0x%llx\n",
			cookie, pThread->waitCookie, pThread->nextCookie,
			pThread->taskCount, pThread->engineFlag);
	}

	for (index = 0; index < cmdq_core_max_task_in_thread(thread);
		index++) {
		pDumpTask = pThread->pCurTask[index];
		if (!pDumpTask)
			continue;

		/* full task dump */
		if (fullTatskDump) {
			CMDQ_ERR("Slot %d, Task:0x%p\n", index, pDumpTask);
			cmdq_core_dump_task(pDumpTask);

			if (dumpCmd)
				cmdq_core_dump_task_buffer_hex(pDumpTask);

			continue;
		}

		/* otherwise, simple dump task info */
		if (list_empty(&pDumpTask->cmd_buffer_list)) {
			value[0] = 0xBCBCBCBC;
			value[1] = 0xBCBCBCBC;
			value[2] = 0xBCBCBCBC;
			value[3] = 0xBCBCBCBC;
		} else {
			value[0] = pDumpTask->pCMDEnd[-3];
			value[1] = pDumpTask->pCMDEnd[-2];
			value[2] = pDumpTask->pCMDEnd[-1];
			value[3] = pDumpTask->pCMDEnd[0];
		}

		cmdq_core_get_task_first_buffer(pDumpTask, &pVABase, &MVABase);
		CMDQ_ERR("Slot:%d Task:0x%p VABase:0x%p MVABase:%pa Size:%d\n",
			index, pDumpTask, pVABase,
			&MVABase, pDumpTask->commandSize);
		CMDQ_ERR(
			"	cont'd: Last Inst 0x%08x:0x%08x 0x%08x:0x%08x priority:%d\n",
			value[0], value[1], value[2], value[3],
			pDumpTask->priority);
		if (pDumpTask->sram_base > 0)
			CMDQ_ERR("** This is SRAM task: sram base:%u",
				pDumpTask->sram_base);

		if (dumpCmd) {
			print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS,
				16, 4, pVABase, pDumpTask->commandSize, true);
		}
	}
}

static void cmdq_core_dump_task_with_engine_flag(
	u64 engineFlag, s32 current_thread)
{
	const u32 max_thread_count = cmdq_dev_get_thread_count();
	s32 thread_idx = 0;

	if (current_thread == CMDQ_INVALID_THREAD)
		return;

	CMDQ_ERR(
		"=============== [CMDQ] All task in thread sharing same engine flag 0x%016llx===============\n",
		engineFlag);

	for (thread_idx = 0; thread_idx < max_thread_count; thread_idx++) {
		struct ThreadStruct *thread = &gCmdqContext.thread[thread_idx];

		if (thread_idx == current_thread || thread->taskCount <= 0 ||
			!(thread->engineFlag & engineFlag))
			continue;
		cmdq_core_dump_task_in_thread(thread_idx, false, false, false);
	}
}

void cmdq_core_dump_secure_metadata(struct cmdqSecDataStruct *pSecData)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	u32 i = 0;
	struct cmdqSecAddrMetadataStruct *pAddr = NULL;

	if (!pSecData)
		return;

	pAddr = (struct cmdqSecAddrMetadataStruct *)
		(CMDQ_U32_PTR(pSecData->addrMetadatas));

	CMDQ_LOG("========= pSecData:%p dump =========\n", pSecData);
	CMDQ_LOG(
		"count:%d(%d) enginesNeedDAPC:0x%llx enginesPortSecurity:0x%llx\n",
		pSecData->addrMetadataCount, pSecData->addrMetadataMaxCount,
		pSecData->enginesNeedDAPC, pSecData->enginesNeedPortSecurity);

	if (!pAddr)
		return;

	for (i = 0; i < pSecData->addrMetadataCount; i++) {
		CMDQ_LOG(
			"idx:%d type:%d baseHandle:0x%016llx blockOffset:%u offset:%u size:%u port:%u\n",
			i, pAddr[i].type, (u64)pAddr[i].baseHandle,
			pAddr[i].blockOffset, pAddr[i].offset,
			pAddr[i].size, pAddr[i].port);
	}
#endif
}

static u32 cmdq_core_interpret_wpr_value_data_register(
	const u32 op, u32 arg_value)
{
	u32 reg_id = arg_value;

	if (op == CMDQ_CODE_WRITE_S || op == CMDQ_CODE_WRITE_S_W_MASK)
		reg_id = ((arg_value >> 16) & 0xFFFF);
	else if (op == CMDQ_CODE_READ_S)
		reg_id = (arg_value & 0xFFFF);
	else
		reg_id += CMDQ_GPR_V3_OFFSET;
	return reg_id;
}

static u32 cmdq_core_interpret_wpr_address_data_register(
	const u32 op, u32 arg_addr)
{
	u32 reg_id = (arg_addr >> 16) & 0x1F;

	if (op == CMDQ_CODE_WRITE_S || op == CMDQ_CODE_WRITE_S_W_MASK ||
		op == CMDQ_CODE_READ_S)
		reg_id = (arg_addr & 0xFFFF);
	else
		reg_id += CMDQ_GPR_V3_OFFSET;
	return reg_id;
}

s32 cmdq_core_interpret_instruction(char *textBuf, int bufLen,
	const u32 op, const u32 arg_a, const u32 arg_b)
{
	int reqLen = 0;
	u32 arg_addr, arg_value;
	u32 arg_addr_type, arg_value_type;
	u32 reg_addr;
	u32 reg_id, use_mask;
	const u32 addr_mask = 0xFFFFFFFE;

	switch (op) {
	case CMDQ_CODE_MOVE:
		if (1 & (arg_a >> 23)) {
			reg_id = ((arg_a >> 16) & 0x1f) + CMDQ_GPR_V3_OFFSET;
			reqLen = snprintf(textBuf, bufLen,
				"MOVE:0x%08x to Reg%d\n", arg_b, reg_id);
		} else {
			reqLen = snprintf(textBuf, bufLen,
				"Set MASK:0x%08x\n", arg_b);
		}
		break;
	case CMDQ_CODE_READ:
	case CMDQ_CODE_WRITE:
	case CMDQ_CODE_POLL:
	case CMDQ_CODE_READ_S:
	case CMDQ_CODE_WRITE_S:
	case CMDQ_CODE_WRITE_S_W_MASK:
		reqLen = snprintf(textBuf, bufLen, "%s: ",
			cmdq_core_parse_op(op));
		bufLen -= reqLen;
		textBuf += reqLen;

		if (op == CMDQ_CODE_READ_S) {
			arg_addr = (arg_a & CMDQ_ARG_A_SUBSYS_MASK) |
				((arg_b >> 16) & 0xFFFF);
			arg_addr_type = arg_a & (1 << 22);
			arg_value = arg_a & 0xFFFF;
			arg_value_type = arg_a & (1 << 23);
		} else {
			arg_addr = arg_a;
			arg_addr_type = arg_a & (1 << 23);
			arg_value = arg_b;
			arg_value_type = arg_a & (1 << 22);
		}

		/* data (value) */
		if (arg_value_type != 0) {
			reg_id = cmdq_core_interpret_wpr_value_data_register(
				op, arg_value);
			reqLen = snprintf(textBuf, bufLen, "Reg%d, ", reg_id);
			bufLen -= reqLen;
			textBuf += reqLen;
		} else {
			reqLen = snprintf(textBuf, bufLen, "0x%08x, ",
				arg_value);
			bufLen -= reqLen;
			textBuf += reqLen;
		}

		/* address */
		if (arg_addr_type != 0) {
			reg_id = cmdq_core_interpret_wpr_address_data_register(
				op, arg_addr);
			reqLen = snprintf(textBuf, bufLen, "Reg%d, ", reg_id);
			bufLen -= reqLen;
			textBuf += reqLen;
		} else {
			reg_addr = cmdq_core_subsys_to_reg_addr(arg_addr);

			reqLen = snprintf(textBuf, bufLen,
				"addr=0x%08x [%s], ", reg_addr & addr_mask,
				cmdq_get_func()->parseModule(reg_addr));
			bufLen -= reqLen;
			textBuf += reqLen;
		}

		use_mask = (arg_addr & 0x1);
		if (op == CMDQ_CODE_WRITE_S_W_MASK)
			use_mask = 1;
		else if (op == CMDQ_CODE_READ_S || op == CMDQ_CODE_WRITE_S)
			use_mask = 0;
		reqLen = snprintf(textBuf, bufLen, "use_mask=%d\n", use_mask);
		bufLen -= reqLen;
		textBuf += reqLen;
		break;
	case CMDQ_CODE_JUMP:
		if (arg_a) {
			if (arg_a & (1 << 22)) {
				/* jump by register */
				reqLen = snprintf(textBuf, bufLen,
					"JUMP(register): Reg%d\n", arg_b);
			} else {
				/* absolute */
				reqLen = snprintf(textBuf, bufLen,
					"JUMP(absolute):0x%08x\n", arg_b);
			}
		} else {
			/* relative */
			if ((s32) arg_b >= 0) {
				reqLen = snprintf(textBuf, bufLen,
					"JUMP(relative):+%d\n", (s32) arg_b);
			} else {
				reqLen = snprintf(textBuf, bufLen,
					"JUMP(relative):%d\n", (s32) arg_b);
			}
		}
		break;
	case CMDQ_CODE_WFE:
		if (arg_b == 0x80008001) {
			reqLen = snprintf(textBuf, bufLen,
				"Wait And Clear Event:%s\n",
				cmdq_core_get_event_name(arg_a));
		} else if (arg_b == 0x80000000) {
			reqLen = snprintf(textBuf, bufLen, "Clear Event:%s\n",
				cmdq_core_get_event_name(arg_a));
		} else if (arg_b == 0x80010000) {
			reqLen = snprintf(textBuf, bufLen, "Set Event:%s\n",
				cmdq_core_get_event_name(arg_a));
		} else if (arg_b == 0x00008001) {
			reqLen = snprintf(textBuf, bufLen,
				"Wait No Clear Event:%s\n",
				cmdq_core_get_event_name(arg_a));
		} else {
			reqLen = snprintf(textBuf, bufLen,
				"SYNC:%s, upd=%d, op=%d, val=%d, wait=%d, wop=%d, val=%d\n",
				cmdq_core_get_event_name(arg_a),
				(arg_b >> 31) & 0x1,
				(arg_b >> 28) & 0x7,
				(arg_b >> 16) & 0xFFF,
				(arg_b >> 15) & 0x1,
				(arg_b >> 12) & 0x7, (arg_b >> 0) & 0xFFF);
		}
		break;
	case CMDQ_CODE_EOC:
		if (arg_a == 0 && arg_b == 0x00000001) {
			reqLen = snprintf(textBuf, bufLen, "EOC\n");
		} else {
			if (cmdq_core_support_sync_non_suspendable()) {
				reqLen = snprintf(textBuf, bufLen,
					"MARKER: sync_no_suspnd=%d",
					(arg_a & (1 << 20)) > 0);
			} else {
				reqLen = snprintf(textBuf, bufLen, "MARKER:");
			}
			bufLen -= reqLen;
			textBuf += reqLen;
			if (arg_b == 0x00100000) {
				reqLen = snprintf(textBuf, bufLen, " Disable");
			} else if (arg_b == 0x00130000) {
				reqLen = snprintf(textBuf, bufLen, " Enable");
			} else {
				reqLen = snprintf(textBuf, bufLen,
					"no_suspnd=%d, no_inc=%d, m=%d, m_en=%d, prefetch=%d, irq=%d\n",
						(arg_a & (1 << 21)) > 0,
						(arg_a & (1 << 16)) > 0,
						(arg_b & (1 << 20)) > 0,
						(arg_b & (1 << 17)) > 0,
						(arg_b & (1 << 16)) > 0,
						(arg_b & (1 << 0)) > 0);
			}
		}
		break;
	case CMDQ_CODE_LOGIC:
		{
			const u32 subsys_bit =
				cmdq_get_func()->getSubsysLSBArgA();
			const u32 s_op =
				(arg_a & CMDQ_ARG_A_SUBSYS_MASK) >> subsys_bit;

			reqLen = snprintf(textBuf, bufLen, "%s: ",
				cmdq_core_parse_op(op));
			bufLen -= reqLen;
			textBuf += reqLen;

			reqLen = snprintf(textBuf, bufLen, "Reg%d = ",
				(arg_a & 0xFFFF));
			bufLen -= reqLen;
			textBuf += reqLen;

			if (s_op == CMDQ_LOGIC_ASSIGN) {
				reqLen = snprintf(textBuf, bufLen, "0x%08x\n",
					arg_b);
				bufLen -= reqLen;
				textBuf += reqLen;
			} else if (s_op == CMDQ_LOGIC_NOT) {
				const u32 arg_b_type = arg_a & (1 << 22);
				const u32 arg_b_i = (arg_b >> 16) & 0xFFFF;

				if (arg_b_type != 0)
					reqLen = snprintf(textBuf, bufLen,
						"~Reg%d\n", arg_b_i);
				else
					reqLen = snprintf(textBuf, bufLen,
						"~%d\n", arg_b_i);

				bufLen -= reqLen;
				textBuf += reqLen;
			} else {
				const u32 arg_b_i = (arg_b >> 16) & 0xFFFF;
				const u32 arg_c_i = arg_b & 0xFFFF;
				const u32 arg_b_type = arg_a & (1 << 22);
				const u32 arg_c_type = arg_a & (1 << 21);

				/* arg_b_i */
				if (arg_b_type != 0) {
					reqLen = snprintf(textBuf, bufLen,
						"Reg%d ", arg_b_i);
					bufLen -= reqLen;
					textBuf += reqLen;
				} else {
					reqLen = snprintf(textBuf, bufLen,
						"%d ", arg_b_i);
					bufLen -= reqLen;
					textBuf += reqLen;
				}
				/* operator */
				reqLen = snprintf(textBuf, bufLen, "%s ",
					cmdq_core_parse_logic_sop(s_op));
				bufLen -= reqLen;
				textBuf += reqLen;
				/* arg_c_i */
				if (arg_c_type != 0) {
					reqLen = snprintf(textBuf, bufLen,
						"Reg%d\n", arg_c_i);
					bufLen -= reqLen;
					textBuf += reqLen;
				} else {
					reqLen = snprintf(textBuf, bufLen,
						"%d\n", arg_c_i);
					bufLen -= reqLen;
					textBuf += reqLen;
				}
			}
		}
		break;
	case CMDQ_CODE_JUMP_C_RELATIVE:
	case CMDQ_CODE_JUMP_C_ABSOLUTE:
		{
			const u32 subsys_bit =
				cmdq_get_func()->getSubsysLSBArgA();
			const u32 s_op =
				(arg_a & CMDQ_ARG_A_SUBSYS_MASK) >> subsys_bit;
			const u32 arg_a_i = arg_a & 0xFFFF;
			const u32 arg_b_i = (arg_b >> 16) & 0xFFFF;
			const u32 arg_c_i = arg_b & 0xFFFF;
			const u32 arg_a_type = arg_a & (1 << 23);
			const u32 arg_b_type = arg_a & (1 << 22);
			const u32 arg_c_type = arg_a & (1 << 21);

			reqLen = snprintf(textBuf, bufLen, "%s: if (",
				cmdq_core_parse_op(op));
			bufLen -= reqLen;
			textBuf += reqLen;

			/* arg_b_i */
			if (arg_b_type != 0) {
				reqLen = snprintf(textBuf, bufLen, "Reg%d ",
					arg_b_i);
				bufLen -= reqLen;
				textBuf += reqLen;
			} else {
				reqLen = snprintf(textBuf, bufLen, "%d ",
					arg_b_i);
				bufLen -= reqLen;
				textBuf += reqLen;
			}
			/* operator */
			reqLen = snprintf(textBuf, bufLen, "%s ",
				cmdq_core_parse_jump_c_sop(s_op));
			bufLen -= reqLen;
			textBuf += reqLen;
			/* arg_c_i */
			if (arg_c_type != 0) {
				reqLen = snprintf(textBuf, bufLen,
					"Reg%d) jump ", arg_c_i);
				bufLen -= reqLen;
				textBuf += reqLen;
			} else {
				reqLen = snprintf(textBuf, bufLen,
					"%d) jump ", arg_c_i);
				bufLen -= reqLen;
				textBuf += reqLen;
			}
			/* jump to */
			if (arg_a_type != 0) {
				reqLen = snprintf(textBuf, bufLen,
					"Reg%d\n", arg_a_i);
				bufLen -= reqLen;
				textBuf += reqLen;
			} else {
				reqLen = snprintf(textBuf, bufLen,
					"+%d\n", arg_a_i);
				bufLen -= reqLen;
				textBuf += reqLen;
			}
		}
		break;
	default:
		reqLen = snprintf(textBuf, bufLen, "UNDEFINED\n");
		break;
	}

	return reqLen;
}

s32 cmdq_core_parse_instruction(const u32 *pCmd, char *textBuf, int bufLen)
{
	int reqLen = 0;

	const u32 op = (pCmd[1] & 0xFF000000) >> 24;
	const u32 arg_a = pCmd[1] & (~0xFF000000);
	const u32 arg_b = pCmd[0];

	reqLen = cmdq_core_interpret_instruction(
		textBuf, bufLen, op, arg_a, arg_b);

	return reqLen;
}

void cmdq_core_dump_error_instruction(const u32 *pcVA, const long pcPA,
	u32 *insts, int thread, u32 lineNum)
{
	char parsedInstruction[128] = { 0 };
	const u32 op = (insts[3] & 0xFF000000) >> 24;

	if (CMDQ_IS_END_ADDR(pcPA)) {
		/* in end address case instruction may not correct */
		CMDQ_ERR("PC stay at GCE end address, line:%u\n", lineNum);
		return;
	} else if (!pcVA) {
		CMDQ_ERR("Dump error instruction with null va, line:%u\n",
			lineNum);
		return;
	}

	cmdq_core_parse_instruction(pcVA, parsedInstruction,
		sizeof(parsedInstruction));
	CMDQ_ERR("Thread %d error instruction:0x%p, 0x%08x:0x%08x => %s",
		 thread, pcVA, insts[2], insts[3], parsedInstruction);

	/* for WFE, we specifically dump the event value */
	if (op == CMDQ_CODE_WFE) {
		u32 regValue = 0;
		const u32 eventID = 0x3FF & insts[3];

		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_ID, eventID);
		regValue = CMDQ_REG_GET32(CMDQ_SYNC_TOKEN_VAL);
		CMDQ_ERR("CMDQ_SYNC_TOKEN_VAL of %s is %d\n",
			 cmdq_core_get_event_name(eventID), regValue);
	}
}

static void cmdq_core_create_nginfo(const struct TaskStruct *ngtask,
	u32 *va_pc, struct NGTaskInfoStruct **nginfo_out)
{
	u8 *buffer = NULL;
	struct NGTaskInfoStruct *nginfo = NULL;
	struct CmdBufferStruct *cmd_buffer = NULL;

	buffer = kzalloc(ngtask->bufferSize, GFP_ATOMIC);
	if (!buffer) {
		CMDQ_ERR("fail to allocate NG buffer\n");
		return;
	}

	nginfo = kzalloc(sizeof(*nginfo), GFP_ATOMIC);
	if (!nginfo) {
		CMDQ_ERR("fail to allocate NG info\n");
		kfree(buffer);
		return;
	}

	/* copy ngtask info */
	cmd_buffer = list_first_entry(&ngtask->cmd_buffer_list,
		struct CmdBufferStruct, listEntry);
	nginfo->va_start = cmd_buffer->pVABase;
	nginfo->va_pc = va_pc;
	nginfo->buffer = (u32 *)buffer;
	nginfo->engine_flag = ngtask->engineFlag;
	nginfo->scenario = ngtask->scenario;
	nginfo->ngtask = ngtask;
	nginfo->buffer_size = ngtask->bufferSize;
	*nginfo_out = nginfo;

	list_for_each_entry(cmd_buffer, &ngtask->cmd_buffer_list, listEntry) {
		u32 buf_size = list_is_last(&cmd_buffer->listEntry,
			&ngtask->cmd_buffer_list) ?
			CMDQ_CMD_BUFFER_SIZE - ngtask->buf_available_size :
			CMDQ_CMD_BUFFER_SIZE;

		memcpy(buffer, cmd_buffer->pVABase, buf_size);
		if (va_pc >= cmd_buffer->pVABase &&
			va_pc < (u32 *)((u8 *)cmd_buffer->pVABase +
			CMDQ_CMD_BUFFER_SIZE)) {
			buffer += (u8 *)va_pc - (u8 *)cmd_buffer->pVABase;
			break;
		}
		buffer += buf_size;
	}

	/* only dump to pc */
	nginfo->dump_size = buffer - (u8 *)nginfo->buffer;
}

static void cmdq_core_release_nginfo(struct NGTaskInfoStruct *nginfo)
{
	if (nginfo) {
		kfree(nginfo->buffer);
		kfree(nginfo);
	}
}

static void cmdq_core_dump_summary(const struct TaskStruct *pTask, int thread,
	const struct TaskStruct **ngtask_out,
	struct NGTaskInfoStruct **nginfo_out)
{
	u32 *pcVA = NULL;
	u32 insts[4] = { 0 };
	struct ThreadStruct *pThread;
	const struct TaskStruct *pNGTask = NULL;
	const char *module = NULL;
	s32 index;
	u32 instA = 0, instB = 0;
	s32 irqFlag = 0;
	long currPC = 0;

	if (!pTask) {
		CMDQ_ERR("dump summary failed since pTask is NULL");
		return;
	}

	if ((list_empty(&pTask->cmd_buffer_list)) ||
		(thread == CMDQ_INVALID_THREAD)) {
		CMDQ_ERR(
			"dump summary failed since invalid param, pTask:%p thread:%d\n",
			pTask, thread);
		return;
	}

	if (pTask->secData.is_secure) {
#if defined(CMDQ_SECURE_PATH_SUPPORT)
		if (pTask->secStatus) {
			/* secure status may contains debug information */
			CMDQ_ERR(
				"Secure status:%d step:0x%08x args:0x%08x 0x%08x 0x%08x 0x%08x task:0x%p\n",
				pTask->secStatus->status,
				pTask->secStatus->step,
				pTask->secStatus->args[0],
				pTask->secStatus->args[1],
				pTask->secStatus->args[2],
				pTask->secStatus->args[3],
				pTask);
			for (index = 0; index < pTask->secStatus->inst_index;
				index += 2) {
				CMDQ_ERR(
					"Secure instruction %d:0x%08x:%08x\n",
					index / 2,
					pTask->secStatus->sec_inst[index],
					pTask->secStatus->sec_inst[index+1]);
			}
		}
#endif
		CMDQ_ERR("Summary dump does not support secure now.\n");
		pNGTask = pTask;
		return;
	}

	/* check if pc stay at fix end address */
	currPC = CMDQ_AREG_TO_PHYS(CMDQ_REG_GET32(
		CMDQ_THR_CURR_ADDR(thread)));
	if (!CMDQ_IS_END_ADDR(currPC)) {
		/* Find correct task */
		pThread = &(gCmdqContext.thread[thread]);
		pcVA = cmdq_core_get_pc(pTask, thread, insts, NULL);
		if (!pcVA) {
			/* Find all task to get correct PC */
			for (index = 0; index <
				cmdq_core_max_task_in_thread(thread);
				index++) {
				pNGTask = pThread->pCurTask[index];
				if (!pNGTask)
					continue;

				pcVA = cmdq_core_get_pc(pNGTask, thread,
					insts, NULL);
				if (pcVA) {
					/* we got NG task ! */
					break;
				}
			}
		}
	}
	if (!pNGTask)
		pNGTask = pTask;

	/* Do summary ! */
	cmdq_core_parse_error(pNGTask, thread, &module, &irqFlag, &instA,
		&instB);
	CMDQ_ERR("** [Module] %s **\n", module);
	if (pTask != pNGTask) {
		CMDQ_ERR(
			"** [Note] PC is not in first error task (0x%p) but in previous task (0x%p) **\n",
			pTask, pNGTask);
	}
	CMDQ_ERR(
		"** [Error Info] Refer to instruction and check engine dump for debug**\n");
	cmdq_core_dump_error_instruction(pcVA, currPC, insts, thread,
		__LINE__);
	cmdq_core_dump_disp_trigger_loop("ERR");

	*ngtask_out = pNGTask;
	if (nginfo_out) {
		cmdq_core_create_nginfo(pNGTask, pcVA, nginfo_out);
		(*nginfo_out)->module = module;
		(*nginfo_out)->irq_flag = irqFlag;
		(*nginfo_out)->inst[1] = instA;
		(*nginfo_out)->inst[0] = instB;
	}
}

void cmdqCoreDumpCommandMem(const u32 *pCmd, s32 commandSize)
{
	static char textBuf[128] = { 0 };
	int i;

	CMDQ_PROF_MUTEX_LOCK(gCmdqTaskMutex, dump_command_mem);

	print_hex_dump(KERN_ERR, "[CMDQ]", DUMP_PREFIX_ADDRESS, 16, 4, pCmd,
		commandSize, false);
	CMDQ_LOG("======TASK command buffer END\n");

	for (i = 0; i < commandSize; i += CMDQ_INST_SIZE, pCmd += 2) {
		cmdq_core_parse_instruction(pCmd, textBuf, 128);
		CMDQ_LOG("%p:%s", pCmd, textBuf);
	}
	CMDQ_LOG("TASK command buffer TRANSLATED END\n");

	CMDQ_PROF_MUTEX_UNLOCK(gCmdqTaskMutex, dump_command_mem);
}

void cmdq_core_dump_task_mem(const struct TaskStruct *pTask, bool full_dump)
{
	struct CmdBufferStruct *cmd_buffer = NULL;

	list_for_each_entry(cmd_buffer, &pTask->cmd_buffer_list, listEntry) {
		if (list_is_last(&cmd_buffer->listEntry,
			&pTask->cmd_buffer_list)) {
			u32 available_size = CMDQ_CMD_BUFFER_SIZE -
				pTask->buf_available_size;
			/* last one, dump according available size */
			CMDQ_LOG("VABase:0x%p MVABase:%pa size:%u\n",
				cmd_buffer->pVABase, &cmd_buffer->MVABase,
				available_size);
			if (full_dump)
				cmdqCoreDumpCommandMem(cmd_buffer->pVABase,
					available_size);
		} else {
			/* not last buffer, dump all */
			CMDQ_LOG("VABase:0x%p MVABase:%pa size:%lu\n",
				cmd_buffer->pVABase, &cmd_buffer->MVABase,
				CMDQ_CMD_BUFFER_SIZE);
			if (full_dump)
				cmdqCoreDumpCommandMem(cmd_buffer->pVABase,
				CMDQ_CMD_BUFFER_SIZE);
		}
	}
}

s32 cmdqCoreDebugDumpSRAM(u32 sram_base, u32 command_size)
{
	void *p_va_dest = NULL;
	dma_addr_t pa_dest = 0;
	s32 status = 0;
	u32 cpr_offset = CMDQ_INVALID_CPR_OFFSET;

	do {
		/* copy SRAM command to DRAM */
		cpr_offset = CMDQ_CPR_OFFSET(sram_base);
		CMDQ_LOG(
			"==Dump SRAM: size (%d) CPR OFFSET(0x%x), ADDR(0x%x)\n",
			command_size, cpr_offset, sram_base);
		p_va_dest = cmdq_core_alloc_hw_buffer(cmdq_dev_get(),
			command_size, &pa_dest, GFP_KERNEL);
		if (!p_va_dest)
			break;
		status = cmdq_task_copy_from_sram(pa_dest, cpr_offset,
			command_size);
		if (status < 0) {
			CMDQ_ERR("%s copy from sram API failed:%d\n",
				__func__, status);
			break;
		}
		CMDQ_LOG("======Dump SRAM: size (%d) SRAM command START\n",
			command_size);
		cmdqCoreDumpCommandMem(p_va_dest, command_size);
		CMDQ_LOG("======Dump SRAM command END\n");
	} while (0);

	if (p_va_dest)
		cmdq_core_free_hw_buffer(cmdq_dev_get(),
			command_size, p_va_dest, pa_dest);

	return status;
}

s32 cmdqCoreDebugDumpCommand(const struct TaskStruct *pTask)
{
	if (!pTask)
		return -EFAULT;

	CMDQ_LOG("======TASK 0x%p size (%d) command START\n",
		pTask, pTask->commandSize);
	if (cmdq_core_task_is_buffer_size_valid(pTask) == false) {
		CMDQ_ERR(
			"Buffer size:%u available size:%u of %u and end cmd:0x%p first va:0x%p out of sync!\n",
			pTask->bufferSize, pTask->buf_available_size,
			(u32)CMDQ_CMD_BUFFER_SIZE,
			pTask->pCMDEnd, cmdq_core_task_get_first_va(pTask));
	}

	cmdq_core_dump_task_mem(pTask, true);

	if (pTask->use_sram_buffer)
		cmdqCoreDebugDumpSRAM(pTask->sram_base, pTask->commandSize);

	CMDQ_LOG("======TASK 0x%p command END\n", pTask);
	return 0;
}

void cmdq_core_set_command_buffer_dump(s32 scenario, s32 bufferSize)
{
	mutex_lock(&gCmdqSaveBufferMutex);

	if (bufferSize != gCmdqBufferDump.bufferSize && bufferSize != -1) {
		if (gCmdqBufferDump.bufferSize) {
			vfree(gCmdqBufferDump.cmdqString);
			gCmdqBufferDump.bufferSize = 0;
			gCmdqBufferDump.count = 0;
		}

		if (bufferSize > 0) {
			gCmdqBufferDump.bufferSize = bufferSize;
			gCmdqBufferDump.cmdqString = vmalloc(
				gCmdqBufferDump.bufferSize);
		}
	}

	if (-1 == scenario) {
		/* clear all scenario */
		gCmdqBufferDump.scenario = 0LL;
	} else if (-2 == scenario) {
		/* set all scenario */
		gCmdqBufferDump.scenario = ~0LL;
	} else if (scenario >= 0 && scenario < CMDQ_MAX_SCENARIO_COUNT) {
		/* set scenario to save command buffer */
		gCmdqBufferDump.scenario |= (1LL << scenario);
	}

	CMDQ_LOG("[SET DUMP]CONFIG: bufferSize:%d scenario:0x%08llx\n",
		gCmdqBufferDump.bufferSize, gCmdqBufferDump.scenario);

	mutex_unlock(&gCmdqSaveBufferMutex);
}

static void cmdq_core_save_buffer(const char *string, ...)
{
	int logLen, redundantLen, i;
	va_list argptr;
	char *pBuffer;

	va_start(argptr, string);
	do {
		logLen = vsnprintf(NULL, 0, string, argptr) + 1;
		if (logLen <= 1)
			break;

		redundantLen = gCmdqBufferDump.bufferSize -
			gCmdqBufferDump.count;
		if (logLen >= redundantLen) {
			for (i = 0; i < redundantLen; i++)
				*(gCmdqBufferDump.cmdqString +
				gCmdqBufferDump.count + i) = 0;
			gCmdqBufferDump.count = 0;
		}

		pBuffer = gCmdqBufferDump.cmdqString + gCmdqBufferDump.count;
		gCmdqBufferDump.count += vsnprintf(
			pBuffer, logLen, string, argptr);
	} while (0);

	va_end(argptr);
}

static void cmdq_core_save_command_buffer_dump(const struct TaskStruct *pTask)
{
	static char textBuf[128] = { 0 };
	struct timeval savetv;
	struct tm nowTM;
	unsigned long long saveTimeSec;
	unsigned long rem_nsec;
	const u32 *pCmd;
	int i;
	struct CmdBufferStruct *cmd_buffer = NULL;
	u32 cmd_size = 0;

	if (gCmdqContext.errNum > 0)
		return;
	if (gCmdqBufferDump.bufferSize <= 0 || list_empty(
		&pTask->cmd_buffer_list))
		return;

	mutex_lock(&gCmdqSaveBufferMutex);

	if (gCmdqBufferDump.scenario & (1LL << pTask->scenario)) {
		cmdq_core_save_buffer(
			"************TASK command buffer TRANSLATED************\n");
		/* get kernel time */
		saveTimeSec = sched_clock();
		rem_nsec = do_div(saveTimeSec, 1000000000);
		/* get UTC time */
		do_gettimeofday(&savetv);
		time_to_tm(savetv.tv_sec, sys_tz.tz_minuteswest * 60, &nowTM);
		/* print current task information */
		cmdq_core_save_buffer("kernel time:[%5llu.%06lu],",
			saveTimeSec, rem_nsec / 1000);
		cmdq_core_save_buffer(
			" UTC time:[%04ld-%02d-%02d %02d:%02d:%02d.%06ld]",
			nowTM.tm_year + 1900, nowTM.tm_mon + 1, nowTM.tm_mday,
			nowTM.tm_hour, nowTM.tm_min, nowTM.tm_sec,
			savetv.tv_usec);
		cmdq_core_save_buffer(" Pid:%d Name:%s\n",
			pTask->callerPid, pTask->callerName);
		cmdq_core_save_buffer(
			"Task:0x%p Scenario:%d Size:%d Flag:0x%016llx\n",
			pTask, pTask->scenario, pTask->commandSize,
			pTask->engineFlag);
		list_for_each_entry(cmd_buffer, &pTask->cmd_buffer_list,
			listEntry) {
			pCmd = cmd_buffer->pVABase;

			if (list_is_last(&cmd_buffer->listEntry,
				&pTask->cmd_buffer_list))
				cmd_size = CMDQ_CMD_BUFFER_SIZE -
				pTask->buf_available_size;
			else
				cmd_size = CMDQ_CMD_BUFFER_SIZE;

			for (i = 0; i < cmd_size; i += CMDQ_INST_SIZE,
				pCmd += 2) {
				cmdq_core_parse_instruction(pCmd, textBuf,
					128);
				cmdq_core_save_buffer("[%5llu.%06lu] %s",
					saveTimeSec, rem_nsec / 1000, textBuf);
			}
		}
		cmdq_core_save_buffer(
			"****************TASK command buffer END***************\n\n");
	}

	mutex_unlock(&gCmdqSaveBufferMutex);
}

#ifdef CMDQ_INSTRUCTION_COUNT
CmdqModulePAStatStruct gCmdqModulePAStat;

CmdqModulePAStatStruct *cmdq_core_Initial_and_get_module_stat(void)
{
	memset(&gCmdqModulePAStat, 0, sizeof(gCmdqModulePAStat));

	return &gCmdqModulePAStat;
}

ssize_t cmdqCorePrintInstructionCountLevel(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	if (buf)
		len = sprintf(buf, "%d\n", gCmdqContext.instructionCountLevel);

	return len;
}

ssize_t cmdqCoreWriteInstructionCountLevel(
	struct device *dev, struct device_attribute *attr, const char *buf,
	size_t size)
{
	int len = 0;
	int value = 0;
	int status = 0;

	char textBuf[10] = { 0 };

	do {
		if (size >= 10) {
			status = -EFAULT;
			break;
		}

		len = size;
		memcpy(textBuf, buf, len);

		textBuf[len] = '\0';
		if (kstrtoint(textBuf, 10, &value) < 0) {
			status = -EFAULT;
			break;
		}

		status = len;
		if (value < 0)
			value = 0;

		cmdq_core_set_instruction_count_level(value);
	} while (0);

	return status;
}

void cmdq_core_set_instruction_count_level(const s32 value)
{
	gCmdqContext.instructionCountLevel = value;
}

static void cmdq_core_fill_module_stat(const u32 *pCommand,
	unsigned short *pModuleCount, u32 *pOtherInstruction,
	u32 *pOtherInstructionCount)
{

	const u32 arg_a = pCommand[1] & (~0xFF000000);
	const u32 addr = cmdq_core_subsys_to_reg_addr(arg_a);
	s32 i;

	for (i = 0; i < CMDQ_MODULE_STAT_GPR; i++) {
		if ((gCmdqModulePAStat.start[i] > 0) && (addr >=
			gCmdqModulePAStat.start[i])
		    && (addr <= gCmdqModulePAStat.end[i])) {
			pModuleCount[i]++;
			break;
		}
	}

	if (i >= CMDQ_MODULE_STAT_GPR) {
		if (3 & (pCommand[1] >> 22)) {
			pModuleCount[CMDQ_MODULE_STAT_GPR]++;
		} else {
			pOtherInstruction[(*pOtherInstructionCount)++] = addr;
			pModuleCount[CMDQ_MODULE_STAT_OTHERS]++;
		}
	}
}

static void cmdq_core_fill_module_event_count(
	const u32 *pCommand, unsigned short *pEventCount)
{

	const u32 arg_a = pCommand[1] & (~0xFF000000);

	if (arg_a >= CMDQ_MAX_HW_EVENT_COUNT)
		pEventCount[CMDQ_EVENT_STAT_SW]++;
	else
		pEventCount[CMDQ_EVENT_STAT_HW]++;
}

static void cmdq_core_fill_task_instruction_stat(
	struct RecordStruct *pRecord, const struct TaskStruct *pTask)
{
	bool invalidinstruction = false;
	s32 commandIndex = 0;
	u32 arg_a_prefetch_en, arg_b_prefetch_en, arg_a_prefetch_dis,
		arg_b_prefetch_dis;
	u32 *pCommand;
	u32 op;
	struct list_head *p = NULL;
	struct CmdBufferStruct *cmd_buffer = NULL;
	u32 buf_size = 0;

	if (gCmdqContext.instructionCountLevel < 1)
		return;

	if (!pRecord || !pTask)
		return;

	memset(&(pRecord->instructionStat[0]), 0x0,
		sizeof(pRecord->instructionStat));
	memset(&(pRecord->writeModule[0]), 0x0, sizeof(pRecord->writeModule));
	memset(&(pRecord->writewmaskModule[0]), 0x0,
		sizeof(pRecord->writewmaskModule));
	memset(&(pRecord->readModlule[0]), 0x0, sizeof(pRecord->readModlule));
	memset(&(pRecord->pollModule[0]), 0x0, sizeof(pRecord->pollModule));
	memset(&(pRecord->eventCount[0]), 0x0, sizeof(pRecord->eventCount));
	memset(&(pRecord->otherInstr[0]), 0x0, sizeof(pRecord->otherInstr));
	pRecord->otherInstrNUM = 0;

	cmd_buffer = list_first_entry(&pTask->cmd_buffer_list,
		struct CmdBufferStruct, listEntry);
	if (list_is_last(cmd_buffer, &pTask->cmd_buffer_list))
		buf_size = CMDQ_CMD_BUFFER_SIZE - pTask->buf_available_size;
	else
		buf_size = CMDQ_CMD_BUFFER_SIZE;

	do {
		if (commandIndex >= buf_size) {
			cmd_buffer = list_next_entry(cmd_buffer, listEntry);
			commandIndex = 0;
			if (list_is_last(&cmd_buffer->listEntry,
				&pTask->cmd_buffer_list))
				buf_size = CMDQ_CMD_BUFFER_SIZE -
				pTask->buf_available_size;
			else
				buf_size = CMDQ_CMD_BUFFER_SIZE;
		}
		pCommand = (u32 *)((u8 *) cmd_buffer->pVABase +
			commandIndex);
		op = (pCommand[1] & 0xFF000000) >> 24;

		switch (op) {
		case CMDQ_CODE_MOVE:
			if (1 & (pCommand[1] >> 23)) {
				if (CMDQ_SPECIAL_SUBSYS_ADDR ==
					cmdq_core_subsys_from_phys_addr(
					pCommand[0])) {
					commandIndex += CMDQ_INST_SIZE;
					pRecord->instructionStat[
						CMDQ_STAT_WRITE]++;
					pRecord->writeModule[
						CMDQ_MODULE_STAT_DISP_PWM]++;
				} else {
					pRecord->instructionStat[
						CMDQ_STAT_MOVE]++;
				}
			} else if ((commandIndex + CMDQ_INST_SIZE) <
				pTask->commandSize) {
				pCommand = (u32 *)((u8 *)
					cmd_buffer->pVABase + commandIndex);
				pCommand = (u32 *)((u8 *)pTask->pVABase +
					commandIndex);
				op = (pCommand[1] & 0xFF000000) >> 24;

				if (op == CMDQ_CODE_WRITE) {
					pRecord->instructionStat[
						CMDQ_STAT_WRITE_W_MASK]++;
					cmdq_core_fill_module_stat(pCommand,
						pRecord->writewmaskModule,
						pRecord->otherInstr,
						&pRecord->otherInstrNUM);
				} else if (op == CMDQ_CODE_POLL) {
					pRecord->instructionStat[
						CMDQ_STAT_POLLING]++;
					cmdq_core_fill_module_stat(pCommand,
						pRecord->pollModule,
						pRecord->otherInstr,
						&pRecord->otherInstrNUM);
				} else {
					invalidinstruction = true;
				}
			} else {
				invalidinstruction = true;
			}
			break;
		case CMDQ_CODE_READ:
			pRecord->instructionStat[CMDQ_STAT_READ]++;
			cmdq_core_fill_module_stat(pCommand,
				pRecord->readModlule,
				pRecord->otherInstr,
				&pRecord->otherInstrNUM);
			break;
		case CMDQ_CODE_WRITE:
			pRecord->instructionStat[CMDQ_STAT_WRITE]++;
			cmdq_core_fill_module_stat(pCommand,
				pRecord->writeModule,
				pRecord->otherInstr, &pRecord->otherInstrNUM);
			break;
		case CMDQ_CODE_WFE:
			pRecord->instructionStat[CMDQ_STAT_SYNC]++;
			cmdq_core_fill_module_event_count(pCommand,
				pRecord->eventCount);
			break;
		case CMDQ_CODE_JUMP:
			pRecord->instructionStat[CMDQ_STAT_JUMP]++;
			break;
		case CMDQ_CODE_EOC:
			arg_b_prefetch_en = ((1 << 20) | (1 << 17) |
				(1 << 16));
			arg_a_prefetch_en =
			    (CMDQ_CODE_EOC << 24) | (0x1 << (53 - 32)) |
			    (0x1 << (48 - 32));
			arg_b_prefetch_dis = (1 << 20);
			arg_a_prefetch_dis = (CMDQ_CODE_EOC << 24) |
				(0x1 << (48 - 32));

			if (arg_b_prefetch_en == pCommand[0] &&
				arg_a_prefetch_en == pCommand[1]) {
				pRecord->instructionStat[
					CMDQ_STAT_PREFETCH_EN]++;
			} else if (arg_b_prefetch_dis == pCommand[0] &&
				arg_a_prefetch_dis == pCommand[1]) {
				pRecord->instructionStat[
					CMDQ_STAT_PREFETCH_DIS]++;
			} else {
				pRecord->instructionStat[CMDQ_STAT_EOC]++;
			}
			break;
		default:
			invalidinstruction = true;
			break;
		}
		commandIndex += CMDQ_INST_SIZE;
		if (invalidinstruction == true) {
			memset(&(pRecord->instructionStat[0]), 0x0,
			       sizeof(pRecord->instructionStat));
			break;
		}
	} while (pCommand != pTask->pCMDEnd + 1);
}

static const char *gCmdqModuleInstructionLabel[CMDQ_MODULE_STAT_MAX] = {
	"MMSYS_CONFIG",
	"MDP_RDMA",
	"MDP_RSZ0",
	"MDP_RSZ1",
	"MDP_WDMA",
	"MDP_WROT",
	"MDP_TDSHP",
	"MM_MUTEX",
	"VENC",
	"DISP_OVL0",
	"DISP_OVL1",
	"DISP_RDMA0",
	"DISP_RDMA1",
	"DISP_WDMA0",
	"DISP_COLOR",
	"DISP_CCORR",
	"DISP_AAL",
	"DISP_GAMMA",
	"DISP_DITHER",
	"DISP_UFOE",
	"DISP_PWM",
	"DISP_WDMA1",
	"DISP_MUTEX",
	"DISP_DSI0",
	"DISP_DPI0",
	"DISP_OD",
	"CAM0",
	"CAM1",
	"CAM2",
	"CAM3",
	"SODI",
	"GPR",
	"Others",
};

int cmdqCorePrintInstructionCountSeq(struct seq_file *m, void *v)
{
	unsigned long flags;
	s32 i;
	s32 index;
	s32 numRec;
	struct RecordStruct record;

	if (gCmdqContext.instructionCountLevel < 1)
		return 0;

	seq_puts(m,
		"Record ID, PID, scenario, total, write, write_w_mask, read,");
	seq_puts(m,
		" polling, move, sync, prefetch_en, prefetch_dis, EOC, jump");
	for (i = 0; i < CMDQ_MODULE_STAT_MAX; i++) {
		seq_printf(m,
			", (%s)=>, write, write_w_mask, read, polling",
			gCmdqModuleInstructionLabel[i]);
	}
	seq_puts(m, ", (SYNC)=>, HW Event, SW Event\n");
	/* we try to minimize time spent in spin lock */
	/* since record is an array so it is okay to */
	/* allow displaying an out-of-date entry. */
	CMDQ_PROF_SPIN_LOCK(gCmdqRecordLock, flags, print_inst);
	numRec = gCmdqContext.recNum;
	index = gCmdqContext.lastID - 1;
	CMDQ_PROF_SPIN_UNLOCK(gCmdqRecordLock, flags, print_inst);

	/* we print record in reverse order. */
	for (; numRec > 0; --numRec, --index) {
		if (index >= CMDQ_MAX_RECORD_COUNT)
			index = 0;
		else if (index < 0)
			index = CMDQ_MAX_RECORD_COUNT - 1;

		CMDQ_PROF_SPIN_LOCK(gCmdqRecordLock, flags, print_inst_loop);
		record = gCmdqContext.record[index];
		CMDQ_PROF_SPIN_UNLOCK(gCmdqRecordLock, flags, print_inst_loop);

		if ((record.instructionStat[CMDQ_STAT_EOC] == 0) &&
		    (record.instructionStat[CMDQ_STAT_JUMP] == 0)) {
			seq_printf(m, "%4d, %5c, %2c, %4d",
				index, 'X', 'X', 0);
			for (i = 0; i < CMDQ_STAT_MAX; i++)
				seq_printf(m, ", %4d", 0);

			for (i = 0; i < CMDQ_MODULE_STAT_MAX; i++)
				seq_printf(m, ", , %4d, %4d, %4d, %4d",
				0, 0, 0, 0);

			seq_printf(m, ", , %4d, %4d", 0, 0);
		} else {
			u32 totalCount = (u32) (record.size / CMDQ_INST_SIZE);

			seq_printf(m, " %4d, %5d, %02d, %4d", index,
				record.user, record.scenario,
				   totalCount);
			for (i = 0; i < CMDQ_STAT_MAX; i++)
				seq_printf(m, ", %4d",
				record.instructionStat[i]);

			for (i = 0; i < CMDQ_MODULE_STAT_MAX; i++) {
				seq_printf(m, ", , %4d, %4d, %4d, %4d",
					record.writeModule[i],
					record.writewmaskModule[i],
					record.readModlule[i],
					record.pollModule[i]);
			}
			seq_printf(m, ", , %4d, %4d",
				record.eventCount[CMDQ_EVENT_STAT_HW],
				record.eventCount[CMDQ_EVENT_STAT_SW]);
		}
		seq_puts(m, "\n");

	}

	seq_puts(m, "\n\n==============Other Instruction==============\n");
	/* we try to minimize time spent in spin lock
	 * since record is an array so it is okay to
	 * allow displaying an out-of-date entry.
	 */
	CMDQ_PROF_SPIN_LOCK(gCmdqRecordLock, flags, print_inst_finish);
	numRec = gCmdqContext.recNum;
	index = gCmdqContext.lastID - 1;
	CMDQ_PROF_SPIN_UNLOCK(gCmdqRecordLock, flags, print_inst_finish);

	/* we print record in reverse order. */
	for (; numRec > 0; --numRec, --index) {
		if (index >= CMDQ_MAX_RECORD_COUNT)
			index = 0;
		else if (index < 0)
			index = CMDQ_MAX_RECORD_COUNT - 1;

		CMDQ_PROF_SPIN_LOCK(gCmdqRecordLock, flags, print_inst_record);
		record = gCmdqContext.record[index];
		CMDQ_PROF_SPIN_UNLOCK(gCmdqRecordLock, flags,
			print_inst_record);

		for (i = 0; i < record.otherInstrNUM; i++)
			seq_printf(m, "0x%08x\n", record.otherInstr[i]);
	}

	return 0;
}
#endif

static void cmdq_core_fill_task_profile_marker_record(
	struct RecordStruct *pRecord, const struct TaskStruct *pTask)
{
	u32 i;
	u32 profileMarkerCount;
	u32 value;
	cmdqBackupSlotHandle hSlot;

	if (!pRecord || !pTask)
		return;

	if (pTask->profileMarker.hSlot == 0)
		return;

	profileMarkerCount = pTask->profileMarker.count;
	hSlot = (cmdqBackupSlotHandle) (pTask->profileMarker.hSlot);

	pRecord->profileMarkerCount = profileMarkerCount;
	for (i = 0; i < profileMarkerCount; i++) {
		/* timestamp, each count is 76ns */
		cmdq_cpu_read_mem(hSlot, i, &value);
		pRecord->profileMarkerTimeNS[i] = value * 38;
		pRecord->profileMarkerTag[i] = (char *)(CMDQ_U32_PTR(
			pTask->profileMarker.tag[i]));
	}
}

static void cmdq_core_fill_task_record(
	struct RecordStruct *pRecord, const struct TaskStruct *pTask,
	u32 thread)
{
	if (pRecord && pTask) {
		/* Record scenario */
		pRecord->user = pTask->callerPid;
		pRecord->scenario = pTask->scenario;
		pRecord->priority = pTask->priority;
		pRecord->thread = thread;
		pRecord->reorder = pTask->reorder;
		pRecord->engineFlag = pTask->engineFlag;
		pRecord->size = pTask->commandSize;

		pRecord->is_secure = pTask->secData.is_secure;

		/* Record time */
		pRecord->submit = pTask->submit;
		pRecord->trigger = pTask->trigger;
		pRecord->gotIRQ = pTask->gotIRQ;
		pRecord->beginWait = pTask->beginWait;
		pRecord->wakedUp = pTask->wakedUp;
		pRecord->durAlloc = pTask->durAlloc;
		pRecord->durReclaim = pTask->durReclaim;
		pRecord->durRelease = pTask->durRelease;

		/* Record address */
		if (!list_empty(&pTask->cmd_buffer_list)) {
			struct CmdBufferStruct *first_entry =
				list_first_entry(&pTask->cmd_buffer_list,
				struct CmdBufferStruct, listEntry);
			struct CmdBufferStruct *last_entry = list_last_entry(
				&pTask->cmd_buffer_list,
				struct CmdBufferStruct, listEntry);

			pRecord->start = (u32)first_entry->MVABase;
			pRecord->end = (u32)last_entry->MVABase +
				CMDQ_CMD_BUFFER_SIZE -
				pTask->buf_available_size;
			pRecord->jump = pTask->pCMDEnd ?
				pTask->pCMDEnd[-1] : 0;
		} else {
			pRecord->start = 0;
			pRecord->end = 0;
			pRecord->jump = 0;
		}

		cmdq_core_fill_task_profile_marker_record(pRecord, pTask);
#ifdef CMDQ_INSTRUCTION_COUNT
		/* Instruction count statistics */
		cmdq_core_fill_task_instruction_stat(pRecord, pTask);
#endif
	}
}

static void cmdq_core_track_task_record(struct TaskStruct *pTask, u32 thread)
{
	struct RecordStruct *pRecord;
	unsigned long flags;
	CMDQ_TIME done;
	int lastID;
	char buf[256];
	int length;

	done = sched_clock();

	CMDQ_PROF_SPIN_LOCK(gCmdqRecordLock, flags, track_record);

	pRecord = &(gCmdqContext.record[gCmdqContext.lastID]);
	lastID = gCmdqContext.lastID;

	cmdq_core_fill_task_record(pRecord, pTask, thread);

	pRecord->done = done;

	gCmdqContext.lastID++;
	if (gCmdqContext.lastID >= CMDQ_MAX_RECORD_COUNT)
		gCmdqContext.lastID = 0;

	gCmdqContext.recNum++;
	if (gCmdqContext.recNum >= CMDQ_MAX_RECORD_COUNT)
		gCmdqContext.recNum = CMDQ_MAX_RECORD_COUNT;

	CMDQ_PROF_SPIN_UNLOCK(gCmdqRecordLock, flags, track_record);

	if (pTask->dumpAllocTime) {
		length = cmdq_core_print_record(pRecord, lastID, buf,
			ARRAY_SIZE(buf));
		CMDQ_LOG("Record:%s", buf);
	}
}

static void cmdq_core_dump_error_buffer(
	const struct TaskStruct *pTask, u32 *hwPC)
{
	struct CmdBufferStruct *cmd_buffer = NULL;
	u32 cmd_size = 0, dump_size = 0;
	bool dump = false;
	bool internal = CMDQ_TASK_IS_INTERNAL(pTask);
	u32 dump_buff_count = 0;

	if (list_empty(&pTask->cmd_buffer_list))
		return;

	if (hwPC) {
		list_for_each_entry(cmd_buffer, &pTask->cmd_buffer_list,
			listEntry) {
			if (list_is_last(&cmd_buffer->listEntry,
				&pTask->cmd_buffer_list))
				cmd_size = CMDQ_CMD_BUFFER_SIZE -
				pTask->buf_available_size;
			else
				cmd_size = CMDQ_CMD_BUFFER_SIZE;
			if (hwPC >= cmd_buffer->pVABase &&
				hwPC < (u32 *)(((u8 *)cmd_buffer->pVABase) +
				cmd_size)) {
				/* because hwPC points to "start" of the
				 * instruction, add offset 1
				 */
				dump_size = (u32)(2 + hwPC -
				cmd_buffer->pVABase) * sizeof(u32);
				dump = true;
			} else {
				dump_size = cmd_size;
			}

			print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS,
				16, 4, cmd_buffer->pVABase, dump_size, true);
			cmdq_core_save_hex_first_dump("", 16, 4,
				cmd_buffer->pVABase, dump_size);

			if (dump)
				break;

			if (internal && dump_buff_count++ >= 2)
				break;
		}
	}

	if (!dump)
		CMDQ_ERR("hwPC is not in region, dump all\n");
}

static void cmdq_core_dump_ng_error_buffer(
	const struct NGTaskInfoStruct *nginfo)
{
	CMDQ_ERR("NGTask buffer start va:0x%p pc:0x%p size:%u dump size:%u\n",
		nginfo->va_start, nginfo->va_pc,
		nginfo->buffer_size, nginfo->dump_size);
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 16, 4,
		nginfo->buffer, nginfo->dump_size, true);
	cmdq_core_save_hex_first_dump("", 16, 4, nginfo->buffer,
		nginfo->dump_size);
}

void cmdq_core_dump_thread(s32 thread, const char *tag)
{
	struct ThreadStruct *pThread;
	u32 value[15] = { 0 };

	pThread = &(gCmdqContext.thread[thread]);
	if (pThread->taskCount == 0 && thread != CMDQ_DELAY_THREAD_ID)
		return;

	CMDQ_LOG(
		"[%s]===== [CMDQ] Error Thread Status index:%d enabled:%d =====\n",
		tag, thread, value[8]);
	/* normal thread */
	value[0] = CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread));
	value[1] = CMDQ_REG_GET32(CMDQ_THR_END_ADDR(thread));
	value[2] = CMDQ_REG_GET32(CMDQ_THR_WAIT_TOKEN(thread));
	value[3] = cmdq_core_thread_exec_counter(thread);
	value[4] = CMDQ_REG_GET32(CMDQ_THR_IRQ_STATUS(thread));
	value[5] = CMDQ_REG_GET32(CMDQ_THR_INST_CYCLES(thread));
	value[6] = CMDQ_REG_GET32(CMDQ_THR_CURR_STATUS(thread));
	value[7] = CMDQ_REG_GET32(CMDQ_THR_IRQ_ENABLE(thread));
	value[8] = CMDQ_REG_GET32(CMDQ_THR_ENABLE_TASK(thread));

	value[9] = CMDQ_REG_GET32(CMDQ_THR_WARM_RESET(thread));
	value[10] = CMDQ_REG_GET32(CMDQ_THR_SUSPEND_TASK(thread));
	value[11] = CMDQ_REG_GET32(CMDQ_THR_SECURITY(thread));
	value[12] = CMDQ_REG_GET32(CMDQ_THR_CFG(thread));
	value[13] = CMDQ_REG_GET32(CMDQ_THR_PREFETCH(thread));
	value[14] = CMDQ_REG_GET32(CMDQ_THR_INST_THRESX(thread));

	CMDQ_LOG(
		"[%s]PC:0x%08x End:0x%08x Wait Token:0x%08x IRQ:0x%x IRQ_EN:0x%x\n",
		tag, value[0], value[1], value[2], value[4], value[7]);
	CMDQ_LOG(
		"[%s]Curr Cookie:%d Wait Cookie:%d Next Cookie:%d Task Count:%d engineFlag:0x%llx\n",
		tag, value[3], pThread->waitCookie, pThread->nextCookie,
		pThread->taskCount, pThread->engineFlag);
	CMDQ_LOG(
		"[%s]Timeout Cycle:%d Status:0x%x reset:0x%x Suspend:%d sec:%d cfg:%d prefetch:%d thrsex:%d\n",
		tag, value[5], value[6], value[9], value[10],
		value[11], value[12], value[13], value[14]);
}

static void cmdq_core_dump_error_task(const struct TaskStruct *pTask,
	const struct TaskStruct *pNGTask, u32 thread, u32 **pc_out)
{
	u32 *hwPC = NULL;
	u64 printEngineFlag = 0;

	cmdq_core_dump_thread(thread, "ERR");

	/* Begin is not first, save NG task but print pTask as well */
	if (pNGTask && pNGTask != pTask) {
		CMDQ_ERR(
			"== [CMDQ] We have NG task, so engine dumps may more than you think ==\n");
		CMDQ_ERR(
			"========== [CMDQ] Error Thread PC (NG Task) ==========\n");
		cmdq_core_dump_pc(pNGTask, thread, "ERR");

		CMDQ_ERR(
			"========= [CMDQ] Error Task Status (NG Task) =========\n");
		cmdq_core_dump_task(pNGTask);

		printEngineFlag |= pNGTask->engineFlag;
	}

	if (pTask) {
		CMDQ_ERR(
			"=============== [CMDQ] Error Thread PC ===============\n");
		hwPC = cmdq_core_dump_pc(pTask, thread, "ERR");

		CMDQ_ERR(
			"=============== [CMDQ] Error Task Status ===============\n");
		cmdq_core_dump_task(pTask);

		printEngineFlag |= pTask->engineFlag;
	}

	/* skip internal testcase */
	if (gCmdqContext.errNum > 1 &&
		((pTask && CMDQ_TASK_IS_INTERNAL(pTask)) ||
		(pNGTask && CMDQ_TASK_IS_INTERNAL(pNGTask))))
		return;

	/* dump tasks in error thread */
	cmdq_core_dump_task_in_thread(thread, false, false, false);
	cmdq_core_dump_task_with_engine_flag(printEngineFlag, thread);

	CMDQ_ERR("=============== [CMDQ] CMDQ Status ===============\n");
	cmdq_core_dump_status("ERR");

	CMDQ_ERR(
		"=============== [CMDQ] Clock Gating Status ===============\n");
	CMDQ_ERR("[CLOCK] common clock ref=%d\n",
		atomic_read(&gCmdqThreadUsage));

	if (pc_out)
		*pc_out = hwPC;
}

static void cmdq_core_attach_cmdq_error(
	const struct TaskStruct *task, s32 thread,
	u32 **pc_out, struct NGTaskInfoStruct **nginfo_out)
{
	const struct TaskStruct *ngtask = NULL;
	u64 eng_flag = 0;
	s32 index = 0;

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->warning, MMPROFILE_FLAG_PULSE,
		((unsigned long)task), thread);

	/* Update engine fail count */
	eng_flag = task->engineFlag;
	for (index = 0; index < CMDQ_MAX_ENGINE_COUNT; index++) {
		if (eng_flag & (1LL << index))
			gCmdqContext.engine[index].failCount++;
	}

	/* register error record */
	if (gCmdqContext.errNum < CMDQ_MAX_ERROR_COUNT) {
		struct ErrorStruct *error =
			&gCmdqContext.error[gCmdqContext.errNum];

		cmdq_core_fill_task_record(&error->errorRec, task, thread);
		error->ts_nsec = local_clock();
	}

	/* Turn on first CMDQ error dump */
	cmdq_core_turnon_first_dump(task);

	/* Then we just print out info */
	CMDQ_ERR(
		"================= [CMDQ] Begin of Error %d================\n",
		gCmdqContext.errNum);

	cmdq_core_dump_summary(task, thread, &ngtask, nginfo_out);
	cmdq_core_dump_error_task(task, ngtask, thread, pc_out);

	CMDQ_ERR(
		"================= [CMDQ] End of Error %d ================\n",
		gCmdqContext.errNum);
	gCmdqContext.errNum++;
}

static void cmdq_core_attach_engine_error(const struct TaskStruct *task,
	s32 thread, const struct NGTaskInfoStruct *nginfo, bool short_log)
{
	s32 index = 0;
	bool disp_scn = false;
	u64 print_eng_flag = 0;
	struct CmdqCBkStruct *callback = NULL;
	static const char *const engineGroupName[] = {
		CMDQ_FOREACH_GROUP(GENERATE_STRING)
	};

#ifndef CONFIG_MTK_FPGA
	CMDQ_ERR("=============== [CMDQ] SMI Status ===============\n");
	cmdq_get_func()->dumpSMI(1);
#endif

	if (short_log) {
		CMDQ_ERR(
			"=============== skip detail error dump ===============\n");
		return;
	}

	print_eng_flag = task->engineFlag;
	if (nginfo)
		print_eng_flag |= nginfo->engine_flag;

	/* Dump MMSYS configuration */
	CMDQ_ERR("=============== [CMDQ] MMSYS_CONFIG ===============\n");
	cmdq_mdp_get_func()->dumpMMSYSConfig();

	/* ask each module to print their status */
	CMDQ_ERR("=============== [CMDQ] Engine Status ===============\n");
	callback = gCmdqGroupCallback;
	for (index = 0; index < CMDQ_MAX_GROUP_COUNT; index++) {
		if (!cmdq_core_is_group_flag((enum CMDQ_GROUP_ENUM) index,
			print_eng_flag))
			continue;

		CMDQ_ERR("====== engine group %s status =======\n",
			engineGroupName[index]);

		if (!callback[index].dumpInfo) {
			CMDQ_ERR("(no dump function)\n");
			continue;
		}

		callback[index].dumpInfo((
			cmdq_mdp_get_func()->getEngineGroupBits(index) &
			print_eng_flag),
			gCmdqContext.logLevel);
	}

	/* force dump DISP for DISP scenario with 0x0 engine flag */
	disp_scn = cmdq_get_func()->isDispScenario(task->scenario);

	if (nginfo)
		disp_scn = disp_scn |
			cmdq_get_func()->isDispScenario(nginfo->scenario);

	if (disp_scn) {
		index = CMDQ_GROUP_DISP;
		if (callback[index].dumpInfo) {
			callback[index].dumpInfo((
				cmdq_mdp_get_func()->getEngineGroupBits(
					index) & print_eng_flag),
				gCmdqContext.logLevel);
		}
	}

	for (index = 0; index < CMDQ_MAX_GROUP_COUNT; ++index) {
		if (!cmdq_core_is_group_flag((enum CMDQ_GROUP_ENUM) index,
			print_eng_flag))
			continue;

		if (callback[index].errorReset) {
			callback[index].errorReset(
				cmdq_mdp_get_func()->getEngineGroupBits(
					index) & print_eng_flag);
			CMDQ_LOG(
				"engine group (%s) reset: function is called\n",
				engineGroupName[index]);
		}

	}
}

static void cmdq_core_dump_task_command(const struct TaskStruct *task,
	u32 *pc, const struct NGTaskInfoStruct *nginfo)
{
	/* Begin is not first, save NG task but print pTask as well */
	if (nginfo && nginfo->ngtask != task) {
		CMDQ_ERR(
			"========== [CMDQ] Error Command Buffer (NG Task) ==========\n");
		cmdq_core_dump_ng_error_buffer(nginfo);
	}

	CMDQ_ERR(
		"=============== [CMDQ] Error Command Buffer ===============\n");
	cmdq_core_dump_error_buffer(task, pc);
}

static void cmdq_core_attach_error_task_detail(const struct TaskStruct *task,
	s32 thread, const struct TaskStruct **ngtask_out,
	const char **module_out, u32 *irq_flag_out,
	u32 *inst_a_out, u32 *inst_b_out)
{
	unsigned long flags = 0L;
	u32 *pc = NULL;
	s32 error_num = gCmdqContext.errNum;
	bool detail_log = false;
	struct NGTaskInfoStruct *nginfo = NULL;

	if (!task) {
		CMDQ_ERR("attach error failed since pTask is NULL");
		return;
	}

	/* prevent errors dump at same time */
	CMDQ_PROF_MUTEX_LOCK(gCmdqErrMutex, dump_error);

	CMDQ_PROF_SPIN_LOCK(gCmdqExecLock, flags, attach_error);
	cmdq_core_attach_cmdq_error(task, thread, &pc, &nginfo);
	CMDQ_PROF_SPIN_UNLOCK(gCmdqExecLock, flags, attach_error_done);

	if (nginfo) {
		if (ngtask_out && nginfo->ngtask)
			*ngtask_out = nginfo->ngtask;

		if (module_out && irq_flag_out && inst_a_out && inst_b_out) {
			*module_out = nginfo->module;
			*irq_flag_out = nginfo->irq_flag;
			*inst_a_out = nginfo->inst[1];
			*inst_b_out = nginfo->inst[0];
		}

		/* skip internal testcase */
		if (CMDQ_TASK_IS_INTERNAL(task)) {
			cmdq_core_release_nginfo(nginfo);
			CMDQ_PROF_MUTEX_UNLOCK(gCmdqErrMutex,
				dump_error_simple);
			return;
		}
	}

	detail_log = error_num <= 2 || error_num % 16 == 0 ||
		cmdq_core_should_full_error();
	cmdq_core_attach_engine_error(task, thread, nginfo, !detail_log);
	if (detail_log)
		cmdq_core_dump_task_command(task, pc, nginfo);

	CMDQ_ERR(
		"================= [CMDQ] End of Full Error %d ================\n",
		error_num);
	CMDQ_PROF_MUTEX_UNLOCK(gCmdqErrMutex, dump_error);

	cmdq_core_release_nginfo(nginfo);
}

static void cmdq_core_attach_error_task(const struct TaskStruct *task,
	s32 thread)
{
	cmdq_core_attach_error_task_detail(task, thread, NULL, NULL, NULL,
		NULL, NULL);
}

static void cmdq_core_attach_error_task_unlock(const struct TaskStruct *task,
	s32 thread)
{
	u32 *pc = NULL;
	s32 error_num = gCmdqContext.errNum;
	bool detail_log = false;

	if (!task) {
		CMDQ_ERR("attach error failed since pTask is NULL");
		return;
	}

	cmdq_core_attach_cmdq_error(task, thread, &pc, NULL);
	detail_log = error_num <= 2 || error_num % 16 == 0 ||
		cmdq_core_should_full_error();
	if (detail_log) {
		cmdq_core_dump_task_command(task, pc, NULL);
		CMDQ_ERR(
			"================= [CMDQ] End of Full Error %d ================\n",
			error_num);
	}
}

static s32 cmdq_core_insert_task_from_thread_array_by_cookie(
	struct TaskStruct *pTask, struct ThreadStruct *pThread,
	const s32 cookie, const bool resetHWThread)
{

	if (!pTask || !pThread) {
		CMDQ_ERR(
			"invalid param pTask:0x%p pThread:0x%p cookie:%d needReset:%d\n",
			pTask, pThread, cookie, resetHWThread);
		return -EFAULT;
	}

	if (resetHWThread == true) {
		pThread->waitCookie = cookie;
		pThread->nextCookie = cookie + 1;
		if (pThread->nextCookie > CMDQ_MAX_COOKIE_VALUE) {
			/* Reach the maximum cookie */
			pThread->nextCookie = 0;
		}

		/* taskCount must start from 0. */
		/* and we are the first task, so set to 1. */
		pThread->taskCount = 1;

	} else {
		pThread->nextCookie += 1;
		if (pThread->nextCookie > CMDQ_MAX_COOKIE_VALUE) {
			/* Reach the maximum cookie */
			pThread->nextCookie = 0;
		}

		pThread->taskCount++;
	}

	/* genernal part */
	pThread->pCurTask[cookie % cmdq_core_max_task_in_thread(
		pTask->thread)] = pTask;
	pThread->allowDispatching = 1;

	/* secure path */
	if (pTask->secData.is_secure) {
		pTask->secData.waitCookie = cookie;
		pTask->secData.resetExecCnt = resetHWThread;
	}

	return 0;
}

static s32 cmdq_core_remove_task_from_thread_by_cookie(
	struct ThreadStruct *pThread, s32 index,
	enum TASK_STATE_ENUM newTaskState)
{
	struct TaskStruct *pTask = NULL;

	if (!pThread || index < 0 || index >= CMDQ_MAX_TASK_IN_THREAD) {
		CMDQ_ERR(
			"remove task from thread array, invalid param THR:0x%p task_slot:%d newTaskState:%d\n",
			pThread, index, newTaskState);
		return -EINVAL;
	}

	pTask = pThread->pCurTask[index];

	if (!pTask) {
		CMDQ_ERR("remove fail task_slot:%d on thread:%p is NULL\n",
			index, pThread);
		return -EINVAL;
	}

	if (cmdq_core_max_task_in_thread(pTask->thread) <= index) {
		CMDQ_ERR(
			"remove task from thread array, invalid index THR:0x%p task_slot:%d newTaskState:%d\n",
			pThread, index, newTaskState);
		return -EINVAL;
	}

	/* to switch a task to done_status(_ERROR, _KILLED, _DONE)
	 * is aligned with thread's taskcount change
	 * check task status to prevent double clean-up thread's taskcount
	 */
	if (pTask->taskState != TASK_STATE_BUSY) {
		CMDQ_ERR(
			"remove task taskStatus err:%d THR:0x%p task_slot:%d targetTaskStaus:%d\n",
			pTask->taskState, pThread, index, newTaskState);
		return -EINVAL;
	}

	CMDQ_VERBOSE("remove task slot:%d targetStatus:%d\n",
		index, newTaskState);
	pTask->taskState = newTaskState;
	pTask = NULL;
	pThread->pCurTask[index] = NULL;
	pThread->taskCount--;

	if (pThread->taskCount < 0) {
		/* Error status print */
		CMDQ_ERR("%s end taskCount < 0\n", __func__);
	}

	return 0;
}

static s32 cmdq_core_remove_task_from_thread_array_sec_fail(
	struct ThreadStruct *pThread, s32 index)
{
	struct TaskStruct *pTask = NULL;
	unsigned long flags = 0L;

	if (!pThread || index < 0 || index >= CMDQ_MAX_TASK_IN_THREAD) {
		CMDQ_ERR(
			"remove task from thread array invalid param. THR:0x%p task_slot:%d\n",
			pThread, index);
		return -EINVAL;
	}

	pTask = pThread->pCurTask[index];

	if (!pTask) {
		CMDQ_ERR("remove fail task_slot:%d on thread:%p is NULL\n",
			index, pThread);
		return -EINVAL;
	}

	if (cmdq_core_max_task_in_thread(pTask->thread) <= index) {
		CMDQ_ERR(
			"remove task from thread array invalid index. THR:0x%p task_slot:%d\n",
			pThread, index);
		return -EINVAL;
	}

	CMDQ_VERBOSE("remove task slot:%d\n", index);
	pTask = NULL;
	CMDQ_PROF_SPIN_LOCK(gCmdqExecLock, flags, remove_sec_fail_task);
	pThread->pCurTask[index] = NULL;
	pThread->taskCount--;
	pThread->nextCookie--;
	CMDQ_PROF_SPIN_UNLOCK(gCmdqExecLock, flags, remove_sec_fail_task_done);

	if (pThread->taskCount < 0) {
		/* Error status print */
		CMDQ_ERR("%s end taskCount < 0\n", __func__);
	}

	return 0;
}

static s32 cmdq_core_force_remove_task_from_thread(
	struct TaskStruct *pTask, u32 thread)
{
	s32 status = 0;
	s32 cookie = 0;
	int index = 0;
	int loop = 0;
	struct TaskStruct *pExecTask = NULL;
	struct ThreadStruct *pThread = NULL;
	dma_addr_t pa = 0;

	if (unlikely(pTask->use_sram_buffer)) {
		CMDQ_AEE("CMDQ", "SRAM task should not enter %s\n", __func__);
		return -EFAULT;
	}

	if (thread == CMDQ_INVALID_THREAD) {
		CMDQ_ERR("Remove inavlid task:0x%p thread:%d\n",
			pTask, thread);
		return -EFAULT;
	}

	pThread = &gCmdqContext.thread[thread];
	status = cmdq_core_suspend_HW_thread(thread, __LINE__);

	CMDQ_REG_SET32(CMDQ_THR_INST_CYCLES(thread),
		cmdq_core_get_task_timeout_cycle(pThread));

	/* The cookie of the task currently being processed */
	cookie = CMDQ_GET_COOKIE_CNT(thread) + 1;

	pExecTask = pThread->pCurTask[cookie %
		cmdq_core_max_task_in_thread(thread)];
	if (pExecTask && pExecTask == pTask) {
		/* The task is executed now, set the PC to EOC for bypass */
		pa = cmdq_core_task_get_eoc_pa(pTask);
		CMDQ_MSG("ending task:0x%p to pa:%pa\n", pTask, &pa);
		CMDQ_REG_SET32(CMDQ_THR_CURR_ADDR(thread),
			CMDQ_PHYS_TO_AREG(pa));
		cmdq_core_reset_hw_engine(pTask->engineFlag);

		pThread->pCurTask[cookie %
			cmdq_core_max_task_in_thread(thread)] = NULL;
		pTask->taskState = TASK_STATE_KILLED;
	} else {
		loop = pThread->taskCount;
		for (index = (cookie % cmdq_core_max_task_in_thread(thread));
			loop > 0; loop--, index++) {
			bool is_last_end = false;

			if (index >= cmdq_core_max_task_in_thread(thread))
				index = 0;

			pExecTask = pThread->pCurTask[index];
			if (!pExecTask)
				continue;

			is_last_end = CMDQ_IS_END_INSTR(pExecTask->pCMDEnd);

			if (is_last_end) {
				/* We reached the last task */
				break;
			} else if (pExecTask->pCMDEnd[-1] ==
				cmdq_core_task_get_first_pa(pTask)) {
				/* Fake EOC command */
				pExecTask->pCMDEnd[-1] = 0x00000001;
				pExecTask->pCMDEnd[0] = 0x40000000;

				/* Bypass the task */
				pExecTask->pCMDEnd[1] = pTask->pCMDEnd[-1];
				pExecTask->pCMDEnd[2] = pTask->pCMDEnd[0];

				index += 1;
				if (index >=
					cmdq_core_max_task_in_thread(thread))
					index = 0;

				pThread->pCurTask[index] = NULL;
				pTask->taskState = TASK_STATE_KILLED;
				status = 0;
				break;
			}
		}
	}

	return status;
}

static void cmdq_core_handle_done_with_cookie_impl(
	s32 thread, s32 value, CMDQ_TIME *pGotIRQ, const u32 cookie)
{
	struct ThreadStruct *pThread;
	s32 count;
	s32 inner;
	s32 maxTaskNUM = cmdq_core_max_task_in_thread(thread);

	pThread = &(gCmdqContext.thread[thread]);

	/* do not print excessive message for looping thread */
	if (!pThread->loopCallback) {
#ifdef CONFIG_MTK_FPGA
		/* ASYNC: debug log,
		 * use printk_sched to prevent block IRQ handler
		 */
		CMDQ_MSG("IRQ: Done, thread:%d cookie:%d\n",
		thread, cookie);
#endif
	}

	if (pThread->waitCookie <= cookie) {
		count = cookie - pThread->waitCookie + 1;
	} else if ((cookie+1) % CMDQ_MAX_COOKIE_VALUE ==
	pThread->waitCookie) {
		count = 0;
		CMDQ_MSG("IRQ: duplicated cookie: waitCookie:%d hwCookie:%d",
			pThread->waitCookie, cookie);
	} else {
		/* Counter wrapped */
		count = (CMDQ_MAX_COOKIE_VALUE - pThread->waitCookie + 1) +
			(cookie + 1);
		CMDQ_ERR(
			"IRQ: counter wrapped: waitCookie:%d hwCookie:%d count:%d",
			pThread->waitCookie, cookie, count);
	}

	for (inner = (pThread->waitCookie % maxTaskNUM); count > 0; count--,
		inner++) {
		if (inner >= maxTaskNUM)
			inner = 0;

		if (pThread->pCurTask[inner]) {
			struct TaskStruct *pTask = pThread->pCurTask[inner];

			pTask->gotIRQ = *pGotIRQ;
			pTask->irqFlag = value;
			cmdq_core_remove_task_from_thread_by_cookie(
				pThread, inner, TASK_STATE_DONE);
		}
	}

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->CMDQ_IRQ, MMPROFILE_FLAG_PULSE,
		thread, cookie);

	pThread->waitCookie = cookie + 1;
	if (pThread->waitCookie > CMDQ_MAX_COOKIE_VALUE)
		/* min cookie value is 0 */
		pThread->waitCookie -= (CMDQ_MAX_COOKIE_VALUE + 1);

	wake_up(&gCmdWaitQueue[thread]);
}

static void cmdqCoreHandleError(s32 thread, s32 value, CMDQ_TIME *pGotIRQ)
{
	struct ThreadStruct *pThread = NULL;
	struct TaskStruct *pTask = NULL;
	s32 cookie;
	s32 count;
	s32 inner;
	s32 status = 0;

	cookie = cmdq_core_thread_exec_counter(thread);

	if (cmdq_get_func()->isSecureThread(thread)) {
		CMDQ_ERR(
			"IRQ: err thread:%d flag:0x%x cookie:%d secure thread!\n",
			thread, value, cookie);
	} else {
		CMDQ_ERR(
			"IRQ: err thread:%d flag:0x%x cookie:%d PC:0x%08x END:0x%08x\n",
			thread, value, cookie,
			CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread)),
			CMDQ_REG_GET32(CMDQ_THR_END_ADDR(thread)));
	}

	pThread = &(gCmdqContext.thread[thread]);

	/* we assume error happens BEFORE EOC
	 * because it wouldn't be error if this interrupt is issue by EOC.
	 * So we should inc by 1 to locate "current" task
	 */
	cookie += 1;

	/* Set the issued task to error state */
#define CMDQ_TEST_PREFETCH_FOR_MULTIPLE_COMMAND
#ifdef CMDQ_TEST_PREFETCH_FOR_MULTIPLE_COMMAND
	cmdq_core_dump_task_in_thread(thread, true, true, true);
#endif
	/* suspend HW thread first, so that we work in a consistent state
	 * outer function should acquire spinlock - gCmdqExecLock
	 */
	if (!cmdq_get_func()->isSecureThread(thread)) {
		status = cmdq_core_suspend_HW_thread(thread, __LINE__);
		if (status < 0) {
			/* suspend HW thread failed */
			CMDQ_ERR("IRQ: suspend HW thread failed!");
		}
	}

	if (pThread->pCurTask[cookie %
		cmdq_core_max_task_in_thread(thread)]) {
		pTask = pThread->pCurTask[cookie %
			cmdq_core_max_task_in_thread(thread)];
		pTask->gotIRQ = *pGotIRQ;
		pTask->irqFlag = value;
		cmdq_core_attach_error_task_unlock(pTask, thread);
		cmdq_core_remove_task_from_thread_by_cookie(
			pThread,
			cookie % cmdq_core_max_task_in_thread(thread),
			TASK_STATE_ERR_IRQ);
	} else {
		if (pThread->taskCount <= 0) {
			cmdq_core_disable_HW_thread(thread);
			CMDQ_ERR(
				"IRQ: there is no task for thread (%d) cmdqCoreHandleError\n",
				thread);
		} else {
			CMDQ_ERR(
				"IRQ: can not find task in cmdqCoreHandleError pc:0x%08x end_pc:0x%08x\n",
				CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread)),
				CMDQ_REG_GET32(CMDQ_THR_END_ADDR(thread)));
		}
	}

	/* Set the remain tasks to done state */
	if (pThread->waitCookie <= cookie) {
		count = cookie - pThread->waitCookie + 1;
	} else if ((cookie+1) % CMDQ_MAX_COOKIE_VALUE ==
		pThread->waitCookie) {
		count = 0;
		CMDQ_MSG("IRQ: duplicated cookie: waitCookie:%d hwCookie:%d",
			pThread->waitCookie, cookie);
	} else {
		/* Counter wrapped */
		count = (CMDQ_MAX_COOKIE_VALUE - pThread->waitCookie + 1) +
			(cookie + 1);
		CMDQ_ERR(
			"IRQ: counter wrapped: waitCookie:%d hwCookie:%d count:%d",
			pThread->waitCookie, cookie, count);
	}

	for (inner = (pThread->waitCookie %
		cmdq_core_max_task_in_thread(thread)); count > 0;
		count--, inner++) {
		if (inner >= cmdq_core_max_task_in_thread(thread))
			inner = 0;

		if (pThread->pCurTask[inner]) {
			pTask = pThread->pCurTask[inner];
			pTask->gotIRQ = (*pGotIRQ);
			/* we don't know the exact irq flag. */
			pTask->irqFlag = 0;
			cmdq_core_remove_task_from_thread_by_cookie(
				pThread, inner, TASK_STATE_DONE);
		}
	}

	/* Error cookie will be handled in
	 * cmdq_core_handle_wait_task_result_impl API
	 */
#if 0
	pThread->waitCookie = cookie + 1;
	if (pThread->waitCookie > CMDQ_MAX_COOKIE_VALUE)
		pThread->waitCookie -= (CMDQ_MAX_COOKIE_VALUE + 1);
#endif

	wake_up(&gCmdWaitQueue[thread]);
}

static void cmdqCoreHandleDone(s32 thread, s32 value, CMDQ_TIME *pGotIRQ)
{
	struct ThreadStruct *pThread;
	s32 cookie;
	s32 loopResult = 0;

	pThread = &gCmdqContext.thread[thread];

	/* Loop execution never gets done; unless
	 * user loop function returns error
	 */
	if (pThread->loopCallback) {
		loopResult = pThread->loopCallback(pThread->loopData);

		CMDQ_PROF_MMP(cmdq_mmp_get_event()->loopBeat,
			MMPROFILE_FLAG_PULSE, thread, loopResult);

		if (loopResult >= 0) {
#ifdef CMDQ_PROFILE_COMMAND_TRIGGER_LOOP
			/* HACK */
			if (pThread->pCurTask[1])
				cmdq_core_track_task_record(
					pThread->pCurTask[1], thread);
#endif
			/* Success, contiue execution as if nothing happens */
			CMDQ_REG_SET32(CMDQ_THR_IRQ_STATUS(thread), ~value);
			return;
		}
	}

	if (loopResult < 0) {
		/* The loop CB failed, so stop HW thread now. */
		cmdq_core_disable_HW_thread(thread);

		/* loop CB failed. the EXECUTION count should not be
		 * used as cookie,
		 * since it will increase by each loop iteration.
		 */
		cookie = pThread->waitCookie;

	} else {
		/* task cookie */
		cookie = cmdq_core_thread_exec_counter(thread);
		CMDQ_MSG("Done: thread %d got cookie:%d\n", thread, cookie);
	}

	cmdq_core_handle_done_with_cookie_impl(thread, value, pGotIRQ, cookie);
}

void cmdqCoreHandleIRQ(s32 thread)
{
	unsigned long flags = 0L;
	CMDQ_TIME gotIRQ;
	int value;
	int enabled;
	s32 cookie;
	char thread_label[20];

	/* note that do_gettimeofday may cause HWT in
	 * spin_lock_irqsave (ALPS01496779)
	 */
	gotIRQ = sched_clock();

	snprintf(thread_label, ARRAY_SIZE(thread_label), "CMDQ_IRQ_THR_%d",
		thread);

	/* Normal execution, marks tasks done and remove from thread
	 * Also, handle "loop CB fail" case
	 */
	CMDQ_PROF_SPIN_LOCK(gCmdqExecLock, flags, handle_irq);

	/* it is possible for another CPU core
	 * to run "releaseTask" right before we acquire the spin lock
	 * and thus reset / disable this HW thread
	 * so we check both the IRQ flag and the enable bit of this thread
	 */
	value = CMDQ_REG_GET32(CMDQ_THR_IRQ_STATUS(thread));

	if ((value & 0x13) == 0) {
		CMDQ_ERR(
			"IRQ: thread %d got interrupt but IRQ flag is 0x%08x in NWd\n",
			thread, value);
		CMDQ_PROF_SPIN_UNLOCK(gCmdqExecLock, flags, handle_irq);
		return;
	}

	if (cmdq_get_func()->isSecureThread(thread) == false) {
		enabled = CMDQ_REG_GET32(CMDQ_THR_ENABLE_TASK(thread));

		if ((enabled & 0x01) == 0) {
			CMDQ_ERR(
				"IRQ: thread %d got interrupt already disabled 0x%08x\n",
				thread, enabled);
			CMDQ_PROF_SPIN_UNLOCK(gCmdqExecLock, flags,
				handle_irq_disable);
			return;
		}
	}

	CMDQ_PROF_START(0, thread_label);

	/* Read HW cookie here to print message only */
	cookie = cmdq_core_thread_exec_counter(thread);
	/* Move the reset IRQ before read HW cookie to prevent
	 * race condition and save the cost of suspend
	 */
	CMDQ_REG_SET32(CMDQ_THR_IRQ_STATUS(thread), ~value);
	CMDQ_MSG(
		"IRQ: thread %d got interrupt after reset and IRQ flag is 0x%08x cookie:%d\n",
		thread, value, cookie);

	if (value & 0x12)
		cmdqCoreHandleError(thread, value, &gotIRQ);
	else if (value & 0x01)
		cmdqCoreHandleDone(thread, value, &gotIRQ);

	CMDQ_PROF_END(0, thread_label);

	CMDQ_PROF_SPIN_UNLOCK(gCmdqExecLock, flags, handle_irq_done);
}

static struct TaskStruct *cmdq_core_search_task_by_pc(u32 threadPC,
	const struct ThreadStruct *pThread, s32 thread)
{
	struct TaskStruct *pTask = NULL;
	int i = 0;

	for (i = 0; i < cmdq_core_max_task_in_thread(thread); ++i) {
		if (pThread->pCurTask[i] &&
			cmdq_core_task_is_valid_pa(pThread->pCurTask[i],
				threadPC)) {
			pTask = pThread->pCurTask[i];
			break;
		}
	}

	return pTask;
}

static void cmdq_core_dump_delay_spr(s32 delay_id, s32 tpr_mask)
{
	/* TODO: add mechanism to detect if current
	 * thread is delaying
	 */
	u32 delay_event = CMDQ_SYNC_TOKEN_DELAY_SET0 + delay_id;

	cmdq_core_dump_thread(CMDQ_DELAY_THREAD_ID, "INFO");
	cmdq_core_dump_thread_pc(CMDQ_DELAY_THREAD_ID);
	CMDQ_LOG(
		"TPR_MASK:0x%08x event#:%d value:%d SPR:0x%08x 0x%08x 0x%08x 0x%08x\n",
		tpr_mask, delay_event,
		cmdqCoreGetEvent(delay_event),
		CMDQ_REG_GET32(CMDQ_THR_SPR0(CMDQ_DELAY_THREAD_ID)),
		CMDQ_REG_GET32(CMDQ_THR_SPR1(CMDQ_DELAY_THREAD_ID)),
		CMDQ_REG_GET32(CMDQ_THR_SPR2(CMDQ_DELAY_THREAD_ID)),
		CMDQ_REG_GET32(CMDQ_THR_SPR3(CMDQ_DELAY_THREAD_ID)));
}

/* Implementation of wait task done
 * Return:
 *     wait time of wait_event_timeout() kernel API
 *     . =0, for timeout elapsed,
 *     . >0, remain jiffies if condition passed
 *
 * Note process will go to sleep with state TASK_UNINTERRUPTIBLE until
 * the condition[task done] passed or timeout happened.
 */
static s32 cmdq_core_wait_task_done_with_timeout_impl(
	struct TaskStruct *pTask, s32 thread, u32 timeout_ms)
{
	s32 waitq = 0;
	unsigned long flags;
	struct ThreadStruct *pThread = NULL;
	s32 retry_count = 0;
	s32 delay_id = cmdq_get_delay_id_by_scenario(pTask->scenario);
	s32 slot = -1, i = 0;
	u32 tpr_mask = 0;

	pThread = &(gCmdqContext.thread[thread]);
	retry_count = 0;

	/* if SW-timeout, pre-dump hang instructions */
	while (timeout_ms > 0) {
		const u32 wait_time_ms = timeout_ms <=
			CMDQ_PREDUMP_TIMEOUT_MS ?
			timeout_ms : CMDQ_PREDUMP_TIMEOUT_MS;

		/* timeout wait & make sure this task is finished.
		 * pTask->taskState flag is updated in IRQ handlers
		 * like cmdqCoreHandleDone.
		 */
		waitq = wait_event_timeout(gCmdWaitQueue[thread],
			(pTask->taskState != TASK_STATE_BUSY
			&& pTask->taskState != TASK_STATE_WAITING),
			msecs_to_jiffies(wait_time_ms));
		if (waitq)
			break;
		timeout_ms -= wait_time_ms;

		if (slot < 0 && pTask->thread != CMDQ_INVALID_THREAD) {
			for (i = 0; i < cmdq_core_max_task_in_thread(thread);
				i++) {
				if (gCmdqContext.thread[thread].pCurTask[i] ==
					pTask) {
					slot = i;
					break;
				}
			}
		}

		CMDQ_LOG(
			"======= [CMDQ] SW timeout Pre-dump(%d) task:0x%p slot:%d =======\n",
			retry_count, pTask, slot);

		tpr_mask = CMDQ_REG_GET32(CMDQ_TPR_MASK);
		if (retry_count == 0) {
			/* print detail log in first time */
			CMDQ_PROF_SPIN_LOCK(gCmdqExecLock, flags,
				wait_task_dump);
			cmdq_core_dump_status("INFO");
			cmdq_core_dump_pc(pTask, thread, "INFO");

			cmdq_core_dump_thread(thread, "INFO");
			CMDQ_LOG(
				"Dump thread SPR:0x%08x 0x%08x 0x%08x 0x%08x\n",
				CMDQ_REG_GET32(CMDQ_THR_SPR0(thread)),
				CMDQ_REG_GET32(CMDQ_THR_SPR1(thread)),
				CMDQ_REG_GET32(CMDQ_THR_SPR2(thread)),
				CMDQ_REG_GET32(CMDQ_THR_SPR3(thread)));

			/* HACK: check trigger thread status */
			cmdq_core_dump_disp_trigger_loop("INFO");
			/* end of HACK */
			CMDQ_PROF_SPIN_UNLOCK(gCmdqExecLock, flags,
				wait_task_dump);

			if (delay_id >= 0 && tpr_mask)
				cmdq_core_dump_delay_spr(delay_id, tpr_mask);
		} else {
			/* dump simple status only */
			CMDQ_PROF_SPIN_LOCK(gCmdqExecLock, flags,
				wait_task_dump);
			cmdq_core_dump_pc(pTask, thread, "INFO");
			cmdq_core_dump_disp_trigger_loop("INFO");
			CMDQ_PROF_SPIN_UNLOCK(gCmdqExecLock, flags,
				wait_task_dump);
			if (delay_id >= 0 && tpr_mask)
				cmdq_core_dump_thread_pc(CMDQ_DELAY_THREAD_ID);
		}

		retry_count++;
	}

	return waitq;
}

bool cmdq_core_check_task_finished(struct TaskStruct *pTask)
{
#if defined(CMDQ_SECURE_PATH_NORMAL_IRQ) || defined(CMDQ_SECURE_PATH_HW_LOCK)
	return (pTask->taskState == TASK_STATE_DONE);
#else
	return (pTask->taskState != TASK_STATE_BUSY);
#endif
}

static s32 cmdq_core_handle_wait_task_result_secure_impl(
	struct TaskStruct *pTask, s32 thread, const s32 waitQ)
{
	s32 i;
	s32 status;
	struct ThreadStruct *pThread = NULL;
	/* error report */
	bool throwAEE = false;
	const char *module = NULL;
	s32 irqFlag = 0;
	struct cmdqSecCancelTaskResultStruct result;
	char parsedInstruction[128] = { 0 };

	/* Init default status */
	status = 0;
	pThread = &(gCmdqContext.thread[thread]);
	memset(&result, 0, sizeof(struct cmdqSecCancelTaskResultStruct));

	/* lock cmdqSecLock */
	cmdq_sec_lock_secure_path();

	do {
		s32 start_sec_thread = CMDQ_MIN_SECURE_THREAD_ID;
		struct TaskPrivateStruct *private = CMDQ_TASK_PRIVATE(pTask);
		unsigned long flags = 0L;

		/* check if this task has finished */
		if (cmdq_core_check_task_finished(pTask))
			break;

		/* Oops, tha tasks is not done.
		 * We have several possible error scenario:
		 * 1. task still running (hang / timeout)
		 * 2. IRQ pending (done or error/timeout IRQ)
		 * 3. task's SW thread has been signaled (e.g. SIGKILL)
		 */

		/* dump shared cookie */
		CMDQ_LOG(
			"WAIT: [1]secure path failed, pTask:%p, thread:%d, shared_cookie(%d, %d, %d)\n",
			pTask, thread,
			cmdq_core_get_secure_thread_exec_counter(
				start_sec_thread),
			cmdq_core_get_secure_thread_exec_counter(
				start_sec_thread + 1),
			cmdq_core_get_secure_thread_exec_counter(
				start_sec_thread + 2));

		/* suppose that task failed, entry secure world to confirm it
		 * we entry secure world:
		 * .check if pending IRQ and update cookie value
		 *  to shared memory
		 * .confirm if task execute done
		 * if not, do error handle
		 *     .recover M4U & DAPC setting
		 *     .dump secure HW thread
		 *     .reset CMDQ secure HW thread
		 */
		cmdq_sec_cancel_error_task_unlocked(pTask, thread, &result);

		status = -ETIMEDOUT;
		throwAEE = !(private && private->internal &&
			private->ignore_timeout);

		/* shall we pass the error instru back from secure path?? */

		/* cmdq_core_parse_error(pTask, thread, &module, &irqFlag,
		 *	&instA, &instB);
		 */
		module = cmdq_get_func()->parseErrorModule(pTask);

		/* module dump */
		cmdq_core_attach_error_task(pTask, thread);

		/* module reset */
		/* TODO: get needReset infor by secure thread PC */
		cmdq_core_reset_hw_engine(pTask->engineFlag);

		CMDQ_PROF_SPIN_LOCK(gCmdqExecLock, flags, cancel_exec_task);
		/* remove all tasks in tread since reset HW thread in SWd */
		for (i = 0; i < cmdq_core_max_task_in_thread(thread); i++) {
			pTask = pThread->pCurTask[i];
			if (!pTask)
				continue;
			cmdq_core_remove_task_from_thread_by_cookie(
				pThread, i, TASK_STATE_ERROR);
		}
		pThread->taskCount = 0;
		pThread->waitCookie = pThread->nextCookie;
		CMDQ_PROF_SPIN_UNLOCK(gCmdqExecLock, flags,
			cancel_exec_task_done);
	} while (0);

	/* unlock cmdqSecLock */
	cmdq_sec_unlock_secure_path();

	/* throw AEE if nessary */
	if (throwAEE) {
		const u32 instA = result.errInstr[1];
		const u32 instB = result.errInstr[0];
		const u32 op = (instA & 0xFF000000) >> 24;

		cmdq_core_interpret_instruction(parsedInstruction,
			sizeof(parsedInstruction), op,
			instA & (~0xFF000000), instB);

		CMDQ_AEE(module,
			"%s in CMDQ IRQ:0x%02x INST:(0x%08x, 0x%08x) OP:%s => %s\n",
			module, irqFlag, instA, instB,
			cmdq_core_parse_op(op), parsedInstruction);
	}

	return status;
}

static void cmdq_core_change_prev_jump(
	struct TaskStruct *task, struct TaskStruct *next_task,
	s32 thread_id)
{
	u32 idx;
	struct TaskStruct *prev_task = NULL;
	struct ThreadStruct *thread = &gCmdqContext.thread[thread_id];

	for (idx = 0; idx < cmdq_core_max_task_in_thread(thread_id); idx++) {
		struct TaskStruct *tsk = thread->pCurTask[idx];

		/* find which task JUMP into task */
		if (tsk && tsk->pCMDEnd && tsk->pCMDEnd[-1] ==
			cmdq_core_task_get_first_pa(task) &&
			tsk->pCMDEnd[0] == ((CMDQ_CODE_JUMP << 24) | 0x1)) {
			prev_task = tsk;
			break;
		}
	}

	if (!prev_task)
		return;

	/* Copy Jump instruction */
	prev_task->pCMDEnd[-1] = task->pCMDEnd[-1];
	prev_task->pCMDEnd[0] = task->pCMDEnd[0];
	if (CMDQ_IS_END_INSTR(task->pCMDEnd)) {
		/* reset end if pTask is last one */
		dma_addr_t instr = cmdq_core_task_get_final_instr(prev_task);
		u32 end = CMDQ_PHYS_TO_AREG(instr - CMDQ_INST_SIZE);

		CMDQ_REG_SET32(CMDQ_THR_END_ADDR(thread_id), end);
		CMDQ_LOG("Reset End Addr to:0x%08x\n", end);
	}
	if (next_task)
		cmdq_core_reorder_task_array(thread, thread_id, idx);
	else
		thread->nextCookie--;

	CMDQ_LOG("WAIT: modify jump to [0x%08x 0x%08x] prev:0x%p task:0x%p\n",
	     task->pCMDEnd[-1], task->pCMDEnd[0], prev_task, task);

	/* Give up fetched command,
	 * invoke CMDQ HW to re-fetch command buffer again.
	 */
	cmdq_core_invalidate_hw_fetched_buffer(thread_id);
}

static void cmdq_core_adjust_hw_thread(s32 thread_id,
	struct TaskStruct *task)
{
	struct ThreadStruct *thread = &gCmdqContext.thread[thread_id];

	/* Reset GCE thread when task state is ERROR or KILL */
	u32 pc, end, cookie_cnt;
	s32 thread_pri;
	u32 spr0, spr1, spr2, spr3;

	if (task->taskState == TASK_STATE_DONE)
		return;

	/* Backup PC, End address, and GCE cookie count
	 * before reset GCE thread
	 */
	pc = CMDQ_AREG_TO_PHYS(CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread_id)));
	end = CMDQ_AREG_TO_PHYS(CMDQ_REG_GET32(CMDQ_THR_END_ADDR(thread_id)));
	cookie_cnt = CMDQ_GET_COOKIE_CNT(thread_id);

	spr0 = CMDQ_REG_GET32(CMDQ_THR_SPR0(thread_id));
	spr1 = CMDQ_REG_GET32(CMDQ_THR_SPR1(thread_id));
	spr2 = CMDQ_REG_GET32(CMDQ_THR_SPR2(thread_id));
	spr3 = CMDQ_REG_GET32(CMDQ_THR_SPR3(thread_id));

	CMDQ_LOG(
		"Reset Backup Thread PC:0x%08x End:0x%08x CookieCnt:0x%08x SPR:0x%08x 0x%08x 0x%08x 0x%08x\n",
		pc, end, cookie_cnt, spr0, spr1, spr2, spr3);
	/* Reset GCE thread */
	if (cmdq_core_reset_HW_thread(thread_id) < 0)
		CMDQ_ERR("reset hardware thread:%d fail but keep restore\n",
			thread_id);

	CMDQ_REG_SET32(CMDQ_THR_SPR0(thread_id), spr0);
	CMDQ_REG_SET32(CMDQ_THR_SPR1(thread_id), spr1);
	CMDQ_REG_SET32(CMDQ_THR_SPR2(thread_id), spr2);
	CMDQ_REG_SET32(CMDQ_THR_SPR3(thread_id), spr3);

	CMDQ_REG_SET32(CMDQ_THR_INST_CYCLES(thread_id),
		cmdq_core_get_task_timeout_cycle(thread));
	/* Set PC & End address */
	CMDQ_REG_SET32(CMDQ_THR_END_ADDR(thread_id), CMDQ_PHYS_TO_AREG(end));
	CMDQ_REG_SET32(CMDQ_THR_CURR_ADDR(thread_id), CMDQ_PHYS_TO_AREG(pc));
	/* bit 0-2 for priority level; */
	thread_pri = cmdq_get_func()->priority(task->scenario);
	CMDQ_MSG("RESET HW THREAD: set HW thread:%d qos:%d\n",
		thread_id, thread_pri);
	CMDQ_REG_SET32(CMDQ_THR_CFG(thread_id), thread_pri & 0x7);
	/* For loop thread, do not enable timeout */
	CMDQ_REG_SET32(CMDQ_THR_IRQ_ENABLE(thread_id),
		thread->loopCallback ? 0x011 : 0x013);
	if (thread->loopCallback) {
		CMDQ_MSG("RESET HW THREAD: HW thread:%d in loop func 0x%p\n",
			 thread_id, thread->loopCallback);
	}
	/* Set GCE cookie count */
	CMDQ_REG_SET32(CMDQ_THR_EXEC_CNT(thread_id), cookie_cnt);
	/* Enable HW thread */
	CMDQ_REG_SET32(CMDQ_THR_ENABLE_TASK(thread_id), 0x01);
}

static s32 cmdq_core_handle_wait_task_result_impl(
	struct TaskStruct *pTask, s32 thread, const s32 waitQ)
{
	s32 status;
	s32 index;
	unsigned long flags;
	struct ThreadStruct *pThread = NULL;
	const struct TaskStruct *pNGTask = NULL;
	bool markAsErrorTask = false;
	/* error report */
	bool throwAEE = false;
	const char *module = NULL;
	u32 instA = 0, instB = 0;
	s32 irqFlag = 0, timeoutIrqFlag = 0;
	dma_addr_t task_pa = 0;

	if (unlikely(pTask->use_sram_buffer)) {
		CMDQ_AEE(
			"CMDQ", "SRAM task is always loop, should not wait\n");
		return -EFAULT;
	}

	/* Init default status */
	status = 0;
	pThread = &(gCmdqContext.thread[thread]);

	if (waitQ == 0) {
		/* print log before entering exec lock in timeout case */
		cmdq_core_attach_error_task_detail(pTask, thread,
			&pNGTask, &module,
			&timeoutIrqFlag, &instA, &instB);
	}

	/* Note that although we disable IRQ, HW continues to execute
	 * so it's possible to have pending IRQ
	 */
	CMDQ_PROF_SPIN_LOCK(gCmdqExecLock, flags, wait_task);

	do {
		struct TaskStruct *pNextTask = NULL;
		s32 cookie = 0;
		long threadPC = 0L;
		struct TaskPrivateStruct *private = CMDQ_TASK_PRIVATE(pTask);

		status = 0;
		throwAEE = false;
		markAsErrorTask = false;

		if (pTask->taskState == TASK_STATE_DONE)
			break;

		CMDQ_ERR("Task state of %p is not TASK_STATE_DONE, %d\n",
			pTask, pTask->taskState);

		/* Oops, tha tasks is not done.
		 * We have several possible error scenario:
		 * 1. task still running (hang / timeout)
		 * 2. IRQ pending (done or error/timeout IRQ)
		 * 3. task's SW thread has been signaled (e.g. SIGKILL)
		 */

		/* suspend HW thread first, to work in consistent state */
		status = cmdq_core_suspend_HW_thread(thread, __LINE__);
		if (status < 0)
			throwAEE = true;

		/* The cookie of the task currently being processed */
		cookie = CMDQ_GET_COOKIE_CNT(thread) + 1;
		threadPC = CMDQ_AREG_TO_PHYS(CMDQ_REG_GET32(
			CMDQ_THR_CURR_ADDR(thread)));

		/* process any pending IRQ
		 * TODO: provide no spin lock version
		 * because we already locked.
		 */
		irqFlag = CMDQ_REG_GET32(CMDQ_THR_IRQ_STATUS(thread));
		if (irqFlag & 0x12)
			cmdqCoreHandleError(thread, irqFlag, &pTask->wakedUp);
		else if (irqFlag & 0x01)
			cmdqCoreHandleDone(thread, irqFlag, &pTask->wakedUp);

		CMDQ_REG_SET32(CMDQ_THR_IRQ_STATUS(thread), ~irqFlag);

		/* check if task has finished after handling pending IRQ */
		if (pTask->taskState == TASK_STATE_DONE)
			break;

		/* Then decide we are SW timeout or SIGNALed (not an error) */
		if (waitQ == 0) {
			/* SW timeout and no IRQ received */
			markAsErrorTask = true;

			/* if we reach here, we're in errornous state. */
			CMDQ_ERR("SW timeout of task 0x%p on thread %d\n",
			pTask, thread);
			if (pTask != pNGTask) {
				CMDQ_ERR(
					"   But pc stays in task 0x%p on thread %d\n",
					pNGTask, thread);
			}

			status = -ETIMEDOUT;
			throwAEE = !(private && private->internal &&
				private->ignore_timeout);
		} else if (waitQ < 0) {
			/* Task be killed.
			 * Not an error, but still need removal.
			 */

			markAsErrorTask = false;

			if (-ERESTARTSYS == waitQ) {
				/* Error status print */
				CMDQ_ERR(
					"Task %p KILLED by waitQ = -ERESTARTSYS\n",
					pTask);
			} else if (-EINTR == waitQ) {
				/* Error status print */
				CMDQ_ERR("Task %p KILLED by waitQ = -EINTR\n",
					pTask);
			} else {
				/* Error status print */
				CMDQ_ERR("Task %p KILLED by waitQ = %d\n",
					pTask, waitQ);
			}

			status = waitQ;
		}

		/* reset HW engine immediately if we already got error IRQ. */
		if ((pTask->taskState == TASK_STATE_ERROR) ||
		    (pTask->taskState == TASK_STATE_ERR_IRQ)) {
			cmdq_core_reset_hw_engine(pTask->engineFlag);
			CMDQ_MSG("WAIT: task state is error, reset engine\n");
		} else if (pTask->taskState == TASK_STATE_BUSY) {
			/* if taskState is BUSY, this means we did not
			 * reach EOC, did not have error IRQ.
			 * - remove the task from thread.pCurTask[]
			 * - and decrease thread.taskCount
			 * NOTE: after this, the pCurTask will not contain
			 * link to pTask anymore.
			 * and pTask should become TASK_STATE_ERROR
			 */

			/* we find our place in pThread->pCurTask[]. */
			for (index = 0; index < cmdq_core_max_task_in_thread(
				thread); index++) {
				if (pThread->pCurTask[index] != pTask)
					continue;

				/* update taskCount and pCurTask[] */
				cmdq_core_remove_task_from_thread_by_cookie(
					pThread, index,
					markAsErrorTask	? TASK_STATE_ERROR :
					TASK_STATE_KILLED);
				break;
			}
		}

		if (!pTask->pCMDEnd)
			break;

		pNextTask = NULL;
		/* find pTask's jump destination */
		if (!CMDQ_IS_END_INSTR(pTask->pCMDEnd)) {
			pNextTask = cmdq_core_search_task_by_pc(
				pTask->pCMDEnd[-1], pThread, thread);
		} else {
			CMDQ_MSG(
				"No next task: LAST instruction : (0x%08x, 0x%08x)\n",
				 pTask->pCMDEnd[0], pTask->pCMDEnd[-1]);
		}

		/* Then, remove pTask from the chain of pThread->pCurTask.
		 * . if HW PC falls in pTask range
		 * . HW EXEC_CNT += 1
		 * . thread.waitCookie += 1
		 * . set HW PC to next task head
		 * . if not, find previous task
		 *   (whose jump address is pTask->MVABase)
		 * . check if HW PC points is not at the EOC/JUMP end
		 * . change jump to fake EOC(no IRQ)
		 * . insert jump to next task head
		 *   and increase cmd buffer size
		 * . if there is no next task, set HW End Address
		 */
		if (cmdq_core_task_is_valid_pa(pTask, threadPC)) {
			if (pNextTask) {
				/* cookie already +1 */
				CMDQ_REG_SET32(CMDQ_THR_EXEC_CNT(thread),
					cookie);
				pThread->waitCookie = cookie + 1;
				task_pa = cmdq_core_task_get_first_pa(
					pNextTask);
				CMDQ_REG_SET32(CMDQ_THR_CURR_ADDR(thread),
					CMDQ_PHYS_TO_AREG(task_pa));
				CMDQ_MSG(
					"WAIT: resume task:0x%p from err, pa:%pa\n",
					pNextTask, &task_pa);
			}
		} else if (pTask->taskState == TASK_STATE_ERR_IRQ) {
			/* Error IRQ might not stay in normal Task range
			 * (jump to a strange part)
			 * We always execute next due to error IRQ
			 * must correct task
			 */
			if (pNextTask) {
				/* cookie already +1 */
				CMDQ_REG_SET32(CMDQ_THR_EXEC_CNT(thread),
					cookie);
				pThread->waitCookie = cookie + 1;
				task_pa = cmdq_core_task_get_first_pa(
					pNextTask);
				CMDQ_REG_SET32(CMDQ_THR_CURR_ADDR(thread),
					CMDQ_PHYS_TO_AREG(task_pa));
				CMDQ_MSG(
					"WAIT: resume task:0x%p from err IRQ, pa:%pa\n",
					pNextTask, &task_pa);
			}
		} else {
			cmdq_core_change_prev_jump(pTask, pNextTask, thread);
		}
	} while (0);

	if (pThread->taskCount <= 0) {
		/* no other task, stop thread */
		cmdq_core_disable_HW_thread(thread);
	} else {
		cmdq_core_adjust_hw_thread(thread, pTask);
		cmdq_core_resume_HW_thread(thread);
	}

	CMDQ_PROF_SPIN_UNLOCK(gCmdqExecLock, flags, wait_task_done);

	if (throwAEE) {
		const u32 op = (instA & 0xFF000000) >> 24;

		switch (op) {
		case CMDQ_CODE_WFE:
			CMDQ_AEE(module,
				"%s in CMDQ IRQ:0x%02x, INST:(0x%08x, 0x%08x), OP:WAIT EVENT:%s\n",
				module, timeoutIrqFlag, instA, instB,
				cmdq_core_get_event_name(instA & 0xFFFFFF));
			break;
		default:
			CMDQ_AEE(module,
				"%s in CMDQ IRQ:0x%02x, INST:(0x%08x, 0x%08x), OP:%s\n",
				 module, irqFlag, instA, instB,
				 cmdq_core_parse_op(op));
			break;
		}
	}

	return status;
}

static s32 cmdq_core_wait_task_done(struct TaskStruct *pTask, u32 timeout_ms)
{
	s32 waitQ;
	s32 status;
	u32 thread;
	struct ThreadStruct *pThread = NULL;
	const u32 max_thread_count = cmdq_dev_get_thread_count();

	status = 0;		/* Default status */

	thread = pTask->thread;
	if (thread == CMDQ_INVALID_THREAD) {
		CMDQ_PROF_MMP(cmdq_mmp_get_event()->wait_thread,
			MMPROFILE_FLAG_PULSE, ((unsigned long)pTask), -1);

		CMDQ_PROF_START(current->pid, "wait_for_thread");

		CMDQ_LOG("pid:%d task:0x%p wait for valid thread first\n",
			current->pid, pTask);

		/* wait for acquire thread
		 * (this is done by cmdq_core_consume_waiting_list);
		 */
		waitQ = wait_event_timeout(gCmdqThreadDispatchQueue,
			(pTask->thread != CMDQ_INVALID_THREAD),
			msecs_to_jiffies(CMDQ_ACQUIRE_THREAD_TIMEOUT_MS));

		CMDQ_PROF_END(current->pid, "wait_for_thread");
		if (waitQ == 0 || pTask->thread == CMDQ_INVALID_THREAD) {
			CMDQ_PROF_MUTEX_LOCK(gCmdqTaskMutex, wait_thread_done);
			/* it's possible that task was just consumed now.
			 * so check again.
			 */
			if (pTask->thread == CMDQ_INVALID_THREAD) {
				struct TaskPrivateStruct *private =
					CMDQ_TASK_PRIVATE(pTask);

				/* task may already released
				 * or starved to death
				 */
				CMDQ_ERR(
					"task 0x%p timeout with invalid thread\n",
					pTask);
				cmdq_core_dump_task(pTask);
				/* remove from waiting list,
				 * so that it won't be consumed in the future
				 */
				list_del_init(&(pTask->listEntry));

				if (private && private->internal)
					private->ignore_timeout = true;

				CMDQ_PROF_MUTEX_UNLOCK(gCmdqTaskMutex,
					wait_thread_invalid);
				return -ETIMEDOUT;
			}

			/* valid thread, so we keep going */
			CMDQ_PROF_MUTEX_UNLOCK(gCmdqTaskMutex,
				wait_thread_done);
		}
	}

	/* double confim if it get a valid thread */
	thread = pTask->thread;
	if ((thread < 0) || (thread >= max_thread_count)) {
		CMDQ_ERR("invalid thread %d in %s\n", thread, __func__);
		return -EINVAL;
	}

	pThread = &(gCmdqContext.thread[thread]);

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->wait_task,
		      MMPROFILE_FLAG_PULSE, ((unsigned long)pTask), thread);

	CMDQ_PROF_START(current->pid, "wait_for_task_done");

	/* start to wait */
	pTask->beginWait = sched_clock();
	CMDQ_MSG("-->WAIT: task 0x%p on thread %d timeout:%u(ms) begin\n",
		pTask, thread, timeout_ms);
	waitQ = cmdq_core_wait_task_done_with_timeout_impl(pTask, thread,
		timeout_ms);

	/* wake up!
	 * so the maximum total waiting time would be
	 * CMDQ_PREDUMP_TIMEOUT_MS * CMDQ_PREDUMP_RETRY_COUNT
	 */
	pTask->wakedUp = sched_clock();
	CMDQ_MSG("WAIT: task 0x%p waitq=%d state=%d\n",
		pTask, waitQ, pTask->taskState);
	CMDQ_PROF_END(current->pid, "wait_for_task_done");
	if (pTask->profileMarker.hSlot) {
		u32 i, msg_offset, value_now, value_pre = 0, duration = 0;
		s32 msg_max_size;
		char long_msg[CMDQ_LONGSTRING_MAX];

		cmdq_long_string_init(true, long_msg, &msg_offset,
			&msg_max_size);
		for (i = 0; i < pTask->profileMarker.count; i++) {
			/* timestamp, each count is 38ns */
			cmdq_cpu_read_mem((cmdqBackupSlotHandle)(
				pTask->profileMarker.hSlot), i, &value_now);
			if (i > 0) {
				cmdq_long_string(long_msg, &msg_offset,
					&msg_max_size, " duration[%d]us ",
					(value_now - value_pre)*38/1000);
				if (!duration)
					duration = (value_now - value_pre) *
						38 / 1000;
			}
			value_pre = value_now;
			cmdq_long_string(long_msg, &msg_offset, &msg_max_size,
				   "tag[%s]", (char *)(CMDQ_U32_PTR(
				   pTask->profileMarker.tag[i])));
		}
		CMDQ_LOG("task 0x%p: Profile:%s\n", pTask, long_msg);
		CMDQ_PROF_MMP(cmdq_mmp_get_event()->wait_task_done,
			MMPROFILE_FLAG_PULSE, ((unsigned long)pTask),
			duration);
	} else {
		CMDQ_PROF_MMP(cmdq_mmp_get_event()->wait_task_done,
			MMPROFILE_FLAG_PULSE, ((unsigned long)pTask),
			pTask->wakedUp - pTask->beginWait);
	}

	status = !pTask->secData.is_secure ?
		cmdq_core_handle_wait_task_result_impl(pTask, thread, waitQ) :
		cmdq_core_handle_wait_task_result_secure_impl(pTask, thread,
		waitQ);

	CMDQ_MSG("<--WAIT: task 0x%p on thread %d end\n", pTask, thread);

	return status;
}

static s32 cmdq_core_exec_task_async_secure_impl(
	struct TaskStruct *pTask, s32 thread)
{
	s32 status;
	struct ThreadStruct *pThread;
	s32 cookie;
	char long_msg[CMDQ_LONGSTRING_MAX];
	u32 msg_offset;
	s32 msg_max_size;
	u32 *pVABase = NULL;
	dma_addr_t MVABase = 0;
	unsigned long flags = 0L;

	cmdq_long_string_init(false, long_msg, &msg_offset, &msg_max_size);
	cmdq_core_get_task_first_buffer(pTask, &pVABase, &MVABase);
	cmdq_long_string(long_msg, &msg_offset, &msg_max_size,
		   "-->EXEC: task:0x%p on thread:%d begin va:0x%p",
		   pTask, thread, pVABase);
	cmdq_long_string(long_msg, &msg_offset, &msg_max_size,
		   " pa:%pa command size:%d bufferSize:%d scenario:%d flag:0x%llx\n",
		   &MVABase, pTask->commandSize, pTask->bufferSize,
		   pTask->scenario, pTask->engineFlag);
	CMDQ_MSG("%s", long_msg);

	status = 0;
	pThread = &gCmdqContext.thread[thread];

	cmdq_sec_lock_secure_path();

	do {
		/* setup whole patah */
		status = cmdq_sec_allocate_path_resource_unlocked(true);
		if (status < 0)
			break;

		/* update task's thread info */
		pTask->thread = thread;
		pTask->irqFlag = 0;
		pTask->taskState = TASK_STATE_BUSY;

		CMDQ_PROF_SPIN_LOCK(gCmdqExecLock, flags, exec_sec_task);
		/* insert task to pThread's task lsit, and */
		/* delay HW config when entry SWd */
		if (pThread->taskCount <= 0) {
			cookie = 1;
			cmdq_core_insert_task_from_thread_array_by_cookie(
				pTask, pThread, cookie, true);
		} else {
			/* append directly */
			cookie = pThread->nextCookie;
			cmdq_core_insert_task_from_thread_array_by_cookie(
				pTask, pThread, cookie, false);
		}
		CMDQ_PROF_SPIN_UNLOCK(gCmdqExecLock, flags,
			exec_sec_task_done);

		pTask->trigger = sched_clock();

		/* execute */
		status = cmdq_sec_exec_task_async_unlocked(pTask, thread);

		if (status < 0) {
			/* config failed case, dump for more detail */
			cmdq_core_attach_error_task(pTask, thread);
			cmdq_core_turnoff_first_dump();
			cmdq_core_remove_task_from_thread_array_sec_fail(
				pThread, cookie);
		}
	} while (0);

	cmdq_sec_unlock_secure_path();

	return status;
}

static inline s32 cmdq_core_exec_find_task_slot(
	struct TaskStruct **pLast, struct TaskStruct *pTask,
	s32 thread, s32 loop)
{
	s32 status = 0;
	struct ThreadStruct *pThread;
	struct TaskStruct *pPrev;
	s32 index;
	s32 prev;
	s32 cookie;
	dma_addr_t task_pa = 0;

	pThread = &(gCmdqContext.thread[thread]);
	cookie = pThread->nextCookie;

	/* Traverse forward to adjust tasks' order
	 * according to their priorities
	 */
	for (prev = (cookie % cmdq_core_max_task_in_thread(thread));
		loop > 0; loop--) {
		index = prev;
		if (index < 0)
			index = cmdq_core_max_task_in_thread(thread) - 1;

		prev = index - 1;
		if (prev < 0)
			prev = cmdq_core_max_task_in_thread(thread) - 1;

		pPrev = pThread->pCurTask[prev];

		/* Maybe the job is killed, search a new one */
		while (!pPrev && loop > 1) {
			CMDQ_LOG("pPrev is NULL prev:%d loop:%d index:%d\n",
				prev, loop, index);
			prev = prev - 1;
			if (prev < 0)
				prev = cmdq_core_max_task_in_thread(thread)
					- 1;

			pPrev = pThread->pCurTask[prev];
			loop--;
		}

		if (!pPrev) {
			cmdq_core_attach_error_task_unlock(pTask, thread);
			CMDQ_ERR("Invalid task state for reorder %d %d\n",
				index, loop);
			status = -EFAULT;
			break;
		}

		if (loop <= 1) {
			u32 prev_task_pa = cmdq_core_task_get_first_pa(pPrev);

			task_pa = cmdq_core_task_get_first_pa(pTask);

			CMDQ_MSG(
				"Set current:%d order for new task:0x%p org pc:%pa inst:0x%08x:%08x jump to:%pa\n",
				index, pPrev, &prev_task_pa,
				pPrev->pCMDEnd[0], pPrev->pCMDEnd[-1],
				&task_pa);

			pThread->pCurTask[index] = pTask;
			/* Jump: Absolute */
			pPrev->pCMDEnd[0] = ((CMDQ_CODE_JUMP << 24) | 0x1);
			/* Jump to here */
			pPrev->pCMDEnd[-1] = task_pa;

			/* Task reordered to first one, next to running task.
			 * Invalid thread to force read instructions again
			 * since we change jump.
			 */
			if (*pLast != pTask)
				cmdq_core_invalid_hw_thread(thread);
			break;
		}

		if (pPrev->priority < pTask->priority) {

			task_pa = cmdq_core_task_get_first_pa(pPrev);
			CMDQ_LOG(
				"Switch prev(%d, 0x%p 0x%08x) and curr(%d, 0x%p 0x%08x) order loop:%d jump:%pa\n",
				prev, pPrev, pPrev->pCMDEnd[-1],
				index, pTask, pTask->pCMDEnd[-1],
				loop, &task_pa);

			pThread->pCurTask[index] = pPrev;
			pPrev->pCMDEnd[0] = pTask->pCMDEnd[0];
			pPrev->pCMDEnd[-1] = pTask->pCMDEnd[-1];

			/* Boot priority for the task */
			pPrev->priority += CMDQ_MIN_AGE_VALUE;
			pPrev->reorder++;

			pThread->pCurTask[prev] = pTask;
			/* Jump: Absolute */
			pTask->pCMDEnd[0] = (CMDQ_CODE_JUMP << 24 | 0x1);
			/* Jump to here */
			pTask->pCMDEnd[-1] = task_pa;

			if (*pLast == pTask) {
				CMDQ_LOG(
					"update pLast from 0x%p (0x%08x) to 0x%p (0x%08x)\n",
					pTask, pTask->pCMDEnd[-1],
					pPrev, pPrev->pCMDEnd[-1]);
				*pLast = pPrev;
			}
		} else {
			task_pa = cmdq_core_task_get_first_pa(pPrev);

			CMDQ_MSG(
				"Set current:%d order for new task, org PC:0x%p pa:%pa inst:0x%08x:%08x",
				index, pPrev, &task_pa,
				pPrev->pCMDEnd[0], pPrev->pCMDEnd[-1]);

			pThread->pCurTask[index] = pTask;
			/* Jump: Absolute */
			pPrev->pCMDEnd[0] = (CMDQ_CODE_JUMP << 24 | 0x1);
			/* Jump to here */
			task_pa = cmdq_core_task_get_first_pa(pTask);
			pPrev->pCMDEnd[-1] = task_pa;
			break;
		}
	}

	CMDQ_MSG("Reorder %d tasks for performance end, pLast:0x%p\n",
		loop, *pLast);

	return status;
}

static s32 cmdq_core_exec_task_async_impl(
	struct TaskStruct *pTask, s32 thread)
{
	s32 status;
	struct ThreadStruct *pThread;
	struct TaskStruct *pLast;
	unsigned long flags;
	s32 loop;
	u32 minimum;
	u32 cookie;
	s32 threadPrio = 0;
	u32 EndAddr;
	char long_msg[CMDQ_LONGSTRING_MAX];
	u32 msg_offset;
	s32 msg_max_size;
	u32 *pVABase = NULL;
	dma_addr_t MVABase = 0;
	/* for no suspend thread, we shift END before JUMP */
	bool shift_end = false;

	cmdq_long_string_init(false, long_msg, &msg_offset, &msg_max_size);
	cmdq_core_get_task_first_buffer(pTask, &pVABase, &MVABase);
	cmdq_long_string(long_msg, &msg_offset, &msg_max_size,
		"-->EXEC: task 0x%p on thread %d begin, va:0x%p pa:%pa",
		pTask, thread, pVABase, &(MVABase));
	cmdq_long_string(long_msg, &msg_offset, &msg_max_size,
		" Size:%d bufferSize:%d scenario:%d flag:0x%llx\n",
		pTask->commandSize, pTask->bufferSize, pTask->scenario,
		pTask->engineFlag);
	if (unlikely(pTask->use_sram_buffer)) {
		cmdq_long_string(long_msg, &msg_offset, &msg_max_size,
			" SRAM Task, SRAM base:%u\n", pTask->sram_base);
	}

	CMDQ_MSG("%s", long_msg);

	status = 0;

	pThread = &(gCmdqContext.thread[thread]);

	pTask->trigger = sched_clock();

	CMDQ_PROF_SPIN_LOCK(gCmdqExecLock, flags, exec_task);

	/* update task's thread info */
	pTask->thread = thread;
	pTask->irqFlag = 0;
	pTask->taskState = TASK_STATE_BUSY;

	/* for loop command, we do not shift END before JUMP */
	if (likely(!pThread->loopCallback))
		shift_end = true;

	if (pThread->taskCount <= 0) {
		bool enablePrefetch;

		CMDQ_MSG("EXEC: new HW thread(%d)\n", thread);
		if (cmdq_core_reset_HW_thread(thread) < 0) {
			CMDQ_PROF_SPIN_UNLOCK(gCmdqExecLock, flags,
				exec_task_fail);
			return -EFAULT;
		}

		CMDQ_REG_SET32(CMDQ_THR_INST_CYCLES(thread),
			       cmdq_core_get_task_timeout_cycle(pThread));
#ifdef _CMDQ_DISABLE_MARKER_
		enablePrefetch = cmdq_core_thread_prefetch_size(thread) > 0;
		if (enablePrefetch) {
			CMDQ_MSG(
				"EXEC: set HW thread(%d) enable prefetch, size(%d)!\n",
				thread,
				cmdq_core_thread_prefetch_size(thread));
			CMDQ_REG_SET32(CMDQ_THR_PREFETCH(thread), 0x1);
		}
#endif
		threadPrio = cmdq_get_func()->priority(pTask->scenario);

		if (unlikely(pTask->use_sram_buffer)) {
			CMDQ_MSG(
				"EXEC: set HW thread(%d) to SRAM base:%u, qos:%d\n",
				thread, pTask->sram_base, threadPrio);
			EndAddr = CMDQ_PHYS_TO_AREG(pTask->sram_base +
				pTask->commandSize / sizeof(u64));
			CMDQ_REG_SET32(CMDQ_THR_END_ADDR(thread), EndAddr);
			CMDQ_REG_SET32(CMDQ_THR_CURR_ADDR(thread),
				CMDQ_PHYS_TO_AREG(pTask->sram_base));
		} else {
			MVABase = cmdq_core_task_get_first_pa(pTask);
			if (shift_end)
				EndAddr = CMDQ_PHYS_TO_AREG(
					cmdq_core_task_get_final_instr(pTask) -
					CMDQ_INST_SIZE);
			else
				EndAddr = CMDQ_PHYS_TO_AREG(
					CMDQ_THR_FIX_END_ADDR(thread));
			CMDQ_MSG(
				"EXEC: set HW thread(%d) pc:%pa qos:%d set end addr:0x%08x\n",
				thread, &MVABase, threadPrio, EndAddr);
			CMDQ_REG_SET32(CMDQ_THR_END_ADDR(thread), EndAddr);
			CMDQ_REG_SET32(CMDQ_THR_CURR_ADDR(thread),
				CMDQ_PHYS_TO_AREG(MVABase));
		}

		/* bit 0-2 for priority level; */
		CMDQ_REG_SET32(CMDQ_THR_CFG(thread), threadPrio & 0x7);

		/* For loop thread, do not enable timeout */
		CMDQ_REG_SET32(CMDQ_THR_IRQ_ENABLE(thread),
			pThread->loopCallback ? 0x011 : 0x013);

		if (pThread->loopCallback) {
			CMDQ_MSG("EXEC: HW thread(%d) in loop func 0x%p\n",
				thread, pThread->loopCallback);
		}

		/* attach task to thread */
		minimum = CMDQ_GET_COOKIE_CNT(thread);
		cmdq_core_insert_task_from_thread_array_by_cookie(
			pTask, pThread, (minimum + 1), true);

		/* enable HW thread */
		CMDQ_MSG("enable HW thread(%d)\n", thread);

		CMDQ_PROF_MMP(cmdq_mmp_get_event()->thread_en,
			      MMPROFILE_FLAG_PULSE, thread,
			      pThread->nextCookie - 1);

		CMDQ_REG_SET32(CMDQ_THR_ENABLE_TASK(thread), 0x01);
	} else {
		u32 last_cookie;
		u32 last_inst_pa = 0;
		u32 thread_pc = 0;
		u32 end_addr = 0;

		CMDQ_MSG(
			"EXEC: reuse HW thread(%d), taskCount:%d, cmd_size:%d\n",
			thread, pThread->taskCount, pTask->commandSize);

		if (unlikely(pTask->use_sram_buffer)) {
			CMDQ_AEE("CMDQ",
				"SRAM task should not execute append\n");
			CMDQ_PROF_SPIN_UNLOCK(gCmdqExecLock, flags,
				exec_task_sram_fail);
			return -EFAULT;
		}

		cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_APPEND_THR(thread));
		cookie = pThread->nextCookie;

		/* Boundary case tested:
		 * EOC have been executed, but JUMP is not executed
		 * Thread PC: 0x9edc0dd8, End: 0x9edc0de0,
		 * Curr Cookie: 1, Next Cookie: 2
		 */

		/* Check if pc stay at last jump since GCE may not execute it,
		 * even if we change jump instruction before resume.
		 */
		last_cookie = pThread->nextCookie <= 0 ?
			(cmdq_core_max_task_in_thread(thread) - 1) :
			(pThread->nextCookie - 1) %
			cmdq_core_max_task_in_thread(thread);
		last_inst_pa = pThread->pCurTask[last_cookie] ?
			cmdq_core_task_get_eoc_pa(
			pThread->pCurTask[last_cookie]) + CMDQ_INST_SIZE : 0;
		thread_pc = CMDQ_AREG_TO_PHYS(CMDQ_REG_GET32(
			CMDQ_THR_CURR_ADDR(thread)));
		end_addr = CMDQ_AREG_TO_PHYS(CMDQ_REG_GET32(
			CMDQ_THR_END_ADDR(thread)));

		/* PC = END - 8, EOC is executed
		 * PC = END - 0, All CMDs are executed
		 */
		if ((thread_pc == end_addr) || (thread_pc == last_inst_pa)) {
			MVABase = cmdq_core_task_get_first_pa(pTask);
			/* set to pTask directly */
			if (shift_end)
				EndAddr = CMDQ_PHYS_TO_AREG(
					cmdq_core_task_get_final_instr(pTask)
					- CMDQ_INST_SIZE);
			else
				EndAddr = CMDQ_PHYS_TO_AREG(
					CMDQ_THR_FIX_END_ADDR(thread));

			cmdq_long_string_init(true, long_msg, &msg_offset,
				&msg_max_size);
			cmdq_long_string(long_msg, &msg_offset, &msg_max_size,
				"EXEC: Task:0x%p Set HW thread(%d) pc from 0x%08x(end:0x%08x) to %pa(end:0x%08x)",
				pTask, thread, thread_pc, end_addr, &MVABase,
				EndAddr);
			cmdq_long_string(long_msg, &msg_offset, &msg_max_size,
				" oriNextCookie:%d oriTaskCount:%d\n",
				cookie, pThread->taskCount);
			CMDQ_MSG("%s", long_msg);

			CMDQ_REG_SET32(CMDQ_THR_CURR_ADDR(thread),
				CMDQ_PHYS_TO_AREG(MVABase));
			CMDQ_REG_SET32(CMDQ_THR_END_ADDR(thread), EndAddr);

			pThread->pCurTask[cookie %
				cmdq_core_max_task_in_thread(thread)] = pTask;
			pThread->taskCount++;
			pThread->allowDispatching = 1;
		} else {
			CMDQ_MSG("Connect new task's MVA to previous one\n");

			/* Current task that shuld be processed */
			minimum = CMDQ_GET_COOKIE_CNT(thread) + 1;
			if (minimum > CMDQ_MAX_COOKIE_VALUE)
				minimum = 0;

			/* Calculate loop count to adjust the tasks' order */
			if (minimum <= cookie) {
				loop = cookie - minimum;
			} else {
				/* Counter wrapped */
				loop = (CMDQ_MAX_COOKIE_VALUE - minimum + 1) +
					cookie;
			}

			CMDQ_MSG(
				"Reorder task:0x%p in range [%d, %d] with count %d thread %d thread pc:0x%08x inst pa:0x%08x\n",
				pTask, minimum, cookie, loop, thread,
				thread_pc, last_inst_pa);

			/* ALPS01672377
			 * .note pThread->taskCount-- when remove task from
			 * pThread in ISR
			 * .In mutlple SW clients or async case,
			 *  clients may continue submit tasks with overlap
			 *  engines
			 *  it's okey 0 = abs(pThread->nextCookie, THR_CNT+1)
			 *  when...
			 *      .submit task_1, trigger GCE
			 *      .submit task_2:
			 *          .GCE exec task1 done
			 *          .task_2 lock execLock when insert task
			 *           to thread
			 *          .task 1's IRQ
			 */

			if (loop < 0) {
				cmdq_core_dump_task_in_thread(thread, true,
					true, false);

				cmdq_long_string_init(true, long_msg,
					&msg_offset, &msg_max_size);
				cmdq_long_string(long_msg, &msg_offset,
					&msg_max_size,
					"Invalid task count(%d) in thread %d for reorder,",
					loop, thread);
				cmdq_long_string(long_msg, &msg_offset,
					&msg_max_size,
					" next cookie:%d next cookie HW:%d task:0x%p\n",
					pThread->nextCookie, minimum, pTask);
				CMDQ_AEE("CMDQ", "%s", long_msg);
				cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_APPEND_THR(
					thread));
				CMDQ_PROF_SPIN_UNLOCK(gCmdqExecLock,
					flags, exec_task_connect_fail);
				return -EFAULT;
			}

			if (loop > cmdq_core_max_task_in_thread(thread)) {
				CMDQ_LOG(
					"loop = %d, execeed max task in thread\n",
					loop);
				loop = loop % cmdq_core_max_task_in_thread(
					thread);
			}
			CMDQ_MSG("Reorder %d tasks for performance begin\n",
				loop);
			/* By default, pTask is the last task,
			 * and insert [cookie % CMDQ_MAX_TASK_IN_THREAD]
			 */
			pLast = pTask;

			status = cmdq_core_exec_find_task_slot(&pLast, pTask,
				thread, loop);
			if (status < 0) {
				cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_APPEND_THR(
					thread));
				CMDQ_PROF_SPIN_UNLOCK(gCmdqExecLock, flags,
					exec_task_invalid_task);
				CMDQ_AEE("CMDQ",
					"Invalid task state for reorder, thread:%d status:%d task:0x%p\n",
					thread, status, pTask);
				return status;
			}

			/* We must set memory barrier here to make sure
			 * we modify jump before enable thread
			 */
			smp_mb();

			if (shift_end) {
				EndAddr = CMDQ_PHYS_TO_AREG(
					cmdq_core_task_get_final_instr(pLast) -
					CMDQ_INST_SIZE);
				CMDQ_REG_SET32(CMDQ_THR_END_ADDR(thread),
					EndAddr);
			}
			pThread->taskCount++;
			pThread->allowDispatching = 1;
		}

		pThread->nextCookie += 1;
		if (pThread->nextCookie > CMDQ_MAX_COOKIE_VALUE) {
			/* Reach the maximum cookie */
			pThread->nextCookie = 0;
		}

		/* resume HW thread */
		CMDQ_PROF_MMP(cmdq_mmp_get_event()->thread_en,
			MMPROFILE_FLAG_PULSE, thread,
			pThread->nextCookie - 1);
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_APPEND_THR(thread));
	}

	CMDQ_PROF_SPIN_UNLOCK(gCmdqExecLock, flags, exec_task_done);

	CMDQ_MSG("<--EXEC: status:%d\n", status);

	return status;
}

s32 cmdqCoreSuspend(void)
{
	unsigned long flags = 0L;
	struct EngineStruct *pEngine = NULL;
	u32 execThreads = 0x0;
	int refCount = 0;
	bool killTasks = false;
	struct TaskStruct *pTask = NULL;
	struct list_head *p = NULL;
	int i = 0;
	const u32 max_thread_count = cmdq_dev_get_thread_count();

	pEngine = gCmdqContext.engine;

	refCount = atomic_read(&gCmdqThreadUsage);
	if (refCount)
		execThreads = CMDQ_REG_GET32(CMDQ_CURR_LOADED_THR);

	if (cmdq_get_func()->moduleEntrySuspend(pEngine) < 0) {
		CMDQ_ERR(
			"[SUSPEND] MDP running, kill tasks. threads:0x%08x ref:%d\n",
			execThreads, refCount);
		killTasks = true;
	} else if ((refCount > 0) || (0x80000000 & execThreads)) {
		CMDQ_ERR(
			"[SUSPEND] other running, kill tasks. threads:0x%08x ref:%d\n",
			execThreads, refCount);
		killTasks = true;
	}

	/* We need to ensure the system is ready to suspend,
	 * so kill all running CMDQ tasks
	 * and release HW engines.
	 */
	if (killTasks) {
		CMDQ_PROF_MUTEX_LOCK(gCmdqTaskMutex, suspend_kill);

		/* print active tasks */
		CMDQ_ERR("[SUSPEND] active tasks during suspend:\n");
		list_for_each(p, &gCmdqContext.taskActiveList) {
			pTask = list_entry(p, struct TaskStruct, listEntry);
			if (cmdq_core_is_valid_in_active_list(pTask))
				cmdq_core_dump_task(pTask);
		}

		/* remove all active task from thread */
		CMDQ_ERR("[SUSPEND] remove all active tasks\n");
		list_for_each(p, &gCmdqContext.taskActiveList) {
			pTask = list_entry(p, struct TaskStruct, listEntry);

			if (pTask->thread != CMDQ_INVALID_THREAD) {
				CMDQ_PROF_SPIN_LOCK(gCmdqExecLock, flags,
					suspend_loop);

				CMDQ_ERR("[SUSPEND] release task:0x%p\n",
					pTask);

				cmdq_core_force_remove_task_from_thread(pTask,
					pTask->thread);
				pTask->taskState = TASK_STATE_KILLED;

				CMDQ_PROF_SPIN_UNLOCK(gCmdqExecLock, flags,
					suspend_loop);

				/* release all thread and mark active tasks
				 * as "KILLED" (so that thread won't release
				 * again)
				 */
				CMDQ_ERR(
					"[SUSPEND] release all threads and HW clocks\n");
				cmdq_core_release_thread(pTask);
			}
		}

		CMDQ_PROF_MUTEX_UNLOCK(gCmdqTaskMutex, suspend_kill);

		/* TODO: skip secure path thread... */
		/* disable all HW thread */
		CMDQ_ERR("[SUSPEND] disable all HW threads\n");
		for (i = 0; i < max_thread_count; i++)
			cmdq_core_disable_HW_thread(i);

		/* reset all threadStruct */
		cmdq_core_reset_thread_struct();

		/* reset all engineStruct */
		memset(&gCmdqContext.engine[0], 0,
			sizeof(gCmdqContext.engine));
		cmdq_core_reset_engine_struct();
	}

	if (gCmdqISPClockCounter)
		CMDQ_AEE("CMDQ", "Call ISP clock is NOT paired:%d\n",
			gCmdqISPClockCounter);

	CMDQ_PROF_SPIN_LOCK(gCmdqThreadLock, flags, suspend);
	gCmdqSuspended = true;
	CMDQ_PROF_SPIN_UNLOCK(gCmdqThreadLock, flags, suspend);

	CMDQ_PROF_MUTEX_LOCK(gCmdqTaskMutex, suspend_pool);
	cmdq_core_destroy_buffer_pool();
	CMDQ_PROF_MUTEX_UNLOCK(gCmdqTaskMutex, suspend_pool);

	CMDQ_MSG("CMDQ is suspended\n");
	g_delay_thread_inited = false;
	/* ALWAYS allow suspend */
	return 0;
}


s32 cmdq_core_resume_impl(const char *tag)
{
	unsigned long flags = 0L;
	int refCount = 0;

	CMDQ_PROF_SPIN_LOCK(gCmdqThreadLock, flags, resume);

	refCount = atomic_read(&gCmdqThreadUsage);
	CMDQ_MSG("[%s] resume, refCount:%d\n", tag, refCount);

	gCmdqSuspended = false;

	CMDQ_PROF_SPIN_UNLOCK(gCmdqThreadLock, flags, resume);

	CMDQ_PROF_MUTEX_LOCK(gCmdqTaskMutex, resume_pool);
	cmdq_core_create_buffer_pool();
	CMDQ_PROF_MUTEX_UNLOCK(gCmdqTaskMutex, resume_pool);

	if (!g_delay_thread_inited) {
		cmdq_copy_delay_to_sram();
		CMDQ_MSG("resume, after copy delay to SRAM\n");
		g_delay_thread_inited = true;
	}

	/* during suspending, there may be queued tasks. */
	/* we should process them if any. */
	if (!work_pending(&gCmdqContext.taskConsumeWaitQueueItem)) {
		CMDQ_MSG("[%s] there are undone task, process them\n", tag);
		/* we use system global work queue
		 * (kernel thread kworker/n)
		 */
		CMDQ_PROF_MMP(cmdq_mmp_get_event()->consume_add,
			MMPROFILE_FLAG_PULSE, 0, 0);
		queue_work(gCmdqContext.taskConsumeWQ,
			&gCmdqContext.taskConsumeWaitQueueItem);
	}

	return 0;
}

s32 cmdqCoreResume(void)
{
	CMDQ_VERBOSE("[RESUME] do nothing\n");
	/* do nothing */
	return 0;
}

s32 cmdqCoreResumedNotifier(void)
{
	/* TEE project limitation:
	 * .t-base daemon process is available after process-unfreeze
	 * .need t-base daemon for communication to secure world
	 * .M4U port security setting backup/resore needs to entry sec world
	 * .M4U port security setting is access normal PA
	 *
	 * Delay resume timing until process-unfreeze done in order to
	 * ensure M4U driver had restore M4U port security setting
	 */
	CMDQ_VERBOSE("[RESUME] cmdqCoreResumedNotifier\n");
	return cmdq_core_resume_impl("RESUME_NOTIFIER");
}

static s32 cmdq_core_exec_task_async_with_retry(
	struct TaskStruct *pTask, s32 thread)
{
	s32 retry = 0;
	s32 status = 0;
	struct ThreadStruct *pThread;

	pThread = &(gCmdqContext.thread[thread]);
	/* Replace instruction by thread ID */
	cmdq_core_replace_v3_instr(pTask, thread);

	if (!cmdq_core_verfiy_command_end(pTask))
		return -EFAULT;

	if (pThread->loopCallback) {
		/* Do not insert Wait for loop due to loop no need append */
		CMDQ_MSG("Ignore insert wait for loop task\n");
	} else {
		u32 *append_instr = cmdq_core_task_get_append_va(pTask);

		if (append_instr) {
			/* Update original JUMP to wait event */
			append_instr[1] = (CMDQ_CODE_WFE << 24) |
				CMDQ_SYNC_TOKEN_APPEND_THR(thread);
			if (!pTask->secData.is_secure) {
				/* Make jump become invalid instruction */
				pTask->pCMDEnd[0] = 0x0;
				pTask->pCMDEnd[-1] = 0x0;
			}
			/* make sure instructions are synced in DRAM */
			smp_mb();
			CMDQ_MSG(
				"After insert wait: pTask 0x%p last 3 instr (%08x:%08x, %08x:%08x, %08x:%08x)\n",
				pTask, append_instr[0], append_instr[1],
				pTask->pCMDEnd[-3], pTask->pCMDEnd[-2],
				pTask->pCMDEnd[-1], pTask->pCMDEnd[0]);
		} else {
			CMDQ_AEE("CMDQ",
				"Task set append instruction failed, size:%d",
				pTask->commandSize);
		}
	}

	do {
		/* Save command buffer dump */
		if (retry == 0)
			cmdq_core_save_command_buffer_dump(pTask);

		status = (pTask->secData.is_secure == false) ?
		    (cmdq_core_exec_task_async_impl(pTask, thread)) :
		    (cmdq_core_exec_task_async_secure_impl(pTask, thread));

		if (status >= 0)
			break;

		if ((pTask->taskState == TASK_STATE_KILLED) ||
		    (pTask->taskState == TASK_STATE_ERROR) ||
		    (pTask->taskState == TASK_STATE_ERR_IRQ)) {
			CMDQ_ERR("cmdq_core_exec_task_async_impl fail\n");
			status = -EFAULT;
			break;
		}

		++retry;
	} while (retry < CMDQ_MAX_RETRY_COUNT);

	return status;
}

static s32 cmdq_core_consume_waiting_list(struct work_struct *_ignore)
{
	struct list_head *p, *n = NULL;
	struct TaskStruct *pTask = NULL;
	struct ThreadStruct *pThread = NULL;
	s32 thread = CMDQ_INVALID_THREAD;
	s32 status = 0;
	bool threadAcquired = false;
	enum CMDQ_HW_THREAD_PRIORITY_ENUM thread_prio = CMDQ_THR_PRIO_NORMAL;
	CMDQ_TIME consumeTime;
	s32 waitingTimeMS;
	bool needLog = false;
	bool dumpTriggerLoop = false;
	bool timeout_flag = false;
	u32 disp_list_count = 0;
	u32 user_list_count = 0;
	u32 index = 0;
	CMDQ_TIME consume_cost;

	/* when we're suspending, do not execute any tasks.
	 * delay & hold them.
	 */
	if (gCmdqSuspended)
		return status;

	CMDQ_PROF_START(current->pid, __func__);
	CMDQ_PROF_MMP(cmdq_mmp_get_event()->consume_done,
		MMPROFILE_FLAG_START, current->pid, 0);
	consumeTime = sched_clock();

	CMDQ_PROF_MUTEX_LOCK(gCmdqTaskMutex, consume_wait_list);

	threadAcquired = false;
	CMDQ_PROF_TIME_BEGIN(consume_cost);

	/* scan and remove (if executed) waiting tasks */
	list_for_each_safe(p, n, &gCmdqContext.taskWaitList) {
		pTask = list_entry(p, struct TaskStruct, listEntry);

		thread_prio = cmdq_get_func()->priority(pTask->scenario);

		CMDQ_MSG(
			"-->THREAD: try acquire thread for task:0x%p thread_prio:%d\n",
			pTask, thread_prio);
		CMDQ_MSG(
			"-->THREAD: task_prio:%d flag:0x%llx scenario:%d begin\n",
			pTask->priority, pTask->engineFlag, pTask->scenario);

		CMDQ_GET_TIME_IN_MS(pTask->submit, consumeTime, waitingTimeMS);
		timeout_flag = waitingTimeMS >= CMDQ_PREDUMP_TIMEOUT_MS;
		needLog = timeout_flag;

		if (timeout_flag) {
			if (cmdq_get_func()->isDispScenario(
				pTask->scenario)) {
				if (disp_list_count > 0)
					needLog = false;
				disp_list_count++;
			} else {
				if (user_list_count > 0)
					needLog = false;
				user_list_count++;
			}
		}

		/* Allocate hw thread */
		CMDQ_PROF_TIME_END(consume_cost, "before_acquire_thread");
		thread = cmdq_core_acquire_thread(pTask, thread_prio, needLog);
		CMDQ_PROF_TIME_END(consume_cost, "after_acquire_thread");

		if (thread == CMDQ_INVALID_THREAD) {
			/* have to wait, remain in wait list */
			CMDQ_MSG(
				"<--THREAD: acquire thread fail, need to wait\n");
			if (needLog) {
				/* task wait too long */
				CMDQ_ERR(
					"acquire thread fail, task:0x%p thread_prio:%d flag:0x%llx\n",
				pTask, thread_prio, pTask->engineFlag);

				dumpTriggerLoop = pTask->scenario ==
				    CMDQ_SCENARIO_PRIMARY_DISP ?
				    true : dumpTriggerLoop;
			}
			continue;
		}

		pThread = &gCmdqContext.thread[thread];

		/* some task is ready to run */
		threadAcquired = true;

		/* Assign loop function if the thread should be loop thread */
		pThread->loopCallback = pTask->loopCallback;
		pThread->loopData = pTask->loopData;

		/* Start execution,
		 * remove from wait list and put into active list
		 */
		list_del_init(&pTask->listEntry);
		list_add_tail(&pTask->listEntry, &gCmdqContext.taskActiveList);

		CMDQ_MSG(
			"<--THREAD: acquire thread w/flag:0x%llx on thread(%d):0x%p end\n",
			pTask->engineFlag, thread, pThread);

		/* callback task for tracked group */
		for (index = 0; index < CMDQ_MAX_GROUP_COUNT; ++index) {
			if (gCmdqGroupCallback[index].trackTask) {
				CMDQ_MSG(
					"Track: track task group %d with task:%p\n",
					index, pTask);
				if (cmdq_core_is_group_flag(
					(enum CMDQ_GROUP_ENUM)index,
					pTask->engineFlag)) {
					CMDQ_MSG(
						"Track: track task group %d flag=0x%llx\n",
						index, pTask->engineFlag);
					gCmdqGroupCallback[index].trackTask(
						pTask);
				}
			}
		}

		/* Run task on thread */
		CMDQ_PROF_TIME_END(consume_cost, "before_exec_task");
		status = cmdq_core_exec_task_async_with_retry(pTask, thread);
		CMDQ_PROF_TIME_END(consume_cost, "after_exec_task");
		if (status < 0) {
			CMDQ_ERR(
				"<--THREAD: cmdq_core_exec_task_async_with_retry fail, release task 0x%p\n",
				pTask);
			cmdq_core_track_task_record(pTask, thread);
			cmdq_core_release_thread(pTask);
			cmdq_core_release_task_unlocked(pTask);
			pTask = NULL;
		}
	}

	if (disp_list_count > 0 && disp_list_count >= user_list_count) {
		/* print error message */
		CMDQ_ERR(
			"There too many DISP (%d) tasks cannot acquire thread\n",
			disp_list_count);
	} else if (user_list_count > 0 && user_list_count >= disp_list_count) {
		/* print error message */
		CMDQ_ERR(
			"There too many user space (%d) tasks cannot acquire thread\n",
			user_list_count);
	}

	if (dumpTriggerLoop) {
		/* HACK: observe trigger loop status
		 * when acquire config thread failed.
		 */
		s32 dumpThread = cmdq_get_func()->dispThread(
			CMDQ_SCENARIO_PRIMARY_DISP);

		cmdq_core_dump_disp_trigger_loop_mini("ACQUIRE");
		cmdq_core_dump_thread_pc(dumpThread);
	}

	if (threadAcquired) {
		/* notify some task's SW thread to change their waiting state.
		 * (if they already called cmdqCoreWaitResultAndReleaseTask())
		 */
		wake_up_all(&gCmdqThreadDispatchQueue);
	}

	CMDQ_PROF_MUTEX_UNLOCK(gCmdqTaskMutex, consume_wait_list);

	CMDQ_PROF_END(current->pid, __func__);

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->consume_done, MMPROFILE_FLAG_END,
		current->pid, 0);

	return status;
}

static void cmdqCoreConsumeWaitQueueItem(struct work_struct *_ignore)
{
	s32 status;

	status = cmdq_core_consume_waiting_list(_ignore);
}

s32 cmdqCoreSubmitTaskAsyncImpl(struct cmdqCommandStruct *pCommandDesc,
	struct CmdqRecExtend *ext, CmdqInterruptCB loopCB,
	unsigned long loopData, struct TaskStruct **ppTaskOut)
{
	struct TaskStruct *pTask = NULL;
	s32 status = 0;
	CMDQ_TIME alloc_cost = 0;

	if (!gCmdqSuspended && !g_delay_thread_inited
		&& pCommandDesc->scenario != CMDQ_SCENARIO_MOVE) {
		if (cmdq_delay_thread_init() < 0) {
			CMDQ_ERR("delay init failed!\n");
			return -EFAULT;
		}
	}

	if (pCommandDesc->scenario != CMDQ_SCENARIO_TRIGGER_LOOP)
		cmdq_core_verfiy_command_desc_end(pCommandDesc);

	CMDQ_MSG("-->SUBMIT_ASYNC: cmd 0x%p begin\n",
		CMDQ_U32_PTR(pCommandDesc->pVABase));
	CMDQ_PROF_START(current->pid, __func__);

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->alloc_task, MMPROFILE_FLAG_START,
		current->pid, pCommandDesc->scenario);
	alloc_cost = sched_clock();

	/* Allocate Task. This creates a new task
	 * and put into tail of waiting list
	 */
	pTask = cmdq_core_acquire_task(pCommandDesc, ext, loopCB, loopData);

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->alloc_task, MMPROFILE_FLAG_END,
		current->pid, pCommandDesc->blockSize/8);
	alloc_cost = sched_clock() - alloc_cost;

	if (!pTask) {
		CMDQ_PROF_END(current->pid, __func__);
		return -EFAULT;
	}

	if (alloc_cost > CMDQ_PROFILE_LIMIT_2) {
		CMDQ_LOG("[warn] alloc task cost %llu us > %u ms\n",
			div_s64(alloc_cost, 1000),
			CMDQ_PROFILE_LIMIT_2 / 1000000);
	}

	if (ppTaskOut)
		*ppTaskOut = pTask;

	/* Try to lock resource base on engine flag */
	cmdqCoreLockResource(pTask->engineFlag, false);

	/* consume the waiting list. */
	/* this may or may not execute the task, */
	/* depending on available threads. */
	status = cmdq_core_consume_waiting_list(NULL);

	CMDQ_MSG("<--SUBMIT_ASYNC: task:0x%p end\n",
		CMDQ_U32_PTR(pCommandDesc->pVABase));
	CMDQ_PROF_END(current->pid, __func__);
	return status;
}

s32 cmdqCoreSubmitTaskAsync(struct cmdqCommandStruct *pCommandDesc,
	struct CmdqRecExtend *ext, CmdqInterruptCB loopCB,
	unsigned long loopData, struct TaskStruct **ppTaskOut)
{
	s32 status = 0;
	struct TaskStruct *pTask = NULL;

	status = cmdqCoreSubmitTaskAsyncImpl(pCommandDesc, ext, loopCB,
		loopData, &pTask);
	if (ppTaskOut)
		*ppTaskOut = pTask;

	return status;
}

s32 cmdqCoreReleaseTask(struct TaskStruct *pTask)
{
	unsigned long flags;
	s32 status = 0;
	s32 thread = pTask->thread;
	struct ThreadStruct *pThread = NULL;

	CMDQ_MSG("<--TASK: cmdqCoreReleaseTask 0x%p\n", pTask);

	if (thread == CMDQ_INVALID_THREAD) {
		CMDQ_ERR("cmdqCoreReleaseTask, thread is invalid (%d)\n",
			thread);
		return -EFAULT;
	}

	pThread = &(gCmdqContext.thread[thread]);

	if (pThread) {
		/* this task is being executed (or queueed) on a HW thread */

		/* get SW lock first to ensure atomic access HW */
		CMDQ_PROF_SPIN_LOCK(gCmdqExecLock, flags, release_task);
		/* make sure instructions are really in DRAM */
		smp_mb();

		if (pThread->loopCallback) {
			/* a loop thread has only 1 task involved
			 * so we can release thread directly
			 * otherwise we need to connect remaining tasks
			 */
			if (pThread->taskCount > 1)
				CMDQ_AEE("CMDQ", "task count more than 1:%u\n",
					pThread->taskCount);

			/* suspend and reset the thread */
			status = cmdq_core_suspend_HW_thread(thread, __LINE__);
			if (status < 0)
				CMDQ_AEE("CMDQ",
					"suspend HW thread failed status:%d\n",
					status);

			pThread->taskCount = 0;
			cmdq_core_disable_HW_thread(thread);
		} else {
			/* TODO: we should check thread enabled or
			 * not before resume it.
			 */
			status = cmdq_core_force_remove_task_from_thread(
				pTask, thread);
			if (pThread->taskCount > 0)
				cmdq_core_resume_HW_thread(thread);
		}

		CMDQ_PROF_SPIN_UNLOCK(gCmdqExecLock, flags, release_task);
		wake_up(&gCmdWaitQueue[thread]);
	}

	cmdq_core_track_task_record(pTask, thread);
	cmdq_core_release_thread(pTask);
	cmdq_core_auto_release_task(pTask);
	CMDQ_MSG("-->TASK: cmdqCoreReleaseTask 0x%p end\n", pTask);
	return 0;
}

s32 cmdqCoreWaitAndReleaseTask(struct TaskStruct *pTask, u32 timeout_ms)
{
	return cmdqCoreWaitResultAndReleaseTask(pTask, NULL, timeout_ms);
}

s32 cmdqCoreWaitResultAndReleaseTask(struct TaskStruct *pTask,
	struct cmdqRegValueStruct *pResult, u32 timeout_ms)
{
	s32 status;
	s32 thread;
	int i;

	if (!pTask) {
		CMDQ_ERR("cmdqCoreWaitAndReleaseTask err ptr=0x%p\n",
			pTask);
		return -EFAULT;
	}

	if (pTask->taskState == TASK_STATE_IDLE) {
		CMDQ_ERR("cmdqCoreWaitAndReleaseTask task=0x%p is IDLE\n",
			pTask);
		return -EFAULT;
	}

	CMDQ_PROF_START(current->pid, __func__);

	/*  */
	/* wait for task finish */
	thread = pTask->thread;
	status = cmdq_core_wait_task_done(pTask, timeout_ms);

	/*  */
	/* retrieve result */
	if (pResult && pResult->count &&
		pResult->count <= CMDQ_MAX_DUMP_REG_COUNT) {
		/* clear results */
		memset(CMDQ_U32_PTR(pResult->regValues), 0,
			pResult->count * sizeof(CMDQ_U32_PTR(
			pResult->regValues)[0]));

		CMDQ_PROF_MUTEX_LOCK(gCmdqTaskMutex, fill_reg_result);
		for (i = 0; i < pResult->count && i < pTask->regCount; ++i) {
			/* fill results */
			CMDQ_U32_PTR(pResult->regValues)[i] =
				pTask->regResults[i];
		}
		CMDQ_PROF_MUTEX_UNLOCK(gCmdqTaskMutex, fill_reg_result);
	}

	cmdq_core_track_task_record(pTask, thread);
	cmdq_core_release_thread(pTask);
	cmdq_core_auto_release_task(pTask);
	if (g_cmdq_consume_again == true) {
		cmdq_core_add_consume_task();
		g_cmdq_consume_again = false;
	}
	CMDQ_PROF_END(current->pid, __func__);

	return status;
}

static void cmdq_core_auto_release_work(struct work_struct *workItem)
{
	s32 status = 0;
	struct TaskStruct *pTask = NULL;
	CmdqAsyncFlushCB finishCallback = NULL;
	u64 user_data = 0;
	u32 *pCmd = NULL;
	s32 commandSize = 0;
	struct CmdBufferStruct *cmd_buffer = NULL;
	u32 *copy_ptr = NULL;

	pTask = container_of(workItem, struct TaskStruct, autoReleaseWork);

	finishCallback = pTask->flushCallback;
	user_data = pTask->flushData;
	commandSize = pTask->commandSize;
	pCmd = kzalloc(commandSize, GFP_KERNEL);
	if (!pCmd) {
		/* allocate command backup buffer failed
		 * wil make dump incomplete
		 */
		CMDQ_ERR("failed to alloc command buffer, size:%d\n",
			commandSize);
	} else {
		copy_ptr = pCmd;
		list_for_each_entry(cmd_buffer, &pTask->cmd_buffer_list,
			listEntry) {
			memcpy(copy_ptr, cmd_buffer->pVABase,
				commandSize > CMDQ_CMD_BUFFER_SIZE ?
				CMDQ_CMD_BUFFER_SIZE : commandSize);
			commandSize -= CMDQ_CMD_BUFFER_SIZE;
			copy_ptr += (CMDQ_CMD_BUFFER_SIZE / sizeof(u32));
		}
		commandSize = pTask->commandSize;
	}

	status = cmdqCoreWaitResultAndReleaseTask(pTask, NULL,
		CMDQ_DEFAULT_TIMEOUT_MS);

	CMDQ_VERBOSE("[Auto Release] released pTask=%p, status=%d\n",
		pTask, status);
	CMDQ_PROF_MMP(cmdq_mmp_get_event()->autoRelease_done,
		MMPROFILE_FLAG_PULSE, ((unsigned long)pTask), current->pid);

	/* Notify user */
	if (finishCallback) {
		CMDQ_VERBOSE(
			"[Auto Release] call user callback %p with data 0x%llx\n",
			finishCallback, user_data);
		if (finishCallback(user_data) < 0) {
			CMDQ_LOG(
				"[DEBUG]user complains execution abnormal, dump command...\n");
			CMDQ_LOG(
				"======TASK 0x%p command (%d) START\n",
				pTask, commandSize);
			if (pCmd)
				cmdqCoreDumpCommandMem(pCmd, commandSize);
			CMDQ_LOG("======TASK 0x%p command END\n", pTask);
		}
	}

	kfree(pCmd);
}

s32 cmdqCoreAutoReleaseTask(struct TaskStruct *pTask)
{
	s32 threadNo = CMDQ_INVALID_THREAD;
	bool is_secure;

	if (!pTask) {
		/* Error occurs when Double INIT_WORK */
		CMDQ_ERR("[Double INIT WORK] pTask is NULL");
		return 0;
	}

	if (!pTask->pCMDEnd || list_empty(&pTask->cmd_buffer_list)) {
		/* Error occurs when Double INIT_WORK */
		CMDQ_ERR("[Double INIT WORK] pTask(0x%p) is already released",
			pTask);
		return 0;
	}

	/* the work item is embeded in pTask already */
	/* but we need to initialized it */
	if (unlikely(atomic_inc_return(&pTask->useWorkQueue) != 1)) {
		/* Error occurs when Double INIT_WORK */
		CMDQ_ERR(
			"[Double INIT WORK] useWorkQueue is already TRUE, pTask(%p)",
			pTask);
		return -EFAULT;
	}

	/* use work queue to release task */
	INIT_WORK(&pTask->autoReleaseWork, cmdq_core_auto_release_work);

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->autoRelease_add,
		MMPROFILE_FLAG_PULSE, ((unsigned long)pTask), pTask->thread);

	/* Put auto release task to corresponded thread */
	if (pTask->thread != CMDQ_INVALID_THREAD) {
		queue_work(gCmdqContext.taskThreadAutoReleaseWQ[pTask->thread],
			&pTask->autoReleaseWork);
	} else {
		/* if task does not belong thread,
		 * use static dispatch thread at first,
		 * otherwise, use global context workqueue
		 */
		is_secure = pTask->secData.is_secure;
		threadNo = cmdq_get_func()->getThreadID(pTask->scenario,
			is_secure);
		if (threadNo != CMDQ_INVALID_THREAD) {
			queue_work(
				gCmdqContext.taskThreadAutoReleaseWQ[threadNo],
				&pTask->autoReleaseWork);
		} else {
			queue_work(gCmdqContext.taskAutoReleaseWQ,
				&pTask->autoReleaseWork);
		}
	}
	return 0;
}

s32 cmdqCoreSubmitTask(struct cmdqCommandStruct *pCommandDesc,
	struct CmdqRecExtend *ext)
{
	s32 status;
	struct TaskStruct *pTask = NULL;

	CMDQ_MSG("-->SUBMIT: SYNC cmd 0x%p begin\n",
		CMDQ_U32_PTR(pCommandDesc->pVABase));
	status = cmdqCoreSubmitTaskAsync(pCommandDesc, ext, NULL, 0, &pTask);

	if (status >= 0) {
		status = cmdqCoreWaitResultAndReleaseTask(pTask,
			&pCommandDesc->regValue,
			CMDQ_DEFAULT_TIMEOUT_MS);
		if (status < 0) {
			/* error status print */
			CMDQ_ERR("Task 0x%p wait fails\n", pTask);
		}
	} else {
		CMDQ_ERR("%s failed=%d", __func__, status);
	}

	CMDQ_MSG("<--SUBMIT: SYNC cmd 0x%p end\n",
		CMDQ_U32_PTR(pCommandDesc->pVABase));
	return status;
}

s32 cmdqCoreQueryUsage(s32 *pCount)
{
	unsigned long flags;
	struct EngineStruct *pEngine;
	s32 index;

	pEngine = gCmdqContext.engine;

	CMDQ_PROF_SPIN_LOCK(gCmdqThreadLock, flags, query_usage);

	for (index = 0; index < CMDQ_MAX_ENGINE_COUNT; index++)
		pCount[index] = pEngine[index].userCount;

	CMDQ_PROF_SPIN_UNLOCK(gCmdqThreadLock, flags, query_usage);

	return 0;
}

void cmdq_core_release_task_by_file_node(void *file_node)
{
	struct TaskStruct *pTask = NULL;
	struct list_head *p = NULL;

	/* Since the file node is closed, there is no way
	 * user space can issue further "wait_and_close" request,
	 * so we must auto-release running/waiting tasks
	 * to prevent resource leakage
	 */

	/* walk through active and waiting lists and release them */
	CMDQ_PROF_MUTEX_LOCK(gCmdqTaskMutex, release_task_by_node);

	list_for_each(p, &gCmdqContext.taskActiveList) {
		pTask = list_entry(p, struct TaskStruct, listEntry);
		if (pTask->taskState != TASK_STATE_IDLE &&
			pTask->privateData &&
			CMDQ_TASK_PRIVATE(pTask)->node_private_data ==
				file_node &&
			cmdq_core_is_request_from_user_space(
				pTask->scenario)) {

			CMDQ_LOG(
				"[WARNING] ACTIVE task 0x%p release because file node 0x%p closed\n",
				pTask, file_node);
			cmdq_core_dump_task(pTask);

			/* since we already inside mutex,
			 * do not cmdqReleaseTask directly,
			 * instead we change state to "KILLED"
			 * and arrange a auto-release.
			 * Note that these tasks may already issued to HW
			 * so there is a chance that following MPU/M4U
			 * violation may occur, if the user space process has
			 * destroyed.
			 * The ideal solution is to stop / cancel HW operation
			 * immediately, but we cannot do so due to SMI
			 * hang risk.
			 */
			cmdqCoreAutoReleaseTask(pTask);
		}
	}
	list_for_each(p, &gCmdqContext.taskWaitList) {
		pTask = list_entry(p, struct TaskStruct, listEntry);
		if (pTask->taskState == TASK_STATE_WAITING &&
			pTask->privateData &&
			CMDQ_TASK_PRIVATE(pTask)->node_private_data ==
				file_node &&
			cmdq_core_is_request_from_user_space(
				pTask->scenario)) {

			CMDQ_LOG(
				"[WARNING] WAITING task 0x%p release because file node 0x%p closed\n",
				pTask, file_node);
			cmdq_core_dump_task(pTask);

			/* since we already inside mutex,
			 * and these WAITING tasks will not be consumed
			 * (acquire thread / exec)
			 * we can release them directly.
			 * note that we use unlocked version since we already
			 * hold gCmdqTaskMutex.
			 */
			cmdq_core_release_task_unlocked(pTask);
		}
	}

	CMDQ_PROF_MUTEX_UNLOCK(gCmdqTaskMutex, release_task_by_node);
}

unsigned long long cmdq_core_get_GPR64(const enum CMDQ_DATA_REGISTER_ENUM regID)
{
#ifdef CMDQ_GPR_SUPPORT
	unsigned long long value;
	unsigned long long value1;
	unsigned long long value2;
	const u32 x = regID & 0x0F;

	if ((regID & 0x10) > 0) {
		/* query address GPR(64bit), Px */
		value1 = 0LL | CMDQ_REG_GET32(CMDQ_GPR_R32((2 * x)));
		value2 = 0LL | CMDQ_REG_GET32(CMDQ_GPR_R32((2 * x + 1)));
	} else {
		/* query data GPR(32bit), Rx */
		value1 = 0LL | CMDQ_REG_GET32(CMDQ_GPR_R32(x));
		value2 = 0LL;
	}

	value = (0LL) | (value2 << 32) | (value1);
	CMDQ_VERBOSE("get_GPR64(%x):0x%llx(0x%llx, 0x%llx)\n",
		regID, value, value2, value1);

	return value;

#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return 0LL;
#endif
}

void cmdq_core_set_GPR64(
	const enum CMDQ_DATA_REGISTER_ENUM regID,
	const unsigned long long value)
{
#ifdef CMDQ_GPR_SUPPORT

	const unsigned long long value1 = 0x00000000FFFFFFFF & value;
	const unsigned long long value2 = 0LL | value >> 32;
	const u32 x = regID & 0x0F;
	unsigned long long result;

	if ((regID & 0x10) > 0) {
		/* set address GPR(64bit), Px */
		CMDQ_REG_SET32(CMDQ_GPR_R32((2 * x)), value1);
		CMDQ_REG_SET32(CMDQ_GPR_R32((2 * x + 1)), value2);
	} else {
		/* set data GPR(32bit), Rx */
		CMDQ_REG_SET32(CMDQ_GPR_R32((2 * x)), value1);
	}

	result = 0LL | cmdq_core_get_GPR64(regID);
	if (value != result) {
		CMDQ_ERR(
			"set_GPR64(%x) failed, value is 0x%llx, not value 0x%llx\n",
			regID, result, value);
	}
#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
#endif
}

u32 cmdqCoreReadDataRegister(enum CMDQ_DATA_REGISTER_ENUM regID)
{
#ifdef CMDQ_GPR_SUPPORT
	return CMDQ_REG_GET32(CMDQ_GPR_R32(regID));
#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return 0;
#endif
}

u32 cmdq_core_thread_prefetch_size(const s32 thread)
{
	if (thread >= 0 && thread < cmdq_dev_get_thread_count())
		return g_dts_setting.prefetch_size[thread];
	else
		return 0;
}

void cmdq_core_dump_dts_setting(void)
{
	u32 index;
	struct ResourceUnitStruct *pResource = NULL;
	const u32 max_thread_count = cmdq_dev_get_thread_count();

	CMDQ_LOG("[DTS] Prefetch Thread Count:%d/%u\n",
		g_dts_setting.prefetch_thread_count, max_thread_count);
	CMDQ_LOG("[DTS] Prefetch Size of Thread:\n");
	for (index = 0; index < g_dts_setting.prefetch_thread_count &&
		index < max_thread_count; index++)
		CMDQ_LOG("	Thread[%d]=%d\n",
			index, g_dts_setting.prefetch_size[index]);
	CMDQ_LOG("[DTS] SRAM Sharing Config:\n");
	list_for_each_entry(pResource, &gCmdqContext.resourceList,
		list_entry) {
		CMDQ_LOG("	Engine=0x%016llx,, event=%d\n",
			pResource->engine_flag, pResource->lockEvent);
	}
}

s32 cmdq_core_get_running_task_by_engine_unlock(
	u64 engineFlag, u32 userDebugStrLen, struct TaskStruct *p_out_task)
{
	struct EngineStruct *pEngine;
	struct ThreadStruct *pThread;
	s32 index;
	s32 thread = CMDQ_INVALID_THREAD;
	s32 status = -EFAULT;
	struct TaskStruct *pTargetTask = NULL;

	if (!p_out_task)
		return -EINVAL;

	pEngine = gCmdqContext.engine;
	pThread = gCmdqContext.thread;
	for (index = 0; index < CMDQ_MAX_ENGINE_COUNT; index++) {
		if (engineFlag & (1LL << index)) {
			if (pEngine[index].userCount > 0) {
				thread = pEngine[index].currOwner;
				break;
			}
		}
	}

	if (thread != CMDQ_INVALID_THREAD) {
		struct TaskStruct *pTask;
		u32 insts[4];
		u32 currPC = CMDQ_AREG_TO_PHYS(CMDQ_REG_GET32(
			CMDQ_THR_CURR_ADDR(thread)));

		currPC = CMDQ_AREG_TO_PHYS(CMDQ_REG_GET32(
			CMDQ_THR_CURR_ADDR(thread)));
		for (index = 0; index < cmdq_core_max_task_in_thread(thread);
			index++) {
			pTask = pThread[thread].pCurTask[index];
			if (!pTask || list_empty(
				&pTask->cmd_buffer_list))
				continue;
			if (cmdq_core_get_pc(pTask, thread, insts, NULL)) {
				pTargetTask = pTask;
				break;
			}
		}
		if (!pTargetTask) {
			u32 currPC = CMDQ_AREG_TO_PHYS(CMDQ_REG_GET32(
				CMDQ_THR_CURR_ADDR(thread)));

			CMDQ_LOG("cannot find pc (0x%08x) at thread (%d)\n",
				currPC, thread);
			cmdq_core_dump_task_in_thread(thread, false, true,
				false);
		}
	}

	if (pTargetTask) {
		u32 current_debug_str_len = pTargetTask->userDebugStr ?
			(u32)strlen(pTargetTask->userDebugStr) : 0;
		u32 debug_str_len = userDebugStrLen < current_debug_str_len ?
			userDebugStrLen : current_debug_str_len;
		char *debug_str_buffer = p_out_task->userDebugStr;

		/* copy content except pointers */
		memcpy(p_out_task, pTargetTask, sizeof(struct TaskStruct));
		p_out_task->pCMDEnd = NULL;
		p_out_task->regResults = NULL;
		p_out_task->secStatus = NULL;

		if (debug_str_buffer) {
			p_out_task->userDebugStr = debug_str_buffer;
			if (debug_str_len)
				strncpy(debug_str_buffer,
					pTargetTask->userDebugStr,
					debug_str_len);
		}

		/* mark success */
		status = 0;
	}

	return status;
}

s32 cmdq_core_get_running_task_by_engine(u64 engineFlag,
	u32 userDebugStrLen, struct TaskStruct *p_out_task)
{
	s32 result = 0;
	unsigned long flags = 0;

	/* make sure context does not change during get and copy */
	CMDQ_PROF_SPIN_LOCK(gCmdqExecLock, flags, running_task);
	result = cmdq_core_get_running_task_by_engine_unlock(
		engineFlag, userDebugStrLen, p_out_task);
	CMDQ_PROF_SPIN_UNLOCK(gCmdqExecLock, flags, running_task);

	return result;
}

s32 cmdqCoreInitialize(void)
{
	struct TaskStruct *pTask;
	s32 index;
	s32 status = 0;
	const u32 max_thread_count = cmdq_dev_get_thread_count();

	atomic_set(&gCmdqThreadUsage, 0);
	atomic_set(&gSMIThreadUsage, 0);

	gCmdWaitQueue = kcalloc(max_thread_count, sizeof(*gCmdWaitQueue),
		GFP_KERNEL);
	for (index = 0; index < max_thread_count; index++)
		init_waitqueue_head(&gCmdWaitQueue[index]);

	init_waitqueue_head(&gCmdqThreadDispatchQueue);

	/* Reset overall context */
	memset(&gCmdqContext, 0x0, sizeof(struct ContextStruct));
	gCmdqContext.thread = kcalloc(max_thread_count, sizeof(
		*gCmdqContext.thread), GFP_KERNEL);
	/* some fields has non-zero initial value */
	cmdq_core_reset_engine_struct();
	cmdq_core_reset_thread_struct();

	/* Create task pool */
	gCmdqContext.taskCache = kmem_cache_create(
		CMDQ_DRIVER_DEVICE_NAME "_task",
		sizeof(struct TaskStruct), __alignof__(struct TaskStruct),
		SLAB_POISON | SLAB_HWCACHE_ALIGN | SLAB_RED_ZONE,
		&cmdq_core_task_ctor);
	/* Initialize task lists */
	INIT_LIST_HEAD(&gCmdqContext.taskFreeList);
	INIT_LIST_HEAD(&gCmdqContext.taskActiveList);
	INIT_LIST_HEAD(&gCmdqContext.taskWaitList);
	INIT_LIST_HEAD(&gCmdqContext.resourceList);
	INIT_LIST_HEAD(&gCmdqContext.sram_allocated_list);
	INIT_WORK(&gCmdqContext.taskConsumeWaitQueueItem,
		cmdqCoreConsumeWaitQueueItem);

	/* Initialize writable address */
	INIT_LIST_HEAD(&gCmdqContext.writeAddrList);

	/* Initialize work queue */
	gCmdqContext.taskAutoReleaseWQ =
		create_singlethread_workqueue("cmdq_auto_release");
	gCmdqContext.taskConsumeWQ =
		create_singlethread_workqueue("cmdq_task");
	gCmdqContext.resourceCheckWQ =
		create_singlethread_workqueue("cmdq_resource");
	cmdq_core_init_thread_work_queue();

	/* Initialize command buffer dump */
	memset(&gCmdqBufferDump, 0x0, sizeof(struct DumpCommandBufferStruct));

#ifdef CMDQ_DUMP_FIRSTERROR
	/* Reset overall first error dump */
	cmdq_core_reset_first_dump();
#endif

	/* pre-allocate free tasks */
	for (index = 0; index < CMDQ_INIT_FREE_TASK_COUNT; index++) {
		pTask = cmdq_core_task_create();
		if (pTask) {
			CMDQ_PROF_MUTEX_LOCK(gCmdqTaskMutex, pre_alloc_free);
			list_add_tail(&(pTask->listEntry),
				&gCmdqContext.taskFreeList);
			CMDQ_PROF_MUTEX_UNLOCK(gCmdqTaskMutex,
				pre_alloc_free);
		}
	}

	/* pre-allocate fake SPR SRAM area */
	{
		u32 fake_spr_sram = 0;

		status = cmdq_core_alloc_sram_buffer(max_thread_count *
			CMDQ_THR_CPR_MAX * sizeof(u32),
			"Fake SPR", &fake_spr_sram);
		if (status < 0) {
			CMDQ_ERR("Allocate Fake SPR failed !!");
		} else {
			CMDQ_LOG(
				"CPR for thread allocated, thread:%u free:%zu\n",
				max_thread_count,
				cmdq_core_get_free_sram_size());
		}
	}

	/* pre-allocate delay CPR SRAM area */
	{
		status = cmdq_core_alloc_sram_buffer(CMDQ_DELAY_MAX_SET *
			CMDQ_DELAY_SET_MAX_CPR * sizeof(u32),
			"Delay CPR", &gCmdqContext.delay_cpr_start);
		if (status < 0)
			CMDQ_ERR("Allocate delay CPR failed !!");
	}
	/* allocate shared memory */
	gCmdqContext.hSecSharedMem = NULL;
#ifdef CMDQ_SECURE_PATH_SUPPORT
	cmdq_sec_create_shared_memory(&(gCmdqContext.hSecSharedMem),
		PAGE_SIZE);
#endif

#if 0
	cmdqCoreRegisterDebugRegDumpCB(testcase_regdump_begin,
		testcase_regdump_end);
#endif

	/* Initialize MET for statistics */
	/* note that we don't need to uninit it. */
	CMDQ_PROF_INIT();
#ifdef CMDQ_PROFILE_MMP
	cmdq_mmp_init();
#endif
	/* Initialize secure path context */
	cmdqSecInitialize();
	/* Initialize test case structure */
	cmdq_test_init_setting();
	/* Initialize Resource via device tree */
	cmdq_dev_init_resource(cmdq_core_init_resource);

	/* Initialize Features */
	gCmdqContext.features[CMDQ_FEATURE_SRAM_SHARE] = 1;

	/* MDP initialization setting */
	cmdq_mdp_get_func()->mdpInitialSet();

	g_cmdq_consume_again = false;
	g_delay_thread_inited = false;
	cmdq_core_create_buffer_pool();

	return 0;
}

s32 cmdqCoreLateInitialize(void)
{
	s32 status = 0;

#ifdef CMDQ_SECURE_PATH_SUPPORT
	struct task_struct *open_th =
		kthread_run(cmdq_sec_init_allocate_resource_thread, NULL,
			"cmdq_WSM_init");
	if (IS_ERR(open_th)) {
		CMDQ_LOG("%s, init kthread_run failed!\n", __func__);
		status = -EFAULT;
	}
#endif

	if (!g_delay_thread_inited) {
		status = cmdq_delay_thread_init();
		if (status < 0)
			CMDQ_ERR("delay init failed in late init!\n");
	}

	return status;
}

void cmdqCoreDeInitialize(void)
{
	struct TaskStruct *pTask = NULL;
	struct list_head *p;
	int index;
	struct list_head *lists[] = {
		&gCmdqContext.taskFreeList,
		&gCmdqContext.taskActiveList,
		&gCmdqContext.taskWaitList
	};

	/* directly destroy the auto release WQ
	 * since we're going to release tasks anyway.
	 */
	destroy_workqueue(gCmdqContext.taskAutoReleaseWQ);
	gCmdqContext.taskAutoReleaseWQ = NULL;

	destroy_workqueue(gCmdqContext.taskConsumeWQ);
	gCmdqContext.taskConsumeWQ = NULL;

	destroy_workqueue(gCmdqContext.resourceCheckWQ);
	gCmdqContext.resourceCheckWQ = NULL;

	cmdq_core_destroy_thread_work_queue();

	/* release all tasks in both list */
	for (index = 0; index < ARRAY_SIZE(lists); ++index) {
		list_for_each(p, lists[index]) {
			CMDQ_PROF_MUTEX_LOCK(gCmdqTaskMutex, deinitialize);

			pTask = list_entry(p, struct TaskStruct, listEntry);

			/* free allocated DMA buffer */
			cmdq_task_free_task_command_buffer(pTask);
			kmem_cache_free(gCmdqContext.taskCache, pTask);
			list_del(p);

			CMDQ_PROF_MUTEX_UNLOCK(gCmdqTaskMutex, deinitialize);
		}
	}

	/* check if there are dangling write addresses. */
	if (!list_empty(&gCmdqContext.writeAddrList)) {
		/* there are unreleased write buffer, raise AEE */
		CMDQ_AEE("CMDQ", "there are unreleased write buffer");
	}

	kmem_cache_destroy(gCmdqContext.taskCache);
	gCmdqContext.taskCache = NULL;

	/* Delay thread de-initialization */
	cmdq_delay_thread_deinit();

	/* Deinitialize secure path context */
	cmdqSecDeInitialize();

	cmdq_core_destroy_buffer_pool();

	/* Deinitialize MDP */
	cmdq_mdp_deinit_pmqos();

	kfree(g_dts_setting.prefetch_size);
	g_dts_setting.prefetch_size = NULL;
	kfree(gCmdqContext.thread);
	gCmdqContext.thread = NULL;
	kfree(gCmdWaitQueue);
	gCmdWaitQueue = NULL;
}

int cmdqCoreAllocWriteAddress(u32 count, dma_addr_t *paStart)
{
	unsigned long flagsWriteAddr = 0L;
	struct WriteAddrStruct *pWriteAddr = NULL;
	int status = 0;

	CMDQ_VERBOSE("ALLOC: line %d\n", __LINE__);

	do {
		if (!paStart) {
			CMDQ_ERR("invalid output argument\n");
			status = -EINVAL;
			break;
		}
		*paStart = 0;

		if (!count || count > CMDQ_MAX_WRITE_ADDR_COUNT) {
			CMDQ_ERR("invalid alloc write addr count:%u max:%u\n",
				count, (u32)CMDQ_MAX_WRITE_ADDR_COUNT);
			status = -EINVAL;
			break;
		}

		CMDQ_VERBOSE("ALLOC: line %d\n", __LINE__);

		pWriteAddr = kzalloc(sizeof(struct WriteAddrStruct),
			GFP_KERNEL);
		if (!pWriteAddr) {
			CMDQ_ERR("failed to alloc WriteAddrStruct\n");
			status = -ENOMEM;
			break;
		}
		memset(pWriteAddr, 0, sizeof(struct WriteAddrStruct));

		CMDQ_VERBOSE("ALLOC: line %d\n", __LINE__);

		pWriteAddr->count = count;
		pWriteAddr->va =
			cmdq_core_alloc_hw_buffer(cmdq_dev_get(),
			count * sizeof(u32), &(pWriteAddr->pa), GFP_KERNEL);
		if (current)
			pWriteAddr->user = current->pid;

		CMDQ_VERBOSE("ALLOC: line %d\n", __LINE__);

		if (!pWriteAddr->va) {
			CMDQ_ERR("failed to alloc write buffer\n");
			status = -ENOMEM;
			break;
		}

		CMDQ_VERBOSE("ALLOC: line %d\n", __LINE__);

		/* clear buffer content */
		do {
			u32 *pInt = (u32 *) pWriteAddr->va;
			int i = 0;

			for (i = 0; i < count; ++i) {
				*(pInt + i) = 0xcdcdabab;
				/* make sure instructions are really in DRAM */
				mb();
				/* make sure instructions are really in DRAM */
				smp_mb();
			}
		} while (0);

		/* assign output pa */
		*paStart = pWriteAddr->pa;

		CMDQ_PROF_SPIN_LOCK(gCmdqWriteAddrLock, flagsWriteAddr,
			alloc_write);
		list_add_tail(&(pWriteAddr->list_node),
			&gCmdqContext.writeAddrList);
		CMDQ_PROF_SPIN_UNLOCK(gCmdqWriteAddrLock, flagsWriteAddr,
			alloc_write);

		status = 0;

	} while (0);

	if (status != 0) {
		/* release resources */
		if (pWriteAddr && pWriteAddr->va) {
			cmdq_core_free_hw_buffer(cmdq_dev_get(),
				sizeof(u32) * pWriteAddr->count,
				pWriteAddr->va, pWriteAddr->pa);
			memset(pWriteAddr, 0,
				sizeof(struct WriteAddrStruct));
		}

		kfree(pWriteAddr);
		pWriteAddr = NULL;
	}

	CMDQ_VERBOSE("ALLOC: line %d\n", __LINE__);

	return status;
}

u32 cmdqCoreReadWriteAddress(dma_addr_t pa)
{
	struct list_head *p = NULL;
	struct WriteAddrStruct *pWriteAddr = NULL;
	s32 offset = 0;
	u32 value = 0;
	unsigned long flagsWriteAddr = 0L;

	/* search for the entry */
	CMDQ_PROF_SPIN_LOCK(gCmdqWriteAddrLock, flagsWriteAddr,
		read_write_addr);
	list_for_each(p, &gCmdqContext.writeAddrList) {
		pWriteAddr = list_entry(p, struct WriteAddrStruct, list_node);
		if (!pWriteAddr)
			continue;

		offset = pa - pWriteAddr->pa;

		if (offset >= 0 && (offset / sizeof(u32)) <
			pWriteAddr->count) {
			CMDQ_VERBOSE(
				"%s input:%pa got offset:%d va:%p pa_start:%pa\n",
				__func__, &pa, offset,
				pWriteAddr->va + offset, &pWriteAddr->pa);
			value = *((u32 *)(pWriteAddr->va + offset));
			CMDQ_VERBOSE(
				"%s found offset:%d va:%p value:0x%08x\n",
				__func__, offset, pWriteAddr->va + offset,
				value);
			break;
		}
	}
	CMDQ_PROF_SPIN_UNLOCK(gCmdqWriteAddrLock, flagsWriteAddr,
		read_write_addr);

	return value;
}

u32 cmdqCoreWriteWriteAddress(dma_addr_t pa, u32 value)
{
	struct list_head *p = NULL;
	struct WriteAddrStruct *pWriteAddr = NULL;
	s32 offset = 0;
	unsigned long flagsWriteAddr = 0L;
	char long_msg[CMDQ_LONGSTRING_MAX];
	u32 msg_offset;
	s32 msg_max_size;

	/* search for the entry */
	CMDQ_PROF_SPIN_LOCK(gCmdqWriteAddrLock, flagsWriteAddr,
		write_write_addr);
	list_for_each(p, &gCmdqContext.writeAddrList) {
		pWriteAddr = list_entry(p, struct WriteAddrStruct, list_node);
		if (!pWriteAddr)
			continue;

		offset = pa - pWriteAddr->pa;

		/* note it is 64 bit length for u32 variable in 64 bit kernel
		 * use sizeof(u_log) to check valid offset range
		 */
		if (offset >= 0 && (offset / sizeof(unsigned long)) <
			pWriteAddr->count) {
			cmdq_long_string_init(false, long_msg, &msg_offset,
				&msg_max_size);
			cmdq_long_string(long_msg, &msg_offset, &msg_max_size,
				"%s input:%pa", __func__, &pa);
			cmdq_long_string(long_msg, &msg_offset, &msg_max_size,
				" got offset:%d va:%p pa_start:%pa value:0x%08x\n",
				offset, pWriteAddr->va + offset,
				&pWriteAddr->pa, value);
			CMDQ_VERBOSE("%s", long_msg);

			*((u32 *)(pWriteAddr->va + offset)) = value;
			break;
		}
	}
	CMDQ_PROF_SPIN_UNLOCK(gCmdqWriteAddrLock, flagsWriteAddr,
		write_write_addr);

	return value;
}


int cmdqCoreFreeWriteAddress(dma_addr_t paStart)
{
	struct list_head *p, *n = NULL;
	struct WriteAddrStruct *pWriteAddr = NULL;
	bool foundEntry;
	unsigned long flagsWriteAddr = 0L;

	foundEntry = false;

	/* search for the entry */
	CMDQ_PROF_SPIN_LOCK(gCmdqWriteAddrLock, flagsWriteAddr,
		free_write_addr);
	list_for_each_safe(p, n, &gCmdqContext.writeAddrList) {
		pWriteAddr = list_entry(p, struct WriteAddrStruct, list_node);
		if (pWriteAddr && pWriteAddr->pa == paStart) {
			list_del(&(pWriteAddr->list_node));
			foundEntry = true;
			break;
		}
	}
	CMDQ_PROF_SPIN_UNLOCK(gCmdqWriteAddrLock, flagsWriteAddr,
		free_write_addr);

	/* when list is not empty, we always get a entry
	 * even we don't found a valid entry
	 * use foundEntry to confirm search result
	 */
	if (!foundEntry) {
		CMDQ_ERR("%s no matching entry, paStart:%pa\n",
			__func__, &paStart);
		return -EINVAL;
	}

	/* release resources */
	if (pWriteAddr->va) {
		cmdq_core_free_hw_buffer(cmdq_dev_get(),
			sizeof(u32) * pWriteAddr->count,
			pWriteAddr->va, pWriteAddr->pa);
		memset(pWriteAddr, 0xda, sizeof(struct WriteAddrStruct));
	}

	kfree(pWriteAddr);
	pWriteAddr = NULL;

	return 0;
}

s32 cmdqCoreDebugRegDumpBegin(u32 taskID, u32 *regCount, u32 **regAddress)
{
	if (!gCmdqDebugCallback.beginDebugRegDump) {
		CMDQ_ERR("beginDebugRegDump not registered\n");
		return -EFAULT;
	}

	return gCmdqDebugCallback.beginDebugRegDump(taskID, regCount,
		regAddress);
}

s32 cmdqCoreDebugRegDumpEnd(u32 taskID, u32 regCount, u32 *regValues)
{
	if (!gCmdqDebugCallback.endDebugRegDump) {
		CMDQ_ERR("endDebugRegDump not registered\n");
		return -EFAULT;
	}

	return gCmdqDebugCallback.endDebugRegDump(taskID, regCount, regValues);
}

void cmdq_core_set_log_level(const s32 value)
{
	if (value == CMDQ_LOG_LEVEL_NORMAL) {
		/* Only print CMDQ ERR and CMDQ LOG */
		gCmdqContext.logLevel = CMDQ_LOG_LEVEL_NORMAL;
	} else if (value < CMDQ_LOG_LEVEL_MAX) {
		/* Modify log level */
		gCmdqContext.logLevel = (1 << value);
	}
}

bool cmdq_core_should_print_msg(void)
{
	bool logLevel = (gCmdqContext.logLevel & (1 << CMDQ_LOG_LEVEL_MSG)) ?
		(1) : (0);
	return logLevel;
}

bool cmdq_core_should_full_error(void)
{
	bool logLevel = (gCmdqContext.logLevel &
		(1 << CMDQ_LOG_LEVEL_FULL_ERROR)) ? (1) : (0);
	return logLevel;
}

s32 cmdq_core_profile_enabled(void)
{
	return gCmdqContext.enableProfile;
}

void cmdq_long_string_init(bool force, char *buf, u32 *offset, s32 *max_size)
{
	buf[0] = '\0';
	*offset = 0;
	if (force || cmdq_core_should_print_msg())
		*max_size = CMDQ_LONGSTRING_MAX - 1;
	else
		*max_size = 0;
}

void cmdq_long_string(char *buf, u32 *offset, s32 *max_size,
			const char *string, ...)
{
	int msg_len;
	va_list arg_ptr;
	char *buffer;

	if (*max_size <= 0)
		return;

	va_start(arg_ptr, string);
	buffer = buf + (*offset);
	msg_len = vsnprintf(buffer, *max_size, string, arg_ptr);
	*max_size -= msg_len;
	if (*max_size < 0)
		*max_size = 0;
	*offset += msg_len;
	va_end(arg_ptr);
}

void cmdq_core_turnon_first_dump(const struct TaskStruct *pTask)
{
#ifdef CMDQ_DUMP_FIRSTERROR
	if (gCmdqFirstError.cmdqCount || !pTask)
		return;

	gCmdqFirstError.flag = true;
	/* save kernel time, pid, and caller name */
	gCmdqFirstError.callerPid = pTask->callerPid;
	snprintf(gCmdqFirstError.callerName, TASK_COMM_LEN, "%s",
		pTask->callerName);
	gCmdqFirstError.savetime = sched_clock();
	do_gettimeofday(&gCmdqFirstError.savetv);
#endif
}

void cmdq_core_turnoff_first_dump(void)
{
#ifdef CMDQ_DUMP_FIRSTERROR
	gCmdqFirstError.flag = false;
#endif
}

void cmdq_core_reset_first_dump(void)
{
#ifdef CMDQ_DUMP_FIRSTERROR
	memset(&gCmdqFirstError, 0, sizeof(gCmdqFirstError));
	gCmdqFirstError.cmdqMaxSize = CMDQ_MAX_FIRSTERROR;
	gCmdqContext.errNum = 0;
#endif
}

s32 cmdq_core_save_first_dump(const char *string, ...)
{
#ifdef CMDQ_DUMP_FIRSTERROR
	int logLen;
	va_list argptr;
	char *pBuffer;

	if (gCmdqFirstError.flag == false)
		return -EFAULT;

	va_start(argptr, string);
	pBuffer = gCmdqFirstError.cmdqString + gCmdqFirstError.cmdqCount;
	logLen = vsnprintf(pBuffer, gCmdqFirstError.cmdqMaxSize, string,
		argptr);
	gCmdqFirstError.cmdqMaxSize -= logLen;
	gCmdqFirstError.cmdqCount += logLen;

	if (gCmdqFirstError.cmdqMaxSize <= 0) {
		gCmdqFirstError.flag = false;
		CMDQ_LOG("[ERR] Error0 dump saving buffer is full\n");
	}
	va_end(argptr);
	return 0;
#else
	return -EFAULT;
#endif
}

#ifdef CMDQ_DUMP_FIRSTERROR
void cmdq_core_hex_dump_to_buffer(const void *buf, size_t len, int rowsize,
	int groupsize, char *linebuf, size_t linebuflen)
{
	const u8 *ptr = buf;
	u8 ch;
	int j, lx = 0;

	if (rowsize != 16 && rowsize != 32)
		rowsize = 16;

	if (!len)
		goto nil;
	if (len > rowsize)	/* limit to one line at a time */
		len = rowsize;
	if ((len % groupsize) != 0)	/* no mixed size output */
		groupsize = 1;

	switch (groupsize) {
	case 8:{
			const u64 *ptr8 = buf;
			int ngroups = len / groupsize;

			for (j = 0; j < ngroups; j++)
				lx += scnprintf(linebuf + lx, linebuflen - lx,
				"%s%16.16llx", j ? " " : "",
				(unsigned long long)*(ptr8 + j));
			break;
		}

	case 4:{
			const u32 *ptr4 = buf;
			int ngroups = len / groupsize;

			for (j = 0; j < ngroups; j++)
				lx += scnprintf(linebuf + lx, linebuflen - lx,
				"%s%8.8x", j ? " " : "", *(ptr4 + j));
			break;
		}

	case 2:{
			const u16 *ptr2 = buf;
			int ngroups = len / groupsize;

			for (j = 0; j < ngroups; j++)
				lx += scnprintf(linebuf + lx, linebuflen - lx,
				"%s%4.4x", j ? " " : "", *(ptr2 + j));
			break;
		}

	default:
		for (j = 0; (j < len) && (lx + 3) <= linebuflen; j++) {
			ch = ptr[j];
			linebuf[lx++] = hex_asc_hi(ch);
			linebuf[lx++] = hex_asc_lo(ch);
			linebuf[lx++] = ' ';
		}
		if (j)
			lx--;
		break;
	}
nil:
	linebuf[lx++] = '\0';
}
#endif

void cmdq_core_save_hex_first_dump(const char *prefix_str,
	int rowsize, int groupsize, const void *buf, size_t len)
{
#ifdef CMDQ_DUMP_FIRSTERROR
	const u8 *ptr = buf;
	int i, linelen, remaining = len;
	unsigned char linebuf[32 * 3 + 2 + 32 + 1];
	int logLen;
	char *pBuffer;

	if (gCmdqFirstError.flag == false)
		return;

	if (rowsize != 16 && rowsize != 32)
		rowsize = 16;

	for (i = 0; i < len; i += rowsize) {
		linelen = min(remaining, rowsize);
		remaining -= rowsize;

		cmdq_core_hex_dump_to_buffer(ptr + i, linelen, rowsize,
			groupsize, linebuf, sizeof(linebuf));

		pBuffer = gCmdqFirstError.cmdqString +
			gCmdqFirstError.cmdqCount;
		logLen = snprintf(pBuffer, gCmdqFirstError.cmdqMaxSize,
			"%s%p:%s\n", prefix_str, ptr + i, linebuf);
		gCmdqFirstError.cmdqMaxSize -= logLen;
		gCmdqFirstError.cmdqCount += logLen;

		if (gCmdqFirstError.cmdqMaxSize <= 0) {
			gCmdqFirstError.flag = false;
			CMDQ_LOG("[ERR] Error0 dump saving buffer is full\n");
		}
	}
#endif
}

static void cmdq_core_lock_res_impl(struct ResourceUnitStruct *res,
	u64 engine_flag, bool from_notify)
{
	mutex_lock(&gCmdqResourceMutex);

	/* find matched engine */
	if (from_notify)
		res->notify = sched_clock();
	else
		res->lock = sched_clock();

	if (!res->used) {
		/* First time used */
		s32 status;

		CMDQ_MSG(
			"[Res] Lock resource with engine:0x%llx fromNotify:%d callback release\n",
			engine_flag, from_notify);

		res->used = true;
		if (!res->releaseCB) {
			CMDQ_LOG("[Res] release CB func is NULL, event:%d\n",
				res->lockEvent);
		} else {
			CmdqResourceReleaseCB cb_func = res->releaseCB;

			/* release mutex before callback */
			mutex_unlock(&gCmdqResourceMutex);
			status = cb_func(res->lockEvent);
			mutex_lock(&gCmdqResourceMutex);

			if (status < 0) {
				/* Error status print */
				CMDQ_ERR(
				"[Res] release CB (%d) return fail:%d\n",
					res->lockEvent, status);
			}
		}
	} else {
		/* Cancel previous delay task if existed */
		if (res->delaying) {
			res->delaying = false;
			cancel_delayed_work(&res->delayCheckWork);
		}
	}
	mutex_unlock(&gCmdqResourceMutex);
}

/* Use CMDQ as Resource Manager */
void cmdqCoreLockResource(u64 engineFlag, bool fromNotify)
{
	struct ResourceUnitStruct *res = NULL;

	if (!cmdq_core_is_feature_on(CMDQ_FEATURE_SRAM_SHARE))
		return;

	list_for_each_entry(res, &gCmdqContext.resourceList, list_entry) {
		if (engineFlag & res->engine_flag)
			cmdq_core_lock_res_impl(res, engineFlag, fromNotify);
	}
}

bool cmdqCoreAcquireResource(enum CMDQ_EVENT_ENUM resourceEvent,
	u64 *engine_flag_out)
{
	struct ResourceUnitStruct *res = NULL;
	bool result = false;

	if (!cmdq_core_is_feature_on(CMDQ_FEATURE_SRAM_SHARE))
		return result;

	CMDQ_MSG("[Res] Acquire resource with event:%d\n", resourceEvent);
	list_for_each_entry(res, &gCmdqContext.resourceList,
		list_entry) {
		if (resourceEvent == res->lockEvent) {
			mutex_lock(&gCmdqResourceMutex);
			/* find matched resource */
			result = !res->used;
			if (result && !res->lend) {
				CMDQ_MSG(
					"[Res] Acquire successfully, event:%d\n",
					resourceEvent);
				cmdqCoreClearEvent(resourceEvent);
				res->acquire = sched_clock();
				res->lend = true;
				*engine_flag_out |= res->engine_flag;
			}
			mutex_unlock(&gCmdqResourceMutex);
			break;
		}
	}
	return result;
}

void cmdqCoreReleaseResource(enum CMDQ_EVENT_ENUM resourceEvent,
	u64 *engine_flag_out)
{
	struct ResourceUnitStruct *res = NULL;

	if (!cmdq_core_is_feature_on(CMDQ_FEATURE_SRAM_SHARE))
		return;

	CMDQ_MSG("[Res] Release resource with event:%d\n", resourceEvent);
	list_for_each_entry(res, &gCmdqContext.resourceList, list_entry) {
		if (resourceEvent == res->lockEvent) {
			mutex_lock(&gCmdqResourceMutex);
			/* find matched resource */
			if (res->lend) {
				res->release = sched_clock();
				res->lend = false;
				*engine_flag_out |= res->engine_flag;
			}
			mutex_unlock(&gCmdqResourceMutex);
			break;
		}
	}
}

void cmdqCoreSetResourceCallback(enum CMDQ_EVENT_ENUM resourceEvent,
	CmdqResourceAvailableCB resourceAvailable,
	CmdqResourceReleaseCB resourceRelease)
{
	struct ResourceUnitStruct *res = NULL;

	CMDQ_MSG("[Res] Set resource callback with event:%d\n",
		resourceEvent);
	list_for_each_entry(res, &gCmdqContext.resourceList, list_entry) {
		if (resourceEvent == res->lockEvent) {
			CMDQ_MSG("[Res] Set resource callback ok!\n");
			mutex_lock(&gCmdqResourceMutex);
			/* find matched resource */
			res->availableCB = resourceAvailable;
			res->releaseCB = resourceRelease;
			mutex_unlock(&gCmdqResourceMutex);
			break;
		}
	}
}

/* Implement dynamic feature configuration */
void cmdq_core_dump_feature(void)
{
	int index;
	static const char *const FEATURE_STRING[] = {
		FOREACH_FEATURE(GENERATE_STRING)
	};

	/* dump all feature status */
	for (index = 0; index < CMDQ_FEATURE_TYPE_MAX; index++) {
		CMDQ_LOG("[Feature] %02d	%s\t\t%d\n",
			index, FEATURE_STRING[index],
			cmdq_core_get_feature(index));
	}
}

void cmdq_core_set_feature(enum CMDQ_FEATURE_TYPE_ENUM featureOption,
	u32 value)
{
	if (atomic_read(&gCmdqThreadUsage) == 0)
		CMDQ_ERR("[FO] Try to set feature (%d) while running!\n",
		featureOption);

	if (featureOption >= CMDQ_FEATURE_TYPE_MAX) {
		CMDQ_ERR("[FO] Set feature invalid:%d\n", featureOption);
	} else {
		CMDQ_LOG("[FO] Set feature:%d, with value:%d\n",
			featureOption, value);
		gCmdqContext.features[featureOption] = value;
	}
}

u32 cmdq_core_get_feature(enum CMDQ_FEATURE_TYPE_ENUM featureOption)
{
	if (featureOption >= CMDQ_FEATURE_TYPE_MAX) {
		CMDQ_ERR("[FO] Set feature invalid:%d\n", featureOption);
		return CMDQ_FEATURE_OFF_VALUE;
	}
	return gCmdqContext.features[featureOption];
}

bool cmdq_core_is_feature_on(enum CMDQ_FEATURE_TYPE_ENUM featureOption)
{
	return cmdq_core_get_feature(featureOption) != CMDQ_FEATURE_OFF_VALUE;
}

struct StressContextStruct *cmdq_core_get_stress_context(void)
{
	return &gStressContext;
}

void cmdq_core_clean_stress_context(void)
{
	memset(&gStressContext, 0, sizeof(gStressContext));
}

struct cmdq_dts_setting *cmdq_core_get_dts_setting(void)
{
	return &g_dts_setting;
}

