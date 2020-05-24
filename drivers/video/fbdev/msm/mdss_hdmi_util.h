/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2010-2016, 2018, 2020, The Linux Foundation. All rights reserved. */

#ifndef __HDMI_UTIL_H__
#define __HDMI_UTIL_H__
#include <linux/mdss_io_util.h>
#include "video/msm_hdmi_modes.h"

#include "mdss_panel.h"
#include "mdss_hdmi_panel.h"

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
#define HDMI_INTERNAL_TIMING_MODE        (0x00000374)
#define HDMI_CTRL_SW_RESET               (0x00000378)
#define HDMI_CTRL_AUDIO_RESET            (0x0000037C)
#define HDMI_SCRATCH                     (0x00000380)
#define HDMI_CLK_CTRL                    (0x00000384)
#define HDMI_CLK_ACTIVE                  (0x00000388)
#define HDMI_VBI_CFG                     (0x0000038C)
#define HDMI_DDC_INT_CTRL0               (0x00000430)
#define HDMI_DDC_INT_CTRL1               (0x00000434)
#define HDMI_DDC_INT_CTRL2               (0x00000438)
#define HDMI_DDC_INT_CTRL3               (0x0000043C)
#define HDMI_DDC_INT_CTRL4               (0x00000440)
#define HDMI_DDC_INT_CTRL5               (0x00000444)
#define HDMI_HDCP2P2_DDC_CTRL            (0x0000044C)
#define HDMI_HDCP2P2_DDC_TIMER_CTRL      (0x00000450)
#define HDMI_HDCP2P2_DDC_TIMER_CTRL2     (0x00000454)
#define HDMI_HDCP2P2_DDC_STATUS          (0x00000458)
#define HDMI_SCRAMBLER_STATUS_DDC_CTRL   (0x00000464)
#define HDMI_SCRAMBLER_STATUS_DDC_TIMER_CTRL    (0x00000468)
#define HDMI_SCRAMBLER_STATUS_DDC_TIMER_CTRL2   (0x0000046C)
#define HDMI_SCRAMBLER_STATUS_DDC_STATUS        (0x00000470)
#define HDMI_SCRAMBLER_STATUS_DDC_TIMER_STATUS  (0x00000474)
#define HDMI_SCRAMBLER_STATUS_DDC_TIMER_STATUS2 (0x00000478)
#define HDMI_HW_DDC_CTRL                 (0x000004CC)
#define HDMI_HDCP2P2_DDC_SW_TRIGGER      (0x000004D0)
#define HDMI_HDCP_STATUS                 (0x00000500)
#define HDMI_HDCP_INT_CTRL2              (0x00000504)

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
#define QFPROM_RAW_VERSION_4             (0x000000A8)
#define SEC_CTRL_HW_VERSION              (0x00006000)
#define HDCP_KSV_LSB                     (0x000060D8)
#define HDCP_KSV_MSB                     (0x000060DC)
#define HDCP_KSV_VERSION_4_OFFSET        (0x00000014)

/* SEC_CTRL version that supports HDCP SEL */
#define HDCP_SEL_MIN_SEC_VERSION         (0x50010000)

#define TOP_AND_BOTTOM		(1 << HDMI_S3D_TOP_AND_BOTTOM)
#define FRAME_PACKING		(1 << HDMI_S3D_FRAME_PACKING)
#define SIDE_BY_SIDE_HALF	(1 << HDMI_S3D_SIDE_BY_SIDE)

#define LPASS_LPAIF_RDDMA_CTL0           (0xFE152000)
#define LPASS_LPAIF_RDDMA_PER_CNT0       (0x00000014)

/* TX major version that supports scrambling */
#define HDMI_TX_SCRAMBLER_MIN_TX_VERSION 0x04

/* TX major versions */
#define HDMI_TX_VERSION_4         4
#define HDMI_TX_VERSION_3         3

/* HDMI SCDC register offsets */
#define HDMI_SCDC_UPDATE_0              0x10
#define HDMI_SCDC_UPDATE_1              0x11
#define HDMI_SCDC_TMDS_CONFIG           0x20
#define HDMI_SCDC_SCRAMBLER_STATUS      0x21
#define HDMI_SCDC_CONFIG_0              0x30
#define HDMI_SCDC_STATUS_FLAGS_0        0x40
#define HDMI_SCDC_STATUS_FLAGS_1        0x41
#define HDMI_SCDC_ERR_DET_0_L           0x50
#define HDMI_SCDC_ERR_DET_0_H           0x51
#define HDMI_SCDC_ERR_DET_1_L           0x52
#define HDMI_SCDC_ERR_DET_1_H           0x53
#define HDMI_SCDC_ERR_DET_2_L           0x54
#define HDMI_SCDC_ERR_DET_2_H           0x55
#define HDMI_SCDC_ERR_DET_CHECKSUM      0x56

