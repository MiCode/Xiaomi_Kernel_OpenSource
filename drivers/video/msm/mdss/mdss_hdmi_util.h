/* Copyright (c) 2010-2014, The Linux Foundation. All rights reserved.
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

#ifndef __HDMI_UTIL_H__
#define __HDMI_UTIL_H__
#include <linux/mdss_io_util.h>
#include "video/msm_hdmi_modes.h"

/* HDMI_TX Registers */
#define HDMI_CTRL                        (0x00000000)
#define HDMI_TEST_PATTERN                (0x00000010)
#define HDMI_RANDOM_PATTERN              (0x00000014)
#define HDMI_PKT_BLK_CTRL                (0x00000018)
#define HDMI_STATUS                      (0x0000001C)
#define HDMI_AUDIO_PKT_CTRL              (0x00000020)
#define HDMI_ACR_PKT_CTRL                (0x00000024)
#define HDMI_VBI_PKT_CTRL                (0x00000028)
#define HDMI_INFOFRAME_CTRL0             (0x0000002C)
#define HDMI_INFOFRAME_CTRL1             (0x00000030)
#define HDMI_GEN_PKT_CTRL                (0x00000034)
#define HDMI_ACP                         (0x0000003C)
#define HDMI_GC                          (0x00000040)
#define HDMI_AUDIO_PKT_CTRL2             (0x00000044)
#define HDMI_ISRC1_0                     (0x00000048)
#define HDMI_ISRC1_1                     (0x0000004C)
#define HDMI_ISRC1_2                     (0x00000050)
#define HDMI_ISRC1_3                     (0x00000054)
#define HDMI_ISRC1_4                     (0x00000058)
#define HDMI_ISRC2_0                     (0x0000005C)
#define HDMI_ISRC2_1                     (0x00000060)
#define HDMI_ISRC2_2                     (0x00000064)
#define HDMI_ISRC2_3                     (0x00000068)
#define HDMI_AVI_INFO0                   (0x0000006C)
#define HDMI_AVI_INFO1                   (0x00000070)
#define HDMI_AVI_INFO2                   (0x00000074)
#define HDMI_AVI_INFO3                   (0x00000078)
#define HDMI_MPEG_INFO0                  (0x0000007C)
#define HDMI_MPEG_INFO1                  (0x00000080)
#define HDMI_GENERIC0_HDR                (0x00000084)
#define HDMI_GENERIC0_0                  (0x00000088)
#define HDMI_GENERIC0_1                  (0x0000008C)
#define HDMI_GENERIC0_2                  (0x00000090)
#define HDMI_GENERIC0_3                  (0x00000094)
#define HDMI_GENERIC0_4                  (0x00000098)
#define HDMI_GENERIC0_5                  (0x0000009C)
#define HDMI_GENERIC0_6                  (0x000000A0)
#define HDMI_GENERIC1_HDR                (0x000000A4)
#define HDMI_GENERIC1_0                  (0x000000A8)
#define HDMI_GENERIC1_1                  (0x000000AC)
#define HDMI_GENERIC1_2                  (0x000000B0)
#define HDMI_GENERIC1_3                  (0x000000B4)
#define HDMI_GENERIC1_4                  (0x000000B8)
#define HDMI_GENERIC1_5                  (0x000000BC)
#define HDMI_GENERIC1_6                  (0x000000C0)
#define HDMI_ACR_32_0                    (0x000000C4)
#define HDMI_ACR_32_1                    (0x000000C8)
#define HDMI_ACR_44_0                    (0x000000CC)
#define HDMI_ACR_44_1                    (0x000000D0)
#define HDMI_ACR_48_0                    (0x000000D4)
#define HDMI_ACR_48_1                    (0x000000D8)
#define HDMI_ACR_STATUS_0                (0x000000DC)
#define HDMI_ACR_STATUS_1                (0x000000E0)
#define HDMI_AUDIO_INFO0                 (0x000000E4)
#define HDMI_AUDIO_INFO1                 (0x000000E8)
#define HDMI_CS_60958_0                  (0x000000EC)
#define HDMI_CS_60958_1                  (0x000000F0)
#define HDMI_RAMP_CTRL0                  (0x000000F8)
#define HDMI_RAMP_CTRL1                  (0x000000FC)
#define HDMI_RAMP_CTRL2                  (0x00000100)
#define HDMI_RAMP_CTRL3                  (0x00000104)
#define HDMI_CS_60958_2                  (0x00000108)
#define HDMI_HDCP_CTRL2                  (0x0000010C)
#define HDMI_HDCP_CTRL                   (0x00000110)
#define HDMI_HDCP_DEBUG_CTRL             (0x00000114)
#define HDMI_HDCP_INT_CTRL               (0x00000118)
#define HDMI_HDCP_LINK0_STATUS           (0x0000011C)
#define HDMI_HDCP_DDC_CTRL_0             (0x00000120)
#define HDMI_HDCP_DDC_CTRL_1             (0x00000124)
#define HDMI_HDCP_DDC_STATUS             (0x00000128)
#define HDMI_HDCP_ENTROPY_CTRL0          (0x0000012C)
#define HDMI_HDCP_RESET                  (0x00000130)
#define HDMI_HDCP_RCVPORT_DATA0          (0x00000134)
#define HDMI_HDCP_RCVPORT_DATA1          (0x00000138)
#define HDMI_HDCP_RCVPORT_DATA2_0        (0x0000013C)
#define HDMI_HDCP_RCVPORT_DATA2_1        (0x00000140)
#define HDMI_HDCP_RCVPORT_DATA3          (0x00000144)
#define HDMI_HDCP_RCVPORT_DATA4          (0x00000148)
#define HDMI_HDCP_RCVPORT_DATA5          (0x0000014C)
#define HDMI_HDCP_RCVPORT_DATA6          (0x00000150)
#define HDMI_HDCP_RCVPORT_DATA7          (0x00000154)
#define HDMI_HDCP_RCVPORT_DATA8          (0x00000158)
#define HDMI_HDCP_RCVPORT_DATA9          (0x0000015C)
#define HDMI_HDCP_RCVPORT_DATA10         (0x00000160)
#define HDMI_HDCP_RCVPORT_DATA11         (0x00000164)
#define HDMI_HDCP_RCVPORT_DATA12         (0x00000168)
#define HDMI_VENSPEC_INFO0               (0x0000016C)
#define HDMI_VENSPEC_INFO1               (0x00000170)
#define HDMI_VENSPEC_INFO2               (0x00000174)
#define HDMI_VENSPEC_INFO3               (0x00000178)
#define HDMI_VENSPEC_INFO4               (0x0000017C)
#define HDMI_VENSPEC_INFO5               (0x00000180)
#define HDMI_VENSPEC_INFO6               (0x00000184)
#define HDMI_HDCP_DEBUG                  (0x00000194)
#define HDMI_TMDS_CTRL_CHAR              (0x0000019C)
#define HDMI_TMDS_CTRL_SEL               (0x000001A4)
#define HDMI_TMDS_SYNCCHAR01             (0x000001A8)
#define HDMI_TMDS_SYNCCHAR23             (0x000001AC)
#define HDMI_TMDS_DEBUG                  (0x000001B4)
#define HDMI_TMDS_CTL_BITS               (0x000001B8)
#define HDMI_TMDS_DCBAL_CTRL             (0x000001BC)
#define HDMI_TMDS_DCBAL_CHAR             (0x000001C0)
#define HDMI_TMDS_CTL01_GEN              (0x000001C8)
#define HDMI_TMDS_CTL23_GEN              (0x000001CC)
#define HDMI_AUDIO_CFG                   (0x000001D0)
#define HDMI_DEBUG                       (0x00000204)
#define HDMI_USEC_REFTIMER               (0x00000208)
#define HDMI_DDC_CTRL                    (0x0000020C)
#define HDMI_DDC_ARBITRATION             (0x00000210)
#define HDMI_DDC_INT_CTRL                (0x00000214)
#define HDMI_DDC_SW_STATUS               (0x00000218)
#define HDMI_DDC_HW_STATUS               (0x0000021C)
#define HDMI_DDC_SPEED                   (0x00000220)
#define HDMI_DDC_SETUP                   (0x00000224)
#define HDMI_DDC_TRANS0                  (0x00000228)
#define HDMI_DDC_TRANS1                  (0x0000022C)
#define HDMI_DDC_TRANS2                  (0x00000230)
#define HDMI_DDC_TRANS3                  (0x00000234)
#define HDMI_DDC_DATA                    (0x00000238)
#define HDMI_HDCP_SHA_CTRL               (0x0000023C)
#define HDMI_HDCP_SHA_STATUS             (0x00000240)
#define HDMI_HDCP_SHA_DATA               (0x00000244)
#define HDMI_HDCP_SHA_DBG_M0_0           (0x00000248)
#define HDMI_HDCP_SHA_DBG_M0_1           (0x0000024C)
#define HDMI_HPD_INT_STATUS              (0x00000250)
#define HDMI_HPD_INT_CTRL                (0x00000254)
#define HDMI_HPD_CTRL                    (0x00000258)
#define HDMI_HDCP_ENTROPY_CTRL1          (0x0000025C)
#define HDMI_HDCP_SW_UPPER_AN            (0x00000260)
#define HDMI_HDCP_SW_LOWER_AN            (0x00000264)
#define HDMI_CRC_CTRL                    (0x00000268)
#define HDMI_VID_CRC                     (0x0000026C)
#define HDMI_AUD_CRC                     (0x00000270)
#define HDMI_VBI_CRC                     (0x00000274)
#define HDMI_DDC_REF                     (0x0000027C)
#define HDMI_HDCP_SW_UPPER_AKSV          (0x00000284)
#define HDMI_HDCP_SW_LOWER_AKSV          (0x00000288)
#define HDMI_CEC_CTRL                    (0x0000028C)
#define HDMI_CEC_WR_DATA                 (0x00000290)
#define HDMI_CEC_RETRANSMIT              (0x00000294)
#define HDMI_CEC_STATUS                  (0x00000298)
#define HDMI_CEC_INT                     (0x0000029C)
#define HDMI_CEC_ADDR                    (0x000002A0)
#define HDMI_CEC_TIME                    (0x000002A4)
#define HDMI_CEC_REFTIMER                (0x000002A8)
#define HDMI_CEC_RD_DATA                 (0x000002AC)
#define HDMI_CEC_RD_FILTER               (0x000002B0)
#define HDMI_ACTIVE_H                    (0x000002B4)
#define HDMI_ACTIVE_V                    (0x000002B8)
#define HDMI_ACTIVE_V_F2                 (0x000002BC)
#define HDMI_TOTAL                       (0x000002C0)
#define HDMI_V_TOTAL_F2                  (0x000002C4)
#define HDMI_FRAME_CTRL                  (0x000002C8)
#define HDMI_AUD_INT                     (0x000002CC)
#define HDMI_DEBUG_BUS_CTRL              (0x000002D0)
#define HDMI_PHY_CTRL                    (0x000002D4)
#define HDMI_CEC_WR_RANGE                (0x000002DC)
#define HDMI_CEC_RD_RANGE                (0x000002E0)
#define HDMI_VERSION                     (0x000002E4)
#define HDMI_BIST_ENABLE                 (0x000002F4)
#define HDMI_TIMING_ENGINE_EN            (0x000002F8)
#define HDMI_INTF_CONFIG                 (0x000002FC)
#define HDMI_HSYNC_CTL                   (0x00000300)
#define HDMI_VSYNC_PERIOD_F0             (0x00000304)
#define HDMI_VSYNC_PERIOD_F1             (0x00000308)
#define HDMI_VSYNC_PULSE_WIDTH_F0        (0x0000030C)
#define HDMI_VSYNC_PULSE_WIDTH_F1        (0x00000310)
#define HDMI_DISPLAY_V_START_F0          (0x00000314)
#define HDMI_DISPLAY_V_START_F1          (0x00000318)
#define HDMI_DISPLAY_V_END_F0            (0x0000031C)
#define HDMI_DISPLAY_V_END_F1            (0x00000320)
#define HDMI_ACTIVE_V_START_F0           (0x00000324)
#define HDMI_ACTIVE_V_START_F1           (0x00000328)
#define HDMI_ACTIVE_V_END_F0             (0x0000032C)
#define HDMI_ACTIVE_V_END_F1             (0x00000330)
#define HDMI_DISPLAY_HCTL                (0x00000334)
#define HDMI_ACTIVE_HCTL                 (0x00000338)
#define HDMI_HSYNC_SKEW                  (0x0000033C)
#define HDMI_POLARITY_CTL                (0x00000340)
#define HDMI_TPG_MAIN_CONTROL            (0x00000344)
#define HDMI_TPG_VIDEO_CONFIG            (0x00000348)
#define HDMI_TPG_COMPONENT_LIMITS        (0x0000034C)
#define HDMI_TPG_RECTANGLE               (0x00000350)
#define HDMI_TPG_INITIAL_VALUE           (0x00000354)
#define HDMI_TPG_BLK_WHT_PATTERN_FRAMES  (0x00000358)
#define HDMI_TPG_RGB_MAPPING             (0x0000035C)
#define HDMI_CEC_COMPL_CTL               (0x00000360)
#define HDMI_CEC_RD_START_RANGE          (0x00000364)
#define HDMI_CEC_RD_TOTAL_RANGE          (0x00000368)
#define HDMI_CEC_RD_ERR_RESP_LO          (0x0000036C)
#define HDMI_CEC_WR_CHECK_CONFIG         (0x00000370)

