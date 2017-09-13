/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#ifndef _DP_REG_H_
#define _DP_REG_H_

/* DP_TX Registers */
#define DP_HW_VERSION				(0x00000000)
#define DP_SW_RESET				(0x00000010)
#define DP_PHY_CTRL				(0x00000014)
#define DP_CLK_CTRL				(0x00000018)
#define DP_CLK_ACTIVE				(0x0000001C)
#define DP_INTR_STATUS				(0x00000020)
#define DP_INTR_STATUS2				(0x00000024)
#define DP_INTR_STATUS3				(0x00000028)

#define DP_DP_HPD_CTRL				(0x00000200)
#define DP_DP_HPD_INT_STATUS			(0x00000204)
#define DP_DP_HPD_INT_ACK			(0x00000208)
#define DP_DP_HPD_INT_MASK			(0x0000020C)
#define DP_DP_HPD_REFTIMER			(0x00000218)
#define DP_DP_HPD_EVENT_TIME_0			(0x0000021C)
#define DP_DP_HPD_EVENT_TIME_1			(0x00000220)
#define DP_AUX_CTRL				(0x00000230)
#define DP_AUX_DATA				(0x00000234)
#define DP_AUX_TRANS_CTRL			(0x00000238)
#define DP_TIMEOUT_COUNT			(0x0000023C)
#define DP_AUX_LIMITS				(0x00000240)
#define DP_AUX_STATUS				(0x00000244)

#define DP_DPCD_CP_IRQ				(0x201)
#define DP_DPCD_RXSTATUS			(0x69493)

#define DP_INTERRUPT_TRANS_NUM			(0x000002A0)

#define DP_MAINLINK_CTRL			(0x00000400)
#define DP_STATE_CTRL				(0x00000404)
#define DP_CONFIGURATION_CTRL			(0x00000408)
#define DP_SOFTWARE_MVID			(0x00000410)
#define DP_SOFTWARE_NVID			(0x00000418)
#define DP_TOTAL_HOR_VER			(0x0000041C)
#define DP_START_HOR_VER_FROM_SYNC		(0x00000420)
#define DP_HSYNC_VSYNC_WIDTH_POLARITY		(0x00000424)
#define DP_ACTIVE_HOR_VER			(0x00000428)
#define DP_MISC1_MISC0				(0x0000042C)
#define DP_VALID_BOUNDARY			(0x00000430)
#define DP_VALID_BOUNDARY_2			(0x00000434)
#define DP_LOGICAL2PHYSCIAL_LANE_MAPPING	(0x00000438)

#define DP_MAINLINK_READY			(0x00000440)
#define DP_MAINLINK_LEVELS			(0x00000444)
#define DP_TU					(0x0000044C)

#define DP_HBR2_COMPLIANCE_SCRAMBLER_RESET	(0x00000454)
#define DP_TEST_80BIT_CUSTOM_PATTERN_REG0	(0x000004C0)
#define DP_TEST_80BIT_CUSTOM_PATTERN_REG1	(0x000004C4)
#define DP_TEST_80BIT_CUSTOM_PATTERN_REG2	(0x000004C8)

#define MMSS_DP_MISC1_MISC0			(0x0000042C)
#define MMSS_DP_AUDIO_TIMING_GEN		(0x00000480)
#define MMSS_DP_AUDIO_TIMING_RBR_32		(0x00000484)
#define MMSS_DP_AUDIO_TIMING_HBR_32		(0x00000488)
#define MMSS_DP_AUDIO_TIMING_RBR_44		(0x0000048C)
#define MMSS_DP_AUDIO_TIMING_HBR_44		(0x00000490)
#define MMSS_DP_AUDIO_TIMING_RBR_48		(0x00000494)
#define MMSS_DP_AUDIO_TIMING_HBR_48		(0x00000498)

#define MMSS_DP_PSR_CRC_RG			(0x00000554)
#define MMSS_DP_PSR_CRC_B			(0x00000558)

