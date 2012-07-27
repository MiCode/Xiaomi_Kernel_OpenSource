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
#include "vcd_res_tracker_api.h"

u32 ddl_device_init(struct ddl_init_config *ddl_init_config,
		    void *client_data)
{
	struct ddl_context *ddl_context;
	u32 status = VCD_S_SUCCESS;

	if ((!ddl_init_config) ||
	    (!ddl_init_config->ddl_callback) ||
	    (!ddl_init_config->core_virtual_base_addr)
	    ) {
		VIDC_LOGERR_STRING("ddl_dev_init:Bad_argument");
		return VCD_ERR_ILLEGAL_PARM;
	}

	ddl_context = ddl_get_context();

	if (DDL_IS_INITIALIZED(ddl_context)) {
		VIDC_LOGERR_STRING("ddl_dev_init:Multiple_init");
		return VCD_ERR_ILLEGAL_OP;
	}
	if (DDL_IS_BUSY(ddl_context)) {
		VIDC_LOGERR_STRING("ddl_dev_init:Ddl_busy");
		return VCD_ERR_BUSY;
	}

	DDL_MEMSET(ddl_context, 0, sizeof(struct ddl_context));
	DDL_BUSY(ddl_context);

	if (res_trk_get_enable_ion()) {
		VIDC_LOGERR_STRING("ddl_dev_init: ION framework enabled");
		ddl_context->video_ion_client  =
			res_trk_get_ion_client();
		if (!ddl_context->video_ion_client) {
			VIDC_LOGERR_STRING("ION client create failed");
			return VCD_ERR_ILLEGAL_OP;
		}
	}
	ddl_context->memtype = res_trk_get_mem_type();
	if (ddl_context->memtype == -1) {
		VIDC_LOGERR_STRING("ddl_dev_init:Invalid Memtype");
		return VCD_ERR_ILLEGAL_PARM;
	}
	ddl_context->ddl_callback = ddl_init_config->ddl_callback;
	ddl_context->interrupt_clr = ddl_init_config->interrupt_clr;
	ddl_context->core_virtual_base_addr =
	    ddl_init_config->core_virtual_base_addr;
	ddl_context->client_data = client_data;

	vidc_720p_set_device_virtual_base(ddl_context->
					   core_virtual_base_addr);

	ddl_context->current_ddl = NULL;
	ddl_move_command_state(ddl_context, DDL_CMD_INVALID);

	ddl_client_transact(DDL_INIT_CLIENTS, NULL);

	ddl_pmem_alloc(&ddl_context->context_buf_addr,
		       DDL_CONTEXT_MEMORY, DDL_LINEAR_BUFFER_ALIGN_BYTES);
	if (!ddl_context->context_buf_addr.virtual_base_addr) {
		VIDC_LOGERR_STRING("ddl_dev_init:Context_alloc_fail");
		status = VCD_ERR_ALLOC_FAIL;
	}
	if (!status) {
		ddl_pmem_alloc(&ddl_context->db_line_buffer,
			       DDL_DB_LINE_BUF_SIZE,
			       DDL_TILE_BUFFER_ALIGN_BYTES);
		if (!ddl_context->db_line_buffer.virtual_base_addr) {
			VIDC_LOGERR_STRING("ddl_dev_init:Line_buf_alloc_fail");
			status = VCD_ERR_ALLOC_FAIL;
		}
	}

	if (!status) {
		ddl_pmem_alloc(&ddl_context->data_partition_tempbuf,
					   DDL_MPEG4_DATA_PARTITION_BUF_SIZE,
					   DDL_TILE_BUFFER_ALIGN_BYTES);
		if (ddl_context->data_partition_tempbuf.virtual_base_addr \
			== NULL) {
			VIDC_LOGERR_STRING
				("ddl_dev_init:Data_partition_buf_alloc_fail");
			status = VCD_ERR_ALLOC_FAIL;
		}
   }

   if (!status) {

		ddl_pmem_alloc(&ddl_context->metadata_shared_input,
					   DDL_METADATA_TOTAL_INPUTBUFSIZE,
					   DDL_LINEAR_BUFFER_ALIGN_BYTES);
		if (!ddl_context->metadata_shared_input.virtual_base_addr) {
			VIDC_LOGERR_STRING
			("ddl_dev_init:metadata_shared_input_alloc_fail");
			status = VCD_ERR_ALLOC_FAIL;
		}
	 }

	if (!status) {
		ddl_pmem_alloc(&ddl_context->dbg_core_dump, \
					   DDL_DBG_CORE_DUMP_SIZE,  \
					   DDL_LINEAR_BUFFER_ALIGN_BYTES);
		if (!ddl_context->dbg_core_dump.virtual_base_addr) {
			VIDC_LOGERR_STRING
				("ddl_dev_init:dbg_core_dump_alloc_failed");
			status = VCD_ERR_ALLOC_FAIL;
		}
		ddl_context->enable_dbg_core_dump = 0;
	}

	if (!status && !vcd_fw_init()) {
		VIDC_LOGERR_STRING("ddl_dev_init:fw_init_failed");
		status = VCD_ERR_ALLOC_FAIL;
	}
	if (status) {
		ddl_release_context_buffers(ddl_context);
		DDL_IDLE(ddl_context);
		return status;
	}

	ddl_move_command_state(ddl_context, DDL_CMD_DMA_INIT);

	ddl_core_init(ddl_context);

	return status;
}

