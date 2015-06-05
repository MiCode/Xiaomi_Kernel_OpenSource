/*
 * Copyright (c) 2013-2015 TRUSTONIC LIMITED
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

#ifndef _MC_LINUX_H_
#define _MC_LINUX_H_

#define MCDRVMODULEAPI_VERSION_MAJOR 2
#define MCDRVMODULEAPI_VERSION_MINOR 1

#ifndef __KERNEL__
#include <stdint.h>
#endif

#define MC_USER_DEVNODE		"mobicore-user"

/** Maximum length of MobiCore product ID string. */
#define MC_PRODUCT_ID_LEN	64

/** Number of buffers that can be mapped at once */
#define MC_MAP_MAX		4

/*
 * Universally Unique Identifier (UUID) according to ISO/IEC 11578.
 */
struct mc_uuid_t {
	uint8_t		value[16];	/* Value of the UUID. */
};

/*
 * GP TA login types.
 */
enum mc_login_type {
	TEEC_LOGIN_PUBLIC = 0,
	TEEC_LOGIN_USER,
	TEEC_LOGIN_GROUP,
	TEEC_LOGIN_APPLICATION = 4,
	TEEC_LOGIN_USER_APPLICATION,
	TEEC_LOGIN_GROUP_APPLICATION,
};

/*
 * GP TA identity structure.
 */
struct mc_identity {
	enum mc_login_type	login_type;
	union {
		uint8_t		login_data[16];
		gid_t		gid;		/* Requested group id */
		struct {
			uid_t	euid;
			uid_t	ruid;
		} uid;
	};
};

/*
 * Data exchange structure of the MC_IO_OPEN_SESSION ioctl command.
 */
struct mc_ioctl_open_sess {
	struct mc_uuid_t uuid;		/* trustlet uuid */
	uint32_t	is_gp_uuid;	/* uuid is for GP TA */
	uint32_t        sid;            /* session id (out) */
	uint64_t	tci;		/* tci buffer pointer */
	uint32_t	tcilen;		/* tci length */
	struct mc_identity identity;	/* GP TA identity */
};

/*
 * Data exchange structure of the MC_IO_OPEN_TRUSTLET ioctl command.
 */
struct mc_ioctl_open_trustlet {
	uint32_t	sid;		/* session id (out) */
	uint32_t	spid;		/* trustlet spid */
	uint64_t	buffer;		/* trustlet binary pointer */
	uint32_t	tlen;		/* binary length  */
	uint64_t	tci;		/* tci buffer pointer */
	uint32_t	tcilen;		/* tci length */
};

/*
 * Data exchange structure of the MC_IO_WAIT ioctl command.
 */
struct mc_ioctl_wait {
	uint32_t	sid;		/* session id (in) */
	int32_t		timeout;	/* notification timeout */
};

/*
 * Data exchange structure of the MC_IO_ALLOC ioctl command.
 */
struct mc_ioctl_alloc {
	uint32_t	len;		/* buffer length  */
	uint32_t	handle;		/* user handle for the buffer (out) */
};

/*
 * Buffer mapping incoming and outgoing information.
 */
struct mc_ioctl_buffer {
	uint64_t	va;		/* user space address of buffer */
	uint32_t	len;		/* buffer length  */
	uint64_t	sva;		/* SWd virt address of buffer (out) */
};

/*
 * Data exchange structure of the MC_IO_MAP and MC_IO_UNMAP ioctl commands.
 */
struct mc_ioctl_map {
	uint32_t		sid;	/* session id */
	struct mc_ioctl_buffer	bufs[MC_MAP_MAX];/* buffers info */
};

/*
 * Data exchange structure of the MC_IO_ERR ioctl command.
 */
struct mc_ioctl_geterr {
	uint32_t	sid;		/* session id */
	int32_t		value;		/* error value (out) */
};

/*
 * Global MobiCore Version Information.
 */
struct mc_version_info {
	char product_id[MC_PRODUCT_ID_LEN];	/** Product ID string */
	uint32_t version_mci;		/** Mobicore Control Interface */
	uint32_t version_so;		/** Secure Objects */
	uint32_t version_mclf;		/** MobiCore Load Format */
	uint32_t version_container;	/** MobiCore Container Format */
	uint32_t version_mc_config;	/** MobiCore Config. Block Format */
	uint32_t version_tl_api;	/** MobiCore Trustlet API */
	uint32_t version_dr_api;	/** MobiCore Driver API */
	uint32_t version_cmp;		/** Content Management Protocol */
};

/*
 * defines for the ioctl mobicore driver module function call from user space.
 */
/* MobiCore IOCTL magic number */
#define MC_IOC_MAGIC	'M'

/*
 * Implement corresponding functions from user api
 */
#define MC_IO_OPEN_SESSION	\
	_IOWR(MC_IOC_MAGIC, 0, struct mc_ioctl_open_sess)
#define MC_IO_OPEN_TRUSTLET	\
	_IOWR(MC_IOC_MAGIC, 1, struct mc_ioctl_open_trustlet)
#define MC_IO_CLOSE_SESSION	_IO(MC_IOC_MAGIC, 2)
#define MC_IO_NOTIFY		_IO(MC_IOC_MAGIC, 3)
#define MC_IO_WAIT		_IOW(MC_IOC_MAGIC, 4, struct mc_ioctl_wait)
#define MC_IO_MAP		_IOWR(MC_IOC_MAGIC, 5, struct mc_ioctl_map)
#define MC_IO_UNMAP		_IOW(MC_IOC_MAGIC, 6, struct mc_ioctl_map)
#define MC_IO_ERR		_IOWR(MC_IOC_MAGIC, 7, struct mc_ioctl_geterr)
#define MC_IO_FREEZE		_IO(MC_IOC_MAGIC, 8)
#define MC_IO_VERSION		_IOR(MC_IOC_MAGIC, 9, struct mc_version_info)
#define MC_IO_DR_VERSION	_IOR(MC_IOC_MAGIC, 10, uint32_t)

#endif /* _MC_LINUX_H_ */
