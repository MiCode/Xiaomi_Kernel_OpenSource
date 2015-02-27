/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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
#include <dt-bindings/clock/msm-clocks-8994.h>

#include "mdss-pll.h"
#include "mdss-hdmi-pll.h"

/* hdmi phy registers */

#define HDMI_PHY_CMD_SIZE  68
#define HDMI_PHY_CLK_SIZE  97

/* Set to 1 for auto KVCO cal; set to 0 for fixed value */
#define HDMI_PHY_AUTO_KVCO_CAL    1

/* PLL REGISTERS */
#define QSERDES_COM_SYS_CLK_CTRL			(0x000)
#define QSERDES_COM_PLL_VCOTAIL_EN			(0x004)
#define QSERDES_COM_CMN_MODE				(0x008)
#define QSERDES_COM_IE_TRIM				(0x00C)
#define QSERDES_COM_IP_TRIM				(0x010)
#define QSERDES_COM_PLL_CNTRL				(0x014)
#define QSERDES_COM_PLL_PHSEL_CONTROL			(0x018)
#define QSERDES_COM_IPTAT_TRIM_VCCA_TX_SEL		(0x01C)
#define QSERDES_COM_PLL_PHSEL_DC			(0x020)
#define QSERDES_COM_PLL_IP_SETI				(0x024)
#define QSERDES_COM_CORE_CLK_IN_SYNC_SEL		(0x028)
#define QSERDES_COM_PLL_BKG_KVCO_CAL_EN			(0x02C)
#define QSERDES_COM_BIAS_EN_CLKBUFLR_EN			(0x030)
#define QSERDES_COM_PLL_CP_SETI				(0x034)
#define QSERDES_COM_PLL_IP_SETP				(0x038)
#define QSERDES_COM_PLL_CP_SETP				(0x03C)
#define QSERDES_COM_ATB_SEL1				(0x040)
#define QSERDES_COM_ATB_SEL2				(0x044)
#define QSERDES_COM_SYSCLK_EN_SEL_TXBAND		(0x048)
#define QSERDES_COM_RESETSM_CNTRL			(0x04C)
#define QSERDES_COM_RESETSM_CNTRL2			(0x050)
#define QSERDES_COM_RESETSM_CNTRL3			(0x054)
#define QSERDES_COM_RESETSM_PLL_CAL_COUNT1		(0x058)
#define QSERDES_COM_RESETSM_PLL_CAL_COUNT2		(0x05C)
#define QSERDES_COM_DIV_REF1				(0x060)
#define QSERDES_COM_DIV_REF2				(0x064)
#define QSERDES_COM_KVCO_COUNT1				(0x068)
#define QSERDES_COM_KVCO_COUNT2				(0x06C)
#define QSERDES_COM_KVCO_CAL_CNTRL			(0x070)
#define QSERDES_COM_KVCO_CODE				(0x074)
#define QSERDES_COM_VREF_CFG1				(0x078)
#define QSERDES_COM_VREF_CFG2				(0x07C)
#define QSERDES_COM_VREF_CFG3				(0x080)
#define QSERDES_COM_VREF_CFG4				(0x084)
#define QSERDES_COM_VREF_CFG5				(0x088)
#define QSERDES_COM_VREF_CFG6				(0x08C)
#define QSERDES_COM_PLLLOCK_CMP1			(0x090)
#define QSERDES_COM_PLLLOCK_CMP2			(0x094)
#define QSERDES_COM_PLLLOCK_CMP3			(0x098)
#define QSERDES_COM_PLLLOCK_CMP_EN			(0x09C)
#define QSERDES_COM_BGTC				(0x0A0)
#define QSERDES_COM_PLL_TEST_UPDN			(0x0A4)
#define QSERDES_COM_PLL_VCO_TUNE			(0x0A8)
#define QSERDES_COM_DEC_START1				(0x0AC)
#define QSERDES_COM_PLL_AMP_OS				(0x0B0)
#define QSERDES_COM_SSC_EN_CENTER			(0x0B4)
#define QSERDES_COM_SSC_ADJ_PER1			(0x0B8)
#define QSERDES_COM_SSC_ADJ_PER2			(0x0BC)
#define QSERDES_COM_SSC_PER1				(0x0C0)
#define QSERDES_COM_SSC_PER2				(0x0C4)
#define QSERDES_COM_SSC_STEP_SIZE1			(0x0C8)
#define QSERDES_COM_SSC_STEP_SIZE2			(0x0CC)
#define QSERDES_COM_RES_CODE_UP				(0x0D0)
#define QSERDES_COM_RES_CODE_DN				(0x0D4)
#define QSERDES_COM_RES_CODE_UP_OFFSET			(0x0D8)
#define QSERDES_COM_RES_CODE_DN_OFFSET			(0x0DC)
#define QSERDES_COM_RES_CODE_START_SEG1			(0x0E0)
#define QSERDES_COM_RES_CODE_START_SEG2			(0x0E4)
#define QSERDES_COM_RES_CODE_CAL_CSR			(0x0E8)
#define QSERDES_COM_RES_CODE				(0x0EC)
#define QSERDES_COM_RES_TRIM_CONTROL			(0x0F0)
#define QSERDES_COM_RES_TRIM_CONTROL2			(0x0F4)
#define QSERDES_COM_RES_TRIM_EN_VCOCALDONE		(0x0F8)
#define QSERDES_COM_FAUX_EN				(0x0FC)
#define QSERDES_COM_DIV_FRAC_START1			(0x100)
#define QSERDES_COM_DIV_FRAC_START2			(0x104)
#define QSERDES_COM_DIV_FRAC_START3			(0x108)
#define QSERDES_COM_DEC_START2				(0x10C)
#define QSERDES_COM_PLL_RXTXEPCLK_EN			(0x110)
#define QSERDES_COM_PLL_CRCTRL				(0x114)
#define QSERDES_COM_PLL_CLKEPDIV			(0x118)
#define QSERDES_COM_PLL_FREQUPDATE			(0x11C)
#define QSERDES_COM_PLL_BKGCAL_TRIM_UP			(0x120)
#define QSERDES_COM_PLL_BKGCAL_TRIM_DN			(0x124)
#define QSERDES_COM_PLL_BKGCAL_TRIM_MUX			(0x128)
#define QSERDES_COM_PLL_BKGCAL_VREF_CFG			(0x12C)
#define QSERDES_COM_PLL_BKGCAL_DIV_REF1			(0x130)
#define QSERDES_COM_PLL_BKGCAL_DIV_REF2			(0x134)
#define QSERDES_COM_MUXADDR				(0x138)
#define QSERDES_COM_LOW_POWER_RO_CONTROL		(0x13C)
#define QSERDES_COM_POST_DIVIDER_CONTROL		(0x140)
#define QSERDES_COM_HR_OCLK2_DIVIDER			(0x144)
#define QSERDES_COM_HR_OCLK3_DIVIDER			(0x148)
#define QSERDES_COM_PLL_VCO_HIGH			(0x14C)
#define QSERDES_COM_RESET_SM				(0x150)
#define QSERDES_COM_MUXVAL				(0x154)
#define QSERDES_COM_CORE_RES_CODE_DN			(0x158)
#define QSERDES_COM_CORE_RES_CODE_UP			(0x15C)
#define QSERDES_COM_CORE_VCO_TUNE			(0x160)
#define QSERDES_COM_CORE_VCO_TAIL			(0x164)
#define QSERDES_COM_CORE_KVCO_CODE			(0x168)

