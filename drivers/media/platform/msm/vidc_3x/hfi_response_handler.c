/* Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
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

#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/hash.h>
#include <soc/qcom/smem.h>
#include "vidc_hfi_helper.h"
#include "vidc_hfi_io.h"
#include "msm_vidc_debug.h"
#include "vidc_hfi.h"

static enum vidc_status hfi_parse_init_done_properties(
		struct msm_vidc_capability *capability,
		u32 num_sessions, u8 *data_ptr, u32 num_properties,
		u32 rem_bytes, u32 codec, u32 domain);

static enum vidc_status hfi_map_err_status(u32 hfi_err)
{
	enum vidc_status vidc_err;

	switch (hfi_err) {
	case HFI_ERR_NONE:
	case HFI_ERR_SESSION_SAME_STATE_OPERATION:
		vidc_err = VIDC_ERR_NONE;
		break;
	case HFI_ERR_SYS_FATAL:
		vidc_err = VIDC_ERR_HW_FATAL;
		break;
	case HFI_ERR_SYS_VERSION_MISMATCH:
	case HFI_ERR_SYS_INVALID_PARAMETER:
	case HFI_ERR_SYS_SESSION_ID_OUT_OF_RANGE:
	case HFI_ERR_SESSION_INVALID_PARAMETER:
	case HFI_ERR_SESSION_INVALID_SESSION_ID:
	case HFI_ERR_SESSION_INVALID_STREAM_ID:
		vidc_err = VIDC_ERR_BAD_PARAM;
		break;
	case HFI_ERR_SYS_INSUFFICIENT_RESOURCES:
	case HFI_ERR_SYS_UNSUPPORTED_DOMAIN:
	case HFI_ERR_SYS_UNSUPPORTED_CODEC:
	case HFI_ERR_SESSION_UNSUPPORTED_PROPERTY:
	case HFI_ERR_SESSION_UNSUPPORTED_SETTING:
	case HFI_ERR_SESSION_INSUFFICIENT_RESOURCES:
	case HFI_ERR_SESSION_UNSUPPORTED_STREAM:
		vidc_err = VIDC_ERR_NOT_SUPPORTED;
		break;
	case HFI_ERR_SYS_MAX_SESSIONS_REACHED:
		vidc_err = VIDC_ERR_MAX_CLIENTS;
		break;
	case HFI_ERR_SYS_SESSION_IN_USE:
		vidc_err = VIDC_ERR_CLIENT_PRESENT;
		break;
	case HFI_ERR_SESSION_FATAL:
		vidc_err = VIDC_ERR_CLIENT_FATAL;
		break;
	case HFI_ERR_SESSION_BAD_POINTER:
		vidc_err = VIDC_ERR_BAD_PARAM;
		break;
	case HFI_ERR_SESSION_INCORRECT_STATE_OPERATION:
		vidc_err = VIDC_ERR_BAD_STATE;
		break;
	case HFI_ERR_SESSION_STREAM_CORRUPT:
	case HFI_ERR_SESSION_STREAM_CORRUPT_OUTPUT_STALLED:
		vidc_err = VIDC_ERR_BITSTREAM_ERR;
		break;
	case HFI_ERR_SESSION_SYNC_FRAME_NOT_DETECTED:
		vidc_err = VIDC_ERR_IFRAME_EXPECTED;
		break;
	case HFI_ERR_SESSION_START_CODE_NOT_FOUND:
		vidc_err = VIDC_ERR_START_CODE_NOT_FOUND;
		break;
	case HFI_ERR_SESSION_EMPTY_BUFFER_DONE_OUTPUT_PENDING:
	default:
		vidc_err = VIDC_ERR_FAIL;
		break;
	}
	return vidc_err;
}

static enum msm_vidc_pixel_depth get_hal_pixel_depth(u32 hfi_bit_depth)
{
	switch (hfi_bit_depth) {
	case HFI_BITDEPTH_8: return MSM_VIDC_BIT_DEPTH_8;
	case HFI_BITDEPTH_9:
	case HFI_BITDEPTH_10: return MSM_VIDC_BIT_DEPTH_10;
	}
	dprintk(VIDC_ERR, "Unsupported bit depth: %d\n", hfi_bit_depth);
	return MSM_VIDC_BIT_DEPTH_UNSUPPORTED;
}

static int hfi_process_sess_evt_seq_changed(u32 device_id,
		struct hfi_msg_event_notify_packet *pkt,
		struct msm_vidc_cb_info *info)
{
	struct msm_vidc_cb_event event_notify = {0};
	int num_properties_changed;
	struct hfi_frame_size *frame_sz;
	struct hfi_profile_level *profile_level;
	struct hfi_bit_depth *pixel_depth;
	struct hfi_pic_struct *pic_struct;
	u8 *data_ptr;
	int prop_id;
	enum msm_vidc_pixel_depth luma_bit_depth, chroma_bit_depth;
	struct hfi_colour_space *colour_info;

	 /* Initialize pic_struct to unknown as default */
	event_notify.pic_struct = MSM_VIDC_PIC_STRUCT_UNKNOWN;

	if (sizeof(struct hfi_msg_event_notify_packet) > pkt->size) {
		dprintk(VIDC_ERR,
				"hal_process_session_init_done: bad_pkt_size\n");
		return -E2BIG;
	}

	event_notify.device_id = device_id;
	event_notify.session_id = (void *)(uintptr_t)pkt->session_id;
	event_notify.status = VIDC_ERR_NONE;
	num_properties_changed = pkt->event_data2;
	switch (pkt->event_data1) {
	case HFI_EVENT_DATA_SEQUENCE_CHANGED_SUFFICIENT_BUFFER_RESOURCES:
		event_notify.hal_event_type =
			HAL_EVENT_SEQ_CHANGED_SUFFICIENT_RESOURCES;
		break;
	case HFI_EVENT_DATA_SEQUENCE_CHANGED_INSUFFICIENT_BUFFER_RESOURCES:
		event_notify.hal_event_type =
			HAL_EVENT_SEQ_CHANGED_INSUFFICIENT_RESOURCES;
		break;
	default:
		break;
	}

	if (num_properties_changed) {
		data_ptr = (u8 *) &pkt->rg_ext_event_data[0];
		do {
			prop_id = (int) *((u32 *)data_ptr);
			switch (prop_id) {
			case HFI_PROPERTY_PARAM_FRAME_SIZE:
				data_ptr = data_ptr + sizeof(u32);
				frame_sz =
					(struct hfi_frame_size *) data_ptr;
				event_notify.width = frame_sz->width;
				event_notify.height = frame_sz->height;
				dprintk(VIDC_DBG, "height: %d width: %d\n",
					frame_sz->height, frame_sz->width);
				data_ptr +=
					sizeof(struct hfi_frame_size);
				break;
			case HFI_PROPERTY_PARAM_PROFILE_LEVEL_CURRENT:
				data_ptr = data_ptr + sizeof(u32);
				profile_level =
					(struct hfi_profile_level *) data_ptr;
				dprintk(VIDC_DBG, "profile: %d level: %d\n",
					profile_level->profile,
					profile_level->level);
				data_ptr +=
					sizeof(struct hfi_profile_level);
				break;
			case HFI_PROPERTY_PARAM_VDEC_PIXEL_BITDEPTH:
				data_ptr = data_ptr + sizeof(u32);
				pixel_depth = (struct hfi_bit_depth *) data_ptr;
				/*
				 * Luma and chroma can have different bitdepths.
				 * Driver should rely on luma and chroma
				 * bitdepth for determining output bitdepth
				 * type.
				 *
				 * pixel_depth->bitdepth will include luma
				 * bitdepth info in bits 0..15 and chroma
				 * bitdept in bits 16..31.
				 */
				luma_bit_depth = get_hal_pixel_depth(
					pixel_depth->bit_depth &
					GENMASK(15, 0));
				chroma_bit_depth = get_hal_pixel_depth(
					(pixel_depth->bit_depth &
					GENMASK(31, 16)) >> 16);
				if (luma_bit_depth == MSM_VIDC_BIT_DEPTH_10 ||
					chroma_bit_depth ==
						MSM_VIDC_BIT_DEPTH_10)
					event_notify.bit_depth =
						MSM_VIDC_BIT_DEPTH_10;
				else
					event_notify.bit_depth = luma_bit_depth;
				dprintk(VIDC_DBG,
					"bitdepth(%d), luma_bit_depth(%d), chroma_bit_depth(%d)\n",
					event_notify.bit_depth, luma_bit_depth,
					chroma_bit_depth);
				data_ptr += sizeof(struct hfi_bit_depth);
				break;
			case HFI_PROPERTY_PARAM_VDEC_PIC_STRUCT:
				data_ptr = data_ptr + sizeof(u32);
				pic_struct = (struct hfi_pic_struct *) data_ptr;
				event_notify.pic_struct =
					pic_struct->progressive_only;
				dprintk(VIDC_DBG,
					"Progressive only flag: %d\n",
						pic_struct->progressive_only);
				data_ptr +=
					sizeof(struct hfi_pic_struct);
				break;
			case HFI_PROPERTY_PARAM_VDEC_COLOUR_SPACE:
				data_ptr = data_ptr + sizeof(u32);
				colour_info =
					(struct hfi_colour_space *) data_ptr;
				event_notify.colour_space =
					colour_info->colour_space;
				dprintk(VIDC_DBG,
					"Colour space value is: %d\n",
						colour_info->colour_space);
				data_ptr +=
					sizeof(struct hfi_colour_space);
				break;
			default:
				dprintk(VIDC_ERR,
					"%s cmd: %#x not supported\n",
					__func__, prop_id);
				break;
			}
			num_properties_changed--;
		} while (num_properties_changed > 0);
	}

	*info = (struct msm_vidc_cb_info) {
		.response_type =  HAL_SESSION_EVENT_CHANGE,
		.response.event = event_notify,
	};

	return 0;
}

