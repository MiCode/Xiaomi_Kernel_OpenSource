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
#include <linux/freezer.h>
#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/version.h>

#include <xen/xen.h>
#include <xen/xenbus.h>

#include <xen/grant_table.h>
#include <xen/events.h>
#include <xen/page.h>
#include <xen/platform_pci.h>

#include <xen/interface/grant_table.h>

#include <xen/interface/io/ring.h>
#include <xen/interface/grant_table.h>
#include <xen/interface/io/ring.h>
#include <xen/interface/mali_xen_hyp.h>
#include <asm/xen/hypercall.h>

#include "../mali_arbiter.h"
#include "mali_arbif_xenback_hypercall.h"
#include "mali_arb_vm_protocol.h"

DEFINE_RING_TYPES(xengpu, uint64_t, uint64_t);

/* Arbiter version against which was implemented this module */
#define MALI_REQUIRED_ARBITER_VERSION 2
#if MALI_REQUIRED_ARBITER_VERSION != MALI_ARBITER_VERSION
#error "Unsupported Mali Arbiter version."
#endif

/* Device tree compatible ID of the Arbiter */
#define MALI_ARBITER_DT_NAME "arm,mali-arbiter"
/*
 * Index of the VM assign interface. Used when hardware separation is supported
 * and multiple VM assign interfaces are available. For solutions without
 * hardware separation support only index 0 is populated.
 */
#define VM_ASSIGN_IF_ID 0

/**
 * enum handshake_state - Xenback states
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
 * struct mali_vm_priv_data - VM private data for the Xenback
 * @dev:    Pointer to the resource group device instance managing this VM.
 * @id:     VM index used by the resource group to identify each VM.
 * @arb_vm: Private Arbiter VM data used by the Arbiter to identify and work
 *          with each VM. This is populated by the Arbiter during the
 *          registration. See register_vm callback in the public interface.
 *
 * Stores the relevant data of each VM managed by this Resource Group.
 */
struct mali_vm_priv_data {
	struct device *dev;
	uint32_t id;
	struct mali_arb_priv_data *arb_vm;
};

/**
 * struct arbif_xenback - Internal arbif xen backend state
 * @xdev:           Xen backend device data.
 * @domid:          Domain ID.
 * @irq:            Used to send interrupts to VM
 * @ring1:          Ring containing messages received from VM
 * @ring2:          Ring containing messages to send to VM
 * @evtchn:         Event used to received interrupts from Xen
 * @gref1:          Xen grant table reference for ring 1
 * @gref2:          Xen grant table reference for ring 2
 * @xenschd:        Listener thread
 * @arb_ops:        Arbiter function callbacks (valid if arb_vm is not NULL)
 * @vm:             Xenback VM private data used to identify each VM.
 * @arbif_wq:       Workqueue for receiving commands from front-end
 * @arbif_req_work: Work item for arbif_wq to process commands
 * @xb_handshake_state:     State of protocal handshake with frontkend
 * @handshake_mutex:     Mutex to protect against accessing handshake state
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
	struct mali_vm_priv_data    vm;
	struct workqueue_struct     *arbif_wq;
	struct work_struct          arbif_req_work;
	enum handshake_state        xb_handshake_state;
	struct mutex                handshake_mutex;
};

struct module *arb_module;
struct mali_arb_data *arb_data;
struct vm_assign_ops *assign_ops;

/* Forward Declaration */
static void disconnect_rings(struct arbif_xenback *back);

/**
 * get_handshake_state() - Get Xenback handshake state
 * @back: Internal backend data.
 * @state: State to be set with handshake state
 *
 * Get Xenback handshake state in a safe way.
 *
 * Return: 0 if successful, otherwise a standard Linux error code.
 */
static int get_handshake_state(struct arbif_xenback *back,
		enum handshake_state *state)
{

	if (!back || !state)
		return -EINVAL;

