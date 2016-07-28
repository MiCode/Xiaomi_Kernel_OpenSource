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

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/iopoll.h>
#include <linux/clk/msm-clk-provider.h>
#include <linux/clk/msm-clk.h>
#include <linux/clk/msm-clock-generic.h>
#include <dt-bindings/clock/msm-clocks-8996.h>

#include "mdss-pll.h"
#include "mdss-hdmi-pll.h"

/* CONSTANTS */
#define HDMI_BIT_CLK_TO_PIX_CLK_RATIO            10
#define HDMI_HIGH_FREQ_BIT_CLK_THRESHOLD         3400000000UL
#define HDMI_DIG_FREQ_BIT_CLK_THRESHOLD          1500000000UL
#define HDMI_MID_FREQ_BIT_CLK_THRESHOLD          750000000
#define HDMI_CLKS_PLL_DIVSEL                     0
#define HDMI_CORECLK_DIV                         5
#define HDMI_REF_CLOCK                           19200000
#define HDMI_64B_ERR_VAL                         0xFFFFFFFFFFFFFFFF
#define HDMI_VERSION_8996_V1                     1
#define HDMI_VERSION_8996_V2                     2
#define HDMI_VERSION_8996_V3                     3
#define HDMI_VERSION_8996_V3_1_8                 4

#define HDMI_VCO_MAX_FREQ                        12000000000
#define HDMI_VCO_MIN_FREQ                        8000000000
#define HDMI_2400MHZ_BIT_CLK_HZ                  2400000000UL
#define HDMI_2250MHZ_BIT_CLK_HZ                  2250000000UL
#define HDMI_2000MHZ_BIT_CLK_HZ                  2000000000UL
#define HDMI_1700MHZ_BIT_CLK_HZ                  1700000000UL
#define HDMI_1200MHZ_BIT_CLK_HZ                  1200000000UL
#define HDMI_1334MHZ_BIT_CLK_HZ                  1334000000UL
#define HDMI_1000MHZ_BIT_CLK_HZ                  1000000000UL
#define HDMI_850MHZ_BIT_CLK_HZ                   850000000
#define HDMI_667MHZ_BIT_CLK_HZ                   667000000
#define HDMI_600MHZ_BIT_CLK_HZ                   600000000
#define HDMI_500MHZ_BIT_CLK_HZ                   500000000
#define HDMI_450MHZ_BIT_CLK_HZ                   450000000
#define HDMI_334MHZ_BIT_CLK_HZ                   334000000
#define HDMI_300MHZ_BIT_CLK_HZ                   300000000
#define HDMI_282MHZ_BIT_CLK_HZ                   282000000
#define HDMI_250MHZ_BIT_CLK_HZ                   250000000
#define HDMI_KHZ_TO_HZ                           1000

/* PLL REGISTERS */
#define QSERDES_COM_ATB_SEL1                     (0x000)
#define QSERDES_COM_ATB_SEL2                     (0x004)
#define QSERDES_COM_FREQ_UPDATE                  (0x008)
#define QSERDES_COM_BG_TIMER                     (0x00C)
#define QSERDES_COM_SSC_EN_CENTER                (0x010)
#define QSERDES_COM_SSC_ADJ_PER1                 (0x014)
#define QSERDES_COM_SSC_ADJ_PER2                 (0x018)
#define QSERDES_COM_SSC_PER1                     (0x01C)
#define QSERDES_COM_SSC_PER2                     (0x020)
#define QSERDES_COM_SSC_STEP_SIZE1               (0x024)
#define QSERDES_COM_SSC_STEP_SIZE2               (0x028)
#define QSERDES_COM_POST_DIV                     (0x02C)
#define QSERDES_COM_POST_DIV_MUX                 (0x030)
#define QSERDES_COM_BIAS_EN_CLKBUFLR_EN          (0x034)
#define QSERDES_COM_CLK_ENABLE1                  (0x038)
#define QSERDES_COM_SYS_CLK_CTRL                 (0x03C)
#define QSERDES_COM_SYSCLK_BUF_ENABLE            (0x040)
#define QSERDES_COM_PLL_EN                       (0x044)
#define QSERDES_COM_PLL_IVCO                     (0x048)
#define QSERDES_COM_LOCK_CMP1_MODE0              (0x04C)
#define QSERDES_COM_LOCK_CMP2_MODE0              (0x050)
#define QSERDES_COM_LOCK_CMP3_MODE0              (0x054)
#define QSERDES_COM_LOCK_CMP1_MODE1              (0x058)
#define QSERDES_COM_LOCK_CMP2_MODE1              (0x05C)
#define QSERDES_COM_LOCK_CMP3_MODE1              (0x060)
#define QSERDES_COM_LOCK_CMP1_MODE2              (0x064)
#define QSERDES_COM_CMN_RSVD0                    (0x064)
#define QSERDES_COM_LOCK_CMP2_MODE2              (0x068)
#define QSERDES_COM_EP_CLOCK_DETECT_CTRL         (0x068)
#define QSERDES_COM_LOCK_CMP3_MODE2              (0x06C)
#define QSERDES_COM_SYSCLK_DET_COMP_STATUS       (0x06C)
#define QSERDES_COM_BG_TRIM                      (0x070)
#define QSERDES_COM_CLK_EP_DIV                   (0x074)
#define QSERDES_COM_CP_CTRL_MODE0                (0x078)
#define QSERDES_COM_CP_CTRL_MODE1                (0x07C)
#define QSERDES_COM_CP_CTRL_MODE2                (0x080)
#define QSERDES_COM_CMN_RSVD1                    (0x080)
#define QSERDES_COM_PLL_RCTRL_MODE0              (0x084)
#define QSERDES_COM_PLL_RCTRL_MODE1              (0x088)
#define QSERDES_COM_PLL_RCTRL_MODE2              (0x08C)
#define QSERDES_COM_CMN_RSVD2                    (0x08C)
#define QSERDES_COM_PLL_CCTRL_MODE0              (0x090)
#define QSERDES_COM_PLL_CCTRL_MODE1              (0x094)
#define QSERDES_COM_PLL_CCTRL_MODE2              (0x098)
#define QSERDES_COM_CMN_RSVD3                    (0x098)
#define QSERDES_COM_PLL_CNTRL                    (0x09C)
#define QSERDES_COM_PHASE_SEL_CTRL               (0x0A0)
#define QSERDES_COM_PHASE_SEL_DC                 (0x0A4)
#define QSERDES_COM_CORE_CLK_IN_SYNC_SEL         (0x0A8)
#define QSERDES_COM_BIAS_EN_CTRL_BY_PSM          (0x0A8)
#define QSERDES_COM_SYSCLK_EN_SEL                (0x0AC)
#define QSERDES_COM_CML_SYSCLK_SEL               (0x0B0)
#define QSERDES_COM_RESETSM_CNTRL                (0x0B4)
#define QSERDES_COM_RESETSM_CNTRL2               (0x0B8)
#define QSERDES_COM_RESTRIM_CTRL                 (0x0BC)
#define QSERDES_COM_RESTRIM_CTRL2                (0x0C0)
#define QSERDES_COM_RESCODE_DIV_NUM              (0x0C4)
#define QSERDES_COM_LOCK_CMP_EN                  (0x0C8)
#define QSERDES_COM_LOCK_CMP_CFG                 (0x0CC)
#define QSERDES_COM_DEC_START_MODE0              (0x0D0)
#define QSERDES_COM_DEC_START_MODE1              (0x0D4)
#define QSERDES_COM_DEC_START_MODE2              (0x0D8)
#define QSERDES_COM_VCOCAL_DEADMAN_CTRL          (0x0D8)
#define QSERDES_COM_DIV_FRAC_START1_MODE0        (0x0DC)
#define QSERDES_COM_DIV_FRAC_START2_MODE0        (0x0E0)
#define QSERDES_COM_DIV_FRAC_START3_MODE0        (0x0E4)
#define QSERDES_COM_DIV_FRAC_START1_MODE1        (0x0E8)
#define QSERDES_COM_DIV_FRAC_START2_MODE1        (0x0EC)
#define QSERDES_COM_DIV_FRAC_START3_MODE1        (0x0F0)
#define QSERDES_COM_DIV_FRAC_START1_MODE2        (0x0F4)
#define QSERDES_COM_VCO_TUNE_MINVAL1             (0x0F4)
#define QSERDES_COM_DIV_FRAC_START2_MODE2        (0x0F8)
#define QSERDES_COM_VCO_TUNE_MINVAL2             (0x0F8)
#define QSERDES_COM_DIV_FRAC_START3_MODE2        (0x0FC)
#define QSERDES_COM_CMN_RSVD4                    (0x0FC)
#define QSERDES_COM_INTEGLOOP_INITVAL            (0x100)
#define QSERDES_COM_INTEGLOOP_EN                 (0x104)
#define QSERDES_COM_INTEGLOOP_GAIN0_MODE0        (0x108)
#define QSERDES_COM_INTEGLOOP_GAIN1_MODE0        (0x10C)
#define QSERDES_COM_INTEGLOOP_GAIN0_MODE1        (0x110)
#define QSERDES_COM_INTEGLOOP_GAIN1_MODE1        (0x114)
#define QSERDES_COM_INTEGLOOP_GAIN0_MODE2        (0x118)
#define QSERDES_COM_VCO_TUNE_MAXVAL1             (0x118)
#define QSERDES_COM_INTEGLOOP_GAIN1_MODE2        (0x11C)
#define QSERDES_COM_VCO_TUNE_MAXVAL2             (0x11C)
#define QSERDES_COM_RES_TRIM_CONTROL2            (0x120)
#define QSERDES_COM_VCO_TUNE_CTRL                (0x124)
#define QSERDES_COM_VCO_TUNE_MAP                 (0x128)
#define QSERDES_COM_VCO_TUNE1_MODE0              (0x12C)
#define QSERDES_COM_VCO_TUNE2_MODE0              (0x130)
#define QSERDES_COM_VCO_TUNE1_MODE1              (0x134)
#define QSERDES_COM_VCO_TUNE2_MODE1              (0x138)
#define QSERDES_COM_VCO_TUNE1_MODE2              (0x13C)
#define QSERDES_COM_VCO_TUNE_INITVAL1            (0x13C)
#define QSERDES_COM_VCO_TUNE2_MODE2              (0x140)
#define QSERDES_COM_VCO_TUNE_INITVAL2            (0x140)
#define QSERDES_COM_VCO_TUNE_TIMER1              (0x144)
#define QSERDES_COM_VCO_TUNE_TIMER2              (0x148)
#define QSERDES_COM_SAR                          (0x14C)
#define QSERDES_COM_SAR_CLK                      (0x150)
#define QSERDES_COM_SAR_CODE_OUT_STATUS          (0x154)
#define QSERDES_COM_SAR_CODE_READY_STATUS        (0x158)
#define QSERDES_COM_CMN_STATUS                   (0x15C)
#define QSERDES_COM_RESET_SM_STATUS              (0x160)
#define QSERDES_COM_RESTRIM_CODE_STATUS          (0x164)
#define QSERDES_COM_PLLCAL_CODE1_STATUS          (0x168)
#define QSERDES_COM_PLLCAL_CODE2_STATUS          (0x16C)
#define QSERDES_COM_BG_CTRL                      (0x170)
#define QSERDES_COM_CLK_SELECT                   (0x174)
#define QSERDES_COM_HSCLK_SEL                    (0x178)
#define QSERDES_COM_INTEGLOOP_BINCODE_STATUS     (0x17C)
#define QSERDES_COM_PLL_ANALOG                   (0x180)
#define QSERDES_COM_CORECLK_DIV                  (0x184)
#define QSERDES_COM_SW_RESET                     (0x188)
#define QSERDES_COM_CORE_CLK_EN                  (0x18C)
#define QSERDES_COM_C_READY_STATUS               (0x190)
#define QSERDES_COM_CMN_CONFIG                   (0x194)
#define QSERDES_COM_CMN_RATE_OVERRIDE            (0x198)
#define QSERDES_COM_SVS_MODE_CLK_SEL             (0x19C)
#define QSERDES_COM_DEBUG_BUS0                   (0x1A0)
#define QSERDES_COM_DEBUG_BUS1                   (0x1A4)
#define QSERDES_COM_DEBUG_BUS2                   (0x1A8)
#define QSERDES_COM_DEBUG_BUS3                   (0x1AC)
#define QSERDES_COM_DEBUG_BUS_SEL                (0x1B0)
#define QSERDES_COM_CMN_MISC1                    (0x1B4)
#define QSERDES_COM_CMN_MISC2                    (0x1B8)
#define QSERDES_COM_CORECLK_DIV_MODE1            (0x1BC)
#define QSERDES_COM_CORECLK_DIV_MODE2            (0x1C0)
#define QSERDES_COM_CMN_RSVD5                    (0x1C0)

/* Tx Channel base addresses */
#define HDMI_TX_L0_BASE_OFFSET                   (0x400)
#define HDMI_TX_L1_BASE_OFFSET                   (0x600)
#define HDMI_TX_L2_BASE_OFFSET                   (0x800)
#define HDMI_TX_L3_BASE_OFFSET                   (0xA00)

/* Tx Channel PHY registers */
#define QSERDES_TX_L0_BIST_MODE_LANENO                    (0x000)
#define QSERDES_TX_L0_BIST_INVERT                         (0x004)
#define QSERDES_TX_L0_CLKBUF_ENABLE                       (0x008)
#define QSERDES_TX_L0_CMN_CONTROL_ONE                     (0x00C)
#define QSERDES_TX_L0_CMN_CONTROL_TWO                     (0x010)
#define QSERDES_TX_L0_CMN_CONTROL_THREE                   (0x014)
#define QSERDES_TX_L0_TX_EMP_POST1_LVL                    (0x018)
#define QSERDES_TX_L0_TX_POST2_EMPH                       (0x01C)
#define QSERDES_TX_L0_TX_BOOST_LVL_UP_DN                  (0x020)
#define QSERDES_TX_L0_HP_PD_ENABLES                       (0x024)
#define QSERDES_TX_L0_TX_IDLE_LVL_LARGE_AMP               (0x028)
#define QSERDES_TX_L0_TX_DRV_LVL                          (0x02C)
#define QSERDES_TX_L0_TX_DRV_LVL_OFFSET                   (0x030)
#define QSERDES_TX_L0_RESET_TSYNC_EN                      (0x034)
#define QSERDES_TX_L0_PRE_STALL_LDO_BOOST_EN              (0x038)
#define QSERDES_TX_L0_TX_BAND                             (0x03C)
#define QSERDES_TX_L0_SLEW_CNTL                           (0x040)
#define QSERDES_TX_L0_INTERFACE_SELECT                    (0x044)
#define QSERDES_TX_L0_LPB_EN                              (0x048)
#define QSERDES_TX_L0_RES_CODE_LANE_TX                    (0x04C)
#define QSERDES_TX_L0_RES_CODE_LANE_RX                    (0x050)
#define QSERDES_TX_L0_RES_CODE_LANE_OFFSET                (0x054)
#define QSERDES_TX_L0_PERL_LENGTH1                        (0x058)
#define QSERDES_TX_L0_PERL_LENGTH2                        (0x05C)
#define QSERDES_TX_L0_SERDES_BYP_EN_OUT                   (0x060)
#define QSERDES_TX_L0_DEBUG_BUS_SEL                       (0x064)
#define QSERDES_TX_L0_HIGHZ_TRANSCEIVEREN_BIAS_DRVR_EN    (0x068)
#define QSERDES_TX_L0_TX_POL_INV                          (0x06C)
#define QSERDES_TX_L0_PARRATE_REC_DETECT_IDLE_EN          (0x070)
#define QSERDES_TX_L0_BIST_PATTERN1                       (0x074)
#define QSERDES_TX_L0_BIST_PATTERN2                       (0x078)
#define QSERDES_TX_L0_BIST_PATTERN3                       (0x07C)
#define QSERDES_TX_L0_BIST_PATTERN4                       (0x080)
#define QSERDES_TX_L0_BIST_PATTERN5                       (0x084)
#define QSERDES_TX_L0_BIST_PATTERN6                       (0x088)
#define QSERDES_TX_L0_BIST_PATTERN7                       (0x08C)
#define QSERDES_TX_L0_BIST_PATTERN8                       (0x090)
#define QSERDES_TX_L0_LANE_MODE                           (0x094)
#define QSERDES_TX_L0_IDAC_CAL_LANE_MODE                  (0x098)
#define QSERDES_TX_L0_IDAC_CAL_LANE_MODE_CONFIGURATION    (0x09C)
#define QSERDES_TX_L0_ATB_SEL1                            (0x0A0)
#define QSERDES_TX_L0_ATB_SEL2                            (0x0A4)
#define QSERDES_TX_L0_RCV_DETECT_LVL                      (0x0A8)
#define QSERDES_TX_L0_RCV_DETECT_LVL_2                    (0x0AC)
#define QSERDES_TX_L0_PRBS_SEED1                          (0x0B0)
#define QSERDES_TX_L0_PRBS_SEED2                          (0x0B4)
#define QSERDES_TX_L0_PRBS_SEED3                          (0x0B8)
#define QSERDES_TX_L0_PRBS_SEED4                          (0x0BC)
#define QSERDES_TX_L0_RESET_GEN                           (0x0C0)
#define QSERDES_TX_L0_RESET_GEN_MUXES                     (0x0C4)
#define QSERDES_TX_L0_TRAN_DRVR_EMP_EN                    (0x0C8)
#define QSERDES_TX_L0_TX_INTERFACE_MODE                   (0x0CC)
#define QSERDES_TX_L0_PWM_CTRL                            (0x0D0)
#define QSERDES_TX_L0_PWM_ENCODED_OR_DATA                 (0x0D4)
#define QSERDES_TX_L0_PWM_GEAR_1_DIVIDER_BAND2            (0x0D8)
#define QSERDES_TX_L0_PWM_GEAR_2_DIVIDER_BAND2            (0x0DC)
#define QSERDES_TX_L0_PWM_GEAR_3_DIVIDER_BAND2            (0x0E0)
#define QSERDES_TX_L0_PWM_GEAR_4_DIVIDER_BAND2            (0x0E4)
#define QSERDES_TX_L0_PWM_GEAR_1_DIVIDER_BAND0_1          (0x0E8)
#define QSERDES_TX_L0_PWM_GEAR_2_DIVIDER_BAND0_1          (0x0EC)
#define QSERDES_TX_L0_PWM_GEAR_3_DIVIDER_BAND0_1          (0x0F0)
#define QSERDES_TX_L0_PWM_GEAR_4_DIVIDER_BAND0_1          (0x0F4)
#define QSERDES_TX_L0_VMODE_CTRL1                         (0x0F8)
#define QSERDES_TX_L0_VMODE_CTRL2                         (0x0FC)
#define QSERDES_TX_L0_TX_ALOG_INTF_OBSV_CNTL              (0x100)
#define QSERDES_TX_L0_BIST_STATUS                         (0x104)
#define QSERDES_TX_L0_BIST_ERROR_COUNT1                   (0x108)
#define QSERDES_TX_L0_BIST_ERROR_COUNT2                   (0x10C)
#define QSERDES_TX_L0_TX_ALOG_INTF_OBSV                   (0x110)

