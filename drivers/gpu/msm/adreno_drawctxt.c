/* Copyright (c) 2002,2007-2014, The Linux Foundation. All rights reserved.
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

#include "kgsl.h"
#include "kgsl_sharedmem.h"
#include "adreno.h"
#include "adreno_trace.h"

#define KGSL_INIT_REFTIMESTAMP		0x7FFFFFFF

static void wait_callback(struct kgsl_device *device,
		struct kgsl_context *context, void *priv, int result)
{
	struct adreno_context *drawctxt = priv;
	wake_up_all(&drawctxt->waiting);
}

#define adreno_wait_event_interruptible_timeout(wq, condition, timeout, io)   \
({                                                                            \
	long __ret = timeout;                                                 \
	if (io)                                                               \
		__wait_io_event_interruptible_timeout(wq, condition, __ret);  \
	else                                                                  \
		__wait_event_interruptible_timeout(wq, condition, __ret);     \
	__ret;                                                                \
})

#define adreno_wait_event_interruptible(wq, condition, io)                    \
({                                                                            \
	long __ret;                                                           \
	if (io)                                                               \
		__wait_io_event_interruptible(wq, condition, __ret);          \
	else                                                                  \
		__wait_event_interruptible(wq, condition, __ret);             \
	__ret;                                                                \
})

static int _check_context_timestamp(struct kgsl_device *device,
		struct adreno_context *drawctxt, unsigned int timestamp)
{
	int ret = 0;

	/* Bail if the drawctxt has been invalidated or destroyed */
	if (kgsl_context_detached(&drawctxt->base) ||
		drawctxt->state != ADRENO_CONTEXT_STATE_ACTIVE)
		return 1;

	kgsl_mutex_lock(&device->mutex, &device->mutex_owner);
	ret = kgsl_check_timestamp(device, &drawctxt->base, timestamp);
	kgsl_mutex_unlock(&device->mutex, &device->mutex_owner);

	return ret;
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
	static unsigned int io_cnt;
	struct kgsl_device *device = &adreno_dev->dev;
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct adreno_context *drawctxt = ADRENO_CONTEXT(context);
	int ret, io;

	if (kgsl_context_detached(context))
		return -EINVAL;

	if (drawctxt->state == ADRENO_CONTEXT_STATE_INVALID)
		return -EDEADLK;

	/* Needs to hold the device mutex */
	BUG_ON(!mutex_is_locked(&device->mutex));

	trace_adreno_drawctxt_wait_start(context->id, timestamp);

	ret = kgsl_add_event(device, &context->events, timestamp,
		wait_callback, (void *) drawctxt);
	if (ret)
		goto done;

	/*
	 * For proper power accounting sometimes we need to call
	 * io_wait_interruptible_timeout and sometimes we need to call
	 * plain old wait_interruptible_timeout. We call the regular
	 * timeout N times out of 100, where N is a number specified by
	 * the current power level
	 */

	io_cnt = (io_cnt + 1) % 100;
	io = (io_cnt < pwr->pwrlevels[pwr->active_pwrlevel].io_fraction)
		? 0 : 1;

	kgsl_mutex_unlock(&device->mutex, &device->mutex_owner);

	if (timeout) {
		long ret_temp;
		ret_temp = adreno_wait_event_interruptible_timeout(
			drawctxt->waiting,
			_check_context_timestamp(device, drawctxt, timestamp),
			msecs_to_jiffies(timeout), io);

		if (ret_temp == 0)
			ret = -ETIMEDOUT;
		else if (ret_temp > 0)
			ret = 0;
		else
			ret = (int) ret_temp;
	} else {
		ret = (int) adreno_wait_event_interruptible(drawctxt->waiting,
			_check_context_timestamp(device, drawctxt, timestamp),
				io);
	}

	kgsl_mutex_lock(&device->mutex, &device->mutex_owner);

	/* -EDEADLK if the context was invalidated while we were waiting */
	if (drawctxt->state == ADRENO_CONTEXT_STATE_INVALID)
		ret = -EDEADLK;


	/* Return -EINVAL if the context was detached while we were waiting */
	if (kgsl_context_detached(context))
		ret = -EINVAL;

