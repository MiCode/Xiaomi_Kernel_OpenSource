// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
/*
 *
 * (C) COPYRIGHT 2019-2021 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 */

/*
 * Part of the Mali reference arbiter
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/freezer.h>
#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/completion.h>
#include <linux/export.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include <xen/xen.h>
#include <xen/xenbus.h>
#include <xen/events.h>
#include <xen/grant_table.h>
#include <xen/page.h>
#include <xen/interface/io/ring.h>
#include "mali_arb_vm_protocol.h"
#include "mali_arbif_adapter.h"

DEFINE_RING_TYPES(xengpu, uint64_t, uint64_t);

/* Forward Declaration */
static int send_command(struct xenbus_device *xdev, uint64_t payload);

/*
 * The LRU mechanism to clean the lists of persistent grants needs to
 * be executed periodically. The time interval between consecutive
 * executions of the purge mechanism is set in ms.
 */
#define LRU_INTERVAL 10000

#define NUM_PAGES_PER_RING 1
#define GPUDEV_RING_SIZE (NUM_PAGES_PER_RING * XEN_PAGE_SIZE)

/* INVALID_GRANT_REF has been added to xen/grant_table.h in v5.12-rc3. */
#ifndef INVALID_GRANT_REF
#define INVALID_GRANT_REF (0)
#endif

#define INVALID_EVTCHN (-1)

/**
 * enum handshake_state - Xenfront states
 * @HANDSHAKE_INIT:        The protocol handshake is being initialized.
 * @HANDSHAKE_IN_PROGRESS: The protocol handshake is in progress.
 * @HANDSHAKE_DONE:        The protocol handshake was done successfully.
 * @HANDSHAKE_FAILED:      The protocol handshake failed.
 *
 * Definition of the possible Resource Group's states
 */
enum handshake_state {
	HANDSHAKE_INIT,
	HANDSHAKE_IN_PROGRESS,
	HANDSHAKE_DONE,
	HANDSHAKE_FAILED,
};

/**
 * struct arbif_xenfront_device - Internal arbif xen front-end
 * @xdev:               Xen front-end device data
 * @evtchn:             Event used to received interrupts from Xen
 * @irq:                IRQ number for notifying remote
 * @gnt_ref1:           Xen grant table reference for ring 1
 * @gnt_ref2:           Xen grant table reference for ring 2
 * @ring_lock:          Ring1 spinlock for disabling and restoring interrupts
 * @ring1:              Ring containing messages to send to Xen backend
 * @ring2:              Ring containing messages received form Xen back-end
 * @comms_dev:          Callback functions between Adapter and Frontend
 * @adapter_pdev:       Adapter platform device
 * @arbif_wq:           Workqueue for receiving commands from back-end
 * @arbif_req_work:     Work item for arbif_wq to process commands
 * @xf_handshake_state: State of protocal handshake with backend
 * @handshake_mutex:    Mutex to protect against accessing handshake state
 * @gpu_requested:      Handshake initiated
 * @version_init_reply: Stores version for handshake
 */
struct arbif_xenfront_device {
	struct xenbus_device     *xdev;
	unsigned int             evtchn;
	int                      irq;
	grant_ref_t              gnt_ref1;
	grant_ref_t              gnt_ref2;
	spinlock_t               ring_lock;
	struct xengpu_front_ring ring1;
	struct xengpu_back_ring  ring2;
	struct arbif_comms_dev   comms_dev;
	struct platform_device   *adapter_pdev;
	struct workqueue_struct  *arbif_wq;
	struct work_struct       arbif_req_work;
	enum handshake_state     xf_handshake_state;
	struct mutex             handshake_mutex;
	bool                     gpu_requested;
	uint8_t                  version_init_reply;
};

/*
 * Adapter to Frontend
 */

/**
 * get_handshake_state() - Get Xenfront handshake state
 * @front: Internal frontend data.
 * @state: State to be set with handshake state
 *
 * Get Xenfront handshake state in a safe way.
 *
 * Return: 0 if successful, otherwise a standard Linux error code.
 */
