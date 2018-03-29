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
#ifndef _MTK_HDMI_HW_H
#define _MTK_HDMI_HW_H

#include <linux/types.h>
#include <linux/hdmi.h>
#include "mtk_hdmi.h"

void mtk_hdmi_hw_vid_black(struct mtk_hdmi *hdmi, bool black);
void mtk_hdmi_hw_aud_mute(struct mtk_hdmi *hdmi, bool mute);
void mtk_hdmi_hw_send_info_frame(struct mtk_hdmi *hdmi, u8 *buffer, u8 len);
void mtk_hdmi_hw_send_aud_packet(struct mtk_hdmi *hdmi, bool enable);
int mtk_hdmi_hw_set_clock(struct mtk_hdmi *hctx, u32 clock);
void mtk_hdmi_hw_config_sys(struct mtk_hdmi *hdmi);
void mtk_hdmi_hw_set_deep_color_mode(struct mtk_hdmi *hdmi);
void mtk_hdmi_hw_send_av_mute(struct mtk_hdmi *hdmi);
void mtk_hdmi_hw_send_av_unmute(struct mtk_hdmi *hdmi);
void mtk_hdmi_hw_ncts_enable(struct mtk_hdmi *hdmi, bool on);
void mtk_hdmi_hw_aud_set_channel_swap(struct mtk_hdmi *hdmi,
				      enum hdmi_aud_channel_swap_type swap);
void mtk_hdmi_hw_aud_raw_data_enable(struct mtk_hdmi *hdmi, bool enable);
void mtk_hdmi_hw_aud_set_bit_num(struct mtk_hdmi *hdmi,
				 enum hdmi_audio_sample_size bit_num);
void mtk_hdmi_hw_aud_set_high_bitrate(struct mtk_hdmi *hdmi, bool enable);
void mtk_hdmi_phy_aud_dst_normal_double_enable(struct mtk_hdmi *hdmi,
					       bool enable);
void mtk_hdmi_hw_aud_dst_enable(struct mtk_hdmi *hdmi, bool enable);
void mtk_hdmi_hw_aud_dsd_enable(struct mtk_hdmi *hdmi, bool enable);
void mtk_hdmi_hw_aud_set_i2s_fmt(struct mtk_hdmi *hdmi,
				 enum hdmi_aud_i2s_fmt i2s_fmt);
void mtk_hdmi_hw_aud_set_i2s_chan_num(struct mtk_hdmi *hdmi,
				      enum hdmi_aud_channel_type channel_type,
				      u8 channel_count);
void mtk_hdmi_hw_aud_set_input_type(struct mtk_hdmi *hdmi,
				    enum hdmi_aud_input_type input_type);
void mtk_hdmi_hw_aud_set_channel_status(struct mtk_hdmi *hdmi,
					u8 *l_chan_status, u8 *r_chan_staus,
					enum hdmi_audio_sample_frequency
					aud_hdmi_fs);
void mtk_hdmi_hw_aud_src_enable(struct mtk_hdmi *hdmi, bool enable);
void mtk_hdmi_hw_aud_set_mclk(struct mtk_hdmi *hdmi, enum hdmi_aud_mclk mclk);
void mtk_hdmi_hw_aud_src_off(struct mtk_hdmi *hdmi);
void mtk_hdmi_hw_aud_src_reenable(struct mtk_hdmi *hdmi);
void mtk_hdmi_hw_aud_aclk_inv_enable(struct mtk_hdmi *hdmi, bool enable);
void mtk_hdmi_hw_aud_set_ncts(struct mtk_hdmi *hdmi,
			      enum hdmi_audio_sample_frequency freq,
			      int clock);
bool mtk_hdmi_hw_is_hpd_high(struct mtk_hdmi *hdmi);
void mtk_hdmi_hw_make_reg_writable(struct mtk_hdmi *hdmi, bool enable);
void mtk_hdmi_hw_reset(struct mtk_hdmi *hdmi);
void mtk_hdmi_hw_enable_notice(struct mtk_hdmi *hdmi, bool enable_notice);
void mtk_hdmi_hw_write_int_mask(struct mtk_hdmi *hdmi, u32 int_mask);
void mtk_hdmi_hw_enable_dvi_mode(struct mtk_hdmi *hdmi, bool enable);
void mtk_hdmi_hw_ncts_auto_write_enable(struct mtk_hdmi *hdmi, bool enable);
void mtk_hdmi_hw_msic_setting(struct mtk_hdmi *hdmi,
			      struct drm_display_mode *mode);
void mtk_hdmi_hw_1p4_version_enable(struct mtk_hdmi *hdmi, bool enable);
void mtk_hdmi_hw_htplg_irq_enable(struct mtk_hdmi *hdmi);
void mtk_hdmi_hw_htplg_irq_disable(struct mtk_hdmi *hdmi);
void mtk_hdmi_hw_clear_htplg_irq(struct mtk_hdmi *hdmi);

#endif /* _MTK_HDMI_HW_H */