u32 ddl_device_release(void *client_data)
{
	struct ddl_context *ddl_context;

	ddl_context = ddl_get_context();

	if (DDL_IS_BUSY(ddl_context)) {
		VIDC_LOGERR_STRING("ddl_dev_rel:Ddl_busy");
		return VCD_ERR_BUSY;
	}

	if (!DDL_IS_INITIALIZED(ddl_context)) {
		VIDC_LOGERR_STRING("ddl_dev_rel:Not_inited");
		return VCD_ERR_ILLEGAL_OP;
	}

	if (!ddl_client_transact(DDL_ACTIVE_CLIENT, NULL)) {
		VIDC_LOGERR_STRING("ddl_dev_rel:Client_present_err");
		return VCD_ERR_CLIENT_PRESENT;
	}
	DDL_BUSY(ddl_context);

	ddl_context->device_state = DDL_DEVICE_NOTINIT;
	ddl_context->client_data = client_data;
	ddl_move_command_state(ddl_context, DDL_CMD_INVALID);
	vidc_720p_stop_fw();

	VIDC_LOG_STRING("FW_ENDDONE");
	ddl_release_context_buffers(ddl_context);
	ddl_context->video_ion_client = NULL;
	DDL_IDLE(ddl_context);

	return VCD_S_SUCCESS;
}

u32 ddl_open(u32 **ddl_handle, u32 decoding)
{
	struct ddl_context *ddl_context;
	struct ddl_client_context *ddl;
	u32 status;

	if (!ddl_handle) {
		VIDC_LOGERR_STRING("ddl_open:Bad_handle");
		return VCD_ERR_BAD_HANDLE;
	}

	ddl_context = ddl_get_context();

	if (!DDL_IS_INITIALIZED(ddl_context)) {
		VIDC_LOGERR_STRING("ddl_open:Not_inited");
		return VCD_ERR_ILLEGAL_OP;
	}

	status = ddl_client_transact(DDL_GET_CLIENT, &ddl);

	if (status) {
		VIDC_LOGERR_STRING("ddl_open:Client_trasac_failed");
		return status;
	}

	ddl_move_client_state(ddl, DDL_CLIENT_OPEN);

	ddl->codec_data.hdr.decoding = decoding;
	ddl->decoding = decoding;

	ddl_set_default_meta_data_hdr(ddl);

	ddl_set_initial_default_values(ddl);

	*ddl_handle = (u32 *) ddl;
	return VCD_S_SUCCESS;
}

