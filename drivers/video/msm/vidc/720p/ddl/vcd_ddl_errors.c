/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <media/msm/vidc_type.h>
#include "vcd_ddl_utils.h"
#include "vcd_ddl.h"

#if DEBUG
#define DBG(x...) printk(KERN_DEBUG x)
#else
#define DBG(x...)
#endif

#define ERR(x...) printk(KERN_ERR x)

#define INVALID_CHANNEL_NUMBER  1
#define INVALID_COMMAND_ID 2
#define CHANNEL_ALREADY_IN_USE 3
#define CHANNEL_NOT_SET_BEFORE_CHANNEL_CLOSE 4
#define CHANNEL_SET_ERROR_INIT_CODEC 5
#define INIT_CODEC_ALREADY_CALLED 6
#define CHANNEL_SET_ERROR_INIT_BUFFERS 7
#define INIT_CODEC_ERROR_INIT_BUFFERS 8
#define INIT_BUFFER_ALREADY_CALLED  9
#define CHANNEL_SET_ERROR_FRAME_RUN 10
#define INIT_CODEC_ERROR_FRAME_RUN 11
#define INIT_BUFFERS_ERROR_FRAME_RUN 12
#define CODEC_LIMIT_EXCEEDED 13
#define FIRMWARE_SIZE_ZERO 14
#define FIRMWARE_ADDRESS_EXT_ZERO 15
#define CONTEXT_DMA_IN_ERROR 16
#define CONTEXT_DMA_OUT_ERROR 17
#define PROGRAM_DMA_ERROR 18
#define CONTEXT_STORE_EXT_ADD_ZERO 19
#define MEM_ALLOCATION_FAILED 20


#define UNSUPPORTED_FEATURE_IN_PROFILE 27
#define RESOLUTION_NOT_SUPPORTED 28
#define HEADER_NOT_FOUND 52
#define MB_NUM_INVALID 61
#define FRAME_RATE_NOT_SUPPORTED 62
#define INVALID_QP_VALUE 63
#define INVALID_RC_REACTION_COEFFICIENT 64
#define INVALID_CPB_SIZE_AT_GIVEN_LEVEL 65

#define ALLOC_DPB_SIZE_NOT_SUFFICIENT 71
#define ALLOC_DB_SIZE_NOT_SUFFICIENT 72
#define ALLOC_COMV_SIZE_NOT_SUFFICIENT 73
#define NUM_BUF_OUT_OF_RANGE 74
#define NULL_CONTEXT_POINTER 75
#define NULL_COMAMND_CONTROL_COMM_POINTER 76
#define NULL_METADATA_INPUT_POINTER 77
#define NULL_DPB_POINTER 78
#define NULL_DB_POINTER 79
#define NULL_COMV_POINTER 80

#define DIVIDE_BY_ZERO 81
#define BIT_STREAM_BUF_EXHAUST 82
#define DMA_NOT_STOPPED 83
#define DMA_TX_NOT_COMPLETE 84

#define MB_HEADER_NOT_DONE  85
#define MB_COEFF_NOT_DONE 86
#define CODEC_SLICE_NOT_DONE 87
#define VME_NOT_READY 88
#define VC1_BITPLANE_DECODE_ERR 89


#define VSP_NOT_READY 90
#define BUFFER_FULL_STATE 91

#define RESOLUTION_MISMATCH 112
#define NV_QUANT_ERR 113
#define SYNC_MARKER_ERR 114
#define FEATURE_NOT_SUPPORTED 115
#define MEM_CORRUPTION  116
#define INVALID_REFERENCE_FRAME  117
#define PICTURE_CODING_TYPE_ERR  118
#define MV_RANGE_ERR  119
#define PICTURE_STRUCTURE_ERR 120
#define SLICE_ADDR_INVALID  121
#define NON_PAIRED_FIELD_NOT_SUPPORTED  122
#define NON_FRAME_DATA_RECEIVED 123
#define INCOMPLETE_FRAME  124
#define NO_BUFFER_RELEASED_FROM_HOST  125
#define PICTURE_MANAGEMENT_ERROR  128
#define INVALID_MMCO  129
#define INVALID_PIC_REORDERING 130
#define INVALID_POC_TYPE 131
#define ACTIVE_SPS_NOT_PRESENT 132
#define ACTIVE_PPS_NOT_PRESENT 133
#define INVALID_SPS_ID 134
#define INVALID_PPS_ID 135


