/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
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
	PWRAP_START = 0,
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

struct pmic_clkbuf_op {
	char pmic_name[20];
	int (*pmic_clk_buf_set_bblpm_hw_msk)(u32 id, bool onoff);
	int (*pmic_clk_buf_bblpm_hw_en)(bool on);
	int (*pmic_clk_buf_get_drv_curr)(u32 id, u32 *drvcurr);
	int (*pmic_clk_buf_set_drv_curr)(u32 id, u32 drvcurr);
	int (*pmic_clk_buf_get_xo_num)(void);
	int (*pmic_clk_buf_get_xo_name)(u32 id, char *name);
	int (*pmic_clk_buf_get_xo_en)(u32 id, u32 *stat);
	int (*pmic_clk_buf_set_xo_sw_en)(u32 id, bool onoff);
	int (*pmic_clk_buf_get_xo_sw_en)(u32 id, u32 *stat);
	int (*pmic_clk_buf_set_xo_mode)(u32 id, u32 mode);
	int (*pmic_clk_buf_get_xo_mode)(u32 id, u32 *stat);
	int (*pmic_clk_buf_get_bblpm_en)(u32 *stat);
	int (*pmic_clk_buf_set_bblpm_sw_en)(bool on);
	int (*pmic_clk_buf_dump_misc_log)(char *buf);
};

void clk_buf_set_init_sta(bool done);
bool clk_buf_get_flight_mode(void);
int clk_buf_hw_set_flight_mode(bool on);
void clk_buf_hw_dump_misc_log(void);
int clk_buf_bblpm_init(void);
void clk_buf_get_enter_bblpm_cond(u32 *bblpm_cond);
int clk_buf_ctrl_bblpm_sw(bool enable);
int clk_buf_hw_ctrl(u32 id, bool onoff);
int clk_buf_xo_init(void);
int clk_buf_hw_get_xo_en(u32 id, u32 *stat);
bool clk_buf_get_init_sta(void);
void clk_buf_get_bringup_node(struct platform_device *pdev);
bool clk_buf_get_bringup_sta(void);
int clk_buf_fs_init(void);
int clk_buf_dts_init(struct platform_device *pdev);

void set_clkbuf_ops(const struct pmic_clkbuf_op *ops);
int clkbuf_hw_is_ready(void);
#endif

