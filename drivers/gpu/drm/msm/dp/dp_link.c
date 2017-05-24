/*
 * Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"[drm-dp] %s: " fmt, __func__

#include "dp_link.h"
#include "dp_panel.h"

#define DP_LINK_ENUM_STR(x)		#x

enum dp_lane_count {
	DP_LANE_COUNT_1	= 1,
	DP_LANE_COUNT_2	= 2,
	DP_LANE_COUNT_4	= 4,
};

enum phy_test_pattern {
	PHY_TEST_PATTERN_NONE,
	PHY_TEST_PATTERN_D10_2_NO_SCRAMBLING,
	PHY_TEST_PATTERN_SYMBOL_ERR_MEASUREMENT_CNT,
	PHY_TEST_PATTERN_PRBS7,
	PHY_TEST_PATTERN_80_BIT_CUSTOM_PATTERN,
	PHY_TEST_PATTERN_HBR2_CTS_EYE_PATTERN,
};

enum dynamic_range {
	DP_DYNAMIC_RANGE_RGB_VESA = 0x00,
	DP_DYNAMIC_RANGE_RGB_CEA = 0x01,
	DP_DYNAMIC_RANGE_UNKNOWN = 0xFFFFFFFF,
};

enum test_video_pattern {
	DP_TEST_VIDEO_PATTERN_NONE = 0x00,
	DP_TEST_VIDEO_PATTERN_COLOR_RAMPS = 0x01,
	DP_TEST_VIDEO_PATTERN_BW_VERT_LINES = 0x02,
	DP_TEST_VIDEO_PATTERN_COLOR_SQUARE = 0x03,
};

enum test_bit_depth {
	DP_TEST_BIT_DEPTH_6 = 0x00,
	DP_TEST_BIT_DEPTH_8 = 0x01,
	DP_TEST_BIT_DEPTH_10 = 0x02,
	DP_TEST_BIT_DEPTH_UNKNOWN = 0xFFFFFFFF,
};

enum dp_link_response {
	TEST_ACK			= 0x1,
	TEST_NACK			= 0x2,
	TEST_EDID_CHECKSUM_WRITE	= 0x4,
};

enum audio_sample_rate {
	AUDIO_SAMPLE_RATE_32_KHZ	= 0x00,
	AUDIO_SAMPLE_RATE_44_1_KHZ	= 0x01,
	AUDIO_SAMPLE_RATE_48_KHZ	= 0x02,
	AUDIO_SAMPLE_RATE_88_2_KHZ	= 0x03,
	AUDIO_SAMPLE_RATE_96_KHZ	= 0x04,
	AUDIO_SAMPLE_RATE_176_4_KHZ	= 0x05,
	AUDIO_SAMPLE_RATE_192_KHZ	= 0x06,
};

enum audio_pattern_type {
	AUDIO_TEST_PATTERN_OPERATOR_DEFINED	= 0x00,
	AUDIO_TEST_PATTERN_SAWTOOTH		= 0x01,
};

struct dp_link_request {
	u32 test_requested;
	u32 test_link_rate;
	u32 test_lane_count;
	u32 phy_test_pattern_sel;
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
	u32 response;
};

struct dp_link_sink_count {
	u32 count;
	bool cp_ready;
};

struct dp_link_private {
	struct device *dev;
	struct dp_aux *aux;
	struct dp_link dp_link;

	struct dp_link_request request;
	struct dp_link_sink_count sink_count;
	u8 link_status[DP_LINK_STATUS_SIZE];
};

/**
 * mdss_dp_test_bit_depth_to_bpp() - convert test bit depth to bpp
 * @tbd: test bit depth
 *
 * Returns the bits per pixel (bpp) to be used corresponding to the
 * git bit depth value. This function assumes that bit depth has
 * already been validated.
 */
static inline u32 dp_link_bit_depth_to_bpp(enum test_bit_depth tbd)
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

static char *dp_link_get_phy_test_pattern(u32 phy_test_pattern_sel)
{
	switch (phy_test_pattern_sel) {
	case PHY_TEST_PATTERN_NONE:
		return DP_LINK_ENUM_STR(PHY_TEST_PATTERN_NONE);
	case PHY_TEST_PATTERN_D10_2_NO_SCRAMBLING:
		return DP_LINK_ENUM_STR(PHY_TEST_PATTERN_D10_2_NO_SCRAMBLING);
	case PHY_TEST_PATTERN_SYMBOL_ERR_MEASUREMENT_CNT:
		return DP_LINK_ENUM_STR(
			PHY_TEST_PATTERN_SYMBOL_ERR_MEASUREMENT_CNT);
	case PHY_TEST_PATTERN_PRBS7:
		return DP_LINK_ENUM_STR(PHY_TEST_PATTERN_PRBS7);
	case PHY_TEST_PATTERN_80_BIT_CUSTOM_PATTERN:
		return DP_LINK_ENUM_STR(PHY_TEST_PATTERN_80_BIT_CUSTOM_PATTERN);
	case PHY_TEST_PATTERN_HBR2_CTS_EYE_PATTERN:
		return DP_LINK_ENUM_STR(PHY_TEST_PATTERN_HBR2_CTS_EYE_PATTERN);
	default:
		return "unknown";
	}
}

static char *dp_link_get_audio_test_pattern(u32 pattern)
{
	switch (pattern) {
	case AUDIO_TEST_PATTERN_OPERATOR_DEFINED:
		return DP_LINK_ENUM_STR(AUDIO_TEST_PATTERN_OPERATOR_DEFINED);
	case AUDIO_TEST_PATTERN_SAWTOOTH:
		return DP_LINK_ENUM_STR(AUDIO_TEST_PATTERN_SAWTOOTH);
	default:
		return "unknown";
	}
}

static char *dp_link_get_audio_sample_rate(u32 rate)
{
	switch (rate) {
	case AUDIO_SAMPLE_RATE_32_KHZ:
		return DP_LINK_ENUM_STR(AUDIO_SAMPLE_RATE_32_KHZ);
	case AUDIO_SAMPLE_RATE_44_1_KHZ:
		return DP_LINK_ENUM_STR(AUDIO_SAMPLE_RATE_44_1_KHZ);
	case AUDIO_SAMPLE_RATE_48_KHZ:
		return DP_LINK_ENUM_STR(AUDIO_SAMPLE_RATE_48_KHZ);
	case AUDIO_SAMPLE_RATE_88_2_KHZ:
		return DP_LINK_ENUM_STR(AUDIO_SAMPLE_RATE_88_2_KHZ);
	case AUDIO_SAMPLE_RATE_96_KHZ:
		return DP_LINK_ENUM_STR(AUDIO_SAMPLE_RATE_96_KHZ);
	case AUDIO_SAMPLE_RATE_176_4_KHZ:
		return DP_LINK_ENUM_STR(AUDIO_SAMPLE_RATE_176_4_KHZ);
	case AUDIO_SAMPLE_RATE_192_KHZ:
		return DP_LINK_ENUM_STR(AUDIO_SAMPLE_RATE_192_KHZ);
	default:
		return "unknown";
	}
}

