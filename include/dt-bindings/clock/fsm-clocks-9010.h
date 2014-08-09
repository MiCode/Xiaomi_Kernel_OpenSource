/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#ifndef __FSM_CLOCKS_9010_H
#define __FSM_CLOCKS_9010_H

/* clock_rpm controlled clocks */
#define clk_ln_bb_clk				0x3ab0b36d

/* clock_gcc controlled clocks */
#define clk_gcc_blsp1_uart1_apps_clk		0xc7c62f90
#define clk_gcc_blsp1_ahb_clk			0x8caa5b4f
#define clk_gcc_sdcc1_ahb_clk			0x691e0caa
#define clk_gcc_sdcc1_apps_clk			0x9ad6fb96
#define clk_gcc_prng_ahb_clk			0x397e7eaa

#define clk_pcie_0_phy_ldo			0x1d30d092
#define clk_pcie_1_phy_ldo			0x63474b42
#define clk_gcc_pcie_0_aux_clk			0x3d2e3ece
#define clk_gcc_pcie_0_cfg_ahb_clk		0x4dd325c3
#define clk_gcc_pcie_0_mstr_axi_clk		0x3f85285b
#define clk_gcc_pcie_0_pipe_clk			0x4f37621e
#define clk_gcc_pcie_0_slv_axi_clk		0xd69638a1
#define clk_gcc_pcie_1_aux_clk			0xc9bb962c
#define clk_gcc_pcie_1_cfg_ahb_clk		0xb6338658
#define clk_gcc_pcie_1_mstr_axi_clk		0xc20f6269
#define clk_gcc_pcie_1_pipe_clk			0xc1627422
#define clk_gcc_pcie_1_slv_axi_clk		0xd54e40d6
#define clk_gcc_pcie_phy_0_reset		0x6bb4df33
#define clk_gcc_pcie_phy_1_reset		0x5fc03a70


#endif
