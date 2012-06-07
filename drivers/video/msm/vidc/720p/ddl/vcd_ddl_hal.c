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

#include <media/msm/vidc_type.h>

#include "vcd_ddl_utils.h"
#include "vcd_ddl_metadata.h"

#if DEBUG
#define DBG(x...) printk(KERN_DEBUG x)
#else
#define DBG(x...)
#endif

#define DBG_INFO(x...) pr_info(x)

void ddl_core_init(struct ddl_context *ddl_context)
{
	char *psz_version;
	struct vcd_fw_details fw_details;
	u32 fw_endianness;
	enum vidc_720p_endian dma_endian;
	u32 interrupt_off;
	enum vidc_720p_interrupt_level_selection interrupt_sel;
	u32 intr_mask = 0x0;

	vcd_get_fw_property(VCD_FW_BOOTCODE, &fw_details);
	vcd_get_fw_property(VCD_FW_ENDIAN, &fw_endianness);
	if (fw_endianness == VCD_FW_BIG_ENDIAN)
		dma_endian = VIDC_720P_BIG_ENDIAN;
	else
		dma_endian = VIDC_720P_LITTLE_ENDIAN;

	interrupt_off = false;
	interrupt_sel = VIDC_720P_INTERRUPT_LEVEL_SEL;

	intr_mask |= VIDC_720P_INTR_BUFFER_FULL;
	intr_mask |= VIDC_720P_INTR_FW_DONE;
	intr_mask |= VIDC_720P_INTR_DMA_DONE;
	intr_mask |= VIDC_720P_INTR_FRAME_DONE;

	vidc_720p_do_sw_reset();

	DBG_INFO("Loading CONTROL_FW of FW_SIZE %u\n",
		fw_details.fw_size*4);

	vidc_720p_init(&psz_version,
			fw_details.fw_size,
			fw_details.fw_buffer_addr,
			dma_endian,
			interrupt_off, interrupt_sel, intr_mask);
	return;
}

void ddl_core_start_cpu(struct ddl_context *ddl_context)
{
	u32 fw_endianness;
	enum vidc_720p_endian dma_endian;
	u32 dbg_core_dump_buf_size = 0;

	vcd_get_fw_property(VCD_FW_ENDIAN, &fw_endianness);
	if (fw_endianness == VCD_FW_BIG_ENDIAN)
		dma_endian = VIDC_720P_LITTLE_ENDIAN;
	else
		dma_endian = VIDC_720P_BIG_ENDIAN;

	ddl_move_command_state(ddl_context, DDL_CMD_CPU_RESET);

	DBG("VSP_BUF_ADDR_SIZE %d",
		ddl_context->context_buf_addr.buffer_size);
	if (ddl_context->enable_dbg_core_dump) {
		dbg_core_dump_buf_size = ddl_context->dbg_core_dump.
			buffer_size;
	}

	vidc_720p_start_cpu(dma_endian,
		ddl_context->context_buf_addr.align_physical_addr,
		ddl_context->dbg_core_dump.align_physical_addr,
		dbg_core_dump_buf_size);

	VIDC_DEBUG_REGISTER_LOG;
}

void ddl_channel_set(struct ddl_client_context *ddl)
{
	enum vidc_720p_enc_dec_selection enc_dec_sel;
	enum vidc_720p_codec codec;
	enum vcd_codec *vcd_codec;
	u32 fw_property_id;
	struct vcd_fw_details fw_details;

	if (ddl->decoding) {
		if (vidc_msg_timing)
			ddl_set_core_start_time(__func__, DEC_OP_TIME);
		enc_dec_sel = VIDC_720P_DECODER;
		fw_property_id = VCD_FW_DECODE;
		vcd_codec = &(ddl->codec_data.decoder.codec.codec);
	} else {
		enc_dec_sel = VIDC_720P_ENCODER;
		fw_property_id = VCD_FW_ENCODE;
		vcd_codec = &(ddl->codec_data.encoder.codec.codec);
	}
	switch (*vcd_codec) {
	default:
	case VCD_CODEC_MPEG4:
		{
			codec = VIDC_720P_MPEG4;

		if (ddl->decoding) {
			vidc_720p_decode_set_mpeg4_data_partitionbuffer
				(ddl->ddl_context->data_partition_tempbuf.
				 align_physical_addr);
		}

			break;
		}
	case VCD_CODEC_H264:
		{
			codec = VIDC_720P_H264;
			break;
		}
	case VCD_CODEC_DIVX_4:
	case VCD_CODEC_DIVX_5:
	case VCD_CODEC_DIVX_6:
		{
			codec = VIDC_720P_DIVX;
			break;
		}
	case VCD_CODEC_XVID:
		{
			codec = VIDC_720P_XVID;
			break;
		}
	case VCD_CODEC_H263:
		{
			codec = VIDC_720P_H263;
			break;
		}
	case VCD_CODEC_MPEG2:
		{
			codec = VIDC_720P_MPEG2;
			break;
		}
	case VCD_CODEC_VC1:
	case VCD_CODEC_VC1_RCV:
		{
			codec = VIDC_720P_VC1;
			break;
		}
	}

	fw_details.codec = *vcd_codec;
	vcd_get_fw_property(fw_property_id, &fw_details);
	VIDC_DEBUG_REGISTER_LOG;

	ddl_move_command_state(ddl->ddl_context, DDL_CMD_CHANNEL_SET);
	ddl_move_client_state(ddl, DDL_CLIENT_WAIT_FOR_CHDONE);

	DBG_INFO("Loading firmware for CODEC:%u of FW_SIZE:%u\n",
		fw_details.codec, fw_details.fw_size*4);

	vidc_720p_set_channel(ddl->channel_id,
			       enc_dec_sel,
			       codec,
			       fw_details.fw_buffer_addr,
			       fw_details.fw_size);
}

