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

#ifndef __CMDQ_CORE_H__
#define __CMDQ_CORE_H__

#include "cmdq_helper_ext.h"

#if 0
#include <linux/list.h>
#include <linux/time.h>
#ifdef CMDQ_AEE_READY
#include <mt-plat/aee.h>
#endif
#include <linux/device.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include "cmdq_def.h"
#include "cmdq_event_common.h"

/* address conversion for 4GB ram support:
 * .address register: 32 bit
 * .physical address: 32 bit,
 *	or 64 bit for CONFIG_ARCH_DMA_ADDR_T_64BIT enabled
 *
 * when 33 bit enabled(4GB ram), 0x_0_xxxx_xxxx
 *	and 0x_1_xxxx_xxxx access is same for CPU
 *
 *
 * 0x0        0x0_4000_0000    0x1_0000_0000     0x1_4000_0000
 * |-1GB HW addr-|----3GB DRAM----|--1GB DRAM(new)--|---3GB DRAM(same)----|
 * |                                     |
 * |<----------4GB RAM support---------->|
 */
#define CMDQ_PHYS_TO_AREG(addr) ((addr) & 0xFFFFFFFF) /* truncate directly */
#define CMDQ_AREG_TO_PHYS(addr) ((addr) | 0L)
/* Always set 33 bit to 1 under 4GB special mode */
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
#define CMDQ_GET_HIGH_ADDR(addr, highAddr)		\
{							\
if (enable_4G())					\
	highAddr = 0x1;					\
else							\
	highAddr = ((addr >> 32) & 0xffff);		\
}
#else
#define CMDQ_GET_HIGH_ADDR(addr, highAddr) { highAddr = 0; }
#endif

#define CMDQ_LONGSTRING_MAX (180)
#define CMDQ_DELAY_RELEASE_RESOURCE_MS (1000)

#define CMDQ_THREAD_SEC_PRIMARY_DISP	(CMDQ_MIN_SECURE_THREAD_ID)
#define CMDQ_THREAD_SEC_SUB_DISP	(CMDQ_MIN_SECURE_THREAD_ID + 1)
#define CMDQ_THREAD_SEC_MDP		(CMDQ_MIN_SECURE_THREAD_ID + 2)

/* max count of regs */
#define CMDQ_MAX_COMMAND_SIZE		(0x10000)
#define CMDQ_MAX_DUMP_REG_COUNT		(2048)
#define CMDQ_MAX_WRITE_ADDR_COUNT	(PAGE_SIZE / sizeof(u32))
#define CMDQ_MAX_DBG_STR_LEN		1024

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
	char cmdqString[CMDQ_MAX_FIRSTERROR];
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
		DB_OPT_MMPROFILE_BUFFER | DB_OPTs, \
		dispatchedTag, "error: "string, ##args); \
} while (0);	\
}

#define CMDQ_AEE(tag, string, args...) \
{ \
	CMDQ_AEE_EX(DB_OPT_DUMP_DISPLAY, tag, string, ##args) \
}

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
do {if (cmdq_core_profile_enabled() > 0) met_tag_init(); } while (0);	\
}

#define CMDQ_PROF_START(args...)	\
{		\
do {if (cmdq_core_profile_enabled() > 0) met_tag_start(args);	\
	} while (0);	\
}

