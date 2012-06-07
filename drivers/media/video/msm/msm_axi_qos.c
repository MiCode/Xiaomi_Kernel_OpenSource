/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
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

#include <linux/clk.h>
#include <mach/camera.h>
#define MSM_AXI_QOS_NAME "msm_camera"

static struct clk *ebi1_clk;

int add_axi_qos(void)
{
	ebi1_clk = clk_get(NULL, "ebi1_vfe_clk");
	if (IS_ERR(ebi1_clk))
		ebi1_clk = NULL;
	else {
		clk_prepare(ebi1_clk);
		clk_enable(ebi1_clk);
	}

	return 0;
}

int update_axi_qos(uint32_t rate)
{
	if (!ebi1_clk)
		return 0;

	return clk_set_rate(ebi1_clk, rate * 1000);
}

void release_axi_qos(void)
{
	if (!ebi1_clk)
		return;

	clk_disable(ebi1_clk);
	clk_unprepare(ebi1_clk);
	clk_put(ebi1_clk);
	ebi1_clk = NULL;
}
