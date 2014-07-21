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

static uint8_t	ipc_req_sn;	/* Maintain node related sequence number */

uint8_t		ipc_own_node;


/* ===========================================================================
 * ipc_appl_init
 * ===========================================================================
 * Description:	This function initializes software/HW during startup
 *
 * Parameters:		def_trns_funcs	- pointer to default transport layer
 *					  function vector
 *
 * Returns: n/a
 *
 */
static inline void ipc_appl_init(struct ipc_trns_func const *def_trns_funcs)
{
	ipc_own_node = ipc_get_own_node();
	ipc_agent_table_clean();

	ipc_trns_fifo_buf_init(ipc_own_node);

	/* Initialize IPC routing table (of CPU#) */
	ipc_route_table_init(def_trns_funcs);
}


unsigned ipc_init(void)
{
	ipc_appl_init(&ipc_fifo_utils);
	return 0;
}



/* ===========================================================================
 * ipc_buf_alloc
 * ===========================================================================
 * Description:	buffer allocation API, should be called before building
 *		new message
 *
 * Parameters:		dest_aid	- Message destination AgentId
 *			prio		- Transport priority level
 *
 *
 * Returns: Pointer to a 128 Byte buffer
 *
*/
char *ipc_buf_alloc(uint8_t dest_aid, enum ipc_trns_prio prio)
{
	char			*ptr = NULL;
	struct ipc_trns_func const *trns_funcs;
	ipc_trns_alloc_t	alloc_func;
	const uint8_t		cpuid = ipc_get_node(dest_aid);

	/* Allocate buffer of 128 Bytes using the allocation function */
	/* associated with the given destination agentId */
	trns_funcs = (void *)get_trns_funcs(cpuid);
	if (likely(trns_funcs)) {
		alloc_func = trns_funcs->trns_alloc;
		if (likely(alloc_func)) {
			ptr = alloc_func(dest_aid, prio);

			/* Clear the 'Next buffer' field. */
			if (likely(ptr))
				((struct ipc_buf_hdr *)ptr)->next = 0;
		}
	}

	return ptr;
}

/* ===========================================================================
 * ipc_buf_free
 * ===========================================================================
 * Description:  Free the buffer, could be called on IPC message receiving node
 *		or on sending node when need to free previously allocated
 *		buffers
 *
 * Parameters:		buf_first	- Pointer to first message buffer
 *			prio		- Transport priority level
 *
 *
 * Returns: Result code
 *
 */
int32_t ipc_buf_free(char *buf_first, enum ipc_trns_prio prio)
{
	struct ipc_buf_hdr		*cur_buf;
	struct ipc_buf_hdr		*next_buf;
	struct ipc_trns_func const	*trns_funcs;
	ipc_trns_free_t			free_func;
	uint8_t				dest_aid;
	uint8_t				cpuid;
	int32_t				res = IPC_GENERIC_ERROR;

	if (likely(buf_first)) {
		dest_aid  = (((struct ipc_first_buf *)buf_first)->
							msg_hdr).dest_aid;
		cur_buf = (struct ipc_buf_hdr *)buf_first;
		cpuid = ipc_get_node(dest_aid);
		trns_funcs = get_trns_funcs(cpuid);
		if (likely(trns_funcs)) {
			free_func = trns_funcs->trns_free;
			if (likely(free_func)) {
				/* Now loop all allocated buffers and free them.
				 * Last buffer is either a single (type = 0)
				 * or the buffer marked as the last (type = 2)
				 * all other buffers have their LSB set
				 * (type = 1 or 3).
				 */
				do {
					next_buf = ((struct ipc_msg_hdr *)
					 IPC_NEXT_PTR_PART(cur_buf))->next;
					free_func(IPC_NEXT_PTR_PART(cur_buf),
							dest_aid, prio);
					cur_buf = next_buf;
				} while ((uint32_t)cur_buf & IPC_BUF_TYPE_MTC);
				res = IPC_SUCCESS;
			}
		}
	}
	return res;
}

/* ===========================================================================
 * ipc_buf_link
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
static int32_t ipc_buf_link(char *buf_prev, char *buf_next)
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
 * ipc_msg_set_len
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
static int32_t ipc_msg_set_len(char *buf_first, size_t len)
{
	if (buf_first == NULL)
		return IPC_GENERIC_ERROR;
	(((struct ipc_first_buf *)buf_first)->msg_hdr).msg_len = len;
	return IPC_SUCCESS;
}

/* ===========================================================================
 * ipc_msg_set_type
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
static int32_t ipc_msg_set_type(char *buf_first, uint8_t type)
{
	if (buf_first == NULL)
		return IPC_GENERIC_ERROR;
	(((struct ipc_first_buf *)buf_first)->msg_hdr).msg_type = type;
	return IPC_SUCCESS;
}

/* ===========================================================================
 * ipc_msg_set_reply_ptr
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
static int32_t ipc_msg_set_reply_ptr(
	char			*buf_first,
	char			*buf_rep
)
{
	if (buf_first == NULL)
		return IPC_GENERIC_ERROR;
	(((struct ipc_first_buf *)buf_first)->msg_hdr).reply = buf_rep;
	return IPC_SUCCESS;
}


/* ===========================================================================
 * ipc_msg_alloc
 * ===========================================================================
 * Description:  Allocate message buffer[s] and set the type and length.
 *		Copy message data into allocated buffers.
 *
 *
 * Parameters:		src_aid	- Message source AgentId
 *			dest_aid	- Message destination AgentId
 *			msg		- Pointer to message data
 *			msg_len		- Message length
 *			msg_type	- Message type
 *			reply		- Pointer to allocated reply buffer
 *			prio		- Transport priority level
 *
 *
 * Returns: Pointer to the message first buffer
 *
 */
