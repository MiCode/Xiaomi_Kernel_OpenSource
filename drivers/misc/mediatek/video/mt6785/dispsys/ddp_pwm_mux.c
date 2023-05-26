/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
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
#define MUX_DISPPWM_ADDR (disp_pmw_mux_base + 0xA0)
#endif

#ifndef CLK_CFG_UPDATE1 /* disp pwm source clock select register address */
#define CLK_CFG_UPDATE1 (disp_pmw_mux_base + 0x08)
#endif

#define DISP_PWM_MUX_CK_UPDATE	(1)

#ifndef OSC_ULPOSC_ADDR /* rosc control register address */
#define OSC_ULPOSC_ADDR (disp_pmw_osc_base + 0x440)
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
		clkid = CLK_OSC_D4;
		break;
	case 1:
		clkid = CLK_OSC_D16;
		break;
	case 2:
		clkid = CLK_OSC_D2;
		break;
	case 3:
		clkid = CLK_UNIVPLL_D3_D4;
		break;
	case 4:
		clkid = CLK_26M; /* Bypass config:default 26M */
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
		/* pr_debug("[PWM]TOPCKGEN node exist"); */
		return 0;
	}

	node = of_find_compatible_node(NULL, NULL, DTSI_TOPCKGEN);
	if (!node) {
		pr_info("[PWM]DISP find TOPCKGEN node failed\n");
		return -1;
	}
	disp_pmw_mux_base = of_iomap(node, 0);
	if (!disp_pmw_mux_base) {
		pr_info("[PWM]DISP TOPCKGEN base failed\n");
		return -1;
	}
	pr_debug("[PWM]find TOPCKGEN node");
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

static void disp_pwm_clkmux_update(void)
{
	unsigned int regsrc = clk_readl(CLK_CFG_UPDATE1);

	regsrc = regsrc | (0x1 << DISP_PWM_MUX_CK_UPDATE);
	/* write 1 to update the change of pwm clock source selection */
	clk_writel(CLK_CFG_UPDATE1, regsrc);
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

	if (clkid != -1) {
		ddp_clk_prepare_enable(CLK_MUX_DISP_PWM);
		ddp_clk_set_parent(CLK_MUX_DISP_PWM, clkid);
		ddp_clk_disable_unprepare(CLK_MUX_DISP_PWM);

		disp_pwm_clkmux_update();
	}

	reg_after = disp_pwm_get_pwmmux();
	g_pwm_mux_clock_source = reg_after & 0x7;

	if (reg_before != reg_after) {
		pr_debug(
			"[PWM]PWM_MUX %x->%x, clk_req=%d clkid=%d",
			reg_before, reg_after, clk_req, clkid);
	}

	return 0;
}

static void __iomem *disp_pmw_osc_base;

/*
 *
 * get disp pwm source osc
 *
 */
static int get_ulposc_base(void)
{
	int ret = 0;
	struct device_node *node;

	if (disp_pmw_osc_base != NULL) {
		/* pr_debug("[PWM]SLEEP node exist"); */
		return 0;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,sleep");
	if (!node) {
		pr_info("[PWM]DISP find SLEEP node failed\n");
		return -1;
	}
	disp_pmw_osc_base = of_iomap(node, 0);
	if (!disp_pmw_osc_base) {
		pr_info("[PWM]DISP find SLEEP base failed\n");
		return -1;
	}

	return ret;
}

static int get_ulposc_status(void)
{
	unsigned int regosc;
	int ret = -1;

	if (get_ulposc_base() == -1) {
		pr_info("[PWM]get ULPOSC status fail");
		return ret;
	}

	regosc = clk_readl(OSC_ULPOSC_ADDR);
	if ((regosc & 0x5) != 0x5) {
		pr_debug("[PWM]ULPOSC is off (%x)", regosc);
		ret = 0;
	} else {
		pr_debug("[PWM]ULPOSC is on (%x)", regosc);
		ret = 1;
	}

	return ret;
}

/*
 *
 * hardcode turn on/off ROSC api
 *
 */
static int ulposc_on(void)
{
	unsigned int regosc;

	if (get_ulposc_base() == -1)
		return -1;

	regosc = clk_readl(OSC_ULPOSC_ADDR);
	/* pr_debug("[PWM]ULPOSC config : 0x%08x", regosc); */

	regosc = regosc | 0x1;
	clk_writel(OSC_ULPOSC_ADDR, regosc);
	/* pr_debug("[PWM]ULPOSC config : 0x%08x after en", regosc); */
	udelay(150);

	regosc = clk_readl(OSC_ULPOSC_ADDR);
	regosc = regosc | 0x4;
	clk_writel(OSC_ULPOSC_ADDR, regosc);
	/* udelay(150); */
	/* regosc = clk_readl(OSC_ULPOSC_ADDR); */
	/* pr_debug("[PWM]ULPOSC config : 0x%08x after rst 1", regosc); */

	return 0;
}

static int ulposc_off(void)
{
	unsigned int regosc;

	if (get_ulposc_base() == -1)
		return -1;

	regosc = clk_readl(OSC_ULPOSC_ADDR);

	regosc = regosc & (~0x4);
	clk_writel(OSC_ULPOSC_ADDR, regosc);

	udelay(150);
	regosc = clk_readl(OSC_ULPOSC_ADDR);
	/* pr_debug("[PWM]ULPOSC config : 0x%08x after cg_en", regosc); */

	regosc = regosc & (~0x1);
	clk_writel(OSC_ULPOSC_ADDR, regosc);
	/* udelay(150); */
	/* regosc = clk_readl(OSC_ULPOSC_ADDR); */
	/* pr_debug("[PWM]ULPOSC config : 0x%08x after en", regosc); */

	return 0;
}

static int ulposc_enable(enum DDP_CLK_ID clkid)
{
	ulposc_on();
	get_ulposc_status();

	return 0;
}

static int ulposc_disable(enum DDP_CLK_ID clkid)
{
	ulposc_off();
	get_ulposc_status();

	return 0;
}

/*
 *
 * disp pwm clock source power on /power off api
 *
 */
int disp_pwm_clksource_enable(int clk_req)
{
	int ret = 0;
	enum DDP_CLK_ID clkid = -1;

	clkid = disp_pwm_get_clkid(clk_req);

	switch (clkid) {
	case CLK_OSC_D16:
	case CLK_OSC_D4:
	case CLK_OSC_D2:
		ulposc_enable(clkid);
		break;
	default:
		break;
	}

	return ret;
}

int disp_pwm_clksource_disable(int clk_req)
{
	int ret = 0;
	enum DDP_CLK_ID clkid = -1;

	clkid = disp_pwm_get_clkid(clk_req);

	switch (clkid) {
	case CLK_OSC_D16:
	case CLK_OSC_D4:
	case CLK_OSC_D2:
		ulposc_disable(clkid);
		break;
	default:
		break;
	}

	return ret;
}

/*
 *
 * disp pwm clock source query api
 *
 */

bool disp_pwm_mux_is_osc(void)
{
	bool is_osc = false;

	if (g_pwm_mux_clock_source == 2 ||
		g_pwm_mux_clock_source == 3 ||
		g_pwm_mux_clock_source == 4)
		is_osc = true;
	return is_osc;
}
#endif		/* BYPASS_CLK_SELECT */
