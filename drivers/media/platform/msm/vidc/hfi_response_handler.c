/* Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
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

#include <linux/slab.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/hash.h>
#include <soc/qcom/smem.h>
#include "vidc_hfi_helper.h"
#include "vidc_hfi_io.h"
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

struct hal_session *hfi_process_get_session(
		struct list_head *sessions, u32 session_id)
{
	struct hal_session *session = NULL;
	bool found_session = false;

	list_for_each_entry(session, sessions, list) {
		if (hash32_ptr(session) == session_id) {
			found_session = true;
			break;
		}
	}
	return found_session ? session : NULL;
}

static void hfi_process_sess_evt_seq_changed(
		msm_vidc_callback callback, u32 device_id,
		struct hal_session *session,
		struct hfi_msg_event_notify_packet *pkt)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};
	struct msm_vidc_cb_event event_notify = {0};
	int num_properties_changed;
	struct hfi_frame_size *frame_sz;
	struct hfi_profile_level *profile_level;
	u8 *data_ptr;
	int prop_id;

	if (sizeof(struct hfi_msg_event_notify_packet)
		> pkt->size) {
		dprintk(VIDC_ERR,
				"hal_process_session_init_done: bad_pkt_size\n");
		return;
	}

	cmd_done.device_id = device_id;
	cmd_done.session_id = session->session_id;
	cmd_done.status = VIDC_ERR_NONE;
	cmd_done.size = sizeof(struct msm_vidc_cb_event);
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
			default:
				dprintk(VIDC_ERR,
						"%s cmd: 0x%x not supported\n",
							__func__, prop_id);
				break;
			}
			num_properties_changed--;
		} while (num_properties_changed > 0);
	}
	cmd_done.data = &event_notify;
	callback(VIDC_EVENT_CHANGE, &cmd_done);
}

static void hfi_process_evt_release_buffer_ref(
		msm_vidc_callback callback, u32 device_id,
		struct hal_session *session,
		struct hfi_msg_event_notify_packet *pkt)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};
	struct msm_vidc_cb_event event_notify = {0};

	struct hfi_msg_release_buffer_ref_event_packet *data;

	dprintk(VIDC_DBG,
			"RECEIVED: EVENT_NOTIFY - release_buffer_reference\n");
	if (sizeof(struct hfi_msg_event_notify_packet)
		> pkt->size) {
		dprintk(VIDC_ERR,
				"hal_process_session_init_done: bad_pkt_size\n");
		return;
	}

	data = (struct hfi_msg_release_buffer_ref_event_packet *)
				pkt->rg_ext_event_data;

	cmd_done.device_id = device_id;
	cmd_done.session_id = session->session_id;
	cmd_done.status = VIDC_ERR_NONE;
	cmd_done.size = sizeof(struct msm_vidc_cb_event);

	event_notify.hal_event_type = HAL_EVENT_RELEASE_BUFFER_REFERENCE;
	event_notify.packet_buffer = data->packet_buffer;
	event_notify.extra_data_buffer = data->extra_data_buffer;
	cmd_done.data = &event_notify;
	callback(VIDC_EVENT_CHANGE, &cmd_done);
}

static void hfi_process_sys_error(
		msm_vidc_callback callback, u32 device_id)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};
	cmd_done.device_id = device_id;
	callback(SYS_ERROR, &cmd_done);
}
static void hfi_process_session_error(
		msm_vidc_callback callback, u32 device_id,
		struct hal_session *session,
		struct hfi_msg_event_notify_packet *pkt)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};
	cmd_done.device_id = device_id;
	cmd_done.session_id = session->session_id;
	cmd_done.status = hfi_map_err_status(pkt->event_data1);
	dprintk(VIDC_INFO, "Received : SESSION_ERROR with event id : %d\n",
		pkt->event_data1);
	switch (pkt->event_data1) {
	case HFI_ERR_SESSION_INVALID_SCALE_FACTOR:
	case HFI_ERR_SESSION_UNSUPPORT_BUFFERTYPE:
	case HFI_ERR_SESSION_UNSUPPORTED_SETTING:
	case HFI_ERR_SESSION_UPSCALE_NOT_SUPPORTED:
		cmd_done.status = VIDC_ERR_NONE;
		dprintk(VIDC_INFO, "Non Fatal : HFI_EVENT_SESSION_ERROR\n");
		break;
	default:
		dprintk(VIDC_ERR, "HFI_EVENT_SESSION_ERROR\n");
		callback(SESSION_ERROR, &cmd_done);
		break;
	}
}
static void hfi_process_event_notify(msm_vidc_callback callback, u32 device_id,
		struct hal_session *session,
		struct hfi_msg_event_notify_packet *pkt)
{
	dprintk(VIDC_DBG, "Received: EVENT_NOTIFY\n");

	if (!callback || !pkt ||
		pkt->size < sizeof(struct hfi_msg_event_notify_packet)) {
		dprintk(VIDC_ERR, "Invalid Params\n");
		return;
	}

	switch (pkt->event_id) {
	case HFI_EVENT_SYS_ERROR:
		dprintk(VIDC_ERR, "HFI_EVENT_SYS_ERROR: %d, 0x%x\n",
			pkt->event_data1, pkt->event_data2);
		hfi_process_sys_error(callback, device_id);
		break;
	case HFI_EVENT_SESSION_PROPERTY_CHANGED:
		dprintk(VIDC_INFO, "HFI_EVENT_SESSION_PROPERTY_CHANGED[%pK]\n",
			session);
		break;
	}

	if (!session) {
		dprintk(VIDC_ERR, "%s Got invalid session id\n", __func__);
		return;
	}

	switch (pkt->event_id) {
	case HFI_EVENT_SESSION_ERROR:
		dprintk(VIDC_INFO, "HFI_EVENT_SESSION_ERROR[%pK]\n", session);
		hfi_process_session_error(callback, device_id, session, pkt);
		break;
	case HFI_EVENT_SESSION_SEQUENCE_CHANGED:
		dprintk(VIDC_INFO, "HFI_EVENT_SESSION_SEQUENCE_CHANGED[%pK]\n",
			session);
		hfi_process_sess_evt_seq_changed(callback, device_id,
				session, pkt);
		break;
	case HFI_EVENT_RELEASE_BUFFER_REFERENCE:
		dprintk(VIDC_INFO, "HFI_EVENT_RELEASE_BUFFER_REFERENCE[%pK]\n",
			session);
		hfi_process_evt_release_buffer_ref(callback, device_id,
				session, pkt);
		break;
	default:
		dprintk(VIDC_WARN,
				"hal_process_event_notify: unknown_event_id[%pK]\n",
				session);
		break;
	}
}
static void hfi_process_sys_init_done(
		msm_vidc_callback callback, u32 device_id,
		struct hfi_msg_sys_init_done_packet *pkt)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};
	struct vidc_hal_sys_init_done sys_init_done = {0};
	u32 rem_bytes, bytes_read = 0, num_properties;
	u8 *data_ptr;
	int prop_id;
	enum vidc_status status = VIDC_ERR_NONE;

	dprintk(VIDC_DBG, "RECEIVED: SYS_INIT_DONE\n");
	if (sizeof(struct hfi_msg_sys_init_done_packet) > pkt->size) {
		dprintk(VIDC_ERR,
				"hal_process_sys_init_done: bad_pkt_size: %d\n",
				pkt->size);
		return;
	}

	status = hfi_map_err_status(pkt->error_type);

	if (!status) {
		if (pkt->num_properties == 0) {
			dprintk(VIDC_ERR,
					"hal_process_sys_init_done: no_properties\n");
			status = VIDC_ERR_FAIL;
			goto err_no_prop;
		}

		rem_bytes = pkt->size - sizeof(struct
			hfi_msg_sys_init_done_packet) + sizeof(u32);

		if (rem_bytes == 0) {
			dprintk(VIDC_ERR,
					"hal_process_sys_init_done: missing_prop_info\n");
			status = VIDC_ERR_FAIL;
			goto err_no_prop;
		}

		data_ptr = (u8 *) &pkt->rg_property_data[0];
		num_properties = pkt->num_properties;

		while ((num_properties != 0) && (rem_bytes >= sizeof(u32))) {
			prop_id = *((u32 *)data_ptr);
			data_ptr = data_ptr + 4;

			switch (prop_id) {
			case HFI_PROPERTY_PARAM_CODEC_SUPPORTED:
			{
				struct hfi_codec_supported *prop =
					(struct hfi_codec_supported *) data_ptr;
				if (rem_bytes < sizeof(struct
						hfi_codec_supported)) {
					status = VIDC_ERR_BAD_PARAM;
					break;
				}
				sys_init_done.dec_codec_supported =
					prop->decoder_codec_supported;
				sys_init_done.enc_codec_supported =
					prop->encoder_codec_supported;
				break;
			}
			default:
				dprintk(VIDC_ERR,
						"hal_process_sys_init_done: bad_prop_id\n");
				status = VIDC_ERR_BAD_PARAM;
				break;
			}
			if (!status) {
				rem_bytes -= bytes_read;
				data_ptr += bytes_read;
				num_properties--;
			}
		}
	}
err_no_prop:
	cmd_done.device_id = device_id;
	cmd_done.session_id = NULL;
	cmd_done.status = (u32) status;
	cmd_done.size = sizeof(struct vidc_hal_sys_init_done);
	cmd_done.data = (void *) &sys_init_done;
	callback(SYS_INIT_DONE, &cmd_done);
}

static void hfi_process_sys_rel_resource_done(
		msm_vidc_callback callback, u32 device_id,
		struct hfi_msg_sys_release_resource_done_packet *pkt)
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
		return;
	}
	status = hfi_map_err_status(pkt->error_type);
	cmd_done.device_id = device_id;
	cmd_done.session_id = NULL;
	cmd_done.status = (u32) status;
	cmd_done.size = 0;
	cmd_done.data = NULL;
	callback(RELEASE_RESOURCE_DONE, &cmd_done);
}

static inline void copy_cap_prop(
		struct hfi_capability_supported *in,
		struct vidc_hal_session_init_done *sess_init_done)
{
	struct hal_capability_supported *out = NULL;
	if (!in || !sess_init_done) {
		dprintk(VIDC_ERR, "%s Invalid input parameter\n",
			__func__);
		return;
	}
	switch (in->capability_type) {
	case HFI_CAPABILITY_FRAME_WIDTH:
		out = &sess_init_done->width;
		break;

	case HFI_CAPABILITY_FRAME_HEIGHT:
		out = &sess_init_done->height;
		break;

	case HFI_CAPABILITY_MBS_PER_FRAME:
		out = &sess_init_done->mbs_per_frame;
		break;

	case HFI_CAPABILITY_MBS_PER_SECOND:
		out = &sess_init_done->mbs_per_sec;
		break;

	case HFI_CAPABILITY_FRAMERATE:
		out = &sess_init_done->frame_rate;
		break;

	case HFI_CAPABILITY_SCALE_X:
		out = &sess_init_done->scale_x;
		break;

	case HFI_CAPABILITY_SCALE_Y:
		out = &sess_init_done->scale_y;
		break;

	case HFI_CAPABILITY_BITRATE:
		out = &sess_init_done->bitrate;
		break;

	case HFI_CAPABILITY_HIER_P_NUM_ENH_LAYERS:
		out = &sess_init_done->hier_p;
		break;

	case HFI_CAPABILITY_ENC_LTR_COUNT:
		out = &sess_init_done->ltr_count;
		break;

	case HFI_CAPABILITY_CP_OUTPUT2_THRESH:
		out = &sess_init_done->secure_output2_threshold;
		break;
	}

	if (out) {
		out->min = in->min;
		out->max = in->max;
		out->step_size = in->step_size;
	}
}

enum vidc_status hfi_process_sess_init_done_prop_read(
	struct hfi_msg_sys_session_init_done_packet *pkt,
	struct vidc_hal_session_init_done *sess_init_done)
{
	u32 rem_bytes, num_properties;
	u8 *data_ptr;
	enum vidc_status status = VIDC_ERR_NONE;
	u32 prop_id, next_offset = 0;
	u32 prop_count = 0;

	rem_bytes = pkt->size - sizeof(struct
			hfi_msg_sys_session_init_done_packet) + sizeof(u32);

	if (rem_bytes == 0) {
		dprintk(VIDC_ERR,
			"hfi_msg_sys_session_init_done: missing_prop_info\n");
		return VIDC_ERR_FAIL;
	}

	status = hfi_map_err_status(pkt->error_type);
	if (status)
		return status;

	data_ptr = (u8 *) &pkt->rg_property_data[0];
	num_properties = pkt->num_properties;

	while ((status == VIDC_ERR_NONE) && num_properties &&
		   (rem_bytes >= sizeof(u32))) {
		prop_id = *((u32 *)data_ptr);
		next_offset = sizeof(u32);

		switch (prop_id) {
		case HFI_PROPERTY_PARAM_CAPABILITY_SUPPORTED:
		{
			struct hfi_capability_supported_info *prop =
				(struct hfi_capability_supported_info *)
				(data_ptr + next_offset);
			u32 num_capabilities;
			struct hfi_capability_supported *cap_ptr;

			if ((rem_bytes - next_offset) < sizeof(*cap_ptr)) {
				status = VIDC_ERR_BAD_PARAM;
				break;
			}

			num_capabilities = prop->num_capabilities;
			cap_ptr = &prop->rg_data[0];
			next_offset += sizeof(u32);

			while (num_capabilities &&
				((rem_bytes - next_offset) >= sizeof(u32))) {
				copy_cap_prop(cap_ptr, sess_init_done);
				cap_ptr++;
				next_offset += sizeof(*cap_ptr);
				num_capabilities--;
			}
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
			next_offset = sizeof(*prop) - sizeof(u32);
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

				fmt_ptr +=  bytes_to_skip;
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
			char *ptr = NULL;
			int count = 0;
			struct hfi_profile_level *prop_level;
			struct hfi_profile_level_supported *prop =
				(struct hfi_profile_level_supported *)
				(data_ptr + next_offset);
			ptr = (char *) &prop->rg_profile_level[0];
			dprintk(VIDC_DBG, "prop->profile_count: %d\n",
				prop->profile_count);
			prop_count = prop->profile_count;
			if (prop_count > MAX_PROFILE_COUNT) {
				prop_count = MAX_PROFILE_COUNT;
				dprintk(VIDC_WARN,
					"prop count exceeds max profile count\n");
			}
			while (prop_count) {
				ptr++;
				prop_level = (struct hfi_profile_level *) ptr;
				sess_init_done->
				profile_level.profile_level[count].profile
					= prop_level->profile;
				sess_init_done->
				profile_level.profile_level[count].level
					= prop_level->level;
				prop_count--;
				count++;
				ptr +=
				sizeof(struct hfi_profile_level) / sizeof(u32);
			}
			next_offset += sizeof(*prop) -
				sizeof(struct hfi_profile_level) +
				prop->profile_count *
				sizeof(struct hfi_profile_level);
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
			int i;
			if (prop->buffer_type == HFI_BUFFER_OUTPUT ||
				prop->buffer_type == HFI_BUFFER_OUTPUT2) {
				sess_init_done->alloc_mode_out = 0;
				for (i = 0; i < prop->num_entries; i++) {
					switch (prop->rg_data[i]) {
					case HFI_BUFFER_MODE_STATIC:
						sess_init_done->alloc_mode_out
						|= HAL_BUFFER_MODE_STATIC;
						break;
					case HFI_BUFFER_MODE_DYNAMIC:
						sess_init_done->alloc_mode_out
						|= HAL_BUFFER_MODE_DYNAMIC;
						break;
					}
					if (i >= 32) {
						dprintk(VIDC_ERR,
						"%s - num_entries: %d from f/w seems suspect\n",
						__func__, prop->num_entries);
						break;
					}
				}
			}
			next_offset += sizeof(*prop) -
				sizeof(u32) + prop->num_entries * sizeof(u32);
			num_properties--;
			break;
		}
		default:
			dprintk(VIDC_DBG,
				"%s default case - 0x%x\n", __func__, prop_id);
		}
		rem_bytes -= next_offset;
		data_ptr += next_offset;
	}
	return status;
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

	if (!req_bytes || (req_bytes % sizeof(struct hfi_profile_level))) {
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

	if (!req_bytes || (req_bytes % sizeof(
		struct hfi_buffer_requirements)) ||
		(!prop->rg_property_data[1])) {
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
		if ((hfi_buf_req->buffer_size) &&
			((hfi_buf_req->buffer_count_min > hfi_buf_req->
			buffer_count_actual)))
				dprintk(VIDC_WARN,
					"hal_process_sess_get_prop_buf_req:"
					"bad_buf_req\n");

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

static void hfi_process_session_prop_info(msm_vidc_callback callback,
		u32 device_id, struct hal_session *session,
		struct hfi_msg_session_property_info_packet *pkt)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};
	struct hfi_profile_level profile_level = {0};
	struct buffer_requirements buff_req;

	memset(&buff_req, 0, sizeof(struct buffer_requirements));
	dprintk(VIDC_DBG, "Received SESSION_PROPERTY_INFO[%pK]\n", session);

	if (pkt->size < sizeof(struct hfi_msg_session_property_info_packet)) {
		dprintk(VIDC_ERR,
				"hal_process_session_prop_info: bad_pkt_size\n");
		return;
	}

	if (pkt->num_properties == 0) {
		dprintk(VIDC_ERR,
			"hal_process_session_prop_info: no_properties\n");
		return;
	}

	switch (pkt->rg_property_data[0]) {
	case HFI_PROPERTY_CONFIG_BUFFER_REQUIREMENTS:
		hfi_process_sess_get_prop_buf_req(pkt, &buff_req);
		cmd_done.device_id = device_id;
		cmd_done.session_id = session->session_id;
		cmd_done.status = VIDC_ERR_NONE;
		cmd_done.data = &buff_req;
		cmd_done.size = sizeof(struct buffer_requirements);
		callback(SESSION_PROPERTY_INFO, &cmd_done);
		break;
	case HFI_PROPERTY_PARAM_PROFILE_LEVEL_CURRENT:
		hfi_process_sess_get_prop_profile_level(pkt, &profile_level);
		cmd_done.device_id = device_id;
		cmd_done.session_id = session->session_id;
		cmd_done.status = VIDC_ERR_NONE;
		cmd_done.data = &profile_level;
		cmd_done.size = sizeof(struct hal_profile_level);
		callback(SESSION_PROPERTY_INFO, &cmd_done);
		break;
	default:
		dprintk(VIDC_DBG,
				"hal_process_session_prop_info: unknown_prop_id: %x\n",
				pkt->rg_property_data[0]);
		break;
	}
}

static void hfi_process_session_init_done(
		msm_vidc_callback callback, u32 device_id,
		struct hal_session *session,
		struct hfi_msg_sys_session_init_done_packet *pkt)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};
	struct vidc_hal_session_init_done session_init_done;

	if (!session) {
		dprintk(VIDC_ERR, "%s - invalid session\n", __func__);
		return;
	}

	memset(&session_init_done, 0, sizeof(struct
				vidc_hal_session_init_done));
	dprintk(VIDC_DBG, "RECEIVED: SESSION_INIT_DONE[%pK]\n", session);

	if (sizeof(struct hfi_msg_sys_session_init_done_packet)
		> pkt->size) {
		dprintk(VIDC_ERR,
				"hal_process_session_init_done: bad_pkt_size\n");
		return;
	}

	cmd_done.device_id = device_id;
	cmd_done.session_id = session->session_id;
	cmd_done.status = hfi_map_err_status(pkt->error_type);
	cmd_done.data = &session_init_done;
	if (!cmd_done.status) {
		cmd_done.status = hfi_process_sess_init_done_prop_read(
			pkt, &session_init_done);
	} else {
		dprintk(VIDC_WARN,
			"Sess init failed: 0x%pK, 0x%pK\n",
			session->session_id, session);
	}
	cmd_done.size = sizeof(struct vidc_hal_session_init_done);
	callback(SESSION_INIT_DONE, &cmd_done);
}

static void hfi_process_session_load_res_done(msm_vidc_callback callback,
		u32 device_id, struct hal_session *session,
		struct hfi_msg_session_load_resources_done_packet *pkt)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};
	dprintk(VIDC_DBG, "RECEIVED: SESSION_LOAD_RESOURCES_DONE[%pK]\n",
		session);

	if (sizeof(struct hfi_msg_session_load_resources_done_packet) !=
		pkt->size) {
		dprintk(VIDC_ERR,
				"hal_process_session_load_res_done: bad packet size: %d\n",
				pkt->size);
		return;
	}

	cmd_done.device_id = device_id;
	cmd_done.session_id = session->session_id;
	cmd_done.status = hfi_map_err_status(pkt->error_type);
	cmd_done.data = NULL;
	cmd_done.size = 0;
	callback(SESSION_LOAD_RESOURCE_DONE, &cmd_done);
}

static void hfi_process_session_flush_done(msm_vidc_callback callback,
		u32 device_id, struct hal_session *session,
		struct hfi_msg_session_flush_done_packet *pkt)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};

	dprintk(VIDC_DBG, "RECEIVED: SESSION_FLUSH_DONE[%pK]\n", session);

	if (sizeof(struct hfi_msg_session_flush_done_packet) != pkt->size) {
		dprintk(VIDC_ERR,
				"hal_process_session_flush_done: bad packet size: %d\n",
				pkt->size);
		return;
	}

	cmd_done.device_id = device_id;
	cmd_done.session_id = session->session_id;
	cmd_done.status = hfi_map_err_status(pkt->error_type);
	cmd_done.data = (void *)(unsigned long)pkt->flush_type;
	cmd_done.size = sizeof(u32);
	callback(SESSION_FLUSH_DONE, &cmd_done);
}

static void hfi_process_session_etb_done(msm_vidc_callback callback,
		u32 device_id, struct hal_session *session,
		struct hfi_msg_session_empty_buffer_done_packet *pkt)
{
	struct msm_vidc_cb_data_done data_done = {0};

	dprintk(VIDC_DBG, "RECEIVED: SESSION_ETB_DONE[%pK]\n", session);

	if (!pkt || pkt->size <
		sizeof(struct hfi_msg_session_empty_buffer_done_packet)) {
		dprintk(VIDC_ERR,
				"hal_process_session_etb_done: bad_pkt_size\n");
		return;
	}

	data_done.device_id = device_id;
	data_done.session_id = session->session_id;
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

	trace_msm_v4l2_vidc_buffer_event_end("ETB",
		(u32)pkt->packet_buffer, -1, -1,
		 pkt->filled_len, pkt->offset);
	callback(SESSION_ETB_DONE, &data_done);
}

static void hfi_process_session_ftb_done(msm_vidc_callback callback,
		u32 device_id, struct hal_session *session,
		void *msg_hdr)
{
	struct msm_vidc_cb_data_done data_done = {0};
	u32 is_decoder = session->is_decoder;

	if (!msg_hdr) {
		dprintk(VIDC_ERR, "Invalid Params\n");
		return;
	}

	dprintk(VIDC_DBG, "RECEIVED: SESSION_FTB_DONE[%pK]\n", session);

	if (is_decoder == 0) {
		struct hfi_msg_session_fill_buffer_done_compressed_packet *pkt =
		(struct hfi_msg_session_fill_buffer_done_compressed_packet *)
		msg_hdr;
		if (sizeof(struct
			hfi_msg_session_fill_buffer_done_compressed_packet)
			> pkt->size) {
			dprintk(VIDC_ERR,
				"hal_process_session_ftb_done: bad_pkt_size\n");
			return;
		} else if (pkt->error_type != HFI_ERR_NONE) {
			dprintk(VIDC_ERR,
				"got buffer back with error %x\n",
				pkt->error_type);
			/* Proceed with the FBD */
		}

		data_done.device_id = device_id;
		data_done.session_id = session->session_id;
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
	} else if (is_decoder == 1) {
		struct hfi_msg_session_fbd_uncompressed_plane0_packet *pkt =
		(struct	hfi_msg_session_fbd_uncompressed_plane0_packet *)
		msg_hdr;
		if (sizeof(
			struct hfi_msg_session_fbd_uncompressed_plane0_packet) >
			pkt->size) {
			dprintk(VIDC_ERR,
					"hal_process_session_ftb_done: bad_pkt_size\n");
			return;
		}

		data_done.device_id = device_id;
		data_done.session_id = session->session_id;
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

		if (pkt->stream_id == 0)
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
	callback(SESSION_FTB_DONE, &data_done);
}

