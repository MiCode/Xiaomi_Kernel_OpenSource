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
#include "mtk_hdmi_hw.h"
#include "mtk_hdmi_regs.h"
#include "mtk_hdmi.h"

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/hdmi.h>
#include <linux/io.h>
#include <linux/psci.h>
#include <linux/regmap.h>

static int (*invoke_psci_fn)(u64, u64, u64, u64);
typedef int (*psci_initcall_t)(const struct device_node *);

asmlinkage int __invoke_psci_fn_hvc(u64, u64, u64, u64);
asmlinkage int __invoke_psci_fn_smc(u64, u64, u64, u64);

static u32 mtk_hdmi_read(struct mtk_hdmi *hdmi, u32 offset)
{
	return readl(hdmi->regs + offset);
}

static void mtk_hdmi_write(struct mtk_hdmi *hdmi, u32 offset, u32 val)
{
	writel(val, hdmi->regs + offset);
}

static void mtk_hdmi_mask(struct mtk_hdmi *hdmi, u32 offset, u32 val, u32 mask)
{
	u32 tmp = mtk_hdmi_read(hdmi, offset) & ~mask;

	tmp |= (val & mask);
	mtk_hdmi_write(hdmi, offset, tmp);
}

static const u8 PREDIV[3][4] = {
	{0x0, 0x0, 0x0, 0x0},	/* 27Mhz */
	{0x1, 0x1, 0x1, 0x1},	/* 74Mhz */
	{0x1, 0x1, 0x1, 0x1}	/* 148Mhz */
};

static const u8 TXDIV[3][4] = {
	{0x3, 0x3, 0x3, 0x2},	/* 27Mhz */
	{0x2, 0x1, 0x1, 0x1},	/* 74Mhz */
	{0x1, 0x0, 0x0, 0x0}	/* 148Mhz */
};

static const u8 FBKSEL[3][4] = {
	{0x1, 0x1, 0x1, 0x1},	/* 27Mhz */
	{0x1, 0x0, 0x1, 0x1},	/* 74Mhz */
	{0x1, 0x0, 0x1, 0x1}	/* 148Mhz */
};

static const u8 FBKDIV[3][4] = {
	{19, 24, 29, 19},	/* 27Mhz */
	{19, 24, 14, 19},	/* 74Mhz */
	{19, 24, 14, 19}	/* 148Mhz */
};

static const u8 DIVEN[3][4] = {
	{0x2, 0x1, 0x1, 0x2},	/* 27Mhz */
	{0x2, 0x2, 0x2, 0x2},	/* 74Mhz */
	{0x2, 0x2, 0x2, 0x2}	/* 148Mhz */
};

static const u8 HTPLLBP[3][4] = {
	{0xc, 0xc, 0x8, 0xc},	/* 27Mhz */
	{0xc, 0xf, 0xf, 0xc},	/* 74Mhz */
	{0xc, 0xf, 0xf, 0xc}	/* 148Mhz */
};

static const u8 HTPLLBC[3][4] = {
	{0x2, 0x3, 0x3, 0x2},	/* 27Mhz */
	{0x2, 0x3, 0x3, 0x2},	/* 74Mhz */
	{0x2, 0x3, 0x3, 0x2}	/* 148Mhz */
};

static const u8 HTPLLBR[3][4] = {
	{0x1, 0x1, 0x0, 0x1},	/* 27Mhz */
	{0x1, 0x2, 0x2, 0x1},	/* 74Mhz */
	{0x1, 0x2, 0x2, 0x1}	/* 148Mhz */
};