#define METADATA_NO_SPACE_QP 151
#define METADATA_NO_SAPCE_CONCEAL_MB 152
#define METADATA_NO_SPACE_VC1_PARAM 153
#define METADATA_NO_SPACE_SEI 154
#define METADATA_NO_SPACE_VUI 155
#define METADATA_NO_SPACE_EXTRA 156
#define METADATA_NO_SPACE_DATA_NONE 157
#define FRAME_RATE_UNKNOWN 158
#define ASPECT_RATIO_UNKOWN 159
#define COLOR_PRIMARIES_UNKNOWN 160
#define TRANSFER_CHAR_UNKWON 161
#define MATRIX_COEFF_UNKNOWN 162
#define NON_SEQ_SLICE_ADDR 163
#define BROKEN_LINK 164
#define FRAME_CONCEALED 165
#define PROFILE_UNKOWN 166
#define LEVEL_UNKOWN 167
#define BIT_RATE_NOT_SUPPORTED 168
#define COLOR_DIFF_FORMAT_NOT_SUPPORTED 169
#define NULL_EXTRA_METADATA_POINTER  170
#define SYNC_POINT_NOT_RECEIVED_STARTED_DECODING  171
#define NULL_FW_DEBUG_INFO_POINTER  172
#define ALLOC_DEBUG_INFO_SIZE_INSUFFICIENT  173
#define MAX_STAGE_COUNTER_EXCEEDED 174

#define METADATA_NO_SPACE_MB_INFO 180
#define METADATA_NO_SPACE_SLICE_SIZE 181
#define RESOLUTION_WARNING 182

static void ddl_handle_npf_decoding_error(
	struct ddl_context *ddl_context);

static u32 ddl_handle_seqhdr_fail_error(
	struct ddl_context *ddl_context);

void ddl_hw_fatal_cb(struct ddl_context *ddl_context)
{
	/* Invalidate the command state */
	ddl_move_command_state(ddl_context, DDL_CMD_INVALID);
	ddl_context->device_state = DDL_DEVICE_HWFATAL;

	/* callback to the client to indicate hw fatal error */
	ddl_context->ddl_callback(VCD_EVT_IND_HWERRFATAL,
					VCD_ERR_HW_FATAL, NULL, 0,
					(void *)ddl_context->current_ddl,
					ddl_context->client_data);

	DDL_IDLE(ddl_context);
}

static u32 ddl_handle_hw_fatal_errors(struct ddl_context
			*ddl_context)
{
	u32 status = false;

	switch (ddl_context->cmd_err_status) {

	case INVALID_CHANNEL_NUMBER:
	case INVALID_COMMAND_ID:
	case CHANNEL_ALREADY_IN_USE:
	case CHANNEL_NOT_SET_BEFORE_CHANNEL_CLOSE:
	case CHANNEL_SET_ERROR_INIT_CODEC:
	case INIT_CODEC_ALREADY_CALLED:
	case CHANNEL_SET_ERROR_INIT_BUFFERS:
	case INIT_CODEC_ERROR_INIT_BUFFERS:
	case INIT_BUFFER_ALREADY_CALLED:
	case CHANNEL_SET_ERROR_FRAME_RUN:
	case INIT_CODEC_ERROR_FRAME_RUN:
	case INIT_BUFFERS_ERROR_FRAME_RUN:
	case CODEC_LIMIT_EXCEEDED:
	case FIRMWARE_SIZE_ZERO:
	case FIRMWARE_ADDRESS_EXT_ZERO:

	case CONTEXT_DMA_IN_ERROR:
	case CONTEXT_DMA_OUT_ERROR:
	case PROGRAM_DMA_ERROR:
	case CONTEXT_STORE_EXT_ADD_ZERO:
	case MEM_ALLOCATION_FAILED:

	case DIVIDE_BY_ZERO:
	case DMA_NOT_STOPPED:
	case DMA_TX_NOT_COMPLETE:

	case VSP_NOT_READY:
	case BUFFER_FULL_STATE:
	case NULL_DB_POINTER:
		ERR("HW FATAL ERROR");
		ddl_hw_fatal_cb(ddl_context);
		status = true;
		break;
	}
	return status;
}

