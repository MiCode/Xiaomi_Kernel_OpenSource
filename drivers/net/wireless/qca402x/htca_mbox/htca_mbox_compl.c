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
#include <linux/kthread.h>
#include <linux/spinlock_types.h>
#include <linux/wait.h>

#include "../hif_sdio/hif.h"
#include "htca.h"
#include "htca_mbox_internal.h"

/* Host Target Communications Completion Management */

/* Top-level callback handler, registered with HIF to be invoked
 * whenever a read/write HIF operation completes. Executed in the
 * context of an HIF task, so we don't want to take much time
 * here. Pass processing to HTCA's compl_task.
 *
 * Used for both reg_requests and mbox_requests.
 */
int htca_rw_completion_handler(void *context, int status)
{
	struct htca_request *req;
	struct htca_target *target;
	unsigned long flags;

	req = (struct htca_request *)context;
	if (!context) {
		/* No completion required for this request.
		 * (e.g. Fire-and-forget register write.)
		 */
		return HTCA_OK;
	}

	target = req->target;
	req->status = status;

	/* Enqueue this completed request on the
	 * Target completion queue.
	 */
	spin_lock_irqsave(&target->compl_queue_lock, flags);
	htca_request_enq_tail(&target->compl_queue, (struct htca_request *)req);
	spin_unlock_irqrestore(&target->compl_queue_lock, flags);

	/* Notify the completion task that it has work */
	htca_compl_task_poke(target);

	return HTCA_OK;
}

/* Request-specific callback invoked by the HTCA Completion Task
 * when a Mbox Send Request completes. Note: Used for Mbox Send
 * requests; not used for Reg requests.
 *
 * Simply dispatch a BUFFER_SENT event to the originator of the request.
 */
void htca_send_compl(struct htca_request *req, int status)
{
	struct htca_target *target;
	u8 end_point_id;
	struct htca_event_info event_info;
	struct htca_endpoint *end_point;
	struct htca_mbox_request *mbox_request =
			(struct htca_mbox_request *)req;
	unsigned long flags;

	end_point = mbox_request->end_point;
	target = end_point->target;
	end_point_id = get_endpoint_id(end_point);

	/* Strip off the HTCA header that was added earlier */
	mbox_request->buffer += HTCA_HEADER_LEN_MAX;

	/* Prepare event frame to notify caller */
	htca_frame_event(&event_info, mbox_request->buffer,
			 mbox_request->buffer_length,
			 mbox_request->actual_length,
			 (status == HIF_OK) ? HTCA_OK : HTCA_ECANCELED,
			 mbox_request->cookie);

	/* Recycle the request */
	spin_lock_irqsave(&end_point->mbox_queue_lock, flags);
	htca_request_enq_tail(&end_point->send_free_queue,
			      (struct htca_request *)mbox_request);
	spin_unlock_irqrestore(&end_point->mbox_queue_lock, flags);
	/* Regardless of success/failure, notify caller that HTCA is done
	 * with his buffer.
	 */
	htca_dispatch_event(target, end_point_id, HTCA_EVENT_BUFFER_SENT,
			    &event_info);
}

/* Request-specific callback invoked by the HTCA Completion Task
 * when a Mbox Recv Request completes. Note: Used for Mbox Recv
 * requests; not used for Reg requests.
 *
 * Simply dispatch a BUFFER_RECEIVED event to the originator
 * of the request.
 */
