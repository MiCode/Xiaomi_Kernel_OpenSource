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

#include "mt_afe_def.h"
#include "mt_afe_clk.h"
#include "mt_afe_reg.h"
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <sync_write.h>
#ifdef IDLE_TASK_DRIVER_API
#include <mt_idle.h>
#endif
#ifdef COMMON_CLOCK_FRAMEWORK_API
#include <linux/clk.h>
#endif
#ifdef MT_DCM_API
#include <mt_dcm.h>
#endif

enum audio_system_clock_type {
	CLOCK_INFRA_SYS_AUDIO = 0,
	CLOCK_TOP_PDN_AUDIO,
	CLOCK_TOP_PDN_AUD_INTBUS,
	CLOCK_TOP_AUD_1_SEL,
	CLOCK_TOP_AUD_2_SEL,
	CLOCK_TOP_APLL1_CK,
	CLOCK_TOP_APLL2_CK,
	CLOCK_CLK26M,
	CLOCK_NUM
};

#ifdef COMMON_CLOCK_FRAMEWORK_API

struct audio_clock_attr {
	const char *name;
	const bool prepare_once;
	bool is_prepared;
	struct clk *clock;
};

static struct audio_clock_attr aud_clks[CLOCK_NUM] = {
	[CLOCK_INFRA_SYS_AUDIO] = {"infra_sys_audio_clk", true, false, NULL},
	[CLOCK_TOP_PDN_AUDIO] = {"top_pdn_audio", true, false, NULL},
	[CLOCK_TOP_PDN_AUD_INTBUS] = {"top_pdn_aud_intbus", true, false, NULL},
	[CLOCK_TOP_AUD_1_SEL] = {"top_audio_1_sel", false, false, NULL},
	[CLOCK_TOP_AUD_2_SEL] = {"top_audio_2_sel", false, false, NULL},
	[CLOCK_TOP_APLL1_CK] = {"top_apll1_ck", false, false, NULL},
	[CLOCK_TOP_APLL2_CK] = {"top_apll2_ck", false, false, NULL},
	[CLOCK_CLK26M] = {"clk26m", false, false, NULL}
};

#endif

/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/
int aud_afe_clk_cntr;
int aud_dac_clk_cntr;
int aud_adc_clk_cntr;
int aud_i2s_clk_cntr;
int aud_hdmi_clk_cntr;
int aud_spdif_clk_cntr;
int aud_spdif2_clk_cntr;
int aud_apll22m_clk_cntr;
int aud_apll24m_clk_cntr;
int aud_apll1_tuner_cntr;
int aud_apll2_tuner_cntr;
int aud_emi_clk_cntr;
int aud_bus_clk_boost_cntr;

static DEFINE_SPINLOCK(afe_clk_lock);
static DEFINE_MUTEX(afe_clk_mutex);
static DEFINE_MUTEX(emi_clk_mutex);
static DEFINE_MUTEX(bus_clk_mutex);

/*****************************************************************************
 *                         INTERNAL FUNCTION
 *****************************************************************************/
void turn_on_afe_clock(void)
{
#if defined(COMMON_CLOCK_FRAMEWORK_API)
	int ret = clk_enable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);

	if (ret)
		pr_err("%s clk_enable %s fail %d\n",
		       __func__, aud_clks[CLOCK_INFRA_SYS_AUDIO].name, ret);

	ret = clk_enable(aud_clks[CLOCK_TOP_PDN_AUD_INTBUS].clock);
	if (ret)
		pr_err("%s clk_enable %s fail %d\n",
		       __func__, aud_clks[CLOCK_TOP_PDN_AUD_INTBUS].name, ret);

	ret = clk_enable(aud_clks[CLOCK_TOP_PDN_AUDIO].clock);
	if (ret)
		pr_err("%s clk_enable %s fail %d\n",
		       __func__, aud_clks[CLOCK_TOP_PDN_AUDIO].name, ret);

	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 2, 1 << 2);
#else
	/* mt_afe_topck_set_reg(AUDIO_CLK_CFG_4, 0 << 31, 1 << 31); */
	/* mt_afe_topck_set_reg(AUDIO_CLK_CFG_4, 0 << 23, 1 << 23); */
	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 2, 1 << 2);
#endif
}

void turn_off_afe_clock(void)
{
#if defined(COMMON_CLOCK_FRAMEWORK_API)
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 2, 1 << 2);
	clk_disable(aud_clks[CLOCK_TOP_PDN_AUDIO].clock);
	clk_disable(aud_clks[CLOCK_TOP_PDN_AUD_INTBUS].clock);
	clk_disable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 2, 1 << 2);
	/* mt_afe_topck_set_reg(AUDIO_CLK_CFG_4, 1 << 23, 1 << 23); */
	/* mt_afe_topck_set_reg(AUDIO_CLK_CFG_4, 1 << 31, 1 << 31); */
