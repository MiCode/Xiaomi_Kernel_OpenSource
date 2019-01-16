#ifndef __CMDQ_CORE_H__
#define __CMDQ_CORE_H__

#include <linux/list.h>
#include <linux/time.h>
#include <linux/xlog.h>
#include <linux/aee.h>
#include <linux/device.h>
#include <linux/printk.h>
#include "cmdq_def.h"

#define CMDQ_LOG(string, args...) \
    if (1) \
    { \
	pr_err("[CMDQ]"string, ##args); \
    }

#define CMDQ_MSG(string, args...) \
    if (cmdq_core_should_print_msg()) \
    { \
	pr_warn("[CMDQ]"string, ##args); \
    }

#define CMDQ_VERBOSE(string, args...) \
    if (cmdq_core_should_print_msg()) \
    { \
	pr_info("[CMDQ]"string, ##args); \
    }

#define CMDQ_ERR(string, args...) \
    if (1) \
    { \
	pr_err("[CMDQ][ERR]"string, ##args); \
    }

#define CMDQ_AEE(tag, string, args...) \
    do { \
	char dispatchedTag[50];\
	snprintf(dispatchedTag, 50, "CRDISPATCH_KEY:%s", tag); \
	pr_err("[CMDQ][AEE]"string, ##args); \
	aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_DEFAULT | DB_OPT_PROC_CMDQ_INFO | DB_OPT_MMPROFILE_BUFFER, dispatchedTag, "error: "string, ##args); \
    } while (0)

#define CMDQ_PROFILE

typedef unsigned long long CMDQ_TIME;

#ifdef CMDQ_PROFILE
#define CMDQ_PROF_INIT() do {if (0 < cmdq_core_profile_enabled()) {met_tag_init(); } } while (0)
#define CMDQ_PROF_START(args...) do {if (0 < cmdq_core_profile_enabled()) {met_tag_start(args); } } while (0)
#define CMDQ_PROF_END(args...) do {if (0 < cmdq_core_profile_enabled()) {met_tag_end(args); } } while (0)
#define CMDQ_PROF_ONESHOT(args...) do {if (0 < cmdq_core_profile_enabled()) {met_tag_oneshot(args); } } while (0)
#define CMDQ_PROF_MMP(args...) do {if (1) {MMProfileLogEx(args); } } while (0)
#else
#define CMDQ_PROF_INIT()
#define CMDQ_PROF_START(args...)
#define CMDQ_PROF_END(args...)
#define CMDQ_PROF_ONESHOT(args...)
#define CMDQ_PROF_MMP(args...)
#endif

#define CMDQ_GET_TIME_IN_MS(start,                                                  \
			    end,                                                    \
			    duration)                                               \
{                                                                                   \
    CMDQ_TIME _duration = end - start;						    \
    do_div(_duration, 1000000);							    \
    duration = (int32_t)_duration;						    \
}

#define CMDQ_GET_TIME_IN_US_PART(start,                                             \
			    end,                                                    \
			    duration)                                               \
{                                                                                   \
    CMDQ_TIME _duration = end - start;						    \
    do_div(_duration, 1000);							    \
    duration = (int32_t)_duration;						    \
}

/*  */
/* address conversion for 4GB ram support: */
/* .address register: 32 bit */
/* .physical address: 32 bit, or 64 bit for CONFIG_ARCH_DMA_ADDR_T_64BIT enabled */
/*  */
/* when 33 bit enabled(4GB ram), 0x_0_xxxx_xxxx and 0x_1_xxxx_xxxx access is same for CPU */
/*  */
/*  */
/* 0x0            0x0_4000_0000        0x1_0000_0000        0x1_4000_0000 */
/* |---1GB HW addr---|------3GB DRAM------|----1GB DRAM(new)----|-----3GB DRAM(same)------| */
/* |                                               | */
/* |<--------------4GB RAM support---------------->| */
/*  */
#define CMDQ_PHYS_TO_AREG(addr) ((addr) & 0xFFFFFFFF)	/* truncate directly */
#define CMDQ_AREG_TO_PHYS(addr) ((addr) | 0L)

#define CMDQ_ENG_ISP_GROUP_BITS                 ((1LL << CMDQ_ENG_ISP_IMGI) |       \
						 (1LL << CMDQ_ENG_ISP_IMGO) |       \
						 (1LL << CMDQ_ENG_ISP_IMG2O))

#define CMDQ_ENG_MDP_GROUP_BITS                 ((1LL << CMDQ_ENG_MDP_CAMIN) |      \
						 (1LL << CMDQ_ENG_MDP_RDMA0) |      \
						 (1LL << CMDQ_ENG_MDP_RDMA1) |      \
						 (1LL << CMDQ_ENG_MDP_RSZ0) |       \
						 (1LL << CMDQ_ENG_MDP_RSZ1) |       \
						 (1LL << CMDQ_ENG_MDP_RSZ2) |       \
						 (1LL << CMDQ_ENG_MDP_TDSHP0) |     \
						 (1LL << CMDQ_ENG_MDP_TDSHP1) |     \
						 (1LL << CMDQ_ENG_MDP_WROT0) |      \
						 (1LL << CMDQ_ENG_MDP_WROT1) |      \
						 (1LL << CMDQ_ENG_MDP_WDMA))

#define CMDQ_ENG_DISP_GROUP_BITS                ((1LL << CMDQ_ENG_DISP_UFOE) |      \
						 (1LL << CMDQ_ENG_DISP_AAL) |       \
						 (1LL << CMDQ_ENG_DISP_COLOR0) |    \
						 (1LL << CMDQ_ENG_DISP_COLOR1) |    \
						 (1LL << CMDQ_ENG_DISP_RDMA0) |     \
						 (1LL << CMDQ_ENG_DISP_RDMA1) |     \
						 (1LL << CMDQ_ENG_DISP_RDMA2) |     \
						 (1LL << CMDQ_ENG_DISP_WDMA0) |     \
						 (1LL << CMDQ_ENG_DISP_WDMA1) |     \
						 (1LL << CMDQ_ENG_DISP_OVL0) |      \
						 (1LL << CMDQ_ENG_DISP_OVL1) |      \
						 (1LL << CMDQ_ENG_DISP_GAMMA) |     \
						 (1LL << CMDQ_ENG_DISP_MERGE) |     \
						 (1LL << CMDQ_ENG_DISP_SPLIT0) |    \
						 (1LL << CMDQ_ENG_DISP_SPLIT1) |    \
						 (1LL << CMDQ_ENG_DISP_DSI0_VDO) |  \
						 (1LL << CMDQ_ENG_DISP_DSI1_VDO) |  \
						 (1LL << CMDQ_ENG_DISP_DSI0_CMD) |  \
						 (1LL << CMDQ_ENG_DISP_DSI1_CMD) |  \
						 (1LL << CMDQ_ENG_DISP_DSI0) |      \
						 (1LL << CMDQ_ENG_DISP_DSI1) |      \
						 (1LL << CMDQ_ENG_DISP_DPI))

