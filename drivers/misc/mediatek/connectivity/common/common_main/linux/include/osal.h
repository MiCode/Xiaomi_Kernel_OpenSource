/*
 * Copyright (C) 2016 MediaTek Inc.
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


/*! \file
 * \brief  Declaration of library functions
 * Any definitions in this file will be shared among GLUE Layer and internal Driver Stack.
 */

#ifndef _OSAL_H_
#define _OSAL_H_

#include "osal_typedef.h"
#include "../../debug_utility/ring.h"
#include <linux/workqueue.h>
/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

#define OS_BIT_OPS_SUPPORT 1

#define _osal_inline_ inline

#define MAX_THREAD_NAME_LEN 16
#define MAX_WAKE_LOCK_NAME_LEN 16
#define MAX_HISTORY_NAME_LEN 16
#define OSAL_OP_BUF_SIZE    64


#if (defined(CONFIG_MTK_GMO_RAM_OPTIMIZE) && !defined(CONFIG_MTK_ENG_BUILD))
#define OSAL_OP_DATA_SIZE   8
#else
#define OSAL_OP_DATA_SIZE   32
#endif

#define DBG_LOG_STR_SIZE    256

#define osal_sizeof(x) sizeof(x)

#define osal_array_size(x) ARRAY_SIZE(x)

#ifndef NAME_MAX
#define NAME_MAX 256
#endif

#define WMT_OP_BIT(x) (0x1UL << x)
#define WMT_OP_HIF_BIT WMT_OP_BIT(0)

#define GET_BIT_MASK(value, mask) ((value) & (mask))
#define SET_BIT_MASK(pdest, value, mask) (*(pdest) = (GET_BIT_MASK(*(pdest), ~(mask)) | GET_BIT_MASK(value, mask)))
#define GET_BIT_RANGE(data, end, begin) ((data) & GENMASK(end, begin))
#define SET_BIT_RANGE(pdest, data, end, begin) (SET_BIT_MASK(pdest, data, GENMASK(end, begin)))

#define RB_LATEST(prb) ((prb)->write - 1)
#define RB_SIZE(prb) ((prb)->size)
#define RB_MASK(prb) (RB_SIZE(prb) - 1)
#define RB_COUNT(prb) ((prb)->write - (prb)->read)
#define RB_FULL(prb) (RB_COUNT(prb) >= RB_SIZE(prb))
#define RB_EMPTY(prb) ((prb)->write == (prb)->read)

#define RB_INIT(prb, qsize) \
do { \
	(prb)->read = (prb)->write = 0; \
	(prb)->size = (qsize); \
} while (0)

#define RB_PUT(prb, value) \
do { \
	if (!RB_FULL(prb)) { \
		(prb)->queue[(prb)->write & RB_MASK(prb)] = value; \
		++((prb)->write); \
	} \
	else { \
		osal_assert(!RB_FULL(prb)); \
	} \
} while (0)

#define RB_GET(prb, value) \
do { \
	if (!RB_EMPTY(prb)) { \
		value = (prb)->queue[(prb)->read & RB_MASK(prb)]; \
		++((prb)->read); \
		if (RB_EMPTY(prb)) { \
			(prb)->read = (prb)->write = 0; \
		} \
	} \
	else { \
		value = NULL; \
		osal_assert(!RB_EMPTY(prb)); \
	} \
} while (0)

#define RB_GET_LATEST(prb, value) \
do { \
	if (!RB_EMPTY(prb)) { \
		value = (prb)->queue[RB_LATEST(prb) & RB_MASK(prb)]; \
		if (RB_EMPTY(prb)) { \
			(prb)->read = (prb)->write = 0; \
		} \
	} \
	else { \
		value = NULL; \
	} \
} while (0)

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0))
typedef VOID(*P_TIMEOUT_HANDLER) (struct timer_list *t);
typedef struct timer_list *timer_handler_arg;
#define GET_HANDLER_DATA(arg, data) \
do { \
	P_OSAL_TIMER osal_timer = from_timer(osal_timer, arg, timer); \
	data = osal_timer->timeroutHandlerData; \
} while (0)
#else
typedef VOID(*P_TIMEOUT_HANDLER) (ULONG);
typedef ULONG timer_handler_arg;
#define GET_HANDLER_DATA(arg, data) (data = arg)
#endif

