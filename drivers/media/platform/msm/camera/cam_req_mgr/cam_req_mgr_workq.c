/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#include "cam_req_mgr_workq.h"
#include "cam_debug_util.h"

#define WORKQ_ACQUIRE_LOCK(workq, flags) {\
	if ((workq)->in_irq) \
		spin_lock_irqsave(&(workq)->lock_bh, (flags)); \
	else \
		spin_lock_bh(&(workq)->lock_bh); \
}

#define WORKQ_RELEASE_LOCK(workq, flags) {\
	if ((workq)->in_irq) \
		spin_unlock_irqrestore(&(workq)->lock_bh, (flags)); \
	else	\
		spin_unlock_bh(&(workq)->lock_bh); \
}

struct crm_workq_task *cam_req_mgr_workq_get_task(
	struct cam_req_mgr_core_workq *workq)
{
	struct crm_workq_task *task = NULL;
	unsigned long flags = 0;

	if (!workq)
		return NULL;

	WORKQ_ACQUIRE_LOCK(workq, flags);
	if (list_empty(&workq->task.empty_head))
		goto end;

	task = list_first_entry(&workq->task.empty_head,
		struct crm_workq_task, entry);
	if (task) {
		atomic_sub(1, &workq->task.free_cnt);
		list_del_init(&task->entry);
	}

end:
	WORKQ_RELEASE_LOCK(workq, flags);

	return task;
}

static void cam_req_mgr_workq_put_task(struct crm_workq_task *task)
{
	struct cam_req_mgr_core_workq *workq =
		(struct cam_req_mgr_core_workq *)task->parent;
	unsigned long flags = 0;

	task->cancel = 0;
	task->process_cb = NULL;
	task->priv = NULL;
	WORKQ_ACQUIRE_LOCK(workq, flags);
	list_add_tail(&task->entry,
		&workq->task.empty_head);
	atomic_add(1, &workq->task.free_cnt);
	WORKQ_RELEASE_LOCK(workq, flags);
}

/**
 * cam_req_mgr_process_task() - Process the enqueued task
 * @task: pointer to task workq thread shall process
 */
static int cam_req_mgr_process_task(struct crm_workq_task *task)
{
	struct cam_req_mgr_core_workq *workq = NULL;

	if (!task)
		return -EINVAL;

	workq = (struct cam_req_mgr_core_workq *)task->parent;
	if (task->process_cb)
		task->process_cb(task->priv, task->payload);
	else
		CAM_WARN(CAM_CRM, "FATAL:no task handler registered for workq");
	cam_req_mgr_workq_put_task(task);

	return 0;
}

/**
 * cam_req_mgr_process_workq() - main loop handling
 * @w: workqueue task pointer
 */
static void cam_req_mgr_process_workq(struct work_struct *w)
{
	struct cam_req_mgr_core_workq *workq = NULL;
	struct crm_workq_task         *task;
	int32_t                        i = CRM_TASK_PRIORITY_0;
	unsigned long                  flags = 0;

	if (!w) {
		CAM_ERR(CAM_CRM, "NULL task pointer can not schedule");
		return;
	}
	workq = (struct cam_req_mgr_core_workq *)
		container_of(w, struct cam_req_mgr_core_workq, work);

	while (i < CRM_TASK_PRIORITY_MAX) {
		WORKQ_ACQUIRE_LOCK(workq, flags);
		while (!list_empty(&workq->task.process_head[i])) {
			task = list_first_entry(&workq->task.process_head[i],
				struct crm_workq_task, entry);
			atomic_sub(1, &workq->task.pending_cnt);
			list_del_init(&task->entry);
			WORKQ_RELEASE_LOCK(workq, flags);
			cam_req_mgr_process_task(task);
			CAM_DBG(CAM_CRM, "processed task %pK free_cnt %d",
				task, atomic_read(&workq->task.free_cnt));
			WORKQ_ACQUIRE_LOCK(workq, flags);
		}
		WORKQ_RELEASE_LOCK(workq, flags);
		i++;
	}
}

