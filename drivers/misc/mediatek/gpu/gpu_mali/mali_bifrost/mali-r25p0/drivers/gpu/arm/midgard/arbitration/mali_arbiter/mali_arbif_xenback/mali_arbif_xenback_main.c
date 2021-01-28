// SPDX-License-Identifier: GPL-2.0

/*
 *
 * (C) COPYRIGHT 2019-2020 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */

/**
 * @file
 * Part of the Mali reference arbiter
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/freezer.h>
#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/of.h>
#include <linux/of_platform.h>

#include <xen/xen.h>
#include <xen/xenbus.h>

#include <xen/grant_table.h>
#include <xen/events.h>
#include <xen/page.h>
#include <xen/platform_pci.h>

#include <xen/interface/grant_table.h>

#include <xen/interface/io/ring.h>
#include <xen/interface/grant_table.h>
#include <xen/interface/io/mali_arbif_xen.h>
#include <xen/interface/mali_xen_hyp.h>
#include <asm/xen/hypercall.h>

#include "../mali_arbiter.h"
#include "mali_arbif_xenback_hypercall.h"

/**
 * struct arbif_xenback - Internal arbif xen backend state
 * @xdev:         Xen backend device data.
 * @domid:        Domain ID.
 * @irq:          Used to send interrupts to VM
 * @ring1:        Ring containing messages received from VM
 * @ring2:        Ring containing messages to send to VM
 * @evtchn:       Event used to received interrupts from Xen
 * @gref1:        Xen grant table reference for ring 1
 * @gref2:        Xen grant table reference for ring 2
 * @xenschd:      Listener thread
 * @arb_ops:      Arbiter function callbacks (valid if arb_dom is not
 *                NULL)
 * @arb_dom:      Internal arbiter domain data or NULL if arbiter is
 *                not available
 * @arbif_wq:     Workqueue for receiving commands from front-end
 * @arbif_req_work: Work item for arbif_wq to process commands
 *
 * A new device is created for each VM instance
 */
struct arbif_xenback {
	struct xenbus_device        *xdev;
	domid_t                     domid;
	unsigned int                irq;
	struct xengpu_back_ring     ring1;
	struct xengpu_front_ring    ring2;
	unsigned int                evtchn;
	grant_ref_t                 gref1;
	grant_ref_t                 gref2;
	struct task_struct          *xenschd;
	struct mali_arb_ops         *arb_ops;
	struct mali_arb_dom         *arb_dom;
	struct workqueue_struct     *arbif_wq;
	struct work_struct          arbif_req_work;
};

struct module *arb_module;
struct mali_arb_dev *arb_dev;

/* Forward Declaration */
static void disconnect_rings(struct arbif_xenback *back);

/**
 * send_command() - Send command to the VM front-end
 * @back: Internal backend data.
 * @req_id: Command ID to send.
 * @param0: Parameter 0 (command specific).
 * @param1: Parameter 1 (command specific).
 * @param2: Parameter 2 (command specific).
 *
 * Send one of the commands defined in mali_arbif_common.h.
 */
static void send_command(struct arbif_xenback *back, uint16_t req_id,
	uint64_t param0, uint64_t param1, uint64_t param2)
{
	struct xengpu_front_ring *ring = &back->ring2;
	struct xengpu_request *req;
	int notify = 0;

	ring->rsp_cons = ring->sring->rsp_prod;
	req = RING_GET_REQUEST(ring, ring->req_prod_pvt);
	ring->req_prod_pvt++;
	req->cmd = req_id;
	req->cmd_param0 = param0;
	req->cmd_param1 = param1;
	req->cmd_param2 = param2;
	RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(ring, notify);
	if (notify)
		notify_remote_via_irq(back->irq);
}

/**
 * on_gpu_granted() - GPU has been granted by the arbiter
 * @dev: Xen backend device.
 *
 * Sends the ARB_VM_GPU_GRANTED to the front-end.
 */
static void on_gpu_granted(struct device *dev)
{
	struct arbif_xenback *back = dev_get_drvdata(dev);

	dev_dbg(dev, "%s %u\n", __func__, back->domid);
	send_command(back, ARB_VM_GPU_GRANTED, 0, 0, 0);
}

