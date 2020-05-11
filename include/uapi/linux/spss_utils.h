/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
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

/* ---------- set fw cmac --------------------------------- */
#define NUM_SPU_UEFI_APPS   3

struct spss_ioc_set_fw_cmac {
	__u32 cmac[4];
	__u32 app_cmacs[NUM_SPU_UEFI_APPS][4];
} __packed;

#define SPSS_IOC_SET_FW_CMAC \
	_IOWR(SPSS_IOC_MAGIC, 1, struct spss_ioc_set_fw_cmac)

/* ---------- wait for event ------------------------------ */
#define SPSS_NUM_EVENTS   8

#define EVENT_STATUS_SIGNALED   0xAAAA
#define EVENT_STATUS_TIMEOUT    0xEEE1
#define EVENT_STATUS_ABORTED    0xEEE2

struct spss_ioc_wait_for_event {
	uint32_t event_id;      /* input */
	uint32_t timeout_sec;   /* input */
	uint32_t status;        /* output */
} __packed;

#define SPSS_IOC_WAIT_FOR_EVENT \
	_IOWR(SPSS_IOC_MAGIC, 2, struct spss_ioc_wait_for_event)

/* ---------- signal event ------------------------------ */
struct spss_ioc_signal_event {
	uint32_t event_id;      /* input */
	uint32_t status;        /* output */
} __packed;

#define SPSS_IOC_SIGNAL_EVENT \
	_IOWR(SPSS_IOC_MAGIC, 3, struct spss_ioc_signal_event)

#endif /* _UAPI_SPSS_UTILS_H_ */
