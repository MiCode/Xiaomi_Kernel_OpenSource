/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _SFP_CONNTRACK_H
#define _SFP_CONNTRACK_H

/*
 * Definitions and Declarations for tuple.
 * Derived from include/net/netfiter/nf_conntrack_tuple.h
 */

union sfp_inet_addr {
	__u32		all[4];
	__be32		ip;
	__be32		ip6[4];
	struct in_addr	in;
	struct in6_addr	in6;
};

union sfp_conntrack_man_proto {
	/* Add other protocols here. */
	__be16 all;

	struct {
		__be16 port;
	} tcp;
	struct {
		__be16 port;
	} udp;
};

/* The manipulable part of the tuple. */
struct sfp_conntrack_man {
	union sfp_inet_addr u3;
	union sfp_conntrack_man_proto u;
	/* Layer 3 protocol */
	u16 l3num;
};

/* This contains the information to distinguish a connection. */
struct sfp_conntrack_tuple {
	struct sfp_conntrack_man src;

	/* These are the parts of the tuple which are fixed. */
	struct {
		union nf_inet_addr u3;
		union {
			/* Add other protocols here. */
			__be16 all;

			struct {
				__be16 port;
			} tcp;
			struct {
				__be16 port;
			} udp;
		} u;

		/* The protocol. */
		u8 protonum;
		/* The direction (for tuplehash) */
		u8 dir;
	} dst;
};
#endif
