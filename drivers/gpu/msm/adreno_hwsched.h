/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _ADRENO_HWSCHED_H_
#define _ADRENO_HWSCHED_H_

/**
 * struct adreno_hwsched_ops - Function table to hook hwscheduler things
 * to target specific routines
 */
struct adreno_hwsched_ops {
	/**
	 * @submit_cmdobj - Target specific function to submit IBs to hardware
	 */
	int (*submit_cmdobj)(struct adreno_device *adreno_dev,
		struct kgsl_drawobj_cmd *cmdobj);
	/**
	 * @preempt_count - Target specific function to get preemption count
	 */
	u32 (*preempt_count)(struct adreno_device *adreno_dev);
};

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
	struct kthread_worker *worker;
	/** @hwsched_ops: Container for target specific hwscheduler ops */
	const struct adreno_hwsched_ops *hwsched_ops;
	/** @ctxt_bad: Container for the context bad hfi packet */
	void *ctxt_bad;
	/** @idle_gate: Gate to wait on for hwscheduler to idle */
	struct completion idle_gate;
	/** @big_cmdobj = Points to the big IB that is inflight */
	struct kgsl_drawobj_cmd *big_cmdobj;
};

/*
 * This value is based on maximum number of IBs that can fit
 * in the ringbuffer.
 */
#define HWSCHED_MAX_IBS 2000

enum adreno_hwsched_flags {
	ADRENO_HWSCHED_POWER = 0,
	ADRENO_HWSCHED_ACTIVE,
};

/**
 * adreno_hwsched_trigger - Function to schedule the hwsched thread
 * @adreno_dev: A handle to adreno device
 *
 * Schedule the hw dispatcher for retiring and submitting command objects
 */
void adreno_hwsched_trigger(struct adreno_device *adreno_dev);

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
 * @hwsched_ops: Pointer to target specific hwsched ops
 *
 * Set up the dispatcher resources.
 * Return: 0 on success or negative on failure.
 */
int adreno_hwsched_init(struct adreno_device *adreno_dev,
	const struct adreno_hwsched_ops *hwsched_ops);

/**
 * adreno_hwsched_fault - Set hwsched fault to request recovery
 * @adreno_dev: A handle to adreno device
 * @fault: The type of fault
 */
void adreno_hwsched_fault(struct adreno_device *adreno_dev, u32 fault);

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

void adreno_hwsched_flush(struct adreno_device *adreno_dev);

/**
 * adreno_hwsched_unregister_contexts - Reset context gmu_registered bit
 * @adreno_dev: pointer to the adreno device
 *
 * Walk the list of contexts and reset the gmu_registered for all
 * contexts
 */
void adreno_hwsched_unregister_contexts(struct adreno_device *adreno_dev);

/**
 * adreno_hwsched_idle - Wait for dispatcher and hardware to become idle
 * @adreno_dev: A handle to adreno device
 *
 * Return: 0 on success or negative error on failure
 */
int adreno_hwsched_idle(struct adreno_device *adreno_dev);
#endif
