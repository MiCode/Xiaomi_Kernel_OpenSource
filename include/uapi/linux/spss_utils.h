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
#define NUM_SPU_UEFI_APPS	3
#define CMAC_SIZE_IN_WORDS	4

struct spss_ioc_set_fw_cmac {
	uint32_t cmac[CMAC_SIZE_IN_WORDS];
	uint32_t app_cmacs[NUM_SPU_UEFI_APPS][CMAC_SIZE_IN_WORDS];
} __packed;

#define SPSS_IOC_SET_FW_CMAC \
	_IOWR(SPSS_IOC_MAGIC, 1, struct spss_ioc_set_fw_cmac)

/* ---------- wait for event ------------------------------ */
enum _spss_event_id {
	/* signaled from user */
	SPSS_EVENT_ID_PIL_CALLED	= 0,
	SPSS_EVENT_ID_NVM_READY		= 1,
	SPSS_EVENT_ID_SPU_READY		= 2,
	SPSS_NUM_USER_EVENTS,

	/* signaled from kernel */
	SPSS_EVENT_ID_SPU_POWER_DOWN	= 6,
	SPSS_EVENT_ID_SPU_POWER_UP	= 7,
	SPSS_NUM_EVENTS,
} spss_event_id;

enum _spss_event_status {
	EVENT_STATUS_SIGNALED		= 0xAAAA,
	EVENT_STATUS_NOT_SIGNALED	= 0xFFFF,
	EVENT_STATUS_TIMEOUT		= 0xEEE1,
	EVENT_STATUS_ABORTED		= 0xEEE2,
} spss_event_status;

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

/* ---------- is event isgnaled ------------------------------ */
struct spss_ioc_is_signaled {
	uint32_t event_id;      /* input */
	uint32_t status;        /* output */
} __attribute__((packed));

#define SPSS_IOC_IS_EVENT_SIGNALED \
	_IOWR(SPSS_IOC_MAGIC, 4, struct spss_ioc_is_signaled)

#endif /* _UAPI_SPSS_UTILS_H_ */
