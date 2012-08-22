/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/io.h>
#include <mach/board.h>
#include "mdss_hdmi_util.h"

const char *hdmi_reg_name(u32 offset)
{
	switch (offset) {
	case 0x00000000: return "HDMI_CTRL";
	case 0x00000010: return "HDMI_TEST_PATTERN";
	case 0x00000014: return "HDMI_RANDOM_PATTERN";
	case 0x00000018: return "HDMI_PKT_BLK_CTRL";
	case 0x0000001C: return "HDMI_STATUS";
	case 0x00000020: return "HDMI_AUDIO_PKT_CTRL";
	case 0x00000024: return "HDMI_ACR_PKT_CTRL";
	case 0x00000028: return "HDMI_VBI_PKT_CTRL";
	case 0x0000002C: return "HDMI_INFOFRAME_CTRL0";
	case 0x00000030: return "HDMI_INFOFRAME_CTRL1";
	case 0x00000034: return "HDMI_GEN_PKT_CTRL";
	case 0x0000003C: return "HDMI_ACP";
	case 0x00000040: return "HDMI_GC";
	case 0x00000044: return "HDMI_AUDIO_PKT_CTRL2";
	case 0x00000048: return "HDMI_ISRC1_0";
	case 0x0000004C: return "HDMI_ISRC1_1";
	case 0x00000050: return "HDMI_ISRC1_2";
	case 0x00000054: return "HDMI_ISRC1_3";
	case 0x00000058: return "HDMI_ISRC1_4";
	case 0x0000005C: return "HDMI_ISRC2_0";
	case 0x00000060: return "HDMI_ISRC2_1";
	case 0x00000064: return "HDMI_ISRC2_2";
	case 0x00000068: return "HDMI_ISRC2_3";
	case 0x0000006C: return "HDMI_AVI_INFO0";
	case 0x00000070: return "HDMI_AVI_INFO1";
	case 0x00000074: return "HDMI_AVI_INFO2";
	case 0x00000078: return "HDMI_AVI_INFO3";
	case 0x0000007C: return "HDMI_MPEG_INFO0";
	case 0x00000080: return "HDMI_MPEG_INFO1";
	case 0x00000084: return "HDMI_GENERIC0_HDR";
	case 0x00000088: return "HDMI_GENERIC0_0";
	case 0x0000008C: return "HDMI_GENERIC0_1";
	case 0x00000090: return "HDMI_GENERIC0_2";
	case 0x00000094: return "HDMI_GENERIC0_3";
	case 0x00000098: return "HDMI_GENERIC0_4";
	case 0x0000009C: return "HDMI_GENERIC0_5";
	case 0x000000A0: return "HDMI_GENERIC0_6";
	case 0x000000A4: return "HDMI_GENERIC1_HDR";
	case 0x000000A8: return "HDMI_GENERIC1_0";
	case 0x000000AC: return "HDMI_GENERIC1_1";
	case 0x000000B0: return "HDMI_GENERIC1_2";
	case 0x000000B4: return "HDMI_GENERIC1_3";
	case 0x000000B8: return "HDMI_GENERIC1_4";
	case 0x000000BC: return "HDMI_GENERIC1_5";
	case 0x000000C0: return "HDMI_GENERIC1_6";
	case 0x000000C4: return "HDMI_ACR_32_0";
	case 0x000000C8: return "HDMI_ACR_32_1";
	case 0x000000CC: return "HDMI_ACR_44_0";
	case 0x000000D0: return "HDMI_ACR_44_1";
	case 0x000000D4: return "HDMI_ACR_48_0";
	case 0x000000D8: return "HDMI_ACR_48_1";
	case 0x000000DC: return "HDMI_ACR_STATUS_0";
	case 0x000000E0: return "HDMI_ACR_STATUS_1";
	case 0x000000E4: return "HDMI_AUDIO_INFO0";
	case 0x000000E8: return "HDMI_AUDIO_INFO1";
	case 0x000000EC: return "HDMI_CS_60958_0";
	case 0x000000F0: return "HDMI_CS_60958_1";
	case 0x000000F8: return "HDMI_RAMP_CTRL0";
	case 0x000000FC: return "HDMI_RAMP_CTRL1";
	case 0x00000100: return "HDMI_RAMP_CTRL2";
	case 0x00000104: return "HDMI_RAMP_CTRL3";
	case 0x00000108: return "HDMI_CS_60958_2";
	case 0x00000110: return "HDMI_HDCP_CTRL";
	case 0x00000114: return "HDMI_HDCP_DEBUG_CTRL";
	case 0x00000118: return "HDMI_HDCP_INT_CTRL";
	case 0x0000011C: return "HDMI_HDCP_LINK0_STATUS";
	case 0x00000120: return "HDMI_HDCP_DDC_CTRL_0";
	case 0x00000124: return "HDMI_HDCP_DDC_CTRL_1";
	case 0x00000128: return "HDMI_HDCP_DDC_STATUS";
	case 0x0000012C: return "HDMI_HDCP_ENTROPY_CTRL0";
	case 0x00000130: return "HDMI_HDCP_RESET";
	case 0x00000134: return "HDMI_HDCP_RCVPORT_DATA0";
	case 0x00000138: return "HDMI_HDCP_RCVPORT_DATA1";
	case 0x0000013C: return "HDMI_HDCP_RCVPORT_DATA2_0";
	case 0x00000140: return "HDMI_HDCP_RCVPORT_DATA2_1";
	case 0x00000144: return "HDMI_HDCP_RCVPORT_DATA3";
	case 0x00000148: return "HDMI_HDCP_RCVPORT_DATA4";
	case 0x0000014C: return "HDMI_HDCP_RCVPORT_DATA5";
	case 0x00000150: return "HDMI_HDCP_RCVPORT_DATA6";
	case 0x00000154: return "HDMI_HDCP_RCVPORT_DATA7";
	case 0x00000158: return "HDMI_HDCP_RCVPORT_DATA8";
	case 0x0000015C: return "HDMI_HDCP_RCVPORT_DATA9";
	case 0x00000160: return "HDMI_HDCP_RCVPORT_DATA10";
	case 0x00000164: return "HDMI_HDCP_RCVPORT_DATA11";
	case 0x00000168: return "HDMI_HDCP_RCVPORT_DATA12";
	case 0x0000016C: return "HDMI_VENSPEC_INFO0";
	case 0x00000170: return "HDMI_VENSPEC_INFO1";
	case 0x00000174: return "HDMI_VENSPEC_INFO2";
	case 0x00000178: return "HDMI_VENSPEC_INFO3";
	case 0x0000017C: return "HDMI_VENSPEC_INFO4";
	case 0x00000180: return "HDMI_VENSPEC_INFO5";
	case 0x00000184: return "HDMI_VENSPEC_INFO6";
	case 0x00000194: return "HDMI_HDCP_DEBUG";
	case 0x0000019C: return "HDMI_TMDS_CTRL_CHAR";
	case 0x000001A4: return "HDMI_TMDS_CTRL_SEL";
	case 0x000001A8: return "HDMI_TMDS_SYNCCHAR01";
	case 0x000001AC: return "HDMI_TMDS_SYNCCHAR23";
	case 0x000001B4: return "HDMI_TMDS_DEBUG";
	case 0x000001B8: return "HDMI_TMDS_CTL_BITS";
	case 0x000001BC: return "HDMI_TMDS_DCBAL_CTRL";
	case 0x000001C0: return "HDMI_TMDS_DCBAL_CHAR";
	case 0x000001C8: return "HDMI_TMDS_CTL01_GEN";
	case 0x000001CC: return "HDMI_TMDS_CTL23_GEN";
	case 0x000001D0: return "HDMI_AUDIO_CFG";
	case 0x00000204: return "HDMI_DEBUG";
	case 0x00000208: return "HDMI_USEC_REFTIMER";
	case 0x0000020C: return "HDMI_DDC_CTRL";
	case 0x00000210: return "HDMI_DDC_ARBITRATION";
	case 0x00000214: return "HDMI_DDC_INT_CTRL";
	case 0x00000218: return "HDMI_DDC_SW_STATUS";
	case 0x0000021C: return "HDMI_DDC_HW_STATUS";
	case 0x00000220: return "HDMI_DDC_SPEED";
	case 0x00000224: return "HDMI_DDC_SETUP";
	case 0x00000228: return "HDMI_DDC_TRANS0";
	case 0x0000022C: return "HDMI_DDC_TRANS1";
	case 0x00000230: return "HDMI_DDC_TRANS2";
	case 0x00000234: return "HDMI_DDC_TRANS3";
	case 0x00000238: return "HDMI_DDC_DATA";
	case 0x0000023C: return "HDMI_HDCP_SHA_CTRL";
	case 0x00000240: return "HDMI_HDCP_SHA_STATUS";
	case 0x00000244: return "HDMI_HDCP_SHA_DATA";
	case 0x00000248: return "HDMI_HDCP_SHA_DBG_M0_0";
	case 0x0000024C: return "HDMI_HDCP_SHA_DBG_M0_1";
	case 0x00000250: return "HDMI_HPD_INT_STATUS";
	case 0x00000254: return "HDMI_HPD_INT_CTRL";
	case 0x00000258: return "HDMI_HPD_CTRL";
	case 0x0000025C: return "HDMI_HDCP_ENTROPY_CTRL1";
	case 0x00000260: return "HDMI_HDCP_SW_UPPER_AN";
	case 0x00000264: return "HDMI_HDCP_SW_LOWER_AN";
	case 0x00000268: return "HDMI_CRC_CTRL";
	case 0x0000026C: return "HDMI_VID_CRC";
	case 0x00000270: return "HDMI_AUD_CRC";
	case 0x00000274: return "HDMI_VBI_CRC";
	case 0x0000027C: return "HDMI_DDC_REF";
	case 0x00000284: return "HDMI_HDCP_SW_UPPER_AKSV";
	case 0x00000288: return "HDMI_HDCP_SW_LOWER_AKSV";
	case 0x0000028C: return "HDMI_CEC_CTRL";
	case 0x00000290: return "HDMI_CEC_WR_DATA";
	case 0x00000294: return "HDMI_CEC_RETRANSMIT";
	case 0x00000298: return "HDMI_CEC_STATUS";
	case 0x0000029C: return "HDMI_CEC_INT";
	case 0x000002A0: return "HDMI_CEC_ADDR";
	case 0x000002A4: return "HDMI_CEC_TIME";
	case 0x000002A8: return "HDMI_CEC_REFTIMER";
	case 0x000002AC: return "HDMI_CEC_RD_DATA";
	case 0x000002B0: return "HDMI_CEC_RD_FILTER";
	case 0x000002B4: return "HDMI_ACTIVE_H";
	case 0x000002B8: return "HDMI_ACTIVE_V";
	case 0x000002BC: return "HDMI_ACTIVE_V_F2";
	case 0x000002C0: return "HDMI_TOTAL";
	case 0x000002C4: return "HDMI_V_TOTAL_F2";
	case 0x000002C8: return "HDMI_FRAME_CTRL";
	case 0x000002CC: return "HDMI_AUD_INT";
	case 0x000002D0: return "HDMI_DEBUG_BUS_CTRL";
	case 0x000002D4: return "HDMI_PHY_CTRL";
	case 0x000002DC: return "HDMI_CEC_WR_RANGE";
	case 0x000002E0: return "HDMI_CEC_RD_RANGE";
	case 0x000002E4: return "HDMI_VERSION";
	case 0x000002F4: return "HDMI_BIST_ENABLE";
	case 0x000002F8: return "HDMI_TIMING_ENGINE_EN";
	case 0x000002FC: return "HDMI_INTF_CONFIG";
	case 0x00000300: return "HDMI_HSYNC_CTL";
	case 0x00000304: return "HDMI_VSYNC_PERIOD_F0";
	case 0x00000308: return "HDMI_VSYNC_PERIOD_F1";
	case 0x0000030C: return "HDMI_VSYNC_PULSE_WIDTH_F0";
	case 0x00000310: return "HDMI_VSYNC_PULSE_WIDTH_F1";
	case 0x00000314: return "HDMI_DISPLAY_V_START_F0";
	case 0x00000318: return "HDMI_DISPLAY_V_START_F1";
	case 0x0000031C: return "HDMI_DISPLAY_V_END_F0";
	case 0x00000320: return "HDMI_DISPLAY_V_END_F1";
	case 0x00000324: return "HDMI_ACTIVE_V_START_F0";
	case 0x00000328: return "HDMI_ACTIVE_V_START_F1";
	case 0x0000032C: return "HDMI_ACTIVE_V_END_F0";
	case 0x00000330: return "HDMI_ACTIVE_V_END_F1";
	case 0x00000334: return "HDMI_DISPLAY_HCTL";
	case 0x00000338: return "HDMI_ACTIVE_HCTL";
	case 0x0000033C: return "HDMI_HSYNC_SKEW";
	case 0x00000340: return "HDMI_POLARITY_CTL";
	case 0x00000344: return "HDMI_TPG_MAIN_CONTROL";
	case 0x00000348: return "HDMI_TPG_VIDEO_CONFIG";
	case 0x0000034C: return "HDMI_TPG_COMPONENT_LIMITS";
	case 0x00000350: return "HDMI_TPG_RECTANGLE";
	case 0x00000354: return "HDMI_TPG_INITIAL_VALUE";
	case 0x00000358: return "HDMI_TPG_BLK_WHT_PATTERN_FRAMES";
	case 0x0000035C: return "HDMI_TPG_RGB_MAPPING";
	default: return "???";
	}
} /* hdmi_reg_name */