	mutex_lock(&back->handshake_mutex);
	*state = back->xb_handshake_state;
	mutex_unlock(&back->handshake_mutex);

	return 0;
}

/**
 * set_handshake_state() - Set Xenback handshake state
 * @back: Internal backend data.
 * @state: State to set handshake state with.
 *
 * Set Xenback handshake state in a safe way.
 *
 * Return: 0 if successful, otherwise a standard Linux error code.
 */
static int set_handshake_state(struct arbif_xenback *back,
		enum handshake_state state)
{

	if (!back)
		return -EINVAL;

	mutex_lock(&back->handshake_mutex);
	back->xb_handshake_state = state;
	mutex_unlock(&back->handshake_mutex);

	return 0;
}

/**
 * send_command() - Send command to the VM front-end
 * @back: Internal backend data.
 * @payload: Message content to be sent.
 *
 * Send one of the commands defined in mali_arbif_common.h.
 */
static void send_command(struct arbif_xenback *back, uint64_t payload)
{
	struct xengpu_front_ring *ring = &back->ring2;
	uint64_t *req;
	int notify = 0;

	ring->rsp_cons = ring->sring->rsp_prod;
	req = RING_GET_REQUEST(ring, ring->req_prod_pvt);
	ring->req_prod_pvt++;
	*req = payload;
	RING_PUSH_REQUESTS_AND_CHECK_NOTIFY(ring, notify);
	if (notify)
		notify_remote_via_irq(back->irq);
}

/**
 * on_gpu_granted() - GPU has been granted by the arbiter
 * @vm:    Opaque pointer to the VM data passed by the backend during the
 *         registration.
 * @freq:  Frequency to be passed to VM.
 *
 * Sends the ARB_VM_GPU_GRANTED to the front-end.
 */
static void on_gpu_granted(struct mali_vm_priv_data *vm, uint32_t freq)
{
	struct device *dev;
	struct arbif_xenback *back;
	uint64_t payload = 0;
	enum handshake_state state;

	if (!vm) {
		pr_err("Message not sent. No VM private data passed.");
		return;
	}
	dev = vm->dev;
	back = dev_get_drvdata(dev);

	get_handshake_state(back, &state);
	if (state != HANDSHAKE_DONE) {
		pr_err("Message not sent. Handshake not completed or failed");
		return;
	}

	dev_dbg(dev, "%s %u\n", __func__, back->domid);
	arb_vm_gpu_granted_build_msg(freq, &payload);
	send_command(back, payload);
}

/**
 * on_gpu_stop() - Arbiter requires VM to stop using GPU
 * @vm: Opaque pointer to the VM data passed by the backend during the
 *      registration.
 *
 * Sends the ARB_VM_GPU_STOP to the front-end.
 */
static void on_gpu_stop(struct mali_vm_priv_data *vm)
{
	struct device *dev;
	struct arbif_xenback *back;
	uint64_t payload = 0;
	enum handshake_state state;

	if (!vm) {
		pr_err("Message not sent. No VM private data passed.");
		return;
	}
	dev = vm->dev;
	back = dev_get_drvdata(dev);

	get_handshake_state(back, &state);
	if (state != HANDSHAKE_DONE) {
		pr_err("Message not sent. Handshake not completed or failed");
		return;
	}

	dev_dbg(dev, "%s %u\n", __func__, back->domid);
	arb_vm_gpu_stop_build_msg(&payload);
	send_command(back, payload);
}

/**
 * on_gpu_lost() - Sends message to VM that GPU has been removed
 * @vm: Opaque pointer to the VM data passed by the backend during the
 *      registration.
 *
 * Sends the ARB_VM_GPU_LOST to the front-end.
 */
