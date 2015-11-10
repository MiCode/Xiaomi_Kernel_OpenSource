/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

/* Qualcomm Technologies, Inc. EMAC SGMII Controller driver.
 */

#include "emac_sgmii.h"
#include "emac_hw.h"

/* SGMII v2 common registers */
#define SGMII_CMN_ATB_SEL1                 0x0000
#define SGMII_CMN_ATB_SEL2                 0x0004
#define SGMII_CMN_FREQ_UPDATE              0x0008
#define SGMII_CMN_BG_TIMER                 0x000C
#define SGMII_CMN_SSC_EN_CENTER            0x0010
#define SGMII_CMN_SSC_ADJ_PER1             0x0014
#define SGMII_CMN_SSC_ADJ_PER2             0x0018
#define SGMII_CMN_SSC_PER1                 0x001C
#define SGMII_CMN_SSC_PER2                 0x0020
#define SGMII_CMN_SSC_STEP_SIZE1           0x0024
#define SGMII_CMN_SSC_STEP_SIZE2           0x0028
#define SGMII_CMN_POST_DIV                 0x002C
#define SGMII_CMN_POST_DIV_MUX             0x0030
#define SGMII_CMN_BIAS_EN_CLKBUFLR_EN      0x0034
#define SGMII_CMN_CLK_ENABLE1              0x0038
#define SGMII_CMN_SYS_CLK_CTRL             0x003C
#define SGMII_CMN_SYSCLK_BUF_ENABLE        0x0040
#define SGMII_CMN_PLL_EN                   0x0044
#define SGMII_CMN_PLL_IVCO                 0x0048
#define SGMII_CMN_LOCK_CMP1_MODE0          0x004C
#define SGMII_CMN_LOCK_CMP2_MODE0          0x0050
#define SGMII_CMN_LOCK_CMP3_MODE0          0x0054
#define SGMII_CMN_LOCK_CMP1_MODE1          0x0058
#define SGMII_CMN_LOCK_CMP2_MODE1          0x005C
#define SGMII_CMN_LOCK_CMP3_MODE1          0x0060
#define SGMII_CMN_LOCK_CMP1_MODE2          0x0064
#define SGMII_CMN_LOCK_CMP2_MODE2          0x0068
#define SGMII_CMN_LOCK_CMP3_MODE2          0x006C
#define SGMII_CMN_BG_TRIM                  0x0070
#define SGMII_CMN_CLK_EP_DIV               0x0074
#define SGMII_CMN_CP_CTRL_MODE0            0x0078
#define SGMII_CMN_CP_CTRL_MODE1            0x007C
#define SGMII_CMN_CP_CTRL_MODE2            0x0080
#define SGMII_CMN_PLL_RCTRL_MODE0          0x0084
#define SGMII_CMN_PLL_RCTRL_MODE1          0x0088
#define SGMII_CMN_PLL_RCTRL_MODE2          0x008C
#define SGMII_CMN_PLL_CCTRL_MODE0          0x0090
#define SGMII_CMN_PLL_CCTRL_MODE1          0x0094
#define SGMII_CMN_PLL_CCTRL_MODE2          0x0098
#define SGMII_CMN_PLL_CNTRL                0x009C
#define SGMII_CMN_PHASE_SEL_CTRL           0x00A0
#define SGMII_CMN_PHASE_SEL_DC             0x00A4
#define SGMII_CMN_CORE_CLK_IN_SYNC_SEL     0x00A8
#define SGMII_CMN_SYSCLK_EN_SEL            0x00AC
#define SGMII_CMN_CML_SYSCLK_SEL           0x00B0
#define SGMII_CMN_RESETSM_CNTRL            0x00B4
#define SGMII_CMN_RESETSM_CNTRL2           0x00B8
#define SGMII_CMN_RESTRIM_CTRL             0x00BC
#define SGMII_CMN_RESTRIM_CTRL2            0x00C0
#define SGMII_CMN_RESCODE_DIV_NUM          0x00C4
#define SGMII_CMN_LOCK_CMP_EN              0x00C8
#define SGMII_CMN_LOCK_CMP_CFG             0x00CC
#define SGMII_CMN_DEC_START_MODE0          0x00D0
#define SGMII_CMN_DEC_START_MODE1          0x00D4
#define SGMII_CMN_DEC_START_MODE2          0x00D8
#define SGMII_CMN_DIV_FRAC_START1_MODE0    0x00DC
#define SGMII_CMN_DIV_FRAC_START2_MODE0    0x00E0
#define SGMII_CMN_DIV_FRAC_START3_MODE0    0x00E4
#define SGMII_CMN_DIV_FRAC_START1_MODE1    0x00E8
#define SGMII_CMN_DIV_FRAC_START2_MODE1    0x00EC
#define SGMII_CMN_DIV_FRAC_START3_MODE1    0x00F0
#define SGMII_CMN_DIV_FRAC_START1_MODE2    0x00F4
#define SGMII_CMN_DIV_FRAC_START2_MODE2    0x00F8
#define SGMII_CMN_DIV_FRAC_START3_MODE2    0x00FC
#define SGMII_CMN_INTEGLOOP_INITVAL        0x0100
#define SGMII_CMN_INTEGLOOP_EN             0x0104
#define SGMII_CMN_INTEGLOOP_GAIN0_MODE0    0x0108
#define SGMII_CMN_INTEGLOOP_GAIN1_MODE0    0x010C
#define SGMII_CMN_INTEGLOOP_GAIN0_MODE1    0x0110
#define SGMII_CMN_INTEGLOOP_GAIN1_MODE1    0x0114
#define SGMII_CMN_INTEGLOOP_GAIN0_MODE2    0x0118
#define SGMII_CMN_INTEGLOOP_GAIN1_MODE2    0x011C
#define SGMII_CMN_RES_TRIM_CONTROL2        0x0120
#define SGMII_CMN_VCO_TUNE_CTRL            0x0124
#define SGMII_CMN_VCO_TUNE_MAP             0x0128
#define SGMII_CMN_VCO_TUNE1_MODE0          0x012C
#define SGMII_CMN_VCO_TUNE2_MODE0          0x0130
#define SGMII_CMN_VCO_TUNE1_MODE1          0x0134
#define SGMII_CMN_VCO_TUNE2_MODE1          0x0138
#define SGMII_CMN_VCO_TUNE1_MODE2          0x013C
#define SGMII_CMN_VCO_TUNE2_MODE2          0x0140
#define SGMII_CMN_VCO_TUNE_TIMER1          0x0144
#define SGMII_CMN_VCO_TUNE_TIMER2          0x0148
#define SGMII_CMN_SAR                      0x014C
#define SGMII_CMN_SAR_CLK                  0x0150
#define SGMII_CMN_SAR_CODE_OUT_STATUS      0x0154
#define SGMII_CMN_SAR_CODE_READY_STATUS    0x0158
#define SGMII_CMN_CMN_STATUS               0x015C
#define SGMII_CMN_RESET_SM_STATUS          0x0160
#define SGMII_CMN_RESTRIM_CODE_STATUS      0x0164
#define SGMII_CMN_PLLCAL_CODE1_STATUS      0x0168
#define SGMII_CMN_PLLCAL_CODE2_STATUS      0x016C
#define SGMII_CMN_BG_CTRL                  0x0170
#define SGMII_CMN_CLK_SELECT               0x0174
#define SGMII_CMN_HSCLK_SEL                0x0178
#define SGMII_CMN_INTEGLOOP_BINCODE_STATUS 0x017C
#define SGMII_CMN_PLL_ANALOG               0x0180
#define SGMII_CMN_CORECLK_DIV              0x0184
#define SGMII_CMN_SW_RESET                 0x0188
#define SGMII_CMN_CORE_CLK_EN              0x018C
#define SGMII_CMN_C_READY_STATUS           0x0190
#define SGMII_CMN_CMN_CONFIG               0x0194
#define SGMII_CMN_CMN_RATE_OVERRIDE        0x0198
#define SGMII_CMN_SVS_MODE_CLK_SEL         0x019C
#define SGMII_CMN_DEBUG_BUS0               0x01A0
#define SGMII_CMN_DEBUG_BUS1               0x01A4
#define SGMII_CMN_DEBUG_BUS2               0x01A8
#define SGMII_CMN_DEBUG_BUS3               0x01AC
#define SGMII_CMN_DEBUG_BUS_SEL            0x01B0
#define SGMII_CMN_CMN_MISC1                0x01B4
#define SGMII_CMN_CMN_MISC2                0x01B8
#define SGMII_CMN_CORECLK_DIV_MODE1        0x01BC
#define SGMII_CMN_CORECLK_DIV_MODE2        0x01C0

