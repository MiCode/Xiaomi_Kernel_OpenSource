/* SPDX-License-Identifier: GPL-2.0 */
/*
 * adsp_clk.h --  Mediatek ADSP clock control
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Celine Liu <Celine.liu@mediatek.com>
 */

#ifndef ADSP_CLK_H
#define ADSP_CLK_H

enum adsp_clk {
	CLK_ADSP_INFRA,
	CLK_TOP_ADSP_SEL,
	CLK_ADSP_CLK26M,
	CLK_TOP_MMPLL_D4,
	CLK_TOP_ADSPPLL_D4,
	CLK_TOP_ADSPPLL_D6,
	ADSP_CLK_NUM
};

int adsp_set_top_mux(enum adsp_clk clk);
int adsp_enable_clock(void);
void adsp_disable_clock(void);
int adsp_clk_device_probe(void *dev);
void adsp_clk_device_remove(void *dev);

#endif /* ADSP_CLK_H */
