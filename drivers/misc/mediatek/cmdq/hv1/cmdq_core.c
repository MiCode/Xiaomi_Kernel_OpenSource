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
/* #include <mach/mt_irq.h> */
#include <linux/memory.h>
#include <linux/ftrace.h>
#ifdef CMDQ_MET_READY
#include <linux/met_drv.h>
#endif
#include <linux/seq_file.h>
#include "cmdq_core.h"
#include "cmdq_reg.h"
#include "cmdq_struct.h"
#include "cmdq_mmp.h"
#include "cmdq_mutex.h"
#include "cmdq_device.h"
#include "cmdq_platform.h"
#include "cmdq_mdp.h"

#include <linux/kthread.h>
#include <linux/delay.h>


#ifdef CMDQ_SECURE_PATH_SUPPORT
#include "cmdq_sec.h"
/* for secure path */
#include "trustzone/kree/mem.h"
#endif

#ifdef CMDQ_CONFIG_SMI
#include "smi_debug.h"
#endif
/* #include "ddp_hal.h" */
/* #include "ddp_reg.h" */

#define CMDQ_GET_COOKIE_CNT(thread) (CMDQ_REG_GET32(CMDQ_THR_EXEC_CNT(thread)) & CMDQ_MAX_COOKIE_VALUE)



/* CCF */
#ifdef CMDQ_OF_SUPPORT
#include <linux/clk.h>
#include <linux/clk-provider.h>
#endif




/* use mutex because we don't access task list in IRQ */
/* and we may allocate memory when create list items */
static DEFINE_MUTEX(gCmdqTaskMutex);
static DEFINE_MUTEX(gCmdqWriteAddrMutex);
/* ensure atomic start/stop notify loop*/
static DEFINE_MUTEX(gCmdqNotifyLoopMutex);
/* t-base(secure OS) doesn't allow entry secure world in ISR context, */
/* but M4U has to restore lab0 register in secure world when enable lab 0 first time. */
/* HACK: use m4u_larb0_enable() to lab0 on/off to ensure larb0 restore and clock on/off sequence */
/* HACK: use gCmdqClockMutex to ensure acquire/release thread and enable/disable clock sequence */
static DEFINE_MUTEX(gCmdqClockMutex);

/* These may access in IRQ so use spin lock. */
static DEFINE_SPINLOCK(gCmdqThreadLock);
static atomic_t gCmdqThreadUsage;
static atomic_t gSMIThreadUsage;
static bool gCmdqSuspended;
static atomic_t gCmdqEarlySuspended;
static DEFINE_SPINLOCK(gCmdqExecLock);
static DEFINE_SPINLOCK(gCmdqRecordLock);

/* Emergency buffer when fail to allocate memory. */
static DEFINE_SPINLOCK(gCmdqAllocLock);
static struct EmergencyBufferStruct gCmdqEmergencyBuffer[CMDQ_EMERGENCY_BLOCK_COUNT];

/* The main context structure */
static wait_queue_head_t gCmdWaitQueue[CMDQ_MAX_THREAD_COUNT];	/* task done notification */
static wait_queue_head_t gCmdqThreadDispatchQueue;	/* thread acquire notification */

struct ContextStruct gCmdqContext;
static struct CmdqCBkStruct gCmdqGroupCallback[CMDQ_MAX_GROUP_COUNT];
static struct CmdqDebugCBkStruct gCmdqDebugCallback;
static uint32_t gSecPrintCount;

/* Debug or test usage */
/* enable this option only when test emergency buffer*/
/* #define CMDQ_TEST_EMERGENCY_BUFFER */
#ifdef CMDQ_TEST_EMERGENCY_BUFFER
static atomic_t gCmdqDebugForceUseEmergencyBuffer = ATOMIC_INIT(0);
#endif
static const uint64_t gCmdqEngineGroupBits[CMDQ_MAX_GROUP_COUNT] = { CMDQ_ENG_ISP_GROUP_BITS,
	CMDQ_ENG_MDP_GROUP_BITS,
	CMDQ_ENG_DISP_GROUP_BITS,
	CMDQ_ENG_JPEG_GROUP_BITS,
	CMDQ_ENG_VENC_GROUP_BITS
};

static void cmdqCoreHandleError(int32_t thread, int32_t value, CMDQ_TIME *pGotIRQ);
static void cmdqCoreHandleDone(int32_t thread, int32_t value, CMDQ_TIME *pGotIRQ);
static void cmdq_core_consume_waiting_list(struct work_struct *);
static void cmdq_core_handle_secure_thread_done_impl(const int32_t thread, const int32_t value,
						     CMDQ_TIME *pGotIRQ);
static uint32_t *cmdq_core_get_pc(const struct TaskStruct *pTask, uint32_t thread,
				  uint32_t insts[4]);

/* CCF */
#ifdef CMDQ_OF_SUPPORT
struct clk *cmdq_clk_map[CMDQ_CLK_NUM];
/*
static struct clk *sys_vde;
static struct clk *sys_isp;
static struct clk *sys_dis;
*/
#endif

static uint16_t gTaskCount[CMDQ_MAX_THREAD_COUNT];
static uint32_t gTaskBuffer[CMDQ_MAX_THREAD_COUNT];
static uint32_t gNextCookieOccupied[CMDQ_MAX_THREAD_COUNT];
static bool gOccupied[CMDQ_MAX_THREAD_COUNT];
static bool bCountEnable;
struct task_struct *cmdq_print_log_task = NULL;
static struct PrintStruct gCmdqPrint;

static int cmdq_core_print_log_kthread(void *data)
{
	int i;
	char longMsg[CMDQ_LONGSTRING_MAX];
	uint32_t msgOffset;
	int32_t msgMAXSize;
	bool needPrintLog;
	struct sched_param param = {.sched_priority = 94};

	sched_setscheduler(current, SCHED_RR, &param);

	while (1) {
		longMsg[0] = '\0';
		msgOffset = 0;
		msgMAXSize = CMDQ_LONGSTRING_MAX;
		needPrintLog = cmdq_core_should_print_msg(LOG_LEVEL_LOG);

		cmdqCoreLongString(needPrintLog, longMsg, &msgOffset, &msgMAXSize,
				   "gTaskCount print,");

		for (i = 0; i < CMDQ_MAX_THREAD_COUNT; i++) {
			if (gTaskCount[i])
				cmdqCoreLongString(needPrintLog, longMsg, &msgOffset, &msgMAXSize,
						   " Thread[%d]=%d,", i, gTaskCount[i]);
		}

		for (i = 0; i < CMDQ_MAX_THREAD_COUNT; i++) {
			if (gTaskBuffer[i])
				cmdqCoreLongString(needPrintLog, longMsg, &msgOffset, &msgMAXSize,
				   " thread %d task count = %d full,", i, gTaskBuffer[i]);
		}

		for (i = 0; i < CMDQ_MAX_THREAD_COUNT; i++) {
			if (gOccupied[i] == true)
				cmdqCoreLongString(needPrintLog, longMsg, &msgOffset, &msgMAXSize,
				   " thread %d nextCookie = %d already has task,", i, gNextCookieOccupied[i]);
		}

		cmdqCoreLongString(needPrintLog, longMsg, &msgOffset, &msgMAXSize,
		   "end\n");
		if (msgOffset > 0) {
			/* print message */
			CMDQ_LOG("%s", longMsg);
		}

		memset(gTaskCount, 0, sizeof(gTaskCount));
		memset(gTaskBuffer, 0, sizeof(gTaskBuffer));
		memset(gOccupied, 0, sizeof(gOccupied));

		msleep(1000);

		if (kthread_should_stop())
			break;
	}

	return 0;
}

int cmdq_core_enable_print_log(bool bEnable)
{
	CMDQ_LOG("%s:bEnable=%d\n", __func__, bEnable);
	gCmdqPrint.enablePrint = bEnable;
	return queue_work(gCmdqPrint.CmdqPrintWQ, &gCmdqPrint.CmdqPrintWork);
}

void cmdq_core_run_print_log(struct work_struct *workItem)
{
	struct PrintStruct *pPrint = NULL;

	pPrint = container_of(workItem, struct PrintStruct, CmdqPrintWork);
	CMDQ_LOG("%s:enablePrint=%d\n", __func__, pPrint->enablePrint);
	bCountEnable = pPrint->enablePrint;
	if (pPrint->enablePrint) {
		if (!cmdq_print_log_task) {
			cmdq_print_log_task =
			    kthread_create(cmdq_core_print_log_kthread, NULL,
					   "cmdq_core_print_log_kthread");
			if (IS_ERR(cmdq_print_log_task))
				CMDQ_ERR(" can not create cmdq_print_log_task kthread\n");

			wake_up_process(cmdq_print_log_task);
		}
	} else {
		if (cmdq_print_log_task) {
			kthread_stop(cmdq_print_log_task);
			cmdq_print_log_task = NULL;
		}
	}
}

void *cmdq_core_alloc_hw_buffer(struct device *dev, size_t size, dma_addr_t *dma_handle,
				const gfp_t flag)
{
	void *pVA;
	dma_addr_t PA;

	do {
		PA = 0;
		pVA = NULL;

#ifdef CMDQ_TEST_EMERGENCY_BUFFER
		const uint32_t minForceEmergencyBufferSize = 64 * 1024;

		if ((1 == atomic_read(&gCmdqDebugForceUseEmergencyBuffer)) &&
		    (minForceEmergencyBufferSize < size)) {
			CMDQ_LOG("[INFO]%s failed because...force use emergency buffer\n",
				 __func__);
			break;
		}
#endif

		CMDQ_PROF_START(current->pid, __func__);

		pVA = dma_alloc_coherent(dev, size, &PA, flag);

		CMDQ_PROF_END(current->pid, __func__);
	} while (0);

	*dma_handle = PA;

	CMDQ_VERBOSE("%s, pVA:0x%p, PA:0x%pa, PAout:0x%pa\n", __func__, pVA, &PA, &(*dma_handle));

	return pVA;
}

void cmdq_core_free_hw_buffer(struct device *dev, size_t size, void *cpu_addr,
			      dma_addr_t dma_handle)
{
	dma_free_coherent(dev, size, cpu_addr, dma_handle);
}

static bool cmdq_core_init_emergency_buffer(void)
{
	int i;

	memset(&gCmdqEmergencyBuffer[0], 0, sizeof(gCmdqEmergencyBuffer));

	for (i = 0; i < CMDQ_EMERGENCY_BLOCK_COUNT; ++i) {
		struct EmergencyBufferStruct *buf = &gCmdqEmergencyBuffer[i];

		buf->va = cmdq_core_alloc_hw_buffer(cmdq_dev_get(),
						    CMDQ_EMERGENCY_BLOCK_SIZE, &buf->pa,
						    GFP_KERNEL);
		buf->size = CMDQ_EMERGENCY_BLOCK_SIZE;
		buf->used = false;
	}

	return true;
}

static bool cmdq_core_uninit_emergency_buffer(void)
{
	int i;

	for (i = 0; i < CMDQ_EMERGENCY_BLOCK_COUNT; ++i) {
		struct EmergencyBufferStruct *buf = &gCmdqEmergencyBuffer[i];

		cmdq_core_free_hw_buffer(cmdq_dev_get(), CMDQ_EMERGENCY_BLOCK_SIZE, buf->va,
					 buf->pa);
		if (buf->used) {
			CMDQ_ERR("Emergency buffer %d, 0x%p, 0x%pa still using\n",
				 i, buf->va, &buf->pa);
		}
	}

	memset(&gCmdqEmergencyBuffer[0], 0, sizeof(gCmdqEmergencyBuffer));

	return true;
}


static bool cmdq_core_alloc_emergency_buffer(void **va, dma_addr_t *pa)
{
	int i;
	bool ret = false;

	spin_lock(&gCmdqAllocLock);
	for (i = 0; i < CMDQ_EMERGENCY_BLOCK_COUNT; ++i) {
		/* find a free emergency buffer */
		CMDQ_VERBOSE("EmergencyBuffer[%d], use:%d, 0x%p, 0x%pa\n",
			     i,
			     gCmdqEmergencyBuffer[i].used,
			     gCmdqEmergencyBuffer[i].va, &(gCmdqEmergencyBuffer[i].pa));

		if (!gCmdqEmergencyBuffer[i].used && gCmdqEmergencyBuffer[i].va) {
			gCmdqEmergencyBuffer[i].used = true;
			*va = gCmdqEmergencyBuffer[i].va;
			*pa = gCmdqEmergencyBuffer[i].pa;
			ret = true;
			break;
		}
	}
	spin_unlock(&gCmdqAllocLock);
	return ret;
}

static void cmdq_core_free_emergency_buffer(void *va, dma_addr_t pa)
{
	int i;

	spin_lock(&gCmdqAllocLock);
	for (i = 0; i < CMDQ_EMERGENCY_BLOCK_COUNT; ++i) {
		if (gCmdqEmergencyBuffer[i].used && va == gCmdqEmergencyBuffer[i].va) {
			gCmdqEmergencyBuffer[i].used = false;
			break;
		}
	}
	spin_unlock(&gCmdqAllocLock);
}

bool cmdq_core_is_emergency_buffer(void *va)
{
	int i;
	bool ret = false;

	spin_lock(&gCmdqAllocLock);
	for (i = 0; i < CMDQ_EMERGENCY_BLOCK_COUNT; ++i) {
		if (gCmdqEmergencyBuffer[i].used && va == gCmdqEmergencyBuffer[i].va) {
			ret = true;
			break;
		}
	}
	spin_unlock(&gCmdqAllocLock);
	return ret;
}


int32_t cmdq_core_set_secure_IRQ_status(uint32_t value)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	const uint32_t offset = CMDQ_SEC_SHARED_IRQ_RAISED_OFFSET;
	uint32_t *pVA;

	value = 0x0;
	if (NULL == gCmdqContext.hSecSharedMem) {
		CMDQ_ERR("%s, shared memory is not created\n", __func__);
		return -EFAULT;
	}

	pVA = (uint32_t *) (gCmdqContext.hSecSharedMem->pVABase + offset);
	(*pVA) = value;

	CMDQ_VERBOSE("[shared_IRQ]set raisedIRQ:0x%08x\n", value);

	return 0;
#else
	CMDQ_ERR("func:%s failed since CMDQ secure path not support in this proj\n", __func__);
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
	if (NULL == gCmdqContext.hSecSharedMem) {
		CMDQ_ERR("%s, shared memory is not created\n", __func__);
		return -EFAULT;
	}

	pVA = (uint32_t *) (gCmdqContext.hSecSharedMem->pVABase + offset);
	value = *pVA;

	CMDQ_VERBOSE("[shared_IRQ]IRQ raised:0x%08x\n", value);

	return value;
#else
	CMDQ_ERR("func:%s failed since CMDQ secure path not support in this proj\n", __func__);
	return -EFAULT;
#endif
}


int32_t cmdq_core_set_secure_thread_exec_counter(const int32_t thread, const uint32_t cookie)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	const uint32_t offset = CMDQ_SEC_SHARED_MEM_THR_CNT_BASE + thread * sizeof(uint32_t);
	uint32_t *pVA = NULL;

	if (0 > cmdq_core_is_a_secure_thread(thread)) {
		CMDQ_ERR("%s, invalid param, thread: %d\n", __func__, thread);
		return -EFAULT;
	}

	if (NULL == gCmdqContext.hSecSharedMem) {
		CMDQ_ERR("%s, shared memory is not created\n", __func__);
		return -EFAULT;
	}

	CMDQ_MSG("[shared_cookie] set thread %d CNT(%p) to %d\n", thread, pVA, cookie);
	pVA = (uint32_t *) (gCmdqContext.hSecSharedMem->pVABase + offset);
	(*pVA) = cookie;

	return 0;
#else
	CMDQ_ERR("func:%s failed since CMDQ secure path not support in this proj\n", __func__);
	return -EFAULT;
#endif
}

int32_t cmdq_core_get_secure_thread_exec_counter(const int32_t thread, uint32_t *pCookie)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	const uint32_t offset = CMDQ_SEC_SHARED_MEM_THR_CNT_BASE + thread * sizeof(uint32_t);
	uint32_t *pVA;
	uint32_t value;

	if (0 > cmdq_core_is_a_secure_thread(thread)) {
		CMDQ_ERR("%s, invalid param, thread: %d\n", __func__, thread);
		return -EFAULT;
	}

	if (NULL == gCmdqContext.hSecSharedMem) {
		CMDQ_ERR("%s, shared memory is not created\n", __func__);
		return -EFAULT;
	}

	pVA = (uint32_t *) (gCmdqContext.hSecSharedMem->pVABase + offset);
	value = *pVA;
	*pCookie = value;

	CMDQ_MSG("[shared_cookie] get thread %d CNT(%p) value is %d\n", thread, pVA, value);

	return 0;
#else
	CMDQ_ERR("func:%s failed since CMDQ secure path not support in this proj\n", __func__);
	return -EFAULT;
#endif

}

#ifdef CMDQ_SECURE_PATH_SUPPORT
cmdqSecSharedMemoryHandle cmdq_core_get_secure_shared_memory(void)
{
	return gCmdqContext.hSecSharedMem;
}
#endif


ssize_t cmdqCorePrintLogLevel(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;

	if (buf)
		len = sprintf(buf, "%d\n", cmdq_core_get_log_level());
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
		if (0 > kstrtoint(textBuf, 10, &value)) {
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

ssize_t cmdqCorePrintProfileEnable(struct device *dev, struct device_attribute *attr, char *buf)
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
		if (0 > kstrtoint(textBuf, 10, &value)) {
			status = -EFAULT;
			break;
		}

		status = len;
		if (value < 0 || value > 3)
			value = 0;

		gCmdqContext.enableProfile = value;
/*		if (0 < value)
			cmdqSecEnableProfile(true);
		else
			cmdqSecEnableProfile(false); */
	} while (0);

	return status;
}



ssize_t cmdqCorePrintStatus(struct device *dev, struct device_attribute *attr, char *buf)
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
		&gCmdqContext.taskFreeList, &gCmdqContext.taskActiveList,
		&gCmdqContext.taskWaitList
	};
	static const char *const listNames[] = { "Free", "Active", "Wait" };

	const enum CMDQ_ENG_ENUM engines[] = CMDQ_FOREACH_STATUS_MODULE_PRINT(GENERATE_ENUM);
	static const char *const engineNames[] =
	    CMDQ_FOREACH_STATUS_MODULE_PRINT(GENERATE_STRING);

	cmdqCorePrintStatus_idv(pBuffer);

	pBuffer += sprintf(pBuffer, "====== Events =======\n");
	pBuffer += cmdq_core_print_event(pBuffer);

	pBuffer += sprintf(pBuffer, "====== Mutex Usage =======\n");
	for (listIdx = 0; listIdx < DISP_MUTEX_MDP_COUNT; ++listIdx) {
		if (cmdqMDPMutexInUse(listIdx)) {
			pBuffer += sprintf(pBuffer, "Mutex %d used by pid:%d\n",
					   listIdx, cmdqMDPMutexOwnerPid(listIdx));
		}
	}

	pBuffer += sprintf(pBuffer, "====== Engine Usage =======\n");

	for (listIdx = 0; listIdx < (sizeof(engines) / sizeof(engines[0])); ++listIdx) {
		pEngine = &gCmdqContext.engine[engines[listIdx]];
		pBuffer += sprintf(pBuffer, "%s: count %d, owner %d, fail: %d, reset: %d\n",
				   engineNames[listIdx],
				   pEngine->userCount,
				   pEngine->currOwner, pEngine->failCount, pEngine->resetCount);
	}


	mutex_lock(&gCmdqTaskMutex);

	/* print all tasks in both list */
	for (listIdx = 0; listIdx < (sizeof(lists) / sizeof(lists[0])); listIdx++) {
		/* skip FreeTasks by default */
		if (cmdq_core_get_log_level() < LOG_LEVEL_LOG && listIdx == 0)
			continue;


		index = 0;
		list_for_each(p, lists[listIdx]) {
			pTask = list_entry(p, struct TaskStruct, listEntry);
			pBuffer +=
			    sprintf(pBuffer, "====== %s Task(%d) 0x%p Usage =======\n",
				    listNames[listIdx], index, pTask);

			pBuffer +=
			    sprintf(pBuffer, "State %d, VABase: 0x%p, MVABase: %pa, Size: %d\n",
				    pTask->taskState, pTask->pVABase, &pTask->MVABase,
				    pTask->commandSize);
			pBuffer +=
			    sprintf(pBuffer,
				    "Scenario %d, Priority: %d, Flag: 0x%08llx, VAEnd: 0x%p\n",
				    pTask->scenario, pTask->priority, pTask->engineFlag,
				    pTask->pCMDEnd);
			pBuffer +=
			    sprintf(pBuffer, "Trigger %lld, IRQ: %lld, Wait: %lld, Wake Up: %lld\n",
				    pTask->trigger, pTask->gotIRQ, pTask->beginWait,
				    pTask->wakedUp);
			++index;
		}
		pBuffer +=
		    sprintf(pBuffer, "====== Total %d %s Task =======\n", index,
			    listNames[listIdx]);
	}

	for (index = 0; index < CMDQ_MAX_THREAD_COUNT; index++) {
		pThread = &(gCmdqContext.thread[index]);
		if (0 == pThread->taskCount)
			continue;
		pBuffer += sprintf(pBuffer, "====== Thread %d Usage =======\n", index);
		pBuffer +=
		    sprintf(pBuffer, "Wait Cookie %d, Next Cookie %d\n",
			    pThread->waitCookie, pThread->nextCookie);

		spin_lock_irqsave(&gCmdqThreadLock, flags);

		for (inner = 0; inner < CMDQ_MAX_TASK_IN_THREAD; inner++) {
			pTask = pThread->pCurTask[inner];
			if (NULL != pTask) {
				/* dump task basic info */
				pBuffer +=
				    sprintf(pBuffer,
					    "Slot: %d, Task: 0x%p, Pid: %d, Name: %s, Scn: %d, VABase: 0x%p, MVABase: %pa, Size: %d, Last Command: 0x%08x:0x%08x\n",
					    index, pTask, pTask->callerPid,
					    pTask->callerName, pTask->scenario,
					    pTask->pVABase, &pTask->MVABase,
					    pTask->commandSize, pTask->pCMDEnd[-1],
					    pTask->pCMDEnd[0]);

				/* dump PC info */
				do {
					uint32_t *pcVA = NULL;
					uint32_t insts[4] = { 0 };
					char parsedInstruction[128] = { 0 };

					pcVA = cmdq_core_get_pc(pTask, index, insts);
					if (pcVA) {
						cmdq_core_parse_instruction(pcVA,
									    parsedInstruction,
									    sizeof
									    (parsedInstruction));
						pBuffer +=
						    sprintf(pBuffer,
							    "PC(VA): 0x%p, 0x%08x:0x%08x => %s",
							    pcVA, insts[2], insts[3],
							    parsedInstruction);
					} else {
						pBuffer +=
						    sprintf(pBuffer, "PC(VA): Not available\n");
					}

				} while (0);


			}
		}

		spin_unlock_irqrestore(&gCmdqThreadLock, flags);
	}

	do {
		uint32_t value[10] = { 0 };
		uint32_t thread = CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT;

		pBuffer += sprintf(pBuffer, ("trigger thread:\n"));
		value[0] = CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread));
		value[1] = CMDQ_REG_GET32(CMDQ_THR_END_ADDR(thread));
		value[2] = CMDQ_REG_GET32(CMDQ_THR_WAIT_TOKEN(thread));
		value[3] = CMDQ_GET_COOKIE_CNT(thread);
		value[4] = CMDQ_REG_GET32(CMDQ_THR_IRQ_STATUS(thread));
		value[5] = CMDQ_REG_GET32(CMDQ_THR_INST_CYCLES(thread));
		value[6] = CMDQ_REG_GET32(CMDQ_THR_CURR_STATUS(thread));
		value[7] = CMDQ_REG_GET32(CMDQ_THR_IRQ_ENABLE(thread));
		value[8] = CMDQ_REG_GET32(CMDQ_THR_ENABLE_TASK(thread));

		pBuffer +=
		    sprintf(pBuffer,
			    "Index: %d, Enabled: %d, IRQ: 0x%08x, Thread PC: 0x%08x, End: 0x%08x, Wait Token: 0x%08x\n",
			    thread, value[8], value[4], value[0], value[1], value[2]);
		pBuffer +=
		    sprintf(pBuffer,
			    "Curr Cookie: %d, Wait Cookie: %d, Next Cookie: %d, Task Count %d,\n",
			    value[3], pThread->waitCookie, pThread->nextCookie, pThread->taskCount);
		pBuffer +=
		    sprintf(pBuffer, "Timeout Cycle:%d, Status:0x%08x, IRQ_EN: 0x%08x\n", value[5],
			    value[6], value[7]);
	} while (0);

	/* dump display registers... */
	length = pBuffer - buf;
	if (length < PAGE_SIZE)
		pBuffer += primary_display_check_path(pBuffer, (PAGE_SIZE - length));

	mutex_unlock(&gCmdqTaskMutex);

	length = pBuffer - buf;

	BUG_ON(length > PAGE_SIZE);

	return length;

}

static int cmdq_core_print_record(const struct RecordStruct *pRecord, int index, char *buf,
				  int bufLen)
{
	int length = 0;
	char *unit[5] = { "ms", "ms", "ms", "ms", "ms" };
	int32_t IRQTime;
	int32_t execTime;
	int32_t beginWaitTime;
	int32_t totalTime;
	int32_t acquireThreadTime;
	unsigned long rem_nsec = 0;
	CMDQ_TIME submitTimeSec = pRecord->submit;

	rem_nsec = do_div(submitTimeSec, 1000000000);

	unit[0] = "ms";
	unit[1] = "ms";
	unit[2] = "ms";
	unit[3] = "ms";
	unit[4] = "ms";
	CMDQ_GET_TIME_IN_MS(pRecord->submit, pRecord->done, totalTime);
	CMDQ_GET_TIME_IN_MS(pRecord->submit, pRecord->trigger, acquireThreadTime);
	CMDQ_GET_TIME_IN_MS(pRecord->submit, pRecord->beginWait, beginWaitTime)
	    CMDQ_GET_TIME_IN_MS(pRecord->trigger, pRecord->gotIRQ, IRQTime)
	    CMDQ_GET_TIME_IN_MS(pRecord->trigger, pRecord->wakedUp, execTime);

	/* detect us interval */
	if (0 == acquireThreadTime) {
		CMDQ_GET_TIME_IN_US_PART(pRecord->submit, pRecord->trigger, acquireThreadTime);
		unit[0] = "us";
	}
	if (0 == IRQTime) {
		CMDQ_GET_TIME_IN_US_PART(pRecord->trigger, pRecord->gotIRQ, IRQTime);
		unit[1] = "us";
	}
	if (0 == beginWaitTime) {
		CMDQ_GET_TIME_IN_US_PART(pRecord->submit, pRecord->beginWait, beginWaitTime);
		unit[2] = "us";
	}
	if (0 == execTime) {
		CMDQ_GET_TIME_IN_US_PART(pRecord->trigger, pRecord->wakedUp, execTime);
		unit[3] = "us";
	}
	if (0 == totalTime) {
		CMDQ_GET_TIME_IN_US_PART(pRecord->submit, pRecord->done, totalTime);
		unit[4] = "us";
	}
	length = snprintf(buf,
			  bufLen,
			  "%4d:(%5d, %2d, 0x%012llx, %2d, %d)(%02d, %02d)(%5dns : %lld, %lld)(%5llu.%06lu, %4d%s, %4d%s, %4d%s, %4d%s)%4d%s\n",
			  index, pRecord->user, pRecord->scenario,
			  pRecord->engineFlag, pRecord->priority,
			  pRecord->isSecure,
			  pRecord->thread,
			  cmdq_core_priority_from_scenario(pRecord->scenario),
			  pRecord->writeTimeNS,
			  pRecord->writeTimeNSBegin, pRecord->writeTimeNSEnd,
			  submitTimeSec, rem_nsec / 1000,
			  acquireThreadTime, unit[0],
			  IRQTime, unit[1],
			  beginWaitTime, unit[2], execTime, unit[3], totalTime, unit[4]);

	return length;
}


ssize_t cmdqCorePrintError(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i;
	int length = 0;

	for (i = 0; i < gCmdqContext.errNum && i < CMDQ_MAX_ERROR_COUNT; ++i) {
		struct ErrorStruct *pError = &gCmdqContext.error[i];
		u64 ts = pError->ts_nsec;
		unsigned long rem_nsec = do_div(ts, 1000000000);

		length += snprintf(buf + length,
				   PAGE_SIZE - length,
				   "[%5lu.%06lu] ", (unsigned long)ts, rem_nsec / 1000);
		length += cmdq_core_print_record(&pError->errorRec,
						 i, buf + length, PAGE_SIZE - length);
		if (length >= PAGE_SIZE)
			break;

	}

	return length;
}

int cmdqCorePrintRecordSeq(struct seq_file *m, void *v)
{
	unsigned long flags;
	int32_t index;
	int32_t numRec;
	struct RecordStruct record;
	char msg[512] = { 0 };


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
	}

	return 0;
}

/*
**	pPage: userspace buffer,data from userspace are store here
**	Count:	buffer size
**	ppos:	the start position of buffer content
**	return:	return negative means error. return 0 or postive to represent the bytes written to file
*/
ssize_t cmdq_read_debug_level_proc(struct file *file,
				   char __user *pPage, size_t Count, loff_t *ppos)
{
#if 0
	static int num;
	char buffer[50];

	memset(buffer, 0, 50);
	CMDQ_ERR("hello:%d\n", num++);
	snprintf(buffer, 50, "debug level is:%d\n", cmdq_core_get_log_level());

	return simple_read_from_buffer(pPage, Count, ppos, buffer, strlen(buffer));
#else
	static int num;
	int length = 0;
	int notSend = 0;
	char buffer[50];

	memset(buffer, 0, 50);
	snprintf(buffer, 50, "debug level is:%d\n", cmdq_core_get_log_level());

	length = strlen(buffer) + 1;

	if (*ppos >= length || Count == 0) {
		CMDQ_ERR("pos:%d, length:%d, Count:%d", (int)(*ppos), length, (int)Count);
		return 0;
	}
	CMDQ_ERR("hello:%d\n", num++);
	if (Count > length - *ppos)
		Count = length - *ppos;
	notSend = copy_to_user(pPage, buffer + *ppos, Count);
	if (notSend == Count) {
		CMDQ_DBG("debug level is:%d, show in userspace error\n", cmdq_core_get_log_level());
		CMDQ_DBG("sec print count is:%d\n", cmdq_core_get_sec_print_count());
		return -EFAULT;
	}
	CMDQ_DBG("read log level:%d\n", cmdq_core_get_log_level());
	CMDQ_DBG("sec print count is:%d\n", cmdq_core_get_sec_print_count());
	Count = Count - notSend;
	*ppos = *ppos + Count;	/*remember to update ppos */
	return length;
#endif
}