static void hfi_process_session_start_done(msm_vidc_callback callback,
		u32 device_id, struct hal_session *session,
		struct hfi_msg_session_start_done_packet *pkt)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};

	dprintk(VIDC_DBG, "RECEIVED: SESSION_START_DONE[%pK]\n", session);

	if (!pkt || pkt->size !=
		sizeof(struct hfi_msg_session_start_done_packet)) {
		dprintk(VIDC_ERR, "%s: bad packet/packet size\n",
			__func__);
		return;
	}

	cmd_done.device_id = device_id;
	cmd_done.session_id = session->session_id;
	cmd_done.status = hfi_map_err_status(pkt->error_type);
	cmd_done.data = NULL;
	cmd_done.size = 0;
	callback(SESSION_START_DONE, &cmd_done);
}

static void hfi_process_session_stop_done(msm_vidc_callback callback,
		u32 device_id, struct hal_session *session,
		struct hfi_msg_session_stop_done_packet *pkt)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};

	dprintk(VIDC_DBG, "RECEIVED: SESSION_STOP_DONE[%pK]\n", session);

	if (!pkt || pkt->size !=
		sizeof(struct hfi_msg_session_stop_done_packet)) {
		dprintk(VIDC_ERR, "%s: bad packet/packet size\n",
			__func__);
		return;
	}

	cmd_done.device_id = device_id;
	cmd_done.session_id = session->session_id;
	cmd_done.status = hfi_map_err_status(pkt->error_type);
	cmd_done.data = NULL;
	cmd_done.size = 0;
	callback(SESSION_STOP_DONE, &cmd_done);
}

