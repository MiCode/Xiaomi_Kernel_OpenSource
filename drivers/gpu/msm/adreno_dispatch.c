// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/slab.h>
#include <linux/sysfs.h>
#include <soc/qcom/msm_performance.h>
#include "adreno.h"
#include "adreno_sysfs.h"
#include "adreno_trace.h"
#include "kgsl_bus.h"
#include "kgsl_eventlog.h"
#include "kgsl_gmu_core.h"
#include "kgsl_timeline.h"

#define DRAWQUEUE_NEXT(_i, _s) (((_i) + 1) % (_s))

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

/* Use a kmem cache to speed up allocations for dispatcher jobs */
static struct kmem_cache *jobs_cache;

#define DRAWQUEUE_RB(_drawqueue) \
	((struct adreno_ringbuffer *) \
		container_of((_drawqueue),\
		struct adreno_ringbuffer, dispatch_q))

#define DRAWQUEUE(_ringbuffer) (&(_ringbuffer)->dispatch_q)

static bool adreno_drawqueue_is_empty(struct adreno_dispatcher_drawqueue *drawqueue)
{
	return (drawqueue && drawqueue->head == drawqueue->tail);
}

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
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int i;

	if (!test_bit(ADRENO_DEVICE_SOFT_FAULT_DETECT, &adreno_dev->priv))
		return;

	for (i = 0; i < adreno_dev->num_ringbuffers; i++) {
		struct adreno_ringbuffer *rb = &(adreno_dev->ringbuffers[i]);

		adreno_rb_readtimestamp(adreno_dev, rb,
			KGSL_TIMESTAMP_RETIRED, &(rb->fault_detect_ts));
	}

	for (i = 0; i < adreno_dev->soft_ft_count; i++) {
		if (adreno_dev->soft_ft_regs[i])
			kgsl_regread(device, adreno_dev->soft_ft_regs[i],
				&adreno_dev->soft_ft_vals[i]);
	}
}

void adreno_dispatcher_start_fault_timer(struct adreno_device *adreno_dev)
{
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;

	if (adreno_soft_fault_detect(adreno_dev))
		mod_timer(&dispatcher->fault_timer,
			jiffies + msecs_to_jiffies(_fault_timer_interval));
}

/*
 * This takes a kgsl_device pointer so that it can be used for the function
 * hook in adreno.c too
 */
void adreno_dispatcher_stop_fault_timer(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;

	if (ADRENO_FEATURE(adreno_dev, ADRENO_SOFT_FAULT_DETECT))
		del_timer_sync(&dispatcher->fault_timer);
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
	struct adreno_ringbuffer *rb = drawctxt->rb;
	struct retire_info info = {0};

	/*
	 * Write the start and end timestamp to the memstore to keep the
	 * accounting sane
	 */
	kgsl_sharedmem_writel(device->memstore,
		KGSL_MEMSTORE_OFFSET(context->id, soptimestamp),
		drawobj->timestamp);

	kgsl_sharedmem_writel(device->memstore,
		KGSL_MEMSTORE_OFFSET(context->id, eoptimestamp),
		drawobj->timestamp);

	drawctxt->submitted_timestamp = drawobj->timestamp;

	/* Retire pending GPU events for the object */
	kgsl_process_event_group(device, &context->events);

	info.inflight = -1;
	info.rb_id = rb->id;
	info.wptr = rb->wptr;
	info.timestamp = drawobj->timestamp;

	msm_perf_events_update(MSM_PERF_GFX, MSM_PERF_RETIRED,
				pid_nr(context->proc_priv->pid),
				context->id, drawobj->timestamp,
				!!(drawobj->flags & KGSL_DRAWOBJ_END_OF_FRAME));

	if (drawobj->flags & KGSL_DRAWOBJ_END_OF_FRAME) {
		atomic64_inc(&context->proc_priv->frame_count);
		atomic_inc(&context->proc_priv->period->frames);
	}

	/*
	 * For A3xx we still get the rptr from the CP_RB_RPTR instead of
	 * rptr scratch out address. At this point GPU clocks turned off.
	 * So avoid reading GPU register directly for A3xx.
	 */
	if (adreno_is_a3xx(ADRENO_DEVICE(device))) {
		trace_adreno_cmdbatch_retired(context, &info,
			drawobj->flags, rb->dispatch_q.inflight, 0);
	} else {
		info.rptr = adreno_get_rptr(rb);

		trace_adreno_cmdbatch_retired(context, &info,
			drawobj->flags, rb->dispatch_q.inflight, 0);
	}

	log_kgsl_cmdbatch_retired_event(context->id, drawobj->timestamp,
		context->priority, drawobj->flags, 0, 0);

	kgsl_drawobj_destroy(drawobj);
}

static int _check_context_queue(struct adreno_context *drawctxt, u32 count)
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
		ret = ((drawctxt->queued + count) < _context_drawqueue_size) ? 1 : 0;

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

static int dispatch_retire_markerobj(struct kgsl_drawobj *drawobj,
				struct adreno_context *drawctxt)
{
	struct kgsl_drawobj_cmd *cmdobj = CMDOBJ(drawobj);

