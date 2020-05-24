// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2010-2016, 2018, 2020, The Linux Foundation. All rights reserved. */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/types.h>

#include "video/msm_hdmi_modes.h"
#include "mdss_debug.h"
#include "mdss_fb.h"
#include "mdss.h"
#include "mdss_hdmi_util.h"
#include "mdss_panel.h"
#include "mdss_hdmi_panel.h"

#define HDMI_TX_SCRAMBLER_THRESHOLD_RATE_KHZ 340000
#define HDMI_TX_SCRAMBLER_TIMEOUT_MSEC 200

#define HDMI_TX_KHZ_TO_HZ                  1000U

/* AVI INFOFRAME DATA */
#define NUM_MODES_AVI 20
#define AVI_MAX_DATA_BYTES 13

/* Line numbers at which AVI Infoframe and Vendor Infoframe will be sent */
#define AVI_IFRAME_LINE_NUMBER 1
#define VENDOR_IFRAME_LINE_NUMBER 3

#define IFRAME_CHECKSUM_32(d)			\
	((d & 0xff) + ((d >> 8) & 0xff) +	\
	((d >> 16) & 0xff) + ((d >> 24) & 0xff))

#define IFRAME_PACKET_OFFSET 0x80
/*
 * InfoFrame Type Code:
 * 0x0 - Reserved
 * 0x1 - Vendor Specific
 * 0x2 - Auxiliary Video Information
 * 0x3 - Source Product Description
 * 0x4 - AUDIO
 * 0x5 - MPEG Source
 * 0x6 - NTSC VBI
 * 0x7 - 0xFF - Reserved
 */
#define AVI_IFRAME_TYPE 0x2
#define AVI_IFRAME_VERSION 0x2
#define LEFT_SHIFT_BYTE(x) ((x) << 8)
#define LEFT_SHIFT_WORD(x) ((x) << 16)
#define LEFT_SHIFT_24BITS(x) ((x) << 24)

/* AVI Infoframe data byte 3, bit 7 (msb) represents ITC bit */
#define SET_ITC_BIT(byte)  (byte = (byte | BIT(7)))
#define CLR_ITC_BIT(byte)  (byte = (byte & ~BIT(7)))

/*
 * CN represents IT content type, if ITC bit in infoframe data byte 3
 * is set, CN bits will represent content type as below:
 * 0b00 Graphics
 * 0b01 Photo
 * 0b10 Cinema
 * 0b11 Game
 */
#define CONFIG_CN_BITS(bits, byte) \
		(byte = (byte & ~(BIT(4) | BIT(5))) |\
			((bits & (BIT(0) | BIT(1))) << 4))

struct hdmi_avi_iframe_bar_info {
	bool vert_binfo_present;
	bool horz_binfo_present;
	u32 end_of_top_bar;
	u32 start_of_bottom_bar;
	u32 end_of_left_bar;
	u32 start_of_right_bar;
};

struct hdmi_avi_infoframe_config {
	u32 pixel_format;
	u32 scan_info;
	bool act_fmt_info_present;
	u32 colorimetry_info;
	u32 ext_colorimetry_info;
	u32 rgb_quantization_range;
	u32 yuv_quantization_range;
	u32 scaling_info;
	bool is_it_content;
	u8 content_type;
	u8 pixel_rpt_factor;
	struct hdmi_avi_iframe_bar_info bar_info;
};

struct hdmi_video_config {
	struct msm_hdmi_mode_timing_info *timing;
	struct hdmi_avi_infoframe_config avi_iframe;
};

struct hdmi_panel {
	struct dss_io_data *io;
	struct hdmi_util_ds_data *ds_data;
	struct hdmi_panel_data *data;
	struct hdmi_video_config vid_cfg;
	struct hdmi_tx_ddc_ctrl *ddc;
	u32 version;
	u32 vic;
	u8 *spd_vendor_name;
	u8 *spd_product_description;
	bool on;
	bool scrambler_enabled;
};

enum {
	DATA_BYTE_1,
	DATA_BYTE_2,
	DATA_BYTE_3,
	DATA_BYTE_4,
	DATA_BYTE_5,
	DATA_BYTE_6,
	DATA_BYTE_7,
	DATA_BYTE_8,
	DATA_BYTE_9,
	DATA_BYTE_10,
	DATA_BYTE_11,
	DATA_BYTE_12,
	DATA_BYTE_13,
};

