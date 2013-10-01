/*
 * Copyright (c) 2013, Linux Foundation. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/iopoll.h>
#include <linux/platform_device.h>
#include "ufshcd.h"
#include "unipro.h"

#define MAX_U32                 (~(u32)0)
#define MPHY_TX_FSM_STATE       0x41
#define TX_FSM_HIBERN8          0x1
#define HBRN8_POLL_TOUT_MS      100
#define DEFAULT_CLK_RATE_HZ     1000000

/* MSM UFS host controller vendor specific registers */
enum {
	REG_UFS_SYS1CLK_1US                 = 0xC0,
	REG_UFS_TX_SYMBOL_CLK_NS_US         = 0xC4,
	REG_UFS_LOCAL_PORT_ID_REG           = 0xC8,
	REG_UFS_PA_ERR_CODE                 = 0xCC,
	REG_UFS_RETRY_TIMER_REG             = 0xD0,
	REG_UFS_PA_LINK_STARTUP_TIMER       = 0xD8,
	REG_UFS_CFG1                        = 0xDC,
	REG_UFS_CFG2                        = 0xE0,
	REG_UFS_HW_VERSION                  = 0xE4,
};

/* bit offset */
enum {
	OFFSET_UFS_PHY_SOFT_RESET           = 1,
	OFFSET_CLK_NS_REG                   = 10,
};

/* bit masks */
enum {
	MASK_UFS_PHY_SOFT_RESET             = 0x2,
	MASK_TX_SYMBOL_CLK_1US_REG          = 0x3FF,
	MASK_CLK_NS_REG                     = 0xFFFC00,
};

/* MSM UFS PHY control registers */

#define COM_OFF(x)     (0x000 + x)
#define PHY_OFF(x)     (0x700 + x)
#define TX_OFF(n, x)   (0x100 + (0x400 * n) + x)
#define RX_OFF(n, x)   (0x200 + (0x400 * n) + x)

/* UFS PHY PLL block registers */
#define QSERDES_COM_SYS_CLK_CTRL                            COM_OFF(0x00)
#define QSERDES_COM_PLL_VCOTAIL_EN                          COM_OFF(0x04)
#define QSERDES_COM_CMN_MODE                                COM_OFF(0x08)
#define QSERDES_COM_IE_TRIM                                 COM_OFF(0x0C)
#define QSERDES_COM_IP_TRIM                                 COM_OFF(0x10)
#define QSERDES_COM_PLL_CNTRL                               COM_OFF(0x14)
#define QSERDES_COM_PLL_IP_SETI                             COM_OFF(0x18)
#define QSERDES_COM_CORE_CLK_IN_SYNC_SEL                    COM_OFF(0x1C)
#define QSERDES_COM_BIAS_EN_CLKBUFLR_EN                     COM_OFF(0x20)
#define QSERDES_COM_PLL_CP_SETI                             COM_OFF(0x24)
#define QSERDES_COM_PLL_IP_SETP                             COM_OFF(0x28)
#define QSERDES_COM_PLL_CP_SETP                             COM_OFF(0x2C)
#define QSERDES_COM_ATB_SEL1                                COM_OFF(0x30)
#define QSERDES_COM_ATB_SEL2                                COM_OFF(0x34)
#define QSERDES_COM_SYSCLK_EN_SEL                           COM_OFF(0x38)
#define QSERDES_COM_RES_CODE_TXBAND                         COM_OFF(0x3C)
#define QSERDES_COM_RESETSM_CNTRL                           COM_OFF(0x40)
#define QSERDES_COM_PLLLOCK_CMP1                            COM_OFF(0x44)
#define QSERDES_COM_PLLLOCK_CMP2                            COM_OFF(0x48)
#define QSERDES_COM_PLLLOCK_CMP3                            COM_OFF(0x4C)
#define QSERDES_COM_PLLLOCK_CMP_EN                          COM_OFF(0x50)
#define QSERDES_COM_RES_TRIM_OFFSET                         COM_OFF(0x54)
#define QSERDES_COM_BGTC                                    COM_OFF(0x58)
#define QSERDES_COM_PLL_TEST_UPDN_RESTRIMSTEP               COM_OFF(0x5C)
#define QSERDES_COM_PLL_VCO_TUNE                            COM_OFF(0x60)
#define QSERDES_COM_DEC_START1                              COM_OFF(0x64)
#define QSERDES_COM_PLL_AMP_OS                              COM_OFF(0x68)
#define QSERDES_COM_SSC_EN_CENTER                           COM_OFF(0x6C)
#define QSERDES_COM_SSC_ADJ_PER1                            COM_OFF(0x70)
#define QSERDES_COM_SSC_ADJ_PER2                            COM_OFF(0x74)
#define QSERDES_COM_SSC_PER1                                COM_OFF(0x78)
#define QSERDES_COM_SSC_PER2                                COM_OFF(0x7C)
#define QSERDES_COM_SSC_STEP_SIZE1                          COM_OFF(0x80)
#define QSERDES_COM_SSC_STEP_SIZE2                          COM_OFF(0x84)
#define QSERDES_COM_RES_TRIM_SEARCH                         COM_OFF(0x88)
#define QSERDES_COM_RES_TRIM_FREEZE                         COM_OFF(0x8C)
#define QSERDES_COM_RES_TRIM_EN_VCOCALDONE                  COM_OFF(0x90)
#define QSERDES_COM_FAUX_EN                                 COM_OFF(0x94)
#define QSERDES_COM_DIV_FRAC_START1                         COM_OFF(0x98)
#define QSERDES_COM_DIV_FRAC_START2                         COM_OFF(0x9C)
#define QSERDES_COM_DIV_FRAC_START3                         COM_OFF(0xA0)
#define QSERDES_COM_DEC_START2                              COM_OFF(0xA4)
#define QSERDES_COM_PLL_RXTXEPCLK_EN                        COM_OFF(0xA8)
#define QSERDES_COM_PLL_CRCTRL                              COM_OFF(0xAC)
#define QSERDES_COM_PLL_CLKEPDIV                            COM_OFF(0xB0)
#define QSERDES_COM_PLL_FREQUPDATE                          COM_OFF(0xB4)
#define QSERDES_COM_PLL_VCO_HIGH                            COM_OFF(0xB8)
#define QSERDES_COM_RESET_SM                                COM_OFF(0xBC)

