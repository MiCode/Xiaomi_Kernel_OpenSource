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
#include "i915_drv.h"
#include "i915_ext_ioctl.h"
#include "i915_gem_userdata.h"

const struct drm_ioctl_desc i915_ext_ioctls[];
int i915_max_ext_ioctl;

int i915_extended_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file_priv)
{
	struct i915_ext_ioctl_data *args = data;
	const struct drm_ioctl_desc *ioctl = NULL;
	drm_ioctl_t *func;
	unsigned int cmd = args->sub_cmd;
	void __user *arg = to_user_ptr(args->args_ptr);
	unsigned int nr = DRM_IOCTL_NR(cmd);
	int retcode = -EINVAL;
	char stack_kdata[128];
	char *kdata = NULL;
	unsigned int usize, asize;

	DRM_DEBUG("sub_nr=%u, table=%u, available ext_ioctls=%d\n",
			nr,
			(unsigned int)args->table,
			i915_max_ext_ioctl);

	/* table is reserved for future expansion, and must be 0 */
	if (args->table > 0) {
		DRM_ERROR("table range error\n");
		goto err_i1;
	} else if (nr >= i915_max_ext_ioctl) {
		DRM_ERROR("sub_nr range error\n");
		goto err_i1;
	} else {
		u32 drv_size;
		DRM_DEBUG("sub_nr is in range\n");

		ioctl = &i915_ext_ioctls[nr];
		drv_size = _IOC_SIZE(ioctl->cmd_drv);
		usize = asize = _IOC_SIZE(cmd);
		if (drv_size > asize)
			asize = drv_size;

		cmd = ioctl->cmd_drv;
	}

	DRM_DEBUG("pid=%d, dev=0x%lx, auth=%d, %s\n",
		task_pid_nr(current),
		(long)old_encode_dev(file_priv->minor->kdev->devt),
		file_priv->authenticated, ioctl->name);

	/* Do not trust userspace, use our own definition */
	func = ioctl->func;

	if (unlikely(!func)) {
		DRM_DEBUG("no function\n");
		retcode = -EINVAL;
		goto err_i1;
	}

	retcode = drm_ioctl_permit(ioctl->flags, file_priv);
	if (unlikely(retcode))
		goto err_i1;

	if (cmd & (IOC_IN | IOC_OUT)) {
		if (asize <= sizeof(stack_kdata)) {
			kdata = stack_kdata;
		} else {
			kdata = kmalloc(asize, GFP_KERNEL);
			if (!kdata) {
				DRM_ERROR("nomem\n");
				retcode = -ENOMEM;
				goto err_i1;
			}
		}

		if (cmd & IOC_IN) {
			if (copy_from_user(kdata, arg, usize) != 0) {
				DRM_ERROR("copy in failed\n");
				retcode = -EFAULT;
				goto err_i1;
			}
		} else {
			memset(kdata, 0, usize);
			if (asize > usize)
				memset(kdata + usize, 0, asize - usize);
		}
	}

	if (ioctl->flags & DRM_UNLOCKED) {
		retcode = func(dev, kdata, file_priv);
	} else {
		mutex_lock(&drm_global_mutex);
		retcode = func(dev, kdata, file_priv);
		mutex_unlock(&drm_global_mutex);
	}
	DRM_DEBUG("sub-func returned %d\n", retcode);

	if (cmd & IOC_OUT) {
		if (copy_to_user(arg, kdata, usize) != 0) {
			DRM_ERROR("copy out failed\n");
			retcode = -EFAULT;
		} else
			DRM_DEBUG("copy-out succeeded\n");
	}


err_i1:
	if (!ioctl) {
		DRM_ERROR("%s: pid=%d, dev=0x%lx, auth=%d, cmd=0x%x, nr=0x%x\n",
			  "invalid ioctl",
			  task_pid_nr(current),
			  (long)old_encode_dev(file_priv->minor->kdev->devt),
			  file_priv->authenticated, cmd, nr);
	}

	if (kdata != stack_kdata)
		kfree(kdata);
	if (retcode)
		DRM_ERROR("ret = %d\n", retcode);
	return retcode;
}

/*
 * ----------------------------------------------------------------------------
 * Extended ioctl interface table
 * Format is identical to the standard ioctls
 * ----------------------------------------------------------------------------
 */

const struct drm_ioctl_desc i915_ext_ioctls[] = {
	DRM_IOCTL_DEF_DRV(I915_EXT_USERDATA, i915_gem_userdata_ioctl,
			  DRM_UNLOCKED|DRM_CONTROL_ALLOW|DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(I915_GEM_FALLOCATE, i915_gem_fallocate_ioctl,
			  DRM_UNLOCKED|DRM_CONTROL_ALLOW|DRM_RENDER_ALLOW),
};

int i915_max_ext_ioctl = ARRAY_SIZE(i915_ext_ioctls);
