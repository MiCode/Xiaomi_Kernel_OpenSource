/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2021, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2008 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#ifndef _DP_LINK_H_
#define _DP_LINK_H_

#include "dp_aux.h"

#define DS_PORT_STATUS_CHANGED 0x200
#define DP_TEST_BIT_DEPTH_UNKNOWN 0xFFFFFFFF
#define DP_LINK_ENUM_STR(x)		#x

#define DP_PHY_TEST_PATTERN_CP2520_2	0x6
#define DP_PHY_TEST_PATTERN_CP2520_3	0x7

struct drm_dp_aux;

#define DP_LINK_CAP_ENHANCED_FRAMING (1 << 0)

struct drm_dp_link {
	unsigned char revision;
	unsigned int rate;
	unsigned int num_lanes;
	unsigned long capabilities;
};

int dp_link_probe(struct drm_dp_aux *aux, struct drm_dp_link *link);
int dp_link_power_up(struct drm_dp_aux *aux, struct drm_dp_link *link);
int dp_link_power_down(struct drm_dp_aux *aux, struct drm_dp_link *link);
int dp_link_configure(struct drm_dp_aux *aux, struct drm_dp_link *link);

enum dp_link_voltage_level {
	DP_LINK_VOLTAGE_LEVEL_0,
	DP_LINK_VOLTAGE_LEVEL_1,
	DP_LINK_VOLTAGE_LEVEL_2,
	DP_LINK_VOLTAGE_LEVEL_3,
};

enum dp_link_preemaphasis_level {
	DP_LINK_PRE_EMPHASIS_LEVEL_0,
	DP_LINK_PRE_EMPHASIS_LEVEL_1,
	DP_LINK_PRE_EMPHASIS_LEVEL_2,
	DP_LINK_PRE_EMPHASIS_LEVEL_3,
};

struct dp_link_sink_count {
	u32 count;
	bool cp_ready;
};

struct dp_link_test_video {
	u32 test_video_pattern;
	u32 test_bit_depth;
	u32 test_dyn_range;
	u32 test_h_total;
	u32 test_v_total;
	u32 test_h_start;
	u32 test_v_start;
	u32 test_hsync_pol;
	u32 test_hsync_width;
	u32 test_vsync_pol;
	u32 test_vsync_width;
	u32 test_h_width;
	u32 test_v_height;
	u32 test_rr_d;
	u32 test_rr_n;
};

struct dp_link_test_audio {
	u32 test_audio_sampling_rate;
	u32 test_audio_channel_count;
	u32 test_audio_pattern_type;
	u32 test_audio_period_ch_1;
	u32 test_audio_period_ch_2;
	u32 test_audio_period_ch_3;
	u32 test_audio_period_ch_4;
	u32 test_audio_period_ch_5;
	u32 test_audio_period_ch_6;
	u32 test_audio_period_ch_7;
	u32 test_audio_period_ch_8;
};

struct dp_link_hdcp_status {
	int hdcp_state;
	int hdcp_version;
};

struct dp_link_phy_params {
	u32 phy_test_pattern_sel;
	u8 v_level;
	u8 p_level;
	u8 max_v_level;
	u8 max_p_level;
};

struct dp_link_params {
	u32 lane_count;
	u32 bw_code;
};

static inline char *dp_link_get_test_name(u32 test_requested)
{
	switch (test_requested) {
	case DP_TEST_LINK_TRAINING:
		return DP_LINK_ENUM_STR(DP_TEST_LINK_TRAINING);
	case DP_TEST_LINK_VIDEO_PATTERN:
		return DP_LINK_ENUM_STR(DP_TEST_LINK_VIDEO_PATTERN);
	case DP_TEST_LINK_EDID_READ:
		return DP_LINK_ENUM_STR(DP_TEST_LINK_EDID_READ);
	case DP_TEST_LINK_PHY_TEST_PATTERN:
		return DP_LINK_ENUM_STR(DP_TEST_LINK_PHY_TEST_PATTERN);
	case DP_TEST_LINK_AUDIO_PATTERN:
		return DP_LINK_ENUM_STR(DP_TEST_LINK_AUDIO_PATTERN);
	case DS_PORT_STATUS_CHANGED:
		return DP_LINK_ENUM_STR(DS_PORT_STATUS_CHANGED);
	case DP_LINK_STATUS_UPDATED:
		return DP_LINK_ENUM_STR(DP_LINK_STATUS_UPDATED);
	default:
		return "unknown";
	}
}

