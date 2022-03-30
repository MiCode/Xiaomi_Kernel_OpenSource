// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note or MIT

/*
 * (C) COPYRIGHT 2020-2021 Arm Limited or its affiliates. All rights reserved.
 */

/*
 * mali_gpu_aw_main.c
 * Mali Access Window module implementation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/iopoll.h>
#include <mali_kbase_arbiter_interface.h>
#include <arb_vm_protocol/mali_arb_vm_protocol.h>
#include <mali_gpu_ptm_msg.h>


/* Arbiter interface version against which was implemented this module */
#define MALI_REQUIRED_KBASE_ARBITER_INTERFACE_VERSION 5
#if MALI_REQUIRED_KBASE_ARBITER_INTERFACE_VERSION != \
			MALI_KBASE_ARBITER_INTERFACE_VERSION
#error "Unsupported Mali Arbiter interface version."
#endif

#define EDGE_IRQ(x) (((x) & (IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING)) != 0)

/* Device tree compatible ID of the Access Window module */
#define AW_MESSAGE_DT_NAME "arm,mali-gpu-aw-message"

#define PTM_ID				0x00
#define PTM_AW_IRQ_RAWSTAT		0x04
#define PTM_AW_IRQ_CLEAR		0x08
#define PTM_AW_IRQ_MASK			0x0c
#define PTM_AW_IRQ_STATUS		0x10
#define PTM_AW_IRQ_INJECTION		0x14
#define PTM_AW_MESSAGE			0x18

/*
 * Mask for all the IRQs in the AW apart from the INVALID_ACCESS which is not
 * served in the AW module. The system module should print a debug message when
 * an invalid access happens.
 */
#define MAX_AW_IRQ_MASK			0x3d
/* We check PTM_DEVICE_ID against this value to determine compatibility */
#define PTM_SUPPORTED_VER		0x9e550000
/* Required and masked fields: */
/* Bits [3:0] VERSION_STATUS - masked */
/* Bits [11:4] VERSION_MINOR - masked */
/* Bits [15:12] VERSION_MAJOR - masked */
/* Bits [19:16] PRODUCT_MAJOR - required */
/* Bits [23:20] ARCH_REV - masked */
/* Bits [27:24] ARCH_MINOR - required */
/* Bits [31:28] ARCH_MAJOR - required */
#define PTM_VER_MASK			0xFF0F0000
#define IRQ_BIT_MASK			1
#define DEVICE_ATTACH_DRIVER_FOUND	1

/*
 * NOTE: 'arbiter' and 'resource group' are terms used interchangeably, they
 * mean the same thing.
 */

/**
 * struct ptm_aw_regs - Structure containing register data for the Access Window
 * @addr:  Virtual register space
 * @res:   Device resource info
 */
struct ptm_aw_regs {
	void __iomem *addr;
	struct resource *res;
};

/**
 * struct ptm_aw_irqs - Structure containing irq data for the Access Window.
 * @irq:    irq number
 * @flags:  irq flags
 */
struct ptm_aw_irqs {
	int irq;
	int flags;
};

/**
 * enum ptm_aw_state - Access Window initialization state
 * @AW_INIT:                 The protocol handshake has not yet started.
 * @AW_HS_INITIATED:         The protocol handshake has been started by the AW.
 * @AW_HS_INITIATED_NO_DEV:  KBASE has unregistered during handshake.
 * @AW_HS_FAILED:            The protocol handshake failed.
 * @AW_HS_DONE:              Handshake complete.  Waiting for KBASE to probe
 *                           and request GPU.
 * @AW_GPU_REQUESTED:        GPU requested by KBASE.  Waiting for GPU to be
 *                           granted.
 * @AW_GPU_REQUESTED_NO_DEV: KBASE has unregistered while waiting for GPU.
 * @AW_ATTACH_DEV:           KBASE re-probe issued.  Waiting for GPU request.
 * @AW_MSG_QUEUED:           Waiting for WQ to run for sending GPU granted
 *                           message to KBASE.
 * @AW_MSG_QUEUED_NO_DEV:    KBASE has unregistered while waiting for WQ to
 *                           run.
 * @AW_READY:                Access Window has fully initialized.
 *
 * Definition of the possible Access Window's initialization states.
 */
enum ptm_aw_state {
	AW_INIT,
	AW_HS_INITIATED,
	AW_HS_INITIATED_NO_DEV,
	AW_HS_FAILED,
	AW_HS_DONE,
	AW_GPU_REQUESTED,
	AW_GPU_REQUESTED_NO_DEV,
	AW_ATTACH_DEV,
	AW_MSG_QUEUED,
	AW_MSG_QUEUED_NO_DEV,
	AW_READY,
};

/**
 * struct init_msg - stores ARB_VM_INIT message data
 * @received:      Flag to check if INIT message has been received or not.
 * @ack:           Protocol ACK received from the VM_ARB_INIT.
 * @version:       Protocol version received from the VM_ARB_INIT.
 * @max_l2_slices: Number of l2 slices assigned.
 * @max_core_mask: Mask of number of cores in each slice assigned.
 *
 * ARB_VM_INIT message data used for the protocol handshake. It has to be stored
 * because the AW can only respond when kbase requests the GPU.
 */
struct init_msg {
	bool received;
	uint8_t ack;
	uint8_t version;
	uint32_t max_l2_slices;
	uint32_t max_core_mask;
};

/**
 * struct ptm_aw_protocol - Access Window protocol data
 * @state:             Current Access Window initialization state.
 * @gpu_requested:     AW has requested GPU from Arbiter.
 * @version_in_use:    Agreed protocol versions used for the communication with
 *                     the Resource Group. Defined during protocol handshake.
 * @init_msg:          ARB_VM_INIT message data received from RG.
 * @mutex:             Mutex to protect against concurrent API calls accessing
 *                     the protocol data.
 * @last_freq:         Last frequency received from Arbiter GRANTED message.
 * @last_msg_id:       Last message ID received from Arbiter.
 * @max_config_req:    true if max config needs to be sent to KBASE.
 */
struct ptm_aw_protocol {
	enum ptm_aw_state state;
	bool gpu_requested;
	uint8_t version_in_use;
	struct init_msg init_msg;
	struct mutex mutex;
	uint32_t last_freq;
	uint8_t last_msg_id;
	bool max_config_req;
};


/**
 * struct mali_gpu_ptm_aw - Device userdata
 * @arbif_dev:          arbiter interface.
 * @dev:                pointer to this access window (AW) device.
 * @vm_dev:             pointer to the VM device registered with this
 *                      AW.
 * @vm_drv_ops:         arbiter callbacks.
 * @vmdev_rwsem:        Protects kbase callbacks.
 * @regs:               contains the register base address for this
 *                      device.
 * @irqs:               the access windows IRQ's.
 * @message_wq:	        the work queue used to process messages.
 * @rx_msg_work:        work item for processing messages received
 *                      apart from the ARB_VM_INIT.
 * @init_msg_work:      work corresponing to ARB_VM_INIT message
 * @device_attach_wq:   Device attach work queue.
 * @device_attach_work: work item to device attach kbase in case it deferred
 *                      the probe and the condition to reprobe is achieved.
 * @msg_handler:        A ptm_msg_handler structure for managing PTM
 *                      messages
 * @bl_state:           Bus logger state
 * @buslogger:          Pointer to the buglogger client
 * @reg_data:           Register data store
 * @prot_data:          Internal protocol data for correct
 *                      communication with the RG.
 * @edge_irq:           Is the AW IRQ edge based?
 * @request_msg_work:   Work item for sending kbase messages after a
 *                      GPU request.
 *
 * Device userdata, allocated and attached to kernel device in probe.
 */
