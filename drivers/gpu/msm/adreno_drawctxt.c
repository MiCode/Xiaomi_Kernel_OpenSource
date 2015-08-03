/* Copyright (c) 2002,2007-2015, The Linux Foundation. All rights reserved.
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

#include <linux/slab.h>
#include <linux/msm_kgsl.h>
#include <linux/sched.h>
#include <linux/debugfs.h>

#include "kgsl.h"
#include "kgsl_sharedmem.h"
#include "adreno.h"
#include "adreno_trace.h"

#define KGSL_INIT_REFTIMESTAMP		0x7FFFFFFF

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
	 * We may have cmdbatch timer running, which also uses same
	 * lock, take a lock with software interrupt disabled (bh)
	 * to avoid spin lock recursion.
	 */
	spin_lock_bh(&drawctxt->lock);
	dev_err(device->dev,
		"  context[%d]: queue=%d, submit=%d, start=%d, retire=%d\n",
		context->id, queue, drawctxt->submitted_timestamp,
		start, retire);

	if (drawctxt->cmdqueue_head != drawctxt->cmdqueue_tail) {
		struct kgsl_cmdbatch *cmdbatch =
			drawctxt->cmdqueue[drawctxt->cmdqueue_head];

		if (test_bit(CMDBATCH_FLAG_FENCE_LOG, &cmdbatch->priv)) {
			dev_err(device->dev,
				"  possible deadlock. Context %d might be blocked for itself\n",
				context->id);
			goto stats;
		}

		/*
		 * We may have cmdbatch timer running, which also uses same
		 * lock, take a lock with software interrupt disabled (bh)
		 * to avoid spin lock recursion.
		 */
		spin_lock_bh(&cmdbatch->lock);

		if (!list_empty(&cmdbatch->synclist)) {
			dev_err(device->dev,
				"  context[%d] (ts=%d) Active sync points:\n",
				context->id, cmdbatch->timestamp);

			kgsl_dump_syncpoints(device, cmdbatch);
		}
		spin_unlock_bh(&cmdbatch->lock);
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
		pos += snprintf(buf + pos, sizeof(buf) - pos, "%d.%0d ",
			(unsigned int)msecs, usecs);
	}
	dev_err(device->dev, "  context[%d]: submit times: %s\n",
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
	struct kgsl_device *device = &adreno_dev->dev;
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
 */
static int adreno_drawctxt_wait_rb(struct adreno_device *adreno_dev,
		struct kgsl_context *context,
		uint32_t timestamp, unsigned int timeout)
{
	struct kgsl_device *device = &adreno_dev->dev;
	struct adreno_context *drawctxt = ADRENO_CONTEXT(context);
	int ret = 0;

	/* Needs to hold the device mutex */
	BUG_ON(!mutex_is_locked(&device->mutex));

	/*
	 * If the context is invalid then return immediately - we may end up
	 * waiting for a timestamp that will never come
	 */
	if (kgsl_context_invalid(context))
		goto done;

	trace_adreno_drawctxt_wait_start(drawctxt->rb->id, context->id,
					timestamp);

	ret = adreno_ringbuffer_waittimestamp(drawctxt->rb, timestamp, timeout);
done:
	trace_adreno_drawctxt_wait_done(drawctxt->rb->id, context->id,
					timestamp, ret);
	return ret;
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

	trace_adreno_drawctxt_invalidate(drawctxt);

	spin_lock(&drawctxt->lock);
	set_bit(KGSL_CONTEXT_PRIV_INVALID, &context->priv);

	/*
	 * set the timestamp to the last value since the context is invalidated
	 * and we want the pending events for this context to go away
	 */
	kgsl_sharedmem_writel(device, &device->memstore,
			KGSL_MEMSTORE_OFFSET(context->id, soptimestamp),
			drawctxt->timestamp);

	kgsl_sharedmem_writel(device, &device->memstore,
			KGSL_MEMSTORE_OFFSET(context->id, eoptimestamp),
			drawctxt->timestamp);

	/* Get rid of commands still waiting in the queue */
	while (drawctxt->cmdqueue_head != drawctxt->cmdqueue_tail) {
		struct kgsl_cmdbatch *cmdbatch =
			drawctxt->cmdqueue[drawctxt->cmdqueue_head];

		drawctxt->cmdqueue_head = (drawctxt->cmdqueue_head + 1) %
			ADRENO_CONTEXT_CMDQUEUE_SIZE;

		kgsl_cancel_events_timestamp(device, &context->events,
			cmdbatch->timestamp);

		kgsl_cmdbatch_destroy(cmdbatch);
	}

	spin_unlock(&drawctxt->lock);

	/* Make sure all pending events are processed or cancelled */
	kgsl_flush_event_group(device, &context->events);

	/* Give the bad news to everybody waiting around */
	wake_up_all(&drawctxt->waiting);
	wake_up_all(&drawctxt->wq);
}

/*
 * Set the priority of the context based on the flags passed into context
 * create.  If the priority is not set in the flags, then the kernel can
 * assign any priority it desires for the context.
 */
#define KGSL_CONTEXT_PRIORITY_MED	0x8

static inline void _set_context_priority(struct adreno_context *drawctxt)
{
	/* If the priority is not set by user, set it for them */
	if ((drawctxt->base.flags & KGSL_CONTEXT_PRIORITY_MASK) ==
			KGSL_CONTEXT_PRIORITY_UNDEF)
		drawctxt->base.flags |= (KGSL_CONTEXT_PRIORITY_MED <<
				KGSL_CONTEXT_PRIORITY_SHIFT);

	/* Store the context priority */
	drawctxt->base.priority =
		(drawctxt->base.flags & KGSL_CONTEXT_PRIORITY_MASK) >>
		KGSL_CONTEXT_PRIORITY_SHIFT;
}

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
	int ret;
	unsigned long local;

	local = *flags & (KGSL_CONTEXT_PREAMBLE |
		KGSL_CONTEXT_NO_GMEM_ALLOC |
		KGSL_CONTEXT_PER_CONTEXT_TS |
		KGSL_CONTEXT_USER_GENERATED_TS |
		KGSL_CONTEXT_NO_FAULT_TOLERANCE |
		KGSL_CONTEXT_CTX_SWITCH |
		KGSL_CONTEXT_PRIORITY_MASK |
		KGSL_CONTEXT_TYPE_MASK |
		KGSL_CONTEXT_PWR_CONSTRAINT |
		KGSL_CONTEXT_IFH_NOP |
		KGSL_CONTEXT_SECURE |
		KGSL_CONTEXT_PREEMPT_STYLE_MASK);

	/* Check for errors before trying to initialize */

	/* If preemption is not supported, ignore preemption request */
	if (ADRENO_FEATURE(adreno_dev, ADRENO_PREEMPTION))
		local &= ~KGSL_CONTEXT_PREEMPT_STYLE_MASK;

	/* We no longer support legacy context switching */
	if ((local & KGSL_CONTEXT_PREAMBLE) == 0 ||
		(local & KGSL_CONTEXT_NO_GMEM_ALLOC) == 0) {
		KGSL_DEV_ERR_ONCE(device,
			"legacy context switch not supported\n");
		return ERR_PTR(-EINVAL);
	}

	/* Make sure that our target can support secure contexts if requested */
	if (!kgsl_mmu_is_secured(&dev_priv->device->mmu) &&
			(local & KGSL_CONTEXT_SECURE)) {
		KGSL_DEV_ERR_ONCE(device, "Secure context not supported\n");
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

	/* Set the context priority */
	_set_context_priority(drawctxt);
	/* set the context ringbuffer */
	drawctxt->rb = adreno_ctx_get_rb(adreno_dev, drawctxt);

	/*
	 * Set up the plist node for the dispatcher.  Insert the node into the
	 * drawctxt pending list based on priority.
	 */
	plist_node_init(&drawctxt->pending, drawctxt->base.priority);

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

	kgsl_sharedmem_writel(device, &device->memstore,
			KGSL_MEMSTORE_OFFSET(drawctxt->base.id, soptimestamp),
			0);
	kgsl_sharedmem_writel(device, &device->memstore,
			KGSL_MEMSTORE_OFFSET(drawctxt->base.id, eoptimestamp),
			0);

	adreno_context_debugfs_init(ADRENO_DEVICE(device), drawctxt);

	/* copy back whatever flags we dediced were valid */
	*flags = drawctxt->base.flags;
	return &drawctxt->base;
}

/**
 * adreno_drawctxt_sched() - Schedule a previously blocked context
 * @device: pointer to a KGSL device
 * @drawctxt: drawctxt to rechedule
 *
 * This function is called by the core when it knows that a previously blocked
 * context has been unblocked.  The default adreno response is to reschedule the
 * context on the dispatcher
 */
void adreno_drawctxt_sched(struct kgsl_device *device,
		struct kgsl_context *context)
{
	adreno_dispatcher_queue_context(device, ADRENO_CONTEXT(context));
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
	struct adreno_context *drawctxt;
	struct adreno_ringbuffer *rb;
	int ret;

	if (context == NULL)
		return;

	device = context->device;
	adreno_dev = ADRENO_DEVICE(device);
	drawctxt = ADRENO_CONTEXT(context);
	rb = drawctxt->rb;

	/* deactivate context */
	mutex_lock(&device->mutex);
	if (rb->drawctxt_active == drawctxt) {
		if (adreno_dev->cur_rb == rb) {
			if (!kgsl_active_count_get(device)) {
				adreno_drawctxt_switch(adreno_dev, rb, NULL, 0);
				kgsl_active_count_put(device);
			} else
				BUG();
		} else
			adreno_drawctxt_switch(adreno_dev, rb, NULL, 0);
	}
	mutex_unlock(&device->mutex);

	spin_lock(&drawctxt->lock);

	while (drawctxt->cmdqueue_head != drawctxt->cmdqueue_tail) {
		struct kgsl_cmdbatch *cmdbatch =
			drawctxt->cmdqueue[drawctxt->cmdqueue_head];

		drawctxt->cmdqueue_head = (drawctxt->cmdqueue_head + 1) %
			ADRENO_CONTEXT_CMDQUEUE_SIZE;

		spin_unlock(&drawctxt->lock);

		/*
		 * If the context is deteached while we are waiting for
		 * the next command in GFT SKIP CMD, print the context
		 * detached status here.
		 */
		adreno_fault_skipcmd_detached(device, drawctxt, cmdbatch);

		/*
		 * Don't hold the drawctxt mutex while the cmdbatch is being
		 * destroyed because the cmdbatch destroy takes the device
		 * mutex and the world falls in on itself
		 */

		kgsl_cmdbatch_destroy(cmdbatch);
		spin_lock(&drawctxt->lock);
	}

	spin_unlock(&drawctxt->lock);
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
	 * If the wait for global fails due to timeout then nothing after this
	 * point is likely to work very well - BUG_ON() so we can take advantage
	 * of the debug tools to figure out what the h - e - double hockey
	 * sticks happened. If EAGAIN error is returned then recovery will kick
	 * in and there will be no more commands in the RB pipe from this
	 * context which is waht we are waiting for, so ignore -EAGAIN error
	 */
	BUG_ON(ret && ret != -EAGAIN);

	kgsl_sharedmem_writel(device, &device->memstore,
			KGSL_MEMSTORE_OFFSET(context->id, soptimestamp),
			drawctxt->timestamp);

	kgsl_sharedmem_writel(device, &device->memstore,
			KGSL_MEMSTORE_OFFSET(context->id, eoptimestamp),
			drawctxt->timestamp);

	adreno_profile_process_results(adreno_dev);

	mutex_unlock(&device->mutex);

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
	debugfs_remove_recursive(drawctxt->debug_root);
	kfree(drawctxt);
}

/**
 * adreno_drawctxt_switch - switch the current draw context in a given RB
 * @adreno_dev - The 3D device that owns the context
 * @rb: The ringubffer pointer on which the current context is being changed
 * @drawctxt - the 3D context to switch to
 * @flags - Flags to accompany the switch (from user space)
 *
 * Switch the current draw context in given RB
 */

int adreno_drawctxt_switch(struct adreno_device *adreno_dev,
				struct adreno_ringbuffer *rb,
				struct adreno_context *drawctxt,
				unsigned int flags)
{
	struct kgsl_device *device = &adreno_dev->dev;
	struct kgsl_pagetable *new_pt;
	int ret = 0;

	/* We always expect a valid rb */
	BUG_ON(!rb);

	/* already current? */
	if (rb->drawctxt_active == drawctxt)
		return ret;

	trace_adreno_drawctxt_switch(rb,
		drawctxt, flags);

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
	if (ret) {
		KGSL_DRV_ERR(device,
			"Failed to set pagetable on rb %d\n", rb->id);
		return ret;
	}

	/* Put the old instance of the active drawctxt */
	if (rb->drawctxt_active)
		kgsl_context_put(&rb->drawctxt_active->base);

	rb->drawctxt_active = drawctxt;
	return 0;
}