static int dp_link_get_period(struct dp_link_private *link, int const addr)
{
	int ret = 0;
	u8 *bp;
	u8 data;
	u32 const param_len = 0x1;
	u32 const max_audio_period = 0xA;

	/* TEST_AUDIO_PERIOD_CH_XX */
	if (drm_dp_dpcd_read(link->aux->drm_aux, addr, &bp,
		param_len) < param_len) {
		pr_err("failed to read test_audio_period (0x%x)\n", addr);
		ret = -EINVAL;
		goto exit;
	}

	data = *bp;

	/* Period - Bits 3:0 */
	data = data & 0xF;
	if ((int)data > max_audio_period) {
		pr_err("invalid test_audio_period_ch_1 = 0x%x\n", data);
		ret = -EINVAL;
		goto exit;
	}

	ret = data;
exit:
	return ret;
}

static int dp_link_parse_audio_channel_period(struct dp_link_private *link)
{
	int ret = 0;
	int const test_audio_period_ch_1_addr = 0x273;
	int const test_audio_period_ch_2_addr = 0x274;
	int const test_audio_period_ch_3_addr = 0x275;
	int const test_audio_period_ch_4_addr = 0x276;
	int const test_audio_period_ch_5_addr = 0x277;
	int const test_audio_period_ch_6_addr = 0x278;
	int const test_audio_period_ch_7_addr = 0x279;
	int const test_audio_period_ch_8_addr = 0x27A;
	struct dp_link_request *req = &link->request;

	/* TEST_AUDIO_PERIOD_CH_1 (Byte 0x273) */
	ret = dp_link_get_period(link, test_audio_period_ch_1_addr);
	if (ret == -EINVAL)
		goto exit;

	req->test_audio_period_ch_1 = ret;
	pr_debug("test_audio_period_ch_1 = 0x%x\n", ret);

	/* TEST_AUDIO_PERIOD_CH_2 (Byte 0x274) */
	ret = dp_link_get_period(link, test_audio_period_ch_2_addr);
	if (ret == -EINVAL)
		goto exit;

	req->test_audio_period_ch_2 = ret;
	pr_debug("test_audio_period_ch_2 = 0x%x\n", ret);

	/* TEST_AUDIO_PERIOD_CH_3 (Byte 0x275) */
	ret = dp_link_get_period(link, test_audio_period_ch_3_addr);
	if (ret == -EINVAL)
		goto exit;

	req->test_audio_period_ch_3 = ret;
	pr_debug("test_audio_period_ch_3 = 0x%x\n", ret);

	/* TEST_AUDIO_PERIOD_CH_4 (Byte 0x276) */
	ret = dp_link_get_period(link, test_audio_period_ch_4_addr);
	if (ret == -EINVAL)
		goto exit;

	req->test_audio_period_ch_4 = ret;
	pr_debug("test_audio_period_ch_4 = 0x%x\n", ret);

	/* TEST_AUDIO_PERIOD_CH_5 (Byte 0x277) */
	ret = dp_link_get_period(link, test_audio_period_ch_5_addr);
	if (ret == -EINVAL)
		goto exit;

	req->test_audio_period_ch_5 = ret;
	pr_debug("test_audio_period_ch_5 = 0x%x\n", ret);

	/* TEST_AUDIO_PERIOD_CH_6 (Byte 0x278) */
	ret = dp_link_get_period(link, test_audio_period_ch_6_addr);
	if (ret == -EINVAL)
		goto exit;

	req->test_audio_period_ch_6 = ret;
	pr_debug("test_audio_period_ch_6 = 0x%x\n", ret);

	/* TEST_AUDIO_PERIOD_CH_7 (Byte 0x279) */
	ret = dp_link_get_period(link, test_audio_period_ch_7_addr);
	if (ret == -EINVAL)
		goto exit;

	req->test_audio_period_ch_7 = ret;
	pr_debug("test_audio_period_ch_7 = 0x%x\n", ret);

	/* TEST_AUDIO_PERIOD_CH_8 (Byte 0x27A) */
	ret = dp_link_get_period(link, test_audio_period_ch_8_addr);
	if (ret == -EINVAL)
		goto exit;

	req->test_audio_period_ch_8 = ret;
	pr_debug("test_audio_period_ch_8 = 0x%x\n", ret);
exit:
	return ret;
}

static int dp_link_parse_audio_pattern_type(struct dp_link_private *link)
{
	int ret = 0;
	u8 *bp;
	u8 data;
	int rlen;
	int const param_len = 0x1;
	int const test_audio_pattern_type_addr = 0x272;
	int const max_audio_pattern_type = 0x1;

	/* Read the requested audio pattern type (Byte 0x272). */
	rlen = drm_dp_dpcd_read(link->aux->drm_aux,
		test_audio_pattern_type_addr, &bp, param_len);
	if (rlen < param_len) {
		pr_err("failed to read link audio mode data\n");
		ret = -EINVAL;
		goto exit;
	}
	data = *bp;

	/* Audio Pattern Type - Bits 7:0 */
	if ((int)data > max_audio_pattern_type) {
		pr_err("invalid audio pattern type = 0x%x\n", data);
		ret = -EINVAL;
		goto exit;
	}

	link->request.test_audio_pattern_type = data;
	pr_debug("audio pattern type = %s\n",
			dp_link_get_audio_test_pattern(data));
exit:
	return ret;
}

static int dp_link_parse_audio_mode(struct dp_link_private *link)
{
	int ret = 0;
	u8 *bp;
	u8 data;
	int rlen;
	int const param_len = 0x1;
	int const test_audio_mode_addr = 0x271;
	int const max_audio_sampling_rate = 0x6;
	int const max_audio_channel_count = 0x8;
	int sampling_rate = 0x0;
	int channel_count = 0x0;

	/* Read the requested audio mode (Byte 0x271). */
	rlen = drm_dp_dpcd_read(link->aux->drm_aux, test_audio_mode_addr,
			&bp, param_len);
	if (rlen < param_len) {
		pr_err("failed to read link audio mode data\n");
		ret = -EINVAL;
		goto exit;
	}
	data = *bp;

	/* Sampling Rate - Bits 3:0 */
	sampling_rate = data & 0xF;
	if (sampling_rate > max_audio_sampling_rate) {
		pr_err("sampling rate (0x%x) greater than max (0x%x)\n",
				sampling_rate, max_audio_sampling_rate);
		ret = -EINVAL;
		goto exit;
	}

	/* Channel Count - Bits 7:4 */
	channel_count = ((data & 0xF0) >> 4) + 1;
	if (channel_count > max_audio_channel_count) {
		pr_err("channel_count (0x%x) greater than max (0x%x)\n",
				channel_count, max_audio_channel_count);
		ret = -EINVAL;
		goto exit;
	}

	link->request.test_audio_sampling_rate = sampling_rate;
	link->request.test_audio_channel_count = channel_count;
	pr_debug("sampling_rate = %s, channel_count = 0x%x\n",
		dp_link_get_audio_sample_rate(sampling_rate), channel_count);
exit:
	return ret;
}

