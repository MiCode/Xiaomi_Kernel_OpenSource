// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
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
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>
#include <linux/atomic.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/memblock.h>
#include <linux/memory.h>
#include <linux/ftrace.h>
#include <sched/sched.h>


#ifdef CMDQ_MET_READY
#include <linux/met_drv.h>
#endif
#include <linux/seq_file.h>
#include <linux/kthread.h>
#ifdef CMDQ_OF_SUPPORT
#define MMSYS_CONFIG_BASE cmdq_dev_get_module_base_VA_MMSYS_CONFIG()
#else
#include <mach/mt_reg_base.h>
#include <mach/mt_irq.h>
#include "ddp_reg.h"
#endif
/* #include <mt-plat/mtk_lpae.h> */


/* #define CMDQ_PROFILE_COMMAND_TRIGGER_LOOP */
/* #define CMDQ_APPEND_WITHOUT_SUSPEND */
/* #define CMDQ_ENABLE_BUS_ULTRA */

#define CMDQ_GET_COOKIE_CNT(thread) \
	(CMDQ_REG_GET32(CMDQ_THR_EXEC_CNT(thread)) & CMDQ_MAX_COOKIE_VALUE)
#define CMDQ_SYNC_TOKEN_APPEND_THR(id)     (CMDQ_SYNC_TOKEN_APPEND_THR0 + id)

/* use mutex because we don't access task list in IRQ */
/* and we may allocate memory when create list items */
static DEFINE_MUTEX(gCmdqTaskMutex);
static DEFINE_MUTEX(gCmdqSaveBufferMutex);
/* static DEFINE_MUTEX(gCmdqWriteAddrMutex); */
static DEFINE_SPINLOCK(gCmdqWriteAddrLock);

#if defined(CMDQ_SECURE_PATH_SUPPORT) && !defined(CMDQ_SECURE_PATH_NORMAL_IRQ)
/* ensure atomic start/stop notify loop*/
static DEFINE_MUTEX(gCmdqNotifyLoopMutex);
#endif

/* t-base(secure OS) doesn't allow entry secure world in ISR context, */
/* but M4U has to restore lab0 register in */
/* secure world when enable lab 0 first time. */
/* HACK: use m4u_larb0_enable() to lab0 on/off to */
/* ensure larb0 restore and clock on/off sequence */
/* HACK: use gCmdqClockMutex to ensure acquire/release */
/* thread and enable/disable clock sequence */
static DEFINE_MUTEX(gCmdqClockMutex);

/* These may access in IRQ so use spin lock. */
static DEFINE_SPINLOCK(gCmdqThreadLock);
static atomic_t gCmdqThreadUsage;
static atomic_t gSMIThreadUsage;
static bool gCmdqSuspended;
static DEFINE_SPINLOCK(gCmdqExecLock);
static DEFINE_SPINLOCK(gCmdqRecordLock);
static DEFINE_MUTEX(gCmdqResourceMutex);

/* The main context structure */
/* task done notification */
static wait_queue_head_t gCmdWaitQueue[CMDQ_MAX_THREAD_COUNT];
/* thread acquire notification */
static wait_queue_head_t gCmdqThreadDispatchQueue;

static struct ContextStruct gCmdqContext;
static struct CmdqCBkStruct gCmdqGroupCallback[CMDQ_MAX_GROUP_COUNT];
static struct CmdqDebugCBkStruct gCmdqDebugCallback;
static struct cmdq_dts_setting g_dts_setting;

#ifdef CMDQ_DUMP_FIRSTERROR
struct DumpFirstErrorStruct gCmdqFirstError;
#endif

static struct DumpCommandBufferStruct gCmdqBufferDump;

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

static const uint64_t gCmdqEngineGroupBits[CMDQ_MAX_GROUP_COUNT] = {
	CMDQ_ENG_ISP_GROUP_BITS,
	CMDQ_ENG_MDP_GROUP_BITS,
	CMDQ_ENG_DISP_GROUP_BITS,
	CMDQ_ENG_JPEG_GROUP_BITS,
	CMDQ_ENG_VENC_GROUP_BITS,
	CMDQ_ENG_DPE_GROUP_BITS
};

static struct cmdqDTSDataStruct gCmdqDtsData;

/* task memory usage monitor */
static DEFINE_SPINLOCK(gCmdqMemMonitorLock);
static struct MemRecordStruct g_cmdq_mem_records[9];
static struct MemMonitorStruct g_cmdq_mem_monitor;

static struct StressContextStruct gStressContext;

uint32_t cmdq_core_max_task_in_thread(int32_t thread)
{
	int32_t maxTaskNUM = CMDQ_MAX_TASK_IN_THREAD;

#ifdef CMDQ_SECURE_PATH_SUPPORT
	if (cmdq_get_func()->isSecureThread(thread) == true)
		maxTaskNUM = CMDQ_MAX_TASK_IN_SECURE_THREAD;
#endif
	return maxTaskNUM;
}

int32_t cmdq_core_suspend_HW_thread(int32_t thread, uint32_t lineNum)
{
	int32_t loop = 0;
	uint32_t enabled = 0;

	if (thread == CMDQ_INVALID_THREAD) {
		CMDQ_ERR("suspend invalid thread\n");
		return -EFAULT;
	}

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->thread_suspend,
		MMPROFILE_FLAG_PULSE, thread, lineNum);
	/* write suspend bit */
	CMDQ_REG_SET32(CMDQ_THR_SUSPEND_TASK(thread), 0x01);

	/* check if the thread is already disabled. */
	/* if already disabled, treat as suspend */
	/* successful but print error log */
	enabled = CMDQ_REG_GET32(CMDQ_THR_ENABLE_TASK(thread));
	if ((0x01 & enabled) == 0) {
		CMDQ_LOG("thread %d suspend not effective, enable=%d\n",
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

static inline void cmdq_core_resume_HW_thread(int32_t thread)
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

static inline int32_t cmdq_core_reset_HW_thread(int32_t thread)
{
	int32_t loop = 0;

	CMDQ_MSG("Reset HW thread(%d)\n", thread);

	CMDQ_REG_SET32(CMDQ_THR_WARM_RESET(thread), 0x01);
	while (0x1 == (CMDQ_REG_GET32(CMDQ_THR_WARM_RESET(thread)))) {
		if (loop > CMDQ_MAX_LOOP_COUNT) {
			CMDQ_AEE("CMDQ", "Reset HW thread %d failed\n", thread);
			return -EFAULT;
		}
		loop++;
	}

	CMDQ_REG_SET32(CMDQ_THR_SLOT_CYCLES, 0x3200);
	return 0;
}

static inline int32_t cmdq_core_disable_HW_thread(int32_t thread)
{
	cmdq_core_reset_HW_thread(thread);

	/* Disable thread */
	CMDQ_MSG("Disable HW thread(%d)\n", thread);
	CMDQ_REG_SET32(CMDQ_THR_ENABLE_TASK(thread), 0x00);
	return 0;
}

/* Use CMDQ as Resource Manager */
void cmdq_core_unlock_resource(struct work_struct *workItem)
{
	struct ResourceUnitStruct *pResource = NULL;
	struct delayed_work *delayedWorkItem = NULL;
	int32_t status = 0;

	delayedWorkItem = container_of(workItem, struct delayed_work, work);
	pResource = container_of(delayedWorkItem,
		struct ResourceUnitStruct, delayCheckWork);

	mutex_lock(&gCmdqResourceMutex);

	CMDQ_MSG("[Res] unlock resource with engine: 0x%016llx\n",
		pResource->engine);
	if (pResource->used && pResource->delaying) {
		pResource->unlock = sched_clock();
		pResource->used = false;
		pResource->delaying = false;
		/* delay time is reached and unlock resource */
		if (pResource->availableCB == NULL) {
			/* print error message */
			CMDQ_LOG("[Res]: available CB func is NULL, event:%d\n",
				pResource->lockEvent);
		} else {
			CmdqResourceAvailableCB cb_func =
				pResource->availableCB;

			/* before call callback, release lock at first */
			mutex_unlock(&gCmdqResourceMutex);
			status = cb_func(pResource->lockEvent);
			mutex_lock(&gCmdqResourceMutex);

			if (status < 0) {
				/* Error status print */
				CMDQ_ERR("[Res]: avail CB(%d)return fail:%d\n",
					pResource->lockEvent, status);
			}
		}
	}
	mutex_unlock(&gCmdqResourceMutex);
}

void cmdq_core_init_resource(uint32_t engineFlag,
	enum CMDQ_EVENT_ENUM resourceEvent)
{
	struct ResourceUnitStruct *pResource;

	pResource = kzalloc(sizeof(struct ResourceUnitStruct), GFP_KERNEL);
	if (pResource) {
		pResource->engine = (1LL << engineFlag);
		pResource->lockEvent = resourceEvent;
		INIT_DELAYED_WORK(&pResource->delayCheckWork,
			cmdq_core_unlock_resource);
		INIT_LIST_HEAD(&(pResource->listEntry));
		list_add_tail(&(pResource->listEntry),
			&gCmdqContext.resourceList);
	}
}

/* engineFlag: task original engineFlag */
/* enginesNotUsed: flag which indicate Not Used engine after release task */
void cmdq_core_delay_check_unlock(uint64_t engineFlag,
	const uint64_t enginesNotUsed)
{
	/* Check engine in enginesNotUsed */
	struct ResourceUnitStruct *pResource = NULL;
	struct list_head *p = NULL;

	if (cmdq_core_is_feature_off(CMDQ_FEATURE_SRAM_SHARE))
		return;

	list_for_each(p, &gCmdqContext.resourceList) {
		pResource = list_entry(p, struct ResourceUnitStruct, listEntry);
		if (enginesNotUsed & pResource->engine) {
			mutex_lock(&gCmdqResourceMutex);
			/* find matched engine become not used*/
			if (!pResource->used) {
				/* resource is not used but */
				/* we got engine is released! */
				/* log as error and still continue */
				CMDQ_ERR("[Res]:eng delay not use:0x%016llx\n",
					pResource->engine);
			}

			/* Cancel previous delay task if existed */
			if (pResource->delaying) {
				pResource->delaying = false;
				cancel_delayed_work(&pResource->delayCheckWork);
			}

			/* Start a new delay task */
			queue_delayed_work(gCmdqContext.resourceCheckWQ,
				&pResource->delayCheckWork,
				CMDQ_DELAY_RELEASE_RESOURCE_MS);
			pResource->delay = sched_clock();
			pResource->delaying = true;
			mutex_unlock(&gCmdqResourceMutex);
		}
	}
}

uint32_t cmdq_core_get_thread_prefetch_size(int32_t thread)
{
	if (thread < 0 || thread >= CMDQ_MAX_THREAD_COUNT)
		return 0;

	return g_dts_setting.prefetch_size[thread];
}

struct cmdqDTSDataStruct *cmdq_core_get_whole_DTS_Data(void)
{
	return &gCmdqDtsData;
}

void cmdq_core_init_DTS_data(void)
{
	uint32_t i;

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

void cmdq_core_set_event_table(enum CMDQ_EVENT_ENUM event, const int32_t value)
{
	if (event >= 0 && event < CMDQ_SYNC_TOKEN_MAX)
		gCmdqDtsData.eventTable[event] = value;
}

int32_t cmdq_core_get_event_value(enum CMDQ_EVENT_ENUM event)
{
	if (event < 0 || event >= CMDQ_SYNC_TOKEN_MAX)
		return -EINVAL;

	return gCmdqDtsData.eventTable[event];
}

int32_t cmdq_core_reverse_event_ENUM(const uint32_t value)
{
	uint32_t eventENUM = CMDQ_SYNC_TOKEN_INVALID;
	uint32_t i;

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
		if (pTask == NULL) {
			isValid = false;
			break;
		}

		if (pTask->taskState == TASK_STATE_IDLE ||
			pTask->thread == CMDQ_INVALID_THREAD ||
			pTask->pCMDEnd == NULL ||
			list_empty(&pTask->cmd_buffer_list)) {
			/* check CMDQ task's contain */
			isValid = false;
		}
	} while (0);

	return isValid;
}

void cmdq_core_set_mem_monitor(bool enable)
{
	if (enable) {
		int i;

		/* here we clear all before new round monitor */
		spin_lock(&gCmdqMemMonitorLock);
		for (i = 0; i < ARRAY_SIZE(g_cmdq_mem_records); i++)
			g_cmdq_mem_records[i].task_count = 0;

		g_cmdq_mem_monitor.mem_current = 0;
		g_cmdq_mem_monitor.mem_phy_current = 0;
		g_cmdq_mem_monitor.mem_max_use = 0;
		g_cmdq_mem_monitor.mem_max_phy_use = 0;

		spin_unlock(&gCmdqMemMonitorLock);
		atomic_set(&g_cmdq_mem_monitor.monitor_mem_enable, 1);
	} else {
		/* simply disable without clear anything */
		atomic_set(&g_cmdq_mem_monitor.monitor_mem_enable, 0);
	}
}

void cmdq_core_dump_mem_monitor(void)
{
	int i;
	size_t last_range = 0;

	spin_lock(&gCmdqMemMonitorLock);

	CMDQ_LOG("[INFO] Max total command size: %zu max physical size: %zu\n",
		g_cmdq_mem_monitor.mem_max_use,
		g_cmdq_mem_monitor.mem_max_phy_use);
	CMDQ_LOG("page size: %u(0x%08x)\n",
		(uint32_t)PAGE_SIZE, (uint32_t)PAGE_SIZE);

	for (i = 0; i < ARRAY_SIZE(g_cmdq_mem_records); i++) {
		CMDQ_LOG("[INFO] Size range: %zu to %zu task count: %u\n",
		last_range,
		g_cmdq_mem_records[i].alloc_range,
		g_cmdq_mem_records[i].task_count);

		/* store current max as next start range */
		last_range = g_cmdq_mem_records[i].alloc_range;
	}

	spin_unlock(&gCmdqMemMonitorLock);
}

void cmdq_core_monitor_record_alloc(size_t size)
{
	int i;
	bool recorded = false;

	spin_lock(&gCmdqMemMonitorLock);

	for (i = 0; i < ARRAY_SIZE(g_cmdq_mem_records); i++) {
		if (size <= g_cmdq_mem_records[i].alloc_range) {
			g_cmdq_mem_records[i].task_count += 1;
			recorded = true;
			break;
		}
	}

	if (recorded == false) {
		CMDQ_LOG("[INFO]%s allocated size large than expect: %zu\n",
			__func__, size);
		g_cmdq_mem_records[
			ARRAY_SIZE(g_cmdq_mem_records)-1].task_count += 1;
	}

	g_cmdq_mem_monitor.mem_phy_current +=
		(size / PAGE_SIZE + (size % PAGE_SIZE > 0 ? 1 : 0)) * PAGE_SIZE;
	g_cmdq_mem_monitor.mem_current += size;
	if (g_cmdq_mem_monitor.mem_current > g_cmdq_mem_monitor.mem_max_use)
		g_cmdq_mem_monitor.mem_max_use = g_cmdq_mem_monitor.mem_current;
	if (g_cmdq_mem_monitor.mem_phy_current >
			g_cmdq_mem_monitor.mem_max_phy_use)
		g_cmdq_mem_monitor.mem_max_phy_use =
			g_cmdq_mem_monitor.mem_phy_current;

	spin_unlock(&gCmdqMemMonitorLock);
}

void cmdq_core_monitor_record_free(size_t size)
{
	size_t used_page_size =
		(size / PAGE_SIZE + (size % PAGE_SIZE > 0 ? 1 : 0)) * PAGE_SIZE;

	spin_lock(&gCmdqMemMonitorLock);

	if (g_cmdq_mem_monitor.mem_current > size &&
		g_cmdq_mem_monitor.mem_phy_current > used_page_size) {
		g_cmdq_mem_monitor.mem_current -= size;
		g_cmdq_mem_monitor.mem_phy_current -= used_page_size;
	} else {
		g_cmdq_mem_monitor.mem_current = 0;
		g_cmdq_mem_monitor.mem_phy_current = 0;
	}

	spin_unlock(&gCmdqMemMonitorLock);
}

void *cmdq_core_alloc_hw_buffer(struct device *dev, size_t size,
	dma_addr_t *dma_handle,
	const gfp_t flag)
{
	void *pVA;
	dma_addr_t PA;

	do {
		PA = 0;
		pVA = NULL;

		CMDQ_PROF_START(current->pid, __func__);

		pVA = dma_alloc_coherent(dev, size, &PA, flag);

		if (pVA != NULL &&
			atomic_read(&g_cmdq_mem_monitor.monitor_mem_enable)
				!= 0)
			cmdq_core_monitor_record_alloc(size);

		CMDQ_PROF_END(current->pid, __func__);
	} while (0);

	*dma_handle = PA;

	CMDQ_VERBOSE("%s, pVA:0x%p, PA:0x%pa, PAout:0x%pa\n", __func__, pVA,
		&PA, &(*dma_handle));

	return pVA;
}

void cmdq_core_free_hw_buffer(struct device *dev, size_t size, void *cpu_addr,
			      dma_addr_t dma_handle)
{
	dma_free_coherent(dev, size, cpu_addr, dma_handle);
	if (atomic_read(&g_cmdq_mem_monitor.monitor_mem_enable) != 0)
		cmdq_core_monitor_record_free(size);
}

int32_t cmdq_core_set_secure_IRQ_status(uint32_t value)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	const uint32_t offset = CMDQ_SEC_SHARED_IRQ_RAISED_OFFSET;
	uint32_t *pVA;

	value = 0x0;
	if (gCmdqContext.hSecSharedMem == NULL) {
		CMDQ_ERR("%s, shared memory is not created\n", __func__);
		return -EFAULT;
	}

	pVA = (uint32_t *) (gCmdqContext.hSecSharedMem->pVABase + offset);
	(*pVA) = value;

	CMDQ_VERBOSE("[shared_IRQ]set raisedIRQ:0x%08x\n", value);

	return 0;
#else
	CMDQ_ERR("CMDQ secure path not support in this proj\n");
	return -EFAULT;
#endif
}

int32_t cmdq_core_get_secure_IRQ_status(void)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	const uint32_t offset = CMDQ_SEC_SHARED_IRQ_RAISED_OFFSET;
	uint32_t *pVA;
	int32_t value;

	value = 0x0;
	if (gCmdqContext.hSecSharedMem == NULL) {
		CMDQ_ERR("%s, shared memory is not created\n", __func__);
		return -EFAULT;
	}

	pVA = (uint32_t *) (gCmdqContext.hSecSharedMem->pVABase + offset);
	value = *pVA;

	CMDQ_VERBOSE("[shared_IRQ]IRQ raised:0x%08x\n", value);

	return value;
#else
	CMDQ_ERR("CMDQ secure path not support in this proj\n");
	return -EFAULT;
#endif
}

int32_t cmdq_core_set_secure_thread_exec_counter(const int32_t thread,
	const uint32_t cookie)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	const uint32_t offset = CMDQ_SEC_SHARED_THR_CNT_OFFSET +
		thread * sizeof(uint32_t);
	uint32_t *pVA = NULL;

	if (cmdq_get_func()->isSecureThread(thread) == false) {
		CMDQ_ERR("%s, invalid param, thread: %d\n", __func__, thread);
		return -EFAULT;
	}

	if (gCmdqContext.hSecSharedMem == NULL) {
		CMDQ_ERR("%s, shared memory is not created\n", __func__);
		return -EFAULT;
	}

	CMDQ_MSG("[shared_cookie] set thread %d CNT(%p) to %d\n",
		thread, pVA, cookie);
	pVA = (uint32_t *) (gCmdqContext.hSecSharedMem->pVABase + offset);
	(*pVA) = cookie;

	return 0;
#else
	CMDQ_ERR("CMDQ secure path not support in this proj\n");
	return -EFAULT;
#endif
}

int32_t cmdq_core_get_secure_thread_exec_counter(const int32_t thread)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	const uint32_t offset = CMDQ_SEC_SHARED_THR_CNT_OFFSET +
		thread * sizeof(uint32_t);
	uint32_t *pVA;
	uint32_t value;

	if (cmdq_get_func()->isSecureThread(thread) == false) {
		CMDQ_ERR("%s, invalid param, thread: %d\n", __func__, thread);
		return -EFAULT;
	}

	if (gCmdqContext.hSecSharedMem == NULL) {
		CMDQ_ERR("%s, shared memory is not created\n", __func__);
		return -EFAULT;
	}

	pVA = (uint32_t *) (gCmdqContext.hSecSharedMem->pVABase + offset);
	value = *pVA;
#if defined(CMDQ_SECURE_PATH_NORMAL_IRQ) || defined(CMDQ_SECURE_PATH_HW_LOCK)
	value = value + 1;
#endif
	CMDQ_VERBOSE("[shared_cookie] get thread %d CNT(%p) value is %d\n",
		thread, pVA, value);

	return value;
#else
	CMDQ_ERR("CMDQ secure path not support in this proj\n");
	return -EFAULT;
#endif

}

void cmdq_core_dump_secure_task_status(void)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	const uint32_t task_va_offset = CMDQ_SEC_SHARED_TASK_VA_OFFSET;
	const uint32_t task_op_offset = CMDQ_SEC_SHARED_OP_OFFSET;

	uint32_t *pVA;
	int32_t va_value_lo, va_value_hi, op_value;

	if (gCmdqContext.hSecSharedMem == NULL) {
		CMDQ_ERR("%s, shared memory is not created\n", __func__);
		return;
	}

	pVA = (uint32_t *) (gCmdqContext.hSecSharedMem->pVABase +
		task_va_offset);
	va_value_lo = *pVA;

	pVA = (uint32_t *) (gCmdqContext.hSecSharedMem->pVABase +
		task_va_offset + sizeof(uint32_t));
	va_value_hi = *pVA;

	pVA = (uint32_t *) (gCmdqContext.hSecSharedMem->pVABase +
		task_op_offset);
	op_value = *pVA;

	CMDQ_ERR("[shared_op_status]task VA:0x%04x%04x, op:%d\n",
		va_value_hi, va_value_lo, op_value);
#else
	CMDQ_ERR("CMDQ secure path not support in this proj\n");
#endif
}

int32_t cmdq_core_thread_exec_counter(const int32_t thread)
{
	return (cmdq_get_func()->isSecureThread(thread) == false) ?
	    (CMDQ_GET_COOKIE_CNT(thread)) :
	    (cmdq_core_get_secure_thread_exec_counter(thread));
}

struct cmdqSecSharedMemoryStruct *cmdq_core_get_secure_shared_memory(void)
{
	return gCmdqContext.hSecSharedMem;
}

int32_t cmdq_core_stop_secure_path_notify_thread(void)
{
#if defined(CMDQ_SECURE_PATH_SUPPORT) && !defined(CMDQ_SECURE_PATH_NORMAL_IRQ)
	int status = 0;
	unsigned long flags;
	struct cmdqRecStruct *notify_loop_handle = NULL;

	mutex_lock(&gCmdqNotifyLoopMutex);
	do {
		if (gCmdqContext.hNotifyLoop == NULL) {
			/* no notify thread */
			CMDQ_MSG("[WARNING]NULL notify loop\n");
			break;
		}

		status = cmdq_task_stop_loop(gCmdqContext.hNotifyLoop);
		if (status < 0) {
			CMDQ_ERR("stop notify loop failed, status:%d\n",
				status);
			break;
		}

		/*
		 * Clear loop handle to in protect to avoid
		 * other thread try to start notify.
		 * Destroy task later since vfree inside
		 * destroy cannot call during spinlock.
		 */
		spin_lock_irqsave(&gCmdqExecLock, flags);
		notify_loop_handle = (struct cmdqRecStruct *)
			gCmdqContext.hNotifyLoop;
		gCmdqContext.hNotifyLoop = NULL;
		spin_unlock_irqrestore(&gCmdqExecLock, flags);
		cmdq_task_destroy(notify_loop_handle);

		/* CPU clear event */
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_SECURE_THR_EOF);
	} while (0);
	mutex_unlock(&gCmdqNotifyLoopMutex);

	return status;
#else
	return 0;
#endif
}

int32_t cmdq_core_start_secure_path_notify_thread(void)
{
#if defined(CMDQ_SECURE_PATH_SUPPORT) && !defined(CMDQ_SECURE_PATH_NORMAL_IRQ)
	int status = 0;
	struct cmdqRecStruct *handle;
	unsigned long flags;

	mutex_lock(&gCmdqNotifyLoopMutex);
	do {
		if (gCmdqContext.hNotifyLoop != NULL) {
			/* already created it */
			break;
		}

		/* CPU clear event */
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_SECURE_THR_EOF);

		/* record command */
		cmdq_task_create(CMDQ_SCENARIO_SECURE_NOTIFY_LOOP, &handle);
		cmdq_task_reset(handle);
		cmdq_op_wait(handle, CMDQ_SYNC_SECURE_THR_EOF);
#ifdef CMDQ_SECURE_PATH_HW_LOCK
		cmdq_op_wait(handle, CMDQ_SYNC_SECURE_WSM_LOCK);
#endif
		status = cmdq_task_start_loop(handle);

		if (status < 0) {
			CMDQ_ERR("start notify loop failed, status:%d\n",
				status);
			break;
		}

		/* update notify handle */
		spin_lock_irqsave(&gCmdqExecLock, flags);
		gCmdqContext.hNotifyLoop = (void *) handle;
		spin_unlock_irqrestore(&gCmdqExecLock, flags);
	} while (0);
	mutex_unlock(&gCmdqNotifyLoopMutex);

	return status;
#else
	return 0;
#endif
}

const char *cmdq_core_get_event_name_ENUM(enum CMDQ_EVENT_ENUM event)
{
	const char *eventName = "CMDQ_EVENT_UNKNOWN";

#undef DECLARE_CMDQ_EVENT
#define DECLARE_CMDQ_EVENT(name, val, dts_name)	\
	{ if (val == event) { eventName = #name; break; }  }
	do {
#include "cmdq_event_common.h"
	} while (0);
#undef DECLARE_CMDQ_EVENT

	return eventName;
}

const char *cmdq_core_get_event_name(enum CMDQ_EVENT_ENUM event)
{
	const int32_t eventENUM = cmdq_core_reverse_event_ENUM(event);

	return cmdq_core_get_event_name_ENUM(eventENUM);
}

void cmdqCoreClearEvent(enum CMDQ_EVENT_ENUM event)
{
	int32_t eventValue = cmdq_core_get_event_value(event);

	CMDQ_MSG("clear event %d\n", eventValue);
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, eventValue);
}

void cmdqCoreSetEvent(enum CMDQ_EVENT_ENUM event)
{
	int32_t eventValue = cmdq_core_get_event_value(event);

	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | eventValue);
}

uint32_t cmdqCoreGetEvent(enum CMDQ_EVENT_ENUM event)
{
	uint32_t regValue = 0;
	int32_t eventValue = cmdq_core_get_event_value(event);

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

ssize_t log_level_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int len = 0;

	if (buf)
		len = sprintf(buf, "%d\n", gCmdqContext.logLevel);

	return len;
}

ssize_t log_level_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
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

ssize_t profile_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int len = 0;

	if (buf)
		len = sprintf(buf, "%d\n", gCmdqContext.enableProfile);

	return len;

}

ssize_t profile_enable_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
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

static void cmdq_core_dump_buffer(const struct TaskStruct *pTask)
{
	struct CmdBufferStruct *cmd_buffer = NULL;

	list_for_each_entry(cmd_buffer, &pTask->cmd_buffer_list, listEntry) {
		uint32_t last_inst_index =
			CMDQ_CMD_BUFFER_SIZE / sizeof(uint32_t);
		uint32_t *va = cmd_buffer->pVABase + last_inst_index - 4;

		if (list_is_last(&cmd_buffer->listEntry,
			&pTask->cmd_buffer_list) &&
			pTask->pCMDEnd > cmd_buffer->pVABase &&
			pTask->pCMDEnd <
				cmd_buffer->pVABase + last_inst_index &&
			va > pTask->pCMDEnd - 3) {
			va = pTask->pCMDEnd - 3;
		}

		CMDQ_ERR("VABase: 0x%p MVABase: 0x%pa\n",
			cmd_buffer->pVABase, &cmd_buffer->MVABase);
		CMDQ_ERR("last inst (0x%p): 0x%08x:%08x 0x%08x:%08x\n",
			va, va[1], va[0], va[3], va[2]);
	}
}

static void cmdq_core_dump_task(const struct TaskStruct *pTask)
{
	CMDQ_ERR("Task: 0x%p, Scenario: %d, State: %d\n",
	     pTask, pTask->scenario, pTask->taskState);
	CMDQ_ERR("Priority: %d, Flag: 0x%016llx\n",
	     pTask->priority, pTask->engineFlag);

	cmdq_core_dump_buffer(pTask);

	/* dump last Inst only when VALID command buffer */
	/* otherwise data abort is happened */
	if (!list_empty(&pTask->cmd_buffer_list)) {
		CMDQ_ERR("CMDEnd: 0x%p, Command Size: %d\n",
			pTask->pCMDEnd, pTask->commandSize);
		CMDQ_ERR("Last Inst: 0x%08x:0x%08x, 0x%08x:0x%08x\n",
			pTask->pCMDEnd[-3],
			pTask->pCMDEnd[-2],
			pTask->pCMDEnd[-1],
			pTask->pCMDEnd[0]);
	} else {
		CMDQ_ERR("CMDEnd: 0x%p, Size: %d\n",
			 pTask->pCMDEnd, pTask->commandSize);
	}

	CMDQ_ERR("Buffer size: %u available size: %u\n",
		pTask->bufferSize, pTask->buf_available_size);

	CMDQ_ERR("Result buffer va: 0x%p pa: 0x%pa count: %u\n",
		pTask->regResults, &pTask->regResultsMVA, pTask->regCount);

	CMDQ_ERR("Reorder: %d, Trigger: %lld, Got IRQ: 0x%llx\n",
		pTask->reorder, pTask->trigger,
		pTask->gotIRQ);
	CMDQ_ERR("Wait: %lld, Finish: %lld\n",
		pTask->beginWait, pTask->wakedUp);
	CMDQ_ERR("Caller pid: %d name: %s\n",
		pTask->callerPid, pTask->callerName);
}

void cmdq_core_dump_task_buffer_hex(struct TaskStruct *pTask)
{
	struct CmdBufferStruct *cmd_buffer = NULL;

	if (unlikely(pTask == NULL || list_empty(&pTask->cmd_buffer_list))) {
		CMDQ_ERR("Try to dump empty task:%p\n", pTask);
		return;
	}

	list_for_each_entry(cmd_buffer, &pTask->cmd_buffer_list, listEntry) {
		print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 16, 4,
			cmd_buffer->pVABase,
			list_is_last(&cmd_buffer->listEntry,
			&pTask->cmd_buffer_list) ?
			CMDQ_CMD_BUFFER_SIZE - pTask->buf_available_size :
			CMDQ_CMD_BUFFER_SIZE,
			true);
	}
}

static bool cmdq_core_task_is_buffer_size_valid(
	const struct TaskStruct *pTask)
{
	return (pTask->bufferSize % CMDQ_CMD_BUFFER_SIZE ==
		(pTask->buf_available_size > 0 ?
		CMDQ_CMD_BUFFER_SIZE - pTask->buf_available_size : 0));
}

static uint32_t *cmdq_core_get_pc(const struct TaskStruct *pTask,
	uint32_t thread, uint32_t insts[4])
{
	long currPC = 0L;
	uint8_t *inst_ptr = NULL;
	struct CmdBufferStruct *cmd_buffer = NULL;

	if (unlikely(pTask == NULL ||
		list_empty(&pTask->cmd_buffer_list) ||
		thread == CMDQ_INVALID_THREAD)) {
		CMDQ_ERR("get pc failed, pTask:0x%p, thread:%d\n",
			pTask, thread);
		return NULL;
	}

	insts[2] = 0;
	insts[3] = 0;

	currPC = CMDQ_AREG_TO_PHYS(CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread)));
	list_for_each_entry(cmd_buffer, &pTask->cmd_buffer_list, listEntry) {
		if (currPC >= cmd_buffer->MVABase &&
			currPC < cmd_buffer->MVABase + CMDQ_CMD_BUFFER_SIZE) {
			inst_ptr = (uint8_t *) cmd_buffer->pVABase +
				(currPC - cmd_buffer->MVABase);
		}
	}

	if (inst_ptr) {
		insts[2] = CMDQ_REG_GET32(inst_ptr + 0);
		insts[3] = CMDQ_REG_GET32(inst_ptr + 4);
	}

	return (uint32_t *) inst_ptr;
}

