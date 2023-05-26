/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef ADSP_CLK_H
#define ADSP_CLK_H

#include <linux/platform_device.h>

enum adsp_clk {
	CLK_ADSP_INFRA_26M,
	CLK_ADSP_INFRA_32K,
	CLK_ADSP_INFRA,
	CLK_TOP_ADSP_SEL,
	CLK_TOP_CLK26M,
	CLK_TOP_ADSPPLL,
	ADSP_CLK_NUM
};

#define CLK_DEFAULT_INIT_CK     CLK_TOP_ADSPPLL
#define CLK_DEFAULT_26M_CK      CLK_TOP_CLK26M

void adsp_set_clock_freq(enum adsp_clk clk);
int adsp_set_top_mux(enum adsp_clk clk);
int adsp_enable_clock(void);
void adsp_disable_clock(void);
int adsp_clk_device_probe(struct platform_device *pdev);
void adsp_clk_device_remove(void *dev);

#endif /* ADSP_CLK_H */