/*
**	buf: userspace buffer,data from userspace are store here
**	count:	the size of buffer content
**	ppos:	the start position of buffer content
**	return:	return negative means error. return 0 or postive to represent the bytes written to file
*/
ssize_t cmdq_write_debug_level_proc(struct file *file,
				    const char __user *buf, size_t count, loff_t *ppos)
{
#if 0
	int value;
	char buffer[20];
	int length = 0;

	memset(buffer, 0, 20);

	length = simple_write_to_buffer(buffer, 20, ppos, buf, count);

	buffer[20] = '\0';
/*	if (sscanf(buffer, "%d", &value) != 1) { */
		CMDQ_ERR("set logLevel error:%s\n", buffer);
		return 0;
	}

	if (value < LOG_LEVEL_MSG || value > LOG_LEVEL_MAX) {
		CMDQ_DBG("set log level error:%d\n", value);
		value = LOG_LEVEL_LOG;
	}
	cmdq_core_set_log_level((enum LOG_LEVEL)(value));
	return length;
#else
	int len = 0;
	int32_t value = 0;
	char textBuf[15] = { 0 };
	long tmp;

	if (count >= 15)
		return -EFAULT;


	len = count;
	if (copy_from_user(textBuf, buf, len))
		return -EFAULT;

	textBuf[len] = '\0';

	if (!strncmp(textBuf, "sec_", 4)) {
		if (len <= 4) {
			CMDQ_ERR("set logLevel error:len<=4\n");
		} else {
			int32_t ret;

			ret = kstrtol(textBuf + 4, 0, &tmp);
			if (!ret)
				cmdq_core_set_sec_print_count((uint32_t) tmp);
			else
				CMDQ_ERR("set logLevel error:%s ret=%d\n", textBuf + 4, ret);
		}
		return len;
	} else if (!strncmp(textBuf, "en_count", 8)) {
		CMDQ_LOG("en_count");
		cmdq_core_enable_print_log(true);
		return len;
	} else if (!strncmp(textBuf, "dis_count", 9)) {
		CMDQ_LOG("dis_count");
		cmdq_core_enable_print_log(false);
		return len;
	}

/*	if (sscanf(textBuf, "%d", &value) != 1) { */
	if (kstrtol(textBuf, 0, &tmp) != 0) {
		CMDQ_ERR("set logLevel error:%s\n", textBuf);
		return len;
	}

	if (value < LOG_LEVEL_MSG || value > LOG_LEVEL_MAX) {
		CMDQ_DBG("set log level error:%d\n", value);
		value = LOG_LEVEL_LOG;
	}
	cmdq_core_set_log_level((enum LOG_LEVEL)(value));
	return len;
#endif
}


int cmdqCorePrintErrorSeq(struct seq_file *m, void *v)
{
	/* error is not used by now */
	return 0;
}

void cmdq_core_print_thread_seq(int thread, struct seq_file *m)
{
	uint32_t value[10] = { 0 };

	seq_puts(m, "trigger thread:\n");
	value[0] = CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread));
	value[1] = CMDQ_REG_GET32(CMDQ_THR_END_ADDR(thread));
	value[2] = CMDQ_REG_GET32(CMDQ_THR_WAIT_TOKEN(thread));
	value[3] = CMDQ_GET_COOKIE_CNT(thread);
	value[4] = CMDQ_REG_GET32(CMDQ_THR_IRQ_STATUS(thread));
	value[5] = CMDQ_REG_GET32(CMDQ_THR_INST_CYCLES(thread));
	value[6] = CMDQ_REG_GET32(CMDQ_THR_CURR_STATUS(thread));
	value[7] = CMDQ_REG_GET32(CMDQ_THR_IRQ_ENABLE(thread));
	value[8] = CMDQ_REG_GET32(CMDQ_THR_ENABLE_TASK(thread));

	seq_printf(m,
		   "Index: %d, Enabled: %d, IRQ: 0x%08x, Thread PC: 0x%08x, End: 0x%08x, Wait Token: 0x%08x\n",
		   thread, value[8], value[4], value[0], value[1], value[2]);
	seq_printf(m, "Timeout Cycle:%d, Status:0x%08x, IRQ_EN: 0x%08x\n", value[5], value[6],
		   value[7]);
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
	static const char *const listNames[] = { "Free", "Active", "Wait" };

	const enum CMDQ_ENG_ENUM engines[] = CMDQ_FOREACH_STATUS_MODULE_PRINT(GENERATE_ENUM);
	static const char *const engineNames[] =
	    CMDQ_FOREACH_STATUS_MODULE_PRINT(GENERATE_STRING);

	cmdqCorePrintStatusSeq_idv(m);

	seq_puts(m, "====== Mutex Usage =======\n");
	for (listIdx = 0; listIdx < DISP_MUTEX_MDP_COUNT; ++listIdx) {
		if (cmdqMDPMutexInUse(listIdx)) {
			seq_printf(m, "Mutex %d used by pid:%d\n",
				   listIdx, cmdqMDPMutexOwnerPid(listIdx));
		}
	}

	do {
		int32_t coreExecThread = CMDQ_INVALID_THREAD;
		uint32_t value[6] = { 0 };

		value[0] = CMDQ_REG_GET32(CMDQ_CURR_LOADED_THR);
		value[1] = CMDQ_REG_GET32(CMDQ_THR_EXEC_CYCLES);
		value[2] = CMDQ_REG_GET32(CMDQ_THR_TIMEOUT_TIMER);
		value[3] = CMDQ_REG_GET32(CMDQ_BUS_CONTROL_TYPE);
		/* this returns (1 + index of least bit set) or 0 if input is 0. */
		coreExecThread = __builtin_ffs(value[0]) - 1;
		seq_printf(m,
			   "IRQ flag:0x%08x, Execing:%d, Exec Thread:%d, CMDQ_CURR_LOADED_THR: 0x%08x\n",
			   CMDQ_REG_GET32(CMDQ_CURR_IRQ_STATUS), (0x80000000 & value[0]) ? 1 : 0,
			   coreExecThread, value[0]);
		if (CMDQ_INVALID_THREAD != coreExecThread) {
			seq_printf(m, "exec thread %d dump:\n", coreExecThread);
			cmdq_core_print_thread_seq(coreExecThread, m);
		}
		seq_printf(m,
			   "CMDQ_THR_EXEC_CYCLES:0x%08x, CMDQ_THR_TIMER:0x%08x, CMDQ_BUS_CTRL:0x%08x\n",
			   value[1], value[2], value[3]);
		seq_printf(m, "CMDQ_DEBUG_1: 0x%08x\n", CMDQ_REG_GET32((GCE_BASE_VA + 0xF0)));
		seq_printf(m, "CMDQ_DEBUG_2: 0x%08x\n", CMDQ_REG_GET32((GCE_BASE_VA + 0xF4)));
		seq_printf(m, "CMDQ_DEBUG_3: 0x%08x\n", CMDQ_REG_GET32((GCE_BASE_VA + 0xF8)));
		seq_printf(m, "CMDQ_DEBUG_4: 0x%08x\n", CMDQ_REG_GET32((GCE_BASE_VA + 0xFC)));

	} while (0);


	seq_puts(m, "====== Events =======\n");
	cmdq_core_print_event_seq(m);

	seq_puts(m, "====== Engine Usage =======\n");

	for (listIdx = 0; listIdx < (sizeof(engines) / sizeof(engines[0])); ++listIdx) {
		pEngine = &gCmdqContext.engine[engines[listIdx]];
		seq_printf(m, "%s: count %d, owner %d, fail: %d, reset: %d\n",
			   engineNames[listIdx],
			   pEngine->userCount,
			   pEngine->currOwner, pEngine->failCount, pEngine->resetCount);
	}


	mutex_lock(&gCmdqTaskMutex);

	/* print all tasks in both list */
	for (listIdx = 0; listIdx < (sizeof(lists) / sizeof(lists[0])); listIdx++) {
		/* skip FreeTasks by default */
		if (cmdq_core_get_log_level() < LOG_LEVEL_LOG && listIdx == 0)
			continue;


		index = 0;
		list_for_each(p, lists[listIdx]) {
			pTask = list_entry(p, struct TaskStruct, listEntry);
			seq_printf(m, "====== %s Task(%d) 0x%p Usage =======\n", listNames[listIdx],
				   index, pTask);

			seq_printf(m, "State %d, VABase: 0x%p, MVABase: %pa, Size: %d\n",
				   pTask->taskState, pTask->pVABase, &pTask->MVABase,
				   pTask->commandSize);
			seq_printf(m, "Scenario %d, Priority: %d, Flag: 0x%08llx, VAEnd: 0x%p\n",
				   pTask->scenario, pTask->priority, pTask->engineFlag,
				   pTask->pCMDEnd);
			seq_printf(m,
				   "Reorder:%d, Trigger %lld, IRQ: %lld, Wait: %lld, Wake Up: %lld\n",
				   pTask->reorder,
				   pTask->trigger, pTask->gotIRQ, pTask->beginWait, pTask->wakedUp);
			++index;
		}
		seq_printf(m, "====== Total %d %s Task =======\n", index, listNames[listIdx]);
	}

	for (index = 0; index < CMDQ_MAX_THREAD_COUNT; index++) {
		pThread = &(gCmdqContext.thread[index]);

		if (pThread->taskCount > 0) {
			seq_printf(m, "====== Thread %d Usage =======\n", index);
			seq_printf(m, "Wait Cookie %d, Next Cookie %d\n", pThread->waitCookie,
				   pThread->nextCookie);

			spin_lock_irqsave(&gCmdqThreadLock, flags);

			for (inner = 0; inner < CMDQ_MAX_TASK_IN_THREAD; inner++) {
				pTask = pThread->pCurTask[inner];
				if (NULL == pTask)
					continue;
				/* dump task basic info */
				seq_printf(m,
					   "Slot: %d, Task: 0x%p, Pid: %d, Name: %s, Scn: %d, VABase: 0x%p, MVABase: %pa, Size: %d, Last Command: 0x%08x:0x%08x\n",
					   index, pTask, pTask->callerPid,
					   pTask->callerName, pTask->scenario,
					   pTask->pVABase, &pTask->MVABase,
					   pTask->commandSize, pTask->pCMDEnd[-1],
					   pTask->pCMDEnd[0]);

				/* dump PC info */
				do {
					uint32_t *pcVA = NULL;
					uint32_t insts[4] = { 0 };
					char parsedInstruction[128] = { 0 };

					pcVA = cmdq_core_get_pc(pTask, index, insts);
					if (pcVA) {
						cmdq_core_parse_instruction(pcVA,
									    parsedInstruction,
									    sizeof
									    (parsedInstruction));
						seq_printf(m,
							   "PC(VA): 0x%p, 0x%08x:0x%08x => %s",
							   pcVA, insts[2], insts[3],
							   parsedInstruction);
					} else {
						seq_puts(m, "PC(VA): Not available\n");
					}

				} while (0);
			}
			spin_unlock_irqrestore(&gCmdqThreadLock, flags);
		}
	}

	cmdq_core_print_thread_seq(CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT, m);

	/* dump display registers... */
	do {
		char dispStatBuf[1024] = { 0 };

		primary_display_check_path(dispStatBuf, sizeof(dispStatBuf));
		seq_printf(m, dispStatBuf);
	} while (0);

	mutex_unlock(&gCmdqTaskMutex);

	return 0;
}

const char *cmdq_core_get_event_name(enum CMDQ_EVENT_ENUM event)
{
#undef DECLARE_CMDQ_EVENT
#define DECLARE_CMDQ_EVENT(name, val) case val:return #name;

	switch (event) {
#include "cmdq_event.h"
	}

	return "CMDQ_EVENT_UNKNOWN";
#undef DECLARE_CMDQ_EVENT
}

int32_t cmdq_subsys_from_phys_addr(uint32_t physAddr)
{
	const int32_t msb = (physAddr & 0x0FFFF0000) >> 16;

#undef DECLARE_CMDQ_SUBSYS
#define DECLARE_CMDQ_SUBSYS(addr, id, grp, base) case addr: return id;
	switch (msb) {
#include "cmdq_subsys.h"
	}

	CMDQ_ERR("unrecognized subsys, msb=0x%04x, physAddr:0x%08x\n", msb, physAddr);
	return -1;
#undef DECLARE_CMDQ_SUBSYS
}

ssize_t cmdqCorePrintRecord(struct device *dev, struct device_attribute *attr, char *buf)
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

		length = cmdq_core_print_record(&record, index, &buf[curPos], bufLen);

		bufLen -= length;
		curPos += length;

		if (bufLen <= 0 || curPos >= PAGE_SIZE)
			break;

	}

	if (curPos >= PAGE_SIZE)
		curPos = PAGE_SIZE;


	return curPos;
}

/*  */
/* For kmemcache, initialize variables of struct TaskStruct (but not buffers) */
static void cmdq_core_task_ctor(void *param)
{
	struct TaskStruct *pTask = (struct TaskStruct *)param;

	CMDQ_VERBOSE("cmdq_core_task_ctor: 0x%p\n", param);
	memset(pTask, 0, sizeof(struct TaskStruct));
	INIT_LIST_HEAD(&(pTask->listEntry));
	pTask->taskState = TASK_STATE_IDLE;
	pTask->thread = CMDQ_INVALID_THREAD;

}

void cmdq_task_free_task_command_buffer(struct TaskStruct *pTask)
{
	if (pTask->pVABase) {
		if (pTask->useEmergencyBuf) {
			cmdq_core_free_emergency_buffer(pTask->pVABase, pTask->MVABase);
		} else {
			cmdq_core_free_hw_buffer(cmdq_dev_get(), pTask->bufferSize, pTask->pVABase,
						 pTask->MVABase);
		}

		pTask->pVABase = NULL;
		pTask->MVABase = 0;
		pTask->bufferSize = 0;
		pTask->commandSize = 0;
		pTask->pCMDEnd = NULL;
	}
}

/*  */
/* Ensures size of command buffer of the given task. */
/* Existing buffer will be copied to new buffer. */
/*  */
/* This buffer is guranteed to be physically continuous. */
/*  */
/* returns -ENOMEM if cannot allocate new buffer */
static int32_t cmdq_core_task_realloc_buffer_size(struct TaskStruct *pTask, uint32_t size)
{
	void *pNewBuffer = NULL;
	dma_addr_t newMVABase = 0;
	int32_t commandSize = 0;
	uint32_t *pCMDEnd = NULL;

	if (pTask->pVABase && pTask->bufferSize >= size) {
		/* buffer size is already good, do nothing. */
		return 0;
	}

	do {
		/* allocate new buffer, try if we can alloc without reclaim */
		pNewBuffer = cmdq_core_alloc_hw_buffer(cmdq_dev_get(),
						       size, &newMVABase,
						       GFP_KERNEL | __GFP_NO_KSWAPD);

		if (pNewBuffer) {
			pTask->useEmergencyBuf = false;
			break;
		}
		/* failed. Try emergency buffer */
		if (size <= CMDQ_EMERGENCY_BLOCK_SIZE)
			cmdq_core_alloc_emergency_buffer(&pNewBuffer, &newMVABase);

		if (pNewBuffer) {
			CMDQ_LOG("emergency buffer %p allocated\n", pNewBuffer);
			pTask->useEmergencyBuf = true;
			break;
		}
		/* finally try reclaim */
		pNewBuffer =
		    cmdq_core_alloc_hw_buffer(cmdq_dev_get(), size, &newMVABase, GFP_KERNEL);
		if (pNewBuffer) {
			CMDQ_LOG("buffer %p allocated after reclaim\n", pNewBuffer);
			pTask->useEmergencyBuf = false;
			break;
		}
	} while (0);

	if (NULL == pNewBuffer) {
		CMDQ_ERR("realloc cmd buffer of size %d failed\n", size);
		return -ENOMEM;
	}

	memset(pNewBuffer, 0, size);

	/* copy and release old buffer */
	if (pTask->pVABase)
		memcpy(pNewBuffer, pTask->pVABase, pTask->bufferSize);

	/* we should keep track of pCMDEnd and cmdSize since they are cleared in free command buffer */
	pCMDEnd = pTask->pCMDEnd;
	commandSize = pTask->commandSize;
	cmdq_task_free_task_command_buffer(pTask);

	/* attach the new buffer */
	pTask->pVABase = (uint32_t *) pNewBuffer;
	pTask->MVABase = newMVABase;
	pTask->bufferSize = size;
	pTask->pCMDEnd = pCMDEnd;
	pTask->commandSize = commandSize;

	CMDQ_MSG("Task Buffer:0x%p, VA:%p PA:%pa\n", pTask, pTask->pVABase, &pTask->MVABase);
	return 0;
}

/*  */
/* Allocate and initialize struct TaskStruct and its command buffer */
static struct TaskStruct *cmdq_core_task_create(void)
{
	struct TaskStruct *pTask = NULL;
	int32_t status = 0;

	pTask = (struct TaskStruct *)kmem_cache_alloc(gCmdqContext.taskCache, GFP_KERNEL);
	status = cmdq_core_task_realloc_buffer_size(pTask, CMDQ_INITIAL_CMD_BLOCK_SIZE);
	if (status < 0) {
		CMDQ_AEE("CMDQ", "Allocate command buffer failed\n");
		kmem_cache_free(gCmdqContext.taskCache, pTask);
		pTask = NULL;
		return NULL;
	}
	return pTask;
}

void cmdq_core_reset_hw_events(void)
{
	/* set all defined events to 0 */
	CMDQ_MSG("cmdq_core_reset_hw_events\n");

#undef DECLARE_CMDQ_EVENT
#define DECLARE_CMDQ_EVENT(name, val) CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (CMDQ_SYNC_TOKEN_MAX & name));
#include "cmdq_event.h"
#undef DECLARE_CMDQ_EVENT

	/* However, GRP_SET are resource flags, */
	/* by default they should be 1. */
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_GPR_SET_0);
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_GPR_SET_1);
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_GPR_SET_2);
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_GPR_SET_3);
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_GPR_SET_4);

#if 0
	do {
		uint32_t value = 0;

		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_ID, 0x1FF & CMDQ_EVENT_MDP_WDMA_EOF);
		value = CMDQ_REG_GET32(CMDQ_SYNC_TOKEN_VAL);
		CMDQ_ERR("[DEBUG] CMDQ_EVENT_MDP_WDMA_EOF after reset is %d\n", value);
	} while (0);
#endif


}

#if 0
uint32_t *addressToDump[3] = { IO_VIRT_TO_PHYS(MMSYS_CONFIG_BASE_VA + 0x0890),
	IO_VIRT_TO_PHYS(MMSYS_CONFIG_BASE_VA + 0x0890),
	IO_VIRT_TO_PHYS(MMSYS_CONFIG_BASE_VA + 0x0890)
};

static int32_t testcase_regdump_begin(uint32_t taskID, uint32_t *regCount, uint32_t **regAddress)
{
	CMDQ_MSG("@@@@@@@@@@@@@@@@@@ testcase_regdump_begin, tid = %d\n", taskID);
	*regCount = 3;
	*regAddress = addressToDump;
	return 0;
}

static int32_t testcase_regdump_end(uint32_t taskID, uint32_t regCount, uint32_t *regValues)
{
	int i;

	CMDQ_MSG("@@@@@@@@@@@@@@@@@@ testcase_regdump_end, tid = %d\n", taskID);
	CMDQ_MSG("@@@@@@@@@@@@@@@@@@ regCount = %d\n", regCount);

	for (i = 0; i < regCount; ++i)
		CMDQ_MSG("@@@@@@@@@@@@@@@@@@ regValue[%d] = 0x%08x\n", i, regValues[i]);


	return 0;
}
#endif

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

int32_t cmdqCoreInitialize(void)
{
	struct TaskStruct *pTask;
	int32_t index;

	atomic_set(&gCmdqThreadUsage, 0);
	atomic_set(&gSMIThreadUsage, 0);

	BUG_ON(0 != atomic_read(&gCmdqThreadUsage));
	BUG_ON(0 != atomic_read(&gSMIThreadUsage));

	for (index = 0; index < CMDQ_MAX_THREAD_COUNT; index++)
		init_waitqueue_head(&gCmdWaitQueue[index]);


	init_waitqueue_head(&gCmdqThreadDispatchQueue);

	/* Reset overall context */
	memset(&gCmdqContext, 0x0, sizeof(struct ContextStruct));
	/* some fields has non-zero initial value */
	cmdq_core_set_log_level(LOG_LEVEL_LOG);	/*enable CMDQ_LOG on default */
	cmdq_core_reset_engine_struct();
	cmdq_core_reset_thread_struct();


	/* Create task pool */
	gCmdqContext.taskCache = kmem_cache_create(CMDQ_DRIVER_DEVICE_NAME "_task",
						   sizeof(struct TaskStruct),
						   __alignof__(struct TaskStruct),
						   SLAB_POISON | SLAB_HWCACHE_ALIGN | SLAB_RED_ZONE,
						   &cmdq_core_task_ctor);
	/* Initialize task lists */
	INIT_LIST_HEAD(&gCmdqContext.taskFreeList);
	INIT_LIST_HEAD(&gCmdqContext.taskActiveList);
	INIT_LIST_HEAD(&gCmdqContext.taskWaitList);
	INIT_WORK(&gCmdqContext.taskConsumeWaitQueueItem, cmdq_core_consume_waiting_list);

	/* Initialize writable address */
	INIT_LIST_HEAD(&gCmdqContext.writeAddrList);

	/* Initialize emergency buffer */
	cmdq_core_init_emergency_buffer();

	gCmdqContext.taskAutoReleaseWQ = create_singlethread_workqueue("cmdq_auto_release");
	gCmdqContext.taskConsumeWQ = create_singlethread_workqueue("cmdq_task");

	/* pre-allocate free tasks */
	for (index = 0; index < CMDQ_INIT_FREE_TASK_COUNT; index++) {
		pTask = cmdq_core_task_create();
		if (pTask) {
			mutex_lock(&gCmdqTaskMutex);
			list_add_tail(&(pTask->listEntry), &gCmdqContext.taskFreeList);
			mutex_unlock(&gCmdqTaskMutex);
		}
	}

#if 0
	/* cmdqCoreRegisterDebugRegDumpCB(testcase_regdump_begin, testcase_regdump_end); */
#endif

	/* Initialize MET for statistics */
	/* note that we don't need to uninit it. */
	CMDQ_PROF_INIT();
	cmdq_mmp_init();

	/* Initialize secure path context */
#ifdef CMDQ_SECURE_PATH_SUPPORT
	cmdqSecInitialize();
#endif

	gCmdqPrint.CmdqPrintWQ = create_singlethread_workqueue("cmdq_print_count_of_submitted_task");
	INIT_WORK(&gCmdqPrint.CmdqPrintWork, cmdq_core_run_print_log);
	return 0;
}


bool cmdq_core_is_valid_group(enum CMDQ_GROUP_ENUM engGroup)
{
	/* check range */
	if (engGroup < 0 || engGroup >= CMDQ_MAX_GROUP_COUNT)
		return false;

	return true;
}

int32_t cmdq_core_is_group_flag(enum CMDQ_GROUP_ENUM engGroup, uint64_t engineFlag)
{
	if (!cmdq_core_is_valid_group(engGroup))
		return false;


	if (gCmdqEngineGroupBits[engGroup] & engineFlag)
		return true;

	return false;
}

static inline uint32_t cmdq_core_get_task_timeout_cycle(struct ThreadStruct *pThread)
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
	CMDQ_MSG("clockOn:  0x%pf, dumpInfo: 0x%pf\n", clockOn, dumpInfo);
	CMDQ_MSG("resetEng: 0x%pf, clockOff: 0x%pf\n", resetEng, clockOff);

	pCallback = &(gCmdqGroupCallback[engGroup]);

	pCallback->clockOn = clockOn;
	pCallback->dumpInfo = dumpInfo;
	pCallback->resetEng = resetEng;
	pCallback->clockOff = clockOff;

	return 0;
}

int32_t cmdqCoreRegisterDebugRegDumpCB(CmdqDebugRegDumpBeginCB beginCB, CmdqDebugRegDumpEndCB endCB)
{
	CMDQ_VERBOSE("Register reg dump: begin=%p, end=%p\n", beginCB, endCB);
	gCmdqDebugCallback.beginDebugRegDump = beginCB;
	gCmdqDebugCallback.endDebugRegDump = endCB;
	return 0;
}

static void cmdq_core_release_task_unlocked(struct TaskStruct *pTask)
{
	pTask->taskState = TASK_STATE_IDLE;
	pTask->thread = CMDQ_INVALID_THREAD;

	if (pTask->profileData) {
		cmdq_core_free_hw_buffer(cmdq_dev_get(),
					 2 * sizeof(uint32_t), pTask->profileData,
					 pTask->profileDataPA);
		pTask->profileData = NULL;
		pTask->profileDataPA = 0;
	}

	if (pTask->regResults) {
		CMDQ_MSG("COMMAND: Free result buf VA:0x%p, PA:%pa\n", pTask->regResults,
			 &pTask->regResultsMVA);
		cmdq_core_free_hw_buffer(cmdq_dev_get(),
					 pTask->regCount * sizeof(pTask->regResults[0]),
					 pTask->regResults, pTask->regResultsMVA);
	}
	pTask->regResults = NULL;
	pTask->regResultsMVA = 0;
	pTask->regCount = 0;

	cmdq_task_free_task_command_buffer(pTask);
	cmdq_core_release_sec_metadata(pTask);

	/* remove from active/waiting list */
	list_del_init(&(pTask->listEntry));
	/* insert into free list. Currently we don't shrink free list. */
	list_add_tail(&(pTask->listEntry), &gCmdqContext.taskFreeList);
}

static void cmdq_core_release_task(struct TaskStruct *pTask)
{
	CMDQ_MSG("-->TASK: Release task structure 0x%p begin\n", pTask);

	mutex_lock(&gCmdqTaskMutex);

	cmdq_core_release_task_unlocked(pTask);

	mutex_unlock(&gCmdqTaskMutex);

	CMDQ_MSG("<--TASK: Release task structure end\n");
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
		pTask =
		    list_first_entry(&(gCmdqContext.taskFreeList), struct TaskStruct, listEntry);
		/* remove from free list */
		list_del_init(&(pTask->listEntry));
	}

	mutex_unlock(&gCmdqTaskMutex);

	return pTask;
}

static void cmdq_core_append_command_into_task(struct TaskStruct *pTask, uint32_t argA,
					       uint32_t argB)
{
	pTask->pCMDEnd[1] = argB;
	pTask->pCMDEnd[2] = argA;
	pTask->commandSize += 1 * CMDQ_INST_SIZE;
	pTask->pCMDEnd += 2;
}

static void cmdq_core_insert_backup_instruction(struct TaskStruct *pTask,
						const uint32_t regAddr,
						const dma_addr_t writeAddress,
						const enum CMDQ_DATA_REGISTER_ENUM valueRegId,
						const enum CMDQ_DATA_REGISTER_ENUM destRegId)
{
	uint32_t argA;
	int32_t subsysCode;

	/* register to read from */
	/* note that we force convert to physical reg address. */
	/* if it is already physical address, it won't be affected (at least on this platform) */
	argA = regAddr;
	/* argA = IO_VIRT_TO_PHYS(regAddr); */
	subsysCode = cmdq_subsys_from_phys_addr(argA);


	/*  */
	/* Load into 32-bit GPR (R0-R15) */
	cmdq_core_append_command_into_task(pTask, (CMDQ_CODE_READ << 24) |
	(argA & 0xffff) | ((subsysCode & 0x1f) << 16) | (2 << 21),	/* 0 1 0 */
					   valueRegId);


	/* Note that <MOVE> argB is 48-bit */
	/* so writeAddress is split into 2 parts */
	/* and we store address in 64-bit GPR (P0-P7) */
	cmdq_core_append_command_into_task(pTask, (CMDQ_CODE_MOVE << 24) |
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
					   ((writeAddress >> 32) & 0xffff) |
#endif
					   ((destRegId & 0x1f) << 16) | (4 << 21),
					   (uint32_t) writeAddress);

	/* write to memory */
	cmdq_core_append_command_into_task(pTask, (CMDQ_CODE_WRITE << 24) |
	(0 & 0xffff) | ((destRegId & 0x1f) << 16) | (6 << 21),	/* 1 0 */
					   valueRegId);

	CMDQ_VERBOSE("COMMAND: copy reg:0x%08x to phys:%pa, GPR(%d, %d)\n", argA, &writeAddress,
		     valueRegId, destRegId);

}

static void cmdq_core_reorder_task_array(struct ThreadStruct *pThread, int32_t prevID)
{
	int loop, nextID, searchLoop, searchID;
	int reorderCount = 0;

	nextID = prevID + 1;
	for (loop = 1; loop < (CMDQ_MAX_TASK_IN_THREAD - 1); loop++, nextID++) {
		if (nextID >= CMDQ_MAX_TASK_IN_THREAD)
			nextID = 0;

		if (NULL != pThread->pCurTask[nextID])
			break;

		searchID = nextID + 1;
		for (searchLoop = (loop + 1); searchLoop < CMDQ_MAX_TASK_IN_THREAD;
		     searchLoop++, searchID++) {
			if (searchID >= CMDQ_MAX_TASK_IN_THREAD)
				searchID = 0;

			if (NULL != pThread->pCurTask[searchID]) {
				pThread->pCurTask[nextID] = pThread->pCurTask[searchID];
				pThread->pCurTask[searchID] = NULL;
				CMDQ_LOG("WAIT: reorder slot %d to slot 0%d.\n",
					     searchID, nextID);
				if ((searchLoop - loop) > reorderCount)
					reorderCount = searchLoop - loop;

				break;
			}
		}

		if ((0x10000000 == pThread->pCurTask[nextID]->pCMDEnd[0]) &&
		    (0x00000008 == pThread->pCurTask[nextID]->pCMDEnd[-1])) {
			/* We reached the last task */
			break;
		}
	}

	pThread->nextCookie -= reorderCount;
	CMDQ_VERBOSE("WAIT: nextcookie minus %d.\n", reorderCount);
}

