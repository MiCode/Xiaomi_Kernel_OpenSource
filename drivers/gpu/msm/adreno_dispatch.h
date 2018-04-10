/* Copyright (c) 2008-2016, The Linux Foundation. All rights reserved.
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

extern unsigned int adreno_disp_preempt_fair_sched;
extern unsigned int adreno_cmdbatch_timeout;
extern unsigned int adreno_dispatch_starvation_time;
extern unsigned int adreno_dispatch_time_slice;

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

/**
 * struct adreno_dispatcher_cmdqueue - List of commands for a RB level
 * @cmd_q: List of command batches submitted to dispatcher
 * @inflight: Number of commands inflight in this q
 * @head: Head pointer to the q
 * @tail: Queues tail pointer
 * @active_context_count: Number of active contexts seen in this rb cmdqueue
 * @expires: The jiffies value at which this cmdqueue has run too long
 */
struct adreno_dispatcher_cmdqueue {
	struct kgsl_cmdbatch *cmd_q[ADRENO_DISPATCH_CMDQUEUE_SIZE];
	unsigned int inflight;
	unsigned int head;
	unsigned int tail;
	int active_context_count;
	unsigned long expires;
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
void adreno_dispatcher_irq_fault(struct adreno_device *adreno_dev);
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
void adreno_preempt_process_dispatch_queue(struct adreno_device *adreno_dev,
	struct adreno_dispatcher_cmdqueue *dispatch_q);

static inline bool adreno_cmdqueue_is_empty(
		struct adreno_dispatcher_cmdqueue *cmdqueue)
{
	return cmdqueue != NULL && cmdqueue->head == cmdqueue->tail;
}
#endif /* __ADRENO_DISPATCHER_H */
