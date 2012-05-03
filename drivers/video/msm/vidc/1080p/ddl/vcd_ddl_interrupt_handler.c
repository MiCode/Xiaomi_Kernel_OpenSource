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
#include "vcd_ddl_metadata.h"
#include "vcd_res_tracker_api.h"
#include <linux/delay.h>

static void ddl_decoder_input_done_callback(
	struct ddl_client_context *ddl, u32 frame_transact_end);
static u32 ddl_decoder_output_done_callback(
	struct ddl_client_context *ddl, u32 frame_transact_end);
static u32 ddl_get_decoded_frame(struct vcd_frame_data  *frame,
	enum vidc_1080p_decode_frame frame_type);
static u32 ddl_get_encoded_frame(struct vcd_frame_data *frame,
	enum vcd_codec codec,
	enum vidc_1080p_encode_frame frame_type);
static void ddl_get_dec_profile_level(struct ddl_decoder_data *decoder,
	u32 profile, u32 level);
static void ddl_handle_enc_frame_done(struct ddl_client_context *ddl,
	u32 eos_present);
static void ddl_handle_slice_done_slice_batch(struct ddl_client_context *ddl);
static void ddl_handle_enc_frame_done_slice_mode(
		struct ddl_client_context *ddl, u32 eos_present);

static void ddl_fw_status_done_callback(struct ddl_context *ddl_context)
{
	DDL_MSG_MED("ddl_fw_status_done_callback");
	if (!DDLCOMMAND_STATE_IS(ddl_context, DDL_CMD_DMA_INIT)) {
		DDL_MSG_ERROR("UNKWN_DMADONE");
	} else {
		DDL_MSG_LOW("FW_STATUS_DONE");
		vidc_1080p_set_host2risc_cmd(VIDC_1080P_HOST2RISC_CMD_SYS_INIT,
			ddl_context->fw_memory_size, 0, 0, 0);
	}
}

static void ddl_sys_init_done_callback(struct ddl_context *ddl_context,
	u32 fw_size)
{
	u32 vcd_status = VCD_S_SUCCESS;
	u8 *fw_ver;

	DDL_MSG_MED("ddl_sys_init_done_callback");
	if (!DDLCOMMAND_STATE_IS(ddl_context, DDL_CMD_DMA_INIT)) {
		DDL_MSG_ERROR("UNKNOWN_SYS_INIT_DONE");
	} else {
		ddl_context->cmd_state = DDL_CMD_INVALID;
		DDL_MSG_LOW("SYS_INIT_DONE");
		vidc_1080p_get_fw_version(&ddl_context->fw_version);
		fw_ver = (u8 *)&ddl_context->fw_version;
		DDL_MSG_ERROR("fw_version %x:%x:20%x",
			fw_ver[1]&0xFF, fw_ver[0]&0xFF, fw_ver[2]&0xFF);
		if (ddl_context->fw_memory_size >= fw_size) {
			ddl_context->device_state = DDL_DEVICE_INITED;
			vcd_status = VCD_S_SUCCESS;
		} else
			vcd_status = VCD_ERR_FAIL;
		ddl_context->ddl_callback(VCD_EVT_RESP_DEVICE_INIT,
			vcd_status, NULL, 0, NULL,
			ddl_context->client_data);
		DDL_IDLE(ddl_context);
	}
}

static void ddl_decoder_eos_done_callback(
	struct ddl_client_context *ddl)
{
	struct ddl_context *ddl_context = ddl->ddl_context;

	if (!ddl->decoding) {
		DDL_MSG_ERROR("STATE-CRITICAL-EOSDONE");
		ddl_client_fatal_cb(ddl);
	} else {
		ddl->client_state = DDL_CLIENT_WAIT_FOR_FRAME;
		DDL_MSG_LOW("EOS_DONE");
		ddl_context->ddl_callback(VCD_EVT_RESP_EOS_DONE,
			VCD_S_SUCCESS, NULL, 0, (u32 *)ddl,
			ddl->client_data);
		ddl_release_command_channel(ddl_context,
			ddl->command_channel);
	}
}

static u32 ddl_channel_set_callback(struct ddl_context *ddl_context,
	u32 instance_id)
{
	struct ddl_client_context *ddl;
	u32 ret = false;

	DDL_MSG_MED("ddl_channel_open_callback");
	ddl = ddl_get_current_ddl_client_for_command(ddl_context,
			DDL_CMD_CHANNEL_SET);
	if (ddl) {
		ddl->cmd_state = DDL_CMD_INVALID;
		if (!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_CHDONE)) {
			DDL_MSG_ERROR("STATE-CRITICAL-CHSET");
			ddl_release_command_channel(ddl_context,
			ddl->command_channel);
		} else {
			DDL_MSG_LOW("CH_SET_DONE");
			DDL_MSG_LOW("ddl_state_transition: %s ~~>"
				"DDL_CLIENT_WAIT_FOR_INITCODEC",
				ddl_get_state_string(ddl->client_state));
			ddl->client_state = DDL_CLIENT_WAIT_FOR_INITCODEC;
			ddl->instance_id = instance_id;
			if (ddl->decoding) {
				if (vidc_msg_timing)
					ddl_calc_core_proc_time(__func__,
						DEC_OP_TIME);
				if (ddl->codec_data.decoder.header_in_start)
					ddl_vidc_decode_init_codec(ddl);
				else {
					ddl_context->ddl_callback(
						VCD_EVT_RESP_START,
						VCD_S_SUCCESS, NULL, 0,
						(u32 *)ddl,
						ddl->client_data);
					ddl_release_command_channel(
						ddl_context,
						ddl->command_channel);
					ret = true;
				}
			} else
				ddl_vidc_encode_init_codec(ddl);
		}
	}
	return ret;
}

static u32 ddl_encoder_seq_done_callback(struct ddl_context *ddl_context,
	struct ddl_client_context *ddl)
{
	struct ddl_encoder_data *encoder;

	DDL_MSG_HIGH("ddl_encoder_seq_done_callback");
	if (!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_INITCODECDONE)) {
		DDL_MSG_ERROR("STATE-CRITICAL-INITCODEC");
		ddl_client_fatal_cb(ddl);
		return true;
	}
	if (vidc_msg_timing)
		ddl_calc_core_proc_time(__func__, ENC_OP_TIME);
	ddl->cmd_state = DDL_CMD_INVALID;
	DDL_MSG_LOW("ddl_state_transition: %s ~~> DDL_CLIENT_WAIT_FOR_FRAME",
	ddl_get_state_string(ddl->client_state));
	ddl->client_state = DDL_CLIENT_WAIT_FOR_FRAME;
	DDL_MSG_LOW("INIT_CODEC_DONE");
	encoder = &ddl->codec_data.encoder;
	vidc_1080p_get_encoder_sequence_header_size(
		&encoder->seq_header_length);
	if ((encoder->codec.codec == VCD_CODEC_H264) &&
		(encoder->profile.profile == VCD_PROFILE_H264_BASELINE))
		if ((encoder->seq_header.align_virtual_addr) &&
			(encoder->seq_header_length > 6))
			encoder->seq_header.align_virtual_addr[6] = 0xC0;
	ddl_context->ddl_callback(VCD_EVT_RESP_START, VCD_S_SUCCESS,
		NULL, 0, (u32 *) ddl, ddl->client_data);
	ddl_release_command_channel(ddl_context,
		ddl->command_channel);
	return true;
}

static void parse_hdr_size_data(struct ddl_client_context *ddl,
	struct vidc_1080p_seq_hdr_info *seq_hdr_info)
{
	u32 progressive;
	struct ddl_decoder_data *decoder = &ddl->codec_data.decoder;
	if (decoder->output_order == VCD_DEC_ORDER_DISPLAY) {
		decoder->frame_size.width = seq_hdr_info->img_size_x;
		decoder->frame_size.height = seq_hdr_info->img_size_y;
		progressive = seq_hdr_info->disp_progressive;
	} else {
		vidc_sm_get_dec_order_resl(
			&ddl->shared_mem[ddl->command_channel],
			&decoder->frame_size.width,
			&decoder->frame_size.height);
		progressive = seq_hdr_info->dec_progressive;
	}
	decoder->min_dpb_num = seq_hdr_info->min_num_dpb;
	vidc_sm_get_min_yc_dpb_sizes(
		&ddl->shared_mem[ddl->command_channel],
		&seq_hdr_info->min_luma_dpb_size,
		&seq_hdr_info->min_chroma_dpb_size);
	decoder->y_cb_cr_size = seq_hdr_info->min_luma_dpb_size +
		seq_hdr_info->min_chroma_dpb_size;
	decoder->dpb_buf_size.size_yuv = decoder->y_cb_cr_size;
	decoder->dpb_buf_size.size_y =
		seq_hdr_info->min_luma_dpb_size;
	decoder->dpb_buf_size.size_c =
		seq_hdr_info->min_chroma_dpb_size;
	decoder->progressive_only = progressive ? false : true;
}

static void parse_hdr_crop_data(struct ddl_client_context *ddl,
	struct vidc_1080p_seq_hdr_info *seq_hdr_info)
{
	struct ddl_decoder_data *decoder = &ddl->codec_data.decoder;
	u32 crop_exists = (decoder->output_order == VCD_DEC_ORDER_DISPLAY) ?
		seq_hdr_info->disp_crop_exists : seq_hdr_info->dec_crop_exists;
	if (crop_exists) {
		if (decoder->output_order ==
			VCD_DEC_ORDER_DISPLAY)
			vidc_sm_get_crop_info(
				&ddl->shared_mem[ddl->command_channel],
				&seq_hdr_info->crop_left_offset,
				&seq_hdr_info->crop_right_offset,
				&seq_hdr_info->crop_top_offset,
				&seq_hdr_info->crop_bottom_offset);
		else
			vidc_sm_get_dec_order_crop_info(
				&ddl->shared_mem[ddl->command_channel],
				&seq_hdr_info->crop_left_offset,
				&seq_hdr_info->crop_right_offset,
				&seq_hdr_info->crop_top_offset,
				&seq_hdr_info->crop_bottom_offset);
		decoder->frame_size.width -=
			seq_hdr_info->crop_right_offset +
			seq_hdr_info->crop_left_offset;
		decoder->frame_size.height -=
			seq_hdr_info->crop_top_offset +
			seq_hdr_info->crop_bottom_offset;
	}
}

