/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#ifndef __MSM_CLOCKS_CALIFORNIUM_H
#define __MSM_CLOCKS_CALIFORNIUM_H

/* RPM controlled clocks */
#define clk_xo 0xf13dfee3
#define clk_xo_a_clk 0xd939b99b
#define clk_ce_clk 0xd8bc64e1
#define clk_ce_a_clk 0x4dfefd47
#define clk_pcnoc_clk 0xc1296d0f
#define clk_pcnoc_a_clk 0x9bcffee4
#define clk_bimc_clk 0x4b80bf00
#define clk_bimc_a_clk 0x4b25668a
#define clk_snoc_clk 0x2c341aa0
#define clk_snoc_a_clk 0x8fcef2af
#define clk_ipa_clk 0xfa685cda
#define clk_ipa_a_clk 0xeeec2919
#define clk_qpic_clk 0x3ce6f7bb
#define clk_qpic_a_clk 0xd70ccb7c
#define clk_qdss_clk 0x1492202a
#define clk_qdss_a_clk 0xdd121669
#define clk_bimc_msmbus_clk 0xd212feea
#define clk_bimc_msmbus_a_clk 0x71d1a499
#define clk_mcd_ce_clk 0x7ad13979
#define clk_pcnoc_keepalive_a_clk 0x9464f720
#define clk_pcnoc_msmbus_clk 0x2b53b688
#define clk_pcnoc_msmbus_a_clk 0x9753a54f
#define clk_pcnoc_pm_clk 0x5e636b5d
#define clk_pcnoc_sps_clk 0x23d3f584
#define clk_qcedev_ce_clk 0x2e7f9cee
#define clk_qcrypto_ce_clk 0xd8cd060b
#define clk_qseecom_ce_clk 0xea036e4b
#define clk_scm_ce_clk 0xfd35bb87
#define clk_snoc_msmbus_clk 0xe6900bb6
#define clk_snoc_msmbus_a_clk 0x5d4683bd
#define clk_cxo_dwc3_clk 0xf79c19f6
#define clk_cxo_lpm_clk 0x94adbf3d
#define clk_cxo_otg_clk 0x4eec0bb9
#define clk_div_clk1 0xaa1157a6
#define clk_div_clk1_ao 0x6b943d68
#define clk_ln_bb_clk 0x3ab0b36d
#define clk_ln_bb_a_clk 0xc7257ea8
#define clk_rf_clk1 0xaabeea5a
#define clk_rf_clk1_ao 0x72a10cb8
#define clk_rf_clk1_pin 0x8f463562
#define clk_rf_clk1_pin_ao 0x62549ff6
#define clk_rf_clk2 0x24a30992
#define clk_rf_clk2_ao 0x944d8bbd
#define clk_rf_clk2_pin 0xa7c5602a
#define clk_rf_clk2_pin_ao 0x2d75eb4d
#define clk_rf_clk3 0xb673936b
#define clk_rf_clk3_ao 0x038bb968
#define clk_rf_clk3_pin 0x726f53f5
#define clk_rf_clk3_pin_ao 0x76f9240f