struct mali_gpu_ptm_aw {
	struct arbiter_if_dev arbif_dev;
	struct device *dev;
	struct device *vm_dev;
	struct arbiter_if_arb_vm_ops vm_drv_ops;
	struct rw_semaphore vmdev_rwsem;
	struct ptm_aw_regs regs;
	struct ptm_aw_irqs irqs;
	struct workqueue_struct *message_wq;
	struct work_struct rx_msg_work;
	struct work_struct init_msg_work;
	struct workqueue_struct *device_attach_wq;
	struct work_struct device_attach_work;
	struct ptm_msg_handler msg_handler;
	struct ptm_aw_protocol prot_data;
	bool edge_irq;
	struct work_struct request_msg_work;
#ifdef CONFIG_MALI_BUSLOG
	enum buslogger_state bl_state;
	struct bus_logger_client *buslogger;
	struct register_store *reg_data;
#endif
};

/**
 * enum message_idx - Message buffer index
 * @MESSAGE_IDX_INIT:   Index for ARB_VM_INIT messages only
 * @MESSAGE_IDX_RX:     Index for all other received messages
 * @MESSAGE_IDX_COUNT:  Message buffer count
 *
 * Definition of the possible Message buffers.
 * This mechanism is used to avoid missing INIT messages, so
 * it is stored in a different buffer from the other messages.
 *
 */
enum message_idx {
	MESSAGE_IDX_INIT,
	MESSAGE_IDX_RX,

	/* Must be last */
	MESSAGE_IDX_COUNT
};

/**
 * aw_read_incoming_msg() - Reads the last message received.
 * @awdev: Access Window device structure.
 *
 */
static void aw_read_incoming_msg(struct mali_gpu_ptm_aw *awdev)
{
	uint64_t message = 0;
	uint8_t message_id = 0;
	uint8_t message_idx = 0;
	struct work_struct *work = NULL;

	if (!awdev)
		return;

	/* Read the message into a receive buffer, overwriting if necessary */
	ptm_msg_read(&awdev->msg_handler, 0, &message);

	if (get_msg_id(&message, &message_id)) {
		dev_dbg(awdev->dev, "get_msg_id() failed\n");
		return;
	}

	if (message_id == ARB_VM_INIT) {
		message_idx = MESSAGE_IDX_INIT;
		work = &awdev->init_msg_work;
	} else {
		message_idx = MESSAGE_IDX_RX;
		work = &awdev->rx_msg_work;
	}

	if (ptm_msg_buff_write(&awdev->msg_handler.recv_msgs,
				message_idx,
				message) > 0)
		dev_dbg(awdev->msg_handler.dev,
			"Overriding incoming PTM message\n");

	/* Clear interrupt flags for message only.
	 * Other interrupts for the GPU have to be cleared at source.
	 */
	iowrite32(IRQ_BIT_MASK, awdev->regs.addr + PTM_AW_IRQ_CLEAR);

	queue_work(awdev->message_wq, work);
}

/**
 * aw_state_str() - Helper function to get string for AW state.
 *
 * @state: AW initialization state
 *
 * Return: string representation of ptm_aw_state
 */
static inline const char *aw_state_str(
	enum ptm_aw_state state)
{
	switch (state) {
	case AW_INIT:
		return "AW_INIT";
	case AW_HS_INITIATED:
		return "AW_HS_INITIATED";
	case AW_HS_INITIATED_NO_DEV:
		return "AW_HS_INITIATED_NO_DEV";
	case AW_HS_FAILED:
		return "AW_HS_FAILED";
	case AW_HS_DONE:
		return "AW_HS_DONE";
	case AW_GPU_REQUESTED:
		return "AW_GPU_REQUESTED";
	case AW_GPU_REQUESTED_NO_DEV:
		return "AW_GPU_REQUESTED_NO_DEV";
	case AW_ATTACH_DEV:
		return "AW_ATTACH_DEV";
	case AW_MSG_QUEUED:
		return "AW_MSG_QUEUED";
	case AW_MSG_QUEUED_NO_DEV:
		return "AW_MSG_QUEUED_NO_DEV";
	case AW_READY:
		return "AW_READY";
	default:
		return "[UnknownState]";
	}
}

/**
 * aw_set_state() - Sets the access window initialization state.
 * @awdev:     Access Window module internal data.
 * @new_state: What the new state should be.
 *
 */
static void aw_set_state(struct mali_gpu_ptm_aw *awdev,
	enum ptm_aw_state new_state)
{
	lockdep_assert_held(&awdev->prot_data.mutex);
	if (awdev->prot_data.state != new_state) {
		dev_dbg(awdev->dev, "%s: %s -> %s\n", __func__,
			aw_state_str(awdev->prot_data.state),
			aw_state_str(new_state));
		awdev->prot_data.state = new_state;
	}
}

/**
 * aw_get_state() - Gets the access window initialization state.
 * @awdev:      Access Window module internal data.
 * @curr_state: Returns the current state for that AW ID.
 *
 */
static void aw_get_state(struct mali_gpu_ptm_aw *awdev,
	enum ptm_aw_state *curr_state)
{
	lockdep_assert_held(&awdev->prot_data.mutex);
	*curr_state = awdev->prot_data.state;
}

/**
 * aw_set_init_data() - Store INIT data used to respond the protocol handshake
 *                      when GPU is requested from kbase during probe.
 * @awdev:           Access Window module internal data.
 * @message:         ARB_VM_INIT message.
 *
 * Stores the INIT data used to respond the protocol handshake when GPU is
 * requested from kbase during probe. This function can be called every time an
 * expected INIT message is received from the RG.
 */
static void aw_set_init_data(struct mali_gpu_ptm_aw *awdev, uint64_t *message)
{
	uint8_t recv_ack;
	uint8_t recv_version;
	uint8_t max_l2_slices;
	uint32_t max_core_mask;

	if (WARN_ON(!message || !awdev))
		return;

	lockdep_assert_held(&awdev->prot_data.mutex);
	if (get_msg_max_config(message, &max_l2_slices, &max_core_mask) ||
	    get_msg_protocol_version(message, &recv_version) ||
	    get_msg_init_ack(message, &recv_ack))
		return;

	/* Store the ini message data */
	awdev->prot_data.init_msg.ack = recv_ack;
	awdev->prot_data.init_msg.version = recv_version;
	awdev->prot_data.init_msg.max_l2_slices = (uint32_t)max_l2_slices;
	awdev->prot_data.init_msg.max_core_mask = max_core_mask;
	awdev->prot_data.init_msg.received = true;
}

/**
 * aw_get_init_data() - Store INIT data used to respond the protocol handshake
 *                      when GPU is requested from kbase during probe.
 * @awdev:     Access Window module internal data.
 * @init_data: Init message data to return the received data.
 *
 * Stores the INIT data used to respond the protocol handshake when GPU is
 * requested from kbase during probe. This function can be called every time an
 * expected INIT message is received from the RG.
 *
 * Return: 0 if success, or a Linux error code
 */
static int aw_get_init_data(struct mali_gpu_ptm_aw *awdev,
	struct init_msg *init_data)
{
	if (WARN_ON(!init_data || !awdev))
		return -EINVAL;

	lockdep_assert_held(&awdev->prot_data.mutex);
	/* Store the ini message data */
	init_data->ack = awdev->prot_data.init_msg.ack;
	init_data->version = awdev->prot_data.init_msg.version;
	init_data->max_l2_slices = awdev->prot_data.init_msg.max_l2_slices;
	init_data->max_core_mask = awdev->prot_data.init_msg.max_core_mask;
	init_data->received = awdev->prot_data.init_msg.received;

	return 0;
}

