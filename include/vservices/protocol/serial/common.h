
/*
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#if !defined(__VSERVICES_SERIAL_PROTOCOL_H__)
#define __VSERVICES_SERIAL_PROTOCOL_H__

#define VSERVICE_SERIAL_PROTOCOL_NAME "com.ok-labs.serial"
typedef enum {
	VSERVICE_SERIAL_BASE_REQ_OPEN,
	VSERVICE_SERIAL_BASE_ACK_OPEN,
	VSERVICE_SERIAL_BASE_NACK_OPEN,
	VSERVICE_SERIAL_BASE_REQ_CLOSE,
	VSERVICE_SERIAL_BASE_ACK_CLOSE,
	VSERVICE_SERIAL_BASE_NACK_CLOSE,
	VSERVICE_SERIAL_BASE_REQ_REOPEN,
	VSERVICE_SERIAL_BASE_ACK_REOPEN,
	VSERVICE_SERIAL_BASE_NACK_REOPEN,
	VSERVICE_SERIAL_BASE_MSG_RESET,
	VSERVICE_SERIAL_SERIAL_MSG_MSG,
} vservice_serial_message_id_t;
typedef enum {
	VSERVICE_SERIAL_NBIT_IN__COUNT
} vservice_serial_nbit_in_t;

typedef enum {
	VSERVICE_SERIAL_NBIT_OUT__COUNT
} vservice_serial_nbit_out_t;

/* Notification mask macros */
#endif				/* ! __VSERVICES_SERIAL_PROTOCOL_H__ */
