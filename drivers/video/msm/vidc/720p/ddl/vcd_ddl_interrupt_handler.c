/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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

#include "vidc_type.h"
#include "vidc.h"
#include "vcd_ddl_utils.h"
#include "vcd_ddl_metadata.h"

#if DEBUG
#define DBG(x...) printk(KERN_DEBUG x)
#else
#define DBG(x...)
#endif

static void ddl_decoder_input_done_callback(
	struct	ddl_client_context *ddl, u32 frame_transact_end);
static u32 ddl_decoder_output_done_callback(
	struct ddl_client_context *ddl, u32 frame_transact_end);

static u32 ddl_get_frame
    (struct vcd_frame_data *frame, u32 frame_type);

static void ddl_getdec_profilelevel
(struct ddl_decoder_data   *decoder, u32 profile, u32 level);

static void ddl_dma_done_callback(struct ddl_context *ddl_context)
{
	if (!DDLCOMMAND_STATE_IS(ddl_context, DDL_CMD_DMA_INIT)) {
		VIDC_LOGERR_STRING("UNKWN_DMADONE");
		return;
	}
	ddl_move_command_state(ddl_context, DDL_CMD_INVALID);
	VIDC_LOG_STRING("DMA_DONE");
	ddl_core_start_cpu(ddl_context);
}

static void ddl_cpu_started_callback(struct ddl_context *ddl_context)
{
	ddl_move_command_state(ddl_context, DDL_CMD_INVALID);
	VIDC_LOG_STRING("CPU-STARTED");

	if (!vidc_720p_cpu_start()) {
		ddl_hw_fatal_cb(ddl_context);
		return;
	}

	vidc_720p_set_deblock_line_buffer(
			ddl_context->db_line_buffer.align_physical_addr,
			ddl_context->db_line_buffer.buffer_size);
	ddl_context->device_state = DDL_DEVICE_INITED;
	ddl_context->ddl_callback(VCD_EVT_RESP_DEVICE_INIT, VCD_S_SUCCESS,
			NULL, 0, NULL, ddl_context->client_data);
	DDL_IDLE(ddl_context);
}


static u32 ddl_eos_done_callback(struct ddl_context *ddl_context)
{
	struct ddl_client_context *ddl = ddl_context->current_ddl;
	u32 displaystatus, resl_change;

	if (!DDLCOMMAND_STATE_IS(ddl_context, DDL_CMD_EOS)) {
		VIDC_LOGERR_STRING("UNKWN_EOSDONE");
		ddl_client_fatal_cb(ddl_context);
		return true;
	}

	if (!ddl ||
		!ddl->decoding ||
		!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_EOS_DONE)
		) {
		VIDC_LOG_STRING("STATE-CRITICAL-EOSDONE");
		ddl_client_fatal_cb(ddl_context);
		return true;
	}
	ddl_move_command_state(ddl_context, DDL_CMD_INVALID);

	vidc_720p_eos_info(&displaystatus, &resl_change);
	if ((enum vidc_720p_display_status)displaystatus
		!= VIDC_720P_EMPTY_BUFFER) {
		VIDC_LOG_STRING("EOSDONE-EMPTYBUF-ISSUE");
	}

	ddl_decode_dynamic_property(ddl, false);
	if (resl_change == 0x1) {
		ddl->codec_data.decoder.header_in_start = false;
		ddl->codec_data.decoder.decode_config.sequence_header =
			ddl->input_frame.vcd_frm.physical;
		ddl->codec_data.decoder.decode_config.sequence_header_len =
			ddl->input_frame.vcd_frm.data_len;
		ddl_decode_init_codec(ddl);
		return false;
	}
	ddl_move_client_state(ddl, DDL_CLIENT_WAIT_FOR_FRAME);
	VIDC_LOG_STRING("EOS_DONE");
	ddl_context->ddl_callback(VCD_EVT_RESP_EOS_DONE, VCD_S_SUCCESS,
		NULL, 0, (u32 *) ddl, ddl_context->client_data);
	DDL_IDLE(ddl_context);

	return true;
}

static u32 ddl_channel_set_callback(struct ddl_context *ddl_context)
{
	struct ddl_client_context *ddl = ddl_context->current_ddl;
	u32 return_status = false;

	ddl_move_command_state(ddl_context, DDL_CMD_INVALID);
	VIDC_DEBUG_REGISTER_LOG;

	if (!ddl ||
		!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_CHDONE)
		) {
		VIDC_LOG_STRING("STATE-CRITICAL-CHSET");
		DDL_IDLE(ddl_context);
		return return_status;
	}
	VIDC_LOG_STRING("Channel-set");
	ddl_move_client_state(ddl, DDL_CLIENT_WAIT_FOR_INITCODEC);

	if (ddl->decoding) {
		if (vidc_msg_timing)
			ddl_calc_core_proc_time(__func__, DEC_OP_TIME);
		if (ddl->codec_data.decoder.header_in_start) {
			ddl_decode_init_codec(ddl);
		} else {
			ddl_context->ddl_callback(VCD_EVT_RESP_START,
				VCD_S_SUCCESS, NULL,
				0, (u32 *) ddl,
				ddl_context->client_data);

			DDL_IDLE(ddl_context);
			return_status = true;
		}
	} else {
		ddl_encode_init_codec(ddl);
	}
	return return_status;
}