/**
 * on_gpu_stop() - Arbiter requires VM to stop using GPU
 * @dev: Xen backend device.
 *
 * Sends the ARB_VM_GPU_STOP to the front-end.
 */
static void on_gpu_stop(struct device *dev)
{
	struct arbif_xenback *back = dev_get_drvdata(dev);

	dev_dbg(dev, "%s %u\n", __func__, back->domid);
	send_command(back, ARB_VM_GPU_STOP, 0, 0, 0);
}

/**
 * on_gpu_lost() - Sends message to VM that GPU has been removed
 * @dev: Xen backend device.
 *
 * Sends the ARB_VM_GPU_LOST to the front-end.
 */
static void on_gpu_lost(struct device *dev)
{
	struct arbif_xenback *back = dev_get_drvdata(dev);

	dev_alert(dev, "%s %u\n", __func__, back->domid);
	send_command(back, ARB_VM_GPU_LOST, 0, 0, 0);
}

/**
 * find_arbiter_dev() - Uses Device Tree to find the Arbiter device
 *
 * Finds and connects to the arbiter device from the device tree.
 *
 * Return: 0 if successful, otherwise a standard Linux error code.
 */
static int find_arbiter_device(void)
{
	int ret;
	struct device_node *node;
	struct platform_device *pdev;

	node = of_find_compatible_node(NULL, NULL, MALI_ARBITER_DT_NAME);
	if (!node) {
		pr_err("Error Mali Arbiter node missing from Device Tree\n");
		return -ENODEV;
	}
	pdev = of_find_device_by_node(node);
	if (!pdev) {
		dev_err(&(pdev->dev), "Error Mali Arbiter not loaded\n");
		return -ENODEV;
	}

	if (!pdev->dev.driver || !pdev->dev.driver->owner ||
			!try_module_get(pdev->dev.driver->owner)) {
		dev_err(&(pdev->dev), "Error Arbiter device not available\n");
		return -EPROBE_DEFER;
	}

	arb_module = pdev->dev.driver->owner;
	arb_dev = platform_get_drvdata(pdev);
	if (!arb_dev) {
		ret = -ENODEV;
		dev_err(&(pdev->dev), "Error Mali Arbiter not ready\n");
		goto cleanup_module;
	}
	return 0;

cleanup_module:
	module_put(arb_module);
	arb_dev = NULL;
	arb_module = NULL;
	return ret;
}

/**
 * register_domain() - Connects the xen backend (domain) device to the arbiter
 * @back: Internal backend data.
 *
 * Finds and connects to the arbiter device from the Device Tree
 *
 * Return: 0 if successful, otherwise a standard Linux error code.
 */
static int register_domain(struct arbif_xenback *back)
{
	int ret;
	struct mali_arb_dom_info arb_info;
	struct xenbus_device *xdev = back->xdev;

	if (!arb_dev->ops.vm_arb_register_dom) {
		ret = -ENODEV;
		xenbus_dev_fatal(xdev, ret,
			"Error Mali Arbiter does not support register op\n");
		return ret;
	}

	arb_info.dev = &back->xdev->dev;
	arb_info.domain_id = back->domid;
	arb_info.flags = 0;
	arb_info.arb_vm_gpu_granted = on_gpu_granted;
	arb_info.arb_vm_gpu_stop = on_gpu_stop;
	arb_info.arb_vm_gpu_lost = on_gpu_lost;
	ret = arb_dev->ops.vm_arb_register_dom(arb_dev, &arb_info,
		&back->arb_dom);
	if (ret) {
		xenbus_dev_fatal(xdev, ret,
			"Error Mali Arbiter registration failed\n");
		return ret;
	}
	back->arb_ops = &arb_dev->ops;
	return 0;
}

/**
 * unregister_domain() - Disconnect and unregister domain from the arbiter
 * @back: Internal backend data.
 *
 * Unregister the domain and initialize to NULL the related references
 */
static void unregister_domain(struct arbif_xenback *back)
{
	if (back->arb_ops && back->arb_ops->vm_arb_unregister_dom)
		back->arb_ops->vm_arb_unregister_dom(back->arb_dom);
	back->arb_dom = NULL;
	back->arb_ops = NULL;
}