enum hdmi_quantization_range {
	HDMI_QUANTIZATION_DEFAULT,
	HDMI_QUANTIZATION_LIMITED_RANGE,
	HDMI_QUANTIZATION_FULL_RANGE
};

enum hdmi_scaling_info {
	HDMI_SCALING_NONE,
	HDMI_SCALING_HORZ,
	HDMI_SCALING_VERT,
	HDMI_SCALING_HORZ_VERT,
};

static void hdmi_panel_update_dfps_data(struct hdmi_panel *panel)
{
	struct mdss_panel_info *pinfo = panel->data->pinfo;

	pinfo->saved_total = mdss_panel_get_htotal(pinfo, true);
	pinfo->saved_fporch = panel->vid_cfg.timing->front_porch_h;

	pinfo->current_fps = panel->vid_cfg.timing->refresh_rate;
	pinfo->default_fps = panel->vid_cfg.timing->refresh_rate;
	pinfo->lcdc.frame_rate = panel->vid_cfg.timing->refresh_rate;
}

static int hdmi_panel_config_avi(struct hdmi_panel *panel)
{
	struct mdss_panel_info *pinfo = panel->data->pinfo;
	struct hdmi_video_config *vid_cfg = &panel->vid_cfg;
	struct hdmi_avi_infoframe_config *avi = &vid_cfg->avi_iframe;
	struct msm_hdmi_mode_timing_info *timing;
	u32 ret = 0;

	timing = panel->vid_cfg.timing;
	if (!timing) {
		pr_err("fmt not supported: %d\n", panel->vic);
		ret = -EPERM;
		goto end;
	}

	/* Setup AVI Infoframe content */
	avi->pixel_format  = pinfo->out_format;
	avi->is_it_content = panel->data->is_it_content;
	avi->content_type  = panel->data->content_type;
	avi->scan_info     = panel->data->scan_info;

	avi->bar_info.end_of_top_bar = 0x0;
	avi->bar_info.start_of_bottom_bar = timing->active_v + 1;
	avi->bar_info.end_of_left_bar = 0;
	avi->bar_info.start_of_right_bar = timing->active_h + 1;

	avi->act_fmt_info_present = true;
	avi->rgb_quantization_range = HDMI_QUANTIZATION_DEFAULT;
	avi->yuv_quantization_range = HDMI_QUANTIZATION_DEFAULT;

	avi->scaling_info = HDMI_SCALING_NONE;

	avi->colorimetry_info = 0;
	avi->ext_colorimetry_info = 0;

	avi->pixel_rpt_factor = 0;
end:
	return ret;
}

static int hdmi_panel_setup_video(struct hdmi_panel *panel)
{
	u32 total_h, start_h, end_h;
	u32 total_v, start_v, end_v;
	u32 div = 0;
	struct dss_io_data *io = panel->io;
	struct msm_hdmi_mode_timing_info *timing;

	timing = panel->vid_cfg.timing;
	if (!timing) {
		pr_err("fmt not supported: %d\n", panel->vic);
		return -EPERM;
	}

	/* reduce horizontal params by half for YUV420 output */
	if (panel->vid_cfg.avi_iframe.pixel_format == MDP_Y_CBCR_H2V2)
		div = 1;

	total_h = (hdmi_tx_get_h_total(timing) >> div) - 1;
	total_v = hdmi_tx_get_v_total(timing) - 1;

	if (((total_v << 16) & 0xE0000000) || (total_h & 0xFFFFE000)) {
		pr_err("total v=%d or h=%d is larger than supported\n",
			total_v, total_h);
		return -EPERM;
	}
	DSS_REG_W(io, HDMI_TOTAL, (total_v << 16) | (total_h << 0));

	start_h = (timing->back_porch_h >> div) +
		  (timing->pulse_width_h >> div);
	end_h   = (total_h + 1) - (timing->front_porch_h >> div);
	if (((end_h << 16) & 0xE0000000) || (start_h & 0xFFFFE000)) {
		pr_err("end_h=%d or start_h=%d is larger than supported\n",
			end_h, start_h);
		return -EPERM;
	}
	DSS_REG_W(io, HDMI_ACTIVE_H, (end_h << 16) | (start_h << 0));

	start_v = timing->back_porch_v + timing->pulse_width_v - 1;
	end_v   = total_v - timing->front_porch_v;
	if (((end_v << 16) & 0xE0000000) || (start_v & 0xFFFFE000)) {
		pr_err("end_v=%d or start_v=%d is larger than supported\n",
			end_v, start_v);
		return -EPERM;
	}
	DSS_REG_W(io, HDMI_ACTIVE_V, (end_v << 16) | (start_v << 0));

	if (timing->interlaced) {
		DSS_REG_W(io, HDMI_V_TOTAL_F2, (total_v + 1) << 0);
		DSS_REG_W(io, HDMI_ACTIVE_V_F2,
			((end_v + 1) << 16) | ((start_v + 1) << 0));
	} else {
		DSS_REG_W(io, HDMI_V_TOTAL_F2, 0);
		DSS_REG_W(io, HDMI_ACTIVE_V_F2, 0);
	}

	DSS_REG_W(io, HDMI_FRAME_CTRL,
		((timing->interlaced << 31) & 0x80000000) |
		((timing->active_low_h << 29) & 0x20000000) |
		((timing->active_low_v << 28) & 0x10000000));

	hdmi_panel_update_dfps_data(panel);

	return 0;
}