static void ddl_init_codec_done_callback(struct ddl_context *ddl_context)
{
	struct ddl_client_context *ddl = ddl_context->current_ddl;
	struct ddl_encoder_data *encoder;

	if (!ddl ||
		ddl->decoding ||
		!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_INITCODECDONE)
		) {
		VIDC_LOG_STRING("STATE-CRITICAL-INITCODEC");
		ddl_client_fatal_cb(ddl_context);
		return;
	}
	ddl_move_command_state(ddl_context, DDL_CMD_INVALID);
	ddl_move_client_state(ddl, DDL_CLIENT_WAIT_FOR_FRAME);
	VIDC_LOG_STRING("INIT_CODEC_DONE");

	encoder = &ddl->codec_data.encoder;
	if (encoder->seq_header.virtual_base_addr) {
		vidc_720p_encode_get_header(&encoder->seq_header.
			buffer_size);
	}

	ddl_context->ddl_callback(VCD_EVT_RESP_START, VCD_S_SUCCESS, NULL,
		0, (u32 *) ddl, ddl_context->client_data);

	DDL_IDLE(ddl_context);
}

static u32 ddl_header_done_callback(struct ddl_context *ddl_context)
{
	struct ddl_client_context *ddl = ddl_context->current_ddl;
	struct ddl_decoder_data *decoder;
	struct vidc_720p_seq_hdr_info seq_hdr_info;

	u32 process_further = true;
	u32 seq_hdr_only_frame = false;
	u32 need_reconfig = true;
	struct vcd_frame_data *input_vcd_frm;
	struct ddl_frame_data_tag *reconfig_payload = NULL;
	u32 reconfig_payload_size = 0;

	if (!ddl ||
		!ddl->decoding ||
		!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_INITCODECDONE)
		) {
		VIDC_LOG_STRING("STATE-CRITICAL-HDDONE");
		ddl_client_fatal_cb(ddl_context);
		return true;
	}
	if (vidc_msg_timing)
		ddl_calc_core_proc_time(__func__, DEC_OP_TIME);
	ddl_move_command_state(ddl_context, DDL_CMD_INVALID);
	ddl_move_client_state(ddl, DDL_CLIENT_WAIT_FOR_DPB);
	VIDC_LOG_STRING("HEADER_DONE");
	VIDC_DEBUG_REGISTER_LOG;

	vidc_720p_decode_get_seq_hdr_info(&seq_hdr_info);

	decoder = &(ddl->codec_data.decoder);
	decoder->frame_size.width = seq_hdr_info.img_size_x;
	decoder->frame_size.height = seq_hdr_info.img_size_y;
	decoder->min_dpb_num = seq_hdr_info.min_num_dpb;
	decoder->y_cb_cr_size = seq_hdr_info.min_dpb_size;
	decoder->progressive_only = 1 - seq_hdr_info.progressive;
	if (!seq_hdr_info.img_size_x || !seq_hdr_info.img_size_y) {
		VIDC_LOGERR_STRING("FATAL: ZeroImageSize");
		ddl_client_fatal_cb(ddl_context);
		return process_further;
	}
	if (seq_hdr_info.data_partitioned == 0x1 &&
		decoder->codec.codec == VCD_CODEC_MPEG4 &&
		seq_hdr_info.img_size_x > DDL_MAX_DP_FRAME_WIDTH &&
		seq_hdr_info.img_size_y > DDL_MAX_DP_FRAME_HEIGHT)	{
		ddl_client_fatal_cb(ddl_context);
		return process_further;
	}
	ddl_getdec_profilelevel(decoder, seq_hdr_info.profile,
		seq_hdr_info.level);
	ddl_calculate_stride(&decoder->frame_size,
			!decoder->progressive_only,
			decoder->codec.codec);
	if (decoder->buf_format.buffer_format == VCD_BUFFER_FORMAT_TILE_4x2) {
		decoder->frame_size.stride =
		DDL_TILE_ALIGN(decoder->frame_size.width,
					DDL_TILE_ALIGN_WIDTH);
		decoder->frame_size.scan_lines =
			DDL_TILE_ALIGN(decoder->frame_size.height,
						 DDL_TILE_ALIGN_HEIGHT);
	}
	if (seq_hdr_info.crop_exists)	{
		decoder->frame_size.width -=
		(seq_hdr_info.crop_right_offset
		+ seq_hdr_info.crop_left_offset);
		decoder->frame_size.height -=
		(seq_hdr_info.crop_top_offset +
		seq_hdr_info.crop_bottom_offset);
	}
	ddl_set_default_decoder_buffer_req(decoder, false);

	if (decoder->header_in_start) {
		decoder->client_frame_size = decoder->frame_size;
		decoder->client_output_buf_req =
			decoder->actual_output_buf_req;
		decoder->client_input_buf_req =
			decoder->actual_input_buf_req;
		ddl_context->ddl_callback(VCD_EVT_RESP_START, VCD_S_SUCCESS,
			NULL, 0, (u32 *) ddl,	ddl_context->client_data);
		DDL_IDLE(ddl_context);
	} else {
		DBG("%s(): Client data: WxH(%u x %u) SxSL(%u x %u) Sz(%u)\n",
			__func__, decoder->client_frame_size.width,
			decoder->client_frame_size.height,
			decoder->client_frame_size.stride,
			decoder->client_frame_size.scan_lines,
			decoder->client_output_buf_req.sz);
		DBG("%s(): DDL data: WxH(%u x %u) SxSL(%u x %u) Sz(%u)\n",
			__func__, decoder->frame_size.width,
			decoder->frame_size.height,
			decoder->frame_size.stride,
			decoder->frame_size.scan_lines,
			decoder->actual_output_buf_req.sz);
		DBG("%s(): min_dpb_num = %d actual_count = %d\n", __func__,
			decoder->min_dpb_num,
			decoder->client_output_buf_req.actual_count);

		input_vcd_frm = &(ddl->input_frame.vcd_frm);

		if (decoder->frame_size.width ==
			decoder->client_frame_size.width
			&& decoder->frame_size.height ==
			decoder->client_frame_size.height
			&& decoder->frame_size.stride ==
			decoder->client_frame_size.stride
			&& decoder->frame_size.scan_lines ==
			decoder->client_frame_size.scan_lines
			&& decoder->actual_output_buf_req.sz <=
			decoder->client_output_buf_req.sz
			&& decoder->actual_output_buf_req.actual_count <=
			decoder->client_output_buf_req.actual_count
			&& decoder->progressive_only)
			need_reconfig = false;
		if ((input_vcd_frm->data_len <= seq_hdr_info.dec_frm_size ||
			 (input_vcd_frm->flags & VCD_FRAME_FLAG_CODECCONFIG)) &&
			(!need_reconfig ||
			 !(input_vcd_frm->flags & VCD_FRAME_FLAG_EOS))) {
			input_vcd_frm->flags |=
				VCD_FRAME_FLAG_CODECCONFIG;
			seq_hdr_only_frame = true;
			input_vcd_frm->data_len = 0;
			ddl->input_frame.frm_trans_end = !need_reconfig;
			ddl_context->ddl_callback(
				VCD_EVT_RESP_INPUT_DONE,
				VCD_S_SUCCESS, &ddl->input_frame,
				sizeof(struct ddl_frame_data_tag),
				(u32 *) ddl,
				ddl->ddl_context->client_data);
		} else if (decoder->codec.codec != VCD_CODEC_H263) {
			input_vcd_frm->offset += seq_hdr_info.dec_frm_size;
			input_vcd_frm->data_len -= seq_hdr_info.dec_frm_size;
		}
		if (need_reconfig) {
			decoder->client_frame_size = decoder->frame_size;
			decoder->client_output_buf_req =
				decoder->actual_output_buf_req;
			decoder->client_input_buf_req =
				decoder->actual_input_buf_req;
			if (!seq_hdr_only_frame) {
				reconfig_payload = &ddl->input_frame;
				reconfig_payload_size =
					sizeof(struct ddl_frame_data_tag);
			}
			ddl_context->ddl_callback(VCD_EVT_IND_OUTPUT_RECONFIG,
					VCD_S_SUCCESS, reconfig_payload,
					reconfig_payload_size,
					(u32 *) ddl,
					ddl_context->client_data);
		}
		if (!need_reconfig && !seq_hdr_only_frame) {
			if (ddl_decode_set_buffers(ddl) == VCD_S_SUCCESS)
				process_further = false;
			else
				ddl_client_fatal_cb(ddl_context);
		} else
			DDL_IDLE(ddl_context);
	}
	return process_further;
}

