/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/regulator/consumer.h>

#include <mach/rpm-regulator-smd.h>
#include <mach/socinfo.h>
#include <mach/rpm-smd.h>

#include "clock-local2.h"
#include "clock-pll.h"
#include "clock-rpm.h"
#include "clock-voter.h"
#include "clock.h"

static struct clk_lookup msm_clocks_dummy[] = {
	CLK_DUMMY("core_clk",   BLSP1_UART_CLK, "f991f000.serial", OFF),
	CLK_DUMMY("iface_clk",  BLSP1_UART_CLK, "f991f000.serial", OFF),
	CLK_DUMMY("core_clk",	SPI_CLK,	"f9928000.spi",  OFF),
	CLK_DUMMY("iface_clk",	SPI_P_CLK,	"f9928000.spi",  OFF),

	CLK_DUMMY("bus_clk", cnoc_msmbus_clk.c, "msm_config_noc", OFF),
	CLK_DUMMY("bus_a_clk", cnoc_msmbus_a_clk.c, "msm_config_noc", OFF),
	CLK_DUMMY("bus_clk", snoc_msmbus_clk.c, "msm_sys_noc", OFF),
	CLK_DUMMY("bus_a_clk", snoc_msmbus_a_clk.c, "msm_sys_noc", OFF),
	CLK_DUMMY("bus_clk", pnoc_msmbus_clk.c, "msm_periph_noc", OFF),
	CLK_DUMMY("bus_a_clk", pnoc_msmbus_a_clk.c, "msm_periph_noc", OFF),
	CLK_DUMMY("mem_clk", bimc_msmbus_clk.c, "msm_bimc", OFF),
	CLK_DUMMY("mem_a_clk", bimc_msmbus_a_clk.c, "msm_bimc", OFF),
	CLK_DUMMY("mem_clk", bimc_acpu_a_clk.c, "", OFF),

