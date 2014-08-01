/*
	All files except if stated otherwise in the begining of the file
	are under the ISC license:
	----------------------------------------------------------------------

	Copyright (c) 2010-2012 Design Art Networks Ltd.

	Permission to use, copy, modify, and/or distribute this software for any
	purpose with or without fee is hereby granted, provided that the above
	copyright notice and this permission notice appear in all copies.

	THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
	WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
	MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
	ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
	WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
	ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
	OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/



#ifndef _IPC_API_H_
#define _IPC_API_H_

/*****************************************************************************
 *                      MACROS
 *****************************************************************************
 */

/* Build AgentId from given CpuId and LocalId */
#define IPC_AGENT_ID(cpuid, lid)	\
		(((cpuid&(PLATFORM_MAX_NUM_OF_NODES-1)) << 4) + (0x0f & (lid)))

#define ipc_get_node(id)	((id) >> 4)
#define ipc_lid(id)		((id) & 0x0f)

#define IPC_BUF_SIZE_MAX	1600		/* in bytes */
#define IPC_BUF_COUNT_MAX	128
#define IPC_MAX_MESSAGE_SIZE	(IPC_BUF_COUNT_MAX * IPC_BUF_SIZE_MAX)
#define IPC_BUF_SIZE_PER_NODE	(IPC_BUF_COUNT_MAX * IPC_BUF_SIZE_MAX)
#define IPC_FIFO_BUF_NUM_HIGH	(IPC_BUF_COUNT_MAX)
#define IPC_FIFO_BUF_NUM_LOW	(IPC_BUF_COUNT_MAX)

#define IPC_FIRST_BUF_DATA_SIZE_MAX	\
			(IPC_BUF_SIZE_MAX - sizeof(struct ipc_msg_hdr))
#define IPC_NEXT_BUF_DATA_SIZE_MAX	\
			(IPC_BUF_SIZE_MAX - sizeof(struct ipc_buf_hdr))

#define IPC_BUF_TYPE_FULL	0	/* Single message buffer */
#define IPC_BUF_TYPE_START	1	/* First buffer, more to come */
#define IPC_BUF_TYPE_END	2	/* Last buffer, no more */
#define IPC_BUF_TYPE_MID	3	/* Mid buffer, more to come */

#define IPC_BUF_TYPE_MTC	1	/* There are more buffers */
#define IPC_BUF_TYPE_BITS	3	/* mask for type bits  */
/* 'Clean' the type part from the next_buffer field (clear 2 LSB bits) */
#define IPC_NEXT_PTR_PART(next) (char *)(((uint32_t)next) & ~IPC_BUF_TYPE_BITS)

#define MAX_AGENT_NAME_LEN	32

#ifndef MAX_AGENTS
#define MAX_AGENTS		256
#endif

#define MAX_LOCAL_AGENT		16

/* IPC result codes */
#define IPC_SUCCESS		0
#define IPC_GENERIC_ERROR	-1


/*********************************
 *  Message header flag field
 *********************************
*/
typedef struct ipc_msg_flags __bitwise {
	uint8_t	reserved:6;
	uint8_t	reply:1;		/* Message is replay */
	uint8_t	request:1;		/* Message is a request */
} ipc_msg_flags_t;

/********************************************
 *  Message header
 *
 *  This is the header of the first buffer
 *
 ********************************************
*/
struct ipc_msg_hdr {
	struct ipc_buf_hdr	*next;		/* Points to the next buffer */
						/* if exists. 2 LSB are used */
						/* to identify buffer type */
						/* (full, start, end, mid) */
	uint8_t			request_num;	/* Request sequence number */
	uint8_t			reply_num;	/* Reply sequence number */
	uint16_t		msg_len;	/* Total message length */
						/* (Data part) */
	uint8_t			dest_aid;	/* Message destination */
	uint8_t			src_aid;	/* Message source */
	uint8_t			msg_type;	/* Application message type */
	ipc_msg_flags_t		flags;		/* Message flag (req/rep) */
	char			*reply;		/* Optional replay message */
						/* pointer */
} __packed;


/********************************************
 *  Buffer header
 *
 *  This is the header of the next buffer[s]
 *
 ********************************************
*/
struct ipc_buf_hdr {
	struct ipc_buf_hdr	*next;		/* Points to the next buffer */
						/* if exists. 2 LSB are used */
						/* to identify buffer type */
						/* (full, start, end, mid) */
};