/**
 * process_msg() - Dispatches a message received from Xen front-end
 * @back: Internal backend data.
 * @cmd_req: Command ID received.
 * @cmd_param0: Parameter 0 (command specific).
 * @cmd_param1: Parameter 1 (command specific).
 * @cmd_param2: Parameter 2 (command specific).
 *
 * Dispatch a message received according the command ID received and
 * trigger the corresponding VM arbiter operation
 */
static void process_msg(struct arbif_xenback *back, uint16_t cmd_req,
	uint64_t cmd_param0, uint64_t cmd_param1, uint64_t cmd_param2)
{
	if (!back->arb_ops)
		return;

	switch (cmd_req) {
	case VM_ARB_GPU_REQUEST:
		dev_dbg(&back->xdev->dev, "recv VM_ARB_GPU_REQUEST %u\n",
			back->domid);
		if (back->arb_ops->vm_arb_gpu_request)
			back->arb_ops->vm_arb_gpu_request(back->arb_dom);
		break;
	case VM_ARB_GPU_STOPPED:
		dev_dbg(&back->xdev->dev,
			"recv VM_ARB_GPU_STOPPED %u, %lld, %lld, %lld\n",
			back->domid, cmd_param0, cmd_param1, cmd_param2);
		if (back->arb_ops->vm_arb_gpu_stopped)
			back->arb_ops->vm_arb_gpu_stopped(back->arb_dom,
			(cmd_param0 != 0 ? true : false));
		break;
	case VM_ARB_GPU_IDLE:
		dev_dbg(&back->xdev->dev, "recv VM_ARB_GPU_IDLE %u\n",
				back->domid);
		if (back->arb_ops->vm_arb_gpu_idle)
			back->arb_ops->vm_arb_gpu_idle(back->arb_dom);
		break;
	case VM_ARB_GPU_ACTIVE:
		dev_dbg(&back->xdev->dev, "recv VM_ARB_GPU_ACTIVE %u\n",
			back->domid);
		if (back->arb_ops->vm_arb_gpu_active)
			back->arb_ops->vm_arb_gpu_active(back->arb_dom);
		break;
	default:
		dev_warn(&back->xdev->dev,
			"Unexpected request from FE %u (dom %u)\n", cmd_req,
			back->domid);
		break;
	}
}

/**
 * process_recv_ring() - Reads commands off the RX ring
 * @back: Internal backend data.
 *
 * Reads the command received in the ring buffer and process the message
 *
 * Return: 0 if ring is empty,
 *         1 if more commands are present and the function will be called again
 */
static int process_recv_ring(struct arbif_xenback *back)
{
	int more_to_do;
	struct xengpu_back_ring *ring = &back->ring1;
	struct xengpu_request *req;
	RING_IDX rc, rp;

	rc = ring->req_cons;
	rp = ring->sring->req_prod;
	rmb(); /* Ensure we see queued requests up to 'rp'. */

	while (rc != rp) {
		req = RING_GET_REQUEST(ring, rc);
		process_msg(back,
			req->cmd,
			req->cmd_param0,
			req->cmd_param1,
			req->cmd_param2);
		ring->req_cons = ++rc;
		ring->rsp_prod_pvt++; /* Not using responses */
	}
	RING_FINAL_CHECK_FOR_REQUESTS(ring, more_to_do);
	if (more_to_do)
		return 1;

	return 0;
}

/**
 * arbif_process_requests() - Worker thread for processing RX commands
 * @data: Work contained within backend data.
 *
 * Process the receiving commands and schedule the required work to handle them
 */
static void arbif_process_requests(struct work_struct *data)
{
	struct arbif_xenback *back = container_of(data,
		struct arbif_xenback, arbif_req_work);
	int ret = process_recv_ring(back);

	if (ret > 0)
		queue_work(back->arbif_wq, &back->arbif_req_work);
}

/**
 * arb_back_isr() - ISR called when commands are received
 * @irq: IRQ number (not used)
 * @dev_id: Internal backend data.
 *
 * Wakes up listener thread to process commands
 *
 * Return: Always returns IRQ_HANDLED
 */
static irqreturn_t arb_back_isr(int irq, void *dev_id)
{
	struct arbif_xenback *back = dev_id;

	queue_work(back->arbif_wq, &back->arbif_req_work);
	return IRQ_HANDLED;
}

/**
 * read_per_ring_refs() - Read ring information from front-end
 * @back: Internal backend data.
 * @dir: Other end directory.
 *
 * Return: 0 if successful, otherwise a standard Linux error code
 */
