/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#include <linux/completion.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/spinlock_types.h>
#include <linux/wait.h>

#include "../hif_sdio/hif.h"
#include "htca.h"
#include "htca_mbox_internal.h"

/* HTCA utility routines  */

/* Invoked when shutting down */
void htca_mbox_queue_flush(struct htca_endpoint *end_point,
			   struct htca_request_queue *pending_queue,
			   struct htca_request_queue *free_queue,
			   u8 event_id)
{
	struct htca_event_info event_info;
	u8 end_point_id;
	struct htca_target *target;
	struct htca_mbox_request *mbox_request;
	unsigned long flags;

	target = end_point->target;
	end_point_id = get_endpoint_id(end_point);

	spin_lock_irqsave(&end_point->mbox_queue_lock, flags);
	for (;;) {
		mbox_request =
		    (struct htca_mbox_request *)htca_request_deq_head(
			pending_queue);
		spin_unlock_irqrestore(&end_point->mbox_queue_lock, flags);

		if (!mbox_request)
			break;

		htca_frame_event(&event_info, mbox_request->buffer,
				 mbox_request->buffer_length, 0, HTCA_ECANCELED,
				 mbox_request->cookie);

		htca_dispatch_event(target, end_point_id, event_id,
				    &event_info);

		/* Recycle the request */
		spin_lock_irqsave(&end_point->mbox_queue_lock, flags);
		htca_request_enq_tail(free_queue,
				      (struct htca_request *)mbox_request);
	}
	spin_unlock_irqrestore(&end_point->mbox_queue_lock, flags);
}

struct htca_target *htca_target_instance(int i)
{
	return htca_target_list[i];
}

void htca_target_instance_add(struct htca_target *target)
{
	int i;

	for (i = 0; i < HTCA_NUM_DEVICES_MAX; i++) {
		if (!htca_target_list[i]) {
			htca_target_list[i] = target;
			break;
		}
	}
	WARN_ON(i >= HTCA_NUM_DEVICES_MAX);
}

void htca_target_instance_remove(struct htca_target *target)
{
	int i;

	for (i = 0; i < HTCA_NUM_DEVICES_MAX; i++) {
		if (htca_target_list[i] == target) {
			htca_target_list[i] = NULL;
			break;
		}
	}
	WARN_ON(i >= HTCA_NUM_DEVICES_MAX);
}

/* Add a request to the tail of a queue.
 * Caller must handle any locking required.
 * TBD: Use Linux queue support
 */
void htca_request_enq_tail(struct htca_request_queue *queue,
			   struct htca_request *req)
{
	req->next = NULL;

	if (queue->tail)
		queue->tail->next = (void *)req;
	else
		queue->head = req;

	queue->tail = req;
}

/* Remove a request from the start of a queue.
 * Caller must handle any locking required.
 * TBD: Use Linux queue support
 * TBD: If cannot allocate from FREE queue, caller may add more elements.
 */
struct htca_request *htca_request_deq_head(struct htca_request_queue *queue)
{
	struct htca_request *req;

	req = queue->head;
	if (!req)
		return NULL;

	queue->head = req->next;
	if (!queue->head)
		queue->tail = NULL;
	req->next = NULL;

	return req;
}

/* Start a Register Refresh cycle.
 *
 * Submits a request to fetch ALL relevant registers from Target.
 * When this completes, we'll take actions based on the new
 * register values.
 */
void htca_register_refresh_start(struct htca_target *target)
{
	int status;
	struct htca_reg_request *reg_request;
	u32 address;
	unsigned long flags;

	htcadebug("Enter\n");
	spin_lock_irqsave(&target->reg_queue_lock, flags);
	reg_request = (struct htca_reg_request *)htca_request_deq_head(
	    &target->reg_free_queue);
	spin_unlock_irqrestore(&target->reg_queue_lock, flags);
	if (!reg_request) {
		WARN_ON(1);
		return;
	}
	if (WARN_ON(reg_request->purpose != UNUSED_PURPOSE))
		return;

	spin_lock_irqsave(&target->pending_op_lock, flags);
	target->pending_register_refresh++;
	spin_unlock_irqrestore(&target->pending_op_lock, flags);

	reg_request->buffer = (u8 *)&reg_request->u.reg_table;
	reg_request->length = sizeof(reg_request->u.reg_table);
	reg_request->purpose = INTR_REFRESH;
	reg_request->epid = 0; /* not used */

	address = get_reg_addr(ALL_STATUS_REG, ENDPOINT_UNUSED);
	status = hif_read_write(target->hif_handle, address,
				&reg_request->u.reg_table,
				sizeof(reg_request->u.reg_table),
				HIF_RD_ASYNC_BYTE_INC, reg_request);
	if (status == HIF_OK && reg_request->req.completion_cb) {
		reg_request->req.completion_cb(
		    (struct htca_request *)reg_request, HIF_OK);
		/* htca_register_refresh_compl */
	} else if (status == HIF_PENDING) {
		/* Will complete later */
	} else { /* HIF error */
		WARN_ON(1);
	}
}