#define CMDQ_PROF_END(args...)	\
{		\
do {if (cmdq_core_profile_enabled() > 0) met_tag_end(args);	\
	} while (0);	\
}
#define CMDQ_PROF_ONESHOT(args...)	\
{		\
do {if (cmdq_core_profile_enabled() > 0) met_tag_oneshot(args);	\
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
	((struct TaskPrivateStruct *)task->privateData)
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
typedef s32(*CmdqResourceReleaseCB) (enum CMDQ_EVENT_ENUM resourceEvent);

/* resource event can be indicated to resource unit */
typedef s32(*CmdqResourceAvailableCB) (
	enum CMDQ_EVENT_ENUM resourceEvent);

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

struct TaskStruct;

/* finished task can be get by callback */
typedef void(*CmdqTrackTaskCB) (const struct TaskStruct *pTask);

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
};

struct CmdqDebugCBkStruct {
	/* Debug Register Dump */
	CmdqDebugRegDumpBeginCB beginDebugRegDump;
	CmdqDebugRegDumpEndCB endDebugRegDump;
};

enum CMDQ_CODE_ENUM {
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
	CMDQ_CODE_RAW = 0x24,	/* allow entirely custom arg_a/arg_b */
	CMDQ_CODE_PREFETCH_ENABLE = 0x41,	/* enable prefetch marker */
	CMDQ_CODE_PREFETCH_DISABLE = 0x42,	/* disable prefetch marker */
	CMDQ_CODE_READ_S = 0x80,	/* read operation (v3 only) */
	CMDQ_CODE_WRITE_S = 0x90,	/* write operation (v3 only) */
	/* write with mask operation (v3 only) */
	CMDQ_CODE_WRITE_S_W_MASK = 0x91,
	CMDQ_CODE_LOGIC = 0xa0,	/* logic operation */
	CMDQ_CODE_JUMP_C_ABSOLUTE = 0xb0, /* conditional jump (absolute) */
	CMDQ_CODE_JUMP_C_RELATIVE = 0xb1, /* conditional jump (related) */
};

#define subsys_lsb_bit (16)
enum CMDQ_CONDITION_ENUM {
	CMDQ_CONDITION_ERROR = -1,

	/* these are actual HW op code */
	CMDQ_EQUAL = 0,
	CMDQ_NOT_EQUAL = 1,
	CMDQ_GREATER_THAN_AND_EQUAL = 2,
	CMDQ_LESS_THAN_AND_EQUAL = 3,
	CMDQ_GREATER_THAN = 4,
	CMDQ_LESS_THAN = 5,

	CMDQ_CONDITION_MAX,
};

enum CMDQ_LOGIC_ENUM {
	/* these are actual HW sOP code */
	CMDQ_LOGIC_ASSIGN = 0,
	CMDQ_LOGIC_ADD = 1,
	CMDQ_LOGIC_SUBTRACT = 2,
	CMDQ_LOGIC_MULTIPLY = 3,
	CMDQ_LOGIC_XOR = 8,
	CMDQ_LOGIC_NOT = 9,
	CMDQ_LOGIC_OR = 10,
	CMDQ_LOGIC_AND = 11,
	CMDQ_LOGIC_LEFT_SHIFT = 12,
	CMDQ_LOGIC_RIGHT_SHIFT = 13
};

enum CMDQ_LOG_LEVEL_ENUM {
	CMDQ_LOG_LEVEL_NORMAL = 0,
	CMDQ_LOG_LEVEL_MSG = 1,
	CMDQ_LOG_LEVEL_FULL_ERROR = 2,
	CMDQ_LOG_LEVEL_EXTENSION = 3,

	CMDQ_LOG_LEVEL_MAX	/* ALWAYS keep at the end */
};

enum TASK_STATE_ENUM {
	TASK_STATE_IDLE,	/* free task */
	TASK_STATE_BUSY,	/* task running on a thread */
	TASK_STATE_KILLED,	/* task process being killed */
	TASK_STATE_ERROR,	/* task execution error */
	TASK_STATE_ERR_IRQ,	/* task execution invalid instruction */
	TASK_STATE_DONE,	/* task finished */
	TASK_STATE_WAITING, /* allocated but waiting for available thread */
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

struct CmdBufferStruct {
	struct list_head listEntry;
	u32 *pVABase;	/* virtual address of command buffer */
	dma_addr_t MVABase;	/* physical address of command buffer */
	bool use_pool;
};

struct CmdFreeWorkStruct {
	struct list_head cmd_buffer_list;
	struct work_struct free_buffer_work;
};

struct TaskPrivateStruct {
	void *node_private_data;
	bool internal;		/* internal used only task */
	bool ignore_timeout;	/* timeout is expected */
};

struct TaskStruct {
	struct list_head listEntry;

	/* For buffer state */
	enum TASK_STATE_ENUM taskState;	/* task life cycle */
	/* list of allocated command buffer */
	struct list_head cmd_buffer_list;
	/* available size for last buffer in list */
	u32 buf_available_size;
	u32 bufferSize;	/* size of allocated command buffer */

	/* For execution */
	s32 scenario;
	s32 priority;
	u64 engineFlag;
	s32 commandSize;
	u32 *pCMDEnd;
	s32 reorder;
	s32 thread;	/* ASYNC: CMDQ_INVALID_THREAD if not running */
	s32 irqFlag;	/* ASYNC: flag of IRQ received */
	CmdqInterruptCB loopCallback;	/* LOOP execution */
	unsigned long loopData;	/* LOOP execution */
	/* Callback on AsyncFlush (fire-and-forget) tasks */
	CmdqAsyncFlushCB flushCallback;
	u64 flushData;	/* for callbacks & error handling */
	/* Work item when auto release is used */
	struct work_struct autoReleaseWork;
	atomic_t useWorkQueue;
	u64 res_engine_flag_acquire;	/* task start use share sram */
	u64 res_engine_flag_release;	/* task stop use share sram */

	/* Output section for "read from reg to mem" */
	u32 regCount;
	u32 *regResults;
	dma_addr_t regResultsMVA;

	/* For register backup
	 * this is to separate backup request from user space and kernel space
	 */
	u32 regCountUserSpace;
	/* user data store for callback beginDebugRegDump / endDebugRegDump */
	u32 regUserToken;

	/* For seucre execution */
	struct cmdqSecDataStruct secData;
	struct iwcCmdqSecStatus_t *secStatus;
	/* For v3 CPR use */
	struct cmdq_v3_replace_struct replace_instr;
	/* use SRAM or not */
	bool use_sram_buffer;
	/* Original PA address of SRAM buffer content */
	u32 sram_base;

	/* For statistics & debug */
	/* ASYNC: task submit time (as soon as task acquired) */
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
	bool dumpAllocTime;	/* flag to print static info to kernel log. */

	void *privateData;	/* track associated file handle */

	pid_t callerPid;
	char callerName[TASK_COMM_LEN];
	char *userDebugStr;

	/* Custom profile marker */
	struct cmdqProfileMarkerStruct profileMarker;
};

/* record NG task info for dump */
struct NGTaskInfoStruct {
	const struct TaskStruct *ngtask;
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

struct CmdqRecExtend {
	/* task access share sram engine */
	u64 res_engine_flag_acquire;
	u64 res_engine_flag_release;
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
	struct TaskStruct *pCurTask[CMDQ_MAX_TASK_IN_THREAD];

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
	u64 profileMarkerTimeNS[CMDQ_MAX_PROFILE_MARKER_IN_TASK];
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

/*
 * shared memory between normal and secure world
 */
struct cmdqSecSharedMemoryStruct {
	void *pVABase;		/* virtual address of command buffer */
	dma_addr_t MVABase;	/* physical address of command buffer */
	u32 size;		/* buffer size */
};

/*
 * resource unit between each module
 */
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
	enum CMDQ_EVENT_ENUM lockEvent;	/* SW token to lock in GCE thread */
	u32 engine_id;			/* which engine is resource */
	u64 engine_flag;		/* engine flag */
	CmdqResourceAvailableCB availableCB;
	CmdqResourceReleaseCB releaseCB;
	/* Delay Work item when delay check is used */
	struct delayed_work delayCheckWork;
};

/*
 * SRAM chunk structure
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

struct ContextStruct {
	/* Task information */
	struct kmem_cache *taskCache;	/* TaskStruct object cache */
	struct list_head taskFreeList;	/* Unused free tasks */
	struct list_head taskActiveList;	/* Active tasks */
	struct list_head taskWaitList;	/* Tasks waiting for available thread */
	struct work_struct taskConsumeWaitQueueItem;
	/* auto-release workqueue */
	struct workqueue_struct *taskAutoReleaseWQ;
	/* task consumption workqueue (for queued tasks) */
	struct workqueue_struct *taskConsumeWQ;
	/* delay resource check workqueue */
	struct workqueue_struct *resourceCheckWQ;

	/* Write Address management */
	struct list_head writeAddrList;

	/* Basic information */
	struct EngineStruct engine[CMDQ_MAX_ENGINE_COUNT];
	struct ThreadStruct *thread;

	/* auto-release workqueue per thread */
	struct workqueue_struct **taskThreadAutoReleaseWQ;

	/* Secure path shared information */
	struct cmdqSecSharedMemoryStruct *hSecSharedMem;
	void *hNotifyLoop;

	/* Profile information */
	s32 enableProfile;
	s32 lastID;
	s32 recNum;
	struct RecordStruct record[CMDQ_MAX_RECORD_COUNT];

	/* Error information */
	s32 logLevel;
	s32 errNum;
	struct ErrorStruct error[CMDQ_MAX_ERROR_COUNT];

	/* feature option information */
	u32 features[CMDQ_FEATURE_TYPE_MAX];

	/* Resource manager information */
	struct list_head resourceList;	/* all resource list */

	/* SRAM manager information */
	struct list_head sram_allocated_list;	/* all allocated SRAM chunk */
	size_t allocated_sram_count;

	/* Delay set CPR start information */
	u32 delay_cpr_start;

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

typedef void (*cmdqStressCallback)(struct TaskStruct *task, s32 thread);

struct StressContextStruct {
	cmdqStressCallback exec_suspend;
};

struct cmdq_event_table {
	u16 event;	/* cmdq event enum value */
	const char *event_name;
	const char *dts_name;
};

struct cmdq_subsys_dts_name {
	const char *name;
	const char *group;
};

#ifdef __cplusplus
extern "C" {
#endif

void cmdqCoreInitGroupCB(void);
void cmdqCoreDeinitGroupCB(void);

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

s32 cmdqCoreSuspend(void);

s32 cmdqCoreResume(void);

s32 cmdqCoreResumedNotifier(void);

void cmdqCoreHandleIRQ(s32 index);

/*
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
 *      loopCB: Assign this CB if your command loops itself.
 *          since this disables thread completion handling, do not set this CB
 *          if it is not intended to be a HW looping thread.
 *      loopData:  The user data passed to loopCB
 *
 * Return:
 *      >=0 for success; else the error code is returned
 */
s32 cmdqCoreSubmitTaskAsync(struct cmdqCommandStruct *pCommandDesc,
	struct CmdqRecExtend *ext, CmdqInterruptCB loopCB,
	unsigned long loopData, struct TaskStruct **ppTaskOut);

/*
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
s32 cmdqCoreWaitAndReleaseTask(struct TaskStruct *pTask, u32 timeout_ms);

/*
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
s32 cmdqCoreWaitResultAndReleaseTask(struct TaskStruct *pTask,
	struct cmdqRegValueStruct *pResult, u32 timeout_ms);

/*
 * Stop task and release it immediately
 *
 * Parameter:
 *      pTask: Task returned from successful cmdqCoreSubmitTaskAsync
 *             additional cleanup will be performed.
 *
 * Return:
 *      >=0 for success; else the error code is returned
 */
s32 cmdqCoreReleaseTask(struct TaskStruct *pTask);

/*
 * Register the task in the auto-release queue. It will be released
 * upon finishing. You MUST NOT perform further operations on this task.
 *
 * Parameter:
 *      pTask: Task returned from successful cmdqCoreSubmitTaskAsync.
 *             additional cleanup will be performed.
 * Return:
 *      >=0 for success; else the error code is returned
 */
s32 cmdqCoreAutoReleaseTask(struct TaskStruct *pTask);

/*
 * Create CMDQ Task and block wait for its completion
 *
 * Return:
 *     >=0 for success; else the error code is returned
 */
s32 cmdqCoreSubmitTask(struct cmdqCommandStruct *pCommandDesc,
	struct CmdqRecExtend *ext);

/*
 * Helper function get valid task pointer
 *
 * Return:
 *     task pointer if available
 */
struct TaskStruct *cmdq_core_get_task_ptr(void *task_handle);

/*
 * Immediately clear CMDQ event to 0 with CPU
 *
 */
void cmdqCoreClearEvent(enum CMDQ_EVENT_ENUM event);

/*
 * Immediately set CMDQ event to 1 with CPU
 */
void cmdqCoreSetEvent(enum CMDQ_EVENT_ENUM event);

/*
 * Get event value with CPU. This is for debug purpose only
 * since it does not guarantee atomicity.
 */
u32 cmdqCoreGetEvent(enum CMDQ_EVENT_ENUM event);

s32 cmdqCoreInitialize(void);
s32 cmdqCoreLateInitialize(void);
void cmdqCoreDeInitialize(void);

/*
 * Allocate/Free HW use buffer, e.g. command buffer forCMDQ HW
 */
void *cmdq_core_alloc_hw_buffer(struct device *dev,
	size_t size, dma_addr_t *dma_handle, const gfp_t flag);
void cmdq_core_free_hw_buffer(struct device *dev, size_t size,
	void *cpu_addr, dma_addr_t dma_handle);

struct cmdqSecSharedMemoryStruct *cmdq_core_get_secure_shared_memory(void);

/*
 * Allocate/Free GCE SRAM
 */
s32 cmdq_core_alloc_sram_buffer(size_t size,
	const char *owner_name, u32 *out_sram_addr);
void cmdq_core_free_sram_buffer(u32 sram_addr, size_t size);
size_t cmdq_core_get_free_sram_size(void);
void cmdq_core_dump_sram(void);

/* Delay control */
u32 cmdq_core_get_delay_start_cpr(void);
s32 cmdq_get_delay_id_by_scenario(enum CMDQ_SCENARIO_ENUM scenario);

/*
 * GCE capability
 */
u32 cmdq_core_subsys_to_reg_addr(u32 arg_a);
const char *cmdq_core_parse_subsys_from_reg_addr(u32 reg_addr);
s32 cmdq_core_subsys_from_phys_addr(u32 physAddr);
void cmdq_core_set_addon_subsys(u32 msb, s32 subsys_id, u32 mask);
s32 cmdq_core_suspend_HW_thread(s32 thread, u32 lineNum);

/*
 * Event
 */
void cmdq_core_reset_hw_events(void);

/*
 * Get and HW information from device tree
 */
void cmdq_core_init_DTS_data(void);
struct cmdqDTSDataStruct *cmdq_core_get_whole_DTS_Data(void);
u32 cmdq_core_get_thread_prefetch_size(s32 thread);

/*
 * Get and set HW event form device tree
 */
void cmdq_core_set_event_table(enum CMDQ_EVENT_ENUM event, const s32 value);
s32 cmdq_core_get_event_value(enum CMDQ_EVENT_ENUM event);
const char *cmdq_core_get_event_name_ENUM(enum CMDQ_EVENT_ENUM event);
const char *cmdq_core_get_event_name(enum CMDQ_EVENT_ENUM event);

/*
 * Utilities
 */
void cmdq_core_set_log_level(const s32 value);
ssize_t cmdqCorePrintLogLevel(struct device *dev,
	struct device_attribute *attr, char *buf);
ssize_t cmdqCoreWriteLogLevel(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);

ssize_t cmdqCorePrintProfileEnable(struct device *dev,
	struct device_attribute *attr, char *buf);
ssize_t cmdqCoreWriteProfileEnable(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);

void cmdq_core_dump_thread(s32 thread, const char *tag);
void cmdq_core_dump_tasks_info(void);
void cmdq_core_dump_secure_metadata(struct cmdqSecDataStruct *pSecData);
s32 cmdqCoreDebugRegDumpBegin(u32 taskID, u32 *regCount,
	u32 **regAddress);
s32 cmdqCoreDebugRegDumpEnd(u32 taskID, u32 regCount, u32 *regValues);
s32 cmdqCoreDebugDumpCommand(const struct TaskStruct *pTask);
s32 cmdqCoreDebugDumpSRAM(u32 sram_base, u32 command_size);
void cmdqCoreDumpCommandMem(const u32 *pCmd, s32 commandSize);
void cmdq_delay_dump_thread(bool dump_sram);
s32 cmdqCoreQueryUsage(s32 *pCount);

int cmdqCorePrintRecordSeq(struct seq_file *m, void *v);
int cmdqCorePrintStatusSeq(struct seq_file *m, void *v);

ssize_t cmdqCorePrintError(struct device *dev,
	struct device_attribute *attr, char *buf);

void cmdq_core_fix_command_scenario_for_user_space(
	struct cmdqCommandStruct *pCommand);
bool cmdq_core_is_request_from_user_space(
	const enum CMDQ_SCENARIO_ENUM scenario);

unsigned long long cmdq_core_get_GPR64(
	const enum CMDQ_DATA_REGISTER_ENUM regID);
void cmdq_core_set_GPR64(const enum CMDQ_DATA_REGISTER_ENUM regID,
	const unsigned long long value);

u32 cmdqCoreReadDataRegister(enum CMDQ_DATA_REGISTER_ENUM regID);

int cmdqCoreAllocWriteAddress(u32 count, dma_addr_t *paStart);
int cmdqCoreFreeWriteAddress(dma_addr_t paStart);
u32 cmdqCoreReadWriteAddress(dma_addr_t pa);
u32 cmdqCoreWriteWriteAddress(dma_addr_t pa, u32 value);

s32 cmdq_core_profile_enabled(void);

bool cmdq_core_should_print_msg(void);
bool cmdq_core_should_full_error(void);

s32 cmdq_core_parse_instruction(const u32 *pCmd, char *textBuf, int bufLen);

void cmdq_core_add_consume_task(void);

/* file_node is a pointer to cmdqFileNodeStruct that is
 * created when opening the device file.
 */
void cmdq_core_release_task_by_file_node(void *file_node);

void cmdq_long_string_init(bool force, char *buf, u32 *offset, s32 *max_size);
void cmdq_long_string(char *buf, u32 *offset, s32 *max_size,
	const char *string, ...);

/* Command Buffer Dump */
void cmdq_core_set_command_buffer_dump(s32 scenario, s32 bufferSize);

/* Dump secure task status */
void cmdq_core_dump_secure_task_status(void);

/* test case initialization */
void cmdq_test_init_setting(void);

#ifdef CMDQ_INSTRUCTION_COUNT
CmdqModulePAStatStruct *cmdq_core_Initial_and_get_module_stat(void);
ssize_t cmdqCorePrintInstructionCountLevel(struct device *dev,
	struct device_attribute *attr, char *buf);
ssize_t cmdqCoreWriteInstructionCountLevel(struct device *dev,
	struct device_attribute *attr, const char *buf,
	size_t size);
void cmdq_core_set_instruction_count_level(const s32 value);
int cmdqCorePrintInstructionCountSeq(struct seq_file *m, void *v);
#endif				/* CMDQ_INSTRUCTION_COUNT */

/*
 * Save first error dump
 */
void cmdq_core_turnon_first_dump(const struct TaskStruct *pTask);
void cmdq_core_turnoff_first_dump(void);
void cmdq_core_reset_first_dump(void);

/*
 * cmdq_core_save_first_dump - save a CMDQ first error dump to file
 */
s32 cmdq_core_save_first_dump(const char *string, ...);

/*
 * cmdq_core_save_hex_first_dump - save a CMDQ first error hex dump to file
 * @prefix_str: string to prefix each line with;
 *	caller supplies trailing spaces for alignment if desired
 * @rowsize: number of bytes to print per line; must be 16 or 32
 * @groupsize: number of bytes to print at a time (1, 2, 4, 8; default = 1)
 * @buf: data blob to dump
 * @len: number of bytes in the @buf
 *
 * Given a buffer of u8 data, cmdq_core_save_hex_first_dump()
 * save a CMDQ first error hex dump
 * to the file , with an optional leading prefix.
 *
 * cmdq_core_save_hex_first_dump() works on one "line" of output at a time,
 * i.e., 16 or 32 bytes of input data converted to hex.
 * cmdq_core_save_hex_first_dump() iterates over the entire input @buf,
 * breaking it into "line size" chunks to format and print.
 *
 * E.g.:
 *	 cmdq_core_save_hex_first_dump("", 16, 4,
 *			       pTask->pVABase, (pTask->commandSize));
 *
 * Example output using 4-byte mode:
 * ed7e4510: ff7fffff 02000000 00800000 0804401d
 * ed7e4520: fffffffe 02000000 00000001 04044001
 * ed7e4530: 80008001 20000043
 */
void cmdq_core_save_hex_first_dump(const char *prefix_str,
	int rowsize, int groupsize, const void *buf, size_t len);

void cmdqCoreLockResource(u64 engineFlag, bool fromNotify);
bool cmdqCoreAcquireResource(enum CMDQ_EVENT_ENUM resourceEvent,
u64 *engine_flag_out);
void cmdqCoreReleaseResource(enum CMDQ_EVENT_ENUM resourceEvent,
	u64 *engine_flag_out);
void cmdqCoreSetResourceCallback(enum CMDQ_EVENT_ENUM resourceEvent,
	CmdqResourceAvailableCB resourceAvailable,
	CmdqResourceReleaseCB resourceRelease);

void cmdq_core_dump_dts_setting(void);
s32 cmdq_core_get_running_task_by_engine_unlock(u64 engineFlag,
	u32 userDebugStrLen, struct TaskStruct *p_out_task);
s32 cmdq_core_get_running_task_by_engine(u64 engineFlag,
	u32 userDebugStrLen, struct TaskStruct *p_out_task);
u32 cmdq_core_thread_prefetch_size(const s32 thread);

void cmdq_core_dump_feature(void);
void cmdq_core_set_feature(enum CMDQ_FEATURE_TYPE_ENUM featureOption,
	u32 value);
u32 cmdq_core_get_feature(enum CMDQ_FEATURE_TYPE_ENUM featureOption);
bool cmdq_core_is_feature_on(enum CMDQ_FEATURE_TYPE_ENUM featureOption);
void cmdq_core_set_mem_monitor(bool enable);
void cmdq_core_dump_mem_monitor(void);

void cmdq_core_dump_task_mem(const struct TaskStruct *pTask, bool full_dump);

struct StressContextStruct *cmdq_core_get_stress_context(void);
void cmdq_core_clean_stress_context(void);
bool cmdq_core_is_clock_enabled(void);
struct cmdq_dts_setting *cmdq_core_get_dts_setting(void);

struct cmdq_event_table *cmdq_event_get_table(void);
u32 cmdq_event_get_table_size(void);
struct cmdq_subsys_dts_name *cmdq_subsys_get_dts(void);
u32 cmdq_subsys_get_size(void);

#ifdef __cplusplus
}
#endif
#endif
#endif	/* __CMDQ_CORE_H__ */