u32 ddl_close(u32 **ddl_handle)
{
	struct ddl_context *ddl_context;
	struct ddl_client_context **ddl =
	    (struct ddl_client_context **)ddl_handle;

	if (!ddl || !*ddl) {
		VIDC_LOGERR_STRING("ddl_close:Bad_handle");
		return VCD_ERR_BAD_HANDLE;
	}

	ddl_context = ddl_get_context();

	if (!DDL_IS_INITIALIZED(ddl_context)) {
		VIDC_LOGERR_STRING("ddl_close:Not_inited");
		return VCD_ERR_ILLEGAL_OP;
	}

	if (!DDLCLIENT_STATE_IS(*ddl, DDL_CLIENT_OPEN)) {
		VIDC_LOGERR_STRING("ddl_close:Not_in_open_state");
		return VCD_ERR_ILLEGAL_OP;
	}

	ddl_move_client_state(*ddl, DDL_CLIENT_INVALID);
	if ((*ddl)->decoding) {
		vcd_fw_transact(false, true,
			(*ddl)->codec_data.decoder.codec.codec);
	} else {
		vcd_fw_transact(false, false,
			(*ddl)->codec_data.encoder.codec.codec);
	}
	ddl_client_transact(DDL_FREE_CLIENT, ddl);

	return VCD_S_SUCCESS;
}

u32 ddl_encode_start(u32 *ddl_handle, void *client_data)
{
	struct ddl_client_context *ddl =
	    (struct ddl_client_context *)ddl_handle;
	struct ddl_context *ddl_context;
	struct ddl_encoder_data *encoder;
	u32 dpb_size;

	ddl_context = ddl_get_context();

	if (!DDL_IS_INITIALIZED(ddl_context)) {
		VIDC_LOGERR_STRING("ddl_enc_start:Not_inited");
		return VCD_ERR_ILLEGAL_OP;
	}
	if (DDL_IS_BUSY(ddl_context)) {
		VIDC_LOGERR_STRING("ddl_enc_start:Ddl_busy");
		return VCD_ERR_BUSY;
	}
	if (!ddl || ddl->decoding) {
		VIDC_LOGERR_STRING("ddl_enc_start:Bad_handle");
		return VCD_ERR_BAD_HANDLE;
	}

	if (!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_OPEN)) {
		VIDC_LOGERR_STRING("ddl_enc_start:Not_opened");
		return VCD_ERR_ILLEGAL_OP;
	}

	if (!ddl_encoder_ready_to_start(ddl)) {
		VIDC_LOGERR_STRING("ddl_enc_start:Err_param_settings");
		return VCD_ERR_ILLEGAL_OP;
	}

	encoder = &ddl->codec_data.encoder;

	dpb_size = ddl_get_yuv_buffer_size(&encoder->frame_size,
					&encoder->re_con_buf_format, false,
					encoder->codec.codec);

	dpb_size *= DDL_ENC_NUM_DPB_BUFFERS;
	ddl_pmem_alloc(&encoder->enc_dpb_addr,
		       dpb_size, DDL_TILE_BUFFER_ALIGN_BYTES);
	if (!encoder->enc_dpb_addr.virtual_base_addr) {
		VIDC_LOGERR_STRING("ddl_enc_start:Dpb_alloc_failed");
		return VCD_ERR_ALLOC_FAIL;
	}

	if ((encoder->codec.codec == VCD_CODEC_MPEG4 &&
	     !encoder->short_header.short_header) ||
	    encoder->codec.codec == VCD_CODEC_H264) {
		ddl_pmem_alloc(&encoder->seq_header,
			       DDL_ENC_SEQHEADER_SIZE,
			       DDL_LINEAR_BUFFER_ALIGN_BYTES);
		if (!encoder->seq_header.virtual_base_addr) {
			ddl_pmem_free(&encoder->enc_dpb_addr);
			VIDC_LOGERR_STRING
			    ("ddl_enc_start:Seq_hdr_alloc_failed");
			return VCD_ERR_ALLOC_FAIL;
		}
	} else {
		encoder->seq_header.buffer_size = 0;
		encoder->seq_header.virtual_base_addr = 0;
	}

	DDL_BUSY(ddl_context);

	ddl_context->current_ddl = ddl;
	ddl_context->client_data = client_data;
	ddl_channel_set(ddl);
	return VCD_S_SUCCESS;
}