static u32 ddl_decoder_seq_done_callback(struct ddl_context *ddl_context,
	struct ddl_client_context *ddl)
{
	struct ddl_decoder_data *decoder = &ddl->codec_data.decoder;
	struct vidc_1080p_seq_hdr_info seq_hdr_info;
	u32 process_further = true;
	struct ddl_profile_info_type disp_profile_info;

	DDL_MSG_MED("ddl_decoder_seq_done_callback");
	if (!ddl->decoding ||
		!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_INITCODECDONE)) {
		DDL_MSG_ERROR("STATE-CRITICAL-HDDONE");
		ddl_client_fatal_cb(ddl);
	} else {
		if (vidc_msg_timing)
			ddl_calc_core_proc_time(__func__, DEC_OP_TIME);
		ddl->cmd_state = DDL_CMD_INVALID;
		DDL_MSG_LOW("ddl_state_transition: %s ~~>"
			"DDL_CLIENT_WAIT_FOR_DPB",
			ddl_get_state_string(ddl->client_state));
		ddl->client_state = DDL_CLIENT_WAIT_FOR_DPB;
		DDL_MSG_LOW("HEADER_DONE");
		vidc_1080p_get_decode_seq_start_result(&seq_hdr_info);
		parse_hdr_size_data(ddl, &seq_hdr_info);
		if (res_trk_get_disable_fullhd() &&
			(seq_hdr_info.img_size_x * seq_hdr_info.img_size_y >
				1280 * 720)) {
			DDL_MSG_ERROR("FATAL:Resolution greater than 720P HD");
			ddl_client_fatal_cb(ddl);
			return process_further;
		}
		if (!seq_hdr_info.img_size_x || !seq_hdr_info.img_size_y) {
			DDL_MSG_ERROR("FATAL:ZeroImageSize");
			ddl_client_fatal_cb(ddl);
			return process_further;
		}
		vidc_sm_get_profile_info(&ddl->shared_mem
			[ddl->command_channel], &disp_profile_info);
		disp_profile_info.pic_profile = seq_hdr_info.profile;
		disp_profile_info.pic_level = seq_hdr_info.level;
		ddl_get_dec_profile_level(decoder, seq_hdr_info.profile,
			seq_hdr_info.level);
		switch (decoder->codec.codec) {
		case VCD_CODEC_H264:
			if (decoder->profile.profile == VCD_PROFILE_H264_HIGH ||
				decoder->profile.profile ==
				VCD_PROFILE_UNKNOWN) {
				if ((disp_profile_info.chroma_format_idc >
					VIDC_1080P_IDCFORMAT_420) ||
					(disp_profile_info.bit_depth_luma_minus8
					 || disp_profile_info.
					bit_depth_chroma_minus8)) {
					DDL_MSG_ERROR("Unsupported H.264 "
						"feature: IDC format : %d, Bitdepth: %d",
						disp_profile_info.
						chroma_format_idc,
						(disp_profile_info.
						 bit_depth_luma_minus8
						 ||	disp_profile_info.
					bit_depth_chroma_minus8));
					ddl_client_fatal_cb(ddl);
					return process_further;
				}
			}
			break;
		case VCD_CODEC_MPEG4:
		case VCD_CODEC_DIVX_4:
		case VCD_CODEC_DIVX_5:
		case VCD_CODEC_DIVX_6:
		case VCD_CODEC_XVID:
			if (seq_hdr_info.data_partition)
				if ((seq_hdr_info.img_size_x *
				seq_hdr_info.img_size_y) > (720 * 576)) {
					DDL_MSG_ERROR("Unsupported DP clip");
					ddl_client_fatal_cb(ddl);
					return process_further;
				}
			break;
		default:
			break;
		}
		ddl_calculate_stride(&decoder->frame_size,
			!decoder->progressive_only);
		decoder->frame_size.scan_lines =
		DDL_ALIGN(decoder->frame_size.height, DDL_TILE_ALIGN_HEIGHT);
		decoder->frame_size.stride =
		DDL_ALIGN(decoder->frame_size.width, DDL_TILE_ALIGN_WIDTH);
		parse_hdr_crop_data(ddl, &seq_hdr_info);
		if (decoder->codec.codec == VCD_CODEC_H264 &&
			seq_hdr_info.level > VIDC_1080P_H264_LEVEL4) {
			DDL_MSG_ERROR("WARNING: H264MaxLevelExceeded : %d",
				seq_hdr_info.level);
		}
		ddl_set_default_decoder_buffer_req(decoder, false);
		if (decoder->header_in_start) {
			if (!(decoder->cont_mode) ||
				(decoder->min_dpb_num >
				 decoder->client_output_buf_req.min_count) ||
				(decoder->actual_output_buf_req.sz >
				 decoder->client_output_buf_req.sz)) {
				decoder->client_frame_size =
					 decoder->frame_size;
				decoder->client_output_buf_req =
					decoder->actual_output_buf_req;
				decoder->client_input_buf_req =
					decoder->actual_input_buf_req;
			}
			ddl_context->ddl_callback(VCD_EVT_RESP_START,
				VCD_S_SUCCESS, NULL, 0, (u32 *) ddl,
				ddl->client_data);
			ddl_release_command_channel(ddl_context,
				ddl->command_channel);
		} else {
			u32 seq_hdr_only_frame = false;
			u32 need_reconfig = false;
			struct vcd_frame_data *input_vcd_frm =
				&ddl->input_frame.vcd_frm;
			need_reconfig = ddl_check_reconfig(ddl);
			DDL_MSG_HIGH("%s : need_reconfig = %u\n", __func__,
				 need_reconfig);
			if (input_vcd_frm->flags &
				  VCD_FRAME_FLAG_EOS) {
				need_reconfig = false;
			}
			if (((input_vcd_frm->flags &
				VCD_FRAME_FLAG_CODECCONFIG) &&
				(!(input_vcd_frm->flags &
				VCD_FRAME_FLAG_SYNCFRAME))) ||
				input_vcd_frm->data_len <=
				seq_hdr_info.dec_frm_size) {
				seq_hdr_only_frame = true;
				input_vcd_frm->offset +=
					seq_hdr_info.dec_frm_size;
				input_vcd_frm->data_len = 0;
				input_vcd_frm->flags |=
					VCD_FRAME_FLAG_CODECCONFIG;
				ddl->input_frame.frm_trans_end =
					!need_reconfig;
				ddl_context->ddl_callback(
					VCD_EVT_RESP_INPUT_DONE,
					VCD_S_SUCCESS, &ddl->input_frame,
					sizeof(struct ddl_frame_data_tag),
					(u32 *) ddl, ddl->client_data);
			} else {
				if (decoder->codec.codec ==
					VCD_CODEC_VC1_RCV) {
					vidc_sm_set_start_byte_number(
						&ddl->shared_mem
						[ddl->command_channel],
						seq_hdr_info.dec_frm_size);
				}
			}
			if (need_reconfig) {
				struct ddl_frame_data_tag *payload =
					&ddl->input_frame;
				u32 payload_size =
					sizeof(struct ddl_frame_data_tag);
				decoder->client_frame_size =
					decoder->frame_size;
				decoder->client_output_buf_req =
					decoder->actual_output_buf_req;
				decoder->client_input_buf_req =
					decoder->actual_input_buf_req;
				if (seq_hdr_only_frame) {
					payload = NULL;
					payload_size = 0;
				}
				DDL_MSG_HIGH("%s : sending port reconfig\n",
					 __func__);
				ddl_context->ddl_callback(
					VCD_EVT_IND_OUTPUT_RECONFIG,
					VCD_S_SUCCESS, payload,
					payload_size, (u32 *) ddl,
					ddl->client_data);
			}
			if (!need_reconfig && !seq_hdr_only_frame) {
				if (!ddl_vidc_decode_set_buffers(ddl))
					process_further = false;
				else {
					DDL_MSG_ERROR("ddl_vidc_decode_set_"
						"buffers failed");
					ddl_client_fatal_cb(ddl);
				}
			} else
				ddl_release_command_channel(ddl_context,
					ddl->command_channel);
		}
	}
	return process_further;
}

static u32 ddl_sequence_done_callback(struct ddl_context *ddl_context)
{
	struct ddl_client_context *ddl;
	u32 channel_inst_id, ret;

	vidc_1080p_get_returned_channel_inst_id(&channel_inst_id);
	vidc_1080p_clear_returned_channel_inst_id();
	ddl = ddl_get_current_ddl_client_for_channel_id(ddl_context,
			ddl_context->response_cmd_ch_id);
	if (!ddl) {
		DDL_MSG_ERROR("UNKWN_SEQ_DONE");
		ret = true;
	} else {
		if (ddl->decoding)
			ret = ddl_decoder_seq_done_callback(ddl_context,
					ddl);
		else
			ret = ddl_encoder_seq_done_callback(ddl_context,
					ddl);
	}
	return ret;
}

static u32 ddl_dpb_buffers_set_done_callback(
	struct ddl_context *ddl_context)
{
	struct ddl_client_context *ddl;
	u32 channel_inst_id, ret_status = true;

	DDL_MSG_MED("ddl_dpb_buffers_set_done_callback");
	vidc_1080p_get_returned_channel_inst_id(&channel_inst_id);
	vidc_1080p_clear_returned_channel_inst_id();
	ddl = ddl_get_current_ddl_client_for_command(ddl_context,
			DDL_CMD_DECODE_SET_DPB);
	if (ddl) {
		ddl->cmd_state = DDL_CMD_INVALID;
		if (!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_DPBDONE)) {
			DDL_MSG_ERROR("STATE-CRITICAL-DPBDONE");
			ddl_client_fatal_cb(ddl);
		} else {
			DDL_MSG_LOW("INTR_DPBDONE");
			DDL_MSG_LOW("ddl_state_transition: %s ~~>"
				"DDL_CLIENT_WAIT_FOR_FRAME",
				ddl_get_state_string(ddl->client_state));
			if (vidc_msg_timing) {
				ddl_calc_core_proc_time(__func__, DEC_OP_TIME);
				ddl_reset_core_time_variables(DEC_OP_TIME);
			}
			ddl->client_state = DDL_CLIENT_WAIT_FOR_FRAME;
			ddl_vidc_decode_frame_run(ddl);
			ret_status = false;
		}
	}
	return ret_status;
}

static void ddl_encoder_frame_run_callback(
	struct ddl_client_context *ddl)
{
	struct ddl_context *ddl_context = ddl->ddl_context;
	struct ddl_encoder_data *encoder =
		&(ddl->codec_data.encoder);
	struct vcd_frame_data *output_frame =
		&(ddl->output_frame.vcd_frm);
	u32 eos_present = false;