void hdmi_reg_w(void __iomem *addr, u32 offset, u32 value, u32 debug)
{
	u32 in_val;

	writel_relaxed(value, addr+offset);
	if (debug && PORT_DEBUG) {
		in_val = readl_relaxed(addr+offset);
		DEV_DBG("HDMI[%04x] => %08x [%08x] %s\n", offset, value,
			in_val, hdmi_reg_name(offset));
	}
} /* hdmi_reg_w */

u32 hdmi_reg_r(void __iomem *addr, u32 offset, u32 debug)
{
	u32 value = readl_relaxed(addr+offset);
	if (debug && PORT_DEBUG)
		DEV_DBG("HDMI[%04x] <= %08x %s\n", offset, value,
			hdmi_reg_name(offset));
	return value;
} /* hdmi_reg_r */

void hdmi_reg_dump(void __iomem *base, u32 length, const char *prefix)
{
	if (REG_DUMP)
		print_hex_dump(KERN_INFO, prefix, DUMP_PREFIX_OFFSET, 32, 4,
			(void *)base, length, false);
} /* hdmi_reg_dump */

static struct hdmi_disp_mode_timing_type
	hdmi_supported_video_mode_lut[HDMI_VFRMT_MAX] = {
	HDMI_SETTINGS_640x480p60_4_3,
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x480p60_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x480p60_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1280x720p60_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1920x1080i60_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x480i60_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x480i60_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x240p60_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x240p60_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x480i60_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x480i60_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x240p60_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x240p60_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x480p60_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x480p60_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1920x1080p60_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x576p50_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x576p50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1280x720p50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1920x1080i50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x576i50_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x576i50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x288p50_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x288p50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x576i50_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x576i50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x288p50_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x288p50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x576p50_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x576p50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1920x1080p50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1920x1080p24_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1920x1080p25_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1920x1080p30_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x480p60_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x480p60_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x576p50_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_2880x576p50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1920x1250i50_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1920x1080i100_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1280x720p100_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x576p100_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x576p100_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x576i100_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x576i100_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1920x1080i120_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1280x720p120_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x480p120_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x480p120_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x480i120_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x480i120_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x576p200_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x576p200_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x576i200_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x576i200_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x480p240_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_720x480p240_16_9),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x480i240_4_3),
	VFRMT_NOT_SUPPORTED(HDMI_VFRMT_1440x480i240_16_9),
}; /* hdmi_supported_video_mode_lut */

