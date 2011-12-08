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
#include <mach/msm_memtypes.h>
#include "vcd_ddl.h"
#include "vcd_ddl_metadata.h"
#include "vcd_res_tracker_api.h"

static unsigned int first_time;

u32 ddl_device_init(struct ddl_init_config *ddl_init_config,
	void *client_data)
{
	struct ddl_context *ddl_context;
	u32 status = VCD_S_SUCCESS;
	void *ptr = NULL;
	DDL_MSG_HIGH("ddl_device_init");

	if ((!ddl_init_config) || (!ddl_init_config->ddl_callback) ||
		(!ddl_init_config->core_virtual_base_addr)) {
		DDL_MSG_ERROR("ddl_dev_init:Bad_argument");
		return VCD_ERR_ILLEGAL_PARM;
	}
	ddl_context = ddl_get_context();
	if (DDL_IS_INITIALIZED(ddl_context)) {
		DDL_MSG_ERROR("ddl_dev_init:Multiple_init");
		return VCD_ERR_ILLEGAL_OP;
	}
	if (!DDL_IS_IDLE(ddl_context)) {
		DDL_MSG_ERROR("ddl_dev_init:Ddl_busy");
		return VCD_ERR_BUSY;
	}
	memset(ddl_context, 0, sizeof(struct ddl_context));
	DDL_BUSY(ddl_context);
	if (res_trk_get_enable_ion()) {
		DDL_MSG_LOW("ddl_dev_init:ION framework enabled");
		ddl_context->video_ion_client  =
			res_trk_get_ion_client();
		if (!ddl_context->video_ion_client) {
			DDL_MSG_ERROR("ION client create failed");
			return VCD_ERR_ILLEGAL_OP;
		}
	}
	ddl_context->ddl_callback = ddl_init_config->ddl_callback;
	if (ddl_init_config->interrupt_clr)
		ddl_context->interrupt_clr =
			ddl_init_config->interrupt_clr;
	ddl_context->core_virtual_base_addr =
		ddl_init_config->core_virtual_base_addr;
	ddl_context->client_data = client_data;
	ddl_context->ddl_hw_response.arg1 = DDL_INVALID_INTR_STATUS;

	ddl_context->frame_channel_depth = VCD_FRAME_COMMAND_DEPTH;

	DDL_MSG_LOW("%s() : virtual address of core(%x)\n", __func__,
		(u32) ddl_init_config->core_virtual_base_addr);
	vidc_1080p_set_device_base_addr(
		ddl_context->core_virtual_base_addr);
	ddl_context->cmd_state =	DDL_CMD_INVALID;
	ddl_client_transact(DDL_INIT_CLIENTS, NULL);
	ddl_context->fw_memory_size =
		DDL_FW_INST_GLOBAL_CONTEXT_SPACE_SIZE;
	if (res_trk_get_firmware_addr(&ddl_context->dram_base_a)) {
		DDL_MSG_ERROR("firmware allocation failed");
		ptr = NULL;
	} else {
		ptr = (void *)ddl_context->dram_base_a.virtual_base_addr;
	}
	if (!ptr) {
		DDL_MSG_ERROR("Memory Aocation Failed for FW Base");
		status = VCD_ERR_ALLOC_FAIL;
	} else {
		DDL_MSG_LOW("%s() : physical address of base(%x)\n",
			 __func__, (u32) ddl_context->dram_base_a.\
			align_physical_addr);
		ddl_context->dram_base_b.align_physical_addr =
			ddl_context->dram_base_a.align_physical_addr;
		ddl_context->dram_base_b.align_virtual_addr  =
			ddl_context->dram_base_a.align_virtual_addr;
	}
	if (!status) {
		ddl_context->metadata_shared_input.mem_type = DDL_MM_MEM;
		ptr = ddl_pmem_alloc(&ddl_context->metadata_shared_input,
			DDL_METADATA_TOTAL_INPUTBUFSIZE,
			DDL_LINEAR_BUFFER_ALIGN_BYTES);
		if (!ptr) {
			DDL_MSG_ERROR("ddl_device_init: metadata alloc fail");
			status = VCD_ERR_ALLOC_FAIL;
		}
	}
	if (!status && !ddl_fw_init(&ddl_context->dram_base_a)) {
		DDL_MSG_ERROR("ddl_dev_init:fw_init_failed");
		status = VCD_ERR_ALLOC_FAIL;
	}
	if (!status) {
		ddl_context->cmd_state = DDL_CMD_DMA_INIT;
		ddl_vidc_core_init(ddl_context);
	} else {
		ddl_release_context_buffers(ddl_context);
		DDL_IDLE(ddl_context);
	}
	return status;
}