/* UFS PHY registers */
#define UFS_PHY_PHY_START                                   PHY_OFF(0x00)
#define UFS_PHY_POWER_DOWN_CONTROL                          PHY_OFF(0x04)
#define UFS_PHY_PWM_G1_CLK_DIVIDER                          PHY_OFF(0x08)
#define UFS_PHY_PWM_G2_CLK_DIVIDER                          PHY_OFF(0x0C)
#define UFS_PHY_PWM_G3_CLK_DIVIDER                          PHY_OFF(0x10)
#define UFS_PHY_PWM_G4_CLK_DIVIDER                          PHY_OFF(0x14)
#define UFS_PHY_TIMER_100US_SYSCLK_STEPS_MSB                PHY_OFF(0x18)
#define UFS_PHY_TIMER_100US_SYSCLK_STEPS_LSB                PHY_OFF(0x1C)
#define UFS_PHY_TIMER_20US_CORECLK_STEPS_MSB                PHY_OFF(0x20)
#define UFS_PHY_TIMER_20US_CORECLK_STEPS_LSB                PHY_OFF(0x24)
#define UFS_PHY_LINE_RESET_TIME                             PHY_OFF(0x28)
#define UFS_PHY_LINE_RESET_GRANULARITY                      PHY_OFF(0x2C)
#define UFS_PHY_CONTROLSYM_ONE_HOT_DISABLE                  PHY_OFF(0x30)
#define UFS_PHY_CORECLK_PWM_G1_CLK_DIVIDER                  PHY_OFF(0x34)
#define UFS_PHY_CORECLK_PWM_G2_CLK_DIVIDER                  PHY_OFF(0x38)
#define UFS_PHY_CORECLK_PWM_G3_CLK_DIVIDER                  PHY_OFF(0x3C)
#define UFS_PHY_CORECLK_PWM_G4_CLK_DIVIDER                  PHY_OFF(0x40)
#define UFS_PHY_TX_LANE_ENABLE                              PHY_OFF(0x44)
#define UFS_PHY_TSYNC_RSYNC_CNTL                            PHY_OFF(0x48)
#define UFS_PHY_RETIME_BUFFER_EN                            PHY_OFF(0x4C)
#define UFS_PHY_PLL_CNTL                                    PHY_OFF(0x50)
#define UFS_PHY_TX_LARGE_AMP_DRV_LVL                        PHY_OFF(0x54)
#define UFS_PHY_TX_LARGE_AMP_POST_EMP_LVL                   PHY_OFF(0x58)
#define UFS_PHY_TX_SMALL_AMP_DRV_LVL                        PHY_OFF(0x5C)
#define UFS_PHY_TX_SMALL_AMP_POST_EMP_LVL                   PHY_OFF(0x60)
#define UFS_PHY_CFG_CHANGE_CNT_VAL                          PHY_OFF(0x64)
#define UFS_PHY_OMC_STATUS_RDVAL                            PHY_OFF(0x68)
#define UFS_PHY_RX_SYNC_WAIT_TIME                           PHY_OFF(0x6C)
#define UFS_PHY_L0_BIST_CTRL                                PHY_OFF(0x70)
#define UFS_PHY_L1_BIST_CTRL                                PHY_OFF(0x74)
#define UFS_PHY_BIST_PRBS_POLY0                             PHY_OFF(0x78)
#define UFS_PHY_BIST_PRBS_POLY1                             PHY_OFF(0x7C)
#define UFS_PHY_BIST_PRBS_SEED0                             PHY_OFF(0x80)
#define UFS_PHY_BIST_PRBS_SEED1                             PHY_OFF(0x84)
#define UFS_PHY_BIST_FIXED_PAT_CTRL                         PHY_OFF(0x88)
#define UFS_PHY_BIST_FIXED_PAT0_DATA                        PHY_OFF(0x8C)
#define UFS_PHY_BIST_FIXED_PAT1_DATA                        PHY_OFF(0x90)
#define UFS_PHY_BIST_FIXED_PAT2_DATA                        PHY_OFF(0x94)
#define UFS_PHY_BIST_FIXED_PAT3_DATA                        PHY_OFF(0x98)
#define UFS_PHY_TX_HSGEAR_CAPABILITY                        PHY_OFF(0x9C)
#define UFS_PHY_TX_PWMGEAR_CAPABILITY                       PHY_OFF(0xA0)
#define UFS_PHY_TX_AMPLITUDE_CAPABILITY                     PHY_OFF(0xA4)
#define UFS_PHY_TX_EXTERNALSYNC_CAPABILITY                  PHY_OFF(0xA8)
#define UFS_PHY_TX_HS_UNTERMINATED_LINE_DRIVE_CAPABILITY    PHY_OFF(0xAC)
#define UFS_PHY_TX_LS_TERMINATED_LINE_DRIVE_CAPABILITY      PHY_OFF(0xB0)
#define UFS_PHY_TX_MIN_SLEEP_NOCONFIG_TIME_CAPABILITY       PHY_OFF(0xB4)
#define UFS_PHY_TX_MIN_STALL_NOCONFIG_TIME_CAPABILITY       PHY_OFF(0xB8)
#define UFS_PHY_TX_MIN_SAVE_CONFIG_TIME_CAPABILITY          PHY_OFF(0xBC)
#define UFS_PHY_TX_REF_CLOCK_SHARED_CAPABILITY              PHY_OFF(0xC0)
#define UFS_PHY_TX_PHY_MAJORMINOR_RELEASE_CAPABILITY        PHY_OFF(0xC4)
#define UFS_PHY_TX_PHY_EDITORIAL_RELEASE_CAPABILITY         PHY_OFF(0xC8)
#define UFS_PHY_TX_HIBERN8TIME_CAPABILITY                   PHY_OFF(0xCC)
#define UFS_PHY_RX_HSGEAR_CAPABILITY                        PHY_OFF(0xD0)
#define UFS_PHY_RX_PWMGEAR_CAPABILITY                       PHY_OFF(0xD4)
#define UFS_PHY_RX_HS_UNTERMINATED_CAPABILITY               PHY_OFF(0xD8)
#define UFS_PHY_RX_LS_TERMINATED_CAPABILITY                 PHY_OFF(0xDC)
#define UFS_PHY_RX_MIN_SLEEP_NOCONFIG_TIME_CAPABILITY       PHY_OFF(0xE0)
#define UFS_PHY_RX_MIN_STALL_NOCONFIG_TIME_CAPABILITY       PHY_OFF(0xE4)
#define UFS_PHY_RX_MIN_SAVE_CONFIG_TIME_CAPABILITY          PHY_OFF(0xE8)
#define UFS_PHY_RX_REF_CLOCK_SHARED_CAPABILITY              PHY_OFF(0xEC)
#define UFS_PHY_RX_HS_G1_SYNC_LENGTH_CAPABILITY             PHY_OFF(0xF0)
#define UFS_PHY_RX_HS_G1_PREPARE_LENGTH_CAPABILITY          PHY_OFF(0xF4)
#define UFS_PHY_RX_LS_PREPARE_LENGTH_CAPABILITY             PHY_OFF(0xF8)
#define UFS_PHY_RX_PWM_BURST_CLOSURE_LENGTH_CAPABILITY      PHY_OFF(0xFC)
#define UFS_PHY_RX_MIN_ACTIVATETIME_CAPABILITY              PHY_OFF(0x100)
#define UFS_PHY_RX_PHY_MAJORMINOR_RELEASE_CAPABILITY        PHY_OFF(0x104)
#define UFS_PHY_RX_PHY_EDITORIAL_RELEASE_CAPABILITY         PHY_OFF(0x108)
#define UFS_PHY_RX_HIBERN8TIME_CAPABILITY                   PHY_OFF(0x10C)
#define UFS_PHY_RX_HS_G2_SYNC_LENGTH_CAPABILITY             PHY_OFF(0x110)
#define UFS_PHY_RX_HS_G3_SYNC_LENGTH_CAPABILITY             PHY_OFF(0x114)
#define UFS_PHY_RX_HS_G2_PREPARE_LENGTH_CAPABILITY          PHY_OFF(0x118)
#define UFS_PHY_RX_HS_G3_PREPARE_LENGTH_CAPABILITY          PHY_OFF(0x11C)
#define UFS_PHY_DEBUG_BUS_SEL                               PHY_OFF(0x120)
#define UFS_PHY_DEBUG_BUS_0_STATUS_CHK                      PHY_OFF(0x124)
#define UFS_PHY_DEBUG_BUS_1_STATUS_CHK                      PHY_OFF(0x128)
#define UFS_PHY_DEBUG_BUS_2_STATUS_CHK                      PHY_OFF(0x12C)
#define UFS_PHY_DEBUG_BUS_3_STATUS_CHK                      PHY_OFF(0x130)
#define UFS_PHY_PCS_READY_STATUS                            PHY_OFF(0x134)
#define UFS_PHY_L0_BIST_CHK_ERR_CNT_L_STATUS                PHY_OFF(0x138)
#define UFS_PHY_L0_BIST_CHK_ERR_CNT_H_STATUS                PHY_OFF(0x13C)
#define UFS_PHY_L1_BIST_CHK_ERR_CNT_L_STATUS                PHY_OFF(0x140)
#define UFS_PHY_L1_BIST_CHK_ERR_CNT_H_STATUS                PHY_OFF(0x144)
#define UFS_PHY_L0_BIST_CHK_STATUS                          PHY_OFF(0x148)
#define UFS_PHY_L1_BIST_CHK_STATUS                          PHY_OFF(0x14C)
#define UFS_PHY_DEBUG_BUS_0_STATUS                          PHY_OFF(0x150)
#define UFS_PHY_DEBUG_BUS_1_STATUS                          PHY_OFF(0x154)
#define UFS_PHY_DEBUG_BUS_2_STATUS                          PHY_OFF(0x158)
#define UFS_PHY_DEBUG_BUS_3_STATUS                          PHY_OFF(0x15C)

