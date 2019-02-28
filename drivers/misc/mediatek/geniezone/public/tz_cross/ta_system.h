/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/** Commands for TA SYSTEM **/

#ifndef __TRUSTZONE_TA_SYSTEM__
#define __TRUSTZONE_TA_SYSTEM__

/* / Special handle for system connect. */
/* / NOTE: Handle manager guarantee normal handle will have bit31=0. */
#define MTEE_SESSION_HANDLE_SYSTEM 0xFFFF1234


/* Session Management */
#define TZCMD_SYS_INIT 0
#define TZCMD_SYS_SESSION_CREATE 1
#define TZCMD_SYS_SESSION_CLOSE 2
#define TZCMD_SYS_IRQ 3
#define TZCMD_SYS_THREAD_CREATE 4

#define GZ_MSG_DATA_MAX_LEN 1024
struct gz_syscall_cmd_param {
	int handle;
	int command;
	int ree_service;
	int payload_size;
	int paramTypes;
	int dummy[4];
	union MTEEC_PARAM param[4];
	char data[GZ_MSG_DATA_MAX_LEN];
};

#define GZ_MSG_HEADER_LEN                                                      \
	(sizeof(struct gz_syscall_cmd_param) - GZ_MSG_DATA_MAX_LEN)

#endif /* __TRUSTZONE_TA_SYSTEM__ */
