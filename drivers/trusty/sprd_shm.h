/*
 * drivers/trusty/sprd_shm.h
 *
 * Copyright (C) 2017 Unisoc, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __SPRD_SHM_H__
#define __SPRD_SHM_H__

#include <linux/device.h>
#include <linux/genalloc.h>
#include <linux/idr.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/plist.h>
#include <linux/types.h>

#define TSHM_ALLOC_USER		BIT(0)
#define TSHM_ALLOC_KERN		BIT(1)
#define TSHM_REGISTERED		BIT(2)

/*
 * struct tshm - The tshm shared memory object
 * @node:        The link node element
 * @paddr:       Physical address of the shared memory
 * @kaddr:       Virtual address of the shared memory
 * @size:        Size of shared memory
 * @offset:      Offset of buffer in user space
 * @pages:       The locked pages from userspace
 * @num_pages:   Number of locked pages
 * @dmabuf:      dmabuf used to for exporting to user space
 * @flags:       The flags used to indicate the attribution of shared memory
 * @id:          The unique id of a shared memory object
 */

struct tshm {
	struct tshm_device *dev;
	struct list_head node;
	phys_addr_t paddr;
	void *kaddr;
	size_t size;
	unsigned int offset;
	struct page **pages;
	size_t num_pages;
	struct dma_buf *dmabuf;
	u32 flags;
	int id;
};

/**
 * struct tshm_pool_params - The pool configuration parameters
 * @start                    The start address of pool
 * @size                     The pool size
 * @kern_pool_size           The size of kernel pool
 */
struct tshm_pool_params {
	phys_addr_t start;
	size_t size;
	size_t kern_pool_size;
};

/**
 * struct tshm_device - The metadata shared memory device node
 * @link                The link head of shared memory list
 * @kern_pool           The tshm pool for shared memory between kernel and
 *                      trusted OS
 * @user_pool           The tshm pool for shared memory exported to userspace
 */
struct tshm_device {
	struct miscdevice dev;
	struct device *trusty_dev;
	struct list_head link;
	struct gen_pool *kern_pool;
	struct gen_pool *user_pool;
	struct tshm_pool_params  pool_params;
	void *pool_base_va;
	struct idr idr;
	struct mutex lock;
};

/******************************************************************************
 * Types and definitions for user interfaces
 *****************************************************************************/

/**
 * struct tshm_alloc_data -  The data type used in ioctl call of shared memory
 *			     allocation
 *@size			     The size to be allocated
 *@flags		     The flags passed to ioctl or passed back to caller
 *@id			     The indentifier of shared memory
 */
struct tshm_alloc_data {
	__u64 size;
	__u32 flags;
	__s32 id;
};

/**
 * struct tshm_register_data -  The data type used in ioctl call of shared
 *				memory registration
 *@addr				The userspace virtual address of shared memory
 *				region to be registered
 *@size				The size of shared memory
 *@flags			The flags passed to ioctl/passed back to caller
 *@id				The indentifier of shared memory
 */
struct tshm_register_data {
	__u64 addr;
	__u64 len;
	__u32 flags;
	__s32 id;
};

#define TSHM_IOC_MAGIC		'T'

/*
 * TSHM_IOC_ALLOC  - allocate shared memory
 * Takes this IOC with tshm_alloc_data type to get a shared memory handle
 */
#define TSHM_IOC_ALLOC		_IOWR(TSHM_IOC_MAGIC, 0, struct tshm_alloc_data)

/*
 * TSHM_IOC_REGISTER  - register shared memory
 * Takes this IOC with tshm_register_data type to get a shared memory handle
 */
#define TSHM_IOC_REGISTER	_IOWR(TSHM_IOC_MAGIC, 0, \
				      struct tshm_register_data)

#endif /* __SPRD_SHM_H__  */
