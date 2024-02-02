/*
 *  User-visible interface to driver for inter-cell links using the
 *  shared-buffer transport.
 *
 *  Copyright (c) 2016 Cog Systems Pty Ltd.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 */
#ifndef _LINUX_OKL4_LINK_SHBUF_H
#define _LINUX_OKL4_LINK_SHBUF_H

#include <linux/ioctl.h>
#include <linux/types.h>

/*
 * Ioctl that indicates a request to raise the outgoing vIRQ. This value is
 * chosen to avoid conflict with the numbers documented in Linux 4.1's
 * ioctl-numbers.txt. The argument is a payload to transmit to the receiver.
 * Note that consecutive transmissions without an interleaved clear of the
 * interrupt results in the payloads being ORed together.
 */
#define OKL4_LINK_SHBUF_IOCTL_IRQ_TX _IOW(0x8d, 1, __u64)

/*
 * Ioctl that indicates a request to clear any pending incoming vIRQ. The value
 * returned through the argument to the ioctl is the payload, which is also
 * cleared.
 *
 * The caller cannot distinguish between the cases of no pending interrupt and
 * a pending interrupt with payload 0. It is expected that the caller is
 * communicating with a cooperative sender and has polled their file descriptor
 * to determine there is a pending interrupt before using this ioctl.
 */
#define OKL4_LINK_SHBUF_IOCTL_IRQ_CLR _IOR(0x8d, 2, __u64)

#endif /* _LINUX_OKL4_LINK_SHBUF_H */