/**
 * dp_parse_audio_pattern_params() - parses audio pattern parameters from DPCD
 * @link: Display Port Driver data
 *
 * Returns 0 if it successfully parses the audio link pattern parameters.
 */
static int dp_link_parse_audio_pattern_params(struct dp_link_private *link)
{
	int ret = 0;

	ret = dp_link_parse_audio_mode(link);
	if (ret)
		goto exit;

	ret = dp_link_parse_audio_pattern_type(link);
	if (ret)
		goto exit;

	ret = dp_link_parse_audio_channel_period(link);

exit:
	return ret;
}

/**
 * dp_link_is_video_pattern_valid() - validates the video pattern
 * @pattern: video pattern requested by the sink
 *
 * Returns true if the requested video pattern is supported.
 */
static bool dp_link_is_video_pattern_valid(u32 pattern)
{
	switch (pattern) {
	case DP_TEST_VIDEO_PATTERN_NONE:
	case DP_TEST_VIDEO_PATTERN_COLOR_RAMPS:
	case DP_TEST_VIDEO_PATTERN_BW_VERT_LINES:
	case DP_TEST_VIDEO_PATTERN_COLOR_SQUARE:
		return true;
	default:
		return false;
	}
}

static char *dp_link_video_pattern_to_string(u32 test_video_pattern)
{
	switch (test_video_pattern) {
	case DP_TEST_VIDEO_PATTERN_NONE:
		return DP_LINK_ENUM_STR(DP_TEST_VIDEO_PATTERN_NONE);
	case DP_TEST_VIDEO_PATTERN_COLOR_RAMPS:
		return DP_LINK_ENUM_STR(DP_TEST_VIDEO_PATTERN_COLOR_RAMPS);
	case DP_TEST_VIDEO_PATTERN_BW_VERT_LINES:
		return DP_LINK_ENUM_STR(DP_TEST_VIDEO_PATTERN_BW_VERT_LINES);
	case DP_TEST_VIDEO_PATTERN_COLOR_SQUARE:
		return DP_LINK_ENUM_STR(DP_TEST_VIDEO_PATTERN_COLOR_SQUARE);
	default:
		return "unknown";
	}
}

/**
 * dp_link_is_dynamic_range_valid() - validates the dynamic range
 * @bit_depth: the dynamic range value to be checked
 *
 * Returns true if the dynamic range value is supported.
 */
static bool dp_link_is_dynamic_range_valid(u32 dr)
{
	switch (dr) {
	case DP_DYNAMIC_RANGE_RGB_VESA:
	case DP_DYNAMIC_RANGE_RGB_CEA:
		return true;
	default:
		return false;
	}
}

static char *dp_link_dynamic_range_to_string(u32 dr)
{
	switch (dr) {
	case DP_DYNAMIC_RANGE_RGB_VESA:
		return DP_LINK_ENUM_STR(DP_DYNAMIC_RANGE_RGB_VESA);
	case DP_DYNAMIC_RANGE_RGB_CEA:
		return DP_LINK_ENUM_STR(DP_DYNAMIC_RANGE_RGB_CEA);
	case DP_DYNAMIC_RANGE_UNKNOWN:
	default:
		return "unknown";
	}
}

/**
 * dp_link_is_bit_depth_valid() - validates the bit depth requested
 * @bit_depth: bit depth requested by the sink
 *
 * Returns true if the requested bit depth is supported.
 */
static bool dp_link_is_bit_depth_valid(u32 tbd)
{
	/* DP_TEST_VIDEO_PATTERN_NONE is treated as invalid */
	switch (tbd) {
	case DP_TEST_BIT_DEPTH_6:
	case DP_TEST_BIT_DEPTH_8:
	case DP_TEST_BIT_DEPTH_10:
		return true;
	default:
		return false;
	}
}

static char *dp_link_bit_depth_to_string(u32 tbd)
{
	switch (tbd) {
	case DP_TEST_BIT_DEPTH_6:
		return DP_LINK_ENUM_STR(DP_TEST_BIT_DEPTH_6);
	case DP_TEST_BIT_DEPTH_8:
		return DP_LINK_ENUM_STR(DP_TEST_BIT_DEPTH_8);
	case DP_TEST_BIT_DEPTH_10:
		return DP_LINK_ENUM_STR(DP_TEST_BIT_DEPTH_10);
	case DP_TEST_BIT_DEPTH_UNKNOWN:
	default:
		return "unknown";
	}
}

static int dp_link_parse_timing_params1(struct dp_link_private *link,
	int const addr, int const len, u32 *val)
{
	u8 *bp;
	int rlen;

	if (len < 2)
		return -EINVAL;

	/* Read the requested video link pattern (Byte 0x221). */
	rlen = drm_dp_dpcd_read(link->aux->drm_aux, addr, &bp, len);
	if (rlen < len) {
		pr_err("failed to read 0x%x\n", addr);
		return -EINVAL;
	}

	*val = bp[1] | (bp[0] << 8);

	return 0;
}

static int dp_link_parse_timing_params2(struct dp_link_private *link,
	int const addr, int const len, u32 *val1, u32 *val2)
{
	u8 *bp;
	int rlen;

	if (len < 2)
		return -EINVAL;

	/* Read the requested video link pattern (Byte 0x221). */
	rlen = drm_dp_dpcd_read(link->aux->drm_aux, addr, &bp, len);
	if (rlen < len) {
		pr_err("failed to read 0x%x\n", addr);
		return -EINVAL;
	}

	*val1 = (bp[0] & BIT(7)) >> 7;
	*val2 = bp[1] | ((bp[0] & 0x7F) << 8);

	return 0;
}

static int dp_link_parse_timing_params3(struct dp_link_private *link,
	int const addr, u32 *val)
{
	u8 *bp;
	u32 len = 1;
	int rlen;

	/* Read the requested video link pattern (Byte 0x221). */
	rlen = drm_dp_dpcd_read(link->aux->drm_aux, addr, &bp, len);
	if (rlen < 1) {
		pr_err("failed to read 0x%x\n", addr);
		return -EINVAL;
	}
	*val = bp[0];

	return 0;
}

