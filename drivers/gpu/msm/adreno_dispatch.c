/* Copyright (c) 2013-2017, The Linux Foundation. All rights reserved.
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

#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/err.h>

#include "kgsl.h"
#include "kgsl_sharedmem.h"
#include "adreno.h"
#include "adreno_ringbuffer.h"
#include "adreno_trace.h"
#include "kgsl_sharedmem.h"

#define DRAWQUEUE_NEXT(_i, _s) (((_i) + 1) % (_s))

/* Time in ms after which the dispatcher tries to schedule an unscheduled RB */
unsigned int adreno_dispatch_starvation_time = 2000;

/* Amount of time in ms that a starved RB is permitted to execute for */
unsigned int adreno_dispatch_time_slice = 25;

/*
 * If set then dispatcher tries to schedule lower priority RB's after if they
 * have commands in their pipe and have been inactive for
 * _dispatch_starvation_time. Also, once an RB is schduled it will be allowed
 * to run for _dispatch_time_slice unless it's commands complete before
 * _dispatch_time_slice
 */
unsigned int adreno_disp_preempt_fair_sched;

/* Number of commands that can be queued in a context before it sleeps */
static unsigned int _context_drawqueue_size = 50;

/* Number of milliseconds to wait for the context queue to clear */
static unsigned int _context_queue_wait = 10000;

/* Number of drawobjs sent at a time from a single context */
static unsigned int _context_drawobj_burst = 5;

/*
 * GFT throttle parameters. If GFT recovered more than
 * X times in Y ms invalidate the context and do not attempt recovery.
 * X -> _fault_throttle_burst
 * Y -> _fault_throttle_time
 */
static unsigned int _fault_throttle_time = 3000;
static unsigned int _fault_throttle_burst = 3;

/*
 * Maximum ringbuffer inflight for the single submitting context case - this
 * should be sufficiently high to keep the GPU loaded
 */
static unsigned int _dispatcher_q_inflight_hi = 15;

/*
 * Minimum inflight for the multiple context case - this should sufficiently low
 * to allow for lower latency context switching
 */
static unsigned int _dispatcher_q_inflight_lo = 4;

/* Command batch timeout (in milliseconds) */
unsigned int adreno_drawobj_timeout = 2000;

/* Interval for reading and comparing fault detection registers */
static unsigned int _fault_timer_interval = 200;

#define DRAWQUEUE_RB(_drawqueue) \
	((struct adreno_ringbuffer *) \
		container_of((_drawqueue),\
		struct adreno_ringbuffer, dispatch_q))

#define DRAWQUEUE(_ringbuffer) (&(_ringbuffer)->dispatch_q)

static int adreno_dispatch_retire_drawqueue(struct adreno_device *adreno_dev,
		struct adreno_dispatcher_drawqueue *drawqueue);

static inline bool drawqueue_is_current(
		struct adreno_dispatcher_drawqueue *drawqueue)
{
	struct adreno_ringbuffer *rb = DRAWQUEUE_RB(drawqueue);
	struct adreno_device *adreno_dev = ADRENO_RB_DEVICE(rb);

	return (adreno_dev->cur_rb == rb);
}

static void _add_context(struct adreno_device *adreno_dev,
		struct adreno_context *drawctxt)
{
	/* Remove it from the list */
	list_del_init(&drawctxt->active_node);

	/* And push it to the front */
	drawctxt->active_time = jiffies;
	list_add(&drawctxt->active_node, &adreno_dev->active_list);
}

static int __count_context(struct adreno_context *drawctxt, void *data)
{
	unsigned long expires = drawctxt->active_time + msecs_to_jiffies(100);

	return time_after(jiffies, expires) ? 0 : 1;
}

static int __count_drawqueue_context(struct adreno_context *drawctxt,
				void *data)
{
	unsigned long expires = drawctxt->active_time + msecs_to_jiffies(100);

	if (time_after(jiffies, expires))
		return 0;

	return (&drawctxt->rb->dispatch_q ==
			(struct adreno_dispatcher_drawqueue *) data) ? 1 : 0;
}

static int _adreno_count_active_contexts(struct adreno_device *adreno_dev,
		int (*func)(struct adreno_context *, void *), void *data)
{
	struct adreno_context *ctxt;
	int count = 0;

	list_for_each_entry(ctxt, &adreno_dev->active_list, active_node) {
		if (func(ctxt, data) == 0)
			return count;

		count++;
	}

	return count;
}

static void _track_context(struct adreno_device *adreno_dev,
		struct adreno_dispatcher_drawqueue *drawqueue,
		struct adreno_context *drawctxt)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	spin_lock(&adreno_dev->active_list_lock);

	_add_context(adreno_dev, drawctxt);

	device->active_context_count =
			_adreno_count_active_contexts(adreno_dev,
					__count_context, NULL);
	drawqueue->active_context_count =
			_adreno_count_active_contexts(adreno_dev,
					__count_drawqueue_context, drawqueue);

	spin_unlock(&adreno_dev->active_list_lock);
}

/*
 *  If only one context has queued in the last 100 milliseconds increase
 *  inflight to a high number to load up the GPU. If multiple contexts
 *  have queued drop the inflight for better context switch latency.
 *  If no contexts have queued what are you even doing here?
 */

static inline int
_drawqueue_inflight(struct adreno_dispatcher_drawqueue *drawqueue)
{
	return (drawqueue->active_context_count > 1)
		? _dispatcher_q_inflight_lo : _dispatcher_q_inflight_hi;
}

static void fault_detect_read(struct adreno_device *adreno_dev)
{
	int i;

	if (!test_bit(ADRENO_DEVICE_SOFT_FAULT_DETECT, &adreno_dev->priv))
		return;

	for (i = 0; i < adreno_dev->num_ringbuffers; i++) {
		struct adreno_ringbuffer *rb = &(adreno_dev->ringbuffers[i]);

		adreno_rb_readtimestamp(adreno_dev, rb,
			KGSL_TIMESTAMP_RETIRED, &(rb->fault_detect_ts));
	}

	for (i = 0; i < adreno_ft_regs_num; i++) {
		if (adreno_ft_regs[i] != 0)
			kgsl_regread(KGSL_DEVICE(adreno_dev), adreno_ft_regs[i],
				&adreno_ft_regs_val[i]);
	}
}

/*
 * Check to see if the device is idle
 */
static inline bool _isidle(struct adreno_device *adreno_dev)
{
	const struct adreno_gpu_core *gpucore = adreno_dev->gpucore;
	unsigned int reg_rbbm_status;

	if (!kgsl_state_is_awake(KGSL_DEVICE(adreno_dev)))
		goto ret;

	if (adreno_rb_empty(adreno_dev->cur_rb))
		goto ret;

	/* only check rbbm status to determine if GPU is idle */
	adreno_readreg(adreno_dev, ADRENO_REG_RBBM_STATUS, &reg_rbbm_status);

	if (reg_rbbm_status & gpucore->busy_mask)
		return false;

ret:
	/* Clear the existing register values */
	memset(adreno_ft_regs_val, 0,
		adreno_ft_regs_num * sizeof(unsigned int));

	return true;
}

/**
 * fault_detect_read_compare() - Read the fault detect registers and compare
 * them to the current value
 * @device: Pointer to the KGSL device struct
 *
 * Read the set of fault detect registers and compare them to the current set
 * of registers.  Return 1 if any of the register values changed. Also, compare
 * if the current RB's timstamp has changed or not.
 */
static int fault_detect_read_compare(struct adreno_device *adreno_dev)
{
	struct adreno_ringbuffer *rb = ADRENO_CURRENT_RINGBUFFER(adreno_dev);
	int i, ret = 0;
	unsigned int ts;

	if (!test_bit(ADRENO_DEVICE_SOFT_FAULT_DETECT, &adreno_dev->priv))
		return 1;

	/* Check to see if the device is idle - if so report no hang */
	if (_isidle(adreno_dev) == true)
		ret = 1;

	for (i = 0; i < adreno_ft_regs_num; i++) {
		unsigned int val;

		if (adreno_ft_regs[i] == 0)
			continue;
		kgsl_regread(KGSL_DEVICE(adreno_dev), adreno_ft_regs[i], &val);
		if (val != adreno_ft_regs_val[i])
			ret = 1;
		adreno_ft_regs_val[i] = val;
	}

	if (!adreno_rb_readtimestamp(adreno_dev, adreno_dev->cur_rb,
				KGSL_TIMESTAMP_RETIRED, &ts)) {
		if (ts != rb->fault_detect_ts)
			ret = 1;

		rb->fault_detect_ts = ts;
	}

	return ret;
}

static void start_fault_timer(struct adreno_device *adreno_dev)
{
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;

	if (adreno_soft_fault_detect(adreno_dev))
		mod_timer(&dispatcher->fault_timer,
			jiffies + msecs_to_jiffies(_fault_timer_interval));
}

/**
 * _retire_timestamp() - Retire object without sending it
 * to the hardware
 * @drawobj: Pointer to the object to retire
 *
 * In some cases ibs can be retired by the software
 * without going to the GPU.  In those cases, update the
 * memstore from the CPU, kick off the event engine to handle
 * expired events and destroy the ib.
 */
static void _retire_timestamp(struct kgsl_drawobj *drawobj)
{
	struct kgsl_context *context = drawobj->context;
	struct adreno_context *drawctxt = ADRENO_CONTEXT(context);
	struct kgsl_device *device = context->device;

	/*
	 * Write the start and end timestamp to the memstore to keep the
	 * accounting sane
	 */
	kgsl_sharedmem_writel(device, &device->memstore,
		KGSL_MEMSTORE_OFFSET(context->id, soptimestamp),
		drawobj->timestamp);

	kgsl_sharedmem_writel(device, &device->memstore,
		KGSL_MEMSTORE_OFFSET(context->id, eoptimestamp),
		drawobj->timestamp);


	/* Retire pending GPU events for the object */
	kgsl_process_event_group(device, &context->events);

	/*
	 * For A3xx we still get the rptr from the CP_RB_RPTR instead of
	 * rptr scratch out address. At this point GPU clocks turned off.
	 * So avoid reading GPU register directly for A3xx.
	 */
	if (adreno_is_a3xx(ADRENO_DEVICE(device)))
		trace_adreno_cmdbatch_retired(drawobj, -1, 0, 0, drawctxt->rb,
				0, 0);
	else
		trace_adreno_cmdbatch_retired(drawobj, -1, 0, 0, drawctxt->rb,
			adreno_get_rptr(drawctxt->rb), 0);
	kgsl_drawobj_destroy(drawobj);
}

static int _check_context_queue(struct adreno_context *drawctxt)
{
	int ret;

	spin_lock(&drawctxt->lock);

	/*
	 * Wake up if there is room in the context or if the whole thing got
	 * invalidated while we were asleep
	 */

	if (kgsl_context_invalid(&drawctxt->base))
		ret = 1;
	else
		ret = drawctxt->queued < _context_drawqueue_size ? 1 : 0;

	spin_unlock(&drawctxt->lock);

	return ret;
}

/*
 * return true if this is a marker command and the dependent timestamp has
 * retired
 */
static bool _marker_expired(struct kgsl_drawobj_cmd *markerobj)
{
	struct kgsl_drawobj *drawobj = DRAWOBJ(markerobj);

	return (drawobj->flags & KGSL_DRAWOBJ_MARKER) &&
		kgsl_check_timestamp(drawobj->device, drawobj->context,
			markerobj->marker_timestamp);
}

static inline void _pop_drawobj(struct adreno_context *drawctxt)
{
	drawctxt->drawqueue_head = DRAWQUEUE_NEXT(drawctxt->drawqueue_head,
		ADRENO_CONTEXT_DRAWQUEUE_SIZE);
	drawctxt->queued--;
}

static void _retire_sparseobj(struct kgsl_drawobj_sparse *sparseobj,
				struct adreno_context *drawctxt)
{
	kgsl_sparse_bind(drawctxt->base.proc_priv, sparseobj);
	_retire_timestamp(DRAWOBJ(sparseobj));
}

static int _retire_markerobj(struct kgsl_drawobj_cmd *cmdobj,
				struct adreno_context *drawctxt)
{
	if (_marker_expired(cmdobj)) {
		_pop_drawobj(drawctxt);
		_retire_timestamp(DRAWOBJ(cmdobj));
		return 0;
	}

