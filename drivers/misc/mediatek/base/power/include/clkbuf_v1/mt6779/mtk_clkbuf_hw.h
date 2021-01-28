/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 MediaTek Inc.
 */

/**
 * @file    mtk_clk_buf_hw.h
 * @brief   Driver for clock buffer control
 *
 */
#ifndef __MTK_CLK_BUF_HW_H__
#define __MTK_CLK_BUF_HW_H__

#include <mtk-clkbuf-bridge.h>
#include <linux/mfd/mt6359/registers.h>

#define PMIC_DCXO_CW00		MT6359_DCXO_CW00
#define PMIC_DCXO_CW00_SET	MT6359_DCXO_CW00_SET
#define PMIC_DCXO_CW00_CLR	MT6359_DCXO_CW00_CLR
#define PMIC_DCXO_CW01		MT6359_DCXO_CW01
#define PMIC_DCXO_CW02		MT6359_DCXO_CW02
#define PMIC_DCXO_CW03		MT6359_DCXO_CW03
#define PMIC_DCXO_CW04		MT6359_DCXO_CW04
#define PMIC_DCXO_CW05		MT6359_DCXO_CW05
#define PMIC_DCXO_CW06		MT6359_DCXO_CW06
#define PMIC_DCXO_CW07		MT6359_DCXO_CW07
#define PMIC_DCXO_CW08		MT6359_DCXO_CW08
#define PMIC_DCXO_CW09		MT6359_DCXO_CW09
#define PMIC_DCXO_CW09_SET	MT6359_DCXO_CW09_SET
#define PMIC_DCXO_CW09_CLR	MT6359_DCXO_CW09_CLR
#define PMIC_DCXO_CW10		MT6359_DCXO_CW10
#define PMIC_DCXO_CW11		MT6359_DCXO_CW11
#define PMIC_DCXO_CW12		MT6359_DCXO_CW12
#define PMIC_DCXO_CW13		MT6359_DCXO_CW13
#define PMIC_DCXO_CW14		MT6359_DCXO_CW14
#define PMIC_DCXO_CW15		MT6359_DCXO_CW15
#define PMIC_DCXO_CW16		MT6359_DCXO_CW16
#define PMIC_DCXO_CW17		MT6359_DCXO_CW17
#define PMIC_DCXO_CW18		MT6359_DCXO_CW18
#define PMIC_DCXO_CW19		MT6359_DCXO_CW19

#define PMIC_DCXO_CW00_SET_ADDR                             \
	MT6359_DCXO_CW00_SET
#define PMIC_DCXO_CW09_SET_ADDR                             \
	MT6359_DCXO_CW09_SET
#define PMIC_DCXO_CW09_CLR_ADDR                             \
	MT6359_DCXO_CW09_CLR

#define PMIC_XO_BB_LPM_EN_SEL_ADDR                          \
	MT6359_DCXO_CW12
#define PMIC_XO_EXTBUF1_BBLPM_EN_MASK_ADDR                  \
	MT6359_DCXO_CW12
#define PMIC_XO_EXTBUF2_BBLPM_EN_MASK_ADDR                  \
	MT6359_DCXO_CW12
#define PMIC_XO_EXTBUF3_BBLPM_EN_MASK_ADDR                  \
	MT6359_DCXO_CW12
#define PMIC_XO_EXTBUF4_BBLPM_EN_MASK_ADDR                  \
	MT6359_DCXO_CW12
#define PMIC_XO_EXTBUF7_BBLPM_EN_MASK_ADDR                  \
	MT6359_DCXO_CW12
#define PMIC_XO_EXTBUF1_MODE_ADDR                           \
	MT6359_DCXO_CW00
#define PMIC_XO_EXTBUF2_MODE_ADDR                           \
	MT6359_DCXO_CW00
#define PMIC_XO_EXTBUF3_MODE_ADDR                           \
	MT6359_DCXO_CW00
#define PMIC_XO_EXTBUF4_MODE_ADDR                           \
	MT6359_DCXO_CW00
#define PMIC_XO_EXTBUF7_MODE_ADDR                           \
	MT6359_DCXO_CW09
#define PMIC_XO_EXTBUF2_CLKSEL_MAN_ADDR                     \
	MT6359_DCXO_CW12
#define PMIC_RG_XO_EXTBUF2_SRSEL_ADDR                       \
	MT6359_DCXO_CW13
#define PMIC_RG_XO_RESERVED1_ADDR                           \
	MT6359_DCXO_CW15
