// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/bitops.h>
#include <linux/soc/qcom/smem.h>
#include "vidc_hfi_helper.h"
#include "msm_vidc_debug.h"
#include "vidc_hfi.h"

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
	case HFI_ERR_SYS_NOC_ERROR:
		vidc_err = VIDC_ERR_NOC_ERROR;
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

static int get_hal_pixel_depth(u32 hfi_bit_depth, u32 sid)
{
	switch (hfi_bit_depth) {
	case HFI_BITDEPTH_8: return MSM_VIDC_BIT_DEPTH_8;
	case HFI_BITDEPTH_9:
	case HFI_BITDEPTH_10: return MSM_VIDC_BIT_DEPTH_10;
	}
	s_vpr_e(sid, "Unsupported bit depth: %d\n", hfi_bit_depth);
	return MSM_VIDC_BIT_DEPTH_UNSUPPORTED;
}

static inline int validate_pkt_size(u32 rem_size, u32 msg_size)
{
	if (rem_size < msg_size) {
		d_vpr_e("%s: bad_packet_size: %d\n", __func__, rem_size);
		return false;
	}
	return true;
}

static int hfi_process_sess_evt_seq_changed(u32 device_id,
		struct hfi_msg_event_notify_packet *pkt,
		struct msm_vidc_cb_info *info)
{
	struct msm_vidc_cb_event event_notify = {0};
	u32 num_properties_changed;
	struct hfi_frame_size *frame_sz;
	struct hfi_profile_level *profile_level;
	struct hfi_bit_depth *pixel_depth;
	struct hfi_pic_struct *pic_struct;
	struct hfi_dpb_counts *dpb_counts;
	u32 rem_size,entropy_mode = 0;
	u8 *data_ptr;
	int prop_id;
	int luma_bit_depth, chroma_bit_depth;
	struct hfi_colour_space *colour_info;
	u32 sid;

	if (!validate_pkt_size(pkt->size,
			       sizeof(struct hfi_msg_event_notify_packet)))
		return -E2BIG;

