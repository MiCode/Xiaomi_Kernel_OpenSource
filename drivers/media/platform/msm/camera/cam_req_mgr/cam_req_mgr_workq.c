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

#include "cam_req_mgr_workq.h"

/* workqueue's task manager methods */
struct crm_workq_task *cam_req_mgr_workq_get_task(
	struct cam_req_mgr_core_workq *workq)
{
	struct crm_workq_task *task = NULL;

	if (!workq)
		return NULL;

	spin_lock(&workq->task.lock);
	task = list_first_entry(&workq->task.empty_head,
		struct crm_workq_task, entry);
	if (task) {
		atomic_sub(1, &workq->task.free_cnt);
		list_del_init(&task->entry);
	}
	spin_unlock(&workq->task.lock);

	return task;
}

static void cam_req_mgr_workq_put_task(struct crm_workq_task *task)
{
	struct cam_req_mgr_core_workq *workq =
		(struct cam_req_mgr_core_workq *)task->parent;

	task->cancel = 0;
	task->process_cb = NULL;
	task->priv = NULL;
	list_add_tail(&task->entry,
		&workq->task.empty_head);
	atomic_add(1, &workq->task.free_cnt);
}

/**
 * cam_req_mgr_process_task() - Process the enqueued task
 * @task: pointer to task worker thread shall process
 */
static int cam_req_mgr_process_task(struct crm_workq_task *task)
{
	struct cam_req_mgr_core_workq *workq = NULL;

	if (!task)
		return -EINVAL;

	workq = (struct cam_req_mgr_core_workq *)task->parent;

	switch (task->type) {
	case CRM_WORKQ_TASK_SCHED_REQ:
	case CRM_WORKQ_TASK_DEV_ADD_REQ:
	case CRM_WORKQ_TASK_NOTIFY_SOF:
	case CRM_WORKQ_TASK_NOTIFY_ACK:
	case CRM_WORKQ_TASK_NOTIFY_ERR:
		if (task->process_cb)
			task->process_cb(task->priv, &task->u);
		else
			CRM_WARN("FATAL:no task handler registered for workq!");
		break;
	case CRM_WORKQ_TASK_GET_DEV_INFO:
	case CRM_WORKQ_TASK_SETUP_LINK:
	case CRM_WORKQ_TASK_APPLY_REQ:
		/* These tasks are not expected to be queued to
		 * workque at the present
		 */
		CRM_DBG("Not supported");
		break;
	case CRM_WORKQ_TASK_INVALID:
	default:
		CRM_ERR("Invalid task type %x", task->type);
		break;
	}
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
	struct crm_workq_task *task, *task_save;

	if (!w) {
		CRM_ERR("NULL task pointer can not schedule");
		return;
	}
	workq = (struct cam_req_mgr_core_workq *)
		container_of(w, struct cam_req_mgr_core_workq, work);

	spin_lock(&workq->task.lock);
	list_for_each_entry_safe(task, task_save,
		&workq->task.process_head, entry) {
		atomic_sub(1, &workq->task.pending_cnt);
		list_del_init(&task->entry);
		cam_req_mgr_process_task(task);
	}
	spin_unlock(&workq->task.lock);
	CRM_DBG("processed task %p free_cnt %d",
		task, atomic_read(&workq->task.free_cnt));
}

int cam_req_mgr_workq_enqueue_task(struct crm_workq_task *task)
{
	int rc = 0;
	struct cam_req_mgr_core_workq *workq = NULL;

	if (!task) {
		CRM_WARN("NULL task pointer can not schedule");
		rc = -EINVAL;
		goto end;
	}
	workq = (struct cam_req_mgr_core_workq *)task->parent;
	if (!workq) {
		CRM_WARN("NULL worker pointer suspect mem corruption");
		rc = -EINVAL;
		goto end;
	}
	if (!workq->job) {
		CRM_WARN("NULL worker pointer suspect mem corruption");
		rc = -EINVAL;
		goto end;
	}

	spin_lock(&workq->task.lock);
	if (task->cancel == 1) {
		cam_req_mgr_workq_put_task(task);
		CRM_WARN("task aborted and queued back to pool");
		rc = 0;
		spin_unlock(&workq->task.lock);
		goto end;
	}
	list_add_tail(&task->entry,
		&workq->task.process_head);
	atomic_add(1, &workq->task.pending_cnt);
	CRM_DBG("enq task %p pending_cnt %d",
		task, atomic_read(&workq->task.pending_cnt));
	spin_unlock(&workq->task.lock);

	queue_work(workq->job, &workq->work);

end:
	return rc;
}

int cam_req_mgr_workq_create(char *name, struct cam_req_mgr_core_workq **workq)
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
		CRM_DBG("create workque crm_workq-%s", name);
		crm_workq->job = alloc_workqueue(buf,
			WQ_HIGHPRI | WQ_UNBOUND, 0, NULL);
		if (!crm_workq->job) {
			kfree(crm_workq);
			return -ENOMEM;
		}

		/* Workq attributes initialization */
		INIT_WORK(&crm_workq->work, cam_req_mgr_process_workq);

		/* Task attributes initialization */
		spin_lock_init(&crm_workq->task.lock);
		atomic_set(&crm_workq->task.pending_cnt, 0);
		atomic_set(&crm_workq->task.free_cnt, 0);
		INIT_LIST_HEAD(&crm_workq->task.process_head);
		INIT_LIST_HEAD(&crm_workq->task.empty_head);
		memset(crm_workq->task.pool, 0,
			sizeof(struct crm_workq_task) *
			CRM_WORKQ_NUM_TASKS);
		for (i = 0; i < CRM_WORKQ_NUM_TASKS; i++) {
			task = &crm_workq->task.pool[i];
			task->parent = (void *)crm_workq;
			/* Put all tasks in free pool */
			cam_req_mgr_workq_put_task(task);
		}
		*workq = crm_workq;
		CRM_DBG("free tasks %d",
			atomic_read(&crm_workq->task.free_cnt));
	}

	return 0;
}

void cam_req_mgr_workq_destroy(struct cam_req_mgr_core_workq *crm_workq)
{
	CRM_DBG("destroy workque %p", crm_workq);
	if (crm_workq) {
		if (crm_workq->job) {
			destroy_workqueue(crm_workq->job);
			crm_workq->job = NULL;
		}
		kfree(crm_workq);
		crm_workq = NULL;
	}
}