	DDL_MSG_MED("ddl_encoder_frame_run_callback\n");
	if (!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_FRAME_DONE) &&
		!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_EOS_DONE)) {
		DDL_MSG_ERROR("STATE-CRITICAL-ENCFRMRUN");
		ddl_client_fatal_cb(ddl);
	} else {
		if (vidc_msg_timing)
			ddl_calc_core_proc_time(__func__, ENC_OP_TIME);
		DDL_MSG_LOW("ENC_FRM_RUN_DONE");
		ddl->cmd_state = DDL_CMD_INVALID;
		vidc_1080p_get_encode_frame_info(&encoder->enc_frame_info);

		if (encoder->meta_data_enable_flag)
			vidc_sm_get_metadata_status(&ddl->shared_mem
				[ddl->command_channel],
				&encoder->enc_frame_info.meta_data_exists);

		if (VCD_FRAME_FLAG_EOS & ddl->input_frame.vcd_frm.flags) {
				DDL_MSG_LOW("%s EOS detected\n", __func__);
				eos_present = true;
		}

		if (encoder->enc_frame_info.enc_frame_size ||
			(encoder->enc_frame_info.enc_frame ==
			VIDC_1080P_ENCODE_FRAMETYPE_SKIPPED) ||
			DDLCLIENT_STATE_IS(ddl,
			DDL_CLIENT_WAIT_FOR_EOS_DONE)) {
			if (encoder->slice_delivery_info.enable) {
				ddl_handle_enc_frame_done_slice_mode(ddl,
								eos_present);
			} else {
				ddl_handle_enc_frame_done(ddl, eos_present);
			}
			if (DDLCLIENT_STATE_IS(ddl,
				DDL_CLIENT_WAIT_FOR_EOS_DONE) &&
				encoder->i_period.b_frames) {
				if ((ddl->extra_output_buf_count < 0) ||
					(ddl->extra_output_buf_count >
					encoder->i_period.b_frames)) {
					DDL_MSG_ERROR("Invalid B frame output"
								  "buffer index");
				} else {
					struct vidc_1080p_enc_frame_start_param
						enc_param;
					ddl->output_frame = ddl->\
					extra_output_frame[ddl->\
					extra_output_buf_count];
					ddl->\
					extra_output_buf_count--;
					output_frame =
					&ddl->output_frame.\
					vcd_frm;
					memset(&enc_param, 0,
						sizeof(enc_param));
					enc_param.cmd_seq_num =
						++ddl_context->cmd_seq_num;
					enc_param.inst_id = ddl->instance_id;
					enc_param.shared_mem_addr_offset =
					   DDL_ADDR_OFFSET(ddl_context->\
						dram_base_a, ddl->shared_mem
						[ddl->command_channel]);
					enc_param.stream_buffer_addr_offset =
						DDL_OFFSET(ddl_context->\
						dram_base_a.\
						align_physical_addr,
						output_frame->physical);
					enc_param.stream_buffer_size =
					encoder->client_output_buf_req.sz;
					enc_param.encode =
					VIDC_1080P_ENC_TYPE_LAST_FRAME_DATA;
					ddl->cmd_state = DDL_CMD_ENCODE_FRAME;
					ddl_context->vidc_encode_frame_start
						[ddl->command_channel]
						(&enc_param);
					}
				} else if (eos_present &&
					encoder->slice_delivery_info.enable) {
					ddl_vidc_encode_eos_run(ddl);
				} else {
				DDL_MSG_LOW("ddl_state_transition: %s ~~>"
					"DDL_CLIENT_WAIT_FOR_FRAME",
					ddl_get_state_string(
					ddl->client_state));
				ddl->client_state =
					DDL_CLIENT_WAIT_FOR_FRAME;
				ddl_release_command_channel(ddl_context,
				ddl->command_channel);
			}
		} else {
			ddl_context->ddl_callback(
				VCD_EVT_RESP_TRANSACTION_PENDING,
				VCD_S_SUCCESS, NULL, 0, (u32 *)ddl,
				ddl->client_data);
			DDL_MSG_LOW("ddl_state_transition: %s ~~>"
				"DDL_CLIENT_WAIT_FOR_FRAME",
			ddl_get_state_string(ddl->client_state));
			ddl->client_state = DDL_CLIENT_WAIT_FOR_FRAME;
			ddl_release_command_channel(ddl_context,
			ddl->command_channel);
		}
	}
}

static void get_dec_status(struct ddl_client_context *ddl,
	 struct vidc_1080p_dec_disp_info *dec_disp_info,
	 u32 output_order, u32 *status, u32 *rsl_chg)
{
	if (output_order == VCD_DEC_ORDER_DISPLAY) {
		vidc_1080p_get_display_frame_result(dec_disp_info);
		*status = dec_disp_info->display_status;
		*rsl_chg = dec_disp_info->disp_resl_change;
	} else {
		vidc_1080p_get_decode_frame_result(dec_disp_info);
		vidc_sm_get_dec_order_resl(
			&ddl->shared_mem[ddl->command_channel],
			&dec_disp_info->img_size_x,
			&dec_disp_info->img_size_y);
		*status = dec_disp_info->decode_status;
		*rsl_chg = dec_disp_info->dec_resl_change;
	}
}

static u32 ddl_decoder_frame_run_callback(struct ddl_client_context *ddl)
{
	struct ddl_context *ddl_context = ddl->ddl_context;
	struct ddl_decoder_data *decoder = &ddl->codec_data.decoder;
	u32 callback_end = false, ret_status = false;
	u32 eos_present = false, rsl_chg;
	u32 more_field_needed, extended_rsl_chg;
	enum vidc_1080p_display_status disp_status;
	DDL_MSG_MED("ddl_decoder_frame_run_callback");
	if (!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_FRAME_DONE)) {
		DDL_MSG_ERROR("STATE-CRITICAL-DECFRMRUN");
		ddl_client_fatal_cb(ddl);
		ret_status = true;
	} else {
		DDL_MSG_LOW("DEC_FRM_RUN_DONE");
		ddl->cmd_state = DDL_CMD_INVALID;
		get_dec_status(ddl, &ddl->codec_data.decoder.dec_disp_info,
			ddl->codec_data.decoder.output_order,
			&disp_status, &rsl_chg);

		vidc_sm_get_extended_decode_status(
			&ddl->shared_mem[ddl->command_channel],
			&more_field_needed,
			&extended_rsl_chg);
		decoder->field_needed_for_prev_ip =
			more_field_needed;
		decoder->prev_ip_frm_tag =
			ddl->input_frame.vcd_frm.ip_frm_tag;

		ddl_vidc_decode_dynamic_property(ddl, false);
		if (rsl_chg != DDL_RESL_CHANGE_NO_CHANGE) {
			ddl_handle_reconfig(rsl_chg, ddl);
			ret_status = false;
		} else {
			if ((VCD_FRAME_FLAG_EOS &
				ddl->input_frame.vcd_frm.flags)) {
				callback_end = false;
				eos_present = true;
			}
			if (disp_status ==
				VIDC_1080P_DISPLAY_STATUS_DECODE_ONLY ||
				disp_status ==
				VIDC_1080P_DISPLAY_STATUS_DECODE_AND_DISPLAY) {
				if (!eos_present)
					callback_end =
					(disp_status ==
					VIDC_1080P_DISPLAY_STATUS_DECODE_ONLY);
				ddl_decoder_input_done_callback(ddl,
					callback_end);
			}
			if (disp_status ==
				VIDC_1080P_DISPLAY_STATUS_DECODE_AND_DISPLAY ||
				disp_status ==
				VIDC_1080P_DISPLAY_STATUS_DISPLAY_ONLY) {
				if (!eos_present)
					callback_end = (disp_status ==
				VIDC_1080P_DISPLAY_STATUS_DECODE_AND_DISPLAY);
				if (ddl_decoder_output_done_callback(
					ddl, callback_end))
					ret_status = true;
			}
			if (!ret_status) {
				if (disp_status ==
					VIDC_1080P_DISPLAY_STATUS_DISPLAY_ONLY
					|| disp_status ==
					VIDC_1080P_DISPLAY_STATUS_DPB_EMPTY ||
					disp_status ==
					VIDC_1080P_DISPLAY_STATUS_NOOP) {
					ddl_vidc_decode_frame_run(ddl);
				} else if (eos_present)
					ddl_vidc_decode_eos_run(ddl);
				else {
					ddl->client_state =
						DDL_CLIENT_WAIT_FOR_FRAME;
					ddl_release_command_channel(ddl_context,
						ddl->command_channel);
					ret_status = true;
				}
			}
		}
	}
	return ret_status;
}

static u32 ddl_eos_frame_done_callback(
	struct ddl_client_context *ddl)
{
	struct ddl_context *ddl_context = ddl->ddl_context;
	struct ddl_decoder_data *decoder = &ddl->codec_data.decoder;
	struct ddl_mask *dpb_mask = &decoder->dpb_mask;
	u32 ret_status = true, rsl_chg, more_field_needed;
	enum vidc_1080p_display_status disp_status;

