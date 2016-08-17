/*
 * arch/arm/mach-tegra/mcerr-t3.c
 *
 * Tegra 3 SoC-specific mcerr code.
 *
 * Copyright (c) 2010-2012, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "mcerr.h"

struct mc_client mc_clients[] = {
	client("ptc"),
	client("display0_wina"), client("display1_wina"),
	client("display0_winb"), client("display1_winb"),
	client("display0_winc"), client("display1_winc"),
	client("display0_winb_vfilter"),
	client("display1_winb_vfilter"),
	client("epp"), client("gr2d_pat"),
	client("gr2d_src"), client("mpe_unified"),
	client("vi_chroma_filter"), client("pcie"),
	client("avp"),
	client("display0_cursor"), client("display1_cursor"),
	client("gr3d0_fdc"), client("gr3d1_fdc"),
	client("gr2d_dst"), client("hda"),
	client("host1x_dma"), client("host1x_generic"),
	client("gr3d0_idx"), client("gr3d1_idx"),
	client("mpe_intrapred"), client("mpe_mpea"),
	client("mpe_mpec"), client("ahb_dma"),
	client("ahb_slave"), client("sata"),
	client("gr3d0_tex"), client("gr3d1_tex"),
	client("vde_bsev"), client("vde_mbe"),
	client("vde_mce"), client("vde_tpe"),
	client("cpu_lp"), client("cpu"),
	client("epp_u"), client("epp_v"),
	client("epp_y"), client("mpe_unified"),
	client("vi_sb"), client("vi_u"),
	client("vi_v"), client("vi_y"),
	client("gr2d_dst"), client("pcie"),
	client("avp"), client("gr3d0_fdc"),
	client("gr3d1_fdc"), client("hda"),
	client("host1x"),	client("isp"),
	client("cpu_lp"),	client("cpu"),
	client("mpe_mpec"), client("ahb_dma"),
	client("ahb_slave"), client("sata"),
	client("vde_bsev"), client("vde_dbg"),
	client("vde_mbe"), client("vde_tpm"),
};

/*
 * Defaults work for T30.
 */
void mcerr_chip_specific_setup(struct mcerr_chip_specific *spec)
{
	return;
}