static u32 ddl_dpb_buffers_set_done_callback(struct ddl_context
						  *ddl_context)
{
	struct ddl_client_context *ddl = ddl_context->current_ddl;

	ddl_move_command_state(ddl_context, DDL_CMD_INVALID);
	if (!ddl ||
		!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_DPBDONE)
		) {
		VIDC_LOG_STRING("STATE-CRITICAL-DPBDONE");
		ddl_client_fatal_cb(ddl_context);
		return true;
	}
	if (vidc_msg_timing) {
		ddl_calc_core_proc_time(__func__, DEC_OP_TIME);
		ddl_reset_core_time_variables(DEC_OP_TIME);
	}
	VIDC_LOG_STRING("INTR_DPBDONE");
	ddl_move_client_state(ddl, DDL_CLIENT_WAIT_FOR_FRAME);
	ddl->codec_data.decoder.dec_disp_info.img_size_x = 0;
	ddl->codec_data.decoder.dec_disp_info.img_size_y = 0;
	ddl_decode_frame_run(ddl);
	return false;
}

static void ddl_encoder_frame_run_callback(struct ddl_context
					   *ddl_context)
{
	struct ddl_client_context *ddl = ddl_context->current_ddl;
	struct ddl_encoder_data *encoder = &(ddl->codec_data.encoder);
	u32 eos_present = false;

	if (!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_FRAME_DONE)
		) {
		VIDC_LOG_STRING("STATE-CRITICAL-ENCFRMRUN");
		ddl_client_fatal_cb(ddl_context);
		return;
	}

	VIDC_LOG_STRING("ENC_FRM_RUN_DONE");

	ddl_move_command_state(ddl_context, DDL_CMD_INVALID);
	vidc_720p_enc_frame_info(&encoder->enc_frame_info);

	ddl->output_frame.vcd_frm.ip_frm_tag =
		ddl->input_frame.vcd_frm.ip_frm_tag;
	ddl->output_frame.vcd_frm.data_len =
		encoder->enc_frame_info.enc_size;
	ddl->output_frame.vcd_frm.flags |= VCD_FRAME_FLAG_ENDOFFRAME;
	ddl_get_frame
		(&(ddl->output_frame.vcd_frm),
		 encoder->enc_frame_info.frame);
	ddl_process_encoder_metadata(ddl);

	ddl_encode_dynamic_property(ddl, false);

	ddl->input_frame.frm_trans_end = false;
	ddl_context->ddl_callback(VCD_EVT_RESP_INPUT_DONE, VCD_S_SUCCESS,
		&(ddl->input_frame), sizeof(struct ddl_frame_data_tag),
		(u32 *) ddl, ddl_context->client_data);

	if (vidc_msg_timing)
		ddl_calc_core_proc_time(__func__, ENC_OP_TIME);

	/* check the presence of EOS */
   eos_present =
	((VCD_FRAME_FLAG_EOS & ddl->input_frame.vcd_frm.flags));

	ddl->output_frame.frm_trans_end = !eos_present;
	ddl_context->ddl_callback(VCD_EVT_RESP_OUTPUT_DONE, VCD_S_SUCCESS,
		&(ddl->output_frame),	sizeof(struct ddl_frame_data_tag),
		(u32 *) ddl, ddl_context->client_data);

	if (eos_present) {
		VIDC_LOG_STRING("ENC-EOS_DONE");
		ddl_context->ddl_callback(VCD_EVT_RESP_EOS_DONE,
				VCD_S_SUCCESS, NULL, 0,	(u32 *)ddl,
				ddl_context->client_data);
	}

	ddl_move_client_state(ddl, DDL_CLIENT_WAIT_FOR_FRAME);
	DDL_IDLE(ddl_context);
}

