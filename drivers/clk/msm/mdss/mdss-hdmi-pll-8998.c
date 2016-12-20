/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/iopoll.h>
#include <linux/clk/msm-clk-provider.h>
#include <linux/clk/msm-clk.h>
#include <linux/clk/msm-clock-generic.h>
#include <dt-bindings/clock/msm-clocks-8998.h>
#include <linux/math64.h>

#include "mdss-pll.h"
#include "mdss-hdmi-pll.h"

#define _W(x, y, z) MDSS_PLL_REG_W(x, y, z)
#define _R(x, y)    MDSS_PLL_REG_R(x, y)

/* PLL REGISTERS */
#define FREQ_UPDATE                  (0x008)
#define BIAS_EN_CLKBUFLR_EN          (0x034)
#define CLK_ENABLE1                  (0x038)
#define SYS_CLK_CTRL                 (0x03C)
#define SYSCLK_BUF_ENABLE            (0x040)
#define PLL_IVCO                     (0x048)
#define CP_CTRL_MODE0                (0x060)
#define PLL_RCTRL_MODE0              (0x068)
#define PLL_CCTRL_MODE0              (0x070)
#define SYSCLK_EN_SEL                (0x080)
#define RESETSM_CNTRL                (0x088)
#define LOCK_CMP_EN                  (0x090)
#define LOCK_CMP1_MODE0              (0x098)
#define LOCK_CMP2_MODE0              (0x09C)
#define LOCK_CMP3_MODE0              (0x0A0)
#define DEC_START_MODE0              (0x0B0)
#define DIV_FRAC_START1_MODE0        (0x0B8)
#define DIV_FRAC_START2_MODE0        (0x0BC)
#define DIV_FRAC_START3_MODE0        (0x0C0)
#define INTEGLOOP_GAIN0_MODE0        (0x0D8)
#define INTEGLOOP_GAIN1_MODE0        (0x0DC)
#define VCO_TUNE_CTRL                (0x0EC)
#define VCO_TUNE_MAP                 (0x0F0)
#define CLK_SELECT                   (0x138)
#define HSCLK_SEL                    (0x13C)
#define CORECLK_DIV_MODE0            (0x148)
#define CORE_CLK_EN                  (0x154)
#define C_READY_STATUS               (0x158)
#define SVS_MODE_CLK_SEL             (0x164)

/* Tx Channel PHY registers */
#define PHY_TX_EMP_POST1_LVL(n)              ((((n) * 0x200) + 0x400) + 0x000)
#define PHY_TX_INTERFACE_SELECT_TX_BAND(n)   ((((n) * 0x200) + 0x400) + 0x008)
#define PHY_TX_CLKBUF_TERM_ENABLE(n)         ((((n) * 0x200) + 0x400) + 0x00C)
#define PHY_TX_DRV_LVL_RES_CODE_OFFSET(n)    ((((n) * 0x200) + 0x400) + 0x014)
#define PHY_TX_DRV_LVL(n)                    ((((n) * 0x200) + 0x400) + 0x018)
#define PHY_TX_LANE_CONFIG(n)                ((((n) * 0x200) + 0x400) + 0x01C)
#define PHY_TX_PRE_DRIVER_1(n)               ((((n) * 0x200) + 0x400) + 0x024)
#define PHY_TX_PRE_DRIVER_2(n)               ((((n) * 0x200) + 0x400) + 0x028)
#define PHY_TX_LANE_MODE(n)                  ((((n) * 0x200) + 0x400) + 0x02C)

/* HDMI PHY registers */
#define PHY_CFG                      (0x00)
#define PHY_PD_CTL                   (0x04)
#define PHY_MODE                     (0x10)
#define PHY_CLOCK                    (0x5C)
#define PHY_CMN_CTRL                 (0x68)
#define PHY_STATUS                   (0xB4)

#define HDMI_VCO_MAX_FREQ			12000000000
#define HDMI_VCO_MIN_FREQ			8000000000
#define HDMI_BIT_CLK_TO_PIX_CLK_RATIO		10
#define HDMI_MHZ_TO_HZ				1000000
#define HDMI_HZ_TO_MHZ				1000000
#define HDMI_KHZ_TO_HZ				1000
#define HDMI_REF_CLOCK_MHZ			19.2
#define HDMI_REF_CLOCK_HZ			(HDMI_REF_CLOCK_MHZ * 1000000)
#define HDMI_VCO_MIN_RATE_HZ			25000000
#define HDMI_VCO_MAX_RATE_HZ			600000000

struct hdmi_8998_reg_cfg {
	u32 tx_band;
	u32 svs_mode_clk_sel;
	u32 hsclk_sel;
	u32 lock_cmp_en;
	u32 cctrl_mode0;
	u32 rctrl_mode0;
	u32 cpctrl_mode0;
	u32 dec_start_mode0;
	u32 div_frac_start1_mode0;
	u32 div_frac_start2_mode0;
	u32 div_frac_start3_mode0;
	u32 integloop_gain0_mode0;
	u32 integloop_gain1_mode0;
	u32 lock_cmp1_mode0;
	u32 lock_cmp2_mode0;
	u32 lock_cmp3_mode0;
	u32 ssc_per1;
	u32 ssc_per2;
	u32 ssc_step_size1;
	u32 ssc_step_size2;
	u32 core_clk_en;
	u32 coreclk_div_mode0;
	u32 phy_mode;
	u64 vco_freq;
	u32 hsclk_divsel;
	u32 vco_ratio;
	u32 ssc_en_center;

