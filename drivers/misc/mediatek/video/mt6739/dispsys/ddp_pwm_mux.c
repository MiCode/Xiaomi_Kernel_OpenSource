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
#include <linux/of.h>
#include <linux/of_address.h>
#include "ddp_reg.h"
#include "ddp_clkmgr.h"
#include "ddp_pwm_mux.h"

#define PWM_MSG(fmt, arg...) pr_debug("[PWM] " fmt "\n", ##arg)
#define PWM_ERR(fmt, arg...) pr_debug("[PWM] " fmt "\n", ##arg)
#define PWM_NOTICE(fmt, arg...) pr_debug("[PWM] " fmt "\n", ##arg)

/*#define BYPASS_CLK_SELECT*/

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
#define MUX_DISPPWM_ADDR (disp_pmw_mux_base + 0xB0)
#endif
#ifdef HARD_CODE_CONFIG
/* disp pwm source clock update register address */
#ifndef MUX_UPDATE_ADDR
#define MUX_UPDATE_ADDR (disp_pmw_mux_base + 0x4)
#endif
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
		clkid = CLK26M;
		break;
	case 1:
		clkid = UNIVPLL2_D4;
		break;
	case 2:
		clkid = UNIVPLL2_D8;
		break;
	case 3:
		clkid = UNIVPLL3_D8;
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
		PWM_MSG("TOPCKGEN node exist");
		return 0;
	}

	node = of_find_compatible_node(NULL, NULL, DTSI_TOPCKGEN);
	if (!node) {
		PWM_ERR("DISP find TOPCKGEN node failed\n");
		return -1;
	}
	disp_pmw_mux_base = of_iomap(node, 0);
	if (!disp_pmw_mux_base) {
		PWM_ERR("DISP TOPCKGEN base failed\n");
		return -1;
	}
	PWM_MSG("find TOPCKGEN node");
	return ret;
}

static unsigned int disp_pwm_get_pwmmux(void)
{
	unsigned int regsrc = 0;

	if (MUX_DISPPWM_ADDR != NULL)
		regsrc = clk_readl(MUX_DISPPWM_ADDR);
	else
		PWM_ERR("mux addr illegal");

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
	enum DDP_CLK_ID clkid = -1;

	clkid = disp_pwm_get_clkid(clk_req);

	disp_pwm_get_muxbase();
	reg_before = disp_pwm_get_pwmmux();

	PWM_NOTICE("clk_req=%d clkid=%d", clk_req, clkid);

	if (clkid != -1) {
		ddp_clk_prepare_enable(MUX_PWM);
		ddp_clk_set_parent(MUX_PWM, clkid);
		ddp_clk_disable_unprepare(MUX_PWM);
	}

	reg_after = disp_pwm_get_pwmmux();
	g_pwm_mux_clock_source = (reg_after) & 0x3;
	PWM_NOTICE("PWM_MUX %x->%x", reg_before, reg_after);

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

	return false;
}
#endif /* BYPASS_CLK_SELECT */
