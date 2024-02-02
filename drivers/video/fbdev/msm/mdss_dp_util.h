/* Copyright (c) 2016-2017, 2020, The Linux Foundation. All rights reserved.
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

#ifndef __DP_UTIL_H__
#define __DP_UTIL_H__

#include "mdss_dp.h"

/* DP_TX Registers */
#define	DP_HW_VERSION				(0x00000000)
#define	DP_SW_RESET				(0x00000010)
#define	DP_PHY_CTRL				(0x00000014)
#define	DP_CLK_CTRL				(0x00000018)
#define	DP_CLK_ACTIVE				(0x0000001C)
#define	DP_INTR_STATUS				(0x00000020)
#define	DP_INTR_STATUS2				(0x00000024)
#define	DP_INTR_STATUS3				(0x00000028)

#define	DP_DP_HPD_CTRL				(0x00000200)
#define	DP_DP_HPD_INT_STATUS			(0x00000204)
#define	DP_DP_HPD_INT_ACK			(0x00000208)
#define	DP_DP_HPD_INT_MASK			(0x0000020C)
#define	DP_DP_HPD_REFTIMER			(0x00000218)
#define	DP_DP_HPD_EVENT_TIME_0			(0x0000021C)
#define	DP_DP_HPD_EVENT_TIME_1			(0x00000220)
#define	DP_AUX_CTRL				(0x00000230)
#define	DP_AUX_DATA				(0x00000234)
#define	DP_AUX_TRANS_CTRL			(0x00000238)
#define	DP_AUX_TIMEOUT_COUNT			(0x0000023C)
#define	DP_AUX_LIMITS				(0x00000240)
#define	DP_AUX_STATUS				(0x00000244)

#define DP_DPCD_CP_IRQ				(0x201)
#define DP_DPCD_RXSTATUS			(0x69493)

#define	DP_INTERRUPT_TRANS_NUM			(0x000002A0)

#define	DP_MAINLINK_CTRL			(0x00000400)
#define	DP_STATE_CTRL				(0x00000404)
#define	DP_CONFIGURATION_CTRL			(0x00000408)
#define	DP_SOFTWARE_MVID			(0x00000410)
#define	DP_SOFTWARE_NVID			(0x00000418)
#define	DP_TOTAL_HOR_VER			(0x0000041C)
#define	DP_START_HOR_VER_FROM_SYNC		(0x00000420)
#define	DP_HSYNC_VSYNC_WIDTH_POLARITY		(0x00000424)
#define	DP_ACTIVE_HOR_VER			(0x00000428)
#define	DP_MISC1_MISC0				(0x0000042C)
#define	DP_VALID_BOUNDARY			(0x00000430)
#define	DP_VALID_BOUNDARY_2			(0x00000434)
#define	DP_LOGICAL2PHYSCIAL_LANE_MAPPING	(0x00000438)

#define	DP_MAINLINK_READY			(0x00000440)
#define DP_MAINLINK_LEVELS			(0x00000444)
#define	DP_TU					(0x0000044C)

#define DP_HBR2_COMPLIANCE_SCRAMBLER_RESET	(0x00000454)
#define DP_TEST_80BIT_CUSTOM_PATTERN_REG0	(0x000004C0)
#define DP_TEST_80BIT_CUSTOM_PATTERN_REG1	(0x000004C4)
#define DP_TEST_80BIT_CUSTOM_PATTERN_REG2	(0x000004C8)

#define MMSS_DP_MISC1_MISC0			(0x0000042C)
#define	MMSS_DP_AUDIO_TIMING_GEN		(0x00000480)
#define	MMSS_DP_AUDIO_TIMING_RBR_32		(0x00000484)
#define	MMSS_DP_AUDIO_TIMING_HBR_32		(0x00000488)
#define	MMSS_DP_AUDIO_TIMING_RBR_44		(0x0000048C)
#define	MMSS_DP_AUDIO_TIMING_HBR_44		(0x00000490)
#define	MMSS_DP_AUDIO_TIMING_RBR_48		(0x00000494)
#define	MMSS_DP_AUDIO_TIMING_HBR_48		(0x00000498)

#define MMSS_DP_PSR_CRC_RG			(0x00000554)
#define MMSS_DP_PSR_CRC_B			(0x00000558)