/* SGMII v2 per lane registers */
#define SGMII_LN_ATB_SEL1                 0x0000
#define SGMII_LN_ATB_SEL2                 0x0004
#define SGMII_LN_ATB_SEL3                 0x0008
#define SGMII_LN_DRVR_CTRL0               0x000C
#define SGMII_LN_DRVR_CTRL1               0x0010
#define SGMII_LN_DRVR_CTRL2               0x0014
#define SGMII_LN_DRVR_TAP_EN              0x0018
#define SGMII_LN_TX_MARGINING             0x001C
#define SGMII_LN_TX_PRE                   0x0020
#define SGMII_LN_TX_POST                  0x0024
#define SGMII_LN_BIST_PATTERN1            0x0028
#define SGMII_LN_BIST_PATTERN2            0x002C
#define SGMII_LN_BIST_PATTERN3            0x0030
#define SGMII_LN_BIST_PATTERN4            0x0034
#define SGMII_LN_BIST_PATTERN5            0x0038
#define SGMII_LN_BIST_MODE_LANENO_SWAP    0x003C
#define SGMII_LN_BIST_INV_ERR_INJ_AUXMODE 0x0040
#define SGMII_LN_PRBS_SEED1               0x0044
#define SGMII_LN_PRBS_SEED2               0x0048
#define SGMII_LN_PRBS_SEED3               0x004C
#define SGMII_LN_PRBS_SEED4               0x0050
#define SGMII_LN_PERL_LENGTH1             0x0054
#define SGMII_LN_PERL_LENGTH2             0x0058
#define SGMII_LN_RCV_DETECT_LVL           0x005C
#define SGMII_LN_TX_BAND_MODE             0x0060
#define SGMII_LN_LANE_MODE                0x0064
#define SGMII_LN_LPB_EN                   0x0068
#define SGMII_LN_LPB_EN1                  0x006C
#define SGMII_LN_LPB_SEL                  0x0070
#define SGMII_LN_PARRATE_REC_DETECT_IDLE_EN  0x0074
#define SGMII_LN_PARALLEL_RATE         0x0078
#define SGMII_LN_CLKBUF_ENABLE         0x007C
#define SGMII_LN_RESET_TSYNC_EN        0x0080
#define SGMII_LN_SERDES_BYP_EN_OUT     0x0084
#define SGMII_LN_HIGHZ_TRANSCEIVEREN_BIAS_DRVR_EN  0x0088
#define SGMII_LN_RESET_GEN             0x08C
#define SGMII_LN_DEBUGBUS_SEL          0x0090
#define SGMII_LN_CTRL_OUT_OVRD0        0x0094
#define SGMII_LN_CTRL_OUT_OVRD1        0x0098
#define SGMII_LN_BIST_STATUS           0x009C
#define SGMII_LN_BIST_ERR_CNT1_STATUS  0x00A0
#define SGMII_LN_BIST_ERR_CNT2_STATUS  0x00A4
#define SGMII_LN_DEBUG_BUS1            0x00A8
#define SGMII_LN_DEBUG_BUS2            0x00AC
#define SGMII_LN_DEBUG_BUS3            0x00B0
#define SGMII_LN_DEBUG_BUS4            0x00B4
#define SGMII_LN_CML_CTRL_MODE0        0x00B8
#define SGMII_LN_CML_CTRL_MODE1        0x00BC
#define SGMII_LN_CML_CTRL_MODE2        0x00C0
#define SGMII_LN_PREAMP_CTRL_MODE0     0x00C4
#define SGMII_LN_PREAMP_CTRL_MODE1     0x00C8
#define SGMII_LN_PREAMP_CTRL_MODE2     0x00CC
#define SGMII_LN_MIXER_CTRL_MODE0      0x00D0
#define SGMII_LN_MIXER_CTRL_MODE1      0x00D4
#define SGMII_LN_MIXER_CTRL_MODE2      0x00D8
#define SGMII_LN_MAIN_THRESH1          0x00DC
#define SGMII_LN_MAIN_THRESH2          0x00E0
#define SGMII_LN_POST_THRESH1          0x00E4
#define SGMII_LN_POST_THRESH2          0x00E8
#define SGMII_LN_PRE_THRESH1           0x00EC
#define SGMII_LN_PRE_THRESH2           0x00F0
#define SGMII_LN_CTLE_THRESH_DFE       0x00F4
#define SGMII_LN_VGA_THRESH_DFE        0x00F8
#define SGMII_LN_CTLE_THRESH           0x00FC
#define SGMII_LN_RXENGINE_EN0          0x0100
#define SGMII_LN_RXENGINE_EN1          0x0104
#define SGMII_LN_RXENGINE_EN2          0x0108
#define SGMII_LN_CTLE_TRAIN_TIME       0x010C
#define SGMII_LN_CTLE_DFE_OVRLP_TIME   0x0110
#define SGMII_LN_DFE_REFRESH_TIME      0x0114
#define SGMII_LN_DFE_ENABLE_TIME       0x0118
#define SGMII_LN_VGA_GAIN              0x011C
#define SGMII_LN_DFE_GAIN              0x0120
#define SGMII_LN_DFE_GAIN_SIGN         0x0124
#define SGMII_LN_EQ_GAIN               0x0128
#define SGMII_LN_OFFSET_GAIN           0x012C
#define SGMII_LN_PRE_GAIN              0x0130
#define SGMII_LN_VGA_INITVAL           0x0134
#define SGMII_LN_DFE_TAP1_INITVAL      0x0138
#define SGMII_LN_DFE_TAP2_INITVAL      0x013C
#define SGMII_LN_DFE_TAP3_INITVAL      0x0140
#define SGMII_LN_DFE_TAP4_INITVAL      0x0144
#define SGMII_LN_DFE_TAP5_INITVAL      0x0148
#define SGMII_LN_EQ_INITVAL            0x014C
#define SGMII_LN_OFFSET_INITVAL        0x0150
#define SGMII_LN_PRE_INITVAL           0x0154
#define SGMII_LN_EDAC_INITVAL          0x0158
#define SGMII_LN_CTLE_SATVAL           0x015C
#define SGMII_LN_RXEQ_INITB0           0x0160
#define SGMII_LN_RXEQ_INITB1           0x0164
#define SGMII_LN_RXEQ_CLK_DIV1         0x0168
#define SGMII_LN_RXEQ_CLK_DIV2         0x016C
#define SGMII_LN_RCVRDONE_THRESH1      0x0170
#define SGMII_LN_RCVRDONE_THRESH2      0x0174
#define SGMII_LN_RXEQ_CTRL             0x0178
#define SGMII_LN_UCDR_FO_GAIN_MODE0    0x017C
#define SGMII_LN_UCDR_FO_GAIN_MODE1    0x0180
#define SGMII_LN_UCDR_FO_GAIN_MODE2    0x0184
#define SGMII_LN_UCDR_SO_GAIN_MODE0    0x0188
#define SGMII_LN_UCDR_SO_GAIN_MODE1    0x018C
#define SGMII_LN_UCDR_SO_GAIN_MODE2    0x0190
#define SGMII_LN_UCDR_SO_CONFIG        0x0194
#define SGMII_LN_UCDR_FO_TO_SO_DELAY   0x0198
#define SGMII_LN_RX_BAND               0x019C
#define SGMII_LN_AUX_ENABLE            0x01A0
#define SGMII_LN_AUX_CONTROL           0x01A4
#define SGMII_LN_AUX_DAC               0x01A8
#define SGMII_LN_AC_JTAG_CTRL          0x01AC
#define SGMII_LN_RX_RCVR_PATH0         0x01B0
#define SGMII_LN_RX_RCVR_PATH1         0x01B4
#define SGMII_LN_RX_RCVR_PATH1_MODE0   0x01B8
#define SGMII_LN_RX_RCVR_PATH1_MODE1   0x01BC
#define SGMII_LN_RX_RCVR_PATH1_MODE2   0x01C0
#define SGMII_LN_SAMPCAL_BYP_CODE_I0   0x01C4
#define SGMII_LN_SAMPCAL_BYP_CODE_I0B  0x01C8
#define SGMII_LN_SAMPCAL_BYP_CODE_I1   0x01CC
#define SGMII_LN_SAMPCAL_BYP_CODE_I1B  0x01D0
#define SGMII_LN_SAMPCAL_BYP_CODE_Q    0x01D4
#define SGMII_LN_SAMPCAL_BYP_CODE_QB   0x01D8
#define SGMII_LN_SAMPCAL_BYP_CODE_A    0x01DC
#define SGMII_LN_SAMPCAL_BYP_CODE_AB   0x01E0
#define SGMII_LN_SAMPCAL_BYP_CODE_E    0x01E4
#define SGMII_LN_SAMPCAL_BYP_CODE_EB   0x01E8
#define SGMII_LN_RX_EOM_START          0x01EC
#define SGMII_LN_RSM_CONFIG            0x01F0
#define SGMII_LN_RX_EOM_MEAS_TIME0     0x01F4
#define SGMII_LN_RX_EOM_MEAS_TIME1     0x01F8
#define SGMII_LN_RX_EOM_CTRL           0x01FC
#define SGMII_LN_RX_HIGHZ_HIGHRATE     0x0200
#define SGMII_LN_RX_SAMPCAL_IDAC_SIGN1 0x0204
#define SGMII_LN_RX_SAMPCAL_IDAC_SIGN2 0x0208
#define SGMII_LN_RX_SAMPCAL_CONFIG     0x020C
#define SGMII_LN_RX_SAMPCAL_TSETTLE    0x0210
#define SGMII_LN_RX_SAMPCAL_ENDSAMP1   0x0214
#define SGMII_LN_RX_SAMPCAL_ENDSAMP2   0x0218
#define SGMII_LN_RX_SAMPCAL_MIDPOINT1  0x021C
#define SGMII_LN_RX_SAMPCAL_MIDPOINT2  0x0220
#define SGMII_LN_SIGDET_ENABLES        0x0224
#define SGMII_LN_SIGDET_CNTRL          0x0228
#define SGMII_LN_SIGDET_DEGLITCH_CNTRL 0x022C
#define SGMII_LN_CDR_FREEZE_UP_DN      0x0230
#define SGMII_LN_RX_INTERFACE_MODE     0x0234
#define SGMII_LN_JITTER_GEN_MODE       0x0238
#define SGMII_LN_BUJ_AMP               0x023C
#define SGMII_LN_SJ_AMP1               0x0240
#define SGMII_LN_SJ_AMP2               0x0244
#define SGMII_LN_SJ_PER1               0x0248
#define SGMII_LN_SJ_PER2               0x024C
#define SGMII_LN_BUJ_STEP_FREQ1        0x0250
#define SGMII_LN_BUJ_STEP_FREQ2        0x0254
#define SGMII_LN_PPM_OFFSET1           0x0258
#define SGMII_LN_PPM_OFFSET2           0x025C
#define SGMII_LN_SIGN_PPM_PERIOD1      0x0260
#define SGMII_LN_SIGN_PPM_PERIOD2      0x0264
#define SGMII_LN_SSC_CTRL              0x0268
#define SGMII_LN_SSC_COUNT1            0x026C
#define SGMII_LN_SSC_COUNT2            0x0270
#define SGMII_LN_DLL_CTUNE_OVRWRT      0x0274
#define SGMII_LN_DLL0_FTUNE_INITVAL    0x0278
#define SGMII_LN_DLL1_FTUNE_INITVAL    0x027C
#define SGMII_LN_DLL0_FTUNE_GAIN       0x0280
#define SGMII_LN_DLL1_FTUNE_GAIN       0x0284
#define SGMII_LN_DLL_ENABLE_WAIT       0x0288
#define SGMII_LN_DLL_CTUNE_OFFSET      0x028C
#define SGMII_LN_DLL_CTUNE_CTRL        0x0290
#define SGMII_LN_DCC_INIT              0x0294
#define SGMII_LN_DCC_GAIN              0x0298
#define SGMII_LN_RSM_START             0x029C
#define SGMII_LN_RX_EN_SIGNAL          0x02A0
#define SGMII_LN_PSM_RX_EN_CAL         0x02A4
#define SGMII_LN_CLK_SHIFT             0x02A8
#define SGMII_LN_RX_MISC_CNTRL0        0x02AC
#define SGMII_LN_RX_MISC_CNTRL1        0x02B0
#define SGMII_LN_TS0_TIMER             0x02B4
#define SGMII_LN_DLL_HIGHDATARATE      0x02B8
#define SGMII_LN_DRVR_LOGIC_CLKDIV     0x02BC
#define SGMII_LN_TX_ADAPTOR_STATUS     0x02C0
#define SGMII_LN_CAL_STATUS            0x02C4
#define SGMII_LN_AC_JTAG_OUT_STATUS    0x02C8
#define SGMII_LN_EOM_STATUS            0x02CC
#define SGMII_LN_EOM_ERR_CNT0_STATUS   0x02D0
#define SGMII_LN_EOM_ERR_CNT1_STATUS   0x02D4

