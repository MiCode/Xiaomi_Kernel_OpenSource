// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2002,2007-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/debugfs.h>

#include "adreno.h"
#include "adreno_iommu.h"
#include "adreno_trace.h"

static void wait_callback(struct kgsl_device *device,
		struct kgsl_event_group *group, void *priv, int result)
{
	struct adreno_context *drawctxt = priv;

	wake_up_all(&drawctxt->waiting);
}

static int _check_context_timestamp(struct kgsl_device *device,
		struct kgsl_context *context, unsigned int timestamp)
{
	/* Bail if the drawctxt has been invalidated or destroyed */
	if (kgsl_context_detached(context) || kgsl_context_invalid(context))
		return 1;

	return kgsl_check_timestamp(device, context, timestamp);
}

/**
 * adreno_drawctxt_dump() - dump information about a draw context
 * @device: KGSL device that owns the context
 * @context: KGSL context to dump information about
 *
 * Dump specific information about the context to the kernel log.  Used for
 * fence timeout callbacks
 */
void adreno_drawctxt_dump(struct kgsl_device *device,
		struct kgsl_context *context)
{
	unsigned int queue, start, retire;
	struct adreno_context *drawctxt = ADRENO_CONTEXT(context);
	int index, pos;
	char buf[120];

	kgsl_readtimestamp(device, context, KGSL_TIMESTAMP_QUEUED, &queue);
	kgsl_readtimestamp(device, context, KGSL_TIMESTAMP_CONSUMED, &start);
	kgsl_readtimestamp(device, context, KGSL_TIMESTAMP_RETIRED, &retire);

	/*
	 * We may have kgsl sync obj timer running, which also uses same
	 * lock, take a lock with software interrupt disabled (bh)
	 * to avoid spin lock recursion.
	 *
	 * Use Spin trylock because dispatcher can acquire drawctxt->lock
	 * if context is pending and the fence it is waiting on just got
	 * signalled. Dispatcher acquires drawctxt->lock and tries to
	 * delete the sync obj timer using del_timer_sync().
	 * del_timer_sync() waits till timer and its pending handlers
	 * are deleted. But if the timer expires at the same time,
	 * timer handler could be waiting on drawctxt->lock leading to a
	 * deadlock. To prevent this use spin_trylock_bh.
	 */
	if (!spin_trylock_bh(&drawctxt->lock)) {
		dev_err(device->dev, "  context[%u]: could not get lock\n",
			context->id);
		return;
	}

	dev_err(device->dev,
		"  context[%u]: queue=%u, submit=%u, start=%u, retire=%u\n",
		context->id, queue, drawctxt->submitted_timestamp,
		start, retire);

	if (drawctxt->drawqueue_head != drawctxt->drawqueue_tail) {
		struct kgsl_drawobj *drawobj =
			drawctxt->drawqueue[drawctxt->drawqueue_head];

		if (test_bit(ADRENO_CONTEXT_FENCE_LOG, &context->priv)) {
			dev_err(device->dev,
				"  possible deadlock. Context %u might be blocked for itself\n",
				context->id);
			goto stats;
		}

		if (!kref_get_unless_zero(&drawobj->refcount))
			goto stats;

		if (drawobj->type == SYNCOBJ_TYPE) {
			struct kgsl_drawobj_sync *syncobj = SYNCOBJ(drawobj);

			if (kgsl_drawobj_events_pending(syncobj)) {
				dev_err(device->dev,
					"  context[%u] (ts=%u) Active sync points:\n",
					context->id, drawobj->timestamp);

				kgsl_dump_syncpoints(device, syncobj);
			}
		}

		kgsl_drawobj_put(drawobj);
	}

stats:
	memset(buf, 0, sizeof(buf));

	pos = 0;

	for (index = 0; index < SUBMIT_RETIRE_TICKS_SIZE; index++) {
		uint64_t msecs;
		unsigned int usecs;

		if (!drawctxt->submit_retire_ticks[index])
			continue;
		msecs = drawctxt->submit_retire_ticks[index] * 10;
		usecs = do_div(msecs, 192);
		usecs = do_div(msecs, 1000);
		pos += scnprintf(buf + pos, sizeof(buf) - pos, "%u.%0u ",
			(unsigned int)msecs, usecs);
	}
	dev_err(device->dev, "  context[%u]: submit times: %s\n",
		context->id, buf);

	spin_unlock_bh(&drawctxt->lock);
}