/* TX LANE n (0, 1) registers */
#define QSERDES_TX_BIST_MODE_LANENO(n)                      TX_OFF(n, 0x00)
#define QSERDES_TX_CLKBUF_ENABLE(n)                         TX_OFF(n, 0x04)
#define QSERDES_TX_TX_EMP_POST1_LVL(n)                      TX_OFF(n, 0x08)
#define QSERDES_TX_TX_DRV_LVL(n)                            TX_OFF(n, 0x0C)
#define QSERDES_TX_RESET_TSYNC_EN(n)                        TX_OFF(n, 0x10)
#define QSERDES_TX_LPB_EN(n)                                TX_OFF(n, 0x14)
#define QSERDES_TX_RES_CODE(n)                              TX_OFF(n, 0x18)
#define QSERDES_TX_PERL_LENGTH1(n)                          TX_OFF(n, 0x1C)
#define QSERDES_TX_PERL_LENGTH2(n)                          TX_OFF(n, 0x20)
#define QSERDES_TX_SERDES_BYP_EN_OUT(n)                     TX_OFF(n, 0x24)
#define QSERDES_TX_HIGHZ_TRANSCEIVEREN_BIAS_EN(n)           TX_OFF(n, 0x28)
#define QSERDES_TX_PARRATE_REC_DETECT_IDLE_EN(n)            TX_OFF(n, 0x2C)
#define QSERDES_TX_BIST_PATTERN1(n)                         TX_OFF(n, 0x30)
#define QSERDES_TX_BIST_PATTERN2(n)                         TX_OFF(n, 0x34)
#define QSERDES_TX_BIST_PATTERN3(n)                         TX_OFF(n, 0x38)
#define QSERDES_TX_BIST_PATTERN4(n)                         TX_OFF(n, 0x3C)
#define QSERDES_TX_BIST_PATTERN5(n)                         TX_OFF(n, 0x40)
#define QSERDES_TX_BIST_PATTERN6(n)                         TX_OFF(n, 0x44)
#define QSERDES_TX_BIST_PATTERN7(n)                         TX_OFF(n, 0x48)
#define QSERDES_TX_BIST_PATTERN8(n)                         TX_OFF(n, 0x4C)
#define QSERDES_TX_LANE_MODE(n)                             TX_OFF(n, 0x50)
#define QSERDES_TX_ATB_SEL(n)                               TX_OFF(n, 0x54)
#define QSERDES_TX_REC_DETECT_LVL(n)                        TX_OFF(n, 0x58)
#define QSERDES_TX_PRBS_SEED1(n)                            TX_OFF(n, 0x5C)
#define QSERDES_TX_PRBS_SEED2(n)                            TX_OFF(n, 0x60)
#define QSERDES_TX_PRBS_SEED3(n)                            TX_OFF(n, 0x64)
#define QSERDES_TX_PRBS_SEED4(n)                            TX_OFF(n, 0x68)
#define QSERDES_TX_RESET_GEN(n)                             TX_OFF(n, 0x6C)
#define QSERDES_TX_TRAN_DRVR_EMP_EN(n)                      TX_OFF(n, 0x70)
#define QSERDES_TX_TX_INTERFACE_MODE(n)                     TX_OFF(n, 0x74)
#define QSERDES_TX_BIST_STATUS(n)                           TX_OFF(n, 0x78)
#define QSERDES_TX_BIST_ERROR_COUNT1(n)                     TX_OFF(n, 0x7C)
#define QSERDES_TX_BIST_ERROR_COUNT2(n)                     TX_OFF(n, 0x80)

/* RX LANE n (0, 1) registers */
#define QSERDES_RX_CDR_CONTROL(n)                           RX_OFF(n, 0x00)
#define QSERDES_RX_AUX_CONTROL(n)                           RX_OFF(n, 0x04)
#define QSERDES_RX_AUX_DATA_TCODE(n)                        RX_OFF(n, 0x08)
#define QSERDES_RX_RCLK_AUXDATA_SEL(n)                      RX_OFF(n, 0x0C)
#define QSERDES_RX_EQ_CONTROL(n)                            RX_OFF(n, 0x10)
#define QSERDES_RX_RX_EQ_GAIN2(n)                           RX_OFF(n, 0x14)
#define QSERDES_RX_AC_JTAG_INIT(n)                          RX_OFF(n, 0x18)
#define QSERDES_RX_AC_JTAG_LVL_EN(n)                        RX_OFF(n, 0x1C)
#define QSERDES_RX_AC_JTAG_MODE(n)                          RX_OFF(n, 0x20)
#define QSERDES_RX_AC_JTAG_RESET(n)                         RX_OFF(n, 0x24)
#define QSERDES_RX_RX_IQ_RXDET_EN(n)                        RX_OFF(n, 0x28)
#define QSERDES_RX_RX_TERM_HIGHZ_CM_AC_COUPLE(n)            RX_OFF(n, 0x2C)
#define QSERDES_RX_RX_EQ_GAIN1(n)                           RX_OFF(n, 0x30)
#define QSERDES_RX_SIGDET_CNTRL(n)                          RX_OFF(n, 0x34)
#define QSERDES_RX_RX_BAND(n)                               RX_OFF(n, 0x38)
#define QSERDES_RX_CDR_FREEZE_UP_DN(n)                      RX_OFF(n, 0x3C)
#define QSERDES_RX_RX_INTERFACE_MODE(n)                     RX_OFF(n, 0x40)
#define QSERDES_RX_JITTER_GEN_MODE(n)                       RX_OFF(n, 0x44)
#define QSERDES_RX_BUJ_AMP(n)                               RX_OFF(n, 0x48)
#define QSERDES_RX_SJ_AMP1(n)                               RX_OFF(n, 0x4C)
#define QSERDES_RX_SJ_AMP2(n)                               RX_OFF(n, 0x50)
#define QSERDES_RX_SJ_PER1(n)                               RX_OFF(n, 0x54)
#define QSERDES_RX_SJ_PER2(n)                               RX_OFF(n, 0x58)
#define QSERDES_RX_BUJ_STEP_FREQ1(n)                        RX_OFF(n, 0x5C)
#define QSERDES_RX_BUJ_STEP_FREQ2(n)                        RX_OFF(n, 0x60)
#define QSERDES_RX_PPM_OFFSET1(n)                           RX_OFF(n, 0x64)
#define QSERDES_RX_PPM_OFFSET2(n)                           RX_OFF(n, 0x68)
#define QSERDES_RX_SIGN_PPM_PERIOD1(n)                      RX_OFF(n, 0x6C)
#define QSERDES_RX_SIGN_PPM_PERIOD2(n)                      RX_OFF(n, 0x70)
#define QSERDES_RX_SSC_CTRL(n)                              RX_OFF(n, 0x74)
#define QSERDES_RX_SSC_COUNT1(n)                            RX_OFF(n, 0x78)
#define QSERDES_RX_SSC_COUNT2(n)                            RX_OFF(n, 0x7C)
#define QSERDES_RX_PWM_CNTRL1(n)                            RX_OFF(n, 0x80)
#define QSERDES_RX_PWM_CNTRL2(n)                            RX_OFF(n, 0x84)
#define QSERDES_RX_PWM_NDIV(n)                              RX_OFF(n, 0x88)
#define QSERDES_RX_SIGDET_CNTRL2(n)                         RX_OFF(n, 0x8C)
#define QSERDES_RX_UFS_CNTRL(n)                             RX_OFF(n, 0x90)
#define QSERDES_RX_CDR_CONTROL3(n)                          RX_OFF(n, 0x94)
#define QSERDES_RX_CDR_CONTROL_HALF(n)                      RX_OFF(n, 0x98)
#define QSERDES_RX_CDR_CONTROL_QUARTER(n)                   RX_OFF(n, 0x9C)
#define QSERDES_RX_CDR_CONTROL_EIGHTH(n)                    RX_OFF(n, 0xA0)
#define QSERDES_RX_UCDR_FO_GAIN(n)                          RX_OFF(n, 0xA4)
#define QSERDES_RX_UCDR_SO_GAIN(n)                          RX_OFF(n, 0xA8)
#define QSERDES_RX_UCDR_SO_SATURATION_AND_ENABLE(n)         RX_OFF(n, 0xAC)
#define QSERDES_RX_UCDR_FO_TO_SO_DELAY(n)                   RX_OFF(n, 0xB0)
#define QSERDES_RX_PI_CTRL1(n)                              RX_OFF(n, 0xB4)
#define QSERDES_RX_PI_CTRL2(n)                              RX_OFF(n, 0xB8)
#define QSERDES_RX_PI_QUAD(n)                               RX_OFF(n, 0xBC)
#define QSERDES_RX_IDATA1(n)                                RX_OFF(n, 0xC0)
#define QSERDES_RX_IDATA2(n)                                RX_OFF(n, 0xC4)
#define QSERDES_RX_AUX_DATA1(n)                             RX_OFF(n, 0xC8)
#define QSERDES_RX_AUX_DATA2(n)                             RX_OFF(n, 0xCC)
#define QSERDES_RX_AC_JTAG_OUTP(n)                          RX_OFF(n, 0xD0)
#define QSERDES_RX_AC_JTAG_OUTN(n)                          RX_OFF(n, 0xD4)
#define QSERDES_RX_RX_SIGDET_PWMDECSTATUS(n)                RX_OFF(n, 0xD8)

enum {
	MASK_SERDES_START       = 0x1,
	MASK_PCS_READY          = 0x1,
};

enum {
	OFFSET_SERDES_START     = 0x0,
};

#define MAX_PROP_NAME              32
#define VDDA_PHY_MIN_UV            1000000
#define VDDA_PHY_MAX_UV            1000000
#define VDDA_PLL_MIN_UV            1800000
#define VDDA_PLL_MAX_UV            1800000

static LIST_HEAD(phy_list);

struct msm_ufs_phy_calibration {
	u32 reg_offset;
	u32 cfg_value;
};