#endif
}

void turn_on_dac_clock(void)
{
#if defined(COMMON_CLOCK_FRAMEWORK_API)
	int ret = clk_enable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);

	if (ret)
		pr_err("%s clk_enable %s fail %d\n",
		       __func__, aud_clks[CLOCK_INFRA_SYS_AUDIO].name, ret);

	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 25, 1 << 25);
	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 26, 1 << 26);
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 25, 1 << 25);
	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 26, 1 << 26);
#endif
}

void turn_off_dac_clock(void)
{
#if defined(COMMON_CLOCK_FRAMEWORK_API)
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 26, 1 << 26);
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 25, 1 << 25);
	clk_disable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 26, 1 << 26);
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 25, 1 << 25);
#endif
}

void turn_on_adc_clock(void)
{
#if defined(COMMON_CLOCK_FRAMEWORK_API)
	int ret = clk_enable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);

	if (ret)
		pr_err("%s clk_enable %s fail %d\n",
		       __func__, aud_clks[CLOCK_INFRA_SYS_AUDIO].name, ret);

	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 24, 1 << 24);
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 24, 1 << 24);
#endif
}

void turn_off_adc_clock(void)
{
#if defined(COMMON_CLOCK_FRAMEWORK_API)
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 24, 1 << 24);
	clk_disable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 24, 1 << 24);
#endif
}

void turn_on_i2s_clock(void)
{
#if defined(COMMON_CLOCK_FRAMEWORK_API)
	int ret = clk_enable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);

	if (ret)
		pr_err("%s clk_enable %s fail %d\n",
		       __func__, aud_clks[CLOCK_INFRA_SYS_AUDIO].name, ret);

	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 6, 1 << 6);
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 6, 1 << 6);
#endif
}

void turn_off_i2s_clock(void)
{
#if defined(COMMON_CLOCK_FRAMEWORK_API)
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 6, 1 << 6);
	clk_disable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 6, 1 << 6);
#endif
}

void turn_on_hdmi_clock(void)
{
#if defined(COMMON_CLOCK_FRAMEWORK_API)
	int ret = clk_enable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);

	if (ret)
		pr_err("%s clk_enable %s fail %d\n",
		       __func__, aud_clks[CLOCK_INFRA_SYS_AUDIO].name, ret);

	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 20, 1 << 20);
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 20, 1 << 20);
#endif
}

void turn_off_hdmi_clock(void)
{
#if defined(COMMON_CLOCK_FRAMEWORK_API)
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 20, 1 << 20);
	clk_disable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 20, 1 << 20);
#endif
}

void turn_on_spdif_clock(void)
{
#if defined(COMMON_CLOCK_FRAMEWORK_API)
	int ret = clk_enable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);

	if (ret)
		pr_err("%s clk_enable %s fail %d\n",
		       __func__, aud_clks[CLOCK_INFRA_SYS_AUDIO].name, ret);

	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 21, 1 << 21);
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 21, 1 << 21);
#endif
}

void turn_off_spdif_clock(void)
{
#if defined(COMMON_CLOCK_FRAMEWORK_API)
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 21, 1 << 21);
	clk_disable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 21, 1 << 21);
#endif
}

void turn_on_spdif2_clock(void)
{
#if defined(COMMON_CLOCK_FRAMEWORK_API)
	int ret = clk_enable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);

	if (ret)
		pr_err("%s clk_enable %s fail %d\n",
		       __func__, aud_clks[CLOCK_INFRA_SYS_AUDIO].name, ret);

	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 11, 1 << 11);
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 11, 1 << 11);
#endif
}

void turn_off_spdif2_clock(void)
{
#if defined(COMMON_CLOCK_FRAMEWORK_API)
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 11, 1 << 11);
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 11, 1 << 11);
#endif
}

void turn_on_apll22m_clock(void)
{
#if defined(COMMON_CLOCK_FRAMEWORK_API)
	int ret = clk_prepare_enable(aud_clks[CLOCK_TOP_AUD_1_SEL].clock);

	if (ret)
		pr_err("%s clk_prepare_enable %s fail %d\n",
		       __func__, aud_clks[CLOCK_TOP_AUD_1_SEL].name, ret);

	ret = clk_set_parent(aud_clks[CLOCK_TOP_AUD_1_SEL].clock,
			     aud_clks[CLOCK_TOP_APLL1_CK].clock);
	if (ret)
		pr_err("%s clk_set_parent %s-%s fail %d\n",
		       __func__, aud_clks[CLOCK_TOP_AUD_1_SEL].name,
		       aud_clks[CLOCK_TOP_APLL1_CK].name, ret);

	ret = clk_enable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);
	if (ret)
		pr_err("%s clk_enable %s fail %d\n",
		       __func__, aud_clks[CLOCK_INFRA_SYS_AUDIO].name, ret);

	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 8, 1 << 8);