/* Tx Channel 0 REGISTERS */
#define QSERDES_TX_L0_BIST_MODE_LANENO			(0x00)
#define QSERDES_TX_L0_CLKBUF_ENABLE			(0x04)
#define QSERDES_TX_L0_TX_EMP_POST1_LVL			(0x08)
#define QSERDES_TX_L0_TX_DRV_LVL			(0x0C)
#define QSERDES_TX_L0_RESET_TSYNC_EN			(0x10)
#define QSERDES_TX_L0_LPB_EN				(0x14)
#define QSERDES_TX_L0_RES_CODE_UP			(0x18)
#define QSERDES_TX_L0_RES_CODE_DN			(0x1C)
#define QSERDES_TX_L0_PERL_LENGTH1			(0x20)
#define QSERDES_TX_L0_PERL_LENGTH2			(0x24)
#define QSERDES_TX_L0_SERDES_BYP_EN_OUT			(0x28)
#define QSERDES_TX_L0_HIGHZ_TRANSCEIVEREN_BIAS_DRVR_EN	(0x2C)
#define QSERDES_TX_L0_PARRATE_REC_DETECT_IDLE_EN	(0x30)
#define QSERDES_TX_L0_BIST_PATTERN1			(0x34)
#define QSERDES_TX_L0_BIST_PATTERN2			(0x38)
#define QSERDES_TX_L0_BIST_PATTERN3			(0x3C)
#define QSERDES_TX_L0_BIST_PATTERN4			(0x40)
#define QSERDES_TX_L0_BIST_PATTERN5			(0x44)
#define QSERDES_TX_L0_BIST_PATTERN6			(0x48)
#define QSERDES_TX_L0_BIST_PATTERN7			(0x4C)
#define QSERDES_TX_L0_BIST_PATTERN8			(0x50)
#define QSERDES_TX_L0_LANE_MODE				(0x54)
#define QSERDES_TX_L0_IDAC_CAL_LANE_MODE		(0x58)
#define QSERDES_TX_L0_IDAC_CAL_LANE_MODE_CONFIGURATION	(0x5C)
#define QSERDES_TX_L0_ATB_SEL1				(0x60)
#define QSERDES_TX_L0_ATB_SEL2				(0x64)
#define QSERDES_TX_L0_RCV_DETECT_LVL			(0x68)
#define QSERDES_TX_L0_PRBS_SEED1			(0x6C)
#define QSERDES_TX_L0_PRBS_SEED2			(0x70)
#define QSERDES_TX_L0_PRBS_SEED3			(0x74)
#define QSERDES_TX_L0_PRBS_SEED4			(0x78)
#define QSERDES_TX_L0_RESET_GEN				(0x7C)
#define QSERDES_TX_L0_TRAN_DRVR_EMP_EN			(0x80)
#define QSERDES_TX_L0_TX_INTERFACE_MODE			(0x84)
#define QSERDES_TX_L0_PWM_CTRL				(0x88)
#define QSERDES_TX_L0_PWM_DATA				(0x8C)
#define QSERDES_TX_L0_PWM_ENC_DIV_CTRL			(0x90)
#define QSERDES_TX_L0_VMODE_CTRL1			(0x94)
#define QSERDES_TX_L0_VMODE_CTRL2			(0x98)
#define QSERDES_TX_L0_VMODE_CTRL3			(0x9C)
#define QSERDES_TX_L0_VMODE_CTRL4			(0xA0)
#define QSERDES_TX_L0_VMODE_CTRL5			(0xA4)
#define QSERDES_TX_L0_VMODE_CTRL6			(0xA8)
#define QSERDES_TX_L0_VMODE_CTRL7			(0xAC)
#define QSERDES_TX_L0_TX_ALOG_INTF_OBSV_CNTL		(0xB0)
#define QSERDES_TX_L0_BIST_STATUS			(0xB4)
#define QSERDES_TX_L0_BIST_ERROR_COUNT1			(0xB8)
#define QSERDES_TX_L0_BIST_ERROR_COUNT2			(0xBC)
#define QSERDES_TX_L0_TX_ALOG_INTF_OBSV			(0xC0)
#define QSERDES_TX_L0_PWM_DEC_STATUS			(0xC4)