static int get_handshake_state(struct arbif_xenfront_device *front,
		enum handshake_state *state)
{

	if (!front || !state)
		return -EINVAL;

	mutex_lock(&front->handshake_mutex);
	*state = front->xf_handshake_state;
	mutex_unlock(&front->handshake_mutex);

	return 0;
}

/**
 * set_handshake_state() - Set Xenfront handshake state
 * @front: Internal frontend data.
 * @state: State to set handshake state with.
 *
 * Set Xenfront handshake state in a safe way.
 *
 * Return: 0 if successful, otherwise a standard Linux error code.
 */
static int set_handshake_state(struct arbif_xenfront_device *front,
		enum handshake_state state)
{

	if (!front)
		return -EINVAL;

	mutex_lock(&front->handshake_mutex);
	front->xf_handshake_state = state;
	mutex_unlock(&front->handshake_mutex);

	return 0;
}

/**
 * arbif_gpu_request() - Send a GPU request to the back-end
 * @fe_priv: Frontend private data.
 *
 * Sends a GPU_REQUEST message to the Arbiter
 */
static void arbif_gpu_request(void *fe_priv)
{
	struct device *dev = fe_priv;
	struct arbif_xenfront_device *front = dev_get_drvdata(dev);
	uint64_t payload = 0;
	uint8_t priority = 0; /* TODO: not implemented yet see AUTOSW-358 */
	enum handshake_state state;

	if (WARN_ON(!front))
		return;

	get_handshake_state(front, &state);
	switch (state) {
	case HANDSHAKE_IN_PROGRESS:
		vm_arb_init_build_msg(1, front->version_init_reply, 1,
				&payload);
		send_command(front->xdev, payload);
		set_handshake_state(front, HANDSHAKE_DONE);
		return;
	case HANDSHAKE_INIT:
		pr_debug("hanshake has not completed.");
		pr_debug("request will be processed when handshake is done\n");
		front->gpu_requested = 1;
		return;
	case HANDSHAKE_FAILED:
		pr_err("can't sent gpu request. handshake has failed\n");
		return;
	default:
		break;
	}

	vm_arb_gpu_request_build_msg(priority, &payload);
	send_command(front->xdev, payload);
}

/**
 * arbif_gpu_active() - Send a GPU active to the back-end
 * @fe_priv: Frontend private data.
 *
 * Sends a GPU_ACTIVE message to the Arbiter
 */
static void arbif_gpu_active(void *fe_priv)
{
	struct device *dev = fe_priv;
	struct arbif_xenfront_device *front = dev_get_drvdata(dev);
	uint64_t payload = 0;
	uint8_t priority = 0; /* TODO: not implemented yet see AUTOSW-358 */
	enum handshake_state state;

	if (WARN_ON(!front))
		return;

	get_handshake_state(front, &state);
	if (state != HANDSHAKE_DONE) {
		pr_err("Message not sent. Handshake not completed or failed\n");
		return;
	}

	vm_arb_gpu_active_build_msg(priority, &payload);
	send_command(front->xdev, payload);
}

/**
 * arbif_gpu_idle() - Send a GPU idle to the back-end
 * @fe_priv: Frontend private data.
 *
 * Sends a GPU_IDLE message to the Arbiter
 */
static void arbif_gpu_idle(void *fe_priv)
{
	struct device *dev = fe_priv;
	struct arbif_xenfront_device *front = dev_get_drvdata(dev);
	uint64_t payload = 0;
	enum handshake_state state;

	if (WARN_ON(!front))
		return;

	get_handshake_state(front, &state);
	if (state != HANDSHAKE_DONE) {
		pr_err("Message not sent. Handshake not completed or failed\n");
		return;
	}

	vm_arb_gpu_idle_build_msg(&payload);
	send_command(front->xdev, payload);
}

/**
 * arbif_on_init() - Callback when ARB_VM_INIT have been received
 * @xfdev:	Xen Frontend device.
 * @payload:	Payload of message received
 *
 * Send the VM_ARB_INIT reply If gpu have already been requested otherwise
 * save reply to be send when gpu is requested
 */
