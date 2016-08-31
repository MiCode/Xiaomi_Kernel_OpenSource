/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#ifndef __NVC_IMAGE_H__
#define __NVC_IMAGE_H__

#include <linux/ioctl.h>

#define NVC_IMAGER_API_CAPS_VER		2
#define NVC_IMAGER_API_STATIC_VER	1
#define NVC_IMAGER_API_DYNAMIC_VER	1
#define NVC_IMAGER_API_BAYER_VER	1

#define NVC_IMAGER_TEST_NONE		0
#define NVC_IMAGER_TEST_COLORBARS	1
#define NVC_IMAGER_TEST_CHECKERBOARD	2
#define NVC_IMAGER_TEST_WALKING1S	3

#define NVC_IMAGER_CROPMODE_NONE	1
#define NVC_IMAGER_CROPMODE_PARTIAL	2

#define NVC_IMAGER_TYPE_HUH		0
#define NVC_IMAGER_TYPE_RAW		1
#define NVC_IMAGER_TYPE_SOC		2

/**
 * Defines camera imager types.
 * Mirrors "NvOdmImagerRegion" in "imager/include/nvodm_imager.h".
 * These must remain in sync.
 */
#define NVC_IMAGER_SENSOR_INTERFACE_PARALLEL_8		1
#define NVC_IMAGER_SENSOR_INTERFACE_PARALLEL_10		2
#define NVC_IMAGER_SENSOR_INTERFACE_SERIAL_A		3
#define NVC_IMAGER_SENSOR_INTERFACE_SERIAL_B		4
#define NVC_IMAGER_SENSOR_INTERFACE_SERIAL_C		5
#define NVC_IMAGER_SENSOR_INTERFACE_SERIAL_AB		6
#define NVC_IMAGER_SENSOR_INTERFACE_CCIR		7
#define NVC_IMAGER_SENSOR_INTERFACE_HOST		8
#define NVC_IMAGER_SENSOR_INTERFACE_HOST_CSI_A		9
#define NVC_IMAGER_SENSOR_INTERFACE_HOST_CSI_B		10
#define NVC_IMAGER_SENSOR_INTERFACE_NUM			11

#define NVC_IMAGER_IDENTIFIER_MAX	32
#define NVC_IMAGER_FORMAT_MAX		4
#define NVC_IMAGER_CLOCK_PROFILE_MAX	2
#define NVC_IMAGER_CAPABILITIES_VERSION2	((0x3434 << 16) | 2)

#define NVC_IMAGER_INT2FLOAT_DIVISOR	1000

#define NVC_FOCUS_INTERNAL		(0x665F4E5643414D69ULL)
#define NVC_FOCUS_GUID(n) (0x665F4E5643414D30ULL | ((n) & 0xF))
#define NVC_TORCH_GUID(n) (0x6C5F4E5643414D30ULL | ((n) & 0xF))


struct nvc_imager_static_nvc {
	__u32 api_version;
	__u32 sensor_type;
	__u32 bits_per_pixel;
	__u32 sensor_id;
	__u32 sensor_id_minor;
	__u32 focal_len;
	__u32 max_aperture;
	__u32 fnumber;
	__u32 view_angle_h;
	__u32 view_angle_v;
	__u32 stereo_cap;
	__u32 res_chg_wait_time;
	__u8 support_isp;
	__u8 align1;
	__u8 align2;
	__u8 align3;
	__u8 fuse_id[16];
	__u32 place_holder1;
	__u32 place_holder2;
	__u32 place_holder3;
	__u32 place_holder4;
} __packed;

struct nvc_imager_dynamic_nvc {
	__u32 api_version;
	__s32 region_start_x;
	__s32 region_start_y;
	__u32 x_scale;
	__u32 y_scale;
	__u32 bracket_caps;
	__u32 flush_count;
	__u32 init_intra_frame_skip;
	__u32 ss_intra_frame_skip;
	__u32 ss_frame_number;
	__u32 coarse_time;
	__u32 max_coarse_diff;
	__u32 min_exposure_course;
	__u32 max_exposure_course;
	__u32 diff_integration_time;
	__u32 line_length;
	__u32 frame_length;
	__u32 min_frame_length;
	__u32 max_frame_length;
	__u32 min_gain;
	__u32 max_gain;
	__u32 inherent_gain;
	__u32 inherent_gain_bin_en;
	__u8 support_bin_control;
	__u8 support_fast_mode;
	__u8 align2;
	__u8 align3;
	__u32 pll_mult;
	__u32 pll_div;
	__u32 mode_sw_wait_frames;
	__u32 place_holder1;
	__u32 place_holder2;
	__u32 place_holder3;
} __packed;

