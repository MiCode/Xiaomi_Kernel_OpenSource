// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
 */
#include "hfi_packetization.h"
#include "msm_vidc_debug.h"

u32 vidc_get_hfi_domain(enum hal_domain hal_domain, u32 sid)
{
	u32 hfi_domain;

	switch (hal_domain) {
	case HAL_VIDEO_DOMAIN_VPE:
		hfi_domain = HFI_VIDEO_DOMAIN_VPE;
		break;
	case HAL_VIDEO_DOMAIN_ENCODER:
		hfi_domain = HFI_VIDEO_DOMAIN_ENCODER;
		break;
	case HAL_VIDEO_DOMAIN_DECODER:
		hfi_domain = HFI_VIDEO_DOMAIN_DECODER;
		break;
	case HAL_VIDEO_DOMAIN_CVP:
		hfi_domain = HFI_VIDEO_DOMAIN_CVP;
		break;
	default:
		s_vpr_e(sid, "%s: invalid domain 0x%x\n",
			__func__, hal_domain);
		hfi_domain = 0;
		break;
	}
	return hfi_domain;
}

u32 vidc_get_hfi_codec(enum hal_video_codec hal_codec, u32 sid)
{
	u32 hfi_codec = 0;

	switch (hal_codec) {
	case HAL_VIDEO_CODEC_H264:
		hfi_codec = HFI_VIDEO_CODEC_H264;
		break;
	case HAL_VIDEO_CODEC_MPEG1:
		hfi_codec = HFI_VIDEO_CODEC_MPEG1;
		break;
	case HAL_VIDEO_CODEC_MPEG2:
		hfi_codec = HFI_VIDEO_CODEC_MPEG2;
		break;
	case HAL_VIDEO_CODEC_VP8:
		hfi_codec = HFI_VIDEO_CODEC_VP8;
		break;
	case HAL_VIDEO_CODEC_HEVC:
		hfi_codec = HFI_VIDEO_CODEC_HEVC;
		break;
	case HAL_VIDEO_CODEC_VP9:
		hfi_codec = HFI_VIDEO_CODEC_VP9;
		break;
	case HAL_VIDEO_CODEC_TME:
		hfi_codec = HFI_VIDEO_CODEC_TME;
		break;
	case HAL_VIDEO_CODEC_CVP:
		hfi_codec = HFI_VIDEO_CODEC_CVP;
		break;
	default:
		s_vpr_h(sid, "%s: invalid codec 0x%x\n",
			__func__, hal_codec);
		hfi_codec = 0;
		break;
	}
	return hfi_codec;
}

int create_pkt_cmd_sys_init(struct hfi_cmd_sys_init_packet *pkt,
			   u32 arch_type)
{
	int rc = 0;

	if (!pkt)
		return -EINVAL;

	pkt->packet_type = HFI_CMD_SYS_INIT;
	pkt->size = sizeof(struct hfi_cmd_sys_init_packet);
	pkt->arch_type = arch_type;
	return rc;
}

int create_pkt_cmd_sys_pc_prep(struct hfi_cmd_sys_pc_prep_packet *pkt)
{
	int rc = 0;

	if (!pkt)
		return -EINVAL;

	pkt->packet_type = HFI_CMD_SYS_PC_PREP;
	pkt->size = sizeof(struct hfi_cmd_sys_pc_prep_packet);
	return rc;
}

int create_pkt_cmd_sys_debug_config(
	struct hfi_cmd_sys_set_property_packet *pkt,
	u32 mode)
{
	struct hfi_debug_config *hfi;

	if (!pkt)
		return -EINVAL;

	pkt->size = sizeof(struct hfi_cmd_sys_set_property_packet) +
		sizeof(struct hfi_debug_config) + sizeof(u32);
	pkt->packet_type = HFI_CMD_SYS_SET_PROPERTY;
	pkt->num_properties = 1;
	pkt->rg_property_data[0] = HFI_PROPERTY_SYS_DEBUG_CONFIG;
	hfi = (struct hfi_debug_config *) &pkt->rg_property_data[1];
	hfi->debug_config = mode;
	hfi->debug_mode = HFI_DEBUG_MODE_QUEUE;
	if (msm_vidc_fw_debug_mode
			<= (HFI_DEBUG_MODE_QUEUE | HFI_DEBUG_MODE_QDSS))
		hfi->debug_mode = msm_vidc_fw_debug_mode;
	return 0;
}

