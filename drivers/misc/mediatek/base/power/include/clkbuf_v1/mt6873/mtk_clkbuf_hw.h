// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/**
 * @file    mtk_clk_buf_hw.h
 * @brief   Driver for clock buffer control
 *
 */
#ifndef __MTK_CLK_BUF_HW_H__
#define __MTK_CLK_BUF_HW_H__
#include <linux/regmap.h>

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
#ifndef _clk_buf_id_
#define _clk_buf_id_
/* clk_buf_id: users of clock buffer */
enum clk_buf_id {
	CLK_BUF_BB_MD		= 0,
	CLK_BUF_CONN,
	CLK_BUF_NFC,
	CLK_BUF_RF,
	CLK_BUF_UFS		= 6,
	CLK_BUF_INVALID
};
#endif
#ifndef _xo_id_
#define _xo_id_
/* xo_id: clock buffer list */
enum xo_id {
	XO_SOC	= 0,
	XO_WCN,
	XO_NFC,
	XO_CEL,
	XO_AUD,		/* Disabled */
	XO_PD,		/* Disabled */
	XO_EXT,		/* UFS */
	XO_NUMBER
};
#endif
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

enum {
	INF_ERROR = 0,
	INF_RC,
	INF_DCXO,
};

/*#define CLKBUF_USE_BBLPM*/

void clk_buf_post_init(void);
void clk_buf_init_pmic_clkbuf(struct regmap *regmap);
void clk_buf_init_pmic_wrap(void);
void clk_buf_init_pmic_swctrl(void);
bool clk_buf_ctrl_combine(enum clk_buf_id id, bool onoff);
void clk_buf_ctrl_bblpm_hw(short on);
#endif