void ddl_decode_init_codec(struct ddl_client_context *ddl)
{
	u32 seq_h = 0, seq_e = 0, start_byte_num = 0;
	struct ddl_decoder_data *decoder = &(ddl->codec_data.decoder);
	struct vcd_sequence_hdr *seq_hdr = &decoder->decode_config;
	enum vidc_720p_memory_access_method mem_access_method;
	if (vidc_msg_timing)
		ddl_set_core_start_time(__func__, DEC_OP_TIME);
	ddl_metadata_enable(ddl);

	vidc_720p_decode_set_error_control(true);

	vidc_720p_decode_set_mpeg4Post_filter(decoder->post_filter.
					       post_filter);

	if (decoder->codec.codec == VCD_CODEC_H264) {
		vidc_720p_decode_setH264VSPBuffer(decoder->
						   h264Vsp_temp_buffer.
						   align_physical_addr);
		VIDC_LOG1("VSP_BUF_ADDR_SIZE",
			   decoder->h264Vsp_temp_buffer.buffer_size);
	}

	if (decoder->codec.codec == VCD_CODEC_VC1_RCV ||
		decoder->codec.codec == VCD_CODEC_VC1) {
		vidc_720p_set_frame_size(decoder->client_frame_size.width,
			decoder->client_frame_size.height);
	} else {
		vidc_720p_set_frame_size(0x0, 0x0);
	}

	switch (decoder->buf_format.buffer_format) {
	default:
	case VCD_BUFFER_FORMAT_NV12:
		{
			mem_access_method = VIDC_720P_TILE_LINEAR;
			break;
		}
	case VCD_BUFFER_FORMAT_TILE_4x2:
		{
			mem_access_method = VIDC_720P_TILE_64x32;
			break;
		}
	}
	VIDC_LOG_STRING("HEADER-PARSE-START");
	VIDC_DEBUG_REGISTER_LOG;
	seq_h = (u32) seq_hdr->sequence_header;
	start_byte_num = 8 - (seq_h & DDL_STREAMBUF_ALIGN_GUARD_BYTES);
	seq_e = seq_h + seq_hdr->sequence_header_len;
	seq_h &= ~(DDL_STREAMBUF_ALIGN_GUARD_BYTES);
	DDL_PADDING_HACK(seq_e);

	ddl_move_client_state(ddl, DDL_CLIENT_WAIT_FOR_INITCODECDONE);
	ddl_move_command_state(ddl->ddl_context, DDL_CMD_HEADER_PARSE);

	vidc_720p_decode_bitstream_header(ddl->channel_id,
		   seq_hdr->sequence_header_len,
		   start_byte_num,
		   seq_h,
		   seq_e,
		   mem_access_method,
		   decoder->output_order);
}