int create_pkt_cmd_sys_coverage_config(
	struct hfi_cmd_sys_set_property_packet *pkt,
	u32 mode, u32 sid)
{
	if (!pkt) {
		s_vpr_e(sid, "In %s(), No input packet\n", __func__);
		return -EINVAL;
	}

	pkt->size = sizeof(struct hfi_cmd_sys_set_property_packet) +
		sizeof(u32);
	pkt->packet_type = HFI_CMD_SYS_SET_PROPERTY;
	pkt->num_properties = 1;
	pkt->rg_property_data[0] = HFI_PROPERTY_SYS_CONFIG_COVERAGE;
	pkt->rg_property_data[1] = mode;
	s_vpr_h(sid, "Firmware coverage mode %d\n", pkt->rg_property_data[1]);
	return 0;
}

int create_pkt_cmd_sys_set_resource(
		struct hfi_cmd_sys_set_resource_packet *pkt,
		struct vidc_resource_hdr *res_hdr,
		void *res_value)
{
	int rc = 0;
	u32 i = 0;

	if (!pkt || !res_hdr || !res_value) {
		d_vpr_e("Invalid paramas pkt %pK res_hdr %pK res_value %pK\n",
				pkt, res_hdr, res_value);
		return -EINVAL;
	}

	pkt->packet_type = HFI_CMD_SYS_SET_RESOURCE;
	pkt->size = sizeof(struct hfi_cmd_sys_set_resource_packet);
	pkt->resource_handle = hash32_ptr(res_hdr->resource_handle);

	switch (res_hdr->resource_id) {
	case VIDC_RESOURCE_SYSCACHE:
	{
		struct hfi_resource_syscache_info_type *res_sc_info =
			(struct hfi_resource_syscache_info_type *) res_value;
		struct hfi_resource_subcache_type *res_sc =
			(struct hfi_resource_subcache_type *)
				&(res_sc_info->rg_subcache_entries[0]);

		struct hfi_resource_syscache_info_type *hfi_sc_info =
			(struct hfi_resource_syscache_info_type *)
				&pkt->rg_resource_data[0];

		struct hfi_resource_subcache_type *hfi_sc =
			(struct hfi_resource_subcache_type *)
			&(hfi_sc_info->rg_subcache_entries[0]);

		pkt->resource_type = HFI_RESOURCE_SYSCACHE;
		hfi_sc_info->num_entries = res_sc_info->num_entries;

		pkt->size += (sizeof(struct hfi_resource_subcache_type))
				 * hfi_sc_info->num_entries;

		for (i = 0; i < hfi_sc_info->num_entries; i++) {
			hfi_sc[i] = res_sc[i];
		d_vpr_h("entry hfi#%d, sc_id %d, size %d\n",
				 i, hfi_sc[i].sc_id, hfi_sc[i].size);
		}
		break;
	}
	default:
		d_vpr_e("Invalid resource_id %d\n", res_hdr->resource_id);
		rc = -ENOTSUPP;
	}

	return rc;
}

int create_pkt_cmd_sys_release_resource(
		struct hfi_cmd_sys_release_resource_packet *pkt,
		struct vidc_resource_hdr *res_hdr)
{
	int rc = 0;

	if (!pkt || !res_hdr) {
		d_vpr_e("Invalid paramas pkt %pK res_hdr %pK\n",
				pkt, res_hdr);
		return -EINVAL;
	}

	pkt->size = sizeof(struct hfi_cmd_sys_release_resource_packet);
	pkt->packet_type = HFI_CMD_SYS_RELEASE_RESOURCE;
	pkt->resource_handle = hash32_ptr(res_hdr->resource_handle);

	switch (res_hdr->resource_id) {
	case VIDC_RESOURCE_SYSCACHE:
		pkt->resource_type = HFI_RESOURCE_SYSCACHE;
		break;
	default:
		d_vpr_e("Invalid resource_id %d\n", res_hdr->resource_id);
		rc = -ENOTSUPP;
	}

	d_vpr_h("rel_res: pkt_type 0x%x res_type 0x%x prepared\n",
		pkt->packet_type, pkt->resource_type);

	return rc;
}

