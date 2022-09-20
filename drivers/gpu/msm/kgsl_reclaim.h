/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */
#ifndef __KGSL_RECLAIM_H
#define __KGSL_RECLAIM_H


#include "kgsl_device.h"

#ifdef CONFIG_QCOM_KGSL_PROCESS_RECLAIM

/* Set if all the memdescs of this process are pinned */
#define KGSL_PROC_PINNED_STATE 0
/* Process foreground/background state. Set if process is in foreground */
#define KGSL_PROC_STATE 1

int kgsl_reclaim_start(void);
int kgsl_reclaim_init(void);
void kgsl_reclaim_close(void);
int kgsl_reclaim_to_pinned_state(struct kgsl_process_private *priv);
void kgsl_reclaim_proc_sysfs_init(struct kgsl_process_private *process);
void kgsl_reclaim_proc_private_init(struct kgsl_process_private *process);
ssize_t kgsl_proc_max_reclaim_limit_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
ssize_t kgsl_proc_max_reclaim_limit_show(struct device *dev,
		struct device_attribute *attr, char *buf);
ssize_t kgsl_nr_to_scan_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
ssize_t kgsl_nr_to_scan_show(struct device *dev,
		struct device_attribute *attr, char *buf);
#else
static inline int kgsl_reclaim_start(void)
{
	return 0;
}

static inline int kgsl_reclaim_init(void)
{
	return 0;
}

static inline void kgsl_reclaim_close(void) { }

static inline int kgsl_reclaim_to_pinned_state(
		struct kgsl_process_private *priv)
{
	return 0;
}

static inline void kgsl_reclaim_proc_sysfs_init
		(struct kgsl_process_private *process) { }

static inline void kgsl_reclaim_proc_private_init
		(struct kgsl_process_private *process) { }

#endif
#endif /* __KGSL_RECLAIM_H */
