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
#include "vcd_ddl_utils.h"

DDL_INLINE struct ddl_context *ddl_get_context(void)
{
	static struct ddl_context ddl_context;
	return &ddl_context;
}

DDL_INLINE void ddl_move_client_state(struct ddl_client_context *ddl,
				      enum ddl_client_state client_state)
{
	ddl->client_state = client_state;
}

DDL_INLINE void ddl_move_command_state(struct ddl_context *ddl_context,
				       enum ddl_cmd_state command_state)
{
	ddl_context->cmd_state = command_state;
}

u32 ddl_client_transact(u32 operation,
			struct ddl_client_context **pddl_client)
{
	u32 ret_status = VCD_ERR_FAIL;
	u32 counter;
	struct ddl_context *ddl_context;

	ddl_context = ddl_get_context();
	switch (operation) {
	case DDL_FREE_CLIENT:
		{
			if (pddl_client && *pddl_client) {
				u32 channel_id;
				channel_id = (*pddl_client)->channel_id;
				if (channel_id < VCD_MAX_NO_CLIENT) {
					ddl_context->
					    ddl_clients[channel_id] = NULL;
				} else {
					VIDC_LOG_STRING("CHID_CORRUPTION");
				}
				DDL_FREE(*pddl_client);
				ret_status = VCD_S_SUCCESS;
			}
			break;
		}
	case DDL_GET_CLIENT:
		{
			ret_status = VCD_ERR_MAX_CLIENT;
			for (counter = 0; counter < VCD_MAX_NO_CLIENT &&
			     ret_status == VCD_ERR_MAX_CLIENT; ++counter) {
				if (!ddl_context->ddl_clients[counter]) {
					*pddl_client =
					    (struct ddl_client_context *)
					    DDL_MALLOC(sizeof
					       (struct ddl_client_context)
					       );
					if (!*pddl_client) {
						ret_status = VCD_ERR_ALLOC_FAIL;
					} else {
						DDL_MEMSET(*pddl_client, 0,
						   sizeof(struct
						   ddl_client_context));
						ddl_context->
						    ddl_clients[counter] =
						    *pddl_client;
						(*pddl_client)->channel_id =
						    counter;
						(*pddl_client)->ddl_context =
						    ddl_context;
						ret_status = VCD_S_SUCCESS;
					}
				}
			}
			break;
		}
	case DDL_INIT_CLIENTS:
		{
			for (counter = 0; counter < VCD_MAX_NO_CLIENT;
			     ++counter) {
				ddl_context->ddl_clients[counter] = NULL;
			}
			ret_status = VCD_S_SUCCESS;
			break;
		}
	case DDL_ACTIVE_CLIENT:
		{
			for (counter = 0; counter < VCD_MAX_NO_CLIENT;
			     ++counter) {
				if (ddl_context->ddl_clients[counter]) {
					ret_status = VCD_S_SUCCESS;
					break;
				}
			}
			break;
		}
	default:
		{
			ret_status = VCD_ERR_ILLEGAL_PARM;
			break;
		}
	}
	return ret_status;
}

