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
	struct drm_i915_private *dev_priv = NULL;
	struct i915_sync_pt *pt = container_of(sync_pt,
					       struct i915_sync_pt, pt);
	struct i915_sync_timeline *obj =
		(struct i915_sync_timeline *)sync_pt->parent;

	dev_priv = (struct drm_i915_private *)obj->pvt.dev->dev_private;

	/* On ring timeout fail the status of pending sync_pts.
	 * This callback is synchronous with the thread which calls
	 * sync_timeline_signal. If this has been signaled due to
	 * an error then ring->active_seqno will be set to the
	 * failing seqno (otherwise it will be 0). Compare the
	 * sync point seqno with the failing seqno to detect errors.
	 */
	if (!obj->pvt.ring)
		return -ENODEV;
	else if (pt->pvt.value == obj->pvt.ring->active_seqno)
		return -ETIMEDOUT;
	else if (pt->pvt.value == 0)
		/* It hasn't yet been assigned a sequence number
		 * which means it can't have finished.
		 */
		return 0;
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
						u32 value, u32 cycle)
{
	struct i915_sync_pt *pt;
	struct intel_engine_cs *ring;

	if (!obj)
		return NULL;

	ring = obj->pvt.ring;

	/* Enable user interrupts for the lifetime of the sync point. */
	if (!ring->irq_get(ring))
		return NULL;

	pt = (struct i915_sync_pt *)
		sync_pt_create(&obj->obj, sizeof(struct i915_sync_pt));

	if (pt) {
		pt->pvt.value = value;
		pt->pvt.cycle = cycle;
	} else
		ring->irq_put(ring);

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
								pt->pvt.cycle);
	return new_pt;
}

static void i915_sync_pt_free(struct sync_pt *sync_pt)
{
	struct i915_sync_timeline *obj =
		(struct i915_sync_timeline *)sync_pt->parent;
	struct intel_engine_cs *ring = obj->pvt.ring;

	/* User interrupts can be disabled when sync point is freed. */
	ring->irq_put(ring);
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
				struct intel_engine_cs *ring)
{
	struct i915_sync_timeline *obj = (struct i915_sync_timeline *)
		sync_timeline_create(&i915_sync_timeline_ops,
				     sizeof(struct i915_sync_timeline),
				     name);

	if (!obj)
		return -EINVAL;

	obj->pvt.dev = dev;
	obj->pvt.ring = ring;

	/* Start the timeline from seqno 0 as this is a special value
	 * that is never assigned to a batch buffer.
	 */
	obj->pvt.value = 0;

	ring->timeline = obj;

	return 0;
}

void i915_sync_timeline_destroy(struct intel_engine_cs *ring)
{
	if (ring->timeline) {
		sync_timeline_destroy(&ring->timeline->obj);
		ring->timeline = NULL;
	}
}

void i915_sync_timeline_signal(struct i915_sync_timeline *obj, u32 value)
{
	/* Update the timeline to notify it that
	 * the monotonic seqno counter has advanced.
	 */
	if (obj) {
		obj->pvt.value = value;
		sync_timeline_signal(&obj->obj);
	}
}

void i915_sync_reset_timelines(struct drm_i915_private *dev_priv)
{
	unsigned int i;

	/* Reset all ring timelines to zero. */
	for (i = 0; i < I915_NUM_RINGS; i++) {
		struct intel_engine_cs *sync_ring = &dev_priv->ring[i];

		if (sync_ring && sync_ring->timeline)
			sync_ring->timeline->pvt.cycle++;

		i915_sync_timeline_signal(sync_ring->timeline, 0);
	}
}

static int i915_write_active_seqno(struct intel_engine_cs *ring, u32 seqno)
{
	int ret;

	ret = intel_ring_begin(ring, 4);
	if (ret)
		return ret;

	intel_ring_emit(ring, MI_STORE_DWORD_INDEX);
	intel_ring_emit(ring, I915_GEM_ACTIVE_SEQNO_INDEX <<
			MI_STORE_DWORD_INDEX_SHIFT);
	intel_ring_emit(ring, seqno);
	intel_ring_emit(ring, MI_NOOP);
	intel_ring_advance(ring);

	return 0;
}

void *i915_sync_prepare_request(struct drm_i915_gem_execbuffer2 *args,
				struct intel_engine_cs *ring, u32 seqno)
{
	int ret;
	struct sync_pt *pt;

	BUG_ON(!ring->timeline);

	/* Write the current seqno to the HWS page so that
	 * we can identify the cause of any hangs.
	 */
	ret = i915_write_active_seqno(ring, seqno);
	if (ret) {
		DRM_DEBUG_DRIVER("Failed to store seqno for %d (%d)\n",
				 ring->id, ret);
		return ERR_PTR(ret);
	}

	/* Fence was not requested, nothing more to do. */
	if (!(args->flags & I915_EXEC_REQUEST_FENCE))
		return NULL;

	/* Caller has requested a sync fence.
	 * User interrupts will be enabled to make sure that
	 * the timeline is signalled on completion.
	 */
	pt = i915_sync_pt_create(ring->timeline, seqno,
				ring->timeline->pvt.cycle);
	if (!pt)
		DRM_DEBUG_DRIVER("Failed to create sync point for %d/%u\n",
					ring->id, seqno);

	return (void *)pt;
}

