/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

/**
 * @file    mtk_clk_buf_hw.h
 * @brief   Driver for clock buffer control
 *
 */
#ifndef __MTK_CLK_BUF_HW_H__
#define __MTK_CLK_BUF_HW_H__
#include <mtk-clkbuf-bridge.h>

#define MT6357_TOP_SPI_CON1                  (0x44a)
#define MT6357_DCXO_CW00                     (0x788)
#define MT6357_DCXO_CW00_SET                 (0x78a)
#define MT6357_DCXO_CW00_CLR                 (0x78c)
#define MT6357_DCXO_CW01                     (0x78e)
#define MT6357_DCXO_CW02                     (0x790)
#define MT6357_DCXO_CW03                     (0x792)
#define MT6357_DCXO_CW04                     (0x794)
#define MT6357_DCXO_CW05                     (0x796)
#define MT6357_DCXO_CW06                     (0x798)
#define MT6357_DCXO_CW07                     (0x79a)
#define MT6357_DCXO_CW08                     (0x79c)
#define MT6357_DCXO_CW09                     (0x79e)
#define MT6357_DCXO_CW10                     (0x7a0)
#define MT6357_DCXO_CW11                     (0x7a2)
#define MT6357_DCXO_CW11_SET                 (0x7a4)
#define MT6357_DCXO_CW11_CLR                 (0x7a6)
#define MT6357_DCXO_CW12                     (0x7a8)
#define MT6357_DCXO_CW13                     (0x7aa)
#define MT6357_DCXO_CW14                     (0x7ac)
#define MT6357_DCXO_CW15                     (0x7ae)
#define MT6357_DCXO_CW16                     (0x7b0)
#define MT6357_DCXO_CW17                     (0x7b2)
#define MT6357_DCXO_CW18                     (0x7b4)
#define MT6357_DCXO_CW19                     (0x7b6)
#define MT6357_DCXO_CW20                     (0x7b8)
#define MT6357_DCXO_CW21                     (0x7ba)
#define MT6357_DCXO_CW22                     (0x7bc)

#define PMIC_DCXO_CW00_SET_ADDR                            \
	MT6357_DCXO_CW00_SET
#define PMIC_DCXO_CW00_CLR_ADDR                            \
	MT6357_DCXO_CW00_CLR
#define PMIC_DCXO_CW11_SET_ADDR                            \
	MT6357_DCXO_CW11_SET
#define PMIC_DCXO_CW11_CLR_ADDR                            \
	MT6357_DCXO_CW11_CLR

#define PMIC_XO_BB_LPM_EN_MASK                             0x1
#define PMIC_XO_BB_LPM_EN_SHIFT                            12

#define PMIC_XO_STATIC_AUXOUT_SEL_MASK                     0x3F
#define PMIC_XO_STATIC_AUXOUT_SEL_SHIFT                    0
#define PMIC_XO_STATIC_AUXOUT_MASK                         0xFFFF
#define PMIC_XO_STATIC_AUXOUT_SHIFT                        0

#define PMIC_XO_BUFLDOK_EN_ADDR                            \
	MT6357_DCXO_CW02
#define PMIC_XO_BUFLDOK_EN_MASK                            0x1
#define PMIC_XO_BUFLDOK_EN_SHIFT                           15

#define PMIC_XO_EXTBUF1_MODE_ADDR                          \
	MT6357_DCXO_CW00
#define PMIC_XO_EXTBUF2_EN_M_ADDR                          \
	MT6357_DCXO_CW00
#define PMIC_XO_EXTBUF2_EN_M_MASK                          0x1
#define PMIC_XO_EXTBUF2_EN_M_SHIFT                         5
#define PMIC_XO_EXTBUF2_MODE_MASK                          0x3
#define PMIC_XO_EXTBUF2_MODE_SHIFT                         3


#define PMIC_XO_EXTBUF3_EN_M_ADDR                          \
	MT6357_DCXO_CW00
#define PMIC_XO_EXTBUF3_EN_M_MASK                          0x1
#define PMIC_XO_EXTBUF3_EN_M_SHIFT                         8
#define PMIC_XO_EXTBUF3_MODE_MASK                          0x3
#define PMIC_XO_EXTBUF3_MODE_SHIFT                         6


