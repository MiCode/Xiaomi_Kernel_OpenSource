/*
 * vservices/ioctl.h - Interface to service character devices
 *
 * Copyright (c) 2016, Cog Systems Pty Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_PUBLIC_VSERVICES_IOCTL_H__
#define __LINUX_PUBLIC_VSERVICES_IOCTL_H__

#include <linux/types.h>
#include <linux/compiler.h>

/* ioctls that work on any opened service device */
#define IOCTL_VS_RESET_SERVICE		_IO('4', 0)
#define IOCTL_VS_GET_NAME		_IOR('4', 1, char[16])
#define IOCTL_VS_GET_PROTOCOL		_IOR('4', 2, char[32])

/*
 * Claim a device for user I/O (if no kernel driver is attached). The claim
 * persists until the char device is closed.
 */
struct vs_ioctl_bind {
	__u32 send_quota;
	__u32 recv_quota;
	__u32 send_notify_bits;
	__u32 recv_notify_bits;
	size_t msg_size;
};
#define IOCTL_VS_BIND_CLIENT _IOR('4', 3, struct vs_ioctl_bind)
#define IOCTL_VS_BIND_SERVER _IOWR('4', 4, struct vs_ioctl_bind)

/* send and receive messages and notifications */
#define IOCTL_VS_NOTIFY _IOW('4', 5, __u32)
struct vs_ioctl_iovec {
	union {
		__u32 iovcnt; /* input */
		__u32 notify_bits; /* output (recv only) */
	};
	struct iovec *iov;
};
#define IOCTL_VS_SEND _IOW('4', 6, struct vs_ioctl_iovec)
#define IOCTL_VS_RECV _IOWR('4', 7, struct vs_ioctl_iovec)

#endif /* __LINUX_PUBLIC_VSERVICES_IOCTL_H__ */