#define	MMSS_DP_AUDIO_CFG			(0x00000600)
#define	MMSS_DP_AUDIO_STATUS			(0x00000604)
#define	MMSS_DP_AUDIO_PKT_CTRL			(0x00000608)
#define	MMSS_DP_AUDIO_PKT_CTRL2			(0x0000060C)
#define	MMSS_DP_AUDIO_ACR_CTRL			(0x00000610)
#define	MMSS_DP_AUDIO_CTRL_RESET		(0x00000614)

#define	MMSS_DP_SDP_CFG				(0x00000628)
#define	MMSS_DP_SDP_CFG2			(0x0000062C)
#define	MMSS_DP_AUDIO_TIMESTAMP_0		(0x00000630)
#define	MMSS_DP_AUDIO_TIMESTAMP_1		(0x00000634)

#define	MMSS_DP_AUDIO_STREAM_0			(0x00000640)
#define	MMSS_DP_AUDIO_STREAM_1			(0x00000644)

#define	MMSS_DP_EXTENSION_0			(0x00000650)
#define	MMSS_DP_EXTENSION_1			(0x00000654)
#define	MMSS_DP_EXTENSION_2			(0x00000658)
#define	MMSS_DP_EXTENSION_3			(0x0000065C)
#define	MMSS_DP_EXTENSION_4			(0x00000660)
#define	MMSS_DP_EXTENSION_5			(0x00000664)
#define	MMSS_DP_EXTENSION_6			(0x00000668)
#define	MMSS_DP_EXTENSION_7			(0x0000066C)
#define	MMSS_DP_EXTENSION_8			(0x00000670)
#define	MMSS_DP_EXTENSION_9			(0x00000674)
#define	MMSS_DP_AUDIO_COPYMANAGEMENT_0		(0x00000678)
#define	MMSS_DP_AUDIO_COPYMANAGEMENT_1		(0x0000067C)
#define	MMSS_DP_AUDIO_COPYMANAGEMENT_2		(0x00000680)
#define	MMSS_DP_AUDIO_COPYMANAGEMENT_3		(0x00000684)
#define	MMSS_DP_AUDIO_COPYMANAGEMENT_4		(0x00000688)
#define	MMSS_DP_AUDIO_COPYMANAGEMENT_5		(0x0000068C)
#define	MMSS_DP_AUDIO_ISRC_0			(0x00000690)
#define	MMSS_DP_AUDIO_ISRC_1			(0x00000694)
#define	MMSS_DP_AUDIO_ISRC_2			(0x00000698)
#define	MMSS_DP_AUDIO_ISRC_3			(0x0000069C)
#define	MMSS_DP_AUDIO_ISRC_4			(0x000006A0)
#define	MMSS_DP_AUDIO_ISRC_5			(0x000006A4)
#define	MMSS_DP_AUDIO_INFOFRAME_0		(0x000006A8)
#define	MMSS_DP_AUDIO_INFOFRAME_1		(0x000006AC)
#define	MMSS_DP_AUDIO_INFOFRAME_2		(0x000006B0)

#define	MMSS_DP_GENERIC0_0			(0x00000700)
#define	MMSS_DP_GENERIC0_1			(0x00000704)
#define	MMSS_DP_GENERIC0_2			(0x00000708)
#define	MMSS_DP_GENERIC0_3			(0x0000070C)
#define	MMSS_DP_GENERIC0_4			(0x00000710)
#define	MMSS_DP_GENERIC0_5			(0x00000714)
#define	MMSS_DP_GENERIC0_6			(0x00000718)
#define	MMSS_DP_GENERIC0_7			(0x0000071C)
#define	MMSS_DP_GENERIC0_8			(0x00000720)
#define	MMSS_DP_GENERIC0_9			(0x00000724)
#define	MMSS_DP_GENERIC1_0			(0x00000728)
#define	MMSS_DP_GENERIC1_1			(0x0000072C)
#define	MMSS_DP_GENERIC1_2			(0x00000730)
#define	MMSS_DP_GENERIC1_3			(0x00000734)
#define	MMSS_DP_GENERIC1_4			(0x00000738)
#define	MMSS_DP_GENERIC1_5			(0x0000073C)
#define	MMSS_DP_GENERIC1_6			(0x00000740)
#define	MMSS_DP_GENERIC1_7			(0x00000744)
#define	MMSS_DP_GENERIC1_8			(0x00000748)
#define	MMSS_DP_GENERIC1_9			(0x0000074C)

#define MMSS_DP_TIMING_ENGINE_EN		(0x00000A10)

