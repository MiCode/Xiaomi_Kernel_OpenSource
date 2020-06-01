/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */
#ifndef _UAPI_QRNG_H_
#define _UAPI_QRNG_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#define QRNG_IOC_MAGIC    0x100

#define QRNG_IOCTL_RESET_BUS_BANDWIDTH\
	_IO(QRNG_IOC_MAGIC, 1)

#endif /* _UAPI_QRNG_H_ */
