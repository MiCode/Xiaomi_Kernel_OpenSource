/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef ADSP_CLK_H
#define ADSP_CLK_H

#define CLK_DEFAULT_INIT_CK     CLK_TOP_ADSPPLL_D6
#define CLK_DEFAULT_26M_CK      CLK_ADSP_CLK26M
#define ADSP_EARLY_PORTING_BYPASS       (1)

enum adsp_clk {
	CLK_ADSP_INFRA,
	CLK_TOP_ADSP_SEL,
	CLK_ADSP_CLK26M,
	CLK_TOP_MMPLL_D4,
	CLK_TOP_ADSPPLL_D4,
	CLK_TOP_ADSPPLL_D6,
	ADSP_CLK_NUM
};

void adsp_set_clock_freq(enum adsp_clk clk);
int adsp_set_top_mux(enum adsp_clk clk);
int adsp_enable_clock(void);
void adsp_disable_clock(void);
int adsp_clk_device_probe(void *dev);
void adsp_clk_device_remove(void *dev);

#endif /* ADSP_CLK_H */
