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
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock_types.h>
#include <linux/wait.h>

#include "../hif_sdio/hif.h"
#include "htca.h"
#include "htca_mbox_internal.h"

/* Host Target Communications Interrupt Management */

/* Interrupt Management
 * When an interrupt occurs at the Host, it is to tell us about
 * a high-priority error interrupt
 * a CPU interrupt (TBD)
 * rx data available
 * tx credits available
 *
 * From an interrupt management perspective, rxdata and txcred
 * interrupts are grouped together. When either of these occurs,
 * we enter a mode where we repeatedly refresh register state
 * and act on all interrupt information in the refreshed registers.
 * We are basically polling with rxdata and txcred interrupts
 * masked. Eventually, we refresh registers and find no rxdata
 * and no txcred interrupts pending. At this point, we unmask
 * those types of interrupts.
 *
 * Unmasking is selective: We unmask only the interrupts that
 * we want to receive which include
 * -rxdata interrupts for endpoints that have received
 * buffers on the recv pending queue
 * -txcred interrupts for endpoints with a very low
 * count of creditsAvailable
 * Other rxdata and txcred interrupts are masked. These include:
 * -rxdata interrupts for endpoint that lack recv buffers
 * -txcred interrupts for endpoint with lots of credits
 *
 * Very little activity takes place in the context of the
 * interrupt function (Delayed Service Routine). We mask
 * interrupts at the Host, send a command to disable all
 * rxdata/txcred interrupts and finally start a register
 * refresh. When the register refresh completes, we unmask
 * interrupts on the Host and poke the work_task which now
 * has valid register state to examine.
 *
 * The work_task repeatedly
 * handles outstanding rx and tx service
 * starts another register refresh
 * Every time a register refresh completes, it pokes the
 * work_task. This cycle continues until the work_task finds
 * nothing to do after a register refresh. At this point,
 * it unmasks rxdata/txcred interrupts at the Target (again,
 * selectively).
 *
 * While in the work_task polling cycle, we maintain a notion
 * of interrupt enables in software rather than commit these
 * to Target HW.
 *
 *
 * Credit State Machine:
 * Credits are
 * -Added by the Target whenever a Target-side receive
 * buffer is added to a mailbox
 * -Never rescinded by the Target
 * -Reaped by this software after a credit refresh cycle
 * which is initiated
 * -as a result of a credit counter interrupt
 * -after a send completes and the number of credits
 * are below an acceptable threshold
 * -used by this software when it sends a message HIF to
 * be sent to the Target
 *
 * The process of "reaping" credits involves first issuing
 * a sequence of reads to the COUNTER_DEC register. (This is
 * known as the start of a credit refresh.) We issue a large
 * number of reads in order to reap as many credits at once
 * as we can. When these reads complete, we determine how
 * many credits were available and increase software's notion
 * of tx_credits_available accordingly.
 *
 * Note: All Target reads/writes issued from the interrupt path
 * should be asynchronous. HIF adds such a request to a queue
 * and immediately returns.
 *
 * TBD: It might be helpful for HIF to support a "priority
 * queue" -- requests that should be issued prior to anything
 * in its normal queue. Even with this, a request might have
 * to wait for a while as the current, read/write request
 * completes on SDIO and then wait for all prior priority
 * requests to finish. So probably not worth the additional
 * complexity.
 */

/* Maximum message sizes for each endpoint.
 * Must be a multiple of the block size.
 * Must be no greater than HTCA_MESSAGE_SIZE_MAX.
 *
 * TBD: These should be tunable. Example anticipated usage:
 * ep0: Host-side networking control messages
 * ep1: Host-side networking data messages
 * ep2: OEM control messages
 * ep3: OEM data messages
 */
static u32 htca_msg_size[HTCA_NUM_MBOX] = {256, 3 * 512, 512, 2048};

/* Commit the shadow interrupt enables in software to
 * Target Hardware. This is where the "lazy commit"
 * occurs. Always called in the context of work_task.
 *
 * When the host's intr_state is POLL:
 * -All credit count interrupts and all rx data interrupts
 * are disabled at the Target.
 *
 * When the host's intr_state is INTERRUPT:
 * -We commit the shadow copy of interrupt enables.
 * -A mailbox with low credit count will have the credit
 * interrupt enabled. A mailbox with high credit count
 * will have the credit interrupt disabled.
 * -A mailbox with no available receive buffers will have
 * the mailbox data interrupt disabled. A mailbox with
 * at least one receive buffer will have the mailbox
 * data interrupt enabled.
 */
