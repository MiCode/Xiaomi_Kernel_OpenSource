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

static u32 *ddl_metadata_hdr_entry(struct ddl_client_context *ddl,
				   u32 meta_data)
{
	u32 skip_words = 0;
	u32 *buffer;

	if (ddl->decoding) {
		buffer = (u32 *)
		    ddl->codec_data.decoder.meta_data_input.
		    align_virtual_addr;
		skip_words = 32 + 1;
		buffer += skip_words;

		switch (meta_data) {
		default:
		case VCD_METADATA_DATANONE:
			{
				skip_words = 0;
				break;
			}
		case VCD_METADATA_QPARRAY:
			{
				skip_words = 3;
				break;
			}
		case VCD_METADATA_CONCEALMB:
			{
				skip_words = 6;
				break;
			}
		case VCD_METADATA_VC1:
			{
				skip_words = 9;
				break;
			}
		case VCD_METADATA_SEI:
			{
				skip_words = 12;
				break;
			}
		case VCD_METADATA_VUI:
			{
				skip_words = 15;
				break;
			}
		case VCD_METADATA_PASSTHROUGH:
			{
				skip_words = 18;
				break;
			}
		case VCD_METADATA_QCOMFILLER:
			{
				skip_words = 21;
				break;
			}
		}
	} else {
		buffer = (u32 *)
		    ddl->codec_data.encoder.meta_data_input.
		    align_virtual_addr;
		skip_words = 2;
		buffer += skip_words;

		switch (meta_data) {
		default:
		case VCD_METADATA_DATANONE:
			{
				skip_words = 0;
				break;
			}
		case VCD_METADATA_ENC_SLICE:
			{
				skip_words = 3;
				break;
			}
		case VCD_METADATA_QCOMFILLER:
			{
				skip_words = 6;
				break;
			}
		}

	}

	buffer += skip_words;
	return buffer;
}

void ddl_set_default_meta_data_hdr(struct ddl_client_context *ddl)
{
	struct ddl_buf_addr *main_buffer =
	    &ddl->ddl_context->metadata_shared_input;
	struct ddl_buf_addr *client_buffer;
	u32 *hdr_entry;

	if (ddl->decoding)
		client_buffer = &(ddl->codec_data.decoder.meta_data_input);
	else
		client_buffer = &(ddl->codec_data.encoder.meta_data_input);

	DDL_METADATA_CLIENT_INPUTBUF(main_buffer, client_buffer,
				     ddl->channel_id);

	hdr_entry = ddl_metadata_hdr_entry(ddl, VCD_METADATA_QCOMFILLER);
	hdr_entry[DDL_METADATA_HDR_VERSION_INDEX] = 0x00000101;
	hdr_entry[DDL_METADATA_HDR_PORT_INDEX] = 1;
	hdr_entry[DDL_METADATA_HDR_TYPE_INDEX] = VCD_METADATA_QCOMFILLER;

	hdr_entry = ddl_metadata_hdr_entry(ddl, VCD_METADATA_DATANONE);
	hdr_entry[DDL_METADATA_HDR_VERSION_INDEX] = 0x00000101;
	hdr_entry[DDL_METADATA_HDR_PORT_INDEX] = 1;
	hdr_entry[DDL_METADATA_HDR_TYPE_INDEX] = VCD_METADATA_DATANONE;

	if (ddl->decoding) {
		hdr_entry =
		    ddl_metadata_hdr_entry(ddl, VCD_METADATA_QPARRAY);
		hdr_entry[DDL_METADATA_HDR_VERSION_INDEX] = 0x00000101;
		hdr_entry[DDL_METADATA_HDR_PORT_INDEX] = 1;
		hdr_entry[DDL_METADATA_HDR_TYPE_INDEX] = VCD_METADATA_QPARRAY;

		hdr_entry =
		    ddl_metadata_hdr_entry(ddl, VCD_METADATA_CONCEALMB);
		hdr_entry[DDL_METADATA_HDR_VERSION_INDEX] = 0x00000101;
		hdr_entry[DDL_METADATA_HDR_PORT_INDEX] = 1;
		hdr_entry[DDL_METADATA_HDR_TYPE_INDEX] = VCD_METADATA_CONCEALMB;

		hdr_entry = ddl_metadata_hdr_entry(ddl, VCD_METADATA_SEI);
		hdr_entry[DDL_METADATA_HDR_VERSION_INDEX] = 0x00000101;
		hdr_entry[DDL_METADATA_HDR_PORT_INDEX] = 1;
		hdr_entry[DDL_METADATA_HDR_TYPE_INDEX] = VCD_METADATA_SEI;

		hdr_entry = ddl_metadata_hdr_entry(ddl, VCD_METADATA_VUI);
		hdr_entry[DDL_METADATA_HDR_VERSION_INDEX] = 0x00000101;
		hdr_entry[DDL_METADATA_HDR_PORT_INDEX] = 1;
		hdr_entry[DDL_METADATA_HDR_TYPE_INDEX] = VCD_METADATA_VUI;

		hdr_entry = ddl_metadata_hdr_entry(ddl, VCD_METADATA_VC1);
		hdr_entry[DDL_METADATA_HDR_VERSION_INDEX] = 0x00000101;
		hdr_entry[DDL_METADATA_HDR_PORT_INDEX] = 1;
		hdr_entry[DDL_METADATA_HDR_TYPE_INDEX] = VCD_METADATA_VC1;

		hdr_entry =
		    ddl_metadata_hdr_entry(ddl, VCD_METADATA_PASSTHROUGH);
		hdr_entry[DDL_METADATA_HDR_VERSION_INDEX] = 0x00000101;
		hdr_entry[DDL_METADATA_HDR_PORT_INDEX] = 1;
		hdr_entry[DDL_METADATA_HDR_TYPE_INDEX] =
		    VCD_METADATA_PASSTHROUGH;

	} else {
		hdr_entry =
		    ddl_metadata_hdr_entry(ddl, VCD_METADATA_ENC_SLICE);
		hdr_entry[DDL_METADATA_HDR_VERSION_INDEX] = 0x00000101;
		hdr_entry[DDL_METADATA_HDR_PORT_INDEX] = 1;
		hdr_entry[DDL_METADATA_HDR_TYPE_INDEX] =
		    VCD_METADATA_ENC_SLICE;
	}
}