static void arbif_on_init(struct arbif_xenfront_device *xfdev,
		uint64_t payload)
{
	uint8_t ack;
	uint8_t version;
	uint64_t payload_reply;

	get_msg_protocol_version(&payload, &version);
	get_msg_init_ack(&payload, &ack);

	if (version < MIN_SUPPORTED_VERSION) {
		xfdev->version_init_reply = MIN_SUPPORTED_VERSION;
		set_handshake_state(xfdev, HANDSHAKE_FAILED);
		pr_err("Incompatible protocol version. Handshake has failed\n");
	} else {
		xfdev->version_init_reply = min(CURRENT_VERSION, version);
		if (!xfdev->gpu_requested) {
			set_handshake_state(xfdev, HANDSHAKE_IN_PROGRESS);
			return;
		}
		set_handshake_state(xfdev, HANDSHAKE_DONE);
	}

	vm_arb_init_build_msg(1, xfdev->version_init_reply,
			xfdev->gpu_requested, &payload_reply);
	send_command(xfdev->xdev, payload_reply);

	xfdev->gpu_requested = 0;
}

/**
 * arbif_gpu_stopped() - Send GPU stop to the back-end
 * @fe_priv: Frontend private data.
 * @gpu_required: The GPU is still needed to do more work.
 *
 * Sends a GPU_STOPPED message to the Arbiter.
 */
static void arbif_gpu_stopped(void *fe_priv, u8 gpu_required)
{
	struct device *dev = fe_priv;
	struct arbif_xenfront_device *front = dev_get_drvdata(dev);
	uint64_t payload = 0;
	enum handshake_state state;

	if (WARN_ON(!front))
		return;

	get_handshake_state(front, &state);
	if (state != HANDSHAKE_DONE) {
		pr_err("Message not sent. Handshake not completed or failed\n");
		return;
	}

	vm_arb_gpu_stopped_build_msg(gpu_required, &payload);
	send_command(front->xdev, payload);
}

/**
 * arbif_xenfront_xen_event_handler() - Xen front-end event handler
 * @irq: IRQ number (not used)
 * @dev: Xen front-end device
 *
 * Wake up processes that are waiting on the wait queue
 *
 * Return: IRQ_HANDLED
 */
irqreturn_t arbif_xenfront_xen_event_handler(int irq, void *dev)
{
	struct arbif_xenfront_device *xfdev = dev;

	queue_work(xfdev->arbif_wq, &xfdev->arbif_req_work);
	return IRQ_HANDLED;
}

/**
 * send_command() - Send command to Xen backend arbiter
 * @xdev: Xen bus device
 * @payload: Message content to be sent.
 *
 * Pack a message including all the required fields and send it to the back-end
 *
 * Return: 0 Always
 */
static int send_command(struct xenbus_device *xdev,
			 uint64_t payload)
{
	struct arbif_xenfront_device *xfdev = dev_get_drvdata(&xdev->dev);
	struct xengpu_front_ring *ring = &xfdev->ring1;
	int notify = 0;
	unsigned long flags;
	uint64_t *req;

	if (xdev->state != XenbusStateConnected) {
		dev_warn(&xdev->dev,
			"Command sent whilst not connected to backend\n");
	}

	ring->rsp_cons = ring->sring->rsp_prod;
	spin_lock_irqsave(&xfdev->ring_lock, flags);
	req = RING_GET_REQUEST(ring, ring->req_prod_pvt);
	ring->req_prod_pvt++;
	*req = payload;
	RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(ring, notify);
	if (notify)
		notify_remote_via_irq(xfdev->irq);

	spin_unlock_irqrestore(&xfdev->ring_lock, flags);
	return 0;
}

/**
 * arbif_xenfront_on_command() - Process backend commands
 * @xfdev: Xen front-end internal data
 * @payload: Payload of message received
 *
 * Given a backed command id, parse it and dispatch the message calling
 * the appropriate arbif callback
 */