/* Tx Channel 1 REGISTERS */
#define QSERDES_TX_L1_BIST_MODE_LANENO			(0x00)
#define QSERDES_TX_L1_CLKBUF_ENABLE			(0x04)
#define QSERDES_TX_L1_TX_EMP_POST1_LVL			(0x08)
#define QSERDES_TX_L1_TX_DRV_LVL			(0x0C)
#define QSERDES_TX_L1_RESET_TSYNC_EN			(0x10)
#define QSERDES_TX_L1_LPB_EN				(0x14)
#define QSERDES_TX_L1_RES_CODE_UP			(0x18)
#define QSERDES_TX_L1_RES_CODE_DN			(0x1C)
#define QSERDES_TX_L1_PERL_LENGTH1			(0x20)
#define QSERDES_TX_L1_PERL_LENGTH2			(0x24)
#define QSERDES_TX_L1_SERDES_BYP_EN_OUT			(0x28)
#define QSERDES_TX_L1_HIGHZ_TRANSCEIVEREN_BIAS_DRVR_EN	(0x2C)
#define QSERDES_TX_L1_PARRATE_REC_DETECT_IDLE_EN	(0x30)
#define QSERDES_TX_L1_BIST_PATTERN1			(0x34)
#define QSERDES_TX_L1_BIST_PATTERN2			(0x38)
#define QSERDES_TX_L1_BIST_PATTERN3			(0x3C)
#define QSERDES_TX_L1_BIST_PATTERN4			(0x40)
#define QSERDES_TX_L1_BIST_PATTERN5			(0x44)
#define QSERDES_TX_L1_BIST_PATTERN6			(0x48)
#define QSERDES_TX_L1_BIST_PATTERN7			(0x4C)
#define QSERDES_TX_L1_BIST_PATTERN8			(0x50)
#define QSERDES_TX_L1_LANE_MODE				(0x54)
#define QSERDES_TX_L1_IDAC_CAL_LANE_MODE		(0x58)
#define QSERDES_TX_L1_IDAC_CAL_LANE_MODE_CONFIGURATION	(0x5C)
#define QSERDES_TX_L1_ATB_SEL1				(0x60)
#define QSERDES_TX_L1_ATB_SEL2				(0x64)
#define QSERDES_TX_L1_RCV_DETECT_LVL			(0x68)
#define QSERDES_TX_L1_PRBS_SEED1			(0x6C)
#define QSERDES_TX_L1_PRBS_SEED2			(0x70)
#define QSERDES_TX_L1_PRBS_SEED3			(0x74)
#define QSERDES_TX_L1_PRBS_SEED4			(0x78)
#define QSERDES_TX_L1_RESET_GEN				(0x7C)
#define QSERDES_TX_L1_TRAN_DRVR_EMP_EN			(0x80)
#define QSERDES_TX_L1_TX_INTERFACE_MODE			(0x84)
#define QSERDES_TX_L1_PWM_CTRL				(0x88)
#define QSERDES_TX_L1_PWM_DATA				(0x8C)
#define QSERDES_TX_L1_PWM_ENC_DIV_CTRL			(0x90)
#define QSERDES_TX_L1_VMODE_CTRL1			(0x94)
#define QSERDES_TX_L1_VMODE_CTRL2			(0x98)
#define QSERDES_TX_L1_VMODE_CTRL3			(0x9C)
#define QSERDES_TX_L1_VMODE_CTRL4			(0xA0)
#define QSERDES_TX_L1_VMODE_CTRL5			(0xA4)
#define QSERDES_TX_L1_VMODE_CTRL6			(0xA8)
#define QSERDES_TX_L1_VMODE_CTRL7			(0xAC)
#define QSERDES_TX_L1_TX_ALOG_INTF_OBSV_CNTL		(0xB0)
#define QSERDES_TX_L1_BIST_STATUS			(0xB4)
#define QSERDES_TX_L1_BIST_ERROR_COUNT1			(0xB8)
#define QSERDES_TX_L1_BIST_ERROR_COUNT2			(0xBC)
#define QSERDES_TX_L1_TX_ALOG_INTF_OBSV			(0xC0)
#define QSERDES_TX_L1_PWM_DEC_STATUS			(0xC4)

/* Tx Channel 2 REGISERS */
#define QSERDES_TX_L2_BIST_MODE_LANENO			(0x00)
#define QSERDES_TX_L2_CLKBUF_ENABLE			(0x04)
#define QSERDES_TX_L2_TX_EMP_POST1_LVL			(0x08)
#define QSERDES_TX_L2_TX_DRV_LVL			(0x0C)
#define QSERDES_TX_L2_RESET_TSYNC_EN			(0x10)
#define QSERDES_TX_L2_LPB_EN				(0x14)
#define QSERDES_TX_L2_RES_CODE_UP			(0x18)
#define QSERDES_TX_L2_RES_CODE_DN			(0x1C)
#define QSERDES_TX_L2_PERL_LENGTH1			(0x20)
#define QSERDES_TX_L2_PERL_LENGTH2			(0x24)
#define QSERDES_TX_L2_SERDES_BYP_EN_OUT			(0x28)
#define QSERDES_TX_L2_HIGHZ_TRANSCEIVEREN_BIAS_DRVR_EN	(0x2C)
#define QSERDES_TX_L2_PARRATE_REC_DETECT_IDLE_EN	(0x30)
#define QSERDES_TX_L2_BIST_PATTERN1			(0x34)
#define QSERDES_TX_L2_BIST_PATTERN2			(0x38)
#define QSERDES_TX_L2_BIST_PATTERN3			(0x3C)
#define QSERDES_TX_L2_BIST_PATTERN4			(0x40)
#define QSERDES_TX_L2_BIST_PATTERN5			(0x44)
#define QSERDES_TX_L2_BIST_PATTERN6			(0x48)
#define QSERDES_TX_L2_BIST_PATTERN7			(0x4C)
#define QSERDES_TX_L2_BIST_PATTERN8			(0x50)
#define QSERDES_TX_L2_LANE_MODE				(0x54)
#define QSERDES_TX_L2_IDAC_CAL_LANE_MODE		(0x58)
#define QSERDES_TX_L2_IDAC_CAL_LANE_MODE_CONFIGURATION	(0x5C)
#define QSERDES_TX_L2_ATB_SEL1				(0x60)
#define QSERDES_TX_L2_ATB_SEL2				(0x64)
#define QSERDES_TX_L2_RCV_DETECT_LVL			(0x68)
#define QSERDES_TX_L2_PRBS_SEED1			(0x6C)
#define QSERDES_TX_L2_PRBS_SEED2			(0x70)
#define QSERDES_TX_L2_PRBS_SEED3			(0x74)
#define QSERDES_TX_L2_PRBS_SEED4			(0x78)
#define QSERDES_TX_L2_RESET_GEN				(0x7C)
#define QSERDES_TX_L2_TRAN_DRVR_EMP_EN			(0x80)
#define QSERDES_TX_L2_TX_INTERFACE_MODE			(0x84)
#define QSERDES_TX_L2_PWM_CTRL				(0x88)
#define QSERDES_TX_L2_PWM_DATA				(0x8C)
#define QSERDES_TX_L2_PWM_ENC_DIV_CTRL			(0x90)
#define QSERDES_TX_L2_VMODE_CTRL1			(0x94)
#define QSERDES_TX_L2_VMODE_CTRL2			(0x98)
#define QSERDES_TX_L2_VMODE_CTRL3			(0x9C)
#define QSERDES_TX_L2_VMODE_CTRL4			(0xA0)
#define QSERDES_TX_L2_VMODE_CTRL5			(0xA4)
#define QSERDES_TX_L2_VMODE_CTRL6			(0xA8)
#define QSERDES_TX_L2_VMODE_CTRL7			(0xAC)
#define QSERDES_TX_L2_TX_ALOG_INTF_OBSV_CNTL		(0xB0)
#define QSERDES_TX_L2_BIST_STATUS			(0xB4)
#define QSERDES_TX_L2_BIST_ERROR_COUNT1			(0xB8)
#define QSERDES_TX_L2_BIST_ERROR_COUNT2			(0xBC)
#define QSERDES_TX_L2_TX_ALOG_INTF_OBSV			(0xC0)
#define QSERDES_TX_L2_PWM_DEC_STATUS			(0xC4)