	/*
	 * If the marker isn't expired but the SKIP bit
	 * is set then there are real commands following
	 * this one in the queue. This means that we
	 * need to dispatch the command so that we can
	 * keep the timestamp accounting correct. If
	 * skip isn't set then we block this queue
	 * until the dependent timestamp expires
	 */
	return test_bit(CMDOBJ_SKIP, &cmdobj->priv) ? 1 : -EAGAIN;
}

static int _retire_syncobj(struct kgsl_drawobj_sync *syncobj,
				struct adreno_context *drawctxt)
{
	if (!kgsl_drawobj_events_pending(syncobj)) {
		_pop_drawobj(drawctxt);
		kgsl_drawobj_destroy(DRAWOBJ(syncobj));
		return 0;
	}

	/*
	 * If we got here, there are pending events for sync object.
	 * Start the canary timer if it hasnt been started already.
	 */
	if (!syncobj->timeout_jiffies) {
		syncobj->timeout_jiffies = jiffies + msecs_to_jiffies(5000);
			mod_timer(&syncobj->timer, syncobj->timeout_jiffies);
	}

	return -EAGAIN;
}

/*
 * Retires all expired marker and sync objs from the context
 * queue and returns one of the below
 * a) next drawobj that needs to be sent to ringbuffer
 * b) -EAGAIN for syncobj with syncpoints pending.
 * c) -EAGAIN for markerobj whose marker timestamp has not expired yet.
 * c) NULL for no commands remaining in drawqueue.
 */
static struct kgsl_drawobj *_process_drawqueue_get_next_drawobj(
				struct adreno_context *drawctxt)
{
	struct kgsl_drawobj *drawobj;
	unsigned int i = drawctxt->drawqueue_head;
	int ret = 0;

	if (drawctxt->drawqueue_head == drawctxt->drawqueue_tail)
		return NULL;

	for (i = drawctxt->drawqueue_head; i != drawctxt->drawqueue_tail;
			i = DRAWQUEUE_NEXT(i, ADRENO_CONTEXT_DRAWQUEUE_SIZE)) {

		drawobj = drawctxt->drawqueue[i];

		if (drawobj == NULL)
			return NULL;

		if (drawobj->type == CMDOBJ_TYPE)
			return drawobj;
		else if (drawobj->type == MARKEROBJ_TYPE) {
			ret = _retire_markerobj(CMDOBJ(drawobj), drawctxt);
			/* Special case where marker needs to be sent to GPU */
			if (ret == 1)
				return drawobj;
		} else if (drawobj->type == SYNCOBJ_TYPE)
			ret = _retire_syncobj(SYNCOBJ(drawobj), drawctxt);
		else
			return ERR_PTR(-EINVAL);

		if (ret == -EAGAIN)
			return ERR_PTR(-EAGAIN);

		continue;
	}

	return NULL;
}

/**
 * adreno_dispatcher_requeue_cmdobj() - Put a command back on the context
 * queue
 * @drawctxt: Pointer to the adreno draw context
 * @cmdobj: Pointer to the KGSL command object to requeue
 *
 * Failure to submit a command to the ringbuffer isn't the fault of the command
 * being submitted so if a failure happens, push it back on the head of the the
 * context queue to be reconsidered again unless the context got detached.
 */
static inline int adreno_dispatcher_requeue_cmdobj(
		struct adreno_context *drawctxt,
		struct kgsl_drawobj_cmd *cmdobj)
{
	unsigned int prev;
	struct kgsl_drawobj *drawobj = DRAWOBJ(cmdobj);

	spin_lock(&drawctxt->lock);

	if (kgsl_context_detached(&drawctxt->base) ||
		kgsl_context_invalid(&drawctxt->base)) {
		spin_unlock(&drawctxt->lock);
		/* get rid of this drawobj since the context is bad */
		kgsl_drawobj_destroy(drawobj);
		return -ENOENT;
	}

	prev = drawctxt->drawqueue_head == 0 ?
		(ADRENO_CONTEXT_DRAWQUEUE_SIZE - 1) :
		(drawctxt->drawqueue_head - 1);

	/*
	 * The maximum queue size always needs to be one less then the size of
	 * the ringbuffer queue so there is "room" to put the drawobj back in
	 */

	WARN_ON(prev == drawctxt->drawqueue_tail);

	drawctxt->drawqueue[prev] = drawobj;
	drawctxt->queued++;

	/* Reset the command queue head to reflect the newly requeued change */
	drawctxt->drawqueue_head = prev;
	spin_unlock(&drawctxt->lock);
	return 0;
}

/**
 * dispatcher_queue_context() - Queue a context in the dispatcher pending list
 * @dispatcher: Pointer to the adreno dispatcher struct
 * @drawctxt: Pointer to the adreno draw context
 *
 * Add a context to the dispatcher pending list.
 */
static void  dispatcher_queue_context(struct adreno_device *adreno_dev,
		struct adreno_context *drawctxt)
{
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;

	/* Refuse to queue a detached context */
	if (kgsl_context_detached(&drawctxt->base))
		return;

	spin_lock(&dispatcher->plist_lock);

	if (plist_node_empty(&drawctxt->pending)) {
		/* Get a reference to the context while it sits on the list */
		if (_kgsl_context_get(&drawctxt->base)) {
			trace_dispatch_queue_context(drawctxt);
			plist_add(&drawctxt->pending, &dispatcher->pending);
		}
	}

	spin_unlock(&dispatcher->plist_lock);
}

/**
 * sendcmd() - Send a drawobj to the GPU hardware
 * @dispatcher: Pointer to the adreno dispatcher struct
 * @drawobj: Pointer to the KGSL drawobj being sent
 *
 * Send a KGSL drawobj to the GPU hardware
 */
static int sendcmd(struct adreno_device *adreno_dev,
	struct kgsl_drawobj_cmd *cmdobj)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_drawobj *drawobj = DRAWOBJ(cmdobj);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;
	struct adreno_context *drawctxt = ADRENO_CONTEXT(drawobj->context);
	struct adreno_dispatcher_drawqueue *dispatch_q =
				ADRENO_DRAWOBJ_DISPATCH_DRAWQUEUE(drawobj);
	struct adreno_submit_time time;
	uint64_t secs = 0;
	unsigned long nsecs = 0;
	int ret;

	mutex_lock(&device->mutex);
	if (adreno_gpu_halt(adreno_dev) != 0) {
		mutex_unlock(&device->mutex);
		return -EBUSY;
	}

	dispatcher->inflight++;
	dispatch_q->inflight++;

	if (dispatcher->inflight == 1 &&
			!test_bit(ADRENO_DISPATCHER_POWER, &dispatcher->priv)) {
		/* Time to make the donuts.  Turn on the GPU */
		ret = kgsl_active_count_get(device);
		if (ret) {
			dispatcher->inflight--;
			dispatch_q->inflight--;
			mutex_unlock(&device->mutex);
			return ret;
		}

		set_bit(ADRENO_DISPATCHER_POWER, &dispatcher->priv);
	}

	if (test_bit(ADRENO_DEVICE_DRAWOBJ_PROFILE, &adreno_dev->priv)) {
		set_bit(CMDOBJ_PROFILE, &cmdobj->priv);
		cmdobj->profile_index = adreno_dev->profile_index;
		adreno_dev->profile_index =
			(adreno_dev->profile_index + 1) %
			ADRENO_DRAWOBJ_PROFILE_COUNT;
	}

	ret = adreno_ringbuffer_submitcmd(adreno_dev, cmdobj, &time);

	/*
	 * On the first command, if the submission was successful, then read the
	 * fault registers.  If it failed then turn off the GPU. Sad face.
	 */

	if (dispatcher->inflight == 1) {
		if (ret == 0) {

			/* Stop fault timer before reading fault registers */
			del_timer_sync(&dispatcher->fault_timer);

			fault_detect_read(adreno_dev);

			/* Start the fault timer on first submission */
			start_fault_timer(adreno_dev);

			if (!test_and_set_bit(ADRENO_DISPATCHER_ACTIVE,
				&dispatcher->priv))
				reinit_completion(&dispatcher->idle_gate);

			/*
			 * We update power stats generally at the expire of
			 * cmdbatch. In cases where the cmdbatch takes a long
			 * time to finish, it will delay power stats update,
			 * in effect it will delay DCVS decision. Start a
			 * timer to update power state on expire of this timer.
			 */
			kgsl_pwrscale_midframe_timer_restart(device);

		} else {
			kgsl_active_count_put(device);
			clear_bit(ADRENO_DISPATCHER_POWER, &dispatcher->priv);
		}
	}


	if (ret) {
		dispatcher->inflight--;
		dispatch_q->inflight--;

		mutex_unlock(&device->mutex);

		/*
		 * Don't log a message in case of:
		 * -ENOENT means that the context was detached before the
		 * command was submitted
		 * -ENOSPC means that there temporarily isn't any room in the
		 *  ringbuffer
		 *  -PROTO means that a fault is currently being worked
		 */

		if (ret != -ENOENT && ret != -ENOSPC && ret != -EPROTO)
			KGSL_DRV_ERR(device,
				"Unable to submit command to the ringbuffer %d\n",
				ret);
		return ret;
	}

	secs = time.ktime;
	nsecs = do_div(secs, 1000000000);

	trace_adreno_cmdbatch_submitted(drawobj, (int) dispatcher->inflight,
		time.ticks, (unsigned long) secs, nsecs / 1000, drawctxt->rb,
		adreno_get_rptr(drawctxt->rb));

	mutex_unlock(&device->mutex);

	cmdobj->submit_ticks = time.ticks;

	dispatch_q->cmd_q[dispatch_q->tail] = cmdobj;
	dispatch_q->tail = (dispatch_q->tail + 1) %
		ADRENO_DISPATCH_DRAWQUEUE_SIZE;

	/*
	 * For the first submission in any given command queue update the
	 * expected expire time - this won't actually be used / updated until
	 * the command queue in question goes current, but universally setting
	 * it here avoids the possibilty of some race conditions with preempt
	 */

	if (dispatch_q->inflight == 1)
		dispatch_q->expires = jiffies +
			msecs_to_jiffies(adreno_drawobj_timeout);

	/*
	 * If we believe ourselves to be current and preemption isn't a thing,
	 * then set up the timer.  If this misses, then preemption is indeed a
	 * thing and the timer will be set up in due time
	 */
	if (adreno_in_preempt_state(adreno_dev, ADRENO_PREEMPT_NONE)) {
		if (drawqueue_is_current(dispatch_q))
			mod_timer(&dispatcher->timer, dispatch_q->expires);
	}


	/*
	 * we just submitted something, readjust ringbuffer
	 * execution level
	 */
	if (gpudev->preemption_schedule)
		gpudev->preemption_schedule(adreno_dev);
	return 0;
}


/*
 * Retires all sync objs from the sparse context
 * queue and returns one of the below
 * a) next sparseobj
 * b) -EAGAIN for syncobj with syncpoints pending
 * c) -EINVAL for unexpected drawobj
 * d) NULL for no sparseobj
 */
static struct kgsl_drawobj_sparse *_get_next_sparseobj(
				struct adreno_context *drawctxt)
{
	struct kgsl_drawobj *drawobj;
	unsigned int i = drawctxt->drawqueue_head;
	int ret = 0;

	if (drawctxt->drawqueue_head == drawctxt->drawqueue_tail)
		return NULL;

	for (i = drawctxt->drawqueue_head; i != drawctxt->drawqueue_tail;
			i = DRAWQUEUE_NEXT(i, ADRENO_CONTEXT_DRAWQUEUE_SIZE)) {

		drawobj = drawctxt->drawqueue[i];

		if (drawobj == NULL)
			return NULL;

		if (drawobj->type == SYNCOBJ_TYPE)
			ret = _retire_syncobj(SYNCOBJ(drawobj), drawctxt);
		else if (drawobj->type == SPARSEOBJ_TYPE)
			return SPARSEOBJ(drawobj);
		else
			return ERR_PTR(-EINVAL);

		if (ret == -EAGAIN)
			return ERR_PTR(-EAGAIN);

		continue;
	}

	return NULL;
}

static int _process_drawqueue_sparse(
		struct adreno_context *drawctxt)
{
	struct kgsl_drawobj_sparse *sparseobj;
	int ret = 0;
	unsigned int i;