/**
 * send_message() - Write message to the sending buffer and try a sychronous
 *                  send if the handshake is done.
 * @awdev:    Access Window module internal data.
 * @message:  Pointer to 64-bit message to send.
 *
 * Writes the message to the sending buffer and try a synchronous register write
 * to send the message if the handshake is done. The prot_data.mutex
 * must be held.
 */
static void send_message(struct mali_gpu_ptm_aw *awdev, uint64_t *message)
{
	struct ptm_msg_handler *msg_handler;
	uint8_t error;

	if (WARN_ON(!awdev || !message))
		return;

	lockdep_assert_held(&awdev->prot_data.mutex);
	msg_handler = &awdev->msg_handler;

	error = ptm_msg_buff_write(&msg_handler->send_msgs, 0,
			*message);
	if (error > 0)
		dev_dbg(msg_handler->dev,
			"Overriding outgoing PTM message for RG\n");
	ptm_msg_send(msg_handler, 0);
}

/**
 * vm_arb_init() - Sends VM_ARB_INIT message to the Arbiter.
 * @awdev:       Access Window module internal data.
 * @ack:         Protocol handshake ack.
 * @version:     Protocol handshake version.
 * @gpu_request: GPU request flag sent as part of the INIT message.
 *
 * Sends VM_ARB_INIT to the arbiter via AW registers.
 */
static void vm_arb_init(struct mali_gpu_ptm_aw *awdev, uint8_t ack,
	uint8_t version, bool gpu_request)
{
	uint64_t msg_payload;

	if (WARN_ON(!awdev))
		return;

	lockdep_assert_held(&awdev->prot_data.mutex);

	if (vm_arb_init_build_msg(ack, version, (uint8_t)gpu_request,
				  &msg_payload))
		return;

	send_message(awdev, &msg_payload);
}

/**
 * aw_respond_to_init() - Response to INIT message to conclude the protocol
 *                        handshake.
 * @awdev:        Access Window module internal data.
 * @recv_version: Protocol version received from the ARB_VM_INIT.
 *
 * Responds to the INIT message received from the RG to conclude the protocol
 * handshake. This function should only be called when a ARB_VM_INIT is
 * received with ack = 0.
 */
static int aw_respond_to_init(struct mali_gpu_ptm_aw *awdev,
	uint8_t recv_version)
{
	uint8_t sending_ack;
	uint8_t sending_version;
	struct ptm_msg_handler *msg_handler;
	enum ptm_aw_state state = AW_INIT;
	/* Always request the GPU while the handshake is in progress. */
	bool gpu_request = true;
	uint32_t err = 0;

	if (WARN_ON(!awdev))
		return -EINVAL;

	lockdep_assert_held(&awdev->prot_data.mutex);
	msg_handler = &awdev->msg_handler;

	/* Init response should always be with ack = 1 */
	sending_ack = 1;

	/* If the received version is not supported the handshake fails */
	if (recv_version < MIN_SUPPORTED_VERSION) {
		/* Send higher version to make sure the other side also fails */
		sending_version = MIN_SUPPORTED_VERSION;
		aw_set_state(awdev, AW_HS_FAILED);
		dev_err(awdev->dev,
			"Protocol handshake failed with RG\n");
		err = -EPERM;
	} else {
		/* Send minimum of the maximum versions */
		sending_version = (recv_version < CURRENT_VERSION) ?
			recv_version : CURRENT_VERSION;

		/* Set protocol in use */
		awdev->prot_data.version_in_use = sending_version;
	}

	aw_get_state(awdev, &state);
	switch (state) {
	case AW_HS_INITIATED:
		/* VM_ARB_INIT sent from RG and AW at the same time */
		aw_set_state(awdev, AW_GPU_REQUESTED);
		break;
	case AW_HS_INITIATED_NO_DEV:
		aw_set_state(awdev, AW_GPU_REQUESTED_NO_DEV);
		break;
	case AW_HS_FAILED:
	case AW_GPU_REQUESTED:
	case AW_GPU_REQUESTED_NO_DEV:
		break;
	default:
		/*
		 * if Handshake is done and a new ARB_VM_INIT is received it
		 * means that the Arbiter got restarted so any eventual missed
		 * gpu_request needs to be resent as part of the handshake
		 * response.
		 */
		gpu_request = awdev->prot_data.gpu_requested;
		break;
	}

	vm_arb_init(awdev, sending_ack, sending_version, gpu_request);
	return err;
}

/**
 * aw_validate_init() - Validate INIT message response to conclude the protocol
 *                      handshake.
 * @awdev:        Access Window module internal data.
 * @recv_version: Protocol version received from the ARB_VM_INIT.
 *
 * Validates the INIT message received from the RG to conclude the protocol
 * handshake. This function should only be called when a ARB_VM_INIT is received
 * with ack = 1 and the handshake is in progress.
 *
 * Return: 0 if success, or a Linux error code
 */
static int aw_validate_init(struct mali_gpu_ptm_aw *awdev,
	uint8_t recv_version)
{
	struct ptm_msg_handler *msg_handler;
	enum ptm_aw_state state = AW_INIT;

	if (WARN_ON(!awdev))
		return -EINVAL;

	lockdep_assert_held(&awdev->prot_data.mutex);
	msg_handler = &awdev->msg_handler;

	/* If the received version is not supported the handshake fails */
	if (recv_version > CURRENT_VERSION ||
			recv_version < MIN_SUPPORTED_VERSION) {
		aw_set_state(awdev, AW_HS_FAILED);
		dev_err(awdev->dev,
			"Protocol handshake failed with RG\n");
		return -EPERM;
	}

	aw_get_state(awdev, &state);
	switch (state) {
	case AW_HS_INITIATED:
		/* ARB_VM_INIT message from RG has been received and
		 * validated, so handshaking is complete. Now go into the
		 * AW_GPU_REQUESTED state to wait for the GPU to be GRANTED.
		 */
		aw_set_state(awdev, AW_GPU_REQUESTED);
		break;
	case AW_HS_INITIATED_NO_DEV:
		aw_set_state(awdev, AW_GPU_REQUESTED_NO_DEV);
		break;
	default:
		break;
	}

	/* If supported the received version should be used */
	awdev->prot_data.version_in_use = recv_version;
	/* Schedule a worker to send messages in the sending buffer */
	ptm_msg_flush_send_buffers(msg_handler);
	return 0;
}

/**
 * update_max_config() - Update kbase with max config information.
 * @awdev:        Access Window module internal data.
 *
 * Sends max config data to KBASE if update is necessary.  Init message
 * must be cached with max config data, RW semaphore must be held and
 * device must be in a registered state.
 */
static void update_max_config(struct mali_gpu_ptm_aw *awdev)
{
	struct init_msg init_data;

	if (WARN_ON(!awdev))
		return;

	lockdep_assert_held(&awdev->prot_data.mutex);
	if (awdev->prot_data.max_config_req) {
		aw_get_init_data(awdev, &init_data);
		if (init_data.received) {
			awdev->prot_data.max_config_req = false;
			mutex_unlock(&awdev->prot_data.mutex);
			awdev->vm_drv_ops.arb_vm_max_config(
					awdev->vm_dev,
					init_data.max_l2_slices,
					init_data.max_core_mask);
			mutex_lock(&awdev->prot_data.mutex);
		}
	}
}

/**
 * aw_start_handshake() - Start handshaking process with the RG.
 * @awdev:       Access Window module internal data.
 *
 * Initializes the INIT message to start the protocol handshake. This function
 * should only be called when Kbase requests the GPU and the handshake has not
 * been initialized yet.  Must only be called from AW_INIT state.
 *
 * Return: true if handshaking is complete
 */
