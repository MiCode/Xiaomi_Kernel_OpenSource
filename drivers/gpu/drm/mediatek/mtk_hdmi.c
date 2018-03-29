/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Jie Qiu <jie.qiu@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <drm/drm_edid.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include "mtk_cec.h"
#include "mtk_hdmi.h"
#include "mtk_hdmi_hw.h"

static u8 mtk_hdmi_aud_get_chnl_count(enum hdmi_aud_channel_type channel_type)
{
	switch (channel_type) {
	case HDMI_AUD_CHAN_TYPE_1_0:
	case HDMI_AUD_CHAN_TYPE_1_1:
	case HDMI_AUD_CHAN_TYPE_2_0:
		return 2;
	case HDMI_AUD_CHAN_TYPE_2_1:
	case HDMI_AUD_CHAN_TYPE_3_0:
		return 3;
	case HDMI_AUD_CHAN_TYPE_3_1:
	case HDMI_AUD_CHAN_TYPE_4_0:
	case HDMI_AUD_CHAN_TYPE_3_0_LRS:
		return 4;
	case HDMI_AUD_CHAN_TYPE_4_1:
	case HDMI_AUD_CHAN_TYPE_5_0:
	case HDMI_AUD_CHAN_TYPE_3_1_LRS:
	case HDMI_AUD_CHAN_TYPE_4_0_CLRS:
		return 5;
	case HDMI_AUD_CHAN_TYPE_5_1:
	case HDMI_AUD_CHAN_TYPE_6_0:
	case HDMI_AUD_CHAN_TYPE_4_1_CLRS:
	case HDMI_AUD_CHAN_TYPE_6_0_CS:
	case HDMI_AUD_CHAN_TYPE_6_0_CH:
	case HDMI_AUD_CHAN_TYPE_6_0_OH:
	case HDMI_AUD_CHAN_TYPE_6_0_CHR:
		return 6;
	case HDMI_AUD_CHAN_TYPE_6_1:
	case HDMI_AUD_CHAN_TYPE_6_1_CS:
	case HDMI_AUD_CHAN_TYPE_6_1_CH:
	case HDMI_AUD_CHAN_TYPE_6_1_OH:
	case HDMI_AUD_CHAN_TYPE_6_1_CHR:
	case HDMI_AUD_CHAN_TYPE_7_0:
	case HDMI_AUD_CHAN_TYPE_7_0_LH_RH:
	case HDMI_AUD_CHAN_TYPE_7_0_LSR_RSR:
	case HDMI_AUD_CHAN_TYPE_7_0_LC_RC:
	case HDMI_AUD_CHAN_TYPE_7_0_LW_RW:
	case HDMI_AUD_CHAN_TYPE_7_0_LSD_RSD:
	case HDMI_AUD_CHAN_TYPE_7_0_LSS_RSS:
	case HDMI_AUD_CHAN_TYPE_7_0_LHS_RHS:
	case HDMI_AUD_CHAN_TYPE_7_0_CS_CH:
	case HDMI_AUD_CHAN_TYPE_7_0_CS_OH:
	case HDMI_AUD_CHAN_TYPE_7_0_CS_CHR:
	case HDMI_AUD_CHAN_TYPE_7_0_CH_OH:
	case HDMI_AUD_CHAN_TYPE_7_0_CH_CHR:
	case HDMI_AUD_CHAN_TYPE_7_0_OH_CHR:
	case HDMI_AUD_CHAN_TYPE_7_0_LSS_RSS_LSR_RSR:
	case HDMI_AUD_CHAN_TYPE_8_0_LH_RH_CS:
		return 7;
	case HDMI_AUD_CHAN_TYPE_7_1:
	case HDMI_AUD_CHAN_TYPE_7_1_LH_RH:
	case HDMI_AUD_CHAN_TYPE_7_1_LSR_RSR:
	case HDMI_AUD_CHAN_TYPE_7_1_LC_RC:
	case HDMI_AUD_CHAN_TYPE_7_1_LW_RW:
	case HDMI_AUD_CHAN_TYPE_7_1_LSD_RSD:
	case HDMI_AUD_CHAN_TYPE_7_1_LSS_RSS:
	case HDMI_AUD_CHAN_TYPE_7_1_LHS_RHS:
	case HDMI_AUD_CHAN_TYPE_7_1_CS_CH:
	case HDMI_AUD_CHAN_TYPE_7_1_CS_OH:
	case HDMI_AUD_CHAN_TYPE_7_1_CS_CHR:
	case HDMI_AUD_CHAN_TYPE_7_1_CH_OH:
	case HDMI_AUD_CHAN_TYPE_7_1_CH_CHR:
	case HDMI_AUD_CHAN_TYPE_7_1_OH_CHR:
	case HDMI_AUD_CHAN_TYPE_7_1_LSS_RSS_LSR_RSR:
		return 8;
	default:
		return 2;
	}
}