#define HDMI_SETUP_LUT(MODE) do {					\
	struct hdmi_disp_mode_timing_type mode = HDMI_SETTINGS_##MODE;	\
	hdmi_supported_video_mode_lut[mode.video_format] = mode;	\
	} while (0)

const struct hdmi_disp_mode_timing_type *hdmi_get_supported_mode(u32 mode)
{
	const struct hdmi_disp_mode_timing_type *ret = NULL;

	if (mode >= HDMI_VFRMT_MAX)
		return NULL;

	ret = &hdmi_supported_video_mode_lut[mode];

	if (ret == NULL || !ret->supported)
		return NULL;

	return ret;
} /* hdmi_get_supported_mode */

void hdmi_set_supported_mode(u32 mode)
{
	switch (mode) {
	case HDMI_VFRMT_640x480p60_4_3:
		HDMI_SETUP_LUT(640x480p60_4_3);
		break;
	case HDMI_VFRMT_720x480p60_4_3:
		HDMI_SETUP_LUT(720x480p60_4_3);
		break;
	case HDMI_VFRMT_720x480p60_16_9:
		HDMI_SETUP_LUT(720x480p60_16_9);
		break;
	case HDMI_VFRMT_720x576p50_4_3:
		HDMI_SETUP_LUT(720x576p50_4_3);
		break;
	case HDMI_VFRMT_720x576p50_16_9:
		HDMI_SETUP_LUT(720x576p50_16_9);
		break;
	case HDMI_VFRMT_1440x480i60_4_3:
		HDMI_SETUP_LUT(1440x480i60_4_3);
		break;
	case HDMI_VFRMT_1440x480i60_16_9:
		HDMI_SETUP_LUT(1440x480i60_16_9);
		break;
	case HDMI_VFRMT_1440x576i50_4_3:
		HDMI_SETUP_LUT(1440x576i50_4_3);
		break;
	case HDMI_VFRMT_1440x576i50_16_9:
		HDMI_SETUP_LUT(1440x576i50_16_9);
		break;
	case HDMI_VFRMT_1280x720p50_16_9:
		HDMI_SETUP_LUT(1280x720p50_16_9);
		break;
	case HDMI_VFRMT_1280x720p60_16_9:
		HDMI_SETUP_LUT(1280x720p60_16_9);
		break;
	case HDMI_VFRMT_1920x1080p24_16_9:
		HDMI_SETUP_LUT(1920x1080p24_16_9);
		break;
	case HDMI_VFRMT_1920x1080p25_16_9:
		HDMI_SETUP_LUT(1920x1080p25_16_9);
		break;
	case HDMI_VFRMT_1920x1080p30_16_9:
		HDMI_SETUP_LUT(1920x1080p30_16_9);
		break;
	case HDMI_VFRMT_1920x1080p50_16_9:
		HDMI_SETUP_LUT(1920x1080p50_16_9);
		break;
	case HDMI_VFRMT_1920x1080i60_16_9:
		HDMI_SETUP_LUT(1920x1080i60_16_9);
		break;
	case HDMI_VFRMT_1920x1080p60_16_9:
		HDMI_SETUP_LUT(1920x1080p60_16_9);
		break;
	default:
		DEV_ERR("%s: unsupported mode=%d\n", __func__, mode);
	}
} /* hdmi_set_supported_mode */

