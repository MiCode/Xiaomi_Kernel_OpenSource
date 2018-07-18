/* Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
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
#include "hfi_packetization.h"
#include "msm_vidc_debug.h"

/* Set up look-up tables to convert HAL_* to HFI_*.
 *
 * The tables below mostly take advantage of the fact that most
 * HAL_* types are defined bitwise. So if we index them normally
 * when declaring the tables, we end up with huge arrays with wasted
 * space.  So before indexing them, we apply log2 to use a more
 * sensible index.
 */

static int entropy_mode[] = {
	[ilog2(HAL_H264_ENTROPY_CAVLC)] = HFI_H264_ENTROPY_CAVLC,
	[ilog2(HAL_H264_ENTROPY_CABAC)] = HFI_H264_ENTROPY_CABAC,
};

static int statistics_mode[] = {
	[ilog2(HAL_STATISTICS_MODE_DEFAULT)] = HFI_STATISTICS_MODE_DEFAULT,
	[ilog2(HAL_STATISTICS_MODE_1)] = HFI_STATISTICS_MODE_1,
	[ilog2(HAL_STATISTICS_MODE_2)] = HFI_STATISTICS_MODE_2,
	[ilog2(HAL_STATISTICS_MODE_3)] = HFI_STATISTICS_MODE_3,
};

static int color_format[] = {
	[ilog2(HAL_COLOR_FORMAT_MONOCHROME)] = HFI_COLOR_FORMAT_MONOCHROME,
	[ilog2(HAL_COLOR_FORMAT_NV12)] = HFI_COLOR_FORMAT_NV12,
	[ilog2(HAL_COLOR_FORMAT_NV21)] = HFI_COLOR_FORMAT_NV21,
	[ilog2(HAL_COLOR_FORMAT_NV12_4x4TILE)] = HFI_COLOR_FORMAT_NV12_4x4TILE,
	[ilog2(HAL_COLOR_FORMAT_NV21_4x4TILE)] = HFI_COLOR_FORMAT_NV21_4x4TILE,
	[ilog2(HAL_COLOR_FORMAT_YUYV)] = HFI_COLOR_FORMAT_YUYV,
	[ilog2(HAL_COLOR_FORMAT_YVYU)] = HFI_COLOR_FORMAT_YVYU,
	[ilog2(HAL_COLOR_FORMAT_UYVY)] = HFI_COLOR_FORMAT_UYVY,
	[ilog2(HAL_COLOR_FORMAT_VYUY)] = HFI_COLOR_FORMAT_VYUY,
	[ilog2(HAL_COLOR_FORMAT_RGB565)] = HFI_COLOR_FORMAT_RGB565,
	[ilog2(HAL_COLOR_FORMAT_BGR565)] = HFI_COLOR_FORMAT_BGR565,
	[ilog2(HAL_COLOR_FORMAT_RGB888)] = HFI_COLOR_FORMAT_RGB888,
	[ilog2(HAL_COLOR_FORMAT_BGR888)] = HFI_COLOR_FORMAT_BGR888,
	/* UBWC Color formats*/
	[ilog2(HAL_COLOR_FORMAT_NV12_UBWC)] =  HFI_COLOR_FORMAT_NV12_UBWC,
	[ilog2(HAL_COLOR_FORMAT_NV12_TP10_UBWC)] =
			HFI_COLOR_FORMAT_YUV420_TP10_UBWC,
	/*P010 10bit format*/
	[ilog2(HAL_COLOR_FORMAT_P010)] =  HFI_COLOR_FORMAT_P010,
};

static int nal_type[] = {
	[ilog2(HAL_NAL_FORMAT_STARTCODES)] = HFI_NAL_FORMAT_STARTCODES,
	[ilog2(HAL_NAL_FORMAT_ONE_NAL_PER_BUFFER)] =
		HFI_NAL_FORMAT_ONE_NAL_PER_BUFFER,
	[ilog2(HAL_NAL_FORMAT_ONE_BYTE_LENGTH)] =
		HFI_NAL_FORMAT_ONE_BYTE_LENGTH,
	[ilog2(HAL_NAL_FORMAT_TWO_BYTE_LENGTH)] =
		HFI_NAL_FORMAT_TWO_BYTE_LENGTH,
	[ilog2(HAL_NAL_FORMAT_FOUR_BYTE_LENGTH)] =
		HFI_NAL_FORMAT_FOUR_BYTE_LENGTH,
};

static inline int hal_to_hfi_type(int property, int hal_type)
{
	if (hal_type <= 0 || roundup_pow_of_two(hal_type) != hal_type) {
		/*
		 * Not a power of 2, it's not going
		 * to be in any of the tables anyway
		 */
		return -EINVAL;
	}

	if (hal_type)
		hal_type = ilog2(hal_type);

	switch (property) {
	case HAL_PARAM_VENC_H264_ENTROPY_CONTROL:
		return (hal_type >= ARRAY_SIZE(entropy_mode)) ?
			-ENOTSUPP : entropy_mode[hal_type];
	case HAL_PARAM_UNCOMPRESSED_FORMAT_SELECT:
		return (hal_type >= ARRAY_SIZE(color_format)) ?
			-ENOTSUPP : color_format[hal_type];
	case HAL_PARAM_NAL_STREAM_FORMAT_SELECT:
		return (hal_type >= ARRAY_SIZE(nal_type)) ?
			-ENOTSUPP : nal_type[hal_type];
	case HAL_PARAM_VENC_MBI_STATISTICS_MODE:
		return (hal_type >= ARRAY_SIZE(statistics_mode)) ?
			-ENOTSUPP : statistics_mode[hal_type];
	default:
		return -ENOTSUPP;
	}
}

enum hal_domain vidc_get_hal_domain(u32 hfi_domain)
{
	enum hal_domain hal_domain = 0;

	switch (hfi_domain) {
	case HFI_VIDEO_DOMAIN_VPE:
		hal_domain = HAL_VIDEO_DOMAIN_VPE;
		break;
	case HFI_VIDEO_DOMAIN_ENCODER:
		hal_domain = HAL_VIDEO_DOMAIN_ENCODER;
		break;
	case HFI_VIDEO_DOMAIN_DECODER:
		hal_domain = HAL_VIDEO_DOMAIN_DECODER;
		break;
	case HFI_VIDEO_DOMAIN_CVP:
		hal_domain = HAL_VIDEO_DOMAIN_CVP;
		break;
	default:
		dprintk(VIDC_ERR, "%s: invalid domain %x\n",
			__func__, hfi_domain);
		hal_domain = 0;
		break;
	}
	return hal_domain;
}

enum hal_video_codec vidc_get_hal_codec(u32 hfi_codec)
{
	enum hal_video_codec hal_codec = 0;

	switch (hfi_codec) {
	case HFI_VIDEO_CODEC_H264:
		hal_codec = HAL_VIDEO_CODEC_H264;
		break;
	case HFI_VIDEO_CODEC_MPEG1:
		hal_codec = HAL_VIDEO_CODEC_MPEG1;
		break;
	case HFI_VIDEO_CODEC_MPEG2:
		hal_codec = HAL_VIDEO_CODEC_MPEG2;
		break;
	case HFI_VIDEO_CODEC_VP8:
		hal_codec = HAL_VIDEO_CODEC_VP8;
		break;
	case HFI_VIDEO_CODEC_HEVC:
		hal_codec = HAL_VIDEO_CODEC_HEVC;
		break;
	case HFI_VIDEO_CODEC_VP9:
		hal_codec = HAL_VIDEO_CODEC_VP9;
		break;
	case HFI_VIDEO_CODEC_TME:
		hal_codec = HAL_VIDEO_CODEC_TME;
		break;
	case HFI_VIDEO_CODEC_CVP:
		hal_codec = HAL_VIDEO_CODEC_CVP;
		break;
	default:
		dprintk(VIDC_INFO, "%s: invalid codec 0x%x\n",
			__func__, hfi_codec);
		hal_codec = 0;
		break;
	}
	return hal_codec;
}


u32 vidc_get_hfi_domain(enum hal_domain hal_domain)
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
		dprintk(VIDC_ERR, "%s: invalid domain 0x%x\n",
			__func__, hal_domain);
		hfi_domain = 0;
		break;
	}
	return hfi_domain;
}

u32 vidc_get_hfi_codec(enum hal_video_codec hal_codec)
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
		dprintk(VIDC_INFO, "%s: invalid codec 0x%x\n",
			__func__, hal_codec);
		hfi_codec = 0;
		break;
	}
	return hfi_codec;
}

