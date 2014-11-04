/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2014 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef __SOCKET_GLOBAL_H_INCLUDED__
#define __SOCKET_GLOBAL_H_INCLUDED__

#include "stream_buffer.h"

/* define the socket port direction */
typedef enum {
	SOCKET_PORT_DIRECTION_NULL,
	SOCKET_PORT_DIRECTION_IN,
	SOCKET_PORT_DIRECTION_OUT
} socket_port_direction_t;

/* pointer to the port's callout function */
typedef void (*socket_port_callout_fp)(void);
typedef struct socket_port_s socket_port_t;
typedef struct socket_s socket_t;

/* data structure of the socket port */
struct socket_port_s {
	unsigned				channel;	/* the port entity */
	socket_port_direction_t direction;	/* the port direction */
	socket_port_callout_fp	callout;	/* the port callout function */

	socket_t				*socket;	/* point to the socket */

	struct {
		unsigned data;
	} buf;								/* the buffer at the port */
};

/* data structure of the socket */
struct socket_s {
	socket_port_t	*in;	/* the in-direction port */
	socket_port_t	*out;	/* the out-direction port */
	stream_buffer_t	buf;	/* the buffer between in-ports and out-ports */
};

#endif /* __SOCKET_GLOBAL_H_INCLUDED__ */

