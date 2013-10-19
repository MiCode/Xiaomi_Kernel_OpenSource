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


#include <linux/string.h>

#include "ipc_reg.h"
#include "ipc_api.h"

#include "danipc_lowlevel.h"


/* -----------------------------------------------------------
 * MACRO (define) section
 * -----------------------------------------------------------
 */
/* max. number of local agents per one Node */
#define MAX_LOCAL_ID     (MAX_LOCAL_AGENT-1)

uint8_t	ipc_localId;	/* Maintained the next available Local Id */
			/* for Agent registration */
uint8_t   ipc_reqSeqNume;	/* Maintain node related sequence number */

uint8_t		IPC_OwnNode;


/* ===========================================================================
 * IPC_appl_init
 * ===========================================================================
 * Description:	This function initializes software/HW during startup
 *
 * Parameters:		defTranVecPtr	- pointer to default transport layer
 *					  function vector
 *
 * Returns: n/a
 *
 */
static void IPC_appl_init(struct IPC_transport_func const *defTranVecPtr)
{
	IPC_OwnNode = IPC_getOwnNode();
	IPC_agent_table_clean();

	ipc_localId = 0;
	ipc_reqSeqNume = 0;

	IPC_trns_fifo_buff_init(IPC_OwnNode); /* add macro GET_CPUID */

	/* Initialize IPC routing table (of CPU#) */
	IPC_routeTableInit(defTranVecPtr);

#if defined(IPC_USE_TRACE)
	IPC_trace_client_id = TRACE_get_client_id((char *)"IPC");
	TRACE_register_client(IPC_trace_client_id, TRACE_TABLE(IPC),
				IPC_TRACE_BUF_SIZE, IPC_trace_rec_buf);
	pr_dbg("TRACE client registered IPC_trace_client_id %d\n",
		IPC_trace_client_id);
#endif
}


/* -----------------------------------------------------------
 * Function:	IPC_init
 * Description:	Called during node boot (initialization), assumes
 *		non-interruptible execution, means that interrupts are disabled
 *		and multi-thread support is not switched on yet
 *		(if present at all on the node)
 * Input:	mem_start:	pointer to the start of memory block that
 *		mem_size:	size of the provided memory block
 *				module can use
 * Output:			Number of bytes allocated during
 *				the initialization all the allocations
 * -----------------------------------------------------------
 */
unsigned IPC_init(void)
{
	IPC_agent_table_clean();
	IPC_appl_init(&IPC_fifo_utils);
	return 0;
}



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
char *IPC_buf_alloc(
	uint8_t				dest_agent_id,
	enum IPC_trns_priority		pri
)
{
	uint8_t			cpuId;
	char			*ptr = NULL;
	struct IPC_transport_func const *funcPtr;
	IPC_trns_buf_alloc_t	allocFuncPtr;

	/* Allocate buffer of 128 Bytes using the allocation function */
	/* associated with the given destination agentId */
	cpuId = IPC_GetNode(dest_agent_id);

	funcPtr = (void *)IPC_getUtilFuncVector(cpuId);
	if (likely(funcPtr)) {
		allocFuncPtr = funcPtr->trns_buf_alloc_ptr;
		if (likely(allocFuncPtr)) {
			ptr = allocFuncPtr(dest_agent_id, pri);

			/* Clear the 'Next buffer' filled */
			if (likely(ptr))
				((struct IPC_buffer_hdr *)ptr)->
							nextBufPtr = 0;
		}
	}

	return ptr;
}

/* ===========================================================================
 * IPC_buf_free
 * ===========================================================================
 * Description:  Free the buffer, could be called on IPC message receiving node
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
int32_t IPC_buf_free(char *buf_first, enum IPC_trns_priority pri)
{
	struct IPC_buffer_hdr		*curPtr;
	struct IPC_buffer_hdr		*nxtPtr;
	struct IPC_transport_func const	*trns_ptr;
	IPC_trns_buf_free_t		freeFunc;
	uint8_t				destAgentId;
	uint8_t				cpuId;
	int32_t				res = IPC_GENERIC_ERROR;

	if (likely(buf_first)) {
		destAgentId  = (((struct IPC_first_buffer *)buf_first)->
							msgHdr).destAgentId;
		curPtr = (struct IPC_buffer_hdr *)buf_first;
		cpuId = IPC_GetNode(destAgentId);
		trns_ptr = IPC_getUtilFuncVector(cpuId);
		if (likely(trns_ptr)) {
			freeFunc = trns_ptr->trns_buf_free_ptr;
			if (likely(freeFunc)) {
				/* Now loop all allocated buffers and free them.
				 * Last buffer is either a single (type = 0)
				 * or the buffer marked as the last (type = 2)
				 * all other buffers have their LSB set
				 * (type = 1 or 3).
				 */
				do {
					nxtPtr = ((struct IPC_message_hdr *)
					 IPC_NEXT_PTR_PART(curPtr))->nextBufPtr;
					(freeFunc)(IPC_NEXT_PTR_PART(curPtr),
							destAgentId, pri);
					curPtr = nxtPtr;
				} while (((uint32_t)curPtr & IPC_BUF_TYPE_MTC));
				res = IPC_SUCCESS;
			}
		}
	}
	return res;
}