u32 ddl_device_release(void *client_data)
{
	struct ddl_context *ddl_context;

	DDL_MSG_HIGH("ddl_device_release");
	ddl_context = ddl_get_context();
	if (!DDL_IS_IDLE(ddl_context)) {
		DDL_MSG_ERROR("ddl_dev_rel:Ddl_busy");
		return VCD_ERR_BUSY;
	}
	if (!DDL_IS_INITIALIZED(ddl_context)) {
		DDL_MSG_ERROR("ddl_dev_rel:Not_inited");
		return VCD_ERR_ILLEGAL_OP;
	}
	if (!ddl_client_transact(DDL_ACTIVE_CLIENT, NULL)) {
		DDL_MSG_ERROR("ddl_dev_rel:Client_present_err");
		return VCD_ERR_CLIENT_PRESENT;
	}
	DDL_BUSY(ddl_context);
	ddl_context->device_state = DDL_DEVICE_NOTINIT;
	ddl_context->client_data = client_data;
	ddl_context->cmd_state = DDL_CMD_INVALID;
	ddl_vidc_core_term(ddl_context);
	DDL_MSG_LOW("FW_ENDDONE");
	ddl_context->core_virtual_base_addr = NULL;
	ddl_release_context_buffers(ddl_context);
	ddl_context->video_ion_client = NULL;
	DDL_IDLE(ddl_context);
	return VCD_S_SUCCESS;
}

u32 ddl_open(u32 **ddl_handle, u32 decoding)
{
	struct ddl_context *ddl_context;
	struct ddl_client_context *ddl;
	void *ptr;
	u32 status;

	DDL_MSG_HIGH("ddl_open");
	if (!ddl_handle) {
		DDL_MSG_ERROR("ddl_open:Bad_handle");
		return VCD_ERR_BAD_HANDLE;
	}
	ddl_context = ddl_get_context();
	if (!DDL_IS_INITIALIZED(ddl_context)) {
		DDL_MSG_ERROR("ddl_open:Not_inited");
		return VCD_ERR_ILLEGAL_OP;
	}
	status = ddl_client_transact(DDL_GET_CLIENT, &ddl);
	if (status) {
		DDL_MSG_ERROR("ddl_open:Client_trasac_failed");
		return status;
	}
	ddl->shared_mem[0].mem_type = DDL_CMD_MEM;
	ptr = ddl_pmem_alloc(&ddl->shared_mem[0],
			DDL_FW_AUX_HOST_CMD_SPACE_SIZE, 0);
	if (!ptr)
		status = VCD_ERR_ALLOC_FAIL;
	if (!status && ddl_context->frame_channel_depth
		== VCD_DUAL_FRAME_COMMAND_CHANNEL) {
		ddl->shared_mem[1].mem_type = DDL_CMD_MEM;
		ptr = ddl_pmem_alloc(&ddl->shared_mem[1],
				DDL_FW_AUX_HOST_CMD_SPACE_SIZE, 0);
		if (!ptr) {
			ddl_pmem_free(&ddl->shared_mem[0]);
			status = VCD_ERR_ALLOC_FAIL;
		}
	}
	if (!status) {
		memset(ddl->shared_mem[0].align_virtual_addr, 0,
			DDL_FW_AUX_HOST_CMD_SPACE_SIZE);
		if (ddl_context->frame_channel_depth ==
			VCD_DUAL_FRAME_COMMAND_CHANNEL) {
			memset(ddl->shared_mem[1].align_virtual_addr, 0,
				DDL_FW_AUX_HOST_CMD_SPACE_SIZE);
		}
		DDL_MSG_LOW("ddl_state_transition: %s ~~> DDL_CLIENT_OPEN",
		ddl_get_state_string(ddl->client_state));
		ddl->client_state = DDL_CLIENT_OPEN;
		ddl->codec_data.hdr.decoding = decoding;
		ddl->decoding = decoding;
		ddl_set_default_meta_data_hdr(ddl);
		ddl_set_initial_default_values(ddl);
		*ddl_handle	= (u32 *) ddl;
	} else {
		ddl_pmem_free(&ddl->shared_mem[0]);
		if (ddl_context->frame_channel_depth
			== VCD_DUAL_FRAME_COMMAND_CHANNEL)
			ddl_pmem_free(&ddl->shared_mem[1]);
		ddl_client_transact(DDL_FREE_CLIENT, &ddl);
	}
	return status;
}