/********************************************
 *  First Buffer structure
 *
 ********************************************
*/
struct ipc_first_buf {
	struct ipc_msg_hdr	msg_hdr;
	char			body[IPC_FIRST_BUF_DATA_SIZE_MAX];
};

/********************************************
 *  Next Buffer[s] structure
 *
 ********************************************
*/
struct ipc_next_buf {
	struct ipc_buf_hdr	buf_hdr;
	char			body[IPC_NEXT_BUF_DATA_SIZE_MAX];
};

/********************************************
 *  Priority (for transport)
 *
 ********************************************
*/
enum ipc_trns_prio {
	ipc_trns_prio_0,	/* Low */
	ipc_trns_prio_1,	/* .. */
	ipc_trns_prio_2,	/* .. */
	ipc_trns_prio_3,	/* High */
};

typedef char *(*ipc_trns_alloc_t)(uint8_t dest_aid, enum ipc_trns_prio pri);
typedef void (*ipc_trns_free_t)(char *buf, uint8_t dest_aid,
					enum ipc_trns_prio pri);
typedef int32_t (*ipc_trns_send_t)(char *buf, uint8_t dest_aid,
					enum ipc_trns_prio pri);

/********************************************
 *  Transport layer utility function vector
 *
 *  The structure hold the transport function
 *  to be used per destination node (alloc, send)
 *  and source (free)
 *
 ********************************************
*/
struct ipc_trns_func {
	ipc_trns_alloc_t	trns_alloc;
	ipc_trns_free_t		trns_free;
	ipc_trns_send_t		trns_send;
};

struct agent_entry {
	char	name[MAX_AGENT_NAME_LEN];
};

/* Own Node number */
extern uint8_t	ipc_own_node;


/* -----------------------------------------------------------
 * Function:	ipc_recv
 * Description:	Processing IPC messages
 * Input:	max_msg_count:		max number processed messages per call
 *		pri:			priority queue
 * Output:	None
 * -----------------------------------------------------------
 */
uint32_t ipc_recv(uint32_t max_msg_count, enum ipc_trns_prio pri);


/* ===========================================================================
 * ipc_buf_alloc
 * ===========================================================================
 * Description:	buffer allocation API, should be called before building
 *		new message
 *
 * Parameters:		dest_aid	- Message destination AgentId
 *			pri		- Transport priority level
 *
 *
 * Returns: Pointer to a 128 Byte buffer
 *
 */
char *ipc_buf_alloc(uint8_t dest_aid, enum ipc_trns_prio pri);

/* ===========================================================================
 * ipc_buf_free
 * ===========================================================================
 * Description:	Free the buffer, could be called on IPC message receiving node
 *		or on sending node when need to free previously allocated
 *		buffers
 *
 * Parameters:		buf_first	- Pointer to first message buffer
 *			pri		- Transport priority level
 *
 *
 * Returns: Result code
 *
 */
int32_t ipc_buf_free(char *buf_first, enum ipc_trns_prio pri);


/* ===========================================================================
 * ipc_msg_alloc
 * ===========================================================================
 * Description:	Allocate message buffer[s] and set the type and length.
 *		Optionally copy message data into allocated buffers (if msg
 *		is not NULL.
 *
 *
 * Parameters:		src_aid		- Message source AgentId
 *			dest_aid	- Message destination AgentId
 *			msg		- Pointer to message data (optional)
 *			msg_len		- Message length
 *			msg_type	- Message type
 *			pri		- Transport priority level
 *
 *
 * Returns: Pointer to the message first buffer
 *
 */
char *ipc_msg_alloc
(
	uint8_t			src_aid,
	uint8_t			dest_aid,
	char			*msg,
	size_t			msg_len,
	uint8_t			msg_type,
	enum ipc_trns_prio	pri
);

/* ===========================================================================
 * ipc_msg_send
 * ===========================================================================
 * Description:	Message send, first buffer of the message should be provided,
 *
 * Parameters:		buf_first	- Pointer to the first message buffer
 *			pri		- Transport priority level
 *
 *
 * Returns: Result code
 *
 */
int32_t ipc_msg_send(char *buf_first, enum ipc_trns_prio pri);


extern const struct ipc_trns_func ipc_fifo_utils;

#endif /*_IPC_API_H_*/
