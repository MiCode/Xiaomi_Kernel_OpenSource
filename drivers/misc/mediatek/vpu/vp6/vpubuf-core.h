/*
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Fish Wu <fish.wu@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __VPUBUF_CORE_H__
#define __VPUBUF_CORE_H__

#include <linux/mm_types.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/dma-buf.h>

#include <uapi/mediatek/vpu_ioctl.h>
/* #define MTK_VPU_SUPPORT_ION */

struct vpu_device;
struct vpu_alloc_ctx;

struct vpu_mem_ops {
	void *(*alloc)(void *alloc_ctx, unsigned long size,
		       enum dma_data_direction dma_dir, gfp_t gfp_flags,
		       unsigned int is_cached);
	void (*put)(void *buf_priv);
	struct dma_buf *(*get_dmabuf)(void *buf_priv, unsigned long flags);

	void *(*get_userptr)(void *alloc_ctx, unsigned long vaddr,
			     unsigned long size,
			     enum dma_data_direction dma_dir);
	void (*put_userptr)(void *buf_priv);

	void (*prepare)(void *buf_priv);
	void (*finish)(void *buf_priv);

	void *(*attach_dmabuf)(void *alloc_ctx, struct dma_buf *dbuf,
			       unsigned long size,
			       enum dma_data_direction dma_dir);
	void (*detach_dmabuf)(void *buf_priv);
	int (*map_dmabuf)(void *buf_priv);
	void (*unmap_dmabuf)(void *buf_priv);

	void *(*vaddr)(void *buf_priv);
	void *(*cookie)(void *buf_priv);

	unsigned int (*num_users)(void *buf_priv);

	int (*mmap)(void *buf_priv, struct vm_area_struct *vma);
};

struct vpu_map_ops {
	void (*init_phy_iova)(struct vpu_device *vpu_device);
	void (*deinit_phy_iova)(struct vpu_device *vpu_device);
	struct vpu_kernel_buf *(*kmap_phy_iova)(struct vpu_device *vpu_device,
						uint32_t usage,
						uint64_t phy_addr,
						uint64_t kva_addr,
						uint32_t iova_addr,
						uint32_t size);
	void (*kunmap_phy_iova)(struct vpu_device *vpu_device,
				struct vpu_kernel_buf *vkbuf);
	uint64_t (*import_handle)(struct vpu_device *vpu_device, int fd);
	void (*free_handle)(struct vpu_device *vpu_device, uint64_t id);
};

struct vpu_kbuffer {
	struct vpu_manage *vpu_manage;
	unsigned int memory;
	unsigned int is_cached;

	void *mem_priv;
	unsigned int dma_addr;	/* IOVA */
	void *vaddr;		/* kernel virtual address */
	struct dma_buf *dbuf;
	unsigned int dbuf_mapped;
	unsigned int length;	/* buffer size */
	int dma_fd;		/* for DMABUF */
};

struct vpu_manage {
	const struct vpu_mem_ops *mem_ops;

	/* dynamic memory allocation */
	struct mutex buf_mutex;
	struct list_head buf_list;
	unsigned int buf_num;

	void *alloc_ctx;
};

enum vkbuf_method {
	VKBUF_METHOD_ION = 0,
	VKBUF_METHOD_STD = 1,
};

enum vkbuf_map_case {
	VKBUF_MAP_FPHY_FIOVA,
	VKBUF_MAP_FPHY_DIOVA,
	VKBUF_MAP_DPHY_FIOVA,
	VKBUF_MAP_DPHY_DIOVA,
};

struct vpu_kernel_buf {
	uint64_t handle;
	uint32_t usage;
	struct sg_table sg;
	uint64_t phy_addr;
	uint32_t iova_addr;
	uint32_t size;
	void *kva;
};

struct vpu_dbg_buf {
	uint64_t handle;
};

extern struct vpu_map_ops vpu_ion_mapops;
extern struct vpu_map_ops vpu_dma_mapops;

/* For IOCTL */
int vbuf_std_info(struct vpu_device *vpu_device, struct vbuf_info *info_ctx);
int vbuf_std_alloc(struct vpu_device *vpu_device, struct vbuf_alloc *alloc_ctx);
int vbuf_std_free(struct vpu_device *vpu_device, struct vbuf_free *free_ctx);
int vbuf_std_sync(struct vpu_device *vpu_device, struct vbuf_sync *sync_ctx);
int vbuf_std_init(struct vpu_device *vpu_device);
void vbuf_std_deinit(struct vpu_device *vpu_device);

void vbuf_init_phy_iova(struct vpu_device *vpu_device);
void vbuf_deinit_phy_iova(struct vpu_device *vpu_device);
struct vpu_kernel_buf *vbuf_kmap_phy_iova(struct vpu_device *vpu_device,
					uint32_t usage, uint64_t phy_addr,
					uint64_t kva_addr, uint32_t iova_addr,
					uint32_t size);
void vbuf_kunmap_phy_iova(struct vpu_device *vpu_device,
				struct vpu_kernel_buf *vkbuf);
uint64_t vbuf_import_handle(struct vpu_device *vpu_device, int fd);
void vbuf_free_handle(struct vpu_device *vpu_device, uint64_t handle);

#endif
