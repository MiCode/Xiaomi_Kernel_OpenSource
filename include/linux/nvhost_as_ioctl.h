/*
 * include/linux/nvhost_as_ioctl.h
 *
 * Tegra Host Address Space Driver
 *
 * Copyright (c) 2011-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __LINUX_NVHOST_AS_IOCTL_H
#define __LINUX_NVHOST_AS_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#if !defined(__KERNEL__)
#define __user
#endif

#define NVHOST_AS_IOCTL_MAGIC 'A'

/*
 * /dev/nvhost-as-* devices
 *
 * Opening a '/dev/nvhost-as-<module_name>' device node creates a new address
 * space.  nvhost channels (for the same module) can then be bound to such an
 * address space to define the addresses it has access to.
 *
 * Once a nvhost channel has been bound to an address space it cannot be
 * unbound.  There is no support for allowing an nvhost channel to change from
 * one address space to another (or from one to none).
 *
 * As long as there is an open device file to the address space, or any bound
 * nvhost channels it will be valid.  Once all references to the address space
 * are removed the address space is deleted.
 *
 */


/*
 * Allocating an address space range:
 *
 * Address ranges created with this ioctl are reserved for later use with
 * fixed-address buffer mappings.
 *
 * If _FLAGS_FIXED_OFFSET is specified then the new range starts at the 'offset'
 * given.  Otherwise the address returned is chosen to be a multiple of 'align.'
 *
 */
struct nvhost32_as_alloc_space_args {
	__u32 pages;     /* in, pages */
	__u32 page_size; /* in, bytes */
	__u32 flags;     /* in */
#define NVHOST_AS_ALLOC_SPACE_FLAGS_FIXED_OFFSET 0x1
#define NVHOST_AS_ALLOC_SPACE_FLAGS_SPARSE 0x2
	union {
		__u64 offset; /* inout, byte address valid iff _FIXED_OFFSET */
		__u64 align;  /* in, alignment multiple (0:={1 or n/a}) */
	} o_a;
};

struct nvhost_as_alloc_space_args {
	__u32 pages;     /* in, pages */
	__u32 page_size; /* in, bytes */
	__u32 flags;     /* in */
	__u32 padding;     /* in */
	union {
		__u64 offset; /* inout, byte address valid iff _FIXED_OFFSET */
		__u64 align;  /* in, alignment multiple (0:={1 or n/a}) */
	} o_a;
};

/*
 * Releasing an address space range:
 *
 * The previously allocated region starting at 'offset' is freed.  If there are
 * any buffers currently mapped inside the region the ioctl will fail.
 */
struct nvhost_as_free_space_args {
	__u64 offset; /* in, byte address */
	__u32 pages;     /* in, pages */
	__u32 page_size; /* in, bytes */
};

/*
 * Binding a nvhost channel to an address space:
 *
 * A channel must be bound to an address space before allocating a gpfifo
 * in nvhost.  The 'channel_fd' given here is the fd used to allocate the
 * channel.  Once a channel has been bound to an address space it cannot
 * be unbound (except for when the channel is destroyed).
 */
struct nvhost_as_bind_channel_args {
	__u32 channel_fd; /* in */
} __packed;

/*
 * Mapping nvmap buffers into an address space:
 *
 * The start address is the 'offset' given if _FIXED_OFFSET is specified.
 * Otherwise the address returned is a multiple of 'align.'
 *
 * If 'page_size' is set to 0 the nvmap buffer's allocation alignment/sizing
 * will be used to determine the page size (largest possible).  The page size
 * chosen will be returned back to the caller in the 'page_size' parameter in
 * that case.
 */
struct nvhost_as_map_buffer_args {
	__u32 flags;          /* in/out */
#define NVHOST_AS_MAP_BUFFER_FLAGS_FIXED_OFFSET	    BIT(0)
#define NVHOST_AS_MAP_BUFFER_FLAGS_CACHEABLE	    BIT(2)
	__u32 nvmap_fd;       /* in */
	__u32 nvmap_handle;   /* in */
	__u32 page_size;      /* inout, 0:= best fit to buffer */
	union {
		__u64 offset; /* inout, byte address valid iff _FIXED_OFFSET */
		__u64 align;  /* in, alignment multiple (0:={1 or n/a})   */
	} o_a;
};

/*
 * Unmapping a buffer:
 *
 * To unmap a previously mapped buffer set 'offset' to the offset returned in
 * the mapping call.  This includes where a buffer has been mapped into a fixed
 * offset of a previously allocated address space range.
 */
struct nvhost_as_unmap_buffer_args {
	__u64 offset; /* in, byte address */
};

#define NVHOST_AS_IOCTL_BIND_CHANNEL \
	_IOWR(NVHOST_AS_IOCTL_MAGIC, 1, struct nvhost_as_bind_channel_args)
#define NVHOST32_AS_IOCTL_ALLOC_SPACE \
	_IOWR(NVHOST_AS_IOCTL_MAGIC, 2, struct nvhost32_as_alloc_space_args)
#define NVHOST_AS_IOCTL_FREE_SPACE \
	_IOWR(NVHOST_AS_IOCTL_MAGIC, 3, struct nvhost_as_free_space_args)
#define NVHOST_AS_IOCTL_MAP_BUFFER \
	_IOWR(NVHOST_AS_IOCTL_MAGIC, 4, struct nvhost_as_map_buffer_args)
#define NVHOST_AS_IOCTL_UNMAP_BUFFER \
	_IOWR(NVHOST_AS_IOCTL_MAGIC, 5, struct nvhost_as_unmap_buffer_args)
#define NVHOST_AS_IOCTL_ALLOC_SPACE \
	_IOWR(NVHOST_AS_IOCTL_MAGIC, 6, struct nvhost_as_alloc_space_args)

#define NVHOST_AS_IOCTL_LAST		\
	_IOC_NR(NVHOST_AS_IOCTL_ALLOC_SPACE)
#define NVHOST_AS_IOCTL_MAX_ARG_SIZE	\
	sizeof(struct nvhost_as_map_buffer_args)


#endif