void ddl_decode_dynamic_property(struct ddl_client_context *ddl,
				 u32 enable)
{
	uint8_t *temp = NULL;
	u32 extra_datastart = 0;
	struct ddl_decoder_data *decoder = &(ddl->codec_data.decoder);
	struct vcd_frame_data *bit_stream =
	    &(ddl->input_frame.vcd_frm);

	if (!enable) {
		if (decoder->dynmic_prop_change_req) {
			decoder->dynmic_prop_change_req = false;
			vidc_720p_decode_dynamic_req_reset();
		}
		return;
	}
	if ((decoder->dynamic_prop_change &
				DDL_DEC_REQ_OUTPUT_FLUSH)) {
		decoder->dynmic_prop_change_req = true;
		decoder->dynamic_prop_change &= ~(DDL_DEC_REQ_OUTPUT_FLUSH);
		decoder->dpb_mask.hw_mask = 0;
		vidc_720p_decode_dynamic_req_set(VIDC_720P_FLUSH_REQ);
	}
	if (((decoder->meta_data_enable_flag & VCD_METADATA_PASSTHROUGH))
	    && ((VCD_FRAME_FLAG_EXTRADATA & bit_stream->flags))
	    ) {

		temp = ((uint8_t *)bit_stream->physical +
					bit_stream->offset +
					bit_stream->data_len + 3);

		extra_datastart = (u32) ((u32)temp & ~3);
		decoder->dynmic_prop_change_req = true;

		vidc_720p_decode_setpassthrough_start(extra_datastart);

		vidc_720p_decode_dynamic_req_set(VIDC_720P_EXTRADATA);
	}
}

void ddl_encode_dynamic_property(struct ddl_client_context *ddl,
				 u32 enable)
{
	struct ddl_encoder_data *encoder = &(ddl->codec_data.encoder);
	u32 enc_param_change = 0;

	if (!enable) {
		if (encoder->dynmic_prop_change_req) {
			encoder->dynmic_prop_change_req = false;
			encoder->ext_enc_control_val &=
				~(VIDC_720P_ENC_IFRAME_REQ);
			vidc_720p_encode_set_control_param
			(encoder->ext_enc_control_val);
			vidc_720p_encoder_set_param_change(enc_param_change);
		}
		return;
	}
	if ((encoder->dynamic_prop_change & DDL_ENC_REQ_IFRAME)) {
		encoder->dynamic_prop_change &= ~(DDL_ENC_REQ_IFRAME);
		encoder->ext_enc_control_val |= VIDC_720P_ENC_IFRAME_REQ;
		vidc_720p_encode_set_control_param
		(encoder->ext_enc_control_val);
	}
	if ((encoder->dynamic_prop_change & DDL_ENC_CHANGE_BITRATE)) {
		vidc_720p_encode_set_bit_rate(
		encoder->target_bit_rate.target_bitrate);
		enc_param_change |= VIDC_720P_ENC_BITRATE_CHANGE;
		encoder->dynamic_prop_change &= ~(DDL_ENC_CHANGE_BITRATE);
	}
	if ((encoder->dynamic_prop_change & DDL_ENC_CHANGE_CIR)) {
		vidc_720p_encode_set_intra_refresh_mb_number(
			encoder->intra_refresh.cir_mb_number);
		encoder->dynamic_prop_change &= ~(DDL_ENC_CHANGE_CIR);
	}
	if ((encoder->dynamic_prop_change & DDL_ENC_CHANGE_IPERIOD)) {
		vidc_720p_encode_set_i_period
			(encoder->i_period.p_frames);
		enc_param_change |= VIDC_720P_ENC_IPERIOD_CHANGE;
		encoder->dynamic_prop_change &= ~(DDL_ENC_CHANGE_IPERIOD);
	}
	if ((encoder->dynamic_prop_change &
				DDL_ENC_CHANGE_FRAMERATE)) {
		vidc_720p_encode_set_fps
		    ((encoder->frame_rate.fps_numerator * 1000) /
		     encoder->frame_rate.fps_denominator);
		enc_param_change |= VIDC_720P_ENC_FRAMERATE_CHANGE;
		encoder->dynamic_prop_change &= ~(DDL_ENC_CHANGE_FRAMERATE);
	}
	if (enc_param_change)
		vidc_720p_encoder_set_param_change(enc_param_change);
}

