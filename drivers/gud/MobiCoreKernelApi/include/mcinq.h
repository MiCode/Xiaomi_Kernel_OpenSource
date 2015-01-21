/*
 * Copyright (c) 2013 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
/*
 * Notifications inform the MobiCore runtime environment that information is
 * pending in a WSM buffer.
 *
 * The Trustlet Connector (TLC) and the corresponding Trustlet also utilize
 * this buffer to notify each other about new data within the
 * Trustlet Connector Interface (TCI).
 *
 * The buffer is set up as a queue, which means that more than one
 * notification can be written to the buffer before the switch to the other
 * world is performed. Each side therefore facilitates an incoming and an
 * outgoing queue for communication with the other side.
 *
 * Notifications hold the session ID, which is used to reference the
 * communication partner in the other world.
 * So if, e.g., the TLC in the normal world wants to notify his Trustlet
 * about new data in the TLC buffer
 *
 * Notification queue declarations.
 */
#ifndef _MCINQ_H_
#define _MCINQ_H_

/* Minimum and maximum count of elements in the notification queue */
#define MIN_NQ_ELEM	1	/* Minimum notification queue elements. */
#define MAX_NQ_ELEM	64	/* Maximum notification queue elements. */

/* Minimum notification length (in bytes). */
#define MIN_NQ_LEN	(MIN_NQ_ELEM * sizeof(notification))

/* Maximum notification length (in bytes). */
#define MAX_NQ_LEN	(MAX_NQ_ELEM * sizeof(notification))

/*
 * MCP session ID is used when directly communicating with the MobiCore
 * (e.g. for starting and stopping of Trustlets).
 */
#define SID_MCP		0
/* Invalid session id is returned in case of an error. */
#define SID_INVALID	0xffffffff

/* Notification data structure. */
struct notification {
	uint32_t	session_id;	/* Session ID. */
	int32_t		payload;	/* Additional notification info */
};

/*
 * Notification payload codes.
 * 0 indicated a plain simple notification,
 * a positive value is a termination reason from the task,
 * a negative value is a termination reason from MobiCore.
 * Possible negative values are given below.
 */
enum notification_payload {
	/* task terminated, but exit code is invalid */
	ERR_INVALID_EXIT_CODE	= -1,
	/* task terminated due to session end, no exit code available */
	ERR_SESSION_CLOSE	= -2,
	/* task terminated due to invalid operation */
	ERR_INVALID_OPERATION	= -3,
	/* session ID is unknown */
	ERR_INVALID_SID		= -4,
	/*  session is not active */
	ERR_SID_NOT_ACTIVE	= -5
};

/*
 * Declaration of the notification queue header.
 * Layout as specified in the data structure specification.
 */
struct notification_queue_header {
	uint32_t	write_cnt;	/* Write counter. */
	uint32_t	read_cnt;	/* Read counter. */
	uint32_t	queue_size;	/* Queue size. */
};

/*
 * Queue struct which defines a queue object.
 * The queue struct is accessed by the queue<operation> type of
 * function. elementCnt must be a power of two and the power needs
 * to be smaller than power of uint32_t (obviously 32).
 */
struct notification_queue {
	/* Queue header. */
	struct notification_queue_header hdr;
	/* Notification elements. */
	struct notification notification[MIN_NQ_ELEM];
};

#endif /* _MCINQ_H_ */
