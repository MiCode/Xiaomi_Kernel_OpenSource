/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __ION_DRV_H__
#define __ION_DRV_H__
#include <linux/version.h>

#include <ion.h>

#define BACKTRACE_SIZE 10

/* Structure definitions */

enum ION_CMDS {
	ION_CMD_SYSTEM,
	ION_CMD_MULTIMEDIA,
	ION_CMD_MULTIMEDIA_SEC
};

enum ION_MM_CMDS {
	ION_MM_CONFIG_BUFFER,
	ION_MM_SET_DEBUG_INFO,
	ION_MM_GET_DEBUG_INFO,
	ION_MM_SET_SF_BUF_INFO,
	ION_MM_GET_SF_BUF_INFO,
	ION_MM_CONFIG_BUFFER_EXT,
	ION_MM_ACQ_CACHE_POOL,
	ION_MM_QRY_CACHE_POOL,
	ION_MM_GET_IOVA,
	ION_MM_GET_IOVA_EXT,
};

enum ION_SYS_CMDS {
	ION_SYS_CACHE_SYNC,
	ION_SYS_GET_PHYS,
	ION_SYS_GET_CLIENT,
	ION_SYS_SET_HANDLE_BACKTRACE,
	ION_SYS_SET_CLIENT_NAME,
	ION_SYS_DMA_OP,
};

enum ION_CACHE_SYNC_TYPE {
	ION_CACHE_CLEAN_BY_RANGE,
	ION_CACHE_INVALID_BY_RANGE,
	ION_CACHE_FLUSH_BY_RANGE,
	ION_CACHE_CLEAN_BY_RANGE_USE_PA,
	ION_CACHE_INVALID_BY_RANGE_USE_PA,
	ION_CACHE_FLUSH_BY_RANGE_USE_PA,
	ION_CACHE_CLEAN_ALL,
	ION_CACHE_INVALID_ALL,
	ION_CACHE_FLUSH_ALL
};

enum ION_ERRORE {
	ION_ERROR_CONFIG_LOCKED = 0x10000
};

/* mm or mm_sec heap flag which is do not conflist */
/* with ION_HEAP_FLAG_DEFER_FREE */
#define ION_FLAG_MM_HEAP_INIT_ZERO BIT(16)
#define ION_FLAG_MM_HEAP_SEC_PA BIT(18)

#define ION_FLAG_GET_FIXED_PHYS 0x103

struct ion_sys_cache_sync_param {
	union {
		ion_user_handle_t handle;
		struct ion_handle *kernel_handle;
	};
	void *va;
	unsigned int size;
	enum ION_CACHE_SYNC_TYPE sync_type;
};

enum ION_DMA_TYPE {
	ION_DMA_MAP_AREA,
	ION_DMA_UNMAP_AREA,
	ION_DMA_MAP_AREA_VA,
	ION_DMA_UNMAP_AREA_VA,
	ION_DMA_FLUSH_BY_RANGE,
	ION_DMA_FLUSH_BY_RANGE_USE_VA,
	ION_DMA_CACHE_FLUSH_ALL
};

enum ION_DMA_DIR {
	ION_DMA_FROM_DEVICE,
	ION_DMA_TO_DEVICE,
	ION_DMA_BIDIRECTIONAL,
};

enum ION_M4U_DOMAIN {
	MM_DOMAIN,
	VPU_DOMAIN,

	DOMAIN_NUM
};

struct ion_dma_param {
	union {
		ion_user_handle_t handle;
		void *kernel_handle;
	};
	void *va;
	unsigned int size;
	enum ION_DMA_TYPE dma_type;
	enum ION_DMA_DIR dma_dir;
};

struct ion_sys_get_phys_param {
	union {
		ion_user_handle_t handle;
		struct ion_handle *kernel_handle;
	};
	unsigned long phy_addr;
	unsigned long len;
};

#define ION_MM_DBG_NAME_LEN 48
#define ION_MM_SF_BUF_INFO_LEN 16

struct ion_sys_client_name {
	char name[ION_MM_DBG_NAME_LEN];
};

struct ion_sys_get_client_param {
	unsigned int client;
};

struct ion_sys_record_param {
	pid_t group_id;
	pid_t pid;
	unsigned int action;
	unsigned int address_type;
	unsigned int address;
	unsigned int length;
	unsigned int backtrace[BACKTRACE_SIZE];
	unsigned int backtrace_num;
	struct ion_handle *handle;
	struct ion_client *client;
	struct ion_buffer *buffer;
	struct file *file;
	int fd;
};

