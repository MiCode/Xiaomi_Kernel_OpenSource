/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */
#ifndef _KGSL_SYSFS_H_
#define _KGSL_SYSFS_H_

struct kgsl_device;

/**
 * struct kgsl_gpu_sysfs_attr - Attribute definition for sysfs objects in the
 * /sys/kernel/gpu kobject
 */
struct kgsl_gpu_sysfs_attr {
	/** @attr: Attribute for the sysfs node */
	struct attribute attr;
	/** @show: Show function for the node */
	ssize_t (*show)(struct kgsl_device *device, char *buf);
	/** @store: Store function for the node */
	ssize_t (*store)(struct kgsl_device *device, const char *buf,
			size_t count);
};

#define GPU_SYSFS_ATTR(_name, _mode, _show, _store)		\
const struct kgsl_gpu_sysfs_attr gpu_sysfs_attr_##_name = {		\
	.attr = { .name = __stringify(_name), .mode = _mode },	\
	.show = _show,						\
	.store = _store,					\
}

#endif