static void create_pkt_enable(void *pkt, u32 type, bool enable)
{
	u32 *pkt_header = pkt;
	u32 *pkt_type = &pkt_header[0];
	struct hfi_enable *hfi_enable = (struct hfi_enable *)&pkt_header[1];

	*pkt_type = type;
	hfi_enable->enable = enable;
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
	u32 mode)
{
	if (!pkt) {
		dprintk(VIDC_ERR, "In %s(), No input packet\n", __func__);
		return -EINVAL;
	}

	pkt->size = sizeof(struct hfi_cmd_sys_set_property_packet) +
		sizeof(u32);
	pkt->packet_type = HFI_CMD_SYS_SET_PROPERTY;
	pkt->num_properties = 1;
	pkt->rg_property_data[0] = HFI_PROPERTY_SYS_CONFIG_COVERAGE;
	pkt->rg_property_data[1] = mode;
	dprintk(VIDC_DBG, "Firmware coverage mode %d\n",
			pkt->rg_property_data[1]);
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
		dprintk(VIDC_ERR,
			"Invalid paramas pkt %pK res_hdr %pK res_value %pK\n",
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
		dprintk(VIDC_DBG, "entry hfi#%d, sc_id %d, size %d\n",
				 i, hfi_sc[i].sc_id, hfi_sc[i].size);
		}
		break;
	}
	default:
		dprintk(VIDC_ERR,
			"Invalid resource_id %d\n", res_hdr->resource_id);
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
		dprintk(VIDC_ERR,
			"Invalid paramas pkt %pK res_hdr %pK\n",
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
		dprintk(VIDC_ERR,
			 "Invalid resource_id %d\n", res_hdr->resource_id);
		rc = -ENOTSUPP;
	}

	dprintk(VIDC_DBG,
		"rel_res: pkt_type 0x%x res_type 0x%x prepared\n",
		pkt->packet_type, pkt->resource_type);

	return rc;
}

int create_pkt_cmd_sys_ping(struct hfi_cmd_sys_ping_packet *pkt)
{
	int rc = 0;

	if (!pkt)
		return -EINVAL;

	pkt->size = sizeof(struct hfi_cmd_sys_ping_packet);
	pkt->packet_type = HFI_CMD_SYS_PING;

	return rc;
}

inline int create_pkt_cmd_sys_session_init(
		struct hfi_cmd_sys_session_init_packet *pkt,
		struct hal_session *session,
		u32 session_domain, u32 session_codec)
{
	int rc = 0;

	if (!pkt)
		return -EINVAL;

	pkt->size = sizeof(struct hfi_cmd_sys_session_init_packet);
	pkt->packet_type = HFI_CMD_SYS_SESSION_INIT;
	pkt->session_id = hash32_ptr(session);
	pkt->session_domain = vidc_get_hfi_domain(session_domain);
	pkt->session_codec = vidc_get_hfi_codec(session_codec);
	if (!pkt->session_codec)
		return -EINVAL;

	return rc;
}

int create_pkt_cmd_session_cmd(struct vidc_hal_session_cmd_pkt *pkt,
			int pkt_type, struct hal_session *session)
{
	int rc = 0;

	if (!pkt)
		return -EINVAL;

	pkt->size = sizeof(struct vidc_hal_session_cmd_pkt);
	pkt->packet_type = pkt_type;
	pkt->session_id = hash32_ptr(session);

	return rc;
}

int create_pkt_cmd_sys_power_control(
	struct hfi_cmd_sys_set_property_packet *pkt, u32 enable)
{
	struct hfi_enable *hfi;

	if (!pkt) {
		dprintk(VIDC_ERR, "No input packet\n");
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

static u32 get_hfi_buffer(int hal_buffer)
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
		dprintk(VIDC_ERR, "Invalid buffer: %#x\n",
				hal_buffer);
		buffer = 0;
		break;
	}
	return buffer;
}

static int get_hfi_extradata_index(enum hal_extradata_id index)
{
	int ret = 0;

	switch (index) {
	case HAL_EXTRADATA_INTERLACE_VIDEO:
		ret = HFI_PROPERTY_PARAM_VDEC_INTERLACE_VIDEO_EXTRADATA;
		break;
	case HAL_EXTRADATA_TIMESTAMP:
		ret = HFI_PROPERTY_PARAM_VDEC_TIMESTAMP_EXTRADATA;
		break;
	case HAL_EXTRADATA_S3D_FRAME_PACKING:
		ret = HFI_PROPERTY_PARAM_S3D_FRAME_PACKING_EXTRADATA;
		break;
	case HAL_EXTRADATA_FRAME_RATE:
		ret = HFI_PROPERTY_PARAM_VDEC_FRAME_RATE_EXTRADATA;
		break;
	case HAL_EXTRADATA_PANSCAN_WINDOW:
		ret = HFI_PROPERTY_PARAM_VDEC_PANSCAN_WNDW_EXTRADATA;
		break;
	case HAL_EXTRADATA_RECOVERY_POINT_SEI:
		ret = HFI_PROPERTY_PARAM_VDEC_RECOVERY_POINT_SEI_EXTRADATA;
		break;
	case HAL_EXTRADATA_NUM_CONCEALED_MB:
		ret = HFI_PROPERTY_PARAM_VDEC_NUM_CONCEALED_MB;
		break;
	case HAL_EXTRADATA_ASPECT_RATIO:
	case HAL_EXTRADATA_OUTPUT_CROP:
		ret = HFI_PROPERTY_PARAM_INDEX_EXTRADATA;
		break;
	case HAL_EXTRADATA_MPEG2_SEQDISP:
		ret = HFI_PROPERTY_PARAM_VDEC_MPEG2_SEQDISP_EXTRADATA;
		break;
	case HAL_EXTRADATA_STREAM_USERDATA:
		ret = HFI_PROPERTY_PARAM_VDEC_STREAM_USERDATA_EXTRADATA;
		break;
	case HAL_EXTRADATA_FRAME_QP:
		ret = HFI_PROPERTY_PARAM_VDEC_FRAME_QP_EXTRADATA;
		break;
	case HAL_EXTRADATA_LTR_INFO:
		ret = HFI_PROPERTY_PARAM_VENC_LTR_INFO;
		break;
	case HAL_EXTRADATA_ROI_QP:
		ret = HFI_PROPERTY_PARAM_VENC_ROI_QP_EXTRADATA;
		break;
	case HAL_EXTRADATA_MASTERING_DISPLAY_COLOUR_SEI:
		ret =
		HFI_PROPERTY_PARAM_VDEC_MASTERING_DISPLAY_COLOUR_SEI_EXTRADATA;
		break;
	case HAL_EXTRADATA_CONTENT_LIGHT_LEVEL_SEI:
		ret = HFI_PROPERTY_PARAM_VDEC_CONTENT_LIGHT_LEVEL_SEI_EXTRADATA;
		break;
	case HAL_EXTRADATA_VUI_DISPLAY_INFO:
		ret = HFI_PROPERTY_PARAM_VUI_DISPLAY_INFO_EXTRADATA;
		break;
	case HAL_EXTRADATA_VPX_COLORSPACE:
		ret = HFI_PROPERTY_PARAM_VDEC_VPX_COLORSPACE_EXTRADATA;
		break;
	case HAL_EXTRADATA_UBWC_CR_STATS_INFO:
		ret = HFI_PROPERTY_PARAM_VDEC_UBWC_CR_STAT_INFO_EXTRADATA;
		break;
	default:
		dprintk(VIDC_WARN, "Extradata index not found: %d\n", index);
		break;
	}
	return ret;
}

static int get_hfi_extradata_id(enum hal_extradata_id index)
{
	int ret = 0;

	switch (index) {
	case HAL_EXTRADATA_ASPECT_RATIO:
		ret = MSM_VIDC_EXTRADATA_ASPECT_RATIO;
		break;
	case HAL_EXTRADATA_OUTPUT_CROP:
		ret = MSM_VIDC_EXTRADATA_OUTPUT_CROP;
		break;
	default:
		ret = get_hfi_extradata_index(index);
		break;
	}
	return ret;
}