	u32 l0_tx_drv_lvl;
	u32 l0_tx_emp_post1_lvl;
	u32 l1_tx_drv_lvl;
	u32 l1_tx_emp_post1_lvl;
	u32 l2_tx_drv_lvl;
	u32 l2_tx_emp_post1_lvl;
	u32 l3_tx_drv_lvl;
	u32 l3_tx_emp_post1_lvl;

	u32 l0_pre_driver_1;
	u32 l0_pre_driver_2;
	u32 l1_pre_driver_1;
	u32 l1_pre_driver_2;
	u32 l2_pre_driver_1;
	u32 l2_pre_driver_2;
	u32 l3_pre_driver_1;
	u32 l3_pre_driver_2;

	bool debug;
};

static void hdmi_8998_get_div(struct hdmi_8998_reg_cfg *cfg, unsigned long pclk)
{
	u32 const ratio_list[] = {1, 2, 3, 4, 5, 6,
				     9, 10, 12, 15, 25};
	u32 const band_list[] = {0, 1, 2, 3};
	u32 const sz_ratio = ARRAY_SIZE(ratio_list);
	u32 const sz_band = ARRAY_SIZE(band_list);
	u32 const cmp_cnt = 1024;
	u32 const th_min = 500, th_max = 1000;
	u32 half_rate_mode = 0;
	u32 list_elements;
	int optimal_index;
	u32 i, j, k;
	u32 found_hsclk_divsel = 0, found_vco_ratio;
	u32 found_tx_band_sel;
	u64 const min_freq = HDMI_VCO_MIN_FREQ, max_freq = HDMI_VCO_MAX_FREQ;
	u64 const bit_clk = ((u64)pclk) * HDMI_BIT_CLK_TO_PIX_CLK_RATIO;
	u64 freq_list[sz_ratio * sz_band];
	u64 found_vco_freq;
	u64 freq_optimal;

find_optimal_index:
	freq_optimal = max_freq;
	optimal_index = -1;
	list_elements = 0;

	for (i = 0; i < sz_ratio; i++) {
		for (j = 0; j < sz_band; j++) {
			u64 freq = div_u64(bit_clk, (1 << half_rate_mode));

			freq *= (ratio_list[i] * (1 << band_list[j]));
			freq_list[list_elements++] = freq;
		}
	}

	for (k = 0; k < ARRAY_SIZE(freq_list); k++) {
		u32 const clks_pll_div = 2, core_clk_div = 5;
		u32 const rng1 = 16, rng2 = 8;
		u32 th1, th2;
		u64 core_clk, rvar1, rem;

		core_clk = (((freq_list[k] /
			      ratio_list[k / sz_band]) /
			      clks_pll_div) / core_clk_div);

		rvar1 = HDMI_REF_CLOCK_HZ * rng1 * HDMI_MHZ_TO_HZ;
		rvar1 = div64_u64_rem(rvar1, (cmp_cnt * core_clk), &rem);
		if (rem > ((cmp_cnt * core_clk) >> 1))
			rvar1++;
		th1 = rvar1;

		rvar1 = HDMI_REF_CLOCK_HZ * rng2 * HDMI_MHZ_TO_HZ;
		rvar1 = div64_u64_rem(rvar1, (cmp_cnt * core_clk), &rem);
		if (rem > ((cmp_cnt * core_clk) >> 1))
			rvar1++;
		th2 = rvar1;

		if (freq_list[k] >= min_freq &&
				freq_list[k] <= max_freq) {
			if ((th1 >= th_min && th1 <= th_max) ||
					(th2 >= th_min && th2 <= th_max)) {
				if (freq_list[k] <= freq_optimal) {
					freq_optimal = freq_list[k];
					optimal_index = k;
				}
			}
		}
	}

	if (optimal_index == -1) {
		if (!half_rate_mode) {
			half_rate_mode = 1;
			goto find_optimal_index;
		} else {
			/* set to default values */
			found_vco_freq = max_freq;
			found_hsclk_divsel = 0;
			found_vco_ratio = 2;
			found_tx_band_sel = 0;
			pr_err("Config error for pclk %ld\n", pclk);
		}
	} else {
		found_vco_ratio = ratio_list[optimal_index / sz_band];
		found_tx_band_sel = band_list[optimal_index % sz_band];
		found_vco_freq = freq_optimal;
	}

	switch (found_vco_ratio) {
	case 1:
		found_hsclk_divsel = 15;
		break;
	case 2:
		found_hsclk_divsel = 0;
		break;
	case 3:
		found_hsclk_divsel = 4;
		break;
	case 4:
		found_hsclk_divsel = 8;
		break;
	case 5:
		found_hsclk_divsel = 12;
		break;
	case 6:
		found_hsclk_divsel = 1;
		break;
	case 9:
		found_hsclk_divsel = 5;
		break;
	case 10:
		found_hsclk_divsel = 2;
		break;
	case 12:
		found_hsclk_divsel = 9;
		break;
	case 15:
		found_hsclk_divsel = 13;
		break;
	case 25:
		found_hsclk_divsel = 14;
		break;
	};

	pr_debug("found_vco_freq=%llu\n", found_vco_freq);
	pr_debug("found_hsclk_divsel=%d\n", found_hsclk_divsel);
	pr_debug("found_vco_ratio=%d\n", found_vco_ratio);
	pr_debug("found_tx_band_sel=%d\n", found_tx_band_sel);
	pr_debug("half_rate_mode=%d\n", half_rate_mode);
	pr_debug("optimal_index=%d\n", optimal_index);

	cfg->vco_freq = found_vco_freq;
	cfg->hsclk_divsel = found_hsclk_divsel;
	cfg->vco_ratio = found_vco_ratio;
	cfg->tx_band = found_tx_band_sel;
}