	sid = pkt->sid;
	event_notify.device_id = device_id;
	event_notify.inst_id = (void *)(uintptr_t)pkt->sid;
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
		rem_size = pkt->size - sizeof(struct
				hfi_msg_event_notify_packet) + sizeof(u32);
		do {
			if (!validate_pkt_size(rem_size, sizeof(u32)))
				return -E2BIG;
			prop_id = (int) *((u32 *)data_ptr);
			rem_size -= sizeof(u32);
			switch (prop_id) {
			case HFI_PROPERTY_PARAM_FRAME_SIZE:
				if (!validate_pkt_size(rem_size, sizeof(struct
					hfi_frame_size)))
					return -E2BIG;
				data_ptr = data_ptr + sizeof(u32);
				frame_sz =
					(struct hfi_frame_size *) data_ptr;
				event_notify.width = frame_sz->width;
				event_notify.height = frame_sz->height;
				s_vpr_hp(sid, "height: %d width: %d\n",
					frame_sz->height, frame_sz->width);
				data_ptr +=
					sizeof(struct hfi_frame_size);
				rem_size -= sizeof(struct hfi_frame_size);
				break;
			case HFI_PROPERTY_PARAM_PROFILE_LEVEL_CURRENT:
				if (!validate_pkt_size(rem_size, sizeof(struct
					hfi_profile_level)))
					return -E2BIG;
				data_ptr = data_ptr + sizeof(u32);
				profile_level =
					(struct hfi_profile_level *) data_ptr;
				event_notify.profile = profile_level->profile;
				event_notify.level = profile_level->level;
				s_vpr_hp(sid, "profile: %d level: %d\n",
					profile_level->profile,
					profile_level->level);
				data_ptr +=
					sizeof(struct hfi_profile_level);
				rem_size -= sizeof(struct hfi_profile_level);
				break;
			case HFI_PROPERTY_PARAM_VDEC_PIXEL_BITDEPTH:
				if (!validate_pkt_size(rem_size, sizeof(struct
					hfi_bit_depth)))
					return -E2BIG;
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
					GENMASK(15, 0), sid);
				chroma_bit_depth = get_hal_pixel_depth(
					(pixel_depth->bit_depth &
					GENMASK(31, 16)) >> 16, sid);
				if (luma_bit_depth == MSM_VIDC_BIT_DEPTH_10 ||
					chroma_bit_depth ==
						MSM_VIDC_BIT_DEPTH_10)
					event_notify.bit_depth =
						MSM_VIDC_BIT_DEPTH_10;
				else
					event_notify.bit_depth = luma_bit_depth;
				s_vpr_hp(sid,
					"bitdepth(%d), luma_bit_depth(%d), chroma_bit_depth(%d)\n",
					event_notify.bit_depth, luma_bit_depth,
					chroma_bit_depth);
				data_ptr += sizeof(struct hfi_bit_depth);
				rem_size -= sizeof(struct hfi_bit_depth);
				break;
			case HFI_PROPERTY_PARAM_VDEC_PIC_STRUCT:
				if (!validate_pkt_size(rem_size, sizeof(struct
					hfi_pic_struct)))
					return -E2BIG;
				data_ptr = data_ptr + sizeof(u32);
				pic_struct = (struct hfi_pic_struct *) data_ptr;
				event_notify.pic_struct =
					pic_struct->progressive_only;
				s_vpr_hp(sid, "Progressive only flag: %d\n",
						pic_struct->progressive_only);
				data_ptr +=
					sizeof(struct hfi_pic_struct);
				rem_size -= sizeof(struct hfi_pic_struct);
				break;
			case HFI_PROPERTY_PARAM_VDEC_COLOUR_SPACE:
				if (!validate_pkt_size(rem_size, sizeof(struct
					hfi_colour_space)))
					return -E2BIG;
				data_ptr = data_ptr + sizeof(u32);
				colour_info =
					(struct hfi_colour_space *) data_ptr;
				event_notify.colour_space =
					colour_info->colour_space;
				s_vpr_h(sid, "Colour space value is: %d\n",
						colour_info->colour_space);
				data_ptr +=
					sizeof(struct hfi_colour_space);
				rem_size -= sizeof(struct hfi_colour_space);
				break;
			case HFI_PROPERTY_CONFIG_VDEC_ENTROPY:
				if (!validate_pkt_size(rem_size, sizeof(u32)))
					return -E2BIG;
				data_ptr = data_ptr + sizeof(u32);
				entropy_mode = *(u32 *)data_ptr;
				event_notify.entropy_mode = entropy_mode;
				s_vpr_hp(sid, "Entropy Mode: 0x%x\n",
					entropy_mode);
				data_ptr +=
					sizeof(u32);
				rem_size -= sizeof(u32);
				break;
			case HFI_PROPERTY_CONFIG_BUFFER_REQUIREMENTS:
				if (!validate_pkt_size(rem_size, sizeof(struct
					hfi_buffer_requirements)))
					return -E2BIG;
				data_ptr = data_ptr + sizeof(u32);
				data_ptr +=
					sizeof(struct hfi_buffer_requirements);
				rem_size -=
					sizeof(struct hfi_buffer_requirements);
				break;
			case HFI_INDEX_EXTRADATA_INPUT_CROP:
				if (!validate_pkt_size(rem_size, sizeof(struct
				     hfi_index_extradata_input_crop_payload)))
					return -E2BIG;
				data_ptr = data_ptr + sizeof(u32);
				data_ptr +=
					sizeof(struct
					hfi_index_extradata_input_crop_payload);
				rem_size -= sizeof(struct
					hfi_index_extradata_input_crop_payload);
				break;
			case HFI_PROPERTY_PARAM_VDEC_DPB_COUNTS:
				if (!validate_pkt_size(rem_size, sizeof(struct
					hfi_dpb_counts)))
					return -E2BIG;
				data_ptr = data_ptr + sizeof(u32);
				dpb_counts = (struct hfi_dpb_counts *) data_ptr;
				event_notify.max_dpb_count =
					dpb_counts->max_dpb_count;
				event_notify.max_ref_frames =
					dpb_counts->max_ref_frames;
				event_notify.max_dec_buffering =
					dpb_counts->max_dec_buffering;
				event_notify.max_reorder_frames =
					dpb_counts->max_reorder_frames;
				event_notify.fw_min_cnt =
					dpb_counts->fw_min_cnt;
				s_vpr_h(sid,
					"FW DPB counts: dpb %d ref %d buff %d reorder %d fw_min_cnt %d\n",
						dpb_counts->max_dpb_count,
						dpb_counts->max_ref_frames,
						dpb_counts->max_dec_buffering,
						dpb_counts->max_reorder_frames,
						dpb_counts->fw_min_cnt);
				data_ptr +=
					sizeof(struct hfi_dpb_counts);
				rem_size -= sizeof(struct hfi_dpb_counts);
				break;
			default:
				s_vpr_e(sid, "%s: cmd: %#x not supported\n",
					__func__, prop_id);
				break;
			}
			num_properties_changed--;
		} while (num_properties_changed > 0);
	}

	info->response_type = HAL_SESSION_EVENT_CHANGE;
	info->response.event = event_notify;

	return 0;
}

static int hfi_process_evt_release_buffer_ref(u32 device_id,
		struct hfi_msg_event_notify_packet *pkt,
		struct msm_vidc_cb_info *info)
{
	struct msm_vidc_cb_event event_notify = {0};
	struct hfi_msg_release_buffer_ref_event_packet *data;

	if (sizeof(struct hfi_msg_event_notify_packet)
		> pkt->size) {
		d_vpr_e("%s: bad_pkt_size\n", __func__);
		return -E2BIG;
	}
	if (pkt->size < sizeof(struct hfi_msg_event_notify_packet) - sizeof(u32)
		+ sizeof(struct hfi_msg_release_buffer_ref_event_packet)) {
		d_vpr_e("%s: bad_pkt_size: %d\n", __func__, pkt->size);
		return -E2BIG;
	}

	data = (struct hfi_msg_release_buffer_ref_event_packet *)
				pkt->rg_ext_event_data;
	s_vpr_l(pkt->sid,
		"RECEIVED: EVENT_NOTIFY - release_buffer_reference\n");

	event_notify.device_id = device_id;
	event_notify.inst_id = (void *)(uintptr_t)pkt->sid;
	event_notify.status = VIDC_ERR_NONE;
	event_notify.hal_event_type = HAL_EVENT_RELEASE_BUFFER_REFERENCE;
	event_notify.packet_buffer = data->packet_buffer;
	event_notify.extra_data_buffer = data->extra_data_buffer;

	info->response_type = HAL_SESSION_EVENT_CHANGE;
	info->response.event = event_notify;

