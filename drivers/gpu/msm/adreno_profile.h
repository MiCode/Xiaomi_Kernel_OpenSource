/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2013-2014,2019-2021 The Linux Foundation. All rights reserved.
 */
#ifndef __ADRENO_PROFILE_H
#define __ADRENO_PROFILE_H

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
	unsigned int offset;    /* LO offset */
	unsigned int offset_hi; /* HI offset */
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
	struct kgsl_memdesc *shared_buffer;
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
void adreno_profile_init(struct adreno_device *adreno_dev);
void adreno_profile_close(struct adreno_device *adreno_dev);
int adreno_profile_process_results(struct  adreno_device *adreno_dev);
u64 adreno_profile_preib_processing(struct adreno_device *adreno_dev,
		struct adreno_context *drawctxt, u32 *dwords);
u64 adreno_profile_postib_processing(struct  adreno_device *adreno_dev,
		struct adreno_context *drawctxt, u32 *dwords);
#else
static inline void adreno_profile_init(struct adreno_device *adreno_dev) { }
static inline void adreno_profile_close(struct adreno_device *adreno_dev) { }
static inline int adreno_profile_process_results(
		struct adreno_device *adreno_dev)
{
	return 0;
}

static inline u64
adreno_profile_preib_processing(struct adreno_device *adreno_dev,
		struct adreno_context *drawctxt, u32 *dwords)
{
	return 0;
}

static inline u64
adreno_profile_postib_processing(struct adreno_device *adreno_dev,
		struct adreno_context *drawctxt, u32 *dwords)
{
	return 0;
}

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