static int mtk_hdmi_video_change_vpll(struct mtk_hdmi *hdmi, u32 clock)
{
	unsigned long rate;
	int ret;

	/* The DPI driver already should have set TVDPLL to the correct rate */
	ret = clk_set_rate(hdmi->clk[MTK_HDMI_CLK_HDMI_PLL], clock);
	if (ret) {
		dev_err(hdmi->dev, "Failed to set PLL to %u Hz: %d\n", clock,
			ret);
		return ret;
	}

	rate = clk_get_rate(hdmi->clk[MTK_HDMI_CLK_HDMI_PLL]);

	if (DIV_ROUND_CLOSEST(rate, 1000) != DIV_ROUND_CLOSEST(clock, 1000))
		dev_warn(hdmi->dev, "Want PLL %u Hz, got %lu Hz\n", clock,
			 rate);
	else
		dev_dbg(hdmi->dev, "Want PLL %u Hz, got %lu Hz\n", clock, rate);

	mtk_hdmi_hw_config_sys(hdmi);
	mtk_hdmi_hw_set_deep_color_mode(hdmi);
	return 0;
}

static void mtk_hdmi_video_set_display_mode(struct mtk_hdmi *hdmi,
					    struct drm_display_mode *mode)
{
	mtk_hdmi_hw_reset(hdmi);
	mtk_hdmi_hw_enable_notice(hdmi, true);
	mtk_hdmi_hw_write_int_mask(hdmi, 0xff);
	mtk_hdmi_hw_enable_dvi_mode(hdmi, hdmi->dvi_mode);
	mtk_hdmi_hw_ncts_auto_write_enable(hdmi, true);

	mtk_hdmi_hw_msic_setting(hdmi, mode);
}

static int mtk_hdmi_aud_enable_packet(struct mtk_hdmi *hdmi, bool enable)
{
	mtk_hdmi_hw_send_aud_packet(hdmi, enable);
	return 0;
}

static int mtk_hdmi_aud_on_off_hw_ncts(struct mtk_hdmi *hdmi, bool on)
{
	mtk_hdmi_hw_ncts_enable(hdmi, on);
	return 0;
}

static int mtk_hdmi_aud_set_input(struct mtk_hdmi *hdmi)
{
	u8 chan_count;

	mtk_hdmi_hw_aud_set_channel_swap(hdmi, HDMI_AUD_SWAP_LFE_CC);
	mtk_hdmi_hw_aud_raw_data_enable(hdmi, true);

	if (hdmi->aud_param.aud_input_type == HDMI_AUD_INPUT_SPDIF &&
	    hdmi->aud_param.aud_codec == HDMI_AUDIO_CODING_TYPE_DST) {
		mtk_hdmi_hw_aud_set_bit_num(hdmi,
					    HDMI_AUDIO_SAMPLE_SIZE_24);
	} else if (hdmi->aud_param.aud_i2s_fmt ==
			HDMI_I2S_MODE_LJT_24BIT) {
		hdmi->aud_param.aud_i2s_fmt = HDMI_I2S_MODE_LJT_16BIT;
	}

	mtk_hdmi_hw_aud_set_i2s_fmt(hdmi,
				    hdmi->aud_param.aud_i2s_fmt);
	mtk_hdmi_hw_aud_set_bit_num(hdmi, HDMI_AUDIO_SAMPLE_SIZE_24);

	mtk_hdmi_hw_aud_set_high_bitrate(hdmi, false);
	mtk_hdmi_phy_aud_dst_normal_double_enable(hdmi, false);
	mtk_hdmi_hw_aud_dst_enable(hdmi, false);

	if (hdmi->aud_param.aud_input_type == HDMI_AUD_INPUT_SPDIF) {
		mtk_hdmi_hw_aud_dsd_enable(hdmi, false);
		if (hdmi->aud_param.aud_codec ==
			HDMI_AUDIO_CODING_TYPE_DST) {
			mtk_hdmi_phy_aud_dst_normal_double_enable(hdmi,
								  true);
			mtk_hdmi_hw_aud_dst_enable(hdmi, true);
		}

		chan_count = mtk_hdmi_aud_get_chnl_count
						 (HDMI_AUD_CHAN_TYPE_2_0);
		mtk_hdmi_hw_aud_set_i2s_chan_num(hdmi,
						 HDMI_AUD_CHAN_TYPE_2_0,
						 chan_count);
		mtk_hdmi_hw_aud_set_input_type(hdmi,
					       HDMI_AUD_INPUT_SPDIF);
	} else {
		mtk_hdmi_hw_aud_dsd_enable(hdmi, false);
		chan_count =
			mtk_hdmi_aud_get_chnl_count(
			hdmi->aud_param.aud_input_chan_type);
		mtk_hdmi_hw_aud_set_i2s_chan_num(
			hdmi,
			hdmi->aud_param.aud_input_chan_type,
			chan_count);
		mtk_hdmi_hw_aud_set_input_type(hdmi,
					       HDMI_AUD_INPUT_I2S);
	}
	return 0;
}