const char *hdmi_get_video_fmt_2string(u32 format)
{
	switch (format) {
	case HDMI_VFRMT_640x480p60_4_3:    return " 640x 480 p60  4/3";
	case HDMI_VFRMT_720x480p60_4_3:    return " 720x 480 p60  4/3";
	case HDMI_VFRMT_720x480p60_16_9:   return " 720x 480 p60 16/9";
	case HDMI_VFRMT_1280x720p60_16_9:  return "1280x 720 p60 16/9";
	case HDMI_VFRMT_1920x1080i60_16_9: return "1920x1080 i60 16/9";
	case HDMI_VFRMT_1440x480i60_4_3:   return "1440x 480 i60  4/3";
	case HDMI_VFRMT_1440x480i60_16_9:  return "1440x 480 i60 16/9";
	case HDMI_VFRMT_1440x240p60_4_3:   return "1440x 240 p60  4/3";
	case HDMI_VFRMT_1440x240p60_16_9:  return "1440x 240 p60 16/9";
	case HDMI_VFRMT_2880x480i60_4_3:   return "2880x 480 i60  4/3";
	case HDMI_VFRMT_2880x480i60_16_9:  return "2880x 480 i60 16/9";
	case HDMI_VFRMT_2880x240p60_4_3:   return "2880x 240 p60  4/3";
	case HDMI_VFRMT_2880x240p60_16_9:  return "2880x 240 p60 16/9";
	case HDMI_VFRMT_1440x480p60_4_3:   return "1440x 480 p60  4/3";
	case HDMI_VFRMT_1440x480p60_16_9:  return "1440x 480 p60 16/9";
	case HDMI_VFRMT_1920x1080p60_16_9: return "1920x1080 p60 16/9";
	case HDMI_VFRMT_720x576p50_4_3:    return " 720x 576 p50  4/3";
	case HDMI_VFRMT_720x576p50_16_9:   return " 720x 576 p50 16/9";
	case HDMI_VFRMT_1280x720p50_16_9:  return "1280x 720 p50 16/9";
	case HDMI_VFRMT_1920x1080i50_16_9: return "1920x1080 i50 16/9";
	case HDMI_VFRMT_1440x576i50_4_3:   return "1440x 576 i50  4/3";
	case HDMI_VFRMT_1440x576i50_16_9:  return "1440x 576 i50 16/9";
	case HDMI_VFRMT_1440x288p50_4_3:   return "1440x 288 p50  4/3";
	case HDMI_VFRMT_1440x288p50_16_9:  return "1440x 288 p50 16/9";
	case HDMI_VFRMT_2880x576i50_4_3:   return "2880x 576 i50  4/3";
	case HDMI_VFRMT_2880x576i50_16_9:  return "2880x 576 i50 16/9";
	case HDMI_VFRMT_2880x288p50_4_3:   return "2880x 288 p50  4/3";
	case HDMI_VFRMT_2880x288p50_16_9:  return "2880x 288 p50 16/9";
	case HDMI_VFRMT_1440x576p50_4_3:   return "1440x 576 p50  4/3";
	case HDMI_VFRMT_1440x576p50_16_9:  return "1440x 576 p50 16/9";
	case HDMI_VFRMT_1920x1080p50_16_9: return "1920x1080 p50 16/9";
	case HDMI_VFRMT_1920x1080p24_16_9: return "1920x1080 p24 16/9";
	case HDMI_VFRMT_1920x1080p25_16_9: return "1920x1080 p25 16/9";
	case HDMI_VFRMT_1920x1080p30_16_9: return "1920x1080 p30 16/9";
	case HDMI_VFRMT_2880x480p60_4_3:   return "2880x 480 p60  4/3";
	case HDMI_VFRMT_2880x480p60_16_9:  return "2880x 480 p60 16/9";
	case HDMI_VFRMT_2880x576p50_4_3:   return "2880x 576 p50  4/3";
	case HDMI_VFRMT_2880x576p50_16_9:  return "2880x 576 p50 16/9";
	case HDMI_VFRMT_1920x1250i50_16_9: return "1920x1250 i50 16/9";
	case HDMI_VFRMT_1920x1080i100_16_9:return "1920x1080 i100 16/9";
	case HDMI_VFRMT_1280x720p100_16_9: return "1280x 720 p100 16/9";
	case HDMI_VFRMT_720x576p100_4_3:   return " 720x 576 p100  4/3";
	case HDMI_VFRMT_720x576p100_16_9:  return " 720x 576 p100 16/9";
	case HDMI_VFRMT_1440x576i100_4_3:  return "1440x 576 i100  4/3";
	case HDMI_VFRMT_1440x576i100_16_9: return "1440x 576 i100 16/9";
	case HDMI_VFRMT_1920x1080i120_16_9:return "1920x1080 i120 16/9";
	case HDMI_VFRMT_1280x720p120_16_9: return "1280x 720 p120 16/9";
	case HDMI_VFRMT_720x480p120_4_3:   return " 720x 480 p120  4/3";
	case HDMI_VFRMT_720x480p120_16_9:  return " 720x 480 p120 16/9";
	case HDMI_VFRMT_1440x480i120_4_3:  return "1440x 480 i120  4/3";
	case HDMI_VFRMT_1440x480i120_16_9: return "1440x 480 i120 16/9";
	case HDMI_VFRMT_720x576p200_4_3:   return " 720x 576 p200  4/3";
	case HDMI_VFRMT_720x576p200_16_9:  return " 720x 576 p200 16/9";
	case HDMI_VFRMT_1440x576i200_4_3:  return "1440x 576 i200  4/3";
	case HDMI_VFRMT_1440x576i200_16_9: return "1440x 576 i200 16/9";
	case HDMI_VFRMT_720x480p240_4_3:   return " 720x 480 p240  4/3";
	case HDMI_VFRMT_720x480p240_16_9:  return " 720x 480 p240 16/9";
	case HDMI_VFRMT_1440x480i240_4_3:  return "1440x 480 i240  4/3";
	case HDMI_VFRMT_1440x480i240_16_9: return "1440x 480 i240 16/9";
	default:                           return "???";
	}
} /* hdmi_get_video_fmt_2string */

