/* Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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

#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/ratelimit.h>
#include "cam_tasklet_util.h"
#include "cam_irq_controller.h"
#include "cam_debug_util.h"

#define CAM_TASKLETQ_SIZE              256

static void cam_tasklet_action(unsigned long data);

/**
 * struct cam_tasklet_queue_cmd:
 * @Brief:                  Structure associated with each slot in the
 *                          tasklet queue
 *
 * @list:                   list_head member for each entry in queue
 * @payload:                Payload structure for the event. This will be
 *                          passed to the handler function
 * @bottom_half_handler:    Function pointer for event handler in bottom
 *                          half context
 *
 */
struct cam_tasklet_queue_cmd {
	struct list_head                   list;
	void                              *payload;
	CAM_IRQ_HANDLER_BOTTOM_HALF        bottom_half_handler;
};

/**
 * struct cam_tasklet_info:
 * @Brief:                  Tasklet private structure
 *
 * @list:                   list_head member for each tasklet
 * @index:                  Instance id for the tasklet
 * @tasklet_lock:           Spin lock
 * @tasklet_active:         Atomic variable to control tasklet state
 * @tasklet:                Tasklet structure used to schedule bottom half
 * @free_cmd_list:          List of free tasklet queue cmd for use
 * @used_cmd_list:          List of used tasklet queue cmd
 * @cmd_queue:              Array of tasklet cmd for storage
 * @ctx_priv:               Private data passed to the handling function
 *
 */
struct cam_tasklet_info {
	struct list_head                   list;
	uint32_t                           index;
	spinlock_t                         tasklet_lock;
	atomic_t                           tasklet_active;
	struct tasklet_struct              tasklet;

	struct list_head                   free_cmd_list;
	struct list_head                   used_cmd_list;
	struct cam_tasklet_queue_cmd       cmd_queue[CAM_TASKLETQ_SIZE];

	void                              *ctx_priv;
};

struct cam_irq_bh_api tasklet_bh_api = {
	.bottom_half_enqueue_func = cam_tasklet_enqueue_cmd,
	.get_bh_payload_func = cam_tasklet_get_cmd,
	.put_bh_payload_func = cam_tasklet_put_cmd,
};

int cam_tasklet_get_cmd(
	void                         *bottom_half,
	void                        **bh_cmd)
{
	int           rc = 0;
	unsigned long flags;
	struct cam_tasklet_info        *tasklet = bottom_half;
	struct cam_tasklet_queue_cmd   *tasklet_cmd = NULL;

	*bh_cmd = NULL;

	if (tasklet == NULL) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "tasklet is NULL");
		return -EINVAL;
	}

	if (!atomic_read(&tasklet->tasklet_active)) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Tasklet is not active");
		rc = -EPIPE;
		return rc;
	}

	spin_lock_irqsave(&tasklet->tasklet_lock, flags);
	if (list_empty(&tasklet->free_cmd_list)) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "No more free tasklet cmd");
		rc = -ENODEV;
		goto spin_unlock;
	} else {
		tasklet_cmd = list_first_entry(&tasklet->free_cmd_list,
			struct cam_tasklet_queue_cmd, list);
		list_del_init(&(tasklet_cmd)->list);
		*bh_cmd = tasklet_cmd;
	}

spin_unlock:
	spin_unlock_irqrestore(&tasklet->tasklet_lock, flags);
	return rc;
}

void cam_tasklet_put_cmd(
	void                         *bottom_half,
	void                        **bh_cmd)
{
	unsigned long flags;
	struct cam_tasklet_info        *tasklet = bottom_half;
	struct cam_tasklet_queue_cmd   *tasklet_cmd = *bh_cmd;

	if (tasklet == NULL) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "tasklet is NULL");
		return;
	}

	if (tasklet_cmd == NULL) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Invalid tasklet_cmd");
		return;
	}

	spin_lock_irqsave(&tasklet->tasklet_lock, flags);
	list_del_init(&tasklet_cmd->list);
	list_add_tail(&tasklet_cmd->list, &tasklet->free_cmd_list);
	*bh_cmd = NULL;
	spin_unlock_irqrestore(&tasklet->tasklet_lock, flags);
}

/**
 * cam_tasklet_dequeue_cmd()
 *
 * @brief:              Initialize the tasklet info structure
 *
 * @hw_mgr_ctx:         Private Ctx data that will be passed to the handler
 *                      function
 * @idx:                Index of tasklet used as identity
 * @tasklet_action:     Tasklet callback function that will be called
 *                      when tasklet runs on CPU
 *
 * @return:             0: Success
 *                      Negative: Failure
 */
static int cam_tasklet_dequeue_cmd(
	struct cam_tasklet_info        *tasklet,
	struct cam_tasklet_queue_cmd  **tasklet_cmd)
{
	int rc = 0;
	unsigned long flags;

	*tasklet_cmd = NULL;

	CAM_DBG(CAM_ISP, "Dequeue before lock.");
	spin_lock_irqsave(&tasklet->tasklet_lock, flags);
	if (list_empty(&tasklet->used_cmd_list)) {
		CAM_DBG(CAM_ISP, "End of list reached. Exit");
		rc = -ENODEV;
		goto spin_unlock;
	} else {
		*tasklet_cmd = list_first_entry(&tasklet->used_cmd_list,
			struct cam_tasklet_queue_cmd, list);
		list_del_init(&(*tasklet_cmd)->list);
		CAM_DBG(CAM_ISP, "Dequeue Successful");
	}

spin_unlock:
	spin_unlock_irqrestore(&tasklet->tasklet_lock, flags);
	return rc;
}