/**
 * adreno_drawctxt_wait() - sleep until a timestamp expires
 * @adreno_dev: pointer to the adreno_device struct
 * @drawctxt: Pointer to the draw context to sleep for
 * @timetamp: Timestamp to wait on
 * @timeout: Number of jiffies to wait (0 for infinite)
 *
 * Register an event to wait for a timestamp on a context and sleep until it
 * has past.  Returns < 0 on error, -ETIMEDOUT if the timeout expires or 0
 * on success
 */
int adreno_drawctxt_wait(struct adreno_device *adreno_dev,
		struct kgsl_context *context,
		uint32_t timestamp, unsigned int timeout)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_context *drawctxt = ADRENO_CONTEXT(context);
	int ret;
	long ret_temp;

	if (kgsl_context_detached(context))
		return -ENOENT;

	if (kgsl_context_invalid(context))
		return -EDEADLK;

	trace_adreno_drawctxt_wait_start(-1, context->id, timestamp);

	ret = kgsl_add_event(device, &context->events, timestamp,
		wait_callback, (void *) drawctxt);
	if (ret)
		goto done;

	/*
	 * If timeout is 0, wait forever. msecs_to_jiffies will force
	 * values larger than INT_MAX to an infinite timeout.
	 */
	if (timeout == 0)
		timeout = UINT_MAX;

	ret_temp = wait_event_interruptible_timeout(drawctxt->waiting,
			_check_context_timestamp(device, context, timestamp),
			msecs_to_jiffies(timeout));

	if (ret_temp == 0) {
		ret = -ETIMEDOUT;
		goto done;
	} else if (ret_temp < 0) {
		ret = (int) ret_temp;
		goto done;
	}
	ret = 0;

	/* -EDEADLK if the context was invalidated while we were waiting */
	if (kgsl_context_invalid(context))
		ret = -EDEADLK;


	/* Return -EINVAL if the context was detached while we were waiting */
	if (kgsl_context_detached(context))
		ret = -ENOENT;

done:
	trace_adreno_drawctxt_wait_done(-1, context->id, timestamp, ret);
	return ret;
}

/**
 * adreno_drawctxt_wait_rb() - Wait for the last RB timestamp at which this
 * context submitted a command to the corresponding RB
 * @adreno_dev: The device on which the timestamp is active
 * @context: The context which subbmitted command to RB
 * @timestamp: The RB timestamp of last command submitted to RB by context
 * @timeout: Timeout value for the wait
 * Caller must hold the device mutex
 */
static int adreno_drawctxt_wait_rb(struct adreno_device *adreno_dev,
		struct kgsl_context *context,
		uint32_t timestamp, unsigned int timeout)
{
	struct adreno_context *drawctxt = ADRENO_CONTEXT(context);
	int ret = 0;

	/*
	 * If the context is invalid (OR) not submitted commands to GPU
	 * then return immediately - we may end up waiting for a timestamp
	 * that will never come
	 */
	if (kgsl_context_invalid(context) ||
			!test_bit(KGSL_CONTEXT_PRIV_SUBMITTED, &context->priv))
		goto done;

	trace_adreno_drawctxt_wait_start(drawctxt->rb->id, context->id,
					timestamp);

	ret = adreno_ringbuffer_waittimestamp(drawctxt->rb, timestamp, timeout);
done:
	trace_adreno_drawctxt_wait_done(drawctxt->rb->id, context->id,
					timestamp, ret);
	return ret;
}

static int drawctxt_detach_drawobjs(struct adreno_context *drawctxt,
		struct kgsl_drawobj **list)
{
	int count = 0;

	while (drawctxt->drawqueue_head != drawctxt->drawqueue_tail) {
		struct kgsl_drawobj *drawobj =
			drawctxt->drawqueue[drawctxt->drawqueue_head];

		drawctxt->drawqueue_head = (drawctxt->drawqueue_head + 1) %
			ADRENO_CONTEXT_DRAWQUEUE_SIZE;

		list[count++] = drawobj;
	}

	return count;
}

/**
 * adreno_drawctxt_invalidate() - Invalidate an adreno draw context
 * @device: Pointer to the KGSL device structure for the GPU
 * @context: Pointer to the KGSL context structure
 *
 * Invalidate the context and remove all queued commands and cancel any pending
 * waiters
 */
