/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef ADSP_CLK_H
#define ADSP_CLK_H

#include <linux/platform_device.h>

#define CLK_DEFAULT_INIT_CK     CLK_TOP_ADSPPLL
#define CLK_DEFAULT_26M_CK      CLK_TOP_CLK26M

enum adsp_clk {
	CLK_SCP_SYS_ADSP,
	CLK_ADSP_CK_CG,
	CLK_TOP_ADSP_SEL,
	CLK_TOP_CLK26M,
	CLK_TOP_ADSPPLL,
	ADSP_CLK_NUM
};

enum scp_clk {
	CLK_TOP_SCP_SEL,
	SCP_CLK_NUM
};

void adsp_set_clock_freq(enum adsp_clk clk);
int adsp_set_top_mux(enum adsp_clk clk);
int adsp_enable_clock(void);
void adsp_disable_clock(void);
int adsp_clk_device_probe(struct platform_device *pdev);
void adsp_clk_device_remove(void *dev);

#endif /* ADSP_CLK_H */