static u32 ddl_decoder_frame_run_callback(struct ddl_context
					   *ddl_context)
{
	struct ddl_client_context *ddl = ddl_context->current_ddl;
	struct vidc_720p_dec_disp_info *dec_disp_info =
	    &(ddl->codec_data.decoder.dec_disp_info);
	u32 callback_end = false;
	u32 status = true, eos_present = false;;

	if (!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_FRAME_DONE)) {
		VIDC_LOG_STRING("STATE-CRITICAL-DECFRMRUN");
		ddl_client_fatal_cb(ddl_context);
		return true;
	}

	VIDC_LOG_STRING("DEC_FRM_RUN_DONE");

	ddl_move_command_state(ddl_context, DDL_CMD_INVALID);

	vidc_720p_decode_display_info(dec_disp_info);

	ddl_decode_dynamic_property(ddl, false);

	if (dec_disp_info->resl_change) {
		VIDC_LOG_STRING
			("DEC_FRM_RUN_DONE: RECONFIG");
		ddl_move_client_state(ddl, DDL_CLIENT_WAIT_FOR_EOS_DONE);
		ddl_move_command_state(ddl_context, DDL_CMD_EOS);
		vidc_720p_submit_command(ddl->channel_id,
			VIDC_720P_CMD_FRAMERUN_REALLOCATE);
		return false;
	}

	if ((VCD_FRAME_FLAG_EOS & ddl->input_frame.vcd_frm.flags)) {
		callback_end = false;
		eos_present = true;
	}


	if (dec_disp_info->disp_status == VIDC_720P_DECODE_ONLY ||
		dec_disp_info->disp_status
			== VIDC_720P_DECODE_AND_DISPLAY) {
		if (!eos_present)
			callback_end = (dec_disp_info->disp_status
					== VIDC_720P_DECODE_ONLY);

	  ddl_decoder_input_done_callback(ddl, callback_end);
	}

	if (dec_disp_info->disp_status == VIDC_720P_DECODE_AND_DISPLAY
		|| dec_disp_info->disp_status == VIDC_720P_DISPLAY_ONLY) {
		if (!eos_present)
			callback_end =
			(dec_disp_info->disp_status
				== VIDC_720P_DECODE_AND_DISPLAY);

		if (ddl_decoder_output_done_callback(ddl, callback_end)
			!= VCD_S_SUCCESS)
			return true;
	}

	if (dec_disp_info->disp_status ==  VIDC_720P_DISPLAY_ONLY ||
		dec_disp_info->disp_status ==  VIDC_720P_EMPTY_BUFFER) {
		/* send the same input once again for decoding */
		ddl_decode_frame_run(ddl);
		/* client need to ignore the interrupt */
		status = false;
	} else if (eos_present) {
		/* send EOS command to HW */
		ddl_decode_eos_run(ddl);
		/* client need to ignore the interrupt */
		status = false;
	} else {
		ddl_move_client_state(ddl, DDL_CLIENT_WAIT_FOR_FRAME);
		/* move to Idle */
		DDL_IDLE(ddl_context);
	}
	return status;
}