static void arbif_xenfront_on_command(struct arbif_xenfront_device *xfdev,
			uint64_t payload)
{
	struct arbif_comms_dev *comms_dev = &xfdev->comms_dev;
	uint8_t cmd_req;
	uint32_t freq;

	if (!comms_dev)
		return;

	get_msg_id(&payload, &cmd_req);
	get_msg_frequency(&payload, &freq);

	switch (cmd_req) {
	case ARB_VM_GPU_GRANTED:
		if (comms_dev->fe_ad_ops.arb_vm_gpu_granted)
			comms_dev->fe_ad_ops.arb_vm_gpu_granted(
					comms_dev->ad_priv, freq);
		break;
	case ARB_VM_GPU_STOP:
		if (comms_dev->fe_ad_ops.arb_vm_gpu_stop)
			comms_dev->fe_ad_ops.arb_vm_gpu_stop(comms_dev->ad_priv);
		break;
	case ARB_VM_GPU_LOST:
		if (comms_dev->fe_ad_ops.arb_vm_gpu_lost)
			comms_dev->fe_ad_ops.arb_vm_gpu_lost(comms_dev->ad_priv);
		break;
	case ARB_VM_INIT:
		arbif_on_init(xfdev, payload);
		break;

	default:
		pr_err("Unknown command\n");
	}
}

/**
 * process_host_requests() - Reads commands off the RX ring
 * @xfdev: Internal front-end data.
 *
 * Return: 0 if ring is empty, 1 if more commands are present
 */
static int process_host_requests(struct arbif_xenfront_device *xfdev)
{
	int more_to_do;
	struct xengpu_back_ring *ring = &xfdev->ring2;
	uint64_t *req;
	RING_IDX rc, rp;

	rc = ring->req_cons;
	rp = ring->sring->req_prod;
	rmb(); /* Ensure we see queued requests up to 'rp'. */

	while (rc != rp) {
		req = RING_GET_REQUEST(ring, rc);
		arbif_xenfront_on_command(xfdev, *req);
		ring->req_cons = ++rc;
		ring->rsp_prod_pvt++;  /* Not using responses */
	}
	RING_FINAL_CHECK_FOR_REQUESTS(ring, more_to_do);
	if (more_to_do) {
		/* This will cause this function to run again */
		return 1;
	}
	return 0;
}

/**
 * arbif_process_requests() - Worker thread for processing RX commands
 * @data: Work contained within front-end data.
 */
static void arbif_process_requests(struct work_struct *data)
{
	struct arbif_xenfront_device *xfdev = container_of(data,
		struct arbif_xenfront_device, arbif_req_work);
	int ret = process_host_requests(xfdev);

	if (ret > 0)
		queue_work(xfdev->arbif_wq, &xfdev->arbif_req_work);
}

/**
 * free_ring() - free allocated shared ring
 * @xfdev: Internal front-end data
 * @grantref: Pointer to grant reference
 * @sring: Pointer to shared ring
 */
static void free_ring(struct arbif_xenfront_device *xfdev,
	grant_ref_t *grantref,
	struct xengpu_sring **sring)
{
	if ((*grantref) != INVALID_GRANT_REF) {
		gnttab_end_foreign_access(*grantref, 0 /* r/w page */,
			(unsigned long)(*sring));
		*grantref = INVALID_GRANT_REF;
	}

	if (*sring) {
		free_pages((unsigned long)*sring, get_order(GPUDEV_RING_SIZE));
		*sring = NULL;
	}
}

/**
 * alloc_ring() - Allocate shared ring
 * @xfdev: Internal front-end data
 * @grantref: Grant reference to be filled in
 * @sring_out: Allocated shared ring to be filled in
 *
 * Return: 0 if successful, otherwise a standard Linux error code
 */
static int alloc_ring(struct arbif_xenfront_device *xfdev,
	grant_ref_t *grantref,
	struct xengpu_sring **sring_out)
{
	grant_ref_t gref = INVALID_GRANT_REF;
	int err = 0;
	struct xengpu_sring *sring;

	/* set up shared page */
	sring = (void *)__get_free_pages(GFP_KERNEL | __GFP_ZERO,
		get_order(GPUDEV_RING_SIZE));
	if (!sring) {
		xenbus_dev_fatal(xfdev->xdev, -ENOMEM,
			"allocating shared ring");
		err = -ENOMEM;
		goto error;
	}