/* HDMI PHY REGISTERS */
#define HDMI_PHY_BASE_OFFSET                  (0xC00)

#define HDMI_PHY_CFG                          (0x00)
#define HDMI_PHY_PD_CTL                       (0x04)
#define HDMI_PHY_MODE                         (0x08)
#define HDMI_PHY_MISR_CLEAR                   (0x0C)
#define HDMI_PHY_TX0_TX1_BIST_CFG0            (0x10)
#define HDMI_PHY_TX0_TX1_BIST_CFG1            (0x14)
#define HDMI_PHY_TX0_TX1_PRBS_SEED_BYTE0      (0x18)
#define HDMI_PHY_TX0_TX1_PRBS_SEED_BYTE1      (0x1C)
#define HDMI_PHY_TX0_TX1_BIST_PATTERN0        (0x20)
#define HDMI_PHY_TX0_TX1_BIST_PATTERN1        (0x24)
#define HDMI_PHY_TX2_TX3_BIST_CFG0            (0x28)
#define HDMI_PHY_TX2_TX3_BIST_CFG1            (0x2C)
#define HDMI_PHY_TX2_TX3_PRBS_SEED_BYTE0      (0x30)
#define HDMI_PHY_TX2_TX3_PRBS_SEED_BYTE1      (0x34)
#define HDMI_PHY_TX2_TX3_BIST_PATTERN0        (0x38)
#define HDMI_PHY_TX2_TX3_BIST_PATTERN1        (0x3C)
#define HDMI_PHY_DEBUG_BUS_SEL                (0x40)
#define HDMI_PHY_TXCAL_CFG0                   (0x44)
#define HDMI_PHY_TXCAL_CFG1                   (0x48)
#define HDMI_PHY_TX0_TX1_LANE_CTL             (0x4C)
#define HDMI_PHY_TX2_TX3_LANE_CTL             (0x50)
#define HDMI_PHY_LANE_BIST_CONFIG             (0x54)
#define HDMI_PHY_CLOCK                        (0x58)
#define HDMI_PHY_MISC1                        (0x5C)
#define HDMI_PHY_MISC2                        (0x60)
#define HDMI_PHY_TX0_TX1_BIST_STATUS0         (0x64)
#define HDMI_PHY_TX0_TX1_BIST_STATUS1         (0x68)
#define HDMI_PHY_TX0_TX1_BIST_STATUS2         (0x6C)
#define HDMI_PHY_TX2_TX3_BIST_STATUS0         (0x70)
#define HDMI_PHY_TX2_TX3_BIST_STATUS1         (0x74)
#define HDMI_PHY_TX2_TX3_BIST_STATUS2         (0x78)
#define HDMI_PHY_PRE_MISR_STATUS0             (0x7C)
#define HDMI_PHY_PRE_MISR_STATUS1             (0x80)
#define HDMI_PHY_PRE_MISR_STATUS2             (0x84)
#define HDMI_PHY_PRE_MISR_STATUS3             (0x88)
#define HDMI_PHY_POST_MISR_STATUS0            (0x8C)
#define HDMI_PHY_POST_MISR_STATUS1            (0x90)
#define HDMI_PHY_POST_MISR_STATUS2            (0x94)
#define HDMI_PHY_POST_MISR_STATUS3            (0x98)
#define HDMI_PHY_STATUS                       (0x9C)
#define HDMI_PHY_MISC3_STATUS                 (0xA0)
#define HDMI_PHY_MISC4_STATUS                 (0xA4)
#define HDMI_PHY_DEBUG_BUS0                   (0xA8)
#define HDMI_PHY_DEBUG_BUS1                   (0xAC)
#define HDMI_PHY_DEBUG_BUS2                   (0xB0)
#define HDMI_PHY_DEBUG_BUS3                   (0xB4)
#define HDMI_PHY_PHY_REVISION_ID0             (0xB8)
#define HDMI_PHY_PHY_REVISION_ID1             (0xBC)
#define HDMI_PHY_PHY_REVISION_ID2             (0xC0)
#define HDMI_PHY_PHY_REVISION_ID3             (0xC4)

#define HDMI_PLL_POLL_MAX_READS                100
#define HDMI_PLL_POLL_TIMEOUT_US               1500

enum hdmi_pll_freqs {
	HDMI_PCLK_25200_KHZ,
	HDMI_PCLK_27027_KHZ,
	HDMI_PCLK_27000_KHZ,
	HDMI_PCLK_74250_KHZ,
	HDMI_PCLK_148500_KHZ,
	HDMI_PCLK_154000_KHZ,
	HDMI_PCLK_268500_KHZ,
	HDMI_PCLK_297000_KHZ,
	HDMI_PCLK_594000_KHZ,
	HDMI_PCLK_MAX
};

struct hdmi_8996_phy_pll_reg_cfg {
	u32 tx_l0_lane_mode;
	u32 tx_l2_lane_mode;
	u32 tx_l0_tx_band;
	u32 tx_l1_tx_band;
	u32 tx_l2_tx_band;
	u32 tx_l3_tx_band;
	u32 com_svs_mode_clk_sel;
	u32 com_hsclk_sel;
	u32 com_pll_cctrl_mode0;
	u32 com_pll_rctrl_mode0;
	u32 com_cp_ctrl_mode0;
	u32 com_dec_start_mode0;
	u32 com_div_frac_start1_mode0;
	u32 com_div_frac_start2_mode0;
	u32 com_div_frac_start3_mode0;
	u32 com_integloop_gain0_mode0;
	u32 com_integloop_gain1_mode0;
	u32 com_lock_cmp_en;
	u32 com_lock_cmp1_mode0;
	u32 com_lock_cmp2_mode0;
	u32 com_lock_cmp3_mode0;
	u32 com_core_clk_en;
	u32 com_coreclk_div;
	u32 com_restrim_ctrl;
	u32 com_vco_tune_ctrl;

	u32 tx_l0_tx_drv_lvl;
	u32 tx_l0_tx_emp_post1_lvl;
	u32 tx_l1_tx_drv_lvl;
	u32 tx_l1_tx_emp_post1_lvl;
	u32 tx_l2_tx_drv_lvl;
	u32 tx_l2_tx_emp_post1_lvl;
	u32 tx_l3_tx_drv_lvl;
	u32 tx_l3_tx_emp_post1_lvl;
	u32 tx_l0_vmode_ctrl1;
	u32 tx_l0_vmode_ctrl2;
	u32 tx_l1_vmode_ctrl1;
	u32 tx_l1_vmode_ctrl2;
	u32 tx_l2_vmode_ctrl1;
	u32 tx_l2_vmode_ctrl2;
	u32 tx_l3_vmode_ctrl1;
	u32 tx_l3_vmode_ctrl2;
	u32 tx_l0_res_code_lane_tx;
	u32 tx_l1_res_code_lane_tx;
	u32 tx_l2_res_code_lane_tx;
	u32 tx_l3_res_code_lane_tx;

	u32 phy_mode;
};

struct hdmi_8996_v3_post_divider {
	u64 vco_freq;
	u64 hsclk_divsel;
	u64 vco_ratio;
	u64 tx_band_sel;
	u64 half_rate_mode;
};

static inline struct hdmi_pll_vco_clk *to_hdmi_8996_vco_clk(struct clk *clk)
{
	return container_of(clk, struct hdmi_pll_vco_clk, c);
}

static inline u64 hdmi_8996_v1_get_post_div_lt_2g(u64 bclk)
{
	if (bclk >= HDMI_2400MHZ_BIT_CLK_HZ)
		return 2;
	else if (bclk >= HDMI_1700MHZ_BIT_CLK_HZ)
		return 3;
	else if (bclk >= HDMI_1200MHZ_BIT_CLK_HZ)
		return 4;
	else if (bclk >= HDMI_850MHZ_BIT_CLK_HZ)
		return 3;
	else if (bclk >= HDMI_600MHZ_BIT_CLK_HZ)
		return 4;
	else if (bclk >= HDMI_450MHZ_BIT_CLK_HZ)
		return 3;
	else if (bclk >= HDMI_300MHZ_BIT_CLK_HZ)
		return 4;

	return HDMI_64B_ERR_VAL;
}

static inline u64 hdmi_8996_v2_get_post_div_lt_2g(u64 bclk, u64 vco_range)
{
	u64 hdmi_8ghz = vco_range;
	u64 tmp_calc;

	hdmi_8ghz <<= 2;
	tmp_calc = hdmi_8ghz;
	do_div(tmp_calc, 6U);

	if (bclk >= vco_range)
		return 2;
	else if (bclk >= tmp_calc)
		return 3;
	else if (bclk >= vco_range >> 1)
		return 4;

	tmp_calc = hdmi_8ghz;
	do_div(tmp_calc, 12U);
	if (bclk >= tmp_calc)
		return 3;
	else if (bclk >= vco_range >> 2)
		return 4;

	tmp_calc = hdmi_8ghz;
	do_div(tmp_calc, 24U);
	if (bclk >= tmp_calc)
		return 3;
	else if (bclk >= vco_range >> 3)
		return 4;

	return HDMI_64B_ERR_VAL;
}

static inline u64 hdmi_8996_v2_get_post_div_gt_2g(u64 hsclk)
{
	if (hsclk >= 0 && hsclk <= 3)
		return hsclk + 1;

	return HDMI_64B_ERR_VAL;
}

static inline u64 hdmi_8996_get_coreclk_div_lt_2g(u64 bclk)
{
	if (bclk >= HDMI_1334MHZ_BIT_CLK_HZ)
		return 1;
	else if (bclk >= HDMI_1000MHZ_BIT_CLK_HZ)
		return 1;
	else if (bclk >= HDMI_667MHZ_BIT_CLK_HZ)
		return 2;
	else if (bclk >= HDMI_500MHZ_BIT_CLK_HZ)
		return 2;
	else if (bclk >= HDMI_334MHZ_BIT_CLK_HZ)
		return 3;
	else if (bclk >= HDMI_250MHZ_BIT_CLK_HZ)
		return 3;

	return HDMI_64B_ERR_VAL;
}

static inline u64 hdmi_8996_get_coreclk_div_ratio(u64 clks_pll_divsel,
						  u64 coreclk_div)
{
	if (clks_pll_divsel == 0)
		return coreclk_div*2;
	else if (clks_pll_divsel == 1)
		return coreclk_div*4;

	return HDMI_64B_ERR_VAL;
}

static inline u64 hdmi_8996_v1_get_tx_band(u64 bclk)
{
	if (bclk >= 2400000000UL)
		return 0;
	if (bclk >= 1200000000UL)
		return 1;
	if (bclk >= 600000000UL)
		return 2;
	if (bclk >= 300000000UL)
		return 3;

	return HDMI_64B_ERR_VAL;
}

static inline u64 hdmi_8996_v2_get_tx_band(u64 bclk, u64 vco_range)
{
	if (bclk >= vco_range)
		return 0;
	else if (bclk >= vco_range >> 1)
		return 1;
	else if (bclk >= vco_range >> 2)
		return 2;
	else if (bclk >= vco_range >> 3)
		return 3;

	return HDMI_64B_ERR_VAL;
}

static inline u64 hdmi_8996_v1_get_hsclk(u64 fdata)
{
	if (fdata >= 9600000000UL)
		return 0;
	else if (fdata >= 4800000000UL)
		return 1;
	else if (fdata >= 3200000000UL)
		return 2;
	else if (fdata >= 2400000000UL)
		return 3;

	return HDMI_64B_ERR_VAL;
}

static inline u64 hdmi_8996_v2_get_hsclk(u64 fdata, u64 vco_range)
{
	u64 tmp_calc = vco_range;

	tmp_calc <<= 2;
	do_div(tmp_calc, 3U);
	if (fdata >= (vco_range << 2))
		return 0;
	else if (fdata >= (vco_range << 1))
		return 1;
	else if (fdata >= tmp_calc)
		return 2;
	else if (fdata >= vco_range)
		return 3;

	return HDMI_64B_ERR_VAL;

}

static inline u64 hdmi_8996_v2_get_vco_freq(u64 bclk, u64 vco_range)
{
	u64 tx_band_div_ratio = 1U << hdmi_8996_v2_get_tx_band(bclk, vco_range);
	u64 pll_post_div_ratio;

	if (bclk >= vco_range) {
		u64 hsclk = hdmi_8996_v2_get_hsclk(bclk, vco_range);

		pll_post_div_ratio = hdmi_8996_v2_get_post_div_gt_2g(hsclk);
	} else {
		pll_post_div_ratio = hdmi_8996_v2_get_post_div_lt_2g(bclk,
								vco_range);
	}

	return bclk * (pll_post_div_ratio * tx_band_div_ratio);
}

static inline u64 hdmi_8996_v2_get_fdata(u64 bclk, u64 vco_range)
{
	if (bclk >= vco_range)
		return bclk;

	u64 tmp_calc = hdmi_8996_v2_get_vco_freq(bclk, vco_range);
	u64 pll_post_div_ratio_lt_2g = hdmi_8996_v2_get_post_div_lt_2g(
						bclk, vco_range);
	if (pll_post_div_ratio_lt_2g == HDMI_64B_ERR_VAL)
		return HDMI_64B_ERR_VAL;

	do_div(tmp_calc, pll_post_div_ratio_lt_2g);
	return tmp_calc;
}

static inline u64 hdmi_8996_get_cpctrl(u64 frac_start, bool gen_ssc)
{
	if ((frac_start != 0) ||
	    (gen_ssc == true))
		/*
		 * This should be ROUND(11/(19.2/20))).
		 * Since ref clock does not change, hardcoding to 11
		 */
		return 0xB;

	return 0x23;
}

static inline u64 hdmi_8996_get_rctrl(u64 frac_start, bool gen_ssc)
{
	if ((frac_start != 0) || (gen_ssc == true))
		return 0x16;

	return 0x10;
}

static inline u64 hdmi_8996_get_cctrl(u64 frac_start, bool gen_ssc)
{
	if ((frac_start != 0) || (gen_ssc == true))
		return 0x28;

	return 0x1;
}