static void ddl_encode_set_profile_level(struct ddl_client_context *ddl)
{
	struct ddl_encoder_data *encoder = &(ddl->codec_data.encoder);
	u32 profile;
	u32 level;

	switch (encoder->profile.profile) {
	default:
	case VCD_PROFILE_MPEG4_SP:
		{
			profile = VIDC_720P_PROFILE_MPEG4_SP;
			break;
		}
	case VCD_PROFILE_MPEG4_ASP:
		{
			profile = VIDC_720P_PROFILE_MPEG4_ASP;
			break;
		}
	case VCD_PROFILE_H264_BASELINE:
		{
			profile = VIDC_720P_PROFILE_H264_CPB;
			break;
		}
	case VCD_PROFILE_H264_MAIN:
		{
			profile = VIDC_720P_PROFILE_H264_MAIN;
			break;
		}
	case VCD_PROFILE_H264_HIGH:
		{
			profile = VIDC_720P_PROFILE_H264_HIGH;
			break;
		}
	case VCD_PROFILE_H263_BASELINE:
		{
			profile = VIDC_720P_PROFILE_H263_BASELINE;
			break;
		}
	}
	switch (encoder->level.level) {
	default:
	case VCD_LEVEL_MPEG4_0:
		{
			level = VIDC_720P_MPEG4_LEVEL0;
			break;
		}
	case VCD_LEVEL_MPEG4_0b:
		{
			level = VIDC_720P_MPEG4_LEVEL0b;
			break;
		}
	case VCD_LEVEL_MPEG4_1:
		{
			level = VIDC_720P_MPEG4_LEVEL1;
			break;
		}
	case VCD_LEVEL_MPEG4_2:
		{
			level = VIDC_720P_MPEG4_LEVEL2;
			break;
		}
	case VCD_LEVEL_MPEG4_3:
		{
			level = VIDC_720P_MPEG4_LEVEL3;
			break;
		}
	case VCD_LEVEL_MPEG4_3b:
		{
			level = VIDC_720P_MPEG4_LEVEL3b;
			break;
		}

	case VCD_LEVEL_MPEG4_4:
	case VCD_LEVEL_MPEG4_4a:
		{
			level = VIDC_720P_MPEG4_LEVEL4a;
			break;
		}
	case VCD_LEVEL_MPEG4_5:
		{
			level = VIDC_720P_MPEG4_LEVEL5;
			break;
		}
	case VCD_LEVEL_MPEG4_6:
		{
			level = VIDC_720P_MPEG4_LEVEL6;
			break;
		}
	case VCD_LEVEL_H264_1:
		{
			level = VIDC_720P_H264_LEVEL1;
			break;
		}
	case VCD_LEVEL_H264_1b:
		{
			level = VIDC_720P_H264_LEVEL1b;
			break;
		}
	case VCD_LEVEL_H264_1p1:
		{
			level = VIDC_720P_H264_LEVEL1p1;
			break;
		}
	case VCD_LEVEL_H264_1p2:
		{
			level = VIDC_720P_H264_LEVEL1p2;
			break;
		}
	case VCD_LEVEL_H264_1p3:
		{
			level = VIDC_720P_H264_LEVEL1p3;
			break;
		}
	case VCD_LEVEL_H264_2:
		{
			level = VIDC_720P_H264_LEVEL2;
			break;
		}
	case VCD_LEVEL_H264_2p1:
		{
			level = VIDC_720P_H264_LEVEL2p1;
			break;
		}
	case VCD_LEVEL_H264_2p2:
		{
			level = VIDC_720P_H264_LEVEL2p2;
			break;
		}
	case VCD_LEVEL_H264_3:
		{
			level = VIDC_720P_H264_LEVEL3;
			break;
		}
	case VCD_LEVEL_H264_3p1:
		{
			level = VIDC_720P_H264_LEVEL3p1;
			break;
		}
	case VCD_LEVEL_H263_10:
		{
			level = VIDC_720P_H263_LEVEL10;
			break;
		}
	case VCD_LEVEL_H263_20:
		{
			level = VIDC_720P_H263_LEVEL20;
			break;
		}
	case VCD_LEVEL_H263_30:
		{
			level = VIDC_720P_H263_LEVEL30;
			break;
		}
	case VCD_LEVEL_H263_40:
		{
			level = VIDC_720P_H263_LEVEL40;
			break;
		}
	case VCD_LEVEL_H263_45:
		{
			level = VIDC_720P_H263_LEVEL45;
			break;
		}
	case VCD_LEVEL_H263_50:
		{
			level = VIDC_720P_H263_LEVEL50;
			break;
		}
	case VCD_LEVEL_H263_60:
		{
			level = VIDC_720P_H263_LEVEL60;
			break;
		}
	case VCD_LEVEL_H263_70:
		{
			level = VIDC_720P_H263_LEVEL70;
			break;
		}
	}
	vidc_720p_encode_set_profile(profile, level);
}