/**
 * dp_parse_video_pattern_params() - parses video pattern parameters from DPCD
 * @link: Display Port Driver data
 *
 * Returns 0 if it successfully parses the video link pattern and the link
 * bit depth requested by the sink and, and if the values parsed are valid.
 */
static int dp_link_parse_video_pattern_params(struct dp_link_private *link)
{
	int ret = 0;
	int rlen;
	u8 *bp;
	u8 data;
	u32 dyn_range;
	int const param_len = 0x1;
	int const test_video_pattern_addr = 0x221;
	int const test_misc_addr = 0x232;

	/* Read the requested video link pattern (Byte 0x221). */
	rlen = drm_dp_dpcd_read(link->aux->drm_aux, test_video_pattern_addr,
			&bp, param_len);
	if (rlen < param_len) {
		pr_err("failed to read link video pattern\n");
		ret = -EINVAL;
		goto exit;
	}
	data = *bp;

	if (!dp_link_is_video_pattern_valid(data)) {
		pr_err("invalid link video pattern = 0x%x\n", data);
		ret = -EINVAL;
		goto exit;
	}

	link->request.test_video_pattern = data;
	pr_debug("link video pattern = 0x%x (%s)\n",
		link->request.test_video_pattern,
		dp_link_video_pattern_to_string(
			link->request.test_video_pattern));

	/* Read the requested color bit depth and dynamic range (Byte 0x232) */
	rlen = drm_dp_dpcd_read(link->aux->drm_aux, test_misc_addr,
			&bp, param_len);
	if (rlen < param_len) {
		pr_err("failed to read link bit depth\n");
		ret = -EINVAL;
		goto exit;
	}
	data = *bp;

	/* Dynamic Range */
	dyn_range = (data & BIT(3)) >> 3;
	if (!dp_link_is_dynamic_range_valid(dyn_range)) {
		pr_err("invalid link dynamic range = 0x%x", dyn_range);
		ret = -EINVAL;
		goto exit;
	}
	link->request.test_dyn_range = dyn_range;
	pr_debug("link dynamic range = 0x%x (%s)\n",
		link->request.test_dyn_range,
		dp_link_dynamic_range_to_string(
			link->request.test_dyn_range));

	/* Color bit depth */
	data &= (BIT(5) | BIT(6) | BIT(7));
	data >>= 5;
	if (!dp_link_is_bit_depth_valid(data)) {
		pr_err("invalid link bit depth = 0x%x\n", data);
		ret = -EINVAL;
		goto exit;
	}

	link->request.test_bit_depth = data;
	pr_debug("link bit depth = 0x%x (%s)\n",
		link->request.test_bit_depth,
		dp_link_bit_depth_to_string(link->request.test_bit_depth));

	/* resolution timing params */
	ret = dp_link_parse_timing_params1(link, 0x222, 2,
			&link->request.test_h_total);
	if (ret) {
		pr_err("failed to parse test_h_total (0x222)\n");
		goto exit;
	}
	pr_debug("TEST_H_TOTAL = %d\n", link->request.test_h_total);

	ret = dp_link_parse_timing_params1(link, 0x224, 2,
			&link->request.test_v_total);
	if (ret) {
		pr_err("failed to parse test_v_total (0x224)\n");
		goto exit;
	}
	pr_debug("TEST_V_TOTAL = %d\n", link->request.test_v_total);

	ret = dp_link_parse_timing_params1(link, 0x226, 2,
			&link->request.test_h_start);
	if (ret) {
		pr_err("failed to parse test_h_start (0x226)\n");
		goto exit;
	}
	pr_debug("TEST_H_START = %d\n", link->request.test_h_start);

	ret = dp_link_parse_timing_params1(link, 0x228, 2,
			&link->request.test_v_start);
	if (ret) {
		pr_err("failed to parse test_v_start (0x228)\n");
		goto exit;
	}
	pr_debug("TEST_V_START = %d\n", link->request.test_v_start);

	ret = dp_link_parse_timing_params2(link, 0x22A, 2,
			&link->request.test_hsync_pol,
			&link->request.test_hsync_width);
	if (ret) {
		pr_err("failed to parse (0x22A)\n");
		goto exit;
	}
	pr_debug("TEST_HSYNC_POL = %d\n", link->request.test_hsync_pol);
	pr_debug("TEST_HSYNC_WIDTH = %d\n", link->request.test_hsync_width);

	ret = dp_link_parse_timing_params2(link, 0x22C, 2,
			&link->request.test_vsync_pol,
			&link->request.test_vsync_width);
	if (ret) {
		pr_err("failed to parse (0x22C)\n");
		goto exit;
	}
	pr_debug("TEST_VSYNC_POL = %d\n", link->request.test_vsync_pol);
	pr_debug("TEST_VSYNC_WIDTH = %d\n", link->request.test_vsync_width);

	ret = dp_link_parse_timing_params1(link, 0x22E, 2,
			&link->request.test_h_width);
	if (ret) {
		pr_err("failed to parse test_h_width (0x22E)\n");
		goto exit;
	}
	pr_debug("TEST_H_WIDTH = %d\n", link->request.test_h_width);

	ret = dp_link_parse_timing_params1(link, 0x230, 2,
			&link->request.test_v_height);
	if (ret) {
		pr_err("failed to parse test_v_height (0x230)\n");
		goto exit;
	}
	pr_debug("TEST_V_HEIGHT = %d\n", link->request.test_v_height);

	ret = dp_link_parse_timing_params3(link, 0x233,
		&link->request.test_rr_d);
	link->request.test_rr_d &= BIT(0);
	if (ret) {
		pr_err("failed to parse test_rr_d (0x233)\n");
		goto exit;
	}
	pr_debug("TEST_REFRESH_DENOMINATOR = %d\n", link->request.test_rr_d);

	ret = dp_link_parse_timing_params3(link, 0x234,
		&link->request.test_rr_n);
	if (ret) {
		pr_err("failed to parse test_rr_n (0x234)\n");
		goto exit;
	}
	pr_debug("TEST_REFRESH_NUMERATOR = %d\n", link->request.test_rr_n);
exit:
	return ret;
}

/**
 * dp_link_is_link_rate_valid() - validates the link rate
 * @lane_rate: link rate requested by the sink
 *
 * Returns true if the requested link rate is supported.
 */