	if (!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_EOS_DONE)) {
		DDL_MSG_ERROR("STATE-CRITICAL-EOSFRMRUN");
		ddl_client_fatal_cb(ddl);
	} else {
		DDL_MSG_LOW("EOS_FRM_RUN_DONE");
		ddl->cmd_state = DDL_CMD_INVALID;
		get_dec_status(ddl, &ddl->codec_data.decoder.dec_disp_info,
			ddl->codec_data.decoder.output_order,
			&disp_status, &rsl_chg);
		vidc_sm_get_extended_decode_status(
			&ddl->shared_mem[ddl->command_channel],
			&more_field_needed, &rsl_chg);

		decoder->field_needed_for_prev_ip =
			more_field_needed;
		decoder->prev_ip_frm_tag =
			ddl->input_frame.vcd_frm.ip_frm_tag;
		ddl_vidc_decode_dynamic_property(ddl, false);
		if (disp_status ==
			VIDC_1080P_DISPLAY_STATUS_DPB_EMPTY) {
			if (rsl_chg) {
				decoder->header_in_start = false;
				decoder->decode_config.sequence_header =
					ddl->input_frame.vcd_frm.physical;
				decoder->decode_config.sequence_header_len =
					ddl->input_frame.vcd_frm.data_len;
				decoder->reconfig_detected = false;
				ddl_vidc_decode_init_codec(ddl);
				ret_status = false;
			} else
				ddl_decoder_eos_done_callback(ddl);
		} else {
			struct vidc_1080p_dec_frame_start_param dec_param;
			ret_status = false;
			if (disp_status ==
				VIDC_1080P_DISPLAY_STATUS_DISPLAY_ONLY) {
				if (ddl_decoder_output_done_callback(
					ddl, false))
					ret_status = true;
			} else if (disp_status !=
				VIDC_1080P_DISPLAY_STATUS_NOOP)
				DDL_MSG_ERROR("EOS-STATE-CRITICAL-"
					"WRONG-DISP-STATUS");
			if (!ret_status) {
				ddl_decoder_dpb_transact(decoder, NULL,
					DDL_DPB_OP_SET_MASK);
				ddl->cmd_state = DDL_CMD_EOS;

				memset(&dec_param, 0, sizeof(dec_param));

				dec_param.cmd_seq_num =
					++ddl_context->cmd_seq_num;
				dec_param.inst_id = ddl->instance_id;
				dec_param.shared_mem_addr_offset =
					DDL_ADDR_OFFSET(
					ddl_context->dram_base_a,
					ddl->shared_mem[ddl->command_channel]);
				dec_param.release_dpb_bit_mask =
					dpb_mask->hw_mask;
				dec_param.decode =
					(decoder->reconfig_detected) ?\
					VIDC_1080P_DEC_TYPE_FRAME_DATA :\
					VIDC_1080P_DEC_TYPE_LAST_FRAME_DATA;

				ddl_context->vidc_decode_frame_start[ddl->\
					command_channel](&dec_param);
			}
		}
	}
	return ret_status;
}

static u32 ddl_frame_run_callback(struct ddl_context *ddl_context)
{
	struct ddl_client_context *ddl;
	u32 channel_inst_id;
	u32 return_status = true;

	vidc_1080p_get_returned_channel_inst_id(&channel_inst_id);
	vidc_1080p_clear_returned_channel_inst_id();
	ddl = ddl_get_current_ddl_client_for_channel_id(ddl_context,
			ddl_context->response_cmd_ch_id);
	if (ddl) {
		if (ddl_context->pix_cache_enable) {
			struct vidc_1080P_pix_cache_statistics
			pixel_cache_stats;
			vidc_pix_cache_get_statistics(&pixel_cache_stats);

			DDL_MSG_HIGH(" pixel cache hits = %d,"
				"miss = %d", pixel_cache_stats.access_hit,
				pixel_cache_stats.access_miss);
			DDL_MSG_HIGH(" pixel cache core reqs = %d,"
				"axi reqs = %d", pixel_cache_stats.core_req,
				pixel_cache_stats.axi_req);
			DDL_MSG_HIGH(" pixel cache core bus stats = %d,"
			"axi bus stats = %d", pixel_cache_stats.core_bus,
				pixel_cache_stats.axi_bus);
		}

		if (ddl->cmd_state == DDL_CMD_DECODE_FRAME)
			return_status = ddl_decoder_frame_run_callback(ddl);
		else if (ddl->cmd_state == DDL_CMD_ENCODE_FRAME)
			ddl_encoder_frame_run_callback(ddl);
		else if (ddl->cmd_state == DDL_CMD_EOS)
			return_status = ddl_eos_frame_done_callback(ddl);
		else {
			DDL_MSG_ERROR("UNKWN_FRAME_DONE");
			return_status = false;
		}
	} else
		return_status = false;

	return return_status;
}

static void ddl_channel_end_callback(struct ddl_context *ddl_context)
{
	struct ddl_client_context *ddl;

	DDL_MSG_MED("ddl_channel_end_callback");
	ddl = ddl_get_current_ddl_client_for_command(ddl_context,
			DDL_CMD_CHANNEL_END);
	if (ddl) {
		ddl->cmd_state = DDL_CMD_INVALID;
		if (!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_CHEND)) {
			DDL_MSG_LOW("STATE-CRITICAL-CHEND");
		} else {
			DDL_MSG_LOW("CH_END_DONE");
			ddl_release_client_internal_buffers(ddl);
			ddl_context->ddl_callback(VCD_EVT_RESP_STOP,
				VCD_S_SUCCESS, NULL, 0, (u32 *)ddl,
				ddl->client_data);
			DDL_MSG_LOW("ddl_state_transition: %s ~~>"
				"DDL_CLIENT_OPEN",
				ddl_get_state_string(ddl->client_state));
			ddl->client_state = DDL_CLIENT_OPEN;
		}
		ddl_release_command_channel(ddl_context,
			ddl->command_channel);
	}
}

static void ddl_edfu_callback(struct ddl_context *ddl_context)
{
	struct ddl_client_context *ddl;
	u32 channel_inst_id;

	DDL_MSG_MED("ddl_edfu_callback");
	vidc_1080p_get_returned_channel_inst_id(&channel_inst_id);
	vidc_1080p_clear_returned_channel_inst_id();
	ddl = ddl_get_current_ddl_client_for_channel_id(ddl_context,
			ddl_context->response_cmd_ch_id);
	if (ddl) {
		if (ddl->cmd_state != DDL_CMD_ENCODE_FRAME)
			DDL_MSG_LOW("UNKWN_EDFU");
	}
}

static void ddl_encoder_eos_done(struct ddl_context *ddl_context)
{
	struct ddl_client_context *ddl;
	u32 channel_inst_id;

	vidc_1080p_get_returned_channel_inst_id(&channel_inst_id);
	vidc_1080p_clear_returned_channel_inst_id();
	ddl = ddl_get_current_ddl_client_for_channel_id(ddl_context,
			ddl_context->response_cmd_ch_id);
	DDL_MSG_LOW("ddl_encoder_eos_done\n");
	if (ddl == NULL) {
		DDL_MSG_ERROR("NO_DDL_CONTEXT");
	} else {
		if (!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_EOS_DONE)) {
			DDL_MSG_ERROR("STATE-CRITICAL-EOSFRMDONE");
			ddl_client_fatal_cb(ddl);
		} else {
			struct ddl_encoder_data *encoder =
				&(ddl->codec_data.encoder);
			vidc_1080p_get_encode_frame_info(
				&encoder->enc_frame_info);
			if (!encoder->slice_delivery_info.enable) {
				ddl_handle_enc_frame_done(ddl, true);
				ddl->cmd_state = DDL_CMD_INVALID;
			}
			DDL_MSG_LOW("encoder_eos_done");
			DDL_MSG_LOW("ddl_state_transition: %s ~~>"
				"DDL_CLIENT_WAIT_FOR_FRAME",
				ddl_get_state_string(ddl->client_state));
			ddl->client_state = DDL_CLIENT_WAIT_FOR_FRAME;
			DDL_MSG_LOW("eos_done");
			ddl_context->ddl_callback(VCD_EVT_RESP_EOS_DONE,
					VCD_S_SUCCESS, NULL, 0,
					(u32 *)ddl, ddl->client_data);
			ddl_release_command_channel(ddl_context,
				ddl->command_channel);
		}
	}
}

static u32 ddl_slice_done_callback(struct ddl_context *ddl_context)
{
	struct ddl_client_context *ddl;
	u32 channel_inst_id;
	u32 return_status = true;
	DDL_MSG_LOW("ddl_sliceDoneCallback");
	vidc_1080p_get_returned_channel_inst_id(&channel_inst_id);
	vidc_1080p_clear_returned_channel_inst_id();
	ddl = ddl_get_current_ddl_client_for_channel_id(ddl_context,
			ddl_context->response_cmd_ch_id);
	if (ddl == NULL) {
		DDL_MSG_ERROR("NO_DDL_CONTEXT");
		return_status = false;
	} else if (ddl->cmd_state == DDL_CMD_ENCODE_FRAME) {
			ddl->cmd_state = DDL_CMD_INVALID;
			if (DDLCLIENT_STATE_IS(ddl,
				DDL_CLIENT_WAIT_FOR_FRAME_DONE)) {
				ddl_handle_slice_done_slice_batch(ddl);
			} else {
				DDL_MSG_ERROR("STATE-CRITICAL-ENCFRMRUN : %s\n",
					 __func__);
				ddl_client_fatal_cb(ddl);
			}
	} else {
		DDL_MSG_ERROR("UNKNOWN_SLICEDONE : %s\n", __func__);
	}
	return return_status;
}

static u32 ddl_process_intr_status(struct ddl_context *ddl_context,
	u32 intr_status)
{
	u32 return_status = true;
	switch (intr_status) {
	case VIDC_1080P_RISC2HOST_CMD_OPEN_CH_RET:
		return_status = ddl_channel_set_callback(ddl_context,
			ddl_context->response_cmd_ch_id);
	break;
	case VIDC_1080P_RISC2HOST_CMD_CLOSE_CH_RET:
		ddl_channel_end_callback(ddl_context);
	break;
	case VIDC_1080P_RISC2HOST_CMD_SEQ_DONE_RET:
		return_status = ddl_sequence_done_callback(ddl_context);
	break;
	case VIDC_1080P_RISC2HOST_CMD_FRAME_DONE_RET:
		return_status = ddl_frame_run_callback(ddl_context);
	break;
	case VIDC_1080P_RISC2HOST_CMD_SLICE_DONE_RET:
		 ddl_slice_done_callback(ddl_context);
	break;
	case VIDC_1080P_RISC2HOST_CMD_SYS_INIT_RET:
		ddl_sys_init_done_callback(ddl_context,
			ddl_context->response_cmd_ch_id);
	break;
	case VIDC_1080P_RISC2HOST_CMD_FW_STATUS_RET:
		ddl_fw_status_done_callback(ddl_context);
	break;
	case VIDC_1080P_RISC2HOST_CMD_EDFU_INT_RET:
		ddl_edfu_callback(ddl_context);
	break;
	case VIDC_1080P_RISC2HOST_CMD_ENC_COMPLETE_RET:
		ddl_encoder_eos_done(ddl_context);
	break;
	case VIDC_1080P_RISC2HOST_CMD_ERROR_RET:
		DDL_MSG_ERROR("CMD_ERROR_INTR");
		return_status = ddl_handle_core_errors(ddl_context);
	break;
	case VIDC_1080P_RISC2HOST_CMD_INIT_BUFFERS_RET:
		return_status =
			ddl_dpb_buffers_set_done_callback(ddl_context);
	break;
	default:
		DDL_MSG_LOW("UNKWN_INTR");
	break;
	}
	return return_status;
}

