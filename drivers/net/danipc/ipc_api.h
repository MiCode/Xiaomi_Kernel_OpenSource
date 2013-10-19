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

/* Macro Build AgentId from Given CpuId and LocalId */
#define IPC_AGENT_ID(cpuid, lid)	\
		(((cpuid&(PLATFORM_MAX_NUM_OF_NODES-1)) << 4) + (0x0f & (lid)))
/* Build IPC_IDX from Given CpuId (Node ID) and LocalId */
#define IPC_GetNode(_id)		((_id)>>4)
#define IPC_LocalId(_id)		((_id)&0x0f)

#define IPC_BUF_SIZE_MAX	128		/* in bytes */
#define IPC_BUF_COUNT_MAX	128
#define IPC_MAX_MESSAGE_SIZE	(IPC_BUF_COUNT_MAX * IPC_BUF_SIZE_MAX)
#define IPC_BUF_SIZE_PER_NODE	(IPC_BUF_COUNT_MAX * IPC_BUF_SIZE_MAX)
#define IPC_FIFO_BUF_NUM_HIGH	(IPC_BUF_COUNT_MAX)
#define IPC_FIFO_BUF_NUM_LOW	(IPC_BUF_COUNT_MAX)

#define IPC_FIRST_BUF_DATA_SIZE_MAX	\
			(IPC_BUF_SIZE_MAX-sizeof(struct IPC_message_hdr))
#define IPC_NEXT_BUF_DATA_SIZE_MAX	\
			(IPC_BUF_SIZE_MAX-sizeof(struct IPC_buffer_hdr))

#define IPC_BUF_TYPE_FULL	0	/* Single message buffer */
#define IPC_BUF_TYPE_START	1	/* First buffer, more to come */
#define IPC_BUF_TYPE_END	2	/* Last buffer, no more */
#define IPC_BUF_TYPE_MID	3	/* Mid buffer, more to come */

#define IPC_BUF_TYPE_MTC	1	/* There are more buffers */
#define IPC_BUF_TYPE_BITS	3	/* mask for type bits  */
/* 'Clean' the type part from the next_buffer field (clear 2 LSB bits) */
#define IPC_NEXT_PTR_PART(_next) (char *)(((uint32_t)_next)&~IPC_BUF_TYPE_BITS)

#define MAX_AGENT_NAME_LEN	32

#ifndef MAX_AGENTS
#define MAX_AGENTS		256
#endif

#define MAX_LOCAL_AGENT		16

/* IPC result codes */
#define IPC_SUCCESS		0
#define IPC_GENERIC_ERROR	-1


/*****************************************************************************
 *                      Debug customization
 *****************************************************************************
 */
typedef void (*IPC_agent_cb_t)(void *);
typedef void (*IPC_forward_cb_t)(void *, unsigned int);


/*********************************
 *  Agent Structure (Temp)
 *********************************
*/
struct IPC_agent {
	char			agentName[MAX_AGENT_NAME_LEN];
	uint8_t			agentId;
	/* Receive CB pointer */
	IPC_agent_cb_t		cb;
	IPC_forward_cb_t	forward_cb;
};

/*********************************
 *  Message header flag field
 *********************************
*/
struct IPC_message_flag {
	uint8_t	reserved:6;
	uint8_t	reply:1;		/* Message is replay */
	uint8_t	request:1;		/* Message is a request */
};

/********************************************
 *  Message header
 *
 *  This is the header of the first buffer
 *
 ********************************************
*/
struct IPC_message_hdr {
	struct IPC_buffer_hdr	*nextBufPtr;	/* Points to the next buffer */
						/* if exists. 2 LSB are used */
						/* to identify buffer type */
						/* (full, start, end, mid) */
	uint8_t			requestSeqNum;	/* Request sequence number */
	uint8_t			replySeqNum;	/* Reply sequence number */
	uint16_t		msgLen;		/* Total message length */
						/* (Data part) */
	uint8_t			destAgentId;	/* Message destination */
	uint8_t			srcAgentId;	/* Message source */
	uint8_t			msgType;	/* Application message type */
	struct IPC_message_flag	flags;		/* Message flag (req/rep) */
	char			*ReplyMsgPtr;	/* Optional replay message */
						/* pointer */
} __packed;


/********************************************
 *  Buffer header
 *
 *  This is the header of the next buffer[s]
 *
 ********************************************
*/
struct IPC_buffer_hdr {
	struct IPC_buffer_hdr	*nextBufPtr;	/* Points to the next buffer */
						/* if exists. 2 LSB are used */
						/* to identify buffer type */
						/* (full, start, end, mid) */
};

/********************************************
 *  First Buffer structure
 *
 ********************************************
*/
struct IPC_first_buffer {
	struct IPC_message_hdr	msgHdr;
	char			body[IPC_FIRST_BUF_DATA_SIZE_MAX];
};