static u32 ddl_eos_frame_done_callback(struct ddl_context *ddl_context)
{
	struct ddl_client_context *ddl = ddl_context->current_ddl;
	struct ddl_decoder_data *decoder = &(ddl->codec_data.decoder);
	struct vidc_720p_dec_disp_info *dec_disp_info =
		&(decoder->dec_disp_info);

	if (!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_EOS_DONE)) {
		VIDC_LOGERR_STRING("STATE-CRITICAL-EOSFRMRUN");
		ddl_client_fatal_cb(ddl_context);
		return true;
	}
	VIDC_LOG_STRING("EOS_FRM_RUN_DONE");

	ddl_move_command_state(ddl_context, DDL_CMD_INVALID);

	vidc_720p_decode_display_info(dec_disp_info);

	ddl_decode_dynamic_property(ddl, false);

	if (dec_disp_info->disp_status == VIDC_720P_DISPLAY_ONLY) {
		if (ddl_decoder_output_done_callback(ddl, false)
			!= VCD_S_SUCCESS)
			return true;
	} else
		VIDC_LOG_STRING("STATE-CRITICAL-WRONG-DISP-STATUS");

	ddl_decoder_dpb_transact(decoder, NULL, DDL_DPB_OP_SET_MASK);
	ddl_move_command_state(ddl_context, DDL_CMD_EOS);
	vidc_720p_submit_command(ddl->channel_id,
		VIDC_720P_CMD_FRAMERUN);
	return false;
}

static void ddl_channel_end_callback(struct ddl_context *ddl_context)
{
	struct ddl_client_context *ddl;

	ddl_move_command_state(ddl_context, DDL_CMD_INVALID);
	VIDC_LOG_STRING("CH_END_DONE");

	ddl = ddl_context->current_ddl;
	if (!ddl ||
		!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_CHEND)
		) {
		VIDC_LOG_STRING("STATE-CRITICAL-CHEND");
		DDL_IDLE(ddl_context);
		return;
	}

	ddl_release_client_internal_buffers(ddl);
	ddl_context->ddl_callback(VCD_EVT_RESP_STOP, VCD_S_SUCCESS,
		NULL, 0, (u32 *) ddl,	ddl_context->client_data);
	ddl_move_client_state(ddl, DDL_CLIENT_OPEN);
	DDL_IDLE(ddl_context);
}

static u32 ddl_operation_done_callback(struct ddl_context *ddl_context)
{
	u32 return_status = true;

	switch (ddl_context->cmd_state) {
	case DDL_CMD_DECODE_FRAME:
		{
			return_status = ddl_decoder_frame_run_callback(
				ddl_context);
			break;
		}
	case DDL_CMD_ENCODE_FRAME:
		{
			ddl_encoder_frame_run_callback(ddl_context);
			break;
		}
	case DDL_CMD_CHANNEL_SET:
		{
			return_status = ddl_channel_set_callback(
				ddl_context);
			break;
		}
	case DDL_CMD_INIT_CODEC:
		{
			ddl_init_codec_done_callback(ddl_context);
			break;
		}
	case DDL_CMD_HEADER_PARSE:
		{
			return_status = ddl_header_done_callback(
				ddl_context);
			break;
		}
	case DDL_CMD_DECODE_SET_DPB:
		{
			return_status = ddl_dpb_buffers_set_done_callback(
				ddl_context);
			break;
		}
	case DDL_CMD_CHANNEL_END:
		{
			ddl_channel_end_callback(ddl_context);
			break;
		}
	case DDL_CMD_EOS:
		{
			return_status = ddl_eos_frame_done_callback(
				ddl_context);
			break;
		}
	case DDL_CMD_CPU_RESET:
		{
			ddl_cpu_started_callback(ddl_context);
			break;
		}
	default:
		{
			VIDC_LOG_STRING("UNKWN_OPDONE");
			return_status = false;
			break;
		}
	}
	return return_status;
}

static u32 ddl_process_intr_status(struct ddl_context *ddl_context,
			u32 int_status)
{
	u32 status = true;
	switch (int_status) {
	case VIDC_720P_INTR_FRAME_DONE:
		 {
			status = ddl_operation_done_callback(ddl_context);
			break;
		 }
	case VIDC_720P_INTR_DMA_DONE:
		 {
			ddl_dma_done_callback(ddl_context);
			status = false;
			break;
		 }
	case VIDC_720P_INTR_FW_DONE:
		 {
			status = ddl_eos_done_callback(ddl_context);
			break;
		 }
	case VIDC_720P_INTR_BUFFER_FULL:
		 {
			VIDC_LOGERR_STRING("BUF_FULL_INTR");
			ddl_hw_fatal_cb(ddl_context);
			break;
		 }
	default:
		 {
			VIDC_LOGERR_STRING("UNKWN_INTR");
			break;
		 }
	}
	return status;
}

