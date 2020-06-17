/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_SCHEDULER_H__
#define __MDLA_SCHEDULER_H__

#include <linux/spinlock.h>
#include <linux/list.h>

struct command_entry;

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
	struct list_head active_ce_queue;
	struct command_entry *pro_ce_normal;
	struct command_entry *pro_ce_high;
	struct command_entry *pro_ce;

	spinlock_t lock;

	void (*enqueue_ce)(unsigned int core_id, struct command_entry *ce);
	unsigned int (*dequeue_ce)(unsigned int core_id);
	void (*issue_ce)(unsigned int core_id);
	unsigned int (*process_ce)(unsigned int core_id);
	void (*complete_ce)(unsigned int core_id);
};

/* platform callback functions */
struct mdla_sched_cb_func {
	void (*split_alloc_cmd_batch)(struct command_entry *ce);
	void (*del_free_cmd_batch)(struct command_entry *ce);
};

struct mdla_sched_cb_func *mdla_sched_plat_cb(void);

#endif /* __MDLA_SCHEDULER_H__ */