/* HDMI PHY Registers */
#define HDMI_PHY_ANA_CFG0                (0x00000000)
#define HDMI_PHY_ANA_CFG1                (0x00000004)
#define HDMI_PHY_PD_CTRL0                (0x00000010)
#define HDMI_PHY_PD_CTRL1                (0x00000014)
#define HDMI_PHY_BIST_CFG0               (0x00000034)
#define HDMI_PHY_BIST_PATN0              (0x0000003C)
#define HDMI_PHY_BIST_PATN1              (0x00000040)
#define HDMI_PHY_BIST_PATN2              (0x00000044)
#define HDMI_PHY_BIST_PATN3              (0x00000048)

/* QFPROM Registers for HDMI/HDCP */
#define QFPROM_RAW_FEAT_CONFIG_ROW0_LSB  (0x000000F8)
#define QFPROM_RAW_FEAT_CONFIG_ROW0_MSB  (0x000000FC)
#define HDCP_KSV_LSB                     (0x000060D8)
#define HDCP_KSV_MSB                     (0x000060DC)

#define TOP_AND_BOTTOM		0x10
#define FRAME_PACKING		0x20
#define SIDE_BY_SIDE_HALF	0x40

#define LPASS_LPAIF_RDDMA_CTL0           (0xFE152000)
#define LPASS_LPAIF_RDDMA_PER_CNT0       (0x00000014)

