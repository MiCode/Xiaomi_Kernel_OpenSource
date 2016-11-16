/*
 * SiI8620 Linux Driver
 *
 * Copyright (C) 2013-2014 Silicon Image, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 * This program is distributed AS-IS WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; INCLUDING without the implied warranty
 * of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.
 * See the GNU General Public License for more details at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 */

#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/list.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>

#include "si_fw_macros.h"
#include "si_infoframe.h"
#include "si_edid.h"
#include "si_mhl_defs.h"
#include "si_mhl2_edid_3d_api.h"
#include "si_8620_internal_api.h"
#include "si_mhl_tx_hw_drv_api.h"
#ifdef MEDIA_DATA_TUNNEL_SUPPORT
#include "si_mdt_inputdev.h"
#endif
#include "mhl_rcp_inputdev.h"
#if (INCLUDE_RBP == 1)
#include "mhl_rbp_inputdev.h"
#endif
#include "mhl_linux_tx.h"
#include "mhl_supp.h"
#include "si_app_devcap.h"
#include "platform.h"
#include "si_mhl_callback_api.h"
#include "si_8620_drv.h"

/*
	We choose 150 ms as the sample period due to hardware
		taking 140 ms to distinguish between an error
		and a disconnection.
	We will keep the last three samples and discard
		most recent two.
*/
#define LOCAL_eCBUS_ERR_SAMPLE_PERIOD 150

static void si_mhl_tx_refresh_peer_devcap_entries_impl(
	struct mhl_dev_context *dev_context, const char *function, int line);
#define si_mhl_tx_refresh_peer_devcap_entries(context) \
	si_mhl_tx_refresh_peer_devcap_entries_impl(context, __func__, __LINE__)

static void cbus_abort_timer_callback(void *callback_param);
static void bist_timer_callback(void *callback_param);
static void cbus_dcap_rdy_timeout_callback(void *callback_param);
static void cbus_dcap_chg_timeout_callback(void *callback_param);
static void cbus_mode_up_timeout_callback(void *callback_param);

uint16_t plim_table[] = {
	 500,
	 900,
	1500,
	 100,
	2000,
	   0,
	   0,
	   0
};
#ifdef DEBUG
static char *get_cbus_command_string(int command)
{
#define CBUS_COMMAND_CASE(command) case command: return #command;
	switch (command) {
		CBUS_COMMAND_CASE(MHL_ACK)
		    CBUS_COMMAND_CASE(MHL_NACK)
		    CBUS_COMMAND_CASE(MHL_ABORT)
		    CBUS_COMMAND_CASE(MHL_WRITE_STAT)
		    CBUS_COMMAND_CASE(MHL_SET_INT)
		    CBUS_COMMAND_CASE(MHL_READ_DEVCAP_REG)
		    CBUS_COMMAND_CASE(MHL_READ_XDEVCAP_REG)
		    CBUS_COMMAND_CASE(MHL_GET_STATE)
		    CBUS_COMMAND_CASE(MHL_GET_VENDOR_ID)
		    CBUS_COMMAND_CASE(MHL_SET_HPD)
		    CBUS_COMMAND_CASE(MHL_CLR_HPD)
		    CBUS_COMMAND_CASE(MHL_SET_CAP_ID)
		    CBUS_COMMAND_CASE(MHL_GET_CAP_ID)
		    CBUS_COMMAND_CASE(MHL_MSC_MSG)
		    CBUS_COMMAND_CASE(MHL_GET_SC1_ERRORCODE)
		    CBUS_COMMAND_CASE(MHL_GET_DDC_ERRORCODE)
		    CBUS_COMMAND_CASE(MHL_GET_MSC_ERRORCODE)
		    CBUS_COMMAND_CASE(MHL_WRITE_BURST)
		    CBUS_COMMAND_CASE(MHL_GET_SC3_ERRORCODE)
		    CBUS_COMMAND_CASE(MHL_WRITE_XSTAT)
		    CBUS_COMMAND_CASE(MHL_READ_DEVCAP)
		    CBUS_COMMAND_CASE(MHL_READ_XDEVCAP)
		    CBUS_COMMAND_CASE(MHL_READ_EDID_BLOCK)
		    CBUS_COMMAND_CASE(MHL_SEND_3D_REQ_OR_FEAT_REQ)
	}
	return "unknown";
}
#else
#define get_cbus_command_string(command) ""
#endif

static char *rapk_error_code_string[] = {
	"NO_ERROR",
	"UNRECOGNIZED_ACTION_CODE",
	"UNSUPPORTED_ACTION_CODE",
	"RESPONDER_BUSY"
};

struct mhl_dev_context *get_mhl_device_context(void *context)
{
	struct mhl_dev_context *dev_context = context;

	if (dev_context->signature != MHL_DEV_CONTEXT_SIGNATURE)
		dev_context = container_of(context,
					   struct mhl_dev_context, drv_context);
	return dev_context;
}

void init_cbus_queue(struct mhl_dev_context *dev_context)
{
	struct cbus_req *entry;
	int idx;

	INIT_LIST_HEAD(&dev_context->cbus_queue);
	INIT_LIST_HEAD(&dev_context->cbus_free_list);

	dev_context->current_cbus_req = NULL;

	/* Place pre-allocated CBUS queue entries on the free list */
	for (idx = 0; idx < NUM_CBUS_EVENT_QUEUE_EVENTS; idx++) {

		entry = &dev_context->cbus_req_entries[idx];
		memset(entry, 0, sizeof(struct cbus_req));
		list_add(&entry->link, &dev_context->cbus_free_list);
	}
}

static struct cbus_req *get_free_cbus_queue_entry_impl(
	struct mhl_dev_context *dev_context, const char *function, int line)
{
	struct cbus_req *req;
	struct list_head *entry;

	if (list_empty(&dev_context->cbus_free_list)) {
		int i;
		MHL_TX_GENERIC_DBG_PRINT(-1,
			"No free cbus queue entries available %s:%d\n",
			function, line);
		list_for_each(entry, &dev_context->cbus_queue) {
			req = list_entry(entry, struct cbus_req, link);
			MHL_TX_GENERIC_DBG_PRINT(-1,
				"cbus_queue entry %d called from %s:%d\n\t%s "
				"0x%02x 0x%02x\n",
				req->sequence, req->function, req->line,
				get_cbus_command_string(req->command),
				req->reg, req->reg_data);
		}
		for (i = 0; i < ARRAY_SIZE(dev_context->cbus_req_entries);
			++i) {
			req = &dev_context->cbus_req_entries[i];
			MHL_TX_GENERIC_DBG_PRINT(-1,
				"%d cbus_req_entries[%d] called from %s:%d\n",
				req->sequence, i, req->function, req->line);
		}
		return NULL;
	}

	entry = dev_context->cbus_free_list.next;
	list_del(entry);
	req = list_entry(entry, struct cbus_req, link);

	/* Start clean */
	req->status.flags.cancel = 0;
	req->completion = NULL;

	req->function = function;
	req->line = line;
	req->sequence = dev_context->sequence++;
	/*MHL_TX_DBG_ERR(,"q %d get:0x%pK %s:%d\n",
		req->sequence,req,function,line); */
	return req;
}

#define get_free_cbus_queue_entry(context) \
	get_free_cbus_queue_entry_impl(context, __func__, __LINE__)

static void return_cbus_queue_entry_impl(struct mhl_dev_context *dev_context,
					 struct cbus_req *pReq,
					 const char *function, int line)
{
	/* MHL_TX_DBG_ERR(,"q ret:0x%pK %s:%d\n",pReq,function,line); */
	list_add(&pReq->link, &dev_context->cbus_free_list);

}

#define return_cbus_queue_entry(context, req) \
	return_cbus_queue_entry_impl(context, req, __func__, __LINE__)

void queue_cbus_transaction(struct mhl_dev_context *dev_context,
			    struct cbus_req *pReq)
{
	MHL_TX_DBG_INFO("0x%02x 0x%02x 0x%02x\n",
			pReq->command,
			(MHL_MSC_MSG == pReq->command) ?
			pReq->msg_data[0] : pReq->reg,
			(MHL_MSC_MSG == pReq->command) ?
			pReq->msg_data[1] : pReq->reg_data);

	list_add_tail(&pReq->link, &dev_context->cbus_queue);
	/* try to send immediately, if possible */
	si_mhl_tx_drive_states(dev_context);
}

void queue_priority_cbus_transaction(struct mhl_dev_context *dev_context,
				     struct cbus_req *req)
{
	MHL_TX_DBG_INFO("0x%02x 0x%02x 0x%02x\n",
			req->command,
			(MHL_MSC_MSG == req->command) ?
			req->msg_data[0] : req->reg,
			(MHL_MSC_MSG == req->command) ?
			req->msg_data[1] : req->reg_data);

	list_add(&req->link, &dev_context->cbus_queue);
}

struct cbus_req *peek_next_cbus_transaction(struct mhl_dev_context *dev_context)
{
	struct list_head *entry;
	struct cbus_req *req;
	if (list_empty(&dev_context->cbus_queue)) {
		MHL_TX_DBG_INFO("Queue empty\n");
		return NULL;
	}
	entry = dev_context->cbus_queue.next;
	req = list_entry(entry, struct cbus_req, link);
	return req;
}

struct cbus_req *get_next_cbus_transaction(struct mhl_dev_context *dev_context)
{
	struct cbus_req *req;
	struct list_head *entry;
	enum cbus_mode_e cbus_mode;

	if (list_empty(&dev_context->cbus_queue)) {
		MHL_TX_DBG_INFO("Queue empty\n");
		return NULL;
	}

	if (dev_context->misc_flags.flags.cbus_abort_delay_active) {
		MHL_TX_DBG_INFO("CBUS abort delay in progress "
				"can't send any messages\n");
		return NULL;
	}

	cbus_mode = si_mhl_tx_drv_get_cbus_mode(dev_context);
	switch (cbus_mode) {
	case CM_NO_CONNECTION:
	case CM_TRANSITIONAL_TO_eCBUS_S_BIST:
	case CM_TRANSITIONAL_TO_eCBUS_S:
	case CM_TRANSITIONAL_TO_eCBUS_S_CAL_BIST:
	case CM_TRANSITIONAL_TO_eCBUS_S_CALIBRATED:
	case CM_TRANSITIONAL_TO_eCBUS_D_BIST:
	case CM_TRANSITIONAL_TO_eCBUS_D:
	case CM_TRANSITIONAL_TO_eCBUS_D_CAL_BIST:
	case CM_TRANSITIONAL_TO_eCBUS_D_CALIBRATED:
	case CM_eCBUS_S_BIST:
	case CM_eCBUS_D_BIST:
	case CM_BIST_DONE_PENDING_DISCONNECT:
		MHL_TX_DBG_INFO("CBUS not available\n");
		return NULL;
	default:
		break;
	}

	entry = dev_context->cbus_queue.next;
	req = list_entry(entry, struct cbus_req, link);

	list_del(entry);

	MHL_TX_DBG_INFO("0x%02x 0x%02x 0x%02x\n",
			req->command,
			(MHL_MSC_MSG == req->command) ?
			req->msg_data[0] : req->reg,
			(MHL_MSC_MSG == req->command) ?
			req->msg_data[1] : req->reg_data);

	return req;
}

#ifdef DEBUG
static char *get_block_id_string(int id)
{
#define BURST_ID_CASE(id) case id: return #id;
	switch (id) {
		BURST_ID_CASE(MHL_TEST_ADOPTER_ID)
		BURST_ID_CASE(burst_id_3D_VIC)
		BURST_ID_CASE(burst_id_3D_DTD)
		BURST_ID_CASE(burst_id_HEV_VIC)
		BURST_ID_CASE(burst_id_HEV_DTDA)
		BURST_ID_CASE(burst_id_HEV_DTDB)
		BURST_ID_CASE(burst_id_VC_ASSIGN)
		BURST_ID_CASE(burst_id_VC_CONFIRM)
		BURST_ID_CASE(burst_id_AUD_DELAY)
		BURST_ID_CASE(burst_id_ADT_BURSTID)
		BURST_ID_CASE(burst_id_BIST_SETUP)
		BURST_ID_CASE(burst_id_BIST_RETURN_STAT)
		BURST_ID_CASE(burst_id_EMSC_SUPPORT)
		BURST_ID_CASE(burst_id_HID_PAYLOAD)
		BURST_ID_CASE(burst_id_BLK_RCV_BUFFER_INFO)
		BURST_ID_CASE(burst_id_BITS_PER_PIXEL_FMT)
		BURST_ID_CASE(LOCAL_ADOPTER_ID)
	}
	return "unknown";
}
#else
#define get_block_id_string(command) ""
#endif

static struct block_req *start_new_block_marshalling_req_impl(
	struct mhl_dev_context *dev_context, const char *function, int line)
{
	struct block_req *req;
	struct list_head *entry;
	union SI_PACK_THIS_STRUCT emsc_payload_t *payload;

	if (list_empty(&dev_context->block_protocol.free_list)) {
		int i;
		MHL_TX_DBG_ERR("No free block queue entries available %s:%d\n",
			function, line);
		list_for_each(entry, &dev_context->block_protocol.queue) {
			req = list_entry(entry, struct block_req, link);
			MHL_TX_DBG_ERR("block_protocol.queue entry %d called "
				"from %s:%d\n\t%s 0x%04x\n",
				req->sequence, req->function, req->line,
				get_block_id_string(BURST_ID(req->payload->
					hdr_and_burst_id.burst_id)),
				BURST_ID(req->payload->
					hdr_and_burst_id.burst_id));
		}
		for (i = 0;
		     i < ARRAY_SIZE(dev_context->block_protocol.req_entries);
		     ++i) {
			req = &dev_context->block_protocol.req_entries[i];
			MHL_TX_DBG_ERR("%d block_protocol.req_entries[%d] "
				"called from %s:%d\n", req->sequence, i,
				req->function, req->line);
		}
		return NULL;
	}

	entry = dev_context->block_protocol.free_list.next;
	list_del(entry);
	req = list_entry(entry, struct block_req, link);

	payload = req->payload;
	req->function = function;
	req->line = line;
	req->sequence = dev_context->block_protocol.sequence++;
	req->sub_payload_size = 0;
	req->space_remaining =
	    sizeof(payload->as_bytes) -
	    sizeof(struct SI_PACK_THIS_STRUCT standard_transport_header_t);
	dev_context->block_protocol.marshalling_req = req;
	MHL_TX_DBG_WARN("q %d get:0x%pK %s:%d\n", req->sequence, req, function,
			line);
	return req;
}

#define start_new_block_marshalling_req(context) \
	start_new_block_marshalling_req_impl(context, __func__, __LINE__)

static void return_block_queue_entry_impl(struct mhl_dev_context *dev_context,
					  struct block_req *pReq,
					  const char *function, int line)
{
	/* MHL_TX_DBG_ERR(,"q ret:0x%pK %s:%d\n",pReq,function,line); */
	list_add(&pReq->link, &dev_context->block_protocol.free_list);

}

#define return_block_queue_entry(context, req) \
	return_block_queue_entry_impl(context, req, __func__, __LINE__)

struct block_req *get_next_block_transaction(struct mhl_dev_context
					     *dev_context)
{
	struct block_req *req;
	struct list_head *entry;

	if (list_empty(&dev_context->block_protocol.queue)) {
		MHL_TX_DBG_INFO("Queue empty\n");
		return NULL;
	}

	entry = dev_context->block_protocol.queue.next;
	list_del(entry);
	req = list_entry(entry, struct block_req, link);

	MHL_TX_DBG_INFO("0x%04x\n", req->payload->hdr_and_burst_id.burst_id);

	return req;
}