	for (i = 0; i < ADRENO_CONTEXT_DRAWQUEUE_SIZE; i++) {

		spin_lock(&drawctxt->lock);
		sparseobj = _get_next_sparseobj(drawctxt);
		if (IS_ERR_OR_NULL(sparseobj)) {
			if (IS_ERR(sparseobj))
				ret = PTR_ERR(sparseobj);
			spin_unlock(&drawctxt->lock);
			return ret;
		}

		_pop_drawobj(drawctxt);
		spin_unlock(&drawctxt->lock);

		_retire_sparseobj(sparseobj, drawctxt);
	}

	return 0;
}

/**
 * dispatcher_context_sendcmds() - Send commands from a context to the GPU
 * @adreno_dev: Pointer to the adreno device struct
 * @drawctxt: Pointer to the adreno context to dispatch commands from
 *
 * Dequeue and send a burst of commands from the specified context to the GPU
 * Returns postive if the context needs to be put back on the pending queue
 * 0 if the context is empty or detached and negative on error
 */
static int dispatcher_context_sendcmds(struct adreno_device *adreno_dev,
		struct adreno_context *drawctxt)
{
	struct adreno_dispatcher_drawqueue *dispatch_q =
					&(drawctxt->rb->dispatch_q);
	int count = 0;
	int ret = 0;
	int inflight = _drawqueue_inflight(dispatch_q);
	unsigned int timestamp;

	if (drawctxt->base.flags & KGSL_CONTEXT_SPARSE)
		return _process_drawqueue_sparse(drawctxt);

	if (dispatch_q->inflight >= inflight) {
		spin_lock(&drawctxt->lock);
		_process_drawqueue_get_next_drawobj(drawctxt);
		spin_unlock(&drawctxt->lock);
		return -EBUSY;
	}

	/*
	 * Each context can send a specific number of drawobjs per cycle
	 */
	while ((count < _context_drawobj_burst) &&
		(dispatch_q->inflight < inflight)) {
		struct kgsl_drawobj *drawobj;
		struct kgsl_drawobj_cmd *cmdobj;

		if (adreno_gpu_fault(adreno_dev) != 0)
			break;

		spin_lock(&drawctxt->lock);
		drawobj = _process_drawqueue_get_next_drawobj(drawctxt);

		/*
		 * adreno_context_get_drawobj returns -EAGAIN if the current
		 * drawobj has pending sync points so no more to do here.
		 * When the sync points are satisfied then the context will get
		 * reqeueued
		 */

		if (IS_ERR_OR_NULL(drawobj)) {
			if (IS_ERR(drawobj))
				ret = PTR_ERR(drawobj);
			spin_unlock(&drawctxt->lock);
			break;
		}
		_pop_drawobj(drawctxt);
		spin_unlock(&drawctxt->lock);

		timestamp = drawobj->timestamp;
		cmdobj = CMDOBJ(drawobj);
		ret = sendcmd(adreno_dev, cmdobj);

		/*
		 * On error from sendcmd() try to requeue the cmdobj
		 * unless we got back -ENOENT which means that the context has
		 * been detached and there will be no more deliveries from here
		 */
		if (ret != 0) {
			/* Destroy the cmdobj on -ENOENT */
			if (ret == -ENOENT)
				kgsl_drawobj_destroy(drawobj);
			else {
				/*
				 * If the requeue returns an error, return that
				 * instead of whatever sendcmd() sent us
				 */
				int r = adreno_dispatcher_requeue_cmdobj(
					drawctxt, cmdobj);
				if (r)
					ret = r;
			}

			break;
		}

		drawctxt->submitted_timestamp = timestamp;

		count++;
	}

	/*
	 * Wake up any snoozing threads if we have consumed any real commands
	 * or marker commands and we have room in the context queue.
	 */

	if (_check_context_queue(drawctxt))
		wake_up_all(&drawctxt->wq);

	if (!ret)
		ret = count;

	/* Return error or the number of commands queued */
	return ret;
}

/**
 * _adreno_dispatcher_issuecmds() - Issue commmands from pending contexts
 * @adreno_dev: Pointer to the adreno device struct
 *
 * Issue as many commands as possible (up to inflight) from the pending contexts
 * This function assumes the dispatcher mutex has been locked.
 */
static void _adreno_dispatcher_issuecmds(struct adreno_device *adreno_dev)
{
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;
	struct adreno_context *drawctxt, *next;
	struct plist_head requeue, busy_list;
	int ret;

	/* Leave early if the dispatcher isn't in a happy state */
	if (adreno_gpu_fault(adreno_dev) != 0)
		return;

	plist_head_init(&requeue);
	plist_head_init(&busy_list);

	/* Try to fill the ringbuffers as much as possible */
	while (1) {

		/* Stop doing things if the dispatcher is paused or faulted */
		if (adreno_gpu_fault(adreno_dev) != 0)
			break;

		if (adreno_gpu_halt(adreno_dev) != 0)
			break;

		spin_lock(&dispatcher->plist_lock);

		if (plist_head_empty(&dispatcher->pending)) {
			spin_unlock(&dispatcher->plist_lock);
			break;
		}

		/* Get the next entry on the list */
		drawctxt = plist_first_entry(&dispatcher->pending,
			struct adreno_context, pending);

		plist_del(&drawctxt->pending, &dispatcher->pending);

		spin_unlock(&dispatcher->plist_lock);

		if (kgsl_context_detached(&drawctxt->base) ||
			kgsl_context_invalid(&drawctxt->base)) {
			kgsl_context_put(&drawctxt->base);
			continue;
		}

		ret = dispatcher_context_sendcmds(adreno_dev, drawctxt);

		/* Don't bother requeuing on -ENOENT - context is detached */
		if (ret != 0 && ret != -ENOENT) {
			spin_lock(&dispatcher->plist_lock);

			/*
			 * Check to seen if the context had been requeued while
			 * we were processing it (probably by another thread
			 * pushing commands). If it has then shift it to the
			 * requeue list if it was not able to submit commands
			 * due to the dispatch_q being full. Also, do a put to
			 * make sure the reference counting stays accurate.
			 * If the node is empty then we will put it on the
			 * requeue list and not touch the refcount since we
			 * already hold it from the first time it went on the
			 * list.
			 */

			if (!plist_node_empty(&drawctxt->pending)) {
				plist_del(&drawctxt->pending,
						&dispatcher->pending);
				kgsl_context_put(&drawctxt->base);
			}

			if (ret == -EBUSY)
				/* Inflight queue is full */
				plist_add(&drawctxt->pending, &busy_list);
			else
				plist_add(&drawctxt->pending, &requeue);

			spin_unlock(&dispatcher->plist_lock);
		} else {
			/*
			 * If the context doesn't need be requeued put back the
			 * refcount
			 */

			kgsl_context_put(&drawctxt->base);
		}
	}

	spin_lock(&dispatcher->plist_lock);

	/* Put the contexts that couldn't submit back on the pending list */
	plist_for_each_entry_safe(drawctxt, next, &busy_list, pending) {
		plist_del(&drawctxt->pending, &busy_list);
		plist_add(&drawctxt->pending, &dispatcher->pending);
	}

	/* Now put the contexts that need to be requeued back on the list */
	plist_for_each_entry_safe(drawctxt, next, &requeue, pending) {
		plist_del(&drawctxt->pending, &requeue);
		plist_add(&drawctxt->pending, &dispatcher->pending);
	}

	spin_unlock(&dispatcher->plist_lock);
}

static inline void _decrement_submit_now(struct kgsl_device *device)
{
	spin_lock(&device->submit_lock);
	device->submit_now--;
	spin_unlock(&device->submit_lock);
}

/**
 * adreno_dispatcher_issuecmds() - Issue commmands from pending contexts
 * @adreno_dev: Pointer to the adreno device struct
 *
 * Lock the dispatcher and call _adreno_dispatcher_issueibcmds
 */
static void adreno_dispatcher_issuecmds(struct adreno_device *adreno_dev)
{
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	spin_lock(&device->submit_lock);
	/* If state transition to SLUMBER, schedule the work for later */
	if (device->slumber == true) {
		spin_unlock(&device->submit_lock);
		goto done;
	}
	device->submit_now++;
	spin_unlock(&device->submit_lock);

	/* If the dispatcher is busy then schedule the work for later */
	if (!mutex_trylock(&dispatcher->mutex)) {
		_decrement_submit_now(device);
		goto done;
	}

	_adreno_dispatcher_issuecmds(adreno_dev);
	mutex_unlock(&dispatcher->mutex);
	_decrement_submit_now(device);
	return;
done:
	adreno_dispatcher_schedule(device);
}

/**
 * get_timestamp() - Return the next timestamp for the context
 * @drawctxt - Pointer to an adreno draw context struct
 * @drawobj - Pointer to a drawobj
 * @timestamp - Pointer to a timestamp value possibly passed from the user
 * @user_ts - user generated timestamp
 *
 * Assign a timestamp based on the settings of the draw context and the command
 * batch.
 */
static int get_timestamp(struct adreno_context *drawctxt,
		struct kgsl_drawobj *drawobj, unsigned int *timestamp,
		unsigned int user_ts)
{

	if (drawctxt->base.flags & KGSL_CONTEXT_USER_GENERATED_TS) {
		/*
		 * User specified timestamps need to be greater than the last
		 * issued timestamp in the context
		 */
		if (timestamp_cmp(drawctxt->timestamp, user_ts) >= 0)
			return -ERANGE;

		drawctxt->timestamp = user_ts;
	} else
		drawctxt->timestamp++;

	*timestamp = drawctxt->timestamp;
	drawobj->timestamp = *timestamp;
	return 0;
}

static void _set_ft_policy(struct adreno_device *adreno_dev,
		struct adreno_context *drawctxt,
		struct kgsl_drawobj_cmd *cmdobj)
{
	/*
	 * Set the fault tolerance policy for the command batch - assuming the
	 * context hasn't disabled FT use the current device policy
	 */
	if (drawctxt->base.flags & KGSL_CONTEXT_NO_FAULT_TOLERANCE)
		set_bit(KGSL_FT_DISABLE, &cmdobj->fault_policy);
	/*
	 *  Set the fault tolerance policy to FT_REPLAY - As context wants
	 *  to invalidate it after a replay attempt fails. This doesn't
	 *  require to execute the default FT policy.
	 */
	else if (drawctxt->base.flags & KGSL_CONTEXT_INVALIDATE_ON_FAULT)
		set_bit(KGSL_FT_REPLAY, &cmdobj->fault_policy);
	else
		cmdobj->fault_policy = adreno_dev->ft_policy;
}

static void _cmdobj_set_flags(struct adreno_context *drawctxt,
			struct kgsl_drawobj_cmd *cmdobj)
{
	/*
	 * Force the preamble for this submission only - this is usually
	 * requested by the dispatcher as part of fault recovery
	 */
	if (test_and_clear_bit(ADRENO_CONTEXT_FORCE_PREAMBLE,
				&drawctxt->base.priv))
		set_bit(CMDOBJ_FORCE_PREAMBLE, &cmdobj->priv);

	/*
	 * Force the premable if set from userspace in the context or
	 * command obj flags
	 */
	if ((drawctxt->base.flags & KGSL_CONTEXT_CTX_SWITCH) ||
		(cmdobj->base.flags & KGSL_DRAWOBJ_CTX_SWITCH))
		set_bit(CMDOBJ_FORCE_PREAMBLE, &cmdobj->priv);

	/* Skip this ib if IFH_NOP is enabled */
	if (drawctxt->base.flags & KGSL_CONTEXT_IFH_NOP)
		set_bit(CMDOBJ_SKIP, &cmdobj->priv);

	/*
	 * If we are waiting for the end of frame and it hasn't appeared yet,
	 * then mark the command obj as skipped.  It will still progress
	 * through the pipeline but it won't actually send any commands
	 */

	if (test_bit(ADRENO_CONTEXT_SKIP_EOF, &drawctxt->base.priv)) {
		set_bit(CMDOBJ_SKIP, &cmdobj->priv);

		/*
		 * If this command obj represents the EOF then clear the way
		 * for the dispatcher to continue submitting
		 */

		if (cmdobj->base.flags & KGSL_DRAWOBJ_END_OF_FRAME) {
			clear_bit(ADRENO_CONTEXT_SKIP_EOF,
				  &drawctxt->base.priv);

			/*
			 * Force the preamble on the next command to ensure that
			 * the state is correct
			 */
			set_bit(ADRENO_CONTEXT_FORCE_PREAMBLE,
				&drawctxt->base.priv);
		}
	}
}