static void hdmi_panel_set_avi_infoframe(struct hdmi_panel *panel)
{
	int i;
	u8  avi_iframe[AVI_MAX_DATA_BYTES] = {0};
	u8 checksum;
	u32 sum, reg_val;
	struct dss_io_data *io = panel->io;
	struct hdmi_avi_infoframe_config *avi;
	struct msm_hdmi_mode_timing_info *timing;

	avi = &panel->vid_cfg.avi_iframe;
	timing = panel->vid_cfg.timing;

	/*
	 * BYTE - 1:
	 *	0:1 - Scan Information
	 *	2:3 - Bar Info
	 *	4   - Active Format Info present
	 *	5:6 - Pixel format type;
	 *	7   - Reserved;
	 */
	avi_iframe[0] = (avi->scan_info & 0x3) |
			(avi->bar_info.vert_binfo_present ? BIT(2) : 0) |
			(avi->bar_info.horz_binfo_present ? BIT(3) : 0) |
			(avi->act_fmt_info_present ? BIT(4) : 0);
	if (avi->pixel_format == MDP_Y_CBCR_H2V2)
		avi_iframe[0] |= (0x3 << 5);
	else if (avi->pixel_format == MDP_Y_CBCR_H2V1)
		avi_iframe[0] |= (0x1 << 5);
	else if (avi->pixel_format == MDP_Y_CBCR_H1V1)
		avi_iframe[0] |= (0x2 << 5);

	/*
	 * BYTE - 2:
	 *	0:3 - Active format info
	 *	4:5 - Picture aspect ratio
	 *	6:7 - Colorimetry info
	 */
	avi_iframe[1] |= 0x08;
	if (timing->ar == HDMI_RES_AR_4_3)
		avi_iframe[1] |= (0x1 << 4);
	else if (timing->ar == HDMI_RES_AR_16_9)
		avi_iframe[1] |= (0x2 << 4);

	avi_iframe[1] |= (avi->colorimetry_info & 0x3) << 6;

	/*
	 * BYTE - 3:
	 *	0:1 - Scaling info
	 *	2:3 - Quantization range
	 *	4:6 - Extended Colorimetry
	 *	7   - IT content
	 */
	avi_iframe[2] |= (avi->scaling_info & 0x3) |
			 ((avi->rgb_quantization_range & 0x3) << 2) |
			 ((avi->ext_colorimetry_info & 0x7) << 4) |
			 ((avi->is_it_content ? 0x1 : 0x0) << 7);
	/*
	 * BYTE - 4:
	 *	0:7 - VIC
	 */
	if (timing->video_format < HDMI_VFRMT_END)
		avi_iframe[3] = timing->video_format;

	/*
	 * BYTE - 5:
	 *	0:3 - Pixel Repeat factor
	 *	4:5 - Content type
	 *	6:7 - YCC Quantization range
	 */
	avi_iframe[4] = (avi->pixel_rpt_factor & 0xF) |
			((avi->content_type & 0x3) << 4) |
			((avi->yuv_quantization_range & 0x3) << 6);

	/* BYTE - 6,7: End of top bar */
	avi_iframe[5] = avi->bar_info.end_of_top_bar & 0xFF;
	avi_iframe[6] = ((avi->bar_info.end_of_top_bar & 0xFF00) >> 8);

	/* BYTE - 8,9: Start of bottom bar */
	avi_iframe[7] = avi->bar_info.start_of_bottom_bar & 0xFF;
	avi_iframe[8] = ((avi->bar_info.start_of_bottom_bar & 0xFF00) >>
			 8);

	/* BYTE - 10,11: Endof of left bar */
	avi_iframe[9] = avi->bar_info.end_of_left_bar & 0xFF;
	avi_iframe[10] = ((avi->bar_info.end_of_left_bar & 0xFF00) >> 8);

	/* BYTE - 12,13: Start of right bar */
	avi_iframe[11] = avi->bar_info.start_of_right_bar & 0xFF;
	avi_iframe[12] = ((avi->bar_info.start_of_right_bar & 0xFF00) >>
			  8);

	sum = IFRAME_PACKET_OFFSET + AVI_IFRAME_TYPE +
		AVI_IFRAME_VERSION + AVI_MAX_DATA_BYTES;

	for (i = 0; i < AVI_MAX_DATA_BYTES; i++)
		sum += avi_iframe[i];
	sum &= 0xFF;
	sum = 256 - sum;
	checksum = (u8) sum;

	reg_val = checksum |
		LEFT_SHIFT_BYTE(avi_iframe[DATA_BYTE_1]) |
		LEFT_SHIFT_WORD(avi_iframe[DATA_BYTE_2]) |
		LEFT_SHIFT_24BITS(avi_iframe[DATA_BYTE_3]);
	DSS_REG_W(io, HDMI_AVI_INFO0, reg_val);

	reg_val = avi_iframe[DATA_BYTE_4] |
		LEFT_SHIFT_BYTE(avi_iframe[DATA_BYTE_5]) |
		LEFT_SHIFT_WORD(avi_iframe[DATA_BYTE_6]) |
		LEFT_SHIFT_24BITS(avi_iframe[DATA_BYTE_7]);
	DSS_REG_W(io, HDMI_AVI_INFO1, reg_val);

	reg_val = avi_iframe[DATA_BYTE_8] |
		LEFT_SHIFT_BYTE(avi_iframe[DATA_BYTE_9]) |
		LEFT_SHIFT_WORD(avi_iframe[DATA_BYTE_10]) |
		LEFT_SHIFT_24BITS(avi_iframe[DATA_BYTE_11]);
	DSS_REG_W(io, HDMI_AVI_INFO2, reg_val);

	reg_val = avi_iframe[DATA_BYTE_12] |
		LEFT_SHIFT_BYTE(avi_iframe[DATA_BYTE_13]) |
		LEFT_SHIFT_24BITS(AVI_IFRAME_VERSION);
	DSS_REG_W(io, HDMI_AVI_INFO3, reg_val);

	/* AVI InfFrame enable (every frame) */
	DSS_REG_W(io, HDMI_INFOFRAME_CTRL0,
		DSS_REG_R(io, HDMI_INFOFRAME_CTRL0) | BIT(1) | BIT(0));

	reg_val = DSS_REG_R(io, HDMI_INFOFRAME_CTRL1);
	reg_val &= ~0x3F;
	reg_val |= AVI_IFRAME_LINE_NUMBER;
	DSS_REG_W(io, HDMI_INFOFRAME_CTRL1, reg_val);
}