inline int create_pkt_cmd_sys_session_init(
		struct hfi_cmd_sys_session_init_packet *pkt,
		u32 sid, u32 session_domain, u32 session_codec)
{
	int rc = 0;

	if (!pkt)
		return -EINVAL;

	pkt->size = sizeof(struct hfi_cmd_sys_session_init_packet);
	pkt->packet_type = HFI_CMD_SYS_SESSION_INIT;
	pkt->sid = sid;
	pkt->session_domain = vidc_get_hfi_domain(session_domain, sid);
	pkt->session_codec = vidc_get_hfi_codec(session_codec, sid);
	if (!pkt->session_codec)
		return -EINVAL;

	return rc;
}


int create_pkt_cmd_sys_ubwc_config(
		struct hfi_cmd_sys_set_property_packet *pkt,
		struct msm_vidc_ubwc_config_data *ubwc_config)
{
	int rc = 0;
	struct hfi_cmd_sys_set_ubwc_config_packet_type *hfi;

	if (!pkt)
		return -EINVAL;

	pkt->size = sizeof(struct hfi_cmd_sys_set_property_packet) +
		sizeof(struct hfi_cmd_sys_set_ubwc_config_packet_type)
		+ sizeof(u32);

	pkt->packet_type = HFI_CMD_SYS_SET_PROPERTY;
	pkt->num_properties = 1;
	pkt->rg_property_data[0] = HFI_PROPERTY_SYS_UBWC_CONFIG;
	hfi = (struct hfi_cmd_sys_set_ubwc_config_packet_type *)
		&pkt->rg_property_data[1];

	hfi->max_channels = ubwc_config->max_channels;
	hfi->override_bit_info.max_channel_override =
		ubwc_config->override_bit_info.max_channel_override;

	hfi->mal_length = ubwc_config->mal_length;
	hfi->override_bit_info.mal_length_override =
		ubwc_config->override_bit_info.mal_length_override;

	hfi->highest_bank_bit = ubwc_config->highest_bank_bit;
	hfi->override_bit_info.hb_override =
		ubwc_config->override_bit_info.hb_override;

	hfi->bank_swzl_level = ubwc_config->bank_swzl_level;
	hfi->override_bit_info.bank_swzl_level_override =
		ubwc_config->override_bit_info.bank_swzl_level_override;

	hfi->bank_spreading = ubwc_config->bank_spreading;
	hfi->override_bit_info.bank_spreading_override =
		ubwc_config->override_bit_info.bank_spreading_override;

	return rc;
}

int create_pkt_cmd_session_cmd(struct vidc_hal_session_cmd_pkt *pkt,
			int pkt_type, u32 sid)
{
	int rc = 0;

	if (!pkt)
		return -EINVAL;

	pkt->size = sizeof(struct vidc_hal_session_cmd_pkt);
	pkt->packet_type = pkt_type;
	pkt->sid = sid;

	return rc;
}

int create_pkt_cmd_sys_power_control(
	struct hfi_cmd_sys_set_property_packet *pkt, u32 enable)
{
	struct hfi_enable *hfi;

	if (!pkt) {
		d_vpr_e("%s: No input packet\n", __func__);
		return -EINVAL;
	}

	pkt->size = sizeof(struct hfi_cmd_sys_set_property_packet) +
		sizeof(struct hfi_enable) + sizeof(u32);
	pkt->packet_type = HFI_CMD_SYS_SET_PROPERTY;
	pkt->num_properties = 1;
	pkt->rg_property_data[0] = HFI_PROPERTY_SYS_CODEC_POWER_PLANE_CTRL;
	hfi = (struct hfi_enable *) &pkt->rg_property_data[1];
	hfi->enable = enable;
	return 0;
}