static bool aw_start_handshake(struct mali_gpu_ptm_aw *awdev)
{
	uint8_t sending_ack;
	uint8_t sending_version;
	struct init_msg init_data;
	bool hs_complete = false;

	if (WARN_ON(!awdev))
		return false;

	lockdep_assert_held(&awdev->prot_data.mutex);

	/* Initial Init should always be with ack = 0 */
	sending_ack = 0;
	/* and version should be the current */
	sending_version = CURRENT_VERSION;

	/* Retrieve Init data received. */
	aw_get_init_data(awdev, &init_data);

	if (!init_data.received) {
		aw_set_state(awdev, AW_HS_INITIATED);
		/* Send INIT message and change the RG status. */
		vm_arb_init(awdev, sending_ack, sending_version, true);
	} else {
		aw_set_state(awdev, AW_GPU_REQUESTED);
		/* Handshake was initialized by RG, just respond it. */
		aw_respond_to_init(awdev, init_data.version);
		hs_complete = true;
	}
	return hs_complete;
}

/**
 * device_attach_handler() - Worker to trigger a KBASE reprobe.
 * @work:    work structure.
 */
static void device_attach_handler(struct work_struct *work)
{
	struct mali_gpu_ptm_aw *awdev;
	enum ptm_aw_state state = AW_INIT;

	if (!work)
		return;

	awdev = container_of(work, struct mali_gpu_ptm_aw, device_attach_work);

	if (!awdev->vm_dev || device_attach(awdev->vm_dev) !=
				DEVICE_ATTACH_DRIVER_FOUND) {
		mutex_lock(&awdev->prot_data.mutex);
		aw_get_state(awdev, &state);
		if (state == AW_MSG_QUEUED) {
			dev_err(awdev->dev, "KBase device attach failed");
			aw_set_state(awdev, AW_MSG_QUEUED_NO_DEV);
			queue_work(awdev->message_wq,
					&awdev->request_msg_work);
		}
		mutex_unlock(&awdev->prot_data.mutex);
	}
}

/**
 * process_protocol_handshake() - Process the protocol handshake
 * @awdev:    Access Window module internal data.
 * @state:    Access Window initialization state.
 * @message:  Received INIT message.
 *
 * Process an INIT message received and respond to it according to the given
 * current state.
 *
 * Return: 0 if successful, otherwise a negative error code.
 */
static int process_protocol_handshake(struct mali_gpu_ptm_aw *awdev,
	enum ptm_aw_state state, uint64_t *message)
{
	uint8_t recv_ack;
	uint8_t recv_version;
	uint8_t message_id;
	uint32_t res = 0;

	if (!awdev || !message)
		return -EINVAL;

	lockdep_assert_held(&awdev->prot_data.mutex);

	/* Decode the rest of the message. */
	if (get_msg_id(message, &message_id) ||
	    get_msg_protocol_version(message, &recv_version) ||
	    get_msg_init_ack(message, &recv_ack))
		return -EPERM;

	if (message_id != ARB_VM_INIT)
		return -EPERM;

	switch (state) {
	case AW_INIT:
		if (recv_ack == 1) {
			dev_dbg(awdev->dev, "Unexpected message from RG\n");
			return -EPERM;
		}
		/* If ack = 0 just cache the init msg on aw_set_init_data. */
		break;
	case AW_HS_INITIATED:
	case AW_HS_INITIATED_NO_DEV:
		if (recv_ack == 1) {
			/* If the other side has responded the init */
			res = aw_validate_init(awdev, recv_version);
			if (res)
				return res;
		} else {
			res = aw_respond_to_init(awdev, recv_version);
			if (res)
				return res;
		}
		break;
	case AW_HS_FAILED:
		dev_dbg(awdev->dev,
			"%s: Unexpected message from RG. Handshake failed.\n",
			__func__);
		return -EPERM;
	default:
		if (recv_ack == 0) {
			/* Arbiter has been restarted */
			res = aw_respond_to_init(awdev, recv_version);
			if (res)
				return res;
		} else {
			dev_dbg(awdev->dev, "Unexpected message from RG\n");
			return -EPERM;
		}
		break;
	}

	aw_set_init_data(awdev, message);

	/* If KBASE is registered, update the max config immediately. */
	if (state == AW_HS_INITIATED || state == AW_GPU_REQUESTED ||
				state == AW_READY)
		update_max_config(awdev);

	return 0;
}

/**
 * msg_str() - Helper function to get string for arb messages.
 * @msg_id: Message ID from RG
 *
 * Return: string representation of message ID
 */
static inline const char *msg_str(
	uint8_t msg_id)
{
	switch (msg_id) {
	case ARB_VM_INIT:
		return "ARB_VM_INIT";
	case ARB_VM_GPU_GRANTED:
		return "ARB_VM_GPU_GRANTED";
	case ARB_VM_GPU_STOP:
		return "ARB_VM_GPU_STOP";
	case ARB_VM_GPU_LOST:
		return "ARB_VM_GPU_LOST";
	default:
		return "[UnknownMsgID]";
	}
}

/**
 * process_queued_request() - Update KBASE after GPU request
 * @awdev:        Access Window module internal data.
 */
static void process_queued_request(struct mali_gpu_ptm_aw *awdev)
{
	uint8_t last_msg_id;
	struct arbiter_if_arb_vm_ops *vm_drv_ops;
	uint32_t freq;
	uint64_t msg_payload = 0;
	enum ptm_aw_state state = AW_INIT;

	last_msg_id = awdev->prot_data.last_msg_id;
	vm_drv_ops = &awdev->vm_drv_ops;

	/* Check last message received from Arbiter to determine
	 * how to respond.  The function runs in the same WQ as
	 * the Arbiter message_handler to ensure synchronization.
	 */
	dev_dbg(awdev->dev, "VM_MSG_QUEUED: Handling %s msg\n",
			msg_str(last_msg_id));
	switch (last_msg_id) {
	case ARB_VM_GPU_GRANTED:
	case ARB_VM_GPU_STOP:
		freq = awdev->prot_data.last_freq;
		aw_set_state(awdev, AW_READY);
		update_max_config(awdev);
		awdev->prot_data.gpu_requested = false;

		/* We cannot hold a mutex when calling into KBASE.  However,
		 * a read lock will still be held to protect against unregister.
		 */
		mutex_unlock(&awdev->prot_data.mutex);

		if (freq != NO_FREQ)
			vm_drv_ops->arb_vm_update_freq(awdev->vm_dev, freq);

		vm_drv_ops->arb_vm_gpu_granted(awdev->vm_dev);
		if (last_msg_id == ARB_VM_GPU_STOP)
			vm_drv_ops->arb_vm_gpu_stop(awdev->vm_dev);

		mutex_lock(&awdev->prot_data.mutex);
		break;
	case ARB_VM_GPU_LOST:
		/* GPU_LOST received before we have granted GPU to KBASE.
		 * Send stopped and request GPU again.
		 */
		dev_warn(awdev->dev,
			"GPU lost occurred before GPU could be granted\n");
		awdev->prot_data.gpu_requested = true;
		if (!vm_arb_gpu_stopped_build_msg(1, &msg_payload))
			send_message(awdev, &msg_payload);

		aw_get_state(awdev, &state);
		if (state == AW_MSG_QUEUED)
			aw_set_state(awdev, AW_GPU_REQUESTED);

		break;
	}
}

/**
 * request_msg_handler() - Worker to send messages to KBASE after request.
 * @work:    request_msg_work within awdev structure.
 *
 * Used to asynchronously send messages to KBASE based on current vm_state
 * (during initialization).  Uses the same workqueue as message_handler
 * to ensure messages sent to KBASE are serialized correctly.
 * Used to:
 * 1) Send any pending messages to complete a reprobe (the
 *    AW_MSG_QUEUED state). See #process_queued_request function.
 * 2) Send MAX_CONFIG to KBASE in response to a get_max_config request (the
 *    AW_GPU_REQUESTED and AW_READY states).
 */