static void hfi_process_session_rel_res_done(msm_vidc_callback callback,
		u32 device_id, struct hal_session *session,
		struct hfi_msg_session_release_resources_done_packet *pkt)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};

	dprintk(VIDC_DBG, "RECEIVED: SESSION_RELEASE_RESOURCES_DONE[%pK]\n",
		session);

	if (!pkt || pkt->size !=
		sizeof(struct hfi_msg_session_release_resources_done_packet)) {
		dprintk(VIDC_ERR, "%s: bad packet/packet size\n",
			__func__);
		return;
	}

	cmd_done.device_id = device_id;
	cmd_done.session_id = session->session_id;
	cmd_done.status = hfi_map_err_status(pkt->error_type);
	cmd_done.data = NULL;
	cmd_done.size = 0;
	callback(SESSION_RELEASE_RESOURCE_DONE, &cmd_done);
}

static void hfi_process_session_rel_buf_done(msm_vidc_callback callback,
		u32 device_id, struct hal_session *session,
		struct hfi_msg_session_release_buffers_done_packet *pkt)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};

	if (!pkt || pkt->size <
		sizeof(struct
			   hfi_msg_session_release_buffers_done_packet)) {
		dprintk(VIDC_ERR, "bad packet/packet size %d\n",
			pkt ? pkt->size : 0);
		return;
	}
	dprintk(VIDC_DBG, "RECEIVED:SESSION_RELEASE_BUFFER_DONE[%pK]\n",
			session);

	cmd_done.device_id = device_id;
	cmd_done.size = sizeof(struct msm_vidc_cb_cmd_done);
	cmd_done.session_id = session->session_id;
	cmd_done.status = hfi_map_err_status(pkt->error_type);
	if (pkt->rg_buffer_info) {
		cmd_done.data = (void *) &pkt->rg_buffer_info;
		cmd_done.size = sizeof(struct hfi_buffer_info);
	} else {
		dprintk(VIDC_ERR, "invalid payload in rel_buff_done\n");
	}
	callback(SESSION_RELEASE_BUFFER_DONE, &cmd_done);
}

