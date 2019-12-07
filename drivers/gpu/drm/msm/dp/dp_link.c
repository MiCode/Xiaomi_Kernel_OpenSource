/*
 * Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
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

enum dynamic_range {
	DP_DYNAMIC_RANGE_RGB_VESA = 0x00,
	DP_DYNAMIC_RANGE_RGB_CEA = 0x01,
	DP_DYNAMIC_RANGE_UNKNOWN = 0xFFFFFFFF,
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
};

struct dp_link_private {
	u32 prev_sink_count;
	struct device *dev;
	struct dp_aux *aux;
	struct dp_link dp_link;

	struct dp_link_request request;
	u8 link_status[DP_LINK_STATUS_SIZE];
};

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
	u8 bp;
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

	data = bp;

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
	struct dp_link_test_audio *req = &link->dp_link.test_audio;

	ret = dp_link_get_period(link, DP_TEST_AUDIO_PERIOD_CH1);
	if (ret == -EINVAL)
		goto exit;

	req->test_audio_period_ch_1 = ret;
	pr_debug("test_audio_period_ch_1 = 0x%x\n", ret);

	ret = dp_link_get_period(link, DP_TEST_AUDIO_PERIOD_CH2);
	if (ret == -EINVAL)
		goto exit;

	req->test_audio_period_ch_2 = ret;
	pr_debug("test_audio_period_ch_2 = 0x%x\n", ret);

	/* TEST_AUDIO_PERIOD_CH_3 (Byte 0x275) */
	ret = dp_link_get_period(link, DP_TEST_AUDIO_PERIOD_CH3);
	if (ret == -EINVAL)
		goto exit;

	req->test_audio_period_ch_3 = ret;
	pr_debug("test_audio_period_ch_3 = 0x%x\n", ret);

	ret = dp_link_get_period(link, DP_TEST_AUDIO_PERIOD_CH4);
	if (ret == -EINVAL)
		goto exit;

	req->test_audio_period_ch_4 = ret;
	pr_debug("test_audio_period_ch_4 = 0x%x\n", ret);

	ret = dp_link_get_period(link, DP_TEST_AUDIO_PERIOD_CH5);
	if (ret == -EINVAL)
		goto exit;

	req->test_audio_period_ch_5 = ret;
	pr_debug("test_audio_period_ch_5 = 0x%x\n", ret);

	ret = dp_link_get_period(link, DP_TEST_AUDIO_PERIOD_CH6);
	if (ret == -EINVAL)
		goto exit;

	req->test_audio_period_ch_6 = ret;
	pr_debug("test_audio_period_ch_6 = 0x%x\n", ret);

	ret = dp_link_get_period(link, DP_TEST_AUDIO_PERIOD_CH7);
	if (ret == -EINVAL)
		goto exit;

	req->test_audio_period_ch_7 = ret;
	pr_debug("test_audio_period_ch_7 = 0x%x\n", ret);

	ret = dp_link_get_period(link, DP_TEST_AUDIO_PERIOD_CH8);
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
	u8 bp;
	u8 data;
	int rlen;
	int const param_len = 0x1;
	int const max_audio_pattern_type = 0x1;

	rlen = drm_dp_dpcd_read(link->aux->drm_aux,
		DP_TEST_AUDIO_PATTERN_TYPE, &bp, param_len);
	if (rlen < param_len) {
		pr_err("failed to read link audio mode data\n");
		ret = -EINVAL;
		goto exit;
	}
	data = bp;

	/* Audio Pattern Type - Bits 7:0 */
	if ((int)data > max_audio_pattern_type) {
		pr_err("invalid audio pattern type = 0x%x\n", data);
		ret = -EINVAL;
		goto exit;
	}

	link->dp_link.test_audio.test_audio_pattern_type = data;
	pr_debug("audio pattern type = %s\n",
			dp_link_get_audio_test_pattern(data));
exit:
	return ret;
}