static int hfi_process_evt_release_buffer_ref(u32 device_id,
		struct hfi_msg_event_notify_packet *pkt,
		struct msm_vidc_cb_info *info)
{
	struct msm_vidc_cb_event event_notify = {0};
	struct hfi_msg_release_buffer_ref_event_packet *data;

	dprintk(VIDC_DBG,
			"RECEIVED: EVENT_NOTIFY - release_buffer_reference\n");
	if (sizeof(struct hfi_msg_event_notify_packet)
		> pkt->size) {
		dprintk(VIDC_ERR,
				"hal_process_session_init_done: bad_pkt_size\n");
		return -E2BIG;
	}

	data = (struct hfi_msg_release_buffer_ref_event_packet *)
				pkt->rg_ext_event_data;

	event_notify.device_id = device_id;
	event_notify.session_id = (void *)(uintptr_t)pkt->session_id;
	event_notify.status = VIDC_ERR_NONE;
	event_notify.hal_event_type = HAL_EVENT_RELEASE_BUFFER_REFERENCE;
	event_notify.packet_buffer = data->packet_buffer;
	event_notify.extra_data_buffer = data->extra_data_buffer;

	*info = (struct msm_vidc_cb_info) {
		.response_type =  HAL_SESSION_EVENT_CHANGE,
		.response.event = event_notify,
	};

	return 0;
}

static int hfi_process_sys_error(u32 device_id, struct msm_vidc_cb_info *info)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};

	cmd_done.device_id = device_id;

	*info = (struct msm_vidc_cb_info) {
		.response_type =  HAL_SYS_ERROR,
		.response.cmd = cmd_done,
	};

	return 0;
}

static int hfi_process_session_error(u32 device_id,
		struct hfi_msg_event_notify_packet *pkt,
		struct msm_vidc_cb_info *info)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};

	cmd_done.device_id = device_id;
	cmd_done.session_id = (void *)(uintptr_t)pkt->session_id;
	cmd_done.status = hfi_map_err_status(pkt->event_data1);
	dprintk(VIDC_INFO, "Received: SESSION_ERROR with event id : %d\n",
		pkt->event_data1);
	switch (pkt->event_data1) {
	case HFI_ERR_SESSION_INVALID_SCALE_FACTOR:
	case HFI_ERR_SESSION_UNSUPPORT_BUFFERTYPE:
	case HFI_ERR_SESSION_UNSUPPORTED_SETTING:
	case HFI_ERR_SESSION_UPSCALE_NOT_SUPPORTED:
		cmd_done.status = VIDC_ERR_NONE;
		dprintk(VIDC_INFO, "Non Fatal: HFI_EVENT_SESSION_ERROR\n");
		*info = (struct msm_vidc_cb_info) {
			.response_type =  HAL_RESPONSE_UNUSED,
			.response.cmd = cmd_done,
		};
		return 0;
	default:
		dprintk(VIDC_ERR, "HFI_EVENT_SESSION_ERROR\n");
		*info = (struct msm_vidc_cb_info) {
			.response_type =  HAL_SESSION_ERROR,
			.response.cmd = cmd_done,
		};
		return 0;
	}
}

static int hfi_process_event_notify(u32 device_id,
		struct hfi_msg_event_notify_packet *pkt,
		struct msm_vidc_cb_info *info)
{
	dprintk(VIDC_DBG, "Received: EVENT_NOTIFY\n");

	if (pkt->size < sizeof(struct hfi_msg_event_notify_packet)) {
		dprintk(VIDC_ERR, "Invalid Params\n");
		return -E2BIG;
	}

	switch (pkt->event_id) {
	case HFI_EVENT_SYS_ERROR:
		dprintk(VIDC_ERR, "HFI_EVENT_SYS_ERROR: %d, %#x\n",
			pkt->event_data1, pkt->event_data2);
		return hfi_process_sys_error(device_id, info);
	case HFI_EVENT_SESSION_ERROR:
		dprintk(VIDC_INFO, "HFI_EVENT_SESSION_ERROR[%#x]\n",
				pkt->session_id);
		return hfi_process_session_error(device_id, pkt, info);

	case HFI_EVENT_SESSION_SEQUENCE_CHANGED:
		dprintk(VIDC_INFO, "HFI_EVENT_SESSION_SEQUENCE_CHANGED[%#x]\n",
			pkt->session_id);
		return hfi_process_sess_evt_seq_changed(device_id, pkt, info);

	case HFI_EVENT_RELEASE_BUFFER_REFERENCE:
		dprintk(VIDC_INFO, "HFI_EVENT_RELEASE_BUFFER_REFERENCE[%#x]\n",
			pkt->session_id);
		return hfi_process_evt_release_buffer_ref(device_id, pkt, info);

	case HFI_EVENT_SESSION_PROPERTY_CHANGED:
	default:
		*info = (struct msm_vidc_cb_info) {
			.response_type =  HAL_RESPONSE_UNUSED,
		};

		return 0;
	}
}

static int hfi_process_sys_init_done(u32 device_id,
		struct hfi_msg_sys_init_done_packet *pkt,
		struct msm_vidc_cb_info *info)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};
	enum vidc_status status = VIDC_ERR_NONE;

	dprintk(VIDC_DBG, "RECEIVED: SYS_INIT_DONE\n");
	if (sizeof(struct hfi_msg_sys_init_done_packet) > pkt->size) {
		dprintk(VIDC_ERR, "%s: bad_pkt_size: %d\n", __func__,
				pkt->size);
		return -E2BIG;
	}
	if (!pkt->num_properties) {
		dprintk(VIDC_ERR,
				"hal_process_sys_init_done: no_properties\n");
		status = VIDC_ERR_FAIL;
		goto err_no_prop;
	}

	status = hfi_map_err_status(pkt->error_type);
	if (status) {
		dprintk(VIDC_ERR, "%s: status %#x\n",
			__func__, status);
		goto err_no_prop;
	}

err_no_prop:
	cmd_done.device_id = device_id;
	cmd_done.session_id = NULL;
	cmd_done.status = (u32)status;
	cmd_done.size = sizeof(struct vidc_hal_sys_init_done);
	*info = (struct msm_vidc_cb_info) {
		.response_type =  HAL_SYS_INIT_DONE,
		.response.cmd = cmd_done,
	};
	return 0;
}

static int hfi_process_sys_rel_resource_done(u32 device_id,
		struct hfi_msg_sys_release_resource_done_packet *pkt,
		struct msm_vidc_cb_info *info)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};
	enum vidc_status status = VIDC_ERR_NONE;
	u32 pkt_size;

	dprintk(VIDC_DBG, "RECEIVED: SYS_RELEASE_RESOURCE_DONE\n");
	pkt_size = sizeof(struct hfi_msg_sys_release_resource_done_packet);
	if (pkt_size > pkt->size) {
		dprintk(VIDC_ERR,
			"hal_process_sys_rel_resource_done: bad size: %d\n",
			pkt->size);
		return -E2BIG;
	}

	status = hfi_map_err_status(pkt->error_type);
	cmd_done.device_id = device_id;
	cmd_done.session_id = NULL;
	cmd_done.status = (u32) status;
	cmd_done.size = 0;

	*info = (struct msm_vidc_cb_info) {
		.response_type =  HAL_SYS_RELEASE_RESOURCE_DONE,
		.response.cmd = cmd_done,
	};

	return 0;
}

enum hal_capability get_hal_cap_type(u32 capability_type)
{
	enum hal_capability hal_cap = 0;

	switch (capability_type) {
	case HFI_CAPABILITY_FRAME_WIDTH:
		hal_cap = HAL_CAPABILITY_FRAME_WIDTH;
		break;
	case HFI_CAPABILITY_FRAME_HEIGHT:
		hal_cap = HAL_CAPABILITY_FRAME_HEIGHT;
		break;
	case HFI_CAPABILITY_MBS_PER_FRAME:
		hal_cap = HAL_CAPABILITY_MBS_PER_FRAME;
		break;
	case HFI_CAPABILITY_MBS_PER_SECOND:
		hal_cap = HAL_CAPABILITY_MBS_PER_SECOND;
		break;
	case HFI_CAPABILITY_FRAMERATE:
		hal_cap = HAL_CAPABILITY_FRAMERATE;
		break;
	case HFI_CAPABILITY_SCALE_X:
		hal_cap = HAL_CAPABILITY_SCALE_X;
		break;
	case HFI_CAPABILITY_SCALE_Y:
		hal_cap = HAL_CAPABILITY_SCALE_Y;
		break;
	case HFI_CAPABILITY_BITRATE:
		hal_cap = HAL_CAPABILITY_BITRATE;
		break;
	case HFI_CAPABILITY_BFRAME:
		hal_cap = HAL_CAPABILITY_BFRAME;
		break;
	case HFI_CAPABILITY_PEAKBITRATE:
		hal_cap = HAL_CAPABILITY_PEAKBITRATE;
		break;
	case HFI_CAPABILITY_HIER_P_NUM_ENH_LAYERS:
		hal_cap = HAL_CAPABILITY_HIER_P_NUM_ENH_LAYERS;
		break;
	case HFI_CAPABILITY_ENC_LTR_COUNT:
		hal_cap = HAL_CAPABILITY_ENC_LTR_COUNT;
		break;
	case HFI_CAPABILITY_CP_OUTPUT2_THRESH:
		hal_cap = HAL_CAPABILITY_SECURE_OUTPUT2_THRESHOLD;
		break;
	case HFI_CAPABILITY_HIER_B_NUM_ENH_LAYERS:
		hal_cap = HAL_CAPABILITY_HIER_B_NUM_ENH_LAYERS;
		break;
	case HFI_CAPABILITY_LCU_SIZE:
		hal_cap = HAL_CAPABILITY_LCU_SIZE;
		break;
	case HFI_CAPABILITY_HIER_P_HYBRID_NUM_ENH_LAYERS:
		hal_cap = HAL_CAPABILITY_HIER_P_HYBRID_NUM_ENH_LAYERS;
		break;
	case HFI_CAPABILITY_MBS_PER_SECOND_POWERSAVE:
		hal_cap = HAL_CAPABILITY_MBS_PER_SECOND_POWER_SAVE;
		break;
	default:
		dprintk(VIDC_DBG, "%s: unknown capablity %#x\n",
			__func__, capability_type);
		break;
	}