#else
	mt_afe_pll_set_reg(AUDIO_APLL1_PWR_CON0, 1 << 0, 1 << 0);
	mt_afe_pll_set_reg(AUDIO_APLL1_PWR_CON0, 0 << 1, 1 << 1);
	mt_afe_pll_set_reg(AUDIO_APLL1_CON0, 1 << 0, 1 << 0);
	mt_afe_topck_set_reg(AUDIO_CLK_CFG_6, 0 << 31, 1 << 31);	/* enable hf_haud_1_ck */
	mt_afe_topck_set_reg(AUDIO_CLK_CFG_6, 0x01000000, 0x03000000);	/* select apll1_ck */
	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 8, 1 << 8);
#endif
}

void turn_off_apll22m_clock(void)
{
#if defined(COMMON_CLOCK_FRAMEWORK_API)
	int ret;

	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 8, 1 << 8);
	clk_disable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);

	ret = clk_set_parent(aud_clks[CLOCK_TOP_AUD_1_SEL].clock, aud_clks[CLOCK_CLK26M].clock);
	if (ret)
		pr_err("%s clk_set_parent %s-%s fail %d\n",
		       __func__, aud_clks[CLOCK_TOP_AUD_1_SEL].name,
		       aud_clks[CLOCK_CLK26M].name, ret);

	clk_disable_unprepare(aud_clks[CLOCK_TOP_AUD_1_SEL].clock);
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 8, 1 << 8);
	mt_afe_topck_set_reg(AUDIO_CLK_CFG_6, 0x0, 0x03000000);	/* select clk26m */
	mt_afe_topck_set_reg(AUDIO_CLK_CFG_6, 1 << 31, 1 << 31);	/* disable hf_haud_1_ck */
	mt_afe_pll_set_reg(AUDIO_APLL1_CON0, 0 << 0, 1 << 0);
	mt_afe_pll_set_reg(AUDIO_APLL1_PWR_CON0, 1 << 1, 1 << 1);
	mt_afe_pll_set_reg(AUDIO_APLL1_PWR_CON0, 0 << 0, 1 << 0);
#endif
}

void turn_on_apll24m_clock(void)
{
#if defined(COMMON_CLOCK_FRAMEWORK_API)
	int ret = clk_prepare_enable(aud_clks[CLOCK_TOP_AUD_2_SEL].clock);

	if (ret)
		pr_err("%s clk_prepare_enable %s fail %d\n",
		       __func__, aud_clks[CLOCK_TOP_AUD_2_SEL].name, ret);

	ret = clk_set_parent(aud_clks[CLOCK_TOP_AUD_2_SEL].clock,
			     aud_clks[CLOCK_TOP_APLL2_CK].clock);
	if (ret)
		pr_err("%s clk_set_parent %s-%s fail %d\n",
		       __func__, aud_clks[CLOCK_TOP_AUD_2_SEL].name,
		       aud_clks[CLOCK_TOP_APLL2_CK].name, ret);

	ret = clk_enable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);
	if (ret)
		pr_err("%s clk_enable %s fail %d\n",
		       __func__, aud_clks[CLOCK_INFRA_SYS_AUDIO].name, ret);

	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 9, 1 << 9);
#else
	mt_afe_pll_set_reg(AUDIO_APLL2_PWR_CON0, 1 << 0, 1 << 0);
	mt_afe_pll_set_reg(AUDIO_APLL2_PWR_CON0, 0 << 1, 1 << 1);
	mt_afe_pll_set_reg(AUDIO_APLL2_CON0, 1 << 0, 1 << 0);
	mt_afe_topck_set_reg(AUDIO_CLK_CFG_7, 0 << 7, 1 << 7);	/* enable hf_haud_2_ck */
	mt_afe_topck_set_reg(AUDIO_CLK_CFG_7, 0x1, 0x3);	/* select apll2_ck */
	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 9, 1 << 9);
#endif
}

