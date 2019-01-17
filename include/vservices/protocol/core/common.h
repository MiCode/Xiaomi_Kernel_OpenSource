
/*
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#if !defined(__VSERVICES_CORE_PROTOCOL_H__)
#define __VSERVICES_CORE_PROTOCOL_H__

#define VSERVICE_CORE_PROTOCOL_NAME "com.ok-labs.core"
typedef enum {
	VSERVICE_CORE_CORE_REQ_CONNECT,
	VSERVICE_CORE_CORE_ACK_CONNECT,
	VSERVICE_CORE_CORE_NACK_CONNECT,
	VSERVICE_CORE_CORE_REQ_DISCONNECT,
	VSERVICE_CORE_CORE_ACK_DISCONNECT,
	VSERVICE_CORE_CORE_NACK_DISCONNECT,
	VSERVICE_CORE_CORE_MSG_STARTUP,
	VSERVICE_CORE_CORE_MSG_SHUTDOWN,
	VSERVICE_CORE_CORE_MSG_SERVICE_CREATED,
	VSERVICE_CORE_CORE_MSG_SERVICE_REMOVED,
	VSERVICE_CORE_CORE_MSG_SERVER_READY,
	VSERVICE_CORE_CORE_MSG_SERVICE_RESET,
} vservice_core_message_id_t;
typedef enum {
	VSERVICE_CORE_NBIT_IN__COUNT
} vservice_core_nbit_in_t;

typedef enum {
	VSERVICE_CORE_NBIT_OUT__COUNT
} vservice_core_nbit_out_t;

/* Notification mask macros */
#endif				/* ! __VSERVICES_CORE_PROTOCOL_H__ */
