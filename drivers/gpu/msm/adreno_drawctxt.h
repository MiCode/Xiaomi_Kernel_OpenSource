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
#ifndef __ADRENO_DRAWCTXT_H
#define __ADRENO_DRAWCTXT_H

#include "a200_reg.h"
#include "a220_reg.h"

/* Flags */

#define CTXT_FLAGS_NOT_IN_USE		0x00000000
#define CTXT_FLAGS_IN_USE		0x00000001

/* state shadow memory allocated */
#define CTXT_FLAGS_STATE_SHADOW		0x00000010

/* gmem shadow memory allocated */
#define CTXT_FLAGS_GMEM_SHADOW		0x00000100
/* gmem must be copied to shadow */
#define CTXT_FLAGS_GMEM_SAVE		0x00000200
/* gmem can be restored from shadow */
#define CTXT_FLAGS_GMEM_RESTORE		0x00000400
/* shader must be copied to shadow */
#define CTXT_FLAGS_SHADER_SAVE		0x00002000
/* shader can be restored from shadow */
#define CTXT_FLAGS_SHADER_RESTORE	0x00004000
/* Context has caused a GPU hang */
#define CTXT_FLAGS_GPU_HANG		0x00008000

struct kgsl_device;
struct adreno_device;
struct kgsl_device_private;
struct kgsl_context;

/* draw context */
struct gmem_shadow_t {
	struct kgsl_memdesc gmemshadow;	/* Shadow buffer address */

	/* 256 KB GMEM surface = 4 bytes-per-pixel x 256 pixels/row x
	* 256 rows. */
	/* width & height must be a multiples of 32, in case tiled textures
	 * are used. */
	enum COLORFORMATX format;
	unsigned int size;	/* Size of surface used to store GMEM */
	unsigned int width;	/* Width of surface used to store GMEM */
	unsigned int height;	/* Height of surface used to store GMEM */
	unsigned int pitch;	/* Pitch of surface used to store GMEM */
	unsigned int gmem_pitch;	/* Pitch value used for GMEM */
	unsigned int *gmem_save_commands;
	unsigned int *gmem_restore_commands;
	unsigned int gmem_save[3];
	unsigned int gmem_restore[3];
	struct kgsl_memdesc quad_vertices;
	struct kgsl_memdesc quad_texcoords;
};

struct adreno_context {
	uint32_t flags;
	struct kgsl_pagetable *pagetable;
	struct kgsl_memdesc gpustate;
	unsigned int reg_save[3];
	unsigned int reg_restore[3];
	unsigned int shader_save[3];
	unsigned int shader_fixup[3];
	unsigned int shader_restore[3];
	unsigned int chicken_restore[3];
	unsigned int bin_base_offset;
	/* Information of the GMEM shadow that is created in context create */
	struct gmem_shadow_t context_gmem_shadow;
};

int adreno_drawctxt_create(struct kgsl_device *device,
			struct kgsl_pagetable *pagetable,
			struct kgsl_context *context,
			uint32_t flags);

void adreno_drawctxt_destroy(struct kgsl_device *device,
			  struct kgsl_context *context);

void adreno_drawctxt_switch(struct adreno_device *adreno_dev,
				struct adreno_context *drawctxt,
				unsigned int flags);
void adreno_drawctxt_set_bin_base_offset(struct kgsl_device *device,
				      struct kgsl_context *context,
					unsigned int offset);

#endif  /* __ADRENO_DRAWCTXT_H */