static bool cmdq_core_task_is_valid_pa(const struct TaskStruct *pTask,
	dma_addr_t pa)
{
	struct CmdBufferStruct *entry = NULL;
	long task_pa = 0;

	/* check if pc stay at end */
	if (CMDQ_IS_END_ADDR(pa) && pTask->pCMDEnd &&
		CMDQ_IS_END_ADDR(pTask->pCMDEnd[-1]))
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

void cmdq_core_get_task_first_buffer(struct TaskStruct *pTask,
			uint32_t **va_ptr, dma_addr_t *pa_handle)
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

static uint32_t *cmdq_core_task_get_first_va(const struct TaskStruct *pTask)
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

static uint32_t *cmdq_core_task_get_last_va(struct TaskStruct *pTask)
{
	struct CmdBufferStruct *entry = NULL;

	if (list_empty(&pTask->cmd_buffer_list))
		return NULL;
	entry = list_last_entry(&pTask->cmd_buffer_list,
		struct CmdBufferStruct, listEntry);
	return entry->pVABase;
}

void cmdq_core_dump_tasks_info(void)
{
	struct TaskStruct *pTask = NULL;
	struct list_head *p = NULL;
	int32_t index = 0;

	/* Remove Mutex, since this will be called in ISR */
	/* mutex_lock(&gCmdqTaskMutex); */

	CMDQ_ERR("========= Active List Task Dump =========\n");
	index = 0;
	list_for_each(p, &gCmdqContext.taskActiveList) {
		pTask = list_entry(p, struct TaskStruct, listEntry);
		CMDQ_ERR("Task(%d) 0x%p, Pid: %d, Name: %s\n",
			   index, pTask, pTask->callerPid, pTask->callerName);
		CMDQ_ERR("Scenario: %d, engineFlag: 0x%llx\n",
			   pTask->scenario, pTask->engineFlag);
		++index;
	}
	CMDQ_ERR("====== Total %d in Active Task =======\n", index);

	/* mutex_unlock(&gCmdqTaskMutex); */
}

int cmdq_core_print_profile_marker(const struct RecordStruct *pRecord,
	char *_buf, int bufLen)
{
	int length = 0;

#ifdef CMDQ_PROFILE_MARKER_SUPPORT
	int32_t profileMarkerCount;
	int32_t i;
	char *buf;

	buf = _buf;

	profileMarkerCount = pRecord->profileMarkerCount;
	if (profileMarkerCount > CMDQ_MAX_PROFILE_MARKER_IN_TASK)
		profileMarkerCount = CMDQ_MAX_PROFILE_MARKER_IN_TASK;

	for (i = 0; i < profileMarkerCount; i++) {
		length = snprintf(buf, bufLen, ",P%d,%s,%lld",
				  i, pRecord->profileMarkerTag[i],
				  pRecord->profileMarkerTimeNS[i]);
		bufLen -= length;
		buf += length;
	}

	if (i > 0) {
		length = snprintf(buf, bufLen, "\n");
		bufLen -= length;
		buf += length;
	}

	length = (buf - _buf);
#endif

	return length;
}

static int cmdq_core_print_record(const struct RecordStruct *pRecord,
	int index, char *_buf, int bufLen)
{
	int length = 0;
	char *unit[5] = { "ms", "ms", "ms", "ms", "ms" };
	int32_t IRQTime;
	int32_t execTime;
	int32_t beginWaitTime;
	int32_t totalTime;
	int32_t acquireThreadTime;
	unsigned long rem_nsec;
	CMDQ_TIME submitTimeSec;
	char *buf;

	rem_nsec = 0;
	submitTimeSec = pRecord->submit;
	rem_nsec = do_div(submitTimeSec, 1000000000);
	buf = _buf;

	unit[0] = "ms";
	unit[1] = "ms";
	unit[2] = "ms";
	unit[3] = "ms";
	unit[4] = "ms";
	CMDQ_GET_TIME_IN_MS(pRecord->submit, pRecord->done, totalTime);
	CMDQ_GET_TIME_IN_MS(pRecord->submit, pRecord->trigger,
		acquireThreadTime);
	CMDQ_GET_TIME_IN_MS(pRecord->submit, pRecord->beginWait, beginWaitTime);
	CMDQ_GET_TIME_IN_MS(pRecord->trigger, pRecord->gotIRQ, IRQTime);
	CMDQ_GET_TIME_IN_MS(pRecord->trigger, pRecord->wakedUp, execTime);

	/* detect us interval */
	if (acquireThreadTime == 0) {
		CMDQ_GET_TIME_IN_US_PART(pRecord->submit,
			pRecord->trigger, acquireThreadTime);
		unit[0] = "us";
	}
	if (IRQTime == 0) {
		CMDQ_GET_TIME_IN_US_PART(pRecord->trigger,
			pRecord->gotIRQ, IRQTime);
		unit[1] = "us";
	}
	if (beginWaitTime == 0) {
		CMDQ_GET_TIME_IN_US_PART(pRecord->submit,
			pRecord->beginWait, beginWaitTime);
		unit[2] = "us";
	}
	if (execTime == 0) {
		CMDQ_GET_TIME_IN_US_PART(pRecord->trigger,
			pRecord->wakedUp, execTime);
		unit[3] = "us";
	}
	if (totalTime == 0) {
		CMDQ_GET_TIME_IN_US_PART(pRecord->submit,
			pRecord->done, totalTime);
		unit[4] = "us";
	}

	/* pRecord->priority for task priority */
	/* when pRecord->is_secure is 0 for secure task */
	length = snprintf(buf, bufLen,
			"%4d,(%5d, %2d, 0x%012llx, %2d, %d, %d),",
			index, pRecord->user, pRecord->scenario,
			pRecord->engineFlag,
			pRecord->priority, pRecord->is_secure, pRecord->size);
	bufLen -= length;
	buf += length;

	length = snprintf(buf, bufLen,
			"(%02d, %02d),(%5dns , %lld, %lld),",
			pRecord->thread,
			cmdq_get_func()->priority(pRecord->scenario),
			pRecord->writeTimeNS, pRecord->writeTimeNSBegin,
			pRecord->writeTimeNSEnd);
	bufLen -= length;
	buf += length;

	length = snprintf(buf, bufLen,
			"(%5llu.%06lu, %4d%s, %4d%s, %4d%s, %4d%s,",
			submitTimeSec, rem_nsec / 1000,
			acquireThreadTime, unit[0],
			IRQTime, unit[1], beginWaitTime, unit[2],
			execTime, unit[3]);
	bufLen -= length;
	buf += length;

	length = snprintf(buf, bufLen,
			" (%dus, %dus, %dus)),%4d%s",
			pRecord->durAlloc, pRecord->durReclaim,
			pRecord->durRelease,
			totalTime, unit[4]);
	bufLen -= length;
	buf += length;

	length = snprintf(buf, bufLen, "\n");
	bufLen -= length;
	buf += length;

	length = (buf - _buf);
	return length;
}

int cmdqCorePrintRecordSeq(struct seq_file *m, void *v)
{
	unsigned long flags;
	int32_t index;
	int32_t numRec;
	struct RecordStruct record;
	char msg[160] = { 0 };

	/* we try to minimize time spent in spin lock */
	/* since record is an array so it is okay to */
	/* allow displaying an out-of-date entry. */
	spin_lock_irqsave(&gCmdqRecordLock, flags);
	numRec = gCmdqContext.recNum;
	index = gCmdqContext.lastID - 1;
	spin_unlock_irqrestore(&gCmdqRecordLock, flags);

	/* we print record in reverse order. */
	for (; numRec > 0; --numRec, --index) {
		if (index >= CMDQ_MAX_RECORD_COUNT)
			index = 0;
		else if (index < 0)
			index = CMDQ_MAX_RECORD_COUNT - 1;

		/* Make sure we don't print a record that is during updating. */
		/* However, this record may already be different */
		/* from the time of entering cmdqCorePrintRecordSeq(). */
		spin_lock_irqsave(&gCmdqRecordLock, flags);
		record = gCmdqContext.record[index];
		spin_unlock_irqrestore(&gCmdqRecordLock, flags);

		cmdq_core_print_record(&record, index, msg, sizeof(msg));
		seq_printf(m, "%s", msg);

		cmdq_core_print_profile_marker(&record, msg, sizeof(msg));
		seq_printf(m, "%s", msg);
	}

	return 0;
}

int cmdqCorePrintErrorSeq(struct seq_file *m, void *v)
{
	/* error is not used by now */
	return 0;
}

int cmdqCorePrintStatusSeq(struct seq_file *m, void *v)
{
	unsigned long flags = 0;
	struct EngineStruct *pEngine = NULL;
	struct TaskStruct *pTask = NULL;
	struct list_head *p = NULL;
	struct ThreadStruct *pThread = NULL;
	int32_t index = 0;
	int32_t inner = 0;
	int listIdx = 0;
	const struct list_head *lists[] = {
		&gCmdqContext.taskFreeList,
		&gCmdqContext.taskActiveList,
		&gCmdqContext.taskWaitList
	};
	uint32_t *pcVA = NULL;
	uint32_t insts[4] = { 0 };
	char parsedInstruction[128] = { 0 };

	static const char *const listNames[] = { "Free", "Active", "Wait" };

	const enum CMDQ_ENG_ENUM engines[] =
		CMDQ_FOREACH_MODULE_PRINT(GENERATE_ENUM);
	static const char *const engineNames[] =
		CMDQ_FOREACH_MODULE_PRINT(GENERATE_STRING);

	struct list_head *p_buf = NULL;
	struct CmdBufferStruct *cmd_buffer = NULL;
	uint32_t *pVABase = NULL;
	dma_addr_t MVABase = 0;

#ifdef CMDQ_DUMP_FIRSTERROR
	if (gCmdqFirstError.cmdqCount > 0) {
		unsigned long long saveTimeSec = gCmdqFirstError.savetime;
		unsigned long rem_nsec = do_div(saveTimeSec, 1000000000);
		struct tm nowTM;

		time_to_tm(gCmdqFirstError.savetv.tv_sec,
			sys_tz.tz_minuteswest * 60, &nowTM);
		seq_puts(m, "========= [CMDQ] Dump first error ===\n");
		seq_printf(m, "kernel time:[%5llu.%06lu],",
			saveTimeSec, rem_nsec / 1000);
		seq_printf(m,
			" UTC time:[%04ld-%02d-%02d %02d:%02d:%02d.%06ld],",
			(nowTM.tm_year + 1900), (nowTM.tm_mon + 1),
			nowTM.tm_mday,
			nowTM.tm_hour, nowTM.tm_min, nowTM.tm_sec,
			gCmdqFirstError.savetv.tv_usec);
		seq_printf(m, " Pid: %d, Name: %s\n", gCmdqFirstError.callerPid,
			   gCmdqFirstError.callerName);
		seq_printf(m, "%s", gCmdqFirstError.cmdqString);
		if (gCmdqFirstError.cmdqMaxSize <= 0)
			seq_printf(m, "\nWARNING: MAX size: %d is full\n",
				CMDQ_MAX_FIRSTERROR);
		seq_puts(m, "\n\n");
	}
#endif

	/* Save command buffer dump */
	if (gCmdqBufferDump.count > 0) {
		int32_t buffer_id;

		seq_puts(m, "===== [CMDQ] Dump Command Buffer ============\n");
		mutex_lock(&gCmdqTaskMutex);
		for (buffer_id = 0;
			buffer_id < gCmdqBufferDump.bufferSize; buffer_id++)
			seq_printf(m, "%c",
				gCmdqBufferDump.cmdqString[buffer_id]);
		mutex_unlock(&gCmdqTaskMutex);
		seq_puts(m, "\n===== [CMDQ] Dump Command Buffer END ===\n\n\n");
	}

#ifdef CMDQ_PWR_AWARE
	/* note for constatnt format (without a % substitution), */
	/* use seq_puts to speed up outputs */
	seq_puts(m, "====== Clock Status =======\n");
	cmdq_get_func()->printStatusSeqClock(m);
#endif

	seq_puts(m, "====== DMA Mask Status =======\n");
	seq_printf(m, "dma_set_mask result: %d\n",
		cmdq_dev_get_dma_mask_result());

	seq_puts(m, "====== Engine Usage =======\n");

	for (listIdx = 0; listIdx < ARRAY_SIZE(engines); ++listIdx) {
		pEngine = &gCmdqContext.engine[engines[listIdx]];
		seq_printf(m, "%s: count %d, owner %d, fail: %d, reset: %d\n",
			   engineNames[listIdx],
			   pEngine->userCount,
			   pEngine->currOwner, pEngine->failCount,
			   pEngine->resetCount);
	}


	mutex_lock(&gCmdqTaskMutex);

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
			seq_printf(m, "State %d, Size: %d\n",
				   pTask->taskState, pTask->commandSize);

			list_for_each(p_buf, &pTask->cmd_buffer_list) {
				cmd_buffer = list_entry(p_buf,
					struct CmdBufferStruct, listEntry);
				seq_printf(m, "VABase: 0x%p, MVABase: %pa\n",
				   cmd_buffer->pVABase, &cmd_buffer->MVABase);
			}
			seq_printf(m, "Scenario %d, Priority: %d,\n",
				pTask->scenario, pTask->priority);
			seq_printf(m, "Flag: 0x%08llx, VAEnd: 0x%p\n",
				pTask->engineFlag,
				pTask->pCMDEnd);
			seq_printf(m,
				"Reorder:%d, Trigger %lld, IRQ: 0x%llx\n",
				pTask->reorder,
				pTask->trigger, pTask->gotIRQ);
			seq_printf(m,
				"Wait: %lld, Wake Up: %lld\n",
				pTask->beginWait, pTask->wakedUp);
			++index;
		}
		seq_printf(m, "====== Total %d %s Task =======\n",
			index, listNames[listIdx]);
	}

	for (index = 0; index < CMDQ_MAX_THREAD_COUNT; index++) {
		pThread = &(gCmdqContext.thread[index]);

		if (pThread->taskCount > 0) {
			seq_printf(m, "====== Thread %d Usage =======\n",
				index);
			seq_printf(m, "Wait Cookie %d, Next Cookie %d\n",
				pThread->waitCookie,
				   pThread->nextCookie);

			spin_lock_irqsave(&gCmdqThreadLock, flags);

			for (inner = 0;
				inner < cmdq_core_max_task_in_thread(index);
				inner++) {
				pTask = pThread->pCurTask[inner];
				if (pTask == NULL)
					continue;
				/* pTask != NULL */
				/* dump task basic info */
				seq_printf(m,
					"Slot: %d, Task: 0x%p, Pid: %d,",
					index, pTask, pTask->callerPid);
				seq_printf(m,
					" Name: %s, Scn: %d,",
					pTask->callerName, pTask->scenario);

				/* here only print first buffer to reduce log */
				cmdq_core_get_task_first_buffer(pTask,
					&pVABase, &MVABase);
				seq_printf(m,
					" VABase: 0x%p, MVABase: %pa,Size:%d",
					pVABase, &MVABase,
					pTask->commandSize);

				if (pTask->pCMDEnd) {
					seq_printf(m,
						", Last Command:0x%08x:0x%08x",
						pTask->pCMDEnd[-1],
						pTask->pCMDEnd[0]);
				}

				seq_puts(m, "\n");

				/* dump PC info */
				pcVA = cmdq_core_get_pc(pTask, index, insts);
				if (pcVA) {
					cmdq_core_parse_instruction(pcVA,
						parsedInstruction,
						sizeof(parsedInstruction));
					seq_printf(m,
						"PC(VA):0x%p,0x%08x:0x%08x=>%s",
						pcVA, insts[2], insts[3],
						parsedInstruction);
				} else {
					seq_puts(m, "PC(VA): Not available\n");
				}
			}

			spin_unlock_irqrestore(&gCmdqThreadLock, flags);
		}
	}

	mutex_unlock(&gCmdqTaskMutex);

	return 0;
}

ssize_t record_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned long flags;
	int32_t begin;
	int32_t curPos;
	ssize_t bufLen = PAGE_SIZE;
	ssize_t length;
	int32_t index;
	int32_t numRec;
	struct RecordStruct record;

	begin = 0;
	curPos = 0;
	length = 0;
	bufLen = PAGE_SIZE;

	/* we try to minimize time spent in spin lock */
	/* since record is an array so it is okay to */
	/* allow displaying an out-of-date entry. */
	spin_lock_irqsave(&gCmdqRecordLock, flags);
	numRec = gCmdqContext.recNum;
	index = gCmdqContext.lastID - 1;
	spin_unlock_irqrestore(&gCmdqRecordLock, flags);

	/* we print record in reverse order. */
	for (; numRec > 0; --numRec, --index) {
		/* CMDQ_ERR("[rec] index=%d numRec =%d\n", index, numRec); */

		if (index >= CMDQ_MAX_RECORD_COUNT)
			index = 0;
		else if (index < 0)
			index = CMDQ_MAX_RECORD_COUNT - 1;

		/* Make sure we don't print a record that is during updating. */
		/* However, this record may already be different */
		/* from the time of entering cmdqCorePrintRecordSeq(). */
		spin_lock_irqsave(&gCmdqRecordLock, flags);
		record = (gCmdqContext.record[index]);
		spin_unlock_irqrestore(&gCmdqRecordLock, flags);

		length = cmdq_core_print_record(&record,
			index, &buf[curPos], bufLen);

		bufLen -= length;
		curPos += length;

		if (bufLen <= 0 || curPos >= PAGE_SIZE)
			break;
	}

	if (curPos >= PAGE_SIZE)
		curPos = PAGE_SIZE;

	return curPos;
}

ssize_t error_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i;
	int length = 0;

	for (i = 0; i < gCmdqContext.errNum && i < CMDQ_MAX_ERROR_COUNT; ++i) {
		struct ErrorStruct *pError = &gCmdqContext.error[i];
		u64 ts = pError->ts_nsec;
		unsigned long rem_nsec = do_div(ts, 1000000000);

		length += snprintf(buf + length,
				   PAGE_SIZE - length,
				   "[%5lu.%06lu] ",
				   (unsigned long)ts, rem_nsec / 1000);
		length += cmdq_core_print_record(&pError->errorRec,
				i, buf + length, PAGE_SIZE - length);
		if (length >= PAGE_SIZE)
			break;
	}

	return length;
}

ssize_t status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	unsigned long flags = 0L;
	struct EngineStruct *pEngine = NULL;
	struct TaskStruct *pTask = NULL;
	struct list_head *p = NULL;
	struct ThreadStruct *pThread = NULL;
	int32_t index = 0;
	int32_t inner = 0;
	int32_t length = 0;
	int listIdx = 0;
	char *pBuffer = buf;
	const struct list_head *lists[] = {
		&gCmdqContext.taskFreeList,
		&gCmdqContext.taskActiveList,
		&gCmdqContext.taskWaitList
	};
	uint32_t *pcVA = NULL;
	uint32_t insts[4] = { 0 };
	char parsedInstruction[128] = { 0 };

	static const char *const listNames[] = { "Free", "Active", "Wait" };

	const enum CMDQ_ENG_ENUM engines[] =
		CMDQ_FOREACH_MODULE_PRINT(GENERATE_ENUM);
	static const char *const engineNames[] =
		CMDQ_FOREACH_MODULE_PRINT(GENERATE_STRING);

	uint32_t *pVABase = NULL;
	dma_addr_t MVABase = 0;

#ifdef CMDQ_PWR_AWARE
	length += snprintf(&pBuffer[length], PAGE_SIZE - length,
		"====== Clock Status =======\n");
	length += cmdq_get_func()->printStatusClock(&pBuffer[length]);
#endif

	length += snprintf(&pBuffer[length], PAGE_SIZE - length,
		"====== DMA Mask Status =======\n");
	length += snprintf(&pBuffer[length], PAGE_SIZE - length,
		"dma_set_mask result: %d\n",
		cmdq_dev_get_dma_mask_result());

	length += snprintf(&pBuffer[length], PAGE_SIZE - length,
		"====== Engine Usage =======\n");

	for (listIdx = 0; listIdx < ARRAY_SIZE(engines); ++listIdx) {
		pEngine = &gCmdqContext.engine[engines[listIdx]];
		length += snprintf(&pBuffer[length], PAGE_SIZE - length,
				"%s: count %d, owner %d, fail:%d, reset: %d\n",
				engineNames[listIdx],
				pEngine->userCount,
				pEngine->currOwner, pEngine->failCount,
				pEngine->resetCount);
	}


	mutex_lock(&gCmdqTaskMutex);

	/* print all tasks in both list */
	for (listIdx = 0; listIdx < ARRAY_SIZE(lists); listIdx++) {
		/* skip FreeTasks by default */
		if (!cmdq_core_should_print_msg() && listIdx == 0)
			continue;

		index = 0;
		list_for_each(p, lists[listIdx]) {
			pTask = list_entry(p, struct TaskStruct, listEntry);
			length += snprintf(&pBuffer[length],
				PAGE_SIZE - length,
				"====== %s Task(%d) 0x%p Usage =======\n",
				listNames[listIdx], index, pTask);

			cmdq_core_get_task_first_buffer(pTask,
				&pVABase, &MVABase);
			length += snprintf(&pBuffer[length],
				PAGE_SIZE - length,
				"State%d,VABase:0x%p,MVABase:%pa,Size:%d\n",
				pTask->taskState, pVABase, &MVABase,
				pTask->commandSize);

			length += snprintf(&pBuffer[length],
				PAGE_SIZE - length,
				"Scenario %d, Priority: %d\n",
				pTask->scenario, pTask->priority);
			length += snprintf(&pBuffer[length],
				PAGE_SIZE - length,
				"Flag: 0x%08llx, VAEnd: 0x%p\n",
				pTask->engineFlag,
				pTask->pCMDEnd);

			length += snprintf(&pBuffer[length],
				PAGE_SIZE - length,
				"Reoder:%d, Trigger %lld, IRQ: 0x%llx\n",
				pTask->reorder,
				pTask->trigger,
				pTask->gotIRQ);
			length += snprintf(&pBuffer[length],
				PAGE_SIZE - length,
				"Wait: %lld, Wake Up: %lld\n",
				pTask->beginWait,
				pTask->wakedUp);
			++index;
		}
		length += snprintf(&pBuffer[length],
			PAGE_SIZE - length,
			"====== Total %d %s Task =======\n", index,
			listNames[listIdx]);
	}

	for (index = 0; index < CMDQ_MAX_THREAD_COUNT; index++) {
		pThread = &(gCmdqContext.thread[index]);

		if (!(pThread->taskCount > 0))
			continue;

		/* pThread->taskCount > 0 */
		length += snprintf(&pBuffer[length], PAGE_SIZE - length,
				"====== Thread %d Usage =======\n", index);
		length += snprintf(&pBuffer[length], PAGE_SIZE - length,
				"Wait Cookie %d, Next Cookie %d\n",
				pThread->waitCookie, pThread->nextCookie);

		spin_lock_irqsave(&gCmdqThreadLock, flags);

		for (inner = 0; inner < cmdq_core_max_task_in_thread(index);
			inner++) {
			pTask = pThread->pCurTask[inner];
			if (pTask == NULL)
				continue;

			/* pTask != NULL */
			/* dump task basic info */
			length += snprintf(&pBuffer[length],
				PAGE_SIZE - length,
				"Slot:%d,Task:0x%p,Pid:%d,Name:%s,Scn: %d,",
				index, pTask, pTask->callerPid,
				pTask->callerName, pTask->scenario);

			cmdq_core_get_task_first_buffer(pTask,
				&pVABase, &MVABase);
			length += snprintf(&pBuffer[length],
				PAGE_SIZE - length,
				" VABase: 0x%p, MVABase: %pa, Size: %d",
				pVABase, &MVABase, pTask->commandSize);

			if (pTask->pCMDEnd) {
				length += snprintf(&pBuffer[length],
					PAGE_SIZE - length,
					", Last Command: 0x%08x:0x%08x",
					pTask->pCMDEnd[-1],
					pTask->pCMDEnd[0]);
			}

			length += snprintf(&pBuffer[length],
				PAGE_SIZE - length, "\n");

			/* dump PC info */
			pcVA = cmdq_core_get_pc(pTask, index, insts);
			if (pcVA) {
				cmdq_core_parse_instruction(pcVA,
					parsedInstruction,
					sizeof(parsedInstruction));
				length += snprintf(&pBuffer[length],
					PAGE_SIZE - length,
					"PC(VA): 0x%p, 0x%08x:0x%08x => %s",
					pcVA,
					insts[2],
					insts[3],
					parsedInstruction);
			} else {
				long pcPA = CMDQ_AREG_TO_PHYS(CMDQ_REG_GET32(
					CMDQ_THR_CURR_ADDR(index)));
				length += snprintf(
					&pBuffer[length],
					PAGE_SIZE - length,
					"PC(VA):Not available,PC(PA):0x%08lx\n",
					pcPA);
			}
		}

		spin_unlock_irqrestore(&gCmdqThreadLock, flags);

	}

	mutex_unlock(&gCmdqTaskMutex);

	length = pBuffer - buf;
	if (length > PAGE_SIZE)
		CMDQ_AEE("CMDQ",
		"Lehgth large than page size, length:%d page size:%lu\n",
			length, PAGE_SIZE);

	return length;

}

static void cmdq_task_init_profile_marker_data(
	struct cmdqCommandStruct *pCommandDesc, struct TaskStruct *pTask)
{
#ifdef CMDQ_PROFILE_MARKER_SUPPORT
	uint32_t i;

	pTask->profileMarker.count = pCommandDesc->profileMarker.count;
	pTask->profileMarker.hSlot = pCommandDesc->profileMarker.hSlot;
	for (i = 0; i < CMDQ_MAX_PROFILE_MARKER_IN_TASK; i++)
		pTask->profileMarker.tag[i] =
			pCommandDesc->profileMarker.tag[i];
#endif
}

static void cmdq_task_deinit_profile_marker_data(
	struct TaskStruct *pTask)
{
#ifdef CMDQ_PROFILE_MARKER_SUPPORT
	if (pTask == NULL)
		return;

	if ((pTask->profileMarker.count <= 0) ||
		(pTask->profileMarker.hSlot == 0))
		return;

	cmdq_free_mem((cmdqBackupSlotHandle) (pTask->profileMarker.hSlot));
	pTask->profileMarker.hSlot = 0LL;
	pTask->profileMarker.count = 0;
#endif
}

/*  */
/* For kmemcache, initialize variables of TaskStruct (but not buffers) */
static void cmdq_core_task_ctor(void *param)
{
	struct TaskStruct *pTask = (struct TaskStruct *) param;

	CMDQ_VERBOSE("%s: 0x%p\n", __func__, param);
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
		cmdq_core_free_hw_buffer(cmdq_dev_get(), CMDQ_CMD_BUFFER_SIZE,
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
		CMDQ_ERR("Unable to start work, free directly, task: 0x%p\n",
			pTask);
		cmdq_task_free_buffer_impl(&pTask->cmd_buffer_list);
		INIT_LIST_HEAD(&pTask->cmd_buffer_list);
	}

	CMDQ_INC_TIME_IN_US(startTime, sched_clock(), pTask->durRelease);

	pTask->buf_available_size = 0;
	pTask->bufferSize = 0;
	pTask->commandSize = 0;
	pTask->pCMDEnd = NULL;
}

static int32_t cmdq_core_task_alloc_single_buffer_list(
	struct TaskStruct *pTask,
	struct CmdBufferStruct **new_buffer_entry_handle)
{
	int32_t status = 0;
	struct CmdBufferStruct *buffer_entry = NULL;

	buffer_entry = kzalloc(sizeof(struct CmdBufferStruct), GFP_KERNEL);
	if (!buffer_entry) {
		CMDQ_ERR("allocate buffer record failed\n");
		return -ENOMEM;
	}

	buffer_entry->pVABase = cmdq_core_alloc_hw_buffer(cmdq_dev_get(),
		CMDQ_CMD_BUFFER_SIZE,
		&buffer_entry->MVABase, GFP_KERNEL);
	if (buffer_entry->pVABase == NULL) {
		CMDQ_ERR("allocate cmd buffer of size %u failed\n",
			(uint32_t)CMDQ_CMD_BUFFER_SIZE);
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

	pTask = (struct TaskStruct *) kmem_cache_alloc(gCmdqContext.taskCache,
		GFP_KERNEL);
	if (pTask == NULL) {
		CMDQ_AEE("CMDQ",
			"Allocate command buffer by kmem_cache_alloc failed\n");
		return NULL;
	}

	INIT_LIST_HEAD(&pTask->cmd_buffer_list);
	return pTask;
}

void cmdq_core_reset_hw_events_impl(enum CMDQ_EVENT_ENUM event)
{
	int32_t value = cmdq_core_get_event_value(event);

	if (value > 0) {
		/* Reset GCE event */
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD,
			(CMDQ_SYNC_TOKEN_MAX & value));
	}
}

void cmdq_core_reset_hw_events(void)
{
	int index;
	struct ResourceUnitStruct *pResource = NULL;
	struct list_head *p = NULL;

	/* set all defined events to 0 */
	CMDQ_MSG("%s\n", __func__);

#undef DECLARE_CMDQ_EVENT
#define DECLARE_CMDQ_EVENT(name, val, dts_name) \
{	\
	cmdq_core_reset_hw_events_impl(name);	\
}
#include "cmdq_event_common.h"
#undef DECLARE_CMDQ_EVENT

	/* However, GRP_SET are resource flags, */
	/* by default they should be 1. */
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_GPR_SET_0);
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_GPR_SET_1);
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_GPR_SET_2);
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_GPR_SET_3);
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_GPR_SET_4);

	/* CMDQ_SYNC_RESOURCE are resource flags, */
	/* by default they should be 1. */
	list_for_each(p, &gCmdqContext.resourceList) {
		pResource = list_entry(p, struct ResourceUnitStruct, listEntry);
		mutex_lock(&gCmdqResourceMutex);
		if (pResource->lend) {
			CMDQ_LOG("[Res] Client is already lend, event: %d\n",
				pResource->lockEvent);
			cmdqCoreClearEvent(pResource->lockEvent);
		} else {
			CMDQ_MSG("[Res] init resource event to 1: %d\n",
				pResource->lockEvent);
			cmdqCoreSetEvent(pResource->lockEvent);
		}
		mutex_unlock(&gCmdqResourceMutex);
	}

	/* However, CMDQ_SYNC_RESOURCE are WSM lock flags, */
	/* by default they should be 1. */
	cmdqCoreSetEvent(CMDQ_SYNC_SECURE_WSM_LOCK);

	/* However, APPEND_THR are resource flags, */
	/* by default they should be 1. */
	for (index = 0; index < CMDQ_MAX_THREAD_COUNT; index++)
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_APPEND_THR(index));
}


void cmdq_core_config_prefetch_gsize(void)
{
	if (g_dts_setting.prefetch_thread_count == 4) {
		uint32_t prefetch_gsize =
				(g_dts_setting.prefetch_size[0]/32-1) |
				(g_dts_setting.prefetch_size[1]/32-1) << 4 |
				(g_dts_setting.prefetch_size[2]/32-1) << 8 |
				(g_dts_setting.prefetch_size[3]/32-1) << 12;
		CMDQ_REG_SET32(CMDQ_PREFETCH_GSIZE, prefetch_gsize);
		CMDQ_MSG("prefetch gsize configure: 0x%08x\n", prefetch_gsize);
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
	struct ThreadStruct *pThread;
	int index;

	/* Reset thread status */
	pThread = &(gCmdqContext.thread[0]);
	for (index = 0; index < CMDQ_MAX_THREAD_COUNT; index++)
		pThread[index].allowDispatching = 1;
}

void cmdq_core_init_thread_work_queue(void)
{
	struct ThreadStruct *pThread;
	int index;

	/* Initialize work queue per thread */
	pThread = &(gCmdqContext.thread[0]);
	for (index = 0; index < CMDQ_MAX_THREAD_COUNT; index++) {
		gCmdqContext.taskThreadAutoReleaseWQ[index] =
		    create_singlethread_workqueue("cmdq_auto_release_thread");
	}
}

void cmdq_core_destroy_thread_work_queue(void)
{
	struct ThreadStruct *pThread;
	int index;

	/* Initialize work queue per thread */
	pThread = &(gCmdqContext.thread[0]);
	for (index = 0; index < CMDQ_MAX_THREAD_COUNT; index++) {
		destroy_workqueue(gCmdqContext.taskThreadAutoReleaseWQ[index]);
		gCmdqContext.taskThreadAutoReleaseWQ[index] = NULL;
	}
}

bool cmdq_core_is_valid_group(enum CMDQ_GROUP_ENUM engGroup)
{
	/* check range */
	if (engGroup < 0 || engGroup >= CMDQ_MAX_GROUP_COUNT)
		return false;

	return true;
}

int32_t cmdq_core_is_group_flag(enum CMDQ_GROUP_ENUM engGroup,
	uint64_t engineFlag)
{
	if (!cmdq_core_is_valid_group(engGroup))
		return false;

	if (gCmdqEngineGroupBits[engGroup] & engineFlag)
		return true;

	return false;
}

static inline uint32_t cmdq_core_get_task_timeout_cycle(
	struct ThreadStruct *pThread)
{
	/* if there is loop callback, this thread is in loop mode, */
	/* and should not have a timeout. */
	/* So pass 0 as "no timeout" */

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

int32_t cmdqCoreRegisterCB(enum CMDQ_GROUP_ENUM engGroup,
			   CmdqClockOnCB clockOn,
			   CmdqDumpInfoCB dumpInfo,
			   CmdqResetEngCB resetEng, CmdqClockOffCB clockOff)
{
	struct CmdqCBkStruct *pCallback;

	if (!cmdq_core_is_valid_group(engGroup))
		return -EFAULT;

	CMDQ_MSG("Register %d group engines' callback\n", engGroup);
	CMDQ_MSG("clockOn:  0x%p, dumpInfo: 0x%p\n", clockOn, dumpInfo);
	CMDQ_MSG("resetEng: 0x%p, clockOff: 0x%p\n", resetEng, clockOff);

	pCallback = &(gCmdqGroupCallback[engGroup]);

	pCallback->clockOn = clockOn;
	pCallback->dumpInfo = dumpInfo;
	pCallback->resetEng = resetEng;
	pCallback->clockOff = clockOff;

	return 0;
}

int32_t cmdqCoreRegisterDispatchModCB(enum CMDQ_GROUP_ENUM engGroup,
	CmdqDispatchModuleCB dispatchMod)
{
	struct CmdqCBkStruct *pCallback;

	if (!cmdq_core_is_valid_group(engGroup))
		return -EFAULT;

	CMDQ_MSG("Register %d group engines' dispatch callback\n", engGroup);
	pCallback = &(gCmdqGroupCallback[engGroup]);
	pCallback->dispatchMod = dispatchMod;

	return 0;
}

int32_t cmdqCoreRegisterDebugRegDumpCB(
	CmdqDebugRegDumpBeginCB beginCB, CmdqDebugRegDumpEndCB endCB)
{
	CMDQ_VERBOSE("Register reg dump: begin=%p, end=%p\n", beginCB, endCB);
	gCmdqDebugCallback.beginDebugRegDump = beginCB;
	gCmdqDebugCallback.endDebugRegDump = endCB;
	return 0;
}

int32_t cmdqCoreRegisterTrackTaskCB(enum CMDQ_GROUP_ENUM engGroup,
			   CmdqTrackTaskCB trackTask)
{
	struct CmdqCBkStruct *pCallback;

	if (!cmdq_core_is_valid_group(engGroup))
		return -EFAULT;

	CMDQ_MSG("Register %d group engines' callback\n", engGroup);
	CMDQ_MSG("trackTask:  %p\n", trackTask);

	pCallback = &(gCmdqGroupCallback[engGroup]);

	pCallback->trackTask = trackTask;

	return 0;
}

struct TaskStruct *cmdq_core_get_task_ptr(void *task_handle)
{
	struct TaskStruct *ptr = NULL;
	struct TaskStruct *task = NULL;

	mutex_lock(&gCmdqTaskMutex);

	list_for_each_entry(task, &gCmdqContext.taskActiveList, listEntry) {
		if (task == task_handle && TASK_STATE_IDLE != task->taskState) {
			ptr = task;
			break;
		}
	}

	if (!ptr) {
		list_for_each_entry(task, &gCmdqContext.taskWaitList,
			listEntry) {
			if (task == task_handle &&
				task->taskState == TASK_STATE_WAITING) {
				ptr = task;
				break;
			}
		}
	}

	mutex_unlock(&gCmdqTaskMutex);

	return ptr;
}

static void cmdq_core_release_buffer(struct TaskStruct *pTask)
{
	CMDQ_MSG("%s start\n", __func__);
	if (pTask->profileData) {
		cmdq_core_free_hw_buffer(cmdq_dev_get(),
			 2 * sizeof(uint32_t), pTask->profileData,
			 pTask->profileDataPA);
		pTask->profileData = NULL;
		pTask->profileDataPA = 0;
	}

	if (pTask->regResults) {
		CMDQ_MSG("COMMAND: Free result buf VA:0x%p, PA:%pa\n",
			pTask->regResults,
			&pTask->regResultsMVA);
		cmdq_core_free_hw_buffer(cmdq_dev_get(),
				 pTask->regCount * sizeof(pTask->regResults[0]),
				 pTask->regResults, pTask->regResultsMVA);
	}

	if (pTask->secData.addrMetadatas !=
		(cmdqU32Ptr_t) (unsigned long)NULL) {
		kfree(CMDQ_U32_PTR(pTask->secData.addrMetadatas));
		pTask->secData.addrMetadatas =
			(cmdqU32Ptr_t) (unsigned long)NULL;
	}

	if (pTask->userDebugStr != NULL) {
		kfree(pTask->userDebugStr);
		pTask->userDebugStr = NULL;
	}

	pTask->regResults = NULL;
	pTask->regResultsMVA = 0;
	pTask->regCount = 0;

	cmdq_task_free_task_command_buffer(pTask);

	cmdq_task_deinit_profile_marker_data(pTask);
	CMDQ_MSG("%s end\n", __func__);
}

static void cmdq_core_release_task_unlocked(struct TaskStruct *pTask)
{
	CMDQ_MSG("%s start\n", __func__);
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
	CMDQ_MSG("-->TASK: Release task structure 0x%p begin\n", pTask);

	pTask->taskState = TASK_STATE_IDLE;
	pTask->thread = CMDQ_INVALID_THREAD;

#if defined(CMDQ_SECURE_PATH_SUPPORT)
	kfree(pTask->secStatus);
	pTask->secStatus = NULL;
#endif

	mutex_lock(&gCmdqTaskMutex);

	cmdq_core_release_buffer(pTask);
	kfree(pTask->privateData);
	pTask->privateData = NULL;

	/* remove from active/waiting list */
	list_del_init(&(pTask->listEntry));
	/* insert into free list. Currently we don't shrink free list. */
	list_add_tail(&(pTask->listEntry), &gCmdqContext.taskFreeList);

	mutex_unlock(&gCmdqTaskMutex);

	CMDQ_MSG("<--TASK: Release task structure end\n");
}

static void cmdq_core_release_task_in_queue(
	struct work_struct *workItem)
{
	struct TaskStruct *pTask = NULL;

	pTask = container_of(workItem, struct TaskStruct, autoReleaseWork);

	CMDQ_MSG("-->Work QUEUE: TASK: Release task structure 0x%p begin\n",
		pTask);

	pTask->taskState = TASK_STATE_IDLE;
	pTask->thread = CMDQ_INVALID_THREAD;

#if defined(CMDQ_SECURE_PATH_SUPPORT)
	kfree(pTask->secStatus);
	pTask->secStatus = NULL;
#endif

	cmdq_core_release_buffer(pTask);
	kfree(pTask->privateData);
	pTask->privateData = NULL;

	mutex_lock(&gCmdqTaskMutex);

	/* remove from active/waiting list */
	list_del_init(&(pTask->listEntry));
	/* insert into free list. Currently we don't shrink free list. */
	list_add_tail(&(pTask->listEntry), &gCmdqContext.taskFreeList);

	mutex_unlock(&gCmdqTaskMutex);

	CMDQ_MSG("<--Work QUEUE: TASK: Release task structure end\n");
}

static void cmdq_core_auto_release_task(struct TaskStruct *pTask)
{
	CMDQ_MSG("-->TASK: Auto release task structure 0x%p begin\n", pTask);

	if (atomic_inc_return(&pTask->useWorkQueue) != 1) {
		/* this is called via auto release work, */
		/* no need to put in work queue again */
		cmdq_core_release_task(pTask);
	} else {
		/* Not auto release work, use for auto release task ! */
		/* the work item is embedded in pTask already */
		/* but we need to initialized it */
		INIT_WORK(&pTask->autoReleaseWork,
			cmdq_core_release_task_in_queue);
		queue_work(gCmdqContext.taskAutoReleaseWQ,
			&pTask->autoReleaseWork);
	}

	CMDQ_MSG("<--TASK: Auto release task structure end\n");
}

/**
 * Re-fetch thread's command buffer
 * Usage:
 *     If SW motifies command buffer content after SW configed command to GCE,
 *     SW should notify GCE to re-fetch command in
 *	order to ensure inconsistent command buffer content
 *     between DRAM and GCE's SRAM.
 */
void cmdq_core_invalidate_hw_fetched_buffer(int32_t thread)
{
	/* Setting HW thread PC will invoke that */
	/* GCE (CMDQ HW) gives up fetched command buffer, */
	/* and fetch command from DRAM to GCE's SRAM again. */
	const int32_t pc = CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread));

	CMDQ_REG_SET32(CMDQ_THR_CURR_ADDR(thread), pc);
}

