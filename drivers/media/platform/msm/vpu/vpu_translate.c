/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"VPU, %s: " fmt, __func__

#include <linux/videodev2.h>

#include <uapi/media/msm_vpu.h>
#include "vpu_translate.h"
#include "vpu_property.h"
#include "vpu_ipc.h"

/*
 * Translations between API parameters and HFI configuration parameters *
 */
u32 translate_port_id(u32 port)
{
	if (port >= INPUT_PORT && port <= OUTPUT_PORT2)
		return port + 1;
	else
		return VPU_IPC_PORT_UNUSED;
}

static void __trans_resolution_to_hfi(const struct v4l2_pix_format_mplane *fmt,
		struct frame_resolution *resolution)
{
	resolution->width = fmt->width;
	resolution->height = fmt->height;
	pr_debug("received width = %d, height = %d\n",
			resolution->width, resolution->height);
}

static void __trans_resolution_to_api(const struct frame_resolution *resolution,
		struct v4l2_format *fmt)
{
	fmt->fmt.pix_mp.width = resolution->width;
	fmt->fmt.pix_mp.height = resolution->height;
}

static u32 __trans_scan_mode_to_hfi(enum v4l2_field field)
{
	u32 scan_mode;

	if (field == V4L2_FIELD_NONE || field == V4L2_FIELD_ANY)
		scan_mode = LINESCANPROGRESSIVE;
	else
		scan_mode = LINESCANINTERLACED;
	pr_debug("received scan mode = %d\n", scan_mode);

	return scan_mode;
}

static enum v4l2_field __trans_scan_mode_to_api(u32 scan_mode)
{
	enum v4l2_field field;

	if (scan_mode == LINESCANPROGRESSIVE)
		field = V4L2_FIELD_NONE;
	else if (scan_mode == LINESCANINTERLACED)
		field = V4L2_FIELD_INTERLACED;
	else
		field = 100; /* invalid */

	return field;
}

u32 translate_pixelformat_to_hfi(u32 api_pix_fmt)
{
	u32 hfi_pix_fmt;

	switch (api_pix_fmt) {
	case V4L2_PIX_FMT_RGB24:
		hfi_pix_fmt = PIXEL_FORMAT_RGB888;
		break;
	case V4L2_PIX_FMT_RGB32:
		hfi_pix_fmt = PIXEL_FORMAT_XRGB8888;
		break;
	case V4L2_PIX_FMT_XRGB2:
		hfi_pix_fmt = PIXEL_FORMAT_XRGB2;
		break;
	case V4L2_PIX_FMT_BGR24:
		hfi_pix_fmt = PIXEL_FORMAT_BGR888;
		break;
	case V4L2_PIX_FMT_BGR32:
		hfi_pix_fmt = PIXEL_FORMAT_BGRX8888;
		break;
	case V4L2_PIX_FMT_XBGR2:
		hfi_pix_fmt = PIXEL_FORMAT_XBGR2;
		break;
	case V4L2_PIX_FMT_NV12:
		hfi_pix_fmt = PIXEL_FORMAT_NV12;
		break;
	case V4L2_PIX_FMT_NV21:
		hfi_pix_fmt = PIXEL_FORMAT_NV21;
		break;
	case V4L2_PIX_FMT_YUYV:
		hfi_pix_fmt = PIXEL_FORMAT_YUYV;
		break;
	case V4L2_PIX_FMT_YVYU:
		hfi_pix_fmt = PIXEL_FORMAT_YVYU;
		break;
	case V4L2_PIX_FMT_VYUY:
		hfi_pix_fmt = PIXEL_FORMAT_VYUY;
		break;
	case V4L2_PIX_FMT_UYVY:
		hfi_pix_fmt = PIXEL_FORMAT_UYVY;
		break;
	case V4L2_PIX_FMT_YUYV10:
		hfi_pix_fmt = PIXEL_FORMAT_YUYV10_LOOSE;
		break;
	case V4L2_PIX_FMT_YUV8:
		hfi_pix_fmt = PIXEL_FORMAT_YUV_8BIT_INTERLEAVED_DENSE;
		break;
	case V4L2_PIX_FMT_YUV10:
		hfi_pix_fmt = PIXEL_FORMAT_YUV_10BIT_INTERLEAVED_LOOSE;
		break;
	case V4L2_PIX_FMT_YUYV10BWC:
		hfi_pix_fmt = PIXEL_FORMAT_COMPRESSED_YUYV422;
		break;
	default:
		hfi_pix_fmt = PIXEL_FORMAT_MAX;
		pr_warn("Unsupported api pixel format: %d\n", api_pix_fmt);
		break;
	}

	pr_debug("received pixel format = %d\n", hfi_pix_fmt);
	return hfi_pix_fmt;
}

