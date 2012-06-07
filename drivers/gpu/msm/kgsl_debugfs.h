/* Copyright (c) 2002,2008-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _KGSL_DEBUGFS_H
#define _KGSL_DEBUGFS_H

struct kgsl_device;

#ifdef CONFIG_DEBUG_FS
void kgsl_core_debugfs_init(void);
void kgsl_core_debugfs_close(void);

void kgsl_device_debugfs_init(struct kgsl_device *device);

extern struct dentry *kgsl_debugfs_dir;
static inline struct dentry *kgsl_get_debugfs_dir(void)
{
	return kgsl_debugfs_dir;
}

#else
static inline void kgsl_core_debugfs_init(void) { }
static inline void kgsl_device_debugfs_init(struct kgsl_device *device) { }
static inline void kgsl_core_debugfs_close(void) { }
static inline struct dentry *kgsl_get_debugfs_dir(void) { return NULL; }

#endif

#endif
