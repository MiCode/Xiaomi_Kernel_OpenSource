/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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

#include "vcd_ddl.h"
#include "vcd_ddl_shared_mem.h"
#include "vidc.h"

static u32 ddl_handle_hw_fatal_errors(struct ddl_client_context *ddl);
static u32 ddl_handle_client_fatal_errors(
	struct ddl_client_context *ddl);
static void ddl_input_failed_cb(struct ddl_client_context *ddl,
	u32 vcd_event, u32 vcd_status);
static u32 ddl_handle_core_recoverable_errors(
	struct ddl_client_context *ddl);
static u32 ddl_handle_core_warnings(u32 error_code);
static void ddl_release_prev_field(
	struct ddl_client_context *ddl);
static u32 ddl_handle_dec_seq_hdr_fail_error(struct ddl_client_context *ddl);
static void print_core_errors(u32 error_code);
static void print_core_recoverable_errors(u32 error_code);

void ddl_hw_fatal_cb(struct ddl_client_context *ddl)
{
	struct ddl_context *ddl_context = ddl->ddl_context;
	u32 error_code = ddl_context->cmd_err_status;

	DDL_MSG_FATAL("VIDC_HW_FATAL");
	ddl->cmd_state = DDL_CMD_INVALID;
	ddl_context->device_state = DDL_DEVICE_HWFATAL;

	ddl_context->ddl_callback(VCD_EVT_IND_HWERRFATAL, VCD_ERR_HW_FATAL,
		&error_code, sizeof(error_code),
		(u32 *)ddl, ddl->client_data);

	ddl_release_command_channel(ddl_context, ddl->command_channel);
}

static u32 ddl_handle_hw_fatal_errors(struct ddl_client_context *ddl)
{
	struct ddl_context *ddl_context = ddl->ddl_context;
	u32 status = false, error_code = ddl_context->cmd_err_status;

	switch (error_code) {
	case VIDC_1080P_ERROR_INVALID_CHANNEL_NUMBER:
	case VIDC_1080P_ERROR_INVALID_COMMAND_ID:
	case VIDC_1080P_ERROR_CHANNEL_ALREADY_IN_USE:
	case VIDC_1080P_ERROR_CHANNEL_NOT_OPEN_BEFORE_CHANNEL_CLOSE:
	case VIDC_1080P_ERROR_OPEN_CH_ERROR_SEQ_START:
	case VIDC_1080P_ERROR_SEQ_START_ALREADY_CALLED:
	case VIDC_1080P_ERROR_OPEN_CH_ERROR_INIT_BUFFERS:
	case VIDC_1080P_ERROR_SEQ_START_ERROR_INIT_BUFFERS:
	case VIDC_1080P_ERROR_INIT_BUFFER_ALREADY_CALLED:
	case VIDC_1080P_ERROR_OPEN_CH_ERROR_FRAME_START:
	case VIDC_1080P_ERROR_SEQ_START_ERROR_FRAME_START:
	case VIDC_1080P_ERROR_INIT_BUFFERS_ERROR_FRAME_START:
	case VIDC_1080P_ERROR_RESOLUTION_CHANGED:
	case VIDC_1080P_ERROR_INVALID_COMMAND_LAST_FRAME:
	case VIDC_1080P_ERROR_INVALID_COMMAND:
	case VIDC_1080P_ERROR_INVALID_CODEC_TYPE:
	case VIDC_1080P_ERROR_MEM_ALLOCATION_FAILED:
	case VIDC_1080P_ERROR_INSUFFICIENT_CONTEXT_SIZE:
	case VIDC_1080P_ERROR_DIVIDE_BY_ZERO:
	case VIDC_1080P_ERROR_DESCRIPTOR_BUFFER_EMPTY:
	case VIDC_1080P_ERROR_DMA_TX_NOT_COMPLETE:
	case VIDC_1080P_ERROR_VSP_NOT_READY:
	case VIDC_1080P_ERROR_BUFFER_FULL_STATE:
		ddl_hw_fatal_cb(ddl);
		status = true;
	break;
	default:
	break;
	}
	return status;
}

void ddl_client_fatal_cb(struct ddl_client_context *ddl)
{
	struct ddl_context *ddl_context = ddl->ddl_context;

	if (ddl->cmd_state == DDL_CMD_DECODE_FRAME)
		ddl_vidc_decode_dynamic_property(ddl, false);
	else if (ddl->cmd_state == DDL_CMD_ENCODE_FRAME)
		ddl_vidc_encode_dynamic_property(ddl, false);
	ddl->cmd_state = DDL_CMD_INVALID;
	DDL_MSG_LOW("ddl_state_transition: %s ~~> DDL_CLIENT_FAVIDC_ERROR",
		ddl_get_state_string(ddl->client_state));
	ddl->client_state = DDL_CLIENT_FAVIDC_ERROR;
	ddl_context->ddl_callback(VCD_EVT_IND_HWERRFATAL,
		VCD_ERR_CLIENT_FATAL, NULL, 0, (u32 *)ddl,
		ddl->client_data);
	ddl_release_command_channel(ddl_context, ddl->command_channel);
}

static u32 ddl_handle_client_fatal_errors(
	struct ddl_client_context *ddl)
{
	struct ddl_context *ddl_context = ddl->ddl_context;
	u32 status = false;