static u32 get_hfi_ltr_mode(enum ltr_mode ltr_mode_type)
{
	u32 ltrmode;

	switch (ltr_mode_type) {
	case HAL_LTR_MODE_DISABLE:
		ltrmode = HFI_LTR_MODE_DISABLE;
		break;
	case HAL_LTR_MODE_MANUAL:
		ltrmode = HFI_LTR_MODE_MANUAL;
		break;
	default:
		dprintk(VIDC_ERR, "Invalid ltr mode: %#x\n",
			ltr_mode_type);
		ltrmode = HFI_LTR_MODE_DISABLE;
		break;
	}
	return ltrmode;
}

static u32 get_hfi_work_mode(enum hal_work_mode work_mode)
{
	u32 hfi_work_mode;

	switch (work_mode) {
	case VIDC_WORK_MODE_1:
		hfi_work_mode = HFI_WORKMODE_1;
		break;
	case VIDC_WORK_MODE_2:
		hfi_work_mode = HFI_WORKMODE_2;
		break;
	default:
		dprintk(VIDC_ERR, "Invalid work mode: %#x\n",
			work_mode);
		hfi_work_mode = HFI_WORKMODE_2;
		break;
	}
	return hfi_work_mode;
}

int create_pkt_cmd_session_set_buffers(
		struct hfi_cmd_session_set_buffers_packet *pkt,
		struct hal_session *session,
		struct vidc_buffer_addr_info *buffer_info)
{
	int rc = 0;
	int i = 0;

	if (!pkt || !session)
		return -EINVAL;

	pkt->packet_type = HFI_CMD_SESSION_SET_BUFFERS;
	pkt->session_id = hash32_ptr(session);
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

	pkt->buffer_type = get_hfi_buffer(buffer_info->buffer_type);
	if (!pkt->buffer_type)
		return -EINVAL;

	return rc;
}

int create_pkt_cmd_session_release_buffers(
		struct hfi_cmd_session_release_buffer_packet *pkt,
		struct hal_session *session,
		struct vidc_buffer_addr_info *buffer_info)
{
	int rc = 0;
	int i = 0;

	if (!pkt || !session)
		return -EINVAL;

	pkt->packet_type = HFI_CMD_SESSION_RELEASE_BUFFERS;
	pkt->session_id = hash32_ptr(session);
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
	pkt->buffer_type = get_hfi_buffer(buffer_info->buffer_type);
	if (!pkt->buffer_type)
		return -EINVAL;
	return rc;
}

int create_pkt_cmd_session_register_buffer(
		struct hfi_cmd_session_register_buffers_packet *pkt,
		struct hal_session *session,
		struct vidc_register_buffer *buffer)
{
	int rc = 0, i;
	struct hfi_buffer_mapping_type *buf;

	if (!pkt || !session) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	pkt->packet_type = HFI_CMD_SESSION_REGISTER_BUFFERS;
	pkt->session_id = hash32_ptr(session);
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
		struct hal_session *session,
		struct vidc_unregister_buffer *buffer)
{
	int rc = 0, i;
	struct hfi_buffer_mapping_type *buf;

	if (!pkt || !session) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	pkt->packet_type = HFI_CMD_SESSION_UNREGISTER_BUFFERS;
	pkt->session_id = hash32_ptr(session);
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
	struct hal_session *session, struct vidc_frame_data *input_frame)
{
	int rc = 0;

	if (!pkt || !session)
		return -EINVAL;

	pkt->size =
		sizeof(struct hfi_cmd_session_empty_buffer_compressed_packet);
	pkt->packet_type = HFI_CMD_SESSION_EMPTY_BUFFER;
	pkt->session_id = hash32_ptr(session);
	pkt->time_stamp_hi = upper_32_bits(input_frame->timestamp);
	pkt->time_stamp_lo = lower_32_bits(input_frame->timestamp);
	pkt->flags = input_frame->flags;
	pkt->mark_target = input_frame->mark_target;
	pkt->mark_data = input_frame->mark_data;
	pkt->offset = input_frame->offset;
	pkt->alloc_len = input_frame->alloc_len;
	pkt->filled_len = input_frame->filled_len;
	pkt->input_tag = input_frame->clnt_data;
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
	struct hal_session *session, struct vidc_frame_data *input_frame)
{
	int rc = 0;

	if (!pkt || !session)
		return -EINVAL;

	pkt->size = sizeof(struct
		hfi_cmd_session_empty_buffer_uncompressed_plane0_packet);
	pkt->packet_type = HFI_CMD_SESSION_EMPTY_BUFFER;
	pkt->session_id = hash32_ptr(session);
	pkt->view_id = 0;
	pkt->time_stamp_hi = upper_32_bits(input_frame->timestamp);
	pkt->time_stamp_lo = lower_32_bits(input_frame->timestamp);
	pkt->flags = input_frame->flags;
	pkt->mark_target = input_frame->mark_target;
	pkt->mark_data = input_frame->mark_data;
	pkt->offset = input_frame->offset;
	pkt->alloc_len = input_frame->alloc_len;
	pkt->filled_len = input_frame->filled_len;
	pkt->input_tag = input_frame->clnt_data;
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
		struct hal_session *session,
		struct vidc_frame_data *output_frame)
{
	int rc = 0;

	if (!pkt || !session || !output_frame)
		return -EINVAL;

	pkt->size = sizeof(struct hfi_cmd_session_fill_buffer_packet);
	pkt->packet_type = HFI_CMD_SESSION_FILL_BUFFER;
	pkt->session_id = hash32_ptr(session);

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
		struct hal_session *session)
{
	int rc = 0;

	if (!pkt || !session)
		return -EINVAL;

	pkt->size = sizeof(struct hfi_cmd_session_get_property_packet);
	pkt->packet_type = HFI_CMD_SESSION_GET_PROPERTY;
	pkt->session_id = hash32_ptr(session);
	pkt->num_properties = 1;
	pkt->rg_property_data[0] = HFI_PROPERTY_CONFIG_BUFFER_REQUIREMENTS;

	return rc;
}

int create_pkt_cmd_session_flush(struct hfi_cmd_session_flush_packet *pkt,
			struct hal_session *session, enum hal_flush flush_mode)
{
	int rc = 0;

	if (!pkt || !session)
		return -EINVAL;

	pkt->size = sizeof(struct hfi_cmd_session_flush_packet);
	pkt->packet_type = HFI_CMD_SESSION_FLUSH;
	pkt->session_id = hash32_ptr(session);
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
		dprintk(VIDC_ERR, "Invalid flush mode: %#x\n", flush_mode);
		return -EINVAL;
	}
	return rc;
}

int create_pkt_cmd_session_get_property(
		struct hfi_cmd_session_get_property_packet *pkt,
		struct hal_session *session, enum hal_property ptype)
{
	/* Currently no get property is supported */
	dprintk(VIDC_ERR, "%s cmd:%#x not supported\n", __func__,
			ptype);
	return -EINVAL;
}

int create_pkt_cmd_session_set_property(
		struct hfi_cmd_session_set_property_packet *pkt,
		struct hal_session *session,
		enum hal_property ptype, void *pdata)
{
	int rc = 0;

	if (!pkt || !session)
		return -EINVAL;

	pkt->size = sizeof(struct hfi_cmd_session_set_property_packet);
	pkt->packet_type = HFI_CMD_SESSION_SET_PROPERTY;
	pkt->session_id = hash32_ptr(session);
	pkt->num_properties = 1;

	dprintk(VIDC_DBG, "Setting HAL Property = 0x%x\n", ptype);