u32 ddl_decoder_dpb_transact(struct ddl_decoder_data *decoder,
			     struct ddl_frame_data_tag *in_out_frame,
			     u32 operation)
{
	u32 vcd_status = VCD_S_SUCCESS;
	u32 loopc;
	struct ddl_frame_data_tag *found_frame = NULL;
	struct ddl_mask *dpb_mask = &decoder->dpb_mask;

	switch (operation) {
	case DDL_DPB_OP_MARK_BUSY:
	case DDL_DPB_OP_MARK_FREE:
		{
			for (loopc = 0; !found_frame &&
			     loopc < decoder->dp_buf.no_of_dec_pic_buf;
			     ++loopc) {
				if (in_out_frame->vcd_frm.physical ==
				    decoder->dp_buf.
				    dec_pic_buffers[loopc].vcd_frm.
				    physical) {
					found_frame =
					    &(decoder->dp_buf.
					      dec_pic_buffers[loopc]);
					break;
				}
			}

			if (found_frame) {
				if (operation == DDL_DPB_OP_MARK_BUSY) {
					dpb_mask->hw_mask &=
					    (~(0x1 << loopc));
					*in_out_frame = *found_frame;
				} else if (operation ==
					DDL_DPB_OP_MARK_FREE) {
					dpb_mask->client_mask |=
					    (0x1 << loopc);
					*found_frame = *in_out_frame;
				}
			} else {
				in_out_frame->vcd_frm.physical = NULL;
				in_out_frame->vcd_frm.virtual = NULL;
				vcd_status = VCD_ERR_BAD_POINTER;
				VIDC_LOG_STRING("BUF_NOT_FOUND");
			}
			break;
		}
	case DDL_DPB_OP_SET_MASK:
		{
			dpb_mask->hw_mask |= dpb_mask->client_mask;
			dpb_mask->client_mask = 0;
			vidc_720p_decode_set_dpb_release_buffer_mask
			    (dpb_mask->hw_mask);
			break;
		}
	case DDL_DPB_OP_INIT:
		{
			u32 dpb_size;
			dpb_size = (!decoder->meta_data_offset) ?
			    decoder->dp_buf.dec_pic_buffers[0].vcd_frm.
			    alloc_len : decoder->meta_data_offset;
			vidc_720p_decode_set_dpb_details(decoder->dp_buf.
						  no_of_dec_pic_buf,
						  dpb_size,
						  decoder->ref_buffer.
						  align_physical_addr);
			for (loopc = 0;
			     loopc < decoder->dp_buf.no_of_dec_pic_buf;
			     ++loopc) {
				vidc_720p_decode_set_dpb_buffers(loopc,
							  (u32 *)
							  decoder->
							  dp_buf.
							  dec_pic_buffers
							  [loopc].
							  vcd_frm.
							  physical);
				VIDC_LOG1("DEC_DPB_BUFn_SIZE",
					   decoder->dp_buf.
					   dec_pic_buffers[loopc].vcd_frm.
					   alloc_len);
			}
			break;
		}
	case DDL_DPB_OP_RETRIEVE:
		{
			u32 position;
			if (dpb_mask->client_mask) {
				position = 0x1;
				for (loopc = 0;
				     loopc <
				     decoder->dp_buf.no_of_dec_pic_buf
				     && !found_frame; ++loopc) {
					if (dpb_mask->
					    client_mask & position) {
						found_frame =
						    &decoder->dp_buf.
						    dec_pic_buffers[loopc];
						dpb_mask->client_mask &=
						    ~(position);
					}
					position <<= 1;
				}
			} else if (dpb_mask->hw_mask) {
				position = 0x1;
				for (loopc = 0;
				     loopc <
				     decoder->dp_buf.no_of_dec_pic_buf
				     && !found_frame; ++loopc) {
					if (dpb_mask->hw_mask
							& position) {
						found_frame =
						    &decoder->dp_buf.
						    dec_pic_buffers[loopc];
						dpb_mask->hw_mask &=
						    ~(position);
					}
					position <<= 1;
				}
			}
			if (found_frame)
				*in_out_frame = *found_frame;
			else {
				in_out_frame->vcd_frm.physical = NULL;
				in_out_frame->vcd_frm.virtual = NULL;
			}
			break;
		}
	}
	return vcd_status;
}

void ddl_release_context_buffers(struct ddl_context *ddl_context)
{
	ddl_pmem_free(&ddl_context->context_buf_addr);
	ddl_pmem_free(&ddl_context->db_line_buffer);
	ddl_pmem_free(&ddl_context->data_partition_tempbuf);
	ddl_pmem_free(&ddl_context->metadata_shared_input);
	ddl_pmem_free(&ddl_context->dbg_core_dump);

	vcd_fw_release();
}

void ddl_release_client_internal_buffers(struct ddl_client_context *ddl)
{
	if (ddl->decoding) {
		struct ddl_decoder_data *decoder =
		    &(ddl->codec_data.decoder);
		ddl_pmem_free(&decoder->h264Vsp_temp_buffer);
		ddl_pmem_free(&decoder->dpb_comv_buffer);
		ddl_pmem_free(&decoder->ref_buffer);
		DDL_FREE(decoder->dp_buf.dec_pic_buffers);
		ddl_decode_dynamic_property(ddl, false);
		decoder->decode_config.sequence_header_len = 0;
		decoder->decode_config.sequence_header = NULL;
		decoder->dpb_mask.client_mask = 0;
		decoder->dpb_mask.hw_mask = 0;
		decoder->dp_buf.no_of_dec_pic_buf = 0;
		decoder->dynamic_prop_change = 0;

	} else {
		struct ddl_encoder_data *encoder =
		    &(ddl->codec_data.encoder);
		ddl_pmem_free(&encoder->enc_dpb_addr);
		ddl_pmem_free(&encoder->seq_header);
		ddl_encode_dynamic_property(ddl, false);
		encoder->dynamic_prop_change = 0;
	}
}
