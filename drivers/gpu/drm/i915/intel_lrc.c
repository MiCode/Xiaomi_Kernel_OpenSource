/*
 * Copyright © 2014 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Ben Widawsky <ben@bwidawsk.net>
 *    Michel Thierry <michel.thierry@intel.com>
 *    Thomas Daniel <thomas.daniel@intel.com>
 *    Oscar Mateo <oscar.mateo@intel.com>
 *
 */

/**
 * DOC: Logical Rings, Logical Ring Contexts and Execlists
 *
 * Motivation:
 * GEN8 brings an expansion of the HW contexts: "Logical Ring Contexts".
 * These expanded contexts enable a number of new abilities, especially
 * "Execlists" (also implemented in this file).
 *
 * One of the main differences with the legacy HW contexts is that logical
 * ring contexts incorporate many more things to the context's state, like
 * PDPs or ringbuffer control registers:
 *
 * The reason why PDPs are included in the context is straightforward: as
 * PPGTTs (per-process GTTs) are actually per-context, having the PDPs
 * contained there mean you don't need to do a ppgtt->switch_mm yourself,
 * instead, the GPU will do it for you on the context switch.
 *
 * But, what about the ringbuffer control registers (head, tail, etc..)?
 * shouldn't we just need a set of those per engine command streamer? This is
 * where the name "Logical Rings" starts to make sense: by virtualizing the
 * rings, the engine cs shifts to a new "ring buffer" with every context
 * switch. When you want to submit a workload to the GPU you: A) choose your
 * context, B) find its appropriate virtualized ring, C) write commands to it
 * and then, finally, D) tell the GPU to switch to that context.
 *
 * Instead of the legacy MI_SET_CONTEXT, the way you tell the GPU to switch
 * to a contexts is via a context execution list, ergo "Execlists".
 *
 * LRC implementation:
 * Regarding the creation of contexts, we have:
 *
 * - One global default context.
 * - One local default context for each opened fd.
 * - One local extra context for each context create ioctl call.
 *
 * Now that ringbuffers belong per-context (and not per-engine, like before)
 * and that contexts are uniquely tied to a given engine (and not reusable,
 * like before) we need:
 *
 * - One ringbuffer per-engine inside each context.
 * - One backing object per-engine inside each context.
 *
 * The global default context starts its life with these new objects fully
 * allocated and populated. The local default context for each opened fd is
 * more complex, because we don't know at creation time which engine is going
 * to use them. To handle this, we have implemented a deferred creation of LR
 * contexts:
 *
 * The local context starts its life as a hollow or blank holder, that only
 * gets populated for a given engine once we receive an execbuffer. If later
 * on we receive another execbuffer ioctl for the same context but a different
 * engine, we allocate/populate a new ringbuffer and context backing object and
 * so on.
 *
 * Finally, regarding local contexts created using the ioctl call: as they are
 * only allowed with the render ring, we can allocate & populate them right
 * away (no need to defer anything, at least for now).
 *
 * Execlists implementation:
 * Execlists are the new method by which, on gen8+ hardware, workloads are
 * submitted for execution (as opposed to the legacy, ringbuffer-based, method).
 * This method works as follows:
 *
 * When a request is committed, its commands (the BB start and any leading or
 * trailing commands, like the seqno breadcrumbs) are placed in the ringbuffer
 * for the appropriate context. The tail pointer in the hardware context is not
 * updated at this time, but instead, kept by the driver in the ringbuffer
 * structure. A structure representing this request is added to a request queue
 * for the appropriate engine: this structure contains a copy of the context's
 * tail after the request was written to the ring buffer and a pointer to the
 * context itself.
 *
 * If the engine's request queue was empty before the request was added, the
 * queue is processed immediately. Otherwise the queue will be processed during
 * a context switch interrupt. In any case, elements on the queue will get sent
 * (in pairs) to the GPU's ExecLists Submit Port (ELSP, for short) with a
 * globally unique 20-bits submission ID.
 *
 * When execution of a request completes, the GPU updates the context status
 * buffer with a context complete event and generates a context switch interrupt.
 * During the interrupt handling, the driver examines the events in the buffer:
 * for each context complete event, if the announced ID matches that on the head
 * of the request queue, then that request is retired and removed from the queue.
 *
 * After processing, if any requests were retired and the queue is not empty
 * then a new execution list can be submitted. The two requests at the front of
 * the queue are next to be submitted but since a context may not occur twice in
 * an execution list, if subsequent requests have the same ID as the first then
 * the two requests must be combined. This is done simply by discarding requests
 * at the head of the queue until either only one requests is left (in which case
 * we use a NULL second context) or the first two requests have unique IDs.
 *
 * By always executing the first two requests in the queue the driver ensures
 * that the GPU is kept as busy as possible. In the case where a single context
 * completes but a second context is still executing, the request for this second
 * context will be at the head of the queue when we remove the first one. This
 * request will then be resubmitted along with a new request for a different context,
 * which will cause the hardware to continue executing the second request and queue
 * the new request (the GPU detects the condition of a context getting preempted
 * with the same context and optimizes the context switch flow by not doing
 * preemption, but just sampling the new tail pointer).
 *
 */

#include <linux/syscalls.h>
#include <drm/drmP.h>
#include <drm/i915_drm.h>
#include "i915_drv.h"
#include "intel_sync.h"
#include "intel_lrc_tdr.h"
#include "i915_scheduler.h"

#define GEN8_LR_CONTEXT_RENDER_SIZE (20 * PAGE_SIZE)
#define GEN8_LR_CONTEXT_OTHER_SIZE (2 * PAGE_SIZE)

#define RING_EXECLIST_QFULL		(1 << 0x2)
#define RING_EXECLIST1_VALID		(1 << 0x3)
#define RING_EXECLIST0_VALID		(1 << 0x4)
#define RING_EXECLIST_ACTIVE_STATUS	(3 << 0xE)
#define RING_EXECLIST1_ACTIVE		(1 << 0x11)
#define RING_EXECLIST0_ACTIVE		(1 << 0x12)

#define GEN8_CTX_STATUS_IDLE_ACTIVE	(1 << 0)
#define GEN8_CTX_STATUS_PREEMPTED	(1 << 1)
#define GEN8_CTX_STATUS_ELEMENT_SWITCH	(1 << 2)
#define GEN8_CTX_STATUS_ACTIVE_IDLE	(1 << 3)
#define GEN8_CTX_STATUS_COMPLETE	(1 << 4)
#define GEN8_CTX_STATUS_LITE_RESTORE	(1 << 15)

#define CTX_LRI_HEADER_0		0x01
#define CTX_CONTEXT_CONTROL		0x02
#define CTX_RING_HEAD			0x04
#define CTX_RING_TAIL			0x06
#define CTX_RING_BUFFER_START		0x08
#define CTX_RING_BUFFER_CONTROL		0x0a
#define CTX_BB_HEAD_U			0x0c
#define CTX_BB_HEAD_L			0x0e
#define CTX_BB_STATE			0x10
#define CTX_SECOND_BB_HEAD_U		0x12
#define CTX_SECOND_BB_HEAD_L		0x14
#define CTX_SECOND_BB_STATE		0x16
#define CTX_BB_PER_CTX_PTR		0x18
#define CTX_RCS_INDIRECT_CTX		0x1a
#define CTX_RCS_INDIRECT_CTX_OFFSET	0x1c
#define CTX_LRI_HEADER_1		0x21
#define CTX_CTX_TIMESTAMP		0x22
#define CTX_PDP3_UDW			0x24
#define CTX_PDP3_LDW			0x26
#define CTX_PDP2_UDW			0x28
#define CTX_PDP2_LDW			0x2a
#define CTX_PDP1_UDW			0x2c
#define CTX_PDP1_LDW			0x2e
#define CTX_PDP0_UDW			0x30
#define CTX_PDP0_LDW			0x32
#define CTX_LRI_HEADER_2		0x41
#define CTX_R_PWR_CLK_STATE		0x42
#define CTX_GPGPU_CSR_BASE_ADDRESS	0x44

#define GEN8_CTX_VALID (1<<0)
#define GEN8_CTX_FORCE_PD_RESTORE (1<<1)
#define GEN8_CTX_FORCE_RESTORE (1<<2)
#define GEN8_CTX_L3LLC_COHERENT (1<<5)
#define GEN8_CTX_PRIVILEGE (1<<8)
enum {
	ADVANCED_CONTEXT = 0,
	LEGACY_CONTEXT,
	ADVANCED_AD_CONTEXT,
	LEGACY_64B_CONTEXT
};
#define GEN8_CTX_MODE_SHIFT 3
enum {
	FAULT_AND_HANG = 0,
	FAULT_AND_HALT, /* Debug only */
	FAULT_AND_STREAM,
	FAULT_AND_CONTINUE /* Unsupported */
};
#define GEN8_CTX_ID_SHIFT 32
#define CTX_RCS_INDIRECT_CTX_OFFSET_DEFAULT  0x17

static int intel_lr_context_pin(struct intel_engine_cs *ring,
		struct intel_context *ctx);

/* Test to see if the ring has sufficient space to submit a given piece of work
 * without causing a stall */
static int logical_ring_test_space(struct intel_ringbuffer *ringbuf, int min_space)
{
	//struct intel_engine_cs *ring = ringbuf->ring;
	//struct drm_device *dev = ring->dev;
	//struct drm_i915_private *dev_priv = dev->dev_private;

	if (ringbuf->space < min_space) {
		/* Need to update the actual ring space. Otherwise, the system
		 * hangs forever testing a software copy of the space value that
		 * never changes!
		 */
		//ringbuf->head  = I915_READ_HEAD(ring);
		//ringbuf->space = intel_ring_space(ringbuf);
		intel_ring_update_space(ringbuf);

		if (ringbuf->space < min_space)
			return -EAGAIN;
	}

	return 0;
}

/**
 * intel_sanitize_enable_execlists() - sanitize i915.enable_execlists
 * @dev: DRM device.
 * @enable_execlists: value of i915.enable_execlists module parameter.
 *
 * Only certain platforms support Execlists (the prerequisites being
 * support for Logical Ring Contexts and Aliasing PPGTT or better),
 * and only when enabled via module parameter.
 *
 * Return: 1 if Execlists is supported and has to be enabled.
 */
int intel_sanitize_enable_execlists(struct drm_device *dev, int enable_execlists)
{
	if (enable_execlists == 0)
		return 0;

	if (HAS_LOGICAL_RING_CONTEXTS(dev) && USES_PPGTT(dev) &&
	    i915.use_mmio_flip >= 0)
		return 1;

	return 0;
}

/**
 * intel_execlists_ctx_id() - get the Execlists Context ID
 * @ctx_obj: Logical Ring Context backing object.
 *
 * Do not confuse with ctx->id! Unfortunately we have a name overload
 * here: the old context ID we pass to userspace as a handler so that
 * they can refer to a context, and the new context ID we pass to the
 * ELSP so that the GPU can inform us of the context status via
 * interrupts.
 *
 * Return: 20-bits globally unique context ID.
 */
u32 intel_execlists_ctx_id(struct drm_i915_gem_object *ctx_obj)
{
	u32 lrca = i915_gem_obj_ggtt_offset(ctx_obj);

	/* LRCA is required to be 4K aligned so the more significant 20 bits
	 * are globally unique */
	return lrca >> 12;
}

static uint64_t execlists_ctx_descriptor(struct drm_i915_gem_object *ctx_obj)
{
	uint64_t desc;
	uint64_t lrca = i915_gem_obj_ggtt_offset(ctx_obj);

	WARN_ON(lrca & 0xFFFFFFFF00000FFFULL);

	desc = GEN8_CTX_VALID;
	desc |= LEGACY_CONTEXT << GEN8_CTX_MODE_SHIFT;
	desc |= GEN8_CTX_L3LLC_COHERENT;
	desc |= GEN8_CTX_PRIVILEGE;
	desc |= lrca;
	desc |= (u64)intel_execlists_ctx_id(ctx_obj) << GEN8_CTX_ID_SHIFT;

	/* TODO: WaDisableLiteRestore when we start using semaphore
	 * signalling between Command Streamers */
	/* desc |= GEN8_CTX_FORCE_RESTORE; */

	return desc;
}

static void execlists_elsp_write(struct intel_engine_cs *ring,
				 struct drm_i915_gem_object *ctx_obj0,
				 struct drm_i915_gem_object *ctx_obj1)
{
	struct drm_i915_private *dev_priv = ring->dev->dev_private;
	uint64_t temp = 0;
	uint32_t desc[4];

	/* XXX: You must always write both descriptors in the order below. */
	if (ctx_obj1)
		temp = execlists_ctx_descriptor(ctx_obj1);
	else
		temp = 0;
	desc[1] = (u32)(temp >> 32);
	desc[0] = (u32)temp;

	temp = execlists_ctx_descriptor(ctx_obj0);
	desc[3] = (u32)(temp >> 32);
	desc[2] = (u32)temp;

	/* Set Force Wakeup bit to prevent GT from entering C6 while ELSP writes
	 * are in progress.
	 *
	 * The other problem is that we can't just call gen6_gt_force_wake_get()
	 * because that function calls intel_runtime_pm_get(), which might sleep.
	 * Instead, we do the runtime_pm_get/put when creating/destroying requests.
	 */
	if (!IS_CHERRYVIEW(dev_priv->dev))
		gen8_gt_force_wake_get(dev_priv);

	I915_WRITE(RING_ELSP(ring), desc[1]);
	I915_WRITE(RING_ELSP(ring), desc[0]);
	I915_WRITE(RING_ELSP(ring), desc[3]);
	/* The context is automatically loaded after the following */
	I915_WRITE(RING_ELSP(ring), desc[2]);

	/* ELSP is a wo register, so use another nearby reg for posting instead */
	POSTING_READ(RING_EXECLIST_STATUS(ring));

	/* Release Force Wakeup (see the big comment above). */
	if (!IS_CHERRYVIEW(dev_priv->dev))
		gen8_gt_force_wake_put(dev_priv);
}

/*
 * execlist_get_context_reg_page
 *
 * Get memory page for context object belonging to context running on a given
 * engine.
 *
 * engine: engine
 * ctx: context running on engine
 * page: returned page
 *
 * Returns:
 * 0 if successful, otherwise propagates error codes.
 */
static inline int execlist_get_context_reg_page(struct intel_engine_cs *engine,
		struct intel_context *ctx,
		struct page **page)
{
	struct drm_i915_gem_object *ctx_obj;

	if (!page)
		return -EINVAL;

	if (!ctx)
		ctx = engine->default_context;

	ctx_obj = ctx->engine[engine->id].state;

	if (!ctx_obj) {
		WARN(1, "Error while getting context register page: " \
			"Context object not set up!");
		return -EINVAL;
	}

	WARN(!i915_gem_obj_is_pinned(ctx_obj),
	     "Error while getting context register page: " \
	     "Context object is not pinned!");

	*page = i915_gem_object_get_page(ctx_obj, 1);

	if (!*page) {
		WARN(1, "Error while getting context register page: " \
			"Context object page could not be resolved!");
		return -EINVAL;
	}

	return 0;
}

static inline int execlists_write_context_reg(struct intel_engine_cs *engine,
		struct intel_context *ctx, u32 ctx_reg, u32 mmio_reg_addr,
		u32 val)
{
	struct page *page = NULL;
	uint32_t *reg_state;

	int ret = execlist_get_context_reg_page(engine, ctx, &page);
	if (ret) {
		WARN(1, "Failed to write %u to register %u for %s!",
			(unsigned int) val, (unsigned int) ctx_reg,
			engine->name);
		return ret;
	}

	reg_state = kmap_atomic(page);

	WARN(reg_state[ctx_reg] != mmio_reg_addr,
	     "Context register address (%x) differs from MMIO register address (%x)!",
	     (unsigned int) reg_state[ctx_reg], (unsigned int) mmio_reg_addr);

	reg_state[ctx_reg+1] = val;
	kunmap_atomic(reg_state);

	return ret;
}