static inline int _check_context_state(struct kgsl_context *context)
{
	if (kgsl_context_invalid(context))
		return -EDEADLK;

	if (kgsl_context_detached(context))
		return -ENOENT;

	return 0;
}

static inline bool _verify_ib(struct kgsl_device_private *dev_priv,
		struct kgsl_context *context, struct kgsl_memobj_node *ib)
{
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_process_private *private = dev_priv->process_priv;

	/* The maximum allowable size for an IB in the CP is 0xFFFFF dwords */
	if (ib->size == 0 || ((ib->size >> 2) > 0xFFFFF)) {
		pr_context(device, context, "ctxt %d invalid ib size %lld\n",
			context->id, ib->size);
		return false;
	}

	/* Make sure that the address is mapped */
	if (!kgsl_mmu_gpuaddr_in_range(private->pagetable, ib->gpuaddr)) {
		pr_context(device, context, "ctxt %d invalid ib gpuaddr %llX\n",
			context->id, ib->gpuaddr);
		return false;
	}

	return true;
}

static inline int _verify_cmdobj(struct kgsl_device_private *dev_priv,
		struct kgsl_context *context, struct kgsl_drawobj *drawobj[],
		uint32_t count)
{
	struct kgsl_device *device = dev_priv->device;
	struct kgsl_memobj_node *ib;
	unsigned int i;

	for (i = 0; i < count; i++) {
		/* Verify the IBs before they get queued */
		if (drawobj[i]->type == CMDOBJ_TYPE) {
			struct kgsl_drawobj_cmd *cmdobj = CMDOBJ(drawobj[i]);

			list_for_each_entry(ib, &cmdobj->cmdlist, node)
				if (_verify_ib(dev_priv,
					&ADRENO_CONTEXT(context)->base, ib)
					== false)
					return -EINVAL;
			/*
			 * Clear the wake on touch bit to indicate an IB has
			 * been submitted since the last time we set it.
			 * But only clear it when we have rendering commands.
			 */
			device->flags &= ~KGSL_FLAG_WAKE_ON_TOUCH;
		}

		/* A3XX does not have support for drawobj profiling */
		if (adreno_is_a3xx(ADRENO_DEVICE(device)) &&
			(drawobj[i]->flags & KGSL_DRAWOBJ_PROFILING))
			return -EOPNOTSUPP;
	}

	return 0;
}

static inline int _wait_for_room_in_context_queue(
	struct adreno_context *drawctxt)
{
	int ret = 0;

	/* Wait for room in the context queue */
	while (drawctxt->queued >= _context_drawqueue_size) {
		trace_adreno_drawctxt_sleep(drawctxt);
		spin_unlock(&drawctxt->lock);

		ret = wait_event_interruptible_timeout(drawctxt->wq,
			_check_context_queue(drawctxt),
			msecs_to_jiffies(_context_queue_wait));

		spin_lock(&drawctxt->lock);
		trace_adreno_drawctxt_wake(drawctxt);

		if (ret <= 0)
			return (ret == 0) ? -ETIMEDOUT : (int) ret;
	}

	return 0;
}

static unsigned int _check_context_state_to_queue_cmds(
	struct adreno_context *drawctxt)
{
	int ret = _check_context_state(&drawctxt->base);

	if (ret)
		return ret;

	ret = _wait_for_room_in_context_queue(drawctxt);
	if (ret)
		return ret;

	/*
	 * Account for the possiblity that the context got invalidated
	 * while we were sleeping
	 */
	return _check_context_state(&drawctxt->base);
}

static void _queue_drawobj(struct adreno_context *drawctxt,
	struct kgsl_drawobj *drawobj)
{
	/* Put the command into the queue */
	drawctxt->drawqueue[drawctxt->drawqueue_tail] = drawobj;
	drawctxt->drawqueue_tail = (drawctxt->drawqueue_tail + 1) %
			ADRENO_CONTEXT_DRAWQUEUE_SIZE;
	drawctxt->queued++;
	trace_adreno_cmdbatch_queued(drawobj, drawctxt->queued);
}

static int _queue_sparseobj(struct adreno_device *adreno_dev,
	struct adreno_context *drawctxt, struct kgsl_drawobj_sparse *sparseobj,
	uint32_t *timestamp, unsigned int user_ts)
{
	struct kgsl_drawobj *drawobj = DRAWOBJ(sparseobj);
	int ret;

	ret = get_timestamp(drawctxt, drawobj, timestamp, user_ts);
	if (ret)
		return ret;

	/*
	 * See if we can fastpath this thing - if nothing is
	 * queued bind/unbind without queueing the context
	 */
	if (!drawctxt->queued)
		return 1;

	drawctxt->queued_timestamp = *timestamp;
	_queue_drawobj(drawctxt, drawobj);

	return 0;
}


static int _queue_markerobj(struct adreno_device *adreno_dev,
	struct adreno_context *drawctxt, struct kgsl_drawobj_cmd *markerobj,
	uint32_t *timestamp, unsigned int user_ts)
{
	struct kgsl_drawobj *drawobj = DRAWOBJ(markerobj);
	int ret;

	ret = get_timestamp(drawctxt, drawobj, timestamp, user_ts);
	if (ret)
		return ret;

	/*
	 * See if we can fastpath this thing - if nothing is queued
	 * and nothing is inflight retire without bothering the GPU
	 */
	if (!drawctxt->queued && kgsl_check_timestamp(drawobj->device,
			drawobj->context, drawctxt->queued_timestamp)) {
		_retire_timestamp(drawobj);
		return 1;
	}

	/*
	 * Remember the last queued timestamp - the marker will block
	 * until that timestamp is expired (unless another command
	 * comes along and forces the marker to execute)
	 */

	markerobj->marker_timestamp = drawctxt->queued_timestamp;
	drawctxt->queued_timestamp = *timestamp;
	_set_ft_policy(adreno_dev, drawctxt, markerobj);
	_cmdobj_set_flags(drawctxt, markerobj);

	_queue_drawobj(drawctxt, drawobj);

	return 0;
}

static int _queue_cmdobj(struct adreno_device *adreno_dev,
	struct adreno_context *drawctxt, struct kgsl_drawobj_cmd *cmdobj,
	uint32_t *timestamp, unsigned int user_ts)
{
	struct kgsl_drawobj *drawobj = DRAWOBJ(cmdobj);
	unsigned int j;
	int ret;

	ret = get_timestamp(drawctxt, drawobj, timestamp, user_ts);
	if (ret)
		return ret;

	/*
	 * If this is a real command then we need to force any markers
	 * queued before it to dispatch to keep time linear - set the
	 * skip bit so the commands get NOPed.
	 */
	j = drawctxt->drawqueue_head;

	while (j != drawctxt->drawqueue_tail) {
		if (drawctxt->drawqueue[j]->type == MARKEROBJ_TYPE) {
			struct kgsl_drawobj_cmd *markerobj =
				CMDOBJ(drawctxt->drawqueue[j]);
				set_bit(CMDOBJ_SKIP, &markerobj->priv);
		}

		j = DRAWQUEUE_NEXT(j, ADRENO_CONTEXT_DRAWQUEUE_SIZE);
	}

	drawctxt->queued_timestamp = *timestamp;
	_set_ft_policy(adreno_dev, drawctxt, cmdobj);
	_cmdobj_set_flags(drawctxt, cmdobj);

	_queue_drawobj(drawctxt, drawobj);

	return 0;
}

static void _queue_syncobj(struct adreno_context *drawctxt,
	struct kgsl_drawobj_sync *syncobj, uint32_t *timestamp)
{
	struct kgsl_drawobj *drawobj = DRAWOBJ(syncobj);

	*timestamp = 0;
	drawobj->timestamp = 0;

	_queue_drawobj(drawctxt, drawobj);
}

/**
 * adreno_dispactcher_queue_cmds() - Queue a new draw object in the context
 * @dev_priv: Pointer to the device private struct
 * @context: Pointer to the kgsl draw context
 * @drawobj: Pointer to the array of drawobj's being submitted
 * @count: Number of drawobj's being submitted
 * @timestamp: Pointer to the requested timestamp
 *
 * Queue a command in the context - if there isn't any room in the queue, then
 * block until there is
 */
int adreno_dispatcher_queue_cmds(struct kgsl_device_private *dev_priv,
		struct kgsl_context *context, struct kgsl_drawobj *drawobj[],
		uint32_t count, uint32_t *timestamp)

{
	struct kgsl_device *device = dev_priv->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_context *drawctxt = ADRENO_CONTEXT(context);
	struct adreno_dispatcher_drawqueue *dispatch_q;
	int ret;
	unsigned int i, user_ts;

	if (!count)
		return -EINVAL;

	ret = _check_context_state(&drawctxt->base);
	if (ret)
		return ret;

	ret = _verify_cmdobj(dev_priv, context, drawobj, count);
	if (ret)
		return ret;

	/* wait for the suspend gate */
	wait_for_completion(&device->halt_gate);

	spin_lock(&drawctxt->lock);

	ret = _check_context_state_to_queue_cmds(drawctxt);
	if (ret) {
		spin_unlock(&drawctxt->lock);
		return ret;
	}

	user_ts = *timestamp;

	for (i = 0; i < count; i++) {

		switch (drawobj[i]->type) {
		case MARKEROBJ_TYPE:
			ret = _queue_markerobj(adreno_dev, drawctxt,
					CMDOBJ(drawobj[i]),
					timestamp, user_ts);
			if (ret == 1) {
				spin_unlock(&drawctxt->lock);
				goto done;
			} else if (ret) {
				spin_unlock(&drawctxt->lock);
				return ret;
			}
			break;
		case CMDOBJ_TYPE:
			ret = _queue_cmdobj(adreno_dev, drawctxt,
						CMDOBJ(drawobj[i]),
						timestamp, user_ts);
			if (ret) {
				spin_unlock(&drawctxt->lock);
				return ret;
			}
			break;
		case SYNCOBJ_TYPE:
			_queue_syncobj(drawctxt, SYNCOBJ(drawobj[i]),
						timestamp);
			break;
		case SPARSEOBJ_TYPE:
			ret = _queue_sparseobj(adreno_dev, drawctxt,
					SPARSEOBJ(drawobj[i]),
					timestamp, user_ts);
			if (ret == 1) {
				spin_unlock(&drawctxt->lock);
				_retire_sparseobj(SPARSEOBJ(drawobj[i]),
						drawctxt);
				return 0;
			} else if (ret) {
				spin_unlock(&drawctxt->lock);
				return ret;
			}
			break;
		default:
			spin_unlock(&drawctxt->lock);
			return -EINVAL;
		}

	}

	dispatch_q = ADRENO_DRAWOBJ_DISPATCH_DRAWQUEUE(drawobj[0]);

	_track_context(adreno_dev, dispatch_q, drawctxt);

	spin_unlock(&drawctxt->lock);

	if (device->pwrctrl.l2pc_update_queue)
		kgsl_pwrctrl_update_l2pc(&adreno_dev->dev,
				KGSL_L2PC_QUEUE_TIMEOUT);

	/* Add the context to the dispatcher pending list */
	dispatcher_queue_context(adreno_dev, drawctxt);

	/*
	 * Only issue commands if inflight is less than burst -this prevents us
	 * from sitting around waiting for the mutex on a busy system - the work
	 * loop will schedule it for us. Inflight is mutex protected but the
	 * worse that can happen is that it will go to 0 after we check and if
	 * it goes to 0 it is because the work loop decremented it and the work
	 * queue will try to schedule new commands anyway.
	 */

	if (dispatch_q->inflight < _context_drawobj_burst)
		adreno_dispatcher_issuecmds(adreno_dev);
done:
	if (test_and_clear_bit(ADRENO_CONTEXT_FAULT, &context->priv))
		return -EPROTO;

	return 0;
}