#define DP_COMPRESSION_MODE_CTRL		(0x00000580)

#define MMSS_DP_AUDIO_CFG			(0x00000600)
#define MMSS_DP_AUDIO_STATUS			(0x00000604)
#define MMSS_DP_AUDIO_PKT_CTRL			(0x00000608)
#define MMSS_DP_AUDIO_PKT_CTRL2			(0x0000060C)
#define MMSS_DP_AUDIO_ACR_CTRL			(0x00000610)
#define MMSS_DP_AUDIO_CTRL_RESET		(0x00000614)

#define MMSS_DP_SDP_CFG				(0x00000628)
#define MMSS_DP_SDP_CFG2			(0x0000062C)
#define MMSS_DP_AUDIO_TIMESTAMP_0		(0x00000630)
#define MMSS_DP_AUDIO_TIMESTAMP_1		(0x00000634)

#define MMSS_DP_AUDIO_STREAM_0			(0x00000640)
#define MMSS_DP_AUDIO_STREAM_1			(0x00000644)

#define MMSS_DP_EXTENSION_0			(0x00000650)
#define MMSS_DP_EXTENSION_1			(0x00000654)
#define MMSS_DP_EXTENSION_2			(0x00000658)
#define MMSS_DP_EXTENSION_3			(0x0000065C)
#define MMSS_DP_EXTENSION_4			(0x00000660)
#define MMSS_DP_EXTENSION_5			(0x00000664)
#define MMSS_DP_EXTENSION_6			(0x00000668)
#define MMSS_DP_EXTENSION_7			(0x0000066C)
#define MMSS_DP_EXTENSION_8			(0x00000670)
#define MMSS_DP_EXTENSION_9			(0x00000674)
#define MMSS_DP_AUDIO_COPYMANAGEMENT_0		(0x00000678)
#define MMSS_DP_AUDIO_COPYMANAGEMENT_1		(0x0000067C)
#define MMSS_DP_AUDIO_COPYMANAGEMENT_2		(0x00000680)
#define MMSS_DP_AUDIO_COPYMANAGEMENT_3		(0x00000684)
#define MMSS_DP_AUDIO_COPYMANAGEMENT_4		(0x00000688)
#define MMSS_DP_AUDIO_COPYMANAGEMENT_5		(0x0000068C)
#define MMSS_DP_AUDIO_ISRC_0			(0x00000690)
#define MMSS_DP_AUDIO_ISRC_1			(0x00000694)
#define MMSS_DP_AUDIO_ISRC_2			(0x00000698)
#define MMSS_DP_AUDIO_ISRC_3			(0x0000069C)
#define MMSS_DP_AUDIO_ISRC_4			(0x000006A0)
#define MMSS_DP_AUDIO_ISRC_5			(0x000006A4)
#define MMSS_DP_AUDIO_INFOFRAME_0		(0x000006A8)
#define MMSS_DP_AUDIO_INFOFRAME_1		(0x000006AC)
#define MMSS_DP_AUDIO_INFOFRAME_2		(0x000006B0)

#define MMSS_DP_GENERIC0_0			(0x00000700)
#define MMSS_DP_GENERIC0_1			(0x00000704)
#define MMSS_DP_GENERIC0_2			(0x00000708)
#define MMSS_DP_GENERIC0_3			(0x0000070C)
#define MMSS_DP_GENERIC0_4			(0x00000710)
#define MMSS_DP_GENERIC0_5			(0x00000714)
#define MMSS_DP_GENERIC0_6			(0x00000718)
#define MMSS_DP_GENERIC0_7			(0x0000071C)
#define MMSS_DP_GENERIC0_8			(0x00000720)
#define MMSS_DP_GENERIC0_9			(0x00000724)
#define MMSS_DP_GENERIC1_0			(0x00000728)
#define MMSS_DP_GENERIC1_1			(0x0000072C)
#define MMSS_DP_GENERIC1_2			(0x00000730)
#define MMSS_DP_GENERIC1_3			(0x00000734)
#define MMSS_DP_GENERIC1_4			(0x00000738)
#define MMSS_DP_GENERIC1_5			(0x0000073C)
#define MMSS_DP_GENERIC1_6			(0x00000740)
#define MMSS_DP_GENERIC1_7			(0x00000744)
#define MMSS_DP_GENERIC1_8			(0x00000748)
#define MMSS_DP_GENERIC1_9			(0x0000074C)