static void on_gpu_lost(struct mali_vm_priv_data *vm)
{
	struct device *dev;
	struct arbif_xenback *back;
	uint64_t payload = 0;
	enum handshake_state state;

	if (!vm) {
		pr_err("Message not sent. No VM private data passed.");
		return;
	}
	dev = vm->dev;
	back = dev_get_drvdata(dev);

	get_handshake_state(back, &state);
	if (state != HANDSHAKE_DONE) {
		pr_err("Message not sent. Handshake not completed or failed");
		return;
	}

	dev_alert(dev, "%s %u\n", __func__, back->domid);
	arb_vm_gpu_lost_build_msg(&payload);
	send_command(back, payload);
}


/**
 * gpu_init() - Process the VM_ARB_INIT message
 * @back:	Xen Backend device.
 * @payload:	Payload of message received
 *
 * Handles VM_ARB_INIT message decides if Handshake is successful
 * or not, and requests the gpu if needed
 */
static void gpu_init(struct arbif_xenback *back, uint64_t payload)
{
	uint8_t version;
	uint8_t gpu_request;

	get_msg_protocol_version(&payload, &version);
	if (version >= MIN_SUPPORTED_VERSION && version <= CURRENT_VERSION) {
		set_handshake_state(back, HANDSHAKE_DONE);
		get_msg_init_request(&payload, &gpu_request);
		if (gpu_request) {
			if (back->arb_ops->gpu_request)
				back->arb_ops->gpu_request(back->vm.arb_vm);
		}
	} else {
		pr_err("Incompatible protocol version. Handshake has failed\n");
		set_handshake_state(back, HANDSHAKE_FAILED);
	}
}

/**
 * find_arbiter_device() - Uses Device Tree to find the Arbiter device
 * Finds and connects to the arbiter device from the device tree.
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
	arb_data = platform_get_drvdata(pdev);
	if (!arb_data) {
		ret = -ENODEV;
		dev_err(&(pdev->dev), "Error Mali Arbiter not ready\n");
		goto cleanup_module;
	}
	return 0;

cleanup_module:
	module_put(arb_module);
	arb_data = NULL;
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
	struct mali_vm_data vm_info;
	struct xenbus_device *xdev = back->xdev;

	if (!arb_data->ops.register_vm) {
		ret = -ENODEV;
		xenbus_dev_fatal(xdev, ret,
			"Error Mali Arbiter does not support register op\n");
		return ret;
	}

	/* Populate public VM data */
	vm_info.dev = &back->xdev->dev;
	vm_info.id = back->domid;
	vm_info.ops.gpu_granted = on_gpu_granted;
	vm_info.ops.gpu_stop = on_gpu_stop;
	vm_info.ops.gpu_lost = on_gpu_lost;

	/* Populate private VM data */
	back->vm.id = back->domid;
	back->vm.dev = &back->xdev->dev;

	ret = arb_data->ops.register_vm(arb_data, &vm_info, &back->vm,
		&back->vm.arb_vm);
	if (ret) {
		xenbus_dev_fatal(xdev, ret,
			"Error Mali Arbiter registration failed\n");
		return ret;
	}
	back->arb_ops = &arb_data->ops;
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
	if (back->arb_ops && back->arb_ops->unregister_vm)
		back->arb_ops->unregister_vm(back->vm.arb_vm);
	back->vm.arb_vm = NULL;
	back->vm.dev = NULL;
	back->arb_ops = NULL;
}

/**
 * process_msg() - Dispatches a message received from Xen front-end
 * @back: Internal backend data.
 * @payload: Message content received.
 *
 * Dispatch a message received according the command ID received and
 * trigger the corresponding VM arbiter operation
 */
