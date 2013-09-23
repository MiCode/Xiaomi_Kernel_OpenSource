/* Copyright (c) 2002,2007-2013, The Linux Foundation. All rights reserved.
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

/* quad for copying GMEM to context shadow */
#define QUAD_LEN 12
#define QUAD_RESTORE_LEN 14

static unsigned int gmem_copy_quad[QUAD_LEN] = {
	0x00000000, 0x00000000, 0x3f800000,
	0x00000000, 0x00000000, 0x3f800000,
	0x00000000, 0x00000000, 0x3f800000,
	0x00000000, 0x00000000, 0x3f800000
};

static unsigned int gmem_restore_quad[QUAD_RESTORE_LEN] = {
	0x00000000, 0x3f800000, 0x3f800000,
	0x00000000, 0x00000000, 0x00000000,
	0x3f800000, 0x00000000, 0x00000000,
	0x3f800000, 0x00000000, 0x00000000,
	0x3f800000, 0x3f800000,
};

#define TEXCOORD_LEN 8

static unsigned int gmem_copy_texcoord[TEXCOORD_LEN] = {
	0x00000000, 0x3f800000,
	0x3f800000, 0x3f800000,
	0x00000000, 0x00000000,
	0x3f800000, 0x00000000
};

/*
 * Helper functions
 * These are global helper functions used by the GPUs during context switch
 */

/**
 * uint2float - convert a uint to IEEE754 single precision float
 * @ uintval - value to convert
 */

unsigned int uint2float(unsigned int uintval)
{
	unsigned int exp, frac = 0;

	if (uintval == 0)
		return 0;

	exp = ilog2(uintval);

	/* Calculate fraction */
	if (23 > exp)
		frac = (uintval & (~(1 << exp))) << (23 - exp);

	/* Exp is biased by 127 and shifted 23 bits */
	exp = (exp + 127) << 23;

	return exp | frac;
}

static void set_gmem_copy_quad(struct gmem_shadow_t *shadow)
{
	/* set vertex buffer values */
	gmem_copy_quad[1] = uint2float(shadow->height);
	gmem_copy_quad[3] = uint2float(shadow->width);
	gmem_copy_quad[4] = uint2float(shadow->height);
	gmem_copy_quad[9] = uint2float(shadow->width);

	gmem_restore_quad[5] = uint2float(shadow->height);
	gmem_restore_quad[7] = uint2float(shadow->width);

	memcpy(shadow->quad_vertices.hostptr, gmem_copy_quad, QUAD_LEN << 2);
	memcpy(shadow->quad_vertices_restore.hostptr, gmem_restore_quad,
		QUAD_RESTORE_LEN << 2);

	memcpy(shadow->quad_texcoords.hostptr, gmem_copy_texcoord,
		TEXCOORD_LEN << 2);
}

/**
 * build_quad_vtxbuff - Create a quad for saving/restoring GMEM
 * @ context - Pointer to the context being created
 * @ shadow - Pointer to the GMEM shadow structure
 * @ incmd - Pointer to pointer to the temporary command buffer
 */

/* quad for saving/restoring gmem */
void build_quad_vtxbuff(struct adreno_context *drawctxt,
		struct gmem_shadow_t *shadow, unsigned int **incmd)
{
	 unsigned int *cmd = *incmd;

	/* quad vertex buffer location (in GPU space) */
	shadow->quad_vertices.hostptr = cmd;
	shadow->quad_vertices.gpuaddr = virt2gpu(cmd, &drawctxt->gpustate);

	cmd += QUAD_LEN;

	/* Used by A3XX, but define for both to make the code easier */
	shadow->quad_vertices_restore.hostptr = cmd;
	shadow->quad_vertices_restore.gpuaddr =
		virt2gpu(cmd, &drawctxt->gpustate);

	cmd += QUAD_RESTORE_LEN;

	/* tex coord buffer location (in GPU space) */
	shadow->quad_texcoords.hostptr = cmd;
	shadow->quad_texcoords.gpuaddr = virt2gpu(cmd, &drawctxt->gpustate);

	cmd += TEXCOORD_LEN;

	set_gmem_copy_quad(shadow);
	*incmd = cmd;
}

