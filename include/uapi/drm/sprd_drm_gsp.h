/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _SPRD_DRM_GSP_H_
#define _SPRD_DRM_GSP_H_

#include <drm/drm.h>
#include "gsp_cfg.h"

#define DRM_SPRD_GSP_GET_CAPABILITY	0
#define DRM_SPRD_GSP_TRIGGER	1

struct drm_gsp_cfg_user {
	__u8 gsp_id;
	bool async;
	__u32 size;
	__u32 num;
	bool split;
	char version[32];
	void *config;
};

struct drm_gsp_capability {
	__u8 gsp_id;
	__u32 size;
	char version[32];
	void *cap;
};

#define DRM_IOCTL_SPRD_GSP_GET_CAPABILITY \
	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_SPRD_GSP_GET_CAPABILITY, \
		struct drm_gsp_capability)

#define DRM_IOCTL_SPRD_GSP_TRIGGER	\
	DRM_IOWR(DRM_COMMAND_BASE + DRM_SPRD_GSP_TRIGGER, \
		struct drm_gsp_cfg_user)

#endif	/* SPRD_DRM_GSP_H_ */
