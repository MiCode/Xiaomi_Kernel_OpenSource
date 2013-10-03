/*
 * Copyright (C) 2011 Google, Inc
 * Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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

#include <linux/memory_alloc.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <mach/scm.h>
#include <linux/highmem.h>

#include "../ion_priv.h"
#include "ion_cp_common.h"

#define MEM_PROTECT_LOCK_ID	0x05
#define MEM_PROTECT_LOCK_ID2 0x0A

struct cp2_mem_chunks {
	unsigned int *chunk_list;
	unsigned int chunk_list_size;
	unsigned int chunk_size;
} __attribute__ ((__packed__));

struct cp2_lock2_req {
	struct cp2_mem_chunks chunks;
	unsigned int mem_usage;
	unsigned int lock;
	unsigned int flags;
} __attribute__ ((__packed__));

/*  SCM related code for locking down memory for content protection */

#define SCM_CP_LOCK_CMD_ID	0x1
#define SCM_CP_PROTECT		0x1
#define SCM_CP_UNPROTECT	0x0

struct cp_lock_msg {
	unsigned int start;
	unsigned int end;
	unsigned int permission_type;
	unsigned char lock;
} __attribute__ ((__packed__));

static int ion_cp_protect_mem_v1(unsigned int phy_base, unsigned int size,
			      unsigned int permission_type)
{
	struct cp_lock_msg cmd;
	cmd.start = phy_base;
	cmd.end = phy_base + size;
	cmd.permission_type = permission_type;
	cmd.lock = SCM_CP_PROTECT;

	return scm_call(SCM_SVC_MP, SCM_CP_LOCK_CMD_ID,
			&cmd, sizeof(cmd), NULL, 0);
}

static int ion_cp_unprotect_mem_v1(unsigned int phy_base, unsigned int size,
				unsigned int permission_type)
{
	struct cp_lock_msg cmd;
	cmd.start = phy_base;
	cmd.end = phy_base + size;
	cmd.permission_type = permission_type;
	cmd.lock = SCM_CP_UNPROTECT;

	return scm_call(SCM_SVC_MP, SCM_CP_LOCK_CMD_ID,
			&cmd, sizeof(cmd), NULL, 0);
}

#define V2_CHUNK_SIZE	SZ_1M

static int ion_cp_change_mem_v2(unsigned int phy_base, unsigned int size,
			      void *data, int lock)
{
	enum cp_mem_usage usage = (enum cp_mem_usage) data;
	unsigned long *chunk_list;
	int nchunks;
	int ret;
	int i;
	int chunk_list_len;
	phys_addr_t chunk_list_phys;

	if (usage < 0 || usage >= MAX_USAGE)
		return -EINVAL;

	if (!IS_ALIGNED(size, V2_CHUNK_SIZE)) {
		pr_err("%s: heap size is not aligned to %x\n",
			__func__, V2_CHUNK_SIZE);
		return -EINVAL;
	}

	nchunks = size / V2_CHUNK_SIZE;
	chunk_list_len = sizeof(unsigned long)*nchunks;

	chunk_list = kmalloc(chunk_list_len, GFP_KERNEL);
	if (!chunk_list)
		return -ENOMEM;

	chunk_list_phys = virt_to_phys(chunk_list);
	for (i = 0; i < nchunks; i++)
		chunk_list[i] = phy_base + i * V2_CHUNK_SIZE;

	/*
	 * Flush the chunk list before sending the memory to the
	 * secure environment to ensure the data is actually present
	 * in RAM
	 */
	dmac_flush_range(chunk_list, chunk_list + chunk_list_len);
	outer_flush_range(chunk_list_phys,
			  chunk_list_phys + chunk_list_len);

	ret = ion_cp_change_chunks_state(chunk_list_phys,
					nchunks, V2_CHUNK_SIZE, usage, lock);

	kfree(chunk_list);
	return ret;
}

int ion_cp_protect_mem(unsigned int phy_base, unsigned int size,
			      unsigned int permission_type, int version,
			      void *data)
{
	switch (version) {
	case ION_CP_V1:
		return ion_cp_protect_mem_v1(phy_base, size, permission_type);
	case ION_CP_V2:
		return ion_cp_change_mem_v2(phy_base, size, data,
						SCM_CP_PROTECT);
	default:
		return -EINVAL;
	}
}

int ion_cp_unprotect_mem(unsigned int phy_base, unsigned int size,
			      unsigned int permission_type, int version,
			      void *data)
{
	switch (version) {
	case ION_CP_V1:
		return ion_cp_unprotect_mem_v1(phy_base, size, permission_type);
	case ION_CP_V2:
		return ion_cp_change_mem_v2(phy_base, size, data,
						SCM_CP_UNPROTECT);
	default:
		return -EINVAL;
	}
}