void turn_off_apll24m_clock(void)
{
#if defined(COMMON_CLOCK_FRAMEWORK_API)
	int ret;

	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 9, 1 << 9);
	clk_disable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);

	ret = clk_set_parent(aud_clks[CLOCK_TOP_AUD_2_SEL].clock, aud_clks[CLOCK_CLK26M].clock);
	if (ret)
		pr_err("%s clk_set_parent %s-%s fail %d\n",
		       __func__, aud_clks[CLOCK_TOP_AUD_2_SEL].name,
		       aud_clks[CLOCK_CLK26M].name, ret);

	clk_disable_unprepare(aud_clks[CLOCK_TOP_AUD_2_SEL].clock);
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 9, 1 << 9);
	mt_afe_pll_set_reg(AUDIO_APLL2_CON0, 0 << 0, 1 << 0);
	mt_afe_topck_set_reg(AUDIO_CLK_CFG_7, 0x0, 0x3);	/* select clk26m */
	mt_afe_topck_set_reg(AUDIO_CLK_CFG_7, 1 << 7, 1 << 7);	/* disable hf_haud_2_ck */
	mt_afe_pll_set_reg(AUDIO_APLL2_PWR_CON0, 1 << 1, 1 << 1);
	mt_afe_pll_set_reg(AUDIO_APLL2_PWR_CON0, 0 << 0, 1 << 0);
#endif
}

void turn_on_apll1_tuner_clock(void)
{
#if defined(COMMON_CLOCK_FRAMEWORK_API)
	int ret = clk_enable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);

	if (ret)
		pr_err("%s clk_enable %s fail %d\n",
		       __func__, aud_clks[CLOCK_INFRA_SYS_AUDIO].name, ret);

	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 19, 1 << 19);
	mt_afe_set_reg(AFE_APLL1_TUNER_CFG, 0x00008033, 0x0000FFF7);
	mt_afe_pll_set_reg(AP_PLL_CON5, 1 << 0, 1 << 0);
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 19, 1 << 19);
	mt_afe_set_reg(AFE_APLL1_TUNER_CFG, 0x00008033, 0x0000FFF7);
	mt_afe_pll_set_reg(AP_PLL_CON5, 1 << 0, 1 << 0);
#endif
}

void turn_off_apll1_tuner_clock(void)
{
#if defined(COMMON_CLOCK_FRAMEWORK_API)
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 19, 1 << 19);
	clk_disable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);
	mt_afe_pll_set_reg(AP_PLL_CON5, 0 << 0, 1 << 0);
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 19, 1 << 19);
	mt_afe_pll_set_reg(AP_PLL_CON5, 0 << 0, 1 << 0);
#endif
}

void turn_on_apll2_tuner_clock(void)
{
#if defined(COMMON_CLOCK_FRAMEWORK_API)
	int ret = clk_enable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);

	if (ret)
		pr_err("%s clk_enable %s fail %d\n",
		       __func__, aud_clks[CLOCK_INFRA_SYS_AUDIO].name, ret);

	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 18, 1 << 18);
	mt_afe_set_reg(AFE_APLL2_TUNER_CFG, 0x00008035, 0x0000FFF7);
	mt_afe_pll_set_reg(AP_PLL_CON5, 1 << 1, 1 << 1);
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 18, 1 << 18);
	mt_afe_set_reg(AFE_APLL2_TUNER_CFG, 0x00008035, 0x0000FFF7);
	mt_afe_pll_set_reg(AP_PLL_CON5, 1 << 1, 1 << 1);
#endif
}

void turn_off_apll2_tuner_clock(void)
{
#if defined(COMMON_CLOCK_FRAMEWORK_API)
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 18, 1 << 18);
	clk_disable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);
	mt_afe_pll_set_reg(AP_PLL_CON5, 0 << 1, 1 << 1);
#else
	mt_afe_set_reg(AUDIO_TOP_CON0, 1 << 18, 1 << 18);
	mt_afe_pll_set_reg(AP_PLL_CON5, 0 << 1, 1 << 1);
#endif
}

void mt_afe_apb_bus_init(void)
{
	unsigned long flags;

	spin_lock_irqsave(&afe_clk_lock, flags);
	mt_afe_set_reg(AUDIO_TOP_CON0, 0x00004000, 0x00004000);
	spin_unlock_irqrestore(&afe_clk_lock, flags);
}

int mt_afe_init_clock(void *dev)
{
	int ret = 0;
#ifdef COMMON_CLOCK_FRAMEWORK_API
	size_t i;

	for (i = 0; i < ARRAY_SIZE(aud_clks); i++) {
		aud_clks[i].clock = devm_clk_get(dev, aud_clks[i].name);
		if (IS_ERR(aud_clks[i].clock)) {
			ret = PTR_ERR(aud_clks[i].clock);
			pr_err("%s devm_clk_get %s fail %d\n", __func__, aud_clks[i].name, ret);
			break;
		}
	}

	if (ret)
		return ret;

	for (i = 0; i < ARRAY_SIZE(aud_clks); i++) {
		if (aud_clks[i].prepare_once) {
			ret = clk_prepare(aud_clks[i].clock);
			if (ret) {
				pr_err("%s clk_prepare %s fail %d\n",
				       __func__, aud_clks[i].name, ret);
				break;
			}
			aud_clks[i].is_prepared = true;
		}
	}
#endif
	return ret;
}