void ddl_read_and_clear_interrupt(void)
{
	struct ddl_context *ddl_context;
	struct ddl_hw_interface  *ddl_hw_response;
	struct ddl_client_context *ddl;
	struct ddl_encoder_data *encoder;

	ddl_context = ddl_get_context();
	if (!ddl_context->core_virtual_base_addr) {
		DDL_MSG_LOW("SPURIOUS_INTERRUPT");
	} else {
		ddl_hw_response = &ddl_context->ddl_hw_response;
		vidc_1080p_get_risc2host_cmd(&ddl_hw_response->cmd,
			&ddl_hw_response->arg1, &ddl_hw_response->arg2,
			&ddl_hw_response->arg3,
			&ddl_hw_response->arg4);
		DDL_MSG_LOW("%s vidc_1080p_get_risc2host_cmd cmd = %u"
			"arg1 = %u arg2 = %u arg3 = %u"
			"arg4 = %u\n",
			__func__, ddl_hw_response->cmd,
			ddl_hw_response->arg1, ddl_hw_response->arg2,
			ddl_hw_response->arg3,
			ddl_hw_response->arg4);
		ddl = ddl_get_current_ddl_client_for_channel_id(ddl_context,
			ddl_context->response_cmd_ch_id);
		if (ddl) {
			encoder = &(ddl->codec_data.encoder);
			if (encoder && encoder->slice_delivery_info.enable
			&&
			((ddl_hw_response->cmd ==
				VIDC_1080P_RISC2HOST_CMD_SLICE_DONE_RET)
			|| (ddl_hw_response->cmd ==
				VIDC_1080P_RISC2HOST_CMD_FRAME_DONE_RET))) {
				vidc_sm_set_encoder_slice_batch_int_ctrl(
				&ddl->shared_mem[ddl->command_channel],
				1);
			}
		}
		vidc_1080p_clear_risc2host_cmd();
		vidc_1080p_clear_interrupt();
		vidc_1080p_get_risc2host_cmd_status(ddl_hw_response->arg2,
			&ddl_context->cmd_err_status,
			&ddl_context->disp_pic_err_status);
		DDL_MSG_LOW("%s cmd_err_status = %d\n", __func__,
				ddl_context->cmd_err_status);
		ddl_context->response_cmd_ch_id = ddl_hw_response->arg1;
	}
}

u32 ddl_process_core_response(void)
{
	struct ddl_context *ddl_context;
	struct ddl_hw_interface *ddl_hw_response;
	u32 status = false;

	ddl_context = ddl_get_context();
	if (!ddl_context->core_virtual_base_addr) {
		DDL_MSG_LOW("SPURIOUS_INTERRUPT");
		return status;
	}
	ddl_hw_response = &ddl_context->ddl_hw_response;
	status = ddl_process_intr_status(ddl_context, ddl_hw_response->cmd);
	if (ddl_context->interrupt_clr)
		(*ddl_context->interrupt_clr)();
	return status;
}

static void ddl_decoder_input_done_callback(
	struct ddl_client_context *ddl, u32 frame_transact_end)
{
	struct ddl_context *ddl_context = ddl->ddl_context;
	struct ddl_decoder_data *decoder = &(ddl->codec_data.decoder);
	struct vidc_1080p_dec_disp_info *dec_disp_info =
		&decoder->dec_disp_info;
	struct vcd_frame_data *input_vcd_frm = &(ddl->input_frame.vcd_frm);
	u32 is_interlaced;
	vidc_1080p_get_decoded_frame_size(
		&dec_disp_info->input_bytes_consumed);
	vidc_sm_set_start_byte_number(&ddl->shared_mem
		[ddl->command_channel], 0);
	vidc_1080p_get_decode_frame(&dec_disp_info->input_frame);
	ddl_get_decoded_frame(input_vcd_frm,
		dec_disp_info->input_frame);
	vidc_1080p_get_decode_frame_result(dec_disp_info);
	is_interlaced = (dec_disp_info->decode_coding ==
		VIDC_1080P_DISPLAY_CODING_INTERLACED);
	if (decoder->output_order == VCD_DEC_ORDER_DECODE) {
		dec_disp_info->tag_bottom = is_interlaced ?
			dec_disp_info->tag_top :
			VCD_FRAMETAG_INVALID;
		dec_disp_info->tag_top = input_vcd_frm->ip_frm_tag;
	}
	input_vcd_frm->interlaced = is_interlaced;
	input_vcd_frm->offset += dec_disp_info->input_bytes_consumed;
	input_vcd_frm->data_len -= dec_disp_info->input_bytes_consumed;
	ddl->input_frame.frm_trans_end = frame_transact_end;
	if (vidc_msg_timing)
		ddl_calc_core_proc_time(__func__, DEC_IP_TIME);
	ddl_context->ddl_callback(VCD_EVT_RESP_INPUT_DONE, VCD_S_SUCCESS,
		&ddl->input_frame, sizeof(struct ddl_frame_data_tag),
		(u32 *)ddl, ddl->client_data);
}

static void get_dec_op_done_data(struct vidc_1080p_dec_disp_info *dec_disp_info,
	u32 output_order, u8 **physical, u32 *is_interlaced)
{
	enum vidc_1080p_display_coding disp_coding;
	if (output_order == VCD_DEC_ORDER_DECODE) {
		*physical = (u8 *)(dec_disp_info->decode_y_addr << 11);
		disp_coding = dec_disp_info->decode_coding;
	} else {
		*physical = (u8 *)(dec_disp_info->display_y_addr << 11);
		disp_coding = dec_disp_info->display_coding;
	}
	*is_interlaced = (disp_coding ==
			VIDC_1080P_DISPLAY_CODING_INTERLACED);
}

static void get_dec_op_done_crop(u32 output_order,
	struct vidc_1080p_dec_disp_info *dec_disp_info,
	struct vcd_frame_rect *crop_data,
	struct vcd_property_frame_size *op_frame_sz,
	struct vcd_property_frame_size *frame_sz,
	struct ddl_buf_addr *shared_mem)
{
	u32 crop_exists =
		(output_order == VCD_DEC_ORDER_DECODE) ?
		dec_disp_info->dec_crop_exists :
		dec_disp_info->disp_crop_exists;
	crop_data->left = 0;
	crop_data->top = 0;
	crop_data->right = dec_disp_info->img_size_x;
	crop_data->bottom = dec_disp_info->img_size_y;
	op_frame_sz->width = dec_disp_info->img_size_x;
	op_frame_sz->height = dec_disp_info->img_size_y;
	ddl_calculate_stride(op_frame_sz, false);
	op_frame_sz->stride = DDL_ALIGN(op_frame_sz->width,
				DDL_TILE_ALIGN_WIDTH);
	op_frame_sz->scan_lines = DDL_ALIGN(op_frame_sz->height,
					DDL_TILE_ALIGN_HEIGHT);
	DDL_MSG_LOW("%s img_size_x = %u img_size_y = %u\n",
				__func__, dec_disp_info->img_size_x,
				dec_disp_info->img_size_y);
	if (crop_exists) {
		if (output_order == VCD_DEC_ORDER_DECODE)
			vidc_sm_get_dec_order_crop_info(shared_mem,
				&dec_disp_info->crop_left_offset,
				&dec_disp_info->crop_right_offset,
				&dec_disp_info->crop_top_offset,
				&dec_disp_info->crop_bottom_offset);
		else
			vidc_sm_get_crop_info(shared_mem,
				&dec_disp_info->crop_left_offset,
				&dec_disp_info->crop_right_offset,
				&dec_disp_info->crop_top_offset,
				&dec_disp_info->crop_bottom_offset);
		crop_data->left = dec_disp_info->crop_left_offset;
		crop_data->top = dec_disp_info->crop_top_offset;
		crop_data->right -= dec_disp_info->crop_right_offset;
		crop_data->bottom -= dec_disp_info->crop_bottom_offset;
		op_frame_sz->width = crop_data->right - crop_data->left;
		op_frame_sz->height = crop_data->bottom - crop_data->top;
	}
}