static inline u64 hdmi_8996_get_integloop_gain(u64 frac_start, bool gen_ssc)
{
	if ((frac_start != 0) || (gen_ssc == true))
		return 0x80;

	return 0xC4;
}

static inline u64 hdmi_8996_v3_get_integloop_gain(u64 frac_start, u64 bclk,
							bool gen_ssc)
{
	u64 digclk_divsel = bclk >= HDMI_DIG_FREQ_BIT_CLK_THRESHOLD ? 1 : 2;
	u64 base = ((frac_start != 0) || (gen_ssc == true)) ? 0x40 : 0xC4;

	base <<= digclk_divsel;

	return (base <= 2046 ? base : 0x7FE);
}

static inline u64 hdmi_8996_get_vco_tune(u64 fdata, u64 div)
{
	u64 vco_tune;

	vco_tune = fdata * div;
	do_div(vco_tune, 1000000);
	vco_tune = 13000 - vco_tune - 256;
	do_div(vco_tune, 5);

	return vco_tune;
}

static inline u64 hdmi_8996_get_pll_cmp(u64 pll_cmp_cnt, u64 core_clk)
{
	u64 pll_cmp;
	u64 rem;

	pll_cmp = pll_cmp_cnt * core_clk;
	rem = do_div(pll_cmp, HDMI_REF_CLOCK);
	if (rem > (HDMI_REF_CLOCK >> 1))
		pll_cmp++;
	pll_cmp -= 1;

	return pll_cmp;
}

static inline u64 hdmi_8996_v3_get_pll_cmp(u64 pll_cmp_cnt, u64 fdata)
{
	u64 dividend = pll_cmp_cnt * fdata;
	u64 divisor = HDMI_REF_CLOCK * 10;
	u64 rem;

	rem = do_div(dividend, divisor);
	if (rem > (divisor >> 1))
		dividend++;

	return dividend - 1;
}

static int hdmi_8996_v3_get_post_div(struct hdmi_8996_v3_post_divider *pd,
						u64 bclk)
{
	u32 ratio[] = {2, 3, 4, 5, 6, 9, 10, 12, 14, 15, 20, 21, 25, 28, 35};
	u32 tx_band_sel[] = {0, 1, 2, 3};
	u64 vco_freq[60];
	u64 vco, vco_optimal, half_rate_mode = 0;
	int vco_optimal_index, vco_freq_index;
	int i, j, k, x;

	for (i = 0; i <= 1; i++) {
		vco_optimal = HDMI_VCO_MAX_FREQ;
		vco_optimal_index = -1;
		vco_freq_index = 0;
		for (j = 0; j < 15; j++) {
			for (k = 0; k < 4; k++) {
				u64 ratio_mult = ratio[j] << tx_band_sel[k];

				vco = bclk >> half_rate_mode;
				vco *= ratio_mult;
				vco_freq[vco_freq_index++] = vco;
			}
		}

		for (x = 0; x < 60; x++) {
			u64 vco_tmp = vco_freq[x];

			if ((vco_tmp >= HDMI_VCO_MIN_FREQ) &&
					(vco_tmp <= vco_optimal)) {
				vco_optimal = vco_tmp;
				vco_optimal_index = x;
			}
		}

		if (vco_optimal_index == -1) {
			if (!half_rate_mode)
				half_rate_mode++;
			else
				return -EINVAL;
		} else {
			pd->vco_freq = vco_optimal;
			pd->tx_band_sel = tx_band_sel[vco_optimal_index % 4];
			pd->vco_ratio = ratio[vco_optimal_index / 4];
			break;
		}
	}

	switch (pd->vco_ratio) {
	case 2:
		pd->hsclk_divsel = 0;
		break;
	case 3:
		pd->hsclk_divsel = 4;
		break;
	case 4:
		pd->hsclk_divsel = 8;
		break;
	case 5:
		pd->hsclk_divsel = 12;
		break;
	case 6:
		pd->hsclk_divsel = 1;
		break;
	case 9:
		pd->hsclk_divsel = 5;
		break;
	case 10:
		pd->hsclk_divsel = 2;
		break;
	case 12:
		pd->hsclk_divsel = 9;
		break;
	case 14:
		pd->hsclk_divsel = 3;
		break;
	case 15:
		pd->hsclk_divsel = 13;
		break;
	case 20:
		pd->hsclk_divsel = 10;
		break;
	case 21:
		pd->hsclk_divsel = 7;
		break;
	case 25:
		pd->hsclk_divsel = 14;
		break;
	case 28:
		pd->hsclk_divsel = 11;
		break;
	case 35:
		pd->hsclk_divsel = 15;
		break;
	};

	return 0;
}

static int hdmi_8996_v1_calculate(u32 pix_clk,
			       struct hdmi_8996_phy_pll_reg_cfg *cfg)
{
	int rc = -EINVAL;
	u64 fdata, clk_divtx, tmds_clk;
	u64 bclk;
	u64 post_div_gt_2g;
	u64 post_div_lt_2g;
	u64 coreclk_div1_lt_2g;
	u64 core_clk_div_ratio;
	u64 core_clk;
	u64 pll_cmp;
	u64 tx_band;
	u64 tx_band_div_ratio;
	u64 hsclk;
	u64 dec_start;
	u64 frac_start;
	u64 pll_divisor = 4 * HDMI_REF_CLOCK;
	u64 cpctrl;
	u64 rctrl;
	u64 cctrl;
	u64 integloop_gain;
	u64 vco_tune;
	u64 vco_freq;
	u64 rem;

	/* FDATA, CLK_DIVTX, PIXEL_CLK, TMDS_CLK */
	bclk = ((u64)pix_clk) * HDMI_BIT_CLK_TO_PIX_CLK_RATIO;

	if (bclk > HDMI_HIGH_FREQ_BIT_CLK_THRESHOLD)
		tmds_clk = bclk/4;
	else
		tmds_clk = bclk;

	post_div_lt_2g = hdmi_8996_v1_get_post_div_lt_2g(bclk);
	if (post_div_lt_2g == HDMI_64B_ERR_VAL)
		goto fail;

	coreclk_div1_lt_2g = hdmi_8996_get_coreclk_div_lt_2g(bclk);

	core_clk_div_ratio = hdmi_8996_get_coreclk_div_ratio(
				HDMI_CLKS_PLL_DIVSEL, HDMI_CORECLK_DIV);

	tx_band = hdmi_8996_v1_get_tx_band(bclk);
	if (tx_band == HDMI_64B_ERR_VAL)
		goto fail;

	tx_band_div_ratio = 1 << tx_band;

	if (bclk >= HDMI_2400MHZ_BIT_CLK_HZ) {
		fdata = bclk;
		hsclk = hdmi_8996_v1_get_hsclk(fdata);
		if (hsclk == HDMI_64B_ERR_VAL)
			goto fail;

		post_div_gt_2g = (hsclk <= 3) ? (hsclk + 1) : HDMI_64B_ERR_VAL;
		if (post_div_gt_2g == HDMI_64B_ERR_VAL)
			goto fail;

		vco_freq = bclk * (post_div_gt_2g * tx_band_div_ratio);
		clk_divtx = vco_freq;
		do_div(clk_divtx, post_div_gt_2g);
	} else {
		vco_freq = bclk * (post_div_lt_2g * tx_band_div_ratio);
		fdata = vco_freq;
		do_div(fdata, post_div_lt_2g);
		hsclk = hdmi_8996_v1_get_hsclk(fdata);
		if (hsclk == HDMI_64B_ERR_VAL)
			goto fail;

		clk_divtx = vco_freq;
		do_div(clk_divtx, post_div_lt_2g);
		post_div_gt_2g = (hsclk <= 3) ? (hsclk + 1) : HDMI_64B_ERR_VAL;
		if (post_div_gt_2g == HDMI_64B_ERR_VAL)
			goto fail;
	}

	/* Decimal and fraction values */
	dec_start = fdata * post_div_gt_2g;
	do_div(dec_start, pll_divisor);
	frac_start = ((pll_divisor - (((dec_start + 1) * pll_divisor) -
			(fdata * post_div_gt_2g))) * (1 << 20));
	rem = do_div(frac_start, pll_divisor);
	/* Round off frac_start to closest integer */
	if (rem >= (pll_divisor >> 1))
		frac_start++;

	cpctrl = hdmi_8996_get_cpctrl(frac_start, false);
	rctrl = hdmi_8996_get_rctrl(frac_start, false);
	cctrl = hdmi_8996_get_cctrl(frac_start, false);
	integloop_gain = hdmi_8996_get_integloop_gain(frac_start, false);
	vco_tune = hdmi_8996_get_vco_tune(fdata, post_div_gt_2g);

	core_clk = clk_divtx;
	do_div(core_clk, core_clk_div_ratio);
	pll_cmp = hdmi_8996_get_pll_cmp(1024, core_clk);

	/* Debug dump */
	DEV_DBG("%s: VCO freq: %llu\n", __func__, vco_freq);
	DEV_DBG("%s: fdata: %llu\n", __func__, fdata);
	DEV_DBG("%s: CLK_DIVTX: %llu\n", __func__, clk_divtx);
	DEV_DBG("%s: pix_clk: %d\n", __func__, pix_clk);
	DEV_DBG("%s: tmds clk: %llu\n", __func__, tmds_clk);
	DEV_DBG("%s: HSCLK_SEL: %llu\n", __func__, hsclk);
	DEV_DBG("%s: DEC_START: %llu\n", __func__, dec_start);
	DEV_DBG("%s: DIV_FRAC_START: %llu\n", __func__, frac_start);
	DEV_DBG("%s: PLL_CPCTRL: %llu\n", __func__, cpctrl);
	DEV_DBG("%s: PLL_RCTRL: %llu\n", __func__, rctrl);
	DEV_DBG("%s: PLL_CCTRL: %llu\n", __func__, cctrl);
	DEV_DBG("%s: INTEGLOOP_GAIN: %llu\n", __func__, integloop_gain);
	DEV_DBG("%s: VCO_TUNE: %llu\n", __func__, vco_tune);
	DEV_DBG("%s: TX_BAND: %llu\n", __func__, tx_band);
	DEV_DBG("%s: PLL_CMP: %llu\n", __func__, pll_cmp);

	/* Convert these values to register specific values */
	cfg->tx_l0_lane_mode = 0x3;
	cfg->tx_l2_lane_mode = 0x3;
	cfg->tx_l0_tx_band = tx_band + 4;
	cfg->tx_l1_tx_band = tx_band + 4;
	cfg->tx_l2_tx_band = tx_band + 4;
	cfg->tx_l3_tx_band = tx_band + 4;
	cfg->tx_l0_res_code_lane_tx = 0x33;
	cfg->tx_l1_res_code_lane_tx = 0x33;
	cfg->tx_l2_res_code_lane_tx = 0x33;
	cfg->tx_l3_res_code_lane_tx = 0x33;
	cfg->com_restrim_ctrl = 0x0;
	cfg->com_vco_tune_ctrl = 0x1C;

	cfg->com_svs_mode_clk_sel =
			(bclk >= HDMI_DIG_FREQ_BIT_CLK_THRESHOLD ? 1 : 2);
	cfg->com_hsclk_sel = (0x28 | hsclk);
	cfg->com_pll_cctrl_mode0 = cctrl;
	cfg->com_pll_rctrl_mode0 = rctrl;
	cfg->com_cp_ctrl_mode0 = cpctrl;
	cfg->com_dec_start_mode0 = dec_start;
	cfg->com_div_frac_start1_mode0 = (frac_start & 0xFF);
	cfg->com_div_frac_start2_mode0 = ((frac_start & 0xFF00) >> 8);
	cfg->com_div_frac_start3_mode0 = ((frac_start & 0xF0000) >> 16);
	cfg->com_integloop_gain0_mode0 = (integloop_gain & 0xFF);
	cfg->com_integloop_gain1_mode0 = ((integloop_gain & 0xF00) >> 8);
	cfg->com_lock_cmp1_mode0 = (pll_cmp & 0xFF);
	cfg->com_lock_cmp2_mode0 = ((pll_cmp & 0xFF00) >> 8);
	cfg->com_lock_cmp3_mode0 = ((pll_cmp & 0x30000) >> 16);
	cfg->com_core_clk_en = (0x6C | (HDMI_CLKS_PLL_DIVSEL << 4));
	cfg->com_coreclk_div = HDMI_CORECLK_DIV;

	if (bclk > HDMI_HIGH_FREQ_BIT_CLK_THRESHOLD) {
		cfg->tx_l0_tx_drv_lvl = 0x25;
		cfg->tx_l0_tx_emp_post1_lvl = 0x23;
		cfg->tx_l1_tx_drv_lvl = 0x25;
		cfg->tx_l1_tx_emp_post1_lvl = 0x23;
		cfg->tx_l2_tx_drv_lvl = 0x25;
		cfg->tx_l2_tx_emp_post1_lvl = 0x23;
		cfg->tx_l3_tx_drv_lvl = 0x22;
		cfg->tx_l3_tx_emp_post1_lvl = 0x27;
		cfg->tx_l0_vmode_ctrl1 = 0x00;
		cfg->tx_l0_vmode_ctrl2 = 0x0D;
		cfg->tx_l1_vmode_ctrl1 = 0x00;
		cfg->tx_l1_vmode_ctrl2 = 0x0D;
		cfg->tx_l2_vmode_ctrl1 = 0x00;
		cfg->tx_l2_vmode_ctrl2 = 0x0D;
		cfg->tx_l3_vmode_ctrl1 = 0x00;
		cfg->tx_l3_vmode_ctrl2 = 0x00;
		cfg->com_restrim_ctrl = 0x0;
	} else if (bclk > HDMI_MID_FREQ_BIT_CLK_THRESHOLD) {
		cfg->tx_l0_tx_drv_lvl = 0x25;
		cfg->tx_l0_tx_emp_post1_lvl = 0x23;
		cfg->tx_l1_tx_drv_lvl = 0x25;
		cfg->tx_l1_tx_emp_post1_lvl = 0x23;
		cfg->tx_l2_tx_drv_lvl = 0x25;
		cfg->tx_l2_tx_emp_post1_lvl = 0x23;
		cfg->tx_l3_tx_drv_lvl = 0x25;
		cfg->tx_l3_tx_emp_post1_lvl = 0x23;
		cfg->tx_l0_vmode_ctrl1 = 0x00;
		cfg->tx_l0_vmode_ctrl2 = 0x0D;
		cfg->tx_l1_vmode_ctrl1 = 0x00;
		cfg->tx_l1_vmode_ctrl2 = 0x0D;
		cfg->tx_l2_vmode_ctrl1 = 0x00;
		cfg->tx_l2_vmode_ctrl2 = 0x0D;
		cfg->tx_l3_vmode_ctrl1 = 0x00;
		cfg->tx_l3_vmode_ctrl2 = 0x00;
		cfg->com_restrim_ctrl = 0x0;
	} else {
		cfg->tx_l0_tx_drv_lvl = 0x20;
		cfg->tx_l0_tx_emp_post1_lvl = 0x20;
		cfg->tx_l1_tx_drv_lvl = 0x20;
		cfg->tx_l1_tx_emp_post1_lvl = 0x20;
		cfg->tx_l2_tx_drv_lvl = 0x20;
		cfg->tx_l2_tx_emp_post1_lvl = 0x20;
		cfg->tx_l3_tx_drv_lvl = 0x20;
		cfg->tx_l3_tx_emp_post1_lvl = 0x20;
		cfg->tx_l0_vmode_ctrl1 = 0x00;
		cfg->tx_l0_vmode_ctrl2 = 0x0E;
		cfg->tx_l1_vmode_ctrl1 = 0x00;
		cfg->tx_l1_vmode_ctrl2 = 0x0E;
		cfg->tx_l2_vmode_ctrl1 = 0x00;
		cfg->tx_l2_vmode_ctrl2 = 0x0E;
		cfg->tx_l3_vmode_ctrl1 = 0x00;
		cfg->tx_l3_vmode_ctrl2 = 0x0E;
		cfg->com_restrim_ctrl = 0xD8;
	}

