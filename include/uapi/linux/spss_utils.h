/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _UAPI_SPSS_UTILS_H_
#define _UAPI_SPSS_UTILS_H_

#include <linux/types.h>    /* uint32_t, bool */
#include <linux/ioctl.h>    /* ioctl() */

/**
 * @brief - Secure Processor Utilities interface to user space
 *
 * The kernel spss_utils driver interface to user space via IOCTL
 * and SYSFS (device attributes).
 */

#define SPSS_IOC_MAGIC  'S'
#define NUM_SPU_UEFI_APPS   3

struct spss_ioc_set_fw_cmac {
	uint32_t cmac[4];
	uint32_t app_cmacs[NUM_SPU_UEFI_APPS][4];
} __packed;

#define SPSS_IOC_SET_FW_CMAC \
	_IOWR(SPSS_IOC_MAGIC, 1, struct spss_ioc_set_fw_cmac)

#endif /* _UAPI_SPSS_UTILS_H_ */