static u32 get_hfi_buffer(int hal_buffer, u32 sid)
{
	u32 buffer;

	switch (hal_buffer) {
	case HAL_BUFFER_INPUT:
		buffer = HFI_BUFFER_INPUT;
		break;
	case HAL_BUFFER_OUTPUT:
		buffer = HFI_BUFFER_OUTPUT;
		break;
	case HAL_BUFFER_OUTPUT2:
		buffer = HFI_BUFFER_OUTPUT2;
		break;
	case HAL_BUFFER_EXTRADATA_INPUT:
		buffer = HFI_BUFFER_EXTRADATA_INPUT;
		break;
	case HAL_BUFFER_EXTRADATA_OUTPUT:
		buffer = HFI_BUFFER_EXTRADATA_OUTPUT;
		break;
	case HAL_BUFFER_EXTRADATA_OUTPUT2:
		buffer = HFI_BUFFER_EXTRADATA_OUTPUT2;
		break;
	case HAL_BUFFER_INTERNAL_SCRATCH:
		buffer = HFI_BUFFER_COMMON_INTERNAL_SCRATCH;
		break;
	case HAL_BUFFER_INTERNAL_SCRATCH_1:
		buffer = HFI_BUFFER_COMMON_INTERNAL_SCRATCH_1;
		break;
	case HAL_BUFFER_INTERNAL_SCRATCH_2:
		buffer = HFI_BUFFER_COMMON_INTERNAL_SCRATCH_2;
		break;
	case HAL_BUFFER_INTERNAL_PERSIST:
		buffer = HFI_BUFFER_INTERNAL_PERSIST;
		break;
	case HAL_BUFFER_INTERNAL_PERSIST_1:
		buffer = HFI_BUFFER_INTERNAL_PERSIST_1;
		break;
	default:
		s_vpr_e(sid, "Invalid buffer: %#x\n", hal_buffer);
		buffer = 0;
		break;
	}
	return buffer;
}

int create_pkt_cmd_session_set_buffers(
		struct hfi_cmd_session_set_buffers_packet *pkt,
		u32 sid, struct vidc_buffer_addr_info *buffer_info)
{
	int rc = 0;
	u32 i = 0;

	if (!pkt)
		return -EINVAL;

	pkt->packet_type = HFI_CMD_SESSION_SET_BUFFERS;
	pkt->sid = sid;
	pkt->buffer_size = buffer_info->buffer_size;
	pkt->min_buffer_size = buffer_info->buffer_size;
	pkt->num_buffers = buffer_info->num_buffers;

	if (buffer_info->buffer_type == HAL_BUFFER_OUTPUT ||
		buffer_info->buffer_type == HAL_BUFFER_OUTPUT2) {
		struct hfi_buffer_info *buff;

		pkt->extra_data_size = buffer_info->extradata_size;

		pkt->size = sizeof(struct hfi_cmd_session_set_buffers_packet) -
				sizeof(u32) + (buffer_info->num_buffers *
				sizeof(struct hfi_buffer_info));
		buff = (struct hfi_buffer_info *) pkt->rg_buffer_info;
		for (i = 0; i < pkt->num_buffers; i++) {
			buff->buffer_addr =
				(u32)buffer_info->align_device_addr;
			buff->extra_data_addr =
				(u32)buffer_info->extradata_addr;
		}
	} else {
		pkt->extra_data_size = 0;
		pkt->size = sizeof(struct hfi_cmd_session_set_buffers_packet) +
			((buffer_info->num_buffers - 1) * sizeof(u32));
		for (i = 0; i < pkt->num_buffers; i++) {
			pkt->rg_buffer_info[i] =
				(u32)buffer_info->align_device_addr;
		}
	}

	pkt->buffer_type =
		get_hfi_buffer(buffer_info->buffer_type, pkt->sid);
	if (!pkt->buffer_type)
		return -EINVAL;

	return rc;
}

int create_pkt_cmd_session_release_buffers(
		struct hfi_cmd_session_release_buffer_packet *pkt,
		u32 sid, struct vidc_buffer_addr_info *buffer_info)
{
	int rc = 0;
	u32 i = 0;

	if (!pkt)
		return -EINVAL;

	pkt->packet_type = HFI_CMD_SESSION_RELEASE_BUFFERS;
	pkt->sid = sid;
	pkt->buffer_size = buffer_info->buffer_size;
	pkt->num_buffers = buffer_info->num_buffers;

	if (buffer_info->buffer_type == HAL_BUFFER_OUTPUT ||
		buffer_info->buffer_type == HAL_BUFFER_OUTPUT2) {
		struct hfi_buffer_info *buff;

		buff = (struct hfi_buffer_info *) pkt->rg_buffer_info;
		for (i = 0; i < pkt->num_buffers; i++) {
			buff->buffer_addr =
				(u32)buffer_info->align_device_addr;
			buff->extra_data_addr =
				(u32)buffer_info->extradata_addr;
		}
		pkt->size = sizeof(struct hfi_cmd_session_set_buffers_packet) -
				sizeof(u32) + (buffer_info->num_buffers *
				sizeof(struct hfi_buffer_info));
	} else {
		for (i = 0; i < pkt->num_buffers; i++) {
			pkt->rg_buffer_info[i] =
				(u32)buffer_info->align_device_addr;
		}
		pkt->extra_data_size = 0;
		pkt->size = sizeof(struct hfi_cmd_session_set_buffers_packet) +
			((buffer_info->num_buffers - 1) * sizeof(u32));
	}
	pkt->response_req = buffer_info->response_required;
	pkt->buffer_type =
		get_hfi_buffer(buffer_info->buffer_type, pkt->sid);
	if (!pkt->buffer_type)
		return -EINVAL;
	return rc;
}