void ddl_client_fatal_cb(struct ddl_context *ddl_context)
{
	struct ddl_client_context  *ddl =
		ddl_context->current_ddl;

	if (ddl_context->cmd_state == DDL_CMD_DECODE_FRAME)
		ddl_decode_dynamic_property(ddl, false);
	else if (ddl_context->cmd_state == DDL_CMD_ENCODE_FRAME)
		ddl_encode_dynamic_property(ddl, false);

	ddl_move_command_state(ddl_context, DDL_CMD_INVALID);

	ddl_move_client_state(ddl, DDL_CLIENT_FATAL_ERROR);

	ddl_context->ddl_callback
	(
		VCD_EVT_IND_HWERRFATAL,
		VCD_ERR_CLIENT_FATAL,
		NULL,
		0,
		(void *)ddl,
		ddl_context->client_data
	);

	DDL_IDLE(ddl_context);
}

static u32 ddl_handle_client_fatal_errors(struct ddl_context
			*ddl_context)
{
	u32 status = false;

	switch (ddl_context->cmd_err_status) {
	case MB_NUM_INVALID:
	case FRAME_RATE_NOT_SUPPORTED:
	case INVALID_QP_VALUE:
	case INVALID_RC_REACTION_COEFFICIENT:
	case INVALID_CPB_SIZE_AT_GIVEN_LEVEL:

	case ALLOC_DPB_SIZE_NOT_SUFFICIENT:
	case ALLOC_DB_SIZE_NOT_SUFFICIENT:
	case ALLOC_COMV_SIZE_NOT_SUFFICIENT:
	case NUM_BUF_OUT_OF_RANGE:
	case NULL_CONTEXT_POINTER:
	case NULL_COMAMND_CONTROL_COMM_POINTER:
	case NULL_METADATA_INPUT_POINTER:
	case NULL_DPB_POINTER:
	case NULL_COMV_POINTER:
		{
			status = true;
			break;
		}
	}

	if (!status)
		ERR("UNKNOWN-OP-FAILED");

	ddl_client_fatal_cb(ddl_context);

	return true;
}

static void ddl_input_failed_cb(struct ddl_context *ddl_context,
			u32 vcd_event, u32 vcd_status)
{
	struct ddl_client_context  *ddl = ddl_context->current_ddl;

	ddl_move_command_state(ddl_context, DDL_CMD_INVALID);

	if (ddl->decoding)
		ddl_decode_dynamic_property(ddl, false);
	else
		ddl_encode_dynamic_property(ddl, false);

	ddl_context->ddl_callback(vcd_event,
		vcd_status, &ddl->input_frame,
		sizeof(struct ddl_frame_data_tag),
		(void *)ddl, ddl_context->client_data);

	ddl_move_client_state(ddl, DDL_CLIENT_WAIT_FOR_FRAME);
}