/*DP PHY Register offsets */
#define DP_PHY_REVISION_ID0                     (0x00000000)
#define DP_PHY_REVISION_ID1                     (0x00000004)
#define DP_PHY_REVISION_ID2                     (0x00000008)
#define DP_PHY_REVISION_ID3                     (0x0000000C)

#define DP_PHY_CFG                              (0x00000010)
#define DP_PHY_PD_CTL                           (0x00000014)
#define DP_PHY_MODE                             (0x00000018)

#define DP_PHY_AUX_CFG0                         (0x0000001C)
#define DP_PHY_AUX_CFG1                         (0x00000020)
#define DP_PHY_AUX_CFG2                         (0x00000024)
#define DP_PHY_AUX_CFG3                         (0x00000028)
#define DP_PHY_AUX_CFG4                         (0x0000002C)
#define DP_PHY_AUX_CFG5                         (0x00000030)
#define DP_PHY_AUX_CFG6                         (0x00000034)
#define DP_PHY_AUX_CFG7                         (0x00000038)
#define DP_PHY_AUX_CFG8                         (0x0000003C)
#define DP_PHY_AUX_CFG9                         (0x00000040)
#define DP_PHY_AUX_INTERRUPT_MASK               (0x00000044)
#define DP_PHY_AUX_INTERRUPT_CLEAR              (0x00000048)
#define DP_PHY_AUX_INTERRUPT_STATUS             (0x000000B8)

#define DP_PHY_SPARE0				0x00A8

#define QSERDES_TX0_OFFSET			0x0400
#define QSERDES_TX1_OFFSET			0x0800

#define TXn_TX_EMP_POST1_LVL			0x000C
#define TXn_TX_DRV_LVL				0x001C

#define TCSR_USB3_DP_PHYMODE			0x48
#define EDID_START_ADDRESS			0x50

/* DP MMSS_CC registers */
#define MMSS_DP_LINK_CMD_RCGR			0x0000
#define MMSS_DP_LINK_CFG_RCGR			0x0004
#define MMSS_DP_PIXEL_M				0x0048
#define MMSS_DP_PIXEL_N				0x004C

/* DP HDCP 1.3 registers */
#define DP_HDCP_CTRL                                   (0x0A0)
#define DP_HDCP_STATUS                                 (0x0A4)
#define DP_HDCP_SW_UPPER_AKSV                          (0x298)
#define DP_HDCP_SW_LOWER_AKSV                          (0x29C)
#define DP_HDCP_ENTROPY_CTRL0                          (0x750)
#define DP_HDCP_ENTROPY_CTRL1                          (0x75C)
#define DP_HDCP_SHA_STATUS                             (0x0C8)
#define DP_HDCP_RCVPORT_DATA2_0                        (0x0B0)
#define DP_HDCP_RCVPORT_DATA3                          (0x2A4)
#define DP_HDCP_RCVPORT_DATA4                          (0x2A8)
#define DP_HDCP_RCVPORT_DATA5                          (0x0C0)
#define DP_HDCP_RCVPORT_DATA6                          (0x0C4)

#define HDCP_SEC_DP_TZ_HV_HLOS_HDCP_SHA_CTRL           (0x024)
#define HDCP_SEC_DP_TZ_HV_HLOS_HDCP_SHA_DATA           (0x028)
#define HDCP_SEC_DP_TZ_HV_HLOS_HDCP_RCVPORT_DATA0      (0x004)
#define HDCP_SEC_DP_TZ_HV_HLOS_HDCP_RCVPORT_DATA1      (0x008)
#define HDCP_SEC_DP_TZ_HV_HLOS_HDCP_RCVPORT_DATA7      (0x00C)
#define HDCP_SEC_DP_TZ_HV_HLOS_HDCP_RCVPORT_DATA8      (0x010)
#define HDCP_SEC_DP_TZ_HV_HLOS_HDCP_RCVPORT_DATA9      (0x014)
#define HDCP_SEC_DP_TZ_HV_HLOS_HDCP_RCVPORT_DATA10     (0x018)
#define HDCP_SEC_DP_TZ_HV_HLOS_HDCP_RCVPORT_DATA11     (0x01C)
#define HDCP_SEC_DP_TZ_HV_HLOS_HDCP_RCVPORT_DATA12     (0x020)

struct edp_cmd {
	char read;	/* 1 == read, 0 == write */
	char i2c;	/* 1 == i2c cmd, 0 == native cmd */
	u32 addr;	/* 20 bits */
	char *datap;
	char *out_buf;
	int len;	/* len to be tx OR len to be rx for read */
	char next;	/* next command */
};