	switch (ddl_context->cmd_err_status) {
	case VIDC_1080P_ERROR_UNSUPPORTED_FEATURE_IN_PROFILE:
	case VIDC_1080P_ERROR_RESOLUTION_NOT_SUPPORTED:
	case VIDC_1080P_ERROR_VOS_END_CODE_RECEIVED:
	case VIDC_1080P_ERROR_FRAME_RATE_NOT_SUPPORTED:
	case VIDC_1080P_ERROR_INVALID_QP_VALUE:
	case VIDC_1080P_ERROR_INVALID_RC_REACTION_COEFFICIENT:
	case VIDC_1080P_ERROR_INVALID_CPB_SIZE_AT_GIVEN_LEVEL:
	case VIDC_1080P_ERROR_B_FRAME_NOT_SUPPORTED:
	case VIDC_1080P_ERROR_ALLOC_DPB_SIZE_NOT_SUFFICIENT:
	case VIDC_1080P_ERROR_NUM_DPB_OUT_OF_RANGE:
	case VIDC_1080P_ERROR_NULL_METADATA_INPUT_POINTER:
	case VIDC_1080P_ERROR_NULL_DPB_POINTER:
	case VIDC_1080P_ERROR_NULL_OTH_EXT_BUFADDR:
	case VIDC_1080P_ERROR_NULL_MV_POINTER:
		status = true;
		DDL_MSG_ERROR("VIDC_CLIENT_FATAL!!");
	break;
	default:
	break;
	}
	if (!status)
		DDL_MSG_ERROR("VIDC_UNKNOWN_OP_FAILED %d",
				ddl_context->cmd_err_status);
	ddl_client_fatal_cb(ddl);
	return true;
}

static void ddl_input_failed_cb(struct ddl_client_context *ddl,
	u32 vcd_event, u32 vcd_status)
{
	struct ddl_context *ddl_context = ddl->ddl_context;
	u32 payload_size = sizeof(struct ddl_frame_data_tag);

	ddl->cmd_state = DDL_CMD_INVALID;
	if (ddl->decoding)
		ddl_vidc_decode_dynamic_property(ddl, false);
	else
		ddl_vidc_encode_dynamic_property(ddl, false);
	if (ddl->client_state == DDL_CLIENT_WAIT_FOR_INITCODECDONE) {
		payload_size = 0;
		DDL_MSG_LOW("ddl_state_transition: %s ~~> "
			"DDL_CLIENT_WAIT_FOR_INITCODEC",
			ddl_get_state_string(ddl->client_state));
		ddl->client_state = DDL_CLIENT_WAIT_FOR_INITCODEC;
	} else {
		DDL_MSG_LOW("ddl_state_transition: %s ~~> "
			"DDL_CLIENT_WAIT_FOR_FRAME",
			ddl_get_state_string(ddl->client_state));
		ddl->client_state = DDL_CLIENT_WAIT_FOR_FRAME;
	}
	if (vcd_status == VCD_ERR_IFRAME_EXPECTED)
		vcd_status = VCD_S_SUCCESS;
	ddl_context->ddl_callback(vcd_event, vcd_status, &ddl->input_frame,
		payload_size, (u32 *)ddl, ddl->client_data);
}

static u32 ddl_handle_core_recoverable_errors(
	struct ddl_client_context *ddl)
{
	struct ddl_context *ddl_context = ddl->ddl_context;
	u32 vcd_status = VCD_S_SUCCESS;
	u32 vcd_event = VCD_EVT_RESP_INPUT_DONE;
	u32 eos = false, status = false;

	if (ddl->decoding) {
		if (ddl_handle_dec_seq_hdr_fail_error(ddl))
			return true;
	}

	if ((ddl->cmd_state != DDL_CMD_DECODE_FRAME) &&
		(ddl->cmd_state != DDL_CMD_ENCODE_FRAME))
		return false;

	if (ddl->decoding &&
		(ddl->codec_data.decoder.field_needed_for_prev_ip == 1)) {
		ddl->codec_data.decoder.field_needed_for_prev_ip = 0;
		ddl_release_prev_field(ddl);
		if (ddl_context->cmd_err_status ==
		 VIDC_1080P_ERROR_NON_PAIRED_FIELD_NOT_SUPPORTED) {
			ddl_vidc_decode_frame_run(ddl);
			return true;
		}
	}

