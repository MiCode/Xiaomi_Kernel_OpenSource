/*
 * include/vservices/types.h
 *
 * Copyright (c) 2012-2018 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _VSERVICE_TYPES_H
#define _VSERVICE_TYPES_H

#include <linux/types.h>

typedef u16 vs_service_id_t;
typedef u16 vs_message_id_t;

/*
 * An opaque handle to a queued asynchronous command. This is used internally
 * by the generated interface code, to identify which of the pending commands
 * is being replied to. It is provided as a parameter to non-blocking handler
 * callbacks for queued asynchronous requests, and must be stored by the server
 * and passed to the corresponding reply call.
 */
typedef struct vservice_queued_request vservice_queued_request_t;

/*
 * Following enum is to be used by server for informing about successful or
 * unsuccessful open callback by using VS_SERVER_RESP_SUCCESS or
 * VS_SERVER_RESP_FAILURE resepectively. Server can choose to complete request
 * explicitely in this case it should return VS_SERVER_RESP_EXPLICIT_COMPLETE.
 */
typedef enum vs_server_response_type {
	VS_SERVER_RESP_SUCCESS,
	VS_SERVER_RESP_FAILURE,
	VS_SERVER_RESP_EXPLICIT_COMPLETE
} vs_server_response_type_t;

#endif /*_VSERVICE_TYPES_H */