u32 ddl_decode_start(u32 *ddl_handle,
     struct vcd_sequence_hdr *header, void *client_data)
{
	struct ddl_client_context *ddl =
	    (struct ddl_client_context *)ddl_handle;
	struct ddl_context *ddl_context;
	struct ddl_decoder_data *decoder;

	ddl_context = ddl_get_context();

	if (!DDL_IS_INITIALIZED(ddl_context)) {
		VIDC_LOGERR_STRING("ddl_dec_start:Not_inited");
		return VCD_ERR_ILLEGAL_OP;
	}
	if (DDL_IS_BUSY(ddl_context)) {
		VIDC_LOGERR_STRING("ddl_dec_start:Ddl_busy");
		return VCD_ERR_BUSY;
	}
	if (!ddl || !ddl->decoding) {
		VIDC_LOGERR_STRING("ddl_dec_start:Bad_handle");
		return VCD_ERR_BAD_HANDLE;
	}
	if (!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_OPEN)) {
		VIDC_LOGERR_STRING("ddl_dec_start:Not_in_opened_state");
		return VCD_ERR_ILLEGAL_OP;
	}

	if ((header) &&
	    ((!header->sequence_header_len) ||
	     (!header->sequence_header)
	    )
	    ) {
		VIDC_LOGERR_STRING("ddl_dec_start:Bad_param_seq_header");
		return VCD_ERR_ILLEGAL_PARM;
	}

	if (!ddl_decoder_ready_to_start(ddl, header)) {
		VIDC_LOGERR_STRING("ddl_dec_start:Err_param_settings");
		return VCD_ERR_ILLEGAL_OP;
	}

	DDL_BUSY(ddl_context);

	decoder = &ddl->codec_data.decoder;
	if (header) {
		decoder->header_in_start = true;
		decoder->decode_config = *header;
	} else {
		decoder->header_in_start = false;
		decoder->decode_config.sequence_header_len = 0;
	}

	if (decoder->codec.codec == VCD_CODEC_H264) {
		ddl_pmem_alloc(&decoder->h264Vsp_temp_buffer,
			       DDL_DECODE_H264_VSPTEMP_BUFSIZE,
			       DDL_LINEAR_BUFFER_ALIGN_BYTES);
		if (!decoder->h264Vsp_temp_buffer.virtual_base_addr) {
			DDL_IDLE(ddl_context);
			VIDC_LOGERR_STRING
			    ("ddl_dec_start:H264Sps_alloc_failed");
			return VCD_ERR_ALLOC_FAIL;
		}
	}

	ddl_context->current_ddl = ddl;
	ddl_context->client_data = client_data;

	ddl_channel_set(ddl);
	return VCD_S_SUCCESS;
}

u32 ddl_decode_frame(u32 *ddl_handle,
     struct ddl_frame_data_tag *input_bits, void *client_data)
{
	u32 vcd_status = VCD_S_SUCCESS;
	struct ddl_client_context *ddl =
	    (struct ddl_client_context *)ddl_handle;
	struct ddl_context *ddl_context = ddl_get_context();

	if (!DDL_IS_INITIALIZED(ddl_context)) {
		VIDC_LOGERR_STRING("ddl_dec_frame:Not_inited");
		return VCD_ERR_ILLEGAL_OP;
	}
	if (DDL_IS_BUSY(ddl_context)) {
		VIDC_LOGERR_STRING("ddl_dec_frame:Ddl_busy");
		return VCD_ERR_BUSY;
	}
	if (!ddl || !ddl->decoding) {
		VIDC_LOGERR_STRING("ddl_dec_frame:Bad_handle");
		return VCD_ERR_BAD_HANDLE;
	}
	if (!input_bits ||
	    ((!input_bits->vcd_frm.physical ||
	      !input_bits->vcd_frm.data_len) &&
	     (!(VCD_FRAME_FLAG_EOS & input_bits->vcd_frm.flags))
	    )
	    ) {
		VIDC_LOGERR_STRING("ddl_dec_frame:Bad_input_param");
		return VCD_ERR_ILLEGAL_PARM;
	}

	DDL_BUSY(ddl_context);