static int mtk_hdmi_aud_set_src(struct mtk_hdmi *hdmi,
				struct drm_display_mode *display_mode)
{
	mtk_hdmi_aud_on_off_hw_ncts(hdmi, false);

	if (hdmi->aud_param.aud_input_type == HDMI_AUD_INPUT_I2S) {
		switch (hdmi->aud_param.aud_hdmi_fs) {
		case HDMI_AUDIO_SAMPLE_FREQUENCY_32000:
		case HDMI_AUDIO_SAMPLE_FREQUENCY_44100:
		case HDMI_AUDIO_SAMPLE_FREQUENCY_48000:
		case HDMI_AUDIO_SAMPLE_FREQUENCY_88200:
		case HDMI_AUDIO_SAMPLE_FREQUENCY_96000:
			mtk_hdmi_hw_aud_src_off(hdmi);
			/* mtk_hdmi_hw_aud_src_enable(hdmi, false); */
			mtk_hdmi_hw_aud_set_mclk(
			hdmi,
			hdmi->aud_param.aud_mclk);
			mtk_hdmi_hw_aud_aclk_inv_enable(hdmi, false);
			break;
		default:
			break;
		}
	} else {
		switch (hdmi->aud_param.iec_frame_fs) {
		case HDMI_IEC_32K:
			hdmi->aud_param.aud_hdmi_fs =
			    HDMI_AUDIO_SAMPLE_FREQUENCY_32000;
			mtk_hdmi_hw_aud_src_off(hdmi);
			mtk_hdmi_hw_aud_set_mclk(hdmi,
						 HDMI_AUD_MCLK_128FS);
			mtk_hdmi_hw_aud_aclk_inv_enable(hdmi, false);
			break;
		case HDMI_IEC_48K:
			hdmi->aud_param.aud_hdmi_fs =
			    HDMI_AUDIO_SAMPLE_FREQUENCY_48000;
			mtk_hdmi_hw_aud_src_off(hdmi);
			mtk_hdmi_hw_aud_set_mclk(hdmi,
						 HDMI_AUD_MCLK_128FS);
			mtk_hdmi_hw_aud_aclk_inv_enable(hdmi, false);
			break;
		case HDMI_IEC_44K:
			hdmi->aud_param.aud_hdmi_fs =
			    HDMI_AUDIO_SAMPLE_FREQUENCY_44100;
			mtk_hdmi_hw_aud_src_off(hdmi);
			mtk_hdmi_hw_aud_set_mclk(hdmi,
						 HDMI_AUD_MCLK_128FS);
			mtk_hdmi_hw_aud_aclk_inv_enable(hdmi, false);
			break;
		default:
			break;
		}
	}
	mtk_hdmi_hw_aud_set_ncts(hdmi, hdmi->aud_param.aud_hdmi_fs,
				 display_mode->clock);

	mtk_hdmi_hw_aud_src_reenable(hdmi);
	return 0;
}

static int mtk_hdmi_aud_set_chnl_status(struct mtk_hdmi *hdmi)
{
	mtk_hdmi_hw_aud_set_channel_status(
		hdmi,
	   hdmi->aud_param.hdmi_l_channel_state,
	   hdmi->aud_param.hdmi_r_channel_state,
	   hdmi->aud_param.aud_hdmi_fs);
	return 0;
}

static int mtk_hdmi_aud_output_config(struct mtk_hdmi *hdmi,
				      struct drm_display_mode *display_mode)
{
	mtk_hdmi_hw_aud_mute(hdmi, true);
	mtk_hdmi_aud_enable_packet(hdmi, false);