static u32 ddl_handle_core_recoverable_errors(struct ddl_context \
			*ddl_context)
{
	struct ddl_client_context  *ddl = ddl_context->current_ddl;
	u32   vcd_status = VCD_S_SUCCESS;
	u32   vcd_event = VCD_EVT_RESP_INPUT_DONE;
	u32   eos = false, pending_display = 0, release_mask = 0;

	if (ddl->decoding)
		if (ddl_handle_seqhdr_fail_error(ddl_context))
			return true;

	if (ddl_context->cmd_state != DDL_CMD_DECODE_FRAME &&
		ddl_context->cmd_state != DDL_CMD_ENCODE_FRAME) {
		return false;
	}
	switch (ddl_context->cmd_err_status) {
	case NON_PAIRED_FIELD_NOT_SUPPORTED:
		{
			ddl_handle_npf_decoding_error(ddl_context);
			return true;
		}
	case NO_BUFFER_RELEASED_FROM_HOST:
		{
			/* lets check sanity of this error */
			release_mask =
				ddl->codec_data.decoder.dpb_mask.hw_mask;
			while (release_mask > 0) {
				if ((release_mask & 0x1))
					pending_display += 1;
				release_mask >>= 1;
			}

			if (pending_display >=
				ddl->codec_data.decoder.min_dpb_num) {
				DBG("FWISSUE-REQBUF!!");
				/* callback to client for client fatal error */
				ddl_client_fatal_cb(ddl_context);
				return true ;
			}
		vcd_event = VCD_EVT_RESP_OUTPUT_REQ;
		break;
		}
	case BIT_STREAM_BUF_EXHAUST:
	case MB_HEADER_NOT_DONE:
	case MB_COEFF_NOT_DONE:
	case CODEC_SLICE_NOT_DONE:
	case VME_NOT_READY:
	case VC1_BITPLANE_DECODE_ERR:
		{
			u32 reset_core;
			/* need to reset the internal core hw engine */
			reset_core = ddl_hal_engine_reset(ddl_context);
			if (!reset_core)
				return true;
			/* fall through to process bitstream error handling */
		}
	case RESOLUTION_MISMATCH:
	case NV_QUANT_ERR:
	case SYNC_MARKER_ERR:
	case FEATURE_NOT_SUPPORTED:
	case MEM_CORRUPTION:
	case INVALID_REFERENCE_FRAME:
	case PICTURE_CODING_TYPE_ERR:
	case MV_RANGE_ERR:
	case PICTURE_STRUCTURE_ERR:
	case SLICE_ADDR_INVALID:
	case NON_FRAME_DATA_RECEIVED:
	case INCOMPLETE_FRAME:
	case PICTURE_MANAGEMENT_ERROR:
	case INVALID_MMCO:
	case INVALID_PIC_REORDERING:
	case INVALID_POC_TYPE:
		{
			vcd_status = VCD_ERR_BITSTREAM_ERR;
			break;
		}
	case ACTIVE_SPS_NOT_PRESENT:
	case ACTIVE_PPS_NOT_PRESENT:
		{
			if (ddl->codec_data.decoder.idr_only_decoding) {
				DBG("Consider warnings as errors in idr mode");
				ddl_client_fatal_cb(ddl_context);
				return true;
			}
			vcd_status = VCD_ERR_BITSTREAM_ERR;
			break;
		}
	case PROFILE_UNKOWN:
		if (ddl->decoding)
			vcd_status = VCD_ERR_BITSTREAM_ERR;
		break;
	}

	if (!vcd_status && vcd_event == VCD_EVT_RESP_INPUT_DONE)
		return false;

	ddl->input_frame.frm_trans_end = true;

	eos = ((vcd_event == VCD_EVT_RESP_INPUT_DONE) &&
		((VCD_FRAME_FLAG_EOS & ddl->input_frame.
				vcd_frm.flags)));

	if ((ddl->decoding && eos) ||
		(!ddl->decoding))
		ddl->input_frame.frm_trans_end = false;

	if (vcd_event == VCD_EVT_RESP_INPUT_DONE &&
		ddl->decoding &&
		!ddl->codec_data.decoder.header_in_start &&
		!ddl->codec_data.decoder.dec_disp_info.img_size_x &&
		!ddl->codec_data.decoder.dec_disp_info.img_size_y &&
		!eos) {
		DBG("Treat header in start error %u as success",
			vcd_status);
		/* this is first frame seq. header only case */
		vcd_status = VCD_S_SUCCESS;
		ddl->input_frame.vcd_frm.flags |=
			VCD_FRAME_FLAG_CODECCONFIG;
		ddl->input_frame.frm_trans_end = !eos;
		/* put just some non - zero value */
		ddl->codec_data.decoder.dec_disp_info.img_size_x = 0xff;
	}
	/* inform client about input failed */
	ddl_input_failed_cb(ddl_context, vcd_event, vcd_status);

	/* for Encoder case, we need to send output done also */
	if (!ddl->decoding) {
		/* transaction is complete after this callback */
		ddl->output_frame.frm_trans_end = !eos;
		/* error case: NO data present */
		ddl->output_frame.vcd_frm.data_len = 0;
		/* call back to client for output frame done */
		ddl_context->ddl_callback(VCD_EVT_RESP_OUTPUT_DONE,
		VCD_ERR_FAIL, &(ddl->output_frame),
			sizeof(struct ddl_frame_data_tag),
			(void *)ddl, ddl_context->client_data);

		if (eos) {
			DBG("ENC-EOS_DONE");
			/* send client EOS DONE callback */
			ddl_context->ddl_callback(VCD_EVT_RESP_EOS_DONE,
				VCD_S_SUCCESS, NULL, 0, (void *)ddl,
				ddl_context->client_data);
		}
	}

	/* if it is decoder EOS case */
	if (ddl->decoding && eos) {
		DBG("DEC-EOS_RUN");
		ddl_decode_eos_run(ddl);
	} else
		DDL_IDLE(ddl_context);

	return true;
}