	return 0;
}

static int hfi_process_sys_error(u32 device_id,
	struct hfi_msg_event_notify_packet *pkt,
	struct msm_vidc_cb_info *info)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};

	cmd_done.device_id = device_id;
	cmd_done.status = hfi_map_err_status(pkt->event_data1);

	info->response_type = HAL_SYS_ERROR;
	info->response.cmd = cmd_done;

	return 0;
}

static int hfi_process_session_error(u32 device_id,
		struct hfi_msg_event_notify_packet *pkt,
		struct msm_vidc_cb_info *info)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};
	u32 sid = pkt->sid;

	cmd_done.device_id = device_id;
	cmd_done.inst_id = (void *)(uintptr_t)pkt->sid;
	cmd_done.status = hfi_map_err_status(pkt->event_data1);
	info->response.cmd = cmd_done;
	s_vpr_h(sid, "RECEIVED: SESSION_ERROR with event id : %#x %#x\n",
		pkt->event_data1, pkt->event_data2);
	switch (pkt->event_data1) {
	/* Ignore below errors */
	case HFI_ERR_SESSION_INVALID_SCALE_FACTOR:
	case HFI_ERR_SESSION_UPSCALE_NOT_SUPPORTED:
		s_vpr_h(sid, "Non Fatal: HFI_EVENT_SESSION_ERROR\n");
		info->response_type = HAL_RESPONSE_UNUSED;
		break;
	default:
		s_vpr_e(sid, "%s: data1 %#x, data2 %#x\n", __func__,
			pkt->event_data1, pkt->event_data2);
		info->response_type = HAL_SESSION_ERROR;
		break;
	}

	return 0;
}

static int hfi_process_event_notify(u32 device_id,
		void *_pkt,
		struct msm_vidc_cb_info *info)
{
	struct hfi_msg_event_notify_packet *pkt = _pkt;

	if (pkt->size < sizeof(struct hfi_msg_event_notify_packet)) {
		d_vpr_e("%s: invalid params %u %u\n", __func__,
			pkt->size, sizeof(struct hfi_msg_event_notify_packet));
		return -E2BIG;
	}

	s_vpr_l(pkt->sid, "RECEIVED: EVENT_NOTIFY\n");

	switch (pkt->event_id) {
	case HFI_EVENT_SYS_ERROR:
		s_vpr_e(pkt->sid, "HFI_EVENT_SYS_ERROR: %d, %#x\n",
			pkt->event_data1, pkt->event_data2);
		return hfi_process_sys_error(device_id, pkt, info);
	case HFI_EVENT_SESSION_ERROR:
		s_vpr_h(pkt->sid, "HFI_EVENT_SESSION_ERROR\n");
		return hfi_process_session_error(device_id, pkt, info);

	case HFI_EVENT_SESSION_SEQUENCE_CHANGED:
		s_vpr_h(pkt->sid, "HFI_EVENT_SESSION_SEQUENCE_CHANGED\n");
		return hfi_process_sess_evt_seq_changed(device_id, pkt, info);

	case HFI_EVENT_RELEASE_BUFFER_REFERENCE:
		s_vpr_l(pkt->sid, "HFI_EVENT_RELEASE_BUFFER_REFERENCE\n");
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
		void *_pkt,
		struct msm_vidc_cb_info *info)
{
	struct hfi_msg_sys_init_done_packet *pkt = _pkt;
	struct msm_vidc_cb_cmd_done cmd_done = {0};
	enum vidc_status status = VIDC_ERR_NONE;

	if (sizeof(struct hfi_msg_sys_init_done_packet) > pkt->size) {
		d_vpr_e("%s: bad_pkt_size: %d\n", __func__,
				pkt->size);
		return -E2BIG;
	}
	d_vpr_h("RECEIVED: SYS_INIT_DONE\n");

	status = hfi_map_err_status(pkt->error_type);
	if (status)
		d_vpr_e("%s: status %#x\n", __func__, status);

	cmd_done.device_id = device_id;
	cmd_done.inst_id = NULL;
	cmd_done.status = (u32)status;
	cmd_done.size = sizeof(struct vidc_hal_sys_init_done);

	info->response_type = HAL_SYS_INIT_DONE;
	info->response.cmd = cmd_done;

	return 0;
}

static int hfi_process_sys_rel_resource_done(u32 device_id,
		void *_pkt,
		struct msm_vidc_cb_info *info)
{
	struct hfi_msg_sys_release_resource_done_packet *pkt = _pkt;
	struct msm_vidc_cb_cmd_done cmd_done = {0};
	enum vidc_status status = VIDC_ERR_NONE;
	u32 pkt_size;

	pkt_size = sizeof(struct hfi_msg_sys_release_resource_done_packet);
	if (pkt_size > pkt->size) {
		d_vpr_e("hal_process_sys_rel_resource_done: bad size: %d\n",
			pkt->size);
		return -E2BIG;
	}
	d_vpr_h("RECEIVED: SYS_RELEASE_RESOURCE_DONE\n");

	status = hfi_map_err_status(pkt->error_type);
	cmd_done.device_id = device_id;
	cmd_done.inst_id = NULL;
	cmd_done.status = (u32) status;
	cmd_done.size = 0;

	info->response_type = HAL_SYS_RELEASE_RESOURCE_DONE;
	info->response.cmd = cmd_done;