u32 ddl_close(u32 **ddl_handle)
{
	struct ddl_context *ddl_context;
	struct ddl_client_context **pp_ddl =
		(struct ddl_client_context **)ddl_handle;

	DDL_MSG_HIGH("ddl_close");
	if (!pp_ddl || !*pp_ddl) {
		DDL_MSG_ERROR("ddl_close:Bad_handle");
		return VCD_ERR_BAD_HANDLE;
	}
	ddl_context = ddl_get_context();
	if (!DDL_IS_INITIALIZED(ddl_context)) {
		DDL_MSG_ERROR("ddl_close:Not_inited");
		return VCD_ERR_ILLEGAL_OP;
	}
	if (!DDLCLIENT_STATE_IS(*pp_ddl, DDL_CLIENT_OPEN)) {
		DDL_MSG_ERROR("ddl_close:Not_in_open_state");
		return VCD_ERR_ILLEGAL_OP;
	}
	ddl_pmem_free(&(*pp_ddl)->shared_mem[0]);
	if (ddl_context->frame_channel_depth ==
		VCD_DUAL_FRAME_COMMAND_CHANNEL)
		ddl_pmem_free(&(*pp_ddl)->shared_mem[1]);
	DDL_MSG_LOW("ddl_state_transition: %s ~~> DDL_CLIENT_INVALID",
	ddl_get_state_string((*pp_ddl)->client_state));
	(*pp_ddl)->client_state = DDL_CLIENT_INVALID;
	ddl_codec_type_transact(*pp_ddl, true, (enum vcd_codec)0);
	ddl_client_transact(DDL_FREE_CLIENT, pp_ddl);
	return VCD_S_SUCCESS;
}