	return hal_cap;
}

static inline void copy_cap_prop(
		struct hfi_capability_supported *in,
		struct msm_vidc_capability *capability)
{
	struct hal_capability_supported *out = NULL;

	if (!in || !capability) {
		dprintk(VIDC_ERR, "%s Invalid input parameters\n",
			__func__);
		return;
	}

	switch (in->capability_type) {
	case HFI_CAPABILITY_FRAME_WIDTH:
		out = &capability->width;
		break;
	case HFI_CAPABILITY_FRAME_HEIGHT:
		out = &capability->height;
		break;
	case HFI_CAPABILITY_MBS_PER_FRAME:
		out = &capability->mbs_per_frame;
		break;
	case HFI_CAPABILITY_MBS_PER_SECOND:
		out = &capability->mbs_per_sec;
		break;
	case HFI_CAPABILITY_FRAMERATE:
		out = &capability->frame_rate;
		break;
	case HFI_CAPABILITY_SCALE_X:
		out = &capability->scale_x;
		break;
	case HFI_CAPABILITY_SCALE_Y:
		out = &capability->scale_y;
		break;
	case HFI_CAPABILITY_BITRATE:
		out = &capability->bitrate;
		break;
	case HFI_CAPABILITY_BFRAME:
		out = &capability->bframe;
		break;
	case HFI_CAPABILITY_PEAKBITRATE:
		out = &capability->peakbitrate;
		break;
	case HFI_CAPABILITY_HIER_P_NUM_ENH_LAYERS:
		out = &capability->hier_p;
		break;
	case HFI_CAPABILITY_ENC_LTR_COUNT:
		out = &capability->ltr_count;
		break;
	case HFI_CAPABILITY_CP_OUTPUT2_THRESH:
		out = &capability->secure_output2_threshold;
		break;
	case HFI_CAPABILITY_HIER_B_NUM_ENH_LAYERS:
		out = &capability->hier_b;
		break;
	case HFI_CAPABILITY_LCU_SIZE:
		out = &capability->lcu_size;
		break;
	case HFI_CAPABILITY_HIER_P_HYBRID_NUM_ENH_LAYERS:
		out = &capability->hier_p_hybrid;
		break;
	case HFI_CAPABILITY_MBS_PER_SECOND_POWERSAVE:
		out = &capability->mbs_per_sec_power_save;
		break;
	default:
		dprintk(VIDC_DBG, "%s: unknown capablity %#x\n",
			__func__, in->capability_type);
		break;
	}

	if (out) {
		out->capability_type = get_hal_cap_type(in->capability_type);
		out->min = in->min;
		out->max = in->max;
		out->step_size = in->step_size;
	}
}

static int hfi_fill_codec_info(u8 *data_ptr,
		struct vidc_hal_sys_init_done *sys_init_done) {
	u32 i;
	u32 codecs = 0, codec_count = 0, size = 0;
	struct msm_vidc_capability *capability;
	u32 prop_id = *((u32 *)data_ptr);
	u8 *orig_data_ptr = data_ptr;

	if (prop_id ==  HFI_PROPERTY_PARAM_CODEC_SUPPORTED) {
		struct hfi_codec_supported *prop;

		data_ptr = data_ptr + sizeof(u32);
		prop = (struct hfi_codec_supported *) data_ptr;
		sys_init_done->dec_codec_supported =
			prop->decoder_codec_supported;
		sys_init_done->enc_codec_supported =
			prop->encoder_codec_supported;
		size = sizeof(struct hfi_codec_supported) + sizeof(u32);
	} else {
		dprintk(VIDC_WARN,
			"%s: prop_id %#x, expected codec_supported property\n",
			__func__, prop_id);
	}

	codecs = sys_init_done->dec_codec_supported;
	for (i = 0; i < 8 * sizeof(codecs); i++) {
		if ((1 << i) & codecs) {
			capability =
				&sys_init_done->capabilities[codec_count++];
			capability->codec =
				vidc_get_hal_codec((1 << i) & codecs);
			capability->domain =
				vidc_get_hal_domain(HFI_VIDEO_DOMAIN_DECODER);
			if (codec_count == VIDC_MAX_DECODE_SESSIONS) {
				dprintk(VIDC_ERR,
					"Max supported decoder sessions reached\n");
				break;
			}
		}
	}
	codecs = sys_init_done->enc_codec_supported;
	for (i = 0; i < 8 * sizeof(codecs); i++) {
		if ((1 << i) & codecs) {
			capability =
				&sys_init_done->capabilities[codec_count++];
			capability->codec =
				vidc_get_hal_codec((1 << i) & codecs);
			capability->domain =
				vidc_get_hal_domain(HFI_VIDEO_DOMAIN_ENCODER);
			if (codec_count == VIDC_MAX_SESSIONS) {
				dprintk(VIDC_ERR,
					"Max supported sessions reached\n");
				break;
			}
		}
	}
	sys_init_done->codec_count = codec_count;

	prop_id = *((u32 *)(orig_data_ptr + size));
	if (prop_id == HFI_PROPERTY_PARAM_MAX_SESSIONS_SUPPORTED) {
		struct hfi_max_sessions_supported *prop =
			(struct hfi_max_sessions_supported *)
			(orig_data_ptr + size + sizeof(u32));

		sys_init_done->max_sessions_supported = prop->max_sessions;
		size += sizeof(struct hfi_max_sessions_supported) + sizeof(u32);
		dprintk(VIDC_DBG, "max_sessions_supported %d\n",
				prop->max_sessions);
	}
	return size;
}

enum vidc_status hfi_process_session_init_done_prop_read(
		struct hfi_msg_sys_session_init_done_packet *pkt,
		struct vidc_hal_session_init_done *session_init_done)
{
	enum vidc_status status = VIDC_ERR_NONE;
	struct msm_vidc_capability *capability = NULL;
	u32 rem_bytes, num_properties;
	u8 *data_ptr;

	rem_bytes = pkt->size - sizeof(struct
			hfi_msg_sys_session_init_done_packet) + sizeof(u32);
	if (!rem_bytes) {
		dprintk(VIDC_ERR, "%s: invalid property info\n", __func__);
		return VIDC_ERR_FAIL;
	}

	status = hfi_map_err_status(pkt->error_type);
	if (status) {
		dprintk(VIDC_ERR, "%s: error status 0x%x\n", __func__, status);
		return status;
	}

	data_ptr = (u8 *)&pkt->rg_property_data[0];
	num_properties = pkt->num_properties;

	capability = &session_init_done->capability;
	status = hfi_parse_init_done_properties(
			capability, 1, data_ptr, num_properties, rem_bytes,
			vidc_get_hfi_codec(capability->codec),
			vidc_get_hfi_domain(capability->domain));
	if (status) {
		dprintk(VIDC_ERR, "%s: parse status 0x%x\n", __func__, status);
		return status;
	}

	return status;
}

static int copy_caps_to_sessions(struct hfi_capability_supported *cap,
		u32 num_caps, struct msm_vidc_capability *capabilities,
		u32 num_sessions, u32 codecs, u32 domain)
{
	u32 i = 0, j = 0;
	struct msm_vidc_capability *capability;
	u32 sess_codec;
	u32 sess_domain;

	/*
	 * iterate over num_sessions and copy all the capabilities
	 * to matching sessions.
	 */
	for (i = 0; i < num_sessions; i++) {
		sess_codec = 0;
		sess_domain = 0;
		capability = &capabilities[i];

		if (capability->codec)
			sess_codec =
				vidc_get_hfi_codec(capability->codec);
		if (capability->domain)
			sess_domain =
				vidc_get_hfi_domain(capability->domain);

		if (!(sess_codec & codecs && sess_domain & domain))
			continue;

		for (j = 0; j < num_caps; j++)
			copy_cap_prop(&cap[j], capability);
	}

	return 0;
}

