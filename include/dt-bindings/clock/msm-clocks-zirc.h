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

#ifndef __MSM_CLOCKS_ZIRC_H
#define __MSM_CLOCKS_ZIRC_H

/* clock_rpm controlled clocks */
#define clk_xo_clk_src			&xo_clk_src
#define clk_xo_a_clk_src		&xo_a_clk_src
#define clk_bimc_clk			&bimc_clk
#define clk_bimc_a_clk			&bimc_a_clk
#define clk_bimc_msmbus_clk		&bimc_msmbus_clk
#define clk_bimc_msmbus_a_clk		&bimc_msmbus_a_clk
#define clk_pcnoc_clk			&pcnoc_clk
#define clk_pcnoc_a_clk			&pcnoc_a_clk
#define clk_pcnoc_msmbus_clk		&pcnoc_msmbus_clk
#define clk_pcnoc_msmbus_a_clk		&pcnoc_msmbus_a_clk
#define clk_snoc_clk			&snoc_clk
#define clk_snoc_a_clk			&snoc_a_clk
#define clk_snoc_msmbus_clk		&snoc_msmbus_clk
#define clk_snoc_msmbus_a_clk		&snoc_msmbus_a_clk
#define clk_ipa_clk			&ipa_clk
#define clk_ipa_a_clk			&ipa_a_clk
#define clk_qdss_clk			&qdss_clk
#define clk_qdss_a_clk			&qdss_a_clk
#define clk_qpic_clk			&qpic_clk
#define clk_qpic_a_clk			&qpic_a_clk
#define clk_ln_bb_clk			&ln_bb_clk
#define clk_cxo_dwc3_clk		&cxo_dwc3_clk
#define clk_ce_clk			&ce_clk
#define clk_ce_a_clk			&ce_a_clk
#define clk_qcedev_ce_clk		&qcedev_ce_clk
#define clk_qcrypto_ce_clk		&qcrypto_ce_clk

/* clock_gcc controlled clocks */
#define clk_gpll0				&gpll0
#define clk_gpll0_ao				&gpll0_ao
#define clk_gpll0_out_main			&gpll0_out_main
#define clk_gcc_sys_noc_usb3_axi_clk		&gcc_sys_noc_usb3_axi_clk
#define clk_gcc_imem_axi_clk			&gcc_imem_axi_clk
#define clk_gcc_imem_cfg_ahb_clk		&gcc_imem_cfg_ahb_clk
#define clk_gcc_sdcc1_apps_clk			&gcc_sdcc1_apps_clk
#define clk_gcc_sdcc1_ahb_clk			&gcc_sdcc1_ahb_clk
#define clk_sdcc1_apps_clk_src			&sdcc1_apps_clk_src
#define clk_gcc_blsp1_ahb_clk			&gcc_blsp1_ahb_clk
#define clk_gcc_blsp1_sleep_clk			&gcc_blsp1_sleep_clk
#define clk_gcc_blsp1_qup1_spi_apps_clk		&gcc_blsp1_qup1_spi_apps_clk
#define clk_gcc_blsp1_qup1_i2c_apps_clk		&gcc_blsp1_qup1_i2c_apps_clk
#define clk_blsp1_qup1_spi_apps_clk_src		&blsp1_qup1_spi_apps_clk_src
#define clk_blsp1_qup1_i2c_apps_clk_src		&blsp1_qup1_i2c_apps_clk_src
#define clk_gcc_blsp1_uart1_apps_clk		&gcc_blsp1_uart1_apps_clk
#define clk_gcc_blsp1_uart1_sim_clk		&gcc_blsp1_uart1_sim_clk
#define clk_blsp1_uart1_apps_clk_src		&blsp1_uart1_apps_clk_src
#define clk_gcc_blsp1_qup2_spi_apps_clk		&gcc_blsp1_qup2_spi_apps_clk
#define clk_gcc_blsp1_qup2_i2c_apps_clk		&gcc_blsp1_qup2_i2c_apps_clk
#define clk_blsp1_qup2_spi_apps_clk_src		&blsp1_qup2_spi_apps_clk_src
#define clk_blsp1_qup2_i2c_apps_clk_src		&blsp1_qup2_i2c_apps_clk_src
#define clk_gcc_blsp1_uart2_apps_clk		&gcc_blsp1_uart2_apps_clk
#define clk_gcc_blsp1_uart2_sim_clk		&gcc_blsp1_uart2_sim_clk
#define clk_blsp1_uart2_apps_clk_src		&blsp1_uart2_apps_clk_src
#define clk_gcc_blsp1_qup3_spi_apps_clk		&gcc_blsp1_qup3_spi_apps_clk
#define clk_gcc_blsp1_qup3_i2c_apps_clk		&gcc_blsp1_qup3_i2c_apps_clk
#define clk_blsp1_qup3_spi_apps_clk_src		&blsp1_qup3_spi_apps_clk_src
#define clk_blsp1_qup3_i2c_apps_clk_src		&blsp1_qup3_i2c_apps_clk_src
#define clk_gcc_blsp1_uart3_apps_clk		&gcc_blsp1_uart3_apps_clk
#define clk_gcc_blsp1_uart3_sim_clk		&gcc_blsp1_uart3_sim_clk
#define clk_blsp1_uart3_apps_clk_src		&blsp1_uart3_apps_clk_src
#define clk_gcc_blsp1_qup4_spi_apps_clk		&gcc_blsp1_qup4_spi_apps_clk
#define clk_gcc_blsp1_qup4_i2c_apps_clk		&gcc_blsp1_qup4_i2c_apps_clk
#define clk_blsp1_qup4_spi_apps_clk_src		&blsp1_qup4_spi_apps_clk_src
#define clk_blsp1_qup4_i2c_apps_clk_src		&blsp1_qup4_i2c_apps_clk_src
#define clk_gcc_blsp1_uart4_apps_clk		&gcc_blsp1_uart4_apps_clk
#define clk_gcc_blsp1_uart4_sim_clk		&gcc_blsp1_uart4_sim_clk
#define clk_blsp1_uart4_apps_clk_src		&blsp1_uart4_apps_clk_src
#define clk_gcc_pdm_ahb_clk			&gcc_pdm_ahb_clk
#define clk_gcc_pdm_xo4_clk			&gcc_pdm_xo4_clk
#define clk_gcc_pdm2_clk			&gcc_pdm2_clk
#define clk_pdm2_clk_src			&pdm2_clk_src
#define clk_pdm_clk_src				&pdm_clk_src
#define clk_pdm_xo4_clk_src			&pdm_xo4_clk_src
#define clk_apss_ahb_clk_src			&apss_ahb_clk_src
#define clk_apss_ahb_postdiv_clk_src		&apss_ahb_postdiv_clk_src
#define clk_usb3_phy_wrapper_gcc_usb3_pipe_clk \
					&usb3_phy_wrapper_gcc_usb3_pipe_clk