static int gen8_write_active_seqno(struct intel_ringbuffer *ringbuf, u32 seqno)
{
	int ret;
	struct intel_engine_cs *ring = ringbuf->ring;

	ret = intel_logical_ring_begin(ringbuf, 4);
	if (ret)
		return ret;

	intel_logical_ring_emit(ringbuf, MI_STORE_DWORD_INDEX);
	intel_logical_ring_emit(ringbuf,
				(ring->status_page.gfx_addr +
				 (I915_GEM_ACTIVE_SEQNO_INDEX <<
				  MI_STORE_DWORD_INDEX_SHIFT)));
	intel_logical_ring_emit(ringbuf, seqno);
	intel_logical_ring_emit(ringbuf, MI_NOOP);
	intel_logical_ring_advance(ringbuf);

	return 0;
}

void *gen8_sync_prepare_request(struct drm_i915_gem_execbuffer2 *args,
				struct intel_ringbuffer *ringbuf,
				u32 seqno)
{
	int ret;
	struct sync_pt *pt;
	struct intel_engine_cs *ring = ringbuf->ring;

	BUG_ON(!ring->timeline);

	/* Write the current seqno to the HWS page so that
	 * we can identify the cause of any hangs.
	 */
	ret = gen8_write_active_seqno(ringbuf, seqno);
	if (ret) {
		DRM_DEBUG_DRIVER("Failed to store seqno for %d (%d)\n",
				 ring->id, ret);
		return ERR_PTR(ret);
	}

	/* Fence was not requested, nothing more to do. */
	if (!(args->flags & I915_EXEC_REQUEST_FENCE))
		return NULL;

	/* Caller has requested a sync fence.
	 * User interrupts will be enabled to make sure that
	 * the timeline is signalled on completion.
	 */
	pt = i915_sync_pt_create(ring->timeline, seqno,
				ring->timeline->pvt.cycle);
	if (!pt)
		DRM_DEBUG_DRIVER("Failed to create sync point for %d/%u\n",
					ring->id, seqno);

	return (void *)pt;
}

int i915_sync_finish_request(void *handle,
				struct drm_i915_gem_execbuffer2 *args,
				struct intel_engine_cs *ring)
{
	struct sync_pt *pt = (struct sync_pt *)handle;
	int err;
	int fd = -1;
	struct sync_fence *fence;

	/* Clear the active seqno. */
	if (i915_write_active_seqno(ring, 0))
		DRM_DEBUG_DRIVER("Failed to clear seqno for %d\n", ring->id);

	/* Fence was not requested, nothing more to do. */
	if (!pt)
		return 0;

	fd = get_unused_fd();
	if (fd < 0) {
		DRM_DEBUG_DRIVER("Unable to get file descriptor for fence\n");
		err = fd;
		goto err;
	}

	fence = sync_fence_create("I915", pt);
	if (!fence) {
		DRM_DEBUG_DRIVER("Fence creation failed\n");
		err = -ENOMEM;
		goto err_fd;
	}

	sync_fence_install(fence, fd);

	/* Return the fence through the rsvd2 field */
	args->rsvd2 = (__u64)fd;

	return 0;

err_fd:
	put_unused_fd(fd);
	fd = err;
err:
	args->rsvd2 = (__u64)fd;

	return err;
}

int gen8_sync_finish_request(void *handle,
			     struct drm_i915_gem_execbuffer2 *args,
			     struct intel_ringbuffer *ringbuf)
{
	struct sync_pt *pt = (struct sync_pt *)handle;
	int err;
	int fd = -1;
	struct sync_fence *fence;
	struct intel_engine_cs *ring = ringbuf->ring;

	/* Clear the active seqno. */
	if (gen8_write_active_seqno(ringbuf, 0))
		DRM_DEBUG_DRIVER("Failed to clear seqno for %d\n", ring->id);

	/* Fence was not requested, nothing more to do. */
	if (!pt)
		return 0;

	fd = get_unused_fd();
	if (fd < 0) {
		DRM_DEBUG_DRIVER("Unable to get file descriptor for fence\n");
		err = fd;
		goto err;
	}

	fence = sync_fence_create("I915", pt);
	if (!fence) {
		DRM_DEBUG_DRIVER("Fence creation failed\n");
		err = -ENOMEM;
		goto err_fd;
	}

	sync_fence_install(fence, fd);

	/* Return the fence through the rsvd2 field */
	args->rsvd2 = (__u64)fd;

	return 0;

err_fd:
	put_unused_fd(fd);
	fd = err;
err:
	args->rsvd2 = (__u64)fd;

	return err;
}

void i915_sync_cancel_request(void *handle,
				struct drm_i915_gem_execbuffer2 *args,
				struct intel_engine_cs *ring)
{
	struct sync_pt *pt = (struct sync_pt *)handle;

	if (pt && !IS_ERR(pt))
		sync_pt_free(pt);
}

void i915_sync_timeline_advance(struct intel_engine_cs *ring)
{
	if (ring->timeline)
		i915_sync_timeline_signal(ring->timeline,
			ring->get_seqno(ring, false));
}

void i915_sync_hung_ring(struct intel_engine_cs *ring)
{
	/* Sample the active seqno to see if this request
	 * failed during a batch buffer execution.
	 */
	ring->active_seqno = intel_read_status_page(ring,
				I915_GEM_ACTIVE_SEQNO_INDEX);

	if (ring->active_seqno) {
		/* Clear it in the HWS to avoid seeing it more than once. */
		intel_write_status_page(ring, I915_GEM_ACTIVE_SEQNO_INDEX, 0);

		/* Signal the timeline. This will cause it to query the
		 * signaled state of any waiting sync points.
		 * If any match with ring->active_seqno then they
		 * will be marked with an error state.
		 */
		i915_sync_timeline_signal(ring->timeline, ring->active_seqno);

		/* Clear the active_seqno so it isn't seen twice. */
		ring->active_seqno = 0;
	}
}