void ddl_encode_init_codec(struct ddl_client_context *ddl)
{
	struct ddl_encoder_data *encoder = &(ddl->codec_data.encoder);
	enum vidc_720p_memory_access_method mem_access_method;
	enum vidc_720p_DBConfig db_config;
	enum vidc_720p_MSlice_selection m_slice_sel;

	ddl_encode_set_profile_level(ddl);

	vidc_720p_set_frame_size
	    (encoder->frame_size.width, encoder->frame_size.height);
	vidc_720p_encode_set_qp_params
	    (encoder->qp_range.max_qp, encoder->qp_range.min_qp);
	vidc_720p_encode_set_rc_config
	    (encoder->rc_level.frame_level_rc,
	     encoder->rc_level.mb_level_rc,
	     encoder->session_qp.i_frame_qp,
	     encoder->session_qp.p_frame_qp);

	if (encoder->r_cframe_skip) {
		if (encoder->vb_vbuffer_size) {
			encoder->ext_enc_control_val = (0x2 << 0x2) |
			(encoder->vb_vbuffer_size << 0x10);
		} else
			encoder->ext_enc_control_val = (0x1 << 2);
	} else
		encoder->ext_enc_control_val = 0;

	vidc_720p_encode_set_fps
	    ((encoder->frame_rate.fps_numerator * 1000) /
	     encoder->frame_rate.fps_denominator);

	vidc_720p_encode_set_vop_time(
			encoder->vop_timing.vop_time_resolution, 0);

	if (encoder->rc_level.frame_level_rc) {
		vidc_720p_encode_set_bit_rate
		    (encoder->target_bit_rate.target_bitrate);

		vidc_720p_encode_set_frame_level_rc_params
		    (encoder->frame_level_rc.reaction_coeff);
	}
	if (encoder->rc_level.mb_level_rc) {
		vidc_720p_encode_set_mb_level_rc_params
		    (encoder->adaptive_rc.dark_region_as_flag,
		     encoder->adaptive_rc.smooth_region_as_flag,
		     encoder->adaptive_rc.static_region_as_flag,
		     encoder->adaptive_rc.activity_region_flag);
	}
	if (encoder->codec.codec == VCD_CODEC_MPEG4) {
		vidc_720p_encode_set_short_header
		    (encoder->short_header.short_header);

		if (encoder->hdr_ext_control) {
			vidc_720p_encode_set_hec_period
			(encoder->hdr_ext_control);
			encoder->ext_enc_control_val |= (0x1 << 0x1);
		}
	}
	/* set extended encoder control settings */
	vidc_720p_encode_set_control_param
	(encoder->ext_enc_control_val);

	if (encoder->codec.codec == VCD_CODEC_H264) {
		enum vidc_720p_entropy_sel entropy_sel;
		enum vidc_720p_cabac_model cabac_model_number;
		switch (encoder->entropy_control.entropy_sel) {
		default:
		case VCD_ENTROPY_SEL_CAVLC:
			{
				entropy_sel = VIDC_720P_ENTROPY_SEL_CAVLC;
				break;
			}
		case VCD_ENTROPY_SEL_CABAC:
			{
				entropy_sel = VIDC_720P_ENTROPY_SEL_CABAC;
				break;
			}
		}
		switch (encoder->entropy_control.cabac_model) {
		default:
		case VCD_CABAC_MODEL_NUMBER_0:
			{
				cabac_model_number =
				    VIDC_720P_CABAC_MODEL_NUMBER_0;
				break;
			}
		case VCD_CABAC_MODEL_NUMBER_1:
			{
				cabac_model_number =
				    VIDC_720P_CABAC_MODEL_NUMBER_1;
				break;
			}
		case VCD_CABAC_MODEL_NUMBER_2:
			{
				cabac_model_number =
				    VIDC_720P_CABAC_MODEL_NUMBER_2;
				break;
			}
		}
		vidc_720p_encode_set_entropy_control
		    (entropy_sel, cabac_model_number);
		switch (encoder->db_control.db_config) {
		default:
		case VCD_DB_ALL_BLOCKING_BOUNDARY:
			{
				db_config =
				    VIDC_720P_DB_ALL_BLOCKING_BOUNDARY;
				break;
			}
		case VCD_DB_DISABLE:
			{
				db_config =
				    VIDC_720P_DB_DISABLE;
				break;
			}
		case VCD_DB_SKIP_SLICE_BOUNDARY:
			{
				db_config =
				    VIDC_720P_DB_SKIP_SLICE_BOUNDARY;
				break;
			}
		}
		vidc_720p_encode_set_db_filter_control
		    (db_config,
		     encoder->db_control.slice_alpha_offset,
		     encoder->db_control.slice_beta_offset);
	}

	vidc_720p_encode_set_intra_refresh_mb_number
	    (encoder->intra_refresh.cir_mb_number);

	switch (encoder->multi_slice.m_slice_sel) {
	default:
	case VCD_MSLICE_OFF:
		m_slice_sel = VIDC_720P_MSLICE_OFF;
		break;
	case VCD_MSLICE_BY_MB_COUNT:
		{
			m_slice_sel = VIDC_720P_MSLICE_BY_MB_COUNT;
			break;
		}
	case VCD_MSLICE_BY_BYTE_COUNT:
		{
			m_slice_sel = VIDC_720P_MSLICE_BY_BYTE_COUNT;
			break;
		}
	case VCD_MSLICE_BY_GOB:
		{
			m_slice_sel = VIDC_720P_MSLICE_BY_GOB;
			break;
		}
	}
	vidc_720p_encode_set_multi_slice_info
	    (m_slice_sel, encoder->multi_slice.m_slice_size);

	vidc_720p_encode_set_dpb_buffer
	    (encoder->enc_dpb_addr.align_physical_addr,
			 encoder->enc_dpb_addr.buffer_size);

	VIDC_LOG1("ENC_DPB_ADDR_SIZE", encoder->enc_dpb_addr.buffer_size);

	vidc_720p_encode_set_i_period(encoder->i_period.p_frames);

	ddl_metadata_enable(ddl);

	if (encoder->seq_header.virtual_base_addr) {
		u32 ext_buffer_start, ext_buffer_end, start_byte_num;
		ext_buffer_start =
		    (u32) encoder->seq_header.align_physical_addr;
		ext_buffer_end =
		    ext_buffer_start + encoder->seq_header.buffer_size;
		start_byte_num =
		    (ext_buffer_start & DDL_STREAMBUF_ALIGN_GUARD_BYTES);
		ext_buffer_start &= ~(DDL_STREAMBUF_ALIGN_GUARD_BYTES);
		ext_buffer_end &= ~(DDL_STREAMBUF_ALIGN_GUARD_BYTES);
		VIDC_LOG1("ENC_SEQHDR_ALLOC_SIZE",
			   encoder->seq_header.buffer_size);
		vidc_720p_encode_set_seq_header_buffer(ext_buffer_start,
							ext_buffer_end,
							start_byte_num);
	}

	if (encoder->re_con_buf_format.buffer_format ==
		VCD_BUFFER_FORMAT_NV12)
		mem_access_method = VIDC_720P_TILE_LINEAR;
	else
		mem_access_method = VIDC_720P_TILE_16x16;

	ddl_move_client_state(ddl, DDL_CLIENT_WAIT_FOR_INITCODECDONE);
	ddl_move_command_state(ddl->ddl_context, DDL_CMD_INIT_CODEC);

	vidc_720p_encode_init_codec(ddl->channel_id, mem_access_method);
}