void translate_colorspace_to_hfi(u32 api_colorspace,
		struct vpu_prop_session_color_space *cs_1,
		struct vpu_prop_session_color_space *cs_2)
{
	switch (api_colorspace) {
	case VPU_CS_RGB_FULL:
		cs_1->cs_config = CONFIG_RGB_RANGE;
		cs_1->value = RGB_RANGE_FULL;
	break;
	case VPU_CS_RGB_LIMITED:
		cs_1->cs_config = CONFIG_RGB_RANGE;
		cs_1->value = RGB_RANGE_LIMITED;
	break;
	case VPU_CS_REC601_FULL:
		cs_1->cs_config = CONFIG_NOMINAL_RANGE;
		cs_1->value = NOMINAL_RANGE_FULL;
		cs_2->cs_config = CONFIG_YCBCR_MATRIX;
		cs_2->value = YCbCr_MATRIX_BT601;
	break;
	case VPU_CS_REC601_LIMITED:
		cs_1->cs_config = CONFIG_NOMINAL_RANGE;
		cs_1->value = NOMINAL_RANGE_LIMITED;
		cs_2->cs_config = CONFIG_YCBCR_MATRIX;
		cs_2->value = YCbCr_MATRIX_BT601;
	break;
	case VPU_CS_REC709_FULL:
		cs_1->cs_config = CONFIG_NOMINAL_RANGE;
		cs_1->value = NOMINAL_RANGE_FULL;
		cs_2->cs_config = CONFIG_YCBCR_MATRIX;
		cs_2->value = YCbCr_MATRIX_BT709;
	break;
	case VPU_CS_REC709_LIMITED:
		cs_1->cs_config = CONFIG_NOMINAL_RANGE;
		cs_1->value = NOMINAL_RANGE_LIMITED;
		cs_2->cs_config = CONFIG_YCBCR_MATRIX;
		cs_2->value = YCbCr_MATRIX_BT709;
	break;
	case VPU_CS_SMPTE240_FULL:
		cs_1->cs_config = CONFIG_NOMINAL_RANGE;
		cs_1->value = NOMINAL_RANGE_FULL;
		cs_2->cs_config = CONFIG_YCBCR_MATRIX;
		cs_2->value = YCbCr_MATRIX_BT240;
	break;
	case VPU_CS_SMPTE240_LIMITED:
		cs_1->cs_config = CONFIG_NOMINAL_RANGE;
		cs_1->value = NOMINAL_RANGE_LIMITED;
		cs_2->cs_config = CONFIG_YCBCR_MATRIX;
		cs_2->value = YCbCr_MATRIX_BT240;
	break;
	default:
		pr_warn("Unsupported api colorspace %d\n", api_colorspace);
	break;
	}
}

u32 translate_input_source(u32 in)
{
	pr_debug("received input source = %d\n", in);

	if (in & VPU_INPUT_TYPE_VCAP)
		return INPUT_SOURCE_VCAP;
	else
		return INPUT_SOURCE_HOST;
}

u32 translate_input_source_ch(u32 in)
{
	u32 hfi_source_ch = 0;

	if (in & VPU_INPUT_TYPE_VCAP) {
		if (in & VPU_PIPE_VCAP0)
			hfi_source_ch = VPU_SOURCE_VCAP_CH_0;
		if (in & VPU_PIPE_VCAP1)
			hfi_source_ch |= VPU_SOURCE_VCAP_CH_1;
	}

	if (hfi_source_ch == 0)
		pr_warn("No VCAP channel set: %d\n", in);

	return hfi_source_ch;
}