/* SGMII v2 PHY common registers */
#define SGMII_PHY_CMN_SERDES_START    0x0400
#define SGMII_PHY_CMN_RESERVED_4      0x0404
#define SGMII_PHY_CMN_CTRL            0x0408
#define SGMII_PHY_CMN_TEST_DEBUG_CTRL 0x040C
#define SGMII_PHY_CMN_RESET_CTRL      0x0410
#define SGMII_PHY_CMN_RESERVED_5      0x0414
#define SGMII_PHY_CMN_RESERVED_0      0x0418
#define SGMII_PHY_CMN_RESERVED_1      0x041C
#define SGMII_PHY_CMN_RESERVED_2      0x0420
#define SGMII_PHY_CMN_RESERVED_3      0x0424
#define SGMII_PHY_CMN_REVISION_ID0    0x0428
#define SGMII_PHY_CMN_REVISION_ID1    0x042C
#define SGMII_PHY_CMN_REVISION_ID2    0x0430
#define SGMII_PHY_CMN_REVISION_ID3    0x0434
#define SGMII_PHY_CMN_DEBUG_BUS_STAT0 0x0438
#define SGMII_PHY_CMN_DEBUG_BUS_STAT1 0x043C
#define SGMII_PHY_CMN_DEBUG_BUS_STAT2 0x0440
#define SGMII_PHY_CMN_DEBUG_BUS_STAT3 0x0444