static void hdmi_panel_set_vendor_specific_infoframe(void *input)
{
	int i;
	u8 vs_iframe[9]; /* two header + length + 6 data */
	u32 sum, reg_val;
	u32 hdmi_vic, hdmi_video_format, s3d_struct = 0;
	struct hdmi_panel *panel = input;
	struct dss_io_data *io = panel->io;

	/* HDMI Spec 1.4a Table 8-10 */
	vs_iframe[0] = 0x81; /* type */
	vs_iframe[1] = 0x1;  /* version */
	vs_iframe[2] = 0x8;  /* length */

	vs_iframe[3] = 0x0; /* PB0: checksum */

	/* PB1..PB3: 24 Bit IEEE Registration Code 00_0C_03 */
	vs_iframe[4] = 0x03;
	vs_iframe[5] = 0x0C;
	vs_iframe[6] = 0x00;

	if ((panel->data->s3d_mode != HDMI_S3D_NONE) &&
	    panel->data->s3d_support) {
		switch (panel->data->s3d_mode) {
		case HDMI_S3D_SIDE_BY_SIDE:
			s3d_struct = 0x8;
			break;
		case HDMI_S3D_TOP_AND_BOTTOM:
			s3d_struct = 0x6;
			break;
		default:
			s3d_struct = 0;
		}
		hdmi_video_format = 0x2;
		hdmi_vic = 0;
		/* PB5: 3D_Structure[7:4], Reserved[3:0] */
		vs_iframe[8] = s3d_struct << 4;
	} else {
		hdmi_video_format = 0x1;
		switch (panel->vic) {
		case HDMI_EVFRMT_3840x2160p30_16_9:
			hdmi_vic = 0x1;
			break;
		case HDMI_EVFRMT_3840x2160p25_16_9:
			hdmi_vic = 0x2;
			break;
		case HDMI_EVFRMT_3840x2160p24_16_9:
			hdmi_vic = 0x3;
			break;
		case HDMI_EVFRMT_4096x2160p24_16_9:
			hdmi_vic = 0x4;
			break;
		default:
			hdmi_video_format = 0x0;
			hdmi_vic = 0x0;
		}
		/* PB5: HDMI_VIC */
		vs_iframe[8] = hdmi_vic;
	}
	/* PB4: HDMI Video Format[7:5],  Reserved[4:0] */
	vs_iframe[7] = (hdmi_video_format << 5) & 0xE0;

	/* compute checksum */
	sum = 0;
	for (i = 0; i < 9; i++)
		sum += vs_iframe[i];

	sum &= 0xFF;
	sum = 256 - sum;
	vs_iframe[3] = (u8)sum;

	reg_val = (s3d_struct << 24) | (hdmi_vic << 16) |
		  (vs_iframe[3] << 8) | (hdmi_video_format << 5) |
		  vs_iframe[2];
	DSS_REG_W(io, HDMI_VENSPEC_INFO0, reg_val);

	/* vendor specific info-frame enable (every frame) */
	DSS_REG_W(io, HDMI_INFOFRAME_CTRL0,
		DSS_REG_R(io, HDMI_INFOFRAME_CTRL0) | BIT(13) | BIT(12));

	reg_val = DSS_REG_R(io, HDMI_INFOFRAME_CTRL1);
	reg_val &= ~0x3F000000;
	reg_val |= (VENDOR_IFRAME_LINE_NUMBER << 24);
	DSS_REG_W(io, HDMI_INFOFRAME_CTRL1, reg_val);
}

