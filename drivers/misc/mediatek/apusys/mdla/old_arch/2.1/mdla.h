/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef __MDLA_H__
#define __MDLA_H__

#include <linux/types.h>
#include <linux/seq_file.h>
#include <linux/of_platform.h>
#include <linux/interrupt.h>
#include <linux/spinlock_types.h>
#include "mdla_ioctl.h"
#include "mdla_plat_setting.h"

#define DRIVER_NAME "mtk_mdla"

extern unsigned long long notrace sched_clock(void);
extern int mdla_init_hw(int core, struct platform_device *pdev);
extern int mdla_uninit_hw(void);
extern long mdla_dvfs_ioctl(struct file *filp, unsigned int command,
		unsigned long arg);
extern int mdla_dvfs_cmd_end_shutdown(void);
extern void mdla_wait_command(struct ioctl_wait_cmd *wt);

extern void *apu_mdla_gsm_top;
extern void *apu_mdla_gsm_base;
//extern void *infracfg_ao_top;


extern u32 mdla_timeout;
extern u32 mdla_poweroff_time;
extern u32 mdla_e1_detect_timeout;
extern u32 mdla_max_num_core;
extern u32 mdla_dvfs_rand;
extern u32 mdla_timeout_dbg;


enum FUNC_FOOT_PRINT {
	/*
	 * F_ENQUEUE: for enqueue_ce, val = (F_ENQUEUE|resume)
	 *      0x10: first add in list
	 *      0x11: add in list because preemption occur
	 */
	F_ENQUEUE               = 0x10,
	F_DEQUEUE               = 0x20,
	/*
	 * F_ISSUE:
	 *    0x30: Success
	 *    0x31: Issue cdma1 fail
	 *    0x32: Issue cdma2 fail
	 *    0x33: Issue cmd fail because HW queue is not empty and in irq
	 *    0x34: Issue cmd fail because HW queue is not empty and not in irq
	 */
	F_ISSUE                 = 0x30,
	F_CMDDONE_CE_PASS       = 0x40,
	F_CMDDONE_GO_TO_ISSUE   = 0x41,
	F_CMDDONE_CE_FIN3ERROR  = 0x42,

	F_COMPLETE              = 0x50,

	/*
	 * F_TIMEOUT:
	 *      0x60: cmd timeout and enter irq
	 *      0x61: others timeout and enter irq
	 */
	F_TIMEOUT               = 0x60,
	F_INIRQ_PASS            = 0x70,
	F_INIRQ_ERROR           = 0x71,
	F_INIRQ_CDMA4ERROR      = 0x72,
};

#define ce_func_trace(ce, val) \
	ce->footprint = (ce->footprint << 8) | val

enum CMD_MODE {
	NORMAL = 0,
	PER_CMD = 1,
	INTERRUPT = 2,
	CMD_MODE_MAX
};

enum REASON_ENUM {
	REASON_MDLA_SUCCESS  = 0,
	REASON_MDLA_PREMPTED = 1,
	REASON_MDLA_NULLPOINT = 2,
	REASON_MDLA_TIMEOUT  = 3,
	REASON_MDLA_POWERON  = 4,
	REASON_MDLA_RETVAL_MAX,
};

enum REASON_MDLA_RETVAL_ENUM {
	REASON_OTHERS    = 0,
	REASON_DRVINIT   = 1,
	REASON_TIMEOUT   = 2,
	REASON_POWERON   = 3,
	REASON_PREEMPTED = 4,
	REASON_MAX,
};

void mdla_reset_lock(unsigned int core, int ret);

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

struct wait_entry {
	u32 async_id;
	struct list_head list;
	struct ioctl_wait_cmd wt;
};

struct command_batch {
	struct list_head node;
	u32 index;
	u32 size;
};

struct preempted_context {
	struct list_head *preempted_context_list; /* Store batch_list_head */
};

struct command_entry {
	struct list_head node;
	struct completion swcmd_done_wait;  /* the completion for CE finish */
	struct completion preempt_wait;  /* the completion for normal CE */
	int flags;
	u32 state;
	u32 irq_state;
	uint64_t footprint;
	int sync;

	void *kva;    /* Virtual Address for Kernel */
	u32 mva;      /* Physical Address for Device */
	u32 count;
	/* cmd series number: use in cdma3, event id */
	uint32_t csn;
	//u32 id;
	int boost_val;