/* SGMII v2 PHY registers per lane */
#define SGMII_PHY_LN_OFFSET          0x0400
#define SGMII_PHY_LN_LANE_STATUS     0x00DC
#define SGMII_PHY_LN_BIST_GEN0       0x008C
#define SGMII_PHY_LN_BIST_GEN1       0x0090
#define SGMII_PHY_LN_BIST_GEN2       0x0094
#define SGMII_PHY_LN_BIST_GEN3       0x0098
#define SGMII_PHY_LN_CDR_CTRL1       0x005C

static struct emac_reg_write sgmii_cmn[] = {
	/* Sysclk/Refclk Settings */
	{0x0AC, 0x00},      /* SGMII_CMN_SYSCLK_EN_SEL */
	{0x03C, 0x06},      /* SGMII_CMN_SYS_CLK_CTRL */
	{0x034, 0x1C},      /* SGMII_CMN_BIAS_EN_CLKBUFLR_EN */
	{0x038, 0x10},      /* SGMII_CMN_CLK_ENABLE1 */
	{0x178, 0x03},      /* SGMII_CMN_HSCLK_SEL */

	/* PLL Settings */
	{0x048, 0xF0},      /* SGMII_CMN_PLL_IVCO */
	{0x070, 0xF0},      /* SGMII_CMN_BG_TRIM */
	{0x18C, 0x12},      /* SGMII_CMN_CORE_CLK_EN */
	{0x090, 0x01},      /* SGMII_CMN_PLL_CCTRL_MODE0 */
	{0x084, 0x10},      /* SGMII_CMN_PLL_RCTRL_MODE0 */
	{0x078, 0x23},      /* SGMII_CMN_CP_CTRL_MODE0 */
	{0x0D0, 0x7D},      /* SGMII_CMN_DEC_START_MODE0 */
	{0x0DC, 0x00},      /* SGMII_CMN_DIV_FRAC_START1_MODE0 */
	{0x0E0, 0x00},      /* SGMII_CMN_DIV_FRAC_START2_MODE0 */
	{0x0E4, 0x00},      /* SGMII_CMN_DIV_FRAC_START3_MODE0 */
	{0x04C, 0x80},      /* SGMII_CMN_LOCK_CMP1_MODE0 */
	{0x050, 0x0C},      /* SGMII_CMN_LOCK_CMP2_MODE0 */
	{0x054, 0x00},      /* SGMII_CMN_LOCK_CMP3_MODE0 */
	{0x108, 0x98},      /* SGMII_CMN_INTEGLOOP_GAIN0_MODE0 */
	{0x10C, 0x01},      /* SGMII_CMN_INTEGLOOP_GAIN1_MODE0 */
	{0x128, 0x00},      /* SGMII_CMN_VCO_TUNE_MAP */
	{0x19C, 0x01},      /* SGMII_CMN_SVS_MODE_CLK_SEL */
	{0x0C8, 0x00},      /* SGMII_CMN_LOCK_CMP_EN */
	{0x0B8, 0x08},      /* SGMII_CMN_RESETSM_CNTRL2 */
	{END_MARKER, END_MARKER},
};