void si_mhl_tx_push_block_transactions(struct mhl_dev_context *dev_context)
{
	struct drv_hw_context *hw_context =
	    (struct drv_hw_context *)(&dev_context->drv_context);
	struct block_req *req;
	struct list_head *entry;
	uint16_t ack_byte_count;

	/*
	   Send the requests out, starting with those in the queue
	 */
	ack_byte_count = hw_context->block_protocol.received_byte_count;
	req = dev_context->block_protocol.marshalling_req;
	if (NULL == req) {
		MHL_TX_DBG_ERR("%s wayward pointer%s\n", ANSI_ESC_RED_TEXT,
			       ANSI_ESC_RESET_TEXT);
		return;
	}
	/* Need to send the unload count even if no other payload.      */
	/* If there is no payload and 2 or less unload bytes, don't bother --
	 * they will be unloaded with the next payload write.
	 * This could violate the MHL3 eMSC block transfer protocol requirement
	 * to ACK within 50ms, but it can also cause an CK feedback loop
	 * between the two peer devices.  If the unload count is larger,
	 * go ahead and send it even if it does cause an extra response
	 * from the other side.
	 */
	if ((ack_byte_count > EMSC_BLK_STD_HDR_LEN) || req->sub_payload_size) {
		/* don't use queue_block_transaction here */
		list_add_tail(&req->link, &dev_context->block_protocol.queue);
		dev_context->block_protocol.marshalling_req = NULL;
	}

	while (!list_empty(&dev_context->block_protocol.queue)) {
		uint16_t payload_size;
		entry = dev_context->block_protocol.queue.next;
		req = list_entry(entry, struct block_req, link);
		payload_size = sizeof(req->payload->hdr_and_burst_id.tport_hdr)
		    + req->sub_payload_size;

		MHL_TX_DBG_INFO(
			"=== sub_payload_size: %d, ack count: %d\n",
			req->sub_payload_size,
			hw_context->block_protocol.received_byte_count);

		if (hw_context->block_protocol.peer_blk_rx_buf_avail <
		    payload_size) {
			/* not enough space in peer's receive buffer,
			   so wait to send until later
			 */
			MHL_TX_DBG_ERR("==== not enough space in peer's "
				"receive buffer, send later payload_size:0x%x,"
				"blk_rx_buffer_avail:0x%x sub-payload size:"
				"0x%x tport_hdr size:0x%x\n",
				payload_size,
				hw_context->block_protocol.
				peer_blk_rx_buf_avail,
				req->sub_payload_size,
				sizeof(req->payload->
				hdr_and_burst_id.tport_hdr));
			break;
		}
		list_del(entry);
		hw_context->block_protocol.peer_blk_rx_buf_avail -=
		    payload_size;
		MHL_TX_DBG_WARN("PEER Buffer Available After Write: %d\n",
				hw_context->block_protocol.
				peer_blk_rx_buf_avail);

		req->payload->hdr_and_burst_id.tport_hdr.length_remaining =
		    req->sub_payload_size;
		req->count = payload_size;
		if (req->count < EMSC_PAYLOAD_LEN) {
			/* The driver layer will fill */
			/* in the rx_unload_ack field */
			mhl_tx_drv_send_block((struct drv_hw_context *)
					      (&dev_context->drv_context), req);
			/* return request to free list */
			return_block_queue_entry(dev_context, req);
		}
	}
	if (NULL == dev_context->block_protocol.marshalling_req) {
		/* now start a new marshalling request */
		req = start_new_block_marshalling_req(dev_context);
		if (NULL == req) {
			MHL_TX_DBG_ERR("%sblock free list exhausted!%s\n",
				       ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
		}
	}
}

void *si_mhl_tx_get_sub_payload_buffer(struct mhl_dev_context *dev_context,
				       uint8_t size)
{
	void *buffer;
	struct block_req *req;
	union emsc_payload_t *payload;
	req = dev_context->block_protocol.marshalling_req;
	if (NULL == req) {
		/* this can only happen if we run out of free requests */
		/* TODO: Lee - can't call this here, because the first thing
		 * it does is check to see if
		 * dev_context->block_protocol.marshalling_req == NULL,
		 * which we know it is, and if it finds NULL, it prints an
		 * error and returns.  So why bother?
		 */
		si_mhl_tx_push_block_transactions(dev_context);
		req = dev_context->block_protocol.marshalling_req;
		if (NULL == req) {
			MHL_TX_DBG_ERR("%sblock free list exhausted!%s\n",
				       ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			return NULL;
		}
	}
	if (size > req->space_remaining) {
		MHL_TX_DBG_INFO("0x%04x\n",
				req->payload->hdr_and_burst_id.burst_id);

		list_add_tail(&req->link, &dev_context->block_protocol.queue);
		si_mhl_tx_push_block_transactions(dev_context);

		req = start_new_block_marshalling_req(dev_context);
		if (NULL == req) {
			MHL_TX_DBG_ERR("%sblock free list exhausted!%s\n",
				       ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			return NULL;
		}
	}
	if (size > EMSC_BLK_CMD_MAX_LEN)
		return NULL;

	payload = req->payload;
	buffer = &payload->as_bytes[sizeof(payload->as_bytes) -
			req->space_remaining];
	req->space_remaining -= size;
	req->sub_payload_size += size;
	return buffer;
}

/*
 * Send the BLK_RCV_BUFFER_INFO BLOCK message.  This must be the first BLOCK
 * message sent, but we will wait until the XDEVCAPs have been read to allow
 * time for each side to initialize their eMSC message handling.
 */

void si_mhl_tx_send_blk_rcv_buf_info(struct mhl_dev_context *context)
{
	uint16_t rcv_buffer_size;
	struct SI_PACK_THIS_STRUCT block_rcv_buffer_info_t *buf_info;
	struct SI_PACK_THIS_STRUCT MHL3_emsc_support_data_t *emsc_supp;
	size_t total_size;

	total_size = sizeof(*buf_info)
			+ sizeof(*emsc_supp)
			- sizeof(emsc_supp->payload.burst_ids)
			+ sizeof(emsc_supp->payload.burst_ids[0]);

	buf_info = si_mhl_tx_get_sub_payload_buffer(context, total_size);
	if (NULL == buf_info) {
		MHL_TX_DBG_ERR("%ssi_mhl_tx_get_sub_payload_buffer failed%s\n",
			ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
	} else {
		/* next byte after blk_rcv_buf_info */
		emsc_supp =
			(struct SI_PACK_THIS_STRUCT MHL3_emsc_support_data_t *)
				(buf_info + 1);
		buf_info->burst_id.low =
			(uint8_t)(burst_id_BLK_RCV_BUFFER_INFO & 0xFF);
		buf_info->burst_id.high =
			(uint8_t)(burst_id_BLK_RCV_BUFFER_INFO >> 8);

		rcv_buffer_size = si_mhl_tx_drv_get_blk_rcv_buf_size();
		buf_info->blk_rcv_buffer_size_low =
			(uint8_t)(rcv_buffer_size & 0xFF);
		buf_info->blk_rcv_buffer_size_high =
			(uint8_t)(rcv_buffer_size >> 8);

		emsc_supp->header.burst_id.high = (uint8_t)
			(burst_id_EMSC_SUPPORT >> 8);
		emsc_supp->header.burst_id.low = (uint8_t)
			(burst_id_EMSC_SUPPORT & 0xFF);
		emsc_supp->header.checksum = 0;
		emsc_supp->header.total_entries = 1;
		emsc_supp->header.sequence_index = 1;
		emsc_supp->num_entries_this_burst = 1;
		emsc_supp->payload.burst_ids[0].high = (uint8_t)
			(SILICON_IMAGE_ADOPTER_ID >> 8);
		emsc_supp->payload.burst_ids[0].low = (uint8_t)
			(SILICON_IMAGE_ADOPTER_ID & 0xFF);

		emsc_supp->header.checksum =
			calculate_generic_checksum(emsc_supp, 0,
				total_size - sizeof(*buf_info));

		MHL_TX_DBG_INFO(
			"blk_rcv_buffer_info: id:0x%02X%02X\n"
			" sz: 0x%02X%02X\n"
			" emsc: 0x%02X%02X\n"
			" emsc-   cksum: 0x%02X\n"
			" emsc- tot_ent: 0x%02X\n"
			" emsc- seq_idx: 0x%02X\n"
			" emsc-this_bst: 0x%02X\n"
			" emsc-    high: 0x%02X\n"
			" emsc-     low: 0x%02X\n",
			buf_info->burst_id.high,
			buf_info->burst_id.low,
			buf_info->blk_rcv_buffer_size_high,
			buf_info->blk_rcv_buffer_size_low,
			emsc_supp->header.burst_id.high,
			emsc_supp->header.burst_id.low,
			emsc_supp->header.checksum,
			emsc_supp->header.total_entries,
			emsc_supp->header.sequence_index,
			emsc_supp->num_entries_this_burst,
			emsc_supp->payload.burst_ids[0].high,
			emsc_supp->payload.burst_ids[0].low);
	}

}

void si_mhl_tx_initialize_block_transport(struct mhl_dev_context *dev_context)
{
	struct drv_hw_context *hw_context =
		(struct drv_hw_context *)(&dev_context->drv_context);
	struct block_req *req;
	uint8_t buffer_size;
	int idx;
	struct block_buffer_info_t block_buffer_info;

	si_mhl_tx_platform_get_block_buffer_info(&block_buffer_info);

	hw_context->block_protocol.peer_blk_rx_buf_avail =
		EMSC_RCV_BUFFER_DEFAULT;
	hw_context->block_protocol.peer_blk_rx_buf_max =
		EMSC_RCV_BUFFER_DEFAULT;

	INIT_LIST_HEAD(&dev_context->block_protocol.queue);
	INIT_LIST_HEAD(&dev_context->block_protocol.free_list);

	/* Place pre-allocated BLOCK queue entries on the free list */
	for (idx = 0; idx < NUM_BLOCK_QUEUE_REQUESTS; idx++) {

		req = &dev_context->block_protocol.req_entries[idx];
		memset(req, 0, sizeof(*req));
		req->platform_header =
			block_buffer_info.buffer +
			block_buffer_info.req_size * idx;
		req->payload = (union SI_PACK_THIS_STRUCT emsc_payload_t *)
			(req->platform_header +
			block_buffer_info.payload_offset);
		list_add(&req->link, &dev_context->block_protocol.free_list);
	}
	buffer_size =
		sizeof(struct SI_PACK_THIS_STRUCT standard_transport_header_t)
		+ sizeof(struct SI_PACK_THIS_STRUCT block_rcv_buffer_info_t);
	if (buffer_size != 6) {
		MHL_TX_DBG_ERR("%scheck structure packing%s\n",
			ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
	}
	/* we just initialized the free list, this call cannot fail */
	start_new_block_marshalling_req(dev_context);

}

int si_mhl_tx_get_num_block_reqs(void)
{
	return NUM_BLOCK_QUEUE_REQUESTS;
}

uint8_t si_get_peer_mhl_version(struct mhl_dev_context *dev_context)
{
	uint8_t ret_val = dev_context->dev_cap_cache.mdc.mhl_version;

	if (0 == dev_context->dev_cap_cache.mdc.mhl_version) {
		/* If we come here it means we have not read devcap and
		 * VERSION_STAT must have placed the version asynchronously
		 * in peer_mhl3_version
		 */
		ret_val = dev_context->peer_mhl3_version;
	}
	return ret_val;
}

uint8_t calculate_generic_checksum(void *info_frame_data_parm, uint8_t checksum,
				   uint8_t length)
{
	uint8_t i;
	uint8_t *info_frame_data = (uint8_t *) info_frame_data_parm;

	for (i = 0; i < length; i++)
		checksum += info_frame_data[i];

	checksum = 0x100 - checksum;

	return checksum;
}

static struct cbus_req *write_stat_done(struct mhl_dev_context *dev_context,
				struct cbus_req *req, uint8_t data1);
/*
 * si_mhl_tx_set_status
 *
 * Set MHL defined STATUS bits in peer's register set.
 *
 * xstat    true for XSTATUS bits
 * register	MHL register to write
 * value	data to write to the register
 */
bool si_mhl_tx_set_status(struct mhl_dev_context *dev_context,
			  bool xstat, uint8_t reg_to_write, uint8_t value)
{
	struct cbus_req *req;

	MHL_TX_DBG_INFO("called\n");

	req = get_free_cbus_queue_entry(dev_context);
	if (req == NULL) {
		MHL_TX_GENERIC_DBG_PRINT(-1, "CBUS free queue exhausted\n")
		    return false;
	}

	req->retry_count = 2;
	if (xstat)
		req->command = MHL_WRITE_XSTAT;
	else {
		req->command = MHL_WRITE_STAT;
		req->completion = write_stat_done;
	}

	req->reg = reg_to_write;
	req->reg_data = value;

	queue_cbus_transaction(dev_context, req);

	return true;
}

/*
 * si_mhl_tx_send_3d_req_hawb
 * Send SET_INT(3D_REQ) as an atomic command.
 * completion is defined as finishing the 3D_DTD/3D_REQ
 * This function returns true if operation was successfully performed.
 *
 */
bool si_mhl_tx_send_3d_req_or_feat_req(struct mhl_dev_context *dev_context)
{
	struct cbus_req *req;

	MHL_TX_DBG_INFO
	    ("%sQueue 3D_REQ(MHL2.x) or FEAT_REQ(MHL 3.x) %s. MHL %02x\n",
	     ANSI_ESC_GREEN_TEXT, ANSI_ESC_RESET_TEXT,
	     si_get_peer_mhl_version(dev_context));
	req = get_free_cbus_queue_entry(dev_context);
	if (req == NULL) {
		MHL_TX_GENERIC_DBG_PRINT(-1, "CBUS free queue exhausted\n")
		    return false;
	}

	req->retry_count = 2;
	req->command = MHL_SEND_3D_REQ_OR_FEAT_REQ;
	req->reg = MHL_RCHANGE_INT;
	if (si_get_peer_mhl_version(dev_context) >= 0x30) {
		req->reg_data = MHL3_INT_FEAT_REQ;
	} else if (si_get_peer_mhl_version(dev_context) >= 0x20) {
		req->reg_data = MHL2_INT_3D_REQ;
	} else {
		/* Code must not come here. This is just a trap so look
		 * for the following message in log
		 */
		MHL_TX_DBG_ERR("%sMHL 1 does not support 3D\n");
		return false;
	}
	queue_cbus_transaction(dev_context, req);
	return true;
}

static struct cbus_req *set_int_done(struct mhl_dev_context *dev_context,
				struct cbus_req *req, uint8_t data1);
/*
 * si_mhl_tx_set_int
 * Set MHL defined INTERRUPT bits in peer's register set.
 * This function returns true if operation was successfully performed.
 *
 *  regToWrite      Remote interrupt register to write
 *  mask            the bits to write to that register
 *
 *  priority        0:  add to head of CBusQueue
 *                  1:  add to tail of CBusQueue
 */
bool si_mhl_tx_set_int(struct mhl_dev_context *dev_context,
		       uint8_t reg_to_write, uint8_t mask,
		       uint8_t priority_level)
{
	struct cbus_req *req;

	req = get_free_cbus_queue_entry(dev_context);
	if (req == NULL) {
		MHL_TX_GENERIC_DBG_PRINT(-1, "CBUS free queue exhausted\n")
		    return false;
	}

	req->retry_count = 2;
	req->command = MHL_SET_INT;
	req->reg = reg_to_write;
	req->reg_data = mask;
	req->completion = set_int_done;

	if (priority_level)
		queue_cbus_transaction(dev_context, req);
	else
		queue_priority_cbus_transaction(dev_context, req);

	return true;
}

static struct cbus_req *write_burst_done(struct mhl_dev_context *dev_context,
				struct cbus_req *req, uint8_t data1);

bool si_mhl_tx_send_write_burst(struct mhl_dev_context *dev_context,
				void *buffer)
{
	struct cbus_req *req;

	MHL_TX_DBG_INFO("%sQueue WRITE_BURST%s\n", ANSI_ESC_GREEN_TEXT,
			ANSI_ESC_RESET_TEXT);

	req = get_free_cbus_queue_entry(dev_context);
	if (req == NULL) {
		MHL_TX_GENERIC_DBG_PRINT(-1, "CBUS free queue exhausted\n")
		    return false;
	}

	req->retry_count = 1;
	req->command = MHL_WRITE_BURST;
	req->length = MHL_SCRATCHPAD_SIZE;
	req->burst_offset = 0;
	req->completion = write_burst_done;
	memcpy(req->msg_data, buffer, MHL_SCRATCHPAD_SIZE);

	queue_cbus_transaction(dev_context, req);
	return true;
}

static void si_mhl_tx_reset_states(struct mhl_dev_context *dev_context)
{
	/*
	 * Make sure that these timers do not start prematurely
	 */
	MHL_TX_DBG_INFO("stopping timers for DCAP_RDY and DCAP_CHG\n");
	mhl_tx_stop_timer(dev_context, dev_context->dcap_rdy_timer);
	mhl_tx_stop_timer(dev_context, dev_context->dcap_chg_timer);
	mhl_tx_stop_timer(dev_context, dev_context->t_rap_max_timer);

	/*
	 * Make sure that this timer does not start prematurely
	 */
	MHL_TX_DBG_INFO("stopping timer for CBUS_MODE_UP\n");
	mhl_tx_stop_timer(dev_context, dev_context->cbus_mode_up_timer);

	init_cbus_queue(dev_context);

	dev_context->mhl_connection_event = false;
	dev_context->edid_valid = false;
	dev_context->mhl_connected = MHL_TX_EVENT_DISCONNECTION;

	dev_context->msc_msg_arrived = false;
	dev_context->status_0 = 0;
	dev_context->status_1 = 0;
	dev_context->link_mode = MHL_STATUS_CLK_MODE_NORMAL;
	/* dev_context->preferred_clk_mode can be overridden by the application
	 * calling si_mhl_tx_set_preferred_pixel_format()
	 */
	dev_context->preferred_clk_mode = MHL_STATUS_CLK_MODE_NORMAL;
	{
		/* preserve BIST role as DUT or TE over disconnection
		*/
		bool temp = dev_context->misc_flags.flags.bist_role_TE;
		dev_context->misc_flags.as_uint32 = 0;
		dev_context->misc_flags.flags.bist_role_TE = temp ? 1 : 0;
	}
	dev_context->bist_timeout_total = 0;

#ifdef MEDIA_DATA_TUNNEL_SUPPORT
	memset(dev_context->mdt_devs.is_dev_registered,
	       INPUT_WAITING_FOR_REGISTRATION, MDT_TYPE_COUNT);
	dev_context->mdt_devs.x_max = X_MAX;
	dev_context->mdt_devs.x_screen = SCALE_X_SCREEN;
	dev_context->mdt_devs.x_raw = SCALE_X_RAW;
	dev_context->mdt_devs.x_shift = X_SHIFT;
	dev_context->mdt_devs.y_max = Y_MAX;
	dev_context->mdt_devs.y_screen = SCALE_Y_SCREEN;
	dev_context->mdt_devs.y_raw = SCALE_Y_RAW;
	dev_context->mdt_devs.y_shift = Y_SHIFT;
	dev_context->mdt_devs.swap_xy = SWAP_XY;
	dev_context->mdt_devs.swap_updown = SWAP_UPDOWN;
	dev_context->mdt_devs.swap_leftright = SWAP_LEFTRIGHT;
#endif

	dev_context->peer_mhl3_version = 0;
	memset(&dev_context->dev_cap_cache, 0,
	       sizeof(dev_context->dev_cap_cache));
	memset(&dev_context->xdev_cap_cache, 0,
	       sizeof(dev_context->xdev_cap_cache));

#if (INCLUDE_HID == 1)
	mhl3_hid_remove_all(dev_context);
#endif
	mhl_tx_stop_timer(dev_context, dev_context->bist_timer);
}

static void t_rap_max_timer_callback(void *callback_param)
{
	struct mhl_dev_context *dev_context = callback_param;
	mhl_event_notify(dev_context, MHL_TX_EVENT_T_RAP_MAX_EXPIRED,
				0x00, NULL);
}

int si_mhl_tx_reserve_resources(struct mhl_dev_context *dev_context)
{
	int ret;

	MHL_TX_DBG_INFO("called\n");
	ret = mhl_tx_create_timer(dev_context, cbus_abort_timer_callback,
				  dev_context, &dev_context->cbus_abort_timer);
	if (ret != 0) {
		MHL_TX_DBG_ERR("Failed to allocate CBUS abort timer!\n");
		return ret;
	}

	ret = mhl_tx_create_timer(dev_context, bist_timer_callback,
				  dev_context, &dev_context->bist_timer);
	if (ret != 0) {
		MHL_TX_DBG_ERR("Failed to allocate BIST timer!\n");
		return ret;
	}

	ret =
	    mhl_tx_create_timer(dev_context, cbus_dcap_rdy_timeout_callback,
				dev_context,
				&dev_context->dcap_rdy_timer);
	if (ret != 0) {
		MHL_TX_DBG_ERR("Failed to allocate dcap_rdy timeout timer!\n");
		return ret;
	}
	ret =
	    mhl_tx_create_timer(dev_context, cbus_dcap_chg_timeout_callback,
				dev_context,
				&dev_context->dcap_chg_timer);
	if (ret != 0) {
		MHL_TX_DBG_ERR("Failed to allocate dcap_chg timeout timer!\n");
		return ret;
	}
	ret = mhl_tx_create_timer(dev_context, cbus_mode_up_timeout_callback,
				  dev_context,
				  &dev_context->cbus_mode_up_timer);
	if (ret != 0) {
		MHL_TX_DBG_ERR
		    ("Failed to allocate cbus_mode_up timeout timer!\n");
		return ret;
	}
	ret = mhl_tx_create_timer(dev_context, t_rap_max_timer_callback,
				  dev_context,
				  &dev_context->t_rap_max_timer);
	if (ret != 0) {
		MHL_TX_DBG_ERR
		    ("Failed to allocate cbus_mode_up timeout timer!\n");
		return ret;
	}
	return ret;
}

int si_mhl_tx_initialize(struct mhl_dev_context *dev_context)
{
	MHL_TX_DBG_INFO("called\n");

	si_mhl_tx_reset_states(dev_context);

	dev_context->bist_setup.t_bist_mode_down = T_BIST_MODE_DOWN_MAX;

	return dev_context->drv_info->mhl_device_initialize(
				(struct drv_hw_context *)
				(&dev_context->drv_context));

}

static void cbus_abort_timer_callback(void *callback_param)
{
	struct mhl_dev_context *dev_context = callback_param;

	MHL_TX_DBG_INFO("CBUS abort timer expired, enable CBUS messaging\n");
	dev_context->misc_flags.flags.cbus_abort_delay_active = false;
	si_mhl_tx_drive_states(dev_context);
}

void si_mhl_tx_bist_cleanup(struct mhl_dev_context *dev_context)
{
	mhl_tx_stop_timer(dev_context, dev_context->bist_timer);
	MHL_TX_DBG_ERR("BIST duration elapsed\n");
	msleep(dev_context->bist_setup.t_bist_mode_down);
	if (BIST_TRIGGER_ECBUS_TX_RX_MASK &
				dev_context->bist_trigger_info) {
		si_mhl_tx_drv_stop_ecbus_bist((struct drv_hw_context *)
						  &dev_context->drv_context,
						  &dev_context->bist_setup);
	}

	if (BIST_TRIGGER_ECBUS_AV_LINK_MASK &
				dev_context->bist_trigger_info) {
		si_mhl_tx_drv_stop_avlink_bist(
			(struct drv_hw_context *)&dev_context->drv_context);
		dev_context->bist_stat.avlink_stat = 0;
	}

	if (BIST_TRIGGER_IMPEDANCE_TEST &
				dev_context->bist_trigger_info) {
		si_mhl_tx_drv_stop_impedance_bist((struct drv_hw_context *)
						  &dev_context->drv_context,
						  &dev_context->bist_setup);
	} else {

		enum cbus_mode_e cbus_mode;
		cbus_mode = si_mhl_tx_drv_get_cbus_mode(dev_context);
		if (cbus_mode > CM_oCBUS_PEER_IS_MHL3) {
			/* allow the other end some time to
				inspect their error count registers */
			MHL_TX_DBG_ERR("T_bist_mode_down\n")
			si_mhl_tx_drv_switch_cbus_mode(
			    (struct drv_hw_context *)&dev_context->drv_context,
			    CM_oCBUS_PEER_IS_MHL3_BIST_STAT);
		}
	}
	mhl_event_notify(dev_context, MHL_TX_EVENT_BIST_TEST_DONE, 0x00, NULL);
	si_mhl_tx_drive_states(dev_context);
}

static void bist_timer_callback(void *callback_param)
{
	struct mhl_dev_context *dev_context = callback_param;
	uint8_t test_sel;
	uint32_t bist_timeout_value = dev_context->bist_timeout_value;
	uint8_t ecbus_rx_run_done = false;
	uint8_t ecbus_tx_run_done = false;
	enum cbus_mode_e cbus_mode;

	cbus_mode = si_mhl_tx_drv_get_cbus_mode(dev_context);

	test_sel = dev_context->bist_trigger_info;
	dev_context->bist_timeout_total += LOCAL_eCBUS_ERR_SAMPLE_PERIOD;

	MHL_TX_DBG_INFO("%s\n", si_mhl_tx_drv_get_cbus_mode_str(cbus_mode))
	if (CM_BIST_DONE_PENDING_DISCONNECT == cbus_mode) {
		MHL_TX_DBG_ERR("%s Peer disconnected before"
				"T_BIST_MODE_DOWN expired%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT)
		return;
	} else if (dev_context->bist_timeout_total < bist_timeout_value) {
		int32_t temp;
		int32_t err_cnt = -1;

		temp = si_mhl_tx_drv_get_ecbus_bist_status(dev_context,
			&ecbus_rx_run_done, &ecbus_tx_run_done);
		/* sample the error counter as appropriate */
		if (dev_context->misc_flags.flags.bist_role_TE) {
			if (BIST_TRIGGER_E_CBUS_TX & test_sel) {
				err_cnt = temp;
				MHL_TX_DBG_WARN(
					"local eCBUS error count: %d\n",
					err_cnt)
			}
		} else {
			if (BIST_TRIGGER_E_CBUS_RX & test_sel) {
				err_cnt = temp;
				MHL_TX_DBG_WARN(
					"local eCBUS error count: %d\n",
					err_cnt)
			}
		}
		if (CM_NO_CONNECTION_BIST_STAT == cbus_mode) {
			MHL_TX_DBG_ERR("%s Peer disconnected before"
					" bist timeout expired%s: %d\n",
					ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT,
					dev_context->bist_timeout_total);
			;
		} else if (si_mhl_tx_drv_ecbus_connected(dev_context)) {
			/* accept the error count
				 only if we're still connected

				Since we can not distinguish between
				"real" errors and errors that happen
				as part of a disconnection, we keep track
				of the last three results and discard the
				two most recent.
			*/
			mhl_tx_start_timer(dev_context, dev_context->bist_timer,
				LOCAL_eCBUS_ERR_SAMPLE_PERIOD);
			dev_context->bist_stat.e_cbus_prev_local_stat =
				dev_context->bist_stat.e_cbus_local_stat;
			dev_context->bist_stat.e_cbus_local_stat =
				dev_context->bist_stat.e_cbus_next_local_stat;
			dev_context->bist_stat.e_cbus_next_local_stat = err_cnt;

			return;
		}
	} else if (0 == dev_context->bist_timeout_value) {
		MHL_TX_DBG_INFO("infinite duration AV BIST\n")
		mhl_tx_start_timer(dev_context, dev_context->bist_timer,
			LOCAL_eCBUS_ERR_SAMPLE_PERIOD);
		return;
	}
	si_mhl_tx_bist_cleanup(dev_context);
}

static void cbus_dcap_rdy_timeout_callback(void *callback_param)
{
	struct mhl_dev_context *dev_context = callback_param;
	enum cbus_mode_e cbus_mode;

	MHL_TX_DBG_ERR("%sCBUS DCAP_RDY timer expired%s\n",
		ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
	mhl_tx_stop_timer(dev_context, dev_context->dcap_rdy_timer);
	cbus_mode = si_mhl_tx_drv_get_cbus_mode(dev_context);
	if (CM_oCBUS_PEER_VERSION_PENDING == cbus_mode) {
		MHL_TX_DBG_ERR("%s%signoring lack of DCAP_RDY%s\n",
			ANSI_ESC_RED_TEXT, ANSI_ESC_YELLOW_BG,
			ANSI_ESC_RESET_TEXT);
		/*
		   Initialize registers to operate in oCBUS mode
		 */
		si_mhl_tx_drv_switch_cbus_mode(
			(struct drv_hw_context *)&dev_context->drv_context,
			CM_oCBUS_PEER_IS_MHL1_2);

		si_mhl_tx_refresh_peer_devcap_entries(dev_context);
		si_mhl_tx_drive_states(dev_context);
	}
}

static void cbus_dcap_chg_timeout_callback(void *callback_param)
{
	struct mhl_dev_context *dev_context = callback_param;
	enum cbus_mode_e cbus_mode;

	MHL_TX_DBG_ERR("%sCBUS DCAP_CHG timer expired%s\n", ANSI_ESC_RED_TEXT,
		ANSI_ESC_RESET_TEXT);
	mhl_tx_stop_timer(dev_context, dev_context->dcap_chg_timer);
	cbus_mode = si_mhl_tx_drv_get_cbus_mode(dev_context);
	if (CM_oCBUS_PEER_IS_MHL1_2 == cbus_mode) {
		MHL_TX_DBG_ERR("%s%signoring lack of DCAP_CHG%s\n",
			ANSI_ESC_RED_TEXT, ANSI_ESC_YELLOW_BG,
			ANSI_ESC_RESET_TEXT);
		si_mhl_tx_refresh_peer_devcap_entries(dev_context);
		si_mhl_tx_drive_states(dev_context);
	}
}

static void cbus_mode_up_timeout_callback(void *callback_param)
{
	struct mhl_dev_context *dev_context = callback_param;
	bool status;

	MHL_TX_DBG_INFO("CBUS_MODE_UP timer expired\n");

	status = si_mhl_tx_rap_send(dev_context, MHL_RAP_CBUS_MODE_UP);
	if (status)
		si_mhl_tx_drive_states(dev_context);
}

void process_cbus_abort(struct mhl_dev_context *dev_context)
{
	struct cbus_req *req;

	/*
	 * Place the CBUS message that errored back on
	 * transmit queue if it has any retries left.
	 */
	if (dev_context->current_cbus_req != NULL) {
		req = dev_context->current_cbus_req;
		dev_context->current_cbus_req = NULL;
		if (req->retry_count) {
			req->retry_count -= 1;
			queue_priority_cbus_transaction(dev_context, req);
		} else {
			return_cbus_queue_entry(dev_context, req);
		}
	}

	/* Delay the sending of any new CBUS messages for 2 seconds */
	dev_context->misc_flags.flags.cbus_abort_delay_active = true;

	mhl_tx_start_timer(dev_context, dev_context->cbus_abort_timer, 2000);
}

/*
 * si_mhl_tx_drive_states
 *
 * This function is called by the interrupt handler in the driver layer.
 * to move the MSC engine to do the next thing before allowing the application
 * to run RCP APIs.
 */
void si_mhl_tx_drive_states(struct mhl_dev_context *dev_context)
{
	struct cbus_req *req;
	struct cbus_req *peek;

	MHL_TX_DBG_INFO("\n");

	peek = peek_next_cbus_transaction(dev_context);
	if (NULL == peek) {
		/* nothing to send */
		return;
	}
	switch (si_mhl_tx_drv_get_cbus_mode(dev_context)) {
	case CM_NO_CONNECTION:
	case CM_TRANSITIONAL_TO_eCBUS_S_BIST:
	case CM_TRANSITIONAL_TO_eCBUS_S:
	case CM_TRANSITIONAL_TO_eCBUS_S_CAL_BIST:
	case CM_TRANSITIONAL_TO_eCBUS_S_CALIBRATED:
	case CM_TRANSITIONAL_TO_eCBUS_D_BIST:
	case CM_TRANSITIONAL_TO_eCBUS_D:
	case CM_TRANSITIONAL_TO_eCBUS_D_CAL_BIST:
	case CM_TRANSITIONAL_TO_eCBUS_D_CALIBRATED:
	case CM_eCBUS_S_BIST:
	case CM_eCBUS_D_BIST:
	case CM_BIST_DONE_PENDING_DISCONNECT:
		MHL_TX_DBG_ERR
		    ("CBUS transactions forbidden in transitional state"
		     " command:0x%x\n", peek->command);
		return;
	default:
		break;
	}

	req = dev_context->current_cbus_req;
	if (req != NULL) {
		char *command_string = get_cbus_command_string(req->command);
		switch (req->command) {
		case MHL_WRITE_BURST:
			if (MHL_WRITE_BURST == peek->command) {
				/* pending and next transactions
				 * are both WRITE_BURST
				 */
				if (si_mhl_tx_drv_hawb_xfifo_avail
					(dev_context)) {
					/* it's OK to send WRITE_BURSTs */
					break;
				}
			}
			MHL_TX_DBG_INFO("WRITE_BURST in progress\n");
			return;
		default:
			MHL_TX_DBG_INFO("%s[0x%02x]=0x%02x in progress\n",
					command_string,
					dev_context->current_cbus_req->reg,
					dev_context->current_cbus_req->
					reg_data);
			return;
		}
	}

	if (MHL_WRITE_BURST != peek->command) {
		if (si_mhl_tx_drv_get_pending_hawb_write_status(dev_context)) {
			/* hawb still pending */
			return;
		}
	}

	/* process queued CBus transactions */
	req = get_next_cbus_transaction(dev_context);
	if (req == NULL)
		return;

	MHL_TX_DBG_INFO("req: %pK\n", req);
	/* coordinate write burst requests and grants. */
	if (MHL_MSC_MSG == req->command) {
		dev_context->msc_msg_last_data = req->msg_data[1];
	}

	else if (MHL_SET_INT == req->command) {
		if (MHL_RCHANGE_INT == req->reg) {
			if (MHL_INT_GRT_WRT == req->reg_data) {
				dev_context->misc_flags.flags.
				    rcv_scratchpad_busy = true;
			}
		}
	}

	MHL_TX_DBG_INFO("req: %pK\n", req);
	if (req) {
		uint8_t ret_val;
		dev_context->current_cbus_req = req;
		switch (req->command) {
		case MHL_WRITE_BURST:
			do {
				struct cbus_req *next_req;
				ret_val = si_mhl_tx_drv_send_cbus_command(
					(struct drv_hw_context *)
					(&dev_context->drv_context), req);
				if (0 == ret_val) {
					/* WB XMIT level not available */
					MHL_TX_DBG_INFO("\n");
					break;
				}

				next_req =
				    peek_next_cbus_transaction(dev_context);

				/* process queued CBus transactions */
				if (next_req == NULL) {
					MHL_TX_DBG_INFO("\n");
					break;	/* out of the do-while loop */
				}

				if (MHL_WRITE_BURST != next_req->command) {
					MHL_TX_DBG_INFO("\n");
					break;
				}

				next_req =
				    get_next_cbus_transaction(dev_context);
				if (ret_val) {
					return_cbus_queue_entry(dev_context,
								req);
					req = next_req;
					dev_context->current_cbus_req =
					    next_req;
				}

			} while (ret_val && req);
			break;
		case MHL_MSC_MSG:
			if (MHL_MSC_MSG_RAP == req->msg_data[0]) {
				MHL_TX_DBG_INFO("sending RAP\n");
				mhl_tx_start_timer(dev_context,
					dev_context->t_rap_max_timer, 1000);
			}
			goto case_default;

		default:
case_default:
			ret_val = si_mhl_tx_drv_send_cbus_command(
				(struct drv_hw_context *)
				(&dev_context->drv_context), req);

			if (ret_val) {
				MHL_TX_DBG_INFO("current command: %s0x%02x%s\n",
						ANSI_ESC_YELLOW_TEXT, ret_val,
						ANSI_ESC_RESET_TEXT);
				req->command = ret_val;
			} else {
				return_cbus_queue_entry(dev_context, req);
				dev_context->current_cbus_req = NULL;
				if (MHL_READ_EDID_BLOCK == req->command) {
					dev_context->misc_flags.flags.
					    edid_loop_active = 0;
					MHL_TX_DBG_INFO
					    ("tag: EDID active: %d\n",
					     dev_context->misc_flags.flags.
					     edid_loop_active);
				}
			}
			break;
		}
	}
}

enum scratch_pad_status si_mhl_tx_request_write_burst(struct mhl_dev_context
						      *dev_context,
						      uint8_t burst_offset,
						      uint8_t length,
						      uint8_t *data)
{
	enum scratch_pad_status status = SCRATCHPAD_BUSY;

	if ((si_get_peer_mhl_version(dev_context) < 0x20) &&
	    !(dev_context->dev_cap_cache.mdc.featureFlag
	      & MHL_FEATURE_SP_SUPPORT)) {
		MHL_TX_DBG_ERR("failed SCRATCHPAD_NOT_SUPPORTED\n");
		status = SCRATCHPAD_NOT_SUPPORTED;

	} else if ((burst_offset + length) > SCRATCHPAD_SIZE) {
		MHL_TX_DBG_ERR("invalid offset + length\n");
		status = SCRATCHPAD_BAD_PARAM;

	} else {
		MHL_TX_DBG_ERR("Sending WB\n");
		si_mhl_tx_send_write_burst(dev_context, data);
		status = SCRATCHPAD_SUCCESS;
	}

	return status;
}

/*
 * si_mhl_tx_send_msc_msg
 *
 * This function sends a MSC_MSG command to the peer.
 * It returns true if successful in doing so.
 */
bool si_mhl_tx_send_msc_msg(struct mhl_dev_context *dev_context,
			    uint8_t command, uint8_t cmdData,
	struct cbus_req *(*completion)(struct mhl_dev_context *dev_context,
				struct cbus_req *req, uint8_t data1)
			)
{
	struct cbus_req *req;

	MHL_TX_DBG_INFO("called\n");

	req = get_free_cbus_queue_entry(dev_context);
	if (req == NULL) {
		MHL_TX_GENERIC_DBG_PRINT(-1, "CBUS free queue exhausted\n")
		    return false;
	}

	req->retry_count = 2;
	req->command = MHL_MSC_MSG;
	req->msg_data[0] = command;
	req->msg_data[1] = cmdData;
	req->completion = completion;

	queue_cbus_transaction(dev_context, req);

	return true;
}

struct cbus_req *rapk_done(struct mhl_dev_context *dev_context,
				struct cbus_req *req,
				uint8_t data1)
{
	if (MHL_RAP_CBUS_MODE_DOWN == dev_context->rap_in_sub_command) {
		MHL_TX_DBG_ERR("%sRAPK complete%s\n",
			ANSI_ESC_GREEN_TEXT,
			ANSI_ESC_RESET_TEXT)
			si_mhl_tx_drv_switch_cbus_mode(
				(struct drv_hw_context
				*)&dev_context->
				drv_context,
				CM_oCBUS_PEER_IS_MHL3);
	} else if (MHL_RAP_CBUS_MODE_UP == dev_context->rap_in_sub_command) {
		MHL_TX_DBG_ERR("%sRAPK complete%s\n",
			ANSI_ESC_GREEN_TEXT,
			ANSI_ESC_RESET_TEXT)
		si_mhl_tx_drv_switch_cbus_mode(
			(struct drv_hw_context *)&dev_context->drv_context,
			CM_eCBUS_S);
	}
	return req;
}
/*
 * si_mhl_rapk_send
 * This function sends RAPK to the peer device.
 */
static bool si_mhl_rapk_send(struct mhl_dev_context *dev_context,
			     uint8_t status)
{
	return si_mhl_tx_send_msc_msg(dev_context, MHL_MSC_MSG_RAPK, status,
			rapk_done);
}

struct cbus_req *rcpe_done(struct mhl_dev_context *dev_context,
				struct cbus_req *req,
				uint8_t data1)
{
	/*
	 * RCPE is always followed by an RCPK with
	 * original key code received.
	 */
	si_mhl_tx_rcpk_send(dev_context, dev_context->msc_save_rcp_key_code);
	return req;
}
/*
 * si_mhl_tx_rcpe_send
 *
 * The function will return a value of true if it could successfully send the
 * RCPE subcommand. Otherwise false.
 *
 * When successful, mhl_tx internally sends RCPK with original (last known)
 * keycode.
 */
bool si_mhl_tx_rcpe_send(struct mhl_dev_context *dev_context,
			 uint8_t rcpe_error_code)
{
	bool status;

	status = si_mhl_tx_send_msc_msg(dev_context, MHL_MSC_MSG_RCPE,
					rcpe_error_code, rcpe_done);
	if (status)
		si_mhl_tx_drive_states(dev_context);

	return status;
}

/*
 * si_mhl_tx_bist_setup
 *
 * This function sends a BIST_SETUP WRITE_BURST to the MHL3 peer.
 * See Section 15 of the MHL3 specification
 *
 * This function returns the status of the command sent.
 *
 * This function is called only when Titan is the BIST initiator,
 * i.e. test equipment. This function can only be called when the CBUS is
 * in oCBUS mode. If the CBUS is in any other mode, the WRITE_BURST will not
 * be sent to the MHL3 peer and this function will return failure.
 */
enum bist_cmd_status si_mhl_tx_bist_setup(struct mhl_dev_context *dev_context,
					  struct bist_setup_info *setup)
{
	enum bist_cmd_status ret_status = BIST_STATUS_NO_ERROR;

	/* Validate current cbus mode and bist_setup_info */
	if (si_mhl_tx_drv_get_cbus_mode(dev_context) !=
		CM_oCBUS_PEER_IS_MHL3_BIST_SETUP) {
		ret_status = BIST_STATUS_NOT_IN_OCBUS;
	} else if (setup->e_cbus_duration == 0x00) {
		ret_status = BIST_STATUS_INVALID_SETUP;
	} else if ((setup->e_cbus_pattern == BIST_ECBUS_PATTERN_UNSPECIFIED) ||
		(setup->e_cbus_pattern > BIST_ECBUS_PATTERN_MAX)) {
		ret_status = BIST_STATUS_INVALID_SETUP;
		/*} if setup->e_cbus_pattern is Fixed10
		 * with no support for eCBUS-D {
		 */
		/* ret_status = BIST_STATUS_INVALID_SETUP; */
	} else
	    if ((setup->avlink_data_rate == BIST_AVLINK_DATA_RATE_UNSPECIFIED)
		|| (setup->avlink_pattern > BIST_AVLINK_DATA_RATE_MAX)) {
		ret_status = BIST_STATUS_INVALID_SETUP;
	} else if ((setup->avlink_pattern == BIST_AVLINK_PATTERN_UNSPECIFIED) ||
		(setup->avlink_pattern > BIST_AVLINK_PATTERN_MAX)) {
		ret_status = BIST_STATUS_INVALID_SETUP;
		/*} validate video mode { */
		/* ret_status = BIST_STATUS_INVALID_SETUP; */
	} else if ((setup->impedance_mode == BIST_IMPEDANCE_MODE_RESERVED_1) ||
		(setup->impedance_mode == BIST_IMPEDANCE_MODE_RESERVED_2) ||
		(setup->impedance_mode > BIST_IMPEDANCE_MODE_MAX)) {
		ret_status = BIST_STATUS_INVALID_SETUP;
	} else {
		/* Build BIST_SETUP WRITE_BURST */
		struct bist_setup_burst burst;
		burst.burst_id_h = HIGH_BYTE_16(burst_id_BIST_SETUP);
		burst.burst_id_l = LOW_BYTE_16(burst_id_BIST_SETUP);
		burst.checksum = 0x00;
		burst.e_cbus_duration = setup->e_cbus_duration;
		burst.e_cbus_pattern = setup->e_cbus_pattern;
		burst.e_cbus_fixed_h = HIGH_BYTE_16(setup->e_cbus_fixed_pat);
		burst.e_cbus_fixed_l =  LOW_BYTE_16(setup->e_cbus_fixed_pat);
		burst.avlink_data_rate = setup->avlink_data_rate;
		burst.avlink_pattern = setup->avlink_pattern;
		burst.avlink_video_mode = setup->avlink_video_mode;
		burst.avlink_duration = setup->avlink_duration;
		burst.avlink_fixed_h = HIGH_BYTE_16(setup->avlink_fixed_pat);
		burst.avlink_fixed_l =  LOW_BYTE_16(setup->avlink_fixed_pat);
		burst.avlink_randomizer = setup->avlink_randomizer;
		burst.impedance_mode = setup->impedance_mode;

		/* calculate checksum */
		burst.checksum =
		    calculate_generic_checksum((uint8_t *) &burst, 0,
					       sizeof(burst));

		/* Send WRITE_BURST */
		si_mhl_tx_request_write_burst(dev_context, 0, sizeof(burst),
					      (uint8_t *) &burst);
		si_mhl_tx_drv_switch_cbus_mode(
			(struct drv_hw_context *)&dev_context->drv_context,
			CM_oCBUS_PEER_IS_MHL3_BIST_SETUP_SENT);
	}

	return ret_status;
}

void si_mhl_tx_set_bist_timer_impl(struct mhl_dev_context *dev_context,
				const char *caller, int line_num)
{
	MHL_TX_DBG_ERR("BIST timeout (%d,%d) from %s:%d\n",
		dev_context->bist_timeout_value,
		LOCAL_eCBUS_ERR_SAMPLE_PERIOD, caller, line_num)

	dev_context->bist_timeout_total = 0;
	mhl_tx_start_timer(dev_context, dev_context->bist_timer,
		LOCAL_eCBUS_ERR_SAMPLE_PERIOD);
}

static bool determine_bist_timeout_value(struct mhl_dev_context *dev_context)
{
	uint32_t bist_timeout = 0;
	uint32_t av_link_timeout = 0;
	uint8_t test_sel;

	MHL_TX_DBG_INFO("\n");

	test_sel = dev_context->bist_trigger_info;
	dev_context->bist_timeout_value = 0;

	if (test_sel & BIST_TRIGGER_IMPEDANCE_TEST) {
		MHL_TX_DBG_INFO("\n")
		bist_timeout =
		    dev_context->bist_setup.e_cbus_duration * 1000;
		dev_context->bist_timeout_value = bist_timeout;
		si_mhl_tx_set_bist_timer(dev_context);
	} else {
		if (test_sel & BIST_TRIGGER_ECBUS_TX_RX_MASK) {
			MHL_TX_DBG_INFO("Initiate eCBUS BIST\n");
			bist_timeout =
				dev_context->bist_setup.e_cbus_duration * 1000;
			dev_context->bist_timeout_value = bist_timeout;
		}
		if (test_sel & BIST_TRIGGER_ECBUS_AV_LINK_MASK) {
			MHL_TX_DBG_INFO("\n")
			av_link_timeout =
			    dev_context->bist_setup.avlink_duration;
			if (dev_context->bist_setup.avlink_pattern <=
			    BIST_AVLINK_PATTERN_FIXED_8) {
				MHL_TX_DBG_INFO("\n")
				/* ~17ms. per frame */
				av_link_timeout *= 32 * 17;
			} else {
				MHL_TX_DBG_INFO("\n")
				av_link_timeout *= 1000;
			}
			/*
			 * Run the test for the longer of either the
			 * eCBUS test time or the AV_LINK test time.
			 */
			if (av_link_timeout > bist_timeout) {
				MHL_TX_DBG_INFO("\n")
				dev_context->bist_timeout_value =
					av_link_timeout;
			}
			if (!(test_sel & BIST_TRIGGER_ECBUS_TX_RX_MASK)) {
				if (0 == av_link_timeout) {
					MHL_TX_DBG_ERR("indefinite\n")
					dev_context->bist_timeout_value = 0;
				}
			}
		}
		dev_context->bist_setup.t_bist_mode_down =
			(dev_context->bist_timeout_value * 10)/100
			+ T_BIST_MODE_DOWN_MIN;
		MHL_TX_DBG_ERR("%d\n", dev_context->bist_timeout_value)
	}
	if (!dev_context->bist_timeout_value)
		MHL_TX_DBG_ERR("No BIST timeout - wait for BIST_STOP\n");
	return true;
}

struct cbus_req *bist_trigger_done(struct mhl_dev_context *dev_context,
				struct cbus_req *req,
				uint8_t data1)
{
	/* We just requested a BIST test
	 * so now set up for it.
	 */
	MHL_TX_DBG_ERR("\n")
	if (BIST_TRIGGER_IMPEDANCE_TEST == dev_context->bist_trigger_info) {
		MHL_TX_DBG_ERR("\n")
		start_bist_initiator_test(dev_context);
	} else if (dev_context->bist_trigger_info) {
		enum cbus_mode_e mode =
		    dev_context->msc_msg_data & BIST_TRIGGER_TEST_E_CBUS_D ?
		    CM_eCBUS_D_BIST : CM_eCBUS_S_BIST;
		MHL_TX_DBG_ERR("\n")
		determine_bist_timeout_value(dev_context);
		if (1 == si_mhl_tx_drv_switch_cbus_mode(
		    (struct drv_hw_context *)&dev_context->drv_context, mode))
			MHL_TX_DBG_ERR("wait TDM\n");

	}
	return req;
}
/*
 * si_mhl_tx_bist_trigger
 *
 * This function sends a BIST_TRIGGER MSC_MSG to the MHL3 peer.
 * See Section 15 of the MHL3 specification
 *
 * This function returns the status of the command sent.
 *
 * This function is called only when Titan is the BIST initiator,
 * i.e. test equipment. This function can only be called when the CBUS is
 * in oCBUS mode. If the CBUS is in any other mode, the MSC_MSG will not
 * be sent to the MHL3 peer and this function will return failure.
 */
enum bist_cmd_status si_mhl_tx_bist_trigger(struct mhl_dev_context *dev_context,
					    uint8_t trigger_operand)
{
	enum bist_cmd_status ret_status = BIST_STATUS_NO_ERROR;

	trigger_operand &= BIST_TRIGGER_OPERAND_VALID_MASK;

	/* Validate current cbus mode and trigger_operand */
	if (si_mhl_tx_drv_get_cbus_mode(dev_context) !=
		CM_oCBUS_PEER_IS_MHL3_BIST_SETUP_PEER_READY) {
		MHL_TX_DBG_ERR("%sBIST_STATUS_NOT_IN_OCBUS%s\n",
			ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT)
		ret_status = BIST_STATUS_DUT_NOT_READY;
	/*} else if (trigger_operand & BIST_TRIGGER_AVLINK_TX) {
		MHL_TX_DBG_ERR("%sBIST_STATUS_INVALID_TRIGGER%s\n",
			ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT)
		ret_status = BIST_STATUS_INVALID_TRIGGER;
	} else if ((trigger_operand & BIST_TRIGGER_OPERAND_SELECT_eCBUS_D) &&
		(eCBUS_D not supported)) {
	*/
	} else if ((trigger_operand & BIST_TRIGGER_IMPEDANCE_TEST) &&
		   ((trigger_operand & ~BIST_TRIGGER_IMPEDANCE_TEST) != 0x00)) {
		MHL_TX_DBG_ERR("%sBIST_STATUS_INVALID_TRIGGER%s\n",
			ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT)
		ret_status = BIST_STATUS_INVALID_TRIGGER;
	} else {
		dev_context->bist_trigger_info = trigger_operand;

		/* Send BIST_TRIGGER */
		si_mhl_tx_send_msc_msg(dev_context, MHL_MSC_MSG_BIST_TRIGGER,
			dev_context->bist_trigger_info, bist_trigger_done);
		si_mhl_tx_drive_states(dev_context);
	}

	return ret_status;
}
struct cbus_req *bist_stop_done(struct mhl_dev_context *dev_context,
				struct cbus_req *req, uint8_t data1)
{
	MHL_TX_DBG_ERR("\n")
	/* bist timer always runs, even for
		bist_timeout_value == 0 (infinite)
	*/
	si_mhl_tx_bist_cleanup(dev_context);
	return req;
}
/*
 * si_mhl_tx_bist_stop
 *
 * This function sends a BIST_STOP MSC_MSG to the MHL3 peer.
 * See Section 15 of the MHL3 specification
 *
 * This function returns the status of the command sent.
 *
 * This function is called only when Titan is the BIST initiator,
 * i.e. test equipment. This function can only be called when the CBUS is
 * in eCBUS mode. If the CBUS is in any other mode, the MSC_MSG will not
 * be sent to the MHL3 peer and this function will return failure.
 */
enum bist_cmd_status si_mhl_tx_bist_stop(struct mhl_dev_context *dev_context)
{
	enum bist_cmd_status ret_status = BIST_STATUS_NO_ERROR;

	/* Validate current cbus mode */
	if (si_mhl_tx_drv_get_cbus_mode(dev_context) < CM_eCBUS_S)
		ret_status = BIST_STATUS_NOT_IN_ECBUS;

	/* Send BIST_STOP */
	si_mhl_tx_send_msc_msg(dev_context,
		MHL_MSC_MSG_BIST_STOP, 0x00, bist_stop_done);
	si_mhl_tx_drive_states(dev_context);

	return ret_status;
}
struct cbus_req *bist_request_stat_done(struct mhl_dev_context *dev_context,
				struct cbus_req *req, uint8_t data1)
{
	MHL_TX_DBG_ERR("\n")
	return req;
}
/*
 * si_mhl_tx_bist_request_stat
 *
 * This function sends a BIST_REQUEST_STAT MSC_MSG to the MHL3 peer.
 * See Section 15 of the MHL3 specification
 *
 * This function returns the status of the command sent.
 *
 * This function is called only when Titan is the BIST initiator,
 * i.e. test equipment. This function can only be called when the CBUS is
 * in oCBUS mode. If the CBUS is in any other mode, the MSC_MSG will not
 * be sent to the MHL3 peer and this function will return failure.
 */
enum bist_cmd_status si_mhl_tx_bist_request_stat(struct mhl_dev_context
						 *dev_context,
						 uint8_t request_operand)
{
	enum bist_cmd_status ret_status = BIST_STATUS_NO_ERROR;

	/* Validate current cbus mode */
	if (si_mhl_tx_drv_get_cbus_mode(dev_context) !=
		CM_oCBUS_PEER_IS_MHL3_BIST_STAT)
		ret_status = BIST_STATUS_NOT_IN_OCBUS;

	/* verify operand 0x00 or 0x01 */

	/* Send BIST_REQUEST_STAT */
	si_mhl_tx_send_msc_msg(dev_context, MHL_MSC_MSG_BIST_REQUEST_STAT,
			       request_operand, bist_request_stat_done);
	si_mhl_tx_drive_states(dev_context);

	return ret_status;
}

void send_bist_status(struct mhl_dev_context *dev_context)
{
	struct bist_return_stat_burst bist_status;
	uint16_t ecbus_local_stat;

	memset(&bist_status, 0, sizeof(bist_status));
	bist_status.burst_id_h = burst_id_BIST_RETURN_STAT >> 8;
	bist_status.burst_id_l = burst_id_BIST_RETURN_STAT;
	bist_status.avlink_stat_h = dev_context->bist_stat.avlink_stat >> 8;
	bist_status.avlink_stat_l = dev_context->bist_stat.avlink_stat;
	if (dev_context->bist_stat.e_cbus_prev_local_stat < 0)
		ecbus_local_stat = 0xFFFF;
	else if (dev_context->bist_stat.e_cbus_prev_local_stat > 0xFFFF)
		ecbus_local_stat = 0xFFFE;
	else
		ecbus_local_stat =
			(uint16_t)dev_context->bist_stat.e_cbus_prev_local_stat;

	bist_status.e_cbus_stat_h = ecbus_local_stat >> 8;
	bist_status.e_cbus_stat_l = ecbus_local_stat;
	bist_status.checksum = calculate_generic_checksum((uint8_t
							   *) (&bist_status), 0,
							  sizeof(bist_status));

	si_mhl_tx_request_write_burst(dev_context, 0, sizeof(bist_status),
				      (uint8_t *) (&bist_status));
}

bool invalid_bist_parms(struct mhl_dev_context *dev_context,
				struct bist_setup_info *setup_info)
{
	uint8_t test_sel;
	bool e_cbus_d_sel = false;

	MHL_TX_DBG_ERR("\n")

	test_sel =  setup_info->bist_trigger_parm;
	e_cbus_d_sel = test_sel & BIST_TRIGGER_TEST_E_CBUS_D ? true : false;
	test_sel &= ~BIST_TRIGGER_E_CBUS_TYPE_MASK;

	if (test_sel > BIST_TRIGGER_IMPEDANCE_TEST) {
		MHL_TX_DBG_ERR("Impedance test cannot be run "
			       "concurrently with other tests!\n");
		return true;
	}

	if (BIST_TRIGGER_ECBUS_AV_LINK_MASK & test_sel) {
		switch (setup_info->avlink_video_mode) {
		case 4:		/* 1280 X 720 (720P) */
			break;
		case 3:		/* 720 X 480 (480P) */
			break;
		default:
			MHL_TX_DBG_ERR("Unsupported VIC received!\n");
			return true;
		}
		switch (setup_info->avlink_data_rate) {
		case 1:		/* 1.5 Gbps */
			MHL_TX_DBG_ERR("AV LINK_DATA_RATE 1.5Gbps\n");
			break;
		case 2:		/* 3.0 Gbps */
			MHL_TX_DBG_ERR("AV LINK_DATA_RATE 3.0Gbps\n");
			break;
		case 3:		/* 6.0 Gbps */
			MHL_TX_DBG_ERR("AV LINK_DATA_RATE 6.0Gbps\n");
			break;
		default:
			MHL_TX_DBG_ERR("%sUnsupported "
				"AVLINK_DATA_RATE %02X%s\n",
				ANSI_ESC_RED_TEXT,
				setup_info->avlink_data_rate,
				ANSI_ESC_RESET_TEXT);
			return true;
		}
		switch (setup_info->avlink_pattern) {
		case BIST_AVLINK_PATTERN_UNSPECIFIED:
		case BIST_AVLINK_PATTERN_PRBS:
			MHL_TX_DBG_ERR("AV LINK_PATTERN %sPRBS%s\n",
				ANSI_ESC_GREEN_TEXT, ANSI_ESC_RESET_TEXT)
			break;

		case BIST_AVLINK_PATTERN_FIXED_8:
			MHL_TX_DBG_ERR("AV LINK_PATTERN %sFixed8%s\n",
				ANSI_ESC_GREEN_TEXT, ANSI_ESC_RESET_TEXT);
			break;

		case BIST_AVLINK_PATTERN_FIXED_10:
			MHL_TX_DBG_ERR("AV LINK_PATTERN %sFixed10%s\n",
				ANSI_ESC_GREEN_TEXT, ANSI_ESC_RESET_TEXT);
			break;

		default:
			MHL_TX_DBG_ERR("%sUnrecognized test "
				"pattern detected!%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
			return true;
		}
	}
	if (BIST_TRIGGER_ECBUS_TX_RX_MASK & test_sel) {
		MHL_TX_DBG_ERR("\n")
		if (e_cbus_d_sel) {
			MHL_TX_DBG_ERR("Testing of eCBUS-D not supported yet\n")
			dev_context->bist_stat.e_cbus_prev_local_stat = 0xFFFF;
			dev_context->bist_stat.e_cbus_local_stat = 0xFFFF;
			dev_context->bist_stat.e_cbus_next_local_stat = 0xFFFF;
			dev_context->bist_stat.e_cbus_remote_stat = 0xFFFF;
			test_sel &= ~BIST_TRIGGER_ECBUS_TX_RX_MASK;
			return true;
		}
	}
	return false;
}

bool invalid_bist_te_parms(struct mhl_dev_context *dev_context,
				struct bist_setup_info *setup_info)
{
	uint8_t test_sel;
	test_sel =  setup_info->bist_trigger_parm;
	if (invalid_bist_parms(dev_context, setup_info)) {
		MHL_TX_DBG_ERR("\n")
		return true;
	} else if (test_sel & BIST_TRIGGER_AVLINK_TX) {
		MHL_TX_DBG_ERR("%sinvalid test_sel%s:0x%02x\n",
			ANSI_ESC_RED_TEXT,
			ANSI_ESC_RESET_TEXT,
			test_sel)
		/* Invalid test for MHL transmitter TE */
		dev_context->bist_stat.avlink_stat = 0xFFFF;
		return true;
	}

	return false;
}

bool invalid_bist_dut_parms(struct mhl_dev_context *dev_context,
				struct bist_setup_info *setup_info)
{
	uint8_t test_sel;
	test_sel =  setup_info->bist_trigger_parm;
	if (invalid_bist_parms(dev_context, setup_info)) {
		MHL_TX_DBG_ERR("\n")
		return true;
	} else if (test_sel & BIST_TRIGGER_AVLINK_RX) {
		/* Invalid test for MHL transmitter DUT */
		MHL_TX_DBG_ERR("%sInvalid test:0x%02X for MHL Tx DUT%s\n",
			ANSI_ESC_RED_TEXT,
			test_sel,
			ANSI_ESC_RESET_TEXT)
		dev_context->bist_stat.avlink_stat = 0xFFFF;
		return true;
	}

	return false;
}

void initiate_bist_test(struct mhl_dev_context *dev_context)
{
	uint8_t test_sel;
	enum cbus_mode_e cbus_mode;
	bool e_cbus_d_sel = false;

	MHL_TX_DBG_ERR("\n")

	if (invalid_bist_dut_parms(dev_context, &dev_context->bist_setup)) {
		MHL_TX_DBG_ERR("\n")
		si_mhl_tx_bist_cleanup(dev_context);
		return;
	}
	test_sel = dev_context->bist_trigger_info;
	e_cbus_d_sel = test_sel & BIST_TRIGGER_TEST_E_CBUS_D ? true : false;
	test_sel &= ~BIST_TRIGGER_E_CBUS_TYPE_MASK;
	cbus_mode = si_mhl_tx_drv_get_cbus_mode(dev_context);
	if (test_sel == 0) {
		MHL_TX_DBG_ERR("%sNo test selected%s\n",
				ANSI_ESC_RED_TEXT,
				ANSI_ESC_RESET_TEXT)
		si_mhl_tx_bist_cleanup(dev_context);
		return;
	}

	if (CM_oCBUS_PEER_IS_MHL3_BIST_STAT == cbus_mode) {
		MHL_TX_DBG_ERR("another BIST_TRIGGER\n")
		si_mhl_tx_drv_switch_cbus_mode(
			(struct drv_hw_context *)&dev_context->drv_context,
			CM_oCBUS_PEER_IS_MHL3_BIST_SETUP_PEER_READY);
		dev_context->bist_stat.avlink_stat = 0;
		dev_context->bist_stat.e_cbus_prev_local_stat = 0;
		dev_context->bist_stat.e_cbus_local_stat = 0;
		dev_context->bist_stat.e_cbus_next_local_stat = 0;
		dev_context->bist_stat.e_cbus_remote_stat = 0;
	} else if (CM_oCBUS_PEER_IS_MHL3_BIST_SETUP_PEER_READY != cbus_mode) {
		dev_context->bist_stat.avlink_stat = 0;
		dev_context->bist_stat.e_cbus_prev_local_stat = 0;
		dev_context->bist_stat.e_cbus_local_stat = 0;
		dev_context->bist_stat.e_cbus_next_local_stat = 0;
		dev_context->bist_stat.e_cbus_remote_stat = 0;
	} else {
		MHL_TX_DBG_ERR("BIST test requested without prior "
			       "valid BIST setup command\n");
		dev_context->bist_stat.avlink_stat = 0xFFFF;
		dev_context->bist_stat.e_cbus_prev_local_stat = 0xFFFF;
		dev_context->bist_stat.e_cbus_local_stat = 0xFFFF;
		dev_context->bist_stat.e_cbus_next_local_stat = 0xFFFF;
		dev_context->bist_stat.e_cbus_remote_stat = 0xFFFF;
		si_mhl_tx_bist_cleanup(dev_context);
		return;
	}

	if (test_sel & BIST_TRIGGER_IMPEDANCE_TEST) {
		if (cbus_mode != CM_oCBUS_PEER_IS_MHL3_BIST_SETUP_PEER_READY) {
			si_mhl_tx_bist_cleanup(dev_context);
			return;
		}
		if (0 == si_mhl_tx_drv_start_impedance_bist(
			(struct drv_hw_context *)
			&dev_context->drv_context,
			&dev_context->bist_setup)) {
			MHL_TX_DBG_ERR("\n")

		}
	} else {
		if (cbus_mode < CM_TRANSITIONAL_TO_eCBUS_S_CAL_BIST) {
			MHL_TX_DBG_ERR
				("%sCannot initiate eCBUS-S BIST when "
				"CBUS mode is%s %s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT,
				si_mhl_tx_drv_get_cbus_mode_str(cbus_mode));
			si_mhl_tx_bist_cleanup(dev_context);
			return;
		}

		if (e_cbus_d_sel &&
			cbus_mode < CM_TRANSITIONAL_TO_eCBUS_D_CAL_BIST) {
			MHL_TX_DBG_ERR
				("%sCannot initiate eCBUS-S BIST when "
				"CBUS mode is%s %s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT,
				si_mhl_tx_drv_get_cbus_mode_str(cbus_mode));
			si_mhl_tx_bist_cleanup(dev_context);
			return;
		}
		if (test_sel & BIST_TRIGGER_ECBUS_TX_RX_MASK) {
			MHL_TX_DBG_INFO("Initiate eCBUS BIST\n");
			si_mhl_tx_drv_start_ecbus_bist(
				(struct drv_hw_context *)
				&dev_context->drv_context,
				&dev_context->bist_setup);
		}
		if (test_sel & BIST_TRIGGER_AVLINK_TX) {
			MHL_TX_DBG_ERR("total: %d value:%d\n",
				dev_context->bist_timeout_total,
				dev_context->bist_timeout_value)
			si_mhl_tx_drv_start_avlink_bist(dev_context,
				&dev_context->bist_setup);
		}
	}
}

void start_bist_initiator_test(struct mhl_dev_context *dev_context)
{
	uint8_t test_sel;
	enum cbus_mode_e cbus_mode;
	bool e_cbus_d_sel = false;

	MHL_TX_DBG_ERR("\n");

	if (invalid_bist_te_parms(dev_context, &dev_context->bist_setup)) {
		MHL_TX_DBG_ERR("\n")
		si_mhl_tx_bist_cleanup(dev_context);
		return;
	}
	test_sel = dev_context->bist_trigger_info;
	cbus_mode = si_mhl_tx_drv_get_cbus_mode(dev_context);
	if (test_sel == 0) {
		MHL_TX_DBG_ERR("%sNo test selected%s\n",
				ANSI_ESC_RED_TEXT,
				ANSI_ESC_RESET_TEXT)
		si_mhl_tx_bist_cleanup(dev_context);
		return;
	}

	if (test_sel & BIST_TRIGGER_IMPEDANCE_TEST) {
		MHL_TX_DBG_ERR("\n")
		if (0 ==
		    si_mhl_tx_drv_start_impedance_bist((struct drv_hw_context *)
						       &dev_context->
						       drv_context,
						       &dev_context->
						       bist_setup)) {
			MHL_TX_DBG_ERR("\n")
		}
	} else {
		MHL_TX_DBG_ERR("\n")
		if (cbus_mode < CM_TRANSITIONAL_TO_eCBUS_S_CAL_BIST) {
			MHL_TX_DBG_ERR
			    ("Cannot initiate eCBUS-S BIST when CBUS mode is"
			     " %s\n",
			     si_mhl_tx_drv_get_cbus_mode_str(cbus_mode))
			si_mhl_tx_bist_cleanup(dev_context);
			return;
		}
		if (e_cbus_d_sel &&
			cbus_mode < CM_TRANSITIONAL_TO_eCBUS_D_CAL_BIST) {
			MHL_TX_DBG_ERR
			    ("Cannot initiate eCBUS-D BIST when CBUS mode is"
			     " %s\n",
			     si_mhl_tx_drv_get_cbus_mode_str(cbus_mode))
			si_mhl_tx_bist_cleanup(dev_context);
			return;
		}
		if (test_sel & BIST_TRIGGER_ECBUS_TX_RX_MASK) {
			MHL_TX_DBG_ERR("active\n")
			si_mhl_tx_drv_start_ecbus_bist(
				(struct drv_hw_context *)
				&dev_context->drv_context,
				&dev_context->bist_setup);
		}
		if (test_sel & BIST_TRIGGER_AVLINK_RX) {
			MHL_TX_DBG_ERR("active\n")
			si_mhl_tx_drv_start_avlink_bist(dev_context,
				&dev_context->bist_setup);
		}
	}
}

/*
API for CTS tester.
	si_mhl_tx_execute_bist
Parameters:
	dev_context: pointer to device context.
	setup_info:  pointer to a structure containing
			a complete abstraction of a BIST_SETUP
			BURST_ID packet along with parameters
			for BIST_TRIGGER and BIST_REQUEST_STAT and
			a run-time adjustable value for T_BIST_MODE_DOWN
			whose default is the maximum value of 5 seconds.
			Parameters for BIST_SETUP are converted from the
			abstraction to the BIST_SETUP WRITE_BURST format
			just before sending the WRITE_BURST.
*/
void si_mhl_tx_execute_bist(struct mhl_dev_context *dev_context,
			struct bist_setup_info *setup_info)
{
	dev_context->misc_flags.flags.bist_role_TE = 1;
	if (invalid_bist_te_parms(dev_context, setup_info)) {
		MHL_TX_DBG_ERR("\n");
	} else {
		dev_context->bist_setup = *setup_info;
		si_mhl_tx_drv_switch_cbus_mode((struct drv_hw_context *)
			&dev_context->drv_context,
			CM_oCBUS_PEER_IS_MHL3_BIST_SETUP);
	}
}
/*
 * si_mhl_tx_process_events
 * This internal function is called at the end of interrupt processing.  It's
 * purpose is to process events detected during the interrupt.  Some events
 * are internally handled here but most are handled by a notification to
 * interested applications.
 */
void si_mhl_tx_process_events(struct mhl_dev_context *dev_context)
{
	uint8_t rapk_status;
	enum cbus_mode_e cbus_mode;

	cbus_mode = si_mhl_tx_drv_get_cbus_mode(dev_context);
	/* Make sure any events detected during the interrupt are processed. */
	si_mhl_tx_drive_states(dev_context);

	if (dev_context->mhl_connection_event) {
		MHL_TX_DBG_INFO("mhl_connection_event\n");

		/* Consume the message */
		dev_context->mhl_connection_event = false;

		/*
		 * Let interested apps know about the connection state change
		 */
		mhl_event_notify(dev_context, dev_context->mhl_connected,
			dev_context->dev_cap_cache.mdc.featureFlag, NULL);

		/* If connection has been lost, reset all state flags. */
		if (MHL_TX_EVENT_DISCONNECTION == dev_context->mhl_connected) {
			MHL_TX_DBG_WARN("MHL Disconnect Event. Reset states\n");
			si_mhl_tx_reset_states(dev_context);
		}

		else if (MHL_TX_EVENT_CONNECTION ==
			dev_context->mhl_connected) {

			/* queue up all three in this order to
			   indicate MHL 3.0 version according to spec. */

#ifdef FORCE_OCBUS_FOR_ECTS
			/* This compile option is always enabled.
			 * It is intended to help identify code deletion by
			 * adopters who do not need this feauture. The control
			 * for forcing oCBUS works by using module parameter
			 * below. Peer version is forced to 2.0 allowing 8620
			 * to treat the sink as if it is MHL 2.0 device and as
			 * a result never switch cbus to MHL3 eCBUS.
			 */
			if (force_ocbus_for_ects) {
				MHL_TX_DBG_ERR("%sQueue DCAP_RDY, DCAP_CHG%s\n",
					       ANSI_ESC_GREEN_TEXT,
					       ANSI_ESC_RESET_TEXT);
				si_mhl_tx_set_status(dev_context, false,
					MHL_STATUS_REG_CONNECTED_RDY,
					MHL_STATUS_DCAP_RDY);
				si_mhl_tx_set_int(dev_context, MHL_RCHANGE_INT,
					MHL_INT_DCAP_CHG, 1);
			} else {
				MHL_TX_DBG_WARN("%sQueue VERSION_STAT,DCAP_RDY"
					", DCAP_CHG%s\n", ANSI_ESC_GREEN_TEXT,
					ANSI_ESC_RESET_TEXT);
				si_mhl_tx_set_status(dev_context, false,
					MHL_STATUS_REG_VERSION_STAT,
					MHL_VERSION);
				si_mhl_tx_set_status(dev_context, false,
					MHL_STATUS_REG_CONNECTED_RDY,
					MHL_STATUS_DCAP_RDY |
					MHL_STATUS_XDEVCAPP_SUPP |
					((DEVCAP_VAL_DEV_CAT &
					(MHL_DEV_CATEGORY_PLIM2_0 |
					MHL_DEV_CATEGORY_POW_BIT)) >> 2)
				    );
				si_mhl_tx_set_int(dev_context, MHL_RCHANGE_INT,
						  MHL_INT_DCAP_CHG, 1);
			}
#else
			MHL_TX_DBG_ERR
			    ("%sQueue VERSION_STAT, DCAP_RDY, DCAP_CHG%s\n",
			     ANSI_ESC_GREEN_TEXT, ANSI_ESC_RESET_TEXT);
			si_mhl_tx_set_status(dev_context, false,
					     MHL_STATUS_REG_VERSION_STAT,
					     MHL_VERSION);
			si_mhl_tx_set_status(dev_context, false,
					     MHL_STATUS_REG_CONNECTED_RDY,
					     MHL_STATUS_DCAP_RDY |
					     MHL_STATUS_XDEVCAPP_SUPP |
					     ((DEVCAP_VAL_DEV_CAT &
					       (MHL_DEV_CATEGORY_PLIM2_0 |
						MHL_DEV_CATEGORY_POW_BIT)
					      ) >> 2)
			    );
			si_mhl_tx_set_int(dev_context, MHL_RCHANGE_INT,
					  MHL_INT_DCAP_CHG, 1);
#endif
			/*
			 * Start timer here to circumvent issue of not getting
			 * DCAP_RDY. Use timeout durations of 7 seconds or more
			 * to distinguish between non-compliant dongles and the
			 * CBUS CTS tester.
			 */
			switch (cbus_mode) {
			case CM_oCBUS_PEER_VERSION_PENDING_BIST_SETUP:
			case CM_oCBUS_PEER_VERSION_PENDING_BIST_STAT:
				/* stop rather than start the timer
				   for these cases
				*/
				mhl_tx_stop_timer(dev_context,
					dev_context->dcap_rdy_timer);
				mhl_tx_stop_timer(dev_context,
					dev_context->dcap_chg_timer);
				break;
			default:
				mhl_tx_start_timer(dev_context,
					dev_context->dcap_rdy_timer, 7000);
			}
		}
	} else if (dev_context->msc_msg_arrived) {

		MHL_TX_DBG_INFO("MSC MSG <%02X, %02X>\n",
				dev_context->msc_msg_sub_command,
				dev_context->msc_msg_data);

		/* Consume the message */
		dev_context->msc_msg_arrived = false;

		cbus_mode = si_mhl_tx_drv_get_cbus_mode(dev_context);

		/*
		 * Map MSG sub-command to an event ID
		 */
		switch (dev_context->msc_msg_sub_command) {
		case MHL_MSC_MSG_RAP:
			/*
			 * RAP messages are fully handled here.
			 */
			if (dev_context->
			    mhl_flags & MHL_STATE_APPLICATION_RAP_BUSY) {
				rapk_status = MHL_RAPK_BUSY;
			} else {
				rapk_status = MHL_RAPK_NO_ERR;
			}
			dev_context->rap_in_sub_command =
			    dev_context->msc_msg_data;

			if (MHL_RAP_POLL == dev_context->msc_msg_data) {
				/* just do the ack */
			} else if (MHL_RAP_CONTENT_ON ==
				   dev_context->msc_msg_data) {
				MHL_TX_DBG_ERR("Got RAP{CONTENT_ON}\n");
				dev_context->misc_flags.flags.rap_content_on =
				    true;
				si_mhl_tx_drv_content_on(
					(struct drv_hw_context *)
					&dev_context->drv_context);
			} else if (MHL_RAP_CONTENT_OFF ==
				   dev_context->msc_msg_data) {
				MHL_TX_DBG_ERR("Got RAP{CONTENT_OFF}\n");
				if (dev_context->misc_flags.flags.
				    rap_content_on) {
					dev_context->misc_flags.flags.
					    rap_content_on = false;
					si_mhl_tx_drv_content_off(
						(struct drv_hw_context *)
						&dev_context->drv_context);
				}
			} else if (MHL_RAP_CBUS_MODE_DOWN ==
				   dev_context->msc_msg_data) {
				MHL_TX_DBG_ERR("Got RAP{CBUS_MODE_DOWN}\n");
			} else if (MHL_RAP_CBUS_MODE_UP ==
				   dev_context->msc_msg_data) {
				MHL_TX_DBG_ERR("Got RAP{CBUS_MODE_UP}\n");
				mhl_tx_stop_timer(dev_context,
						  dev_context->
						  cbus_mode_up_timer);
			} else {
				MHL_TX_DBG_ERR("Unrecognized RAP code: 0x%02x "
					       "received\n",
					       dev_context->msc_msg_data);
				rapk_status = MHL_RAPK_UNRECOGNIZED;
			}

			/* Always RAPK to the peer */
			si_mhl_rapk_send(dev_context, rapk_status);

			if (rapk_status == MHL_RAPK_NO_ERR)
				mhl_event_notify(dev_context,
						 MHL_TX_EVENT_RAP_RECEIVED,
						 dev_context->msc_msg_data,
						 NULL);
			break;

		case MHL_MSC_MSG_RCP:
			/*
			 * If we get a RCP key that we do NOT support,
			 * send back RCPE. Do not notify app layer.
			 */
			if (rcpSupportTable
			    [dev_context->msc_msg_data & MHL_RCP_KEY_ID_MASK].
			    rcp_support & MHL_LOGICAL_DEVICE_MAP) {
				mhl_event_notify(dev_context,
						 MHL_TX_EVENT_RCP_RECEIVED,
						 dev_context->msc_msg_data,
						 NULL);
			} else {
				/* Save keycode to send a RCPK after RCPE. */
				dev_context->msc_save_rcp_key_code =
				    dev_context->msc_msg_data;
				si_mhl_tx_rcpe_send(dev_context,
						    RCPE_INEFFECTIVE_KEY_CODE);
			}
			break;

		case MHL_MSC_MSG_RCPK:
			mhl_event_notify(dev_context,
					 MHL_TX_EVENT_RCPK_RECEIVED,
					 dev_context->msc_msg_data, NULL);
			break;

		case MHL_MSC_MSG_RCPE:
			mhl_event_notify(dev_context,
					 MHL_TX_EVENT_RCPE_RECEIVED,
					 dev_context->msc_msg_data, NULL);
			break;

		case MHL_MSC_MSG_UCP:
			/*
			 * Save key code so that we can send an UCPE message in
			 * case the UCP key code is rejected by the host
			 * application.
			 */
			dev_context->msc_save_ucp_key_code =
			    dev_context->msc_msg_data;
			mhl_event_notify(dev_context, MHL_TX_EVENT_UCP_RECEIVED,
					 dev_context->msc_save_ucp_key_code,
					 NULL);
			break;

		case MHL_MSC_MSG_UCPK:
			mhl_event_notify(dev_context,
					 MHL_TX_EVENT_UCPK_RECEIVED,
					 dev_context->msc_msg_data, NULL);
			break;

		case MHL_MSC_MSG_UCPE:
			mhl_event_notify(dev_context,
					 MHL_TX_EVENT_UCPE_RECEIVED,
					 dev_context->msc_msg_data, NULL);
			break;
#if (INCLUDE_RBP == 1)
		case MHL_MSC_MSG_RBP:
			/*
			 * Save button code so that we can send an RBPE message
			 * in case the RBP button code is rejected by the host
			 * application.
			 */
			dev_context->msc_save_rbp_button_code =
			    dev_context->msc_msg_data;
			mhl_event_notify(dev_context, MHL_TX_EVENT_RBP_RECEIVED,
					 dev_context->msc_save_rbp_button_code,
					 NULL);
			break;

		case MHL_MSC_MSG_RBPK:
			mhl_event_notify(dev_context,
					 MHL_TX_EVENT_RBPK_RECEIVED,
					 dev_context->msc_msg_data, NULL);
			break;

		case MHL_MSC_MSG_RBPE:
			mhl_event_notify(dev_context,
					 MHL_TX_EVENT_RBPE_RECEIVED,
					 dev_context->msc_msg_data, NULL);
			break;
#endif
		case MHL_MSC_MSG_RAPK:
			mhl_tx_stop_timer(dev_context,
				dev_context->t_rap_max_timer);
			/* the only RAP commands that we send are
			 * CBUS_MODE_UP and CBUS_MODE_DOWN.
			 */
			if (MHL_RAP_CBUS_MODE_DOWN ==
			    dev_context->msc_msg_last_data) {
				MHL_TX_DBG_ERR
				    ("Got RAPK{%s} for RAP{CBUS_MODE_DOWN}\n",
				     rapk_error_code_string[dev_context->
							    msc_msg_data &
							    0x03]);
				si_mhl_tx_drv_switch_cbus_mode(
					(struct drv_hw_context *)
					&dev_context->drv_context,
					CM_oCBUS_PEER_IS_MHL3);

			} else if (MHL_RAP_CBUS_MODE_UP ==
				   dev_context->msc_msg_last_data) {
				enum cbus_mode_e cbus_mode;
				cbus_mode =
				si_mhl_tx_drv_get_cbus_mode(dev_context);
				MHL_TX_DBG_ERR
				    ("Got RAPK{%s}\n",
				     rapk_error_code_string[dev_context->
							    msc_msg_data &
							    0x03]);

				if (MHL_RAPK_NO_ERR ==
				    dev_context->msc_msg_data) {

					/* CBUS Mode switch to eCBUS */
					si_mhl_tx_drv_switch_cbus_mode(
						(struct drv_hw_context *)
						&dev_context->drv_context,
						CM_eCBUS_S);

				} else if (MHL_RAPK_BUSY ==
					   dev_context->msc_msg_data) {
					if (CM_oCBUS_PEER_IS_MHL3 ==
						cbus_mode) {
						MHL_TX_DBG_ERR(
							"starting timer for "
							"CBUS_MODE_UP\n")
						mhl_tx_start_timer(dev_context,
							dev_context->
							cbus_mode_up_timer,
							100);
					}

				} else {
					/*
					 * Nothing to do for
					 * MHL_RAPK_UNRECOGNIZED,
					 * MHL_RAPK_UNSUPPORTED
					 */
				}

			} else {
				MHL_TX_DBG_ERR("Got RAPK for RAP "
					"cmd: %02x,err_code: %02x\n",
					dev_context->msc_msg_last_data,
					dev_context->msc_msg_data);
					/* post status */
					dev_context->rap_out_status =
						dev_context->msc_msg_data;
			}
			break;

		case MHL_MSC_MSG_RHID:
		case MHL_MSC_MSG_RHIDK:
#if (INCLUDE_HID == 1)
			MHL_TX_DBG_WARN("Received MSC_MSG_RHID/K from sink\n");
			mhl_tx_hid_host_negotiation(dev_context);
#endif
			break;

		case MHL_MSC_MSG_BIST_READY:
			MHL_TX_DBG_INFO("Got BIST_READY\n");
			if (CM_oCBUS_PEER_IS_MHL3_BIST_SETUP_SENT ==
				cbus_mode) {
				bool status;
				si_mhl_tx_drv_switch_cbus_mode(
				    (struct drv_hw_context *)
				    &dev_context->drv_context,
				    CM_oCBUS_PEER_IS_MHL3_BIST_SETUP_PEER_READY
				    );
				mhl_event_notify(dev_context,
					MHL_TX_EVENT_BIST_READY_RECEIVED,
					dev_context->msc_msg_data,
					NULL);
				status = si_mhl_tx_bist_trigger(
					dev_context, dev_context->
					bist_setup.bist_trigger_parm);
				if (BIST_STATUS_NO_ERROR != status) {
					si_mhl_tx_drv_switch_cbus_mode(
					    (struct drv_hw_context *)
					    &dev_context->drv_context,
					    CM_oCBUS_PEER_IS_MHL3_BIST_STAT);

				}
			}
			break;

		case MHL_MSC_MSG_BIST_TRIGGER:
			if (CM_oCBUS_PEER_IS_MHL3_BIST_STAT == cbus_mode) {
				MHL_TX_DBG_ERR("another BIST_TRIGGER\n");
				si_mhl_tx_drv_switch_cbus_mode(
				    (struct drv_hw_context *)
				    &dev_context->drv_context,
				CM_oCBUS_PEER_IS_MHL3_BIST_SETUP_PEER_READY);
			} else if (CM_oCBUS_PEER_IS_MHL3_BIST_SETUP_PEER_READY
				!= cbus_mode) {
				MHL_TX_DBG_ERR
				    ("Got BIST_TRIGGER when CBUS mode is %s\n",
				     si_mhl_tx_drv_get_cbus_mode_str
				     (cbus_mode));
				break;
			}

			MHL_TX_DBG_ERR("Got BIST_TRIGGER\n");
			dev_context->bist_setup.bist_trigger_parm =
			dev_context->bist_trigger_info =
			    dev_context->msc_msg_data;
			if (dev_context->msc_msg_data ==
			    BIST_TRIGGER_IMPEDANCE_TEST) {
				initiate_bist_test(dev_context);
			} else if (dev_context->msc_msg_data != 0) {
				cbus_mode = dev_context->msc_msg_data &
					BIST_TRIGGER_TEST_E_CBUS_D ?
					CM_eCBUS_D_BIST : CM_eCBUS_S_BIST;
				MHL_TX_DBG_ERR("\n")
				determine_bist_timeout_value(dev_context);
				if (1 == si_mhl_tx_drv_switch_cbus_mode(
					(struct drv_hw_context *)
					&dev_context->drv_context, cbus_mode))
						MHL_TX_DBG_ERR("Wait CoC Cal\n")
			}
			break;

		case MHL_MSC_MSG_BIST_STOP:
			if (cbus_mode < CM_eCBUS_S_AV_BIST) {
				MHL_TX_DBG_ERR
				    ("Got BIST_STOP when CBUS mode is %s\n",
				     si_mhl_tx_drv_get_cbus_mode_str
				     (cbus_mode));
				break;
			}

			MHL_TX_DBG_INFO("Got BIST_STOP\n");

			mhl_tx_stop_timer(dev_context,
					  dev_context->bist_timer);
			dev_context->bist_stat.avlink_stat = 0;
			/* bist timer always runs, even for
				bist_timeout_value == 0 (infinite)
			*/
			si_mhl_tx_bist_cleanup(dev_context);
			break;

		case MHL_MSC_MSG_BIST_REQUEST_STAT:
			if (cbus_mode != CM_oCBUS_PEER_IS_MHL3_BIST_STAT) {
				MHL_TX_DBG_ERR("Got BIST_REQUEST_STAT when "
					"CBUS mode is %s\n",
					si_mhl_tx_drv_get_cbus_mode_str
					(cbus_mode));
				break;
			}

			MHL_TX_DBG_ERR("Got BIST_REQUEST_STAT\n");

			if (dev_context->msc_msg_data) {
				MHL_TX_DBG_ERR("Send BIST status\n");
				send_bist_status(dev_context);
			}
			break;

		default:
			MHL_TX_DBG_WARN("Unexpected MSC message "
					"sub-command code: 0x%02x received!\n",
					dev_context->msc_msg_sub_command);
			break;
		}
	}
}

static struct cbus_req *read_devcap_done(struct mhl_dev_context *dev_context,
				struct cbus_req *req, uint8_t data1);

bool si_mhl_tx_read_devcap(struct mhl_dev_context *dev_context)
{
	struct cbus_req *req;

	MHL_TX_DBG_INFO("called\n");

	req = get_free_cbus_queue_entry(dev_context);
	if (req == NULL) {
		MHL_TX_GENERIC_DBG_PRINT(-1, "CBUS free queue exhausted\n")
		    return false;
	}

	req->retry_count = 2;
	req->command = MHL_READ_DEVCAP;
	req->reg = 0;
	req->reg_data = 0;	/* do this to avoid confusion */
	req->completion = read_devcap_done;

	queue_cbus_transaction(dev_context, req);

	return true;
}

bool si_mhl_tx_read_devcap_reg(struct mhl_dev_context *dev_context,
			       uint8_t offset)
{
	struct cbus_req *req;

	MHL_TX_DBG_INFO("called\n");

	req = get_free_cbus_queue_entry(dev_context);
	if (req == NULL) {
		MHL_TX_GENERIC_DBG_PRINT(-1, "CBUS free queue exhausted\n")
		    return false;
	}

	req->retry_count = 2;
	req->command = MHL_READ_DEVCAP_REG;
	req->reg = offset;
	req->reg_data = 0;	/* do this to avoid confusion */

	queue_cbus_transaction(dev_context, req);

	return true;
}

static struct cbus_req *read_xdevcap_done(struct mhl_dev_context *dev_context,
				struct cbus_req *req, uint8_t data1);

bool si_mhl_tx_read_xdevcap_impl(struct mhl_dev_context *dev_context,
				 const char *func_name, int line_num)
{
	struct cbus_req *req;

	MHL_TX_DBG_INFO("called from %s:%d\n", func_name, line_num);

	req = get_free_cbus_queue_entry(dev_context);
	if (req == NULL) {
		MHL_TX_GENERIC_DBG_PRINT(-1, "CBUS free queue exhausted\n")
		    return false;
	}

	req->retry_count = 2;
	req->command = MHL_READ_XDEVCAP;
	req->reg = 0;
	req->reg_data = 0;	/* do this to avoid confusion */
	req->completion = read_xdevcap_done;

	queue_cbus_transaction(dev_context, req);

	return true;
}

static struct cbus_req *read_xdevcap_reg_done(
				struct mhl_dev_context *dev_context,
				struct cbus_req *req, uint8_t data1);
#define si_mhl_tx_read_xdevcap(dev_context) \
	si_mhl_tx_read_xdevcap_impl(dev_context, __func__, __LINE__)

bool si_mhl_tx_read_xdevcap_reg_impl(struct mhl_dev_context *dev_context,
		uint8_t reg_addr, const char *func_name, int line_num)
{
	struct cbus_req *req;

	MHL_TX_DBG_WARN("called from %s:%d\n", func_name, line_num);

	req = get_free_cbus_queue_entry(dev_context);
	if (req == NULL) {
		MHL_TX_GENERIC_DBG_PRINT(-1, "CBUS free queue exhausted\n")
		    return false;
	}

	req->retry_count = 2;
	req->command = MHL_READ_XDEVCAP_REG;
	req->reg = reg_addr;
	req->reg_data = 0;	/* avoid confusion */
	req->completion = read_xdevcap_reg_done;
	queue_cbus_transaction(dev_context, req);

	return true;
}

#define si_mhl_tx_read_xdevcap_reg(dev_context, offset) \
	si_mhl_tx_read_xdevcap_reg_impl(dev_context, offset, __func__, \
	__LINE__)

struct cbus_req *rcpk_done(struct mhl_dev_context *dev_context,
				struct cbus_req *req, uint8_t data1)
{
	MHL_TX_DBG_ERR("\n")
	return req;
}

bool si_mhl_tx_rcpk_send(struct mhl_dev_context *dev_context,
			 uint8_t rcp_key_code)
{
	bool status;

	status = si_mhl_tx_send_msc_msg(dev_context, MHL_MSC_MSG_RCPK,
					rcp_key_code, rcpk_done);
	if (status)
		si_mhl_tx_drive_states(dev_context);

	return status;
}

static struct cbus_req *read_edid_done(struct mhl_dev_context *dev_context,
				struct cbus_req *req, uint8_t data1);
/*
 * si_mhl_tx_request_first_edid_block
 *
 * This function initiates a CBUS command to read the specified EDID block.
 * Returns true if the command was queued successfully.
 */
void si_mhl_tx_request_first_edid_block(struct mhl_dev_context *dev_context)
{
	MHL_TX_DBG_INFO("tag: EDID active: %d\n",
			dev_context->misc_flags.flags.edid_loop_active);
	if (!dev_context->misc_flags.flags.edid_loop_active) {
		struct cbus_req *req;

		req = get_free_cbus_queue_entry(dev_context);
		if (req == NULL) {
			MHL_TX_DBG_INFO("couldn't get free cbus req\n");
		} else {
			dev_context->misc_flags.flags.edid_loop_active = 1;
			MHL_TX_DBG_INFO("tag: EDID active: %d\n",
					dev_context->misc_flags.flags.
					edid_loop_active);

			/* Send MHL_READ_EDID_BLOCK command */
			req->retry_count = 2;
			req->command = MHL_READ_EDID_BLOCK;
			req->burst_offset = 0;	/* block number */
			req->msg_data[0] = 0;	/* avoid confusion */
			req->completion = read_edid_done;

			queue_cbus_transaction(dev_context, req);

			si_mhl_tx_drive_states(dev_context);
		}
	}
}

/*
	si_mhl_tx_ecbus_speeds_done
*/

static void si_mhl_tx_ecbus_speeds_done(struct mhl_dev_context *dev_context)
{
	struct drv_hw_context *hw_context =
	    (struct drv_hw_context *)&dev_context->drv_context;
	enum cbus_mode_e cbus_mode;
	cbus_mode = si_mhl_tx_drv_get_cbus_mode(dev_context);
	/*
	 * Set default eCBUS virtual channel slot assignments
	 * based on the capabilities of both the transmitter
	 * and receiver.
	 */
	if ((dev_context->xdev_cap_cache.mxdc.ecbus_speeds &
	     MHL_XDC_ECBUS_D_150) &&
	    si_mhl_tx_drv_support_e_cbus_d(
		    (struct drv_hw_context *)
		    &dev_context->drv_context)) {
		dev_context->virt_chan_slot_counts[TDM_VC_CBUS1] = 1;
		dev_context->virt_chan_slot_counts[TDM_VC_E_MSC] = 39;
		dev_context->virt_chan_slot_counts[TDM_VC_T_CBUS] = 160;
	} else {
		dev_context->virt_chan_slot_counts[TDM_VC_CBUS1] = 1;
		dev_context->virt_chan_slot_counts[TDM_VC_E_MSC] = 4;
		dev_context->virt_chan_slot_counts[TDM_VC_T_CBUS] = 20;
	}
	memcpy(hw_context->tdm_virt_chan_slot_counts,
	       dev_context->virt_chan_slot_counts,
	       sizeof(hw_context->tdm_virt_chan_slot_counts));


	if (cbus_mode <= CM_oCBUS_PEER_IS_MHL3) {
		bool status;
		enum bist_cmd_status bcs;
		bool send_bist_setup = true;
		status = si_mhl_tx_set_status(
			dev_context, true,
			MHL_XSTATUS_REG_CBUS_MODE,
			MHL_XDS_ECBUS_S |
			MHL_XDS_SLOT_MODE_8BIT);

		switch (cbus_mode) {
		case CM_oCBUS_PEER_IS_MHL3_BIST_SETUP:
			MHL_TX_DBG_ERR("\n")
			if (BIST_TRIGGER_ECBUS_AV_LINK_MASK &
				dev_context->bist_setup.bist_trigger_parm) {
				MHL_TX_DBG_ERR("\n")
				if (dev_context->
					misc_flags.flags.bist_role_TE) {
					MHL_TX_DBG_ERR("\n")
					send_bist_setup = false;
					setup_sans_cbus1(dev_context);
				}
			}
			if (send_bist_setup) {
				MHL_TX_DBG_ERR(
				    "%sissuing BIST_SETUP%s\n",
					ANSI_ESC_GREEN_TEXT,
					ANSI_ESC_RESET_TEXT)
				bcs = si_mhl_tx_bist_setup(dev_context,
					&dev_context->bist_setup);
			}
			MHL_TX_DBG_ERR("status: %d\n", status);
			break;
		case CM_oCBUS_PEER_IS_MHL3_BIST_STAT:

			if (dev_context->misc_flags.flags.bist_role_TE) {
				if ((BIST_TRIGGER_ECBUS_AV_LINK_MASK |
					BIST_TRIGGER_E_CBUS_RX |
					BIST_TRIGGER_IMPEDANCE_TEST) &
					dev_context->bist_trigger_info) {
					MHL_TX_DBG_ERR(
					    "%sissuing BIST_REQUEST_STAT%s\n",
						ANSI_ESC_GREEN_TEXT,
						ANSI_ESC_RESET_TEXT)
					status = si_mhl_tx_bist_request_stat(
					    dev_context,
					    dev_context->
						bist_setup.bist_stat_parm);
					MHL_TX_DBG_ERR("status: %d\n", status);
					break;
				}
				MHL_TX_DBG_ERR
					("%sLocal BIST results%s"
					" eCBUS TX:%s0x%06x%s\n",
					ANSI_ESC_GREEN_TEXT,
					ANSI_ESC_RESET_TEXT,
					ANSI_ESC_GREEN_TEXT,
					dev_context->bist_stat.
						e_cbus_prev_local_stat,
					ANSI_ESC_RESET_TEXT)
			} else if (BIST_TRIGGER_E_CBUS_RX &
				    dev_context->bist_trigger_info) {
				MHL_TX_DBG_ERR("%swaiting for "
					"BIST_REQUEST_STAT "
					"from sink TE%s\n",
					ANSI_ESC_GREEN_TEXT,
					ANSI_ESC_RESET_TEXT)
				break;
			}

			dev_context->bist_trigger_info = 0x00;
			/* fall through here to do cbus_mode_up */
		default:
			/* issue RAP(CBUS_MODE_UP)
			 * RAP support is required for MHL3 and
			 * later devices
			 */
			MHL_TX_DBG_ERR(
				"%sissuing CBUS_MODE_UP%s\n",
				ANSI_ESC_GREEN_TEXT,
				ANSI_ESC_RESET_TEXT)
		#ifdef BIST_DONE_DEBUG
			si_dump_important_regs(
				(struct drv_hw_context *)
				&dev_context->drv_context);
		#endif
			status = si_mhl_tx_rap_send(
				dev_context,
				MHL_RAP_CBUS_MODE_UP);
			if (status)
				si_mhl_tx_drive_states(dev_context);
		}
	}
}

static struct cbus_req *read_xdevcap_reg_done(
				struct mhl_dev_context *dev_context,
				struct cbus_req *req, uint8_t data1)
{
	MHL_TX_DBG_INFO("mhl: peer XDEVCAP[0x%02x] data1:0x%02x\n",
			req->reg, data1);
	dev_context->xdev_cap_cache.
	    xdevcap_cache[XDEVCAP_OFFSET(req->reg)] = data1;

	if (XDEVCAP_ADDR_ECBUS_SPEEDS == req->reg)
		si_mhl_tx_ecbus_speeds_done(dev_context);

	return req;
}


static struct cbus_req *read_xdevcap_done(struct mhl_dev_context *dev_context,
				struct cbus_req *req, uint8_t data1)
{
	int i;
	union MHLXDevCap_u old_xdevcap, xdevcap_changes;
	uint8_t roles;

	old_xdevcap = dev_context->xdev_cap_cache;
	MHL_TX_DBG_INFO("mhl: peer XDEVCAP data1:0x%02x\n", data1);
	si_mhl_tx_read_xdevcap_fifo((struct drv_hw_context *)
		&dev_context->drv_context,
		&dev_context->xdev_cap_cache);

	/*
	 *  Generate a change mask between the old and new devcaps
	 */
	for (i = 0; i < XDEVCAP_OFFSET(XDEVCAP_LIMIT); ++i) {
		xdevcap_changes.xdevcap_cache[i]
		    = dev_context->xdev_cap_cache.xdevcap_cache[i]
		    ^ old_xdevcap.xdevcap_cache[i];
		if (xdevcap_changes.xdevcap_cache[i]) {
			MHL_TX_DBG_INFO("XDEVCAP[%d] changed from "
				"0x%02x to 0x%02x\n", i,
				old_xdevcap.xdevcap_cache[i],
				dev_context->xdev_cap_cache.
				xdevcap_cache[i]);
		}
	}

	roles = XDEVCAP_OFFSET(XDEVCAP_ADDR_ECBUS_DEV_ROLES);
	MHL_TX_DBG_INFO
	    ("mhl: xdevcap_changes.xdevcap_cache[%d]= %02X\n", roles,
	     xdevcap_changes.xdevcap_cache[roles]);
	if (xdevcap_changes.xdevcap_cache[roles]) {
		roles =
		    dev_context->xdev_cap_cache.xdevcap_cache[roles];
		MHL_TX_DBG_INFO
		    ("mhl: XDEVCAP_ADDR_ECBUS_DEV_ROLES= %02X\n",
		     roles);
		/*
		 * If sink supports HID_DEVICE,
		 * tell it we want to be the host
		 */
#if (INCLUDE_HID)
		if (roles & MHL_XDC_HID_DEVICE) {
			MHL_TX_DBG_INFO("mhl: calling "
				"mhl_tx_hid_host_role_request()\n");
			mhl_tx_hid_host_role_request(dev_context,
				MHL_RHID_REQUEST_HOST);
		}
#endif
	}

	si_mhl_tx_read_devcap(dev_context);

	return req;
}

static struct cbus_req *read_devcap_done(struct mhl_dev_context *dev_context,
				struct cbus_req *req, uint8_t data1)
{
	uint8_t temp;
	int i;
	union MHLDevCap_u old_devcap, devcap_changes;

	old_devcap = dev_context->dev_cap_cache;
	si_mhl_tx_read_devcap_fifo((struct drv_hw_context *)
		&dev_context->drv_context,
		&dev_context->dev_cap_cache);

	if (dev_context->peer_mhl3_version < 0x30) {
		struct drv_hw_context *hw_context =
		    (struct drv_hw_context *)&dev_context->drv_context;
		dev_context->peer_mhl3_version =
		    dev_context->dev_cap_cache.mdc.mhl_version;
		hw_context->cbus_mode = CM_oCBUS_PEER_IS_MHL1_2;
		si_set_cbus_mode_leds(CM_oCBUS_PEER_IS_MHL1_2);
#ifdef	FORCE_OCBUS_FOR_ECTS
		/* This compile option is always enabled.
		 * It is intended to help identify code deletion by
		 * adopters who do not need this feauture. The control
		 * for forcing oCBUS works by using module parameter
		 * below. Peer version is forced to 2.0 allowing 8620
		 * to treat the sink as if it is MHL 2.0 device and as
		 * a result never switch cbus to MHL3 eCBUS.
		 */
		{
			if (force_ocbus_for_ects) {
				/* todo: what if the peer is MHL 1.x? */
				dev_context->peer_mhl3_version = 0x20;
			}
		}
#endif
	}

	MHL_TX_DBG_WARN("DEVCAP->MHL_VER = %02x\n",
		       dev_context->peer_mhl3_version);
	/*
	 *  Generate a change mask between the old and new devcaps
	 */
	for (i = 0; i < sizeof(old_devcap); ++i) {
		devcap_changes.devcap_cache[i]
		    = dev_context->dev_cap_cache.devcap_cache[i]
		    ^ old_devcap.devcap_cache[i];
	}
	/* look for a change in the pow bit */
	if (MHL_DEV_CATEGORY_POW_BIT & devcap_changes.mdc.
	    deviceCategory) {
		uint8_t param;
		uint8_t index;
		index = dev_context->dev_cap_cache.mdc.deviceCategory
			& MHL_DEV_CATEGORY_PLIM2_0;
		index >>= 5;
		param = dev_context->dev_cap_cache.mdc.deviceCategory
			& MHL_DEV_CATEGORY_POW_BIT;

		if (param) {
			MHL_TX_DBG_WARN("%ssink drives VBUS%s\n",
			ANSI_ESC_GREEN_TEXT, ANSI_ESC_RESET_TEXT);
			/* limit incoming current */
			mhl_tx_vbus_control(VBUS_OFF);
			mhl_tx_vbus_current_ctl(plim_table[index]);
		} else {
			MHL_TX_DBG_WARN("%ssource drives VBUS%s\n",
			ANSI_ESC_YELLOW_TEXT, ANSI_ESC_RESET_TEXT);
			mhl_tx_vbus_control(VBUS_ON);
		}

		/* Inform interested Apps of the MHL power change */
		mhl_event_notify(dev_context, MHL_TX_EVENT_POW_BIT_CHG,
				 param, NULL);
	}

	/* indicate that the DEVCAP cache is up to date. */
	dev_context->misc_flags.flags.have_complete_devcap = true;

	/*
	 * Check to see if any other bits besides POW_BIT have changed
	 */
	devcap_changes.mdc.deviceCategory &=
		~(MHL_DEV_CATEGORY_POW_BIT | MHL_DEV_CATEGORY_PLIM2_0);
	temp = 0;
	for (i = 0; i < sizeof(devcap_changes); ++i)
		temp |= devcap_changes.devcap_cache[i];

	if (temp) {
		if (dev_context->misc_flags.flags.mhl_hpd) {
			MHL_TX_DBG_INFO("Have HPD\n");
			si_mhl_tx_initiate_edid_sequence(
				dev_context->edid_parser_context);
		} else {
			MHL_TX_DBG_INFO("No HPD\n");
		}
	}
	return req;
}

static struct cbus_req *read_edid_done(struct mhl_dev_context *dev_context,
				struct cbus_req *req, uint8_t data1)
{
	si_mhl_tx_drive_states(dev_context);
	MHL_TX_GENERIC_DBG_PRINT(data1 ? -1 : 1, "data1: %d\n", data1)
	    if (0 == data1) {
		dev_context->edid_valid = true;
		si_mhl_tx_handle_atomic_hw_edid_read_complete
		    (dev_context->edid_parser_context);
	}
	dev_context->misc_flags.flags.edid_loop_active = 0;
	MHL_TX_DBG_INFO("tag: EDID active: %d\n",
			dev_context->misc_flags.flags.edid_loop_active);
	return req;
}

static struct cbus_req *write_stat_done(struct mhl_dev_context *dev_context,
				struct cbus_req *req, uint8_t data1)
{
	MHL_TX_DBG_INFO("Sent WRITE_STAT[0x%02x]->0x%02x"
			" miscFlags: %08X\n", req->reg, req->reg_data,
			dev_context->misc_flags.as_uint32);
	if (MHL_STATUS_REG_CONNECTED_RDY == req->reg) {
		if (MHL_STATUS_DCAP_RDY & req->reg_data) {
			dev_context->misc_flags.flags.sent_dcap_rdy =
			    true;

			MHL_TX_DBG_INFO("%sSent DCAP_RDY%s\n",
					ANSI_ESC_GREEN_TEXT,
					ANSI_ESC_RESET_TEXT);
		}
	} else if (MHL_STATUS_REG_LINK_MODE == req->reg) {
		if (MHL_STATUS_PATH_ENABLED & req->reg_data) {
			dev_context->misc_flags.flags.sent_path_en =
			    true;
			MHL_TX_DBG_INFO("FLAGS_SENT_PATH_EN\n");
		}
	}
	return req;
}

static struct cbus_req *write_burst_done(struct mhl_dev_context *dev_context,
				struct cbus_req *req, uint8_t data1)
{
	struct tdm_alloc_burst *tdm_burst;
	tdm_burst = (struct tdm_alloc_burst *)
		req->msg_data;
	MHL_TX_DBG_INFO("MHL_WRITE_BURST\n");
	if (burst_id_VC_CONFIRM
		== BURST_ID(tdm_burst->header.burst_id)) {
		int i;
		int can_reassign = 1;
		MHL_TX_DBG_ERR("VC_CONFIRM done\n");
		for (i = 0; i < VC_MAX; ++i) {
			if (VC_RESPONSE_ACCEPT != tdm_burst->vc_info[i].
				req_resp.channel_size) {
				can_reassign = 0;
			}
		}
	/* changing slot allocations may result in a loss of data; however,
	 * the link will self-synchronize
	 */
		if ((can_reassign)
		    && (0 == si_mhl_tx_drv_set_tdm_slot_allocation(
		    (struct drv_hw_context *)&dev_context->drv_context,
			dev_context->virt_chan_slot_counts, true))) {
			MHL_TX_DBG_ERR("Slots reassigned.\n");
		}
	} else if (burst_id_BIST_RETURN_STAT
		== BURST_ID(tdm_burst->header.burst_id)) {
		bool status;

		MHL_TX_DBG_ERR("%sBIST DONE%s\n",
			ANSI_ESC_YELLOW_TEXT,
			ANSI_ESC_RESET_TEXT)
		/* we already know that were in MHL_BIST */
		si_mhl_tx_drv_switch_cbus_mode((struct drv_hw_context *)
			&dev_context->drv_context,
			CM_oCBUS_PEER_IS_MHL3);

		/* issue RAP(CBUS_MODE_UP)
		 * RAP support is required for MHL3 and
		 * later devices
		 */
		MHL_TX_DBG_ERR(
			"%sissuing CBUS_MODE_UP%s\n",
			ANSI_ESC_GREEN_TEXT,
			ANSI_ESC_RESET_TEXT)
		#ifdef BIST_DONE_DEBUG
			si_dump_important_regs(
				(struct drv_hw_context *)
				&dev_context->drv_context);
		#endif
		status = si_mhl_tx_rap_send(
			dev_context,
			MHL_RAP_CBUS_MODE_UP);
		if (status) {
			si_mhl_tx_drive_states(
				dev_context);
		}
	}
	return req;
}


static struct cbus_req *set_int_done(struct mhl_dev_context *dev_context,
				struct cbus_req *req, uint8_t data1)
{
	MHL_TX_DBG_INFO("MHL_SET_INT\n");
	if (MHL_RCHANGE_INT == req->reg) {
		MHL_TX_DBG_INFO("%sSent MHL_RCHANGE_INT%s\n",
				ANSI_ESC_GREEN_TEXT,
				ANSI_ESC_RESET_TEXT);

		if (MHL3_INT_FEAT_COMPLETE == req->reg_data) {
			MHL_TX_DBG_WARN("%sSent FEAT_COMPLETE%s\n",
				       ANSI_ESC_GREEN_TEXT,
				       ANSI_ESC_RESET_TEXT);
		}
	}
	return req;
}

/*
 * si_mhl_tx_msc_command_done
 *
 * This function is called by the driver to notify completion of last command.
 *
 * It is called in interrupt context to meet some MHL specified timings.
 * Therefore, it should not call app layer and do negligible processing, no
 * printfs.
 */

void si_mhl_tx_msc_command_done(struct mhl_dev_context *dev_context,
				uint8_t data1)
{
	struct cbus_req *req;

	MHL_TX_DBG_INFO("data1 = %02X\n", data1);

	req = dev_context->current_cbus_req;
	if (req == NULL) {
		MHL_TX_DBG_ERR("No message to associate with "
			       "completion notification\n");
		return;
	}
	MHL_TX_DBG_INFO("current command:0x%02x\n", req->command);

	dev_context->current_cbus_req = NULL;

	if (req->status.flags.cancel == true) {
		MHL_TX_DBG_INFO("Canceling request with command 0x%02x\n",
				req->command);

	} else if (MHL_MSC_MSG == req->command) {
		if (dev_context->intr_info.flags & DRV_INTR_MSC_NAK) {

			msleep(1000);
			MHL_TX_DBG_INFO("MSC_NAK, re-trying...\n");
			/*
			 * Request must be retried, so place it back
			 * on the front of the queue.
			 */
			req->status.as_uint8 = 0;
			queue_priority_cbus_transaction(dev_context, req);
			req = NULL;
		} else if (req->completion) {
			MHL_TX_DBG_INFO
			    ("MHL_MSC_MSG sub cmd: 0x%02x data: 0x%02x\n",
			     req->msg_data[0], req->msg_data[1]);
			req = req->completion(dev_context, req, data1);
		} else {
			MHL_TX_DBG_INFO("default\n"
					"\tcommand: 0x%02X\n"
					"\tmsg_data: 0x%02X "
					"msc_msg_last_data: 0x%02X\n",
					req->command,
					req->msg_data[0],
					dev_context->msc_msg_last_data);
		}
	} else if (req->completion) {
		req = req->completion(dev_context, req, data1);
#if 0
	} else if (MHL_READ_XDEVCAP == req->command) {
		req = read_xdevcap_done(dev_context, req, data1);

	} else if (MHL_READ_XDEVCAP_REG == req->command) {
		req = read_xdevcap_reg_done(dev_context, req, data1);
	} else if (MHL_READ_DEVCAP == req->command) {
		req = read_devcap_done(dev_context, req, data1);
	} else if (MHL_READ_EDID_BLOCK == req->command) {
		req = read_edid_done(dev_context, req, data1);
	} else if (MHL_WRITE_STAT == req->command) {
		req = write_stat_done(dev_context, req, data1);
#endif
#if 0
	} else if (MHL_WRITE_BURST == req->command) {
		req = write_burst_done(dev_context, req, data1);
	} else if (MHL_SET_INT == req->command) {
		req = set_int_done(dev_context, req, data1);
#endif
	} else if (MHL_SEND_3D_REQ_OR_FEAT_REQ == req->command) {
		MHL_TX_DBG_ERR("3D_REQ or FEAT_REQ completed\n");
	} else {
		MHL_TX_DBG_INFO("default\n"
				"\tcommand: 0x%02X reg: 0x%02x reg_data: "
				"0x%02x burst_offset: 0x%02x msg_data[0]: "
				"0x%02x msg_data[1]: 0x%02x\n",
				req->command,
				req->reg, req->reg_data,
				req->burst_offset,
				req->msg_data[0], req->msg_data[1]);
	}

	if (req != NULL)
		return_cbus_queue_entry(dev_context, req);

}

void si_mhl_tx_process_vc_assign_burst(struct mhl_dev_context *dev_context,
				       uint8_t *write_burst_data)
{
	struct tdm_alloc_burst *tdm_burst;
	uint8_t idx = 0;
	int8_t save_burst_TDM_VC_E_MSC_idx = -1;
	int8_t save_burst_TDM_VC_T_CBUS_idx = -1;

	tdm_burst = (struct tdm_alloc_burst *)write_burst_data;

	if (0 != calculate_generic_checksum(write_burst_data, 0, 16)) {
		uint8_t i;
		for (i = 0; i < 16; i++)
			MHL_TX_DBG_ERR("0x%02X\n", write_burst_data[i]);

		MHL_TX_DBG_ERR("Bad checksum in virtual channel assign\n");
		return;
	}

	/* The virtual channel assignment in the WRITE_BURST may contain one,
	 * two or three channel allocations
	 */
	if (tdm_burst->num_entries_this_burst > 3) {
		MHL_TX_DBG_ERR("Bad number of assignment requests in "
			"virtual channel assign\n");
		return;
	}

	for (idx = 0; idx < VC_MAX; idx++) {
		dev_context->prev_virt_chan_slot_counts[idx] =
		    dev_context->virt_chan_slot_counts[idx];
	}

	for (idx = 0; idx < tdm_burst->num_entries_this_burst; idx++) {
		switch (tdm_burst->vc_info[idx].feature_id) {
		case FEATURE_ID_E_MSC:
			dev_context->virt_chan_slot_counts[TDM_VC_E_MSC] =
			    tdm_burst->vc_info[idx].req_resp.channel_size;
			save_burst_TDM_VC_E_MSC_idx = idx;
			break;
		case FEATURE_ID_USB:
			dev_context->virt_chan_slot_counts[TDM_VC_T_CBUS] =
			    tdm_burst->vc_info[idx].req_resp.channel_size;
			save_burst_TDM_VC_T_CBUS_idx = idx;
			break;
		default:
			tdm_burst->vc_info[idx].req_resp.channel_size =
			    VC_RESPONSE_BAD_FEATURE_ID;
			break;
		}
	}

	if (si_mhl_tx_drv_set_tdm_slot_allocation
		((struct drv_hw_context *)&dev_context->drv_context,
		dev_context->virt_chan_slot_counts, false)) {

		MHL_TX_DBG_INFO("Source will reject request to assign "
			"CBUS virtual channels\n");

		for (idx = 0; idx < VC_MAX; idx++)
			dev_context->virt_chan_slot_counts[idx] =
			    dev_context->prev_virt_chan_slot_counts[idx];

		if (save_burst_TDM_VC_E_MSC_idx >= 0)
			tdm_burst->vc_info[save_burst_TDM_VC_E_MSC_idx].
			    req_resp.channel_size =
			    VC_RESPONSE_BAD_CHANNEL_SIZE;
		if (save_burst_TDM_VC_T_CBUS_idx >= 0)
			tdm_burst->vc_info[save_burst_TDM_VC_T_CBUS_idx].
			    req_resp.channel_size =
			    VC_RESPONSE_BAD_CHANNEL_SIZE;

	} else {
		if (save_burst_TDM_VC_E_MSC_idx >= 0)
			tdm_burst->vc_info[save_burst_TDM_VC_E_MSC_idx].
			    req_resp.channel_size = VC_RESPONSE_ACCEPT;
		if (save_burst_TDM_VC_T_CBUS_idx >= 0)
			tdm_burst->vc_info[save_burst_TDM_VC_T_CBUS_idx].
			    req_resp.channel_size = VC_RESPONSE_ACCEPT;

	}

	/* Respond back to requester to indicate acceptance or rejection */
	tdm_burst->header.burst_id.high = burst_id_VC_CONFIRM >> 8;
	tdm_burst->header.burst_id.low = (uint8_t) burst_id_VC_CONFIRM;
	tdm_burst->header.checksum = 0;
	tdm_burst->header.checksum =
	    calculate_generic_checksum((uint8_t *) (tdm_burst), 0,
				       sizeof(*tdm_burst));

	si_mhl_tx_request_write_burst(dev_context, 0, sizeof(*tdm_burst),
				      (uint8_t *) (tdm_burst));
	/* the actual assignment will occur when the
		 write_burst is completed*/
}

void si_mhl_tx_process_vc_confirm_burst(struct mhl_dev_context *dev_context,
					uint8_t *write_burst_data)
{
	struct tdm_alloc_burst *tdm_burst;
	bool assign_ok = true;

	tdm_burst = (struct tdm_alloc_burst *)write_burst_data;

	if (0 != calculate_generic_checksum(write_burst_data, 0, 16)) {
		uint8_t i;
		for (i = 0; i < 16; i++)
			MHL_TX_DBG_ERR("0x%02X\n", write_burst_data[i]);

		MHL_TX_DBG_ERR("Bad checksum in virtual channel confirm\n");
		return;
	}

	if (tdm_burst->vc_info[TDM_VC_E_MSC - 1].req_resp.response !=
	    VC_RESPONSE_ACCEPT) {
		MHL_TX_DBG_WARN("Sink rejected request to assign"
				"%d slots to E_MSC virtual channel with status: 0x%02x\n",
				dev_context->
				virt_chan_slot_counts[TDM_VC_E_MSC],
				tdm_burst->vc_info[TDM_VC_E_MSC -
						   1].req_resp.response);
		assign_ok = false;
	}

	if (tdm_burst->vc_info[TDM_VC_T_CBUS - 1].req_resp.response !=
	    VC_RESPONSE_ACCEPT) {
		MHL_TX_DBG_WARN("Sink rejected request to assign"
				"%d slots to T_CBUS virtual channel with status: 0x%02x\n",
				dev_context->
				virt_chan_slot_counts[TDM_VC_T_CBUS],
				tdm_burst->vc_info[TDM_VC_T_CBUS -
						   1].req_resp.response);
		assign_ok = false;
	}

	if (assign_ok) {
		si_mhl_tx_drv_set_tdm_slot_allocation((struct drv_hw_context *)
						      &dev_context->drv_context,
						      dev_context->
						      virt_chan_slot_counts,
						      true);
		MHL_TX_DBG_ERR
		    ("Sink accepted requested virtual channel assignments\n");
	} else {
		dev_context->virt_chan_slot_counts[TDM_VC_E_MSC] =
		    dev_context->prev_virt_chan_slot_counts[TDM_VC_E_MSC];
		dev_context->virt_chan_slot_counts[TDM_VC_T_CBUS] =
		    dev_context->prev_virt_chan_slot_counts[TDM_VC_T_CBUS];
	}
}

struct cbus_req *bist_ready_done(struct mhl_dev_context *dev_context,
				struct cbus_req *req, uint8_t data1)
{
	MHL_TX_DBG_ERR("\n")
	return req;
}

void send_bist_ready(struct mhl_dev_context *dev_context)
{
	si_mhl_tx_send_msc_msg(dev_context, MHL_MSC_MSG_BIST_READY,
			      dev_context->bist_ready_status, bist_ready_done);
}
void si_mhl_tx_process_bist_setup_burst(struct mhl_dev_context *dev_context,
					uint8_t *write_burst_data)
{
	struct bist_setup_burst *bist_setup;
	enum cbus_mode_e cbus_mode;
	uint8_t tmp;
	uint8_t setup_status = 0;

	MHL_TX_DBG_INFO("\n");

	cbus_mode = si_mhl_tx_drv_get_cbus_mode(dev_context);
	switch (cbus_mode) {
	case CM_oCBUS_PEER_IS_MHL3_BIST_STAT:
		si_mhl_tx_drv_switch_cbus_mode(
			(struct drv_hw_context *)&dev_context->drv_context,
			CM_oCBUS_PEER_IS_MHL3_BIST_SETUP_PEER_READY);
		break;
	case CM_oCBUS_PEER_IS_MHL3_BIST_SETUP:
	case CM_oCBUS_PEER_IS_MHL3_BIST_SETUP_PEER_READY:
	case CM_oCBUS_PEER_IS_MHL3:

		break;
	default:
		MHL_TX_DBG_ERR("BIST_SETUP received while CBUS mode is %s\n",
			       si_mhl_tx_drv_get_cbus_mode_str(cbus_mode));
		return;
	}
	bist_setup = (struct bist_setup_burst *)write_burst_data;

	dev_context->misc_flags.flags.bist_role_TE = 0;
	/*
	 * Validate the received BIST setup info and if it checks out
	 * save it for use later when a BIST test is requested.
	 */
	tmp = calculate_generic_checksum(write_burst_data, 0,
					 sizeof(*bist_setup));
	if (tmp != 0) {
		MHL_TX_DBG_ERR("Bad checksum (0x%02x) in BIST setup burst\n",
			       tmp);
		return;
	}

	mhl_tx_stop_timer(dev_context, dev_context->cbus_mode_up_timer);

	dev_context->bist_setup.avlink_data_rate = bist_setup->avlink_data_rate;
	dev_context->bist_setup.avlink_duration = bist_setup->avlink_duration;
	dev_context->bist_setup.avlink_fixed_pat =
	    (bist_setup->avlink_fixed_h << 8) | bist_setup->avlink_fixed_l;
	dev_context->bist_setup.avlink_pattern = bist_setup->avlink_pattern;
	dev_context->bist_setup.avlink_randomizer =
	    bist_setup->avlink_randomizer;
	dev_context->bist_setup.avlink_video_mode =
	    bist_setup->avlink_video_mode;
	dev_context->bist_setup.e_cbus_duration = bist_setup->e_cbus_duration;
	dev_context->bist_setup.e_cbus_fixed_pat =
	    (bist_setup->e_cbus_fixed_h << 8) | bist_setup->e_cbus_fixed_l;
	dev_context->bist_setup.e_cbus_pattern = bist_setup->e_cbus_pattern;
	dev_context->bist_setup.impedance_mode = bist_setup->impedance_mode;

	si_mhl_tx_drv_switch_cbus_mode(
		(struct drv_hw_context *) &dev_context->drv_context,
		CM_oCBUS_PEER_IS_MHL3_BIST_SETUP_PEER_READY);
	switch (bist_setup->impedance_mode) {
	case BIST_IMPEDANCE_MODE_AVLINK_TX_LOW:
	case BIST_IMPEDANCE_MODE_AVLINK_TX_HIGH:
	case BIST_IMPEDANCE_MODE_ECBUS_S_TX_LOW:
	case BIST_IMPEDANCE_MODE_ECBUS_S_TX_HIGH:
		break;
	case BIST_IMPEDANCE_MODE_ECBUS_D_TX_LOW:
	case BIST_IMPEDANCE_MODE_ECBUS_D_TX_HIGH:
		if (si_mhl_tx_drv_support_e_cbus_d(
			(struct drv_hw_context *)&dev_context->drv_context)) {
			break;
		}
	default:
		MHL_TX_DBG_ERR("Invalid value 0x%02x specified in "
			       "IMPEDANCE_MODE field\n",
			       bist_setup->impedance_mode);
		setup_status |= BIST_READY_TERM_ERROR;
		si_mhl_tx_drv_switch_cbus_mode((struct drv_hw_context *)
			&dev_context->drv_context,
			CM_oCBUS_PEER_IS_MHL3);
		break;
	}

	if (bist_setup->e_cbus_duration == 0) {
		MHL_TX_DBG_ERR("Invalid value 0x00 specified in "
			       "eCBUS_DURATION field\n");
		setup_status |= BIST_READY_E_CBUS_ERROR | BIST_READY_TERM_ERROR;
		si_mhl_tx_drv_switch_cbus_mode((struct drv_hw_context *)
			&dev_context->drv_context,
			CM_oCBUS_PEER_IS_MHL3);
	}

	if (bist_setup->e_cbus_pattern > BIST_ECBUS_PATTERN_MAX) {
		MHL_TX_DBG_ERR("Invalid value 0x%02x specified in "
			       "eCBUS_PATTERN field\n",
			       bist_setup->e_cbus_pattern);
		setup_status |= BIST_READY_E_CBUS_ERROR;
		si_mhl_tx_drv_switch_cbus_mode((struct drv_hw_context *)
			&dev_context->drv_context,
			CM_oCBUS_PEER_IS_MHL3);
	}

	if (bist_setup->avlink_pattern > BIST_AVLINK_PATTERN_MAX) {
		MHL_TX_DBG_ERR("Invalid value 0x%02x specified in "
			       "AVLINK_PATTERN field\n",
			       bist_setup->avlink_pattern);
		setup_status |= BIST_READY_AVLINK_ERROR;
		si_mhl_tx_drv_switch_cbus_mode((struct drv_hw_context *)
			&dev_context->drv_context,
			CM_oCBUS_PEER_IS_MHL3);
	}

	if (!(bist_setup->avlink_video_mode == 3 ||
	      bist_setup->avlink_video_mode == 4)) {
		MHL_TX_DBG_ERR("Invalid value specified in "
			       "AVLINK_VIDEO_MODE field\n");
		setup_status |= BIST_READY_AVLINK_ERROR;
		si_mhl_tx_drv_switch_cbus_mode((struct drv_hw_context *)
			&dev_context->drv_context,
			CM_oCBUS_PEER_IS_MHL3);
	}

	if (bist_setup->avlink_randomizer > 1) {
		MHL_TX_DBG_ERR("Invalid value 0x%02x specified in "
			       "AVLINK_RANDOMIZER field\n",
			       bist_setup->avlink_randomizer);
		setup_status |= BIST_READY_AVLINK_ERROR;
		si_mhl_tx_drv_switch_cbus_mode((struct drv_hw_context *)
			&dev_context->drv_context,
			CM_oCBUS_PEER_IS_MHL3);
	}

	/*
	 * Make sure the specified AV data rate is valid and supported by the
	 * transmitter.
	 */
	if (bist_setup->avlink_data_rate == 0 ||
	    bist_setup->avlink_data_rate > 3) {
		MHL_TX_DBG_ERR("Invalid value 0x%02x specified in "
			       "AVLINK_PATTERN field\n",
			       bist_setup->avlink_data_rate);
		setup_status |= BIST_READY_AVLINK_ERROR;
	}

	if (!(setup_status & BIST_READY_E_CBUS_ERROR))
		setup_status |= BIST_READY_E_CBUS_READY;
	if (!(setup_status & BIST_READY_AVLINK_ERROR))
		setup_status |= BIST_READY_AVLINK_READY;
	if (!(setup_status & BIST_READY_TERM_ERROR))
		setup_status |= BIST_READY_TERM_READY;

	cbus_mode = si_mhl_tx_drv_get_cbus_mode(dev_context);
	if (CM_oCBUS_PEER_IS_MHL3_BIST_SETUP_PEER_READY == cbus_mode) {
		mhl_tx_stop_timer(dev_context, dev_context->cbus_mode_up_timer);
		MHL_TX_DBG_ERR("\n")
		dev_context->bist_ready_status = setup_status;
		setup_sans_cbus1(dev_context);
	}

}

void si_mhl_tx_process_write_burst_data(struct mhl_dev_context *dev_context)
{
	int ret_val = 0;
	enum BurstId_e burst_id;
	enum cbus_mode_e cbus_mode;

	MHL_TX_DBG_INFO("\n");
	cbus_mode = si_mhl_tx_drv_get_cbus_mode(dev_context);

	/* continue else statement to support 3D along with MDT */
	ret_val = si_mhl_tx_drv_get_scratch_pad(
		(struct drv_hw_context *)(&dev_context->drv_context),
		0,
		dev_context->incoming_scratch_pad.asBytes,
		sizeof(dev_context->incoming_scratch_pad));
	if (ret_val < 0) {
		MHL_TX_DBG_ERR("scratch pad failure 0x%x\n", ret_val);
	} else {
		burst_id =
		    BURST_ID(dev_context->incoming_scratch_pad.videoFormatData.
			     header.burst_id);

		MHL_TX_DBG_WARN("Got WRITE_BURST {0x%04x}\n", burst_id);

		switch (burst_id) {
		case burst_id_3D_VIC:
			si_mhl_tx_process_3d_vic_burst(
				dev_context->edid_parser_context,
				&dev_context->incoming_scratch_pad.
					videoFormatData);
			break;

		case burst_id_3D_DTD:
			si_mhl_tx_process_3d_dtd_burst(
				dev_context->edid_parser_context,
				&dev_context->incoming_scratch_pad.
					videoFormatData);
			break;
		case burst_id_HEV_VIC:
			si_mhl_tx_process_hev_vic_burst(
				dev_context->edid_parser_context,
				&dev_context->incoming_scratch_pad.
					hev_vic_data);
			break;

		case burst_id_HEV_DTDA:
			si_mhl_tx_process_hev_dtd_a_burst(
				dev_context->edid_parser_context,
				&dev_context->incoming_scratch_pad.
					hev_dtd_a_data);
			break;
		case burst_id_HEV_DTDB:
			si_mhl_tx_process_hev_dtd_b_burst(
				dev_context->edid_parser_context,
				&dev_context->incoming_scratch_pad.
					hev_dtd_b_data);
			break;

		case burst_id_VC_ASSIGN:
			si_mhl_tx_process_vc_assign_burst(dev_context,
				(uint8_t *)(&dev_context->
					incoming_scratch_pad));
			break;

		case burst_id_VC_CONFIRM:
			si_mhl_tx_process_vc_confirm_burst(dev_context,
				(uint8_t *)(&dev_context->
					incoming_scratch_pad));
			break;
		case burst_id_AUD_DELAY:

			mhl_event_notify(dev_context,
				MHL_EVENT_AUD_DELAY_RCVD, 0x00,
				&dev_context->incoming_scratch_pad);
			break;

		case burst_id_ADT_BURSTID:
			break;

		case burst_id_BIST_SETUP:
			si_mhl_tx_process_bist_setup_burst(dev_context,
				(uint8_t *)(&dev_context->
					incoming_scratch_pad));
			break;

		case burst_id_BIST_RETURN_STAT:
			{
			struct bist_return_stat_burst *bist_status;
			bist_status = (struct bist_return_stat_burst *)
				dev_context->incoming_scratch_pad.asBytes;
			if (0 != calculate_generic_checksum(
				(uint8_t *)bist_status, 0,
				sizeof(*bist_status))) {
					MHL_TX_DBG_ERR
					    ("BIST_RETURN_STAT received "
					     "with bad checksum\n");
			} else {
				uint16_t e_cbus_remote_stat;
				uint16_t av_link_stat;
				bool status;
				e_cbus_remote_stat =
					bist_status->e_cbus_stat_h << 8;
				e_cbus_remote_stat |=
					bist_status->e_cbus_stat_l;
				av_link_stat = bist_status->avlink_stat_h << 8;
				av_link_stat |= bist_status->avlink_stat_l;
				dev_context->bist_stat.e_cbus_remote_stat =
					e_cbus_remote_stat;
				dev_context->bist_stat.avlink_stat =
					av_link_stat;
				MHL_TX_DBG_ERR
					("BIST_RETURN_STAT received eCBUS_STAT"
					" remote:%s0x%04x%s"
					" local:%s0x%06x%s"
					" AV_LINK_STAT %s0x%04x%s\n",
					ANSI_ESC_GREEN_TEXT,
					e_cbus_remote_stat,
					ANSI_ESC_RESET_TEXT,
					ANSI_ESC_GREEN_TEXT,
					dev_context->bist_stat.
						e_cbus_prev_local_stat,
					ANSI_ESC_RESET_TEXT,
					ANSI_ESC_GREEN_TEXT,
					av_link_stat,
					ANSI_ESC_RESET_TEXT);
				mhl_event_notify(dev_context,
					MHL_TX_EVENT_BIST_STATUS_RECEIVED,
					0x00, &(dev_context->bist_stat));
				/*
				 * BIST run completed
				 */
				si_mhl_tx_drv_switch_cbus_mode(
					(struct drv_hw_context *)
					&dev_context->drv_context,
					CM_oCBUS_PEER_IS_MHL3);
				MHL_TX_DBG_ERR(
					"%sissuing CBUS_MODE_UP%s\n",
					ANSI_ESC_GREEN_TEXT,
					ANSI_ESC_RESET_TEXT)
				#ifdef BIST_DONE_DEBUG
					si_dump_important_regs(
						(struct drv_hw_context *)
						&dev_context->drv_context);
				#endif
				status = si_mhl_tx_rap_send(
					dev_context,
					MHL_RAP_CBUS_MODE_UP);
				if (status) {
					si_mhl_tx_drive_states(
						dev_context);
				}
			}
			}
			break;

		case burst_id_BIST_DISCARD:
			{
			struct bist_discard_burst *bist_discard;
			bist_discard = (struct bist_discard_burst *)
				dev_context->incoming_scratch_pad.asBytes;
				MHL_TX_DBG_ERR("BIST_DISCARD:0x%x\n",
					bist_discard->hdr.remaining_length)
			}
			break;

		case burst_id_BIST_ECHO_REQUEST:
			{
			struct bist_echo_request_burst *echo_request;
			struct bist_echo_response_burst echo_response;
			int i;
			echo_request = (struct bist_echo_request_burst *)
				dev_context->incoming_scratch_pad.asBytes;
			echo_response.hdr.burst_id.high =
				HIGH_BYTE_16(burst_id_BIST_ECHO_RESPONSE);
			echo_response.hdr.burst_id.low =
				LOW_BYTE_16(burst_id_BIST_ECHO_RESPONSE);
			echo_response.hdr.remaining_length =
				echo_request->hdr.remaining_length;
			for (i = 0;
				(i < echo_request->hdr.remaining_length)
				&& (i < sizeof(echo_response.payload));
				++i) {
				echo_response.payload[i] =
					echo_request->payload[i];
			}
			si_mhl_tx_request_write_burst(
					dev_context, 0, sizeof(echo_response),
					(uint8_t *)&echo_response);
			}
			break;

		case burst_id_BIST_ECHO_RESPONSE:
			break;

		case burst_id_EMSC_SUPPORT:
			break;

		case burst_id_HID_PAYLOAD:
			break;

		case burst_id_BLK_RCV_BUFFER_INFO:
			break;

		case burst_id_BITS_PER_PIXEL_FMT:
			break;
		case LOCAL_ADOPTER_ID:
#ifdef MEDIA_DATA_TUNNEL_SUPPORT
		case MHL_TEST_ADOPTER_ID:
			if (CM_oCBUS_PEER_IS_MHL3_BIST_STAT == cbus_mode) {
				MHL_TX_DBG_ERR("MHL_TEST_ID 0x%04x\n",
						burst_id);
				si_mhl_tx_drv_switch_cbus_mode(
					(struct drv_hw_context *)
					&dev_context->drv_context,
					CM_oCBUS_PEER_IS_MHL3);
			} else {
				si_mhl_tx_mdt_process_packet(dev_context,
				    (void *)&dev_context->incoming_scratch_pad.
					asBytes);
			}
#else
			/*
			 * Cause a notification event to be raised to allow
			 * interested applications a chance to process the
			 * received write burst data.
			 */
			mhl_event_notify(dev_context,
				MHL_TX_EVENT_SPAD_RECEIVED,
				sizeof(dev_context->incoming_scratch_pad),
				dev_context->incoming_scratch_pad.asBytes);
#endif
			break;

		default:
			MHL_TX_DBG_ERR("Dropping write burst with "
					"invalid adopter id: 0x%04x\n",
					burst_id);
			break;
		}
	}
}

static bool si_mhl_tx_set_path_en(struct mhl_dev_context *dev_context)
{
	MHL_TX_DBG_INFO("called\n");
	si_mhl_tx_drv_enable_video_path((struct drv_hw_context *)
					(&dev_context->drv_context));
	dev_context->link_mode |= MHL_STATUS_PATH_ENABLED;
	return si_mhl_tx_set_status(dev_context, false,
				    MHL_STATUS_REG_LINK_MODE,
				    dev_context->link_mode);
}

static bool si_mhl_tx_clr_path_en(struct mhl_dev_context *dev_context)
{
	MHL_TX_DBG_INFO("called\n");
	si_mhl_tx_drv_disable_video_path((struct drv_hw_context *)
					 (&dev_context->drv_context));
	dev_context->link_mode &= ~MHL_STATUS_PATH_ENABLED;
	return si_mhl_tx_set_status(dev_context, false,
				    MHL_STATUS_REG_LINK_MODE,
				    dev_context->link_mode);
}

static void si_mhl_tx_refresh_peer_devcap_entries_impl(struct mhl_dev_context
						       *dev_context,
						       const char *function,
						       int line)
{
	MHL_TX_DBG_INFO("from %s:%d\n", function, line);
	MHL_TX_DBG_INFO("DCAP_RDY DEVCAP: %s\n",
			dev_context->misc_flags.flags.
			have_complete_devcap ? "current" : "stale");

	/*
	 * If there is a DEV CAP read operation in progress
	 * cancel it and issue a new DEV CAP read to make sure
	 * we pick up all the DEV CAP register changes.
	 */
	if (dev_context->current_cbus_req != NULL) {
		if (dev_context->current_cbus_req->command == MHL_READ_DEVCAP) {
			dev_context->current_cbus_req->status.flags.cancel =
			    true;
			MHL_TX_DBG_ERR("%s cancelling MHL_READ_DEVCAP%s\n",
				ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
		}
	}
	si_mhl_tx_read_devcap(dev_context);

	MHL_TX_DBG_INFO("mhl_version:0x%x\n",
			dev_context->dev_cap_cache.mdc.mhl_version);
}

/*
 * si_mhl_tx_got_mhl_intr
 *
 * This function is called to inform of the arrival
 * of an MHL INTERRUPT message.
 */
void si_mhl_tx_got_mhl_intr(struct mhl_dev_context *dev_context,
			    uint8_t intr_0, uint8_t intr_1)
{
	MHL_TX_DBG_INFO("INTERRUPT Arrived. %02X, %02X\n", intr_0, intr_1);

	/* Handle DCAP_CHG INTR here */
	if (MHL_INT_DCAP_CHG & intr_0) {
		MHL_TX_DBG_WARN("got DCAP_CHG stopping timer...\n");
		mhl_tx_stop_timer(dev_context, dev_context->dcap_chg_timer);
		if (MHL_STATUS_DCAP_RDY & dev_context->status_0) {
			MHL_TX_DBG_WARN("got DCAP_CHG & DCAP_RDY\n");
			if (si_mhl_tx_drv_connection_is_mhl3(dev_context)) {
				if (si_mhl_tx_drv_get_cbus_mode(dev_context) <
				    CM_eCBUS_S) {
					si_mhl_tx_read_xdevcap_reg(dev_context,
						XDEVCAP_ADDR_ECBUS_SPEEDS);
				} else {
					si_mhl_tx_read_xdevcap(dev_context);
				}
			} else {
				si_mhl_tx_refresh_peer_devcap_entries
				    (dev_context);
			}
		}
	}

	if (MHL_INT_DSCR_CHG & intr_0) {
		/* remote WRITE_BURST is complete */
		dev_context->misc_flags.flags.rcv_scratchpad_busy = false;
		si_mhl_tx_process_write_burst_data(dev_context);
	}

	if (MHL_INT_REQ_WRT & intr_0) {
		/* Scratch pad write request from the sink device. */
		if (dev_context->misc_flags.flags.rcv_scratchpad_busy) {
			/*
			 * Use priority 1 to defer sending grant until
			 * local traffic is done
			 */
			si_mhl_tx_set_int(dev_context, MHL_RCHANGE_INT,
					  MHL_INT_GRT_WRT, 1);
		} else {
			/* use priority 0 to respond immediately */
			si_mhl_tx_set_int(dev_context, MHL_RCHANGE_INT,
					  MHL_INT_GRT_WRT, 0);
		}
	}

	if (MHL3_INT_FEAT_REQ & intr_0) {
		/* Send write bursts for all features that we support */

		struct MHL3_emsc_support_data_t emsc_support = {
			{ENCODE_BURST_ID(burst_id_EMSC_SUPPORT),
			 0,	/* checksum  starts at 0 */
			 1,	/* total entries */
			 1	/* sequence id */
			 },
			1,	/* num entries this burst */
			    {
			       {
				ENCODE_BURST_ID(burst_id_HID_PAYLOAD),
				ENCODE_BURST_ID(0),
				ENCODE_BURST_ID(0),
				ENCODE_BURST_ID(0),
				ENCODE_BURST_ID(0)
				}
			       }
		};
		struct MHL3_adt_data_t adt_burst = {
			{
			 ENCODE_BURST_ID(burst_id_ADT_BURSTID),
			 0,	/* checksum  starts at 0 */
			 1,	/* total entries */
			 1	/* sequence id */
			 },
			{
			   0,	/* format flags */
			   .descriptors = {
				.short_descs = {0, 0, 0, 0, 0, 0, 0, 0, 0}
#if 0
				.spkr_alloc_db = {
					.cea861f_spkr_alloc = {0, 0, 0},
					.cea861f_spkr_alloc = {0, 0, 0},
					.cea861f_spkr_alloc = {0, 0, 0}
				}
#endif
			}
		    }
		};
		MHL_TX_DBG_WARN("%sGot FEAT_REQ%s\n", ANSI_ESC_GREEN_TEXT,
			       ANSI_ESC_RESET_TEXT);
		emsc_support.header.checksum =
		    calculate_generic_checksum((uint8_t *) &emsc_support, 0,
					       sizeof(emsc_support));
		adt_burst.header.checksum =
		    calculate_generic_checksum((uint8_t *) &adt_burst, 0,
					       sizeof(adt_burst));

		/* Step 6: Store HEV_VIC in Burst Out-Queue. */
		if (false ==
		    si_mhl_tx_send_write_burst(dev_context, &emsc_support)) {
			MHL_TX_DBG_ERR("%scbus queue flooded%s\n",
				       ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
		} else if (false ==
			   si_mhl_tx_send_write_burst(dev_context,
						      &adt_burst)) {
			MHL_TX_DBG_ERR("%scbus queue flooded%s\n",
				       ANSI_ESC_RED_TEXT, ANSI_ESC_RESET_TEXT);
		}
		/* send with normal priority */
		si_mhl_tx_set_int(dev_context, MHL_RCHANGE_INT,
				  MHL3_INT_FEAT_COMPLETE, 1);
	}

	if (MHL3_INT_FEAT_COMPLETE & intr_0) {
		/* sink is finished sending write bursts in response to
		 * MHL3_INT_FEAT_REQ
		 */
		si_mhl_tx_display_timing_enumeration_end(dev_context->
							 edid_parser_context);
	}

	if (MHL_INT_EDID_CHG & intr_1) {

		dev_context->edid_valid = false;
		MHL_TX_DBG_INFO("MHL_INT_EDID_CHG\n");
		si_edid_reset(dev_context->edid_parser_context);
		if (dev_context->misc_flags.flags.have_complete_devcap) {
			if (dev_context->misc_flags.flags.mhl_hpd) {
				MHL_TX_DBG_INFO("tag: EDID_CHG\n");
				si_mhl_tx_initiate_edid_sequence(
					dev_context->edid_parser_context);
			}
		}
	}
}

/*
 * si_si_mhl_tx_check_av_link_status
 */
static void si_mhl_tx_check_av_link_status(struct mhl_dev_context *dev_context)
{
	/* is TMDS normal */
	if (MHL_XDS_LINK_STATUS_TMDS_NORMAL & dev_context->xstatus_1) {
		MHL_TX_DBG_WARN("AV LINK_MODE_STATUS is NORMAL.\n");
		if (dev_context->misc_flags.flags.edid_loop_active) {
			MHL_TX_DBG_ERR("EDID busy\n");
		} else if (!dev_context->misc_flags.flags.mhl_hpd) {
			MHL_TX_DBG_ERR(
				"No HPD when receiving AV LINK_MODE_STATUS\n");
		} else {
			si_mhl_tx_drv_start_cp(dev_context);
		}
	} else {
		MHL_TX_DBG_WARN("AV LINK_MODE_STATUS not NORMAL\n");
		si_mhl_tx_drv_shut_down_HDCP2((struct drv_hw_context *)
			(&dev_context->drv_context));
	}
}

/*
 * si_mhl_tx_got_mhl_status
 *
 * This function is called by the driver to inform of arrival of a MHL STATUS.
 */
void si_mhl_tx_got_mhl_status(struct mhl_dev_context *dev_context,
			      struct mhl_device_status *dev_status)
{
	uint8_t status_change_bit_mask_0;
	uint8_t status_change_bit_mask_1;
	uint8_t xstatus_change_bit_mask_1;
	uint8_t xstatus_change_bit_mask_3;

	MHL_TX_DBG_WARN
	    ("STATUS Arrived. %02X, %02X %02X : %02X %02X %02X %02X\n",
	     dev_status->write_stat[0], dev_status->write_stat[1],
	     dev_status->write_stat[2], dev_status->write_xstat[0],
	     dev_status->write_xstat[1], dev_status->write_xstat[2],
	     dev_status->write_xstat[3]);
	/*
	 * Handle DCAP_RDY STATUS here itself
	 */
	status_change_bit_mask_0 =
	    dev_status->write_stat[0] ^ dev_context->status_0;
	status_change_bit_mask_1 =
	    dev_status->write_stat[1] ^ dev_context->status_1;
	xstatus_change_bit_mask_1 =
	    dev_status->write_xstat[1] ^ dev_context->xstatus_1;
	xstatus_change_bit_mask_3 =
	    dev_status->write_xstat[3] ^ dev_context->xstatus_3;

	/*
	 * Remember the event.   (other code checks the saved values,
	 * so save the values early, but not before the XOR operations above)
	 */
	dev_context->status_0 = dev_status->write_stat[0];
	dev_context->status_1 = dev_status->write_stat[1];
	dev_context->xstatus_1 = dev_status->write_xstat[1];
	dev_context->xstatus_3 = dev_status->write_xstat[3];

	if (0xFF & dev_status->write_stat[2]) {
		if (0 == dev_context->peer_mhl3_version) {
			dev_context->peer_mhl3_version =
			    dev_status->write_stat[2];
		}
		MHL_TX_DBG_WARN("Got VERSION_STAT->MHL_VER = %02x\n",
				dev_context->peer_mhl3_version);
	}

	if (MHL_STATUS_DCAP_RDY & status_change_bit_mask_0) {
		MHL_TX_DBG_WARN("DCAP_RDY changed\n");
		if (MHL_STATUS_DCAP_RDY & dev_status->write_stat[0]) {
			if (dev_context->peer_mhl3_version >= 0x30) {
				MHL_TX_DBG_INFO("Assuming minimum MHL3.x"
					" feature support\n");
				dev_context->dev_cap_cache.mdc.featureFlag |=
					MHL_FEATURE_RCP_SUPPORT |
					MHL_FEATURE_RAP_SUPPORT |
					MHL_FEATURE_SP_SUPPORT;
			}
			mhl_tx_stop_timer(dev_context,
				dev_context->dcap_rdy_timer);
			/* some dongles send DCAP_RDY, but not DCAP_CHG */
			mhl_tx_start_timer(dev_context,
				dev_context->dcap_chg_timer, 2000);
			if (si_mhl_tx_drv_connection_is_mhl3(dev_context))
				MHL_TX_DBG_WARN("waiting for DCAP_CHG\n");
		}
	}
	/* look for a change in the pow bit */
	if (MHL_STATUS_POW_STAT & status_change_bit_mask_0) {
		uint8_t sink_drives_vbus =
		    (MHL_STATUS_POW_STAT & dev_status->write_stat[0]);
		uint8_t param = 0;

		if (sink_drives_vbus) {
			uint8_t index;

			index = dev_status->write_stat[0];
			index &= MHL_STATUS_PLIM_STAT_MASK;
			index >>= 3;
			param = MHL_DEV_CATEGORY_POW_BIT;
			/*
			 * Since downstream device is supplying VBUS power,
			 * turn off our VBUS power here. If the platform
			 * application can control VBUS power it should turn
			 * off it's VBUS power now.
			 */
			MHL_TX_DBG_WARN("%ssink drives VBUS%s\n",
				ANSI_ESC_GREEN_TEXT, ANSI_ESC_RESET_TEXT);
			/* limit incoming current */
			mhl_tx_vbus_control(VBUS_OFF);
			mhl_tx_vbus_current_ctl(plim_table[index]);
		} else {
			MHL_TX_DBG_WARN("%ssource drives VBUS"
				" according to PLIM_STAT%s\n",
			ANSI_ESC_YELLOW_TEXT, ANSI_ESC_RESET_TEXT);
			/* limit outgoing current */
			mhl_tx_vbus_control(VBUS_ON);
		}
		/* we only get POW_STAT if the sink is MHL3 or newer,
		 * so update the POW bit
		 */
		dev_context->dev_cap_cache.mdc.deviceCategory &=
		    ~MHL_DEV_CATEGORY_POW_BIT;
		dev_context->dev_cap_cache.mdc.deviceCategory |= param;
		/* Inform interested Apps of the MHL power change */
		mhl_event_notify(dev_context, MHL_TX_EVENT_POW_BIT_CHG,
				 param, NULL);
	}

	/* did PATH_EN change? */
	if (MHL_STATUS_PATH_ENABLED & status_change_bit_mask_1) {
		MHL_TX_DBG_INFO("PATH_EN changed\n");
		if (MHL_STATUS_PATH_ENABLED & dev_status->write_stat[1])
			si_mhl_tx_set_path_en(dev_context);
		else
			si_mhl_tx_clr_path_en(dev_context);
	}

	if (xstatus_change_bit_mask_1) {
		MHL_TX_DBG_WARN("link mode status changed %02X\n",
			xstatus_change_bit_mask_1);
		si_mhl_tx_check_av_link_status(dev_context);
	}
}
struct cbus_req *rcp_done(struct mhl_dev_context *dev_context,
				struct cbus_req *req, uint8_t data1)
{
	MHL_TX_DBG_ERR("\n")
	return req;
}
/*
 * si_mhl_tx_rcp_send
 *
 * This function checks if the peer device supports RCP and sends rcpKeyCode.
 * The function will return a value of true if it could successfully send the
 * RCP subcommand and the key code. Otherwise false.
 *
 */
bool si_mhl_tx_rcp_send(struct mhl_dev_context *dev_context,
	uint8_t rcpKeyCode)
{
	bool status;

	MHL_TX_DBG_INFO("called\n");

	/*
	 * Make sure peer supports RCP
	 */
	if (dev_context->dev_cap_cache.mdc.featureFlag &
	     MHL_FEATURE_RCP_SUPPORT) {

		status = si_mhl_tx_send_msc_msg(dev_context, MHL_MSC_MSG_RCP,
					   rcpKeyCode, rcp_done);
		if (status)
			si_mhl_tx_drive_states(dev_context);
	} else {
		MHL_TX_DBG_ERR("failed, peer feature flag = 0x%X\n",
			dev_context->dev_cap_cache.mdc.featureFlag);
		status = false;
	}

	return status;
}

struct cbus_req *ucp_done(struct mhl_dev_context *dev_context,
				struct cbus_req *req, uint8_t data1)
{
	MHL_TX_DBG_ERR("\n")
	return req;
}

/*
 * si_mhl_tx_ucp_send
 *
 * This function is (indirectly) called by a host application to send
 * a UCP key code to the downstream device.
 *
 * Returns true if the key code can be sent, false otherwise.
 */
bool si_mhl_tx_ucp_send(struct mhl_dev_context *dev_context,
			uint8_t ucp_key_code)
{
	bool status;
	MHL_TX_DBG_INFO("called key code: 0x%02x\n", ucp_key_code);

	/*
	 * Make sure peer supports UCP and that the connection is
	 * in a state where a UCP message can be sent.
	 */
	if (dev_context->dev_cap_cache.mdc.featureFlag &
	     MHL_FEATURE_UCP_RECV_SUPPORT) {

		status = si_mhl_tx_send_msc_msg(dev_context, MHL_MSC_MSG_UCP,
						ucp_key_code, ucp_done);
		if (status)
			si_mhl_tx_drive_states(dev_context);
	} else {
		MHL_TX_DBG_ERR("failed\n");
		status = false;
	}
	return status;
}

struct cbus_req *ucpk_done(struct mhl_dev_context *dev_context,
				struct cbus_req *req, uint8_t data1)
{
	MHL_TX_DBG_ERR("\n")

	return req;
}
/*
 * si_mhl_tx_ucp_send
 *
 * This function is (indirectly) called by a host application to send
 * a UCP acknowledge message for a received UCP key code message.
 *
 * Returns true if the message can be sent, false otherwise.
 */
bool si_mhl_tx_ucpk_send(struct mhl_dev_context *dev_context,
			 uint8_t ucp_key_code)
{
	bool status;
	MHL_TX_DBG_INFO("called key code: 0x%02x\n", ucp_key_code);

	status = si_mhl_tx_send_msc_msg(dev_context, MHL_MSC_MSG_UCPK,
					ucp_key_code, ucpk_done);
	return status;
}

struct cbus_req *ucpe_done(struct mhl_dev_context *dev_context,
				struct cbus_req *req, uint8_t data1)
{
	MHL_TX_DBG_ERR("\n")
	/*
	 * UCPE is always followed by an UCPK with
	 * original key code received.
	 */
	si_mhl_tx_ucpk_send(dev_context, dev_context->msc_save_ucp_key_code);
	return req;
}
/*
 * si_mhl_tx_ucpe_send
 *
 * This function is (indirectly) called by a host application to send a
 * UCP negative acknowledgment message for a received UCP key code message.
 *
 * Returns true if the message can be sent, false otherwise.
 *
 * When successful, mhl_tx internally sends UCPK with original (last known)
 * UCP keycode.
 */
bool si_mhl_tx_ucpe_send(struct mhl_dev_context *dev_context,
			 uint8_t ucpe_error_code)
{
	MHL_TX_DBG_INFO("called\n");

	return si_mhl_tx_send_msc_msg
		(dev_context, MHL_MSC_MSG_UCPE, ucpe_error_code, ucpe_done);
}

#if (INCLUDE_RBP == 1)
struct cbus_req *rbp_done(struct mhl_dev_context *dev_context,
				struct cbus_req *req, uint8_t data1)
{
	MHL_TX_DBG_ERR("\n")
	return req;
}

/*
 * si_mhl_tx_rbp_send
 *
 * This function checks if the peer device supports RBP and sends rbpButtonCode.
 * The function will return a value of true if it could successfully send the
 * RBP subcommand and the button code. Otherwise false.
 *
 */
bool si_mhl_tx_rbp_send(struct mhl_dev_context *dev_context,
	uint8_t rbpButtonCode)
{
	bool status;

	MHL_TX_DBG_INFO("called\n");

	/*
	 * Make sure peer supports RBP
	 */
	if (dev_context->dev_cap_cache.mdc.featureFlag &
	     MHL_FEATURE_RBP_SUPPORT) {

		status =
		    si_mhl_tx_send_msc_msg(dev_context, MHL_MSC_MSG_RBP,
					   rbpButtonCode, rbp_done);
		if (status)
			si_mhl_tx_drive_states(dev_context);
	} else {
		MHL_TX_DBG_ERR("failed, peer feature flag = 0x%X\n",
			dev_context->dev_cap_cache.mdc.featureFlag);
		status = false;
	}

	return status;
}

struct cbus_req *rbpk_done(struct mhl_dev_context *dev_context,
				struct cbus_req *req, uint8_t data1)
{
	MHL_TX_DBG_ERR("\n")
	return req;
}
/*
 * si_mhl_tx_rbpk_send
 *
 * This function is (indirectly) called by a host application to send
 * a RBP acknowledge message for a received RBP button code message.
 *
 * Returns true if the message can be sent, false otherwise.
 */
bool si_mhl_tx_rbpk_send(struct mhl_dev_context *dev_context,
			 uint8_t rbp_button_code)
{
	bool status;

	status = si_mhl_tx_send_msc_msg(dev_context, MHL_MSC_MSG_RBPK,
					rbp_button_code, rbpk_done);
	if (status)
		si_mhl_tx_drive_states(dev_context);

	return status;
}

struct cbus_req *rbpe_done(struct mhl_dev_context *dev_context,
				struct cbus_req *req, uint8_t data1)
{
	/*
	 * RBPE is always followed by an RBPK with
	 * original button code received.
	 */
	MHL_TX_DBG_ERR("\n")
	si_mhl_tx_rbpk_send(dev_context, dev_context->msc_save_rbp_button_code);
	return req;
}
/*
 * si_mhl_tx_rbpe_send
 *
 * The function will return a value of true if it could successfully send the
 * RBPE subcommand. Otherwise false.
 *
 * When successful, mhl_tx internally sends RBPK with original (last known)
 * button code.
 */
bool si_mhl_tx_rbpe_send(struct mhl_dev_context *dev_context,
			 uint8_t rbpe_error_code)
{
	bool status;

	status = si_mhl_tx_send_msc_msg(dev_context, MHL_MSC_MSG_RBPE,
					rbpe_error_code, rbpe_done);
	if (status)
		si_mhl_tx_drive_states(dev_context);

	return status;
}
#endif

char *rap_strings_high[4] = {
	"POLL",
	"CONTENT_",
	"CBUS_MODE_",
	"out of range"
};

char *rap_strings_low[4][2] = {
	{"", ""},
	{"ON", "OFF"},
	{"DOWN", "UP"},
	{"OOR", "OOR"},
};

struct cbus_req *rap_done(struct mhl_dev_context *dev_context,
				struct cbus_req *req, uint8_t data1)
{
	MHL_TX_DBG_ERR("\n")
	/* the only RAP commands that we send are
	 * CBUS_MODE_UP AND CBUS_MODE_DOWN.
	 */
	/*
	if (MHL_RAP_CBUS_MODE_DOWN == dev_context->msc_msg_last_data) {
	} else if (MHL_RAP_CBUS_MODE_UP == dev_context->msc_msg_last_data) {
	}
	*/
	return req;
}

/*
 * si_mhl_tx_rap_send
 *
 * This function sends the requested RAP action code message if RAP
 * is supported by the downstream device.
 *
 * The function returns true if the message can be sent, false otherwise.
 */
bool si_mhl_tx_rap_send(struct mhl_dev_context *dev_context,
			uint8_t rap_action_code)
{
	bool status;

	MHL_TX_DBG_ERR("%sSend RAP{%s%s}%s\n", ANSI_ESC_GREEN_TEXT,
		rap_strings_high[(rap_action_code>>4)&3],
		rap_strings_low[(rap_action_code>>4)&3][rap_action_code & 1],
		ANSI_ESC_RESET_TEXT);
	/*
	 * Make sure peer supports RAP and that the connection is
	 * in a state where a RAP message can be sent.
	 */
	if (si_get_peer_mhl_version(dev_context) >= 0x30) {
		/*
		 * MHL3.0 Requires RAP support,
		 * no need to check if sink is MHL 3.0
		 */

		status = si_mhl_tx_send_msc_msg(dev_context, MHL_MSC_MSG_RAP,
						rap_action_code, rap_done);

	} else
	    if (dev_context->dev_cap_cache.mdc.
		 featureFlag & MHL_FEATURE_RAP_SUPPORT) {

		status = si_mhl_tx_send_msc_msg(dev_context, MHL_MSC_MSG_RAP,
						rap_action_code, rap_done);
	} else {
		MHL_TX_DBG_ERR("%sERROR in sending RAP{CBUS_MODE_%s}%s\n",
			       ANSI_ESC_RED_TEXT,
			       (rap_action_code == 0x21) ? "UP" : "DOWN",
			       ANSI_ESC_RESET_TEXT);
		status = false;
	}

	return status;
}

/*
 * si_mhl_tx_notify_downstream_hpd_change
 *
 * Handle the arrival of SET_HPD or CLEAR_HPD messages.
 *
 * Turn the content off or on based on what we got.
 */
void si_mhl_tx_notify_downstream_hpd_change(struct mhl_dev_context *dev_context,
					    uint8_t downstream_hpd)
{

	MHL_TX_DBG_INFO("HPD = %s\n", downstream_hpd ? "HIGH" : "LOW");

	if (0 == downstream_hpd) {
		struct cbus_req *req = dev_context->current_cbus_req;
		dev_context->misc_flags.flags.mhl_hpd = false;
		dev_context->edid_valid = false;
		if (req) {
			if (MHL_READ_EDID_BLOCK == req->command) {

				return_cbus_queue_entry(dev_context, req);
				dev_context->current_cbus_req = NULL;
				dev_context->misc_flags.flags.edid_loop_active =
				    0;
				MHL_TX_DBG_INFO("tag: EDID active: %d\n",
						dev_context->misc_flags.flags.
						edid_loop_active);
			}
		}
		si_edid_reset(dev_context->edid_parser_context);
	} else {
		dev_context->misc_flags.flags.mhl_hpd = true;

		/*
		 *  possible EDID read is complete here
		 *  see MHL spec section 5.9.1
		 */
		if (dev_context->misc_flags.flags.have_complete_devcap) {
			/* Devcap refresh is complete */
			MHL_TX_DBG_INFO("tag:\n");
			si_mhl_tx_initiate_edid_sequence(
					dev_context->edid_parser_context);
		}
	}
}

/*
 *	si_mhl_tx_get_peer_dev_cap_entry
 *
 *	index -- the devcap index to get
 *	*data pointer to location to write data
 *
 *	returns
 *		0 -- success
 *		1 -- busy.
 */
uint8_t si_mhl_tx_get_peer_dev_cap_entry(struct mhl_dev_context *dev_context,
					 uint8_t index, uint8_t *data)
{
	if (!dev_context->misc_flags.flags.have_complete_devcap) {
		/* update is in progress */
		return 1;
	} else {
		*data = dev_context->dev_cap_cache.devcap_cache[index];
		return 0;
	}
}

/*
 * si_get_scratch_pad_vector
 *
 * offset
 *	The beginning offset into the scratch pad from which to fetch entries.
 * length
 *	The number of entries to fetch
 * *data
 *	A pointer to an array of bytes where the data should be placed.
 *
 * returns:
 *	scratch_pad_status see si_mhl_tx_api.h for details
*/
enum scratch_pad_status si_get_scratch_pad_vector(struct mhl_dev_context
						  *dev_context, uint8_t offset,
						  uint8_t length,
						  uint8_t *data)
{
	if (!(dev_context->dev_cap_cache.mdc.featureFlag
	      & MHL_FEATURE_SP_SUPPORT)) {

		MHL_TX_DBG_INFO("failed SCRATCHPAD_NOT_SUPPORTED\n");
		return SCRATCHPAD_NOT_SUPPORTED;

	} else if (dev_context->misc_flags.flags.rcv_scratchpad_busy) {
		return SCRATCHPAD_BUSY;

	} else if ((offset >= sizeof(dev_context->incoming_scratch_pad)) ||
		   (length >
		    (sizeof(dev_context->incoming_scratch_pad) - offset))) {
		return SCRATCHPAD_BAD_PARAM;
	} else {
		uint8_t *scratch_pad =
		    dev_context->incoming_scratch_pad.asBytes;

		scratch_pad += offset;
		memcpy(data, scratch_pad, length);
	}
	return SCRATCHPAD_SUCCESS;
}

#ifdef ENABLE_DUMP_INFOFRAME

#define AT_ROW_END(i, length) ((i & (length-1)) == (length-1))

void DumpIncomingInfoFrameImpl(char *pszId, char *pszFile, int iLine,
			       info_frame_t *pInfoFrame, uint8_t length)
{
	uint8_t j;
	uint8_t *pData = (uint8_t *) pInfoFrame;

	printk(KERN_DEFAULT "mhl_tx: %s: length:0x%02x -- ", pszId, length);
	for (j = 0; j < length; j++) {
		printk(KERN_DEFAULT "%02X ", pData[j]);
		if (AT_ROW_END(j, 32))
			printk("\n");
	}
	printk(KERN_DEFAULT "\n");
}
#endif

void *si_mhl_tx_get_drv_context(void *context)
{
	struct mhl_dev_context *dev_context = context;

	if (dev_context->signature == MHL_DEV_CONTEXT_SIGNATURE)
		return &dev_context->drv_context;
	else
		return context;
}

int si_peer_supports_packed_pixel(void *dev_context)
{
	struct mhl_dev_context *dev_context_ptr =
	    (struct mhl_dev_context *)dev_context;
	return PACKED_PIXEL_AVAILABLE(dev_context_ptr);
}

int si_mhl_tx_shutdown(struct mhl_dev_context *dev_context)
{
	MHL_TX_DBG_INFO("SiI8620 Driver Shutdown.\n");
	mhl_tx_stop_all_timers(dev_context);
	si_mhl_tx_drv_shutdown((struct drv_hw_context *)&dev_context->
		drv_context);
	return 0;
}

int si_mhl_tx_ecbus_started(struct mhl_dev_context *dev_context)
{
	MHL_TX_DBG_INFO("eCBUS started\n");
	/* queue up both of these */
	si_mhl_tx_read_xdevcap(dev_context);
	return 0;
}