	uint32_t bandwidth;

	int result;
	u64 receive_t;   /* time of receive the request (ns) */
	u64 queue_t;     /* time queued in list (ns) */
	u64 poweron_t;   /* dvfs start time */
	u64 req_start_t; /* request stant time (ns) */
	u64 req_end_t;   /* request end time (ns) */
	u64 wait_t;      /* time waited by user */
	uint64_t cmd_id;
	uint32_t multicore_total; // how many cores to exec this subcmd

	__u64 deadline_t;

	u32 fin_cid;         /* record the last finished command id */
	u32 wish_fin_cid;
	u32 cmd_batch_size;  /* command batch size */
	u16 priority;       /* enable command batch or not */
	struct list_head *batch_list_head;/* list of command batch */
	//struct command_batch *batch_list;	/* list of command batch */
	struct apusys_kmem *cmdbuf;
	int ctx_id;
	int (*context_callback)(int a, int b, uint8_t c);
	u32 *cmd_int_backup; /* backup MREG_CMD_TILE_CNT_INT */
	u32 *cmd_ctrl_1_backup; /* backup MREG_CMD_GENERAL_CTRL_1 */
};

struct mdla_wait_cmd {
	__u32 id;              /* [in] command id */
	int  result;           /* [out] success(0), timeout(1) */
	uint64_t queue_time;   /* [out] time queued in driver (ns) */
	uint64_t busy_time;    /* [out] mdla execution time (ns) */
	uint32_t bandwidth;    /* [out] mdla bandwidth */
};

struct mdla_run_cmd {
	uint32_t offset_code_buf;
	uint32_t reserved;
	uint32_t size;
	uint32_t mva;
	__u32 offset;        /* [in] command byte offset in buf */
	__u32 count;         /* [in] # of commands */
	__u32 id;            /* [out] command id */
};

struct mdla_run_cmd_sync {
	struct mdla_run_cmd req;
	struct mdla_wait_cmd res;
	__u32 mdla_id;
};

struct mdla_pmu_info {
	u64 cmd_id;
	u64 PMU_res_buf_addr0;
	u64 PMU_res_buf_addr1;
	u8 pmu_mode;
	struct mdla_pmu_hnd *pmu_hnd;
};

/*mdla dev info, register to apusys callback*/
struct mdla_dev {
	u32 mdlaid;
	u32 mdla_zero_skip_count;
	u32 mdla_e1_detect_count;
	u32 async_cmd_id;
	u32 max_cmd_id;
	struct mutex cmd_lock;
	struct mutex cmd_list_lock;
	struct mutex power_lock;
	struct completion command_done;
	u32 last_reset_id;
	spinlock_t hw_lock;
	struct timer_list power_timer;
	struct work_struct power_off_work;
	void (*power_pdn_work)(struct work_struct *work);
	int mdla_power_status;
	int mdla_sw_power_status;
	u32 cmd_list_cnt;
	struct mdla_scheduler *sched; //scheduler
	u32 error_bit;
	struct mdla_pmu_info pmu[PRIORITY_LEVEL];
	void *cmd_buf_dmp;
	u32 cmd_buf_len;
	struct mutex cmd_buf_dmp_lock;
	struct hrtimer hr_timer;
	u8 timer_started;
};

struct mdla_irq_desc {
	unsigned int irq;
	irq_handler_t handler;
};

struct mdla_reg_ctl {
	void *apu_mdla_cmde_mreg_top;
	void *apu_mdla_biu_top;
	void *apu_mdla_config_top;
};

struct smp_deadline {
	spinlock_t lock;
	u64 deadline;
};

extern struct smp_deadline g_smp_deadline[PRIORITY_LEVEL];
extern struct mdla_reg_ctl mdla_reg_control[];
extern struct mdla_dev mdla_devices[];
extern struct mdla_irq_desc mdla_irqdesc[];

extern struct mutex wake_lock_mutex;

/*
 * @ worker: record All MDLA HW state
 *           bit 0: MDLA0 has normal work
 *           bit 1: MDLA0 has priority work
 *           bit 2: MDLA1 has normal work
 *           bit 3: MDLA1 has priority work
 *
 * @ worker_lock: protect worker variable
 */