	SHARED_RING_INIT(sring);

	err = xenbus_grant_ring(xfdev->xdev, sring,
		NUM_PAGES_PER_RING, &gref);
	if (err < 0) {
		xenbus_dev_fatal(xfdev->xdev, err, "allocating shared ring");
		goto cleanup_get_pages;
	}
	*grantref = gref;
	*sring_out = sring;
	return 0;

cleanup_get_pages:
	free_pages((unsigned long)sring, get_order(GPUDEV_RING_SIZE));
error:
	*grantref = INVALID_GRANT_REF;
	*sring_out = NULL;
	return err;
}

/**
 * free_xfdev() - Cleans up resources
 * @xfdev: Internal front-end data.
 */
static void free_xfdev(struct arbif_xenfront_device *xfdev)
{
	dev_dbg(&xfdev->xdev->dev, "freeing xfdev @ 0x%p\n", xfdev);

	if (xfdev->irq >= 0)
		unbind_from_irqhandler(xfdev->irq, xfdev);

	if (xfdev->evtchn != INVALID_EVTCHN)
		xenbus_free_evtchn(xfdev->xdev, xfdev->evtchn);

	if (xfdev->gnt_ref2 != INVALID_GRANT_REF)
		free_ring(xfdev, &xfdev->gnt_ref2, &xfdev->ring2.sring);

	if (xfdev->gnt_ref1 != INVALID_GRANT_REF)
		free_ring(xfdev, &xfdev->gnt_ref1, &xfdev->ring1.sring);

	if (xfdev->arbif_wq) {
		destroy_workqueue(xfdev->arbif_wq);
		xfdev->arbif_wq = NULL;
	}

	dev_set_drvdata(&xfdev->xdev->dev, NULL);
	kfree(xfdev);
}

/**
 * alloc_xfdev() - Allocate internal front-end data
 * @xdev: Xen bus device data.
 *
 * Return: Internal front-end data.
 */
static struct arbif_xenfront_device *alloc_xfdev(struct xenbus_device *xdev)
{
	struct arbif_xenfront_device *xfdev;
	int err = 0;

	xfdev = kzalloc(sizeof(struct arbif_xenfront_device), GFP_KERNEL);
	if (xfdev == NULL) {
		xenbus_dev_fatal(xdev, -ENOMEM, "allocating device");
		return NULL;
	}

	dev_set_drvdata(&xdev->dev, xfdev);
	xfdev->xdev = xdev;

	spin_lock_init(&xfdev->ring_lock);
	xfdev->arbif_wq = alloc_ordered_workqueue("arb_front_wq",
		WQ_HIGHPRI);
	if (!xfdev->arbif_wq) {
		xenbus_dev_fatal(xdev, -EFAULT,
			"Failed to allocate front arbif_wq");
		xfdev->arbif_wq = NULL;
		goto cleanup_pdev;
	}
	INIT_WORK(&xfdev->arbif_req_work, arbif_process_requests);

	err = alloc_ring(xfdev, &xfdev->gnt_ref1, &xfdev->ring1.sring);
	if (err)
		goto cleanup_pdev;

	FRONT_RING_INIT(&xfdev->ring1, xfdev->ring1.sring, GPUDEV_RING_SIZE);

	err = alloc_ring(xfdev, &xfdev->gnt_ref2, &xfdev->ring2.sring);
	if (err)
		goto cleanup_pdev;

	BACK_RING_INIT(&xfdev->ring2, xfdev->ring2.sring, GPUDEV_RING_SIZE);

	/* Create event channel for this xenbus device */
	err = xenbus_alloc_evtchn(xdev, &xfdev->evtchn);
	if (err)
		goto cleanup_pdev;

	/* bind event channel to our event handler */
	err = bind_evtchn_to_irqhandler(xfdev->evtchn,
			arbif_xenfront_xen_event_handler, 0, "gpufront", xfdev);
	if (err < 0)
		goto cleanup_pdev;