static int _mark_context(int id, void *ptr, void *data)
{
	unsigned int guilty = *((unsigned int *) data);
	struct kgsl_context *context = ptr;

	/*
	 * If the context is guilty mark it as such.  Otherwise mark it as
	 * innocent if it had not already been marked as guilty.  If id is
	 * passed as 0 then mark EVERYBODY guilty (recovery failed)
	 */

	if (guilty == 0 || guilty == context->id)
		context->reset_status =
			KGSL_CTX_STAT_GUILTY_CONTEXT_RESET_EXT;
	else if (context->reset_status !=
		KGSL_CTX_STAT_GUILTY_CONTEXT_RESET_EXT)
		context->reset_status =
			KGSL_CTX_STAT_INNOCENT_CONTEXT_RESET_EXT;

	return 0;
}

/**
 * mark_guilty_context() - Mark the given context as guilty (failed recovery)
 * @device: Pointer to a KGSL device structure
 * @id: Context ID of the guilty context (or 0 to mark all as guilty)
 *
 * Mark the given (or all) context(s) as guilty (failed recovery)
 */
static void mark_guilty_context(struct kgsl_device *device, unsigned int id)
{
	/* Mark the status for all the contexts in the device */

	read_lock(&device->context_lock);
	idr_for_each(&device->context_idr, _mark_context, &id);
	read_unlock(&device->context_lock);
}

/*
 * If an IB inside of the drawobj has a gpuaddr that matches the base
 * passed in then zero the size which effectively skips it when it is submitted
 * in the ringbuffer.
 */
static void _skip_ib(struct kgsl_drawobj_cmd *cmdobj, uint64_t base)
{
	struct kgsl_memobj_node *ib;

	list_for_each_entry(ib, &cmdobj->cmdlist, node) {
		if (ib->gpuaddr == base) {
			ib->priv |= MEMOBJ_SKIP;
			if (base)
				return;
		}
	}
}

static void _skip_cmd(struct kgsl_drawobj_cmd *cmdobj,
	struct kgsl_drawobj_cmd **replay, int count)
{
	struct kgsl_drawobj *drawobj = DRAWOBJ(cmdobj);
	struct adreno_context *drawctxt = ADRENO_CONTEXT(drawobj->context);
	int i;

	/*
	 * SKIPCMD policy: next IB issued for this context is tentative
	 * if it fails we assume that GFT failed and if it succeeds
	 * we mark GFT as a success.
	 *
	 * Find next commandbatch for the faulting context
	 * If commandbatch is found
	 * a) store the current commandbatch fault_policy in context's next
	 *    commandbatch fault_policy
	 * b) force preamble for next commandbatch
	 */
	for (i = 1; i < count; i++) {
		if (DRAWOBJ(replay[i])->context->id == drawobj->context->id) {
			replay[i]->fault_policy = replay[0]->fault_policy;
			set_bit(CMDOBJ_FORCE_PREAMBLE, &replay[i]->priv);
			set_bit(KGSL_FT_SKIPCMD, &replay[i]->fault_recovery);
			break;
		}
	}

	/*
	 * If we did not find the next cmd then
	 * a) set a flag for next command issued in this context
	 * b) store the fault_policy, this fault_policy becomes the policy of
	 *    next command issued in this context
	 */
	if ((i == count) && drawctxt) {
		set_bit(ADRENO_CONTEXT_SKIP_CMD, &drawctxt->base.priv);
		drawctxt->fault_policy = replay[0]->fault_policy;
	}

	/* set the flags to skip this cmdobj */
	set_bit(CMDOBJ_SKIP, &cmdobj->priv);
	cmdobj->fault_recovery = 0;
}

static void _skip_frame(struct kgsl_drawobj_cmd *cmdobj,
	struct kgsl_drawobj_cmd **replay, int count)
{
	struct kgsl_drawobj *drawobj = DRAWOBJ(cmdobj);
	struct adreno_context *drawctxt = ADRENO_CONTEXT(drawobj->context);
	int skip = 1;
	int i;

	for (i = 0; i < count; i++) {

		struct kgsl_drawobj *replay_obj = DRAWOBJ(replay[i]);

		/*
		 * Only operate on drawobj's that belong to the
		 * faulting context
		 */

		if (replay_obj->context->id != drawobj->context->id)
			continue;

		/*
		 * Skip all the drawobjs in this context until
		 * the EOF flag is seen.  If the EOF flag is seen then
		 * force the preamble for the next command.
		 */

		if (skip) {
			set_bit(CMDOBJ_SKIP, &replay[i]->priv);

			if (replay_obj->flags & KGSL_DRAWOBJ_END_OF_FRAME)
				skip = 0;
		} else {
			set_bit(CMDOBJ_FORCE_PREAMBLE, &replay[i]->priv);
			return;
		}
	}

	/*
	 * If the EOF flag hasn't been seen yet then set the flag in the
	 * drawctxt to keep looking for it
	 */

	if (skip && drawctxt)
		set_bit(ADRENO_CONTEXT_SKIP_EOF, &drawctxt->base.priv);

	/*
	 * If we did see the EOF flag then force the preamble on for the
	 * next command issued on this context
	 */

	if (!skip && drawctxt)
		set_bit(ADRENO_CONTEXT_FORCE_PREAMBLE, &drawctxt->base.priv);
}

static void remove_invalidated_cmdobjs(struct kgsl_device *device,
		struct kgsl_drawobj_cmd **replay, int count)
{
	int i;

	for (i = 0; i < count; i++) {
		struct kgsl_drawobj_cmd *cmdobj = replay[i];
		struct kgsl_drawobj *drawobj = DRAWOBJ(cmdobj);

		if (cmdobj == NULL)
			continue;

		if (kgsl_context_detached(drawobj->context) ||
			kgsl_context_invalid(drawobj->context)) {
			replay[i] = NULL;

			mutex_lock(&device->mutex);
			kgsl_cancel_events_timestamp(device,
				&drawobj->context->events, drawobj->timestamp);
			mutex_unlock(&device->mutex);

			kgsl_drawobj_destroy(drawobj);
		}
	}
}

static char _pidname[TASK_COMM_LEN];

static inline const char *_kgsl_context_comm(struct kgsl_context *context)
{
	if (context && context->proc_priv)
		strlcpy(_pidname, context->proc_priv->comm, sizeof(_pidname));
	else
		snprintf(_pidname, TASK_COMM_LEN, "unknown");

	return _pidname;
}

#define pr_fault(_d, _c, fmt, args...) \
		dev_err((_d)->dev, "%s[%d]: " fmt, \
		_kgsl_context_comm((_c)->context), \
		(_c)->context->proc_priv->pid, ##args)


static void adreno_fault_header(struct kgsl_device *device,
		struct adreno_ringbuffer *rb, struct kgsl_drawobj_cmd *cmdobj)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_drawobj *drawobj = DRAWOBJ(cmdobj);
	unsigned int status, rptr, wptr, ib1sz, ib2sz;
	uint64_t ib1base, ib2base;

	adreno_readreg(adreno_dev, ADRENO_REG_RBBM_STATUS, &status);
	adreno_readreg(adreno_dev, ADRENO_REG_CP_RB_RPTR, &rptr);
	adreno_readreg(adreno_dev, ADRENO_REG_CP_RB_WPTR, &wptr);
	adreno_readreg64(adreno_dev, ADRENO_REG_CP_IB1_BASE,
					  ADRENO_REG_CP_IB1_BASE_HI, &ib1base);
	adreno_readreg(adreno_dev, ADRENO_REG_CP_IB1_BUFSZ, &ib1sz);
	adreno_readreg64(adreno_dev, ADRENO_REG_CP_IB2_BASE,
					   ADRENO_REG_CP_IB2_BASE_HI, &ib2base);
	adreno_readreg(adreno_dev, ADRENO_REG_CP_IB2_BUFSZ, &ib2sz);

	if (drawobj != NULL) {
		struct adreno_context *drawctxt =
			ADRENO_CONTEXT(drawobj->context);

		trace_adreno_gpu_fault(drawobj->context->id,
			drawobj->timestamp,
			status, rptr, wptr, ib1base, ib1sz,
			ib2base, ib2sz, drawctxt->rb->id);

		pr_fault(device, drawobj,
			"gpu fault ctx %d ts %d status %8.8X rb %4.4x/%4.4x ib1 %16.16llX/%4.4x ib2 %16.16llX/%4.4x\n",
			drawobj->context->id, drawobj->timestamp, status,
			rptr, wptr, ib1base, ib1sz, ib2base, ib2sz);

		if (rb != NULL)
			pr_fault(device, drawobj,
				"gpu fault rb %d rb sw r/w %4.4x/%4.4x\n",
				rb->id, rptr, rb->wptr);
	} else {
		int id = (rb != NULL) ? rb->id : -1;

		dev_err(device->dev,
			"RB[%d]: gpu fault status %8.8X rb %4.4x/%4.4x ib1 %16.16llX/%4.4x ib2 %16.16llX/%4.4x\n",
			id, status, rptr, wptr, ib1base, ib1sz, ib2base,
			ib2sz);
		if (rb != NULL)
			dev_err(device->dev,
				"RB[%d] gpu fault rb sw r/w %4.4x/%4.4x\n",
				rb->id, rptr, rb->wptr);
	}
}

void adreno_fault_skipcmd_detached(struct adreno_device *adreno_dev,
				 struct adreno_context *drawctxt,
				 struct kgsl_drawobj *drawobj)
{
	if (test_bit(ADRENO_CONTEXT_SKIP_CMD, &drawctxt->base.priv) &&
			kgsl_context_detached(&drawctxt->base)) {
		pr_context(KGSL_DEVICE(adreno_dev), drawobj->context,
			"gpu detached context %d\n", drawobj->context->id);
		clear_bit(ADRENO_CONTEXT_SKIP_CMD, &drawctxt->base.priv);
	}
}

/**
 * process_cmdobj_fault() - Process a cmdobj for fault policies
 * @device: Device on which the cmdobj caused a fault
 * @replay: List of cmdobj's that are to be replayed on the device. The
 * first command in the replay list is the faulting command and the remaining
 * cmdobj's in the list are commands that were submitted to the same queue
 * as the faulting one.
 * @count: Number of cmdobj's in replay
 * @base: The IB1 base at the time of fault
 * @fault: The fault type
 */