	switch (ddl_context->cmd_err_status) {
	case VIDC_1080P_ERROR_SYNC_POINT_NOT_RECEIVED:
		vcd_status = VCD_ERR_IFRAME_EXPECTED;
		break;
	case VIDC_1080P_ERROR_NO_BUFFER_RELEASED_FROM_HOST:
		{
			u32 pending_display = 0, release_mask;

			release_mask =
				ddl->codec_data.decoder.\
				dpb_mask.hw_mask;
			while (release_mask > 0) {
				if (release_mask & 0x1)
					pending_display++;
				release_mask >>= 1;
			}
			if (pending_display >= ddl->codec_data.\
				decoder.min_dpb_num) {
				DDL_MSG_ERROR("VIDC_FW_ISSUE_REQ_BUF");
				ddl_client_fatal_cb(ddl);
				status = true;
			} else {
				vcd_event = VCD_EVT_RESP_OUTPUT_REQ;
				DDL_MSG_LOW("VIDC_OUTPUT_BUF_REQ!!");
			}
			break;
		}
	case VIDC_1080P_ERROR_BIT_STREAM_BUF_EXHAUST:
	case VIDC_1080P_ERROR_DESCRIPTOR_TABLE_ENTRY_INVALID:
	case VIDC_1080P_ERROR_MB_COEFF_NOT_DONE:
	case VIDC_1080P_ERROR_CODEC_SLICE_NOT_DONE:
	case VIDC_1080P_ERROR_VIDC_CORE_TIME_OUT:
	case VIDC_1080P_ERROR_VC1_BITPLANE_DECODE_ERR:
	case VIDC_1080P_ERROR_RESOLUTION_MISMATCH:
	case VIDC_1080P_ERROR_NV_QUANT_ERR:
	case VIDC_1080P_ERROR_SYNC_MARKER_ERR:
	case VIDC_1080P_ERROR_FEATURE_NOT_SUPPORTED:
	case VIDC_1080P_ERROR_MEM_CORRUPTION:
	case VIDC_1080P_ERROR_INVALID_REFERENCE_FRAME:
	case VIDC_1080P_ERROR_PICTURE_CODING_TYPE_ERR:
	case VIDC_1080P_ERROR_MV_RANGE_ERR:
	case VIDC_1080P_ERROR_PICTURE_STRUCTURE_ERR:
	case VIDC_1080P_ERROR_SLICE_ADDR_INVALID:
	case VIDC_1080P_ERROR_NON_FRAME_DATA_RECEIVED:
	case VIDC_1080P_ERROR_NALU_HEADER_ERROR:
	case VIDC_1080P_ERROR_SPS_PARSE_ERROR:
	case VIDC_1080P_ERROR_PPS_PARSE_ERROR:
	case VIDC_1080P_ERROR_HEADER_NOT_FOUND:
	case VIDC_1080P_ERROR_SLICE_PARSE_ERROR:
	case VIDC_1080P_ERROR_NON_PAIRED_FIELD_NOT_SUPPORTED:
		vcd_status = VCD_ERR_BITSTREAM_ERR;
		DDL_MSG_ERROR("VIDC_BIT_STREAM_ERR");
		break;
	case VIDC_1080P_ERROR_B_FRAME_NOT_SUPPORTED:
	case VIDC_1080P_ERROR_UNSUPPORTED_FEATURE_IN_PROFILE:
	case VIDC_1080P_ERROR_RESOLUTION_NOT_SUPPORTED:
		if (ddl->decoding) {
			vcd_status = VCD_ERR_BITSTREAM_ERR;
			DDL_MSG_ERROR("VIDC_BIT_STREAM_ERR");
		}
		break;
	default:
		break;
	}

	if (((vcd_status) || (vcd_event != VCD_EVT_RESP_INPUT_DONE)) &&
		!status) {
				ddl->input_frame.frm_trans_end = true;
		eos = ((vcd_event == VCD_EVT_RESP_INPUT_DONE) &&
			(ddl->input_frame.vcd_frm.flags & VCD_FRAME_FLAG_EOS));
		if (((ddl->decoding) && (eos)) || !ddl->decoding)
			ddl->input_frame.frm_trans_end = false;
		ddl_input_failed_cb(ddl, vcd_event, vcd_status);
		if (!ddl->decoding) {
			ddl->output_frame.frm_trans_end = !eos;
			ddl->output_frame.vcd_frm.data_len = 0;
			ddl_context->ddl_callback(VCD_EVT_RESP_OUTPUT_DONE,
				VCD_ERR_FAIL, &ddl->output_frame,
				sizeof(struct ddl_frame_data_tag), (u32 *)ddl,
				ddl->client_data);
			if (eos) {
				DDL_MSG_LOW("VIDC_ENC_EOS_DONE");
				ddl_context->ddl_callback(VCD_EVT_RESP_EOS_DONE,
					VCD_S_SUCCESS, NULL, 0, (u32 *)ddl,
					ddl->client_data);
			}
		}
		if ((ddl->decoding) && (eos))
			ddl_vidc_decode_eos_run(ddl);
		else
			ddl_release_command_channel(ddl_context,
				ddl->command_channel);
			status = true;
	}
	return status;
}