	cfg->phy_mode = (bclk > HDMI_HIGH_FREQ_BIT_CLK_THRESHOLD) ? 0x10 : 0x0;
	DEV_DBG("HDMI 8996 PLL: PLL Settings\n");
	DEV_DBG("PLL PARAM: tx_l0_lane_mode = 0x%x\n", cfg->tx_l0_lane_mode);
	DEV_DBG("PLL PARAM: tx_l2_lane_mode = 0x%x\n", cfg->tx_l2_lane_mode);
	DEV_DBG("PLL PARAM: tx_l0_tx_band = 0x%x\n", cfg->tx_l0_tx_band);
	DEV_DBG("PLL PARAM: tx_l1_tx_band = 0x%x\n", cfg->tx_l1_tx_band);
	DEV_DBG("PLL PARAM: tx_l2_tx_band = 0x%x\n", cfg->tx_l2_tx_band);
	DEV_DBG("PLL PARAM: tx_l3_tx_band = 0x%x\n", cfg->tx_l3_tx_band);
	DEV_DBG("PLL PARAM: com_svs_mode_clk_sel = 0x%x\n",
						cfg->com_svs_mode_clk_sel);
	DEV_DBG("PLL PARAM: com_hsclk_sel = 0x%x\n", cfg->com_hsclk_sel);
	DEV_DBG("PLL PARAM: com_pll_cctrl_mode0 = 0x%x\n",
						cfg->com_pll_cctrl_mode0);
	DEV_DBG("PLL PARAM: com_pll_rctrl_mode0 = 0x%x\n",
						cfg->com_pll_rctrl_mode0);
	DEV_DBG("PLL PARAM: com_cp_ctrl_mode0 = 0x%x\n",
						cfg->com_cp_ctrl_mode0);
	DEV_DBG("PLL PARAM: com_dec_start_mode0 = 0x%x\n",
						cfg->com_dec_start_mode0);
	DEV_DBG("PLL PARAM: com_div_frac_start1_mode0 = 0x%x\n",
						cfg->com_div_frac_start1_mode0);
	DEV_DBG("PLL PARAM: com_div_frac_start2_mode0 = 0x%x\n",
						cfg->com_div_frac_start2_mode0);
	DEV_DBG("PLL PARAM: com_div_frac_start3_mode0 = 0x%x\n",
						cfg->com_div_frac_start3_mode0);
	DEV_DBG("PLL PARAM: com_integloop_gain0_mode0 = 0x%x\n",
						cfg->com_integloop_gain0_mode0);
	DEV_DBG("PLL PARAM: com_integloop_gain1_mode0 = 0x%x\n",
						cfg->com_integloop_gain1_mode0);
	DEV_DBG("PLL PARAM: com_lock_cmp1_mode0 = 0x%x\n",
						cfg->com_lock_cmp1_mode0);
	DEV_DBG("PLL PARAM: com_lock_cmp2_mode0 = 0x%x\n",
						cfg->com_lock_cmp2_mode0);
	DEV_DBG("PLL PARAM: com_lock_cmp3_mode0 = 0x%x\n",
						cfg->com_lock_cmp3_mode0);
	DEV_DBG("PLL PARAM: com_core_clk_en = 0x%x\n", cfg->com_core_clk_en);
	DEV_DBG("PLL PARAM: com_coreclk_div = 0x%x\n", cfg->com_coreclk_div);
	DEV_DBG("PLL PARAM: com_restrim_ctrl = 0x%x\n",	cfg->com_restrim_ctrl);

	DEV_DBG("PLL PARAM: l0_tx_drv_lvl = 0x%x\n", cfg->tx_l0_tx_drv_lvl);
	DEV_DBG("PLL PARAM: l0_tx_emp_post1_lvl = 0x%x\n",
						cfg->tx_l0_tx_emp_post1_lvl);
	DEV_DBG("PLL PARAM: l1_tx_drv_lvl = 0x%x\n", cfg->tx_l1_tx_drv_lvl);
	DEV_DBG("PLL PARAM: l1_tx_emp_post1_lvl = 0x%x\n",
						cfg->tx_l1_tx_emp_post1_lvl);
	DEV_DBG("PLL PARAM: l2_tx_drv_lvl = 0x%x\n", cfg->tx_l2_tx_drv_lvl);
	DEV_DBG("PLL PARAM: l2_tx_emp_post1_lvl = 0x%x\n",
						cfg->tx_l2_tx_emp_post1_lvl);
	DEV_DBG("PLL PARAM: l3_tx_drv_lvl = 0x%x\n", cfg->tx_l3_tx_drv_lvl);
	DEV_DBG("PLL PARAM: l3_tx_emp_post1_lvl = 0x%x\n",
						cfg->tx_l3_tx_emp_post1_lvl);

	DEV_DBG("PLL PARAM: l0_vmode_ctrl1 = 0x%x\n", cfg->tx_l0_vmode_ctrl1);
	DEV_DBG("PLL PARAM: l0_vmode_ctrl2 = 0x%x\n", cfg->tx_l0_vmode_ctrl2);
	DEV_DBG("PLL PARAM: l1_vmode_ctrl1 = 0x%x\n", cfg->tx_l1_vmode_ctrl1);
	DEV_DBG("PLL PARAM: l1_vmode_ctrl2 = 0x%x\n", cfg->tx_l1_vmode_ctrl2);
	DEV_DBG("PLL PARAM: l2_vmode_ctrl1 = 0x%x\n", cfg->tx_l2_vmode_ctrl1);
	DEV_DBG("PLL PARAM: l2_vmode_ctrl2 = 0x%x\n", cfg->tx_l2_vmode_ctrl2);
	DEV_DBG("PLL PARAM: l3_vmode_ctrl1 = 0x%x\n", cfg->tx_l3_vmode_ctrl1);
	DEV_DBG("PLL PARAM: l3_vmode_ctrl2 = 0x%x\n", cfg->tx_l3_vmode_ctrl2);
	DEV_DBG("PLL PARAM: tx_l0_res_code_lane_tx = 0x%x\n",
					cfg->tx_l0_res_code_lane_tx);
	DEV_DBG("PLL PARAM: tx_l1_res_code_lane_tx = 0x%x\n",
					cfg->tx_l1_res_code_lane_tx);
	DEV_DBG("PLL PARAM: tx_l2_res_code_lane_tx = 0x%x\n",
					cfg->tx_l2_res_code_lane_tx);
	DEV_DBG("PLL PARAM: tx_l3_res_code_lane_tx = 0x%x\n",
					cfg->tx_l3_res_code_lane_tx);

	DEV_DBG("PLL PARAM: phy_mode = 0x%x\n", cfg->phy_mode);
	rc = 0;
fail:
	return rc;
}

static int hdmi_8996_v2_calculate(u32 pix_clk,
			       struct hdmi_8996_phy_pll_reg_cfg *cfg)
{
	int rc = -EINVAL;
	u64 fdata, clk_divtx, tmds_clk;
	u64 bclk;
	u64 post_div;
	u64 core_clk_div;
	u64 core_clk_div_ratio;
	u64 core_clk;
	u64 pll_cmp;
	u64 tx_band;
	u64 tx_band_div_ratio;
	u64 hsclk;
	u64 dec_start;
	u64 frac_start;
	u64 pll_divisor = 4 * HDMI_REF_CLOCK;
	u64 cpctrl;
	u64 rctrl;
	u64 cctrl;
	u64 integloop_gain;
	u64 vco_tune;
	u64 vco_freq;
	u64 vco_range;
	u64 rem;

	/* FDATA, CLK_DIVTX, PIXEL_CLK, TMDS_CLK */
	bclk = ((u64)pix_clk) * HDMI_BIT_CLK_TO_PIX_CLK_RATIO;

	if (bclk > HDMI_HIGH_FREQ_BIT_CLK_THRESHOLD)
		tmds_clk = pix_clk >> 2;
	else
		tmds_clk = pix_clk;

	vco_range = bclk < HDMI_282MHZ_BIT_CLK_HZ ? HDMI_2000MHZ_BIT_CLK_HZ :
				HDMI_2250MHZ_BIT_CLK_HZ;

	fdata = hdmi_8996_v2_get_fdata(bclk, vco_range);
	if (fdata == HDMI_64B_ERR_VAL)
		goto fail;

	hsclk = hdmi_8996_v2_get_hsclk(fdata, vco_range);
	if (hsclk == HDMI_64B_ERR_VAL)
		goto fail;

	if (bclk >= vco_range)
		post_div = hdmi_8996_v2_get_post_div_gt_2g(hsclk);
	else
		post_div = hdmi_8996_v2_get_post_div_lt_2g(bclk, vco_range);

	if (post_div == HDMI_64B_ERR_VAL)
		goto fail;

	core_clk_div = 5;
	core_clk_div_ratio = core_clk_div * 2;

	tx_band = hdmi_8996_v2_get_tx_band(bclk, vco_range);
	if (tx_band == HDMI_64B_ERR_VAL)
		goto fail;

	tx_band_div_ratio = 1 << tx_band;

	vco_freq = hdmi_8996_v2_get_vco_freq(bclk, vco_range);
	clk_divtx = vco_freq;
	do_div(clk_divtx, post_div);

	/* Decimal and fraction values */
	dec_start = fdata * post_div;
	do_div(dec_start, pll_divisor);
	frac_start = ((pll_divisor - (((dec_start + 1) * pll_divisor) -
			(fdata * post_div))) * (1 << 20));
	rem = do_div(frac_start, pll_divisor);
	/* Round off frac_start to closest integer */
	if (rem >= (pll_divisor >> 1))
		frac_start++;

	cpctrl = hdmi_8996_get_cpctrl(frac_start, false);
	rctrl = hdmi_8996_get_rctrl(frac_start, false);
	cctrl = hdmi_8996_get_cctrl(frac_start, false);
	integloop_gain = hdmi_8996_get_integloop_gain(frac_start, false);
	vco_tune = hdmi_8996_get_vco_tune(fdata, post_div);

	core_clk = clk_divtx;
	do_div(core_clk, core_clk_div_ratio);
	pll_cmp = hdmi_8996_get_pll_cmp(1024, core_clk);

	/* Debug dump */
	DEV_DBG("%s: VCO freq: %llu\n", __func__, vco_freq);
	DEV_DBG("%s: fdata: %llu\n", __func__, fdata);
	DEV_DBG("%s: CLK_DIVTX: %llu\n", __func__, clk_divtx);
	DEV_DBG("%s: pix_clk: %d\n", __func__, pix_clk);
	DEV_DBG("%s: tmds clk: %llu\n", __func__, tmds_clk);
	DEV_DBG("%s: HSCLK_SEL: %llu\n", __func__, hsclk);
	DEV_DBG("%s: DEC_START: %llu\n", __func__, dec_start);
	DEV_DBG("%s: DIV_FRAC_START: %llu\n", __func__, frac_start);
	DEV_DBG("%s: PLL_CPCTRL: %llu\n", __func__, cpctrl);
	DEV_DBG("%s: PLL_RCTRL: %llu\n", __func__, rctrl);
	DEV_DBG("%s: PLL_CCTRL: %llu\n", __func__, cctrl);
	DEV_DBG("%s: INTEGLOOP_GAIN: %llu\n", __func__, integloop_gain);
	DEV_DBG("%s: VCO_TUNE: %llu\n", __func__, vco_tune);
	DEV_DBG("%s: TX_BAND: %llu\n", __func__, tx_band);
	DEV_DBG("%s: PLL_CMP: %llu\n", __func__, pll_cmp);

	/* Convert these values to register specific values */
	cfg->tx_l0_lane_mode = 0x3;
	cfg->tx_l2_lane_mode = 0x3;
	cfg->tx_l0_tx_band = tx_band + 4;
	cfg->tx_l1_tx_band = tx_band + 4;
	cfg->tx_l2_tx_band = tx_band + 4;
	cfg->tx_l3_tx_band = tx_band + 4;

	if (bclk > HDMI_DIG_FREQ_BIT_CLK_THRESHOLD)
		cfg->com_svs_mode_clk_sel = 1;
	else
		cfg->com_svs_mode_clk_sel = 2;

	cfg->com_hsclk_sel = (0x28 | hsclk);
	cfg->com_pll_cctrl_mode0 = cctrl;
	cfg->com_pll_rctrl_mode0 = rctrl;
	cfg->com_cp_ctrl_mode0 = cpctrl;
	cfg->com_dec_start_mode0 = dec_start;
	cfg->com_div_frac_start1_mode0 = (frac_start & 0xFF);
	cfg->com_div_frac_start2_mode0 = ((frac_start & 0xFF00) >> 8);
	cfg->com_div_frac_start3_mode0 = ((frac_start & 0xF0000) >> 16);
	cfg->com_integloop_gain0_mode0 = (integloop_gain & 0xFF);
	cfg->com_integloop_gain1_mode0 = ((integloop_gain & 0xF00) >> 8);
	cfg->com_lock_cmp1_mode0 = (pll_cmp & 0xFF);
	cfg->com_lock_cmp2_mode0 = ((pll_cmp & 0xFF00) >> 8);
	cfg->com_lock_cmp3_mode0 = ((pll_cmp & 0x30000) >> 16);
	cfg->com_core_clk_en = (0x6C | (HDMI_CLKS_PLL_DIVSEL << 4));
	cfg->com_coreclk_div = HDMI_CORECLK_DIV;
	cfg->com_vco_tune_ctrl = 0x0;

	if (bclk > HDMI_HIGH_FREQ_BIT_CLK_THRESHOLD) {
		cfg->tx_l0_tx_drv_lvl = 0x25;
		cfg->tx_l0_tx_emp_post1_lvl = 0x23;
		cfg->tx_l1_tx_drv_lvl = 0x25;
		cfg->tx_l1_tx_emp_post1_lvl = 0x23;
		cfg->tx_l2_tx_drv_lvl = 0x25;
		cfg->tx_l2_tx_emp_post1_lvl = 0x23;
		cfg->tx_l3_tx_drv_lvl = 0x22;
		cfg->tx_l3_tx_emp_post1_lvl = 0x27;
		cfg->tx_l0_vmode_ctrl1 = 0x00;
		cfg->tx_l0_vmode_ctrl2 = 0x0D;
		cfg->tx_l1_vmode_ctrl1 = 0x00;
		cfg->tx_l1_vmode_ctrl2 = 0x0D;
		cfg->tx_l2_vmode_ctrl1 = 0x00;
		cfg->tx_l2_vmode_ctrl2 = 0x0D;
		cfg->tx_l3_vmode_ctrl1 = 0x00;
		cfg->tx_l3_vmode_ctrl2 = 0x00;
		cfg->tx_l0_res_code_lane_tx = 0x3F;
		cfg->tx_l1_res_code_lane_tx = 0x3F;
		cfg->tx_l2_res_code_lane_tx = 0x3F;
		cfg->tx_l3_res_code_lane_tx = 0x3F;
		cfg->com_restrim_ctrl = 0x0;
	} else if (bclk > HDMI_MID_FREQ_BIT_CLK_THRESHOLD) {
		cfg->tx_l0_tx_drv_lvl = 0x25;
		cfg->tx_l0_tx_emp_post1_lvl = 0x23;
		cfg->tx_l1_tx_drv_lvl = 0x25;
		cfg->tx_l1_tx_emp_post1_lvl = 0x23;
		cfg->tx_l2_tx_drv_lvl = 0x25;
		cfg->tx_l2_tx_emp_post1_lvl = 0x23;
		cfg->tx_l3_tx_drv_lvl = 0x25;
		cfg->tx_l3_tx_emp_post1_lvl = 0x23;
		cfg->tx_l0_vmode_ctrl1 = 0x00;
		cfg->tx_l0_vmode_ctrl2 = 0x0D;
		cfg->tx_l1_vmode_ctrl1 = 0x00;
		cfg->tx_l1_vmode_ctrl2 = 0x0D;
		cfg->tx_l2_vmode_ctrl1 = 0x00;
		cfg->tx_l2_vmode_ctrl2 = 0x0D;
		cfg->tx_l3_vmode_ctrl1 = 0x00;
		cfg->tx_l3_vmode_ctrl2 = 0x00;
		cfg->tx_l0_res_code_lane_tx = 0x39;
		cfg->tx_l1_res_code_lane_tx = 0x39;
		cfg->tx_l2_res_code_lane_tx = 0x39;
		cfg->tx_l3_res_code_lane_tx = 0x39;
		cfg->com_restrim_ctrl = 0x0;
	} else {
		cfg->tx_l0_tx_drv_lvl = 0x20;
		cfg->tx_l0_tx_emp_post1_lvl = 0x20;
		cfg->tx_l1_tx_drv_lvl = 0x20;
		cfg->tx_l1_tx_emp_post1_lvl = 0x20;
		cfg->tx_l2_tx_drv_lvl = 0x20;
		cfg->tx_l2_tx_emp_post1_lvl = 0x20;
		cfg->tx_l3_tx_drv_lvl = 0x20;
		cfg->tx_l3_tx_emp_post1_lvl = 0x20;
		cfg->tx_l0_vmode_ctrl1 = 0x00;
		cfg->tx_l0_vmode_ctrl2 = 0x0E;
		cfg->tx_l1_vmode_ctrl1 = 0x00;
		cfg->tx_l1_vmode_ctrl2 = 0x0E;
		cfg->tx_l2_vmode_ctrl1 = 0x00;
		cfg->tx_l2_vmode_ctrl2 = 0x0E;
		cfg->tx_l3_vmode_ctrl1 = 0x00;
		cfg->tx_l3_vmode_ctrl2 = 0x0E;
		cfg->tx_l0_res_code_lane_tx = 0x3F;
		cfg->tx_l1_res_code_lane_tx = 0x3F;
		cfg->tx_l2_res_code_lane_tx = 0x3F;
		cfg->tx_l3_res_code_lane_tx = 0x3F;
		cfg->com_restrim_ctrl = 0xD8;
	}