static int dp_link_parse_audio_mode(struct dp_link_private *link)
{
	int ret = 0;
	u8 bp;
	u8 data;
	int rlen;
	int const param_len = 0x1;
	int const max_audio_sampling_rate = 0x6;
	int const max_audio_channel_count = 0x8;
	int sampling_rate = 0x0;
	int channel_count = 0x0;

	rlen = drm_dp_dpcd_read(link->aux->drm_aux, DP_TEST_AUDIO_MODE,
			&bp, param_len);
	if (rlen < param_len) {
		pr_err("failed to read link audio mode data\n");
		ret = -EINVAL;
		goto exit;
	}
	data = bp;

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

	link->dp_link.test_audio.test_audio_sampling_rate = sampling_rate;
	link->dp_link.test_audio.test_audio_channel_count = channel_count;
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
	case DP_NO_TEST_PATTERN:
	case DP_COLOR_RAMP:
	case DP_BLACK_AND_WHITE_VERTICAL_LINES:
	case DP_COLOR_SQUARE:
		return true;
	default:
		return false;
	}
}

static char *dp_link_video_pattern_to_string(u32 test_video_pattern)
{
	switch (test_video_pattern) {
	case DP_NO_TEST_PATTERN:
		return DP_LINK_ENUM_STR(DP_NO_TEST_PATTERN);
	case DP_COLOR_RAMP:
		return DP_LINK_ENUM_STR(DP_COLOR_RAMP);
	case DP_BLACK_AND_WHITE_VERTICAL_LINES:
		return DP_LINK_ENUM_STR(DP_BLACK_AND_WHITE_VERTICAL_LINES);
	case DP_COLOR_SQUARE:
		return DP_LINK_ENUM_STR(DP_COLOR_SQUARE);
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
	u8 bp[2];
	int rlen;

	if (len < 2)
		return -EINVAL;

	/* Read the requested video link pattern (Byte 0x221). */
	rlen = drm_dp_dpcd_read(link->aux->drm_aux, addr, bp, len);
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
	u8 bp[2];
	int rlen;

	if (len < 2)
		return -EINVAL;

	/* Read the requested video link pattern (Byte 0x221). */
	rlen = drm_dp_dpcd_read(link->aux->drm_aux, addr, bp, len);
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
	u8 bp;
	u32 len = 1;
	int rlen;

	rlen = drm_dp_dpcd_read(link->aux->drm_aux, addr, &bp, len);
	if (rlen < 1) {
		pr_err("failed to read 0x%x\n", addr);
		return -EINVAL;
	}
	*val = bp;

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
	u8 bp;
	u8 data;
	u32 dyn_range;
	int const param_len = 0x1;

	rlen = drm_dp_dpcd_read(link->aux->drm_aux, DP_TEST_PATTERN,
			&bp, param_len);
	if (rlen < param_len) {
		pr_err("failed to read link video pattern\n");
		ret = -EINVAL;
		goto exit;
	}
	data = bp;

	if (!dp_link_is_video_pattern_valid(data)) {
		pr_err("invalid link video pattern = 0x%x\n", data);
		ret = -EINVAL;
		goto exit;
	}

	link->dp_link.test_video.test_video_pattern = data;
	pr_debug("link video pattern = 0x%x (%s)\n",
		link->dp_link.test_video.test_video_pattern,
		dp_link_video_pattern_to_string(
			link->dp_link.test_video.test_video_pattern));

	/* Read the requested color bit depth and dynamic range (Byte 0x232) */
	rlen = drm_dp_dpcd_read(link->aux->drm_aux, DP_TEST_MISC0,
			&bp, param_len);
	if (rlen < param_len) {
		pr_err("failed to read link bit depth\n");
		ret = -EINVAL;
		goto exit;
	}
	data = bp;

	/* Dynamic Range */
	dyn_range = (data & DP_TEST_DYNAMIC_RANGE_CEA) >> 3;
	if (!dp_link_is_dynamic_range_valid(dyn_range)) {
		pr_err("invalid link dynamic range = 0x%x", dyn_range);
		ret = -EINVAL;
		goto exit;
	}
	link->dp_link.test_video.test_dyn_range = dyn_range;
	pr_debug("link dynamic range = 0x%x (%s)\n",
		link->dp_link.test_video.test_dyn_range,
		dp_link_dynamic_range_to_string(
			link->dp_link.test_video.test_dyn_range));

	/* Color bit depth */
	data &= DP_TEST_BIT_DEPTH_MASK;
	if (!dp_link_is_bit_depth_valid(data)) {
		pr_err("invalid link bit depth = 0x%x\n", data);
		ret = -EINVAL;
		goto exit;
	}

	link->dp_link.test_video.test_bit_depth = data;
	pr_debug("link bit depth = 0x%x (%s)\n",
		link->dp_link.test_video.test_bit_depth,
		dp_link_bit_depth_to_string(
		link->dp_link.test_video.test_bit_depth));

	/* resolution timing params */
	ret = dp_link_parse_timing_params1(link, DP_TEST_H_TOTAL_HI, 2,
			&link->dp_link.test_video.test_h_total);
	if (ret) {
		pr_err("failed to parse test_h_total (DP_TEST_H_TOTAL_HI)\n");
		goto exit;
	}
	pr_debug("TEST_H_TOTAL = %d\n", link->dp_link.test_video.test_h_total);

	ret = dp_link_parse_timing_params1(link, DP_TEST_V_TOTAL_HI, 2,
			&link->dp_link.test_video.test_v_total);
	if (ret) {
		pr_err("failed to parse test_v_total (DP_TEST_V_TOTAL_HI)\n");
		goto exit;
	}
	pr_debug("TEST_V_TOTAL = %d\n", link->dp_link.test_video.test_v_total);

	ret = dp_link_parse_timing_params1(link, DP_TEST_H_START_HI, 2,
			&link->dp_link.test_video.test_h_start);
	if (ret) {
		pr_err("failed to parse test_h_start (DP_TEST_H_START_HI)\n");
		goto exit;
	}
	pr_debug("TEST_H_START = %d\n", link->dp_link.test_video.test_h_start);

	ret = dp_link_parse_timing_params1(link, DP_TEST_V_START_HI, 2,
			&link->dp_link.test_video.test_v_start);
	if (ret) {
		pr_err("failed to parse test_v_start (DP_TEST_V_START_HI)\n");
		goto exit;
	}
	pr_debug("TEST_V_START = %d\n", link->dp_link.test_video.test_v_start);

	ret = dp_link_parse_timing_params2(link, DP_TEST_HSYNC_HI, 2,
			&link->dp_link.test_video.test_hsync_pol,
			&link->dp_link.test_video.test_hsync_width);
	if (ret) {
		pr_err("failed to parse (DP_TEST_HSYNC_HI)\n");
		goto exit;
	}
	pr_debug("TEST_HSYNC_POL = %d\n",
		link->dp_link.test_video.test_hsync_pol);
	pr_debug("TEST_HSYNC_WIDTH = %d\n",
		link->dp_link.test_video.test_hsync_width);

	ret = dp_link_parse_timing_params2(link, DP_TEST_VSYNC_HI, 2,
			&link->dp_link.test_video.test_vsync_pol,
			&link->dp_link.test_video.test_vsync_width);
	if (ret) {
		pr_err("failed to parse (DP_TEST_VSYNC_HI)\n");
		goto exit;
	}
	pr_debug("TEST_VSYNC_POL = %d\n",
		link->dp_link.test_video.test_vsync_pol);
	pr_debug("TEST_VSYNC_WIDTH = %d\n",
		link->dp_link.test_video.test_vsync_width);

	ret = dp_link_parse_timing_params1(link, DP_TEST_H_WIDTH_HI, 2,
			&link->dp_link.test_video.test_h_width);
	if (ret) {
		pr_err("failed to parse test_h_width (DP_TEST_H_WIDTH_HI)\n");
		goto exit;
	}
	pr_debug("TEST_H_WIDTH = %d\n", link->dp_link.test_video.test_h_width);

	ret = dp_link_parse_timing_params1(link, DP_TEST_V_HEIGHT_HI, 2,
			&link->dp_link.test_video.test_v_height);
	if (ret) {
		pr_err("failed to parse test_v_height (DP_TEST_V_HEIGHT_HI)\n");
		goto exit;
	}
	pr_debug("TEST_V_HEIGHT = %d\n",
		link->dp_link.test_video.test_v_height);

	ret = dp_link_parse_timing_params3(link, DP_TEST_MISC1,
		&link->dp_link.test_video.test_rr_d);
	link->dp_link.test_video.test_rr_d &= DP_TEST_REFRESH_DENOMINATOR;
	if (ret) {
		pr_err("failed to parse test_rr_d (DP_TEST_MISC1)\n");
		goto exit;
	}
	pr_debug("TEST_REFRESH_DENOMINATOR = %d\n",
		link->dp_link.test_video.test_rr_d);

	ret = dp_link_parse_timing_params3(link, DP_TEST_REFRESH_RATE_NUMERATOR,
		&link->dp_link.test_video.test_rr_n);
	if (ret) {
		pr_err("failed to parse test_rr_n (DP_TEST_REFRESH_RATE_NUMERATOR)\n");
		goto exit;
	}
	pr_debug("TEST_REFRESH_NUMERATOR = %d\n",
		link->dp_link.test_video.test_rr_n);
exit:
	return ret;
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
	u8 bp;
	u8 data;
	int ret = 0;
	int rlen;
	int const param_len = 0x1;

	rlen = drm_dp_dpcd_read(link->aux->drm_aux, DP_TEST_LINK_RATE,
			&bp, param_len);
	if (rlen < param_len) {
		pr_err("failed to read link rate\n");
		ret = -EINVAL;
		goto exit;
	}
	data = bp;

	if (!is_link_rate_valid(data)) {
		pr_err("invalid link rate = 0x%x\n", data);
		ret = -EINVAL;
		goto exit;
	}

	link->request.test_link_rate = data;
	pr_debug("link rate = 0x%x\n", link->request.test_link_rate);

	rlen = drm_dp_dpcd_read(link->aux->drm_aux, DP_TEST_LANE_COUNT,
			&bp, param_len);
	if (rlen < param_len) {
		pr_err("failed to read lane count\n");
		ret = -EINVAL;
		goto exit;
	}
	data = bp;
	data &= 0x1F;

	if (!is_lane_count_valid(data)) {
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
	case DP_TEST_PHY_PATTERN_NONE:
	case DP_TEST_PHY_PATTERN_D10_2_NO_SCRAMBLING:
	case DP_TEST_PHY_PATTERN_SYMBOL_ERR_MEASUREMENT_CNT:
	case DP_TEST_PHY_PATTERN_PRBS7:
	case DP_TEST_PHY_PATTERN_80_BIT_CUSTOM_PATTERN:
	case DP_TEST_PHY_PATTERN_CP2520_PATTERN_1:
	case DP_TEST_PHY_PATTERN_CP2520_PATTERN_3:
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
	u8 bp;
	u8 data;
	int rlen;
	int const param_len = 0x1;
	int ret = 0;

	rlen = drm_dp_dpcd_read(link->aux->drm_aux, DP_TEST_PHY_PATTERN,
			&bp, param_len);
	if (rlen < param_len) {
		pr_err("failed to read phy link pattern\n");
		ret = -EINVAL;
		goto end;
	}

	data = bp;

	link->dp_link.phy_params.phy_test_pattern_sel = data;

	pr_debug("phy_test_pattern_sel = %s\n",
			dp_link_get_phy_test_pattern(data));

	if (!dp_link_is_phy_test_pattern_supported(data))
		ret = -EINVAL;
end:
	return ret;
}

/**
 * dp_link_is_video_audio_test_requested() - checks for audio/video link request
 * @link: link requested by the sink
 *
 * Returns true if the requested link is a permitted audio/video link.
 */
static bool dp_link_is_video_audio_test_requested(u32 link)
{
	return (link == DP_TEST_LINK_VIDEO_PATTERN) ||
		(link == (DP_TEST_LINK_AUDIO_PATTERN |
		DP_TEST_LINK_VIDEO_PATTERN)) ||
		(link == DP_TEST_LINK_AUDIO_PATTERN) ||
		(link == (DP_TEST_LINK_AUDIO_PATTERN |
		DP_TEST_LINK_AUDIO_DISABLED_VIDEO));
}

/**
 * dp_link_supported() - checks if link requested by sink is supported
 * @test_requested: link requested by the sink
 *
 * Returns true if the requested link is supported.
 */
static bool dp_link_is_test_supported(u32 test_requested)
{
	return (test_requested == DP_TEST_LINK_TRAINING) ||
		(test_requested == DP_TEST_LINK_EDID_READ) ||
		(test_requested == DP_TEST_LINK_PHY_TEST_PATTERN) ||
		dp_link_is_video_audio_test_requested(test_requested);
}

static bool dp_link_is_test_edid_read(struct dp_link_private *link)
{
	return (link->request.test_requested == DP_TEST_LINK_EDID_READ);
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
	u8 bp;
	u8 data;
	int rlen;
	u32 const param_len = 0x1;

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

	data = bp;

	if (!(data & DP_AUTOMATED_TEST_REQUEST))
		return 0;

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

	data = bp;

	if (!dp_link_is_test_supported(data)) {
		pr_debug("link 0x%x not supported\n", data);
		goto end;
	}

	link->request.test_requested = data;

	if (link->request.test_requested == DP_TEST_LINK_PHY_TEST_PATTERN) {
		ret = dp_link_parse_phy_test_params(link);
		if (ret)
			goto end;
		ret = dp_link_parse_link_training_params(link);
	}

	if (link->request.test_requested == DP_TEST_LINK_TRAINING)
		ret = dp_link_parse_link_training_params(link);

	if (dp_link_is_video_audio_test_requested(
			link->request.test_requested)) {
		ret = dp_link_parse_video_pattern_params(link);
		if (ret)
			goto end;

		ret = dp_link_parse_audio_pattern_params(link);
	}
end:
	/**
	 * Send a DP_TEST_ACK if all link parameters are valid, otherwise send
	 * a DP_TEST_NAK.
	 */
	if (ret) {
		link->dp_link.test_response = DP_TEST_NAK;
	} else {
		if (!dp_link_is_test_edid_read(link))
			link->dp_link.test_response = DP_TEST_ACK;
		else
			link->dp_link.test_response =
				DP_TEST_EDID_CHECKSUM_WRITE;
	}

	return ret;
}

/**
 * dp_link_parse_sink_count() - parses the sink count
 *
 * Parses the DPCD to check if there is an update to the sink count
 * (Byte 0x200), and whether all the sink devices connected have Content
 * Protection enabled.
 */
static int dp_link_parse_sink_count(struct dp_link *dp_link)
{
	int rlen;
	int const param_len = 0x1;
	struct dp_link_private *link = container_of(dp_link,
			struct dp_link_private, dp_link);

	rlen = drm_dp_dpcd_read(link->aux->drm_aux, DP_SINK_COUNT,
			&link->dp_link.sink_count.count, param_len);
	if (rlen < param_len) {
		pr_err("failed to read sink count\n");
		return -EINVAL;
	}

	link->dp_link.sink_count.cp_ready =
		link->dp_link.sink_count.count & DP_SINK_CP_READY;
	/* BIT 7, BIT 5:0 */
	link->dp_link.sink_count.count =
		DP_GET_SINK_COUNT(link->dp_link.sink_count.count);

	pr_debug("sink_count = 0x%x, cp_ready = 0x%x\n",
		link->dp_link.sink_count.count,
		link->dp_link.sink_count.cp_ready);
	return 0;
}

static void dp_link_parse_sink_status_field(struct dp_link_private *link)
{
	int len = 0;

	link->prev_sink_count = link->dp_link.sink_count.count;
	dp_link_parse_sink_count(&link->dp_link);

	len = drm_dp_dpcd_read_link_status(link->aux->drm_aux,
		link->link_status);
	if (len < DP_LINK_STATUS_SIZE)
		pr_err("DP link status read failed\n");
	dp_link_parse_request(link);
}

static bool dp_link_is_link_training_requested(struct dp_link_private *link)
{
	return (link->request.test_requested == DP_TEST_LINK_TRAINING);
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
			dp_link_get_test_name(DP_TEST_LINK_TRAINING),
			link->request.test_link_rate,
			link->request.test_lane_count);

	link->dp_link.link_params.lane_count = link->request.test_lane_count;
	link->dp_link.link_params.bw_code = link->request.test_link_rate;

	return 0;
}

static void dp_link_send_test_response(struct dp_link *dp_link)
{
	struct dp_link_private *link = NULL;
	u32 const response_len = 0x1;

	if (!dp_link) {
		pr_err("invalid input\n");
		return;
	}

	link = container_of(dp_link, struct dp_link_private, dp_link);

	drm_dp_dpcd_write(link->aux->drm_aux, DP_TEST_RESPONSE,
			&dp_link->test_response, response_len);
}

static int dp_link_psm_config(struct dp_link *dp_link,
	struct drm_dp_link *link_info, bool enable)
{
	struct dp_link_private *link = NULL;
	int ret = 0;

	if (!dp_link) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	link = container_of(dp_link, struct dp_link_private, dp_link);

	if (enable)
		ret = drm_dp_link_power_down(link->aux->drm_aux, link_info);
	else
		ret = drm_dp_link_power_up(link->aux->drm_aux, link_info);

	if (ret)
		pr_err("Failed to %s low power mode\n",
			(enable ? "enter" : "exit"));

	return ret;
}

static void dp_link_send_edid_checksum(struct dp_link *dp_link, u8 checksum)
{
	struct dp_link_private *link = NULL;
	u32 const response_len = 0x1;

	if (!dp_link) {
		pr_err("invalid input\n");
		return;
	}

	link = container_of(dp_link, struct dp_link_private, dp_link);

	drm_dp_dpcd_write(link->aux->drm_aux, DP_TEST_EDID_CHECKSUM,
			&checksum, response_len);
}

static int dp_link_parse_vx_px(struct dp_link_private *link)
{
	u8 bp;
	u8 data;
	int const param_len = 0x1;
	int ret = 0;
	u32 v0, p0, v1, p1, v2, p2, v3, p3;
	int rlen;

	pr_debug("\n");

	rlen = drm_dp_dpcd_read(link->aux->drm_aux, DP_ADJUST_REQUEST_LANE0_1,
			&bp, param_len);
	if (rlen < param_len) {
		pr_err("failed reading lanes 0/1\n");
		ret = -EINVAL;
		goto end;
	}

	data = bp;

	pr_debug("lanes 0/1 (Byte 0x206): 0x%x\n", data);

	v0 = data & 0x3;
	data = data >> 2;
	p0 = data & 0x3;
	data = data >> 2;

	v1 = data & 0x3;
	data = data >> 2;
	p1 = data & 0x3;
	data = data >> 2;

	rlen = drm_dp_dpcd_read(link->aux->drm_aux, DP_ADJUST_REQUEST_LANE2_3,
			&bp, param_len);
	if (rlen < param_len) {
		pr_err("failed reading lanes 2/3\n");
		ret = -EINVAL;
		goto end;
	}

	data = bp;

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
			link->dp_link.phy_params.v_level,
			link->dp_link.phy_params.p_level);
	pr_debug("Requested: v_level = 0x%x, p_level = 0x%x\n", v0, p0);
	link->dp_link.phy_params.v_level = v0;
	link->dp_link.phy_params.p_level = p0;

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

	if (!(link->request.test_requested & DP_TEST_LINK_PHY_TEST_PATTERN)) {
		pr_debug("no phy test\n");
		return -EINVAL;
	}

	test_link_rate = link->request.test_link_rate;
	test_lane_count = link->request.test_lane_count;

	if (!is_link_rate_valid(test_link_rate) ||
		!is_lane_count_valid(test_lane_count)) {
		pr_err("Invalid params: link rate = 0x%x, lane count = 0x%x\n",
				test_link_rate, test_lane_count);
		return -EINVAL;
	}

	pr_debug("start\n");

	pr_info("Current: bw_code = 0x%x, lane count = 0x%x\n",
			link->dp_link.link_params.bw_code,
			link->dp_link.link_params.lane_count);

	pr_info("Requested: bw_code = 0x%x, lane count = 0x%x\n",
			test_link_rate, test_lane_count);

	link->dp_link.link_params.lane_count = link->request.test_lane_count;
	link->dp_link.link_params.bw_code = link->request.test_link_rate;

	dp_link_parse_vx_px(link);

	pr_debug("end\n");

	return 0;
}