static inline int execlists_read_context_reg(struct intel_engine_cs *engine,
		struct intel_context *ctx, u32 ctx_reg, u32 mmio_reg_addr,
		u32 *val)
{
	struct page *page = NULL;
	uint32_t *reg_state;
	int ret = 0;

	if (!val)
		return -EINVAL;

	ret = execlist_get_context_reg_page(engine, ctx, &page);
	if (ret) {
		WARN(1, "Failed to read from register %u for %s!",
			(unsigned int) ctx_reg, engine->name);
		return ret;
	}

	reg_state = kmap_atomic(page);

	WARN(reg_state[ctx_reg] != mmio_reg_addr,
	     "Context register address (%x) differs from MMIO register address (%x)!",
	     (unsigned int) reg_state[ctx_reg], (unsigned int) mmio_reg_addr);

	*val = reg_state[ctx_reg+1];
	kunmap_atomic(reg_state);

	return ret;
}

/*
 * Generic macros for generating function implementation for context register
 * read/write functions.
 *
 * Macro parameters
 * ----------------
 * reg_name: Designated name of context register (e.g. tail, head, buffer_ctl)
 *
 * reg_def: Context register macro definition (e.g. CTX_RING_TAIL)
 *
 * mmio_reg_def: Name of macro function used to determine the address
 *		 of the corresponding MMIO register (e.g. RING_TAIL, RING_HEAD).
 *		 This macro function is assumed to be defined on the form of:
 *
 *			#define mmio_reg_def(base) (base+register_offset)
 *
 *		 Where "base" is the MMIO base address of the respective ring
 *		 and "register_offset" is the offset relative to "base".
 *
 * Function parameters
 * -------------------
 * engine: The engine that the context is running on
 * ctx: The context of the register that is to be accessed
 * reg_name: Value to be written/read to/from the register.
 */
#define INTEL_EXECLISTS_WRITE_REG(reg_name, reg_def, mmio_reg_def) \
	int intel_execlists_write_##reg_name(struct intel_engine_cs *engine, \
					     struct intel_context *ctx, \
					     u32 reg_name) \
{ \
	return execlists_write_context_reg(engine, ctx, (reg_def), \
			mmio_reg_def(engine->mmio_base), (reg_name)); \
}

#define INTEL_EXECLISTS_READ_REG(reg_name, reg_def, mmio_reg_def) \
	int intel_execlists_read_##reg_name(struct intel_engine_cs *engine, \
					    struct intel_context *ctx, \
					    u32 *reg_name) \
{ \
	return execlists_read_context_reg(engine, ctx, (reg_def), \
			mmio_reg_def(engine->mmio_base), (reg_name)); \
}

INTEL_EXECLISTS_WRITE_REG(tail, CTX_RING_TAIL, RING_TAIL)
INTEL_EXECLISTS_READ_REG(tail, CTX_RING_TAIL, RING_TAIL)
INTEL_EXECLISTS_WRITE_REG(head, CTX_RING_HEAD, RING_HEAD)
INTEL_EXECLISTS_READ_REG(head, CTX_RING_HEAD, RING_HEAD)
INTEL_EXECLISTS_WRITE_REG(buffer_ctl, CTX_RING_BUFFER_CONTROL, RING_CTL)
INTEL_EXECLISTS_READ_REG(buffer_ctl, CTX_RING_BUFFER_CONTROL, RING_CTL)

#undef INTEL_EXECLISTS_READ_REG
#undef INTEL_EXECLISTS_WRITE_REG

static void perfmon_send_config(
	struct intel_ringbuffer *ringbuf,
	struct drm_i915_perfmon_config *config)
{
	int i;

	for (i = 0; i < config->size; i++) {
		DRM_DEBUG("perfmon config %x reg:%05x val:%08x\n",
			config->id,
			config->entries[i].offset,
			config->entries[i].value);

		intel_logical_ring_emit(ringbuf, MI_NOOP);
		intel_logical_ring_emit(ringbuf, MI_LOAD_REGISTER_IMM(1));
		intel_logical_ring_emit(ringbuf, config->entries[i].offset);
		intel_logical_ring_emit(ringbuf, config->entries[i].value);
	}
}

static inline struct drm_i915_perfmon_config *get_perfmon_config(
	struct drm_i915_private *dev_priv,
	struct intel_context *ctx,
	struct drm_i915_perfmon_config *config_global,
	struct drm_i915_perfmon_config *config_context,
	__u32 ctx_submitted_config_id)

{
	struct drm_i915_perfmon_config *config  = NULL;
	enum DRM_I915_PERFMON_CONFIG_TARGET target;

	BUG_ON(!mutex_is_locked(&dev_priv->perfmon.config.lock));

	target = dev_priv->perfmon.config.target;
	switch (target) {
	case I915_PERFMON_CONFIG_TARGET_CTX:
		config = config_context;
		break;
	case I915_PERFMON_CONFIG_TARGET_PID:
		if (pid_vnr(ctx->pid) == dev_priv->perfmon.config.pid)
			config = config_global;
		break;
	case I915_PERFMON_CONFIG_TARGET_ALL:
		config = config_global;
		break;
	default:
		BUG_ON(1);
		break;
	}

	if (config != NULL) {
		if (config->size == 0 || config->id == 0) {
			/* configuration is empty or targets other context */
			DRM_DEBUG("perfmon configuration empty\n");
			config = NULL;
		} else if (config->id == ctx_submitted_config_id) {
			/* configuration is already submitted in this context*/
			DRM_DEBUG("perfmon configuration %x is submitted\n",
					config->id);
			config = NULL;
		}
	}

	if (config != NULL)
		DRM_DEBUG("perfmon configuration TARGET:%u SIZE:%x ID:%x",
			target,
			config->size,
			config->id);

	return config;
}

static inline int
i915_program_perfmon(struct drm_device *dev,
			struct intel_ringbuffer *ringbuf,
			struct intel_context *ctx)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_i915_perfmon_config *config_oa, *config_gp;
	size_t size;
	int ret = 0;

	if (!atomic_read(&dev_priv->perfmon.config.enable) &&
	    ctx->perfmon.config.oa.submitted_id == 0)
		return 0;

	ret = mutex_lock_interruptible(&dev_priv->perfmon.config.lock);
	if (ret)
		return ret;

	if (!atomic_read(&dev_priv->perfmon.config.enable)) {
		if (ctx->perfmon.config.oa.submitted_id != 0) {
			/* write 0 to OA_CTX_CONTROL to stop counters */
			ret = intel_logical_ring_begin(ringbuf, 4);
			if (!ret) {
				intel_logical_ring_emit(ringbuf, MI_NOOP);
				intel_logical_ring_emit(ringbuf,
					MI_LOAD_REGISTER_IMM(1));
				intel_logical_ring_emit(ringbuf,
					GEN8_OA_CTX_CONTROL);
				intel_logical_ring_emit(ringbuf, 0);
				intel_logical_ring_advance(ringbuf);
			}
			ctx->perfmon.config.oa.submitted_id = 0;
		}
		goto unlock;
	}

	/* check for pending OA config */
	config_oa = get_perfmon_config(dev_priv, ctx,
					&dev_priv->perfmon.config.oa,
					&ctx->perfmon.config.oa.pending,
					ctx->perfmon.config.oa.submitted_id);

	/* check for pending PERFMON config */
	config_gp = get_perfmon_config(dev_priv, ctx,
					&dev_priv->perfmon.config.gp,
					&ctx->perfmon.config.gp.pending,
					ctx->perfmon.config.gp.submitted_id);

	size = (config_oa ? config_oa->size : 0) +
	       (config_gp ? config_gp->size : 0);

	if (size == 0)
		goto unlock;

	ret = intel_logical_ring_begin(ringbuf, 4 * size);
	if (ret)
		goto unlock;

	/* submit pending OA config */
	if (config_oa) {
		perfmon_send_config(
			ringbuf,
			config_oa);
		ctx->perfmon.config.oa.submitted_id = config_oa->id;

		i915_perfmon_update_workaround_bb(dev_priv, config_oa);
	}


	/* submit pending general purpose perfmon counters config */
	if (config_gp) {
		perfmon_send_config(
			ringbuf,
			config_gp);
		ctx->perfmon.config.gp.submitted_id = config_gp->id;
	}

	intel_logical_ring_advance(ringbuf);

unlock:
	mutex_unlock(&dev_priv->perfmon.config.lock);
	return ret;
}

static int execlists_update_context(struct drm_i915_gem_object *ctx_obj,
				    struct drm_i915_gem_object *ring_obj,
				    u32 tail)
{
	struct page *page;
	uint32_t *reg_state;

	page = i915_gem_object_get_page(ctx_obj, 1);
	reg_state = kmap_atomic(page);

	reg_state[CTX_RING_TAIL+1] = tail;
	reg_state[CTX_RING_BUFFER_START+1] = i915_gem_obj_ggtt_offset(ring_obj);

	kunmap_atomic(reg_state);

	return 0;
}

static int execlists_submit_context(struct intel_engine_cs *ring,
				    struct intel_context *to0, u32 tail0,
				    struct intel_context *to1, u32 tail1)
{
	struct drm_i915_gem_object *ctx_obj0 = to0->engine[ring->id].state;
	struct intel_ringbuffer *ringbuf0 = to0->engine[ring->id].ringbuf;
	struct drm_i915_gem_object *ctx_obj1 = NULL;
	struct intel_ringbuffer *ringbuf1 = NULL;

	BUG_ON(!ctx_obj0);
	WARN_ON(!i915_gem_obj_is_pinned(ctx_obj0));
	WARN_ON(!i915_gem_obj_is_pinned(ringbuf0->obj));

	execlists_update_context(ctx_obj0, ringbuf0->obj, tail0);

	if (to1) {
		ringbuf1 = to1->engine[ring->id].ringbuf;
		ctx_obj1 = to1->engine[ring->id].state;
		BUG_ON(!ctx_obj1);
		WARN_ON(!i915_gem_obj_is_pinned(ctx_obj1));
		WARN_ON(!i915_gem_obj_is_pinned(ringbuf1->obj));

		execlists_update_context(ctx_obj1, ringbuf1->obj, tail1);
	}

	execlists_elsp_write(ring, ctx_obj0, ctx_obj1);

	return 0;
}

static void execlists_fetch_requests(struct intel_engine_cs *ring,
			struct intel_ctx_submit_request **req0,
			struct intel_ctx_submit_request **req1)
{
	struct intel_ctx_submit_request *cursor = NULL, *tmp = NULL;

	if (!req0)
		return;

	*req0 = NULL;

	if (req1)
		*req1 = NULL;

	/* Try to read in pairs */
	list_for_each_entry_safe(cursor, tmp, &ring->execlist_queue,
				 execlist_link) {
		if (!(*req0))
			*req0 = cursor;
		else if ((*req0)->ctx == cursor->ctx) {
			/*
			 * Same ctx: ignore first request, as second request
			 * will update tail past first request's workload
			 */
			cursor->elsp_submitted = (*req0)->elsp_submitted;
			list_del(&(*req0)->execlist_link);
			list_add_tail(&(*req0)->execlist_link,
				&ring->execlist_retired_req_list);
			*req0 = cursor;
		} else {
			if (req1)
				*req1 = cursor;
			break;
		}
	}

	if (IS_GEN8(ring->dev)) {
		/* Make sure we never cause a lite restore with HEAD == TAIL */
		if ((*req0) && ((*req0)->elsp_submitted == 1)) {
			/*
			 * Consume the buffer NOOPs to ensure HEAD != TAIL when
			 * submitting. elsp_submitted can only be >1 after
			 * reset, in which case we don't need the workaround as
			 * a lite restore will not occur.
			 */
			struct intel_ringbuffer *ringbuf;

			ringbuf = (*req0)->ctx->engine[ring->id].ringbuf;
			(*req0)->tail += 8;
			(*req0)->tail &= ringbuf->size - 1;
		}
	}
}

static void execlists_context_unqueue(struct intel_engine_cs *ring)
{
	struct intel_ctx_submit_request *req0 = NULL, *req1 = NULL;

	assert_spin_locked(&ring->execlist_lock);
	if (list_empty(&ring->execlist_queue))
		return;

	execlists_fetch_requests(ring, &req0, &req1);

	WARN_ON(req1 && req1->elsp_submitted);

	WARN_ON(execlists_submit_context(ring, req0->ctx, req0->tail,
					 req1 ? req1->ctx : NULL,
					 req1 ? req1->tail : 0));

	req0->elsp_submitted++;
	if (req1)
		req1->elsp_submitted++;
}

/*
 * execlists_TDR_context_unqueue is a TDR-specific variant of the
 * ordinary unqueue function used exclusively by the TDR.
 *
 * When doing TDR context resubmission we only want to resubmit the hung
 * context and nothing else, thus only fetch one request from the queue.
 * The exception being if the second element in the queue already has been
 * submitted, in which case we need to submit that one too. Also, don't
 * increment the elsp_submitted counter following submission since lite restore
 * context event interrupts do not not happen if the engine is hung, which
 * would normally happen in the case of a context resubmission. If we increment
 * the elsp_counter in this special case the execlist state machine would
 * expect a corresponding lite restore interrupt, which is never produced.
 */
static void execlists_TDR_context_unqueue(struct intel_engine_cs *ring)
{
	struct intel_ctx_submit_request *req0 = NULL, *req1 = NULL;

	assert_spin_locked(&ring->execlist_lock);
	if (list_empty(&ring->execlist_queue))
		return;

	execlists_fetch_requests(ring, &req0, &req1);

	/*
	 * If the second head element was not already submitted we do not have
	 * to resubmit it. Let the interrupt handler unqueue it at an
	 * appropriate time. If it was already submitted it needs to go in
	 * again to allow the hardware to switch over to it as expected.
	 * Otherwise the interrupt handler will do another unqueue of the same
	 * context and we will end up with a desync between number of
	 * submissions and interrupts and thus wait endlessly for an interrupt
	 * that will never come.
	 */
	if (req1 && !req1->elsp_submitted)
		req1 = NULL;

	WARN_ON(execlists_submit_context(ring, req0->ctx, req0->tail,
					 req1 ? req1->ctx : NULL,
					 req1 ? req1->tail : 0));
}

/**
 * intel_execlists_TDR_get_submitted_context() - return context currently
 * processed by engine
 *
 * @ring: Engine currently running context to be returned.
 * @ctx: Output parameter containing current context. May be null if
 *		 no valid context has been submitted to the execlist queue of
 *		 this engine.
 *
 * Return:
 *	CONTEXT_SUBMISSION_STATUS_OK if context is found to be submitted and is
 *	currently running on engine.
 *
 *	CONTEXT_SUBMISSION_STATUS_SUBMITTED if context is found to be submitted
 *	but not in a state that is consistent with current hardware state for
 *	the given engine. This happens in two cases:
 *
 *		1. Before the engine has switched to this context after it has
 *		been submitted to the execlist queue.
 *
 *		2. After the engine has switched away from this context but
 *		before the context has been removed from the execlist queue.
 *
 *	CONTEXT_SUBMISSION_STATUS_NONE_SUBMITTED if no context has been found
 *	to be submitted to the execlist queue and if the hardware is idle.
 *
 *	CONTEXT_SUBMISSION_STATUS_UNDEFINED if the passed context was null
 *
 */
enum context_submission_status
intel_execlists_TDR_get_submitted_context(struct intel_engine_cs *ring,
		struct intel_context **ctx)
{
	struct drm_i915_private *dev_priv = ring->dev->dev_private;
	unsigned long flags;
	struct intel_ctx_submit_request *req;
	unsigned hw_context = 0;
	enum context_submission_status status =
			CONTEXT_SUBMISSION_STATUS_UNDEFINED;

