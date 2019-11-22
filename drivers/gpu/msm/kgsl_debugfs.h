/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2002,2008-2011,2013,2015,2017,2019, The Linux Foundation. All rights reserved.
 */
#ifndef _KGSL_DEBUGFS_H
#define _KGSL_DEBUGFS_H

struct kgsl_device;
struct kgsl_process_private;

#ifdef CONFIG_DEBUG_FS
void kgsl_core_debugfs_init(void);
void kgsl_core_debugfs_close(void);

void kgsl_device_debugfs_init(struct kgsl_device *device);
void kgsl_device_debugfs_close(struct kgsl_device *device);

extern struct dentry *kgsl_debugfs_dir;
static inline struct dentry *kgsl_get_debugfs_dir(void)
{
	return kgsl_debugfs_dir;
}

void kgsl_process_init_debugfs(struct kgsl_process_private *priv);
#else
static inline void kgsl_core_debugfs_init(void) { }
static inline void kgsl_device_debugfs_init(struct kgsl_device *device) { }
static inline void kgsl_device_debugfs_close(struct kgsl_device *device) { }
static inline void kgsl_core_debugfs_close(void) { }
static inline struct dentry *kgsl_get_debugfs_dir(void) { return NULL; }
static inline void kgsl_process_init_debugfs(struct kgsl_process_private *priv)
{
}
#endif

#endif
