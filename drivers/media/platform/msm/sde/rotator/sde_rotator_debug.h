/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __SDE_ROTATOR_DEBUG_H__
#define __SDE_ROTATOR_DEBUG_H__

#include <linux/types.h>
#include <linux/dcache.h>

struct sde_rotator_device;

#if defined(CONFIG_DEBUG_FS)
struct dentry *sde_rotator_create_debugfs(
		struct sde_rotator_device *rot_dev);

void sde_rotator_destroy_debugfs(struct dentry *debugfs);
#else
static inline
struct dentry *sde_rotator_create_debugfs(
		struct sde_rotator_device *rot_dev)
{
	return NULL;
}

static inline
void sde_rotator_destroy_debugfs(struct dentry *debugfs)
{
}
#endif
#endif /* __SDE_ROTATOR_DEBUG_H__ */