void htca_recv_compl(struct htca_request *req, int status)
{
	struct htca_target *target;
	struct htca_event_info event_info;
	u8 end_point_id;
	struct htca_endpoint *end_point;
	struct htca_mbox_request *mbox_request =
	    (struct htca_mbox_request *)req;
	unsigned long flags;

	end_point = mbox_request->end_point;
	target = end_point->target;
	end_point_id = get_endpoint_id(end_point);

	/* Signaling:
	 * Now that we have consumed recv data, clar rx_frame_length so that
	 * htca_manage_pending_recvs will not try to re-read the same data.
	 *
	 * Set need_register_refresh so we can determine whether or not there
	 * is additional data waiting to be read.
	 *
	 * Clear our endpoint from the pending_recv_mask so
	 * htca_manage_pending_recvs
	 * is free to issue another read.
	 *
	 * Finally, poke the work_task.
	 */
	end_point->rx_frame_length = 0;
	target->need_register_refresh = 1;
	spin_lock_irqsave(&target->pending_op_lock, flags);
	target->pending_recv_mask &= ~(1 << end_point_id);
	spin_unlock_irqrestore(&target->pending_op_lock, flags);
	htca_work_task_poke(target);

	if (status == HIF_OK) {
		u32 check_length;
		/* Length coming from Target is always LittleEndian */
		check_length = ((mbox_request->buffer[0] << 0) |
				(mbox_request->buffer[1] << 8));
		WARN_ON(mbox_request->actual_length != check_length);
	}

	/* Strip off header */
	mbox_request->buffer += HTCA_HEADER_LEN_MAX;

	htca_frame_event(&event_info, mbox_request->buffer,
			 mbox_request->buffer_length,
			 mbox_request->actual_length,
			 (status == HIF_OK) ? HTCA_OK : HTCA_ECANCELED,
			 mbox_request->cookie);

	/* Recycle the request */
	spin_lock_irqsave(&end_point->mbox_queue_lock, flags);
	htca_request_enq_tail(&end_point->recv_free_queue,
			      (struct htca_request *)mbox_request);
	spin_unlock_irqrestore(&end_point->mbox_queue_lock, flags);

	htca_dispatch_event(target, end_point_id, HTCA_EVENT_BUFFER_RECEIVED,
			    &event_info);
}

/* Request-specific callback invoked when a register read/write
 * request completes. reg_request structures are not used for
 * register WRITE requests so there's not much to do for writes.
 *
 * Note: For Mbox Request completions see htca_send_compl
 * and htca_recv_compl.
 */

/* Request-specific callback invoked by the HTCA Completion Task
 * when a Reg Request completes. Note: Used for Reg requests;
 * not used for Mbox requests.
 */
void htca_reg_compl(struct htca_request *req, int status)
{
	struct htca_target *target;
	struct htca_reg_request *reg_request = (struct htca_reg_request *)req;
	unsigned long flags;

	if (WARN_ON(!reg_request))
		return;

	htcadebug("purpose=0x%x\n", reg_request->purpose);

	/* Process async register read/write completion */

	target = reg_request->req.target;
	if (status != HIF_OK) {
		/* Recycle the request */
		reg_request->purpose = UNUSED_PURPOSE;
		spin_lock_irqsave(&target->reg_queue_lock, flags);
		htca_request_enq_tail(&target->reg_free_queue,
				      (struct htca_request *)reg_request);
		spin_unlock_irqrestore(&target->reg_queue_lock, flags);

		/* A register read/write accepted by HIF
		 * should never fail.
		 */
		WARN_ON(1);
		return;
	}

	switch (reg_request->purpose) {
	case INTR_REFRESH:
		/* Target register state, including interrupt
		 * registers, has been fetched.
		 */
		htca_register_refresh_compl(target, reg_request);
		break;

	case CREDIT_REFRESH:
		htca_credit_refresh_compl(target, reg_request);
		break;

	case UPDATE_TARG_INTRS:
	case UPDATE_TARG_AND_ENABLE_HOST_INTRS:
		htca_update_intr_enbs_compl(target, reg_request);
		break;

	default:
		WARN_ON(1); /* unhandled request type */
		break;
	}

	/* Recycle this register read/write request */
	reg_request->purpose = UNUSED_PURPOSE;
	spin_lock_irqsave(&target->reg_queue_lock, flags);
	htca_request_enq_tail(&target->reg_free_queue,
			      (struct htca_request *)reg_request);
	spin_unlock_irqrestore(&target->reg_queue_lock, flags);
}

/* After a Register Refresh, uppdate tx_credits_to_reap for each end_point.  */
static void htca_update_tx_credits_to_reap(struct htca_target *target,
					   struct htca_reg_request *reg_request)
{
	struct htca_endpoint *end_point;
	int ep;

	for (ep = 0; ep < HTCA_NUM_MBOX; ep++) {
		end_point = &target->end_point[ep];

		if (reg_request->u.reg_table.status.counter_int_status &
		    (0x10 << ep)) {
			end_point->tx_credits_to_reap = true;
		} else {
			end_point->tx_credits_to_reap = false;
		}
	}
}

/* After a Register Refresh, uppdate rx_frame_length for each end_point.  */
static void htca_update_rx_frame_lengths(struct htca_target *target,
					 struct htca_reg_request *reg_request)
{
	struct htca_endpoint *end_point;
	u32 rx_lookahead;
	u32 frame_length;
	int ep;