#define NCTS_BYTES          0x07
static const u8 HDMI_NCTS[7][9][NCTS_BYTES] = {
	{{0x00, 0x00, 0x69, 0x78, 0x00, 0x10, 0x00},
	 {0x00, 0x00, 0xd2, 0xf0, 0x00, 0x10, 0x00},
	 {0x00, 0x03, 0x37, 0xf9, 0x00, 0x2d, 0x80},
	 {0x00, 0x01, 0x22, 0x0a, 0x00, 0x10, 0x00},
	 {0x00, 0x06, 0x6f, 0xf3, 0x00, 0x2d, 0x80},
	 {0x00, 0x02, 0x44, 0x14, 0x00, 0x10, 0x00},
	 {0x00, 0x01, 0xA5, 0xe0, 0x00, 0x10, 0x00},
	 {0x00, 0x06, 0x6F, 0xF3, 0x00, 0x16, 0xC0},
	 {0x00, 0x03, 0x66, 0x1E, 0x00, 0x0C, 0x00}
	 },
	{{0x00, 0x00, 0x75, 0x30, 0x00, 0x18, 0x80},
	 {0x00, 0x00, 0xea, 0x60, 0x00, 0x18, 0x80},
	 {0x00, 0x03, 0x93, 0x87, 0x00, 0x45, 0xac},
	 {0x00, 0x01, 0x42, 0x44, 0x00, 0x18, 0x80},
	 {0x00, 0x03, 0x93, 0x87, 0x00, 0x22, 0xd6},
	 {0x00, 0x02, 0x84, 0x88, 0x00, 0x18, 0x80},
	 {0x00, 0x01, 0xd4, 0xc0, 0x00, 0x18, 0x80},
	 {0x00, 0x03, 0x93, 0x87, 0x00, 0x11, 0x6B},
	 {0x00, 0x03, 0xC6, 0xCC, 0x00, 0x12, 0x60}
	 },
	{{0x00, 0x00, 0x69, 0x78, 0x00, 0x18, 0x00},
	 {0x00, 0x00, 0xd2, 0xf0, 0x00, 0x18, 0x00},
	 {0x00, 0x02, 0x25, 0x51, 0x00, 0x2d, 0x80},
	 {0x00, 0x01, 0x22, 0x0a, 0x00, 0x18, 0x00},
	 {0x00, 0x02, 0x25, 0x51, 0x00, 0x16, 0xc0},
	 {0x00, 0x02, 0x44, 0x14, 0x00, 0x18, 0x00},
	 {0x00, 0x01, 0xA5, 0xe0, 0x00, 0x18, 0x00},
	 {0x00, 0x04, 0x4A, 0xA2, 0x00, 0x16, 0xC0},
	 {0x00, 0x03, 0xC6, 0xCC, 0x00, 0x14, 0x00}
	 },
	{{0x00, 0x00, 0x75, 0x30, 0x00, 0x31, 0x00},
	 {0x00, 0x00, 0xea, 0x60, 0x00, 0x31, 0x00},
	 {0x00, 0x03, 0x93, 0x87, 0x00, 0x8b, 0x58},
	 {0x00, 0x01, 0x42, 0x44, 0x00, 0x31, 0x00},
	 {0x00, 0x03, 0x93, 0x87, 0x00, 0x45, 0xac},
	 {0x00, 0x02, 0x84, 0x88, 0x00, 0x31, 0x00},
	 {0x00, 0x01, 0xd4, 0xc0, 0x00, 0x31, 0x00},
	 {0x00, 0x03, 0x93, 0x87, 0x00, 0x22, 0xD6},
	 {0x00, 0x03, 0xC6, 0xCC, 0x00, 0x24, 0xC0}
	 },
	{{0x00, 0x00, 0x69, 0x78, 0x00, 0x30, 0x00},
	 {0x00, 0x00, 0xd2, 0xf0, 0x00, 0x30, 0x00},
	 {0x00, 0x02, 0x25, 0x51, 0x00, 0x5b, 0x00},
	 {0x00, 0x01, 0x22, 0x0a, 0x00, 0x30, 0x00},
	 {0x00, 0x02, 0x25, 0x51, 0x00, 0x2d, 0x80},
	 {0x00, 0x02, 0x44, 0x14, 0x00, 0x30, 0x00},
	 {0x00, 0x01, 0xA5, 0xe0, 0x00, 0x30, 0x00},
	 {0x00, 0x04, 0x4A, 0xA2, 0x00, 0x2D, 0x80},
	 {0x00, 0x03, 0xC6, 0xCC, 0x00, 0x28, 0x80}
	 },
	{{0x00, 0x00, 0x75, 0x30, 0x00, 0x62, 0x00},
	 {0x00, 0x00, 0xea, 0x60, 0x00, 0x62, 0x00},
	 {0x00, 0x03, 0x93, 0x87, 0x01, 0x16, 0xb0},
	 {0x00, 0x01, 0x42, 0x44, 0x00, 0x62, 0x00},
	 {0x00, 0x03, 0x93, 0x87, 0x00, 0x8b, 0x58},
	 {0x00, 0x02, 0x84, 0x88, 0x00, 0x62, 0x00},
	 {0x00, 0x01, 0xd4, 0xc0, 0x00, 0x62, 0x00},
	 {0x00, 0x03, 0x93, 0x87, 0x00, 0x45, 0xAC},
	 {0x00, 0x03, 0xC6, 0xCC, 0x00, 0x49, 0x80}
	 },
	{{0x00, 0x00, 0x69, 0x78, 0x00, 0x60, 0x00},
	 {0x00, 0x00, 0xd2, 0xf0, 0x00, 0x60, 0x00},
	 {0x00, 0x02, 0x25, 0x51, 0x00, 0xb6, 0x00},
	 {0x00, 0x01, 0x22, 0x0a, 0x00, 0x60, 0x00},
	 {0x00, 0x02, 0x25, 0x51, 0x00, 0x5b, 0x00},
	 {0x00, 0x02, 0x44, 0x14, 0x00, 0x60, 0x00},
	 {0x00, 0x01, 0xA5, 0xe0, 0x00, 0x60, 0x00},
	 {0x00, 0x04, 0x4A, 0xA2, 0x00, 0x5B, 0x00},
	 {0x00, 0x03, 0xC6, 0xCC, 0x00, 0x50, 0x00}
	 }
};

