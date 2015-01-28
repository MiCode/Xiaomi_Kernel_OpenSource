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
 *    Jon Bloomfield <jon.bloomfield@intel.com>
 *
 */
#include <drm/drmP.h>
#include <drm/i915_drm.h>
#include <linux/spinlock.h>
#include "i915_drv.h"

static int
i915_gem_userdata(struct drm_device *dev,
		  struct drm_file *file,
		  u16 op, u16 flags,
		  u32 handle, u32 offset, u32 bytes,
		  void __user *data,
		  u16 *actual_bytes)
{
#define SIZE_LIMIT 4096
	struct drm_i915_gem_object *obj;
	int set = 0;
	int ret = -EINVAL;
	u8 *stored_data = NULL;
	struct i915_gem_userdata *userdata_blk;

	DRM_DEBUG("op=%u, flags=0x%x, handle=%0lx, offset=%lu, bytes=%lu\n",
		  (unsigned)op, (unsigned)flags,
		  (unsigned long)handle,
		  (unsigned long)offset, (unsigned long)bytes);


	WARN_ON(!actual_bytes);
	*actual_bytes = 0;

	obj = to_intel_bo(drm_gem_object_lookup(dev, file, handle));
	if (&obj->base == NULL) {
		DRM_ERROR("Bad object: handle=%lx\n",
			  (unsigned long)handle);
		ret = -ENOENT;
		goto unref;
	}

	/* All operations return the currently allocated length */
	userdata_blk = obj->userdata_blk;
	if (userdata_blk)
		*actual_bytes = userdata_blk->length;

	if (bytes > SIZE_LIMIT) {
		DRM_ERROR("Bad size (%lu): data is limitted to %u bytes\n",
			  (unsigned long)bytes, SIZE_LIMIT);
		ret = -E2BIG;
		goto unref;
	}

	switch (op) {
	case I915_USERDATA_CREATE_OP:
		/* Cannot re-create if already created */
		if (userdata_blk) {
			DRM_ERROR("userdata already created: size %u\n",
				  (unsigned)userdata_blk->length);
			ret = -EEXIST;
			goto unref;
		}

		/* offset is not used for create and must be 0 */
		if (offset != 0) {
			DRM_ERROR("invalid offset: Must be 0 for CREATE\n");
			goto unref;
		}

		if (!bytes) {
			DRM_ERROR("invalid size (0). Must be > 0\n");
			goto unref;
		}

		if ((flags != 0) && (flags != I915_USERDATA_READONLY)) {
			DRM_ERROR("invalid flags: %x\n", (unsigned)flags);
			goto unref;
		}

		userdata_blk =
			kmalloc(sizeof(userdata_blk[0])+bytes, GFP_KERNEL);
		if (!userdata_blk) {
			DRM_ERROR("Failed to alloc userdata len=%lu\n",
				  (unsigned long)bytes);
			ret = -ENOMEM;
			goto unref;
		}

		userdata_blk->length = (u16)bytes;
		userdata_blk->flags  = flags;
		userdata_blk->lock = __RW_LOCK_UNLOCKED(userdata_blk->lock);

		if (data) {
			ret = copy_from_user(userdata_blk->data, data, bytes);
			if (ret != 0) {
				kfree(userdata_blk);
				ret = -EFAULT;
				goto unref;
			}
		} else {
			memset(userdata_blk->data, 0, bytes);
		}

		/*
		 * We're about to link the new area into the object.
		 * The pointer assignment below will be atomic, but we must
		 * ensure that no readers can observe the pointer before they
		 * can observe the above initialization
		 */
		wmb();

		/* Make the data visible to readers */
		obj->userdata_blk = userdata_blk;
		ret = 0;
		break;

	case I915_USERDATA_SET_OP:
		set = 1;
	case I915_USERDATA_GET_OP:
		if (!userdata_blk) {
			DRM_ERROR("Can't set/get: userdata not created\n");
			goto unref;
		}

		if (flags != 0) {
			DRM_ERROR("Only create can accept flags\n");
			goto unref;
		}

		if (set && (userdata_blk->flags & I915_USERDATA_READONLY)) {
			DRM_ERROR("Can't set: userdata is read-only\n");
			ret = -EPERM;
			goto unref;
		}

		if (offset > SIZE_LIMIT) {
			DRM_ERROR("Bad offset (%lu): max is %u bytes\n",
				  (unsigned long)bytes, SIZE_LIMIT);
			ret = -E2BIG;
			goto unref;
		}

		if ((offset + bytes) > userdata_blk->length) {
			DRM_ERROR("Overflow: Allocated userdata size: %u,"
				  "offset: %u, bytes: %u\n",
				  (unsigned)userdata_blk->length,
				  (unsigned)offset, (unsigned)bytes);
			ret = -E2BIG;
			goto unref;
		}

		stored_data = userdata_blk->data + offset;
		if (set) {
			write_lock(&userdata_blk->lock);
			ret = copy_from_user(stored_data, data, bytes);
			write_unlock(&userdata_blk->lock);
		} else {
			read_lock(&userdata_blk->lock);
			ret = copy_to_user(data, stored_data, bytes);
			read_unlock(&userdata_blk->lock);
		}

		if (ret != 0)
			ret = -EFAULT;
		break;

	default:
		DRM_ERROR("invalid op: %u\n", (unsigned)(op));
	}

unref:
	drm_gem_object_unreference(&obj->base);

	return ret;
#undef SIZE_LIMIT
}

int
i915_gem_userdata_ioctl(struct drm_device *dev, void *data,
			struct drm_file *file)
{
	struct drm_i915_gem_userdata_blk *args = data;
	u16 actual_bytes = 0;

	int ret = i915_gem_userdata(dev, file,
				    args->op,
				    args->flags,
				    args->handle,
				    args->offset,
				    args->bytes,
				    to_user_ptr(args->data_ptr),
				    &actual_bytes);

	/* Original size is returned back in the bytes arg */
	args->bytes = actual_bytes;
	return ret;
}