	CLK_DUMMY("clktype", gcc_imem_axi_clk         , "drivername", OFF),
	CLK_DUMMY("clktype", gcc_imem_cfg_ahb_clk     , "drivername", OFF),
	CLK_DUMMY("clktype", gcc_mss_cfg_ahb_clk      , "drivername", OFF),
	CLK_DUMMY("clktype", gcc_mss_q6_bimc_axi_clk  , "drivername", OFF),
	CLK_DUMMY("mem_clk", gcc_usb30_master_clk     , "drivername", OFF),
	CLK_DUMMY("sleep_clk", gcc_usb30_sleep_clk    , "drivername", OFF),
	CLK_DUMMY("utmi_clk", gcc_usb30_mock_utmi_clk , "drivername", OFF),
	CLK_DUMMY("iface_clk", gcc_usb_hsic_ahb_clk   , "drivername", OFF),
	CLK_DUMMY("core_clk", gcc_usb_hsic_system_clk , "drivername", OFF),
	CLK_DUMMY("phyclk", gcc_usb_hsic_clk         , "drivername", OFF),
	CLK_DUMMY("cal_clk", gcc_usb_hsic_io_cal_clk  , "drivername", OFF),
	CLK_DUMMY("core_clk", gcc_usb_hs_system_clk   , "drivername", OFF),
	CLK_DUMMY("iface_clk", gcc_usb_hs_ahb_clk     , "drivername", OFF),
	CLK_DUMMY("sleep_a_clk", gcc_usb2a_phy_sleep_clk  , "drivername", OFF),
	CLK_DUMMY("sleep_b_clk", gcc_usb2b_phy_sleep_clk  , "drivername", OFF),
	CLK_DUMMY("core_clk", gcc_sdcc2_apps_clk      , "drivername", OFF),
	CLK_DUMMY("iface_clk", gcc_sdcc2_ahb_clk      , "drivername", OFF),
	CLK_DUMMY("core_clk", gcc_sdcc3_apps_clk      , "drivername", OFF),
	CLK_DUMMY("iface_clk", gcc_sdcc3_ahb_clk      , "drivername", OFF),
	CLK_DUMMY("core_clk", sdcc3_apps_clk_src      , "drivername", OFF),
	CLK_DUMMY("iface_clk", gcc_blsp1_ahb_clk      , "drivername", OFF),
	CLK_DUMMY("core_clk", gcc_blsp1_qup1_spi_apps_clk, "drivername", OFF),
	CLK_DUMMY("core_clk", gcc_blsp1_qup1_i2c_apps_clk, "drivername", OFF),
	CLK_DUMMY("core_clk", gcc_blsp1_uart1_apps_clk , "drivername", OFF),
	CLK_DUMMY("core_clk", gcc_blsp1_qup2_spi_apps_clk, "drivername", OFF),
	CLK_DUMMY("core_clk", gcc_blsp1_qup2_i2c_apps_clk, "drivername", OFF),
	CLK_DUMMY("core_clk", gcc_blsp1_uart2_apps_clk , "drivername", OFF),
	CLK_DUMMY("core_clk", gcc_blsp1_qup3_spi_apps_clk, "drivername", OFF),
	CLK_DUMMY("core_clk", gcc_blsp1_qup3_i2c_apps_clk, "drivername", OFF),
	CLK_DUMMY("core_clk", gcc_blsp1_uart3_apps_clk , "drivername", OFF),
	CLK_DUMMY("core_clk", gcc_blsp1_qup4_spi_apps_clk, "drivername", OFF),
	CLK_DUMMY("core_clk", gcc_blsp1_qup4_i2c_apps_clk, "drivername", OFF),
	CLK_DUMMY("core_clk", gcc_blsp1_uart4_apps_clk , "drivername", OFF),
	CLK_DUMMY("core_clk", gcc_blsp1_qup5_spi_apps_clk, "drivername", OFF),
	CLK_DUMMY("core_clk", gcc_blsp1_qup5_i2c_apps_clk, "drivername", OFF),
	CLK_DUMMY("core_clk", gcc_blsp1_uart5_apps_clk , "drivername", OFF),
	CLK_DUMMY("core_clk", gcc_blsp1_qup6_spi_apps_clk, "drivername", OFF),
	CLK_DUMMY("core_clk", gcc_blsp1_qup6_i2c_apps_clk, "drivername", OFF),
	CLK_DUMMY("core_clk", gcc_blsp1_uart6_apps_clk , "drivername", OFF),
	CLK_DUMMY("core_clk", blsp1_uart6_apps_clk_src , "drivername", OFF),
	CLK_DUMMY("iface_clk", gcc_pdm_ahb_clk        , "drivername", OFF),
	CLK_DUMMY("core_clk", gcc_pdm2_clk            , "drivername", OFF),
	CLK_DUMMY("core_clk", gcc_prng_ahb_clk        , "drivername", OFF),
	CLK_DUMMY("dma_bam_pclk", gcc_bam_dma_ahb_clk , "drivername", OFF),
	CLK_DUMMY("mem_clk", gcc_boot_rom_ahb_clk     , "drivername", OFF),
	CLK_DUMMY("core_clk", gcc_ce1_clk             , "drivername", OFF),
	CLK_DUMMY("core_clk", gcc_ce1_axi_clk         , "drivername", OFF),
	CLK_DUMMY("core_clk", gcc_ce1_ahb_clk         , "drivername", OFF),
	CLK_DUMMY("core_clk_src", ce1_clk_src         , "drivername", OFF),
	CLK_DUMMY("bus_clk", gcc_lpass_q6_axi_clk     , "drivername", OFF),
	CLK_DUMMY("clktype", pcie_pipe_clk            , "drivername", OFF),
	CLK_DUMMY("clktype", gcc_gp1_clk              , "drivername", OFF),
	CLK_DUMMY("clktype", gp1_clk_src              , "drivername", OFF),
	CLK_DUMMY("clktype", gcc_gp2_clk              , "drivername", OFF),
	CLK_DUMMY("clktype", gp2_clk_src              , "drivername", OFF),
	CLK_DUMMY("clktype", gcc_gp3_clk              , "drivername", OFF),
	CLK_DUMMY("clktype", gp3_clk_src              , "drivername", OFF),
	CLK_DUMMY("core_clk", gcc_ipa_clk             , "drivername", OFF),
	CLK_DUMMY("iface_clk", gcc_ipa_cnoc_clk       , "drivername", OFF),
	CLK_DUMMY("inactivity_clk", gcc_ipa_sleep_clk , "drivername", OFF),
	CLK_DUMMY("core_clk_src", ipa_clk_src         , "drivername", OFF),
	CLK_DUMMY("clktype", gcc_dcs_clk              , "drivername", OFF),
	CLK_DUMMY("clktype", dcs_clk_src              , "drivername", OFF),
	CLK_DUMMY("clktype", gcc_pcie_cfg_ahb_clk     , "drivername", OFF),
	CLK_DUMMY("clktype", gcc_pcie_pipe_clk        , "drivername", OFF),
	CLK_DUMMY("clktype", gcc_pcie_axi_clk         , "drivername", OFF),
	CLK_DUMMY("clktype", gcc_pcie_sleep_clk       , "drivername", OFF),
	CLK_DUMMY("clktype", gcc_pcie_axi_mstr_clk    , "drivername", OFF),
	CLK_DUMMY("clktype", pcie_pipe_clk_src        , "drivername", OFF),
	CLK_DUMMY("clktype", pcie_aux_clk_src         , "drivername", OFF),
};

struct clock_init_data msmkrypton_clock_init_data __initdata = {
	.table = msm_clocks_dummy,
	.size = ARRAY_SIZE(msm_clocks_dummy),
};