static int hdmi_8998_config_phy(unsigned long rate,
		struct hdmi_8998_reg_cfg *cfg)
{
	u64 const high_freq_bit_clk_threshold = 3400000000UL;
	u64 const dig_freq_bit_clk_threshold = 1500000000UL;
	u64 const mid_freq_bit_clk_threshold = 750000000;
	int rc = 0;
	u64 fdata, tmds_clk;
	u32 pll_div = 4 * HDMI_REF_CLOCK_HZ;
	u64 bclk;
	u64 vco_freq;
	u64 hsclk_sel, dec_start, div_frac_start;
	u64 rem;
	u64 cpctrl, rctrl, cctrl;
	u64 integloop_gain;
	u32 digclk_divsel;
	u32 tmds_bclk_ratio;
	u64 cmp_rng, cmp_cnt = 1024, pll_cmp;
	bool gen_ssc = false;

	bclk = rate * HDMI_BIT_CLK_TO_PIX_CLK_RATIO;

	if (bclk > high_freq_bit_clk_threshold) {
		tmds_clk = rate / 4;
		tmds_bclk_ratio = 1;
	} else {
		tmds_clk = rate;
		tmds_bclk_ratio = 0;
	}

	hdmi_8998_get_div(cfg, rate);

	vco_freq = cfg->vco_freq;
	fdata = cfg->vco_freq;
	do_div(fdata, cfg->vco_ratio);

	hsclk_sel = cfg->hsclk_divsel;
	dec_start = vco_freq;
	do_div(dec_start, pll_div);

	div_frac_start = vco_freq * (1 << 20);
	rem = do_div(div_frac_start, pll_div);
	div_frac_start -= (dec_start * (1 << 20));
	if (rem > (pll_div >> 1))
		div_frac_start++;

	if ((div_frac_start != 0) || (gen_ssc == true)) {
		cpctrl = 0x8;
		rctrl = 0x16;
		cctrl = 0x34;
	} else {
		cpctrl = 0x30;
		rctrl = 0x18;
		cctrl = 0x2;
	}

	digclk_divsel = (bclk > dig_freq_bit_clk_threshold) ? 0x1 : 0x2;

	integloop_gain = ((div_frac_start != 0) ||
			(gen_ssc == true)) ? 0x3F : 0xC4;
	integloop_gain <<= (digclk_divsel == 2 ? 1 : 0);
	integloop_gain = (integloop_gain <= 2046 ? integloop_gain : 0x7FE);

	cmp_rng = gen_ssc ? 0x40 : 0x10;

	pll_cmp = cmp_cnt * fdata;
	rem = do_div(pll_cmp, (u32)(HDMI_REF_CLOCK_HZ * 10));
	if (rem > ((u64)(HDMI_REF_CLOCK_HZ * 10) >> 1))
		pll_cmp++;

	pll_cmp =  pll_cmp - 1;

	pr_debug("VCO_FREQ = %llu\n", cfg->vco_freq);
	pr_debug("FDATA = %llu\n", fdata);
	pr_debug("DEC_START = %llu\n", dec_start);
	pr_debug("DIV_FRAC_START = %llu\n", div_frac_start);
	pr_debug("CPCTRL = %llu\n", cpctrl);
	pr_debug("RCTRL = %llu\n", rctrl);
	pr_debug("CCTRL = %llu\n", cctrl);
	pr_debug("DIGCLK_DIVSEL = %u\n", digclk_divsel);
	pr_debug("INTEGLOOP_GAIN = %llu\n", integloop_gain);
	pr_debug("CMP_RNG = %llu\n", cmp_rng);
	pr_debug("PLL_CMP = %llu\n", pll_cmp);