void htca_update_intr_enbs(struct htca_target *target,
			   int enable_host_intrs)
{
	int status;
	struct htca_reg_request *reg_request;
	struct htca_intr_enables *enbregs;
	unsigned long flags;
	u32 address;

	htcadebug("Enter: enable_host_intrs=%d\n",
		  enable_host_intrs);
	htcadebug("ints: 0x%02x  --> 0x%02x\n",
		  target->last_committed_enb.int_status_enb,
		  target->enb.int_status_enb);
	htcadebug("cpu: 0x%02x	--> 0x%02x\n",
		  target->last_committed_enb.cpu_int_status_enb,
		  target->enb.cpu_int_status_enb);
	htcadebug("error: 0x%02x  --> 0x%02x\n",
		  target->last_committed_enb.err_status_enb,
		  target->enb.err_status_enb);
	htcadebug("counters: 0x%02x  --> 0x%02x\n",
		  target->last_committed_enb.counter_int_status_enb,
		  target->enb.counter_int_status_enb);
	if ((target->enb.int_status_enb ==
			target->last_committed_enb.int_status_enb) &&
		(target->enb.counter_int_status_enb ==
			target->last_committed_enb.counter_int_status_enb) &&
		(target->enb.cpu_int_status_enb ==
			target->last_committed_enb.cpu_int_status_enb) &&
		(target->enb.err_status_enb ==
			target->last_committed_enb.err_status_enb)) {
		/* No changes to Target-side interrupt enables are required.
		 * But we must still need to enable Host-side interrupts.
		 */
		if (enable_host_intrs) {
			htcadebug("Unmasking - no change to Target enables\n");
			hif_un_mask_interrupt(target->hif_handle);
		}
		return;
	}

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

	reg_request->buffer = NULL;
	reg_request->length = 0;
	reg_request->epid = 0; /* unused */
	enbregs = &reg_request->u.enb;

	if (target->intr_state == HTCA_INTERRUPT) {
		enbregs->int_status_enb = target->enb.int_status_enb;
		enbregs->counter_int_status_enb =
		    target->enb.counter_int_status_enb;
	} else {
		enbregs->int_status_enb = (target->enb.int_status_enb &
					   ~HOST_INT_STATUS_MBOX_DATA_MASK);
		enbregs->counter_int_status_enb = 0;
	}

	enbregs->cpu_int_status_enb = target->enb.cpu_int_status_enb;
	enbregs->err_status_enb = target->enb.err_status_enb;

	target->last_committed_enb = *enbregs; /* structure copy */

	if (enable_host_intrs)
		reg_request->purpose = UPDATE_TARG_AND_ENABLE_HOST_INTRS;
	else
		reg_request->purpose = UPDATE_TARG_INTRS;

	address = get_reg_addr(INTR_ENB_REG, ENDPOINT_UNUSED);

	status = hif_read_write(target->hif_handle, address, enbregs,
				sizeof(*enbregs), HIF_WR_ASYNC_BYTE_INC,
				reg_request);
	if (status == HIF_OK && reg_request->req.completion_cb) {
		reg_request->req.completion_cb(
		    (struct htca_request *)reg_request, HIF_OK);
		/* htca_update_intr_enbs_compl */
	} else if (status == HIF_PENDING) {
		/* Will complete later */
	} else { /* HIF error */
		WARN_ON(1);
	}
}

/* Delayed Service Routine, invoked from HIF in thread context
 * (from sdio's irqhandler) in order to handle interrupts
 * caused by the Target.
 *
 * This serves as a top-level interrupt dispatcher for HTCA.
 */