	if (!ctx)
		return CONTEXT_SUBMISSION_STATUS_UNDEFINED;

	gen8_gt_force_wake_get(dev_priv);
	spin_lock_irqsave(&ring->execlist_lock, flags);
	hw_context = I915_READ(RING_EXECLIST_STATUS_CTX_ID(ring));

	req = list_first_entry_or_null(&ring->execlist_queue,
		struct intel_ctx_submit_request, execlist_link);

	*ctx = NULL;
	if (req) {
		if (req->ctx) {
			*ctx = req->ctx;
			i915_gem_context_reference(*ctx);
		} else {
			WARN(1, "No context in request %p", req);
		}
	}

	if (*ctx) {
		unsigned sw_context =
			intel_execlists_ctx_id((*ctx)->engine[ring->id].state);

		status = ((hw_context == sw_context) && (0 != hw_context)) ?
			CONTEXT_SUBMISSION_STATUS_OK :
			CONTEXT_SUBMISSION_STATUS_SUBMITTED;
	} else {
		/*
		 * If we don't have any queue entries and the
		 * EXECLIST_STATUS register points to zero we are
		 * clearly not processing any context right now
		 */
		status = hw_context ?
			CONTEXT_SUBMISSION_STATUS_SUBMITTED :
			CONTEXT_SUBMISSION_STATUS_NONE_SUBMITTED;
	}

	spin_unlock_irqrestore(&ring->execlist_lock, flags);
	gen8_gt_force_wake_put(dev_priv);

	return status;
}

static bool execlists_check_remove_request(struct intel_engine_cs *ring,
					   u32 request_id)
{
	struct intel_ctx_submit_request *head_req;

	assert_spin_locked(&ring->execlist_lock);

	head_req = list_first_entry_or_null(&ring->execlist_queue,
					    struct intel_ctx_submit_request,
					    execlist_link);

	if (head_req != NULL) {
		struct drm_i915_gem_object *ctx_obj =
				head_req->ctx->engine[ring->id].state;
		if (intel_execlists_ctx_id(ctx_obj) == request_id) {
			WARN(head_req->elsp_submitted == 0,
			     "Never submitted head request\n");

			if (--head_req->elsp_submitted <= 0) {
				list_del(&head_req->execlist_link);
				list_add_tail(&head_req->execlist_link,
					&ring->execlist_retired_req_list);
				return true;
			}
		}
	}

	return false;
}

/**
 * intel_execlists_handle_ctx_events() - handle Context Switch interrupts
 * @ring: Engine Command Streamer to handle.
 *
 * Check the unread Context Status Buffers and manage the submission of new
 * contexts to the ELSP accordingly.
 */
void intel_execlists_handle_ctx_events(struct intel_engine_cs *ring)
{
	struct drm_i915_private *dev_priv = ring->dev->dev_private;
	u32 status_pointer;
	u8 read_pointer;
	u8 write_pointer;
	u32 status;
	u32 status_id;
	u32 submit_contexts = 0;

	status_pointer = I915_READ(RING_CONTEXT_STATUS_PTR(ring));

	read_pointer = ring->next_context_status_buffer;
	write_pointer = status_pointer & 0x07;
	if (read_pointer > write_pointer)
		write_pointer += 6;

	spin_lock(&ring->execlist_lock);

	while (read_pointer < write_pointer) {
		read_pointer++;
		status = I915_READ(RING_CONTEXT_STATUS_BUF(ring) +
				(read_pointer % 6) * 8);
		status_id = I915_READ(RING_CONTEXT_STATUS_BUF(ring) +
				(read_pointer % 6) * 8 + 4);

		if (status & GEN8_CTX_STATUS_PREEMPTED) {
			if (status & GEN8_CTX_STATUS_LITE_RESTORE) {
				if (execlists_check_remove_request(ring, status_id))
					WARN(1, "Lite Restored request removed from queue\n");
			} else
				WARN(1, "Preemption without Lite Restore\n");
		}

		 if ((status & GEN8_CTX_STATUS_ACTIVE_IDLE) ||
		     (status & GEN8_CTX_STATUS_ELEMENT_SWITCH)) {
			if (execlists_check_remove_request(ring, status_id))
				submit_contexts++;
		}
	}

	if (submit_contexts != 0)
		execlists_context_unqueue(ring);

	spin_unlock(&ring->execlist_lock);

	WARN(submit_contexts > 2, "More than two context complete events?\n");
	ring->next_context_status_buffer = write_pointer % 6;

	I915_WRITE(RING_CONTEXT_STATUS_PTR(ring),
		   ((u32)ring->next_context_status_buffer & 0x07) << 8);
}

static int execlists_context_queue(struct intel_engine_cs *ring,
				   struct intel_context *to,
				   u32 tail)
{
	struct intel_ctx_submit_request *req = NULL, *cursor;
	struct drm_i915_private *dev_priv = ring->dev->dev_private;
	unsigned long flags;
	int num_elements = 0;

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (req == NULL)
		return -ENOMEM;
	req->ctx = to;
	i915_gem_context_reference(req->ctx);

	if (to != ring->default_context)
		intel_lr_context_pin(ring, to);

	req->ring = ring;
	req->tail = tail;

	if (IS_GEN8(ring->dev)) {
		struct intel_ringbuffer *ringbuf = to->engine[ring->id].ringbuf;
		/*
		 * Here are two extra NOOPs as padding to avoid lite restore of
		 * a context with HEAD==TAIL.
		 */
		intel_logical_ring_emit(ringbuf, MI_NOOP);
		intel_logical_ring_emit(ringbuf, MI_NOOP);
		intel_logical_ring_advance(ringbuf);
	}

	intel_runtime_pm_get(dev_priv);

	spin_lock_irqsave(&ring->execlist_lock, flags);

	list_for_each_entry(cursor, &ring->execlist_queue, execlist_link)
		if (++num_elements > 2)
			break;

	if (num_elements > 2) {
		struct intel_ctx_submit_request *tail_req;

		tail_req = list_last_entry(&ring->execlist_queue,
					   struct intel_ctx_submit_request,
					   execlist_link);

		if (to == tail_req->ctx) {
			WARN(tail_req->elsp_submitted != 0,
				"More than 2 already-submitted reqs queued\n");
			list_del(&tail_req->execlist_link);
			list_add_tail(&tail_req->execlist_link,
				&ring->execlist_retired_req_list);
		}
	}

	list_add_tail(&req->execlist_link, &ring->execlist_queue);
	if (num_elements == 0)
		execlists_context_unqueue(ring);

	spin_unlock_irqrestore(&ring->execlist_lock, flags);

	return 0;
}

/**
 * intel_execlists_TDR_context_queue() - ELSP context submission bypassing
 * queue
 *
 * Context submission mechanism exclusively used by TDR that bypasses the
 * execlist queue. This is necessary since at the point of TDR hang recovery
 * the hardware will be hung and resubmitting a fixed context (the context that
 * the TDR has identified as hung and fixed up in order to move past the
 * blocking batch buffer) to a hung execlist queue will lock up the TDR.
 * Instead, opt for direct ELSP submission without depending on the rest of the
 * driver.
 * If execlist queue is empty we fall back to ordinary queue-based submission
 * since we require the context under submission to be present in the queue
 * (yes, this happens sometimes - e.g. during full GPU reset in which case the
 * queues are reinitialized).
 *
 * @ring: engine to submit context to
 * @to: context to be resubmitted
 * @tail: position in ring to submit to
 *
 * Return:
 *	0 if successful, otherwise propagate error code.
 */
int intel_execlists_TDR_context_queue(struct intel_engine_cs *ring,
			struct intel_context *to,
			u32 tail)
{
	struct intel_ctx_submit_request *req = NULL;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&ring->execlist_lock, flags);

	if (list_empty(&ring->execlist_queue)) {
		spin_unlock_irqrestore(&ring->execlist_lock, flags);

		/*
		 * Fallback path in case the execlist queue is empty - just use
		 * ordinary queue-based submission.
		 *
		 * At first glance this looks like a possible race since
		 * someone might add an entry to the queue after we enter
		 * this if statement and before we end up in
		 * execlists_context_queue to add a new queue entry expecting
		 * it to be immediately unqueued. However, TDR should hold
		 * precendence during hang recovery and all other request
		 * submitters should call i915_mutex_lock_interruptible to
		 * acquire the struct_mutex before submitting new work. This
		 * way of acquiring the mutex takes TDR into account and makes
		 * sure that nobody else is allowed to submit work to a hung
		 * execlist queue during an ongoing hang recovery.
		 */
		ret = execlists_context_queue(ring, to, tail);

	} else {
		/*
		 * Submission path used for hang recovery bypassing execlist
		 * queue. When a context needs to be resubmitted for lite
		 * restore during hang recovery we cannot use the execlist
		 * queue since it will be hung just like its corresponding ring
		 * engine. Instead go for direct submission to ELSP.
		 */
		struct intel_context *c = NULL;
		struct drm_i915_gem_object *ctx_obj = NULL;
		u32 c_ctxid = 0;
		u32 to_ctxid = 0;

		req = list_first_entry(&ring->execlist_queue,
			typeof(*req),
			execlist_link);

		if (!req) {
			WARN(1, "Request is null, " \
				"context resubmission to %s failed!",
					ring->name);

			return -EINVAL;
		}

		c = req->ctx;

		if (!c) {
			WARN(1, "Context null for request %p, " \
				"context resubmission to %s failed",
				req, ring->name);

			return -EINVAL;
		}

		ctx_obj = c->engine[ring->id].state;

		if (!ctx_obj) {
			WARN(1, "Context object null for context %p, " \
				"context resubmission to %s failed", c,
				ring->name);

			return -EINVAL;
		}

		WARN(req->elsp_submitted == 0,
			"Allegedly hung request has never been submitted " \
			"to ELSP\n");

		c_ctxid = intel_execlists_ctx_id(c->engine[ring->id].state);
		to_ctxid = intel_execlists_ctx_id(to->engine[ring->id].state);

		/*
		 * At the beginning of hang recovery the TDR asks for the
		 * currently submitted context (which has been determined to be
		 * hung at that point). This should be the context at the head
		 * of the execlist queue. If we reach a point during the
		 * recovery where we need to do a lite restore of the hung
		 * context only to discover that the head context of the
		 * execlist queue has changed, what do we do? The least we can
		 * do is produce a warning.
		 */
		WARN(c_ctxid != to_ctxid,
		    "Context (%x) at head of execlist queue for %s " \
		    "is not the suspected hung context (%x)! Was execlist " \
		    "queue reordered during hang recovery?",
		    (unsigned int) c_ctxid, ring->name,
		    (unsigned int) to_ctxid);

		execlists_TDR_context_unqueue(ring);

		spin_unlock_irqrestore(&ring->execlist_lock, flags);
	}

	return ret;
}


static int logical_ring_invalidate_all_caches(struct intel_ringbuffer *ringbuf)
{
	struct intel_engine_cs *ring = ringbuf->ring;
	uint32_t flush_domains;
	int ret;

	flush_domains = 0;
	if (ring->gpu_caches_dirty)
		flush_domains = I915_GEM_GPU_DOMAINS;

	ret = ring->emit_flush(ringbuf, I915_GEM_GPU_DOMAINS, flush_domains);
	if (ret)
		return ret;

	ring->gpu_caches_dirty = false;
	return 0;
}

static int
gen8_ring_start_watchdog(struct intel_ringbuffer *ringbuf)
{
	int ret;
	struct intel_engine_cs *ring = ringbuf->ring;

	ret = intel_logical_ring_begin(ringbuf, 10);
	if (ret)
		return ret;

	/* i915_reg.h includes a warning to place a MI_NOOP
	* before a MI_LOAD_REGISTER_IMM*/
	intel_logical_ring_emit(ringbuf, MI_NOOP);
	intel_logical_ring_emit(ringbuf, MI_NOOP);

	/* Set counter period */
	intel_logical_ring_emit(ringbuf, MI_LOAD_REGISTER_IMM(1));
	intel_logical_ring_emit(ringbuf, RING_THRESH(ring->mmio_base));
	intel_logical_ring_emit(ringbuf, ring->watchdog_threshold);
	intel_logical_ring_emit(ringbuf, MI_NOOP);

	/* Start counter */
	intel_logical_ring_emit(ringbuf, MI_LOAD_REGISTER_IMM(1));
	intel_logical_ring_emit(ringbuf, RING_CNTR(ring->mmio_base));
	intel_logical_ring_emit(ringbuf, WATCHDOG_ENABLE);
	intel_logical_ring_emit(ringbuf, MI_NOOP);
	intel_logical_ring_advance(ringbuf);

	return 0;
}

static int
gen8_ring_stop_watchdog(struct intel_ringbuffer *ringbuf)
{
	int ret;
	struct intel_engine_cs *ring = ringbuf->ring;

	ret = intel_logical_ring_begin(ringbuf, 6);
	if (ret)
		return ret;

	/* i915_reg.h includes a warning to place a MI_NOOP
	* before a MI_LOAD_REGISTER_IMM*/
	intel_logical_ring_emit(ringbuf, MI_NOOP);
	intel_logical_ring_emit(ringbuf, MI_NOOP);

	intel_logical_ring_emit(ringbuf, MI_LOAD_REGISTER_IMM(1));
	intel_logical_ring_emit(ringbuf, RING_CNTR(ring->mmio_base));

	switch (ring->id) {
	default:
	case RCS:
		intel_logical_ring_emit(ringbuf, RCS_WATCHDOG_DISABLE);
		break;
	case VCS:
		intel_logical_ring_emit(ringbuf, VCS_WATCHDOG_DISABLE);
		break;
	}

	intel_logical_ring_emit(ringbuf, MI_NOOP);
	intel_logical_ring_advance(ringbuf);

	return 0;
}

static int
logical_ring_write_active_request(struct intel_ringbuffer *ringbuf,
				  struct drm_i915_gem_request *req)
{
	int ret;

	ret = intel_logical_ring_begin(ringbuf, 4);
	if (ret)
		return ret;

	intel_logical_ring_emit(ringbuf, MI_STORE_DWORD_INDEX);
	intel_logical_ring_emit(ringbuf, I915_GEM_ACTIVE_SEQNO_INDEX <<
				  MI_STORE_DWORD_INDEX_SHIFT);
	intel_logical_ring_emit(ringbuf, i915_gem_request_get_seqno(req));
	intel_logical_ring_emit(ringbuf, MI_NOOP);
	intel_logical_ring_advance(ringbuf);

	return 0;
}

static int execlists_move_to_gpu(struct intel_ringbuffer *ringbuf,
				 struct list_head *vmas)
{
	struct intel_engine_cs *ring = ringbuf->ring;
	struct i915_vma *vma;
	uint32_t flush_domains = 0;
	bool flush_chipset = false;
	int ret;

	list_for_each_entry(vma, vmas, exec_list) {
		struct drm_i915_gem_object *obj = vma->obj;

		ret = i915_gem_object_sync(obj, ring, true);
		if (ret)
			return ret;

		if (obj->base.write_domain & I915_GEM_DOMAIN_CPU)
			flush_chipset |= i915_gem_clflush_object(obj, false);

		flush_domains |= obj->base.write_domain;
	}

	if (flush_domains & I915_GEM_DOMAIN_GTT)
		wmb();

	return 0;
}

static int
gen8_logical_disable_protected_mem(struct intel_ringbuffer *ringbuf)
{
	int ret;

	ret = intel_logical_ring_begin(ringbuf, 6);
	if (ret)
		return ret;

	/* Pipe Control */
	intel_logical_ring_emit(ringbuf, GFX_OP_PIPE_CONTROL(5));
	intel_logical_ring_emit(ringbuf, 0x81010a0);
	intel_logical_ring_emit(ringbuf, 0);
	intel_logical_ring_emit(ringbuf, 0);
	intel_logical_ring_emit(ringbuf, 0);
	intel_logical_ring_emit(ringbuf, MI_NOOP);
	intel_logical_ring_advance(ringbuf);

	return 0;
}