struct nvc_imager_bayer {
	__u32 api_version;
	__s32 res_x;
	__s32 res_y;
	__u32 frame_length;
	__u32 coarse_time;
	__u32 gain;
	__u8 bin_en;
	__u8 align1;
	__u8 align2;
	__u8 align3;
	__u32 place_holder1;
	__u32 place_holder2;
	__u32 place_holder3;
	__u32 place_holder4;
} __packed;

struct nvc_imager_mode {
	__s32 res_x;
	__s32 res_y;
	__s32 active_start_x;
	__s32 active_stary_y;
	__u32 peak_frame_rate;
	__u32 pixel_aspect_ratio;
	__u32 pll_multiplier;
	__u32 crop_mode;
	__u32 rect_left;
	__u32 rect_top;
	__u32 rect_right;
	__u32 rect_bottom;
	__u32 point_x;
	__u32 point_y;
	__u32 type;
} __packed;

struct nvc_imager_dnvc {
	__s32 res_x;
	__s32 res_y;
	struct nvc_imager_mode *p_mode;
	struct nvc_imager_dynamic_nvc *p_dnvc;
} __packed;

struct nvc_imager_mode_list {
	struct nvc_imager_mode *p_modes;
	__u32 *p_num_mode;
} __packed;

struct nvc_clock_profile {
	__u32 external_clock_khz;
	__u32 clock_multiplier;
} __packed;

struct nvc_imager_cap {
	char identifier[NVC_IMAGER_IDENTIFIER_MAX];
	__u32 sensor_nvc_interface;
	__u32 pixel_types[NVC_IMAGER_FORMAT_MAX];
	__u32 orientation;
	__u32 direction;
	__u32 initial_clock_rate_khz;
	struct nvc_clock_profile clock_profiles[NVC_IMAGER_CLOCK_PROFILE_MAX];
	__u32 h_sync_edge;
	__u32 v_sync_edge;
	__u32 mclk_on_vgp0;
	__u8 csi_port;
	__u8 data_lanes;
	__u8 virtual_channel_id;
	__u8 discontinuous_clk_mode;
	__u8 cil_threshold_settle;
	__u8 align1;
	__u8 align2;
	__u8 align3;
	__s32 min_blank_time_width;
	__s32 min_blank_time_height;
	__u32 preferred_mode_index;
	__u64 focuser_guid;
	__u64 torch_guid;
	__u32 cap_version;
	__u8 flash_control_enabled;
	__u8 adjustable_flash_timing;
	__u8 is_hdr;
	__u8 align5;
} __packed;

struct nvc_imager_ae {
	__u32 frame_length;
	__u8  frame_length_enable;
	__u32 coarse_time;
	__u8  coarse_time_enable;
	__u32 gain;
	__u8  gain_enable;
} __packed;

union nvc_imager_flash_control {
	__u16 mode;
	struct {
		__u16 enable:1;		/* enable the on-sensor flash control */
		__u16 edge_trig_en:1;	/* two types of flash controls:
					 * 0 - LED_FLASH_EN - supports continued
					 *     flash level only, doesn't
					 *     support start edge/repeat/dly.
					 * 1 - FLASH_EN - supports control pulse
					 *     control pulse attributes are
					 *     defined below.
					 */
		__u16 start_edge:1;	/* flash control pulse rise position:
					 * 0 - at the start of the next frame.
					 * 1 - at the effective pixel end
					 *     position of the next frame.
					 */
		__u16 repeat:1;		/* flash control pulse repeat:
					 * 0 - only triggers one frame.
					 * 1 - trigger repeats every frame until
					 * Flash_EN = 0.
					 */
		__u16 delay_frm:2;	/* flash control pulse can be delayed
					 * in frame units: (0 - 3) - frame
					 * numbers the pulse is delayed.
					 */
	} settings;
};

#define NVC_IOCTL_CAPS_RD	_IOWR('o', 106, struct nvc_imager_cap)
#define NVC_IOCTL_MODE_WR	_IOW('o', 107, struct nvc_imager_bayer)
#define NVC_IOCTL_MODE_RD	_IOWR('o', 108, struct nvc_imager_mode_list)
#define NVC_IOCTL_STATIC_RD	_IOWR('o', 109, struct nvc_imager_static_nvc)
#define NVC_IOCTL_DYNAMIC_RD	_IOWR('o', 110, struct nvc_imager_dnvc)

#endif /* __NVC_IMAGE_H__ */
