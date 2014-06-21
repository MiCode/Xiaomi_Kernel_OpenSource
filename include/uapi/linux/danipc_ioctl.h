/*
	All files except if stated otherwise in the begining of the file
	are under the ISC license:
	----------------------------------------------------------------------

	Copyright (c) 2010-2012 Design Art Networks Ltd.

	Permission to use, copy, modify, and/or distribute this software for any
	purpose with or without fee is hereby granted, provided that the above
	copyright notice and this permission notice appear in all copies.

	THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
	WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
	MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
	ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
	WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
	ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
	OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/


#ifndef __UAPI_LINUX_DANIPC_IOCTL_H__
#define __UAPI_LINUX_DANIPC_IOCTL_H__

#include <linux/types.h>

#define DANIPC_IOCS_REGISTER	(SIOCDEVPRIVATE + 0)
#define DANIPC_IOCG_ADDR2NAME	(SIOCDEVPRIVATE + 1)
#define DANIPC_IOCG_NAME2ADDR	(SIOCDEVPRIVATE + 2)

#define MAX_AGENTS		256
#define MAX_AGENT_NAME		32

#define INVALID_ID		((unsigned)(-1))

struct danipc_reg {
	/* (IN) Agent name */
	char			name[MAX_AGENT_NAME];

	/* (IN) Actually this is priority from enum IPC_trns_priority */
	unsigned		prio;

	/* (IN) If different from INVALID_ID specifies preferred local ID. */
	unsigned		requested_lid;

	/* (OUT) Assigned local ID. */
	unsigned		assigned_lid;

	/* (OUT) Cookie in network format */
	uint16_t		cookie;
};


typedef uint8_t	__bitwise	danipc_addr_t;

struct danipc_name {
	char			name[MAX_AGENT_NAME];
	danipc_addr_t		addr;
};

#define DANIPC_MAX_BUF 1600
#endif /* __UAPI_LINUX_DANIPC_IOCTL_H__ */