void mt_afe_deinit_clock(void *dev)
{
#ifdef COMMON_CLOCK_FRAMEWORK_API
	size_t i;

	pr_debug("%s\n", __func__);
	for (i = 0; i < ARRAY_SIZE(aud_clks); i++) {
		if (aud_clks[i].clock && !IS_ERR(aud_clks[i].clock) && aud_clks[i].is_prepared) {
			clk_unprepare(aud_clks[i].clock);
			aud_clks[i].is_prepared = false;
		}
	}
#endif
}

void mt_afe_power_off_default_clock(void)
{
#ifdef COMMON_CLOCK_FRAMEWORK_API
	clk_enable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);
	mt_afe_set_reg(AUDIO_TOP_CON0, 0 << 19, 0xf000b42);
	clk_disable(aud_clks[CLOCK_INFRA_SYS_AUDIO].clock);
#endif
}

void mt_afe_main_clk_on(void)
{
	unsigned long flags;

	spin_lock_irqsave(&afe_clk_lock, flags);
	pr_debug("%s aud_afe_clk_cntr:%d\n", __func__, aud_afe_clk_cntr);

	if (aud_afe_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_on_afe_clock();
#endif
	}
	aud_afe_clk_cntr++;

	spin_unlock_irqrestore(&afe_clk_lock, flags);
}

void mt_afe_main_clk_off(void)
{
	unsigned long flags;

	spin_lock_irqsave(&afe_clk_lock, flags);
	pr_debug("%s aud_afe_clk_cntr:%d\n", __func__, aud_afe_clk_cntr);

	aud_afe_clk_cntr--;
	if (aud_afe_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_off_afe_clock();
#endif
	} else if (aud_afe_clk_cntr < 0) {
		pr_err("%s aud_afe_clk_cntr:%d<0\n", __func__, aud_afe_clk_cntr);
		AUDIO_ASSERT(true);
		aud_afe_clk_cntr = 0;
	}

	spin_unlock_irqrestore(&afe_clk_lock, flags);
}

void mt_afe_dac_clk_on(void)
{
	unsigned long flags;

	spin_lock_irqsave(&afe_clk_lock, flags);
	pr_debug("%s aud_dac_clk_cntr:%d\n", __func__, aud_dac_clk_cntr);

	if (aud_dac_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_on_dac_clock();
#endif
	}
	aud_dac_clk_cntr++;

	spin_unlock_irqrestore(&afe_clk_lock, flags);
}

void mt_afe_dac_clk_off(void)
{
	unsigned long flags;

	spin_lock_irqsave(&afe_clk_lock, flags);
	pr_debug("%s aud_dac_clk_cntr:%d\n", __func__, aud_dac_clk_cntr);

	aud_dac_clk_cntr--;
	if (aud_dac_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_off_dac_clock();
#endif
	} else if (aud_dac_clk_cntr < 0) {
		pr_err("%s aud_dac_clk_cntr(%d)<0\n", __func__, aud_dac_clk_cntr);
		aud_dac_clk_cntr = 0;
	}

	spin_unlock_irqrestore(&afe_clk_lock, flags);
}

void mt_afe_adc_clk_on(void)
{
	unsigned long flags;

	spin_lock_irqsave(&afe_clk_lock, flags);
	pr_debug("%s aud_adc_clk_cntr:%d\n", __func__, aud_adc_clk_cntr);

	if (aud_adc_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_on_adc_clock();
#endif
	}
	aud_adc_clk_cntr++;

	spin_unlock_irqrestore(&afe_clk_lock, flags);
}

void mt_afe_adc_clk_off(void)
{
	unsigned long flags;

	spin_lock_irqsave(&afe_clk_lock, flags);
	pr_debug("%s aud_adc_clk_cntr:%d\n", __func__, aud_adc_clk_cntr);

	aud_adc_clk_cntr--;
	if (aud_adc_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_off_adc_clock();
#endif
	} else if (aud_adc_clk_cntr < 0) {
		pr_err("%s aud_adc_clk_cntr(%d)<0\n", __func__, aud_adc_clk_cntr);
		aud_adc_clk_cntr = 0;
	}

	spin_unlock_irqrestore(&afe_clk_lock, flags);
}

void mt_afe_i2s_clk_on(void)
{
	unsigned long flags;

	spin_lock_irqsave(&afe_clk_lock, flags);
	pr_debug("%s aud_i2s_clk_cntr:%d\n", __func__, aud_i2s_clk_cntr);

	if (aud_i2s_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_on_i2s_clock();
#endif
	}
	aud_i2s_clk_cntr++;

	spin_unlock_irqrestore(&afe_clk_lock, flags);
}

