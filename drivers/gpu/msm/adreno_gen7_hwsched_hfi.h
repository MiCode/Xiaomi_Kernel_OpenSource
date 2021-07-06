/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _ADRENO_GEN7_HWSCHED_HFI_H_
#define _ADRENO_GEN7_HWSCHED_HFI_H_

/* Maximum number of IBs in a submission */
#define HWSCHED_MAX_NUMIBS \
	((HFI_MAX_MSG_SIZE - offsetof(struct hfi_issue_cmd_cmd, ibs)) \
		/ sizeof(struct hfi_issue_ib))

struct gen7_hwsched_hfi {
	struct hfi_mem_alloc_entry mem_alloc_table[32];
	u32 mem_alloc_entries;
	/** @irq_mask: Store the hfi interrupt mask */
	u32 irq_mask;
	/** @msglock: To protect the list of un-ACKed hfi packets */
	rwlock_t msglock;
	/** @msglist: List of un-ACKed hfi packets */
	struct list_head msglist;
	/** @f2h_task: Task for processing gmu fw to host packets */
	struct task_struct *f2h_task;
	/** @f2h_wq: Waitqueue for the f2h_task */
	wait_queue_head_t f2h_wq;
	/** @big_ib: GMU buffer to hold big IBs */
	struct kgsl_memdesc *big_ib;
};

struct kgsl_drawobj_cmd;

/**
 * gen7_hwsched_hfi_probe - Probe hwsched hfi resources
 * @adreno_dev: Pointer to adreno device structure
 *
 * Return: 0 on success and negative error on failure.
 */
int gen7_hwsched_hfi_probe(struct adreno_device *adreno_dev);

/**
 * gen7_hwsched_hfi_remove - Release hwsched hfi resources
 * @adreno_dev: Pointer to adreno device structure
 */
void gen7_hwsched_hfi_remove(struct adreno_device *adreno_dev);

/**
 * gen7_hwsched_hfi_init - Initialize hfi resources
 * @adreno_dev: Pointer to adreno device structure
 *
 * This function is used to initialize hfi resources
 * once before the very first gmu boot
 *
 * Return: 0 on success and negative error on failure.
 */
int gen7_hwsched_hfi_init(struct adreno_device *adreno_dev);

/**
 * gen7_hwsched_hfi_start - Start hfi resources
 * @adreno_dev: Pointer to adreno device structure
 *
 * Send the various hfi packets before booting the gpu
 *
 * Return: 0 on success and negative error on failure.
 */
int gen7_hwsched_hfi_start(struct adreno_device *adreno_dev);

/**
 * gen7_hwsched_hfi_stop - Stop the hfi resources
 * @adreno_dev: Pointer to the adreno device
 *
 * This function does the hfi cleanup when powering down the gmu
 */
void gen7_hwsched_hfi_stop(struct adreno_device *adreno_dev);

/**
 * gen7_hwched_cp_init - Send CP_INIT via HFI
 * @adreno_dev: Pointer to adreno device structure
 *
 * This function is used to send CP INIT packet and bring
 * GPU out of secure mode using hfi raw packets.
 *
 * Return: 0 on success and negative error on failure.
 */
int gen7_hwsched_cp_init(struct adreno_device *adreno_dev);

/**
 * gen7_hfi_send_cmd_async - Send an hfi packet
 * @adreno_dev: Pointer to adreno device structure
 * @data: Data to be sent in the hfi packet
 *
 * Send data in the form of an HFI packet to gmu and wait for
 * it's ack asynchronously
 *
 * Return: 0 on success and negative error on failure.
 */
int gen7_hfi_send_cmd_async(struct adreno_device *adreno_dev, void *data);

/**
 * gen7_hwsched_submit_cmdobj - Dispatch IBs to dispatch queues
 * @adreno_dev: Pointer to adreno device structure
 * @cmdobj: The command object which needs to be submitted
 *
 * This function is used to register the context if needed and submit
 * IBs to the hfi dispatch queues.

 * Return: 0 on success and negative error on failure
 */
int gen7_hwsched_submit_cmdobj(struct adreno_device *adreno_dev,
		struct kgsl_drawobj_cmd *cmdobj);

/**
 * gen7_hwsched_context_detach - Unregister a context with GMU
 * @drawctxt: Pointer to the adreno context
 *
 * This function sends context unregister HFI and waits for the ack
 * to ensure all submissions from this context have retired
 */
void gen7_hwsched_context_detach(struct adreno_context *drawctxt);

/* Helper function to get to gen7 hwsched hfi device from adreno device */
struct gen7_hwsched_hfi *to_gen7_hwsched_hfi(struct adreno_device *adreno_dev);

/**
 * gen7_hwsched_preempt_count_get - Get preemption count from GMU
 * @adreno_dev: Pointer to adreno device
 *
 * This function sends a GET_VALUE HFI packet to get the number of
 * preemptions completed since last SLUMBER exit.
 *
 * Return: Preemption count
 */
u32 gen7_hwsched_preempt_count_get(struct adreno_device *adreno_dev);

/**
 * gen7_hwsched_parse_payload - Parse payload to look up a key
 * @payload: Pointer to a payload section
 * @key: The key who's value is to be looked up
 *
 * This function parses the payload data which is a sequence
 * of key-value pairs.
 *
 * Return: The value of the key or 0 if key is not found
 */
u32 gen7_hwsched_parse_payload(struct payload_section *payload, u32 key);

/**
 * gen7_hwsched_process_msgq - Process hfi msg queue
 * @adreno_dev: Pointer to adreno device
 *
 * Process any pending firmware to host packets in the message
 * queue
 */
void gen7_hwsched_process_msgq(struct adreno_device *adreno_dev);
#endif
