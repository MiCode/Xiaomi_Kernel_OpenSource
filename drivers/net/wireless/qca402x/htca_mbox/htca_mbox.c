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

/* Implementation of Host Target Communication
 * API v1 and HTCA Protocol v1
 * over Qualcomm QCA mailbox-based SDIO/SPI interconnects.
 */

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "../hif_sdio/hif.h"
#include "htca.h"
#include "htca_mbox_internal.h"

struct htca_target *htca_target_list[HTCA_NUM_DEVICES_MAX];

/* Single thread module initialization, module shutdown,
 * target start and target stop.
 */
static DEFINE_MUTEX(htca_startup_mutex);
static bool htca_initialized;

/* Initialize the HTCA software module.
 * Typically invoked exactly once.
 */
int htca_init(void)
{
	struct cbs_from_os callbacks;

	if (mutex_lock_interruptible(&htca_startup_mutex))
		return HTCA_ERROR; /* interrupted */

	if (htca_initialized) {
		mutex_unlock(&htca_startup_mutex);
		return HTCA_OK; /* Already initialized */
	}

	htca_initialized = true;

	htca_event_table_init();

	memset(&callbacks, 0, sizeof(callbacks));
	callbacks.dev_inserted_hdl = htca_target_inserted_handler;
	callbacks.dev_removed_hdl = htca_target_removed_handler;
	hif_init(&callbacks);

	mutex_unlock(&htca_startup_mutex);

	return HTCA_OK;
}

/* Shutdown the entire module and free all module data.
 * Inverse of htca_init.
 *
 * May be invoked only after all Targets are stopped.
 */
void htca_shutdown(void)
{
	int i;

	if (mutex_lock_interruptible(&htca_startup_mutex))
		return; /* interrupted */

	if (!htca_initialized) {
		mutex_unlock(&htca_startup_mutex);
		return; /* Not initialized, so nothing to shut down */
	}

	for (i = 0; i < HTCA_NUM_DEVICES_MAX; i++) {
		if (htca_target_instance(i)) {
			/* One or more Targets are still active --
			 * cannot shutdown software.
			 */
			mutex_unlock(&htca_startup_mutex);
			WARN_ON(1);
			return;
		}
	}

	hif_shutdown_device(NULL); /* Tell HIF that we're all done */
	htca_initialized = false;

	mutex_unlock(&htca_startup_mutex);
}

/* Start a Target. This typically happens once per Target after
 * the module has been initialized and a Target is powered on.
 *
 * When a Target starts, it posts a single credit to each mailbox
 * and it enters "HTCA configuration". During configuration
 * negotiation, block sizes for each HTCA endpoint are established
 * that both Host and Target agree. Once this is complete, the
 * Target starts normal operation so it can send/receive.
 */