u32 ddl_encode_start(u32 *ddl_handle, void *client_data)
{
	struct ddl_client_context *ddl =
		(struct ddl_client_context *) ddl_handle;
	struct ddl_context *ddl_context;
	struct ddl_encoder_data *encoder;
	void *ptr;
	u32 status = VCD_S_SUCCESS;
	DDL_MSG_HIGH("ddl_encode_start");
	if (vidc_msg_timing) {
		if (first_time < 2) {
			ddl_reset_core_time_variables(ENC_OP_TIME);
			first_time++;
		 }
		ddl_set_core_start_time(__func__, ENC_OP_TIME);
	}
	ddl_context = ddl_get_context();
	if (!DDL_IS_INITIALIZED(ddl_context)) {
		DDL_MSG_ERROR("ddl_enc_start:Not_inited");
		return VCD_ERR_ILLEGAL_OP;
	}
	if (DDL_IS_BUSY(ddl_context)) {
		DDL_MSG_ERROR("ddl_enc_start:Ddl_busy");
		return VCD_ERR_BUSY;
	}
	if (!ddl || ddl->decoding) {
		DDL_MSG_ERROR("ddl_enc_start:Bad_handle");
		return VCD_ERR_BAD_HANDLE;
	}
	if (!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_OPEN)) {
		DDL_MSG_ERROR("ddl_enc_start:Not_opened");
		return VCD_ERR_ILLEGAL_OP;
	}
	if (!ddl_encoder_ready_to_start(ddl)) {
		DDL_MSG_ERROR("ddl_enc_start:Err_param_settings");
		return VCD_ERR_ILLEGAL_OP;
	}
	encoder = &ddl->codec_data.encoder;
	status = ddl_allocate_enc_hw_buffers(ddl);
	if (status)
		return status;
#ifdef DDL_BUF_LOG
	ddl_list_buffers(ddl);
#endif
	encoder->seq_header.mem_type = DDL_MM_MEM;
	ptr = ddl_pmem_alloc(&encoder->seq_header,
		DDL_ENC_SEQHEADER_SIZE, DDL_LINEAR_BUFFER_ALIGN_BYTES);
	if (!ptr) {
		ddl_free_enc_hw_buffers(ddl);
		DDL_MSG_ERROR("ddl_enc_start:Seq_hdr_alloc_failed");
		return VCD_ERR_ALLOC_FAIL;
	}
	if (!ddl_take_command_channel(ddl_context, ddl, client_data))
		return VCD_ERR_BUSY;
	ddl_vidc_channel_set(ddl);
	return status;
}

u32 ddl_decode_start(u32 *ddl_handle, struct vcd_sequence_hdr *header,
	void *client_data)
{
	struct ddl_client_context  *ddl =
		(struct ddl_client_context *) ddl_handle;
	struct ddl_context *ddl_context;
	struct ddl_decoder_data *decoder;
	u32 status = VCD_S_SUCCESS;

	DDL_MSG_HIGH("ddl_decode_start");
	if (vidc_msg_timing) {
		ddl_reset_core_time_variables(DEC_OP_TIME);
		ddl_reset_core_time_variables(DEC_IP_TIME);
	}
	ddl_context = ddl_get_context();
	if (!DDL_IS_INITIALIZED(ddl_context)) {
		DDL_MSG_ERROR("ddl_dec_start:Not_inited");
		return VCD_ERR_ILLEGAL_OP;
	}
	if (DDL_IS_BUSY(ddl_context)) {
		DDL_MSG_ERROR("ddl_dec_start:Ddl_busy");
		return VCD_ERR_BUSY;
	}
	if (!ddl || !ddl->decoding) {
		DDL_MSG_ERROR("ddl_dec_start:Bad_handle");
		return VCD_ERR_BAD_HANDLE;
	}
	if (!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_OPEN)) {
		DDL_MSG_ERROR("ddl_dec_start:Not_in_opened_state");
		return VCD_ERR_ILLEGAL_OP;
	}

	if ((header) && ((!header->sequence_header_len) ||
		(!header->sequence_header))) {
		DDL_MSG_ERROR("ddl_dec_start:Bad_param_seq_header");
		return VCD_ERR_ILLEGAL_PARM;
	}
	if (!ddl_decoder_ready_to_start(ddl, header)) {
		DDL_MSG_ERROR("ddl_dec_start:Err_param_settings");
		return VCD_ERR_ILLEGAL_OP;
	}
	decoder = &ddl->codec_data.decoder;
	status = ddl_allocate_dec_hw_buffers(ddl);
	if (status)
		return status;
#ifdef DDL_BUF_LOG
	ddl_list_buffers(ddl);