static int32_t cmdq_core_insert_read_reg_command(struct TaskStruct *pTask,
						       struct cmdqCommandStruct *pCommandDesc)
{
#define CMDQ_PROFILE_COMMAND

	int32_t status = 0;
	uint32_t extraBufferSize = 0;
	int i = 0;
	enum CMDQ_DATA_REGISTER_ENUM valueRegId;
	enum CMDQ_DATA_REGISTER_ENUM destRegId;
	enum CMDQ_EVENT_ENUM regAccessToken;
	uint32_t prependBufferSize = 0;
	const bool userSpaceRequest = cmdq_core_is_request_from_user_space(pTask->scenario);

	/* HACK: let blockSize >= commandSize + 1 * CMDQ_INST_SIZE to prevnet GCE stop wrongly */
	/* ALPS01676155, queue 2 tasks in same HW thread */
	/* .task_2: MVABase 0x0x435b3000, Size: 2944 */
	/* .task_3: MVABase 0x0x435b2000, Size: 4096 */
	/* THR_END = end of task_3
	 *         = 0x0x435b3000 = 0x0x435b2000 + 4096
	 *         = begin of task_2, so...GCE stop at begin of task_2's command and never exec task_2 & task_3 */
	const uint32_t reservedAppendBufferSize = 1 * CMDQ_INST_SIZE;

#ifdef CMDQ_PROFILE_COMMAND
	bool profileCommand = false;
#endif
	uint32_t copiedBeforeProfile = 0;


	/* calculate required buffer size */
	/* we need to consider {READ, MOVE, WRITE} for each register */
	/* and the SYNC in the begin and end */
	if (pTask->regCount)
		extraBufferSize = (3 * CMDQ_INST_SIZE * pTask->regCount) + (2 * CMDQ_INST_SIZE);
	else
		extraBufferSize = 0;



	CMDQ_VERBOSE("test %d, original command size = %d\n", __LINE__, pTask->commandSize);

#ifdef CMDQ_PROFILE_COMMAND
	/* do not insert profile command for trigger loop */
#ifdef CMDQ_PROFILE_COMMAND_TRIGGER_LOOP
	profileCommand = pTask &&
	    cmdq_core_should_profile(pTask->scenario) && (pTask->profileData != NULL);
#else
	profileCommand = pTask &&
	    cmdq_core_should_profile(pTask->scenario) &&
	    (pTask->loopCallback == NULL) && (pTask->profileData != NULL);

#endif

	if (profileCommand) {
		/* backup GPT at begin and end */
		extraBufferSize += (CMDQ_INST_SIZE * 3);

		/* insert after the first MARKER instruction */
		/* and first SYNC instruction (if any) */
		prependBufferSize += 2 * (CMDQ_INST_SIZE * 3);
		if (pTask->commandSize < prependBufferSize)
			prependBufferSize = 0;

	}
#endif

	status = cmdq_core_task_realloc_buffer_size(pTask,
						    pTask->commandSize + prependBufferSize +
						    extraBufferSize + reservedAppendBufferSize);
	if (status < 0) {
		CMDQ_ERR("finalize command buffer failed to realloc, pTask=0x%p, requireSize=%d\n",
			 pTask, pTask->commandSize + prependBufferSize + extraBufferSize);
		return status;
	}

	/* pCMDEnd start from beginning. Note it is out-of-sync with pTask->commandSize. */
	pTask->pCMDEnd = pTask->pVABase - 1;


#ifdef CMDQ_PROFILE_COMMAND
	if (profileCommand) {
		if (cmdq_core_should_enable_prefetch(pTask->scenario)) {
			/* HACK: */
			/* MARKER + WAIT_FOR_EOF */
			copiedBeforeProfile = 2 * CMDQ_INST_SIZE;
		} else if (pTask->loopCallback != NULL) {
#ifdef CMDQ_PROFILE_COMMAND_TRIGGER_LOOP
			/* HACK: insert profile instr after WAIT TE_EOF (command mode only) */
			/* note content of trigger loop varies according to platform */
			copiedBeforeProfile = 8 * CMDQ_INST_SIZE;
#endif
		} else {
			/* HACK: we copy the 1st "WAIT FOR EOF" instruction, */
			/* because this is the point where we start writing registers! */
			copiedBeforeProfile = 1 * CMDQ_INST_SIZE;
		}


		memcpy(pTask->pCMDEnd + 1, CMDQ_U32_PTR(pCommandDesc->pVABase),
		       copiedBeforeProfile);
		pTask->pCMDEnd =
		    pTask->pVABase + (copiedBeforeProfile / sizeof(pTask->pVABase[0])) - 1;

		/* now we start insert backup instructions */
		CMDQ_VERBOSE("test %d\n", __LINE__);
		do {
			CMDQ_VERBOSE("[BACKUP]va=%p, pa=%pa, task=%p\n", pTask->profileData,
				     &pTask->profileDataPA, pTask);

			cmdq_core_insert_backup_instruction(pTask,
							    CMDQ_APXGPT2_COUNT,
							    pTask->profileDataPA,
							    CMDQ_DATA_REG_JPEG,
							    CMDQ_DATA_REG_JPEG_DST);
		} while (0);
		/* this increases pTask->commandSize */
	}
#endif

	/* Copy the commands to our DMA buffer */
	if (false == userSpaceRequest) {
		CMDQ_VERBOSE("COMMAND: Copy kernel to 0x%p\n", CMDQ_U32_PTR(pTask->pVABase));
		memcpy(pTask->pCMDEnd + 1,
		       CMDQ_U32_PTR(pCommandDesc->pVABase) +
		       (copiedBeforeProfile / sizeof(CMDQ_U32_PTR(pCommandDesc->pVABase)[0])),
		       pCommandDesc->blockSize - copiedBeforeProfile);
	} else {
		CMDQ_VERBOSE("COMMAND: Copy user to 0x%p\n", pTask->pVABase);
		if (copy_from_user
		    (pTask->pCMDEnd + 1,
		     CMDQ_U32_PTR(pCommandDesc->pVABase) +
		     (copiedBeforeProfile / sizeof(CMDQ_U32_PTR(pCommandDesc->pVABase)[0])),
		     pCommandDesc->blockSize - copiedBeforeProfile)) {
			CMDQ_AEE("CMDQ",
				 "CRDISPATCH_KEY:CMDQ Fail to copy from user 0x%p, size:%d\n",
				 CMDQ_U32_PTR(pCommandDesc->pVABase), pCommandDesc->blockSize);
			return -ENOMEM;
		}
	}

	/* re-adjust pCMDEnd according to commandSize */
	pTask->pCMDEnd = pTask->pVABase + (pTask->commandSize / sizeof(pTask->pVABase[0])) - 1;

	CMDQ_VERBOSE("test %d, CMDEnd=%p, base=%p, cmdSize=%d\n", __LINE__, pTask->pCMDEnd,
		     pTask->pVABase, pTask->commandSize);

	/* if no read request, no post-process needed. */
	if (0 == pTask->regCount && extraBufferSize == 0)
		return 0;

	/* move EOC+JUMP to the new end */
	memcpy(pTask->pCMDEnd + 1 - 4 + (extraBufferSize / sizeof(pTask->pCMDEnd[0])),
	       &pTask->pCMDEnd[-3], 2 * CMDQ_INST_SIZE);

	/* start from old EOC (replace it) */
	pTask->pCMDEnd -= 4;

	if (pTask->regCount) {
		CMDQ_VERBOSE("COMMAND:allocate register output section\n");
		/* allocate register output section */
		BUG_ON(pTask->regResults);
		pTask->regResults = cmdq_core_alloc_hw_buffer(cmdq_dev_get(),
							      pTask->regCount *
							      sizeof(pTask->regResults[0]),
							      &pTask->regResultsMVA,
							      GFP_KERNEL);
		CMDQ_MSG("COMMAND: result buf VA:0x%p, PA:%pa\n", pTask->regResults,
			 &pTask->regResultsMVA);


		/* allocate GPR resource */
		cmdq_core_get_reg_id_from_hwflag(pTask->engineFlag, &valueRegId, &destRegId,
						 &regAccessToken);

		/* use SYNC TOKEN to make sure only 1 thread access at a time */
		/* bit 0-11: wait_value */
		/* bit 15: to_wait, true */
		/* bit 31: to_update, true */
		/* bit 16-27: update_value */
		cmdq_core_append_command_into_task(pTask, (CMDQ_CODE_WFE << 24) |
			regAccessToken, ((1 << 31) | (1 << 15) | 1)	/* wait and clear */
		    );

		for (i = 0; i < pTask->regCount; ++i) {
			cmdq_core_insert_backup_instruction(pTask,
							    CMDQ_U32_PTR(pCommandDesc->
									 regRequest.
									 regAddresses)[i],
							    pTask->regResultsMVA +
							    (i *
							     sizeof(pTask->regResults[0])),
							    valueRegId, destRegId);
		}

		cmdq_core_append_command_into_task(pTask, (CMDQ_CODE_WFE << 24) |
			regAccessToken, ((1 << 31) | (1 << 16))	/* set directly */
		    );
	}
#ifdef CMDQ_PROFILE_COMMAND
	if (profileCommand) {
		cmdq_core_insert_backup_instruction(pTask,
						    CMDQ_APXGPT2_COUNT,
						    pTask->profileDataPA + sizeof(uint32_t),
						    CMDQ_DATA_REG_JPEG,
						    CMDQ_DATA_REG_JPEG_DST);
	}
	/* this increases pTask->commandSize */
#endif

	/* move END to copied EOC+JUMP */
	pTask->pCMDEnd += 4;

#ifdef CMDQ_PROFILE_COMMAND_TRIGGER_LOOP
	/* revise jump */
	if (pTask && pTask->loopCallback) {
		if (((pTask->pCMDEnd[0] & 0xFF000000) >> 24) == CMDQ_CODE_JUMP) {
			int32_t old_jump = pTask->pCMDEnd[-1];
			int32_t new_jump = old_jump - (CMDQ_INST_SIZE * 6);

			pTask->pCMDEnd[-1] = new_jump;
		} else {
			CMDQ_ERR("trigger loop error since profile\n");
		}
	}
#endif

	/* make sure instructions are really in DRAM */
	smp_mb();		/*coding style requires to comment after smb_mb to explain why need memory_barrier */

#ifdef CMDQ_PROFILE_COMMAND_TRIGGER_LOOP
	if (pTask && pTask->loopCallback && cmdq_core_should_profile(pTask->scenario))
		cmdqCoreDebugDumpCommand(pTask);

#endif

	CMDQ_MSG("COMMAND: size = %d, end = 0x%p\n", pTask->commandSize, pTask->pCMDEnd);

	cmdq_core_verfiy_command_end(pTask);

	return status;
}