static int read_per_ring_refs(struct arbif_xenback *back,
	const char *dir)
{
	int err;
	struct xenbus_device *xdev = back->xdev;

	err = xenbus_scanf(XBT_NIL, dir, "event-channel", "%u",
		&back->evtchn);

	if (err != 1) {
		err = -EINVAL;
		xenbus_dev_fatal(xdev, err, "reading %s/event-channel", dir);
		return err;
	}

	err = xenbus_scanf(XBT_NIL, dir, "ring-ref-g2h", "%u", &back->gref1);
	if (err != 1) {
		err = -EINVAL;
		xenbus_dev_fatal(xdev, err, "reading %s/ring-ref-g2h", dir);
		return err;
	}

	err = xenbus_scanf(XBT_NIL, dir, "ring-ref-h2g", "%u",
		&back->gref2);
	if (err != 1) {
		err = -EINVAL;
		xenbus_dev_fatal(xdev, err, "reading %s/ring-ref-h2g", dir);
		return err;
	}

	return 0;
}

/**
 * disconnect_rings() - Disconnect from front-end rings
 * @back: Internal backend data.
 *
 * Flush the remaining works and unmap all the memory and free the resources
 */
static void disconnect_rings(struct arbif_xenback *back)
{
	flush_work(&back->arbif_req_work);

	if (back->ring2.sring) {
		xenbus_unmap_ring_vfree(back->xdev, back->ring2.sring);
		back->ring2.sring = NULL;
	}

	if (back->ring1.sring) {
		xenbus_unmap_ring_vfree(back->xdev, back->ring1.sring);
		back->ring1.sring = NULL;
	}

	if (back->irq) {
		unbind_from_irqhandler(back->irq, back);
		back->irq = 0;
	}
}

/**
 * connect_rings() - Initialize and connect to front-end rings
 * @back: Internal backend data.
 *
 * Allocate the memory and resources necessary to initialize the rings used to
 * exchange messages
 *
 * Return: 0 if successful, otherwise a standard Linux error code
 */
static int connect_rings(struct arbif_xenback *back)
{
	int err;
	struct xenbus_device *xdev = back->xdev;
	void *alloc = NULL;

	dev_dbg(&back->xdev->dev, "%s %u\n", __func__, back->domid);

	if (back->irq)
		return 0;

	err = read_per_ring_refs(back, xdev->otherend);
	if (err)
		return err;

	err = xenbus_map_ring_valloc(xdev, &back->gref1, 1, &alloc);
	if (err < 0)
		return err;

	back->ring1.sring = alloc;
	BACK_RING_INIT(&back->ring1, back->ring1.sring, XEN_PAGE_SIZE);

	err = xenbus_map_ring_valloc(xdev, &back->gref2, 1, &alloc);
	if (err < 0)
		return err;

	back->ring2.sring = alloc;
	FRONT_RING_INIT(&back->ring2, back->ring2.sring, XEN_PAGE_SIZE);

	err = bind_interdomain_evtchn_to_irqhandler(back->domid,
		back->evtchn, arb_back_isr, 0, "mali_arbif_xenback",
		back);
	if (err < 0)
		goto cleanup_ring;

	back->irq = err;
	return 0;

cleanup_ring:
	if (back->ring2.sring) {
		xenbus_unmap_ring_vfree(back->xdev, back->ring2.sring);
		back->ring2.sring = NULL;
	}
	if (back->ring1.sring) {
		xenbus_unmap_ring_vfree(back->xdev, back->ring1.sring);
		back->ring1.sring = NULL;
	}
	return err;
}

/**
 * frontend_changed() - Called when Xen front-end state changes
 * @xdev: Xen bus device data.
 * @frontend_state: New state
 *
 * Processes state change and updates backend state when required
 */
static void frontend_changed(struct xenbus_device *xdev,
	enum xenbus_state frontend_state)
{
	struct arbif_xenback *back = dev_get_drvdata(&xdev->dev);
	int err;

	dev_dbg(&back->xdev->dev, "New FE state %s, BE state %s\n",
		xenbus_strstate(frontend_state),
		xenbus_strstate(xdev->state));

