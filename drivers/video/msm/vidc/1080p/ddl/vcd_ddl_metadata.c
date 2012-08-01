/* Copyright (c) 2010, 2012, The Linux Foundation. All rights reserved.
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

static u32 *ddl_metadata_hdr_entry(struct ddl_client_context *ddl,
	u32 meta_data)
{
	u32 skip_words = 0;
	u32 *buffer;

	if (ddl->decoding) {
		buffer = (u32 *) ddl->codec_data.decoder.meta_data_input.
			align_virtual_addr;
		skip_words = 32 + 1;
		buffer += skip_words;
		switch (meta_data) {
		default:
		case VCD_METADATA_DATANONE:
			skip_words = 0;
		break;
		case VCD_METADATA_QPARRAY:
			skip_words = 3;
		break;
		case VCD_METADATA_CONCEALMB:
			skip_words = 6;
		break;
		case VCD_METADATA_VC1:
			skip_words = 9;
		break;
		case VCD_METADATA_SEI:
			skip_words = 12;
		break;
		case VCD_METADATA_VUI:
			skip_words = 15;
		break;
		case VCD_METADATA_PASSTHROUGH:
			skip_words = 18;
		break;
		case VCD_METADATA_QCOMFILLER:
			skip_words = 21;
		break;
		case VCD_METADATA_USER_DATA:
			skip_words = 27;
		break;
		case VCD_METADATA_EXT_DATA:
			skip_words = 30;
		break;
		}
	} else {
		buffer = (u32 *) ddl->codec_data.encoder.meta_data_input.
				align_virtual_addr;
		skip_words = 2;
		buffer += skip_words;
		switch (meta_data) {
		default:
		case VCD_METADATA_DATANONE:
			skip_words = 0;
		break;
		case VCD_METADATA_ENC_SLICE:
			skip_words = 3;
		break;
		case VCD_METADATA_QCOMFILLER:
			skip_words = 6;
		break;
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
		ddl->instance_id);
	hdr_entry = ddl_metadata_hdr_entry(ddl, VCD_METADATA_QCOMFILLER);
	hdr_entry[DDL_METADATA_HDR_VERSION_INDEX] = 0x00000101;
	hdr_entry[DDL_METADATA_HDR_PORT_INDEX] = 1;
	hdr_entry[DDL_METADATA_HDR_TYPE_INDEX] = VCD_METADATA_QCOMFILLER;
	hdr_entry = ddl_metadata_hdr_entry(ddl, VCD_METADATA_DATANONE);
	hdr_entry[DDL_METADATA_HDR_VERSION_INDEX] = 0x00000101;
	hdr_entry[DDL_METADATA_HDR_PORT_INDEX] = 1;
	hdr_entry[DDL_METADATA_HDR_TYPE_INDEX] = VCD_METADATA_DATANONE;
	if (ddl->decoding) {
		hdr_entry = ddl_metadata_hdr_entry(ddl, VCD_METADATA_QPARRAY);
		hdr_entry[DDL_METADATA_HDR_VERSION_INDEX] = 0x00000101;
		hdr_entry[DDL_METADATA_HDR_PORT_INDEX] = 1;
		hdr_entry[DDL_METADATA_HDR_TYPE_INDEX] = VCD_METADATA_QPARRAY;
		hdr_entry = ddl_metadata_hdr_entry(ddl,	VCD_METADATA_CONCEALMB);
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
		hdr_entry = ddl_metadata_hdr_entry(ddl,
			VCD_METADATA_PASSTHROUGH);
		hdr_entry[DDL_METADATA_HDR_VERSION_INDEX] = 0x00000101;
		hdr_entry[DDL_METADATA_HDR_PORT_INDEX] = 1;
		hdr_entry[DDL_METADATA_HDR_TYPE_INDEX] =
			VCD_METADATA_PASSTHROUGH;
		hdr_entry = ddl_metadata_hdr_entry(ddl,
			VCD_METADATA_USER_DATA);
		hdr_entry[DDL_METADATA_HDR_VERSION_INDEX] = 0x00000101;
		hdr_entry[DDL_METADATA_HDR_PORT_INDEX] = 1;
		hdr_entry[DDL_METADATA_HDR_TYPE_INDEX] =
			VCD_METADATA_USER_DATA;
		hdr_entry = ddl_metadata_hdr_entry(ddl,
			VCD_METADATA_EXT_DATA);
		hdr_entry[DDL_METADATA_HDR_VERSION_INDEX] = 0x00000101;
		hdr_entry[DDL_METADATA_HDR_PORT_INDEX] = 1;
		hdr_entry[DDL_METADATA_HDR_TYPE_INDEX] =
			VCD_METADATA_EXT_DATA;
	} else {
		hdr_entry = ddl_metadata_hdr_entry(ddl, VCD_METADATA_ENC_SLICE);
		hdr_entry[DDL_METADATA_HDR_VERSION_INDEX] = 0x00000101;
		hdr_entry[DDL_METADATA_HDR_PORT_INDEX] = 1;
		hdr_entry[DDL_METADATA_HDR_TYPE_INDEX] = VCD_METADATA_ENC_SLICE;
	}
}

static u32 ddl_supported_metadata_flag(struct ddl_client_context *ddl)
{
	u32 flag = 0;

	if (ddl->decoding) {
		enum vcd_codec codec =
			ddl->codec_data.decoder.codec.codec;

		flag |= (VCD_METADATA_CONCEALMB | VCD_METADATA_PASSTHROUGH |
				VCD_METADATA_QPARRAY);
		if (codec == VCD_CODEC_H264)
			flag |= (VCD_METADATA_SEI | VCD_METADATA_VUI);
		else if (codec == VCD_CODEC_VC1 ||
			codec == VCD_CODEC_VC1_RCV)
			flag |= VCD_METADATA_VC1;
		else if (codec == VCD_CODEC_MPEG2)
			flag |= (VCD_METADATA_USER_DATA |
				VCD_METADATA_EXT_DATA);
	} else
		flag |= VCD_METADATA_ENC_SLICE;
	return flag;
}

void ddl_set_default_metadata_flag(struct ddl_client_context *ddl)
{
	if (ddl->decoding)
		ddl->codec_data.decoder.meta_data_enable_flag = 0;
	else
		ddl->codec_data.encoder.meta_data_enable_flag = 0;
}

void ddl_set_default_decoder_metadata_buffer_size(struct ddl_decoder_data
	*decoder, struct vcd_property_frame_size *frame_size,
	struct vcd_buffer_requirement *output_buf_req)
{
	u32 flag = decoder->meta_data_enable_flag;
	u32 suffix = 0, size = 0;
	if (!flag) {
		decoder->suffix = 0;
		return;
	}
	if (flag & VCD_METADATA_QPARRAY) {
		u32 num_of_mb = DDL_NO_OF_MB(frame_size->width,
			frame_size->height);

		size = DDL_METADATA_HDR_SIZE;
		size += num_of_mb;
		DDL_METADATA_ALIGNSIZE(size);
		suffix += size;
	}
	if (flag & VCD_METADATA_CONCEALMB) {
		u32 num_of_mb = DDL_NO_OF_MB(frame_size->width,
			frame_size->height);
		size = DDL_METADATA_HDR_SIZE + (num_of_mb >> 3);
		DDL_METADATA_ALIGNSIZE(size);
		suffix += size;
	}
	if (flag & VCD_METADATA_VC1) {
		size = DDL_METADATA_HDR_SIZE;
		size += DDL_METADATA_VC1_PAYLOAD_SIZE;
		DDL_METADATA_ALIGNSIZE(size);
		suffix += size;
	}
	if (flag & VCD_METADATA_SEI) {
		size = DDL_METADATA_HDR_SIZE;
		size += DDL_METADATA_SEI_PAYLOAD_SIZE;
		DDL_METADATA_ALIGNSIZE(size);
		suffix += (size * DDL_METADATA_SEI_MAX);
	}
	if (flag & VCD_METADATA_VUI) {
		size = DDL_METADATA_HDR_SIZE;
		size += DDL_METADATA_VUI_PAYLOAD_SIZE;
		DDL_METADATA_ALIGNSIZE(size);
		suffix += (size);
	}
	if (flag & VCD_METADATA_PASSTHROUGH) {
		size = DDL_METADATA_HDR_SIZE;
		size += DDL_METADATA_PASSTHROUGH_PAYLOAD_SIZE;
		DDL_METADATA_ALIGNSIZE(size);
		suffix += (size);
	}
	if (flag & VCD_METADATA_USER_DATA) {
		size = DDL_METADATA_HDR_SIZE;
		size += DDL_METADATA_USER_PAYLOAD_SIZE;
		DDL_METADATA_ALIGNSIZE(size);
		suffix += (size);
	}
	if (flag & VCD_METADATA_EXT_DATA) {
		size = DDL_METADATA_HDR_SIZE;
		size += DDL_METADATA_EXT_PAYLOAD_SIZE;
		DDL_METADATA_ALIGNSIZE(size);
		suffix += (size);
	}
	size = DDL_METADATA_EXTRADATANONE_SIZE;
	DDL_METADATA_ALIGNSIZE(size);
	suffix += (size);
	suffix += DDL_METADATA_EXTRAPAD_SIZE;
	DDL_METADATA_ALIGNSIZE(suffix);
	decoder->suffix = suffix;
	output_buf_req->sz += suffix;
	decoder->meta_data_offset = 0;
	DDL_MSG_LOW("metadata output buf size : %d", suffix);
}

void ddl_set_default_encoder_metadata_buffer_size(struct ddl_encoder_data
	*encoder)
{
	u32 flag = encoder->meta_data_enable_flag;
	u32 suffix = 0, size = 0;

	if (!flag) {
		encoder->suffix = 0;
		return;
	}
	if (flag & VCD_METADATA_ENC_SLICE) {
		u32 num_of_mb = DDL_NO_OF_MB(encoder->frame_size.width,
			encoder->frame_size.height);
		size = DDL_METADATA_HDR_SIZE;
		size += 4;
		size += (num_of_mb << 3);
		DDL_METADATA_ALIGNSIZE(size);
		suffix += size;
	}
	size = DDL_METADATA_EXTRADATANONE_SIZE;
	DDL_METADATA_ALIGNSIZE(size);
	suffix += (size);
	suffix += DDL_METADATA_EXTRAPAD_SIZE;
	DDL_METADATA_ALIGNSIZE(suffix);
	encoder->suffix = suffix;
	encoder->output_buf_req.sz += suffix;
	encoder->output_buf_req.sz =
		DDL_ALIGN(encoder->output_buf_req.sz, DDL_KILO_BYTE(4));
}

u32 ddl_set_metadata_params(struct ddl_client_context *ddl,
	struct vcd_property_hdr *property_hdr, void *property_value)
{
	u32  vcd_status = VCD_ERR_ILLEGAL_PARM;
	if (property_hdr->prop_id == VCD_I_METADATA_ENABLE) {
		struct vcd_property_meta_data_enable *meta_data_enable =
			(struct vcd_property_meta_data_enable *) property_value;
		u32 *meta_data_enable_flag;
		enum vcd_codec codec;

		if (ddl->decoding) {
			meta_data_enable_flag =
			&(ddl->codec_data.decoder.meta_data_enable_flag);
			codec = ddl->codec_data.decoder.codec.codec;
		} else {
			meta_data_enable_flag =
				&ddl->codec_data.encoder.meta_data_enable_flag;
			codec = ddl->codec_data.encoder.codec.codec;
		}
		if (sizeof(struct vcd_property_meta_data_enable) ==
			property_hdr->sz &&
			DDLCLIENT_STATE_IS(ddl, DDL_CLIENT_OPEN) && codec) {
			u32 flag = ddl_supported_metadata_flag(ddl);
			flag &= (meta_data_enable->meta_data_enable_flag);
			if (flag)
				flag |= DDL_METADATA_MANDATORY;
			if (*meta_data_enable_flag != flag) {
				if (VCD_CODEC_MPEG2 == codec)
					ddl_set_mp2_dump_default(
						&ddl->codec_data.decoder,
						flag);
				*meta_data_enable_flag = flag;
				if (ddl->decoding)
					ddl_set_default_decoder_buffer_req(
						&ddl->codec_data.decoder, true);
				else
					ddl_set_default_encoder_buffer_req(
						&ddl->codec_data.encoder);
			}
			vcd_status = VCD_S_SUCCESS;
		}
	} else if (property_hdr->prop_id == VCD_I_METADATA_HEADER) {
		struct vcd_property_metadata_hdr *hdr =
			(struct vcd_property_metadata_hdr *) property_value;

		if (sizeof(struct vcd_property_metadata_hdr) ==
			property_hdr->sz) {
			u32 flag = ddl_supported_metadata_flag(ddl);

			flag |= DDL_METADATA_MANDATORY;
			flag &= hdr->meta_data_id;
			if (!(flag & (flag - 1))) {
				u32 *hdr_entry = ddl_metadata_hdr_entry(ddl,
					flag);
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
	struct vcd_property_hdr *property_hdr, void *property_value)
{
	u32 vcd_status = VCD_ERR_ILLEGAL_PARM;
	if (property_hdr->prop_id == VCD_I_METADATA_ENABLE &&
		sizeof(struct vcd_property_meta_data_enable) ==
		property_hdr->sz) {
		struct vcd_property_meta_data_enable *meta_data_enable =
			(struct vcd_property_meta_data_enable *) property_value;

		meta_data_enable->meta_data_enable_flag =
			((ddl->decoding) ?
			(ddl->codec_data.decoder.meta_data_enable_flag) :
			(ddl->codec_data.encoder.meta_data_enable_flag));
		vcd_status = VCD_S_SUCCESS;
	} else if (property_hdr->prop_id == VCD_I_METADATA_HEADER &&
		sizeof(struct vcd_property_metadata_hdr) ==
		property_hdr->sz) {
		struct vcd_property_metadata_hdr *hdr =
			(struct vcd_property_metadata_hdr *) property_value;
		u32 flag = ddl_supported_metadata_flag(ddl);

		flag |= DDL_METADATA_MANDATORY;
		flag &= hdr->meta_data_id;
		if (!(flag & (flag - 1))) {
			u32 *hdr_entry = ddl_metadata_hdr_entry(ddl, flag);
			hdr->version =
				hdr_entry[DDL_METADATA_HDR_VERSION_INDEX];
			hdr->port_index =
				hdr_entry[DDL_METADATA_HDR_PORT_INDEX];
			hdr->type = hdr_entry[DDL_METADATA_HDR_TYPE_INDEX];
			vcd_status = VCD_S_SUCCESS;
		}
	}
	return vcd_status;
}

void ddl_vidc_metadata_enable(struct ddl_client_context *ddl)
{
	u32 flag, extradata_enable = false;
	u32 qp_enable = false, concealed_mb_enable = false;
	u32 vc1_param_enable = false, sei_nal_enable = false;
	u32 vui_enable = false, enc_slice_size_enable = false;
	u32 mp2_data_dump_enable = false;

	if (ddl->decoding)
		flag = ddl->codec_data.decoder.meta_data_enable_flag;
	else
		flag = ddl->codec_data.encoder.meta_data_enable_flag;
	if (flag) {
		if (flag & VCD_METADATA_QPARRAY)
			qp_enable = true;
		if (flag & VCD_METADATA_CONCEALMB)
			concealed_mb_enable = true;
		if (flag & VCD_METADATA_VC1)
			vc1_param_enable = true;
		if (flag & VCD_METADATA_SEI)
			sei_nal_enable = true;
		if (flag & VCD_METADATA_VUI)
			vui_enable = true;
		if (flag & VCD_METADATA_ENC_SLICE)
			enc_slice_size_enable = true;
		if (flag & VCD_METADATA_PASSTHROUGH)
			extradata_enable = true;
	}

	DDL_MSG_LOW("metadata enable flag : %d", sei_nal_enable);
	if (flag & VCD_METADATA_EXT_DATA || flag & VCD_METADATA_USER_DATA) {
		mp2_data_dump_enable = true;
		ddl->codec_data.decoder.extn_user_data_enable =
				mp2_data_dump_enable;
		vidc_sm_set_mp2datadump_enable(&ddl->shared_mem
		[ddl->command_channel],
		&ddl->codec_data.decoder.mp2_datadump_enable);
	} else {
		mp2_data_dump_enable = false;
		ddl->codec_data.decoder.extn_user_data_enable =
				mp2_data_dump_enable;
	}
	vidc_sm_set_metadata_enable(&ddl->shared_mem
		[ddl->command_channel], extradata_enable, qp_enable,
		concealed_mb_enable, vc1_param_enable, sei_nal_enable,
		vui_enable, enc_slice_size_enable, mp2_data_dump_enable);
}

u32 ddl_vidc_encode_set_metadata_output_buf(struct ddl_client_context *ddl)
{
	struct ddl_encoder_data *encoder = &ddl->codec_data.encoder;
	struct vcd_frame_data *stream = &ddl->output_frame.vcd_frm;
	struct ddl_context *ddl_context;
	u32 ext_buffer_end, hw_metadata_start;
	u32 *buffer;

	ddl_context = ddl_get_context();
	ext_buffer_end = (u32) stream->physical + stream->alloc_len;
	if (!encoder->meta_data_enable_flag) {
		ext_buffer_end &= ~(DDL_STREAMBUF_ALIGN_GUARD_BYTES);
		return ext_buffer_end;
	}
	hw_metadata_start = (ext_buffer_end - encoder->suffix) &
		~(DDL_STREAMBUF_ALIGN_GUARD_BYTES);
	ext_buffer_end = (hw_metadata_start - 1) &
		~(DDL_STREAMBUF_ALIGN_GUARD_BYTES);
	buffer = (u32 *) encoder->meta_data_input.align_virtual_addr;
	*buffer++ = encoder->suffix;
	*buffer  = DDL_OFFSET(ddl_context->dram_base_a.align_physical_addr,
		hw_metadata_start);
	encoder->meta_data_offset = hw_metadata_start - (u32) stream->physical;
	return ext_buffer_end;
}

void ddl_vidc_decode_set_metadata_output(struct ddl_decoder_data *decoder)
{
	struct ddl_context *ddl_context;
	u32 loopc, yuv_size;
	u32 *buffer;

	if (!decoder->meta_data_enable_flag) {
		decoder->meta_data_offset = 0;
		return;
	}
	ddl_context = ddl_get_context();
	yuv_size = ddl_get_yuv_buffer_size(&decoder->client_frame_size,
		&decoder->buf_format, !decoder->progressive_only,
		decoder->hdr.decoding, NULL);
	decoder->meta_data_offset = DDL_ALIGN_SIZE(yuv_size,
		DDL_LINEAR_BUF_ALIGN_GUARD_BYTES, DDL_LINEAR_BUF_ALIGN_MASK);
	buffer = (u32 *) decoder->meta_data_input.align_virtual_addr;
	*buffer++ = decoder->suffix;
	DDL_MSG_LOW("Metadata offset & size : %d/%d",
		decoder->meta_data_offset, decoder->suffix);
	for (loopc = 0; loopc < decoder->dp_buf.no_of_dec_pic_buf;
		++loopc) {
		*buffer++ = (u32)(decoder->meta_data_offset + (u8 *)
			DDL_OFFSET(ddl_context->dram_base_a.
			align_physical_addr, decoder->dp_buf.
			dec_pic_buffers[loopc].vcd_frm.physical));
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
	if (!encoder->enc_frame_info.meta_data_exists) {
		out_frame->flags &= ~(VCD_FRAME_FLAG_EXTRADATA);
		return;
	}
	out_frame->flags |= VCD_FRAME_FLAG_EXTRADATA;
	DDL_MSG_LOW("processing metadata for encoder");
	start_addr = (u32) ((u8 *)out_frame->virtual + out_frame->offset);
	qfiller = (u32 *)((out_frame->data_len +
				start_addr + 3) & ~3);
	qfiller_size = (u32)((encoder->meta_data_offset +
		(u8 *) out_frame->virtual) - (u8 *) qfiller);
	qfiller_hdr = ddl_metadata_hdr_entry(ddl, VCD_METADATA_QCOMFILLER);
	*qfiller++ = qfiller_size;
	*qfiller++ = qfiller_hdr[DDL_METADATA_HDR_VERSION_INDEX];
	*qfiller++ = qfiller_hdr[DDL_METADATA_HDR_PORT_INDEX];
	*qfiller++ = qfiller_hdr[DDL_METADATA_HDR_TYPE_INDEX];
	*qfiller = (u32)(qfiller_size - DDL_METADATA_HDR_SIZE);
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
	if (!decoder->meta_data_exists) {
		output_frame->flags &= ~(VCD_FRAME_FLAG_EXTRADATA);
		return;
	}
	if (!decoder->mp2_datadump_status && decoder->codec.codec ==
		VCD_CODEC_MPEG2 && !decoder->extn_user_data_enable) {
		output_frame->flags &= ~(VCD_FRAME_FLAG_EXTRADATA);
		return;
	}
	DDL_MSG_LOW("processing metadata for decoder");
	DDL_MSG_LOW("data_len/metadata_offset : %d/%d",
		output_frame->data_len, decoder->meta_data_offset);
	output_frame->flags |= VCD_FRAME_FLAG_EXTRADATA;
	if (output_frame->data_len != decoder->meta_data_offset) {
		qfiller = (u32 *)((u32)((output_frame->data_len +
			output_frame->offset  +
				(u8 *) output_frame->virtual) + 3) & ~3);
		qfiller_size = (u32)((decoder->meta_data_offset +
				(u8 *) output_frame->virtual) -
				(u8 *) qfiller);
		qfiller_hdr = ddl_metadata_hdr_entry(ddl,
				VCD_METADATA_QCOMFILLER);
		*qfiller++ = qfiller_size;
		*qfiller++ = qfiller_hdr[DDL_METADATA_HDR_VERSION_INDEX];
		*qfiller++ = qfiller_hdr[DDL_METADATA_HDR_PORT_INDEX];
		*qfiller++ = qfiller_hdr[DDL_METADATA_HDR_TYPE_INDEX];
		*qfiller = (u32)(qfiller_size - DDL_METADATA_HDR_SIZE);
	}
}

void ddl_set_mp2_dump_default(struct ddl_decoder_data *decoder, u32 flag)
{

	if (flag & VCD_METADATA_EXT_DATA) {
		decoder->mp2_datadump_enable.pictempscalable_extdump_enable =
			true;
		decoder->mp2_datadump_enable.picspat_extdump_enable = true;
		decoder->mp2_datadump_enable.picdisp_extdump_enable = true;
		decoder->mp2_datadump_enable.copyright_extdump_enable = true;
		decoder->mp2_datadump_enable.quantmatrix_extdump_enable =
			true;
		decoder->mp2_datadump_enable.seqscalable_extdump_enable =
			true;
		decoder->mp2_datadump_enable.seqdisp_extdump_enable = true;
		decoder->mp2_datadump_enable.seq_extdump_enable = true;
	}
	if (flag & VCD_METADATA_USER_DATA)
		decoder->mp2_datadump_enable.userdatadump_enable =
				DDL_METADATA_USER_DUMP_FULL_MODE;
	else
		decoder->mp2_datadump_enable.userdatadump_enable =
				DDL_METADATA_USER_DUMP_DISABLE_MODE;
}