static struct TaskStruct *cmdq_core_acquire_task(struct cmdqCommandStruct *pCommandDesc,
						       CmdqInterruptCB loopCB,
						       unsigned long loopData)
{
	struct TaskStruct *pTask = NULL;
	int32_t status = 0;

	CMDQ_MSG("-->TASK: acquire task begin CMD: 0x%p, size: %d, Eng: 0x%016llx\n",
		 CMDQ_U32_PTR(pCommandDesc->pVABase), pCommandDesc->blockSize,
		 pCommandDesc->engineFlag);
	CMDQ_PROF_START(current->pid, __func__);

	pTask = cmdq_core_find_free_task();
	do {
		if (NULL == pTask) {
			CMDQ_AEE("CMDQ", "Can't acquire task info\n");
			break;
		}
		/* initialize field values */
		pTask->scenario = pCommandDesc->scenario;
		pTask->priority = pCommandDesc->priority;
		pTask->engineFlag = pCommandDesc->engineFlag;
		pTask->privateData = pCommandDesc->privateData;
		pTask->loopCallback = loopCB;
		pTask->loopData = loopData;
		pTask->taskState = TASK_STATE_WAITING;
		pTask->reorder = 0;
		pTask->thread = CMDQ_INVALID_THREAD;
		pTask->irqFlag = 0x0;

		/* secure exec data */
		pTask->secData.isSecure = pCommandDesc->secData.isSecure;
		pTask->secData.enginesNeedDAPC = pCommandDesc->secData.enginesNeedDAPC;
		pTask->secData.enginesNeedPortSecurity =
		    pCommandDesc->secData.enginesNeedPortSecurity;
#ifdef CMDQ_SECURE_PATH_SUPPORT
		pTask->secData.secMode = pCommandDesc->secData.secMode;
#endif
		pTask->secData.addrMetadataCount = pCommandDesc->secData.addrMetadataCount;
		/* pTask->secData.addrMetadatas = pCommandDesc->secData.addrMetadatas; */
		status = cmdq_core_alloc_sec_metadata(pCommandDesc, pTask);
		pTask->secData.srcHandle = pCommandDesc->secData.srcHandle;
		pTask->secData.dstHandle = pCommandDesc->secData.dstHandle;
		/* profile data */
		if (cmdq_core_should_profile(pTask->scenario)) {
			pTask->profileData = cmdq_core_alloc_hw_buffer(cmdq_dev_get(),
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

		pTask->commandSize = pCommandDesc->blockSize;
		pTask->regCount = pCommandDesc->regRequest.count;

		/* store caller info for debug */
		if (current) {
			pTask->callerPid = current->pid;
			memcpy(pTask->callerName, current->comm, sizeof(current->comm));
		}

		if (0 > status) {
			/* raise AEE first */
			CMDQ_AEE("CMDQ", "Can't alloc sec metadata buffer\n");

			/* then release task */
			cmdq_core_release_task(pTask);
			pTask = NULL;
			break;
		}

		status = cmdq_core_insert_read_reg_command(pTask, pCommandDesc);
		if (0 > status) {
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

		pTask->submit = sched_clock();

		/* add to waiting list, keep it sorted by priority */
		/* so that we add high-priority tasks first. */
		list_for_each(p, &gCmdqContext.taskWaitList) {
			taskEntry = list_entry(p, struct TaskStruct, listEntry);
			/* keep the list sorted. */
			/* higher priority tasks are inserted in front of the queue */
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

bool cmdqIsValidTaskPtr(void *pTask)
{
	struct TaskStruct *ptr = NULL;
	struct list_head *p = NULL;
	bool ret = false;

	mutex_lock(&gCmdqTaskMutex);

	list_for_each(p, &gCmdqContext.taskActiveList) {
		ptr = list_entry(p, struct TaskStruct, listEntry);
		if (ptr == pTask && TASK_STATE_IDLE != ptr->taskState) {
			ret = true;
			break;
		}
	}

	list_for_each(p, &gCmdqContext.taskWaitList) {
		ptr = list_entry(p, struct TaskStruct, listEntry);
		if (ptr == pTask && TASK_STATE_WAITING == ptr->taskState) {
			ret = true;
			break;
		}
	}

	mutex_unlock(&gCmdqTaskMutex);
	return ret;
}

static void cmdq_core_enable_common_clock_locked(const bool enable, const uint64_t engineFlag,
						 enum CMDQ_SCENARIO_ENUM scenario)
{
	/* CMDQ(GCE) clock */
	if (enable) {
		CMDQ_VERBOSE("[CLOCK] Enable CMDQ(GCE) Clock test=%d SMI %d\n",
			     atomic_read(&gCmdqThreadUsage), atomic_read(&gSMIThreadUsage));

		if (0 == atomic_read(&gCmdqThreadUsage)) {
			/* CMDQ init flow: */
			/* 1. clock-on */
			/* 2. reset all events */
			cmdq_core_enable_gce_clock_locked_impl(enable);

			cmdq_core_reset_hw_events();
		}
		atomic_inc(&gCmdqThreadUsage);
		/* SMI related threads common clock enable, excluding display scenario on his own */
		if (!cmdq_core_is_disp_scenario(scenario) && (scenario != CMDQ_SCENARIO_TRIGGER_LOOP)) {
			if (0 == atomic_read(&gSMIThreadUsage)) {
				CMDQ_VERBOSE("[CLOCK] SMI clock enable %d\n", scenario);
				cmdq_core_enable_common_clock_locked_impl(enable);
			}
			atomic_inc(&gSMIThreadUsage);
		}

	} else {
		atomic_dec(&gCmdqThreadUsage);
		CMDQ_VERBOSE("[CLOCK] Disable CMDQ(GCE) Clock test=%d SMI %d\n",
			     atomic_read(&gCmdqThreadUsage), atomic_read(&gSMIThreadUsage));
		if (0 >= atomic_read(&gCmdqThreadUsage))
			cmdq_core_enable_gce_clock_locked_impl(enable);


		/* SMI related threads common clock enable, excluding display scenario on his own */
		if (!cmdq_core_is_disp_scenario(scenario) && (scenario != CMDQ_SCENARIO_TRIGGER_LOOP)) {
			atomic_dec(&gSMIThreadUsage);

			if (0 >= atomic_read(&gSMIThreadUsage)) {
				CMDQ_VERBOSE("[CLOCK] SMI clock disable %d\n", scenario);
				cmdq_core_enable_common_clock_locked_impl(enable);
			}
		}
	}
}

static uint64_t cmdq_core_get_actual_engine_flag_for_enable_clock(uint64_t engineFlag,
								  int32_t thread)
{
	struct EngineStruct *pEngine;
	uint64_t engines;
	int32_t index;

	pEngine = gCmdqContext.engine;
	engines = 0;
	for (index = 0; index < CMDQ_MAX_ENGINE_COUNT; index++) {
		if (engineFlag & (1LL << index)) {
			if (pEngine[index].userCount <= 0) {
				pEngine[index].currOwner = thread;
				engines |= (1LL << index);
			}

			pEngine[index].userCount++;
		}
	}
	return engines;
}

static int32_t gCmdqISPClockCounter;

static void cmdq_core_enable_clock(uint64_t engineFlag,
				   int32_t thread,
				   uint64_t engineMustEnableClock, enum CMDQ_SCENARIO_ENUM scenario)
{
	const uint64_t engines = engineMustEnableClock;
	int32_t index;
	struct CmdqCBkStruct *pCallback;
	int32_t status;

	CMDQ_VERBOSE("-->CLOCK: Enable flag 0x%llx thread %d begin, mustEnable: 0x%llx(0x%llx)\n",
		     engineFlag, thread, engineMustEnableClock, engines);

	/* enable fundamental clocks if needed */
	cmdq_core_enable_common_clock_locked(true, engineFlag, scenario);

	pCallback = gCmdqGroupCallback;

	/* ISP special check: Always call ISP on/off if this task */
	/* involves ISP. Ignore the ISP HW flags. */
	if (cmdq_core_is_group_flag(CMDQ_GROUP_ISP, engineFlag)) {
		CMDQ_VERBOSE("CLOCK: enable group %d clockOn\n", CMDQ_GROUP_ISP);

		if (NULL == pCallback[CMDQ_GROUP_ISP].clockOn) {
			CMDQ_ERR("CLOCK: enable group %d clockOn func NULL\n", CMDQ_GROUP_ISP);
		} else {
			status =
			    pCallback[CMDQ_GROUP_ISP].clockOn(gCmdqEngineGroupBits[CMDQ_GROUP_ISP] &
							      engineFlag);

#if 1
			++gCmdqISPClockCounter;
#endif

			if (status < 0)
				CMDQ_ERR("CLOCK: enable group %d clockOn failed\n", CMDQ_GROUP_ISP);

		}
	}

	for (index = CMDQ_MAX_GROUP_COUNT - 1; index >= 0; --index) {
		/* note that DISPSYS controls their own clock on/off */
		if (CMDQ_GROUP_DISP == index)
			continue;

		/* note that ISP is per-task on/off, not per HW flag */
		if (CMDQ_GROUP_ISP == index)
			continue;


		if (cmdq_core_is_group_flag((enum CMDQ_GROUP_ENUM)index, engines)) {
			CMDQ_MSG("CLOCK: enable group %d clockOn\n", index);
			if (NULL == pCallback[index].clockOn) {
				CMDQ_ERR("CLOCK: enable group %d clockOn func NULL\n", index);
				continue;
			}
			status = pCallback[index].clockOn(gCmdqEngineGroupBits[index] & engines);
			if (status < 0)
				CMDQ_ERR("CLOCK: enable group %d clockOn failed\n", index);

		}
	}

	CMDQ_MSG("<--CLOCK: Enable hardware clock end\n");
}

/*
	traverse gCmdqContext.taskWaitList and find the first task which has engine conflict with current task
	if pFirstWaitTask is null, then this task can acquire HW thread.
	if pFirstWaitTask is not null, then check whether pFirstWaitTask's secure flag is same as current task's flag
	true for can
	false for can not
*/
static const int32_t cmdq_core_can_start_to_acquire_HW_thread_unlocked(const uint64_t
										   engineFlag,
										   const bool isSecure)
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
		if (NULL != pTempTask && (engineFlag & (pTempTask->engineFlag))) {
			pFirstWaitingTask = pTempTask;
			break;
		}
	}

	do {
		if (NULL == pFirstWaitingTask) {
			/* no waiting task with overlape engine, go to dispath thread */
			break;
		}

		preferSecurePath = pFirstWaitingTask->secData.isSecure;

		if (preferSecurePath == isSecure) {
			/* same security path as first waiting task, go to start to thread dispatch */
			longMsg[0] = '\0';
			msgOffset = 0;
			msgMAXSize = CMDQ_LONGSTRING_MAX;
			cmdqCoreLongString(false, longMsg, &msgOffset, &msgMAXSize,
					   "THREAD: is sec(%d, eng:0x%llx) as first waiting task",
					   isSecure, engineFlag);
			cmdqCoreLongString(false, longMsg, &msgOffset, &msgMAXSize,
					   "(0x%p, eng:0x%llx), start thread dispatch.\n",
					   pFirstWaitingTask, pFirstWaitingTask->engineFlag);
			if (msgOffset > 0) {
				/* print message */
				CMDQ_MSG("%s", longMsg);
			}
			break;
		}

		CMDQ_VERBOSE("THREAD: is not the first waiting task(0x%p), yield.\n",
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
 *     isSecure: [IN] secure path
 *     *pThreadOut:
 *         [IN] prefer thread. please pass CMDQ_INVALID_THREAD if no prefere
 *         [OUT] dispatch thread result
 * Return:
 *     0 for success; else the error code is returned
 */

static bool cmdq_core_check_engine_conflict_for_thread_dispatch_locked(const uint64_t
									     engineFlag,
									     bool forceLog,
									     const bool isSecure,
									     int32_t *pThreadOut)
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
	free = (CMDQ_INVALID_THREAD == thread) ? 0xFFFFFFFF : 0xFFFFFFFF & (~(0x1 << thread));

	/* check if engine conflict */
	for (index = 0; ((index < CMDQ_MAX_ENGINE_COUNT) && (engine != 0)); index++) {
		if (engine & (0x1LL << index)) {
			if (CMDQ_INVALID_THREAD == pEngine[index].currOwner) {
				continue;
			} else if (CMDQ_INVALID_THREAD == thread) {
				thread = pEngine[index].currOwner;
				free &= ~(0x1 << thread);
			} else if (thread != pEngine[index].currOwner) {
				/* Partial HW occupied by different threads, */
				/* we need to wait. */
				if (forceLog) {
					longMsg[0] = '\0';
					msgOffset = 0;
					msgMAXSize = CMDQ_LONGSTRING_MAX;
					cmdqCoreLongString(true, longMsg, &msgOffset, &msgMAXSize,
							   "THREAD: try locate on thread %d but engine %d",
							   thread, index);
					cmdqCoreLongString(true, longMsg, &msgOffset, &msgMAXSize,
							   " also occupied by thread %d, secure:%d\n",
							   pEngine[index].currOwner, isSecure);
					if (msgOffset > 0) {
						/* print message */
						CMDQ_LOG("%s", longMsg);
					}
				} else {
					longMsg[0] = '\0';
					msgOffset = 0;
					msgMAXSize = CMDQ_LONGSTRING_MAX;
					cmdqCoreLongString(false, longMsg, &msgOffset, &msgMAXSize,
							   "THREAD: try locate on thread %d but engine %d",
							   thread, index);
					cmdqCoreLongString(false, longMsg, &msgOffset, &msgMAXSize,
							   " also occupied by thread %d, secure:%d\n",
							   pEngine[index].currOwner, isSecure);
					if (msgOffset > 0) {
						/* print message */
						CMDQ_VERBOSE("%s", longMsg);
					}
				}

				isEngineConflict = true;	/* engine conflict! */
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
						     enum CMDQ_SCENARIO_ENUM scenario,
						     bool forceLog, const bool isSecure)
{
	struct ThreadStruct *pThread;
	unsigned long flagsExecLock;
	int32_t index;
	int32_t thread;
	bool isEngineConflict;

	pThread = gCmdqContext.thread;

	do {
		CMDQ_VERBOSE("THREAD: find a free thread, engine:0x%llx, scenario: %d, secure:%d\n",
			     engineFlag, scenario, isSecure);

		/* start to dispatch? */
		/* note we should not favor secure or normal path, */
		/* traverse waiting list to decide that we should dispatch thread to secure or normal path */
		if (0 > cmdq_core_can_start_to_acquire_HW_thread_unlocked(engineFlag, isSecure)) {
			thread = CMDQ_INVALID_THREAD;
			break;
		}

		/* it's okey to dispatch thread, */
		/* use scenario and pTask->secure to get default thread */
		thread =
		    cmdq_core_get_thread_index_from_scenario_and_secure_data(scenario, isSecure);

		/* check if engine conflict happened except DISP scenario */
		isEngineConflict = false;
		if (false == cmdq_core_is_disp_scenario(scenario)) {
			isEngineConflict =
			    cmdq_core_check_engine_conflict_for_thread_dispatch_locked(engineFlag,
										       forceLog,
										       isSecure,
										       &thread);
		}
		CMDQ_VERBOSE("THREAD: isEngineConflict:%d, thread:%d\n", isEngineConflict, thread);

#if 1				/* TODO: secure path proting */
		/* because all thread are pre-dispatched, there 2 outcome of engine conflict check:
		 * 1. pre-dispatched secure thread, and no conflict with normal path
		 * 2. pre-dispatched secure thread, but conflict with normal/anothor secure path
		 *
		 * no need to check get normal thread in secure path
		 */

		/* ensure not dispatch secure thread to normal task */
		if ((false == isSecure) && (true == cmdq_core_is_a_secure_thread(thread))) {
			thread = CMDQ_INVALID_THREAD;
			isEngineConflict = true;
			break;
		}
#endif
		/* no enfine conflict with running thread, AND used engines have no owner */
		/* try to find a free thread */
		if ((false == isEngineConflict) && (CMDQ_INVALID_THREAD == thread)) {

			/* thread 0 - CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT
			   are preserved for DISPSYS for high-prio threads */
			const bool isDisplayThread = CMDQ_THR_PRIO_DISPLAY_TRIGGER < thread_prio;
			int startIndex = isDisplayThread ? 0 : CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT;
			int endIndex = isDisplayThread ?
			    CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT : CMDQ_MAX_THREAD_COUNT;

			for (index = startIndex; index < endIndex; ++index) {
				spin_lock_irqsave(&gCmdqExecLock, flagsExecLock);

				if ((0 == pThread[index].taskCount)
				    && (1 == pThread[index].allowDispatching)) {

					CMDQ_VERBOSE
					    ("THREAD: dispatch to thread %d, taskCount:%d, allowDispatching:%d\n",
					     index, pThread[index].taskCount,
					     pThread[index].allowDispatching);

					thread = index;
					pThread[index].allowDispatching = 0;
					spin_unlock_irqrestore(&gCmdqExecLock, flagsExecLock);
					break;
				}

				spin_unlock_irqrestore(&gCmdqExecLock, flagsExecLock);
			}
		}

		/* no thread available now, wait for it */
		if (CMDQ_INVALID_THREAD == thread)
			break;

		/* Make sure the found thread has enough space for the task; */
		/* ThreadStruct->pCurTask has size limitation. */
		if (CMDQ_MAX_TASK_IN_THREAD <= pThread[thread].taskCount) {
			if (!bCountEnable)
				cmdq_core_enable_print_log(true);
			if (forceLog) {
				gTaskBuffer[thread] = pThread[thread].taskCount;
				CMDQ_MSG("THREAD: thread %d task count = %d full\n",
					 thread, pThread[thread].taskCount);
			} else {
				CMDQ_VERBOSE("THREAD: thread %d task count = %d full\n",
					     thread, pThread[thread].taskCount);
			}

			thread = CMDQ_INVALID_THREAD;
		}

		if (pThread[thread].
		    pCurTask[pThread[thread].nextCookie % CMDQ_MAX_TASK_IN_THREAD] != NULL) {
			if (!bCountEnable)
				cmdq_core_enable_print_log(true);
			if (forceLog) {
				gOccupied[thread] = true;
				gNextCookieOccupied[thread] = pThread[thread].nextCookie;
				CMDQ_MSG("THREAD: thread %d nextCookie = %d already has task\n",
					 thread, pThread[thread].nextCookie);
			} else {
				CMDQ_VERBOSE("THREAD: thread %d nextCookie = %d already has task\n",
					     thread, pThread[thread].nextCookie);
			}

			thread = CMDQ_INVALID_THREAD;
		}

	} while (0);

	return thread;
}

static int32_t cmdq_core_acquire_thread(uint64_t engineFlag,
					      enum CMDQ_HW_THREAD_PRIORITY_ENUM thread_prio,
					      enum CMDQ_SCENARIO_ENUM scenario, bool forceLog,
					      const bool isSecure)
{
	unsigned long flags;
	int32_t thread;
	uint64_t engineMustEnableClock;

	CMDQ_PROF_START(current->pid, __func__);

	do {
		mutex_lock(&gCmdqClockMutex);
		spin_lock_irqsave(&gCmdqThreadLock, flags);

		thread =
		    cmdq_core_find_a_free_HW_thread(engineFlag, thread_prio, scenario, forceLog,
						    isSecure);

		if (CMDQ_INVALID_THREAD != thread) {
			/* get actual engine flag. Each bit represents a engine must enable clock. */
			engineMustEnableClock =
			    cmdq_core_get_actual_engine_flag_for_enable_clock(engineFlag, thread);
		}

		spin_unlock_irqrestore(&gCmdqThreadLock, flags);

		/* enable clock */
		if (CMDQ_INVALID_THREAD != thread)
			cmdq_core_enable_clock(engineFlag, thread, engineMustEnableClock, scenario);

		mutex_unlock(&gCmdqClockMutex);
	} while (0);

	CMDQ_PROF_END(current->pid, __func__);

	return thread;
}

static uint64_t cmdq_core_get_not_used_engine_flag_for_disable_clock(const uint64_t engineFlag)
{
	struct EngineStruct *pEngine;
	uint64_t enginesNotUsed;
	int32_t index;

	enginesNotUsed = 0LL;
	pEngine = gCmdqContext.engine;

	for (index = 0; index < CMDQ_MAX_ENGINE_COUNT; index++) {
		if (engineFlag & (1LL << index)) {
			pEngine[index].userCount--;
			if (pEngine[index].userCount <= 0) {
				enginesNotUsed |= (1LL << index);
				pEngine[index].currOwner = CMDQ_INVALID_THREAD;
			}
		}
	}
	CMDQ_VERBOSE("%s, enginesNotUsed:0x%llx\n", __func__, enginesNotUsed);
	return enginesNotUsed;
}

static void cmdq_core_disable_clock(uint64_t engineFlag,
				    const uint64_t enginesNotUsed, enum CMDQ_SCENARIO_ENUM scenario)
{
	int32_t index;
	int32_t status;
	struct CmdqCBkStruct *pCallback;

	CMDQ_VERBOSE("-->CLOCK: Disable hardware clock 0x%llx begin, enginesNotUsed 0x%llx\n",
		     engineFlag, enginesNotUsed);

	pCallback = gCmdqGroupCallback;

	/* ISP special check: Always call ISP on/off if this task */
	/* involves ISP. Ignore the ISP HW flags ref count. */
	if (cmdq_core_is_group_flag(CMDQ_GROUP_ISP, engineFlag)) {
		CMDQ_VERBOSE("CLOCK: disable group %d clockOff\n", CMDQ_GROUP_ISP);
		if (NULL == pCallback[CMDQ_GROUP_ISP].clockOff) {
			CMDQ_ERR("CLOCK: disable group %d clockOff func NULL\n", CMDQ_GROUP_ISP);
		} else {
			status =
			    pCallback[CMDQ_GROUP_ISP].clockOff(gCmdqEngineGroupBits[CMDQ_GROUP_ISP]
							       & engineFlag);

#if 1
			--gCmdqISPClockCounter;
			if (gCmdqISPClockCounter != 0)
				CMDQ_VERBOSE("CLOCK: ISP clockOff cnt=%d\n", gCmdqISPClockCounter);

#endif

			if (status < 0) {
				CMDQ_ERR("CLOCK: disable group %d clockOff failed\n",
					 CMDQ_GROUP_ISP);
			}
		}
	}
	/* Turn off unused engines */
	for (index = 0; index < CMDQ_MAX_GROUP_COUNT; ++index) {
		/* note that DISPSYS controls their own clock on/off */
		if (CMDQ_GROUP_DISP == index)
			continue;

		/* note that ISP is per-task on/off, not per HW flag */
		if (CMDQ_GROUP_ISP == index)
			continue;


		if (cmdq_core_is_group_flag((enum CMDQ_GROUP_ENUM)index, enginesNotUsed)) {
			CMDQ_MSG("CLOCK: Disable engine group %d flag=0x%llx clockOff\n", index,
				 enginesNotUsed);
			if (NULL == pCallback[index].clockOff) {
				CMDQ_ERR("CLOCK: Disable engine group %d clockOff func NULL\n",
					 index);
				continue;
			}
			status =
			    pCallback[index].clockOff(gCmdqEngineGroupBits[index] & enginesNotUsed);
			if (status < 0)
				CMDQ_ERR("CLOCK: Disable engine group %d clock failed\n", index);

		}
	}

	/* disable fundamental clocks if needed */
	cmdq_core_enable_common_clock_locked(false, engineFlag, scenario);

	CMDQ_MSG("<--CLOCK: Disable hardware clock 0x%llx end\n", engineFlag);
}

void cmdq_core_add_consume_task(void)
{
	if (!work_pending(&gCmdqContext.taskConsumeWaitQueueItem)) {
		CMDQ_PROF_MMP(cmdq_mmp_get_event()->consume_add, MMProfileFlagPulse, 0, 0);
		queue_work(gCmdqContext.taskConsumeWQ, &gCmdqContext.taskConsumeWaitQueueItem);
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
	engineNotUsed = cmdq_core_get_not_used_engine_flag_for_disable_clock(engineFlag);
	pTask->thread = CMDQ_INVALID_THREAD;

	spin_unlock_irqrestore(&gCmdqThreadLock, flags);

	/* clock off */
	cmdq_core_disable_clock(engineFlag, engineNotUsed, pTask->scenario);

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
		if (cmdq_core_is_group_flag((enum CMDQ_GROUP_ENUM)index, engines)) {
			CMDQ_MSG("Reset engine group %d clock\n", index);
			if (NULL == pCallback[index].resetEng) {
				CMDQ_ERR("Reset engine group %d clock func NULL\n", index);
				continue;
			}
			status =
			    pCallback[index].resetEng(gCmdqEngineGroupBits[index] & engineFlag);
			if (status < 0)
				CMDQ_ERR("Reset engine group %d clock failed\n", index);

		}
	}

	CMDQ_MSG("Reset hardware engine end\n");
}

int32_t cmdq_core_suspend_HW_thread(int32_t thread)
{
	int32_t loop = 0;
	uint32_t enabled = 0;

	if (CMDQ_INVALID_THREAD == thread) {
		CMDQ_ERR("suspend invalid thread\n");
		return -EFAULT;
	}
	/* write suspend bit */
	CMDQ_REG_SET32(CMDQ_THR_SUSPEND_TASK(thread), 0x01);

	/* check if the thread is already disabled. */
	/* if already disabled, treat as suspend successful but print error log */
	enabled = CMDQ_REG_GET32(CMDQ_THR_ENABLE_TASK(thread));
	if (0 == (0x01 & enabled)) {
		CMDQ_LOG("[WARNING] thread %d suspend not effective, enable=%d\n", thread, enabled);
		return 0;
	}

	loop = 0;
	while (0x0 == (CMDQ_REG_GET32(CMDQ_THR_CURR_STATUS(thread)) & 0x2)) {
		if (loop > CMDQ_MAX_LOOP_COUNT) {
			CMDQ_AEE("CMDQ", "Suspend HW thread %d failed\n", thread);
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
	 /**/ smp_mb();		/*coding style requires to comment after smb_mb to explain why need memory_barrier */
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

static uint32_t *cmdq_core_get_pc(const struct TaskStruct *pTask, uint32_t thread,
					uint32_t insts[4])
{
	long currPC = 0L;
	uint8_t *pInst = NULL;

	insts[0] = 0;
	insts[1] = 0;
	insts[2] = 0;
	insts[3] = 0;

	if ((NULL == pTask) || (NULL == pTask->pVABase) || (CMDQ_INVALID_THREAD == thread)) {
		CMDQ_ERR
		    ("get pc failed since invalid param, pTask %p, pTask->pVABase:%p, thread:%d\n",
		     pTask, pTask->pVABase, thread);
		return NULL;
	}

	if (true == pTask->secData.isSecure) {
		/* be carefull it dose not allow normal access to secure threads' register */
		return NULL;
	}

	currPC = CMDQ_AREG_TO_PHYS(CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread)));
	pInst = (uint8_t *) pTask->pVABase + (currPC - pTask->MVABase);

	/*PABase:0x%08x\n", thread, currPC, pTask->pVABase, pTask->MVABase); */
	if (((uint8_t *) pTask->pVABase <= pInst) && (pInst <= (uint8_t *) pTask->pCMDEnd)) {
		if (pInst != (uint8_t *) pTask->pCMDEnd) {
			/* If PC points to start of pCMD, */
			/* - 8 causes access violation */
			/* insts[0] = CMDQ_REG_GET32(pInst - 8); */
			/* insts[1] = CMDQ_REG_GET32(pInst - 4); */
			insts[2] = CMDQ_REG_GET32(pInst + 0);
			insts[3] = CMDQ_REG_GET32(pInst + 4);
		} else {
			/* insts[0] = CMDQ_REG_GET32(pInst - 16); */
			/* insts[1] = CMDQ_REG_GET32(pInst - 12); */
			insts[2] = CMDQ_REG_GET32(pInst - 8);
			insts[3] = CMDQ_REG_GET32(pInst - 4);
		}
	} else {
		/* invalid PC address */
		return NULL;
	}

	return (uint32_t *) pInst;
}

uint32_t cmdq_core_subsys_to_reg_addr(uint32_t argA)
{
	const uint32_t subsysBit = cmdq_core_get_subsys_LSB_in_argA();
	const int32_t subsys_id = (argA & CMDQ_ARG_A_SUBSYS_MASK) >> subsysBit;
	const uint32_t offset = (argA & 0xFFFF);
	uint32_t base_addr = 0;

#undef DECLARE_CMDQ_SUBSYS
#define DECLARE_CMDQ_SUBSYS(addr, id, grp, base) case id: base_addr = addr; break;
	switch (subsys_id) {
#include "cmdq_subsys.h"
	}
#undef DECLARE_CMDQ_SUBSYS

	return (base_addr << subsysBit) | offset;
}

static const char *cmdq_core_parse_op(uint32_t opCode)
{
	switch (opCode) {
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

static uint32_t *cmdq_core_dump_pc(const struct TaskStruct *pTask, int thread, const char *tag)
{
	uint32_t *pcVA = NULL;
	uint32_t insts[4] = { 0 };
	char parsedInstruction[128] = { 0 };

	pcVA = cmdq_core_get_pc(pTask, thread, insts);
	if (pcVA) {
		const uint32_t op = (insts[3] & 0xFF000000) >> 24;

		cmdq_core_parse_instruction(pcVA, parsedInstruction, sizeof(parsedInstruction));
		CMDQ_LOG("[%s]Thread %d PC(VA): 0x%p, 0x%08x:0x%08x => %s",
			 tag, thread, pcVA, insts[2], insts[3], parsedInstruction);

		/* for WFE, we specifically dump the event value */
		if (op == CMDQ_CODE_WFE) {
			uint32_t regValue = 0;
			const uint32_t eventID = 0x3FF & insts[3];

			CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_ID, eventID);
			regValue = CMDQ_REG_GET32(CMDQ_SYNC_TOKEN_VAL);
			CMDQ_LOG("[%s]CMDQ_SYNC_TOKEN_VAL of %s is %d\n", tag,
				 cmdq_core_get_event_name(eventID), regValue);
		}
	} else {
		if (true == pTask->secData.isSecure) {
			CMDQ_LOG("[%s]Thread %d PC(VA): HIDDEN INFO since is it's secure thread\n",
				 tag, thread);
		} else {
			CMDQ_LOG("[%s]Thread %d PC(VA): Not available\n", tag, thread);
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
	CMDQ_LOG("[%s]IRQ flag:0x%08x, Execing:%d, Exec Thread:%d, CMDQ_CURR_LOADED_THR: 0x%08x\n",
		 tag,
		 CMDQ_REG_GET32(CMDQ_CURR_IRQ_STATUS),
		 (0x80000000 & value[0]) ? 1 : 0, coreExecThread, value[0]);
	CMDQ_LOG("[%s]CMDQ_THR_EXEC_CYCLES:0x%08x, CMDQ_THR_TIMER:0x%08x, CMDQ_BUS_CTRL:0x%08x\n",
		 tag, value[1], value[2], value[3]);
	CMDQ_LOG("[%s]CMDQ_DEBUG_1: 0x%08x\n", tag, CMDQ_REG_GET32((GCE_BASE_VA + 0xF0)));
	CMDQ_LOG("[%s]CMDQ_DEBUG_2: 0x%08x\n", tag, CMDQ_REG_GET32((GCE_BASE_VA + 0xF4)));
	CMDQ_LOG("[%s]CMDQ_DEBUG_3: 0x%08x\n", tag, CMDQ_REG_GET32((GCE_BASE_VA + 0xF8)));
	CMDQ_LOG("[%s]CMDQ_DEBUG_4: 0x%08x\n", tag, CMDQ_REG_GET32((GCE_BASE_VA + 0xFC)));

}

static void cmdq_core_dump_disp_trigger_loop(void)
{
	CMDQ_LOG("dump disp trigger loop\n");
	/* we assume the first non-high-priority thread is trigger loop thread. */
	/* since it will start very early */
	if (gCmdqContext.thread[CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT].taskCount
	    && gCmdqContext.thread[CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT].pCurTask[1]
	    && gCmdqContext.thread[CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT].loopCallback) {

		uint32_t regValue = 0;

		cmdq_core_dump_pc(gCmdqContext.
				  thread[CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT].pCurTask[1],
				  CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT, "INFO");

		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_ID, CMDQ_EVENT_DISP_RDMA0_EOF);
		regValue = CMDQ_REG_GET32(CMDQ_SYNC_TOKEN_VAL);
		CMDQ_LOG("CMDQ_SYNC_TOKEN_VAL of %s is %d\n",
			 cmdq_core_get_event_name(CMDQ_EVENT_DISP_RDMA0_EOF), regValue);
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

	if (CMDQ_INVALID_THREAD == thread)
		return;


	pThread = &(gCmdqContext.thread[thread]);
	pcVA = NULL;

	for (i = 0; i < CMDQ_MAX_TASK_IN_THREAD; i++) {
		pTask = pThread->pCurTask[i];

		if (NULL != pTask) {
			pcVA = cmdq_core_get_pc(pTask, thread, insts);
			if (pcVA) {
				const uint32_t op = (insts[3] & 0xFF000000) >> 24;

				cmdq_core_parse_instruction(pcVA, parsedInstruction,
							    sizeof(parsedInstruction));
				CMDQ_LOG
				    ("[INFO]task:%p(index:%d), Thread %d PC(VA): 0x%p, 0x%08x:0x%08x => %s",
				     pTask, i, thread, pcVA, insts[2], insts[3], parsedInstruction);

				/* for wait event case, dump token value */
				/* for WFE, we specifically dump the event value */
				if (op == CMDQ_CODE_WFE) {
					uint32_t regValue = 0;
					const uint32_t eventID = 0x3FF & insts[3];

					CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_ID, eventID);
					regValue = CMDQ_REG_GET32(CMDQ_SYNC_TOKEN_VAL);
					CMDQ_LOG("[INFO]CMDQ_SYNC_TOKEN_VAL of %s is %d\n",
						 cmdq_core_get_event_name(eventID), regValue);
				}
				break;
			}
		}
	}
}

static void cmdq_core_dump_task(const struct TaskStruct *pTask)
{
	CMDQ_ERR
	    ("Task: 0x%p, Scenario: %d, State: %d, Priority: %d, Flag: 0x%016llx, VABase: 0x%p\n",
	     pTask, pTask->scenario, pTask->taskState, pTask->priority, pTask->engineFlag,
	     pTask->pVABase);

	/* dump last Inst only when VALID command buffer */
	/* otherwise data abort is happened */
	if (pTask->pVABase) {
		CMDQ_ERR
		    ("CMDEnd: 0x%p, MVABase: %pa, Size: %d, Last Inst: 0x%08x:0x%08x, 0x%08x:0x%08x\n",
		     pTask->pCMDEnd, &pTask->MVABase, pTask->commandSize, pTask->pCMDEnd[-3],
		     pTask->pCMDEnd[-2], pTask->pCMDEnd[-1], pTask->pCMDEnd[0]);
	} else {
		CMDQ_ERR("CMDEnd: 0x%p, MVABase: %pa, Size: %d\n",
			 pTask->pCMDEnd, &pTask->MVABase, pTask->commandSize);
	}

	CMDQ_ERR("Reorder:%d, Trigger: %lld, Got IRQ: %lld, Wait: %lld, Finish: %lld\n",
		 pTask->reorder, pTask->trigger, pTask->gotIRQ, pTask->beginWait, pTask->wakedUp);
	CMDQ_ERR("Caller pid:%d name:%s\n", pTask->callerPid, pTask->callerName);

}

bool cmdq_core_verfiy_command_desc_end(struct cmdqCommandStruct *pCommandDesc)
{

	uint32_t *pCMDEnd = NULL;
	bool valid = true;
#if 1				/* for debug */
	/* make sure we have sufficient command to parse */
	if (!CMDQ_U32_PTR(pCommandDesc->pVABase) || pCommandDesc->blockSize < (2 * CMDQ_INST_SIZE))
		return false;
	if (true == cmdq_core_is_request_from_user_space(pCommandDesc->scenario)) {
		/* command buffer has not copied from user space yet, skip verify. */
		return true;
	}

	pCMDEnd =
	    CMDQ_U32_PTR(pCommandDesc->pVABase) + (pCommandDesc->blockSize / sizeof(uint32_t)) - 1;

	/* make sure the command is ended by EOC + JUMP */
	if ((pCMDEnd[-3] & 0x1) != 1) {
		CMDQ_ERR
		    ("[CMD] command desc 0x%p does not throw IRQ (%08x:%08x), pEnd:%p(%p, %d)\n",
		     pCommandDesc, pCMDEnd[-3], pCMDEnd[-2], pCMDEnd,
		     CMDQ_U32_PTR(pCommandDesc->pVABase), pCommandDesc->blockSize);
		valid = false;
	}

	if (((pCMDEnd[-2] & 0xFF000000) >> 24) != CMDQ_CODE_EOC ||
	    ((pCMDEnd[0] & 0xFF000000) >> 24) != CMDQ_CODE_JUMP) {
		CMDQ_ERR
		    ("[CMD] command desc 0x%p does not end in EOC+JUMP (%08x:%08x, %08x:%08x), pEnd:%p(%p, %d)\n",
		     pCommandDesc, pCMDEnd[-3], pCMDEnd[-2], pCMDEnd[-1], pCMDEnd[0], pCMDEnd,
		     CMDQ_U32_PTR(pCommandDesc->pVABase), pCommandDesc->blockSize);
		valid = false;
	}
#if 0
	BUG_ON(!valid);
#else
	if (false == valid)
		CMDQ_AEE("CMDQ", "INVALID command desc 0x%p\n", pCommandDesc);

#endif
#endif
	return valid;

}

bool cmdq_core_verfiy_command_end(const struct TaskStruct *pTask)
{
	bool valid = true;
#if 1				/* for debug */
	/* make sure we have sufficient command to parse */
	if (!pTask->pVABase || pTask->commandSize < (2 * CMDQ_INST_SIZE))
		return false;


	/* make sure the command is ended by EOC + JUMP */
	if ((pTask->pCMDEnd[-3] & 0x1) != 1) {
		CMDQ_ERR("[CMD] pTask 0x%p does not throw IRQ (%08x:%08x)\n",
			 pTask, pTask->pCMDEnd[-3], pTask->pCMDEnd[-2]);
		valid = false;
	}
	if (((pTask->pCMDEnd[-2] & 0xFF000000) >> 24) != CMDQ_CODE_EOC ||
	    ((pTask->pCMDEnd[0] & 0xFF000000) >> 24) != CMDQ_CODE_JUMP) {

		CMDQ_ERR("[CMD] pTask 0x%p does not end in EOC+JUMP (%08x:%08x, %08x:%08x)\n",
			 pTask,
			 pTask->pCMDEnd[-3],
			 pTask->pCMDEnd[-2], pTask->pCMDEnd[-1], pTask->pCMDEnd[0]);
		valid = false;
	}
#if 0
	BUG_ON(!valid);
#else
	if (false == valid)
		CMDQ_AEE("CMDQ", "INVALID pTask 0x%p\n", pTask);

#endif
#endif
	return valid;
}

static void cmdq_core_dump_task_with_engine_flag(uint64_t engineFlag)
{
	struct TaskStruct *pDumpTask = NULL;
	struct list_head *p = NULL;

	CMDQ_ERR
	    ("=============== [CMDQ] All active tasks sharing same engine flag 0x%08llx===============\n",
	     engineFlag);

	list_for_each(p, &gCmdqContext.taskActiveList) {
		pDumpTask = list_entry(p, struct TaskStruct, listEntry);
		if (NULL != pDumpTask && (engineFlag & pDumpTask->engineFlag)) {
			/*line over 120 characters,so devide to two statments */
			CMDQ_ERR
			    ("Thr %d, Task: 0x%p, VABase: 0x%p, MVABase 0x%pa, Size: %d, Flag: 0x%08llx,",
			     (pDumpTask->thread), (pDumpTask), (pDumpTask->pVABase),
			     &(pDumpTask->MVABase), pDumpTask->commandSize, pDumpTask->engineFlag);
			CMDQ_ERR
			    (" Last Inst 0x%08x:0x%08x, 0x%08x:0x%08x\n",
			     pDumpTask->pCMDEnd[-3], pDumpTask->pCMDEnd[-2], pDumpTask->pCMDEnd[-1],
			     pDumpTask->pCMDEnd[0]);
		}
	}

}

static void cmdq_core_dump_task_in_thread(const int32_t thread,
					  const bool fullTatskDump, const bool dumpCookie,
					  const bool dumpCmd)
{
	struct ThreadStruct *pThread;
	struct TaskStruct *pDumpTask;
	int32_t index;
	uint32_t value[4] = { 0 };
	uint32_t cookie;
	char longMsg[CMDQ_LONGSTRING_MAX];
	uint32_t msgOffset;
	int32_t msgMAXSize;

	if (CMDQ_INVALID_THREAD == thread)
		return;


	pThread = &(gCmdqContext.thread[thread]);
	pDumpTask = NULL;

	CMDQ_ERR("=============== [CMDQ] All Task in Error Thread %d ===============\n", thread);
	cookie = CMDQ_GET_COOKIE_CNT(thread);
	if (dumpCookie) {
		CMDQ_ERR("Curr Cookie: %d, Wait Cookie: %d, Next Cookie: %d, Task Count %d\n",
			 cookie, pThread->waitCookie, pThread->nextCookie, pThread->taskCount);
	}

	for (index = 0; index < CMDQ_MAX_TASK_IN_THREAD; index++) {
		pDumpTask = pThread->pCurTask[index];
		if (NULL == pDumpTask)
			continue;


		/* full task dump */
		if (fullTatskDump) {
			CMDQ_ERR("Slot %d, Task: 0x%p\n", index, pDumpTask);
			cmdq_core_dump_task(pDumpTask);

			if (true == dumpCmd) {
				print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 16, 4,
					       pDumpTask->pVABase, (pDumpTask->commandSize), true);
			}
			continue;
		}

		/* otherwise, simple dump task info */
		if (NULL == pDumpTask->pVABase) {
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

		longMsg[0] = '\0';
		msgOffset = 0;
		msgMAXSize = CMDQ_LONGSTRING_MAX;
		cmdqCoreLongString(true, longMsg, &msgOffset, &msgMAXSize,
				   "Slot %d, Task: 0x%p, VABase: 0x%p, MVABase 0x%pa, Size: %d,",
				   index, (pDumpTask), (pDumpTask->pVABase),
				   &(pDumpTask->MVABase), pDumpTask->commandSize);
		cmdqCoreLongString(true, longMsg, &msgOffset, &msgMAXSize,
				   " Last Inst 0x%08x:0x%08x, 0x%08x:0x%08x, priority:%d\n",
				   value[0], value[1], value[2], value[3], pDumpTask->priority);
		if (msgOffset > 0) {
			/* print message */
			CMDQ_ERR("%s", longMsg);
		}

		if (true == dumpCmd) {
			print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 16, 4,
				       pDumpTask->pVABase, (pDumpTask->commandSize), true);
		}
	}
}

int32_t cmdq_core_parse_instruction(const uint32_t *pCmd, char *textBuf, int bufLen)
{
	int reqLen = 0;
	const uint32_t op = (pCmd[1] & 0xFF000000) >> 24;
	const uint32_t argA = pCmd[1] & (~0xFF000000);
	const uint32_t argB = pCmd[0];

	switch (op) {
	case CMDQ_CODE_MOVE:
		if (1 & (argA >> 23)) {
			reqLen =
			    snprintf(textBuf, bufLen, "MOVE: 0x%08x to R%d\n", argB,
				     (argA >> 16) & 0x1f);
		} else {
			reqLen = snprintf(textBuf, bufLen, "MASK: 0x%08x\n", argB);
		}
		break;
	case CMDQ_CODE_READ:
	case CMDQ_CODE_WRITE:
	case CMDQ_CODE_POLL:
		reqLen = snprintf(textBuf, bufLen, "%s: ", cmdq_core_parse_op(op));
		bufLen -= reqLen;
		textBuf += reqLen;

		/* data (value) */
		if (argA & (1 << 22)) {
			reqLen = snprintf(textBuf, bufLen, "R%d, ", argB);
			bufLen -= reqLen;
			textBuf += reqLen;
		} else {
			reqLen = snprintf(textBuf, bufLen, "0x%08x, ", argB);
			bufLen -= reqLen;
			textBuf += reqLen;
		}

		/* address */
		if (argA & (1 << 23)) {
			reqLen = snprintf(textBuf, bufLen, "R%d\n", (argA >> 16) & 0x1F);
			bufLen -= reqLen;
			textBuf += reqLen;
		} else {
			const uint32_t addr = cmdq_core_subsys_to_reg_addr(argA);

			reqLen = snprintf(textBuf, bufLen, "addr=0x%08x [%s],use_mask=%d\n",
					  (addr & 0xFFFFFFFE),
					  cmdq_core_parse_module_from_reg_addr(addr), (addr & 0x1)
			    );
			bufLen -= reqLen;
			textBuf += reqLen;
		}
		break;
	case CMDQ_CODE_JUMP:
		if (argA) {
			if (argA & (1 << 22)) {
				/* jump by register */
				reqLen = snprintf(textBuf, bufLen, "JUMP(REG): R%d\n", argB);
			} else {
				/* absolute */
				reqLen = snprintf(textBuf, bufLen, "JUMP(ABS): 0x%08x\n", argB);
			}
		} else {
			/* relative */
			if ((int32_t) argB >= 0) {
				reqLen = snprintf(textBuf, bufLen,
						  "JUMP(REL): +%d\n", (int32_t) argB);
			} else {
				reqLen = snprintf(textBuf, bufLen,
						  "JUMP(REL): %d\n", (int32_t) argB);
			}
		}
		break;
	case CMDQ_CODE_WFE:
		if (0x80008001 == argB) {
			reqLen =
			    snprintf(textBuf, bufLen, "WFE: %s\n", cmdq_core_get_event_name(argA));
		} else {
			reqLen = snprintf(textBuf, bufLen,
					  "SYNC: %s, upd=%d, op=%d, val=%d, wait=%d, wop=%d, val=%d\n",
					  cmdq_core_get_event_name(argA),
					  (argB >> 31) & 0x1,
					  (argB >> 28) & 0x7,
					  (argB >> 16) & 0xFFF,
					  (argB >> 15) & 0x1,
					  (argB >> 12) & 0x7, (argB >> 0) & 0xFFF);
		}
		break;
	case CMDQ_CODE_EOC:
		if (argA == 0 && argB == 0x00000001) {
			reqLen = snprintf(textBuf, bufLen, "EOC\n");
		} else {

			if (cmdq_core_support_sync_non_suspendable()) {
				reqLen = snprintf(textBuf, bufLen,
						  "MARKER: sync_no_suspnd=%d",
						  (argA & (1 << 20)) > 0);
			} else {
				reqLen = snprintf(textBuf, bufLen, "MARKER:");
			}

			reqLen = snprintf(textBuf, bufLen,
					  "no_suspnd=%d, no_inc=%d, m=%d, m_en=%d, prefetch=%d, irq=%d\n",
					  (argA & (1 << 21)) > 0,
					  (argA & (1 << 16)) > 0,
					  (argB & (1 << 20)) > 0,
					  (argB & (1 << 17)) > 0,
					  (argB & (1 << 16)) > 0, (argB & (1 << 0)) > 0);
		}
		break;
	default:
		reqLen = snprintf(textBuf, bufLen, "UNDEFINED\n");
		break;
	}

	return reqLen;
}

int32_t cmdqCoreDebugDumpCommand(struct TaskStruct *pTask)
{
	const uint32_t *pCmd = NULL;
	static char textBuf[128] = { 0 };
	int i = 0;

	if (NULL == pTask)
		return -EFAULT;


	mutex_lock(&gCmdqTaskMutex);

	CMDQ_LOG("======TASK 0x%p command buffer:\n", pTask);
	print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 16, 4,
		       pTask->pVABase, pTask->commandSize, false);
	CMDQ_LOG("======TASK 0x%p command buffer END\n", pTask);
	CMDQ_LOG("TASK 0x%p command buffer TRANSLATED:\n", pTask);
	for (i = 0, pCmd = pTask->pVABase; i < pTask->commandSize; i += 8, pCmd += 2) {
		cmdq_core_parse_instruction(pCmd, textBuf, 128);
		CMDQ_LOG("%s", textBuf);
	}
	CMDQ_LOG("======TASK 0x%p command END\n", pTask);

	mutex_unlock(&gCmdqTaskMutex);

	return 0;
}

void cmdq_core_dump_instructions(uint64_t *pInstrBuffer, uint32_t bufferSize)
{
	uint64_t *pBuffer = pInstrBuffer;
	char textBuffer[100];
	int i = 0;

	while (pBuffer <= (uint64_t *) ((uint8_t *) (pInstrBuffer) + bufferSize)) {
		memset(textBuffer, 0, 100);
		cmdq_core_parse_instruction((uint32_t *) pBuffer, textBuffer, 99);
		CMDQ_LOG("index:%05d,INST:0x%016llx    ----->      %s\n", i, *pBuffer, textBuffer);
		pBuffer++;
	}
}


static void cmdq_core_fill_task_record(struct RecordStruct *pRecord, const struct TaskStruct *pTask,
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
		pRecord->isSecure = pTask->secData.isSecure;

		if (NULL == pTask->profileData) {
			pRecord->writeTimeNS = 0;

		} else {
			/* Command exec time, each count is 76ns */
			begin = *((volatile uint32_t *)pTask->profileData);
			end = *((volatile uint32_t *)(pTask->profileData + 1));
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
	}
}

static void cmdq_core_track_task_record(struct TaskStruct *pTask, uint32_t thread)
{
	struct RecordStruct *pRecord;
	unsigned long flags;
	CMDQ_TIME done;


#if 0
	if (cmdq_core_should_profile(pTask->scenario))
		return;

#endif

	done = sched_clock();

	spin_lock_irqsave(&gCmdqRecordLock, flags);

	pRecord = &(gCmdqContext.record[gCmdqContext.lastID]);

	cmdq_core_fill_task_record(pRecord, pTask, thread);

	pRecord->done = done;

	gCmdqContext.lastID++;
	if (gCmdqContext.lastID >= CMDQ_MAX_RECORD_COUNT)
		gCmdqContext.lastID = 0;


	gCmdqContext.recNum++;
	if (gCmdqContext.recNum >= CMDQ_MAX_RECORD_COUNT)
		gCmdqContext.recNum = CMDQ_MAX_RECORD_COUNT;


	spin_unlock_irqrestore(&gCmdqRecordLock, flags);
}

void cmdq_core_dump_GIC(void)
{
#if CMDQ_DUMP_GIC
	mt_irq_dump_status(cmdq_dev_get_irq_id());
	mt_irq_dump_status(cmdq_dev_get_irq_secure_id());
#endif
}

static void cmdq_core_copy_error_task_instruction(struct ErrorTaskItemStruct *pErrorTask,
						  const struct TaskStruct *pTask)
{
	int32_t index = 0;

	pErrorTask->pTask = pTask;
	pErrorTask->commandSize = pTask->commandSize;
	pErrorTask->trigger = pTask->trigger;
	if (NULL != pErrorTask->pVABase)
		return;

	pErrorTask->pVABase = kmalloc(pTask->commandSize, GFP_KERNEL);
	if (NULL == pErrorTask->pVABase) {
		CMDQ_ERR("%s:alloc memory for error task failed\n", __func__);
		return;
	}
	memset(pErrorTask->pVABase, 0, pTask->commandSize);

	for (index = 0; index < (pTask->commandSize) / 4; index++)
		pErrorTask->pVABase[index] = pTask->pVABase[index];

}


static void cmdq_core_attach_error_task(const struct TaskStruct *pTask, int32_t thread)
{
	struct EngineStruct *pEngine = NULL;
	struct ThreadStruct *pThread = NULL;
	struct CmdqCBkStruct *pCallback = NULL;
	uint64_t engFlag = 0;
	int32_t index = 0;
	uint32_t *hwPC = NULL;
	uint32_t value[10] = { 0 };
	static const char *const engineGroupName[] = {
		CMDQ_FOREACH_GROUP(GENERATE_STRING)
	};

	pThread = &(gCmdqContext.thread[thread]);
	pEngine = gCmdqContext.engine;

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->warning, MMProfileFlagPulse, (uint32_t) pTask, thread);

	/*  */
	/* Update engine fail count */
	/*  */
	if (pTask) {
		engFlag = pTask->engineFlag;
		for (index = 0; index < CMDQ_MAX_ENGINE_COUNT; index++) {
			if (engFlag & (1LL << index))
				pEngine[index].failCount++;

		}
	}
	/*  */
	/* register error record */
	/*  */
	if (gCmdqContext.errNum < CMDQ_MAX_ERROR_COUNT) {
		struct ErrorStruct *pError = &gCmdqContext.error[gCmdqContext.errNum];

		cmdq_core_fill_task_record(&pError->errorRec, pTask, thread);
		pError->ts_nsec = local_clock();
	}

	/*  */
	/* Then we just print out info */
	/*  */
	CMDQ_ERR("====== [CMDQ] Begin of Error %d pTask[%p] trigger[%lld] ======\n",
		 gCmdqContext.errNum, pTask, pTask->trigger);
#ifndef CONFIG_MTK_FPGA
	CMDQ_ERR("=============== [CMDQ] SMI Status ===============\n");
	cmdq_core_dump_smi(1);
#endif
	CMDQ_ERR("=============== [CMDQ] Error Thread Status ===============\n");
	if (false == cmdq_core_is_a_secure_thread(thread)) {
		/* normal thread */
		value[0] = CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread));
		value[1] = CMDQ_REG_GET32(CMDQ_THR_END_ADDR(thread));
		value[2] = CMDQ_REG_GET32(CMDQ_THR_WAIT_TOKEN(thread));
		value[3] = CMDQ_GET_COOKIE_CNT(thread);
		value[4] = CMDQ_REG_GET32(CMDQ_THR_IRQ_STATUS(thread));
		value[5] = CMDQ_REG_GET32(CMDQ_THR_INST_CYCLES(thread));
		value[6] = CMDQ_REG_GET32(CMDQ_THR_CURR_STATUS(thread));
		value[7] = CMDQ_REG_GET32(CMDQ_THR_IRQ_ENABLE(thread));
		value[8] = CMDQ_REG_GET32(CMDQ_THR_ENABLE_TASK(thread));

		CMDQ_ERR
		    ("Index: %d, Enabled: %d, IRQ: 0x%08x, Thread PC: 0x%08x, End: 0x%08x, Wait Token: 0x%08x\n",
		     thread, value[8], value[4], value[0], value[1], value[2]);
		CMDQ_ERR("Curr Cookie: %d, Wait Cookie: %d, Next Cookie: %d, Task Count %d,\n",
			 value[3], pThread->waitCookie, pThread->nextCookie, pThread->taskCount);
		CMDQ_ERR("Timeout Cycle:%d, Status:0x%08x, IRQ_EN: 0x%08x\n", value[5], value[6],
			 value[7]);
	} else {
		/* do nothing since it's a secure thread */
		CMDQ_ERR("Wait Cookie: %d, Next Cookie: %d, Task Count %d,\n",
			 pThread->waitCookie, pThread->nextCookie, pThread->taskCount);
	}
	CMDQ_ERR("=============== [CMDQ] CMDQ Status ===============\n");
	cmdq_core_dump_status("ERR");

	if (NULL != pTask) {
		CMDQ_ERR
		    ("=============== [CMDQ] Error Thread PC Task[%p] trigger[%lld]===============\n",
		     pTask, pTask->trigger);
		hwPC = cmdq_core_dump_pc(pTask, thread, "ERR");

		CMDQ_ERR
		    ("=============== [CMDQ] Begin of Error Task[%p] trigger[%lld] Status ===============\n",
		     pTask, pTask->trigger);
		cmdq_core_dump_task(pTask);
		cmdq_core_dump_instructions((uint64_t *) pTask->pVABase, pTask->commandSize);

		CMDQ_ERR
		    ("=============== [CMDQ] End of Error Task[%p] trigger[%lld] Status ===============\n",
		     pTask, pTask->trigger);
	}

	CMDQ_ERR("=============== [CMDQ] Clock Gating Status ===============\n");
	CMDQ_ERR("[CLOCK] common clock ref=%d\n", atomic_read(&gCmdqThreadUsage));
	cmdq_core_dump_clock_gating();

	/*  */
	/* Dump MMSYS configuration */
	/*  */
	CMDQ_ERR("=============== [CMDQ] MMSYS_CONFIG ===============\n");
	cmdq_core_dump_mmsys_config();

	/*  */
	/* ask each module to print their status */
	/*  */
	CMDQ_ERR("=============== [CMDQ] Engine Status ===============\n");
	pCallback = gCmdqGroupCallback;
	for (index = 0; index < CMDQ_MAX_GROUP_COUNT; ++index) {
		if (!cmdq_core_is_group_flag((enum CMDQ_GROUP_ENUM)index, pTask->engineFlag))
			continue;

		CMDQ_ERR("====== engine group %s status =======\n", engineGroupName[index]);

		if (NULL == pCallback[index].dumpInfo) {
			CMDQ_ERR("(no dump function)\n");
			continue;
		}

		pCallback[index].dumpInfo((gCmdqEngineGroupBits[index] & pTask->engineFlag),
					  cmdq_core_get_log_level());
	}

	/* force dump DISP for DISP scenario with 0x0 engine flag */
	if ((0x0 == pTask->engineFlag) && cmdq_core_is_disp_scenario(pTask->scenario)) {
		index = CMDQ_GROUP_DISP;
		if (pCallback[index].dumpInfo) {
			pCallback[index].dumpInfo((gCmdqEngineGroupBits[index] & pTask->engineFlag),
						  cmdq_core_get_log_level());
		}
	}
	CMDQ_ERR("=============== [CMDQ] GIC dump ===============\n");
	cmdq_core_dump_GIC();

	/* dump tasks in error thread */
	cmdq_core_dump_task_in_thread(thread, false, false, false);
	cmdq_core_dump_task_with_engine_flag(pTask->engineFlag);

#if 0
	CMDQ_ERR("=============== [CMDQ] Error Command Buffer ===============\n");

	if (hwPC && pTask && hwPC >= pTask->pVABase) {
		/* because hwPC points to "start" of the instruction */
		/* add offset 1 */
		print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 16, 4,
			       pTask->pVABase, (2 + hwPC - pTask->pVABase) * sizeof(uint32_t),
			       true);
	} else {
		CMDQ_ERR("hwPC is not in region, dump all\n");
		print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 16, 4,
			       pTask->pVABase, (pTask->commandSize), true);
	}

	if (NULL != pTask)
		cmdq_core_dump_instructions((uint64_t *) (pTask->pVABase), pTask->commandSize);
#endif

	CMDQ_ERR("====== [CMDQ] End of Error %d pTask[%p] trigger[%lld] ======\n",
		 gCmdqContext.errNum, pTask, pTask->trigger);

	gCmdqContext.errNum++;
}

static void cmdq_core_parse_error(struct TaskStruct *pTask, uint32_t thread,
					const char **moduleName, int32_t *flag, uint32_t *instA,
					uint32_t *instB)
{
	uint32_t op, argA, argB;
	uint32_t insts[4] = { 0 };
	uint32_t addr = 0;
	const char *module = NULL;
	int32_t irqFlag = pTask->irqFlag;
	int isSMIHang = 0;

	do {
#ifndef CONFIG_MTK_FPGA
		/* confirm if SMI is hang */
		isSMIHang = cmdq_core_dump_smi(0);
		if (isSMIHang) {
			module = "SMI";
			break;
		}
#endif
		/* other cases, use instruction to judge */
		/* because scenario / HW flag are not sufficient */
		/* e.g. ISP pass 2 involves both MDP and ISP */
		/* so we need to check which instruction timeout-ed. */
		if (cmdq_core_get_pc(pTask, thread, insts)) {
			op = (insts[3] & 0xFF000000) >> 24;
			argA = insts[3] & (~0xFF000000);
			argB = insts[2];

			/* quick exam by hwflag first */
			module = cmdq_core_parse_error_module_by_hwflag_impl(pTask);
			if (NULL != module)
				break;


			switch (op) {
			case CMDQ_CODE_POLL:
			case CMDQ_CODE_WRITE:
				addr = cmdq_core_subsys_to_reg_addr(argA);
				module = cmdq_core_parse_module_from_reg_addr(addr);
				break;
			case CMDQ_CODE_WFE:
				/* argA is the event ID */
				module =
				    cmdq_core_module_from_event_id((enum CMDQ_EVENT_ENUM)argA, argA,
								   argB);
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

static int32_t cmdq_core_insert_task_from_thread_array_by_cookie(struct TaskStruct *pTask,
								       struct ThreadStruct *pThread,
								       const int32_t cookie,
								       const bool resetHWThread)
{

	if (NULL == pTask || NULL == pThread) {
		CMDQ_ERR("invalid param, pTask[0x%p], pThread[0x%p], cookie[%d], needReset[%d]\n",
			 pTask, pThread, cookie, resetHWThread);
		return -EFAULT;
	}

	if (true == resetHWThread) {
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
	pThread->pCurTask[cookie % CMDQ_MAX_TASK_IN_THREAD] = pTask;
	pThread->allowDispatching = 1;

	/* secure path */
	if (pTask->secData.isSecure) {
		pTask->secData.waitCookie = cookie;
		pTask->secData.resetExecCnt = resetHWThread;
	}

	return 0;
}

static int32_t cmdq_core_remove_task_from_thread_array_by_cookie(struct ThreadStruct *
								       pThread, int32_t index,
								       enum TASK_STATE_ENUM
								       newTaskState)
{
	struct TaskStruct *pTask = NULL;
	int32_t thread;

	if ((NULL == pThread) || (CMDQ_MAX_TASK_IN_THREAD <= index) || (0 > index)) {
		CMDQ_ERR
		    ("remove task from thread array, invalid param. THR[0x%p], task_slot[%d], newTaskState[%d]\n",
		     pThread, index, newTaskState);
		return -EINVAL;
	}
	pTask = pThread->pCurTask[index];
	if (NULL == pTask) {
		CMDQ_ERR("remove fail, task_slot[%d] on thread[%p] is NULL\n", index, pThread);
		return -EINVAL;
	}
	/* note timing to switch a task to done_status(_ERROR, _KILLED, _DONE) */
	/* is aligned with thread's taskcount change */
	/* check task status to prevent double clean-up thread's taskcount */
	if (TASK_STATE_BUSY != pTask->taskState) {
		CMDQ_ERR
		    ("remove task, taskStatus err[%d]. THR[0x%p], task_slot[%d], targetTaskStaus[%d]\n",
		     pTask->taskState, pThread, index, newTaskState);
		return -EINVAL;
	}
	CMDQ_VERBOSE("remove task, slot[%d], targetStatus: %d\n", index, newTaskState);
	thread = pTask->thread;
	pTask->taskState = newTaskState;
	pTask = NULL;
	pThread->pCurTask[index] = NULL;
	pThread->taskCount--;

	if (0 > (int32_t)pThread->taskCount) {
		CMDQ_ERR("taskCount < 0 after cmdq_core_remove_task_from_thread_array_by_cookie\n");
		CMDQ_ERR("thread[%d], index=%d\n", thread, index);
		/* for Error Handle */
		pThread->taskCount = 0;
	}

	return 0;
}

static int32_t cmdq_core_force_remove_task_from_thread(struct TaskStruct *pTask,
							     uint32_t thread)
{
	int32_t status = 0;
	int32_t cookie = 0;
	int index = 0;
	int loop = 0;
	struct TaskStruct *pExecTask = NULL;
	struct ThreadStruct *pThread = &(gCmdqContext.thread[thread]);

	status = cmdq_core_suspend_HW_thread(thread);

	CMDQ_REG_SET32(CMDQ_THR_INST_CYCLES(thread), cmdq_core_get_task_timeout_cycle(pThread));

	/* The cookie of the task currently being processed */
	cookie = CMDQ_GET_COOKIE_CNT(thread) + 1;

	pExecTask = pThread->pCurTask[cookie % CMDQ_MAX_TASK_IN_THREAD];
	if (NULL != pExecTask && (pExecTask == pTask)) {
		/* The task is executed now, set the PC to EOC for bypass */
		CMDQ_REG_SET32(CMDQ_THR_CURR_ADDR(thread),
			       CMDQ_PHYS_TO_AREG(pTask->MVABase + pTask->commandSize - 16));

		cmdq_core_reset_hw_engine(pTask->engineFlag);

		pThread->pCurTask[cookie % CMDQ_MAX_TASK_IN_THREAD] = NULL;
		pTask->taskState = TASK_STATE_KILLED;
	} else {
		loop = pThread->taskCount;
		for (index = (cookie % CMDQ_MAX_TASK_IN_THREAD); loop > 0; loop--, index++) {
			if (index >= CMDQ_MAX_TASK_IN_THREAD)
				index = 0;


			pExecTask = pThread->pCurTask[index];
			if (NULL == pExecTask)
				continue;


			if ((0x10000000 == pExecTask->pCMDEnd[0]) &&
			    (0x00000008 == pExecTask->pCMDEnd[-1])) {
				/* We reached the last task */
				break;
			} else if (pExecTask->pCMDEnd[-1] == pTask->MVABase) {
				/* Fake EOC command */
				pExecTask->pCMDEnd[-1] = 0x00000001;
				pExecTask->pCMDEnd[0] = 0x40000000;

				/* Bypass the task */
				pExecTask->pCMDEnd[1] = pTask->pCMDEnd[-1];
				pExecTask->pCMDEnd[2] = pTask->pCMDEnd[0];

				index += 1;
				if (index >= CMDQ_MAX_TASK_IN_THREAD)
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

static struct TaskStruct *cmdq_core_search_task_by_pc(uint32_t threadPC,
						      const struct ThreadStruct *pThread)
{
	struct TaskStruct *pTask = NULL;
	int i = 0;

	for (i = 0; i < CMDQ_MAX_TASK_IN_THREAD; ++i) {
		pTask = pThread->pCurTask[i];
		if (pTask &&
		    threadPC >= pTask->MVABase &&
		    threadPC <= (pTask->MVABase + pTask->commandSize)) {
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
static int32_t cmdq_core_wait_task_done_with_timeout_impl(struct TaskStruct *pTask,
								int32_t thread)
{
	int32_t waitQ;
	unsigned long flags;
	struct ThreadStruct *pThread = NULL;
	int32_t retryCount = 0;
	struct ErrorTaskItemStruct *pErrorTask = NULL;

	pThread = &(gCmdqContext.thread[thread]);


	/* timeout wait & make sure this task is finished. */
	/* pTask->taskState flag is updated in IRQ handlers like cmdqCoreHandleDone. */
	retryCount = 0;
	waitQ = wait_event_timeout(gCmdWaitQueue[thread],
				   (TASK_STATE_BUSY != pTask->taskState
				    && TASK_STATE_WAITING != pTask->taskState),
				   /* timeout_jiffies); */
				   msecs_to_jiffies(CMDQ_PREDUMP_TIMEOUT_MS));

	/* if SW-timeout, pre-dump hang instructions */
	while (0 == waitQ && retryCount < CMDQ_PREDUMP_RETRY_COUNT) {
		CMDQ_LOG
		    ("==========[CMDQ] timeout Pre-dump(%d) pTask[0x%p], trigger[%lld]==========\n",
		     retryCount, pTask, pTask->trigger);

		if (0 == retryCount) {
			/* dump task instructions */
			CMDQ_ERR("------------pTask[%p] Trigger[%lld] dump begin------------\n",
				 pTask, pTask->trigger);
			cmdq_core_dump_instructions((uint64_t *) (pTask->pVABase),
						    pTask->commandSize);
			CMDQ_ERR("------------pTask[%p] Trigger[%lld] dump finish------------\n",
				 pTask, pTask->trigger);
			if (gCmdqContext.errorTask.errTaskCount < CMDQ_MAX_ERROR_COUNT) {
				/* copy error task to error task buffer */
				pErrorTask =
				    &gCmdqContext.errorTask.errorTaskBuffer[gCmdqContext.errNum];
				cmdq_core_copy_error_task_instruction(pErrorTask, pTask);
				gCmdqContext.errorTask.errTaskCount++;
			}

		}

		++retryCount;
		spin_lock_irqsave(&gCmdqExecLock, flags);
		cmdq_core_dump_status("INFO");
		cmdq_core_dump_pc(pTask, thread, "INFO");

		/* HACK: check trigger thread status */
		cmdq_core_dump_disp_trigger_loop();
		/* end of HACK */

		spin_unlock_irqrestore(&gCmdqExecLock, flags);

		/* then we wait again */
		waitQ = wait_event_timeout(gCmdWaitQueue[thread],
					   (TASK_STATE_BUSY != pTask->taskState
					    && TASK_STATE_WAITING != pTask->taskState),
					   msecs_to_jiffies(CMDQ_PREDUMP_TIMEOUT_MS));
	}

	return waitQ;
}

static int32_t cmdq_core_handle_wait_task_result_secure_impl(struct TaskStruct *pTask,
								   int32_t thread,
								   const int32_t waitQ)
{
#ifndef CMDQ_SECURE_PATH_SUPPORT
	CMDQ_ERR("SVP feature not supported\n");
	return -1;
#else
	int32_t i;
	int32_t status;
	struct ThreadStruct *pThread = NULL;
/* bool markAsErrorTask = false; */
	/* error report */
	bool throwAEE = false;
	const char *module = NULL;
	int32_t irqFlag = 0;
	cmdqSecCancelTaskResultStruct result;
	unsigned long flags;

	/* Init default status */
	status = 0;
	pThread = &(gCmdqContext.thread[thread]);
	memset(&result, 0, sizeof(cmdqSecCancelTaskResultStruct));

	/* lock cmdqSecLock */
	cmdq_sec_lock_secure_path();

	do {
		/* check if this task has finished */
		if (TASK_STATE_BUSY != pTask->taskState)
			break;


		/* Oops, tha tasks is not done. */
		/* We have several possible error scenario: */
		/* 1. task still running (hang / timeout) */
		/* 2. IRQ pending (done or error/timeout IRQ) */
		/* 3. task's SW thread has been signaled (e.g. SIGKILL) */


		/* suppose that task failed, entry secure world to confirm it */
		/* we entry secure world: */
		/* .check if pending IRQ and update cookie value to shared memory */
		/* .confirm if task execute done  */
		/* if not, do error handle */
		/*     .recover M4U & DAPC setting */
		/*     .dump secure HW thread */
		/*     .reset CMDQ secure HW thread */
		cmdq_sec_cancel_error_task_unlocked(pTask, thread, &result);

		spin_lock_irqsave(&gCmdqExecLock, flags);

		/* confirm pending IRQ first */
		cmdq_core_handle_secure_thread_done_impl(thread, 0x01, &pTask->wakedUp);

		/* check if this task has finished after handling pending IRQ */
		if (TASK_STATE_DONE == pTask->taskState) {
			if (result.hasReset == true) {
				/* remove all tasks in tread since we have reset HW thread in SWd */
				for (i = 0; i < CMDQ_MAX_TASK_IN_THREAD; i++) {
					pTask = pThread->pCurTask[i];
					if (pTask) {
						cmdq_core_remove_task_from_thread_array_by_cookie(pThread, i,
												  TASK_STATE_ERROR);
					}
				}
				pThread->taskCount = 0;
				pThread->waitCookie = pThread->nextCookie;
			}
			spin_unlock_irqrestore(&gCmdqExecLock, flags);
			break;
		}

		status = -ETIMEDOUT;
		throwAEE = true;
		/* shall we pass the error instru back from secure path?? */
		/* cmdq_core_parse_error(pTask, thread, &module, &irqFlag, &instA, &instB); */
		module = cmdq_core_parse_error_module_by_hwflag_impl(pTask);

		/* module dump */
		cmdq_core_attach_error_task(pTask, thread);

		/* module reset */
		/* TODO: get needReset infor by secure thread PC */
		cmdq_core_reset_hw_engine(pTask->engineFlag);

		/* remove all tasks in tread since we have reset HW thread in SWd */
		for (i = 0; i < CMDQ_MAX_TASK_IN_THREAD; i++) {
			pTask = pThread->pCurTask[i];
			if (pTask) {
				cmdq_core_remove_task_from_thread_array_by_cookie(pThread, i,
										  TASK_STATE_ERROR);
			}
		}
		pThread->taskCount = 0;
		pThread->waitCookie = pThread->nextCookie;
		spin_unlock_irqrestore(&gCmdqExecLock, flags);
	} while (0);

	/* unlock cmdqSecLock */
	cmdq_sec_unlock_secure_path();

	/* throw AEE if nessary */
	if (throwAEE) {
#if 1
		const uint32_t instA = result.errInstr[1];
		const uint32_t instB = result.errInstr[0];
		const uint32_t op = (instA & 0xFF000000) >> 24;

		CMDQ_AEE(module, "%s in CMDQ IRQ:0x%02x, INST:(0x%08x, 0x%08x), OP:%s\n",
			 module, irqFlag, instA, instB, cmdq_core_parse_op(op));

#else
		CMDQ_AEE("CMDQ",
			 "[SEC]<--SEC_SUBMIT: err[%d], pTask[0x%p], THR[%d], tgid[%d:%d], duration_ms[%d], cmdId[%d]\n",
			 status, pTask, thread, tgid, pid, duration, iwcCommand);
#endif
	}

	return status;
#endif
}

static int32_t cmdq_core_handle_wait_task_result_impl(struct TaskStruct *pTask,
							    int32_t thread, const int32_t waitQ)
{
	int32_t status;
	int32_t index;
	unsigned long flags;
	struct ThreadStruct *pThread = NULL;
	bool markAsErrorTask = false;
	/* error report */
	bool throwAEE = false;
	const char *module = NULL;
	uint32_t instA = 0, instB = 0;
	int32_t irqFlag = 0;


	/* Init default status */
	status = 0;
	pThread = &(gCmdqContext.thread[thread]);

	/* Note that although we disable IRQ, HW continues to execute */
	/* so it's possible to have pending IRQ */
	spin_lock_irqsave(&gCmdqExecLock, flags);

	do {
		struct TaskStruct *pNextTask = NULL;
		struct TaskStruct *pPrevTask = NULL;
		struct TaskStruct *pRunningTask = NULL;
		int32_t cookie = 0;
		long threadPC = 0L;

		status = 0;
		throwAEE = false;
		markAsErrorTask = false;

		if (TASK_STATE_DONE == pTask->taskState)
			break;


		CMDQ_ERR("Task state of %p is not TASK_STATE_DONE, %d\n", pTask, pTask->taskState);

		/* Oops, tha tasks is not done. */
		/* We have several possible error scenario: */
		/* 1. task still running (hang / timeout) */
		/* 2. IRQ pending (done or error/timeout IRQ) */
		/* 3. task's SW thread has been signaled (e.g. SIGKILL) */

		/* suspend HW thread first, so that we work in a consistent state */
		status = cmdq_core_suspend_HW_thread(thread);
		if (0 > status)
			throwAEE = true;


		/* The cookie of the task currently being processed */
		cookie = CMDQ_GET_COOKIE_CNT(thread) + 1;
		pRunningTask = pThread->pCurTask[cookie % CMDQ_MAX_TASK_IN_THREAD];
		threadPC = CMDQ_AREG_TO_PHYS(CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread)));

		/* process any pending IRQ */
		/* TODO: provide no spin lock version because we already locked. */
		irqFlag = CMDQ_REG_GET32(CMDQ_THR_IRQ_STATUS(thread));
		if (irqFlag & 0x12)
			cmdqCoreHandleError(thread, irqFlag, &pTask->wakedUp);
		else if (irqFlag & 0x01)
			cmdqCoreHandleDone(thread, irqFlag, &pTask->wakedUp);

		CMDQ_REG_SET32(CMDQ_THR_IRQ_STATUS(thread), ~irqFlag);

		/* check if this task has finished after handling pending IRQ */
		if (TASK_STATE_DONE == pTask->taskState)
			break;


		/* Then decide we are SW timeout or SIGNALed (not an error) */
		if (0 == waitQ) {
			/* SW timeout and no IRQ received */
			markAsErrorTask = true;

			/* if we reach here, we're in errornous state. */
			/* print error log immediately. */
			cmdq_core_attach_error_task(pTask, thread);

			CMDQ_ERR("SW timeout of task 0x%p on thread %d\n", pTask, thread);
			throwAEE = true;
			cmdq_core_parse_error(pTask, thread, &module, &irqFlag, &instA, &instB);
			status = -ETIMEDOUT;

		} else if (0 > waitQ) {
			/* Task be killed. Not an error, but still need removal. */

			markAsErrorTask = false;

			if (-ERESTARTSYS == waitQ)
				CMDQ_ERR("Task %p KILLED by waitQ = -ERESTARTSYS\n", pTask);
			else if (-EINTR == waitQ)
				CMDQ_ERR("Task %p KILLED by waitQ = -EINTR\n", pTask);
			else
				CMDQ_ERR("Task %p KILLED by waitQ = %d\n", pTask, waitQ);


			status = waitQ;
		}

		/* reset HW engine immediately if we already got error IRQ. */
		if (TASK_STATE_ERROR == pTask->taskState) {
			cmdq_core_reset_hw_engine(pTask->engineFlag);
		} else if (TASK_STATE_BUSY == pTask->taskState) {
			/*  */
			/* if taskState is BUSY, this means we did not reach EOC, did not have error IRQ. */
			/* - remove the task from thread.pCurTask[] */
			/* - and decrease thread.taskCount */
			/* NOTE: after this, the pCurTask will not contain link to pTask anymore. */
			/* and pTask should become TASK_STATE_ERROR */

			/* we find our place in pThread->pCurTask[]. */
			for (index = 0; index < CMDQ_MAX_TASK_IN_THREAD; ++index) {
				if (pThread->pCurTask[index] == pTask) {
					/* update taskCount and pCurTask[] */
					cmdq_core_remove_task_from_thread_array_by_cookie(pThread,
											  index,
											  markAsErrorTask
											  ?
											  TASK_STATE_ERROR
											  :
											  TASK_STATE_KILLED);
					break;
				}
			}
		}

		pNextTask = NULL;

		/* find pTask's jump destination */
		if (0x10000001 == pTask->pCMDEnd[0]) {
			pNextTask = cmdq_core_search_task_by_pc(pTask->pCMDEnd[-1], pThread);
		} else {
			CMDQ_MSG("No next task: LAST instruction : (0x%08x, 0x%08x)\n",
				 pTask->pCMDEnd[0], pTask->pCMDEnd[-1]);
		}

		/* Then, we try remove pTask from the chain of pThread->pCurTask. */
		/* . if HW PC falls in pTask range */
		/* . HW EXEC_CNT += 1 */
		/* . thread.waitCookie += 1 */
		/* . set HW PC to next task head */
		/* . if not, find previous task (whose jump address is pTask->MVABase) */
		/* . check if HW PC points is not at the EOC/JUMP end */
		/* . change jump to fake EOC(no IRQ) */
		/* . insert jump to next task head and increase cmd buffer size */
		/* . if there is no next task, set HW End Address */
		if (pTask->pCMDEnd && threadPC >= pTask->MVABase
		    && threadPC <= (pTask->MVABase + pTask->commandSize)) {
			if (pNextTask) {
				/* cookie already +1 */
				CMDQ_REG_SET32(CMDQ_THR_EXEC_CNT(thread), cookie);
				pThread->waitCookie = cookie + 1;
				CMDQ_REG_SET32(CMDQ_THR_CURR_ADDR(thread),
					       CMDQ_PHYS_TO_AREG(pNextTask->MVABase));
				CMDQ_MSG("WAIT: resume task 0x%p from err\n", pNextTask);
			}
		} else {
			pPrevTask = NULL;
			for (index = 0; index < CMDQ_MAX_TASK_IN_THREAD; ++index) {
				pPrevTask = pThread->pCurTask[index];

				/* find which task JUMP into pTask */
				if (pPrevTask && pPrevTask->pCMDEnd &&
					pPrevTask->pCMDEnd[-1] == pTask->MVABase &&	/* pTask command */
				    pPrevTask->pCMDEnd[0] == 0x10000001) {	/* JUMP */
					/* Copy Jump instruction */
					pPrevTask->pCMDEnd[-1] = pTask->pCMDEnd[-1];
					pPrevTask->pCMDEnd[0] = pTask->pCMDEnd[0];

					if (pNextTask)
						cmdq_core_reorder_task_array(pThread, index);

					CMDQ_VERBOSE
					    ("WAIT: modify jump to 0x%08x (pPrev:0x%p, pTask:0x%p)\n",
					     pTask->pCMDEnd[-1], pPrevTask, pTask);

					/* Give up fetched command, invoke CMDQ HW to re-fetch command buffer again. */
					cmdq_core_invalidate_hw_fetched_buffer(thread);

					break;
				}
			}
		}
	} while (0);

	if (pThread->taskCount <= 0)
		cmdq_core_disable_HW_thread(thread);
	else
		cmdq_core_resume_HW_thread(thread);


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
			CMDQ_AEE(module, "%s in CMDQ IRQ:0x%02x, INST:(0x%08x, 0x%08x), OP:%s\n",
				 module, irqFlag, instA, instB, cmdq_core_parse_op(op));
			break;
		}
	}

	return status;
}

/**
 * Re-fetch thread's command buffer
 * Usage:
 *     If SW motifies command buffer content after SW configed command to GCE,
 *     SW should notify GCE to re-fetch command in order to ensure inconsistent command buffer content
 *     between DRAM and GCE's SRAM. */
void cmdq_core_invalidate_hw_fetched_buffer(int32_t thread)
{
	/* Setting HW thread PC will invoke that */
	/* GCE (CMDQ HW) gives up fetched command buffer, and fetch command from DRAM to GCE's SRAM again. */
	const int32_t pc = CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread));

	CMDQ_REG_SET32(CMDQ_THR_CURR_ADDR(thread), pc);
}

static int32_t cmdq_core_wait_task_done(struct TaskStruct *pTask, long timeout_jiffies)
{
	int32_t waitQ;
	int32_t status;
	uint32_t thread;
	struct ThreadStruct *pThread = NULL;
	/* error report */

	status = 0;		/* Default status */

	thread = pTask->thread;
	if (CMDQ_INVALID_THREAD == thread) {
		CMDQ_PROF_MMP(cmdq_mmp_get_event()->wait_thread,
			      MMProfileFlagPulse, (uint32_t) pTask, -1);

		CMDQ_PROF_START(current->pid, "wait_for_thread");

		CMDQ_LOG("pid:%d task:0x%p wait for valid thread first\n", current->pid, pTask);

		/* wait for acquire thread (this is done by cmdq_core_consume_waiting_list); */
		waitQ = wait_event_timeout(gCmdqThreadDispatchQueue,
					   (CMDQ_INVALID_THREAD != pTask->thread),
					   msecs_to_jiffies(CMDQ_ACQUIRE_THREAD_TIMEOUT_MS));

		CMDQ_PROF_END(current->pid, "wait_for_thread");
		if (0 == waitQ || CMDQ_INVALID_THREAD == pTask->thread) {
			mutex_lock(&gCmdqTaskMutex);
			/* it's possible that the task was just consumed now. */
			/* so check again. */
			if (CMDQ_INVALID_THREAD == pTask->thread) {
				/* task may already released, or starved to death */
				CMDQ_ERR("task 0x%p timeout with invalid thread\n", pTask);
				cmdq_core_dump_task(pTask);
				cmdq_core_dump_task_with_engine_flag(pTask->engineFlag);
				/* remove from waiting list, */
				/* so that it won't be consumed in the future */
				list_del_init(&(pTask->listEntry));

				mutex_unlock(&gCmdqTaskMutex);
				return -EINVAL;
			}

			/* valid thread, so we keep going */
			mutex_unlock(&gCmdqTaskMutex);
		}
	}

	thread = pTask->thread;
	if ((0 > thread) || (CMDQ_MAX_THREAD_COUNT <= thread)) {
		CMDQ_ERR("invalid thread %d in %s\n", thread, __func__);
		return -EINVAL;
	}
	pThread = &(gCmdqContext.thread[thread]);

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->wait_task,
		      MMProfileFlagPulse, (uint32_t) pTask, thread);

	CMDQ_PROF_START(current->pid, "wait_for_task_done");

	/* start to wait */
	pTask->beginWait = sched_clock();
	CMDQ_MSG("-->WAIT: task 0x%p on thread %d timeout: %d(ms) begin\n", pTask, thread,
		 jiffies_to_msecs(timeout_jiffies));
	waitQ = cmdq_core_wait_task_done_with_timeout_impl(pTask, thread);

	/* wake up! */
	/* so the maximum total waiting time would be */
	/* CMDQ_PREDUMP_TIMEOUT_MS * CMDQ_PREDUMP_RETRY_COUNT */
	pTask->wakedUp = sched_clock();
	CMDQ_MSG("WAIT: task 0x%p waitq=%d state=%d\n", pTask, waitQ, pTask->taskState);
	CMDQ_PROF_END(current->pid, "wait_for_task_done");

	status = (false == pTask->secData.isSecure) ?
	    cmdq_core_handle_wait_task_result_impl(pTask, thread, waitQ) :
	    cmdq_core_handle_wait_task_result_secure_impl(pTask, thread, waitQ);

	CMDQ_MSG("<--WAIT: task 0x%p on thread %d end\n", pTask, thread);

	return status;
}

#if 0
static int32_t cmdq_core_wait_task_done(struct TaskStruct *pTask, long timeout_jiffies)
{
	int32_t waitQ;
	int32_t status;
	int32_t index;
	unsigned long flags;
	uint32_t thread;
	struct ThreadStruct *pThread = NULL;
	bool markAsErrorTask = false;
	/* error report */
	bool throwAEE = false;
	const char *module = NULL;
	uint32_t instA = 0, instB = 0;
	int32_t retryCount = 0;
	int32_t irqFlag = 0;

	status = 0;		/* Default status */

	thread = pTask->thread;
	if (CMDQ_INVALID_THREAD == thread) {
		CMDQ_PROF_MMP(cmdq_mmp_get_event()->wait_thread,
			      MMProfileFlagPulse, (uint32_t) pTask, -1);

		CMDQ_PROF_START(current->pid, "wait_for_thread");

		CMDQ_LOG("pid:%d task:0x%p wait for valid thread first\n", current->pid, pTask);

		/* wait for acquire thread (this is done by cmdq_core_consume_waiting_list); */
		waitQ = wait_event_timeout(gCmdqThreadDispatchQueue,
					   (CMDQ_INVALID_THREAD != pTask->thread),
					   msecs_to_jiffies(CMDQ_ACQUIRE_THREAD_TIMEOUT_MS));

		CMDQ_PROF_END(current->pid, "wait_for_thread");
		if (0 == waitQ || CMDQ_INVALID_THREAD == pTask->thread) {
			mutex_lock(&gCmdqTaskMutex);
			/* it's possible that the task was just consumed now. */
			/* so check again. */
			if (CMDQ_INVALID_THREAD == pTask->thread) {
				/* task may already released, or starved to death */
				CMDQ_ERR("task 0x%p timeout with invalid thread\n", pTask);
				cmdq_core_dump_task(pTask);
				cmdq_core_dump_task_with_engine_flag(pTask->engineFlag);
				/* remove from waiting list, */
				/* so that it won't be consumed in the future */
				list_del_init(&(pTask->listEntry));

				mutex_unlock(&gCmdqTaskMutex);
				return -EINVAL;
			}
			/* valid thread, so we keep going */
			mutex_unlock(&gCmdqTaskMutex);
		}
	}

	thread = pTask->thread;
	pThread = &(gCmdqContext.thread[thread]);

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->wait_task,
		      MMProfileFlagPulse, (uint32_t) pTask, thread);

	CMDQ_PROF_START(current->pid, "wait_for_task_done");

	pTask->beginWait = sched_clock();

	CMDQ_MSG("-->WAIT: task 0x%p on thread %d timeout: %d(ms) begin\n", pTask, thread,
		 jiffies_to_msecs(timeout_jiffies));

	/* timeout wait & make sure this task is finished. */
	/* pTask->taskState flag is updated in IRQ handlers like cmdqCoreHandleDone. */
	retryCount = 0;
	waitQ = wait_event_timeout(gCmdWaitQueue[thread],
				   (TASK_STATE_BUSY != pTask->taskState
				    && TASK_STATE_WAITING != pTask->taskState),
				   /* timeout_jiffies); */
				   msecs_to_jiffies(CMDQ_PREDUMP_TIMEOUT_MS));

	/* if SW-timeout, pre-dump hang instructions */
	while (0 == waitQ && retryCount < CMDQ_PREDUMP_RETRY_COUNT) {
		CMDQ_LOG("=============== [CMDQ] SW timeout Pre-dump(%d)===============\n",
			 retryCount);
		++retryCount;

		spin_lock_irqsave(&gCmdqExecLock, flags);
		cmdq_core_dump_status("INFO");
		cmdq_core_dump_pc(pTask, thread, "INFO");
		/* HACK: check trigger thread status */
		/* we assume the first non-high-priority thread is trigger loop thread. */
		/* since it will start very early */
		if (gCmdqContext.thread[CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT].taskCount
		    && gCmdqContext.thread[CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT].pCurTask[1]
		    && gCmdqContext.thread[CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT].loopCallback) {
			uint32_t regValue = 0;

			cmdq_core_dump_pc(gCmdqContext.thread[CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT].
					  pCurTask[1], CMDQ_MAX_HIGH_PRIORITY_THREAD_COUNT, "INFO");
			CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_ID, CMDQ_EVENT_DISP_RDMA0_EOF);
			regValue = CMDQ_REG_GET32(CMDQ_SYNC_TOKEN_VAL);
			CMDQ_LOG("CMDQ_SYNC_TOKEN_VAL of %s is %d\n",
				 cmdq_core_get_event_name(CMDQ_EVENT_DISP_RDMA0_EOF), regValue);
		}
		/* end of HACK */

		spin_unlock_irqrestore(&gCmdqExecLock, flags);

		/* then we wait again */
		waitQ = wait_event_timeout(gCmdWaitQueue[thread],
					   (TASK_STATE_BUSY != pTask->taskState
					    && TASK_STATE_WAITING != pTask->taskState),
					   msecs_to_jiffies(CMDQ_PREDUMP_TIMEOUT_MS));
	}

	/* so the maximum total waiting time would be */
	/* CMDQ_PREDUMP_TIMEOUT_MS * CMDQ_PREDUMP_RETRY_COUNT */
	pTask->wakedUp = sched_clock();
	CMDQ_MSG("WAIT: task 0x%p waitq=%d state=%d\n", pTask, waitQ, pTask->taskState);
	CMDQ_PROF_END(current->pid, "wait_for_task_done");

	/* Note that although we disable IRQ, HW continues to execute */
	/* so it's possible to have pending IRQ */
	spin_lock_irqsave(&gCmdqExecLock, flags);

	do {
		struct TaskStruct *pNextTask = NULL;
		struct TaskStruct *pPrevTask = NULL;
		struct TaskStruct *pRunningTask = NULL;
		int32_t cookie = 0;
		long threadPC = 0L;

		status = 0;
		throwAEE = false;
		markAsErrorTask = false;

		if (TASK_STATE_DONE == pTask->taskState)
			break;


		CMDQ_ERR("Task state of %p is not TASK_STATE_DONE, %d\n", pTask, pTask->taskState);

		/* Oops, tha tasks is not done. */
		/* We have several possible error scenario: */
		/* 1. task still running (hang / timeout) */
		/* 2. IRQ pending (done or error/timeout IRQ) */
		/* 3. task's SW thread has been signaled (e.g. SIGKILL) */

		/* suspend HW thread first, so that we work in a consistent state */
		status = cmdq_core_suspend_HW_thread(thread);
		if (0 > status)
			throwAEE = true;

		/* The cookie of the task currently being processed */
		cookie = CMDQ_GET_COOKIE_CNT(thread) + 1;
		pRunningTask = pThread->pCurTask[cookie % CMDQ_MAX_TASK_IN_THREAD];
		threadPC = CMDQ_AREG_TO_PHYS(CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread)));

		/* process any pending IRQ */
		/* TODO: provide no spin lock version because we already locked. */
		irqFlag = CMDQ_REG_GET32(CMDQ_THR_IRQ_STATUS(thread));
		if (irqFlag & 0x12)
			cmdqCoreHandleError(thread, irqFlag, &pTask->wakedUp);
		else if (irqFlag & 0x01)
			cmdqCoreHandleDone(thread, irqFlag, &pTask->wakedUp);

		CMDQ_REG_SET32(CMDQ_THR_IRQ_STATUS(thread), ~irqFlag);

		/* check if this task has finished after handling pending IRQ */
		if (TASK_STATE_DONE == pTask->taskState)
			break;

		/* Then decide we are SW timeout or SIGNALed (not an error) */
		if (0 == waitQ) {
			/* SW timeout and no IRQ received */
			markAsErrorTask = true;

			/* if we reach here, we're in errornous state. */
			/* print error log immediately. */
			cmdq_core_attach_error_task(pTask, thread);

			CMDQ_ERR("SW timeout of task 0x%p on thread %d\n", pTask, thread);
			throwAEE = true;
			cmdq_core_parse_error(pTask, thread, &module, &irqFlag, &instA, &instB);
			status = -ETIMEDOUT;

		} else if (0 > waitQ) {	/* Task be killed. Not an error, but still need removal. */
			markAsErrorTask = false;

			if (-ERESTARTSYS == waitQ)
				CMDQ_ERR("Task %p KILLED by waitQ = -ERESTARTSYS\n", pTask);
			else if (-EINTR == waitQ)
				CMDQ_ERR("Task %p KILLED by waitQ = -EINTR\n", pTask);
			else
				CMDQ_ERR("Task %p KILLED by waitQ = %d\n", pTask, waitQ);


			status = waitQ;
		}
		/* reset HW engine immediately if we already got error IRQ. */
		if (TASK_STATE_ERROR == pTask->taskState) {
			cmdq_core_reset_hw_engine(pTask->engineFlag);
		} else if (TASK_STATE_BUSY == pTask->taskState) {
			/*  */
			/* if taskState is BUSY, this means we did not reach EOC, did not have error IRQ. */
			/* - remove the task from thread.pCurTask[] */
			/* - and decrease thread.taskCount */
			/* NOTE: after this, the pCurTask will not contain link to pTask anymore. */
			/* and pTask should become TASK_STATE_ERROR */

			/* we find our place in pThread->pCurTask[]. */
			for (index = 0; index < CMDQ_MAX_TASK_IN_THREAD; ++index) {
				if (pThread->pCurTask[index] == pTask) {
					/* update taskCount and pCurTask[] */
					cmdq_core_remove_task_from_thread_array_by_cookie(pThread,
											  index,
											  markAsErrorTask
											  ?
											  TASK_STATE_ERROR
											  :
											  TASK_STATE_KILLED);
					break;
				}
			}
		}
		/* Then, we try remove pTask from the chain of pThread->pCurTask. */
		/* . if HW PC falls in pTask range */
		/* . HW EXEC_CNT += 1 */
		/* . thread.waitCookie += 1 */
		/* . set HW PC to next task head */
		/* . if not, find previous task (whose jump address is pTask->MVABase) */
		/* . check if HW PC points is not at the EOC/JUMP end */
		/* . change jump to fake EOC(no IRQ) */
		/* . insert jump to next task head and increase cmd buffer size */
		/* . if there is no next task, set HW End Address */
		if (pTask->pCMDEnd && threadPC >= pTask->MVABase
		    && threadPC <= (pTask->MVABase + pTask->commandSize)) {
			pNextTask = NULL;

			/* find pTask's jump destination */
			if (0x10000001 == pTask->pCMDEnd[0]) {
				pNextTask =
				    cmdq_core_search_task_by_pc(pTask->pCMDEnd[-1], pThread);
			}

			if (pNextTask) {
				/* cookie already +1 */
				CMDQ_REG_SET32(CMDQ_THR_EXEC_CNT(thread), cookie);
				pThread->waitCookie = cookie + 1;
				CMDQ_REG_SET32(CMDQ_THR_CURR_ADDR(thread),
					       CMDQ_PHYS_TO_AREG(pNextTask->MVABase));
				CMDQ_MSG("WAIT: resume task 0x%p from err\n", pNextTask);
			}
		} else {
			pPrevTask = NULL;
			for (index = 0; index < CMDQ_MAX_TASK_IN_THREAD; ++index) {
				pPrevTask = pThread->pCurTask[index];

				/* find which task JUMP into pTask */
				if (pPrevTask && pPrevTask->pCMDEnd[-1] == pTask->MVABase &&	/* pTask command */
				    pPrevTask->pCMDEnd[0] == 0x10000001) {	/* JUMP */
					/* Fake EOC command */
					/* only increment EXEC_CNT, but no IRQ throw */
					pPrevTask->pCMDEnd[-1] = 0x00000000;
					pPrevTask->pCMDEnd[0] = 0x40000000;

					/* Bypass the task */
					pPrevTask->pCMDEnd[1] = pTask->pCMDEnd[-1];
					pPrevTask->pCMDEnd[2] = pTask->pCMDEnd[0];
					break;
				}
			}
		}
	} while (0);

	if (pThread->taskCount <= 0) {
#if 0
		CMDQ_LOG("bypass disable_HW_thread, thread:%d\n", thread);
#else
		cmdq_core_disable_HW_thread(thread);
#endif
	} else {
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
			CMDQ_AEE(module, "%s in CMDQ IRQ:0x%02x, INST:(0x%08x, 0x%08x), OP:%s\n",
				 module, irqFlag, instA, instB, cmdq_core_parse_op(op));
			break;
		}
	}

	CMDQ_MSG("<--WAIT: task 0x%p on thread %d end\n", pTask, thread);

	return status;
}
#endif
int32_t cmdq_core_stop_secure_path_notify_thread(void)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	int status = 0;
	unsigned long flags;

	mutex_lock(&gCmdqNotifyLoopMutex);
	do {
		if (NULL == gCmdqContext.hNotifyLoop) {
			/* no notify thread */
			CMDQ_LOG("[WARNING]NULL notify loop\n");
			break;
		}
		CMDQ_LOG("stop HW thread 15\n");
		status = cmdqRecStopLoop(gCmdqContext.hNotifyLoop);
		if (0 > status) {
			CMDQ_ERR("stop notify loop failed, status:%d\n", status);
			break;
		}

		/* destroy handle */
		spin_lock_irqsave(&gCmdqExecLock, flags);
		cmdqRecDestroy(gCmdqContext.hNotifyLoop);
		gCmdqContext.hNotifyLoop = NULL;
		spin_unlock_irqrestore(&gCmdqExecLock, flags);

		/* CPU clear event */
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_SECURE_THR_EOF);
	} while (0);
	mutex_unlock(&gCmdqNotifyLoopMutex);
	CMDQ_LOG("stop notify loop success\n");
	return status;
#else
	CMDQ_ERR("SVP not supported\n");
	return 0;
#endif
}

/*start hw thread 15*/
int32_t cmdq_core_start_secure_path_notify_thread(void)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT

	int status = 0;
/* CmdqRecLoopHandle handle; */
	cmdqRecHandle handle;
	unsigned long flags;

	mutex_lock(&gCmdqNotifyLoopMutex);
	do {
		if (NULL != gCmdqContext.hNotifyLoop) {
			/* already created it */
			break;
		}

		/* CPU clear event */
		CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, CMDQ_SYNC_SECURE_THR_EOF);

		/* record command */
		cmdqRecCreate(CMDQ_SCENARIO_SECURE_NOTIFY_LOOP, &handle);
		cmdqRecReset(handle);
		cmdqRecWait(handle, CMDQ_SYNC_SECURE_THR_EOF);
		status = cmdqRecStartLoop(handle);

		if (0 > status) {
			CMDQ_ERR("start notify loop failed, status:%d\n", status);
			break;
		}

		/* update notify handle */
		spin_lock_irqsave(&gCmdqExecLock, flags);
		gCmdqContext.hNotifyLoop = (CmdqRecLoopHandle *) handle;
		spin_unlock_irqrestore(&gCmdqExecLock, flags);
	} while (0);
	mutex_unlock(&gCmdqNotifyLoopMutex);

	return status;
#else
	return 0;
#endif
}


static int32_t cmdq_core_exec_task_async_secure_impl(struct TaskStruct *pTask,
							   int32_t thread)
{
#ifndef CMDQ_SECURE_PATH_SUPPORT
	CMDQ_ERR("SVP feature is not supported\n");
	return -1;
#else
	int32_t status;
	struct ThreadStruct *pThread;
	int32_t cookie;	unsigned long flags;
	char longMsg[CMDQ_LONGSTRING_MAX];
	uint32_t msgOffset;
	int32_t msgMAXSize;

	longMsg[0] = '\0';
	msgOffset = 0;
	msgMAXSize = CMDQ_LONGSTRING_MAX;
	cmdqCoreLongString(false, longMsg, &msgOffset, &msgMAXSize,
			   "-->EXEC: task 0x%p on thread %d begin, VABase: 0x%p,",
			   pTask, thread, pTask->pVABase);
	cmdqCoreLongString(false, longMsg, &msgOffset, &msgMAXSize,
			   " MVABase: %pa, Size: %d, bufferSize: %d, scenario:%d, flag:0x%llx\n",
			   &(pTask->MVABase), pTask->commandSize, pTask->bufferSize,
			   pTask->scenario, pTask->engineFlag);
	if (msgOffset > 0) {
		/* print message */
		CMDQ_MSG("%s", longMsg);
	}

	status = 0;
	pThread = &(gCmdqContext.thread[thread]);

	cmdq_sec_lock_secure_path();

	do {
		/* setup whole patah */
		status = cmdq_sec_allocate_path_resource_unlocked();
		if (0 > status)
			break;

		spin_lock_irqsave(&gCmdqExecLock, flags);

		/* update task's thread info */
		pTask->thread = thread;
		pTask->irqFlag = 0;
		pTask->taskState = TASK_STATE_BUSY;

		/* insert task to pThread's task lsit, and */
		/* delay HW config when entry SWd */
		if (pThread->taskCount <= 0) {
			cookie = 1;
			cmdq_core_insert_task_from_thread_array_by_cookie(pTask, pThread, cookie,
									  true);
		} else {
			/* append directly */
			cookie = pThread->nextCookie;
			cmdq_core_insert_task_from_thread_array_by_cookie(pTask, pThread, cookie,
									  false);
		}

		spin_unlock_irqrestore(&gCmdqExecLock, flags);

		pTask->trigger = sched_clock();

		/* execute */
		status = cmdq_sec_exec_task_async_unlocked(pTask, thread);

		if (0 > status) {
			spin_lock_irqsave(&gCmdqExecLock, flags);
			/* config failed case, dump for more detail */
			cmdq_core_attach_error_task(pTask, thread);
			cmdq_core_remove_task_from_thread_array_by_cookie(pThread, cookie, TASK_STATE_ERROR);
			spin_unlock_irqrestore(&gCmdqExecLock, flags);
		}
	} while (0);

	cmdq_sec_unlock_secure_path();

	return status;
#endif
}

static inline int32_t cmdq_core_exec_find_task_slot(struct TaskStruct **pLast, struct TaskStruct *pTask,
						    int32_t thread, int32_t loop)
{
	int32_t status = 0;
	struct ThreadStruct *pThread;
	struct TaskStruct *pPrev;
	int32_t index;
	int32_t prev;
	int32_t cookie;

	pThread = &(gCmdqContext.thread[thread]);
	cookie = pThread->nextCookie;

	/* Traverse forward to adjust tasks' order according to their priorities */
	for (prev = (cookie % CMDQ_MAX_TASK_IN_THREAD); loop > 0; loop--) {
		index = prev;
		if (index < 0)
			index = CMDQ_MAX_TASK_IN_THREAD - 1;

		prev = index - 1;
		if (prev < 0)
			prev = CMDQ_MAX_TASK_IN_THREAD - 1;

		pPrev = pThread->pCurTask[prev];

		/* Maybe the job is killed, search a new one */
		while ((NULL == pPrev) && (loop > 1)) {
			CMDQ_LOG("pPrev is NULL, prev:%d, loop:%d, index:%d\n", prev, loop, index);
			prev = prev - 1;
			if (prev < 0)
				prev = CMDQ_MAX_TASK_IN_THREAD - 1;

			pPrev = pThread->pCurTask[prev];
			loop--;
		}

		if (NULL == pPrev) {
			cmdq_core_attach_error_task(pTask, thread);
			CMDQ_ERR("Invalid task state for reorder %d %d\n", index, loop);
			status = -EFAULT;
			break;
		}

		if (loop <= 1) {
			CMDQ_MSG("Set current(%d) order for the new task, line:%d\n", index, __LINE__);

			CMDQ_MSG("Original PC %pa, end %pa\n", &pPrev->MVABase,
				 &pPrev->MVABase + pPrev->commandSize);
			CMDQ_MSG("Original instruction 0x%08x, 0x%08x\n", pPrev->pCMDEnd[0],
				 pPrev->pCMDEnd[-1]);

			pThread->pCurTask[index] = pTask;
			/* Jump: Absolute */
			pPrev->pCMDEnd[0] = 0x10000001;
			/* Jump to here */
			pPrev->pCMDEnd[-1] = pTask->MVABase;
			CMDQ_VERBOSE("EXEC: modify jump to %pa, line:%d\n", &(pTask->MVABase), __LINE__);
#ifndef CMDQ_APPEND_WITHOUT_SUSPEND
			/* re-fetch command buffer again. */
			cmdq_core_invalidate_hw_fetched_buffer(thread);
#endif
			break;
		}

		if (pPrev->priority < pTask->priority) {
			CMDQ_LOG("Switch prev(%d, 0x%p) and curr(%d, 0x%p) order\n",
				 prev, pPrev, index, pTask);

			pThread->pCurTask[index] = pPrev;
			pPrev->pCMDEnd[0] = pTask->pCMDEnd[0];
			pPrev->pCMDEnd[-1] = pTask->pCMDEnd[-1];

			/* Boot priority for the task */
			pPrev->priority += CMDQ_MIN_AGE_VALUE;
			pPrev->reorder++;

			pThread->pCurTask[prev] = pTask;
			/* Jump: Absolute */
			pTask->pCMDEnd[0] = 0x10000001;
			/* Jump to here */
			pTask->pCMDEnd[-1] = pPrev->MVABase;

			CMDQ_VERBOSE("EXEC: modify jump to %pa, line:%d\n", &(pPrev->MVABase), __LINE__);
#ifndef CMDQ_APPEND_WITHOUT_SUSPEND
			/* re-fetch command buffer again. */
			cmdq_core_invalidate_hw_fetched_buffer(thread);
#endif
			if (*pLast == pTask) {
				CMDQ_LOG("update pLast from 0x%p to 0x%p\n", pTask, pPrev);
				*pLast = pPrev;
			}
		} else {
			CMDQ_MSG("Set current(%d) order for the new task, line:%d\n", index, __LINE__);

			CMDQ_MSG("Original PC %pa, end %pa\n", &pPrev->MVABase,
				 &pPrev->MVABase + pPrev->commandSize);
			CMDQ_MSG("Original instruction 0x%08x, 0x%08x\n", pPrev->pCMDEnd[0], pPrev->pCMDEnd[-1]);

			pThread->pCurTask[index] = pTask;
			/* Jump: Absolute */
			pPrev->pCMDEnd[0] = 0x10000001;
			/* Jump to here */
			pPrev->pCMDEnd[-1] = pTask->MVABase;

			CMDQ_VERBOSE("EXEC: modify jump to %pa, line:%d\n", &(pTask->MVABase), __LINE__);
#ifndef CMDQ_APPEND_WITHOUT_SUSPEND
			/* re-fetch command buffer again. */
			cmdq_core_invalidate_hw_fetched_buffer(thread);
#endif
			break;
		}
	}

	CMDQ_MSG("Reorder %d tasks for performance end, pLast:0x%p\n", loop, *pLast);

	return status;
}

static int32_t cmdq_core_exec_task_async_impl(struct TaskStruct *pTask, int32_t thread)
{
	int32_t status;
	struct ThreadStruct *pThread;
	struct TaskStruct *pLast;
	unsigned long flags;
	int32_t loop;
	int32_t minimum;
	int32_t cookie;
	int threadPrio = 0;
	char longMsg[CMDQ_LONGSTRING_MAX];
	uint32_t msgOffset;
	int32_t msgMAXSize;

	longMsg[0] = '\0';
	msgOffset = 0;
	msgMAXSize = CMDQ_LONGSTRING_MAX;
	cmdqCoreLongString(false, longMsg, &msgOffset, &msgMAXSize,
			   "-->EXEC: task 0x%p on thread %d begin, VABase: 0x%p, MVABase: %pa,",
			   pTask, thread, pTask->pVABase, &(pTask->MVABase));
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

	if (pThread->taskCount <= 0) {
		bool enablePrefetch;

		CMDQ_MSG("EXEC: new HW thread(%d)\n", thread);

		if (cmdq_core_reset_HW_thread(thread) < 0) {
			spin_unlock_irqrestore(&gCmdqExecLock, flags);
			return -EFAULT;
		}

		CMDQ_REG_SET32(CMDQ_THR_INST_CYCLES(thread),
			       cmdq_core_get_task_timeout_cycle(pThread));
#if 1/* #ifdef _CMDQ_DISABLE_MARKER_ */
		enablePrefetch = cmdq_core_should_enable_prefetch(pTask->scenario);
		if (enablePrefetch) {
			CMDQ_MSG("EXEC: set HW thread(%d) enable prefetch!\n", thread);
			CMDQ_REG_SET32(CMDQ_THR_PREFETCH(thread), 0x1);
		}
#endif
		threadPrio = cmdq_core_priority_from_scenario(pTask->scenario);
		CMDQ_MSG("EXEC: set HW thread(%d) pc:%pa, qos:%d\n", thread, &pTask->MVABase,
			 threadPrio);
		CMDQ_REG_SET32(CMDQ_THR_CURR_ADDR(thread), CMDQ_PHYS_TO_AREG(pTask->MVABase));
		CMDQ_REG_SET32(CMDQ_THR_END_ADDR(thread),
			       CMDQ_PHYS_TO_AREG(pTask->MVABase + pTask->commandSize));
		CMDQ_REG_SET32(CMDQ_THR_CFG(thread), threadPrio & 0x7);	/* bit 0-2 for priority level; */

#if 0				/*add for debug */
		int32_t priority = CMDQ_REG_GET32(CMDQ_THR_CFG(thread)) & 0x07;	/* bit 0-2 for priority level; */

		if (priority == threadPrio) {
			CMDQ_LOG("EXEC: set HW thread(%d) pc:%pa, qos:%d\n", thread,
				 &pTask->MVABase, threadPrio);
		} else {
			CMDQ_LOG("set thread(%d),qos:(%d) failed,actual qos is %d\n",
				 thread, threadPrio, priority);
		}

#endif
		/* For loop thread, do not enable timeout */
		CMDQ_REG_SET32(CMDQ_THR_IRQ_ENABLE(thread), pThread->loopCallback ? 0x011 : 0x013);

		if (pThread->loopCallback) {
			CMDQ_MSG("EXEC: HW thread(%d) in loop func 0x%p\n", thread,
				 pThread->loopCallback);
		}

		minimum = CMDQ_GET_COOKIE_CNT(thread);
		cmdq_core_insert_task_from_thread_array_by_cookie(pTask, pThread, (minimum + 1),
								  true);

		/* verify that we don't corrupt EOC + JUMP pattern */
		cmdq_core_verfiy_command_end(pTask);

#if 0
		pThread->nextCookie = minimum + 1;
		if (pThread->nextCookie > CMDQ_MAX_COOKIE_VALUE) {	/* Reach the maximum cookie */
			pThread->nextCookie = 0;
		}

		cookie = pThread->nextCookie;
		pThread->waitCookie = cookie;

		pThread->pCurTask[cookie % CMDQ_MAX_TASK_IN_THREAD] = pTask;
		/* taskCount must start from 0. */
		/* and we are the first task, so set to 1. */
		pThread->taskCount = 1;
		pThread->allowDispatching = 1;

		pThread->nextCookie += 1;
		if (pThread->nextCookie > CMDQ_MAX_COOKIE_VALUE) {	/* Reach the maximum cookie */
			pThread->nextCookie = 0;
		}
#endif
		/* enable HW thread */
		CMDQ_MSG("enable HW thread(%d)\n", thread);

		CMDQ_PROF_MMP(cmdq_mmp_get_event()->thread_en,
			      MMProfileFlagPulse, thread, pThread->nextCookie - 1);
		CMDQ_REG_SET32(CMDQ_THR_ENABLE_TASK(thread), 0x01);
	} else {
		CMDQ_MSG("EXEC: reuse HW thread(%d), taskCount:%d\n", thread, pThread->taskCount);

		if (pTask->loopCallback) {
			CMDQ_ERR
			    ("[WARNING]trigger loop task 0x%p on thread %d has running task count=%d\n",
			     pTask, thread, pThread->taskCount);
		}

		status = cmdq_core_suspend_HW_thread(thread);
		if (status < 0) {
			spin_unlock_irqrestore(&gCmdqExecLock, flags);
			return status;
		}

		CMDQ_REG_SET32(CMDQ_THR_INST_CYCLES(thread),
			       cmdq_core_get_task_timeout_cycle(pThread));

		cookie = pThread->nextCookie;

		/* Boundary case tested: EOC have been executed, but JUMP is not executed */
		/* Thread PC: 0x9edc0dd8, End: 0x9edc0de0, Curr Cookie: 1, Next Cookie: 2 */
		/* PC = END - 8, EOC is executed */
		/* PC = END - 0, All CMDs are executed */

		if ((CMDQ_AREG_TO_PHYS(CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread))) ==
		     (CMDQ_AREG_TO_PHYS(CMDQ_REG_GET32(CMDQ_THR_END_ADDR(thread))) - 8)) ||
		    (CMDQ_AREG_TO_PHYS(CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread))) ==
		     (CMDQ_AREG_TO_PHYS(CMDQ_REG_GET32(CMDQ_THR_END_ADDR(thread))) - 0))) {
			longMsg[0] = '\0';
			msgOffset = 0;
			msgMAXSize = CMDQ_LONGSTRING_MAX;
			cmdqCoreLongString(true, longMsg, &msgOffset, &msgMAXSize,
					   "EXEC: Set HW thread(%d) pc from 0x%08x(end:0x%08x) to %pa,",
					   thread,
					   CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread)),
					   CMDQ_REG_GET32(CMDQ_THR_END_ADDR(thread)),
					   &pTask->MVABase);
			cmdqCoreLongString(true, longMsg, &msgOffset, &msgMAXSize,
					   " oriNextCookie:%d, oriTaskCount:%d\n",
					   cookie, pThread->taskCount);
			if (msgOffset > 0) {
				/* print message */
				CMDQ_LOG("%s", longMsg);
			}

			/* set to pTask directly */
			CMDQ_REG_SET32(CMDQ_THR_CURR_ADDR(thread),
				       CMDQ_PHYS_TO_AREG(pTask->MVABase));
			CMDQ_REG_SET32(CMDQ_THR_END_ADDR(thread),
				       CMDQ_PHYS_TO_AREG(pTask->MVABase + pTask->commandSize));
			pThread->pCurTask[cookie % CMDQ_MAX_TASK_IN_THREAD] = pTask;
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
				loop = (CMDQ_MAX_COOKIE_VALUE - minimum + 1) + cookie;
			}

			CMDQ_MSG("Reorder task in range [%d, %d] with count %d\n", minimum, cookie,
				 loop);

			/* ALPS01672377 */
			/* .note pThread->taskCount-- when remove task from pThread in ISR */
			/* .In mutlple SW clients or async case, */
			/*clients may continue submit tasks with overlap engines */
			/*  it's okey 0 = abs(pThread->nextCookie, THR_CNT+1) when... */
			/*      .submit task_1, trigger GCE */
			/*      .submit task_2: */
			/*          .GCE exec task1 done */
			/*          .task_2 lock execLock when insert task to thread */
			/*          .task 1's IRQ */

			if (loop < 0) {
				cmdq_core_dump_task_in_thread(thread, true, true, false);

				longMsg[0] = '\0';
				msgOffset = 0;
				msgMAXSize = CMDQ_LONGSTRING_MAX;
				cmdqCoreLongString(true, longMsg, &msgOffset, &msgMAXSize,
						   "Invalid task count(%d) in thread %d for reorder,",
						   loop, thread);
				cmdqCoreLongString(true, longMsg, &msgOffset, &msgMAXSize,
						   " nextCookie:%d, nextCookieHW:%d, pTask:%p\n",
						   pThread->nextCookie, minimum, pTask);
				if (msgOffset > 0) {
					/* print message */
					CMDQ_AEE("CMDQ", "%s", longMsg);
				}
				spin_unlock_irqrestore(&gCmdqExecLock, flags);
				return -EFAULT;
			}
			if (loop > CMDQ_MAX_TASK_IN_THREAD) {
				CMDQ_LOG("loop = %d, execeed max task in thread", loop);
				loop = loop % CMDQ_MAX_TASK_IN_THREAD;
			}
			CMDQ_MSG("Reorder %d tasks for performance begin\n", loop);

			/* By default, pTask is the last task, and insert [cookie % CMDQ_MAX_TASK_IN_THREAD] */
			pLast = pTask;	/* Default last task */

			status = cmdq_core_exec_find_task_slot(&pLast, pTask, thread, loop);
			if (status < 0) {
#ifdef CMDQ_APPEND_WITHOUT_SUSPEND
				cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_APPEND_THR(thread));