void mtk_hdmi_hw_vid_black(struct mtk_hdmi *hdmi,
			   bool black)
{
	mtk_hdmi_mask(hdmi, VIDEO_CFG_4, black ? GEN_RGB : NORMAL_PATH,
		      VIDEO_SOURCE_SEL);
}

void mtk_hdmi_hw_make_reg_writable(struct mtk_hdmi *hdmi, bool enable)
{
	invoke_psci_fn = __invoke_psci_fn_smc;
	invoke_psci_fn(MTK_SIP_SET_AUTHORIZED_SECURE_REG,
		       0x14000904, 0x80000000, 0);

	regmap_update_bits(hdmi->sys_regmap, hdmi->sys_offset + HDMI_SYS_CFG20,
			   HDMI_PCLK_FREE_RUN, enable ? HDMI_PCLK_FREE_RUN : 0);
	regmap_update_bits(hdmi->sys_regmap, hdmi->sys_offset + HDMI_SYS_CFG1C,
			   HDMI_ON | ANLG_ON, enable ? (HDMI_ON | ANLG_ON) : 0);
}

void mtk_hdmi_hw_1p4_version_enable(struct mtk_hdmi *hdmi, bool enable)
{
	regmap_update_bits(hdmi->sys_regmap, hdmi->sys_offset + HDMI_SYS_CFG20,
			   HDMI2P0_EN, enable ? 0 : HDMI2P0_EN);
}

void mtk_hdmi_hw_aud_mute(struct mtk_hdmi *hdmi, bool mute)
{
	mtk_hdmi_mask(hdmi, GRL_AUDIO_CFG, mute ? AUDIO_ZERO : 0, AUDIO_ZERO);
}

void mtk_hdmi_hw_reset(struct mtk_hdmi *hdmi)
{
	regmap_update_bits(hdmi->sys_regmap, hdmi->sys_offset + HDMI_SYS_CFG1C,
			   HDMI_RST, HDMI_RST);
	regmap_update_bits(hdmi->sys_regmap, hdmi->sys_offset + HDMI_SYS_CFG1C,
			   HDMI_RST, 0);
	mtk_hdmi_mask(hdmi, GRL_CFG3, 0, CFG3_CONTROL_PACKET_DELAY);
	regmap_update_bits(hdmi->sys_regmap, hdmi->sys_offset + HDMI_SYS_CFG1C,
			   ANLG_ON, ANLG_ON);
}

void mtk_hdmi_hw_enable_notice(struct mtk_hdmi *hdmi, bool enable_notice)
{
	mtk_hdmi_mask(hdmi, GRL_CFG2, enable_notice ? CFG2_NOTICE_EN : 0,
		      CFG2_NOTICE_EN);
}

