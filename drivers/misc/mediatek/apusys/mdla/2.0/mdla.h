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

#define DRIVER_NAME "mtk_mdla"
#define MTK_MDLA_CORE 1//move to dts latter
#ifndef CONFIG_MTK_MDLA_DEBUG
#define CONFIG_MTK_MDLA_DEBUG
#endif

//#ifndef CONFIG_MTK_MDLA_ION
//#define CONFIG_MTK_MDLA_ION //move to dts latter
//#endif

#define __APUSYS_MIDDLEWARE__ //TODO remove after APUSYS Trial run done
#define __APUSYS_MDLA_UT__ //TODO remove after UT issue fixed
#define __APUSYS_MDLA_SW_PORTING_WORKAROUND__

//#define __APUSYS_PREEMPTION__
extern unsigned long long notrace sched_clock(void);
extern int mdla_init_hw(int core, struct platform_device *pdev);
extern int mdla_uninit_hw(void);
extern long mdla_dvfs_ioctl(struct file *filp, unsigned int command,
		unsigned long arg);
extern int mdla_dvfs_cmd_end_shutdown(void);

extern void *apu_mdla_gsm_top;
extern void *apu_mdla_gsm_base;
//extern void *infracfg_ao_top;


extern u32 mdla_timeout;
extern u32 mdla_poweroff_time;
extern u32 mdla_e1_detect_timeout;
extern u32 mdla_e1_detect_count;
extern u32 mdla_max_num_core;

enum REASON_ENUM {
	REASON_OTHERS = 0,
	REASON_DRVINIT = 1,
	REASON_TIMEOUT = 2,
	REASON_POWERON = 3,
	REASON_PREEMPTION = 4,
	REASON_MAX
};

void mdla_reset_lock(int core, int ret);

enum command_entry_state {
	CE_NONE = 0,
	CE_QUEUE = 1,
	CE_RUN = 2,
	CE_DONE = 3,
	CE_FIN = 4,
};

enum command_entry_flags {
	CE_NOP = 0x00,
	CE_SYNC = 0x01,
	CE_POLLING = 0x02,
};

struct wait_entry {
	u32 async_id;
	struct list_head list;
	struct ioctl_wait_cmd wt;
};

struct command_entry {
	struct list_head node;
	struct completion done;  /* the completion for CE */
	int flags;
	int state;
	int sync;

	void *kva;    /* Virtual Address for Kernel */
	u32 mva;      /* Physical Address for Device */
	u32 count;
	//u32 id;

	//u8 boost_value;  /* dvfs boost value */
	uint32_t bandwidth;

	int result;
	u64 receive_t;   /* time of receive the request (ns) */
	u64 queue_t;     /* time queued in list (ns) */
	u64 poweron_t;   /* dvfs start time */
	u64 req_start_t; /* request stant time (ns) */
	u64 req_end_t;   /* request end time (ns) */
	u64 wait_t;      /* time waited by user */

	u32 fin_cid;         /* record the last finished command id */
	u32 cmd_batch_size;  /* the command batch size of this CE */
	bool preempted;      /* indicate that CE has been preempted or not */
};

struct mdla_wait_cmd {
	__u32 id;              /* [in] command id */
	int  result;           /* [out] success(0), timeout(1) */
	uint64_t queue_time;   /* [out] time queued in driver (ns) */
	uint64_t busy_time;    /* [out] mdla execution time (ns) */
	uint32_t bandwidth;    /* [out] mdla bandwidth */
};


struct mdla_run_cmd {
	void *kva;
	uint32_t size;
	uint32_t mva;
	__u32 offset;        /* [in] command byte offset in buf */
	__u32 count;         /* [in] # of commands */
	__u32 id;            /* [out] command id */
	//__u8 boost_value;    /* [in] dvfs boost value */
};

struct mdla_run_cmd_sync {
	struct mdla_run_cmd req;
	struct mdla_wait_cmd res;
};

#define MTK_MDLA_MAX_NUM 2 // shift to dts later

/*mdla dev info, register to apusys callback*/
struct mdla_dev {
	u32 mdlaid;
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
#ifdef __APUSYS_PREEMPTION__
	struct mdla_scheduler *scheduler;
#endif
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
extern struct mdla_reg_ctl mdla_reg_control[];
extern struct mdla_dev mdla_devices[];
extern struct mdla_irq_desc mdla_irqdesc[];

#ifdef __APUSYS_PREEMPTION__
/*
 * struct mdla_scheduler
 *
 * @completed_ce_queue:   Queue for the completed CE.
 *                        Elements should be removed before returning to the
 *                        user-space caller.
 * @processing_ce:        Pointer to the CE that is under processing.
 *                        This pointer would be updated on:
 *                        1. dequeueing a CE from ce_queue
 *                        2. CE completed
 *                        3. CE timeout
 *                        Pinter should be NULL if there is no incoming CEs.
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
 *                        The implementation should includes:
 *                        1. set the HW RGs for execution on the batch of
 *                        commands or tiles.
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
 *                        1. move the CE to completed_ce_queue
 *                        2. set processing_ce to NULL for the next CE
 *                        3. complete the processing_ce->done to notify
 * @preempt_ce:           Pointer to function for handling preemption for HW
 *                        engine.
 *                        The implementation should includes:
 *                        1. update the HW RGs on preemption. For example,
 *                        that might be resetting HW engine or clearing status.
 * @all_ce_done:          Pointer to function for the callback on all CEs done.
 *                        The implementation should includes:
 *                        1. let the HW engine go into a low power-consuming
 *                        state. For example, that might be shutting down the
 *                        HW engine, or downgrading frequency and voltage.
 */
struct mdla_scheduler {
	struct list_head ce_queue;
	struct list_head completed_ce_queue;
	struct command_entry *processing_ce;
	spinlock_t lock;

	void (*enqueue_ce)(unsigned int core_id, struct command_entry *ce);
	void (*dequeue_ce)(unsigned int core_id);
	void (*issue_ce)(unsigned int core_id);
	unsigned int (*process_ce)(unsigned int core_id);
	void (*complete_ce)(unsigned int core_id);
	void (*preempt_ce)(unsigned int core_id);
	// FIXME: void (*all_ce_done)(unsigned int core_id);
	void (*all_ce_done)(void);
};

void mdla_dequeue_ce(unsigned int core_id);
void mdla_enqueue_ce(unsigned int core_id, struct command_entry *ce);
unsigned int mdla_process_ce(unsigned int core_id);
void mdla_issue_ce(unsigned int core_id);
void mdla_complete_ce(unsigned int core_id);
void mdla_preempt_ce(unsigned int core_id);
void mdla_command_done(int core_id);
#endif // __APUSYS_PREEMPTION__

#endif