	mtk_hdmi_aud_set_input(hdmi);
	mtk_hdmi_aud_set_src(hdmi, display_mode);
	mtk_hdmi_aud_set_chnl_status(hdmi);

	usleep_range(50, 100);

	mtk_hdmi_aud_on_off_hw_ncts(hdmi, true);
	mtk_hdmi_aud_enable_packet(hdmi, true);
	mtk_hdmi_hw_aud_mute(hdmi, false);
	return 0;
}

static int mtk_hdmi_setup_av_mute_packet(struct mtk_hdmi *hdmi)
{
	mtk_hdmi_hw_send_av_mute(hdmi);
	return 0;
}

static int mtk_hdmi_setup_av_unmute_packet(struct mtk_hdmi *hdmi)
{
	mtk_hdmi_hw_send_av_unmute(hdmi);
	return 0;
}

static int mtk_hdmi_setup_avi_infoframe(struct mtk_hdmi *hdmi,
					struct drm_display_mode *mode)
{
	struct hdmi_avi_infoframe frame;
	u8 buffer[17];
	ssize_t err;

	err = drm_hdmi_avi_infoframe_from_display_mode(&frame, mode);
	if (err < 0) {
		dev_err(hdmi->dev,
			"Failed to get AVI infoframe from mode: %zd\n", err);
		return err;
	}

	err = hdmi_avi_infoframe_pack(&frame, buffer, sizeof(buffer));
	if (err < 0) {
		dev_err(hdmi->dev, "Failed to pack AVI infoframe: %zd\n", err);
		return err;
	}

	mtk_hdmi_hw_send_info_frame(hdmi, buffer, sizeof(buffer));
	return 0;
}

static int mtk_hdmi_setup_spd_infoframe(struct mtk_hdmi *hdmi,
					const char *vendor,
					const char *product)
{
	struct hdmi_spd_infoframe frame;
	u8 buffer[29];
	ssize_t err;

	err = hdmi_spd_infoframe_init(&frame, vendor, product);
	if (err < 0) {
		dev_err(hdmi->dev, "Failed to initialize SPD infoframe %zd\n",
			     err);
		return err;
	}

	err = hdmi_spd_infoframe_pack(&frame, buffer, sizeof(buffer));
	if (err < 0) {
		dev_err(hdmi->dev, "Failed to pack SDP infoframe: %zd\n", err);
		return err;
	}

	mtk_hdmi_hw_send_info_frame(hdmi, buffer, sizeof(buffer));
	return 0;
}

static int mtk_hdmi_setup_audio_infoframe(struct mtk_hdmi *hdmi)
{
	struct hdmi_audio_infoframe frame;
	u8 buffer[14];
	ssize_t err;

	err = hdmi_audio_infoframe_init(&frame);
	if (err < 0) {
		dev_err(hdmi->dev, "Faied to setup audio infoframe: %zd\n",
			err);
		return err;
	}

	frame.coding_type = HDMI_AUDIO_CODING_TYPE_STREAM;
	frame.sample_frequency = HDMI_AUDIO_SAMPLE_FREQUENCY_STREAM;
	frame.sample_size = HDMI_AUDIO_SAMPLE_SIZE_STREAM;
	frame.channels =
	    mtk_hdmi_aud_get_chnl_count(
	    hdmi->aud_param.aud_input_chan_type);

	err = hdmi_audio_infoframe_pack(&frame, buffer, sizeof(buffer));
	if (err < 0) {
		dev_err(hdmi->dev, "Failed to pack audio infoframe: %zd\n",
			err);
		return err;
	}

	mtk_hdmi_hw_send_info_frame(hdmi, buffer, sizeof(buffer));
	return 0;
}

static int mtk_hdmi_setup_vendor_specific_infoframe(struct mtk_hdmi *hdmi,
						struct drm_display_mode *mode)
{
	struct hdmi_vendor_infoframe frame;
	u8 buffer[10];
	ssize_t err;

	err = drm_hdmi_vendor_infoframe_from_display_mode(&frame, mode);
	if (err) {
		dev_err(hdmi->dev,
			"Failed to get vendor infoframe from mode: %zd\n", err);
		return err;
	}

	err = hdmi_vendor_infoframe_pack(&frame, buffer, sizeof(buffer));
	if (err) {
		dev_err(hdmi->dev, "Failed to pack vendor infoframe: %zd\n",
			err);
		return err;
	}

	mtk_hdmi_hw_send_info_frame(hdmi, buffer, sizeof(buffer));
	return 0;
}