int ion_cp_change_chunks_state(unsigned long chunks, unsigned int nchunks,
				unsigned int chunk_size,
				enum cp_mem_usage usage,
				int lock)
{
	struct cp2_lock2_req request;
	u32 resp;

	request.mem_usage = usage;
	request.lock = lock;
	request.flags = 0;

	request.chunks.chunk_list = (unsigned int *)chunks;
	request.chunks.chunk_list_size = nchunks;
	request.chunks.chunk_size = chunk_size;

	kmap_flush_unused();
	kmap_atomic_flush_unused();
	return scm_call(SCM_SVC_MP, MEM_PROTECT_LOCK_ID2,
			&request, sizeof(request), &resp, sizeof(resp));

}

/* Must be protected by ion_cp_buffer lock */
static int __ion_cp_protect_buffer(struct ion_buffer *buffer, int version,
					void *data, int flags)
{
	struct ion_cp_buffer *buf = buffer->priv_virt;
	int ret_value = 0;

	if (atomic_inc_return(&buf->secure_cnt) == 1) {
		ret_value = ion_cp_protect_mem(buf->buffer,
				buffer->size, 0,
				version, data);

		if (ret_value) {
			pr_err("Failed to secure buffer %p, error %d\n",
				buffer, ret_value);
			atomic_dec(&buf->secure_cnt);
		} else {
			pr_debug("Protected buffer %p from %pa (size %x)\n",
				buffer, &buf->buffer,
				buffer->size);
			buf->want_delayed_unsecure |=
				flags & ION_UNSECURE_DELAYED ? 1 : 0;
			buf->data = data;
			buf->version = version;
		}
	}
	pr_debug("buffer %p protect count %d\n", buffer,
		atomic_read(&buf->secure_cnt));
	BUG_ON(atomic_read(&buf->secure_cnt) < 0);
	return ret_value;
}

/* Must be protected by ion_cp_buffer lock */
static int __ion_cp_unprotect_buffer(struct ion_buffer *buffer, int version,
					void *data, int force_unsecure)
{
	struct ion_cp_buffer *buf = buffer->priv_virt;
	int ret_value = 0;

	if (force_unsecure) {
		if (!buf->is_secure || atomic_read(&buf->secure_cnt) == 0)
			return 0;

		if (atomic_read(&buf->secure_cnt) != 1) {
			WARN(1, "Forcing unsecure of buffer with outstanding secure count %d!\n",
				atomic_read(&buf->secure_cnt));
			atomic_set(&buf->secure_cnt, 1);
		}
	}

	if (atomic_dec_and_test(&buf->secure_cnt)) {
		ret_value = ion_cp_unprotect_mem(
			buf->buffer, buffer->size,
			0, version, data);

		if (ret_value) {
			pr_err("Failed to unsecure buffer %p, error %d\n",
				buffer, ret_value);
			/*
			 * If the force unsecure is happening, the buffer
			 * is being destroyed. We failed to unsecure the
			 * buffer even though the memory is given back.
			 * Just die now rather than discovering later what
			 * happens when trying to use the secured memory as
			 * unsecured...
			 */
			BUG_ON(force_unsecure);
			/* Bump the count back up one to try again later */
			atomic_inc(&buf->secure_cnt);
		} else {
			buf->version = -1;
			buf->data = NULL;
		}
	}
	pr_debug("buffer %p unprotect count %d\n", buffer,
		atomic_read(&buf->secure_cnt));
	BUG_ON(atomic_read(&buf->secure_cnt) < 0);
	return ret_value;
}

int ion_cp_secure_buffer(struct ion_buffer *buffer, int version, void *data,
				int flags)
{
	int ret_value;
	struct ion_cp_buffer *buf = buffer->priv_virt;

	mutex_lock(&buf->lock);
	if (!buf->is_secure) {
		pr_err("%s: buffer %p was not allocated as secure\n",
			__func__, buffer);
		ret_value = -EINVAL;
		goto out_unlock;
	}

	if (ION_IS_CACHED(buffer->flags)) {
		pr_err("%s: buffer %p was allocated as cached\n",
			__func__, buffer);
		ret_value = -EINVAL;
		goto out_unlock;
	}

	if (atomic_read(&buf->map_cnt)) {
		pr_err("%s: cannot secure buffer %p with outstanding mappings. Total count: %d",
			__func__, buffer, atomic_read(&buf->map_cnt));
		ret_value = -EINVAL;
		goto out_unlock;
	}

	if (atomic_read(&buf->secure_cnt) && !buf->ignore_check) {
		if (buf->version != version || buf->data != data) {
			pr_err("%s: Trying to re-secure buffer with different values",
				__func__);
			pr_err("Last secured version: %d Currrent %d\n",
				buf->version, version);
			pr_err("Last secured data: %p current %p\n",
				buf->data, data);
			ret_value = -EINVAL;
			goto out_unlock;
		}
	}
	ret_value = __ion_cp_protect_buffer(buffer, version, data, flags);

out_unlock:
	mutex_unlock(&buf->lock);
	return ret_value;
}

int ion_cp_unsecure_buffer(struct ion_buffer *buffer, int force_unsecure)
{
	int ret_value = 0;
	struct ion_cp_buffer *buf = buffer->priv_virt;

	mutex_lock(&buf->lock);
	ret_value = __ion_cp_unprotect_buffer(buffer, buf->version, buf->data,
						force_unsecure);
	mutex_unlock(&buf->lock);
	return ret_value;
}