int create_pkt_cmd_session_register_buffer(
		struct hfi_cmd_session_register_buffers_packet *pkt,
		u32 sid, struct vidc_register_buffer *buffer)
{
	int rc = 0;
	u32 i;
	struct hfi_buffer_mapping_type *buf;

	if (!pkt) {
		d_vpr_e("%s: invalid params %pK\n", __func__, pkt);
		return -EINVAL;
	}
	pkt->packet_type = HFI_CMD_SESSION_REGISTER_BUFFERS;
	pkt->sid = sid;
	pkt->client_data = buffer->client_data;
	pkt->response_req = buffer->response_required;
	pkt->num_buffers = 1;
	pkt->size = sizeof(struct hfi_cmd_session_register_buffers_packet) -
			sizeof(u32) + (pkt->num_buffers *
			sizeof(struct hfi_buffer_mapping_type));

	buf = (struct hfi_buffer_mapping_type *)pkt->buffer;
	for (i = 0; i < pkt->num_buffers; i++) {
		buf->index = buffer->index;
		buf->device_addr = buffer->device_addr;
		buf->size = buffer->size;
		buf++;
	}

	return rc;
}

int create_pkt_cmd_session_unregister_buffer(
		struct hfi_cmd_session_unregister_buffers_packet *pkt,
		u32 sid, struct vidc_unregister_buffer *buffer)
{
	int rc = 0;
	u32 i;
	struct hfi_buffer_mapping_type *buf;

	if (!pkt) {
		d_vpr_e("%s: invalid params %pK\n", __func__, pkt);
		return -EINVAL;
	}
	pkt->packet_type = HFI_CMD_SESSION_UNREGISTER_BUFFERS;
	pkt->sid = sid;
	pkt->client_data = buffer->client_data;
	pkt->response_req = buffer->response_required;
	pkt->num_buffers = 1;
	pkt->size = sizeof(struct hfi_cmd_session_unregister_buffers_packet) -
			sizeof(u32) + (pkt->num_buffers *
			sizeof(struct hfi_buffer_mapping_type));

	buf = (struct hfi_buffer_mapping_type *)pkt->buffer;
	for (i = 0; i < pkt->num_buffers; i++) {
		buf->index = buffer->index;
		buf->device_addr = buffer->device_addr;
		buf->size = buffer->size;
		buf++;
	}

	return rc;
}

int create_pkt_cmd_session_etb_decoder(
	struct hfi_cmd_session_empty_buffer_compressed_packet *pkt,
	u32 sid, struct vidc_frame_data *input_frame)
{
	int rc = 0;

	if (!pkt)
		return -EINVAL;

	pkt->size =
		sizeof(struct hfi_cmd_session_empty_buffer_compressed_packet);
	pkt->packet_type = HFI_CMD_SESSION_EMPTY_BUFFER;
	pkt->sid = sid;
	pkt->time_stamp_hi = upper_32_bits(input_frame->timestamp);
	pkt->time_stamp_lo = lower_32_bits(input_frame->timestamp);
	pkt->flags = input_frame->flags;
	pkt->offset = input_frame->offset;
	pkt->alloc_len = input_frame->alloc_len;
	pkt->filled_len = input_frame->filled_len;
	pkt->input_tag = input_frame->input_tag;
	pkt->packet_buffer = (u32)input_frame->device_addr;

	trace_msm_v4l2_vidc_buffer_event_start("ETB",
		input_frame->device_addr, input_frame->timestamp,
		input_frame->alloc_len, input_frame->filled_len,
		input_frame->offset);

	if (!pkt->packet_buffer)
		rc = -EINVAL;
	return rc;
}

