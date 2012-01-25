/* Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <asm/processor.h>
#include <mach/msm_iomap.h>
#include "clock-dss-8960.h"

/* HDMI PLL macros */
#define HDMI_PHY_PLL_REFCLK_CFG          (MSM_HDMI_BASE + 0x00000500)
#define HDMI_PHY_PLL_CHRG_PUMP_CFG       (MSM_HDMI_BASE + 0x00000504)
#define HDMI_PHY_PLL_LOOP_FLT_CFG0       (MSM_HDMI_BASE + 0x00000508)
#define HDMI_PHY_PLL_LOOP_FLT_CFG1       (MSM_HDMI_BASE + 0x0000050c)
#define HDMI_PHY_PLL_IDAC_ADJ_CFG        (MSM_HDMI_BASE + 0x00000510)
#define HDMI_PHY_PLL_I_VI_KVCO_CFG       (MSM_HDMI_BASE + 0x00000514)
#define HDMI_PHY_PLL_PWRDN_B             (MSM_HDMI_BASE + 0x00000518)
#define HDMI_PHY_PLL_SDM_CFG0            (MSM_HDMI_BASE + 0x0000051c)
#define HDMI_PHY_PLL_SDM_CFG1            (MSM_HDMI_BASE + 0x00000520)
#define HDMI_PHY_PLL_SDM_CFG2            (MSM_HDMI_BASE + 0x00000524)
#define HDMI_PHY_PLL_SDM_CFG3            (MSM_HDMI_BASE + 0x00000528)
#define HDMI_PHY_PLL_SDM_CFG4            (MSM_HDMI_BASE + 0x0000052c)
#define HDMI_PHY_PLL_SSC_CFG0            (MSM_HDMI_BASE + 0x00000530)
#define HDMI_PHY_PLL_SSC_CFG1            (MSM_HDMI_BASE + 0x00000534)
#define HDMI_PHY_PLL_SSC_CFG2            (MSM_HDMI_BASE + 0x00000538)
#define HDMI_PHY_PLL_SSC_CFG3            (MSM_HDMI_BASE + 0x0000053c)
#define HDMI_PHY_PLL_LOCKDET_CFG0        (MSM_HDMI_BASE + 0x00000540)
#define HDMI_PHY_PLL_LOCKDET_CFG1        (MSM_HDMI_BASE + 0x00000544)
#define HDMI_PHY_PLL_LOCKDET_CFG2        (MSM_HDMI_BASE + 0x00000548)
#define HDMI_PHY_PLL_VCOCAL_CFG0         (MSM_HDMI_BASE + 0x0000054c)
#define HDMI_PHY_PLL_VCOCAL_CFG1         (MSM_HDMI_BASE + 0x00000550)
#define HDMI_PHY_PLL_VCOCAL_CFG2         (MSM_HDMI_BASE + 0x00000554)
#define HDMI_PHY_PLL_VCOCAL_CFG3         (MSM_HDMI_BASE + 0x00000558)
#define HDMI_PHY_PLL_VCOCAL_CFG4         (MSM_HDMI_BASE + 0x0000055c)
#define HDMI_PHY_PLL_VCOCAL_CFG5         (MSM_HDMI_BASE + 0x00000560)
#define HDMI_PHY_PLL_VCOCAL_CFG6         (MSM_HDMI_BASE + 0x00000564)
#define HDMI_PHY_PLL_VCOCAL_CFG7         (MSM_HDMI_BASE + 0x00000568)
#define HDMI_PHY_PLL_DEBUG_SEL           (MSM_HDMI_BASE + 0x0000056c)
#define HDMI_PHY_PLL_MISC0               (MSM_HDMI_BASE + 0x00000570)
#define HDMI_PHY_PLL_MISC1               (MSM_HDMI_BASE + 0x00000574)
#define HDMI_PHY_PLL_MISC2               (MSM_HDMI_BASE + 0x00000578)
#define HDMI_PHY_PLL_MISC3               (MSM_HDMI_BASE + 0x0000057c)
#define HDMI_PHY_PLL_MISC4               (MSM_HDMI_BASE + 0x00000580)
#define HDMI_PHY_PLL_MISC5               (MSM_HDMI_BASE + 0x00000584)
#define HDMI_PHY_PLL_MISC6               (MSM_HDMI_BASE + 0x00000588)
#define HDMI_PHY_PLL_DEBUG_BUS0          (MSM_HDMI_BASE + 0x0000058c)
#define HDMI_PHY_PLL_DEBUG_BUS1          (MSM_HDMI_BASE + 0x00000590)
#define HDMI_PHY_PLL_DEBUG_BUS2          (MSM_HDMI_BASE + 0x00000594)
#define HDMI_PHY_PLL_STATUS0             (MSM_HDMI_BASE + 0x00000598)
#define HDMI_PHY_PLL_STATUS1             (MSM_HDMI_BASE + 0x0000059c)
#define HDMI_PHY_CTRL                    (MSM_HDMI_BASE + 0x000002D4)
#define HDMI_PHY_REG_0                   (MSM_HDMI_BASE + 0x00000400)
#define HDMI_PHY_REG_1                   (MSM_HDMI_BASE + 0x00000404)
#define HDMI_PHY_REG_2                   (MSM_HDMI_BASE + 0x00000408)
#define HDMI_PHY_REG_3                   (MSM_HDMI_BASE + 0x0000040c)
#define HDMI_PHY_REG_4                   (MSM_HDMI_BASE + 0x00000410)
#define HDMI_PHY_REG_5                   (MSM_HDMI_BASE + 0x00000414)
#define HDMI_PHY_REG_6                   (MSM_HDMI_BASE + 0x00000418)
#define HDMI_PHY_REG_7                   (MSM_HDMI_BASE + 0x0000041c)
#define HDMI_PHY_REG_8                   (MSM_HDMI_BASE + 0x00000420)
#define HDMI_PHY_REG_9                   (MSM_HDMI_BASE + 0x00000424)
#define HDMI_PHY_REG_10                  (MSM_HDMI_BASE + 0x00000428)
#define HDMI_PHY_REG_11                  (MSM_HDMI_BASE + 0x0000042c)
#define HDMI_PHY_REG_12                  (MSM_HDMI_BASE + 0x00000430)
#define HDMI_PHY_REG_BIST_CFG            (MSM_HDMI_BASE + 0x00000434)
#define HDMI_PHY_DEBUG_BUS_SEL           (MSM_HDMI_BASE + 0x00000438)
#define HDMI_PHY_REG_MISC0               (MSM_HDMI_BASE + 0x0000043c)
#define HDMI_PHY_REG_13                  (MSM_HDMI_BASE + 0x00000440)
#define HDMI_PHY_REG_14                  (MSM_HDMI_BASE + 0x00000444)
#define HDMI_PHY_REG_15                  (MSM_HDMI_BASE + 0x00000448)