int htca_start(void *tar)
{
	int status;
	u32 address;
	struct htca_target *target = (struct htca_target *)tar;

	mutex_lock(&htca_startup_mutex);

	if (!htca_initialized) {
		mutex_unlock(&htca_startup_mutex);
		return HTCA_ERROR;
	}

	init_waitqueue_head(&target->target_init_wait);

	/* Unmask Host controller interrupts associated with this Target */
	hif_un_mask_interrupt(target->hif_handle);

	/* Enable all interrupts of interest on the Target. */

	target->enb.int_status_enb = INT_STATUS_ENABLE_ERROR_SET(0x01) |
				     INT_STATUS_ENABLE_CPU_SET(0x01) |
				     INT_STATUS_ENABLE_COUNTER_SET(0x01) |
				     INT_STATUS_ENABLE_MBOX_DATA_SET(0x0F);

	target->enb.cpu_int_status_enb = CPU_INT_STATUS_ENABLE_BIT_SET(0x00);

	target->enb.err_status_enb =
	    ERROR_STATUS_ENABLE_RX_UNDERFLOW_SET(0x01) |
	    ERROR_STATUS_ENABLE_TX_OVERFLOW_SET(0x01);

	target->enb.counter_int_status_enb =
	    COUNTER_INT_STATUS_ENABLE_BIT_SET(0xFF);

	/* Commit interrupt register values to Target HW. */
	address = get_reg_addr(INTR_ENB_REG, ENDPOINT_UNUSED);
	status =
	    hif_read_write(target->hif_handle, address, &target->enb,
			   sizeof(target->enb), HIF_WR_SYNC_BYTE_INC, NULL);
	if (status != HIF_OK) {
		_htca_stop(target);
		mutex_unlock(&htca_startup_mutex);
		return HTCA_ERROR;
	}

	/* At this point, we're waiting for the Target to post
	 * 1 credit to each mailbox. This allows us to begin
	 * configuration negotiation. We should see an interrupt
	 * as soon as the first credit is posted. The remaining
	 * credits should be posted almost immediately after.
	 */

	/* Wait indefinitely until configuration negotiation with
	 * the Target completes and the Target tells us it is ready to go.
	 */
	if (!target->ready) {
		/* NB: Retain the htca_statup_mutex during this wait.
		 * This serializes startup but should be OK.
		 */

		wait_event_interruptible(target->target_init_wait,
					 target->ready);

		if (target->ready) {
			status = HTCA_OK;
		} else {
			status = HTCA_ERROR;
			_htca_stop(target);
		}
	}

	mutex_unlock(&htca_startup_mutex);
	return status;
}

void _htca_stop(struct htca_target *target)
{
	uint ep;
	struct htca_endpoint *end_point;
	u32 address;

	/* Note: htca_startup_mutex must be held on entry */
	if (!htca_initialized)
		return;

	htca_work_task_stop(target);

	/* Disable interrupts at source, on Target */
	target->enb.int_status_enb = 0;
	target->enb.cpu_int_status_enb = 0;
	target->enb.err_status_enb = 0;
	target->enb.counter_int_status_enb = 0;

	address = get_reg_addr(INTR_ENB_REG, ENDPOINT_UNUSED);

	/* Try to disable all interrupts on the Target. */
	(void)hif_read_write(target->hif_handle, address, &target->enb,
			     sizeof(target->enb), HIF_WR_SYNC_BYTE_INC, NULL);

	/* Disable Host controller interrupts */
	hif_mask_interrupt(target->hif_handle);

	/* Flush all the queues and return the buffers to their owner */
	for (ep = 0; ep < HTCA_NUM_MBOX; ep++) {
		unsigned long flags;

		end_point = &target->end_point[ep];

		spin_lock_irqsave(&end_point->tx_credit_lock, flags);
		end_point->tx_credits_available = 0;
		spin_unlock_irqrestore(&end_point->tx_credit_lock, flags);

		end_point->enabled = false;

		/* Flush the Pending Receive Queue */
		htca_mbox_queue_flush(end_point, &end_point->recv_pending_queue,
				      &end_point->recv_free_queue,
				      HTCA_EVENT_BUFFER_RECEIVED);

		/* Flush the Pending Send Queue */
		htca_mbox_queue_flush(end_point, &end_point->send_pending_queue,
				      &end_point->send_free_queue,
				      HTCA_EVENT_BUFFER_SENT);
	}

	target->ready = false;

	hif_detach(target->hif_handle);

	/* Remove this Target from the global list */
	htca_target_instance_remove(target);

	/* Free target memory */
	kfree(target);
}

void htca_stop(void *tar)
{
	struct htca_target *target = (struct htca_target *)tar;

	htca_work_task_stop(target);
	htca_compl_task_stop(target);

	mutex_lock(&htca_startup_mutex);
	_htca_stop(target);
	mutex_unlock(&htca_startup_mutex);
}

/* Provides an interface for the caller to register for
 * various events supported by the HTCA module.
 */