void adreno_drawctxt_invalidate(struct kgsl_device *device,
		struct kgsl_context *context)
{
	struct adreno_context *drawctxt = ADRENO_CONTEXT(context);
	struct kgsl_drawobj *list[ADRENO_CONTEXT_DRAWQUEUE_SIZE];
	int i, count;

	trace_adreno_drawctxt_invalidate(drawctxt);

	spin_lock(&drawctxt->lock);
	set_bit(KGSL_CONTEXT_PRIV_INVALID, &context->priv);

	/*
	 * set the timestamp to the last value since the context is invalidated
	 * and we want the pending events for this context to go away
	 */
	kgsl_sharedmem_writel(device->memstore,
			KGSL_MEMSTORE_OFFSET(context->id, soptimestamp),
			drawctxt->timestamp);

	kgsl_sharedmem_writel(device->memstore,
			KGSL_MEMSTORE_OFFSET(context->id, eoptimestamp),
			drawctxt->timestamp);

	/* Get rid of commands still waiting in the queue */
	count = drawctxt_detach_drawobjs(drawctxt, list);
	spin_unlock(&drawctxt->lock);

	for (i = 0; i < count; i++) {
		kgsl_cancel_events_timestamp(device, &context->events,
			list[i]->timestamp);
		kgsl_drawobj_destroy(list[i]);
	}

	/* Make sure all pending events are processed or cancelled */
	kgsl_flush_event_group(device, &context->events);

	/* Give the bad news to everybody waiting around */
	wake_up_all(&drawctxt->waiting);
	wake_up_all(&drawctxt->wq);
	wake_up_all(&drawctxt->timeout);
}

#define KGSL_CONTEXT_PRIORITY_MED	0x8

/**
 * adreno_drawctxt_create - create a new adreno draw context
 * @dev_priv: the owner of the context
 * @flags: flags for the context (passed from user space)
 *
 * Create and return a new draw context for the 3D core.
 */
struct kgsl_context *
adreno_drawctxt_create(struct kgsl_device_private *dev_priv,
			uint32_t *flags)
{
	struct adreno_context *drawctxt;
	struct kgsl_device *device = dev_priv->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	int ret;
	unsigned int local;

	local = *flags & (KGSL_CONTEXT_PREAMBLE |
		KGSL_CONTEXT_NO_GMEM_ALLOC |
		KGSL_CONTEXT_PER_CONTEXT_TS |
		KGSL_CONTEXT_USER_GENERATED_TS |
		KGSL_CONTEXT_NO_FAULT_TOLERANCE |
		KGSL_CONTEXT_INVALIDATE_ON_FAULT |
		KGSL_CONTEXT_CTX_SWITCH |
		KGSL_CONTEXT_PRIORITY_MASK |
		KGSL_CONTEXT_TYPE_MASK |
		KGSL_CONTEXT_PWR_CONSTRAINT |
		KGSL_CONTEXT_IFH_NOP |
		KGSL_CONTEXT_SECURE |
		KGSL_CONTEXT_PREEMPT_STYLE_MASK |
		KGSL_CONTEXT_NO_SNAPSHOT |
		KGSL_CONTEXT_SPARSE);

	/* Check for errors before trying to initialize */

	/* If preemption is not supported, ignore preemption request */
	if (!test_bit(ADRENO_DEVICE_PREEMPTION, &adreno_dev->priv))
		local &= ~KGSL_CONTEXT_PREEMPT_STYLE_MASK;

	/* We no longer support legacy context switching */
	if ((local & KGSL_CONTEXT_PREAMBLE) == 0 ||
		(local & KGSL_CONTEXT_NO_GMEM_ALLOC) == 0) {
		dev_err_once(device->dev,
			"legacy context switch not supported\n");
		return ERR_PTR(-EINVAL);
	}

	/* Make sure that our target can support secure contexts if requested */
	if (!kgsl_mmu_is_secured(&dev_priv->device->mmu) &&
			(local & KGSL_CONTEXT_SECURE)) {
		dev_err_once(device->dev, "Secure context not supported\n");
		return ERR_PTR(-EOPNOTSUPP);
	}

	drawctxt = kzalloc(sizeof(struct adreno_context), GFP_KERNEL);

	if (drawctxt == NULL)
		return ERR_PTR(-ENOMEM);

	drawctxt->timestamp = 0;

	drawctxt->base.flags = local;

	/* Always enable per-context timestamps */
	drawctxt->base.flags |= KGSL_CONTEXT_PER_CONTEXT_TS;
	drawctxt->type = (drawctxt->base.flags & KGSL_CONTEXT_TYPE_MASK)
		>> KGSL_CONTEXT_TYPE_SHIFT;
	spin_lock_init(&drawctxt->lock);
	init_waitqueue_head(&drawctxt->wq);
	init_waitqueue_head(&drawctxt->waiting);
	init_waitqueue_head(&drawctxt->timeout);

	/* If the priority is not set by user, set it for them */
	if ((drawctxt->base.flags & KGSL_CONTEXT_PRIORITY_MASK) ==
			KGSL_CONTEXT_PRIORITY_UNDEF)
		drawctxt->base.flags |= (KGSL_CONTEXT_PRIORITY_MED <<
				KGSL_CONTEXT_PRIORITY_SHIFT);

	/* Store the context priority */
	drawctxt->base.priority =
		(drawctxt->base.flags & KGSL_CONTEXT_PRIORITY_MASK) >>
		KGSL_CONTEXT_PRIORITY_SHIFT;

	/* set the context ringbuffer */
	drawctxt->rb = adreno_ctx_get_rb(adreno_dev, drawctxt);

	/*
	 * Now initialize the common part of the context. This allocates the
	 * context id, and then possibly another thread could look it up.
	 * So we want all of our initializtion that doesn't require the context
	 * id to be done before this call.
	 */
	ret = kgsl_context_init(dev_priv, &drawctxt->base);
	if (ret != 0) {
		kfree(drawctxt);
		return ERR_PTR(ret);
	}

	kgsl_sharedmem_writel(device->memstore,
			KGSL_MEMSTORE_OFFSET(drawctxt->base.id, soptimestamp),
			0);
	kgsl_sharedmem_writel(device->memstore,
			KGSL_MEMSTORE_OFFSET(drawctxt->base.id, eoptimestamp),
			0);

	adreno_context_debugfs_init(ADRENO_DEVICE(device), drawctxt);

	INIT_LIST_HEAD(&drawctxt->active_node);

	if (gpudev->preemption_context_init) {
		ret = gpudev->preemption_context_init(&drawctxt->base);
		if (ret != 0) {
			kgsl_context_detach(&drawctxt->base);
			return ERR_PTR(ret);
		}
	}

	/* copy back whatever flags we dediced were valid */
	*flags = drawctxt->base.flags;
	return &drawctxt->base;
}

