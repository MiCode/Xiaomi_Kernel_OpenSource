/*
 * file mhi.h
 *
 * Modem-Host Interface (MHI) kernel interface
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef LINUX_MHI_H
#define LINUX_MHI_H

#include <linux/types.h>
#include <linux/socket.h>
#include <net/sock.h>
#include <asm/byteorder.h>


struct mhi_sock {
	struct sock	sk;
	int		sk_l3proto;
	int		sk_ifindex;
};

struct sockaddr_mhi {
	sa_family_t	sa_family;
	int		sa_ifindex;
	__u8		sa_zero[sizeof(struct sockaddr)
				- sizeof(sa_family_t) - sizeof(int)];
};


static inline struct mhi_sock *mhi_sk(struct sock *sk)
{
	return (struct mhi_sock *)sk;
}

static inline struct sockaddr_mhi *sa_mhi(struct sockaddr *sa)
{
	return (struct sockaddr_mhi *)sa;
}

#endif