#define AHB_EN_REG                       (MSM_MMSS_CLK_CTL_BASE + 0x0008)

/* HDMI PHY/PLL bit field macros */
#define SW_RESET BIT(2)
#define SW_RESET_PLL BIT(0)
#define PWRDN_B BIT(7)

#define PLL_PWRDN_B BIT(3)
#define PD_PLL BIT(1)

static unsigned current_rate;
static unsigned hdmi_pll_on;

int hdmi_pll_enable(void)
{
	unsigned int val;
	u32 ahb_en_reg, ahb_enabled;

	ahb_en_reg = readl_relaxed(AHB_EN_REG);
	ahb_enabled = ahb_en_reg & BIT(4);
	if (!ahb_enabled) {
		writel_relaxed(ahb_en_reg | BIT(4), AHB_EN_REG);
		/* Make sure iface clock is enabled before register access */
		mb();
	}

	/* Assert PLL S/W reset */
	writel_relaxed(0x8D, HDMI_PHY_PLL_LOCKDET_CFG2);
	writel_relaxed(0x10, HDMI_PHY_PLL_LOCKDET_CFG0);
	writel_relaxed(0x1A, HDMI_PHY_PLL_LOCKDET_CFG1);
	/* De-assert PLL S/W reset */
	writel_relaxed(0x0D, HDMI_PHY_PLL_LOCKDET_CFG2);

	val = readl_relaxed(HDMI_PHY_REG_12);
	val |= BIT(5);
	/* Assert PHY S/W reset */
	writel_relaxed(val, HDMI_PHY_REG_12);
	val &= ~BIT(5);
	/* De-assert PHY S/W reset */
	writel_relaxed(val, HDMI_PHY_REG_12);
	writel_relaxed(0x3f, HDMI_PHY_REG_2);

	val = readl_relaxed(HDMI_PHY_REG_12);
	val |= PWRDN_B;
	writel_relaxed(val, HDMI_PHY_REG_12);
	/* Wait 10 us for enabling global power for PHY */
	mb();
	udelay(10);

	val = readl_relaxed(HDMI_PHY_PLL_PWRDN_B);
	val |= PLL_PWRDN_B;
	val &= ~PD_PLL;
	writel_relaxed(val, HDMI_PHY_PLL_PWRDN_B);
	writel_relaxed(0x80, HDMI_PHY_REG_2);

	while (!(readl_relaxed(HDMI_PHY_PLL_STATUS0) & BIT(0)))
		cpu_relax();

	if (!ahb_enabled)
		writel_relaxed(ahb_en_reg & ~BIT(4), AHB_EN_REG);
	hdmi_pll_on = 1;
	return 0;
}

