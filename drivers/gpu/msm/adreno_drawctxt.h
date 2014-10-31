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
#ifndef __ADRENO_DRAWCTXT_H
#define __ADRENO_DRAWCTXT_H

#include "adreno_pm4types.h"
#include "a2xx_reg.h"


/* Symbolic table for the adreno draw context type */
#define ADRENO_DRAWCTXT_TYPES \
	{ KGSL_CONTEXT_TYPE_ANY, "any" }, \
	{ KGSL_CONTEXT_TYPE_GL, "GL" }, \
	{ KGSL_CONTEXT_TYPE_CL, "CL" }, \
	{ KGSL_CONTEXT_TYPE_C2D, "C2D" }, \
	{ KGSL_CONTEXT_TYPE_RS, "RS" }, \
	{ KGSL_CONTEXT_TYPE_UNKNOWN, "UNKNOWN" }

struct adreno_context_type {
	unsigned int type;
	const char *str;
};

#define ADRENO_CONTEXT_CMDQUEUE_SIZE 128

#define ADRENO_CONTEXT_DEFAULT_PRIORITY 1

#define ADRENO_CONTEXT_STATE_ACTIVE 0
#define ADRENO_CONTEXT_STATE_INVALID 1

struct kgsl_device;
struct adreno_device;
struct kgsl_device_private;
struct kgsl_context;

/* draw context */
struct gmem_shadow_t {
	struct kgsl_memdesc gmemshadow;	/* Shadow buffer address */

	/*
	 * 256 KB GMEM surface = 4 bytes-per-pixel x 256 pixels/row x
	 * 256 rows. Width & height must be multiples of 32 in case tiled
	 * textures are used
	*/

	enum COLORFORMATX format; /* Unused on A3XX */
	unsigned int size;	/* Size of surface used to store GMEM */
	unsigned int width;	/* Width of surface used to store GMEM */
	unsigned int height;	/* Height of surface used to store GMEM */
	unsigned int pitch;	/* Pitch of surface used to store GMEM */
	unsigned int gmem_pitch;	/* Pitch value used for GMEM */
	unsigned int *gmem_save_commands;    /* Unused on A3XX */
	unsigned int *gmem_restore_commands; /* Unused on A3XX */
	unsigned int gmem_save[3];
	unsigned int gmem_restore[3];
	struct kgsl_memdesc quad_vertices;
	struct kgsl_memdesc quad_texcoords;
	struct kgsl_memdesc quad_vertices_restore;
};

struct adreno_context;

/**
 * struct adreno_context_ops - context state management functions
 * @save: optional hook for saving context state
 * @restore: required hook for restoring state,
 *		adreno_context_restore() may be used directly here.
 * @draw_workaround: optional hook for a workaround after every IB
 * @detach: optional hook for freeing state tracking memory.
 */
struct adreno_context_ops {
	int (*save)(struct adreno_device *, struct adreno_context *);
	int (*restore)(struct adreno_device *, struct adreno_context *);
	int (*draw_workaround)(struct adreno_device *,
				struct adreno_context *);
	void (*detach)(struct adreno_context *);
};

int adreno_context_restore(struct adreno_device *, struct adreno_context *);

/* generic context ops for preamble context switch */
extern const struct adreno_context_ops adreno_preamble_ctx_ops;

