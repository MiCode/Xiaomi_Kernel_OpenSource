/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __TEEI_IOC_H_
#define __TEEI_IOC_H_

#define TEEI_IOC_MAGIC 'T'

#define TEEI_CONFIG_IOC_MAGIC		TEEI_IOC_MAGIC
#define TEEI_CLIENT_IOC_MAGIC		TEEI_IOC_MAGIC
#define UT_TUI_CLIENT_IOC_MAGIC		TEEI_IOC_MAGIC

/*
 * /dev/teei_config
 */
#define MAX_DRV_UUIDS 30
#define UUID_LEN 32

struct init_param {
	char uuids[MAX_DRV_UUIDS][UUID_LEN+1];
	__u32 uuid_count;
	__u32 flag;
};

#define TEEI_CONFIG_IOCTL_INIT_TEEI	\
	_IOWR(TEEI_CONFIG_IOC_MAGIC, 3, struct init_param)

#define TEEI_CONFIG_IOCTL_UNLOCK	\
	_IOWR(TEEI_CONFIG_IOC_MAGIC, 4, int)
/*
 * /dev/ut_keymaster
 */
#define CMD_KM_MEM_CLEAR	_IO(TEEI_IOC_MAGIC, 0x1)
#define CMD_KM_MEM_SEND		_IO(TEEI_IOC_MAGIC, 0x2)
#define CMD_KM_NOTIFY_UTD	_IO(TEEI_IOC_MAGIC, 0x3)
#define CMD_KM_FIRST_TIME_BOOT	_IO(TEEI_IOC_MAGIC, 0x4)

/*
 * /dev/teei_fp
 */
#define CMD_FP_MEM_CLEAR	_IO(TEEI_IOC_MAGIC, 0x1)
#define CMD_FP_CMD		_IO(TEEI_IOC_MAGIC, 0x2)
#define CMD_FP_LOAD_TEE		_IO(TEEI_IOC_MAGIC, 0x4)

#define TEEI_VFS_NOTIFY_DRM	_IOWR(TEEI_CONFIG_IOC_MAGIC, 0x75, int)
#define TEEI_VFS_GET_FP_UUID 0x50

#endif
