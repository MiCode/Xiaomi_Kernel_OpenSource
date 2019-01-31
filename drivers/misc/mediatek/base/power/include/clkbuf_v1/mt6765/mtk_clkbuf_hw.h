/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/**
 * @file    mtk_clk_buf_hw.h
 * @brief   Driver for clock buffer control
 *
 */
#ifndef __MTK_CLK_BUF_HW_H__
#define __MTK_CLK_BUF_HW_H__

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

/* clk_buf_id: users of clock buffer */
enum clk_buf_id {
	CLK_BUF_BB_MD		= 0,
	CLK_BUF_CONN,
	CLK_BUF_NFC,
	CLK_BUF_RF,
	CLK_BUF_AUDIO,
	CLK_BUF_CHG,
	CLK_BUF_UFS,
	CLK_BUF_INVALID
};

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

enum {
	BBLPM_COND_SKIP	= (1 << 0),
	BBLPM_COND_WCN	= (1 << CLK_BUF_CONN),
	BBLPM_COND_NFC	= (1 << CLK_BUF_NFC),
	BBLPM_COND_CEL	= (1 << CLK_BUF_RF),
	BBLPM_COND_EXT	= (1 << CLK_BUF_UFS),
};

enum {
	CLKBUF_PWR_STATUS_MD	= (1 << 0),
	CLKBUF_PWR_STATUS_CONN	= (1 << 1),
};

#define CLKBUF_USE_BBLPM

void clk_buf_post_init(void);
void clk_buf_init_pmic_clkbuf(void);
void clk_buf_init_pmic_wrap(void);
void clk_buf_init_pmic_swctrl(void);

#endif