static void hfi_process_session_end_done(msm_vidc_callback callback,
		u32 device_id, struct hal_session *session,
		struct hfi_msg_sys_session_end_done_packet *pkt)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};

	dprintk(VIDC_DBG, "RECEIVED: SESSION_END_DONE[%pK]\n", session);

	if (!pkt || pkt->size !=
		sizeof(struct hfi_msg_sys_session_end_done_packet)) {
		dprintk(VIDC_ERR, "%s: bad packet/packet size\n", __func__);
		return;
	}

	cmd_done.device_id = device_id;
	cmd_done.session_id = session->session_id;
	cmd_done.status = hfi_map_err_status(pkt->error_type);
	cmd_done.data = NULL;
	cmd_done.size = 0;
	callback(SESSION_END_DONE, &cmd_done);
}

static void hfi_process_session_abort_done(msm_vidc_callback callback,
		u32 device_id, struct hal_session *session,
	struct hfi_msg_sys_session_abort_done_packet *pkt)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};

	dprintk(VIDC_DBG, "RECEIVED: SESSION_ABORT_DONE[%pK]\n", session);

	if (!pkt || pkt->size !=
		sizeof(struct hfi_msg_sys_session_abort_done_packet)) {
		dprintk(VIDC_ERR, "%s: bad packet/packet size: %d\n",
				__func__, pkt ? pkt->size : 0);
		return;
	}
	cmd_done.device_id = device_id;
	cmd_done.session_id = session->session_id;
	cmd_done.status = hfi_map_err_status(pkt->error_type);
	cmd_done.data = NULL;
	cmd_done.size = 0;

	callback(SESSION_ABORT_DONE, &cmd_done);
}

