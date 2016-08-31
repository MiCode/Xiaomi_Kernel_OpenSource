/*
 * drivers/video/tegra/host/gk20a/priv_ring_gk20a.c
 *
 * GK20A priv ring
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/delay.h>	/* for mdelay */

#include "../dev.h"

#include "gk20a.h"
#include "hw_mc_gk20a.h"
#include "hw_pri_ringmaster_gk20a.h"
#include "hw_pri_ringstation_sys_gk20a.h"
#include "hw_trim_gk20a.h"

void gk20a_reset_priv_ring(struct gk20a *g)
{
	u32 data;

	if (tegra_platform_is_linsim())
		return;

	data = gk20a_readl(g, trim_sys_gpc2clk_out_r());
	data = set_field(data,
			trim_sys_gpc2clk_out_bypdiv_m(),
			trim_sys_gpc2clk_out_bypdiv_f(0));
	gk20a_writel(g, trim_sys_gpc2clk_out_r(), data);

	gk20a_reset(g, mc_enable_priv_ring_enabled_f());

	gk20a_writel(g,pri_ringmaster_command_r(),
			0x4);

	gk20a_writel(g, pri_ringstation_sys_decode_config_r(),
			0x2);

	gk20a_readl(g, pri_ringstation_sys_decode_config_r());
}

void gk20a_priv_ring_isr(struct gk20a *g)
{
	u32 status0, status1;
	u32 cmd;
	s32 retry = 100;

	if (tegra_platform_is_linsim())
		return;

	status0 = gk20a_readl(g, pri_ringmaster_intr_status0_r());
	status1 = gk20a_readl(g, pri_ringmaster_intr_status1_r());

	nvhost_dbg_info("ringmaster intr status0: 0x%08x,"
		"status1: 0x%08x", status0, status1);

	if (status0 & (0x1 | 0x2 | 0x4)) {
		gk20a_reset_priv_ring(g);
	}

	cmd = gk20a_readl(g, pri_ringmaster_command_r());
	cmd = set_field(cmd, pri_ringmaster_command_cmd_m(),
		pri_ringmaster_command_cmd_ack_interrupt_f());
	gk20a_writel(g, pri_ringmaster_command_r(), cmd);

	do {
		cmd = pri_ringmaster_command_cmd_v(
			gk20a_readl(g, pri_ringmaster_command_r()));
		usleep_range(20, 40);
	} while (cmd != pri_ringmaster_command_cmd_no_cmd_v() && --retry);

	if (retry <= 0)
		nvhost_warn(dev_from_gk20a(g),
			"priv ringmaster cmd ack too many retries");

	status0 = gk20a_readl(g, pri_ringmaster_intr_status0_r());
	status1 = gk20a_readl(g, pri_ringmaster_intr_status1_r());

	nvhost_dbg_info("ringmaster intr status0: 0x%08x,"
		" status1: 0x%08x", status0, status1);
}