static u32 ddl_supported_metadata_flag(struct ddl_client_context *ddl)
{
	u32 flag = 0;

	if (ddl->decoding) {
		enum vcd_codec codec =
		    ddl->codec_data.decoder.codec.codec;

		flag |= (VCD_METADATA_CONCEALMB |
			   VCD_METADATA_PASSTHROUGH | VCD_METADATA_QPARRAY);
		if (codec == VCD_CODEC_H264) {
			flag |= (VCD_METADATA_SEI | VCD_METADATA_VUI);
		} else if (codec == VCD_CODEC_VC1 ||
			   codec == VCD_CODEC_VC1_RCV) {
			flag |= VCD_METADATA_VC1;
		}
	} else {
		flag |= VCD_METADATA_ENC_SLICE;
	}

	return flag;
}

void ddl_set_default_metadata_flag(struct ddl_client_context *ddl)
{
	if (ddl->decoding)
		ddl->codec_data.decoder.meta_data_enable_flag = 0;
	else
		ddl->codec_data.encoder.meta_data_enable_flag = 0;
}

void ddl_set_default_decoder_metadata_buffer_size(
	struct ddl_decoder_data *decoder,
	struct vcd_property_frame_size *frame_size,
	struct vcd_buffer_requirement *output_buf_req)
{
	u32 flag = decoder->meta_data_enable_flag;
	u32 suffix = 0;
	size_t sz = 0;

	if (!flag) {
		decoder->suffix = 0;
		return;
	}

	if (flag & VCD_METADATA_QPARRAY) {
		u32 num_of_mb =
		    ((frame_size->width * frame_size->height) >> 8);
		sz = DDL_METADATA_HDR_SIZE;
		sz += num_of_mb;
		DDL_METADATA_ALIGNSIZE(sz);
		suffix += sz;
	}
	if (flag & VCD_METADATA_CONCEALMB) {
		u32 num_of_mb =
		    ((frame_size->width * frame_size->height) >> 8);
		sz = DDL_METADATA_HDR_SIZE + (num_of_mb >> 3);
		DDL_METADATA_ALIGNSIZE(sz);
		suffix += sz;
	}
	if (flag & VCD_METADATA_VC1) {
		sz = DDL_METADATA_HDR_SIZE;
		sz += DDL_METADATA_VC1_PAYLOAD_SIZE;
		DDL_METADATA_ALIGNSIZE(sz);
		suffix += sz;
	}
	if (flag & VCD_METADATA_SEI) {
		sz = DDL_METADATA_HDR_SIZE;
		sz += DDL_METADATA_SEI_PAYLOAD_SIZE;
		DDL_METADATA_ALIGNSIZE(sz);
		suffix += (sz * DDL_METADATA_SEI_MAX);
	}
	if (flag & VCD_METADATA_VUI) {
		sz = DDL_METADATA_HDR_SIZE;
		sz += DDL_METADATA_VUI_PAYLOAD_SIZE;
		DDL_METADATA_ALIGNSIZE(sz);
		suffix += (sz);
	}
	if (flag & VCD_METADATA_PASSTHROUGH) {
		sz = DDL_METADATA_HDR_SIZE;
		sz += DDL_METADATA_PASSTHROUGH_PAYLOAD_SIZE;
		DDL_METADATA_ALIGNSIZE(sz);
		suffix += (sz);
	}
	sz = DDL_METADATA_EXTRADATANONE_SIZE;
	DDL_METADATA_ALIGNSIZE(sz);
	suffix += (sz);

