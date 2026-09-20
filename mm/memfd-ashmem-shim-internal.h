// SPDX-License-Identifier: GPL-2.0

/*
 * Ashmem compatability for memfd
 *
 * Copyright (c) 2025, Google LLC.
 * Author: Isaac J. Manjarres <isaacmanjarres@google.com>
 */

#ifndef _MM_MEMFD_ASHMEM_SHIM_INTERNAL_H
#define _MM_MEMFD_ASHMEM_SHIM_INTERNAL_H

#include <linux/compat.h>
#include <linux/ioctl.h>
#include <linux/types.h>

#define ASHMEM_NAME_LEN		256

/* Return values from ASHMEM_PIN: Was the mapping purged while unpinned? */
#define ASHMEM_NOT_PURGED	0
#define ASHMEM_WAS_PURGED	1

/* Return values from ASHMEM_GET_PIN_STATUS: Is the mapping pinned? */
#define ASHMEM_IS_UNPINNED	0
#define ASHMEM_IS_PINNED	1

struct ashmem_pin {
	__u32 offset;	/* offset into region, in bytes, page-aligned */
	__u32 len;	/* length forward from offset, in bytes, page-aligned */
};

#define __ASHMEMIOC		0x77

#define ASHMEM_SET_NAME		_IOW(__ASHMEMIOC, 1, char[ASHMEM_NAME_LEN])
#define ASHMEM_GET_NAME		_IOR(__ASHMEMIOC, 2, char[ASHMEM_NAME_LEN])
#define ASHMEM_SET_SIZE		_IOW(__ASHMEMIOC, 3, size_t)
#define ASHMEM_GET_SIZE		_IO(__ASHMEMIOC, 4)
#define ASHMEM_SET_PROT_MASK	_IOW(__ASHMEMIOC, 5, unsigned long)
#define ASHMEM_GET_PROT_MASK	_IO(__ASHMEMIOC, 6)
#define ASHMEM_PIN		_IOW(__ASHMEMIOC, 7, struct ashmem_pin)
#define ASHMEM_UNPIN		_IOW(__ASHMEMIOC, 8, struct ashmem_pin)
#define ASHMEM_GET_PIN_STATUS	_IO(__ASHMEMIOC, 9)
#define ASHMEM_PURGE_ALL_CACHES	_IO(__ASHMEMIOC, 10)
#define ASHMEM_GET_FILE_ID		_IOR(__ASHMEMIOC, 11, unsigned long)

/* support of 32bit userspace on 64bit platforms */
#ifdef CONFIG_COMPAT
#define COMPAT_ASHMEM_SET_SIZE		_IOW(__ASHMEMIOC, 3, compat_size_t)
#define COMPAT_ASHMEM_SET_PROT_MASK	_IOW(__ASHMEMIOC, 5, unsigned int)
#endif

#endif /* _MM_MEMFD_ASHMEM_SHIM_INTERNAL_H */
