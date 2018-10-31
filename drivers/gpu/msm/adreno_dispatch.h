/* Copyright (c) 2008-2018, The Linux Foundation. All rights reserved.
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

extern unsigned int adreno_drawobj_timeout;

/*
 * Maximum size of the dispatcher ringbuffer - the actual inflight size will be
 * smaller then this but this size will allow for a larger range of inflight
 * sizes that can be chosen at runtime
 */

#define ADRENO_DISPATCH_DRAWQUEUE_SIZE 128

#define DRAWQUEUE_NEXT(_i, _s) (((_i) + 1) % (_s))

/**
 * struct adreno_dispatcher_drawqueue - List of commands for a RB level
 * @cmd_q: List of command obj's submitted to dispatcher
 * @inflight: Number of commands inflight in this q
 * @head: Head pointer to the q
 * @tail: Queues tail pointer
 * @active_context_count: Number of active contexts seen in this rb drawqueue
 * @expires: The jiffies value at which this drawqueue has run too long
 */
struct adreno_dispatcher_drawqueue {
	struct kgsl_drawobj_cmd *cmd_q[ADRENO_DISPATCH_DRAWQUEUE_SIZE];
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
 * @timer: Timer to monitor the progress of the drawobjs
 * @inflight: Number of drawobj operations pending in the ringbuffer
 * @fault: Non-zero if a fault was detected.
 * @pending: Priority list of contexts waiting to submit drawobjs
 * @plist_lock: Spin lock to protect the pending queue
 * @work: work_struct to put the dispatcher in a work queue
 * @kobj: kobject for the dispatcher directory in the device sysfs node
 * @idle_gate: Gate to wait on for dispatcher to idle
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
	struct kthread_work work;
	struct kobject kobj;
	struct completion idle_gate;
};

enum adreno_dispatcher_flags {
	ADRENO_DISPATCHER_POWER = 0,
	ADRENO_DISPATCHER_ACTIVE = 1,
};

void adreno_dispatcher_start(struct kgsl_device *device);
void adreno_dispatcher_halt(struct kgsl_device *device);
void adreno_dispatcher_unhalt(struct kgsl_device *device);
int adreno_dispatcher_init(struct adreno_device *adreno_dev);
void adreno_dispatcher_close(struct adreno_device *adreno_dev);
int adreno_dispatcher_idle(struct adreno_device *adreno_dev);
void adreno_dispatcher_irq_fault(struct adreno_device *adreno_dev);
void adreno_dispatcher_stop(struct adreno_device *adreno_dev);
void adreno_dispatcher_stop_fault_timer(struct kgsl_device *device);

int adreno_dispatcher_queue_cmds(struct kgsl_device_private *dev_priv,
		struct kgsl_context *context, struct kgsl_drawobj *drawobj[],
		uint32_t count, uint32_t *timestamp);

void adreno_dispatcher_schedule(struct kgsl_device *device);
void adreno_dispatcher_pause(struct adreno_device *adreno_dev);
void adreno_dispatcher_queue_context(struct kgsl_device *device,
		struct adreno_context *drawctxt);
void adreno_dispatcher_preempt_callback(struct adreno_device *adreno_dev,
					int bit);
void adreno_preempt_process_dispatch_queue(struct adreno_device *adreno_dev,
	struct adreno_dispatcher_drawqueue *dispatch_q);

static inline bool adreno_drawqueue_is_empty(
		struct adreno_dispatcher_drawqueue *drawqueue)
{
	return (drawqueue != NULL && drawqueue->head == drawqueue->tail);
}
#endif /* __ADRENO_DISPATCHER_H */