void cam_tasklet_enqueue_cmd(
	void                              *bottom_half,
	void                              *bh_cmd,
	void                              *handler_priv,
	void                              *evt_payload_priv,
	CAM_IRQ_HANDLER_BOTTOM_HALF        bottom_half_handler)
{
	unsigned long                  flags;
	struct cam_tasklet_queue_cmd  *tasklet_cmd = bh_cmd;
	struct cam_tasklet_info       *tasklet = bottom_half;

	if (!bottom_half) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "NULL bottom half");
		return;
	}

	if (!bh_cmd) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "NULL bh cmd");
		return;
	}

	if (!atomic_read(&tasklet->tasklet_active)) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Tasklet is not active\n");
		return;
	}

	CAM_DBG(CAM_ISP, "Enqueue tasklet cmd");
	tasklet_cmd->bottom_half_handler = bottom_half_handler;
	tasklet_cmd->payload = evt_payload_priv;
	spin_lock_irqsave(&tasklet->tasklet_lock, flags);
	list_add_tail(&tasklet_cmd->list,
		&tasklet->used_cmd_list);
	spin_unlock_irqrestore(&tasklet->tasklet_lock, flags);
	tasklet_hi_schedule(&tasklet->tasklet);
}

int cam_tasklet_init(
	void                    **tasklet_info,
	void                     *hw_mgr_ctx,
	uint32_t                  idx)
{
	int i;
	struct cam_tasklet_info  *tasklet = NULL;

	tasklet = kzalloc(sizeof(struct cam_tasklet_info), GFP_KERNEL);
	if (!tasklet) {
		CAM_DBG(CAM_ISP,
			"Error! Unable to allocate memory for tasklet");
		*tasklet_info = NULL;
		return -ENOMEM;
	}

	tasklet->ctx_priv = hw_mgr_ctx;
	tasklet->index = idx;
	spin_lock_init(&tasklet->tasklet_lock);
	memset(tasklet->cmd_queue, 0, sizeof(tasklet->cmd_queue));
	INIT_LIST_HEAD(&tasklet->free_cmd_list);
	INIT_LIST_HEAD(&tasklet->used_cmd_list);
	for (i = 0; i < CAM_TASKLETQ_SIZE; i++) {
		INIT_LIST_HEAD(&tasklet->cmd_queue[i].list);
		list_add_tail(&tasklet->cmd_queue[i].list,
			&tasklet->free_cmd_list);
	}
	tasklet_init(&tasklet->tasklet, cam_tasklet_action,
		(unsigned long)tasklet);
	tasklet_disable(&tasklet->tasklet);

	*tasklet_info = tasklet;

	return 0;
}

void cam_tasklet_deinit(void    **tasklet_info)
{
	struct cam_tasklet_info *tasklet = *tasklet_info;

	if (atomic_read(&tasklet->tasklet_active)) {
		atomic_set(&tasklet->tasklet_active, 0);
		tasklet_kill(&tasklet->tasklet);
		tasklet_disable(&tasklet->tasklet);
	}
	kfree(tasklet);
	*tasklet_info = NULL;
}

static inline void cam_tasklet_flush(struct cam_tasklet_info *tasklet_info)
{
	cam_tasklet_action((unsigned long) tasklet_info);
}

int cam_tasklet_start(void  *tasklet_info)
{
	struct cam_tasklet_info       *tasklet = tasklet_info;
	int i = 0;

	if (atomic_read(&tasklet->tasklet_active)) {
		CAM_ERR(CAM_ISP, "Tasklet already active. idx = %d",
			tasklet->index);
		return -EBUSY;
	}

	/* clean up the command queue first */
	for (i = 0; i < CAM_TASKLETQ_SIZE; i++) {
		list_del_init(&tasklet->cmd_queue[i].list);
		list_add_tail(&tasklet->cmd_queue[i].list,
			&tasklet->free_cmd_list);
	}

	atomic_set(&tasklet->tasklet_active, 1);

	tasklet_enable(&tasklet->tasklet);

	return 0;
}

void cam_tasklet_stop(void  *tasklet_info)
{
	struct cam_tasklet_info  *tasklet = tasklet_info;

	if (!atomic_read(&tasklet->tasklet_active))
		return;

	atomic_set(&tasklet->tasklet_active, 0);
	tasklet_kill(&tasklet->tasklet);
	tasklet_disable(&tasklet->tasklet);
	cam_tasklet_flush(tasklet);
}

/*
 * cam_tasklet_action()
 *
 * @brief:              Process function that will be called  when tasklet runs
 *                      on CPU
 *
 * @data:               Tasklet Info structure that is passed in tasklet_init
 *
 * @return:             Void
 */
static void cam_tasklet_action(unsigned long data)
{
	struct cam_tasklet_info          *tasklet_info = NULL;
	struct cam_tasklet_queue_cmd     *tasklet_cmd = NULL;

	tasklet_info = (struct cam_tasklet_info *)data;

	while (!cam_tasklet_dequeue_cmd(tasklet_info, &tasklet_cmd)) {
		tasklet_cmd->bottom_half_handler(tasklet_info->ctx_priv,
			tasklet_cmd->payload);
		cam_tasklet_put_cmd(tasklet_info, (void **)(&tasklet_cmd));
	}
}