static void process_cmdobj_fault(struct kgsl_device *device,
		struct kgsl_drawobj_cmd **replay, int count,
		unsigned int base,
		int fault)
{
	struct kgsl_drawobj_cmd *cmdobj = replay[0];
	struct kgsl_drawobj *drawobj = DRAWOBJ(cmdobj);
	int i;
	char *state = "failed";

	/*
	 * If GFT recovered more than X times in Y ms invalidate the context
	 * and do not attempt recovery.
	 * Example: X==3 and Y==3000 ms, GPU hung at 500ms, 1700ms, 25000ms and
	 * 3000ms for the same context, we will not try FT and invalidate the
	 * context @3000ms because context triggered GFT more than 3 times in
	 * last 3 seconds. If a context caused recoverable GPU hangs
	 * where 1st and 4th gpu hang are more than 3 seconds apart we
	 * won't disable GFT and invalidate the context.
	 */
	if (test_bit(KGSL_FT_THROTTLE, &cmdobj->fault_policy)) {
		if (time_after(jiffies, (drawobj->context->fault_time
				+ msecs_to_jiffies(_fault_throttle_time)))) {
			drawobj->context->fault_time = jiffies;
			drawobj->context->fault_count = 1;
		} else {
			drawobj->context->fault_count++;
			if (drawobj->context->fault_count >
					_fault_throttle_burst) {
				set_bit(KGSL_FT_DISABLE,
						&cmdobj->fault_policy);
				pr_context(device, drawobj->context,
					 "gpu fault threshold exceeded %d faults in %d msecs\n",
					 _fault_throttle_burst,
					 _fault_throttle_time);
			}
		}
	}

	/*
	 * If FT is disabled for this cmdobj invalidate immediately
	 */

	if (test_bit(KGSL_FT_DISABLE, &cmdobj->fault_policy) ||
		test_bit(KGSL_FT_TEMP_DISABLE, &cmdobj->fault_policy)) {
		state = "skipped";
		bitmap_zero(&cmdobj->fault_policy, BITS_PER_LONG);
	}

	/* If the context is detached do not run FT on context */
	if (kgsl_context_detached(drawobj->context)) {
		state = "detached";
		bitmap_zero(&cmdobj->fault_policy, BITS_PER_LONG);
	}

	/*
	 * Set a flag so we don't print another PM dump if the cmdobj fails
	 * again on replay
	 */

	set_bit(KGSL_FT_SKIP_PMDUMP, &cmdobj->fault_policy);

	/*
	 * A hardware fault generally means something was deterministically
	 * wrong with the cmdobj - no point in trying to replay it
	 * Clear the replay bit and move on to the next policy level
	 */

	if (fault & ADRENO_HARD_FAULT)
		clear_bit(KGSL_FT_REPLAY, &(cmdobj->fault_policy));

	/*
	 * A timeout fault means the IB timed out - clear the policy and
	 * invalidate - this will clear the FT_SKIP_PMDUMP bit but that is okay
	 * because we won't see this cmdobj again
	 */

	if ((fault & ADRENO_TIMEOUT_FAULT) ||
				(fault & ADRENO_CTX_DETATCH_TIMEOUT_FAULT))
		bitmap_zero(&cmdobj->fault_policy, BITS_PER_LONG);

	/*
	 * If the context had a GPU page fault then it is likely it would fault
	 * again if replayed
	 */

	if (test_bit(KGSL_CONTEXT_PRIV_PAGEFAULT,
		     &drawobj->context->priv)) {
		/* we'll need to resume the mmu later... */
		clear_bit(KGSL_FT_REPLAY, &cmdobj->fault_policy);
		clear_bit(KGSL_CONTEXT_PRIV_PAGEFAULT,
			  &drawobj->context->priv);
	}

	/*
	 * Execute the fault tolerance policy. Each cmdobj stores the
	 * current fault policy that was set when it was queued.
	 * As the options are tried in descending priority
	 * (REPLAY -> SKIPIBS -> SKIPFRAME -> NOTHING) the bits are cleared
	 * from the cmdobj policy so the next thing can be tried if the
	 * change comes around again
	 */

	/* Replay the hanging cmdobj again */
	if (test_and_clear_bit(KGSL_FT_REPLAY, &cmdobj->fault_policy)) {
		trace_adreno_cmdbatch_recovery(cmdobj, BIT(KGSL_FT_REPLAY));
		set_bit(KGSL_FT_REPLAY, &cmdobj->fault_recovery);
		return;
	}

	/*
	 * Skip the last IB1 that was played but replay everything else.
	 * Note that the last IB1 might not be in the "hung" cmdobj
	 * because the CP may have caused a page-fault while it was prefetching
	 * the next IB1/IB2. walk all outstanding commands and zap the
	 * supposedly bad IB1 where ever it lurks.
	 */

	if (test_and_clear_bit(KGSL_FT_SKIPIB, &cmdobj->fault_policy)) {
		trace_adreno_cmdbatch_recovery(cmdobj, BIT(KGSL_FT_SKIPIB));
		set_bit(KGSL_FT_SKIPIB, &cmdobj->fault_recovery);

		for (i = 0; i < count; i++) {
			if (replay[i] != NULL &&
				DRAWOBJ(replay[i])->context->id ==
					drawobj->context->id)
				_skip_ib(replay[i], base);
		}

		return;
	}

	/* Skip the faulted cmdobj submission */
	if (test_and_clear_bit(KGSL_FT_SKIPCMD, &cmdobj->fault_policy)) {
		trace_adreno_cmdbatch_recovery(cmdobj, BIT(KGSL_FT_SKIPCMD));

		/* Skip faulting cmdobj */
		_skip_cmd(cmdobj, replay, count);

		return;
	}

	if (test_and_clear_bit(KGSL_FT_SKIPFRAME, &cmdobj->fault_policy)) {
		trace_adreno_cmdbatch_recovery(cmdobj,
			BIT(KGSL_FT_SKIPFRAME));
		set_bit(KGSL_FT_SKIPFRAME, &cmdobj->fault_recovery);

		/*
		 * Skip all the pending cmdobj's for this context until
		 * the EOF frame is seen
		 */
		_skip_frame(cmdobj, replay, count);
		return;
	}

	/* If we get here then all the policies failed */

	pr_context(device, drawobj->context, "gpu %s ctx %d ts %d\n",
		state, drawobj->context->id, drawobj->timestamp);

	/* Mark the context as failed */
	mark_guilty_context(device, drawobj->context->id);

	/* Invalidate the context */
	adreno_drawctxt_invalidate(device, drawobj->context);
}

/**
 * recover_dispatch_q() - Recover all commands in a dispatch queue by
 * resubmitting the commands
 * @device: Device on which recovery is performed
 * @dispatch_q: The command queue to recover
 * @fault: Faults caused by the command in the dispatch q
 * @base: The IB1 base during the fault
 */
static void recover_dispatch_q(struct kgsl_device *device,
		struct adreno_dispatcher_drawqueue *dispatch_q,
		int fault,
		unsigned int base)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_drawobj_cmd **replay;
	unsigned int ptr;
	int first = 0;
	int count = 0;
	int i;

	/* Allocate memory to store the inflight commands */
	replay = kcalloc(dispatch_q->inflight, sizeof(*replay), GFP_KERNEL);

	if (replay == NULL) {
		unsigned int ptr = dispatch_q->head;

		/* Recovery failed - mark everybody on this q guilty */
		while (ptr != dispatch_q->tail) {
			struct kgsl_drawobj_cmd *cmdobj =
						dispatch_q->cmd_q[ptr];
			struct kgsl_drawobj *drawobj = DRAWOBJ(cmdobj);

			mark_guilty_context(device, drawobj->context->id);
			adreno_drawctxt_invalidate(device, drawobj->context);
			kgsl_drawobj_destroy(drawobj);

			ptr = DRAWQUEUE_NEXT(ptr,
				ADRENO_DISPATCH_DRAWQUEUE_SIZE);
		}

		/*
		 * Set the replay count to zero - this will ensure that the
		 * hardware gets reset but nothing else gets played
		 */

		count = 0;
		goto replay;
	}

	/* Copy the inflight cmdobj's into the temporary storage */
	ptr = dispatch_q->head;

	while (ptr != dispatch_q->tail) {
		replay[count++] = dispatch_q->cmd_q[ptr];
		ptr = DRAWQUEUE_NEXT(ptr, ADRENO_DISPATCH_DRAWQUEUE_SIZE);
	}

	if (fault && count)
		process_cmdobj_fault(device, replay,
					count, base, fault);
replay:
	dispatch_q->inflight = 0;
	dispatch_q->head = dispatch_q->tail = 0;
	/* Remove any pending cmdobj's that have been invalidated */
	remove_invalidated_cmdobjs(device, replay, count);

	/* Replay the pending command buffers */
	for (i = 0; i < count; i++) {

		int ret;

		if (replay[i] == NULL)
			continue;

		/*
		 * Force the preamble on the first command (if applicable) to
		 * avoid any strange stage issues
		 */

		if (first == 0) {
			set_bit(CMDOBJ_FORCE_PREAMBLE, &replay[i]->priv);
			first = 1;
		}

		/*
		 * Force each cmdobj to wait for idle - this avoids weird
		 * CP parse issues
		 */

		set_bit(CMDOBJ_WFI, &replay[i]->priv);

		ret = sendcmd(adreno_dev, replay[i]);

		/*
		 * If sending the command fails, then try to recover by
		 * invalidating the context
		 */

		if (ret) {
			pr_context(device, replay[i]->base.context,
				"gpu reset failed ctx %d ts %d\n",
				replay[i]->base.context->id,
				replay[i]->base.timestamp);

			/* Mark this context as guilty (failed recovery) */
			mark_guilty_context(device,
				replay[i]->base.context->id);

			adreno_drawctxt_invalidate(device,
				replay[i]->base.context);
			remove_invalidated_cmdobjs(device, &replay[i],
				count - i);
		}
	}

	/* Clear the fault bit */
	clear_bit(ADRENO_DEVICE_FAULT, &adreno_dev->priv);

	kfree(replay);
}

static void do_header_and_snapshot(struct kgsl_device *device, int fault,
		struct adreno_ringbuffer *rb, struct kgsl_drawobj_cmd *cmdobj)
{
	struct kgsl_drawobj *drawobj = DRAWOBJ(cmdobj);

	/* Always dump the snapshot on a non-drawobj failure */
	if (cmdobj == NULL) {
		adreno_fault_header(device, rb, NULL);
		kgsl_device_snapshot(device, NULL, fault & ADRENO_GMU_FAULT);
		return;
	}

	/* Skip everything if the PMDUMP flag is set */
	if (test_bit(KGSL_FT_SKIP_PMDUMP, &cmdobj->fault_policy))
		return;

	/* Print the fault header */
	adreno_fault_header(device, rb, cmdobj);

	if (!(drawobj->context->flags & KGSL_CONTEXT_NO_SNAPSHOT))
		kgsl_device_snapshot(device, drawobj->context,
					fault & ADRENO_GMU_FAULT);
}

static int dispatcher_do_fault(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;
	struct adreno_dispatcher_drawqueue *dispatch_q = NULL, *dispatch_q_temp;
	struct adreno_ringbuffer *rb;
	struct adreno_ringbuffer *hung_rb = NULL;
	unsigned int reg;
	uint64_t base = 0;
	struct kgsl_drawobj_cmd *cmdobj = NULL;
	int ret, i;
	int fault;
	int halt;
	bool gx_on = true;

	fault = atomic_xchg(&dispatcher->fault, 0);
	if (fault == 0)
		return 0;

	/* Mask all GMU interrupts */
	if (kgsl_gmu_isenabled(device)) {
		adreno_write_gmureg(adreno_dev,
			ADRENO_REG_GMU_AO_HOST_INTERRUPT_MASK,
			0xFFFFFFFF);
		adreno_write_gmureg(adreno_dev,
			ADRENO_REG_GMU_GMU2HOST_INTR_MASK,
			0xFFFFFFFF);
	}

	if (gpudev->gx_is_on)
		gx_on = gpudev->gx_is_on(adreno_dev);

	/*
	 * In the very unlikely case that the power is off, do nothing - the
	 * state will be reset on power up and everybody will be happy
	 */

	if (!kgsl_state_is_awake(device) && (fault & ADRENO_SOFT_FAULT)) {
		/* Clear the existing register values */
		memset(adreno_ft_regs_val, 0,
				adreno_ft_regs_num * sizeof(unsigned int));
		return 0;
	}

	/*
	 * On A5xx and A6xx, read RBBM_STATUS3:SMMU_STALLED_ON_FAULT (BIT 24)
	 * to tell if this function was entered after a pagefault. If so, only
	 * proceed if the fault handler has already run in the IRQ thread,
	 * else return early to give the fault handler a chance to run.
	 */
	if (!(fault & ADRENO_IOMMU_PAGE_FAULT) &&
		(adreno_is_a5xx(adreno_dev) || adreno_is_a6xx(adreno_dev)) &&
		gx_on) {
		unsigned int val;

		mutex_lock(&device->mutex);
		adreno_readreg(adreno_dev, ADRENO_REG_RBBM_STATUS3, &val);
		mutex_unlock(&device->mutex);
		if (val & BIT(24))
			return 0;
	}

	/* Turn off all the timers */
	del_timer_sync(&dispatcher->timer);
	del_timer_sync(&dispatcher->fault_timer);
	/*
	 * Deleting uninitialized timer will block for ever on kernel debug
	 * disable build. Hence skip del timer if it is not initialized.
	 */
	if (adreno_is_preemption_enabled(adreno_dev))
		del_timer_sync(&adreno_dev->preempt.timer);

	mutex_lock(&device->mutex);

	if (gx_on)
		adreno_readreg64(adreno_dev, ADRENO_REG_CP_RB_BASE,
			ADRENO_REG_CP_RB_BASE_HI, &base);

	/*
	 * Force the CP off for anything but a hard fault to make sure it is
	 * good and stopped
	 */
	if (!(fault & ADRENO_HARD_FAULT) && gx_on) {
		adreno_readreg(adreno_dev, ADRENO_REG_CP_ME_CNTL, &reg);
		if (adreno_is_a5xx(adreno_dev) || adreno_is_a6xx(adreno_dev))
			reg |= 1 | (1 << 1);
		else
			reg |= (1 << 27) | (1 << 28);
		adreno_writereg(adreno_dev, ADRENO_REG_CP_ME_CNTL, reg);
	}
	/*
	 * retire cmdobj's from all the dispatch_q's before starting recovery
	 */
	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		adreno_dispatch_retire_drawqueue(adreno_dev,
			&(rb->dispatch_q));
		/* Select the active dispatch_q */
		if (base == rb->buffer_desc.gpuaddr) {
			dispatch_q = &(rb->dispatch_q);
			hung_rb = rb;
			if (adreno_dev->cur_rb != hung_rb) {
				adreno_dev->prev_rb = adreno_dev->cur_rb;
				adreno_dev->cur_rb = hung_rb;
			}
		}
		if (ADRENO_DISPATCHER_RB_STARVE_TIMER_ELAPSED ==
			rb->starve_timer_state) {
			adreno_put_gpu_halt(adreno_dev);
			rb->starve_timer_state =
			ADRENO_DISPATCHER_RB_STARVE_TIMER_UNINIT;
		}
	}

	if (dispatch_q && !adreno_drawqueue_is_empty(dispatch_q)) {
		cmdobj = dispatch_q->cmd_q[dispatch_q->head];
		trace_adreno_cmdbatch_fault(cmdobj, fault);
	}

	if (gx_on)
		adreno_readreg64(adreno_dev, ADRENO_REG_CP_IB1_BASE,
			ADRENO_REG_CP_IB1_BASE_HI, &base);

	do_header_and_snapshot(device, fault, hung_rb, cmdobj);

	/* Turn off the KEEPALIVE vote from the ISR for hard fault */
	if (gpudev->gpu_keepalive && fault & ADRENO_HARD_FAULT)
		gpudev->gpu_keepalive(adreno_dev, false);

	/* Terminate the stalled transaction and resume the IOMMU */
	if (fault & ADRENO_IOMMU_PAGE_FAULT)
		kgsl_mmu_pagefault_resume(&device->mmu);

	/* Reset the dispatcher queue */
	dispatcher->inflight = 0;

	/* Reset the GPU and make sure halt is not set during recovery */
	halt = adreno_gpu_halt(adreno_dev);
	adreno_clear_gpu_halt(adreno_dev);

	/*
	 * If there is a stall in the ringbuffer after all commands have been
	 * retired then we could hit problems if contexts are waiting for
	 * internal timestamps that will never retire
	 */

	if (hung_rb != NULL) {
		kgsl_sharedmem_writel(device, &device->memstore,
				MEMSTORE_RB_OFFSET(hung_rb, soptimestamp),
				hung_rb->timestamp);

		kgsl_sharedmem_writel(device, &device->memstore,
				MEMSTORE_RB_OFFSET(hung_rb, eoptimestamp),
				hung_rb->timestamp);

		/* Schedule any pending events to be run */
		kgsl_process_event_group(device, &hung_rb->events);
	}

	if (gpudev->reset)
		ret = gpudev->reset(device, fault);
	else
		ret = adreno_reset(device, fault);

	mutex_unlock(&device->mutex);
	/* if any other fault got in until reset then ignore */
	atomic_set(&dispatcher->fault, 0);

	/* If adreno_reset() fails then what hope do we have for the future? */
	BUG_ON(ret);

	/* recover all the dispatch_q's starting with the one that hung */
	if (dispatch_q)
		recover_dispatch_q(device, dispatch_q, fault, base);
	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		dispatch_q_temp = &(rb->dispatch_q);
		if (dispatch_q_temp != dispatch_q)
			recover_dispatch_q(device, dispatch_q_temp, 0, base);
	}

	atomic_add(halt, &adreno_dev->halt);

	return 1;
}