#endif
	if (!ddl_take_command_channel(ddl_context, ddl, client_data))
		return VCD_ERR_BUSY;
	if (header) {
		decoder->header_in_start = true;
		decoder->decode_config = *header;
	} else {
		decoder->header_in_start = false;
		decoder->decode_config.sequence_header_len = 0;
	}
	ddl_vidc_channel_set(ddl);
	return status;
}

u32 ddl_decode_frame(u32 *ddl_handle,
	struct ddl_frame_data_tag *input_bits, void *client_data)
{
	u32 vcd_status = VCD_S_SUCCESS;
	struct ddl_client_context *ddl =
		(struct ddl_client_context *) ddl_handle;
	struct ddl_context *ddl_context;
	struct ddl_decoder_data *decoder;
	DDL_MSG_HIGH("ddl_decode_frame");
	ddl_context = ddl_get_context();
	if (!DDL_IS_INITIALIZED(ddl_context)) {
		DDL_MSG_ERROR("ddl_dec_frame:Not_inited");
		return VCD_ERR_ILLEGAL_OP;
	}
	if (DDL_IS_BUSY(ddl_context)) {
		DDL_MSG_ERROR("ddl_dec_frame:Ddl_busy");
		return VCD_ERR_BUSY;
	}
	if (!ddl || !ddl->decoding) {
		DDL_MSG_ERROR("ddl_dec_frame:Bad_handle");
		return VCD_ERR_BAD_HANDLE;
	}
	if (!input_bits || ((!input_bits->vcd_frm.physical ||
		!input_bits->vcd_frm.data_len) &&
		(!(VCD_FRAME_FLAG_EOS &	input_bits->vcd_frm.flags)))) {
		DDL_MSG_ERROR("ddl_dec_frame:Bad_input_param");
		return VCD_ERR_ILLEGAL_PARM;
	}
	if (!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_FRAME) &&
		!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_INITCODEC) &&
		!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_DPB)) {
		DDL_MSG_ERROR("Dec_frame:Wrong_state");
		return VCD_ERR_ILLEGAL_OP;
	}
	decoder = &(ddl->codec_data.decoder);
	if (DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_INITCODEC)	&&
		!ddl->codec_data.decoder.dp_buf.no_of_dec_pic_buf) {
		DDL_MSG_ERROR("ddl_dec_frame:Dpbs_requied");
		return VCD_ERR_ILLEGAL_OP;
	}
	if (!ddl_take_command_channel(ddl_context, ddl, client_data))
		return VCD_ERR_BUSY;

	ddl->input_frame = *input_bits;
	if (DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_FRAME))
		ddl_vidc_decode_frame_run(ddl);
	else {
		if (!ddl->codec_data.decoder.dp_buf.no_of_dec_pic_buf) {
			DDL_MSG_ERROR("ddl_dec_frame:Dpbs_requied");
			vcd_status = VCD_ERR_ILLEGAL_OP;
		} else if (DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_DPB)) {
			vcd_status = ddl_vidc_decode_set_buffers(ddl);
		if (vcd_status)
			ddl_release_command_channel(ddl_context,
				ddl->command_channel);
		} else if (DDLCLIENT_STATE_IS(ddl,
			DDL_CLIENT_WAIT_FOR_INITCODEC)) {
			if (decoder->codec.codec == VCD_CODEC_DIVX_3) {
				if ((!decoder->client_frame_size.width) ||
				(!decoder->client_frame_size.height))
					return VCD_ERR_ILLEGAL_OP;
		}
		ddl->codec_data.decoder.decode_config.sequence_header =
			ddl->input_frame.vcd_frm.physical;
		ddl->codec_data.decoder.decode_config.sequence_header_len =
			ddl->input_frame.vcd_frm.data_len;
		ddl_vidc_decode_init_codec(ddl);
		} else {
			DDL_MSG_ERROR("Dec_frame:Wrong_state");
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
		(struct ddl_client_context *) ddl_handle;
	struct ddl_context *ddl_context;
	u32 vcd_status = VCD_S_SUCCESS;

	DDL_MSG_HIGH("ddl_encode_frame");
	if (vidc_msg_timing)
		ddl_set_core_start_time(__func__, ENC_OP_TIME);
	ddl_context = ddl_get_context();
	if (!DDL_IS_INITIALIZED(ddl_context)) {
		DDL_MSG_ERROR("ddl_enc_frame:Not_inited");
		return VCD_ERR_ILLEGAL_OP;
	}
	if (DDL_IS_BUSY(ddl_context)) {
		DDL_MSG_ERROR("ddl_enc_frame:Ddl_busy");
		return VCD_ERR_BUSY;
	}
	if (!ddl || ddl->decoding) {
		DDL_MSG_ERROR("ddl_enc_frame:Bad_handle");
	return VCD_ERR_BAD_HANDLE;
	}
	if (!input_frame || !input_frame->vcd_frm.physical	||
		!input_frame->vcd_frm.data_len) {
		DDL_MSG_ERROR("ddl_enc_frame:Bad_input_params");
		return VCD_ERR_ILLEGAL_PARM;
	}
	if ((((u32) input_frame->vcd_frm.physical +
		input_frame->vcd_frm.offset) &
		(DDL_STREAMBUF_ALIGN_GUARD_BYTES))) {
		DDL_MSG_ERROR("ddl_enc_frame:Un_aligned_yuv_start_address");
		return VCD_ERR_ILLEGAL_PARM;
	}
	if (!output_bit || !output_bit->vcd_frm.physical ||
		!output_bit->vcd_frm.alloc_len) {
		DDL_MSG_ERROR("ddl_enc_frame:Bad_output_params");
		return VCD_ERR_ILLEGAL_PARM;
	}
	if ((ddl->codec_data.encoder.output_buf_req.sz +
		output_bit->vcd_frm.offset) >
		output_bit->vcd_frm.alloc_len)
		DDL_MSG_ERROR("ddl_enc_frame:offset_large,"
			"Exceeds_min_buf_size");
	if (!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_FRAME)) {
		DDL_MSG_ERROR("ddl_enc_frame:Wrong_state");
		return VCD_ERR_ILLEGAL_OP;
	}
	if (!ddl_take_command_channel(ddl_context, ddl, client_data))
		return VCD_ERR_BUSY;

	ddl->input_frame = *input_frame;
	ddl->output_frame = *output_bit;
	if (ddl->codec_data.encoder.i_period.b_frames > 0) {
		if (!ddl->b_count) {
			ddl->first_output_frame = *output_bit;
			ddl->b_count++;
		} else if (ddl->codec_data.encoder.i_period.b_frames >=
			ddl->b_count) {
			ddl->extra_output_frame[ddl->b_count-1] =
				*output_bit;
			ddl->output_frame = ddl->first_output_frame;
			ddl->b_count++;
		}
	}
	ddl_insert_input_frame_to_pool(ddl, input_frame);
	if (!vcd_status)
		ddl_vidc_encode_frame_run(ddl);
	else
		DDL_MSG_ERROR("insert to frame pool failed %u", vcd_status);
	return vcd_status;
}