u32 translate_output_destination(u32 out)
{
	pr_debug("received output destination = %d\n", out);

	if (out & VPU_OUTPUT_TYPE_DISPLAY)
		return OUTPUT_DEST_MDSS;
	else
		return OUTPUT_DEST_HOST;
}

u32 translate_output_destination_ch(u32 out)
{
	u32 hfi_destination_ch = 0;

	if (out & VPU_OUTPUT_TYPE_DISPLAY) {
		if (out & VPU_PIPE_DISPLAY0)
			hfi_destination_ch = VPU_DEST_MDSS_CH_0;
		if (out & VPU_PIPE_DISPLAY1)
			hfi_destination_ch |= VPU_DEST_MDSS_CH_1;
		if (out & VPU_PIPE_DISPLAY2)
			hfi_destination_ch |= VPU_DEST_MDSS_CH_2;
		if (out & VPU_PIPE_DISPLAY3)
			hfi_destination_ch |= VPU_DEST_MDSS_CH_3;
	}

	if (hfi_destination_ch == 0)
		pr_warn("No MDSS channel set: %d\n", out);

	return hfi_destination_ch;
}

u32 translate_pixelformat_to_api(u32 hfi_pix_fmt)
{
	u32 api_pix_fmt;

	switch (hfi_pix_fmt) {
	case PIXEL_FORMAT_RGB888:
		api_pix_fmt = V4L2_PIX_FMT_RGB24;
		break;
	case PIXEL_FORMAT_XRGB8888:
		api_pix_fmt = V4L2_PIX_FMT_RGB32;
		break;
	case PIXEL_FORMAT_XRGB2:
		api_pix_fmt = V4L2_PIX_FMT_XRGB2;
		break;
	case PIXEL_FORMAT_BGR888:
		api_pix_fmt = V4L2_PIX_FMT_BGR24;
		break;
	case PIXEL_FORMAT_BGRX8888:
		api_pix_fmt = V4L2_PIX_FMT_BGR32;
		break;
	case PIXEL_FORMAT_XBGR2:
		api_pix_fmt = V4L2_PIX_FMT_XBGR2;
		break;
	case PIXEL_FORMAT_NV12:
		api_pix_fmt = V4L2_PIX_FMT_NV12;
		break;
	case PIXEL_FORMAT_NV21:
		api_pix_fmt = V4L2_PIX_FMT_NV21;
		break;
	case PIXEL_FORMAT_YUYV:
		api_pix_fmt = V4L2_PIX_FMT_YUYV;
		break;
	case PIXEL_FORMAT_YVYU:
		api_pix_fmt = V4L2_PIX_FMT_YVYU;
		break;
	case PIXEL_FORMAT_VYUY:
		api_pix_fmt = V4L2_PIX_FMT_VYUY;
		break;
	case PIXEL_FORMAT_UYVY:
		api_pix_fmt = V4L2_PIX_FMT_UYVY;
		break;
	case PIXEL_FORMAT_YUYV10_LOOSE:
		api_pix_fmt = V4L2_PIX_FMT_YUYV10;
		break;
	case PIXEL_FORMAT_YUV_8BIT_INTERLEAVED_DENSE:
		api_pix_fmt = V4L2_PIX_FMT_YUV8;
		break;
	case PIXEL_FORMAT_YUV_10BIT_INTERLEAVED_LOOSE:
		api_pix_fmt = V4L2_PIX_FMT_YUV10;
		break;
	case PIXEL_FORMAT_COMPRESSED_YUYV422:
		api_pix_fmt = V4L2_PIX_FMT_YUYV10BWC;
		break;
	default:
		api_pix_fmt = 0;
		pr_warn("Unsupported hfi pixel format: %d\n", hfi_pix_fmt);
		break;
	}

	return api_pix_fmt;
}

static void __trans_stride_to_hfi(const struct v4l2_pix_format_mplane *fmt,
		struct vpu_frame_info *frame_info)
{
	frame_info->plane0_stride = fmt->plane_fmt[0].bytesperline;
	frame_info->plane1_stride = fmt->plane_fmt[1].bytesperline;
	frame_info->plane2_stride = fmt->plane_fmt[2].bytesperline;
}