#endif
				spin_unlock_irqrestore(&gCmdqExecLock, flags);
				CMDQ_AEE("CMDQ", "Invalid task state for reorder.\n");
				return status;
			}

			/* We must set memory barrier here to make sure we modify jump before enable thread */
			smp_mb();

			CMDQ_REG_SET32(CMDQ_THR_END_ADDR(thread),
				       CMDQ_PHYS_TO_AREG(pLast->MVABase + pLast->commandSize));
			pThread->taskCount++;
			pThread->allowDispatching = 1;
		}

		pThread->nextCookie += 1;
		if (pThread->nextCookie > CMDQ_MAX_COOKIE_VALUE) {	/* Reach the maximum cookie */
			/* Reach the maximum cookie */
			pThread->nextCookie = 0;
		}

		/* verify that we don't corrupt EOC + JUMP pattern */
		cmdq_core_verfiy_command_end(pTask);

		/* resume HW thread */
		CMDQ_PROF_MMP(cmdq_mmp_get_event()->thread_en,
			      MMProfileFlagPulse, thread, pThread->nextCookie - 1);

		cmdq_core_resume_HW_thread(thread);
	}

	spin_unlock_irqrestore(&gCmdqExecLock, flags);

	CMDQ_MSG("<--EXEC: status: %d\n", status);

	return status;
}