	suffix += DDL_METADATA_EXTRAPAD_SIZE;
	DDL_METADATA_ALIGNSIZE(suffix);

	decoder->suffix = suffix;
	output_buf_req->sz += suffix;
	return;
}

void ddl_set_default_encoder_metadata_buffer_size(struct ddl_encoder_data
						  *encoder)
{
	u32 flag = encoder->meta_data_enable_flag;
	u32 suffix = 0;
	size_t sz = 0;

	if (!flag) {
		encoder->suffix = 0;
		return;
	}

	if (flag & VCD_METADATA_ENC_SLICE) {
		u32 num_of_mb = (encoder->frame_size.width *
				   encoder->frame_size.height / 16 / 16);
		sz = DDL_METADATA_HDR_SIZE;

		sz += 4;

		sz += (8 * num_of_mb);
		DDL_METADATA_ALIGNSIZE(sz);
		suffix += sz;
	}

	sz = DDL_METADATA_EXTRADATANONE_SIZE;
	DDL_METADATA_ALIGNSIZE(sz);
	suffix += (sz);

	suffix += DDL_METADATA_EXTRAPAD_SIZE;
	DDL_METADATA_ALIGNSIZE(suffix);

	encoder->suffix = suffix;
	encoder->output_buf_req.sz += suffix;
}

u32 ddl_set_metadata_params(struct ddl_client_context *ddl,
			    struct vcd_property_hdr *property_hdr,
			    void *property_value)
{
	u32 vcd_status = VCD_ERR_ILLEGAL_PARM;
	if (property_hdr->prop_id == VCD_I_METADATA_ENABLE) {
		struct vcd_property_meta_data_enable *meta_data_enable =
		    (struct vcd_property_meta_data_enable *)
		    property_value;
		u32 *meta_data_enable_flag;
		enum vcd_codec codec;
		if (ddl->decoding) {
			meta_data_enable_flag =
			    &(ddl->codec_data.decoder.
			      meta_data_enable_flag);
			codec = ddl->codec_data.decoder.codec.codec;
		} else {
			meta_data_enable_flag =
			    &(ddl->codec_data.encoder.
			      meta_data_enable_flag);
			codec = ddl->codec_data.encoder.codec.codec;
		}
		if (sizeof(struct vcd_property_meta_data_enable) ==
		    property_hdr->sz &&
		    DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_OPEN) &&
					codec) {
			u32 flag = ddl_supported_metadata_flag(ddl);
			flag &= (meta_data_enable->meta_data_enable_flag);
			if (flag)
				flag |= DDL_METADATA_MANDATORY;
			if (flag != *meta_data_enable_flag) {
				*meta_data_enable_flag = flag;
				if (ddl->decoding) {
					ddl_set_default_decoder_buffer_req
						(&ddl->codec_data.decoder,
						 true);
				} else {
					ddl_set_default_encoder_buffer_req
						(&ddl->codec_data.encoder);
				}
			}
			vcd_status = VCD_S_SUCCESS;
		}
	} else if (property_hdr->prop_id == VCD_I_METADATA_HEADER) {
		struct vcd_property_metadata_hdr *hdr =
		    (struct vcd_property_metadata_hdr *)property_value;
		if (sizeof(struct vcd_property_metadata_hdr) ==
		    property_hdr->sz) {
			u32 flag = ddl_supported_metadata_flag(ddl);
			flag |= DDL_METADATA_MANDATORY;
			flag &= hdr->meta_data_id;
			if (!(flag & (flag - 1))) {
				u32 *hdr_entry =
				    ddl_metadata_hdr_entry(ddl, flag);
				hdr_entry[DDL_METADATA_HDR_VERSION_INDEX] =
				    hdr->version;
				hdr_entry[DDL_METADATA_HDR_PORT_INDEX] =
				    hdr->port_index;
				hdr_entry[DDL_METADATA_HDR_TYPE_INDEX] =
				    hdr->type;
				vcd_status = VCD_S_SUCCESS;
			}
		}
	}
	return vcd_status;
}

