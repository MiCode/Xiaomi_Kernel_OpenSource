/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _UAPI_LINUX_SPEC_SYNC_H
#define _UAPI_LINUX_SPEC_SYNC_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define SPEC_FENCE_SIGNAL_ANY 0x1
#define SPEC_FENCE_SIGNAL_ALL 0x2

/**
 * struct fence_bind_data - data passed to bind ioctl
 * @out_bind_fd:	file descriptor of second fence
 * @fds:	file descriptor list of child fences
 */
struct fence_bind_data {
	__u32	out_bind_fd;
	__u64	fds;
};

/**
 * struct fence_create_data - detailed fence information
 * @num_fences:	Total fences that array needs to carry.
 * @flags:	Flags specifying on how to signal the array
 * @out_bind_fd:	Returns the fence fd.
 */
struct fence_create_data {
	__u32	num_fences;
	__u32	flags;
	__u32	out_bind_fd;
};

#define SPEC_SYNC_MAGIC		'>'

/**
 * DOC: SPEC_SYNC_IOC_BIND - bind two fences
 *
 * Takes a struct fence_bind_data.  binds the child fds with the fence array
 * pointed by fd1.
 */
#define SPEC_SYNC_IOC_BIND	_IOWR(SPEC_SYNC_MAGIC, 3, struct fence_bind_data)

/**
 * DOC: SPEC_SYNC_IOC_CREATE_FENCE - Create a fence array
 *
 * Takes a struct fence_create_data. If num_fences is > 0, fence array will be
 * created and returns the array fd in fence_create_data.fd1
 */
#define SPEC_SYNC_IOC_CREATE_FENCE	_IOWR(SPEC_SYNC_MAGIC, 4, struct fence_create_data)

/**
 * DOC: SPEC_SYNC_IOC_GET_VER - Get Spec driver version
 *
 * Returns Spec driver version.
 */
#define SPEC_SYNC_IOC_GET_VER	_IOWR(SPEC_SYNC_MAGIC, 5, __u64)

#endif /* _UAPI_LINUX_SPEC_SYNC_H */
