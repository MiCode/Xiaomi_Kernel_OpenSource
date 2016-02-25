/* Copyright (c) 2008-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


#ifndef ____ADRENO_DISPATCHER_H
#define ____ADRENO_DISPATCHER_H

/* Time to allow preemption to complete (in ms) */
#define ADRENO_DISPATCH_PREEMPT_TIMEOUT 10000

extern unsigned int adreno_disp_preempt_fair_sched;
extern unsigned int adreno_cmdbatch_timeout;

/**
 * enum adreno_dispatcher_preempt_states - States of dispatcher for ringbuffer
 * preemption
 * @ADRENO_DISPATCHER_PREEMPT_CLEAR: No preemption is underway,
 * only 1 preemption can be underway at any point
 * @ADRENO_DISPATCHER_PREEMPT_TRIGGERED: A preemption is underway
 * @ADRENO_DISPATCHER_PREEMPT_COMPLETE: A preemption has just completed
 */
enum adreno_dispatcher_preempt_states {
	ADRENO_DISPATCHER_PREEMPT_CLEAR = 0,
	ADRENO_DISPATCHER_PREEMPT_TRIGGERED,
	ADRENO_DISPATCHER_PREEMPT_COMPLETE,
};

/**
 * enum adreno_dispatcher_starve_timer_states - Starvation control states of
 * a RB
 * @ADRENO_DISPATCHER_RB_STARVE_TIMER_UNINIT: Uninitialized, starvation control
 * is not operating
 * @ADRENO_DISPATCHER_RB_STARVE_TIMER_INIT: Starvation timer is initialized
 * and counting
 * @ADRENO_DISPATCHER_RB_STARVE_TIMER_ELAPSED: The starvation timer has elapsed
 * this state indicates that the RB is starved
 * @ADRENO_DISPATCHER_RB_STARVE_TIMER_SCHEDULED: RB is scheduled on the device
 * and will remain scheduled for a minimum time slice when in this state.
 */
enum adreno_dispatcher_starve_timer_states {
	ADRENO_DISPATCHER_RB_STARVE_TIMER_UNINIT = 0,
	ADRENO_DISPATCHER_RB_STARVE_TIMER_INIT = 1,
	ADRENO_DISPATCHER_RB_STARVE_TIMER_ELAPSED = 2,
	ADRENO_DISPATCHER_RB_STARVE_TIMER_SCHEDULED = 3,
};

/*
 * Maximum size of the dispatcher ringbuffer - the actual inflight size will be
 * smaller then this but this size will allow for a larger range of inflight
 * sizes that can be chosen at runtime
 */

#define ADRENO_DISPATCH_CMDQUEUE_SIZE 128

#define CMDQUEUE_NEXT(_i, _s) (((_i) + 1) % (_s))

#define ACTIVE_CONTEXT_LIST_MAX 2

struct adreno_context_list {
	unsigned int id;
	unsigned long jiffies;
};

/**
 * struct adreno_dispatcher_cmdqueue - List of commands for a RB level
 * @cmd_q: List of command batches submitted to dispatcher
 * @inflight: Number of commands inflight in this q
 * @head: Head pointer to the q
 * @tail: Queues tail pointer
 * @active_contexts: List of most recently seen contexts
 * @active_context_count: Number of active contexts in the active_contexts list
 */
struct adreno_dispatcher_cmdqueue {
	struct kgsl_cmdbatch *cmd_q[ADRENO_DISPATCH_CMDQUEUE_SIZE];
	unsigned int inflight;
	unsigned int head;
	unsigned int tail;
	struct adreno_context_list active_contexts[ACTIVE_CONTEXT_LIST_MAX];
	int active_context_count;
};

/**
 * struct adreno_dispatcher - container for the adreno GPU dispatcher
 * @mutex: Mutex to protect the structure
 * @state: Current state of the dispatcher (active or paused)
 * @timer: Timer to monitor the progress of the command batches
 * @inflight: Number of command batch operations pending in the ringbuffer
 * @fault: Non-zero if a fault was detected.
 * @pending: Priority list of contexts waiting to submit command batches
 * @plist_lock: Spin lock to protect the pending queue
 * @work: work_struct to put the dispatcher in a work queue
 * @kobj: kobject for the dispatcher directory in the device sysfs node
 * @idle_gate: Gate to wait on for dispatcher to idle
 * @preemption_state: Indicated what state the dispatcher is in, states are
 * defined by enum adreno_dispatcher_preempt_states
 * @preempt_token_submit: Indicates if a preempt token has been subnitted in
 * current ringbuffer.
 * @preempt_timer: Timer to track if preemption occured within specified time
 * @disp_preempt_fair_sched: If set then dispatcher will try to be fair to
 * starving RB's by scheduling them in and enforcing a minimum time slice
 * for every RB that is scheduled to run on the device
 */
struct adreno_dispatcher {
	struct mutex mutex;
	unsigned long priv;
	struct timer_list timer;
	struct timer_list fault_timer;
	unsigned int inflight;
	atomic_t fault;
	struct plist_head pending;
	spinlock_t plist_lock;
	struct work_struct work;
	struct kobject kobj;
	struct completion idle_gate;
	atomic_t preemption_state;
	int preempt_token_submit;
	struct timer_list preempt_timer;
	unsigned int disp_preempt_fair_sched;
};

enum adreno_dispatcher_flags {
	ADRENO_DISPATCHER_POWER = 0,
	ADRENO_DISPATCHER_ACTIVE = 1,
};

void adreno_dispatcher_start(struct kgsl_device *device);
int adreno_dispatcher_init(struct adreno_device *adreno_dev);
void adreno_dispatcher_close(struct adreno_device *adreno_dev);
int adreno_dispatcher_idle(struct adreno_device *adreno_dev);
void adreno_dispatcher_stop(struct adreno_device *adreno_dev);

int adreno_dispatcher_queue_cmd(struct adreno_device *adreno_dev,
		struct adreno_context *drawctxt, struct kgsl_cmdbatch *cmdbatch,
		uint32_t *timestamp);

void adreno_dispatcher_schedule(struct kgsl_device *device);
void adreno_dispatcher_pause(struct adreno_device *adreno_dev);
void adreno_dispatcher_queue_context(struct kgsl_device *device,
		struct adreno_context *drawctxt);
void adreno_dispatcher_preempt_callback(struct adreno_device *adreno_dev,
					int bit);
struct adreno_ringbuffer *adreno_dispatcher_get_highest_busy_rb(
					struct adreno_device *adreno_dev);
int adreno_dispatch_process_cmdqueue(struct adreno_device *adreno_dev,
				struct adreno_dispatcher_cmdqueue *dispatch_q,
				int long_ib_detect);
void adreno_preempt_process_dispatch_queue(struct adreno_device *adreno_dev,
	struct adreno_dispatcher_cmdqueue *dispatch_q);

#endif /* __ADRENO_DISPATCHER_H */