struct msm_ufs_phy_vreg {
	const char *name;
	struct regulator *reg;
	int max_uA;
	int min_uV;
	int max_uV;
	bool enabled;
};

struct msm_ufs_phy {
	struct list_head list;
	struct device *dev;
	void __iomem *mmio;
	struct clk *tx_iface_clk;
	struct clk *rx_iface_clk;
	bool is_iface_clk_enabled;
	struct clk *ref_clk_src;
	struct clk *ref_clk_parent;
	struct clk *ref_clk;
	bool is_ref_clk_enabled;
	struct msm_ufs_phy_vreg vdda_pll;
	struct msm_ufs_phy_vreg vdda_phy;
};

static struct msm_ufs_phy_calibration phy_cal_table[] = {
	{
		.cfg_value = 0x01,
		.reg_offset = UFS_PHY_POWER_DOWN_CONTROL,
	},
	{
		.cfg_value = 0xFF,
		.reg_offset = QSERDES_COM_PLL_CRCTRL,
	},
	{
		.cfg_value = 0x24,
		.reg_offset = QSERDES_COM_PLL_CNTRL,
	},
	{
		.cfg_value = 0x08,
		.reg_offset = QSERDES_COM_SYSCLK_EN_SEL,
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_COM_SYS_CLK_CTRL,
	},
	{
		.cfg_value = 0x03,
		.reg_offset = QSERDES_COM_PLL_CLKEPDIV,
	},
	{
		.cfg_value = 0x82,
		.reg_offset = QSERDES_COM_DEC_START1,
	},
	{
		.cfg_value = 0x03,
		.reg_offset = QSERDES_COM_DEC_START2,
	},
	{
		.cfg_value = 0x80,
		.reg_offset = QSERDES_COM_DIV_FRAC_START1,
	},
	{
		.cfg_value = 0x80,
		.reg_offset = QSERDES_COM_DIV_FRAC_START2,
	},
	{
		.cfg_value = 0x10,
		.reg_offset = QSERDES_COM_DIV_FRAC_START3,
	},
	{
		.cfg_value = 0xff,
		.reg_offset = QSERDES_COM_PLLLOCK_CMP1,
	},
	{
		.cfg_value = 0x67,
		.reg_offset = QSERDES_COM_PLLLOCK_CMP2,
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_COM_PLLLOCK_CMP3,
	},
	{
		.cfg_value = 0x01,
		.reg_offset = QSERDES_COM_PLLLOCK_CMP_EN,
	},
	{
		.cfg_value = 0x10,
		.reg_offset = QSERDES_COM_RESETSM_CNTRL,
	},
	{
		.cfg_value = 0x13,
		.reg_offset = QSERDES_COM_PLL_RXTXEPCLK_EN,
	},
	{
		.cfg_value = 0x43,
		.reg_offset = QSERDES_RX_PWM_CNTRL1(0),
	},
	{
		.cfg_value = 0x43,
		.reg_offset = QSERDES_RX_PWM_CNTRL1(1),
	},
	{
		.cfg_value = 0xf2,
		.reg_offset = QSERDES_RX_CDR_CONTROL(0),
	},
	{
		.cfg_value = 0x2a,
		.reg_offset = QSERDES_RX_CDR_CONTROL_HALF(0),
	},
	{
		.cfg_value = 0x2a,
		.reg_offset = QSERDES_RX_CDR_CONTROL_QUARTER(0),
	},
	{
		.cfg_value = 0xf2,
		.reg_offset = QSERDES_RX_CDR_CONTROL(1),
	},
	{
		.cfg_value = 0x2a,
		.reg_offset = QSERDES_RX_CDR_CONTROL_HALF(1),
	},
	{
		.cfg_value = 0x2a,
		.reg_offset = QSERDES_RX_CDR_CONTROL_QUARTER(1),
	},
	{
		.cfg_value = 0xC0,
		.reg_offset = QSERDES_RX_SIGDET_CNTRL(0),
	},
	{
		.cfg_value = 0xC0,
		.reg_offset = QSERDES_RX_SIGDET_CNTRL(1),
	},
	{
		.cfg_value = 0x07,
		.reg_offset = QSERDES_RX_SIGDET_CNTRL2(0),
	},
	{
		.cfg_value = 0x07,
		.reg_offset = QSERDES_RX_SIGDET_CNTRL2(1),
	},
	{
		.cfg_value = 0x50,
		.reg_offset = UFS_PHY_PWM_G1_CLK_DIVIDER,
	},
	{
		.cfg_value = 0x28,
		.reg_offset = UFS_PHY_PWM_G2_CLK_DIVIDER,
	},
	{
		.cfg_value = 0x10,
		.reg_offset = UFS_PHY_PWM_G3_CLK_DIVIDER,
	},
	{
		.cfg_value = 0x08,
		.reg_offset = UFS_PHY_PWM_G4_CLK_DIVIDER,
	},
	{
		.cfg_value = 0xa8,
		.reg_offset = UFS_PHY_CORECLK_PWM_G1_CLK_DIVIDER,
	},
	{
		.cfg_value = 0x54,
		.reg_offset = UFS_PHY_CORECLK_PWM_G2_CLK_DIVIDER,
	},
	{
		.cfg_value = 0x2a,
		.reg_offset = UFS_PHY_CORECLK_PWM_G3_CLK_DIVIDER,
	},
	{
		.cfg_value = 0x15,
		.reg_offset = UFS_PHY_CORECLK_PWM_G4_CLK_DIVIDER,
	},
	{
		.cfg_value = 0xff,
		.reg_offset = UFS_PHY_OMC_STATUS_RDVAL,
	},
	{
		.cfg_value = 0x1f,
		.reg_offset = UFS_PHY_LINE_RESET_TIME,
	},
	{
		.cfg_value = 0x00,
		.reg_offset = UFS_PHY_LINE_RESET_GRANULARITY,
	},
	{
		.cfg_value = 0x03,
		.reg_offset = UFS_PHY_TSYNC_RSYNC_CNTL,
	},
	{
		.cfg_value = 0x01,
		.reg_offset = UFS_PHY_PLL_CNTL,
	},
	{
		.cfg_value = 0x0f,
		.reg_offset = UFS_PHY_TX_LARGE_AMP_DRV_LVL,
	},
	{
		.cfg_value = 0x1a,
		.reg_offset = UFS_PHY_TX_SMALL_AMP_DRV_LVL,
	},
	{
		.cfg_value = 0x00,
		.reg_offset = UFS_PHY_TX_LARGE_AMP_POST_EMP_LVL,
	},
	{
		.cfg_value = 0x00,
		.reg_offset = UFS_PHY_TX_SMALL_AMP_POST_EMP_LVL,
	},
	{
		.cfg_value = 0x09,
		.reg_offset = UFS_PHY_CFG_CHANGE_CNT_VAL,
	},
	{
		.cfg_value = 0x30,
		.reg_offset = UFS_PHY_RX_SYNC_WAIT_TIME,
	},
	{
		.cfg_value = 0x01,
		.reg_offset = UFS_PHY_TX_MIN_SLEEP_NOCONFIG_TIME_CAPABILITY,
	},
	{
		.cfg_value = 0x08,
		.reg_offset = UFS_PHY_RX_MIN_SLEEP_NOCONFIG_TIME_CAPABILITY,
	},
	{
		.cfg_value = 0x01,
		.reg_offset = UFS_PHY_TX_MIN_STALL_NOCONFIG_TIME_CAPABILITY,
	},
	{
		.cfg_value = 0x0f,
		.reg_offset = UFS_PHY_RX_MIN_STALL_NOCONFIG_TIME_CAPABILITY,
	},
	{
		.cfg_value = 0x04,
		.reg_offset = UFS_PHY_TX_MIN_SAVE_CONFIG_TIME_CAPABILITY,
	},
	{
		.cfg_value = 0xc8,
		.reg_offset = UFS_PHY_RX_MIN_SAVE_CONFIG_TIME_CAPABILITY,
	},
	{
		.cfg_value = 0x10,
		.reg_offset = UFS_PHY_RX_PWM_BURST_CLOSURE_LENGTH_CAPABILITY,
	},
	{
		.cfg_value = 0x01,
		.reg_offset = UFS_PHY_RX_MIN_ACTIVATETIME_CAPABILITY,
	},
	{
		.cfg_value = 0x07,
		.reg_offset = QSERDES_RX_RX_EQ_GAIN1(0),
	},
	{
		.cfg_value = 0x07,
		.reg_offset = QSERDES_RX_RX_EQ_GAIN2(0),
	},
	{
		.cfg_value = 0x07,
		.reg_offset = QSERDES_RX_RX_EQ_GAIN1(1),
	},
	{
		.cfg_value = 0x07,
		.reg_offset = QSERDES_RX_RX_EQ_GAIN2(1),
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_RX_CDR_CONTROL3(0),
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_RX_CDR_CONTROL3(1),
	},
	{
		.cfg_value = 0x01,
		.reg_offset = QSERDES_COM_PLL_IP_SETI,
	},
	{
		.cfg_value = 0x3f,
		.reg_offset = QSERDES_COM_PLL_CP_SETI,
	},
	{
		.cfg_value = 0x01,
		.reg_offset = QSERDES_COM_PLL_IP_SETP,
	},
	{
		.cfg_value = 0x01,
		.reg_offset = QSERDES_COM_PLL_CP_SETP,
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_COM_RES_TRIM_OFFSET,
	},
	{
		.cfg_value = 0x0f,
		.reg_offset = QSERDES_COM_BGTC,
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_COM_PLL_AMP_OS,
	},
	{
		.cfg_value = 0x0f,
		.reg_offset = QSERDES_TX_TX_DRV_LVL(0),
	},
	{
		.cfg_value = 0x0f,
		.reg_offset = QSERDES_TX_TX_DRV_LVL(1),
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_TX_BIST_MODE_LANENO(0),
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_TX_BIST_MODE_LANENO(1),
	},
	{
		.cfg_value = 0x04,
		.reg_offset = QSERDES_TX_TX_EMP_POST1_LVL(0),
	},
	{
		.cfg_value = 0x04,
		.reg_offset = QSERDES_TX_TX_EMP_POST1_LVL(1),
	},
	{
		.cfg_value = 0x05,
		.reg_offset = QSERDES_TX_HIGHZ_TRANSCEIVEREN_BIAS_EN(0),
	},
	{
		.cfg_value = 0x05,
		.reg_offset = QSERDES_TX_HIGHZ_TRANSCEIVEREN_BIAS_EN(1),
	},
	{
		.cfg_value = 0x07,
		.reg_offset = UFS_PHY_TIMER_100US_SYSCLK_STEPS_MSB,
	},
	{
		.cfg_value = 0x80,
		.reg_offset = UFS_PHY_TIMER_100US_SYSCLK_STEPS_LSB,
	},
	{
		.cfg_value = 0x27,
		.reg_offset = UFS_PHY_TIMER_20US_CORECLK_STEPS_MSB,
	},
	{
		.cfg_value = 0x00,
		.reg_offset = UFS_PHY_TIMER_20US_CORECLK_STEPS_LSB,
	},
	{
		.cfg_value = 0x00,
		.reg_offset = UFS_PHY_CONTROLSYM_ONE_HOT_DISABLE,
	},
	{
		.cfg_value = 0x01,
		.reg_offset = UFS_PHY_RETIME_BUFFER_EN,
	},
	{
		.cfg_value = 0x03,
		.reg_offset = UFS_PHY_TX_HSGEAR_CAPABILITY,
	},
	{
		.cfg_value = 0x04,
		.reg_offset = UFS_PHY_TX_PWMGEAR_CAPABILITY,
	},
	{
		.cfg_value = 0x03,
		.reg_offset = UFS_PHY_TX_AMPLITUDE_CAPABILITY,
	},
	{
		.cfg_value = 0x01,
		.reg_offset = UFS_PHY_TX_EXTERNALSYNC_CAPABILITY,
	},
	{
		.cfg_value = 0x01,
		.reg_offset = UFS_PHY_TX_HS_UNTERMINATED_LINE_DRIVE_CAPABILITY,
	},
	{
		.cfg_value = 0x01,
		.reg_offset = UFS_PHY_TX_LS_TERMINATED_LINE_DRIVE_CAPABILITY,
	},
	{
		.cfg_value = 0x01,
		.reg_offset = UFS_PHY_TX_REF_CLOCK_SHARED_CAPABILITY,
	},
	{
		.cfg_value = 0x01,
		.reg_offset = UFS_PHY_TX_HIBERN8TIME_CAPABILITY,
	},
	{
		.cfg_value = 0x03,
		.reg_offset = UFS_PHY_RX_HSGEAR_CAPABILITY,
	},
	{
		.cfg_value = 0x04,
		.reg_offset = UFS_PHY_RX_PWMGEAR_CAPABILITY,
	},
	{
		.cfg_value = 0x01,
		.reg_offset = UFS_PHY_RX_HS_UNTERMINATED_CAPABILITY,
	},
	{
		.cfg_value = 0x01,
		.reg_offset = UFS_PHY_RX_LS_TERMINATED_CAPABILITY,
	},
	{
		.cfg_value = 0x01,
		.reg_offset = UFS_PHY_RX_REF_CLOCK_SHARED_CAPABILITY,
	},
	{
		.cfg_value = 0x48,
		.reg_offset = UFS_PHY_RX_HS_G1_SYNC_LENGTH_CAPABILITY,
	},
	{
		.cfg_value = 0x0f,
		.reg_offset = UFS_PHY_RX_HS_G1_PREPARE_LENGTH_CAPABILITY,
	},
	{
		.cfg_value = 0x09,
		.reg_offset = UFS_PHY_RX_LS_PREPARE_LENGTH_CAPABILITY,
	},
	{
		.cfg_value = 0x01,
		.reg_offset = UFS_PHY_RX_HIBERN8TIME_CAPABILITY,
	},
	{
		.cfg_value = 0x48,
		.reg_offset = UFS_PHY_RX_HS_G2_SYNC_LENGTH_CAPABILITY,
	},
	{
		.cfg_value = 0x48,
		.reg_offset = UFS_PHY_RX_HS_G3_SYNC_LENGTH_CAPABILITY,
	},
	{
		.cfg_value = 0x0f,
		.reg_offset = UFS_PHY_RX_HS_G2_PREPARE_LENGTH_CAPABILITY,
	},
	{
		.cfg_value = 0x0f,
		.reg_offset = UFS_PHY_RX_HS_G3_PREPARE_LENGTH_CAPABILITY,
	},
	{
		.cfg_value = 0x09,
		.reg_offset = QSERDES_TX_CLKBUF_ENABLE(0),
	},
	{
		.cfg_value = 0x01,
		.reg_offset = QSERDES_TX_RESET_TSYNC_EN(0),
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_TX_RES_CODE(0),
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_TX_SERDES_BYP_EN_OUT(0),
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_TX_REC_DETECT_LVL(0),
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_TX_PARRATE_REC_DETECT_IDLE_EN(0),
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_TX_TRAN_DRVR_EMP_EN(0),
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_RX_AUX_CONTROL(0),
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_RX_AUX_DATA_TCODE(0),
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_RX_RCLK_AUXDATA_SEL(0),
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_RX_EQ_CONTROL(0),
	},
	{
		.cfg_value = 0x73,
		.reg_offset = QSERDES_RX_RX_IQ_RXDET_EN(0),
	},
	{
		.cfg_value = 0x05,
		.reg_offset = QSERDES_RX_RX_TERM_HIGHZ_CM_AC_COUPLE(0),
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_RX_CDR_FREEZE_UP_DN(0),
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_RX_UFS_CNTRL(0),
	},
	{
		.cfg_value = 0x22,
		.reg_offset = QSERDES_RX_CDR_CONTROL_EIGHTH(0),
	},
	{
		.cfg_value = 0x0a,
		.reg_offset = QSERDES_RX_UCDR_FO_GAIN(0),
	},
	{
		.cfg_value = 0x06,
		.reg_offset = QSERDES_RX_UCDR_SO_GAIN(0),
	},
	{
		.cfg_value = 0x35,
		.reg_offset = QSERDES_RX_UCDR_SO_SATURATION_AND_ENABLE(0),
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_RX_UCDR_FO_TO_SO_DELAY(0),
	},
	{
		.cfg_value = 0x09,
		.reg_offset = QSERDES_TX_CLKBUF_ENABLE(1),
	},
	{
		.cfg_value = 0x01,
		.reg_offset = QSERDES_TX_RESET_TSYNC_EN(1),
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_TX_RES_CODE(1),
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_TX_SERDES_BYP_EN_OUT(1),
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_TX_REC_DETECT_LVL(1),
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_TX_PARRATE_REC_DETECT_IDLE_EN(1),
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_TX_TRAN_DRVR_EMP_EN(1),
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_RX_AUX_CONTROL(1),
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_RX_AUX_DATA_TCODE(1),
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_RX_RCLK_AUXDATA_SEL(1),
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_RX_EQ_CONTROL(1),
	},
	{
		.cfg_value = 0x73,
		.reg_offset = QSERDES_RX_RX_IQ_RXDET_EN(1),
	},
	{
		.cfg_value = 0x05,
		.reg_offset = QSERDES_RX_RX_TERM_HIGHZ_CM_AC_COUPLE(1),
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_RX_CDR_FREEZE_UP_DN(1),
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_RX_UFS_CNTRL(1),
	},
	{
		.cfg_value = 0x22,
		.reg_offset = QSERDES_RX_CDR_CONTROL_EIGHTH(1),
	},
	{
		.cfg_value = 0x0a,
		.reg_offset = QSERDES_RX_UCDR_FO_GAIN(1),
	},
	{
		.cfg_value = 0x06,
		.reg_offset = QSERDES_RX_UCDR_SO_GAIN(1),
	},
	{
		.cfg_value = 0x35,
		.reg_offset = QSERDES_RX_UCDR_SO_SATURATION_AND_ENABLE(1),
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_RX_UCDR_FO_TO_SO_DELAY(1),
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_COM_CMN_MODE,
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_COM_IE_TRIM,
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_COM_IP_TRIM,
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_COM_CORE_CLK_IN_SYNC_SEL,
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_COM_BIAS_EN_CLKBUFLR_EN,
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_COM_PLL_TEST_UPDN_RESTRIMSTEP,
	},
	{
		.cfg_value = 0x00,
		.reg_offset = QSERDES_COM_FAUX_EN,
	},
};