#define clk_gcc_usb30_master_clk		&gcc_usb30_master_clk
#define clk_gcc_usb30_sleep_clk			&gcc_usb30_sleep_clk
#define clk_gcc_usb30_mock_utmi_clk		&gcc_usb30_mock_utmi_clk
#define clk_usb30_master_clk_src		&usb30_master_clk_src
#define clk_usb30_mock_utmi_clk_src		&usb30_mock_utmi_clk_src
#define clk_usb30_mock_utmi_postdiv_clk_src	&usb30_mock_utmi_postdiv_clk_src
#define clk_gcc_usb_phy_cfg_ahb_clk		&gcc_usb_phy_cfg_ahb_clk
#define clk_gcc_usb3_pipe_clk			&gcc_usb3_pipe_clk
#define clk_gcc_usb3_aux_clk			&gcc_usb3_aux_clk
#define clk_usb3_pipe_clk_src			&usb3_pipe_clk_src
#define clk_usb3_aux_clk_src			&usb3_aux_clk_src
#define clk_gcc_qusb2a_phy_reset		&gcc_qusb2a_phy_reset
#define clk_gcc_usb3_phy_reset			&gcc_usb3_phy_reset
#define clk_gcc_usb3phy_phy_reset		&gcc_usb3phy_phy_reset
#define clk_usb_ss_ldo				&usb_ss_ldo
#define clk_gcc_pcie_cfg_ahb_clk		&gcc_pcie_cfg_ahb_clk
#define clk_gcc_pcie_pipe_clk			&gcc_pcie_pipe_clk
#define clk_gcc_pcie_axi_clk			&gcc_pcie_axi_clk
#define clk_gcc_pcie_sleep_clk			&gcc_pcie_sleep_clk
#define clk_gcc_pcie_axi_mstr_clk		&gcc_pcie_axi_mstr_clk
#define clk_pcie_pipe_clk_src			&pcie_pipe_clk_src
#define clk_pcie_aux_clk_src			&pcie_aux_clk_src
#define clk_pcie_gpio_ldo			&pcie_gpio_ldo
#define clk_gcc_pcie_phy_reset			&gcc_pcie_phy_reset
#define clk_gpll0_out_msscc			&gpll0_out_msscc
#define clk_gcc_mss_cfg_ahb_clk			&gcc_mss_cfg_ahb_clk
#define clk_gcc_mss_q6_bimc_axi_clk		&gcc_mss_q6_bimc_axi_clk
#define clk_gcc_boot_rom_ahb_clk		&gcc_boot_rom_ahb_clk
#define clk_gcc_prng_ahb_clk			&gcc_prng_ahb_clk

/* a7pll */
#define clk_a7pll_clk				&a7pll_clk
#define clk_a7_debug_mux			&a7_debug_mux

#endif