static void hfi_process_sys_idle(msm_vidc_callback callback,
		u32 device_id, struct hfi_msg_sys_idle_packet *pkt)
{
	struct msm_vidc_cb_cmd_done cmd_done = {0};
	cmd_done.device_id = device_id;
	callback(SYS_IDLE, &cmd_done);
}

static void hfi_process_session_get_seq_hdr_done(msm_vidc_callback callback,
		u32 device_id, struct hal_session *session,
	struct hfi_msg_session_get_sequence_header_done_packet *pkt)
{
	struct msm_vidc_cb_data_done data_done = {0};
	if (!pkt || pkt->size !=
		sizeof(struct
		hfi_msg_session_get_sequence_header_done_packet)) {
		dprintk(VIDC_ERR, "%s: bad packet/packet size\n",
			__func__);
		return;
	}
	dprintk(VIDC_DBG, "RECEIVED:SESSION_GET_SEQ_HDR_DONE[%pK]\n", session);

	data_done.device_id = device_id;
	data_done.size = sizeof(struct msm_vidc_cb_data_done);
	data_done.session_id = session->session_id;
	data_done.status = hfi_map_err_status(pkt->error_type);
	data_done.output_done.packet_buffer1 =
		(ion_phys_addr_t)pkt->sequence_header;
	data_done.output_done.filled_len1 = pkt->header_len;
	dprintk(VIDC_INFO, "seq_hdr: 0x%x, Length: %d\n",
		   pkt->sequence_header, pkt->header_len);
	callback(SESSION_GET_SEQ_HDR_DONE, &data_done);
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
	if (smem_table_ptr &&
			((smem_image_index_venus +
				version_string_size) <= smem_block_size))
		memcpy(smem_table_ptr + smem_image_index_venus,
				str_image_version, version_string_size);
}