static struct msm_ufs_phy *msm_get_ufs_phy(struct device *dev)
{
	int err  = -EPROBE_DEFER;
	struct msm_ufs_phy *phy;
	struct device_node *node;

	if (list_empty(&phy_list))
		goto out;

	node = of_parse_phandle(dev->of_node, "ufs-phy", 0);
	if (!node) {
		err = -EINVAL;
		dev_err(dev, "%s: ufs-phy property not specified\n", __func__);
		goto out;
	}

	list_for_each_entry(phy, &phy_list, list) {
		if (phy->dev->of_node == node) {
			err = 0;
			break;
		}
	}

	of_node_put(node);
out:
	if (err)
		return ERR_PTR(err);
	return phy;
}

/* Turn ON M-PHY RMMI interface clocks */
static int msm_ufs_enable_phy_iface_clk(struct msm_ufs_phy *phy)
{
	int ret = 0;

	if (phy->is_iface_clk_enabled)
		goto out;

	ret = clk_prepare_enable(phy->tx_iface_clk);
	if (ret)
		goto out;
	ret = clk_prepare_enable(phy->rx_iface_clk);
	if (ret)
		goto disable_tx_iface_clk;

	phy->is_iface_clk_enabled = true;

disable_tx_iface_clk:
	if (ret)
		clk_disable_unprepare(phy->tx_iface_clk);
out:
	if (ret)
		dev_err(phy->dev, "%s: iface_clk enable failed %d\n",
				__func__, ret);
	return ret;
}

