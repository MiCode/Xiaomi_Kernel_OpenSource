/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#ifndef __MDSS_PLL_H
#define __MDSS_PLL_H

#include <linux/mdss_io_util.h>
#include <linux/io.h>

#define MDSS_PLL_REG_W(base, offset, data)	\
				writel_relaxed((data), (base) + (offset))
#define MDSS_PLL_REG_R(base, offset)		readl_relaxed((base) + (offset))

enum {
	MDSS_DSI_PLL,
	MDSS_EDP_PLL,
	MDSS_HDMI_PLL,
	MDSS_UNKNOWN_PLL,
};

struct mdss_pll_resources {

	/* Pll specific resources like GPIO, power supply, clocks, etc*/
	struct dss_module_power mp;

	/* dsi/edp/hmdi plls' base register and phy register mapping */
	void __iomem	*pll_base;
	void __iomem	*phy_base;

	/*
	 * Certain pll's needs to update the same vco rate after resume in
	 * suspend/resume scenario. Cached the vco rate for such plls.
	 */
	unsigned long	vco_cached_rate;

	/* dsi/edp/hmdi pll interface type */
	u32		pll_interface_type;

	/*
	 * Keep track to resource status to avoid updating same status for the
	 * pll from different paths
	 */
	bool		resource_enable;

	/*
	 * Certain plls' do not allow vco rate update if it is on. Keep track of
	 * status for them to turn on/off after set rate success.
	 */
	bool		pll_on;

	/*
	 * handoff_status is true of pll is already enabled by bootloader with
	 * continuous splash enable case. Clock API will call the handoff API
	 * to enable the status. It is disabled if continuous splash
	 * feature is disabled.
	 */
	bool		handoff_resources;

	/*
	 * Keep refrence count of pll resource client to avoid releasing them
	 * before all clients are finished with their tasks
	 */
	unsigned int	resource_refcount;

	/* Lock status to provide updated resource status to all clients */
	struct mutex	resource_lock;
};

int mdss_pll_resource_enable(struct mdss_pll_resources *pll_res, bool enable);
int mdss_pll_util_resource_init(struct platform_device *pdev,
					struct mdss_pll_resources *pll_res);
void mdss_pll_util_resource_deinit(struct platform_device *pdev,
					 struct mdss_pll_resources *pll_res);
void mdss_pll_util_resource_release(struct platform_device *pdev,
					struct mdss_pll_resources *pll_res);
int mdss_pll_util_resource_enable(struct mdss_pll_resources *pll_res,
								bool enable);
int mdss_pll_util_resource_parse(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res);
#endif