static void cmdq_core_handle_done_with_cookie_impl(int32_t thread,
							 int32_t value, CMDQ_TIME *pGotIRQ,
							 const uint32_t cookie)
{
	struct ThreadStruct *pThread;
	int32_t count;
	int32_t inner;

	pThread = &(gCmdqContext.thread[thread]);

	/* do not print excessive message for looping thread */
	if (NULL == pThread->loopCallback) {
#ifdef CONFIG_MTK_FPGA
		/* ASYNC: debug log, use printk_sched to prevent block IRQ handler */
		CMDQ_MSG("IRQ: Done, thread: %d\n", thread);
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
		count = (CMDQ_MAX_COOKIE_VALUE - pThread->waitCookie + 1) + (cookie + 1);
		CMDQ_ERR("IRQ: counter wrapped: waitCookie:%d, hwCookie:%d, count=%d",
			pThread->waitCookie, cookie, count);
	}

	for (inner = (pThread->waitCookie % CMDQ_MAX_TASK_IN_THREAD); count > 0; count--, inner++) {
		if (inner >= CMDQ_MAX_TASK_IN_THREAD)
			inner = 0;


		if (NULL != pThread->pCurTask[inner]) {
			struct TaskStruct *pTask = pThread->pCurTask[inner];

			pTask->gotIRQ = *pGotIRQ;
			pTask->irqFlag = value;
			cmdq_core_remove_task_from_thread_array_by_cookie(pThread,
									  inner, TASK_STATE_DONE);
		}
	}

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->CMDQ_IRQ, MMProfileFlagPulse, thread, cookie);

	pThread->waitCookie = cookie + 1;
	if (pThread->waitCookie > CMDQ_MAX_COOKIE_VALUE)
		pThread->waitCookie -= (CMDQ_MAX_COOKIE_VALUE + 1);	/* min cookie value is 0 */


	wake_up(&gCmdWaitQueue[thread]);
}