int cam_req_mgr_workq_enqueue_task(struct crm_workq_task *task,
	void *priv, int32_t prio)
{
	int rc = 0;
	struct cam_req_mgr_core_workq *workq = NULL;
	unsigned long flags = 0;

	if (!task) {
		CAM_WARN(CAM_CRM, "NULL task pointer can not schedule");
		rc = -EINVAL;
		goto end;
	}
	workq = (struct cam_req_mgr_core_workq *)task->parent;
	if (!workq) {
		CAM_DBG(CAM_CRM, "NULL workq pointer suspect mem corruption");
		rc = -EINVAL;
		goto end;
	}

	if (task->cancel == 1) {
		cam_req_mgr_workq_put_task(task);
		CAM_WARN(CAM_CRM, "task aborted and queued back to pool");
		rc = 0;
		goto end;
	}
	task->priv = priv;
	task->priority =
		(prio < CRM_TASK_PRIORITY_MAX && prio >= CRM_TASK_PRIORITY_0)
		? prio : CRM_TASK_PRIORITY_0;

	WORKQ_ACQUIRE_LOCK(workq, flags);
		if (!workq->job) {
			rc = -EINVAL;
			WORKQ_RELEASE_LOCK(workq, flags);
			goto end;
		}

	list_add_tail(&task->entry,
		&workq->task.process_head[task->priority]);

	atomic_add(1, &workq->task.pending_cnt);
	CAM_DBG(CAM_CRM, "enq task %pK pending_cnt %d",
		task, atomic_read(&workq->task.pending_cnt));

	queue_work(workq->job, &workq->work);
	WORKQ_RELEASE_LOCK(workq, flags);
end:
	return rc;
}

int cam_req_mgr_workq_create(char *name, int32_t num_tasks,
	struct cam_req_mgr_core_workq **workq, enum crm_workq_context in_irq)
{
	int32_t i;
	struct crm_workq_task  *task;
	struct cam_req_mgr_core_workq *crm_workq = NULL;
	char buf[128] = "crm_workq-";

	if (!*workq) {
		crm_workq = (struct cam_req_mgr_core_workq *)
			kzalloc(sizeof(struct cam_req_mgr_core_workq),
			GFP_KERNEL);
		if (crm_workq == NULL)
			return -ENOMEM;

		strlcat(buf, name, sizeof(buf));
		CAM_DBG(CAM_CRM, "create workque crm_workq-%s", name);
		crm_workq->job = alloc_workqueue(buf,
			WQ_HIGHPRI | WQ_UNBOUND, 0, NULL);
		if (!crm_workq->job) {
			kfree(crm_workq);
			return -ENOMEM;
		}

		/* Workq attributes initialization */
		INIT_WORK(&crm_workq->work, cam_req_mgr_process_workq);
		spin_lock_init(&crm_workq->lock_bh);
		CAM_DBG(CAM_CRM, "LOCK_DBG workq %s lock %pK",
			name, &crm_workq->lock_bh);

		/* Task attributes initialization */
		atomic_set(&crm_workq->task.pending_cnt, 0);
		atomic_set(&crm_workq->task.free_cnt, 0);
		for (i = CRM_TASK_PRIORITY_0; i < CRM_TASK_PRIORITY_MAX; i++)
			INIT_LIST_HEAD(&crm_workq->task.process_head[i]);
		INIT_LIST_HEAD(&crm_workq->task.empty_head);
		crm_workq->in_irq = in_irq;
		crm_workq->task.num_task = num_tasks;
		crm_workq->task.pool = (struct crm_workq_task *)
			kzalloc(sizeof(struct crm_workq_task) *
				crm_workq->task.num_task,
				GFP_KERNEL);
		if (!crm_workq->task.pool) {
			CAM_WARN(CAM_CRM, "Insufficient memory %lu",
				sizeof(struct crm_workq_task) *
				crm_workq->task.num_task);
			kfree(crm_workq);
			return -ENOMEM;
		}

		for (i = 0; i < crm_workq->task.num_task; i++) {
			task = &crm_workq->task.pool[i];
			task->parent = (void *)crm_workq;
			/* Put all tasks in free pool */
			cam_req_mgr_workq_put_task(task);
		}
		*workq = crm_workq;
		CAM_DBG(CAM_CRM, "free tasks %d",
			atomic_read(&crm_workq->task.free_cnt));
	}

	return 0;
}

void cam_req_mgr_workq_destroy(struct cam_req_mgr_core_workq **crm_workq)
{
	unsigned long flags = 0;
	struct workqueue_struct   *job;
	CAM_DBG(CAM_CRM, "destroy workque %pK", crm_workq);
	if (*crm_workq) {
		WORKQ_ACQUIRE_LOCK(*crm_workq, flags);
		if ((*crm_workq)->job) {
			job = (*crm_workq)->job;
			(*crm_workq)->job = NULL;
			WORKQ_RELEASE_LOCK(*crm_workq, flags);
			cancel_work_sync(&(*crm_workq)->work);
			destroy_workqueue(job);
		} else
			WORKQ_RELEASE_LOCK(*crm_workq, flags);
		kfree((*crm_workq)->task.pool);
		kfree(*crm_workq);
		*crm_workq = NULL;
	}
}