#define PMIC_RG_XO_EXTBUF2_RSEL_ADDR                        \
	MT6359_DCXO_CW19
#define PMIC_RG_SRCLKEN_IN3_EN_ADDR                         \
	MT6359_TOP_SPI_CON1
#define PMIC_RG_LDO_VRFCK_HW14_OP_EN_ADDR                   \
	MT6359_LDO_VRFCK_OP_EN
#define PMIC_RG_LDO_VBBCK_HW14_OP_EN_ADDR                   \
	MT6359_LDO_VBBCK_OP_EN
#define PMIC_RG_LDO_VRFCK_EN_ADDR                           \
	MT6359_LDO_VRFCK_CON0
#define PMIC_RG_LDO_VBBCK_EN_ADDR                           \
	MT6359_LDO_VBBCK_CON0
#define PMIC_XO_STATIC_AUXOUT_SEL_ADDR                      \
	MT6359_DCXO_CW16
#define PMIC_XO_STATIC_AUXOUT_ADDR                          \
	MT6359_DCXO_CW17

#define PMIC_XO_EXTBUF7_EN_M_MASK                           0x1
#define PMIC_XO_EXTBUF7_EN_M_SHIFT                          14
#define PMIC_XO_BB_LPM_EN_SEL_MASK                          0x1
#define PMIC_XO_BB_LPM_EN_SEL_SHIFT                         0
#define PMIC_XO_BB_LPM_EN_M_MASK                            0x1
#define PMIC_XO_BB_LPM_EN_M_SHIFT                           12
#define PMIC_XO_EXTBUF1_BBLPM_EN_MASK_MASK                  0x1
#define PMIC_XO_EXTBUF1_BBLPM_EN_MASK_SHIFT                 1
#define PMIC_XO_EXTBUF2_BBLPM_EN_MASK_MASK                  0x1
#define PMIC_XO_EXTBUF2_BBLPM_EN_MASK_SHIFT                 2
#define PMIC_XO_EXTBUF3_BBLPM_EN_MASK_MASK                  0x1
#define PMIC_XO_EXTBUF3_BBLPM_EN_MASK_SHIFT                 3
#define PMIC_XO_EXTBUF4_BBLPM_EN_MASK_MASK                  0x1
#define PMIC_XO_EXTBUF4_BBLPM_EN_MASK_SHIFT                 4
#define PMIC_XO_EXTBUF7_BBLPM_EN_MASK_MASK                  0x1
#define PMIC_XO_EXTBUF7_BBLPM_EN_MASK_SHIFT                 6

#define PMIC_XO_EXTBUF2_MODE_MASK                           0x3
#define PMIC_XO_EXTBUF2_MODE_SHIFT                          3
#define PMIC_XO_EXTBUF2_EN_M_MASK                           0x1
#define PMIC_XO_EXTBUF2_EN_M_SHIFT                          5
#define PMIC_XO_EXTBUF3_MODE_MASK                           0x3
#define PMIC_XO_EXTBUF3_MODE_SHIFT                          6
#define PMIC_XO_EXTBUF3_EN_M_MASK                           0x1
#define PMIC_XO_EXTBUF3_EN_M_SHIFT                          8
#define PMIC_XO_EXTBUF4_MODE_MASK                           0x3
#define PMIC_XO_EXTBUF4_MODE_SHIFT                          9
#define PMIC_XO_EXTBUF4_EN_M_MASK                           0x1
#define PMIC_XO_EXTBUF4_EN_M_SHIFT                          11
#define PMIC_XO_EXTBUF6_MODE_MASK                           0x3
#define PMIC_XO_EXTBUF6_MODE_SHIFT                          9
#define PMIC_XO_EXTBUF6_EN_M_MASK                           0x1
#define PMIC_XO_EXTBUF6_EN_M_SHIFT                          11
#define PMIC_XO_EXTBUF7_MODE_MASK                           0x3
#define PMIC_XO_EXTBUF7_MODE_SHIFT                          12
#define PMIC_XO_EXTBUF7_EN_M_MASK                           0x1
#define PMIC_XO_EXTBUF7_EN_M_SHIFT                          14