	return 0;
}

static void copy_hfi_to_hal_buf_req(struct hal_buffer_requirements *dst,
	struct hfi_buffer_requirements *src) {
	dst->buffer_size = src->buffer_size;
	dst->buffer_count_min = (u16)src->buffer_count_min;
	dst->buffer_count_min_host = (u16)src->buffer_count_min_host;
	dst->buffer_count_actual = (u16)src->buffer_count_actual;
	dst->buffer_alignment = (u16)src->buffer_alignment;
}

static void hfi_process_sess_get_prop_buf_req(
	struct hfi_msg_session_property_info_packet *prop,
	struct buffer_requirements *buffreq, u32 sid)
{
	struct hfi_buffer_requirements *hfi_buf_req;
	struct hal_buffer_requirements *hal_buf_req;
	u32 req_bytes;

	if (!prop) {
		s_vpr_e(sid, "%s: bad_prop: %pK\n", __func__, prop);
		return;
	}

	req_bytes = prop->size - sizeof(
			struct hfi_msg_session_property_info_packet);
	if (!req_bytes || req_bytes % sizeof(struct hfi_buffer_requirements) ||
		!prop->rg_property_data[1]) {
		s_vpr_e(sid, "%s: bad_pkt: %d\n", __func__,	req_bytes);
		return;
	}

	hfi_buf_req = (struct hfi_buffer_requirements *)
		&prop->rg_property_data[1];

	if (!hfi_buf_req) {
		s_vpr_e(sid, "%s: invalid buffer req pointer\n", __func__);
		return;
	}

	while (req_bytes) {
		s_vpr_h(sid, "got buffer requirements for: %d\n",
					hfi_buf_req->buffer_type);
		switch (hfi_buf_req->buffer_type) {
		case HFI_BUFFER_INPUT:
			hal_buf_req = &buffreq->buffer[0];
			hal_buf_req->buffer_type = HAL_BUFFER_INPUT;
			break;
		case HFI_BUFFER_OUTPUT:
			hal_buf_req = &buffreq->buffer[1];
			hal_buf_req->buffer_type = HAL_BUFFER_OUTPUT;
			break;
		case HFI_BUFFER_OUTPUT2:
			hal_buf_req = &buffreq->buffer[2];
			hal_buf_req->buffer_type = HAL_BUFFER_OUTPUT2;
			break;
		case HFI_BUFFER_EXTRADATA_INPUT:
			hal_buf_req = &buffreq->buffer[3];
			hal_buf_req->buffer_type =
				HAL_BUFFER_EXTRADATA_INPUT;
			break;
		case HFI_BUFFER_EXTRADATA_OUTPUT:
			hal_buf_req = &buffreq->buffer[4];
			hal_buf_req->buffer_type =
				HAL_BUFFER_EXTRADATA_OUTPUT;
			break;
		case HFI_BUFFER_EXTRADATA_OUTPUT2:
			hal_buf_req = &buffreq->buffer[5];
			hal_buf_req->buffer_type =
				HAL_BUFFER_EXTRADATA_OUTPUT2;
			break;
		case HFI_BUFFER_COMMON_INTERNAL_SCRATCH:
			hal_buf_req = &buffreq->buffer[6];
			hal_buf_req->buffer_type =
				HAL_BUFFER_INTERNAL_SCRATCH;
			break;
		case HFI_BUFFER_COMMON_INTERNAL_SCRATCH_1:
			hal_buf_req = &buffreq->buffer[7];
			hal_buf_req->buffer_type =
				HAL_BUFFER_INTERNAL_SCRATCH_1;
			break;
		case HFI_BUFFER_COMMON_INTERNAL_SCRATCH_2:
			hal_buf_req = &buffreq->buffer[8];
			hal_buf_req->buffer_type =
				HAL_BUFFER_INTERNAL_SCRATCH_2;
			break;
		case HFI_BUFFER_INTERNAL_PERSIST:
			hal_buf_req = &buffreq->buffer[9];
			hal_buf_req->buffer_type =
				HAL_BUFFER_INTERNAL_PERSIST;
			break;
		case HFI_BUFFER_INTERNAL_PERSIST_1:
			hal_buf_req = &buffreq->buffer[10];
			hal_buf_req->buffer_type =
				HAL_BUFFER_INTERNAL_PERSIST_1;
			break;
		case HFI_BUFFER_COMMON_INTERNAL_RECON:
			hal_buf_req = &buffreq->buffer[11];
			hal_buf_req->buffer_type =
				HAL_BUFFER_INTERNAL_RECON;
			break;
		default:
			hal_buf_req = NULL;
			s_vpr_e(sid, "%s: bad_buffer_type: %d\n",
				__func__, hfi_buf_req->buffer_type);
			break;
		}
		if (hal_buf_req)
			copy_hfi_to_hal_buf_req(hal_buf_req, hfi_buf_req);
		req_bytes -= sizeof(struct hfi_buffer_requirements);
		hfi_buf_req++;
	}
}

static int hfi_process_session_prop_info(u32 device_id,
		void *_pkt,
		struct msm_vidc_cb_info *info)
{
	struct hfi_msg_session_property_info_packet *pkt = _pkt;
	struct msm_vidc_cb_cmd_done cmd_done = {0};
	struct buffer_requirements buff_req = { { {0} } };