static void request_msg_handler(struct work_struct *work)
{
	struct mali_gpu_ptm_aw *awdev;
	enum ptm_aw_state state = AW_INIT;
	uint64_t msg_payload = 0;

	if (WARN_ON(!work))
		return;

	awdev = container_of(work, struct mali_gpu_ptm_aw, request_msg_work);

	down_read(&awdev->vmdev_rwsem);
	mutex_lock(&awdev->prot_data.mutex);

	aw_get_state(awdev, &state);

	switch (state) {
	case AW_MSG_QUEUED:
		process_queued_request(awdev);
		break;
	case AW_MSG_QUEUED_NO_DEV:
		if (!vm_arb_gpu_stopped_build_msg(0, &msg_payload))
			send_message(awdev, &msg_payload);

		aw_set_state(awdev, AW_HS_DONE);
		break;
	case AW_GPU_REQUESTED:
	case AW_READY:
		update_max_config(awdev);
		break;
	default:
		break;
	}

	mutex_unlock(&awdev->prot_data.mutex);
	up_read(&awdev->vmdev_rwsem);
}

/**
 * init_msg_handler() - Completes protocol handshake after
 *                      receiving ARB_VM_INIT.
 * @work: Pointer to the work structure.
 */
static void init_msg_handler(struct work_struct *work)
{
	struct mali_gpu_ptm_aw *awdev;
	enum ptm_aw_state state = AW_INIT;
	uint64_t message = 0;

	if (!work)
		return;

	awdev = container_of(work, struct mali_gpu_ptm_aw, init_msg_work);

	down_read(&awdev->vmdev_rwsem);
	mutex_lock(&awdev->prot_data.mutex);
	aw_get_state(awdev, &state);

	ptm_msg_buff_read(&awdev->msg_handler.recv_msgs,
			MESSAGE_IDX_INIT, &message);

	dev_dbg(awdev->dev, "%s: state %s\n",
		__func__, aw_state_str(state));

	if (process_protocol_handshake(awdev, state, &message))
		dev_dbg(awdev->dev, "Init MSG from RG ignored\n");

	mutex_unlock(&awdev->prot_data.mutex);
	up_read(&awdev->vmdev_rwsem);
}

/**
 * message_handler() - send message to VM.
 * @work: Pointer to the work structure.
 */
static void message_handler(struct work_struct *work)
{
	struct mali_gpu_ptm_aw *awdev;
	struct arbiter_if_arb_vm_ops *vm_drv_ops;
	uint8_t msg_id = 0;
	uint64_t message = 0;
	enum ptm_aw_state state = AW_INIT;
	uint32_t freq = NO_FREQ;
	bool gpu_granted = false;

	if (!work)
		return;

	awdev = container_of(work, struct mali_gpu_ptm_aw, rx_msg_work);

	down_read(&awdev->vmdev_rwsem);
	mutex_lock(&awdev->prot_data.mutex);
	aw_get_state(awdev, &state);

	ptm_msg_buff_read(&awdev->msg_handler.recv_msgs,
			MESSAGE_IDX_RX, &message);

	if (get_msg_id(&message, &msg_id)) {
		dev_err(awdev->dev, "get_msg_id() failed\n");
		goto cleanup_mutex;
	}
	dev_dbg(awdev->dev, "%s: msg %s, state %s\n",
		__func__, msg_str(msg_id), aw_state_str(state));

	switch (msg_id) {
	case ARB_VM_GPU_GRANTED:
		get_msg_frequency(&message, &freq);
		awdev->prot_data.last_freq = freq;
		/* Fall through */
	case ARB_VM_GPU_STOP:
		/* Stop also behaves like a GPU granted as we might have
		 * missed the previous message from the RG
		 */
		gpu_granted = true;
		break;
	default:
		break;
	}

	awdev->prot_data.last_msg_id = msg_id;
	vm_drv_ops = &awdev->vm_drv_ops;

	switch (state) {
	case AW_GPU_REQUESTED:
		process_queued_request(awdev);
		goto cleanup_mutex;
	case AW_GPU_REQUESTED_NO_DEV:
		if (gpu_granted) {
			aw_set_state(awdev, AW_ATTACH_DEV);
			queue_work(awdev->device_attach_wq,
				&awdev->device_attach_work);
		}
		goto cleanup_mutex;
	case AW_READY:
		break;
	default:
		goto cleanup_mutex;
	}

	if (awdev->prot_data.gpu_requested) {
		if (msg_id != ARB_VM_GPU_GRANTED) {
			process_queued_request(awdev);
			goto cleanup_mutex;
		}
		awdev->prot_data.gpu_requested = false;
	}

	/* We cannot hold a mutex when calling into KBASE.  However,
	 * a read lock will still be held to protect against unregister.
	 */
	mutex_unlock(&awdev->prot_data.mutex);

	/* We are now in state AW_READY, so all messages received can
	 * be forwarded directly to KBASE.
	 */
	switch (msg_id) {
	case ARB_VM_GPU_GRANTED:
		/* updating frequency must happen before processing
		 * granted message to prevent a race condition with
		 * the granting process that might lead to reporting
		 * old version of frequency
		 */
		if (awdev->prot_data.last_freq != NO_FREQ)
			vm_drv_ops->arb_vm_update_freq(awdev->vm_dev,
					awdev->prot_data.last_freq);
		vm_drv_ops->arb_vm_gpu_granted(awdev->vm_dev);
		break;
	case ARB_VM_GPU_STOP:
		vm_drv_ops->arb_vm_gpu_stop(awdev->vm_dev);
		break;
	case ARB_VM_GPU_LOST:
		vm_drv_ops->arb_vm_gpu_lost(awdev->vm_dev);
		break;
	default:
		dev_err(awdev->dev, "Command not recognized 0x%08x\n",
			(uint32_t)msg_id);
		break;
	}
	up_read(&awdev->vmdev_rwsem);
	return;

cleanup_mutex:
	mutex_unlock(&awdev->prot_data.mutex);
	up_read(&awdev->vmdev_rwsem);
}

/**
 * aw_irq_handler() - Access Window interrupt handler
 * @irq:   IRQ identifier.
 * @data:  pointer to the mali_gpu_ptm_aw structure passed in when the
 *         handler was installed.
 *
 * Return: IRQ_HANDLED if handled successfully, or IRQ_NONE if not.
 *
 * Called when an interrupt arrives.
 */
static irqreturn_t aw_irq_handler(int irq, void *data)
{
	struct mali_gpu_ptm_aw *awdev;
	const struct device *dev;
	uint32_t irq_status;
	irqreturn_t ret = IRQ_HANDLED;

	if (!data)
		return IRQ_NONE;

	awdev = data;
	dev = awdev->dev;

	/* Check if correct IRQ number */
	if (irq == awdev->irqs.irq)
		irq_status = readl(awdev->regs.addr + PTM_AW_IRQ_STATUS);
	else {
		dev_err(dev, "Unknown irq %d\n", irq);
		ret = IRQ_NONE;
		goto cleanup;
	}

	/* mask out all but the interrupts we're supporting */
	irq_status &= IRQ_BIT_MASK;

	if (irq_status) {
		aw_read_incoming_msg(awdev);
	} else {
		/* GPU interrupts not handled by this device */
		ret = IRQ_NONE;
		goto cleanup;
	}

cleanup:
	/* If this is an edge-based interrupt, clear and restore the mask
	 * to create an edge in case there's another interrupt pending
	 */
	if (awdev->edge_irq) {
		iowrite32(0, awdev->regs.addr + PTM_AW_IRQ_MASK);
		iowrite32(MAX_AW_IRQ_MASK, awdev->regs.addr + PTM_AW_IRQ_MASK);
	}

	return awdev->edge_irq ? IRQ_HANDLED : ret;
}