static void hdmi_panel_set_spd_infoframe(struct hdmi_panel *panel)
{
	u32 packet_header  = 0;
	u32 check_sum      = 0;
	u32 packet_payload = 0;
	u32 packet_control = 0;
	u8 *vendor_name = NULL;
	u8 *product_description = NULL;
	struct dss_io_data *io = panel->io;

	vendor_name = panel->spd_vendor_name;
	product_description = panel->spd_product_description;

	/* Setup Packet header and payload */
	/*
	 * 0x83 InfoFrame Type Code
	 * 0x01 InfoFrame Version Number
	 * 0x19 Length of Source Product Description InfoFrame
	 */
	packet_header  = 0x83 | (0x01 << 8) | (0x19 << 16);
	DSS_REG_W(io, HDMI_GENERIC1_HDR, packet_header);
	check_sum += IFRAME_CHECKSUM_32(packet_header);

	packet_payload = (vendor_name[3] & 0x7f)
		| ((vendor_name[4] & 0x7f) << 8)
		| ((vendor_name[5] & 0x7f) << 16)
		| ((vendor_name[6] & 0x7f) << 24);
	DSS_REG_W(io, HDMI_GENERIC1_1, packet_payload);
	check_sum += IFRAME_CHECKSUM_32(packet_payload);

	/* Product Description (7-bit ASCII code) */
	packet_payload = (vendor_name[7] & 0x7f)
		| ((product_description[0] & 0x7f) << 8)
		| ((product_description[1] & 0x7f) << 16)
		| ((product_description[2] & 0x7f) << 24);
	DSS_REG_W(io, HDMI_GENERIC1_2, packet_payload);
	check_sum += IFRAME_CHECKSUM_32(packet_payload);

	packet_payload = (product_description[3] & 0x7f)
		| ((product_description[4] & 0x7f) << 8)
		| ((product_description[5] & 0x7f) << 16)
		| ((product_description[6] & 0x7f) << 24);
	DSS_REG_W(io, HDMI_GENERIC1_3, packet_payload);
	check_sum += IFRAME_CHECKSUM_32(packet_payload);

	packet_payload = (product_description[7] & 0x7f)
		| ((product_description[8] & 0x7f) << 8)
		| ((product_description[9] & 0x7f) << 16)
		| ((product_description[10] & 0x7f) << 24);
	DSS_REG_W(io, HDMI_GENERIC1_4, packet_payload);
	check_sum += IFRAME_CHECKSUM_32(packet_payload);

	packet_payload = (product_description[11] & 0x7f)
		| ((product_description[12] & 0x7f) << 8)
		| ((product_description[13] & 0x7f) << 16)
		| ((product_description[14] & 0x7f) << 24);
	DSS_REG_W(io, HDMI_GENERIC1_5, packet_payload);
	check_sum += IFRAME_CHECKSUM_32(packet_payload);

	/*
	 * Source Device Information
	 * 00h unknown
	 * 01h Digital STB
	 * 02h DVD
	 * 03h D-VHS
	 * 04h HDD Video
	 * 05h DVC
	 * 06h DSC
	 * 07h Video CD
	 * 08h Game
	 * 09h PC general
	 */
	packet_payload = (product_description[15] & 0x7f) | 0x00 << 8;
	DSS_REG_W(io, HDMI_GENERIC1_6, packet_payload);
	check_sum += IFRAME_CHECKSUM_32(packet_payload);

	/* Vendor Name (7bit ASCII code) */
	packet_payload = ((vendor_name[0] & 0x7f) << 8)
		| ((vendor_name[1] & 0x7f) << 16)
		| ((vendor_name[2] & 0x7f) << 24);
	check_sum += IFRAME_CHECKSUM_32(packet_payload);
	packet_payload |= ((0x100 - (0xff & check_sum)) & 0xff);
	DSS_REG_W(io, HDMI_GENERIC1_0, packet_payload);

	/*
	 * GENERIC1_LINE | GENERIC1_CONT | GENERIC1_SEND
	 * Setup HDMI TX generic packet control
	 * Enable this packet to transmit every frame
	 * Enable HDMI TX engine to transmit Generic packet 1
	 */
	packet_control = DSS_REG_R_ND(io, HDMI_GEN_PKT_CTRL);
	packet_control |= ((0x1 << 24) | (1 << 5) | (1 << 4));
	DSS_REG_W(io, HDMI_GEN_PKT_CTRL, packet_control);
}