void cmdq_core_fix_command_scenario_for_user_space(
	struct cmdqCommandStruct *pCommand)
{
	if ((pCommand->scenario == CMDQ_SCENARIO_USER_DISP_COLOR)
	    || (pCommand->scenario == CMDQ_SCENARIO_USER_MDP)) {
		CMDQ_VERBOSE("user space request, scenario:%d\n",
			pCommand->scenario);
	} else {
		CMDQ_VERBOSE("fix request to CMDQ_SCENARIO_USER_SPACE\n");
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

static int32_t cmdq_core_extend_cmd_buffer(struct TaskStruct *pTask)
{
	s32 status = 0;
	struct CmdBufferStruct *buffer_entry = NULL;
	uint32_t *va = NULL;

	va = cmdq_core_task_get_last_va(pTask);
	CMDQ_MSG("Extend from buffer size: %u available size: %u\n",
		pTask->bufferSize, pTask->buf_available_size);
	CMDQ_MSG("end: 0x%p last va: 0x%p\n",
		pTask->pCMDEnd, va);

	if (pTask->pCMDEnd != NULL) {
		/*
		 * There are instructions exist in available buffers.
		 * Check if buffer full and allocate new one.
		 */
		if (pTask->buf_available_size == 0) {
			bool is_eoc_end = (pTask->bufferSize >=
						2 * CMDQ_INST_SIZE &&
				((pTask->pCMDEnd[0] >> 24) & 0xff) ==
						CMDQ_CODE_JUMP &&
				((pTask->pCMDEnd[-2] >> 24) & 0xff) ==
						CMDQ_CODE_EOC);

			/*
			 * If last 2 instruction is EOC+JUMP,
			 * DO NOT copy last instruction to new buffer.
			 * So that we keep pCMDEnd[-2]:pCMDEnd[-3]
			 * can offset to EOC directly.
			 */
			if (is_eoc_end) {
				char buffer[200] = {0};
				char num = 0;

				num += snprintf(buffer + num,
					sizeof(buffer) - num,
					"Extend after EOC+END not support, ");
				num += snprintf(buffer + num,
					sizeof(buffer) - num,
					"task:0x%p inst:0x%08x:%08x size:%u\n",
					pTask, pTask->pCMDEnd[-1],
					pTask->pCMDEnd[0],
					pTask->bufferSize);

				CMDQ_AEE("CMDQ", "%s", buffer);


				return -EFAULT;
			}

			status = cmdq_core_task_alloc_single_buffer_list(pTask,
				&buffer_entry);
			if (status < 0)
				return status;

			/* copy last instruction to head of new */
			/* buffer and use jump to replace */
			buffer_entry->pVABase[0] = pTask->pCMDEnd[-1];
			buffer_entry->pVABase[1] = pTask->pCMDEnd[0];

			/* In normal case, insert */
			/* jump to jump start of new buffer. */
			pTask->pCMDEnd[-1] = CMDQ_PHYS_TO_AREG(
				buffer_entry->MVABase);
			/* jump to absolute addr */
			pTask->pCMDEnd[0] = (CMDQ_CODE_JUMP << 24 | 1);

			/* update pCMDEnd to new buffer when not eoc end case */
			pTask->pCMDEnd = buffer_entry->pVABase + 1;
			/* update buffer size since we insert 1 jump */
			/* update available size */
			pTask->buf_available_size -= CMDQ_INST_SIZE;
			/* +1 for jump instruction */
			pTask->bufferSize += CMDQ_INST_SIZE;

			if (unlikely(cmdq_core_task_is_buffer_size_valid(pTask)
					== false)) {
				char buffer[200] = {0};
				int num = 0;

				num += snprintf(buffer + num,
				sizeof(buffer) - num,
					"Buffer size: %u, available size: %u ",
					pTask->bufferSize,
					pTask->buf_available_size);
				num += snprintf(buffer + num,
					sizeof(buffer) - num,
					"of %u and end cmd:0x%p first va:0x%p",
					(uint32_t)CMDQ_CMD_BUFFER_SIZE,
					pTask->pCMDEnd,
					cmdq_core_task_get_first_va(pTask));
				num += snprintf(buffer + num,
					sizeof(buffer) - num,
					"out of sync!\n");



				CMDQ_AEE("CMDQ", "%s", buffer);
				cmdq_core_dump_task(pTask);
			}
		}
	} else {
		/* allocate first buffer */
		status = cmdq_core_task_alloc_single_buffer_list(pTask,
			&buffer_entry);
		if (status < 0)
			return status;

		pTask->pCMDEnd = buffer_entry->pVABase - 1;

		if (pTask->bufferSize != 0) {
			/* no instruction, buffer size should be 0 */
			CMDQ_ERR("Task buffer size not sync, size: %u\n",
				pTask->bufferSize);
		}
	}

	va = cmdq_core_task_get_last_va(pTask);
	CMDQ_MSG("Extend to buffer size: %u available size: %u\n",
		pTask->bufferSize, pTask->buf_available_size);
	CMDQ_MSG("end: 0x%p last va: 0x%p\n",
		pTask->pCMDEnd, va);

	return status;
}

static void cmdq_core_append_command(struct TaskStruct *pTask,
	uint32_t arg_a, uint32_t arg_b)
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

	mutex_lock(&gCmdqTaskMutex);

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

	mutex_unlock(&gCmdqTaskMutex);
}

static void cmdq_core_insert_backup_instr(struct TaskStruct *pTask,
		  const uint32_t regAddr,
		  const dma_addr_t writeAddress,
		  const enum CMDQ_DATA_REGISTER_ENUM valueRegId,
		  const enum CMDQ_DATA_REGISTER_ENUM destRegId)
{
	uint32_t arg_a;
	int32_t subsysCode;
	uint32_t highAddr = 0;

	/* register to read from */
	/* note that we force convert to physical reg address. */
	/* if it is already physical address, */
	/* it won't be affected (at least on this platform) */
	arg_a = regAddr;
	subsysCode = cmdq_core_subsys_from_phys_addr(arg_a);

	/* CMDQ_ERR("test %d\n", __LINE__); */
	/*  */

	if (subsysCode == CMDQ_SPECIAL_SUBSYS_ADDR) {
		CMDQ_LOG("Backup: Special handle memory base address 0x%08x\n",
			arg_a);
		/* Move extra handle APB address to destRegId */
		cmdq_core_append_command(pTask,
			 (CMDQ_CODE_MOVE << 24) | ((destRegId & 0x1f) << 16) |
			 (4 << 21),
			 arg_a);
		/* Use arg-A GPR enable instruction to */
		/* read destRegId value to valueRegId */
		cmdq_core_append_command(pTask,
					 (CMDQ_CODE_READ << 24) |
					 ((destRegId & 0x1f) << 16) |
					 (6 << 21),
					 valueRegId);
	} else if (-1 == subsysCode) {
		CMDQ_ERR("Backup: Unsupported memory base address 0x%08x\n",
			arg_a);
	} else {
		/* Load into 32-bit GPR (R0-R15) */
		cmdq_core_append_command(pTask,
			(CMDQ_CODE_READ << 24) | (arg_a & 0xffff) |
			((subsysCode & 0x1f) << 16) | (2 << 21),
			valueRegId);
	}

	/* CMDQ_ERR("test %d\n", __LINE__); */

	/* Note that <MOVE> arg_b is 48-bit */
	/* so writeAddress is split into 2 parts */
	/* and we store address in 64-bit GPR (P0-P7) */
	CMDQ_GET_HIGH_ADDR(writeAddress, highAddr);
	cmdq_core_append_command(pTask, (CMDQ_CODE_MOVE << 24) |
				 highAddr |
				 ((destRegId & 0x1f) << 16) | (4 << 21),
				 (uint32_t) writeAddress);

	/* CMDQ_ERR("test %d\n", __LINE__); */

	/* write to memory */
	cmdq_core_append_command(pTask,
				 (CMDQ_CODE_WRITE << 24) | (0 & 0xffff) |
				 ((destRegId & 0x1f) << 16) | (6 << 21),
				 valueRegId);

	CMDQ_VERBOSE("COMMAND: copy reg:0x%08x to phys:%pa, GPR(%d, %d)\n",
		arg_a, &writeAddress,
		valueRegId, destRegId);

	/* CMDQ_ERR("test %d\n", __LINE__); */
}

/**
 * Insert instruction to back secure threads' cookie count to normal world
 * Return:
 *     < 0, return the error code
 *     >=0, okay case, return number of bytes for inserting instruction
 */
#ifdef CMDQ_SECURE_PATH_NORMAL_IRQ
static int32_t cmdq_core_insert_backup_cookie_instr(
	struct TaskStruct *pTask, int32_t thread)
{
	const enum CMDQ_DATA_REGISTER_ENUM valueRegId = CMDQ_DATA_REG_DEBUG;
	const enum CMDQ_DATA_REGISTER_ENUM destRegId = CMDQ_DATA_REG_DEBUG_DST;
	const uint32_t regAddr = CMDQ_THR_EXEC_CNT_PA(thread);
	uint64_t addrCookieOffset = CMDQ_SEC_SHARED_THR_CNT_OFFSET +
		thread * sizeof(uint32_t);
	uint64_t WSMCookieAddr = gCmdqContext.hSecSharedMem->MVABase +
		addrCookieOffset;
	const uint32_t subsysBit = cmdq_get_func()->getSubsysLSBArgA();
	int32_t subsysCode = cmdq_core_subsys_from_phys_addr(regAddr);
	uint32_t highAddr = 0;

	const enum CMDQ_EVENT_ENUM regAccessToken = CMDQ_SYNC_TOKEN_GPR_SET_4;

	if (gCmdqContext.hSecSharedMem == NULL) {
		CMDQ_ERR("%s shared memory is not created\n", __func__);
		return -EFAULT;
	}

	/* use SYNC TOKEN to make sure only 1 thread access at a time */
	/* bit 0-11: wait_value */
	/* bit 15: to_wait, true */
	/* bit 31: to_update, true */
	/* bit 16-27: update_value */
	/* wait and clear */
	cmdq_core_append_command(pTask, (CMDQ_CODE_WFE << 24) | regAccessToken,
		((1 << 31) | (1 << 15) | 1));

	/* Load into 32-bit GPR (R0-R15) */
	cmdq_core_append_command(pTask,
		(CMDQ_CODE_READ << 24) | (regAddr & 0xffff) |
		((subsysCode & 0x1f) << subsysBit) | (2 << 21), valueRegId);

	/* Note that <MOVE> arg_b is 48-bit */
	/* so writeAddress is split into 2 parts */
	/* and we store address in 64-bit GPR (P0-P7) */
	CMDQ_GET_HIGH_ADDR(WSMCookieAddr, highAddr);
	cmdq_core_append_command(pTask,
		(CMDQ_CODE_MOVE << 24) | highAddr |
		((destRegId & 0x1f) << 16) | (4 << 21),
		(uint32_t) WSMCookieAddr);

	/* write to memory */
	cmdq_core_append_command(pTask,
		(CMDQ_CODE_WRITE << 24) |
		((destRegId & 0x1f) << 16) | (6 << 21), valueRegId);

	/* set directly */
	cmdq_core_append_command(pTask, (CMDQ_CODE_WFE << 24) | regAccessToken,
		((1 << 31) | (1 << 16)));

	return 0;
}
#endif	/* CMDQ_SECURE_PATH_NORMAL_IRQ */

/**
 * Insert instruction to backup secure threads' cookie and IRQ to normal world
 * Return:
 *     < 0, return the error code
 *     >=0, okay case, return number of bytes for inserting instruction
 */
#ifdef CMDQ_SECURE_PATH_HW_LOCK
static int32_t cmdq_core_insert_secure_IRQ_instr(
	struct TaskStruct *pTask, int32_t thread)
{
	const uint32_t originalSize = pTask->commandSize;
	const enum CMDQ_EVENT_ENUM regAccessToken = CMDQ_SYNC_TOKEN_GPR_SET_4;
	const enum CMDQ_DATA_REGISTER_ENUM valueRegId = CMDQ_DATA_REG_DEBUG;
	const enum CMDQ_DATA_REGISTER_ENUM destRegId = CMDQ_DATA_REG_DEBUG_DST;
	const uint32_t regAddr = CMDQ_THR_EXEC_CNT_PA(thread);
	uint64_t addrCookieOffset = CMDQ_SEC_SHARED_THR_CNT_OFFSET +
			thread * sizeof(uint32_t);
	uint64_t WSMCookieAddr = gCmdqContext.hSecSharedMem->MVABase +
			addrCookieOffset;
	uint64_t WSMIRQAddr = gCmdqContext.hSecSharedMem->MVABase +
			CMDQ_SEC_SHARED_IRQ_RAISED_OFFSET;
	const uint32_t subsysBit = cmdq_get_func()->getSubsysLSBArgA();
	int32_t subsysCode = cmdq_core_subsys_from_phys_addr(regAddr);
	int32_t offset;

	if (cmdq_get_func()->isSecureThread(thread) == false) {
		CMDQ_ERR("%s, invalid param, thread: %d\n", __func__, thread);
		return -EFAULT;
	}

	if (gCmdqContext.hSecSharedMem == NULL) {
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
	cmdq_core_append_command(pTask,
		(CMDQ_CODE_WFE << 24) | CMDQ_SYNC_SECURE_WSM_LOCK,
		((1 << 31) | (1 << 15) | 1));

	cmdq_core_append_command(pTask, (CMDQ_CODE_WFE << 24) | regAccessToken,
				 ((1 << 31) | (1 << 15) | 1));

	/* Load into 32-bit GPR (R0-R15) */
	cmdq_core_append_command(pTask,
				 (CMDQ_CODE_READ << 24) | (regAddr & 0xffff) |
				 ((subsysCode & 0x1f) << subsysBit) | (2 << 21),
				 valueRegId);

	/* Note that <MOVE> arg_b is 48-bit */
	/* so writeAddress is split into 2 parts */
	/* and we store address in 64-bit GPR (P0-P7) */
	cmdq_core_append_command(pTask,
				 (CMDQ_CODE_MOVE << 24) |
				 ((WSMCookieAddr >> 32) & 0xffff) |
				 ((destRegId & 0x1f) << 16) | (4 << 21),
				 (uint32_t) WSMCookieAddr);

	/* write to memory */
	cmdq_core_append_command(pTask,
				 (CMDQ_CODE_WRITE << 24) |
				 ((destRegId & 0x1f) << 16) | (6 << 21),
				 valueRegId);

	/* Write GCE secure thread's IRQ to WSM */
	cmdq_core_append_command(pTask,
				 (CMDQ_CODE_MOVE << 24), ~(1 << thread));

	cmdq_core_append_command(pTask,
				 (CMDQ_CODE_MOVE << 24) |
				 ((destRegId & 0x1f) << 16) | (4 << 21),
				 WSMIRQAddr);

	cmdq_core_append_command(pTask,
				 (CMDQ_CODE_WRITE << 24) |
				 ((destRegId & 0x1f) << 16) | (4 << 21) | 1,
				 (1 << thread));

	/* set directly */
	cmdq_core_append_command(pTask, (CMDQ_CODE_WFE << 24) | regAccessToken,
				 ((1 << 31) | (1 << 16)));

	/* set unlock WSM resource directly */
	cmdq_core_append_command(pTask, (CMDQ_CODE_WFE << 24) |
		CMDQ_SYNC_SECURE_WSM_LOCK,
		((1 << 31) | (1 << 16)));

	/* set notify thread token directly */
	cmdq_core_append_command(pTask, (CMDQ_CODE_WFE << 24) |
		CMDQ_SYNC_SECURE_THR_EOF,
		((1 << 31) | (1 << 16)));

	pTask->pCMDEnd += 4;

	/* secure thread doesn't raise IRQ */
	pTask->pCMDEnd[-3] = pTask->pCMDEnd[-3] & 0xFFFFFFFE;

	/* calculate added command length */
	offset = pTask->commandSize - originalSize;

	CMDQ_VERBOSE("insert_backup_cookie, offset:%d\n", offset);

	return offset;
}
#endif

static int32_t cmdq_core_insert_secure_handle_instr(
	struct TaskStruct *pTask, int32_t thread)
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

static void cmdq_core_reorder_task_array(struct ThreadStruct *pThread,
	int32_t thread, int32_t prevID)
{
	int loop, nextID, searchLoop, searchID;
	int reorderCount = 0;

	nextID = prevID + 1;
	for (loop = 1; loop < (cmdq_core_max_task_in_thread(thread) - 1);
		loop++, nextID++) {
		if (nextID >= cmdq_core_max_task_in_thread(thread))
			nextID = 0;

		if (pThread->pCurTask[nextID] != NULL)
			break;

		searchID = nextID + 1;
		for (searchLoop = (loop + 1); searchLoop <
				cmdq_core_max_task_in_thread(thread);
		     searchLoop++, searchID++) {
			if (searchID >= cmdq_core_max_task_in_thread(thread))
				searchID = 0;

			if (pThread->pCurTask[searchID] != NULL) {
				pThread->pCurTask[nextID] =
					pThread->pCurTask[searchID];
				pThread->pCurTask[searchID] = NULL;
				CMDQ_VERBOSE("reorder slot %d to slot 0%d.\n",
					     searchID, nextID);
				if ((searchLoop - loop) > reorderCount)
					reorderCount = searchLoop - loop;

				break;
			}
		}

		if (pThread->pCurTask[nextID] &&
			((pThread->pCurTask[nextID]->pCMDEnd[0] >> 24) & 0xff)
				== CMDQ_CODE_JUMP &&
			CMDQ_IS_END_ADDR(
				pThread->pCurTask[nextID]->pCMDEnd[-1])) {
			/* We reached the last task */
			CMDQ_LOG("Break in last task loop: %d nextID: %d\n",
				loop, nextID);
			CMDQ_LOG("searchLoop: %d searchID: %d\n",
				searchLoop, searchID);
			break;
		}
	}

	pThread->nextCookie -= reorderCount;
	CMDQ_VERBOSE("WAIT: nextcookie minus %d.\n", reorderCount);
}

static int32_t cmdq_core_copy_buffer_impl(void *dst, void *src,
	const uint32_t size,
	const bool copyFromUser)
{
	int32_t status = 0;

	if (copyFromUser == false) {
		CMDQ_VERBOSE("COMMAND: Copy kernel to 0x%p\n", dst);
		memcpy(dst, src, size);
	} else {
		CMDQ_VERBOSE("COMMAND: Copy user to 0x%p\n", dst);
		if (copy_from_user(dst, src, size)) {
			char buffer[200] = {0};
			int num = 0;

			num += snprintf(buffer + num,
				sizeof(buffer) - num,
				"CRDISPATCH_KEY:CMDQ Fail to copy from user");
			num += snprintf(buffer + num,
				sizeof(buffer) - num,
				" 0x%p, size:%d\n",
				src, size);
			CMDQ_AEE("CMDQ", "%s", buffer);
			status = -ENOMEM;
		}
	}

	return status;
}

int32_t cmdq_core_copy_cmd_to_task_impl(struct TaskStruct *pTask,
	void *src, const uint32_t size,
	const bool is_copy_from_user)
{
	s32 status = 0;
	uint32_t remaind_cmd_size = size;
	uint32_t copy_size = 0;

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
		pTask->pCMDEnd += (copy_size / sizeof(uint32_t));
		pTask->buf_available_size -= copy_size;
		pTask->bufferSize += copy_size;
		remaind_cmd_size -= copy_size;

		if (unlikely(cmdq_core_task_is_buffer_size_valid(pTask) ==
			false)) {
			/* buffer size is total size and */
			/* should sync with available space */
			char buffer[200] = {0};
			int num = 0;

			num += snprintf(buffer + num,
				sizeof(buffer) - num,
				"Buffer size: %u,available size: %u of %u and",
				pTask->bufferSize, pTask->buf_available_size,
				(uint32_t)CMDQ_CMD_BUFFER_SIZE);
			num += snprintf(buffer + num,
				sizeof(buffer) - num,
				" end cmd: 0x%p first va: 0x%p out of sync!\n",
				pTask->pCMDEnd,
				cmdq_core_task_get_first_va(pTask));

			CMDQ_AEE("CMDQ", "%s", buffer);
			cmdq_core_dump_task(pTask);
		}
	}

	return status;
}

static dma_addr_t cmdq_core_get_current_pa_addr(
	struct TaskStruct *pTask)
{
	return (cmdq_core_task_get_last_pa(pTask) +
		CMDQ_CMD_BUFFER_SIZE - pTask->buf_available_size);
}

bool cmdq_core_verfiy_command_desc_end(
	struct cmdqCommandStruct *pCommandDesc)
{
	uint32_t *pCMDEnd = NULL;
	bool valid = true;
	bool internal_desc = pCommandDesc->privateData &&
		((struct TaskPrivateStruct *)
		(CMDQ_U32_PTR(pCommandDesc->privateData)))->internal;

	/* make sure we have sufficient command to parse */
	if (!CMDQ_U32_PTR(pCommandDesc->pVABase) ||
		pCommandDesc->blockSize < (2 * CMDQ_INST_SIZE))
		return false;

	if (cmdq_core_is_request_from_user_space(
			pCommandDesc->scenario) == true) {
		/* command buffer has not copied */
		/* from user space yet, skip verify. */
		return true;
	}

	pCMDEnd =
	    CMDQ_U32_PTR(pCommandDesc->pVABase) +
		(pCommandDesc->blockSize / sizeof(uint32_t)) - 1;

	/* make sure the command is ended by EOC + JUMP */
	if ((pCMDEnd[-3] & 0x1) != 1 && !internal_desc) {
		CMDQ_ERR
		    ("[CMD] command desc 0x%p does not throw\n",
			pCommandDesc);
		CMDQ_ERR
		    ("IRQ (%08x:%08x), pEnd:%p(%p, %d)\n",
			pCMDEnd[-3], pCMDEnd[-2], pCMDEnd,
			CMDQ_U32_PTR(pCommandDesc->pVABase),
			pCommandDesc->blockSize);
		valid = false;
	}

	if (((pCMDEnd[-2] & 0xFF000000) >> 24) != CMDQ_CODE_EOC ||
	    ((pCMDEnd[0] & 0xFF000000) >> 24) != CMDQ_CODE_JUMP) {
		CMDQ_ERR("[CMD] command desc 0x%p does not end in EOC+JUMP\n",
			pCommandDesc);
		CMDQ_ERR("[CMD] (%08x:%08x, %08x:%08x), pEnd:%p(%p, %d)\n",
			pCMDEnd[-3], pCMDEnd[-2],
			pCMDEnd[-1], pCMDEnd[0], pCMDEnd,
			CMDQ_U32_PTR(pCommandDesc->pVABase),
			pCommandDesc->blockSize);
		valid = false;
	}

	if (valid == false) {
		/* invalid command, raise AEE */
		CMDQ_AEE("CMDQ", "INVALID command desc 0x%p\n",
			pCommandDesc);
	}

	return valid;
}

bool cmdq_core_verfiy_command_end(const struct TaskStruct *pTask)
{
	bool valid = true;
	bool noIRQ = false;
	uint32_t *last_inst = NULL;
	struct CmdBufferStruct *cmd_buffer = NULL;

	/* make sure we have sufficient command to parse */
	if (list_empty(&pTask->cmd_buffer_list) ||
		pTask->commandSize < (2 * CMDQ_INST_SIZE))
		return false;

#ifdef CMDQ_SECURE_PATH_HW_LOCK
	if ((pTask->pCMDEnd[-3] & 0x1) != 1 &&
		pTask->secData.is_secure == false)
		noIRQ = true;
#else
	if ((pTask->pCMDEnd[-3] & 0x1) != 1)
		noIRQ = true;
#endif

	/* make sure the command is ended by EOC + JUMP */
	if (noIRQ) {
		if (cmdq_get_func()->is_disp_loop(pTask->scenario)) {
			/* Allow display only loop not throw IRQ */
			CMDQ_MSG("[CMD] DISP Loop pTask 0x%p does not\n",
				 pTask);
			CMDQ_MSG("[CMD] throw IRQ (%08x:%08x)\n",
				pTask->pCMDEnd[-3], pTask->pCMDEnd[-2]);
		} else {
			CMDQ_ERR("pTask 0x%p does not throw IRQ(%08x:%08x)\n",
				 pTask, pTask->pCMDEnd[-3],
				 pTask->pCMDEnd[-2]);
			valid = false;
		}
	}
	if (((pTask->pCMDEnd[-2] & 0xFF000000) >> 24) != CMDQ_CODE_EOC ||
	    ((pTask->pCMDEnd[0] & 0xFF000000) >> 24) != CMDQ_CODE_JUMP) {
		CMDQ_ERR("Task:0x%p not end in EOC+JUMP(%08x:%08x,%08x:%08x)\n",
			pTask,
			pTask->pCMDEnd[-3], pTask->pCMDEnd[-2],
			pTask->pCMDEnd[-1],
			pTask->pCMDEnd[0]);
		valid = false;
	}

	/* verify end of each buffer will jump to next buffer */
	list_for_each_entry(cmd_buffer, &pTask->cmd_buffer_list, listEntry) {
		bool last_entry = list_is_last(&cmd_buffer->listEntry,
			&pTask->cmd_buffer_list);

		if (last_inst) {
			if (last_entry && pTask->pCMDEnd - 1 == last_inst) {
				/* EOC+JUMP command locate */
				/* at last 2nd buffer, skip test */
				break;
			}
			if ((last_inst[1] & 0x1) != 1 || last_inst[0] !=
				cmd_buffer->MVABase) {
				CMDQ_ERR(
					"Invalid task: 0x%p buffer jump instruction: 0x%08x:%08x next PA: 0x%pa cmd end:0x%p\n",
					pTask, last_inst[1], last_inst[0],
					&cmd_buffer->MVABase, pTask->pCMDEnd);
				cmdq_core_dump_buffer(pTask);
				valid = false;
				break;
			}
		}

		if (!last_entry) {
			last_inst = &cmd_buffer->pVABase[
				CMDQ_CMD_BUFFER_SIZE / sizeof(uint32_t) - 2];
			if (last_inst[1] >> 24 != CMDQ_CODE_JUMP) {
				CMDQ_ERR("Invalid task: 0x%p\n",
					pTask);
				CMDQ_ERR("instr: 0x%08x:%08x is not jump\n",
					last_inst[1], last_inst[0]);
				cmdq_core_dump_buffer(pTask);
				valid = false;
				break;
			}
		}
	}

	if (valid == false) {
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
			"Finalize JUMP: 0x%08x:%08x last pa: 0x%pa buffer size: %d cmd size: %d line: %d\n",
			pTask->pCMDEnd[0], pTask->pCMDEnd[-1], &pa,
			pTask->bufferSize, pTask->commandSize, __LINE__);
	} else if ((pTask->pCMDEnd[0] & 0x1) == 0 &&
		*((int32_t *)(pTask->pCMDEnd - 1)) ==
		(-pTask->commandSize + CMDQ_INST_SIZE)) {
		/* JUMP to head of command, loop case. */
		pa = cmdq_core_task_get_first_pa(pTask);
		pTask->pCMDEnd[-1] = CMDQ_PHYS_TO_AREG(pa);
		pTask->pCMDEnd[0] = (CMDQ_CODE_JUMP << 24 | 0x1);

		CMDQ_MSG(
			"Finalize JUMP: 0x%08x:%08x first pa: 0x%pa\n",
			pTask->pCMDEnd[0], pTask->pCMDEnd[-1], &pa);
		CMDQ_MSG(
			"buffer size: %d cmd size: %d line: %d\n",
			pTask->bufferSize, pTask->commandSize, __LINE__);
	} else {
		char buffer[200] = {0};
		int num = 0;

		num += snprintf(buffer + num, sizeof(buffer) - num,
			"Final JUMP un-expect, task: 0x%p",
			pTask);

		num += snprintf(buffer + num, sizeof(buffer) - num,
			"inst: (0x%p) 0x%08x:%08x size: %u(%u)\n",
			pTask->pCMDEnd,
			pTask->pCMDEnd[0], pTask->pCMDEnd[-1],
			pTask->commandSize, pTask->bufferSize);

		CMDQ_AEE("CMDQ", "%s", buffer);
		return false;
	}

	return true;
}

static struct TaskStruct *cmdq_core_find_free_task(void)
{
	struct TaskStruct *pTask = NULL;

	mutex_lock(&gCmdqTaskMutex);

	/* Pick from free list first; */
	/* create one if there is no free entry. */
	if (list_empty(&gCmdqContext.taskFreeList)) {
		pTask = cmdq_core_task_create();
	} else {
		pTask = list_first_entry(&(gCmdqContext.taskFreeList),
			struct TaskStruct, listEntry);
		/* remove from free list */
		list_del_init(&(pTask->listEntry));
	}

	mutex_unlock(&gCmdqTaskMutex);

	return pTask;
}

static bool cmdq_core_check_gpr_valid(const uint32_t gpr, const bool val)
{
	if (val)
		switch (gpr) {
		case CMDQ_DATA_REG_JPEG:
		case CMDQ_DATA_REG_PQ_COLOR:
		case CMDQ_DATA_REG_2D_SHARPNESS_0:
		case CMDQ_DATA_REG_2D_SHARPNESS_1:
		case CMDQ_DATA_REG_DEBUG:
			return true;
		default:
			return false;
		}
	else
		switch (gpr >> 16) {
		case CMDQ_DATA_REG_JPEG_DST:
		case CMDQ_DATA_REG_PQ_COLOR_DST:
		case CMDQ_DATA_REG_2D_SHARPNESS_0:
		case CMDQ_DATA_REG_2D_SHARPNESS_0_DST:
		case CMDQ_DATA_REG_2D_SHARPNESS_1_DST:
		case CMDQ_DATA_REG_DEBUG_DST:
			return true;
		default:
			return false;
		}
	return false;
}

static bool cmdq_core_check_dma_addr_valid(const unsigned long pa)
{
	struct WriteAddrStruct *pWriteAddr = NULL;
	unsigned long flagsWriteAddr = 0L;
	phys_addr_t start = memblock_start_of_DRAM();
	bool ret = false;

	spin_lock_irqsave(&gCmdqWriteAddrLock, flagsWriteAddr);
	list_for_each_entry(pWriteAddr, &gCmdqContext.writeAddrList, list_node)
		if (pa < start || pa - (unsigned long)pWriteAddr->pa <
			pWriteAddr->count << 2) {
			ret = true;
			break;
		}
	spin_unlock_irqrestore(&gCmdqWriteAddrLock, flagsWriteAddr);
	return ret;
}

static bool cmdq_core_check_instr_valid(const uint64_t instr)
{
	u32 op = instr >> 56, option = (instr >> 53) & 0x7;
	u32 argA = (instr >> 32) & 0x1FFFFF, argB = instr & 0xFFFFFFFF;

	switch (op) {
	case CMDQ_CODE_WRITE:
		if (!option)
			return true;
		if (option == 0x4 && cmdq_core_check_gpr_valid(argA, false))
			return true;
	case CMDQ_CODE_READ:
		if (option == 0x2 && cmdq_core_check_gpr_valid(argB, true))
			return true;
		if (option == 0x6 && cmdq_core_check_gpr_valid(argA, false) &&
			cmdq_core_check_gpr_valid(argB, true))
			return true;
		break;
	case CMDQ_CODE_MOVE:
		if (!option && !argA)
			return true;
		if (option == 0x4 && cmdq_core_check_gpr_valid(argA, false) &&
			cmdq_core_check_dma_addr_valid(argB))
			return true;
		break;
	case CMDQ_CODE_JUMP:
		if (!argA && argB == 0x8)
			return true;
		break;
	default:
		return true;
	}
	return false;
}

static bool cmdq_core_check_task_valid(struct TaskStruct *pTask)
{

	struct CmdBufferStruct *cmd_buffer = NULL;
	int32_t cmd_size = CMDQ_CMD_BUFFER_SIZE;
	uint64_t *va;
	bool ret = true;

	list_for_each_entry(cmd_buffer, &pTask->cmd_buffer_list, listEntry) {
		if (list_is_last(&cmd_buffer->listEntry,
			&pTask->cmd_buffer_list))
			cmd_size -= pTask->buf_available_size;

		for (va = (uint64_t *)cmd_buffer->pVABase; ret &&
			(unsigned long)(va + 1) <
			(unsigned long)cmd_buffer->pVABase + cmd_size; va++)
			ret &= cmdq_core_check_instr_valid(*va);

		if (ret && (*va >> 56) != CMDQ_CODE_JUMP)
			ret &= cmdq_core_check_instr_valid(*va);
		if (!ret)
			break;
	}
	return ret;
}

static int32_t cmdq_core_insert_read_reg_command(
	struct TaskStruct *pTask,
	struct cmdqCommandStruct *pCommandDesc)
{
	/* #define CMDQ_PROFILE_COMMAND */

	int32_t status = 0;
	uint32_t extraBufferSize = 0;
	int i = 0;
	enum CMDQ_DATA_REGISTER_ENUM valueRegId;
	enum CMDQ_DATA_REGISTER_ENUM destRegId;
	enum CMDQ_EVENT_ENUM regAccessToken;
	const bool userSpaceRequest = false;
	bool postInstruction = false;

	int32_t subsysCode;
	uint32_t physAddr;

	uint32_t *copyCmdSrc = NULL;
	uint32_t copyCmdSize = 0;
	uint32_t cmdLoopStartAddr = 0;

