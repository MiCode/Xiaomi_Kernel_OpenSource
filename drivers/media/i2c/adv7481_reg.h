/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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
#ifndef __ADV7481_REG_H__
#define __ADV7481_REG_H__

#define ADV_REG_SETFIELD(val, field) \
	(((val) << (field##_SHFT)) & (field##_BMSK))

#define ADV_REG_GETFIELD(val, field) \
	(((val) & (field##_BMSK)) >> (field##_SHFT))

#define ADV_REG_RSTFIELD(val, field) \
	((val) & ~((field##_BMSK) << (field##_SHFT)))

/* IO Map Registers */
#define IO_REG_MAIN_RST_ADDR                    0xFF
#define IO_REG_MAIN_RST_VALUE                   0xFF

#define IO_REG_PWR_DOWN_CTRL_ADDR               0x00
#define IO_CTRL_RX_EN_BMSK                      0x0040
#define IO_CTRL_RX_EN_SHFT                      6
#define IO_CTRL_RX_PWDN_BMSK                    0x0020
#define IO_CTRL_RX_PWDN_SHFT                    5
#define IO_CTRL_XTAL_PWDN_BMSK                  0x0004
#define IO_CTRL_XTAL_PWDN_SHFT                  2
#define IO_CTRL_CORE_PWDN_BMSK                  0x0002
#define IO_CTRL_CORE_PWDN_SHFT                  1
#define IO_CTRL_MASTER_PWDN_BMSK                0x0001
#define IO_CTRL_MASTER_PWDN_SHFT                0

#define IO_REG_PWR_DN2_XTAL_HIGH_ADDR           0x01
#define IO_CTRL_CEC_WAKE_UP_PWRDN2B_BMSK        0x0080
#define IO_CTRL_CEC_WAKE_UP_PWRDN2B_SHFT        7
#define IO_CTRL_CEC_WAKE_UP_PWRDNB_BMSK         0x0040
#define IO_CTRL_CEC_WAKE_UP_PWRDNB_SHFT         6
#define IO_PROG_XTAL_FREQ_HIGH_BMSK             0x003F
#define IO_PROG_XTAL_FREQ_HIGH_SHFT             0

#define IO_REG_XTAL_FREQ_LOW_ADDR               0x02
#define IO_PROG_XTAL_FREQ_LOW_BMSK              0x00FF
#define IO_PROG_XTAL_FREQ_LOW_SHFT              0

#define IO_REG_CP_VID_STD_ADDR                  0x05

#define IO_REG_CSI_PIX_EN_SEL_ADDR              0x10
#define IO_CTRL_CSI4_EN_BMSK                    0x0080
#define IO_CTRL_CSI4_EN_SHFT                    7
#define IO_CTRL_CSI1_EN_BMSK                    0x0040
#define IO_CTRL_CSI1_EN_SHFT                    6
#define IO_CTRL_PIX_OUT_EN_BMSK                 0x0020
#define IO_CTRL_PIX_OUT_EN_SHFT                 5
#define IO_CTRL_SD_THRU_PIX_OUT_BMSK            0x0010
#define IO_CTRL_SD_THRU_PIX_OUT_SHFT            4
#define IO_CTRL_CSI4_IN_SEL_BMSK                0x000C
#define IO_CTRL_CSI4_IN_SEL_SHFT                2

#define IO_PAD_CTRLS_ADDR                       0x0E
#define IO_PAD_FILTER_CTRLS_ADDR                0x0F

#define IO_REG_I2C_CFG_ADDR                     0xF2
#define IO_REG_I2C_AUTOINC_EN_REG_VALUE         0x01

#define IO_CTRL_MASTER_PWDN_REG_VALUE           0x01

/* Interrupts */
#define IO_HDMI_LVL_INT_CLEAR_1_ADDR            0x69

#define IO_HDMI_LVL_INT_MASKB_1_ADDR            0x6B
#define IO_AVI_INFO_MB1_BMSK                    0x0001
#define IO_AVI_INFO_MB1_SHFT                    0

#define IO_HDMI_LVL_INT_CLEAR_2_ADDR            0x6E

#define IO_HDMI_LVL_RAW_STATUS_3_ADDR           0x71
#define IO_TMDSPLL_LCK_A_RAW_BMSK               0x0080
#define IO_TMDSPLL_LCK_A_RAW_SHFT               7
#define IO_CABLE_DET_A_RAW_BMSK                 0x0040
#define IO_CABLE_DET_A_RAW_SHFT                 6
#define IO_V_LOCKED_RAW_BMSK                    0x0002
#define IO_V_LOCKED_RAW_SHFT                    1
#define IO_DE_REGEN_LCK_RAW_BMSK                0x0001
#define IO_DE_REGEN_LCK_RAW_SHFT                0

#define IO_HDMI_LVL_INT_STATUS_3_ADDR           0x72
#define IO_CABLE_DET_A_ST_BMSK                  0x0040
#define IO_CABLE_DET_A_ST_SHFT                  6
#define IO_V_LOCKED_ST_BMSK                     0x0002
#define IO_V_LOCKED_ST_SHFT                     1
#define IO_DE_REGEN_LCK_ST_BMSK                 0x0001
#define IO_DE_REGEN_LCK_ST_SHFT                 0

#define IO_HDMI_LVL_INT_CLEAR_3_ADDR            0x73
#define IO_CABLE_DET_A_CLR_BMSK                 0x0040
#define IO_CABLE_DET_A_CLR_SHFT                 6

#define IO_HDMI_LVL_INT2_MASKB_3_ADDR           0x74
#define IO_CABLE_DET_A_MB2_BMSK                 0x0040
#define IO_CABLE_DET_A_MB2_SHFT                 6

#define IO_HDMI_LVL_INT_MASKB_3_ADDR            0x75
#define IO_CABLE_DET_A_MB1_BMSK                 0x0040
#define IO_CABLE_DET_A_MB1_SHFT                 6
#define IO_V_LOCKED_MB1_BMSK                    0x0002
#define IO_V_LOCKED_MB1_SHFT                    1
#define IO_DE_REGEN_LCK_MB1_BMSK                0x0001
#define IO_DE_REGEN_LCK_MB1_SHFT                0

#define IO_HDMI_EDG_RAW_STATUS_1_ADDR           0x80
#define IO_NEW_AVI_INFO_RAW_BMSK                0x0001
#define IO_NEW_AVI_INFO_RAW_SHFT                0

#define IO_HDMI_EDG_INT_STATUS_1_ADDR           0x81
#define IO_NEW_AVI_INFO_ST_BMSK                 0x0001
#define IO_NEW_AVI_INFO_ST_SHFT                 0

#define IO_HDMI_EDG_INT_CLEAR_1_ADDR            0x82
#define IO_NEW_AVI_INFO_CLR_BMSK                0x0001
#define IO_NEW_AVI_INFO_CLR_SHFT                0

#define IO_HDMI_EDG_INT2_MASKB_1_ADDR           0x83
#define IO_NEW_AVI_INFO_MB2_BMSK                0x0001
#define IO_NEW_AVI_INFO_MB2_SHFT                0

#define IO_HDMI_EDG_INT_MASKB_1_ADDR            0x84
#define IO_NEW_AVI_INFO_MB1_BMSK                0x0001
#define IO_NEW_AVI_INFO_MB1_SHFT                0

#define IO_HDMI_EDG_INT_CLEAR_2_ADDR            0x87
#define IO_HDMI_EDG_INT_CLEAR_3_ADDR            0x8C

#define IO_REG_PAD_CTRL_1_ADDR                  0x1D
#define IO_PDN_INT1_BMSK                        0x0080
#define IO_PDN_INT1_SHFT                        7
#define IO_PDN_INT2_BMSK                        0x0040
#define IO_PDN_INT2_SHFT                        6
#define IO_PDN_INT3_BMSK                        0x0020
#define IO_PDN_INT3_SHFT                        5
#define IO_INV_LLC_BMSK                         0x0010
#define IO_INV_LLC_SHFT                         4
#define IO_DRV_LLC_PAD_BMSK                     0x000C
#define IO_DRV_LLC_PAD_SHFT                     2

#define IO_REG_INT_RAW_STATUS_ADDR              0x3F
#define IO_INT_CEC_ST_BMSK                      0x0010
#define IO_INT_CEC_ST_SHFT                      4
#define IO_INT_HDMI_ST_BMSK                     0x0008
#define IO_INT_HDMI_ST_SHFT                     3
#define IO_INTRQ3_RAW_BMSK                      0x0004
#define IO_INTRQ3_RAW_SHFT                      2
#define IO_INTRQ2_RAW_BMSK                      0x0002
#define IO_INTRQ2_RAW_SHFT                      1
#define IO_INTRQ1_RAW_BMSK                      0x0001
#define IO_INTRQ1_RAW_SHFT                      0

#define IO_REG_INT1_CONF_ADDR                   0x40
#define IO_INTRQ_DUR_SEL_BMSK                   0x00C0
#define IO_INTRQ_DUR_SEL_SHFT                   6
#define IO_INTRQ_OP_SEL_BMSK                    0x0003
#define IO_INTRQ_OP_SEL_SHFT                    0

#define IO_REG_INT2_CONF_ADDR                   0x41
#define IO_INTRQ2_DUR_SEL_BMSK                  0x00C0
#define IO_INTRQ2_DUR_SEL_SHFT                  6
#define IO_CP_LOCK_UNLOCK_EDGE_SEL_BMSK         0x0020
#define IO_CP_LOCK_UNLOCK_EDGE_SEL_SHFT         5
#define IO_EN_UMASK_RAW_INTRQ2_BMSK             0x0008
#define IO_EN_UMASK_RAW_INTRQ2_SHFT             3
#define IO_INT2_EN_BMSK                         0x0004
#define IO_INT2_EN_SHFT                         2
#define IO_INTRQ2_OP_SEL_BMSK                   0x0003
#define IO_INTRQ2_OP_SEL_SHFT                   0

#define IO_REG_DATAPATH_RAW_STATUS_ADDR         0x43
#define IO_CP_LOCK_CP_RAW_BMSK                  0x0080
#define IO_CP_LOCK_CP_RAW_SHFT                  7
#define IO_CP_UNLOCK_CP_RAW_BMSK                0x0040
#define IO_CP_UNLOCK_CP_RAW_SHFT                6
#define IO_VMUTE_REQUEST_HDMI_RAW_BMSK          0x0020
#define IO_VMUTE_REQUEST_HDMI_RAW_SHFT          5
#define IO_MPU_STIM_INTRQ_RAW_BMSK              0x0002
#define IO_MPU_STIM_INTRQ_RAW_SHFT              1
#define IO_INT_SD_RAW_BMSK                      0x0001
#define IO_INT_SD_RAW_SHFT                      0

#define IO_REG_DATAPATH_INT_STATUS_ADDR         0x44
#define IO_CP_LOCK_CP_ST_BMSK                   0x0080
#define IO_CP_LOCK_CP_ST_SHFT                   7
#define IO_CP_UNLOCK_CP_ST_BMSK                 0x0040
#define IO_CP_UNLOCK_CP_ST_SHFT                 6
#define IO_VMUTE_REQUEST_HDMI_ST_BMSK           0x0020
#define IO_VMUTE_REQUEST_HDMI_ST_SHFT           5
#define IO_MPU_STIM_INTRQ_ST_BMSK               0x0002
#define IO_MPU_STIM_INTRQ_ST_SHFT               1
#define IO_INT_SD_ST_BMSK                       0x0001
#define IO_INT_SD_ST_SHFT                       0

#define IO_REG_DATAPATH_INT_CLEAR_ADDR          0x45

#define IO_REG_DATAPATH_INT_MASKB_ADDR          0x47
#define IO_CP_LOCK_CP_MB1_BMSK                  0x0080
#define IO_CP_LOCK_CP_MB1_SHFT                  7
#define IO_CP_UNLOCK_CP_MB1_BMSK                0x0040
#define IO_CP_UNLOCK_CP_MB1_SHFT                6
#define IO_VMUTE_REQUEST_HDMI_MB1_BMSK          0x0020
#define IO_VMUTE_REQUEST_HDMI_MB1_SHFT          5
#define IO_MPU_STIM_INTRQ_MB1_BMSK              0x0002
#define IO_MPU_STIM_INTRQ_MB1_SHFT              1
#define IO_INT_SD_MB1_BMSK                      0x0001
#define IO_INT_SD_MB1_SHFT                      0

#define IO_REG_CHIP_REV_ID_1_ADDR               0xDF
#define IO_REG_CHIP_REV_ID_2_ADDR               0xE0

/* Offsets */
#define IO_REG_DPLL_ADDR                        0xF3
#define IO_REG_CP_ADDR                          0xF4
#define IO_REG_HDMI_ADDR                        0xF5
#define IO_REG_EDID_ADDR                        0xF6
#define IO_REG_HDMI_REP_ADDR                    0xF7
#define IO_REG_HDMI_INF_ADDR                    0xF8
#define IO_REG_CBUS_ADDR                        0xF9
#define IO_REG_CEC_ADDR                         0xFA
#define IO_REG_SDP_ADDR                         0xFB
#define IO_REG_CSI_TXB_ADDR                     0xFC
#define IO_REG_CSI_TXA_ADDR                     0xFD

/* Sub Address Map Locations */
#define IO_REG_DPLL_SADDR                       0x4C
#define IO_REG_CP_SADDR                         0x44
#define IO_REG_HDMI_SADDR                       0x74
#define IO_REG_EDID_SADDR                       0x78
#define IO_REG_HDMI_REP_SADDR                   0x64
#define IO_REG_HDMI_INF_SADDR                   0x62
#define IO_REG_CBUS_SADDR                       0xF0
#define IO_REG_CEC_SADDR                        0x82
#define IO_REG_SDP_SADDR                        0xF2
#define IO_REG_CSI_TXB_SADDR                    0x90
#define IO_REG_CSI_TXA_SADDR                    0x94

/* HDMI Map Registers */
#define HDMI_REG_HDMI_PARAM4_ADDR               0x04
#define HDMI_REG_AV_MUTE_BMSK                   0x0040
#define HDMI_REG_AV_MUTE_SHFT                   6
#define HDMI_REG_TMDS_PLL_LOCKED_BMSK           0x0002
#define HDMI_REG_TMDS_PLL_LOCKED_SHFT           1
#define HDMI_REG_AUDIO_PLL_LOCKED_BMSK          0x0001
#define HDMI_REG_AUDIO_PLL_LOCKED_SHFT          0

#define HDMI_REG_HDMI_PARAM5_ADDR               0x05
#define HDMI_REG_HDMI_MODE_BMSK                 0x0080
#define HDMI_REG_TMDS_FREQ_0_SHFT               7
#define HDMI_REG_HDMI_CONT_ENCRYPT_BMSK         0x0040
#define HDMI_REG_HDMI_CONT_ENCRYPT_SHFT         6
#define HDMI_REG_DVI_HSYNC_POLARITY_BMSK        0x0020
#define HDMI_REG_DVI_HSYNC_POLARITY_SHFT        5
#define HDMI_REG_DVI_VSYNC_POLARITY_BMSK        0x0010
#define HDMI_REG_DVI_VSYNC_POLARITY_SHFT        4
#define HDMI_REG_PIXEL_REPETITION_BMSK          0x000F
#define HDMI_REG_PIXEL_REPETITION_SHFT          0

#define HDMI_REG_LINE_WIDTH_1_ADDR              0x07
#define HDMI_VERT_FILTER_LOCKED_BMSK            0x0080
#define HDMI_VERT_FILTER_LOCKED_SHFT            7
#define HDMI_AUDIO_CHANNEL_MODE_BMSK            0x0040
#define HDMI_AUDIO_CHANNEL_MODE_SHFT            6
#define HDMI_DE_REGEN_FILTER_LCK_BMSK           0x0020
#define HDMI_DE_REGEN_FILTER_LCK_SHFT           5
#define HDMI_REG_LINE_WIDTH_1_BMSK              0x001F
#define HDMI_REG_LINE_WIDTH_1_SHFT              0

#define HDMI_REG_LINE_WIDTH_2_ADDR              0x08
#define HDMI_REG_LINE_WIDTH_2_BMSK              0x00FF
#define HDMI_REG_LINE_WIDTH_2_SHFT              0

#define HDMI_REG_FIELD0_HEIGHT_1_ADDR           0x09
#define HDMI_REG_FIELD0_HEIGHT_1_BMSK           0x001F
#define HDMI_REG_FIELD0_HEIGHT_1_SHFT           0
#define HDMI_REG_FIELD0_HEIGHT_2_ADDR           0x0A
#define HDMI_REG_FIELD0_HEIGHT_2_BMSK           0x00FF
#define HDMI_REG_FIELD0_HEIGHT_2_SHFT           0

#define HDMI_REG_FIELD1_HEIGHT1_ADDR            0x0B
#define HDMI_REG_DEEP_COLOR_MODE_BMSK           0x00C0
#define HDMI_REG_DEEP_COLOR_MODE_SHFT           6
#define HDMI_REG_HDMI_INTERLACED_BMSK           0x0020
#define HDMI_REG_HDMI_INTERLACED_SHFT           5

#define HDMI_REG_TOTAL_LINE_WIDTH_1_ADDR        0x1E
#define HDMI_REG_TOTAL_LINE_WIDTH_1_BMSK        0x003F
#define HDMI_REG_TOTAL_LINE_WIDTH_1_SHFT        0

#define HDMI_REG_TOTAL_LINE_WIDTH_2_ADDR        0x1F
#define HDMI_REG_TOTAL_LINE_WIDTH_2_BMSK        0x00FF
#define HDMI_REG_TOTAL_LINE_WIDTH_2_SHFT        0

#define HDMI_REG_FIELD0_TOTAL_HEIGHT_1_ADDR     0x26
#define HDMI_REG_FIELD0_TOT_HEIGHT_1_BMSK       0x003F
#define HDMI_REG_FIELD0_TOT_HEIGHT_1_SHFT       0

#define HDMI_REG_FIELD0_TOTAL_HEIGHT_2_ADDR     0x27
#define HDMI_REG_FIELD0_TOT_HEIGHT_2_BMSK       0x00FF
#define HDMI_REG_FIELD0_TOT_HEIGHT_2_SHFT       0

#define HDMI_REG_DIS_CABLE_DET_RST_ADDR         0x48
#define HDMI_DIS_CABLE_DET_RST_BMSK             0x0040
#define HDMI_DIS_CABLE_DET_RST_SHFT             6

#define HDMI_REG_TMDS_FREQ_ADDR                 0x51
#define HDMI_REG_TMDS_FREQ_BMSK                 0x00FF
#define HDMI_REG_TMDS_FREQ_SHFT                 0

#define HDMI_REG_TMDS_FREQ_FRAC_ADDR            0x52
#define HDMI_REG_TMDS_FREQ_0_BMSK               0x0080
#define HDMI_REG_TMDS_FREQ_0_SHFT               7
#define HDMI_REG_TMDS_FREQ_FRAC_BMSK            0x007F
#define HDMI_REG_TMDS_FREQ_FRAC_SHFT            0

#define HDMI_REG_RST_CTRLS_ADDR                 0x5A
#define HDMI_HDCP_REPT_EDID_RST_BMSK            0x0008
#define HDMI_HDCP_REPT_EDID_RST_SHFT            3

#define HDMI_REG_MUX_SPDIF_TO_I2S_ADDR          0x6E
#define HDMI_MUX_SPDIF_TO_I2S_EN_BMSK           0x0008
#define HDMI_MUX_SPDIF_TO_I2S_EN_SHFT           3

/* HDMI Repeater Map Registers */
#define HDMI_REG_HDCP_EDID_CTRLS_ADDR           0x74
#define HDMI_MAN_EDID_A_ENABLE_BMSK             0x0001
#define HDMI_MAN_EDID_A_ENABLE_SHFT             0

#define HDMI_REG_RO_EDID_DEBUG_2_ADDR           0x76
#define HDMI_EDID_A_ENABLE_BMSK                 0x0001
#define HDMI_EDID_A_ENABLE_SHFT                 0

/* CEC Map Registers */
#define CEC_REG_LOG_ADDR_MASK_ADDR              0x27
#define CEC_REG_LOG_ADDR_MASK2_BMSK             0x0040
#define CEC_REG_LOG_ADDR_MASK2_SHFT             6
#define CEC_REG_LOG_ADDR_MASK1_BMSK             0x0020
#define CEC_REG_LOG_ADDR_MASK1_SHFT             5
#define CEC_REG_LOG_ADDR_MASK0_BMSK             0x0010
#define CEC_REG_LOG_ADDR_MASK0_SHFT             4
#define CEC_REG_ERROR_REPORT_MODE_BMSK          0x0008
#define CEC_REG_ERROR_REPORT_MODE_SHFT          3
#define CEC_REG_ERROR_REPORT_DET_BMSK           0x0004
#define CEC_REG_ERROR_REPORT_DET_SHFT           2
#define CEC_REG_FORCE_NACK_BMSK                 0x0002
#define CEC_REG_FORCE_NACK_SHFT                 1
#define CEC_REG_FORCE_IGNORE_BMSK               0x0001
#define CEC_REG_FORCE_IGNORE_SHFT               0

#define CEC_REG_LOGICAL_ADDRESS0_1_ADDR         0x28
#define CEC_REG_LOGICAL_ADDRESS1_BMSK           0x00F0
#define CEC_REG_LOGICAL_ADDRESS1_SHFT           4
#define CEC_REG_LOGICAL_ADDRESS0_BMSK           0x000F
#define CEC_REG_LOGICAL_ADDRESS0_SHFT           0

#define CEC_REG_LOGICAL_ADDRESS2_ADDR           0x29
#define CEC_REG_LOGICAL_ADDRESS2_BMSK           0x000F
#define CEC_REG_LOGICAL_ADDRESS2_SHFT           0

#define CEC_REG_CEC_POWER_UP_ADDR               0x2A
#define CEC_REG_CEC_POWER_UP_BMSK               0x0001
#define CEC_REG_CEC_POWER_UP_SHFT               0

#define CEC_REG_CLR_RX_RDY_SFT_RST_ADDR         0x2C
#define CEC_REG_CEC_SOFT_RESET_BMSK             0x0001
#define CEC_REG_CEC_SOFT_RESET_SHFT             0


/* CP Map Registers */
#define CP_REG_CONTRAST                         0x3A
#define CP_REG_SATURATION                       0x3B
#define CP_REG_BRIGHTNESS                       0x3C
#define CP_REG_HUE                              0x3D
#define CP_REG_VID_ADJ                          0x3E
#define CP_CTR_VID_ADJ_EN                       0x80
#define CP_REG_STDI_CH_ADDR                     0xB1
#define CP_STDI_DVALID_CH1_BMSK                 0x0080
#define CP_STDI_DVALID_CH1_SHFT                 7

/* SDP Main Map */
#define SDP_RW_MAP_REG                          0x0e

/* SDP MAP 1 Registers */
#define SDP_RW_LOCK_UNLOCK_CLR_ADDR             0x43
#define SDP_RW_LOCK_UNLOCK_MASK_ADDR            0x44

/* SDP R/O Main Map Registers */
#define SDP_RO_MAIN_STATUS1_ADDR                0x10
#define SDP_RO_MAIN_COL_KILL_BMSK               0x0080
#define SDP_RO_MAIN_COL_KILL_SHFT               7
#define SDP_RO_MAIN_AD_RESULT_BMSK              0x0070
#define SDP_RO_MAIN_AD_RESULT_SHFT              4
#define SDP_RO_MAIN_FOLLOW_PW_BMSK              0x0008
#define SDP_RO_MAIN_FOLLOW_PW_SHFT              3
#define SDP_RO_MAIN_FSC_LOCK_BMSK               0x0004
#define SDP_RO_MAIN_FSC_LOCK_SHFT               2
#define SDP_RO_MAIN_LOST_LOCK_BMSK              0x0002
#define SDP_RO_MAIN_LOST_LOCK_SHFT              1
#define SDP_RO_MAIN_IN_LOCK_BMSK                0x0001
#define SDP_RO_MAIN_IN_LOCK_SHFT                0


/*
 * CSI Map Registers
 */
#define CSI_REG_TX_CFG1_ADDR                    0x00
#define CSI_CTRL_TX_PWRDN_BMSK                  0x0080
#define CSI_CTRL_TX_PWRDN_SHFT                  7
#define CSI_CTRL_AUTO_PARAMS_BMSK               0x0020
#define CSI_CTRL_AUTO_PARAMS_SHFT               5
#define CSI_CTRL_NUM_LANES_BMSK                 0x0007
#define CSI_CTRL_NUM_LANES_SHFT                 0

#define CSI_REG_TX_DPHY_PWDN_ADDR               0xF0
#define CSI_CTRL_DPHY_PWDN_BMSK                 0x0001
#define CSI_CTRL_DPHY_PWDN_SHFT                 0

enum adv7481_adresult {
	AD_NTSM_M_J   = 0x0,
	AD_NTSC_4_43  = 0x1,
	AD_PAL_M      = 0x2,
	AD_PAL_60     = 0x3,
	AD_PAL_B_G    = 0x4,
	AD_SECAM      = 0x5,
	AD_PAL_COMB_N = 0x6,
	AD_SECAM_525  = 0x7,
};

enum adv7481_color_depth {
	CD_8BIT  = 0x0,
	CD_10BIT = 0x1,
	CD_12BIT = 0x2,
	CD_16BIT = 0x3,
};

enum adv7481_intrq_dur_sel {
	AD_4_XTAL_PER       = 0x0,
	AD_16_XTAL_PER      = 0x1,
	AD_64_XTAL_PER      = 0x2,
	AD_ACTIVE_UNTIL_CLR = 0x3,
};

enum adv7481_intrq_op_sel {
	AD_OP_OPEN_DRAIN = 0x0,
	AD_OP_DRIVE_LOW  = 0x1,
	AD_OP_DRIVE_HIGH = 0x2,
	AD_OP_DISABLED   = 0x3,
};

enum adv7481_drv_llc_pad {
	AD_LLC_PAD_NOT_USED  = 0x0,
	AD_MIN_DRIVE_STRNGTH = 0x1,
	AD_MID_DRIVE_STRNGTH = 0x2,
	AD_MAX_DRIVE_STRNGTH = 0x3,
};

#endif