/* HDCP secure registers directly accessible to HLOS since HDMI controller
 * version major version 4.0
 */
#define HDCP_SEC_TZ_HV_HLOS_HDCP_RCVPORT_DATA0  (0x00000004)
#define HDCP_SEC_TZ_HV_HLOS_HDCP_RCVPORT_DATA1  (0x00000008)
#define HDCP_SEC_TZ_HV_HLOS_HDCP_RCVPORT_DATA7  (0x0000000C)
#define HDCP_SEC_TZ_HV_HLOS_HDCP_RCVPORT_DATA8  (0x00000010)
#define HDCP_SEC_TZ_HV_HLOS_HDCP_RCVPORT_DATA9  (0x00000014)
#define HDCP_SEC_TZ_HV_HLOS_HDCP_RCVPORT_DATA10 (0x00000018)
#define HDCP_SEC_TZ_HV_HLOS_HDCP_RCVPORT_DATA11 (0x0000001C)
#define HDCP_SEC_TZ_HV_HLOS_HDCP_RCVPORT_DATA12 (0x00000020)
#define HDCP_SEC_TZ_HV_HLOS_HDCP_SHA_CTRL       (0x00000024)
#define HDCP_SEC_TZ_HV_HLOS_HDCP_SHA_DATA       (0x00000028)

/*
 * Offsets in HDMI_DDC_INT_CTRL0 register
 *
 * The HDMI_DDC_INT_CTRL0 register is intended for HDCP 2.2 RxStatus
 * register manipulation. It reads like this:
 *
 * Bit 31: RXSTATUS_MESSAGE_SIZE_MASK (1 = generate interrupt when size > 0)
 * Bit 30: RXSTATUS_MESSAGE_SIZE_ACK  (1 = Acknowledge message size intr)
 * Bits 29-20: RXSTATUS_MESSAGE_SIZE  (Actual size of message available)
 * Bits 19-18: RXSTATUS_READY_MASK    (1 = generate interrupt when ready = 1
 *				       2 = generate interrupt when ready = 0)
 * Bit 17: RXSTATUS_READY_ACK         (1 = Acknowledge ready bit interrupt)
 * Bit 16: RXSTATUS_READY	      (1 = Rxstatus ready bit read is 1)
 * Bit 15: RXSTATUS_READY_NOT         (1 = Rxstatus ready bit read is 0)
 * Bit 14: RXSTATUS_REAUTH_REQ_MASK   (1 = generate interrupt when reauth is
 *					   requested by sink)
 * Bit 13: RXSTATUS_REAUTH_REQ_ACK    (1 = Acknowledge Reauth req interrupt)
 * Bit 12: RXSTATUS_REAUTH_REQ        (1 = Rxstatus reauth req bit read is 1)
 * Bit 10: RXSTATUS_DDC_FAILED_MASK   (1 = generate interrupt when DDC
 *					   tranasaction fails)
 * Bit 9:  RXSTATUS_DDC_FAILED_ACK    (1 = Acknowledge ddc failure interrupt)
 * Bit 8:  RXSTATUS_DDC_FAILED	      (1 = DDC transaction failed)
 * Bit 6:  RXSTATUS_DDC_DONE_MASK     (1 = generate interrupt when DDC
 *					   transaction completes)
 * Bit 5:  RXSTATUS_DDC_DONE_ACK      (1 = Acknowledge ddc done interrupt)
 * Bit 4:  RXSTATUS_DDC_DONE	      (1 = DDC transaction is done)
 * Bit 2:  RXSTATUS_DDC_REQ_MASK      (1 = generate interrupt when DDC Read
 *					   request for RXstatus is made)
 * Bit 1:  RXSTATUS_DDC_REQ_ACK       (1 = Acknowledge Rxstatus read interrupt)
 * Bit 0:  RXSTATUS_DDC_REQ           (1 = RXStatus DDC read request is made)
 *
 */
#define HDCP2P2_RXSTATUS_MESSAGE_SIZE_SHIFT         20
#define HDCP2P2_RXSTATUS_MESSAGE_SIZE_MASK          0x3ff00000
#define HDCP2P2_RXSTATUS_MESSAGE_SIZE_ACK_SHIFT     30
#define HDCP2P2_RXSTATUS_MESSAGE_SIZE_INTR_SHIFT    31