/* ===========================================================================
 * IPC_buf_link
 * ===========================================================================
 * Description:	Link two buffers, should be called when message does not fit
 *		the single buffer
 *
 * Parameters:		buf_prev	- Pointer to a message buffer
 *			buf_next	- Pointer to the next message buffer
 *					  (to be linked to)
 *
 * Returns: Result code
 *
 */
static int32_t IPC_buf_link(char *buf_prev, char *buf_next)
{
	if (buf_prev == NULL || buf_next == NULL)
		return IPC_GENERIC_ERROR;

	/* Set the next buffer pointer in place */
	*(uint32_t *)buf_prev |= (uint32_t)buf_next & ~IPC_BUF_TYPE_BITS;
	/* Set the LSB of the prev buffer to signal there are more to come */
	*(uint32_t *)buf_prev |= IPC_BUF_TYPE_MTC;
	/* Mark the next buffer as the last one */
	*(uint32_t *)buf_next |= IPC_BUF_TYPE_END;
	return IPC_SUCCESS;
}

/* ===========================================================================
 * IPC_msg_set_len
 * ===========================================================================
 * Description:	sets message length, first buffer of the message
 *		should be provided
 *
 * Parameters:		buf_first	- Pointer to the first message buffer
 *			len		- Message length (bytes)
 *
 *
 * Returns: Result code
 *
 */
static int32_t IPC_msg_set_len(char *buf_first, uint16_t len)
{
	if (buf_first == NULL)
		return IPC_GENERIC_ERROR;
	(((struct IPC_first_buffer *)buf_first)->msgHdr).msgLen = len;
	return IPC_SUCCESS;
}

/* ===========================================================================
 * IPC_msg_set_type
 * ===========================================================================
 * Description:  sets message type, first buffer of the message
 *		should be provided
 *
 * Parameters:		buf_first	- Pointer to the first message buffer
 *			type		- Message type
 *
 *
 * Returns: Result code
 *
 */
static int32_t IPC_msg_set_type(char *buf_first, uint8_t type)
{
	if (buf_first == NULL)
		return IPC_GENERIC_ERROR;
	(((struct IPC_first_buffer *)buf_first)->msgHdr).msgType = type;
	return IPC_SUCCESS;
}

/* ===========================================================================
 * IPC_msg_set_reply_ptr
 * ===========================================================================
 * Description:  sets message reply buffer pointer
 *
 * Parameters:		buf_first	- Pointer to the first message buffer
 *			buf_rep		- Pointer to the expected replay message
 *
 *
 * Returns: Result code
 *
 */
static int32_t IPC_msg_set_reply_ptr(
	char			*buf_first,
	char			*buf_rep
)
{
	if (buf_first == NULL)
		return IPC_GENERIC_ERROR;
	(((struct IPC_first_buffer *)buf_first)->msgHdr).ReplyMsgPtr = buf_rep;
	return IPC_SUCCESS;
}


/* ===========================================================================
 * IPC_msg_alloc
 * ===========================================================================
 * Description:  Allocate message buffer[s] and set the type and length.
 *		Copy message data into allocated buffers.
 *
 *
 * Parameters:		src_agent_id	- Message source AgentId
 *			dest_agent_id	- Message destination AgentId
 *			msgPtr		- Pointer to message data
 *			msgLen		- Message length
 *			msgtype		- Message type
 *			repPtr		- Pointer to allocated reply buffer
 *			pri		- Transport priority level
 *
 *
 * Returns: Pointer to the message first buffer
 *
 */
