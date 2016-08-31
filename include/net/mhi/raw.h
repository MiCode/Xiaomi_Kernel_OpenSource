/*
 * File: mhi/raw.h
 *
 * MHI RAW socket definitions
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

#ifndef MHI_RAW_H
#define MHI_RAW_H

#include <linux/types.h>
#include <linux/socket.h>

#include <net/sock.h>


extern int mhi_raw_sock_create(
	struct net *net,
	struct socket *sock,
	int proto,
	int kern);

extern int  mhi_raw_proto_init(void);
extern void mhi_raw_proto_exit(void);


#endif /* MHI_RAW_H */