static u32 ddl_decoder_output_done_callback(
	struct ddl_client_context *ddl, u32 frame_transact_end)
{
	struct ddl_context *ddl_context = ddl->ddl_context;
	struct ddl_decoder_data *decoder = &(ddl->codec_data.decoder);
	struct vidc_1080p_dec_disp_info *dec_disp_info =
		&(decoder->dec_disp_info);
	struct ddl_frame_data_tag *output_frame = &(ddl->output_frame);
	struct vcd_frame_data *output_vcd_frm = &(output_frame->vcd_frm);
	u32 vcd_status, free_luma_dpb = 0, disp_pict = 0, is_interlaced;
	get_dec_op_done_data(dec_disp_info, decoder->output_order,
		&output_vcd_frm->physical, &is_interlaced);
	decoder->progressive_only = !(is_interlaced);
	output_vcd_frm->frame = VCD_FRAME_YUV;
	if (decoder->codec.codec == VCD_CODEC_MPEG4 ||
		decoder->codec.codec == VCD_CODEC_VC1 ||
		decoder->codec.codec == VCD_CODEC_VC1_RCV ||
		(decoder->codec.codec >= VCD_CODEC_DIVX_3 &&
		decoder->codec.codec <= VCD_CODEC_XVID)) {
		vidc_sm_get_displayed_picture_frame(&ddl->shared_mem
		[ddl->command_channel], &disp_pict);
		if (decoder->output_order == VCD_DEC_ORDER_DISPLAY) {
			if (!disp_pict) {
				output_vcd_frm->frame = VCD_FRAME_NOTCODED;
				vidc_sm_get_available_luma_dpb_address(
					&ddl->shared_mem[ddl->command_channel],
					&free_luma_dpb);
			}
		} else {
			if (dec_disp_info->input_frame ==
				VIDC_1080P_DECODE_FRAMETYPE_NOT_CODED) {
				output_vcd_frm->frame = VCD_FRAME_NOTCODED;
			vidc_sm_get_available_luma_dpb_dec_order_address(
					&ddl->shared_mem[ddl->command_channel],
					&free_luma_dpb);
			}
		}
		if (free_luma_dpb)
			output_vcd_frm->physical =
				(u8 *)(free_luma_dpb << 11);
	}
	vcd_status = ddl_decoder_dpb_transact(decoder, output_frame,
			DDL_DPB_OP_MARK_BUSY);
	if (vcd_status) {
		DDL_MSG_ERROR("CORRUPTED_OUTPUT_BUFFER_ADDRESS");
		ddl_hw_fatal_cb(ddl);
	} else {
		vidc_sm_get_metadata_status(&ddl->shared_mem
			[ddl->command_channel],
			&decoder->meta_data_exists);
		if (decoder->output_order == VCD_DEC_ORDER_DISPLAY) {
			vidc_sm_get_frame_tags(&ddl->shared_mem
				[ddl->command_channel],
				&dec_disp_info->tag_top,
				&dec_disp_info->tag_bottom);
			if (dec_disp_info->display_correct ==
				VIDC_1080P_DECODE_NOT_CORRECT ||
				dec_disp_info->display_correct ==
				VIDC_1080P_DECODE_APPROX_CORRECT)
				output_vcd_frm->flags |=
					VCD_FRAME_FLAG_DATACORRUPT;
		} else {
			if (dec_disp_info->decode_correct ==
				VIDC_1080P_DECODE_NOT_CORRECT ||
				dec_disp_info->decode_correct ==
				VIDC_1080P_DECODE_APPROX_CORRECT)
				output_vcd_frm->flags |=
					VCD_FRAME_FLAG_DATACORRUPT;
		}
		output_vcd_frm->ip_frm_tag = dec_disp_info->tag_top;
		vidc_sm_get_picture_times(&ddl->shared_mem
			[ddl->command_channel],
			&dec_disp_info->pic_time_top,
			&dec_disp_info->pic_time_bottom);
		get_dec_op_done_crop(decoder->output_order, dec_disp_info,
			&output_vcd_frm->dec_op_prop.disp_frm,
			&output_vcd_frm->dec_op_prop.frm_size,
			&decoder->frame_size,
			&ddl->shared_mem[ddl_context->response_cmd_ch_id]);
		if ((decoder->cont_mode) &&
			((output_vcd_frm->dec_op_prop.frm_size.width !=
			decoder->frame_size.width) ||
			(output_vcd_frm->dec_op_prop.frm_size.height !=
			decoder->frame_size.height) ||
			(decoder->frame_size.width !=
			decoder->client_frame_size.width) ||
			(decoder->frame_size.height !=
			decoder->client_frame_size.height))) {
			DDL_MSG_LOW("%s o/p width = %u o/p height = %u"
				"decoder width = %u decoder height = %u ",
				__func__,
				output_vcd_frm->dec_op_prop.frm_size.width,
				output_vcd_frm->dec_op_prop.frm_size.height,
				decoder->frame_size.width,
				decoder->frame_size.height);
			DDL_MSG_HIGH("%s Sending INFO_OP_RECONFIG event\n",
				 __func__);
			ddl_context->ddl_callback(
				VCD_EVT_IND_INFO_OUTPUT_RECONFIG,
				VCD_S_SUCCESS, NULL, 0,
				(u32 *)ddl,
				ddl->client_data);
			decoder->frame_size =
				 output_vcd_frm->dec_op_prop.frm_size;
			decoder->client_frame_size = decoder->frame_size;
			decoder->y_cb_cr_size =
				ddl_get_yuv_buffer_size(&decoder->frame_size,
					&decoder->buf_format,
					(!decoder->progressive_only),
					decoder->codec.codec, NULL);
			decoder->actual_output_buf_req.sz =
				decoder->y_cb_cr_size + decoder->suffix;
			decoder->min_output_buf_req =
				decoder->actual_output_buf_req;
			DDL_MSG_LOW("%s y_cb_cr_size = %u "
				"actual_output_buf_req.sz = %u"
				"min_output_buf_req.sz = %u\n",
				decoder->y_cb_cr_size,
				decoder->actual_output_buf_req.sz,
				decoder->min_output_buf_req.sz);
			vidc_sm_set_chroma_addr_change(
			&ddl->shared_mem[ddl->command_channel],
			false);
		}
		output_vcd_frm->interlaced = is_interlaced;
		output_vcd_frm->intrlcd_ip_frm_tag =
			(!is_interlaced || !dec_disp_info->tag_bottom) ?
			VCD_FRAMETAG_INVALID : dec_disp_info->tag_bottom;
		output_vcd_frm->offset = 0;
		output_vcd_frm->data_len = decoder->y_cb_cr_size;
		if (free_luma_dpb) {
			output_vcd_frm->data_len = 0;
			output_vcd_frm->flags |= VCD_FRAME_FLAG_DECODEONLY;
		}
		output_vcd_frm->flags |= VCD_FRAME_FLAG_ENDOFFRAME;
		output_frame->frm_trans_end = frame_transact_end;
		if (vidc_msg_timing)
			ddl_calc_core_proc_time(__func__, DEC_OP_TIME);
		ddl_process_decoder_metadata(ddl);
		vidc_sm_get_aspect_ratio_info(
			&ddl->shared_mem[ddl->command_channel],
			&output_vcd_frm->aspect_ratio_info);
		ddl_context->ddl_callback(VCD_EVT_RESP_OUTPUT_DONE,
			vcd_status, output_frame,
			sizeof(struct ddl_frame_data_tag),
			(u32 *)ddl, ddl->client_data);
	}
	return vcd_status;
}

static u32 ddl_get_decoded_frame(struct vcd_frame_data  *frame,
	enum vidc_1080p_decode_frame frame_type)
{
	u32 status = true;

	switch (frame_type) {
	case VIDC_1080P_DECODE_FRAMETYPE_I:
		frame->flags |= VCD_FRAME_FLAG_SYNCFRAME;
		frame->frame = VCD_FRAME_I;
	break;
	case VIDC_1080P_DECODE_FRAMETYPE_P:
		frame->frame = VCD_FRAME_P;
	break;
	case VIDC_1080P_DECODE_FRAMETYPE_B:
		frame->frame = VCD_FRAME_B;
	break;
	case VIDC_1080P_DECODE_FRAMETYPE_NOT_CODED:
		frame->frame = VCD_FRAME_NOTCODED;
		frame->data_len = 0;
		DDL_MSG_HIGH("DDL_INFO:Decoder:NotCodedFrame>");
	break;
	case VIDC_1080P_DECODE_FRAMETYPE_OTHERS:
		frame->frame = VCD_FRAME_YUV;
	break;
	case VIDC_1080P_DECODE_FRAMETYPE_32BIT:
	default:
		DDL_MSG_ERROR("UNKNOWN-FRAMETYPE");
		status = false;
	break;
	}
	return status;
}

static u32 ddl_get_encoded_frame(struct vcd_frame_data *frame,
	enum vcd_codec codec,
	enum vidc_1080p_encode_frame frame_type)
{
	u32 status = true;

	if (codec == VCD_CODEC_H264) {
		switch (frame_type) {
		case VIDC_1080P_ENCODE_FRAMETYPE_NOT_CODED:
			frame->frame = VCD_FRAME_P;
		break;
		case VIDC_1080P_ENCODE_FRAMETYPE_I:
			frame->flags |= VCD_FRAME_FLAG_SYNCFRAME;
			frame->frame = VCD_FRAME_I;
		break;
		case VIDC_1080P_ENCODE_FRAMETYPE_P:
			frame->frame = VCD_FRAME_P;
		break;
		case VIDC_1080P_ENCODE_FRAMETYPE_B:
			frame->frame = VCD_FRAME_B;
			frame->flags |= VCD_FRAME_FLAG_BFRAME;
		break;
		case VIDC_1080P_ENCODE_FRAMETYPE_SKIPPED:
			frame->frame = VCD_FRAME_NOTCODED;
			frame->data_len = 0;
		break;
		case VIDC_1080P_ENCODE_FRAMETYPE_OTHERS:
			DDL_MSG_LOW("FRAMETYPE-OTHERS");
		break;
		case VIDC_1080P_ENCODE_FRAMETYPE_32BIT:
		default:
			DDL_MSG_LOW("UNKNOWN-FRAMETYPE");
			status = false;
		break;
		}
	} else if (codec == VCD_CODEC_MPEG4) {
		switch (frame_type) {
		case VIDC_1080P_ENCODE_FRAMETYPE_NOT_CODED:
			frame->frame = VCD_FRAME_P;
		break;
		case VIDC_1080P_ENCODE_FRAMETYPE_I:
			frame->flags |= VCD_FRAME_FLAG_SYNCFRAME;
			frame->frame = VCD_FRAME_I;
		break;
		case VIDC_1080P_ENCODE_FRAMETYPE_P:
			frame->frame = VCD_FRAME_P;
		break;
		case VIDC_1080P_ENCODE_FRAMETYPE_B:
			frame->frame = VCD_FRAME_B;
			frame->flags |= VCD_FRAME_FLAG_BFRAME;
		break;
		case VIDC_1080P_ENCODE_FRAMETYPE_SKIPPED:
			frame->frame = VCD_FRAME_NOTCODED;
			frame->data_len = 0;
		break;
		case VIDC_1080P_ENCODE_FRAMETYPE_OTHERS:
			DDL_MSG_LOW("FRAMETYPE-OTHERS");
		break;
		case VIDC_1080P_ENCODE_FRAMETYPE_32BIT:
		default:
			DDL_MSG_LOW("UNKNOWN-FRAMETYPE");
			status = false;
		break;
		}
	} else if (codec == VCD_CODEC_H263) {
		switch (frame_type) {
		case VIDC_1080P_ENCODE_FRAMETYPE_NOT_CODED:
			frame->frame = VCD_FRAME_P;
		break;
		case VIDC_1080P_ENCODE_FRAMETYPE_I:
			frame->flags |= VCD_FRAME_FLAG_SYNCFRAME;
			frame->frame = VCD_FRAME_I;
		break;
		case VIDC_1080P_ENCODE_FRAMETYPE_P:
			frame->frame = VCD_FRAME_P;
		break;
		case VIDC_1080P_ENCODE_FRAMETYPE_SKIPPED:
			frame->frame = VCD_FRAME_NOTCODED;
			frame->data_len = 0;
		break;
		case VIDC_1080P_ENCODE_FRAMETYPE_OTHERS:
			DDL_MSG_LOW("FRAMETYPE-OTHERS");
		break;
		case VIDC_1080P_ENCODE_FRAMETYPE_32BIT:
		default:
			DDL_MSG_LOW("UNKNOWN-FRAMETYPE");
			status = false;
		break;
		}
	} else
		status = false;
	DDL_MSG_HIGH("Enc Frame Type %u", (u32)frame->frame);
	return status;
}