static struct emac_reg_write sgmii_laned[] = {
	/* CDR Settings */
	{0x17C, 0x8a},         /* SGMII_LN_UCDR_FO_GAIN_MODE0 */
	{0x188, 0x06},         /* SGMII_LN_UCDR_SO_GAIN_MODE0 */
	{0x194, 0x4c},         /* SGMII_LN_UCDR_SO_CONFIG */

	/* TX/RX Settings */
	{0x2A0, 0xC0},         /* SGMII_LN_RX_EN_SIGNAL */

	{0x00C, 0x11},         /* SGMII_LN_DRVR_CTRL0 */
	{0x018, 0x01},         /* SGMII_LN_DRVR_TAP_EN */
	{0x01C, 0x59},         /* SGMII_LN_TX_MARGINING */
	{0x020, 0x40},         /* SGMII_LN_TX_PRE */
	{0x024, 0x40},         /* SGMII_LN_TX_POST */

	{0x0B8, 0x09},         /* SGMII_LN_CML_CTRL_MODE0 */
	{0x0D0, 0x31},         /* MIXER_CTRL_MODE0 */
	{0x134, 0x1F},         /* SGMII_LN_VGA_INITVAL */
	{0x224, 0x21},         /* SGMII_LN_SIGDET_ENABLES */
	{0x228, 0x80},         /* SGMII_LN_SIGDET_CNTRL */