#define PMIC_XO_EXTBUF4_EN_M_ADDR                          \
	MT6357_DCXO_CW00
#define PMIC_XO_EXTBUF4_EN_M_MASK                          0x1
#define PMIC_XO_EXTBUF4_EN_M_SHIFT                         11
#define PMIC_XO_EXTBUF4_MODE_MASK                          0x3
#define PMIC_XO_EXTBUF4_MODE_SHIFT                         9


#define PMIC_XO_EXTBUF6_EN_M_ADDR                          \
	MT6357_DCXO_CW11
#define PMIC_XO_EXTBUF6_EN_M_MASK                          0x1
#define PMIC_XO_EXTBUF6_EN_M_SHIFT                         10
#define PMIC_XO_EXTBUF6_MODE_ADDR                          \
	MT6357_DCXO_CW11
#define PMIC_XO_EXTBUF6_MODE_MASK                          0x3
#define PMIC_XO_EXTBUF6_MODE_SHIFT                         8

#define PMIC_XO_EXTBUF7_EN_M_ADDR                          \
	MT6357_DCXO_CW11
#define PMIC_XO_EXTBUF7_EN_M_MASK                          0x1
#define PMIC_XO_EXTBUF7_EN_M_SHIFT                         13
#define PMIC_XO_EXTBUF7_MODE_MASK                          0x3
#define PMIC_XO_EXTBUF7_MODE_SHIFT                         11

#define PMIC_RG_SRCLKEN_IN3_EN_ADDR                        \
	MT6357_TOP_SPI_CON1
#define PMIC_RG_SRCLKEN_IN3_EN_MASK                        0x1
#define PMIC_RG_SRCLKEN_IN3_EN_SHIFT                       0

#define PMIC_RG_XO_RESERVED4_ADDR                          \
	MT6357_DCXO_CW13
#define PMIC_XO_EXTBUF2_CLKSEL_MAN_ADDR                    \
	MT6357_DCXO_CW14
#define PMIC_RG_XO_EXTBUF1_HD_ADDR                         \
	MT6357_DCXO_CW15
#define PMIC_RG_XO_RESRVED10_ADDR                          \
	MT6357_DCXO_CW20

#define PMIC_XO_EXTBUF1_ISET_M_ADDR                        \
	MT6357_DCXO_CW16
#define PMIC_XO_EXTBUF1_ISET_M_MASK                        0x3
#define PMIC_XO_EXTBUF1_ISET_M_SHIFT                       0
#define PMIC_XO_EXTBUF2_ISET_M_MASK                        0x3
#define PMIC_XO_EXTBUF2_ISET_M_SHIFT                       2
#define PMIC_XO_EXTBUF3_ISET_M_MASK                        0x3
#define PMIC_XO_EXTBUF3_ISET_M_SHIFT                       4
#define PMIC_XO_EXTBUF4_ISET_M_MASK                        0x3
#define PMIC_XO_EXTBUF4_ISET_M_SHIFT                       6
#define PMIC_XO_EXTBUF6_ISET_M_MASK                        0x3
#define PMIC_XO_EXTBUF6_ISET_M_SHIFT                       10
#define PMIC_XO_EXTBUF7_ISET_M_MASK                        0x3
#define PMIC_XO_EXTBUF7_ISET_M_SHIFT                       12

enum MTK_CLK_BUF_STATUS {
	CLOCK_BUFFER_DISABLE,
	CLOCK_BUFFER_SW_CONTROL,
	CLOCK_BUFFER_HW_CONTROL,
};

enum MTK_CLK_BUF_DRIVING_CURR {
	CLK_BUF_DRIVING_CURR_AUTO_K = -1,
	CLK_BUF_DRIVING_CURR_0,
	CLK_BUF_DRIVING_CURR_1,
	CLK_BUF_DRIVING_CURR_2,
	CLK_BUF_DRIVING_CURR_3
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

#define CLKBUF_USE_BBLPM

void clk_buf_post_init(void);
void clk_buf_init_pmic_clkbuf(struct regmap *regmap);
void clk_buf_init_pmic_wrap(void);
void clk_buf_init_pmic_swctrl(void);

#endif

