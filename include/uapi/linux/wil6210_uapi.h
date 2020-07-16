/* SPDX-License-Identifier: ISC */
/*
 * Copyright (c) 2014 Qualcomm Atheros, Inc.
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __WIL6210_UAPI_H__
#define __WIL6210_UAPI_H__

#if !defined(__KERNEL__)
#define __user
#endif

#include <linux/types.h>
#include <linux/sockios.h>

/* Numbers SIOCDEVPRIVATE and SIOCDEVPRIVATE + 1
 * are used by Android devices to implement PNO (preferred network offload).
 * Albeit it is temporary solution, use different numbers to avoid conflicts
 */

/**
 * Perform 32-bit I/O operation to the card memory
 *
 * User code should arrange data in memory like this:
 *
 *	struct wil_memio io;
 *	struct ifreq ifr = {
 *		.ifr_data = &io,
 *	};
 */
#define WIL_IOCTL_MEMIO (SIOCDEVPRIVATE + 2)

/**
 * Perform block I/O operation to the card memory
 *
 * User code should arrange data in memory like this:
 *
 *	void *buf;
 *	struct wil_memio_block io = {
 *		.block = buf,
 *	};
 *	struct ifreq ifr = {
 *		.ifr_data = &io,
 *	};
 */
#define WIL_IOCTL_MEMIO_BLOCK (SIOCDEVPRIVATE + 3)

/** operation to perform */
#define WIL_MMIO_READ 0
#define WIL_MMIO_WRITE 1
#define WIL_MMIO_OP_MASK 0xff

/** addressing mode to use */
#define WIL_MMIO_ADDR_LINKER (0 << 8)
#define WIL_MMIO_ADDR_AHB (1 << 8)
#define WIL_MMIO_ADDR_BAR (2 << 8)
#define WIL_MMIO_ADDR_MASK 0xff00

struct wil_memio {
	__u32 op; /* enum wil_memio_op */
	__u32 addr; /* should be 32-bit aligned */
	__u32 val;
};

struct wil_memio_block {
	__u32 op; /* enum wil_memio_op */
	__u32 addr; /* should be 32-bit aligned */
	__u32 size; /* should be multiple of 4 */
	__u64 __user block; /* block address */
};

#endif /* __WIL6210_UAPI_H__ */