	cfg->svs_mode_clk_sel = (digclk_divsel & 0xFF);
	cfg->hsclk_sel = (0x20 | hsclk_sel);
	cfg->lock_cmp_en = (gen_ssc ? 0x4 : 0x0);
	cfg->cctrl_mode0 = (cctrl & 0xFF);
	cfg->rctrl_mode0 = (rctrl & 0xFF);
	cfg->cpctrl_mode0 = (cpctrl & 0xFF);
	cfg->dec_start_mode0 = (dec_start & 0xFF);
	cfg->div_frac_start1_mode0 = (div_frac_start & 0xFF);
	cfg->div_frac_start2_mode0 = ((div_frac_start & 0xFF00) >> 8);
	cfg->div_frac_start3_mode0 = ((div_frac_start & 0xF0000) >> 16);
	cfg->integloop_gain0_mode0 = (integloop_gain & 0xFF);
	cfg->integloop_gain1_mode0 = (integloop_gain & 0xF00) >> 8;
	cfg->lock_cmp1_mode0 = (pll_cmp & 0xFF);
	cfg->lock_cmp2_mode0 = ((pll_cmp & 0xFF00) >> 8);
	cfg->lock_cmp3_mode0 = ((pll_cmp & 0x30000) >> 16);
	cfg->ssc_per1 = 0;
	cfg->ssc_per2 = 0;
	cfg->ssc_step_size1 = 0;
	cfg->ssc_step_size2 = 0;
	cfg->core_clk_en = 0x2C;
	cfg->coreclk_div_mode0 = 0x5;
	cfg->phy_mode = (tmds_bclk_ratio ? 0x5 : 0x4);
	cfg->ssc_en_center = 0x0;

	if (bclk > high_freq_bit_clk_threshold) {
		cfg->l0_tx_drv_lvl = 0xA;
		cfg->l0_tx_emp_post1_lvl = 0x3;
		cfg->l1_tx_drv_lvl = 0xA;
		cfg->l1_tx_emp_post1_lvl = 0x3;
		cfg->l2_tx_drv_lvl = 0xA;
		cfg->l2_tx_emp_post1_lvl = 0x3;
		cfg->l3_tx_drv_lvl = 0x8;
		cfg->l3_tx_emp_post1_lvl = 0x3;
		cfg->l0_pre_driver_1 = 0x0;
		cfg->l0_pre_driver_2 = 0x1C;
		cfg->l1_pre_driver_1 = 0x0;
		cfg->l1_pre_driver_2 = 0x1C;
		cfg->l2_pre_driver_1 = 0x0;
		cfg->l2_pre_driver_2 = 0x1C;
		cfg->l3_pre_driver_1 = 0x0;
		cfg->l3_pre_driver_2 = 0x0;
	} else if (bclk > dig_freq_bit_clk_threshold) {
		cfg->l0_tx_drv_lvl = 0x9;
		cfg->l0_tx_emp_post1_lvl = 0x3;
		cfg->l1_tx_drv_lvl = 0x9;
		cfg->l1_tx_emp_post1_lvl = 0x3;
		cfg->l2_tx_drv_lvl = 0x9;
		cfg->l2_tx_emp_post1_lvl = 0x3;
		cfg->l3_tx_drv_lvl = 0x8;
		cfg->l3_tx_emp_post1_lvl = 0x3;
		cfg->l0_pre_driver_1 = 0x0;
		cfg->l0_pre_driver_2 = 0x16;
		cfg->l1_pre_driver_1 = 0x0;
		cfg->l1_pre_driver_2 = 0x16;
		cfg->l2_pre_driver_1 = 0x0;
		cfg->l2_pre_driver_2 = 0x16;
		cfg->l3_pre_driver_1 = 0x0;
		cfg->l3_pre_driver_2 = 0x0;
	} else if (bclk > mid_freq_bit_clk_threshold) {
		cfg->l0_tx_drv_lvl = 0x9;
		cfg->l0_tx_emp_post1_lvl = 0x3;
		cfg->l1_tx_drv_lvl = 0x9;
		cfg->l1_tx_emp_post1_lvl = 0x3;
		cfg->l2_tx_drv_lvl = 0x9;
		cfg->l2_tx_emp_post1_lvl = 0x3;
		cfg->l3_tx_drv_lvl = 0x8;
		cfg->l3_tx_emp_post1_lvl = 0x3;
		cfg->l0_pre_driver_1 = 0x0;
		cfg->l0_pre_driver_2 = 0x0E;
		cfg->l1_pre_driver_1 = 0x0;
		cfg->l1_pre_driver_2 = 0x0E;
		cfg->l2_pre_driver_1 = 0x0;
		cfg->l2_pre_driver_2 = 0x0E;
		cfg->l3_pre_driver_1 = 0x0;
		cfg->l3_pre_driver_2 = 0x0;
	} else {
		cfg->l0_tx_drv_lvl = 0x0;
		cfg->l0_tx_emp_post1_lvl = 0x0;
		cfg->l1_tx_drv_lvl = 0x0;
		cfg->l1_tx_emp_post1_lvl = 0x0;
		cfg->l2_tx_drv_lvl = 0x0;
		cfg->l2_tx_emp_post1_lvl = 0x0;
		cfg->l3_tx_drv_lvl = 0x0;
		cfg->l3_tx_emp_post1_lvl = 0x0;
		cfg->l0_pre_driver_1 = 0x0;
		cfg->l0_pre_driver_2 = 0x01;
		cfg->l1_pre_driver_1 = 0x0;
		cfg->l1_pre_driver_2 = 0x01;
		cfg->l2_pre_driver_1 = 0x0;
		cfg->l2_pre_driver_2 = 0x01;
		cfg->l3_pre_driver_1 = 0x0;
		cfg->l3_pre_driver_2 = 0x0;
	}

