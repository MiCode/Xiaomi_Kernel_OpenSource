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

#ifndef __MSM_CLOCKS_VPIPA_H
#define __MSM_CLOCKS_VPIPA_H

/* clock_rpm controlled clocks */
#define clk_xo_clk_src 0x23f5649f
#define clk_xo_a_clk_src 0x2fdd2c7c
#define clk_bimc_clk 0x4b80bf00
#define clk_bimc_a_clk 0x4b25668a
#define clk_bimc_msmbus_clk 0xd212feea
#define clk_bimc_msmbus_a_clk 0x71d1a499
#define clk_pcnoc_clk 0xc1296d0f
#define clk_pcnoc_a_clk 0x9bcffee4
#define clk_pcnoc_msmbus_clk 0x2b53b688
#define clk_pcnoc_msmbus_a_clk 0x9753a54f
#define clk_snoc_clk 0x2c341aa0
#define clk_snoc_a_clk 0x8fcef2af
#define clk_snoc_msmbus_clk 0xe6900bb6
#define clk_snoc_msmbus_a_clk 0x5d4683bd
#define clk_ipa_clk 0xfa685cda
#define clk_ipa_a_clk 0xeeec2919
#define clk_qdss_clk 0x1492202a
#define clk_qdss_a_clk 0xdd121669
#define clk_qpic_clk 0x3ce6f7bb
#define clk_qpic_a_clk 0xd70ccb7c

/* clock_gcc controlled clocks */
#define clk_gpll0 0x1ebe3bc4
#define clk_gpll0_ao 0xa1368304
#define clk_gpll0_out_main 0xe9374de7
#define clk_gcc_sys_noc_usb3_axi_clk 0x94d26800
#define clk_gcc_imem_axi_clk 0x3e52e7d8
#define clk_gcc_imem_cfg_ahb_clk 0x5d4f462e
#define clk_gcc_sdcc1_apps_clk 0x9ad6fb96
#define clk_gcc_sdcc1_ahb_clk 0x691e0caa
#define clk_sdcc1_apps_clk_src 0xd4975db2
#define clk_gcc_blsp1_ahb_clk 0x8caa5b4f
#define clk_gcc_blsp1_sleep_clk 0x989f22f3
#define clk_gcc_blsp1_qup1_spi_apps_clk 0x759a76b0
#define clk_gcc_blsp1_qup1_i2c_apps_clk 0xc303fae9
#define clk_blsp1_qup1_spi_apps_clk_src 0xf534c4fa
#define clk_blsp1_qup1_i2c_apps_clk_src 0x17f78f5e
#define clk_gcc_blsp1_uart1_apps_clk 0xc7c62f90
#define clk_gcc_blsp1_uart1_sim_clk 0x36377c55
#define clk_blsp1_uart1_apps_clk_src 0xf8146114
#define clk_gcc_blsp1_qup2_spi_apps_clk 0x3e77d48f
#define clk_gcc_blsp1_qup2_i2c_apps_clk 0x1076f220
#define clk_blsp1_qup2_spi_apps_clk_src 0x33cf809a
#define clk_blsp1_qup2_i2c_apps_clk_src 0x8de71c79
#define clk_gcc_blsp1_uart2_apps_clk 0xf8a61c96
#define clk_gcc_blsp1_uart2_sim_clk 0xdeaa39fe
#define clk_blsp1_uart2_apps_clk_src 0xfc9c2f73
#define clk_gcc_blsp1_qup3_spi_apps_clk 0xfb978880
#define clk_gcc_blsp1_qup3_i2c_apps_clk 0x9e25ac82
#define clk_blsp1_qup3_spi_apps_clk_src 0x5e95683f
#define clk_blsp1_qup3_i2c_apps_clk_src 0xf161b902
#define clk_gcc_blsp1_uart3_apps_clk 0xc3298bd7
#define clk_gcc_blsp1_uart3_sim_clk 0xbaf22819
#define clk_blsp1_uart3_apps_clk_src 0x600497f2
#define clk_gcc_blsp1_qup4_spi_apps_clk 0x80f8722f
#define clk_gcc_blsp1_qup4_i2c_apps_clk 0xd7f40f6f
#define clk_blsp1_qup4_spi_apps_clk_src 0xddb5bbdb
#define clk_blsp1_qup4_i2c_apps_clk_src 0xb2ecce68
#define clk_gcc_blsp1_uart4_apps_clk 0x26be16c0
#define clk_gcc_blsp1_uart4_sim_clk 0x0fe63389
#define clk_blsp1_uart4_apps_clk_src 0x56bff15c
#define clk_gcc_pdm_ahb_clk 0x365664f6
#define clk_gcc_pdm_xo4_clk 0x3d32f1d0
#define clk_gcc_pdm2_clk 0x99d55711
#define clk_pdm2_clk_src 0x31e494fd
#define clk_pdm_clk_src 0x388230aa
#define clk_pdm_xo4_clk_src 0x842fef1a
#define clk_apss_ahb_clk_src 0x36f8495f
#define clk_apss_ahb_postdiv_clk_src 0x72168d9f
#define clk_usb3_phy_wrapper_gcc_usb3_pipe_clk 0x283a7497
#define clk_gcc_usb30_master_clk 0xb3b4e2cb
#define clk_gcc_usb30_sleep_clk 0xd0b65c92
#define clk_gcc_usb30_mock_utmi_clk 0xa800b65a
#define clk_usb30_master_clk_src 0xc6262f89
#define clk_usb30_mock_utmi_clk_src 0xa024a976
#define clk_usb30_mock_utmi_postdiv_clk_src 0xce6ee796
#define clk_gcc_usb_phy_cfg_ahb_clk 0xccb7e26f
#define clk_gcc_usb3_pipe_clk 0x26f8a97a
#define clk_gcc_usb3_aux_clk 0x555d16b2
#define clk_usb3_pipe_clk_src 0x8b922db4
#define clk_usb3_aux_clk_src 0xfde7ae09
#define clk_gcc_pcie_cfg_ahb_clk 0xddc9a515
#define clk_gcc_pcie_pipe_clk 0x8be62558
#define clk_gcc_pcie_axi_clk 0xb833d9e3
#define clk_gcc_pcie_sleep_clk 0x8b8bfc3b
#define clk_gcc_pcie_axi_mstr_clk 0x54d09178
#define clk_pcie_pipe_clk_src 0x3ef3897d
#define clk_pcie_aux_clk_src 0xebc50566

#endif
