/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MT_SOC_AFE_CLK_H__
#define __MT_SOC_AFE_CLK_H__
#include <linux/types.h>

void mt_afe_apb_bus_init(void);

int mt_afe_init_clock(void *dev);
void mt_afe_deinit_clock(void *dev);
void mt_afe_power_off_default_clock(void);

void mt_afe_main_clk_on(void);
void mt_afe_main_clk_off(void);

void mt_afe_dac_clk_on(void);
void mt_afe_dac_clk_off(void);

void mt_afe_adc_clk_on(void);
void mt_afe_adc_clk_off(void);

void mt_afe_i2s_clk_on(void);
void mt_afe_i2s_clk_off(void);

void mt_afe_hdmi_clk_on(void);
void mt_afe_hdmi_clk_off(void);

void mt_afe_spdif_clk_on(void);
void mt_afe_spdif_clk_off(void);

void mt_afe_ana_clk_on(void);
void mt_afe_ana_clk_off(void);

void mt_afe_suspend_clk_on(void);
void mt_afe_suspend_clk_off(void);

void mt_afe_aplltuner_clk_on(void);
void mt_afe_aplltuner_clk_off(void);

void mt_afe_top_apll_clk_on(void);
void mt_afe_top_apll_clk_off(void);


void mt_afe_set_hdmi_clock_source(uint32_t SampleRate, int apllclksel);

void mt_afe_emi_clk_on(void);
void mt_afe_emi_clk_off(void);

#endif