typedef INT32(*P_COND) (PVOID);

typedef struct _OSAL_TIMER_ {
	struct timer_list timer;
	P_TIMEOUT_HANDLER timeoutHandler;
	ULONG timeroutHandlerData;
} OSAL_TIMER, *P_OSAL_TIMER;

typedef struct _OSAL_UNSLEEPABLE_LOCK_ {
	spinlock_t lock;
	ULONG flag;
} OSAL_UNSLEEPABLE_LOCK, *P_OSAL_UNSLEEPABLE_LOCK;

typedef struct _OSAL_SLEEPABLE_LOCK_ {
	struct mutex lock;
} OSAL_SLEEPABLE_LOCK, *P_OSAL_SLEEPABLE_LOCK;

typedef struct _OSAL_SIGNAL_ {
	struct completion comp;
	UINT32 timeoutValue;
	UINT32 timeoutExtension;	/* max number of timeout caused by thread not able to acquire CPU */
} OSAL_SIGNAL, *P_OSAL_SIGNAL;

typedef struct _OSAL_EVENT_ {
	wait_queue_head_t waitQueue;
/* VOID *pWaitQueueData; */
	UINT32 timeoutValue;
	INT32 waitFlag;

} OSAL_EVENT, *P_OSAL_EVENT;

/* Data collected from sched_entity and sched_statistics */
typedef struct _OSAL_THREAD_SCHEDSTATS_ {
	UINT64 time;		/* when marked: the profiling start time(ms), when unmarked: total duration(ms) */
	UINT64 exec;		/* time spent in exec (sum_exec_runtime) */
	UINT64 runnable;	/* time spent in run-queue while not being scheduled (wait_sum) */
	UINT64 iowait;		/* time spent waiting for I/O (iowait_sum) */
} OSAL_THREAD_SCHEDSTATS, *P_OSAL_THREAD_SCHEDSTATS;

typedef struct _OSAL_THREAD_ {
	struct task_struct *pThread;
	PVOID pThreadFunc;
	PVOID pThreadData;
	INT8 threadName[MAX_THREAD_NAME_LEN];
} OSAL_THREAD, *P_OSAL_THREAD;


typedef struct _OSAL_FIFO_ {
	/*fifo definition */
	PVOID pFifoBody;
	spinlock_t fifoSpinlock;
	/*fifo operations */
	INT32 (*FifoInit)(struct _OSAL_FIFO_ *pFifo, PUINT8 buf, UINT32);
	INT32 (*FifoDeInit)(struct _OSAL_FIFO_ *pFifo);
	INT32 (*FifoReset)(struct _OSAL_FIFO_ *pFifo);
	INT32 (*FifoSz)(struct _OSAL_FIFO_ *pFifo);
	INT32 (*FifoAvailSz)(struct _OSAL_FIFO_ *pFifo);
	INT32 (*FifoLen)(struct _OSAL_FIFO_ *pFifo);
	INT32 (*FifoIsEmpty)(struct _OSAL_FIFO_ *pFifo);
	INT32 (*FifoIsFull)(struct _OSAL_FIFO_ *pFifo);
	INT32 (*FifoDataIn)(struct _OSAL_FIFO_ *pFifo, const PVOID buf, UINT32 len);
	INT32 (*FifoDataOut)(struct _OSAL_FIFO_ *pFifo, PVOID buf, UINT32 len);
} OSAL_FIFO, *P_OSAL_FIFO;

typedef struct firmware osal_firmware;

typedef struct _OSAL_OP_DAT {
	UINT32 opId;		/* Event ID */
	UINT32 u4InfoBit;	/* Reserved */
	SIZE_T au4OpData[OSAL_OP_DATA_SIZE];	/* OP Data */
} OSAL_OP_DAT, *P_OSAL_OP_DAT;

