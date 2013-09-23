/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
#ifndef __ADRENO_PROFILE_H
#define __ADRENO_PROFILE_H
#include <linux/seq_file.h>

/**
 * struct adreno_profile_assigns_list: linked list for assigned perf counters
 * @list: linkage  for nodes in list
 * @name: group name  or GPU name name
 * @groupid: group id
 * @countable: countable assigned to perfcounter
 * @offset: perfcounter register address offset
 */
struct adreno_profile_assigns_list {
	struct list_head list;
	char name[25];
	unsigned int groupid;
	unsigned int countable;
	unsigned int offset;   /* LO offset,  HI offset is +1 */
};

struct adreno_profile {
	struct list_head assignments_list; /* list of all assignments */
	unsigned int assignment_count;  /* Number of assigned counters */
	unsigned int *log_buffer;
	unsigned int *log_head;
	unsigned int *log_tail;
	bool enabled;
	/* counter, pre_ib, and post_ib held in one large circular buffer
	 * shared between kgsl and GPU
	 * counter entry 0
	 * pre_ib entry 0
	 * post_ib entry 0
	 * ...
	 * counter entry N
	 * pre_ib entry N
	 * post_ib entry N
	 */
	struct kgsl_memdesc shared_buffer;
	unsigned int shared_head;
	unsigned int shared_tail;
	unsigned int shared_size;
};

#define ADRENO_PROFILE_SHARED_BUF_SIZE_DWORDS (48 * 4096 / sizeof(uint))
/* sized @ 48 pages should allow for over 50 outstanding IBs minimum, 1755 max*/

#define ADRENO_PROFILE_LOG_BUF_SIZE  (1024 * 920)
/* sized for 1024 entries of fully assigned 45 cnters in log buffer, 230 pages*/
#define ADRENO_PROFILE_LOG_BUF_SIZE_DWORDS  (ADRENO_PROFILE_LOG_BUF_SIZE / \
						sizeof(unsigned int))

#ifdef CONFIG_DEBUG_FS
void adreno_profile_init(struct kgsl_device *device);
void adreno_profile_close(struct kgsl_device *device);
int adreno_profile_process_results(struct kgsl_device *device);
void adreno_profile_preib_processing(struct kgsl_device *device,
		unsigned int context_id, unsigned int *cmd_flags,
		unsigned int **rbptr, unsigned int *cmds_gpu);
void adreno_profile_postib_processing(struct kgsl_device *device,
		unsigned int *cmd_flags, unsigned int **rbptr,
		unsigned int *cmds_gpu);
#else
static inline void adreno_profile_init(struct kgsl_device *device) { }
static inline void adreno_profile_close(struct kgsl_device *device) { }
static inline int adreno_profile_process_results(struct kgsl_device *device)
{
	return 0;
}

static inline void adreno_profile_preib_processing(struct kgsl_device *device,
		unsigned int context_id, unsigned int *cmd_flags,
		unsigned int **rbptr, unsigned int *cmds_gpu) { }

static inline void adreno_profile_postib_processing(struct kgsl_device *device,
		unsigned int *cmd_flags, unsigned int **rbptr,
		unsigned int *cmds_gpu) { }
#endif

static inline bool adreno_profile_enabled(struct adreno_profile *profile)
{
	return profile->enabled;
}

static inline bool adreno_profile_has_assignments(
	struct adreno_profile *profile)
{
	return list_empty(&profile->assignments_list) ? false : true;
}

static inline bool adreno_profile_assignments_ready(
	struct adreno_profile *profile)
{
	return adreno_profile_enabled(profile) &&
		adreno_profile_has_assignments(profile);
}

#endif