/* Tx Channel 3 REGISERS */
#define QSERDES_TX_L3_BIST_MODE_LANENO			(0x00)
#define QSERDES_TX_L3_CLKBUF_ENABLE			(0x04)
#define QSERDES_TX_L3_TX_EMP_POST1_LVL			(0x08)
#define QSERDES_TX_L3_TX_DRV_LVL			(0x0C)
#define QSERDES_TX_L3_RESET_TSYNC_EN			(0x10)
#define QSERDES_TX_L3_LPB_EN				(0x14)
#define QSERDES_TX_L3_RES_CODE_UP			(0x18)
#define QSERDES_TX_L3_RES_CODE_DN			(0x1C)
#define QSERDES_TX_L3_PERL_LENGTH1			(0x20)
#define QSERDES_TX_L3_PERL_LENGTH2			(0x24)
#define QSERDES_TX_L3_SERDES_BYP_EN_OUT			(0x28)
#define QSERDES_TX_L3_HIGHZ_TRANSCEIVEREN_BIAS_DRVR_EN	(0x2C)
#define QSERDES_TX_L3_PARRATE_REC_DETECT_IDLE_EN	(0x30)
#define QSERDES_TX_L3_BIST_PATTERN1			(0x34)
#define QSERDES_TX_L3_BIST_PATTERN2			(0x38)
#define QSERDES_TX_L3_BIST_PATTERN3			(0x3C)
#define QSERDES_TX_L3_BIST_PATTERN4			(0x40)
#define QSERDES_TX_L3_BIST_PATTERN5			(0x44)
#define QSERDES_TX_L3_BIST_PATTERN6			(0x48)
#define QSERDES_TX_L3_BIST_PATTERN7			(0x4C)
#define QSERDES_TX_L3_BIST_PATTERN8			(0x50)
#define QSERDES_TX_L3_LANE_MODE				(0x54)
#define QSERDES_TX_L3_IDAC_CAL_LANE_MODE		(0x58)
#define QSERDES_TX_L3_IDAC_CAL_LANE_MODE_CONFIGURATION	(0x5C)
#define QSERDES_TX_L3_ATB_SEL1				(0x60)
#define QSERDES_TX_L3_ATB_SEL2				(0x64)
#define QSERDES_TX_L3_RCV_DETECT_LVL			(0x68)
#define QSERDES_TX_L3_PRBS_SEED1			(0x6C)
#define QSERDES_TX_L3_PRBS_SEED2			(0x70)
#define QSERDES_TX_L3_PRBS_SEED3			(0x74)
#define QSERDES_TX_L3_PRBS_SEED4			(0x78)
#define QSERDES_TX_L3_RESET_GEN				(0x7C)
#define QSERDES_TX_L3_TRAN_DRVR_EMP_EN			(0x80)
#define QSERDES_TX_L3_TX_INTERFACE_MODE			(0x84)
#define QSERDES_TX_L3_PWM_CTRL				(0x88)
#define QSERDES_TX_L3_PWM_DATA				(0x8C)
#define QSERDES_TX_L3_PWM_ENC_DIV_CTRL			(0x90)
#define QSERDES_TX_L3_VMODE_CTRL1			(0x94)
#define QSERDES_TX_L3_VMODE_CTRL2			(0x98)
#define QSERDES_TX_L3_VMODE_CTRL3			(0x9C)
#define QSERDES_TX_L3_VMODE_CTRL4			(0xA0)
#define QSERDES_TX_L3_VMODE_CTRL5			(0xA4)
#define QSERDES_TX_L3_VMODE_CTRL6			(0xA8)
#define QSERDES_TX_L3_VMODE_CTRL7			(0xAC)
#define QSERDES_TX_L3_TX_ALOG_INTF_OBSV_CNTL		(0xB0)
#define QSERDES_TX_L3_BIST_STATUS			(0xB4)
#define QSERDES_TX_L3_BIST_ERROR_COUNT1			(0xB8)
#define QSERDES_TX_L3_BIST_ERROR_COUNT2			(0xBC)
#define QSERDES_TX_L3_TX_ALOG_INTF_OBSV			(0xC0)
#define QSERDES_TX_L3_PWM_DEC_STATUS			(0xC4)