	switch (ptype) {
	case HAL_CONFIG_FRAME_RATE:
	{
		u32 buffer_type;
		struct hfi_frame_rate *hfi;
		struct hal_frame_rate *prop = (struct hal_frame_rate *) pdata;

		pkt->rg_property_data[0] = HFI_PROPERTY_CONFIG_FRAME_RATE;
		hfi = (struct hfi_frame_rate *) &pkt->rg_property_data[1];
		buffer_type = get_hfi_buffer(prop->buffer_type);
		if (buffer_type)
			hfi->buffer_type = buffer_type;
		else
			return -EINVAL;

		hfi->frame_rate = prop->frame_rate;
		pkt->size += sizeof(struct hfi_frame_rate);
		break;
	}
	case HAL_CONFIG_OPERATING_RATE:
	{
		struct hfi_operating_rate *hfi;
		struct hal_operating_rate *prop =
			(struct hal_operating_rate *) pdata;

		pkt->rg_property_data[0] = HFI_PROPERTY_CONFIG_OPERATING_RATE;
		hfi = (struct hfi_operating_rate *) &pkt->rg_property_data[1];
		hfi->operating_rate = prop->operating_rate;
		pkt->size += sizeof(struct hfi_operating_rate);
		break;
	}
	case HAL_PARAM_UNCOMPRESSED_FORMAT_SELECT:
	{
		u32 buffer_type;
		struct hfi_uncompressed_format_select *hfi;
		struct hal_uncompressed_format_select *prop =
			(struct hal_uncompressed_format_select *) pdata;

		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_UNCOMPRESSED_FORMAT_SELECT;

		hfi = (struct hfi_uncompressed_format_select *)
					&pkt->rg_property_data[1];
		buffer_type = get_hfi_buffer(prop->buffer_type);
		if (buffer_type)
			hfi->buffer_type = buffer_type;
		else
			return -EINVAL;
		hfi->format = hal_to_hfi_type(
				HAL_PARAM_UNCOMPRESSED_FORMAT_SELECT,
				prop->format);
		pkt->size += sizeof(struct hfi_uncompressed_format_select);
		break;
	}
	case HAL_PARAM_UNCOMPRESSED_PLANE_ACTUAL_CONSTRAINTS_INFO:
	{
		struct hfi_uncompressed_plane_actual_constraints_info *hfi;
		struct hal_uncompressed_plane_actual_constraints_info *prop =
		(struct hal_uncompressed_plane_actual_constraints_info *) pdata;
		u32 buffer_type;
		u32 num_plane = prop->num_planes;
		u32 hfi_pkt_size =
			2 * sizeof(u32)
			+ num_plane
			* sizeof(struct hal_uncompressed_plane_constraints);

		pkt->rg_property_data[0] =
		HFI_PROPERTY_PARAM_UNCOMPRESSED_PLANE_ACTUAL_CONSTRAINTS_INFO;

		hfi = (struct hfi_uncompressed_plane_actual_constraints_info *)
					&pkt->rg_property_data[1];
		buffer_type = get_hfi_buffer(prop->buffer_type);
		if (buffer_type)
			hfi->buffer_type = buffer_type;
		else
			return -EINVAL;

		hfi->num_planes = prop->num_planes;
		memcpy(hfi->rg_plane_format, prop->rg_plane_format,
			hfi->num_planes
			*sizeof(struct hal_uncompressed_plane_constraints));
		pkt->size += hfi_pkt_size;
		break;
	}
	case HAL_PARAM_UNCOMPRESSED_PLANE_ACTUAL_INFO:
		break;
	case HAL_PARAM_FRAME_SIZE:
	{
		struct hfi_frame_size *hfi;
		struct hal_frame_size *prop = (struct hal_frame_size *) pdata;
		u32 buffer_type;

		pkt->rg_property_data[0] = HFI_PROPERTY_PARAM_FRAME_SIZE;
		hfi = (struct hfi_frame_size *) &pkt->rg_property_data[1];
		buffer_type = get_hfi_buffer(prop->buffer_type);
		if (buffer_type)
			hfi->buffer_type = buffer_type;
		else
			return -EINVAL;

		hfi->height = prop->height;
		hfi->width = prop->width;
		pkt->size += sizeof(struct hfi_frame_size);
		break;
	}
	case HAL_CONFIG_REALTIME:
	{
		create_pkt_enable(pkt->rg_property_data,
			HFI_PROPERTY_CONFIG_REALTIME,
			(((struct hal_enable *) pdata)->enable));
		pkt->size += sizeof(u32);
		break;
	}
	case HAL_PARAM_BUFFER_COUNT_ACTUAL:
	{
		struct hfi_buffer_count_actual *hfi;
		struct hal_buffer_count_actual *prop =
			(struct hal_buffer_count_actual *) pdata;
		u32 buffer_type;

		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_BUFFER_COUNT_ACTUAL;
		hfi = (struct hfi_buffer_count_actual *)
			&pkt->rg_property_data[1];
		hfi->buffer_count_actual = prop->buffer_count_actual;
		hfi->buffer_count_min_host = prop->buffer_count_min_host;

		buffer_type = get_hfi_buffer(prop->buffer_type);
		if (buffer_type)
			hfi->buffer_type = buffer_type;
		else
			return -EINVAL;

		pkt->size += sizeof(struct hfi_buffer_count_actual);

		break;
	}
	case HAL_PARAM_NAL_STREAM_FORMAT_SELECT:
	{
		struct hfi_nal_stream_format_select *hfi;
		struct hal_nal_stream_format_select *prop =
			(struct hal_nal_stream_format_select *)pdata;

		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_NAL_STREAM_FORMAT_SELECT;
		hfi = (struct hfi_nal_stream_format_select *)
			&pkt->rg_property_data[1];
		dprintk(VIDC_DBG, "data is :%d\n",
				prop->nal_stream_format_select);
		hfi->nal_stream_format_select = hal_to_hfi_type(
				HAL_PARAM_NAL_STREAM_FORMAT_SELECT,
				prop->nal_stream_format_select);
		pkt->size += sizeof(struct hfi_nal_stream_format_select);
		break;
	}
	case HAL_PARAM_VDEC_OUTPUT_ORDER:
	{
		int *data = (int *) pdata;

		pkt->rg_property_data[0] =
				HFI_PROPERTY_PARAM_VDEC_OUTPUT_ORDER;
		switch (*data) {
		case HAL_OUTPUT_ORDER_DECODE:
			pkt->rg_property_data[1] = HFI_OUTPUT_ORDER_DECODE;
			break;
		case HAL_OUTPUT_ORDER_DISPLAY:
			pkt->rg_property_data[1] = HFI_OUTPUT_ORDER_DISPLAY;
			break;
		default:
			dprintk(VIDC_ERR, "invalid output order: %#x\n",
						  *data);
			break;
		}
		pkt->size += sizeof(u32);
		break;
	}
	case HAL_PARAM_VDEC_PICTURE_TYPE_DECODE:
	{
		struct hfi_enable_picture *hfi;

		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VDEC_PICTURE_TYPE_DECODE;
		hfi = (struct hfi_enable_picture *) &pkt->rg_property_data[1];
		hfi->picture_type =
			((struct hfi_enable_picture *)pdata)->picture_type;
		pkt->size += sizeof(u32);
		break;
	}
	case HAL_PARAM_VDEC_OUTPUT2_KEEP_ASPECT_RATIO:
	{
		create_pkt_enable(pkt->rg_property_data,
			HFI_PROPERTY_PARAM_VDEC_OUTPUT2_KEEP_ASPECT_RATIO,
			((struct hal_enable *)pdata)->enable);
		pkt->size += sizeof(u32);
		break;
	}
	case HAL_PARAM_VDEC_MULTI_STREAM:
	{
		struct hfi_multi_stream *hfi;
		struct hal_multi_stream *prop =
			(struct hal_multi_stream *) pdata;
		u32 buffer_type;

		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VDEC_MULTI_STREAM;
		hfi = (struct hfi_multi_stream *) &pkt->rg_property_data[1];

		buffer_type = get_hfi_buffer(prop->buffer_type);
		if (buffer_type)
			hfi->buffer_type = buffer_type;
		else
			return -EINVAL;
		hfi->enable = prop->enable;
		pkt->size += sizeof(struct hfi_multi_stream);
		break;
	}
	case HAL_CONFIG_VDEC_MB_ERROR_MAP_REPORTING:
	{
		create_pkt_enable(pkt->rg_property_data,
			HFI_PROPERTY_CONFIG_VDEC_MB_ERROR_MAP_REPORTING,
			((struct hal_enable *)pdata)->enable);
		pkt->size += sizeof(u32);
		break;
	}
	case HAL_PARAM_VDEC_SYNC_FRAME_DECODE:
	{
		create_pkt_enable(pkt->rg_property_data,
			HFI_PROPERTY_PARAM_VDEC_THUMBNAIL_MODE,
			((struct hal_enable *)pdata)->enable);
		pkt->size += sizeof(u32);
		break;
	}
	case HAL_PARAM_SECURE:
	{
		create_pkt_enable(pkt->rg_property_data,
			  HFI_PROPERTY_PARAM_SECURE_SESSION,
			  ((struct hal_enable *)pdata)->enable);
		pkt->size += sizeof(u32);
		break;
	}
	case HAL_PARAM_VENC_SYNC_FRAME_SEQUENCE_HEADER:
	{
		create_pkt_enable(pkt->rg_property_data,
			HFI_PROPERTY_CONFIG_VENC_SYNC_FRAME_SEQUENCE_HEADER,
			((struct hal_enable *)pdata)->enable);
		pkt->size += sizeof(u32);
		break;
	}
	case HAL_CONFIG_VENC_REQUEST_IFRAME:
		pkt->rg_property_data[0] =
			HFI_PROPERTY_CONFIG_VENC_REQUEST_SYNC_FRAME;
		break;
	case HAL_CONFIG_VENC_TARGET_BITRATE:
	{
		struct hfi_bitrate *hfi;

		pkt->rg_property_data[0] =
			HFI_PROPERTY_CONFIG_VENC_TARGET_BITRATE;
		hfi = (struct hfi_bitrate *) &pkt->rg_property_data[1];
		hfi->bit_rate = ((struct hal_bitrate *)pdata)->bit_rate;
		hfi->layer_id = ((struct hal_bitrate *)pdata)->layer_id;
		pkt->size += sizeof(struct hfi_bitrate);
		break;
	}
	case HAL_PARAM_PROFILE_LEVEL_CURRENT:
	{
		struct hfi_profile_level *hfi;
		struct hal_profile_level *prop =
			(struct hal_profile_level *) pdata;

		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_PROFILE_LEVEL_CURRENT;
		hfi = (struct hfi_profile_level *)
			&pkt->rg_property_data[1];

		/* There is an assumption here that HAL level is same as
		 * HFI level
		 */
		hfi->level = prop->level;
		hfi->profile = prop->profile;
		if (hfi->profile <= 0) {
			hfi->profile = HFI_H264_PROFILE_HIGH;
			dprintk(VIDC_WARN,
					"Profile %d not supported, falling back to high\n",
					prop->profile);
		}

		pkt->size += sizeof(struct hfi_profile_level);
		break;
	}
	case HAL_PARAM_VENC_H264_ENTROPY_CONTROL:
	{
		struct hfi_h264_entropy_control *hfi;
		struct hal_h264_entropy_control *prop =
			(struct hal_h264_entropy_control *) pdata;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_H264_ENTROPY_CONTROL;
		hfi = (struct hfi_h264_entropy_control *)
			&pkt->rg_property_data[1];
		hfi->entropy_mode = hal_to_hfi_type(
		   HAL_PARAM_VENC_H264_ENTROPY_CONTROL,
		   prop->entropy_mode);

		hfi->cabac_model = HFI_H264_CABAC_MODEL_0;
		pkt->size += sizeof(struct hfi_h264_entropy_control);
		break;
	}
	case HAL_PARAM_VENC_RATE_CONTROL:
	{
		u32 *rc;

		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_RATE_CONTROL;
		rc = (u32 *)pdata;
		switch ((enum hal_rate_control) *rc) {
		case HAL_RATE_CONTROL_OFF:
			pkt->rg_property_data[1] = HFI_RATE_CONTROL_OFF;
			break;
		case HAL_RATE_CONTROL_CBR:
			pkt->rg_property_data[1] = HFI_RATE_CONTROL_CBR_CFR;
			break;
		case HAL_RATE_CONTROL_VBR:
			pkt->rg_property_data[1] = HFI_RATE_CONTROL_VBR_CFR;
			break;
		case HAL_RATE_CONTROL_MBR:
			pkt->rg_property_data[1] = HFI_RATE_CONTROL_MBR_CFR;
			break;
		case HAL_RATE_CONTROL_CBR_VFR:
			pkt->rg_property_data[1] = HFI_RATE_CONTROL_CBR_VFR;
			break;
		case HAL_RATE_CONTROL_MBR_VFR:
			pkt->rg_property_data[1] = HFI_RATE_CONTROL_MBR_VFR;
			break;
		default:
			dprintk(VIDC_ERR,
					"Invalid Rate control setting: %pK\n",
					pdata);
			break;
		}
		pkt->size += sizeof(u32);
		break;
	}
	case HAL_PARAM_VENC_H264_DEBLOCK_CONTROL:
	{
		struct hfi_h264_db_control *hfi;
		struct hal_h264_db_control *prop =
			(struct hal_h264_db_control *) pdata;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_H264_DEBLOCK_CONTROL;
		hfi = (struct hfi_h264_db_control *) &pkt->rg_property_data[1];
		switch (prop->mode) {
		case HAL_H264_DB_MODE_DISABLE:
			hfi->mode = HFI_H264_DB_MODE_DISABLE;
			break;
		case HAL_H264_DB_MODE_SKIP_SLICE_BOUNDARY:
			hfi->mode = HFI_H264_DB_MODE_SKIP_SLICE_BOUNDARY;
			break;
		case HAL_H264_DB_MODE_ALL_BOUNDARY:
			hfi->mode = HFI_H264_DB_MODE_ALL_BOUNDARY;
			break;
		default:
			dprintk(VIDC_ERR, "Invalid deblocking mode: %#x\n",
						  prop->mode);
			break;
		}
		hfi->slice_alpha_offset = prop->slice_alpha_offset;
		hfi->slice_beta_offset = prop->slice_beta_offset;
		pkt->size += sizeof(struct hfi_h264_db_control);
		break;
	}
	case HAL_CONFIG_VENC_FRAME_QP:
	{
		struct hfi_quantization *hfi;
		struct hal_quantization *hal_quant =
			(struct hal_quantization *) pdata;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_CONFIG_VENC_FRAME_QP;
		hfi = (struct hfi_quantization *) &pkt->rg_property_data[1];
		hfi->qp_packed = hal_quant->qpi | hal_quant->qpp << 8 |
			hal_quant->qpb << 16;
		hfi->layer_id = hal_quant->layer_id;
		hfi->enable = hal_quant->enable;
		pkt->size += sizeof(struct hfi_quantization);
		break;
	}
	case HAL_PARAM_VENC_SESSION_QP_RANGE:
	{
		struct hfi_quantization_range *hfi;
		struct hal_quantization_range *hal_range =
			(struct hal_quantization_range *) pdata;

		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_SESSION_QP_RANGE;
		hfi = (struct hfi_quantization_range *)
				&pkt->rg_property_data[1];

		/*
		 * When creating the packet, pack the qp value as
		 * 0xbbppii, where ii = qp range for I-frames,
		 * pp = qp range for P-frames, etc.
		 */
		hfi->min_qp.qp_packed = hal_range->qpi_min |
			hal_range->qpp_min << 8 |
			hal_range->qpb_min << 16;
		hfi->max_qp.qp_packed = hal_range->qpi_max |
			hal_range->qpp_max << 8 |
			hal_range->qpb_max << 16;
		hfi->max_qp.layer_id = hal_range->layer_id;
		hfi->min_qp.layer_id = hal_range->layer_id;

		pkt->size += sizeof(struct hfi_quantization_range);
		break;
	}
	case HAL_CONFIG_VENC_INTRA_PERIOD:
	{
		struct hfi_intra_period *hfi;

		pkt->rg_property_data[0] =
			HFI_PROPERTY_CONFIG_VENC_INTRA_PERIOD;
		hfi = (struct hfi_intra_period *) &pkt->rg_property_data[1];
		memcpy(hfi, (struct hfi_intra_period *) pdata,
				sizeof(struct hfi_intra_period));
		pkt->size += sizeof(struct hfi_intra_period);

		if (hfi->bframes) {
			struct hfi_enable *hfi_enable;
			u32 *prop_type;

			prop_type = (u32 *)((u8 *)&pkt->rg_property_data[0] +
				sizeof(u32) + sizeof(struct hfi_intra_period));
			*prop_type =  HFI_PROPERTY_PARAM_VENC_ADAPTIVE_B;
			hfi_enable = (struct hfi_enable *)(prop_type + 1);
			hfi_enable->enable = true;
			pkt->num_properties = 2;
			pkt->size += sizeof(struct hfi_enable) + sizeof(u32);
		}
		break;
	}
	case HAL_CONFIG_VENC_IDR_PERIOD:
	{
		struct hfi_idr_period *hfi;

		pkt->rg_property_data[0] = HFI_PROPERTY_CONFIG_VENC_IDR_PERIOD;
		hfi = (struct hfi_idr_period *) &pkt->rg_property_data[1];
		hfi->idr_period = ((struct hfi_idr_period *) pdata)->idr_period;
		pkt->size += sizeof(u32);
		break;
	}
	case HAL_PARAM_VENC_ADAPTIVE_B:
	{
		create_pkt_enable(pkt->rg_property_data,
			HFI_PROPERTY_PARAM_VENC_ADAPTIVE_B,
			((struct hal_enable *)pdata)->enable);
		pkt->size += sizeof(struct hfi_enable);
		break;
	}
	case HAL_PARAM_VDEC_CONCEAL_COLOR:
	{
		struct hfi_conceal_color *hfi;

		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VDEC_CONCEAL_COLOR;
		hfi = (struct hfi_conceal_color *) &pkt->rg_property_data[1];
		if (hfi) {
			hfi->conceal_color_8bit =
				((struct hfi_conceal_color *) pdata)->
				conceal_color_8bit;
			hfi->conceal_color_10bit =
				((struct hfi_conceal_color *) pdata)->
				conceal_color_10bit;
		}
		pkt->size += sizeof(struct hfi_conceal_color);
		break;
	}
	case HAL_PARAM_VPE_ROTATION:
	{
		struct hfi_vpe_rotation_type *hfi;
		struct hal_vpe_rotation *prop =
			(struct hal_vpe_rotation *) pdata;
		pkt->rg_property_data[0] = HFI_PROPERTY_PARAM_VPE_ROTATION;
		hfi = (struct hfi_vpe_rotation_type *)&pkt->rg_property_data[1];
		switch (prop->rotate) {
		case 0:
			hfi->rotation = HFI_ROTATE_NONE;
			break;
		case 90:
			hfi->rotation = HFI_ROTATE_90;
			break;
		case 180:
			hfi->rotation = HFI_ROTATE_180;
			break;
		case 270:
			hfi->rotation = HFI_ROTATE_270;
			break;
		default:
			dprintk(VIDC_ERR, "Invalid rotation setting: %#x\n",
				prop->rotate);
			rc = -EINVAL;
			break;
		}
		switch (prop->flip) {
		case HAL_FLIP_NONE:
			hfi->flip = HFI_FLIP_NONE;
			break;
		case HAL_FLIP_HORIZONTAL:
			hfi->flip = HFI_FLIP_HORIZONTAL;
			break;
		case HAL_FLIP_VERTICAL:
			hfi->flip = HFI_FLIP_VERTICAL;
			break;
		case HAL_FLIP_BOTH:
			hfi->flip = HFI_FLIP_HORIZONTAL | HFI_FLIP_VERTICAL;
			break;
		default:
			dprintk(VIDC_ERR, "Invalid flip setting: %#x\n",
				prop->flip);
			rc = -EINVAL;
			break;
		}
		pkt->size += sizeof(struct hfi_vpe_rotation_type);
		break;
	}
	case HAL_PARAM_VENC_INTRA_REFRESH:
	{
		struct hfi_intra_refresh *hfi;
		struct hal_intra_refresh *prop =
			(struct hal_intra_refresh *) pdata;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_INTRA_REFRESH;
		hfi = (struct hfi_intra_refresh *) &pkt->rg_property_data[1];
		hfi->mbs = 0;
		switch (prop->mode) {
		case HAL_INTRA_REFRESH_NONE:
			hfi->mode = HFI_INTRA_REFRESH_NONE;
			break;
		case HAL_INTRA_REFRESH_CYCLIC:
			hfi->mode = HFI_INTRA_REFRESH_CYCLIC;
			hfi->mbs = prop->ir_mbs;
			break;
		case HAL_INTRA_REFRESH_RANDOM:
			hfi->mode = HFI_INTRA_REFRESH_RANDOM;
			hfi->mbs = prop->ir_mbs;
			break;
		default:
			dprintk(VIDC_ERR,
					"Invalid intra refresh setting: %#x\n",
					prop->mode);
			break;
		}
		pkt->size += sizeof(struct hfi_intra_refresh);
		break;
	}
	case HAL_PARAM_VENC_MULTI_SLICE_CONTROL:
	{
		struct hfi_multi_slice_control *hfi;
		struct hal_multi_slice_control *prop =
			(struct hal_multi_slice_control *) pdata;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_MULTI_SLICE_CONTROL;
		hfi = (struct hfi_multi_slice_control *)
			&pkt->rg_property_data[1];
		switch (prop->multi_slice) {
		case HAL_MULTI_SLICE_OFF:
			hfi->multi_slice = HFI_MULTI_SLICE_OFF;
			break;
		case HAL_MULTI_SLICE_BY_MB_COUNT:
			hfi->multi_slice = HFI_MULTI_SLICE_BY_MB_COUNT;
			break;
		case HAL_MULTI_SLICE_BY_BYTE_COUNT:
			hfi->multi_slice = HFI_MULTI_SLICE_BY_BYTE_COUNT;
			break;
		default:
			dprintk(VIDC_ERR, "Invalid slice settings: %#x\n",
				prop->multi_slice);
			break;
		}
		hfi->slice_size = prop->slice_size;
		pkt->size += sizeof(struct
					hfi_multi_slice_control);
		break;
	}
	case HAL_PARAM_INDEX_EXTRADATA:
	{
		struct hfi_index_extradata_config *hfi;
		struct hal_extradata_enable *extra = pdata;
		int id = 0;

		pkt->rg_property_data[0] =
			get_hfi_extradata_index(extra->index);
		hfi = (struct hfi_index_extradata_config *)
			&pkt->rg_property_data[1];
		hfi->enable = extra->enable;
		id = get_hfi_extradata_id(extra->index);
		if (id)
			hfi->index_extra_data_id = id;
		else {
			dprintk(VIDC_WARN,
				"Failed to find extradata id: %d\n",
				id);
			rc = -EINVAL;
		}
		pkt->size += sizeof(struct hfi_index_extradata_config);
		break;
	}
	case HAL_PARAM_VENC_SLICE_DELIVERY_MODE:
	{
		create_pkt_enable(pkt->rg_property_data,
				HFI_PROPERTY_PARAM_VENC_SLICE_DELIVERY_MODE,
				((struct hal_enable *)pdata)->enable);
		pkt->size += sizeof(struct hfi_enable);
		break;
	}
	case HAL_PARAM_VENC_VUI_TIMING_INFO:
	{
		struct hfi_vui_timing_info *hfi;
		struct hal_vui_timing_info *timing_info = pdata;

		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_VUI_TIMING_INFO;

		hfi = (struct hfi_vui_timing_info *)&pkt->rg_property_data[1];
		hfi->enable = timing_info->enable;
		hfi->fixed_frame_rate = timing_info->fixed_frame_rate;
		hfi->time_scale = timing_info->time_scale;

		pkt->size += sizeof(struct hfi_vui_timing_info);
		break;
	}
	case HAL_PARAM_VENC_GENERATE_AUDNAL:
	{
		create_pkt_enable(pkt->rg_property_data,
				HFI_PROPERTY_PARAM_VENC_GENERATE_AUDNAL,
				((struct hal_enable *)pdata)->enable);
		pkt->size += sizeof(struct hfi_enable);
		break;
	}
	case HAL_PARAM_VENC_PRESERVE_TEXT_QUALITY:
	{
		create_pkt_enable(pkt->rg_property_data,
				HFI_PROPERTY_PARAM_VENC_PRESERVE_TEXT_QUALITY,
				((struct hal_enable *)pdata)->enable);
		pkt->size += sizeof(struct hfi_enable);
		break;
	}
	case HAL_PARAM_VENC_LTRMODE:
	{
		struct hfi_ltr_mode *hfi;
		struct hal_ltr_mode *hal = pdata;

		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_LTRMODE;
		hfi = (struct hfi_ltr_mode *) &pkt->rg_property_data[1];
		hfi->ltr_mode = get_hfi_ltr_mode(hal->mode);
		hfi->ltr_count = hal->count;
		hfi->trust_mode = hal->trust_mode;
		pkt->size += sizeof(struct hfi_ltr_mode);
		break;
	}
	case HAL_CONFIG_VENC_USELTRFRAME:
	{
		struct hfi_ltr_use *hfi;
		struct hal_ltr_use *hal = pdata;

		pkt->rg_property_data[0] =
			HFI_PROPERTY_CONFIG_VENC_USELTRFRAME;
		hfi = (struct hfi_ltr_use *) &pkt->rg_property_data[1];
		hfi->frames = hal->frames;
		hfi->ref_ltr = hal->ref_ltr;
		hfi->use_constrnt = hal->use_constraint;
		pkt->size += sizeof(struct hfi_ltr_use);
		break;
	}
	case HAL_CONFIG_VENC_MARKLTRFRAME:
	{
		struct hfi_ltr_mark *hfi;
		struct hal_ltr_mark *hal = pdata;

		pkt->rg_property_data[0] =
			HFI_PROPERTY_CONFIG_VENC_MARKLTRFRAME;
		hfi = (struct hfi_ltr_mark *) &pkt->rg_property_data[1];
		hfi->mark_frame = hal->mark_frame;
		pkt->size += sizeof(struct hfi_ltr_mark);
		break;
	}
	case HAL_PARAM_VENC_HIER_P_MAX_ENH_LAYERS:
	{
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_HIER_P_MAX_NUM_ENH_LAYER;
		pkt->rg_property_data[1] = *(u32 *)pdata;
		pkt->size += sizeof(u32);
		break;
	}
	case HAL_CONFIG_VENC_HIER_P_NUM_FRAMES:
	{
		pkt->rg_property_data[0] =
			HFI_PROPERTY_CONFIG_VENC_HIER_P_ENH_LAYER;
		pkt->rg_property_data[1] = *(u32 *)pdata;
		pkt->size += sizeof(u32);
		break;
	}
	case HAL_PARAM_VENC_DISABLE_RC_TIMESTAMP:
	{
		create_pkt_enable(pkt->rg_property_data,
				HFI_PROPERTY_PARAM_VENC_DISABLE_RC_TIMESTAMP,
				((struct hal_enable *)pdata)->enable);
		pkt->size += sizeof(struct hfi_enable);
		break;
	}
	case HAL_PARAM_VPE_COLOR_SPACE_CONVERSION:
	{
		struct hfi_vpe_color_space_conversion *hfi = NULL;
		struct hal_vpe_color_space_conversion *hal = pdata;

		pkt->rg_property_data[0] =
				HFI_PROPERTY_PARAM_VPE_COLOR_SPACE_CONVERSION;
		hfi = (struct hfi_vpe_color_space_conversion *)
			&pkt->rg_property_data[1];

		hfi->input_color_primaries = hal->input_color_primaries;
		if (hal->custom_matrix_enabled)
			/* Bit Mask to enable all custom values */
			hfi->custom_matrix_enabled = 0x7;
		else
			hfi->custom_matrix_enabled = 0x0;
		memcpy(hfi->csc_matrix, hal->csc_matrix,
				sizeof(hfi->csc_matrix));
		memcpy(hfi->csc_bias, hal->csc_bias, sizeof(hfi->csc_bias));
		memcpy(hfi->csc_limit, hal->csc_limit, sizeof(hfi->csc_limit));
		pkt->size += sizeof(struct hfi_vpe_color_space_conversion);
		break;
	}
	case HAL_PARAM_VENC_VPX_ERROR_RESILIENCE_MODE:
	{
		create_pkt_enable(pkt->rg_property_data,
			HFI_PROPERTY_PARAM_VENC_VPX_ERROR_RESILIENCE_MODE,
			((struct hal_enable *)pdata)->enable);
		pkt->size += sizeof(struct hfi_enable);
		break;
	}
	case HAL_CONFIG_VENC_PERF_MODE:
	{
		u32 hfi_perf_mode = 0;
		enum hal_perf_mode hal_perf_mode = *(enum hal_perf_mode *)pdata;

		switch (hal_perf_mode) {
		case HAL_PERF_MODE_POWER_SAVE:
			hfi_perf_mode = HFI_VENC_PERFMODE_POWER_SAVE;
			break;
		case HAL_PERF_MODE_POWER_MAX_QUALITY:
			hfi_perf_mode = HFI_VENC_PERFMODE_MAX_QUALITY;
			break;
		default:
			return -ENOTSUPP;
		}

		pkt->rg_property_data[0] = HFI_PROPERTY_CONFIG_VENC_PERF_MODE;
		pkt->rg_property_data[1] = hfi_perf_mode;
		pkt->size += sizeof(u32);
		break;
	}
	case HAL_PARAM_VENC_HIER_P_HYBRID_MODE:
	{
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_HIER_P_HYBRID_MODE;
		pkt->rg_property_data[1] =
			((struct hfi_hybrid_hierp *)pdata)->layers ?: 0xFF;
		pkt->size += sizeof(u32) +
			sizeof(struct hfi_hybrid_hierp);
		break;
	}
	case HAL_PARAM_VENC_MBI_STATISTICS_MODE:
	{
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_MBI_DUMPING;
		pkt->rg_property_data[1] = hal_to_hfi_type(
			HAL_PARAM_VENC_MBI_STATISTICS_MODE,
				*(u32 *)pdata);
		pkt->size += sizeof(u32);
		break;
	}
	case HAL_CONFIG_VENC_BASELAYER_PRIORITYID:
	{
		pkt->rg_property_data[0] =
			HFI_PROPERTY_CONFIG_VENC_BASELAYER_PRIORITYID;
		pkt->rg_property_data[1] = *(u32 *)pdata;
		pkt->size += sizeof(u32);
		break;
	}
	case HAL_PROPERTY_PARAM_VENC_ASPECT_RATIO:
	{
		struct hfi_aspect_ratio *hfi = NULL;
		struct hal_aspect_ratio *hal = pdata;

		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_ASPECT_RATIO;
		hfi = (struct hfi_aspect_ratio *)
			&pkt->rg_property_data[1];
		memcpy(hfi, hal,
			sizeof(struct hfi_aspect_ratio));
		pkt->size += sizeof(struct hfi_aspect_ratio);
		break;
	}
	case HAL_PARAM_VENC_BITRATE_TYPE:
	{
		create_pkt_enable(pkt->rg_property_data,
			HFI_PROPERTY_PARAM_VENC_BITRATE_TYPE,
			((struct hal_enable *)pdata)->enable);
		pkt->size += sizeof(struct hfi_enable);
		break;
	}
	case HAL_PARAM_VENC_H264_TRANSFORM_8x8:
	{
		create_pkt_enable(pkt->rg_property_data,
			HFI_PROPERTY_PARAM_VENC_H264_8X8_TRANSFORM,
			((struct hal_enable *)pdata)->enable);
		pkt->size += sizeof(struct hfi_enable);
		break;
	}
	case HAL_PARAM_VENC_VIDEO_SIGNAL_INFO:
	{
		struct hal_video_signal_info *hal = pdata;
		struct hfi_video_signal_metadata *signal_info =
			(struct hfi_video_signal_metadata *)
			&pkt->rg_property_data[1];

		signal_info->enable = true;
		signal_info->video_format = MSM_VIDC_NTSC;
		signal_info->video_full_range = hal->full_range;
		signal_info->color_description = MSM_VIDC_COLOR_DESC_PRESENT;
		signal_info->color_primaries = hal->color_space;
		signal_info->transfer_characteristics = hal->transfer_chars;
		signal_info->matrix_coeffs = hal->matrix_coeffs;

		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_VIDEO_SIGNAL_INFO;
		pkt->size += sizeof(*signal_info);
		break;
	}
	case HAL_PARAM_VENC_IFRAMESIZE_TYPE:
	{
		enum hal_iframesize_type hal =
			*(enum hal_iframesize_type *)pdata;
		struct hfi_iframe_size *hfi = (struct hfi_iframe_size *)
			&pkt->rg_property_data[1];

		switch (hal) {
		case HAL_IFRAMESIZE_TYPE_DEFAULT:
			hfi->type = HFI_IFRAME_SIZE_DEFAULT;
			break;
		case HAL_IFRAMESIZE_TYPE_MEDIUM:
			hfi->type = HFI_IFRAME_SIZE_MEDIUM;
			break;
		case HAL_IFRAMESIZE_TYPE_HUGE:
			hfi->type = HFI_IFRAME_SIZE_HIGH;
			break;
		case HAL_IFRAMESIZE_TYPE_UNLIMITED:
			hfi->type = HFI_IFRAME_SIZE_UNLIMITED;
			break;
		default:
			return -ENOTSUPP;
		}
		pkt->rg_property_data[0] = HFI_PROPERTY_PARAM_VENC_IFRAMESIZE;
		pkt->size += sizeof(struct hfi_iframe_size);
		break;
	}
	case HAL_PARAM_BUFFER_SIZE_MINIMUM:
	{
		struct hfi_buffer_size_minimum *hfi;
		struct hal_buffer_size_minimum *prop =
			(struct hal_buffer_size_minimum *) pdata;
		u32 buffer_type;

		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_BUFFER_SIZE_MINIMUM;

		hfi = (struct hfi_buffer_size_minimum *)
			&pkt->rg_property_data[1];
		hfi->buffer_size = prop->buffer_size;

		buffer_type = get_hfi_buffer(prop->buffer_type);
		if (buffer_type)
			hfi->buffer_type = buffer_type;
		else
			return -EINVAL;

		pkt->size += sizeof(struct hfi_buffer_size_minimum);
		break;
	}
	case HAL_PARAM_SYNC_BASED_INTERRUPT:
	{
		create_pkt_enable(pkt->rg_property_data,
			HFI_PROPERTY_PARAM_SYNC_BASED_INTERRUPT,
			((struct hal_enable *)pdata)->enable);
		pkt->size += sizeof(struct hfi_enable);
		break;
	}
	case HAL_PARAM_VENC_LOW_LATENCY:
	{
		struct hfi_enable *hfi;

		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_LOW_LATENCY_MODE;
		hfi = (struct hfi_enable *) &pkt->rg_property_data[1];
		hfi->enable = ((struct hal_enable *) pdata)->enable;
		pkt->size += sizeof(u32);
		break;
	}
	case HAL_CONFIG_VENC_BLUR_RESOLUTION:
	{
		struct hfi_frame_size *hfi;
		struct hal_frame_size *prop = (struct hal_frame_size *) pdata;
		u32 buffer_type;

		pkt->rg_property_data[0] =
			HFI_PROPERTY_CONFIG_VENC_BLUR_FRAME_SIZE;
		hfi = (struct hfi_frame_size *) &pkt->rg_property_data[1];
		buffer_type = get_hfi_buffer(prop->buffer_type);
		if (buffer_type)
			hfi->buffer_type = buffer_type;
		else
			return -EINVAL;

		hfi->height = prop->height;
		hfi->width = prop->width;
		pkt->size += sizeof(struct hfi_frame_size);
		break;
	}
	case HAL_PARAM_VIDEO_CORES_USAGE:
	{
		struct hal_videocores_usage_info *hal = pdata;
		struct hfi_videocores_usage_type *core_info =
			(struct hfi_videocores_usage_type *)
			&pkt->rg_property_data[1];

		core_info->video_core_enable_mask = hal->video_core_enable_mask;

		pkt->rg_property_data[0] =
			HFI_PROPERTY_CONFIG_VIDEOCORES_USAGE;
		pkt->size += sizeof(*core_info);
		break;
	}
	case HAL_PARAM_VIDEO_WORK_MODE:
	{
		struct hal_video_work_mode *hal = pdata;
		struct hfi_video_work_mode *work_mode =
			(struct hfi_video_work_mode *)
			&pkt->rg_property_data[1];

		work_mode->video_work_mode = get_hfi_work_mode(
						hal->video_work_mode);

		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_WORK_MODE;
		pkt->size += sizeof(*work_mode);
		break;
	}
	case HAL_PARAM_VIDEO_WORK_ROUTE:
	{
		struct hal_video_work_route *hal = pdata;
		struct hfi_video_work_route *prop =
			(struct hfi_video_work_route *)
			&pkt->rg_property_data[1];
		prop->video_work_route =
			hal->video_work_route;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_WORK_ROUTE;
		pkt->size += sizeof(*prop);
		break;
	}
	case HAL_PARAM_VENC_HDR10_PQ_SEI:
	{
		struct hfi_hdr10_pq_sei *hfi;
		struct hal_hdr10_pq_sei *prop =
			(struct hal_hdr10_pq_sei *) pdata;

		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_HDR10_PQ_SEI;
		hfi = (struct hfi_hdr10_pq_sei *)
			&pkt->rg_property_data[1];

		memcpy(hfi, prop, sizeof(*hfi));
		pkt->size += sizeof(struct hfi_hdr10_pq_sei);
		break;
	}
	case HAL_CONFIG_VENC_VBV_HRD_BUF_SIZE:
	{
		struct hfi_vbv_hdr_buf_size *hfi;
		struct hal_vbv_hdr_buf_size *prop =
			(struct hal_vbv_hdr_buf_size *) pdata;

		pkt->rg_property_data[0] =
			HFI_PROPERTY_CONFIG_VENC_VBV_HRD_BUF_SIZE;
		hfi = (struct hfi_vbv_hdr_buf_size *)
			&pkt->rg_property_data[1];

		hfi->vbv_hdr_buf_size = prop->vbv_hdr_buf_size;
		pkt->size += sizeof(struct hfi_vbv_hdr_buf_size);
		break;
	}
	/* FOLLOWING PROPERTIES ARE NOT IMPLEMENTED IN CORE YET */
	case HAL_CONFIG_BUFFER_REQUIREMENTS:
	case HAL_CONFIG_PRIORITY:
	case HAL_CONFIG_BATCH_INFO:
	case HAL_PARAM_METADATA_PASS_THROUGH:
	case HAL_SYS_IDLE_INDICATOR:
	case HAL_PARAM_UNCOMPRESSED_FORMAT_SUPPORTED:
	case HAL_PARAM_INTERLACE_FORMAT_SUPPORTED:
	case HAL_PARAM_CHROMA_SITE:
	case HAL_PARAM_PROPERTIES_SUPPORTED:
	case HAL_PARAM_PROFILE_LEVEL_SUPPORTED:
	case HAL_PARAM_CAPABILITY_SUPPORTED:
	case HAL_PARAM_NAL_STREAM_FORMAT_SUPPORTED:
	case HAL_PARAM_MULTI_VIEW_FORMAT:
	case HAL_PARAM_MAX_SEQUENCE_HEADER_SIZE:
	case HAL_PARAM_CODEC_SUPPORTED:
	case HAL_PARAM_VDEC_MULTI_VIEW_SELECT:
	case HAL_PARAM_VDEC_MB_QUANTIZATION:
	case HAL_PARAM_VDEC_NUM_CONCEALED_MB:
	case HAL_PARAM_VDEC_H264_ENTROPY_SWITCHING:
	case HAL_CONFIG_BUFFER_COUNT_ACTUAL:
	case HAL_CONFIG_VDEC_MULTI_STREAM:
	case HAL_PARAM_VENC_MULTI_SLICE_INFO:
	case HAL_CONFIG_VENC_TIMESTAMP_SCALE:
	default:
		dprintk(VIDC_ERR, "DEFAULT: Calling %#x\n", ptype);
		rc = -ENOTSUPP;
		break;
	}
	return rc;
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
		dprintk(VIDC_WARN,
			"SSR trigger type not recognized, using WDOG.\n");
	}
	return rc;
}