#define CMDQ_ENG_VENC_GROUP_BITS                ((1LL << CMDQ_ENG_VIDEO_ENC))

#define CMDQ_ENG_JPEG_GROUP_BITS                ((1LL << CMDQ_ENG_JPEG_ENC) | \
						 (1LL << CMDQ_ENG_JPEG_REMDC) | \
						 (1LL << CMDQ_ENG_JPEG_DEC))

#define CMDQ_ENG_ISP_GROUP_FLAG(flag)   ((flag) & (CMDQ_ENG_ISP_GROUP_BITS))

#define CMDQ_ENG_MDP_GROUP_FLAG(flag)   ((flag) & (CMDQ_ENG_MDP_GROUP_BITS))

#define CMDQ_ENG_DISP_GROUP_FLAG(flag)  ((flag) & (CMDQ_ENG_DISP_GROUP_BITS))

#define CMDQ_ENG_JPEG_GROUP_FLAG(flag)  ((flag) & (CMDQ_ENG_JPEG_GROUP_BITS))

#define CMDQ_ENG_VENC_GROUP_FLAG(flag)  ((flag) & (CMDQ_ENG_VENC_GROUP_BITS))

#define GENERATE_ENUM(_enum, _string) _enum,
#define GENERATE_STRING(_enum, _string) #_string,