	{0x22C, 0x08},         /* SGMII_LN_SIGDET_DEGLITCH_CNTRL */
	{0x2AC, 0x00},         /* SGMII_LN_RX_MISC_CNTRL0 */
	{0x2BC, 0x14},         /* SGMII_LN_DRVR_LOGIC_CLKDIV */

	{0x078, 0x01},         /* SGMII_LN_PARALLEL_RATE */
	{0x060, 0x01},         /* SGMII_LN_TX_BAND_MODE */
	{0x19C, 0x02},         /* SGMII_LN_RX_BAND */
	{0x064, 0x1A},         /* SGMII_LN_LANE_MODE */
	{0x1B8, 0x40},         /* SGMII_LN_RX_RCVR_PATH1_MODE0 */
	{0x1F0, 0x03},         /* SGMII_LN_RSM_CONFIG */
	{END_MARKER, END_MARKER},
};

static struct emac_reg_write sgmii_phy[] = {
	{0x480, 0x01},	/* SGMII_PHY_LN_POW_DWN_CTRL0 */
	{0x458, 0x0F},	/* SGMII_PHY_LN_CDR_CTRL0 */
	{0x40C, 0x00},	/* SGMII_PHY_LN_TX_PWR_CTRL */
	{0x418, 0x40},	/* SGMII_PHY_LN_LANE_CTRL1: */
	{END_MARKER, END_MARKER},
};

