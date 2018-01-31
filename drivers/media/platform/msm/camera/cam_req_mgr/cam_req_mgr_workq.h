/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _CAM_REQ_MGR_WORKQ_H_
#define _CAM_REQ_MGR_WORKQ_H_

#include<linux/kernel.h>
#include<linux/module.h>
#include<linux/init.h>
#include<linux/sched.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/timer.h>

#include "cam_req_mgr_core.h"

/* Task priorities, lower the number higher the priority*/
enum crm_task_priority {
	CRM_TASK_PRIORITY_0,
	CRM_TASK_PRIORITY_1,
	CRM_TASK_PRIORITY_MAX,
};

/* workqueue will be used from irq context or not */
enum crm_workq_context {
	CRM_WORKQ_USAGE_NON_IRQ,
	CRM_WORKQ_USAGE_IRQ,
	CRM_WORKQ_USAGE_INVALID,
};

/** struct crm_workq_task
 * @priority   : caller can assign priority to task based on type.
 * @payload    : depending of user of task this payload type will change
 * @process_cb : registered callback called by workq when task enqueued is
 *               ready for processing in workq thread context
 * @parent     : workq's parent is link which is enqqueing taks to this workq
 * @entry      : list head of this list entry is worker's empty_head
 * @cancel     : if caller has got free task from pool but wants to abort
 *               or put back without using it
 * @priv       : when task is enqueuer caller can attach priv along which
 *               it will get in process callback
 * @ret        : return value in future to use for blocking calls
 */
struct crm_workq_task {
	int32_t                  priority;
	void                    *payload;
	int32_t                (*process_cb)(void *, void *);
	void                    *parent;
	struct list_head         entry;
	uint8_t                  cancel;
	void                    *priv;
	int32_t                  ret;
};

/** struct cam_req_mgr_core_workq
 * @work       : work token used by workqueue
 * @job        : workqueue internal job struct
 * task -
 * @lock_bh    : lock for task structs
 * @in_irq     : set true if workque can be used in irq context
 * @free_cnt   : num of free/available tasks
 * @empty_head : list  head of available taska which can be used
 *               or acquired in order to enqueue a task to workq
 * @pool       : pool of tasks used for handling events in workq context
 * @num_task   : size of tasks pool
 * -
 */
struct cam_req_mgr_core_workq {
	struct work_struct         work;
	struct workqueue_struct   *job;
	spinlock_t                 lock_bh;
	uint32_t                   in_irq;

	/* tasks */
	struct {
		struct mutex           lock;
		atomic_t               pending_cnt;
		atomic_t               free_cnt;

		struct list_head       process_head[CRM_TASK_PRIORITY_MAX];
		struct list_head       empty_head;
		struct crm_workq_task *pool;
		uint32_t               num_task;
	} task;
};

/**
 * cam_req_mgr_workq_create()
 * @brief    : create a workqueue
 * @name     : Name of the workque to be allocated, it is combination
 *             of session handle and link handle
 * @num_task : Num_tasks to be allocated for workq
 * @workq    : Double pointer worker
 * @in_irq   : Set to one if workq might be used in irq context
 * This function will allocate and create workqueue and pass
 * the workq pointer to caller.
 */
int cam_req_mgr_workq_create(char *name, int32_t num_tasks,
	struct cam_req_mgr_core_workq **workq, enum crm_workq_context in_irq);

/**
 * cam_req_mgr_workq_destroy()
 * @brief: destroy workqueue
 * @workq: pointer to worker data struct
 * this function will destroy workqueue and clean up resources
 * associated with worker such as tasks.
 */
void cam_req_mgr_workq_destroy(struct cam_req_mgr_core_workq **workq);

/**
 * cam_req_mgr_workq_enqueue_task()
 * @brief: Enqueue task in worker queue
 * @task : task to be processed by worker
 * @priv : clients private data
 * @prio : task priority
 * process callback func
 */
int cam_req_mgr_workq_enqueue_task(struct crm_workq_task *task,
	void *priv, int32_t prio);

/**
 * cam_req_mgr_workq_get_task()
 * @brief: Returns empty task pointer for use
 * @workq: workque used for processing
 */
struct crm_workq_task *cam_req_mgr_workq_get_task(
	struct cam_req_mgr_core_workq *workq);

#endif