#define CMDQ_FOREACH_GROUP(ACTION)\
    ACTION(CMDQ_GROUP_ISP, ISP) \
    ACTION(CMDQ_GROUP_MDP, MDP) \
    ACTION(CMDQ_GROUP_DISP, DISP) \
    ACTION(CMDQ_GROUP_JPEG, JPEG) \
    ACTION(CMDQ_GROUP_VENC, VENC)

typedef enum CMDQ_GROUP_ENUM {
	CMDQ_FOREACH_GROUP(GENERATE_ENUM)
	    CMDQ_MAX_GROUP_COUNT,	/* ALWAYS keep at the end */
} CMDQ_GROUP_ENUM;

/* engineFlag are bit fields defined in CMDQ_ENG_ENUM */
typedef int32_t(*CmdqClockOnCB) (uint64_t engineFlag);

/* engineFlag are bit fields defined in CMDQ_ENG_ENUM */
typedef int32_t(*CmdqDumpInfoCB) (uint64_t engineFlag, int level);

/* engineFlag are bit fields defined in CMDQ_ENG_ENUM */
typedef int32_t(*CmdqResetEngCB) (uint64_t engineFlag);

/* engineFlag are bit fields defined in CMDQ_ENG_ENUM */
typedef int32_t(*CmdqClockOffCB) (uint64_t engineFlag);

/* data are user data passed to APIs */
typedef int32_t(*CmdqInterruptCB) (unsigned long data);

/* data are user data passed to APIs */
typedef int32_t(*CmdqAsyncFlushCB) (unsigned long data);

/* TaskID is passed down from IOCTL */
/* client should fill "regCount" and "regAddress" */
/* the buffer pointed by (*regAddress) must be valid until */
/* CmdqDebugRegDumpEndCB() is called. */
typedef int32_t(*CmdqDebugRegDumpBeginCB) (uint32_t taskID, uint32_t *regCount,
					   uint32_t **regAddress);
typedef int32_t(*CmdqDebugRegDumpEndCB) (uint32_t taskID, uint32_t regCount, uint32_t *regValues);

typedef struct CmdqCBkStruct {
	CmdqClockOnCB clockOn;
	CmdqDumpInfoCB dumpInfo;
	CmdqResetEngCB resetEng;
	CmdqClockOffCB clockOff;
} CmdqCBkStruct;

typedef struct CmdqDebugCBkStruct {
	/* Debug Register Dump */
	CmdqDebugRegDumpBeginCB beginDebugRegDump;
	CmdqDebugRegDumpEndCB endDebugRegDump;
} CmdqDebugCBkStruct;

typedef enum CMDQ_CODE_ENUM {
	/* these are actual HW op code */
	CMDQ_CODE_READ = 0x01,
	CMDQ_CODE_MOVE = 0x02,
	CMDQ_CODE_WRITE = 0x04,
	CMDQ_CODE_POLL = 0x08,
	CMDQ_CODE_JUMP = 0x10,
	CMDQ_CODE_WFE = 0x20,	/* wait for event and clear */
	CMDQ_CODE_EOC = 0x40,	/* end of command */

	/* these are pseudo op code defined by SW */
	/* for instruction generation */
	CMDQ_CODE_SET_TOKEN = 0x21,	/* set event */
	CMDQ_CODE_WAIT_NO_CLEAR = 0x22,	/* wait event, but don't clear it */
	CMDQ_CODE_CLEAR_TOKEN = 0x23,	/* clear event */
	CMDQ_CODE_RAW = 0x24,	/* allow entirely custom argA/argB */
} CMDQ_CODE_ENUM;

