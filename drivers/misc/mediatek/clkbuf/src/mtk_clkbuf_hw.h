/* SPDX-License-Identifier: GPL-2.0 */
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

enum MTK_CLK_BUF_DRIVING_CURR {
	CLK_BUF_DRIVING_CURR_AUTO_K = -1,
	CLK_BUF_DRIVING_CURR_0,
	CLK_BUF_DRIVING_CURR_1,
	CLK_BUF_DRIVING_CURR_2,
	CLK_BUF_DRIVING_CURR_3
};

enum cmd_type {
	CLK_BUF_OFF,
	CLK_BUF_ON,
	CLK_BUF_ENBB,
	CLK_BUF_SIG,
	CLK_BUF_COBUF,
	CLK_BUF_INIT_SETTING
};

enum {
	PWR_STATUS_MD	= (1 << 0),
	PWR_STATUS_CONN	= (1 << 1),
};

enum {
	BUF_MAN_M = 0,
	EN_BB_M,
	SIG_CTRL_M,
	CO_BUF_M,
	MODE_M_NUM,
};

enum reg_type {
	PMIC_R = 0,
	PWRAP_R,
	SPM,
	REGMAP_NUM,
};

enum dev_sta {
	DEV_NOT_SUPPORT = 0,
	DEV_ON,
	DEV_OFF,
};

enum dts_arg {
	DCXO_START = 0,
	XO_START = DCXO_START,
	XO_SW_EN = XO_START,
	DCXO_CW,
	XO_HW_SEL,
	XO_END,

	BBLPM_START = XO_END,
	BBL_SW_EN = BBLPM_START,
	BBLPM_END,

	MISC_START = BBLPM_END,
	MISC_SRCLKENI_EN = MISC_START,
	//MISC_DRV_CURR,
	MISC_END,

	//AUXOUT_START = MISC_END,
	//AUXOUT_SEL = AUXOUT_START,
	//AUXOUT_XO_SOC_WCN_EN,
	//AUXOUT_XO_NFC_CEL_EN,
	//AUXOUT_XO_PD_EN,
	//AUXOUT_XO_EXT_EN,
	//AUXOUT_BBLPM_EN,
	//AUXOUT_XO_SOC_WCN_CURR,
	//AUXOUT_XO_NFC_CEL_CURR,
	//AUXOUT_XO_PD_EXT_CURR,
	//AUXOUT_END,
	DCXO_END = MISC_END,

	PWRAP_START = DCXO_END,
	PWRAP_DCXO_EN = PWRAP_START,
	PWRAP_CONN_EN,
	PWRAP_NFC_EN,
	PWRAP_CONN_CFG,
	PWRAP_NFC_CFG,
	PWRAP_END,

	GPIO_START = PWRAP_END,
	GPIO_END = GPIO_START,

	SPM_START = GPIO_END,
	SPM_MD_PWR_STA = SPM_START,
	SPM_CONN_PWR_STA,
	SPM_END,
	DTS_NUM = SPM_END,
};

struct dts_predef {
	const char prop[20];
	u32 len;
	u32 idx;
	u32 mask;
	u32 interval;
};

struct reg_info {
	struct regmap *regmap;
	u32 *ofs;
	u32 *bit;
};

struct clk_buf_op {
	/* initial flow */
	void		(*xo_init)(void);
	int		(*bblpm_init)(void);
	int		(*dts_init)(struct platform_device *dev);
	int		(*fs_init)(void);
	/* cat state */
	bool		(*get_bringup_sta)(void);
	bool		(*get_clkbuf_init_sta)(void);
	bool		(*get_flight_mode)(void);
	int		(*get_xo_sta)(enum xo_id id);
	void		(*get_bblpm_enter_cond)(u32 *bblpm_cond);
	int		(*get_bblpm_sta)(void);
	/* dump log */
	void		(*get_main_log)(void);
	int		(*get_dws_log)(char *buf);
	int		(*get_misc_log)(char *buf);
	/* set state */
	void		(*set_bringup_sta)(bool on);
	void		(*set_clkbuf_init_sta)(bool on);
	void		(*set_flight_mode)(bool on);
	bool		(*set_xo_sta)(enum clk_buf_id id, bool on);
	int		(*set_xo_cmd)(enum clk_buf_id id, enum cmd_type type);
	int		(*set_bblpm_sta)(bool on);
	int		(*set_bblpm_hw_mode)(bool on);
};

int clk_buf_hw_probe(struct platform_device *pdev);
#endif