u32 ddl_get_metadata_params(struct ddl_client_context *ddl,
	struct vcd_property_hdr *property_hdr,
	void	*property_value)
{
	u32 vcd_status = VCD_ERR_ILLEGAL_PARM ;
	if (property_hdr->prop_id == VCD_I_METADATA_ENABLE &&
		sizeof(struct vcd_property_meta_data_enable)
		== property_hdr->sz) {
		struct vcd_property_meta_data_enable *meta_data_enable =
			(struct vcd_property_meta_data_enable *)
			property_value;
		meta_data_enable->meta_data_enable_flag =
			((ddl->decoding) ?
			(ddl->codec_data.decoder.meta_data_enable_flag)
			: (ddl->codec_data.encoder.meta_data_enable_flag));
		vcd_status = VCD_S_SUCCESS;
	} else if (property_hdr->prop_id == VCD_I_METADATA_HEADER &&
		sizeof(struct vcd_property_metadata_hdr) ==
		property_hdr->sz) {
		struct vcd_property_metadata_hdr *hdr =
			(struct vcd_property_metadata_hdr *)
			property_value;
		u32 flag = ddl_supported_metadata_flag(ddl);
		flag |= DDL_METADATA_MANDATORY;
		flag &= hdr->meta_data_id;
		if (!(flag & (flag - 1))) {
			u32 *hdr_entry = ddl_metadata_hdr_entry(ddl,
				flag);
			hdr->version =
			hdr_entry[DDL_METADATA_HDR_VERSION_INDEX];
			hdr->port_index =
			hdr_entry[DDL_METADATA_HDR_PORT_INDEX];
			hdr->type =
				hdr_entry[DDL_METADATA_HDR_TYPE_INDEX];
			vcd_status = VCD_S_SUCCESS;
		}
	}
	return vcd_status;
}

void ddl_metadata_enable(struct ddl_client_context *ddl)
{
	u32 flag, hal_flag = 0;
	u32 *metadata_input;
	if (ddl->decoding) {
		flag = ddl->codec_data.decoder.meta_data_enable_flag;
		metadata_input =
		    ddl->codec_data.decoder.meta_data_input.
		    align_physical_addr;
	} else {
		flag = ddl->codec_data.encoder.meta_data_enable_flag;
		metadata_input =
		    ddl->codec_data.encoder.meta_data_input.
		    align_physical_addr;
	}
	if (flag) {
		if (flag & VCD_METADATA_QPARRAY)
			hal_flag |= VIDC_720P_METADATA_ENABLE_QP;
		if (flag & VCD_METADATA_CONCEALMB)
			hal_flag |= VIDC_720P_METADATA_ENABLE_CONCEALMB;
		if (flag & VCD_METADATA_VC1)
			hal_flag |= VIDC_720P_METADATA_ENABLE_VC1;
		if (flag & VCD_METADATA_SEI)
			hal_flag |= VIDC_720P_METADATA_ENABLE_SEI;
		if (flag & VCD_METADATA_VUI)
			hal_flag |= VIDC_720P_METADATA_ENABLE_VUI;
		if (flag & VCD_METADATA_ENC_SLICE)
			hal_flag |= VIDC_720P_METADATA_ENABLE_ENCSLICE;
		if (flag & VCD_METADATA_PASSTHROUGH)
			hal_flag |= VIDC_720P_METADATA_ENABLE_PASSTHROUGH;
	} else {
		metadata_input = 0;
	}
	vidc_720p_metadata_enable(hal_flag, metadata_input);
}

u32 ddl_encode_set_metadata_output_buf(struct ddl_client_context *ddl)
{
	struct ddl_encoder_data *encoder = &ddl->codec_data.encoder;
	u32 *buffer;
	struct vcd_frame_data *stream = &(ddl->output_frame.vcd_frm);
	u32 ext_buffer_end, hw_metadata_start;

	ext_buffer_end = (u32) stream->physical + stream->alloc_len;
	if (!encoder->meta_data_enable_flag) {
		ext_buffer_end &= ~(DDL_STREAMBUF_ALIGN_GUARD_BYTES);
		return ext_buffer_end;
	}
	hw_metadata_start = (ext_buffer_end - encoder->suffix) &
	    ~(DDL_STREAMBUF_ALIGN_GUARD_BYTES);

	ext_buffer_end = (hw_metadata_start - 1) &
	    ~(DDL_STREAMBUF_ALIGN_GUARD_BYTES);

	buffer = encoder->meta_data_input.align_virtual_addr;

	*buffer++ = encoder->suffix;

	*buffer = hw_metadata_start;

	encoder->meta_data_offset =
	    hw_metadata_start - (u32) stream->physical;

	return ext_buffer_end;
}