/**
 * struct adreno_context - Adreno GPU draw context
 * @id: Unique integer ID of the context
 * @timestamp: Last issued context-specific timestamp
 * @internal_timestamp: Global timestamp of the last issued command
 *			NOTE: guarded by device->mutex, not drawctxt->mutex!
 * @state: Current state of the context
 * @priv: Internal flags
 * @type: Context type (GL, CL, RS)
 * @mutex: Mutex to protect the cmdqueue
 * @pagetable: Pointer to the GPU pagetable for the context
 * @gpustate: Pointer to the GPU scratch memory for context save/restore
 * @reg_restore: Command buffer for restoring context registers
 * @shader_save: Command buffer for saving shaders
 * @shader_restore: Command buffer to restore shaders
 * @context_gmem_shadow: GMEM shadow structure for save/restore
 * @reg_save: A2XX command buffer to save context registers
 * @shader_fixup: A2XX command buffer to "fix" shaders on restore
 * @chicken_restore: A2XX command buffer to "fix" register restore
 * @bin_base_offset: Saved value of the A2XX BIN_BASE_OFFSET register
 * @regconstant_save: A3XX command buffer to save some registers
 * @constant_retore: A3XX command buffer to restore some registers
 * @hslqcontrol_restore: A3XX command buffer to restore HSLSQ registers
 * @save_fixup: A3XX command buffer to "fix" register save
 * @restore_fixup: A3XX cmmand buffer to restore register save fixes
 * @shader_load_commands: A3XX GPU memory descriptor for shader load IB
 * @shader_save_commands: A3XX GPU memory descriptor for shader save IB
 * @constantr_save_commands: A3XX GPU memory descriptor for constant save IB
 * @constant_load_commands: A3XX GPU memory descriptor for constant load IB
 * @cond_execs: A3XX GPU memory descriptor for conditional exec IB
 * @hlsq_restore_commands: A3XX GPU memory descriptor for HLSQ restore IB
 * @cmdqueue: Queue of command batches waiting to be dispatched for this context
 * @cmdqueue_head: Head of the cmdqueue queue
 * @cmdqueue_tail: Tail of the cmdqueue queue
 * @pending: Priority list node for the dispatcher list of pending contexts
 * @wq: Workqueue structure for contexts to sleep pending room in the queue
 * @waiting: Workqueue structure for contexts waiting for a timestamp or event
 * @queued: Number of commands queued in the cmdqueue
 * @ops: Context switch functions for this context.
 * @fault_policy: GFT fault policy set in cmdbatch_skip_cmd();
 * @queued_timestamp: The last timestamp that was queued on this context
 * @submitted_timestamp: The last timestamp that was submitted for this context
 */
struct adreno_context {
	struct kgsl_context base;
	unsigned int timestamp;
	unsigned int internal_timestamp;
	int state;
	unsigned long priv;
	unsigned int type;
	struct mutex mutex;
	struct kgsl_memdesc gpustate;
	unsigned int reg_restore[3];
	unsigned int shader_save[3];
	unsigned int shader_restore[3];

	struct gmem_shadow_t context_gmem_shadow;

	/* A2XX specific items */
	unsigned int reg_save[3];
	unsigned int shader_fixup[3];
	unsigned int chicken_restore[3];
	unsigned int bin_base_offset;

	/* A3XX specific items */
	unsigned int regconstant_save[3];
	unsigned int constant_restore[3];
	unsigned int hlsqcontrol_restore[3];
	unsigned int save_fixup[3];
	unsigned int restore_fixup[3];
	struct kgsl_memdesc shader_load_commands[2];
	struct kgsl_memdesc shader_save_commands[4];
	struct kgsl_memdesc constant_save_commands[3];
	struct kgsl_memdesc constant_load_commands[3];
	struct kgsl_memdesc cond_execs[4];
	struct kgsl_memdesc hlsqcontrol_restore_commands[1];

	/* Dispatcher */
	struct kgsl_cmdbatch *cmdqueue[ADRENO_CONTEXT_CMDQUEUE_SIZE];
	unsigned int cmdqueue_head;
	unsigned int cmdqueue_tail;

	struct plist_node pending;
	wait_queue_head_t wq;
	wait_queue_head_t waiting;

	int queued;

	const struct adreno_context_ops *ops;
	unsigned int fault_policy;
	unsigned int queued_timestamp;
	unsigned int submitted_timestamp;
};