void mtk_hdmi_hw_write_int_mask(struct mtk_hdmi *hdmi, u32 int_mask)
{
	mtk_hdmi_write(hdmi, GRL_INT_MASK, int_mask);
}

void mtk_hdmi_hw_enable_dvi_mode(struct mtk_hdmi *hdmi, bool enable)
{
	mtk_hdmi_mask(hdmi, GRL_CFG1, enable ? CFG1_DVI : 0, CFG1_DVI);
}

void mtk_hdmi_hw_send_info_frame(struct mtk_hdmi *hdmi, u8 *buffer, u8 len)
{
	u32 ctrl_reg = GRL_CTRL;
	int i;
	u8 *frame_data;
	u8 frame_type;
	u8 frame_ver;
	u8 frame_len;
	u8 checksum;
	int ctrl_frame_en = 0;

	frame_type = *buffer;
	buffer += 1;
	frame_ver = *buffer;
	buffer += 1;
	frame_len = *buffer;
	buffer += 1;
	checksum = *buffer;
	buffer += 1;
	frame_data = buffer;

	dev_info(hdmi->dev,
		 "frame_type:0x%x,frame_ver:0x%x,frame_len:0x%x,checksum:0x%x\n",
		 frame_type, frame_ver, frame_len, checksum);

	switch (frame_type) {
	case HDMI_INFOFRAME_TYPE_AVI:
		ctrl_frame_en = CTRL_AVI_EN;
		ctrl_reg = GRL_CTRL;
		break;
	case HDMI_INFOFRAME_TYPE_SPD:
		ctrl_frame_en = CTRL_SPD_EN;
		ctrl_reg = GRL_CTRL;
		break;
	case HDMI_INFOFRAME_TYPE_AUDIO:
		ctrl_frame_en = CTRL_AUDIO_EN;
		ctrl_reg = GRL_CTRL;
		break;
	case HDMI_INFOFRAME_TYPE_VENDOR:
		ctrl_frame_en = VS_EN;
		ctrl_reg = GRL_ACP_ISRC_CTRL;
		break;
	default:
		break;
	}
	mtk_hdmi_mask(hdmi, ctrl_reg, 0, ctrl_frame_en);
	mtk_hdmi_write(hdmi, GRL_INFOFRM_TYPE, frame_type);
	mtk_hdmi_write(hdmi, GRL_INFOFRM_VER, frame_ver);
	mtk_hdmi_write(hdmi, GRL_INFOFRM_LNG, frame_len);

	mtk_hdmi_write(hdmi, GRL_IFM_PORT, checksum);
	for (i = 0; i < frame_len; i++)
		mtk_hdmi_write(hdmi, GRL_IFM_PORT, frame_data[i]);

	mtk_hdmi_mask(hdmi, ctrl_reg, ctrl_frame_en, ctrl_frame_en);
}

void mtk_hdmi_hw_send_aud_packet(struct mtk_hdmi *hdmi, bool enable)
{
	mtk_hdmi_mask(hdmi, GRL_SHIFT_R2, enable ? 0 : AUDIO_PACKET_OFF,
		      AUDIO_PACKET_OFF);
}

void mtk_hdmi_hw_config_sys(struct mtk_hdmi *hdmi)
{
	regmap_update_bits(hdmi->sys_regmap, hdmi->sys_offset + HDMI_SYS_CFG20,
			   HDMI_OUT_FIFO_EN | MHL_MODE_ON, 0);
	mdelay(2);
	regmap_update_bits(hdmi->sys_regmap, hdmi->sys_offset + HDMI_SYS_CFG20,
			   HDMI_OUT_FIFO_EN | MHL_MODE_ON, HDMI_OUT_FIFO_EN);
}

void mtk_hdmi_hw_set_deep_color_mode(struct mtk_hdmi *hdmi)
{
	regmap_update_bits(hdmi->sys_regmap, hdmi->sys_offset + HDMI_SYS_CFG20,
			   DEEP_COLOR_MODE_MASK | DEEP_COLOR_EN, COLOR_8BIT_MODE);
}