/**
 * execlists_submission() - submit a batchbuffer for execution, Execlists style
 * @dev: DRM device.
 * @file: DRM file.
 * @ring: Engine Command Streamer to submit to.
 * @ctx: Context to employ for this submission.
 * @args: execbuffer call arguments.
 * @vmas: list of vmas.
 * @batch_obj: the batchbuffer to submit.
 * @exec_start: batchbuffer start virtual address pointer.
 * @dispatch_flags: translated execbuffer call flags.
 *
 * This is the evil twin version of i915_gem_ringbuffer_submission. It abstracts
 * away the submission details of the execbuffer ioctl call.
 *
 * Return: non-zero if the submission fails.
 */
int intel_execlists_submission(struct i915_execbuffer_params *params,
			       struct drm_i915_gem_execbuffer2 *args,
			       struct list_head *vmas)
{
	struct i915_scheduler_queue_entry *qe;
	struct drm_device       *dev = params->dev;
	struct intel_engine_cs  *ring = params->ring;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_ringbuffer *ringbuf = params->ctx->engine[ring->id].ringbuf;
	int ret;

	params->instp_mode = args->flags & I915_EXEC_CONSTANTS_MASK;
	params->instp_mask = I915_EXEC_CONSTANTS_MASK;
	switch (params->instp_mode) {
	case I915_EXEC_CONSTANTS_REL_GENERAL:
	case I915_EXEC_CONSTANTS_ABSOLUTE:
	case I915_EXEC_CONSTANTS_REL_SURFACE:
		if (params->instp_mode != 0 && ring != &dev_priv->ring[RCS]) {
			DRM_DEBUG("non-0 rel constants mode on non-RCS\n");
			return -EINVAL;
		}

		if (params->instp_mode != dev_priv->relative_constants_mode) {
			if (params->instp_mode == I915_EXEC_CONSTANTS_REL_SURFACE) {
				DRM_DEBUG("rel surface constants mode invalid on gen5+\n");
				return -EINVAL;
			}

			/* The HW changed the meaning on this bit on gen6 */
			params->instp_mask &= ~I915_EXEC_CONSTANTS_REL_SURFACE;
		}
		break;
	default:
		DRM_DEBUG("execbuf with unknown constants: %d\n", params->instp_mode);
		return -EINVAL;
	}

	if (args->num_cliprects != 0) {
		u32 priv_data;

		/*
		 * cliprects is only used by the userland to pass in private
		 * handshake data for gen5+.
		 */
		if (args->num_cliprects != sizeof(priv_data))
			return -EINVAL;

		if (copy_from_user((void *)&priv_data,
			to_user_ptr(args->cliprects_ptr), sizeof(priv_data))) {
			return -EFAULT;
		}

		if (priv_data == 0xffffffff)
			params->dispatch_flags |= I915_DISPATCH_LAUNCH_CB2;
	} else {
		if (args->DR4 == 0xffffffff) {
			DRM_DEBUG("UXA submitting garbage DR4, fixing up\n");
			args->DR4 = 0;
		}

		if (args->DR1 || args->DR4 || args->cliprects_ptr) {
			DRM_DEBUG("0 cliprects but dirt in cliprects fields\n");
			return -EINVAL;
		}
	}

	if (args->flags & I915_EXEC_GEN7_SOL_RESET) {
		DRM_DEBUG("sol reset is gen7 only\n");
		return -EINVAL;
	}

	ret = execlists_move_to_gpu(ringbuf, vmas);
	if (ret)
		return ret;

	i915_gem_execbuffer_move_to_active(vmas, ring);

	/* Make sure the OLR hasn't advanced (which would indicate a flush
	 * of the work in progress which in turn would be a Bad Thing). */
	WARN_ON(ring->outstanding_lazy_request != params->request);

	/*
	 * A new request has been assigned to the buffer and saved away for
	 * future reference. So clear the OLR to ensure that any further
	 * work is assigned a brand new request:
	 */
	ring->outstanding_lazy_request = NULL;

	trace_i915_gem_ring_queue(ring, params);

	qe = container_of(params, typeof(*qe), params);
	ret = i915_scheduler_queue_execbuffer(qe);
	if (ret)
		return ret;

	return 0;
}

/*
 * This is the main function for adding a batch to the ring.
 * It is called from the scheduler, with the struct_mutex already held.
 */
int intel_execlists_submission_final(struct i915_execbuffer_params *params)
{
	struct drm_i915_private *dev_priv = params->dev->dev_private;
	struct intel_engine_cs  *ring = params->ring;
	struct intel_ringbuffer *ringbuf = params->ctx->engine[ring->id].ringbuf;
	u64 exec_start;
	int ret;
	bool watchdog_running = 0;
	uint32_t min_space;

	/* The mutex must be acquired before calling this function */
	BUG_ON(!mutex_is_locked(&params->dev->struct_mutex));

	/*
	 * It would be a bad idea to run out of space while writing commands
	 * to the ring. One of the major aims of the scheduler is to not stall
	 * at any point for any reason. However, doing an early exit half way
	 * through submission could result in a partial sequence being written
	 * which would leave the engine in an unknown state. Therefore, check in
	 * advance that there will be enough space for the entire submission
	 * whether emitted by the code below OR by any other functions that may
	 * be executed before the end of final().
	 *
	 * NB: This test deliberately overestimates, because that's easier than
	 * tracing every potential path that could be taken!
	 *
	 * Current measurements suggest that we may need to emit up to ??? bytes
	 * (186 dwords), so this is rounded up to 256 dwords here. Then we double
	 * that to get the free space requirement, because the block isn't allowed
	 * to span the transition from the end to the beginning of the ring.
	 */
#define I915_BATCH_EXEC_MAX_LEN         256	/* max dwords emitted here	*/
	min_space = I915_BATCH_EXEC_MAX_LEN * 2 * sizeof(uint32_t);
	ret = logical_ring_test_space(ringbuf, min_space);
	if (ret)
		return ret;

	/* Assign an identifier to track this request through the hardware: */
	WARN_ON(params->request->seqno != 0);
	ret = i915_gem_get_seqno(ring->dev, &params->request->seqno);
	if (ret)
		goto error;

	/* Ensure the correct request gets assigned to the correct buffer: */
	WARN_ON(ring->outstanding_lazy_request != NULL);
	WARN_ON(params->request == NULL);
	ring->outstanding_lazy_request = params->request;

	ret = intel_logical_ring_begin(ringbuf, I915_BATCH_EXEC_MAX_LEN);
	if (ret)
		goto error;

	/* Request matches? */
	WARN_ON(ring->outstanding_lazy_request != params->request);

	/* Start watchdog timer */
	if (params->args_flags & I915_EXEC_ENABLE_WATCHDOG) {
		if (!intel_ring_supports_watchdog(ring)) {
			DRM_ERROR("%s does NOT support watchdog timeout!\n",
					ring->name);
			ret = -EINVAL;
			goto error;
		}

		ret = gen8_ring_start_watchdog(ringbuf);
		if (ret)
			goto error;

		watchdog_running = 1;
	}

	/* Request matches? */
	WARN_ON(ring->outstanding_lazy_request != params->request);

	/*
	 * Unconditionally invalidate gpu caches and ensure that we do flush
	 * any residual writes from the previous batch.
	 */
	ret = logical_ring_invalidate_all_caches(ringbuf);
	if (ret)
		goto error;

	/* Request matches? */
	WARN_ON(ring->outstanding_lazy_request != params->request);

	if (ring == &dev_priv->ring[RCS] &&
	    params->instp_mode != dev_priv->relative_constants_mode) {
		intel_logical_ring_emit(ringbuf, MI_NOOP);
		intel_logical_ring_emit(ringbuf, MI_LOAD_REGISTER_IMM(1));
		intel_logical_ring_emit(ringbuf, INSTPM);
		intel_logical_ring_emit(ringbuf, params->instp_mask << 16 | params->instp_mode);
		intel_logical_ring_advance(ringbuf);

		dev_priv->relative_constants_mode = params->instp_mode;
	}

	if (IS_GEN8(params->dev) && ring == &dev_priv->ring[RCS])
		i915_program_perfmon(params->dev, ringbuf, params->ctx);

	/* Flag this request as being active on the ring so the watchdog
	 * code knows where to look if things go wrong. */
	ret = logical_ring_write_active_request(ringbuf, params->request);
	if (ret) {
		DRM_DEBUG_DRIVER("Failed to tag request on ring %d (%d)\n",
				 ring->id, ret);
		goto error;
	}

	/* Request matches? */
	WARN_ON(ring->outstanding_lazy_request != params->request);

	exec_start = params->batch_obj_vm_offset +
		     params->args_batch_start_offset;

	ret = ring->emit_bb_start(ringbuf, exec_start, params->dispatch_flags);
	if (ret)
		goto error;

	/* Send pipe control with protected memory disable if requested */
	if (params->dispatch_flags & I915_DISPATCH_LAUNCH_CB2) {
		ret = gen8_logical_disable_protected_mem(ringbuf);
		if (ret)
			goto error;
	}

	/* Clear the active request again */
	ret = logical_ring_write_active_request(ringbuf, NULL);
	if (ret)
		goto error;

	/* Cancel watchdog timer */
	if (watchdog_running) {
		ret = gen8_ring_stop_watchdog(ringbuf);
		if (ret)
			return ret;
	}

	/* Request matches? */
	WARN_ON(ring->outstanding_lazy_request != params->request);

	trace_i915_gem_ring_dispatch(params->request, params->dispatch_flags);

	i915_gem_execbuffer_retire_commands(params->dev, params->file, ring, params->batch_obj);

	/*
	 * CHV: Extend RC6 promotion timer upon hitting Media workload to help
	 * increase power savings with media scenarios.
	 */
	if (((params->args_flags & I915_EXEC_RING_MASK) == I915_EXEC_BSD) &&
		IS_CHERRYVIEW(dev_priv->dev) && dev_priv->rps.enabled) {

		vlv_modify_rc6_promotion_timer(dev_priv, true);

		/* Start a timer for 1 sec to reset this value to original */
		mod_delayed_work(dev_priv->wq,
				&dev_priv->rps.vlv_media_timeout_work,
				msecs_to_jiffies(1000));

	}

	/* OLR should be empty by now. */
	WARN_ON(ring->outstanding_lazy_request);

	return 0;

error:
	/* Reset the OLR ready to try again later. */
	ring->outstanding_lazy_request = NULL;

	return ret;
}

void intel_execlists_retire_requests(struct intel_engine_cs *ring)
{
	struct intel_ctx_submit_request *req, *tmp;
	struct drm_i915_private *dev_priv = ring->dev->dev_private;
	unsigned long flags;
	struct list_head retired_list;

	WARN_ON(!mutex_is_locked(&ring->dev->struct_mutex));
	if (list_empty(&ring->execlist_retired_req_list))
		return;

	INIT_LIST_HEAD(&retired_list);
	spin_lock_irqsave(&ring->execlist_lock, flags);
	list_replace_init(&ring->execlist_retired_req_list, &retired_list);
	spin_unlock_irqrestore(&ring->execlist_lock, flags);

	list_for_each_entry_safe(req, tmp, &retired_list, execlist_link) {
		struct intel_context *ctx = req->ctx;
		struct drm_i915_gem_object *ctx_obj =
				ctx->engine[ring->id].state;

		if (ctx_obj && (ctx != ring->default_context))
			intel_lr_context_unpin(ring, ctx);
		intel_runtime_pm_put(dev_priv);
		i915_gem_context_unreference(req->ctx);
		list_del(&req->execlist_link);
		kfree(req);
	}
}

void intel_logical_ring_stop(struct intel_engine_cs *ring)
{
	struct drm_i915_private *dev_priv = ring->dev->dev_private;
	int ret;

	if (!intel_ring_initialized(ring))
		return;

	ret = intel_ring_idle(ring, true);
	if (ret && !i915_reset_in_progress(&to_i915(ring->dev)->gpu_error))
		DRM_ERROR("failed to quiesce %s whilst cleaning up: %d\n",
			  ring->name, ret);

	/* FIXME: Stopping rings through MI_MODE is not defined for execlists */

	I915_WRITE_MODE(ring, _MASKED_BIT_ENABLE(RING_MODE_STOP));
	if (wait_for_atomic((I915_READ_MODE(ring) & RING_MODE_IDLE) != 0, 1000)) {
		DRM_ERROR("%s :timed out trying to stop ring\n", ring->name);
		return;
	}
	I915_WRITE_MODE(ring, _MASKED_BIT_DISABLE(RING_MODE_STOP));
}

int logical_ring_flush_all_caches(struct intel_ringbuffer *ringbuf)
{
	struct intel_engine_cs *ring = ringbuf->ring;
	int ret;

	if (!ring->gpu_caches_dirty)
		return 0;

	ret = ring->emit_flush(ringbuf, 0, I915_GEM_GPU_DOMAINS);
	if (ret)
		return ret;

	ring->gpu_caches_dirty = false;
	return 0;
}

/**
 * intel_logical_ring_advance_and_submit() - advance the tail and submit the workload
 * @ringbuf: Logical Ringbuffer to advance.
 *
 * The tail is updated in our logical ringbuffer struct, not in the actual context. What
 * really happens during submission is that the context and current tail will be placed
 * on a queue waiting for the ELSP to be ready to accept a new context submission. At that
 * point, the tail *inside* the context is updated and the ELSP written to.
 */
void intel_logical_ring_advance_and_submit(struct intel_ringbuffer *ringbuf)
{
	struct intel_engine_cs *ring = ringbuf->ring;
	struct intel_context *ctx = ringbuf->FIXME_lrc_ctx;

	intel_logical_ring_advance(ringbuf);

	if (intel_ring_stopped(ring))
		return;

	execlists_context_queue(ring, ctx, ringbuf->tail);
}

static int intel_lr_context_pin(struct intel_engine_cs *ring,
		struct intel_context *ctx)
{
	struct drm_i915_gem_object *ctx_obj = ctx->engine[ring->id].state;
	struct intel_ringbuffer *ringbuf = ctx->engine[ring->id].ringbuf;
	int ret = 0;

	WARN_ON(!mutex_is_locked(&ring->dev->struct_mutex));
	if (ctx->engine[ring->id].unpin_count++ == 0) {
		ret = i915_gem_obj_ggtt_pin(ctx_obj,
				GEN8_LR_CONTEXT_ALIGN, 0);
		if (ret)
			goto reset_unpin_count;

		ret = intel_pin_and_map_ringbuffer_obj(ring->dev, ringbuf);
		if (ret)
			goto unpin_ctx_obj;
	}

	return ret;

unpin_ctx_obj:
	i915_gem_object_ggtt_unpin(ctx_obj);
reset_unpin_count:
	ctx->engine[ring->id].unpin_count = 0;

	return ret;
}

void intel_lr_context_unpin(struct intel_engine_cs *ring,
		struct intel_context *ctx)
{
	struct drm_i915_gem_object *ctx_obj = ctx->engine[ring->id].state;
	struct intel_ringbuffer *ringbuf = ctx->engine[ring->id].ringbuf;

	if (ctx_obj) {
		WARN_ON(!mutex_is_locked(&ring->dev->struct_mutex));
		if (--ctx->engine[ring->id].unpin_count == 0) {
			intel_unpin_ringbuffer_obj(ringbuf);
			i915_gem_object_ggtt_unpin(ctx_obj);
		}
	}
}

int intel_logical_ring_alloc_request(struct intel_engine_cs *ring,
				     struct intel_context *ctx)
{
	struct drm_i915_gem_request *request;
	struct drm_i915_private *dev_private = ring->dev->dev_private;
	int ret;

	if (ring->outstanding_lazy_request)
		return 0;