static int copy_alloc_mode_to_sessions(
		struct hfi_buffer_alloc_mode_supported *prop,
		struct msm_vidc_capability *capabilities,
		u32 num_sessions, u32 codecs, u32 domain)
{
	u32 i = 0, j = 0;
	struct msm_vidc_capability *capability;
	u32 sess_codec;
	u32 sess_domain;

	/*
	 * iterate over num_sessions and copy all the entries
	 * to matching sessions.
	 */
	for (i = 0; i < num_sessions; i++) {
		sess_codec = 0;
		sess_domain = 0;
		capability = &capabilities[i];

		if (capability->codec)
			sess_codec =
				vidc_get_hfi_codec(capability->codec);
		if (capability->domain)
			sess_domain =
				vidc_get_hfi_domain(capability->domain);

		if (!(sess_codec & codecs && sess_domain & domain))
			continue;

		for (j = 0; j < prop->num_entries; j++) {
			if (prop->buffer_type == HFI_BUFFER_OUTPUT ||
				prop->buffer_type == HFI_BUFFER_OUTPUT2) {
				switch (prop->rg_data[j]) {
				case HFI_BUFFER_MODE_STATIC:
					capability->alloc_mode_out |=
						HAL_BUFFER_MODE_STATIC;
					break;
				case HFI_BUFFER_MODE_RING:
					capability->alloc_mode_out |=
						HAL_BUFFER_MODE_RING;
					break;
				case HFI_BUFFER_MODE_DYNAMIC:
					capability->alloc_mode_out |=
						HAL_BUFFER_MODE_DYNAMIC;
					break;
				}
			} else if (prop->buffer_type == HFI_BUFFER_INPUT) {
				switch (prop->rg_data[j]) {
				case HFI_BUFFER_MODE_STATIC:
					capability->alloc_mode_in |=
						HAL_BUFFER_MODE_STATIC;
					break;
				case HFI_BUFFER_MODE_RING:
					capability->alloc_mode_in |=
						HAL_BUFFER_MODE_RING;
					break;
				case HFI_BUFFER_MODE_DYNAMIC:
					capability->alloc_mode_in |=
						HAL_BUFFER_MODE_DYNAMIC;
					break;
				}
			}
		}
	}

	return 0;
}

static enum vidc_status hfi_parse_init_done_properties(
		struct msm_vidc_capability *capabilities,
		u32 num_sessions, u8 *data_ptr, u32 num_properties,
		u32 rem_bytes, u32 codecs, u32 domain)
{
	enum vidc_status status = VIDC_ERR_NONE;
	u32 prop_id, next_offset;

	while (status == VIDC_ERR_NONE && num_properties &&
			rem_bytes >= sizeof(u32)) {

		prop_id = *((u32 *)data_ptr);
		next_offset = sizeof(u32);

		switch (prop_id) {
		case HFI_PROPERTY_PARAM_CODEC_MASK_SUPPORTED:
		{
			struct hfi_codec_mask_supported *prop =
				(struct hfi_codec_mask_supported *)
				(data_ptr + next_offset);

			codecs = prop->codecs;
			domain = prop->video_domains;
			next_offset += sizeof(struct hfi_codec_mask_supported);
			num_properties--;
			break;
		}
		case HFI_PROPERTY_PARAM_CAPABILITY_SUPPORTED:
		{
			struct hfi_capability_supported_info *prop =
				(struct hfi_capability_supported_info *)
				(data_ptr + next_offset);

			if ((rem_bytes - next_offset) < prop->num_capabilities *
				sizeof(struct hfi_capability_supported)) {
				status = VIDC_ERR_BAD_PARAM;
				break;
			}
			next_offset += sizeof(u32) +
				prop->num_capabilities *
				sizeof(struct hfi_capability_supported);

			copy_caps_to_sessions(&prop->rg_data[0],
					prop->num_capabilities,
					capabilities, num_sessions,
					codecs, domain);
			num_properties--;
			break;
		}
		case HFI_PROPERTY_PARAM_UNCOMPRESSED_FORMAT_SUPPORTED:
		{
			struct hfi_uncompressed_format_supported *prop =
				(struct hfi_uncompressed_format_supported *)
				(data_ptr + next_offset);
			u32 num_format_entries;
			char *fmt_ptr;
			struct hfi_uncompressed_plane_info *plane_info;

			if ((rem_bytes - next_offset) < sizeof(*prop)) {
				status = VIDC_ERR_BAD_PARAM;
				break;
			}
			num_format_entries = prop->format_entries;
			next_offset = sizeof(*prop);
			fmt_ptr = (char *)&prop->rg_format_info[0];

			while (num_format_entries) {
				u32 bytes_to_skip;

				plane_info =
				(struct hfi_uncompressed_plane_info *) fmt_ptr;

				if ((rem_bytes - next_offset) <
						sizeof(*plane_info)) {
					status = VIDC_ERR_BAD_PARAM;
					break;
				}
				bytes_to_skip = sizeof(*plane_info) -
					sizeof(struct
					hfi_uncompressed_plane_constraints) +
					plane_info->num_planes *
					sizeof(struct
					hfi_uncompressed_plane_constraints);

				fmt_ptr += bytes_to_skip;
				next_offset += bytes_to_skip;
				num_format_entries--;
			}
			num_properties--;
			break;
		}
		case HFI_PROPERTY_PARAM_PROPERTIES_SUPPORTED:
		{
			struct hfi_properties_supported *prop =
				(struct hfi_properties_supported *)
				(data_ptr + next_offset);
			next_offset += sizeof(*prop) - sizeof(u32)
				+ prop->num_properties * sizeof(u32);
			num_properties--;
			break;
		}
		case HFI_PROPERTY_PARAM_PROFILE_LEVEL_SUPPORTED:
		{
			struct msm_vidc_capability capability;
			char *ptr = NULL;
			u32 count = 0;
			u32 prof_count = 0;
			struct hfi_profile_level *prof_level;
			struct hfi_profile_level_supported *prop =
				(struct hfi_profile_level_supported *)
				(data_ptr + next_offset);

			ptr = (char *) &prop->rg_profile_level[0];
			prof_count = prop->profile_count;
			next_offset += sizeof(u32);

			if (prof_count > MAX_PROFILE_COUNT) {
				prof_count = MAX_PROFILE_COUNT;
				dprintk(VIDC_WARN,
					"prop count exceeds max profile count\n");
				break;
			}
			while (prof_count) {
				prof_level = (struct hfi_profile_level *)ptr;
				capability.
				profile_level.profile_level[count].profile
					= prof_level->profile;
				capability.
				profile_level.profile_level[count].level
					= prof_level->level;
				prof_count--;
				count++;
				ptr += sizeof(struct hfi_profile_level);
				next_offset += sizeof(struct hfi_profile_level);
			}
			num_properties--;
			break;
		}
		case HFI_PROPERTY_PARAM_INTERLACE_FORMAT_SUPPORTED:
		{
			next_offset +=
				sizeof(struct hfi_interlace_format_supported);
			num_properties--;
			break;
		}
		case HFI_PROPERTY_PARAM_NAL_STREAM_FORMAT_SUPPORTED:
		{
			next_offset +=
				sizeof(struct hfi_nal_stream_format_supported);
			num_properties--;
			break;
		}
		case HFI_PROPERTY_PARAM_NAL_STREAM_FORMAT_SELECT:
		{
			next_offset += sizeof(u32);
			num_properties--;
			break;
		}
		case HFI_PROPERTY_PARAM_MAX_SEQUENCE_HEADER_SIZE:
		{
			next_offset += sizeof(u32);
			num_properties--;
			break;
		}
		case HFI_PROPERTY_PARAM_VENC_INTRA_REFRESH:
		{
			next_offset +=
				sizeof(struct hfi_intra_refresh);
			num_properties--;
			break;
		}
		case HFI_PROPERTY_PARAM_BUFFER_ALLOC_MODE_SUPPORTED:
		{
			struct hfi_buffer_alloc_mode_supported *prop =
				(struct hfi_buffer_alloc_mode_supported *)
				(data_ptr + next_offset);

			if (prop->num_entries >= 32) {
				dprintk(VIDC_ERR,
					"%s - num_entries: %d from f/w seems suspect\n",
					__func__, prop->num_entries);
				break;
			}
			next_offset +=
				sizeof(struct hfi_buffer_alloc_mode_supported) -
				sizeof(u32) + prop->num_entries * sizeof(u32);

			copy_alloc_mode_to_sessions(prop,
					capabilities, num_sessions,
					codecs, domain);

			num_properties--;
			break;
		}
		default:
			dprintk(VIDC_DBG,
				"%s: default case - data_ptr %pK, prop_id 0x%x\n",
				__func__, data_ptr, prop_id);
			break;
		}
		rem_bytes -= next_offset;
		data_ptr += next_offset;
	}

	return status;
}

enum vidc_status hfi_process_sys_init_done_prop_read(
	struct hfi_msg_sys_init_done_packet *pkt,
	struct vidc_hal_sys_init_done *sys_init_done)
{
	enum vidc_status status = VIDC_ERR_NONE;
	u32 rem_bytes, bytes_read, num_properties;
	u8 *data_ptr;
	u32 codecs = 0, domain = 0;

	if (!pkt || !sys_init_done) {
		dprintk(VIDC_ERR,
			"hfi_msg_sys_init_done: Invalid input\n");
		return VIDC_ERR_FAIL;
	}

	rem_bytes = pkt->size - sizeof(struct
			hfi_msg_sys_init_done_packet) + sizeof(u32);

	if (!rem_bytes) {
		dprintk(VIDC_ERR,
			"hfi_msg_sys_init_done: missing_prop_info\n");
		return VIDC_ERR_FAIL;
	}