void mtk_hdmi_hw_send_av_mute(struct mtk_hdmi *hdmi)
{
	mtk_hdmi_mask(hdmi, GRL_CFG4, 0, CTRL_AVMUTE);
	mdelay(2);
	mtk_hdmi_mask(hdmi, GRL_CFG4, CTRL_AVMUTE, CTRL_AVMUTE);
}

void mtk_hdmi_hw_send_av_unmute(struct mtk_hdmi *hdmi)
{
	mtk_hdmi_mask(hdmi, GRL_CFG4, CFG4_AV_UNMUTE_EN,
		      CFG4_AV_UNMUTE_EN | CFG4_AV_UNMUTE_SET);
	mdelay(2);
	mtk_hdmi_mask(hdmi, GRL_CFG4, CFG4_AV_UNMUTE_SET,
		      CFG4_AV_UNMUTE_EN | CFG4_AV_UNMUTE_SET);
}

void mtk_hdmi_hw_ncts_enable(struct mtk_hdmi *hdmi, bool on)
{
	mtk_hdmi_mask(hdmi, GRL_CTS_CTRL, on ? 0 : CTS_CTRL_SOFT,
		      CTS_CTRL_SOFT);
}

void mtk_hdmi_hw_ncts_auto_write_enable(struct mtk_hdmi *hdmi, bool enable)
{
	mtk_hdmi_mask(hdmi, GRL_CTS_CTRL, enable ? NCTS_WRI_ANYTIME : 0,
		      NCTS_WRI_ANYTIME);
}

void mtk_hdmi_hw_msic_setting(struct mtk_hdmi *hdmi,
			      struct drm_display_mode *mode)
{
	mtk_hdmi_mask(hdmi, GRL_CFG4, 0, CFG_MHL_MODE);

	if (mode->flags & DRM_MODE_FLAG_INTERLACE &&
	    mode->clock == 74250 &&
	    mode->vdisplay == 1080)
		mtk_hdmi_mask(hdmi, GRL_CFG2, 0, MHL_DE_SEL);
	else
		mtk_hdmi_mask(hdmi, GRL_CFG2, MHL_DE_SEL, MHL_DE_SEL);
}

void mtk_hdmi_hw_aud_set_channel_swap(struct mtk_hdmi *hdmi,
				      enum hdmi_aud_channel_swap_type swap)
{
	u8 swap_bit;

	switch (swap) {
	case HDMI_AUD_SWAP_LR:
		swap_bit = LR_SWAP;
		break;
	case HDMI_AUD_SWAP_LFE_CC:
		swap_bit = LFE_CC_SWAP;
		break;
	case HDMI_AUD_SWAP_LSRS:
		swap_bit = LSRS_SWAP;
		break;
	case HDMI_AUD_SWAP_RLS_RRS:
		swap_bit = RLS_RRS_SWAP;
		break;
	case HDMI_AUD_SWAP_LR_STATUS:
		swap_bit = LR_STATUS_SWAP;
		break;
	default:
		swap_bit = LFE_CC_SWAP;
		break;
	}
	mtk_hdmi_mask(hdmi, GRL_CH_SWAP, swap_bit, 0xff);
}

void mtk_hdmi_hw_aud_raw_data_enable(struct mtk_hdmi *hdmi, bool enable)
{
	mtk_hdmi_mask(hdmi, GRL_MIX_CTRL, enable ? MIX_CTRL_FLAT : 0,
		      MIX_CTRL_FLAT);
}

void mtk_hdmi_hw_aud_set_bit_num(struct mtk_hdmi *hdmi,
				 enum hdmi_audio_sample_size bit_num)
{
	u32 val = 0;

	if (bit_num == HDMI_AUDIO_SAMPLE_SIZE_16)
		val = AOUT_16BIT;
	else if (bit_num == HDMI_AUDIO_SAMPLE_SIZE_20)
		val = AOUT_20BIT;
	else if (bit_num == HDMI_AUDIO_SAMPLE_SIZE_24)
		val = AOUT_24BIT;

	mtk_hdmi_mask(hdmi, GRL_AOUT_BNUM_SEL, val, 0x03);
}

void mtk_hdmi_hw_aud_set_i2s_fmt(struct mtk_hdmi *hdmi,
				 enum hdmi_aud_i2s_fmt i2s_fmt)
{
	u32 val = 0;