	switch (frontend_state) {
	case XenbusStateInitialising:
		if (xdev->state == XenbusStateClosed)
			xenbus_switch_state(xdev, XenbusStateInitWait);
		break;
	case XenbusStateInitialised:
	case XenbusStateConnected:
		/* Ensure we connect even when two watches fire in
		 * close succession and we miss the intermediate value
		 * of frontend_state.
		 */
		if (xdev->state == XenbusStateConnected)
			break;

		disconnect_rings(back);
		err = connect_rings(back);
		if (err)
			break;

		xenbus_switch_state(xdev, XenbusStateConnected);
		break;

	case XenbusStateClosing:
		xenbus_switch_state(xdev, XenbusStateClosing);
		break;
	case XenbusStateClosed:
		disconnect_rings(back);
		xenbus_switch_state(xdev, XenbusStateClosed);
		if (xenbus_dev_is_online(xdev))
			break;
		/* fall through if not online */
	case XenbusStateUnknown:
		/* implies xen_blkif_disconnect() via xen_blkbk_remove() */
		device_unregister(&xdev->dev);
		break;

	default:
		xenbus_dev_fatal(xdev, -EINVAL, "saw state %d at frontend",
			frontend_state);
		break;
	}
}

/**
 * free_device_info() - Frees the backend data
 * @xdev: Xen bus device data.
 *
 * Disconnect the rings, destroy workqueue and free memory
 */
static void free_device_info(struct xenbus_device *xdev)
{
	struct arbif_xenback *back = dev_get_drvdata(&xdev->dev);

	if (back) {
		disconnect_rings(back);
		destroy_workqueue(back->arbif_wq);
		back->arbif_wq = NULL;
		unregister_domain(back);
		dev_set_drvdata(&xdev->dev, NULL);
		devm_kfree(&xdev->dev, back);
		back = NULL;
	}
}

/**
 * alloc_device_info() - Allocates and initializes the backend data
 * @xdev: Xen bus device data.
 *
 * Allocate all the resources necessary to the device during runtime, such as
 * memory and work queues
 */
static struct arbif_xenback *alloc_device_info(struct xenbus_device *xdev)
{
	int err;

	struct device *dev = &xdev->dev;
	struct arbif_xenback *back =
		devm_kzalloc(dev, sizeof(struct arbif_xenback), GFP_KERNEL);
	if (!back) {
		xenbus_dev_fatal(xdev, -ENOMEM,
			"allocating backend structure");
		return NULL;
	}
	back->xdev = xdev;
	back->domid = xdev->otherend_id;
	err = register_domain(back);
	if (err)
		goto fail;

	back->arbif_wq = alloc_ordered_workqueue("arb_back_wq", WQ_HIGHPRI);
	if (!back->arbif_wq) {
		xenbus_dev_fatal(xdev, -EFAULT,
			"Failed to allocate back arbif_wq");
		goto cleanup_arbiter;
	}
	INIT_WORK(&back->arbif_req_work, arbif_process_requests);
	dev_set_drvdata(&xdev->dev, back);
	return back;

cleanup_arbiter:
	unregister_domain(back);
fail:
	free_device_info(xdev);
	return NULL;
}

/**
 * arbif_xenback_remove() - Cleans up resources for device remove
 * @xdev: Xen bus device data.
 *
 * Remove the device and clean up the resources related to it
 */
static int arbif_xenback_remove(struct xenbus_device *xdev)
{
	free_device_info(xdev);
	return 0;
}

/**
 * arbif_xenback_probe() - Initializes and allocates device resources
 * @xdev: Xen bus device data.
 * @id: Matches device ID (not used)
 *
 * Probe the device and initialize all the required resources for the runtime
 * execution
 *
 * Return: 0 if successful, otherwise a standard Linux error code
 */
static int arbif_xenback_probe(struct xenbus_device *xdev,
	const struct xenbus_device_id *id)
{
	int err;
	struct arbif_xenback *back;

	dev_info(&xdev->dev, "%s - domain %u\n", __func__, xdev->otherend_id);

	back = alloc_device_info(xdev);
	if (!back) {
		err = -ENOMEM;
		xenbus_dev_fatal(xdev, err,
			"Error allocating backend device info\n");
		goto fail;
	}

	err = xenbus_printf(XBT_NIL, xdev->nodename,
		"max-ring-page-order", "%u",
		XENBUS_MAX_RING_GRANT_ORDER);
	if (err)
		dev_err(&back->xdev->dev,
			"Write out 'max-ring-page-order' failed\n");

	err = xenbus_switch_state(xdev, XenbusStateInitWait);
	if (err)
		goto fail;

	return 0;

fail:
	arbif_xenback_remove(xdev);
	return err;
}

