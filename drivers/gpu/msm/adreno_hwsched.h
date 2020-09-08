/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _ADRENO_HWSCHED_H_
#define _ADRENO_HWSCHED_H_

/**
 * struct adreno_hwsched - Container for the hardware scheduler
 */
struct adreno_hwsched {
	 /** @mutex: Mutex needed to run dispatcher function */
	struct mutex mutex;
	/** @flags: Container for the dispatcher internal flags */
	unsigned long flags;
	/** @inflight: Number of active submissions to the dispatch queues */
	u32 inflight;
	/** @jobs - Array of dispatch job lists for each priority level */
	struct llist_head jobs[16];
	/** @requeue - Array of lists for dispatch jobs that got requeued */
	struct llist_head requeue[16];
	/** @work: The work structure to execute dispatcher function */
	struct kthread_work work;
	/** @cmd_list: List of objects submitted to dispatch queues */
	struct list_head cmd_list;
	/** @fault: Atomic to record a fault */
	atomic_t fault;
};

enum adreno_hwsched_flags {
	ADRENO_HWSCHED_POWER = 0,
	ADRENO_HWSCHED_FAULT_RESTART,
	ADRENO_HWSCHED_FAULT_REPLAY,
};

/**
 * adreno_hwsched_trigger - Function to schedule the hwsched thread
 * @adreno_dev: A handle to adreno device
 *
 * Schedule the hw dispatcher for retiring and submitting command objects
 */
void adreno_hwsched_trigger(struct adreno_device *adreno_dev);

/**
 * adreno_hwsched_queue_cmds() - Queue a new draw object in the context
 * @dev_priv: Pointer to the device private struct
 * @context: Pointer to the kgsl draw context
 * @drawobj: Pointer to the array of drawobj's being submitted
 * @count: Number of drawobj's being submitted
 * @timestamp: Pointer to the requested timestamp
 *
 * Queue a command in the context - if there isn't any room in the queue, then
 * block until there is
 *
 * Return: 0 on success and negative error on failure to queue
 */
int adreno_hwsched_queue_cmds(struct kgsl_device_private *dev_priv,
	struct kgsl_context *context, struct kgsl_drawobj *drawobj[],
	u32 count, u32 *timestamp);
/**
 * adreno_hwsched_queue_context() - schedule a drawctxt in the hw dispatcher
 * @device: pointer to the KGSL device
 * @drawctxt: pointer to the drawctxt to schedule
 *
 * Put a draw context on the dispatcher job listse and schedule the
 * dispatcher. This is used to reschedule changes that might have been blocked
 * for sync points or other concerns
 */
void adreno_hwsched_queue_context(struct kgsl_device *device,
	struct adreno_context *drawctxt);
/**
 * adreno_hwsched_start() - activate the hwsched dispatcher
 * @adreno_dev: pointer to the adreno device
 *
 * Enable dispatcher thread to execute
 */
void adreno_hwsched_start(struct adreno_device *adreno_dev);
/**
 * adreno_hwsched_dispatcher_init() - Initialize the hwsched dispatcher
 * @adreno_dev: pointer to the adreno device
 *
 * Set up the dispatcher resources
 */
void adreno_hwsched_init(struct adreno_device *adreno_dev);

/**
 * adreno_hwsched_dispatcher_close() - close the hwsched dispatcher
 * @adreno_dev: pointer to the adreno device structure
 *
 * Free the dispatcher resources
 */
void adreno_hwsched_dispatcher_close(struct adreno_device *adreno_dev);

/**
 * adreno_hwsched_set_fault - Set hwsched fault to request recovery
 * @adreno_dev: A handle to adreno device
 */
void adreno_hwsched_set_fault(struct adreno_device *adreno_dev);

/**
 * adreno_hwsched_mark_drawobj() - Get the drawobj that faulted
 * @adreno_dev: pointer to the adreno device
 * @ctxt_id: context id of the faulty submission
 * @ts: timestamp of the faulty submission
 *
 * When we get a context bad hfi, use this function to get to the
 * faulty submission and mark the submission for snapshot purposes
 */
void adreno_hwsched_mark_drawobj(struct adreno_device *adreno_dev, u32 ctxt_id,
	u32 ts);

/**
 * adreno_hwsched_parse_fault_ib - Parse the faulty submission
 * @adreno_dev: pointer to the adreno device
 * @snapshot: Pointer to the snapshot structure
 *
 * Walk the list of active submissions to find the one that faulted and
 * parse it so that relevant command buffers can be added to the snapshot
 */
void adreno_hwsched_parse_fault_cmdobj(struct adreno_device *adreno_dev,
	struct kgsl_snapshot *snapshot);
#endif