static void hfi_process_sys_property_info(
		msm_vidc_callback callback, u32 device_id,
		struct hfi_msg_sys_property_info_packet *pkt)
{
	if (!pkt) {
		dprintk(VIDC_ERR, "%s: invalid param\n", __func__);
		return;
	}
	if (pkt->size < sizeof(*pkt)) {
		dprintk(VIDC_ERR,
				"hfi_process_sys_property_info: bad_pkt_size\n");
		return;
	}
	if (pkt->num_properties == 0) {
		dprintk(VIDC_ERR,
				"hfi_process_sys_property_info: no_properties\n");
		return;
	}

	switch (pkt->rg_property_data[0]) {
	case HFI_PROPERTY_SYS_IMAGE_VERSION:
		hfi_process_sys_get_prop_image_version(pkt);
		break;
	default:
		dprintk(VIDC_DBG,
				"hfi_process_sys_property_info: unknown_prop_id: %x\n",
				pkt->rg_property_data[0]);
	}
}

u32 hfi_process_msg_packet(msm_vidc_callback callback, u32 device_id,
		struct vidc_hal_msg_pkt_hdr *msg_hdr,
		struct list_head *sessions, struct mutex *session_lock)
{
	u32 rc = 0;
	struct hal_session *session = NULL;
	typedef void (*session_pkt_func_def)(msm_vidc_callback, u32,
			void *, void *);
	typedef void (*sys_pkt_func_def)(msm_vidc_callback, u32, void *);
	session_pkt_func_def session_pkt_func = NULL;
	sys_pkt_func_def sys_pkt_func = NULL;