	if (pkt->size < sizeof(struct hfi_msg_session_property_info_packet)) {
		d_vpr_e("hal_process_session_prop_info: bad_pkt_size\n");
		return -E2BIG;
	} else if (!pkt->num_properties) {
		d_vpr_e("hal_process_session_prop_info: no_properties\n");
		return -EINVAL;
	}
	s_vpr_h(pkt->sid, "Received SESSION_PROPERTY_INFO\n");

	switch (pkt->rg_property_data[0]) {
	case HFI_PROPERTY_CONFIG_BUFFER_REQUIREMENTS:
		hfi_process_sess_get_prop_buf_req(pkt, &buff_req, pkt->sid);
		cmd_done.device_id = device_id;
		cmd_done.inst_id = (void *)(uintptr_t)pkt->sid;
		cmd_done.status = VIDC_ERR_NONE;
		cmd_done.data.property.buf_req = buff_req;
		cmd_done.size = sizeof(buff_req);

		info->response_type = HAL_SESSION_PROPERTY_INFO;
		info->response.cmd = cmd_done;

		return 0;
	default:
		s_vpr_h(pkt->sid, "%s: unknown_prop_id: %x\n",
				__func__, pkt->rg_property_data[0]);
		return -ENOTSUPP;
	}
}

static int hfi_process_session_init_done(u32 device_id,
		void *_pkt,
		struct msm_vidc_cb_info *info)
{
	struct hfi_msg_sys_session_init_done_packet *pkt = _pkt;
	struct msm_vidc_cb_cmd_done cmd_done = {0};

	if (sizeof(struct hfi_msg_sys_session_init_done_packet) > pkt->size) {
		d_vpr_e("hal_process_session_init_done: bad_pkt_size\n");
		return -E2BIG;
	}
	s_vpr_h(pkt->sid, "RECEIVED: SESSION_INIT_DONE\n");

	cmd_done.device_id = device_id;
	cmd_done.inst_id = (void *)(uintptr_t)pkt->sid;
	cmd_done.status = hfi_map_err_status(pkt->error_type);

	info->response_type = HAL_SESSION_INIT_DONE;
	info->response.cmd = cmd_done;

	return 0;
}

static int hfi_process_session_load_res_done(u32 device_id,
		void *_pkt,
		struct msm_vidc_cb_info *info)
{
	struct hfi_msg_session_load_resources_done_packet *pkt = _pkt;
	struct msm_vidc_cb_cmd_done cmd_done = {0};

	if (sizeof(struct hfi_msg_session_load_resources_done_packet) !=
		pkt->size) {
		d_vpr_e("%s: bad packet size: %d\n", __func__, pkt->size);
		return -E2BIG;
	}
	s_vpr_h(pkt->sid, "RECEIVED: SESSION_LOAD_RESOURCES_DONE\n");

	cmd_done.device_id = device_id;
	cmd_done.inst_id = (void *)(uintptr_t)pkt->sid;
	cmd_done.status = hfi_map_err_status(pkt->error_type);
	cmd_done.size = 0;

	info->response_type = HAL_SESSION_LOAD_RESOURCE_DONE;
	info->response.cmd = cmd_done;

	return 0;
}

static int hfi_process_session_flush_done(u32 device_id,
		void *_pkt,
		struct msm_vidc_cb_info *info)
{
	struct hfi_msg_session_flush_done_packet *pkt = _pkt;
	struct msm_vidc_cb_cmd_done cmd_done = {0};

	if (sizeof(struct hfi_msg_session_flush_done_packet) != pkt->size) {
		d_vpr_e("hal_process_session_flush_done: bad packet size: %d\n",
				pkt->size);
		return -E2BIG;
	}
	s_vpr_h(pkt->sid, "RECEIVED: SESSION_FLUSH_DONE\n");

	cmd_done.device_id = device_id;
	cmd_done.inst_id = (void *)(uintptr_t)pkt->sid;
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
		s_vpr_e(pkt->sid, "%s: invalid flush type!", __func__);
		return -EINVAL;
	}

	info->response_type = HAL_SESSION_FLUSH_DONE;
	info->response.cmd = cmd_done;

	return 0;
}

static int hfi_process_session_etb_done(u32 device_id,
		void *_pkt,
		struct msm_vidc_cb_info *info)
{
	struct hfi_msg_session_empty_buffer_done_packet *pkt = _pkt;
	struct msm_vidc_cb_data_done data_done = {0};

	if (!pkt || pkt->size <
		sizeof(struct hfi_msg_session_empty_buffer_done_packet))
		goto bad_packet_size;

	s_vpr_l(pkt->sid, "RECEIVED: SESSION_ETB_DONE\n");

	data_done.device_id = device_id;
	data_done.inst_id = (void *)(uintptr_t)pkt->sid;
	data_done.status = hfi_map_err_status(pkt->error_type);
	data_done.size = sizeof(struct msm_vidc_cb_data_done);
	data_done.input_done.input_tag = pkt->input_tag;
	data_done.input_done.recon_stats.buffer_index =
		pkt->ubwc_cr_stats.frame_index;
	memcpy(&data_done.input_done.recon_stats.ubwc_stats_info,
		&pkt->ubwc_cr_stats.ubwc_stats_info,
		sizeof(data_done.input_done.recon_stats.ubwc_stats_info));
	data_done.input_done.recon_stats.complexity_number =
		pkt->ubwc_cr_stats.complexity_number;
	data_done.input_done.offset = pkt->offset;
	data_done.input_done.filled_len = pkt->filled_len;
	data_done.input_done.flags = pkt->flags;
	data_done.input_done.packet_buffer = pkt->packet_buffer;
	data_done.input_done.extra_data_buffer = pkt->extra_data_buffer;
	data_done.input_done.status =
		hfi_map_err_status(pkt->error_type);