	val = mtk_hdmi_read(hdmi, GRL_CFG0);
	val &= ~0x33;

	switch (i2s_fmt) {
	case HDMI_I2S_MODE_RJT_24BIT:
		val |= (CFG0_I2S_MODE_RTJ | CFG0_I2S_MODE_24BIT);
		break;
	case HDMI_I2S_MODE_RJT_16BIT:
		val |= (CFG0_I2S_MODE_RTJ | CFG0_I2S_MODE_16BIT);
		break;
	case HDMI_I2S_MODE_LJT_24BIT:
		val |= (CFG0_I2S_MODE_LTJ | CFG0_I2S_MODE_24BIT);
		break;
	case HDMI_I2S_MODE_LJT_16BIT:
		val |= (CFG0_I2S_MODE_LTJ | CFG0_I2S_MODE_16BIT);
		break;
	case HDMI_I2S_MODE_I2S_24BIT:
		val |= (CFG0_I2S_MODE_I2S | CFG0_I2S_MODE_24BIT);
		break;
	case HDMI_I2S_MODE_I2S_16BIT:
		val |= (CFG0_I2S_MODE_I2S | CFG0_I2S_MODE_16BIT);
		break;
	default:
		break;
	}
	mtk_hdmi_write(hdmi, GRL_CFG0, val);
}

void mtk_hdmi_hw_aud_set_high_bitrate(struct mtk_hdmi *hdmi, bool enable)
{
	mtk_hdmi_mask(hdmi, GRL_AOUT_BNUM_SEL,
		      enable ? HIGH_BIT_RATE_PACKET_ALIGN : 0,
		      HIGH_BIT_RATE_PACKET_ALIGN);
	mtk_hdmi_mask(hdmi, GRL_AUDIO_CFG, enable ? HIGH_BIT_RATE : 0,
		      HIGH_BIT_RATE);
}

void mtk_hdmi_phy_aud_dst_normal_double_enable(struct mtk_hdmi *hdmi,
					       bool enable)
{
	mtk_hdmi_mask(hdmi, GRL_AUDIO_CFG, enable ? DST_NORMAL_DOUBLE : 0,
		      DST_NORMAL_DOUBLE);
}

void mtk_hdmi_hw_aud_dst_enable(struct mtk_hdmi *hdmi, bool enable)
{
	mtk_hdmi_mask(hdmi, GRL_AUDIO_CFG, enable ? SACD_DST : 0, SACD_DST);
}

void mtk_hdmi_hw_aud_dsd_enable(struct mtk_hdmi *hdmi, bool enable)
{
	mtk_hdmi_mask(hdmi, GRL_AUDIO_CFG, enable ? SACD_SEL : 0, SACD_SEL);
}

void mtk_hdmi_hw_aud_set_i2s_chan_num(struct mtk_hdmi *hdmi,
				      enum hdmi_aud_channel_type channel_type,
				      u8 channel_count)
{
	u8 val_1, val_2, val_3, val_4;

	if (channel_count == 2) {
		val_1 = 0x04;
		val_2 = 0x50;
	} else if (channel_count == 3 || channel_count == 4) {
		if (channel_count == 4 &&
		    (channel_type == HDMI_AUD_CHAN_TYPE_3_0_LRS ||
		    channel_type == HDMI_AUD_CHAN_TYPE_4_0)) {
			val_1 = 0x14;
		} else {
			val_1 = 0x0c;
		}
		val_2 = 0x50;
	} else if (channel_count == 6 || channel_count == 5) {
		if (channel_count == 6 &&
		    channel_type != HDMI_AUD_CHAN_TYPE_5_1 &&
		    channel_type != HDMI_AUD_CHAN_TYPE_4_1_CLRS) {
			val_1 = 0x3c;
			val_2 = 0x50;
		} else {
			val_1 = 0x1c;
			val_2 = 0x50;
		}
	} else if (channel_count == 8 || channel_count == 7) {
		val_1 = 0x3c;
		val_2 = 0x50;
	} else {
		val_1 = 0x04;
		val_2 = 0x50;
	}

	val_3 = 0xc6;
	val_4 = 0xfa;

	mtk_hdmi_write(hdmi, GRL_CH_SW0, val_2);
	mtk_hdmi_write(hdmi, GRL_CH_SW1, val_3);
	mtk_hdmi_write(hdmi, GRL_CH_SW2, val_4);
	mtk_hdmi_write(hdmi, GRL_I2S_UV, val_1);
}