	return rc;
}

static int hdmi_8998_pll_set_clk_rate(struct clk *c, unsigned long rate)
{
	int rc = 0;
	struct hdmi_pll_vco_clk *vco = to_hdmi_vco_clk(c);
	struct mdss_pll_resources *io = vco->priv;
	struct hdmi_8998_reg_cfg cfg = {0};
	void __iomem *phy = io->phy_base, *pll = io->pll_base;

	rc = hdmi_8998_config_phy(rate, &cfg);
	if (rc) {
		pr_err("rate calculation failed\n, rc=%d", rc);
		return rc;
	}

	_W(phy, PHY_PD_CTL, 0x0);
	udelay(500);

	_W(phy, PHY_PD_CTL, 0x1);
	_W(pll, RESETSM_CNTRL, 0x20);
	_W(phy, PHY_CMN_CTRL, 0x6);
	_W(pll, PHY_TX_INTERFACE_SELECT_TX_BAND(0), cfg.tx_band);
	_W(pll, PHY_TX_INTERFACE_SELECT_TX_BAND(1), cfg.tx_band);
	_W(pll, PHY_TX_INTERFACE_SELECT_TX_BAND(2), cfg.tx_band);
	_W(pll, PHY_TX_INTERFACE_SELECT_TX_BAND(3), cfg.tx_band);
	_W(pll, PHY_TX_CLKBUF_TERM_ENABLE(0), 0x1);
	_W(pll, PHY_TX_LANE_MODE(0), 0x20);
	_W(pll, PHY_TX_LANE_MODE(1), 0x20);
	_W(pll, PHY_TX_LANE_MODE(2), 0x20);
	_W(pll, PHY_TX_LANE_MODE(3), 0x20);
	_W(pll, PHY_TX_CLKBUF_TERM_ENABLE(1), 0x1);
	_W(pll, PHY_TX_CLKBUF_TERM_ENABLE(2), 0x1);
	_W(pll, PHY_TX_CLKBUF_TERM_ENABLE(3), 0x1);
	_W(pll, SYSCLK_BUF_ENABLE, 0x2);
	_W(pll, BIAS_EN_CLKBUFLR_EN, 0xB);
	_W(pll, SYSCLK_EN_SEL, 0x37);
	_W(pll, SYS_CLK_CTRL, 0x2);
	_W(pll, CLK_ENABLE1, 0xE);
	_W(pll, PLL_IVCO, 0x7);
	_W(pll, VCO_TUNE_CTRL, 0x0);
	_W(pll, SVS_MODE_CLK_SEL, cfg.svs_mode_clk_sel);
	_W(pll, CLK_SELECT, 0x30);
	_W(pll, HSCLK_SEL, cfg.hsclk_sel);
	_W(pll, LOCK_CMP_EN, cfg.lock_cmp_en);
	_W(pll, PLL_CCTRL_MODE0, cfg.cctrl_mode0);
	_W(pll, PLL_RCTRL_MODE0, cfg.rctrl_mode0);
	_W(pll, CP_CTRL_MODE0, cfg.cpctrl_mode0);
	_W(pll, DEC_START_MODE0, cfg.dec_start_mode0);
	_W(pll, DIV_FRAC_START1_MODE0, cfg.div_frac_start1_mode0);
	_W(pll, DIV_FRAC_START2_MODE0, cfg.div_frac_start2_mode0);
	_W(pll, DIV_FRAC_START3_MODE0, cfg.div_frac_start3_mode0);
	_W(pll, INTEGLOOP_GAIN0_MODE0, cfg.integloop_gain0_mode0);
	_W(pll, INTEGLOOP_GAIN1_MODE0, cfg.integloop_gain1_mode0);
	_W(pll, LOCK_CMP1_MODE0, cfg.lock_cmp1_mode0);
	_W(pll, LOCK_CMP2_MODE0, cfg.lock_cmp2_mode0);
	_W(pll, LOCK_CMP3_MODE0, cfg.lock_cmp3_mode0);
	_W(pll, VCO_TUNE_MAP, 0x0);
	_W(pll, CORE_CLK_EN, cfg.core_clk_en);
	_W(pll, CORECLK_DIV_MODE0, cfg.coreclk_div_mode0);

	_W(pll, PHY_TX_DRV_LVL(0), cfg.l0_tx_drv_lvl);
	_W(pll, PHY_TX_DRV_LVL(1), cfg.l1_tx_drv_lvl);
	_W(pll, PHY_TX_DRV_LVL(2), cfg.l2_tx_drv_lvl);
	_W(pll, PHY_TX_DRV_LVL(3), cfg.l3_tx_drv_lvl);

	_W(pll, PHY_TX_EMP_POST1_LVL(0), cfg.l0_tx_emp_post1_lvl);
	_W(pll, PHY_TX_EMP_POST1_LVL(1), cfg.l1_tx_emp_post1_lvl);
	_W(pll, PHY_TX_EMP_POST1_LVL(2), cfg.l2_tx_emp_post1_lvl);
	_W(pll, PHY_TX_EMP_POST1_LVL(3), cfg.l3_tx_emp_post1_lvl);

	_W(pll, PHY_TX_PRE_DRIVER_1(0), cfg.l0_pre_driver_1);
	_W(pll, PHY_TX_PRE_DRIVER_1(1), cfg.l1_pre_driver_1);
	_W(pll, PHY_TX_PRE_DRIVER_1(2), cfg.l2_pre_driver_1);
	_W(pll, PHY_TX_PRE_DRIVER_1(3), cfg.l3_pre_driver_1);

	_W(pll, PHY_TX_PRE_DRIVER_2(0), cfg.l0_pre_driver_2);
	_W(pll, PHY_TX_PRE_DRIVER_2(1), cfg.l1_pre_driver_2);
	_W(pll, PHY_TX_PRE_DRIVER_2(2), cfg.l2_pre_driver_2);
	_W(pll, PHY_TX_PRE_DRIVER_2(3), cfg.l3_pre_driver_2);

	_W(pll, PHY_TX_DRV_LVL_RES_CODE_OFFSET(0), 0x3);
	_W(pll, PHY_TX_DRV_LVL_RES_CODE_OFFSET(1), 0x0);
	_W(pll, PHY_TX_DRV_LVL_RES_CODE_OFFSET(2), 0x0);
	_W(pll, PHY_TX_DRV_LVL_RES_CODE_OFFSET(3), 0x3);

	_W(phy, PHY_MODE, cfg.phy_mode);

	_W(pll, PHY_TX_LANE_CONFIG(0), 0x10);
	_W(pll, PHY_TX_LANE_CONFIG(1), 0x10);
	_W(pll, PHY_TX_LANE_CONFIG(2), 0x10);
	_W(pll, PHY_TX_LANE_CONFIG(3), 0x10);

	/* Ensure all registers are flushed to hardware */
	wmb();

	return 0;
}