static u32 ddl_handle_core_warnings(u32 err_status)
{
	u32 status = false;

	switch (err_status) {
	case VIDC_1080P_WARN_COMMAND_FLUSHED:
	case VIDC_1080P_WARN_FRAME_RATE_UNKNOWN:
	case VIDC_1080P_WARN_ASPECT_RATIO_UNKNOWN:
	case VIDC_1080P_WARN_COLOR_PRIMARIES_UNKNOWN:
	case VIDC_1080P_WARN_TRANSFER_CHAR_UNKNOWN:
	case VIDC_1080P_WARN_MATRIX_COEFF_UNKNOWN:
	case VIDC_1080P_WARN_NON_SEQ_SLICE_ADDR:
	case VIDC_1080P_WARN_BROKEN_LINK:
	case VIDC_1080P_WARN_FRAME_CONCEALED:
	case VIDC_1080P_WARN_PROFILE_UNKNOWN:
	case VIDC_1080P_WARN_LEVEL_UNKNOWN:
	case VIDC_1080P_WARN_BIT_RATE_NOT_SUPPORTED:
	case VIDC_1080P_WARN_COLOR_DIFF_FORMAT_NOT_SUPPORTED:
	case VIDC_1080P_WARN_NULL_EXTRA_METADATA_POINTER:
	case VIDC_1080P_WARN_DEBLOCKING_NOT_DONE:
	case VIDC_1080P_WARN_INCOMPLETE_FRAME:
	case VIDC_1080P_ERROR_NULL_FW_DEBUG_INFO_POINTER:
	case VIDC_1080P_ERROR_ALLOC_DEBUG_INFO_SIZE_INSUFFICIENT:
	case VIDC_1080P_WARN_METADATA_NO_SPACE_NUM_CONCEAL_MB:
	case VIDC_1080P_WARN_METADATA_NO_SPACE_QP:
	case VIDC_1080P_WARN_METADATA_NO_SPACE_CONCEAL_MB:
	case VIDC_1080P_WARN_METADATA_NO_SPACE_VC1_PARAM:
	case VIDC_1080P_WARN_METADATA_NO_SPACE_SEI:
	case VIDC_1080P_WARN_METADATA_NO_SPACE_VUI:
	case VIDC_1080P_WARN_METADATA_NO_SPACE_EXTRA:
	case VIDC_1080P_WARN_METADATA_NO_SPACE_DATA_NONE:
	case VIDC_1080P_WARN_METADATA_NO_SPACE_MB_INFO:
	case VIDC_1080P_WARN_METADATA_NO_SPACE_SLICE_SIZE:
	case VIDC_1080P_WARN_RESOLUTION_WARNING:
	case VIDC_1080P_WARN_NO_LONG_TERM_REFERENCE:
	case VIDC_1080P_WARN_NO_SPACE_MPEG2_DATA_DUMP:
	case VIDC_1080P_WARN_METADATA_NO_SPACE_MISSING_MB:
		status = true;
		DDL_MSG_ERROR("VIDC_WARNING_IGNORED");
	break;
	default:
	break;
	}
	return status;
}

u32 ddl_handle_core_errors(struct ddl_context *ddl_context)
{
	struct ddl_client_context *ddl;
	u32 channel_inst_id, status = false;
	u32 disp_status;

	if (!ddl_context->cmd_err_status &&
		!ddl_context->disp_pic_err_status) {
		DDL_MSG_ERROR("VIDC_NO_ERROR");
		return false;
	}
		vidc_1080p_get_returned_channel_inst_id(&channel_inst_id);
		vidc_1080p_clear_returned_channel_inst_id();
		ddl = ddl_get_current_ddl_client_for_channel_id(ddl_context,
			ddl_context->response_cmd_ch_id);
	if (!ddl) {
		DDL_MSG_ERROR("VIDC_SPURIOUS_INTERRUPT_ERROR");
		return true;
	}
	if (ddl_context->cmd_err_status) {
		print_core_errors(ddl_context->cmd_err_status);
		print_core_recoverable_errors(ddl_context->cmd_err_status);
	}
	if (ddl_context->disp_pic_err_status)
		print_core_errors(ddl_context->disp_pic_err_status);
	status = ddl_handle_core_warnings(ddl_context->cmd_err_status);
	disp_status = ddl_handle_core_warnings(
		ddl_context->disp_pic_err_status);
	if (!status && !disp_status) {
		DDL_MSG_ERROR("ddl_warning:Unknown");
		status = ddl_handle_hw_fatal_errors(ddl);
		if (!status)
			status = ddl_handle_core_recoverable_errors(ddl);
		if (!status)
			status = ddl_handle_client_fatal_errors(ddl);
	}
	return status;
}

static void ddl_release_prev_field(struct ddl_client_context *ddl)
{
	ddl->output_frame.vcd_frm.ip_frm_tag =
		ddl->codec_data.decoder.prev_ip_frm_tag;
		ddl->output_frame.vcd_frm.physical = NULL;
		ddl->output_frame.vcd_frm.virtual = NULL;
		ddl->output_frame.frm_trans_end = false;
		ddl->ddl_context->ddl_callback(VCD_EVT_RESP_OUTPUT_DONE,
			VCD_ERR_INTRLCD_FIELD_DROP, &(ddl->output_frame),
			sizeof(struct ddl_frame_data_tag),
			(u32 *) ddl, ddl->client_data);
}