static void process_msg(struct arbif_xenback *back, uint64_t payload)
{
	uint8_t cmd_req;

	if (!back->arb_ops)
		return;

	get_msg_id(&payload, &cmd_req);

	switch (cmd_req) {
	case VM_ARB_GPU_REQUEST:
		dev_dbg(&back->xdev->dev, "recv VM_ARB_GPU_REQUEST %u\n",
			back->domid);
		if (back->arb_ops->gpu_request)
			back->arb_ops->gpu_request(back->vm.arb_vm);
		break;
	case VM_ARB_GPU_STOPPED:
	{
		bool req_again;
		uint8_t priority;

		get_msg_priority(&payload, &priority);
		req_again = (priority != 0)?true:false;

		dev_dbg(&back->xdev->dev,
			"recv VM_ARB_GPU_STOPPED %u, %d\n",
			back->domid, req_again);
		if (back->arb_ops->gpu_stopped)
			back->arb_ops->gpu_stopped(back->vm.arb_vm, req_again);
		break;
	}
	case VM_ARB_GPU_IDLE:
		dev_dbg(&back->xdev->dev, "recv VM_ARB_GPU_IDLE %u\n",
				back->domid);
		if (back->arb_ops->gpu_idle)
			back->arb_ops->gpu_idle(back->vm.arb_vm);
		break;
	case VM_ARB_GPU_ACTIVE:
		dev_dbg(&back->xdev->dev, "recv VM_ARB_GPU_ACTIVE %u\n",
			back->domid);
		if (back->arb_ops->gpu_active)
			back->arb_ops->gpu_active(back->vm.arb_vm);
		break;
	case VM_ARB_INIT:
		gpu_init(back, payload);
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
	uint64_t *req;
	RING_IDX rc, rp;

	rc = ring->req_cons;
	rp = ring->sring->req_prod;
	rmb(); /* Ensure we see queued requests up to 'rp'. */

	while (rc != rp) {
		req = RING_GET_REQUEST(ring, rc);
		process_msg(back, *req);
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
#if (KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE)
	xen_irq_lateeoi(irq, XEN_EOI_FLAG_SPURIOUS);
#endif

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

/* LINUX_VERSION_CODE >= 5.12 */
#if (KERNEL_VERSION(5, 12, 0) <= LINUX_VERSION_CODE)
	err = bind_interdomain_evtchn_to_irqhandler_lateeoi(xdev,
		back->evtchn, arb_back_isr, 0, "mali_arbif_xenback", back);
/* 5.10 <= LINUX_VERSION_CODE < 5.12 */
#elif ((KERNEL_VERSION(5, 12, 0) > LINUX_VERSION_CODE) && \
	(KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE))
	err = bind_interdomain_evtchn_to_irqhandler_lateeoi(back->domid,
		back->evtchn, arb_back_isr, 0, "mali_arbif_xenback", back);
#else /* LINUX_VERSION_CODE < 5.10 */
	err = bind_interdomain_evtchn_to_irqhandler(back->domid,
		back->evtchn, arb_back_isr, 0, "mali_arbif_xenback", back);
#endif  /* LINUX_VERSION_CODE */
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
	uint64_t message;

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

		/* Initiate protocol handshake */
		arb_vm_init_build_msg(0, 0, 0, CURRENT_VERSION, &message);
		send_command(back, message);
		set_handshake_state(back, HANDSHAKE_IN_PROGRESS);
		break;

	case XenbusStateClosing:
		xenbus_switch_state(xdev, XenbusStateClosing);
		break;
	case XenbusStateClosed:
		set_handshake_state(back, HANDSHAKE_INIT);
		disconnect_rings(back);
		xenbus_switch_state(xdev, XenbusStateClosed);
		if (xenbus_dev_is_online(xdev))
			break;
		/* fallthrough */
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
 *
 * Return: Pointer to struct arbif_xenback on success else NULL
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
 *
 * Return: 0 always
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

/*
 * arbif_xenback_ids: Match the xenbus device with the front-end.
 */
static const struct xenbus_device_id arbif_xenback_ids[] = {
	{ "mali_arbif_xen" },
	{ "" }
};

/*
 * arbif_xenback_driver: Xenbus driver data.
 */
static struct xenbus_driver arbif_xenback_driver = {
	.ids  = arbif_xenback_ids,
	.probe = arbif_xenback_probe,
	.otherend_changed = frontend_changed,
	.remove = arbif_xenback_remove,
};

/**
 * assign_vm_gpu() - Assign GPU to a given domain
 * @dev:   VM Assign device (not used)
 * @vm_id: VM domain to use
 *
 * Makes a hypercall to assign the GPU to a given domain ID
 *
 * Return: 0 if successful, otherwise a standard Linux error code
 */
static int assign_vm_gpu(struct device *dev, uint32_t vm_id)
{
	int ret;
	struct xen_arb_gpu_op op;

	op.cmd = ARB_HYP_ASSIGN_VM_GPU;
	op.interface_version = XENARBGPU_INTERFACE_VERSION;
	op.domain = vm_id;
	ret = HYPERVISOR_arb_gpu_op(&op);
	if (ret) {
		pr_alert("HYPERVISOR_arb_gpu_op (assign) failure 0x%08x\n",
			ret);
		return ret;
	}
	return 0;
}

/**
 * unassign_vm_gpu() - Unassign GPU from a any domain - No-op
 * @dev: VM Assign device (not used)
 *
 * No-op in the xenback. By design in paravirtualization solution the unassign
 * should be done as part of the assign.
 *
 * Return: 0 if successful, otherwise a standard Linux error code
 */
static int unassign_vm_gpu(struct device *dev)
{
	return 0;
}

/**
 * arbif_xenback_init() - Register xenbus driver
 *
 * Initialize and register the driver during the init process
 *
 * Return: 0 if successful, otherwise standard error code.
 */
static int __init arbif_xenback_init(void)
{
	int ret;
	struct vm_assign_interface vm_assign_if = {0};

	arb_module = NULL;
	arb_data = NULL;

	ret = find_arbiter_device();
	if (ret)
		return ret;

	/*
	 * Allocate the VM assign ops so that it can be used by the Arbiter.
	 * Note that this interface is commom for all the xen backend devices,
	 * so it neededs to be allocated on module init and dealocated on exit.
	 */
	assign_ops = kzalloc(sizeof(struct vm_assign_ops), GFP_KERNEL);
	if (!assign_ops)
		return -ENOMEM;

	assign_ops->assign_vm = assign_vm_gpu;
	assign_ops->unassign_vm = unassign_vm_gpu;
	/* Get assigned is not implemented in xenback */
	assign_ops->get_assigned_vm = NULL;
	vm_assign_if.if_id = VM_ASSIGN_IF_ID;
	vm_assign_if.ops = assign_ops;
	ret = arb_data->ops.arb_reg_assign_if(arb_data, vm_assign_if);
	if (ret) {
		pr_err("Failed to register VM assign interface\n");
		goto dealoc_ops;
	}
	ret = xenbus_register_backend(&arbif_xenback_driver);
	if (ret) {
		arb_data->ops.arb_unreg_assign_if(arb_data, VM_ASSIGN_IF_ID);
		pr_err("Failed to register backend driver\n");
		goto dealoc_ops;
	}
	return 0;

dealoc_ops:
	arb_data = NULL;
	kfree(assign_ops);
	assign_ops = NULL;
	return ret;
}
module_init(arbif_xenback_init);

/**
 * arbif_xenback_exit - Unregister xenbus driver
 */
static void __exit arbif_xenback_exit(void)
{
	xenbus_unregister_driver(&arbif_xenback_driver);
	if (arb_data)
		arb_data->ops.arb_unreg_assign_if(arb_data, VM_ASSIGN_IF_ID);
	kfree(assign_ops);
	assign_ops = NULL;

	if (arb_module) {
		module_put(arb_module);
		arb_module = NULL;
	}
}
module_exit(arbif_xenback_exit);

MODULE_DESCRIPTION("Xen back-end reference driver to support VM GPU arbitration");
MODULE_LICENSE("GPL");
MODULE_ALIAS("mali-arbif-xenback");
