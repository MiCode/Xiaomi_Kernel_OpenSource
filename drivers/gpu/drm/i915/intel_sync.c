/**************************************************************************
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
#include <linux/device.h>
#include "drmP.h"
#include "uapi/drm/drm.h"
#include "i915_drm.h"
#include "i915_drv.h"
#include "intel_drv.h"
#include "intel_sync.h"

static int i915_sync_pt_has_signaled(struct sync_pt *sync_pt)
{
	struct i915_sync_pt *pt = container_of(sync_pt,
					       struct i915_sync_pt, pt);
	struct i915_sync_timeline *obj =
		(struct i915_sync_timeline *)sync_pt->parent;

	/* On ring timeout fail the status of pending sync_pts.
	 * This callback is synchronous with the thread which calls
	 * sync_timeline_signal. If this has been signaled due to
	 * an error then timeline->killed_at will be set to the dead
	 * value.
	 */
	if (pt->pvt.value == obj->pvt.killed_at)
		return -ETIMEDOUT;
	else if (pt->pvt.cycle != obj->pvt.cycle) {
		/* The seqno has wrapped so complete this point */
		return 1;
	} else
		/* This shouldn't require locking as it is synchronous
		 * with the timeline signal function which is the only updater
		 * of these fields
		 */
		return (obj->pvt.value >= pt->pvt.value) ? 1 : 0;

	return 0;
}

static int i915_sync_pt_compare(struct sync_pt *a, struct sync_pt *b)
{
	struct i915_sync_pt *pt_a = container_of(a, struct i915_sync_pt, pt);
	struct i915_sync_pt *pt_b = container_of(b, struct i915_sync_pt, pt);

	if (pt_a->pvt.value == pt_b->pvt.value)
		return 0;
	else
		return (pt_a->pvt.value > pt_b->pvt.value) ? 1 : -1;
}

static int i915_sync_fill_driver_data(struct sync_pt *sync_pt,
				    void *data, int size)
{
	struct i915_sync_pt *pt = container_of(sync_pt,
					       struct i915_sync_pt, pt);

	if (size < sizeof(pt->pvt))
		return -ENOMEM;

	memcpy(data, &pt->pvt, sizeof(pt->pvt));

	return sizeof(pt->pvt);
}

static
struct sync_pt *i915_sync_pt_create(struct i915_sync_timeline *obj,
				    u32 value, u32 cycle, u64 ring_mask)
{
	struct i915_sync_pt *pt;

	if (!obj)
		return NULL;

	pt = (struct i915_sync_pt *)
		sync_pt_create(&obj->obj, sizeof(struct i915_sync_pt));

	if (pt) {
		pt->pvt.value = value;
		pt->pvt.cycle = cycle;
		pt->pvt.ring_mask = ring_mask;
	}

	return (struct sync_pt *)pt;
}

static struct sync_pt *i915_sync_pt_dup(struct sync_pt *sync_pt)
{
	struct i915_sync_pt *pt = container_of(sync_pt,
					       struct i915_sync_pt, pt);
	struct sync_pt *new_pt;
	struct i915_sync_timeline *obj =
		(struct i915_sync_timeline *)sync_pt->parent;

	new_pt = (struct sync_pt *)i915_sync_pt_create(obj, pt->pvt.value,
					pt->pvt.cycle, pt->pvt.ring_mask);
	return new_pt;
}

static void i915_sync_pt_free(struct sync_pt *sync_pt)
{
}

struct sync_timeline_ops i915_sync_timeline_ops = {
	.driver_name = "i915_sync",
	.dup = i915_sync_pt_dup,
	.has_signaled = i915_sync_pt_has_signaled,
	.compare = i915_sync_pt_compare,
	.fill_driver_data = i915_sync_fill_driver_data,
	.free_pt = i915_sync_pt_free,
};

int i915_sync_timeline_create(struct drm_device *dev,
			      const char *name,
			      struct intel_context *ctx,
			      struct intel_engine_cs *ring)
{
	struct i915_sync_timeline **timeline;
	struct i915_sync_timeline *local;

	if (i915.enable_execlists)
		timeline = &ctx->engine[ring->id].sync_timeline;
	else
		timeline = &ctx->legacy_hw_ctx.sync_timeline;

	if (*timeline)
		return 0;

	local = (struct i915_sync_timeline *)
			sync_timeline_create(&i915_sync_timeline_ops,
				     sizeof(struct i915_sync_timeline),
				     name);

	if (!local)
		return -EINVAL;

	local->pvt.killed_at = 0;
	local->pvt.next      = 1;

	/* Start the timeline from seqno 0 as this is a special value
	 * that is reserved for invalid sync points.
	 */
	local->pvt.value = 0;

	*timeline = local;

	return 0;
}