/**
 * adreno_drawctxt_detach(): detach a context from the GPU
 * @context: Generic KGSL context container for the context
 *
 */
void adreno_drawctxt_detach(struct kgsl_context *context)
{
	struct kgsl_device *device;
	struct adreno_device *adreno_dev;
	struct adreno_gpudev *gpudev;
	struct adreno_context *drawctxt;
	struct adreno_ringbuffer *rb;
	int ret, count, i;
	struct kgsl_drawobj *list[ADRENO_CONTEXT_DRAWQUEUE_SIZE];

	if (context == NULL)
		return;

	device = context->device;
	adreno_dev = ADRENO_DEVICE(device);
	gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	drawctxt = ADRENO_CONTEXT(context);
	rb = drawctxt->rb;

	spin_lock(&drawctxt->lock);

	spin_lock(&adreno_dev->active_list_lock);
	list_del_init(&drawctxt->active_node);
	spin_unlock(&adreno_dev->active_list_lock);


	count = drawctxt_detach_drawobjs(drawctxt, list);
	spin_unlock(&drawctxt->lock);

	for (i = 0; i < count; i++) {
		/*
		 * If the context is deteached while we are waiting for
		 * the next command in GFT SKIP CMD, print the context
		 * detached status here.
		 */
		adreno_fault_skipcmd_detached(adreno_dev, drawctxt, list[i]);
		kgsl_drawobj_destroy(list[i]);
	}

	debugfs_remove_recursive(drawctxt->debug_root);
	/* The debugfs file has a reference, release it */
	if (drawctxt->debug_root)
		kgsl_context_put(context);

	/*
	 * internal_timestamp is set in adreno_ringbuffer_addcmds,
	 * which holds the device mutex.
	 */
	mutex_lock(&device->mutex);

	/*
	 * Wait for the last global timestamp to pass before continuing.
	 * The maxumum wait time is 30s, some large IB's can take longer
	 * than 10s and if hang happens then the time for the context's
	 * commands to retire will be greater than 10s. 30s should be sufficient
	 * time to wait for the commands even if a hang happens.
	 */
	ret = adreno_drawctxt_wait_rb(adreno_dev, context,
		drawctxt->internal_timestamp, 30 * 1000);

	/*
	 * If the wait for global fails due to timeout then mark it as
	 * context detach timeout fault and schedule dispatcher to kick
	 * in GPU recovery. For a ADRENO_CTX_DETATCH_TIMEOUT_FAULT we clear
	 * the policy and invalidate the context. If EAGAIN error is returned
	 * then recovery will kick in and there will be no more commands in the
	 * RB pipe from this context which is what we are waiting for, so ignore
	 * -EAGAIN error.
	 */
	if (ret && ret != -EAGAIN) {
		dev_err(device->dev,
				"Wait for global ctx=%u ts=%u type=%d error=%d\n",
				drawctxt->base.id, drawctxt->internal_timestamp,
				drawctxt->type, ret);

		adreno_set_gpu_fault(adreno_dev,
				ADRENO_CTX_DETATCH_TIMEOUT_FAULT);
		mutex_unlock(&device->mutex);

		/* Schedule dispatcher to kick in recovery */
		adreno_dispatcher_schedule(device);

		/* Wait for context to be invalidated and release context */
		wait_event_interruptible_timeout(drawctxt->timeout,
					kgsl_context_invalid(&drawctxt->base),
					msecs_to_jiffies(5000));
		return;
	}

	kgsl_sharedmem_writel(device->memstore,
			KGSL_MEMSTORE_OFFSET(context->id, soptimestamp),
			drawctxt->timestamp);

	kgsl_sharedmem_writel(device->memstore,
			KGSL_MEMSTORE_OFFSET(context->id, eoptimestamp),
			drawctxt->timestamp);

	adreno_profile_process_results(adreno_dev);

	mutex_unlock(&device->mutex);

	if (gpudev->preemption_context_destroy)
		gpudev->preemption_context_destroy(context);

	/* wake threads waiting to submit commands from this context */
	wake_up_all(&drawctxt->waiting);
	wake_up_all(&drawctxt->wq);
}

