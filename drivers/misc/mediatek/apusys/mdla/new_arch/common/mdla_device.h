/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_DEVICE_H__
#define __MDLA_DEVICE_H__

#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/list.h>
#include <linux/types.h>

#define MAX_CORE_NUM 16

struct device;
struct apusys_kmem;

struct mdla_run_cmd;
struct mdla_run_cmd_sync;
struct mdla_wait_cmd;
struct mdla_wait_entry;

struct mdla_pwr_ctrl;
struct mdla_prof_dev;
struct mdla_pmu_info;
struct mdla_scheduler;

enum REASON_ENUM {
	REASON_MDLA_SUCCESS  = 0,
	REASON_MDLA_PREMPTED = 1,
	REASON_MDLA_NULLPOINT = 2,
	REASON_MDLA_TIMEOUT  = 3,
	REASON_MDLA_POWERON  = 4,
	REASON_MDLA_RETVAL_MAX,
};

enum MDLA_RESULT {
	MDLA_SUCCESS = 0,
	MDLA_TIMEOUT = 1,
};

enum command_entry_flags {
	CE_NOP     = 0x00,
	CE_SYNC    = 0x01,
	CE_POLLING = 0x02,
};

enum command_entry_state {
	CE_NONE         = 0,
	CE_QUEUE        = 1,
	CE_DEQUE        = 2,
	CE_RUN          = 3,
	CE_PREEMPTING   = 4,
	CE_SCHED        = 5,
	CE_PREEMPTED    = 6,
	CE_RESUMED      = 7,
	CE_DONE         = 8,
	CE_FIN          = 9,
	CE_TIMEOUT      = 10,
	CE_FAIL         = 11,
	CE_SKIP         = 12,
	CE_ISSUE_ERROR1 = 13,
	CE_ISSUE_ERROR2 = 14,
	CE_ISSUE_ERROR3 = 15,
	CE_QUEUE_RESUME = 16,
};

enum interrupt_error {
	IRQ_SUCCESS                 = 0,
	IRQ_NO_SCHEDULER            = 0x1,
	IRQ_NO_PROCESSING_CE        = 0x2,
	IRQ_NO_WRONG_DEQUEUE_STATUS = 0x4,
	IRQ_TWICE                   = 0x8,
	IRQ_N_EMPTY_IN_ISSUE      = 0x10,
	IRQ_N_EMPTY_IN_SCHED      = 0x20,
	IRQ_NE_ISSUE_FIRST          = 0x40,
	IRQ_NE_SCHED_FIRST          = 0x80,
	IRQ_IN_IRQ                  = 0x100,
	IRQ_NOT_IN_IRQ              = 0x200,
	IRQ_TIMEOUT                 = 0x400,
	IRQ_RECORD_ERROR            = 0x800,
};

enum REASON_QUEUE_STATE_ENUM {
	REASON_QUEUE_NOCHANGE      = 0,
	REASON_QUEUE_NORMALEXE     = 1,
	REASON_QUEUE_PREEMPTION    = 2,
	REASON_QUEUE_NULLSCHEDULER = 3,
	REASON_QUEUE_MAX,
};

enum MDLA_STATUS_TYPE {
	MDLA_FREE      = 0x0,
	MDLA_GOTO_STOP = 0x1,
	MDLA_STOP      = 0x2,
	MDLA_RUN       = 0x3,
	MDLA_SMP_PROP  = 0x100,
};

/*mdla dev info, register to apusys callback*/
struct mdla_dev {
	u32 mdla_id;
	struct device *dev;

	u32 max_cmd_id;
	struct mutex cmd_lock;
	struct list_head cmd_list;
	struct mutex cmd_list_lock;
	struct completion command_done;
	spinlock_t hw_lock;

	/* power */
	struct mdla_pwr_ctrl *power;
	u32 cmd_list_cnt;
	bool power_is_on;
	bool sw_power_is_on;

	/* profile*/
	struct mdla_prof_dev *prof;

	/* pmu */
	struct mdla_pmu_dev *pmu_dev;
	struct mdla_pmu_info *pmu_info;

	/* platform */
	void *cmd_buf_dmp;
	size_t cmd_buf_len;
	struct mutex cmd_buf_dmp_lock;

	/* SW preemption */
	struct mdla_scheduler *sched;

	/* SMP HW preemption*/
	uint32_t status;
	spinlock_t stat_lock;

	u32 error_bit;
};

struct command_entry {
	struct list_head node;
	struct completion swcmd_done_wait;  /* the completion for CE finish */
	struct completion preempt_wait;  /* the completion for normal CE */
	int flags;
	int state;
	u32 irq_state;
	u64 footprint;
	int sync;

	void *kva;    /* Virtual Address for Kernel */
	u32 mva;      /* Physical Address for Device */
	u32 count;
	u32 csn;
	int boost_val;

	u32 bandwidth;

	int result;
	u64 receive_t;   /* time of receive the request (ns) */
	u64 queue_t;     /* time queued in list (ns) */
	u64 poweron_t;   /* dvfs start time */
	u64 req_start_t; /* request start time (ns) */
	u64 req_end_t;   /* request end time (ns) */
	u64 exec_time;   /* HW execution time (ns) */
	u64 wait_t;      /* time waited by user */
	u64 cmd_id;
	u32 multicore_total;

	u64 deadline_t;

	u32 fin_cid;         /* record the last finished command id */
	u32 wish_fin_cid;

	struct apusys_kmem *cmdbuf;
	int ctx_id;
	int (*context_callback)(int a, int b, unsigned char c);

	/* SW preemption information */
	u32 priority;
	u32 cmd_batch_size;  /* command batch size */
	bool cmd_batch_en;       /* enable command batch or not */
	struct list_head *batch_list_head;/* list of command batch */
	u32 *cmd_int_backup; /* backup MREG_CMD_TILE_CNT_INT */
	u32 *cmd_ctrl_1_backup; /* backup MREG_CMD_GENERAL_CTRL_1 */

	/* HW preemption information */
	u32 hw_sync0;
	u32 hw_sync1;
	u32 hw_sync2;
	u32 hw_sync3;
};

struct mdla_dev *mdla_get_device(int id);
void mdla_set_device(struct mdla_dev *dev, u32 num);

#endif /* __MDLA_DEVICE_H__ */