static u32 ddl_handle_core_warnings(u32 err_status)
{
	u32 status = false;

	switch (err_status) {
	case FRAME_RATE_UNKNOWN:
	case ASPECT_RATIO_UNKOWN:
	case COLOR_PRIMARIES_UNKNOWN:
	case TRANSFER_CHAR_UNKWON:
	case MATRIX_COEFF_UNKNOWN:
	case NON_SEQ_SLICE_ADDR:
	case BROKEN_LINK:
	case FRAME_CONCEALED:
	case PROFILE_UNKOWN:
	case LEVEL_UNKOWN:
	case BIT_RATE_NOT_SUPPORTED:
	case COLOR_DIFF_FORMAT_NOT_SUPPORTED:
	case NULL_EXTRA_METADATA_POINTER:
	case SYNC_POINT_NOT_RECEIVED_STARTED_DECODING:

	case NULL_FW_DEBUG_INFO_POINTER:
	case ALLOC_DEBUG_INFO_SIZE_INSUFFICIENT:
	case MAX_STAGE_COUNTER_EXCEEDED:

	case METADATA_NO_SPACE_MB_INFO:
	case METADATA_NO_SPACE_SLICE_SIZE:
	case RESOLUTION_WARNING:

	/* decoder warnings */
	case METADATA_NO_SPACE_QP:
	case METADATA_NO_SAPCE_CONCEAL_MB:
	case METADATA_NO_SPACE_VC1_PARAM:
	case METADATA_NO_SPACE_SEI:
	case METADATA_NO_SPACE_VUI:
	case METADATA_NO_SPACE_EXTRA:
	case METADATA_NO_SPACE_DATA_NONE:
		{
			status = true;
			DBG("CMD-WARNING-IGNORED!!");
			break;
		}
	}
	return status;
}

u32 ddl_handle_core_errors(struct ddl_context *ddl_context)
{
	u32 status = false;

	if (!ddl_context->cmd_err_status &&
		!ddl_context->disp_pic_err_status &&
		!ddl_context->op_failed)
		return false;

	if (ddl_context->cmd_state == DDL_CMD_INVALID) {
		DBG("SPURIOUS_INTERRUPT_ERROR");
		return true;
	}

	if (!ddl_context->op_failed) {
		u32 disp_status;
		status = ddl_handle_core_warnings(ddl_context->
			cmd_err_status);
		disp_status = ddl_handle_core_warnings(
			ddl_context->disp_pic_err_status);
		if (!status && !disp_status)
			DBG("ddl_warning:Unknown");

		return false;
	}

	ERR("\n %s(): OPFAILED!!", __func__);
	ERR("\n CMD_ERROR_STATUS = %u, DISP_ERR_STATUS = %u",
		ddl_context->cmd_err_status,
		ddl_context->disp_pic_err_status);

	status = ddl_handle_hw_fatal_errors(ddl_context);

	if (!status)
		status = ddl_handle_core_recoverable_errors(ddl_context);

	if (!status)
		status = ddl_handle_client_fatal_errors(ddl_context);

	return status;
}