static u32 ddl_handle_dec_seq_hdr_fail_error(struct ddl_client_context *ddl)
{
	struct ddl_context *ddl_context = ddl->ddl_context;
	u32 status = false;

	if ((ddl->cmd_state != DDL_CMD_HEADER_PARSE) ||
		(ddl->client_state != DDL_CLIENT_WAIT_FOR_INITCODECDONE)) {
		DDL_MSG_ERROR("STATE-CRITICAL-HDDONE");
		return false;
	}

	switch (ddl_context->cmd_err_status) {
	case VIDC_1080P_ERROR_UNSUPPORTED_FEATURE_IN_PROFILE:
	case VIDC_1080P_ERROR_RESOLUTION_NOT_SUPPORTED:
	case VIDC_1080P_ERROR_HEADER_NOT_FOUND:
	case VIDC_1080P_ERROR_SPS_PARSE_ERROR:
	case VIDC_1080P_ERROR_PPS_PARSE_ERROR:
	{
		struct ddl_decoder_data *decoder = &ddl->codec_data.decoder;
		if (ddl_context->cmd_err_status ==
			VIDC_1080P_ERROR_UNSUPPORTED_FEATURE_IN_PROFILE
			&& decoder->codec.codec == VCD_CODEC_H264) {
			DDL_MSG_ERROR("Unsupported Feature for H264");
			ddl_client_fatal_cb(ddl);
			return true;
		}
		if ((ddl_context->cmd_err_status ==
			VIDC_1080P_ERROR_RESOLUTION_NOT_SUPPORTED)
			&& (decoder->codec.codec == VCD_CODEC_H263
			|| decoder->codec.codec == VCD_CODEC_H264
			|| decoder->codec.codec == VCD_CODEC_MPEG4
			|| decoder->codec.codec == VCD_CODEC_VC1
			|| decoder->codec.codec == VCD_CODEC_VC1_RCV)) {
			DDL_MSG_ERROR("Unsupported resolution");
			ddl_client_fatal_cb(ddl);
			return true;
		}

		DDL_MSG_ERROR("SEQHDR-FAILED");
		if (decoder->header_in_start) {
			decoder->header_in_start = false;
			ddl_context->ddl_callback(VCD_EVT_RESP_START,
				VCD_ERR_SEQHDR_PARSE_FAIL, NULL, 0,
				(u32 *) ddl, ddl->client_data);
		} else {
			ddl->input_frame.frm_trans_end = true;
			if ((ddl->input_frame.vcd_frm.flags &
				VCD_FRAME_FLAG_EOS)) {
				ddl->input_frame.frm_trans_end = false;
			}
			ddl_vidc_decode_dynamic_property(ddl, false);
			ddl_context->ddl_callback(
				VCD_EVT_RESP_INPUT_DONE,
				VCD_ERR_SEQHDR_PARSE_FAIL, &ddl->input_frame,
				sizeof(struct ddl_frame_data_tag), (u32 *)ddl,
				ddl->client_data);
			if ((ddl->input_frame.vcd_frm.flags &
				VCD_FRAME_FLAG_EOS)) {
				DDL_MSG_HIGH("EOS_DONE-fromDDL");
				ddl_context->ddl_callback(VCD_EVT_RESP_EOS_DONE,
				VCD_S_SUCCESS, NULL, 0, (u32 *) ddl,
				ddl->client_data);
			}
		}
		DDL_MSG_LOW("ddl_state_transition: %s ~~> "
			"DDL_CLIENT_WAIT_FOR_INITCODEC",
			ddl_get_state_string(ddl->client_state));
		ddl->client_state = DDL_CLIENT_WAIT_FOR_INITCODEC;
		ddl_release_command_channel(ddl_context, ddl->command_channel);
		status = true;
		break;
	}
	default:
		break;
	}
	return status;
}