int create_pkt_cmd_session_etb_encoder(
	struct hfi_cmd_session_empty_buffer_uncompressed_plane0_packet *pkt,
	u32 sid, struct vidc_frame_data *input_frame)
{
	int rc = 0;

	if (!pkt)
		return -EINVAL;

	pkt->size = sizeof(struct
		hfi_cmd_session_empty_buffer_uncompressed_plane0_packet);
	pkt->packet_type = HFI_CMD_SESSION_EMPTY_BUFFER;
	pkt->sid = sid;
	pkt->view_id = 0;
	pkt->time_stamp_hi = upper_32_bits(input_frame->timestamp);
	pkt->time_stamp_lo = lower_32_bits(input_frame->timestamp);
	pkt->flags = input_frame->flags;
	pkt->offset = input_frame->offset;
	pkt->alloc_len = input_frame->alloc_len;
	pkt->filled_len = input_frame->filled_len;
	pkt->input_tag = input_frame->input_tag;
	pkt->packet_buffer = (u32)input_frame->device_addr;
	pkt->extra_data_buffer = (u32)input_frame->extradata_addr;

	trace_msm_v4l2_vidc_buffer_event_start("ETB",
		input_frame->device_addr, input_frame->timestamp,
		input_frame->alloc_len, input_frame->filled_len,
		input_frame->offset);

	if (!pkt->packet_buffer)
		rc = -EINVAL;
	return rc;
}

int create_pkt_cmd_session_ftb(struct hfi_cmd_session_fill_buffer_packet *pkt,
		u32 sid, struct vidc_frame_data *output_frame)
{
	int rc = 0;

	if (!pkt || !output_frame)
		return -EINVAL;

	pkt->size = sizeof(struct hfi_cmd_session_fill_buffer_packet);
	pkt->packet_type = HFI_CMD_SESSION_FILL_BUFFER;
	pkt->sid = sid;

	if (output_frame->buffer_type == HAL_BUFFER_OUTPUT)
		pkt->stream_id = 0;
	else if (output_frame->buffer_type == HAL_BUFFER_OUTPUT2)
		pkt->stream_id = 1;

	if (!output_frame->device_addr)
		return -EINVAL;

	pkt->packet_buffer = (u32)output_frame->device_addr;
	pkt->extra_data_buffer = (u32)output_frame->extradata_addr;
	pkt->alloc_len = output_frame->alloc_len;
	pkt->filled_len = output_frame->filled_len;
	pkt->offset = output_frame->offset;
	pkt->rgData[0] = output_frame->extradata_size;

	trace_msm_v4l2_vidc_buffer_event_start("FTB",
		output_frame->device_addr, output_frame->timestamp,
		output_frame->alloc_len, output_frame->filled_len,
		output_frame->offset);

	return rc;
}

int create_pkt_cmd_session_get_buf_req(
		struct hfi_cmd_session_get_property_packet *pkt,
		u32 sid)
{
	int rc = 0;

	if (!pkt)
		return -EINVAL;

	pkt->size = sizeof(struct hfi_cmd_session_get_property_packet);
	pkt->packet_type = HFI_CMD_SESSION_GET_PROPERTY;
	pkt->sid = sid;
	pkt->num_properties = 1;
	pkt->rg_property_data[0] = HFI_PROPERTY_CONFIG_BUFFER_REQUIREMENTS;

	return rc;
}

int create_pkt_cmd_session_flush(struct hfi_cmd_session_flush_packet *pkt,
			u32 sid, enum hal_flush flush_mode)
{
	int rc = 0;

	if (!pkt)
		return -EINVAL;

	pkt->size = sizeof(struct hfi_cmd_session_flush_packet);
	pkt->packet_type = HFI_CMD_SESSION_FLUSH;
	pkt->sid = sid;
	switch (flush_mode) {
	case HAL_FLUSH_INPUT:
		pkt->flush_type = HFI_FLUSH_INPUT;
		break;
	case HAL_FLUSH_OUTPUT:
		pkt->flush_type = HFI_FLUSH_OUTPUT;
		break;
	case HAL_FLUSH_ALL:
		pkt->flush_type = HFI_FLUSH_ALL;
		break;
	default:
		s_vpr_e(pkt->sid, "Invalid flush mode: %#x\n", flush_mode);
		return -EINVAL;
	}
	return rc;
}

int create_pkt_cmd_session_set_property(
		struct hfi_cmd_session_set_property_packet *pkt,
		u32 sid,
		u32 ptype, void *pdata, u32 size)
{
	if (!pkt)
		return -EINVAL;

	pkt->size = sizeof(struct hfi_cmd_session_set_property_packet);
	pkt->packet_type = HFI_CMD_SESSION_SET_PROPERTY;
	pkt->sid = sid;
	pkt->num_properties = 1;
	pkt->size += size;
	pkt->rg_property_data[0] = ptype;
	if (size && pdata)
		memcpy(&pkt->rg_property_data[1], pdata, size);

	s_vpr_h(pkt->sid, "Setting HAL Property = 0x%x\n", ptype);
	return 0;
}