static bool dp_link_is_link_rate_valid(u32 link_rate)
{
	return ((link_rate == DP_LINK_BW_1_62) ||
		(link_rate == DP_LINK_BW_2_7) ||
		(link_rate == DP_LINK_BW_5_4) ||
		(link_rate == DP_LINK_RATE_810));
}

/**
 * dp_link_is_lane_count_valid() - validates the lane count
 * @lane_count: lane count requested by the sink
 *
 * Returns true if the requested lane count is supported.
 */
static bool dp_link_is_lane_count_valid(u32 lane_count)
{
	return (lane_count == DP_LANE_COUNT_1) ||
		(lane_count == DP_LANE_COUNT_2) ||
		(lane_count == DP_LANE_COUNT_4);
}

/**
 * dp_link_parse_link_training_params() - parses link training parameters from
 * DPCD
 * @link: Display Port Driver data
 *
 * Returns 0 if it successfully parses the link rate (Byte 0x219) and lane
 * count (Byte 0x220), and if these values parse are valid.
 */
static int dp_link_parse_link_training_params(struct dp_link_private *link)
{
	u8 *bp;
	u8 data;
	int ret = 0;
	int rlen;
	int const param_len = 0x1;

	/* Read the requested link rate (Byte 0x219). */
	rlen = drm_dp_dpcd_read(link->aux->drm_aux, DP_TEST_LINK_RATE,
			&bp, param_len);
	if (rlen < param_len) {
		pr_err("failed to read link rate\n");
		ret = -EINVAL;
		goto exit;
	}
	data = *bp;

	if (!dp_link_is_link_rate_valid(data)) {
		pr_err("invalid link rate = 0x%x\n", data);
		ret = -EINVAL;
		goto exit;
	}

	link->request.test_link_rate = data;
	pr_debug("link rate = 0x%x\n", link->request.test_link_rate);

	/* Read the requested lane count (Byte 0x220). */
	rlen = drm_dp_dpcd_read(link->aux->drm_aux, DP_TEST_LANE_COUNT,
			&bp, param_len);
	if (rlen < param_len) {
		pr_err("failed to read lane count\n");
		ret = -EINVAL;
		goto exit;
	}
	data = *bp;
	data &= 0x1F;

	if (!dp_link_is_lane_count_valid(data)) {
		pr_err("invalid lane count = 0x%x\n", data);
		ret = -EINVAL;
		goto exit;
	}

	link->request.test_lane_count = data;
	pr_debug("lane count = 0x%x\n", link->request.test_lane_count);
exit:
	return ret;
}

static bool dp_link_is_phy_test_pattern_supported(u32 phy_test_pattern_sel)
{
	switch (phy_test_pattern_sel) {
	case PHY_TEST_PATTERN_NONE:
	case PHY_TEST_PATTERN_D10_2_NO_SCRAMBLING:
	case PHY_TEST_PATTERN_SYMBOL_ERR_MEASUREMENT_CNT:
	case PHY_TEST_PATTERN_PRBS7:
	case PHY_TEST_PATTERN_80_BIT_CUSTOM_PATTERN:
	case PHY_TEST_PATTERN_HBR2_CTS_EYE_PATTERN:
		return true;
	default:
		return false;
	}
}

/**
 * dp_parse_phy_test_params() - parses the phy link parameters
 * @link: Display Port Driver data
 *
 * Parses the DPCD (Byte 0x248) for the DP PHY link pattern that is being
 * requested.
 */
static int dp_link_parse_phy_test_params(struct dp_link_private *link)
{
	u8 *bp;
	u8 data;
	int rlen;
	int const param_len = 0x1;
	int const phy_test_pattern_addr = 0x248;
	int ret = 0;

	rlen = drm_dp_dpcd_read(link->aux->drm_aux, phy_test_pattern_addr,
			&bp, param_len);
	if (rlen < param_len) {
		pr_err("failed to read phy link pattern\n");
		ret = -EINVAL;
		goto end;
	}

	data = *bp;

	link->request.phy_test_pattern_sel = data;

	pr_debug("phy_test_pattern_sel = %s\n",
			dp_link_get_phy_test_pattern(data));

	if (!dp_link_is_phy_test_pattern_supported(data))
		ret = -EINVAL;
end:
	return ret;
}

static char *dp_link_get_test_name(u32 test_requested)
{
	switch (test_requested) {
	case TEST_LINK_TRAINING: return DP_LINK_ENUM_STR(TEST_LINK_TRAINING);
	case TEST_VIDEO_PATTERN: return DP_LINK_ENUM_STR(TEST_VIDEO_PATTERN);
	case PHY_TEST_PATTERN:	 return DP_LINK_ENUM_STR(PHY_TEST_PATTERN);
	case TEST_EDID_READ:	 return DP_LINK_ENUM_STR(TEST_EDID_READ);
	case TEST_AUDIO_PATTERN: return DP_LINK_ENUM_STR(TEST_AUDIO_PATTERN);
	default:		 return "unknown";
	}
}

/**
 * dp_link_is_video_audio_test_requested() - checks for audio/video link request
 * @link: link requested by the sink
 *
 * Returns true if the requested link is a permitted audio/video link.
 */
static bool dp_link_is_video_audio_test_requested(u32 link)
{
	return (link == TEST_VIDEO_PATTERN) ||
		(link == (TEST_AUDIO_PATTERN | TEST_VIDEO_PATTERN)) ||
		(link == TEST_AUDIO_PATTERN) ||
		(link == (TEST_AUDIO_PATTERN | TEST_AUDIO_DISABLED_VIDEO));
}

/**
 * dp_link_supported() - checks if link requested by sink is supported
 * @test_requested: link requested by the sink
 *
 * Returns true if the requested link is supported.
 */
static bool dp_link_is_test_supported(u32 test_requested)
{
	return (test_requested == TEST_LINK_TRAINING) ||
		(test_requested == TEST_EDID_READ) ||
		(test_requested == PHY_TEST_PATTERN) ||
		dp_link_is_video_audio_test_requested(test_requested);
}

/**
 * dp_sink_parse_test_request() - parses link request parameters from sink
 * @link: Display Port Driver data
 *
 * Parses the DPCD to check if an automated link is requested (Byte 0x201),
 * and what type of link automation is being requested (Byte 0x218).
 */