	xfdev->irq = err;

	/* Configure Adapter to Frontend callbacks */
	xfdev->comms_dev.ad_fe_ops.vm_arb_gpu_request = arbif_gpu_request;
	xfdev->comms_dev.ad_fe_ops.vm_arb_gpu_active = arbif_gpu_active;
	xfdev->comms_dev.ad_fe_ops.vm_arb_gpu_idle = arbif_gpu_idle;
	xfdev->comms_dev.ad_fe_ops.vm_arb_gpu_stopped = arbif_gpu_stopped;
	xfdev->comms_dev.fe_priv = &xfdev->xdev->dev;

	return xfdev;

cleanup_pdev:
	free_xfdev(xfdev);
	return NULL;
}

/**
 * arbif_xenfront_publish_info() - Write configuration data to xen state
 * @xfdev: Internal front-end data.
 *
 * Return: 0 if successful , or an error code
 */
static int arbif_xenfront_publish_info(struct arbif_xenfront_device *xfdev)
{
	int err = 0;
	struct xenbus_transaction trans;

do_publish:
	err = xenbus_transaction_start(&trans);
	if (err) {
		xenbus_dev_fatal(xfdev->xdev, err,
			"Error writing configuration for backend 1(start transaction)");
		goto cleanup_exit;
	}

	err = xenbus_printf(trans, xfdev->xdev->nodename,
				"ring-ref-g2h", "%u", xfdev->gnt_ref1);

	if (err) {
		xenbus_dev_fatal(xfdev->xdev, err,
				"Error writing ring-ref-g2h");
		goto cleanup_transaction;
	}

	err = xenbus_printf(trans, xfdev->xdev->nodename,
				"ring-ref-h2g", "%u", xfdev->gnt_ref2);

	if (err) {
		xenbus_dev_fatal(xfdev->xdev, err,
				"Error writing ring-ref-h2g");
		goto cleanup_transaction;
	}

	err = xenbus_printf(trans, xfdev->xdev->nodename,
				"event-channel", "%u", xfdev->evtchn);
	if (err) {
		xenbus_dev_fatal(xfdev->xdev, err,
				"Error writing configuration for backend 2");
		goto cleanup_transaction;
	}
	err = xenbus_transaction_end(trans, 0);
	if (err == -EAGAIN)
		goto do_publish;
	else {
		if (err) {
			xenbus_dev_fatal(xfdev->xdev, err,
				"Error completing transaction for backend");
			goto cleanup_exit;
		}
	}

	return 0;

cleanup_transaction:
	xenbus_transaction_end(trans, 1);

cleanup_exit:
	return err;
}

/**
 * backend_changed() - Called when Xen backend state changes
 * @xdev: Xen bus device data.
 * @backend_state: New state
 *
 * Processes state change and updates front-end state when required
 */
static void backend_changed(struct xenbus_device *xdev,
			enum xenbus_state backend_state)
{
	struct arbif_xenfront_device *xfdev = dev_get_drvdata(&xdev->dev);

	dev_dbg(&xfdev->xdev->dev, "Backend changed, BE = %s, FE = %s\n",
			xenbus_strstate(backend_state),
			xenbus_strstate(xdev->state));

	switch (backend_state) {
	case XenbusStateInitialising:
	case XenbusStateInitialised:
	case XenbusStateReconfiguring:
	case XenbusStateReconfigured:
	case XenbusStateUnknown:
		break;

	case XenbusStateInitWait:
		if (xdev->state == XenbusStateClosed)
			xenbus_switch_state(xdev, XenbusStateInitialising);

		if (xdev->state != XenbusStateInitialising)
			break;

		xenbus_switch_state(xdev, XenbusStateInitialised);
		break;

	case XenbusStateConnected:
		xenbus_switch_state(xdev, XenbusStateConnected);
		dev_info(&xdev->dev,
			"Connected to backend device.\n");
		arbif_adapter_init_comms(xfdev->adapter_pdev,
					&xfdev->comms_dev);
		break;

	case XenbusStateClosed:
		if (xdev->state == XenbusStateClosed)
			break;

	/* Missed the back-end's CLOSING state */
	/* fallthrough */
	case XenbusStateClosing:
		set_handshake_state(xfdev, HANDSHAKE_INIT);
		flush_work(&xfdev->arbif_req_work);
		xenbus_frontend_closed(xdev);
		arbif_adapter_deinit_comms(xfdev->adapter_pdev,
					&xfdev->comms_dev);
	}
}

