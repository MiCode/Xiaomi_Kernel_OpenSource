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

#ifndef _CAM_WORKER_H_
#define _CAM_WORKER_H_

#include<linux/kernel.h>
#include<linux/module.h>
#include<linux/init.h>
#include<linux/sched.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/timer.h>

#include "cam_req_mgr_core.h"

/* Macros */
#define CRM_WORKQ_NUM_TASKS 30

/**enum crm_workq_task_type
 * @codes: to identify which type of task is present
 */
enum crm_workq_task_type {
	CRM_WORKQ_TASK_GET_DEV_INFO,
	CRM_WORKQ_TASK_SETUP_LINK,
	CRM_WORKQ_TASK_SCHED_REQ,
	CRM_WORKQ_TASK_DEV_ADD_REQ,
	CRM_WORKQ_TASK_APPLY_REQ,
	CRM_WORKQ_TASK_NOTIFY_SOF,
	CRM_WORKQ_TASK_NOTIFY_ACK,
	CRM_WORKQ_TASK_NOTIFY_ERR,
	CRM_WORKQ_TASK_INVALID,
};

/** struct crm_workq_task
 * @type: type of task
 * u -
 * @csl_req: contains info of  incoming reqest from CSL to CRM
 * @dev_req: contains tracking info of available req id at device
 * @apply_req: contains info of which request is applied at device
 * @notify_sof: contains notification from IFE to CRM about SOF trigger
 * @notify_err: contains error inf happened while processing request
 * @dev_info: contains info about which device is connected with CRM
 * @link_setup: contains info about new link being setup
 * -
 * @process_cb: registered callback called by workq when task enqueued is ready
 *  for processing in workq thread context
 * @parent: workq's parent is link which is enqqueing taks to this workq
 * @entry: list head of this list entry is worker's empty_head
 * @cancel: if caller has got free task from pool but wants to abort or put
 *  back without using it
 * @priv: when task is enqueuer caller can attach cookie
 */
struct crm_workq_task {
	enum crm_workq_task_type type;
	union {
		struct cam_req_mgr_sched_request csl_req;
		struct cam_req_mgr_add_request dev_req;
		struct cam_req_mgr_apply_request apply_req;
		struct cam_req_mgr_sof_notify notify_sof;
		struct cam_req_mgr_error_notify notify_err;
		struct cam_req_mgr_device_info dev_info;
		struct cam_req_mgr_core_dev_link_setup link_setup;
	} u;
	int (*process_cb)(void *, void *);
	void *parent;
	struct list_head entry;
	uint8_t cancel;
	void *priv;
};

/** struct crm_core_worker
 * @work: work token used by workqueue
 * @job: workqueue internal job struct
 *task -
 * @lock: lock for task structs
 * @pending_cnt:  num of tasks pending to be processed
 * @free_cnt:  num of free/available tasks
 * @process_head: list  head of tasks pending process
 * @empty_head: list  head of available tasks which can be used
 * or acquired in order to enqueue a task to workq
 * @pool: pool  of tasks used for handling events in workq context
 *@num_task : size of tasks pool
 */
struct cam_req_mgr_core_workq {
	struct work_struct work;
	struct workqueue_struct *job;

	struct {
		spinlock_t lock;
		atomic_t pending_cnt;
		atomic_t free_cnt;

		struct list_head process_head;
		struct list_head empty_head;
		struct crm_workq_task pool[CRM_WORKQ_NUM_TASKS];
	} task;
};

/**
 * cam_req_mgr_workq_create()
 * @brief: create a workqueue
 * @name: Name of the workque to be allocated,
 * it is combination of session handle and link handle
 * @workq: Double pointer worker
 * This function will allocate and create workqueue and pass
 * the worker pointer to caller.
 */
int cam_req_mgr_workq_create(char *name,
	struct cam_req_mgr_core_workq **workq);

/**
 * cam_req_mgr_workq_destroy()
 * @brief: destroy workqueue
 * @workq: pointer to worker data struct
 * this function will destroy workqueue and clean up resources
 * associated with worker such as tasks.
 */
void cam_req_mgr_workq_destroy(struct cam_req_mgr_core_workq *workq);

/**
 * cam_req_mgr_workq_enqueue_task()
 * @brief: Enqueue task in worker queue
 * @task: task to be processed by worker
 * process callback func
 */
int cam_req_mgr_workq_enqueue_task(struct crm_workq_task *task);

/**
 * cam_req_mgr_workq_get_task()
 * @brief: Returns empty task pointer for use
 * @workq: workque used for processing
 */
struct crm_workq_task *cam_req_mgr_workq_get_task(
	struct cam_req_mgr_core_workq *workq);

#endif