	ddl_context->current_ddl = ddl;
	ddl_context->client_data = client_data;

	ddl->input_frame = *input_bits;

	if (DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_FRAME)) {
		ddl_decode_frame_run(ddl);
	} else {
		if (!ddl->codec_data.decoder.dp_buf.no_of_dec_pic_buf) {
			VIDC_LOGERR_STRING("ddl_dec_frame:Dpbs_requied");
			vcd_status = VCD_ERR_ILLEGAL_OP;
		} else if (DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_DPB)) {
			vcd_status = ddl_decode_set_buffers(ddl);
		} else
		    if (DDLCLIENT_STATE_IS
			(ddl, DDL_CLIENT_WAIT_FOR_INITCODEC)) {
			ddl->codec_data.decoder.decode_config.
			    sequence_header =
			    ddl->input_frame.vcd_frm.physical;
			ddl->codec_data.decoder.decode_config.
			    sequence_header_len =
			    ddl->input_frame.vcd_frm.data_len;
			ddl_decode_init_codec(ddl);
		} else {
			VIDC_LOGERR_STRING("Dec_frame:Wrong_state");
			vcd_status = VCD_ERR_ILLEGAL_OP;
		}
		if (vcd_status)
			DDL_IDLE(ddl_context);
	}
	return vcd_status;
}

u32 ddl_encode_frame(u32 *ddl_handle,
     struct ddl_frame_data_tag *input_frame,
     struct ddl_frame_data_tag *output_bit, void *client_data)
{
	struct ddl_client_context *ddl =
	    (struct ddl_client_context *)ddl_handle;
	struct ddl_context *ddl_context = ddl_get_context();

	if (vidc_msg_timing)
		ddl_set_core_start_time(__func__, ENC_OP_TIME);

	if (!DDL_IS_INITIALIZED(ddl_context)) {
		VIDC_LOGERR_STRING("ddl_enc_frame:Not_inited");
		return VCD_ERR_ILLEGAL_OP;
	}
	if (DDL_IS_BUSY(ddl_context)) {
		VIDC_LOGERR_STRING("ddl_enc_frame:Ddl_busy");
		return VCD_ERR_BUSY;
	}
	if (!ddl || ddl->decoding) {
		VIDC_LOGERR_STRING("ddl_enc_frame:Bad_handle");
		return VCD_ERR_BAD_HANDLE;
	}
	if (!input_frame ||
	    !input_frame->vcd_frm.physical ||
	    !input_frame->vcd_frm.data_len) {
		VIDC_LOGERR_STRING("ddl_enc_frame:Bad_input_params");
		return VCD_ERR_ILLEGAL_PARM;
	}
	if ((((u32) input_frame->vcd_frm.physical +
		   input_frame->vcd_frm.offset) &
		  (DDL_STREAMBUF_ALIGN_GUARD_BYTES)
	    )
	    ) {
		VIDC_LOGERR_STRING
		    ("ddl_enc_frame:Un_aligned_yuv_start_address");
		return VCD_ERR_ILLEGAL_PARM;
	}
	if (!output_bit ||
	    !output_bit->vcd_frm.physical ||
	    !output_bit->vcd_frm.alloc_len) {
		VIDC_LOGERR_STRING("ddl_enc_frame:Bad_output_params");
		return VCD_ERR_ILLEGAL_PARM;
	}
	if ((ddl->codec_data.encoder.output_buf_req.sz +
	     output_bit->vcd_frm.offset) >
	    output_bit->vcd_frm.alloc_len) {
		VIDC_LOGERR_STRING
		    ("ddl_enc_frame:offset_large, Exceeds_min_buf_size");
	}
	if (!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_FRAME)) {
		VIDC_LOGERR_STRING("ddl_enc_frame:Wrong_state");
		return VCD_ERR_ILLEGAL_OP;
	}

	DDL_BUSY(ddl_context);

	ddl_context->current_ddl = ddl;
	ddl_context->client_data = client_data;

	ddl->input_frame = *input_frame;
	ddl->output_frame = *output_bit;

	ddl_encode_frame_run(ddl);
	return VCD_S_SUCCESS;
}