	status = hfi_map_err_status(pkt->error_type);
	if (status) {
		dprintk(VIDC_ERR, "%s: status %#x\n", __func__, status);
		return status;
	}

	data_ptr = (u8 *) &pkt->rg_property_data[0];
	num_properties = pkt->num_properties;
	dprintk(VIDC_DBG,
		"%s: data_start %pK, num_properties %#x\n",
		__func__, data_ptr, num_properties);
	if (!num_properties) {
		sys_init_done->capabilities = NULL;
		dprintk(VIDC_DBG,
			"Venus didn't set any properties in SYS_INIT_DONE");
		return status;
	}
	bytes_read = hfi_fill_codec_info(data_ptr, sys_init_done);
	data_ptr += bytes_read;
	rem_bytes -= bytes_read;
	num_properties--;

	status = hfi_parse_init_done_properties(
			sys_init_done->capabilities,
			VIDC_MAX_SESSIONS, data_ptr, num_properties,
			rem_bytes, codecs, domain);
	if (status) {
		dprintk(VIDC_ERR, "%s: parse status %#x\n",
			__func__, status);
		return status;
	}

	return status;
}

static void hfi_process_sess_get_prop_dec_entropy(
	struct hfi_msg_session_property_info_packet *prop,
	enum hal_h264_entropy *entropy)
{
	u32 req_bytes, hfi_entropy;

	req_bytes = prop->size - sizeof(
			struct hfi_msg_session_property_info_packet);

	if (!req_bytes || req_bytes % sizeof(hfi_entropy)) {
		dprintk(VIDC_ERR, "%s: bad packet: %d\n", __func__, req_bytes);
		return;
	}

	hfi_entropy = prop->rg_property_data[1];
	*entropy =
		hfi_entropy == HFI_H264_ENTROPY_CAVLC ? HAL_H264_ENTROPY_CAVLC :
		hfi_entropy == HFI_H264_ENTROPY_CABAC ? HAL_H264_ENTROPY_CABAC :
							HAL_UNUSED_ENTROPY;
}

static void hfi_process_sess_get_prop_profile_level(
	struct hfi_msg_session_property_info_packet *prop,
	struct hfi_profile_level *profile_level)
{
	struct hfi_profile_level *hfi_profile_level;
	u32 req_bytes;

	dprintk(VIDC_DBG, "Entered %s\n", __func__);
	if (!prop) {
		dprintk(VIDC_ERR,
			"hal_process_sess_get_profile_level: bad_prop: %pK\n",
			prop);
		return;
	}
	req_bytes = prop->size - sizeof(
			struct hfi_msg_session_property_info_packet);

	if (!req_bytes || req_bytes % sizeof(struct hfi_profile_level)) {
		dprintk(VIDC_ERR,
			"hal_process_sess_get_profile_level: bad_pkt: %d\n",
			req_bytes);
		return;
	}
	hfi_profile_level = (struct hfi_profile_level *)
				&prop->rg_property_data[1];
	profile_level->profile = hfi_profile_level->profile;
	profile_level->level = hfi_profile_level->level;
	dprintk(VIDC_DBG, "%s profile: %d level: %d\n",
		__func__, profile_level->profile,
		profile_level->level);
}

static void hfi_process_sess_get_prop_buf_req(
	struct hfi_msg_session_property_info_packet *prop,
	struct buffer_requirements *buffreq)
{
	struct hfi_buffer_requirements *hfi_buf_req;
	u32 req_bytes;

	if (!prop) {
		dprintk(VIDC_ERR,
			"hal_process_sess_get_prop_buf_req: bad_prop: %pK\n",
			prop);
		return;
	}

	req_bytes = prop->size - sizeof(
			struct hfi_msg_session_property_info_packet);
	if (!req_bytes || req_bytes % sizeof(struct hfi_buffer_requirements) ||
		!prop->rg_property_data[1]) {
		dprintk(VIDC_ERR,
			"hal_process_sess_get_prop_buf_req: bad_pkt: %d\n",
			req_bytes);
		return;
	}

	hfi_buf_req = (struct hfi_buffer_requirements *)
		&prop->rg_property_data[1];

	if (!hfi_buf_req) {
		dprintk(VIDC_ERR, "%s - invalid buffer req pointer\n",
			__func__);
		return;
	}

	while (req_bytes) {
		if (hfi_buf_req->buffer_size &&
			hfi_buf_req->buffer_count_min > hfi_buf_req->
			buffer_count_actual)
			dprintk(VIDC_WARN,
				"Bad buffer requirements for %#x: min %d, actual %d\n",
				hfi_buf_req->buffer_type,
				hfi_buf_req->buffer_count_min,
				hfi_buf_req->buffer_count_actual);

		dprintk(VIDC_DBG, "got buffer requirements for: %d\n",
					hfi_buf_req->buffer_type);
		switch (hfi_buf_req->buffer_type) {
		case HFI_BUFFER_INPUT:
			memcpy(&buffreq->buffer[0], hfi_buf_req,
				sizeof(struct hfi_buffer_requirements));
			buffreq->buffer[0].buffer_type = HAL_BUFFER_INPUT;
			break;
		case HFI_BUFFER_OUTPUT:
			memcpy(&buffreq->buffer[1], hfi_buf_req,
			sizeof(struct hfi_buffer_requirements));
			buffreq->buffer[1].buffer_type = HAL_BUFFER_OUTPUT;
			break;
		case HFI_BUFFER_OUTPUT2:
			memcpy(&buffreq->buffer[2], hfi_buf_req,
				sizeof(struct hfi_buffer_requirements));
			buffreq->buffer[2].buffer_type = HAL_BUFFER_OUTPUT2;
			break;
		case HFI_BUFFER_EXTRADATA_INPUT:
			memcpy(&buffreq->buffer[3], hfi_buf_req,
				sizeof(struct hfi_buffer_requirements));
			buffreq->buffer[3].buffer_type =
				HAL_BUFFER_EXTRADATA_INPUT;
			break;
		case HFI_BUFFER_EXTRADATA_OUTPUT:
			memcpy(&buffreq->buffer[4], hfi_buf_req,
				sizeof(struct hfi_buffer_requirements));
			buffreq->buffer[4].buffer_type =
				HAL_BUFFER_EXTRADATA_OUTPUT;
			break;
		case HFI_BUFFER_EXTRADATA_OUTPUT2:
			memcpy(&buffreq->buffer[5], hfi_buf_req,
				sizeof(struct hfi_buffer_requirements));
			buffreq->buffer[5].buffer_type =
				HAL_BUFFER_EXTRADATA_OUTPUT2;
			break;
		case HFI_BUFFER_INTERNAL_SCRATCH:
			memcpy(&buffreq->buffer[6], hfi_buf_req,
			sizeof(struct hfi_buffer_requirements));
			buffreq->buffer[6].buffer_type =
				HAL_BUFFER_INTERNAL_SCRATCH;
			break;
		case HFI_BUFFER_INTERNAL_SCRATCH_1:
			memcpy(&buffreq->buffer[7], hfi_buf_req,
				sizeof(struct hfi_buffer_requirements));
			buffreq->buffer[7].buffer_type =
				HAL_BUFFER_INTERNAL_SCRATCH_1;
			break;
		case HFI_BUFFER_INTERNAL_SCRATCH_2:
			memcpy(&buffreq->buffer[8], hfi_buf_req,
				sizeof(struct hfi_buffer_requirements));
			buffreq->buffer[8].buffer_type =
				HAL_BUFFER_INTERNAL_SCRATCH_2;
			break;
		case HFI_BUFFER_INTERNAL_PERSIST:
			memcpy(&buffreq->buffer[9], hfi_buf_req,
			sizeof(struct hfi_buffer_requirements));
			buffreq->buffer[9].buffer_type =
				HAL_BUFFER_INTERNAL_PERSIST;
			break;
		case HFI_BUFFER_INTERNAL_PERSIST_1:
			memcpy(&buffreq->buffer[10], hfi_buf_req,
				sizeof(struct hfi_buffer_requirements));
			buffreq->buffer[10].buffer_type =
				HAL_BUFFER_INTERNAL_PERSIST_1;
			break;
		default:
			dprintk(VIDC_ERR,
			"hal_process_sess_get_prop_buf_req: bad_buffer_type: %d\n",
			hfi_buf_req->buffer_type);
			break;
		}
		req_bytes -= sizeof(struct hfi_buffer_requirements);
		hfi_buf_req++;
	}
}