/* Turn OFF M-PHY RMMI interface clocks */
static void msm_ufs_disable_phy_iface_clk(struct msm_ufs_phy *phy)
{
	if (phy->is_iface_clk_enabled) {
		clk_disable_unprepare(phy->tx_iface_clk);
		clk_disable_unprepare(phy->rx_iface_clk);
		phy->is_iface_clk_enabled = false;
	}
}

static int msm_ufs_enable_phy_ref_clk(struct msm_ufs_phy *phy)
{
	int ret = 0;

	if (phy->is_ref_clk_enabled)
		goto out;

	/*
	 * reference clock is propagated in a daisy-chained manner from
	 * source to phy, so ungate them at each stage.
	 */
	ret = clk_prepare_enable(phy->ref_clk_src);
	if (ret) {
		dev_err(phy->dev, "%s: ref_clk_src enable failed %d\n",
				__func__, ret);
		goto out;
	}

	ret = clk_prepare_enable(phy->ref_clk_parent);
	if (ret) {
		dev_err(phy->dev, "%s: ref_clk_parent enable failed %d\n",
				__func__, ret);
		goto out_disable_src;
	}

	ret = clk_prepare_enable(phy->ref_clk);
	if (ret) {
		dev_err(phy->dev, "%s: ref_clk enable failed %d\n",
				__func__, ret);
		goto out_disable_parent;
	}
	goto out;

out_disable_parent:
	clk_disable_unprepare(phy->ref_clk_parent);
out_disable_src:
	clk_disable_unprepare(phy->ref_clk_src);
out:
	return ret;
}

static void msm_ufs_disable_phy_ref_clk(struct msm_ufs_phy *phy)
{
	if (phy->is_ref_clk_enabled) {
		clk_disable_unprepare(phy->ref_clk);
		clk_disable_unprepare(phy->ref_clk_parent);
		clk_disable_unprepare(phy->ref_clk_src);
		phy->is_ref_clk_enabled = false;
	}
}


static int msm_ufs_phy_cfg_vreg(struct device *dev,
				struct msm_ufs_phy_vreg *vreg, bool on)
{
	int ret = 0;
	struct regulator *reg = vreg->reg;
	const char *name = vreg->name;
	int min_uV, uA_load;

	BUG_ON(!vreg);

	if (regulator_count_voltages(reg) > 0) {
		min_uV = on ? vreg->min_uV : 0;
		ret = regulator_set_voltage(reg, min_uV, vreg->max_uV);
		if (ret) {
			dev_err(dev, "%s: %s set voltage failed, err=%d\n",
					__func__, name, ret);
			goto out;
		}

		uA_load = on ? vreg->max_uA : 0;
		ret = regulator_set_optimum_mode(reg, uA_load);
		if (ret >= 0) {
			/*
			 * regulator_set_optimum_mode() returns new regulator
			 * mode upon success.
			 */
			ret = 0;
		} else {
			dev_err(dev, "%s: %s set optimum mode(uA_load=%d) failed, err=%d\n",
					__func__, name, uA_load, ret);
			goto out;
		}
	}
out:
	return ret;
}

static int msm_ufs_phy_enable_vreg(struct msm_ufs_phy *phy,
					struct msm_ufs_phy_vreg *vreg)
{
	struct device *dev = phy->dev;
	int ret = 0;

	if (!vreg || vreg->enabled)
		goto out;

	ret = msm_ufs_phy_cfg_vreg(dev, vreg, true);
	if (!ret)
		ret = regulator_enable(vreg->reg);

	if (!ret)
		vreg->enabled = true;
	else
		dev_err(dev, "%s: %s enable failed, err=%d\n",
				__func__, vreg->name, ret);
out:
	return ret;
}

static int msm_ufs_phy_disable_vreg(struct msm_ufs_phy *phy,
					struct msm_ufs_phy_vreg *vreg)
{
	struct device *dev = phy->dev;
	int ret = 0;

	if (!vreg || !vreg->enabled)
		goto out;

	ret = regulator_disable(vreg->reg);

	if (!ret) {
		/* ignore errors on applying disable config */
		msm_ufs_phy_cfg_vreg(dev, vreg, false);
		vreg->enabled = false;
	} else {
		dev_err(dev, "%s: %s disable failed, err=%d\n",
				__func__, vreg->name, ret);
	}
out:
	return ret;
}

static void msm_ufs_phy_calibrate(struct msm_ufs_phy *phy)
{
	struct msm_ufs_phy_calibration *tbl = phy_cal_table;
	int tbl_size = ARRAY_SIZE(phy_cal_table);
	int i;

	for (i = 0; i < tbl_size; i++)
		writel_relaxed(tbl[i].cfg_value, phy->mmio + tbl[i].reg_offset);

	/* flush buffered writes */
	mb();
}

static int msm_ufs_enable_tx_lanes(struct ufs_hba *hba)
{
	int err;
	u32 tx_lanes;
	u32 val;
	struct msm_ufs_phy *phy = hba->priv;

	err = ufshcd_dme_get(hba,
			UIC_ARG_MIB(PA_CONNECTEDTXDATALANES), &tx_lanes);
	if (err) {
		dev_err(hba->dev, "%s: couldn't read PA_CONNECTEDTXDATALANES %d\n",
				__func__, err);
		goto out;
	}

	val = ~(MAX_U32 << tx_lanes);
	writel_relaxed(val, phy->mmio + UFS_PHY_TX_LANE_ENABLE);
	mb();
out:
	return err;
}

static int msm_ufs_check_hibern8(struct ufs_hba *hba)
{
	int err;
	u32 tx_fsm_val = 0;
	unsigned long timeout = jiffies + msecs_to_jiffies(HBRN8_POLL_TOUT_MS);

	do {
		err = ufshcd_dme_get(hba,
			UIC_ARG_MIB(MPHY_TX_FSM_STATE), &tx_fsm_val);
		if (err || tx_fsm_val == TX_FSM_HIBERN8)
			break;

		/* sleep for max. 200us */
		usleep_range(100, 200);
	} while (time_before(jiffies, timeout));

	/*
	 * we might have scheduled out for long during polling so
	 * check the state again.
	 */
	if (time_after(jiffies, timeout))
		err = ufshcd_dme_get(hba,
				UIC_ARG_MIB(MPHY_TX_FSM_STATE), &tx_fsm_val);

	if (err) {
		dev_err(hba->dev, "%s: unable to get TX_FSM_STATE, err %d\n",
				__func__, err);
	} else if (tx_fsm_val != TX_FSM_HIBERN8) {
		err = tx_fsm_val;
		dev_err(hba->dev, "%s: invalid TX_FSM_STATE = %d\n",
				__func__, err);
	}

	return err;
}