static u8 get_link_status(const u8 link_status[DP_LINK_STATUS_SIZE], int r)
{
	return link_status[r - DP_LANE0_1_STATUS];
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
	if (!(get_link_status(link->link_status, DP_LANE_ALIGN_STATUS_UPDATED) &
		DP_LINK_STATUS_UPDATED) || /* link status updated */
		(drm_dp_clock_recovery_ok(link->link_status,
			link->dp_link.link_params.lane_count) &&
	     drm_dp_channel_eq_ok(link->link_status,
			link->dp_link.link_params.lane_count)))
		return -EINVAL;

	pr_debug("channel_eq_done = %d, clock_recovery_done = %d\n",
			drm_dp_channel_eq_ok(link->link_status,
			link->dp_link.link_params.lane_count),
			drm_dp_clock_recovery_ok(link->link_status,
			link->dp_link.link_params.lane_count));

	return 0;
}

static bool dp_link_is_ds_port_status_changed(struct dp_link_private *link)
{
	if (get_link_status(link->link_status, DP_LANE_ALIGN_STATUS_UPDATED) &
		DP_DOWNSTREAM_PORT_STATUS_CHANGED) /* port status changed */
		return true;

	if (link->prev_sink_count != link->dp_link.sink_count.count)
		return true;

	return false;
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

	/* reset prev_sink_count */
	link->prev_sink_count = link->dp_link.sink_count.count;

	return 0;
}