void mt_afe_i2s_clk_off(void)
{
	unsigned long flags;

	spin_lock_irqsave(&afe_clk_lock, flags);
	pr_debug("%s aud_i2s_clk_cntr:%d\n", __func__, aud_i2s_clk_cntr);

	aud_i2s_clk_cntr--;
	if (aud_i2s_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_off_i2s_clock();
#endif
	} else if (aud_i2s_clk_cntr < 0) {
		pr_err("%s aud_i2s_clk_cntr:%d<0\n", __func__, aud_i2s_clk_cntr);
		AUDIO_ASSERT(true);
		aud_i2s_clk_cntr = 0;
	}

	spin_unlock_irqrestore(&afe_clk_lock, flags);
}

void mt_afe_hdmi_clk_on(void)
{
	unsigned long flags;

	spin_lock_irqsave(&afe_clk_lock, flags);
	pr_debug("%s aud_hdmi_clk_cntr:%d\n", __func__, aud_hdmi_clk_cntr);

	if (aud_hdmi_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_on_hdmi_clock();
#endif
	}
	aud_hdmi_clk_cntr++;

	spin_unlock_irqrestore(&afe_clk_lock, flags);
}

void mt_afe_hdmi_clk_off(void)
{
	unsigned long flags;

	spin_lock_irqsave(&afe_clk_lock, flags);
	pr_debug("%s aud_hdmi_clk_cntr:%d\n", __func__, aud_hdmi_clk_cntr);

	aud_hdmi_clk_cntr--;
	if (aud_hdmi_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_off_hdmi_clock();
#endif
	} else if (aud_hdmi_clk_cntr < 0) {
		pr_err("%s aud_hdmi_clk_cntr:%d<0\n", __func__, aud_hdmi_clk_cntr);
		AUDIO_ASSERT(true);
		aud_hdmi_clk_cntr = 0;
	}

	spin_unlock_irqrestore(&afe_clk_lock, flags);
}

/*****************************************************************************
  * FUNCTION
  *  mt_afe_spdif_clk_on / mt_afe_spdif_clk_off
  *
  * DESCRIPTION
  *  Enable/Disable SPDIF clock
  *
  *****************************************************************************/

void mt_afe_spdif_clk_on(void)
{
	unsigned long flags;

	spin_lock_irqsave(&afe_clk_lock, flags);
	pr_debug("%s aud_spdif_clk_cntr:%d\n", __func__, aud_spdif_clk_cntr);

	if (aud_spdif_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_on_spdif_clock();
#endif
	}
	aud_spdif_clk_cntr++;

	spin_unlock_irqrestore(&afe_clk_lock, flags);
}

void mt_afe_spdif_clk_off(void)
{
	unsigned long flags;

	spin_lock_irqsave(&afe_clk_lock, flags);
	pr_debug("%s aud_spdif_clk_cntr:%d\n", __func__, aud_spdif_clk_cntr);

	aud_spdif_clk_cntr--;
	if (aud_spdif_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_off_spdif_clock();
#endif
	} else if (aud_spdif_clk_cntr < 0) {
		pr_err("%s aud_spdif_clk_cntr:%d<0\n", __func__, aud_spdif_clk_cntr);
		AUDIO_ASSERT(true);
		aud_spdif_clk_cntr = 0;
	}

	spin_unlock_irqrestore(&afe_clk_lock, flags);
}

void mt_afe_spdif2_clk_on(void)
{
	unsigned long flags;

	spin_lock_irqsave(&afe_clk_lock, flags);
	pr_debug("%s aud_spdif2_clk_cntr:%d\n", __func__, aud_spdif2_clk_cntr);

	if (aud_spdif2_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_on_spdif2_clock();
#endif
	}
	aud_spdif2_clk_cntr++;

	spin_unlock_irqrestore(&afe_clk_lock, flags);
}

void mt_afe_spdif2_clk_off(void)
{
	unsigned long flags;

	spin_lock_irqsave(&afe_clk_lock, flags);
	pr_debug("%s aud_spdif2_clk_cntr:%d\n", __func__, aud_spdif2_clk_cntr);

	aud_spdif2_clk_cntr--;
	if (aud_spdif2_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_off_spdif2_clock();
#endif
	} else if (aud_spdif2_clk_cntr < 0) {
		pr_err("%s aud_spdif2_clk_cntr:%d<0\n", __func__, aud_spdif2_clk_cntr);
		AUDIO_ASSERT(true);
		aud_spdif2_clk_cntr = 0;
	}

	spin_unlock_irqrestore(&afe_clk_lock, flags);
}