typedef enum TASK_STATE_ENUM {
	TASK_STATE_IDLE,	/* free task */
	TASK_STATE_BUSY,	/* task running on a thread */
	TASK_STATE_KILLED,	/* task process being killed */
	TASK_STATE_ERROR,	/* task execution error */
	TASK_STATE_DONE,	/* task finished */
	TASK_STATE_WAITING,	/* allocated but waiting for available thread */
} TASK_STATE_ENUM;

typedef struct TaskStruct {
	struct list_head listEntry;

	/* For buffer state */
	TASK_STATE_ENUM taskState;	/* task life cycle */
	uint32_t *pVABase;	/* virtual address of command buffer */
	dma_addr_t MVABase;	/* physical address of command buffer */
	uint32_t bufferSize;	/* size of allocated command buffer */
	bool useEmergencyBuf;	/* is the command buffer emergency buffer? */

	/* For execution */
	int32_t scenario;
	int32_t priority;
	uint64_t engineFlag;
	int32_t commandSize;
	uint32_t *pCMDEnd;
	int32_t reorder;
	int32_t thread;		/* ASYNC: CMDQ_INVALID_THREAD if not running */
	int32_t irqFlag;	/* ASYNC: flag of IRQ received */
	CmdqInterruptCB loopCallback;	/* LOOP execution */
	unsigned long loopData;	/* LOOP execution */
	CmdqAsyncFlushCB flushCallback;	/* Callback on AsyncFlush (fire-and-forget) tasks */
	unsigned long flushData;	/* for callbacks & error handling */
	struct work_struct autoReleaseWork;	/* Work item when auto release is used */

	/* Output section for "read from reg to mem" */
	uint32_t regCount;
	uint32_t *regResults;
	dma_addr_t regResultsMVA;

	/* For register backup */
	uint32_t regCountUserSpace;	/* this is to separate backup request from user space and kernel space. */
	uint32_t regUserToken;	/* user data store for callback beginDebugRegDump / endDebugRegDump */

	/* For seucre execution */
	cmdqSecDataStruct secData;

	/* For statistics & debug */
	CMDQ_TIME submit;	/* ASYNC: task submit time (as soon as task acquired) */
	CMDQ_TIME trigger;
	CMDQ_TIME beginWait;
	CMDQ_TIME gotIRQ;
	CMDQ_TIME wakedUp;
	CMDQ_TIME entrySec; /* time stamp of entry secure world */
	CMDQ_TIME exitSec; /* time stamp of exit secure world */

	uint32_t *profileData;	/* store GPT counter when it starts and ends */
	dma_addr_t profileDataPA;

	void *privateData;	/* this is used to track associated file handle */

	pid_t callerPid;
	char callerName[TASK_COMM_LEN];

	/* Custom profile marker */
#ifdef CMDQ_PROFILE_MARKER_SUPPORT
	cmdqProfileMarkerStruct profileMarker;
#endif
} TaskStruct;

typedef struct EngineStruct {
	int32_t userCount;
	int32_t currOwner;
	int32_t resetCount;
	int32_t failCount;
} EngineStruct;

typedef struct ThreadStruct {
	uint32_t taskCount;
	uint32_t waitCookie;
	uint32_t nextCookie;
	CmdqInterruptCB loopCallback;	/* LOOP execution */
	unsigned long loopData;	/* LOOP execution */
	TaskStruct * pCurTask[CMDQ_MAX_TASK_IN_THREAD];

	/* 1 to describe thread is available to dispatch a task. 0: not available */
	/* .note thread's taskCount increase when attatch a task to it. */
	/* used it to prevent 2 tasks, which uses different engines, acquire same HW thread when dispatching happened before attaches task to thread */
	/* .note it is align task attachment, so use cmdqExecLock to ensure atomic access */
	uint32_t allowDispatching;
} ThreadStruct;