struct mdla_dev_worker {
	u8 worker;
	struct mutex worker_lock;
};
extern struct mdla_dev_worker mdla_dev_workers;
/*
 * struct mdla_scheduler
 *
 * @ce_list:              Queue for the active CEs, which would be issued when
 *                        HW engine is available.
 * @processing_ce:        Pointer to the CE that is under processing.
 *                        This pointer would be updated on:
 *                        1. dequeueing a CE from ce_queue
 *                        2. CE completed
 *                        3. CE timeout
 *                        Pinter should be NULL if there is no incoming CEs.
 *
 * @lock:                 Lock to protect the scheduler elements.
 *
 * @enqueue_ce:           Pointer to function for enqueueing a CE to ce_queue.
 *                        The implementation should includes:
 *                        1. move the CE into its ce_queue
 *                        2. start the HW engine if processing_ce is NULL
 * @dequeue_ce:           Pointer to function for dequeueing a CE from ce_queue.
 *                        The preemption of CE might be taken in this procedure.
 *                        The implementation should includes:
 *                        1. search the next CE to be issued
 *                        2. if processing_ce is not NULL, callee must handle
 *                        the preemption of CE
 *                        3. update processing_ce to the next CE, and move
 *                        the previous processing one to ce_queue
 * @issue_ce:             Pointer to function for issuing a batch of the CE
 *                        to HW engine.
 *                        The implementation should include:
 *                        1. set the HW RGs for execution on the batch of
 *                        commands.
 *                        2. not only handle the normal case, but also the
 *                        preemption case.
 *                        3. callee could integrate power-on flow in this
 *                        procedure, yet the reentrant issue should be taken
 *                        into consideration.
 * @process_ce:           Pointer to function for processing the CE when
 *                        the scheduler receives a interrupt from HW engine
 *                        The implementation should includes:
 *                        1. read the engine status from HW RGs, and do proper
 *                        operation based on the status
 *                        2. check whether this command batch completed or not
 *                        The return value should be one of:
 *                        1. CE_DONE if all the batches in the CE completed
 *                        2. CE_RUN if a batch completed
 *                        3. CE_NONE if the batch is still under processing
 * @complete_ce:          Pointer to function for completing the CE.
 *                        The implementation should includes:
 *                        1. set the status of processing_ce to CE_FIN
 *                        2. set processing_ce to NULL for the next CE
 *                        3. complete the processing_ce->done to notify
 */
struct mdla_scheduler {
	struct list_head ce_list[PRIORITY_LEVEL];
	struct command_entry *ce[PRIORITY_LEVEL];
	struct command_entry *pro_ce;

	spinlock_t lock;

	void (*enqueue_ce)(
		unsigned int core_id,
		struct command_entry *ce,
		uint32_t resume);
	struct command_entry * (*dequeue_ce)(unsigned int core_id);
	void (*issue_ce)(unsigned int core_id);
	void (*issue_dual_lowce)(
		unsigned int core_id,
		uint64_t dual_cmd_id);
	unsigned int (*process_ce)(unsigned int core_id);
	void (*complete_ce)(unsigned int core_id);
	void (*preempt_ce)(unsigned int core_id, struct command_entry *high_ce);
};

enum REASON_QUEUE_STATE_ENUM {
	REASON_QUEUE_NOCHANGE      = 0,
	REASON_QUEUE_NORMALEXE     = 1,
	REASON_QUEUE_PREEMPTION    = 2,
	REASON_QUEUE_NULLSCHEDULER = 3,
	REASON_QUEUE_MAX,
};

struct command_entry *mdla_dequeue_ce_2_1(unsigned int core_id);

void mdla_enqueue_ce_2_1(
	unsigned int core_id,
	struct command_entry *ce,
	uint32_t resume);

unsigned int mdla_process_ce_2_1(unsigned int core_id);

void mdla_issue_ce_2_1(unsigned int core_id);

void mdla_complete_ce_2_1(unsigned int core_id);

void mdla_preempt_ce_2_1(
	unsigned int core_id,
	struct command_entry *high_ce);

void mdla_normal_dual_cmd_issue(
	unsigned int core_id,
	uint64_t dual_cmd_id);

irqreturn_t mdla_scheduler_2_1(unsigned int core_id);
#endif

