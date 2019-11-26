/* Copyright (c) 2015, 2019, The Linux Foundation. All rights reserved.
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

#ifndef __MDM_CLOCKS_9607_H
#define __MDM_CLOCKS_9607_H

/*PLL Sources */
#define clk_gpll0_clk_src					 0x5933b69f
#define clk_gpll0_ao_clk_src                                     0x6b2fb034
#define clk_gpll2_clk_src					 0x7c34503b
#define clk_gpll1_clk_src					 0x916f8847

#define clk_a7sspll						 0x0b2e5cbd

/*RPM and Voter clocks */
#define clk_pcnoc_clk						 0xc1296d0f
#define clk_pcnoc_a_clk						 0x9bcffee4
#define clk_pcnoc_msmbus_clk					 0x2b53b688
#define clk_pcnoc_msmbus_a_clk					 0x9753a54f
#define clk_pcnoc_keepalive_a_clk				 0x9464f720
#define clk_pcnoc_usb_clk					 0x57adc448
#define clk_pcnoc_usb_a_clk					 0x11d6a74e
#define clk_bimc_clk						 0x4b80bf00
#define clk_bimc_a_clk						 0x4b25668a
#define clk_bimc_msmbus_clk					 0xd212feea
#define clk_bimc_msmbus_a_clk					 0x71d1a499
#define clk_bimc_usb_clk					 0x9bd2b2bf
#define clk_bimc_usb_a_clk					 0xea410834
#define clk_qdss_clk						 0x1492202a
#define clk_qdss_a_clk						 0xdd121669
#define clk_qpic_clk						 0x3ce6f7bb
#define clk_qpic_a_clk						 0xd70ccb7c
#define clk_xo_clk_src						 0x23f5649f
#define clk_xo_a_clk_src					 0x2fdd2c7c
#define clk_xo_otg_clk						 0x79bca5cc
#define clk_xo_lpm_clk						 0x2be48257
#define clk_xo_pil_mss_clk					 0xe97a8354
#define clk_bb_clk1						 0xf5304268
#define clk_bb_clk1_pin						 0x6dd0a779

/* SRCs */
#define clk_apss_ahb_clk_src					 0x36f8495f
#define clk_emac_0_125m_clk_src					 0x955db353
#define clk_blsp1_qup1_i2c_apps_clk_src				 0x17f78f5e
#define clk_blsp1_qup1_spi_apps_clk_src				 0xf534c4fa
#define clk_blsp1_qup2_i2c_apps_clk_src				 0x8de71c79
#define clk_blsp1_qup2_spi_apps_clk_src				 0x33cf809a
#define clk_blsp1_qup3_i2c_apps_clk_src				 0xf161b902
#define clk_blsp1_qup3_spi_apps_clk_src				 0x5e95683f
#define clk_blsp1_qup4_i2c_apps_clk_src				 0xb2ecce68
#define clk_blsp1_qup4_spi_apps_clk_src				 0xddb5bbdb
#define clk_blsp1_qup5_i2c_apps_clk_src				 0x71ea7804
#define clk_blsp1_qup5_spi_apps_clk_src				 0x9752f35f
#define clk_blsp1_qup6_i2c_apps_clk_src				 0x28806803
#define clk_blsp1_qup6_spi_apps_clk_src				 0x44a1edc4
#define clk_blsp1_uart1_apps_clk_src				 0xf8146114
#define clk_blsp1_uart2_apps_clk_src				 0xfc9c2f73
#define clk_blsp1_uart3_apps_clk_src				 0x600497f2
#define clk_blsp1_uart4_apps_clk_src				 0x56bff15c
#define clk_blsp1_uart5_apps_clk_src				 0x218ef697
#define clk_blsp1_uart6_apps_clk_src				 0x8fbdbe4c
#define clk_crypto_clk_src					 0x37a21414
#define clk_gp1_clk_src						 0xad85b97a
#define clk_gp2_clk_src						 0xfb1f0065
#define clk_gp3_clk_src						 0x63b693d6
#define clk_pdm2_clk_src					 0x31e494fd
#define clk_sdcc1_apps_clk_src					 0xd4975db2
#define clk_sdcc2_apps_clk_src					 0xfc46c821
#define clk_emac_0_sys_25m_clk_src				 0x92fe3614
#define clk_emac_0_tx_clk_src					 0x0487ec76
#define clk_usb_hs_system_clk_src				 0x28385546
#define clk_usb_hsic_clk_src					 0x141b01df
#define clk_usb_hsic_io_cal_clk_src				 0xc83584bd
#define clk_usb_hsic_system_clk_src				 0x52ef7224