static void __trans_stride_to_api(const struct vpu_frame_info *frame_info,
		struct v4l2_format *fmt)
{
	fmt->fmt.pix_mp.plane_fmt[0].bytesperline =
			frame_info->plane0_stride;
	fmt->fmt.pix_mp.plane_fmt[1].bytesperline =
			frame_info->plane1_stride;
	fmt->fmt.pix_mp.plane_fmt[2].bytesperline =
			frame_info->plane2_stride;
}

void translate_input_format_to_hfi(const struct vpu_port_info *port_info,
		struct vpu_prop_session_input *in)
{
	memset(in, 0, sizeof(*in));

	__trans_resolution_to_hfi(&port_info->format,
			&in->frame_info.resolution);
	in->frame_info.pixel_format =
		translate_pixelformat_to_hfi(port_info->format.pixelformat);
	__trans_stride_to_hfi(&port_info->format, &in->frame_info);

	in->video_format = port_info->video_fmt;
	in->input_source = translate_input_source(port_info->source);
	in->scan_mode = port_info->scan_mode;
	in->frame_rate = port_info->framerate;

	translate_roi_rect_to_hfi(&port_info->roi, &in->region_interest);

	in->flags = (port_info->secure_content) ?
			VPU_CHANNEL_FLAG_SECURE_CONTENT : 0;
}

void translate_output_format_to_hfi(const struct vpu_port_info *port_info,
		struct vpu_prop_session_output *out)
{
	memset(out, 0, sizeof(*out));

	__trans_resolution_to_hfi(&port_info->format,
			&out->frame_info.resolution);
	out->frame_info.pixel_format =
		translate_pixelformat_to_hfi(port_info->format.pixelformat);
	__trans_stride_to_hfi(&port_info->format, &out->frame_info);

	out->video_format = port_info->video_fmt;
	out->output_dest = translate_output_destination(port_info->destination);
	out->frame_rate = port_info->framerate;

	translate_roi_rect_to_hfi(&port_info->roi, &out->dest_rect);
	translate_roi_rect_to_hfi(&port_info->roi, &out->target_rect);

	out->flags = (port_info->secure_content) ?
			VPU_CHANNEL_FLAG_SECURE_CONTENT : 0;
}

void translate_input_format_to_api(const struct vpu_prop_session_input *in,
		struct v4l2_format *fmt)
{
	__trans_resolution_to_api(&in->frame_info.resolution, fmt);
	fmt->fmt.pix_mp.field = __trans_scan_mode_to_api(in->scan_mode);
	fmt->fmt.pix_mp.pixelformat =
		translate_pixelformat_to_api(in->frame_info.pixel_format);
	__trans_stride_to_api(&in->frame_info, fmt);
}

void translate_output_format_to_api(const struct vpu_prop_session_output *out,
		struct v4l2_format *fmt)
{
	__trans_resolution_to_api(&out->frame_info.resolution, fmt);
	fmt->fmt.pix_mp.pixelformat =
		translate_pixelformat_to_api(out->frame_info.pixel_format);
	__trans_stride_to_api(&out->frame_info, fmt);
}

void translate_roi_rect_to_hfi(const struct v4l2_rect *crop,
		struct rect *roi)
{
	roi->left = (u32) crop->left;
	roi->top = (u32) crop->top;
	roi->right = (u32) (crop->width + crop->left - 1);
	roi->bottom = (u32) (crop->top + crop->height - 1);
	pr_debug("received left = %d, top = %d, right = %d, bottom = %d\n",
			roi->left, roi->top, roi->right, roi->bottom);
}


void translate_roi_rect_to_api(const struct rect *roi,
		struct v4l2_rect *crop)
{
	crop->left = roi->left;
	crop->top = roi->top;
	crop->width = roi->right - roi->left + 1;
	crop->height = roi->bottom - roi->top + 1;
}