static int dp_link_parse_request(struct dp_link_private *link)
{
	int ret = 0;
	u8 *bp;
	u8 data;
	int rlen;
	u32 const param_len = 0x1;
	u8 buf[4];

	/**
	 * Read the device service IRQ vector (Byte 0x201) to determine
	 * whether an automated link has been requested by the sink.
	 */
	rlen = drm_dp_dpcd_read(link->aux->drm_aux,
		DP_DEVICE_SERVICE_IRQ_VECTOR, &bp, param_len);
	if (rlen < param_len) {
		pr_err("aux read failed\n");
		ret = -EINVAL;
		goto end;
	}

	data = *bp;

	pr_debug("device service irq vector = 0x%x\n", data);

	if (!(data & BIT(1))) {
		pr_debug("no link requested\n");
		goto end;
	}

	/**
	 * Read the link request byte (Byte 0x218) to determine what type
	 * of automated link has been requested by the sink.
	 */
	rlen = drm_dp_dpcd_read(link->aux->drm_aux, DP_TEST_REQUEST,
			&bp, param_len);
	if (rlen < param_len) {
		pr_err("aux read failed\n");
		ret = -EINVAL;
		goto end;
	}

	data = *bp;

	if (!dp_link_is_test_supported(data)) {
		pr_debug("link 0x%x not supported\n", data);
		goto end;
	}

	pr_debug("%s (0x%x) requested\n", dp_link_get_test_name(data), data);
	link->request.test_requested = data;

	if (link->request.test_requested == PHY_TEST_PATTERN) {
		ret = dp_link_parse_phy_test_params(link);
		if (ret)
			goto end;
		ret = dp_link_parse_link_training_params(link);
	}

	if (link->request.test_requested == TEST_LINK_TRAINING)
		ret = dp_link_parse_link_training_params(link);

	if (dp_link_is_video_audio_test_requested(
			link->request.test_requested)) {
		ret = dp_link_parse_video_pattern_params(link);
		if (ret)
			goto end;

		ret = dp_link_parse_audio_pattern_params(link);
	}
end:
	/* clear the link request IRQ */
	buf[0] = 1;
	drm_dp_dpcd_write(link->aux->drm_aux, DP_TEST_REQUEST, buf, 1);

	/**
	 * Send a TEST_ACK if all link parameters are valid, otherwise send
	 * a TEST_NACK.
	 */
	if (ret)
		link->request.response = TEST_NACK;
	else
		link->request.response = TEST_ACK;

	return ret;
}

/**
 * dp_link_parse_sink_count() - parses the sink count
 *
 * Parses the DPCD to check if there is an update to the sink count
 * (Byte 0x200), and whether all the sink devices connected have Content
 * Protection enabled.
 */
static void dp_link_parse_sink_count(struct dp_link_private *link)
{
	u8 *bp;
	u8 data;
	int rlen;
	int const param_len = 0x1;

	rlen = drm_dp_dpcd_read(link->aux->drm_aux, DP_SINK_COUNT,
			&bp, param_len);
	if (rlen < param_len) {
		pr_err("failed to read sink count\n");
		return;
	}

	data = *bp;

	/* BIT 7, BIT 5:0 */
	link->sink_count.count = (data & BIT(7)) << 6 | (data & 0x63);
	/* BIT 6*/
	link->sink_count.cp_ready = data & BIT(6);

	pr_debug("sink_count = 0x%x, cp_ready = 0x%x\n",
		link->sink_count.count, link->sink_count.cp_ready);
}

static void dp_link_parse_sink_status_field(struct dp_link_private *link)
{
	int len = 0;

	dp_link_parse_sink_count(link);
	dp_link_parse_request(link);
	len = drm_dp_dpcd_read_link_status(link->aux->drm_aux,
		link->link_status);
	if (len < DP_LINK_STATUS_SIZE)
		pr_err("DP link status read failed\n");
}

static bool dp_link_is_link_training_requested(struct dp_link_private *link)
{
	return (link->request.test_requested == TEST_LINK_TRAINING);
}

/**
 * dp_link_process_link_training_request() - processes new training requests
 * @link: Display Port link data
 *
 * This function will handle new link training requests that are initiated by
 * the sink. In particular, it will update the requested lane count and link
 * link rate, and then trigger the link retraining procedure.
 *
 * The function will return 0 if a link training request has been processed,
 * otherwise it will return -EINVAL.
 */
static int dp_link_process_link_training_request(struct dp_link_private *link)
{
	if (!dp_link_is_link_training_requested(link))
		return -EINVAL;

	pr_debug("%s link rate = 0x%x, lane count = 0x%x\n",
			dp_link_get_test_name(TEST_LINK_TRAINING),
			link->request.test_link_rate,
			link->request.test_lane_count);

	link->dp_link.lane_count = link->request.test_lane_count;
	link->dp_link.link_rate = link->request.test_link_rate;

	return 0;
}

static bool dp_link_phy_pattern_requested(struct dp_link *dp_link)
{
	struct dp_link_private *link = container_of(dp_link,
			struct dp_link_private, dp_link);

	return (link->request.test_requested == PHY_TEST_PATTERN);
}

static int dp_link_parse_vx_px(struct dp_link_private *link)
{
	u8 *bp;
	u8 data;
	int const param_len = 0x1;
	int const addr1 = 0x206;
	int const addr2 = 0x207;
	int ret = 0;
	u32 v0, p0, v1, p1, v2, p2, v3, p3;
	int rlen;

	pr_debug("\n");

	rlen = drm_dp_dpcd_read(link->aux->drm_aux, addr1, &bp, param_len);
	if (rlen < param_len) {
		pr_err("failed reading lanes 0/1\n");
		ret = -EINVAL;
		goto end;
	}

	data = *bp;

	pr_debug("lanes 0/1 (Byte 0x206): 0x%x\n", data);

	v0 = data & 0x3;
	data = data >> 2;
	p0 = data & 0x3;
	data = data >> 2;

	v1 = data & 0x3;
	data = data >> 2;
	p1 = data & 0x3;
	data = data >> 2;

	rlen = drm_dp_dpcd_read(link->aux->drm_aux, addr2, &bp, param_len);
	if (rlen < param_len) {
		pr_err("failed reading lanes 2/3\n");
		ret = -EINVAL;
		goto end;
	}

	data = *bp;

	pr_debug("lanes 2/3 (Byte 0x207): 0x%x\n", data);

	v2 = data & 0x3;
	data = data >> 2;
	p2 = data & 0x3;
	data = data >> 2;

	v3 = data & 0x3;
	data = data >> 2;
	p3 = data & 0x3;
	data = data >> 2;

	pr_debug("vx: 0=%d, 1=%d, 2=%d, 3=%d\n", v0, v1, v2, v3);
	pr_debug("px: 0=%d, 1=%d, 2=%d, 3=%d\n", p0, p1, p2, p3);

	/**
	 * Update the voltage and pre-emphasis levels as per DPCD request
	 * vector.
	 */
	pr_debug("Current: v_level = 0x%x, p_level = 0x%x\n",
			link->dp_link.v_level, link->dp_link.p_level);
	pr_debug("Requested: v_level = 0x%x, p_level = 0x%x\n", v0, p0);
	link->dp_link.v_level = v0;
	link->dp_link.p_level = p0;

	pr_debug("Success\n");
end:
	return ret;
}

