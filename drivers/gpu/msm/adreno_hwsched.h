/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _ADRENO_HWSCHED_H_
#define _ADRENO_HWSCHED_H_

#include <linux/soc/qcom/msm_hw_fence.h>

#include "kgsl_sync.h"

/**
 * struct adreno_hw_fence_entry - A structure to store hardware fence and the context
 */
struct adreno_hw_fence_entry {
	/** @kfence: Pointer to the kgsl fence */
	struct kgsl_sync_fence *kfence;
	/** @drawctxt: Pointer to the context */
	struct adreno_context *drawctxt;
	/** @node: list node to add it to a list */
	struct list_head node;
};

/**
 * struct adreno_hwsched_ops - Function table to hook hwscheduler things
 * to target specific routines
 */
struct adreno_hwsched_ops {
	/**
	 * @submit_drawobj - Target specific function to submit IBs to hardware
	 */
	int (*submit_drawobj)(struct adreno_device *adreno_dev,
		struct kgsl_drawobj *drawobj);
	/**
	 * @preempt_count - Target specific function to get preemption count
	 */
	u32 (*preempt_count)(struct adreno_device *adreno_dev);
	/**
	 * @send_hw_fence - Target specific function to send hardware fence
	 * info to the GMU
	 */
	int (*send_hw_fence)(struct adreno_device *adreno_dev,
		struct adreno_hw_fence_entry *entry);

};

/**
 * struct adreno_hw_fence - Container for hardware fences instance
 */
struct adreno_hw_fence {
	/** @handle: Handle for hardware fences */
	void *handle;
	/** @descriptor: Memory descriptor for hardware fences */
	struct msm_hw_fence_mem_addr mem_descriptor;
	/** @memdesc: Kgsl memory descriptor for hardware fences queue */
	struct kgsl_memdesc memdesc;
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
	/** @recurring_cmdobj: Recurring commmand object sent to GMU */
	struct kgsl_drawobj_cmd *recurring_cmdobj;
	/** @lsr_timer: Timer struct to schedule lsr work */
	struct timer_list lsr_timer;
	/** @lsr_check_ws: Lsr work to update power stats */
	struct work_struct lsr_check_ws;
	/** @hw_fence: Container for the hw fences instance */
	struct adreno_hw_fence hw_fence;
	/** @hw_fence_cache: kmem cache for storing hardware output fences */
	struct kmem_cache *hw_fence_cache;
	/** @hw_fence_list: List of hardware fences sent to GMU */
	struct list_head hw_fence_list;
	/** @hw_fence_count: Number of hardware fences that haven't yet been sent to Tx Queue */
	u32 hw_fence_count;

};

/*
 * This value is based on maximum number of IBs that can fit
 * in the ringbuffer.
 */
#define HWSCHED_MAX_IBS 2000

enum adreno_hwsched_flags {
	ADRENO_HWSCHED_POWER = 0,
	ADRENO_HWSCHED_ACTIVE,
	ADRENO_HWSCHED_CTX_BAD_LEGACY,
	ADRENO_HWSCHED_CONTEXT_QUEUE,
	ADRENO_HWSCHED_HW_FENCE,
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

static inline bool hwsched_in_fault(struct adreno_hwsched *hwsched)
{
	/* make sure we're reading the latest value */
	smp_rmb();
	return atomic_read(&hwsched->fault) != 0;
}

void adreno_hwsched_retire_cmdobj(struct adreno_hwsched *hwsched,
	struct kgsl_drawobj_cmd *cmdobj);

bool adreno_hwsched_context_queue_enabled(struct adreno_device *adreno_dev);

/**
 * adreno_hwsched_register_hw_fence - Register GPU as a hardware fence client
 * @adreno_dev: pointer to the adreno device
 *
 * Register with the hardware fence driver to be able to trigger and wait
 * for hardware fences. Also, set up the memory descriptor for mapping the
 * client queue to the GMU.
 */
void adreno_hwsched_register_hw_fence(struct adreno_device *adreno_dev);

/**
 * adreno_hwsched_deregister_hw_fence - Deregister GPU as a hardware fence client
 * @adreno_dev: pointer to the adreno device
 *
 * Deregister with the hardware fence driver and free up any resources allocated
 * as part of registering with the hardware fence driver
 */
void adreno_hwsched_deregister_hw_fence(struct adreno_device *adreno_dev);

/**
 * adreno_hwsched_remove_hw_fence_entry - Remove hardware fence entry
 * @adreno_dev: pointer to the adreno device
 * @entry: Pointer to the hardware fence entry
 */
void adreno_hwsched_remove_hw_fence_entry(struct adreno_device *adreno_dev,
	struct adreno_hw_fence_entry *entry);

/**
 * adreno_hwsched_trigger_hw_fence_cpu - Trigger hardware fence from cpu
 * @adreno_dev: pointer to the adreno device
 * @fence: hardware fence entry to be triggered
 *
 * Trigger the hardware fence by sending it to GMU's Tx Queue and raise the
 * interrupt from GMU to APPS
 */
void adreno_hwsched_trigger_hw_fence_cpu(struct adreno_device *adreno_dev,
	struct adreno_hw_fence_entry *fence);

#endif
