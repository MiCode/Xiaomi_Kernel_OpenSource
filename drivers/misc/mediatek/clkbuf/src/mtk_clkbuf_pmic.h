/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/**
 * @file    mtk_clk_buf_hw.h
 * @brief   Driver for clock buffer control
 *
 */
#ifndef __MTK_CLK_BUF_PMIC_H__
#define __MTK_CLK_BUF_PMIC_H__

#include <linux/list.h>

#include <mtk_clkbuf_common.h>
#include <mtk-clkbuf-bridge.h>

enum {
	PMIC_6357 = 0,
	PMIC_6359,
	PMIC_NUM,
};

enum {
	PMIC_DRV_CURR = 0,
	PMIC_HW_BBLPM,
	PMIC_HW_BBLPM_SEL,
	PMIC_LDO_VRFCK,
	PMIC_LDO_VBBCK,
	PMIC_AUXOUT_SEL,
	PMIC_AUXOUT_XO,
	PMIC_AUXOUT_DRV_CURR,
	PMIC_AUXOUT_BBLPM_EN,
	PMIC_AUXOUT_BBLPM_O,
	PMIC_HW_DTS_NUM, /* Number of HW dependent PMIC properties */
};

struct pmic_clkbuf_reg {
	struct regmap *regmap;
	u32 *ofs;
	u32 *bit;
};

struct pmic_clkbuf_dts {
	char name[32];
	struct pmic_clkbuf_reg cfg;
	u32 mask;
	struct list_head dts_list;
};

struct pmic_clkbuf_op {
	char pmic_name[20];
	void (*pmic_clk_buf_set_bblpm_hw_msk)(enum clk_buf_id id, bool onoff);
	int (*pmic_clk_buf_bblpm_hw_en)(bool on);
	void (*pmic_clk_buf_get_drv_curr)(u32 *drvcurr);
	void (*pmic_clk_buf_set_drv_curr)(u32 *drvcurr);
	int (*pmic_clk_buf_dts_init)(struct device_node *pmic_node,
					struct regmap *regmap);
	void (*pmic_clk_buf_get_xo_en)(u32 *stat);
	void (*pmic_clk_buf_get_bblpm_en)(u32 *stat);
	int (*pmic_clk_buf_dump_misc_log)(char *buf);
};

int get_pmic_clkbuf(struct device_node *node, struct pmic_clkbuf_op **pmic_op);

#endif