typedef struct RecordStruct {
	pid_t user;		/* calling SW thread tid */
	int32_t scenario;	/* task scenario */
	int32_t priority;	/* task priority (not thread priority) */
	int32_t thread;		/* allocated thread */
	int32_t reorder;
	int32_t size;
	uint32_t writeTimeNS;	/* if profile enabled, the time of command execution */
	uint64_t engineFlag;	/* task engine flag */

	bool isSecure; /* true for secure task */

	CMDQ_TIME submit;	/* epoch time of IOCTL/Kernel API call */
	CMDQ_TIME trigger;	/* epoch time of enable HW thread */
	CMDQ_TIME beginWait;	/* epoch time of start waiting for task completion */
	CMDQ_TIME gotIRQ;	/* epoch time of IRQ event */
	CMDQ_TIME wakedUp;	/* epoch time of SW thread leaving wait state */
	CMDQ_TIME done;	/* epoch time of task finish */

	unsigned long long writeTimeNSBegin;
	unsigned long long writeTimeNSEnd;

	/* Custom profile marker */
#ifdef CMDQ_PROFILE_MARKER_SUPPORT
	uint32_t profileMarkerCount;
	unsigned long long profileMarkerTimeNS[CMDQ_MAX_PROFILE_MARKER_IN_TASK];
	const char *profileMarkerTag[CMDQ_MAX_PROFILE_MARKER_IN_TASK];
#endif
} RecordStruct;

typedef struct ErrorStruct {
	RecordStruct errorRec;	/* the record of the error task */
	u64 ts_nsec;		/* kernel time of attach_error_task */
} ErrorStruct;

typedef struct WriteAddrStruct {
	struct list_head list_node;
	uint32_t count;
	void *va;
	dma_addr_t pa;
	pid_t user;
} WriteAddrStruct;

typedef struct EmergencyBufferStruct {
	bool used;
	uint32_t size;
	void *va;
	dma_addr_t pa;
} EmergencyBufferStruct;

/**
 * shared memory between normal and secure world
 */
typedef struct cmdqSecSharedMemoryStruct {
	void *pVABase;	/* virtual address of command buffer */
	dma_addr_t MVABase;	/* physical address of command buffer */
	uint32_t size; /* buffer size*/
} cmdqSecSharedMemoryStruct, *cmdqSecSharedMemoryHandle;

typedef struct ContextStruct {
	/* Task information */
	struct kmem_cache *taskCache;	/* TaskStruct object cache */
	struct list_head taskFreeList;	/* Unused free tasks */
	struct list_head taskActiveList;	/* Active tasks */
	struct list_head taskWaitList;	/* Tasks waiting for available thread */
	struct work_struct taskConsumeWaitQueueItem;
	struct workqueue_struct *taskAutoReleaseWQ;	/* auto-release workqueue */
	struct workqueue_struct *taskConsumeWQ;	/* task consumption workqueue (for queued tasks) */

	/* Write Address management */
	struct list_head writeAddrList;

	/* Basic information */
	EngineStruct engine[CMDQ_MAX_ENGINE_COUNT];
	ThreadStruct thread[CMDQ_MAX_THREAD_COUNT];

	/* Secure path shared information */
	cmdqSecSharedMemoryHandle hSecSharedMem;
	void* hNotifyLoop;

	/* Profile information */
	int32_t enableProfile;
	int32_t lastID;
	int32_t recNum;
	RecordStruct record[CMDQ_MAX_RECORD_COUNT];

	/* Error information */
	int32_t logLevel;
	int32_t errNum;
	ErrorStruct error[CMDQ_MAX_ERROR_COUNT];
} ContextStruct;