	trace_msm_v4l2_vidc_buffer_event_end("ETB",
		(u32)pkt->packet_buffer, -1, -1,
		pkt->filled_len, pkt->offset);

	info->response_type = HAL_SESSION_ETB_DONE;
	info->response.data = data_done;

	return 0;
bad_packet_size:
	d_vpr_e("%s: ebd - bad_pkt_size: %d\n",
		__func__, pkt ? pkt->size : 0);
	return -E2BIG;
}

static int hfi_process_session_ftb_done(
		u32 device_id, void *_pkt,
		struct msm_vidc_cb_info *info)
{
	struct vidc_hal_msg_pkt_hdr *msg_hdr = _pkt;
	struct msm_vidc_cb_data_done data_done = {0};
	u32 struct_size = 0;

	bool is_decoder = false, is_encoder = false;

	if (!msg_hdr) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	struct_size = sizeof(struct
		hfi_msg_session_fill_buffer_done_compressed_packet) + 4;
	is_encoder = (msg_hdr->size == struct_size) ||
		(msg_hdr->size == (struct_size +
				   sizeof(struct hfi_ubwc_cr_stats) - 4));

	struct_size = sizeof(struct
		hfi_msg_session_fbd_uncompressed_plane0_packet) + 4;
	is_decoder = (msg_hdr->size == struct_size) ||
		(msg_hdr->size == (struct_size +
				   sizeof(struct hfi_ubwc_cr_stats) - 4));

	if (!(is_encoder ^ is_decoder)) {
		d_vpr_e("Ambiguous packet (%#x) received (size %d)\n",
				msg_hdr->packet, msg_hdr->size);
		return -EBADHANDLE;
	}

	if (is_encoder) {
		struct hfi_msg_session_fill_buffer_done_compressed_packet *pkt =
		(struct hfi_msg_session_fill_buffer_done_compressed_packet *)
		msg_hdr;
		if (sizeof(struct
			hfi_msg_session_fill_buffer_done_compressed_packet)
			> pkt->size) {
			d_vpr_e("hal_process_session_ftb_done: bad_pkt_size\n");
			return -E2BIG;
		} else if (pkt->error_type != HFI_ERR_NONE) {
			s_vpr_e(pkt->sid, "got buffer back with error %x\n",
				pkt->error_type);
			/* Proceed with the FBD */
		}
		s_vpr_l(pkt->sid, "RECEIVED: SESSION_FTB_DONE\n");

		data_done.device_id = device_id;
		data_done.inst_id = (void *)(uintptr_t)pkt->sid;
		data_done.status = hfi_map_err_status(pkt->error_type);
		data_done.size = sizeof(struct msm_vidc_cb_data_done);

		data_done.output_done.input_tag = pkt->input_tag;
		data_done.output_done.timestamp_hi = pkt->time_stamp_hi;
		data_done.output_done.timestamp_lo = pkt->time_stamp_lo;
		data_done.output_done.flags1 = pkt->flags;
		data_done.output_done.stats = pkt->stats;
		data_done.output_done.offset1 = pkt->offset;
		data_done.output_done.alloc_len1 = pkt->alloc_len;
		data_done.output_done.filled_len1 = pkt->filled_len;
		data_done.output_done.picture_type = pkt->picture_type;
		data_done.output_done.packet_buffer1 = pkt->packet_buffer;
		data_done.output_done.extra_data_buffer =
			pkt->extra_data_buffer;
		data_done.output_done.buffer_type = HAL_BUFFER_OUTPUT;
		/* FBD packet is extended only when stats=1. */
		if (pkt->stats == 1) {
			struct hfi_ubwc_cr_stats *ubwc_stat =
				(struct hfi_ubwc_cr_stats *)pkt->rgData;
			data_done.output_done.ubwc_cr_stat.is_valid =
				ubwc_stat->is_valid;
			data_done.output_done.ubwc_cr_stat.worst_cr =
				ubwc_stat->worst_compression_ratio;
			data_done.output_done.ubwc_cr_stat.worst_cf =
				ubwc_stat->worst_complexity_number;
		}
	} else /* if (is_decoder) */ {
		struct hfi_msg_session_fbd_uncompressed_plane0_packet *pkt =
		(struct	hfi_msg_session_fbd_uncompressed_plane0_packet *)
		msg_hdr;
		if (sizeof(
			struct hfi_msg_session_fbd_uncompressed_plane0_packet) >
			pkt->size) {
			d_vpr_e("hal_process_session_ftb_done: bad_pkt_size\n");
			return -E2BIG;
		}
		s_vpr_l(pkt->sid, "RECEIVED: SESSION_FTB_DONE\n");

		data_done.device_id = device_id;
		data_done.inst_id = (void *)(uintptr_t)pkt->sid;
		data_done.status = hfi_map_err_status(pkt->error_type);
		data_done.size = sizeof(struct msm_vidc_cb_data_done);

		data_done.output_done.stream_id = pkt->stream_id;
		data_done.output_done.view_id = pkt->view_id;
		data_done.output_done.timestamp_hi = pkt->time_stamp_hi;
		data_done.output_done.timestamp_lo = pkt->time_stamp_lo;
		data_done.output_done.flags1 = pkt->flags;
		data_done.output_done.stats = pkt->stats;
		data_done.output_done.alloc_len1 = pkt->alloc_len;
		data_done.output_done.filled_len1 = pkt->filled_len;
		data_done.output_done.offset1 = pkt->offset;
		data_done.output_done.frame_width = pkt->frame_width;
		data_done.output_done.frame_height = pkt->frame_height;
		data_done.output_done.start_x_coord = pkt->start_x_coord;
		data_done.output_done.start_y_coord = pkt->start_y_coord;
		data_done.output_done.input_tag = pkt->input_tag;
		data_done.output_done.input_tag2 = pkt->input_tag2;
		data_done.output_done.picture_type = pkt->picture_type;
		data_done.output_done.packet_buffer1 = pkt->packet_buffer;
		data_done.output_done.extra_data_buffer =
			pkt->extra_data_buffer;

		/* FBD packet is extended only when view_id=1. */
		if (pkt->view_id == 1) {
			struct hfi_ubwc_cr_stats *ubwc_stat =
				(struct hfi_ubwc_cr_stats *)pkt->rgData;
			data_done.output_done.ubwc_cr_stat.is_valid =
				ubwc_stat->is_valid;
			data_done.output_done.ubwc_cr_stat.worst_cr =
				ubwc_stat->worst_compression_ratio;
			data_done.output_done.ubwc_cr_stat.worst_cf =
				ubwc_stat->worst_complexity_number;
		}

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

	info->response_type = HAL_SESSION_FTB_DONE;
	info->response.data = data_done;

	return 0;
}

static int hfi_process_session_start_done(u32 device_id,
		void *_pkt,
		struct msm_vidc_cb_info *info)
{
	struct hfi_msg_session_start_done_packet *pkt = _pkt;
	struct msm_vidc_cb_cmd_done cmd_done = {0};