char *IPC_msg_alloc(
	uint8_t			src_agent_id,
	uint8_t			dest_agent_id,
	char			*msgPtr,
	uint16_t		msgLen,
	uint8_t			msgType,
	char			*repPtr,
	enum IPC_trns_priority	pri
)
{
	char	*firstPtr = NULL;
	char	*prevPtr = NULL;
	char	*nextPtr = NULL;
	uint16_t	numOfNextBufs = 0;
	uint16_t	BufsCnt, tmpSize, dataReminder;
	char	*lastData;

	if ((msgLen > IPC_MAX_MESSAGE_SIZE) || (msgLen == 0))
		return NULL;

	/* Calculate number of 'next' buffers required */
	/* (i.e. buffers additional to the first buffer) */
	if (msgLen > IPC_FIRST_BUF_DATA_SIZE_MAX) {
		numOfNextBufs = (msgLen - IPC_FIRST_BUF_DATA_SIZE_MAX) /
					IPC_NEXT_BUF_DATA_SIZE_MAX;
		if ((msgLen - IPC_FIRST_BUF_DATA_SIZE_MAX) %
					IPC_NEXT_BUF_DATA_SIZE_MAX)
			numOfNextBufs++;
	}

	firstPtr = prevPtr = IPC_buf_alloc(dest_agent_id, pri);
	for (BufsCnt = 0; BufsCnt < numOfNextBufs; BufsCnt++) {
		if (prevPtr == NULL)
			break;
		nextPtr = IPC_buf_alloc(dest_agent_id, pri);
		if (nextPtr != NULL)
			IPC_buf_link(prevPtr, nextPtr);
		prevPtr = nextPtr;
	}

	/* If buffer allocation failed free the entire buffers */
	if ((prevPtr == NULL) && (firstPtr != NULL)) {
		IPC_buf_free(firstPtr, pri);
		firstPtr = NULL;
	} else if (firstPtr) {
		IPC_msg_set_type(firstPtr, msgType);
		IPC_msg_set_len(firstPtr, msgLen);
		IPC_msg_set_reply_ptr(firstPtr, repPtr);
		((struct IPC_message_hdr *)firstPtr)->destAgentId =
						dest_agent_id;
		((struct IPC_message_hdr *)firstPtr)->srcAgentId =
						src_agent_id;
		((struct IPC_message_hdr *)firstPtr)->requestSeqNum =
						ipc_reqSeqNume;
		ipc_reqSeqNume++;

		if (msgPtr != NULL) {
			lastData = msgPtr + msgLen;

			/* Now copy the Data */
			dataReminder = msgLen;
			tmpSize = min_t(uint16_t, dataReminder,
					IPC_FIRST_BUF_DATA_SIZE_MAX);

			memcpy(((struct IPC_first_buffer *)firstPtr)->body,
					lastData - dataReminder, tmpSize);

			dataReminder -= tmpSize;
			prevPtr = firstPtr;

			while (dataReminder > 0) {
				nextPtr = IPC_NEXT_PTR_PART(
					((struct IPC_message_hdr *)prevPtr)->
								nextBufPtr);
				tmpSize = min_t(uint16_t, dataReminder,
						IPC_NEXT_BUF_DATA_SIZE_MAX);

				memcpy(((struct IPC_next_buffer *)nextPtr)->
									body,
						lastData - dataReminder,
						tmpSize);

				dataReminder -= tmpSize;
				prevPtr = nextPtr;
			}
		}
	}

	return firstPtr;
}

/* ===========================================================================
 * IPC_msg_send
 * ===========================================================================
 * Description:  Message send, first buffer of the message should be provided,
 *
 * Parameters:		buf_first	- Pointer to the first message buffer
 *			pri		- Transport priority level
 *
 *
 * Returns: Result code
 *
 */
int32_t IPC_msg_send(char *buf_first, enum IPC_trns_priority pri)
{
	struct IPC_next_buf		*curBufferPtr;
	struct IPC_transport_func const	*trns_ptr;
	IPC_trns_buf_send_t		sendFuncPtr;
	uint8_t				destAgentId;
	uint8_t				cpuId;
	int32_t				res = IPC_GENERIC_ERROR;

	if (likely(buf_first)) {
		destAgentId	= (((struct IPC_first_buffer *)buf_first)->
							msgHdr).destAgentId;
		cpuId		= IPC_GetNode(destAgentId);
		curBufferPtr	= (struct IPC_next_buf *)buf_first;
		trns_ptr	= IPC_getUtilFuncVector(cpuId);
		if (likely(trns_ptr)) {
			sendFuncPtr = trns_ptr->trns_buf_send_ptr;
			if (sendFuncPtr)
				res = (sendFuncPtr)((char *)curBufferPtr,
							destAgentId, pri);
		}
	}
	return res;
}

/* -----------------------------------------------------------
 * Function:	IPC_receive
 * Description:	Processing IPC messages
 * Input:		max_msg_count	- max number processed messages per call
 *			pri		- transport priority level
 * Output:		number of processed messages
 * -----------------------------------------------------------
 */
uint32_t IPC_receive(uint32_t max_msg_count, enum IPC_trns_priority pri)
{
	uint32_t		ix;
	char			*ipcData;

	for (ix = 0; ix < max_msg_count; ix++) {
		ipcData = IPC_trns_fifo_buf_read(pri);

		if (ipcData) {
			/* IPC_msg_handler(ipcData); */
			handle_incoming_packet(ipcData, IPC_OwnNode, pri);
		} else
			break; /* no more messages, queue empty */
	}
	return ix;
}
