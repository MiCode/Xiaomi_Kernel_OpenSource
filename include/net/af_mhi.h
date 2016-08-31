/*
 * File: net/af_mhi.h
 *
 * MHI Protocol Family kernel definitions
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

#ifndef __LINUX_NET_AFMHI_H
#define __LINUX_NET_AFMHI_H

#include <linux/types.h>
#include <linux/socket.h>

#include <net/sock.h>


extern int mhi_register_protocol(int protocol);
extern int mhi_unregister_protocol(int protocol);
extern int mhi_protocol_registered(int protocol);

extern int mhi_skb_send(struct sk_buff *skb, struct net_device *dev, u8 proto);


#endif /* __LINUX_NET_AFMHI_H */