u32 ddl_decode_end(u32 *ddl_handle, void *client_data)
{
	struct ddl_client_context *ddl =
		(struct ddl_client_context *) ddl_handle;
	struct ddl_context *ddl_context;

	DDL_MSG_HIGH("ddl_decode_end");
	if (vidc_msg_timing) {
		ddl_reset_core_time_variables(DEC_OP_TIME);
		ddl_reset_core_time_variables(DEC_IP_TIME);
	}
	ddl_context = ddl_get_context();
	if (!DDL_IS_INITIALIZED(ddl_context)) {
		DDL_MSG_ERROR("ddl_dec_end:Not_inited");
		return VCD_ERR_ILLEGAL_OP;
	}
	if (DDL_IS_BUSY(ddl_context)) {
		DDL_MSG_ERROR("ddl_dec_end:Ddl_busy");
		return VCD_ERR_BUSY;
	}
	if (!ddl || !ddl->decoding) {
		DDL_MSG_ERROR("ddl_dec_end:Bad_handle");
		return VCD_ERR_BAD_HANDLE;
	}
	if (!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_FRAME) &&
		!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_INITCODEC) &&
		!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_DPB) &&
		!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_FAVIDC_ERROR)) {
		DDL_MSG_ERROR("ddl_dec_end:Wrong_state");
		return VCD_ERR_ILLEGAL_OP;
	}
	if (!ddl_take_command_channel(ddl_context, ddl, client_data))
		return VCD_ERR_BUSY;
	ddl_vidc_channel_end(ddl);
	return VCD_S_SUCCESS;
}