/**
 * dp_link_process_phy_test_pattern_request() - process new phy link requests
 * @link: Display Port Driver data
 *
 * This function will handle new phy link pattern requests that are initiated
 * by the sink. The function will return 0 if a phy link pattern has been
 * processed, otherwise it will return -EINVAL.
 */
static int dp_link_process_phy_test_pattern_request(
		struct dp_link_private *link)
{
	u32 test_link_rate = 0, test_lane_count = 0;

	if (!dp_link_phy_pattern_requested(&link->dp_link))
		return -EINVAL;

	test_link_rate = link->request.test_link_rate;
	test_lane_count = link->request.test_lane_count;

	if (!dp_link_is_link_rate_valid(test_link_rate) ||
		!dp_link_is_lane_count_valid(test_lane_count)) {
		pr_err("Invalid params: link rate = 0x%x, lane count = 0x%x\n",
				test_link_rate, test_lane_count);
		return -EINVAL;
	}

	pr_debug("start\n");

	link->dp_link.lane_count = link->request.test_lane_count;
	link->dp_link.link_rate = link->request.test_link_rate;

	dp_link_parse_vx_px(link);

	pr_debug("end\n");

	return 0;
}

/**
 * dp_link_process_link_status_update() - processes link status updates
 * @link: Display Port link module data
 *
 * This function will check for changes in the link status, e.g. clock
 * recovery done on all lanes, and trigger link training if there is a
 * failure/error on the link.
 *
 * The function will return 0 if the a link status update has been processed,
 * otherwise it will return -EINVAL.
 */
static int dp_link_process_link_status_update(struct dp_link_private *link)
{
	if (!(link->link_status[2] & BIT(7)) || /* link status updated */
		(drm_dp_clock_recovery_ok(link->link_status,
			link->dp_link.lane_count) &&
	     drm_dp_channel_eq_ok(link->link_status,
			link->dp_link.lane_count)))
		return -EINVAL;

	pr_debug("channel_eq_done = %d, clock_recovery_done = %d\n",
			drm_dp_clock_recovery_ok(link->link_status,
			link->dp_link.lane_count),
			drm_dp_clock_recovery_ok(link->link_status,
			link->dp_link.lane_count));

	return 0;
}

static bool dp_link_is_ds_port_status_changed(struct dp_link_private *link)
{
	return (link->link_status[2] & BIT(6)); /* port status changed */
}

/**
 * dp_link_process_downstream_port_status_change() - process port status changes
 * @link: Display Port Driver data
 *
 * This function will handle downstream port updates that are initiated by
 * the sink. If the downstream port status has changed, the EDID is read via
 * AUX.
 *
 * The function will return 0 if a downstream port update has been
 * processed, otherwise it will return -EINVAL.
 */
static int dp_link_process_ds_port_status_change(struct dp_link_private *link)
{
	if (!dp_link_is_ds_port_status_changed(link))
		return -EINVAL;

	return 0;
}

static bool dp_link_is_video_pattern_requested(struct dp_link_private *link)
{
	return (link->request.test_requested & TEST_VIDEO_PATTERN)
		&& !(link->request.test_requested & TEST_AUDIO_DISABLED_VIDEO);
}

static bool dp_link_is_audio_pattern_requested(struct dp_link_private *link)
{
	return (link->request.test_requested & TEST_AUDIO_PATTERN);
}

/**
 * dp_link_process_video_pattern_request() - process new video pattern request
 * @link: Display Port link module's data
 *
 * This function will handle a new video pattern request that are initiated by
 * the sink. This is acheieved by first sending a disconnect notification to
 * the sink followed by a subsequent connect notification to the user modules,
 * where it is expected that the user modules would draw the required link
 * pattern.
 */
static int dp_link_process_video_pattern_request(struct dp_link_private *link)
{
	if (!dp_link_is_video_pattern_requested(link))
		goto end;

	pr_debug("%s: bit depth=%d(%d bpp) pattern=%s\n",
		dp_link_get_test_name(TEST_VIDEO_PATTERN),
		link->request.test_bit_depth,
		dp_link_bit_depth_to_bpp(link->request.test_bit_depth),
		dp_link_video_pattern_to_string(
			link->request.test_video_pattern));

	return 0;
end:
	return -EINVAL;
}

/**
 * dp_link_process_audio_pattern_request() - process new audio pattern request
 * @link: Display Port link module data
 *
 * This function will handle a new audio pattern request that is initiated by
 * the sink. This is acheieved by sending the necessary secondary data packets
 * to the sink. It is expected that any simulatenous requests for video
 * patterns will be handled before the audio pattern is sent to the sink.
 */
static int dp_link_process_audio_pattern_request(struct dp_link_private *link)
{
	if (!dp_link_is_audio_pattern_requested(link))
		return -EINVAL;

	pr_debug("sampling_rate=%s, channel_count=%d, pattern_type=%s\n",
		dp_link_get_audio_sample_rate(
			link->request.test_audio_sampling_rate),
		link->request.test_audio_channel_count,
		dp_link_get_audio_test_pattern(
			link->request.test_audio_pattern_type));

	pr_debug("audio_period: ch1=0x%x, ch2=0x%x, ch3=0x%x, ch4=0x%x\n",
		link->request.test_audio_period_ch_1,
		link->request.test_audio_period_ch_2,
		link->request.test_audio_period_ch_3,
		link->request.test_audio_period_ch_4);

	pr_debug("audio_period: ch5=0x%x, ch6=0x%x, ch7=0x%x, ch8=0x%x\n",
		link->request.test_audio_period_ch_5,
		link->request.test_audio_period_ch_6,
		link->request.test_audio_period_ch_7,
		link->request.test_audio_period_ch_8);

	return 0;
}

static void dp_link_reset_data(struct dp_link_private *link)
{
	link->request = (const struct dp_link_request){ 0 };
	link->request.test_bit_depth = DP_TEST_BIT_DEPTH_UNKNOWN;

	link->dp_link.test_requested = 0;
}

/**
 * dp_link_process_request() - handle HPD IRQ transition to HIGH
 * @link: pointer to link module data
 *
 * This function will handle the HPD IRQ state transitions from LOW to HIGH
 * (including cases when there are back to back HPD IRQ HIGH) indicating
 * the start of a new link training request or sink status update.
 */