int htca_dsr_handler(void *htca_handle)
{
	struct htca_target *target = (struct htca_target *)htca_handle;

	htcadebug("Enter\n");
	if (target->ready) {
		/* Transition state to polling mode.
		 * Temporarily disable intrs at Host
		 * until interrupts are stopped in
		 * Target HW.
		 */
		htcadebug("Masking interrupts\n");
		hif_mask_interrupt(target->hif_handle);
		target->need_start_polling = 1;

		/* Kick off a register refresh so we
		 * use updated registers in order to
		 * figure out what needs to be serviced.
		 *
		 * RegisterRefresh completion wakes the
		 * work_task which re-enables Host-side
		 * interrupts.
		 */
		htca_register_refresh_start(target);
	} else { /* startup time */
		 /* Assumption is that we are receiving an interrupt
		  * because the Target made a TX Credit available
		  * on each endpoint (for configuration negotiation).
		  */

		hif_mask_interrupt(target->hif_handle);
		if (htca_negotiate_config(target)) {
			/* All endpoints are configured.
			 * Target is now ready for normal operation.
			 */
			/* TBDXXX - Fix Quartz-side and remove this */
			{
				/* HACK: Signal Target to read mbox Cfg info.
				 * TBD: Target should use EOM rather than an
				 * an explicit Target Interrupt for this.
				 */
				u8 my_targ_int;
				u32 address;
				int status;

				/* Set HTCA_INT_TARGET_INIT_HOST_REQ */
				my_targ_int = 1;

				address =
				    get_reg_addr(
					INT_TARGET_REG, ENDPOINT_UNUSED);
				status = hif_read_write(
				    target->hif_handle, address, &my_targ_int,
				    sizeof(my_targ_int), HIF_WR_SYNC_BYTE_INC,
				    NULL);
				if (WARN_ON(status != HIF_OK))
					return status;
			}
			target->ready = true;
			htcadebug("HTCA TARGET IS READY\n");
			wake_up(&target->target_init_wait);
		}
		hif_un_mask_interrupt(target->hif_handle);
	}
	return HTCA_OK;
}

/* Handler for CPU interrupts that are explicitly
 * initiated by Target firmware. Not used by system firmware today.
 */
void htca_service_cpu_interrupt(struct htca_target *target,
				struct htca_reg_request *req)
{
	int status;
	u32 address;
	u8 cpu_int_status;

	htcadebug("Enter\n");
	cpu_int_status = req->u.reg_table.status.cpu_int_status &
			 target->enb.cpu_int_status_enb;

	/* Clear pending interrupts on Target -- Write 1 to Clear */
	address = get_reg_addr(CPU_INT_STATUS_REG, ENDPOINT_UNUSED);

	status =
	    hif_read_write(target->hif_handle, address, &cpu_int_status,
			   sizeof(cpu_int_status), HIF_WR_SYNC_BYTE_INC, NULL);

	WARN_ON(status != HIF_OK);

	/* Handle cpu_int_status actions here. None are currently used */
}

/* Handler for error interrupts on Target.
 * If everything is working properly we hope never to see these.
 */
void htca_service_error_interrupt(struct htca_target *target,
				  struct htca_reg_request *req)
{
	int status = HIF_ERROR;
	u32 address;
	u8 err_int_status;
	struct htca_endpoint *end_point;

	htcadebug("Enter\n");
	err_int_status =
	    req->u.reg_table.status.err_int_status & target->enb.err_status_enb;

	end_point = &target->end_point[req->epid];
	htcadebug("epid=%d txCreditsAvailable=%d\n",
		  (int)req->epid, end_point->tx_credits_available);
	htcadebug("statusregs host=0x%02x cpu=0x%02x err=0x%02x cnt=0x%02x\n",
		  req->u.reg_table.status.host_int_status,
		  req->u.reg_table.status.cpu_int_status,
		  req->u.reg_table.status.err_int_status,
		  req->u.reg_table.status.counter_int_status);

	/* Clear pending interrupts on Target -- Write 1 to Clear */
	address = get_reg_addr(ERROR_INT_STATUS_REG, ENDPOINT_UNUSED);
	status =
	    hif_read_write(target->hif_handle, address, &err_int_status,
			   sizeof(err_int_status), HIF_WR_SYNC_BYTE_INC, NULL);

	if (WARN_ON(status != HIF_OK))
		return;

	if (ERROR_INT_STATUS_WAKEUP_GET(err_int_status)) {
		/* Wakeup */
		htcadebug("statusregs host=0x%x\n",
			  ERROR_INT_STATUS_WAKEUP_GET(err_int_status));
		/* Nothing needed here */
	}

	if (ERROR_INT_STATUS_RX_UNDERFLOW_GET(err_int_status)) {
		/* TBD: Rx Underflow */
		/* Host posted a read to an empty mailbox? */
		/* Target DMA was not able to keep pace with Host reads? */
		if (WARN_ON(2)) /* TBD */
			return;
	}

	if (ERROR_INT_STATUS_TX_OVERFLOW_GET(err_int_status)) {
		/* TBD: Tx Overflow */
		/* Host posted a write to a mailbox with no credits? */
		/* Target DMA was not able to keep pace with Host writes? */
		if (WARN_ON(1)) /* TBD */
			return;
	}
}