static bool dp_link_is_video_pattern_requested(struct dp_link_private *link)
{
	return (link->request.test_requested & DP_TEST_LINK_VIDEO_PATTERN)
		&& !(link->request.test_requested &
		DP_TEST_LINK_AUDIO_DISABLED_VIDEO);
}

static bool dp_link_is_audio_pattern_requested(struct dp_link_private *link)
{
	return (link->request.test_requested & DP_TEST_LINK_AUDIO_PATTERN);
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
		dp_link_get_test_name(DP_TEST_LINK_VIDEO_PATTERN),
		link->dp_link.test_video.test_bit_depth,
		dp_link_bit_depth_to_bpp(
		link->dp_link.test_video.test_bit_depth),
		dp_link_video_pattern_to_string(
			link->dp_link.test_video.test_video_pattern));

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
			link->dp_link.test_audio.test_audio_sampling_rate),
		link->dp_link.test_audio.test_audio_channel_count,
		dp_link_get_audio_test_pattern(
			link->dp_link.test_audio.test_audio_pattern_type));

	pr_debug("audio_period: ch1=0x%x, ch2=0x%x, ch3=0x%x, ch4=0x%x\n",
		link->dp_link.test_audio.test_audio_period_ch_1,
		link->dp_link.test_audio.test_audio_period_ch_2,
		link->dp_link.test_audio.test_audio_period_ch_3,
		link->dp_link.test_audio.test_audio_period_ch_4);

	pr_debug("audio_period: ch5=0x%x, ch6=0x%x, ch7=0x%x, ch8=0x%x\n",
		link->dp_link.test_audio.test_audio_period_ch_5,
		link->dp_link.test_audio.test_audio_period_ch_6,
		link->dp_link.test_audio.test_audio_period_ch_7,
		link->dp_link.test_audio.test_audio_period_ch_8);

	return 0;
}