#define MMSS_DP_VSCEXT_0			(0x000006D0)
#define MMSS_DP_VSCEXT_1			(0x000006D4)
#define MMSS_DP_VSCEXT_2			(0x000006D8)
#define MMSS_DP_VSCEXT_3			(0x000006DC)
#define MMSS_DP_VSCEXT_4			(0x000006E0)
#define MMSS_DP_VSCEXT_5			(0x000006E4)
#define MMSS_DP_VSCEXT_6			(0x000006E8)
#define MMSS_DP_VSCEXT_7			(0x000006EC)
#define MMSS_DP_VSCEXT_8			(0x000006F0)
#define MMSS_DP_VSCEXT_9			(0x000006F4)

#define MMSS_DP_TIMING_ENGINE_EN		(0x00000A10)
#define MMSS_DP_ASYNC_FIFO_CONFIG		(0x00000A88)

/*DP PHY Register offsets */
#define DP_PHY_REVISION_ID0                     (0x00000000)
#define DP_PHY_REVISION_ID1                     (0x00000004)
#define DP_PHY_REVISION_ID2                     (0x00000008)
#define DP_PHY_REVISION_ID3                     (0x0000000C)

#define DP_PHY_CFG                              (0x00000010)
#define DP_PHY_PD_CTL                           (0x00000018)
#define DP_PHY_MODE                             (0x0000001C)

#define DP_PHY_AUX_CFG0                         (0x00000020)
#define DP_PHY_AUX_CFG1                         (0x00000024)
#define DP_PHY_AUX_CFG2                         (0x00000028)
#define DP_PHY_AUX_CFG3                         (0x0000002C)
#define DP_PHY_AUX_CFG4                         (0x00000030)
#define DP_PHY_AUX_CFG5                         (0x00000034)
#define DP_PHY_AUX_CFG6                         (0x00000038)
#define DP_PHY_AUX_CFG7                         (0x0000003C)
#define DP_PHY_AUX_CFG8                         (0x00000040)
#define DP_PHY_AUX_CFG9                         (0x00000044)
#define DP_PHY_AUX_INTERRUPT_MASK               (0x00000048)
#define DP_PHY_AUX_INTERRUPT_CLEAR              (0x0000004C)

#define DP_PHY_SPARE0				(0x00AC)

#define TXn_TX_EMP_POST1_LVL			(0x000C)
#define TXn_TX_DRV_LVL				(0x001C)

#define QSERDES_COM_BIAS_EN_CLKBUFLR_EN		(0x004)

/* DP MMSS_CC registers */
#define MMSS_DP_LINK_CMD_RCGR			(0x0138)
#define MMSS_DP_LINK_CFG_RCGR			(0x013C)
#define MMSS_DP_PIXEL_M				(0x0174)
#define MMSS_DP_PIXEL_N				(0x0178)

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

/* USB3 DP COM registers */
#define USB3_DP_COM_RESET_OVRD_CTRL (0x1C)
#define USB3_DP_COM_PHY_MODE_CTRL   (0x00)
#define USB3_DP_COM_SW_RESET        (0x04)
#define USB3_DP_COM_TYPEC_CTRL      (0x10)
#define USB3_DP_COM_SWI_CTRL        (0x0c)
#define USB3_DP_COM_POWER_DOWN_CTRL (0x08)



#endif /* _DP_REG_H_ */