void ddl_decode_set_metadata_output(struct ddl_decoder_data *decoder)
{
	u32 *buffer;
	u32 loopc;

	if (!decoder->meta_data_enable_flag) {
		decoder->meta_data_offset = 0;
		return;
	}

	decoder->meta_data_offset = ddl_get_yuv_buffer_size(
		&decoder->client_frame_size, &decoder->buf_format,
		(!decoder->progressive_only), decoder->codec.codec);

	buffer = decoder->meta_data_input.align_virtual_addr;

	*buffer++ = decoder->suffix;

	for (loopc = 0; loopc < decoder->dp_buf.no_of_dec_pic_buf;
	     ++loopc) {
		*buffer++ = (u32) (decoder->meta_data_offset + (u8 *)
				     decoder->dp_buf.
				     dec_pic_buffers[loopc].vcd_frm.
				     physical);
	}
}

void ddl_process_encoder_metadata(struct ddl_client_context *ddl)
{
	struct ddl_encoder_data *encoder = &(ddl->codec_data.encoder);
	struct vcd_frame_data *out_frame =
	    &(ddl->output_frame.vcd_frm);
	u32 *qfiller_hdr, *qfiller, start_addr;
	u32 qfiller_size;

	if (!encoder->meta_data_enable_flag) {
		out_frame->flags &= ~(VCD_FRAME_FLAG_EXTRADATA);
		return;
	}

	if (!encoder->enc_frame_info.metadata_exists) {
		out_frame->flags &= ~(VCD_FRAME_FLAG_EXTRADATA);
		return;
	}
	out_frame->flags |= VCD_FRAME_FLAG_EXTRADATA;

	start_addr = (u32) ((u8 *) out_frame->virtual +
			      out_frame->offset);
	qfiller = (u32 *) ((out_frame->data_len + start_addr + 3) & ~3);

	qfiller_size = (u32) ((encoder->meta_data_offset +
				 (u8 *) out_frame->virtual) -
				(u8 *) qfiller);

	qfiller_hdr = ddl_metadata_hdr_entry(ddl, VCD_METADATA_QCOMFILLER);

	*qfiller++ = qfiller_size;
	*qfiller++ = qfiller_hdr[DDL_METADATA_HDR_VERSION_INDEX];
	*qfiller++ = qfiller_hdr[DDL_METADATA_HDR_PORT_INDEX];
	*qfiller++ = qfiller_hdr[DDL_METADATA_HDR_TYPE_INDEX];
	*qfiller = (u32) (qfiller_size - DDL_METADATA_HDR_SIZE);
}

void ddl_process_decoder_metadata(struct ddl_client_context *ddl)
{
	struct ddl_decoder_data *decoder = &(ddl->codec_data.decoder);
	struct vcd_frame_data *output_frame =
	    &(ddl->output_frame.vcd_frm);
	u32 *qfiller_hdr, *qfiller;
	u32 qfiller_size;

	if (!decoder->meta_data_enable_flag) {
		output_frame->flags &= ~(VCD_FRAME_FLAG_EXTRADATA);
		return;
	}

	if (!decoder->dec_disp_info.metadata_exists) {
		output_frame->flags &= ~(VCD_FRAME_FLAG_EXTRADATA);
		return;
	}
	output_frame->flags |= VCD_FRAME_FLAG_EXTRADATA;

	if (output_frame->data_len != decoder->meta_data_offset) {
		qfiller = (u32 *) ((u32) ((output_frame->data_len +
					     output_frame->offset +
					     (u8 *) output_frame->virtual) +
					    3) & ~3);

		qfiller_size = (u32) ((decoder->meta_data_offset +
					 (u8 *) output_frame->virtual) -
					(u8 *) qfiller);

		qfiller_hdr =
		    ddl_metadata_hdr_entry(ddl, VCD_METADATA_QCOMFILLER);
		*qfiller++ = qfiller_size;
		*qfiller++ = qfiller_hdr[DDL_METADATA_HDR_VERSION_INDEX];
		*qfiller++ = qfiller_hdr[DDL_METADATA_HDR_PORT_INDEX];
		*qfiller++ = qfiller_hdr[DDL_METADATA_HDR_TYPE_INDEX];
		*qfiller = (u32) (qfiller_size - DDL_METADATA_HDR_SIZE);
	}
}