static inline int drawobj_consumed(struct kgsl_drawobj *drawobj,
		unsigned int consumed, unsigned int retired)
{
	return ((timestamp_cmp(drawobj->timestamp, consumed) >= 0) &&
		(timestamp_cmp(retired, drawobj->timestamp) < 0));
}

static void _print_recovery(struct kgsl_device *device,
		struct kgsl_drawobj_cmd *cmdobj)
{
	static struct {
		unsigned int mask;
		const char *str;
	} flags[] = { ADRENO_FT_TYPES };

	int i, nr = find_first_bit(&cmdobj->fault_recovery, BITS_PER_LONG);
	char *result = "unknown";
	struct kgsl_drawobj *drawobj = DRAWOBJ(cmdobj);

	for (i = 0; i < ARRAY_SIZE(flags); i++) {
		if (flags[i].mask == BIT(nr)) {
			result = (char *) flags[i].str;
			break;
		}
	}

	pr_context(device, drawobj->context,
		"gpu %s ctx %d ts %d policy %lX\n",
		result, drawobj->context->id, drawobj->timestamp,
		cmdobj->fault_recovery);
}

static void cmdobj_profile_ticks(struct adreno_device *adreno_dev,
	struct kgsl_drawobj_cmd *cmdobj, uint64_t *start, uint64_t *retire)
{
	void *ptr = adreno_dev->profile_buffer.hostptr;
	struct adreno_drawobj_profile_entry *entry;

	entry = (struct adreno_drawobj_profile_entry *)
		(ptr + (cmdobj->profile_index * sizeof(*entry)));

	/* get updated values of started and retired */
	rmb();
	*start = entry->started;
	*retire = entry->retired;
}

static void retire_cmdobj(struct adreno_device *adreno_dev,
		struct kgsl_drawobj_cmd *cmdobj)
{
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;
	struct kgsl_drawobj *drawobj = DRAWOBJ(cmdobj);
	struct adreno_context *drawctxt = ADRENO_CONTEXT(drawobj->context);
	uint64_t start = 0, end = 0;

	if (cmdobj->fault_recovery != 0) {
		set_bit(ADRENO_CONTEXT_FAULT, &drawobj->context->priv);
		_print_recovery(KGSL_DEVICE(adreno_dev), cmdobj);
	}

	if (test_bit(CMDOBJ_PROFILE, &cmdobj->priv))
		cmdobj_profile_ticks(adreno_dev, cmdobj, &start, &end);

	/*
	 * For A3xx we still get the rptr from the CP_RB_RPTR instead of
	 * rptr scratch out address. At this point GPU clocks turned off.
	 * So avoid reading GPU register directly for A3xx.
	 */
	if (adreno_is_a3xx(adreno_dev))
		trace_adreno_cmdbatch_retired(drawobj,
			(int) dispatcher->inflight, start, end,
			ADRENO_DRAWOBJ_RB(drawobj), 0, cmdobj->fault_recovery);
	else
		trace_adreno_cmdbatch_retired(drawobj,
			(int) dispatcher->inflight, start, end,
			ADRENO_DRAWOBJ_RB(drawobj),
			adreno_get_rptr(drawctxt->rb), cmdobj->fault_recovery);

	drawctxt->submit_retire_ticks[drawctxt->ticks_index] =
		end - cmdobj->submit_ticks;

	drawctxt->ticks_index = (drawctxt->ticks_index + 1) %
		SUBMIT_RETIRE_TICKS_SIZE;

	kgsl_drawobj_destroy(drawobj);
}

static int adreno_dispatch_retire_drawqueue(struct adreno_device *adreno_dev,
		struct adreno_dispatcher_drawqueue *drawqueue)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;
	int count = 0;

	while (!adreno_drawqueue_is_empty(drawqueue)) {
		struct kgsl_drawobj_cmd *cmdobj =
			drawqueue->cmd_q[drawqueue->head];
		struct kgsl_drawobj *drawobj = DRAWOBJ(cmdobj);

		if (!kgsl_check_timestamp(device, drawobj->context,
			drawobj->timestamp))
			break;

		retire_cmdobj(adreno_dev, cmdobj);

		dispatcher->inflight--;
		drawqueue->inflight--;

		drawqueue->cmd_q[drawqueue->head] = NULL;

		drawqueue->head = DRAWQUEUE_NEXT(drawqueue->head,
			ADRENO_DISPATCH_DRAWQUEUE_SIZE);

		count++;
	}

	return count;
}

static void _adreno_dispatch_check_timeout(struct adreno_device *adreno_dev,
		struct adreno_dispatcher_drawqueue *drawqueue)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_drawobj *drawobj =
			DRAWOBJ(drawqueue->cmd_q[drawqueue->head]);

	/* Don't timeout if the timer hasn't expired yet (duh) */
	if (time_is_after_jiffies(drawqueue->expires))
		return;

	/* Don't timeout if the IB timeout is disabled globally */
	if (!adreno_long_ib_detect(adreno_dev))
		return;

	/* Don't time out if the context has disabled it */
	if (drawobj->context->flags & KGSL_CONTEXT_NO_FAULT_TOLERANCE)
		return;

	pr_context(device, drawobj->context, "gpu timeout ctx %d ts %d\n",
		drawobj->context->id, drawobj->timestamp);

	adreno_set_gpu_fault(adreno_dev, ADRENO_TIMEOUT_FAULT);
}

static int adreno_dispatch_process_drawqueue(struct adreno_device *adreno_dev,
		struct adreno_dispatcher_drawqueue *drawqueue)
{
	int count = adreno_dispatch_retire_drawqueue(adreno_dev, drawqueue);

	/* Nothing to do if there are no pending commands */
	if (adreno_drawqueue_is_empty(drawqueue))
		return count;

	/* Don't update the drawqueue timeout if it isn't active */
	if (!drawqueue_is_current(drawqueue))
		return count;

	/*
	 * If the current ringbuffer retired any commands then universally
	 * reset the timeout
	 */

	if (count) {
		drawqueue->expires = jiffies +
			msecs_to_jiffies(adreno_drawobj_timeout);
		return count;
	}

	/*
	 * If we get here then 1) the ringbuffer is current and 2) we haven't
	 * retired anything.  Check to see if the timeout if valid for the
	 * current drawobj and fault if it has expired
	 */
	_adreno_dispatch_check_timeout(adreno_dev, drawqueue);
	return 0;
}

/* Update the dispatcher timers */
static void _dispatcher_update_timers(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;

	/* Kick the idle timer */
	mutex_lock(&device->mutex);
	kgsl_pwrscale_update(device);
	mod_timer(&device->idle_timer,
		jiffies + device->pwrctrl.interval_timeout);
	mutex_unlock(&device->mutex);

	/* Check to see if we need to update the command timer */
	if (adreno_in_preempt_state(adreno_dev, ADRENO_PREEMPT_NONE)) {
		struct adreno_dispatcher_drawqueue *drawqueue =
			DRAWQUEUE(adreno_dev->cur_rb);

		if (!adreno_drawqueue_is_empty(drawqueue))
			mod_timer(&dispatcher->timer, drawqueue->expires);
	}
}

/* Take down the dispatcher and release any power states */
static void _dispatcher_power_down(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;

	mutex_lock(&device->mutex);

	if (test_and_clear_bit(ADRENO_DISPATCHER_ACTIVE, &dispatcher->priv))
		complete_all(&dispatcher->idle_gate);

	del_timer_sync(&dispatcher->fault_timer);

	if (test_bit(ADRENO_DISPATCHER_POWER, &dispatcher->priv)) {
		kgsl_active_count_put(device);
		clear_bit(ADRENO_DISPATCHER_POWER, &dispatcher->priv);
	}

	mutex_unlock(&device->mutex);
}

static void adreno_dispatcher_work(struct kthread_work *work)
{
	struct adreno_dispatcher *dispatcher =
		container_of(work, struct adreno_dispatcher, work);
	struct adreno_device *adreno_dev =
		container_of(dispatcher, struct adreno_device, dispatcher);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	int count = 0;
	unsigned int i = 0;

	mutex_lock(&dispatcher->mutex);

	/*
	 * As long as there are inflight commands, process retired comamnds from
	 * all drawqueues
	 */
	for (i = 0; i < adreno_dev->num_ringbuffers; i++) {
		struct adreno_dispatcher_drawqueue *drawqueue =
			DRAWQUEUE(&adreno_dev->ringbuffers[i]);

		count += adreno_dispatch_process_drawqueue(adreno_dev,
			drawqueue);
		if (dispatcher->inflight == 0)
			break;
	}

	kgsl_process_event_groups(device);

	/*
	 * dispatcher_do_fault() returns 0 if no faults occurred. If that is the
	 * case, then clean up preemption and try to schedule more work
	 */
	if (dispatcher_do_fault(adreno_dev) == 0) {

		/* Clean up after preemption */
		if (gpudev->preemption_schedule)
			gpudev->preemption_schedule(adreno_dev);

		/* Run the scheduler for to dispatch new commands */
		_adreno_dispatcher_issuecmds(adreno_dev);
	}

	/*
	 * If there are commands pending, update the timers, otherwise release
	 * the power state to prepare for power down
	 */
	if (dispatcher->inflight > 0)
		_dispatcher_update_timers(adreno_dev);
	else
		_dispatcher_power_down(adreno_dev);

	mutex_unlock(&dispatcher->mutex);
}