	if (!pkt || pkt->size !=
		sizeof(struct hfi_msg_session_start_done_packet)) {
		d_vpr_e("%s: bad packet/packet size\n", __func__);
		return -E2BIG;
	}
	s_vpr_h(pkt->sid, "RECEIVED: SESSION_START_DONE\n");

	cmd_done.device_id = device_id;
	cmd_done.inst_id = (void *)(uintptr_t)pkt->sid;
	cmd_done.status = hfi_map_err_status(pkt->error_type);
	cmd_done.size = 0;

	info->response_type = HAL_SESSION_START_DONE;
	info->response.cmd = cmd_done;
	return 0;
}

static int hfi_process_session_stop_done(u32 device_id,
		void *_pkt,
		struct msm_vidc_cb_info *info)
{
	struct hfi_msg_session_stop_done_packet *pkt = _pkt;
	struct msm_vidc_cb_cmd_done cmd_done = {0};

	if (!pkt || pkt->size !=
		sizeof(struct hfi_msg_session_stop_done_packet)) {
		d_vpr_e("%s: bad packet/packet size\n", __func__);
		return -E2BIG;
	}
	s_vpr_h(pkt->sid, "RECEIVED: SESSION_STOP_DONE\n");

	cmd_done.device_id = device_id;
	cmd_done.inst_id = (void *)(uintptr_t)pkt->sid;
	cmd_done.status = hfi_map_err_status(pkt->error_type);
	cmd_done.size = 0;

	info->response_type = HAL_SESSION_STOP_DONE;
	info->response.cmd = cmd_done;

	return 0;
}

static int hfi_process_session_rel_res_done(u32 device_id,
		void *_pkt,
		struct msm_vidc_cb_info *info)
{
	struct hfi_msg_session_release_resources_done_packet *pkt = _pkt;
	struct msm_vidc_cb_cmd_done cmd_done = {0};

	if (!pkt || pkt->size !=
		sizeof(struct hfi_msg_session_release_resources_done_packet)) {
		d_vpr_e("%s: bad packet/packet size\n", __func__);
		return -E2BIG;
	}
	s_vpr_h(pkt->sid, "RECEIVED: SESSION_RELEASE_RESOURCES_DONE\n");

	cmd_done.device_id = device_id;
	cmd_done.inst_id = (void *)(uintptr_t)pkt->sid;
	cmd_done.status = hfi_map_err_status(pkt->error_type);
	cmd_done.size = 0;

	info->response_type = HAL_SESSION_RELEASE_RESOURCE_DONE;
	info->response.cmd = cmd_done;

	return 0;
}

static int hfi_process_session_rel_buf_done(u32 device_id,
		void *_pkt,
		struct msm_vidc_cb_info *info)
{
	struct hfi_msg_session_release_buffers_done_packet *pkt = _pkt;
	struct msm_vidc_cb_cmd_done cmd_done = {0};

	if (!pkt || pkt->size <
		sizeof(struct hfi_msg_session_release_buffers_done_packet)) {
		d_vpr_e("bad packet/packet size %d\n",
			pkt ? pkt->size : 0);
		return -E2BIG;
	}
	s_vpr_h(pkt->sid, "RECEIVED:SESSION_RELEASE_BUFFER_DONE\n");

	cmd_done.device_id = device_id;
	cmd_done.size = sizeof(struct msm_vidc_cb_cmd_done);
	cmd_done.inst_id = (void *)(uintptr_t)pkt->sid;
	cmd_done.status = hfi_map_err_status(pkt->error_type);
	cmd_done.data.buffer_info.buffer_addr = *pkt->rg_buffer_info;
	cmd_done.size = sizeof(struct hal_buffer_info);