struct dp_vc_tu_mapping_table {
	u32 vic;
	u8 lanes;
	u8 lrate; /* DP_LINK_RATE -> 162(6), 270(10), 540(20) */
	u8 bpp;
	u8 valid_boundary_link;
	u16 delay_start_link;
	bool boundary_moderation_en;
	u8 valid_lower_boundary_link;
	u8 upper_boundary_count;
	u8 lower_boundary_count;
	u8 tu_size_minus1;
};

static const struct dp_vc_tu_mapping_table tu_table[] = {
	{HDMI_VFRMT_640x480p60_4_3, 4, 06, 24,
			0x07, 0x0056, false, 0x00, 0x00, 0x00, 0x3b},
	{HDMI_VFRMT_640x480p60_4_3, 2, 06, 24,
			0x0e, 0x004f, false, 0x00, 0x00, 0x00, 0x3b},
	{HDMI_VFRMT_640x480p60_4_3, 1, 06, 24,
			0x15, 0x0039, false, 0x00, 0x00, 0x00, 0x2c},
	{HDMI_VFRMT_720x480p60_4_3, 1, 06, 24,
			0x13, 0x0038, true, 0x12, 0x0c, 0x0b, 0x24},
	{HDMI_VFRMT_720x480p60_16_9, 1, 06, 24,
			0x13, 0x0038, true, 0x12, 0x0c, 0x0b, 0x24},
	{HDMI_VFRMT_1280x720p60_16_9, 4, 06, 24,
			0x0c, 0x0020, false, 0x00, 0x00, 0x00, 0x1f},
	{HDMI_VFRMT_1280x720p60_16_9, 2, 06, 24,
			0x16, 0x0015, false, 0x00, 0x00, 0x00, 0x1f},
	{HDMI_VFRMT_1280x720p60_16_9, 1, 10, 24,
			0x21, 0x001a, false, 0x00, 0x00, 0x00, 0x27},
	{HDMI_VFRMT_1920x1080p60_16_9, 4, 06, 24,
			0x16, 0x000f, false, 0x00, 0x00, 0x00, 0x1f},
	{HDMI_VFRMT_1920x1080p60_16_9, 4, 10, 24,
			0x21, 0x0013, false, 0x21, 0x8, 0x1, 0x26},
	{HDMI_VFRMT_1920x1080p60_16_9, 2, 10, 24,
			0x21, 0x0011, false, 0x00, 0x00, 0x00, 0x27},
	{HDMI_VFRMT_1920x1080p60_16_9, 1, 20, 24,
			0x21, 0x001a, false, 0x00, 0x00, 0x00, 0x27},
	{HDMI_VFRMT_3840x2160p24_16_9, 4, 10, 24,
			0x21, 0x000c, false, 0x00, 0x00, 0x00, 0x27},
	{HDMI_VFRMT_3840x2160p30_16_9, 4, 10, 24,
			0x21, 0x000c, false, 0x00, 0x00, 0x00, 0x27},
	{HDMI_VFRMT_3840x2160p60_16_9, 4, 20, 24,
			0x21, 0x000c, false, 0x00, 0x00, 0x00, 0x27},
	{HDMI_VFRMT_4096x2160p24_256_135, 4, 10, 24,
			0x21, 0x000c, false, 0x00, 0x00, 0x00, 0x27},
	{HDMI_VFRMT_4096x2160p30_256_135, 4, 10, 24,
			0x21, 0x000c, false, 0x00, 0x00, 0x00, 0x27},
	{HDMI_VFRMT_4096x2160p60_256_135, 4, 20, 24,
			0x21, 0x000c, false, 0x00, 0x00, 0x00, 0x27},
	{HDMI_EVFRMT_4096x2160p24_16_9, 4, 10, 24,
			0x21, 0x000c, false, 0x00, 0x00, 0x00, 0x27},
};

static inline struct mdss_dp_phy_cfg *mdss_dp_phy_aux_get_config(
	struct mdss_dp_drv_pdata *dp, enum dp_phy_aux_config_type cfg_type)
{
	return &dp->aux_cfg[cfg_type];
}

static inline u32 mdss_dp_phy_aux_get_config_cnt(
	struct mdss_dp_drv_pdata *dp, enum dp_phy_aux_config_type cfg_type)
{
	return dp->aux_cfg[cfg_type].cfg_cnt;
}

