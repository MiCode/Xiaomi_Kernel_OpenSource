/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2019 aQuantia Corporation. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#ifndef _ATLFWD_CTX_H_
#define _ATLFWD_CTX_H_

#include "libmnl/libmnl.h"
#include "linux/genetlink.h"

#include "atl_fwdnl.h"

struct nl_context {
	struct mnl_socket *sock;
	unsigned int port;
	unsigned int seq;
	char *msgbuf;
	unsigned int msgbufsize;
	struct nlmsghdr *nlhdr;
	struct genlmsghdr *gnlhdr;
	int family_id;
	char *devname;
	int verbose;
};

#endif