void mtk_hdmi_hw_aud_set_input_type(struct mtk_hdmi *hdmi,
				    enum hdmi_aud_input_type input_type)
{
	u32 val = 0;

	val = mtk_hdmi_read(hdmi, GRL_CFG1);
	if (input_type == HDMI_AUD_INPUT_I2S &&
	    (val & CFG1_SPDIF) == CFG1_SPDIF) {
		val &= ~CFG1_SPDIF;
	} else if (input_type == HDMI_AUD_INPUT_SPDIF &&
		(val & CFG1_SPDIF) == 0) {
		val |= CFG1_SPDIF;
	}
	mtk_hdmi_write(hdmi, GRL_CFG1, val);
}

void mtk_hdmi_hw_aud_set_channel_status(struct mtk_hdmi *hdmi,
					u8 *l_chan_status, u8 *r_chan_status,
					enum hdmi_audio_sample_frequency
					aud_hdmi_fs)
{
	u8 l_status[5];
	u8 r_status[5];
	u8 val = 0;

	l_status[0] = l_chan_status[0];
	l_status[1] = l_chan_status[1];
	l_status[2] = l_chan_status[2];
	r_status[0] = r_chan_status[0];
	r_status[1] = r_chan_status[1];
	r_status[2] = r_chan_status[2];

	l_status[0] &= ~0x02;
	r_status[0] &= ~0x02;

	val = l_chan_status[3] & 0xf0;
	switch (aud_hdmi_fs) {
	case HDMI_AUDIO_SAMPLE_FREQUENCY_32000:
		val |= 0x03;
		break;
	case HDMI_AUDIO_SAMPLE_FREQUENCY_44100:
		break;
	case HDMI_AUDIO_SAMPLE_FREQUENCY_88200:
		val |= 0x08;
		break;
	case HDMI_AUDIO_SAMPLE_FREQUENCY_96000:
		val |= 0x0a;
		break;
	case HDMI_AUDIO_SAMPLE_FREQUENCY_48000:
		val |= 0x02;
		break;
	default:
		val |= 0x02;
		break;
	}
	l_status[3] = val;
	r_status[3] = val;

	val = l_chan_status[4];
	val |= ((~(l_status[3] & 0x0f)) << 4);
	l_status[4] = val;
	r_status[4] = val;

	val = l_status[0];
	mtk_hdmi_write(hdmi, GRL_I2S_C_STA0, val);
	mtk_hdmi_write(hdmi, GRL_L_STATUS_0, val);

	val = r_status[0];
	mtk_hdmi_write(hdmi, GRL_R_STATUS_0, val);

	val = l_status[1];
	mtk_hdmi_write(hdmi, GRL_I2S_C_STA1, val);
	mtk_hdmi_write(hdmi, GRL_L_STATUS_1, val);

	val = r_status[1];
	mtk_hdmi_write(hdmi, GRL_R_STATUS_1, val);

	val = l_status[2];
	mtk_hdmi_write(hdmi, GRL_I2S_C_STA2, val);
	mtk_hdmi_write(hdmi, GRL_L_STATUS_2, val);

	val = r_status[2];
	mtk_hdmi_write(hdmi, GRL_R_STATUS_2, val);

	val = l_status[3];
	mtk_hdmi_write(hdmi, GRL_I2S_C_STA3, val);
	mtk_hdmi_write(hdmi, GRL_L_STATUS_3, val);

	val = r_status[3];
	mtk_hdmi_write(hdmi, GRL_R_STATUS_3, val);

	val = l_status[4];
	mtk_hdmi_write(hdmi, GRL_I2S_C_STA4, val);
	mtk_hdmi_write(hdmi, GRL_L_STATUS_4, val);

	val = r_status[4];
	mtk_hdmi_write(hdmi, GRL_R_STATUS_4, val);

	for (val = 0; val < 19; val++) {
		mtk_hdmi_write(hdmi, GRL_L_STATUS_5 + val * 4, 0);
		mtk_hdmi_write(hdmi, GRL_R_STATUS_5 + val * 4, 0);
	}
}