const char *hdmi_get_single_video_3d_fmt_2string(u32 format)
{
	switch (format) {
	case TOP_AND_BOTTOM: return "TAB";
	case FRAME_PACKING: return "FP";
	case SIDE_BY_SIDE_HALF: return "SSH";
	}
	return "";
} /* hdmi_get_single_video_3d_fmt_2string */

ssize_t hdmi_get_video_3d_fmt_2string(u32 format, char *buf)
{
	ssize_t ret, len = 0;
	ret = snprintf(buf, PAGE_SIZE, "%s",
		hdmi_get_single_video_3d_fmt_2string(
			format & FRAME_PACKING));
	len += ret;

	if (len && (format & TOP_AND_BOTTOM))
		ret = snprintf(buf + len, PAGE_SIZE, ":%s",
			hdmi_get_single_video_3d_fmt_2string(
				format & TOP_AND_BOTTOM));
	else
		ret = snprintf(buf + len, PAGE_SIZE, "%s",
			hdmi_get_single_video_3d_fmt_2string(
				format & TOP_AND_BOTTOM));
	len += ret;

	if (len && (format & SIDE_BY_SIDE_HALF))
		ret = snprintf(buf + len, PAGE_SIZE, ":%s",
			hdmi_get_single_video_3d_fmt_2string(
				format & SIDE_BY_SIDE_HALF));
	else
		ret = snprintf(buf + len, PAGE_SIZE, "%s",
			hdmi_get_single_video_3d_fmt_2string(
				format & SIDE_BY_SIDE_HALF));
	len += ret;

	return len;
} /* hdmi_get_video_3d_fmt_2string */