int mtk_hdmi_hpd_high(struct mtk_hdmi *hdmi)
{
	return hdmi->cec_dev ? mtk_cec_hpd_high(hdmi->cec_dev) : false;
}

int mtk_hdmi_output_init(struct mtk_hdmi *hdmi)
{
	struct hdmi_audio_param *aud_param = &hdmi->aud_param;

	if (hdmi->init)
		return -EINVAL;

	hdmi->csp = HDMI_COLORSPACE_RGB;
	hdmi->output = true;
	aud_param->aud_codec = HDMI_AUDIO_CODING_TYPE_PCM;
	aud_param->aud_hdmi_fs = HDMI_AUDIO_SAMPLE_FREQUENCY_48000;
	aud_param->aud_sampe_size = HDMI_AUDIO_SAMPLE_SIZE_16;
	aud_param->aud_input_type = HDMI_AUD_INPUT_I2S;
	aud_param->aud_i2s_fmt = HDMI_I2S_MODE_I2S_24BIT;
	aud_param->aud_mclk = HDMI_AUD_MCLK_128FS;
	aud_param->iec_frame_fs = HDMI_IEC_48K;
	aud_param->aud_input_chan_type = HDMI_AUD_CHAN_TYPE_2_0;
	aud_param->hdmi_l_channel_state[2] = 2;
	aud_param->hdmi_r_channel_state[2] = 2;
	hdmi->init = true;

	return 0;
}

void mtk_hdmi_power_on(struct mtk_hdmi *hdmi)
{
	mtk_hdmi_hw_make_reg_writable(hdmi, true);
	mtk_hdmi_hw_1p4_version_enable(hdmi, true);
}

void mtk_hdmi_power_off(struct mtk_hdmi *hdmi)
{
	mtk_hdmi_hw_1p4_version_enable(hdmi, true);
	mtk_hdmi_hw_make_reg_writable(hdmi, false);
}

void mtk_hdmi_audio_enable(struct mtk_hdmi *hdmi)
{
	mtk_hdmi_aud_enable_packet(hdmi, true);
	hdmi->audio_enable = true;
}

void mtk_hdmi_audio_disable(struct mtk_hdmi *hdmi)
{
	mtk_hdmi_aud_enable_packet(hdmi, false);
	hdmi->audio_enable = false;
}

int mtk_hdmi_audio_set_param(struct mtk_hdmi *hdmi,
			     struct hdmi_audio_param *param)
{
	if (!hdmi->audio_enable) {
		dev_err(hdmi->dev, "hdmi audio is in disable state!\n");
		return -EINVAL;
	}
	dev_info(hdmi->dev, "codec:%d, input:%d, channel:%d, fs:%d\n",
		 param->aud_codec, param->aud_input_type,
		 param->aud_input_chan_type, param->aud_hdmi_fs);
	memcpy(&hdmi->aud_param, param, sizeof(*param));
	return mtk_hdmi_aud_output_config(hdmi, &hdmi->mode);
}

int mtk_hdmi_detect_dvi_monitor(struct mtk_hdmi *hdmi)
{
	return hdmi->dvi_mode;
}

int mtk_hdmi_output_set_display_mode(struct mtk_hdmi *hdmi,
				     struct drm_display_mode *mode)
{
	int ret;

	if (!hdmi->init) {
		dev_err(hdmi->dev, "doesn't init hdmi control context!\n");
		return -EINVAL;
	}

	mtk_hdmi_hw_vid_black(hdmi, true);
	mtk_hdmi_hw_aud_mute(hdmi, true);
	mtk_hdmi_setup_av_mute_packet(hdmi);

	ret = mtk_hdmi_video_change_vpll(hdmi,
					 mode->clock * 1000);
	if (ret) {
		dev_err(hdmi->dev, "set vpll failed!\n");
		return ret;
	}
	mtk_hdmi_video_set_display_mode(hdmi, mode);
	mtk_hdmi_aud_output_config(hdmi, mode);
	mtk_hdmi_setup_audio_infoframe(hdmi);
	mtk_hdmi_setup_avi_infoframe(hdmi, mode);
	mtk_hdmi_setup_spd_infoframe(hdmi, "mediatek", "chromebook");
	if (mode->flags & DRM_MODE_FLAG_3D_MASK)
		mtk_hdmi_setup_vendor_specific_infoframe(hdmi, mode);

	mtk_hdmi_hw_vid_black(hdmi, false);
	mtk_hdmi_hw_aud_mute(hdmi, false);
	mtk_hdmi_setup_av_unmute_packet(hdmi);

	return 0;
}