	request = kzalloc(sizeof(*request), GFP_KERNEL);
	if (request == NULL)
		return -ENOMEM;

	if (ctx != ring->default_context) {
		ret = intel_lr_context_pin(ring, ctx);
		if (ret) {
			kfree(request);
			return ret;
		}
	}

	kref_init(&request->ref);
	request->ring = ring;
	request->uniq = dev_private->request_uniq++;

	/*
	 * Hold a reference to the context this request belongs to
	 * (we will need it when the time comes to emit/retire the
	 * request). Likewise, the ringbuff is useful to keep track of.
	 */
	request->ctx = ctx;
	i915_gem_context_reference(request->ctx);
	request->ringbuf = ctx->engine[ring->id].ringbuf;

	ring->outstanding_lazy_request = request;
	return 0;
}

static int logical_ring_wait_request(struct intel_ringbuffer *ringbuf,
				     int bytes)
{
	struct intel_engine_cs *ring = ringbuf->ring;
	struct drm_i915_gem_request *request;
	int ret;

	if (intel_ring_space(ringbuf) >= bytes)
		return 0;

	list_for_each_entry(request, &ring->request_list, list) {
		/*
		 * The request queue is per-engine, so can contain requests
		 * from multiple ringbuffers. Here, we must ignore any that
		 * aren't from the ringbuffer we're considering.
		 */
		struct intel_context *ctx = request->ctx;
		if (ctx->engine[ring->id].ringbuf != ringbuf)
			continue;

		/* Would completion of this request free enough space? */
		if (__intel_ring_space(request->tail, ringbuf->tail,
				       ringbuf->size) >= bytes) {
			break;
		}
	}

	if (&request->list == &ring->request_list)
		return -ENOSPC;

	ret = i915_wait_request(request);
	if (ret)
		return ret;

	i915_gem_retire_requests_ring(ring);

	return intel_ring_space(ringbuf) >= bytes ? 0 : -ENOSPC;
}

static int logical_ring_wait_for_space(struct intel_ringbuffer *ringbuf,
				       int bytes)
{
	struct intel_engine_cs *ring = ringbuf->ring;
	struct drm_device *dev = ring->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	unsigned long end;
	int ret;

	ret = logical_ring_wait_request(ringbuf, bytes);
	if (ret != -ENOSPC)
		return ret;

	/* Force the context submission in case we have been skipping it */
	intel_logical_ring_advance_and_submit(ringbuf);

	/* With GEM the hang check should kick us out of the loop,
	 * leaving it early runs the risk of corrupting GEM state (due
	 * to running on almost untested codepaths). But on resume
	 * timers don't work yet, so prevent a complete hang in that
	 * case by choosing an insanely large timeout. */
	end = jiffies + 60 * HZ;

	ret = 0;
	do {
		if (intel_ring_space(ringbuf) >= bytes)
			break;

		msleep(1);

		if (dev_priv->mm.interruptible && signal_pending(current)) {
			ret = -ERESTARTSYS;
			break;
		}

		ret = i915_gem_check_wedge(&dev_priv->gpu_error,
					   dev_priv->mm.interruptible, ring);
		if (ret)
			break;

		if (time_after(jiffies, end)) {
			ret = -EBUSY;
			break;
		}
	} while (1);

	return ret;
}

static int logical_ring_wrap_buffer(struct intel_ringbuffer *ringbuf)
{
	uint32_t __iomem *virt;
	int rem = ringbuf->size - ringbuf->tail;

	if (ringbuf->space < rem) {
		int ret = logical_ring_wait_for_space(ringbuf, rem);
		if (ret)
			return ret;
	}

	virt = ringbuf->virtual_start + ringbuf->tail;
	rem /= 4;
	while (rem--)
		iowrite32(MI_NOOP, virt++);

	ringbuf->tail = 0;
	intel_ring_update_space(ringbuf);

	return 0;
}

static int logical_ring_prepare(struct intel_ringbuffer *ringbuf, int bytes)
{
	int ret;

	if (unlikely(ringbuf->tail + bytes > ringbuf->effective_size)) {
		ret = logical_ring_wrap_buffer(ringbuf);
		if (unlikely(ret))
			return ret;
	}

	if (unlikely(ringbuf->space < bytes)) {
		ret = logical_ring_wait_for_space(ringbuf, bytes);
		if (unlikely(ret))
			return ret;
	}

	return 0;
}

/**
 * intel_logical_ring_begin() - prepare the logical ringbuffer to accept some commands
 *
 * @ringbuf: Logical ringbuffer.
 * @num_dwords: number of DWORDs that we plan to write to the ringbuffer.
 *
 * The ringbuffer might not be ready to accept the commands right away (maybe it needs to
 * be wrapped, or wait a bit for the tail to be updated). This function takes care of that
 * and also preallocates a request (every workload submission is still mediated through
 * requests, same as it did with legacy ringbuffer submission).
 *
 * Return: non-zero if the ringbuffer is not ready to be written to.
 */
int intel_logical_ring_begin(struct intel_ringbuffer *ringbuf, int num_dwords)
{
	struct intel_engine_cs *ring = ringbuf->ring;
	struct drm_device *dev = ring->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret;

	if (IS_GEN8(dev))
		/*
		 * Reserve space for 2 NOOPs at the end of each request to be
		 * used as a workaround for not being allowed to do lite
		 * restore with HEAD==TAIL.
		 */
		num_dwords += 2;

	ret = i915_gem_check_wedge(&dev_priv->gpu_error,
				   dev_priv->mm.interruptible, ring);
	if (ret)
		return ret;

	ret = logical_ring_prepare(ringbuf, num_dwords * sizeof(uint32_t));
	if (ret)
		return ret;

	/* Preallocate the olr before touching the ring */
	ret = intel_logical_ring_alloc_request(ring, ringbuf->FIXME_lrc_ctx);
	if (ret)
		return ret;

	ringbuf->space -= num_dwords * sizeof(uint32_t);
	return 0;
}

static int intel_logical_ring_workarounds_emit(struct intel_engine_cs *ring,
	       struct intel_context *ctx)
{
	int ret, i;
	struct intel_ringbuffer *ringbuf = ctx->engine[ring->id].ringbuf;
	struct drm_device *dev = ring->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct i915_workarounds *w = &dev_priv->workarounds;

	if (WARN_ON(w->count == 0))
		return 0;

	ring->gpu_caches_dirty = true;
	ret = logical_ring_flush_all_caches(ringbuf);
	if (ret)
		return ret;

	ret = intel_logical_ring_begin(ringbuf, w->count * 2 + 2);
	if (ret)
		return ret;

	intel_logical_ring_emit(ringbuf, MI_LOAD_REGISTER_IMM(w->count));
	for (i = 0; i < w->count; i++) {
		intel_logical_ring_emit(ringbuf, w->reg[i].addr);
		intel_logical_ring_emit(ringbuf, w->reg[i].value);
	}
	intel_logical_ring_emit(ringbuf, MI_NOOP);

	intel_logical_ring_advance(ringbuf);

	ring->gpu_caches_dirty = true;
	ret = logical_ring_flush_all_caches(ringbuf);
	if (ret)
		return ret;

	return 0;
}

static struct intel_ringbuffer *
create_wa_bb(struct intel_context *ctx,
	     struct intel_engine_cs *ring,
	     uint32_t bb_size)
{
	struct drm_device *dev = ring->dev;
	struct intel_ringbuffer *ringbuf;
	int ret;

	ringbuf = kzalloc(sizeof(*ringbuf), GFP_KERNEL);
	if (!ringbuf)
		return NULL;

	ringbuf->ring = ring;
	ringbuf->FIXME_lrc_ctx = ctx;

	ringbuf->size = roundup(bb_size, PAGE_SIZE);
	ringbuf->effective_size = ringbuf->size;
	ringbuf->head = 0;
	ringbuf->tail = 0;
	ringbuf->space = ringbuf->size;
	ringbuf->last_retired_head = -1;

	ret = intel_alloc_ringbuffer_obj(dev, ringbuf);
	if (ret) {
		DRM_DEBUG_DRIVER("Failed to allocate ringbuffer obj %s: %d\n",
				ring->name, ret);
		kfree(ringbuf);
		return NULL;
	}

	ret = intel_pin_and_map_ringbuffer_obj(dev, ringbuf);
	if (ret) {
		DRM_ERROR("Failed to pin and map %s w/a batch: %d\n",
			  ring->name, ret);
		intel_destroy_ringbuffer_obj(ringbuf);
		kfree(ringbuf);
		return NULL;
	}

	return ringbuf;
}

static int gen8_init_indirectctx_bb(struct intel_engine_cs *ring,
				    struct intel_context *ctx)
{
	unsigned long flags = 0;
	u32 scratch_addr;
	struct intel_ringbuffer *ringbuf = NULL;

	if (!get_pipe_control_scratch_addr(ring)) {
		DRM_ERROR("scratch page not allocated for %s\n", ring->name);
		return -EINVAL;
	}

	ringbuf = create_wa_bb(ctx, ring, PAGE_SIZE);
	if (!ringbuf)
		return -ENOMEM;

	ctx->indirect_ctx_wa_bb = ringbuf;

	/* WaDisableCtxRestoreArbitration:bdw,chv */
	intel_logical_ring_emit(ringbuf, MI_ARB_ON_OFF | MI_ARB_DISABLE);

	/* WaFlushCoherentL3CacheLinesAtContextSwitch:bdw,chv */
	intel_logical_ring_emit(ringbuf, GFX_OP_PIPE_CONTROL(6));
	intel_logical_ring_emit(ringbuf, PIPE_CONTROL_GLOBAL_GTT_IVB |
				PIPE_CONTROL_DC_FLUSH_ENABLE);
	intel_logical_ring_emit(ringbuf, 0);
	intel_logical_ring_emit(ringbuf, 0);
	intel_logical_ring_emit(ringbuf, 0);
	intel_logical_ring_emit(ringbuf, 0);

	/* WaClearSlmSpaceAtContextSwitch:bdw,chv */
	flags = PIPE_CONTROL_FLUSH_RO_CACHES |
		PIPE_CONTROL_GLOBAL_GTT_IVB |
		PIPE_CONTROL_CS_STALL |
		PIPE_CONTROL_QW_WRITE;

	/* Actual scratch location is at 128 bytes offset */
	scratch_addr = get_pipe_control_scratch_addr(ring) + 2*CACHELINE_BYTES;
	scratch_addr |= PIPE_CONTROL_GLOBAL_GTT;

	intel_logical_ring_emit(ringbuf, GFX_OP_PIPE_CONTROL(6));
	intel_logical_ring_emit(ringbuf, flags);
	intel_logical_ring_emit(ringbuf, scratch_addr);
	intel_logical_ring_emit(ringbuf, 0);
	intel_logical_ring_emit(ringbuf, 0);
	intel_logical_ring_emit(ringbuf, 0);

	/* Padding to align with cache line */
	intel_logical_ring_emit(ringbuf, 0);
	intel_logical_ring_emit(ringbuf, 0);
	intel_logical_ring_emit(ringbuf, 0);

	/*
	 * No MI_BATCH_BUFFER_END is required in Indirect ctx BB because
	 * execution depends on the size defined in CTX_RCS_INDIRECT_CTX
	 */

	return 0;
}

static int gen8_init_perctx_bb(struct intel_engine_cs *ring,
			       struct intel_context *ctx)
{
	unsigned long flags = 0;
	u32 scratch_addr;
	struct intel_ringbuffer *ringbuf = NULL;

	if (!get_pipe_control_scratch_addr(ring)) {
		DRM_ERROR("scratch page not allocated for %s\n", ring->name);
		return -EINVAL;
	}

	ringbuf = create_wa_bb(ctx, ring, PAGE_SIZE);
	if (!ringbuf)
		return -ENOMEM;

	ctx->per_ctx_wa_bb = ringbuf;

	/* Actual scratch location is at 128 bytes offset */
	scratch_addr = get_pipe_control_scratch_addr(ring) + 2*CACHELINE_BYTES;
	scratch_addr |= PIPE_CONTROL_GLOBAL_GTT;

	/* WaDisableCtxRestoreArbitration:bdw,chv */
	intel_logical_ring_emit(ringbuf, MI_ARB_ON_OFF | MI_ARB_ENABLE);

	/*
	 * As per Bspec, to workaround a known HW issue, SW must perform the
	 * below programming sequence prior to programming MI_BATCH_BUFFER_END.
	 *
	 * This is only applicable for Gen8.
	 */

	/* WaRsRestoreWithPerCtxtBb:bdw,chv */
	intel_logical_ring_emit(ringbuf, MI_LOAD_REGISTER_IMM(1));
	intel_logical_ring_emit(ringbuf, INSTPM);
	intel_logical_ring_emit(ringbuf,
				_MASKED_BIT_DISABLE(INSTPM_FORCE_ORDERING));

	flags = MI_ATOMIC_MEMORY_TYPE_GGTT |
		MI_ATOMIC_INLINE_DATA |
		MI_ATOMIC_CS_STALL |
		MI_ATOMIC_RETURN_DATA_CTL |
		MI_ATOMIC_MOVE;

	intel_logical_ring_emit(ringbuf, MI_ATOMIC(5) | flags);
	intel_logical_ring_emit(ringbuf, scratch_addr);
	intel_logical_ring_emit(ringbuf, 0);
	intel_logical_ring_emit(ringbuf,
				_MASKED_BIT_ENABLE(INSTPM_FORCE_ORDERING));
	intel_logical_ring_emit(ringbuf,
				_MASKED_BIT_ENABLE(INSTPM_FORCE_ORDERING));

	/*
	 * Bspec says MI_LOAD_REGISTER_MEM, MI_LOAD_REGISTER_REG and
	 * MI_BATCH_BUFFER_END need to be in the same cacheline.
	 */
	while (((unsigned long) ringbuf->tail % CACHELINE_BYTES) != 0)
		intel_logical_ring_emit(ringbuf, MI_NOOP);

	intel_logical_ring_emit(ringbuf,
				MI_LOAD_REGISTER_MEM |
				MI_LRM_USE_GLOBAL_GTT |
				MI_LRM_ASYNC_MODE_ENABLE);
	intel_logical_ring_emit(ringbuf, INSTPM);
	intel_logical_ring_emit(ringbuf, scratch_addr);

	/*
	 * Bspec says there should not be any commands programmed
	 * between MI_LOAD_REGISTER_REG and MI_BATCH_BUFFER_END so
	 * do not add any new commands
	 */
	intel_logical_ring_emit(ringbuf, MI_LOAD_REGISTER_REG);
	intel_logical_ring_emit(ringbuf, GEN8_RS_PREEMPT_STATUS);
	intel_logical_ring_emit(ringbuf, GEN8_RS_PREEMPT_STATUS);
	/* Padding */
	intel_logical_ring_emit(ringbuf, MI_NOOP);

	intel_logical_ring_emit(ringbuf, MI_BATCH_BUFFER_END);

	return 0;
}

static int intel_init_workaround_bb(struct intel_engine_cs *ring,
				    struct intel_context *ctx)
{
	int ret;
	struct drm_device *dev = ring->dev;

	WARN_ON(ring->id != RCS);

	if (IS_GEN8(dev)) {
		ret = gen8_init_indirectctx_bb(ring, ctx);
		if (ret)
			return ret;

		ret = gen8_init_perctx_bb(ring, ctx);
		if (ret)
			return ret;
	}

	return 0;

}

static int gen8_init_common_ring(struct intel_engine_cs *ring)
{
	struct drm_device *dev = ring->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;

	I915_WRITE_IMR(ring, ~(ring->irq_enable_mask | ring->irq_keep_mask));
	I915_WRITE(RING_HWSTAM(ring->mmio_base), 0xffffffff);

	I915_WRITE(RING_MODE_GEN7(ring),
		   _MASKED_BIT_DISABLE(GFX_REPLAY_MODE) |
		   _MASKED_BIT_ENABLE(GFX_RUN_LIST_ENABLE));
	POSTING_READ(RING_MODE_GEN7(ring));
	DRM_DEBUG_DRIVER("Execlists enabled for %s\n", ring->name);

	return 0;
}