static int hdmi_panel_setup_infoframe(struct hdmi_panel *panel)
{
	int rc = 0;

	if (!panel) {
		pr_err("invalid panel data\n");
		rc = -EINVAL;
		goto end;
	}

	if (panel->data->infoframe) {
		hdmi_panel_set_avi_infoframe(panel);
		hdmi_panel_set_vendor_specific_infoframe(panel);
		hdmi_panel_set_spd_infoframe(panel);
	}
end:
	return rc;
}

static int hdmi_panel_setup_dc(struct hdmi_panel *panel)
{
	u32 hdmi_ctrl_reg;
	u32 vbi_pkt_reg;

	pr_debug("Deep Color: %s\n", panel->data->dc_enable ? "ON" : "OFF");

	/* enable deep color if supported */
	if (panel->data->dc_enable) {
		hdmi_ctrl_reg = DSS_REG_R(panel->io, HDMI_CTRL);

		/* GC CD override */
		hdmi_ctrl_reg |= BIT(27);

		/* enable deep color for RGB888 30 bits */
		hdmi_ctrl_reg |= BIT(24);
		DSS_REG_W(panel->io, HDMI_CTRL, hdmi_ctrl_reg);

		/* Enable GC_CONT and GC_SEND in General Control Packet
		 * (GCP) register so that deep color data is
		 * transmitted to the sink on every frame, allowing
		 * the sink to decode the data correctly.
		 *
		 * GC_CONT: 0x1 - Send GCP on every frame
		 * GC_SEND: 0x1 - Enable GCP Transmission
		 */
		vbi_pkt_reg = DSS_REG_R(panel->io, HDMI_VBI_PKT_CTRL);
		vbi_pkt_reg |= BIT(5) | BIT(4);
		DSS_REG_W(panel->io, HDMI_VBI_PKT_CTRL, vbi_pkt_reg);
	}

	return 0;
}

