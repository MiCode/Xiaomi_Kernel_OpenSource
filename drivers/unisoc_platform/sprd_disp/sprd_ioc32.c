// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <drm/drm_atomic_helper.h>
#include <drm/drm_file.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_print.h>
//#include <uapi/drm/sprd_drm_gsp.h>
#include "sprd_drm_gsp.h"

#define DRM_IOCTL32_DEF(n, f)[DRM_##n] = {.fn = f, .name = #n}

struct drm_gsp_cfg_user32 {
	u8 gsp_id;
	bool async;
	u32 size;
	u32 num;
	bool split;
	u32 config;
};

struct drm_gsp_capability32 {
	u8 gsp_id;
	u32 size;
	u32 cap;
};

static int sprd_gsp_trigger_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct drm_gsp_cfg_user32 getparam32;
	struct drm_gsp_cfg_user getparam;

	if (copy_from_user(&getparam32, (void __user *)arg, sizeof(getparam32)))
		return -EFAULT;

	getparam.gsp_id = getparam32.gsp_id;
	getparam.async = getparam32.async;
	getparam.size = getparam32.size;
	getparam.num = getparam32.num;
	getparam.split = getparam32.split;
	getparam.config = compat_ptr(getparam32.config);

	return drm_ioctl_kernel(file, sprd_gsp_trigger_ioctl, &getparam, 0);
}

static int sprd_gsp_get_capability_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct drm_gsp_capability32 getparam32;
	struct drm_gsp_capability getparam;

	if (copy_from_user(&getparam32, (void __user *)arg, sizeof(getparam32)))
		return -EFAULT;

	getparam.gsp_id = getparam32.gsp_id;
	getparam.size = getparam32.size;
	getparam.cap = compat_ptr(getparam32.cap);
	return drm_ioctl_kernel(file, sprd_gsp_get_capability_ioctl, &getparam, 0);
}

static struct {
	drm_ioctl_compat_t *fn;
	char *name;
} sprd_compat_ioctls[] = {
	DRM_IOCTL32_DEF(SPRD_GSP_GET_CAPABILITY, sprd_gsp_get_capability_compat_ioctl),
	DRM_IOCTL32_DEF(SPRD_GSP_TRIGGER, sprd_gsp_trigger_compat_ioctl),
};

long sprd_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	unsigned int nr = DRM_IOCTL_NR(cmd);
	struct drm_file *file_priv = filp->private_data;
	drm_ioctl_compat_t *fn = NULL;
	int ret;

	if (nr < DRM_COMMAND_BASE)
		return drm_compat_ioctl(filp, cmd, arg);

	if (nr >= DRM_COMMAND_BASE + ARRAY_SIZE(sprd_compat_ioctls))
		return drm_ioctl(filp, cmd, arg);

	fn = sprd_compat_ioctls[nr - DRM_COMMAND_BASE].fn;
	if (!fn)
		return drm_ioctl(filp, cmd, arg);

	DRM_DEBUG("pid=%d, dev=0x%lx, auth=%d, %s\n", task_pid_nr(current),
			(long)old_encode_dev(file_priv->minor->kdev->devt),
			file_priv->authenticated,
			sprd_compat_ioctls[nr - DRM_COMMAND_BASE].name);
	ret = (*fn) (filp, cmd, arg);
	if (ret)
		DRM_DEBUG("ret = %d\n", ret);
	return ret;
}