	/* calculate required buffer size */
	/* we need to consider {READ, MOVE, WRITE} for each register */
	/* and the SYNC in the begin and end */
	if (pTask->regCount && pTask->regCount <= CMDQ_MAX_DUMP_REG_COUNT) {
		extraBufferSize = (3 * CMDQ_INST_SIZE * pTask->regCount) +
			(2 * CMDQ_INST_SIZE);
		/* Add move instruction count for handle */
		/* Extra APB address (add move instructions) */
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

	CMDQ_VERBOSE("test %d, original command size = %d\n",
		__LINE__, pTask->commandSize);

	/* init pCMDEnd */
	/* mark command end to NULL as initial state */
	pTask->pCMDEnd = NULL;

	/* backup command start mva for loop case */
	cmdLoopStartAddr = (uint32_t)cmdq_core_get_current_pa_addr(pTask);

	/* for post instruction process we copy last 2 inst later */
	postInstruction = (pTask->regCount != 0 && extraBufferSize != 0) ||
		pTask->secData.is_secure;

	/* Copy the commands to our DMA buffer, */
	/* except last 2 instruction EOC+JUMP. */
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

	CMDQ_VERBOSE("CMDEnd: %p cmdSize: %d bufferSize: %u block size: %u\n",
		pTask->pCMDEnd, pTask->commandSize,
		pTask->bufferSize, pCommandDesc->blockSize);

	if (userSpaceRequest && !cmdq_core_check_task_valid(pTask))
		return -EFAULT;

	/* If no read request, no post-process needed. Do verify and stop */
	if (postInstruction == false) {
		if (cmdq_core_task_finalize_end(pTask) == false) {
			CMDQ_ERR("[CMD]cmdSize:%d bufferSize:%u blockSize:%d\n",
				pTask->commandSize, pTask->bufferSize,
				pCommandDesc->blockSize);
			cmdq_core_dump_task(pTask);
			cmdq_core_dump_all_task();
		}

		return 0;
	}

	/**
	 * Backup end and append cmd
	 */

	if (pTask->regCount && pTask->regCount <= CMDQ_MAX_DUMP_REG_COUNT) {
		CMDQ_VERBOSE("COMMAND: allocate register output section\n");
		/* allocate register output section */
		if (pTask->regResults)
			CMDQ_AEE("CMDQ",
				"Result is not empty, addr:0x%p task:0x%p\n",
				pTask->regResults, pTask);
		pTask->regResults = cmdq_core_alloc_hw_buffer(cmdq_dev_get(),
			pTask->regCount * sizeof(pTask->regResults[0]),
			&pTask->regResultsMVA,
			GFP_KERNEL);
		CMDQ_MSG("COMMAND: result buf VA: 0x%p, PA: %pa\n",
			pTask->regResults,
			&pTask->regResultsMVA);

		/* allocate GPR resource */
		cmdq_get_func()->getRegID(pTask->engineFlag,
					&valueRegId, &destRegId,
					  &regAccessToken);

		/* use SYNC TOKEN to make sure only 1 thread access at a time */
		/* bit 0-11: wait_value */
		/* bit 15: to_wait, true */
		/* bit 31: to_update, true */
		/* bit 16-27: update_value */

		/* wait and clear */
		cmdq_core_append_command(pTask,
					 (CMDQ_CODE_WFE << 24) |
					 regAccessToken,
					 ((1 << 31) | (1 << 15) | 1));

		for (i = 0; i < pTask->regCount; ++i) {
			cmdq_core_insert_backup_instr(pTask,
				CMDQ_U32_PTR(
				pCommandDesc->regRequest.regAddresses)[i],
				pTask->regResultsMVA +
				(i * sizeof(pTask->regResults[0])),
				valueRegId, destRegId);
		}

		/* set directly */
		cmdq_core_append_command(pTask,
					 (CMDQ_CODE_WFE << 24) | regAccessToken,
					 ((1 << 31) | (1 << 16)));
	}

	if (pTask->secData.is_secure) {
		/*
		 * Use scenario to get thread ID before acquire thread,
		 * use ID to insert secure instruction.
		 * The acquire thread process may still fail later.
		 */
		int32_t thread = cmdq_get_func()->getThreadID(pTask->scenario,
			true);

		status = cmdq_core_insert_secure_handle_instr(pTask, thread);
		if (status < 0)
			return status;
	}

	/* copy END instructsions EOC+JUMP */
	status = cmdq_core_copy_cmd_to_task_impl(pTask,
			copyCmdSrc + (copyCmdSize / sizeof(copyCmdSrc[0])),
			2 * CMDQ_INST_SIZE, userSpaceRequest);
	if (status < 0)
		return status;

	/* make sure instructions are really in DRAM */
	smp_mb();

	CMDQ_VERBOSE("[CMD]CMDEnd:%p cmdSize:%d bufferSize:%u block size:%u\n",
		pTask->pCMDEnd,
		pTask->commandSize,
		pTask->bufferSize, pCommandDesc->blockSize);

	if (cmdq_core_task_finalize_end(pTask) == false) {
		CMDQ_ERR("[CMD] cmdSize: %d bufferSize: %u blockSize: %d\n",
			pTask->commandSize,
			pTask->bufferSize,
			pCommandDesc->blockSize);
		cmdq_core_dump_task(pTask);
		cmdq_core_dump_all_task();
	}

	return status;
}

static struct TaskStruct *cmdq_core_acquire_task(
	struct cmdqCommandStruct *pCommandDesc,
	CmdqInterruptCB loopCB, unsigned long loopData)
{
	struct TaskStruct *pTask = NULL;
	int32_t status;

	CMDQ_MSG("-->TASK: acquire task begin CMD: 0x%p\n",
		CMDQ_U32_PTR(pCommandDesc->pVABase));
	CMDQ_MSG("-->TASK: size: %d, Eng: 0x%016llx\n",
		pCommandDesc->blockSize,
		pCommandDesc->engineFlag);
	CMDQ_PROF_START(current->pid, __func__);

	pTask = cmdq_core_find_free_task();
	do {
		struct TaskPrivateStruct *private = NULL, *desc_private = NULL;

		if (pTask == NULL) {
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
#if defined(CONFIG_MTK_CMDQ_TAB)
		pTask->secData.secMode = pCommandDesc->secData.secMode;
#endif
#endif

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

		if (pTask->secData.is_secure == true &&
			pCommandDesc->secData.addrMetadataCount > 0 &&
			pCommandDesc->secData.addrMetadataCount <
				CMDQ_IWC_MAX_ADDR_LIST_LENGTH) {
			u32 metadata_length = 0;
			void *p_metadatas = NULL;

			pTask->secData.addrMetadataCount =
				pCommandDesc->secData.addrMetadataCount;
			metadata_length = (pTask->secData.addrMetadataCount) *
				sizeof(struct cmdqSecAddrMetadataStruct);
			/* create sec data task buffer for working */
			p_metadatas = kzalloc(metadata_length, GFP_KERNEL);
			if (p_metadatas == NULL) {
				/* raise AEE first */
				char buffer[200] = {0};
				int num = 0;

				num += snprintf(buffer + num,
					sizeof(buffer) - num,
					"Can't alloc secData buffer,count:%d,",
					pTask->secData.addrMetadataCount);
				num += snprintf(buffer + num,
					sizeof(buffer) - num,
					" alloacted_size:%d\n",
					metadata_length);
				CMDQ_AEE("CMDQ", "%s", buffer);

				/* then release task */
				cmdq_core_release_task(pTask);
				pTask = NULL;
				break;
			}
			memcpy(p_metadatas,
				CMDQ_U32_PTR(
					pCommandDesc->secData.addrMetadatas),
			       metadata_length);
			pTask->secData.addrMetadatas =
				(cmdqU32Ptr_t)(unsigned long)p_metadatas;
		} else {
			pTask->secData.addrMetadatas =
				(cmdqU32Ptr_t)(unsigned long)NULL;
			pTask->secData.addrMetadataCount = 0;
		}
#endif

		/* profile data for command profiling */
		if (cmdq_get_func()->shouldProfile(pTask->scenario)) {
			pTask->profileData =
			    cmdq_core_alloc_hw_buffer(cmdq_dev_get(),
						      2 * sizeof(uint32_t),
						      &pTask->profileDataPA,
						      GFP_KERNEL);
		} else {
			pTask->profileData = NULL;
		}

		/* profile timers */
		memset(&(pTask->trigger), 0x0, sizeof(pTask->trigger));
		memset(&(pTask->gotIRQ), 0x0, sizeof(pTask->gotIRQ));
		memset(&(pTask->beginWait), 0x0, sizeof(pTask->beginWait));
		memset(&(pTask->wakedUp), 0x0, sizeof(pTask->wakedUp));

		/* profile marker */
		cmdq_task_init_profile_marker_data(pCommandDesc, pTask);

		pTask->commandSize = pCommandDesc->blockSize;
		pTask->regCount = pCommandDesc->regRequest.count;

		/* store caller info for debug */
		if (current) {
			pTask->callerPid = current->pid;
			memcpy(pTask->callerName,
				current->comm, sizeof(current->comm));
		}

		/* store user debug string for debug */
		if (pCommandDesc->userDebugStr != 0 &&
			pCommandDesc->userDebugStrLen > 0) {
			pTask->userDebugStr = kzalloc(
				pCommandDesc->userDebugStrLen, GFP_KERNEL);
			if (pTask->userDebugStr != NULL) {
				int len = 0;

				len = strncpy_from_user(pTask->userDebugStr,
						(const char *)(unsigned long)
						(pCommandDesc->userDebugStr),
						pCommandDesc->userDebugStrLen);
				if (len < 0) {
					CMDQ_ERR("copy memory fail,size:%d\n",
						pCommandDesc->userDebugStrLen);
				} else if (len ==
					pCommandDesc->userDebugStrLen) {
					pTask->userDebugStr[
						pCommandDesc->userDebugStrLen
						- 1] = '\0';
				}
				CMDQ_MSG("user debug string: %s\n",
					(const char *)(unsigned long)(
						pCommandDesc->userDebugStr));
			} else {
				CMDQ_ERR("allocate memory failed, size: %d\n",
					pCommandDesc->userDebugStrLen);
			}
		}

		status = cmdq_core_insert_read_reg_command(pTask, pCommandDesc);
		if (status < 0) {
			/* raise AEE first */
			CMDQ_AEE("CMDQ", "Can't alloc command buffer\n");

			/* then release task */
			cmdq_core_release_task(pTask);
			pTask = NULL;
		}
	} while (0);

	/*  */
	/* insert into waiting list to process */
	/*  */
	mutex_lock(&gCmdqTaskMutex);
	if (pTask) {
		struct list_head *insertAfter = &gCmdqContext.taskWaitList;

		struct TaskStruct *taskEntry = NULL;
		struct list_head *p = NULL;

		/* add to waiting list, keep it sorted by priority */
		/* so that we add high-priority tasks first. */
		list_for_each(p, &gCmdqContext.taskWaitList) {
			taskEntry = list_entry(p, struct TaskStruct, listEntry);
			/* keep the list sorted. */
			/* higher priority tasks are  */
			/* inserted in front of the queue */
			if (taskEntry->priority < pTask->priority)
				break;

			insertAfter = p;
		}

		list_add(&(pTask->listEntry), insertAfter);
	}
	mutex_unlock(&gCmdqTaskMutex);

	CMDQ_MSG("<--TASK: acquire task 0x%p end\n", pTask);
	CMDQ_PROF_END(current->pid, __func__);
	return pTask;
}

bool cmdq_core_is_clock_enabled(void)
{
	return (atomic_read(&gCmdqThreadUsage) > 0);
}

static void cmdq_core_enable_common_clock_locked(const bool enable,
				 const uint64_t engineFlag,
				 enum CMDQ_SCENARIO_ENUM scenario)
{
	/* CMDQ(GCE) clock */
	if (enable) {
		CMDQ_VERBOSE("[CLOCK] Enable CMDQ(GCE) Clock test=%d SMI %d\n",
			     atomic_read(&gCmdqThreadUsage),
			     atomic_read(&gSMIThreadUsage));

		if (atomic_read(&gCmdqThreadUsage) == 0) {
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
			/* Restore event */
			cmdq_get_func()->eventRestore();

		}
		atomic_inc(&gCmdqThreadUsage);

		/* SMI related threads common clock enable, */
		/* excluding display scenario on his own */
		if (!cmdq_get_func()->isDispScenario(scenario)) {
			if (atomic_read(&gSMIThreadUsage) == 0) {
				CMDQ_VERBOSE("[CLOCK] SMI clock enable %d\n",
					scenario);
				cmdq_get_func()->enableCommonClockLocked(
					enable);
			}
			atomic_inc(&gSMIThreadUsage);
		}

	} else {
		atomic_dec(&gCmdqThreadUsage);

		CMDQ_VERBOSE("[CLOCK] Disable CMDQ(GCE) Clock test=%d SMI %d\n",
			     atomic_read(&gCmdqThreadUsage),
			     atomic_read(&gSMIThreadUsage));
		if (atomic_read(&gCmdqThreadUsage) <= 0) {
			/* Backup event */
			cmdq_get_func()->eventBackup();
			/* clock-off */
			cmdq_get_func()->enableGCEClockLocked(enable);
		}

		/* SMI related threads common clock enable, */
		/* excluding display scenario on his own */
		if (!cmdq_get_func()->isDispScenario(scenario)) {
			atomic_dec(&gSMIThreadUsage);

			if (atomic_read(&gSMIThreadUsage) <= 0) {
				CMDQ_VERBOSE("[CLOCK] SMI clock disable %d\n",
					scenario);
				cmdq_get_func()->enableCommonClockLocked(
					enable);
			}
		}
	}
}

static uint64_t cmdq_core_get_actual_engine_flag_for_enable_clock(
	uint64_t engineFlag,
	int32_t thread)
{
	struct EngineStruct *pEngine;
	struct ThreadStruct *pThread;
	uint64_t engines;
	int32_t index;

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

static int32_t gCmdqISPClockCounter;

static void cmdq_core_enable_clock(uint64_t engineFlag,
				   int32_t thread,
				   uint64_t engineMustEnableClock,
				   enum CMDQ_SCENARIO_ENUM scenario)
{
	const uint64_t engines = engineMustEnableClock;
	int32_t index;
	struct CmdqCBkStruct *pCallback;
	int32_t status;

	CMDQ_VERBOSE("-->CLOCK: Enable flag 0x%llx thread %d begin\n",
		engineFlag, thread);
	CMDQ_VERBOSE("-->CLOCK: mustEnable: 0x%llx(0x%llx)\n",
		engineMustEnableClock, engines);

	/* enable fundamental clocks if needed */
	cmdq_core_enable_common_clock_locked(true, engineFlag, scenario);

	pCallback = gCmdqGroupCallback;

	/* ISP special check: Always call ISP on/off if this task */
	/* involves ISP. Ignore the ISP HW flags. */
	if (cmdq_core_is_group_flag(CMDQ_GROUP_ISP, engineFlag)) {
		CMDQ_VERBOSE("CLOCK: enable group %d clockOn\n",
			CMDQ_GROUP_ISP);

		if (pCallback[CMDQ_GROUP_ISP].clockOn == NULL) {
			CMDQ_ERR("CLOCK: enable group %d clockOn func NULL\n",
				CMDQ_GROUP_ISP);
		} else {
			status = pCallback[CMDQ_GROUP_ISP].clockOn(
				gCmdqEngineGroupBits[CMDQ_GROUP_ISP] &
				engineFlag);

			++gCmdqISPClockCounter;

			if (status < 0) {
				/* Error status print */
				CMDQ_ERR("CLK:enable group:%d clockOn fail\n",
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
			if (pCallback[index].clockOn == NULL) {
				CMDQ_LOG("[WARNING]CLOCK: enable group %d\n",
					 index);
				CMDQ_LOG("clockOn func NULL\n");
				continue;
			}
			status = pCallback[index].clockOn(
				gCmdqEngineGroupBits[index] & engines);
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

static int32_t cmdq_core_can_start_to_acquire_HW_thread_unlocked(
	const uint64_t engineFlag,
	const bool is_secure)
{
	struct TaskStruct *pFirstWaitingTask = NULL;
	struct TaskStruct *pTempTask = NULL;
	struct list_head *p = NULL;
	bool preferSecurePath;
	int32_t status = 0;
	char longMsg[CMDQ_LONGSTRING_MAX];
	uint32_t msgOffset;
	int32_t msgMAXSize;

	/* find the first waiting task with OVERLAPPED engine flag with pTask */
	list_for_each(p, &gCmdqContext.taskWaitList) {
		pTempTask = list_entry(p, struct TaskStruct, listEntry);
		if (pTempTask != NULL &&
			(engineFlag & (pTempTask->engineFlag))) {
			pFirstWaitingTask = pTempTask;
			break;
		}
	}

	do {
		if (pFirstWaitingTask == NULL) {
			/* no waiting task with overlape */
			/* engine, go to dispath thread */
			break;
		}

		preferSecurePath = pFirstWaitingTask->secData.is_secure;

		if (preferSecurePath == is_secure) {
			/* same security path as first */
			/* waiting task, go to start to thread dispatch */
			cmdq_core_longstring_init(longMsg,
				&msgOffset, &msgMAXSize);
			cmdqCoreLongString(false, longMsg,
				&msgOffset, &msgMAXSize,
				"THREAD: is sec(%d, eng:0x%llx) as first waiting task",
				is_secure, engineFlag);
			cmdqCoreLongString(false, longMsg, &msgOffset,
				&msgMAXSize,
				"(0x%p, eng:0x%llx), start thread dispatch.\n",
				pFirstWaitingTask,
				pFirstWaitingTask->engineFlag);
			if (msgOffset > 0) {
				/* print message */
				CMDQ_MSG("%s", longMsg);
			}
			break;
		}

		CMDQ_VERBOSE("THR:not the first waiting task(0x%p), yield.\n",
			     pFirstWaitingTask);
		status = -EFAULT;
	} while (0);

	return status;
}

/**
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
	const uint64_t engineFlag,
	bool forceLog,
	const bool is_secure, int32_t *pThreadOut)
{
	struct EngineStruct *pEngine;
	struct ThreadStruct *pThread;
	uint32_t free;
	int32_t index;
	int32_t thread;
	uint64_t engine;
	bool isEngineConflict;
	char longMsg[CMDQ_LONGSTRING_MAX];
	uint32_t msgOffset;
	int32_t msgMAXSize;

	pEngine = gCmdqContext.engine;
	pThread = gCmdqContext.thread;
	isEngineConflict = false;

	engine = engineFlag;
	thread = (*pThreadOut);
	free = (thread == CMDQ_INVALID_THREAD) ? 0xFFFFFFFF :
		0xFFFFFFFF & (~(0x1 << thread));

	/* check if engine conflict */
	for (index = 0;
		((index < CMDQ_MAX_ENGINE_COUNT) && (engine != 0));
		index++) {
		if (engine & (0x1LL << index)) {
			if (pEngine[index].currOwner == CMDQ_INVALID_THREAD) {
				continue;
			} else if (thread == CMDQ_INVALID_THREAD) {
				thread = pEngine[index].currOwner;
				free &= ~(0x1 << thread);
			} else if (thread != pEngine[index].currOwner) {
				/* Partial HW occupied by different threads, */
				/* we need to wait. */
				if (forceLog) {
					cmdq_core_longstring_init(longMsg,
						&msgOffset, &msgMAXSize);
					cmdqCoreLongString(true, longMsg,
						&msgOffset, &msgMAXSize,
						"THREAD: try locate on thr %d",
						thread);
					cmdqCoreLongString(true, longMsg,
						&msgOffset, &msgMAXSize,
						" but engine %d",
						index);
					cmdqCoreLongString(true, longMsg,
						&msgOffset, &msgMAXSize,
						" also occupied by thread %d,",
						pEngine[index].currOwner);
					cmdqCoreLongString(true, longMsg,
						&msgOffset, &msgMAXSize,
						" secure:%d\n",
						is_secure);
					if (msgOffset > 0) {
						/* print message */
						CMDQ_LOG("%s", longMsg);
					}
				} else {
					cmdq_core_longstring_init(longMsg,
						&msgOffset, &msgMAXSize);
					cmdqCoreLongString(false, longMsg,
						&msgOffset, &msgMAXSize,
						"THREAD: try locate on thr %d",
						thread);
					cmdqCoreLongString(false, longMsg,
						&msgOffset, &msgMAXSize,
						"but engine %d",
						index);

					cmdqCoreLongString(false,
						longMsg, &msgOffset,
						&msgMAXSize,
						" also occupied by thread %d,",
						pEngine[index].currOwner);
					cmdqCoreLongString(false,
						longMsg, &msgOffset,
						&msgMAXSize,
						" secure:%d\n",
						is_secure);
					if (msgOffset > 0) {
						/* print message */
						CMDQ_VERBOSE("%s", longMsg);
					}
				}

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

static int32_t cmdq_core_find_a_free_HW_thread(uint64_t engineFlag,
	enum CMDQ_HW_THREAD_PRIORITY_ENUM thread_prio,
	enum CMDQ_SCENARIO_ENUM scenario, bool forceLog,
	const bool is_secure)
{
	struct ThreadStruct *pThread;
	unsigned long flagsExecLock;
	int32_t index;
	int32_t thread;
	bool isEngineConflict;

	int32_t insertCookie;

	pThread = gCmdqContext.thread;

	do {
		CMDQ_VERBOSE
		    ("find free thr, eng:0x%llx, scenario:%d, secure:%d\n",
		     engineFlag, scenario, is_secure);

		/* start to dispatch? */
		/* note we should not favor secure or normal path, */
		/* traverse waiting list to decide that we should */
		/* dispatch thread to secure or normal path */
		if (cmdq_core_can_start_to_acquire_HW_thread_unlocked(
			engineFlag, is_secure) < 0) {
			thread = CMDQ_INVALID_THREAD;
			break;
		}

		/* it's okey to dispatch thread, */
		/* use scenario and pTask->secure to get default thread */
		thread = cmdq_get_func()->getThreadID(scenario, is_secure);

		/* check if engine conflict happened except DISP scenario */
		isEngineConflict = false;
		if (cmdq_get_func()->isDispScenario(scenario) == false) {
			isEngineConflict =
				cmdq_core_check_engine_conflict_unlocked(
					engineFlag,
					forceLog,
					is_secure,
					&thread);
		}
		CMDQ_VERBOSE("THREAD: isEngineConflict:%d, thread:%d\n",
			isEngineConflict, thread);

		/* TODO: secure path proting */
		/* because all thread are pre-dispatched,
		 * there 2 outcome of engine conflict check:
		 * 1. pre-dispatched secure thread,
		 * and no conflict with normal path
		 * 2. pre-dispatched secure thread,
		 * but conflict with normal/anothor secure path
		 * no need to check get normal thread in secure path
		 */

		/* ensure not dispatch secure thread to normal task */
		if ((is_secure == false) &&
			(cmdq_get_func()->isSecureThread(thread) == true)) {
			thread = CMDQ_INVALID_THREAD;
			isEngineConflict = true;
			break;
		}
		/* no enfine conflict with running thread, */
		/* AND used engines have no owner */
		/* try to find a free thread */
		if ((isEngineConflict == false) &&
			(thread == CMDQ_INVALID_THREAD)) {

			/* thread 0 - CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT */
			/* are preserved for DISPSYS */
			const bool isDisplayThread =
				thread_prio > CMDQ_THR_PRIO_DISPLAY_TRIGGER;
			int startIndex = isDisplayThread ? 0 :
				CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT;
#ifdef CMDQ_SECURE_PATH_SUPPORT
			int endIndex = isDisplayThread ?
			    CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT :
			    CMDQ_MAX_THREAD_COUNT -
				CMDQ_MAX_SECURE_THREAD_COUNT;
#else
			int endIndex = isDisplayThread ?
			    CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT :
			    CMDQ_MAX_THREAD_COUNT;
#endif

			for (index = startIndex; index < endIndex; ++index) {

				spin_lock_irqsave(&gCmdqExecLock,
					flagsExecLock);

				if ((pThread[index].engineFlag == 0) &&
				    (pThread[index].taskCount == 0) &&
				    (pThread[index].allowDispatching == 1)) {

					CMDQ_VERBOSE
					("THREAD:dispatch to thread %d\n",
						index);
					CMDQ_VERBOSE
					("taskCount:%d, allowDispatching:%d\n",
						pThread[index].taskCount,
						pThread[
						index].allowDispatching);

					thread = index;
					pThread[index].allowDispatching = 0;
					spin_unlock_irqrestore(&gCmdqExecLock,
						flagsExecLock);
					break;
				}

				spin_unlock_irqrestore(&gCmdqExecLock,
					flagsExecLock);
			}
		}

		/* no thread available now, wait for it */
		if (thread == CMDQ_INVALID_THREAD)
			break;

		/* Make sure the found thread has enough space for the task; */
		/* ThreadStruct->pCurTask has size limitation. */
		if (cmdq_core_max_task_in_thread(thread) <=
			pThread[thread].taskCount) {
			if (forceLog) {
				CMDQ_LOG("THREAD: thread %d count:%d full\n",
					 thread, pThread[thread].taskCount);
			} else {
				CMDQ_VERBOSE("THREAD:thr:%d count:%d full\n",
					     thread, pThread[thread].taskCount);
			}

			thread = CMDQ_INVALID_THREAD;
		} else {
			insertCookie = pThread[thread].nextCookie %
				cmdq_core_max_task_in_thread(thread);
			if (pThread[thread].pCurTask[insertCookie] == NULL)
				break;
			/* pThread[thread].pCurTask[insertCookie] != NULL */
			if (forceLog) {
				CMDQ_LOG("THREAD: thread %d nextCookie = %d\n",
					 thread,
					 pThread[thread].nextCookie);
				CMDQ_LOG("already has task\n");
			} else {
				CMDQ_VERBOSE("THREAD: %d nextCookie=%d\n",
				     thread,
				     pThread[thread].nextCookie);
				CMDQ_VERBOSE("already has task\n");
			}

			thread = CMDQ_INVALID_THREAD;
		}
	} while (0);

	return thread;
}

static int32_t cmdq_core_acquire_thread(uint64_t engineFlag,
		enum CMDQ_HW_THREAD_PRIORITY_ENUM thread_prio,
		enum CMDQ_SCENARIO_ENUM scenario, bool forceLog,
		const bool is_secure)
{
	unsigned long flags;
	int32_t thread;
	uint64_t engineMustEnableClock;

	CMDQ_PROF_START(current->pid, __func__);

	do {
		mutex_lock(&gCmdqClockMutex);
		spin_lock_irqsave(&gCmdqThreadLock, flags);

		thread = cmdq_core_find_a_free_HW_thread(engineFlag,
			thread_prio,
			scenario, forceLog,
			is_secure);

		if (thread != CMDQ_INVALID_THREAD) {
			/* get actual engine flag. Each bit represents */
			/* a engine must enable clock. */
			engineMustEnableClock =
			    cmdq_core_get_actual_engine_flag_for_enable_clock(
				engineFlag, thread);
		}

		if (thread == CMDQ_INVALID_THREAD &&
			is_secure == true &&
			scenario == CMDQ_SCENARIO_USER_MDP)
			g_cmdq_consume_again = true;

		spin_unlock_irqrestore(&gCmdqThreadLock, flags);

		if (thread != CMDQ_INVALID_THREAD) {
			/* enable clock */
			cmdq_core_enable_clock(engineFlag, thread,
				engineMustEnableClock, scenario);
		}

		mutex_unlock(&gCmdqClockMutex);
	} while (0);

	CMDQ_PROF_END(current->pid, __func__);

	return thread;
}

static uint64_t cmdq_core_get_not_used_engine_flag_for_disable_clock(
	const uint64_t engineFlag)
{
	struct EngineStruct *pEngine;
	struct ThreadStruct *pThread;
	uint64_t enginesNotUsed;
	int32_t index;
	int32_t currOwnerThread = CMDQ_INVALID_THREAD;

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
	CMDQ_VERBOSE("%s, enginesNotUsed:0x%llx\n", __func__, enginesNotUsed);
	return enginesNotUsed;
}

static void cmdq_core_disable_clock(uint64_t engineFlag,
				    const uint64_t enginesNotUsed,
				    enum CMDQ_SCENARIO_ENUM scenario)
{
	int32_t index;
	int32_t status;
	struct CmdqCBkStruct *pCallback;

	CMDQ_VERBOSE("-->CLOCK: Disable hardware clock 0x%llx begin\n",
		engineFlag);
	CMDQ_VERBOSE("-->CLOCK: enginesNotUsed 0x%llx\n",
		enginesNotUsed);

	pCallback = gCmdqGroupCallback;

	/* ISP special check: Always call ISP on/off if this task */
	/* involves ISP. Ignore the ISP HW flags ref count. */
	if (cmdq_core_is_group_flag(CMDQ_GROUP_ISP, engineFlag)) {
		CMDQ_VERBOSE("CLOCK: disable group %d clockOff\n",
			CMDQ_GROUP_ISP);
		if (pCallback[CMDQ_GROUP_ISP].clockOff == NULL) {
			CMDQ_ERR("CLK: disable group %d clockOff func NULL\n",
				CMDQ_GROUP_ISP);
		} else {
			status =
			    pCallback[CMDQ_GROUP_ISP].clockOff(
				gCmdqEngineGroupBits[CMDQ_GROUP_ISP] &
					engineFlag);

			--gCmdqISPClockCounter;
			if (gCmdqISPClockCounter != 0) {
				/* ISP clock off */
				CMDQ_VERBOSE("CLOCK: ISP clockOff cnt=%d\n",
					gCmdqISPClockCounter);
			}

			if (status < 0)
				CMDQ_ERR("CLK:disable ISP clockOff fail\n");
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
			CMDQ_MSG("CLOCK: Disable engine\n");
			CMDQ_MSG("group %d flag=0x%llx clockOff\n",
				index,
				enginesNotUsed);
			if (pCallback[index].clockOff == NULL) {
				CMDQ_LOG
				    ("[WARNING]CLOCK: Disable engine\n");
				CMDQ_LOG
				    ("group %d clockOff func NULL\n", index);
				continue;
			}
			status =
			    pCallback[index].clockOff(
				gCmdqEngineGroupBits[index] & enginesNotUsed);
			if (status < 0) {
				/* Error status print */
				CMDQ_ERR("CLK:Disable eng group %d clk fail\n",
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
	const int32_t thread = pTask->thread;
	const uint64_t engineFlag = pTask->engineFlag;
	uint64_t engineNotUsed = 0LL;

	if (thread == CMDQ_INVALID_THREAD)
		return;

	mutex_lock(&gCmdqClockMutex);
	spin_lock_irqsave(&gCmdqThreadLock, flags);

	/* get not used engines for disable clock */
	engineNotUsed = cmdq_core_get_not_used_engine_flag_for_disable_clock(
		engineFlag);
	pTask->thread = CMDQ_INVALID_THREAD;

	spin_unlock_irqrestore(&gCmdqThreadLock, flags);

	/* clock off */
	cmdq_core_disable_clock(engineFlag, engineNotUsed, pTask->scenario);
	/* Delay release resource  */
	cmdq_core_delay_check_unlock(engineFlag, engineNotUsed);

	mutex_unlock(&gCmdqClockMutex);
}

static void cmdq_core_reset_hw_engine(int32_t engineFlag)
{
	struct EngineStruct *pEngine;
	uint32_t engines;
	int32_t index;
	int32_t status;
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
			if (pCallback[index].resetEng == NULL) {
				CMDQ_ERR("Reset eng group %d clk func NULL\n",
					index);
				continue;
			}
			status =
			    pCallback[index].resetEng(
				gCmdqEngineGroupBits[index] & engineFlag);
			if (status < 0) {
				/* Error status print */
				CMDQ_ERR("Reset engine group %d clock failed\n",
					index);
			}
		}
	}

	CMDQ_MSG("Reset hardware engine end\n");
}

uint32_t cmdq_core_subsys_to_reg_addr(uint32_t arg_a)
{
	const uint32_t subsysBit = cmdq_get_func()->getSubsysLSBArgA();
	const int32_t subsys_id = (arg_a & CMDQ_ARG_A_SUBSYS_MASK) >> subsysBit;
	uint32_t offset = 0;
	uint32_t base_addr = 0;
	uint32_t i;

	for (i = 0; i < CMDQ_SUBSYS_MAX_COUNT; i++) {
		if (gCmdqDtsData.subsys[i].subsysID == subsys_id) {
			base_addr = gCmdqDtsData.subsys[i].msb;
			offset = (arg_a & ~gCmdqDtsData.subsys[i].mask);
			break;
		}
	}

	return base_addr | offset;
}

const char *cmdq_core_parse_subsys_from_reg_addr(uint32_t reg_addr)
{
	uint32_t addr_base_shifted;
	const char *module = "CMDQ";
	uint32_t i;

	for (i = 0; i < CMDQ_SUBSYS_MAX_COUNT; i++) {
		if (-1 == gCmdqDtsData.subsys[i].subsysID)
			continue;

		addr_base_shifted = (reg_addr & gCmdqDtsData.subsys[i].mask);
		if (gCmdqDtsData.subsys[i].msb == addr_base_shifted) {
			module = gCmdqDtsData.subsys[i].grpName;
			break;
		}
	}

	return module;
}

int32_t cmdq_core_subsys_from_phys_addr(uint32_t physAddr)
{
	int32_t msb;
	int32_t subsysID = CMDQ_SPECIAL_SUBSYS_ADDR;
	uint32_t i;

	for (i = 0; i < CMDQ_SUBSYS_MAX_COUNT; i++) {
		if (-1 == gCmdqDtsData.subsys[i].subsysID)
			continue;

		msb = (physAddr & gCmdqDtsData.subsys[i].mask);
		if (msb == gCmdqDtsData.subsys[i].msb) {
			subsysID = gCmdqDtsData.subsys[i].subsysID;
			break;
		}
	}

	return subsysID;
}

static const char *cmdq_core_parse_op(uint32_t op_code)
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
	}
	return NULL;
}

static void cmdq_core_parse_error(const struct TaskStruct *pTask,
	uint32_t thread,
	const char **moduleName, int32_t *flag, uint32_t *instA,
	uint32_t *instB)
{
	uint32_t op, arg_a, arg_b;
	int32_t eventENUM;
	uint32_t insts[4] = { 0 };
	uint32_t addr = 0;
	const char *module = NULL;
	int32_t irqFlag = pTask->irqFlag;
	int isSMIHang = 0;

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
		if (cmdq_core_get_pc(pTask, thread, insts)) {
			op = (insts[3] & 0xFF000000) >> 24;
			arg_a = insts[3] & (~0xFF000000);
			arg_b = insts[2];

			/* quick exam by hwflag first */
			module = cmdq_get_func()->parseErrorModule(pTask);
			if (module != NULL)
				break;

			switch (op) {
			case CMDQ_CODE_POLL:
			case CMDQ_CODE_WRITE:
				addr = cmdq_core_subsys_to_reg_addr(arg_a);
				module = cmdq_get_func()->parseModule(addr);
				break;
			case CMDQ_CODE_WFE:
				/* arg_a is the event ID */
				eventENUM = cmdq_core_reverse_event_ENUM(arg_a);
				module = cmdq_get_func()->moduleFromEvent(
					eventENUM,
					gCmdqGroupCallback, pTask->engineFlag);
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
	*flag = irqFlag;
	*instA = insts[3];
	*instB = insts[2];

}

void cmdq_core_dump_resource_status(
	enum CMDQ_EVENT_ENUM resourceEvent)
{
	struct ResourceUnitStruct *pResource = NULL;
	struct list_head *p = NULL;

	if (cmdq_core_is_feature_off(CMDQ_FEATURE_SRAM_SHARE))
		return;

	list_for_each(p, &gCmdqContext.resourceList) {
		pResource = list_entry(p, struct ResourceUnitStruct, listEntry);
		if (resourceEvent == pResource->lockEvent) {
			CMDQ_ERR("[Res] Dump resource with event: %d\n",
				resourceEvent);
			mutex_lock(&gCmdqResourceMutex);
			/* find matched resource */
			CMDQ_ERR("[Res] Dump resource latest time:\n");
			CMDQ_ERR("[Res]   notify: %llu, delay: %lld\n",
				pResource->notify, pResource->delay);
			CMDQ_ERR("[Res]   lock: %llu, unlock: %lld\n",
				pResource->lock, pResource->unlock);
			CMDQ_ERR("[Res]   acquire: %llu, release: %lld\n",
				pResource->acquire, pResource->release);
			CMDQ_ERR("[Res] isUsed:%d, isLend:%d, isDelay:%d\n",
				pResource->used, pResource->lend,
				pResource->delaying);
			if (pResource->releaseCB == NULL)
				CMDQ_ERR("[Res]: release CB func is NULL\n");
			mutex_unlock(&gCmdqResourceMutex);
			break;
		}
	}
}

static uint32_t *cmdq_core_dump_pc(const struct TaskStruct *pTask,
	int thread, const char *tag)
{
	uint32_t *pcVA = NULL;
	uint32_t insts[4] = { 0 };
	char parsedInstruction[128] = { 0 };

	pcVA = cmdq_core_get_pc(pTask, thread, insts);
	if (pcVA) {
		const uint32_t op = (insts[3] & 0xFF000000) >> 24;

		cmdq_core_parse_instruction(pcVA, parsedInstruction,
			sizeof(parsedInstruction));

		/* for WFE, we specifically dump the event value */
		if (op == CMDQ_CODE_WFE) {
			uint32_t regValue = 0;
			const uint32_t eventID = 0x3FF & insts[3];

			CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_ID, eventID);
			regValue = CMDQ_REG_GET32(CMDQ_SYNC_TOKEN_VAL);
			CMDQ_LOG("[%s]Thread %d PC(VA): 0x%p\n",
				tag, thread, pcVA);
			CMDQ_LOG("[%s]0x%08x:0x%08x => %s, value:(%d)\n",
				tag, insts[2], insts[3],
				parsedInstruction, regValue);
			cmdq_core_dump_resource_status(eventID);
		} else {
			CMDQ_LOG("[%s]Thr %d PC(VA):0x%p,0x%08x:0x%08x => %s",
			 tag, thread, pcVA, insts[2], insts[3],
			 parsedInstruction);
		}
	} else {
		if (pTask->secData.is_secure == true) {
			CMDQ_LOG("[%s]Thread %d PC(VA):HIDDEN since secure\n",
				 tag, thread);
		} else {
			CMDQ_LOG("[%s]Thread %d PC(VA): Not available\n",
				tag, thread);
		}
	}

	return pcVA;
}

static void cmdq_core_dump_status(const char *tag)
{
	int32_t coreExecThread = CMDQ_INVALID_THREAD;
	uint32_t value[6] = { 0 };

	value[0] = CMDQ_REG_GET32(CMDQ_CURR_LOADED_THR);
	value[1] = CMDQ_REG_GET32(CMDQ_THR_EXEC_CYCLES);
	value[2] = CMDQ_REG_GET32(CMDQ_THR_TIMEOUT_TIMER);
	value[3] = CMDQ_REG_GET32(CMDQ_BUS_CONTROL_TYPE);

	/* this returns (1 + index of least bit set) or 0 if input is 0. */
	coreExecThread = __builtin_ffs(value[0]) - 1;
	CMDQ_LOG("[%s]IRQ flag:0x%08x, Execing:%d, Exec Thread:%d\n",
		 tag,
		 CMDQ_REG_GET32(CMDQ_CURR_IRQ_STATUS),
		 (0x80000000 & value[0]) ? 1 : 0, coreExecThread);
	CMDQ_LOG("[%s]IRQ flag:CMDQ_CURR_LOADED_THR: 0x%08x\n",
		tag, value[0]);

	CMDQ_LOG("[%s]CMDQ_THR_EXEC_CYCLES:0x%08x, CMDQ_THR_TIMER:0x%08x\n",
		 tag, value[1], value[2]);
	CMDQ_LOG("[%s]CMDQ_BUS_CTRL:0x%08x\n",
		 tag, value[3]);

	CMDQ_LOG("[%s]CMDQ_DEBUG_1: 0x%08x\n", tag,
		CMDQ_REG_GET32((GCE_BASE_VA + 0xF0)));
	CMDQ_LOG("[%s]CMDQ_DEBUG_2: 0x%08x\n", tag,
		CMDQ_REG_GET32((GCE_BASE_VA + 0xF4)));
	CMDQ_LOG("[%s]CMDQ_DEBUG_3: 0x%08x\n", tag,
		CMDQ_REG_GET32((GCE_BASE_VA + 0xF8)));
	CMDQ_LOG("[%s]CMDQ_DEBUG_4: 0x%08x\n", tag,
		CMDQ_REG_GET32((GCE_BASE_VA + 0xFC)));
}

void cmdq_core_dump_disp_trigger_loop(const char *tag)
{
	/* we assume the first non-high-priority */
	/* thread is trigger loop thread. */
	/* since it will start very early */
	if (gCmdqContext.thread[CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT].taskCount
	    && gCmdqContext.thread[
		CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT].pCurTask[1]
		&& gCmdqContext.thread[
		CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT].loopCallback) {

		uint32_t regValue = 0;
		struct TaskStruct *pTask =
		    gCmdqContext.thread[
			CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT].pCurTask[1];

		cmdq_core_dump_pc(pTask,
			CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT, tag);

		regValue = cmdqCoreGetEvent(CMDQ_EVENT_DISP_RDMA0_EOF);
		CMDQ_LOG("[%s]CMDQ_SYNC_TOKEN_VAL of %s is %d\n",
			 tag,
			 cmdq_core_get_event_name_ENUM(
				CMDQ_EVENT_DISP_RDMA0_EOF), regValue);
	}
}

void cmdq_core_dump_disp_trigger_loop_mini(const char *tag)
{
	/* we assume the first non-high-priority */
	/* thread is trigger loop thread. */
	/* since it will start very early */
	if (gCmdqContext.thread[CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT].taskCount
	    && gCmdqContext.thread[
		CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT].pCurTask[1]
	    && gCmdqContext.thread[
		CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT].loopCallback) {

		struct TaskStruct *pTask =
		    gCmdqContext.thread[
			CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT].pCurTask[1];

		cmdq_core_dump_pc(pTask, CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT,
			tag);
	}
}

static void cmdq_core_dump_thread_pc(const int32_t thread)
{
	int32_t i;
	struct ThreadStruct *pThread;
	struct TaskStruct *pTask;
	uint32_t *pcVA;
	uint32_t insts[4] = { 0 };
	char parsedInstruction[128] = { 0 };

	if (thread == CMDQ_INVALID_THREAD)
		return;

	pThread = &(gCmdqContext.thread[thread]);
	pcVA = NULL;

	for (i = 0; i < cmdq_core_max_task_in_thread(thread); i++) {
		pTask = pThread->pCurTask[i];

		if (pTask == NULL)
			continue;

		pcVA = cmdq_core_get_pc(pTask, thread, insts);
		if (pcVA) {
			const uint32_t op = (insts[3] & 0xFF000000) >> 24;

			cmdq_core_parse_instruction(pcVA, parsedInstruction,
						    sizeof(parsedInstruction));

			/* for wait event case, dump token value */
			/* for WFE, we specifically dump the event value */
			if (op == CMDQ_CODE_WFE) {
				uint32_t regValue = 0;
				const uint32_t eventID = 0x3FF & insts[3];

				CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_ID, eventID);
				regValue = CMDQ_REG_GET32(CMDQ_SYNC_TOKEN_VAL);
				CMDQ_LOG
				("[INFO]task:%p(ID:%d), Thread %d\n)",
				pTask, i, thread);
				CMDQ_LOG
				("PC(VA):0x%p,0x%08x:0x%08x =>%s,value:(%d)\n",
				pcVA, insts[2], insts[3],
				parsedInstruction, regValue);
			} else {
				CMDQ_LOG
				("[INFO]task:%p(ID:%d), Thread %d\n",
				pTask, i, thread);
				CMDQ_LOG
				("[INFO]PC(VA): 0x%p, 0x%08x:0x%08x => %s\n",
				pcVA, insts[2],
				insts[3], parsedInstruction);
			}
			break;
		}
	}
}

static void cmdq_core_dump_task_in_thread(const int32_t thread,
					  const bool fullTatskDump,
					  const bool dumpCookie,
					  const bool dumpCmd)
{
	struct ThreadStruct *pThread;
	struct TaskStruct *pDumpTask;
	int32_t index;
	uint32_t value[4] = { 0 };
	uint32_t cookie;
	uint32_t *pVABase = NULL;
	dma_addr_t MVABase = 0;

	if (thread == CMDQ_INVALID_THREAD)
		return;

	pThread = &(gCmdqContext.thread[thread]);
	pDumpTask = NULL;

	CMDQ_ERR("====== [CMDQ] All Task in Error Thread %d =======\n",
		thread);
	cookie = cmdq_core_thread_exec_counter(thread);
	if (dumpCookie) {
		CMDQ_ERR("Curr Cookie: %d, Wait Cookie: %d, Next Cookie: %d\n",
			cookie, pThread->waitCookie, pThread->nextCookie);
		CMDQ_ERR("Task Count %d, engineFlag: 0x%llx\n",
			pThread->taskCount,
			pThread->engineFlag);
	}

	for (index = 0; index < cmdq_core_max_task_in_thread(thread); index++) {
		pDumpTask = pThread->pCurTask[index];
		if (pDumpTask == NULL)
			continue;

		/* full task dump */
		if (fullTatskDump) {
			CMDQ_ERR("Slot %d, Task: 0x%p\n", index, pDumpTask);
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
		CMDQ_ERR("Slot %d, Task: 0x%p, VABase: 0x%p\n",
				   index, (pDumpTask), (pVABase));
		CMDQ_ERR("MVABase: 0x%p, Size: %d\n",
		   &(MVABase), pDumpTask->commandSize);
		CMDQ_ERR("Last Inst 0x%08x:0x%08x,0x%08x:0x%08x,prio: %d\n",
		   value[0], value[1], value[2], value[3],
		   pDumpTask->priority);

		if (dumpCmd == true) {
			print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 16, 4,
				pVABase, (pDumpTask->commandSize), true);
		}
	}
}

static void cmdq_core_dump_task_with_engine_flag(
	uint64_t engineFlag, s32 current_thread)
{
	s32 thread_idx = 0;

	if (current_thread == CMDQ_INVALID_THREAD)
		return;

	CMDQ_ERR(
		"[CMDQ] All task in thr sharing same engine flag 0x%016llx\n",
		engineFlag);

	for (thread_idx = 0; thread_idx < CMDQ_MAX_THREAD_COUNT; thread_idx++) {
		struct ThreadStruct *thread = &gCmdqContext.thread[thread_idx];

		if (thread_idx == current_thread || thread->taskCount <= 0 ||
			!(thread->engineFlag & engineFlag))
			continue;
		cmdq_core_dump_task_in_thread(thread_idx, false, false, false);
	}
}

void cmdq_core_dump_secure_metadata(
	struct cmdqSecDataStruct *pSecData)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	uint32_t i = 0;
	struct cmdqSecAddrMetadataStruct *pAddr = NULL;

	if (pSecData == NULL)
		return;

	pAddr = (struct cmdqSecAddrMetadataStruct *)
		(CMDQ_U32_PTR(pSecData->addrMetadatas));

	CMDQ_LOG("========= pSecData: %p dump =========\n", pSecData);
	CMDQ_LOG("count:%d(%d), enginesNeedDAPC:0x%llx\n",
		 pSecData->addrMetadataCount, pSecData->addrMetadataMaxCount,
		 pSecData->enginesNeedDAPC);
	CMDQ_LOG("enginesPortSecurity:0x%llx\n",
		pSecData->enginesNeedPortSecurity);

	if (pAddr == NULL)
		return;

	for (i = 0; i < pSecData->addrMetadataCount; i++) {
		CMDQ_LOG("idx:%d, type:%d, baseHandle:0x%016llx\n",
			 i, pAddr[i].type, (u64)pAddr[i].baseHandle);
		CMDQ_LOG("blockOffset:%u, offset:%u, size:%u, port:%u\n",
			 pAddr[i].blockOffset, pAddr[i].offset,
			 pAddr[i].size, pAddr[i].port);
	}
#endif
}

int32_t cmdq_core_interpret_instruction(char *textBuf, int bufLen,
					const uint32_t op, const uint32_t arg_a,
					const uint32_t arg_b)
{
	int reqLen = 0;
	uint32_t arg_addr, arg_value;
	uint32_t arg_addr_type, arg_value_type;
	uint32_t reg_addr;
	uint32_t reg_id, use_mask;
	const uint32_t addr_mask = 0xFFFFFFFE;

	switch (op) {
	case CMDQ_CODE_MOVE:
		if (1 & (arg_a >> 23)) {
			reg_id = ((arg_a >> 16) & 0x1f);
			reqLen =
			    snprintf(textBuf, bufLen,
				"MOVE: 0x%08x to Reg%d\n", arg_b, reg_id);
		} else {
			reqLen = snprintf(textBuf, bufLen,
				"Set MASK: 0x%08x\n", arg_b);
		}
		break;
	case CMDQ_CODE_READ:
	case CMDQ_CODE_WRITE:
	case CMDQ_CODE_POLL:
		reqLen = snprintf(textBuf, bufLen, "%s: ",
			cmdq_core_parse_op(op));
		bufLen -= reqLen;
		textBuf += reqLen;

		arg_addr = arg_a;
		arg_addr_type = arg_a & (1 << 23);
		arg_value = arg_b;
		arg_value_type = arg_a & (1 << 22);

		/* data (value) */
		if (arg_value_type != 0) {
			reg_id = arg_value;
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
			reg_id = (arg_addr >> 16) & 0x1F;
			reqLen = snprintf(textBuf, bufLen, "Reg%d, ", reg_id);
			bufLen -= reqLen;
			textBuf += reqLen;
		} else {
			reg_addr = cmdq_core_subsys_to_reg_addr(arg_addr);

			reqLen = snprintf(textBuf, bufLen, "addr=0x%08x [%s], ",
				(reg_addr & addr_mask),
				cmdq_get_func()->parseModule(reg_addr));
			bufLen -= reqLen;
			textBuf += reqLen;
		}

		use_mask = (arg_addr & 0x1);
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
				reqLen =
				    snprintf(textBuf, bufLen,
					"JUMP(absolute): 0x%08x\n", arg_b);
			}
		} else {
			/* relative */
			if ((int32_t) arg_b >= 0) {
				reqLen = snprintf(textBuf, bufLen,
						  "JUMP(relative): +%d\n",
						  (int32_t) arg_b);
			} else {
				reqLen = snprintf(textBuf, bufLen,
						  "JUMP(relative): %d\n",
						  (int32_t) arg_b);
			}
		}
		break;
	case CMDQ_CODE_WFE:
		if (arg_b == 0x80008001) {
			reqLen =
			    snprintf(textBuf, bufLen,
				"Wait And Clear Event: %s\n",
				cmdq_core_get_event_name(arg_a));
		} else if (arg_b == 0x80000000) {
			reqLen =
			    snprintf(textBuf, bufLen, "Clear Event: %s\n",
				     cmdq_core_get_event_name(arg_a));
		} else if (arg_b == 0x80010000) {
			reqLen =
			    snprintf(textBuf, bufLen, "Set Event: %s\n",
				     cmdq_core_get_event_name(arg_a));
		} else if (arg_b == 0x00008001) {
			reqLen =
			    snprintf(textBuf, bufLen,
				"Wait No Clear Event: %s\n",
				cmdq_core_get_event_name(arg_a));
		} else {
			reqLen = snprintf(textBuf, bufLen,
					  "SYNC: %s, upd=%d, op=%d, val=%d, wait=%d, wop=%d, val=%d\n",
					  cmdq_core_get_event_name(arg_a),
					  (arg_b >> 31) & 0x1,
					  (arg_b >> 28) & 0x7,
					  (arg_b >> 16) & 0xFFF,
					  (arg_b >> 15) & 0x1,
					  (arg_b >> 12) & 0x7,
					  (arg_b >> 0) & 0xFFF);
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
	default:
		reqLen = snprintf(textBuf, bufLen, "UNDEFINED\n");
		break;
	}

	return reqLen;
}

int32_t cmdq_core_parse_instruction(const uint32_t *pCmd, char *textBuf,
	int bufLen)
{
	int reqLen = 0;

	const uint32_t op = (pCmd[1] & 0xFF000000) >> 24;
	const uint32_t arg_a = pCmd[1] & (~0xFF000000);
	const uint32_t arg_b = pCmd[0];

	reqLen = cmdq_core_interpret_instruction(textBuf, bufLen, op,
		arg_a, arg_b);

	return reqLen;
}

void cmdq_core_dump_error_instruction(const uint32_t *pcVA,
	const long pcPA,
	uint32_t *insts, int thread, uint32_t lineNum)
{
	char parsedInstruction[128] = { 0 };
	const uint32_t op = (insts[3] & 0xFF000000) >> 24;

	if (CMDQ_IS_END_ADDR(pcPA)) {
		/* in end address case instruction may not correct */
		CMDQ_ERR("PC stay at GCE end address, line: %u\n", lineNum);
		return;
	} else if (pcVA == NULL) {
		CMDQ_ERR("Dump error instruction with null va, line: %u\n",
			lineNum);
		return;
	}

	cmdq_core_parse_instruction(pcVA, parsedInstruction,
		sizeof(parsedInstruction));
	CMDQ_ERR("Thread %d error instruction: 0x%p, 0x%08x:0x%08x => %s",
		 thread, pcVA, insts[2], insts[3], parsedInstruction);

	/* for WFE, we specifically dump the event value */
	if (op == CMDQ_CODE_WFE) {
		uint32_t regValue = 0;
		const uint32_t eventID = 0x3FF & insts[3];

		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_ID, eventID);
		regValue = CMDQ_REG_GET32(CMDQ_SYNC_TOKEN_VAL);
		CMDQ_ERR("CMDQ_SYNC_TOKEN_VAL of %s is %d\n",
			 cmdq_core_get_event_name(eventID), regValue);
	}
}

static void cmdq_core_dump_summary(const struct TaskStruct *pTask,
	int thread,
	const struct TaskStruct **pOutNGTask)
{
	uint32_t *pcVA = NULL;
	uint32_t insts[4] = { 0 };
	struct ThreadStruct *pThread;
	const struct TaskStruct *pNGTask = NULL;
	const char *module = NULL;
	int32_t index;
	uint32_t instA = 0, instB = 0;
	int32_t irqFlag = 0;
	long currPC = 0;

	if (pTask == NULL) {
		CMDQ_ERR("dump summary failed since pTask is NULL");
		return;
	}

	if ((list_empty(&pTask->cmd_buffer_list)) ||
		(thread == CMDQ_INVALID_THREAD)) {
		CMDQ_ERR("invalid param, pTask: %p, thread: %d\n",
			pTask, thread);
		return;
	}

	if (pTask->secData.is_secure == true) {
#if defined(CMDQ_SECURE_PATH_SUPPORT)
		if (pTask->secStatus) {
			/* secure status may contains debug information */
			CMDQ_ERR("Secure status: %d step: 0x%08x\n",
				pTask->secStatus->status,
				pTask->secStatus->step);
			CMDQ_ERR("arg:0x%08x 0x%08x 0x%08x 0x%08x task:0x%p\n",
				pTask->secStatus->args[0],
				pTask->secStatus->args[1],
				pTask->secStatus->args[2],
				pTask->secStatus->args[3],
				pTask);
			for (index = 0; index < pTask->secStatus->inst_index;
				index += 2) {
				CMDQ_ERR("Secure instruction %d: 0x%08x:%08x\n",
					(index / 2),
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
	currPC = CMDQ_AREG_TO_PHYS(CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread)));
	if (CMDQ_IS_END_ADDR(currPC) == false) {
		/* Find correct task */
		pThread = &(gCmdqContext.thread[thread]);
		pcVA = cmdq_core_get_pc(pTask, thread, insts);
		if (pcVA == NULL) {
			/* Find all task to get correct PC */
			for (index = 0;
				index < cmdq_core_max_task_in_thread(thread);
				index++) {
				pNGTask = pThread->pCurTask[index];
				if (pNGTask == NULL)
					continue;

				pcVA = cmdq_core_get_pc(pNGTask, thread, insts);
				if (pcVA) {
					/* we got NG task ! */
					break;
				}
			}
		}
	}
	if (pNGTask == NULL)
		pNGTask = pTask;

	/* Do summary ! */
	CMDQ_ERR("***************************************\n");
	cmdq_core_parse_error(pNGTask, thread, &module, &irqFlag,
		&instA, &instB);
	CMDQ_ERR("** [Module] %s **\n", module);
	if (pTask != pNGTask) {
		CMDQ_ERR("** [Note] PC is not in first error task (0x%p)\n",
			pTask);
		CMDQ_ERR("but in previous task (0x%p) **\n",
			pNGTask);
	}
	CMDQ_ERR("[Error Info] Refer to instr and engine dump for debug**\n");
	cmdq_core_dump_error_instruction(pcVA, currPC, insts, thread, __LINE__);
	cmdq_core_dump_disp_trigger_loop("ERR");
	CMDQ_ERR("***************************************\n");

	*pOutNGTask = pNGTask;
}

void cmdqCoreDumpCommandMem(const u32 *pCmd, s32 commandSize)
{
	static char textBuf[128] = { 0 };
	int i;

	mutex_lock(&gCmdqTaskMutex);

	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 16, 4, pCmd,
		commandSize, false);
	CMDQ_LOG("======TASK command buffer END\n");

	for (i = 0; i < commandSize; i += CMDQ_INST_SIZE, pCmd += 2) {
		cmdq_core_parse_instruction(pCmd, textBuf, 128);
		CMDQ_LOG("%s", textBuf);
	}
	CMDQ_LOG("TASK command buffer TRANSLATED END\n");

	mutex_unlock(&gCmdqTaskMutex);
}

int32_t cmdqCoreDebugDumpCommand(struct TaskStruct *pTask)
{
	struct CmdBufferStruct *cmd_buffer = NULL;

	if (pTask == NULL)
		return -EFAULT;

	CMDQ_LOG("======TASK 0x%p , size (%d) command START\n", pTask,
		pTask->commandSize);
	if (cmdq_core_task_is_buffer_size_valid(pTask) == false) {
		CMDQ_ERR("Buffer size: %u, available size: %u of %u\n",
			pTask->bufferSize, pTask->buf_available_size,
			(uint32_t)CMDQ_CMD_BUFFER_SIZE);
		CMDQ_ERR("end cmd: 0x%p first va: 0x%p out of sync!\n",
			pTask->pCMDEnd, cmdq_core_task_get_first_va(pTask));
	}

	list_for_each_entry(cmd_buffer, &pTask->cmd_buffer_list, listEntry) {
		if (list_is_last(&cmd_buffer->listEntry,
			&pTask->cmd_buffer_list)) {
			cmdqCoreDumpCommandMem(cmd_buffer->pVABase,
				CMDQ_CMD_BUFFER_SIZE -
					pTask->buf_available_size);
		} else {
			cmdqCoreDumpCommandMem(cmd_buffer->pVABase,
				CMDQ_CMD_BUFFER_SIZE);
		}
	}
	CMDQ_LOG("======TASK 0x%p command END\n", pTask);
	return 0;
}

void cmdq_core_set_command_buffer_dump(int32_t scenario,
	int32_t bufferSize)
{
	mutex_lock(&gCmdqSaveBufferMutex);

	if (bufferSize != gCmdqBufferDump.bufferSize && bufferSize != -1) {
		if (gCmdqBufferDump.bufferSize != 0) {
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

	CMDQ_LOG("[SET DUMP]CONFIG: bufferSize: %d, scenario: 0x%08llx\n",
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
		gCmdqBufferDump.count += vsnprintf(pBuffer,
			logLen, string, argptr);
	} while (0);

	va_end(argptr);
}

static void cmdq_core_save_command_buffer_dump(
	const struct TaskStruct *pTask)
{
	static char textBuf[128] = { 0 };
	struct timeval savetv;
	struct tm nowTM;
	unsigned long long saveTimeSec;
	unsigned long rem_nsec;
	const uint32_t *pCmd;
	int i;
	struct CmdBufferStruct *cmd_buffer = NULL;
	uint32_t cmd_size = 0;

	if (gCmdqContext.errNum > 0)
		return;
	if (gCmdqBufferDump.bufferSize <= 0 ||
		list_empty(&pTask->cmd_buffer_list))
		return;

	mutex_lock(&gCmdqSaveBufferMutex);

	if (gCmdqBufferDump.scenario & (1LL << pTask->scenario)) {
		cmdq_core_save_buffer("***TASK command buffer TRANSLATED***\n");
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
			(nowTM.tm_year + 1900), (nowTM.tm_mon + 1),
			nowTM.tm_mday,
			nowTM.tm_hour, nowTM.tm_min, nowTM.tm_sec,
			savetv.tv_usec);
		cmdq_core_save_buffer(" Pid: %d, Name: %s\n",
			pTask->callerPid, pTask->callerName);
		cmdq_core_save_buffer(
			"Task: 0x%p, Scenario: %d, Size: %d, Flag:0x%016llx\n",
			pTask, pTask->scenario,
			pTask->commandSize, pTask->engineFlag);
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
				cmdq_core_parse_instruction(pCmd, textBuf, 128);
				cmdq_core_save_buffer("[%5llu.%06lu] %s",
					saveTimeSec, rem_nsec / 1000, textBuf);
			}
		}
		cmdq_core_save_buffer("*****TASK command buffer END*****\n\n");
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

ssize_t instruction_count_level_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	int len = 0;

	if (buf)
		len = sprintf(buf, "%d\n", gCmdqContext.instructionCountLevel);

	return len;
}

ssize_t instruction_count_level_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf,
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

void cmdq_core_set_instruction_count_level(const int32_t value)
{
	gCmdqContext.instructionCountLevel = value;
}

static void cmdq_core_fill_module_stat(const uint32_t *pCommand,
				       unsigned short *pModuleCount,
				       uint32_t *pOtherInstruction,
				       uint32_t *pOtherInstructionCount)
{

	const uint32_t arg_a = pCommand[1] & (~0xFF000000);
	const uint32_t addr = cmdq_core_subsys_to_reg_addr(arg_a);
	int32_t i;

	for (i = 0; i < CMDQ_MODULE_STAT_GPR; i++) {
		if ((gCmdqModulePAStat.start[i] > 0) &&
			(addr >= gCmdqModulePAStat.start[i])
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

static void cmdq_core_fill_module_event_count(const uint32_t *pCommand,
					      unsigned short *pEventCount)
{

	const uint32_t arg_a = pCommand[1] & (~0xFF000000);

	if (arg_a >= CMDQ_MAX_HW_EVENT_COUNT)
		pEventCount[CMDQ_EVENT_STAT_SW]++;
	else
		pEventCount[CMDQ_EVENT_STAT_HW]++;
}

static void cmdq_core_fill_task_instruction_stat(
	struct RecordStruct *pRecord, const struct TaskStruct *pTask)
{
	bool invalidinstruction = false;
	int32_t commandIndex = 0;
	uint32_t arg_a_prefetch_en, arg_b_prefetch_en;
	uint32_t arg_a_prefetch_dis, arg_b_prefetch_dis;
	uint32_t *pCommand;
	uint32_t op;
	struct list_head *p = NULL;
	struct CmdBufferStruct *cmd_buffer = NULL;
	uint32_t buf_size = 0;

	if (gCmdqContext.instructionCountLevel < 1)
		return;

	if ((pRecord == NULL) || (pTask == NULL))
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
		pCommand = (uint32_t *)
			((uint8_t *) cmd_buffer->pVABase + commandIndex);
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
				pCommand = (uint32_t *) ((uint8_t *)
					cmd_buffer->pVABase + commandIndex);
				pCommand = (uint32_t *) ((uint8_t *)
					pTask->pVABase + commandIndex);
				op = (pCommand[1] & 0xFF000000) >> 24;

				if (op == CMDQ_CODE_WRITE) {
					pRecord->instructionStat[
						CMDQ_STAT_WRITE_W_MASK]++;
					cmdq_core_fill_module_stat(pCommand,
						pRecord->writewmaskModule,
						pRecord->otherInstr,
						&(pRecord->otherInstrNUM));
				} else if (op == CMDQ_CODE_POLL) {
					pRecord->instructionStat[
						CMDQ_STAT_POLLING]++;
					cmdq_core_fill_module_stat(pCommand,
						pRecord->pollModule,
						pRecord->otherInstr,
						&(pRecord->otherInstrNUM));
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
				&(pRecord->otherInstrNUM));
			break;
		case CMDQ_CODE_WRITE:
			pRecord->instructionStat[CMDQ_STAT_WRITE]++;
			cmdq_core_fill_module_stat(pCommand,
				pRecord->writeModule,
				pRecord->otherInstr,
				&(pRecord->otherInstrNUM));
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
			arg_b_prefetch_en = ((1 << 20) | (1 << 17) | (1 << 16));
			arg_a_prefetch_en =
			    (CMDQ_CODE_EOC << 24) | (0x1 << (53 - 32)) |
				(0x1 << (48 - 32));
			arg_b_prefetch_dis = (1 << 20);
			arg_a_prefetch_dis = (CMDQ_CODE_EOC << 24) |
				(0x1 << (48 - 32));

			if ((arg_b_prefetch_en == pCommand[0]) &&
				(arg_a_prefetch_en == pCommand[1])) {
				pRecord->instructionStat[
					CMDQ_STAT_PREFETCH_EN]++;
			} else if ((arg_b_prefetch_dis == pCommand[0])
				   && (arg_a_prefetch_dis == pCommand[1])) {
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
	int32_t i;
	int32_t index;
	int32_t numRec;
	struct RecordStruct record;

	if (gCmdqContext.instructionCountLevel < 1)
		return 0;

	seq_puts(m,
		"Record ID, PID, scenario, total, write, write_w_mask, read,");
	seq_puts(m,
		" polling, move, sync, prefetch_en, prefetch_dis, EOC, jump");
	for (i = 0; i < CMDQ_MODULE_STAT_MAX; i++) {
		seq_printf(m, ", (%s)=>, write, write_w_mask, read, polling",
			   gCmdqModuleInstructionLabel[i]);
	}
	seq_puts(m, ", (SYNC)=>, HW Event, SW Event\n");
	/* we try to minimize time spent in spin lock */
	/* since record is an array so it is okay to */
	/* allow displaying an out-of-date entry. */
	spin_lock_irqsave(&gCmdqRecordLock, flags);
	numRec = gCmdqContext.recNum;
	index = gCmdqContext.lastID - 1;
	spin_unlock_irqrestore(&gCmdqRecordLock, flags);

	/* we print record in reverse order. */
	for (; numRec > 0; --numRec, --index) {
		if (index >= CMDQ_MAX_RECORD_COUNT)
			index = 0;
		else if (index < 0)
			index = CMDQ_MAX_RECORD_COUNT - 1;

		/* Make sure we don't print a record that is during updating. */
		/* However, this record may already be different */
		/* from the time of entering cmdqCorePrintRecordSeq(). */
		spin_lock_irqsave(&gCmdqRecordLock, flags);
		record = gCmdqContext.record[index];
		spin_unlock_irqrestore(&gCmdqRecordLock, flags);

		if ((record.instructionStat[CMDQ_STAT_EOC] == 0) &&
		    (record.instructionStat[CMDQ_STAT_JUMP] == 0)) {
			seq_printf(m, "%4d, %5c, %2c, %4d", index, 'X', 'X', 0);
			for (i = 0; i < CMDQ_STAT_MAX; i++)
				seq_printf(m, ", %4d", 0);

			for (i = 0; i < CMDQ_MODULE_STAT_MAX; i++)
				seq_printf(m, ", , %4d, %4d, %4d, %4d",
					0, 0, 0, 0);

			seq_printf(m, ", , %4d, %4d", 0, 0);
		} else {
			uint32_t totalCount = (uint32_t)
				(record.size / CMDQ_INST_SIZE);

			seq_printf(m, " %4d, %5d, %02d, %4d",
				index, record.user, record.scenario,
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
	/* we try to minimize time spent in spin lock */
	/* since record is an array so it is okay to */
	/* allow displaying an out-of-date entry. */
	spin_lock_irqsave(&gCmdqRecordLock, flags);
	numRec = gCmdqContext.recNum;
	index = gCmdqContext.lastID - 1;
	spin_unlock_irqrestore(&gCmdqRecordLock, flags);

	/* we print record in reverse order. */
	for (; numRec > 0; --numRec, --index) {
		if (index >= CMDQ_MAX_RECORD_COUNT)
			index = 0;
		else if (index < 0)
			index = CMDQ_MAX_RECORD_COUNT - 1;

		/* Make sure we don't print a record that is during updating. */
		/* However, this record may already be different */
		/* from the time of entering cmdqCorePrintRecordSeq(). */
		spin_lock_irqsave(&gCmdqRecordLock, flags);
		record = gCmdqContext.record[index];
		spin_unlock_irqrestore(&gCmdqRecordLock, flags);

		for (i = 0; i < record.otherInstrNUM; i++)
			seq_printf(m, "0x%08x\n", record.otherInstr[i]);
	}

	return 0;
}
#endif

static void cmdq_core_fill_task_profile_marker_record(
	struct RecordStruct *pRecord,
	const struct TaskStruct *pTask)
{
#ifdef CMDQ_PROFILE_MARKER_SUPPORT
	uint32_t i;
	uint32_t profileMarkerCount;
	uint32_t value;
	cmdqBackupSlotHandle hSlot;

	if ((pRecord == NULL) || (pTask == NULL))
		return;

	if (pTask->profileMarker.hSlot == 0)
		return;

	profileMarkerCount = pTask->profileMarker.count;
	hSlot = (cmdqBackupSlotHandle) (pTask->profileMarker.hSlot);

	pRecord->profileMarkerCount = profileMarkerCount;
	for (i = 0; i < profileMarkerCount; i++) {
		/* timestamp, each count is 76ns */
		cmdq_cpu_read_mem(hSlot, i, &value);
		pRecord->profileMarkerTimeNS[i] = value * 76;
		pRecord->profileMarkerTag[i] =
			(char *)(CMDQ_U32_PTR(pTask->profileMarker.tag[i]));
	}
#endif
}

static void cmdq_core_fill_task_record(struct RecordStruct *pRecord,
	const struct TaskStruct *pTask,
	uint32_t thread)
{
	uint32_t begin, end;

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

		if (pTask->profileData == NULL) {
			pRecord->writeTimeNS = 0;
			pRecord->writeTimeNSBegin = 0;
			pRecord->writeTimeNSEnd = 0;
		} else {
			/* Command exec time, each count is 76ns */
			begin = *((uint32_t *)pTask->profileData);
			end = *((uint32_t *)(pTask->profileData + 1));
			pRecord->writeTimeNS = (end - begin) * 76;

			pRecord->writeTimeNSBegin = (begin) * 76;
			pRecord->writeTimeNSEnd = (end) * 76;
		}

		/* Record time */
		pRecord->submit = pTask->submit;
		pRecord->trigger = pTask->trigger;
		pRecord->gotIRQ = pTask->gotIRQ;
		pRecord->beginWait = pTask->beginWait;
		pRecord->wakedUp = pTask->wakedUp;
		pRecord->durAlloc = pTask->durAlloc;
		pRecord->durReclaim = pTask->durReclaim;
		pRecord->durRelease = pTask->durRelease;

		cmdq_core_fill_task_profile_marker_record(pRecord, pTask);
#ifdef CMDQ_INSTRUCTION_COUNT
		/* Instruction count statistics */
		cmdq_core_fill_task_instruction_stat(pRecord, pTask);
#endif
	}
}

static void cmdq_core_track_task_record(struct TaskStruct *pTask,
	uint32_t thread)
{
	struct RecordStruct *pRecord;
	unsigned long flags;
	CMDQ_TIME done;
	int lastID;
	char buf[256];
	int length;


	done = sched_clock();

	spin_lock_irqsave(&gCmdqRecordLock, flags);

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

	spin_unlock_irqrestore(&gCmdqRecordLock, flags);

	if (pTask->dumpAllocTime) {
		length = cmdq_core_print_record(pRecord, lastID, buf,
			ARRAY_SIZE(buf));
		CMDQ_LOG("Record: %s", buf);
	}
}

void cmdq_core_dump_GIC(void)
{
/* OF Support removes mt_irq.h, */
/* mt_irq_dump_status support will be added later. */
#ifndef CMDQ_OF_SUPPORT
#if CMDQ_DUMP_GIC
	mt_irq_dump_status(cmdq_dev_get_irq_id());
	mt_irq_dump_status(cmdq_dev_get_irq_secure_id());
#endif
#endif
}

static void cmdq_core_dump_error_buffer(const struct TaskStruct *pTask,
	uint32_t *hwPC)
{
	struct CmdBufferStruct *cmd_buffer = NULL;
	u32 cmd_size = 0, dump_size = 0;
	bool dump = false;

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
				/* because hwPC points to */
				/* "start" of the instruction, add offset 1 */
				dump_size = (u32)(2 + hwPC -
					cmd_buffer->pVABase) * sizeof(u32);
				dump = true;
			} else {
				dump_size = cmd_size;
			}

			print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 16, 4,
				cmd_buffer->pVABase, dump_size, true);
			cmdq_core_save_hex_first_dump("", 16, 4,
				cmd_buffer->pVABase, dump_size);

			if (dump)
				break;
		}
	}

	if (!dump) {
		CMDQ_ERR("hwPC is not in region, dump all\n");
		list_for_each_entry(cmd_buffer, &pTask->cmd_buffer_list,
			listEntry) {
			if (list_is_last(&cmd_buffer->listEntry,
				&pTask->cmd_buffer_list))
				cmd_size = CMDQ_CMD_BUFFER_SIZE -
					pTask->buf_available_size;
			else
				cmd_size = CMDQ_CMD_BUFFER_SIZE;
			print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 16, 4,
				cmd_buffer->pVABase, (cmd_size), true);
			cmdq_core_save_hex_first_dump("", 16, 4,
				cmd_buffer->pVABase, (cmd_size));
		}
	}
}

void cmdq_core_dump_thread(uint32_t thread, const char *tag)
{
	struct ThreadStruct *pThread;
	uint32_t value[15] = { 0 };

	pThread = &(gCmdqContext.thread[thread]);
	if (pThread->taskCount == 0)
		return;

	CMDQ_LOG("[%s]=============== [CMDQ] Error Thread Status ==========\n",
		tag);
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

	CMDQ_LOG("[%s]Index: %d, Enabled: %d, IRQ: 0x%08x\n",
		tag, thread, value[8], value[4]);
	CMDQ_LOG("[%s]Thread PC: 0x%08x, End: 0x%08x, Wait Token: 0x%08x\n",
		tag, value[0], value[1], value[2]);

	CMDQ_LOG("[%s]Curr Cookie: %d, Wait Cookie: %d, Next Cookie: %d\n",
		tag, value[3], pThread->waitCookie, pThread->nextCookie);
	CMDQ_LOG("[%s]Task Count %d, engineFlag: 0x%llx\n",
		tag, pThread->taskCount, pThread->engineFlag);

	CMDQ_LOG("[%s]Timeout Cycle: %d, Status: 0x%08x\n",
		tag, value[5], value[6]);
	CMDQ_LOG("[%s]IRQ_EN: 0x%08x, reset: 0x%08x\n",
		tag, value[7], value[9]);

	CMDQ_LOG("[%s]Suspend task: %d sec: %d cfg: %d\n",
		tag, value[10], value[11], value[12]);
	CMDQ_LOG("[%s]prefetch: %d thresx: %d\n",
		tag, value[13], value[14]);
}

static void cmdq_core_dump_error_task(const struct TaskStruct *pTask,
	const struct TaskStruct *pNGTask, uint32_t thread, bool short_log)
{
	struct CmdqCBkStruct *pCallback = NULL;
	struct ThreadStruct *pThread;
	int32_t index = 0;
	uint32_t *hwPC = NULL;
	uint32_t *hwNGPC = NULL;
	uint64_t printEngineFlag = 0;
	bool isDispScn = false;

	static const char *const engineGroupName[] = {
		CMDQ_FOREACH_GROUP(GENERATE_STRING)
	};

	pThread = &(gCmdqContext.thread[thread]);
	if (cmdq_get_func()->isSecureThread(thread) == false) {
		/* normal thread */
		cmdq_core_dump_thread(thread, "ERR");
	} else {
		/* do nothing since it's a secure thread */
		CMDQ_ERR("Wait Cookie: %d, Next Cookie: %d, Task Count %d,\n",
			 pThread->waitCookie, pThread->nextCookie,
			 pThread->taskCount);
	}

	/* skip internal testcase */
	if (gCmdqContext.errNum > 1 &&
		((pTask && CMDQ_TASK_IS_INTERNAL(pTask)) ||
		(pNGTask && CMDQ_TASK_IS_INTERNAL(pNGTask))))
		return;

	/* Begin is not first, save NG task but print pTask as well */
	if (pNGTask != NULL && pNGTask != pTask) {
		CMDQ_ERR("== [CMDQ] We have NG task ==\n");
		CMDQ_ERR("===== [CMDQ] Error Thread PC (NG Task) =======\n");
		hwNGPC = cmdq_core_dump_pc(pNGTask, thread, "ERR");

		CMDQ_ERR("===== [CMDQ] Error Task Status (NG Task) =======\n");
		cmdq_core_dump_task(pNGTask);

		printEngineFlag |= pNGTask->engineFlag;
	}

	if (pTask != NULL) {
		CMDQ_ERR("=============== [CMDQ] Error Thread PC =======\n");
		hwPC = cmdq_core_dump_pc(pTask, thread, "ERR");

		CMDQ_ERR("=============== [CMDQ] Error Task Status ========\n");
		cmdq_core_dump_task(pTask);

		printEngineFlag |= pTask->engineFlag;
	}

	/* dump tasks in error thread */
	cmdq_core_dump_task_in_thread(thread, false, false, false);
	cmdq_core_dump_task_with_engine_flag(printEngineFlag, thread);

	if (short_log) {
		CMDQ_ERR("=============== skip detail error dump ==========\n");
		return;
	}

	CMDQ_ERR("=============== [CMDQ] CMDQ Status ===============\n");
	cmdq_core_dump_status("ERR");

	if (!pTask || !CMDQ_TASK_IS_INTERNAL(pTask)) {
#ifndef CONFIG_MTK_FPGA
		CMDQ_ERR("=============== [CMDQ] SMI Status ===============\n");
		cmdq_get_func()->dumpSMI(1);
#endif

		CMDQ_ERR("====== [CMDQ] Clock Gating Status ============\n");
		CMDQ_ERR("[CLOCK] common clock ref=%d\n",
			atomic_read(&gCmdqThreadUsage));
		cmdq_get_func()->dumpClockGating();

		/* Dump MMSYS configuration */
		CMDQ_ERR("=============== [CMDQ] MMSYS_CONFIG ===========\n");
		cmdq_mdp_get_func()->dumpMMSYSConfig();
	}

	/*      */
	/* ask each module to print their status */
	/*      */
	CMDQ_ERR("=============== [CMDQ] Engine Status ===============\n");
	pCallback = gCmdqGroupCallback;
	for (index = 0; index < CMDQ_MAX_GROUP_COUNT; ++index) {
		if (!cmdq_core_is_group_flag((enum CMDQ_GROUP_ENUM) index,
			printEngineFlag))
			continue;

		CMDQ_ERR("====== engine group %s status =======\n",
			engineGroupName[index]);

		if (pCallback[index].dumpInfo == NULL) {
			CMDQ_ERR("(no dump function)\n");
			continue;
		}

		pCallback[index].dumpInfo(
			(gCmdqEngineGroupBits[index] & printEngineFlag),
			gCmdqContext.logLevel);
	}

	/* force dump DISP for DISP scenario with 0x0 engine flag */
	if (pTask != NULL)
		isDispScn = cmdq_get_func()->isDispScenario(pTask->scenario);

	if (pNGTask != NULL)
		isDispScn = isDispScn |
			cmdq_get_func()->isDispScenario(pNGTask->scenario);

	if (isDispScn) {
		index = CMDQ_GROUP_DISP;
		if (pCallback[index].dumpInfo) {
			pCallback[index].dumpInfo(
				(gCmdqEngineGroupBits[index] & printEngineFlag),
				gCmdqContext.logLevel);
		}
	}

	CMDQ_ERR("=============== [CMDQ] GIC dump ===============\n");
	cmdq_core_dump_GIC();

	/* Begin is not first, save NG task but print pTask as well */
	if (pNGTask != NULL && pNGTask != pTask) {
		CMDQ_ERR("=== [CMDQ] Error Command Buffer (NG Task) ======\n");
		cmdq_core_dump_error_buffer(pNGTask, hwNGPC);
	}

	if (pTask != NULL) {
		CMDQ_ERR("========= [CMDQ] Error Command Buffer ======\n");
		cmdq_core_dump_error_buffer(pTask, hwPC);
	}
}

static void cmdq_core_attach_error_task(
	const struct TaskStruct *pTask, int32_t thread,
	const struct TaskStruct **pOutNGTask)
{
	struct EngineStruct *pEngine = NULL;
	struct ThreadStruct *pThread = NULL;
	const struct TaskStruct *pNGTask = NULL;
	uint64_t engFlag = 0;
	int32_t index = 0;
	bool short_log = false;

	if (pTask == NULL) {
		CMDQ_ERR("attach error failed since pTask is NULL");
		return;
	}

	pThread = &(gCmdqContext.thread[thread]);
	pEngine = gCmdqContext.engine;

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->warning,
		MMPROFILE_FLAG_PULSE, ((unsigned long)pTask),
		      thread);

	/*  */
	/* Update engine fail count */
	/*  */
	engFlag = pTask->engineFlag;
	for (index = 0; index < CMDQ_MAX_ENGINE_COUNT; index++) {
		if (engFlag & (1LL << index))
			pEngine[index].failCount++;
	}

	/*  */
	/* register error record */
	/*  */
	if (gCmdqContext.errNum < CMDQ_MAX_ERROR_COUNT) {
		struct ErrorStruct *pError =
			&gCmdqContext.error[gCmdqContext.errNum];

		cmdq_core_fill_task_record(&pError->errorRec, pTask, thread);
		pError->ts_nsec = local_clock();
	}

	/* Turn on first CMDQ error dump */
	cmdq_core_turnon_first_dump(pTask);

	/*  */
	/* Then we just print out info */
	/*  */
	CMDQ_ERR("============== [CMDQ] Begin of Error %d================\n",
		 gCmdqContext.errNum);

#ifdef CONFIG_MTK_CMDQ_TAB
	CMDQ_ERR("GCE clock_on:%d\n", cmdq_dev_gce_clock_is_on());
#endif

	cmdq_core_dump_summary(pTask, thread, &pNGTask);
	short_log = !(gCmdqContext.errNum <= 2 ||
		gCmdqContext.errNum % 16 == 0 ||
		cmdq_core_should_full_error());
	cmdq_core_dump_error_task(pTask, pNGTask, thread, short_log);

	CMDQ_ERR("================= [CMDQ] End of Error %d ==========\n",
		 gCmdqContext.errNum);
	gCmdqContext.errNum++;

	if (pOutNGTask != NULL) {
		if (pNGTask != NULL)
			*pOutNGTask = pNGTask;
		else
			*pOutNGTask = pTask;
	}
}

static int32_t cmdq_core_insert_task_from_thread_array_by_cookie(
	struct TaskStruct *pTask,
	struct ThreadStruct *pThread,
	const int32_t cookie,
	const bool resetHWThread)
{

	if (pTask == NULL || pThread == NULL) {
		CMDQ_ERR("invalid param, pTask[0x%p], pThread[0x%p]\n",
			 pTask, pThread);
		CMDQ_ERR("invalid param, cookie[%d], needReset[%d]\n",
			cookie, resetHWThread);
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
	pThread->pCurTask[cookie % cmdq_core_max_task_in_thread(pTask->thread)]
		= pTask;
	pThread->allowDispatching = 1;

	/* secure path */
	if (pTask->secData.is_secure) {
		pTask->secData.waitCookie = cookie;
		pTask->secData.resetExecCnt = resetHWThread;
	}

	return 0;
}

static int32_t cmdq_core_remove_task_from_array_by_cookie(
	struct ThreadStruct *pThread,
	int32_t index,
	enum TASK_STATE_ENUM newTaskState)
{
	struct TaskStruct *pTask = NULL;

	if ((pThread == NULL) || (index < 0) ||
		(index >= CMDQ_MAX_TASK_IN_THREAD)) {
		CMDQ_ERR("remove task from thread array, invalid param.\n");
		CMDQ_ERR("THR[0x%p], task_slot[%d], newTaskState[%d]\n",
		     pThread, index, newTaskState);
		return -EINVAL;
	}

	pTask = pThread->pCurTask[index];

	if (pTask == NULL) {
		CMDQ_ERR("remove fail, task_slot[%d] on thread[%p] is NULL\n",
			index, pThread);
		return -EINVAL;
	}

	if (cmdq_core_max_task_in_thread(pTask->thread) <= index) {
		CMDQ_ERR("remove task from thread array, invalid index.\n");
		CMDQ_ERR("THR[0x%p], task_slot[%d], newTaskState[%d]\n",
			pThread, index, newTaskState);
		return -EINVAL;
	}

	/* to switch a task to done_status(_ERROR, _KILLED, _DONE) */
	/* is aligned with thread's taskcount change */
	/* check task status to prevent double clean-up thread's taskcount */
	if (pTask->taskState != TASK_STATE_BUSY) {
		CMDQ_ERR("remove task, taskStatus err[%d]. THR[0x%p]\n",
			pTask->taskState, pThread);
		CMDQ_ERR("task_slot[%d], targetTaskStaus[%d]\n",
			index, newTaskState);
		return -EINVAL;
	}

	CMDQ_VERBOSE("remove task, slot[%d], targetStatus: %d\n",
		index, newTaskState);
	pTask->taskState = newTaskState;
	pTask = NULL;
	pThread->pCurTask[index] = NULL;
	pThread->taskCount--;

	if (pThread->taskCount < 0) {
		/* Error status print */
		CMDQ_ERR("taskCount < 0 after %s\n", __func__);
	}

	return 0;
}

static int32_t cmdq_core_remove_task_from_thread_array_when_secure_submit_fail(
	struct ThreadStruct *pThread,
	int32_t index)
{
	struct TaskStruct *pTask = NULL;
	unsigned long flags = 0L;

	if ((pThread == NULL) || (index < 0) ||
		(index >= CMDQ_MAX_TASK_IN_THREAD)) {
		CMDQ_ERR("remove task from thread array, invalid param.\n");
		CMDQ_ERR("THR[0x%p], task_slot[%d]\n",
			pThread, index);
		return -EINVAL;
	}

	pTask = pThread->pCurTask[index];

	if (pTask == NULL) {
		CMDQ_ERR("remove fail, task_slot[%d] on thread[%p] is NULL\n",
			index, pThread);
		return -EINVAL;
	}

	if (cmdq_core_max_task_in_thread(pTask->thread) <= index) {
		CMDQ_ERR("remove task from thread array, invalid index\n");
		CMDQ_ERR("THR[0x%p], task_slot[%d]\n",
			pThread, index);
		return -EINVAL;
	}

	CMDQ_VERBOSE("remove task, slot[%d]\n", index);
	spin_lock_irqsave(&gCmdqExecLock, flags);
	pTask = NULL;
	pThread->pCurTask[index] = NULL;
	pThread->taskCount--;
	pThread->nextCookie--;
	spin_unlock_irqrestore(&gCmdqExecLock, flags);

	if (pThread->taskCount < 0) {
		/* Error status print */
		CMDQ_ERR("taskCount < 0 after %s\n", __func__);
	}

	return 0;
}

static int32_t cmdq_core_force_remove_task_from_thread(
	struct TaskStruct *pTask, uint32_t thread)
{
	int32_t status = 0;
	int32_t cookie = 0;
	int index = 0;
	int loop = 0;
	struct TaskStruct *pExecTask = NULL;
	struct ThreadStruct *pThread = &(gCmdqContext.thread[thread]);
	dma_addr_t pa = 0;

	status = cmdq_core_suspend_HW_thread(thread, __LINE__);

	CMDQ_REG_SET32(CMDQ_THR_INST_CYCLES(thread),
		cmdq_core_get_task_timeout_cycle(pThread));

	/* The cookie of the task currently being processed */
	cookie = CMDQ_GET_COOKIE_CNT(thread) + 1;

	pExecTask = pThread->pCurTask[cookie %
		cmdq_core_max_task_in_thread(thread)];
	if (pExecTask != NULL && (pExecTask == pTask)) {
		/* The task is executed now, set the PC to EOC for bypass */
		pa = cmdq_core_task_get_eoc_pa(pTask);
		CMDQ_MSG("ending task: 0x%p to pa: 0x%pa\n", pTask, &pa);
		CMDQ_REG_SET32(CMDQ_THR_CURR_ADDR(thread),
				   CMDQ_PHYS_TO_AREG(pa));
		cmdq_core_reset_hw_engine(pTask->engineFlag);

		pThread->pCurTask[cookie % cmdq_core_max_task_in_thread(thread)]
			= NULL;
		pTask->taskState = TASK_STATE_KILLED;
	} else {
		loop = pThread->taskCount;
		for (index = (cookie % cmdq_core_max_task_in_thread(thread));
			loop > 0; loop--, index++) {
			bool is_last_end = false;

			if (index >= cmdq_core_max_task_in_thread(thread))
				index = 0;

			pExecTask = pThread->pCurTask[index];
			if (pExecTask == NULL)
				continue;

			is_last_end = (((pExecTask->pCMDEnd[0] >> 24) & 0xff)
				== CMDQ_CODE_JUMP) &&
				(CMDQ_IS_END_ADDR(pExecTask->pCMDEnd[-1]));

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

static void cmdq_core_handle_done_with_cookie_impl(int32_t thread,
		int32_t value, CMDQ_TIME *pGotIRQ,
		const uint32_t cookie)
{
#ifdef CMDQ_MDP_MET_STATUS
	struct TaskStruct *pTask;
#endif
	struct ThreadStruct *pThread;
	int32_t count;
	int32_t inner;
	int32_t maxTaskNUM = cmdq_core_max_task_in_thread(thread);

	pThread = &(gCmdqContext.thread[thread]);

	/* do not print excessive message for looping thread */
	if (pThread->loopCallback == NULL) {
#ifdef CONFIG_MTK_FPGA
		/* ASYNC: debug log */
		/* to prevent block IRQ handler */
		CMDQ_MSG("IRQ: Done, thread: %d, cookie:%d\n", thread, cookie);
#endif
	}

	if (pThread->waitCookie <= cookie) {
		count = cookie - pThread->waitCookie + 1;
	} else if ((cookie+1) % CMDQ_MAX_COOKIE_VALUE == pThread->waitCookie) {
		count = 0;
		CMDQ_MSG("IRQ: duplicated cookie: waitCookie:%d, hwCookie:%d",
			pThread->waitCookie, cookie);
	} else {
		/* Counter wrapped */
		count = (CMDQ_MAX_COOKIE_VALUE - pThread->waitCookie + 1) +
			(cookie + 1);
		CMDQ_ERR("IRQ: cnt wrapped:waitCookie:%d,hwCookie:%d,count=%d",
			pThread->waitCookie, cookie, count);
	}

	for (inner = (pThread->waitCookie % maxTaskNUM);
		count > 0;
		count--, inner++) {
		if (inner >= maxTaskNUM)
			inner = 0;

		if (pThread->pCurTask[inner] != NULL) {
			struct TaskStruct *pTask = pThread->pCurTask[inner];

			pTask->gotIRQ = *pGotIRQ;
			pTask->irqFlag = value;
			cmdq_core_remove_task_from_array_by_cookie(
				pThread,
				inner, TASK_STATE_DONE);
#ifdef CMDQ_MDP_MET_STATUS
			/* MET MMSYS: Thread done */
			if (met_mmsys_event_gce_thread_end)
				met_mmsys_event_gce_thread_end(thread,
					(uintptr_t) pTask, pTask->engineFlag);
#endif
		}
	}

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->CMDQ_IRQ,
		MMPROFILE_FLAG_PULSE, thread, cookie);

	pThread->waitCookie = cookie + 1;
	/* min cookie value is 0 */
	if (pThread->waitCookie > CMDQ_MAX_COOKIE_VALUE)
		pThread->waitCookie -= (CMDQ_MAX_COOKIE_VALUE + 1);
#ifdef CMDQ_MDP_MET_STATUS
	/* MET MMSYS: GCE should trigger next waiting task */
	if ((pThread->taskCount > 0) && met_mmsys_event_gce_thread_begin) {
		count = pThread->nextCookie - pThread->waitCookie;
		for (inner = (pThread->waitCookie % maxTaskNUM); count > 0;
			count--, inner++) {
			if (inner >= maxTaskNUM)
				inner = 0;

			if (pThread->pCurTask[inner] != NULL) {
				pTask = pThread->pCurTask[inner];
				met_mmsys_event_gce_thread_begin(thread,
					(uintptr_t) pTask, pTask->engineFlag,
					(void *)cmdq_core_task_get_first_va(
						pTask),
					pTask->commandSize);
				break;
			}
		}
	}
#endif
	wake_up(&gCmdWaitQueue[thread]);
}
#ifdef CONFIG_MTK_CMDQ_TAB
static void cmdq_core_handle_secure_thread_done_impl(
	const int32_t thread,
	const int32_t value, CMDQ_TIME *pGotIRQ)
{
	uint32_t cookie;

	/* get cookie value from shared memory */
	cookie = cmdq_core_get_secure_thread_exec_counter(thread);
	if (cookie < 0)
		return;
	cmdq_core_handle_done_with_cookie_impl(thread, value, pGotIRQ, cookie);

}

const bool cmdq_core_is_valid_notify_thread_for_secure_path(
	const int32_t thread)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	return (thread == 15) ? (true) : (false);
#else
	return false;
#endif
}

static void cmdq_core_handle_secure_paths_exec_done_notify(
	const int32_t notifyThread,
	const int32_t value,
	CMDQ_TIME *pGotIRQ)
{
	uint32_t i;
	uint32_t raisedIRQ;
	int32_t thread;
	uint32_t secure_exec_counter[3];
	const uint32_t startThread = CMDQ_MIN_SECURE_THREAD_ID;
	const uint32_t endThread = CMDQ_MIN_SECURE_THREAD_ID +
		CMDQ_MAX_SECURE_THREAD_COUNT;

	memset(secure_exec_counter, 0, 3);
	/* HACK:
	 * IRQ of the notify thread,
	 * implies threre are some secure tasks execute done.
	 *
	 * when receive it, we should
	 * .suspend notify thread
	 * .scan shared memory to update secure path task status
	 *  (and notify waiting process context to check result)
	 * .resume notify thread
	 */

	/* it's okey that SWd update and NWd read shared memory, which used to
	 * store copy value of secure thread cookie, at the same time.
	 *
	 * The reason is NWd will receive a notify
	 * thread IRQ again after resume notify thread.
	 * The later IRQ let driver scan shared memory again.
	 * (note it's possible that same content in shared memory in such case)
	 */

	/* confirm if it is notify thread */
	if (false ==
		cmdq_core_is_valid_notify_thread_for_secure_path(notifyThread))
		return;



	raisedIRQ = cmdq_core_get_secure_IRQ_status();
	secure_exec_counter[0] = cmdq_core_get_secure_thread_exec_counter(12);
	secure_exec_counter[1] = cmdq_core_get_secure_thread_exec_counter(13);
	secure_exec_counter[2] = cmdq_core_get_secure_thread_exec_counter(14);
	CMDQ_MSG("%s, raisedIRQ:0x%08x, shared_cookie(%d, %d, %d)\n",
		 __func__,
		 raisedIRQ, secure_exec_counter[0], secure_exec_counter[1],
		 secure_exec_counter[2]);


	/* update tasks' status according cookie in shared memory */
	for (i = startThread; i < endThread; i++) {
		/* bit X = 1 means thread X raised IRQ */
		if (0 == (raisedIRQ & (0x1 << i)))
			continue;

		thread = i;
		cmdq_core_handle_secure_thread_done_impl(thread,
			value, pGotIRQ);
	}
	cmdq_core_set_secure_IRQ_status(0x0);
}
#endif

static void cmdqCoreHandleError(int32_t thread, int32_t value,
	CMDQ_TIME *pGotIRQ)
{
	struct ThreadStruct *pThread = NULL;
	struct TaskStruct *pTask = NULL;
	int32_t cookie;
	int32_t count;
	int32_t inner;
	int32_t status = 0;

	cookie = cmdq_core_thread_exec_counter(thread);

	CMDQ_ERR("IRQ: error thread=%d, irq_flag=0x%x, cookie:%d\n",
		thread, value, cookie);
	CMDQ_ERR("IRQ: Thread PC: 0x%08x, End PC:0x%08x\n",
		 CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread)),
		 CMDQ_REG_GET32(CMDQ_THR_END_ADDR(thread)));

	pThread = &(gCmdqContext.thread[thread]);

	/* we assume error happens BEFORE EOC */
	/* because it wouldn't be error if this interrupt is issue by EOC. */
	/* So we should inc by 1 to locate "current" task */
	cookie += 1;

	/* Set the issued task to error state */
#define CMDQ_TEST_PREFETCH_FOR_MULTIPLE_COMMAND
#ifdef CMDQ_TEST_PREFETCH_FOR_MULTIPLE_COMMAND
	cmdq_core_dump_task_in_thread(thread, true, true, true);
#endif
	/* suspend HW thread first, so that we work in a consistent state */
	/* outer function should acquire spinlock - gCmdqExecLock */
	status = cmdq_core_suspend_HW_thread(thread, __LINE__);
	if (status < 0) {
		/* suspend HW thread failed */
		CMDQ_ERR("IRQ: suspend HW thread failed!");
	}
	CMDQ_ERR("Error IRQ: always suspend thread(%d) to prevent error IRQ\n",
		thread);
	if (pThread->pCurTask[cookie % cmdq_core_max_task_in_thread(thread)] !=
		NULL) {
		pTask = pThread->pCurTask[cookie %
			cmdq_core_max_task_in_thread(thread)];
		pTask->gotIRQ = *pGotIRQ;
		pTask->irqFlag = value;
		cmdq_core_attach_error_task(pTask, thread, NULL);
		cmdq_core_remove_task_from_array_by_cookie(pThread,
			cookie % cmdq_core_max_task_in_thread(thread),
			TASK_STATE_ERR_IRQ);
	} else {
		CMDQ_ERR
		    ("IRQ: can not find task in %s, pc:0x%08x,end_pc:0x%08x\n",
				__func__,
		     CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread)),
		     CMDQ_REG_GET32(CMDQ_THR_END_ADDR(thread)));
		if (pThread->taskCount <= 0) {
			cmdq_core_disable_HW_thread(thread);
			CMDQ_ERR("IRQ: there is no task for thread (%d) %s\n",
				 thread, __func__);
		}
	}

	/* Set the remain tasks to done state */
	if (pThread->waitCookie <= cookie) {
		count = cookie - pThread->waitCookie + 1;
	} else if ((cookie+1) % CMDQ_MAX_COOKIE_VALUE == pThread->waitCookie) {
		count = 0;
		CMDQ_MSG("IRQ: duplicated cookie: waitCookie:%d, hwCookie:%d",
			pThread->waitCookie, cookie);
	} else {
		/* Counter wrapped */
		count = (CMDQ_MAX_COOKIE_VALUE - pThread->waitCookie + 1) +
			(cookie + 1);
		CMDQ_ERR("IRQ: cnt wrapped:waitCookie:%d,hwCookie:%d,count=%d",
			pThread->waitCookie, cookie, count);
	}

	for (inner = (pThread->waitCookie %
		cmdq_core_max_task_in_thread(thread));
		count > 0; count--, inner++) {
		if (inner >= cmdq_core_max_task_in_thread(thread))
			inner = 0;

		if (pThread->pCurTask[inner] != NULL) {
			pTask = pThread->pCurTask[inner];
			pTask->gotIRQ = (*pGotIRQ);
			/* we don't know the exact irq flag. */
			pTask->irqFlag = 0;
			cmdq_core_remove_task_from_array_by_cookie(
				pThread,
				inner, TASK_STATE_DONE);
		}
	}

	/* Error cookie will be handled in */
	/* cmdq_core_handle_wait_task_result_impl API */
	/**
	 *	pThread->waitCookie = cookie + 1;
	 *	   if (pThread->waitCookie > CMDQ_MAX_COOKIE_VALUE) {
	 *	   pThread->waitCookie -= (CMDQ_MAX_COOKIE_VALUE + 1);
	 *	}
	 */

	wake_up(&gCmdWaitQueue[thread]);
}

static void cmdqCoreHandleDone(int32_t thread, int32_t value,
	CMDQ_TIME *pGotIRQ)
{
	struct ThreadStruct *pThread;
	int32_t cookie;
	int32_t loopResult = 0;

	pThread = &(gCmdqContext.thread[thread]);

	/*  */
	/* Loop execution never gets done; unless */
	/* user loop function returns error */
	/*  */
	if (pThread->loopCallback != NULL) {
		loopResult = pThread->loopCallback(pThread->loopData);

		CMDQ_PROF_MMP(cmdq_mmp_get_event()->loopBeat,
			      MMPROFILE_FLAG_PULSE, thread, loopResult);
#ifdef CONFIG_MTK_CMDQ_TAB
		/* HACK: there are some seucre task execue done */
		cmdq_core_handle_secure_paths_exec_done_notify(
			thread, value, pGotIRQ);
#endif

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

		/* loop CB failed. the EXECUTION count */
		/* should not be used as cookie, */
		/* since it will increase by each loop iteration. */
		cookie = pThread->waitCookie;
	} else {
		/* task cookie */
		cookie = cmdq_core_thread_exec_counter(thread);
		CMDQ_MSG("Done: thread %d got cookie: %d\n", thread, cookie);
	}

	cmdq_core_handle_done_with_cookie_impl(thread, value, pGotIRQ, cookie);
}

void cmdqCoreHandleIRQ(int32_t thread)
{
	unsigned long flags = 0L;
	CMDQ_TIME gotIRQ;
	int value;
	int enabled;
	int32_t cookie;

	/* note that do_gettimeofday may cause */
	/* HWT in spin_lock_irqsave (ALPS01496779) */
	gotIRQ = sched_clock();

	/*  */
	/* Normal execution, marks tasks done and remove from thread */
	/* Also, handle "loop CB fail" case */
	/*  */
	spin_lock_irqsave(&gCmdqExecLock, flags);

	/* it is possible for another CPU core */
	/* to run "releaseTask" right before we acquire the spin lock */
	/* and thus reset / disable this HW thread */
	/* so we check both the IRQ flag and the enable bit of this thread */
	value = CMDQ_REG_GET32(CMDQ_THR_IRQ_STATUS(thread));

	if ((value & 0x13) == 0) {
		CMDQ_ERR("IRQ: thread %d got interrupt\n",
			thread);
		CMDQ_ERR("but IRQ flag is 0x%08x in NWd\n",
			value);
		spin_unlock_irqrestore(&gCmdqExecLock, flags);
		return;
	}

	if (cmdq_get_func()->isSecureThread(thread) == false) {
		enabled = CMDQ_REG_GET32(CMDQ_THR_ENABLE_TASK(thread));

		if ((enabled & 0x01) == 0) {
			CMDQ_ERR("thr:%d interrupt already disable:0x%08x\n",
				thread,
				enabled);
			spin_unlock_irqrestore(&gCmdqExecLock, flags);
			return;
		}
	}

	CMDQ_PROF_START(0, gCmdqThreadLabel[thread]);

	/* Read HW cookie here to print message only */
	cookie = cmdq_core_thread_exec_counter(thread);
	/* Move the reset IRQ before read HW cookie to */
	/* prevent race condition and save the cost of suspend */
	CMDQ_REG_SET32(CMDQ_THR_IRQ_STATUS(thread), ~value);
	CMDQ_MSG("IRQ: thread %d got interrupt, after reset\n",
		 thread);
	CMDQ_MSG("and IRQ flag is 0x%08x, cookie: %d\n",
		value, cookie);

	if (value & 0x12)
		cmdqCoreHandleError(thread, value, &gotIRQ);
	else if (value & 0x01)
		cmdqCoreHandleDone(thread, value, &gotIRQ);

	CMDQ_PROF_END(0, gCmdqThreadLabel[thread]);

	spin_unlock_irqrestore(&gCmdqExecLock, flags);
}

static struct TaskStruct *cmdq_core_search_task_by_pc(
	uint32_t threadPC,
	const struct ThreadStruct *pThread, int32_t thread)
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

/* Implementation of wait task done
 * Return:
 *     wait time of wait_event_timeout() kernel API
 *     . =0, for timeout elapsed,
 *     . >0, remain jiffies if condition passed
 *
 * Note process will go to sleep with state TASK_UNINTERRUPTIBLE until
 * the condition[task done] passed or timeout happened.
 */
static int32_t cmdq_core_wait_task_done_with_timeout_impl(
	struct TaskStruct *pTask, int32_t thread)
{
	int32_t waitQ;
	unsigned long flags;
	struct ThreadStruct *pThread = NULL;
	int32_t retryCount = 0;

	pThread = &(gCmdqContext.thread[thread]);


	/* timeout wait & make sure this task is finished. */
	/* pTask->taskState flag is updated */
	/* in IRQ handlers like cmdqCoreHandleDone. */
	retryCount = 0;
	waitQ = wait_event_timeout(gCmdWaitQueue[thread],
				   (pTask->taskState != TASK_STATE_BUSY
				    && pTask->taskState != TASK_STATE_WAITING),
				   /* timeout_jiffies); */
				   msecs_to_jiffies(CMDQ_PREDUMP_TIMEOUT_MS));

	/* if SW-timeout, pre-dump hang instructions */
	while (waitQ == 0 && retryCount < CMDQ_PREDUMP_RETRY_COUNT) {
		CMDQ_LOG("===== [CMDQ] SW timeout Pre-dump(%d)===========\n",
			 retryCount);

		++retryCount;

		spin_lock_irqsave(&gCmdqExecLock, flags);
		cmdq_core_dump_status("INFO");
		cmdq_core_dump_pc(pTask, thread, "INFO");
		cmdq_core_dump_thread(thread, "INFO");

		/* HACK: check trigger thread status */
		cmdq_core_dump_disp_trigger_loop("INFO");
		/* end of HACK */

		spin_unlock_irqrestore(&gCmdqExecLock, flags);

		/* then we wait again */
		waitQ = wait_event_timeout(gCmdWaitQueue[thread],
			(pTask->taskState != TASK_STATE_BUSY
			&& pTask->taskState != TASK_STATE_WAITING),
			msecs_to_jiffies(CMDQ_PREDUMP_TIMEOUT_MS));
	}

	return waitQ;
}

bool cmdq_core_check_task_finished(struct TaskStruct *pTask)
{
#if defined(CMDQ_SECURE_PATH_NORMAL_IRQ) || defined(CMDQ_SECURE_PATH_HW_LOCK)
	return (pTask->taskState == TASK_STATE_DONE);
#else
	return (pTask->taskState != TASK_STATE_BUSY);
#endif
}

static int32_t cmdq_core_handle_wait_task_result_secure_impl(
	struct TaskStruct *pTask,
	int32_t thread, const int32_t waitQ)
{
	int32_t i;
	int32_t status;
	struct ThreadStruct *pThread = NULL;
	/* error report */
	bool throwAEE = false;
	const char *module = NULL;
	int32_t irqFlag = 0;
	struct cmdqSecCancelTaskResultStruct result;
	char parsedInstruction[128] = { 0 };

	/* Init default status */
	status = 0;
	pThread = &(gCmdqContext.thread[thread]);
	memset(&result, 0, sizeof(struct cmdqSecCancelTaskResultStruct));

	/* lock cmdqSecLock */
	cmdq_sec_lock_secure_path();

	do {
		struct TaskPrivateStruct *private = CMDQ_TASK_PRIVATE(pTask);
		unsigned long flags = 0L;

		/* check if this task has finished */
		if (cmdq_core_check_task_finished(pTask))
			break;

		/* Oops, tha tasks is not done. */
		/* We have several possible error scenario: */
		/* 1. task still running (hang / timeout) */
		/* 2. IRQ pending (done or error/timeout IRQ) */
		/* 3. task's SW thread has been signaled (e.g. SIGKILL) */

		/* dump shared cookie */
		CMDQ_LOG
		    ("WAIT: [1]secure path failed, pTask:%p, thread:%d\n",
		     pTask, thread);
		CMDQ_LOG
		    ("shared_cookie(%d, %d, %d)\n",
		     cmdq_core_get_secure_thread_exec_counter(12),
		     cmdq_core_get_secure_thread_exec_counter(13),
		     cmdq_core_get_secure_thread_exec_counter(14));

		/* suppose that task failed, */
		/* entry secure world to confirm it */
		/* we entry secure world: */
		/* .check if pending IRQ and update */
		/* cookie value to shared memory */
		/* .confirm if task execute done  */
		/* if not, do error handle */
		/*     .recover M4U & DAPC setting */
		/*     .dump secure HW thread */
		/*     .reset CMDQ secure HW thread */
		cmdq_sec_cancel_error_task_unlocked(pTask, thread, &result);

		status = -ETIMEDOUT;
		throwAEE = !(private && private->internal &&
			private->ignore_timeout);
		/* shall we pass the error instru back from secure path?? */
		module = cmdq_get_func()->parseErrorModule(pTask);

		/* module dump */
		cmdq_core_attach_error_task(pTask, thread, NULL);

		/* module reset */
		/* TODO: get needReset infor by secure thread PC */
		cmdq_core_reset_hw_engine(pTask->engineFlag);

		/* remove all tasks in tread since */
		/* we have reset HW thread in SWd */
		spin_lock_irqsave(&gCmdqExecLock, flags);
		for (i = 0; i < cmdq_core_max_task_in_thread(thread); i++) {
			pTask = pThread->pCurTask[i];
			if (!pTask)
				continue;
			cmdq_core_remove_task_from_array_by_cookie(
				pThread, i,
				TASK_STATE_ERROR);
		}
		pThread->taskCount = 0;
		pThread->waitCookie = pThread->nextCookie;
		spin_unlock_irqrestore(&gCmdqExecLock, flags);
	} while (0);

	/* unlock cmdqSecLock */
	cmdq_sec_unlock_secure_path();

	/* throw AEE if nessary */
	if (throwAEE) {
		char buffer[200] = {0};
		int num = 0;
		const uint32_t instA = result.errInstr[1];
		const uint32_t instB = result.errInstr[0];
		const uint32_t op = (instA & 0xFF000000) >> 24;

		cmdq_core_interpret_instruction(parsedInstruction,
			sizeof(parsedInstruction), op,
			instA & (~0xFF000000), instB);

		num += snprintf(buffer + num, sizeof(buffer) - num,
			"%s in CMDQ IRQ:0x%02x, INST:(0x%08x, 0x%08x)",
			 module, irqFlag, instA, instB);
		num += snprintf(buffer + num, sizeof(buffer) - num,
			", OP:%s => %s\n",
			 cmdq_core_parse_op(op), parsedInstruction);
		CMDQ_AEE(module, "%s", buffer);
	}

	return status;
}

static int32_t cmdq_core_handle_wait_task_result_impl(
	struct TaskStruct *pTask, int32_t thread,
	const int32_t waitQ)
{
	int32_t status;
	int32_t index;
	unsigned long flags;
	struct ThreadStruct *pThread = NULL;
	const struct TaskStruct *pNGTask = NULL;
	bool markAsErrorTask = false;
	/* error report */
	bool throwAEE = false;
	const char *module = NULL;
	uint32_t instA = 0, instB = 0;
	int32_t irqFlag = 0;
	dma_addr_t task_pa = 0;

	/* Init default status */
	status = 0;
	pThread = &(gCmdqContext.thread[thread]);

	/* Note that although we disable IRQ, HW continues to execute */
	/* so it's possible to have pending IRQ */
	spin_lock_irqsave(&gCmdqExecLock, flags);

	do {
		struct TaskStruct *pNextTask = NULL;
		struct TaskStruct *pPrevTask = NULL;
		int32_t cookie = 0;
		long threadPC = 0L;
		struct TaskPrivateStruct *private = CMDQ_TASK_PRIVATE(pTask);

		status = 0;
		throwAEE = false;
		markAsErrorTask = false;

		if (pTask->taskState == TASK_STATE_DONE)
			break;

		CMDQ_ERR("Task state of %p is not TASK_STATE_DONE, %d\n",
			pTask, pTask->taskState);
#ifdef CONFIG_MTK_CMDQ_TAB
		CMDQ_ERR("thread:%d suspended:%d\n",
			thread, CMDQ_REG_GET32(CMDQ_THR_SUSPEND_TASK(thread)));
		cmdq_core_dump_thread(thread, "ERR");
		for (index = 0; index < CMDQ_MAX_THREAD_COUNT; index++) {
			if (index == thread)
				continue;
			cmdq_core_dump_thread(index, "ERR");
		}
#endif

		/* Oops, tha tasks is not done. */
		/* We have several possible error scenario: */
		/* 1. task still running (hang / timeout) */
		/* 2. IRQ pending (done or error/timeout IRQ) */
		/* 3. task's SW thread has been signaled (e.g. SIGKILL) */

		/* suspend HW thread first, */
		/* so that we work in a consistent state */
		status = cmdq_core_suspend_HW_thread(thread, __LINE__);
		if (status < 0)
			throwAEE = true;

		/* The cookie of the task currently being processed */
		cookie = CMDQ_GET_COOKIE_CNT(thread) + 1;
		threadPC = CMDQ_AREG_TO_PHYS(
			CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread)));

		/* process any pending IRQ */
		/* TODO: provide no spin lock version */
		/* because we already locked. */
		irqFlag = CMDQ_REG_GET32(CMDQ_THR_IRQ_STATUS(thread));
		if (irqFlag & 0x12)
			cmdqCoreHandleError(thread, irqFlag, &pTask->wakedUp);
		else if (irqFlag & 0x01)
			cmdqCoreHandleDone(thread, irqFlag, &pTask->wakedUp);

		CMDQ_REG_SET32(CMDQ_THR_IRQ_STATUS(thread), ~irqFlag);

		/* check if this task has finished after handling pending IRQ */
		if (pTask->taskState == TASK_STATE_DONE)
			break;

		/* Then decide we are SW timeout or SIGNALed (not an error) */
		if (waitQ == 0) {
			/* SW timeout and no IRQ received */
			markAsErrorTask = true;

			/* if we reach here, we're in errornous state. */
			/* print error log immediately. */
			cmdq_core_attach_error_task(pTask, thread, &pNGTask);
			CMDQ_ERR("SW timeout of task 0x%p on thread %d\n",
				pTask, thread);
			if (pTask != pNGTask) {
				CMDQ_ERR("pc stays in task:0x%p on thr %d\n",
					pNGTask,
					thread);
			}
			throwAEE = !(private && private->internal &&
				private->ignore_timeout);
			cmdq_core_parse_error(pNGTask, thread, &module,
				&irqFlag, &instA, &instB);
			status = -ETIMEDOUT;

		} else if (waitQ < 0) {
			/* Task be killed. Not an error, */
			/* but still need removal. */

			markAsErrorTask = false;

			if (-ERESTARTSYS == waitQ) {
				/* Error status print */
				CMDQ_ERR("Task %p waitQ = -ERESTARTSYS\n",
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
			/*  */
			/* if taskState is BUSY, this means we did not */
			/* reach EOC, did not have error IRQ. */
			/* - remove the task from thread.pCurTask[] */
			/* - and decrease thread.taskCount */
			/* NOTE: after this, the pCurTask will */
			/* not contain link to pTask anymore. */
			/* and pTask should become TASK_STATE_ERROR */

			/* we find our place in pThread->pCurTask[]. */
			for (index = 0;
				index < cmdq_core_max_task_in_thread(thread);
				++index) {
				if (pThread->pCurTask[index] != pTask)
					continue;
				/* update taskCount and pCurTask[] */
				cmdq_core_remove_task_from_array_by_cookie(
					pThread,
					index,
					markAsErrorTask	?
					TASK_STATE_ERROR :
					TASK_STATE_KILLED);
				break;
			}
		}

		if (pTask->pCMDEnd == NULL)
			break;

		pNextTask = NULL;
		/* find pTask's jump destination */
		if (pTask->pCMDEnd[0] == 0x10000001 &&
			!CMDQ_IS_END_ADDR(pTask->pCMDEnd[-1])) {
			pNextTask = cmdq_core_search_task_by_pc(
				pTask->pCMDEnd[-1],
				pThread, thread);
		} else {
			CMDQ_MSG("No next task: LAST instr:(0x%08x, 0x%08x)\n",
				 pTask->pCMDEnd[0], pTask->pCMDEnd[-1]);
		}

		/* Then, we try remove pTask from the */
		/* chain of pThread->pCurTask. */
		/* . if HW PC falls in pTask range */
		/* . HW EXEC_CNT += 1 */
		/* . thread.waitCookie += 1 */
		/* . set HW PC to next task head */
		/* . if not, find previous task (whose jump */
		/* address is pTask->MVABase) */
		/* . check if HW PC points is not at the EOC/JUMP end */
		/* . change jump to fake EOC(no IRQ) */
		/* . insert jump to next task head */
		/* and increase cmd buffer size */
		/* . if there is no next task, set HW End Address */
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
				CMDQ_MSG("resume task:0x%p from err,pa:0x%p\n",
					pNextTask, &task_pa);
			}
		} else if (pTask->taskState == TASK_STATE_ERR_IRQ) {
			/* Error IRQ might not stay in normal */
			/* Task range (jump to a strange part) */
			/* We always execute next due */
			/* to error IRQ must correct task */
			if (pNextTask) {
				/* cookie already +1 */
				CMDQ_REG_SET32(CMDQ_THR_EXEC_CNT(thread),
					cookie);
				pThread->waitCookie = cookie + 1;
				task_pa = cmdq_core_task_get_first_pa(
					pNextTask);
				CMDQ_REG_SET32(CMDQ_THR_CURR_ADDR(thread),
						   CMDQ_PHYS_TO_AREG(task_pa));
				CMDQ_MSG("resume task: 0x%p from err IRQ\n",
					pNextTask);
				CMDQ_MSG("WAIT: resume task: pa: 0x%p\n",
					&task_pa);
			}
		} else {
			pPrevTask = NULL;
			for (index = 0;
				index < cmdq_core_max_task_in_thread(thread);
				index++) {
				bool is_jump_to = false;

				pPrevTask = pThread->pCurTask[index];

				/* find which task JUMP into pTask */
				is_jump_to = (pPrevTask && pPrevTask->pCMDEnd &&
					pPrevTask->pCMDEnd[-1] ==
					cmdq_core_task_get_first_pa(pTask) &&
					pPrevTask->pCMDEnd[0] ==
						((CMDQ_CODE_JUMP << 24) | 0x1));

				if (is_jump_to) {
					/* Copy Jump instruction */
					pPrevTask->pCMDEnd[-1] =
						pTask->pCMDEnd[-1];
					pPrevTask->pCMDEnd[0] =
						pTask->pCMDEnd[0];

					if (pNextTask)
						cmdq_core_reorder_task_array(
							pThread, thread, index);
					else
						pThread->nextCookie--;

					CMDQ_VERBOSE
					    ("WAIT: modify jump to 0x%08x\n",
					     pTask->pCMDEnd[-1]);
					CMDQ_VERBOSE
					    ("WAIT: (pPrev:0x%p,pTask:0x%p)\n",
						pPrevTask, pTask);

					/* Give up fetched command,*/
					/* invoke CMDQ  */
					/* HW to re-fetch command */
					/* buffer again. */
					cmdq_core_invalidate_hw_fetched_buffer(
						thread);
					break;
				}
			}
		}
	} while (0);

	if (pThread->taskCount <= 0) {
		cmdq_core_disable_HW_thread(thread);
	} else {
		do {
			/* Reset GCE thread when task state is ERROR or KILL */
			uint32_t backupCurrPC, backupEnd, backupCookieCnt;
			int threadPrio;

			if (pTask->taskState == TASK_STATE_DONE)
				break;

			/* Backup PC, End address, and GCE */
			/* cookie count before reset GCE thread */
			backupCurrPC =
			    CMDQ_AREG_TO_PHYS(
				CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread)));
			backupEnd = CMDQ_AREG_TO_PHYS(
				CMDQ_REG_GET32(CMDQ_THR_END_ADDR(thread)));
			backupCookieCnt = CMDQ_GET_COOKIE_CNT(thread);
			CMDQ_LOG("Reset Backup Thread PC: 0x%08x,End:0x%08x\n",
			     backupCurrPC, backupEnd);
			CMDQ_LOG("CookieCnt: 0x%08x\n",
				backupCookieCnt);
			/* Reset GCE thread */
			if (cmdq_core_reset_HW_thread(thread) < 0) {
				status = -EFAULT;
				break;
			}

			CMDQ_REG_SET32(CMDQ_THR_INST_CYCLES(thread),
				cmdq_core_get_task_timeout_cycle(pThread));
			/* Set PC & End address */
			CMDQ_REG_SET32(CMDQ_THR_END_ADDR(thread),
				CMDQ_PHYS_TO_AREG(backupEnd));
			CMDQ_REG_SET32(CMDQ_THR_CURR_ADDR(thread),
				CMDQ_PHYS_TO_AREG(backupCurrPC));
			/* bit 0-2 for priority level; */
			threadPrio = cmdq_get_func()->priority(pTask->scenario);
			CMDQ_MSG("RESET HW THREAD: set HW thread(%d), qos:%d\n",
				thread,
				 threadPrio);
			CMDQ_REG_SET32(CMDQ_THR_CFG(thread), threadPrio & 0x7);
			/* For loop thread, do not enable timeout */
			CMDQ_REG_SET32(CMDQ_THR_IRQ_ENABLE(thread),
				       pThread->loopCallback ? 0x011 : 0x013);
			if (pThread->loopCallback) {
				CMDQ_MSG("RESET HW THREAD:(%d) in func 0x%p\n",
					 thread, pThread->loopCallback);
			}
			/* Set GCE cookie count */
			CMDQ_REG_SET32(CMDQ_THR_EXEC_CNT(thread),
				backupCookieCnt);
			/* Enable HW thread */
			CMDQ_REG_SET32(CMDQ_THR_ENABLE_TASK(thread), 0x01);
		} while (0);
		cmdq_core_resume_HW_thread(thread);
	}

	spin_unlock_irqrestore(&gCmdqExecLock, flags);

	if (throwAEE) {
		const uint32_t op = (instA & 0xFF000000) >> 24;

		switch (op) {
		case CMDQ_CODE_WFE:
			CMDQ_AEE(module,
			"%s in CMDQ IRQ:0x%02x, INST:(0x%08x, 0x%08x), OP:WAIT EVENT:%s\n",
			module, irqFlag, instA, instB,
			cmdq_core_get_event_name(instA & (~0xFF000000)));
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

static int32_t cmdq_core_wait_task_done(struct TaskStruct *pTask,
	long timeout_jiffies)
{
	int32_t waitQ;
	int32_t status;
	uint32_t thread;
	struct ThreadStruct *pThread = NULL;

	status = 0;		/* Default status */

	thread = pTask->thread;
	if (thread == CMDQ_INVALID_THREAD) {
		CMDQ_PROF_MMP(cmdq_mmp_get_event()->wait_thread,
			      MMPROFILE_FLAG_PULSE,
			      ((unsigned long)pTask), -1);

		CMDQ_PROF_START(current->pid, "wait_for_thread");

		CMDQ_LOG("pid:%d task:0x%p wait for valid thread first\n",
			current->pid, pTask);

		/* wait for acquire thread (this is done by */
		/* cmdq_core_consume_waiting_list); */
		waitQ = wait_event_timeout(gCmdqThreadDispatchQueue,
			(pTask->thread != CMDQ_INVALID_THREAD),
			msecs_to_jiffies(CMDQ_ACQUIRE_THREAD_TIMEOUT_MS));

		CMDQ_PROF_END(current->pid, "wait_for_thread");
		if (waitQ == 0 || pTask->thread == CMDQ_INVALID_THREAD) {
			mutex_lock(&gCmdqTaskMutex);
			/* it's possible that the task was just consumed now. */
			/* so check again. */
			if (pTask->thread == CMDQ_INVALID_THREAD) {
				struct TaskPrivateStruct *private =
					CMDQ_TASK_PRIVATE(pTask);

				/* task may already released, */
				/* or starved to death */
				CMDQ_ERR("task 0x%p timeout w/invalid thread\n",
					pTask);
				cmdq_core_dump_task(pTask);
				/* remove from waiting list, */
				/* so that it won't be consumed in the future */
				list_del_init(&(pTask->listEntry));

				if (private && private->internal)
					private->ignore_timeout = true;

				mutex_unlock(&gCmdqTaskMutex);
				return -ETIMEDOUT;
			}

			/* valid thread, so we keep going */
			mutex_unlock(&gCmdqTaskMutex);
		}
	}

	/* double confim if it get a valid thread */
	thread = pTask->thread;
	if ((thread < 0) || (thread >= CMDQ_MAX_THREAD_COUNT)) {
		CMDQ_ERR("invalid thread %d in %s\n", thread, __func__);
		return -EINVAL;
	}

	pThread = &(gCmdqContext.thread[thread]);

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->wait_task,
		      MMPROFILE_FLAG_PULSE, ((unsigned long)pTask), thread);

	CMDQ_PROF_START(current->pid, "wait_for_task_done");

	/* start to wait */
	pTask->beginWait = sched_clock();
	CMDQ_MSG("-->WAIT: task 0x%p on thread %d timeout: %d(ms) begin\n",
		pTask, thread,
		jiffies_to_msecs(timeout_jiffies));
	waitQ = cmdq_core_wait_task_done_with_timeout_impl(pTask, thread);

	/* wake up! */
	/* so the maximum total waiting time would be */
	/* CMDQ_PREDUMP_TIMEOUT_MS * CMDQ_PREDUMP_RETRY_COUNT */
	pTask->wakedUp = sched_clock();
	CMDQ_MSG("WAIT: task 0x%p waitq=%d state=%d\n",
		pTask, waitQ, pTask->taskState);
	CMDQ_PROF_END(current->pid, "wait_for_task_done");

	status = (pTask->secData.is_secure == false) ?
	    cmdq_core_handle_wait_task_result_impl(pTask, thread, waitQ) :
	    cmdq_core_handle_wait_task_result_secure_impl(pTask, thread, waitQ);

	CMDQ_MSG("<--WAIT: task 0x%p on thread %d end\n", pTask, thread);

	return status;
}

static int32_t cmdq_core_exec_task_async_secure_impl(
	struct TaskStruct *pTask,
	int32_t thread)
{
	int32_t status;
	struct ThreadStruct *pThread;
	int32_t cookie;
	char longMsg[CMDQ_LONGSTRING_MAX];
	uint32_t msgOffset;
	int32_t msgMAXSize;
	uint32_t *pVABase = NULL;
	dma_addr_t MVABase = 0;
	unsigned long flags = 0L;

	cmdq_core_longstring_init(longMsg, &msgOffset, &msgMAXSize);
	cmdq_core_get_task_first_buffer(pTask, &pVABase, &MVABase);
	cmdqCoreLongString(false, longMsg, &msgOffset, &msgMAXSize,
		   "-->EXEC: task: 0x%p on thread: %d begin, VABase: 0x%p,",
		   pTask, thread, pVABase);
	cmdqCoreLongString(false, longMsg, &msgOffset, &msgMAXSize,
		   " MVABase: 0x%pa, command size: %d, bufferSize: %d, scenario: %d, flag: 0x%llx\n",
		   &(MVABase), pTask->commandSize, pTask->bufferSize,
		   pTask->scenario, pTask->engineFlag);
	if (msgOffset > 0) {
		/* print message */
		CMDQ_MSG("%s", longMsg);
	}

	status = 0;
	pThread = &(gCmdqContext.thread[thread]);

	cmdq_sec_lock_secure_path();

	/* setup whole patah */
	status = cmdq_sec_allocate_path_resource_unlocked(true);
	if (status < 0)
		return status;

	/* update task's thread info */
	pTask->thread = thread;
	pTask->irqFlag = 0;
	pTask->taskState = TASK_STATE_BUSY;

	/* insert task to pThread's task lsit, and */
	/* delay HW config when entry SWd */
	spin_lock_irqsave(&gCmdqExecLock, flags);
	if (pThread->taskCount <= 0) {
		cookie = 1;
		cmdq_core_insert_task_from_thread_array_by_cookie(pTask,
			pThread, cookie,
			true);
	} else {
		/* append directly */
		cookie = pThread->nextCookie;
		cmdq_core_insert_task_from_thread_array_by_cookie(
			pTask,
			pThread, cookie,
			false);
	}
	spin_unlock_irqrestore(&gCmdqExecLock, flags);

	pTask->trigger = sched_clock();

	/* execute */
	status = cmdq_sec_exec_task_async_unlocked(pTask, thread);

	if (status < 0) {
		/* config failed case, dump for more detail */
		cmdq_core_attach_error_task(pTask, thread, NULL);
		cmdq_core_turnoff_first_dump();
		cmdq_core_remove_task_from_thread_array_when_secure_submit_fail(
			pThread, cookie);
	}


	cmdq_sec_unlock_secure_path();

	return status;
}

static inline int32_t cmdq_core_exec_find_task_slot(
	struct TaskStruct **pLast, struct TaskStruct *pTask,
	int32_t thread, int32_t loop)
{
	int32_t status = 0;
	struct ThreadStruct *pThread;
	struct TaskStruct *pPrev;
	int32_t index;
	int32_t prev;
	int32_t cookie;
	dma_addr_t task_pa = 0;

	pThread = &(gCmdqContext.thread[thread]);
	cookie = pThread->nextCookie;

	/* Traverse forward to adjust tasks' order */
	/* according to their priorities */
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
		while ((pPrev == NULL) && (loop > 1)) {
			CMDQ_LOG("pPrev is NULL, prev:%d, loop:%d, index:%d\n",
				prev, loop, index);
			prev = prev - 1;
			if (prev < 0)
				prev = cmdq_core_max_task_in_thread(thread) - 1;

			pPrev = pThread->pCurTask[prev];
			loop--;
		}

		if (pPrev == NULL) {
			cmdq_core_attach_error_task(pTask, thread, NULL);
			CMDQ_ERR("Invalid task state for reorder %d %d\n",
				index, loop);
			status = -EFAULT;
			break;
		}

		if (loop <= 1) {
			task_pa = cmdq_core_task_get_first_pa(pPrev);
			CMDQ_MSG(
				"Set current(%d) order for new task, org PC(0x%p): %pa, size: %d inst: 0x%08x:%08x line: %d\n",
				index, pPrev,
				&task_pa, pTask->commandSize,
				pPrev->pCMDEnd[0], pPrev->pCMDEnd[-1],
				__LINE__);

			pThread->pCurTask[index] = pTask;
			/* Jump: Absolute */
			pPrev->pCMDEnd[0] = ((CMDQ_CODE_JUMP << 24) | 0x1);
			/* Jump to here */
			task_pa = cmdq_core_task_get_first_pa(pTask);
			pPrev->pCMDEnd[-1] = task_pa;
			CMDQ_VERBOSE("EXEC: modify jump to 0x%pa, line: %d\n",
				&(task_pa), __LINE__);

#ifndef CMDQ_APPEND_WITHOUT_SUSPEND
			/* re-fetch command buffer again. */
			cmdq_core_invalidate_hw_fetched_buffer(thread);
#endif
			break;
		}

		if (pPrev->priority < pTask->priority) {
			CMDQ_LOG("Switch prev(%d, 0x%p) and curr(%d, 0x%p)\n",
				 prev, pPrev, index, pTask);

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
			task_pa = cmdq_core_task_get_first_pa(pPrev);
			pTask->pCMDEnd[-1] = task_pa;
			CMDQ_VERBOSE("EXEC: modify jump to 0x%pa, line:%d\n",
				&(task_pa), __LINE__);

#ifndef CMDQ_APPEND_WITHOUT_SUSPEND
			/* re-fetch command buffer again. */
			cmdq_core_invalidate_hw_fetched_buffer(thread);
#endif
			if (*pLast == pTask) {
				CMDQ_LOG("update pLast from 0x%p to 0x%p\n",
					pTask, pPrev);
				*pLast = pPrev;
			}
		} else {
			task_pa = cmdq_core_task_get_first_pa(pPrev);
			CMDQ_MSG("Set current(%d) order for new task\n",
				index);
			CMDQ_MSG("org PC(0x%p):%pa,size:%d inst:0x%08x:%08x\n",
				pPrev,
				&task_pa, pPrev->commandSize,
				pPrev->pCMDEnd[0], pPrev->pCMDEnd[-1]);

			pThread->pCurTask[index] = pTask;
			/* Jump: Absolute */
			pPrev->pCMDEnd[0] = (CMDQ_CODE_JUMP << 24 | 0x1);
			/* Jump to here */
			task_pa = cmdq_core_task_get_first_pa(pTask);
			pPrev->pCMDEnd[-1] = task_pa;
			CMDQ_VERBOSE("EXEC: modify jump to %pa, line:%d\n",
				&task_pa, __LINE__);

#ifndef CMDQ_APPEND_WITHOUT_SUSPEND
			/* re-fetch command buffer again. */
			cmdq_core_invalidate_hw_fetched_buffer(thread);
#endif
			break;
		}
	}

	CMDQ_MSG("Reorder %d tasks for performance end, pLast:0x%p\n",
		loop, *pLast);

	return status;
}

static int32_t cmdq_core_exec_task_async_impl(
	struct TaskStruct *pTask, int32_t thread)
{
	int32_t status;
	struct ThreadStruct *pThread;
	struct TaskStruct *pLast;
	unsigned long flags;
	int32_t loop;
	uint32_t minimum;
	uint32_t cookie;
	int threadPrio = 0;
	uint32_t EndAddr;
	char longMsg[CMDQ_LONGSTRING_MAX];
	uint32_t msgOffset;
	int32_t msgMAXSize;
	uint32_t *pVABase = NULL;
	dma_addr_t MVABase = 0;

	cmdq_core_longstring_init(longMsg, &msgOffset, &msgMAXSize);
	cmdq_core_get_task_first_buffer(pTask, &pVABase, &MVABase);
	cmdqCoreLongString(false, longMsg, &msgOffset, &msgMAXSize,
		"-->EXEC:task 0x%p on thread %d begin,VABase:0x%p,MVABase:%p,",
			   pTask, thread, pVABase, &(MVABase));
	cmdqCoreLongString(false, longMsg, &msgOffset, &msgMAXSize,
		" Size: %d, bufferSize: %d, scenario:%d, flag:0x%llx\n",
		pTask->commandSize, pTask->bufferSize, pTask->scenario,
		pTask->engineFlag);
	if (msgOffset > 0) {
		/* print message */
		CMDQ_MSG("%s", longMsg);
	}

	status = 0;

	pThread = &(gCmdqContext.thread[thread]);

	pTask->trigger = sched_clock();

	spin_lock_irqsave(&gCmdqExecLock, flags);

	/* update task's thread info */
	pTask->thread = thread;
	pTask->irqFlag = 0;
	pTask->taskState = TASK_STATE_BUSY;

	/* update task end address by with thread */
	if (CMDQ_IS_END_ADDR(pTask->pCMDEnd[-1])) {
		pTask->pCMDEnd[-1] = CMDQ_THR_FIX_END_ADDR(thread);
		/* make sure address change to DRAM before start HW thread */
		smp_mb();
	}

	if (pThread->taskCount <= 0) {
		bool enablePrefetch;

		CMDQ_MSG("EXEC: new HW thread(%d)\n", thread);
		if (cmdq_core_reset_HW_thread(thread) < 0) {
			spin_unlock_irqrestore(&gCmdqExecLock, flags);
			return -EFAULT;
		}

		CMDQ_REG_SET32(CMDQ_THR_INST_CYCLES(thread),
			       cmdq_core_get_task_timeout_cycle(pThread));
#ifdef _CMDQ_DISABLE_MARKER_
		enablePrefetch = cmdq_core_thread_prefetch_size(thread) > 0;
		if (enablePrefetch) {
			CMDQ_MSG(
			"EXEC: set HW thread(%d) enable prefetch, size(%d)!\n",
			thread, cmdq_core_thread_prefetch_size(thread));
			CMDQ_REG_SET32(CMDQ_THR_PREFETCH(thread), 0x1);
		}
#endif
		threadPrio = cmdq_get_func()->priority(pTask->scenario);
		MVABase = cmdq_core_task_get_first_pa(pTask);
		EndAddr = CMDQ_PHYS_TO_AREG(CMDQ_THR_FIX_END_ADDR(thread));
		CMDQ_MSG("set thread(%d) pc: 0x%pa, qos:%d end_addr:0x%08x\n",
			 thread, &MVABase, threadPrio, EndAddr);
		CMDQ_REG_SET32(CMDQ_THR_END_ADDR(thread), EndAddr);
		CMDQ_REG_SET32(CMDQ_THR_CURR_ADDR(thread),
			CMDQ_PHYS_TO_AREG(MVABase));

		/* bit 0-2 for priority level; */
		CMDQ_REG_SET32(CMDQ_THR_CFG(thread), threadPrio & 0x7);

		/* For loop thread, do not enable timeout */
		CMDQ_REG_SET32(CMDQ_THR_IRQ_ENABLE(thread),
			pThread->loopCallback ? 0x011 : 0x013);

		if (pThread->loopCallback) {
			CMDQ_MSG("EXEC: HW thread(%d) in loop func 0x%p\n",
				thread,
				pThread->loopCallback);
		}

		/* attach task to thread */
		minimum = CMDQ_GET_COOKIE_CNT(thread);
		cmdq_core_insert_task_from_thread_array_by_cookie(pTask,
			pThread,
			(minimum + 1),
								  true);

		/* verify that we don't corrupt EOC + JUMP pattern */
		cmdq_core_verfiy_command_end(pTask);

		/* enable HW thread */
		CMDQ_MSG("enable HW thread(%d)\n", thread);

		CMDQ_PROF_MMP(cmdq_mmp_get_event()->thread_en,
			      MMPROFILE_FLAG_PULSE, thread,
			      pThread->nextCookie - 1);

		CMDQ_REG_SET32(CMDQ_THR_ENABLE_TASK(thread), 0x01);
#ifdef CMDQ_MDP_MET_STATUS
		/* MET MMSYS : Primary Trigger start */
		if (met_mmsys_event_gce_thread_begin) {
			cmdq_core_get_task_first_buffer(pTask,
				&pVABase, &MVABase);
			met_mmsys_event_gce_thread_begin(thread,
				(uintptr_t) pTask, pTask->engineFlag,
				(void *)pVABase, pTask->commandSize);
		}
#endif	/* end of CMDQ_MDP_MET_STATUS */
	} else {
		uint32_t last_cookie;
		uint32_t last_inst_pa = 0;
		uint32_t thread_pc = 0;
		uint32_t end_addr = 0;

		CMDQ_MSG("EXEC: reuse HW thread(%d), taskCount:%d\n",
			thread, pThread->taskCount);

#ifdef CMDQ_APPEND_WITHOUT_SUSPEND
		cmdqCoreClearEvent(CMDQ_SYNC_TOKEN_APPEND_THR(thread));
#else
		if (CMDQ_TASK_IS_INTERNAL(pTask) && gStressContext.exec_suspend)
			gStressContext.exec_suspend(pTask, thread);

		status = cmdq_core_suspend_HW_thread(thread, __LINE__);
		if (status < 0) {
			spin_unlock_irqrestore(&gCmdqExecLock, flags);
			return status;
		}

		CMDQ_REG_SET32(CMDQ_THR_INST_CYCLES(thread),
			       cmdq_core_get_task_timeout_cycle(pThread));
#endif

		cookie = pThread->nextCookie;

		/* Boundary case tested: EOC have been */
		/* executed, but JUMP is not executed */
		/* Thread PC: 0x9edc0dd8, End: 0x9edc0de0, */
		/* Curr Cookie: 1, Next Cookie: 2 */

		/*
		 * Check if pc stay at last jump since GCE may not execute it,
		 * even if we change jump instruction before resume.
		 */
		last_cookie = pThread->nextCookie <= 0 ?
			(cmdq_core_max_task_in_thread(thread) - 1) :
			(pThread->nextCookie - 1) %
				cmdq_core_max_task_in_thread(thread);
		last_inst_pa = pThread->pCurTask[last_cookie] ?
			cmdq_core_task_get_eoc_pa(
			pThread->pCurTask[last_cookie]) + CMDQ_INST_SIZE :
			0;
		thread_pc = CMDQ_AREG_TO_PHYS(
			CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread)));
		end_addr = CMDQ_AREG_TO_PHYS(
			CMDQ_REG_GET32(CMDQ_THR_END_ADDR(thread)));

		/*
		 * PC = END - 8, EOC is executed
		 * PC = END - 0, All CMDs are executed
		 */
		if ((thread_pc == end_addr) || (thread_pc == last_inst_pa)) {
			cmdq_core_longstring_init(longMsg,
				&msgOffset,
				&msgMAXSize);
			MVABase = cmdq_core_task_get_first_pa(pTask);
			cmdqCoreLongString(true, longMsg,
				&msgOffset, &msgMAXSize,
				"EXEC: Task: 0x%p Set HW thread(%d) pc from 0x%08x(end:0x%08x) to 0x%pa,",
				pTask, thread, thread_pc, end_addr, &MVABase);
			cmdqCoreLongString(true, longMsg,
				&msgOffset, &msgMAXSize,
				" oriNextCookie:%d, oriTaskCount:%d\n",
					   cookie, pThread->taskCount);
			if (msgOffset > 0) {
				/* print message */
				CMDQ_LOG("%s", longMsg);
			}

			/* set to pTask directly */
			EndAddr = CMDQ_PHYS_TO_AREG(
				CMDQ_THR_FIX_END_ADDR(thread));
			CMDQ_MSG("EXEC: set end addr: 0x%08x for task: 0x%p\n",
				EndAddr,
				pTask);
			CMDQ_REG_SET32(CMDQ_THR_END_ADDR(thread), EndAddr);
			CMDQ_REG_SET32(CMDQ_THR_CURR_ADDR(thread),
				CMDQ_PHYS_TO_AREG(MVABase));

			pThread->pCurTask[
				cookie % cmdq_core_max_task_in_thread(thread)] =
					pTask;
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

			CMDQ_MSG("Reorder task: 0x%p in range [%d, %d]\n",
				pTask, minimum, cookie);
			CMDQ_MSG("with count %d thread %d\n",
				loop, thread);

			/* ALPS01672377 */
			/* .note pThread->taskCount-- when */
			/* remove task from pThread in ISR */
			/* .In mutlple SW clients or async case, */
			/*  clients may continue submit */
			/* tasks with overlap engines */
			/*  it's okey 0 = abs(pThread->nextCookie, */
			/* THR_CNT+1) when... */
			/*  .submit task_1, trigger GCE */
			/*  .submit task_2: */
			/*  .GCE exec task1 done */
			/*  .task_2 lock execLock when insert task to thread */
			/*  .task 1's IRQ */

			if (loop < 0) {
				cmdq_core_dump_task_in_thread(thread,
					true, true, false);

				cmdq_core_longstring_init(longMsg,
					&msgOffset, &msgMAXSize);
				cmdqCoreLongString(true, longMsg,
					&msgOffset,
					&msgMAXSize,
					"Invalid task cnt(%d) ",
					loop);
				cmdqCoreLongString(true, longMsg,
					&msgOffset,
					&msgMAXSize,
					" in thr:%d for reorder,",
					thread);
				cmdqCoreLongString(true, longMsg,
					&msgOffset, &msgMAXSize,
					" nextCookie:%d, nextCookieHW:%d,",
					pThread->nextCookie,
					minimum);
				cmdqCoreLongString(true, longMsg,
					&msgOffset, &msgMAXSize,
					" pTask:%p\n",
					pTask);
				if (msgOffset > 0) {
					/* print message */
					CMDQ_AEE("CMDQ", "%s", longMsg);
				}
#ifdef CMDQ_APPEND_WITHOUT_SUSPEND
				cmdqCoreSetEvent(
					CMDQ_SYNC_TOKEN_APPEND_THR(thread));
#endif
				spin_unlock_irqrestore(&gCmdqExecLock, flags);
				return -EFAULT;
			}

			if (loop > cmdq_core_max_task_in_thread(thread)) {
				CMDQ_LOG("loop:%d, execeed max task in thread",
					loop);
				loop = loop %
					cmdq_core_max_task_in_thread(thread);
			}
			CMDQ_MSG("Reorder %d tasks for performance begin\n",
				loop);
			/* By default, pTask is the last task, */
			/* and insert [cookie % CMDQ_MAX_TASK_IN_THREAD] */
			pLast = pTask;

			status = cmdq_core_exec_find_task_slot(&pLast,
				pTask, thread, loop);
			if (status < 0) {
#ifdef CMDQ_APPEND_WITHOUT_SUSPEND
				cmdqCoreSetEvent(
					CMDQ_SYNC_TOKEN_APPEND_THR(thread));
#endif
				spin_unlock_irqrestore(&gCmdqExecLock, flags);
				CMDQ_AEE("CMDQ",
					"Invalid task state for reorder.\n");
				return status;
			}

			/* We must set memory barrier here */
			/*to make sure we modify jump before enable thread */
			smp_mb();

			pThread->taskCount++;
			pThread->allowDispatching = 1;
		}

		pThread->nextCookie += 1;
		if (pThread->nextCookie > CMDQ_MAX_COOKIE_VALUE) {
			/* Reach the maximum cookie */
			pThread->nextCookie = 0;
		}

		/* verify that we don't corrupt EOC + JUMP pattern */
		cmdq_core_verfiy_command_end(pTask);

		/* resume HW thread */
		CMDQ_PROF_MMP(cmdq_mmp_get_event()->thread_en,
			      MMPROFILE_FLAG_PULSE,
			      thread,
			      pThread->nextCookie - 1);
#ifdef CMDQ_APPEND_WITHOUT_SUSPEND
		cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_APPEND_THR(thread));
#else
		cmdq_core_resume_HW_thread(thread);
#endif
	}

	spin_unlock_irqrestore(&gCmdqExecLock, flags);

	CMDQ_MSG("<--EXEC: status: %d\n", status);

	return status;
}

#ifdef CMDQ_PROFILE
static const char *gCmdqThreadLabel[CMDQ_MAX_THREAD_COUNT] = {
	"CMDQ_IRQ_THR_0",
	"CMDQ_IRQ_THR_1",
	"CMDQ_IRQ_THR_2",
	"CMDQ_IRQ_THR_3",
	"CMDQ_IRQ_THR_4",
	"CMDQ_IRQ_THR_5",
	"CMDQ_IRQ_THR_6",
	"CMDQ_IRQ_THR_7",
	"CMDQ_IRQ_THR_8",
	"CMDQ_IRQ_THR_9",
	"CMDQ_IRQ_THR_10",
	"CMDQ_IRQ_THR_11",
	"CMDQ_IRQ_THR_12",
	"CMDQ_IRQ_THR_13",
	"CMDQ_IRQ_THR_14",
	"CMDQ_IRQ_THR_15",
};
#endif

int32_t cmdqCoreSuspend(void)
{
	unsigned long flags = 0L;
	struct EngineStruct *pEngine = NULL;
	uint32_t execThreads = 0x0;
	int refCount = 0;
	bool killTasks = false;
	struct TaskStruct *pTask = NULL;
	struct list_head *p = NULL;
	int i = 0;

	/* destroy secure path notify thread */
	cmdq_core_stop_secure_path_notify_thread();

	pEngine = gCmdqContext.engine;

	refCount = atomic_read(&gCmdqThreadUsage);
	if (refCount)
		execThreads = CMDQ_REG_GET32(CMDQ_CURR_LOADED_THR);

	if (cmdq_get_func()->moduleEntrySuspend(pEngine) < 0) {
		CMDQ_ERR("[SUSPEND] MDP running, kill tasks\n");
		CMDQ_ERR("threads:0x%08x, ref:%d\n",
			execThreads,
			refCount);
		killTasks = true;
	} else if ((refCount > 0) || (0x80000000 & execThreads)) {
		CMDQ_ERR("[SUSPEND] other running, kill tasks.\n");
		CMDQ_ERR("threads:0x%08x, ref:%d\n",
			execThreads, refCount);
		killTasks = true;
	}

	/*  */
	/* We need to ensure the system is ready to suspend, */
	/* so kill all running CMDQ tasks */
	/* and release HW engines. */
	/*  */
	if (killTasks) {
		/* print active tasks */
		CMDQ_ERR("[SUSPEND] active tasks during suspend:\n");
		list_for_each(p, &gCmdqContext.taskActiveList) {
			pTask = list_entry(p, struct TaskStruct, listEntry);
			if (cmdq_core_is_valid_in_active_list(pTask) == true)
				cmdq_core_dump_task(pTask);
		}

		/* remove all active task from thread */
		CMDQ_ERR("[SUSPEND] remove all active tasks\n");
		list_for_each(p, &gCmdqContext.taskActiveList) {
			pTask = list_entry(p, struct TaskStruct, listEntry);

			if (pTask->thread != CMDQ_INVALID_THREAD) {
				spin_lock_irqsave(&gCmdqExecLock, flags);

				cmdq_core_force_remove_task_from_thread(pTask,
					pTask->thread);
				pTask->taskState = TASK_STATE_KILLED;

				spin_unlock_irqrestore(&gCmdqExecLock, flags);

				/* release all thread and mark all */
				/* active tasks as "KILLED" */
				/* (so that thread won't release again) */
				/* release all Threads & HW Clocks */
				CMDQ_ERR("[SUSPEND] release all HW\n");
				cmdq_core_release_thread(pTask);
			}
		}

		/* TODO: skip secure path thread... */
		/* disable all HW thread */
		CMDQ_ERR("[SUSPEND] disable all HW threads\n");
		for (i = 0; i < CMDQ_MAX_THREAD_COUNT; ++i)
			cmdq_core_disable_HW_thread(i);

		/* reset all threadStruct */
		memset(&gCmdqContext.thread[0], 0, sizeof(gCmdqContext.thread));
		cmdq_core_reset_thread_struct();

		/* reset all engineStruct */
		memset(&gCmdqContext.engine[0], 0, sizeof(gCmdqContext.engine));
		cmdq_core_reset_engine_struct();
	}

	spin_lock_irqsave(&gCmdqThreadLock, flags);
	gCmdqSuspended = true;
	spin_unlock_irqrestore(&gCmdqThreadLock, flags);

	/* ALWAYS allow suspend */
	return 0;
}


int32_t cmdq_core_reume_impl(const char *tag)
{
	unsigned long flags = 0L;
	int refCount = 0;

	spin_lock_irqsave(&gCmdqThreadLock, flags);

	refCount = atomic_read(&gCmdqThreadUsage);
	CMDQ_MSG("[%s] resume, refCount:%d\n", tag, refCount);

	gCmdqSuspended = false;

	/* during suspending, there may be queued tasks. */
	/* we should process them if any. */
	if (!work_pending(&gCmdqContext.taskConsumeWaitQueueItem)) {
		CMDQ_MSG("[%s] there are undone task, process them\n", tag);
		/* we use system global work queue (kernel thread kworker/n) */
		CMDQ_PROF_MMP(cmdq_mmp_get_event()->consume_add,
			MMPROFILE_FLAG_PULSE, 0, 0);
		queue_work(gCmdqContext.taskConsumeWQ,
			&gCmdqContext.taskConsumeWaitQueueItem);
	}

	spin_unlock_irqrestore(&gCmdqThreadLock, flags);
	return 0;
}

int32_t cmdqCoreResume(void)
{
	CMDQ_VERBOSE("[RESUME] do nothing\n");
	/* do nothing */
	return 0;
}

int32_t cmdqCoreResumedNotifier(void)
{
	/* TEE project limitation:
	 * .t-base daemon process is available after process-unfreeze
	 * .need t-base daemon for communication to secure world
	 * .M4U port security setting backup/resore needs to entry secure world
	 * .M4U port security setting is access normal PA
	 *
	 * Delay resume timing until process-unfreeze done in order to
	 * ensure M4U driver had restore M4U port security setting
	 */
	CMDQ_VERBOSE("[RESUME] %s\n", __func__);
	return cmdq_core_reume_impl("RESUME_NOTIFIER");
}

static int32_t cmdq_core_exec_task_async_with_retry(
	struct TaskStruct *pTask,
	int32_t thread)
{
	int32_t retry = 0;
	int32_t status = 0;
	struct ThreadStruct *pThread;

	pThread = &(gCmdqContext.thread[thread]);

	if (pThread->loopCallback) {
		/* Do not insert Wait for loop due to loop no need append */
		CMDQ_MSG("Ignore insert wait for loop task\n");
	} else {
#ifdef CMDQ_APPEND_WITHOUT_SUSPEND
		/* Shift JUMP and EOC */
		pTask->pCMDEnd += 2;
		pTask->pCMDEnd[0] = pTask->pCMDEnd[-2];
		pTask->pCMDEnd[-1] = pTask->pCMDEnd[-3];
		pTask->pCMDEnd[-2] = pTask->pCMDEnd[-4];
		pTask->pCMDEnd[-3] = pTask->pCMDEnd[-5];
		/* Update original JUMP to wait event */
		/* Sync: Op and sync event */
		pTask->pCMDEnd[-4] = (CMDQ_CODE_WFE << 24) |
			CMDQ_SYNC_TOKEN_APPEND_THR(thread);
		/* Sync: Wait and no clear */
		pTask->pCMDEnd[-5] = ((0 << 31) | (1 << 15) | 1);
		pTask->commandSize += CMDQ_INST_SIZE;
		/* make sure instructions are synced in DRAM */
		smp_mb();
		CMDQ_MSG
		    ("After insert wait: pTask 0x%p\n",
		     pTask);
		CMDQ_MSG
		    ("last 3 instr (%08x:%08x, %08x:%08x, %08x:%08x)\n",
		     pTask->pCMDEnd[-5],
		     pTask->pCMDEnd[-4],
		     pTask->pCMDEnd[-3],
		     pTask->pCMDEnd[-2],
		     pTask->pCMDEnd[-1],
		     pTask->pCMDEnd[0]);
#endif
	}

	do {
		if (cmdq_core_verfiy_command_end(pTask) == false) {
			status = -EFAULT;
			break;
		}

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

static int32_t cmdq_core_consume_waiting_list(struct work_struct *_ignore)
{
	struct list_head *p, *n = NULL;
	struct TaskStruct *pTask = NULL;
	struct ThreadStruct *pThread = NULL;
	int32_t thread = CMDQ_INVALID_THREAD;
	int32_t status = 0;
	bool threadAcquired = false;
	enum CMDQ_HW_THREAD_PRIORITY_ENUM thread_prio = CMDQ_THR_PRIO_NORMAL;
	CMDQ_TIME consumeTime;
	int32_t waitingTimeMS;
	bool needLog = false;
	bool dumpTriggerLoop = false;
	bool timeout_flag = false;
	uint32_t disp_list_count = 0;
	uint32_t user_list_count = 0;
	uint32_t index = 0;

	/* when we're suspending, do not execute */
	/* any tasks. delay & hold them. */
	if (gCmdqSuspended)
		return status;

	CMDQ_PROF_START(current->pid, __func__);
	CMDQ_PROF_MMP(cmdq_mmp_get_event()->consume_done,
		MMPROFILE_FLAG_START,
		current->pid, 0);
	consumeTime = sched_clock();

	mutex_lock(&gCmdqTaskMutex);

	threadAcquired = false;

	/* scan and remove (if executed) waiting tasks */
	list_for_each_safe(p, n, &gCmdqContext.taskWaitList) {
		pTask = list_entry(p, struct TaskStruct, listEntry);

		thread_prio = cmdq_get_func()->priority(pTask->scenario);

		CMDQ_MSG("-->THREAD:try acquire thr for task:0x%p, prio: %d\n",
			pTask,
			thread_prio);
		CMDQ_MSG("-->THREAD:prio:%d, flag:0x%llx, scenario:%d begin\n",
			pTask->priority,
			pTask->engineFlag,
			pTask->scenario);

		CMDQ_GET_TIME_IN_MS(pTask->submit, consumeTime, waitingTimeMS);
		timeout_flag = waitingTimeMS >= CMDQ_PREDUMP_TIMEOUT_MS;
		needLog = timeout_flag;

		if (timeout_flag == true) {
			if (cmdq_get_func()->isDispScenario(
					pTask->scenario) == true) {
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
		thread = cmdq_core_acquire_thread(pTask->engineFlag,
			thread_prio, pTask->scenario, needLog,
			pTask->secData.is_secure);

		if (thread == CMDQ_INVALID_THREAD) {
			/* have to wait, remain in wait list */
			CMDQ_MSG("<--THREAD: acquire thread fail,wait\n");
			if (needLog == true) {
				/* task wait too long */
				CMDQ_ERR("acquire thread fail, task(0x%p)\n",
					pTask);

				CMDQ_ERR("thread_prio(%d), flag(0x%llx)\n",
					thread_prio,
					pTask->engineFlag);

				dumpTriggerLoop =
				    (pTask->scenario ==
					CMDQ_SCENARIO_PRIMARY_DISP) ?
				    (true) : (dumpTriggerLoop);
			}
			continue;
		}

		pThread = &gCmdqContext.thread[thread];

		/* some task is ready to run */
		threadAcquired = true;

		/* Assign loop function if the thread should be a loop thread */
		pThread->loopCallback = pTask->loopCallback;
		pThread->loopData = pTask->loopData;

		/* Start execution, */
		/* remove from wait list and put into active list */
		list_del_init(&(pTask->listEntry));
		list_add_tail(&(pTask->listEntry),
			&gCmdqContext.taskActiveList);

		CMDQ_MSG("acquire thread w/flag: 0x%llx on thr(%d):0x%p end\n",
			 pTask->engineFlag, thread, pThread);

		/* callback task for tracked group */
		for (index = 0; index < CMDQ_MAX_GROUP_COUNT; ++index) {
			if (gCmdqGroupCallback[index].trackTask) {
				CMDQ_MSG("task group %d with task: %p\n",
					index, pTask);
				if (cmdq_core_is_group_flag(
					(enum CMDQ_GROUP_ENUM) index,
					pTask->engineFlag)) {
					CMDQ_MSG("task group %d flag=0x%llx\n",
						index,
						pTask->engineFlag);
					gCmdqGroupCallback[index].trackTask(
						pTask);
				}
			}
		}

		/* Run task on thread */
		status = cmdq_core_exec_task_async_with_retry(pTask, thread);
		if (status < 0) {
			CMDQ_ERR
			    ("<--THREAD: exec_async fail, release task 0x%p\n",
			     pTask);
			cmdq_core_track_task_record(pTask, thread);
			cmdq_core_release_thread(pTask);
			cmdq_core_release_task_unlocked(pTask);
			pTask = NULL;
		}
	}

	if ((disp_list_count > 0) && (disp_list_count >= user_list_count)) {
		/* print error message */
		CMDQ_ERR("Too many DISP (%d) tasks cannot acquire thread\n",
			disp_list_count);
	} else if ((user_list_count > 0) &&
		(user_list_count >= disp_list_count)) {
		/* print error message */
		CMDQ_ERR("Too many userspace(%d)tasks cannot acquire thread\n",
			user_list_count);
	}

	if (dumpTriggerLoop) {
		/* HACK: observe trigger loop */
		/* status when acquire config thread failed. */
		int32_t dumpThread =
			cmdq_get_func()->dispThread(CMDQ_SCENARIO_PRIMARY_DISP);

		cmdq_core_dump_disp_trigger_loop_mini("ACQUIRE");
		cmdq_core_dump_thread_pc(dumpThread);
	}

	if (threadAcquired) {
		/* notify some task's SW thread */
		/* to change their waiting state. */
		/* (if they already called */
		/* cmdqCoreWaitResultAndReleaseTask()) */
		wake_up_all(&gCmdqThreadDispatchQueue);
	}

	mutex_unlock(&gCmdqTaskMutex);

	CMDQ_PROF_END(current->pid, __func__);

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->consume_done,
		MMPROFILE_FLAG_END, current->pid, 0);

	return status;
}

static void cmdqCoreConsumeWaitQueueItem(struct work_struct *_ignore)
{
	int32_t status;

	status = cmdq_core_consume_waiting_list(_ignore);
}

int32_t cmdqCoreSubmitTaskAsyncImpl(struct cmdqCommandStruct *pCommandDesc,
				    CmdqInterruptCB loopCB,
				    unsigned long loopData,
				    struct TaskStruct **ppTaskOut)
{
	struct TaskStruct *pTask = NULL;
	int32_t status = 0;

	if (pCommandDesc->scenario != CMDQ_SCENARIO_TRIGGER_LOOP)
		cmdq_core_verfiy_command_desc_end(pCommandDesc);

	CMDQ_MSG("-->SUBMIT_ASYNC: cmd 0x%p begin\n",
		CMDQ_U32_PTR(pCommandDesc->pVABase));
	CMDQ_PROF_START(current->pid, __func__);

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->alloc_task,
		MMPROFILE_FLAG_START, current->pid, 0);

	/* Allocate Task. This creates a new task */
	/* and put into tail of waiting list */
	pTask = cmdq_core_acquire_task(pCommandDesc, loopCB, loopData);

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->alloc_task,
		MMPROFILE_FLAG_END, current->pid, 0);

	if (pTask == NULL) {
		CMDQ_PROF_END(current->pid, __func__);
		return -EFAULT;
	}

	if (ppTaskOut != NULL)
		*ppTaskOut = pTask;

	/* Try to lock resource base on engine flag */
	cmdqCoreLockResource(pTask->engineFlag, false);

	/* consume the waiting list. */
	/* this may or may not execute the task, */
	/* depending on available threads. */
	status = cmdq_core_consume_waiting_list(NULL);

	CMDQ_MSG("<--SUBMIT_ASYNC: task: 0x%p end\n",
		CMDQ_U32_PTR(pCommandDesc->pVABase));
	CMDQ_PROF_END(current->pid, __func__);
	return status;
}

int32_t cmdqCoreSubmitTaskAsync(struct cmdqCommandStruct *pCommandDesc,
				CmdqInterruptCB loopCB,
				unsigned long loopData,
				struct TaskStruct **ppTaskOut)
{
	int32_t status = 0;
	struct TaskStruct *pTask = NULL;

	if (pCommandDesc->secData.is_secure == true) {
		status = cmdq_core_start_secure_path_notify_thread();
		if (status < 0)
			return status;
	}

	status = cmdqCoreSubmitTaskAsyncImpl(pCommandDesc,
		loopCB, loopData, &pTask);
	if (ppTaskOut != NULL)
		*ppTaskOut = pTask;

	return status;
}

int32_t cmdqCoreReleaseTask(struct TaskStruct *pTask)
{
	unsigned long flags;
	int32_t status = 0;
	int32_t thread = pTask->thread;
	struct ThreadStruct *pThread = NULL;

	CMDQ_MSG("<--TASK: %s 0x%p\n", __func__, pTask);

	if (thread == CMDQ_INVALID_THREAD) {
		CMDQ_ERR("%s, thread is invalid (%d)\n", __func__, thread);
		return -EFAULT;
	}

	pThread = &(gCmdqContext.thread[thread]);

	if (pThread != NULL) {
		/* this task is being executed (or queueed) on a HW thread */

		/* get SW lock first to ensure atomic access HW */
		spin_lock_irqsave(&gCmdqExecLock, flags);
		/* make sure instructions are really in DRAM */
		smp_mb();

		if (pThread->loopCallback) {
			/* a loop thread has only 1 task involved */
			/* so we can release thread directly */
			/* otherwise we need to connect remaining tasks */
			if (pThread->taskCount > 1)
				CMDQ_AEE("CMDQ",
					"task count more than 1:%u\n",
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
			/* TODO: we should check thread */
			/* enabled or not before resume it. */
			status = cmdq_core_force_remove_task_from_thread(pTask,
				thread);
			if (pThread->taskCount > 0)
				cmdq_core_resume_HW_thread(thread);
		}

		spin_unlock_irqrestore(&gCmdqExecLock, flags);
		wake_up(&gCmdWaitQueue[thread]);
	}

	cmdq_core_track_task_record(pTask, thread);
	cmdq_core_release_thread(pTask);
	cmdq_core_auto_release_task(pTask);
	CMDQ_MSG("-->TASK: %s 0x%p end\n", __func__, pTask);
	return 0;
}

int32_t cmdqCoreWaitAndReleaseTask(struct TaskStruct *pTask,
	long timeout_jiffies)
{
	return cmdqCoreWaitResultAndReleaseTask(pTask, NULL, timeout_jiffies);
}

int32_t cmdqCoreWaitResultAndReleaseTask(struct TaskStruct *pTask,
	struct cmdqRegValueStruct *pResult,
	long timeout_jiffies)
{
	int32_t status;
	int32_t thread;
	int i;

	if (pTask == NULL) {
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
	status = cmdq_core_wait_task_done(pTask, timeout_jiffies);

	/*  */
	/* retrieve result */
	if (pResult && pResult->count &&
		pResult->count <= CMDQ_MAX_DUMP_REG_COUNT) {
		/* clear results */
		memset(CMDQ_U32_PTR(pResult->regValues), 0,
		       pResult->count *
		       sizeof(CMDQ_U32_PTR(pResult->regValues)[0]));

		mutex_lock(&gCmdqTaskMutex);
		for (i = 0; i < pResult->count && i < pTask->regCount; ++i) {
			/* fill results */
			CMDQ_U32_PTR(pResult->regValues)[i] =
				pTask->regResults[i];
		}
		mutex_unlock(&gCmdqTaskMutex);
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
	int32_t status = 0;
	struct TaskStruct *pTask = NULL;
	CmdqAsyncFlushCB finishCallback = NULL;
	uint32_t userData = 0;
	uint32_t *pCmd = NULL;
	int32_t commandSize = 0;
	struct CmdBufferStruct *cmd_buffer = NULL;
	uint32_t *copy_ptr = NULL;

	pTask = container_of(workItem, struct TaskStruct, autoReleaseWork);

	if (pTask) {
		finishCallback = pTask->flushCallback;
		userData = pTask->flushData;
		commandSize = pTask->commandSize;
		pCmd = kzalloc(commandSize, GFP_KERNEL);
		if (pCmd == NULL) {
			/* allocate command backup buffer */
			/* failed wil make dump incomplete */
			CMDQ_ERR("failed to alloc command buffer, size: %d\n",
				commandSize);
		} else {
			copy_ptr = pCmd;
			list_for_each_entry(cmd_buffer,
				&pTask->cmd_buffer_list,
				listEntry) {
				memcpy(copy_ptr, cmd_buffer->pVABase,
					commandSize > CMDQ_CMD_BUFFER_SIZE ?
					CMDQ_CMD_BUFFER_SIZE :
					commandSize);
				commandSize -= CMDQ_CMD_BUFFER_SIZE;
				copy_ptr +=
					(CMDQ_CMD_BUFFER_SIZE /
						sizeof(uint32_t));
			}
			commandSize = pTask->commandSize;
		}

		status = cmdqCoreWaitResultAndReleaseTask(pTask,
					NULL,
					msecs_to_jiffies
					(CMDQ_DEFAULT_TIMEOUT_MS));

		CMDQ_VERBOSE("[Auto Release] released pTask=%p, status=%d\n",
			pTask, status);
		CMDQ_PROF_MMP(cmdq_mmp_get_event()->autoRelease_done,
			      MMPROFILE_FLAG_PULSE,
			      ((unsigned long)pTask),
			      current->pid);

		/* Notify user */
		if (finishCallback) {
			CMDQ_VERBOSE("[Auto Release]callback %p data 0x%08x\n",
				     finishCallback, userData);
			if (finishCallback(userData) < 0) {
				CMDQ_LOG
				    ("[DEBUG]user ret abnormal, dump cmd\n");
				CMDQ_LOG("======TASK 0x%p command(%d) START\n",
					pTask,
					commandSize);
				if (pCmd != NULL)
					cmdqCoreDumpCommandMem(pCmd,
						commandSize);
				CMDQ_LOG("======TASK 0x%p command END\n",
					pTask);
			}
		}

		kfree(pCmd);
		pCmd = NULL;

		pTask = NULL;
	}
}

int32_t cmdqCoreAutoReleaseTask(struct TaskStruct *pTask)
{
	int32_t threadNo = CMDQ_INVALID_THREAD;
	bool is_secure;

	if (pTask == NULL) {
		/* Error occurs when Double INIT_WORK */
		CMDQ_ERR("[Double INIT WORK] pTask is NULL");
		return 0;
	}

	if (pTask->pCMDEnd == NULL || list_empty(&pTask->cmd_buffer_list)) {
		/* Error occurs when Double INIT_WORK */
		CMDQ_ERR("[Double INIT WORK] pTask(0x%p) is already released",
			pTask);
		return 0;
	}

	/* the work item is embedded in pTask already */
	/* but we need to initialized it */
	if (unlikely(atomic_inc_return(&pTask->useWorkQueue) != 1)) {
		/* Error occurs when Double INIT_WORK */
		CMDQ_ERR("useWorkQueue is already TRUE, pTask(%p)",
			pTask);
		return -EFAULT;
	}

	/* use work queue to release task */
	INIT_WORK(&pTask->autoReleaseWork, cmdq_core_auto_release_work);

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->autoRelease_add,
		MMPROFILE_FLAG_PULSE,
		((unsigned long)pTask), pTask->thread);

	/* Put auto release task to corresponded thread */
	if (pTask->thread != CMDQ_INVALID_THREAD) {
		queue_work(gCmdqContext.taskThreadAutoReleaseWQ[pTask->thread],
			   &pTask->autoReleaseWork);
	} else {
		/* if task does not belong thread, */
		/* use static dispatch thread at first, */
		/* otherwise, use global context workqueue */
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

int32_t cmdqCoreSubmitTask(struct cmdqCommandStruct *pCommandDesc)
{
	int32_t status;
	struct TaskStruct *pTask = NULL;

	CMDQ_MSG("-->SUBMIT: SYNC cmd 0x%p begin\n",
		CMDQ_U32_PTR(pCommandDesc->pVABase));
	status = cmdqCoreSubmitTaskAsync(pCommandDesc, NULL, 0, &pTask);

	if (status >= 0) {
		status = cmdqCoreWaitResultAndReleaseTask(pTask,
			&pCommandDesc->regValue,
			msecs_to_jiffies
			(CMDQ_DEFAULT_TIMEOUT_MS));
		if (status < 0) {
			/* error status print */
			CMDQ_ERR("Task 0x%p wait fails\n", pTask);
		}
	} else {
		CMDQ_ERR("cmdqCoreSubmitTaskAsync failed=%d", status);
	}

	CMDQ_MSG("<--SUBMIT: SYNC cmd 0x%p end\n",
		CMDQ_U32_PTR(pCommandDesc->pVABase));
	return status;
}

int32_t cmdqCoreQueryUsage(int32_t *pCount)
{
	unsigned long flags;
	struct EngineStruct *pEngine;
	int32_t index;

	pEngine = gCmdqContext.engine;

	spin_lock_irqsave(&gCmdqThreadLock, flags);

	for (index = 0; index < CMDQ_MAX_ENGINE_COUNT; index++)
		pCount[index] = pEngine[index].userCount;

	spin_unlock_irqrestore(&gCmdqThreadLock, flags);

	return 0;
}

void cmdq_core_release_task_by_file_node(void *file_node)
{
	struct TaskStruct *pTask = NULL;
	struct list_head *p = NULL;

	/* Since the file node is closed, there is no way */
	/* user space can issue further "wait_and_close" request, */
	/* so we must auto-release running/waiting tasks */
	/* to prevent resource leakage */

	/* walk through active and waiting lists and release them */
	mutex_lock(&gCmdqTaskMutex);

	list_for_each(p, &gCmdqContext.taskActiveList) {
		pTask = list_entry(p, struct TaskStruct, listEntry);
		if (pTask->taskState != TASK_STATE_IDLE &&
			pTask->privateData &&
			CMDQ_TASK_PRIVATE(pTask)->node_private_data
				== file_node &&
			(cmdq_core_is_request_from_user_space(
				pTask->scenario))) {
			CMDQ_LOG("task 0x%p release asnode 0x%p closed\n",
			     pTask, file_node);
			cmdq_core_dump_task(pTask);

			/* since we already inside mutex, */
			/* do not cmdqReleaseTask directly, */
			/* instead we change state to "KILLED" */
			/* and arrange a auto-release. */
			/* Note that these tasks may already issued to HW */
			/* so there is a chance that */
			/* following MPU/M4U violation */
			/* may occur, if the user space */
			/* process has destroyed. */
			/* The ideal solution is to */
			/*stop / cancel HW operation */
			/* immediately, but we cannot do */
			/* so due to SMI hang risk. */
			cmdqCoreAutoReleaseTask(pTask);
		}
	}
	list_for_each(p, &gCmdqContext.taskWaitList) {
		pTask = list_entry(p, struct TaskStruct, listEntry);
		if (pTask->taskState == TASK_STATE_WAITING &&
			pTask->privateData &&
			CMDQ_TASK_PRIVATE(pTask)->node_private_data
				== file_node &&
			(cmdq_core_is_request_from_user_space(
				pTask->scenario))) {

			CMDQ_LOG("task 0x%p release as node 0x%p closed\n",
			     pTask, file_node);
			cmdq_core_dump_task(pTask);

			/* since we already inside mutex, */
			/* and these WAITING tasks will not be */
			/* consumed (acquire thread / exec) */
			/* we can release them directly. */
			/* note that we use unlocked version since */
			/* we already hold gCmdqTaskMutex. */
			cmdq_core_release_task_unlocked(pTask);
		}
	}

	mutex_unlock(&gCmdqTaskMutex);
}

unsigned long long cmdq_core_get_GPR64(const enum CMDQ_DATA_REGISTER_ENUM regID)
{
#ifdef CMDQ_GPR_SUPPORT
	unsigned long long value;
	unsigned long long value1;
	unsigned long long value2;
	const uint32_t x = regID & 0x0F;

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
	CMDQ_VERBOSE("get_GPR64(%x): 0x%llx(0x%llx, 0x%llx)\n",
		regID, value, value2, value1);

	return value;

#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return 0LL;
#endif
}

void cmdq_core_set_GPR64(const enum CMDQ_DATA_REGISTER_ENUM regID,
	const unsigned long long value)
{
#ifdef CMDQ_GPR_SUPPORT

	const unsigned long long value1 = 0x00000000FFFFFFFF & value;
	const unsigned long long value2 = 0LL | value >> 32;
	const uint32_t x = regID & 0x0F;
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
		CMDQ_ERR("set_GPR64(%x) failed, value 0x%llx, not 0x%llx\n",
			regID,
			result,
			value);
	}
#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
#endif
}

uint32_t cmdqCoreReadDataRegister(enum CMDQ_DATA_REGISTER_ENUM regID)
{
#ifdef CMDQ_GPR_SUPPORT
	return CMDQ_REG_GET32(CMDQ_GPR_R32(regID));
#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return 0;
#endif
}

uint32_t cmdq_core_thread_prefetch_size(const int32_t thread)
{
	if (thread >= 0 && thread < CMDQ_MAX_THREAD_COUNT)
		return g_dts_setting.prefetch_size[thread];
	else
		return 0;
}

void cmdq_core_dump_dts_setting(void)
{
	uint32_t index;
	struct ResourceUnitStruct *pResource = NULL;
	struct list_head *p = NULL;

	CMDQ_LOG("[DTS] Prefetch Thread Count:%d\n",
		g_dts_setting.prefetch_thread_count);
	CMDQ_LOG("[DTS] Prefetch Size of Thread:\n");
	for (index = 0;
		index < g_dts_setting.prefetch_thread_count &&
		index < CMDQ_MAX_THREAD_COUNT;
		index++)
		CMDQ_LOG("	Thread[%d]=%d\n",
			index,
			g_dts_setting.prefetch_size[index]);
	CMDQ_LOG("[DTS] SRAM Sharing Config:\n");
	list_for_each(p, &gCmdqContext.resourceList) {
		pResource = list_entry(p, struct ResourceUnitStruct, listEntry);
		CMDQ_LOG("	Engine=0x%016llx,, event=%d\n",
			pResource->engine,
			pResource->lockEvent);
	}
}

int32_t cmdq_core_get_running_task_by_engine_unlock(uint64_t engineFlag,
	uint32_t userDebugStrLen, struct TaskStruct *p_out_task)
{
	struct EngineStruct *pEngine;
	struct ThreadStruct *pThread;
	int32_t index;
	int32_t thread = CMDQ_INVALID_THREAD;
	int32_t status = -EFAULT;
	struct TaskStruct *pTargetTask = NULL;

	if (p_out_task == NULL)
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
		uint32_t insts[4];
		uint32_t currPC = CMDQ_AREG_TO_PHYS(
			CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread)));

		currPC = CMDQ_AREG_TO_PHYS(
			CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread)));
		for (index = 0;
			index < cmdq_core_max_task_in_thread(thread);
			index++) {
			pTask = pThread[thread].pCurTask[index];
			if (pTask == NULL ||
				list_empty(&pTask->cmd_buffer_list))
				continue;
			if (cmdq_core_get_pc(pTask, thread, insts)) {
				pTargetTask = pTask;
				break;
			}
		}
		if (!pTargetTask) {
			uint32_t currPC = CMDQ_AREG_TO_PHYS(
				CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread)));

