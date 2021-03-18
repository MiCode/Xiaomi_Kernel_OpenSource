/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_CMD_DATA_V1_X_H__
#define __MDLA_CMD_DATA_V1_X_H__

#include <linux/types.h>
#include <linux/list.h>

/* Get priority level */
#include <utilities/mdla_util.h>

struct command_entry;

struct mdla_wait_cmd {
	uint32_t id;           /* [in] command id */
	int32_t  result;       /* [out] success(0), timeout(1) */
	uint64_t queue_time;   /* [out] time queued in driver (ns) */
	uint64_t busy_time;    /* [out] mdla execution time (ns) */
	uint32_t bandwidth;    /* [out] mdla bandwidth */
};

struct mdla_run_cmd {
	uint32_t offset_code_buf;
	uint32_t reserved;
	uint32_t size;
	uint32_t mva;
	uint32_t offset;        /* [in] command byte offset in buf */
	uint32_t count;         /* [in] # of commands */
	uint32_t id;            /* [out] command id */
};

struct mdla_run_cmd_sync {
	struct mdla_run_cmd req;
	struct mdla_wait_cmd res;
	uint32_t mdla_id;
};

struct mdla_wait_entry {
	uint32_t async_id;
	struct list_head list;
	struct mdla_wait_cmd wt;
};

/*
 * struct mdla_scheduler
 *
 * @active_ce_queue:      Queue for the active CEs, which would be issued when
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

	void (*enqueue_ce)(u32 core_id, struct command_entry *ce, u32 resume);
	struct command_entry* (*dequeue_ce)(u32 core_id);
	void (*issue_ce)(u32 core_id);
	void (*issue_dual_lowce)(u32 core_id, uint64_t dual_cmd_id);
	int (*process_ce)(u32 core_id);
	void (*complete_ce)(u32 core_id);
	void (*preempt_ce)(u32 core_id, struct command_entry *high_ce);
	u64 (*get_smp_deadline)(int priority);
	void (*set_smp_deadline)(int priority, u64 deadline);
};

#endif /* __MDLA_CMD_DATA_V1_X_H__ */