void hdmi_pll_disable(void)
{
	unsigned int val;
	u32 ahb_en_reg, ahb_enabled;

	ahb_en_reg = readl_relaxed(AHB_EN_REG);
	ahb_enabled = ahb_en_reg & BIT(4);
	if (!ahb_enabled) {
		writel_relaxed(ahb_en_reg | BIT(4), AHB_EN_REG);
		mb();
	}

	val = readl_relaxed(HDMI_PHY_REG_12);
	val &= (~PWRDN_B);
	writel_relaxed(val, HDMI_PHY_REG_12);

	val = readl_relaxed(HDMI_PHY_PLL_PWRDN_B);
	val |= PD_PLL;
	val &= (~PLL_PWRDN_B);
	writel_relaxed(val, HDMI_PHY_PLL_PWRDN_B);
	/* Make sure HDMI PHY/PLL are powered down */
	mb();

	if (!ahb_enabled)
		writel_relaxed(ahb_en_reg & ~BIT(4), AHB_EN_REG);
	hdmi_pll_on = 0;
}

unsigned hdmi_pll_get_rate(void)
{
	return current_rate;
}

int hdmi_pll_set_rate(unsigned rate)
{
	unsigned int set_power_dwn = 0;
	u32 ahb_en_reg = readl_relaxed(AHB_EN_REG);
	u32 ahb_enabled = ahb_en_reg & BIT(4);

	if (!ahb_enabled) {
		writel_relaxed(ahb_en_reg | BIT(4), AHB_EN_REG);
		/* Make sure iface clock is enabled before register access */
		mb();
	}

	if (hdmi_pll_on) {
		hdmi_pll_disable();
		set_power_dwn = 1;
	}

	switch (rate) {
	case 27030000:
		/* 480p60/480i60 case */
		writel_relaxed(0x32, HDMI_PHY_PLL_REFCLK_CFG);
		writel_relaxed(0x2, HDMI_PHY_PLL_CHRG_PUMP_CFG);
		writel_relaxed(0x08, HDMI_PHY_PLL_LOOP_FLT_CFG0);
		writel_relaxed(0x77, HDMI_PHY_PLL_LOOP_FLT_CFG1);
		writel_relaxed(0x2C, HDMI_PHY_PLL_IDAC_ADJ_CFG);
		writel_relaxed(0x6, HDMI_PHY_PLL_I_VI_KVCO_CFG);
		writel_relaxed(0x7b, HDMI_PHY_PLL_SDM_CFG0);
		writel_relaxed(0x01, HDMI_PHY_PLL_SDM_CFG1);
		writel_relaxed(0x4C, HDMI_PHY_PLL_SDM_CFG2);
		writel_relaxed(0xC0, HDMI_PHY_PLL_SDM_CFG3);
		writel_relaxed(0x00, HDMI_PHY_PLL_SDM_CFG4);
		writel_relaxed(0x9A, HDMI_PHY_PLL_SSC_CFG0);
		writel_relaxed(0x00, HDMI_PHY_PLL_SSC_CFG1);
		writel_relaxed(0x00, HDMI_PHY_PLL_SSC_CFG2);
		writel_relaxed(0x00, HDMI_PHY_PLL_SSC_CFG3);
		writel_relaxed(0x2A, HDMI_PHY_PLL_VCOCAL_CFG0);
		writel_relaxed(0x03, HDMI_PHY_PLL_VCOCAL_CFG1);
		writel_relaxed(0x2B, HDMI_PHY_PLL_VCOCAL_CFG2);
		writel_relaxed(0x00, HDMI_PHY_PLL_VCOCAL_CFG3);
		writel_relaxed(0x86, HDMI_PHY_PLL_VCOCAL_CFG4);
		writel_relaxed(0x00, HDMI_PHY_PLL_VCOCAL_CFG5);
		writel_relaxed(0x33, HDMI_PHY_PLL_VCOCAL_CFG6);
		writel_relaxed(0x00, HDMI_PHY_PLL_VCOCAL_CFG7);
	break;

	case 25200000:
		/* 640x480p60 */
		writel_relaxed(0x32, HDMI_PHY_PLL_REFCLK_CFG);
		writel_relaxed(0x2, HDMI_PHY_PLL_CHRG_PUMP_CFG);
		writel_relaxed(0x01, HDMI_PHY_PLL_LOOP_FLT_CFG0);
		writel_relaxed(0x33, HDMI_PHY_PLL_LOOP_FLT_CFG1);
		writel_relaxed(0x2C, HDMI_PHY_PLL_IDAC_ADJ_CFG);
		writel_relaxed(0x6, HDMI_PHY_PLL_I_VI_KVCO_CFG);
		writel_relaxed(0x77, HDMI_PHY_PLL_SDM_CFG0);
		writel_relaxed(0x4C, HDMI_PHY_PLL_SDM_CFG1);
		writel_relaxed(0x00, HDMI_PHY_PLL_SDM_CFG2);
		writel_relaxed(0xC0, HDMI_PHY_PLL_SDM_CFG3);
		writel_relaxed(0x00, HDMI_PHY_PLL_SDM_CFG4);
		writel_relaxed(0x9A, HDMI_PHY_PLL_SSC_CFG0);
		writel_relaxed(0x00, HDMI_PHY_PLL_SSC_CFG1);
		writel_relaxed(0x00, HDMI_PHY_PLL_SSC_CFG2);
		writel_relaxed(0x20, HDMI_PHY_PLL_SSC_CFG3);
		writel_relaxed(0xF4, HDMI_PHY_PLL_VCOCAL_CFG0);
		writel_relaxed(0x02, HDMI_PHY_PLL_VCOCAL_CFG1);
		writel_relaxed(0x2B, HDMI_PHY_PLL_VCOCAL_CFG2);
		writel_relaxed(0x00, HDMI_PHY_PLL_VCOCAL_CFG3);
		writel_relaxed(0x86, HDMI_PHY_PLL_VCOCAL_CFG4);
		writel_relaxed(0x00, HDMI_PHY_PLL_VCOCAL_CFG5);
		writel_relaxed(0x33, HDMI_PHY_PLL_VCOCAL_CFG6);
		writel_relaxed(0x00, HDMI_PHY_PLL_VCOCAL_CFG7);
	break;

	case 27000000:
		/* 576p50/576i50 case */
		writel_relaxed(0x32, HDMI_PHY_PLL_REFCLK_CFG);
		writel_relaxed(0x2, HDMI_PHY_PLL_CHRG_PUMP_CFG);
		writel_relaxed(0x01, HDMI_PHY_PLL_LOOP_FLT_CFG0);
		writel_relaxed(0x33, HDMI_PHY_PLL_LOOP_FLT_CFG1);
		writel_relaxed(0x2C, HDMI_PHY_PLL_IDAC_ADJ_CFG);
		writel_relaxed(0x6, HDMI_PHY_PLL_I_VI_KVCO_CFG);
		writel_relaxed(0x7B, HDMI_PHY_PLL_SDM_CFG0);
		writel_relaxed(0x01, HDMI_PHY_PLL_SDM_CFG1);
		writel_relaxed(0x4C, HDMI_PHY_PLL_SDM_CFG2);
		writel_relaxed(0xC0, HDMI_PHY_PLL_SDM_CFG3);
		writel_relaxed(0x00, HDMI_PHY_PLL_SDM_CFG4);
		writel_relaxed(0x9A, HDMI_PHY_PLL_SSC_CFG0);
		writel_relaxed(0x00, HDMI_PHY_PLL_SSC_CFG1);
		writel_relaxed(0x00, HDMI_PHY_PLL_SSC_CFG2);
		writel_relaxed(0x00, HDMI_PHY_PLL_SSC_CFG3);
		writel_relaxed(0x2a, HDMI_PHY_PLL_VCOCAL_CFG0);
		writel_relaxed(0x03, HDMI_PHY_PLL_VCOCAL_CFG1);
		writel_relaxed(0x2B, HDMI_PHY_PLL_VCOCAL_CFG2);
		writel_relaxed(0x00, HDMI_PHY_PLL_VCOCAL_CFG3);
		writel_relaxed(0x86, HDMI_PHY_PLL_VCOCAL_CFG4);
		writel_relaxed(0x00, HDMI_PHY_PLL_VCOCAL_CFG5);
		writel_relaxed(0x33, HDMI_PHY_PLL_VCOCAL_CFG6);
		writel_relaxed(0x00, HDMI_PHY_PLL_VCOCAL_CFG7);
	break;

	case 74250000:
		/* 720p60/720p50/1080i60/1080i50
		 * 1080p24/1080p30/1080p25 case
		 */
		writel_relaxed(0x12, HDMI_PHY_PLL_REFCLK_CFG);
		writel_relaxed(0x01, HDMI_PHY_PLL_LOOP_FLT_CFG0);
		writel_relaxed(0x33, HDMI_PHY_PLL_LOOP_FLT_CFG1);
		writel_relaxed(0x76, HDMI_PHY_PLL_SDM_CFG0);
		writel_relaxed(0xE6, HDMI_PHY_PLL_VCOCAL_CFG0);
		writel_relaxed(0x02, HDMI_PHY_PLL_VCOCAL_CFG1);
	break;

	case 148500000:
		/* 1080p60/1080p50 case */
		writel_relaxed(0x2, HDMI_PHY_PLL_REFCLK_CFG);
		writel_relaxed(0x2, HDMI_PHY_PLL_CHRG_PUMP_CFG);
		writel_relaxed(0x01, HDMI_PHY_PLL_LOOP_FLT_CFG0);
		writel_relaxed(0x33, HDMI_PHY_PLL_LOOP_FLT_CFG1);
		writel_relaxed(0x2C, HDMI_PHY_PLL_IDAC_ADJ_CFG);
		writel_relaxed(0x6, HDMI_PHY_PLL_I_VI_KVCO_CFG);
		writel_relaxed(0x76, HDMI_PHY_PLL_SDM_CFG0);
		writel_relaxed(0x01, HDMI_PHY_PLL_SDM_CFG1);
		writel_relaxed(0x4C, HDMI_PHY_PLL_SDM_CFG2);
		writel_relaxed(0xC0, HDMI_PHY_PLL_SDM_CFG3);
		writel_relaxed(0x00, HDMI_PHY_PLL_SDM_CFG4);
		writel_relaxed(0x9A, HDMI_PHY_PLL_SSC_CFG0);
		writel_relaxed(0x00, HDMI_PHY_PLL_SSC_CFG1);
		writel_relaxed(0x00, HDMI_PHY_PLL_SSC_CFG2);
		writel_relaxed(0x00, HDMI_PHY_PLL_SSC_CFG3);
		writel_relaxed(0xe6, HDMI_PHY_PLL_VCOCAL_CFG0);
		writel_relaxed(0x02, HDMI_PHY_PLL_VCOCAL_CFG1);
		writel_relaxed(0x2B, HDMI_PHY_PLL_VCOCAL_CFG2);
		writel_relaxed(0x00, HDMI_PHY_PLL_VCOCAL_CFG3);
		writel_relaxed(0x86, HDMI_PHY_PLL_VCOCAL_CFG4);
		writel_relaxed(0x00, HDMI_PHY_PLL_VCOCAL_CFG5);
		writel_relaxed(0x33, HDMI_PHY_PLL_VCOCAL_CFG6);
		writel_relaxed(0x00, HDMI_PHY_PLL_VCOCAL_CFG7);
	break;
	}

	/* Make sure writes complete before disabling iface clock */
	mb();

	if (set_power_dwn)
		hdmi_pll_enable();

	current_rate = rate;
	if (!ahb_enabled)
		writel_relaxed(ahb_en_reg & ~BIT(4), AHB_EN_REG);

	return 0;
}