u32 ddl_encode_end(u32 *ddl_handle, void *client_data)
{
	struct ddl_client_context  *ddl =
		(struct ddl_client_context *) ddl_handle;
	struct ddl_context *ddl_context;

	DDL_MSG_HIGH("ddl_encode_end");
	if (vidc_msg_timing)
		ddl_reset_core_time_variables(ENC_OP_TIME);
	ddl_context = ddl_get_context();
	if (!DDL_IS_INITIALIZED(ddl_context)) {
		DDL_MSG_ERROR("ddl_enc_end:Not_inited");
		return VCD_ERR_ILLEGAL_OP;
	}
	if (DDL_IS_BUSY(ddl_context)) {
		DDL_MSG_ERROR("ddl_enc_end:Ddl_busy");
		return VCD_ERR_BUSY;
	}
	if (!ddl || ddl->decoding) {
		DDL_MSG_ERROR("ddl_enc_end:Bad_handle");
		return VCD_ERR_BAD_HANDLE;
	}
	if (!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_FRAME) &&
		!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_WAIT_FOR_INITCODEC) &&
		!DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_FAVIDC_ERROR)) {
		DDL_MSG_ERROR("ddl_enc_end:Wrong_state");
		return VCD_ERR_ILLEGAL_OP;
	}
	if (!ddl_take_command_channel(ddl_context, ddl, client_data))
		return VCD_ERR_BUSY;
	ddl_vidc_channel_end(ddl);
	return VCD_S_SUCCESS;
}

u32 ddl_reset_hw(u32 mode)
{
	struct ddl_context *ddl_context;
	struct ddl_client_context *ddl;
	u32 i;

	DDL_MSG_HIGH("ddl_reset_hw");
	DDL_MSG_LOW("ddl_reset_hw:called");
	ddl_context = ddl_get_context();
	ddl_context->cmd_state = DDL_CMD_INVALID;
	DDL_BUSY(ddl_context);
	if (ddl_context->core_virtual_base_addr) {
		vidc_1080p_do_sw_reset(VIDC_1080P_RESET_IN_SEQ_FIRST_STAGE);
		msleep(DDL_SW_RESET_SLEEP);
		vidc_1080p_do_sw_reset(VIDC_1080P_RESET_IN_SEQ_SECOND_STAGE);
		msleep(DDL_SW_RESET_SLEEP);
		ddl_context->core_virtual_base_addr = NULL;
	}
	ddl_context->device_state = DDL_DEVICE_NOTINIT;
	for (i = 0; i < VCD_MAX_NO_CLIENT; i++) {
		ddl = ddl_context->ddl_clients[i];
		ddl_context->ddl_clients[i] = NULL;
		if (ddl) {
			ddl_release_client_internal_buffers(ddl);
			ddl_client_transact(DDL_FREE_CLIENT, &ddl);
		}
	}
	ddl_release_context_buffers(ddl_context);
	memset(ddl_context, 0, sizeof(struct ddl_context));
	return true;
}