static void cmdq_core_handle_secure_thread_done_impl(const int32_t thread,
							   const int32_t value, CMDQ_TIME *pGotIRQ)
{
	uint32_t cookie;

	/* get cookie value from shared memory */
	if (0 > cmdq_core_get_secure_thread_exec_counter(thread, &cookie))
		return;


	cmdq_core_handle_done_with_cookie_impl(thread, value, pGotIRQ, cookie);

}

static void cmdq_core_handle_secure_paths_exec_done_notify(const int32_t notifyThread,
								 const int32_t value,
								 CMDQ_TIME *pGotIRQ)
{
	uint32_t i;
	uint32_t raisedIRQ;
	int32_t thread;
	uint32_t secure_exec_counter[3];
	const uint32_t startThread = CMDQ_MIN_SECURE_THREAD_ID;
	const uint32_t endThread = CMDQ_MIN_SECURE_THREAD_ID + CMDQ_MAX_SECURE_THREAD_COUNT;

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
	 * The reason is NWd will receive a notify thread IRQ again after resume notify thread.
	 * The later IRQ let driver scan shared memory again.
	 * (note it's possible that same content in shared memory in such case)
	 */

	/* confirm if it is notify thread */
	if (false == cmdq_core_is_valid_notify_thread_for_secure_path(notifyThread))
		return;



	raisedIRQ = cmdq_core_get_secure_IRQ_status();
	cmdq_core_get_secure_thread_exec_counter(12, &secure_exec_counter[0]);
	cmdq_core_get_secure_thread_exec_counter(13, &secure_exec_counter[1]);
	cmdq_core_get_secure_thread_exec_counter(14, &secure_exec_counter[2]);
	CMDQ_MSG("debug sec IRQ:receive secure world irq in normal world\n");
	CMDQ_MSG("%s, raisedIRQ:0x%08x, shared_cookie(%d, %d, %d)\n",
		 __func__,
		 raisedIRQ, secure_exec_counter[0], secure_exec_counter[1], secure_exec_counter[2]);


	/* update tasks' status according cookie in shared memory */
	for (i = startThread; i < endThread; i++) {
		/* bit X = 1 means thread X raised IRQ */
		if (0 == (raisedIRQ & (0x1 << i)))
			continue;

		thread = i;
		cmdq_core_handle_secure_thread_done_impl(thread, value, pGotIRQ);
	}
	cmdq_core_set_secure_IRQ_status(0x0);
	if (gSecPrintCount)
		gSecPrintCount--;
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

static void cmdqCoreHandleError(int32_t thread, int32_t value, CMDQ_TIME *pGotIRQ)
{
	struct ThreadStruct *pThread = NULL;
	struct TaskStruct *pTask = NULL;
	int32_t cookie;
	int32_t count;
	int32_t inner;
	int status;

	CMDQ_ERR("IRQ: error thread=%d, irq_flag=0x%x\n", thread, value);
	CMDQ_ERR("IRQ: Thread PC: 0x%08x, End PC:0x%08x\n",
		 CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread)),
		 CMDQ_REG_GET32(CMDQ_THR_END_ADDR(thread)));

	pThread = &(gCmdqContext.thread[thread]);

	cookie = CMDQ_GET_COOKIE_CNT(thread);
	/* we assume error happens BEFORE EOC */
	/* because it wouldn't be error if this interrupt is issue by EOC. */
	/* So we should inc by 1 to locate "current" task */
	cookie += 1;

	/* Set the issued task to error state */
#define CMDQ_TEST_PREFETCH_FOR_MULTIPLE_COMMAND
#ifdef CMDQ_TEST_PREFETCH_FOR_MULTIPLE_COMMAND
	cmdq_core_dump_task_in_thread(thread, true, true, true);
#endif
	if (NULL != pThread->pCurTask[cookie % CMDQ_MAX_TASK_IN_THREAD]) {
		pTask = pThread->pCurTask[cookie % CMDQ_MAX_TASK_IN_THREAD];
		pTask->gotIRQ = *pGotIRQ;
		pTask->irqFlag = value;
		cmdq_core_attach_error_task(pTask, thread);
		cmdq_core_remove_task_from_thread_array_by_cookie(pThread,
								  cookie % CMDQ_MAX_TASK_IN_THREAD,
								  TASK_STATE_ERROR);
	} else {
		CMDQ_ERR
		    ("IRQ: can not find task in cmdqCoreHandleError, pc:0x%08x, end_pc:0x%08x\n",
		     CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(thread)),
		     CMDQ_REG_GET32(CMDQ_THR_END_ADDR(thread)));
		if (0 >= pThread->taskCount) {
			/* suspend HW thread first, so that we work in a consistent state */
			/* outer function should acquire spinlock - gCmdqExecLock */
			status = cmdq_core_suspend_HW_thread(thread);
			if (0 > status)
				CMDQ_ERR("IRQ: suspend HW thread failed!");

			cmdq_core_disable_HW_thread(thread);
			CMDQ_ERR("IRQ: there is no task for thread (%d) cmdqCoreHandleError\n",
				 thread);
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
		count = (CMDQ_MAX_COOKIE_VALUE - pThread->waitCookie + 1) + (cookie + 1);
		CMDQ_ERR("IRQ: counter wrapped: waitCookie:%d, hwCookie:%d, count=%d",
			pThread->waitCookie, cookie, count);
	}

	for (inner = (pThread->waitCookie % CMDQ_MAX_TASK_IN_THREAD); count > 0; count--, inner++) {
		if (inner >= CMDQ_MAX_TASK_IN_THREAD)
			inner = 0;


		if (NULL != pThread->pCurTask[inner]) {
			pTask = pThread->pCurTask[inner];
			pTask->gotIRQ = *pGotIRQ;
			/* pTask->gotIRQ = sched_clock(); */
			pTask->irqFlag = 0;	/* we don't know the exact irq flag. */
			cmdq_core_remove_task_from_thread_array_by_cookie(pThread,
									  inner, TASK_STATE_DONE);
		}
	}

	pThread->waitCookie = cookie + 1;
	if (pThread->waitCookie > CMDQ_MAX_COOKIE_VALUE)
		pThread->waitCookie -= (CMDQ_MAX_COOKIE_VALUE + 1);	/* min cookie value is 0 */


	wake_up(&gCmdWaitQueue[thread]);
}

static void cmdqCoreHandleDone(int32_t thread, int32_t value, CMDQ_TIME *pGotIRQ)
{
	struct ThreadStruct *pThread;
	int32_t cookie;
	int32_t loopResult = 0;

	pThread = &(gCmdqContext.thread[thread]);

	/*  */
	/* Loop execution never gets done; unless */
	/* user loop function returns error */
	/*  */
	if (NULL != pThread->loopCallback) {
		loopResult = pThread->loopCallback(pThread->loopData);

		CMDQ_PROF_MMP(cmdq_mmp_get_event()->loopBeat,
			      MMProfileFlagPulse, thread, loopResult);

		/* HACK: there are some seucre task execue done */
		cmdq_core_handle_secure_paths_exec_done_notify(thread, value, pGotIRQ);

		if (loopResult >= 0) {
#ifdef CMDQ_PROFILE_COMMAND_TRIGGER_LOOP
			/* HACK */
			if (pThread->pCurTask[1])
				cmdq_core_track_task_record(pThread->pCurTask[1], thread);

#endif
			/* Success, contiue execution as if nothing happens */
			CMDQ_REG_SET32(CMDQ_THR_IRQ_STATUS(thread), ~value);
			return;
		}
	}
	/* do not print excessive message for looping thread */
	if (NULL == pThread->loopCallback) {
#ifdef CONFIG_MTK_FPGA
		/* ASYNC: debug log, use printk_sched to prevent block IRQ handler */
		CMDQ_MSG("IRQ: Done, thread: %d\n", thread);
#endif
	}

	if (loopResult < 0) {
		/* The loop CB failed, so stop HW thread now. */
		cmdq_core_disable_HW_thread(thread);

		/* loop CB failed. the EXECUTION count should not be used as cookie, */
		/* since it will increase by each loop iteration. */
		cookie = pThread->waitCookie;
	} else {
		/* Normal task cookie */
		cookie = CMDQ_GET_COOKIE_CNT(thread);
		CMDQ_MSG("Done: thread %d got cookie: %d\n", thread, cookie);
	}

	cmdq_core_handle_done_with_cookie_impl(thread, value, pGotIRQ, cookie);
#if 0
	if (pThread->waitCookie <= cookie) {
		count = cookie - pThread->waitCookie + 1;
	} else {
		/* Counter wrapped */
		count = (CMDQ_MAX_COOKIE_VALUE - pThread->waitCookie + 1) + (cookie + 1);
	}

	for (inner = (pThread->waitCookie % CMDQ_MAX_TASK_IN_THREAD); count > 0; count--, inner++) {
		if (inner >= CMDQ_MAX_TASK_IN_THREAD)
			inner = 0;


		if (NULL != pThread->pCurTask[inner]) {
			struct TaskStruct *pTask = pThread->pCurTask[inner];

			pTask->gotIRQ = *pGotIRQ;
			pTask->irqFlag = value;
			cmdq_core_remove_task_from_thread_array_by_cookie(pThread,
									  inner, TASK_STATE_DONE);
		}
	}

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->CMDQ_IRQ, MMProfileFlagPulse, thread, cookie);

	pThread->waitCookie = cookie + 1;
	if (pThread->waitCookie > CMDQ_MAX_COOKIE_VALUE)
		pThread->waitCookie -= (CMDQ_MAX_COOKIE_VALUE + 1);	/* min cookie value is 0 */


	wake_up(&gCmdWaitQueue[thread]);
#endif
}

void cmdqCoreHandleIRQ(int32_t thread)
{
	unsigned long flags = 0L;
	CMDQ_TIME gotIRQ;
	int value;
	int enabled;
	int32_t cookie;

	/* note that do_gettimeofday may cause HWT in spin_lock_irqsave (ALPS01496779) */
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

	if (0 == (value & 0x13)) {
		CMDQ_ERR("IRQ: thread %d got interrupt but IRQ flag is 0x%08x in NWd\n", thread,
			 value);
		spin_unlock_irqrestore(&gCmdqExecLock, flags);
		return;
	}

	if (false == cmdq_core_is_a_secure_thread(thread)) {
		enabled = CMDQ_REG_GET32(CMDQ_THR_ENABLE_TASK(thread));

		if (0 == (enabled & 0x01)) {
			CMDQ_ERR("IRQ: thread %d got interrupt already disabled 0x%08x\n", thread,
				 enabled);
			spin_unlock_irqrestore(&gCmdqExecLock, flags);
			return;
		}
	}

#ifdef CMDQ_PROFILE
	CMDQ_PROF_START(0, gCmdqThreadLabel[thread]);
#endif

	/* Read HW cookie here to print message only */
	cookie = CMDQ_GET_COOKIE_CNT(thread);
	/* Move the reset IRQ before read HW cookie to prevent race condition and save the cost of suspend */
	CMDQ_REG_SET32(CMDQ_THR_IRQ_STATUS(thread), ~value);
	CMDQ_MSG("IRQ: thread %d got interrupt, after reset, and IRQ flag is 0x%08x, cookie: %d\n",
		 thread, value, cookie);

	if (value & 0x12)
		cmdqCoreHandleError(thread, value, &gotIRQ);
	else if (value & 0x01)
		cmdqCoreHandleDone(thread, value, &gotIRQ);

#ifdef CMDQ_PROFILE
	CMDQ_PROF_END(0, gCmdqThreadLabel[thread]);
#endif

	spin_unlock_irqrestore(&gCmdqExecLock, flags);
}

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

#ifdef CMDQ_SECURE_PATH_SUPPORT
	/* destroy secure path notify thread */
	cmdq_core_stop_secure_path_notify_thread();
#endif

	pEngine = gCmdqContext.engine;

	execThreads = CMDQ_REG_GET32(CMDQ_CURR_LOADED_THR);
	refCount = atomic_read(&gCmdqThreadUsage);

	if (0 > cmdq_core_can_module_entry_suspend(pEngine)) {
		CMDQ_ERR("[SUSPEND] MDP running, kill tasks. threads:0x%08x, ref:%d\n", execThreads,
			 refCount);
		killTasks = true;
	} else if ((refCount > 0) || (0x80000000 & execThreads)) {
		CMDQ_ERR("[SUSPEND] other running, kill tasks. threads:0x%08x, ref:%d\n",
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
			cmdq_core_dump_task(pTask);
		}

		/* remove all active task from thread */
		CMDQ_ERR("[SUSPEND] remove all active tasks\n");
		list_for_each(p, &gCmdqContext.taskActiveList) {
			pTask = list_entry(p, struct TaskStruct, listEntry);
			if (pTask->thread != CMDQ_INVALID_THREAD) {
				spin_lock_irqsave(&gCmdqExecLock, flags);
				cmdq_core_force_remove_task_from_thread(pTask, pTask->thread);
				pTask->taskState = TASK_STATE_KILLED;
				spin_unlock_irqrestore(&gCmdqExecLock, flags);

				/* release all thread and mark all active tasks as "KILLED" */
				/* (so that thread won't release again) */
				CMDQ_ERR("[SUSPEND] release all threads and HW clocks\n");
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
		CMDQ_PROF_MMP(cmdq_mmp_get_event()->consume_add, MMProfileFlagPulse, 0, 0);
		queue_work(gCmdqContext.taskConsumeWQ, &gCmdqContext.taskConsumeWaitQueueItem);
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
	CMDQ_VERBOSE("[RESUME] cmdqCoreResumedNotifier\n");
	return cmdq_core_reume_impl("RESUME_NOTIFIER");
}

int32_t cmdqCoreEarlySuspend(void)
{
	/* destroy secure path notify thread */
	cmdq_core_stop_secure_path_notify_thread();
	atomic_set(&gCmdqEarlySuspended, 1);
	return 0;
}

int32_t cmdqCoreLateResume(void)
{
	atomic_set(&gCmdqEarlySuspended, 0);
	return 0;
}

bool cmdqCoreIsEarlySuspended(void)
{
	return atomic_read(&gCmdqEarlySuspended);
}

static int32_t cmdq_core_exec_task_async_with_retry(struct TaskStruct *pTask, int32_t thread)
{
	int32_t retry;
	int32_t status;

	retry = 0;
	status = -EFAULT;
	do {
		cmdq_core_verfiy_command_end(pTask);

		status = (false == pTask->secData.isSecure) ?
		    (cmdq_core_exec_task_async_impl(pTask, thread)) :
		    (cmdq_core_exec_task_async_secure_impl(pTask, thread));

#if 0				/* add for debug */
		if (true == pTask->secData.isSecure) {
			CMDQ_ERR("file:%s line:%d secure task status:0x%x\n", __FILE__,
				 __LINE__, pTask->taskState);
		}
#endif

		if (status >= 0)
			break;


		if ((TASK_STATE_KILLED == pTask->taskState) ||
		    (TASK_STATE_ERROR == pTask->taskState)) {
			CMDQ_ERR("cmdq_core_exec_task_async_impl fail\n");
			status = -EFAULT;
			break;
		}

		++retry;
	} while (retry < CMDQ_MAX_RETRY_COUNT);

	return status;
}

static void cmdq_core_consume_waiting_list(struct work_struct *_ignore)
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
	char longMsg[CMDQ_LONGSTRING_MAX];
	uint32_t msgOffset;
	int32_t msgMAXSize;
	bool needPrintLog;

	/* when we're suspending, do not execute any tasks. delay & hold them. */
	if (gCmdqSuspended)
		return;


	CMDQ_PROF_START(current->pid, __func__);
	CMDQ_PROF_MMP(cmdq_mmp_get_event()->consume_done, MMProfileFlagStart, current->pid, 0);
	consumeTime = sched_clock();

	mutex_lock(&gCmdqTaskMutex);

	threadAcquired = false;

	/* scan and remove (if executed) waiting tasks */
	list_for_each_safe(p, n, &gCmdqContext.taskWaitList) {
		pTask = list_entry(p, struct TaskStruct, listEntry);

		thread_prio = cmdq_core_priority_from_scenario(pTask->scenario);

		longMsg[0] = '\0';
		msgOffset = 0;
		msgMAXSize = CMDQ_LONGSTRING_MAX;
		needPrintLog = cmdq_core_should_print_msg(LOG_LEVEL_MSG);
		cmdqCoreLongString(needPrintLog, longMsg, &msgOffset, &msgMAXSize,
				   "-->THREAD: try acquire thread for task: 0x%p, thread_prio: %d,",
				   pTask, thread_prio);
		cmdqCoreLongString(needPrintLog, longMsg, &msgOffset, &msgMAXSize,
				   " task_prio: %d, flag: 0x%llx, scenario:%d begin\n",
				   pTask->priority, pTask->engineFlag, pTask->scenario);
		if (msgOffset > 0) {
			/* print message */
			CMDQ_MSG("%s", longMsg);
		}

		CMDQ_GET_TIME_IN_MS(pTask->submit, consumeTime, waitingTimeMS);
		needLog = waitingTimeMS >= CMDQ_PREDUMP_TIMEOUT_MS;
		/* Allocate hw thread */
		thread = cmdq_core_acquire_thread(pTask->engineFlag,
						  thread_prio, pTask->scenario, needLog,
						  pTask->secData.isSecure);
		pThread = &gCmdqContext.thread[thread];

		if (CMDQ_INVALID_THREAD == thread || NULL == pThread) {
			/* have to wait, remain in wait list */
			CMDQ_MSG("<--THREAD: acquire thread fail, need to wait\n");
			if (needLog) {
				/* task wait too long */
				longMsg[0] = '\0';
				msgOffset = 0;
				msgMAXSize = CMDQ_LONGSTRING_MAX;
				needPrintLog = cmdq_core_should_print_msg(LOG_LEVEL_ERR);
				cmdqCoreLongString(needPrintLog, longMsg, &msgOffset, &msgMAXSize,
						   "acquire thread pre-dump for task: 0x%p,",
						   pTask);
				cmdqCoreLongString(needPrintLog, longMsg, &msgOffset, &msgMAXSize,
						   " thread_prio: %d, task_prio: %d, flag: 0x%llx,",
						   thread_prio, pTask->priority, pTask->engineFlag);
				cmdqCoreLongString(needPrintLog, longMsg, &msgOffset, &msgMAXSize,
						   " scenario:%d, submit:%#llx, state=%d\n",
						   pTask->scenario, pTask->submit,
						   pTask->taskState);
				if (msgOffset > 0) {
					/* print message */
					/* for Error Handle */
					CMDQ_MSG("%s", longMsg);
				}

				dumpTriggerLoop =
				    (CMDQ_SCENARIO_PRIMARY_DISP ==
				     pTask->scenario) ? (true) : (dumpTriggerLoop);
			}
			continue;
		}
		/* some task is ready to run */
		threadAcquired = true;
		/* Assign loop function if the thread should be a loop thread */
		pThread->loopCallback = pTask->loopCallback;
		pThread->loopData = pTask->loopData;
		if (pTask->loopCallback)
			CMDQ_MSG("[TRIGGER]loop 0x%p on thread %d\n", pTask, thread);

		/* Start execution, */
		/* remove from wait list and put into active list */
		list_del_init(&(pTask->listEntry));
		list_add_tail(&(pTask->listEntry), &gCmdqContext.taskActiveList);

		CMDQ_MSG("<--THREAD: acquire thread w/flag: 0x%llx on thread(%d): 0x%p end\n",
			 pTask->engineFlag, thread, pThread);

		/* Run task on thread */
		status = cmdq_core_exec_task_async_with_retry(pTask, thread);
		if (status < 0) {
			CMDQ_ERR
			    ("<--THREAD: cmdq_core_exec_task_async_with_retry fail, release task 0x%p\n",
			     pTask);
			cmdq_core_track_task_record(pTask, thread);
			cmdq_core_release_thread(pTask);
			cmdq_core_release_task_unlocked(pTask);
			pTask = NULL;
		}
	}

	if (dumpTriggerLoop) {
		/* HACK: observe trigger loop status when acquire config thread failed. */
		cmdq_core_dump_disp_trigger_loop();
		cmdq_core_dump_thread_pc(cmdq_core_disp_thread_index_from_scenario
					 (CMDQ_SCENARIO_PRIMARY_DISP));
	}

	if (threadAcquired) {
		/* notify some task's SW thread to change their waiting state. */
		/* (if they already called cmdqCoreWaitResultAndReleaseTask()) */
		wake_up_all(&gCmdqThreadDispatchQueue);
	}

	mutex_unlock(&gCmdqTaskMutex);

	CMDQ_PROF_END(current->pid, __func__);

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->consume_done, MMProfileFlagEnd, current->pid, 0);

}