static int hdmi_8998_pll_lock_status(struct mdss_pll_resources *io)
{
	u32 const delay_us = 100;
	u32 const timeout_us = 5000;
	u32 status;
	int rc = 0;
	void __iomem *pll = io->pll_base;

	rc = mdss_pll_resource_enable(io, true);
	if (rc) {
		pr_err("pll resource can't be enabled\n");
		return rc;
	}
	rc = readl_poll_timeout_atomic(pll + C_READY_STATUS,
			status,
			((status & BIT(0)) > 0),
			delay_us,
			timeout_us);
	if (rc)
		pr_err("HDMI PLL(%d) lock failed, status=0x%08x\n",
				io->index, status);
	else
		pr_debug("HDMI PLL(%d) lock passed, status=0x%08x\n",
				io->index, status);

	mdss_pll_resource_enable(io, false);

	return rc;
}

static int hdmi_8998_phy_ready_status(struct mdss_pll_resources *io)
{
	u32 const delay_us = 100;
	u32 const timeout_us = 5000;
	u32 status;
	int rc = 0;
	void __iomem *phy = io->phy_base;

	rc = mdss_pll_resource_enable(io, true);
	if (rc) {
		pr_err("pll resource can't be enabled\n");
		return rc;
	}

	rc = readl_poll_timeout_atomic(phy + PHY_STATUS,
			status,
			((status & BIT(0)) > 0),
			delay_us,
			timeout_us);
	if (rc)
		pr_err("HDMI PHY(%d) not ready, status=0x%08x\n",
				io->index, status);
	else
		pr_debug("HDMI PHY(%d) ready, status=0x%08x\n",
				io->index, status);

	mdss_pll_resource_enable(io, false);

	return rc;
}