static int gen8_init_render_ring(struct intel_engine_cs *ring)
{
	struct drm_device *dev = ring->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret;

	ret = gen8_init_common_ring(ring);
	if (ret)
		return ret;

	/* We need to disable the AsyncFlip performance optimisations in order
	 * to use MI_WAIT_FOR_EVENT within the CS. It should already be
	 * programmed to '1' on all products.
	 *
	 * WaDisableAsyncFlipPerfMode:snb,ivb,hsw,vlv,bdw,chv
	 */
	I915_WRITE(MI_MODE, _MASKED_BIT_ENABLE(ASYNC_FLIP_PERF_DISABLE));

	ret = intel_init_pipe_control(ring);
	if (ret)
		return ret;

	I915_WRITE(INSTPM, _MASKED_BIT_ENABLE(INSTPM_FORCE_ORDERING));

	return init_workarounds_ring(ring);
}

static int gen8_emit_bb_start(struct intel_ringbuffer *ringbuf,
			      u64 offset, unsigned flags)
{
	bool ppgtt = !(flags & I915_DISPATCH_SECURE);
	int ret;

	ret = intel_logical_ring_begin(ringbuf, 4);
	if (ret)
		return ret;

	/* FIXME(BDW): Address space and security selectors. */
	intel_logical_ring_emit(ringbuf, MI_BATCH_BUFFER_START_GEN8 |
			(ppgtt<<8) |
			(flags &
			 I915_DISPATCH_RS ? MI_BATCH_RESOURCE_STREAMER : 0));
	intel_logical_ring_emit(ringbuf, lower_32_bits(offset));
	intel_logical_ring_emit(ringbuf, upper_32_bits(offset));
	intel_logical_ring_emit(ringbuf, MI_NOOP);
	intel_logical_ring_advance(ringbuf);

	return 0;
}

static bool gen8_logical_ring_get_irq(struct intel_engine_cs *ring)
{
	struct drm_device *dev = ring->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	unsigned long flags;

	if (!dev->irq_enabled)
		return false;

	spin_lock_irqsave(&dev_priv->irq_lock, flags);
	if (ring->irq_refcount++ == 0) {
		I915_WRITE_IMR(ring, ~(ring->irq_enable_mask | ring->irq_keep_mask));
		POSTING_READ(RING_IMR(ring->mmio_base));
	}
	spin_unlock_irqrestore(&dev_priv->irq_lock, flags);

	return true;
}

static void gen8_logical_ring_put_irq(struct intel_engine_cs *ring)
{
	struct drm_device *dev = ring->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	unsigned long flags;

	spin_lock_irqsave(&dev_priv->irq_lock, flags);
	if (--ring->irq_refcount == 0) {
		I915_WRITE_IMR(ring, ~ring->irq_keep_mask);
		POSTING_READ(RING_IMR(ring->mmio_base));
	}
	spin_unlock_irqrestore(&dev_priv->irq_lock, flags);
}

static int gen8_emit_flush(struct intel_ringbuffer *ringbuf,
			   u32 invalidate_domains,
			   u32 unused)
{
	struct intel_engine_cs *ring = ringbuf->ring;
	struct drm_device *dev = ring->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	uint32_t cmd;
	int ret;

	ret = intel_logical_ring_begin(ringbuf, 4);
	if (ret)
		return ret;

	cmd = MI_FLUSH_DW + 1;

	if (ring == &dev_priv->ring[VCS]) {
		if (invalidate_domains & I915_GEM_GPU_DOMAINS)
			cmd |= MI_INVALIDATE_TLB | MI_INVALIDATE_BSD |
				MI_FLUSH_DW_STORE_INDEX |
				MI_FLUSH_DW_OP_STOREDW;
	} else {
		if (invalidate_domains & I915_GEM_DOMAIN_RENDER)
			cmd |= MI_INVALIDATE_TLB | MI_FLUSH_DW_STORE_INDEX |
				MI_FLUSH_DW_OP_STOREDW;
	}

	intel_logical_ring_emit(ringbuf, cmd);
	intel_logical_ring_emit(ringbuf,
				I915_GEM_HWS_SCRATCH_ADDR |
				MI_FLUSH_DW_USE_GTT);
	intel_logical_ring_emit(ringbuf, 0); /* upper addr */
	intel_logical_ring_emit(ringbuf, 0); /* value */
	intel_logical_ring_advance(ringbuf);

	return 0;
}

static int gen8_emit_flush_render(struct intel_ringbuffer *ringbuf,
				  u32 invalidate_domains,
				  u32 flush_domains)
{
	struct intel_engine_cs *ring = ringbuf->ring;
	u32 scratch_addr = ring->scratch.gtt_offset + 2 * CACHELINE_BYTES;
	u32 flags = 0;
	int ret;

	flags |= PIPE_CONTROL_CS_STALL;

	if (flush_domains) {
		flags |= PIPE_CONTROL_RENDER_TARGET_CACHE_FLUSH;
		flags |= PIPE_CONTROL_DEPTH_CACHE_FLUSH;
	}

	if (invalidate_domains) {
		flags |= PIPE_CONTROL_TLB_INVALIDATE;
		flags |= PIPE_CONTROL_INSTRUCTION_CACHE_INVALIDATE;
		flags |= PIPE_CONTROL_TEXTURE_CACHE_INVALIDATE;
		flags |= PIPE_CONTROL_VF_CACHE_INVALIDATE;
		flags |= PIPE_CONTROL_CONST_CACHE_INVALIDATE;
		flags |= PIPE_CONTROL_STATE_CACHE_INVALIDATE;
		flags |= PIPE_CONTROL_QW_WRITE;
		flags |= PIPE_CONTROL_GLOBAL_GTT_IVB;
	}

	ret = intel_logical_ring_begin(ringbuf, 6);
	if (ret)
		return ret;

	intel_logical_ring_emit(ringbuf, GFX_OP_PIPE_CONTROL(6));
	intel_logical_ring_emit(ringbuf, flags);
	intel_logical_ring_emit(ringbuf, scratch_addr);
	intel_logical_ring_emit(ringbuf, 0);
	intel_logical_ring_emit(ringbuf, 0);
	intel_logical_ring_emit(ringbuf, 0);
	intel_logical_ring_advance(ringbuf);

	return 0;
}

static u32 gen8_get_seqno(struct intel_engine_cs *ring, bool lazy_coherency)
{
	return intel_read_status_page(ring, I915_GEM_HWS_INDEX);
}

static void gen8_set_seqno(struct intel_engine_cs *ring, u32 seqno)
{
	intel_write_status_page(ring, I915_GEM_HWS_INDEX, seqno);
}

static int gen8_emit_request(struct intel_ringbuffer *ringbuf)
{
	struct intel_engine_cs *ring = ringbuf->ring;
	u32 cmd;
	int ret;

	ret = intel_logical_ring_begin(ringbuf, 6);
	if (ret)
		return ret;

	cmd = MI_STORE_DWORD_IMM_GEN4;
	cmd |= MI_GLOBAL_GTT;

	intel_logical_ring_emit(ringbuf, cmd);
	intel_logical_ring_emit(ringbuf,
				(ring->status_page.gfx_addr +
				(I915_GEM_HWS_INDEX << MI_STORE_DWORD_INDEX_SHIFT)));
	intel_logical_ring_emit(ringbuf, 0);
	intel_logical_ring_emit(ringbuf,
		i915_gem_request_get_seqno(ring->outstanding_lazy_request));
	intel_logical_ring_emit(ringbuf, MI_USER_INTERRUPT);
	intel_logical_ring_emit(ringbuf, MI_NOOP);
	intel_logical_ring_advance_and_submit(ringbuf);

	return 0;
}

static int
gen8_ring_disable(struct intel_engine_cs *ring, struct intel_context *ctx)
{
	struct drm_i915_private *dev_priv = (ring->dev)->dev_private;
	uint32_t ring_ctl = 0;
	int ret = 0;

	/* Request the ring to go idle */
	I915_WRITE_MODE(ring, _MASKED_BIT_ENABLE(RING_MODE_STOP));

	/* Disable the ring */
	ret = I915_READ_CTL_CTX(ring, ctx, ring_ctl);
	if (ret)
		return ret;

	ring_ctl &= (RING_NR_PAGES | RING_REPORT_MASK);
	I915_WRITE_CTL_CTX_MMIO(ring, ctx, ring_ctl);
	ring_ctl = I915_READ_CTL(ring);  /* Barrier read */

	WARN(!((ring_ctl & RING_VALID) == 0), "Failed to disable %s!",
		ring->name);

	return 0;
}

static int
gen8_ring_enable(struct intel_engine_cs *ring, struct intel_context *ctx)
{
	struct drm_i915_private *dev_priv = (ring->dev)->dev_private;
	uint32_t mode = 0;
	uint32_t ring_ctl = 0;
	uint32_t tail = 0;
	int ret = 0;

	ret = I915_READ_TAIL_CTX(ring, ctx, tail);
	if (ret)
		return ret;

	/* Clear the MI_MODE stop bit */
	I915_WRITE_MODE(ring, _MASKED_BIT_DISABLE(RING_MODE_STOP));
	mode = I915_READ_MODE(ring);    /* Barrier read */

	/* Enable the ring */
	ret = I915_READ_CTL_CTX(ring, ctx, ring_ctl);
	if (ret)
		return ret;

	ring_ctl &= (RING_NR_PAGES | RING_REPORT_MASK);
	I915_WRITE_CTL_CTX_MMIO(ring, ctx, ring_ctl | RING_VALID);
	ring_ctl = I915_READ_CTL(ring); /* Barrier read */

	/*
	 * After enabling the ring and updating the ring context
	 * do context resubmission to kick off hardware again.
	 */
	intel_execlists_TDR_context_queue(ring, ctx, tail);

	return 0;
}

/*
 * gen8_ring_save()
 *
 * Saves part of engine/context state to scratch memory while
 * engine is reset and reinitialized. The saved engine/context state
 * is as follows (in the stated order):
 *
 *	Context buffer control register of the currently hung context
 *
 *	Context tail register of the currently hung context
 *
 *	Nudged head MMIO register value of the currently hung engine.
 *	Before saving the head MMIO register we nudge it to be correctly
 *	aligned with a QWORD boundary. The reason this works even though
 *	the head register points to the first instruction following the
 *	hung batch buffer is that the driver also pads the instruction
 *	stream so that the third DWORD of the BB_START instruction is
 *	followed by a MI_NOOP. That means that we're skipping the MI_NOOP
 *	and end up at the first interesting instruction after the MI_NOOP.
 *
 * ring: engine under reset
 * ctx: context currently running on engine
 * data: scratch memory that holds state temporarily during reset.
 * size: number of 32-bit words stored in data
 * flags: information on how to nudge head when saving it to state memory
 */
static int
gen8_ring_save(struct intel_engine_cs *ring, struct intel_context *ctx,
		uint32_t *data, uint32_t data_size, u32 flags)
{
	struct drm_i915_private *dev_priv = ring->dev->dev_private;
	struct intel_ringbuffer *ringbuf = NULL;
	int ret = 0;
	int clamp_to_tail = 0;
	uint32_t ctl;
	uint32_t head;
	uint32_t tail;
	uint32_t head_addr;
	uint32_t tail_addr;
	uint32_t hws_pga;
	uint32_t hw_context_id1 = ~0u;
	uint32_t hw_context_id2 = ~0u;

	/*
	 * Expect no less space than for three registers:
	 * head, tail and ring buffer control
	 */
	if (data_size < GEN8_RING_CONTEXT_SIZE) {
		DRM_ERROR("State size is too small! (%u)\n", data_size);
		return -EINVAL;
	}

	if (!ring || !ctx) {
		WARN(!ring, "Ring is null! Ring state save failed!\n");
		WARN(!ctx, "Context is null! Ring state save failed!\n");
		return -EINVAL;
	}

	ringbuf = ctx->engine[ring->id].ringbuf;

	hw_context_id1 = I915_READ(RING_EXECLIST_STATUS_CTX_ID(ring));

	/*
	 * Read head from MMIO register since it contains the
	 * most up to date value of head at this point.
	 */
	head = I915_READ_HEAD(ring);

	hw_context_id2 = I915_READ(RING_EXECLIST_STATUS_CTX_ID(ring));

	if (hw_context_id1 != hw_context_id2) {
		WARN(1, "Somehow the currently running context has changed " \
			"beneath our feet (%x != %x)! Bailing and retrying!\n",
			(unsigned int) hw_context_id1,
			(unsigned int) hw_context_id2);

		return -EAGAIN;
	}

	/*
	 * Read tail from the context because the execlist queue
	 * updates the tail value there first during submission.
	 * The MMIO tail register is not be updated until the actual
	 * ring submission is completed.
	 */
	ret = I915_READ_TAIL_CTX(ring, ctx, tail);
	if (ret)
		return ret;

	/*
	 * head_addr and tail_addr are the head and tail values
	 * excluding ring wrapping information and aligned to DWORD
	 * boundary
	 */
	head_addr = head & HEAD_ADDR;
	tail_addr = tail & TAIL_ADDR;

	/*
	 * The head must always chase the tail.
	 * If the tail is beyond the head then do not allow
	 * the head to overtake it. If the tail is less than
	 * the head then the tail has already wrapped and
	 * there is no problem in advancing the head or even
	 * wrapping the head back to 0 as worst case it will
	 * become equal to tail
	 */
	if (head_addr <= tail_addr)
		clamp_to_tail = 1;

	if (flags & FORCE_ADVANCE) {

		/* Force head pointer to next QWORD boundary */
		head_addr &= ~0x7;
		head_addr += 8;
		DRM_DEBUG_TDR("Forced head to 0x%08x\n", (unsigned int) head_addr);

	} else if (head & 0x7) {

		/* Ensure head pointer is pointing to a QWORD boundary */
		DRM_DEBUG_TDR("Rounding up head 0x%08x\n", (unsigned int) head);
		head += 0x7;
		head &= ~0x7;
		head_addr = head;
	}

	if (clamp_to_tail && (head_addr > tail_addr)) {
		head_addr = tail_addr;
	} else if (head_addr >= ringbuf->size) {
		/* Wrap head back to start if it exceeds ring size*/
		head_addr = 0;
	}

	/* Update the register */
	head &= ~HEAD_ADDR;
	head |= (head_addr & HEAD_ADDR);

	/* Save ring control register */
	ret = I915_READ_CTL_CTX(ring, ctx, ctl);
	if (ret)
		return ret;
	ctl &= (RING_NR_PAGES | RING_REPORT_MASK);

	/* Save head and tail as 0 so they are reset on restore */
	if (flags & RESET_HEAD_TAIL)
		head = tail = 0;

	/* HW is losing HWS Page address after reset, save it */
	hws_pga = I915_READ(RING_HWS_PGA(ring->mmio_base));

	data[0] = ctl;
	data[1] = tail;

	/*
	 * Head will already have advanced to next instruction location
	 * even if the current instruction caused a hang, so we just
	 * save the current value as the value to restart at
	 */
	data[2] = head;
	data[3] = hws_pga;

	return 0;
}

