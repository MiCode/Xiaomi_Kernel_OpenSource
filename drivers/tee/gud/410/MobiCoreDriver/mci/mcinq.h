/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 TRUSTONIC LIMITED
 */

#ifndef NQ_H_
#define NQ_H_

/** \name NQ Size Defines
 * Minimum and maximum count of elements in the notification queue.
 */
#define MIN_NQ_ELEM 1	/** Minimum notification queue elements */
#define MAX_NQ_ELEM 64	/** Maximum notification queue elements */

/* Compute notification queue size in bytes from its number of elements */
#define NQ_SIZE(n)   (2 * (sizeof(struct notification_queue_header)\
			+ (n) * sizeof(struct notification)))

/** \name NQ Length Defines
 * Note that there is one queue for NWd->SWd and one queue for SWd->NWd
 */
/** Minimum size for the notification queue data structure */
#define MIN_NQ_LEN NQ_SIZE(MIN_NQ_ELEM)
/** Maximum size for the notification queue data structure */
#define MAX_NQ_LEN NQ_SIZE(MAX_NQ_ELEM)

/** \name Session ID Defines
 * Standard Session IDs.
 */
/** MCP session ID, used to communicate with MobiCore (e.g. to start/stop TA) */
#define SID_MCP       0
/** Invalid session id, returned in case of error */
#define SID_INVALID   0xffffffff

/** Notification data structure */
struct notification {
	u32	session_id;	/** Session ID */
	s32	payload;	/** Additional notification info */
};

/** Notification payload codes.
 * 0 indicated a plain simple notification,
 * a positive value is a termination reason from the task,
 * a negative value is a termination reason from MobiCore.
 * Possible negative values are given below.
 */
enum notification_payload {
	/** task terminated, but exit code is invalid */
	ERR_INVALID_EXIT_CODE = -1,
	/** task terminated due to session end, no exit code available */
	ERR_SESSION_CLOSE     = -2,
	/** task terminated due to invalid operation */
	ERR_INVALID_OPERATION = -3,
	/** session ID is unknown */
	ERR_INVALID_SID       = -4,
	/** session is not active */
	ERR_SID_NOT_ACTIVE    = -5,
	/** session was force-killed (due to an administrative command). */
	ERR_SESSION_KILLED    = -6,
};

/** Declaration of the notification queue header.
 * layout as specified in the data structure specification.
 */
struct notification_queue_header {
	u32	write_cnt;	/** Write counter */
	u32	read_cnt;	/** Read counter */
	u32	queue_size;	/** Queue size */
};

/** Queue struct which defines a queue object.
 * The queue struct is accessed by the queue<operation> type of
 * function. elementCnt must be a power of two and the power needs
 * to be smaller than power of u32 (obviously 32).
 */
struct notification_queue {
	struct notification_queue_header hdr;		/** Queue header */
	struct notification notification[MIN_NQ_ELEM];	/** Elements */
};

#endif /** NQ_H_ */