void print_core_errors(u32 error_code)
{
	s8 *string = NULL;

	switch (error_code) {
	case VIDC_1080P_ERROR_INVALID_CHANNEL_NUMBER:
		string = "VIDC_1080P_ERROR_INVALID_CHANNEL_NUMBER";
	break;
	case VIDC_1080P_ERROR_INVALID_COMMAND_ID:
		string = "VIDC_1080P_ERROR_INVALID_COMMAND_ID";
	break;
	case VIDC_1080P_ERROR_CHANNEL_ALREADY_IN_USE:
		string = "VIDC_1080P_ERROR_CHANNEL_ALREADY_IN_USE";
	break;
	case VIDC_1080P_ERROR_CHANNEL_NOT_OPEN_BEFORE_CHANNEL_CLOSE:
		string =
		"VIDC_1080P_ERROR_CHANNEL_NOT_OPEN_BEFORE_CHANNEL_CLOSE";
	break;
	case VIDC_1080P_ERROR_OPEN_CH_ERROR_SEQ_START:
		string = "VIDC_1080P_ERROR_OPEN_CH_ERROR_SEQ_START";
	break;
	case VIDC_1080P_ERROR_SEQ_START_ALREADY_CALLED:
		string = "VIDC_1080P_ERROR_SEQ_START_ALREADY_CALLED";
	break;
	case VIDC_1080P_ERROR_OPEN_CH_ERROR_INIT_BUFFERS:
		string = "VIDC_1080P_ERROR_OPEN_CH_ERROR_INIT_BUFFERS";
	break;
	case VIDC_1080P_ERROR_SEQ_START_ERROR_INIT_BUFFERS:
		string = "VIDC_1080P_ERROR_SEQ_START_ERROR_INIT_BUFFERS";
	break;
	case VIDC_1080P_ERROR_INIT_BUFFER_ALREADY_CALLED:
		string = "VIDC_1080P_ERROR_INIT_BUFFER_ALREADY_CALLED";
	break;
	case VIDC_1080P_ERROR_OPEN_CH_ERROR_FRAME_START:
		string = "VIDC_1080P_ERROR_OPEN_CH_ERROR_FRAME_START";
	break;
	case VIDC_1080P_ERROR_SEQ_START_ERROR_FRAME_START:
		string = "VIDC_1080P_ERROR_SEQ_START_ERROR_FRAME_START";
	break;
	case VIDC_1080P_ERROR_INIT_BUFFERS_ERROR_FRAME_START:
		string = "VIDC_1080P_ERROR_INIT_BUFFERS_ERROR_FRAME_START";
	break;
	case VIDC_1080P_ERROR_RESOLUTION_CHANGED:
		string = "VIDC_1080P_ERROR_RESOLUTION_CHANGED";
	break;
	case VIDC_1080P_ERROR_INVALID_COMMAND_LAST_FRAME:
		string = "VIDC_1080P_ERROR_INVALID_COMMAND_LAST_FRAME";
	break;
	case VIDC_1080P_ERROR_INVALID_COMMAND:
		string = "VIDC_1080P_ERROR_INVALID_COMMAND";
	break;
	case VIDC_1080P_ERROR_INVALID_CODEC_TYPE:
		string = "VIDC_1080P_ERROR_INVALID_CODEC_TYPE";
	break;
	case VIDC_1080P_ERROR_MEM_ALLOCATION_FAILED:
		string = "VIDC_1080P_ERROR_MEM_ALLOCATION_FAILED";
	break;
	case VIDC_1080P_ERROR_INSUFFICIENT_CONTEXT_SIZE:
		string = "VIDC_1080P_ERROR_INSUFFICIENT_CONTEXT_SIZE";
	break;
	case VIDC_1080P_ERROR_DIVIDE_BY_ZERO:
		string = "VIDC_1080P_ERROR_DIVIDE_BY_ZERO";
	break;
	case VIDC_1080P_ERROR_DESCRIPTOR_BUFFER_EMPTY:
		string = "VIDC_1080P_ERROR_DESCRIPTOR_BUFFER_EMPTY";
	break;
	case VIDC_1080P_ERROR_DMA_TX_NOT_COMPLETE:
		string = "VIDC_1080P_ERROR_DMA_TX_NOT_COMPLETE";
	break;
	case VIDC_1080P_ERROR_VSP_NOT_READY:
		string = "VIDC_1080P_ERROR_VSP_NOT_READY";
	break;
	case VIDC_1080P_ERROR_BUFFER_FULL_STATE:
		string = "VIDC_1080P_ERROR_BUFFER_FULL_STATE";
	break;
	case VIDC_1080P_ERROR_UNSUPPORTED_FEATURE_IN_PROFILE:
		string = "VIDC_1080P_ERROR_UNSUPPORTED_FEATURE_IN_PROFILE";
	break;
	case VIDC_1080P_ERROR_HEADER_NOT_FOUND:
		string = "VIDC_1080P_ERROR_HEADER_NOT_FOUND";
	break;
	case VIDC_1080P_ERROR_VOS_END_CODE_RECEIVED:
		string = "VIDC_1080P_ERROR_VOS_END_CODE_RECEIVED";
	break;
	case VIDC_1080P_ERROR_RESOLUTION_NOT_SUPPORTED:
		string = "VIDC_1080P_ERROR_RESOLUTION_NOT_SUPPORTED";
	break;
	case VIDC_1080P_ERROR_FRAME_RATE_NOT_SUPPORTED:
		string = "VIDC_1080P_ERROR_FRAME_RATE_NOT_SUPPORTED";
	break;
	case VIDC_1080P_ERROR_INVALID_QP_VALUE:
		string = "VIDC_1080P_ERROR_INVALID_QP_VALUE";
	break;
	case VIDC_1080P_ERROR_INVALID_RC_REACTION_COEFFICIENT:
		string = "VIDC_1080P_ERROR_INVALID_RC_REACTION_COEFFICIENT";
	break;
	case VIDC_1080P_ERROR_INVALID_CPB_SIZE_AT_GIVEN_LEVEL:
		string = "VIDC_1080P_ERROR_INVALID_CPB_SIZE_AT_GIVEN_LEVEL";
	break;
	case VIDC_1080P_ERROR_B_FRAME_NOT_SUPPORTED:
		string = "VIDC_1080P_ERROR_B_FRAME_NOT_SUPPORTED";
	break;
	case VIDC_1080P_ERROR_ALLOC_DPB_SIZE_NOT_SUFFICIENT:
		string = "VIDC_1080P_ERROR_ALLOC_DPB_SIZE_NOT_SUFFICIENT";
	break;
	case VIDC_1080P_ERROR_NUM_DPB_OUT_OF_RANGE:
		string = "VIDC_1080P_ERROR_NUM_DPB_OUT_OF_RANGE";
	break;
	case VIDC_1080P_ERROR_NULL_METADATA_INPUT_POINTER:
		string = "VIDC_1080P_ERROR_NULL_METADATA_INPUT_POINTER";
	break;
	case VIDC_1080P_ERROR_NULL_DPB_POINTER:
		string = "VIDC_1080P_ERROR_NULL_DPB_POINTER";
	break;
	case VIDC_1080P_ERROR_NULL_OTH_EXT_BUFADDR:
		string = "VIDC_1080P_ERROR_NULL_OTH_EXT_BUFADDR";
	break;
	case VIDC_1080P_ERROR_NULL_MV_POINTER:
		string = "VIDC_1080P_ERROR_NULL_MV_POINTER";
	break;
	case VIDC_1080P_ERROR_NON_PAIRED_FIELD_NOT_SUPPORTED:
		string = "VIDC_1080P_ERROR_NON_PAIRED_FIELD_NOT_SUPPORTED";
	break;
	case VIDC_1080P_WARN_COMMAND_FLUSHED:
		string = "VIDC_1080P_WARN_COMMAND_FLUSHED";
	break;
	case VIDC_1080P_WARN_FRAME_RATE_UNKNOWN:
		string = "VIDC_1080P_WARN_FRAME_RATE_UNKNOWN";
	break;
	case VIDC_1080P_WARN_ASPECT_RATIO_UNKNOWN:
		string = "VIDC_1080P_WARN_ASPECT_RATIO_UNKNOWN";
	break;
	case VIDC_1080P_WARN_COLOR_PRIMARIES_UNKNOWN:
		string = "VIDC_1080P_WARN_COLOR_PRIMARIES_UNKNOWN";
	break;
	case VIDC_1080P_WARN_TRANSFER_CHAR_UNKNOWN:
		string = "VIDC_1080P_WARN_TRANSFER_CHAR_UNKNOWN";
	break;
	case VIDC_1080P_WARN_MATRIX_COEFF_UNKNOWN:
		string = "VIDC_1080P_WARN_MATRIX_COEFF_UNKNOWN";
	break;
	case VIDC_1080P_WARN_NON_SEQ_SLICE_ADDR:
		string = "VIDC_1080P_WARN_NON_SEQ_SLICE_ADDR";
	break;
	case VIDC_1080P_WARN_BROKEN_LINK:
		string = "VIDC_1080P_WARN_BROKEN_LINK";
	break;
	case VIDC_1080P_WARN_FRAME_CONCEALED:
		string = "VIDC_1080P_WARN_FRAME_CONCEALED";
	break;
	case VIDC_1080P_WARN_PROFILE_UNKNOWN:
		string = "VIDC_1080P_WARN_PROFILE_UNKNOWN";
	break;
	case VIDC_1080P_WARN_LEVEL_UNKNOWN:
		string = "VIDC_1080P_WARN_LEVEL_UNKNOWN";
	break;
	case VIDC_1080P_WARN_BIT_RATE_NOT_SUPPORTED:
		string = "VIDC_1080P_WARN_BIT_RATE_NOT_SUPPORTED";
	break;
	case VIDC_1080P_WARN_COLOR_DIFF_FORMAT_NOT_SUPPORTED:
		string = "VIDC_1080P_WARN_COLOR_DIFF_FORMAT_NOT_SUPPORTED";
	break;
	case VIDC_1080P_WARN_NULL_EXTRA_METADATA_POINTER:
		string = "VIDC_1080P_WARN_NULL_EXTRA_METADATA_POINTER";
	break;
	case VIDC_1080P_WARN_DEBLOCKING_NOT_DONE:
		string = "VIDC_1080P_WARN_DEBLOCKING_NOT_DONE";
	break;
	case VIDC_1080P_WARN_INCOMPLETE_FRAME:
		string = "VIDC_1080P_WARN_INCOMPLETE_FRAME";
	break;
	case VIDC_1080P_ERROR_NULL_FW_DEBUG_INFO_POINTER:
		string = "VIDC_1080P_ERROR_NULL_FW_DEBUG_INFO_POINTER";
	break;
	case VIDC_1080P_ERROR_ALLOC_DEBUG_INFO_SIZE_INSUFFICIENT:
		string =
		"VIDC_1080P_ERROR_ALLOC_DEBUG_INFO_SIZE_INSUFFICIENT";
	break;
	case VIDC_1080P_WARN_METADATA_NO_SPACE_NUM_CONCEAL_MB:
		string = "VIDC_1080P_WARN_METADATA_NO_SPACE_NUM_CONCEAL_MB";
	break;
	case VIDC_1080P_WARN_METADATA_NO_SPACE_QP:
		string = "VIDC_1080P_WARN_METADATA_NO_SPACE_QP";
	break;
	case VIDC_1080P_WARN_METADATA_NO_SPACE_CONCEAL_MB:
		string = "VIDC_1080P_WARN_METADATA_NO_SPACE_CONCEAL_MB";
	break;
	case VIDC_1080P_WARN_METADATA_NO_SPACE_VC1_PARAM:
		string = "VIDC_1080P_WARN_METADATA_NO_SPACE_VC1_PARAM";
	break;
	case VIDC_1080P_WARN_METADATA_NO_SPACE_SEI:
		string = "VIDC_1080P_WARN_METADATA_NO_SPACE_SEI";
	break;
	case VIDC_1080P_WARN_METADATA_NO_SPACE_VUI:
		string = "VIDC_1080P_WARN_METADATA_NO_SPACE_VUI";
	break;
	case VIDC_1080P_WARN_METADATA_NO_SPACE_EXTRA:
		string = "VIDC_1080P_WARN_METADATA_NO_SPACE_EXTRA";
	break;
	case VIDC_1080P_WARN_METADATA_NO_SPACE_DATA_NONE:
		string = "VIDC_1080P_WARN_METADATA_NO_SPACE_DATA_NONE";
	break;
	case VIDC_1080P_WARN_METADATA_NO_SPACE_MB_INFO:
		string = "VIDC_1080P_WARN_METADATA_NO_SPACE_MB_INFO";
	break;
	case VIDC_1080P_WARN_METADATA_NO_SPACE_SLICE_SIZE:
		string = "VIDC_1080P_WARN_METADATA_NO_SPACE_SLICE_SIZE";
	break;
	case VIDC_1080P_WARN_RESOLUTION_WARNING:
		string = "VIDC_1080P_WARN_RESOLUTION_WARNING";
	break;
	case VIDC_1080P_WARN_NO_LONG_TERM_REFERENCE:
		string = "VIDC_1080P_WARN_NO_LONG_TERM_REFERENCE";
	break;
	case VIDC_1080P_WARN_NO_SPACE_MPEG2_DATA_DUMP:
		string = "VIDC_1080P_WARN_NO_SPACE_MPEG2_DATA_DUMP";
	break;
	case VIDC_1080P_WARN_METADATA_NO_SPACE_MISSING_MB:
		string = "VIDC_1080P_WARN_METADATA_NO_SPACE_MISSING_MB";
	break;
	}
	if (string)
		DDL_MSG_ERROR("Error code = 0x%x : %s", error_code, string);
}