void ddl_read_and_clear_interrupt(void)
{
	struct ddl_context *ddl_context;

	ddl_context = ddl_get_context();
	if (!ddl_context->core_virtual_base_addr) {
		VIDC_LOGERR_STRING("SPURIOUS_INTERRUPT");
		return;
	}
	vidc_720p_get_interrupt_status(&ddl_context->intr_status,
		&ddl_context->cmd_err_status,
		&ddl_context->disp_pic_err_status,
		&ddl_context->op_failed
	);

	vidc_720p_interrupt_done_clear();

}

u32 ddl_process_core_response(void)
{
	struct ddl_context *ddl_context;
	u32 return_status = true;

	ddl_context = ddl_get_context();
	if (!ddl_context->core_virtual_base_addr) {
		VIDC_LOGERR_STRING("UNKWN_INTR");
		return false;
	}

	if (!ddl_handle_core_errors(ddl_context)) {
		return_status = ddl_process_intr_status(ddl_context,
			ddl_context->intr_status);
	}

	if (ddl_context->interrupt_clr)
		(*ddl_context->interrupt_clr)();

	return return_status;
}

static void ddl_decoder_input_done_callback(
	struct	ddl_client_context *ddl, u32 frame_transact_end)
{
	struct vidc_720p_dec_disp_info *dec_disp_info =
		&(ddl->codec_data.decoder.dec_disp_info);
	struct vcd_frame_data *input_vcd_frm =
		&(ddl->input_frame.vcd_frm);
	ddl_get_frame(input_vcd_frm, dec_disp_info->
		input_frame);

	input_vcd_frm->interlaced = (dec_disp_info->
		input_is_interlace);

	input_vcd_frm->offset += dec_disp_info->input_bytes_consumed;
	input_vcd_frm->data_len -= dec_disp_info->input_bytes_consumed;

	ddl->input_frame.frm_trans_end = frame_transact_end;
	if (vidc_msg_timing)
		ddl_calc_core_proc_time(__func__, DEC_IP_TIME);
	ddl->ddl_context->ddl_callback(
		VCD_EVT_RESP_INPUT_DONE,
		VCD_S_SUCCESS,
		&ddl->input_frame,
		sizeof(struct ddl_frame_data_tag),
		(void *)ddl,
		ddl->ddl_context->client_data);
}

static u32 ddl_decoder_output_done_callback(
	struct ddl_client_context *ddl,
	u32 frame_transact_end)
{
	struct ddl_decoder_data *decoder = &(ddl->codec_data.decoder);
	struct vidc_720p_dec_disp_info *dec_disp_info =
		&(decoder->dec_disp_info);
	struct ddl_frame_data_tag *output_frame =
		&ddl->output_frame;
	struct vcd_frame_data *output_vcd_frm =
		&(output_frame->vcd_frm);
	u32 vcd_status;
	u32 free_luma_dpb = 0;

	output_vcd_frm->physical = (u8 *)dec_disp_info->y_addr;

	if (decoder->codec.codec == VCD_CODEC_MPEG4 ||
		decoder->codec.codec == VCD_CODEC_VC1 ||
		decoder->codec.codec == VCD_CODEC_VC1_RCV ||
		(decoder->codec.codec >= VCD_CODEC_DIVX_3 &&
		 decoder->codec.codec <= VCD_CODEC_XVID)){
		vidc_720p_decode_skip_frm_details(&free_luma_dpb);
		if (free_luma_dpb)
			output_vcd_frm->physical = (u8 *) free_luma_dpb;
	}


	vcd_status = ddl_decoder_dpb_transact(
			decoder,
			output_frame,
			DDL_DPB_OP_MARK_BUSY);

	if (vcd_status != VCD_S_SUCCESS) {
		VIDC_LOGERR_STRING("CorruptedOutputBufferAddress");
		ddl_hw_fatal_cb(ddl->ddl_context);
		return vcd_status;
	}

	output_vcd_frm->ip_frm_tag =  dec_disp_info->tag_top;
	if (dec_disp_info->crop_exists == 0x1) {
		output_vcd_frm->dec_op_prop.disp_frm.left =
			dec_disp_info->crop_left_offset;
		output_vcd_frm->dec_op_prop.disp_frm.top =
			dec_disp_info->crop_top_offset;
		output_vcd_frm->dec_op_prop.disp_frm.right =
			dec_disp_info->img_size_x -
			dec_disp_info->crop_right_offset;
		output_vcd_frm->dec_op_prop.disp_frm.bottom =
			dec_disp_info->img_size_y -
			dec_disp_info->crop_bottom_offset;
	} else {
		output_vcd_frm->dec_op_prop.disp_frm.left = 0;
		output_vcd_frm->dec_op_prop.disp_frm.top = 0;
		output_vcd_frm->dec_op_prop.disp_frm.right =
			dec_disp_info->img_size_x;
		output_vcd_frm->dec_op_prop.disp_frm.bottom =
			dec_disp_info->img_size_y;
	}
	if (!dec_disp_info->disp_is_interlace) {
		output_vcd_frm->interlaced = false;
		output_vcd_frm->intrlcd_ip_frm_tag = VCD_FRAMETAG_INVALID;
	} else {
		output_vcd_frm->interlaced = true;
		output_vcd_frm->intrlcd_ip_frm_tag =
			dec_disp_info->tag_bottom;
	}

	output_vcd_frm->offset = 0;
	output_vcd_frm->data_len = decoder->y_cb_cr_size;
	if (free_luma_dpb) {
		output_vcd_frm->data_len = 0;
		output_vcd_frm->flags |= VCD_FRAME_FLAG_DECODEONLY;
	}
	output_vcd_frm->flags |= VCD_FRAME_FLAG_ENDOFFRAME;
	ddl_process_decoder_metadata(ddl);
	output_frame->frm_trans_end = frame_transact_end;

	if (vidc_msg_timing)
		ddl_calc_core_proc_time(__func__, DEC_OP_TIME);

	ddl->ddl_context->ddl_callback(
		VCD_EVT_RESP_OUTPUT_DONE,
		vcd_status,
		output_frame,
		sizeof(struct ddl_frame_data_tag),
		(void *)ddl,
		ddl->ddl_context->client_data);
	return vcd_status;
}