static int hdmi_8998_pll_enable(struct clk *c)
{
	int rc = 0;
	struct hdmi_pll_vco_clk *vco = to_hdmi_vco_clk(c);
	struct mdss_pll_resources *io = vco->priv;
	void __iomem *phy = io->phy_base, *pll = io->pll_base;

	_W(phy, PHY_CFG, 0x1);
	udelay(100);
	_W(phy, PHY_CFG, 0x59);
	udelay(100);

	/* Ensure all registers are flushed to hardware */
	wmb();

	rc = hdmi_8998_pll_lock_status(io);
	if (rc) {
		pr_err("PLL not locked, rc=%d\n", rc);
		return rc;
	}

	_W(pll, PHY_TX_LANE_CONFIG(0), 0x1F);
	_W(pll, PHY_TX_LANE_CONFIG(1), 0x1F);
	_W(pll, PHY_TX_LANE_CONFIG(2), 0x1F);
	_W(pll, PHY_TX_LANE_CONFIG(3), 0x1F);

	/* Ensure all registers are flushed to hardware */
	wmb();

	rc = hdmi_8998_phy_ready_status(io);
	if (rc) {
		pr_err("PHY NOT READY, rc=%d\n", rc);
		return rc;
	}

	_W(phy, PHY_CFG, 0x58);
	udelay(1);
	_W(phy, PHY_CFG, 0x59);

	/* Ensure all registers are flushed to hardware */
	wmb();

	io->pll_on = true;
	return rc;
}

/*
 * Get the clock range allowed in atomic update. If clock rate
 * goes beyond this range, a full tear down is required to set
 * the new pixel clock.
 */
static int hdmi_8998_vco_get_lock_range(struct clk *c,
	unsigned long pixel_clk)
{
	const u32 rng = 64, cmp_cnt = 1024;
	const u32 coreclk_div = 5, clks_pll_divsel = 2;
	u32 vco_freq, vco_ratio, ppm_range;
	struct hdmi_8998_reg_cfg cfg = {0};

	pr_debug("rate=%ld\n", pixel_clk);

	hdmi_8998_get_div(&cfg, pixel_clk);
	if (cfg.vco_ratio <= 0 || cfg.vco_freq <= 0) {
		pr_err("couldn't get post div\n");
		return -EINVAL;
	}

	do_div(cfg.vco_freq, HDMI_KHZ_TO_HZ * HDMI_KHZ_TO_HZ);

	vco_freq  = (u32) cfg.vco_freq;
	vco_ratio = (u32) cfg.vco_ratio;

	pr_debug("freq %d, ratio %d\n", vco_freq, vco_ratio);

	ppm_range = (rng * HDMI_REF_CLOCK_HZ) / cmp_cnt;
	ppm_range /= vco_freq / vco_ratio;
	ppm_range *= coreclk_div * clks_pll_divsel;

	pr_debug("ppm range: %d\n", ppm_range);

	return ppm_range;
}

static int hdmi_8998_vco_rate_atomic_update(struct clk *c,
	unsigned long rate)
{
	struct hdmi_pll_vco_clk *vco = to_hdmi_vco_clk(c);
	struct mdss_pll_resources *io = vco->priv;
	void __iomem *pll;
	struct hdmi_8998_reg_cfg cfg = {0};
	int rc = 0;

	rc = hdmi_8998_config_phy(rate, &cfg);
	if (rc) {
		pr_err("rate calculation failed\n, rc=%d", rc);
		goto end;
	}

	pll = io->pll_base;

	_W(pll, DEC_START_MODE0, cfg.dec_start_mode0);
	_W(pll, DIV_FRAC_START1_MODE0, cfg.div_frac_start1_mode0);
	_W(pll, DIV_FRAC_START2_MODE0, cfg.div_frac_start2_mode0);
	_W(pll, DIV_FRAC_START3_MODE0, cfg.div_frac_start3_mode0);

	_W(pll, FREQ_UPDATE, 0x01);
	_W(pll, FREQ_UPDATE, 0x00);

	pr_debug("updated to rate %ld\n", rate);
end:
	return rc;
}

static int hdmi_8998_vco_set_rate(struct clk *c, unsigned long rate)
{
	struct hdmi_pll_vco_clk *vco = to_hdmi_vco_clk(c);
	struct mdss_pll_resources *io = vco->priv;
	unsigned int set_power_dwn = 0;
	bool atomic_update = false;
	int pll_lock_range = 0;
	int rc = 0;

	rc = mdss_pll_resource_enable(io, true);
	if (rc) {
		pr_err("pll resource enable failed, rc=%d\n", rc);
		return rc;
	}

	pr_debug("rate %ld, vco_rate %ld\n", rate, vco->rate);

	if (_R(io->pll_base, C_READY_STATUS) & BIT(0) &&
		_R(io->phy_base, PHY_STATUS) & BIT(0)) {
		pll_lock_range = hdmi_8998_vco_get_lock_range(c, vco->rate);

		if (pll_lock_range > 0 && vco->rate) {
			u32 range_limit;

			range_limit  = pll_lock_range *
				(vco->rate / HDMI_KHZ_TO_HZ);
			range_limit /= HDMI_KHZ_TO_HZ;

			pr_debug("range limit %d\n", range_limit);

			if (abs(rate - vco->rate) < range_limit)
				atomic_update = true;
		}
	}

	if (io->pll_on && !atomic_update)
		set_power_dwn = 1;

	if (atomic_update)
		rc = hdmi_8998_vco_rate_atomic_update(c, rate);
	else
		rc = hdmi_8998_pll_set_clk_rate(c, rate);

	if (rc) {
		pr_err("failed to set clk rate\n");
		goto error;
	}

	if (set_power_dwn) {
		rc = hdmi_8998_pll_enable(c);
		if (rc) {
			pr_err("failed to enable pll, rc=%d\n", rc);
			goto error;
		}
	}

	vco->rate = rate;
	vco->rate_set = true;

error:
	(void)mdss_pll_resource_enable(io, false);

	return rc;
}

