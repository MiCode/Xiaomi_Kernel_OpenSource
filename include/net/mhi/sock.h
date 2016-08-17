/*
 * File: mhi/sock.h
 *
 * MHI socket definitions
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

#ifndef MHI_SOCK_H
#define MHI_SOCK_H

#include <linux/types.h>
#include <linux/socket.h>

#include <net/sock.h>

extern const struct proto_ops mhi_socket_ops;

extern int  mhi_sock_rcv_unicast(struct sk_buff *skb, u8 l3prot, u32 l3len);
extern int  mhi_sock_rcv_multicast(struct sk_buff *skb, u8 l3prot, u32 l3len);

extern void mhi_sock_hash(struct sock *sk);
extern void mhi_sock_unhash(struct sock *sk);

extern int  mhi_sock_init(void);
extern void mhi_sock_exit(void);


#endif /* MHI_SOCK_H */
