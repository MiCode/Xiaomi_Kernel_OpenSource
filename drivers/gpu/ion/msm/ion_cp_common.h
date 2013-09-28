/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#ifndef ION_CP_COMMON_H
#define ION_CP_COMMON_H

#include <asm-generic/errno-base.h>
#include <linux/msm_ion.h>

#define ION_CP_V1	1
#define ION_CP_V2	2

struct ion_cp_buffer {
	phys_addr_t buffer;
	atomic_t secure_cnt;
	int is_secure;
	int want_delayed_unsecure;
	/*
	 * Currently all user/kernel mapping is protected by the heap lock.
	 * This is sufficient to protect the map count as well. The lock
	 * should be used to protect map_cnt if the whole heap lock is
	 * ever removed.
	 */
	atomic_t map_cnt;
	/*
	 * protects secure_cnt for securing.
	 */
	struct mutex lock;
	int version;
	void *data;
	/*
	 * secure is happening at allocation time, ignore version/data check
	 */
	bool ignore_check;
};

#if defined(CONFIG_ION_MSM)
/*
 * ion_cp2_protect_mem - secures memory via trustzone
 *
 * @chunks - physical address of the array containing the chunks to
 *		be locked down
 * @nchunks - number of entries in the array
 * @chunk_size - size of each memory chunk
 * @usage - usage hint
 * @lock - 1 for lock, 0 for unlock
 *
 * return value is the result of the scm call
 */
int ion_cp_change_chunks_state(unsigned long chunks, unsigned int nchunks,
			unsigned int chunk_size, enum cp_mem_usage usage,
			int lock);

int ion_cp_protect_mem(unsigned int phy_base, unsigned int size,
			unsigned int permission_type, int version,
			void *data);

int ion_cp_unprotect_mem(unsigned int phy_base, unsigned int size,
				unsigned int permission_type, int version,
				void *data);

int ion_cp_secure_buffer(struct ion_buffer *buffer, int version, void *data,
				int flags);

int ion_cp_unsecure_buffer(struct ion_buffer *buffer, int force_unsecure);

int msm_ion_secure_table(struct sg_table *table, enum cp_mem_usage usage,
			int flags, bool skip_usage);

int msm_ion_unsecure_table(struct sg_table *table);

#else
static inline int ion_cp_change_chunks_state(unsigned long chunks,
			unsigned int nchunks, unsigned int chunk_size,
			enum cp_mem_usage usage, int lock)
{
	return -ENODEV;
}

static inline int ion_cp_protect_mem(unsigned int phy_base, unsigned int size,
			unsigned int permission_type, int version,
			void *data)
{
	return -ENODEV;
}

static inline int ion_cp_unprotect_mem(unsigned int phy_base, unsigned int size,
				unsigned int permission_type, int version,
				void *data)
{
	return -ENODEV;
}

static inline int ion_cp_secure_buffer(struct ion_buffer *buffer, int version,
				void *data, int flags)
{
	return -ENODEV;
}

static inline int ion_cp_unsecure_buffer(struct ion_buffer *buffer,
				int force_unsecure)
{
	return -ENODEV;
}

int msm_ion_secure_table(struct sg_table *table, enum cp_mem_usage usage,
			int flags, bool skip_usage)
{
	return -ENODEV;
}

int msm_ion_unsecure_table(struct sg_table *table)
{
	return -ENODEV;
}
#endif

#endif