int htca_event_reg(void *tar,
		   u8 end_point_id,
		   u8 event_id,
		   htca_event_handler event_handler, void *param)
{
	int status;
	struct htca_endpoint *end_point;
	struct htca_event_info event_info;
	struct htca_target *target = (struct htca_target *)tar;

	/* Register a new handler BEFORE dispatching events.
	 * UNregister a handler AFTER dispatching events.
	 */
	if (event_handler) {
		/* Register a new event handler */

		status = htca_add_to_event_table(target, end_point_id, event_id,
						 event_handler, param);
		if (status != HTCA_OK)
			return status; /* Fail to register handler */
	}

	/* Handle events associated with this handler */
	switch (event_id) {
	case HTCA_EVENT_TARGET_AVAILABLE:
		if (event_handler) {
			struct htca_target *targ;
			int i;

			/* Dispatch a Target Available event for all Targets
			 * that are already present.
			 */
			for (i = 0; i < HTCA_NUM_DEVICES_MAX; i++) {
				targ = htca_target_list[i];
				if (targ) {
					size_t size = hif_get_device_size();

					htca_frame_event(&event_info,
							 (u8 *)targ->hif_handle,
							 size, size,
							 HTCA_OK, NULL);

					htca_dispatch_event(
					    targ, ENDPOINT_UNUSED,
					    HTCA_EVENT_TARGET_AVAILABLE,
					    &event_info);
				}
			}
		}
		break;

	case HTCA_EVENT_TARGET_UNAVAILABLE:
		break;

	case HTCA_EVENT_BUFFER_RECEIVED:
		if (!event_handler) {
			/* Flush the Pending Recv queue before unregistering
			 * the event handler.
			 */
			end_point = &target->end_point[end_point_id];
			htca_mbox_queue_flush(end_point,
					      &end_point->recv_pending_queue,
					      &end_point->recv_free_queue,
					      HTCA_EVENT_BUFFER_RECEIVED);
		}
		break;

	case HTCA_EVENT_BUFFER_SENT:
		if (!event_handler) {
			/* Flush the Pending Send queue before unregistering
			 * the event handler.
			 */
			end_point = &target->end_point[end_point_id];
			htca_mbox_queue_flush(end_point,
					      &end_point->send_pending_queue,
					      &end_point->send_free_queue,
					      HTCA_EVENT_BUFFER_SENT);
		}
		break;

	case HTCA_EVENT_DATA_AVAILABLE:
		/* We could dispatch a data available event. Instead,
		 * we require users to register this event handler
		 * before posting receive buffers.
		 */
		break;

	default:
		return HTCA_EINVAL; /* unknown event? */
	}

	if (!event_handler) {
		/* Unregister an event handler */
		status = htca_remove_from_event_table(target,
						      end_point_id, event_id);
		if (status != HTCA_OK)
			return status;
	}

	return HTCA_OK;
}

/* Enqueue to the endpoint's recv_pending_queue an empty buffer
 * which will receive data from the Target.
 */