#define PMIC_RG_SRCLKEN_IN3_EN_MASK                         0x1
#define PMIC_RG_SRCLKEN_IN3_EN_SHIFT                        0
#define PMIC_RG_LDO_VRFCK_HW14_OP_EN_MASK                   0x1
#define PMIC_RG_LDO_VRFCK_HW14_OP_EN_SHIFT                  14
#define PMIC_RG_LDO_VBBCK_HW14_OP_EN_MASK                   0x1
#define PMIC_RG_LDO_VBBCK_HW14_OP_EN_SHIFT                  14
#define PMIC_RG_LDO_VRFCK_EN_MASK                           0x1
#define PMIC_RG_LDO_VRFCK_EN_SHIFT                          0
#define PMIC_RG_LDO_VBBCK_EN_MASK                           0x1
#define PMIC_RG_LDO_VBBCK_EN_SHIFT                          0
#define PMIC_XO_STATIC_AUXOUT_SEL_MASK                      0x3F
#define PMIC_XO_STATIC_AUXOUT_SEL_SHIFT                     0
#define PMIC_XO_STATIC_AUXOUT_MASK                          0xFFFF
#define PMIC_XO_STATIC_AUXOUT_SHIFT                         0


enum MTK_CLK_BUF_STATUS {
	CLOCK_BUFFER_DISABLE,
	CLOCK_BUFFER_SW_CONTROL,
	CLOCK_BUFFER_HW_CONTROL,
};

enum MTK_CLK_BUF_OUTPUT_IMPEDANCE {
	CLK_BUF_OUTPUT_IMPEDANCE_0,
	CLK_BUF_OUTPUT_IMPEDANCE_1,
	CLK_BUF_OUTPUT_IMPEDANCE_2,
	CLK_BUF_OUTPUT_IMPEDANCE_3,
	CLK_BUF_OUTPUT_IMPEDANCE_4,
	CLK_BUF_OUTPUT_IMPEDANCE_5,
	CLK_BUF_OUTPUT_IMPEDANCE_6,
	CLK_BUF_OUTPUT_IMPEDANCE_7,
};

enum MTK_CLK_BUF_CONTROLS_FOR_DESENSE {
	CLK_BUF_CONTROLS_FOR_DESENSE_0,
	CLK_BUF_CONTROLS_FOR_DESENSE_1,
	CLK_BUF_CONTROLS_FOR_DESENSE_2,
	CLK_BUF_CONTROLS_FOR_DESENSE_3,
	CLK_BUF_CONTROLS_FOR_DESENSE_4,
	CLK_BUF_CONTROLS_FOR_DESENSE_5,
	CLK_BUF_CONTROLS_FOR_DESENSE_6,
	CLK_BUF_CONTROLS_FOR_DESENSE_7,
};

enum clk_buf_onff {
	CLK_BUF_FORCE_OFF,
	CLK_BUF_FORCE_ON,
	CLK_BUF_INIT_SETTING
};

enum {
	BBLPM_COND_SKIP	= (1 << 0),
	BBLPM_COND_WCN	= (1 << CLK_BUF_CONN),
	BBLPM_COND_NFC	= (1 << CLK_BUF_NFC),
	BBLPM_COND_CEL	= (1 << CLK_BUF_RF),
	BBLPM_COND_EXT	= (1 << CLK_BUF_UFS),
};

enum {
	PWR_STATUS_MD	= (1 << 0),
	PWR_STATUS_CONN	= (1 << 1),
};

enum {
	SOC_EN_M = 0,
	SOC_EN_BB_G,
	SOC_CLK_SEL_G,
	SOC_EN_BB_CLK_SEL_G,
};

enum {
	WCN_EN_M = 0,
	WCN_EN_BB_G,
	WCN_SRCLKEN_CONN,
	WCN_BUF24_EN,
};

enum {
	NFC_EN_M = 0,
	NFC_EN_BB_G,
	NFC_CLK_SEL_G,
	NFC_BUF234_EN,
};

enum {
	CEL_EN_M = 0,
	CEL_EN_BB_G,
	CEL_CLK_SEL_G,
	CEL_BUF24_EN,
};

enum {
	EXT_EN_M = 0,
	EXT_EN_BB_G,
	EXT_CLK_SEL_G,
	EXT_BUF247_EN,
};

#define CLKBUF_USE_BBLPM

void clk_buf_post_init(void);
void clk_buf_init_pmic_clkbuf(struct regmap *regmap);
void clk_buf_init_pmic_wrap(void);
void clk_buf_init_pmic_swctrl(void);
bool clk_buf_ctrl_combine(enum clk_buf_id id, bool onoff);
void clk_buf_ctrl_bblpm_hw(short on);
#endif