done:
	trace_adreno_drawctxt_wait_done(context->id, timestamp, ret);
	return ret;
}
static void global_wait_callback(struct kgsl_device *device,
		struct kgsl_context *context, void *priv, int result)
{
	struct adreno_context *drawctxt = priv;

	wake_up_all(&drawctxt->waiting);
	kgsl_context_put(&drawctxt->base);
}

static int _check_global_timestamp(struct kgsl_device *device,
		struct adreno_context *drawctxt, unsigned int timestamp)
{
	/* Stop waiting if the context is invalidated */
	if (drawctxt->state == ADRENO_CONTEXT_STATE_INVALID)
		return 1;

	return kgsl_check_timestamp(device, NULL, timestamp);
}

static int adreno_drawctxt_wait_global(struct adreno_device *adreno_dev,
		struct kgsl_context *context,
		uint32_t timestamp, unsigned int timeout)
{
	struct kgsl_device *device = &adreno_dev->dev;
	struct adreno_context *drawctxt = ADRENO_CONTEXT(context);
	int ret = 0;

	/* Needs to hold the device mutex */
	BUG_ON(!mutex_is_locked(&device->mutex));

	if (!_kgsl_context_get(context)) {
		ret = -EINVAL;
		goto done;
	}

	/*
	 * If the context is invalid then return immediately - we may end up
	 * waiting for a timestamp that will never come
	 */
	if (drawctxt->state == ADRENO_CONTEXT_STATE_INVALID) {
		kgsl_context_put(context);
		goto done;
	}

	trace_adreno_drawctxt_wait_start(KGSL_MEMSTORE_GLOBAL, timestamp);

	ret = kgsl_add_event(device, &device->global_events, timestamp,
		global_wait_callback, (void *) drawctxt);
	if (ret) {
		kgsl_context_put(context);
		goto done;
	}

	kgsl_mutex_unlock(&device->mutex, &device->mutex_owner);

	if (timeout) {
		ret = (int) wait_event_timeout(drawctxt->waiting,
			_check_global_timestamp(device, drawctxt, timestamp),
			msecs_to_jiffies(timeout));

		if (ret == 0)
			ret = -ETIMEDOUT;
		else if (ret > 0)
			ret = 0;
	} else {
		wait_event(drawctxt->waiting,
			_check_global_timestamp(device, drawctxt, timestamp));
	}

	kgsl_mutex_lock(&device->mutex, &device->mutex_owner);

	if (ret)
		kgsl_cancel_events_timestamp(device, &device->global_events,
			timestamp);

done:
	trace_adreno_drawctxt_wait_done(KGSL_MEMSTORE_GLOBAL, timestamp, ret);
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

	drawctxt->state = ADRENO_CONTEXT_STATE_INVALID;

	/* Clear the pending queue */
	mutex_lock(&drawctxt->mutex);

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

	while (drawctxt->cmdqueue_head != drawctxt->cmdqueue_tail) {
		struct kgsl_cmdbatch *cmdbatch =
			drawctxt->cmdqueue[drawctxt->cmdqueue_head];

		drawctxt->cmdqueue_head = (drawctxt->cmdqueue_head + 1) %
			ADRENO_CONTEXT_CMDQUEUE_SIZE;

		mutex_unlock(&drawctxt->mutex);

		kgsl_mutex_lock(&device->mutex, &device->mutex_owner);
		kgsl_cancel_events_timestamp(device, &context->events,
			cmdbatch->timestamp);
		kgsl_mutex_unlock(&device->mutex, &device->mutex_owner);

		kgsl_cmdbatch_destroy(cmdbatch);
		mutex_lock(&drawctxt->mutex);
	}

	mutex_unlock(&drawctxt->mutex);

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
	int ret;

	drawctxt = kzalloc(sizeof(struct adreno_context), GFP_KERNEL);

	if (drawctxt == NULL)
		return ERR_PTR(-ENOMEM);

	ret = kgsl_context_init(dev_priv, &drawctxt->base);
	if (ret != 0) {
		kfree(drawctxt);
		return ERR_PTR(ret);
	}

	drawctxt->timestamp = 0;

	drawctxt->base.flags = *flags & (KGSL_CONTEXT_PREAMBLE |
		KGSL_CONTEXT_NO_GMEM_ALLOC |
		KGSL_CONTEXT_PER_CONTEXT_TS |
		KGSL_CONTEXT_USER_GENERATED_TS |
		KGSL_CONTEXT_NO_FAULT_TOLERANCE |
		KGSL_CONTEXT_CTX_SWITCH |
		KGSL_CONTEXT_PRIORITY_MASK |
		KGSL_CONTEXT_TYPE_MASK |
		KGSL_CONTEXT_PWR_CONSTRAINT |
		KGSL_CONTEXT_IFH_NOP);

	/* Always enable per-context timestamps */
	drawctxt->base.flags |= KGSL_CONTEXT_PER_CONTEXT_TS;
	drawctxt->type = (drawctxt->base.flags & KGSL_CONTEXT_TYPE_MASK)
		>> KGSL_CONTEXT_TYPE_SHIFT;
	mutex_init(&drawctxt->mutex);
	init_waitqueue_head(&drawctxt->wq);
	init_waitqueue_head(&drawctxt->waiting);

	/* Set the context priority */
	_set_context_priority(drawctxt);

	/*
	 * Set up the plist node for the dispatcher.  Insert the node into the
	 * drawctxt pending list based on priority.
	 */
	plist_node_init(&drawctxt->pending, drawctxt->base.priority);

	if ((drawctxt->base.flags & KGSL_CONTEXT_PREAMBLE) == 0 ||
		  (drawctxt->base.flags & KGSL_CONTEXT_NO_GMEM_ALLOC) == 0) {
		KGSL_DEV_ERR_ONCE(device,
				"legacy context switch not supported\n");
		ret = -EINVAL;
		goto err;

	}

	kgsl_sharedmem_writel(device, &device->memstore,
			KGSL_MEMSTORE_OFFSET(drawctxt->base.id, soptimestamp),
			0);
	kgsl_sharedmem_writel(device, &device->memstore,
			KGSL_MEMSTORE_OFFSET(drawctxt->base.id, eoptimestamp),
			0);
	/* copy back whatever flags we dediced were valid */
	*flags = drawctxt->base.flags;
	return &drawctxt->base;
err:
	kgsl_context_detach(&drawctxt->base);
	return ERR_PTR(ret);
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
int adreno_drawctxt_detach(struct kgsl_context *context)
{
	struct kgsl_device *device;
	struct adreno_device *adreno_dev;
	struct adreno_context *drawctxt;
	int ret;

	if (context == NULL)
		return 0;

	device = context->device;
	adreno_dev = ADRENO_DEVICE(device);
	drawctxt = ADRENO_CONTEXT(context);

	/* deactivate context */
	if (adreno_dev->drawctxt_active == drawctxt)
		adreno_drawctxt_switch(adreno_dev, NULL, 0);

	mutex_lock(&drawctxt->mutex);

	while (drawctxt->cmdqueue_head != drawctxt->cmdqueue_tail) {
		struct kgsl_cmdbatch *cmdbatch =
			drawctxt->cmdqueue[drawctxt->cmdqueue_head];

		drawctxt->cmdqueue_head = (drawctxt->cmdqueue_head + 1) %
			ADRENO_CONTEXT_CMDQUEUE_SIZE;

		mutex_unlock(&drawctxt->mutex);

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
		mutex_lock(&drawctxt->mutex);
	}

	mutex_unlock(&drawctxt->mutex);
	/*
	 * internal_timestamp is set in adreno_ringbuffer_addcmds,
	 * which holds the device mutex. The entire context destroy
	 * process requires the device mutex as well. But lets
	 * make sure we notice if the locking changes.
	 */
	BUG_ON(!mutex_is_locked(&device->mutex));

	/* Wait for the last global timestamp to pass before continuing */
	ret = adreno_drawctxt_wait_global(adreno_dev, context,
		drawctxt->internal_timestamp, 10 * 1000);

	/*
	 * If the wait for global fails then nothing after this point is likely
	 * to work very well - BUG_ON() so we can take advantage of the debug
	 * tools to figure out what the h - e - double hockey sticks happened
	 */

	BUG_ON(ret);

	kgsl_sharedmem_writel(device, &device->memstore,
			KGSL_MEMSTORE_OFFSET(context->id, soptimestamp),
			drawctxt->timestamp);

	kgsl_sharedmem_writel(device, &device->memstore,
			KGSL_MEMSTORE_OFFSET(context->id, eoptimestamp),
			drawctxt->timestamp);

	adreno_profile_process_results(device);

	/* wake threads waiting to submit commands from this context */
	wake_up_all(&drawctxt->waiting);
	wake_up_all(&drawctxt->wq);

	return ret;
}

void adreno_drawctxt_destroy(struct kgsl_context *context)
{
	struct adreno_context *drawctxt;
	if (context == NULL)
		return;

	drawctxt = ADRENO_CONTEXT(context);
	kfree(drawctxt);
}

/**
 * adreno_context_restore() - generic context restore handler
 * @adreno_dev: the device
 * @context: the context
 *
 * Basic context restore handler that writes the context identifier
 * to the ringbuffer and issues pagetable switch commands if necessary.
 */
static int adreno_context_restore(struct adreno_device *adreno_dev,
				  struct adreno_context *context)
{
	struct kgsl_device *device;
	unsigned int cmds[8];

	if (adreno_dev == NULL || context == NULL)
		return -EINVAL;

	device = &adreno_dev->dev;

	/* write the context identifier to the ringbuffer */
	cmds[0] = cp_nop_packet(1);
	cmds[1] = KGSL_CONTEXT_TO_MEM_IDENTIFIER;
	cmds[2] = cp_type3_packet(CP_MEM_WRITE, 2);
	cmds[3] = device->memstore.gpuaddr +
		KGSL_MEMSTORE_OFFSET(KGSL_MEMSTORE_GLOBAL, current_context);
	cmds[4] = context->base.id;
	/* Flush the UCHE for new context */
	cmds[5] = cp_type0_packet(
		adreno_getreg(adreno_dev, ADRENO_REG_UCHE_INVALIDATE0), 2);
	cmds[6] = 0;
	if (adreno_is_a4xx(adreno_dev))
		cmds[7] = 0x12;
	else if (adreno_is_a3xx(adreno_dev))
		cmds[7] = 0x90000000;
	return adreno_ringbuffer_issuecmds(device, context,
				KGSL_CMD_FLAGS_NONE, cmds, 8);
}

/**
 * adreno_drawctxt_switch - switch the current draw context
 * @adreno_dev - The 3D device that owns the context
 * @drawctxt - the 3D context to switch to
 * @flags - Flags to accompany the switch (from user space)
 *
 * Switch the current draw context
 */

int adreno_drawctxt_switch(struct adreno_device *adreno_dev,
				struct adreno_context *drawctxt,
				unsigned int flags)
{
	struct kgsl_device *device = &adreno_dev->dev;
	int ret = 0;

	/* already current? */
	if (adreno_dev->drawctxt_active == drawctxt)
		return ret;

	trace_adreno_drawctxt_switch(adreno_dev->drawctxt_active,
		drawctxt, flags);

	/* Get a refcount to the new instance */
	if (drawctxt) {
		if (!_kgsl_context_get(&drawctxt->base))
			return -EINVAL;

		ret = kgsl_mmu_setstate(&device->mmu,
			drawctxt->base.proc_priv->pagetable,
			adreno_dev->drawctxt_active ?
			adreno_dev->drawctxt_active->base.id :
			KGSL_CONTEXT_INVALID);
		/* Set the new context */
		ret = adreno_context_restore(adreno_dev, drawctxt);
		if (ret) {
			KGSL_DRV_ERR(device,
					"Error in GPU context %d restore: %d\n",
					drawctxt->base.id, ret);
			return ret;
		}
	} else {
		/*
		 * No context - set the default pagetable and thats it.
		 * If there isn't a current context, the kgsl_mmu_setstate
		 * will use the CPU path so we don't need to give
		 * it a valid context id.
		 */
		ret = kgsl_mmu_setstate(&device->mmu,
					 device->mmu.defaultpagetable,
					adreno_dev->drawctxt_active->base.id);
	}
	/* Put the old instance of the active drawctxt */
	if (adreno_dev->drawctxt_active)
		kgsl_context_put(&adreno_dev->drawctxt_active->base);
	adreno_dev->drawctxt_active = drawctxt;
	return 0;
}