/* HDMI PHY REGISTERS */
#define HDMI_PHY_CFG					(0x00)
#define HDMI_PHY_PD_CTL					(0x04)
#define HDMI_PHY_MODE					(0x08)
#define HDMI_PHY_MISR_CLEAR				(0x0C)
#define HDMI_PHY_TX0_TX1_BIST_CFG0			(0x10)
#define HDMI_PHY_TX0_TX1_BIST_CFG1			(0x14)
#define HDMI_PHY_TX0_TX1_PRBS_SEED_BYTE0		(0x18)
#define HDMI_PHY_TX0_TX1_PRBS_SEED_BYTE1		(0x1C)
#define HDMI_PHY_TX0_TX1_PRBS_SEED_BYTE2		(0x20)
#define HDMI_PHY_TX0_TX1_PRBS_SEED_BYTE3		(0x24)
#define HDMI_PHY_TX0_TX1_PRBS_POLY_BYTE0		(0x28)
#define HDMI_PHY_TX0_TX1_PRBS_POLY_BYTE1		(0x2C)
#define HDMI_PHY_TX0_TX1_PRBS_POLY_BYTE2		(0x30)
#define HDMI_PHY_TX0_TX1_PRBS_POLY_BYTE3		(0x34)
#define HDMI_PHY_TX2_TX3_BIST_CFG0			(0x38)
#define HDMI_PHY_TX2_TX3_BIST_CFG1			(0x3C)
#define HDMI_PHY_TX2_TX3_PRBS_SEED_BYTE0		(0x40)
#define HDMI_PHY_TX2_TX3_PRBS_SEED_BYTE1		(0x44)
#define HDMI_PHY_TX2_TX3_PRBS_SEED_BYTE2		(0x48)
#define HDMI_PHY_TX2_TX3_PRBS_SEED_BYTE3		(0x4C)
#define HDMI_PHY_TX2_TX3_PRBS_POLY_BYTE0		(0x50)
#define HDMI_PHY_TX2_TX3_PRBS_POLY_BYTE1		(0x54)
#define HDMI_PHY_TX2_TX3_PRBS_POLY_BYTE2		(0x58)
#define HDMI_PHY_TX2_TX3_PRBS_POLY_BYTE3		(0x5C)
#define HDMI_PHY_DEBUG_BUS_SEL				(0x60)
#define HDMI_PHY_TXCAL_CFG0				(0x64)
#define HDMI_PHY_TXCAL_CFG1				(0x68)
#define HDMI_PHY_TX0_TX1_BIST_STATUS0			(0x6C)
#define HDMI_PHY_TX0_TX1_BIST_STATUS1			(0x70)
#define HDMI_PHY_TX0_TX1_BIST_STATUS2			(0x74)
#define HDMI_PHY_TX2_TX3_BIST_STATUS0			(0x78)
#define HDMI_PHY_TX2_TX3_BIST_STATUS1			(0x7C)
#define HDMI_PHY_TX2_TX3_BIST_STATUS2			(0x80)
#define HDMI_PHY_PRE_MISR_STATUS0			(0x84)
#define HDMI_PHY_PRE_MISR_STATUS1			(0x88)
#define HDMI_PHY_PRE_MISR_STATUS2			(0x8C)
#define HDMI_PHY_PRE_MISR_STATUS3			(0x90)
#define HDMI_PHY_POST_MISR_STATUS0			(0x94)
#define HDMI_PHY_POST_MISR_STATUS1			(0x98)
#define HDMI_PHY_POST_MISR_STATUS2			(0x9C)
#define HDMI_PHY_POST_MISR_STATUS3			(0xA0)
#define HDMI_PHY_STATUS					(0xA4)
#define HDMI_PHY_MISC3_STATUS				(0xA8)
#define HDMI_PHY_DEBUG_BUS0				(0xAC)
#define HDMI_PHY_DEBUG_BUS1				(0xB0)
#define HDMI_PHY_DEBUG_BUS2				(0xB4)
#define HDMI_PHY_DEBUG_BUS3				(0xB8)
#define HDMI_PHY_REVISION_ID0				(0xBC)
#define HDMI_PHY_REVISION_ID1				(0xC0)
#define HDMI_PHY_REVISION_ID2				(0xC4)
#define HDMI_PHY_REVISION_ID3				(0xC8)

#define HDMI_PLL_POLL_MAX_READS			2500
#define HDMI_PLL_POLL_TIMEOUT_US		50
#define HDMI_PLL_REF_CLK_RATE			192ULL
#define HDMI_PLL_DIVISOR			10000000000ULL
#define HDMI_PLL_DIVISOR_32			100000U
#define HDMI_PLL_MIN_VCO_CLK			160000000ULL
#define HDMI_PLL_TMDS_MAX			800000000U


static int hdmi_20nm_pll_lock_status(struct mdss_pll_resources *io)
{
	u32 status;
	int pll_locked = 0;
	int phy_ready = 0;
	int rc;

	rc = mdss_pll_resource_enable(io, true);
	if (rc) {
		pr_err("pll resource can't be enabled\n");
		return rc;
	}

	/* Poll for C_READY and PHY READY */
	pr_debug("%s: Waiting for PHY Ready\n", __func__);

	/* poll for PLL ready status */
	if (!readl_poll_timeout_noirq(
		(io->pll_base + QSERDES_COM_RESET_SM),
		status, status & BIT(6),
		HDMI_PLL_POLL_MAX_READS,
		HDMI_PLL_POLL_TIMEOUT_US)) {
		pr_debug("%s: C READY\n", __func__);
		pll_locked = 1;
	} else {
		pr_debug("%s: C READY TIMEOUT\n", __func__);
		pll_locked = 0;
	}

	/* poll for PHY ready status */
	if (pll_locked && !readl_poll_timeout_noirq(
		(io->phy_base + HDMI_PHY_STATUS),
		status, status & BIT(0),
		HDMI_PLL_POLL_MAX_READS,
		HDMI_PLL_POLL_TIMEOUT_US)) {
		pr_debug("%s: PHY READY\n", __func__);
		phy_ready = 1;
	} else {
		pr_debug("%s: PHY READY TIMEOUT\n", __func__);
		phy_ready = 0;
	}
	mdss_pll_resource_enable(io, false);

	return phy_ready;
}

static inline struct hdmi_pll_vco_clk *to_hdmi_20nm_vco_clk(struct clk *clk)
{
	return container_of(clk, struct hdmi_pll_vco_clk, c);
}

static void hdmi_20nm_phy_pll_calc_settings(struct mdss_pll_resources *io,
			struct hdmi_pll_vco_clk *vco, u32 vco_clk, u32 tmds_clk)
{
	u64 dec_start_val, frac_start_val, pll_lock_cmp;

	/* Calculate decimal and fractional values */
	dec_start_val = 1000000UL * vco_clk;
	do_div(dec_start_val, HDMI_PLL_REF_CLK_RATE);
	do_div(dec_start_val, 2U);
	frac_start_val = dec_start_val;
	do_div(frac_start_val, HDMI_PLL_DIVISOR_32);
	do_div(frac_start_val, HDMI_PLL_DIVISOR_32);
	frac_start_val *= HDMI_PLL_DIVISOR;
	frac_start_val = dec_start_val - frac_start_val;
	frac_start_val *= (u64)(2 << 19);
	do_div(frac_start_val, HDMI_PLL_DIVISOR_32);
	do_div(frac_start_val, HDMI_PLL_DIVISOR_32);
	pll_lock_cmp = dec_start_val;
	do_div(pll_lock_cmp, 10U);
	pll_lock_cmp *= 0x800;
	do_div(pll_lock_cmp, HDMI_PLL_DIVISOR_32);
	do_div(pll_lock_cmp, HDMI_PLL_DIVISOR_32);
	pll_lock_cmp -= 1U;
	do_div(dec_start_val, HDMI_PLL_DIVISOR_32);
	do_div(dec_start_val, HDMI_PLL_DIVISOR_32);