static int hfi_process_session_prop_info(u32 device_id,
		struct hfi_msg_session_property_info_packet *pkt,
		struct msm_vidc_cb_info *info)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};
	struct hfi_profile_level profile_level = {0};
	enum hal_h264_entropy entropy = HAL_UNUSED_ENTROPY;
	struct buffer_requirements buff_req = { { {0} } };

	dprintk(VIDC_DBG, "Received SESSION_PROPERTY_INFO[%#x]\n",
			pkt->session_id);

	if (pkt->size < sizeof(struct hfi_msg_session_property_info_packet)) {
		dprintk(VIDC_ERR,
				"hal_process_session_prop_info: bad_pkt_size\n");
		return -E2BIG;
	} else if (!pkt->num_properties) {
		dprintk(VIDC_ERR,
			"hal_process_session_prop_info: no_properties\n");
		return -EINVAL;
	}

	switch (pkt->rg_property_data[0]) {
	case HFI_PROPERTY_CONFIG_BUFFER_REQUIREMENTS:
		hfi_process_sess_get_prop_buf_req(pkt, &buff_req);
		cmd_done.device_id = device_id;
		cmd_done.session_id = (void *)(uintptr_t)pkt->session_id;
		cmd_done.status = VIDC_ERR_NONE;
		cmd_done.data.property.buf_req = buff_req;
		cmd_done.size = sizeof(buff_req);

		*info = (struct msm_vidc_cb_info) {
			.response_type =  HAL_SESSION_PROPERTY_INFO,
			.response.cmd = cmd_done,
		};

		return 0;
	case HFI_PROPERTY_PARAM_PROFILE_LEVEL_CURRENT:
		hfi_process_sess_get_prop_profile_level(pkt, &profile_level);
		cmd_done.device_id = device_id;
		cmd_done.session_id = (void *)(uintptr_t)pkt->session_id;
		cmd_done.status = VIDC_ERR_NONE;
		cmd_done.data.property.profile_level =
			(struct hal_profile_level) {
				.profile = profile_level.profile,
				.level = profile_level.level,
			};
		cmd_done.size = sizeof(struct hal_profile_level);

		*info = (struct msm_vidc_cb_info) {
			.response_type =  HAL_SESSION_PROPERTY_INFO,
			.response.cmd = cmd_done,
		};
		return 0;
	case HFI_PROPERTY_CONFIG_VDEC_ENTROPY:
		hfi_process_sess_get_prop_dec_entropy(pkt, &entropy);
		cmd_done.device_id = device_id;
		cmd_done.session_id = (void *)(uintptr_t)pkt->session_id;
		cmd_done.status = VIDC_ERR_NONE;
		cmd_done.data.property.h264_entropy = entropy;
		cmd_done.size = sizeof(enum hal_h264_entropy);

		*info = (struct msm_vidc_cb_info) {
			.response_type =  HAL_SESSION_PROPERTY_INFO,
			.response.cmd = cmd_done,
		};
		return 0;
	default:
		dprintk(VIDC_DBG,
				"hal_process_session_prop_info: unknown_prop_id: %x\n",
				pkt->rg_property_data[0]);
		return -ENOTSUPP;
	}
}

static int hfi_process_session_init_done(u32 device_id,
		struct hfi_msg_sys_session_init_done_packet *pkt,
		struct msm_vidc_cb_info *info)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};
	struct vidc_hal_session_init_done session_init_done = { {0} };

	dprintk(VIDC_DBG, "RECEIVED: SESSION_INIT_DONE[%x]\n", pkt->session_id);

	if (sizeof(struct hfi_msg_sys_session_init_done_packet) > pkt->size) {
		dprintk(VIDC_ERR,
				"hal_process_session_init_done: bad_pkt_size\n");
		return -E2BIG;
	}

	cmd_done.device_id = device_id;
	cmd_done.session_id = (void *)(uintptr_t)pkt->session_id;
	cmd_done.status = hfi_map_err_status(pkt->error_type);
	if (!cmd_done.status) {
		cmd_done.status = hfi_process_session_init_done_prop_read(
			pkt, &session_init_done);
	}

	cmd_done.data.session_init_done = session_init_done;
	cmd_done.size = sizeof(struct vidc_hal_session_init_done);

	*info = (struct msm_vidc_cb_info) {
		.response_type =  HAL_SESSION_INIT_DONE,
		.response.cmd = cmd_done,
	};

	return 0;
}

static int hfi_process_session_load_res_done(u32 device_id,
		struct hfi_msg_session_load_resources_done_packet *pkt,
		struct msm_vidc_cb_info *info)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};

	dprintk(VIDC_DBG, "RECEIVED: SESSION_LOAD_RESOURCES_DONE[%#x]\n",
		pkt->session_id);

	if (sizeof(struct hfi_msg_session_load_resources_done_packet) !=
		pkt->size) {
		dprintk(VIDC_ERR,
				"hal_process_session_load_res_done: bad packet size: %d\n",
				pkt->size);
		return -E2BIG;
	}

	cmd_done.device_id = device_id;
	cmd_done.session_id = (void *)(uintptr_t)pkt->session_id;
	cmd_done.status = hfi_map_err_status(pkt->error_type);
	cmd_done.size = 0;

	*info = (struct msm_vidc_cb_info) {
		.response_type =  HAL_SESSION_LOAD_RESOURCE_DONE,
		.response.cmd = cmd_done,
	};

	return 0;
}

static int hfi_process_session_flush_done(u32 device_id,
		struct hfi_msg_session_flush_done_packet *pkt,
		struct msm_vidc_cb_info *info)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};

	dprintk(VIDC_DBG, "RECEIVED: SESSION_FLUSH_DONE[%#x]\n",
			pkt->session_id);

	if (sizeof(struct hfi_msg_session_flush_done_packet) != pkt->size) {
		dprintk(VIDC_ERR,
				"hal_process_session_flush_done: bad packet size: %d\n",
				pkt->size);
		return -E2BIG;
	}

	cmd_done.device_id = device_id;
	cmd_done.session_id = (void *)(uintptr_t)pkt->session_id;
	cmd_done.status = hfi_map_err_status(pkt->error_type);
	cmd_done.size = sizeof(u32);

	switch (pkt->flush_type) {
	case HFI_FLUSH_OUTPUT:
		cmd_done.data.flush_type = HAL_FLUSH_OUTPUT;
		break;
	case HFI_FLUSH_INPUT:
		cmd_done.data.flush_type = HAL_FLUSH_INPUT;
		break;
	case HFI_FLUSH_ALL:
		cmd_done.data.flush_type = HAL_FLUSH_ALL;
		break;
	default:
		dprintk(VIDC_ERR,
				"%s: invalid flush type!", __func__);
		return -EINVAL;
	}

	*info = (struct msm_vidc_cb_info) {
		.response_type =  HAL_SESSION_FLUSH_DONE,
		.response.cmd = cmd_done,
	};

	return 0;
}

static int hfi_process_session_etb_done(u32 device_id,
		struct hfi_msg_session_empty_buffer_done_packet *pkt,
		struct msm_vidc_cb_info *info)
{
	struct msm_vidc_cb_data_done data_done = {0};
	struct hfi_picture_type *hfi_picture_type = NULL;

	dprintk(VIDC_DBG, "RECEIVED: SESSION_ETB_DONE[%#x]\n", pkt->session_id);

	if (!pkt || pkt->size <
		sizeof(struct hfi_msg_session_empty_buffer_done_packet)) {
		dprintk(VIDC_ERR,
				"hal_process_session_etb_done: bad_pkt_size\n");
		return -E2BIG;
	}

	data_done.device_id = device_id;
	data_done.session_id = (void *)(uintptr_t)pkt->session_id;
	data_done.status = hfi_map_err_status(pkt->error_type);
	data_done.size = sizeof(struct msm_vidc_cb_data_done);
	data_done.clnt_data = pkt->input_tag;
	data_done.input_done.offset = pkt->offset;
	data_done.input_done.filled_len = pkt->filled_len;
	data_done.input_done.packet_buffer =
		(ion_phys_addr_t)pkt->packet_buffer;
	data_done.input_done.extra_data_buffer =
		(ion_phys_addr_t)pkt->extra_data_buffer;
	data_done.input_done.status =
		hfi_map_err_status(pkt->error_type);
	hfi_picture_type = (struct hfi_picture_type *)&pkt->rgData[0];
	if (hfi_picture_type->is_sync_frame) {
		if (hfi_picture_type->picture_type)
			data_done.input_done.flags =
				hfi_picture_type->picture_type;
		else
			dprintk(VIDC_DBG,
				"Non-Sync frame sent for H264/HEVC\n");
	}

	trace_msm_v4l2_vidc_buffer_event_end("ETB",
		(u32)pkt->packet_buffer, -1, -1,
		pkt->filled_len, pkt->offset);

	*info = (struct msm_vidc_cb_info) {
		.response_type =  HAL_SESSION_ETB_DONE,
		.response.data = data_done,
	};

	return 0;
}

static int hfi_process_session_ftb_done(
		u32 device_id, struct vidc_hal_msg_pkt_hdr *msg_hdr,
		struct msm_vidc_cb_info *info)
{
	struct msm_vidc_cb_data_done data_done = {0};
	bool is_decoder = false, is_encoder = false;

	if (!msg_hdr) {
		dprintk(VIDC_ERR, "Invalid Params\n");
		return -EINVAL;
	}

	is_encoder = msg_hdr->size == sizeof(struct
			hfi_msg_session_fill_buffer_done_compressed_packet) + 4;
	is_decoder = msg_hdr->size == sizeof(struct
			hfi_msg_session_fbd_uncompressed_plane0_packet) + 4;

	if (!(is_encoder ^ is_decoder)) {
		dprintk(VIDC_ERR, "Ambiguous packet (%#x) received (size %d)\n",
				msg_hdr->packet, msg_hdr->size);
		return -EBADHANDLE;
	}