	cfg->phy_mode = (bclk > HDMI_HIGH_FREQ_BIT_CLK_THRESHOLD) ? 0x10 : 0x0;
	DEV_DBG("HDMI 8996 PLL: PLL Settings\n");
	DEV_DBG("PLL PARAM: tx_l0_lane_mode = 0x%x\n", cfg->tx_l0_lane_mode);
	DEV_DBG("PLL PARAM: tx_l2_lane_mode = 0x%x\n", cfg->tx_l2_lane_mode);
	DEV_DBG("PLL PARAM: tx_l0_tx_band = 0x%x\n", cfg->tx_l0_tx_band);
	DEV_DBG("PLL PARAM: tx_l1_tx_band = 0x%x\n", cfg->tx_l1_tx_band);
	DEV_DBG("PLL PARAM: tx_l2_tx_band = 0x%x\n", cfg->tx_l2_tx_band);
	DEV_DBG("PLL PARAM: tx_l3_tx_band = 0x%x\n", cfg->tx_l3_tx_band);
	DEV_DBG("PLL PARAM: com_svs_mode_clk_sel = 0x%x\n",
						cfg->com_svs_mode_clk_sel);
	DEV_DBG("PLL PARAM: com_vco_tune_ctrl = 0x%x\n",
						cfg->com_vco_tune_ctrl);
	DEV_DBG("PLL PARAM: com_hsclk_sel = 0x%x\n", cfg->com_hsclk_sel);
	DEV_DBG("PLL PARAM: com_lock_cmp_en = 0x%x\n", cfg->com_lock_cmp_en);
	DEV_DBG("PLL PARAM: com_pll_cctrl_mode0 = 0x%x\n",
						cfg->com_pll_cctrl_mode0);
	DEV_DBG("PLL PARAM: com_pll_rctrl_mode0 = 0x%x\n",
						cfg->com_pll_rctrl_mode0);
	DEV_DBG("PLL PARAM: com_cp_ctrl_mode0 = 0x%x\n",
						cfg->com_cp_ctrl_mode0);
	DEV_DBG("PLL PARAM: com_dec_start_mode0 = 0x%x\n",
						cfg->com_dec_start_mode0);
	DEV_DBG("PLL PARAM: com_div_frac_start1_mode0 = 0x%x\n",
						cfg->com_div_frac_start1_mode0);
	DEV_DBG("PLL PARAM: com_div_frac_start2_mode0 = 0x%x\n",
						cfg->com_div_frac_start2_mode0);
	DEV_DBG("PLL PARAM: com_div_frac_start3_mode0 = 0x%x\n",
						cfg->com_div_frac_start3_mode0);
	DEV_DBG("PLL PARAM: com_integloop_gain0_mode0 = 0x%x\n",
						cfg->com_integloop_gain0_mode0);
	DEV_DBG("PLL PARAM: com_integloop_gain1_mode0 = 0x%x\n",
						cfg->com_integloop_gain1_mode0);
	DEV_DBG("PLL PARAM: com_lock_cmp1_mode0 = 0x%x\n",
						cfg->com_lock_cmp1_mode0);
	DEV_DBG("PLL PARAM: com_lock_cmp2_mode0 = 0x%x\n",
						cfg->com_lock_cmp2_mode0);
	DEV_DBG("PLL PARAM: com_lock_cmp3_mode0 = 0x%x\n",
						cfg->com_lock_cmp3_mode0);
	DEV_DBG("PLL PARAM: com_core_clk_en = 0x%x\n", cfg->com_core_clk_en);
	DEV_DBG("PLL PARAM: com_coreclk_div = 0x%x\n", cfg->com_coreclk_div);

	DEV_DBG("PLL PARAM: l0_tx_drv_lvl = 0x%x\n", cfg->tx_l0_tx_drv_lvl);
	DEV_DBG("PLL PARAM: l0_tx_emp_post1_lvl = 0x%x\n",
						cfg->tx_l0_tx_emp_post1_lvl);
	DEV_DBG("PLL PARAM: l1_tx_drv_lvl = 0x%x\n", cfg->tx_l1_tx_drv_lvl);
	DEV_DBG("PLL PARAM: l1_tx_emp_post1_lvl = 0x%x\n",
						cfg->tx_l1_tx_emp_post1_lvl);
	DEV_DBG("PLL PARAM: l2_tx_drv_lvl = 0x%x\n", cfg->tx_l2_tx_drv_lvl);
	DEV_DBG("PLL PARAM: l2_tx_emp_post1_lvl = 0x%x\n",
						cfg->tx_l2_tx_emp_post1_lvl);
	DEV_DBG("PLL PARAM: l3_tx_drv_lvl = 0x%x\n", cfg->tx_l3_tx_drv_lvl);
	DEV_DBG("PLL PARAM: l3_tx_emp_post1_lvl = 0x%x\n",
						cfg->tx_l3_tx_emp_post1_lvl);

	DEV_DBG("PLL PARAM: l0_vmode_ctrl1 = 0x%x\n", cfg->tx_l0_vmode_ctrl1);
	DEV_DBG("PLL PARAM: l0_vmode_ctrl2 = 0x%x\n", cfg->tx_l0_vmode_ctrl2);
	DEV_DBG("PLL PARAM: l1_vmode_ctrl1 = 0x%x\n", cfg->tx_l1_vmode_ctrl1);
	DEV_DBG("PLL PARAM: l1_vmode_ctrl2 = 0x%x\n", cfg->tx_l1_vmode_ctrl2);
	DEV_DBG("PLL PARAM: l2_vmode_ctrl1 = 0x%x\n", cfg->tx_l2_vmode_ctrl1);
	DEV_DBG("PLL PARAM: l2_vmode_ctrl2 = 0x%x\n", cfg->tx_l2_vmode_ctrl2);
	DEV_DBG("PLL PARAM: l3_vmode_ctrl1 = 0x%x\n", cfg->tx_l3_vmode_ctrl1);
	DEV_DBG("PLL PARAM: l3_vmode_ctrl2 = 0x%x\n", cfg->tx_l3_vmode_ctrl2);
	DEV_DBG("PLL PARAM: tx_l0_res_code_lane_tx = 0x%x\n",
					cfg->tx_l0_res_code_lane_tx);
	DEV_DBG("PLL PARAM: tx_l1_res_code_lane_tx = 0x%x\n",
					cfg->tx_l1_res_code_lane_tx);
	DEV_DBG("PLL PARAM: tx_l2_res_code_lane_tx = 0x%x\n",
					cfg->tx_l2_res_code_lane_tx);
	DEV_DBG("PLL PARAM: tx_l3_res_code_lane_tx = 0x%x\n",
					cfg->tx_l3_res_code_lane_tx);
	DEV_DBG("PLL PARAM: com_restrim_ctrl = 0x%x\n",	cfg->com_restrim_ctrl);

	DEV_DBG("PLL PARAM: phy_mode = 0x%x\n", cfg->phy_mode);
	rc = 0;
fail:
	return rc;
}

static int hdmi_8996_v3_calculate(u32 pix_clk,
			       struct hdmi_8996_phy_pll_reg_cfg *cfg)
{
	int rc = -EINVAL;
	struct hdmi_8996_v3_post_divider pd;
	u64 fdata, tmds_clk;
	u64 bclk;
	u64 pll_cmp;
	u64 tx_band;
	u64 hsclk;
	u64 dec_start;
	u64 frac_start;
	u64 pll_divisor = 4 * HDMI_REF_CLOCK;
	u64 cpctrl;
	u64 rctrl;
	u64 cctrl;
	u64 integloop_gain;
	u64 vco_freq;
	u64 rem;

	/* FDATA, HSCLK, PIXEL_CLK, TMDS_CLK */
	bclk = ((u64)pix_clk) * HDMI_BIT_CLK_TO_PIX_CLK_RATIO;

	if (bclk > HDMI_HIGH_FREQ_BIT_CLK_THRESHOLD)
		tmds_clk = pix_clk >> 2;
	else
		tmds_clk = pix_clk;

	if (hdmi_8996_v3_get_post_div(&pd, bclk) || pd.vco_ratio <= 0 ||
			pd.vco_freq <= 0)
		goto fail;

	vco_freq = pd.vco_freq;
	fdata = pd.vco_freq;
	do_div(fdata, pd.vco_ratio);

	hsclk = pd.hsclk_divsel;
	dec_start = vco_freq;
	do_div(dec_start, pll_divisor);

	frac_start = vco_freq * (1 << 20);
	rem = do_div(frac_start, pll_divisor);
	frac_start -= dec_start * (1 << 20);
	if (rem > (pll_divisor >> 1))
		frac_start++;

	cpctrl = hdmi_8996_get_cpctrl(frac_start, false);
	rctrl = hdmi_8996_get_rctrl(frac_start, false);
	cctrl = hdmi_8996_get_cctrl(frac_start, false);
	integloop_gain = hdmi_8996_v3_get_integloop_gain(frac_start, bclk,
									false);
	pll_cmp = hdmi_8996_v3_get_pll_cmp(1024, fdata);
	tx_band = pd.tx_band_sel;

	/* Debug dump */
	DEV_DBG("%s: VCO freq: %llu\n", __func__, vco_freq);
	DEV_DBG("%s: fdata: %llu\n", __func__, fdata);
	DEV_DBG("%s: pix_clk: %d\n", __func__, pix_clk);
	DEV_DBG("%s: tmds clk: %llu\n", __func__, tmds_clk);
	DEV_DBG("%s: HSCLK_SEL: %llu\n", __func__, hsclk);
	DEV_DBG("%s: DEC_START: %llu\n", __func__, dec_start);
	DEV_DBG("%s: DIV_FRAC_START: %llu\n", __func__, frac_start);
	DEV_DBG("%s: PLL_CPCTRL: %llu\n", __func__, cpctrl);
	DEV_DBG("%s: PLL_RCTRL: %llu\n", __func__, rctrl);
	DEV_DBG("%s: PLL_CCTRL: %llu\n", __func__, cctrl);
	DEV_DBG("%s: INTEGLOOP_GAIN: %llu\n", __func__, integloop_gain);
	DEV_DBG("%s: TX_BAND: %llu\n", __func__, tx_band);
	DEV_DBG("%s: PLL_CMP: %llu\n", __func__, pll_cmp);

	/* Convert these values to register specific values */
	cfg->tx_l0_tx_band = tx_band + 4;
	cfg->tx_l1_tx_band = tx_band + 4;
	cfg->tx_l2_tx_band = tx_band + 4;
	cfg->tx_l3_tx_band = tx_band + 4;

	if (bclk > HDMI_DIG_FREQ_BIT_CLK_THRESHOLD)
		cfg->com_svs_mode_clk_sel = 1;
	else
		cfg->com_svs_mode_clk_sel = 2;

	cfg->com_hsclk_sel = (0x20 | hsclk);
	cfg->com_pll_cctrl_mode0 = cctrl;
	cfg->com_pll_rctrl_mode0 = rctrl;
	cfg->com_cp_ctrl_mode0 = cpctrl;
	cfg->com_dec_start_mode0 = dec_start;
	cfg->com_div_frac_start1_mode0 = (frac_start & 0xFF);
	cfg->com_div_frac_start2_mode0 = ((frac_start & 0xFF00) >> 8);
	cfg->com_div_frac_start3_mode0 = ((frac_start & 0xF0000) >> 16);
	cfg->com_integloop_gain0_mode0 = (integloop_gain & 0xFF);
	cfg->com_integloop_gain1_mode0 = ((integloop_gain & 0xF00) >> 8);
	cfg->com_lock_cmp1_mode0 = (pll_cmp & 0xFF);
	cfg->com_lock_cmp2_mode0 = ((pll_cmp & 0xFF00) >> 8);
	cfg->com_lock_cmp3_mode0 = ((pll_cmp & 0x30000) >> 16);
	cfg->com_lock_cmp_en = 0x04;
	cfg->com_core_clk_en = 0x2C;
	cfg->com_coreclk_div = HDMI_CORECLK_DIV;
	cfg->phy_mode = (bclk > HDMI_HIGH_FREQ_BIT_CLK_THRESHOLD) ? 0x10 : 0x0;
	cfg->com_vco_tune_ctrl = 0x0;

	cfg->tx_l0_lane_mode = 0x43;
	cfg->tx_l2_lane_mode = 0x43;

	if (bclk > HDMI_HIGH_FREQ_BIT_CLK_THRESHOLD) {
		cfg->tx_l0_tx_drv_lvl = 0x25;
		cfg->tx_l0_tx_emp_post1_lvl = 0x23;
		cfg->tx_l1_tx_drv_lvl = 0x25;
		cfg->tx_l1_tx_emp_post1_lvl = 0x23;
		cfg->tx_l2_tx_drv_lvl = 0x25;
		cfg->tx_l2_tx_emp_post1_lvl = 0x23;
		cfg->tx_l3_tx_drv_lvl = 0x22;
		cfg->tx_l3_tx_emp_post1_lvl = 0x27;
		cfg->tx_l0_vmode_ctrl1 = 0x00;
		cfg->tx_l0_vmode_ctrl2 = 0x0D;
		cfg->tx_l1_vmode_ctrl1 = 0x00;
		cfg->tx_l1_vmode_ctrl2 = 0x0D;
		cfg->tx_l2_vmode_ctrl1 = 0x00;
		cfg->tx_l2_vmode_ctrl2 = 0x0D;
		cfg->tx_l3_vmode_ctrl1 = 0x00;
		cfg->tx_l3_vmode_ctrl2 = 0x00;
	} else if (bclk > HDMI_MID_FREQ_BIT_CLK_THRESHOLD) {
		cfg->tx_l0_tx_drv_lvl = 0x25;
		cfg->tx_l0_tx_emp_post1_lvl = 0x23;
		cfg->tx_l1_tx_drv_lvl = 0x25;
		cfg->tx_l1_tx_emp_post1_lvl = 0x23;
		cfg->tx_l2_tx_drv_lvl = 0x25;
		cfg->tx_l2_tx_emp_post1_lvl = 0x23;
		cfg->tx_l3_tx_drv_lvl = 0x25;
		cfg->tx_l3_tx_emp_post1_lvl = 0x23;
		cfg->tx_l0_vmode_ctrl1 = 0x00;
		cfg->tx_l0_vmode_ctrl2 = 0x0D;
		cfg->tx_l1_vmode_ctrl1 = 0x00;
		cfg->tx_l1_vmode_ctrl2 = 0x0D;
		cfg->tx_l2_vmode_ctrl1 = 0x00;
		cfg->tx_l2_vmode_ctrl2 = 0x0D;
		cfg->tx_l3_vmode_ctrl1 = 0x00;
		cfg->tx_l3_vmode_ctrl2 = 0x00;
	} else {
		cfg->tx_l0_tx_drv_lvl = 0x20;
		cfg->tx_l0_tx_emp_post1_lvl = 0x20;
		cfg->tx_l1_tx_drv_lvl = 0x20;
		cfg->tx_l1_tx_emp_post1_lvl = 0x20;
		cfg->tx_l2_tx_drv_lvl = 0x20;
		cfg->tx_l2_tx_emp_post1_lvl = 0x20;
		cfg->tx_l3_tx_drv_lvl = 0x20;
		cfg->tx_l3_tx_emp_post1_lvl = 0x20;
		cfg->tx_l0_vmode_ctrl1 = 0x00;
		cfg->tx_l0_vmode_ctrl2 = 0x0E;
		cfg->tx_l1_vmode_ctrl1 = 0x00;
		cfg->tx_l1_vmode_ctrl2 = 0x0E;
		cfg->tx_l2_vmode_ctrl1 = 0x00;
		cfg->tx_l2_vmode_ctrl2 = 0x0E;
		cfg->tx_l3_vmode_ctrl1 = 0x00;
		cfg->tx_l3_vmode_ctrl2 = 0x0E;
	}