	/* PLL calibration */
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_DIV_FRAC_START1,
		0x80 | (frac_start_val & 0x7F));
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_DIV_FRAC_START2,
		0x80 | ((frac_start_val >> 7) & 0x7F));
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_DIV_FRAC_START3,
		0x40 | ((frac_start_val >> 14) & 0x3F));
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_DEC_START1,
		0x80 | (dec_start_val & 0x7F));
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_DEC_START2,
		0x02 | (0x01 & dec_start_val >> 7));
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_PLLLOCK_CMP1,
		pll_lock_cmp & 0xFF);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_PLLLOCK_CMP2,
		(pll_lock_cmp >> 8) & 0xFF);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_PLLLOCK_CMP3,
		(pll_lock_cmp >> 16) & 0xFF);
}

static u32 hdmi_20nm_phy_pll_set_clk_rate(struct clk *c, u32 tmds_clk)
{
	u32 tx_band = 0;

	struct hdmi_pll_vco_clk *vco = to_hdmi_20nm_vco_clk(c);
	struct mdss_pll_resources *io = vco->priv;
	u64 vco_clk = tmds_clk;

	while (vco_clk > 0 && vco_clk < HDMI_PLL_MIN_VCO_CLK) {
		tx_band++;
		vco_clk *= 2;
	}

	/* Initially shut down PHY */
	pr_debug("%s: Disabling PHY\n", __func__);
	MDSS_PLL_REG_W(io->phy_base, HDMI_PHY_PD_CTL, 0x0);
	udelay(1000);
	mb();

	/* power-up and recommended common block settings */
	MDSS_PLL_REG_W(io->phy_base, HDMI_PHY_PD_CTL, 0x1F);
	MDSS_PLL_REG_W(io->phy_base, HDMI_PHY_CFG, 0x01);
	udelay(1000);
	mb();

	MDSS_PLL_REG_W(io->phy_base, HDMI_PHY_CFG, 0x07);
	udelay(1000);
	mb();

	MDSS_PLL_REG_W(io->phy_base, HDMI_PHY_CFG, 0x05);
	udelay(1000);
	mb();

	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_SYS_CLK_CTRL, 0x42);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_PLL_VCOTAIL_EN, 0x03);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_CMN_MODE, 0x00);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_IE_TRIM, 0x00);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_IP_TRIM, 0x00);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_PLL_CNTRL, 0x01);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_PLL_PHSEL_CONTROL, 0x04);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_IPTAT_TRIM_VCCA_TX_SEL,
		tmds_clk > 148500000 ? 0x80 : 0xC0);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_PLL_PHSEL_DC, 0x00);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_CORE_CLK_IN_SYNC_SEL, 0x00);

	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_PLL_BKG_KVCO_CAL_EN, 0x00);

	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_BIAS_EN_CLKBUFLR_EN, 0x0F);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_ATB_SEL1, 0x00);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_ATB_SEL2, 0x00);

	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_SYSCLK_EN_SEL_TXBAND,
		0x4A + (0x10 * tx_band));

	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_KVCO_CODE, 0x30);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_BGTC, 0x0F);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_PLL_TEST_UPDN, 0x00);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_PLL_VCO_TUNE, 0x00);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_PLL_AMP_OS, 0x00);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_SSC_EN_CENTER, 0x00);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_RES_CODE_UP, 0x00);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_RES_CODE_DN, 0x00);

	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_RES_CODE_CAL_CSR, 0x77);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_RES_TRIM_EN_VCOCALDONE, 0x00);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_FAUX_EN, 0x00);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_PLL_RXTXEPCLK_EN, 0x0E);

	/* PLL loop bandwidth */
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_PLL_IP_SETI, 0x01);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_PLL_CP_SETI, 0x3F);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_PLL_IP_SETP, 0x06);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_PLL_CP_SETP, 0x1F);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_PLL_CRCTRL, 0xBB);

	hdmi_20nm_phy_pll_calc_settings(io, vco, vco_clk, tmds_clk);

	/* PLL calibration */
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_PLLLOCK_CMP_EN, 0x01);

	/* Resistor calibration linear search */
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_RES_CODE_START_SEG1, 0x60);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_RES_CODE_START_SEG2, 0x60);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_RES_TRIM_CONTROL, 0x01);

	/* Reset state machine control */
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_RESETSM_CNTRL, 0x80);
	MDSS_PLL_REG_W(io->pll_base, QSERDES_COM_RESETSM_CNTRL2, 0x07);

	udelay(1000);
	mb();

	/* TX lanes (transceivers) power-up sequence */
	MDSS_PLL_REG_W(io->phy_base, HDMI_PHY_MODE, tx_band);

	MDSS_PLL_REG_W(io->pll_base + 0x400, QSERDES_TX_L0_CLKBUF_ENABLE, 0x03);
	MDSS_PLL_REG_W(io->pll_base + 0x600, QSERDES_TX_L1_CLKBUF_ENABLE, 0x03);
	MDSS_PLL_REG_W(io->pll_base + 0x800, QSERDES_TX_L2_CLKBUF_ENABLE, 0x03);
	MDSS_PLL_REG_W(io->pll_base + 0xA00, QSERDES_TX_L3_CLKBUF_ENABLE, 0x03);

	MDSS_PLL_REG_W(io->pll_base + 0x400,
		QSERDES_TX_L0_TRAN_DRVR_EMP_EN, 0x03);
	MDSS_PLL_REG_W(io->pll_base + 0x600,
		QSERDES_TX_L1_TRAN_DRVR_EMP_EN, 0x03);
	MDSS_PLL_REG_W(io->pll_base + 0x800,
		QSERDES_TX_L2_TRAN_DRVR_EMP_EN, 0x03);
	MDSS_PLL_REG_W(io->pll_base + 0xA00,
		QSERDES_TX_L3_TRAN_DRVR_EMP_EN, 0x03);

	MDSS_PLL_REG_W(io->pll_base + 0x400,
		QSERDES_TX_L0_HIGHZ_TRANSCEIVEREN_BIAS_DRVR_EN    , 0x6F);
	MDSS_PLL_REG_W(io->pll_base + 0x600,
		QSERDES_TX_L1_HIGHZ_TRANSCEIVEREN_BIAS_DRVR_EN    , 0x6F);
	MDSS_PLL_REG_W(io->pll_base + 0x800,
		QSERDES_TX_L2_HIGHZ_TRANSCEIVEREN_BIAS_DRVR_EN    , 0x6F);
	MDSS_PLL_REG_W(io->pll_base + 0xA00,
		QSERDES_TX_L3_HIGHZ_TRANSCEIVEREN_BIAS_DRVR_EN    , 0x6F);

	MDSS_PLL_REG_W(io->pll_base + 0x400, QSERDES_TX_L0_TX_EMP_POST1_LVL,
		tmds_clk >= 74000000 ? 0x2F : 0x21);
	MDSS_PLL_REG_W(io->phy_base, HDMI_PHY_TXCAL_CFG0,
		tmds_clk >= 74000000 ? 0xAF : 0xA1);

	MDSS_PLL_REG_W(io->pll_base + 0x400, QSERDES_TX_L0_VMODE_CTRL1,
		tmds_clk >= 74000000 ? 0x0C : 0x04);
	MDSS_PLL_REG_W(io->pll_base + 0x800, QSERDES_TX_L2_VMODE_CTRL1,
		tmds_clk >= 74000000 ? 0x0D : 0x05);
	MDSS_PLL_REG_W(io->pll_base + 0x400, QSERDES_TX_L0_TX_DRV_LVL,
		tmds_clk >= 74000000 ? 0x1F : 0x11);
	MDSS_PLL_REG_W(io->pll_base + 0x800, QSERDES_TX_L2_TX_DRV_LVL,
		tmds_clk >= 74000000 ? 0x1F : 0x11);
	MDSS_PLL_REG_W(io->pll_base + 0x400, QSERDES_TX_L0_VMODE_CTRL2, 0x80);
	MDSS_PLL_REG_W(io->pll_base + 0x800, QSERDES_TX_L2_VMODE_CTRL2, 0x00);
	MDSS_PLL_REG_W(io->pll_base + 0x400, QSERDES_TX_L0_VMODE_CTRL3,
		tmds_clk >= 74000000 ? 0x01 : 0x02);
	MDSS_PLL_REG_W(io->pll_base + 0x800, QSERDES_TX_L2_VMODE_CTRL3,
		tmds_clk >= 74000000 ? 0x00 : 0x02);
	MDSS_PLL_REG_W(io->pll_base + 0x400, QSERDES_TX_L0_VMODE_CTRL5,
		tmds_clk >= 74000000 ? 0x00 : 0xA0);
	MDSS_PLL_REG_W(io->pll_base + 0x800, QSERDES_TX_L2_VMODE_CTRL5,
		tmds_clk >= 74000000 ? 0x00 : 0xA0);
	MDSS_PLL_REG_W(io->pll_base + 0x400, QSERDES_TX_L0_VMODE_CTRL6, 0x00);
	MDSS_PLL_REG_W(io->pll_base + 0x800, QSERDES_TX_L2_VMODE_CTRL6, 0x00);

	MDSS_PLL_REG_W(io->pll_base + 0x400,
		QSERDES_TX_L0_PARRATE_REC_DETECT_IDLE_EN, 0x40);
	MDSS_PLL_REG_W(io->pll_base + 0x400,
		QSERDES_TX_L0_TX_INTERFACE_MODE, 0x00);
	MDSS_PLL_REG_W(io->pll_base + 0x600,
		QSERDES_TX_L1_PARRATE_REC_DETECT_IDLE_EN, 0x40);
	MDSS_PLL_REG_W(io->pll_base + 0x600,
		QSERDES_TX_L1_TX_INTERFACE_MODE, 0x00);
	MDSS_PLL_REG_W(io->pll_base + 0x800,
		QSERDES_TX_L2_PARRATE_REC_DETECT_IDLE_EN, 0x40);
	MDSS_PLL_REG_W(io->pll_base + 0x800,
		QSERDES_TX_L2_TX_INTERFACE_MODE, 0x00);
	MDSS_PLL_REG_W(io->pll_base + 0xA00,
		QSERDES_TX_L3_PARRATE_REC_DETECT_IDLE_EN, 0x40);
	MDSS_PLL_REG_W(io->pll_base + 0xA00,
		QSERDES_TX_L3_TX_INTERFACE_MODE, 0x00);

	return 0;
}