static int
gen8_ring_restore(struct intel_engine_cs *ring, struct intel_context *ctx,
		uint32_t *data, uint32_t data_size)
{
	struct drm_i915_private *dev_priv = ring->dev->dev_private;
	uint32_t head;
	uint32_t tail;
	uint32_t ctl;
	uint32_t hws_pga;

	/*
	 * Expect no less space than for three registers:
	 * head, tail and ring buffer control
	 */
	if (data_size < GEN8_RING_CONTEXT_SIZE) {
		DRM_ERROR("State size is too small! (%u)\n", data_size);
		return -EINVAL;
	}

	/* Re-initialize ring */
	if (ring->init) {
		int ret = ring->init(ring);
		if (ret != 0) {
			DRM_ERROR("Failed to re-initialize %s\n",
					ring->name);
			return ret;
		}
	} else {
		DRM_ERROR("ring init function pointer not set up\n");
		return -EINVAL;
	}

	if (ring->id == RCS) {
		/*
		 * These register reinitializations are only located here
		 * temporarily until they are moved out of the
		 * init_clock_gating function to some function we can
		 * call from here.
		 */

		/* WaVSRefCountFullforceMissDisable:chv */
		/* WaDSRefCountFullforceMissDisable:chv */
		I915_WRITE(GEN7_FF_THREAD_MODE,
			   I915_READ(GEN7_FF_THREAD_MODE) &
			   ~(GEN8_FF_DS_REF_CNT_FFME | GEN7_FF_VS_REF_CNT_FFME));

		I915_WRITE(_3D_CHICKEN3,
			   _3D_CHICKEN_SDE_LIMIT_FIFO_POLY_DEPTH(2));

		/* WaSwitchSolVfFArbitrationPriority:bdw */
		I915_WRITE(GAM_ECOCHK, I915_READ(GAM_ECOCHK) | HSW_ECOCHK_ARB_PRIO_SOL);
	}

	ctl = data[0];
	tail = data[1];
	head = data[2];
	hws_pga = data[3];

	/* Restore head, tail ring buffer control and hws page address */

	I915_WRITE_HEAD_CTX_MMIO(ring, ctx, head);
	I915_WRITE_TAIL(ring, tail);
	I915_WRITE_CTL_CTX_MMIO(ring, ctx, ctl);
	I915_WRITE(RING_HWS_PGA(ring->mmio_base), hws_pga);

	return 0;
}


/**
 * intel_logical_ring_cleanup() - deallocate the Engine Command Streamer
 *
 * @ring: Engine Command Streamer.
 *
 */
void intel_logical_ring_cleanup(struct intel_engine_cs *ring)
{
	struct drm_i915_private *dev_priv;

	if (!intel_ring_initialized(ring))
		return;

	dev_priv = ring->dev->dev_private;

	intel_logical_ring_stop(ring);
	WARN_ON((I915_READ_MODE(ring) & RING_MODE_IDLE) == 0);

	i915_gem_request_assign(&ring->outstanding_lazy_request, NULL);

	if (ring->cleanup)
		ring->cleanup(ring);

	i915_cmd_parser_fini_ring(ring);

	if (ring->status_page.obj) {
		kunmap(sg_page(ring->status_page.obj->pages->sgl));
		ring->status_page.obj = NULL;
	}
}

static int logical_ring_init(struct drm_device *dev, struct intel_engine_cs *ring)
{
	int ret;

	/* Intentionally left blank. */
	ring->buffer = NULL;

	ring->dev = dev;
	INIT_LIST_HEAD(&ring->active_list);
	INIT_LIST_HEAD(&ring->request_list);
	spin_lock_init(&ring->reqlist_lock);
	init_waitqueue_head(&ring->irq_queue);
	INIT_LIST_HEAD(&ring->delayed_free_list);

	INIT_LIST_HEAD(&ring->execlist_queue);
	INIT_LIST_HEAD(&ring->execlist_retired_req_list);
	spin_lock_init(&ring->execlist_lock);
	ring->next_context_status_buffer = 0;

	ret = i915_cmd_parser_init_ring(ring);
	if (ret)
		return ret;

	if (ring->init) {
		ret = ring->init(ring);
		if (ret)
			return ret;
	}

	ret = intel_lr_context_deferred_create(ring->default_context, ring);

	return ret;
}

static int logical_render_ring_init(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_engine_cs *ring = &dev_priv->ring[RCS];

	ring->name = "render ring";
	ring->id = RCS;
	ring->mmio_base = RENDER_RING_BASE;
	ring->irq_enable_mask =
		GT_RENDER_USER_INTERRUPT << GEN8_RCS_IRQ_SHIFT;
	ring->irq_keep_mask =
		GT_CONTEXT_SWITCH_INTERRUPT << GEN8_RCS_IRQ_SHIFT;
	if (HAS_L3_DPF(dev))
		ring->irq_keep_mask |= GT_RENDER_L3_PARITY_ERROR_INTERRUPT;

	ring->irq_keep_mask |=
		(GT_GEN8_RCS_WATCHDOG_INTERRUPT << GEN8_RCS_IRQ_SHIFT);

	ring->init = gen8_init_render_ring;
	ring->init_context = intel_logical_ring_workarounds_emit;
	ring->init_context_bb = intel_init_workaround_bb;
	ring->cleanup = intel_fini_pipe_control;
	ring->get_seqno = gen8_get_seqno;
	ring->set_seqno = gen8_set_seqno;
	ring->emit_request = gen8_emit_request;
	ring->emit_flush = gen8_emit_flush_render;
	ring->irq_get = gen8_logical_ring_get_irq;
	ring->irq_put = gen8_logical_ring_put_irq;
	ring->emit_bb_start = gen8_emit_bb_start;
	ring->enable = gen8_ring_enable;
	ring->disable = gen8_ring_disable;
	ring->save = gen8_ring_save;
	ring->restore = gen8_ring_restore;

	return logical_ring_init(dev, ring);
}

static int logical_bsd_ring_init(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_engine_cs *ring = &dev_priv->ring[VCS];

	ring->name = "bsd ring";
	ring->id = VCS;
	ring->mmio_base = GEN6_BSD_RING_BASE;
	ring->irq_enable_mask =
		GT_RENDER_USER_INTERRUPT << GEN8_VCS1_IRQ_SHIFT;
	ring->irq_keep_mask =
		GT_CONTEXT_SWITCH_INTERRUPT << GEN8_VCS1_IRQ_SHIFT;
	ring->irq_keep_mask |=
		(GT_GEN8_VCS_WATCHDOG_INTERRUPT << GEN8_VCS1_IRQ_SHIFT);

	ring->init = gen8_init_common_ring;
	ring->get_seqno = gen8_get_seqno;
	ring->set_seqno = gen8_set_seqno;
	ring->emit_request = gen8_emit_request;
	ring->emit_flush = gen8_emit_flush;
	ring->irq_get = gen8_logical_ring_get_irq;
	ring->irq_put = gen8_logical_ring_put_irq;
	ring->emit_bb_start = gen8_emit_bb_start;
	ring->enable = gen8_ring_enable;
	ring->disable = gen8_ring_disable;
	ring->save = gen8_ring_save;
	ring->restore = gen8_ring_restore;

	return logical_ring_init(dev, ring);
}

static int logical_bsd2_ring_init(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_engine_cs *ring = &dev_priv->ring[VCS2];

	ring->name = "bds2 ring";
	ring->id = VCS2;
	ring->mmio_base = GEN8_BSD2_RING_BASE;
	ring->irq_enable_mask =
		GT_RENDER_USER_INTERRUPT << GEN8_VCS2_IRQ_SHIFT;
	ring->irq_keep_mask =
		GT_CONTEXT_SWITCH_INTERRUPT << GEN8_VCS2_IRQ_SHIFT;
	ring->irq_keep_mask |=
		(GT_GEN8_VCS_WATCHDOG_INTERRUPT << GEN8_VCS2_IRQ_SHIFT);

	ring->init = gen8_init_common_ring;
	ring->get_seqno = gen8_get_seqno;
	ring->set_seqno = gen8_set_seqno;
	ring->emit_request = gen8_emit_request;
	ring->emit_flush = gen8_emit_flush;
	ring->irq_get = gen8_logical_ring_get_irq;
	ring->irq_put = gen8_logical_ring_put_irq;
	ring->emit_bb_start = gen8_emit_bb_start;
	ring->enable = gen8_ring_enable;
	ring->disable = gen8_ring_disable;
	ring->save = gen8_ring_save;
	ring->restore = gen8_ring_restore;

	return logical_ring_init(dev, ring);
}

static int logical_blt_ring_init(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_engine_cs *ring = &dev_priv->ring[BCS];

	ring->name = "blitter ring";
	ring->id = BCS;
	ring->mmio_base = BLT_RING_BASE;
	ring->irq_enable_mask =
		GT_RENDER_USER_INTERRUPT << GEN8_BCS_IRQ_SHIFT;
	ring->irq_keep_mask =
		GT_CONTEXT_SWITCH_INTERRUPT << GEN8_BCS_IRQ_SHIFT;

	ring->init = gen8_init_common_ring;
	ring->get_seqno = gen8_get_seqno;
	ring->set_seqno = gen8_set_seqno;
	ring->emit_request = gen8_emit_request;
	ring->emit_flush = gen8_emit_flush;
	ring->irq_get = gen8_logical_ring_get_irq;
	ring->irq_put = gen8_logical_ring_put_irq;
	ring->emit_bb_start = gen8_emit_bb_start;
	ring->enable = gen8_ring_enable;
	ring->disable = gen8_ring_disable;
	ring->save = gen8_ring_save;
	ring->restore = gen8_ring_restore;

	return logical_ring_init(dev, ring);
}

static int logical_vebox_ring_init(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_engine_cs *ring = &dev_priv->ring[VECS];

	ring->name = "video enhancement ring";
	ring->id = VECS;
	ring->mmio_base = VEBOX_RING_BASE;
	ring->irq_enable_mask =
		GT_RENDER_USER_INTERRUPT << GEN8_VECS_IRQ_SHIFT;
	ring->irq_keep_mask =
		GT_CONTEXT_SWITCH_INTERRUPT << GEN8_VECS_IRQ_SHIFT;

	ring->init = gen8_init_common_ring;
	ring->get_seqno = gen8_get_seqno;
	ring->set_seqno = gen8_set_seqno;
	ring->emit_request = gen8_emit_request;
	ring->emit_flush = gen8_emit_flush;
	ring->irq_get = gen8_logical_ring_get_irq;
	ring->irq_put = gen8_logical_ring_put_irq;
	ring->emit_bb_start = gen8_emit_bb_start;
	ring->enable = gen8_ring_enable;
	ring->disable = gen8_ring_disable;
	ring->save = gen8_ring_save;
	ring->restore = gen8_ring_restore;

	return logical_ring_init(dev, ring);
}

/**
 * intel_logical_rings_init() - allocate, populate and init the Engine Command Streamers
 * @dev: DRM device.
 *
 * This function inits the engines for an Execlists submission style (the equivalent in the
 * legacy ringbuffer submission world would be i915_gem_init_rings). It does it only for
 * those engines that are present in the hardware.
 *
 * Return: non-zero if the initialization failed.
 */
int intel_logical_rings_init(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret;

	ret = logical_render_ring_init(dev);
	if (ret)
		return ret;

	if (HAS_BSD(dev)) {
		ret = logical_bsd_ring_init(dev);
		if (ret)
			goto cleanup_render_ring;
	}

	if (HAS_BLT(dev)) {
		ret = logical_blt_ring_init(dev);
		if (ret)
			goto cleanup_bsd_ring;
	}

	if (HAS_VEBOX(dev)) {
		ret = logical_vebox_ring_init(dev);
		if (ret)
			goto cleanup_blt_ring;
	}

	if (HAS_BSD2(dev)) {
		ret = logical_bsd2_ring_init(dev);
		if (ret)
			goto cleanup_vebox_ring;
	}

	ret = i915_gem_set_seqno(dev, ((u32)~0 - 0x1000));
	if (ret)
		goto cleanup_bsd2_ring;

	return 0;

cleanup_bsd2_ring:
	intel_logical_ring_cleanup(&dev_priv->ring[VCS2]);
cleanup_vebox_ring:
	intel_logical_ring_cleanup(&dev_priv->ring[VECS]);
cleanup_blt_ring:
	intel_logical_ring_cleanup(&dev_priv->ring[BCS]);
cleanup_bsd_ring:
	intel_logical_ring_cleanup(&dev_priv->ring[VCS]);
cleanup_render_ring:
	intel_logical_ring_cleanup(&dev_priv->ring[RCS]);

	return ret;
}

int intel_lr_context_render_state_init(struct intel_engine_cs *ring,
				       struct intel_context *ctx)
{
	struct intel_ringbuffer *ringbuf = ctx->engine[ring->id].ringbuf;
	struct render_state so;
	struct drm_i915_file_private *file_priv = ctx->file_priv;
	struct drm_file *file = file_priv ? file_priv->file : NULL;
	int ret;

	ret = i915_gem_render_state_prepare(ring, &so);
	if (ret)
		return ret;

	if (so.rodata == NULL)
		return 0;

	ret = ring->emit_bb_start(ringbuf,
			so.ggtt_offset,
			I915_DISPATCH_SECURE);
	if (ret)
		goto out;

	i915_vma_move_to_active(i915_gem_obj_to_ggtt(so.obj), ring);

	ret = __i915_add_request(ring, file, so.obj, true);
	/* intel_logical_ring_add_request moves object to inactive if it
	 * fails */
out:
	i915_gem_render_state_fini(&so);
	return ret;
}

static int
populate_lr_context(struct intel_context *ctx, struct drm_i915_gem_object *ctx_obj,
		    struct intel_engine_cs *ring, struct intel_ringbuffer *ringbuf)
{
	struct drm_device *dev = ring->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct i915_hw_ppgtt *ppgtt = ctx->ppgtt;
	struct page *page;
	uint32_t *reg_state;
	int ret;

	if (!ppgtt)
		ppgtt = dev_priv->mm.aliasing_ppgtt;

	ret = i915_gem_object_set_to_cpu_domain(ctx_obj, true);
	if (ret) {
		DRM_DEBUG_DRIVER("Could not set to CPU domain\n");
		return ret;
	}

	ret = i915_gem_object_get_pages(ctx_obj);
	if (ret) {
		DRM_DEBUG_DRIVER("Could not get object pages\n");
		return ret;
	}

	i915_gem_object_pin_pages(ctx_obj);

	/* The second page of the context object contains some fields which must
	 * be set up prior to the first execution. */
	page = i915_gem_object_get_page(ctx_obj, 1);
	reg_state = kmap_atomic(page);

