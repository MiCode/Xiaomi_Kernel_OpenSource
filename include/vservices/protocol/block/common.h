
/*
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#if !defined(__VSERVICES_BLOCK_PROTOCOL_H__)
#define __VSERVICES_BLOCK_PROTOCOL_H__

#define VSERVICE_BLOCK_PROTOCOL_NAME "com.ok-labs.block"
typedef enum {
	VSERVICE_BLOCK_BASE_REQ_OPEN,
	VSERVICE_BLOCK_BASE_ACK_OPEN,
	VSERVICE_BLOCK_BASE_NACK_OPEN,
	VSERVICE_BLOCK_BASE_REQ_CLOSE,
	VSERVICE_BLOCK_BASE_ACK_CLOSE,
	VSERVICE_BLOCK_BASE_NACK_CLOSE,
	VSERVICE_BLOCK_BASE_REQ_REOPEN,
	VSERVICE_BLOCK_BASE_ACK_REOPEN,
	VSERVICE_BLOCK_BASE_NACK_REOPEN,
	VSERVICE_BLOCK_BASE_MSG_RESET,
	VSERVICE_BLOCK_IO_REQ_READ,
	VSERVICE_BLOCK_IO_ACK_READ,
	VSERVICE_BLOCK_IO_NACK_READ,
	VSERVICE_BLOCK_IO_REQ_WRITE,
	VSERVICE_BLOCK_IO_ACK_WRITE,
	VSERVICE_BLOCK_IO_NACK_WRITE,
} vservice_block_message_id_t;
typedef enum {
	VSERVICE_BLOCK_NBIT_IN__COUNT
} vservice_block_nbit_in_t;

typedef enum {
	VSERVICE_BLOCK_NBIT_OUT__COUNT
} vservice_block_nbit_out_t;

/* Notification mask macros */
#endif				/* ! __VSERVICES_BLOCK_PROTOCOL_H__ */