static int hdmi_panel_setup_scrambler(struct hdmi_panel *panel)
{
	int rc = 0;
	int timeout_hsync;
	u32 reg_val = 0;
	u32 tmds_clock_ratio = 0;
	u32 tmds_clock = 0;
	bool scrambler_on = false;
	struct msm_hdmi_mode_timing_info *timing = NULL;
	struct mdss_panel_info *pinfo = NULL;

	if (!panel) {
		pr_err("invalid panel data\n");
		return -EINVAL;
	}

	timing = panel->vid_cfg.timing;
	if (!timing) {
		pr_err("Invalid timing info\n");
		return -EINVAL;
	}

	pinfo = panel->data->pinfo;
	if (!pinfo) {
		pr_err("invalid panel info\n");
		return -EINVAL;
	}

	/* Scrambling is supported from HDMI TX 4.0 */
	if (panel->version < HDMI_TX_SCRAMBLER_MIN_TX_VERSION) {
		pr_debug("scrambling not supported by tx\n");
		return 0;
	}

	tmds_clock = hdmi_tx_setup_tmds_clk_rate(timing->pixel_freq,
		pinfo->out_format, panel->data->dc_enable);

	if (tmds_clock > HDMI_TX_SCRAMBLER_THRESHOLD_RATE_KHZ) {
		scrambler_on = true;
		tmds_clock_ratio = 1;
	} else {
		scrambler_on = panel->data->scrambler;
	}

	pr_debug("scrambler %s\n", scrambler_on ? "on" : "off");

	if (scrambler_on) {
		rc = hdmi_scdc_write(panel->ddc,
			HDMI_TX_SCDC_TMDS_BIT_CLOCK_RATIO_UPDATE,
			tmds_clock_ratio);
		if (rc) {
			pr_err("TMDS CLK RATIO ERR\n");
			return rc;
		}

		reg_val = DSS_REG_R(panel->io, HDMI_CTRL);
		reg_val |= BIT(31); /* Enable Update DATAPATH_MODE */
		reg_val |= BIT(28); /* Set SCRAMBLER_EN bit */

		DSS_REG_W(panel->io, HDMI_CTRL, reg_val);

		rc = hdmi_scdc_write(panel->ddc,
			HDMI_TX_SCDC_SCRAMBLING_ENABLE, 0x1);
		if (!rc) {
			panel->scrambler_enabled = true;
		} else {
			pr_err("failed to enable scrambling\n");
			return rc;
		}

		/*
		 * Setup hardware to periodically check for scrambler
		 * status bit on the sink. Sink should set this bit
		 * with in 200ms after scrambler is enabled.
		 */
		timeout_hsync = hdmi_utils_get_timeout_in_hysnc(
					panel->vid_cfg.timing,
					HDMI_TX_SCRAMBLER_TIMEOUT_MSEC);

		if (timeout_hsync <= 0) {
			pr_err("err in timeout hsync calc\n");
			timeout_hsync = HDMI_DEFAULT_TIMEOUT_HSYNC;
		}

		pr_debug("timeout for scrambling en: %d hsyncs\n",
			timeout_hsync);

		rc = hdmi_setup_ddc_timers(panel->ddc,
			HDMI_TX_DDC_TIMER_SCRAMBLER_STATUS, timeout_hsync);
	} else {
		hdmi_scdc_write(panel->ddc,
			HDMI_TX_SCDC_SCRAMBLING_ENABLE, 0x0);

		panel->scrambler_enabled = false;
	}

	return rc;
}

static int hdmi_panel_update_fps(void *input, u32 fps)
{
	struct hdmi_panel *panel = input;
	struct mdss_panel_info *pinfo = panel->data->pinfo;
	struct msm_hdmi_mode_timing_info *timing = panel->vid_cfg.timing;
	u64 pclk;
	int vic;

	timing->back_porch_h = pinfo->lcdc.h_back_porch;
	timing->front_porch_h = pinfo->lcdc.h_front_porch;
	timing->pulse_width_h = pinfo->lcdc.h_pulse_width;

	timing->back_porch_v = pinfo->lcdc.v_back_porch;
	timing->front_porch_v = pinfo->lcdc.v_front_porch;
	timing->pulse_width_v = pinfo->lcdc.v_pulse_width;

	timing->refresh_rate = fps;

	pclk = pinfo->clk_rate;
	do_div(pclk, HDMI_TX_KHZ_TO_HZ);
	timing->pixel_freq = (unsigned long) pclk;

	if (hdmi_panel_setup_video(panel)) {
		DEV_DBG("%s: no change in video timing\n", __func__);
		goto end;
	}

	vic = hdmi_get_video_id_code(timing, panel->ds_data);

	if (vic > 0 && panel->vic != vic) {
		panel->vic = vic;
		DEV_DBG("%s: switched to new resolution id %d\n",
			__func__, vic);
	}

	pinfo->dynamic_fps = false;
end:
	return panel->vic;
}