enum hdmi_tx_feature_type {
	HDMI_TX_FEAT_EDID,
	HDMI_TX_FEAT_HDCP,
	HDMI_TX_FEAT_CEC,
	HDMI_TX_FEAT_MAX,
};

struct hdmi_tx_ddc_ctrl {
	struct dss_io_data *io;
	struct completion ddc_sw_done;
};

struct hdmi_tx_ddc_data {
	char *what;
	u8 *data_buf;
	u32 data_len;
	u32 dev_addr;
	u32 offset;
	u32 request_len;
	u32 no_align;
	int retry;
};

/* video timing related utility routines */
void hdmi_setup_video_mode_lut(void);
int hdmi_get_video_id_code(struct msm_hdmi_mode_timing_info *timing_in);
const struct msm_hdmi_mode_timing_info *hdmi_get_supported_mode(u32 mode);
void hdmi_del_supported_mode(u32 mode);
ssize_t hdmi_get_video_3d_fmt_2string(u32 format, char *buf, u32 size);
const char *msm_hdmi_mode_2string(u32 mode);

/* todo: Fix this. Right now this is defined in mdss_hdmi_tx.c */
void *hdmi_get_featuredata_from_sysfs_dev(struct device *device, u32 type);

/* DDC */
void hdmi_ddc_config(struct hdmi_tx_ddc_ctrl *);
int hdmi_ddc_isr(struct hdmi_tx_ddc_ctrl *);
int hdmi_ddc_write(struct hdmi_tx_ddc_ctrl *, struct hdmi_tx_ddc_data *);
int hdmi_ddc_read_seg(struct hdmi_tx_ddc_ctrl *, struct hdmi_tx_ddc_data *);
int hdmi_ddc_read(struct hdmi_tx_ddc_ctrl *, struct hdmi_tx_ddc_data *);

#endif /* __HDMI_UTIL_H__ */
