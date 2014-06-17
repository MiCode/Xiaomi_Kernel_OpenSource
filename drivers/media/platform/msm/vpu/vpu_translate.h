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

#ifndef _H_VPU_TRANSLATE_H_
#define _H_VPU_TRANSLATE_H_

#include <linux/videodev2.h>

#include "vpu_v4l2.h"
#include "vpu_channel.h"
#include "vpu_property.h"

/*
 * Translations between API parameters and HFI parameters
 */
u32 translate_port_id(u32 port);

u32 translate_input_source(u32 in);
u32 translate_input_source_ch(u32 in);
u32 translate_output_destination(u32 out);
u32 translate_output_destination_ch(u32 out);

u32 translate_pixelformat_to_hfi(u32 api_pix_fmt);
u32 translate_pixelformat_to_api(u32 hfi_pix_fmt);

void translate_colorspace_to_hfi(u32 api_colorspace,
		struct vpu_prop_session_color_space *cs_1,
		struct vpu_prop_session_color_space *cs_2);

void translate_input_format_to_hfi(const struct vpu_port_info *port_info,
		struct vpu_prop_session_input *in);
void translate_output_format_to_hfi(const struct vpu_port_info *port_info,
		struct vpu_prop_session_output *out);

void translate_input_format_to_api(const struct vpu_prop_session_input *in,
		struct v4l2_format *fmt);
void translate_output_format_to_api(const struct vpu_prop_session_output *out,
		struct v4l2_format *fmt);

void translate_roi_rect_to_hfi(const struct v4l2_rect *crop,
		struct rect *roi);
void translate_roi_rect_to_api(const struct rect *roi,
		struct v4l2_rect *crop);

u32 translate_field_to_hfi(enum v4l2_field api_field);
u32 translate_field_to_api(u32 hfi_field);

u32 translate_v4l2_scan_mode(enum v4l2_field field);


void translate_ctrl_value_to_hfi(const void *api_data, void *hfi_data);
void translate_ctrl_value_to_api(const void *hfi_data, void *api_data);

void translate_ctrl_standard_to_hfi(const void *api_data, void *hfi_data);
void translate_ctrl_standard_to_api(const void *hfi_data, void *api_data);

void translate_ctrl_auto_manual_to_hfi(const void *api_data, void *hfi_data);
void translate_ctrl_auto_manual_to_api(const void *hfi_data, void *api_data);

void translate_range_mapping_to_hfi(const void *api_data, void *hfi_data);
void translate_range_mapping_to_api(const void *hfi_data, void *api_data);

void translate_active_region_param_to_hfi(const void *api_data,
		void *hfi_data);
void translate_active_region_result_to_api(const void *hfi_data,
		void *api_data);

void translate_deinterlacing_mode_to_hfi(const void *api_data, void *hfi_data);
void translate_deinterlacing_mode_to_api(const void *hfi_data, void *api_data);

void translate_hqv_to_hfi(const void *api_data, void *hfi_data);
void translate_hqv_to_api(const void *hfi_data, void *api_data);

void translate_timestamp_to_hfi(const void *api_data, void *hfi_data);
void translate_timestamp_to_api(const void *hfi_data, void *api_data);

#endif /* _H_VPU_TRANSLATE_H_ */