/*Branch*/
#define clk_gcc_apss_ahb_clk					 0x2b0d39ff
#define clk_gcc_apss_axi_clk					 0x1d47f4ff
#define clk_gcc_prng_ahb_clk					 0x397e7eaa
#define clk_gcc_qdss_dap_clk					 0x7fa9aa73
#define clk_gcc_apss_tcu_clk					 0xaf56a329
#define clk_gcc_blsp1_ahb_clk					 0x8caa5b4f
#define clk_gcc_blsp1_qup1_i2c_apps_clk				 0xc303fae9
#define clk_gcc_blsp1_qup1_spi_apps_clk				 0x759a76b0
#define clk_gcc_blsp1_qup2_i2c_apps_clk				 0x1076f220
#define clk_gcc_blsp1_qup2_spi_apps_clk				 0x3e77d48f
#define clk_gcc_blsp1_qup3_i2c_apps_clk				 0x9e25ac82
#define clk_gcc_blsp1_qup3_spi_apps_clk				 0xfb978880
#define clk_gcc_blsp1_qup4_i2c_apps_clk				 0xd7f40f6f
#define clk_gcc_blsp1_qup4_spi_apps_clk				 0x80f8722f
#define clk_gcc_blsp1_qup5_i2c_apps_clk				 0xacae5604
#define clk_gcc_blsp1_qup5_spi_apps_clk				 0xbf3e15d7
#define clk_gcc_blsp1_qup6_i2c_apps_clk				 0x5c6ad820
#define clk_gcc_blsp1_qup6_spi_apps_clk				 0x780d9f85
#define clk_gcc_blsp1_uart1_apps_clk				 0xc7c62f90
#define clk_gcc_blsp1_uart2_apps_clk				 0xf8a61c96
#define clk_gcc_blsp1_uart3_apps_clk				 0xc3298bd7
#define clk_gcc_blsp1_uart4_apps_clk				 0x26be16c0
#define clk_gcc_blsp1_uart5_apps_clk				 0x28a6bc74
#define clk_gcc_blsp1_uart6_apps_clk				 0x28fd3466
#define clk_gcc_boot_rom_ahb_clk				 0xde2adeb1
#define clk_gcc_crypto_ahb_clk					 0x94de4919
#define clk_gcc_crypto_axi_clk					 0xd4415c9b
#define clk_gcc_crypto_clk					 0x00d390d2
#define clk_gcc_gp1_clk						 0x057f7b69
#define clk_gcc_gp2_clk						 0x9bf83ffd
#define clk_gcc_gp3_clk						 0xec6539ee
#define clk_gcc_mss_cfg_ahb_clk					 0x111cde81
#define clk_gcc_mss_q6_bimc_axi_clk				 0x67544d62
#define clk_gcc_pdm2_clk					 0x99d55711
#define clk_gcc_pdm_ahb_clk					 0x365664f6
#define clk_gcc_sdcc1_ahb_clk					 0x691e0caa
#define clk_gcc_sdcc1_apps_clk					 0x9ad6fb96
#define clk_gcc_sdcc2_ahb_clk					 0x23d5727f
#define clk_gcc_sdcc2_apps_clk					 0x861b20ac
#define clk_gcc_emac_0_125m_clk					 0xe556de53
#define clk_gcc_emac_0_ahb_clk					 0x6a741d38
#define clk_gcc_emac_0_axi_clk					 0xf2b04fb4
#define clk_gcc_emac_0_rx_clk					 0x869a4e5c
#define clk_gcc_emac_0_sys_25m_clk				 0x5812832b
#define clk_gcc_emac_0_sys_clk					 0x34fb62b0
#define clk_gcc_emac_0_tx_clk					 0x331d3573
#define clk_gcc_smmu_cfg_clk					 0x75eaefa5
#define clk_gcc_usb2a_phy_sleep_clk				 0x6caa736f
#define clk_gcc_usb_hs_phy_cfg_ahb_clk				 0xe13808fd
#define clk_gcc_usb_hs_ahb_clk					 0x72ce8032
#define clk_gcc_usb_hs_system_clk				 0xa11972e5
#define clk_gcc_usb_hsic_ahb_clk				 0x3ec2631a
#define clk_gcc_usb_hsic_clk					 0x8de18b0e
#define clk_gcc_usb_hsic_io_cal_clk				 0xbc21f776
#define clk_gcc_usb_hsic_io_cal_sleep_clk			 0x20e09a22
#define clk_gcc_usb_hsic_system_clk				 0x145e9366
#define clk_gcc_usb2_hs_phy_only_clk				 0x0047179d
#define clk_gcc_qusb2_phy_clk					 0x996884d5
/* DEBUG */
#define clk_gcc_debug_mux					 0x8121ac15
#define clk_apss_debug_pri_mux					 0xc691ff55
#define clk_apc0_m_clk						 0xce1e9473
#define clk_apc1_m_clk						 0x990fbaf7
#define clk_apc2_m_clk						 0x252cd4ae
#define clk_apc3_m_clk						 0x78c64486
#define clk_l2_m_clk						 0x4bedf4d0

#define clk_wcnss_m_clk						 0x709f430b

#define GCC_USB2_HS_PHY_ONLY_BCR				0
#define GCC_QUSB2_PHY_BCR					1
#define GCC_USB_HS_BCR						2
#define GCC_USB_HS_HSIC_BCR					3

#endif