			CMDQ_LOG("cannot find pc (0x%08x) at thread (%d)\n",
				currPC, thread);
			cmdq_core_dump_task_in_thread(thread,
				false, true, false);
		}
	}

	if (pTargetTask) {
		uint32_t current_debug_str_len = pTargetTask->userDebugStr ?
			(uint32_t)strlen(pTargetTask->userDebugStr) : 0;
		uint32_t debug_str_len =
			userDebugStrLen < current_debug_str_len ?
				userDebugStrLen :
				current_debug_str_len;
		char *debug_str_buffer = p_out_task->userDebugStr;

		/* copy content except pointers */
		memcpy(p_out_task, pTargetTask, sizeof(struct TaskStruct));
		p_out_task->pCMDEnd = NULL;
		p_out_task->regResults = NULL;
		p_out_task->secStatus = NULL;
		p_out_task->profileData = NULL;

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

int32_t cmdq_core_get_running_task_by_engine(uint64_t engineFlag,
	uint32_t userDebugStrLen, struct TaskStruct *p_out_task)
{
	int32_t result = 0;
	unsigned long flags = 0;

	/* make sure context does not change during get and copy */
	spin_lock_irqsave(&gCmdqExecLock, flags);
	result = cmdq_core_get_running_task_by_engine_unlock(
		engineFlag, userDebugStrLen, p_out_task);
	spin_unlock_irqrestore(&gCmdqExecLock, flags);

	return result;
}

int32_t cmdqCoreInitialize(void)
{
	struct TaskStruct *pTask;
	int32_t index;
	uint32_t last_size_ragne = 1 * 32;

	atomic_set(&gCmdqThreadUsage, 0);
	atomic_set(&gSMIThreadUsage, 0);

	for (index = 0; index < CMDQ_MAX_THREAD_COUNT; index++)
		init_waitqueue_head(&gCmdWaitQueue[index]);

	init_waitqueue_head(&gCmdqThreadDispatchQueue);

	/* Reset overall context */
	memset(&gCmdqContext, 0x0, sizeof(struct ContextStruct));
	/* some fields has non-zero initial value */
	cmdq_core_reset_engine_struct();
	cmdq_core_reset_thread_struct();

	/* Create task pool */
	gCmdqContext.taskCache = kmem_cache_create(
		CMDQ_DRIVER_DEVICE_NAME "_task",
		sizeof(struct TaskStruct),
		__alignof__(struct TaskStruct),
		SLAB_POISON | SLAB_HWCACHE_ALIGN | SLAB_RED_ZONE,
		&cmdq_core_task_ctor);
	/* Initialize task lists */
	INIT_LIST_HEAD(&gCmdqContext.taskFreeList);
	INIT_LIST_HEAD(&gCmdqContext.taskActiveList);
	INIT_LIST_HEAD(&gCmdqContext.taskWaitList);
	INIT_LIST_HEAD(&gCmdqContext.resourceList);
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
			mutex_lock(&gCmdqTaskMutex);
			list_add_tail(&(pTask->listEntry),
				&gCmdqContext.taskFreeList);
			mutex_unlock(&gCmdqTaskMutex);
		}
	}

	/* allocate shared memory */
	gCmdqContext.hSecSharedMem = NULL;