#define HDCP2P2_RXSTATUS_REAUTH_REQ_SHIFT           12
#define HDCP2P2_RXSTATUS_REAUTH_REQ_MASK             1
#define HDCP2P2_RXSTATUS_REAUTH_REQ_ACK_SHIFT	    13
#define HDCP2P2_RXSTATUS_REAUTH_REQ_INTR_SHIFT	    14

#define HDCP2P2_RXSTATUS_READY_SHIFT		    16
#define HDCP2P2_RXSTATUS_READY_MASK                  1
#define HDCP2P2_RXSTATUS_READY_ACK_SHIFT            17
#define HDCP2P2_RXSTATUS_READY_INTR_SHIFT           18
#define HDCP2P2_RXSTATUS_READY_INTR_MASK            18

#define HDCP2P2_RXSTATUS_DDC_FAILED_SHIFT           8
#define HDCP2P2_RXSTATUS_DDC_FAILED_ACKSHIFT        9
#define HDCP2P2_RXSTATUS_DDC_FAILED_INTR_MASK       10
#define HDCP2P2_RXSTATUS_DDC_DONE                   6

/*
 * Bits 1:0 in HDMI_HW_DDC_CTRL that dictate how the HDCP 2.2 RxStatus will be
 * read by the hardware
 */
#define HDCP2P2_RXSTATUS_HW_DDC_DISABLE             0
#define HDCP2P2_RXSTATUS_HW_DDC_AUTOMATIC_LOOP      1
#define HDCP2P2_RXSTATUS_HW_DDC_FORCE_LOOP          2
#define HDCP2P2_RXSTATUS_HW_DDC_SW_TRIGGER          3

/* default hsyncs for 4k@60 for 200ms */
#define HDMI_DEFAULT_TIMEOUT_HSYNC 28571

enum hdmi_tx_feature_type {
	HDMI_TX_FEAT_EDID     = BIT(0),
	HDMI_TX_FEAT_HDCP     = BIT(1),
	HDMI_TX_FEAT_HDCP2P2  = BIT(2),
	HDMI_TX_FEAT_CEC_HW   = BIT(3),
	HDMI_TX_FEAT_CEC_ABST = BIT(4),
	HDMI_TX_FEAT_PANEL    = BIT(5),
	HDMI_TX_FEAT_MAX      = HDMI_TX_FEAT_EDID | HDMI_TX_FEAT_HDCP |
				HDMI_TX_FEAT_HDCP2P2 | HDMI_TX_FEAT_CEC_HW |
				HDMI_TX_FEAT_CEC_ABST | HDMI_TX_FEAT_PANEL
};

enum hdmi_tx_scdc_access_type {
	HDMI_TX_SCDC_SCRAMBLING_STATUS,
	HDMI_TX_SCDC_SCRAMBLING_ENABLE,
	HDMI_TX_SCDC_TMDS_BIT_CLOCK_RATIO_UPDATE,
	HDMI_TX_SCDC_CLOCK_DET_STATUS,
	HDMI_TX_SCDC_CH0_LOCK_STATUS,
	HDMI_TX_SCDC_CH1_LOCK_STATUS,
	HDMI_TX_SCDC_CH2_LOCK_STATUS,
	HDMI_TX_SCDC_CH0_ERROR_COUNT,
	HDMI_TX_SCDC_CH1_ERROR_COUNT,
	HDMI_TX_SCDC_CH2_ERROR_COUNT,
	HDMI_TX_SCDC_READ_ENABLE,
	HDMI_TX_SCDC_MAX,
};

enum hdmi_tx_ddc_timer_type {
	HDMI_TX_DDC_TIMER_HDCP2P2_RD_MSG,
	HDMI_TX_DDC_TIMER_SCRAMBLER_STATUS,
	HDMI_TX_DDC_TIMER_UPDATE_FLAGS,
	HDMI_TX_DDC_TIMER_STATUS_FLAGS,
	HDMI_TX_DDC_TIMER_CED,
	HDMI_TX_DDC_TIMER_MAX,
};

struct hdmi_tx_ddc_data {
	char *what;
	u8 *data_buf;
	u32 data_len;
	u32 dev_addr;
	u32 offset;
	u32 request_len;
	u32 retry_align;
	u32 hard_timeout;
	u32 timeout_left;
	int retry;
};