int htca_buffer_receive(void *tar,
			u8 end_point_id, u8 *buffer,
			u32 length, void *cookie)
{
	struct htca_endpoint *end_point;
	struct htca_mbox_request *mbox_request;
	struct htca_event_table_element *ev;
	unsigned long flags;
	struct htca_target *target = (struct htca_target *)tar;

	end_point = &target->end_point[end_point_id];

	if (!end_point->enabled)
		return HTCA_ERROR;

	/* Length must be a multiple of block_size.
	 * (Ideally, length should match the largest message that can be sent
	 * over this endpoint, including HTCA header, rounded up to blocksize.)
	 */
	if (length % end_point->block_size)
		return HTCA_EINVAL;

	if (length > HTCA_MESSAGE_SIZE_MAX)
		return HTCA_EINVAL;

	if (length < HTCA_HEADER_LEN_MAX)
		return HTCA_EINVAL;

	ev = htca_event_id_to_event(target, end_point_id,
				    HTCA_EVENT_BUFFER_RECEIVED);
	if (!ev->handler) {
		/* In order to use this API, caller must
		 * register an event handler for HTCA_EVENT_BUFFER_RECEIVED.
		 */
		return HTCA_ERROR;
	}

	spin_lock_irqsave(&end_point->mbox_queue_lock, flags);
	mbox_request = (struct htca_mbox_request *)htca_request_deq_head(
	    &end_point->recv_free_queue);
	spin_unlock_irqrestore(&end_point->mbox_queue_lock, flags);
	if (!mbox_request)
		return HTCA_ENOMEM;

	if (WARN_ON(mbox_request->req.target != target))
		return HTCA_ERROR;

	mbox_request->buffer = buffer;
	/* includes space for HTCA header */
	mbox_request->buffer_length = length;
	/* filled in after message is received */
	mbox_request->actual_length = 0;
	mbox_request->end_point = end_point;
	mbox_request->cookie = cookie;

	spin_lock_irqsave(&end_point->mbox_queue_lock, flags);
	htca_request_enq_tail(&end_point->recv_pending_queue,
			      (struct htca_request *)mbox_request);
	spin_unlock_irqrestore(&end_point->mbox_queue_lock, flags);

	/* Alert the work_task that there may be work to do */
	htca_work_task_poke(target);

	return HTCA_OK;
}

/* Enqueue a buffer to be sent to the Target.
 *
 * Supplied buffer must be preceded by HTCA_HEADER_LEN_MAX bytes for the
 * HTCA header (of which HTCA_HEADER_LEN bytes are actually used, and the
 * remaining are padding).
 *
 * Must be followed with sufficient space for block-size padding.
 *
 * Example:
 * To send a 10B message over an endpoint that uses 64B blocks, caller
 * specifies length=10. HTCA adds HTCA_HEADER_LEN_MAX bytes just before
 * buffer, consisting of HTCA_HEADER_LEN header bytes followed by
 * HTCA_HEADER_LEN_MAX-HTCA_HEADER_LEN pad bytes. HTC sends blockSize
 * bytes starting at buffer-HTCA_HEADER_LEN_MAX.
 */
int htca_buffer_send(void *tar,
		     u8 end_point_id,
		     u8 *buffer, u32 length, void *cookie)
{
	struct htca_endpoint *end_point;
	struct htca_mbox_request *mbox_request;
	struct htca_event_table_element *ev;
	unsigned long flags;
	struct htca_target *target = (struct htca_target *)tar;

	end_point = &target->end_point[end_point_id];

	if (!end_point->enabled)
		return HTCA_ERROR;

	if (length + HTCA_HEADER_LEN_MAX > HTCA_MESSAGE_SIZE_MAX)
		return HTCA_EINVAL;

	ev = htca_event_id_to_event(target, end_point_id,
				    HTCA_EVENT_BUFFER_SENT);
	if (!ev->handler) {
		/* In order to use this API, caller must
		 * register an event handler for HTCA_EVENT_BUFFER_SENT.
		 */
		return HTCA_ERROR;
	}

	spin_lock_irqsave(&end_point->mbox_queue_lock, flags);
	mbox_request = (struct htca_mbox_request *)htca_request_deq_head(
	    &end_point->send_free_queue);
	spin_unlock_irqrestore(&end_point->mbox_queue_lock, flags);
	if (!mbox_request)
		return HTCA_ENOMEM;

	/* Buffer will be adjusted by HTCA_HEADER_LEN later, in
	 * htca_send_request_to_hif.
	 */
	mbox_request->buffer = buffer;
	mbox_request->buffer_length = length;
	mbox_request->actual_length = length;
	mbox_request->end_point = end_point;
	mbox_request->cookie = cookie;

	spin_lock_irqsave(&end_point->mbox_queue_lock, flags);
	htca_request_enq_tail(&end_point->send_pending_queue,
			      (struct htca_request *)mbox_request);
	spin_unlock_irqrestore(&end_point->mbox_queue_lock, flags);

	/* Alert the work_task that there may be work to do */
	htca_work_task_poke(target);

	return HTCA_OK;
}
