/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _UAPI_SPCOM_H_
#define _UAPI_SPCOM_H_

#include <linux/types.h>	/* uint32_t, bool */
#ifndef BIT
	#define BIT(x) (1 << x)
#endif

#ifndef PAGE_SIZE
	#define PAGE_SIZE 4096
#endif

/**
 * @brief - Secure Processor Communication interface to user space spcomlib.
 *
 * Sending data and control commands by write() file operation.
 * Receiving data by read() file operation.
 * Getting the next request size by read() file operation,
 * with special size SPCOM_GET_NEXT_REQUEST_SIZE.
 */

/*
 * Maximum number of channel between Secure Processor and HLOS.
 * including predefined channels, like "sp_kernel".
 */
#define SPCOM_MAX_CHANNELS	0x20

/* Maximum size (including null) for channel names */
#define SPCOM_CHANNEL_NAME_SIZE		32
/*
 * file read(fd, buf, size) with this size,
 * hints the kernel that user space wants to read the next-req-size.
 * This size is bigger than both SPCOM_MAX_REQUEST_SIZE and
 * SPCOM_MAX_RESPONSE_SIZE , so it is not a valid data size.
 */
#define SPCOM_GET_NEXT_REQUEST_SIZE	(PAGE_SIZE-1)

/* Command Id between spcomlib and spcom driver, on write() */
enum spcom_cmd_id {
	SPCOM_CMD_LOAD_APP	= 0x4C4F4144, /* "LOAD" = 0x4C4F4144 */
	SPCOM_CMD_RESET_SP	= 0x52455354, /* "REST" = 0x52455354 */
	SPCOM_CMD_SEND		= 0x53454E44, /* "SEND" = 0x53454E44 */
	SPCOM_CMD_SEND_MODIFIED	= 0x534E444D, /* "SNDM" = 0x534E444D */
	SPCOM_CMD_LOCK_ION_BUF  = 0x4C4F434B, /* "LOCK" = 0x4C4F434B */
	SPCOM_CMD_UNLOCK_ION_BUF = 0x554C434B, /* "ULCK" = 0x4C4F434B */
	SPCOM_CMD_FSSR		= 0x46535352, /* "FSSR" = 0x46535352 */
	SPCOM_CMD_CREATE_CHANNEL = 0x43524554, /* "CRET" = 0x43524554 */

#define SPCOM_CMD_ENABLE_SSR \
	SPCOM_CMD_ENABLE_SSR
	SPCOM_CMD_ENABLE_SSR     = 0x45535352,   /* "ESSR" =0x45535352*/

#define SPCOM_CMD_RESTART_SP \
	SPCOM_CMD_RESTART_SP
	SPCOM_CMD_RESTART_SP         = 0x52535452,   /* "RSTR" = 0x52535452 */
};

/*
 * @note: Event types that are always implicitly polled:
 * POLLERR=0x08 | POLLHUP=0x10 | POLLNVAL=0x20
 * so bits 3,4,5 can't be used
 */
enum spcom_poll_events {
	SPCOM_POLL_LINK_STATE	= BIT(1),
	SPCOM_POLL_CH_CONNECT	= BIT(2),
	SPCOM_POLL_READY_FLAG	= BIT(14), /* output */
	SPCOM_POLL_WAIT_FLAG	= BIT(15), /* if set , wait for the event */
};

/* Common Command structure between User Space and spcom driver, on write() */
struct spcom_user_command {
	enum spcom_cmd_id cmd_id;
	uint32_t arg;
} __packed;

/* Command structure between User Space and spcom driver, on write() */
struct spcom_send_command {
	enum spcom_cmd_id cmd_id;
	uint32_t timeout_msec;
	uint32_t buf_size;
	char buf[0]; /* Variable buffer size - must be last field */
} __packed;

/* Command structure between userspace spcomlib and spcom driver, on write() */
struct spcom_user_create_channel_command {
	enum spcom_cmd_id cmd_id;
	char ch_name[SPCOM_CHANNEL_NAME_SIZE];
#define SPCOM_IS_SHARABLE_SUPPORTED
	bool is_sharable;
} __packed;

/* Command structure between userspace spcomlib and spcom driver, on write() */
#define SPCOM_USER_RESTART_SP_CMD
struct spcom_user_restart_sp_command {
	enum spcom_cmd_id cmd_id;
	uint32_t arg;
#define SPCOM_IS_UPDATED_SUPPORETED
	uint32_t is_updated;
} __packed;

/* maximum ION buf for send-modfied-command */
#define SPCOM_MAX_ION_BUF 4

struct spcom_ion_info {
	int32_t fd; /* ION buffer File Descriptor, set -1 for invalid fd */
	uint32_t buf_offset; /* virtual address offset in request/response */
};

/* Pass this FD to unlock all ION buffer for the specific channel */
#define SPCOM_ION_FD_UNLOCK_ALL	0xFFFF

struct spcom_ion_handle {
	int32_t fd;		/* File Descriptor associated with the buffer */
};

/* Command structure between User Space and spcom driver, on write() */
struct spcom_user_send_modified_command {
	enum spcom_cmd_id cmd_id;
	struct spcom_ion_info ion_info[SPCOM_MAX_ION_BUF];
	uint32_t timeout_msec;
	uint32_t buf_size;
	char buf[0]; /* Variable buffer size - must be last field */
} __packed;

enum {
	SPCOM_IONFD_CMD,
	SPCOM_POLL_CMD,
};

enum spcom_poll_cmd_id {
	SPCOM_LINK_STATE_REQ,
	SPCOM_CH_CONN_STATE_REQ,
};

struct spcom_poll_param {
	/* input parameters */
	bool wait;
	enum spcom_poll_cmd_id cmd_id;
	/* output parameter */
	int retval;
} __packed;

#define SPCOM_IOCTL_MAGIC	'S'
#define SPCOM_GET_IONFD _IOR(SPCOM_IOCTL_MAGIC, SPCOM_IONFD_CMD, \
			     struct spcom_ion_handle)
#define SPCOM_SET_IONFD _IOW(SPCOM_IOCTL_MAGIC, SPCOM_IONFD_CMD, \
			     struct spcom_ion_handle)
#define SPCOM_POLL_STATE _IOWR(SPCOM_IOCTL_MAGIC, SPCOM_POLL_CMD, \
			       struct spcom_poll_param)

#endif /* _UAPI_SPCOM_H_ */