	if (_marker_expired(cmdobj)) {
		_pop_drawobj(drawctxt);
		_retire_timestamp(drawobj);
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

static int dispatch_retire_syncobj(struct kgsl_drawobj *drawobj,
				struct adreno_context *drawctxt)
{
	struct kgsl_drawobj_sync *syncobj = SYNCOBJ(drawobj);

	if (!kgsl_drawobj_events_pending(syncobj)) {
		_pop_drawobj(drawctxt);
		kgsl_drawobj_destroy(drawobj);
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

static int drawqueue_retire_timelineobj(struct kgsl_drawobj *drawobj,
		struct adreno_context *drawctxt)
{
	struct kgsl_drawobj_timeline *timelineobj = TIMELINEOBJ(drawobj);
	int i;

	for (i = 0; i < timelineobj->count; i++)
		kgsl_timeline_signal(timelineobj->timelines[i].timeline,
			timelineobj->timelines[i].seqno);

	_pop_drawobj(drawctxt);
	_retire_timestamp(drawobj);

	return 0;
}

static int drawqueue_retire_bindobj(struct kgsl_drawobj *drawobj,
		struct adreno_context *drawctxt)
{
	struct kgsl_drawobj_bind *bindobj = BINDOBJ(drawobj);

	if (test_bit(KGSL_BINDOBJ_STATE_DONE, &bindobj->state)) {
		_pop_drawobj(drawctxt);
		_retire_timestamp(drawobj);
		return 0;
	}

	if (!test_and_set_bit(KGSL_BINDOBJ_STATE_START, &bindobj->state)) {
		/*
		 * Take a referencre to the drawobj and the context because both
		 * get referenced in the bind callback
		 */
		_kgsl_context_get(&drawctxt->base);
		kref_get(&drawobj->refcount);

		kgsl_sharedmem_bind_ranges(bindobj->bind);
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

	if (drawctxt->drawqueue_head == drawctxt->drawqueue_tail)
		return NULL;

	for (i = drawctxt->drawqueue_head; i != drawctxt->drawqueue_tail;
			i = DRAWQUEUE_NEXT(i, ADRENO_CONTEXT_DRAWQUEUE_SIZE)) {
		int ret = 0;

		drawobj = drawctxt->drawqueue[i];
		if (!drawobj)
			return NULL;

		switch (drawobj->type) {
		case CMDOBJ_TYPE:
			return drawobj;
		case MARKEROBJ_TYPE:
			ret = dispatch_retire_markerobj(drawobj, drawctxt);
			/* Special case where marker needs to be sent to GPU */
			if (ret == 1)
				return drawobj;
			break;
		case SYNCOBJ_TYPE:
			ret = dispatch_retire_syncobj(drawobj, drawctxt);
			break;
		case BINDOBJ_TYPE:
			ret = drawqueue_retire_bindobj(drawobj, drawctxt);
			break;
		case TIMELINEOBJ_TYPE:
			ret = drawqueue_retire_timelineobj(drawobj, drawctxt);
			break;
		default:
			ret = -EINVAL;
			break;
		}

		if (ret)
			return ERR_PTR(ret);
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

	if (kgsl_context_is_bad(&drawctxt->base)) {
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
	cmdobj->requeue_cnt++;
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
static int dispatcher_queue_context(struct adreno_device *adreno_dev,
		struct adreno_context *drawctxt)
{
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;
	struct adreno_dispatch_job *job;

	/* Refuse to queue a detached context */
	if (kgsl_context_detached(&drawctxt->base))
		return 0;

	if (!_kgsl_context_get(&drawctxt->base))
		return 0;

	/* This function can be called in an atomic context */
	job = kmem_cache_alloc(jobs_cache, GFP_ATOMIC);
	if (!job) {
		kgsl_context_put(&drawctxt->base);
		return -ENOMEM;
	}

	job->drawctxt = drawctxt;

	trace_dispatch_queue_context(drawctxt);
	llist_add(&job->node, &dispatcher->jobs[drawctxt->base.priority]);

	return 0;
}

/*
 * Real time clients may demand high BW and have strict latency requirement.
 * GPU bus DCVS is not fast enough to account for sudden BW requirements.
 * Bus hint helps to bump up the bus vote (IB) upfront for known time-critical
 * workloads.
 */
static void process_rt_bus_hint(struct kgsl_device *device, bool on)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_dispatcher_drawqueue *drawqueue =
			DRAWQUEUE(&adreno_dev->ringbuffers[0]);

	if (!adreno_is_preemption_enabled(adreno_dev) ||
		!device->pwrctrl.rt_bus_hint)
		return;

	if (device->pwrctrl.rt_bus_hint_active == on)
		return;

	if (on && drawqueue->inflight == 1)
		kgsl_bus_update(device, KGSL_BUS_VOTE_RT_HINT_ON);

	if (!on && drawqueue->inflight == 0)
		kgsl_bus_update(device, KGSL_BUS_VOTE_RT_HINT_OFF);
}

#define ADRENO_DRAWOBJ_PROFILE_COUNT \
	(PAGE_SIZE / sizeof(struct adreno_drawobj_profile_entry))

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
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;
	struct adreno_context *drawctxt = ADRENO_CONTEXT(drawobj->context);
	struct kgsl_context *context = drawobj->context;
	struct adreno_dispatcher_drawqueue *dispatch_q = &drawctxt->rb->dispatch_q;
	struct adreno_submit_time time;
	uint64_t secs = 0;
	unsigned long nsecs = 0;
	int ret;
	struct submission_info info = {0};

	mutex_lock(&device->mutex);
	if (adreno_gpu_halt(adreno_dev) != 0) {
		mutex_unlock(&device->mutex);
		return -EBUSY;
	}

	memset(&time, 0x0, sizeof(time));

	dispatcher->inflight++;
	dispatch_q->inflight++;

	if (dispatcher->inflight == 1 &&
			!test_bit(ADRENO_DISPATCHER_POWER, &dispatcher->priv)) {
		/* Time to make the donuts.  Turn on the GPU */
		ret = adreno_active_count_get(adreno_dev);
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

	process_rt_bus_hint(device, true);

	ret = adreno_ringbuffer_submitcmd(adreno_dev, cmdobj, &time);

	/*
	 * On the first command, if the submission was successful, then read the
	 * fault registers.  If it failed then turn off the GPU. Sad face.
	 */

	if (dispatcher->inflight == 1) {
		if (ret == 0) {

			/* Stop fault timer before reading fault registers */
			adreno_dispatcher_stop_fault_timer(device);

			fault_detect_read(adreno_dev);

			/* Start the fault timer on first submission */
			adreno_dispatcher_start_fault_timer(adreno_dev);

			if (!test_and_set_bit(ADRENO_DISPATCHER_ACTIVE,
				&dispatcher->priv))
				reinit_completion(&dispatcher->idle_gate);
		} else {
			adreno_active_count_put(adreno_dev);
			clear_bit(ADRENO_DISPATCHER_POWER, &dispatcher->priv);
		}
	}


	if (ret) {
		dispatcher->inflight--;
		dispatch_q->inflight--;

		process_rt_bus_hint(device, false);

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
			dev_err(device->dev,
				     "Unable to submit command to the ringbuffer %d\n",
				     ret);
		return ret;
	}

	secs = time.ktime;
	nsecs = do_div(secs, 1000000000);

	/*
	 * For the first submission in any given command queue update the
	 * expected expire time - this won't actually be used / updated until
	 * the command queue in question goes current, but universally setting
	 * it here avoids the possibilty of some race conditions with preempt
	 */

	if (dispatch_q->inflight == 1)
		dispatch_q->expires = jiffies +
			msecs_to_jiffies(adreno_drawobj_timeout);

	info.inflight = (int) dispatcher->inflight;
	info.rb_id = drawctxt->rb->id;
	info.rptr = adreno_get_rptr(drawctxt->rb);
	info.wptr = drawctxt->rb->wptr;
	info.gmu_dispatch_queue = -1;

	msm_perf_events_update(MSM_PERF_GFX, MSM_PERF_SUBMIT,
			       pid_nr(context->proc_priv->pid),
			       context->id, drawobj->timestamp,
			       !!(drawobj->flags & KGSL_DRAWOBJ_END_OF_FRAME));

	trace_adreno_cmdbatch_submitted(drawobj, &info,
			time.ticks, (unsigned long) secs, nsecs / 1000,
			dispatch_q->inflight);

	log_kgsl_cmdbatch_submitted_event(context->id, drawobj->timestamp,
		context->priority, drawobj->flags);

	mutex_unlock(&device->mutex);

	cmdobj->submit_ticks = time.ticks;

	dispatch_q->cmd_q[dispatch_q->tail] = cmdobj;
	dispatch_q->tail = (dispatch_q->tail + 1) %
		ADRENO_DISPATCH_DRAWQUEUE_SIZE;

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
		struct kgsl_context *context;

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
		context = drawobj->context;
		trace_adreno_cmdbatch_ready(context->id, context->priority,
			drawobj->timestamp, cmdobj->requeue_cnt);
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

	if (_check_context_queue(drawctxt, 0))
		wake_up_all(&drawctxt->wq);

	if (!ret)
		ret = count;

	/* Return error or the number of commands queued */
	return ret;
}

static bool adreno_gpu_stopped(struct adreno_device *adreno_dev)
{
	return (adreno_gpu_fault(adreno_dev) || adreno_gpu_halt(adreno_dev));
}

static void dispatcher_handle_jobs_list(struct adreno_device *adreno_dev,
		int id, unsigned long *map, struct llist_node *list)
{
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;
	struct adreno_dispatch_job *job, *next;

	if (!list)
		return;

	/* Reverse the order so the oldest context is considered first */
	list = llist_reverse_order(list);

	llist_for_each_entry_safe(job, next, list, node) {
		int ret;

		if (kgsl_context_is_bad(&job->drawctxt->base)) {
			kgsl_context_put(&job->drawctxt->base);
			kmem_cache_free(jobs_cache, job);
			continue;
		}

		/*
		 * Due to the nature of the lockless queue the same context
		 * might have multiple jobs on the list. We allow this so we
		 * don't have to query the list on the producer side but on the
		 * consumer side we only want each context to be considered
		 * once. Use a bitmap to remember which contexts we've already
		 * seen and quietly discard duplicate jobs
		 */
		if (test_and_set_bit(job->drawctxt->base.id, map)) {
			kgsl_context_put(&job->drawctxt->base);
			kmem_cache_free(jobs_cache, job);
			continue;
		}

		/*
		 * If gpu is in fault or dispatcher is halted, add back the jobs
		 * so that they are processed after recovery or when dispatcher
		 * is resumed.
		 */
		if (adreno_gpu_stopped(adreno_dev)) {
			llist_add(&job->node, &dispatcher->jobs[id]);
			continue;
		}

		ret = dispatcher_context_sendcmds(adreno_dev, job->drawctxt);

		/*
		 * If the context had nothing queued or the context has been
		 * destroyed then drop the job
		 */
		if (!ret || ret == -ENOENT) {
			kgsl_context_put(&job->drawctxt->base);
			kmem_cache_free(jobs_cache, job);
			continue;
		}

		/*
		 * If the ringbuffer is full then requeue the job to be
		 * considered first next time. Otherwise the context
		 * either successfully submmitted to the GPU or another error
		 * happened and it should go back on the regular queue
		 */
		if (ret == -EBUSY)
			llist_add(&job->node, &dispatcher->requeue[id]);
		else
			llist_add(&job->node, &dispatcher->jobs[id]);
	}
}

static void dispatcher_handle_jobs(struct adreno_device *adreno_dev, int id)
{
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;
	unsigned long map[BITS_TO_LONGS(KGSL_MEMSTORE_MAX)];
	struct llist_node *requeue, *jobs;

	memset(map, 0, sizeof(map));

	requeue = llist_del_all(&dispatcher->requeue[id]);
	jobs = llist_del_all(&dispatcher->jobs[id]);

	dispatcher_handle_jobs_list(adreno_dev, id, map, requeue);
	dispatcher_handle_jobs_list(adreno_dev, id, map, jobs);
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
	int i;

	/* Leave early if the dispatcher isn't in a happy state */
	if (adreno_gpu_fault(adreno_dev) != 0)
		return;

	for (i = 0; i < ARRAY_SIZE(dispatcher->jobs); i++)
		dispatcher_handle_jobs(adreno_dev, i);
}

/* Update the dispatcher timers */
static void _dispatcher_update_timers(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;

	/* Kick the idle timer */
	mutex_lock(&device->mutex);
	kgsl_pwrscale_update(device);
	process_rt_bus_hint(device, false);
	kgsl_start_idle_timer(device);
	mutex_unlock(&device->mutex);

	/* Check to see if we need to update the command timer */
	if (adreno_in_preempt_state(adreno_dev, ADRENO_PREEMPT_NONE)) {
		struct adreno_dispatcher_drawqueue *drawqueue =
			DRAWQUEUE(adreno_dev->cur_rb);

		if (!adreno_drawqueue_is_empty(drawqueue))
			mod_timer(&dispatcher->timer, drawqueue->expires);
	}
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
	if (device->slumber) {
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

	if (dispatcher->inflight)
		_dispatcher_update_timers(adreno_dev);

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

static inline int _wait_for_room_in_context_queue(
	struct adreno_context *drawctxt, u32 count) __must_hold(&drawctxt->lock)
{
	int ret = 0;

	/*
	 * There is always a possibility that dispatcher may end up pushing
	 * the last popped draw object back to the context drawqueue. Hence,
	 * we can only queue up to _context_drawqueue_size - 1 here to make
	 * sure we never let drawqueue->queued exceed _context_drawqueue_size.
	 */
	if ((drawctxt->queued + count) > (_context_drawqueue_size - 1)) {
		trace_adreno_drawctxt_sleep(drawctxt);
		spin_unlock(&drawctxt->lock);

		ret = wait_event_interruptible_timeout(drawctxt->wq,
			_check_context_queue(drawctxt, count),
			msecs_to_jiffies(_context_queue_wait));

		spin_lock(&drawctxt->lock);
		trace_adreno_drawctxt_wake(drawctxt);

		/*
		 * Account for the possibility that the context got invalidated
		 * while we were sleeping
		 */
		if (ret > 0)
			ret = kgsl_check_context_state(&drawctxt->base);
		else if (ret == 0)
			ret = -ETIMEDOUT;
	}

	return ret;
}

static unsigned int _check_context_state_to_queue_cmds(
	struct adreno_context *drawctxt, u32 count)
{
	int ret = kgsl_check_context_state(&drawctxt->base);

	if (ret)
		return ret;

	return _wait_for_room_in_context_queue(drawctxt, count);
}

static void _queue_drawobj(struct adreno_context *drawctxt,
	struct kgsl_drawobj *drawobj)
{
	struct kgsl_context *context = drawobj->context;

	/* Put the command into the queue */
	drawctxt->drawqueue[drawctxt->drawqueue_tail] = drawobj;
	drawctxt->drawqueue_tail = (drawctxt->drawqueue_tail + 1) %
			ADRENO_CONTEXT_DRAWQUEUE_SIZE;
	drawctxt->queued++;
	msm_perf_events_update(MSM_PERF_GFX, MSM_PERF_QUEUE,
				pid_nr(context->proc_priv->pid),
				context->id, drawobj->timestamp,
				!!(drawobj->flags & KGSL_DRAWOBJ_END_OF_FRAME));
	trace_adreno_cmdbatch_queued(drawobj, drawctxt->queued);
}

static int drawctxt_queue_auxobj(struct adreno_device *adreno_dev,
		struct adreno_context *drawctxt, struct kgsl_drawobj *drawobj,
		u32 *timestamp, u32 user_ts)
{
	int ret;

	ret = get_timestamp(drawctxt, drawobj, timestamp, user_ts);
	if (ret)
		return ret;

	drawctxt->queued_timestamp = *timestamp;
	_queue_drawobj(drawctxt, drawobj);

	return 0;
}

static int drawctxt_queue_markerobj(struct adreno_device *adreno_dev,
	struct adreno_context *drawctxt, struct kgsl_drawobj *drawobj,
	uint32_t *timestamp, unsigned int user_ts)
{
	struct kgsl_drawobj_cmd *markerobj = CMDOBJ(drawobj);
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

static int drawctxt_queue_cmdobj(struct adreno_device *adreno_dev,
	struct adreno_context *drawctxt, struct kgsl_drawobj *drawobj,
	uint32_t *timestamp, unsigned int user_ts)
{
	struct kgsl_drawobj_cmd *cmdobj = CMDOBJ(drawobj);
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

static void drawctxt_queue_syncobj(struct adreno_context *drawctxt,
	struct kgsl_drawobj *drawobj, uint32_t *timestamp)
{
	*timestamp = 0;
	drawobj->timestamp = 0;

	_queue_drawobj(drawctxt, drawobj);
}

/*
 * Queue a command in the context - if there isn't any room in the queue, then
 * block until there is
 */
static int adreno_dispatcher_queue_cmds(struct kgsl_device_private *dev_priv,
		struct kgsl_context *context, struct kgsl_drawobj *drawobj[],
		uint32_t count, uint32_t *timestamp)

{
	struct kgsl_device *device = dev_priv->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_context *drawctxt = ADRENO_CONTEXT(context);
	struct adreno_dispatcher_drawqueue *dispatch_q;
	struct adreno_dispatch_job *job;
	int ret;
	unsigned int i, user_ts;

	/*
	 * There is always a possibility that dispatcher may end up pushing
	 * the last popped draw object back to the context drawqueue. Hence,
	 * we can only queue up to _context_drawqueue_size - 1 here to make
	 * sure we never let drawqueue->queued exceed _context_drawqueue_size.
	 */
	if (!count || count > _context_drawqueue_size - 1)
		return -EINVAL;

	ret = kgsl_check_context_state(&drawctxt->base);
	if (ret)
		return ret;

	ret = adreno_verify_cmdobj(dev_priv, context, drawobj, count);
	if (ret)
		return ret;

	/* wait for the suspend gate */
	wait_for_completion(&device->halt_gate);

	job = kmem_cache_alloc(jobs_cache, GFP_KERNEL);
	if (!job)
		return -ENOMEM;

	job->drawctxt = drawctxt;

	spin_lock(&drawctxt->lock);

	ret = _check_context_state_to_queue_cmds(drawctxt, count);
	if (ret) {
		spin_unlock(&drawctxt->lock);
		kmem_cache_free(jobs_cache, job);
		return ret;
	}

	user_ts = *timestamp;

	/*
	 * If there is only one drawobj in the array and it is of
	 * type SYNCOBJ_TYPE, skip comparing user_ts as it can be 0
	 */
	if (!(count == 1 && drawobj[0]->type == SYNCOBJ_TYPE) &&
		(drawctxt->base.flags & KGSL_CONTEXT_USER_GENERATED_TS)) {
		/*
		 * User specified timestamps need to be greater than the last
		 * issued timestamp in the context
		 */
		if (timestamp_cmp(drawctxt->timestamp, user_ts) >= 0) {
			spin_unlock(&drawctxt->lock);
			kmem_cache_free(jobs_cache, job);
			return -ERANGE;
		}
	}

	for (i = 0; i < count; i++) {

		switch (drawobj[i]->type) {
		case MARKEROBJ_TYPE:
			ret = drawctxt_queue_markerobj(adreno_dev, drawctxt,
				drawobj[i], timestamp, user_ts);
			if (ret) {
				spin_unlock(&drawctxt->lock);
				kmem_cache_free(jobs_cache, job);
			}

			if (ret == 1)
				goto done;
			else if (ret)
				return ret;
			break;
		case CMDOBJ_TYPE:
			ret = drawctxt_queue_cmdobj(adreno_dev, drawctxt,
				drawobj[i], timestamp, user_ts);
			if (ret) {
				spin_unlock(&drawctxt->lock);
				kmem_cache_free(jobs_cache, job);
				return ret;
			}
			break;
		case SYNCOBJ_TYPE:
			drawctxt_queue_syncobj(drawctxt, drawobj[i], timestamp);
			break;
		case BINDOBJ_TYPE:
		case TIMELINEOBJ_TYPE:
			ret = drawctxt_queue_auxobj(adreno_dev,
				drawctxt, drawobj[i], timestamp, user_ts);
			if (ret) {
				spin_unlock(&drawctxt->lock);
				kmem_cache_free(jobs_cache, job);
				return ret;
			}
			break;
		default:
			spin_unlock(&drawctxt->lock);
			kmem_cache_free(jobs_cache, job);
			return -EINVAL;
		}

	}

	dispatch_q = &(ADRENO_CONTEXT(drawobj[0]->context)->rb->dispatch_q);

	_track_context(adreno_dev, dispatch_q, drawctxt);

	spin_unlock(&drawctxt->lock);

	/* Add the context to the dispatcher pending list */
	if (_kgsl_context_get(&drawctxt->base)) {
		trace_dispatch_queue_context(drawctxt);
		llist_add(&job->node,
			&adreno_dev->dispatcher.jobs[drawctxt->base.priority]);
	} else {
		kmem_cache_free(jobs_cache, job);
		goto done;
	}

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

		if (kgsl_context_is_bad(drawobj->context)) {
			replay[i] = NULL;

			mutex_lock(&device->mutex);
			kgsl_cancel_events_timestamp(device,
				&drawobj->context->events, drawobj->timestamp);
			mutex_unlock(&device->mutex);

			kgsl_drawobj_destroy(drawobj);
		}
	}
}

#define pr_fault(_d, _c, fmt, args...) \
		pr_context(_d, (_c)->context, fmt, ##args)

static void adreno_fault_header(struct kgsl_device *device,
		struct adreno_ringbuffer *rb, struct kgsl_drawobj_cmd *cmdobj,
		int fault)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_drawobj *drawobj = DRAWOBJ(cmdobj);
	struct adreno_context *drawctxt =
			drawobj ? ADRENO_CONTEXT(drawobj->context) : NULL;
	unsigned int status, rptr, wptr, ib1sz, ib2sz;
	uint64_t ib1base, ib2base;
	bool gx_on = adreno_gx_is_on(adreno_dev);
	int id = (rb != NULL) ? rb->id : -1;
	const char *type = fault & ADRENO_GMU_FAULT ? "gmu" : "gpu";

	if (!gx_on) {
		if (drawobj != NULL) {
			pr_fault(device, drawobj,
				"%s fault ctx %u ctx_type %s ts %u and GX is OFF\n",
				type, drawobj->context->id,
				kgsl_context_type(drawctxt->type),
				drawobj->timestamp);
			pr_fault(device, drawobj, "cmdline: %s\n",
					drawctxt->base.proc_priv->cmdline);
		} else
			dev_err(device->dev, "RB[%d] : %s fault and GX is OFF\n",
				id, type);

		return;
	}

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
		drawctxt->base.total_fault_count++;
		drawctxt->base.last_faulted_cmd_ts = drawobj->timestamp;

		trace_adreno_gpu_fault(drawobj->context->id,
			drawobj->timestamp,
			status, rptr, wptr, ib1base, ib1sz,
			ib2base, ib2sz, drawctxt->rb->id);

		pr_fault(device, drawobj,
			"%s fault ctx %u ctx_type %s ts %u status %8.8X rb %4.4x/%4.4x ib1 %16.16llX/%4.4x ib2 %16.16llX/%4.4x\n",
			type, drawobj->context->id,
			kgsl_context_type(drawctxt->type),
			drawobj->timestamp, status,
			rptr, wptr, ib1base, ib1sz, ib2base, ib2sz);

		pr_fault(device, drawobj, "cmdline: %s\n",
				drawctxt->base.proc_priv->cmdline);

		if (rb != NULL)
			pr_fault(device, drawobj,
				"%s fault rb %d rb sw r/w %4.4x/%4.4x\n",
				type, rb->id, rptr, rb->wptr);
	} else {
		dev_err(device->dev,
			"RB[%d] : %s fault status %8.8X rb %4.4x/%4.4x ib1 %16.16llX/%4.4x ib2 %16.16llX/%4.4x\n",
			id, type, status, rptr, wptr, ib1base, ib1sz, ib2base,
			ib2sz);
		if (rb != NULL)
			dev_err(device->dev,
				"RB[%d] : %s fault rb sw r/w %4.4x/%4.4x\n",
				rb->id, type, rptr, rb->wptr);
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
		if (ktime_ms_delta(ktime_get(), drawobj->context->fault_time) >
				_fault_throttle_time) {
			drawobj->context->fault_time = ktime_get();
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

	pr_context(device, drawobj->context, "gpu %s ctx %d ts %u\n",
		state, drawobj->context->id, drawobj->timestamp);

	/* Mark the context as failed and invalidate it */
	adreno_drawctxt_set_guilty(device, drawobj->context);
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

			adreno_drawctxt_set_guilty(device, drawobj->context);
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
				"gpu reset failed ctx %u ts %u\n",
				replay[i]->base.context->id,
				replay[i]->base.timestamp);

			/* Mark this context as guilty (failed recovery) */
			adreno_drawctxt_set_guilty(device, replay[i]->base.context);
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
		adreno_fault_header(device, rb, NULL, fault);

		/* GMU snapshot will also pull a full device snapshot */
		if (fault & ADRENO_GMU_FAULT)
			gmu_core_fault_snapshot(device);
		else
			kgsl_device_snapshot(device, NULL, NULL, false);
		return;
	}

	/* Skip everything if the PMDUMP flag is set */
	if (test_bit(KGSL_FT_SKIP_PMDUMP, &cmdobj->fault_policy))
		return;

	/* Print the fault header */
	adreno_fault_header(device, rb, cmdobj, fault);

	if (!(drawobj->context->flags & KGSL_CONTEXT_NO_SNAPSHOT))
		kgsl_device_snapshot(device, drawobj->context, NULL,
					fault & ADRENO_GMU_FAULT);
}

static int dispatcher_do_fault(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
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
	bool gx_on;

	fault = atomic_xchg(&dispatcher->fault, 0);
	if (fault == 0)
		return 0;

	mutex_lock(&device->mutex);

	/*
	 * In the very unlikely case that the power is off, do nothing - the
	 * state will be reset on power up and everybody will be happy
	 */
	if (!kgsl_state_is_awake(device)) {
		mutex_unlock(&device->mutex);
		return 0;
	}

	/* Mask all GMU interrupts */
	if (gmu_core_isenabled(device)) {
		adreno_write_gmureg(adreno_dev,
			ADRENO_REG_GMU_AO_HOST_INTERRUPT_MASK,
			0xFFFFFFFF);
		adreno_write_gmureg(adreno_dev,
			ADRENO_REG_GMU_GMU2HOST_INTR_MASK,
			0xFFFFFFFF);
	}

	gx_on = adreno_gx_is_on(adreno_dev);

	/*
	 * On non-A3xx, read RBBM_STATUS3:SMMU_STALLED_ON_FAULT (BIT 24)
	 * to tell if this function was entered after a pagefault. If so, only
	 * proceed if the fault handler has already run in the IRQ thread,
	 * else return early to give the fault handler a chance to run.
	 */
	if (!(fault & ADRENO_IOMMU_PAGE_FAULT) &&
		!adreno_is_a3xx(adreno_dev) && gx_on) {
		unsigned int val;

		adreno_readreg(adreno_dev, ADRENO_REG_RBBM_STATUS3, &val);
		if (val & BIT(24)) {
			mutex_unlock(&device->mutex);
			dev_err(device->dev,
				"SMMU is stalled without a pagefault\n");
			return -EBUSY;
		}
	}

	/* Turn off all the timers */
	del_timer_sync(&dispatcher->timer);

	adreno_dispatcher_stop_fault_timer(device);

	/*
	 * Deleting uninitialized timer will block for ever on kernel debug
	 * disable build. Hence skip del timer if it is not initialized.
	 */
	if (adreno_is_preemption_enabled(adreno_dev))
		del_timer_sync(&adreno_dev->preempt.timer);

	if (gx_on)
		adreno_readreg64(adreno_dev, ADRENO_REG_CP_RB_BASE,
			ADRENO_REG_CP_RB_BASE_HI, &base);

	/*
	 * Force the CP off for anything but a hard fault to make sure it is
	 * good and stopped
	 */
	if (!(fault & ADRENO_HARD_FAULT) && gx_on) {
		adreno_readreg(adreno_dev, ADRENO_REG_CP_ME_CNTL, &reg);
		if (!adreno_is_a3xx(adreno_dev))
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
		if (base == rb->buffer_desc->gpuaddr) {
			dispatch_q = &(rb->dispatch_q);
			hung_rb = rb;
			if (adreno_dev->cur_rb != hung_rb) {
				adreno_dev->prev_rb = adreno_dev->cur_rb;
				adreno_dev->cur_rb = hung_rb;
			}
		}
	}

	if (dispatch_q && !adreno_drawqueue_is_empty(dispatch_q)) {
		cmdobj = dispatch_q->cmd_q[dispatch_q->head];
		trace_adreno_cmdbatch_fault(cmdobj, fault);
	}

	if (gx_on)
		adreno_readreg64(adreno_dev, ADRENO_REG_CP_IB1_BASE,
			ADRENO_REG_CP_IB1_BASE_HI, &base);

	if (!test_bit(KGSL_FT_PAGEFAULT_GPUHALT_ENABLE, &device->mmu.pfpolicy)
		&& adreno_dev->cooperative_reset)
		gmu_core_dev_cooperative_reset(device);

	if (!(fault & ADRENO_GMU_FAULT_SKIP_SNAPSHOT))
		do_header_and_snapshot(device, fault, hung_rb, cmdobj);

	/* Turn off the KEEPALIVE vote from the ISR for hard fault */
	if (gpudev->gpu_keepalive && fault & ADRENO_HARD_FAULT)
		gpudev->gpu_keepalive(adreno_dev, false);

	/* Terminate the stalled transaction and resume the IOMMU */
	if (fault & ADRENO_IOMMU_PAGE_FAULT)
		kgsl_mmu_pagefault_resume(&device->mmu, true);

	/* Reset the dispatcher queue */
	dispatcher->inflight = 0;

	/* Remove the bus hint */
	device->pwrctrl.rt_bus_hint_active = false;

	/* Reset the GPU and make sure halt is not set during recovery */
	halt = adreno_gpu_halt(adreno_dev);
	adreno_clear_gpu_halt(adreno_dev);

	/*
	 * If there is a stall in the ringbuffer after all commands have been
	 * retired then we could hit problems if contexts are waiting for
	 * internal timestamps that will never retire
	 */

	if (hung_rb != NULL) {
		kgsl_sharedmem_writel(device->memstore,
			MEMSTORE_RB_OFFSET(hung_rb, soptimestamp),
			hung_rb->timestamp);

		kgsl_sharedmem_writel(device->memstore,
				MEMSTORE_RB_OFFSET(hung_rb, eoptimestamp),
				hung_rb->timestamp);

		/* Schedule any pending events to be run */
		kgsl_process_event_group(device, &hung_rb->events);
	}

	ret = adreno_reset(device, fault);

	mutex_unlock(&device->mutex);

	/* If adreno_reset() fails then what hope do we have for the future? */
	BUG_ON(ret);

	/* if any other fault got in until reset then ignore */
	atomic_set(&dispatcher->fault, 0);

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

static const char *_ft_type(enum kgsl_ft_policy_bits nr)
{
	if (nr == KGSL_FT_OFF)
		return "off";
	else if (nr == KGSL_FT_REPLAY)
		return "replay";
	else if (nr == KGSL_FT_SKIPIB)
		return "skipib";
	else if (nr == KGSL_FT_SKIPFRAME)
		return "skipfame";
	else if (nr == KGSL_FT_DISABLE)
		return "disable";
	else if (nr == KGSL_FT_TEMP_DISABLE)
		return "temp";
	else if (nr == KGSL_FT_THROTTLE)
		return "throttle";
	else if (nr == KGSL_FT_SKIPCMD)
		return "skipcmd";

	return "";
}

static void _print_recovery(struct kgsl_device *device,
		struct kgsl_drawobj_cmd *cmdobj)
{
	int nr = find_first_bit(&cmdobj->fault_recovery, BITS_PER_LONG);
	struct kgsl_drawobj *drawobj = DRAWOBJ(cmdobj);

	pr_context(device, drawobj->context,
		"gpu %s ctx %u ts %u policy %lX\n",
		_ft_type(nr), drawobj->context->id, drawobj->timestamp,
		cmdobj->fault_recovery);
}

static void cmdobj_profile_ticks(struct adreno_device *adreno_dev,
	struct kgsl_drawobj_cmd *cmdobj, uint64_t *start, uint64_t *retire,
	uint64_t *active)
{
	void *ptr = adreno_dev->profile_buffer->hostptr;
	struct adreno_drawobj_profile_entry *entry;

	entry = (struct adreno_drawobj_profile_entry *)
		(ptr + (cmdobj->profile_index * sizeof(*entry)));

	/* get updated values of started and retired */
	rmb();
	*start = entry->started;
	*retire = entry->retired;
	if (ADRENO_GPUREV(adreno_dev) < 600)
		*active = entry->retired - entry->started;
	else
		*active = entry->ctx_end - entry->ctx_start;
}

static void retire_cmdobj(struct adreno_device *adreno_dev,
		struct kgsl_drawobj_cmd *cmdobj)
{
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;
	struct kgsl_drawobj *drawobj = DRAWOBJ(cmdobj);
	struct adreno_context *drawctxt = ADRENO_CONTEXT(drawobj->context);
	struct adreno_ringbuffer *rb = drawctxt->rb;
	struct kgsl_context *context = drawobj->context;
	uint64_t start = 0, end = 0, active = 0;
	struct retire_info info = {0};

	if (cmdobj->fault_recovery != 0) {
		set_bit(ADRENO_CONTEXT_FAULT, &drawobj->context->priv);
		_print_recovery(KGSL_DEVICE(adreno_dev), cmdobj);
	}

	if (test_bit(CMDOBJ_PROFILE, &cmdobj->priv))
		cmdobj_profile_ticks(adreno_dev, cmdobj, &start, &end, &active);

	info.inflight = (int)dispatcher->inflight;
	info.rb_id = rb->id;
	info.wptr = rb->wptr;
	info.timestamp = drawobj->timestamp;
	info.sop = start;
	info.eop = end;
	info.active = active;

	/* protected GPU work must not be reported */
	if  (!(context->flags & KGSL_CONTEXT_SECURE))
		kgsl_work_period_update(KGSL_DEVICE(adreno_dev),
					     context->proc_priv->period, active);

	msm_perf_events_update(MSM_PERF_GFX, MSM_PERF_RETIRED,
			       pid_nr(context->proc_priv->pid),
			       context->id, drawobj->timestamp,
			       !!(drawobj->flags & KGSL_DRAWOBJ_END_OF_FRAME));

	if (drawobj->flags & KGSL_DRAWOBJ_END_OF_FRAME) {
		atomic64_inc(&context->proc_priv->frame_count);
		atomic_inc(&context->proc_priv->period->frames);
	}

	/*
	 * For A3xx we still get the rptr from the CP_RB_RPTR instead of
	 * rptr scratch out address. At this point GPU clocks turned off.
	 * So avoid reading GPU register directly for A3xx.
	 */
	if (adreno_is_a3xx(adreno_dev)) {
		trace_adreno_cmdbatch_retired(drawobj->context, &info,
			drawobj->flags, rb->dispatch_q.inflight,
			cmdobj->fault_recovery);
	} else {
		info.rptr = adreno_get_rptr(rb);
		trace_adreno_cmdbatch_retired(drawobj->context, &info,
			drawobj->flags, rb->dispatch_q.inflight,
			cmdobj->fault_recovery);
	}

	log_kgsl_cmdbatch_retired_event(context->id, drawobj->timestamp,
		context->priority, drawobj->flags, start, end);

	drawctxt->submit_retire_ticks[drawctxt->ticks_index] =
		end - cmdobj->submit_ticks;

	drawctxt->ticks_index = (drawctxt->ticks_index + 1) %
		SUBMIT_RETIRE_TICKS_SIZE;

	trace_adreno_cmdbatch_done(drawobj->context->id,
		drawobj->context->priority, drawobj->timestamp);
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

	pr_context(device, drawobj->context, "gpu timeout ctx %u ts %u\n",
		drawobj->context->id, drawobj->timestamp);

	adreno_set_gpu_fault(adreno_dev, ADRENO_TIMEOUT_FAULT);

	/*
	 * This makes sure dispatcher doesn't run endlessly in cases where
	 * we couldn't run recovery
	 */
	drawqueue->expires = jiffies + msecs_to_jiffies(adreno_drawobj_timeout);
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

/* Take down the dispatcher and release any power states */
static void _dispatcher_power_down(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;

	mutex_lock(&device->mutex);

	if (test_and_clear_bit(ADRENO_DISPATCHER_ACTIVE, &dispatcher->priv))
		complete_all(&dispatcher->idle_gate);

	adreno_dispatcher_stop_fault_timer(device);
	process_rt_bus_hint(device, false);

	if (test_bit(ADRENO_DISPATCHER_POWER, &dispatcher->priv)) {
		adreno_active_count_put(adreno_dev);
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
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
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

	kthread_queue_work(dispatcher->worker, &dispatcher->work);
}

/*
 * Put a draw context on the dispatcher pending queue and schedule the
 * dispatcher. This is used to reschedule changes that might have been blocked
 * for sync points or other concerns
 */
static void adreno_dispatcher_queue_context(struct adreno_device *adreno_dev,
	struct adreno_context *drawctxt)
{
	dispatcher_queue_context(adreno_dev, drawctxt);
	adreno_dispatcher_schedule(KGSL_DEVICE(adreno_dev));
}

void adreno_dispatcher_fault(struct adreno_device *adreno_dev,
		u32 fault)
{
	adreno_set_gpu_fault(adreno_dev, fault);
	adreno_dispatcher_schedule(KGSL_DEVICE(adreno_dev));
}

/*
 * This is called when the timer expires - it either means the GPU is hung or
 * the IB is taking too long to execute
 */
static void adreno_dispatcher_timer(struct timer_list *t)
{
	struct adreno_dispatcher *dispatcher = from_timer(dispatcher, t, timer);
	struct adreno_device *adreno_dev = container_of(dispatcher,
					struct adreno_device, dispatcher);

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

	adreno_dispatcher_stop_fault_timer(KGSL_DEVICE(adreno_dev));
}

/* Return the ringbuffer that matches the draw context priority */
static struct adreno_ringbuffer *dispatch_get_rb(struct adreno_device *adreno_dev,
		struct adreno_context *drawctxt)
{
	int level;

	/* If preemption is disabled everybody goes on the same ringbuffer */
	if (!adreno_is_preemption_enabled(adreno_dev))
		return &adreno_dev->ringbuffers[0];

	/*
	 * Math to convert the priority field in context structure to an RB ID.
	 * Divide up the context priority based on number of ringbuffer levels.
	 */
	level = min_t(int, drawctxt->base.priority / adreno_dev->num_ringbuffers,
		adreno_dev->num_ringbuffers - 1);

	return &adreno_dev->ringbuffers[level];
}

static void adreno_dispatcher_setup_context(struct adreno_device *adreno_dev,
		struct adreno_context *drawctxt)
{
	drawctxt->rb = dispatch_get_rb(adreno_dev, drawctxt);
}

static int _skipsaverestore_store(struct adreno_device *adreno_dev, bool val)
{
	adreno_dev->preempt.skipsaverestore = val ? true : false;
	return 0;
}

static bool _skipsaverestore_show(struct adreno_device *adreno_dev)
{
	return adreno_dev->preempt.skipsaverestore;
}

static int _usesgmem_store(struct adreno_device *adreno_dev, bool val)
{
	adreno_dev->preempt.usesgmem = val ? true : false;
	return 0;
}

static bool _usesgmem_show(struct adreno_device *adreno_dev)
{
	return adreno_dev->preempt.usesgmem;
}

static int _preempt_level_store(struct adreno_device *adreno_dev,
		unsigned int val)
{
	adreno_dev->preempt.preempt_level = min_t(unsigned int, val, 2);
	return 0;
}

static unsigned int _preempt_level_show(struct adreno_device *adreno_dev)
{
	return adreno_dev->preempt.preempt_level;
}

static void change_preemption(struct adreno_device *adreno_dev, void *priv)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_context *context;
	struct adreno_context *drawctxt;
	struct adreno_ringbuffer *rb;
	int id, i, ret;

	/* Make sure all ringbuffers are finished */
	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		ret = adreno_ringbuffer_waittimestamp(rb, rb->timestamp,
			2 * 1000);
		if (ret) {
			dev_err(device->dev,
				"Cannot disable preemption because couldn't idle ringbuffer[%d] ret: %d\n",
				rb->id, ret);
			return;
		}
	}

	change_bit(ADRENO_DEVICE_PREEMPTION, &adreno_dev->priv);
	adreno_dev->cur_rb = &adreno_dev->ringbuffers[0];
	adreno_dev->next_rb = NULL;
	adreno_dev->prev_rb = NULL;

	/* Update the ringbuffer for each draw context */
	write_lock(&device->context_lock);
	idr_for_each_entry(&device->context_idr, context, id) {
		drawctxt = ADRENO_CONTEXT(context);
		drawctxt->rb = dispatch_get_rb(adreno_dev, drawctxt);

		/*
		 * Make sure context destroy checks against the correct
		 * ringbuffer's timestamp.
		 */
		adreno_rb_readtimestamp(adreno_dev, drawctxt->rb,
			KGSL_TIMESTAMP_RETIRED, &drawctxt->internal_timestamp);
	}
	write_unlock(&device->context_lock);
}

static int _preemption_store(struct adreno_device *adreno_dev, bool val)
{
	if (!(ADRENO_FEATURE(adreno_dev, ADRENO_PREEMPTION)) ||
		(test_bit(ADRENO_DEVICE_PREEMPTION,
		&adreno_dev->priv) == val))
		return 0;

	return adreno_power_cycle(adreno_dev, change_preemption, NULL);
}

static bool _preemption_show(struct adreno_device *adreno_dev)
{
	return adreno_is_preemption_enabled(adreno_dev);
}

static unsigned int _preempt_count_show(struct adreno_device *adreno_dev)
{
	return adreno_dev->preempt.count;
}

static int _ft_long_ib_detect_store(struct adreno_device *adreno_dev, bool val)
{
	adreno_dev->long_ib_detect = val ? true : false;
	return 0;
}

static bool _ft_long_ib_detect_show(struct adreno_device *adreno_dev)
{
	return adreno_dev->long_ib_detect;
}

static ADRENO_SYSFS_BOOL(preemption);
static ADRENO_SYSFS_U32(preempt_level);
static ADRENO_SYSFS_BOOL(usesgmem);
static ADRENO_SYSFS_BOOL(skipsaverestore);
static ADRENO_SYSFS_RO_U32(preempt_count);
static ADRENO_SYSFS_BOOL(ft_long_ib_detect);

static const struct attribute *_dispatch_attr_list[] = {
	&adreno_attr_preemption.attr.attr,
	&adreno_attr_preempt_level.attr.attr,
	&adreno_attr_usesgmem.attr.attr,
	&adreno_attr_skipsaverestore.attr.attr,
	&adreno_attr_preempt_count.attr.attr,
	&adreno_attr_ft_long_ib_detect.attr.attr,
	NULL,
};

static void adreno_dispatcher_close(struct adreno_device *adreno_dev)
{
	struct adreno_dispatcher *dispatcher = &adreno_dev->dispatcher;
	int i;
	struct adreno_ringbuffer *rb;

	mutex_lock(&dispatcher->mutex);
	del_timer_sync(&dispatcher->timer);

	adreno_dispatcher_stop_fault_timer(KGSL_DEVICE(adreno_dev));

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

	kthread_destroy_worker(dispatcher->worker);

	adreno_set_dispatch_ops(adreno_dev, NULL);

	kobject_put(&dispatcher->kobj);

	kmem_cache_destroy(jobs_cache);

	clear_bit(ADRENO_DISPATCHER_INIT, &dispatcher->priv);
}

struct dispatcher_attribute {
	struct attribute attr;
	ssize_t (*show)(struct adreno_dispatcher *dispatcher,
			struct dispatcher_attribute *attr, char *buf);
	ssize_t (*store)(struct adreno_dispatcher *dispatcher,
			struct dispatcher_attribute *attr, const char *buf,
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

	ret = kstrtou32(buf, 0, &val);
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
	return scnprintf(buf, PAGE_SIZE, "%u\n",
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
	NULL,
};

ATTRIBUTE_GROUPS(dispatcher);

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
	.default_groups = dispatcher_groups,
};

static const struct adreno_dispatch_ops swsched_ops = {
	.close = adreno_dispatcher_close,
	.queue_cmds = adreno_dispatcher_queue_cmds,
	.setup_context = adreno_dispatcher_setup_context,
	.queue_context = adreno_dispatcher_queue_context,
	.fault = adreno_dispatcher_fault,
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
	int ret, i;

	if (test_bit(ADRENO_DISPATCHER_INIT, &dispatcher->priv))
		return 0;

	ret = kobject_init_and_add(&dispatcher->kobj, &ktype_dispatcher,
		&device->dev->kobj, "dispatch");
	if (ret)
		return ret;

	dispatcher->worker = kthread_create_worker(0, "kgsl_dispatcher");
	if (IS_ERR(dispatcher->worker)) {
		kobject_put(&dispatcher->kobj);
		return PTR_ERR(dispatcher->worker);
	}

	WARN_ON(sysfs_create_files(&device->dev->kobj, _dispatch_attr_list));

	mutex_init(&dispatcher->mutex);

	timer_setup(&dispatcher->timer, adreno_dispatcher_timer, 0);

	kthread_init_work(&dispatcher->work, adreno_dispatcher_work);

	init_completion(&dispatcher->idle_gate);
	complete_all(&dispatcher->idle_gate);

	jobs_cache = KMEM_CACHE(adreno_dispatch_job, 0);

	for (i = 0; i < ARRAY_SIZE(dispatcher->jobs); i++) {
		init_llist_head(&dispatcher->jobs[i]);
		init_llist_head(&dispatcher->requeue[i]);
	}

	adreno_set_dispatch_ops(adreno_dev, &swsched_ops);

	sched_set_fifo(dispatcher->worker->task);

	set_bit(ADRENO_DISPATCHER_INIT, &dispatcher->priv);

	return 0;
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

	if (device->state != KGSL_STATE_ACTIVE)
		return 0;

	/*
	 * Ensure that this function is not called when dispatcher
	 * mutex is held and device is started
	 */

	if (WARN_ON(mutex_is_locked(&dispatcher->mutex)))
		return -EDEADLK;

	adreno_get_gpu_halt(adreno_dev);

	mutex_unlock(&device->mutex);

	/*
	 * Flush the worker to make sure all executing
	 * or pending dispatcher works on worker are
	 * finished
	 */
	kthread_flush_worker(dispatcher->worker);

	ret = wait_for_completion_timeout(&dispatcher->idle_gate,
			msecs_to_jiffies(ADRENO_IDLE_TIMEOUT));
	if (ret == 0) {
		ret = -ETIMEDOUT;
		WARN(1, "Dispatcher halt timeout\n");
	} else if (ret < 0) {
		dev_err(device->dev, "Dispatcher halt failed %d\n", ret);
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