static int emac_sgmii_v2_init(struct emac_adapter *adpt)
{
	struct emac_phy *phy = &adpt->phy;
	struct emac_sgmii *sgmii = phy->private;
	void __iomem *phy_regs = sgmii->base + 0x300;
	u32 lnstatus, i;
	static bool sgmii_cmn_init_done;

	emac_sgmii_init_link(adpt, phy->autoneg_advertised, phy->autoneg,
			     !phy->disable_fc_autoneg);

	/* PCS Programing */
	if (!sgmii_cmn_init_done)
		writel_relaxed(0x04, sgmii->base + SGMII_PHY_CMN_CTRL);
		wmb(); /* ensure PCS is programmed before setting the lanes */

	/* PCS lane-x init */
	emac_reg_write_all(sgmii->laned, sgmii_phy);

	/* SGMII common init */
	if (!sgmii_cmn_init_done)
		emac_reg_write_all(sgmii->base, sgmii_cmn);

	/* SGMII lane-x init */
	emac_reg_write_all(sgmii->laned, sgmii_laned);

	/* First time reset common PHY */
	if (!sgmii_cmn_init_done) {
		writel_relaxed(0x00, sgmii->base + SGMII_PHY_CMN_RESET_CTRL);
		wmb(); /* ensure reset is written before power up */
		sgmii_cmn_init_done = true;
	}

	/* Power up PCS and start reset lane state machine */
	writel_relaxed(0x00, phy_regs + EMAC_SGMII_PHY_RESET_CTRL);
	writel_relaxed(0x01, sgmii->laned + SGMII_LN_RSM_START);
	wmb(); /* ensure power up is written before checking lane status */

	/* Wait for c_ready assertion */
	for (i = 0; i < SERDES_START_WAIT_TIMES; i++) {
		lnstatus = readl_relaxed(phy_regs + SGMII_PHY_LN_LANE_STATUS);
		rmb(); /* ensure status read is complete before testing it */
		if (lnstatus & 0x02)
			break;
		usleep_range(100, 200);
	}

	if (i == SERDES_START_WAIT_TIMES) {
		emac_err(adpt, "SGMII failed to start\n");
		return -EIO;
	}

	/* Disable digital and SERDES loopback */
	writel_relaxed(0x0, phy_regs + SGMII_PHY_LN_BIST_GEN0);
	writel_relaxed(0x0, phy_regs + SGMII_PHY_LN_BIST_GEN2);
	writel_relaxed(0x0, phy_regs + SGMII_PHY_LN_CDR_CTRL1);

	/* Mask out all the SGMII Interrupt */
	writel_relaxed(0x0, phy_regs + EMAC_SGMII_PHY_INTERRUPT_MASK);
	wmb(); /* ensure writes are flushed to hw */

	emac_hw_clear_sgmii_intr_status(adpt, SGMII_PHY_INTERRUPT_ERR);

	return 0;
}