u32 translate_field_to_hfi(enum v4l2_field api_field)
{
	u32 hfi_field;

	switch (api_field) {
	case V4L2_FIELD_ANY:
		/* fall through to progressive */
	case V4L2_FIELD_NONE:
		hfi_field = BUFFER_PKT_FLAG_PROGRESSIVE_FRAME;
		break;
	case V4L2_FIELD_INTERLACED:
		/* fall through to interlaced_tb */
	case V4L2_FIELD_INTERLACED_TB:
		hfi_field = BUFFER_PKT_FLAG_INTERLEAVED_FRAME_TOP_FIRST;
		break;
	case V4L2_FIELD_INTERLACED_BT:
		hfi_field = BUFFER_PKT_FLAG_INTERLEAVED_FRAME_BOT_FIRST;
		break;
	case V4L2_FIELD_SEQ_TB:
		hfi_field = BUFFER_PKT_FLAG_INTERLACED_TOP_FIRST;
		break;
	case V4L2_FIELD_SEQ_BT:
		hfi_field = BUFFER_PKT_FLAG_INTERLACED_BOT_FIRST;
		break;
	case V4L2_FIELD_TOP:
		hfi_field = BUFFER_PKT_FLAG_SINGLE_FIELD_TOP;
		break;
	case V4L2_FIELD_BOTTOM:
		hfi_field = BUFFER_PKT_FLAG_SINGLE_FIELD_BOT;
		break;
	case V4L2_FIELD_ALTERNATE:
		/* fall through to default */
	default:
		pr_warn("Unsupported api field type (v4l2_field=%d)",
				api_field);
		hfi_field = 0;
		break;
	}

	return hfi_field;
}

u32 translate_field_to_api(u32 hfi_field)
{
	enum v4l2_field api_field;

	switch (hfi_field & BUFFER_PKT_FLAG_BUFFER_TYPE_MASK) {
	case BUFFER_PKT_FLAG_PROGRESSIVE_FRAME:
		api_field = (u32) V4L2_FIELD_NONE;
		break;
	case BUFFER_PKT_FLAG_INTERLEAVED_FRAME_TOP_FIRST:
		api_field = (u32) V4L2_FIELD_INTERLACED_TB;
		break;
	case BUFFER_PKT_FLAG_INTERLEAVED_FRAME_BOT_FIRST:
		api_field = (u32) V4L2_FIELD_INTERLACED_BT;
		break;
	case BUFFER_PKT_FLAG_INTERLACED_TOP_FIRST:
		api_field = (u32) V4L2_FIELD_SEQ_TB;
		break;
	case BUFFER_PKT_FLAG_INTERLACED_BOT_FIRST:
		api_field = (u32) V4L2_FIELD_SEQ_BT;
		break;
	case BUFFER_PKT_FLAG_SINGLE_FIELD_TOP:
		api_field = (u32) V4L2_FIELD_TOP;
		break;
	case BUFFER_PKT_FLAG_SINGLE_FIELD_BOT:
		api_field = (u32) V4L2_FIELD_BOTTOM;
		break;
	default:
		pr_warn("Unsupported hfi field type (%d)\n", hfi_field);
		api_field = 100; /* invalid */
		break;
	}

	return api_field;
}

u32 translate_v4l2_scan_mode(enum v4l2_field field)
{
	return __trans_scan_mode_to_hfi(field);
}

void translate_ctrl_value_to_hfi(const void *api_data, void *hfi_data)
{
	const __s32 *value = api_data;
	struct vpu_s_data_value *hfi = hfi_data;

	hfi->flags = PROP_FALSE;
	hfi->value = *value;
}

void translate_ctrl_value_to_api(const void *hfi_data, void *api_data)
{
	const struct vpu_s_data_value *hfi = hfi_data;

	__s32 *value = api_data;
	*value = hfi->value;
}

void translate_ctrl_standard_to_hfi(const void *api_data, void *hfi_data)
{
	const struct vpu_ctrl_standard *api = api_data;
	struct vpu_s_data_value *hfi = hfi_data;

	hfi->flags = api->enable ? PROP_TRUE : PROP_FALSE;
	hfi->value = api->value;
	pr_debug("flags = %d, value = %d\n", hfi->flags, hfi->value);
}

void translate_ctrl_standard_to_api(const void *hfi_data, void *api_data)
{
	struct vpu_ctrl_standard *api = api_data;
	const struct vpu_s_data_value *hfi = hfi_data;

	api->enable = hfi->flags ? PROP_TRUE : PROP_FALSE;
	api->value = hfi->value;
	pr_debug("enable = %d, value = %d\n", api->enable, api->value);
}