static void wait_callback(struct kgsl_device *device, void *priv, u32 id,
		u32 timestamp, u32 type)
{
	struct adreno_context *drawctxt = priv;
	wake_up_interruptible_all(&drawctxt->waiting);
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

	mutex_lock(&device->mutex);
	ret = kgsl_check_timestamp(device, &drawctxt->base, timestamp);
	mutex_unlock(&device->mutex);

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

	ret = kgsl_add_event(device, context->id, timestamp,
		wait_callback, drawctxt, NULL);
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

	mutex_unlock(&device->mutex);

	if (timeout) {
		ret = (int) adreno_wait_event_interruptible_timeout(
			drawctxt->waiting,
			_check_context_timestamp(device, drawctxt, timestamp),
			msecs_to_jiffies(timeout), io);

		if (ret == 0)
			ret = -ETIMEDOUT;
		else if (ret > 0)
			ret = 0;
	} else {
		ret = (int) adreno_wait_event_interruptible(drawctxt->waiting,
			_check_context_timestamp(device, drawctxt, timestamp),
				io);
	}

	mutex_lock(&device->mutex);

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

static void global_wait_callback(struct kgsl_device *device, void *priv, u32 id,
		u32 timestamp, u32 type)
{
	struct adreno_context *drawctxt = priv;

	wake_up_interruptible_all(&drawctxt->waiting);
	kgsl_context_put(&drawctxt->base);
}

static int _check_global_timestamp(struct kgsl_device *device,
		unsigned int timestamp)
{
	int ret;

	mutex_lock(&device->mutex);
	ret = kgsl_check_timestamp(device, NULL, timestamp);
	mutex_unlock(&device->mutex);

	return ret;
}

int adreno_drawctxt_wait_global(struct adreno_device *adreno_dev,
		struct kgsl_context *context,
		uint32_t timestamp, unsigned int timeout)
{
	struct kgsl_device *device = &adreno_dev->dev;
	struct adreno_context *drawctxt = ADRENO_CONTEXT(context);
	int ret;

	/* Needs to hold the device mutex */
	BUG_ON(!mutex_is_locked(&device->mutex));

	if (!_kgsl_context_get(context)) {
		ret = -EINVAL;
		goto done;
	}

	trace_adreno_drawctxt_wait_start(KGSL_MEMSTORE_GLOBAL, timestamp);

	ret = kgsl_add_event(device, KGSL_MEMSTORE_GLOBAL, timestamp,
		global_wait_callback, drawctxt, NULL);
	if (ret) {
		kgsl_context_put(context);
		goto done;
	}

	mutex_unlock(&device->mutex);

	if (timeout) {
		ret = (int) wait_event_interruptible_timeout(drawctxt->waiting,
			_check_global_timestamp(device, timestamp),
			msecs_to_jiffies(timeout));

		if (ret == 0)
			ret = -ETIMEDOUT;
		else if (ret > 0)
			ret = 0;
	} else {
		ret = (int) wait_event_interruptible(drawctxt->waiting,
			_check_global_timestamp(device, timestamp));
	}

	mutex_lock(&device->mutex);

	if (ret)
		kgsl_cancel_events_timestamp(device, NULL, timestamp);

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

		mutex_lock(&device->mutex);
		kgsl_cancel_events_timestamp(device, context,
			cmdbatch->timestamp);
		mutex_unlock(&device->mutex);

		kgsl_cmdbatch_destroy(cmdbatch);
		mutex_lock(&drawctxt->mutex);
	}

	mutex_unlock(&drawctxt->mutex);

	/* Give the bad news to everybody waiting around */
	wake_up_interruptible_all(&drawctxt->waiting);
	wake_up_interruptible_all(&drawctxt->wq);
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

	drawctxt = kzalloc(sizeof(struct adreno_context), GFP_KERNEL);

	if (drawctxt == NULL)
		return ERR_PTR(-ENOMEM);

	ret = kgsl_context_init(dev_priv, &drawctxt->base);
	if (ret != 0) {
		kfree(drawctxt);
		return ERR_PTR(ret);
	}

	drawctxt->bin_base_offset = 0;
	drawctxt->timestamp = 0;

	*flags &= (KGSL_CONTEXT_PREAMBLE |
		KGSL_CONTEXT_NO_GMEM_ALLOC |
		KGSL_CONTEXT_PER_CONTEXT_TS |
		KGSL_CONTEXT_USER_GENERATED_TS |
		KGSL_CONTEXT_NO_FAULT_TOLERANCE |
		KGSL_CONTEXT_TYPE_MASK);

	/* Always enable per-context timestamps */
	*flags |= KGSL_CONTEXT_PER_CONTEXT_TS;
	drawctxt->flags |= CTXT_FLAGS_PER_CONTEXT_TS;

	if (*flags & KGSL_CONTEXT_PREAMBLE)
		drawctxt->flags |= CTXT_FLAGS_PREAMBLE;

	if (*flags & KGSL_CONTEXT_NO_GMEM_ALLOC)
		drawctxt->flags |= CTXT_FLAGS_NOGMEMALLOC;

	if (*flags & KGSL_CONTEXT_USER_GENERATED_TS)
		drawctxt->flags |= CTXT_FLAGS_USER_GENERATED_TS;

	mutex_init(&drawctxt->mutex);
	init_waitqueue_head(&drawctxt->wq);
	init_waitqueue_head(&drawctxt->waiting);

	/*
	 * Set up the plist node for the dispatcher.  For now all contexts have
	 * the same priority, but later the priority will be set at create time
	 * by the user
	 */

	plist_node_init(&drawctxt->pending, ADRENO_CONTEXT_DEFAULT_PRIORITY);

	if (*flags & KGSL_CONTEXT_NO_FAULT_TOLERANCE)
		drawctxt->flags |= CTXT_FLAGS_NO_FAULT_TOLERANCE;

	drawctxt->type =
		(*flags & KGSL_CONTEXT_TYPE_MASK) >> KGSL_CONTEXT_TYPE_SHIFT;

	ret = adreno_dev->gpudev->ctxt_create(adreno_dev, drawctxt);
	if (ret)
		goto err;

	kgsl_sharedmem_writel(device, &device->memstore,
			KGSL_MEMSTORE_OFFSET(drawctxt->base.id, soptimestamp),
			0);
	kgsl_sharedmem_writel(device, &device->memstore,
			KGSL_MEMSTORE_OFFSET(drawctxt->base.id, eoptimestamp),
			0);

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
	if (adreno_dev->drawctxt_active == drawctxt) {
		/* no need to save GMEM or shader, the context is
		 * being destroyed.
		 */
		drawctxt->flags &= ~(CTXT_FLAGS_GMEM_SAVE |
				     CTXT_FLAGS_SHADER_SAVE |
				     CTXT_FLAGS_GMEM_SHADOW |
				     CTXT_FLAGS_STATE_SHADOW);

		drawctxt->flags |= CTXT_FLAGS_BEING_DESTROYED;

		adreno_drawctxt_switch(adreno_dev, NULL, 0);
	}

	mutex_lock(&drawctxt->mutex);

	while (drawctxt->cmdqueue_head != drawctxt->cmdqueue_tail) {
		struct kgsl_cmdbatch *cmdbatch =
			drawctxt->cmdqueue[drawctxt->cmdqueue_head];

		drawctxt->cmdqueue_head = (drawctxt->cmdqueue_head + 1) %
			ADRENO_CONTEXT_CMDQUEUE_SIZE;

		mutex_unlock(&drawctxt->mutex);

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

	kgsl_sharedmem_writel(device, &device->memstore,
			KGSL_MEMSTORE_OFFSET(context->id, soptimestamp),
			drawctxt->timestamp);

	kgsl_sharedmem_writel(device, &device->memstore,
			KGSL_MEMSTORE_OFFSET(context->id, eoptimestamp),
			drawctxt->timestamp);

	adreno_profile_process_results(device);

	kgsl_sharedmem_free(&drawctxt->gpustate);
	kgsl_sharedmem_free(&drawctxt->context_gmem_shadow.gmemshadow);

	/* wake threads waiting to submit commands from this context */
	wake_up_interruptible_all(&drawctxt->waiting);
	wake_up_interruptible_all(&drawctxt->wq);

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
 * adreno_drawctxt_set_bin_base_offset - set bin base offset for the context
 * @device - KGSL device that owns the context
 * @context- Generic KGSL context container for the context
 * @offset - Offset to set
 *
 * Set the bin base offset for A2XX devices.  Not valid for A3XX devices.
 */

void adreno_drawctxt_set_bin_base_offset(struct kgsl_device *device,
				      struct kgsl_context *context,
				      unsigned int offset)
{
	struct adreno_context *drawctxt;

	if (context == NULL)
		return;
	drawctxt = ADRENO_CONTEXT(context);
	drawctxt->bin_base_offset = offset;
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

	if (drawctxt) {
		if (flags & KGSL_CONTEXT_SAVE_GMEM)
			/* Set the flag in context so that the save is done
			* when this context is switched out. */
			drawctxt->flags |= CTXT_FLAGS_GMEM_SAVE;
		else
			/* Remove GMEM saving flag from the context */
			drawctxt->flags &= ~CTXT_FLAGS_GMEM_SAVE;
	}

	/* already current? */
	if (adreno_dev->drawctxt_active == drawctxt) {
		if (adreno_dev->gpudev->ctxt_draw_workaround &&
			adreno_is_a225(adreno_dev))
				ret = adreno_dev->gpudev->ctxt_draw_workaround(
					adreno_dev, drawctxt);
		return ret;
	}

	trace_adreno_drawctxt_switch(adreno_dev->drawctxt_active,
		drawctxt, flags);

	/* Save the old context */
	if (adreno_dev->gpudev->ctxt_save) {
		ret = adreno_dev->gpudev->ctxt_save(adreno_dev,
			adreno_dev->drawctxt_active);

		if (ret) {
			KGSL_DRV_ERR(device,
				"Error in GPU context %d save: %d\n",
				adreno_dev->drawctxt_active->base.id, ret);
			return ret;
		}
	}

	/* Put the old instance of the active drawctxt */
	if (adreno_dev->drawctxt_active) {
		kgsl_context_put(&adreno_dev->drawctxt_active->base);
		adreno_dev->drawctxt_active = NULL;
	}

	/* Get a refcount to the new instance */
	if (drawctxt) {
		if (!_kgsl_context_get(&drawctxt->base))
			return -EINVAL;
	}

	/* Set the new context */
	ret = adreno_dev->gpudev->ctxt_restore(adreno_dev, drawctxt);
	if (ret) {
		KGSL_DRV_ERR(device,
			"Error in GPU context %d restore: %d\n",
			drawctxt ? drawctxt->base.id : 0, ret);
		return ret;
	}

	adreno_dev->drawctxt_active = drawctxt;
	return 0;
}