#ifdef CMDQ_SECURE_PATH_SUPPORT
	cmdq_sec_create_shared_memory(&(gCmdqContext.hSecSharedMem), PAGE_SIZE);
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
	/* Initialize DTS Setting structure */
	memset(&g_dts_setting, 0x0, sizeof(struct cmdq_dts_setting));
	/* Initialize setting for legacy chip */
	g_dts_setting.prefetch_thread_count = 3;
	g_dts_setting.prefetch_size[0] = 240;
	g_dts_setting.prefetch_size[2] = 32;
	cmdq_dev_get_dts_setting(&g_dts_setting);
	/* Initialize Resource via device tree */
	cmdq_dev_init_resource(cmdq_core_init_resource);

	/* Initialize Features */
	gCmdqContext.features[CMDQ_FEATURE_SRAM_SHARE] = 1;

	/* MDP initialization setting */
	cmdq_mdp_get_func()->mdpInitialSet();

	g_cmdq_consume_again = false;

	atomic_set(&g_cmdq_mem_monitor.monitor_mem_enable, 0);

	for (index = 0; index < ARRAY_SIZE(g_cmdq_mem_records) - 1; index++) {
		last_size_ragne *= 2;
		g_cmdq_mem_records[index].alloc_range = last_size_ragne;
	}
	/* always assign last one as large buffer size */
	g_cmdq_mem_records[
		ARRAY_SIZE(g_cmdq_mem_records)-1].alloc_range = 256 * 1024;

	return 0;
}

