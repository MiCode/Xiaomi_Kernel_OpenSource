/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _MSM_SYSSTATS_H_
#define _MSM_SYSSTATS_H_

#include <uapi/linux/msm_sysstats.h>

struct memcheck_gpumem_pinfo {
	struct seq_file *seq;
	int pid;
	char comm[TASK_COMM_LEN];
	void *private_base;
	void *pagetable;
	unsigned long kgsl_mem;
	unsigned long kgsl_dmabuf;
	bool print_enforced;
};

#if IS_ENABLED(CONFIG_MSM_SYSSTATS)
extern void sysstats_register_kgsl_stats_cb(u64 (*cb)(pid_t pid));
extern void sysstats_unregister_kgsl_stats_cb(void);

extern void sysstats_register_kgsl_memstat_cb(size_t (*cb)(const char *name));
extern size_t (*memcheck_kgsl_memstat)(const char *name);
extern void sysstats_register_iterate_kgsl_process_privates(int (*cb)(int (*f)(void *), void *data));
extern int (*memcheck_walk_process_privates)(int (*f)(void *), void *data);
#else
static inline void sysstats_register_kgsl_stats_cb(u64 (*cb)(pid_t pid))
{
}
static inline void sysstats_unregister_kgsl_stats_cb(void)
{
}
void sysstats_register_kgsl_memstat_cb(size_t (*cb)(const char *name))
{
}
void sysstats_register_iterate_kgsl_process_privates(int (*cb)(void *))
{
}
#endif
#endif /* _MSM_SYSSTATS_H_ */