	if (!callback || !session_lock || !msg_hdr ||
		msg_hdr->size < VIDC_IFACEQ_MIN_PKT_SIZE) {
		dprintk(VIDC_ERR, "%s: bad packet/packet size\n",
			__func__);
		rc = -EINVAL;
		return rc;
	}

	dprintk(VIDC_INFO, "Received: 0x%x\n", msg_hdr->packet);
	rc = (u32) msg_hdr->packet;
	switch (msg_hdr->packet) {
	case HFI_MSG_EVENT_NOTIFY:
		session_pkt_func =
			(session_pkt_func_def)hfi_process_event_notify;
		break;
	case  HFI_MSG_SYS_INIT_DONE:
		sys_pkt_func = (sys_pkt_func_def)hfi_process_sys_init_done;
		break;
	case HFI_MSG_SYS_SESSION_INIT_DONE:
		session_pkt_func =
			(session_pkt_func_def)hfi_process_session_init_done;
		break;
	case HFI_MSG_SYS_PROPERTY_INFO:
		sys_pkt_func = (sys_pkt_func_def)hfi_process_sys_property_info;
		break;
	case HFI_MSG_SYS_SESSION_END_DONE:
		session_pkt_func =
			(session_pkt_func_def)hfi_process_session_end_done;
		break;
	case HFI_MSG_SESSION_LOAD_RESOURCES_DONE:
		session_pkt_func =
			(session_pkt_func_def)hfi_process_session_load_res_done;
		break;
	case HFI_MSG_SESSION_START_DONE:
		session_pkt_func =
			(session_pkt_func_def)hfi_process_session_start_done;
		break;
	case HFI_MSG_SESSION_STOP_DONE:
		session_pkt_func =
			(session_pkt_func_def)hfi_process_session_stop_done;
		break;
	case HFI_MSG_SESSION_EMPTY_BUFFER_DONE:
		session_pkt_func =
			(session_pkt_func_def)hfi_process_session_etb_done;
		break;
	case HFI_MSG_SESSION_FILL_BUFFER_DONE:
		session_pkt_func =
			(session_pkt_func_def)hfi_process_session_ftb_done;
		break;
	case HFI_MSG_SESSION_FLUSH_DONE:
		session_pkt_func =
			(session_pkt_func_def)hfi_process_session_flush_done;
		break;
	case HFI_MSG_SESSION_PROPERTY_INFO:
		session_pkt_func =
			(session_pkt_func_def)hfi_process_session_prop_info;
		break;
	case HFI_MSG_SESSION_RELEASE_RESOURCES_DONE:
		session_pkt_func =
			(session_pkt_func_def)hfi_process_session_rel_res_done;
		break;
	case HFI_MSG_SYS_RELEASE_RESOURCE:
		sys_pkt_func =
			(sys_pkt_func_def)hfi_process_sys_rel_resource_done;
		break;
	case HFI_MSG_SESSION_GET_SEQUENCE_HEADER_DONE:
		session_pkt_func =
			(session_pkt_func_def)
			hfi_process_session_get_seq_hdr_done;
		break;
	case HFI_MSG_SESSION_RELEASE_BUFFERS_DONE:
		session_pkt_func =
			(session_pkt_func_def)hfi_process_session_rel_buf_done;
		break;
	case HFI_MSG_SYS_SESSION_ABORT_DONE:
		session_pkt_func =
			(session_pkt_func_def)hfi_process_session_abort_done;
		break;
	case HFI_MSG_SYS_IDLE:
		sys_pkt_func =
			(sys_pkt_func_def)hfi_process_sys_idle;
		break;
	case HFI_MSG_SYS_PC_PREP_DONE:
		break;
	default:
		dprintk(VIDC_DBG, "UNKNOWN_MSG_TYPE : %d\n", msg_hdr->packet);
		break;
	}
	mutex_lock(session_lock);
	if (session_pkt_func) {
		struct vidc_hal_session_cmd_pkt *pkt =
			(struct vidc_hal_session_cmd_pkt *)msg_hdr;
		session = hfi_process_get_session(sessions, pkt->session_id);
		/* Event of type HFI_EVENT_SYS_ERROR will not have any session
		 * associated with it */
		if (!session && (msg_hdr->packet != HFI_MSG_EVENT_NOTIFY)) {
			dprintk(VIDC_ERR, "%s Got invalid session id: %d\n",
					__func__, pkt->session_id);
			goto invalid_session;
		}
	}

	if (session_pkt_func)
		session_pkt_func(callback, device_id, session, msg_hdr);
	if (sys_pkt_func)
		sys_pkt_func(callback, device_id, msg_hdr);

invalid_session:
	mutex_unlock(session_lock);
	return rc;
}
