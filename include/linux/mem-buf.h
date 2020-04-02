/* SPDX-License-Identifier: GPL-2.0-only */
/*
 *  Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _MEM_BUF_H
#define _MEM_BUF_H

#include <linux/err.h>
#include <linux/errno.h>
#include <uapi/linux/mem-buf.h>

/**
 * struct mem_buf_allocation_data - Data structure that contains information
 * about a memory buffer allocation request.
 * @size: The size (in bytes) of the memory to be requested from a remote VM
 * @nr_acl_entries: The number of ACL entries in @acl_list
 * @acl_list: A list of VMID and permission pairs that describe what VMIDs will
 * have access to the memory, and with what permissions
 * @src_mem_type: The type of memory that the remote VM should allocate
 * (e.g. ION memory)
 * @src_data: A pointer to memory type specific data that the remote VM may need
 * when performing an allocation (e.g. ION memory allocations require a heap ID)
 * @dst_mem_type: The type of memory that the native VM wants (e.g. ION memory)
 * @dst_data: A pointer to memory type specific data that the native VM may
 * need when adding the memory from the remote VM (e.g. ION memory requires a
 * heap ID to add the memory to).
 */
struct mem_buf_allocation_data {
	size_t size;
	unsigned int nr_acl_entries;
	struct acl_entry *acl_list;
	enum mem_buf_mem_type src_mem_type;
	void *src_data;
	enum mem_buf_mem_type dst_mem_type;
	void *dst_data;
};

#if IS_ENABLED(CONFIG_QCOM_MEM_BUF)

void *mem_buf_alloc(struct mem_buf_allocation_data *alloc_data);

int mem_buf_get_fd(void *membuf_desc);

void mem_buf_put(void *membuf_desc);

void *mem_buf_get(int fd);

#else

static inline void *mem_buf_alloc(struct mem_buf_allocation_data *alloc_data)
{
	return ERR_PTR(-ENODEV);
}

static inline int mem_buf_get_fd(void *membuf_desc)
{
	return -ENODEV;
}

static inline void mem_buf_put(void *membuf_desc)
{
}

static inline void *mem_buf_get(int fd)
{
	return ERR_PTR(-ENODEV);
}

#endif /* CONFIG_QCOM_MEM_BUF */
#endif /* _MEM_BUF_H */