/**
 * aw_protocol_data_init() - Initialize the protocol data struct.
 * @awdev: Access Window device structure.
 *
 * Return: 0 on success, or error code
 */
static int aw_protocol_data_init(struct mali_gpu_ptm_aw *awdev)
{
	if (!awdev) {
		dev_err(awdev->dev, "Invalid argument\n");
		return -EINVAL;
	}

	mutex_init(&(awdev->prot_data.mutex));
	/* handshake didn't happen yet */
	awdev->prot_data.state = AW_INIT;
	awdev->prot_data.gpu_requested = false;
	awdev->prot_data.init_msg.received = false;
	awdev->prot_data.last_freq = NO_FREQ;
	awdev->prot_data.last_msg_id = VM_ARB_INIT;
	awdev->prot_data.max_config_req = false;
	return 0;
}

/**
 * initialize_communication() - Initialize communication with the assigned AWs.
 * @awdev: Access Window device structure.
 *
 * Return: 0 on success, or error code
 */
static int initialize_communication(struct mali_gpu_ptm_aw *awdev)
{
	uint32_t irq_status;

	if (!awdev)
		return -EINVAL;

	/* Read the IRQ raw status to verify any message received before */
	irq_status = readl(awdev->regs.addr + PTM_AW_IRQ_RAWSTAT);

	if (irq_status) {
		aw_read_incoming_msg(awdev);

		/* Synchronously process incoming message */
		flush_workqueue(awdev->message_wq);
	}

	return 0;
}

/**
 * register_vm_dev() - Register VM device driver callbacks.
 * @arbif_dev: The arbiter interface we are registering device callbacks
 * @dev:       The device structure to supply in the callbacks.
 * @ops:       The callbacks that the device driver supports
 *             (none are optional).
 *
 * Return: 0 if success, or a Linux error code
 */
static int register_vm_dev(struct arbiter_if_dev *arbif_dev,
		struct device *dev, struct arbiter_if_arb_vm_ops *ops)
{
	struct mali_gpu_ptm_aw *awdev;

	if (!arbif_dev || !dev || !ops)
		return -EINVAL;

	if (!ops->arb_vm_gpu_granted || !ops->arb_vm_gpu_lost ||
			!ops->arb_vm_gpu_stop || !ops->arb_vm_max_config ||
			!ops->arb_vm_update_freq)
		return -EINVAL;

	awdev = container_of(arbif_dev, struct mali_gpu_ptm_aw, arbif_dev);

	dev_dbg(awdev->dev, "%s\n", __func__);
	down_write(&awdev->vmdev_rwsem);
	mutex_lock(&awdev->prot_data.mutex);
	awdev->vm_dev = dev;
	awdev->vm_drv_ops.arb_vm_gpu_granted = ops->arb_vm_gpu_granted;
	awdev->vm_drv_ops.arb_vm_gpu_lost = ops->arb_vm_gpu_lost;
	awdev->vm_drv_ops.arb_vm_gpu_stop = ops->arb_vm_gpu_stop;
	awdev->vm_drv_ops.arb_vm_max_config = ops->arb_vm_max_config;
	awdev->vm_drv_ops.arb_vm_update_freq = ops->arb_vm_update_freq;
	awdev->prot_data.max_config_req = true;
	mutex_unlock(&awdev->prot_data.mutex);
	up_write(&awdev->vmdev_rwsem);

	return 0;
}

/**
 * unregister_vm_dev() - Unregister VM device driver callbacks.
 * @arbif_dev: The arbiter interface device.
 */
static void unregister_vm_dev(struct arbiter_if_dev *arbif_dev)
{
	struct mali_gpu_ptm_aw *awdev;
	enum ptm_aw_state state = AW_INIT;

	if (WARN_ON(!arbif_dev))
		return;

	awdev = container_of(arbif_dev, struct mali_gpu_ptm_aw, arbif_dev);
	dev_dbg(awdev->dev, "%s\n", __func__);
	down_write(&awdev->vmdev_rwsem);
	mutex_lock(&awdev->prot_data.mutex);
	awdev->vm_drv_ops.arb_vm_gpu_granted = NULL;
	awdev->vm_drv_ops.arb_vm_gpu_lost = NULL;
	awdev->vm_drv_ops.arb_vm_gpu_stop = NULL;
	awdev->vm_drv_ops.arb_vm_max_config = NULL;
	awdev->vm_drv_ops.arb_vm_update_freq = NULL;
	aw_get_state(awdev, &state);
	switch (state) {
	case AW_HS_INITIATED:
		aw_set_state(awdev, AW_HS_INITIATED_NO_DEV);
		break;
	case AW_GPU_REQUESTED:
		aw_set_state(awdev, AW_GPU_REQUESTED_NO_DEV);
		break;
	case AW_MSG_QUEUED:
		aw_set_state(awdev, AW_MSG_QUEUED_NO_DEV);
		break;
	case AW_READY:
		aw_set_state(awdev, AW_HS_DONE);
		break;
	default:
		break;
	}
	mutex_unlock(&awdev->prot_data.mutex);
	up_write(&awdev->vmdev_rwsem);
}

/**
 * get_max_config() - VM request max config.
 * @arbif_dev: The arbiter interface we are registering device callbacks
 *
 * Sends VM_ARB_INIT to the arbiter via AW registers.
 * See #vm_arb_get_max_config in mali_arbif_adapter.h
 */
static void get_max_config(struct arbiter_if_dev *arbif_dev)
{
	struct mali_gpu_ptm_aw *awdev;
	enum ptm_aw_state state = AW_INIT;

	if (WARN_ON(!arbif_dev))
		return;

	awdev = container_of(arbif_dev, struct mali_gpu_ptm_aw, arbif_dev);
	if (WARN_ON(!awdev))
		return;

	mutex_lock(&awdev->prot_data.mutex);
	aw_get_state(awdev, &state);
	awdev->prot_data.max_config_req = true;
	mutex_unlock(&awdev->prot_data.mutex);
	/* Schedule WQ to resend max_config */
	if (state == AW_GPU_REQUESTED || state == AW_READY)
		queue_work(awdev->message_wq, &awdev->request_msg_work);
}

/**
 * gpu_request() - VM request for GPU access.
 * @arbif_dev: The arbiter interface we are registering device callbacks
 *
 * Sends VM_ARB_GPU_REQUEST if the handshake protocol has been done before or it
 * sends VM_ARB_INIT with gpu request to initialize/complete the protocol
 * handshake.
 *
 * See #vm_arb_gpu_request in mali_arbif_adapter.h
 */