void print_core_recoverable_errors(u32 error_code)
{
	s8 *string = NULL;

	switch (error_code) {
	case VIDC_1080P_ERROR_SYNC_POINT_NOT_RECEIVED:
		string = "VIDC_1080P_ERROR_SYNC_POINT_NOT_RECEIVED";
	break;
	case VIDC_1080P_ERROR_NO_BUFFER_RELEASED_FROM_HOST:
		string = "VIDC_1080P_ERROR_NO_BUFFER_RELEASED_FROM_HOST";
	break;
	case VIDC_1080P_ERROR_BIT_STREAM_BUF_EXHAUST:
		string = "VIDC_1080P_ERROR_BIT_STREAM_BUF_EXHAUST";
	break;
	case VIDC_1080P_ERROR_DESCRIPTOR_TABLE_ENTRY_INVALID:
		string = "VIDC_1080P_ERROR_DESCRIPTOR_TABLE_ENTRY_INVALID";
	break;
	case VIDC_1080P_ERROR_MB_COEFF_NOT_DONE:
		string = "VIDC_1080P_ERROR_MB_COEFF_NOT_DONE";
	break;
	case VIDC_1080P_ERROR_CODEC_SLICE_NOT_DONE:
		string = "VIDC_1080P_ERROR_CODEC_SLICE_NOT_DONE";
	break;
	case VIDC_1080P_ERROR_VIDC_CORE_TIME_OUT:
		string = "VIDC_1080P_ERROR_VIDC_CORE_TIME_OUT";
	break;
	case VIDC_1080P_ERROR_VC1_BITPLANE_DECODE_ERR:
		string = "VIDC_1080P_ERROR_VC1_BITPLANE_DECODE_ERR";
	break;
	case VIDC_1080P_ERROR_RESOLUTION_MISMATCH:
		string = "VIDC_1080P_ERROR_RESOLUTION_MISMATCH";
	break;
	case VIDC_1080P_ERROR_NV_QUANT_ERR:
		string = "VIDC_1080P_ERROR_NV_QUANT_ERR";
	break;
	case VIDC_1080P_ERROR_SYNC_MARKER_ERR:
		string = "VIDC_1080P_ERROR_SYNC_MARKER_ERR";
	break;
	case VIDC_1080P_ERROR_FEATURE_NOT_SUPPORTED:
		string = "VIDC_1080P_ERROR_FEATURE_NOT_SUPPORTED";
	break;
	case VIDC_1080P_ERROR_MEM_CORRUPTION:
		string = "VIDC_1080P_ERROR_MEM_CORRUPTION";
	break;
	case VIDC_1080P_ERROR_INVALID_REFERENCE_FRAME:
		string = "VIDC_1080P_ERROR_INVALID_REFERENCE_FRAME";
	break;
	case VIDC_1080P_ERROR_PICTURE_CODING_TYPE_ERR:
		string = "VIDC_1080P_ERROR_PICTURE_CODING_TYPE_ERR";
	break;
	case VIDC_1080P_ERROR_MV_RANGE_ERR:
		string = "VIDC_1080P_ERROR_MV_RANGE_ERR";
	break;
	case VIDC_1080P_ERROR_PICTURE_STRUCTURE_ERR:
		string = "VIDC_1080P_ERROR_PICTURE_STRUCTURE_ERR";
	break;
	case VIDC_1080P_ERROR_SLICE_ADDR_INVALID:
		string = "VIDC_1080P_ERROR_SLICE_ADDR_INVALID";
	break;
	case VIDC_1080P_ERROR_NON_FRAME_DATA_RECEIVED:
		string = "VIDC_1080P_ERROR_NON_FRAME_DATA_RECEIVED";
	break;
	case VIDC_1080P_ERROR_NALU_HEADER_ERROR:
		string = "VIDC_1080P_ERROR_NALU_HEADER_ERROR";
	break;
	case VIDC_1080P_ERROR_SPS_PARSE_ERROR:
		string = "VIDC_1080P_ERROR_SPS_PARSE_ERROR";
	break;
	case VIDC_1080P_ERROR_PPS_PARSE_ERROR:
		string = "VIDC_1080P_ERROR_PPS_PARSE_ERROR";
	break;
	case VIDC_1080P_ERROR_SLICE_PARSE_ERROR:
		string = "VIDC_1080P_ERROR_SLICE_PARSE_ERROR";
	break;
	}
	if (string)
		DDL_MSG_ERROR("Recoverable Error code = 0x%x : %s",
					  error_code, string);
}