static inline void msm_ufs_phy_start_serdes(struct msm_ufs_phy *phy)
{
	u32 tmp;

	tmp = readl_relaxed(phy->mmio + UFS_PHY_PHY_START);
	tmp &= ~MASK_SERDES_START;
	tmp |= (1 << OFFSET_SERDES_START);
	writel_relaxed(tmp, phy->mmio + UFS_PHY_PHY_START);
	mb();
}

static int msm_ufs_phy_power_on(struct msm_ufs_phy *phy)
{
	int err;

	err = msm_ufs_phy_enable_vreg(phy, &phy->vdda_phy);
	if (err)
		goto out;

	/* vdda_pll also enables ref clock LDOs so enable it first */
	err = msm_ufs_phy_enable_vreg(phy, &phy->vdda_pll);
	if (err)
		goto out_disable_phy;

	err = msm_ufs_enable_phy_ref_clk(phy);
	if (err)
		goto out_disable_pll;

	err = msm_ufs_enable_phy_iface_clk(phy);
	if (err)
		goto out_disable_ref;

	goto out;

out_disable_ref:
	msm_ufs_disable_phy_ref_clk(phy);
out_disable_pll:
	msm_ufs_phy_disable_vreg(phy, &phy->vdda_pll);
out_disable_phy:
	msm_ufs_phy_disable_vreg(phy, &phy->vdda_phy);
out:
	return err;
}

static int msm_ufs_phy_power_off(struct msm_ufs_phy *phy)
{
	writel_relaxed(0x0, phy->mmio + UFS_PHY_POWER_DOWN_CONTROL);
	mb();

	msm_ufs_disable_phy_iface_clk(phy);
	msm_ufs_disable_phy_ref_clk(phy);

	msm_ufs_phy_disable_vreg(phy, &phy->vdda_pll);
	msm_ufs_phy_disable_vreg(phy, &phy->vdda_phy);

	return 0;
}

static void msm_ufs_cfg_timers(struct ufs_hba *hba)
{
	unsigned long core_clk_rate = 0;
	unsigned long tx_clk_rate = 0;
	u32 core_clk_cycles_per_us;
	u32 core_clk_period_in_ns;
	u32 tx_clk_cycles_per_us;
	u32 core_clk_cycles_per_100ms;
	struct ufs_clk_info *clki;

	list_for_each_entry(clki, &hba->clk_list_head, list) {
		if (!strcmp(clki->name, "core_clk"))
			core_clk_rate = clk_get_rate(clki->clk);
		else if (!strcmp(clki->name, "tx_lane0_sync_clk"))
			tx_clk_rate = clk_get_rate(clki->clk);
	}

	/* If frequency is smaller than 1MHz, set to 1MHz */
	if (core_clk_rate < DEFAULT_CLK_RATE_HZ)
		core_clk_rate = DEFAULT_CLK_RATE_HZ;

	if (tx_clk_rate < DEFAULT_CLK_RATE_HZ)
		tx_clk_rate = DEFAULT_CLK_RATE_HZ;

	core_clk_cycles_per_us = core_clk_rate / USEC_PER_SEC;
	ufshcd_writel(hba, core_clk_cycles_per_us, REG_UFS_SYS1CLK_1US);

	core_clk_period_in_ns = NSEC_PER_SEC / core_clk_rate;
	tx_clk_cycles_per_us = tx_clk_rate / USEC_PER_SEC;
	tx_clk_cycles_per_us &= MASK_TX_SYMBOL_CLK_1US_REG;
	core_clk_period_in_ns <<= OFFSET_CLK_NS_REG;
	core_clk_period_in_ns &= MASK_CLK_NS_REG;
	ufshcd_writel(hba, core_clk_period_in_ns | tx_clk_cycles_per_us,
			REG_UFS_TX_SYMBOL_CLK_NS_US);

	core_clk_cycles_per_100ms = (core_clk_rate / MSEC_PER_SEC) * 100;
	ufshcd_writel(hba, core_clk_cycles_per_100ms,
				REG_UFS_PA_LINK_STARTUP_TIMER);
}

static inline void msm_ufs_assert_reset(struct ufs_hba *hba)
{
	ufshcd_rmwl(hba, MASK_UFS_PHY_SOFT_RESET,
			1 << OFFSET_UFS_PHY_SOFT_RESET, REG_UFS_CFG1);
	mb();
}

static inline void msm_ufs_deassert_reset(struct ufs_hba *hba)
{
	ufshcd_rmwl(hba, MASK_UFS_PHY_SOFT_RESET,
			0 << OFFSET_UFS_PHY_SOFT_RESET, REG_UFS_CFG1);
	mb();
}

static int msm_ufs_hce_enable_notify(struct ufs_hba *hba, bool status)
{
	struct msm_ufs_phy *phy = hba->priv;
	u32 val;
	int err = -EINVAL;

	switch (status) {
	case PRE_CHANGE:
		/* Assert PHY reset and apply PHY calibration values */
		msm_ufs_assert_reset(hba);

		/* provide 1ms delay to let the reset pulse propagate */
		usleep_range(1000, 1100);

		msm_ufs_phy_calibrate(phy);

		/* De-assert PHY reset and start serdes */
		msm_ufs_deassert_reset(hba);

		/*
		 * after reset deassertion, phy will need all ref clocks,
		 * voltage, current to settle down before starting serdes.
		 */
		usleep_range(1000, 1100);

		msm_ufs_phy_start_serdes(phy);

		/* poll for PCS_READY for max. 1sec */
		err = readl_poll_timeout(phy->mmio + UFS_PHY_PCS_READY_STATUS,
				val, (val & MASK_PCS_READY), 10, 1000000);
		if (err)
			dev_err(phy->dev, "%s: phy init failed, %d\n",
					__func__, err);
		break;
	case POST_CHANGE:
		/* check if UFS PHY moved from DISABLED to HIBERN8 */
		err = msm_ufs_check_hibern8(hba);
	default:
		break;
	}

	return err;
}

static int msm_ufs_link_startup_notify(struct ufs_hba *hba, bool status)
{
	switch (status) {
	case PRE_CHANGE:
		msm_ufs_cfg_timers(hba);
	case POST_CHANGE:
		msm_ufs_enable_tx_lanes(hba);
	default:
		break;
	}

	return 0;
}

static int msm_ufs_suspend(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	struct msm_ufs_phy *phy = hba->priv;
	int ret = 0;

	if (!phy)
		return 0;

	if (ufshcd_is_link_off(hba)) {
		msm_ufs_phy_power_off(phy);
		goto out;
	}

	/* M-PHY RMMI interface clocks can be turned off */
	msm_ufs_disable_phy_iface_clk(phy);

	/*
	 * If UniPro link is not active, PHY ref_clk and main PHY analog power
	 * can be switched off.
	 */
	if (!ufshcd_is_link_active(hba)) {
		msm_ufs_disable_phy_ref_clk(phy);
		ret = msm_ufs_phy_disable_vreg(phy, &phy->vdda_phy);
		/*
		 * TODO: Check if "vdda_pll" can voted off when link is hibern8
		 * or power off state?
		 */
	}

out:
	return ret;
}

static int msm_ufs_resume(struct ufs_hba *hba, enum ufs_pm_op pm_op)
{
	struct msm_ufs_phy *phy = hba->priv;

	if (!phy)
		return 0;

	return msm_ufs_phy_power_on(phy);
}

#define UFS_HW_VER_MAJOR_SHFT	(28)
#define UFS_HW_VER_MAJOR_MASK	(0x000F << UFS_HW_VER_MAJOR_SHFT)
#define UFS_HW_VER_MINOR_SHFT	(16)
#define UFS_HW_VER_MINOR_MASK	(0x0FFF << UFS_HW_VER_MINOR_SHFT)
#define UFS_HW_VER_STEP_SHFT	(0)
#define UFS_HW_VER_STEP_MASK	(0xFFFF << UFS_HW_VER_STEP_SHFT)

/**
 * msm_ufs_advertise_quirks - advertise the known MSM UFS controller quirks
 * @hba: host controller instance
 *
 * MSM UFS host controller might have some non standard behaviours (quirks)
 * than what is specified by UFSHCI specification. Advertise all such
 * quirks to standard UFS host controller driver so standard takes them into
 * account.
 */
