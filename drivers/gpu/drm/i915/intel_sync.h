/*
 * Copyright © 2013 Intel Corporation
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *      Satyanantha RamaGopal M <rama.gopal.m.satyanantha@intel.com>
 *      Ian Lister <ian.lister@intel.com>
 *      Tvrtko Ursulin <tvrtko.ursulin@intel.com>
 */
#ifndef _INTEL_SYNC_H_
#define _INTEL_SYNC_H_

#include "../../../../drivers/staging/android/sync.h"

struct drm_i915_private;

#ifdef CONFIG_DRM_I915_SYNC
#define I915_SYNC_USER_INTERRUPTS (GT_RENDER_USER_INTERRUPT | \
				   GT_BSD_USER_INTERRUPT | \
				   GT_BLT_USER_INTERRUPT)
#else
#define I915_SYNC_USER_INTERRUPTS (0)
#endif

struct i915_sync_timeline {
	struct	sync_timeline	obj;

	struct {
		u32         value;
		u32         cycle;
		uint32_t    killed_at;
		uint32_t    next;
	} pvt;
};

struct i915_sync_pt {
	struct sync_pt		pt;

	struct drm_i915_gem_syncpt_driver_data pvt;
};

#ifdef CONFIG_DRM_I915_SYNC

int i915_sync_timeline_create(struct drm_device *dev,
			      const char *name,
			      struct intel_context *ctx,
			      struct intel_engine_cs *ring);

void i915_sync_timeline_destroy(struct intel_context *ctx,
				struct intel_engine_cs *ring);

int i915_sync_create_fence(struct drm_i915_gem_request *req,
			   int *fd_out, u64 ring_mask);

void i915_sync_timeline_advance(struct intel_context *ctx,
				struct intel_engine_cs *ring,
				uint32_t value);
void i915_sync_hung_ring(struct intel_engine_cs *ring);

#else

static inline
int i915_sync_timeline_create(struct drm_device *dev,
			      const char *name,
			      struct intel_context *ctx,
			      struct intel_engine_cs *ring)
{
	return 0;
}

static inline
void i915_sync_timeline_destroy(struct intel_context *ctx,
				struct intel_engine_cs *ring)
{

}

static int i915_sync_create_fence(struct drm_i915_gem_request *req,
				  int *fd_out, u64 ring_mask)
{
	return 0;
}

static inline
void i915_sync_timeline_advance(struct intel_context *ctx,
				struct intel_engine_cs *ring,
				uint32_t value)
{

}

static inline
void i915_sync_hung_ring(struct intel_engine_cs *ring)
{

}

#endif /* CONFIG_DRM_I915_SYNC */

#endif /* _INTEL_SYNC_H_ */