	DEV_DBG("HDMI 8996 PLL: PLL Settings\n");
	DEV_DBG("PLL PARAM: tx_l0_tx_band = 0x%x\n", cfg->tx_l0_tx_band);
	DEV_DBG("PLL PARAM: tx_l1_tx_band = 0x%x\n", cfg->tx_l1_tx_band);
	DEV_DBG("PLL PARAM: tx_l2_tx_band = 0x%x\n", cfg->tx_l2_tx_band);
	DEV_DBG("PLL PARAM: tx_l3_tx_band = 0x%x\n", cfg->tx_l3_tx_band);
	DEV_DBG("PLL PARAM: com_svs_mode_clk_sel = 0x%x\n",
						cfg->com_svs_mode_clk_sel);
	DEV_DBG("PLL PARAM: com_hsclk_sel = 0x%x\n", cfg->com_hsclk_sel);
	DEV_DBG("PLL PARAM: com_lock_cmp_en = 0x%x\n", cfg->com_lock_cmp_en);
	DEV_DBG("PLL PARAM: com_pll_cctrl_mode0 = 0x%x\n",
						cfg->com_pll_cctrl_mode0);
	DEV_DBG("PLL PARAM: com_pll_rctrl_mode0 = 0x%x\n",
						cfg->com_pll_rctrl_mode0);
	DEV_DBG("PLL PARAM: com_cp_ctrl_mode0 = 0x%x\n",
						cfg->com_cp_ctrl_mode0);
	DEV_DBG("PLL PARAM: com_dec_start_mode0 = 0x%x\n",
						cfg->com_dec_start_mode0);
	DEV_DBG("PLL PARAM: com_div_frac_start1_mode0 = 0x%x\n",
						cfg->com_div_frac_start1_mode0);
	DEV_DBG("PLL PARAM: com_div_frac_start2_mode0 = 0x%x\n",
						cfg->com_div_frac_start2_mode0);
	DEV_DBG("PLL PARAM: com_div_frac_start3_mode0 = 0x%x\n",
						cfg->com_div_frac_start3_mode0);
	DEV_DBG("PLL PARAM: com_integloop_gain0_mode0 = 0x%x\n",
						cfg->com_integloop_gain0_mode0);
	DEV_DBG("PLL PARAM: com_integloop_gain1_mode0 = 0x%x\n",
						cfg->com_integloop_gain1_mode0);
	DEV_DBG("PLL PARAM: com_lock_cmp1_mode0 = 0x%x\n",
						cfg->com_lock_cmp1_mode0);
	DEV_DBG("PLL PARAM: com_lock_cmp2_mode0 = 0x%x\n",
						cfg->com_lock_cmp2_mode0);
	DEV_DBG("PLL PARAM: com_lock_cmp3_mode0 = 0x%x\n",
						cfg->com_lock_cmp3_mode0);
	DEV_DBG("PLL PARAM: com_core_clk_en = 0x%x\n", cfg->com_core_clk_en);
	DEV_DBG("PLL PARAM: com_coreclk_div = 0x%x\n", cfg->com_coreclk_div);
	DEV_DBG("PLL PARAM: phy_mode = 0x%x\n", cfg->phy_mode);

	DEV_DBG("PLL PARAM: tx_l0_lane_mode = 0x%x\n", cfg->tx_l0_lane_mode);
	DEV_DBG("PLL PARAM: tx_l2_lane_mode = 0x%x\n", cfg->tx_l2_lane_mode);
	DEV_DBG("PLL PARAM: l0_tx_drv_lvl = 0x%x\n", cfg->tx_l0_tx_drv_lvl);
	DEV_DBG("PLL PARAM: l0_tx_emp_post1_lvl = 0x%x\n",
						cfg->tx_l0_tx_emp_post1_lvl);
	DEV_DBG("PLL PARAM: l1_tx_drv_lvl = 0x%x\n", cfg->tx_l1_tx_drv_lvl);
	DEV_DBG("PLL PARAM: l1_tx_emp_post1_lvl = 0x%x\n",
						cfg->tx_l1_tx_emp_post1_lvl);
	DEV_DBG("PLL PARAM: l2_tx_drv_lvl = 0x%x\n", cfg->tx_l2_tx_drv_lvl);
	DEV_DBG("PLL PARAM: l2_tx_emp_post1_lvl = 0x%x\n",
						cfg->tx_l2_tx_emp_post1_lvl);
	DEV_DBG("PLL PARAM: l3_tx_drv_lvl = 0x%x\n", cfg->tx_l3_tx_drv_lvl);
	DEV_DBG("PLL PARAM: l3_tx_emp_post1_lvl = 0x%x\n",
						cfg->tx_l3_tx_emp_post1_lvl);

	DEV_DBG("PLL PARAM: l0_vmode_ctrl1 = 0x%x\n", cfg->tx_l0_vmode_ctrl1);
	DEV_DBG("PLL PARAM: l0_vmode_ctrl2 = 0x%x\n", cfg->tx_l0_vmode_ctrl2);
	DEV_DBG("PLL PARAM: l1_vmode_ctrl1 = 0x%x\n", cfg->tx_l1_vmode_ctrl1);
	DEV_DBG("PLL PARAM: l1_vmode_ctrl2 = 0x%x\n", cfg->tx_l1_vmode_ctrl2);
	DEV_DBG("PLL PARAM: l2_vmode_ctrl1 = 0x%x\n", cfg->tx_l2_vmode_ctrl1);
	DEV_DBG("PLL PARAM: l2_vmode_ctrl2 = 0x%x\n", cfg->tx_l2_vmode_ctrl2);
	DEV_DBG("PLL PARAM: l3_vmode_ctrl1 = 0x%x\n", cfg->tx_l3_vmode_ctrl1);
	DEV_DBG("PLL PARAM: l3_vmode_ctrl2 = 0x%x\n", cfg->tx_l3_vmode_ctrl2);
	rc = 0;
fail:
	return rc;
}

static int hdmi_8996_calculate(u32 pix_clk,
			       struct hdmi_8996_phy_pll_reg_cfg *cfg, u32 ver)
{
	switch (ver) {
	case HDMI_VERSION_8996_V3:
	case HDMI_VERSION_8996_V3_1_8:
		return hdmi_8996_v3_calculate(pix_clk, cfg);
	case HDMI_VERSION_8996_V2:
		return hdmi_8996_v2_calculate(pix_clk, cfg);
	default:
		return hdmi_8996_v1_calculate(pix_clk, cfg);
	}
}

static int hdmi_8996_phy_pll_set_clk_rate(struct clk *c, u32 tmds_clk, u32 ver)
{
	int rc = 0;
	struct hdmi_pll_vco_clk *vco = to_hdmi_8996_vco_clk(c);
	struct mdss_pll_resources *io = vco->priv;
	struct hdmi_8996_phy_pll_reg_cfg cfg = {0};

	rc = hdmi_8996_calculate(tmds_clk, &cfg, ver);
	if (rc) {
		DEV_ERR("%s: PLL calculation failed\n", __func__);
		return rc;
	}

	/* Initially shut down PHY */
	DEV_DBG("%s: Disabling PHY\n", __func__);
	MDSS_PLL_REG_W(io->phy_base, HDMI_PHY_PD_CTL, 0x0);
	udelay(500);

	/* Power up sequence */
	switch (ver) {
	case HDMI_VERSION_8996_V2:
	case HDMI_VERSION_8996_V3:
	case HDMI_VERSION_8996_V3_1_8:
		MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_BG_CTRL, 0x04);
		break;
	};

	MDSS_PLL_REG_W(io->phy_base, HDMI_PHY_PD_CTL, 0x1);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_RESETSM_CNTRL, 0x20);
	MDSS_PLL_REG_W(io->phy_base, HDMI_PHY_TX0_TX1_LANE_CTL, 0x0F);
	MDSS_PLL_REG_W(io->phy_base, HDMI_PHY_TX2_TX3_LANE_CTL, 0x0F);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L0_BASE_OFFSET,
				     QSERDES_TX_L0_CLKBUF_ENABLE, 0x03);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L1_BASE_OFFSET,
				     QSERDES_TX_L0_CLKBUF_ENABLE, 0x03);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L2_BASE_OFFSET,
				     QSERDES_TX_L0_CLKBUF_ENABLE, 0x03);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L3_BASE_OFFSET,
				     QSERDES_TX_L0_CLKBUF_ENABLE, 0x03);

	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L0_BASE_OFFSET,
		     QSERDES_TX_L0_LANE_MODE, cfg.tx_l0_lane_mode);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L2_BASE_OFFSET,
		     QSERDES_TX_L0_LANE_MODE, cfg.tx_l2_lane_mode);

	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L0_BASE_OFFSET,
		     QSERDES_TX_L0_TX_BAND, cfg.tx_l0_tx_band);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L1_BASE_OFFSET,
		     QSERDES_TX_L0_TX_BAND, cfg.tx_l1_tx_band);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L2_BASE_OFFSET,
		     QSERDES_TX_L0_TX_BAND, cfg.tx_l2_tx_band);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L3_BASE_OFFSET,
		     QSERDES_TX_L0_TX_BAND, cfg.tx_l3_tx_band);

	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L0_BASE_OFFSET,
			       QSERDES_TX_L0_RESET_TSYNC_EN, 0x03);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L1_BASE_OFFSET,
			       QSERDES_TX_L0_RESET_TSYNC_EN, 0x03);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L2_BASE_OFFSET,
			       QSERDES_TX_L0_RESET_TSYNC_EN, 0x03);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L3_BASE_OFFSET,
			       QSERDES_TX_L0_RESET_TSYNC_EN, 0x03);

	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_SYSCLK_BUF_ENABLE, 0x1E);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_BIAS_EN_CLKBUFLR_EN, 0x07);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_SYSCLK_EN_SEL, 0x37);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_SYS_CLK_CTRL, 0x02);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_CLK_ENABLE1, 0x0E);
	if (ver == HDMI_VERSION_8996_V1)
		MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_BG_CTRL, 0x06);

	/* Bypass VCO calibration */
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_SVS_MODE_CLK_SEL,
					cfg.com_svs_mode_clk_sel);

	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_BG_TRIM, 0x0F);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_PLL_IVCO, 0x0F);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_VCO_TUNE_CTRL,
			cfg.com_vco_tune_ctrl);

	switch (ver) {
	case HDMI_VERSION_8996_V1:
		MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_SVS_MODE_CLK_SEL,
					cfg.com_svs_mode_clk_sel);
		break;
	default:
		MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_BG_CTRL, 0x06);
	}

	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_CLK_SELECT, 0x30);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_HSCLK_SEL,
		       cfg.com_hsclk_sel);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_LOCK_CMP_EN,
		       cfg.com_lock_cmp_en);

	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_PLL_CCTRL_MODE0,
		       cfg.com_pll_cctrl_mode0);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_PLL_RCTRL_MODE0,
		       cfg.com_pll_rctrl_mode0);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_CP_CTRL_MODE0,
		       cfg.com_cp_ctrl_mode0);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_DEC_START_MODE0,
		       cfg.com_dec_start_mode0);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_DIV_FRAC_START1_MODE0,
		       cfg.com_div_frac_start1_mode0);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_DIV_FRAC_START2_MODE0,
		       cfg.com_div_frac_start2_mode0);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_DIV_FRAC_START3_MODE0,
		       cfg.com_div_frac_start3_mode0);

	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_INTEGLOOP_GAIN0_MODE0,
			cfg.com_integloop_gain0_mode0);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_INTEGLOOP_GAIN1_MODE0,
			cfg.com_integloop_gain1_mode0);

	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_LOCK_CMP1_MODE0,
			cfg.com_lock_cmp1_mode0);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_LOCK_CMP2_MODE0,
			cfg.com_lock_cmp2_mode0);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_LOCK_CMP3_MODE0,
			cfg.com_lock_cmp3_mode0);

	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_VCO_TUNE_MAP, 0x00);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_CORE_CLK_EN,
		       cfg.com_core_clk_en);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_CORECLK_DIV,
		       cfg.com_coreclk_div);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_CMN_CONFIG, 0x02);

	if (ver == HDMI_VERSION_8996_V3 || ver == HDMI_VERSION_8996_V3_1_8)
		MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_RESCODE_DIV_NUM, 0x15);

	/* TX lanes setup (TX 0/1/2/3) */
	if (ver == HDMI_VERSION_8996_V3_1_8) {
		MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L0_BASE_OFFSET,
				QSERDES_TX_L0_TX_DRV_LVL,
				0x00000023);
	} else {
		MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L0_BASE_OFFSET,
				QSERDES_TX_L0_TX_DRV_LVL,
				cfg.tx_l0_tx_drv_lvl);
	}
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L0_BASE_OFFSET,
		       QSERDES_TX_L0_TX_EMP_POST1_LVL,
		       cfg.tx_l0_tx_emp_post1_lvl);

	if (ver == HDMI_VERSION_8996_V3_1_8) {
		MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L1_BASE_OFFSET,
				QSERDES_TX_L0_TX_DRV_LVL,
				0x00000023);
	} else {
		MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L1_BASE_OFFSET,
				QSERDES_TX_L0_TX_DRV_LVL,
				cfg.tx_l1_tx_drv_lvl);
	}
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L1_BASE_OFFSET,
		       QSERDES_TX_L0_TX_EMP_POST1_LVL,
		       cfg.tx_l1_tx_emp_post1_lvl);

	if (ver == HDMI_VERSION_8996_V3_1_8) {
		MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L2_BASE_OFFSET,
				QSERDES_TX_L0_TX_DRV_LVL,
				0x00000023);
	} else {
		MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L2_BASE_OFFSET,
				QSERDES_TX_L0_TX_DRV_LVL,
				cfg.tx_l2_tx_drv_lvl);
	}
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L2_BASE_OFFSET,
		       QSERDES_TX_L0_TX_EMP_POST1_LVL,
		       cfg.tx_l2_tx_emp_post1_lvl);

	if (ver == HDMI_VERSION_8996_V3_1_8) {
		MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L3_BASE_OFFSET,
				QSERDES_TX_L0_TX_DRV_LVL,
				0x00000020);
	} else {
		MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L3_BASE_OFFSET,
				QSERDES_TX_L0_TX_DRV_LVL,
				cfg.tx_l3_tx_drv_lvl);
	}
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L3_BASE_OFFSET,
		       QSERDES_TX_L0_TX_EMP_POST1_LVL,
		       cfg.tx_l3_tx_emp_post1_lvl);

	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L0_BASE_OFFSET,
		       QSERDES_TX_L0_VMODE_CTRL1,
		       cfg.tx_l0_vmode_ctrl1);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L0_BASE_OFFSET,
		       QSERDES_TX_L0_VMODE_CTRL2,
		       cfg.tx_l0_vmode_ctrl2);

	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L1_BASE_OFFSET,
		       QSERDES_TX_L0_VMODE_CTRL1,
		       cfg.tx_l1_vmode_ctrl1);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L1_BASE_OFFSET,
		       QSERDES_TX_L0_VMODE_CTRL2,
		       cfg.tx_l1_vmode_ctrl2);

	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L2_BASE_OFFSET,
		       QSERDES_TX_L0_VMODE_CTRL1,
		       cfg.tx_l2_vmode_ctrl1);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L2_BASE_OFFSET,
			QSERDES_TX_L0_VMODE_CTRL2,
			cfg.tx_l2_vmode_ctrl2);

	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L3_BASE_OFFSET,
		       QSERDES_TX_L0_VMODE_CTRL1,
		       cfg.tx_l3_vmode_ctrl1);
	if (ver == HDMI_VERSION_8996_V3_1_8) {
		MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L3_BASE_OFFSET,
				QSERDES_TX_L0_VMODE_CTRL2,
				0x0000000D);
	} else {
		MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L3_BASE_OFFSET,
				QSERDES_TX_L0_VMODE_CTRL2,
				cfg.tx_l3_vmode_ctrl2);
	}

	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L0_BASE_OFFSET,
		       QSERDES_TX_L0_TX_DRV_LVL_OFFSET, 0x00);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L1_BASE_OFFSET,
		       QSERDES_TX_L0_TX_DRV_LVL_OFFSET, 0x00);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L2_BASE_OFFSET,
		       QSERDES_TX_L0_TX_DRV_LVL_OFFSET, 0x00);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L3_BASE_OFFSET,
		       QSERDES_TX_L0_TX_DRV_LVL_OFFSET, 0x00);

	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L0_BASE_OFFSET,
		       QSERDES_TX_L0_RES_CODE_LANE_OFFSET, 0x00);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L1_BASE_OFFSET,
		       QSERDES_TX_L0_RES_CODE_LANE_OFFSET, 0x00);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L2_BASE_OFFSET,
		       QSERDES_TX_L0_RES_CODE_LANE_OFFSET, 0x00);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L3_BASE_OFFSET,
		       QSERDES_TX_L0_RES_CODE_LANE_OFFSET, 0x00);

	if (ver < HDMI_VERSION_8996_V3) {
		MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L0_BASE_OFFSET,
			       QSERDES_TX_L0_RES_CODE_LANE_TX,
			       cfg.tx_l0_res_code_lane_tx);
		MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L1_BASE_OFFSET,
			       QSERDES_TX_L0_RES_CODE_LANE_TX,
			       cfg.tx_l1_res_code_lane_tx);
		MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L2_BASE_OFFSET,
			       QSERDES_TX_L0_RES_CODE_LANE_TX,
			       cfg.tx_l2_res_code_lane_tx);
		MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L3_BASE_OFFSET,
			       QSERDES_TX_L0_RES_CODE_LANE_TX,
			       cfg.tx_l3_res_code_lane_tx);
		MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_RESTRIM_CTRL,
			       cfg.com_restrim_ctrl);

		MDSS_PLL_REG_W(io->phy_base, HDMI_PHY_TXCAL_CFG0, 0x00);
		MDSS_PLL_REG_W(io->phy_base, HDMI_PHY_TXCAL_CFG1, 0x05);
	}

	MDSS_PLL_REG_W(io->phy_base, HDMI_PHY_MODE, cfg.phy_mode);
	MDSS_PLL_REG_W(io->phy_base, HDMI_PHY_PD_CTL, 0x1F);

	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L0_BASE_OFFSET,
			QSERDES_TX_L0_TRAN_DRVR_EMP_EN, 0x03);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L1_BASE_OFFSET,
			QSERDES_TX_L0_TRAN_DRVR_EMP_EN, 0x03);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L2_BASE_OFFSET,
			QSERDES_TX_L0_TRAN_DRVR_EMP_EN, 0x03);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L3_BASE_OFFSET,
			QSERDES_TX_L0_TRAN_DRVR_EMP_EN, 0x03);

	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L0_BASE_OFFSET,
			QSERDES_TX_L0_PARRATE_REC_DETECT_IDLE_EN, 0x40);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L1_BASE_OFFSET,
			QSERDES_TX_L0_PARRATE_REC_DETECT_IDLE_EN, 0x40);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L2_BASE_OFFSET,
			QSERDES_TX_L0_PARRATE_REC_DETECT_IDLE_EN, 0x40);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L3_BASE_OFFSET,
			QSERDES_TX_L0_PARRATE_REC_DETECT_IDLE_EN, 0x40);

	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L0_BASE_OFFSET,
		       QSERDES_TX_L0_HP_PD_ENABLES, 0x0C);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L1_BASE_OFFSET,
		       QSERDES_TX_L0_HP_PD_ENABLES, 0x0C);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L2_BASE_OFFSET,
		       QSERDES_TX_L0_HP_PD_ENABLES, 0x0C);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L3_BASE_OFFSET,
		       QSERDES_TX_L0_HP_PD_ENABLES, 0x03);

	if (ver == HDMI_VERSION_8996_V2) {
		MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_ATB_SEL1, 0x01);
		MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_ATB_SEL2, 0x01);
	}
	/*
	 * Ensure that vco configuration gets flushed to hardware before
	 * enabling the PLL
	 */
	wmb();
	return 0;
}