static void gpu_request(struct arbiter_if_dev *arbif_dev)
{
	uint64_t msg_payload;
	struct mali_gpu_ptm_aw *awdev;
	enum ptm_aw_state state = AW_INIT;
	struct ptm_msg_handler *msg_handler;
	/* TO-DO: priority is not implemented yet. See AUTOSW-358 */
	uint8_t priority = 0;
	bool schedule_wq = false;

	if (WARN_ON(!arbif_dev))
		return;

	awdev = container_of(arbif_dev, struct mali_gpu_ptm_aw, arbif_dev);

	dev_dbg(awdev->dev, "%s\n", __func__);
	mutex_lock(&awdev->prot_data.mutex);

	msg_handler = &awdev->msg_handler;
	/* Retrieve AW State */
	aw_get_state(awdev, &state);

	switch (state) {
	case AW_INIT:
		/* KBASE has requested the GPU but the handshaking has not yet
		 * been initialized. We can transition into one of the
		 * following states from here:
		 * AW_GPU_REQUESTED - We have already received a valid INIT
		 * message from the RG. We send a INIT response back requesting
		 * the GPU at the same time.
		 * AW_HS_INITIATED - We have not yet received an INIT message
		 * from the RG so an INIT message has been sent to initiate the
		 * handshake.
		 * AW_HS_FAILED -  We have received an INIT message from the
		 * RG but it was not valid.
		 */
		schedule_wq = aw_start_handshake(awdev);
		break;
	case AW_HS_INITIATED_NO_DEV:
		aw_set_state(awdev, AW_HS_INITIATED);
		break;
	case AW_GPU_REQUESTED_NO_DEV:
		aw_set_state(awdev, AW_GPU_REQUESTED);
		break;
	case AW_ATTACH_DEV:
		aw_set_state(awdev, AW_MSG_QUEUED);
		schedule_wq = true;
		break;
	case AW_MSG_QUEUED_NO_DEV:
		aw_set_state(awdev, AW_MSG_QUEUED);
		break;
	case AW_HS_DONE:
	case AW_READY:
	case AW_GPU_REQUESTED:
		if (state == AW_HS_DONE)
			aw_set_state(awdev, AW_GPU_REQUESTED);

		awdev->prot_data.gpu_requested = true;
		if (vm_arb_gpu_request_build_msg(priority, &msg_payload))
			goto cleanup_mutex;

		send_message(awdev, &msg_payload);
		break;
	default:
		dev_dbg(awdev->dev, "%s, message can't be sent now\n",
				aw_state_str(state));
		break;
	}

cleanup_mutex:
	mutex_unlock(&awdev->prot_data.mutex);
	/* Queue worker outside of lock to avoid unnecessary thread
	 * reschedule when WQ runs.
	 */
	if (schedule_wq)
		queue_work(awdev->message_wq, &awdev->request_msg_work);
}

/**
 * gpu_active() - VM indicating that GPU now in the active state.
 * @arbif_dev: The arbiter interface device.
 *
 * Sends VM_ARB_GPU_ACTIVE to the arbiter via AW registers.
 * See #vm_arb_gpu_active in mali_arbif_adapter.h
 */
static void gpu_active(struct arbiter_if_dev *arbif_dev)
{
	uint64_t msg_payload;
	struct mali_gpu_ptm_aw *awdev;
	/* TO-DO: priority is not implemented yet. See AUTOSW-358 */
	uint8_t priority = 0;
	enum ptm_aw_state state = AW_INIT;

	if (WARN_ON(!arbif_dev))
		return;

	awdev = container_of(arbif_dev, struct mali_gpu_ptm_aw, arbif_dev);
	if (WARN_ON(!awdev))
		return;

	if (vm_arb_gpu_active_build_msg(priority, &msg_payload))
		return;

	mutex_lock(&awdev->prot_data.mutex);
	aw_get_state(awdev, &state);
	if (state == AW_READY)
		send_message(awdev, &msg_payload);
	mutex_unlock(&awdev->prot_data.mutex);
}

/**
 * gpu_idle() - VM indicating that GPU is now in the idle state.
 * @arbif_dev: The arbiter interface device.
 *
 * Sends VM_ARB_GPU_IDLE to the arbiter via AW registers.
 * See #vm_arb_gpu_idle in mali_arbif_adapter.h
 */
static void gpu_idle(struct arbiter_if_dev *arbif_dev)
{
	uint64_t msg_payload;
	struct mali_gpu_ptm_aw *awdev;
	enum ptm_aw_state state = AW_INIT;

	if (WARN_ON(!arbif_dev))
		return;

	awdev = container_of(arbif_dev, struct mali_gpu_ptm_aw, arbif_dev);
	if (WARN_ON(!awdev))
		return;

	if (vm_arb_gpu_idle_build_msg(&msg_payload))
		return;

	mutex_lock(&awdev->prot_data.mutex);
	aw_get_state(awdev, &state);
	if (state == AW_READY)
		send_message(awdev, &msg_payload);
	mutex_unlock(&awdev->prot_data.mutex);
}

/**
 * gpu_stopped() - VM indicates GPU has stopped.
 * @arbif_dev:    The arbiter interface device.
 * @gpu_required: If > 0 the arbiter will give the gpu back
 *                eventually to the VM without requiring another request
 *                to be sent.
 *
 * Sends VM_ARB_GPU_STOPPED to the arbiter via AW registers.
 * See #vm_arb_gpu_stopped in mali_arbif_adapter.h
 */
static void gpu_stopped(struct arbiter_if_dev *arbif_dev, u8 gpu_required)
{
	uint64_t msg_payload;
	struct mali_gpu_ptm_aw *awdev;
	enum ptm_aw_state state = AW_INIT;

	if (WARN_ON(!arbif_dev))
		return;

	awdev = container_of(arbif_dev, struct mali_gpu_ptm_aw, arbif_dev);
	if (WARN_ON(!awdev))
		return;

	dev_dbg(awdev->dev, "%s\n", __func__);

	mutex_lock(&awdev->prot_data.mutex);
	aw_get_state(awdev, &state);
	awdev->prot_data.gpu_requested = (gpu_required == 0 ? false : true);
	switch (state) {
	case AW_HS_INITIATED:
		aw_set_state(awdev, AW_HS_INITIATED_NO_DEV);
		break;
	case AW_GPU_REQUESTED:
		aw_set_state(awdev, AW_GPU_REQUESTED_NO_DEV);
		break;
	case AW_READY:
		if (!vm_arb_gpu_stopped_build_msg(gpu_required,
				&msg_payload))
			send_message(awdev, &msg_payload);
		break;
	default:
		break;
	}
	mutex_unlock(&awdev->prot_data.mutex);
}

/**
 * free_awdev() - Cleans up resources
 * @awdev: Internal aw data.
 */
static void free_awdev(struct mali_gpu_ptm_aw *awdev)
{
	struct device *dev;

	if (WARN_ON(!awdev))
		return;

	dev = awdev->dev;
	dev_set_drvdata(awdev->dev, NULL);
	devm_kfree(dev, awdev);
}


/**
 * aw_probe - Initialize the aw device
 * @pdev: The platform device
 *
 * Called when device is matched in device tree, allocate the resources
 * for the device and set up the proper callbacks in the arbiter_if struct.
 *
 * Return: 0 if success, or a Linux error code
 */
