/* Copyright (c) 2002,2007-2011, Code Aurora Forum. All rights reserved.
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

#include "kgsl.h"
#include "kgsl_sharedmem.h"
#include "adreno.h"

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

/**
 * adreno_drawctxt_create - create a new adreno draw context
 * @device - KGSL device to create the context on
 * @pagetable - Pagetable for the context
 * @context- Generic KGSL context structure
 * @flags - flags for the context (passed from user space)
 *
 * Create a new draw context for the 3D core.  Return 0 on success,
 * or error code on failure.
 */
int adreno_drawctxt_create(struct kgsl_device *device,
			struct kgsl_pagetable *pagetable,
			struct kgsl_context *context, uint32_t flags)
{
	struct adreno_context *drawctxt;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int ret;

	drawctxt = kzalloc(sizeof(struct adreno_context), GFP_KERNEL);

	if (drawctxt == NULL)
		return -ENOMEM;

	drawctxt->pagetable = pagetable;
	drawctxt->bin_base_offset = 0;

	/* FIXME: Deal with preambles */

	ret = adreno_dev->gpudev->ctxt_gpustate_shadow(adreno_dev, drawctxt);
	if (ret)
		goto err;

	/* Save the shader instruction memory on context switching */
	drawctxt->flags |= CTXT_FLAGS_SHADER_SAVE;

	if (!(flags & KGSL_CONTEXT_NO_GMEM_ALLOC)) {
		/* create gmem shadow */
		ret = adreno_dev->gpudev->ctxt_gmem_shadow(adreno_dev,
			drawctxt);
		if (ret != 0)
			goto err;
	}

	context->devctxt = drawctxt;
	return 0;
err:
	kgsl_sharedmem_free(&drawctxt->gpustate);
	kfree(drawctxt);
	return ret;
}

/**
 * adreno_drawctxt_destroy - destroy a draw context
 * @device - KGSL device that owns the context
 * @context- Generic KGSL context container for the context
 *
 * Destroy an existing context.  Return 0 on success or error
 * code on failure.
 */

/* destroy a drawing context */

void adreno_drawctxt_destroy(struct kgsl_device *device,
			  struct kgsl_context *context)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_context *drawctxt = context->devctxt;

	if (drawctxt == NULL)
		return;

	/* deactivate context */
	if (adreno_dev->drawctxt_active == drawctxt) {
		/* no need to save GMEM or shader, the context is
		 * being destroyed.
		 */
		drawctxt->flags &= ~(CTXT_FLAGS_GMEM_SAVE |
				     CTXT_FLAGS_SHADER_SAVE |
				     CTXT_FLAGS_GMEM_SHADOW |
				     CTXT_FLAGS_STATE_SHADOW);

		adreno_drawctxt_switch(adreno_dev, NULL, 0);
	}

	adreno_idle(device, KGSL_TIMEOUT_DEFAULT);

	kgsl_sharedmem_free(&drawctxt->gpustate);
	kgsl_sharedmem_free(&drawctxt->context_gmem_shadow.gmemshadow);

	kfree(drawctxt);
	context->devctxt = NULL;
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
	struct adreno_context *drawctxt = context->devctxt;

	if (drawctxt)
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

void adreno_drawctxt_switch(struct adreno_device *adreno_dev,
				struct adreno_context *drawctxt,
				unsigned int flags)
{
	struct kgsl_device *device = &adreno_dev->dev;

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
	if (adreno_dev->drawctxt_active == drawctxt)
		return;

	KGSL_CTXT_INFO(device, "from %p to %p flags %d\n",
			adreno_dev->drawctxt_active, drawctxt, flags);

	/* Save the old context */
	adreno_dev->gpudev->ctxt_save(adreno_dev, adreno_dev->drawctxt_active);

	/* Set the new context */
	adreno_dev->drawctxt_active = drawctxt;
	adreno_dev->gpudev->ctxt_restore(adreno_dev, drawctxt);
}