int create_pkt_ssr_cmd(enum hal_ssr_trigger_type type,
		struct hfi_cmd_sys_test_ssr_packet *pkt)
{
	if (!pkt) {
		dprintk(VIDC_ERR, "Invalid params, device: %pK\n", pkt);
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
		dprintk(VIDC_ERR, "%s invalid param :%pK\n", __func__, pkt);
		return -EINVAL;
	}
	pkt->size = sizeof(struct hfi_cmd_sys_get_property_packet);
	pkt->packet_type = HFI_CMD_SYS_GET_PROPERTY;
	pkt->num_properties = 1;
	pkt->rg_property_data[0] = HFI_PROPERTY_SYS_IMAGE_VERSION;
	return 0;
}

int create_pkt_cmd_session_sync_process(
		struct hfi_cmd_session_sync_process_packet *pkt,
		struct hal_session *session)
{
	if (!pkt || !session)
		return -EINVAL;

	*pkt = (struct hfi_cmd_session_sync_process_packet) {0};
	pkt->size = sizeof(*pkt);
	pkt->packet_type = HFI_CMD_SESSION_SYNC;
	pkt->session_id = hash32_ptr(session);
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
	.sys_ping = create_pkt_cmd_sys_ping,
	.sys_image_version = create_pkt_cmd_sys_image_version,
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
	.session_get_property = create_pkt_cmd_session_get_property,
	.session_set_property = create_pkt_cmd_session_set_property,
};

struct hfi_packetization_ops *hfi_get_pkt_ops_handle(
			enum hfi_packetization_type type)
{
	dprintk(VIDC_DBG, "%s selected\n",
		type == HFI_PACKETIZATION_4XX ?
		"4xx packetization" : "Unknown hfi");

	switch (type) {
	case HFI_PACKETIZATION_4XX:
		return &hfi_default;
	}

	return NULL;
}