u32 ddl_decode_end(u32 *ddl_handle, void *client_data)
{
	struct ddl_client_context *ddl =
	    (struct ddl_client_context *)ddl_handle;
	struct ddl_context *ddl_context;

	ddl_context = ddl_get_context();

	if (vidc_msg_timing) {
		ddl_reset_core_time_variables(DEC_OP_TIME);
		ddl_reset_core_time_variables(DEC_IP_TIME);
	}

	if (!DDL_IS_INITIALIZED(ddl_context)) {
		VIDC_LOGERR_STRING("ddl_dec_end:Not_inited");
		return VCD_ERR_ILLEGAL_OP;
	}
	if (DDL_IS_BUSY(ddl_context)) {
		VIDC_LOGERR_STRING("ddl_dec_end:Ddl_busy");
		return VCD_ERR_BUSY;
	}
	if (!ddl || !ddl->decoding) {
		VIDC_LOGERR_STRING("ddl_dec_end:Bad_handle");
		return VCD_ERR_BAD_HANDLE;
	}
	if (!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_FRAME) &&
		!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_INITCODEC) &&
		!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_DPB) &&
		!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_FATAL_ERROR)
	    ) {
		VIDC_LOGERR_STRING("ddl_dec_end:Wrong_state");
		return VCD_ERR_ILLEGAL_OP;
	}
	DDL_BUSY(ddl_context);

	ddl_context->current_ddl = ddl;
	ddl_context->client_data = client_data;

	ddl_channel_end(ddl);
	return VCD_S_SUCCESS;
}

u32 ddl_encode_end(u32 *ddl_handle, void *client_data)
{
	struct ddl_client_context *ddl =
	    (struct ddl_client_context *)ddl_handle;
	struct ddl_context *ddl_context;

	ddl_context = ddl_get_context();

	if (vidc_msg_timing)
		ddl_reset_core_time_variables(ENC_OP_TIME);

	if (!DDL_IS_INITIALIZED(ddl_context)) {
		VIDC_LOGERR_STRING("ddl_enc_end:Not_inited");
		return VCD_ERR_ILLEGAL_OP;
	}
	if (DDL_IS_BUSY(ddl_context)) {
		VIDC_LOGERR_STRING("ddl_enc_end:Ddl_busy");
		return VCD_ERR_BUSY;
	}
	if (!ddl || ddl->decoding) {
		VIDC_LOGERR_STRING("ddl_enc_end:Bad_handle");
		return VCD_ERR_BAD_HANDLE;
	}
	if (!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_FRAME) &&
		!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_INITCODEC) &&
		!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_FATAL_ERROR)) {
		VIDC_LOGERR_STRING("ddl_enc_end:Wrong_state");
		return VCD_ERR_ILLEGAL_OP;
	}
	DDL_BUSY(ddl_context);

	ddl_context->current_ddl = ddl;
	ddl_context->client_data = client_data;

	ddl_channel_end(ddl);
	return VCD_S_SUCCESS;
}

u32 ddl_reset_hw(u32 mode)
{
	struct ddl_context *ddl_context;
	struct ddl_client_context *ddl;
	int i_client_num;

	VIDC_LOG_STRING("ddl_reset_hw:called");
	ddl_context = ddl_get_context();
	ddl_move_command_state(ddl_context, DDL_CMD_INVALID);
	DDL_BUSY(ddl_context);

	if (ddl_context->core_virtual_base_addr)
		vidc_720p_do_sw_reset();

	ddl_context->device_state = DDL_DEVICE_NOTINIT;
	for (i_client_num = 0; i_client_num < VCD_MAX_NO_CLIENT;
			++i_client_num) {
		ddl = ddl_context->ddl_clients[i_client_num];
		ddl_context->ddl_clients[i_client_num] = NULL;
		if (ddl) {
			ddl_release_client_internal_buffers(ddl);
			ddl_client_transact(DDL_FREE_CLIENT, &ddl);
		}
	}

	ddl_release_context_buffers(ddl_context);
	DDL_MEMSET(ddl_context, 0, sizeof(struct ddl_context));

	return true;
}