char *ipc_msg_alloc(
	uint8_t			src_aid,
	uint8_t			dest_aid,
	char			*msg,
	size_t			msg_len,
	uint8_t			msg_type,
	char			*reply,
	enum ipc_trns_prio	prio
)
{
	char			*first_buf = NULL;
	char			*prev_buf = NULL;
	char			*next_buf = NULL;
	unsigned		buf;
	unsigned		next_bufs_num = 0;
	size_t			tmp_size, reminder;
	char			*last_data;

	if ((msg_len > IPC_MAX_MESSAGE_SIZE) || (msg_len == 0))
		return NULL;

	/* Calculate number of 'next' buffers required */
	/* (i.e. buffers additional to the first buffer) */
	if (msg_len > IPC_FIRST_BUF_DATA_SIZE_MAX) {
		next_bufs_num = (msg_len - IPC_FIRST_BUF_DATA_SIZE_MAX) /
					IPC_NEXT_BUF_DATA_SIZE_MAX;
		if ((msg_len - IPC_FIRST_BUF_DATA_SIZE_MAX) %
					IPC_NEXT_BUF_DATA_SIZE_MAX)
			next_bufs_num++;
	}

	first_buf = prev_buf = ipc_buf_alloc(dest_aid, prio);
	for (buf = 0; buf < next_bufs_num; buf++) {
		if (prev_buf == NULL)
			break;
		next_buf = ipc_buf_alloc(dest_aid, prio);
		if (next_buf != NULL)
			ipc_buf_link(prev_buf, next_buf);
		prev_buf = next_buf;
	}

	/* If buffer allocation failed free the entire buffers */
	if ((prev_buf == NULL) && (first_buf != NULL)) {
		ipc_buf_free(first_buf, prio);
		first_buf = NULL;
	} else if (first_buf) {
		ipc_msg_set_type(first_buf, msg_type);
		ipc_msg_set_len(first_buf, msg_len);
		ipc_msg_set_reply_ptr(first_buf, reply);
		((struct ipc_msg_hdr *)first_buf)->dest_aid = dest_aid;
		((struct ipc_msg_hdr *)first_buf)->src_aid = src_aid;
		((struct ipc_msg_hdr *)first_buf)->request_num = ipc_req_sn;
		ipc_req_sn++;

		if (msg) {
			last_data = msg + msg_len;

			/* Now copy the Data */
			reminder = msg_len;
			tmp_size = min_t(size_t, reminder,
					IPC_FIRST_BUF_DATA_SIZE_MAX);

			memcpy(((struct ipc_first_buf *)first_buf)->body,
					last_data - reminder, tmp_size);

			reminder -= tmp_size;
			prev_buf = first_buf;

			while (reminder > 0) {
				next_buf = IPC_NEXT_PTR_PART(
					((struct ipc_msg_hdr *)prev_buf)->next);
				tmp_size = min_t(size_t, reminder,
						IPC_NEXT_BUF_DATA_SIZE_MAX);

				memcpy(((struct ipc_next_buf *)next_buf)->body,
						last_data - reminder, tmp_size);

				reminder -= tmp_size;
				prev_buf = next_buf;
			}
		}
	}

	return first_buf;
}

/* ===========================================================================
 * ipc_msg_send
 * ===========================================================================
 * Description:  Message send, first buffer of the message should be provided,
 *
 * Parameters:		buf_first	- Pointer to the first message buffer
 *			prio		- Transport priority level
 *
 *
 * Returns: Result code
 *
 */
int32_t ipc_msg_send(char *buf_first, enum ipc_trns_prio prio)
{
	struct ipc_next_buf		*buf;
	struct ipc_trns_func const	*trns_funcs;
	ipc_trns_send_t			send_func;
	uint8_t				dest_aid;
	uint8_t				cpuid;
	int32_t				res = IPC_GENERIC_ERROR;

	if (likely(buf_first)) {
		dest_aid	= (((struct ipc_first_buf *)buf_first)->
							msg_hdr).dest_aid;
		cpuid		= ipc_get_node(dest_aid);
		buf		= (struct ipc_next_buf *)buf_first;
		trns_funcs	= get_trns_funcs(cpuid);
		if (likely(trns_funcs)) {
			send_func = trns_funcs->trns_send;
			if (send_func)
				res = send_func((char *)buf, dest_aid, prio);
		}
	}
	return res;
}

/* -----------------------------------------------------------
 * Function:	ipc_recv
 * Description:	Processing IPC messages
 * Input:		max_msg_count	- max number processed messages per call
 *			prio		- transport priority level
 * Output:		number of processed messages
 * -----------------------------------------------------------
 */
uint32_t ipc_recv(uint32_t max_msg_count, enum ipc_trns_prio prio)
{
	unsigned		ix;
	char			*ipc_data;

	for (ix = 0; ix < max_msg_count; ix++) {
		ipc_data = ipc_trns_fifo_buf_read(prio);

		if (ipc_data) {
			/* IPC_msg_handler(ipc_data); */
			handle_incoming_packet(ipc_data, ipc_own_node, prio);
		} else
			break; /* no more messages, queue empty */
	}
	return ix;
}