/********************************************
 *  Next Buffer[s] structure
 *
 ********************************************
*/
struct IPC_next_buffer {
	struct IPC_buffer_hdr	bufHdr;
	char			body[IPC_NEXT_BUF_DATA_SIZE_MAX];
};

/********************************************
 *  Priority (for transport)
 *
 ********************************************
*/
enum IPC_trns_priority {
	IPC_trns_prio_0,    /* Low */
	IPC_trns_prio_1,    /* .. */
	IPC_trns_prio_2,    /* .. */
	IPC_trns_prio_3,    /* High */
};

typedef char *(*IPC_trns_buf_alloc_t)(uint8_t dest_agent_id,
					enum IPC_trns_priority pri);
typedef void (*IPC_trns_buf_free_t)(char *buf, uint8_t dest_agent_id,
					enum IPC_trns_priority pri);
typedef int32_t (*IPC_trns_buf_send_t)(char *buf, uint8_t dest_agent_id,
					enum IPC_trns_priority pri);

/********************************************
 *  Transport layer utility function vector
 *
 *  The structure hold the transport function
 *  to be used per destination node (alloc, send)
 *  and source (free)
 *
 ********************************************
*/
struct IPC_transport_func {
	IPC_trns_buf_alloc_t	trns_buf_alloc_ptr;
	IPC_trns_buf_free_t	trns_buf_free_ptr;
	IPC_trns_buf_send_t	trns_buf_send_ptr;
};

struct agentNameEntry {
	char	agentName[MAX_AGENT_NAME_LEN];
};

/* Own Node number */
extern uint8_t	IPC_OwnNode;

extern struct IPC_transport_func IPC_eth2proxy_utils;


/* -----------------------------------------------------------
 * Function:	IPC_receive
 * Description:	Processing IPC messages
 * Input:	max_msg_count:		max number processed messages per call
 *		pri:			priority queue
 * Output:	None
 * -----------------------------------------------------------
 */
uint32_t IPC_receive(uint32_t max_msg_count, enum IPC_trns_priority pri);


/* ===========================================================================
 * IPC_buf_alloc
 * ===========================================================================
 * Description:	buffer allocation API, should be called before building
 *		new message
 *
 * Parameters:		dest_agent_id	- Message destination AgentId
 *			pri		- Transport priority level
 *
 *
 * Returns: Pointer to a 128 Byte buffer
 *
 */
char *IPC_buf_alloc(uint8_t dest_agent_id, enum IPC_trns_priority pri);

/* ===========================================================================
 * IPC_buf_free
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
int32_t IPC_buf_free(char *buf_first, enum IPC_trns_priority pri);


/* ===========================================================================
 * IPC_msg_alloc
 * ===========================================================================
 * Description:	Allocate message buffer[s] and set the type and length.
 *		Optionally copy message data into allocated buffers (if msgPtr
 *		is not NULL.
 *
 *
 * Parameters:		src_agent_id	- Message source AgentId
 *			dest_agent_id	- Message destination AgentId
 *			msgPtr		- Pointer to message data (optional)
 *			msgLen		- Message length
 *			msgtype		- Message type
 *			repPtr		- Pointer to allocated reply buffer
 *			pri		- Transport priority level
 *
 *
 * Returns: Pointer to the message first buffer
 *
 */
char *IPC_msg_alloc
(
	uint8_t			src_agent_id,
	uint8_t			dest_agent_id,
	char			*msgPtr,
	uint16_t		msgLen,
	uint8_t			msgType,
	char			*repPtr,
	enum IPC_trns_priority	pri
);

/* ===========================================================================
 * IPC_msg_send
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
int32_t IPC_msg_send(char *buf_first, enum IPC_trns_priority pri);

/* ===========================================================================
 * IPC_trns_eth_add
 * ===========================================================================
 * Description:  Adds Ehernet transport layer function vector
 *
 * Parameters: trn_vec_p - function vector
 *
 * Returns: n/a
 *
 */
void IPC_trns_eth_add(struct IPC_transport_func *trn_vec_p);

/* ==========================================================================
 * display_message
 * ===========================================================================
 * Description:  Debug function to display message buffer content
 *
 * Parameters:		buf_first	- First message buffer
 *
 * Returns: n/a
 *
 */
void display_message(char *buf_first);

/* ===========================================================================
 * display_header
 * ===========================================================================
 * Description:  Debug function to display message header content
 *
 * Parameters:		buf_first	- First message buffer
 *
 * Returns: n/a
 *
 */
void display_header(char *buf_first);

extern const struct IPC_transport_func IPC_fifo_utils;

#endif /*_IPC_API_H_*/