static void emac_sgmii_v2_reset(struct emac_adapter *adpt)
{
	emac_clk_set_rate(adpt, EMAC_CLK_125M, EMC_CLK_RATE_19_2MHZ);
	emac_sgmii_reset_prepare(adpt);
	emac_sgmii_v2_init(adpt);
	emac_clk_set_rate(adpt, EMAC_CLK_125M, EMC_CLK_RATE_125MHZ);
}

int emac_sgmii_v2_link_setup_no_ephy(struct emac_adapter *adpt, u32 speed,
				     bool autoneg)
{
	struct emac_phy *phy = &adpt->phy;

	phy->autoneg		= autoneg;
	phy->autoneg_advertised	= speed;
	/* The AN_ENABLE and SPEED_CFG can't change on fly. The SGMII_PHY has
	 * to be re-initialized.
	 */
	emac_sgmii_reset_prepare(adpt);
	return emac_sgmii_v2_init(adpt);
}

struct emac_phy_ops emac_sgmii_v2_ops = {
	.config			= emac_sgmii_config,
	.up			= emac_sgmii_up,
	.down			= emac_sgmii_down,
	.init			= emac_sgmii_v2_init,
	.reset			= emac_sgmii_v2_reset,
	.init_ephy		= emac_sgmii_init_ephy_nop,
	.link_setup_no_ephy	= emac_sgmii_v2_link_setup_no_ephy,
	.link_check_no_ephy	= emac_sgmii_link_check_no_ephy,
	.tx_clk_set_rate	= emac_sgmii_tx_clk_set_rate_nop,
	.periodic_task		= emac_sgmii_periodic_check,
};