static int hdmi_8996_phy_ready_status(struct mdss_pll_resources *io)
{
	u32 status = 0;
	int phy_ready = 0;
	int rc;
	u32 read_count = 0;

	rc = mdss_pll_resource_enable(io, true);
	if (rc) {
		DEV_ERR("%s: pll resource can't be enabled\n", __func__);
		return rc;
	}

	DEV_DBG("%s: Waiting for PHY Ready\n", __func__);

	/* Poll for PHY read status */
	while (read_count < HDMI_PLL_POLL_MAX_READS) {
		status = MDSS_PLL_REG_R(io->phy_base, HDMI_PHY_STATUS);
		if ((status & BIT(0)) == 1) {
			phy_ready = 1;
			DEV_DBG("%s: PHY READY\n", __func__);
			break;
		}
		udelay(HDMI_PLL_POLL_TIMEOUT_US);
		read_count++;
	}

	if (read_count == HDMI_PLL_POLL_MAX_READS) {
		phy_ready = 0;
		DEV_DBG("%s: PHY READY TIMEOUT\n", __func__);
	}

	mdss_pll_resource_enable(io, false);

	return phy_ready;
}

static int hdmi_8996_pll_lock_status(struct mdss_pll_resources *io)
{
	u32 status;
	int pll_locked = 0;
	int rc;
	u32 read_count = 0;

	rc = mdss_pll_resource_enable(io, true);
	if (rc) {
		DEV_ERR("%s: pll resource can't be enabled\n", __func__);
		return rc;
	}

	DEV_DBG("%s: Waiting for PLL lock\n", __func__);

	while (read_count < HDMI_PLL_POLL_MAX_READS) {
		status = MDSS_PLL_REG_R(io->pll_base,
				QSERDES_COM_C_READY_STATUS);
		if ((status & BIT(0)) == 1) {
			pll_locked = 1;
			DEV_DBG("%s: C READY\n", __func__);
			break;
		}
		udelay(HDMI_PLL_POLL_TIMEOUT_US);
		read_count++;
	}

	if (read_count == HDMI_PLL_POLL_MAX_READS) {
		pll_locked = 0;
		DEV_DBG("%s: C READY TIMEOUT\n", __func__);
	}

	mdss_pll_resource_enable(io, false);

	return pll_locked;
}

static int hdmi_8996_v1_perform_sw_calibration(struct clk *c)
{
	int rc = 0;
	struct hdmi_pll_vco_clk *vco = to_hdmi_8996_vco_clk(c);
	struct mdss_pll_resources *io = vco->priv;

	u32 max_code = 0x190;
	u32 min_code = 0x0;
	u32 max_cnt = 0;
	u32 min_cnt = 0;
	u32 expected_counter_value = 0;
	u32 step = 0;
	u32 dbus_all = 0;
	u32 dbus_sel = 0;
	u32 vco_code = 0;
	u32 val = 0;

	vco_code = 0xC8;

	DEV_DBG("%s: Starting SW calibration with vco_code = %d\n", __func__,
		 vco_code);

	expected_counter_value =
	   (MDSS_PLL_REG_R(io->pll_base, QSERDES_COM_LOCK_CMP3_MODE0) << 16) |
	   (MDSS_PLL_REG_R(io->pll_base, QSERDES_COM_LOCK_CMP2_MODE0) << 8) |
	   (MDSS_PLL_REG_R(io->pll_base, QSERDES_COM_LOCK_CMP1_MODE0));

	DEV_DBG("%s: expected_counter_value = %d\n", __func__,
		 expected_counter_value);

	val = MDSS_PLL_REG_R(io->pll_base, QSERDES_COM_CMN_MISC1);
	val |= BIT(4);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_CMN_MISC1, val);

	val = MDSS_PLL_REG_R(io->pll_base, QSERDES_COM_CMN_MISC1);
	val |= BIT(3);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_CMN_MISC1, val);

	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_DEBUG_BUS_SEL, 0x4);

	val = MDSS_PLL_REG_R(io->pll_base, QSERDES_COM_LOCK_CMP_CFG);
	val |= BIT(1);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_LOCK_CMP_CFG, val);

	udelay(60);

	while (1) {
		MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_VCO_TUNE1_MODE0,
			       vco_code & 0xFF);
		MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_VCO_TUNE2_MODE0,
			       (vco_code >> 8) & 0x3);

		udelay(20);

		val = MDSS_PLL_REG_R(io->pll_base, QSERDES_COM_LOCK_CMP_CFG);
		val &= ~BIT(1);
		MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_LOCK_CMP_CFG, val);

		udelay(60);

		val = MDSS_PLL_REG_R(io->pll_base, QSERDES_COM_LOCK_CMP_CFG);
		val |= BIT(1);
		MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_LOCK_CMP_CFG, val);

		udelay(60);

		dbus_all =
		  (MDSS_PLL_REG_R(io->pll_base, QSERDES_COM_DEBUG_BUS3) << 24) |
		  (MDSS_PLL_REG_R(io->pll_base, QSERDES_COM_DEBUG_BUS2) << 16) |
		  (MDSS_PLL_REG_R(io->pll_base, QSERDES_COM_DEBUG_BUS1) << 8) |
		  (MDSS_PLL_REG_R(io->pll_base, QSERDES_COM_DEBUG_BUS0));

		dbus_sel = (dbus_all >> 9) & 0x3FFFF;
		DEV_DBG("%s: loop[%d], dbus_all = 0x%x, dbus_sel = 0x%x\n",
			__func__, step, dbus_all, dbus_sel);
		if (dbus_sel == 0)
			DEV_ERR("%s: CHECK HDMI REF CLK\n", __func__);

		if (dbus_sel == expected_counter_value) {
			max_code = vco_code;
			max_cnt = dbus_sel;
			min_code = vco_code;
			min_cnt = dbus_sel;
		} else if (dbus_sel == 0) {
			max_code = vco_code;
			max_cnt = dbus_sel;
			vco_code = (max_code + min_code)/2;
		} else if (dbus_sel > expected_counter_value) {
			min_code = vco_code;
			min_cnt = dbus_sel;
			vco_code = (max_code + min_code)/2;
		} else if (dbus_sel < expected_counter_value) {
			max_code = vco_code;
			max_cnt = dbus_sel;
			vco_code = (max_code + min_code)/2;
		}

		step++;

		if ((vco_code == 0) || (vco_code == 0x3FF) || (step > 0x3FF)) {
			DEV_ERR("%s: VCO tune code search failed\n", __func__);
			rc = -ENOTSUPP;
			break;
		}
		if ((max_code - min_code) <= 1) {
			if ((max_code - min_code) == 1) {
				if (abs((int)(max_cnt - expected_counter_value))
				    < abs((int)(min_cnt - expected_counter_value
					))) {
					vco_code = max_code;
				} else {
					vco_code = min_code;
				}
			}
			break;
		}
		DEV_DBG("%s: loop[%d], new vco_code = %d\n", __func__, step,
			 vco_code);
	}

	DEV_DBG("%s: CALIB done. vco_code = %d\n", __func__, vco_code);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_VCO_TUNE1_MODE0,
		       vco_code & 0xFF);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_VCO_TUNE2_MODE0,
		       (vco_code >> 8) & 0x3);
	val = MDSS_PLL_REG_R(io->pll_base, QSERDES_COM_LOCK_CMP_CFG);
	val &= ~BIT(1);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_LOCK_CMP_CFG, val);

	val = MDSS_PLL_REG_R(io->pll_base, QSERDES_COM_CMN_MISC1);
	val |= BIT(4);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_CMN_MISC1, val);

	val = MDSS_PLL_REG_R(io->pll_base, QSERDES_COM_CMN_MISC1);
	val &= ~BIT(3);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_CMN_MISC1, val);

	return rc;
}

static int hdmi_8996_v2_perform_sw_calibration(struct clk *c)
{
	int rc = 0;
	struct hdmi_pll_vco_clk *vco = to_hdmi_8996_vco_clk(c);
	struct mdss_pll_resources *io = vco->priv;
	u32 vco_code1, vco_code2, integral_loop, ready_poll;
	u32 read_count = 0;

	while (read_count < (HDMI_PLL_POLL_MAX_READS << 1)) {
		ready_poll = MDSS_PLL_REG_R(io->pll_base,
				QSERDES_COM_C_READY_STATUS);
		if ((ready_poll & BIT(0)) == 1) {
			ready_poll = 1;
			DEV_DBG("%s: C READY\n", __func__);
			break;
		}
		udelay(HDMI_PLL_POLL_TIMEOUT_US);
		read_count++;
	}

	if (read_count == (HDMI_PLL_POLL_MAX_READS << 1)) {
		ready_poll = 0;
		DEV_DBG("%s: C READY TIMEOUT, TRYING SW CALIBRATION\n",
								__func__);
	}

	vco_code1 = MDSS_PLL_REG_R(io->pll_base,
				QSERDES_COM_PLLCAL_CODE1_STATUS);
	vco_code2 = MDSS_PLL_REG_R(io->pll_base,
				QSERDES_COM_PLLCAL_CODE2_STATUS);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_DEBUG_BUS_SEL, 0x5);
	integral_loop = MDSS_PLL_REG_R(io->pll_base,
				QSERDES_COM_DEBUG_BUS0);

	if (((ready_poll & 0x1) == 0) || (((ready_poll & 1) == 1) &&
			(vco_code1 == 0xFF) && ((vco_code2 & 0x3) == 0x1) &&
			(integral_loop > 0xC0))) {
		MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_ATB_SEL1, 0x04);
		MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_ATB_SEL2, 0x00);
		MDSS_PLL_REG_W(io->phy_base, HDMI_PHY_CFG, 0x17);
		udelay(100);

		MDSS_PLL_REG_W(io->phy_base, HDMI_PHY_CFG, 0x11);
		udelay(100);

		MDSS_PLL_REG_W(io->phy_base, HDMI_PHY_CFG, 0x19);
	}
	return rc;
}

static int hdmi_8996_perform_sw_calibration(struct clk *c, u32 ver)
{
	switch (ver) {
	case HDMI_VERSION_8996_V1:
		return hdmi_8996_v1_perform_sw_calibration(c);
	case HDMI_VERSION_8996_V2:
		return hdmi_8996_v2_perform_sw_calibration(c);
	}
	return 0;
}

static int hdmi_8996_vco_enable(struct clk *c, u32 ver)
{
	int rc = 0;
	struct hdmi_pll_vco_clk *vco = to_hdmi_8996_vco_clk(c);
	struct mdss_pll_resources *io = vco->priv;

	MDSS_PLL_REG_W(io->phy_base, HDMI_PHY_CFG, 0x1);
	udelay(100);

	MDSS_PLL_REG_W(io->phy_base, HDMI_PHY_CFG, 0x19);
	udelay(100);

	rc = hdmi_8996_perform_sw_calibration(c, ver);
	if (rc) {
		DEV_ERR("%s: software calibration failed\n", __func__);
		return rc;
	}

	rc = hdmi_8996_pll_lock_status(io);
	if (!rc) {
		DEV_ERR("%s: PLL not locked\n", __func__);
		return rc;
	}

	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L0_BASE_OFFSET,
		       QSERDES_TX_L0_HIGHZ_TRANSCEIVEREN_BIAS_DRVR_EN,
		       0x6F);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L1_BASE_OFFSET,
		       QSERDES_TX_L0_HIGHZ_TRANSCEIVEREN_BIAS_DRVR_EN,
		       0x6F);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L2_BASE_OFFSET,
		       QSERDES_TX_L0_HIGHZ_TRANSCEIVEREN_BIAS_DRVR_EN,
		       0x6F);
	MDSS_PLL_REG_W(io->pll_base + HDMI_TX_L3_BASE_OFFSET,
		       QSERDES_TX_L0_HIGHZ_TRANSCEIVEREN_BIAS_DRVR_EN,
		       0x6F);

	/* Disable SSC */
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_SSC_PER1, 0x0);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_SSC_PER2, 0x0);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_SSC_STEP_SIZE1, 0x0);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_SSC_STEP_SIZE2, 0x0);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_SSC_EN_CENTER, 0x2);

	rc = hdmi_8996_phy_ready_status(io);
	if (!rc) {
		DEV_ERR("%s: PHY not READY\n", __func__);
		return rc;
	}

	/* Restart the retiming buffer */
	MDSS_PLL_REG_W(io->phy_base, HDMI_PHY_CFG, 0x18);
	udelay(1);
	MDSS_PLL_REG_W(io->phy_base, HDMI_PHY_CFG, 0x19);

	io->pll_on = true;
	return 0;
}

static int hdmi_8996_v1_vco_enable(struct clk *c)
{
	return hdmi_8996_vco_enable(c, HDMI_VERSION_8996_V1);
}

static int hdmi_8996_v2_vco_enable(struct clk *c)
{
	return hdmi_8996_vco_enable(c, HDMI_VERSION_8996_V2);
}

static int hdmi_8996_v3_vco_enable(struct clk *c)
{
	return hdmi_8996_vco_enable(c, HDMI_VERSION_8996_V3);
}

static int hdmi_8996_v3_1p8_vco_enable(struct clk *c)
{
	return hdmi_8996_vco_enable(c, HDMI_VERSION_8996_V3_1_8);
}

