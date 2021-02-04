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

#ifndef __CMDQ_HELPER_EXT_H__
#define __CMDQ_HELPER_EXT_H__

#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/soc/mediatek/mtk-cmdq.h>
#include <linux/trace_events.h>

#include "cmdq_def.h"

#ifdef CMDQ_AEE_READY
#include <mt-plat/aee.h>
#endif

/* #define CMDQ_TIMER_ENABLE */

#define CMDQ_EVENT_ENUM cmdq_event

#define CMDQ_PHYS_TO_AREG(addr) ((addr) & 0xFFFFFFFF) /* truncate directly */
#define CMDQ_AREG_TO_PHYS(addr) ((addr) | 0L)

struct cmdqRecStruct;

enum TASK_STATE_ENUM {
	TASK_STATE_IDLE,	/* free task */
	TASK_STATE_BUSY,	/* task running on a thread */
	TASK_STATE_KILLED,	/* task process being killed */
	TASK_STATE_ERROR,	/* task execution error */
	TASK_STATE_ERR_IRQ,	/* task execution invalid instruction */
	TASK_STATE_DONE,	/* task finished */
	TASK_STATE_WAITING,	/* allocated but waiting for available thread */
	TASK_STATE_TIMEOUT,     /* task timeout */
};


#define CMDQ_LONGSTRING_MAX (180)
#define CMDQ_DELAY_RELEASE_RESOURCE_MS (1000)

#define CMDQ_THREAD_SEC_PRIMARY_DISP	(CMDQ_MIN_SECURE_THREAD_ID)

#if IS_ENABLED(CONFIG_MACH_MT6779)
#define CMDQ_THREAD_SEC_SUB_DISP	(CMDQ_MIN_SECURE_THREAD_ID + 1)
#define CMDQ_THREAD_SEC_MDP		(CMDQ_MIN_SECURE_THREAD_ID + 2)
#define CMDQ_THREAD_SEC_ISP		(CMDQ_MIN_SECURE_THREAD_ID + 3)
#else
/* for only 16 thread case mt6761 isp share second disp */
#define CMDQ_THREAD_SEC_SUB_DISP	(CMDQ_MIN_SECURE_THREAD_ID + 1)
#define CMDQ_THREAD_SEC_ISP		(CMDQ_MIN_SECURE_THREAD_ID + 1)
#define CMDQ_THREAD_SEC_MDP		(CMDQ_MIN_SECURE_THREAD_ID + 2)
#endif

/* max count of input */
#define CMDQ_MAX_COMMAND_SIZE		(0x80000000)
#define CMDQ_MAX_DUMP_REG_COUNT		(2048)
#define CMDQ_MAX_WRITE_ADDR_COUNT	(PAGE_SIZE / sizeof(u32))
#define CMDQ_MAX_DBG_STR_LEN		(1024)
#define CMDQ_MAX_USER_PROP_SIZE		(1024)

#ifdef CMDQ_DUMP_FIRSTERROR
#ifdef CMDQ_LARGE_MAX_FIRSTERROR_BUFFER
#define CMDQ_MAX_FIRSTERROR	(128*1024)
#else
#define CMDQ_MAX_FIRSTERROR	(32*1024)
#endif
struct DumpFirstErrorStruct {
	pid_t callerPid;
	char callerName[TASK_COMM_LEN];
	unsigned long long savetime;	/* epoch time of first error occur */
	char *cmdqString;
	u32 cmdqCount;
	s32 cmdqMaxSize;
	bool flag;
	struct timeval savetv;
};
#endif

