/*
 * Copyright (c) 2013-2017 TRUSTONIC LIMITED
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

#ifndef __MC_ADMIN_IOCTL_H__
#define __MC_ADMIN_IOCTL_H__

#include <linux/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MC_ADMIN_DEVNODE "mobicore"

/* Driver/daemon commands */
enum {
	/* Command 0 is reserved */
	MC_DRV_GET_ROOT_CONTAINER = 1,
	MC_DRV_GET_SP_CONTAINER = 2,
	MC_DRV_GET_TRUSTLET_CONTAINER = 3,
	MC_DRV_GET_TRUSTLET = 4,
	MC_DRV_SIGNAL_CRASH = 5,
};

/* MobiCore IOCTL magic number */
#define MC_IOC_MAGIC    'M'

struct mc_admin_request {
	__u32		 request_id;	/* Unique request identifier */
	__u32		 command;	/* Command to daemon */
	struct mc_uuid_t uuid;		/* UUID of trustlet, if relevant */
	__u32		 is_gp;		/* Whether trustlet is GP */
	__u32		 spid;		/* SPID of trustlet, if relevant */
};

struct mc_admin_response {
	__u32		request_id;	/* Unique request identifier */
	__u32		error_no;	/* Errno from daemon */
	__u32		spid;		/* SPID of trustlet, if relevant */
	__u32		service_type;	/* Type of trustlet being returned */
	__u32		length;		/* Length of data to get */
	/* Any data follows */
};

struct mc_admin_driver_info {
	/* Version, and something else..*/
	__u32		drv_version;
	__u32		initial_cmd_id;
};

struct mc_admin_load_info {
	__u32		 spid;		/* SPID of trustlet, if relevant */
	__u64		 address;	/* Address of the data */
	__u32		 length;	/* Length of data to get */
	struct mc_uuid_t uuid;		/* UUID of trustlet, if relevant */
};

#define MC_ADMIN_IO_GET_DRIVER_REQUEST \
	_IOR(MC_IOC_MAGIC, 0, struct mc_admin_request)
#define MC_ADMIN_IO_GET_INFO \
	_IOR(MC_IOC_MAGIC, 1, struct mc_admin_driver_info)
#define MC_ADMIN_IO_LOAD_DRIVER \
	_IOW(MC_IOC_MAGIC, 2, struct mc_admin_load_info)
#define MC_ADMIN_IO_LOAD_TOKEN \
	_IOW(MC_IOC_MAGIC, 3, struct mc_admin_load_info)
#define MC_ADMIN_IO_LOAD_CHECK \
	_IOW(MC_IOC_MAGIC, 4, struct mc_admin_load_info)
#define MC_ADMIN_IO_LOAD_KEY_SO \
	_IOW(MC_IOC_MAGIC, 5, struct mc_admin_load_info)

#ifdef __cplusplus
}
#endif
#endif /* __MC_ADMIN_IOCTL_H__ */