static void dp_link_reset_data(struct dp_link_private *link)
{
	link->request = (const struct dp_link_request){ 0 };
	link->dp_link.test_video = (const struct dp_link_test_video){ 0 };
	link->dp_link.test_video.test_bit_depth = DP_TEST_BIT_DEPTH_UNKNOWN;
	link->dp_link.test_audio = (const struct dp_link_test_audio){ 0 };
	link->dp_link.phy_params.phy_test_pattern_sel = 0;
	link->dp_link.sink_request = 0;
	link->dp_link.test_response = 0;
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

	dp_link_reset_data(link);

	dp_link_parse_sink_status_field(link);

	if (dp_link_is_test_edid_read(link)) {
		dp_link->sink_request |= DP_TEST_LINK_EDID_READ;
		goto exit;
	}

	ret = dp_link_process_ds_port_status_change(link);
	if (!ret) {
		dp_link->sink_request |= DS_PORT_STATUS_CHANGED;
		goto exit;
	}

	ret = dp_link_process_link_training_request(link);
	if (!ret) {
		dp_link->sink_request |= DP_TEST_LINK_TRAINING;
		goto exit;
	}

	ret = dp_link_process_phy_test_pattern_request(link);
	if (!ret) {
		dp_link->sink_request |= DP_TEST_LINK_PHY_TEST_PATTERN;
		goto exit;
	}

	ret = dp_link_process_link_status_update(link);
	if (!ret) {
		dp_link->sink_request |= DP_LINK_STATUS_UPDATED;
		goto exit;
	}

	ret = dp_link_process_video_pattern_request(link);
	if (!ret) {
		dp_link->sink_request |= DP_TEST_LINK_VIDEO_PATTERN;
		goto exit;
	}

	ret = dp_link_process_audio_pattern_request(link);
	if (!ret) {
		dp_link->sink_request |= DP_TEST_LINK_AUDIO_PATTERN;
		goto exit;
	}

	pr_debug("no test requested\n");
	return ret;
exit:
	/*
	 * log this as it can be a use initiated action to run a DP CTS
	 * test or in normal cases, sink has encountered a problem and
	 * and want source to redo some part of initialization which can
	 * be helpful in debugging.
	 */
	pr_info("test requested: %s\n",
		dp_link_get_test_name(dp_link->sink_request));
	return 0;
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
		dr = link->dp_link.test_video.test_dyn_range;
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
	u8 buf[8] = {0}, offset = 0;

	if (!dp_link) {
		pr_err("invalid input\n");
		return -EINVAL;
	}

	link = container_of(dp_link, struct dp_link_private, dp_link);

	/* use the max level across lanes */
	for (i = 0; i < dp_link->link_params.lane_count; i++) {
		data = drm_dp_get_adjust_request_voltage(link_status, i);
		data >>= DP_TRAIN_VOLTAGE_SWING_SHIFT;

		offset = i * 2;
		if (offset < sizeof(buf))
			buf[offset] = data;

		if (max < data)
			max = data;
	}

	dp_link->phy_params.v_level = max;

	/* use the max level across lanes */
	max = 0;
	for (i = 0; i < dp_link->link_params.lane_count; i++) {
		data = drm_dp_get_adjust_request_pre_emphasis(link_status, i);
		data >>= DP_TRAIN_PRE_EMPHASIS_SHIFT;

		offset = (i * 2) + 1;
		if (offset < sizeof(buf))
			buf[offset] = data;

		if (max < data)
			max = data;
	}

	dp_link->phy_params.p_level = max;

	print_hex_dump(KERN_DEBUG, "[drm-dp] Req (VxPx): ",
		DUMP_PREFIX_NONE, 8, 2, buf, sizeof(buf), false);

	/**
	 * Adjust the voltage swing and pre-emphasis level combination to within
	 * the allowable range.
	 */
	if (dp_link->phy_params.v_level > DP_LINK_VOLTAGE_MAX)
		dp_link->phy_params.v_level = DP_LINK_VOLTAGE_MAX;

	if (dp_link->phy_params.p_level > DP_LINK_PRE_EMPHASIS_MAX)
		dp_link->phy_params.p_level = DP_LINK_PRE_EMPHASIS_MAX;

	if ((dp_link->phy_params.p_level > DP_LINK_PRE_EMPHASIS_LEVEL_1)
		&& (dp_link->phy_params.v_level == DP_LINK_VOLTAGE_LEVEL_2))
		dp_link->phy_params.p_level = DP_LINK_PRE_EMPHASIS_LEVEL_1;

	pr_debug("Set (VxPx): %x%x\n",
		dp_link->phy_params.v_level, dp_link->phy_params.p_level);

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
	u32 tbd;

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

	if (tbd != DP_TEST_BIT_DEPTH_UNKNOWN)
		tbd = (tbd >> DP_TEST_BIT_DEPTH_SHIFT);

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
	dp_link->send_test_response     = dp_link_send_test_response;
	dp_link->psm_config             = dp_link_psm_config;
	dp_link->send_edid_checksum     = dp_link_send_edid_checksum;

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