static u32 ddl_get_frame
	(struct vcd_frame_data *frame, u32 frametype) {
	enum vidc_720p_frame vidc_frame =
		(enum vidc_720p_frame)frametype;
	u32 status = true;

	switch (vidc_frame) {
	case VIDC_720P_IFRAME:
		{
			frame->flags |= VCD_FRAME_FLAG_SYNCFRAME;
			frame->frame = VCD_FRAME_I;
			break;
		}
	case VIDC_720P_PFRAME:
		{
			frame->frame = VCD_FRAME_P;
			break;
		}
	case VIDC_720P_BFRAME:
		{
			frame->frame = VCD_FRAME_B;
			break;
		}
	case VIDC_720P_NOTCODED:
		{
			frame->frame = VCD_FRAME_NOTCODED;
			frame->data_len = 0;
			break;
		}
	default:
		{
			VIDC_LOG_STRING("CRITICAL-FRAMETYPE");
			status = false;
			break;
		}
	}
	return status;
}

static void ddl_getmpeg4_declevel(enum vcd_codec_level *codec_level,
	u32 level)
{
	switch (level) {
	case VIDC_720P_MPEG4_LEVEL0:
		{
			*codec_level = VCD_LEVEL_MPEG4_0;
			break;
		}
	case VIDC_720P_MPEG4_LEVEL0b:
		{
			*codec_level = VCD_LEVEL_MPEG4_0b;
			break;
		}
	case VIDC_720P_MPEG4_LEVEL1:
		{
			*codec_level = VCD_LEVEL_MPEG4_1;
			break;
		}
	case VIDC_720P_MPEG4_LEVEL2:
		{
			*codec_level = VCD_LEVEL_MPEG4_2;
			break;
		}
	case VIDC_720P_MPEG4_LEVEL3:
		{
			*codec_level = VCD_LEVEL_MPEG4_3;
			break;
		}
	case VIDC_720P_MPEG4_LEVEL3b:
		{
			*codec_level = VCD_LEVEL_MPEG4_3b;
			break;
		}
	case VIDC_720P_MPEG4_LEVEL4a:
		{
			*codec_level = VCD_LEVEL_MPEG4_4a;
			break;
		}
	case VIDC_720P_MPEG4_LEVEL5:
		{
			*codec_level = VCD_LEVEL_MPEG4_5;
			break;
		}
	case VIDC_720P_MPEG4_LEVEL6:
		{
			*codec_level = VCD_LEVEL_MPEG4_6;
			break;
		}
	}
}

static void ddl_geth264_declevel(enum vcd_codec_level *codec_level,
	u32 level)
{
	switch (level) {
	case VIDC_720P_H264_LEVEL1:
		{
			*codec_level = VCD_LEVEL_H264_1;
			break;
		}
	case VIDC_720P_H264_LEVEL1b:
		{
			*codec_level = VCD_LEVEL_H264_1b;
			break;
		}
	case VIDC_720P_H264_LEVEL1p1:
		{
			*codec_level = VCD_LEVEL_H264_1p1;
			break;
		}
	case VIDC_720P_H264_LEVEL1p2:
		{
			*codec_level = VCD_LEVEL_H264_1p2;
			break;
		}
	case VIDC_720P_H264_LEVEL1p3:
		{
			*codec_level = VCD_LEVEL_H264_1p3;
			break;
		}
	case VIDC_720P_H264_LEVEL2:
		{
			*codec_level = VCD_LEVEL_H264_2;
			break;
		}
	case VIDC_720P_H264_LEVEL2p1:
		{
			*codec_level = VCD_LEVEL_H264_2p1;
			break;
		}
	case VIDC_720P_H264_LEVEL2p2:
		{
			*codec_level = VCD_LEVEL_H264_2p2;
			break;
		}
	case VIDC_720P_H264_LEVEL3:
		{
			*codec_level = VCD_LEVEL_H264_3;
			break;
		}
	case VIDC_720P_H264_LEVEL3p1:
		{
			*codec_level = VCD_LEVEL_H264_3p1;
			break;
		}
	case VIDC_720P_H264_LEVEL3p2:
	{
		*codec_level = VCD_LEVEL_H264_3p2;
		break;
	}

	}
}