void adreno_drawctxt_destroy(struct kgsl_context *context)
{
	struct adreno_context *drawctxt;

	if (context == NULL)
		return;

	drawctxt = ADRENO_CONTEXT(context);
	kfree(drawctxt);
}

static void _drawctxt_switch_wait_callback(struct kgsl_device *device,
		struct kgsl_event_group *group,
		void *priv, int result)
{
	struct adreno_context *drawctxt = (struct adreno_context *) priv;

	kgsl_context_put(&drawctxt->base);
}

/**
 * adreno_drawctxt_switch - switch the current draw context in a given RB
 * @adreno_dev - The 3D device that owns the context
 * @rb: The ringubffer pointer on which the current context is being changed
 * @drawctxt - the 3D context to switch to
 *
 * Switch the current draw context in given RB
 */

int adreno_drawctxt_switch(struct adreno_device *adreno_dev,
				struct adreno_ringbuffer *rb,
				struct adreno_context *drawctxt)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_pagetable *new_pt;
	int ret = 0;

	/* We always expect a valid rb */
	if (!rb)
		return -EINVAL;

	/* already current? */
	if (rb->drawctxt_active == drawctxt)
		return ret;

	/*
	 * Submitting pt switch commands from a detached context can
	 * lead to a race condition where the pt is destroyed before
	 * the pt switch commands get executed by the GPU, leading to
	 * pagefaults.
	 */
	if (drawctxt != NULL && kgsl_context_detached(&drawctxt->base))
		return -ENOENT;

	trace_adreno_drawctxt_switch(rb, drawctxt);

	/* Get a refcount to the new instance */
	if (drawctxt) {
		if (!_kgsl_context_get(&drawctxt->base))
			return -ENOENT;

		new_pt = drawctxt->base.proc_priv->pagetable;
	} else {
		 /* No context - set the default pagetable and thats it. */
		new_pt = device->mmu.defaultpagetable;
	}

	ret = adreno_iommu_set_pt_ctx(rb, new_pt, drawctxt);
	if (ret)
		return ret;

	if (rb->drawctxt_active) {
		/* Wait for the timestamp to expire */
		if (kgsl_add_event(device, &rb->events, rb->timestamp,
			_drawctxt_switch_wait_callback,
			rb->drawctxt_active)) {
			kgsl_context_put(&rb->drawctxt_active->base);
		}
	}

	rb->drawctxt_active = drawctxt;
	return 0;
}