void mt_afe_suspend_clk_on(void)
{
#ifdef PM_MANAGER_API
	unsigned long flags;

	pr_debug("%s aud_afe_clk_cntr:%d\n", __func__, aud_afe_clk_cntr);

	spin_lock_irqsave(&afe_clk_lock, flags);

	if (aud_afe_clk_cntr > 0)
		turn_on_afe_clock();

	if (aud_i2s_clk_cntr > 0)
		turn_on_i2s_clock();

	if (aud_dac_clk_cntr > 0)
		turn_on_dac_clock();

	if (aud_adc_clk_cntr > 0)
		turn_on_adc_clock();

	if (aud_hdmi_clk_cntr > 0)
		turn_on_hdmi_clock();

	if (aud_spdif_clk_cntr > 0)
		turn_on_spdif_clock();

	if (aud_spdif2_clk_cntr > 0)
		turn_on_spdif2_clock();

	spin_unlock_irqrestore(&afe_clk_lock, flags);

	mutex_lock(&afe_clk_mutex);

	if (aud_apll22m_clk_cntr > 0)
		turn_on_apll22m_clock();

	if (aud_apll24m_clk_cntr > 0)
		turn_on_apll24m_clock();

	mutex_unlock(&afe_clk_mutex);
#endif
}

void mt_afe_suspend_clk_off(void)
{
#ifdef PM_MANAGER_API
	unsigned long flags;

	pr_debug("%s aud_afe_clk_cntr:%d\n", __func__, aud_afe_clk_cntr);

	mutex_lock(&afe_clk_mutex);

	if (aud_apll22m_clk_cntr > 0)
		turn_off_apll22m_clock();

	if (aud_apll24m_clk_cntr > 0)
		turn_off_apll24m_clock();

	mutex_unlock(&afe_clk_mutex);

	spin_lock_irqsave(&afe_clk_lock, flags);

	if (aud_i2s_clk_cntr > 0)
		turn_off_i2s_clock();

	if (aud_dac_clk_cntr > 0)
		turn_off_dac_clock();

	if (aud_adc_clk_cntr > 0)
		turn_off_adc_clock();

	if (aud_hdmi_clk_cntr > 0)
		turn_off_hdmi_clock();

	if (aud_spdif_clk_cntr > 0)
		turn_off_spdif_clock();

	if (aud_spdif2_clk_cntr > 0)
		turn_off_spdif2_clock();

	if (aud_afe_clk_cntr > 0)
		turn_off_afe_clock();

	spin_unlock_irqrestore(&afe_clk_lock, flags);
#endif
}

void mt_afe_apll24m_clk_on(void)
{
	mutex_lock(&afe_clk_mutex);
	pr_debug("%s aud_apll24m_clk_cntr:%d\n", __func__, aud_apll24m_clk_cntr);

	if (aud_apll24m_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_on_apll24m_clock();
#endif
	}
	aud_apll24m_clk_cntr++;

	mutex_unlock(&afe_clk_mutex);
}

void mt_afe_apll24m_clk_off(void)
{
	mutex_lock(&afe_clk_mutex);
	pr_debug("%s aud_apll24m_clk_cntr(%d)\n", __func__, aud_apll24m_clk_cntr);

	aud_apll24m_clk_cntr--;
	if (aud_apll24m_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_off_apll24m_clock();
#endif
	} else if (aud_apll24m_clk_cntr < 0) {
		pr_err("%s aud_apll24m_clk_cntr:%d<0\n", __func__, aud_apll24m_clk_cntr);
		aud_apll24m_clk_cntr = 0;
	}

	mutex_unlock(&afe_clk_mutex);
}

void mt_afe_apll22m_clk_on(void)
{
	mutex_lock(&afe_clk_mutex);
	pr_debug("%s aud_apll22m_clk_cntr:%d\n", __func__, aud_apll22m_clk_cntr);

	if (aud_apll22m_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_on_apll22m_clock();
#endif
	}
	aud_apll22m_clk_cntr++;

	mutex_unlock(&afe_clk_mutex);
}

void mt_afe_apll22m_clk_off(void)
{
	mutex_lock(&afe_clk_mutex);
	pr_debug("%s aud_apll22m_clk_cntr(%d)\n", __func__, aud_apll22m_clk_cntr);

	aud_apll22m_clk_cntr--;
	if (aud_apll22m_clk_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_off_apll22m_clock();
#endif
	}
	if (aud_apll22m_clk_cntr < 0) {
		pr_err("%s aud_apll22m_clk_cntr:%d<0\n", __func__, aud_apll22m_clk_cntr);
		aud_apll22m_clk_cntr = 0;
	}

	mutex_unlock(&afe_clk_mutex);
}