void ddl_handle_npf_decoding_error(struct ddl_context *ddl_context)
{
	struct ddl_client_context *ddl = ddl_context->current_ddl;
	struct ddl_decoder_data *decoder = &ddl->codec_data.decoder;
	if (!ddl->decoding) {
		ERR("FWISSUE-ENC-NPF!!!");
		ddl_client_fatal_cb(ddl_context);
		return;
	}
	vidc_720p_decode_display_info(&decoder->dec_disp_info);
	ddl_decode_dynamic_property(ddl, false);
	ddl->output_frame.vcd_frm.ip_frm_tag =
		decoder->dec_disp_info.tag_top;
	ddl->output_frame.vcd_frm.physical = NULL;
	ddl->output_frame.frm_trans_end = false;
	ddl->ddl_context->ddl_callback(
		VCD_EVT_RESP_OUTPUT_DONE,
		VCD_ERR_INTRLCD_FIELD_DROP,
		&ddl->output_frame,
		sizeof(struct ddl_frame_data_tag),
		(void *)ddl,
		ddl->ddl_context->client_data);
	ddl_decode_frame_run(ddl);
}

u32 ddl_handle_seqhdr_fail_error(struct ddl_context *ddl_context)
{
	struct ddl_client_context *ddl = ddl_context->current_ddl;
	struct ddl_decoder_data *decoder = &ddl->codec_data.decoder;
	u32 status = false;
	if (ddl_context->cmd_state == DDL_CMD_HEADER_PARSE &&
		DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_INITCODECDONE)) {
		switch (ddl_context->cmd_err_status) {
		case UNSUPPORTED_FEATURE_IN_PROFILE:
		case HEADER_NOT_FOUND:
		case INVALID_SPS_ID:
		case INVALID_PPS_ID:
		case RESOLUTION_NOT_SUPPORTED:
		case PROFILE_UNKOWN:
			ERR("SEQ-HDR-FAILED!!!");
			if ((ddl_context->cmd_err_status ==
				 RESOLUTION_NOT_SUPPORTED) &&
				(decoder->codec.codec == VCD_CODEC_H264 ||
				decoder->codec.codec == VCD_CODEC_H263 ||
				decoder->codec.codec == VCD_CODEC_MPEG4 ||
				decoder->codec.codec == VCD_CODEC_VC1_RCV ||
				decoder->codec.codec == VCD_CODEC_VC1)) {
				ddl_client_fatal_cb(ddl_context);
				status = true;
				break;
			}
			if (decoder->header_in_start) {
				decoder->header_in_start = false;
				ddl_context->ddl_callback(VCD_EVT_RESP_START,
					VCD_ERR_SEQHDR_PARSE_FAIL,
					NULL, 0, (void *)ddl,
					ddl_context->client_data);
			} else {
				if (ddl->input_frame.vcd_frm.flags &
					VCD_FRAME_FLAG_EOS)
					ddl->input_frame.frm_trans_end = false;
				else
					ddl->input_frame.frm_trans_end = true;
				ddl_decode_dynamic_property(ddl, false);
				ddl_context->ddl_callback(
					VCD_EVT_RESP_INPUT_DONE,
					VCD_ERR_SEQHDR_PARSE_FAIL,
					&ddl->input_frame,
					sizeof(struct ddl_frame_data_tag),
					(void *)ddl, ddl_context->client_data);
				if (ddl->input_frame.vcd_frm.flags &
					VCD_FRAME_FLAG_EOS)
					ddl_context->ddl_callback(
						VCD_EVT_RESP_EOS_DONE,
						VCD_S_SUCCESS, NULL,
						0, (void *)ddl,
						ddl_context->client_data);
			}
			ddl_move_client_state(ddl,
				DDL_CLIENT_WAIT_FOR_INITCODEC);
			DDL_IDLE(ddl_context);
			status = true;
		}
	}
	return status;
}