/**
 * enum adreno_context_priv - Private flags for an adreno draw context
 * @ADRENO_CONTEXT_FAULT - set if the context has faulted (and recovered)
 * @ADRENO_CONTEXT_GMEM_SAVE - gmem must be copied to shadow
 * @ADRENO_CONTEXT_GMEM_RESTORE - gmem can be restored from shadow
 * @ADRENO_CONTEXT_SHADER_SAVE - shader must be copied to shadow
 * @ADRENO_CONTEXT_SHADER_RESTORE - shader can be restored from shadow
 * @ADRENO_CONTEXT_GPU_HANG - Context has caused a GPU hang
 * @ADRENO_CONTEXT_GPU_HANG_FT - Context has caused a GPU hang
 *      and fault tolerance was successful
 * @ADRENO_CONTEXT_SKIP_EOF - Context skip IBs until the next end of frame
 *      marker.
 * @ADRENO_CONTEXT_FORCE_PREAMBLE - Force the preamble for the next submission.
 * @ADRENO_CONTEXT_SKIP_CMD - Context's command batch is skipped during
	fault tolerance.
 */
enum adreno_context_priv {
	ADRENO_CONTEXT_FAULT = 0,
	ADRENO_CONTEXT_GMEM_SAVE,
	ADRENO_CONTEXT_GMEM_RESTORE,
	ADRENO_CONTEXT_SHADER_SAVE,
	ADRENO_CONTEXT_SHADER_RESTORE,
	ADRENO_CONTEXT_GPU_HANG,
	ADRENO_CONTEXT_GPU_HANG_FT,
	ADRENO_CONTEXT_SKIP_EOF,
	ADRENO_CONTEXT_FORCE_PREAMBLE,
	ADRENO_CONTEXT_SKIP_CMD,
};

struct kgsl_context *adreno_drawctxt_create(struct kgsl_device_private *,
			uint32_t *flags);

int adreno_drawctxt_detach(struct kgsl_context *context);

void adreno_drawctxt_destroy(struct kgsl_context *context);

void adreno_drawctxt_sched(struct kgsl_device *device,
		struct kgsl_context *context);

int adreno_drawctxt_switch(struct adreno_device *adreno_dev,
				struct adreno_context *drawctxt,
				unsigned int flags);
void adreno_drawctxt_set_bin_base_offset(struct kgsl_device *device,
					struct kgsl_context *context,
					unsigned int offset);

int adreno_drawctxt_wait(struct adreno_device *adreno_dev,
		struct kgsl_context *context,
		uint32_t timestamp, unsigned int timeout);

void adreno_drawctxt_invalidate(struct kgsl_device *device,
		struct kgsl_context *context);

/* GPU context switch helper functions */

void build_quad_vtxbuff(struct adreno_context *drawctxt,
		struct gmem_shadow_t *shadow, unsigned int **incmd);

unsigned int uint2float(unsigned int);

static inline unsigned int virt2gpu(unsigned int *cmd,
				    struct kgsl_memdesc *memdesc)
{
	return memdesc->gpuaddr + ((char *) cmd - (char *) memdesc->hostptr);
}

static inline void create_ib1(struct adreno_context *drawctxt,
			      unsigned int *cmd,
			      unsigned int *start,
			      unsigned int *end)
{
	cmd[0] = CP_HDR_INDIRECT_BUFFER_PFD;
	cmd[1] = virt2gpu(start, &drawctxt->gpustate);
	cmd[2] = end - start;
}


static inline unsigned int *reg_range(unsigned int *cmd, unsigned int start,
	unsigned int end)
{
	*cmd++ = CP_REG(start);		/* h/w regs, start addr */
	*cmd++ = end - start + 1;	/* count */
	return cmd;
}

static inline void calc_gmemsize(struct gmem_shadow_t *shadow, int gmem_size)
{
	int w = 64, h = 64;

	shadow->format = COLORX_8_8_8_8;

	/* convert from bytes to 32-bit words */
	gmem_size = (gmem_size + 3) / 4;

	while ((w * h) < gmem_size) {
		if (w < h)
			w *= 2;
		else
			h *= 2;
	}

	shadow->pitch = shadow->width = w;
	shadow->height = h;
	shadow->gmem_pitch = shadow->pitch;
	shadow->size = shadow->pitch * shadow->height * 4;
}

void adreno_drawctxt_dump(struct kgsl_device *device,
		struct kgsl_context *context);

#endif  /* __ADRENO_DRAWCTXT_H */