static long hdmi_8998_vco_round_rate(struct clk *c, unsigned long rate)
{
	unsigned long rrate = rate;
	struct hdmi_pll_vco_clk *vco = to_hdmi_vco_clk(c);

	if (rate < vco->min_rate)
		rrate = vco->min_rate;
	if (rate > vco->max_rate)
		rrate = vco->max_rate;

	return rrate;
}

static int hdmi_8998_vco_prepare(struct clk *c)
{
	struct hdmi_pll_vco_clk *vco = to_hdmi_vco_clk(c);
	struct mdss_pll_resources *io = vco->priv;
	int rc = 0;

	if (!io) {
		pr_err("hdmi pll resources are not available\n");
		return -EINVAL;
	}

	rc = mdss_pll_resource_enable(io, true);
	if (rc) {
		pr_err("pll resource enable failed, rc=%d\n", rc);
		return rc;
	}

	if (!vco->rate_set && vco->rate) {
		rc = hdmi_8998_pll_set_clk_rate(c, vco->rate);
		if (rc) {
			pr_err("set rate failed, rc=%d\n", rc);
			goto error;
		}
	}

	rc = hdmi_8998_pll_enable(c);
	if (rc)
		pr_err("pll enabled failed, rc=%d\n", rc);

error:
	if (rc)
		mdss_pll_resource_enable(io, false);

	return rc;
}

static void hdmi_8998_pll_disable(struct hdmi_pll_vco_clk *vco)
{
	struct mdss_pll_resources *io = vco->priv;
	void __iomem *phy = io->phy_base;

	if (!io->pll_on)
		return;

	_W(phy, PHY_PD_CTL, 0x0);

	/* Ensure all registers are flushed to hardware */
	wmb();

	vco->rate_set = false;
	io->handoff_resources = false;
	io->pll_on = false;
}

static void hdmi_8998_vco_unprepare(struct clk *c)
{
	struct hdmi_pll_vco_clk *vco = to_hdmi_vco_clk(c);
	struct mdss_pll_resources *io = vco->priv;

	if (!io) {
		pr_err("HDMI pll resources not available\n");
		return;
	}

	hdmi_8998_pll_disable(vco);
	mdss_pll_resource_enable(io, false);
}

static enum handoff hdmi_8998_vco_handoff(struct clk *c)
{
	enum handoff ret = HANDOFF_DISABLED_CLK;
	struct hdmi_pll_vco_clk *vco = to_hdmi_vco_clk(c);
	struct mdss_pll_resources *io = vco->priv;

	if (mdss_pll_resource_enable(io, true)) {
		pr_err("pll resource can't be enabled\n");
		return ret;
	}

	io->handoff_resources = true;

	if (_R(io->pll_base, C_READY_STATUS) & BIT(0) &&
			_R(io->phy_base, PHY_STATUS) & BIT(0)) {
		io->pll_on = true;
		/* TODO: calculate rate based on the phy/pll register values. */
		ret = HANDOFF_ENABLED_CLK;
	} else {
		io->handoff_resources = false;
		mdss_pll_resource_enable(io, false);
		pr_debug("%s: PHY/PLL not ready\n", __func__);
	}

	pr_debug("done, ret=%d\n", ret);
	return ret;
}

static struct clk_ops hdmi_8998_vco_clk_ops = {
	.set_rate = hdmi_8998_vco_set_rate,
	.round_rate = hdmi_8998_vco_round_rate,
	.prepare = hdmi_8998_vco_prepare,
	.unprepare = hdmi_8998_vco_unprepare,
	.handoff = hdmi_8998_vco_handoff,
};

static struct hdmi_pll_vco_clk hdmi_vco_clk = {
	.min_rate = HDMI_VCO_MIN_RATE_HZ,
	.max_rate = HDMI_VCO_MAX_RATE_HZ,
	.c = {
		.dbg_name = "hdmi_8998_vco_clk",
		.ops = &hdmi_8998_vco_clk_ops,
		CLK_INIT(hdmi_vco_clk.c),
	},
};

static struct clk_lookup hdmipllcc_8998[] = {
	CLK_LIST(hdmi_vco_clk),
};

int hdmi_8998_pll_clock_register(struct platform_device *pdev,
				   struct mdss_pll_resources *pll_res)
{
	int rc = 0;

	if (!pdev || !pll_res) {
		pr_err("invalid input parameters\n");
		return -EINVAL;
	}

	hdmi_vco_clk.priv = pll_res;

	rc = of_msm_clock_register(pdev->dev.of_node, hdmipllcc_8998,
				   ARRAY_SIZE(hdmipllcc_8998));
	if (rc) {
		pr_err("clock register failed, rc=%d\n", rc);
		return rc;
	}

	return rc;
}
