/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/errno.h>
#include <linux/log2.h>
#include <linux/hash.h>
#include <soc/qcom/ocmem.h>
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
static int profile_table[] = {
	[ilog2(HAL_H264_PROFILE_BASELINE)] = HFI_H264_PROFILE_BASELINE,
	[ilog2(HAL_H264_PROFILE_MAIN)] = HFI_H264_PROFILE_MAIN,
	[ilog2(HAL_H264_PROFILE_HIGH)] = HFI_H264_PROFILE_HIGH,
	[ilog2(HAL_H264_PROFILE_CONSTRAINED_BASE)] =
		HFI_H264_PROFILE_CONSTRAINED_BASE,
	[ilog2(HAL_H264_PROFILE_CONSTRAINED_HIGH)] =
		HFI_H264_PROFILE_CONSTRAINED_HIGH,
	[ilog2(HAL_VPX_PROFILE_VERSION_1)] = HFI_VPX_PROFILE_VERSION_1,
	[ilog2(HAL_MVC_PROFILE_STEREO_HIGH)] = HFI_H264_PROFILE_STEREO_HIGH,
};

static int entropy_mode[] = {
	[ilog2(HAL_H264_ENTROPY_CAVLC)] = HFI_H264_ENTROPY_CAVLC,
	[ilog2(HAL_H264_ENTROPY_CABAC)] = HFI_H264_ENTROPY_CABAC,
};

static int cabac_model[] = {
	[ilog2(HAL_H264_CABAC_MODEL_0)] = HFI_H264_CABAC_MODEL_0,
	[ilog2(HAL_H264_CABAC_MODEL_1)] = HFI_H264_CABAC_MODEL_1,
	[ilog2(HAL_H264_CABAC_MODEL_2)] = HFI_H264_CABAC_MODEL_2,
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
	if (hal_type && (roundup_pow_of_two(hal_type) != hal_type)) {
		/* Not a power of 2, it's not going
		 * to be in any of the tables anyway */
		return -EINVAL;
	}

	if (hal_type)
		hal_type = ilog2(hal_type);

	switch (property) {
	case HAL_PARAM_PROFILE_LEVEL_CURRENT:
		return (hal_type >= ARRAY_SIZE(profile_table)) ?
			-ENOTSUPP : profile_table[hal_type];
	case HAL_PARAM_VENC_H264_ENTROPY_CONTROL:
		return (hal_type >= ARRAY_SIZE(entropy_mode)) ?
			-ENOTSUPP : entropy_mode[hal_type];
	case HAL_PARAM_VENC_H264_ENTROPY_CABAC_MODEL:
		return (hal_type >= ARRAY_SIZE(cabac_model)) ?
			-ENOTSUPP : cabac_model[hal_type];
	case HAL_PARAM_UNCOMPRESSED_FORMAT_SELECT:
		return (hal_type >= ARRAY_SIZE(color_format)) ?
			-ENOTSUPP : color_format[hal_type];
	case HAL_PARAM_NAL_STREAM_FORMAT_SELECT:
		return (hal_type >= ARRAY_SIZE(nal_type)) ?
			-ENOTSUPP : nal_type[hal_type];
	default:
		return -ENOTSUPP;
	}
}

static inline u32 get_hfi_layout(enum hal_buffer_layout_type hal_buf_layout)
{
	u32 hfi_layout;
	switch (hal_buf_layout) {
	case HAL_BUFFER_LAYOUT_TOP_BOTTOM:
		hfi_layout = HFI_MVC_BUFFER_LAYOUT_TOP_BOTTOM;
		break;
	case HAL_BUFFER_LAYOUT_SEQ:
		hfi_layout = HFI_MVC_BUFFER_LAYOUT_SEQ;
		break;
	default:
		dprintk(VIDC_ERR, "Invalid buffer layout: 0x%x\n",
			hal_buf_layout);
		hfi_layout = HFI_MVC_BUFFER_LAYOUT_SEQ;
		break;
	}
	return hfi_layout;
}