void ddl_channel_end(struct ddl_client_context *ddl)
{
	VIDC_DEBUG_REGISTER_LOG;

	ddl_move_client_state(ddl, DDL_CLIENT_WAIT_FOR_CHEND);
	ddl_move_command_state(ddl->ddl_context, DDL_CMD_CHANNEL_END);

	vidc_720p_submit_command(ddl->channel_id, VIDC_720P_CMD_CHEND);
}

void ddl_encode_frame_run(struct ddl_client_context *ddl)
{
	u32 ext_buffer_start, ext_buffer_end;
	u32 y_addr, c_addr;
	u32 start_byte_number = 0;
	struct ddl_encoder_data *encoder = &(ddl->codec_data.encoder);
	struct vcd_frame_data *stream = &(ddl->output_frame.vcd_frm);

	ext_buffer_start = (u32) stream->physical + stream->offset;
	ext_buffer_end = ddl_encode_set_metadata_output_buf(ddl);
	start_byte_number =
	    (ext_buffer_start & DDL_STREAMBUF_ALIGN_GUARD_BYTES);
	if (start_byte_number) {
		u32 upper_data, lower_data;
		u32 *align_virtual_addr;
		ext_buffer_start &= ~(DDL_STREAMBUF_ALIGN_GUARD_BYTES);
		align_virtual_addr = (u32 *) (((u32) stream->virtual +
						 stream->offset) -
						start_byte_number);
		upper_data = *align_virtual_addr;
		align_virtual_addr++;
		lower_data = *align_virtual_addr;
		vidc_720p_encode_unalign_bitstream(upper_data, lower_data);
	}

	y_addr = (u32) ddl->input_frame.vcd_frm.physical +
	    ddl->input_frame.vcd_frm.offset;
	c_addr = (y_addr + (encoder->frame_size.scan_lines *
				encoder->frame_size.stride));
	ddl_move_client_state(ddl, DDL_CLIENT_WAIT_FOR_FRAME_DONE);
	ddl_move_command_state(ddl->ddl_context, DDL_CMD_ENCODE_FRAME);

	if (encoder->dynamic_prop_change) {
		encoder->dynmic_prop_change_req = true;
		ddl_encode_dynamic_property(ddl, true);
	}
	vidc_720p_encode_set_vop_time(
			encoder->vop_timing.vop_time_resolution,
			ddl->input_frame.frm_delta
	    );

	vidc_720p_encode_frame(ddl->channel_id,
			ext_buffer_start,
				ext_buffer_end,
				start_byte_number, y_addr, c_addr);
}