void mdss_dp_aux_set_limits(struct dss_io_data *ctrl_io);
int dp_aux_read(void *ep, struct edp_cmd *cmds);
int dp_aux_write(void *ep, struct edp_cmd *cmd);
void mdss_dp_state_ctrl(struct dss_io_data *ctrl_io, u32 data);
u32 mdss_dp_get_ctrl_hw_version(struct dss_io_data *ctrl_io);
u32 mdss_dp_get_phy_hw_version(struct dss_io_data *phy_io);
void mdss_dp_ctrl_reset(struct dss_io_data *ctrl_io);
void mdss_dp_aux_reset(struct dss_io_data *ctrl_io);
void mdss_dp_mainlink_reset(struct dss_io_data *ctrl_io);
void mdss_dp_phy_reset(struct dss_io_data *ctrl_io);
void mdss_dp_switch_usb3_phy_to_dp_mode(struct dss_io_data *tcsr_reg_io);
void mdss_dp_assert_phy_reset(struct dss_io_data *ctrl_io, bool assert);
void mdss_dp_setup_tr_unit(struct dss_io_data *ctrl_io, u8 link_rate,
			u8 ln_cnt, u32 res, struct mdss_panel_info *pinfo);
void mdss_dp_config_misc(struct mdss_dp_drv_pdata *dp, u32 bd, u32 cc);
void mdss_dp_phy_aux_setup(struct mdss_dp_drv_pdata *dp);
void mdss_dp_phy_aux_update_config(struct mdss_dp_drv_pdata *dp,
		enum dp_phy_aux_config_type config_type);
void mdss_dp_hpd_configure(struct dss_io_data *ctrl_io, bool enable);
void mdss_dp_aux_ctrl(struct dss_io_data *ctrl_io, bool enable);
void mdss_dp_mainlink_ctrl(struct dss_io_data *ctrl_io, bool enable);
void mdss_dp_ctrl_lane_mapping(struct dss_io_data *ctrl_io, char *l_map);
bool mdss_dp_mainlink_ready(struct mdss_dp_drv_pdata *dp);
void mdss_dp_timing_cfg(struct dss_io_data *ctrl_io,
				struct mdss_panel_info *pinfo);
void mdss_dp_configuration_ctrl(struct dss_io_data *ctrl_io, u32 data);
void mdss_dp_state_ctrl(struct dss_io_data *ctrl_io, u32 data);
int mdss_dp_irq_setup(struct mdss_dp_drv_pdata *dp_drv);
void mdss_dp_irq_enable(struct mdss_dp_drv_pdata *dp_drv);
void mdss_dp_irq_disable(struct mdss_dp_drv_pdata *dp_drv);
void mdss_dp_sw_config_msa(struct mdss_dp_drv_pdata *dp);
void mdss_dp_usbpd_ext_capabilities(struct usbpd_dp_capabilities *dp_cap);
void mdss_dp_usbpd_ext_dp_status(struct usbpd_dp_status *dp_status);
u32 mdss_dp_usbpd_gen_config_pkt(struct mdss_dp_drv_pdata *dp);
void mdss_dp_phy_share_lane_config(struct dss_io_data *phy_io,
		u8 orientation, u8 ln_cnt, u32 phy_reg_offset);
void mdss_dp_config_audio_acr_ctrl(struct dss_io_data *ctrl_io,
						char link_rate);
void mdss_dp_audio_setup_sdps(struct dss_io_data *ctrl_io, u32 num_of_channels);
void mdss_dp_audio_enable(struct dss_io_data *ctrl_io, bool enable);
void mdss_dp_audio_select_core(struct dss_io_data *ctrl_io);
void mdss_dp_audio_set_sample_rate(struct dss_io_data *ctrl_io,
		char dp_link_rate, uint32_t audio_freq);
void mdss_dp_set_safe_to_exit_level(struct dss_io_data *ctrl_io,
		uint32_t lane_cnt);
int mdss_dp_aux_read_rx_status(struct mdss_dp_drv_pdata *dp, u8 *rx_status);
void mdss_dp_phy_send_test_pattern(struct mdss_dp_drv_pdata *dp);
void mdss_dp_config_ctl_frame_crc(struct mdss_dp_drv_pdata *dp, bool enable);
int mdss_dp_read_ctl_frame_crc(struct mdss_dp_drv_pdata *dp);

#endif /* __DP_UTIL_H__ */
