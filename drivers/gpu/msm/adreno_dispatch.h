/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2008-2021, The Linux Foundation. All rights reserved.
 */

#ifndef ____ADRENO_DISPATCHER_H
#define ____ADRENO_DISPATCHER_H

#include <linux/kobject.h>
#include <linux/kthread.h>
#include <linux/llist.h>

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
 * struct adreno_dispatch_job - An instance of work for the dispatcher
 * @node: llist node for the list of jobs
 * @drawctxt: A pointer to an adreno draw context
 *
 * This struct defines work for the dispatcher. When a drawctxt is ready to send
 * commands it will attach itself to the appropriate list for it's priority.
 * The dispatcher will process all jobs on each priority every time it goes
 * through a dispatch cycle
 */
struct adreno_dispatch_job {
	struct llist_node node;
	struct adreno_context *drawctxt;
};

/**
 * struct adreno_dispatcher - container for the adreno GPU dispatcher
 * @mutex: Mutex to protect the structure
 * @state: Current state of the dispatcher (active or paused)
 * @timer: Timer to monitor the progress of the drawobjs
 * @inflight: Number of drawobj operations pending in the ringbuffer
 * @fault: Non-zero if a fault was detected.
 * @pending: Priority list of contexts waiting to submit drawobjs
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
	/** @jobs - Array of dispatch job lists for each priority level */
	struct llist_head jobs[16];
	/** @requeue - Array of lists for dispatch jobs that got requeued */
	struct llist_head requeue[16];
	struct kthread_work work;
	struct kobject kobj;
	struct completion idle_gate;
	struct kthread_worker *worker;
};

enum adreno_dispatcher_flags {
	ADRENO_DISPATCHER_POWER = 0,
	ADRENO_DISPATCHER_ACTIVE,
	ADRENO_DISPATCHER_INIT,
};

struct adreno_device;
struct kgsl_device;

void adreno_dispatcher_start(struct kgsl_device *device);
int adreno_dispatcher_init(struct adreno_device *adreno_dev);
int adreno_dispatcher_idle(struct adreno_device *adreno_dev);
void adreno_dispatcher_stop(struct adreno_device *adreno_dev);

void adreno_dispatcher_start_fault_timer(struct adreno_device *adreno_dev);
void adreno_dispatcher_stop_fault_timer(struct kgsl_device *device);

void adreno_dispatcher_schedule(struct kgsl_device *device);

/**
 * adreno_dispatcher_fault - Set dispatcher fault to request recovery
 * @adreno_dev: A handle to adreno device
 * @fault: The type of fault
 */
void adreno_dispatcher_fault(struct adreno_device *adreno_dev, u32 fault);
#endif /* __ADRENO_DISPATCHER_H */