static void msm_ufs_advertise_quirks(struct ufs_hba *hba)
{
	u32 ver = ufshcd_readl(hba, REG_UFS_HW_VERSION);
	u8 major;
	u16 minor, step;

	major = (ver & UFS_HW_VER_MAJOR_MASK) >> UFS_HW_VER_MAJOR_SHFT;
	minor = (ver & UFS_HW_VER_MINOR_MASK) >> UFS_HW_VER_MINOR_SHFT;
	step = (ver & UFS_HW_VER_STEP_MASK) >> UFS_HW_VER_STEP_SHFT;

	/*
	 * Interrupt aggregation and HIBERN8 on UFS HW controller revision 1.1.0
	 * is broken.
	 */
	if ((major == 0x1) && (minor == 0x001) && (step == 0x0000))
		hba->quirks |= (UFSHCD_QUIRK_BROKEN_INTR_AGGR
			      | UFSHCD_QUIRK_BROKEN_HIBERN8
			      | UFSHCD_QUIRK_BROKEN_VER_REG_1_1
			      | UFSHCD_QUIRK_BROKEN_CAP_64_BIT_0
			      | UFSHCD_QUIRK_BROKEN_DEVICE_Q_CMND
			      | UFSHCD_QUIRK_BROKEN_PWR_MODE_CHANGE
			      | UFSHCD_QUIRK_BROKEN_SUSPEND);
}

/**
 * msm_ufs_init - bind phy with controller
 * @hba: host controller instance
 *
 * Binds PHY with controller and powers up PHY enabling clocks
 * and regulators.
 *
 * Returns -EPROBE_DEFER if binding fails, returns negative error
 * on phy power up failure and returns zero on success.
 */
static int msm_ufs_init(struct ufs_hba *hba)
{
	int err;
	struct msm_ufs_phy *phy = msm_get_ufs_phy(hba->dev);

	if (IS_ERR(phy)) {
		err = PTR_ERR(phy);
		goto out;
	}

	hba->priv = (void *)phy;

	err = msm_ufs_phy_power_on(phy);
	if (err)
		hba->priv = NULL;

	msm_ufs_advertise_quirks(hba);
	if (hba->quirks & UFSHCD_QUIRK_BROKEN_SUSPEND) {
		/*
		 * During runtime suspend and system suspend keep the device
		 * and the link active but shut-off the system clocks.
		 */
		hba->rpm_lvl = UFS_PM_LVL_0;
		hba->spm_lvl = UFS_PM_LVL_0;

	} else if (hba->quirks & UFSHCD_QUIRK_BROKEN_HIBERN8) {
		/*
		 * During runtime suspend, keep link active but put device in
		 * sleep state.
		 * During system suspend, power off both link and device.
		 */
		hba->rpm_lvl = UFS_PM_LVL_2;
		hba->spm_lvl = UFS_PM_LVL_4;
	} else {
		/*
		 * During runtime & system suspend, put link in Hibern8 and
		 * device in sleep.
		 */
		hba->rpm_lvl = UFS_PM_LVL_3;
		hba->spm_lvl = UFS_PM_LVL_3;
	}
out:
	return err;
}

static void msm_ufs_exit(struct ufs_hba *hba)
{
	struct msm_ufs_phy *phy = hba->priv;

	msm_ufs_phy_power_off(phy);
}

static int msm_ufs_phy_init_vreg(struct device *dev,
		struct msm_ufs_phy_vreg *vreg, const char *name)
{
	int err = 0;
	char prop_name[MAX_PROP_NAME];

	vreg->name = kstrdup(name, GFP_KERNEL);
	if (!vreg->name) {
		err = -ENOMEM;
		goto out;
	}

	vreg->reg = devm_regulator_get(dev, name);
	if (IS_ERR(vreg->reg)) {
		err = PTR_ERR(vreg->reg);
		dev_err(dev, "failed to get %s, %d\n", name, err);
		goto out;
	}

	if (dev->of_node) {
		snprintf(prop_name, MAX_PROP_NAME, "%s-max-microamp", name);
		err = of_property_read_u32(dev->of_node,
					prop_name, &vreg->max_uA);
		if (err && err != -EINVAL) {
			dev_err(dev, "%s: failed to read %s\n",
					__func__, prop_name);
			goto out;
		} else if (err == -EINVAL || !vreg->max_uA) {
			if (regulator_count_voltages(vreg->reg) > 0) {
				dev_err(dev, "%s: %s is mandatory\n",
						__func__, prop_name);
				goto out;
			}
			err = 0;
		}
	}

	if (!strcmp(name, "vdda-pll")) {
		vreg->max_uV = VDDA_PLL_MAX_UV;
		vreg->min_uV = VDDA_PLL_MIN_UV;
	} else if (!strcmp(name, "vdda-phy")) {
		vreg->max_uV = VDDA_PHY_MAX_UV;
		vreg->min_uV = VDDA_PHY_MIN_UV;
	}

out:
	if (err)
		kfree(vreg->name);
	return err;
}

static int msm_ufs_phy_clk_get(struct device *dev,
		const char *name, struct clk **clk_out)
{
	struct clk *clk;
	int err = 0;

	clk = devm_clk_get(dev, name);
	if (IS_ERR(clk)) {
		err = PTR_ERR(clk);
		dev_err(dev, "failed to get %s err %d", name, err);
	} else {
		*clk_out = clk;
	}

	return err;
}

static int msm_ufs_phy_probe(struct platform_device *pdev)
{
	struct msm_ufs_phy *phy;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int err = 0;

	phy = devm_kzalloc(dev, sizeof(*phy), GFP_KERNEL);
	if (!phy) {
		err = -ENOMEM;
		dev_err(dev, "failed to allocate memory %d\n", err);
		goto out;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	phy->mmio = devm_ioremap_resource(dev, res);
	if (IS_ERR(phy->mmio)) {
		err = PTR_ERR(phy->mmio);
		goto out;
	}

	err = msm_ufs_phy_clk_get(dev, "tx_iface_clk", &phy->tx_iface_clk);
	if (err)
		goto out;

	err = msm_ufs_phy_clk_get(dev, "rx_iface_clk", &phy->rx_iface_clk);
	if (err)
		goto out;

	err = msm_ufs_phy_clk_get(dev, "ref_clk_src", &phy->ref_clk_src);
	if (err)
		goto out;

	err = msm_ufs_phy_clk_get(dev, "ref_clk_parent", &phy->ref_clk_parent);
	if (err)
		goto out;

	err = msm_ufs_phy_clk_get(dev, "ref_clk", &phy->ref_clk);
	if (err)
		goto out;

	err = msm_ufs_phy_init_vreg(dev, &phy->vdda_pll, "vdda-pll");
	if (err)
		goto out;

	err = msm_ufs_phy_init_vreg(dev, &phy->vdda_phy, "vdda-phy");
	if (err)
		goto out;

	phy->dev = dev;
	dev_set_drvdata(dev, phy);

	list_add_tail(&phy->list, &phy_list);
out:
	return err;
}

static int msm_ufs_phy_remove(struct platform_device *pdev)
{
	struct msm_ufs_phy *phy = platform_get_drvdata(pdev);

	msm_ufs_phy_power_off(phy);
	list_del_init(&phy->list);
	kfree(phy->vdda_pll.name);
	kfree(phy->vdda_phy.name);

	return 0;
}

/**
 * struct ufs_hba_msm_vops - UFS MSM specific variant operations
 *
 * The variant operations configure the necessary controller and PHY
 * handshake during initializaiton.
 */
const struct ufs_hba_variant_ops ufs_hba_msm_vops = {
	.name                   = "msm",
	.init                   = msm_ufs_init,
	.exit                   = msm_ufs_exit,
	.hce_enable_notify      = msm_ufs_hce_enable_notify,
	.link_startup_notify    = msm_ufs_link_startup_notify,
	.suspend		= msm_ufs_suspend,
	.resume			= msm_ufs_resume,
};
EXPORT_SYMBOL(ufs_hba_msm_vops);

static const struct of_device_id msm_ufs_phy_of_match[] = {
	{.compatible = "qcom,ufsphy"},
	{},
};
MODULE_DEVICE_TABLE(of, msm_ufs_phy_of_match);

static struct platform_driver msm_ufs_phy_driver = {
	.probe = msm_ufs_phy_probe,
	.remove = msm_ufs_phy_remove,
	.driver = {
		.of_match_table = msm_ufs_phy_of_match,
		.name = "msm_ufs_phy",
		.owner = THIS_MODULE,
	},
};

module_platform_driver(msm_ufs_phy_driver);

MODULE_DESCRIPTION("Qualcomm Universal Flash Storage (UFS) PHY");
MODULE_LICENSE("GPL v2");