typedef struct _OSAL_LXOP_ {
	OSAL_OP_DAT op;
	OSAL_SIGNAL signal;
	INT32 result;
	atomic_t ref_count;
} OSAL_OP, *P_OSAL_OP;

typedef struct _OSAL_LXOP_Q {
	OSAL_SLEEPABLE_LOCK sLock;
	UINT32 write;
	UINT32 read;
	UINT32 size;
	P_OSAL_OP queue[OSAL_OP_BUF_SIZE];
} OSAL_OP_Q, *P_OSAL_OP_Q;

typedef struct _OSAL_WAKE_LOCK_ {
	struct wakeup_source *wake_lock;
	UINT8 name[MAX_WAKE_LOCK_NAME_LEN];
	INT32 init_flag;
} OSAL_WAKE_LOCK, *P_OSAL_WAKE_LOCK;
#if 1
typedef struct _OSAL_BIT_OP_VAR_ {
	ULONG data;
	OSAL_UNSLEEPABLE_LOCK opLock;
} OSAL_BIT_OP_VAR, *P_OSAL_BIT_OP_VAR;
#else
#define OSAL_BIT_OP_VAR unsigned long
#define P_OSAL_BIT_OP_VAR unsigned long *

#endif
typedef UINT32(*P_OSAL_EVENT_CHECKER) (P_OSAL_THREAD pThread);

struct osal_op_history_entry {
	VOID *opbuf_address;
	UINT32 op_id;
	UINT32 opbuf_ref_count;
	UINT32 op_info_bit;
	SIZE_T param_0;
	SIZE_T param_1;
	SIZE_T param_2;
	SIZE_T param_3;
	UINT64 ts;
	ULONG usec;
};

struct osal_op_history {
	struct ring ring_buffer;
	struct osal_op_history_entry *queue;
	spinlock_t lock;
	struct ring dump_ring_buffer;
	struct work_struct dump_work;
	UINT8 name[MAX_HISTORY_NAME_LEN];
};

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

UINT32 osal_strlen(const PINT8 str);
INT32 osal_strcmp(const PINT8 dst, const PINT8 src);
INT32 osal_strncmp(const PINT8 dst, const PINT8 src, UINT32 len);
PINT8 osal_strcpy(PINT8 dst, const PINT8 src);
PINT8 osal_strncpy(PINT8 dst, const PINT8 src, UINT32 len);
PINT8 osal_strcat(PINT8 dst, const PINT8 src);
PINT8 osal_strncat(PINT8 dst, const PINT8 src, UINT32 len);
PINT8 osal_strchr(const PINT8 str, UINT8 c);
PINT8 osal_strsep(PPINT8 str, const PINT8 c);
INT32 osal_strtol(const PINT8 str, UINT32 adecimal, PLONG res);
PINT8 osal_strstr(PINT8 str1, const PINT8 str2);
PINT8 osal_strnstr(PINT8 str1, const PINT8 str2, INT32 n);

VOID osal_bug_on(UINT32 val);

INT32 osal_snprintf(PINT8 buf, UINT32 len, const PINT8 fmt, ...);
INT32 osal_err_print(const PINT8 str, ...);
INT32 osal_dbg_print(const PINT8 str, ...);
INT32 osal_warn_print(const PINT8 str, ...);

INT32 osal_dbg_assert(INT32 expr, const PINT8 file, INT32 line);
INT32 osal_dbg_assert_aee(const PINT8 module, const PINT8 detail_description, ...);
INT32 osal_sprintf(PINT8 str, const PINT8 format, ...);
PVOID osal_malloc(UINT32 size);
VOID osal_free(const PVOID dst);
PVOID osal_memset(PVOID buf, INT32 i, UINT32 len);
PVOID osal_memcpy(PVOID dst, const PVOID src, UINT32 len);
VOID osal_memcpy_fromio(PVOID dst, const PVOID src, UINT32 len);
VOID osal_memcpy_toio(PVOID dst, const PVOID src, UINT32 len);
INT32 osal_memcmp(const PVOID buf1, const PVOID buf2, UINT32 len);