u32 ddl_decode_set_buffers(struct ddl_client_context *ddl)
{
	struct ddl_decoder_data *decoder = &(ddl->codec_data.decoder);
	u32 comv_buf_size = DDL_COMV_BUFLINE_NO, comv_buf_no = 0;
	u32 ref_buf_no = 0;
	struct ddl_context  *ddl_ctxt = NULL;

	if (!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_DPB)) {
		VIDC_LOG_STRING("STATE-CRITICAL");
		return VCD_ERR_FAIL;
	}
	if (vidc_msg_timing)
		ddl_set_core_start_time(__func__, DEC_OP_TIME);
	switch (decoder->codec.codec) {
	default:
	case VCD_CODEC_DIVX_4:
	case VCD_CODEC_DIVX_5:
	case VCD_CODEC_DIVX_6:
	case VCD_CODEC_XVID:
	case VCD_CODEC_MPEG2:
	case VCD_CODEC_MPEG4:
		{
			comv_buf_no = DDL_MPEG_COMV_BUF_NO;
			ref_buf_no = DDL_MPEG_REFBUF_COUNT;
			break;
		}
	case VCD_CODEC_H263:
		{
			comv_buf_no = DDL_H263_COMV_BUF_NO;
			break;
		}
	case VCD_CODEC_VC1:
	case VCD_CODEC_VC1_RCV:
		{
			comv_buf_no =
			    decoder->client_output_buf_req.actual_count + 1;
			comv_buf_size = DDL_VC1_COMV_BUFLINE_NO;
			break;
		}
	case VCD_CODEC_H264:
		{
			if (decoder->idr_only_decoding)
				comv_buf_no = decoder->min_dpb_num;
			else
				comv_buf_no =
					decoder->
					client_output_buf_req.
					actual_count;
			break;
		}
	}

	if (comv_buf_no) {
		comv_buf_size *= (comv_buf_no *
			(decoder->client_frame_size.stride >> 4) *
			((decoder->client_frame_size.scan_lines >> 4) + 1));
		if (decoder->dpb_comv_buffer.virtual_base_addr)
			ddl_pmem_free(&decoder->dpb_comv_buffer);
		ddl_pmem_alloc(&decoder->dpb_comv_buffer, comv_buf_size,
			       DDL_LINEAR_BUFFER_ALIGN_BYTES);
		if (!decoder->dpb_comv_buffer.virtual_base_addr) {
			VIDC_LOGERR_STRING
			    ("Dec_set_buf:Comv_buf_alloc_failed");
			return VCD_ERR_ALLOC_FAIL;
		}
		vidc_720p_decode_set_comv_buffer(decoder->dpb_comv_buffer.
						  align_physical_addr,
						  decoder->dpb_comv_buffer.
						  buffer_size);
	}
	decoder->ref_buffer.align_physical_addr = NULL;
	if (ref_buf_no) {
		size_t sz, align_bytes, y_sz, frm_sz;
		u32 i = 0;
		sz = decoder->dp_buf.dec_pic_buffers[0].vcd_frm.alloc_len;
		frm_sz = sz;
		y_sz = decoder->client_frame_size.height *
				decoder->client_frame_size.width;
		sz *= ref_buf_no;
		align_bytes = decoder->client_output_buf_req.align;
		if (decoder->ref_buffer.virtual_base_addr)
			ddl_pmem_free(&decoder->ref_buffer);
		ddl_pmem_alloc(&decoder->ref_buffer, sz, align_bytes);
		if (!decoder->ref_buffer.virtual_base_addr) {
			ddl_pmem_free(&decoder->dpb_comv_buffer);
			VIDC_LOGERR_STRING
			    ("Dec_set_buf:mpeg_ref_buf_alloc_failed");
			return VCD_ERR_ALLOC_FAIL;
		}
		memset((u8 *)decoder->ref_buffer.virtual_base_addr,
			0x80, sz);
		for (i = 0; i < ref_buf_no; i++)
			memset((u8 *)decoder->ref_buffer.align_virtual_addr +
				i*frm_sz, 0x10, y_sz);
	}
	ddl_decode_set_metadata_output(decoder);
	ddl_decoder_dpb_transact(decoder, NULL, DDL_DPB_OP_INIT);
	ddl_ctxt = ddl_get_context();
	vidc_720p_set_deblock_line_buffer(
		ddl_ctxt->db_line_buffer.align_physical_addr,
		ddl_ctxt->db_line_buffer.buffer_size);
	ddl_move_client_state(ddl, DDL_CLIENT_WAIT_FOR_DPBDONE);
	ddl_move_command_state(ddl->ddl_context, DDL_CMD_DECODE_SET_DPB);

	vidc_720p_submit_command(ddl->channel_id,
		VIDC_720P_CMD_INITBUFFERS);
	return VCD_S_SUCCESS;
}

