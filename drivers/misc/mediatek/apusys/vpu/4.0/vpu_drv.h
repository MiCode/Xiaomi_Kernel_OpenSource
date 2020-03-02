/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __VPU_DRV_H__
#define __VPU_DRV_H__

#include <linux/types.h>
#include <linux/kref.h>
 // #include <vpu_dvfs.h>
#include "vpu_ioctl.h"

/* kernel internal struct */
struct __vpu_algo {
	struct vpu_algo a;
	bool info_valid;        /* is algo info valid */
	struct kref ref;        /* reference count */
	struct list_head list;  /* link to device algo list */
};
#endif
