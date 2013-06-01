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

#include "kgsl.h"
#include "kgsl_sharedmem.h"
#include "adreno.h"

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

	if (*flags & KGSL_CONTEXT_PREAMBLE)
		drawctxt->flags |= CTXT_FLAGS_PREAMBLE;

	if (*flags & KGSL_CONTEXT_NO_GMEM_ALLOC)
		drawctxt->flags |= CTXT_FLAGS_NOGMEMALLOC;

	if (*flags & KGSL_CONTEXT_PER_CONTEXT_TS)
		drawctxt->flags |= CTXT_FLAGS_PER_CONTEXT_TS;

	if (*flags & KGSL_CONTEXT_USER_GENERATED_TS) {
		if (!(*flags & KGSL_CONTEXT_PER_CONTEXT_TS)) {
			ret = -EINVAL;
			goto err;
		}
		drawctxt->flags |= CTXT_FLAGS_USER_GENERATED_TS;
	}

	if (*flags & KGSL_CONTEXT_NO_FAULT_TOLERANCE)
		drawctxt->flags |= CTXT_FLAGS_NO_FAULT_TOLERANCE;

	drawctxt->type =
		(*flags & KGSL_CONTEXT_TYPE_MASK) >> KGSL_CONTEXT_TYPE_SHIFT;

	ret = adreno_dev->gpudev->ctxt_create(adreno_dev, drawctxt);
	if (ret)
		goto err;

	kgsl_sharedmem_writel(device, &device->memstore,
			KGSL_MEMSTORE_OFFSET(drawctxt->base.id, ref_wait_ts),
			KGSL_INIT_REFTIMESTAMP);
	kgsl_sharedmem_writel(device, &device->memstore,
			KGSL_MEMSTORE_OFFSET(drawctxt->base.id, ts_cmp_enable),
			0);
	kgsl_sharedmem_writel(device, &device->memstore,
			KGSL_MEMSTORE_OFFSET(drawctxt->base.id, soptimestamp),
			0);
	kgsl_sharedmem_writel(device, &device->memstore,
			KGSL_MEMSTORE_OFFSET(drawctxt->base.id, eoptimestamp),
			0);

	return &drawctxt->base;
err:
	kgsl_context_put(&drawctxt->base);
	return ERR_PTR(ret);
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

	if (context == NULL)
		return;

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

	if (device->state != KGSL_STATE_HUNG)
		adreno_idle(device);

	adreno_profile_process_results(device);

	kgsl_sharedmem_free(&drawctxt->gpustate);
	kgsl_sharedmem_free(&drawctxt->context_gmem_shadow.gmemshadow);
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
	if (adreno_dev->drawctxt_active == drawctxt) {
		if (adreno_dev->gpudev->ctxt_draw_workaround &&
			adreno_is_a225(adreno_dev))
				adreno_dev->gpudev->ctxt_draw_workaround(
					adreno_dev, drawctxt);
		return;
	}

	KGSL_CTXT_INFO(device, "from %d to %d flags %d\n",
		adreno_dev->drawctxt_active ?
		adreno_dev->drawctxt_active->base.id : 0,
		drawctxt ? drawctxt->base.id : 0, flags);

	/* Save the old context */
	adreno_dev->gpudev->ctxt_save(adreno_dev, adreno_dev->drawctxt_active);

	/* Put the old instance of the active drawctxt */
	if (adreno_dev->drawctxt_active) {
		kgsl_context_put(&adreno_dev->drawctxt_active->base);
		adreno_dev->drawctxt_active = NULL;
	}

	/* Get a refcount to the new instance */
	if (drawctxt)
		_kgsl_context_get(&drawctxt->base);

	/* Set the new context */
	adreno_dev->gpudev->ctxt_restore(adreno_dev, drawctxt);
	adreno_dev->drawctxt_active = drawctxt;
}