#ifdef CMDQ_SECURE_PATH_SUPPORT
int32_t cmdqCoreLateInitialize(void)
{
	int32_t status = 0;
	struct task_struct *open_th =
#ifndef CONFIG_MTK_CMDQ_TAB
	kthread_run(cmdq_sec_init_allocate_resource_thread,
		NULL,
		"cmdq_WSM_init");
#else
	kthread_run(cmdq_sec_init_secure_path, NULL, "cmdq_WSM_init");
#endif
	if (IS_ERR(open_th)) {
		CMDQ_LOG("%s, init kthread_run failed!\n", __func__);
		status = -EFAULT;
	}
	return status;
}
#endif

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

	/* directly destroy the auto release */
	/* WQ since we're going to release tasks anyway. */
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
			mutex_lock(&gCmdqTaskMutex);

			pTask = list_entry(p, struct TaskStruct, listEntry);

			/* free allocated DMA buffer */
			cmdq_task_free_task_command_buffer(pTask);
			kmem_cache_free(gCmdqContext.taskCache, pTask);
			list_del(p);

			mutex_unlock(&gCmdqTaskMutex);
		}
	}

	/* check if there are dangling write addresses. */
	if (!list_empty(&gCmdqContext.writeAddrList)) {
		/* there are unreleased write buffer, raise AEE */
		CMDQ_AEE("CMDQ", "there are unreleased write buffer");
	}

	kmem_cache_destroy(gCmdqContext.taskCache);
	gCmdqContext.taskCache = NULL;

	/* Deinitialize secure path context */
	cmdqSecDeInitialize();
}

