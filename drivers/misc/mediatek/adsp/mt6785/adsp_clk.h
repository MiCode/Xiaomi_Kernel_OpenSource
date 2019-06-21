/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
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