void ddl_decode_frame_run(struct ddl_client_context *ddl)
{
	u32 ext_buffer_start = 0, ext_buffer_end = 0;
	u32 start_byte_num = 8;
	struct ddl_decoder_data *decoder = &ddl->codec_data.decoder;
	struct vcd_frame_data *bit_stream =
	    &(ddl->input_frame.vcd_frm);
	if (vidc_msg_timing) {
		ddl_set_core_start_time(__func__, DEC_OP_TIME);
		ddl_set_core_start_time(__func__, DEC_IP_TIME);
	}
	if (!bit_stream->data_len ||
		!bit_stream->physical) {
		ddl_decode_eos_run(ddl);
		return;
	}

	ddl_move_client_state(ddl, DDL_CLIENT_WAIT_FOR_FRAME_DONE);

	ddl_decode_dynamic_property(ddl, true);

	ddl_decoder_dpb_transact(decoder, NULL, DDL_DPB_OP_SET_MASK);

	ext_buffer_start = (u32)bit_stream->physical +
		bit_stream->offset;
	start_byte_num = 8 - (ext_buffer_start &
		DDL_STREAMBUF_ALIGN_GUARD_BYTES);
	ext_buffer_end = ext_buffer_start + bit_stream->data_len;
	ext_buffer_start &= ~(DDL_STREAMBUF_ALIGN_GUARD_BYTES);
	DDL_PADDING_HACK(ext_buffer_end);

	ddl_move_command_state(ddl->ddl_context, DDL_CMD_DECODE_FRAME);

	vidc_720p_decode_frame(ddl->channel_id,
			ext_buffer_start,
			ext_buffer_end,
			bit_stream->data_len,
			start_byte_num, bit_stream->ip_frm_tag);
}

void  ddl_decode_eos_run(struct ddl_client_context *ddl)
{
	struct ddl_decoder_data *decoder = &ddl->codec_data.decoder;

	ddl_move_client_state(ddl, DDL_CLIENT_WAIT_FOR_EOS_DONE);

	ddl_decode_dynamic_property(ddl, true);

	ddl_decoder_dpb_transact(decoder, NULL, DDL_DPB_OP_SET_MASK);

	decoder->dynmic_prop_change_req = true;

	ddl_move_command_state(ddl->ddl_context, DDL_CMD_EOS);

	vidc_720p_issue_eos(ddl->channel_id);
}

u32 ddl_hal_engine_reset(struct ddl_context *ddl_context)
{
	u32 eng_reset;
	u32 channel_id = 0;
	u32 fw_endianness;
	enum vidc_720p_endian dma_endian;
	enum vidc_720p_interrupt_level_selection interrupt_sel;
	u32 intr_mask = 0x0;

	if (ddl_context->current_ddl)
		channel_id = ddl_context->current_ddl->channel_id;

	interrupt_sel = VIDC_720P_INTERRUPT_LEVEL_SEL;
	/* Enable all the supported interrupt */
	intr_mask |= VIDC_720P_INTR_BUFFER_FULL;
	intr_mask |= VIDC_720P_INTR_FW_DONE;
	intr_mask |= VIDC_720P_INTR_DMA_DONE;
	intr_mask |= VIDC_720P_INTR_FRAME_DONE;

	vcd_get_fw_property(VCD_FW_ENDIAN, &fw_endianness);
	/* Reverse the endianness settings after boot code download */
	if (fw_endianness == VCD_FW_BIG_ENDIAN)
		dma_endian = VIDC_720P_LITTLE_ENDIAN;
	else
		dma_endian = VIDC_720P_BIG_ENDIAN;

	/* Need to reset MFC silently */
	eng_reset = vidc_720p_engine_reset(
		channel_id,
		dma_endian, interrupt_sel,
		intr_mask);
	if (!eng_reset) {
		/* call the hw fatal callback if engine reset fails */
		ddl_hw_fatal_cb(ddl_context);
	}
	return eng_reset ;
}