static void ddl_get_vc1_dec_level(
	enum vcd_codec_level *codec_level, u32 level,
	enum vcd_codec_profile vc1_profile)
{
	if (vc1_profile == VCD_PROFILE_VC1_ADVANCE)	{
		switch (level) {
		case VIDC_720P_VC1_LEVEL0:
			{
				*codec_level = VCD_LEVEL_VC1_A_0;
				break;
			}
		case VIDC_720P_VC1_LEVEL1:
			{
				*codec_level = VCD_LEVEL_VC1_A_1;
				break;
			}
		case VIDC_720P_VC1_LEVEL2:
			{
				*codec_level = VCD_LEVEL_VC1_A_2;
				break;
			}
		case VIDC_720P_VC1_LEVEL3:
			{
				*codec_level = VCD_LEVEL_VC1_A_3;
				break;
			}
		case VIDC_720P_VC1_LEVEL4:
			{
				*codec_level = VCD_LEVEL_VC1_A_4;
				break;
			}
		}
		return;
	} else if (vc1_profile == VCD_PROFILE_VC1_MAIN) {
		switch (level) {
		case VIDC_720P_VC1_LEVEL_LOW:
			{
				*codec_level = VCD_LEVEL_VC1_M_LOW;
				break;
			}
		case VIDC_720P_VC1_LEVEL_MED:
			{
				*codec_level = VCD_LEVEL_VC1_M_MEDIUM;
				break;
			}
		case VIDC_720P_VC1_LEVEL_HIGH:
			{
				*codec_level = VCD_LEVEL_VC1_M_HIGH;
				break;
			}
		}
	} else if (vc1_profile == VCD_PROFILE_VC1_SIMPLE) {
		switch (level) {
		case VIDC_720P_VC1_LEVEL_LOW:
			{
				*codec_level = VCD_LEVEL_VC1_S_LOW;
				break;
			}
		case VIDC_720P_VC1_LEVEL_MED:
			{
				*codec_level = VCD_LEVEL_VC1_S_MEDIUM;
				break;
			}
		}
	}
}

static void ddl_get_mpeg2_dec_level(enum vcd_codec_level *codec_level,
								 u32 level)
{
	switch (level) {
	case VIDCL_720P_MPEG2_LEVEL_LOW:
		{
			*codec_level = VCD_LEVEL_MPEG2_LOW;
			break;
		}
	case VIDCL_720P_MPEG2_LEVEL_MAIN:
		{
			*codec_level = VCD_LEVEL_MPEG2_MAIN;
			break;
		}
	case VIDCL_720P_MPEG2_LEVEL_HIGH14:
		{
			*codec_level = VCD_LEVEL_MPEG2_HIGH_14;
			break;
		}
	}
}

static void ddl_getdec_profilelevel(struct ddl_decoder_data *decoder,
		u32 profile, u32 level)
{
	enum vcd_codec_profile codec_profile = VCD_PROFILE_UNKNOWN;
	enum vcd_codec_level codec_level = VCD_LEVEL_UNKNOWN;

	switch (decoder->codec.codec) {
	case VCD_CODEC_MPEG4:
		{
			if (profile == VIDC_720P_PROFILE_MPEG4_SP)
				codec_profile = VCD_PROFILE_MPEG4_SP;
			else if (profile == VIDC_720P_PROFILE_MPEG4_ASP)
				codec_profile = VCD_PROFILE_MPEG4_ASP;

			ddl_getmpeg4_declevel(&codec_level, level);
			break;
		}
	case VCD_CODEC_H264:
		{
			if (profile == VIDC_720P_PROFILE_H264_BASELINE)
				codec_profile = VCD_PROFILE_H264_BASELINE;
			else if (profile == VIDC_720P_PROFILE_H264_MAIN)
				codec_profile = VCD_PROFILE_H264_MAIN;
			else if (profile == VIDC_720P_PROFILE_H264_HIGH)
				codec_profile = VCD_PROFILE_H264_HIGH;
			ddl_geth264_declevel(&codec_level, level);
			break;
		}
	default:
	case VCD_CODEC_H263:
		{
			break;
		}
	case VCD_CODEC_VC1:
	case VCD_CODEC_VC1_RCV:
		{
			if (profile == VIDC_720P_PROFILE_VC1_SP)
				codec_profile = VCD_PROFILE_VC1_SIMPLE;
			else if (profile == VIDC_720P_PROFILE_VC1_MAIN)
				codec_profile = VCD_PROFILE_VC1_MAIN;
			else if (profile == VIDC_720P_PROFILE_VC1_ADV)
				codec_profile = VCD_PROFILE_VC1_ADVANCE;
			ddl_get_vc1_dec_level(&codec_level, level, profile);
			break;
		}
	case VCD_CODEC_MPEG2:
		{
			if (profile == VIDC_720P_PROFILE_MPEG2_MAIN)
				codec_profile = VCD_PROFILE_MPEG2_MAIN;
			else if (profile == VIDC_720P_PROFILE_MPEG2_SP)
				codec_profile = VCD_PROFILE_MPEG2_SIMPLE;
			ddl_get_mpeg2_dec_level(&codec_level, level);
			break;
		}
	}

	decoder->profile.profile = codec_profile;
	decoder->level.level = codec_level;
}