	if (is_encoder) {
		struct hfi_msg_session_fill_buffer_done_compressed_packet *pkt =
		(struct hfi_msg_session_fill_buffer_done_compressed_packet *)
		msg_hdr;
		dprintk(VIDC_DBG, "RECEIVED: SESSION_FTB_DONE[%#x]\n",
				pkt->session_id);
		if (sizeof(struct
			hfi_msg_session_fill_buffer_done_compressed_packet)
			> pkt->size) {
			dprintk(VIDC_ERR,
				"hal_process_session_ftb_done: bad_pkt_size\n");
			return -E2BIG;
		} else if (pkt->error_type != HFI_ERR_NONE) {
			dprintk(VIDC_ERR,
				"got buffer back with error %x\n",
				pkt->error_type);
			/* Proceed with the FBD */
		}

		data_done.device_id = device_id;
		data_done.session_id = (void *)(uintptr_t)pkt->session_id;
		data_done.status = hfi_map_err_status(pkt->error_type);
		data_done.size = sizeof(struct msm_vidc_cb_data_done);
		data_done.clnt_data = 0;

		data_done.output_done.timestamp_hi = pkt->time_stamp_hi;
		data_done.output_done.timestamp_lo = pkt->time_stamp_lo;
		data_done.output_done.flags1 = pkt->flags;
		data_done.output_done.mark_target = pkt->mark_target;
		data_done.output_done.mark_data = pkt->mark_data;
		data_done.output_done.stats = pkt->stats;
		data_done.output_done.offset1 = pkt->offset;
		data_done.output_done.alloc_len1 = pkt->alloc_len;
		data_done.output_done.filled_len1 = pkt->filled_len;
		data_done.output_done.picture_type = pkt->picture_type;
		data_done.output_done.packet_buffer1 =
			(ion_phys_addr_t)pkt->packet_buffer;
		data_done.output_done.extra_data_buffer =
			(ion_phys_addr_t)pkt->extra_data_buffer;
		data_done.output_done.buffer_type = HAL_BUFFER_OUTPUT;
	} else /* if (is_decoder) */ {
		struct hfi_msg_session_fbd_uncompressed_plane0_packet *pkt =
		(struct	hfi_msg_session_fbd_uncompressed_plane0_packet *)
		msg_hdr;

		dprintk(VIDC_DBG, "RECEIVED: SESSION_FTB_DONE[%#x]\n",
				pkt->session_id);
		if (sizeof(
			struct hfi_msg_session_fbd_uncompressed_plane0_packet) >
			pkt->size) {
			dprintk(VIDC_ERR,
					"hal_process_session_ftb_done: bad_pkt_size\n");
			return -E2BIG;
		}

		data_done.device_id = device_id;
		data_done.session_id = (void *)(uintptr_t)pkt->session_id;
		data_done.status = hfi_map_err_status(pkt->error_type);
		data_done.size = sizeof(struct msm_vidc_cb_data_done);
		data_done.clnt_data = 0;

		data_done.output_done.stream_id = pkt->stream_id;
		data_done.output_done.view_id = pkt->view_id;
		data_done.output_done.timestamp_hi = pkt->time_stamp_hi;
		data_done.output_done.timestamp_lo = pkt->time_stamp_lo;
		data_done.output_done.flags1 = pkt->flags;
		data_done.output_done.mark_target = pkt->mark_target;
		data_done.output_done.mark_data = pkt->mark_data;
		data_done.output_done.stats = pkt->stats;
		data_done.output_done.alloc_len1 = pkt->alloc_len;
		data_done.output_done.filled_len1 = pkt->filled_len;
		data_done.output_done.offset1 = pkt->offset;
		data_done.output_done.frame_width = pkt->frame_width;
		data_done.output_done.frame_height = pkt->frame_height;
		data_done.output_done.start_x_coord = pkt->start_x_coord;
		data_done.output_done.start_y_coord = pkt->start_y_coord;
		data_done.output_done.input_tag1 = pkt->input_tag;
		data_done.output_done.picture_type = pkt->picture_type;
		data_done.output_done.packet_buffer1 = pkt->packet_buffer;
		data_done.output_done.extra_data_buffer =
			pkt->extra_data_buffer;

		if (!pkt->stream_id)
			data_done.output_done.buffer_type = HAL_BUFFER_OUTPUT;
		else if (pkt->stream_id == 1)
			data_done.output_done.buffer_type = HAL_BUFFER_OUTPUT2;
	}

	trace_msm_v4l2_vidc_buffer_event_end("FTB",
		(u32)data_done.output_done.packet_buffer1,
		(((u64)data_done.output_done.timestamp_hi) << 32)
		+ ((u64)data_done.output_done.timestamp_lo),
		data_done.output_done.alloc_len1,
		data_done.output_done.filled_len1,
		data_done.output_done.offset1);

	*info = (struct msm_vidc_cb_info) {
		.response_type =  HAL_SESSION_FTB_DONE,
		.response.data = data_done,
	};

	return 0;
}

static int hfi_process_session_start_done(u32 device_id,
		struct hfi_msg_session_start_done_packet *pkt,
		struct msm_vidc_cb_info *info)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};

	dprintk(VIDC_DBG, "RECEIVED: SESSION_START_DONE[%#x]\n",
			pkt->session_id);

	if (!pkt || pkt->size !=
		sizeof(struct hfi_msg_session_start_done_packet)) {
		dprintk(VIDC_ERR, "%s: bad packet/packet size\n",
			__func__);
		return -E2BIG;
	}

	cmd_done.device_id = device_id;
	cmd_done.session_id = (void *)(uintptr_t)pkt->session_id;
	cmd_done.status = hfi_map_err_status(pkt->error_type);
	cmd_done.size = 0;

	*info = (struct msm_vidc_cb_info) {
		.response_type =  HAL_SESSION_START_DONE,
		.response.cmd = cmd_done,
	};
	return 0;
}

static int hfi_process_session_stop_done(u32 device_id,
		struct hfi_msg_session_stop_done_packet *pkt,
		struct msm_vidc_cb_info *info)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};

	dprintk(VIDC_DBG, "RECEIVED: SESSION_STOP_DONE[%#x]\n",
			pkt->session_id);

	if (!pkt || pkt->size !=
		sizeof(struct hfi_msg_session_stop_done_packet)) {
		dprintk(VIDC_ERR, "%s: bad packet/packet size\n",
			__func__);
		return -E2BIG;
	}

	cmd_done.device_id = device_id;
	cmd_done.session_id = (void *)(uintptr_t)pkt->session_id;
	cmd_done.status = hfi_map_err_status(pkt->error_type);
	cmd_done.size = 0;

	*info = (struct msm_vidc_cb_info) {
		.response_type =  HAL_SESSION_STOP_DONE,
		.response.cmd = cmd_done,
	};

	return 0;
}

static int hfi_process_session_rel_res_done(u32 device_id,
		struct hfi_msg_session_release_resources_done_packet *pkt,
		struct msm_vidc_cb_info *info)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};

	dprintk(VIDC_DBG, "RECEIVED: SESSION_RELEASE_RESOURCES_DONE[%#x]\n",
		pkt->session_id);

	if (!pkt || pkt->size !=
		sizeof(struct hfi_msg_session_release_resources_done_packet)) {
		dprintk(VIDC_ERR, "%s: bad packet/packet size\n",
			__func__);
		return -E2BIG;
	}

	cmd_done.device_id = device_id;
	cmd_done.session_id = (void *)(uintptr_t)pkt->session_id;
	cmd_done.status = hfi_map_err_status(pkt->error_type);
	cmd_done.size = 0;

	*info = (struct msm_vidc_cb_info) {
		.response_type =  HAL_SESSION_RELEASE_RESOURCE_DONE,
		.response.cmd = cmd_done,
	};

	return 0;
}

static int hfi_process_session_rel_buf_done(u32 device_id,
		struct hfi_msg_session_release_buffers_done_packet *pkt,
		struct msm_vidc_cb_info *info)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};

	if (!pkt || pkt->size <
		sizeof(struct hfi_msg_session_release_buffers_done_packet)) {
		dprintk(VIDC_ERR, "bad packet/packet size %d\n",
			pkt ? pkt->size : 0);
		return -E2BIG;
	}
	dprintk(VIDC_DBG, "RECEIVED:SESSION_RELEASE_BUFFER_DONE[%#x]\n",
			pkt->session_id);

	cmd_done.device_id = device_id;
	cmd_done.size = sizeof(struct msm_vidc_cb_cmd_done);
	cmd_done.session_id = (void *)(uintptr_t)pkt->session_id;
	cmd_done.status = hfi_map_err_status(pkt->error_type);
	if (pkt->rg_buffer_info) {
		cmd_done.data.buffer_info =
			*(struct hal_buffer_info *)pkt->rg_buffer_info;
		cmd_done.size = sizeof(struct hal_buffer_info);
	} else {
		dprintk(VIDC_ERR, "invalid payload in rel_buff_done\n");
	}

	*info = (struct msm_vidc_cb_info) {
		.response_type =  HAL_SESSION_RELEASE_BUFFER_DONE,
		.response.cmd = cmd_done,
	};

	return 0;
}

static int hfi_process_session_end_done(u32 device_id,
		struct hfi_msg_sys_session_end_done_packet *pkt,
		struct msm_vidc_cb_info *info)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};

	dprintk(VIDC_DBG, "RECEIVED: SESSION_END_DONE[%#x]\n", pkt->session_id);

	if (!pkt || pkt->size !=
		sizeof(struct hfi_msg_sys_session_end_done_packet)) {
		dprintk(VIDC_ERR, "%s: bad packet/packet size\n", __func__);
		return -E2BIG;
	}

	cmd_done.device_id = device_id;
	cmd_done.session_id = (void *)(uintptr_t)pkt->session_id;
	cmd_done.status = hfi_map_err_status(pkt->error_type);
	cmd_done.size = 0;

	*info = (struct msm_vidc_cb_info) {
		.response_type =  HAL_SESSION_END_DONE,
		.response.cmd = cmd_done,
	};

	return 0;
}