int32_t cmdqCoreSubmitTaskAsyncImpl(struct cmdqCommandStruct *pCommandDesc,
				    CmdqInterruptCB loopCB,
				    unsigned long loopData, struct TaskStruct **ppTaskOut)
{
	struct TaskStruct *pTask = NULL;

	if (CMDQ_SCENARIO_TRIGGER_LOOP != pCommandDesc->scenario)
		cmdq_core_verfiy_command_desc_end(pCommandDesc);

	if (bCountEnable)
		gTaskCount[cmdq_core_get_thread_index_from_scenario_and_secure_data
			   (pCommandDesc->scenario, pCommandDesc->secData.isSecure)] += 1;

	CMDQ_MSG("-->SUBMIT_ASYNC: cmd 0x%p begin\n", CMDQ_U32_PTR(pCommandDesc->pVABase));
	CMDQ_PROF_START(current->pid, __func__);

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->alloc_task, MMProfileFlagStart, current->pid, 0);


	/* Allocate Task. This creates a new task */
	/* and put into tail of waiting list */
	pTask = cmdq_core_acquire_task(pCommandDesc, loopCB, loopData);

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->alloc_task, MMProfileFlagEnd, current->pid, 0);

	if (NULL == pTask) {
		CMDQ_PROF_END(current->pid, __func__);
		return -EFAULT;
	}

	if (NULL != ppTaskOut)
		*ppTaskOut = pTask;

	/* consume the waiting list. */
	/* this may or may not execute the task, */
	/* depending on available threads. */
	cmdq_core_consume_waiting_list(NULL);

	CMDQ_MSG("<--SUBMIT_ASYNC: task: 0x%p end\n", CMDQ_U32_PTR(pCommandDesc->pVABase));
	CMDQ_PROF_END(current->pid, __func__);
	return 0;
}

int32_t cmdqCoreSubmitTaskAsync(struct cmdqCommandStruct *pCommandDesc,
				CmdqInterruptCB loopCB,
				unsigned long loopData, struct TaskStruct **ppTaskOut)
{
	int32_t status = 0;
	struct TaskStruct *pTask = NULL;

	if (pCommandDesc->secData.isSecure) {
		status = cmdq_core_start_secure_path_notify_thread();	/* start HW thread 15 if it's secure task */
		if (0 > status)
			return status;

	}


	status = cmdqCoreSubmitTaskAsyncImpl(pCommandDesc, loopCB, loopData, &pTask);
	if (NULL != ppTaskOut)
		*ppTaskOut = pTask;

	return status;
}

int32_t cmdqCoreReleaseTask(struct TaskStruct *pTask)
{
	unsigned long flags;
	int32_t status = 0;
	int32_t thread = pTask->thread;
	struct ThreadStruct *pThread = &(gCmdqContext.thread[thread]);

	CMDQ_MSG("<--TASK: cmdqCoreReleaseTask 0x%p\n", pTask);

	if (thread != CMDQ_INVALID_THREAD && NULL != pThread) {
		/* this task is being executed (or queueed) on a HW thread */

		/* get SW lock first to ensure atomic access HW */
		spin_lock_irqsave(&gCmdqExecLock, flags);
		smp_mb();	/*coding style requires to comment after smb_mb to explain why need memory_barrier */

		if (pThread->loopCallback) {
			/* a loop thread has only 1 task involved */
			/* so we can release thread directly */
			/* otherwise we need to connect remaining tasks */
			BUG_ON(pThread->taskCount > 1);

			/* suspend and reset the thread */
			/* might meet unsuspendable situation, can be ignored due to following reset */
			/* status = cmdq_core_suspend_HW_thread(thread); */
			/* BUG_ON(status < 0); */
			pThread->taskCount = 0;
			cmdq_core_disable_HW_thread(thread);
		} else {
			/* TODO: we should check thread enabled or not before resume it. */
			status = cmdq_core_force_remove_task_from_thread(pTask, thread);
			if (pThread->taskCount > 0)
				cmdq_core_resume_HW_thread(thread);

		}

		spin_unlock_irqrestore(&gCmdqExecLock, flags);
		wake_up(&gCmdWaitQueue[thread]);
	}

	cmdq_core_track_task_record(pTask, thread);
	cmdq_core_release_thread(pTask);
	cmdq_core_release_task(pTask);
	CMDQ_MSG("-->TASK: cmdqCoreReleaseTask 0x%p end\n", pTask);
	return 0;
}

int32_t cmdqCoreWaitAndReleaseTask(struct TaskStruct *pTask, long timeout_jiffies)
{
	return cmdqCoreWaitResultAndReleaseTask(pTask, NULL, timeout_jiffies);
}

int32_t cmdqCoreWaitResultAndReleaseTask(struct TaskStruct *pTask,
					 struct cmdqRegValueStruct *pResult, long timeout_jiffies)
{
	int32_t status;
	int32_t thread;
	int i;

	if (NULL == pTask) {
		CMDQ_ERR("cmdqCoreWaitAndReleaseTask err ptr=0x%p\n", pTask);
		return -EFAULT;
	}

	if (pTask->taskState == TASK_STATE_IDLE) {
		CMDQ_ERR("cmdqCoreWaitAndReleaseTask task=0x%p is IDLE\n", pTask);
		return -EFAULT;
	}
	CMDQ_PROF_START(current->pid, __func__);


	/*  */
	/* wait for task finish */
	thread = pTask->thread;
	status = cmdq_core_wait_task_done(pTask, timeout_jiffies);

	/*  */
	/* retrieve result */
	if (pResult && pResult->count) {
		/* clear results */
		memset(CMDQ_U32_PTR(pResult->regValues), 0,
		       pResult->count * sizeof(CMDQ_U32_PTR(pResult->regValues)[0]));

		/* fill results */
		mutex_lock(&gCmdqTaskMutex);
		for (i = 0; i < pResult->count && i < pTask->regCount; ++i)
			CMDQ_U32_PTR(pResult->regValues)[i] = pTask->regResults[i];

		mutex_unlock(&gCmdqTaskMutex);
	}


	cmdq_core_track_task_record(pTask, thread);
	cmdq_core_release_thread(pTask);
	cmdq_core_release_task(pTask);

	CMDQ_PROF_END(current->pid, __func__);

	return status;
}

static void cmdq_core_auto_release_work(struct work_struct *workItem)
{
	int32_t status = 0;
	struct TaskStruct *pTask = NULL;
	CmdqAsyncFlushCB finishCallback = NULL;
	uint32_t userData = 0;

	pTask = container_of(workItem, struct TaskStruct, autoReleaseWork);

	if (pTask) {
		finishCallback = pTask->flushCallback;
		userData = pTask->flushData;

		status = cmdqCoreWaitResultAndReleaseTask(pTask,
							  NULL,
							  msecs_to_jiffies
							  (CMDQ_DEFAULT_TIMEOUT_MS));

		CMDQ_VERBOSE("[Auto Release] released pTask=%p, statu=%d\n", pTask, status);
		CMDQ_PROF_MMP(cmdq_mmp_get_event()->autoRelease_done,
			      MMProfileFlagPulse, (uint32_t) pTask, current->pid);

		pTask = NULL;

		/* Notify user */
		if (finishCallback) {
			CMDQ_VERBOSE("[Auto Release] call user callback %p with data 0x%08x\n",
				     finishCallback, userData);
			finishCallback(userData);
		}
	}
}

int32_t cmdqCoreAutoReleaseTask(struct TaskStruct *pTask)
{
	/* the work item is embeded in pTask already */
	/* but we need to initialized it */
	INIT_WORK(&pTask->autoReleaseWork, cmdq_core_auto_release_work);

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->autoRelease_add,
		      MMProfileFlagPulse, (uint32_t) pTask, 0);

	queue_work(gCmdqContext.taskAutoReleaseWQ, &pTask->autoReleaseWork);
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
		if (TASK_STATE_IDLE != pTask->taskState &&
		    pTask->privateData == (cmdqU32Ptr_t) file_node &&
		    (cmdq_core_is_request_from_user_space(pTask->scenario))) {
			CMDQ_LOG
			    ("[WARNING] ACTIVE task 0x%p release because file node 0x%p closed\n",
			     pTask, file_node);
			cmdq_core_dump_task(pTask);

			/* since we already inside mutex, */
			/* do not cmdqReleaseTask directly, */
			/* instead we change state to "KILLED" */
			/* and arrange a auto-release. */
			/* Note that these tasks may already issued to HW */
			/* so there is a chance that following MPU/M4U violation */
			/* may occur, if the user space process has destroyed. */
			/* The ideal solution is to stop / cancel HW operation */
			/* immediately, but we cannot do so due to SMI hang risk. */
			cmdqCoreAutoReleaseTask(pTask);
		}
	}

	list_for_each(p, &gCmdqContext.taskWaitList) {
		pTask = list_entry(p, struct TaskStruct, listEntry);
		if (TASK_STATE_WAITING == pTask->taskState &&
		    pTask->privateData == (cmdqU32Ptr_t) file_node &&
		    (cmdq_core_is_request_from_user_space(pTask->scenario))) {
			CMDQ_LOG
			    ("[WARNING] WAITING task 0x%p release because file node 0x%p closed\n",
			     pTask, file_node);
			cmdq_core_dump_task(pTask);

			/* since we already inside mutex, */
			/* and these WAITING tasks will not be consumed (acquire thread / exec) */
			/* we can release them directly. */
			/* note that we use unlocked version since we already hold gCmdqTaskMutex. */
			cmdq_core_release_task_unlocked(pTask);
		}
	}

	mutex_unlock(&gCmdqTaskMutex);
}

int32_t cmdqCoreSubmitTask(struct cmdqCommandStruct *pCommandDesc)
{
	int32_t status;
	struct TaskStruct *pTask = NULL;

	CMDQ_MSG("-->SUBMIT: SYNC cmd 0x%p begin\n", CMDQ_U32_PTR(pCommandDesc->pVABase));
	status = cmdqCoreSubmitTaskAsync(pCommandDesc, NULL, 0, &pTask);

	if (status >= 0) {
		status = cmdqCoreWaitResultAndReleaseTask(pTask,
							  &pCommandDesc->regValue,
							  msecs_to_jiffies
							  (CMDQ_DEFAULT_TIMEOUT_MS));
		if (status < 0)
			CMDQ_ERR("Task 0x%p wait fails\n", pTask);

	} else {
		CMDQ_ERR("cmdqCoreSubmitTaskAsync failed=%d", status);
	}

	CMDQ_MSG("<--SUBMIT: SYNC cmd 0x%p end\n", CMDQ_U32_PTR(pCommandDesc->pVABase));
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

unsigned long long cmdq_core_get_GPR64(const enum CMDQ_DATA_REGISTER_ENUM regID)
{
#ifdef CMDQ_GPR_SUPPORT
	unsigned long long value;
	unsigned long long value1;
	unsigned long long value2;
	const uint32_t x = regID & 0x0F;

	if (0 < (regID & 0x10)) {
		/* query address GPR(64bit), Px */
		value1 = 0LL | CMDQ_REG_GET32(CMDQ_GPR_R32((2 * x)));
		value2 = 0LL | CMDQ_REG_GET32(CMDQ_GPR_R32((2 * x + 1)));
	} else {
		/* query data GPR(32bit), Rx */
		value1 = 0LL | CMDQ_REG_GET32(CMDQ_GPR_R32(x));
		value2 = 0LL;
	}

	value = (0LL) | (value2 << 32) | (value1);
	CMDQ_VERBOSE("get_GPR64(%x): 0x%llx(0x%llx, 0x%llx)\n", regID, value, value2, value1);

	return value;
#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return 0LL;
#endif
}

void cmdq_core_set_GPR64(const enum CMDQ_DATA_REGISTER_ENUM regID, const unsigned long long value)
{
#ifdef CMDQ_GPR_SUPPORT

	const unsigned long long value1 = 0x00000000FFFFFFFF & value;
	const unsigned long long value2 = 0LL | value >> 32;
	const uint32_t x = regID & 0x0F;
	unsigned long long result;

	if (0 < (regID & 0x10)) {
		/* set address GPR(64bit), Px */
		CMDQ_REG_SET32(CMDQ_GPR_R32((2 * x)), value1);
		CMDQ_REG_SET32(CMDQ_GPR_R32((2 * x + 1)), value2);
	} else {
		/* set data GPR(32bit), Rx */
		CMDQ_REG_SET32(CMDQ_GPR_R32((2 * x)), value1);
	}

	result = 0LL | cmdq_core_get_GPR64(regID);
	if (value != result) {
		CMDQ_ERR("set_GPR64(%x) failed, value is 0x%llx, not value 0x%llx\n", regID, result,
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

void cmdqCoreDeInitialize(void)
{
	struct TaskStruct *pTask = NULL;
	struct list_head *p;
	int index;
	struct list_head *lists[] = {
		&gCmdqContext.taskFreeList, &gCmdqContext.taskActiveList,
		&gCmdqContext.taskWaitList
	};

	/* directly destroy the auto release WQ since we're going to release tasks anyway. */
	destroy_workqueue(gCmdqContext.taskAutoReleaseWQ);
	gCmdqContext.taskAutoReleaseWQ = NULL;

	destroy_workqueue(gCmdqContext.taskConsumeWQ);
	gCmdqContext.taskConsumeWQ = NULL;

	for (index = 0; index < CMDQ_MAX_ERROR_COUNT; index++) {
		if (NULL != gCmdqContext.errorTask.errorTaskBuffer[index].pVABase)
			kfree(gCmdqContext.errorTask.errorTaskBuffer[index].pVABase);

	}
	gCmdqContext.errorTask.errTaskCount = 0;

	/* release all tasks in both list */
	for (index = 0; index < (sizeof(lists) / sizeof(lists[0])); ++index) {
		list_for_each(p, lists[index]) {
			mutex_lock(&gCmdqTaskMutex);

			pTask = list_entry(p, struct TaskStruct, listEntry);

			/* free allocated DMA buffer */
			cmdq_task_free_task_command_buffer(pTask);
			cmdq_core_release_sec_metadata(pTask);
			kmem_cache_free(gCmdqContext.taskCache, pTask);
			list_del(p);

			mutex_unlock(&gCmdqTaskMutex);
		}
	}

	/* check if there are dangling write addresses. */
	if (!list_empty(&gCmdqContext.writeAddrList))
		CMDQ_AEE("CMDQ", "there are unreleased write buffer");


	kmem_cache_destroy(gCmdqContext.taskCache);
	gCmdqContext.taskCache = NULL;


	/* release emergency buffer */
	cmdq_core_uninit_emergency_buffer();

#ifdef CMDQ_SECURE_PATH_SUPPORT
	/* Deinitialize secure path context */
	cmdqSecDeInitialize();
#endif

	destroy_workqueue(gCmdqPrint.CmdqPrintWQ);
	gCmdqPrint.CmdqPrintWQ = NULL;
}

void cmdqCoreSetEvent(enum CMDQ_EVENT_ENUM event)
{
	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_UPD, (1L << 16) | event);
}

uint32_t cmdqCoreGetEvent(enum CMDQ_EVENT_ENUM event)
{
	uint32_t regValue = 0;

	CMDQ_REG_SET32(CMDQ_SYNC_TOKEN_ID, (0x3FF && event));
	regValue = CMDQ_REG_GET32(CMDQ_SYNC_TOKEN_VAL);
	return regValue;
}

int cmdqCoreAllocWriteAddress(uint32_t count, dma_addr_t *paStart)
{
	struct WriteAddrStruct *pWriteAddr = NULL;
	int status = 0;

	CMDQ_VERBOSE("ALLOC: line %d\n", __LINE__);

	do {
		if (NULL == paStart) {
			CMDQ_ERR("invalid output argument\n");
			status = -EINVAL;
			break;
		}
		*paStart = 0;

		CMDQ_VERBOSE("ALLOC: line %d\n", __LINE__);

		pWriteAddr = kzalloc(sizeof(struct WriteAddrStruct), GFP_KERNEL);
		if (NULL == pWriteAddr) {
			CMDQ_ERR("failed to alloc struct WriteAddrStruct\n");
			status = -ENOMEM;
			break;
		}
		memset(pWriteAddr, 0, sizeof(struct WriteAddrStruct));

		CMDQ_VERBOSE("ALLOC: line %d\n", __LINE__);

		pWriteAddr->count = count;
		pWriteAddr->va =
		    cmdq_core_alloc_hw_buffer(cmdq_dev_get(), count * sizeof(uint32_t),
					      &(pWriteAddr->pa), GFP_KERNEL);
		if (current)
			pWriteAddr->user = current->pid;


		CMDQ_VERBOSE("ALLOC: line %d\n", __LINE__);

		if (NULL == pWriteAddr->va) {
			CMDQ_ERR("failed to alloc write buffer\n");
			status = -ENOMEM;
			break;
		}

		CMDQ_VERBOSE("ALLOC: line %d\n", __LINE__);

		/* clear buffer content */
		do {
			volatile uint32_t *pInt = (uint32_t *) pWriteAddr->va;
			int i = 0;

			for (i = 0; i < count; ++i) {
				*(pInt + i) = 0xcdcdabab;
				/*coding style requires to comment after smb_mb to explain why need memory_barrier */
				mb();
				/* make sure instructions are really in DRAM */
				smp_mb();
			}
		} while (0);

		/* assign output pa */
		*paStart = pWriteAddr->pa;

		mutex_lock(&gCmdqWriteAddrMutex);
		list_add_tail(&(pWriteAddr->list_node), &gCmdqContext.writeAddrList);
		mutex_unlock(&gCmdqWriteAddrMutex);

		status = 0;

	} while (0);

	if (0 != status) {
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

	/* search for the entry */
	mutex_lock(&gCmdqWriteAddrMutex);
	list_for_each(p, &gCmdqContext.writeAddrList) {
		pWriteAddr = list_entry(p, struct WriteAddrStruct, list_node);
		if (NULL == pWriteAddr)
			continue;


		offset = pa - pWriteAddr->pa;

		if (offset >= 0 && (offset / sizeof(uint32_t)) < pWriteAddr->count) {
			/*line over 120 characters,so devide to two statments */
			CMDQ_VERBOSE
			    ("cmdqCoreReadWriteAddress() input:%pa, got offset=%d va=%p pa_start=%pa\n",
			     &pa, offset, (pWriteAddr->va + offset), &(pWriteAddr->pa));
			value = *((volatile uint32_t *)(pWriteAddr->va + offset));
			CMDQ_VERBOSE
			    ("cmdqCoreReadWriteAddress() found offset=%d va=%p value=0x%08x\n",
			     offset, (pWriteAddr->va + offset), value);
			break;
		}
	}
	mutex_unlock(&gCmdqWriteAddrMutex);

	return value;
}

uint32_t cmdqCoreWriteWriteAddress(dma_addr_t pa, uint32_t value)
{
	struct list_head *p = NULL;
	struct WriteAddrStruct *pWriteAddr = NULL;
	int32_t offset = 0;

	/* search for the entry */
	mutex_lock(&gCmdqWriteAddrMutex);
	list_for_each(p, &gCmdqContext.writeAddrList) {
		pWriteAddr = list_entry(p, struct WriteAddrStruct, list_node);
		if (NULL == pWriteAddr)
			continue;


		offset = pa - pWriteAddr->pa;

		/* note it is 64 bit length for uint32_t variable in 64 bit kernel */
		/* use sizeof(u_log) to check valid offset range */
		if (offset >= 0 && (offset / sizeof(unsigned long)) < pWriteAddr->count) {
			/*line over 120 characters,so devide to two statments */
			CMDQ_VERBOSE
			    ("cmdqCoreWriteWriteAddress() input:0x%pa, got offset=%d ",
			     &pa, offset);
			CMDQ_VERBOSE
			    ("va=%p pa_start=0x%pa, value=0x%08x\n",
			     (pWriteAddr->va + offset), &pWriteAddr->pa, value);
			*((volatile uint32_t *)(pWriteAddr->va + offset)) = value;
			break;
		}
	}
	mutex_unlock(&gCmdqWriteAddrMutex);
	return value;
}


int cmdqCoreFreeWriteAddress(dma_addr_t paStart)
{
	struct list_head *p, *n = NULL;
	struct WriteAddrStruct *pWriteAddr = NULL;
	bool foundEntry;

	foundEntry = false;
	/* search for the entry */
	mutex_lock(&gCmdqWriteAddrMutex);
	list_for_each_safe(p, n, &gCmdqContext.writeAddrList) {
		pWriteAddr = list_entry(p, struct WriteAddrStruct, list_node);
		if (pWriteAddr && pWriteAddr->pa == paStart) {
			list_del(&(pWriteAddr->list_node));
			foundEntry = true;
			break;
		}
	}
	mutex_unlock(&gCmdqWriteAddrMutex);

	if (false == foundEntry) {
		CMDQ_ERR("cmdqCoreFreeWriteAddress() no matching entry, paStart:%pa\n", &paStart);
		return -EINVAL;
	}
	/* release resources */
	if (pWriteAddr->va) {
		cmdq_core_free_hw_buffer(cmdq_dev_get(),
					 sizeof(uint32_t) * pWriteAddr->count,
					 pWriteAddr->va, pWriteAddr->pa);
		memset(pWriteAddr, 0xdeaddead, sizeof(struct WriteAddrStruct));
	}


	kfree(pWriteAddr);
	pWriteAddr = NULL;


	return 0;
}

int32_t cmdqCoreDebugRegDumpBegin(uint32_t taskID, uint32_t *regCount, uint32_t **regAddress)
{
	if (NULL == gCmdqDebugCallback.beginDebugRegDump) {
		CMDQ_ERR("beginDebugRegDump not registered\n");
		return -EFAULT;
	}

	return gCmdqDebugCallback.beginDebugRegDump(taskID, regCount, regAddress);
}

int32_t cmdqCoreDebugRegDumpEnd(uint32_t taskID, uint32_t regCount, uint32_t *regValues)
{
	if (NULL == gCmdqDebugCallback.endDebugRegDump) {
		CMDQ_ERR("endDebugRegDump not registered\n");
		return -EFAULT;
	}

	return gCmdqDebugCallback.endDebugRegDump(taskID, regCount, regValues);
}


struct ThreadStruct *cmdq_core_getThreadStruct(int threadID)
{
	if (threadID < 0 || threadID > CMDQ_MAX_TASK_IN_THREAD)
		return NULL;
	return &gCmdqContext.thread[threadID];
}

void cmdq_core_set_log_level(const enum LOG_LEVEL value)
{
#if 0				/*the caller should ensure value is ileagle not here */
	if (value < LOG_LEVEL_MSG || value >= LOG_LEVEL_MAX) {
		CMDQ_ERR("set log level error,out of range:%d\n", value);
		gCmdqContext.logLevel = LOG_LEVEL_LOG;
		return;
	}
#endif
	gCmdqContext.logLevel = value;
	CMDQ_DBG("set logLevel to %d\n", value);
}

enum LOG_LEVEL cmdq_core_get_log_level(void)
{
	return gCmdqContext.logLevel;
}

void cmdq_core_set_sec_print_count(uint32_t count)
{
	gSecPrintCount = count;
	CMDQ_DBG("set sec count to %d\n", count);
}

uint32_t cmdq_core_get_sec_print_count(void)
{
	return gSecPrintCount;
}

bool cmdq_core_should_print_msg(enum LOG_LEVEL log_level)
{
	return gCmdqContext.logLevel <= log_level;
}

bool cmdq_core_profile_enabled(void)
{
	return gCmdqContext.enableProfile > 0;
}

int32_t cmdq_core_enable_emergency_buffer_test(const bool enable)
{
#ifdef CMDQ_TEST_EMERGENCY_BUFFER
	const uint32_t value = (enable) ? (1) : (0);

	atomic_set(&gCmdqDebugForceUseEmergencyBuffer, value);
	return 0;
#else
	CMDQ_ERR("CMDQ_TEST_EMERGENCY_BUFFER not support\n");
	return -EFAULT;
#endif
}

void cmdqCoreLongString(bool forceLog, char *buf, uint32_t *offset, int32_t *maxSize,
			const char *string, ...)
{
	int msgLen;
	va_list argptr;
	char *pBuffer = buf + (*offset);

	if (false == forceLog)
		return;

	va_start(argptr, string);
	msgLen = vsnprintf(pBuffer, *maxSize, string, argptr);
	*maxSize -= msgLen;
	*offset += msgLen;
	va_end(argptr);

}

#ifdef CMDQ_OF_SUPPORT
void cmdq_core_get_clk_map(struct platform_device *pDevice)
{
	int i;

	for (i = 0; i < CMDQ_CLK_NUM; i++) {
		cmdq_clk_map[i] = devm_clk_get(&pDevice->dev, cmdq_core_get_clk_name(i));
		if (IS_ERR(cmdq_clk_map[i])) {
			CMDQ_ERR("Get Clock:%s(ID=%d) Fail!\n", cmdq_core_get_clk_name(i), i);
			BUG_ON(1);
		}
	}
/*
	sys_vde = devm_clk_get(&pDevice->dev, "sys_vde");
	if (IS_ERR(sys_vde)) {
		CMDQ_ERR("Get Clock:sys_vde Fail!\n");
		BUG_ON(1);
	}
	sys_isp = devm_clk_get(&pDevice->dev, "sys_isp");
	if (IS_ERR(sys_isp)) {
		CMDQ_ERR("Get Clock:sys_isp Fail!\n");
		BUG_ON(1);
	}
	sys_dis = devm_clk_get(&pDevice->dev, "sys_dis");
	if (IS_ERR(sys_dis)) {
		CMDQ_ERR("Get Clock:sys_dis Fail!\n");
		BUG_ON(1);
	}
*/
}

int cmdq_core_enable_ccf_clk(CMDQ_CLK_ENUM clk_enum)
{
	int ret = 0;

	if (IS_ERR(cmdq_clk_map[clk_enum])) {
		CMDQ_ERR("Enable CCF Clock:%s(ID=%d) Fail!\n", cmdq_core_get_clk_name(clk_enum),
			 clk_enum);
		ret = -1;
		BUG_ON(1);
	} else {
		ret += clk_prepare(cmdq_clk_map[clk_enum]);
		ret += clk_enable(cmdq_clk_map[clk_enum]);
		if (ret != 0)
			CMDQ_ERR("Prepare/Enable CCF Clock:%s(ID=%d) Fail!\n",
				 cmdq_core_get_clk_name(clk_enum), clk_enum);
	}

	return ret;
}

int cmdq_core_disable_ccf_clk(CMDQ_CLK_ENUM clk_enum)
{
	int ret = 0;

	if (IS_ERR(cmdq_clk_map[clk_enum])) {
		CMDQ_ERR("Disable CCF Clock:%s(ID=%d) Fail!\n", cmdq_core_get_clk_name(clk_enum),
			 clk_enum);
		ret = -1;
		BUG_ON(1);
	} else {
		clk_disable(cmdq_clk_map[clk_enum]);
		clk_unprepare(cmdq_clk_map[clk_enum]);
	}

	return ret;
}

/*
int cmdq_core_enable_mtcmos_clock(bool enable)
{
	int status = 0;
#ifdef CONFIG_PM_RUNTIME
	if (cmdq_dev_get() != NULL) {
		if (enable)
			status = pm_runtime_get_sync(cmdq_dev_get());
		else
			status = pm_runtime_put_sync(cmdq_dev_get());
	} else {
		CMDQ_ERR("MTCMOS fail, enable=%d cmdq_pdev=NULL\n", enable);
		status = -1;
	}
#endif
	if (status != 0)
		CMDQ_ERR("MTCMOS fail, enable=%d status=%d\n", enable, status);

	return status;
}
*/

bool cmdq_core_clock_is_on(CMDQ_CLK_ENUM clk_enum)
{
	if (__clk_get_enable_count(cmdq_clk_map[clk_enum]) > 0)
		return 1;
	else
		return 0;
}

/*
bool cmdq_core_subsys_is_on(CMDQ_SUBSYS_ENUM clk_enum)
{
	struct clk *subsys_clk = NULL;
	switch (clk_enum) {
	case CMDQ_SUBSYSCLK_SYS_VDE:
		subsys_clk = sys_vde;
		break;
	case CMDQ_SUBSYSCLK_SYS_ISP:
		subsys_clk = sys_isp;
		break;
	case CMDQ_SUBSYSCLK_SYS_DIS:
		subsys_clk = sys_dis;
		break;
	default:
		CMDQ_ERR("invalid subsys clk id=%d\n", clk_enum);
	}

	if (subsys_clk == NULL || IS_ERR(subsys_clk)) {
		CMDQ_ERR("subsys_clk uninitialized, id=%d\n", clk_enum);
		return 0;
	}

	if (__clk_get_enable_count(subsys_clk) > 0)
		return 1;
	else
		return 0;
}
*/
#endif

int32_t cmdq_core_alloc_sec_metadata(struct cmdqCommandStruct *pCommandDesc,
				     struct TaskStruct *pTask)
{
	int32_t status = 0;
	void *pNewBuf = NULL;

	if (pCommandDesc->secData.isSecure && pCommandDesc->secData.addrMetadataCount) {
		uint32_t size =
		    sizeof(cmdqSecAddrMetadataStruct) * pCommandDesc->secData.addrMetadataCount;
		pNewBuf = kzalloc(size, GFP_KERNEL);
		if (NULL == pNewBuf) {
			CMDQ_ERR("REC: kzalloc %d bytes addrMetadatas buffer failed\n", size);
			status = -1;
		}

		memcpy(pNewBuf, (void *)CMDQ_U32_PTR(pCommandDesc->secData.addrMetadatas), size);
	}

	pTask->secData.addrMetadatas = (cmdqU32Ptr_t) (unsigned long)pNewBuf;
	return status;
}

int32_t cmdq_core_release_sec_metadata(struct TaskStruct *pTask)
{
	if (pTask->secData.addrMetadatas) {
		kfree((void *)CMDQ_U32_PTR(pTask->secData.addrMetadatas));
		pTask->secData.addrMetadatas = (cmdqU32Ptr_t) (unsigned long)NULL;
		pTask->secData.addrMetadataMaxCount = 0;
		pTask->secData.addrMetadataCount = 0;
	}

	return 0;
}