int cmdqCoreAllocWriteAddress(uint32_t count, dma_addr_t *paStart)
{
	unsigned long flagsWriteAddr = 0L;
	struct WriteAddrStruct *pWriteAddr = NULL;
	int status = 0;

	CMDQ_VERBOSE("ALLOC: line %d\n", __LINE__);

	do {
		if (paStart == NULL) {
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
		if (pWriteAddr == NULL) {
			CMDQ_ERR("failed to alloc WriteAddrStruct\n");
			status = -ENOMEM;
			break;
		}
		memset(pWriteAddr, 0, sizeof(struct WriteAddrStruct));

		CMDQ_VERBOSE("ALLOC: line %d\n", __LINE__);

		pWriteAddr->count = count;
		pWriteAddr->va = cmdq_core_alloc_hw_buffer(cmdq_dev_get(),
			count * sizeof(uint32_t),
			&(pWriteAddr->pa),
			GFP_KERNEL);
		if (current)
			pWriteAddr->user = current->pid;

		CMDQ_VERBOSE("ALLOC: line %d\n", __LINE__);

		if (pWriteAddr->va == NULL) {
			CMDQ_ERR("failed to alloc write buffer\n");
			status = -ENOMEM;
			break;
		}

		CMDQ_VERBOSE("ALLOC: line %d\n", __LINE__);

		/* clear buffer content */
		do {
			uint32_t *pInt = (uint32_t *) pWriteAddr->va;
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

		spin_lock_irqsave(&gCmdqWriteAddrLock, flagsWriteAddr);
		list_add_tail(&(pWriteAddr->list_node),
			&gCmdqContext.writeAddrList);
		spin_unlock_irqrestore(&gCmdqWriteAddrLock, flagsWriteAddr);

		status = 0;

	} while (0);

	if (status != 0) {
		/* release resources */
		if (pWriteAddr && pWriteAddr->va) {
			cmdq_core_free_hw_buffer(cmdq_dev_get(),
				sizeof(uint32_t) * pWriteAddr->count,
				pWriteAddr->va, pWriteAddr->pa);
			memset(pWriteAddr, 0, sizeof(struct WriteAddrStruct));
		}

		kfree(pWriteAddr);
		pWriteAddr = NULL;
	}

	CMDQ_VERBOSE("ALLOC: line %d\n", __LINE__);

	return status;
}

uint32_t cmdqCoreReadWriteAddress(dma_addr_t pa)
{
	struct list_head *p = NULL;
	struct WriteAddrStruct *pWriteAddr = NULL;
	int32_t offset = 0;
	uint32_t value = 0;
	unsigned long flagsWriteAddr = 0L;

	/* search for the entry */
	spin_lock_irqsave(&gCmdqWriteAddrLock, flagsWriteAddr);
	list_for_each(p, &gCmdqContext.writeAddrList) {
		pWriteAddr = list_entry(p, struct WriteAddrStruct, list_node);
		if (pWriteAddr == NULL)
			continue;

		offset = pa - pWriteAddr->pa;

		if (offset >= 0 &&
			(offset / sizeof(uint32_t)) < pWriteAddr->count) {
			CMDQ_VERBOSE
			("%s() input:%p, offset=%d va=%p pa_start=%pa\n",
			    __func__,
			    &pa, offset,
			    (pWriteAddr->va + offset),
			    &(pWriteAddr->pa));
			value = *((uint32_t *)(pWriteAddr->va + offset));
			CMDQ_VERBOSE
			    ("%s() found offset=%d va=%p value=0x%08x\n",
			    __func__,
			     offset, (pWriteAddr->va + offset), value);
			break;
		}
	}
	spin_unlock_irqrestore(&gCmdqWriteAddrLock, flagsWriteAddr);

	return value;
}

uint32_t cmdqCoreWriteWriteAddress(dma_addr_t pa, uint32_t value)
{
	struct list_head *p = NULL;
	struct WriteAddrStruct *pWriteAddr = NULL;
	int32_t offset = 0;
	unsigned long flagsWriteAddr = 0L;
	char longMsg[CMDQ_LONGSTRING_MAX];
	uint32_t msgOffset;
	int32_t msgMAXSize;

	/* search for the entry */
	spin_lock_irqsave(&gCmdqWriteAddrLock, flagsWriteAddr);
	list_for_each(p, &gCmdqContext.writeAddrList) {
		pWriteAddr = list_entry(p, struct WriteAddrStruct, list_node);
		if (pWriteAddr == NULL)
			continue;

		offset = pa - pWriteAddr->pa;

		if (offset >= 0 &&
			(offset / sizeof(uint32_t)) < pWriteAddr->count) {
			cmdq_core_longstring_init(longMsg,
				&msgOffset, &msgMAXSize);
			cmdqCoreLongString(false, longMsg,
				&msgOffset, &msgMAXSize,
				"%s() input:0x%pa,",
				__func__, &pa);
			cmdqCoreLongString(false, longMsg,
				&msgOffset, &msgMAXSize,
				" got offset=%d va=%p pa_start=0x%pa, value=0x%08x\n",
				offset, (pWriteAddr->va + offset),
				&pWriteAddr->pa, value);
			if (msgOffset > 0) {
				/* print message */
				CMDQ_VERBOSE("%s", longMsg);
			}

			*((uint32_t *)(pWriteAddr->va + offset)) = value;
			break;
		}
	}
	spin_unlock_irqrestore(&gCmdqWriteAddrLock, flagsWriteAddr);

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
	spin_lock_irqsave(&gCmdqWriteAddrLock, flagsWriteAddr);
	list_for_each_safe(p, n, &gCmdqContext.writeAddrList) {
		pWriteAddr = list_entry(p, struct WriteAddrStruct, list_node);
		if (pWriteAddr && pWriteAddr->pa == paStart) {
			list_del(&(pWriteAddr->list_node));
			foundEntry = true;
			break;
		}
	}
	spin_unlock_irqrestore(&gCmdqWriteAddrLock, flagsWriteAddr);

	/* when list is not empty, we always get a entry */
	/* even we don't found a valid entry */
	/* use foundEntry to confirm search result */
	if (foundEntry == false) {
		CMDQ_ERR("%s() no matching entry, paStart:%pa\n",
			__func__, &paStart);
		return -EINVAL;
	}

	/* release resources */
	if (pWriteAddr->va) {
		cmdq_core_free_hw_buffer(cmdq_dev_get(),
					 sizeof(uint32_t) * pWriteAddr->count,
					 pWriteAddr->va, pWriteAddr->pa);
		memset(pWriteAddr, 0xda, sizeof(struct WriteAddrStruct));
	}

	kfree(pWriteAddr);
	pWriteAddr = NULL;

	return 0;
}

int32_t cmdqCoreDebugRegDumpBegin(uint32_t taskID,
	uint32_t *regCount, uint32_t **regAddress)
{
	if (gCmdqDebugCallback.beginDebugRegDump == NULL) {
		CMDQ_ERR("beginDebugRegDump not registered\n");
		return -EFAULT;
	}

	return gCmdqDebugCallback.beginDebugRegDump(taskID,
		regCount, regAddress);
}

int32_t cmdqCoreDebugRegDumpEnd(uint32_t taskID,
	uint32_t regCount, uint32_t *regValues)
{
	if (gCmdqDebugCallback.endDebugRegDump == NULL) {
		CMDQ_ERR("endDebugRegDump not registered\n");
		return -EFAULT;
	}

	return gCmdqDebugCallback.endDebugRegDump(taskID, regCount, regValues);
}

void cmdq_core_set_log_level(const int32_t value)
{
	if (value == CMDQ_LOG_LEVEL_NORMAL) {
		/* Only print CMDQ ERR and CMDQ LOG */
		gCmdqContext.logLevel = CMDQ_LOG_LEVEL_NORMAL;
	} else if (value < CMDQ_LOG_LEVEL_MAX) {
		/* Modify log level */
		gCmdqContext.logLevel = (1 << value);
	}
}

int32_t cmdq_core_get_log_level(void)
{
	return gCmdqContext.logLevel;
}

bool cmdq_core_should_print_msg(void)
{
	bool logLevel = (gCmdqContext.logLevel &
		(1 << CMDQ_LOG_LEVEL_MSG)) ? (1) : (0);
	return logLevel;
}

bool cmdq_core_should_full_error(void)
{
	bool logLevel = (gCmdqContext.logLevel &
		(1 << CMDQ_LOG_LEVEL_FULL_ERROR)) ? (1) : (0);
	return logLevel;
}

int32_t cmdq_core_profile_enabled(void)
{
	return gCmdqContext.enableProfile;
}

void cmdq_core_longstring_init(char *buf, uint32_t *offset, int32_t *maxSize)
{
	buf[0] = '\0';
	*offset = 0;
	*maxSize = CMDQ_LONGSTRING_MAX - 1;
}

void cmdqCoreLongString(bool forceLog,
	char *buf,
	uint32_t *offset,
	int32_t *maxSize,
	const char *string, ...)
{
	int msgLen;
	va_list argptr;
	char *pBuffer;

	if ((forceLog == false) &&
		(cmdq_core_should_print_msg() == false) &&
		(*maxSize <= 0))
		return;

	va_start(argptr, string);
	pBuffer = buf + (*offset);
	msgLen = vsnprintf(pBuffer, *maxSize, string, argptr);
	*maxSize -= msgLen;
	if (*maxSize < 0)
		*maxSize = 0;
	*offset += msgLen;
	va_end(argptr);
}

void cmdq_core_turnon_first_dump(const struct TaskStruct *pTask)
{
#ifdef CMDQ_DUMP_FIRSTERROR
	if (gCmdqFirstError.cmdqCount != 0 || pTask == NULL)
		return;

	gCmdqFirstError.flag = true;
	/* save kernel time, pid, and caller name */
	gCmdqFirstError.callerPid = pTask->callerPid;
	snprintf(gCmdqFirstError.callerName,
		TASK_COMM_LEN,
		"%s",
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

int32_t cmdq_core_save_first_dump(const char *string, ...)
{
#ifdef CMDQ_DUMP_FIRSTERROR
	int logLen;
	va_list argptr;
	char *pBuffer;

	if (gCmdqFirstError.flag == false)
		return -EFAULT;

	va_start(argptr, string);
	pBuffer = gCmdqFirstError.cmdqString + gCmdqFirstError.cmdqCount;
	logLen = vsnprintf(pBuffer,
		gCmdqFirstError.cmdqMaxSize,
		string,
		argptr);
	gCmdqFirstError.cmdqMaxSize -= logLen;
	gCmdqFirstError.cmdqCount += logLen;

	if (gCmdqFirstError.cmdqMaxSize <= 0) {
		gCmdqFirstError.flag = false;
		CMDQ_ERR("[CMDQ][ERR] Error0 dump saving buffer is full\n");
	}
	va_end(argptr);
	return 0;
#else
	return -EFAULT;
#endif
}

#ifdef CMDQ_DUMP_FIRSTERROR
void cmdq_core_hex_dump_to_buffer(const void *buf, size_t len, int rowsize,
	int groupsize,
	char *linebuf,
	size_t linebuflen)
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
					"%s%8.8x", j ? " " : "",
					*(ptr4 + j));
		break;
	}

	case 2:{
		const u16 *ptr2 = buf;
		int ngroups = len / groupsize;

		for (j = 0; j < ngroups; j++)
			lx += scnprintf(linebuf + lx, linebuflen - lx,
					"%s%4.4x",
					j ? " " : "",
					*(ptr2 + j));
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
	int rowsize,
	int groupsize,
	const void *buf,
	size_t len)
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

		cmdq_core_hex_dump_to_buffer(ptr + i,
			linelen, rowsize, groupsize,
			linebuf, sizeof(linebuf));

		pBuffer = gCmdqFirstError.cmdqString +
			gCmdqFirstError.cmdqCount;
		logLen = snprintf(pBuffer,
			gCmdqFirstError.cmdqMaxSize,
			"%s%p: %s\n",
			prefix_str, ptr + i, linebuf);
		gCmdqFirstError.cmdqMaxSize -= logLen;
		gCmdqFirstError.cmdqCount += logLen;

		if (gCmdqFirstError.cmdqMaxSize <= 0) {
			gCmdqFirstError.flag = false;
			CMDQ_ERR("Error0 dump saving buffer is full\n");
		}
	}
#endif
}

/* Use CMDQ as Resource Manager */
void cmdqCoreLockResource(uint64_t engineFlag, bool fromNotify)
{
	struct ResourceUnitStruct *pResource = NULL;
	struct list_head *p = NULL;

	if (cmdq_core_is_feature_off(CMDQ_FEATURE_SRAM_SHARE))
		return;

	list_for_each(p, &gCmdqContext.resourceList) {
		pResource = list_entry(p, struct ResourceUnitStruct, listEntry);
		if (!(engineFlag & pResource->engine))
			continue;

		mutex_lock(&gCmdqResourceMutex);
		/* find matched engine */
		if (fromNotify)
			pResource->notify = sched_clock();
		else
			pResource->lock = sched_clock();

		if (!pResource->used) {
			/* First time used */
			int32_t status;

			CMDQ_MSG("[Res]Lock eng: 0x%016llx, fromNotify:%d\n",
				engineFlag, fromNotify);

			pResource->used = true;
			CMDQ_MSG("[Res] Callback to release\n");
			if (pResource->releaseCB == NULL) {
				CMDQ_LOG("[Res]: release CB NULL, event:%d\n",
					pResource->lockEvent);
			} else {
				CmdqResourceReleaseCB cb_func =
					pResource->releaseCB;

				/* release mutex before callback */
				mutex_unlock(&gCmdqResourceMutex);
				status = cb_func(pResource->lockEvent);
				mutex_lock(&gCmdqResourceMutex);

				if (status < 0) {
					/* Error status print */
					CMDQ_ERR("release CB (%d) fail:%d\n",
						pResource->lockEvent, status);
				}
			}
		} else {
			/* Cancel previous delay task if existed */
			if (pResource->delaying) {
				pResource->delaying = false;
				cancel_delayed_work(
					&pResource->delayCheckWork);
			}
		}
		mutex_unlock(&gCmdqResourceMutex);
	}
}

bool cmdqCoreAcquireResource(enum CMDQ_EVENT_ENUM resourceEvent)
{
	struct ResourceUnitStruct *pResource = NULL;
	struct list_head *p = NULL;
	bool result = false;

	if (cmdq_core_is_feature_off(CMDQ_FEATURE_SRAM_SHARE))
		return result;

	CMDQ_MSG("[Res] Acquire resource with event: %d\n", resourceEvent);
	list_for_each(p, &gCmdqContext.resourceList) {
		pResource = list_entry(p, struct ResourceUnitStruct, listEntry);
		if (resourceEvent == pResource->lockEvent) {
			mutex_lock(&gCmdqResourceMutex);
			/* find matched resource */
			result = !pResource->used;
			if (result) {
				CMDQ_MSG("[Res] Acquire success, event: %d\n",
					resourceEvent);
				cmdqCoreClearEvent(resourceEvent);
				pResource->acquire = sched_clock();
				pResource->lend = true;
			}
			mutex_unlock(&gCmdqResourceMutex);
			break;
		}
	}
	return result;
}

void cmdqCoreReleaseResource(enum CMDQ_EVENT_ENUM resourceEvent)
{
	struct ResourceUnitStruct *pResource = NULL;
	struct list_head *p = NULL;

	if (cmdq_core_is_feature_off(CMDQ_FEATURE_SRAM_SHARE))
		return;

	CMDQ_MSG("[Res] Release resource with event: %d\n", resourceEvent);
	list_for_each(p, &gCmdqContext.resourceList) {
		pResource = list_entry(p, struct ResourceUnitStruct, listEntry);
		if (resourceEvent == pResource->lockEvent) {
			mutex_lock(&gCmdqResourceMutex);
			/* find matched resource */
			pResource->release = sched_clock();
			pResource->lend = false;
			mutex_unlock(&gCmdqResourceMutex);
			break;
		}
	}
}

void cmdqCoreSetResourceCallback(enum CMDQ_EVENT_ENUM resourceEvent,
	CmdqResourceAvailableCB resourceAvailable,
	CmdqResourceReleaseCB resourceRelease)
{
	struct ResourceUnitStruct *pResource = NULL;
	struct list_head *p = NULL;

	CMDQ_MSG("[Res] Set resource callback with event: %d\n", resourceEvent);
	list_for_each(p, &gCmdqContext.resourceList) {
		pResource = list_entry(p, struct ResourceUnitStruct, listEntry);
		if (resourceEvent == pResource->lockEvent) {
			CMDQ_MSG("[Res] Set resource callback ok!\n");
			mutex_lock(&gCmdqResourceMutex);
			/* find matched resource */
			pResource->availableCB = resourceAvailable;
			pResource->releaseCB = resourceRelease;
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
		uint32_t value)
{
	if (atomic_read(&gCmdqThreadUsage) == 0)
		CMDQ_ERR("[FO] Try to set feature (%d) while running!\n",
			featureOption);

	if (featureOption >= CMDQ_FEATURE_TYPE_MAX) {
		CMDQ_ERR("[FO] Set feature invalid: %d\n", featureOption);
	} else {
		CMDQ_LOG("[FO] Set feature: %d, with value:%d\n",
			featureOption, value);
		gCmdqContext.features[featureOption] = value;
	}
}

uint32_t cmdq_core_get_feature(enum CMDQ_FEATURE_TYPE_ENUM featureOption)
{
	if (featureOption >= CMDQ_FEATURE_TYPE_MAX) {
		CMDQ_ERR("[FO] Set feature invalid: %d\n", featureOption);
		return CMDQ_FEATURE_OFF_VALUE;
	}
	return gCmdqContext.features[featureOption];
}

bool cmdq_core_is_feature_off(enum CMDQ_FEATURE_TYPE_ENUM featureOption)
{
	return cmdq_core_get_feature(featureOption) == CMDQ_FEATURE_OFF_VALUE;
}

struct ContextStruct *cmdq_core_get_cmdqcontext(void)
{
	return &gCmdqContext;
}

struct StressContextStruct *cmdq_core_get_stress_context(void)
{
	return &gStressContext;
}

void cmdq_core_clean_stress_context(void)
{
	memset(&gStressContext, 0, sizeof(gStressContext));
}