struct dp_link {
	u32 sink_request;
	u32 test_response;

	struct dp_link_sink_count sink_count;
	struct dp_link_test_video test_video;
	struct dp_link_test_audio test_audio;
	struct dp_link_phy_params phy_params;
	struct dp_link_params link_params;
	struct dp_link_hdcp_status hdcp_status;

	u32 (*get_test_bits_depth)(struct dp_link *dp_link, u32 bpp);
	int (*process_request)(struct dp_link *dp_link);
	int (*get_colorimetry_config)(struct dp_link *dp_link);
	int (*adjust_levels)(struct dp_link *dp_link, u8 *link_status);
	int (*send_psm_request)(struct dp_link *dp_link, bool req);
	void (*send_test_response)(struct dp_link *dp_link);
	int (*psm_config)(struct dp_link *dp_link,
		struct drm_dp_link *link_info, bool enable);
	void (*send_edid_checksum)(struct dp_link *dp_link, u8 checksum);
};

static inline char *dp_link_get_phy_test_pattern(u32 phy_test_pattern_sel)
{
	switch (phy_test_pattern_sel) {
	case DP_PHY_TEST_PATTERN_NONE:
		return DP_LINK_ENUM_STR(DP_PHY_TEST_PATTERN_NONE);
	case DP_PHY_TEST_PATTERN_D10_2:
		return DP_LINK_ENUM_STR(
			DP_PHY_TEST_PATTERN_D10_2);
	case DP_PHY_TEST_PATTERN_ERROR_COUNT:
		return DP_LINK_ENUM_STR(
			DP_PHY_TEST_PATTERN_ERROR_COUNT);
	case DP_PHY_TEST_PATTERN_PRBS7:
		return DP_LINK_ENUM_STR(DP_PHY_TEST_PATTERN_PRBS7);
	case DP_PHY_TEST_PATTERN_80BIT_CUSTOM:
		return DP_LINK_ENUM_STR(
			DP_PHY_TEST_PATTERN_80BIT_CUSTOM);
	case DP_PHY_TEST_PATTERN_CP2520:
		return DP_LINK_ENUM_STR(DP_PHY_TEST_PATTERN_CP2520);
	case DP_PHY_TEST_PATTERN_CP2520_2:
		return DP_LINK_ENUM_STR(DP_PHY_TEST_PATTERN_CP2520_2);
	case DP_PHY_TEST_PATTERN_CP2520_3:
		return DP_LINK_ENUM_STR(DP_PHY_TEST_PATTERN_CP2520_3);
	default:
		return "unknown";
	}
}

/**
 * mdss_dp_test_bit_depth_to_bpp() - convert test bit depth to bpp
 * @tbd: test bit depth
 *
 * Returns the bits per pixel (bpp) to be used corresponding to the
 * git bit depth value. This function assumes that bit depth has
 * already been validated.
 */
static inline u32 dp_link_bit_depth_to_bpp(u32 tbd)
{
	u32 bpp;

	/*
	 * Few simplistic rules and assumptions made here:
	 *    1. Bit depth is per color component
	 *    2. If bit depth is unknown return 0
	 *    3. Assume 3 color components
	 */
	switch (tbd) {
	case DP_TEST_BIT_DEPTH_6:
		bpp = 18;
		break;
	case DP_TEST_BIT_DEPTH_8:
		bpp = 24;
		break;
	case DP_TEST_BIT_DEPTH_10:
		bpp = 30;
		break;
	case DP_TEST_BIT_DEPTH_UNKNOWN:
	default:
		bpp = 0;
	}

	return bpp;
}

/**
 * dp_link_get() - get the functionalities of dp test module
 *
 *
 * return: a pointer to dp_link struct
 */
struct dp_link *dp_link_get(struct device *dev, struct dp_aux *aux, u32 dp_core_revision);

/**
 * dp_link_put() - releases the dp test module's resources
 *
 * @dp_link: an instance of dp_link module
 *
 */
void dp_link_put(struct dp_link *dp_link);

#endif /* _DP_LINK_H_ */