#define CMDQ_LOG(string, args...) \
do {			\
	pr_notice("[CMDQ]"string, ##args); \
	cmdq_core_save_first_dump("[CMDQ]"string, ##args); \
} while (0)

#define CMDQ_MSG(string, args...) \
do {			\
if (cmdq_core_should_print_msg()) { \
	pr_notice("[CMDQ]"string, ##args); \
}			\
} while (0)

#define CMDQ_VERBOSE(string, args...) \
do {			\
if (cmdq_core_should_print_msg()) { \
	pr_debug("[CMDQ]"string, ##args); \
}			\
} while (0)


#define CMDQ_ERR(string, args...) \
do {			\
	pr_notice("[CMDQ][ERR]"string, ##args); \
	cmdq_core_save_first_dump("[CMDQ][ERR]"string, ##args); \
} while (0)

#define CMDQ_CHECK_AND_BREAK_STATUS(status)\
{					\
if (status < 0)		\
	break;			\
}

#ifdef CMDQ_AEE_READY
#define CMDQ_AEE_EX(DB_OPTs, tag, string, args...) \
{		\
do {			\
	char dispatchedTag[50]; \
	snprintf(dispatchedTag, 50, "CRDISPATCH_KEY:%s", tag); \
	pr_notice("[CMDQ][AEE]"string, ##args); \
	cmdq_core_save_first_dump("[CMDQ][AEE]"string, ##args); \
	cmdq_core_turnoff_first_dump(); \
	aee_kernel_warning_api(__FILE__, __LINE__, \
		DB_OPT_DEFAULT | DB_OPT_PROC_CMDQ_INFO | \
		DB_OPT_MMPROFILE_BUFFER | DB_OPT_FTRACE | DB_OPTs, \
		dispatchedTag, "error: "string, ##args); \
} while (0);	\
}

#define CMDQ_AEE(tag, string, args...) \
do { \
	if (cmdq_core_aee_enable()) \
		CMDQ_AEE_EX(DB_OPT_DUMP_DISPLAY, tag, string, ##args) \
} while (0)

#else
#define CMDQ_AEE(tag, string, args...) \
{		\
do {			\
	char dispatchedTag[50]; \
	snprintf(dispatchedTag, 50, "CRDISPATCH_KEY:%s", tag); \
	pr_debug("[CMDQ][AEE] AEE not READY!!!"); \
	pr_debug("[CMDQ][AEE]"string, ##args); \
	cmdq_core_save_first_dump("[CMDQ][AEE]"string, ##args); \
	cmdq_core_turnoff_first_dump(); \
} while (0);	\
}
#endif

/*#define CMDQ_PROFILE*/

/* typedef unsigned long long CMDQ_TIME; */
#define CMDQ_TIME unsigned long long

#ifdef CMDQ_PROFILE
#define CMDQ_PROF_INIT()	\
{		\
do {if (cmdq_core_met_enabled()) met_tag_init(); } while (0);	\
}

#define CMDQ_PROF_START(args...)	\
{		\
do {if (cmdq_core_met_enabled()) met_tag_start(args);	\
	} while (0);	\
}

#define CMDQ_PROF_END(args...)	\
{		\
do {if (cmdq_core_met_enabled()) met_tag_end(args);	\
	} while (0);	\
}
#define CMDQ_PROF_ONESHOT(args...)	\
{		\
do {if (cmdq_core_met_enabled()) met_tag_oneshot(args);	\
	} while (0);	\
}
#else
#define CMDQ_PROF_INIT()
#define CMDQ_PROF_START(args...)
#define CMDQ_PROF_END(args...)
#define CMDQ_PROF_ONESHOT(args...)
#endif

#ifdef CMDQ_PROFILE_MMP
#define CMDQ_PROF_MMP(args...)\
{\
do {if (1) mmprofile_log_ex(args); } while (0);	\
}
#else
#define CMDQ_PROF_MMP(args...)
#endif

/* CMDQ FTRACE */
#define CMDQ_TRACE_FORCE_BEGIN(fmt, args...) do { \
	preempt_disable(); \
	event_trace_printk(cmdq_get_tracing_mark(), \
		"B|%d|"fmt, current->tgid, ##args); \
	preempt_enable();\
} while (0)

#define CMDQ_TRACE_FORCE_END() do { \
	preempt_disable(); \
	event_trace_printk(cmdq_get_tracing_mark(), "E\n"); \
	preempt_enable(); \
} while (0)


#define CMDQ_SYSTRACE_BEGIN(fmt, args...) do { \
	if (cmdq_core_ftrace_enabled()) { \
		CMDQ_TRACE_FORCE_BEGIN(fmt, ##args); \
	} \
} while (0)

#define CMDQ_SYSTRACE_END() do { \
	if (cmdq_core_ftrace_enabled()) { \
		CMDQ_TRACE_FORCE_END(); \
	} \
} while (0)

#define CMDQ_GET_TIME_IN_MS(start, end, duration)	\
{	\
CMDQ_TIME _duration = end - start;	\
do_div(_duration, 1000000);	\
duration = (s32)_duration;	\
}

#define CMDQ_GET_TIME_IN_US_PART(start, end, duration)	\
{	\
CMDQ_TIME _duration = end - start;	\
do_div(_duration, 1000);	\
duration = (s32)_duration;	\
}

#define CMDQ_INC_TIME_IN_US(start, end, target)	\
{	\
CMDQ_TIME _duration = end - start;	\
do_div(_duration, 1000);	\
target += (s32)_duration;	\
}

#define GENERATE_ENUM(_enum, _string) _enum,
#define GENERATE_STRING(_enum, _string) (#_string),

#define CMDQ_TASK_PRIVATE(task) \
	((struct task_private *)task->privateData)
#define CMDQ_TASK_IS_INTERNAL(task) \
	(task->privateData && (CMDQ_TASK_PRIVATE(task)->internal))

/* engineFlag are bit fields defined in CMDQ_ENG_ENUM */
typedef s32(*CmdqClockOnCB) (u64 engineFlag);

/* engineFlag are bit fields defined in CMDQ_ENG_ENUM */
typedef s32(*CmdqDumpInfoCB) (u64 engineFlag, int level);

/* engineFlag are bit fields defined in CMDQ_ENG_ENUM */
typedef s32(*CmdqResetEngCB) (u64 engineFlag);

/* engineFlag are bit fields defined in CMDQ_ENG_ENUM */
typedef s32(*CmdqClockOffCB) (u64 engineFlag);

/* data are user data passed to APIs */
typedef s32(*CmdqInterruptCB) (unsigned long data);

/* data are user data passed to APIs */
typedef s32(*CmdqAsyncFlushCB) (unsigned long data);

/* resource event can be indicated to resource unit */
typedef s32(*CmdqResourceReleaseCB) (enum cmdq_event resourceEvent);

/* resource event can be indicated to resource unit */
typedef s32(*CmdqResourceAvailableCB) (
	enum cmdq_event resourceEvent);

/* PMQOS */
/* task begin for pmqos */
typedef void(*CmdqBeginTaskCB) (struct cmdqRecStruct *handle,
	struct cmdqRecStruct **handle_list, u32 size);

/* task end for pmqos */
typedef void(*CmdqEndTaskCB) (struct cmdqRecStruct *handle,
	struct cmdqRecStruct **handle_list, u32 size);

/* TaskID is passed down from IOCTL */
/* client should fill "regCount" and "regAddress" */
/* the buffer pointed by (*regAddress) must be valid until */
/* CmdqDebugRegDumpEndCB() is called. */
typedef s32(*CmdqDebugRegDumpBeginCB) (u32 taskID, u32 *regCount,
	u32 **regAddress);
typedef s32(*CmdqDebugRegDumpEndCB) (u32 taskID, u32 regCount,
	u32 *regValues);

/* dispatch module can be change by callback */
typedef const char*(*CmdqDispatchModuleCB) (u64 engineFlag);

struct NGTaskInfoStruct;

/* finished task can be get by callback */
typedef void(*CmdqTrackTaskCB) (const struct cmdqRecStruct *pTask);

/* finished task can be get by callback */
typedef void(*CmdqErrorResetCB) (u64 engineFlag);

struct CmdqCBkStruct {
	CmdqClockOnCB clockOn;
	CmdqDumpInfoCB dumpInfo;
	CmdqResetEngCB resetEng;
	CmdqClockOffCB clockOff;
	CmdqDispatchModuleCB dispatchMod;
	CmdqTrackTaskCB trackTask;
	CmdqErrorResetCB errorReset;
	CmdqBeginTaskCB beginTask;
	CmdqEndTaskCB endTask;
};

struct CmdqDebugCBkStruct {
	/* Debug Register Dump */
	CmdqDebugRegDumpBeginCB beginDebugRegDump;
	CmdqDebugRegDumpEndCB endDebugRegDump;
};

enum CMDQ_CLT_ENUM {
	CMDQ_CLT_UNKN,
	CMDQ_CLT_MDP,
	CMDQ_CLT_CMDQ,
	CMDQ_CLT_GNRL,
	CMDQ_CLT_DISP,
	CMDQ_CLT_MAX	/* ALWAYS keep at the end */
};

/* sync with request in atf */
enum cmdq_smc_request {
	CMDQ_ENABLE_DEBUG,
};

/* handle to gce life cycle callback */
typedef void (*cmdq_core_handle_cb)(struct cmdqRecStruct *handle);

#define subsys_lsb_bit (16)

enum CMDQ_LOG_LEVEL_ENUM {
	CMDQ_LOG_LEVEL_NORMAL = 0,
	CMDQ_LOG_LEVEL_MSG = 1,
	CMDQ_LOG_LEVEL_FULL_ERROR = 2,
	CMDQ_LOG_LEVEL_EXTENSION = 3,
	CMDQ_LOG_LEVEL_PMQOS = 4,
	CMDQ_LOG_LEVEL_SECURE = 5,

	CMDQ_LOG_LEVEL_MAX	/* ALWAYS keep at the end */
};

enum CMDQ_PROFILE_LEVEL {
	CMDQ_PROFILE_OFF = 0,
	CMDQ_PROFILE_MET = 1,
	CMDQ_PROFILE_FTRACE = 2,

	CMDQ_PROFILE_MAX	/* ALWAYS keep at the end */
};


#define CMDQ_FEATURE_OFF_VALUE (0)
#define FOREACH_FEATURE(FEATURE) \
FEATURE(CMDQ_FEATURE_SRAM_SHARE, "SRAM Share") \

enum CMDQ_FEATURE_TYPE_ENUM {
	FOREACH_FEATURE(GENERATE_ENUM)
	CMDQ_FEATURE_TYPE_MAX,	 /* ALWAYS keep at the end */
};

#ifdef CMDQ_INSTRUCTION_COUNT
/* GCE instructions count information */
enum CMDQ_STAT_ENUM {
	CMDQ_STAT_WRITE = 0,
	CMDQ_STAT_WRITE_W_MASK = 1,
	CMDQ_STAT_READ = 2,
	CMDQ_STAT_POLLING = 3,
	CMDQ_STAT_MOVE = 4,
	CMDQ_STAT_SYNC = 5,
	CMDQ_STAT_PREFETCH_EN = 6,
	CMDQ_STAT_PREFETCH_DIS = 7,
	CMDQ_STAT_EOC = 8,
	CMDQ_STAT_JUMP = 9,

	CMDQ_STAT_MAX		/* ALWAYS keep at the end */
};

enum CMDQ_MODULE_STAT_ENUM {
	CMDQ_MODULE_STAT_MMSYS_CONFIG = 0,
	CMDQ_MODULE_STAT_MDP_RDMA = 1,
	CMDQ_MODULE_STAT_MDP_RSZ0 = 2,
	CMDQ_MODULE_STAT_MDP_RSZ1 = 3,
	CMDQ_MODULE_STAT_MDP_WDMA = 4,
	CMDQ_MODULE_STAT_MDP_WROT = 5,
	CMDQ_MODULE_STAT_MDP_TDSHP = 6,
	CMDQ_MODULE_STAT_MM_MUTEX = 7,
	CMDQ_MODULE_STAT_VENC = 8,
	CMDQ_MODULE_STAT_DISP_OVL0 = 9,
	CMDQ_MODULE_STAT_DISP_OVL1 = 10,
	CMDQ_MODULE_STAT_DISP_RDMA0 = 11,
	CMDQ_MODULE_STAT_DISP_RDMA1 = 12,
	CMDQ_MODULE_STAT_DISP_WDMA0 = 13,
	CMDQ_MODULE_STAT_DISP_COLOR = 14,
	CMDQ_MODULE_STAT_DISP_CCORR = 15,
	CMDQ_MODULE_STAT_DISP_AAL = 16,
	CMDQ_MODULE_STAT_DISP_GAMMA = 17,
	CMDQ_MODULE_STAT_DISP_DITHER = 18,
	CMDQ_MODULE_STAT_DISP_UFOE = 19,
	CMDQ_MODULE_STAT_DISP_PWM = 20,
	CMDQ_MODULE_STAT_DISP_WDMA1 = 21,
	CMDQ_MODULE_STAT_DISP_MUTEX = 22,
	CMDQ_MODULE_STAT_DISP_DSI0 = 23,
	CMDQ_MODULE_STAT_DISP_DPI0 = 24,
	CMDQ_MODULE_STAT_DISP_OD = 25,
	CMDQ_MODULE_STAT_CAM0 = 26,
	CMDQ_MODULE_STAT_CAM1 = 27,
	CMDQ_MODULE_STAT_CAM2 = 28,
	CMDQ_MODULE_STAT_CAM3 = 29,
	CMDQ_MODULE_STAT_SODI = 30,
	CMDQ_MODULE_STAT_GPR = 31,
	CMDQ_MODULE_STAT_OTHERS = 32,

	CMDQ_MODULE_STAT_MAX	/* ALWAYS keep at the end */
};

enum CMDQ_EVENT_STAT_ENUM {
	CMDQ_EVENT_STAT_HW = 0,
	CMDQ_EVENT_STAT_SW = 1,

	CMDQ_EVENT_STAT_MAX	/* ALWAYS keep at the end */
};

#define CMDQ_MAX_OTHERINSTRUCTION_MAX		(16)

struct CmdqModulePAStatStruct {
	long start[CMDQ_MODULE_STAT_MAX];
};
#endif

/* record NG handle info for dump */
struct cmdq_ng_handle_info {
	const struct cmdqRecStruct *nghandle;
	u64 engine_flag;
	u32 scenario;
	u32 *va_start;	/* original buffer va start */
	u32 *va_pc;	/* hw pc for original buffer */
	u32 *buffer;
	u32 buffer_size;
	u32 dump_size;
	const char *module;
	s32 irq_flag;
	u32 inst[2];
};

struct EngineStruct {
	s32 userCount;
	s32 currOwner;
	s32 resetCount;
	s32 failCount;
};

struct ThreadStruct {
	u32 taskCount;
	u32 waitCookie;
	u32 nextCookie;
	/* keep used engine to look up while dispatch thread */
	u64 engineFlag;
	CmdqInterruptCB loopCallback;	/* LOOP execution */
	unsigned long loopData;	/* LOOP execution */
	//struct TaskStruct *pCurTask[CMDQ_MAX_TASK_IN_THREAD];

	/* 1 to describe thread is available to dispatch a task.
	 * 0: not available
	 * .note thread's taskCount increase when attatch a task to it.
	 * used it to prevent 2 tasks, which uses different engines,
	 * acquire same HW thread when dispatching happened before
	 * attaches task to thread
	 * .note it is align task attachment, so use cmdqExecLock
	 * to ensure atomic access
	 */
	u32 allowDispatching;
};

struct cmdq_core_thread {
	bool used;	/* true if thread static allocated */
	u32 acquire;	/* acquired ref count */
	s32 scenario;
	u32 handle_count;
};

struct RecordStruct {
	pid_t user;	/* calling SW thread tid */
	s32 scenario;	/* task scenario */
	s32 priority;	/* task priority (not thread priority) */
	s32 thread;	/* allocated thread */
	s32 reorder;
	s32 size;
	u64 engineFlag;	/* task engine flag */

	bool is_secure;		/* true for secure task */

	CMDQ_TIME submit;	/* epoch time of IOCTL/Kernel API call */
	CMDQ_TIME trigger;	/* epoch time of enable HW thread */
	/* epoch time of start waiting for task completion */
	CMDQ_TIME beginWait;
	CMDQ_TIME gotIRQ;	/* epoch time of IRQ event */
	CMDQ_TIME wakedUp;	/* epoch time of SW thread leaving wait state */
	CMDQ_TIME done;		/* epoch time of task finish */

	u32 durAlloc;	/* allocae time duration */
	u32 durReclaim;	/* allocae time duration */
	u32 durRelease;	/* release time duration */

	u32 start;	/* buffer start address */
	u32 end;	/* command end address */
	u32 jump;	/* last jump destination */

	/* Custom profile marker */
	u32 profileMarkerCount;
	unsigned long long profileMarkerTimeNS[CMDQ_MAX_PROFILE_MARKER_IN_TASK];
	const char *profileMarkerTag[CMDQ_MAX_PROFILE_MARKER_IN_TASK];

	/* GCE instructions count information */
#ifdef CMDQ_INSTRUCTION_COUNT
	unsigned short instructionStat[CMDQ_STAT_MAX];
	unsigned short writeModule[CMDQ_MODULE_STAT_MAX];
	unsigned short writewmaskModule[CMDQ_MODULE_STAT_MAX];
	unsigned short readModlule[CMDQ_MODULE_STAT_MAX];
	unsigned short pollModule[CMDQ_MODULE_STAT_MAX];
	unsigned short eventCount[CMDQ_EVENT_STAT_MAX];
	u32 otherInstr[CMDQ_MAX_OTHERINSTRUCTION_MAX];
	u32 otherInstrNUM;
#endif
};

struct ErrorStruct {
	struct RecordStruct errorRec;	/* the record of the error task */
	u64 ts_nsec;		/* kernel time of attach_error_task */
};

struct WriteAddrStruct {
	struct list_head list_node;
	u32 count;
	void *va;
	dma_addr_t pa;
	pid_t user;
};

/* resource unit between each module */
struct ResourceUnitStruct {
	struct list_head list_entry;
	CMDQ_TIME notify;	/* notify time from module prepare */
	CMDQ_TIME lock;		/* lock time from module lock */
	CMDQ_TIME unlock;	/* unlock time from module unlock*/
	CMDQ_TIME delay;	/* delay start time from module release*/
	CMDQ_TIME acquire;	/* acquire time from module acquire */
	CMDQ_TIME release;	/* release time from module release */
	bool used;	/* indicate resource is in use by owner or not */
	bool lend;	/* indicate resource is lend by client or not */
	bool delaying;	/* indicate resource is in delay check or not */
	enum cmdq_event lockEvent;	/* SW token to lock in GCE thread */
	u32 engine_id;			/* which engine is resource */
	u64 engine_flag;		/* engine flag */
	CmdqResourceAvailableCB availableCB;
	CmdqResourceReleaseCB releaseCB;
	/* Delay Work item when delay check is used */
	struct delayed_work delayCheckWork;
};

/* SRAM chunk structure
 *	allocated_start: allocated start address
 *	allocated_size: allocated SRAM size
 *	allocated_owner: allocate owner name
 */
struct SRAMChunk {
	struct list_head list_node;
	u32 start_offset;
	size_t count;
	char owner[CMDQ_MAX_SRAM_OWNER_NAME];
};

/**
 * shared memory between normal and secure world
 */
struct cmdqSecSharedMemoryStruct {
	void *pVABase;		/* virtual address of command buffer */
	dma_addr_t MVABase;	/* physical address of command buffer */
	uint32_t size;		/* buffer size */
};

struct ContextStruct {
	/* handle information */
	struct list_head handle_active;

	/* Write Address management */
	struct list_head writeAddrList;
	atomic_t write_addr_cnt;

	/* Basic information */
	struct cmdq_core_thread thread[CMDQ_MAX_THREAD_COUNT];

	/* auto-release workqueue per thread */
	struct workqueue_struct **taskThreadAutoReleaseWQ;

	/* Secure path shared information */
	struct cmdqSecSharedMemoryStruct *hSecSharedMem;

	/* Profile information */
	s32 enableProfile;
	s32 lastID;
	s32 recNum;
	struct RecordStruct record[CMDQ_MAX_RECORD_COUNT];

	/* Error information */
	s32 logLevel;
	s32 errNum;
	struct ErrorStruct error[CMDQ_MAX_ERROR_COUNT];
	bool aee;

	/* SRAM manager information */
	struct list_head sram_allocated_list;	/* all allocated SRAM chunk */
	size_t allocated_sram_count;

	/* Delay set CPR start information */
	u32 delay_cpr_start;

	void *inst_check_buffer;

#ifdef CMDQ_INSTRUCTION_COUNT
	/* GCE instructions count information */
	s32 instructionCountLevel;
#endif
};

/* Command dump information */
struct DumpCommandBufferStruct {
	u64 scenario;
	u32 bufferSize;
	u32 count;
	char *cmdqString;
};

/* TODO: add stress support */
#if 0
typedef void (*cmdqStressCallback)(struct TaskStruct *task, s32 thread);

struct StressContextStruct {
	cmdqStressCallback exec_suspend;
};
#endif

struct cmdq_event_table {
	u16 event;	/* cmdq event enum value */
	const char *event_name;
	const char *dts_name;
};

struct cmdq_subsys_dts_name {
	const char *name;
	const char *group;
};

struct task_private {
	void *node_private_data;
	bool internal;		/* internal used only task */
	bool ignore_timeout;	/* timeout is expected */
};

enum cmdq_thread_dispatch {
	CMDQ_THREAD_NOTSET = 0,
	CMDQ_THREAD_STATIC,
	CMDQ_THREAD_ACQUIRE,
	CMDQ_THREAD_DYNAMIC,
};

enum CMDQ_SPM_MODE {
	CMDQ_CG_MODE,
	CMDQ_PD_MODE,
};

/* secure world wsm metadata type in message ex,
 * must sync with cmdq_sec_meta_type in cmdq_sec_iwc_common.h
 */
enum cmdq_sec_rec_meta_type {
	CMDQ_SEC_METAEX_NONE,
	CMDQ_SEC_METAEX_FD,
	CMDQ_SEC_METAEX_CQ,
};

struct cmdqRecStruct {
	struct list_head list_entry;
	struct cmdq_pkt *pkt;
	u32 *cmd_end;
	u64 engineFlag;
	s32 scenario;
	/* running task after start loop */
	void *running_task;
	bool jump_replace;	/* jump replace or not */
	bool finalized;		/* set to true after flush() or startLoop() */
	bool force_inorder;
	CmdqInterruptCB loop_cb;
	unsigned long loop_user_data;
	CmdqAsyncFlushCB async_callback;
	u64 async_user_data;
	u32 prefetchCount;	/* maintenance prefetch instruction */
	bool use_sram_buffer;	/* use SRAM or not */
	const char *sram_owner_name;
	u32 sram_base;	/* Original PA address of SRAM buffer content */
	void *node_private;
	void *user_private;

	struct cmdqSecDataStruct secData;	/* secure execution data */

	/* for CPR conditional and variable use */
	struct cmdq_v3_replace_struct replace_instr;
	u8 local_var_num;
	struct cmdq_stack_node *if_stack_node;
	struct cmdq_stack_node *while_stack_node;
	CMDQ_VARIABLE arg_source;	/* poll source, wait_timeout event */
	CMDQ_VARIABLE arg_value;	/* poll value, wait_timeout start */
	CMDQ_VARIABLE arg_timeout;	/* wait_timeout timeout */

	/* task executing data */
	atomic_t exec;
	enum TASK_STATE_ENUM state;	/* task life cycle */
	s32 thread;
	enum cmdq_thread_dispatch thd_dispatch;
	/* work item when auto release is used */
	struct work_struct auto_release_work;

	/* register backup at end of task */
	u32 reg_count;
	u32 *reg_values;
	dma_addr_t reg_values_pa;

	/* user space data */
	u32 user_reg_count;
	u32 user_token;
	pid_t caller_pid;
	char caller_name[TASK_COMM_LEN];
	char *user_debug_str;

	/* profile marker */
	struct cmdqProfileMarkerStruct profileMarker;

	/* flag to enable/disable clock */
	u64 engine_clk;
	u64 res_flag_acquire;
	u64 res_flag_release;

	/* controller interface */
	const struct cmdq_controller *ctrl;
	cmdq_core_handle_cb prepare;
	cmdq_core_handle_cb unprepare;
	cmdq_core_handle_cb stop;

	struct cmdq_timeout_info *timeout_info;

	/* debug information */
	u32 error_irq_pc;
	bool dumpAllocTime;	/* flag to print static info to kernel log. */
	bool profile_exec;
	s32 reorder;
	CMDQ_TIME submit;
	CMDQ_TIME trigger;
	CMDQ_TIME beginWait;
	CMDQ_TIME gotIRQ;
	CMDQ_TIME wakedUp;
	CMDQ_TIME entrySec;	/* time stamp of entry secure world */
	CMDQ_TIME exitSec;	/* time stamp of exit secure world */
	u32 durAlloc;	/* allocae time duration */
	u32 durReclaim;	/* allocae time duration */
	u32 durRelease;	/* release time duration */

	/* PMQoS information */
	void *prop_addr;
	u32 prop_size;

	/* secure world */
	struct iwcCmdqSecStatus_t *secStatus;
	u32 irq;
	void *sec_client_meta;
	enum cmdq_sec_rec_meta_type sec_meta_type;
	u32 sec_meta_size;

	u32 check_list_del;
};

/* TODO: add controller support */
struct cmdq_controller {
#if 0
	s32 (*compose)(struct cmdqCommandStruct *desc,
		struct TaskStruct *task);
	s32 (*copy_command)(struct cmdqCommandStruct *desc,
		struct TaskStruct *task);
#endif
	s32 (*get_thread_id)(s32 scenario);
	s32 (*handle_wait_result)(struct cmdqRecStruct *handle, s32 thread);
#if 0
	s32 (*execute_prepare)(struct TaskStruct *task, s32 thread);
	s32 (*execute)(struct TaskStruct *task, s32 thread);
	s32 (*handle_wait_result)(struct TaskStruct *task, s32 thread,
		s32 wait_ret);
	void (*free_buffer)(struct TaskStruct *task);
	void (*append_command)(struct TaskStruct *task, u32 arg_a, u32 arg_b);
	void (*dump_err_buffer)(const struct TaskStruct *task, u32 *hwpc);
	void (*dump_summary)(const struct TaskStruct *task, s32 thread,
		const struct TaskStruct **ngtask_out,
		struct NGTaskInfoStruct **nginfo_out);
#endif
	bool change_jump;
};

/* subsys and event mapping */

struct cmdq_subsys_dts_name *cmdq_subsys_get_dts(void);
u32 cmdq_subsys_get_size(void);
struct cmdq_event_table *cmdq_event_get_table(void);
u32 cmdq_event_get_table_size(void);

/* CMDQ core feature functions */

bool cmdq_core_check_user_valid(void *src, u32 size);

void cmdq_core_deinit_group_cb(void);

s32 cmdqCoreRegisterCB(enum CMDQ_GROUP_ENUM engGroup,
	CmdqClockOnCB clockOn,
	CmdqDumpInfoCB dumpInfo,
	CmdqResetEngCB resetEng, CmdqClockOffCB clockOff);

s32 cmdqCoreRegisterDispatchModCB(
	enum CMDQ_GROUP_ENUM engGroup,
	CmdqDispatchModuleCB dispatchMod);

s32 cmdqCoreRegisterDebugRegDumpCB(
	CmdqDebugRegDumpBeginCB beginCB,
	CmdqDebugRegDumpEndCB endCB);

s32 cmdqCoreRegisterTrackTaskCB(enum CMDQ_GROUP_ENUM engGroup,
	CmdqTrackTaskCB trackTask);

s32 cmdqCoreRegisterErrorResetCB(enum CMDQ_GROUP_ENUM engGroup,
	CmdqErrorResetCB errorReset);

void cmdq_core_register_status_dump(struct notifier_block *notifier);
void cmdq_core_remove_status_dump(struct notifier_block *notifier);

/* PMQoS register function */
s32 cmdq_core_register_task_cycle_cb(enum CMDQ_GROUP_ENUM group,
	CmdqBeginTaskCB beginTask, CmdqEndTaskCB endTask);

const char *cmdq_core_parse_op(u32 op_code);
s32 cmdq_core_interpret_instruction(char *textBuf, s32 bufLen,
	const u32 op, const u32 arg_a, const u32 arg_b);
s32 cmdq_core_parse_instruction(const u32 *pCmd, char *textBuf, int bufLen);

bool cmdq_core_should_print_msg(void);
bool cmdq_core_should_full_error(void);
bool cmdq_core_should_pmqos_log(void);
bool cmdq_core_should_secure_log(void);
bool cmdq_core_aee_enable(void);
void cmdq_core_set_aee(bool enable);

bool cmdq_core_ftrace_enabled(void);
void cmdq_long_string_init(bool force, char *buf, u32 *offset, s32 *max_size);
void cmdq_long_string(char *buf, u32 *offset, s32 *max_size,
	const char *string, ...);

s32 cmdq_core_reg_dump_begin(u32 taskID, u32 *regCount,
	u32 **regAddress);
s32 cmdq_core_reg_dump_end(u32 taskID, u32 regCount, u32 *regValues);

int cmdq_core_print_record_seq(struct seq_file *m, void *v);
int cmdq_core_print_status_seq(struct seq_file *m, void *v);

void cmdq_core_dump_trigger_loop_thread(const char *tag);

/* Save first error dump */
void cmdq_core_turnon_first_dump(const struct cmdqRecStruct *task);

void cmdq_core_turnoff_first_dump(void);
void cmdq_core_reset_first_dump(void);

/* cmdq_core_save_first_dump - save a CMDQ first error dump to file */
s32 cmdq_core_save_first_dump(const char *string, ...);
const char *cmdq_core_query_first_err_mod(void);

/* Allocate/Free HW use buffer, e.g. command buffer forCMDQ HW */
void *cmdq_core_alloc_hw_buffer_clt(struct device *dev, size_t size,
	dma_addr_t *dma_handle, const gfp_t flag, enum CMDQ_CLT_ENUM clt);
void cmdq_core_free_hw_buffer_clt(struct device *dev, size_t size,
	void *cpu_addr, dma_addr_t dma_handle, enum CMDQ_CLT_ENUM clt);
void *cmdq_core_alloc_hw_buffer(struct device *dev,
	size_t size, dma_addr_t *dma_handle, const gfp_t flag);
void cmdq_core_free_hw_buffer(struct device *dev, size_t size,
	void *cpu_addr, dma_addr_t dma_handle);
s32 cmdq_core_alloc_pool_buf(struct cmdq_pkt_buffer *buf);
s32 cmdq_core_free_pool_buf(struct cmdq_pkt_buffer *buf);
void cmdq_core_dump_sram(void);
s32 cmdq_core_alloc_sram_buffer(size_t size,
	const char *owner_name, u32 *out_cpr_offset);
void cmdq_core_free_sram_buffer(u32 cpr_offset, size_t size);
size_t cmdq_core_get_free_sram_size(void);
size_t cmdq_core_get_cpr_cnt(void);
void cmdq_delay_dump_thread(bool dump_sram);
u32 cmdq_core_get_delay_start_cpr(void);
s32 cmdq_delay_get_id_by_scenario(enum CMDQ_SCENARIO_ENUM scenario);
int cmdqCoreAllocWriteAddress(u32 count, dma_addr_t *paStart,
	enum CMDQ_CLT_ENUM clt);
u32 cmdqCoreReadWriteAddress(dma_addr_t pa);
void cmdqCoreReadWriteAddressBatch(u32 *addrs, u32 count, u32 *val_out);
u32 cmdqCoreWriteWriteAddress(dma_addr_t pa, u32 value);
int cmdqCoreFreeWriteAddress(dma_addr_t paStart, enum CMDQ_CLT_ENUM clt);

/* Get and HW information from device tree */
void cmdq_core_init_dts_data(void);
struct cmdqDTSDataStruct *cmdq_core_get_dts_data(void);
u32 cmdq_core_get_thread_prefetch_size(const s32 thread);
void cmdq_core_dump_dts_setting(void);

/* Get and set HW event form device tree */
void cmdq_core_set_event_table(enum cmdq_event event, const s32 value);
s32 cmdq_core_get_event_value(enum cmdq_event event);
const char *cmdq_core_get_event_name_enum(enum cmdq_event event);
const char *cmdq_core_get_event_name(u32 hw_event);

/* Immediately clear CMDQ event to 0 with CPU */
void cmdqCoreClearEvent(enum cmdq_event event);

/* Immediately set CMDQ event to 1 with CPU */
void cmdqCoreSetEvent(enum cmdq_event event);

/* Get event value with CPU. This is for debug purpose only
 * since it does not guarantee atomicity.
 */
u32 cmdqCoreGetEvent(enum cmdq_event event);


/* GCE capability */
void cmdq_core_reset_gce(void);
void cmdq_core_set_addon_subsys(u32 msb, s32 subsys_id, u32 mask);
u32 cmdq_core_subsys_to_reg_addr(u32 arg_a);
const char *cmdq_core_parse_subsys_from_reg_addr(u32 reg_addr);
s32 cmdq_core_subsys_from_phys_addr(u32 physAddr);

ssize_t cmdq_core_print_error(struct device *dev,
	struct device_attribute *attr, char *buf);
void cmdq_core_set_log_level(const s32 value);
ssize_t cmdq_core_print_log_level(struct device *dev,
	struct device_attribute *attr, char *buf);
ssize_t cmdq_core_write_log_level(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
ssize_t cmdq_core_print_profile_enable(struct device *dev,
	struct device_attribute *attr, char *buf);
ssize_t cmdq_core_write_profile_enable(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);

void cmdq_core_dump_tasks_info(void);
struct cmdqRecStruct *cmdq_core_get_valid_handle(unsigned long job);
u32 *cmdq_core_get_pc_inst(const struct cmdqRecStruct *handle,
	s32 thread, u32 insts[2], u32 *pa_out);
void cmdq_core_dump_handle_buffer(const struct cmdq_pkt *pkt,
	const char *tag);
u32 *cmdq_core_dump_pc(const struct cmdqRecStruct *handle,
	int thread, const char *tag);

s32 cmdq_core_is_group_flag(enum CMDQ_GROUP_ENUM engGroup, u64 engineFlag);
s32 cmdq_core_acquire_thread(enum CMDQ_SCENARIO_ENUM scenario, bool exclusive);
void cmdq_core_release_thread(s32 scenario, s32 thread);

bool cmdq_thread_in_use(void);

s32 cmdq_core_suspend_hw_thread(s32 thread);

u64 cmdq_core_get_gpr64(const enum cmdq_gpr_reg regID);
void cmdq_core_set_gpr64(const enum cmdq_gpr_reg regID, const u64 value);

void cmdq_core_release_handle_by_file_node(void *file_node);
s32 cmdq_core_suspend(void);
s32 cmdq_core_resume(void);
s32 cmdq_core_resume_notifier(void);

void cmdq_core_set_spm_mode(enum CMDQ_SPM_MODE mode);

struct cmdq_dts_setting *cmdq_core_get_dts_setting(void);
struct ContextStruct *cmdq_core_get_context(void);
struct CmdqCBkStruct *cmdq_core_get_group_cb(void);
dma_addr_t cmdq_core_get_pc(s32 thread);
dma_addr_t cmdq_core_get_end(s32 thread);
unsigned long cmdq_get_tracing_mark(void);
const struct cmdq_controller *cmdq_core_get_controller(void);


/* mailbox pkt flush functions */

s32 cmdq_pkt_get_cmd_by_pc(const struct cmdqRecStruct *handle, u32 pc,
	u32 *inst_out, u32 size);

void cmdq_pkt_get_first_buffer(struct cmdqRecStruct *handle,
	void **va_out, dma_addr_t *pa_out);

void *cmdq_pkt_get_first_va(const struct cmdqRecStruct *handle);

s32 cmdq_pkt_alloc_single_buffer_list(struct cmdqRecStruct *handle,
	struct cmdq_pkt_buffer **buf_out);

s32 cmdq_pkt_extend_cmd_buffer(struct cmdqRecStruct *handle);

s32 cmdq_pkt_copy_cmd(struct cmdqRecStruct *handle, void *src, const u32 size,
	const bool is_copy_from_user);

void cmdq_pkt_release_handle(struct cmdqRecStruct *handle);

s32 cmdq_pkt_dump_command(struct cmdqRecStruct *handle);

s32 cmdq_pkt_wait_flush_ex_result(struct cmdqRecStruct *handle);

s32 cmdq_pkt_auto_release_task(struct cmdqRecStruct *handle);

s32 cmdq_pkt_flush_ex(struct cmdqRecStruct *handle);

/*
 * cmdq_pkt_flush_async() - trigger CMDQ to asynchronously execute the CMDQ
 *                          packet and call back at the end of done packet
 * @client:	the CMDQ mailbox client
 * @pkt:	the CMDQ packet
 * @cb:		called at the end of done packet
 * @data:	this data will pass back to cb
 *
 * Return: 0 for success; else the error code is returned
 *
 * Trigger CMDQ to asynchronously execute the CMDQ packet and call back
 * at the end of done packet. Note that this is an ASYNC function. When the
 * function returned, it may or may not be finished.
 */
s32 cmdq_pkt_flush_async_ex(struct cmdqRecStruct *handle,
	CmdqAsyncFlushCB cb, u64 user_data, bool auto_release);

s32 cmdq_pkt_stop(struct cmdqRecStruct *handle);

/* mailbox helper functions */

s32 cmdq_helper_mbox_register(struct device *dev);
struct cmdq_client *cmdq_helper_mbox_client(u32 idx);
struct cmdq_base *cmdq_helper_mbox_base(void);
void cmdq_helper_mbox_clear_pools(void);
void cmdq_core_initialize(void);
void cmdq_core_deinitialize(void);
void cmdq_helper_ext_deinit(void);

struct cmdqSecSharedMemoryStruct *cmdq_core_get_secure_shared_memory(void);
void cmdq_core_attach_error_handle(const struct cmdqRecStruct *handle,
	s32 thread);
#endif