void mt_afe_apll1tuner_clk_on(void)
{
	mutex_lock(&afe_clk_mutex);
	pr_debug("%s aud_apll1_tuner_cntr:%d\n", __func__, aud_apll1_tuner_cntr);

	if (aud_apll1_tuner_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_on_apll1_tuner_clock();
#endif
	}
	aud_apll1_tuner_cntr++;

	mutex_unlock(&afe_clk_mutex);
}

void mt_afe_apll1tuner_clk_off(void)
{
	mutex_lock(&afe_clk_mutex);
	pr_debug("%s aud_apll1_tuner_cntr:%d\n", __func__, aud_apll1_tuner_cntr);

	aud_apll1_tuner_cntr--;
	if (aud_apll1_tuner_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_off_apll1_tuner_clock();
#endif
	} else if (aud_apll1_tuner_cntr < 0) {
		pr_err("%s aud_apll1_tuner_cntr:%d<0\n", __func__, aud_apll1_tuner_cntr);
		aud_apll1_tuner_cntr = 0;
	}

	mutex_unlock(&afe_clk_mutex);
}

void mt_afe_apll2tuner_clk_on(void)
{
	mutex_lock(&afe_clk_mutex);
	pr_debug("%s aud_apll2_tuner_cntr:%d\n", __func__, aud_apll2_tuner_cntr);

	if (aud_apll2_tuner_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_on_apll2_tuner_clock();
#endif
	}
	aud_apll2_tuner_cntr++;

	mutex_unlock(&afe_clk_mutex);
}

void mt_afe_apll2tuner_clk_off(void)
{
	mutex_lock(&afe_clk_mutex);
	pr_debug("%s aud_apll2_tuner_cntr:%d\n", __func__, aud_apll2_tuner_cntr);

	aud_apll2_tuner_cntr--;
	if (aud_apll2_tuner_cntr == 0) {
#ifdef PM_MANAGER_API
		turn_off_apll2_tuner_clock();
#endif
	} else if (aud_apll2_tuner_cntr < 0) {
		pr_err("%s aud_apll1_tuner_cntr:%d<0\n", __func__, aud_apll2_tuner_cntr);
		aud_apll2_tuner_cntr = 0;
	}

	mutex_unlock(&afe_clk_mutex);
}

void mt_afe_emi_clk_on(void)
{
#ifdef IDLE_TASK_DRIVER_API
	mutex_lock(&emi_clk_mutex);
	if (aud_emi_clk_cntr == 0) {
		disable_dpidle_by_bit(MT_CG_AUDIO_AFE);
		disable_soidle_by_bit(MT_CG_AUDIO_AFE);
	}
	aud_emi_clk_cntr++;
	mutex_unlock(&emi_clk_mutex);
#endif
}

void mt_afe_emi_clk_off(void)
{
#ifdef IDLE_TASK_DRIVER_API
	mutex_lock(&emi_clk_mutex);
	aud_emi_clk_cntr--;
	if (aud_emi_clk_cntr == 0) {
		enable_dpidle_by_bit(MT_CG_AUDIO_AFE);
		enable_soidle_by_bit(MT_CG_AUDIO_AFE);
	} else if (aud_emi_clk_cntr < 0) {
		pr_err("%s aud_emi_clk_cntr:%d<0\n", __func__, aud_emi_clk_cntr);
		aud_emi_clk_cntr = 0;
	}
	mutex_unlock(&emi_clk_mutex);
#endif
}

void mt_afe_bus_clk_boost(void)
{
#ifdef MT_DCM_API
	mutex_lock(&bus_clk_mutex);
	if (aud_bus_clk_boost_cntr == 0)
		bus_dcm_set_freq_div(BUS_DCM_FREQ_DIV_16, BUS_DCM_AUDIO);

	aud_bus_clk_boost_cntr++;
	mutex_unlock(&bus_clk_mutex);
#endif
}

void mt_afe_bus_clk_restore(void)
{
#ifdef MT_DCM_API
	mutex_lock(&bus_clk_mutex);
	aud_bus_clk_boost_cntr--;
	if (aud_bus_clk_boost_cntr == 0) {
		bus_dcm_set_freq_div(BUS_DCM_FREQ_DEFAULT, BUS_DCM_AUDIO);
	} else if (aud_bus_clk_boost_cntr < 0) {
		pr_err("%s aud_bus_clk_boost_cntr:%d<0\n", __func__, aud_bus_clk_boost_cntr);
		aud_bus_clk_boost_cntr = 0;
	}
	mutex_unlock(&bus_clk_mutex);
#endif
}