static int hdmi_8996_vco_get_lock_range(struct clk *c, unsigned long pixel_clk)
{
	u32 rng = 64, cmp_cnt = 1024;
	u32 coreclk_div = 5, clks_pll_divsel = 2;
	u32 vco_freq, vco_ratio, ppm_range;
	u64 bclk;
	struct hdmi_8996_v3_post_divider pd;

	bclk = ((u64)pixel_clk) * HDMI_BIT_CLK_TO_PIX_CLK_RATIO;

	DEV_DBG("%s: rate=%ld\n", __func__, pixel_clk);

	if (hdmi_8996_v3_get_post_div(&pd, bclk) ||
		pd.vco_ratio <= 0 || pd.vco_freq <= 0) {
		DEV_ERR("%s: couldn't get post div\n", __func__);
		return -EINVAL;
	}

	do_div(pd.vco_freq, HDMI_KHZ_TO_HZ * HDMI_KHZ_TO_HZ);

	vco_freq  = (u32) pd.vco_freq;
	vco_ratio = (u32) pd.vco_ratio;

	DEV_DBG("%s: freq %d, ratio %d\n", __func__,
		vco_freq, vco_ratio);

	ppm_range = (rng * HDMI_REF_CLOCK) / cmp_cnt;
	ppm_range /= vco_freq / vco_ratio;
	ppm_range *= coreclk_div * clks_pll_divsel;

	DEV_DBG("%s: ppm range: %d\n", __func__, ppm_range);

	return ppm_range;
}

static int hdmi_8996_vco_rate_atomic_update(struct clk *c,
	unsigned long rate, u32 ver)
{
	struct hdmi_pll_vco_clk *vco = to_hdmi_8996_vco_clk(c);
	struct mdss_pll_resources *io = vco->priv;
	void __iomem *pll;
	struct hdmi_8996_phy_pll_reg_cfg cfg = {0};
	int rc = 0;

	rc = hdmi_8996_calculate(rate, &cfg, ver);
	if (rc) {
		DEV_ERR("%s: PLL calculation failed\n", __func__);
		goto end;
	}

	pll = io->pll_base;

	MDSS_PLL_REG_W(pll, QSERDES_COM_DEC_START_MODE0,
		       cfg.com_dec_start_mode0);
	MDSS_PLL_REG_W(pll, QSERDES_COM_DIV_FRAC_START1_MODE0,
		       cfg.com_div_frac_start1_mode0);
	MDSS_PLL_REG_W(pll, QSERDES_COM_DIV_FRAC_START2_MODE0,
		       cfg.com_div_frac_start2_mode0);
	MDSS_PLL_REG_W(pll, QSERDES_COM_DIV_FRAC_START3_MODE0,
		       cfg.com_div_frac_start3_mode0);

	MDSS_PLL_REG_W(pll, QSERDES_COM_FREQ_UPDATE, 0x01);
	MDSS_PLL_REG_W(pll, QSERDES_COM_FREQ_UPDATE, 0x00);

	DEV_DBG("%s: updated to rate %ld\n", __func__, rate);
end:
	return rc;
}

static int hdmi_8996_vco_set_rate(struct clk *c, unsigned long rate, u32 ver)
{
	struct hdmi_pll_vco_clk *vco = to_hdmi_8996_vco_clk(c);
	struct mdss_pll_resources *io = vco->priv;
	unsigned int set_power_dwn = 0;
	bool atomic_update = false;
	int rc, pll_lock_range;

	rc = mdss_pll_resource_enable(io, true);
	if (rc) {
		DEV_ERR("pll resource can't be enabled\n");
		return rc;
	}

	DEV_DBG("%s: rate %ld\n", __func__, rate);

	if (MDSS_PLL_REG_R(io->pll_base, QSERDES_COM_C_READY_STATUS) & BIT(0) &&
		MDSS_PLL_REG_R(io->phy_base, HDMI_PHY_STATUS) & BIT(0)) {
		pll_lock_range = hdmi_8996_vco_get_lock_range(c, vco->rate);

		if (pll_lock_range > 0 && vco->rate) {
			u32 range_limit;

			range_limit  = vco->rate *
				(pll_lock_range / HDMI_KHZ_TO_HZ);
			range_limit /= HDMI_KHZ_TO_HZ;

			DEV_DBG("%s: range limit %d\n", __func__, range_limit);

			if (abs(rate - vco->rate) < range_limit)
				atomic_update = true;
		}
	}

	if (io->pll_on && !atomic_update)
		set_power_dwn = 1;

	if (atomic_update) {
		hdmi_8996_vco_rate_atomic_update(c, rate, ver);
	} else {
		rc = hdmi_8996_phy_pll_set_clk_rate(c, rate, ver);
		if (rc)
			DEV_ERR("%s: Failed to set clk rate\n", __func__);
	}

	mdss_pll_resource_enable(io, false);

	if (set_power_dwn)
		hdmi_8996_vco_enable(c, ver);

	vco->rate = rate;
	vco->rate_set = true;

	return 0;
}

static int hdmi_8996_v1_vco_set_rate(struct clk *c, unsigned long rate)
{
	return hdmi_8996_vco_set_rate(c, rate, HDMI_VERSION_8996_V1);
}

static int hdmi_8996_v2_vco_set_rate(struct clk *c, unsigned long rate)
{
	return hdmi_8996_vco_set_rate(c, rate, HDMI_VERSION_8996_V2);
}

static int hdmi_8996_v3_vco_set_rate(struct clk *c, unsigned long rate)
{
	return hdmi_8996_vco_set_rate(c, rate, HDMI_VERSION_8996_V3);
}

static int hdmi_8996_v3_1p8_vco_set_rate(struct clk *c, unsigned long rate)
{
	return hdmi_8996_vco_set_rate(c, rate, HDMI_VERSION_8996_V3_1_8);
}

static unsigned long hdmi_get_hsclk_sel_divisor(unsigned long hsclk_sel)
{
	unsigned long divisor;

	switch (hsclk_sel) {
	case 0:
		divisor = 2;
		break;
	case 1:
		divisor = 6;
		break;
	case 2:
		divisor = 10;
		break;
	case 3:
		divisor = 14;
		break;
	case 4:
		divisor = 3;
		break;
	case 5:
		divisor = 9;
		break;
	case 6:
	case 13:
		divisor = 15;
		break;
	case 7:
		divisor = 21;
		break;
	case 8:
		divisor = 4;
		break;
	case 9:
		divisor = 12;
		break;
	case 10:
		divisor = 20;
		break;
	case 11:
		divisor = 28;
		break;
	case 12:
		divisor = 5;
		break;
	case 14:
		divisor = 25;
		break;
	case 15:
		divisor = 35;
		break;
	default:
		divisor = 1;
		DEV_ERR("%s: invalid hsclk_sel value = %lu",
				__func__, hsclk_sel);
		break;
	}

	return divisor;
}

static unsigned long hdmi_8996_vco_get_rate(struct clk *c)
{
	unsigned long freq = 0, hsclk_sel = 0, tx_band = 0, dec_start = 0,
		      div_frac_start = 0, vco_clock_freq = 0;
	struct hdmi_pll_vco_clk *vco = to_hdmi_8996_vco_clk(c);
	struct mdss_pll_resources *io = vco->priv;

	if (mdss_pll_resource_enable(io, true)) {
		DEV_ERR("%s: pll resource can't be enabled\n", __func__);
		return freq;
	}

	dec_start = MDSS_PLL_REG_R(io->pll_base, QSERDES_COM_DEC_START_MODE0);

	div_frac_start =
		MDSS_PLL_REG_R(io->pll_base,
				QSERDES_COM_DIV_FRAC_START1_MODE0) |
		MDSS_PLL_REG_R(io->pll_base,
				QSERDES_COM_DIV_FRAC_START2_MODE0) << 8 |
		MDSS_PLL_REG_R(io->pll_base,
				QSERDES_COM_DIV_FRAC_START3_MODE0) << 16;

	vco_clock_freq = (dec_start + (div_frac_start / (1 << 20)))
		* 4 * (HDMI_REF_CLOCK);

	hsclk_sel = MDSS_PLL_REG_R(io->pll_base, QSERDES_COM_HSCLK_SEL) & 0x15;
	hsclk_sel = hdmi_get_hsclk_sel_divisor(hsclk_sel);
	tx_band = MDSS_PLL_REG_R(io->pll_base + HDMI_TX_L0_BASE_OFFSET,
			QSERDES_TX_L0_TX_BAND) & 0x3;

	freq = vco_clock_freq / (10 * hsclk_sel * (1 << tx_band));

	mdss_pll_resource_enable(io, false);

	DEV_DBG("%s: freq = %lu\n", __func__, freq);

	return freq;
}

static long hdmi_8996_vco_round_rate(struct clk *c, unsigned long rate)
{
	unsigned long rrate = rate;

	DEV_DBG("rrate=%ld\n", rrate);

	return rrate;
}

static int hdmi_8996_vco_prepare(struct clk *c, u32 ver)
{
	struct hdmi_pll_vco_clk *vco = to_hdmi_8996_vco_clk(c);
	struct mdss_pll_resources *io = vco->priv;
	int ret = 0;

	DEV_DBG("rate=%ld\n", vco->rate);

	if (!vco->rate_set && vco->rate)
		ret = hdmi_8996_vco_set_rate(c, vco->rate, ver);

	if (!ret) {
		ret = mdss_pll_resource_enable(io, true);
		if (ret)
			DEV_ERR("pll resource can't be enabled\n");
	}

	return ret;
}

static int hdmi_8996_v1_vco_prepare(struct clk *c)
{
	return hdmi_8996_vco_prepare(c, HDMI_VERSION_8996_V1);
}

static int hdmi_8996_v2_vco_prepare(struct clk *c)
{
	return hdmi_8996_vco_prepare(c, HDMI_VERSION_8996_V2);
}

static int hdmi_8996_v3_vco_prepare(struct clk *c)
{
	return hdmi_8996_vco_prepare(c, HDMI_VERSION_8996_V3);
}

static int hdmi_8996_v3_1p8_vco_prepare(struct clk *c)
{
	return hdmi_8996_vco_prepare(c, HDMI_VERSION_8996_V3_1_8);
}

static void hdmi_8996_vco_unprepare(struct clk *c)
{
	struct hdmi_pll_vco_clk *vco = to_hdmi_8996_vco_clk(c);
	struct mdss_pll_resources *io = vco->priv;

	vco->rate_set = false;

	if (!io) {
		DEV_ERR("Invalid input parameter\n");
		return;
	}

	if (!io->pll_on &&
		mdss_pll_resource_enable(io, true)) {
		DEV_ERR("pll resource can't be enabled\n");
		return;
	}

	io->handoff_resources = false;
	mdss_pll_resource_enable(io, false);
	io->pll_on = false;
}

static enum handoff hdmi_8996_vco_handoff(struct clk *c)
{
	enum handoff ret = HANDOFF_DISABLED_CLK;
	struct hdmi_pll_vco_clk *vco = to_hdmi_8996_vco_clk(c);
	struct mdss_pll_resources *io = vco->priv;

	if (is_gdsc_disabled(io))
		return HANDOFF_DISABLED_CLK;

	if (mdss_pll_resource_enable(io, true)) {
		DEV_ERR("pll resource can't be enabled\n");
		return ret;
	}

	io->handoff_resources = true;

	if (MDSS_PLL_REG_R(io->pll_base, QSERDES_COM_C_READY_STATUS) & BIT(0)) {
		if (MDSS_PLL_REG_R(io->phy_base, HDMI_PHY_STATUS) & BIT(0)) {
			io->pll_on = true;
			c->rate = hdmi_8996_vco_get_rate(c);
			vco->rate = c->rate;
			ret = HANDOFF_ENABLED_CLK;
		} else {
			io->handoff_resources = false;
			mdss_pll_resource_enable(io, false);
			DEV_DBG("%s: PHY not ready\n", __func__);
		}
	} else {
		io->handoff_resources = false;
		mdss_pll_resource_enable(io, false);
		DEV_DBG("%s: PLL not locked\n", __func__);
	}

	DEV_DBG("done, ret=%d\n", ret);
	return ret;
}

static struct clk_ops hdmi_8996_v1_vco_clk_ops = {
	.enable = hdmi_8996_v1_vco_enable,
	.set_rate = hdmi_8996_v1_vco_set_rate,
	.get_rate = hdmi_8996_vco_get_rate,
	.round_rate = hdmi_8996_vco_round_rate,
	.prepare = hdmi_8996_v1_vco_prepare,
	.unprepare = hdmi_8996_vco_unprepare,
	.handoff = hdmi_8996_vco_handoff,
};

static struct clk_ops hdmi_8996_v2_vco_clk_ops = {
	.enable = hdmi_8996_v2_vco_enable,
	.set_rate = hdmi_8996_v2_vco_set_rate,
	.get_rate = hdmi_8996_vco_get_rate,
	.round_rate = hdmi_8996_vco_round_rate,
	.prepare = hdmi_8996_v2_vco_prepare,
	.unprepare = hdmi_8996_vco_unprepare,
	.handoff = hdmi_8996_vco_handoff,
};

static struct clk_ops hdmi_8996_v3_vco_clk_ops = {
	.enable = hdmi_8996_v3_vco_enable,
	.set_rate = hdmi_8996_v3_vco_set_rate,
	.get_rate = hdmi_8996_vco_get_rate,
	.round_rate = hdmi_8996_vco_round_rate,
	.prepare = hdmi_8996_v3_vco_prepare,
	.unprepare = hdmi_8996_vco_unprepare,
	.handoff = hdmi_8996_vco_handoff,
};

static struct clk_ops hdmi_8996_v3_1p8_vco_clk_ops = {
	.enable = hdmi_8996_v3_1p8_vco_enable,
	.set_rate = hdmi_8996_v3_1p8_vco_set_rate,
	.get_rate = hdmi_8996_vco_get_rate,
	.round_rate = hdmi_8996_vco_round_rate,
	.prepare = hdmi_8996_v3_1p8_vco_prepare,
	.unprepare = hdmi_8996_vco_unprepare,
	.handoff = hdmi_8996_vco_handoff,
};


static struct hdmi_pll_vco_clk hdmi_vco_clk = {
	.c = {
		.dbg_name = "hdmi_8996_vco_clk",
		.ops = &hdmi_8996_v1_vco_clk_ops,
		CLK_INIT(hdmi_vco_clk.c),
	},
};

static struct clk_lookup hdmipllcc_8996[] = {
	CLK_LIST(hdmi_vco_clk),
};

int hdmi_8996_pll_clock_register(struct platform_device *pdev,
				 struct mdss_pll_resources *pll_res, u32 ver)
{
	int rc = -ENOTSUPP;

	if (!pll_res || !pll_res->phy_base || !pll_res->pll_base) {
		DEV_ERR("%s: Invalid input parameters\n", __func__);
		return -EPROBE_DEFER;
	}

	/* Set client data for vco, mux and div clocks */
	hdmi_vco_clk.priv = pll_res;

	switch (ver) {
	case HDMI_VERSION_8996_V2:
		hdmi_vco_clk.c.ops = &hdmi_8996_v2_vco_clk_ops;
		break;
	case HDMI_VERSION_8996_V3:
		hdmi_vco_clk.c.ops = &hdmi_8996_v3_vco_clk_ops;
		break;
	case HDMI_VERSION_8996_V3_1_8:
		hdmi_vco_clk.c.ops = &hdmi_8996_v3_1p8_vco_clk_ops;
		break;
	default:
		hdmi_vco_clk.c.ops = &hdmi_8996_v1_vco_clk_ops;
		break;
	};

	rc = of_msm_clock_register(pdev->dev.of_node, hdmipllcc_8996,
					ARRAY_SIZE(hdmipllcc_8996));
	if (rc) {
		DEV_ERR("%s: Clock register failed rc=%d\n", __func__, rc);
		rc = -EPROBE_DEFER;
	} else {
		DEV_DBG("%s SUCCESS\n", __func__);
	}

	return rc;
}

int hdmi_8996_v1_pll_clock_register(struct platform_device *pdev,
				 struct mdss_pll_resources *pll_res)
{
	return hdmi_8996_pll_clock_register(pdev, pll_res,
						HDMI_VERSION_8996_V1);
}

int hdmi_8996_v2_pll_clock_register(struct platform_device *pdev,
				 struct mdss_pll_resources *pll_res)
{
	return hdmi_8996_pll_clock_register(pdev, pll_res,
						HDMI_VERSION_8996_V2);
}

int hdmi_8996_v3_pll_clock_register(struct platform_device *pdev,
				 struct mdss_pll_resources *pll_res)
{
	return hdmi_8996_pll_clock_register(pdev, pll_res,
						HDMI_VERSION_8996_V3);
}

int hdmi_8996_v3_1p8_pll_clock_register(struct platform_device *pdev,
				 struct mdss_pll_resources *pll_res)
{
	return hdmi_8996_pll_clock_register(pdev, pll_res,
						HDMI_VERSION_8996_V3_1_8);
}