	htcadebug("Enter\n");
	for (ep = 0; ep < HTCA_NUM_MBOX; ep++) {
		end_point = &target->end_point[ep];

		if (end_point->rx_frame_length != 0) {
			/* NB: Will be cleared in htca_recv_compl after
			 * frame is read
			 */
			continue;
		}

		if (!(reg_request->u.reg_table.rx_lookahead_valid &
		      (1 << ep))) {
			continue;
		}

		/* The length of the incoming message is contained
		 * in the first two (HTCA_HEADER_LEN) bytes in
		 * LittleEndian order.
		 *
		 * This length does NOT include the HTCA header nor block
		 * padding.
		 */
		rx_lookahead = reg_request->u.reg_table.rx_lookahead[ep];
		frame_length = rx_lookahead & 0x0000ffff;

		end_point->rx_frame_length = frame_length;
		htcadebug("ep#%d : %d\n", ep,
			  frame_length);
	}
}

static unsigned int htca_debug_no_pending; /* debug only */

/* Completion for a register refresh.
 *
 * Update rxFrameLengths and tx_credits_to_reap info for
 * each endpoint. Then handle all pending interrupts (o
 * if interrupts are currently masked at the Host, handle
 * all interrupts that would be pending if interrupts
 * were enabled).
 *
 * Called in the context of HIF's completion task whenever
 * results from a register refresh are received.
 */
void htca_register_refresh_compl(struct htca_target *target,
				 struct htca_reg_request *req)
{
	u8 host_int_status;
	u8 pnd_enb_intrs; /* pending and enabled interrupts */
	u8 pending_int;
	u8 enabled_int;
	unsigned long flags;

	htcadebug("Enter\n");

	if (WARN_ON(target->pending_register_refresh == 0))
		return;

	spin_lock_irqsave(&target->pending_op_lock, flags);
	target->pending_register_refresh--;
	spin_unlock_irqrestore(&target->pending_op_lock, flags);

	htcadebug(
	    "REGDUMP: hostis=0x%02x cpuis=0x%02x erris=0x%02x cntris=0x%02x\n",
	    req->u.reg_table.status.host_int_status,
	    req->u.reg_table.status.cpu_int_status,
	    req->u.reg_table.status.err_int_status,
	    req->u.reg_table.status.counter_int_status);
	htcadebug(
	    "mbox_frame=0x%02x lav=0x%02x la0=0x%08x la1=0x%08x la2=0x%08x la3=0x%08x\n",
	    req->u.reg_table.mbox_frame, req->u.reg_table.rx_lookahead_valid,
	    req->u.reg_table.rx_lookahead[0], req->u.reg_table.rx_lookahead[1],
	    req->u.reg_table.rx_lookahead[2], req->u.reg_table.rx_lookahead[3]);

	/* Update rxFrameLengths */
	htca_update_rx_frame_lengths(target, req);

	/* Update tx_credits_to_reap */
	htca_update_tx_credits_to_reap(target, req);

	/* Process pending Target interrupts. */

	/* Restrict attention to pending interrupts of interest */
	host_int_status = req->u.reg_table.status.host_int_status;

	/* Unexpected and unhandled */
	if (WARN_ON(host_int_status & HOST_INT_STATUS_DRAGON_INT_MASK))
		return;

	/* Form software's idea of pending and enabled interrupts.
	 * Start with ERRORs and CPU interrupts.
	 */
	pnd_enb_intrs = host_int_status &
			(HOST_INT_STATUS_ERROR_MASK | HOST_INT_STATUS_CPU_MASK);

	/* Software may have intended to enable/disable credit
	 * counter interrupts; but we commit these updates to
	 * Target hardware lazily, just before re-enabling
	 * interrupts. So registers that we have now may not
	 * reflect the intended state of interrupt enables.
	 */

	/* Based on software credit enable bits, update pnd_enb_intrs
	 * (which is like a software copy of host_int_status) as if
	 * all desired interrupt enables had been committed to HW.
	 */
	pending_int = req->u.reg_table.status.counter_int_status;
	enabled_int = target->enb.counter_int_status_enb;
	if (pending_int & enabled_int)
		pnd_enb_intrs |= HOST_INT_STATUS_COUNTER_MASK;

	/* Based on software recv data enable bits, update
	 * pnd_enb_intrs AS IF all the interrupt enables had
	 * been committed to HW.
	 */
	pending_int = host_int_status;
	enabled_int = target->enb.int_status_enb;
	pnd_enb_intrs |= (pending_int & enabled_int);