static int get_hfi_ssr_type(enum hal_ssr_trigger_type type)
{
	int rc = HFI_TEST_SSR_HW_WDOG_IRQ;

	switch (type) {
	case SSR_ERR_FATAL:
		rc = HFI_TEST_SSR_SW_ERR_FATAL;
		break;
	case SSR_SW_DIV_BY_ZERO:
		rc = HFI_TEST_SSR_SW_DIV_BY_ZERO;
		break;
	case SSR_HW_WDOG_IRQ:
		rc = HFI_TEST_SSR_HW_WDOG_IRQ;
		break;
	default:
		d_vpr_e("SSR trigger type not recognized, using WDOG.\n");
	}
	return rc;
}

int create_pkt_ssr_cmd(enum hal_ssr_trigger_type type,
		struct hfi_cmd_sys_test_ssr_packet *pkt)
{
	if (!pkt) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	pkt->size = sizeof(struct hfi_cmd_sys_test_ssr_packet);
	pkt->packet_type = HFI_CMD_SYS_TEST_SSR;
	pkt->trigger_type = get_hfi_ssr_type(type);
	return 0;
}

int create_pkt_cmd_sys_image_version(
		struct hfi_cmd_sys_get_property_packet *pkt)
{
	if (!pkt) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	pkt->size = sizeof(struct hfi_cmd_sys_get_property_packet);
	pkt->packet_type = HFI_CMD_SYS_GET_PROPERTY;
	pkt->num_properties = 1;
	pkt->rg_property_data[0] = HFI_PROPERTY_SYS_IMAGE_VERSION;
	return 0;
}

int create_pkt_cmd_session_sync_process(
	struct hfi_cmd_session_sync_process_packet *pkt, u32 sid)
{
	if (!pkt)
		return -EINVAL;

	*pkt = (struct hfi_cmd_session_sync_process_packet) {0};
	pkt->size = sizeof(*pkt);
	pkt->packet_type = HFI_CMD_SESSION_SYNC;
	pkt->sid = sid;
	pkt->sync_id = 0;

	return 0;
}

static struct hfi_packetization_ops hfi_default = {
	.sys_init = create_pkt_cmd_sys_init,
	.sys_pc_prep = create_pkt_cmd_sys_pc_prep,
	.sys_power_control = create_pkt_cmd_sys_power_control,
	.sys_set_resource = create_pkt_cmd_sys_set_resource,
	.sys_debug_config = create_pkt_cmd_sys_debug_config,
	.sys_coverage_config = create_pkt_cmd_sys_coverage_config,
	.sys_release_resource = create_pkt_cmd_sys_release_resource,
	.sys_image_version = create_pkt_cmd_sys_image_version,
	.sys_ubwc_config = create_pkt_cmd_sys_ubwc_config,
	.ssr_cmd = create_pkt_ssr_cmd,
	.session_init = create_pkt_cmd_sys_session_init,
	.session_cmd = create_pkt_cmd_session_cmd,
	.session_set_buffers = create_pkt_cmd_session_set_buffers,
	.session_release_buffers = create_pkt_cmd_session_release_buffers,
	.session_register_buffer = create_pkt_cmd_session_register_buffer,
	.session_unregister_buffer = create_pkt_cmd_session_unregister_buffer,
	.session_etb_decoder = create_pkt_cmd_session_etb_decoder,
	.session_etb_encoder = create_pkt_cmd_session_etb_encoder,
	.session_ftb = create_pkt_cmd_session_ftb,
	.session_get_buf_req = create_pkt_cmd_session_get_buf_req,
	.session_flush = create_pkt_cmd_session_flush,
	.session_set_property = create_pkt_cmd_session_set_property,
	.session_sync_process = create_pkt_cmd_session_sync_process,
};

struct hfi_packetization_ops *hfi_get_pkt_ops_handle(
			enum hfi_packetization_type type)
{
	d_vpr_h("%s selected\n", type == HFI_PACKETIZATION_4XX ?
		"4xx packetization" : "Unknown hfi");

	switch (type) {
	case HFI_PACKETIZATION_4XX:
		return &hfi_default;
	}

	return NULL;
}