static int hfi_process_session_abort_done(u32 device_id,
	struct hfi_msg_sys_session_abort_done_packet *pkt,
	struct msm_vidc_cb_info *info)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};

	dprintk(VIDC_DBG, "RECEIVED: SESSION_ABORT_DONE[%#x]\n",
			pkt->session_id);

	if (!pkt || pkt->size !=
		sizeof(struct hfi_msg_sys_session_abort_done_packet)) {
		dprintk(VIDC_ERR, "%s: bad packet/packet size: %d\n",
				__func__, pkt ? pkt->size : 0);
		return -E2BIG;
	}
	cmd_done.device_id = device_id;
	cmd_done.session_id = (void *)(uintptr_t)pkt->session_id;
	cmd_done.status = hfi_map_err_status(pkt->error_type);
	cmd_done.size = 0;

	*info = (struct msm_vidc_cb_info) {
		.response_type =  HAL_SESSION_ABORT_DONE,
		.response.cmd = cmd_done,
	};

	return 0;
}

static int hfi_process_session_get_seq_hdr_done(
		u32 device_id,
		struct hfi_msg_session_get_sequence_header_done_packet *pkt,
		struct msm_vidc_cb_info *info)
{
	struct msm_vidc_cb_data_done data_done = {0};

	if (!pkt || pkt->size !=
			sizeof(struct
			hfi_msg_session_get_sequence_header_done_packet)) {
		dprintk(VIDC_ERR, "%s: bad packet/packet size\n",
			__func__);
		return -E2BIG;
	}

	dprintk(VIDC_DBG, "RECEIVED:SESSION_GET_SEQ_HDR_DONE[%#x]\n",
			pkt->session_id);

	data_done.device_id = device_id;
	data_done.size = sizeof(struct msm_vidc_cb_data_done);
	data_done.session_id = (void *)(uintptr_t)pkt->session_id;
	data_done.status = hfi_map_err_status(pkt->error_type);
	data_done.output_done.packet_buffer1 =
		(ion_phys_addr_t)pkt->sequence_header;
	data_done.output_done.filled_len1 = pkt->header_len;
	dprintk(VIDC_INFO, "seq_hdr: %#x, Length: %d\n",
			pkt->sequence_header, pkt->header_len);

	*info = (struct msm_vidc_cb_info) {
		.response_type =  HAL_SESSION_GET_SEQ_HDR_DONE,
		.response.data = data_done,
	};

	return 0;
}

static void hfi_process_sys_get_prop_image_version(
		struct hfi_msg_sys_property_info_packet *pkt)
{
	int i = 0;
	u32 smem_block_size = 0;
	u8 *smem_table_ptr;
	char version[256];
	const u32 version_string_size = 128;
	const u32 smem_image_index_venus = 14 * 128;
	u8 *str_image_version;
	int req_bytes;

	req_bytes = pkt->size - sizeof(*pkt);
	if (req_bytes < version_string_size ||
			!pkt->rg_property_data[1] ||
			pkt->num_properties > 1) {
		dprintk(VIDC_ERR,
				"hfi_process_sys_get_prop_image_version: bad_pkt: %d\n",
				req_bytes);
		return;
	}
	str_image_version = (u8 *)&pkt->rg_property_data[1];
	/*
	 * The version string returned by firmware includes null
	 * characters at the start and in between. Replace the null
	 * characters with space, to print the version info.
	 */
	for (i = 0; i < version_string_size; i++) {
		if (str_image_version[i] != '\0')
			version[i] = str_image_version[i];
		else
			version[i] = ' ';
	}
	version[i] = '\0';
	dprintk(VIDC_DBG, "F/W version: %s\n", version);

	smem_table_ptr = smem_get_entry(SMEM_IMAGE_VERSION_TABLE,
			&smem_block_size, 0, SMEM_ANY_HOST_FLAG);
	if ((smem_image_index_venus + version_string_size) <= smem_block_size &&
			smem_table_ptr)
		memcpy(smem_table_ptr + smem_image_index_venus,
				str_image_version, version_string_size);
}

static int hfi_process_sys_property_info(u32 device_id,
		struct hfi_msg_sys_property_info_packet *pkt,
		struct msm_vidc_cb_info *info)
{
	if (!pkt) {
		dprintk(VIDC_ERR, "%s: invalid param\n", __func__);
		return -EINVAL;
	} else if (pkt->size < sizeof(*pkt)) {
		dprintk(VIDC_ERR,
				"hfi_process_sys_property_info: bad_pkt_size\n");
		return -E2BIG;
	} else if (!pkt->num_properties) {
		dprintk(VIDC_ERR,
				"hfi_process_sys_property_info: no_properties\n");
		return -EINVAL;
	}

	switch (pkt->rg_property_data[0]) {
	case HFI_PROPERTY_SYS_IMAGE_VERSION:
		hfi_process_sys_get_prop_image_version(pkt);

		*info = (struct msm_vidc_cb_info) {
			.response_type =  HAL_RESPONSE_UNUSED,
		};
		return 0;
	default:
		dprintk(VIDC_DBG,
				"hfi_process_sys_property_info: unknown_prop_id: %x\n",
				pkt->rg_property_data[0]);
		return -ENOTSUPP;
	}

}

static int hfi_process_ignore(u32 device_id,
		struct vidc_hal_msg_pkt_hdr *msg_hdr,
		struct msm_vidc_cb_info *info)
{
	*info = (struct msm_vidc_cb_info) {
		.response_type =  HAL_RESPONSE_UNUSED,
	};

	return 0;
}

int hfi_process_msg_packet(u32 device_id, struct vidc_hal_msg_pkt_hdr *msg_hdr,
		struct msm_vidc_cb_info *info)
{
	typedef int (*pkt_func_def)(u32, void *, struct msm_vidc_cb_info *info);
	pkt_func_def pkt_func = NULL;

	if (!info || !msg_hdr || msg_hdr->size < VIDC_IFACEQ_MIN_PKT_SIZE) {
		dprintk(VIDC_ERR, "%s: bad packet/packet size\n",
			__func__);
		return -EINVAL;
	}

	dprintk(VIDC_DBG, "Parse response %#x\n", msg_hdr->packet);
	switch (msg_hdr->packet) {
	case HFI_MSG_EVENT_NOTIFY:
		pkt_func = (pkt_func_def)hfi_process_event_notify;
		break;
	case  HFI_MSG_SYS_INIT_DONE:
		pkt_func = (pkt_func_def)hfi_process_sys_init_done;
		break;
	case HFI_MSG_SYS_SESSION_INIT_DONE:
		pkt_func = (pkt_func_def)hfi_process_session_init_done;
		break;
	case HFI_MSG_SYS_PROPERTY_INFO:
		pkt_func = (pkt_func_def)hfi_process_sys_property_info;
		break;
	case HFI_MSG_SYS_SESSION_END_DONE:
		pkt_func = (pkt_func_def)hfi_process_session_end_done;
		break;
	case HFI_MSG_SESSION_LOAD_RESOURCES_DONE:
		pkt_func = (pkt_func_def)hfi_process_session_load_res_done;
		break;
	case HFI_MSG_SESSION_START_DONE:
		pkt_func = (pkt_func_def)hfi_process_session_start_done;
		break;
	case HFI_MSG_SESSION_STOP_DONE:
		pkt_func = (pkt_func_def)hfi_process_session_stop_done;
		break;
	case HFI_MSG_SESSION_EMPTY_BUFFER_DONE:
		pkt_func = (pkt_func_def)hfi_process_session_etb_done;
		break;
	case HFI_MSG_SESSION_FILL_BUFFER_DONE:
		pkt_func = (pkt_func_def)hfi_process_session_ftb_done;
		break;
	case HFI_MSG_SESSION_FLUSH_DONE:
		pkt_func = (pkt_func_def)hfi_process_session_flush_done;
		break;
	case HFI_MSG_SESSION_PROPERTY_INFO:
		pkt_func = (pkt_func_def)hfi_process_session_prop_info;
		break;
	case HFI_MSG_SESSION_RELEASE_RESOURCES_DONE:
		pkt_func = (pkt_func_def)hfi_process_session_rel_res_done;
		break;
	case HFI_MSG_SYS_RELEASE_RESOURCE:
		pkt_func = (pkt_func_def)hfi_process_sys_rel_resource_done;
		break;
	case HFI_MSG_SESSION_GET_SEQUENCE_HEADER_DONE:
		pkt_func = (pkt_func_def) hfi_process_session_get_seq_hdr_done;
		break;
	case HFI_MSG_SESSION_RELEASE_BUFFERS_DONE:
		pkt_func = (pkt_func_def)hfi_process_session_rel_buf_done;
		break;
	case HFI_MSG_SYS_SESSION_ABORT_DONE:
		pkt_func = (pkt_func_def)hfi_process_session_abort_done;
		break;
	case HFI_MSG_SESSION_SYNC_DONE:
		pkt_func = (pkt_func_def)hfi_process_ignore;
		break;
	default:
		dprintk(VIDC_DBG, "Unable to parse message: %#x\n",
				msg_hdr->packet);
		break;
	}

	return pkt_func ? pkt_func(device_id, msg_hdr, info) : -ENOTSUPP;
}