void mtk_hdmi_hw_aud_src_reenable(struct mtk_hdmi *hdmi)
{
	u32 val;

	val = mtk_hdmi_read(hdmi, GRL_MIX_CTRL);
	if (val & MIX_CTRL_SRC_EN) {
		val &= ~MIX_CTRL_SRC_EN;
		mtk_hdmi_write(hdmi, GRL_MIX_CTRL, val);
		usleep_range(255, 512);
		val |= MIX_CTRL_SRC_EN;
		mtk_hdmi_write(hdmi, GRL_MIX_CTRL, val);
	}
}

void mtk_hdmi_hw_aud_src_off(struct mtk_hdmi *hdmi)
{
	u32 val;

	val = mtk_hdmi_read(hdmi, GRL_MIX_CTRL);
	val &= ~MIX_CTRL_SRC_EN;
	mtk_hdmi_write(hdmi, GRL_MIX_CTRL, val);
	mtk_hdmi_write(hdmi, GRL_SHIFT_L1, 0x00);
}

void mtk_hdmi_hw_aud_set_mclk(struct mtk_hdmi *hdmi, enum hdmi_aud_mclk mclk)
{
	u32 val;

	val = mtk_hdmi_read(hdmi, GRL_CFG5);
	val &= CFG5_CD_RATIO_MASK;

	switch (mclk) {
	case HDMI_AUD_MCLK_128FS:
		val |= CFG5_FS128;
		break;
	case HDMI_AUD_MCLK_256FS:
		val |= CFG5_FS256;
		break;
	case HDMI_AUD_MCLK_384FS:
		val |= CFG5_FS384;
		break;
	case HDMI_AUD_MCLK_512FS:
		val |= CFG5_FS512;
		break;
	case HDMI_AUD_MCLK_768FS:
		val |= CFG5_FS768;
		break;
	default:
		val |= CFG5_FS256;
		break;
	}
	mtk_hdmi_write(hdmi, GRL_CFG5, val);
}

void mtk_hdmi_hw_aud_aclk_inv_enable(struct mtk_hdmi *hdmi, bool enable)
{
	u32 val;

	val = mtk_hdmi_read(hdmi, GRL_CFG2);
	if (enable)
		val |= 0x80;
	else
		val &= ~0x80;
	mtk_hdmi_write(hdmi, GRL_CFG2, val);
}

static void do_hdmi_hw_aud_set_ncts(struct mtk_hdmi *hdmi,
				    enum hdmi_audio_sample_frequency freq,
				    int pix)
{
	unsigned char val[NCTS_BYTES];
	unsigned int temp;
	int i = 0;

	mtk_hdmi_write(hdmi, GRL_NCTS, 0);
	mtk_hdmi_write(hdmi, GRL_NCTS, 0);
	mtk_hdmi_write(hdmi, GRL_NCTS, 0);
	memset(val, 0, sizeof(val));

	for (i = 0; i < NCTS_BYTES; i++) {
		if ((freq < 8) && (pix < 9))
			val[i] = HDMI_NCTS[freq - 1][pix][i];
	}
	temp = (val[0] << 24) | (val[1] << 16) |
		(val[2] << 8) | (val[3]);	/* CTS */

	for (i = 0; i < NCTS_BYTES; i++)
		mtk_hdmi_write(hdmi, GRL_NCTS, val[i]);
}

void mtk_hdmi_hw_aud_set_ncts(struct mtk_hdmi *hdmi,
			      enum hdmi_audio_sample_frequency freq, int clock)
{
	int pix = 0;

	switch (clock) {
	case 27000:
		pix = 0;
		break;
	case 74175:
		pix = 2;
		break;
	case 74250:
		pix = 3;
		break;
	case 148350:
		pix = 4;
		break;
	case 148500:
		pix = 5;
		break;
	default:
		pix = 0;
		break;
	}

	mtk_hdmi_mask(hdmi, DUMMY_304, AUDIO_I2S_NCTS_SEL_64,
		      AUDIO_I2S_NCTS_SEL);
	do_hdmi_hw_aud_set_ncts(hdmi, freq, pix);
}
