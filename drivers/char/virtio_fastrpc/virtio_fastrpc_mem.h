/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __VIRTIO_FASTRPC_MEM_H__
#define __VIRTIO_FASTRPC_MEM_H__

#include <linux/dma-buf.h>
#include <linux/types.h>
#include "virtio_fastrpc_core.h"

struct vfastrpc_mmap {
	struct hlist_node hn;
	struct vfastrpc_file *vfl;
	int fd;
	uint32_t flags;
	struct dma_buf *buf;
	struct sg_table *table;
	struct dma_buf_attachment *attach;
	uint64_t phys;
	size_t size;
	uintptr_t va;
	size_t len;
	uintptr_t raddr;
	int refs;
	/*
	 * Used to store attributes of the fastrpc_mmap when it's created,
	 * such as FASTRPC_ATTR_KEEP_MAP.
	 */
	unsigned int attr;
};

struct vfastrpc_buf {
	struct hlist_node hn;
	struct hlist_node hn_rem;
	struct vfastrpc_file *vfl;
	size_t size;
	struct sg_table sgt;
	struct page **pages;
	void *va;
	unsigned long dma_attr;
	/*
	 * Indicate cacheability of the map, set to 0 for uncached buf,
	 * set to FASTRPC_MAP_ATTR_CACHED for cached buf.
	 */
	u32 map_attr;
	uintptr_t raddr;
	uint32_t flags;
	int remote;
};

enum vfastrpc_buf_type {
	VFASTRPC_BUF_TYPE_NORMAL,
	VFASTRPC_BUF_TYPE_ION,
	VFASTRPC_BUF_TYPE_INTERNAL,
};

struct vfastrpc_buf_desc {
	enum vfastrpc_buf_type type;
	struct vfastrpc_buf *buf;
};

/* vfastrpc_mmap_* APIs are not thread-safe, caller needs to take fl->map_mutex */
int vfastrpc_mmap_create(struct vfastrpc_file *vfl, int fd, unsigned int attr,
	uintptr_t va, size_t len, int mflags, struct vfastrpc_mmap **ppmap);

int vfastrpc_mmap_find(struct vfastrpc_file *vfl, int fd,
		uintptr_t va, size_t len, int mflags, int refs,
		struct vfastrpc_mmap **ppmap);

void vfastrpc_mmap_free(struct vfastrpc_file *vfl,
		struct vfastrpc_mmap *map, uint32_t force_free);

int vfastrpc_mmap_remove_fd(struct vfastrpc_file *vfl, int fd, u32 *entries);

int vfastrpc_mmap_remove(struct vfastrpc_file *vfl, int fd, uintptr_t va,
		size_t len, struct vfastrpc_mmap **ppmap);

void vfastrpc_mmap_add(struct vfastrpc_file *vfl, struct vfastrpc_mmap *map);

int vfastrpc_buf_alloc(struct vfastrpc_file *vfl, size_t size,
				unsigned long dma_attr, uint32_t rflags,
				int remote, pgprot_t prot, struct vfastrpc_buf **obuf);

void vfastrpc_buf_free(struct vfastrpc_buf *buf, int cache);
#endif /*__VIRTIO_FASTRPC_MEM_H__*/