#ifdef __cplusplus
extern "C" {
#endif


	int32_t cmdqCoreInitialize(void);

	void cmdqCoreInitGroupCB(void);
	void cmdqCoreDeinitGroupCB(void);

	int32_t cmdqCoreRegisterCB(CMDQ_GROUP_ENUM engGroup,
				   CmdqClockOnCB clockOn,
				   CmdqDumpInfoCB dumpInfo,
				   CmdqResetEngCB resetEng, CmdqClockOffCB clockOff);

	int32_t cmdqCoreRegisterDebugRegDumpCB(CmdqDebugRegDumpBeginCB beginCB,
					       CmdqDebugRegDumpEndCB endCB);

	int32_t cmdqCoreSuspend(void);

	int32_t cmdqCoreResume(void);

	int32_t cmdqCoreResumedNotifier(void);

	void cmdqCoreHandleIRQ(int32_t index);

/**
 * Wait for completion of the given CmdQ task
 *
 * Parameter:
 *      scenario: The Scnerio enumeration
 *      priority: Desied task priority
 *      engineFlag: HW engine involved in the CMDs
 *      pCMDBlock: The command buffer
 *      blockSize: Size of the command buffer
 *      ppTaskOut: output pointer to a pTask for the resulting task
 *                 if fail, this gives NULL
 *      loopCB:    Assign this CB if your command loops itself.
 *                 since this disables thread completion handling, do not set this CB
 *                 if it is not intended to be a HW looping thread.
 *      loopData:  The user data passed to loopCB
 *
 * Return:
 *      >=0 for success; else the error code is returned
 */
	int32_t cmdqCoreSubmitTaskAsync(cmdqCommandStruct *pCommandDesc,
					CmdqInterruptCB loopCB,
					unsigned long loopData, TaskStruct **ppTaskOut);

/**
 * Wait for completion of the given CmdQ task
 *
 * Parameter:
 *      pTask: Task returned from successful cmdqCoreSubmitTaskAsync
 *             additional cleanup will be performed.
 *
 *      timeout: SW timeout error will be generated after this threshold
 * Return:
 *      >=0 for success; else the error code is returned
 */
	int32_t cmdqCoreWaitAndReleaseTask(TaskStruct *pTask, long timeout_jiffies);


/**
 * Wait for completion of the given CmdQ task, and retrieve
 * read register result.
 *
 * Parameter:
 *      pTask: Task returned from successful cmdqCoreSubmitTaskAsync
 *             additional cleanup will be performed.
 *
 *      timeout: SW timeout error will be generated after this threshold
 * Return:
 *      >=0 for success; else the error code is returned
 */
	int32_t cmdqCoreWaitResultAndReleaseTask(TaskStruct *pTask, cmdqRegValueStruct *pResult,
						 long timeout_jiffies);


/**
 * Stop task and release it immediately
 *
 * Parameter:
 *      pTask: Task returned from successful cmdqCoreSubmitTaskAsync
 *             additional cleanup will be performed.
 *
 * Return:
 *      >=0 for success; else the error code is returned
 */
	int32_t cmdqCoreReleaseTask(TaskStruct *pTask);

/**
 * Register the task in the auto-release queue. It will be released
 * upon finishing. You MUST NOT perform further operations on this task.
 *
 * Parameter:
 *      pTask: Task returned from successful cmdqCoreSubmitTaskAsync.
 *             additional cleanup will be performed.
 * Return:
 *      >=0 for success; else the error code is returned
 */
	int32_t cmdqCoreAutoReleaseTask(TaskStruct *pTask);

/**
 * Create CMDQ Task and block wait for its completion
 *
 * Return:
 *     >=0 for success; else the error code is returned
 */
	int32_t cmdqCoreSubmitTask(cmdqCommandStruct *pCommandDesc);


/**
 * Helper function checking validity of a task pointer
 *
 * Return:
 *     false if NOT a valid pointer
 *     true if valid
 */
	bool cmdqIsValidTaskPtr(void *pTask);

/**
 * Immediately clear CMDQ event to 0 with CPU
 *
 */

void cmdqCoreClearEvent(CMDQ_EVENT_ENUM event);

/**
 * Immediately set CMDQ event to 1 with CPU
 *
 */
	void cmdqCoreSetEvent(CMDQ_EVENT_ENUM event);

/**
 * Get event value with CPU. This is for debug purpose only
 * since it does not guarantee atomicity.
 *
 */
	uint32_t cmdqCoreGetEvent(CMDQ_EVENT_ENUM event);

	int cmdqCoreAllocWriteAddress(uint32_t count, dma_addr_t *paStart);
	int cmdqCoreFreeWriteAddress(dma_addr_t paStart);
	uint32_t cmdqCoreReadWriteAddress(dma_addr_t pa);
	uint32_t cmdqCoreWriteWriteAddress(dma_addr_t pa, uint32_t value);

	void cmdqCoreDeInitialize(void);

/**
 * Allocate/Free HW use buffer, e.g. command buffer forCMDQ HW
 */
	void *cmdq_core_alloc_hw_buffer(struct device *dev, size_t size, dma_addr_t *dma_handle, const gfp_t flag);
	void cmdq_core_free_hw_buffer(struct device *dev, size_t size, void *cpu_addr, dma_addr_t dma_handle);

/**
 * Set a given value to CMDQ secure HW thread's THR executed count to secShareMemory
 * Parameter:
 *    thread: [IN] CMDQ thread id
 *    cookie: [IN] the desired value, which should copy from CMDQ_THR_EXEC_CMDS_CNT of secure thread
 * Return:
 *     = 0 for successfully, < 0 for access failed
 */
	int32_t cmdq_core_set_secure_thread_exec_counter(const int32_t thread, const uint32_t cookie);

/**
 * Get CMDQ secure HW thread's executed count to secShareMemory
 * Parameter:
 *    thread: [IN] CMDQ thread id
 *    pCookie: [out] pointer to the cookie value
 * Return:
 *	   >= 0 for successfully, return value is HW thread's executed count
 *     < 0 for access failed
 */
	int32_t cmdq_core_get_secure_thread_exec_counter(const int32_t thread);

	cmdqSecSharedMemoryHandle cmdq_core_get_secure_shared_memory(void);

/**
 * Utilities
 */
	ssize_t cmdqCorePrintRecord(struct device *dev, struct device_attribute *attr, char *buf);
	ssize_t cmdqCorePrintError(struct device *dev, struct device_attribute *attr, char *buf);
	ssize_t cmdqCorePrintStatus(struct device *dev, struct device_attribute *attr, char *buf);

	ssize_t cmdqCorePrintLogLevel(struct device *dev, struct device_attribute *attr, char *buf);
	ssize_t cmdqCoreWriteLogLevel(struct device *dev,
				      struct device_attribute *attr, const char *buf, size_t size);

	ssize_t cmdqCorePrintProfileEnable(struct device *dev, struct device_attribute *attr,
					   char *buf);
	ssize_t cmdqCoreWriteProfileEnable(struct device *dev, struct device_attribute *attr,
					   const char *buf, size_t size);

	int32_t cmdqCoreDebugRegDumpBegin(uint32_t taskID, uint32_t *regCount,
					  uint32_t **regAddress);
	int32_t cmdqCoreDebugRegDumpEnd(uint32_t taskID, uint32_t regCount, uint32_t *regValues);
	int32_t cmdqCoreDebugDumpCommand(TaskStruct *pTask);
	int32_t cmdqCoreQueryUsage(int32_t *pCount);

	int cmdqCorePrintRecordSeq(struct seq_file *m, void *v);
	int cmdqCorePrintErrorSeq(struct seq_file *m, void *v);
	int cmdqCorePrintStatusSeq(struct seq_file *m, void *v);

	int32_t cmdq_subsys_from_phys_addr(uint32_t physAddr);
	unsigned long long cmdq_core_get_GPR64(const CMDQ_DATA_REGISTER_ENUM regID);
	void cmdq_core_set_GPR64(const CMDQ_DATA_REGISTER_ENUM regID, const unsigned long long value);
	uint32_t cmdqCoreReadDataRegister(CMDQ_DATA_REGISTER_ENUM regID);
	void cmdq_core_set_log_level(const int32_t value);
	bool cmdq_core_should_print_msg(void);
	int32_t cmdq_core_profile_enabled(void);

	int32_t cmdq_core_enable_emergency_buffer_test(const bool enable);

	int32_t cmdq_core_parse_instruction(const uint32_t *pCmd, char *textBuf, int bufLen);
	void cmdq_core_add_consume_task(void);

/* file_node is a pointer to cmdqFileNodeStruct that is */
/* created when opening the device file. */
	void cmdq_core_release_task_by_file_node(void *file_node);

/* dump GIC */
	void cmdq_core_dump_GIC(void);

#ifdef __cplusplus
}
#endif
#endif				/* __CMDQ_CORE_H__ */