void translate_ctrl_auto_manual_to_hfi(const void *api_data, void *hfi_data)
{
	const struct vpu_ctrl_auto_manual *api = api_data;
	struct vpu_s_data_value *hfi = hfi_data;

	if (!api->enable)
		hfi->flags = PROP_MODE_DISABLED;
	else if (api->auto_mode)
		hfi->flags = PROP_MODE_AUTO;
	else
		hfi->flags = PROP_MODE_MANUAL;
	hfi->value = api->value;
	pr_debug("flags = %d, value = %d\n", hfi->flags, hfi->value);
}

void translate_ctrl_auto_manual_to_api(const void *hfi_data, void *api_data)
{
	struct vpu_ctrl_auto_manual *api = api_data;
	const struct vpu_s_data_value *hfi = hfi_data;

	api->enable = PROP_TRUE;
	if (hfi->flags & PROP_MODE_AUTO)
		api->auto_mode = PROP_TRUE;
	else if (hfi->flags & PROP_MODE_MANUAL)
		api->auto_mode = PROP_FALSE;
	else
		api->enable = PROP_FALSE;
	api->value = hfi->value;
	pr_debug("enable = %d, auto_mode = %d, value = %d\n",
			api->enable, api->auto_mode, api->value);
}

void translate_range_mapping_to_hfi(const void *api_data, void *hfi_data)
{
	const struct vpu_ctrl_range_mapping *api = api_data;
	struct vpu_prop_session_range_mapping *hfi = hfi_data;

	hfi->enabled = api->enable ? PROP_TRUE : PROP_FALSE;
	hfi->y_map_range = api->y_range;
	hfi->uv_map_range = api->uv_range;
}

void translate_active_region_param_to_hfi(const void *api_data, void *hfi_data)
{
	const struct vpu_ctrl_active_region_param *api = api_data;
	struct vpu_prop_session_active_region_detect *hfi = hfi_data;
	struct rect *hfi_rect;
	int i = 0;

	hfi->enabled = api->enable ? PROP_TRUE : PROP_FALSE;
	hfi->num_exclusion_regions = api->num_exclusions;
	translate_roi_rect_to_hfi(
			(struct v4l2_rect *) &api->detection_region,
			&hfi->region_of_interest);

	hfi_rect = (struct rect *) (hfi_data + sizeof(*hfi));
	for (i = 0; i < api->num_exclusions; i++)
		translate_roi_rect_to_hfi(
				(struct v4l2_rect *) &api->excluded_regions[i],
				(hfi_rect++));
}

void translate_active_region_result_to_api(const void *hfi_data, void *api_data)
{
	const struct rect *hfi = hfi_data;

	struct v4l2_rect *api = api_data;
	translate_roi_rect_to_api((struct rect *) hfi, api);
}

void translate_hqv_to_hfi(const void *api_data, void *hfi_data)
{
	const struct vpu_ctrl_hqv *api = api_data;
	struct vpu_prop_session_auto_hqv *hfi = hfi_data;

	hfi->enabled = api->enable ? PROP_TRUE : PROP_FALSE;
	hfi->sharpen_strength = api->sharpen_strength;
	hfi->auto_nr_strength = api->auto_nr_strength;
}

void translate_timestamp_to_hfi(const void *api_data, void *hfi_data)
{
	const struct vpu_info_frame_timestamp *api = api_data;
	struct vpu_prop_session_timestamp *hfi = hfi_data;

	hfi->presentation.low = api->pts_low;
	hfi->presentation.high = api->pts_high;
	hfi->qtimer.low = api->qtime_low;
	hfi->qtimer.high = api->qtime_high;
}

void translate_timestamp_to_api(const void *hfi_data, void *api_data)
{
	const struct vpu_prop_session_timestamp *hfi = hfi_data;
	struct vpu_info_frame_timestamp *api = api_data;

	api->pts_low = hfi->presentation.low;
	api->pts_high = hfi->presentation.high;
	api->qtime_low = hfi->qtimer.low;
	api->qtime_high = hfi->qtimer.high;
}