/* Handler for Credit Counter interrupts from Target.
 *
 * This occurs when the number of credits available on a mailbox
 * increases from 0 to non-zero. (i.e. when Target firmware queues a
 * DMA Receive buffer to an endpoint that previously had no buffers.)
 *
 * This interrupt is masked when we have a sufficient number of
 * credits available. It is unmasked only when we have reaped all
 * available credits and are still below a desired threshold.
 */
void htca_service_credit_counter_interrupt(struct htca_target *target,
					   struct htca_reg_request *req)
{
	struct htca_endpoint *end_point;
	u8 counter_int_status;
	u8 eps_with_credits;
	int ep;

	htcadebug("Enter\n");
	counter_int_status = req->u.reg_table.status.counter_int_status;

	/* Service the credit counter interrupt.
	 * COUNTER bits [4..7] are used for credits on endpoints [0..3].
	 */
	eps_with_credits =
	    counter_int_status & target->enb.counter_int_status_enb;
	htcadebug("eps_with_credits=0x%02x\n", eps_with_credits);
	htcadebug("counter_int_status=0x%02x\n", counter_int_status);
	htcadebug("counter_int_status_enb=0x%02x\n",
		  target->enb.counter_int_status_enb);

	for (ep = 0; ep < HTCA_NUM_MBOX; ep++) {
		if (!(eps_with_credits & (0x10 << ep)))
			continue;

		end_point = &target->end_point[ep];

		/* We need credits on this endpoint AND
		 * the target tells us that there are some.
		 * Start a credit refresh cycle on this
		 * endpoint.
		 */
		(void)htca_credit_refresh_start(end_point);
	}
}

/* Callback registered with HIF to be invoked when Target
 * presence is first detected.
 *
 * Allocate memory for Target, endpoints, requests, etc.
 */