UINT16 osal_crc16(const PUINT8 buffer, const UINT32 length);
VOID osal_thread_show_stack(P_OSAL_THREAD pThread);

INT32 osal_sleep_ms(UINT32 ms);
INT32 osal_udelay(UINT32 us);
INT32 osal_usleep_range(ULONG min, ULONG max);
INT32 osal_timer_create(P_OSAL_TIMER);
INT32 osal_timer_start(P_OSAL_TIMER, UINT32);
INT32 osal_timer_stop(P_OSAL_TIMER);
INT32 osal_timer_stop_sync(P_OSAL_TIMER pTimer);
INT32 osal_timer_modify(P_OSAL_TIMER, UINT32);
INT32 osal_timer_delete(P_OSAL_TIMER);

INT32 osal_fifo_init(P_OSAL_FIFO pFifo, PUINT8 buffer, UINT32 size);
VOID osal_fifo_deinit(P_OSAL_FIFO pFifo);
INT32 osal_fifo_reset(P_OSAL_FIFO pFifo);
UINT32 osal_fifo_in(P_OSAL_FIFO pFifo, PUINT8 buffer, UINT32 size);
UINT32 osal_fifo_out(P_OSAL_FIFO pFifo, PUINT8 buffer, UINT32 size);
UINT32 osal_fifo_len(P_OSAL_FIFO pFifo);
UINT32 osal_fifo_sz(P_OSAL_FIFO pFifo);
UINT32 osal_fifo_avail(P_OSAL_FIFO pFifo);
UINT32 osal_fifo_is_empty(P_OSAL_FIFO pFifo);
UINT32 osal_fifo_is_full(P_OSAL_FIFO pFifo);

INT32 osal_wake_lock_init(P_OSAL_WAKE_LOCK plock);
INT32 osal_wake_lock(P_OSAL_WAKE_LOCK plock);
INT32 osal_wake_unlock(P_OSAL_WAKE_LOCK plock);
INT32 osal_wake_lock_count(P_OSAL_WAKE_LOCK plock);
INT32 osal_wake_lock_deinit(P_OSAL_WAKE_LOCK plock);

#if defined(CONFIG_PROVE_LOCKING)
#define osal_unsleepable_lock_init(l) { spin_lock_init(&((l)->lock)); }
#else
INT32 osal_unsleepable_lock_init(P_OSAL_UNSLEEPABLE_LOCK);
#endif
INT32 osal_lock_unsleepable_lock(P_OSAL_UNSLEEPABLE_LOCK);
INT32 osal_unlock_unsleepable_lock(P_OSAL_UNSLEEPABLE_LOCK);
INT32 osal_trylock_unsleepable_lock(P_OSAL_UNSLEEPABLE_LOCK);
INT32 osal_unsleepable_lock_deinit(P_OSAL_UNSLEEPABLE_LOCK);

#if defined(CONFIG_PROVE_LOCKING)
#define osal_sleepable_lock_init(l) { mutex_init(&((l)->lock)); }
#else
INT32 osal_sleepable_lock_init(P_OSAL_SLEEPABLE_LOCK);
#endif
INT32 osal_lock_sleepable_lock(P_OSAL_SLEEPABLE_LOCK);
INT32 osal_unlock_sleepable_lock(P_OSAL_SLEEPABLE_LOCK);
INT32 osal_trylock_sleepable_lock(P_OSAL_SLEEPABLE_LOCK);
INT32 osal_sleepable_lock_deinit(P_OSAL_SLEEPABLE_LOCK);

INT32 osal_signal_init(P_OSAL_SIGNAL);
INT32 osal_wait_for_signal(P_OSAL_SIGNAL);
INT32 osal_wait_for_signal_timeout(P_OSAL_SIGNAL, P_OSAL_THREAD);
INT32 osal_raise_signal(P_OSAL_SIGNAL);
INT32 osal_signal_active_state(P_OSAL_SIGNAL pSignal);
INT32 osal_signal_deinit(P_OSAL_SIGNAL);