static int aw_probe(struct platform_device *pdev)
{
	int err;
	struct device_node *arbif_node;
	struct mali_gpu_ptm_aw *awdev;
	static struct resource *aw_reg_res;
	struct device *dev;
	uint32_t aw_reg_ptm_id;
	struct resource *irq_res;
	void __iomem *addr;

	if (WARN_ON(!pdev))
		return -EINVAL;

	dev = &pdev->dev;
	dev_dbg(dev, "%s\n", __func__);

	arbif_node = pdev->dev.of_node;

	/* Set up access window data structure */
	awdev = devm_kzalloc(dev, sizeof(struct mali_gpu_ptm_aw), GFP_KERNEL);
	if (awdev == NULL) {
		err = -ENOMEM;
		dev_err(dev, "Error: memory allocation failed\n");
		goto fail_request_mem;
	}

	awdev->dev = dev;
	init_rwsem(&awdev->vmdev_rwsem);

	/* Register our callbacks with the VM */
	awdev->arbif_dev.vm_ops.vm_arb_register_dev = register_vm_dev;
	awdev->arbif_dev.vm_ops.vm_arb_unregister_dev = unregister_vm_dev;
	awdev->arbif_dev.vm_ops.vm_arb_get_max_config = get_max_config;
	awdev->arbif_dev.vm_ops.vm_arb_gpu_request = gpu_request;
	awdev->arbif_dev.vm_ops.vm_arb_gpu_active = gpu_active;
	awdev->arbif_dev.vm_ops.vm_arb_gpu_idle = gpu_idle;
	awdev->arbif_dev.vm_ops.vm_arb_gpu_stopped = gpu_stopped;

	aw_reg_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!aw_reg_res) {
		dev_err(dev, "Invalid register resource.\n");
		err = -ENOENT;
		/* If the platform_get_resource fails, free the device data */
		goto fail_request_mem;
	}

	/* Request the memory region */
	if (!request_mem_region(aw_reg_res->start, resource_size(aw_reg_res),
				dev_name(dev))) {
		dev_err(dev, "Register window unavailable.\n");
		err = -EIO;
		/* If the request_mem_region fails, free the device data */
		goto fail_request_mem;
	}

	/* ioremap to the virtual address */
	addr = ioremap(aw_reg_res->start, resource_size(aw_reg_res));
	if (!addr) {
		dev_err(dev, "Can't remap register window.\n");
		err = -EINVAL;
		/* If the ioremap fails, release the memory region */
		goto fail_ioremap;
	}

	/* Initialise the PTM message handler */
	ptm_msg_handler_init(&awdev->msg_handler,
			dev,
			addr + PTM_AW_MESSAGE,
			MESSAGE_IDX_COUNT);

	aw_reg_ptm_id = ioread32(addr + PTM_ID);
	/* Mask the status, minor, major versions and arch_rev. */
	aw_reg_ptm_id &= PTM_VER_MASK;
	if (aw_reg_ptm_id != (PTM_SUPPORTED_VER & PTM_VER_MASK)) {
		dev_err(dev, "Unsupported partition manager version.\n");
		err = -EINVAL;
		/* If the device ID is not supported, iounmap and release the
		 * memory region.
		 */
		goto cleanup;
	}

	awdev->regs.res = aw_reg_res;
	awdev->regs.addr = addr;

	irq_res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!irq_res) {
		dev_err(dev, "No IRQ resource at index %d\n", 0);
		err = -ENOENT;
		goto cleanup;
	}

	awdev->irqs.irq = irq_res->start;
	awdev->irqs.flags = irq_res->flags & IRQF_TRIGGER_MASK;

	/* Initialize protocol data */
	err = aw_protocol_data_init(awdev);
	if (err) {
		dev_err(dev, "Failed to initialize protocol data %d", err);
		goto cleanup;
	}

	awdev->message_wq =
		alloc_workqueue("aw gpu message", WQ_HIGHPRI | WQ_UNBOUND, 1);

	/* Initialise our work items for the message_wq */
	INIT_WORK(&awdev->rx_msg_work, message_handler);
	INIT_WORK(&awdev->init_msg_work, init_msg_handler);
	INIT_WORK(&awdev->request_msg_work, request_msg_handler);
	awdev->device_attach_wq =
		alloc_workqueue("aw device attach", WQ_HIGHPRI | WQ_UNBOUND, 1);
	INIT_WORK(&awdev->device_attach_work, device_attach_handler);

	platform_set_drvdata(pdev, &awdev->arbif_dev);

	/*
	 * Initializing the communication by reading any messages received
	 * before probe.
	 */
	if (initialize_communication(awdev))
		dev_dbg(dev,
			"Error initializing the communication layer.\n");

	err = request_irq(awdev->irqs.irq,
		aw_irq_handler,
		awdev->irqs.flags | IRQF_SHARED,
		dev_name(dev), awdev);

	if (err) {
		destroy_workqueue(awdev->message_wq);
		awdev->message_wq = NULL;
		destroy_workqueue(awdev->device_attach_wq);
		awdev->device_attach_wq = NULL;
		dev_err(dev, "Can't request interrupt.\n");
		err = -ENOENT;
		/* If request_irq fails, free the device data */
		goto cleanup;
	}
	awdev->edge_irq = EDGE_IRQ(awdev->irqs.flags);

	/* Enable all IRQs */
	iowrite32(MAX_AW_IRQ_MASK, awdev->regs.addr + PTM_AW_IRQ_MASK);


	dev_info(dev, "Probed\n");
	return 0;

cleanup:
	iounmap(addr);
fail_ioremap:
	release_mem_region(aw_reg_res->start, resource_size(aw_reg_res));
fail_request_mem:
	free_awdev(awdev);
	return err;
}

/**
 * aw_remove() - Remove the aw device
 * @pdev: The platform device
 *
 * Called when the device is being unloaded to do any cleanup
 *
 * Return: Linux error code
 */
static int aw_remove(struct platform_device *pdev)
{
	struct arbiter_if_dev *arbif_dev;
	struct mali_gpu_ptm_aw *awdev;
	struct resource *res;
	void __iomem *addr;
	enum ptm_aw_state state = AW_INIT;
	uint64_t msg_payload;

	if (WARN_ON(!pdev))
		return -EINVAL;

	arbif_dev = platform_get_drvdata(pdev);
	if (WARN_ON(!arbif_dev))
		return -EFAULT;

	awdev = container_of(arbif_dev, struct mali_gpu_ptm_aw, arbif_dev);
	if (awdev) {
		dev_dbg(awdev->dev, "%s\n", __func__);
		mutex_lock(&awdev->prot_data.mutex);
		/* Ensure we are in a good state before removing the device */
		aw_get_state(awdev, &state);
		switch (state) {
		case AW_HS_FAILED:
		case AW_INIT:
		case AW_HS_DONE:
			break;
		default:
			aw_set_state(awdev, AW_INIT);
			if (!vm_arb_gpu_stopped_build_msg(0, &msg_payload))
				send_message(awdev, &msg_payload);

			break;
		}
		mutex_unlock(&awdev->prot_data.mutex);

		/* Disable all IRQs */
		iowrite32(0x0, awdev->regs.addr + PTM_AW_IRQ_MASK);

		ptm_msg_handler_destroy(&awdev->msg_handler);

		if (awdev->message_wq) {
			flush_workqueue(awdev->message_wq);
			destroy_workqueue(awdev->message_wq);
			awdev->message_wq = NULL;
		}
		if (awdev->device_attach_wq) {
			flush_workqueue(awdev->device_attach_wq);
			destroy_workqueue(awdev->device_attach_wq);
			awdev->device_attach_wq = NULL;
		}
		addr = awdev->regs.addr;
		res = awdev->regs.res;
		free_irq(awdev->irqs.irq, awdev);
		if (res) {
			iounmap(addr);
			release_mem_region(res->start, resource_size(res));
		}


		free_awdev(awdev);
	}
	return 0;
}

/*
 * aw_dt_match: Match the platform device with the Device Tree.
 */
static const struct of_device_id aw_dt_match[] = {
	{ .compatible = AW_MESSAGE_DT_NAME },
	{}
};

/*
 * aw_platform_driver: Platform driver data.
 */
static struct platform_driver aw_platform_driver = {
	.probe = aw_probe,
	.remove = aw_remove,
	.driver = {
		.name = "mali_gpu_aw",
		.of_match_table = aw_dt_match,
	},
};

/**
 * mali_gpu_aw_init - Register platform driver
 *
 * Return: See definition of platform_driver_register()
 */
static int __init mali_gpu_aw_init(void)
{
	return platform_driver_register(&aw_platform_driver);
}
module_init(mali_gpu_aw_init);

/**
 * mali_gpu_aw_exit() - Unregister platform driver
 */
static void __exit mali_gpu_aw_exit(void)
{
	platform_driver_unregister(&aw_platform_driver);
}
module_exit(mali_gpu_aw_exit);

MODULE_DESCRIPTION("Access Window driver.");
MODULE_AUTHOR("ARM Ltd.");
MODULE_LICENSE("Dual MIT/GPL");
MODULE_ALIAS("mali-gpu-aw");
MODULE_VERSION("1.0");