void adreno_dispatcher_schedule(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;

	kthread_queue_work(&kgsl_driver.worker, &dispatcher->work);
}

/**
 * adreno_dispatcher_queue_context() - schedule a drawctxt in the dispatcher
 * device: pointer to the KGSL device
 * drawctxt: pointer to the drawctxt to schedule
 *
 * Put a draw context on the dispatcher pending queue and schedule the
 * dispatcher. This is used to reschedule changes that might have been blocked
 * for sync points or other concerns
 */
void adreno_dispatcher_queue_context(struct kgsl_device *device,
	struct adreno_context *drawctxt)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	dispatcher_queue_context(adreno_dev, drawctxt);
	adreno_dispatcher_schedule(device);
}

/*
 * This is called on a regular basis while cmdobj's are inflight.  Fault
 * detection registers are read and compared to the existing values - if they
 * changed then the GPU is still running.  If they are the same between
 * subsequent calls then the GPU may have faulted
 */

static void adreno_dispatcher_fault_timer(unsigned long data)
{
	struct adreno_device *adreno_dev = (struct adreno_device *) data;
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;

	/* Leave if the user decided to turn off fast hang detection */
	if (!adreno_soft_fault_detect(adreno_dev))
		return;

	if (adreno_gpu_fault(adreno_dev)) {
		adreno_dispatcher_schedule(KGSL_DEVICE(adreno_dev));
		return;
	}

	/*
	 * Read the fault registers - if it returns 0 then they haven't changed
	 * so mark the dispatcher as faulted and schedule the work loop.
	 */

	if (!fault_detect_read_compare(adreno_dev)) {
		adreno_set_gpu_fault(adreno_dev, ADRENO_SOFT_FAULT);
		adreno_dispatcher_schedule(KGSL_DEVICE(adreno_dev));
	} else if (dispatcher->inflight > 0) {
		mod_timer(&dispatcher->fault_timer,
			jiffies + msecs_to_jiffies(_fault_timer_interval));
	}
}

/*
 * This is called when the timer expires - it either means the GPU is hung or
 * the IB is taking too long to execute
 */
static void adreno_dispatcher_timer(unsigned long data)
{
	struct adreno_device *adreno_dev = (struct adreno_device *) data;

	adreno_dispatcher_schedule(KGSL_DEVICE(adreno_dev));
}

/**
 * adreno_dispatcher_start() - activate the dispatcher
 * @adreno_dev: pointer to the adreno device structure
 *
 */
void adreno_dispatcher_start(struct kgsl_device *device)
{
	complete_all(&device->halt_gate);

	/* Schedule the work loop to get things going */
	adreno_dispatcher_schedule(device);
}

/**
 * adreno_dispatcher_stop() - stop the dispatcher
 * @adreno_dev: pointer to the adreno device structure
 *
 * Stop the dispatcher and close all the timers
 */
void adreno_dispatcher_stop(struct adreno_device *adreno_dev)
{
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;

	del_timer_sync(&dispatcher->timer);
	del_timer_sync(&dispatcher->fault_timer);
}

/**
 * adreno_dispatcher_stop_fault_timer() - stop the dispatcher fault timer
 * @device: pointer to the KGSL device structure
 *
 * Stop the dispatcher fault timer
 */
void adreno_dispatcher_stop_fault_timer(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;

	del_timer_sync(&dispatcher->fault_timer);
}

/**
 * adreno_dispatcher_close() - close the dispatcher
 * @adreno_dev: pointer to the adreno device structure
 *
 * Close the dispatcher and free all the oustanding commands and memory
 */
void adreno_dispatcher_close(struct adreno_device *adreno_dev)
{
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;
	int i;
	struct adreno_ringbuffer *rb;

	mutex_lock(&dispatcher->mutex);
	del_timer_sync(&dispatcher->timer);
	del_timer_sync(&dispatcher->fault_timer);

	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		struct adreno_dispatcher_drawqueue *dispatch_q =
			&(rb->dispatch_q);
		while (!adreno_drawqueue_is_empty(dispatch_q)) {
			kgsl_drawobj_destroy(
				DRAWOBJ(dispatch_q->cmd_q[dispatch_q->head]));
			dispatch_q->head = (dispatch_q->head + 1)
				% ADRENO_DISPATCH_DRAWQUEUE_SIZE;
		}
	}

	mutex_unlock(&dispatcher->mutex);

	kobject_put(&dispatcher->kobj);
}

struct dispatcher_attribute {
	struct attribute attr;
	ssize_t (*show)(struct adreno_dispatcher *,
			struct dispatcher_attribute *, char *);
	ssize_t (*store)(struct adreno_dispatcher *,
			struct dispatcher_attribute *, const char *buf,
			size_t count);
	unsigned int max;
	unsigned int *value;
};

#define DISPATCHER_UINT_ATTR(_name, _mode, _max, _value) \
	struct dispatcher_attribute dispatcher_attr_##_name =  { \
		.attr = { .name = __stringify(_name), .mode = _mode }, \
		.show = _show_uint, \
		.store = _store_uint, \
		.max = _max, \
		.value = &(_value), \
	}

#define to_dispatcher_attr(_a) \
	container_of((_a), struct dispatcher_attribute, attr)
#define to_dispatcher(k) container_of(k, struct adreno_dispatcher, kobj)

static ssize_t _store_uint(struct adreno_dispatcher *dispatcher,
		struct dispatcher_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int val = 0;
	int ret;

	ret = kgsl_sysfs_store(buf, &val);
	if (ret)
		return ret;

	if (!val || (attr->max && (val > attr->max)))
		return -EINVAL;

	*((unsigned int *) attr->value) = val;
	return size;
}

static ssize_t _show_uint(struct adreno_dispatcher *dispatcher,
		struct dispatcher_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n",
		*((unsigned int *) attr->value));
}

static DISPATCHER_UINT_ATTR(inflight, 0644, ADRENO_DISPATCH_DRAWQUEUE_SIZE,
	_dispatcher_q_inflight_hi);

static DISPATCHER_UINT_ATTR(inflight_low_latency, 0644,
	ADRENO_DISPATCH_DRAWQUEUE_SIZE, _dispatcher_q_inflight_lo);
/*
 * Our code that "puts back" a command from the context is much cleaner
 * if we are sure that there will always be enough room in the
 * ringbuffer so restrict the maximum size of the context queue to
 * ADRENO_CONTEXT_DRAWQUEUE_SIZE - 1
 */
static DISPATCHER_UINT_ATTR(context_drawqueue_size, 0644,
	ADRENO_CONTEXT_DRAWQUEUE_SIZE - 1, _context_drawqueue_size);
static DISPATCHER_UINT_ATTR(context_burst_count, 0644, 0,
	_context_drawobj_burst);
static DISPATCHER_UINT_ATTR(drawobj_timeout, 0644, 0,
	adreno_drawobj_timeout);
static DISPATCHER_UINT_ATTR(context_queue_wait, 0644, 0, _context_queue_wait);
static DISPATCHER_UINT_ATTR(fault_detect_interval, 0644, 0,
	_fault_timer_interval);
static DISPATCHER_UINT_ATTR(fault_throttle_time, 0644, 0,
	_fault_throttle_time);
static DISPATCHER_UINT_ATTR(fault_throttle_burst, 0644, 0,
	_fault_throttle_burst);
static DISPATCHER_UINT_ATTR(disp_preempt_fair_sched, 0644, 0,
	adreno_disp_preempt_fair_sched);
static DISPATCHER_UINT_ATTR(dispatch_time_slice, 0644, 0,
	adreno_dispatch_time_slice);
static DISPATCHER_UINT_ATTR(dispatch_starvation_time, 0644, 0,
	adreno_dispatch_starvation_time);

static struct attribute *dispatcher_attrs[] = {
	&dispatcher_attr_inflight.attr,
	&dispatcher_attr_inflight_low_latency.attr,
	&dispatcher_attr_context_drawqueue_size.attr,
	&dispatcher_attr_context_burst_count.attr,
	&dispatcher_attr_drawobj_timeout.attr,
	&dispatcher_attr_context_queue_wait.attr,
	&dispatcher_attr_fault_detect_interval.attr,
	&dispatcher_attr_fault_throttle_time.attr,
	&dispatcher_attr_fault_throttle_burst.attr,
	&dispatcher_attr_disp_preempt_fair_sched.attr,
	&dispatcher_attr_dispatch_time_slice.attr,
	&dispatcher_attr_dispatch_starvation_time.attr,
	NULL,
};

static ssize_t dispatcher_sysfs_show(struct kobject *kobj,
				   struct attribute *attr, char *buf)
{
	struct adreno_dispatcher *dispatcher = to_dispatcher(kobj);
	struct dispatcher_attribute *pattr = to_dispatcher_attr(attr);
	ssize_t ret = -EIO;

	if (pattr->show)
		ret = pattr->show(dispatcher, pattr, buf);

	return ret;
}

static ssize_t dispatcher_sysfs_store(struct kobject *kobj,
				    struct attribute *attr,
				    const char *buf, size_t count)
{
	struct adreno_dispatcher *dispatcher = to_dispatcher(kobj);
	struct dispatcher_attribute *pattr = to_dispatcher_attr(attr);
	ssize_t ret = -EIO;

	if (pattr->store)
		ret = pattr->store(dispatcher, pattr, buf, count);

	return ret;
}

static const struct sysfs_ops dispatcher_sysfs_ops = {
	.show = dispatcher_sysfs_show,
	.store = dispatcher_sysfs_store
};

static struct kobj_type ktype_dispatcher = {
	.sysfs_ops = &dispatcher_sysfs_ops,
	.default_attrs = dispatcher_attrs,
};

/**
 * adreno_dispatcher_init() - Initialize the dispatcher
 * @adreno_dev: pointer to the adreno device structure
 *
 * Initialize the dispatcher
 */
int adreno_dispatcher_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;
	int ret;

	memset(dispatcher, 0, sizeof(*dispatcher));

	mutex_init(&dispatcher->mutex);

	setup_timer(&dispatcher->timer, adreno_dispatcher_timer,
		(unsigned long) adreno_dev);

	setup_timer(&dispatcher->fault_timer, adreno_dispatcher_fault_timer,
		(unsigned long) adreno_dev);

	kthread_init_work(&dispatcher->work, adreno_dispatcher_work);

	init_completion(&dispatcher->idle_gate);
	complete_all(&dispatcher->idle_gate);

	plist_head_init(&dispatcher->pending);
	spin_lock_init(&dispatcher->plist_lock);

	ret = kobject_init_and_add(&dispatcher->kobj, &ktype_dispatcher,
		&device->dev->kobj, "dispatch");

	return ret;
}

/*
 * adreno_dispatcher_idle() - Wait for dispatcher to idle
 * @adreno_dev: Adreno device whose dispatcher needs to idle
 *
 * Signal dispatcher to stop sending more commands and complete
 * the commands that have already been submitted. This function
 * should not be called when dispatcher mutex is held.
 * The caller must hold the device mutex.
 */
int adreno_dispatcher_idle(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;
	int ret;

	if (!test_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv))
		return 0;

	/*
	 * Ensure that this function is not called when dispatcher
	 * mutex is held and device is started
	 */
	if (mutex_is_locked(&dispatcher->mutex) &&
		dispatcher->mutex.owner == current)
		return -EDEADLK;

	adreno_get_gpu_halt(adreno_dev);

	mutex_unlock(&device->mutex);

	ret = wait_for_completion_timeout(&dispatcher->idle_gate,
			msecs_to_jiffies(ADRENO_IDLE_TIMEOUT));
	if (ret == 0) {
		ret = -ETIMEDOUT;
		WARN(1, "Dispatcher halt timeout ");
	} else if (ret < 0) {
		KGSL_DRV_ERR(device, "Dispatcher halt failed %d\n", ret);
	} else {
		ret = 0;
	}

	mutex_lock(&device->mutex);
	adreno_put_gpu_halt(adreno_dev);
	/*
	 * requeue dispatcher work to resubmit pending commands
	 * that may have been blocked due to this idling request
	 */
	adreno_dispatcher_schedule(device);
	return ret;
}
