/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/**
 * @file    mtk_clk_buf_ctl.h
 * @brief   Driver for clock buffer control
 *
 */
#ifndef __MTK_CLK_BUF_CTL_H__
#define __MTK_CLK_BUF_CTL_H__

enum RC_CTRL_CMD {
	HW = 0,
	SW_OFF,
	SW_BBLPM,
	SW_FPM,
	SW_LPM,
	MAX_RC_REQ_NUM
};

enum CLK_BUF_TYPE {
	CLK_BUF_SW_DISABLE = 0,
	CLK_BUF_SW_ENABLE  = 1,
};

enum clk_buf_ret_type {
	CLK_BUF_DISABLE = 0,
	CLK_BUF_ENABLE  = 1,
	CLK_BUF_OK = 2,
	CLK_BUF_NOT_READY = -1,
	CLK_BUF_NOT_SUPPORT = -2,
	CLK_BUF_FAIL = -3,
};

/* clk_buf_id: users of clock buffer */
enum clk_buf_id {
	CLK_BUF_BB_MD = 0,
	CLK_BUF_CONN,
	CLK_BUF_NFC,
	CLK_BUF_RF,
	CLK_BUF_UFS = 6,	/* usually set as XO_7(idx = 6) */
	CLK_BUF_INVALID
};

/* xo_id: clock buffer list */
enum xo_id {
	XO_SOC = 0,
	XO_WCN,
	XO_NFC,
	XO_CEL,
	XO_AUD,		/* Disabled */
	XO_PD,		/* Disabled */
	XO_EXT,		/* UFS */
	XO_NUMBER
};

enum {
	BBLPM_SKIP = (1 << 0),
	BBLPM_WCN = (1 << XO_WCN),
	BBLPM_NFC = (1 << XO_NFC),
	BBLPM_CEL = (1 << XO_CEL),
	BBLPM_EXT = (1 << XO_EXT),
};

extern int clk_buf_hw_ctrl(u32 id, bool onoff);
extern int clk_buf_set_by_flightmode(bool on);
extern int clk_buf_control_bblpm(bool on);
extern int clk_buf_dump_log(void);
extern int clk_buf_get_xo_en_sta(u32 id);
extern int clk_buf_check_bblpm_enter_cond(void);
extern int clk_buf_voter_ctrl_by_id(const uint8_t subsys_id, enum RC_CTRL_CMD rc_req);
#endif

