/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef ADSP_CLK_H
#define ADSP_CLK_H

#include <linux/platform_device.h>

enum adsp_clk_mode {
	CLK_LOW_POWER,
	CLK_DEFAULT_INIT,
	CLK_HIGH_PERFORM,
};

struct adsp_clk_operations {
	void (*select)(enum adsp_clk_mode mode);
	int (*enable)(void);
	void (*disable)(void);
};

void adsp_select_clock_mode(enum adsp_clk_mode mode);
int adsp_enable_clock(void);
void adsp_disable_clock(void);

#endif /* ADSP_CLK_H */