static void ddl_get_mpeg4_dec_level(enum vcd_codec_level *level,
	u32 level_codec, enum vcd_codec_profile mpeg4_profile)
{
	switch (level_codec) {
	case VIDC_1080P_MPEG4_LEVEL0:
		*level = VCD_LEVEL_MPEG4_0;
	break;
	case VIDC_1080P_MPEG4_LEVEL0b:
		*level = VCD_LEVEL_MPEG4_0b;
	break;
	case VIDC_1080P_MPEG4_LEVEL1:
		*level = VCD_LEVEL_MPEG4_1;
	break;
	case VIDC_1080P_MPEG4_LEVEL2:
		*level = VCD_LEVEL_MPEG4_2;
	break;
	case VIDC_1080P_MPEG4_LEVEL3:
		*level = VCD_LEVEL_MPEG4_3;
	break;
	case VIDC_1080P_MPEG4_LEVEL3b:
		if (mpeg4_profile == VCD_PROFILE_MPEG4_SP)
			*level = VCD_LEVEL_MPEG4_7;
		else
			*level = VCD_LEVEL_MPEG4_3b;
	break;
	case VIDC_1080P_MPEG4_LEVEL4a:
		*level = VCD_LEVEL_MPEG4_4a;
	break;
	case VIDC_1080P_MPEG4_LEVEL5:
		*level = VCD_LEVEL_MPEG4_5;
	break;
	case VIDC_1080P_MPEG4_LEVEL6:
		*level = VCD_LEVEL_MPEG4_6;
	break;
	default:
		*level = VCD_LEVEL_UNKNOWN;
	break;
	}
}

static void ddl_get_h264_dec_level(enum vcd_codec_level *level,
	u32 level_codec)
{
	switch (level_codec) {
	case VIDC_1080P_H264_LEVEL1:
		*level = VCD_LEVEL_H264_1;
	break;
	case VIDC_1080P_H264_LEVEL1b:
		*level = VCD_LEVEL_H264_1b;
	break;
	case VIDC_1080P_H264_LEVEL1p1:
		*level = VCD_LEVEL_H264_1p1;
	break;
	case VIDC_1080P_H264_LEVEL1p2:
		*level = VCD_LEVEL_H264_1p2;
	break;
	case VIDC_1080P_H264_LEVEL1p3:
		*level = VCD_LEVEL_H264_1p3;
	break;
	case VIDC_1080P_H264_LEVEL2:
		*level = VCD_LEVEL_H264_2;
	break;
	case VIDC_1080P_H264_LEVEL2p1:
		*level = VCD_LEVEL_H264_2p1;
	break;
	case VIDC_1080P_H264_LEVEL2p2:
		*level = VCD_LEVEL_H264_2p2;
	break;
	case VIDC_1080P_H264_LEVEL3:
		*level = VCD_LEVEL_H264_3;
	break;
	case VIDC_1080P_H264_LEVEL3p1:
		*level = VCD_LEVEL_H264_3p1;
	break;
	case VIDC_1080P_H264_LEVEL3p2:
		*level = VCD_LEVEL_H264_3p2;
	break;
	case VIDC_1080P_H264_LEVEL4:
		*level = VCD_LEVEL_H264_4;
	break;
	default:
		*level = VCD_LEVEL_UNKNOWN;
	break;
	}
}

static void ddl_get_h263_dec_level(enum vcd_codec_level *level,
	u32 level_codec)
{
	switch (level_codec) {
	case VIDC_1080P_H263_LEVEL10:
		*level = VCD_LEVEL_H263_10;
	break;
	case VIDC_1080P_H263_LEVEL20:
		*level = VCD_LEVEL_H263_20;
	break;
	case VIDC_1080P_H263_LEVEL30:
		*level = VCD_LEVEL_H263_30;
	break;
	case VIDC_1080P_H263_LEVEL40:
		*level = VCD_LEVEL_H263_40;
	break;
	case VIDC_1080P_H263_LEVEL45:
		*level = VCD_LEVEL_H263_45;
	break;
	case VIDC_1080P_H263_LEVEL50:
		*level = VCD_LEVEL_H263_50;
	break;
	case VIDC_1080P_H263_LEVEL60:
		*level = VCD_LEVEL_H263_60;
	break;
	case VIDC_1080P_H263_LEVEL70:
		*level = VCD_LEVEL_H263_70;
	break;
	default:
		*level = VCD_LEVEL_UNKNOWN;
	break;
	}
}

static void ddl_get_vc1_dec_level(enum vcd_codec_level *level,
	u32 level_codec, enum vcd_codec_profile vc1_profile)
{
	if (vc1_profile == VCD_PROFILE_VC1_ADVANCE) {
		switch (level_codec) {
		case VIDC_SM_LEVEL_VC1_ADV_0:
			*level = VCD_LEVEL_VC1_A_0;
		break;
		case VIDC_SM_LEVEL_VC1_ADV_1:
			*level = VCD_LEVEL_VC1_A_1;
		break;
		case VIDC_SM_LEVEL_VC1_ADV_2:
			*level = VCD_LEVEL_VC1_A_2;
		break;
		case VIDC_SM_LEVEL_VC1_ADV_3:
			*level = VCD_LEVEL_VC1_A_3;
		break;
		case VIDC_SM_LEVEL_VC1_ADV_4:
			*level = VCD_LEVEL_VC1_A_4;
		break;
		default:
			*level = VCD_LEVEL_UNKNOWN;
		break;
		}
	} else if (vc1_profile == VCD_PROFILE_VC1_MAIN) {
		switch (level_codec) {
		case VIDC_SM_LEVEL_VC1_LOW:
			*level = VCD_LEVEL_VC1_M_LOW;
		break;
		case VIDC_SM_LEVEL_VC1_MEDIUM:
			*level = VCD_LEVEL_VC1_M_MEDIUM;
		break;
		case VIDC_SM_LEVEL_VC1_HIGH:
			*level = VCD_LEVEL_VC1_M_HIGH;
		break;
		default:
			*level = VCD_LEVEL_UNKNOWN;
		break;
		}
	} else if (vc1_profile == VCD_PROFILE_VC1_SIMPLE) {
		switch (level_codec) {
		case VIDC_SM_LEVEL_VC1_LOW:
			*level = VCD_LEVEL_VC1_S_LOW;
		break;
		case VIDC_SM_LEVEL_VC1_MEDIUM:
			*level = VCD_LEVEL_VC1_S_MEDIUM;
		break;
		default:
			*level = VCD_LEVEL_UNKNOWN;
		break;
		}
	}
}

static void ddl_get_mpeg2_dec_level(enum vcd_codec_level *level,
	u32 level_codec)
{
	switch (level_codec) {
	case VIDC_SM_LEVEL_MPEG2_LOW:
		*level = VCD_LEVEL_MPEG2_LOW;
	break;
	case VIDC_SM_LEVEL_MPEG2_MAIN:
		*level = VCD_LEVEL_MPEG2_MAIN;
	break;
	case VIDC_SM_LEVEL_MPEG2_HIGH_1440:
		*level = VCD_LEVEL_MPEG2_HIGH_14;
	break;
	case VIDC_SM_LEVEL_MPEG2_HIGH:
		*level = VCD_LEVEL_MPEG2_HIGH;
	break;
	default:
		*level = VCD_LEVEL_UNKNOWN;
	break;
	}
}

static void ddl_get_dec_profile_level(struct ddl_decoder_data *decoder,
	u32 profile_codec, u32 level_codec)
{
	enum vcd_codec_profile profile = VCD_PROFILE_UNKNOWN;
	enum vcd_codec_level level = VCD_LEVEL_UNKNOWN;

	switch (decoder->codec.codec) {
	case VCD_CODEC_MPEG4:
	case VCD_CODEC_XVID:
		if (profile_codec == VIDC_SM_PROFILE_MPEG4_SIMPLE)
			profile = VCD_PROFILE_MPEG4_SP;
		else if (profile_codec == VIDC_SM_PROFILE_MPEG4_ADV_SIMPLE)
			profile = VCD_PROFILE_MPEG4_ASP;
		else
			profile = VCD_PROFILE_UNKNOWN;
		ddl_get_mpeg4_dec_level(&level, level_codec, profile);
	break;
	case VCD_CODEC_H264:
		if (profile_codec == VIDC_SM_PROFILE_H264_BASELINE)
			profile = VCD_PROFILE_H264_BASELINE;
		else if (profile_codec == VIDC_SM_PROFILE_H264_MAIN)
			profile = VCD_PROFILE_H264_MAIN;
		else if (profile_codec == VIDC_SM_PROFILE_H264_HIGH)
			profile = VCD_PROFILE_H264_HIGH;
		else
			profile = VCD_PROFILE_UNKNOWN;
		ddl_get_h264_dec_level(&level, level_codec);
	break;
	case VCD_CODEC_H263:
		if (profile_codec == VIDC_SM_PROFILE_H263_BASELINE)
			profile = VCD_PROFILE_H263_BASELINE;
		else
			profile = VCD_PROFILE_UNKNOWN;
		ddl_get_h263_dec_level(&level, level_codec);
	break;
	case VCD_CODEC_MPEG2:
		if (profile_codec == VIDC_SM_PROFILE_MPEG2_MAIN)
			profile = VCD_PROFILE_MPEG2_MAIN;
		else if (profile_codec == VIDC_SM_PROFILE_MPEG2_SIMPLE)
			profile = VCD_PROFILE_MPEG2_SIMPLE;
		else
			profile = VCD_PROFILE_UNKNOWN;
		ddl_get_mpeg2_dec_level(&level, level_codec);
	break;
	case VCD_CODEC_VC1:
	case VCD_CODEC_VC1_RCV:
		if (profile_codec == VIDC_SM_PROFILE_VC1_SIMPLE)
			profile = VCD_PROFILE_VC1_SIMPLE;
		else if (profile_codec == VIDC_SM_PROFILE_VC1_MAIN)
			profile = VCD_PROFILE_VC1_MAIN;
		else if (profile_codec == VIDC_SM_PROFILE_VC1_ADVANCED)
			profile = VCD_PROFILE_VC1_ADVANCE;
		else
			profile = VCD_PROFILE_UNKNOWN;
		ddl_get_vc1_dec_level(&level, level_codec, profile);
	break;
	default:
		if (!profile_codec)
			profile = VCD_PROFILE_UNKNOWN;
		if (!level)
			level = VCD_LEVEL_UNKNOWN;
	break;
	}
	decoder->profile.profile = profile;
	decoder->level.level = level;
}

