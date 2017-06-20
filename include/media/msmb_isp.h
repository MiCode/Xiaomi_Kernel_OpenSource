/* Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __MSMB_ISP__
#define __MSMB_ISP__

#include <uapi/media/msmb_isp.h>

#ifdef CONFIG_COMPAT
struct msm_isp_event_data32 {
	struct compat_timeval timestamp;
	struct compat_timeval mono_timestamp;
	uint32_t frame_id;
	union {
		struct msm_isp_stats_event stats;
		struct msm_isp_buf_event buf_done;
		struct msm_isp_fetch_eng_event fetch_done;
		struct msm_isp_error_info error_info;
		struct msm_isp_output_info output_info;
		struct msm_isp_sof_info sof_info;
	} u;
};

struct msm_isp32_event_data32 {
	struct compat_timeval timestamp;
	struct compat_timeval mono_timestamp;
	enum msm_vfe_input_src input_intf;
	uint32_t frame_id;
	union {
		struct msm_isp_stats_event stats;
		struct msm_isp_buf_event buf_done;
		struct msm_isp32_error_info error_info;
	} u;
};

#endif
#endif /* __MSMB_ISP__ */