struct ion_sys_data {
	enum ION_SYS_CMDS sys_cmd;
	union {
		struct ion_sys_cache_sync_param cache_sync_param;
		struct ion_sys_get_phys_param get_phys_param;
		struct ion_sys_get_client_param get_client_param;
		struct ion_sys_client_name client_name_param;
		struct ion_sys_record_param record_param;
		struct ion_dma_param dma_param;
	};
};

struct ion_mm_config_buffer_param {
	union {
		ion_user_handle_t handle;
		struct ion_handle *kernel_handle;
	};
	int module_id;
	unsigned int security;
	unsigned int coherent;
	unsigned int reserve_iova_start;
	unsigned int reserve_iova_end;
};

struct ion_mm_buf_debug_info {
	union {
		ion_user_handle_t handle;
		struct ion_handle *kernel_handle;
	};
	char dbg_name[ION_MM_DBG_NAME_LEN];
	unsigned int value1;
	unsigned int value2;
	unsigned int value3;
	unsigned int value4;
};

struct ion_mm_sf_buf_info {
	union {
		ion_user_handle_t handle;
		struct ion_handle *kernel_handle;
	};
	unsigned int info[ION_MM_SF_BUF_INFO_LEN];
};

struct ion_mm_pool_info {
	size_t len;
	size_t align;
	unsigned int heap_id_mask;
	unsigned int flags;
	unsigned int ret;
};

struct ion_mm_get_iova_param {
	union {
		ion_user_handle_t handle;
		struct ion_handle *kernel_handle;
	};
	int module_id;
	unsigned int security;
	unsigned int coherent;
	unsigned int reserve_iova_start;
	unsigned int reserve_iova_end;
	u64 phy_addr;
	unsigned long len;
};

struct ion_mm_data {
	enum ION_MM_CMDS mm_cmd;
	union {
		struct ion_mm_config_buffer_param config_buffer_param;
		struct ion_mm_buf_debug_info buf_debug_info_param;
		struct ion_mm_pool_info pool_info_param;
		struct ion_mm_get_iova_param get_phys_param;
	};
};

#ifdef __KERNEL__
#define ION_LOG_TAG "ion_dbg"
#define IONMSG(string, args...)	pr_err("[ION]"string, ##args)
#define IONDBG(string, args...)	pr_debug("[ION]"string, ##args)

/* Exported global variables */
extern struct ion_device *g_ion_device;

/* Exported functions */
long ion_kernel_ioctl(struct ion_client *client, unsigned int cmd,
		      unsigned long arg);
struct ion_handle *ion_drv_get_handle(struct ion_client *client,
				      int user_handle,
				      struct ion_handle *kernel_handle,
				      int from_kernel);
int ion_drv_put_kernel_handle(void *kernel_handle);

/**
 * ion_mm_heap_total_memory() - get mm heap total buffer size.
 */
size_t ion_mm_heap_total_memory(void);
/**
 * ion_mm_heap_total_memory() - get mm heap buffer detail info.
 */
void ion_mm_heap_memory_detail(void);
int ion_drv_create_FB_heap(ion_phys_addr_t fb_base, size_t fb_size);

typedef int (ion_mm_buf_destroy_callback_t)(struct ion_buffer *buffer,
					    unsigned int phy_addr);
int ion_mm_heap_register_buf_destroy_cb(struct ion_buffer *buffer,
					ion_mm_buf_destroy_callback_t *fn);

int ion_dma_map_area(int fd, ion_user_handle_t handle, int dir);
int ion_dma_unmap_area(int fd, ion_user_handle_t handle, int dir);
void ion_dma_map_area_va(void *start, size_t size, enum ION_DMA_DIR dir);
void ion_dma_unmap_area_va(void *start, size_t size, enum ION_DMA_DIR dir);

struct ion_heap *ion_mm_heap_create(struct ion_platform_heap *unused);
void ion_mm_heap_destroy(struct ion_heap *heap);

struct ion_heap *ion_fb_heap_create(struct ion_platform_heap *heap_data);
void ion_fb_heap_destroy(struct ion_heap *heap);

int ion_device_destroy_heaps(struct ion_device *dev);

struct ion_heap *ion_sec_heap_create(struct ion_platform_heap *unused);
void ion_sec_heap_destroy(struct ion_heap *heap);
void ion_sec_heap_dump_info(void);
#endif

#endif