static void ddl_handle_enc_frame_done(struct ddl_client_context *ddl,
					u32 eos_present)
{
	struct ddl_context       *ddl_context = ddl->ddl_context;
	struct ddl_encoder_data  *encoder = &(ddl->codec_data.encoder);
	struct vcd_frame_data    *output_frame = &(ddl->output_frame.vcd_frm);
	u32 bottom_frame_tag;
	u8 *input_buffer_address = NULL;

	vidc_sm_get_frame_tags(&ddl->shared_mem[ddl->command_channel],
		&output_frame->ip_frm_tag, &bottom_frame_tag);
	output_frame->data_len = encoder->enc_frame_info.enc_frame_size;
	output_frame->flags |= VCD_FRAME_FLAG_ENDOFFRAME;
	(void)ddl_get_encoded_frame(output_frame,
		encoder->codec.codec, encoder->enc_frame_info.enc_frame
								);
	ddl_process_encoder_metadata(ddl);
	ddl_vidc_encode_dynamic_property(ddl, false);
	ddl->input_frame.frm_trans_end = false;
	input_buffer_address = ddl_context->dram_base_a.align_physical_addr +
			encoder->enc_frame_info.enc_luma_address;
	ddl_get_input_frame_from_pool(ddl, input_buffer_address);
	ddl_context->ddl_callback(VCD_EVT_RESP_INPUT_DONE,
					VCD_S_SUCCESS, &(ddl->input_frame),
					sizeof(struct ddl_frame_data_tag),
					(u32 *) ddl, ddl->client_data);
	ddl->output_frame.frm_trans_end = eos_present ?
		false : true;
	ddl_context->ddl_callback(VCD_EVT_RESP_OUTPUT_DONE,
				VCD_S_SUCCESS, &(ddl->output_frame),
				sizeof(struct ddl_frame_data_tag),
				(u32 *) ddl, ddl->client_data);
}

static void ddl_handle_slice_done_slice_batch(struct ddl_client_context *ddl)
{
	struct ddl_context       *ddl_context = ddl->ddl_context;
	struct ddl_encoder_data  *encoder = &(ddl->codec_data.encoder);
	struct vcd_frame_data    *output_frame = NULL;
	u32 bottom_frame_tag;
	struct vidc_1080p_enc_slice_batch_out_param *slice_output = NULL;
	u32 num_slices_comp = 0;
	u32 index = 0;
	u32 start_bfr_idx = 0;
	u32 actual_idx = 0;

	DDL_MSG_LOW("%s\n", __func__);
	vidc_sm_get_num_slices_comp(
			&ddl->shared_mem[ddl->command_channel],
			&num_slices_comp);
	slice_output = (struct vidc_1080p_enc_slice_batch_out_param *)
		(encoder->batch_frame.slice_batch_out.align_virtual_addr);
	DDL_MSG_LOW(" after get no of slices = %d\n", num_slices_comp);
	if (slice_output == NULL)
		DDL_MSG_ERROR(" slice_output is NULL\n");
	encoder->slice_delivery_info.num_slices_enc += num_slices_comp;
	if (vidc_msg_timing) {
		ddl_calc_core_proc_time_cnt(__func__, ENC_SLICE_OP_TIME,
					num_slices_comp);
		ddl_set_core_start_time(__func__, ENC_SLICE_OP_TIME);
	}
	DDL_MSG_LOW("%s : Slices Completed %d Total slices %d OutBfrInfo:"
			"Cmd %d Size %d\n",
			__func__,
			num_slices_comp,
			encoder->slice_delivery_info.num_slices_enc,
			slice_output->cmd_type,
			slice_output->output_size);
	start_bfr_idx = encoder->batch_frame.out_frm_next_frmindex;
	for (index = 0; index < num_slices_comp; index++) {
		actual_idx =
		slice_output->slice_info[start_bfr_idx+index].stream_buffer_idx;
		DDL_MSG_LOW("Slice Info: OutBfrIndex %d SliceSize %d\n",
			actual_idx,
			slice_output->slice_info[start_bfr_idx+index].
			stream_buffer_size);
		output_frame = &(
			encoder->batch_frame.output_frame[actual_idx].vcd_frm);
		DDL_MSG_LOW("OutBfr: vcd_frm 0x%x frmbfr(virtual) 0x%x"
			"frmbfr(physical) 0x%x\n",
			&output_frame,
			output_frame.virtual_base_addr,
			output_frame.physical_base_addr);
		vidc_1080p_get_encode_frame_info(&encoder->enc_frame_info);
		vidc_sm_get_frame_tags(&ddl->shared_mem
			[ddl->command_channel],
			&output_frame->ip_frm_tag, &bottom_frame_tag);
		ddl_get_encoded_frame(output_frame,
				encoder->codec.codec,
				encoder->enc_frame_info.enc_frame);
		output_frame->data_len =
			slice_output->slice_info[actual_idx].stream_buffer_size;
		ddl->output_frame =
			encoder->batch_frame.output_frame[actual_idx];
		ddl->output_frame.frm_trans_end = false;
		ddl_context->ddl_callback(VCD_EVT_RESP_OUTPUT_DONE,
				VCD_S_SUCCESS, &(ddl->output_frame),
				sizeof(struct ddl_frame_data_tag),
				(u32 *) ddl, ddl->client_data);
		ddl->input_frame.frm_trans_end = false;
		DDL_MSG_LOW("%s Set i/p o/p transactions to false\n", __func__);
	}
	encoder->batch_frame.out_frm_next_frmindex = start_bfr_idx + index;
	ddl->cmd_state = DDL_CMD_ENCODE_FRAME;
	vidc_sm_set_encoder_slice_batch_int_ctrl(
			&ddl->shared_mem[ddl->command_channel],
			0);
}

static void ddl_handle_enc_frame_done_slice_mode(
		struct ddl_client_context *ddl, u32 eos_present)
{
	struct ddl_context       *ddl_context = ddl->ddl_context;
	struct ddl_encoder_data  *encoder = &(ddl->codec_data.encoder);
	struct vcd_frame_data    *output_frame = NULL;
	u32 bottom_frame_tag;
	u8 *input_buffer_address = NULL;
	struct vidc_1080p_enc_slice_batch_out_param *slice_output = NULL;
	u32 num_slices_comp = 0;
	u32 index = 0;
	u32 start_bfr_idx = 0;
	u32 actual_idx = 0;
	struct vcd_transc *transc;

	DDL_MSG_LOW("%s\n", __func__);
	vidc_sm_get_num_slices_comp(
				&ddl->shared_mem[ddl->command_channel],
				&num_slices_comp);
	slice_output = (struct vidc_1080p_enc_slice_batch_out_param *)
		encoder->batch_frame.slice_batch_out.align_virtual_addr;
	encoder->slice_delivery_info.num_slices_enc += num_slices_comp;
	if (vidc_msg_timing) {
		ddl_calc_core_proc_time_cnt(__func__, ENC_OP_TIME,
					num_slices_comp);
	}
	DDL_MSG_LOW("%s Slices Completed %d Total slices done = %d"
		" OutBfrInfo: Cmd %d Size %d",
		__func__,
		num_slices_comp,
		encoder->slice_delivery_info.num_slices_enc,
		slice_output->cmd_type,
		slice_output->output_size);
	start_bfr_idx = encoder->batch_frame.out_frm_next_frmindex;
	if ((encoder->slice_delivery_info.num_slices_enc %
		encoder->batch_frame.num_output_frames) != 0) {
		DDL_MSG_ERROR("ERROR : %d %d\n",
		encoder->slice_delivery_info.num_slices_enc,
		encoder->batch_frame.num_output_frames);
	}
	for (index = 0; index < num_slices_comp; index++) {
		actual_idx =
			slice_output->slice_info[start_bfr_idx+index]. \
			stream_buffer_idx;
		DDL_MSG_LOW("Slice Info: OutBfrIndex %d SliceSize %d",
			actual_idx,
			slice_output->slice_info[start_bfr_idx+index]. \
			stream_buffer_size, 0);
		output_frame =
		&(encoder->batch_frame.output_frame[actual_idx].vcd_frm);
		DDL_MSG_LOW("OutBfr: vcd_frm 0x%x frmbfr(virtual) 0x%x"
				"frmbfr(physical) 0x%x",
				&output_frame,
				output_frame.virtual_base_addr,
				output_frame.physical_base_addr);
		vidc_1080p_get_encode_frame_info(
			&encoder->enc_frame_info);
		vidc_sm_get_frame_tags(&ddl->shared_mem
			[ddl->command_channel],
			&output_frame->ip_frm_tag, &bottom_frame_tag);
		ddl_get_encoded_frame(output_frame,
				encoder->codec.codec,
				encoder->enc_frame_info.enc_frame);
		output_frame->data_len =
			slice_output->slice_info[actual_idx].stream_buffer_size;
		ddl->output_frame =
			encoder->batch_frame.output_frame[actual_idx];
		DDL_MSG_LOW(" %s actual_idx = %d"
		"encoder->batch_frame.num_output_frames = %d\n", __func__,
		actual_idx, encoder->batch_frame.num_output_frames);
		if (encoder->batch_frame.num_output_frames == (actual_idx+1)) {
			output_frame->flags |= VCD_FRAME_FLAG_ENDOFFRAME;
			ddl_vidc_encode_dynamic_property(ddl, false);
			ddl->input_frame.frm_trans_end = true;
			DDL_MSG_LOW("%s End of frame detected\n", __func__);
			input_buffer_address =
				ddl_context->dram_base_a.align_physical_addr +
				encoder->enc_frame_info.enc_luma_address;
			ddl_get_input_frame_from_pool(ddl,
				input_buffer_address);
			transc = (struct vcd_transc *)(ddl->client_data);
			if (eos_present)
				ddl->output_frame.frm_trans_end = false;
			else
				ddl->output_frame.frm_trans_end = true;
		}
		DDL_MSG_LOW("%s Before output done cb\n", __func__);
		transc = (struct vcd_transc *)(ddl->client_data);
		ddl->output_frame.vcd_frm = *output_frame;
		ddl_context->ddl_callback(VCD_EVT_RESP_OUTPUT_DONE,
				VCD_S_SUCCESS, &(ddl->output_frame),
				sizeof(struct ddl_frame_data_tag),
				(u32 *) ddl, ddl->client_data);
	}
	if (encoder->batch_frame.num_output_frames == (actual_idx+1)) {
		DDL_MSG_LOW("%s sending input done\n", __func__);
		ddl_context->ddl_callback(VCD_EVT_RESP_INPUT_DONE,
				VCD_S_SUCCESS, &(ddl->input_frame),
				sizeof(struct ddl_frame_data_tag),
				(u32 *) ddl, ddl->client_data);
	}
}