/**
 * arbif_xenfront_probe() - Initializes and allocates device resources
 * @xdev: Xen bus device data.
 * @id: Matches device ID (not used)
 *
 * Return: 0 if successful, otherwise a standard Linux error code
 */
static int arbif_xenfront_probe(struct xenbus_device *xdev,
		const struct xenbus_device_id *id)
{
	int err = 0;
	struct arbif_xenfront_device *xfdev;
	struct device_node *node;
	struct platform_device *pdev;

	xenbus_switch_state(xdev, XenbusStateInitialising);

	xfdev = alloc_xfdev(xdev);
	if (xfdev == NULL) {
		err = -ENOMEM;
		xenbus_dev_fatal(xdev, err,
			"Error allocating arbif_xenfront_device struct");
		return err;
	}
	/* write config info to xen state */
	err = arbif_xenfront_publish_info(xfdev);
	if (err) {
		pr_err("error in probe.\n");
		free_xfdev(xfdev);
		return err;
	}

	node = of_find_compatible_node(NULL, NULL, ARBITER_IF_DT_NAME);
	if (!node) {
		err = -ENODEV;
		xenbus_dev_fatal(xdev, err,
			"mali_arbif node missing from Device Tree\n");
		return err;
	}

	pdev = of_find_device_by_node(node);
	if (!pdev) {
		err = -ENODEV;
		dev_warn(&xdev->dev, "mali_arbif_adapter device not loaded\n");
		return err;
	}

	xfdev->adapter_pdev = pdev;

	/* Increase the device reference count */
	get_device(&xfdev->adapter_pdev->dev);

	return 0;
}

/**
 * arbif_xenfront_remove() - Cleans up resources for device remove
 * @xdev: Xen bus device data.
 *
 * Return: 0 always
 */
static int arbif_xenfront_remove(struct xenbus_device *xdev)
{
	struct arbif_xenfront_device *xfdev = dev_get_drvdata(&xdev->dev);

	if (xfdev) {
		flush_work(&xfdev->arbif_req_work);
		arbif_adapter_deinit_comms(xfdev->adapter_pdev,
			&xfdev->comms_dev);
		/* Decrease the device reference count */
		put_device(&xfdev->adapter_pdev->dev);
		xfdev->adapter_pdev = NULL;
	}
	return 0;
}

/*
 * arbif_xenfront_ids: Match the xenbus device with the backend.
 */
static const struct xenbus_device_id arbif_xenfront_ids[] = {
	{ "mali_arbif_xen"  },
	{ ""  }
};

/*
 * arbif_xenfront_driver: Xenbus driver data
 */
static struct xenbus_driver arbif_xenfront_driver = {
	.ids  = arbif_xenfront_ids,
	.probe = arbif_xenfront_probe,
	.remove = arbif_xenfront_remove,
	.otherend_changed = backend_changed
};

/**
 * arbif_xenfront_init - Register xenbus driver
 */
static int __init arbif_xenfront_init(void)
{
	int ret;

	ret = arbif_adapter_register();
	if (ret)
		return ret;

	return xenbus_register_frontend(&arbif_xenfront_driver);
}
module_init(arbif_xenfront_init);

/**
 * arbif_xenfront_exit() - Unregister xenbus driver
 */
static void __exit arbif_xenfront_exit(void)
{
	xenbus_unregister_driver(&arbif_xenfront_driver);
	arbif_adapter_unregister();
}
module_exit(arbif_xenfront_exit);

MODULE_DESCRIPTION("Xen front-end reference driver to support VM GPU arbitration");
MODULE_LICENSE("GPL");
MODULE_ALIAS("xengpufront");
MODULE_ALIAS("xen:gputs");