static int dp_link_process_request(struct dp_link *dp_link)
{
	int ret = 0;
	struct dp_link_private *link;

	if (!dp_link) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	link = container_of(dp_link, struct dp_link_private, dp_link);

	pr_debug("start\n");

	dp_link_reset_data(link);

	dp_link_parse_sink_status_field(link);

	ret = dp_link_process_link_training_request(link);
	if (!ret) {
		dp_link->test_requested |= TEST_LINK_TRAINING;
		goto exit;
	}

	ret = dp_link_process_phy_test_pattern_request(link);
	if (!ret) {
		dp_link->test_requested |= PHY_TEST_PATTERN;
		goto exit;
	}

	ret = dp_link_process_link_status_update(link);
	if (!ret) {
		dp_link->test_requested |= LINK_STATUS_UPDATED;
		goto exit;
	}

	ret = dp_link_process_ds_port_status_change(link);
	if (!ret) {
		dp_link->test_requested |= DS_PORT_STATUS_CHANGED;
		goto exit;
	}

	ret = dp_link_process_video_pattern_request(link);
	if (!ret) {
		dp_link->test_requested |= TEST_VIDEO_PATTERN;
		goto exit;
	}

	ret = dp_link_process_audio_pattern_request(link);
	if (!ret) {
		dp_link->test_requested |= TEST_AUDIO_PATTERN;
		goto exit;
	}

	pr_debug("done\n");
exit:
	return ret;
}

static int dp_link_get_colorimetry_config(struct dp_link *dp_link)
{
	u32 cc;
	enum dynamic_range dr;
	struct dp_link_private *link;

	if (!dp_link) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	link = container_of(dp_link, struct dp_link_private, dp_link);

	/* unless a video pattern CTS test is ongoing, use CEA_VESA */
	if (dp_link_is_video_pattern_requested(link))
		dr = link->request.test_dyn_range;
	else
		dr = DP_DYNAMIC_RANGE_RGB_VESA;

	/* Only RGB_VESA nd RGB_CEA supported for now */
	switch (dr) {
	case DP_DYNAMIC_RANGE_RGB_CEA:
		cc = BIT(3);
		break;
	case DP_DYNAMIC_RANGE_RGB_VESA:
	default:
		cc = 0;
	}

	return cc;
}

static int dp_link_adjust_levels(struct dp_link *dp_link, u8 *link_status)
{
	int i;
	int max = 0;
	u8 data;
	struct dp_link_private *link;

	if (!dp_link) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	link = container_of(dp_link, struct dp_link_private, dp_link);

	/* use the max level across lanes */
	for (i = 0; i < dp_link->lane_count; i++) {
		data = drm_dp_get_adjust_request_voltage(link_status, i);
		pr_debug("lane=%d req_voltage_swing=%d\n", i, data);
		if (max < data)
			max = data;
	}

	dp_link->v_level = max >> DP_TRAIN_VOLTAGE_SWING_SHIFT;

	/* use the max level across lanes */
	max = 0;
	for (i = 0; i < dp_link->lane_count; i++) {
		data = drm_dp_get_adjust_request_pre_emphasis(link_status, i);
		pr_debug("lane=%d req_pre_emphasis=%d\n", i, data);
		if (max < data)
			max = data;
	}

	dp_link->p_level = max >> DP_TRAIN_PRE_EMPHASIS_SHIFT;

	/**
	 * Adjust the voltage swing and pre-emphasis level combination to within
	 * the allowable range.
	 */
	if (dp_link->v_level > DP_LINK_VOLTAGE_MAX) {
		pr_debug("Requested vSwingLevel=%d, change to %d\n",
				dp_link->v_level, DP_LINK_VOLTAGE_MAX);
		dp_link->v_level = DP_LINK_VOLTAGE_MAX;
	}

	if (dp_link->p_level > DP_LINK_PRE_EMPHASIS_MAX) {
		pr_debug("Requested preEmphasisLevel=%d, change to %d\n",
				dp_link->p_level, DP_LINK_PRE_EMPHASIS_MAX);
		dp_link->p_level = DP_LINK_PRE_EMPHASIS_MAX;
	}

	if ((dp_link->p_level > DP_LINK_PRE_EMPHASIS_LEVEL_1)
			&& (dp_link->v_level == DP_LINK_VOLTAGE_LEVEL_2)) {
		pr_debug("Requested preEmphasisLevel=%d, change to %d\n",
				dp_link->p_level, DP_LINK_PRE_EMPHASIS_LEVEL_1);
		dp_link->p_level = DP_LINK_PRE_EMPHASIS_LEVEL_1;
	}

	pr_debug("v_level=%d, p_level=%d\n",
		dp_link->v_level, dp_link->p_level);

	return 0;
}

static int dp_link_send_psm_request(struct dp_link *dp_link, bool req)
{
	struct dp_link_private *link;

	if (!dp_link) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	link = container_of(dp_link, struct dp_link_private, dp_link);

	return 0;
}

static u32 dp_link_get_test_bits_depth(struct dp_link *dp_link, u32 bpp)
{
	enum test_bit_depth tbd;

	/*
	 * Few simplistic rules and assumptions made here:
	 *    1. Test bit depth is bit depth per color component
	 *    2. Assume 3 color components
	 */
	switch (bpp) {
	case 18:
		tbd = DP_TEST_BIT_DEPTH_6;
		break;
	case 24:
		tbd = DP_TEST_BIT_DEPTH_8;
		break;
	case 30:
		tbd = DP_TEST_BIT_DEPTH_10;
		break;
	default:
		tbd = DP_TEST_BIT_DEPTH_UNKNOWN;
		break;
	}

	return tbd;
}

struct dp_link *dp_link_get(struct device *dev, struct dp_aux *aux)
{
	int rc = 0;
	struct dp_link_private *link;
	struct dp_link *dp_link;

	if (!dev || !aux) {
		pr_err("invalid input\n");
		rc = -EINVAL;
		goto error;
	}

	link = devm_kzalloc(dev, sizeof(*link), GFP_KERNEL);
	if (!link) {
		rc = -EINVAL;
		goto error;
	}

	link->dev   = dev;
	link->aux   = aux;

	dp_link = &link->dp_link;

	dp_link->process_request        = dp_link_process_request;
	dp_link->get_test_bits_depth    = dp_link_get_test_bits_depth;
	dp_link->get_colorimetry_config = dp_link_get_colorimetry_config;
	dp_link->adjust_levels          = dp_link_adjust_levels;
	dp_link->send_psm_request       = dp_link_send_psm_request;
	dp_link->phy_pattern_requested  = dp_link_phy_pattern_requested;

	return dp_link;
error:
	return ERR_PTR(rc);
}

void dp_link_put(struct dp_link *dp_link)
{
	struct dp_link_private *link;

	if (!dp_link)
		return;

	link = container_of(dp_link, struct dp_link_private, dp_link);

	devm_kfree(link->dev, link);
}