	if (!pnd_enb_intrs) {
		/* No enabled interrupts are pending. */
		htca_debug_no_pending++;
	}

	/* Invoke specific handlers for each enabled and pending interrupt.
	 * The goal of each service routine is to clear interrupts at the
	 * source (on the Target).
	 *
	 * We deal with four types of interrupts in the HOST_INT_STATUS
	 * summary register:
	 * errors
	 * This remains set until bits in ERROR_INT_STATUS are cleared
	 *
	 * CPU
	 * This remains set until bits in CPU_INT_STATUS are cleared
	 *
	 * rx data available
	 * These remain set as long as rx data is available. HW clears
	 * the rx data available enable bits when receive buffers
	 * are exhausted. If we exhaust Host-side received buffers, we
	 * mask the rx data available interrupt.
	 *
	 * tx credits available
	 * This remains set until all bits in COUNTER_INT_STATUS are
	 * cleared by HW after Host SW reaps all credits on a mailbox.
	 * If credits on an endpoint are sufficient, we mask the
	 * corresponding COUNTER_INT_STATUS bit. We avoid "dribbling"
	 * one credit at a time and instead reap them en masse.
	 *
	 * The HOST_INT_STATUS register is read-only these bits are cleared
	 * by HW when the underlying condition is cleared.
	 */

	if (HOST_INT_STATUS_ERROR_GET(pnd_enb_intrs))
		htca_service_error_interrupt(target, req);

	if (HOST_INT_STATUS_CPU_GET(pnd_enb_intrs))
		htca_service_cpu_interrupt(target, req);

	if (HOST_INT_STATUS_COUNTER_GET(pnd_enb_intrs))
		htca_service_credit_counter_interrupt(target, req);

	/* Always needed in order to at least unmask Host interrupts */
	htca_work_task_poke(target);
}

/* Complete an update of interrupt enables. */
void htca_update_intr_enbs_compl(struct htca_target *target,
				 struct htca_reg_request *req)
{
	htcadebug("Enter\n");
	if (req->purpose == UPDATE_TARG_AND_ENABLE_HOST_INTRS) {
		/* NB: non-intuitive, but correct */

		/* While waiting for rxdata and txcred
		 * interrupts to be disabled at the Target,
		 * we temporarily masked interrupts at
		 * the Host. It is now safe to allow
		 * interrupts (esp. ERROR and CPU) at
		 * the Host.
		 */
		htcadebug("Unmasking\n");
		hif_un_mask_interrupt(target->hif_handle);
	}
}

/* Called to complete htca_credit_refresh_start.
 *
 * Ends a credit refresh cycle. Called after decrementing a
 * credit counter register (many times in a row). HW atomically
 * decrements the counter and returns the OLD value but HW will
 * never reduce it below 0.
 *
 * Called in the context of the work_task when the credit counter
 * decrement operation completes synchronously. Called in the
 * context of the compl_task when the credit counter decrement
 * operation completes asynchronously.
 */
void htca_credit_refresh_compl(struct htca_target *target,
			       struct htca_reg_request *reg_request)
{
	struct htca_endpoint *end_point;
	unsigned long flags;
	int reaped;
	int i;

	/* A non-zero value indicates 1 credit reaped.
	 * Typically, we will find monotonically descending
	 * values that reach 0 with the remaining values
	 * all zero. But we must scan the entire results
	 * to handle the case where the Target just happened
	 * to increment credits simultaneously with our
	 * series of credit decrement operations.
	 */
	htcadebug("ep=%d\n", reg_request->epid);
	end_point = &target->end_point[reg_request->epid];
	reaped = 0;
	for (i = 0; i < HTCA_TX_CREDITS_REAP_MAX; i++) {
		htcadebug("|R0x%02x", reg_request->u.credit_dec_results[i]);
		if (reg_request->u.credit_dec_results[i])
			reaped++;
	}

	htcadebug("\nreaped %d credits on ep=%d\n", reaped, reg_request->epid);

	spin_lock_irqsave(&end_point->tx_credit_lock, flags);
	end_point->tx_credits_available += reaped;
	end_point->tx_credit_refresh_in_progress = false;
	spin_unlock_irqrestore(&end_point->tx_credit_lock, flags);

	htca_work_task_poke(target);
}