enum hdmi_tx_hdcp2p2_rxstatus_intr_mask {
	RXSTATUS_MESSAGE_SIZE = BIT(31),
	RXSTATUS_READY = BIT(18),
	RXSTATUS_REAUTH_REQ = BIT(14),
};

struct hdmi_tx_hdcp2p2_ddc_data {
	enum hdmi_tx_hdcp2p2_rxstatus_intr_mask intr_mask;
	u32 timeout_ms;
	u32 timeout_hsync;
	u32 periodic_timer_hsync;
	u32 timeout_left;
	u32 read_method;
	u32 message_size;
	bool encryption_ready;
	bool ready;
	bool reauth_req;
	bool ddc_max_retries_fail;
	bool ddc_done;
	bool ddc_read_req;
	bool ddc_timeout;
	bool wait;
	int irq_wait_count;
	void (*link_cb)(void *data);
	void *link_data;
};

struct hdmi_tx_ddc_ctrl {
	atomic_t write_busy_wait_done;
	atomic_t read_busy_wait_done;
	atomic_t rxstatus_busy_wait_done;
	struct dss_io_data *io;
	struct completion ddc_sw_done;
	struct hdmi_tx_ddc_data ddc_data;
	struct hdmi_tx_hdcp2p2_ddc_data hdcp2p2_ddc_data;
};


struct hdmi_util_ds_data {
	bool ds_registered;
	u32 ds_max_clk;
	u32 modes_num;
	u32 *modes;
};

static inline int hdmi_tx_get_v_total(const struct msm_hdmi_mode_timing_info *t)
{
	if (t) {
		return t->active_v + t->front_porch_v + t->pulse_width_v +
			t->back_porch_v;
	}

	return 0;
}

static inline int hdmi_tx_get_h_total(const struct msm_hdmi_mode_timing_info *t)
{
	if (t) {
		return t->active_h + t->front_porch_h + t->pulse_width_h +
			t->back_porch_h;
	}

	return 0;
}

/* video timing related utility routines */
int hdmi_get_video_id_code(struct msm_hdmi_mode_timing_info *timing_in,
	struct hdmi_util_ds_data *ds_data);
int hdmi_get_supported_mode(struct msm_hdmi_mode_timing_info *info,
	struct hdmi_util_ds_data *ds_data, u32 mode);
ssize_t hdmi_get_video_3d_fmt_2string(u32 format, char *buf, u32 size);
const char *msm_hdmi_mode_2string(u32 mode);
int hdmi_set_resv_timing_info(struct msm_hdmi_mode_timing_info *mode);
bool hdmi_is_valid_resv_timing(int mode);
void hdmi_reset_resv_timing_info(void);
int hdmi_panel_get_vic(struct mdss_panel_info *pinfo,
		struct hdmi_util_ds_data *ds_data);
int hdmi_tx_setup_tmds_clk_rate(u32 pixel_freq, u32 out_format, bool dc_enable);

/* todo: Fix this. Right now this is defined in mdss_hdmi_tx.c */
void *hdmi_get_featuredata_from_sysfs_dev(struct device *device, u32 type);

/* DDC */
void hdmi_ddc_config(struct hdmi_tx_ddc_ctrl *ctrl);
int hdmi_ddc_isr(struct hdmi_tx_ddc_ctrl *ctrl, u32 version);
int hdmi_ddc_write(struct hdmi_tx_ddc_ctrl *ctrl);
int hdmi_ddc_read_seg(struct hdmi_tx_ddc_ctrl *ctrl);
int hdmi_ddc_read(struct hdmi_tx_ddc_ctrl *ctrl);
int hdmi_ddc_abort_transaction(struct hdmi_tx_ddc_ctrl *ctrl);

int hdmi_scdc_read(struct hdmi_tx_ddc_ctrl *ctrl, u32 data_type, u32 *val);
int hdmi_scdc_write(struct hdmi_tx_ddc_ctrl *ctrl, u32 data_type, u32 val);
int hdmi_setup_ddc_timers(struct hdmi_tx_ddc_ctrl *ctrl,
			  u32 type, u32 to_in_num_lines);
void hdmi_scrambler_ddc_disable(struct hdmi_tx_ddc_ctrl *ctrl);
void hdmi_hdcp2p2_ddc_disable(struct hdmi_tx_ddc_ctrl *ctrl);
int hdmi_hdcp2p2_ddc_read_rxstatus(struct hdmi_tx_ddc_ctrl *ctrl);
int hdmi_utils_get_timeout_in_hysnc(struct msm_hdmi_mode_timing_info *timing,
	u32 timeout_ms);

#endif /* __HDMI_UTIL_H__ */
