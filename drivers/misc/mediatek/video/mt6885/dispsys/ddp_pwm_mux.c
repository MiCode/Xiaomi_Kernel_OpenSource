/*
 * Copyright (C) 2016 MediaTek Inc.
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

#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include "ddp_clkmgr.h"
#include "ddp_pwm_mux.h"
#include <linux/of.h>
#include <linux/of_address.h>
#include "ddp_reg.h"

/* #define BYPASS_CLK_SELECT */
/*
 *
 * dummy function
 *
 */
#ifdef BYPASS_CLK_SELECT
int disp_pwm_set_pwmmux(unsigned int clk_req)
{
	return 0;
}
int disp_pwm_clksource_enable(int clk_req)
{
	return 0;
}
int disp_pwm_clksource_disable(int clk_req)
{
	return 0;
}
bool disp_pwm_mux_is_osc(void)
{
	return false;
}
#else
/*
 *
 * variable for get clock node fromdts
 *
 */
static void __iomem *disp_pmw_mux_base;

#ifndef MUX_DISPPWM_ADDR /* disp pwm source clock select register address */
#define MUX_DISPPWM_ADDR (disp_pmw_mux_base + 0x80)
#endif

/* clock hard code access API */
#define DRV_Reg32(addr) INREG32(addr)
#define clk_readl(addr) DRV_Reg32(addr)
#define clk_writel(addr, val) mt_reg_sync_writel(val, addr)

static int g_pwm_mux_clock_source = -1;

/*
 *
 * disp pwm source clock select mux api
 *
 */
enum DDP_CLK_ID disp_pwm_get_clkid(unsigned int clk_req)
{
	enum DDP_CLK_ID clkid = -1;

	switch (clk_req) {
	case 0:
		clkid = OSC_D4;
		break;
	case 1:
		clkid = OSC_D16;
		break;
	case 2:
		clkid = OSC_D2;
		break;
	case 3:
		clkid = UNIVPLL_D6_D4;
		break;
	case 4:
		clkid = CLK26M; /* Bypass config:default 26M */
		break;
	default:
		clkid = -1;
		break;
	}

	return clkid;
}

/*
 *
 * get disp pwm source mux node
 *
 */
#define DTSI_TOPCKGEN "mediatek,topckgen"
static int disp_pwm_get_muxbase(void)
{
	int ret = 0;
	struct device_node *node;

	if (disp_pmw_mux_base != NULL) {
		pr_notice("[PWM]TOPCKGEN node exist");
		return 0;
	}

	node = of_find_compatible_node(NULL, NULL, DTSI_TOPCKGEN);
	if (!node) {
		pr_notice("[PWM]DISP find TOPCKGEN node failed\n");
		return -1;
	}
	disp_pmw_mux_base = of_iomap(node, 0);
	if (!disp_pmw_mux_base) {
		pr_notice("[PWM]DISP TOPCKGEN base failed\n");
		return -1;
	}
	pr_notice("[PWM]find TOPCKGEN node");
	return ret;
}

static unsigned int disp_pwm_get_pwmmux(void)
{
	unsigned int regsrc = 0;

	if (MUX_DISPPWM_ADDR != NULL)
		regsrc = clk_readl(MUX_DISPPWM_ADDR);
	else
		pr_info("[PWM]mux addr illegal");

	return regsrc;
}

/*
 *
 * disp pwm source clock select mux api
 *
 */
int disp_pwm_set_pwmmux(unsigned int clk_req)
{
	unsigned int reg_before, reg_after;
	int ret = 0;
	enum DDP_CLK_ID clkid = -1;

	clkid = disp_pwm_get_clkid(clk_req);

	ret = disp_pwm_get_muxbase();
	reg_before = disp_pwm_get_pwmmux();

	pr_notice("[PWM]clk_req=%d clkid=%d", clk_req, clkid);

	if (clkid != -1) {
		ddp_clk_prepare_enable(CLK_MUX_DISP_PWM);
		ddp_clk_set_parent(CLK_MUX_DISP_PWM, clkid);
		ddp_clk_disable_unprepare(CLK_MUX_DISP_PWM);
	}

	reg_after = disp_pwm_get_pwmmux();
	g_pwm_mux_clock_source = (reg_after>>24) & 0x3;
	pr_notice("[PWM]PWM_MUX %x->%x", reg_before, reg_after);

	return 0;
}

/*
 *
 * disp pwm clock source power on /power off api
 *
 */
int disp_pwm_clksource_enable(int clk_req)
{
	return 0;
}

int disp_pwm_clksource_disable(int clk_req)
{
	return 0;
}

/*
 *
 * disp pwm clock source query api
 *
 */

bool disp_pwm_mux_is_osc(void)
{
	bool is_osc = false;

	if (g_pwm_mux_clock_source == 2 || g_pwm_mux_clock_source == 3)
		is_osc = true;

	return is_osc;
}
#endif		/* BYPASS_CLK_SELECT */