static int hdmi_20nm_vco_enable(struct clk *c)
{
	u32 ready_poll;
	u32 time_out_loop;
	/* Hardware recommended timeout iterator */
	u32 time_out_max = 50000;

	struct hdmi_pll_vco_clk *vco = to_hdmi_20nm_vco_clk(c);
	struct mdss_pll_resources *io = vco->priv;

	MDSS_PLL_REG_W(io->phy_base, HDMI_PHY_CFG, 0x00000001);
	udelay(100);
	mb();
	MDSS_PLL_REG_W(io->phy_base, HDMI_PHY_CFG, 0x00000003);
	udelay(100);
	mb();
	MDSS_PLL_REG_W(io->phy_base, HDMI_PHY_CFG, 0x00000009);
	udelay(100);
	mb();

	/* Poll for C_READY and PHY READY */
	pr_debug("%s: Waiting for PHY Ready\n", __func__);
	time_out_loop = 0;
	do {
		ready_poll = MDSS_PLL_REG_R(io->pll_base, QSERDES_COM_RESET_SM);
		time_out_loop++;
		udelay(10);
	} while (((ready_poll  & (1 << 6)) == 0) &&
		(time_out_loop < time_out_max));
	if (time_out_loop >= time_out_max)
		pr_err("%s: ERROR: TIMED OUT BEFORE C READY\n", __func__);
	else
		pr_debug("%s: C READY\n", __func__);

	/* Poll for PHY READY */
	pr_debug("%s: Waiting for PHY Ready\n", __func__);
	time_out_loop = 0;
	do {
		ready_poll = MDSS_PLL_REG_R(io->phy_base, HDMI_PHY_STATUS);
		time_out_loop++;
		udelay(1);
	} while (((ready_poll & 0x1) == 0) && (time_out_loop < time_out_max));

	if (time_out_loop >= time_out_max)
		pr_err("%s: TIMED OUT BEFORE PHY READY\n", __func__);
	else
		pr_debug("%s: HDMI PHY READY\n", __func__);

	MDSS_PLL_REG_W(io->phy_base, HDMI_PHY_CFG, 0x00000008);
	udelay(100);

	MDSS_PLL_REG_W(io->phy_base, HDMI_PHY_CFG, 0x00000009);
	udelay(100);

	io->pll_on = true;

	return 0;
}