	info->response_type = HAL_SESSION_RELEASE_BUFFER_DONE;
	info->response.cmd = cmd_done;

	return 0;
}

static int hfi_process_session_end_done(u32 device_id,
		void *_pkt,
		struct msm_vidc_cb_info *info)
{
	struct hfi_msg_sys_session_end_done_packet *pkt = _pkt;
	struct msm_vidc_cb_cmd_done cmd_done = {0};

	if (!pkt || pkt->size !=
		sizeof(struct hfi_msg_sys_session_end_done_packet)) {
		d_vpr_e("%s: bad packet/packet size\n", __func__);
		return -E2BIG;
	}
	s_vpr_h(pkt->sid, "RECEIVED: SESSION_END_DONE\n");

	cmd_done.device_id = device_id;
	cmd_done.inst_id = (void *)(uintptr_t)pkt->sid;
	cmd_done.status = hfi_map_err_status(pkt->error_type);
	cmd_done.size = 0;

	info->response_type = HAL_SESSION_END_DONE;
	info->response.cmd = cmd_done;

	return 0;
}

static int hfi_process_session_abort_done(u32 device_id,
	void *_pkt,
	struct msm_vidc_cb_info *info)
{
	struct hfi_msg_sys_session_abort_done_packet *pkt = _pkt;
	struct msm_vidc_cb_cmd_done cmd_done = {0};

	if (!pkt || pkt->size !=
		sizeof(struct hfi_msg_sys_session_abort_done_packet)) {
		d_vpr_e("%s: bad packet/packet size: %d\n",
				__func__, pkt ? pkt->size : 0);
		return -E2BIG;
	}
	s_vpr_h(pkt->sid, "RECEIVED: SESSION_ABORT_DONE\n");
	cmd_done.device_id = device_id;
	cmd_done.inst_id = (void *)(uintptr_t)pkt->sid;
	cmd_done.status = hfi_map_err_status(pkt->error_type);
	cmd_done.size = 0;

	info->response_type = HAL_SESSION_ABORT_DONE;
	info->response.cmd = cmd_done;

	return 0;
}

static void hfi_process_sys_get_prop_image_version(
		struct hfi_msg_sys_property_info_packet *pkt)
{
	u32 i = 0;
	size_t smem_block_size = 0;
	u8 *smem_table_ptr;
	char version[256];
	const u32 version_string_size = 128;
	const u32 smem_image_index_venus = 14 * 128;
	u8 *str_image_version;
	u32 req_bytes;

	req_bytes = pkt->size - sizeof(*pkt);
	if (req_bytes < version_string_size ||
			!pkt->rg_property_data[1] ||
			pkt->num_properties > 1) {
		d_vpr_e("%s: bad_pkt: %d\n", __func__, req_bytes);
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
	d_vpr_h("F/W version: %s\n", version);

	smem_table_ptr = qcom_smem_get(QCOM_SMEM_HOST_ANY,
			SMEM_IMAGE_VERSION_TABLE, &smem_block_size);
	if ((smem_image_index_venus + version_string_size) <= smem_block_size &&
			smem_table_ptr)
		memcpy(smem_table_ptr + smem_image_index_venus,
				str_image_version, version_string_size);
}

static int hfi_process_sys_property_info(u32 device_id,
		void *_pkt,
		struct msm_vidc_cb_info *info)
{
	struct hfi_msg_sys_property_info_packet *pkt = _pkt;
	if (!pkt) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	} else if (pkt->size < sizeof(*pkt)) {
		d_vpr_e("%s: bad_pkt_size\n", __func__);
		return -E2BIG;
	} else if (!pkt->num_properties) {
		d_vpr_e("%s: no_properties\n", __func__);
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
		d_vpr_h("%s: unknown_prop_id: %x\n",
				__func__, pkt->rg_property_data[0]);
		return -ENOTSUPP;
	}

}

int hfi_process_msg_packet(u32 device_id, struct vidc_hal_msg_pkt_hdr *msg_hdr,
		struct msm_vidc_cb_info *info)
{
	typedef int (*pkt_func_def)(u32, void *, struct msm_vidc_cb_info *info);
	pkt_func_def pkt_func = NULL;

	if (!info || !msg_hdr || msg_hdr->size < VIDC_IFACEQ_MIN_PKT_SIZE) {
		d_vpr_e("%s: bad packet/packet size\n", __func__);
		return -EINVAL;
	}

	switch (msg_hdr->packet) {
	case HFI_MSG_EVENT_NOTIFY:
		pkt_func = (pkt_func_def)hfi_process_event_notify;
		break;
	case HFI_MSG_SYS_INIT_DONE:
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
	case HFI_MSG_SESSION_RELEASE_BUFFERS_DONE:
		pkt_func = (pkt_func_def)hfi_process_session_rel_buf_done;
		break;
	case HFI_MSG_SYS_SESSION_ABORT_DONE:
		pkt_func = (pkt_func_def)hfi_process_session_abort_done;
		break;
	default:
		d_vpr_l("Unable to parse message: %#x\n", msg_hdr->packet);
		break;
	}

	return pkt_func ?
		pkt_func(device_id, (void *)msg_hdr, info) : -ENOTSUPP;
}