static uint32_t get_next_value(struct i915_sync_timeline *timeline)
{
	uint32_t value;

	value = timeline->pvt.next;

	/* Reserve zero for invalid */
	if (++timeline->pvt.next == 0 ) {
		timeline->pvt.next = 1;
		timeline->pvt.cycle++;
	}

	return value;
}

void i915_sync_timeline_destroy(struct intel_context *ctx,
				struct intel_engine_cs *ring)
{
	struct i915_sync_timeline **timeline;

	if (i915.enable_execlists)
		timeline = &ctx->engine[ring->id].sync_timeline;
	else
		timeline = &ctx->legacy_hw_ctx.sync_timeline;

	if (*timeline) {
		sync_timeline_destroy(&(*timeline)->obj);
		*timeline = NULL;
	}
}

void i915_sync_timeline_signal(struct i915_sync_timeline *obj, u32 value)
{
	/* Update the timeline to notify it that
	 * the monotonic counter has advanced.
	 */
	if (obj) {
		obj->pvt.value = value;
		sync_timeline_signal(&obj->obj);
	}
}

int i915_sync_create_fence(struct drm_i915_gem_request *req,
			   int *fd_out, u64 ring_mask)
{
	struct sync_pt *pt;
	int fd = -1, err;
	struct sync_fence *fence;
	struct i915_sync_timeline *timeline;

	if (req->sync_value) {
		DRM_DEBUG_DRIVER("Already got a sync point! [ring:%s, ctx:%p, seqno:%u]\n",
				 req->ring->name, req->ctx, i915_gem_request_get_seqno(req));
		*fd_out = -1;
		return -EINVAL;
	}

	if (i915.enable_execlists)
		timeline = req->ctx->engine[req->ring->id].sync_timeline;
	else
		timeline = req->ctx->legacy_hw_ctx.sync_timeline;

	if (!timeline) {
		DRM_DEBUG_DRIVER("Missing timeline! [ring:%s, ctx:%p, seqno:%u]\n",
				 req->ring->name, req->ctx, i915_gem_request_get_seqno(req));
		*fd_out = -1;
		return -ENODEV;
	}

	req->sync_value = get_next_value(timeline);
	pt = i915_sync_pt_create(timeline,
				 req->sync_value,
				 timeline->pvt.cycle,
				 ring_mask);
	if (!pt) {
		DRM_DEBUG_DRIVER("Failed to create sync point for ring:%s, ctx:%p, seqno:%u\n",
				 req->ring->name, req->ctx, i915_gem_request_get_seqno(req));
		*fd_out = -1;
		return -ENOMEM;
	}

	fd = get_unused_fd();
	if (fd < 0) {
		DRM_DEBUG_DRIVER("Unable to get file descriptor for fence\n");
		err = fd;
		goto err;
	}

	fence = sync_fence_create("I915", pt);
	if (fence) {
		sync_fence_install(fence, fd);
		*fd_out = fd;
		return 0;
	}

	DRM_DEBUG_DRIVER("Fence creation failed\n");
	err = -ENOMEM;
	put_unused_fd(fd);
err:
	sync_pt_free(pt);
	*fd_out = -1;
	return err;
}

void i915_sync_timeline_advance(struct intel_context *ctx,
				struct intel_engine_cs *ring,
				uint32_t value)
{
	struct i915_sync_timeline *timeline;

	if (!ctx)
		return;

	if (i915.enable_execlists)
		timeline = ctx->engine[ring->id].sync_timeline;
	else
		timeline = ctx->legacy_hw_ctx.sync_timeline;

	if (timeline)
		i915_sync_timeline_signal(timeline, value);
}

void i915_sync_hung_ring(struct intel_engine_cs *ring)
{
	struct i915_sync_timeline *timeline;
	struct drm_i915_gem_request *req;
	uint32_t active_seqno;

	/* Sample the active seqno to see if this request
	 * failed during a batch buffer execution.
	 */
	active_seqno = intel_read_status_page(ring,
					I915_GEM_ACTIVE_SEQNO_INDEX);

	if (!active_seqno)
		return;

	/* Clear it in the HWS to avoid seeing it more than once. */
	intel_write_status_page(ring, I915_GEM_ACTIVE_SEQNO_INDEX, 0);

	/* Map the seqno back to a request: */
	req = i915_gem_request_find_by_seqno(ring, active_seqno);
	if (!req) {
		DRM_DEBUG_DRIVER("Request not found for hung seqno!\n");
		return;
	}

	if (i915.enable_execlists)
		timeline = req->ctx->engine[req->ring->id].sync_timeline;
	else
		timeline = req->ctx->legacy_hw_ctx.sync_timeline;

	/* Signal the timeline. This will cause it to query the
	 * signaled state of any waiting sync points.
	 * If any match with ring->active_seqno then they
	 * will be marked with an error state.
	 */
	timeline->pvt.killed_at = req->sync_value;
	i915_sync_timeline_advance(req->ctx, req->ring, req->sync_value);
	timeline->pvt.killed_at = 0;
}