static int hdmi_20nm_vco_set_rate(struct clk *c, unsigned long rate)
{
	struct hdmi_pll_vco_clk *vco = to_hdmi_20nm_vco_clk(c);
	struct mdss_pll_resources *io = vco->priv;
	void __iomem		*pll_base;
	void __iomem		*phy_base;
	unsigned int set_power_dwn = 0;
	int rc;

	rc = mdss_pll_resource_enable(io, true);
	if (rc) {
		pr_err("pll resource can't be enabled\n");
		return rc;
	}

	if (io->pll_on)
		set_power_dwn = 1;

	pll_base = io->pll_base;
	phy_base = io->phy_base;

	pr_debug("rate=%ld\n", rate);

	hdmi_20nm_phy_pll_set_clk_rate(c, rate);

	mdss_pll_resource_enable(io, false);

	if (set_power_dwn)
		hdmi_20nm_vco_enable(c);

	vco->rate = rate;
	vco->rate_set = true;

	return 0;
}

static unsigned long hdmi_20nm_vco_get_rate(struct clk *c)
{
	unsigned long freq = 0;
	int rc;
	struct hdmi_pll_vco_clk *vco = to_hdmi_20nm_vco_clk(c);
	struct mdss_pll_resources *io = vco->priv;

	if (is_gdsc_disabled(io))
		return 0;

	rc = mdss_pll_resource_enable(io, true);
	if (rc) {
		pr_err("pll resource can't be enabled\n");
		return rc;
	}

	mdss_pll_resource_enable(io, false);

	return freq;
}

static long hdmi_20nm_vco_round_rate(struct clk *c, unsigned long rate)
{
	unsigned long rrate = rate;

	pr_debug("rrate=%ld\n", rrate);

	return rrate;
}

static int hdmi_20nm_vco_prepare(struct clk *c)
{
	struct hdmi_pll_vco_clk *vco = to_hdmi_20nm_vco_clk(c);
	struct mdss_pll_resources *io = vco->priv;
	int ret = 0;

	pr_debug("rate=%ld\n", vco->rate);

	if (!vco->rate_set && vco->rate)
		ret = hdmi_20nm_vco_set_rate(c, vco->rate);

	if (!ret) {
		ret = mdss_pll_resource_enable(io, true);
		if (ret)
			pr_err("pll resource can't be enabled\n");
	}

	return ret;
}

static void hdmi_20nm_vco_unprepare(struct clk *c)
{
	struct hdmi_pll_vco_clk *vco = to_hdmi_20nm_vco_clk(c);
	struct mdss_pll_resources *io = vco->priv;

	vco->rate_set = false;

	if (!io) {
		pr_err("Invalid input parameter\n");
		return;
	}

	if (!io->pll_on &&
		mdss_pll_resource_enable(io, true)) {
		pr_err("pll resource can't be enabled\n");
		return;
	}

	io->handoff_resources = false;
	mdss_pll_resource_enable(io, false);
	io->pll_on = false;
}

static enum handoff hdmi_20nm_vco_handoff(struct clk *c)
{
	enum handoff ret = HANDOFF_DISABLED_CLK;
	struct hdmi_pll_vco_clk *vco = to_hdmi_20nm_vco_clk(c);
	struct mdss_pll_resources *io = vco->priv;

	if (is_gdsc_disabled(io))
		return HANDOFF_DISABLED_CLK;

	if (mdss_pll_resource_enable(io, true)) {
		pr_err("pll resource can't be enabled\n");
		return ret;
	}

	io->handoff_resources = true;

	if (hdmi_20nm_pll_lock_status(io)) {
		io->pll_on = true;
		c->rate = hdmi_20nm_vco_get_rate(c);
		ret = HANDOFF_ENABLED_CLK;
	} else {
		io->handoff_resources = false;
		mdss_pll_resource_enable(io, false);
	}

	pr_debug("done, ret=%d\n", ret);
	return ret;
}

static struct clk_ops hdmi_20nm_vco_clk_ops = {
	.enable = hdmi_20nm_vco_enable,
	.set_rate = hdmi_20nm_vco_set_rate,
	.get_rate = hdmi_20nm_vco_get_rate,
	.round_rate = hdmi_20nm_vco_round_rate,
	.prepare = hdmi_20nm_vco_prepare,
	.unprepare = hdmi_20nm_vco_unprepare,
	.handoff = hdmi_20nm_vco_handoff,
};

static struct hdmi_pll_vco_clk hdmi_20nm_vco_clk = {
	.c = {
		.dbg_name = "hdmi_20nm_vco_clk",
		.ops = &hdmi_20nm_vco_clk_ops,
		CLK_INIT(hdmi_20nm_vco_clk.c),
	},
};

static struct clk_lookup hdmipllcc_8994[] = {
	CLK_LIST(hdmi_20nm_vco_clk),
};

int hdmi_20nm_pll_clock_register(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res)
{
	int rc = -ENOTSUPP;

	if (!pll_res || !pll_res->phy_base || !pll_res->pll_base) {
		pr_err("Invalide input parameters\n");
		return -EPROBE_DEFER;
	}

	/* Set client data for vco, mux and div clocks */
	hdmi_20nm_vco_clk.priv = pll_res;

	rc = of_msm_clock_register(pdev->dev.of_node, hdmipllcc_8994,
						 ARRAY_SIZE(hdmipllcc_8994));
	if (rc) {
		pr_err("Clock register failed rc=%d\n", rc);
		rc = -EPROBE_DEFER;
	} else {
		pr_debug("%s: SUCCESS\n", __func__);
	}

	return rc;
}