static int hdmi_panel_power_on(void *input)
{
	int rc = 0;
	bool res_changed = false;
	struct hdmi_panel *panel = input;
	struct mdss_panel_info *pinfo;
	struct msm_hdmi_mode_timing_info *info;

	if (!panel) {
		pr_err("invalid panel data\n");
		rc = -EINVAL;
		goto err;
	}

	pinfo = panel->data->pinfo;
	if (!pinfo) {
		pr_err("invalid panel info\n");
		rc = -EINVAL;
		goto err;
	}

	if (panel->vic != panel->data->vic) {
		res_changed = true;

		pr_debug("switching from %d => %d\n",
			panel->vic, panel->data->vic);

		panel->vic = panel->data->vic;
	}

	if (pinfo->cont_splash_enabled) {
		pinfo->cont_splash_enabled = false;

		if (!res_changed) {
			panel->on = true;

			hdmi_panel_set_vendor_specific_infoframe(panel);
			hdmi_panel_set_spd_infoframe(panel);

			pr_debug("handoff done\n");

			goto end;
		}
	}

	rc = hdmi_panel_config_avi(panel);
	if (rc) {
		pr_err("avi config failed. rc=%d\n", rc);
		goto err;
	}

	rc = hdmi_panel_setup_video(panel);
	if (rc) {
		pr_err("video setup failed. rc=%d\n", rc);
		goto err;
	}

	rc = hdmi_panel_setup_infoframe(panel);
	if (rc) {
		pr_err("infoframe setup failed. rc=%d\n", rc);
		goto err;
	}

	rc = hdmi_panel_setup_scrambler(panel);
	if (rc) {
		pr_err("scrambler setup failed. rc=%d\n", rc);
		goto err;
	}

	rc = hdmi_panel_setup_dc(panel);
	if (rc) {
		pr_err("Deep Color setup failed. rc=%d\n", rc);
		goto err;
	}
end:
	panel->on = true;

	info = panel->vid_cfg.timing;
	pr_debug("%dx%d%s@%dHz %dMHz %s (%d)\n",
		info->active_h, info->active_v,
		info->interlaced ? "i" : "p",
		info->refresh_rate / 1000,
		info->pixel_freq / 1000,
		pinfo->out_format == MDP_Y_CBCR_H2V2 ? "Y420" : "RGB",
		info->video_format);
err:
	return rc;
}

static int hdmi_panel_power_off(void *input)
{
	struct hdmi_panel *panel = input;

	panel->on = false;

	pr_debug("panel off\n");
	return 0;
}

void *hdmi_panel_init(struct hdmi_panel_init_data *data)
{
	struct hdmi_panel *panel = NULL;

	if (!data) {
		pr_err("invalid panel init data\n");
		goto end;
	}

	panel = kzalloc(sizeof(*panel), GFP_KERNEL);
	if (!panel)
		goto end;

	panel->io = data->io;
	panel->ds_data = data->ds_data;
	panel->data = data->panel_data;
	panel->spd_vendor_name = data->spd_vendor_name;
	panel->spd_product_description = data->spd_product_description;
	panel->version = data->version;
	panel->ddc = data->ddc;
	panel->vid_cfg.timing = data->timing;

	if (data->ops) {
		data->ops->on = hdmi_panel_power_on;
		data->ops->off = hdmi_panel_power_off;
		data->ops->vendor = hdmi_panel_set_vendor_specific_infoframe;
		data->ops->update_fps = hdmi_panel_update_fps;
	}
end:
	return panel;
}

void hdmi_panel_deinit(void *input)
{
	struct hdmi_panel *panel = input;

	kfree(panel);
}
