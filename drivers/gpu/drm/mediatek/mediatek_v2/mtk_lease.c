// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/file.h>
#include <linux/uaccess.h>

#include <drm/drm_auth.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_lease.h>
#include <drm/drm_print.h>

#include "mtk_log.h"

static struct file *lease_file;
static DEFINE_MUTEX(lease_mutex);

int mtk_drm_set_lease_info_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	int *fd = data;

	if (PTR_ERR_OR_ZERO(fd)) {
		DDPPR_ERR("%s invalid input fd %x\n", __func__, fd);
		return -EINVAL;
	}

	mutex_lock(&lease_mutex);
	/* update leassor pid */
	lease_file = fget(*fd);
	if (PTR_ERR_OR_ZERO(lease_file)) {
		DDPPR_ERR("%s can't open fd %d\n", __func__, *fd);
		mutex_unlock(&lease_mutex);
		return -EBADF;
	}

	mutex_unlock(&lease_mutex);
	DDPMSG("update lease file\n");

	return 0;
}

int mtk_drm_get_lease_info_ioctl(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	int *ret_fd = data;
	int fd;

	if (PTR_ERR_OR_ZERO(ret_fd)) {
		DDPPR_ERR("%s invalid input fd %x\n", __func__, ret_fd);
		return -EINVAL;
	}

	mutex_lock(&lease_mutex);
	if (!lease_file) {
		DDPPR_ERR("%s uninitialized lease info\n", __func__);
		mutex_unlock(&lease_mutex);
		return -ENOENT;
	}

	if (WARN_ON(file_count(lease_file) <= 1)) {
		DDPPR_ERR("%s improper lessee file ref count, abort\n", __func__);
		mutex_unlock(&lease_mutex);
		return -ENOSPC;
	}

	/* get lease fd */
	fd = get_unused_fd_flags(O_CLOEXEC);
	fd_install(fd, lease_file);
	if (fd <= 0) {
		DDPPR_ERR("%s can't get lease fd %d\n", __func__, fd);
		mutex_unlock(&lease_mutex);
		return -EBADF;
	}
	get_file(lease_file);
	*ret_fd = fd;

	mutex_unlock(&lease_mutex);
	DDPMSG("get lease fd %d\n", fd);

	return 0;
}