int htca_target_inserted_handler(void *unused_context,
				 void *hif_handle)
{
	struct htca_target *target;
	struct htca_endpoint *end_point;
	int ep;
	struct htca_event_info event_info;
	struct htca_request_queue *send_free_queue, *recv_free_queue;
	struct htca_request_queue *reg_queue;
	u32 block_size[HTCA_NUM_MBOX];
	struct cbs_from_hif htca_callbacks; /* Callbacks from HIF to HTCA */
	int status = HTCA_OK;
	int i;

	htcadebug("Enter\n");

	target = kzalloc(sizeof(*target), GFP_KERNEL);
	/* target->ready = false; */

	/* Give a handle to HIF for this target */
	target->hif_handle = hif_handle;
	hif_set_handle(hif_handle, (void *)target);

	/* Register htca_callbacks from HIF */
	memset(&htca_callbacks, 0, sizeof(htca_callbacks));
	htca_callbacks.rw_completion_hdl = htca_rw_completion_handler;
	htca_callbacks.dsr_hdl = htca_dsr_handler;
	htca_callbacks.context = target;
	(void)hif_attach(hif_handle, &htca_callbacks);

	/* Get block sizes and start addresses for each mailbox */
	hif_configure_device(hif_handle,
			     HIF_DEVICE_GET_MBOX_BLOCK_SIZE, &block_size,
			     sizeof(block_size));

	/* Initial software copies of interrupt enables */
	target->enb.int_status_enb =
	    INT_STATUS_ENABLE_ERROR_MASK | INT_STATUS_ENABLE_CPU_MASK |
	    INT_STATUS_ENABLE_COUNTER_MASK | INT_STATUS_ENABLE_MBOX_DATA_MASK;

	/* All 8 CPU interrupts enabled */
	target->enb.cpu_int_status_enb = CPU_INT_STATUS_ENABLE_BIT_MASK;

	target->enb.err_status_enb = ERROR_STATUS_ENABLE_RX_UNDERFLOW_MASK |
				     ERROR_STATUS_ENABLE_TX_OVERFLOW_MASK;

	/* credit counters in upper bits */
	target->enb.counter_int_status_enb = COUNTER_INT_STATUS_ENABLE_BIT_MASK;

	spin_lock_init(&target->reg_queue_lock);
	spin_lock_init(&target->compl_queue_lock);
	spin_lock_init(&target->pending_op_lock);
	mutex_init(&target->task_mutex);

	status = htca_work_task_start(target);
	if (status != HTCA_OK)
		goto done;

	status = htca_compl_task_start(target);
	if (status != HTCA_OK)
		goto done;

	/* Initialize the register request free list */
	reg_queue = &target->reg_free_queue;
	for (i = 0; i < HTCA_REG_REQUEST_COUNT; i++) {
		struct htca_reg_request *reg_request;

		/* Add a reg_request to the Reg Free Queue */
		reg_request = kzalloc(sizeof(*reg_request), GFP_DMA);
		reg_request->req.target = target;
		reg_request->req.completion_cb = htca_reg_compl;

		/* no lock required -- startup */
		htca_request_enq_tail(reg_queue,
				      (struct htca_request *)reg_request);
	}

	/* Initialize endpoints, mbox queues and event tables */
	for (ep = 0; ep < HTCA_NUM_MBOX; ep++) {
		end_point = &target->end_point[ep];

		spin_lock_init(&end_point->tx_credit_lock);
		spin_lock_init(&end_point->mbox_queue_lock);

		end_point->tx_credits_available = 0;
		end_point->max_msg_sz = htca_msg_size[ep];
		end_point->rx_frame_length = 0;
		end_point->tx_credits_to_reap = false;
		end_point->target = target;
		end_point->enabled = false;
		end_point->block_size = block_size[ep];
		end_point->mbox_start_addr = MBOX_START_ADDR(ep);
		end_point->mbox_end_addr = MBOX_END_ADDR(ep);

		/* Initialize per-endpoint queues */
		end_point->send_pending_queue.head = NULL;
		end_point->send_pending_queue.tail = NULL;
		end_point->recv_pending_queue.head = NULL;
		end_point->recv_pending_queue.tail = NULL;

		send_free_queue = &end_point->send_free_queue;
		recv_free_queue = &end_point->recv_free_queue;
		for (i = 0; i < HTCA_MBOX_REQUEST_COUNT; i++) {
			struct htca_mbox_request *mbox_request;

			/* Add an mbox_request to the mbox SEND Free Queue */
			mbox_request = kzalloc(sizeof(*mbox_request),
					       GFP_KERNEL);
			mbox_request->req.target = target;
			mbox_request->req.completion_cb = htca_send_compl;
			mbox_request->end_point = end_point;
			htca_request_enq_tail(
			    send_free_queue,
			    (struct htca_request *)mbox_request);

			/* Add an mbox_request to the mbox RECV Free Queue */
			mbox_request = kzalloc(sizeof(*mbox_request),
					       GFP_KERNEL);
			mbox_request->req.target = target;
			mbox_request->req.completion_cb = htca_recv_compl;
			mbox_request->end_point = end_point;
			htca_request_enq_tail(
			    recv_free_queue,
			    (struct htca_request *)mbox_request);
		}
	}

	/* Target and endpoint structures are now completely initialized.
	 * Add the target instance to the global list of targets.
	 */
	htca_target_instance_add(target);

	/* Frame a TARGET_AVAILABLE event and send it to
	 * the caller. Return the hif_device handle as a
	 * parameter with the event.
	 */
	htca_frame_event(&event_info, (u8 *)hif_handle,
			 hif_get_device_size(),
			 hif_get_device_size(), HTCA_OK, NULL);
	htca_dispatch_event(target, ENDPOINT_UNUSED,
			    HTCA_EVENT_TARGET_AVAILABLE, &event_info);

done:
	return status;
}

/* Callback registered with HIF to be invoked when Target
 * is removed
 *
 * Also see htca_stop
 * Stop tasks
 * Free memory for Target, endpoints, requests, etc.
 *
 * TBD: Not yet supported
 */
int htca_target_removed_handler(void *unused_context,
				void *htca_handle)
{
	struct htca_target *target = (struct htca_target *)htca_handle;
	struct htca_event_info event_info;
	struct htca_endpoint *end_point;
	int ep;

	htcadebug("Enter\n");
	/* Disable each of the endpoints to stop accepting requests. */
	for (ep = 0; ep < HTCA_NUM_MBOX; ep++) {
		end_point = &target->end_point[ep];
		end_point->enabled = false;
	}

	if (target) {
		/* Frame a TARGET_UNAVAILABLE event and send it to the host */
		htca_frame_event(&event_info, NULL, 0, 0, HTCA_OK, NULL);
		htca_dispatch_event(target, ENDPOINT_UNUSED,
				    HTCA_EVENT_TARGET_UNAVAILABLE, &event_info);
	}

	/* TBD: call htca_stop? */
	/* TBD: Must be sure that nothing is going on before we free. */
	if (WARN_ON(1)) /* TBD */
		return HTCA_ERROR;

	/* Free everything allocated earlier, including target
	 * structure and all request structures.
	 */
	/* TBD: kfree .... */

	return HTCA_OK;
}