static inline u32 get_hfi_codec(enum hal_video_codec hal_codec)
{
	u32 hfi_codec;
	switch (hal_codec) {
	case HAL_VIDEO_CODEC_MVC:
	case HAL_VIDEO_CODEC_H264:
		hfi_codec = HFI_VIDEO_CODEC_H264;
		break;
	case HAL_VIDEO_CODEC_H263:
		hfi_codec = HFI_VIDEO_CODEC_H263;
		break;
	case HAL_VIDEO_CODEC_MPEG1:
		hfi_codec = HFI_VIDEO_CODEC_MPEG1;
		break;
	case HAL_VIDEO_CODEC_MPEG2:
		hfi_codec = HFI_VIDEO_CODEC_MPEG2;
		break;
	case HAL_VIDEO_CODEC_MPEG4:
		hfi_codec = HFI_VIDEO_CODEC_MPEG4;
		break;
	case HAL_VIDEO_CODEC_DIVX_311:
		hfi_codec = HFI_VIDEO_CODEC_DIVX_311;
		break;
	case HAL_VIDEO_CODEC_DIVX:
		hfi_codec = HFI_VIDEO_CODEC_DIVX;
		break;
	case HAL_VIDEO_CODEC_VC1:
		hfi_codec = HFI_VIDEO_CODEC_VC1;
		break;
	case HAL_VIDEO_CODEC_SPARK:
		hfi_codec = HFI_VIDEO_CODEC_SPARK;
		break;
	case HAL_VIDEO_CODEC_VP8:
		hfi_codec = HFI_VIDEO_CODEC_VP8;
		break;
	case HAL_VIDEO_CODEC_HEVC:
		hfi_codec = HFI_VIDEO_CODEC_HEVC;
		break;
	case HAL_VIDEO_CODEC_HEVC_HYBRID:
		hfi_codec = HFI_VIDEO_CODEC_HEVC_HYBRID;
		break;
	default:
		dprintk(VIDC_ERR, "Invalid codec 0x%x\n", hal_codec);
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

int create_pkt_cmd_sys_idle_indicator(
	struct hfi_cmd_sys_set_property_packet *pkt,
	u32 enable)
{
	struct hfi_enable *hfi;
	if (!pkt)
		return -EINVAL;

	pkt->size = sizeof(struct hfi_cmd_sys_set_property_packet) +
		sizeof(struct hfi_enable) + sizeof(u32);
	pkt->packet_type = HFI_CMD_SYS_SET_PROPERTY;
	pkt->num_properties = 1;
	pkt->rg_property_data[0] = HFI_PROPERTY_SYS_IDLE_INDICATOR;
	hfi = (struct hfi_enable *) &pkt->rg_property_data[1];
	hfi->enable = enable;
	return 0;
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
	if (msm_fw_debug_mode <= HFI_DEBUG_MODE_QDSS)
		hfi->debug_mode = msm_fw_debug_mode;
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

int create_pkt_set_cmd_sys_resource(
		struct hfi_cmd_sys_set_resource_packet *pkt,
		struct vidc_resource_hdr *resource_hdr,
		void *resource_value)
{
	int rc = 0;
	if (!pkt || !resource_hdr || !resource_value)
		return -EINVAL;

	pkt->packet_type = HFI_CMD_SYS_SET_RESOURCE;
	pkt->size = sizeof(struct hfi_cmd_sys_set_resource_packet);
	pkt->resource_handle = resource_hdr->resource_handle;

	switch (resource_hdr->resource_id) {
	case VIDC_RESOURCE_OCMEM:
	{
		struct hfi_resource_ocmem *hfioc_mem =
			(struct hfi_resource_ocmem *)
			&pkt->rg_resource_data[0];
		struct ocmem_buf *ocmem =
			(struct ocmem_buf *) resource_value;

		pkt->resource_type = HFI_RESOURCE_OCMEM;
		pkt->size += sizeof(struct hfi_resource_ocmem) - sizeof(u32);
		hfioc_mem->size = (u32)ocmem->len;
		hfioc_mem->mem = (u32)ocmem->addr;
		break;
	}
	default:
		dprintk(VIDC_ERR, "Invalid resource_id %d\n",
					resource_hdr->resource_id);
		rc = -EINVAL;
	}

	return rc;
}

int create_pkt_cmd_sys_release_resource(
		struct hfi_cmd_sys_release_resource_packet *pkt,
		struct vidc_resource_hdr *resource_hdr)
{
	int rc = 0;
	if (!pkt)
		return -EINVAL;

	pkt->size = sizeof(struct hfi_cmd_sys_release_resource_packet);
	pkt->packet_type = HFI_CMD_SYS_RELEASE_RESOURCE;
	pkt->resource_type = resource_hdr->resource_id;
	pkt->resource_handle = resource_hdr->resource_handle;

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
	pkt->session_domain = session_domain;
	pkt->session_codec = get_hfi_codec(session_codec);
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
		buffer = HFI_BUFFER_INTERNAL_SCRATCH;
		break;
	case HAL_BUFFER_INTERNAL_SCRATCH_1:
		buffer = HFI_BUFFER_INTERNAL_SCRATCH_1;
		break;
	case HAL_BUFFER_INTERNAL_SCRATCH_2:
		buffer = HFI_BUFFER_INTERNAL_SCRATCH_2;
		break;
	case HAL_BUFFER_INTERNAL_PERSIST:
		buffer = HFI_BUFFER_INTERNAL_PERSIST;
		break;
	case HAL_BUFFER_INTERNAL_PERSIST_1:
		buffer = HFI_BUFFER_INTERNAL_PERSIST_1;
		break;
	default:
		dprintk(VIDC_ERR, "Invalid buffer :0x%x\n",
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
	case HAL_EXTRADATA_MB_QUANTIZATION:
		ret = HFI_PROPERTY_PARAM_VDEC_MB_QUANTIZATION;
		break;
	case HAL_EXTRADATA_INTERLACE_VIDEO:
		ret = HFI_PROPERTY_PARAM_VDEC_INTERLACE_VIDEO_EXTRADATA;
		break;
	case HAL_EXTRADATA_VC1_FRAMEDISP:
		ret = HFI_PROPERTY_PARAM_VDEC_VC1_FRAMEDISP_EXTRADATA;
		break;
	case HAL_EXTRADATA_VC1_SEQDISP:
		ret = HFI_PROPERTY_PARAM_VDEC_VC1_SEQDISP_EXTRADATA;
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
	case HAL_EXTRADATA_MULTISLICE_INFO:
		ret = HFI_PROPERTY_PARAM_VENC_MULTI_SLICE_INFO;
		break;
	case HAL_EXTRADATA_NUM_CONCEALED_MB:
		ret = HFI_PROPERTY_PARAM_VDEC_NUM_CONCEALED_MB;
		break;
	case HAL_EXTRADATA_ASPECT_RATIO:
	case HAL_EXTRADATA_INPUT_CROP:
	case HAL_EXTRADATA_DIGITAL_ZOOM:
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
	case HAL_EXTRADATA_FRAME_BITS_INFO:
		ret = HFI_PROPERTY_PARAM_VDEC_FRAME_BITS_INFO_EXTRADATA;
		break;
	case HAL_EXTRADATA_LTR_INFO:
		ret = HFI_PROPERTY_PARAM_VENC_LTR_INFO;
		break;
	case HAL_EXTRADATA_METADATA_MBI:
		ret = HFI_PROPERTY_PARAM_VENC_MBI_DUMPING;
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
	case HAL_EXTRADATA_INPUT_CROP:
		ret = MSM_VIDC_EXTRADATA_INPUT_CROP;
		break;
	case HAL_EXTRADATA_DIGITAL_ZOOM:
		ret = MSM_VIDC_EXTRADATA_DIGITAL_ZOOM;
		break;
	default:
		ret = get_hfi_extradata_index(index);
		break;
	}
	return ret;
}

static u32 get_hfi_buf_mode(enum buffer_mode_type hal_buf_mode)
{
	u32 buf_mode;
	switch (hal_buf_mode) {
	case HAL_BUFFER_MODE_STATIC:
		buf_mode = HFI_BUFFER_MODE_STATIC;
		break;
	case HAL_BUFFER_MODE_RING:
		buf_mode = HFI_BUFFER_MODE_RING;
		break;
	case HAL_BUFFER_MODE_DYNAMIC:
		buf_mode = HFI_BUFFER_MODE_DYNAMIC;
		break;
	default:
		dprintk(VIDC_ERR, "Invalid buffer mode :0x%x\n",
				hal_buf_mode);
		buf_mode = 0;
		break;
	}
	return buf_mode;
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
	case HAL_LTR_MODE_PERIODIC:
		ltrmode = HFI_LTR_MODE_PERIODIC;
		break;
	default:
		dprintk(VIDC_ERR, "Invalid ltr mode :0x%x\n",
			ltr_mode_type);
		ltrmode = HFI_LTR_MODE_DISABLE;
		break;
	}
	return ltrmode;
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

	if ((buffer_info->buffer_type == HAL_BUFFER_OUTPUT) ||
		(buffer_info->buffer_type == HAL_BUFFER_OUTPUT2)) {
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

	if ((buffer_info->buffer_type == HAL_BUFFER_OUTPUT) ||
		(buffer_info->buffer_type == HAL_BUFFER_OUTPUT2)) {
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
	pkt->time_stamp_hi = (int) (((u64)input_frame->timestamp) >> 32);
	pkt->time_stamp_lo = (int) input_frame->timestamp;
	pkt->flags = input_frame->flags;
	pkt->mark_target = input_frame->mark_target;
	pkt->mark_data = input_frame->mark_data;
	pkt->offset = input_frame->offset;
	pkt->alloc_len = input_frame->alloc_len;
	pkt->filled_len = input_frame->filled_len;
	pkt->input_tag = input_frame->clnt_data;
	pkt->packet_buffer = (u32)input_frame->device_addr;
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
	pkt->time_stamp_hi = (u32)(((u64)input_frame->timestamp) >> 32);
	pkt->time_stamp_lo = (u32)input_frame->timestamp;
	pkt->flags = input_frame->flags;
	pkt->mark_target = input_frame->mark_target;
	pkt->mark_data = input_frame->mark_data;
	pkt->offset = input_frame->offset;
	pkt->alloc_len = input_frame->alloc_len;
	pkt->filled_len = input_frame->filled_len;
	pkt->input_tag = input_frame->clnt_data;
	pkt->packet_buffer = (u32)input_frame->device_addr;
	pkt->extra_data_buffer = (u32)input_frame->extradata_addr;
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
	dprintk(VIDC_DBG, "### Q OUTPUT BUFFER ###: %d, %d, %d\n",
			pkt->alloc_len, pkt->filled_len, pkt->offset);

	return rc;
}

int create_pkt_cmd_session_parse_seq_header(
		struct hfi_cmd_session_parse_sequence_header_packet *pkt,
		struct hal_session *session, struct vidc_seq_hdr *seq_hdr)
{
	int rc = 0;
	if (!pkt || !session || !seq_hdr)
		return -EINVAL;

	pkt->size = sizeof(struct hfi_cmd_session_parse_sequence_header_packet);
	pkt->packet_type = HFI_CMD_SESSION_PARSE_SEQUENCE_HEADER;
	pkt->session_id = hash32_ptr(session);
	pkt->header_len = seq_hdr->seq_hdr_len;
	if (!seq_hdr->seq_hdr)
		return -EINVAL;
	pkt->packet_buffer = (u32)seq_hdr->seq_hdr;
	return rc;
}

int create_pkt_cmd_session_get_seq_hdr(
		struct hfi_cmd_session_get_sequence_header_packet *pkt,
		struct hal_session *session, struct vidc_seq_hdr *seq_hdr)
{
	int rc = 0;

	if (!pkt || !session || !seq_hdr)
		return -EINVAL;

	pkt->size = sizeof(struct hfi_cmd_session_get_sequence_header_packet);
	pkt->packet_type = HFI_CMD_SESSION_GET_SEQUENCE_HEADER;
	pkt->session_id = hash32_ptr(session);
	pkt->buffer_len = seq_hdr->seq_hdr_len;
	if (!seq_hdr->seq_hdr)
		return -EINVAL;
	pkt->packet_buffer = (u32)seq_hdr->seq_hdr;
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
	case HAL_FLUSH_OUTPUT2:
		pkt->flush_type = HFI_FLUSH_OUTPUT2;
		break;
	case HAL_FLUSH_ALL:
		pkt->flush_type = HFI_FLUSH_ALL;
		break;
	default:
		dprintk(VIDC_ERR, "Invalid flush mode: 0x%x\n", flush_mode);
		return -EINVAL;
	}
	return rc;
}

int create_pkt_cmd_session_get_property(
		struct hfi_cmd_session_get_property_packet *pkt,
		struct hal_session *session, enum hal_property ptype)
{
	int rc = 0;
	if (!pkt || !session) {
		dprintk(VIDC_ERR, "%s Invalid parameters\n", __func__);
		return -EINVAL;
	}
	pkt->size = sizeof(struct hfi_cmd_session_get_property_packet);
	pkt->packet_type = HFI_CMD_SESSION_GET_PROPERTY;
	pkt->session_id = hash32_ptr(session);
	pkt->num_properties = 1;
	switch (ptype) {
	case HAL_PARAM_PROFILE_LEVEL_CURRENT:
			pkt->rg_property_data[0] =
				HFI_PROPERTY_PARAM_PROFILE_LEVEL_CURRENT;
		break;
	default:
		dprintk(VIDC_ERR, "%s cmd:0x%x not supported\n", __func__,
			ptype);
		rc = -EINVAL;
		break;
	}
	return rc;
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
		pkt->size += sizeof(u32) + sizeof(struct hfi_frame_rate);
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
		pkt->size += sizeof(u32) +
			sizeof(struct hfi_uncompressed_format_select);
		break;
	}
	case HAL_PARAM_UNCOMPRESSED_PLANE_ACTUAL_CONSTRAINTS_INFO:
		break;
	case HAL_PARAM_UNCOMPRESSED_PLANE_ACTUAL_INFO:
		break;
	case HAL_PARAM_EXTRA_DATA_HEADER_CONFIG:
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
		pkt->size += sizeof(u32) + sizeof(struct hfi_frame_size);
		break;
	}
	case HAL_CONFIG_REALTIME:
	{
		struct hfi_enable *hfi;
		pkt->rg_property_data[0] = HFI_PROPERTY_CONFIG_REALTIME;
		hfi = (struct hfi_enable *) &pkt->rg_property_data[1];
		hfi->enable = ((struct hfi_enable *) pdata)->enable;
		pkt->size += sizeof(u32) * 2;
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

		buffer_type = get_hfi_buffer(prop->buffer_type);
		if (buffer_type)
			hfi->buffer_type = buffer_type;
		else
			return -EINVAL;

		pkt->size += sizeof(u32) + sizeof(struct
				hfi_buffer_count_actual);

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
		pkt->size += sizeof(u32) +
			sizeof(struct hfi_nal_stream_format_select);
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
			dprintk(VIDC_ERR, "invalid output order: 0x%x\n",
						  *data);
			break;
		}
		pkt->size += sizeof(u32) * 2;
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
		pkt->size += sizeof(u32) * 2;
		break;
	}
	case HAL_PARAM_VDEC_OUTPUT2_KEEP_ASPECT_RATIO:
	{
		struct hfi_enable *hfi;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VDEC_OUTPUT2_KEEP_ASPECT_RATIO;
		hfi = (struct hfi_enable *) &pkt->rg_property_data[1];
		hfi->enable = ((struct hfi_enable *) pdata)->enable;
		pkt->size += sizeof(u32) * 2;
		break;
	}
	case HAL_CONFIG_VDEC_POST_LOOP_DEBLOCKER:
	{
		struct hfi_enable *hfi;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_CONFIG_VDEC_POST_LOOP_DEBLOCKER;
		hfi = (struct hfi_enable *) &pkt->rg_property_data[1];
		hfi->enable = ((struct hfi_enable *) pdata)->enable;
		pkt->size += sizeof(u32) * 2;
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
		hfi->width = prop->width;
		hfi->height = prop->height;
		pkt->size += sizeof(u32) + sizeof(struct hfi_multi_stream);
		break;
	}
	case HAL_PARAM_VDEC_DISPLAY_PICTURE_BUFFER_COUNT:
	{
		struct hfi_display_picture_buffer_count *hfi;
		struct hal_display_picture_buffer_count *prop =
			(struct hal_display_picture_buffer_count *) pdata;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VDEC_DISPLAY_PICTURE_BUFFER_COUNT;
		hfi = (struct hfi_display_picture_buffer_count *)
			&pkt->rg_property_data[1];
		hfi->count = prop->count;
		hfi->enable = prop->enable;
		pkt->size += sizeof(u32) +
			sizeof(struct hfi_display_picture_buffer_count);
		break;
	}
	case HAL_PARAM_DIVX_FORMAT:
	{
		int *data = pdata;
		pkt->rg_property_data[0] = HFI_PROPERTY_PARAM_DIVX_FORMAT;
		switch (*data) {
		case HAL_DIVX_FORMAT_4:
			pkt->rg_property_data[1] = HFI_DIVX_FORMAT_4;
			break;
		case HAL_DIVX_FORMAT_5:
			pkt->rg_property_data[1] = HFI_DIVX_FORMAT_5;
			break;
		case HAL_DIVX_FORMAT_6:
			pkt->rg_property_data[1] = HFI_DIVX_FORMAT_6;
			break;
		default:
			dprintk(VIDC_ERR, "Invalid divx format: 0x%x\n", *data);
			break;
		}
		pkt->size += sizeof(u32) * 2;
		break;
	}
	case HAL_CONFIG_VDEC_MB_ERROR_MAP_REPORTING:
	{
		struct hfi_enable *hfi;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_CONFIG_VDEC_MB_ERROR_MAP_REPORTING;
		hfi = (struct hfi_enable *) &pkt->rg_property_data[1];
		hfi->enable = ((struct hfi_enable *) pdata)->enable;
		pkt->size += sizeof(u32) * 2;
		break;
	}
	case HAL_PARAM_VDEC_CONTINUE_DATA_TRANSFER:
	{
		struct hfi_enable *hfi;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VDEC_CONTINUE_DATA_TRANSFER;
		hfi = (struct hfi_enable *) &pkt->rg_property_data[1];
		hfi->enable = ((struct hfi_enable *) pdata)->enable;
		pkt->size += sizeof(u32) * 2;
		break;
	}
	case HAL_PARAM_VDEC_SYNC_FRAME_DECODE:
	{
		struct hfi_enable *hfi;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VDEC_THUMBNAIL_MODE;
		hfi = (struct hfi_enable *) &pkt->rg_property_data[1];
		hfi->enable = ((struct hfi_enable *) pdata)->enable;
		pkt->size += sizeof(u32) * 2;
		break;
	}
	case HAL_PARAM_VENC_SYNC_FRAME_SEQUENCE_HEADER:
	{
		struct hfi_enable *hfi;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_CONFIG_VENC_SYNC_FRAME_SEQUENCE_HEADER;
		hfi = (struct hfi_enable *) &pkt->rg_property_data[1];
		hfi->enable = ((struct hfi_enable *) pdata)->enable;
		pkt->size += sizeof(u32) * 2;
		break;
	}
	case HAL_CONFIG_VENC_REQUEST_IFRAME:
		pkt->rg_property_data[0] =
			HFI_PROPERTY_CONFIG_VENC_REQUEST_SYNC_FRAME;
		pkt->size += sizeof(u32);
		break;
	case HAL_PARAM_VENC_MPEG4_SHORT_HEADER:
		break;
	case HAL_PARAM_VENC_MPEG4_AC_PREDICTION:
		break;
	case HAL_CONFIG_VENC_TARGET_BITRATE:
	{
		struct hfi_bitrate *hfi;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_CONFIG_VENC_TARGET_BITRATE;
		hfi = (struct hfi_bitrate *) &pkt->rg_property_data[1];
		hfi->bit_rate = ((struct hal_bitrate *)pdata)->bit_rate;
		hfi->layer_id = ((struct hal_bitrate *)pdata)->layer_id;
		pkt->size += sizeof(u32) + sizeof(struct hfi_bitrate);
		break;
	}
	case HAL_CONFIG_VENC_MAX_BITRATE:
	{
		struct hfi_bitrate *hfi;

		pkt->rg_property_data[0] =
			HFI_PROPERTY_CONFIG_VENC_MAX_BITRATE;
		hfi = (struct hfi_bitrate *) &pkt->rg_property_data[1];
		hfi->bit_rate = ((struct hal_bitrate *)pdata)->bit_rate;
		hfi->layer_id = ((struct hal_bitrate *)pdata)->layer_id;

		pkt->size += sizeof(u32) + sizeof(struct hfi_bitrate);
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
		hfi->level = prop->level;
		hfi->profile = hal_to_hfi_type(HAL_PARAM_PROFILE_LEVEL_CURRENT,
				prop->profile);
		if (hfi->profile <= 0) {
			hfi->profile = HFI_H264_PROFILE_HIGH;
			dprintk(VIDC_WARN,
					"Profile %d not supported, falling back to high\n",
					prop->profile);
		}

		if (!hfi->level) {
			hfi->level = 1;
			dprintk(VIDC_WARN,
					"Level %d not supported, falling back to high\n",
					prop->level);
		}

		pkt->size += sizeof(u32) + sizeof(struct hfi_profile_level);
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
		if (hfi->entropy_mode == HAL_H264_ENTROPY_CABAC)
			hfi->cabac_model = hal_to_hfi_type(
			   HAL_PARAM_VENC_H264_ENTROPY_CABAC_MODEL,
			   prop->cabac_model);
		pkt->size += sizeof(u32) + sizeof(
			struct hfi_h264_entropy_control);
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
		case HAL_RATE_CONTROL_CBR_CFR:
			pkt->rg_property_data[1] = HFI_RATE_CONTROL_CBR_CFR;
			break;
		case HAL_RATE_CONTROL_CBR_VFR:
			pkt->rg_property_data[1] = HFI_RATE_CONTROL_CBR_VFR;
			break;
		case HAL_RATE_CONTROL_VBR_CFR:
			pkt->rg_property_data[1] = HFI_RATE_CONTROL_VBR_CFR;
			break;
		case HAL_RATE_CONTROL_VBR_VFR:
			pkt->rg_property_data[1] = HFI_RATE_CONTROL_VBR_VFR;
			break;
		default:
			dprintk(VIDC_ERR,
					"Invalid Rate control setting: 0x%p\n",
					pdata);
			break;
		}
		pkt->size += sizeof(u32) * 2;
		break;
	}
	case HAL_PARAM_VENC_MPEG4_TIME_RESOLUTION:
	{
		struct hfi_mpeg4_time_resolution *hfi;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_MPEG4_TIME_RESOLUTION;
		hfi = (struct hfi_mpeg4_time_resolution *)
			&pkt->rg_property_data[1];
		hfi->time_increment_resolution =
			((struct hal_mpeg4_time_resolution *)pdata)->
					time_increment_resolution;
		pkt->size += sizeof(u32) * 2;
		break;
	}
	case HAL_PARAM_VENC_MPEG4_HEADER_EXTENSION:
	{
		struct hfi_mpeg4_header_extension *hfi;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_MPEG4_HEADER_EXTENSION;
		hfi = (struct hfi_mpeg4_header_extension *)
			&pkt->rg_property_data[1];
		hfi->header_extension = (u32)(unsigned long) pdata;
		pkt->size += sizeof(u32) * 2;
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
			dprintk(VIDC_ERR, "Invalid deblocking mode: 0x%x\n",
						  prop->mode);
			break;
		}
		hfi->slice_alpha_offset = prop->slice_alpha_offset;
		hfi->slice_beta_offset = prop->slice_beta_offset;
		pkt->size += sizeof(u32) +
			sizeof(struct hfi_h264_db_control);
		break;
	}
	case HAL_PARAM_VENC_SESSION_QP:
	{
		struct hfi_quantization *hfi;
		struct hal_quantization *hal_quant =
			(struct hal_quantization *) pdata;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_SESSION_QP;
		hfi = (struct hfi_quantization *) &pkt->rg_property_data[1];
		hfi->qp_i = hal_quant->qpi;
		hfi->qp_p = hal_quant->qpp;
		hfi->qp_b = hal_quant->qpb;
		hfi->layer_id = hal_quant->layer_id;
		pkt->size += sizeof(u32) + sizeof(struct hfi_quantization);
		break;
	}
	case HAL_PARAM_VENC_SESSION_QP_RANGE:
	{
		struct hfi_quantization_range *hfi;
		struct hfi_quantization_range *hal_range =
			(struct hfi_quantization_range *) pdata;
		u32 min_qp, max_qp;

		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_SESSION_QP_RANGE;
		hfi = (struct hfi_quantization_range *)
				&pkt->rg_property_data[1];

		min_qp = hal_range->min_qp;
		max_qp = hal_range->max_qp;

		/* We'll be packing in the qp, so make sure we
		 * won't be losing data when masking */
		if (min_qp > 0xff || max_qp > 0xff) {
			dprintk(VIDC_ERR, "qp value out of range\n");
			rc = -ERANGE;
			break;
		}

		/* When creating the packet, pack the qp value as
		 * 0xiippbb, where ii = qp range for I-frames,
		 * pp = qp range for P-frames, etc. */
		hfi->min_qp = min_qp | min_qp << 8 | min_qp << 16;
		hfi->max_qp = max_qp | max_qp << 8 | max_qp << 16;
		hfi->layer_id = hal_range->layer_id;

		pkt->size += sizeof(u32) +
			sizeof(struct hfi_quantization_range);
		break;
	}
	case HAL_PARAM_VENC_SEARCH_RANGE:
	{
		struct hfi_vc1e_perf_cfg_type *hfi;
		struct hal_vc1e_perf_cfg_type *hal_mv_searchrange =
			(struct hal_vc1e_perf_cfg_type *) pdata;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_VC1_PERF_CFG;
		hfi = (struct hfi_vc1e_perf_cfg_type *)
				&pkt->rg_property_data[1];
		hfi->search_range_x_subsampled[0] =
			hal_mv_searchrange->i_frame.x_subsampled;
		hfi->search_range_x_subsampled[1] =
			hal_mv_searchrange->p_frame.x_subsampled;
		hfi->search_range_x_subsampled[2] =
			hal_mv_searchrange->b_frame.x_subsampled;
		hfi->search_range_y_subsampled[0] =
			hal_mv_searchrange->i_frame.y_subsampled;
		hfi->search_range_y_subsampled[1] =
			hal_mv_searchrange->p_frame.y_subsampled;
		hfi->search_range_y_subsampled[2] =
			hal_mv_searchrange->b_frame.y_subsampled;
		pkt->size += sizeof(u32) +
			sizeof(struct hfi_vc1e_perf_cfg_type);
		break;
	}
	case HAL_PARAM_VENC_MAX_NUM_B_FRAMES:
	{
		struct hfi_max_num_b_frames *hfi;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_MAX_NUM_B_FRAMES;
		hfi = (struct hfi_max_num_b_frames *) &pkt->rg_property_data[1];
		memcpy(hfi, (struct hfi_max_num_b_frames *) pdata,
				sizeof(struct hfi_max_num_b_frames));
		pkt->size += sizeof(u32) + sizeof(struct hfi_max_num_b_frames);
		break;
	}
	case HAL_CONFIG_VENC_INTRA_PERIOD:
	{
		struct hfi_intra_period *hfi;
		int period = 0;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_CONFIG_VENC_INTRA_PERIOD;
		hfi = (struct hfi_intra_period *) &pkt->rg_property_data[1];
		memcpy(hfi, (struct hfi_intra_period *) pdata,
				sizeof(struct hfi_intra_period));
		pkt->size += sizeof(u32) + sizeof(struct hfi_intra_period);
		period = hfi->pframes + hfi->bframes + 1;
		if (hfi->bframes) {
			hfi->pframes = period / (hfi->bframes + 1);
			hfi->bframes = period - hfi->pframes;
		} else {
			hfi->pframes = period - 1;
			hfi->bframes = 0;
		}
		break;
	}
	case HAL_CONFIG_VENC_IDR_PERIOD:
	{
		struct hfi_idr_period *hfi;
		pkt->rg_property_data[0] = HFI_PROPERTY_CONFIG_VENC_IDR_PERIOD;
		hfi = (struct hfi_idr_period *) &pkt->rg_property_data[1];
		hfi->idr_period = ((struct hfi_idr_period *) pdata)->idr_period;
		pkt->size += sizeof(u32) * 2;
		break;
	}
	case HAL_PARAM_VDEC_CONCEAL_COLOR:
	{
		struct hfi_conceal_color *hfi;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VDEC_CONCEAL_COLOR;
		hfi = (struct hfi_conceal_color *) &pkt->rg_property_data[1];
		if (hfi)
			hfi->conceal_color =
				((struct hfi_conceal_color *) pdata)->
				conceal_color;
		pkt->size += sizeof(u32) * 2;
		break;
	}
	case HAL_CONFIG_VPE_OPERATIONS:
	{
		struct hfi_operations_type *hfi;
		struct hal_operations *prop =
			(struct hal_operations *) pdata;
		pkt->rg_property_data[0] = HFI_PROPERTY_CONFIG_VPE_OPERATIONS;
		hfi = (struct hfi_operations_type *) &pkt->rg_property_data[1];
		switch (prop->rotate) {
		case HAL_ROTATE_NONE:
			hfi->rotation = HFI_ROTATE_NONE;
			break;
		case HAL_ROTATE_90:
			hfi->rotation = HFI_ROTATE_90;
			break;
		case HAL_ROTATE_180:
			hfi->rotation = HFI_ROTATE_180;
			break;
		case HAL_ROTATE_270:
			hfi->rotation = HFI_ROTATE_270;
			break;
		default:
			dprintk(VIDC_ERR, "Invalid rotation setting: 0x%x\n",
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
		default:
			dprintk(VIDC_ERR, "Invalid flip setting: 0x%x\n",
				prop->flip);
			rc = -EINVAL;
			break;
		}
		pkt->size += sizeof(u32) + sizeof(struct hfi_operations_type);
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
		switch (prop->mode) {
		case HAL_INTRA_REFRESH_NONE:
			hfi->mode = HFI_INTRA_REFRESH_NONE;
			break;
		case HAL_INTRA_REFRESH_ADAPTIVE:
			hfi->mode = HFI_INTRA_REFRESH_ADAPTIVE;
			break;
		case HAL_INTRA_REFRESH_CYCLIC:
			hfi->mode = HFI_INTRA_REFRESH_CYCLIC;
			break;
		case HAL_INTRA_REFRESH_CYCLIC_ADAPTIVE:
			hfi->mode = HFI_INTRA_REFRESH_CYCLIC_ADAPTIVE;
			break;
		case HAL_INTRA_REFRESH_RANDOM:
			hfi->mode = HFI_INTRA_REFRESH_RANDOM;
			break;
		default:
			dprintk(VIDC_ERR,
					"Invalid intra refresh setting: 0x%x\n",
					prop->mode);
			break;
		}
		hfi->air_mbs = prop->air_mbs;
		hfi->air_ref = prop->air_ref;
		hfi->cir_mbs = prop->cir_mbs;
		pkt->size += sizeof(u32) + sizeof(struct hfi_intra_refresh);
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
		case HAL_MULTI_SLICE_GOB:
			hfi->multi_slice = HFI_MULTI_SLICE_GOB;
			break;
		case HAL_MULTI_SLICE_BY_MB_COUNT:
			hfi->multi_slice = HFI_MULTI_SLICE_BY_MB_COUNT;
			break;
		case HAL_MULTI_SLICE_BY_BYTE_COUNT:
			hfi->multi_slice = HFI_MULTI_SLICE_BY_BYTE_COUNT;
			break;
		default:
			dprintk(VIDC_ERR, "Invalid slice settings: 0x%x\n",
				prop->multi_slice);
			break;
		}
		hfi->slice_size = prop->slice_size;
		pkt->size += sizeof(u32) + sizeof(struct
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
		hfi =
			(struct hfi_index_extradata_config *)
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
		pkt->size += sizeof(u32) +
			sizeof(struct hfi_index_extradata_config);
		break;
	}
	case HAL_PARAM_VENC_SLICE_DELIVERY_MODE:
	{
		struct hfi_enable *hfi;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_SLICE_DELIVERY_MODE;
		hfi = (struct hfi_enable *) &pkt->rg_property_data[1];
		hfi->enable = ((struct hal_enable *) pdata)->enable;
		pkt->size += sizeof(u32) + sizeof(struct hfi_enable);
		break;
	}
	case HAL_PARAM_VENC_H264_VUI_TIMING_INFO:
	{
		struct hfi_h264_vui_timing_info *hfi;
		struct hal_h264_vui_timing_info *timing_info = pdata;

		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_H264_VUI_TIMING_INFO;

		hfi = (struct hfi_h264_vui_timing_info *)&pkt->
			rg_property_data[1];
		hfi->enable = timing_info->enable;
		hfi->fixed_frame_rate = timing_info->fixed_frame_rate;
		hfi->time_scale = timing_info->time_scale;

		pkt->size += sizeof(u32) +
			sizeof(struct hfi_h264_vui_timing_info);
		break;
	}
	case HAL_CONFIG_VPE_DEINTERLACE:
	{
		struct hfi_enable *hfi;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_CONFIG_VPE_DEINTERLACE;
		hfi = (struct hfi_enable *) &pkt->rg_property_data[1];
		hfi->enable = ((struct hal_enable *) pdata)->enable;
		pkt->size += sizeof(u32) + sizeof(struct hfi_enable);
		break;
	}
	case HAL_PARAM_VENC_H264_GENERATE_AUDNAL:
	{
		struct hfi_enable *hfi;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_H264_GENERATE_AUDNAL;
		hfi = (struct hfi_enable *) &pkt->rg_property_data[1];
		hfi->enable = ((struct hal_enable *) pdata)->enable;
		pkt->size += sizeof(u32) + sizeof(struct hfi_enable);
		break;
	}
	case HAL_PARAM_BUFFER_ALLOC_MODE:
	{
		u32 buffer_type;
		u32 buffer_mode;
		struct hfi_buffer_alloc_mode *hfi;
		struct hal_buffer_alloc_mode *alloc_info = pdata;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_BUFFER_ALLOC_MODE;
		hfi = (struct hfi_buffer_alloc_mode *)
			&pkt->rg_property_data[1];
		buffer_type = get_hfi_buffer(alloc_info->buffer_type);
		if (buffer_type)
			hfi->buffer_type = buffer_type;
		else
			return -EINVAL;
		buffer_mode = get_hfi_buf_mode(alloc_info->buffer_mode);
		if (buffer_mode)
			hfi->buffer_mode = buffer_mode;
		else
			return -EINVAL;
		pkt->size += sizeof(u32) + sizeof(struct hfi_buffer_alloc_mode);
		break;
	}
	case HAL_PARAM_VDEC_FRAME_ASSEMBLY:
	{
		struct hfi_enable *hfi;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VDEC_FRAME_ASSEMBLY;
		hfi = (struct hfi_enable *) &pkt->rg_property_data[1];
		hfi->enable = ((struct hfi_enable *) pdata)->enable;
		pkt->size += sizeof(u32) + sizeof(struct hfi_enable);
		break;
	}
	case HAL_PARAM_VENC_H264_VUI_BITSTREAM_RESTRC:
	{
		struct hfi_enable *hfi;
		struct hal_h264_vui_bitstream_restrc *hal = pdata;

		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_H264_VUI_BITSTREAM_RESTRC;
		hfi = (struct hfi_enable *) &pkt->rg_property_data[1];
		hfi->enable = hal->enable;
		pkt->size += sizeof(u32) + sizeof(struct hfi_enable);
		break;
	}
	case HAL_PARAM_VENC_PRESERVE_TEXT_QUALITY:
	{
		struct hfi_enable *hfi;
		struct hal_preserve_text_quality *hal = pdata;

		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_PRESERVE_TEXT_QUALITY;
		hfi = (struct hfi_enable *) &pkt->rg_property_data[1];
		hfi->enable = hal->enable;
		pkt->size += sizeof(u32) + sizeof(struct hfi_enable);
		break;
	}
	case HAL_PARAM_VDEC_SCS_THRESHOLD:
	{
		struct hfi_scs_threshold *hfi;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VDEC_SCS_THRESHOLD;
		hfi = (struct hfi_scs_threshold *) &pkt->rg_property_data[1];
		hfi->threshold_value =
			((struct hfi_scs_threshold *) pdata)->threshold_value;
		pkt->size += sizeof(u32) + sizeof(struct hfi_scs_threshold);
		break;
	}
	case HAL_PARAM_MVC_BUFFER_LAYOUT:
	{
		struct hfi_mvc_buffer_layout_descp_type *hfi;
		struct hal_mvc_buffer_layout *layout_info = pdata;
		pkt->rg_property_data[0] = HFI_PROPERTY_PARAM_MVC_BUFFER_LAYOUT;
		hfi = (struct hfi_mvc_buffer_layout_descp_type *)
			&pkt->rg_property_data[1];
		hfi->layout_type = get_hfi_layout(layout_info->layout_type);
		hfi->bright_view_first = layout_info->bright_view_first;
		hfi->ngap = layout_info->ngap;
		pkt->size += sizeof(u32) +
			sizeof(struct hfi_mvc_buffer_layout_descp_type);
		break;
	}
	case HAL_PARAM_VENC_LTRMODE:
	{
		struct hfi_ltrmode *hfi;
		struct hal_ltrmode *hal = pdata;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_LTRMODE;
		hfi = (struct hfi_ltrmode *) &pkt->rg_property_data[1];
		hfi->ltrmode = get_hfi_ltr_mode(hal->ltrmode);
		hfi->ltrcount = hal->ltrcount;
		hfi->trustmode = hal->trustmode;
		pkt->size += sizeof(u32) + sizeof(struct hfi_ltrmode);
		break;
	}
	case HAL_CONFIG_VENC_USELTRFRAME:
	{
		struct hfi_ltruse *hfi;
		struct hal_ltruse *hal = pdata;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_CONFIG_VENC_USELTRFRAME;
		hfi = (struct hfi_ltruse *) &pkt->rg_property_data[1];
		hfi->frames = hal->frames;
		hfi->refltr = hal->refltr;
		hfi->useconstrnt = hal->useconstrnt;
		pkt->size += sizeof(u32) + sizeof(struct hfi_ltruse);
		break;
	}
	case HAL_CONFIG_VENC_MARKLTRFRAME:
	{
		struct hfi_ltrmark *hfi;
		struct hal_ltrmark *hal = pdata;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_CONFIG_VENC_MARKLTRFRAME;
		hfi = (struct hfi_ltrmark *) &pkt->rg_property_data[1];
		hfi->markframe = hal->markframe;
		pkt->size += sizeof(u32) + sizeof(struct hfi_ltrmark);
		break;
	}
	case HAL_PARAM_VENC_HIER_P_MAX_ENH_LAYERS:
	{
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_HIER_P_MAX_NUM_ENH_LAYER;
		pkt->rg_property_data[1] = *(u32 *)pdata;
		pkt->size += sizeof(u32) * 2;
		break;
	}
	case HAL_CONFIG_VENC_HIER_P_NUM_FRAMES:
	{
		pkt->rg_property_data[0] =
			HFI_PROPERTY_CONFIG_VENC_HIER_P_ENH_LAYER;
		pkt->rg_property_data[1] = *(u32 *)pdata;
		pkt->size += sizeof(u32) * 2;
		break;
	}
	case HAL_PARAM_VENC_DISABLE_RC_TIMESTAMP:
	{
		struct hfi_enable *hfi;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_DISABLE_RC_TIMESTAMP;
		hfi = (struct hfi_enable *)&pkt->rg_property_data[1];
		hfi->enable = ((struct hfi_enable *)pdata)->enable;
		pkt->size += sizeof(u32) * 2;
		break;
	}
	case HAL_PARAM_VENC_ENABLE_INITIAL_QP:
	{
		struct hfi_initial_quantization *hfi;
		struct hal_initial_quantization *quant = pdata;
		pkt->rg_property_data[0] =
			HFI_PROPERTY_PARAM_VENC_INITIAL_QP;
		hfi = (struct hfi_initial_quantization *)
			&pkt->rg_property_data[1];
		hfi->init_qp_enable = quant->init_qp_enable;
		hfi->qp_i = quant->qpi;
		hfi->qp_p = quant->qpp;
		hfi->qp_b = quant->qpb;
		pkt->size += sizeof(u32) +
			sizeof(struct hfi_initial_quantization);
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
	case HAL_PARAM_VENC_MPEG4_DATA_PARTITIONING:
	case HAL_CONFIG_BUFFER_COUNT_ACTUAL:
	case HAL_CONFIG_VDEC_MULTI_STREAM:
	case HAL_PARAM_VENC_MULTI_SLICE_INFO:
	case HAL_CONFIG_VENC_TIMESTAMP_SCALE:
	case HAL_PARAM_VENC_LOW_LATENCY:
	default:
		dprintk(VIDC_ERR, "DEFAULT: Calling 0x%x\n", ptype);
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
		dprintk(VIDC_ERR, "Invalid params, device: %p\n", pkt);
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
		dprintk(VIDC_ERR, "%s invalid param :%p\n", __func__, pkt);
		return -EINVAL;
	}
	pkt->size = sizeof(struct hfi_cmd_sys_get_property_packet);
	pkt->packet_type = HFI_CMD_SYS_GET_PROPERTY;
	pkt->num_properties = 1;
	pkt->rg_property_data[0] = HFI_PROPERTY_SYS_IMAGE_VERSION;
	return 0;
}