	/* A context is actually a big batch buffer with several MI_LOAD_REGISTER_IMM
	 * commands followed by (reg, value) pairs. The values we are setting here are
	 * only for the first context restore: on a subsequent save, the GPU will
	 * recreate this batchbuffer with new values (including all the missing
	 * MI_LOAD_REGISTER_IMM commands that we are not initializing here). */
	if (ring->id == RCS)
		reg_state[CTX_LRI_HEADER_0] = MI_LOAD_REGISTER_IMM(14);
	else
		reg_state[CTX_LRI_HEADER_0] = MI_LOAD_REGISTER_IMM(11);
	reg_state[CTX_LRI_HEADER_0] |= MI_LRI_FORCE_POSTED;
	reg_state[CTX_CONTEXT_CONTROL] = RING_CONTEXT_CONTROL(ring);
	reg_state[CTX_CONTEXT_CONTROL+1] =
			_MASKED_BIT_ENABLE((1<<3) | MI_RESTORE_INHIBIT);
	reg_state[CTX_RING_HEAD] = RING_HEAD(ring->mmio_base);
	reg_state[CTX_RING_HEAD+1] = 0;
	reg_state[CTX_RING_TAIL] = RING_TAIL(ring->mmio_base);
	reg_state[CTX_RING_TAIL+1] = 0;
	reg_state[CTX_RING_BUFFER_START] = RING_START(ring->mmio_base);
	/* Ring buffer start address is not known until the buffer is pinned.
	 * It is written to the context image in execlists_update_context()
	 */
	reg_state[CTX_RING_BUFFER_CONTROL] = RING_CTL(ring->mmio_base);
	reg_state[CTX_RING_BUFFER_CONTROL+1] =
			((ringbuf->size - PAGE_SIZE) & RING_NR_PAGES) | RING_VALID;
	reg_state[CTX_BB_HEAD_U] = ring->mmio_base + 0x168;
	reg_state[CTX_BB_HEAD_U+1] = 0;
	reg_state[CTX_BB_HEAD_L] = ring->mmio_base + 0x140;
	reg_state[CTX_BB_HEAD_L+1] = 0;
	reg_state[CTX_BB_STATE] = ring->mmio_base + 0x110;
	reg_state[CTX_BB_STATE+1] = (1<<5);
	reg_state[CTX_SECOND_BB_HEAD_U] = ring->mmio_base + 0x11c;
	reg_state[CTX_SECOND_BB_HEAD_U+1] = 0;
	reg_state[CTX_SECOND_BB_HEAD_L] = ring->mmio_base + 0x114;
	reg_state[CTX_SECOND_BB_HEAD_L+1] = 0;
	reg_state[CTX_SECOND_BB_STATE] = ring->mmio_base + 0x118;
	reg_state[CTX_SECOND_BB_STATE+1] = 0;
	if (ring->id == RCS) {
		reg_state[CTX_BB_PER_CTX_PTR] = ring->mmio_base + 0x1c0;

		if (ctx->per_ctx_wa_bb)
			reg_state[CTX_BB_PER_CTX_PTR + 1] =
				i915_gem_obj_ggtt_offset(
					ctx->per_ctx_wa_bb->obj) | 0x01;
		else
			reg_state[CTX_BB_PER_CTX_PTR+1] = 0;

		reg_state[CTX_RCS_INDIRECT_CTX] = ring->mmio_base + 0x1c4;
		reg_state[CTX_RCS_INDIRECT_CTX_OFFSET] = ring->mmio_base + 0x1c8;

		if (ctx->indirect_ctx_wa_bb) {
			reg_state[CTX_RCS_INDIRECT_CTX + 1] =
				i915_gem_obj_ggtt_offset(
				ctx->indirect_ctx_wa_bb->obj) | 0x01;

			reg_state[CTX_RCS_INDIRECT_CTX_OFFSET + 1] =
				CTX_RCS_INDIRECT_CTX_OFFSET_DEFAULT << 6;
		} else {
			reg_state[CTX_RCS_INDIRECT_CTX+1] = 0;
			reg_state[CTX_RCS_INDIRECT_CTX_OFFSET+1] = 0;
		}
	}
	reg_state[CTX_LRI_HEADER_1] = MI_LOAD_REGISTER_IMM(9);
	reg_state[CTX_LRI_HEADER_1] |= MI_LRI_FORCE_POSTED;
	reg_state[CTX_CTX_TIMESTAMP] = ring->mmio_base + 0x3a8;
	reg_state[CTX_CTX_TIMESTAMP+1] = 0;
	reg_state[CTX_PDP3_UDW] = GEN8_RING_PDP_UDW(ring, 3);
	reg_state[CTX_PDP3_LDW] = GEN8_RING_PDP_LDW(ring, 3);
	reg_state[CTX_PDP2_UDW] = GEN8_RING_PDP_UDW(ring, 2);
	reg_state[CTX_PDP2_LDW] = GEN8_RING_PDP_LDW(ring, 2);
	reg_state[CTX_PDP1_UDW] = GEN8_RING_PDP_UDW(ring, 1);
	reg_state[CTX_PDP1_LDW] = GEN8_RING_PDP_LDW(ring, 1);
	reg_state[CTX_PDP0_UDW] = GEN8_RING_PDP_UDW(ring, 0);
	reg_state[CTX_PDP0_LDW] = GEN8_RING_PDP_LDW(ring, 0);
	reg_state[CTX_PDP3_UDW+1] = upper_32_bits(ppgtt->pd_dma_addr[3]);
	reg_state[CTX_PDP3_LDW+1] = lower_32_bits(ppgtt->pd_dma_addr[3]);
	reg_state[CTX_PDP2_UDW+1] = upper_32_bits(ppgtt->pd_dma_addr[2]);
	reg_state[CTX_PDP2_LDW+1] = lower_32_bits(ppgtt->pd_dma_addr[2]);
	reg_state[CTX_PDP1_UDW+1] = upper_32_bits(ppgtt->pd_dma_addr[1]);
	reg_state[CTX_PDP1_LDW+1] = lower_32_bits(ppgtt->pd_dma_addr[1]);
	reg_state[CTX_PDP0_UDW+1] = upper_32_bits(ppgtt->pd_dma_addr[0]);
	reg_state[CTX_PDP0_LDW+1] = lower_32_bits(ppgtt->pd_dma_addr[0]);
	if (ring->id == RCS) {
		reg_state[CTX_LRI_HEADER_2] = MI_LOAD_REGISTER_IMM(1);
		reg_state[CTX_R_PWR_CLK_STATE] = 0x20c8;
		reg_state[CTX_R_PWR_CLK_STATE+1] = 0;
	}

	kunmap_atomic(reg_state);

	ctx_obj->dirty = 1;
	set_page_dirty(page);
	i915_gem_object_unpin_pages(ctx_obj);

	return 0;
}

/**
 * intel_lr_context_free() - free the LRC specific bits of a context
 * @ctx: the LR context to free.
 *
 * The real context freeing is done in i915_gem_context_free: this only
 * takes care of the bits that are LRC related: the per-engine backing
 * objects and the logical ringbuffer.
 */
void intel_lr_context_free(struct intel_context *ctx)
{
	int i;

	for (i = 0; i < I915_NUM_RINGS; i++) {
		struct drm_i915_gem_object *ctx_obj = ctx->engine[i].state;

		if (ctx_obj) {
			struct intel_ringbuffer *ringbuf =
					ctx->engine[i].ringbuf;
			struct intel_engine_cs *ring = ringbuf->ring;

			if (ctx == ring->default_context) {
				intel_unpin_ringbuffer_obj(ringbuf);
				i915_gem_object_ggtt_unpin(ctx_obj);
			}
			intel_destroy_ringbuffer_obj(ringbuf);
			kfree(ringbuf);
			drm_gem_object_unreference(&ctx_obj->base);
		}
	}

	if (ctx->indirect_ctx_wa_bb) {
		intel_unpin_ringbuffer_obj(ctx->indirect_ctx_wa_bb);
		intel_destroy_ringbuffer_obj(ctx->indirect_ctx_wa_bb);
		kfree(ctx->indirect_ctx_wa_bb);
	}

	if (ctx->per_ctx_wa_bb) {
		intel_unpin_ringbuffer_obj(ctx->per_ctx_wa_bb);
		intel_destroy_ringbuffer_obj(ctx->per_ctx_wa_bb);
		kfree(ctx->per_ctx_wa_bb);
	}
}

static uint32_t get_lr_context_size(struct intel_engine_cs *ring)
{
	int ret = 0;

	WARN_ON(INTEL_INFO(ring->dev)->gen != 8);

	switch (ring->id) {
	case RCS:
		ret = GEN8_LR_CONTEXT_RENDER_SIZE;
		break;
	case VCS:
	case BCS:
	case VECS:
	case VCS2:
		ret = GEN8_LR_CONTEXT_OTHER_SIZE;
		break;
	}

	return ret;
}

static int lrc_setup_hardware_status_page(struct intel_engine_cs *ring,
		struct drm_i915_gem_object *default_ctx_obj)
{
	struct drm_i915_private *dev_priv = ring->dev->dev_private;

	/* The status page is offset 0 from the default context object
	 * in LRC mode. */
	ring->status_page.gfx_addr = i915_gem_obj_ggtt_offset(default_ctx_obj);
	ring->status_page.page_addr =
			kmap(sg_page(default_ctx_obj->pages->sgl));
	if (ring->status_page.page_addr == NULL)
		return -ENOMEM;
	ring->status_page.obj = default_ctx_obj;

	I915_WRITE(RING_HWS_PGA(ring->mmio_base),
			(u32)ring->status_page.gfx_addr);
	POSTING_READ(RING_HWS_PGA(ring->mmio_base));

	return 0;
}

/**
 * intel_lr_context_deferred_create() - create the LRC specific bits of a context
 * @ctx: LR context to create.
 * @ring: engine to be used with the context.
 *
 * This function can be called more than once, with different engines, if we plan
 * to use the context with them. The context backing objects and the ringbuffers
 * (specially the ringbuffer backing objects) suck a lot of memory up, and that's why
 * the creation is a deferred call: it's better to make sure first that we need to use
 * a given ring with the context.
 *
 * Return: non-zero on eror.
 */
int intel_lr_context_deferred_create(struct intel_context *ctx,
				     struct intel_engine_cs *ring)
{
	const bool is_global_default_ctx = (ctx == ring->default_context);
	struct drm_device *dev = ring->dev;
	struct drm_i915_gem_object *ctx_obj;
	uint32_t context_size;
	struct intel_ringbuffer *ringbuf;
	int ret;

	WARN_ON(ctx->legacy_hw_ctx.rcs_state != NULL);
	if (ctx->engine[ring->id].state)
		return 0;

	intel_runtime_pm_get(dev->dev_private);

	context_size = round_up(get_lr_context_size(ring), 4096);

	ctx_obj = i915_gem_alloc_context_obj(dev, context_size);
	if (IS_ERR(ctx_obj)) {
		ret = PTR_ERR(ctx_obj);
		DRM_DEBUG_DRIVER("Alloc LRC backing obj failed: %d\n", ret);
		goto error_pm;
	}

	if (is_global_default_ctx) {
		ret = i915_gem_obj_ggtt_pin(ctx_obj, GEN8_LR_CONTEXT_ALIGN, 0);
		if (ret) {
			DRM_DEBUG_DRIVER("Pin LRC backing obj failed: %d\n",
					ret);
			drm_gem_object_unreference(&ctx_obj->base);
			goto error_pm;
		}
	}

	ringbuf = kzalloc(sizeof(*ringbuf), GFP_KERNEL);
	if (!ringbuf) {
		DRM_DEBUG_DRIVER("Failed to allocate ringbuffer %s\n",
				ring->name);
		ret = -ENOMEM;
		goto error_unpin_ctx;
	}

	ringbuf->ring = ring;
	ringbuf->FIXME_lrc_ctx = ctx;

	ringbuf->size = 32 * PAGE_SIZE;
	ringbuf->effective_size = ringbuf->size;
	ringbuf->head = 0;
	ringbuf->tail = 0;
	ringbuf->last_retired_head = -1;
	intel_ring_update_space(ringbuf);

	if (ringbuf->obj == NULL) {
		ret = intel_alloc_ringbuffer_obj(dev, ringbuf);
		if (ret) {
			DRM_DEBUG_DRIVER(
				"Failed to allocate ringbuffer obj %s: %d\n",
				ring->name, ret);
			goto error_free_rbuf;
		}

		if (is_global_default_ctx) {
			ret = intel_pin_and_map_ringbuffer_obj(dev, ringbuf);
			if (ret) {
				DRM_ERROR(
					"Failed to pin and map ringbuffer %s: %d\n",
					ring->name, ret);
				goto error_destroy_rbuf;
			}
		}

	}

	if (ring->id == RCS && !ctx->rcs_initialized) {
		if (ring->init_context_bb) {
			ret = ring->init_context_bb(ring, ctx);
			if (ret) {
				DRM_ERROR("ring init context bb: %d\n", ret);
				goto error;
			}
		}
	}

	ret = populate_lr_context(ctx, ctx_obj, ring, ringbuf);
	if (ret) {
		DRM_DEBUG_DRIVER("Failed to populate LRC: %d\n", ret);
		goto error;
	}

	/* Create a timeline for HW Native Sync support*/
	ret = i915_sync_timeline_create(dev, ring->name, ctx, ring);
	if (ret) {
		DRM_ERROR("Sync timeline creation failed for ring %s, ctx %p\n",
			ring->name, ctx);
		goto error;
	}

	ctx->engine[ring->id].ringbuf = ringbuf;
	ctx->engine[ring->id].state = ctx_obj;

	if (ctx == ring->default_context) {
		ret = lrc_setup_hardware_status_page(ring, ctx_obj);
		if (ret) {
			DRM_ERROR("Failed to setup hardware status page\n");
			goto error;
		}
	}

	if (ring->id == RCS && !ctx->rcs_initialized) {
		if (ring->init_context) {
			ret = ring->init_context(ring, ctx);
			if (ret)
				DRM_ERROR("ring init context: %d\n", ret);
		}

		ret = intel_lr_context_render_state_init(ring, ctx);
		if (ret) {
			DRM_ERROR("Init render state failed: %d\n", ret);
			ctx->engine[ring->id].ringbuf = NULL;
			ctx->engine[ring->id].state = NULL;
			goto error;
		}

		ctx->rcs_initialized = true;
	}

	intel_runtime_pm_put(dev->dev_private);
	return 0;

error:
	if (ctx->indirect_ctx_wa_bb) {
		intel_unpin_ringbuffer_obj(ctx->indirect_ctx_wa_bb);
		intel_destroy_ringbuffer_obj(ctx->indirect_ctx_wa_bb);
		kfree(ctx->indirect_ctx_wa_bb);
	}
	if (ctx->per_ctx_wa_bb) {
		intel_unpin_ringbuffer_obj(ctx->per_ctx_wa_bb);
		intel_destroy_ringbuffer_obj(ctx->per_ctx_wa_bb);
		kfree(ctx->per_ctx_wa_bb);
	}

	if (is_global_default_ctx)
		intel_unpin_ringbuffer_obj(ringbuf);
error_destroy_rbuf:
	intel_destroy_ringbuffer_obj(ringbuf);
error_free_rbuf:
	kfree(ringbuf);
error_unpin_ctx:
	if (is_global_default_ctx)
		i915_gem_object_ggtt_unpin(ctx_obj);
	drm_gem_object_unreference(&ctx_obj->base);
error_pm:
	intel_runtime_pm_put(dev->dev_private);
	return ret;
}

/**
 * execlists_TDR_force_resubmit() - resubmit pending context if EXECLIST_STATUS
 * context ID is stuck to 0.
 *
 * @dev_priv: ...
 * @ringid: engine to resubmit context to.
 *
 * This function is simply a hack to work around a hardware oddity that
 * manifests itself through stuck context ID zero in EXECLIST_STATUS register
 * even though context is pending post-submission. There is no reason for this
 * hardware behaviour but until we have resolved this issue we need this
 * workaround.
 */
void intel_execlists_TDR_force_resubmit(struct drm_i915_private *dev_priv,
		unsigned ringid)
{
	unsigned long flags;
	struct intel_engine_cs *ring = &dev_priv->ring[ringid];
	struct intel_ctx_submit_request *req = NULL;
	struct intel_context *ctx = NULL;
	unsigned hw_context = I915_READ(RING_EXECLIST_STATUS_CTX_ID(ring));

	if (spin_is_locked(&ring->execlist_lock))
		return;
	else
		spin_lock_irqsave(&ring->execlist_lock, flags);

	if (hw_context) {
		WARN(1, "EXECLIST_STATUS context ID (%u) on %s is " \
			"not zero - no need for forced resubmission!\n",
			hw_context, ring->name);
		goto exit;
	}

	req = list_first_entry_or_null(&ring->execlist_queue,
			struct intel_ctx_submit_request, execlist_link);

	if (req) {
		if (req->ctx) {
			ctx = req->ctx;
			i915_gem_context_reference(ctx);

		} else {
			WARN(1, "No context in request %p!", req);
			goto exit;
		}
	} else {
		WARN(1, "No context submitted to %s!\n", ring->name);
		goto exit;
	}

	execlists_TDR_context_unqueue(ring);

exit:
	if (ctx)
		i915_gem_context_unreference(ctx);

	spin_unlock_irqrestore(&ring->execlist_lock, flags);
}