/**
 * @arbif_xenback_ids: Match the xenbus device with the front-end.
 */
static const struct xenbus_device_id arbif_xenback_ids[] = {
	{ "mali_arbif_xen" },
	{ "" }
};

/**
 * @arbif_xenback_driver: Xenbus driver data.
 */
static struct xenbus_driver arbif_xenback_driver = {
	.ids  = arbif_xenback_ids,
	.probe = arbif_xenback_probe,
	.otherend_changed = frontend_changed,
	.remove = arbif_xenback_remove,
};

/**
 * assign_vm_gpu() - Assign GPU to a given domain
 * @ctx: Caller context (not used)
 * @domain_id: VM domain to use
 *
 * Makes a hypercall to assign the GPU to a given domain ID
 *
 * Return: 0 if successful, otherwise a standard Linux error code
 */
static int assign_vm_gpu(void *ctx, u32 domain_id)
{
	int ret;
	struct xen_arb_gpu_op op;

	op.cmd = ARB_HYP_ASSIGN_VM_GPU;
	op.interface_version = XENARBGPU_INTERFACE_VERSION;
	op.domain = domain_id;
	ret = HYPERVISOR_arb_gpu_op(&op);
	if (ret) {
		pr_alert("HYPERVISOR_arb_gpu_op (assign) failure 0x%08x\n",
			ret);
		return ret;
	}
	return 0;
}

/**
 * force_assign_vm_gpu() - Force-ably assign GPU to a given domain
 * @ctx: Caller context (not used)
 * @domain_id: VM domain to use
 *
 * Makes a hypercall to force-ably assign the GPU to a given domain ID.
 * Ideally this function should not fail
 * because there is no way for the arbiter to recover.
 *
 * Return: 0 if successful, otherwise a standard Linux error code
 */
static int force_assign_vm_gpu(void *ctx, u32 domain_id)
{
	int ret;
	struct xen_arb_gpu_op op;

	op.cmd = ARB_HYP_FORCE_ASSIGN_VM_GPU;
	op.interface_version = XENARBGPU_INTERFACE_VERSION;
	op.domain = domain_id;
	ret = HYPERVISOR_arb_gpu_op(&op);
	if (ret) {
		pr_alert("HYPERVISOR_arb_gpu_op (force) failure 0x%08x\n", ret);
		return ret;
	}
	return 0;
}


/**
 * arbif_xenback_init() - Register xenbus driver
 *
 * Initialize and register the driver during the init process
 */
static int __init arbif_xenback_init(void)
{
	int ret;
	struct mali_arb_hyp_callbacks hyp_cbs = {0};

	arb_module = NULL;
	arb_dev = NULL;

	ret = find_arbiter_device();
	if (ret)
		return ret;

	hyp_cbs.arb_hyp_assign_vm_gpu = assign_vm_gpu;
	hyp_cbs.arb_hyp_force_assign_vm_gpu = force_assign_vm_gpu;

	ret = arb_dev->ops.vm_arb_register_hyp(arb_dev, NULL, &hyp_cbs);
	if (ret) {
		arb_dev = NULL;
		pr_err("Failed to register arbiter hypercall functions\n");
		return ret;
	}

	return xenbus_register_backend(&arbif_xenback_driver);
}
module_init(arbif_xenback_init);

/**
 * arbif_xenback_exit - Unregister xenbus driver
 */
static void __exit arbif_xenback_exit(void)
{
	xenbus_unregister_driver(&arbif_xenback_driver);
	if (arb_dev)
		arb_dev->ops.vm_arb_unregister_hyp(arb_dev);

	if (arb_module) {
		module_put(arb_module);
		arb_module = NULL;
	}
}
module_exit(arbif_xenback_exit);

MODULE_DESCRIPTION("Xen back-end reference driver to support VM GPU arbitration");
MODULE_LICENSE("GPL");
MODULE_ALIAS("mali-arbif-xenback");