INT32 osal_event_init(P_OSAL_EVENT);
INT32 osal_wait_for_event(P_OSAL_EVENT, P_COND, PVOID);
INT32 osal_wait_for_event_timeout(P_OSAL_EVENT, P_COND, PVOID);
extern INT32 osal_trigger_event(P_OSAL_EVENT);

INT32 osal_event_deinit(P_OSAL_EVENT);
LONG osal_wait_for_event_bit_set(P_OSAL_EVENT pEvent, PULONG pState, UINT32 bitOffset);
LONG osal_wait_for_event_bit_clr(P_OSAL_EVENT pEvent, PULONG pState, UINT32 bitOffset);

INT32 osal_thread_create(P_OSAL_THREAD);
INT32 osal_thread_run(P_OSAL_THREAD);
INT32 osal_thread_should_stop(P_OSAL_THREAD);
INT32 osal_thread_stop(P_OSAL_THREAD);
/*INT32 osal_thread_wait_for_event(P_OSAL_THREAD, P_OSAL_EVENT);*/
INT32 osal_thread_wait_for_event(P_OSAL_THREAD, P_OSAL_EVENT, P_OSAL_EVENT_CHECKER);
/*check pOsalLxOp and OSAL_THREAD_SHOULD_STOP*/
INT32 osal_thread_destroy(P_OSAL_THREAD);
INT32 osal_thread_sched_mark(P_OSAL_THREAD, P_OSAL_THREAD_SCHEDSTATS schedstats);
INT32 osal_thread_sched_unmark(P_OSAL_THREAD pThread, P_OSAL_THREAD_SCHEDSTATS schedstats);

INT32 osal_clear_bit(UINT32 bitOffset, P_OSAL_BIT_OP_VAR pData);
INT32 osal_set_bit(UINT32 bitOffset, P_OSAL_BIT_OP_VAR pData);
INT32 osal_test_bit(UINT32 bitOffset, P_OSAL_BIT_OP_VAR pData);
INT32 osal_test_and_clear_bit(UINT32 bitOffset, P_OSAL_BIT_OP_VAR pData);
INT32 osal_test_and_set_bit(UINT32 bitOffset, P_OSAL_BIT_OP_VAR pData);

INT32 osal_gettimeofday(PINT32 sec, PINT32 usec);
void osal_do_gettimeofday(struct timeval *tv);
INT32 osal_printtimeofday(const PUINT8 prefix);
VOID osal_get_local_time(PUINT64 sec, PULONG nsec);
UINT64 osal_elapsed_us(UINT64 ts, ULONG usec);

VOID osal_buffer_dump(const PUINT8 buf, const PUINT8 title, UINT32 len, UINT32 limit);
VOID osal_buffer_dump_data(const PUINT32 buf, const PUINT8 title, const UINT32 len, const UINT32 limit,
			   const INT32 flag);

UINT32 osal_op_get_id(P_OSAL_OP pOp);
MTK_WCN_BOOL osal_op_is_wait_for_signal(P_OSAL_OP pOp);
VOID osal_op_raise_signal(P_OSAL_OP pOp, INT32 result);
VOID osal_set_op_result(P_OSAL_OP pOp, INT32 result);
VOID osal_opq_dump(const char *qName, P_OSAL_OP_Q pOpQ);
VOID osal_opq_dump_locked(const char *qName, P_OSAL_OP_Q pOpQ);
MTK_WCN_BOOL osal_opq_has_op(P_OSAL_OP_Q pOpQ, P_OSAL_OP pOp);

INT32 osal_ftrace_print(const PINT8 str, ...);
INT32 osal_ftrace_print_ctrl(INT32 flag);

VOID osal_dump_thread_state(const PUINT8 name);
VOID osal_op_history_init(struct osal_op_history *log_history, INT32 queue_size);
VOID osal_op_history_save(struct osal_op_history *log_history, P_OSAL_OP pOp);
VOID osal_op_history_print(struct osal_op_history *log_history, PINT8 name);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#define osal_assert(condition) \
do { \
	if (!(condition)) \
		osal_err_print("%s, %d, (%s)\n", __FILE__, __LINE__, #condition); \
} while (0)

#endif /* _OSAL_H_ */