/* APSS controlled clocks */
#define clk_gpll0 0x1ebe3bc4
#define clk_gpll0_ao 0xa1368304
#define clk_gpll0_out_msscc 0x7d794829
#define clk_usb30_master_clk_src 0xc6262f89
#define clk_blsp1_qup1_i2c_apps_clk_src 0x17f78f5e
#define clk_blsp1_qup1_spi_apps_clk_src 0xf534c4fa
#define clk_blsp1_qup2_i2c_apps_clk_src 0x8de71c79
#define clk_blsp1_qup2_spi_apps_clk_src 0x33cf809a
#define clk_blsp1_qup3_i2c_apps_clk_src 0xf161b902
#define clk_blsp1_qup3_spi_apps_clk_src 0x5e95683f
#define clk_blsp1_qup4_i2c_apps_clk_src 0xb2ecce68
#define clk_blsp1_qup4_spi_apps_clk_src 0xddb5bbdb
#define clk_blsp1_uart1_apps_clk_src 0xf8146114
#define clk_blsp1_uart2_apps_clk_src 0xfc9c2f73
#define clk_blsp1_uart3_apps_clk_src 0x600497f2
#define clk_blsp1_uart4_apps_clk_src 0x56bff15c
#define clk_gp1_clk_src 0xad85b97a
#define clk_gp2_clk_src 0xfb1f0065
#define clk_gp3_clk_src 0x63b693d6
#define clk_pcie_aux_clk_src 0xebc50566
#define clk_pdm2_clk_src 0x31e494fd
#define clk_sdcc1_apps_clk_src 0xd4975db2
#define clk_usb30_mock_utmi_clk_src 0xa024a976
#define clk_usb3_aux_clk_src 0xfde7ae09
#define clk_gcc_pcie_phy_reset 0x9bc3c959
#define clk_gcc_qusb2a_phy_reset 0x2a9dfa9f
#define clk_gcc_usb3phy_phy_reset 0xb1a4f885
#define clk_gcc_usb3_phy_reset 0x03d559f1
#define clk_gpll0_out_main_cgc 0xb0298998
#define clk_gcc_blsp1_ahb_clk 0x8caa5b4f
#define clk_gcc_blsp1_qup1_i2c_apps_clk 0xc303fae9
#define clk_gcc_blsp1_qup1_spi_apps_clk 0x759a76b0
#define clk_gcc_blsp1_qup2_i2c_apps_clk 0x1076f220
#define clk_gcc_blsp1_qup2_spi_apps_clk 0x3e77d48f
#define clk_gcc_blsp1_qup3_i2c_apps_clk 0x9e25ac82
#define clk_gcc_blsp1_qup3_spi_apps_clk 0xfb978880
#define clk_gcc_blsp1_qup4_i2c_apps_clk 0xd7f40f6f
#define clk_gcc_blsp1_qup4_spi_apps_clk 0x80f8722f
#define clk_gcc_blsp1_uart1_apps_clk 0xc7c62f90
#define clk_gcc_blsp1_uart2_apps_clk 0xf8a61c96
#define clk_gcc_blsp1_uart3_apps_clk 0xc3298bd7
#define clk_gcc_blsp1_uart4_apps_clk 0x26be16c0
#define clk_gcc_boot_rom_ahb_clk 0xde2adeb1
#define clk_gcc_dcc_clk 0xd1000c50
#define clk_gpll0_out_main_div2_cgc 0xc76ac7ae
#define clk_gcc_gp1_clk 0x057f7b69
#define clk_gcc_gp2_clk 0x9bf83ffd
#define clk_gcc_gp3_clk 0xec6539ee
#define clk_gcc_mss_q6_bimc_axi_clk 0x67544d62
#define clk_gcc_pcie_axi_clk 0xb833d9e3
#define clk_gcc_pcie_axi_mstr_clk 0x54d09178
#define clk_gcc_pcie_cfg_ahb_clk 0xddc9a515
#define clk_gcc_pcie_pipe_clk 0x8be62558
#define clk_gcc_pcie_sleep_clk 0x8b8bfc3b
#define clk_gcc_pdm2_clk 0x99d55711
#define clk_gcc_pdm_ahb_clk 0x365664f6
#define clk_gcc_prng_ahb_clk 0x397e7eaa
#define clk_gcc_sdcc1_ahb_clk 0x691e0caa
#define clk_gcc_sdcc1_apps_clk 0x9ad6fb96
#define clk_gcc_apss_tcu_clk 0xaf56a329
#define clk_gcc_pcie_axi_tbu_clk 0xab70f06e
#define clk_gcc_pcie_ref_clk 0x63fca50a
#define clk_gcc_usb_ss_ref_clk 0xb85dadfa
#define clk_gcc_qusb_ref_clk 0x16e35a90
#define clk_gcc_smmu_cfg_clk 0x75eaefa5
#define clk_gcc_usb3_axi_tbu_clk 0x18779c6e
#define clk_gcc_sys_noc_usb3_axi_clk 0x94d26800
#define clk_gcc_usb30_master_clk 0xb3b4e2cb
#define clk_gcc_usb30_mock_utmi_clk 0xa800b65a
#define clk_gcc_usb30_sleep_clk 0xd0b65c92
#define clk_gcc_usb3_aux_clk 0x555d16b2
#define clk_gcc_usb3_pipe_clk 0x26f8a97a
#define clk_gcc_usb_phy_cfg_ahb_clk 0xccb7e26f
#define clk_gcc_mss_cfg_ahb_clk 0x111cde81

/* a7pll */
#define clk_a7pll_clk		0x3dd5dd94

/* clock_debug controlled clocks */
#define clk_gcc_debug_mux 0x8121ac15

/* Audio External Clocks */
#define clk_audio_lpass_mclk 0x575ec22b

#endif
